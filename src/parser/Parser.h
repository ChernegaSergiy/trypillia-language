#ifndef PARSER_H
#define PARSER_H

#include "Lexer.h"
#include "AST.h"

class Parser {
public:
    Parser(Lexer &lexer);
    ASTNode* parse();

private:
    Lexer &lexer;
    Token currentToken;

    void advance();
    void consume(TokenType type);
    ASTNode* expression();
    ASTNode* statement();
    ASTNode* declaration();
    ASTNode* parseFunction();
    ASTNode* parseClass();
};

#endif // PARSER_H
