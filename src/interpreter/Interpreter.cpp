#include "Interpreter.h"
#include "../utils/ErrorHandling.h"
#include <iostream>
#include <string>
#include <variant>
#include <memory>
#include <unordered_map>
#include <functional>

// Value type for the interpreter
using Value = std::variant<std::nullptr_t, bool, double, std::string>;

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
    Value lastValue;
    
public:
    InterpreterVisitor() : environment(std::make_shared<Environment>()) {}
    
    Value getValue() const {
        return lastValue;
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
        // This would be where we implement function calls
        // For simplicity, we'll just set lastValue to nullptr
        lastValue = nullptr;
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
        // For simplicity, we're not implementing function calls
        // In a real interpreter, we would define a callable object here
    }
    
    void visit(ClassNode* node) override {
        // For simplicity, we're not implementing classes
        // In a real interpreter, we would define a class object here
    }
};

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
