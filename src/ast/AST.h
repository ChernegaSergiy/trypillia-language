#ifndef AST_H
#define AST_H

#include <vector>
#include <string>

class ASTNode {
public:
    virtual ~ASTNode() = default;
};

class ExprNode : public ASTNode {};
class StmtNode : public ASTNode {};

class BinaryExpr : public ExprNode {
public:
    ExprNode* left;
    Token op;
    ExprNode* right;
};

class FunctionNode : public StmtNode {
public:
    std::string name;
    std::vector<std::string> params;
    std::vector<StmtNode*> body;
};

class ClassNode : public StmtNode {
public:
    std::string name;
    std::vector<FunctionNode*> methods;
};

#endif // AST_H
