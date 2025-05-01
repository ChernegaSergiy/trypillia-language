#include "Interpreter.h"
#include "../utils/ErrorHandling.h"
#include <iostream>
#include <string>
#include <variant>
#include <memory>
#include <unordered_map>
#include <functional>
#include <vector>

// Forward declarations
class Function;
class Class;
class Instance;

// Value type for the interpreter
using Value = std::variant<std::nullptr_t, bool, double, std::string, 
                           std::shared_ptr<Function>, std::shared_ptr<Class>, 
                           std::shared_ptr<Instance>>;

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
    
    std::string toString() const {
        return "<fn " + declaration->name + ">";
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
class Instance {
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
            // Bind the method to this instance
            // In a more complete implementation, this would create a 'bound method'
            return method;
        }
        
        throw std::runtime_error("Undefined property '" + name + "'");
    }
    
    void set(const std::string& name, const Value& value) {
        fields[name] = value;
    }
    
    std::string toString() const {
        return "<instance of " + std::get<std::shared_ptr<Class>>(klass)->toString() + ">";
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
        std::string result = std::to_string(num);
        
        // Remove trailing zeros
        if (result.find('.') != std::string::npos) {
            result = result.substr(0, result.find_last_not_of('0') + 1);
            if (result.back() == '.') {
                result.pop_back();
            }
        }
        
        return result;
    } else if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    } else if (std::holds_alternative<std::shared_ptr<Function>>(value)) {
        return std::get<std::shared_ptr<Function>>(value)->toString();
    } else if (std::holds_alternative<std::shared_ptr<Class>>(value)) {
        return std::get<std::shared_ptr<Class>>(value)->toString();
    } else if (std::holds_alternative<std::shared_ptr<Instance>>(value)) {
        return std::get<std::shared_ptr<Instance>>(value)->toString();
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
    std::shared_ptr<Environment> environment;
    std::shared_ptr<Environment> globals;
    Value lastValue;
    
public:
    InterpreterVisitor() : globals(std::make_shared<Environment>()), environment(globals) {
        // Add native functions to global environment
        // For example, a 'clock' function could be added here
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
            
            node->body->accept(this);
        }
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
    } catch (const std::runtime_error& returnValue) {
        // In a more complete implementation, this would handle return statements
        // by catching a special exception
    }
    
    return nullptr;
}

// Implementation of Class::call that depends on InterpreterVisitor
Value Class::call(InterpreterVisitor* interpreter, const std::vector<Value>& arguments) {
    std::shared_ptr<Instance> instance = std::make_shared<Instance>(std::make_shared<Class>(*this));
    
    // Call the initializer if it exists
    std::shared_ptr<Function> initializer = findMethod("init");
    if (initializer) {
        initializer->call(interpreter, arguments);
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
