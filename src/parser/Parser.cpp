#include "Parser.h"
#include "../ast/AST.h"
#include "../utils/ErrorHandling.h"
#include <stdexcept>
#include <vector>

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
        std::string message = "Expected token type " + std::to_string(static_cast<int>(type)) + 
                            ", got " + std::to_string(static_cast<int>(currentToken.type));
        ErrorHandling::reportError(message);
        throw std::runtime_error(message);
    }
}

// Match checks if the current token is of the expected type
bool Parser::match(TokenType type) {
    if (currentToken.type == type) {
        advance();
        return true;
    }
    return false;
}

bool Parser::match(std::initializer_list<TokenType> types) {
    for (auto type : types) {
        if (currentToken.type == type) {
            advance();
            return true;
        }
    }
    return false;
}

ASTNode* Parser::parse() {
    std::vector<ASTNode*> declarations;
    try {
        while (currentToken.type != TokenType::END_OF_FILE) {
            declarations.push_back(declaration());
        }
    } 
    catch (const std::exception& e) {
        ErrorHandling::reportError("Error while parsing: " + std::string(e.what()));
        // Synchronize to continue parsing despite errors
        synchronize();
    }
    
    return new ProgramNode(declarations);
}

// Primary expressions: literals, identifiers, grouped expressions
ExprNode* Parser::primary() {
    if (match(TokenType::NUMBER) || match(TokenType::STRING)) {
        return new LiteralExpr(currentToken);
    }
    
    if (match(TokenType::IDENTIFIER)) {
        return new VariableExpr(currentToken);
    }
    
    if (match(TokenType::LPAREN)) {
        ExprNode* expr = expression();
        consume(TokenType::RPAREN);
        return expr;
    }
    
    throw std::runtime_error("Expected expression");
}

// Call expressions and member access
ExprNode* Parser::finishCall(ExprNode* callee) {
    std::vector<ExprNode*> arguments;
    
    if (currentToken.type != TokenType::RPAREN) {
        do {
            arguments.push_back(expression());
        } while (match(TokenType::COMMA));
    }
    
    Token paren = currentToken;
    consume(TokenType::RPAREN);
    
    return new CallExpr(callee, paren, arguments);
}

ExprNode* Parser::call() {
    ExprNode* expr = primary();
    
    while (true) {
        if (match(TokenType::LPAREN)) {
            expr = finishCall(expr);
        } else {
            break;
        }
    }
    
    return expr;
}

// Unary expressions: !, -, etc.
ExprNode* Parser::unary() {
    // For now, we don't implement unary operators
    return call();
}

// Multiplication and division
ExprNode* Parser::factor() {
    ExprNode* expr = unary();
    
    while (match({TokenType::STAR, TokenType::SLASH})) {
        Token op = currentToken;
        advance();
        ExprNode* right = unary();
        expr = new BinaryExpr(expr, op, right);
    }
    
    return expr;
}

// Addition and subtraction
ExprNode* Parser::term() {
    ExprNode* expr = factor();
    
    while (match({TokenType::PLUS, TokenType::MINUS})) {
        Token op = currentToken;
        advance();
        ExprNode* right = factor();
        expr = new BinaryExpr(expr, op, right);
    }
    
    return expr;
}

// Comparison operators (>, <, >=, <=)
ExprNode* Parser::comparison() {
    // This would implement comparison operators, but for simplicity, we'll skip to the next level
    return term();
}

// Equality operators (==, !=)
ExprNode* Parser::equality() {
    // This would implement equality operators, but for simplicity, we'll skip to the next level
    return comparison();
}

// Assignment expressions
ExprNode* Parser::assignment() {
    ExprNode* expr = equality();
    
    if (match(TokenType::ASSIGN)) {
        Token equals = currentToken;
        ExprNode* value = assignment();
        
        if (VariableExpr* varExpr = dynamic_cast<VariableExpr*>(expr)) {
            Token name = varExpr->name;
            return new AssignExpr(name, value);
        }
        
        throw std::runtime_error("Invalid assignment target");
    }
    
    return expr;
}

// Main expression parser
ExprNode* Parser::expression() {
    return assignment();
}

// Statement parsers
StmtNode* Parser::expressionStatement() {
    ExprNode* expr = expression();
    consume(TokenType::SEMICOLON);
    return new ExpressionStmt(expr);
}

StmtNode* Parser::printStatement() {
    ExprNode* value = expression();
    consume(TokenType::SEMICOLON);
    return new PrintStmt(value);
}

StmtNode* Parser::block() {
    std::vector<StmtNode*> statements;
    
    while (currentToken.type != TokenType::RBRACE && currentToken.type != TokenType::END_OF_FILE) {
        statements.push_back(declaration());
    }
    
    consume(TokenType::RBRACE);
    return new BlockStmt(statements);
}

StmtNode* Parser::ifStatement() {
    consume(TokenType::LPAREN);
    ExprNode* condition = expression();
    consume(TokenType::RPAREN);
    
    StmtNode* thenBranch = statement();
    StmtNode* elseBranch = nullptr;
    
    if (match(TokenType::ELSE)) {
        elseBranch = statement();
    }
    
    return new IfStmt(condition, thenBranch, elseBranch);
}

StmtNode* Parser::whileStatement() {
    consume(TokenType::LPAREN);
    ExprNode* condition = expression();
    consume(TokenType::RPAREN);
    StmtNode* body = statement();
    
    return new WhileStmt(condition, body);
}

StmtNode* Parser::statement() {
    if (match(TokenType::IF)) {
        return ifStatement();
    }
    
    if (match(TokenType::PRINT)) {
        return printStatement();
    }
    
    if (match(TokenType::WHILE)) {
        return whileStatement();
    }
    
    if (match(TokenType::LBRACE)) {
        return block();
    }
    
    return expressionStatement();
}

// Declaration parsers
StmtNode* Parser::varDeclaration() {
    Token name = currentToken;
    consume(TokenType::IDENTIFIER);
    
    ExprNode* initializer = nullptr;
    if (match(TokenType::ASSIGN)) {
        initializer = expression();
    }
    
    consume(TokenType::SEMICOLON);
    return new VarStmt(name, initializer);
}

FunctionNode* Parser::parseFunction() {
    consume(TokenType::FN);
    
    Token name = currentToken;
    consume(TokenType::IDENTIFIER);
    
    consume(TokenType::LPAREN);
    std::vector<std::string> parameters;
    
    if (currentToken.type != TokenType::RPAREN) {
        do {
            Token param = currentToken;
            consume(TokenType::IDENTIFIER);
            parameters.push_back(param.lexeme);
        } while (match(TokenType::COMMA));
    }
    
    consume(TokenType::RPAREN);
    
    // Parse function body
    consume(TokenType::LBRACE);
    std::vector<StmtNode*> body;
    
    while (currentToken.type != TokenType::RBRACE && currentToken.type != TokenType::END_OF_FILE) {
        body.push_back(declaration());
    }
    
    consume(TokenType::RBRACE);
    
    return new FunctionNode(name.lexeme, parameters, body);
}

ClassNode* Parser::parseClass() {
    consume(TokenType::CLASS);
    
    Token name = currentToken;
    consume(TokenType::IDENTIFIER);
    
    // Parse class body
    consume(TokenType::LBRACE);
    std::vector<FunctionNode*> methods;
    
    while (currentToken.type != TokenType::RBRACE && currentToken.type != TokenType::END_OF_FILE) {
        if (currentToken.type == TokenType::FN) {
            methods.push_back(parseFunction());
        } else {
            // Skip non-method declarations in class
            advance();
        }
    }
    
    consume(TokenType::RBRACE);
    
    return new ClassNode(name.lexeme, methods);
}

ASTNode* Parser::declaration() {
    if (match(TokenType::CLASS)) {
        return parseClass();
    }
    
    if (match(TokenType::FN)) {
        return parseFunction();
    }
    
    if (match(TokenType::LET)) {
        return varDeclaration();
    }
    
    return statement();
}

// Error recovery
void Parser::synchronize() {
    advance();
    
    while (currentToken.type != TokenType::END_OF_FILE) {
        // Look for statement boundaries to resynchronize
        if (currentToken.type == TokenType::SEMICOLON) {
            advance();
            return;
        }
        
        switch (currentToken.type) {
            case TokenType::CLASS:
            case TokenType::FN:
            case TokenType::LET:
            case TokenType::IF:
            case TokenType::WHILE:
            case TokenType::PRINT:
                return;
            default:
                break;
        }
        
        advance();
    }
}
