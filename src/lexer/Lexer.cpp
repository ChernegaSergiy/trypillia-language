#include "Lexer.h"
#include <cctype>
#include <stdexcept>

Lexer::Lexer(const std::string &source) : source(source), currentIndex(0), line(1) {}

Token Lexer::nextToken() {
    while (!isAtEnd()) {
        char c = advance();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                continue;
            case '\n':
                line++;
                continue;
            case '+': return {TokenType::PLUS, "+", line};
            case '-': return {TokenType::MINUS, "-", line};
            case '*': return {TokenType::STAR, "*", line};
            case '/': return {TokenType::SLASH, "/", line};
            case '(': return {TokenType::LPAREN, "(", line};
            case ')': return {TokenType::RPAREN, ")", line};
            case '=': return {TokenType::ASSIGN, "=", line};
            default:
                if (std::isalpha(c)) {
                    return identifier();
                } else if (std::isdigit(c)) {
                    return number();
                } else if (c == '"') {
                    return string();
                }
                return {TokenType::UNKNOWN, std::string(1, c), line};
        }
    }
    return {TokenType::END_OF_FILE, "", line};
}

char Lexer::advance() {
    return source[currentIndex++];
}

bool Lexer::isAtEnd() {
    return currentIndex >= source.length();
}

char Lexer::peek() {
    if (isAtEnd()) return '\0';
    return source[currentIndex];
}

char Lexer::peekNext() {
    if (currentIndex + 1 >= source.length()) return '\0';
    return source[currentIndex + 1];
}

Token Lexer::identifier() {
    size_t start = currentIndex - 1;
    while (std::isalnum(peek())) advance();
    std::string text = source.substr(start, currentIndex - start);
    return {TokenType::IDENTIFIER, text, line};
}

Token Lexer::number() {
    size_t start = currentIndex - 1;
    while (std::isdigit(peek())) advance();
    std::string text = source.substr(start, currentIndex - start);
    return {TokenType::NUMBER, text, line};
}

Token Lexer::string() {
    size_t start = currentIndex;
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n') line++;
        advance();
    }
    if (isAtEnd()) return {TokenType::UNKNOWN, "", line}; // Unterminated string
    advance(); // Consume the closing '"'
    std::string text = source.substr(start, currentIndex - start - 1);
    return {TokenType::STRING, text, line};
}
