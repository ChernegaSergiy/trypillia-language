#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>

enum class TokenType {
    IDENTIFIER,
    NUMBER,
    STRING,
    PLUS,
    MINUS,
    STAR,
    SLASH,
    LPAREN,
    RPAREN,
    ASSIGN,
    CLASS,
    FN,
    LET,
    VIRTUAL,
    OVERRIDE,
    PRINT,
    IF,
    ELSE,
    WHILE,
    END_OF_FILE,
    UNKNOWN
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
    bool isAtEnd();
    char peek();
    char peekNext();
    Token identifier();
    Token number();
    Token string();
    Token scanToken();
};

#endif // LEXER_H
