#include "SemanticAnalyzer.h"
#include "../symbol/SymbolTable.h"
#include "../utils/ErrorHandling.h"
#include "../lexer/Lexer.h"
#include "../parser/Parser.h"
#include <stdexcept>
#include <iostream>
#include <fstream>
#include "../native/StdLib.h"

class SemanticVisitor : public ASTVisitor {
private:
    SymbolTable* currentScope;
    
public:
    SemanticVisitor() : currentScope(new SymbolTable()) {
        StdLib::registerSymbols(currentScope);
    }
    
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

    void visit(CompoundAssignExpr* node) override {
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

    void visit(UnaryExpr* node) override {
        if (node->op.type == TokenType::PLUS_PLUS || node->op.type == TokenType::MINUS_MINUS) {
            VariableExpr* var = dynamic_cast<VariableExpr*>(node->right);
            if (!var) {
                ErrorHandling::reportError("Invalid prefix expression target");
                return;
            }
            Symbol* symbol = currentScope->resolve(var->name.lexeme);
            if (!symbol) {
                std::string error = "Undefined variable '" + var->name.lexeme + "'";
                ErrorHandling::reportError(error);
            } else if (symbol->isConst) {
                std::string error = "Cannot modify const variable '" + var->name.lexeme + "'";
                ErrorHandling::reportError(error);
            }
            return;
        }
        node->right->accept(this);
    }

    void visit(PostfixExpr* node) override {
        Symbol* symbol = currentScope->resolve(node->name.lexeme);
        if (!symbol) {
            std::string error = "Undefined variable '" + node->name.lexeme + "'";
            ErrorHandling::reportError(error);
        } else if (symbol->isConst) {
            std::string error = "Cannot modify const variable '" + node->name.lexeme + "'";
            ErrorHandling::reportError(error);
        }
    }

    void visit(TernaryExpr* node) override {
        node->condition->accept(this);
        node->thenBranch->accept(this);
        node->elseBranch->accept(this);
    }

    void visit(ListExpr* node) override {
        for (auto* el : node->elements) {
            el->accept(this);
        }
    }

    void visit(IndexGetExpr* node) override {
        node->object->accept(this);
        node->index->accept(this);
    }

    void visit(IndexSetExpr* node) override {
        node->object->accept(this);
        node->index->accept(this);
        node->value->accept(this);
    }

    void visit(ThisExpr* node) override {
        Symbol* symbol = currentScope->resolve("this");
        if (!symbol) {
            std::string error = "'this' can only be used inside a method";
            ErrorHandling::reportError(error);
        }
    }
    
    void visit(SuperExpr* node) override {
        Symbol* symbol = currentScope->resolve("this");
        if (!symbol) {
            std::string error = "'super' can only be used inside a method";
            ErrorHandling::reportError(error);
        }
    }
    

    void visit(GetExpr* node) override {
        node->object->accept(this);
    }

    void visit(SetExpr* node) override {
        node->object->accept(this);
        node->value->accept(this);
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
        symbol.isConst = node->isConst;
        
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

    void visit(DoWhileStmt* node) override {
        node->body->accept(this);
        node->condition->accept(this);
    }

    void visit(ReturnStmt* node) override {
        if (node->value != nullptr) {
            node->value->accept(this);
        }
    }

    void visit(BreakStmt* node) override {
    }

    void visit(ContinueStmt* node) override {
    }

    void visit(ForStmt* node) override {
        if (node->initializer != nullptr) {
            node->initializer->accept(this);
        }
        if (node->condition != nullptr) {
            node->condition->accept(this);
        }
        if (node->increment != nullptr) {
            node->increment->accept(this);
        }
        node->body->accept(this);
    }

    void visit(ForeachStmt* node) override {
        node->iterable->accept(this);

        currentScope = new SymbolTable(currentScope);
        Symbol symbol;
        symbol.name = node->name.lexeme;
        symbol.type = "";
        symbol.isConst = false;
        currentScope->define(symbol);

        node->body->accept(this);

        currentScope = currentScope->getParent();
    }

    void visit(SwitchStmt* node) override {
        node->expression->accept(this);
        for (auto& caseClause : node->cases) {
            if (caseClause.value) caseClause.value->accept(this);
            for (auto& stmt : caseClause.body) stmt->accept(this);
        }
    }

    void visit(UsingStmt* node) override {
        SymbolTable* enclosingScope = currentScope;
        currentScope = new SymbolTable(enclosingScope);
        
        node->declaration->accept(this);
        node->body->accept(this);
        
        SymbolTable* previous = currentScope;
        currentScope = currentScope->getParent();
        delete previous;
    }

    void visit(LambdaExpr* node) override {
        SymbolTable* enclosingScope = currentScope;
        currentScope = new SymbolTable(enclosingScope);
        
        for (const auto& param : node->params) {
            Symbol symbol;
            symbol.name = param;
            symbol.type = "";
            symbol.isConst = false;
            currentScope->define(symbol);
        }
        
        for (auto& stmt : node->body) {
            stmt->accept(this);
        }
        
        SymbolTable* previous = currentScope;
        currentScope = currentScope->getParent();
        delete previous;
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
    
    void visit(FieldDeclNode* node) override {
        Symbol fieldSymbol;
        fieldSymbol.name = node->name;
        fieldSymbol.type = "field";
        fieldSymbol.isConst = node->isConst;
        currentScope->define(fieldSymbol);
        
        if (node->initializer) {
            node->initializer->accept(this);
        }
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
        
        // Resolve parent class
        if (!node->parentName.empty()) {
            Symbol* parentSymbol = currentScope->resolve(node->parentName);
            if (!parentSymbol) {
                std::string error = "Parent class '" + node->parentName + "' not found";
                ErrorHandling::reportError(error);
            } else if (parentSymbol->type != "class") {
                std::string error = "'" + node->parentName + "' is not a class";
                ErrorHandling::reportError(error);
            }
        }
        
        // Resolve implemented interfaces and traits
        for (auto& ifaceName : node->interfaceNames) {
            Symbol* ifaceSymbol = currentScope->resolve(ifaceName);
            if (!ifaceSymbol) {
                std::string error = "Interface/trait '" + ifaceName + "' not found";
                ErrorHandling::reportError(error);
            } else if (ifaceSymbol->type != "interface" && ifaceSymbol->type != "trait") {
                std::string error = "'" + ifaceName + "' is not an interface or trait";
                ErrorHandling::reportError(error);
            }
        }
        
        SymbolTable* enclosingScope = currentScope;
        currentScope = new SymbolTable(enclosingScope);

        Symbol thisSymbol;
        thisSymbol.name = "this";
        thisSymbol.type = "instance";
        thisSymbol.isConst = true;
        currentScope->define(thisSymbol);
        
        for (auto& field : node->fields) {
            field->accept(this);
        }

        for (auto& method : node->methods) {
            method->accept(this);
        }
        
        SymbolTable* previous = currentScope;
        currentScope = currentScope->getParent();
        delete previous;
    }

    void visit(InterfaceNode* node) override {
        Symbol ifaceSymbol;
        ifaceSymbol.name = node->name;
        ifaceSymbol.type = "interface";
        ifaceSymbol.isConst = true;
        if (!currentScope->define(ifaceSymbol)) {
            std::string error = "Interface '" + node->name + "' already defined in this scope";
            ErrorHandling::reportError(error);
        }
        
        for (auto& method : node->methods) {
            method->accept(this);
        }
    }

    void visit(TraitNode* node) override {
        Symbol traitSymbol;
        traitSymbol.name = node->name;
        traitSymbol.type = "trait";
        traitSymbol.isConst = true;
        if (!currentScope->define(traitSymbol)) {
            std::string error = "Trait '" + node->name + "' already defined in this scope";
            ErrorHandling::reportError(error);
        }
        
        for (auto& method : node->methods) {
            method->accept(this);
        }
    }

    void visit(StaticGetExpr* node) override {}
    void visit(StaticCallExpr* node) override {}
    void visit(StaticSetExpr* node) override {}
    void visit(LoadStmt* node) override {
        std::string path = node->filename.lexeme;
        if (path.size() >= 2 && path.front() == '"' && path.back() == '"') {
            path = path.substr(1, path.size() - 2);
        }
        
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Semantic error: Could not load file '" << path << "'" << std::endl;
            return;
        }
        
        std::string source((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
                            
        Lexer lexer(source);
        Parser parser(lexer);
        ASTNode* ast = parser.parse();
        
        if (ast) {
            ast->accept(this);
            delete ast;
        }
    }

    void visit(DictExpr* node) override {
        for (auto& pair : node->elements) {
            pair.first->accept(this);
            pair.second->accept(this);
        }
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
