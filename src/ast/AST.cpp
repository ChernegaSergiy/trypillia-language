#include "AST.h"

// Implementation of accept methods for each AST node type

void ProgramNode::accept(ASTVisitor* visitor) {
    visitor->visit(this);
}

void UnaryExpr::accept(ASTVisitor* visitor) {
    visitor->visit(this);
}

void ThisExpr::accept(ASTVisitor* visitor) {
    visitor->visit(this);
}

void GetExpr::accept(ASTVisitor* visitor) {
    visitor->visit(this);
}

void SetExpr::accept(ASTVisitor* visitor) {
    visitor->visit(this);
}

void PostfixExpr::accept(ASTVisitor* visitor) {
    visitor->visit(this);
}

void TernaryExpr::accept(ASTVisitor* visitor) {
    visitor->visit(this);
}

void BinaryExpr::accept(ASTVisitor* visitor) {
    visitor->visit(this);
}

void LiteralExpr::accept(ASTVisitor* visitor) {
    visitor->visit(this);
}

void VariableExpr::accept(ASTVisitor* visitor) {
    visitor->visit(this);
}

void AssignExpr::accept(ASTVisitor* visitor) {
    visitor->visit(this);
}

void CompoundAssignExpr::accept(ASTVisitor* visitor) {
    visitor->visit(this);
}

void CallExpr::accept(ASTVisitor* visitor) {
    visitor->visit(this);
}

void ExpressionStmt::accept(ASTVisitor* visitor) {
    visitor->visit(this);
}

void PrintStmt::accept(ASTVisitor* visitor) {
    visitor->visit(this);
}

void VarStmt::accept(ASTVisitor* visitor) {
    visitor->visit(this);
}

void BlockStmt::accept(ASTVisitor* visitor) {
    visitor->visit(this);
}

void IfStmt::accept(ASTVisitor* visitor) {
    visitor->visit(this);
}

void WhileStmt::accept(ASTVisitor* visitor) {
    visitor->visit(this);
}

void ReturnStmt::accept(ASTVisitor* visitor) {
    visitor->visit(this);
}

void ForStmt::accept(ASTVisitor* visitor) {
    visitor->visit(this);
}

void FunctionNode::accept(ASTVisitor* visitor) {
    visitor->visit(this);
}

void ClassNode::accept(ASTVisitor* visitor) {
    visitor->visit(this);
}
