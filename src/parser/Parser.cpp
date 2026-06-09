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
    if (currentToken.type == TokenType::NUMBER || currentToken.type == TokenType::STRING) {
        Token literal = currentToken;
        advance();
        return new LiteralExpr(literal);
    }
    
    if (currentToken.type == TokenType::TRUE || currentToken.type == TokenType::FALSE || currentToken.type == TokenType::NIL) {
        Token literal = currentToken;
        advance();
        return new LiteralExpr(literal);
    }
    
    if (currentToken.type == TokenType::IDENTIFIER) {
        Token name = currentToken;
        advance();
        return new VariableExpr(name);
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
        } else if (currentToken.type == TokenType::PLUS_PLUS ||
                   currentToken.type == TokenType::MINUS_MINUS) {
            Token op = currentToken;
            advance();
            if (VariableExpr* varExpr = dynamic_cast<VariableExpr*>(expr)) {
                expr = new PostfixExpr(varExpr->name, op);
            } else {
                throw std::runtime_error("Invalid postfix expression target");
            }
        } else {
            break;
        }
    }
    
    return expr;
}

// Unary expressions: !, -, ++, -- etc.
ExprNode* Parser::unary() {
    if (currentToken.type == TokenType::BANG ||
        currentToken.type == TokenType::PLUS_PLUS ||
        currentToken.type == TokenType::MINUS_MINUS) {
        Token op = currentToken;
        advance();
        ExprNode* right = unary();
        return new UnaryExpr(op, right);
    }

    return call();
}

// Multiplication and division
ExprNode* Parser::factor() {
    ExprNode* expr = unary();
    
    while (currentToken.type == TokenType::STAR || currentToken.type == TokenType::SLASH || currentToken.type == TokenType::PERCENT) {
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
    
    while (currentToken.type == TokenType::PLUS || currentToken.type == TokenType::MINUS) {
        Token op = currentToken;
        advance();
        ExprNode* right = factor();
        expr = new BinaryExpr(expr, op, right);
    }
    
    return expr;
}

// Comparison operators (>, <, >=, <=)
ExprNode* Parser::comparison() {
    ExprNode* expr = term();

    while (currentToken.type == TokenType::GREATER ||
           currentToken.type == TokenType::GREATER_EQUAL ||
           currentToken.type == TokenType::LESS ||
           currentToken.type == TokenType::LESS_EQUAL) {
        Token op = currentToken;
        advance();
        ExprNode* right = term();
        expr = new BinaryExpr(expr, op, right);
    }

    return expr;
}

// Logical OR
ExprNode* Parser::orExpr() {
    ExprNode* expr = andExpr();

    while (currentToken.type == TokenType::OR) {
        Token op = currentToken;
        advance();
        ExprNode* right = andExpr();
        expr = new BinaryExpr(expr, op, right);
    }

    return expr;
}

// Logical AND
ExprNode* Parser::andExpr() {
    ExprNode* expr = equality();

    while (currentToken.type == TokenType::AND) {
        Token op = currentToken;
        advance();
        ExprNode* right = equality();
        expr = new BinaryExpr(expr, op, right);
    }

    return expr;
}

// Equality operators (==, !=)
ExprNode* Parser::equality() {
    ExprNode* expr = comparison();

    while (currentToken.type == TokenType::BANG_EQUAL ||
           currentToken.type == TokenType::EQUAL_EQUAL) {
        Token op = currentToken;
        advance();
        ExprNode* right = comparison();
        expr = new BinaryExpr(expr, op, right);
    }

    return expr;
}

// Assignment expressions
ExprNode* Parser::assignment() {
    ExprNode* expr = orExpr();
    
    if (currentToken.type == TokenType::ASSIGN) {
        Token equals = currentToken;
        advance();
        ExprNode* value = assignment();
        
        if (VariableExpr* varExpr = dynamic_cast<VariableExpr*>(expr)) {
            Token name = varExpr->name;
            return new AssignExpr(name, value);
        }
        
        throw std::runtime_error("Invalid assignment target");
    }

    if (currentToken.type == TokenType::PLUS_EQUAL ||
        currentToken.type == TokenType::MINUS_EQUAL ||
        currentToken.type == TokenType::STAR_EQUAL ||
        currentToken.type == TokenType::SLASH_EQUAL) {
        Token op = currentToken;
        advance();
        ExprNode* value = assignment();

        if (VariableExpr* varExpr = dynamic_cast<VariableExpr*>(expr)) {
            Token name = varExpr->name;
            return new CompoundAssignExpr(name, op, value);
        }

        throw std::runtime_error("Invalid compound assignment target");
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
        statements.push_back(dynamic_cast<StmtNode*>(declaration()));
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
        body.push_back(dynamic_cast<StmtNode*>(declaration()));
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
    if (currentToken.type == TokenType::CLASS) {
        return parseClass();
    }
    
    if (currentToken.type == TokenType::FN) {
        return parseFunction();
    }
    
    if (currentToken.type == TokenType::LET) {
        advance();
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
