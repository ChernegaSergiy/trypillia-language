#ifndef AST_H
#define AST_H

#include <vector>
#include <string>
#include "../lexer/Lexer.h"

// Forward declaration
class ASTVisitor;

class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor* visitor) = 0;
};

class ExprNode : public ASTNode {
public:
    virtual ~ExprNode() = default;
};

class StmtNode : public ASTNode {
public:
    virtual ~StmtNode() = default;
};

class ProgramNode : public ASTNode {
public:
    std::vector<ASTNode*> declarations;
    
    ProgramNode(const std::vector<ASTNode*>& declarations) : declarations(declarations) {}
    virtual ~ProgramNode() {
        for (auto decl : declarations) {
            delete decl;
        }
    }
    
    void accept(ASTVisitor* visitor) override;
};

class BinaryExpr : public ExprNode {
public:
    ExprNode* left;
    Token op;
    ExprNode* right;
    
    BinaryExpr(ExprNode* left, Token op, ExprNode* right) 
        : left(left), op(op), right(right) {}
    
    ~BinaryExpr() {
        delete left;
        delete right;
    }
    
    void accept(ASTVisitor* visitor) override;
};

class LiteralExpr : public ExprNode {
public:
    Token value;
    
    LiteralExpr(Token value) : value(value) {}
    
    void accept(ASTVisitor* visitor) override;
};

class VariableExpr : public ExprNode {
public:
    Token name;
    
    VariableExpr(Token name) : name(name) {}
    
    void accept(ASTVisitor* visitor) override;
};

class AssignExpr : public ExprNode {
public:
    Token name;
    ExprNode* value;
    
    AssignExpr(Token name, ExprNode* value) : name(name), value(value) {}
    ~AssignExpr() {
        delete value;
    }
    
    void accept(ASTVisitor* visitor) override;
};

class CallExpr : public ExprNode {
public:
    ExprNode* callee;
    Token paren;
    std::vector<ExprNode*> arguments;
    
    CallExpr(ExprNode* callee, Token paren, std::vector<ExprNode*> arguments)
        : callee(callee), paren(paren), arguments(arguments) {}
    
    ~CallExpr() {
        delete callee;
        for (auto arg : arguments) {
            delete arg;
        }
    }
    
    void accept(ASTVisitor* visitor) override;
};

class ExpressionStmt : public StmtNode {
public:
    ExprNode* expression;
    
    ExpressionStmt(ExprNode* expression) : expression(expression) {}
    ~ExpressionStmt() {
        delete expression;
    }
    
    void accept(ASTVisitor* visitor) override;
};

class PrintStmt : public StmtNode {
public:
    ExprNode* expression;
    
    PrintStmt(ExprNode* expression) : expression(expression) {}
    ~PrintStmt() {
        delete expression;
    }
    
    void accept(ASTVisitor* visitor) override;
};

class VarStmt : public StmtNode {
public:
    Token name;
    ExprNode* initializer;
    
    VarStmt(Token name, ExprNode* initializer) 
        : name(name), initializer(initializer) {}
    
    ~VarStmt() {
        delete initializer;
    }
    
    void accept(ASTVisitor* visitor) override;
};

class BlockStmt : public StmtNode {
public:
    std::vector<StmtNode*> statements;
    
    BlockStmt(std::vector<StmtNode*> statements) : statements(statements) {}
    
    ~BlockStmt() {
        for (auto stmt : statements) {
            delete stmt;
        }
    }
    
    void accept(ASTVisitor* visitor) override;
};

class IfStmt : public StmtNode {
public:
    ExprNode* condition;
    StmtNode* thenBranch;
    StmtNode* elseBranch;
    
    IfStmt(ExprNode* condition, StmtNode* thenBranch, StmtNode* elseBranch)
        : condition(condition), thenBranch(thenBranch), elseBranch(elseBranch) {}
    
    ~IfStmt() {
        delete condition;
        delete thenBranch;
        delete elseBranch;
    }
    
    void accept(ASTVisitor* visitor) override;
};

class WhileStmt : public StmtNode {
public:
    ExprNode* condition;
    StmtNode* body;
    
    WhileStmt(ExprNode* condition, StmtNode* body)
        : condition(condition), body(body) {}
    
    ~WhileStmt() {
        delete condition;
        delete body;
    }
    
    void accept(ASTVisitor* visitor) override;
};

class FunctionNode : public StmtNode {
public:
    std::string name;
    std::vector<std::string> params;
    std::vector<StmtNode*> body;
    
    FunctionNode(std::string name, std::vector<std::string> params, std::vector<StmtNode*> body)
        : name(name), params(params), body(body) {}
    
    FunctionNode() : name(""), params(), body() {}
    
    ~FunctionNode() {
        for (auto stmt : body) {
            delete stmt;
        }
    }
    
    void accept(ASTVisitor* visitor) override;
};

class ClassNode : public StmtNode {
public:
    std::string name;
    std::vector<FunctionNode*> methods;
    
    ClassNode(std::string name, std::vector<FunctionNode*> methods)
        : name(name), methods(methods) {}
    
    ~ClassNode() {
        for (auto method : methods) {
            delete method;
        }
    }
    
    void accept(ASTVisitor* visitor) override;
};

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    virtual void visit(ProgramNode* node) = 0;
    virtual void visit(BinaryExpr* node) = 0;
    virtual void visit(LiteralExpr* node) = 0;
    virtual void visit(VariableExpr* node) = 0;
    virtual void visit(AssignExpr* node) = 0;
    virtual void visit(CallExpr* node) = 0;
    virtual void visit(ExpressionStmt* node) = 0;
    virtual void visit(PrintStmt* node) = 0;
    virtual void visit(VarStmt* node) = 0;
    virtual void visit(BlockStmt* node) = 0;
    virtual void visit(IfStmt* node) = 0;
    virtual void visit(WhileStmt* node) = 0;
    virtual void visit(FunctionNode* node) = 0;
    virtual void visit(ClassNode* node) = 0;
};

#endif // AST_H
