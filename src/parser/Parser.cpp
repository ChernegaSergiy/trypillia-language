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
        std::string message = "Expected token type " + tokenTypeToString(type) + 
                            ", got " + tokenTypeToString(currentToken.type) + " at line " + std::to_string(currentToken.line) + ":" + std::to_string(currentToken.column);
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
    bool isArrow = false;
    if (currentToken.type == TokenType::IDENTIFIER) {
        Lexer tempLexer = lexer;
        Token next = tempLexer.nextToken();
        if (next.type == TokenType::ARROW) {
            isArrow = true;
        }
    } else if (currentToken.type == TokenType::LPAREN) {
        Lexer tempLexer = lexer;
        Token t = tempLexer.nextToken();
        bool validParams = true;
        if (t.type != TokenType::RPAREN) {
            while (true) {
                if (t.type != TokenType::IDENTIFIER) { validParams = false; break; }
                t = tempLexer.nextToken();
                if (t.type == TokenType::COMMA) {
                    t = tempLexer.nextToken();
                } else {
                    break;
                }
            }
        }
        if (validParams && t.type == TokenType::RPAREN) {
            Token afterParen = tempLexer.nextToken();
            if (afterParen.type == TokenType::ARROW) {
                isArrow = true;
            }
        }
    }

    if (isArrow) {
        std::vector<std::string> parameters;
        if (currentToken.type == TokenType::IDENTIFIER) {
            parameters.push_back(currentToken.lexeme);
            advance(); // consume ident
        } else {
            consume(TokenType::LPAREN);
            if (currentToken.type != TokenType::RPAREN) {
                do {
                    Token param = currentToken;
                    consume(TokenType::IDENTIFIER);
                    parameters.push_back(param.lexeme);
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RPAREN);
        }
        consume(TokenType::ARROW);
        
        std::vector<StmtNode*> body;
        if (currentToken.type == TokenType::LBRACE) {
            consume(TokenType::LBRACE);
            while (currentToken.type != TokenType::RBRACE && currentToken.type != TokenType::END_OF_FILE) {
                body.push_back(dynamic_cast<StmtNode*>(declaration()));
            }
            consume(TokenType::RBRACE);
        } else {
            ExprNode* expr = expression();
            Token retToken;
            retToken.type = TokenType::RETURN;
            retToken.lexeme = "return";
            retToken.line = currentToken.line;
            body.push_back(new ReturnStmt(retToken, expr));
        }
        return new LambdaExpr(parameters, body);
    }

    if (currentToken.type == TokenType::STRING) {
        Token literal = currentToken;
        advance();
        
        std::string str = literal.lexeme;
        // Check for interpolation, but respect escaped braces \{
        bool hasInterpolation = false;
        for (size_t i = 0; i < str.length(); i++) {
            if (str[i] == '{' && (i == 0 || str[i-1] != '\\')) {
                hasInterpolation = true;
                break;
            }
        }

        if (hasInterpolation) {
            std::vector<ExprNode*> parts;
            std::string currentPart = "";
            
            for (size_t i = 0; i < str.length(); i++) {
                if (str[i] == '\\' && i + 1 < str.length() && (str[i+1] == '{' || str[i+1] == '}')) {
                    currentPart += str[i+1];
                    i++;
                    continue;
                }
                
                if (str[i] == '{') {
                    if (!currentPart.empty()) {
                        Token t = literal; t.lexeme = currentPart;
                        parts.push_back(new LiteralExpr(t));
                        currentPart = "";
                    }
                    
                    size_t start = i + 1;
                    int nest = 1;
                    while (i + 1 < str.length() && nest > 0) {
                        i++;
                        if (str[i] == '{' && str[i-1] != '\\') nest++;
                        if (str[i] == '}' && str[i-1] != '\\') nest--;
                    }
                    
                    std::string exprStr = str.substr(start, i - start);
                    try {
                        Lexer lex(exprStr);
                        Parser p(lex);
                        ExprNode* e = p.expression();
                        if (e) parts.push_back(e);
                    } catch (...) {
                        Token t = literal; t.lexeme = "{" + exprStr + "}";
                        parts.push_back(new LiteralExpr(t));
                    }
                } else {
                    currentPart += str[i];
                }
            }
            
            if (!currentPart.empty()) {
                Token t = literal; t.lexeme = currentPart;
                parts.push_back(new LiteralExpr(t));
            }
            
            if (parts.empty()) return new LiteralExpr(literal);
            
            ExprNode* result = parts[0];
            for (size_t j = 1; j < parts.size(); j++) {
                Token plusToken; plusToken.type = TokenType::PLUS; plusToken.lexeme = "+"; plusToken.line = literal.line;
                result = new BinaryExpr(result, plusToken, parts[j]);
            }
            return result;
        }
        std::string cleanedStr = "";
        for (size_t i = 0; i < str.length(); i++) {
            if (str[i] == '\\' && i + 1 < str.length() && (str[i+1] == '{' || str[i+1] == '}')) {
                cleanedStr += str[i+1];
                i++;
            } else {
                cleanedStr += str[i];
            }
        }
        Token t = literal; t.lexeme = cleanedStr;
        return new LiteralExpr(t);
    }
    
    if (currentToken.type == TokenType::NUMBER) {
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
    
    if (currentToken.type == TokenType::SUPER) {
        Token keyword = currentToken;
        advance();
        consume(TokenType::DOT);
        Token method = currentToken;
        consume(TokenType::IDENTIFIER);
        return new SuperExpr(keyword, method);
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
    
    if (match(TokenType::LBRACE)) {
        std::vector<std::pair<ExprNode*, ExprNode*>> elements;
        
        if (currentToken.type != TokenType::RBRACE) {
            do {
                ExprNode* key = expression();
                consume(TokenType::COLON);
                ExprNode* value = expression();
                elements.push_back({key, value});
            } while (match(TokenType::COMMA));
        }
        
        consume(TokenType::RBRACE);
        return new DictExpr(elements);
    }
    
    if (match(TokenType::FN)) {
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
        
        consume(TokenType::LBRACE);
        std::vector<StmtNode*> body;
        while (currentToken.type != TokenType::RBRACE && currentToken.type != TokenType::END_OF_FILE) {
            body.push_back(dynamic_cast<StmtNode*>(declaration()));
        }
        consume(TokenType::RBRACE);
        
        return new LambdaExpr(parameters, body);
    }

    std::string errMsg = "Unexpected token in expression: " + currentToken.lexeme + " at line " + std::to_string(currentToken.line) + ":" + std::to_string(currentToken.column);
    throw std::runtime_error(errMsg);
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
        } else if (match(TokenType::COLON_COLON)) {
            if (!dynamic_cast<VariableExpr*>(expr)) {
                throw std::runtime_error("Static access requires a class name");
            }
            Token className = dynamic_cast<VariableExpr*>(expr)->name;
            Token member = currentToken;
            consume(TokenType::IDENTIFIER);
            if (match(TokenType::LPAREN)) {
                std::vector<ExprNode*> arguments;
                if (currentToken.type != TokenType::RPAREN) {
                    do {
                        arguments.push_back(expression());
                    } while (match(TokenType::COMMA));
                }
                Token paren = currentToken;
                consume(TokenType::RPAREN);
                return new StaticCallExpr(className, member, paren, arguments);
            } else {
                return new StaticGetExpr(className, member);
            }
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

// Unary operators (!, -, ~)
ExprNode* Parser::unary() {
    if (currentToken.type == TokenType::BANG ||
        currentToken.type == TokenType::MINUS ||
        currentToken.type == TokenType::PLUS_PLUS ||
        currentToken.type == TokenType::MINUS_MINUS ||
        currentToken.type == TokenType::BITWISE_NOT) {
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

// Comparison operators (<, <=, >, >=)
ExprNode* Parser::comparison() {
    ExprNode* expr = bitwiseOrExpr();

    while (currentToken.type == TokenType::LESS ||
           currentToken.type == TokenType::LESS_EQUAL ||
           currentToken.type == TokenType::GREATER ||
           currentToken.type == TokenType::GREATER_EQUAL) {
        Token op = currentToken;
        advance();
        ExprNode* right = bitwiseOrExpr();
        expr = new BinaryExpr(expr, op, right);
    }

    return expr;
}

ExprNode* Parser::bitwiseOrExpr() {
    ExprNode* expr = bitwiseXorExpr();

    while (currentToken.type == TokenType::BITWISE_OR) {
        Token op = currentToken;
        advance();
        ExprNode* right = bitwiseXorExpr();
        expr = new BinaryExpr(expr, op, right);
    }

    return expr;
}

ExprNode* Parser::bitwiseXorExpr() {
    ExprNode* expr = bitwiseAndExpr();

    while (currentToken.type == TokenType::BITWISE_XOR) {
        Token op = currentToken;
        advance();
        ExprNode* right = bitwiseAndExpr();
        expr = new BinaryExpr(expr, op, right);
    }

    return expr;
}

ExprNode* Parser::bitwiseAndExpr() {
    ExprNode* expr = shiftExpr();

    while (currentToken.type == TokenType::BITWISE_AND) {
        Token op = currentToken;
        advance();
        ExprNode* right = shiftExpr();
        expr = new BinaryExpr(expr, op, right);
    }

    return expr;
}

ExprNode* Parser::shiftExpr() {
    ExprNode* expr = term();

    while (currentToken.type == TokenType::SHIFT_LEFT ||
           currentToken.type == TokenType::SHIFT_RIGHT) {
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
        
        if (StaticGetExpr* staticGetExpr = dynamic_cast<StaticGetExpr*>(expr)) {
            return new StaticSetExpr(staticGetExpr->className, staticGetExpr->memberName, value);
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

StmtNode* Parser::loadStatement() {
    Token filename = currentToken;
    consume(TokenType::STRING);
    consume(TokenType::SEMICOLON);
    return new LoadStmt(filename);
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

StmtNode* Parser::doWhileStatement() {
    StmtNode* body = statement();
    consume(TokenType::WHILE);
    consume(TokenType::LPAREN);
    ExprNode* condition = expression();
    consume(TokenType::RPAREN);
    consume(TokenType::SEMICOLON);
    
    return new DoWhileStmt(condition, body);
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

StmtNode* Parser::switchStatement() {
    consume(TokenType::LPAREN);
    ExprNode* expr = expression();
    consume(TokenType::RPAREN);
    consume(TokenType::LBRACE);
    
    std::vector<SwitchStmt::Case> cases;
    
    while (currentToken.type != TokenType::RBRACE && currentToken.type != TokenType::END_OF_FILE) {
        if (match(TokenType::CASE)) {
            ExprNode* value = expression();
            consume(TokenType::COLON);
            
            std::vector<StmtNode*> body;
            while (currentToken.type != TokenType::CASE && 
                   currentToken.type != TokenType::DEFAULT && 
                   currentToken.type != TokenType::RBRACE && 
                   currentToken.type != TokenType::END_OF_FILE) {
                body.push_back(statement());
            }
            
            cases.push_back({value, body});
        } else if (match(TokenType::DEFAULT)) {
            consume(TokenType::COLON);
            
            std::vector<StmtNode*> body;
            while (currentToken.type != TokenType::CASE && 
                   currentToken.type != TokenType::RBRACE && 
                   currentToken.type != TokenType::END_OF_FILE) {
                body.push_back(statement());
            }
            
            cases.push_back({nullptr, body});
        } else {
            throw std::runtime_error("Expected 'case' or 'default' in switch statement");
        }
    }
    
    consume(TokenType::RBRACE);
    return new SwitchStmt(expr, cases);
}

StmtNode* Parser::usingStatement() {
    consume(TokenType::LPAREN);
    
    StmtNode* declaration = nullptr;
    if (currentToken.type == TokenType::LET || currentToken.type == TokenType::CONST) {
        bool isConst = (currentToken.type == TokenType::CONST);
        consume(isConst ? TokenType::CONST : TokenType::LET);
        if (isConst) {
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
        declaration = new VarStmt(name, initializer, isConst);
    } else {
        ExprNode* expr = expression();
        declaration = new ExpressionStmt(expr);
    }
    
    consume(TokenType::RPAREN);
    StmtNode* body = statement();
    
    return new UsingStmt(declaration, body);
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
    
    if (match(TokenType::LOAD)) {
        return loadStatement();
    }
    
    if (match(TokenType::WHILE)) {
        return whileStatement();
    }
    
    if (match(TokenType::DO)) {
        return doWhileStatement();
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
    
    if (match(TokenType::SWITCH)) {
        return switchStatement();
    }
    
    if (match(TokenType::LBRACE)) {
        return block();
    }
    
    if (currentToken.type == TokenType::USING) {
        advance();
        return usingStatement();
    }
    
    return expressionStatement();
}

// Declaration parsers
StmtNode* Parser::varDeclaration() {
    bool isConst = (currentToken.type == TokenType::CONST);
    consume(isConst ? TokenType::CONST : TokenType::LET);
    if (isConst) {
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
    bool isConst = (currentToken.type == TokenType::CONST);
    if (isConst) {
        advance();
    }
    
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
    } else if (isConst) {
        throw std::runtime_error("Constant field requires an initializer");
    }
    
    consume(TokenType::SEMICOLON);
    return new FieldDeclNode(name.lexeme, initializer, accessModifier, isConst);
}

FunctionNode* Parser::parseFunction(AccessModifier accessModifier, bool isAbstract, bool isStatic) {
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
    
    if (isAbstract) {
        consume(TokenType::SEMICOLON);
        FunctionNode* node = new FunctionNode(name.lexeme, parameters, {});
        node->accessModifier = accessModifier;
        node->isAbstract = true;
        node->isStatic = isStatic;
        return node;
    }
    
    // Parse function body
    consume(TokenType::LBRACE);
    std::vector<StmtNode*> body;
    
    while (currentToken.type != TokenType::RBRACE && currentToken.type != TokenType::END_OF_FILE) {
        body.push_back(dynamic_cast<StmtNode*>(declaration()));
    }
    
    consume(TokenType::RBRACE);
    
    FunctionNode* node = new FunctionNode(name.lexeme, parameters, body);
    node->accessModifier = accessModifier;
    node->isStatic = isStatic;
    return node;
}

ClassNode* Parser::parseClass() {
    consume(TokenType::CLASS);
    
    Token name = currentToken;
    consume(TokenType::IDENTIFIER);
    
    // Parse optional parent class (< ParentName)
    std::string parentName = "";
    if (currentToken.type == TokenType::LESS) {
        advance();
        Token parent = currentToken;
        consume(TokenType::IDENTIFIER);
        parentName = parent.lexeme;
    }
    
    // Parse optional implements clause
    std::vector<std::string> interfaceNames;
    if (currentToken.type == TokenType::IMPLEMENTS) {
        advance();
        do {
            Token iface = currentToken;
            consume(TokenType::IDENTIFIER);
            interfaceNames.push_back(iface.lexeme);
        } while (match(TokenType::COMMA));
    }
    
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
        } else if (currentToken.type == TokenType::PROTECTED) {
            advance();
            currentAccess = AccessModifier::PROTECTED;
        } else if (currentToken.type == TokenType::ABSTRACT) {
            advance();
            methods.push_back(parseFunction(currentAccess, true));
        } else if (currentToken.type == TokenType::FN) {
            methods.push_back(parseFunction(currentAccess));
        } else if (currentToken.type == TokenType::DESTROY) {
            advance();
            consume(TokenType::LBRACE);
            std::vector<StmtNode*> body;
            while (currentToken.type != TokenType::RBRACE && currentToken.type != TokenType::END_OF_FILE) {
                body.push_back(dynamic_cast<StmtNode*>(declaration()));
            }
            consume(TokenType::RBRACE);
            auto node = new FunctionNode("destroy", {}, body);
            node->accessModifier = currentAccess;
            methods.push_back(node);
        } else if (currentToken.type == TokenType::STATIC) {
            advance();
            if (currentToken.type == TokenType::FN) {
                methods.push_back(parseFunction(currentAccess, false, true));
            } else if (currentToken.type == TokenType::LET || currentToken.type == TokenType::IDENTIFIER || currentToken.type == TokenType::CONST) {
                auto field = parseFieldDecl(currentAccess);
                field->isStatic = true;
                fields.push_back(field);
            } else {
                throw std::runtime_error("Expected 'fn' or field declaration after 'static'");
            }
        } else if (currentToken.type == TokenType::LET || currentToken.type == TokenType::IDENTIFIER || currentToken.type == TokenType::CONST) {
            fields.push_back(parseFieldDecl(currentAccess));
        } else {
            advance();
        }
    }
    
    consume(TokenType::RBRACE);
    
    auto node = new ClassNode(name.lexeme, parentName, methods, fields);
    node->interfaceNames = interfaceNames;
    return node;
}

InterfaceNode* Parser::parseInterface() {
    Token name = currentToken;
    consume(TokenType::IDENTIFIER);
    
    // Parse optional parent interfaces (< Parent1, Parent2)
    std::vector<std::string> parentNames;
    if (currentToken.type == TokenType::LESS) {
        advance();
        do {
            Token parent = currentToken;
            consume(TokenType::IDENTIFIER);
            parentNames.push_back(parent.lexeme);
        } while (match(TokenType::COMMA));
    }
    
    consume(TokenType::LBRACE);
    std::vector<FunctionNode*> methods;
    
    while (currentToken.type != TokenType::RBRACE && currentToken.type != TokenType::END_OF_FILE) {
        if (currentToken.type == TokenType::FN) {
            advance();
            
            Token methodName = currentToken;
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
            consume(TokenType::SEMICOLON);
            
            auto method = new FunctionNode(methodName.lexeme, parameters, {});
            method->isAbstract = true;
            methods.push_back(method);
        } else {
            advance();
        }
    }
    
    consume(TokenType::RBRACE);
    return new InterfaceNode(name.lexeme, methods, parentNames);
}

TraitNode* Parser::parseTrait() {
    Token name = currentToken;
    consume(TokenType::IDENTIFIER);
    
    std::vector<std::string> parentNames;
    if (currentToken.type == TokenType::LESS) {
        advance();
        do {
            Token parent = currentToken;
            consume(TokenType::IDENTIFIER);
            parentNames.push_back(parent.lexeme);
        } while (match(TokenType::COMMA));
    }
    
    consume(TokenType::LBRACE);
    std::vector<FunctionNode*> methods;
    
    while (currentToken.type != TokenType::RBRACE && currentToken.type != TokenType::END_OF_FILE) {
        if (currentToken.type == TokenType::FN) {
            advance();
            
            Token methodName = currentToken;
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
            
            if (currentToken.type == TokenType::SEMICOLON) {
                // Abstract method
                advance();
                auto method = new FunctionNode(methodName.lexeme, parameters, {});
                method->isAbstract = true;
                methods.push_back(method);
            } else {
                // Method with body
                consume(TokenType::LBRACE);
                std::vector<StmtNode*> body;
                while (currentToken.type != TokenType::RBRACE && currentToken.type != TokenType::END_OF_FILE) {
                    body.push_back(dynamic_cast<StmtNode*>(declaration()));
                }
                consume(TokenType::RBRACE);
                auto method = new FunctionNode(methodName.lexeme, parameters, body);
                methods.push_back(method);
            }
        } else {
            advance();
        }
    }
    
    consume(TokenType::RBRACE);
    return new TraitNode(name.lexeme, methods, parentNames);
}

ASTNode* Parser::declaration() {
    if (currentToken.type == TokenType::NAMESPACE) {
        advance();
        return parseNamespaceDeclaration();
    }
    if (currentToken.type == TokenType::USE) {
        advance();
        return parseUseStatement();
    }
    if (currentToken.type == TokenType::ABSTRACT) {
        advance();
        if (currentToken.type == TokenType::CLASS) {
            auto node = parseClass();
            node->isAbstract = true;
            return node;
        }
        throw std::runtime_error("Expected 'class' after 'abstract'");
    }
    
    if (currentToken.type == TokenType::INTERFACE) {
        advance();
        return parseInterface();
    }
    
    if (currentToken.type == TokenType::TRAIT) {
        advance();
        return parseTrait();
    }
    
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
            case TokenType::LOAD:
                return;
            default:
                break;
        }
        
        advance();
    }
}

StmtNode* Parser::parseNamespaceDeclaration() {
    Token nameToken = currentToken;
    std::string ns = "";
    do {
        consume(TokenType::IDENTIFIER);
        ns += nameToken.lexeme;
        if (match(TokenType::DOT)) {
            ns += ".";
            nameToken = currentToken;
        } else {
            break;
        }
    } while (true);
    
    consume(TokenType::SEMICOLON);
    this->currentNamespace = ns;
    
    Token nsToken = nameToken;
    nsToken.lexeme = ns;
    return new NamespaceStmt(nsToken);
}

StmtNode* Parser::parseUseStatement() {
    Token nameToken = currentToken;
    std::string fqn = "";
    std::string lastId = "";
    do {
        consume(TokenType::IDENTIFIER);
        fqn += nameToken.lexeme;
        lastId = nameToken.lexeme;
        if (match(TokenType::DOT)) {
            fqn += ".";
            nameToken = currentToken;
        } else {
            break;
        }
    } while (true);
    
    consume(TokenType::SEMICOLON);
    
    this->useAliases[lastId] = fqn;
    
    Token fqnToken = nameToken;
    fqnToken.lexeme = fqn;
    Token aliasToken = nameToken;
    aliasToken.lexeme = lastId;
    return new UseStmt(fqnToken, aliasToken);
}
