#include "SemanticAnalyzer.h"
#include "../symbol/SymbolTable.h"
#include "../utils/ErrorHandling.h"
#include <stdexcept>
#include <iostream>

class SemanticVisitor : public ASTVisitor {
private:
    SymbolTable* currentScope;
    
public:
    SemanticVisitor() : currentScope(new SymbolTable()) {}
    
    ~SemanticVisitor() {
        while (currentScope) {
            SymbolTable* parent = currentScope->getParent();
            delete currentScope;
            currentScope = parent;
        }
    }
    
    void visit(ProgramNode* node) override {
        for (auto& decl : node->declarations) {
            decl->accept(this);
        }
    }
    
    void visit(BinaryExpr* node) override {
        node->left->accept(this);
        node->right->accept(this);
        // Type checking could be implemented here
    }
    
    void visit(LiteralExpr* node) override {
        // Nothing to do for literals
    }
    
    void visit(VariableExpr* node) override {
        Symbol* symbol = currentScope->resolve(node->name.lexeme);
        if (!symbol) {
            std::string error = "Undefined variable '" + node->name.lexeme + "'";
            ErrorHandling::reportError(error);
        }
    }
    
    void visit(AssignExpr* node) override {
        node->value->accept(this);
        
        Symbol* symbol = currentScope->resolve(node->name.lexeme);
        if (!symbol) {
            std::string error = "Undefined variable '" + node->name.lexeme + "'";
            ErrorHandling::reportError(error);
        } else if (symbol->isConst) {
            std::string error = "Cannot assign to const variable '" + node->name.lexeme + "'";
            ErrorHandling::reportError(error);
        }
    }
    
    void visit(CallExpr* node) override {
        node->callee->accept(this);
        
        for (auto& arg : node->arguments) {
            arg->accept(this);
        }
        // Function existence and argument count checks could be added here
    }
    
    void visit(ExpressionStmt* node) override {
        node->expression->accept(this);
    }
    
    void visit(PrintStmt* node) override {
        node->expression->accept(this);
    }
    
    void visit(VarStmt* node) override {
        if (node->initializer) {
            node->initializer->accept(this);
        }
        
        Symbol symbol;
        symbol.name = node->name.lexeme;
        symbol.type = ""; // Type inference would go here
        symbol.isConst = false;
        
        if (!currentScope->define(symbol)) {
            std::string error = "Variable '" + node->name.lexeme + "' already defined in this scope";
            ErrorHandling::reportError(error);
        }
    }
    
    void visit(BlockStmt* node) override {
        SymbolTable* enclosingScope = currentScope;
        currentScope = new SymbolTable(enclosingScope);
        
        for (auto& stmt : node->statements) {
            stmt->accept(this);
        }
        
        SymbolTable* previous = currentScope;
        currentScope = currentScope->getParent();
        delete previous;
    }
    
    void visit(IfStmt* node) override {
        node->condition->accept(this);
        node->thenBranch->accept(this);
        
        if (node->elseBranch) {
            node->elseBranch->accept(this);
        }
    }
    
    void visit(WhileStmt* node) override {
        node->condition->accept(this);
        node->body->accept(this);
    }
    
    void visit(FunctionNode* node) override {
        Symbol functionSymbol;
        functionSymbol.name = node->name;
        functionSymbol.type = "function";
        functionSymbol.isConst = true;
        
        if (!currentScope->define(functionSymbol)) {
            std::string error = "Function '" + node->name + "' already defined in this scope";
            ErrorHandling::reportError(error);
        }
        
        SymbolTable* enclosingScope = currentScope;
        currentScope = new SymbolTable(enclosingScope);
        
        for (const auto& param : node->params) {
            Symbol paramSymbol;
            paramSymbol.name = param;
            paramSymbol.type = "";
            paramSymbol.isConst = false;
            
            if (!currentScope->define(paramSymbol)) {
                std::string error = "Parameter '" + param + "' already defined";
                ErrorHandling::reportError(error);
            }
        }
        
        for (auto& stmt : node->body) {
            stmt->accept(this);
        }
        
        SymbolTable* previous = currentScope;
        currentScope = currentScope->getParent();
        delete previous;
    }
    
    void visit(ClassNode* node) override {
        Symbol classSymbol;
        classSymbol.name = node->name;
        classSymbol.type = "class";
        classSymbol.isConst = true;
        
        if (!currentScope->define(classSymbol)) {
            std::string error = "Class '" + node->name + "' already defined in this scope";
            ErrorHandling::reportError(error);
        }
        
        SymbolTable* enclosingScope = currentScope;
        currentScope = new SymbolTable(enclosingScope);
        
        for (auto& method : node->methods) {
            method->accept(this);
        }
        
        SymbolTable* previous = currentScope;
        currentScope = currentScope->getParent();
        delete previous;
    }
};

void SemanticAnalyzer::analyze(ASTNode* ast) {
    if (!ast) {
        ErrorHandling::reportError("AST is null");
        return;
    }
    
    SemanticVisitor visitor;
    ast->accept(&visitor);
}
