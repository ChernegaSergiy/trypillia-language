#include "Interpreter.h"
#include "../utils/ErrorHandling.h"
#include <stdexcept>
#include <iostream>
#include <string>
#include <variant>
#include <cmath>
#include <memory>
#include <unordered_map>
#include <functional>
#include <vector>

// Forward declarations
class Function;
class Class;
class Instance;
class BoundMethod;
class ListValue;

// Value type for the interpreter
using Value = std::variant<std::nullptr_t, bool, double, std::string, std::shared_ptr<ListValue>,
                           std::shared_ptr<Function>, std::shared_ptr<Class>, 
                           std::shared_ptr<Instance>, std::shared_ptr<BoundMethod>>;

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
    std::shared_ptr<Environment> enclosing;
    
public:
    Environment(std::shared_ptr<Environment> enclosing = nullptr)
        : enclosing(enclosing) {}
    
    void define(const std::string& name, const Value& value) {
        values[name] = value;
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
    
public:
    Function(FunctionNode* declaration, std::shared_ptr<Environment> closure)
        : declaration(declaration), closure(closure) {}
    
    int arity() const override {
        return declaration->params.size();
    }
    
    Value call(InterpreterVisitor* interpreter, const std::vector<Value>& arguments) override;
    
    std::shared_ptr<Environment> getClosure() const { return closure; }
    FunctionNode* getDeclaration() const { return declaration; }
    
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

    std::string toString() const {
        return "<bound method>";
    }
};

// Class implementation
class Class : public Callable {
private:
    std::string name;
    std::unordered_map<std::string, std::shared_ptr<Function>> methods;
    
public:
    Class(const std::string& name, const std::unordered_map<std::string, std::shared_ptr<Function>>& methods)
        : name(name), methods(methods) {}
    
    int arity() const override {
        auto initMethod = findMethod("init");
        if (initMethod) {
            return initMethod->arity();
        }
        return 0;
    }
    
    Value call(InterpreterVisitor* interpreter, const std::vector<Value>& arguments) override;
    
    std::shared_ptr<Function> findMethod(const std::string& name) const {
        auto it = methods.find(name);
        if (it != methods.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    std::string toString() const {
        return "<class " + name + ">";
    }
};

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
    
public:
    InterpreterVisitor() : globals(std::make_shared<Environment>()), environment(globals) {
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
                } else if (std::holds_alternative<std::string>(left) && std::holds_alternative<std::string>(right)) {
                    lastValue = std::get<std::string>(left) + std::get<std::string>(right);
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

    void visit(GetExpr* node) override {
        node->object->accept(this);
        Value object = lastValue;

        if (std::holds_alternative<std::shared_ptr<Instance>>(object)) {
            auto instance = std::get<std::shared_ptr<Instance>>(object);
            lastValue = instance->get(node->name.lexeme);
        } else {
            throw std::runtime_error("Only instances have properties");
        }
    }

    void visit(SetExpr* node) override {
        node->object->accept(this);
        Value object = lastValue;

        if (!std::holds_alternative<std::shared_ptr<Instance>>(object)) {
            throw std::runtime_error("Only instances have properties");
        }

        node->value->accept(this);
        std::get<std::shared_ptr<Instance>>(object)->set(node->name.lexeme, lastValue);
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
        
        environment->define(node->name.lexeme, value);
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
    }

    void visit(FunctionNode* node) override {
        std::shared_ptr<Function> function = std::make_shared<Function>(node, environment);
        environment->define(node->name, function);
    }
    
    void visit(ClassNode* node) override {
        environment->define(node->name, nullptr);
        
        std::unordered_map<std::string, std::shared_ptr<Function>> methods;
        for (auto& method : node->methods) {
            std::shared_ptr<Function> function = std::make_shared<Function>(method, environment);
            methods[method->name] = function;
        }
        
        std::shared_ptr<Class> klass = std::make_shared<Class>(node->name, methods);
        environment->assign(node->name, klass);
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

    try {
        interpreter->executeBlock(method->getDeclaration()->body, environment);
    } catch (const ReturnException& returnValue) {
        return returnValue.value;
    }

    return nullptr;
}

// Implementation of Class::call that depends on InterpreterVisitor
Value Class::call(InterpreterVisitor* interpreter, const std::vector<Value>& arguments) {
    std::shared_ptr<Instance> instance = std::make_shared<Instance>(std::make_shared<Class>(*this));
    
    // Call the initializer if it exists
    std::shared_ptr<Function> initializer = findMethod("init");
    if (initializer) {
        std::shared_ptr<BoundMethod> boundInit = std::make_shared<BoundMethod>(instance, initializer);
        boundInit->call(interpreter, arguments);
    }
    
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
