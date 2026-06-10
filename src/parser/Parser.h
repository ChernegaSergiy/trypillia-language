#ifndef PARSER_H
#define PARSER_H

#include "../lexer/Lexer.h"
#include "../ast/AST.h"
#include <initializer_list>

class Parser {
public:
    Parser(Lexer &lexer);
    ASTNode* parse();

private:
    Lexer &lexer;
    Token currentToken;

    void advance();
    void consume(TokenType type);
    bool match(TokenType type);
    bool match(std::initializer_list<TokenType> types);

    // Error recovery
    void synchronize();

    // Expression parsing methods
    ExprNode* expression();
    ExprNode* assignment();
    ExprNode* ternary();
    ExprNode* orExpr();
    ExprNode* andExpr();
    ExprNode* equality();
    ExprNode* comparison();
    ExprNode* term();
    ExprNode* factor();
    ExprNode* unary();
    ExprNode* call();
    ExprNode* finishCall(ExprNode* callee);
    ExprNode* primary();

    // Statement parsing methods
    StmtNode* statement();
    StmtNode* expressionStatement();
    StmtNode* printStatement();
    StmtNode* block();
    StmtNode* ifStatement();
    StmtNode* whileStatement();
    StmtNode* doWhileStatement();
    StmtNode* forStatement();
    StmtNode* finishForLoop(StmtNode* initializer);
    StmtNode* returnStatement();
    StmtNode* breakStatement();
    StmtNode* continueStatement();
    StmtNode* switchStatement();

    // Declaration parsing methods
    ASTNode* declaration();
    StmtNode* varDeclaration();
    FieldDeclNode* parseFieldDecl(AccessModifier accessModifier = AccessModifier::PUBLIC);
    FunctionNode* parseFunction(AccessModifier accessModifier = AccessModifier::PUBLIC, bool isAbstract = false);
    ClassNode* parseClass();
    InterfaceNode* parseInterface();
    TraitNode* parseTrait();
};

#endif // PARSER_H
