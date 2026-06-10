#include "Interpreter.h"
#include "../utils/ErrorHandling.h"
#include "../lexer/Lexer.h"
#include "../parser/Parser.h"
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <string>
#include <variant>
#include <cmath>
#include <memory>
#include <unordered_map>
#include <functional>
#include <unordered_set>
#include <vector>

// Forward declarations
class Function;
class Class;
class Instance;
class BoundMethod;
class ListValue;
class Interface;
class Trait;

// Value type for the interpreter
using Value = std::variant<std::nullptr_t, bool, double, std::string, std::shared_ptr<ListValue>,
                           std::shared_ptr<Function>, std::shared_ptr<Class>, 
                           std::shared_ptr<Instance>, std::shared_ptr<BoundMethod>,
                           std::shared_ptr<Interface>, std::shared_ptr<Trait>>;

// List value implementation (after Value is defined)
class ListValue {
public:
    std::vector<Value> elements;
    ListValue() = default;
    ListValue(std::vector<Value> elements) : elements(std::move(elements)) {}
};

// Environment for variable storage
class Environment {
private:
    std::unordered_map<std::string, Value> values;
    std::unordered_map<std::string, bool> constFlags;
    std::shared_ptr<Environment> enclosing;
    
public:
    Environment(std::shared_ptr<Environment> enclosing = nullptr)
        : enclosing(enclosing) {}
    
    void define(const std::string& name, const Value& value, bool isConst = false) {
        values[name] = value;
        constFlags[name] = isConst;
    }
    
    Value get(const std::string& name) {
        auto it = values.find(name);
        if (it != values.end()) {
            return it->second;
        }
        
        if (enclosing) {
            return enclosing->get(name);
        }
        
        throw std::runtime_error("Undefined variable '" + name + "'");
    }
    
    void assign(const std::string& name, const Value& value) {
        auto it = values.find(name);
        if (it != values.end()) {
            if (constFlags[name]) {
                throw std::runtime_error("Cannot assign to constant '" + name + "'");
            }
            it->second = value;
            return;
        }
        
        if (enclosing) {
            enclosing->assign(name, value);
            return;
        }
        
        throw std::runtime_error("Undefined variable '" + name + "'");
    }
};

// Return exception for flow control
class ReturnException : public std::runtime_error {
public:
    Value value;

    ReturnException(const Value& value)
        : std::runtime_error("return"), value(value) {}
};

class BreakException : public std::runtime_error {
public:
    BreakException() : std::runtime_error("break") {}
};

class ContinueException : public std::runtime_error {
public:
    ContinueException() : std::runtime_error("continue") {}
};

// Forward declaration of the interpreter visitor
class InterpreterVisitor;
class Class;

// Base callable interface
class Callable {
public:
    virtual ~Callable() = default;
    virtual Value call(InterpreterVisitor* interpreter, const std::vector<Value>& arguments) = 0;
    virtual int arity() const = 0;
};

// Function implementation
class Function : public Callable {
private:
    FunctionNode* declaration;
    std::shared_ptr<Environment> closure;
    AccessModifier accessModifier;
    std::shared_ptr<Class> owningClass;
    
public:
    Function(FunctionNode* declaration, std::shared_ptr<Environment> closure, 
             AccessModifier accessModifier = AccessModifier::PUBLIC,
             std::shared_ptr<Class> owningClass = nullptr)
        : declaration(declaration), closure(closure), accessModifier(accessModifier), owningClass(owningClass) {}
    
    int arity() const override {
        return declaration->params.size();
    }
    
    Value call(InterpreterVisitor* interpreter, const std::vector<Value>& arguments) override;
    
    std::shared_ptr<Environment> getClosure() const { return closure; }
    FunctionNode* getDeclaration() const { return declaration; }
    AccessModifier getAccessModifier() const { return accessModifier; }
    std::shared_ptr<Class> getOwningClass() const { return owningClass; }
    bool isAbstract() const { return declaration->isAbstract; }

    std::string toString() const {
        return "<fn " + declaration->name + ">";
    }
};

// Bound method — binds an instance to a method
class BoundMethod : public Callable {
private:
    std::shared_ptr<Instance> receiver;
    std::shared_ptr<Function> method;

public:
    BoundMethod(std::shared_ptr<Instance> receiver, std::shared_ptr<Function> method)
        : receiver(receiver), method(method) {}

    int arity() const override {
        return method->arity();
    }

    Value call(InterpreterVisitor* interpreter, const std::vector<Value>& arguments) override;

    std::shared_ptr<Function> getMethod() const { return method; }
    std::shared_ptr<Instance> getReceiver() const { return receiver; }

    std::string toString() const {
        return "<bound method>";
    }
};

// Class implementation
class Class : public Callable, public std::enable_shared_from_this<Class> {
public:
    struct FieldDecl {
        std::string name;
        ExprNode* initializer;
        AccessModifier accessModifier;
        bool isConst;
    };

private:
    std::string name;
    std::unordered_map<std::string, std::shared_ptr<Function>> methods;
    std::vector<FieldDecl> fieldDecls;
    std::shared_ptr<Environment> closure;
    std::unordered_map<std::string, AccessModifier> fieldAccess;
    std::unordered_set<std::string> fieldConst;
    std::shared_ptr<Class> parent;
    bool isAbstract;
    std::vector<std::shared_ptr<Interface>> interfaces;
    std::vector<std::shared_ptr<Trait>> traits;
    std::unordered_map<std::string, std::shared_ptr<Function>> staticMethods;
    std::unordered_map<std::string, Value> staticFields;
    std::unordered_set<std::string> staticFieldNames;
    
public:
    Class(const std::string& name, 
          const std::unordered_map<std::string, std::shared_ptr<Function>>& methods,
          const std::vector<FieldDecl>& fieldDecls = {},
          std::shared_ptr<Environment> closure = nullptr,
          std::shared_ptr<Class> parent = nullptr)
        : name(name), methods(methods), fieldDecls(fieldDecls), closure(closure), parent(parent), isAbstract(false) {
        for (auto& fd : fieldDecls) {
            fieldAccess[fd.name] = fd.accessModifier;
            if (fd.isConst) fieldConst.insert(fd.name);
        }
    }
    
    int arity() const override {
        auto initMethod = findMethod("init");
        if (initMethod) {
            return initMethod->arity();
        }
        return 0;
    }
    
    Value call(InterpreterVisitor* interpreter, const std::vector<Value>& arguments) override;
    
    std::shared_ptr<Function> findMethod(const std::string& methodName) const;
    
    std::pair<AccessModifier, std::shared_ptr<Class>> getFieldAccess(const std::string& fieldName) {
        auto it = fieldAccess.find(fieldName);
        if (it != fieldAccess.end()) return {it->second, shared_from_this()};
        if (parent) return parent->getFieldAccess(fieldName);
        return {AccessModifier::PUBLIC, nullptr};
    }
    
    std::shared_ptr<Class> getParent() const { return parent; }
    
    void setMethods(const std::unordered_map<std::string, std::shared_ptr<Function>>& m) { methods = m; }
    void setFieldDecls(const std::vector<FieldDecl>& fds) { 
        fieldDecls = fds;
        fieldAccess.clear();
        fieldConst.clear();
        for (auto& fd : fieldDecls) {
            fieldAccess[fd.name] = fd.accessModifier;
            if (fd.isConst) fieldConst.insert(fd.name);
        }
    }
    
    void setAbstract(bool abs) { isAbstract = abs; }
    bool getAbstract() const { return isAbstract; }
    
    void setInterfaces(const std::vector<std::shared_ptr<Interface>>& ifaces) { interfaces = ifaces; }
    std::vector<std::shared_ptr<Interface>> getInterfaces() const { return interfaces; }
    
    void setTraits(const std::vector<std::shared_ptr<Trait>>& ts) { traits = ts; }
    std::vector<std::shared_ptr<Trait>> getTraits() const { return traits; }
    
    void applyTraitMethods();
    
    bool isFieldConst(const std::string& fieldName) {
        if (fieldConst.find(fieldName) != fieldConst.end()) return true;
        if (parent) return parent->isFieldConst(fieldName);
        return false;
    }
    
    const std::vector<FieldDecl>& getFieldDecls() const { return fieldDecls; }
    std::shared_ptr<Environment> getClosure() const { return closure; }
    
    void setStaticMethods(const std::unordered_map<std::string, std::shared_ptr<Function>>& sm) { staticMethods = sm; }
    const std::unordered_map<std::string, std::shared_ptr<Function>>& getStaticMethods() const { return staticMethods; }
    std::shared_ptr<Function> findStaticMethod(const std::string& methodName) const {
        auto it = staticMethods.find(methodName);
        if (it != staticMethods.end()) return it->second;
        if (parent) return parent->findStaticMethod(methodName);
        return nullptr;
    }
    
    void setStaticFieldNames(const std::unordered_set<std::string>& names) { staticFieldNames = names; }
    bool isStaticField(const std::string& fieldName) const {
        if (staticFieldNames.find(fieldName) != staticFieldNames.end()) return true;
        if (parent) return parent->isStaticField(fieldName);
        return false;
    }
    void setStaticField(const std::string& name, const Value& value) { staticFields[name] = value; }
    Value getStaticField(const std::string& fieldName) const {
        auto it = staticFields.find(fieldName);
        if (it != staticFields.end()) return it->second;
        if (parent) return parent->getStaticField(fieldName);
        return nullptr;
    }
    const std::unordered_map<std::string, Value>& getStaticFields() const { return staticFields; }
    
    std::string toString() const {
        return "<class " + name + ">";
    }
};

// Interface runtime type
class Interface : public std::enable_shared_from_this<Interface> {
private:
    std::string name;
    std::unordered_map<std::string, std::shared_ptr<Function>> methods;
    std::vector<std::shared_ptr<Interface>> parents;

public:
    Interface(const std::string& name,
              const std::unordered_map<std::string, std::shared_ptr<Function>>& methods,
              std::vector<std::shared_ptr<Interface>> parents = {})
        : name(name), methods(methods), parents(parents) {}

    std::shared_ptr<Function> findMethod(const std::string& methodName) const {
        auto it = methods.find(methodName);
        if (it != methods.end()) return it->second;
        for (auto& parent : parents) {
            auto result = parent->findMethod(methodName);
            if (result) return result;
        }
        return nullptr;
    }

    const std::unordered_map<std::string, std::shared_ptr<Function>>& getMethods() const {
        return methods;
    }

    std::string getName() const { return name; }
    std::vector<std::shared_ptr<Interface>> getParents() const { return parents; }

    std::string toString() const {
        return "<interface " + name + ">";
    }
};

// Trait runtime type
class Trait : public std::enable_shared_from_this<Trait> {
private:
    std::string name;
    std::unordered_map<std::string, std::shared_ptr<Function>> methods;
    std::vector<std::shared_ptr<Trait>> parents;

public:
    Trait(const std::string& name,
          const std::unordered_map<std::string, std::shared_ptr<Function>>& methods,
          std::vector<std::shared_ptr<Trait>> parents = {})
        : name(name), methods(methods), parents(parents) {}

    std::shared_ptr<Function> findMethod(const std::string& methodName) const {
        auto it = methods.find(methodName);
        if (it != methods.end()) return it->second;
        for (auto& parent : parents) {
            auto result = parent->findMethod(methodName);
            if (result) return result;
        }
        return nullptr;
    }

    const std::unordered_map<std::string, std::shared_ptr<Function>>& getMethods() const {
        return methods;
    }

    std::string getName() const { return name; }
    std::vector<std::shared_ptr<Trait>> getParents() const { return parents; }

    std::string toString() const {
        return "<trait " + name + ">";
    }
};

// Trait method application
void Class::applyTraitMethods() {
    for (auto& trait : traits) {
        for (auto& [name, func] : trait->getMethods()) {
            if (methods.find(name) == methods.end() && !func->isAbstract()) {
                methods[name] = func;
            }
        }
        for (auto& parentTrait : trait->getParents()) {
            for (auto& [name, func] : parentTrait->getMethods()) {
                if (methods.find(name) == methods.end() && !func->isAbstract()) {
                    methods[name] = func;
                }
            }
        }
    }
}

// Class instance implementation
class Instance : public std::enable_shared_from_this<Instance> {
private:
    std::shared_ptr<Class> klass;
    std::unordered_map<std::string, Value> fields;
    
public:
    Instance(std::shared_ptr<Class> klass) : klass(klass) {}
    
    Value get(const std::string& name) {
        auto it = fields.find(name);
        if (it != fields.end()) {
            return it->second;
        }
        
        std::shared_ptr<Function> method = klass->findMethod(name);
        if (method) {
            return std::make_shared<BoundMethod>(shared_from_this(), method);
        }
        
        throw std::runtime_error("Undefined property '" + name + "'");
    }
    
    void set(const std::string& name, const Value& value) {
        fields[name] = value;
    }
    
    std::shared_ptr<Class> getClass() const { return klass; }
    
    std::string toString() const {
        if (klass) { // Check if klass is not nullptr
            return "<instance of " + klass->toString() + ">";
        }
        return "<unknown instance>";
    }
};

// Helper functions for Value operations
std::string valueToString(const Value& value) {
    if (std::holds_alternative<std::nullptr_t>(value)) {
        return "nil";
    } else if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value) ? "true" : "false";
    } else if (std::holds_alternative<double>(value)) {
        double num = std::get<double>(value);
        return std::to_string(num);
    } else if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    } else if (std::holds_alternative<std::shared_ptr<Function>>(value)) {
        return std::get<std::shared_ptr<Function>>(value)->toString();
    } else if (std::holds_alternative<std::shared_ptr<Class>>(value)) {
        return std::get<std::shared_ptr<Class>>(value)->toString();
    } else if (std::holds_alternative<std::shared_ptr<Instance>>(value)) {
        return std::get<std::shared_ptr<Instance>>(value)->toString();
    } else if (std::holds_alternative<std::shared_ptr<BoundMethod>>(value)) {
        return std::get<std::shared_ptr<BoundMethod>>(value)->toString();
    } else if (std::holds_alternative<std::shared_ptr<ListValue>>(value)) {
        const auto& list = std::get<std::shared_ptr<ListValue>>(value)->elements;
        std::string result = "[";
        for (size_t i = 0; i < list.size(); i++) {
            if (i > 0) result += ", ";
            result += valueToString(list[i]);
        }
        result += "]";
        return result;
    }
    
    return "unknown";
}

bool isTruthy(const Value& value) {
    if (std::holds_alternative<std::nullptr_t>(value)) {
        return false;
    } else if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value);
    }
    return true;
}

double asNumber(const Value& value) {
    if (std::holds_alternative<double>(value)) {
        return std::get<double>(value);
    }
    throw std::runtime_error("Operand must be a number");
}

// Interpreter implementation
class InterpreterVisitor : public ASTVisitor {
private:
    std::shared_ptr<Environment> globals;
    std::shared_ptr<Environment> environment;
    Value lastValue;
    std::vector<ASTNode*> loadedASTs;
    
public:
    std::shared_ptr<Class> currentClass;
    
    InterpreterVisitor() : globals(std::make_shared<Environment>()), environment(globals), currentClass(nullptr) {
        // Add native functions to global environment
    }
    
    Value getValue() const {
        return lastValue;
    }
    
    void executeBlock(const std::vector<StmtNode*>& statements, std::shared_ptr<Environment> env) {
        std::shared_ptr<Environment> previous = environment;
        
        try {
            environment = env;
            
            for (auto& stmt : statements) {
                stmt->accept(this);
            }
        } catch (...) {
            environment = previous;
            throw;
        }
        
        environment = previous;
    }
    
    void executeFieldInitializers(std::shared_ptr<Instance> instance,
                                   const std::vector<Class::FieldDecl>& fieldDecls,
                                   std::shared_ptr<Environment> env) {
        std::shared_ptr<Environment> previous = environment;
        environment = env;
        
        for (auto& field : fieldDecls) {
            if (field.initializer) {
                field.initializer->accept(this);
                instance->set(field.name, lastValue);
            }
        }
        
        environment = previous;
    }
    
    void visit(ProgramNode* node) override {
        for (auto& decl : node->declarations) {
            decl->accept(this);
        }
    }
    
    void visit(BinaryExpr* node) override {
        // Short-circuit logical operators
        if (node->op.type == TokenType::AND) {
            node->left->accept(this);
            if (!isTruthy(lastValue)) {
                return;
            }
            node->right->accept(this);
            return;
        }

        if (node->op.type == TokenType::OR) {
            node->left->accept(this);
            if (isTruthy(lastValue)) {
                return;
            }
            node->right->accept(this);
            return;
        }

        node->left->accept(this);
        Value left = lastValue;
        
        node->right->accept(this);
        Value right = lastValue;
        
        switch (node->op.type) {
            case TokenType::PLUS:
                if (std::holds_alternative<double>(left) && std::holds_alternative<double>(right)) {
                    lastValue = std::get<double>(left) + std::get<double>(right);
                } else if (std::holds_alternative<std::string>(left) || std::holds_alternative<std::string>(right)) {
                    lastValue = valueToString(left) + valueToString(right);
                } else {
                    throw std::runtime_error("Operands must be two numbers or two strings");
                }
                break;
                
            case TokenType::MINUS:
                lastValue = asNumber(left) - asNumber(right);
                break;
                
            case TokenType::STAR:
                lastValue = asNumber(left) * asNumber(right);
                break;
                
            case TokenType::SLASH:
                if (asNumber(right) == 0) {
                    throw std::runtime_error("Division by zero");
                }
                lastValue = asNumber(left) / asNumber(right);
                break;

            case TokenType::PERCENT:
                if (asNumber(right) == 0) {
                    throw std::runtime_error("Division by zero");
                }
                lastValue = std::fmod(asNumber(left), asNumber(right));
                break;

            case TokenType::EQUAL_EQUAL:
                lastValue = left == right;
                break;

            case TokenType::BANG_EQUAL:
                lastValue = !(left == right);
                break;

            case TokenType::LESS:
                lastValue = asNumber(left) < asNumber(right);
                break;

            case TokenType::LESS_EQUAL:
                lastValue = asNumber(left) <= asNumber(right);
                break;

            case TokenType::GREATER:
                lastValue = asNumber(left) > asNumber(right);
                break;

            case TokenType::GREATER_EQUAL:
                lastValue = asNumber(left) >= asNumber(right);
                break;
                
            default:
                lastValue = nullptr;
                break;
        }
    }
    
    void visit(LiteralExpr* node) override {
        switch (node->value.type) {
            case TokenType::NUMBER:
                lastValue = std::stod(node->value.lexeme);
                break;

            case TokenType::STRING:
                lastValue = node->value.lexeme;
                break;

            case TokenType::TRUE:
                lastValue = true;
                break;

            case TokenType::FALSE:
                lastValue = false;
                break;

            case TokenType::NIL:
                lastValue = nullptr;
                break;

            default:
                lastValue = nullptr;
                break;
        }
    }
    
    void visit(VariableExpr* node) override {
        lastValue = environment->get(node->name.lexeme);
    }
    
    void visit(AssignExpr* node) override {
        node->value->accept(this);
        
        environment->assign(node->name.lexeme, lastValue);
    }

    void visit(CompoundAssignExpr* node) override {
        Value left = environment->get(node->name.lexeme);
        node->value->accept(this);
        Value right = lastValue;

        switch (node->op.type) {
            case TokenType::PLUS_EQUAL:
                if (std::holds_alternative<double>(left) && std::holds_alternative<double>(right)) {
                    lastValue = std::get<double>(left) + std::get<double>(right);
                } else if (std::holds_alternative<std::string>(left) && std::holds_alternative<std::string>(right)) {
                    lastValue = std::get<std::string>(left) + std::get<std::string>(right);
                } else {
                    throw std::runtime_error("Operands must be two numbers or two strings");
                }
                break;
            case TokenType::MINUS_EQUAL:
                lastValue = asNumber(left) - asNumber(right);
                break;
            case TokenType::STAR_EQUAL:
                lastValue = asNumber(left) * asNumber(right);
                break;
            case TokenType::SLASH_EQUAL:
                if (asNumber(right) == 0) {
                    throw std::runtime_error("Division by zero");
                }
                lastValue = asNumber(left) / asNumber(right);
                break;
            default:
                lastValue = nullptr;
                break;
        }

        environment->assign(node->name.lexeme, lastValue);
    }

    void visit(UnaryExpr* node) override {
        if (node->op.type == TokenType::PLUS_PLUS || node->op.type == TokenType::MINUS_MINUS) {
            VariableExpr* var = dynamic_cast<VariableExpr*>(node->right);
            if (!var) {
                throw std::runtime_error("Invalid prefix expression target");
            }
            double num = asNumber(environment->get(var->name.lexeme));
            if (node->op.type == TokenType::PLUS_PLUS) {
                num += 1;
            } else {
                num -= 1;
            }
            lastValue = num;
            environment->assign(var->name.lexeme, lastValue);
            return;
        }

        node->right->accept(this);

        switch (node->op.type) {
            case TokenType::BANG:
                lastValue = !isTruthy(lastValue);
                break;
            case TokenType::MINUS:
                lastValue = -asNumber(lastValue);
                break;
            default:
                lastValue = nullptr;
                break;
        }
    }

    void visit(PostfixExpr* node) override {
        double num = asNumber(environment->get(node->name.lexeme));
        if (node->op.type == TokenType::PLUS_PLUS) {
            environment->assign(node->name.lexeme, num + 1);
            lastValue = num;
        } else {
            environment->assign(node->name.lexeme, num - 1);
            lastValue = num;
        }
    }

    void visit(TernaryExpr* node) override {
        node->condition->accept(this);
        if (isTruthy(lastValue)) {
            node->thenBranch->accept(this);
        } else {
            node->elseBranch->accept(this);
        }
    }

    void visit(ListExpr* node) override {
        std::vector<Value> elements;
        for (auto* el : node->elements) {
            el->accept(this);
            elements.push_back(lastValue);
        }
        lastValue = std::make_shared<ListValue>(std::move(elements));
    }

    void visit(IndexGetExpr* node) override {
        node->object->accept(this);
        Value object = lastValue;
        
        if (!std::holds_alternative<std::shared_ptr<ListValue>>(object)) {
            throw std::runtime_error("Only lists support indexing");
        }
        
        node->index->accept(this);
        Value index = lastValue;
        
        if (!std::holds_alternative<double>(index)) {
            throw std::runtime_error("Index must be a number");
        }
        
        auto& list = std::get<std::shared_ptr<ListValue>>(object)->elements;
        int idx = static_cast<int>(std::get<double>(index));
        
        if (idx < 0 || idx >= static_cast<int>(list.size())) {
            throw std::runtime_error("Index out of bounds");
        }
        
        lastValue = list[idx];
    }

    void visit(IndexSetExpr* node) override {
        node->object->accept(this);
        Value object = lastValue;
        
        if (!std::holds_alternative<std::shared_ptr<ListValue>>(object)) {
            throw std::runtime_error("Only lists support indexing");
        }
        
        node->index->accept(this);
        Value index = lastValue;
        
        if (!std::holds_alternative<double>(index)) {
            throw std::runtime_error("Index must be a number");
        }
        
        node->value->accept(this);
        Value value = lastValue;
        
        auto& list = std::get<std::shared_ptr<ListValue>>(object)->elements;
        int idx = static_cast<int>(std::get<double>(index));
        
        if (idx < 0 || idx >= static_cast<int>(list.size())) {
            throw std::runtime_error("Index out of bounds");
        }
        
        list[idx] = value;
        lastValue = value;
    }

    void visit(ThisExpr* node) override {
        lastValue = environment->get("this");
    }
    
    void visit(SuperExpr* node) override {
        if (!currentClass) {
            throw std::runtime_error("'super' can only be used inside a method");
        }
        
        std::shared_ptr<Class> parent = currentClass->getParent();
        if (!parent) {
            throw std::runtime_error("No parent class to call 'super." + node->method.lexeme + "'");
        }
        
        Value thisVal = environment->get("this");
        if (!std::holds_alternative<std::shared_ptr<Instance>>(thisVal)) {
            throw std::runtime_error("'super' can only be used inside a method");
        }
        
        auto instance = std::get<std::shared_ptr<Instance>>(thisVal);
        std::shared_ptr<Function> method = parent->findMethod(node->method.lexeme);
        if (!method) {
            throw std::runtime_error("Undefined method '" + node->method.lexeme + "' in parent class");
        }
        
        lastValue = std::make_shared<BoundMethod>(instance, method);
    }
    
    void visit(GetExpr* node) override {
        node->object->accept(this);
        Value object = lastValue;

        if (std::holds_alternative<std::shared_ptr<Instance>>(object)) {
            auto instance = std::get<std::shared_ptr<Instance>>(object);
            
            // Check field access BEFORE get() to avoid leaking private field values
            auto [fieldMod, fieldDeclClass] = instance->getClass()->getFieldAccess(node->name.lexeme);
            if (fieldMod != AccessModifier::PUBLIC) {
                checkAccess(instance, node->name.lexeme, fieldDeclClass, fieldMod);
            }
            
            lastValue = instance->get(node->name.lexeme);
            
            // Enforce access control for methods
            if (std::holds_alternative<std::shared_ptr<BoundMethod>>(lastValue)) {
                auto bound = std::get<std::shared_ptr<BoundMethod>>(lastValue);
                auto modifier = bound->getMethod()->getAccessModifier();
                if (modifier != AccessModifier::PUBLIC) {
                    auto declaringClass = bound->getMethod()->getOwningClass();
                    checkAccess(instance, node->name.lexeme, declaringClass, modifier);
                }
            }
        } else {
            throw std::runtime_error("Only instances have properties");
        }
    }

    bool isSubclassOf(std::shared_ptr<Class> child, std::shared_ptr<Class> parent) {
        auto klass = child;
        while (klass) {
            if (klass == parent) return true;
            klass = klass->getParent();
        }
        return false;
    }
    
    void checkAccess(std::shared_ptr<Instance> targetInstance, const std::string& name,
                     std::shared_ptr<Class> declaringClass, AccessModifier modifier) {
        if (modifier == AccessModifier::PUBLIC) return;
        
        try {
            Value thisVal = environment->get("this");
            if (std::holds_alternative<std::shared_ptr<Instance>>(thisVal)) {
                if (modifier == AccessModifier::PRIVATE) {
                    if (currentClass == declaringClass) return;
                } else if (modifier == AccessModifier::PROTECTED) {
                    if (isSubclassOf(currentClass, declaringClass)) return;
                }
            }
        } catch (...) {}
        
        std::string modStr = (modifier == AccessModifier::PRIVATE) ? "private" : "protected";
        throw std::runtime_error("Cannot access " + modStr + " member '" + name + "' from outside the class");
    }
    
    void visit(SetExpr* node) override {
        node->object->accept(this);
        Value object = lastValue;

        if (!std::holds_alternative<std::shared_ptr<Instance>>(object)) {
            throw std::runtime_error("Only instances have properties");
        }

        node->value->accept(this);
        
        auto instance = std::get<std::shared_ptr<Instance>>(object);
        
        // Enforce access control for fields
        auto [fieldMod, fieldDeclClass] = instance->getClass()->getFieldAccess(node->name.lexeme);
        if (fieldMod != AccessModifier::PUBLIC) {
            checkAccess(instance, node->name.lexeme, fieldDeclClass, fieldMod);
        }
        
        // Enforce const fields
        if (instance->getClass()->isFieldConst(node->name.lexeme)) {
            throw std::runtime_error("Cannot assign to const field '" + node->name.lexeme + "'");
        }
        
        instance->set(node->name.lexeme, lastValue);
    }

    void visit(CallExpr* node) override {
        node->callee->accept(this);
        Value callee = lastValue;
        
        std::vector<Value> arguments;
        for (auto& argument : node->arguments) {
            argument->accept(this);
            arguments.push_back(lastValue);
        }
        
        if (std::holds_alternative<std::shared_ptr<Function>>(callee)) {
            auto function = std::get<std::shared_ptr<Function>>(callee);
            
            if (arguments.size() != function->arity()) {
                throw std::runtime_error("Expected " + 
                                        std::to_string(function->arity()) + 
                                        " arguments but got " + 
                                        std::to_string(arguments.size()));
            }
            
            lastValue = function->call(this, arguments);
        } else if (std::holds_alternative<std::shared_ptr<Class>>(callee)) {
            auto klass = std::get<std::shared_ptr<Class>>(callee);
            
            if (arguments.size() != klass->arity()) {
                throw std::runtime_error("Expected " + 
                                        std::to_string(klass->arity()) + 
                                        " arguments but got " + 
                                        std::to_string(arguments.size()));
            }
            
            lastValue = klass->call(this, arguments);
        } else if (std::holds_alternative<std::shared_ptr<BoundMethod>>(callee)) {
            auto bound = std::get<std::shared_ptr<BoundMethod>>(callee);

            if (arguments.size() != bound->arity()) {
                throw std::runtime_error("Expected " +
                                        std::to_string(bound->arity()) +
                                        " arguments but got " +
                                        std::to_string(arguments.size()));
            }

            lastValue = bound->call(this, arguments);
        } else {
            throw std::runtime_error("Can only call functions and classes");
        }
    }
    
    void visit(ExpressionStmt* node) override {
        node->expression->accept(this);
    }
    
    void visit(PrintStmt* node) override {
        node->expression->accept(this);
        std::cout << valueToString(lastValue) << std::endl;
    }
    
    void visit(VarStmt* node) override {
        Value value = nullptr;
        
        if (node->initializer) {
            node->initializer->accept(this);
            value = lastValue;
        }
        
        environment->define(node->name.lexeme, value, node->isConst);
    }
    
    void visit(BlockStmt* node) override {
        executeBlock(node->statements, std::make_shared<Environment>(environment));
    }
    
    void visit(IfStmt* node) override {
        node->condition->accept(this);
        
        if (isTruthy(lastValue)) {
            node->thenBranch->accept(this);
        } else if (node->elseBranch) {
            node->elseBranch->accept(this);
        }
    }
    
    void visit(WhileStmt* node) override {
        while (true) {
            node->condition->accept(this);
            
            if (!isTruthy(lastValue)) {
                break;
            }
            
            try {
                node->body->accept(this);
            } catch (const BreakException&) {
                break;
            } catch (const ContinueException&) {
                // continue to next iteration
            }
        }
    }

    void visit(DoWhileStmt* node) override {
        do {
            try {
                node->body->accept(this);
            } catch (const BreakException&) {
                break;
            } catch (const ContinueException&) {
                // continue to condition check
            }
            
            node->condition->accept(this);
        } while (isTruthy(lastValue));
    }

    void visit(ReturnStmt* node) override {
        Value value = nullptr;
        if (node->value != nullptr) {
            node->value->accept(this);
            value = lastValue;
        }
        throw ReturnException(value);
    }

    void visit(BreakStmt* node) override {
        throw BreakException();
    }

    void visit(ContinueStmt* node) override {
        throw ContinueException();
    }

    void visit(ForStmt* node) override {
        if (node->initializer != nullptr) {
            node->initializer->accept(this);
        }

        while (true) {
            if (node->condition != nullptr) {
                node->condition->accept(this);
                if (!isTruthy(lastValue)) {
                    break;
                }
            }

            try {
                node->body->accept(this);
            } catch (const BreakException&) {
                break;
            } catch (const ContinueException&) {
                // fall through to increment
            }

            if (node->increment != nullptr) {
                node->increment->accept(this);
            }
        }
    }

    void visit(ForeachStmt* node) override {
        node->iterable->accept(this);
        Value iterable = lastValue;

        if (!std::holds_alternative<std::shared_ptr<ListValue>>(iterable)) {
            throw std::runtime_error("Foreach requires a list");
        }

        auto& list = std::get<std::shared_ptr<ListValue>>(iterable)->elements;

        for (size_t i = 0; i < list.size(); i++) {
            std::shared_ptr<Environment> loopEnv = std::make_shared<Environment>(environment);
            loopEnv->define(node->name.lexeme, list[i]);

            try {
                executeBlock({node->body}, loopEnv);
            } catch (const BreakException&) {
                break;
            } catch (const ContinueException&) {
                continue;
            }
        }
    }

    void visit(SwitchStmt* node) override {
        node->expression->accept(this);
        Value switchValue = lastValue;
        
        for (auto& case_ : node->cases) {
            bool matched = false;
            if (case_.value) {
                case_.value->accept(this);
                if (switchValue == lastValue) {
                    matched = true;
                }
            } else {
                matched = true;  // default case
            }
            
            if (matched) {
                try {
                    for (auto* stmt : case_.body) {
                        stmt->accept(this);
                    }
                } catch (const BreakException&) {
                    // Exit the switch
                } catch (const ContinueException&) {
                    throw;  // Propagate to enclosing loop
                }
                break;  // No fall-through
            }
        }
    }

    void visit(FunctionNode* node) override {
        std::shared_ptr<Function> function = std::make_shared<Function>(node, environment);
        environment->define(node->name, function);
    }
    
    void visit(FieldDeclNode* node) override {
    }
    
    void visit(ClassNode* node) override {
        environment->define(node->name, nullptr);
        
        // Resolve parent class
        std::shared_ptr<Class> parentClass = nullptr;
        if (!node->parentName.empty()) {
            Value parentVal = environment->get(node->parentName);
            if (!std::holds_alternative<std::shared_ptr<Class>>(parentVal)) {
                throw std::runtime_error("Parent '" + node->parentName + "' is not a class");
            }
            parentClass = std::get<std::shared_ptr<Class>>(parentVal);
        }
        
        // Create the class first so we can pass it as owningClass
        auto klass = std::make_shared<Class>(node->name, 
            std::unordered_map<std::string, std::shared_ptr<Function>>(),
            std::vector<Class::FieldDecl>(), environment, parentClass);
        
        klass->setAbstract(node->isAbstract);
        
        std::unordered_map<std::string, std::shared_ptr<Function>> methods;
        std::unordered_map<std::string, std::shared_ptr<Function>> staticMethods;
        for (auto& method : node->methods) {
            std::shared_ptr<Function> function = std::make_shared<Function>(method, environment, method->accessModifier, klass);
            if (method->isStatic) {
                staticMethods[method->name] = function;
            } else {
                methods[method->name] = function;
            }
        }
        
        std::vector<Class::FieldDecl> fieldDecls;
        std::unordered_set<std::string> staticFieldNames;
        for (auto& field : node->fields) {
            fieldDecls.push_back({field->name, field->initializer, field->accessModifier, field->isConst});
            if (field->isStatic) {
                staticFieldNames.insert(field->name);
            }
        }
        
        // Initialize static fields
        for (auto& field : node->fields) {
            if (field->isStatic && field->initializer) {
                field->initializer->accept(this);
                klass->setStaticField(field->name, lastValue);
            } else if (field->isStatic) {
                klass->setStaticField(field->name, nullptr);
            }
        }
        
        // Resolve interfaces and traits
        std::vector<std::shared_ptr<Interface>> interfaces;
        std::vector<std::shared_ptr<Trait>> traits;
        for (auto& ifaceName : node->interfaceNames) {
            Value val = environment->get(ifaceName);
            if (std::holds_alternative<std::shared_ptr<Interface>>(val)) {
                interfaces.push_back(std::get<std::shared_ptr<Interface>>(val));
            } else if (std::holds_alternative<std::shared_ptr<Trait>>(val)) {
                traits.push_back(std::get<std::shared_ptr<Trait>>(val));
            } else {
                throw std::runtime_error("'" + ifaceName + "' is not an interface or trait");
            }
        }
        
        // Update the class with methods, fields, interfaces, and traits
        klass->setMethods(methods);
        klass->setStaticMethods(staticMethods);
        klass->setFieldDecls(fieldDecls);
        klass->setStaticFieldNames(staticFieldNames);
        klass->setInterfaces(interfaces);
        klass->setTraits(traits);
        klass->applyTraitMethods();
        
        environment->assign(node->name, klass);
    }

    void visit(InterfaceNode* node) override {
        // Resolve parent interfaces
        std::vector<std::shared_ptr<Interface>> parentIfaces;
        for (auto& parentName : node->parentNames) {
            Value parentVal = environment->get(parentName);
            if (!std::holds_alternative<std::shared_ptr<Interface>>(parentVal)) {
                throw std::runtime_error("'" + parentName + "' is not an interface");
            }
            parentIfaces.push_back(std::get<std::shared_ptr<Interface>>(parentVal));
        }
        
        environment->define(node->name, nullptr);
        
        // Create the interface
        std::unordered_map<std::string, std::shared_ptr<Function>> methods;
        for (auto& method : node->methods) {
            auto func = std::make_shared<Function>(method, environment);
            methods[method->name] = func;
        }
        
        auto iface = std::make_shared<Interface>(node->name, methods, parentIfaces);
        environment->assign(node->name, iface);
    }

    void visit(TraitNode* node) override {
        // Resolve parent traits
        std::vector<std::shared_ptr<Trait>> parentTraits;
        for (auto& parentName : node->parentNames) {
            Value parentVal = environment->get(parentName);
            if (!std::holds_alternative<std::shared_ptr<Trait>>(parentVal)) {
                throw std::runtime_error("'" + parentName + "' is not a trait");
            }
            parentTraits.push_back(std::get<std::shared_ptr<Trait>>(parentVal));
        }
        
        environment->define(node->name, nullptr);
        
        // Create the trait
        std::unordered_map<std::string, std::shared_ptr<Function>> methods;
        for (auto& method : node->methods) {
            auto func = std::make_shared<Function>(method, environment);
            methods[method->name] = func;
        }
        
        auto trait = std::make_shared<Trait>(node->name, methods, parentTraits);
        environment->assign(node->name, trait);
    }

    void visit(StaticGetExpr* node) override {
        Value classVal = environment->get(node->className.lexeme);
        if (!std::holds_alternative<std::shared_ptr<Class>>(classVal)) {
            throw std::runtime_error("'" + node->className.lexeme + "' is not a class");
        }
        auto klass = std::get<std::shared_ptr<Class>>(classVal);
        lastValue = klass->getStaticField(node->memberName.lexeme);
    }

    void visit(StaticCallExpr* node) override {
        Value classVal = environment->get(node->className.lexeme);
        if (!std::holds_alternative<std::shared_ptr<Class>>(classVal)) {
            throw std::runtime_error("'" + node->className.lexeme + "' is not a class");
        }
        auto klass = std::get<std::shared_ptr<Class>>(classVal);
        
        auto method = klass->findStaticMethod(node->memberName.lexeme);
        if (!method) {
            throw std::runtime_error("Static method '" + node->memberName.lexeme + "' not found in class '" + node->className.lexeme + "'");
        }
        
        std::vector<Value> args;
        for (auto& arg : node->arguments) {
            arg->accept(this);
            args.push_back(lastValue);
        }
        
        // Create environment with static method's closure
        auto env = std::make_shared<Environment>(method->getClosure());
        for (size_t i = 0; i < method->getDeclaration()->params.size(); i++) {
            env->define(method->getDeclaration()->params[i], args[i]);
        }
        
        auto prevEnv = environment;
        environment = env;
        
        try {
            executeBlock(method->getDeclaration()->body, env);
        } catch (const ReturnException& returnValue) {
            lastValue = returnValue.value;
            environment = prevEnv;
            return;
        }
        
        lastValue = nullptr;
        environment = prevEnv;
    }

    void visit(StaticSetExpr* node) override {
        Value classVal = environment->get(node->className.lexeme);
        if (!std::holds_alternative<std::shared_ptr<Class>>(classVal)) {
            throw std::runtime_error("'" + node->className.lexeme + "' is not a class");
        }
        auto klass = std::get<std::shared_ptr<Class>>(classVal);
        
        node->value->accept(this);
        klass->setStaticField(node->memberName.lexeme, lastValue);
    }

    void visit(LoadStmt* node) override {
        std::string path = node->filename.lexeme;
        // Strip surrounding quotes
        if (path.size() >= 2 && path.front() == '"' && path.back() == '"') {
            path = path.substr(1, path.size() - 2);
        }
        
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Could not load file '" + path + "'");
        }
        
        std::string source((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        
        Lexer lexer(source);
        Parser parser(lexer);
        ASTNode* ast = parser.parse();
        
        if (ast) {
            loadedASTs.push_back(ast);
            ast->accept(this);
        }
    }
};

// Implementation of Function::call that depends on InterpreterVisitor
Value Function::call(InterpreterVisitor* interpreter, const std::vector<Value>& arguments) {
    std::shared_ptr<Environment> environment = std::make_shared<Environment>(closure);
    
    for (size_t i = 0; i < declaration->params.size(); i++) {
        environment->define(declaration->params[i], arguments[i]);
    }
    
    try {
        interpreter->executeBlock(declaration->body, environment);
    } catch (const ReturnException& returnValue) {
        return returnValue.value;
    }
    
    return nullptr;
}

// Implementation of BoundMethod::call that depends on InterpreterVisitor
Value BoundMethod::call(InterpreterVisitor* interpreter, const std::vector<Value>& arguments) {
    std::shared_ptr<Environment> environment = std::make_shared<Environment>(method->getClosure());
    environment->define("this", receiver);

    for (size_t i = 0; i < method->getDeclaration()->params.size(); i++) {
        environment->define(method->getDeclaration()->params[i], arguments[i]);
    }

    auto prevClass = interpreter->currentClass;
    interpreter->currentClass = method->getOwningClass();

    try {
        interpreter->executeBlock(method->getDeclaration()->body, environment);
    } catch (const ReturnException& returnValue) {
        interpreter->currentClass = prevClass;
        return returnValue.value;
    }

    interpreter->currentClass = prevClass;
    return nullptr;
}

// Implementation of Class::findMethod that depends on Interface (defined above)
std::shared_ptr<Function> Class::findMethod(const std::string& methodName) const {
    auto it = methods.find(methodName);
    if (it != methods.end()) {
        return it->second;
    }
    if (parent) {
        return parent->findMethod(methodName);
    }
    for (auto& iface : interfaces) {
        auto result = iface->findMethod(methodName);
        if (result) return result;
    }
    for (auto& trait : traits) {
        auto result = trait->findMethod(methodName);
        if (result && !result->isAbstract()) return result;
    }
    return nullptr;
}

// Implementation of Class::call that depends on InterpreterVisitor
Value Class::call(InterpreterVisitor* interpreter, const std::vector<Value>& arguments) {
    if (isAbstract) {
        throw std::runtime_error("Cannot instantiate abstract class '" + name + "'");
    }
    
    // Verify all abstract methods from parent chain are implemented
    auto klass = shared_from_this();
    while (klass) {
        for (auto& [methodName, method] : klass->methods) {
            if (method->getDeclaration()->isAbstract) {
                if (!findMethod(methodName) || findMethod(methodName)->getDeclaration()->isAbstract) {
                    throw std::runtime_error("Class '" + name + "' must implement abstract method '" + methodName + "'");
                }
            }
        }
        klass = klass->parent;
    }
    
    // Verify all interface methods are implemented
    for (auto& iface : interfaces) {
        for (auto& [methodName, method] : iface->getMethods()) {
            auto impl = findMethod(methodName);
            if (!impl || impl->getDeclaration()->isAbstract) {
                throw std::runtime_error("Class '" + name + "' must implement interface method '" + methodName + "'");
            }
        }
    }
    
    std::shared_ptr<Instance> instance = std::make_shared<Instance>(shared_from_this());
    
    // Evaluate field initializers from the whole hierarchy
    std::vector<std::shared_ptr<Class>> hierarchy;
    auto hierarchyKlass = shared_from_this();
    while (hierarchyKlass) {
        hierarchy.push_back(hierarchyKlass);
        hierarchyKlass = hierarchyKlass->getParent();
    }
    // Walk from topmost parent to most derived
    for (auto it = hierarchy.rbegin(); it != hierarchy.rend(); ++it) {
        auto& fds = (*it)->getFieldDecls();
        auto cls = (*it)->getClosure();
        if (!fds.empty() && cls) {
            auto env = std::make_shared<Environment>(cls);
            env->define("this", instance);
            interpreter->executeFieldInitializers(instance, fds, env);
        }
    }
    
    auto prevClass = interpreter->currentClass;
    interpreter->currentClass = shared_from_this();
    
    // Call the initializer if it exists
    std::shared_ptr<Function> initializer = findMethod("init");
    if (initializer) {
        std::shared_ptr<BoundMethod> boundInit = std::make_shared<BoundMethod>(instance, initializer);
        boundInit->call(interpreter, arguments);
    }
    
    interpreter->currentClass = prevClass;
    return instance;
}

void Interpreter::execute(ASTNode* ast) {
    if (!ast) {
        ErrorHandling::reportError("AST is null");
        return;
    }
    
    try {
        InterpreterVisitor visitor;
        ast->accept(&visitor);
    } catch (const std::exception& e) {
        ErrorHandling::reportError("Runtime error: " + std::string(e.what()));
    }
}
