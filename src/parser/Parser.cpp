#include "Parser.h"
#include <stdexcept>

Parser::Parser(Lexer &lexer) : lexer(lexer) {
    advance();
}

void Parser::advance() {
    currentToken = lexer.nextToken();
}

void Parser::consume(TokenType type) {
    if (currentToken.type == type) {
        advance();
    } else {
        throw std::runtime_error("Unexpected token");
    }
}

ASTNode* Parser::parse() {
    std::vector<ASTNode*> declarations;
    while (currentToken.type != TokenType::END_OF_FILE) {
        declarations.push_back(declaration());
    }
    return new ProgramNode(declarations); // Need to define ProgramNode
}

ASTNode* Parser::expression() {
    ASTNode* node = term();
    while (currentToken.type == TokenType::PLUS || 
           currentToken.type == TokenType::MINUS) {
        Token op = currentToken;
        advance();
        node = new BinaryExpr(node, op, term());
    }
    return node;
}

ASTNode* Parser::declaration() {
    // Example for parsing a function declaration
    if (currentToken.type == TokenType::FN) {
        return parseFunction();
    }
    // Handle other declarations
    return nullptr;
}

ASTNode* Parser::parseFunction() {
    consume(TokenType::FN);
    // Parse function name, parameters, and body
    return new FunctionNode(); // Placeholder
}

ASTNode* Parser::parseClass() {
    // Implement class parsing logic
    return nullptr;
}
