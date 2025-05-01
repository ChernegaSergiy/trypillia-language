#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>

enum class TokenType {
    // Single-character tokens
    LPAREN, RPAREN, LBRACE, RBRACE, COMMA, DOT, SEMICOLON,
    
    // One or two character tokens
    PLUS, MINUS, STAR, SLASH, 
    ASSIGN, EQUAL_EQUAL,
    BANG, BANG_EQUAL,
    LESS, LESS_EQUAL,
    GREATER, GREATER_EQUAL,
    
    // Literals
    IDENTIFIER, NUMBER, STRING,
    
    // Keywords
    CLASS, FN, LET, VIRTUAL, OVERRIDE, PRINT, IF, ELSE, WHILE,
    
    // Special
    END_OF_FILE, UNKNOWN
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
};

class Lexer {
public:
    Lexer(const std::string &source);
    Token nextToken();

private:
    std::string source;
    size_t currentIndex;
    int line;

    char advance();
    bool match(char expected);
    bool isAtEnd();
    char peek();
    char peekNext();
    void skipWhitespace();
    
    Token identifier();
    Token number();
    Token string();
    Token scanToken();
};

#endif // LEXER_H
