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
    
    if (currentToken.type == TokenType::THIS) {
        Token keyword = currentToken;
        advance();
        return new ThisExpr(keyword);
    }
    
    if (match(TokenType::LPAREN)) {
        ExprNode* expr = expression();
        consume(TokenType::RPAREN);
        return expr;
    }
    
    if (match(TokenType::LBRACKET)) {
        std::vector<ExprNode*> elements;
        
        if (currentToken.type != TokenType::RBRACKET) {
            do {
                elements.push_back(expression());
            } while (match(TokenType::COMMA));
        }
        
        consume(TokenType::RBRACKET);
        return new ListExpr(elements);
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
        } else if (match(TokenType::DOT)) {
            Token name = currentToken;
            consume(TokenType::IDENTIFIER);
            expr = new GetExpr(expr, name);
        } else if (match(TokenType::LBRACKET)) {
            ExprNode* index = expression();
            consume(TokenType::RBRACKET);
            expr = new IndexGetExpr(expr, index);
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
        currentToken.type == TokenType::MINUS ||
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

// Ternary conditional (?:)
ExprNode* Parser::ternary() {
    ExprNode* expr = orExpr();

    if (currentToken.type == TokenType::QUESTION) {
        advance();
        ExprNode* thenBranch = expression();
        consume(TokenType::COLON);
        ExprNode* elseBranch = ternary();
        expr = new TernaryExpr(expr, thenBranch, elseBranch);
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
    ExprNode* expr = ternary();
    
    if (currentToken.type == TokenType::ASSIGN) {
        Token equals = currentToken;
        advance();
        ExprNode* value = assignment();
        
        if (VariableExpr* varExpr = dynamic_cast<VariableExpr*>(expr)) {
            Token name = varExpr->name;
            return new AssignExpr(name, value);
        }
        
        if (GetExpr* getExpr = dynamic_cast<GetExpr*>(expr)) {
            return new SetExpr(getExpr->object, getExpr->name, value);
        }
        
        if (IndexGetExpr* indexGetExpr = dynamic_cast<IndexGetExpr*>(expr)) {
            return new IndexSetExpr(indexGetExpr->object, indexGetExpr->index, value);
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

StmtNode* Parser::returnStatement() {
    Token keyword = currentToken;
    advance();

    if (currentToken.type == TokenType::SEMICOLON) {
        advance();
        return new ReturnStmt(keyword, nullptr);
    }

    ExprNode* value = expression();
    consume(TokenType::SEMICOLON);
    return new ReturnStmt(keyword, value);
}

StmtNode* Parser::breakStatement() {
    Token keyword = currentToken;
    advance();
    consume(TokenType::SEMICOLON);
    return new BreakStmt(keyword);
}

StmtNode* Parser::continueStatement() {
    Token keyword = currentToken;
    advance();
    consume(TokenType::SEMICOLON);
    return new ContinueStmt(keyword);
}

StmtNode* Parser::forStatement() {
    advance(); // consume FOR
    
    consume(TokenType::LPAREN);
    
    // Check for foreach: for (let x in iterable)
    if (match(TokenType::LET)) {
        Token name = currentToken;
        consume(TokenType::IDENTIFIER);
        
        if (match(TokenType::IN)) {
            ExprNode* iterable = expression();
            consume(TokenType::RPAREN);
            StmtNode* body = statement();
            return new ForeachStmt(name, iterable, body);
        }
        
        // Regular for with var initializer
        ExprNode* initExpr = nullptr;
        if (match(TokenType::ASSIGN)) {
            initExpr = expression();
        }
        
        StmtNode* initializer = new VarStmt(name, initExpr);
        consume(TokenType::SEMICOLON);
        return finishForLoop(initializer);
    }
    
    // Regular for without var initializer
    StmtNode* initializer = nullptr;
    if (currentToken.type == TokenType::SEMICOLON) {
        advance(); // no initializer
    } else {
        ExprNode* expr = expression();
        initializer = new ExpressionStmt(expr);
    }
    consume(TokenType::SEMICOLON);
    return finishForLoop(initializer);
}

StmtNode* Parser::finishForLoop(StmtNode* initializer) {
    // Condition (optional, default true)
    ExprNode* condition = nullptr;
    if (currentToken.type != TokenType::SEMICOLON) {
        condition = expression();
    }
    consume(TokenType::SEMICOLON);
    
    // Increment (optional)
    ExprNode* increment = nullptr;
    if (currentToken.type != TokenType::RPAREN) {
        increment = expression();
    }
    consume(TokenType::RPAREN);
    
    StmtNode* body = statement();
    
    return new ForStmt(initializer, condition, increment, body);
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
    
    if (currentToken.type == TokenType::RETURN) {
        return returnStatement();
    }
    
    if (currentToken.type == TokenType::BREAK) {
        return breakStatement();
    }
    
    if (currentToken.type == TokenType::CONTINUE) {
        return continueStatement();
    }
    
    if (currentToken.type == TokenType::FOR) {
        return forStatement();
    }
    
    if (match(TokenType::LBRACE)) {
        return block();
    }
    
    return expressionStatement();
}

// Declaration parsers
StmtNode* Parser::varDeclaration() {
    bool isConst = (currentToken.type == TokenType::CONST);
    if (isConst) {
        advance();
        consume(TokenType::LET);
    }
    
    Token name = currentToken;
    consume(TokenType::IDENTIFIER);
    
    ExprNode* initializer = nullptr;
    if (match(TokenType::ASSIGN)) {
        initializer = expression();
    } else if (isConst) {
        throw std::runtime_error("Constant declaration requires an initializer");
    }
    
    consume(TokenType::SEMICOLON);
    return new VarStmt(name, initializer, isConst);
}

FieldDeclNode* Parser::parseFieldDecl(AccessModifier accessModifier) {
    // Optionally consume 'let'
    if (currentToken.type == TokenType::LET) {
        advance();
    }
    
    Token name = currentToken;
    consume(TokenType::IDENTIFIER);
    
    ExprNode* initializer = nullptr;
    if (currentToken.type == TokenType::ASSIGN) {
        advance();
        initializer = expression();
    }
    
    consume(TokenType::SEMICOLON);
    return new FieldDeclNode(name.lexeme, initializer, accessModifier);
}

FunctionNode* Parser::parseFunction(AccessModifier accessModifier) {
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
    
    FunctionNode* node = new FunctionNode(name.lexeme, parameters, body);
    node->accessModifier = accessModifier;
    return node;
}

ClassNode* Parser::parseClass() {
    consume(TokenType::CLASS);
    
    Token name = currentToken;
    consume(TokenType::IDENTIFIER);
    
    // Parse class body
    consume(TokenType::LBRACE);
    std::vector<FunctionNode*> methods;
    std::vector<FieldDeclNode*> fields;
    
    AccessModifier currentAccess = AccessModifier::PUBLIC;
    
    while (currentToken.type != TokenType::RBRACE && currentToken.type != TokenType::END_OF_FILE) {
        if (currentToken.type == TokenType::PUBLIC) {
            advance();
            currentAccess = AccessModifier::PUBLIC;
        } else if (currentToken.type == TokenType::PRIVATE) {
            advance();
            currentAccess = AccessModifier::PRIVATE;
        } else if (currentToken.type == TokenType::FN) {
            methods.push_back(parseFunction(currentAccess));
        } else if (currentToken.type == TokenType::LET || currentToken.type == TokenType::IDENTIFIER) {
            fields.push_back(parseFieldDecl(currentAccess));
        } else {
            advance();
        }
    }
    
    consume(TokenType::RBRACE);
    
    return new ClassNode(name.lexeme, methods, fields);
}

ASTNode* Parser::declaration() {
    if (currentToken.type == TokenType::CLASS) {
        return parseClass();
    }
    
    if (currentToken.type == TokenType::FN) {
        return parseFunction();
    }
    
    if (currentToken.type == TokenType::LET || currentToken.type == TokenType::CONST) {
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
