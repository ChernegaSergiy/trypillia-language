#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>

enum class TokenType {
    // Single-character tokens
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET, COMMA, DOT, SEMICOLON, QUESTION, COLON,
    
    // One or two character tokens
    PLUS, MINUS, STAR, SLASH, PERCENT,
    PLUS_PLUS, MINUS_MINUS,
    PLUS_EQUAL, MINUS_EQUAL, STAR_EQUAL, SLASH_EQUAL,
    ASSIGN, EQUAL_EQUAL,
    BANG, BANG_EQUAL,
    LESS, LESS_EQUAL,
    GREATER, GREATER_EQUAL,
    COLON_COLON,
    
    // Logical operators
    AND, OR,
    
    // Literals
    IDENTIFIER, NUMBER, STRING,
    
    // Keywords
    CLASS, FN, LET, CONST, VIRTUAL, OVERRIDE, PUBLIC, PRIVATE, PROTECTED, STATIC, PRINT, IF, ELSE, WHILE, DO, FOR, IN, RETURN, BREAK, CONTINUE, SWITCH, CASE, DEFAULT, ABSTRACT, INTERFACE, TRAIT, IMPLEMENTS, TRUE, FALSE, NIL, THIS, SUPER,
    
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
