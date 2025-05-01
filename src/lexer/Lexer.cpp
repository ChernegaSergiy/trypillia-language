#include "Lexer.h"
#include <cctype>
#include <stdexcept>
#include <unordered_map>

// Define keywords
static std::unordered_map<std::string, TokenType> keywords = {
    {"class", TokenType::CLASS},
    {"fn", TokenType::FN},
    {"let", TokenType::LET},
    {"virtual", TokenType::VIRTUAL},
    {"override", TokenType::OVERRIDE},
    {"print", TokenType::PRINT},
    {"if", TokenType::IF},
    {"else", TokenType::ELSE},
    {"while", TokenType::WHILE}
};

Lexer::Lexer(const std::string &source) : source(source), currentIndex(0), line(1) {}

Token Lexer::nextToken() {
    skipWhitespace();
    
    if (isAtEnd()) {
        return {TokenType::END_OF_FILE, "", line};
    }
    
    char c = advance();
    
    if (std::isalpha(c) || c == '_') {
        return identifier();
    }
    
    if (std::isdigit(c)) {
        return number();
    }
    
    switch (c) {
        case '(': return {TokenType::LPAREN, "(", line};
        case ')': return {TokenType::RPAREN, ")", line};
        case '{': return {TokenType::LBRACE, "{", line};
        case '}': return {TokenType::RBRACE, "}", line};
        case ',': return {TokenType::COMMA, ",", line};
        case '.': return {TokenType::DOT, ".", line};
        case ';': return {TokenType::SEMICOLON, ";", line};
        case '+': return {TokenType::PLUS, "+", line};
        case '-': return {TokenType::MINUS, "-", line};
        case '*': return {TokenType::STAR, "*", line};
        case '/': 
            if (match('/')) {
                // Comment extends to the end of the line
                while (peek() != '\n' && !isAtEnd()) {
                    advance();
                }
                return nextToken();
            } else {
                return {TokenType::SLASH, "/", line};
            }
        case '=': 
            if (match('=')) {
                return {TokenType::EQUAL_EQUAL, "==", line};
            } else {
                return {TokenType::ASSIGN, "=", line};
            }
        case '!': 
            if (match('=')) {
                return {TokenType::BANG_EQUAL, "!=", line};
            } else {
                return {TokenType::BANG, "!", line};
            }
        case '<': 
            if (match('=')) {
                return {TokenType::LESS_EQUAL, "<=", line};
            } else {
                return {TokenType::LESS, "<", line};
            }
        case '>': 
            if (match('=')) {
                return {TokenType::GREATER_EQUAL, ">=", line};
            } else {
                return {TokenType::GREATER, ">", line};
            }
        case '"': return string();
    }
    
    return {TokenType::UNKNOWN, std::string(1, c), line};
}

void Lexer::skipWhitespace() {
    while (true) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                line++;
                advance();
                break;
            default:
                return;
        }
    }
}

char Lexer::advance() {
    return source[currentIndex++];
}

bool Lexer::match(char expected) {
    if (isAtEnd() || source[currentIndex] != expected) {
        return false;
    }
    
    currentIndex++;
    return true;
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
    
    while (std::isalnum(peek()) || peek() == '_') {
        advance();
    }
    
    std::string text = source.substr(start, currentIndex - start);
    
    auto it = keywords.find(text);
    if (it != keywords.end()) {
        return {it->second, text, line};
    }
    
    return {TokenType::IDENTIFIER, text, line};
}

Token Lexer::number() {
    size_t start = currentIndex - 1;
    
    while (std::isdigit(peek())) {
        advance();
    }
    
    // Look for a decimal part
    if (peek() == '.' && std::isdigit(peekNext())) {
        // Consume the '.'
        advance();
        
        while (std::isdigit(peek())) {
            advance();
        }
    }
    
    std::string text = source.substr(start, currentIndex - start);
    return {TokenType::NUMBER, text, line};
}

Token Lexer::string() {
    size_t start = currentIndex;
    
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n') {
            line++;
        }
        advance();
    }
    
    if (isAtEnd()) {
        return {TokenType::UNKNOWN, "", line}; // Unterminated string
    }
    
    // Consume the closing '"'
    advance();
    
    std::string text = source.substr(start, currentIndex - start - 1);
    return {TokenType::STRING, text, line};
}

Token Lexer::scanToken() {
    skipWhitespace();
    
    if (isAtEnd()) {
        return {TokenType::END_OF_FILE, "", line};
    }
    
    char c = advance();
    
    if (std::isalpha(c)) {
        return identifier();
    }
    
    if (std::isdigit(c)) {
        return number();
    }
    
    // Other token types would be handled here
    
    return {TokenType::UNKNOWN, std::string(1, c), line};
}
