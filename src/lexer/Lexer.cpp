#include "Lexer.h"
#include <cctype>
#include <stdexcept>
#include <unordered_map>

// Define keywords
static std::unordered_map<std::string, TokenType> keywords = {
    {"class", TokenType::CLASS},
    {"fn", TokenType::FN},
    {"let", TokenType::LET},
    {"const", TokenType::CONST},
    {"virtual", TokenType::VIRTUAL},
    {"override", TokenType::OVERRIDE},
    {"public", TokenType::PUBLIC},
    {"private", TokenType::PRIVATE},
    {"protected", TokenType::PROTECTED},
    {"if", TokenType::IF},
    {"else", TokenType::ELSE},
    {"while", TokenType::WHILE},
    {"for", TokenType::FOR},
    {"in", TokenType::IN},
    {"return", TokenType::RETURN},
    {"break", TokenType::BREAK},
    {"continue", TokenType::CONTINUE},
    {"do", TokenType::DO},
    {"abstract", TokenType::ABSTRACT},
    {"interface", TokenType::INTERFACE},
    {"implements", TokenType::IMPLEMENTS},
    {"trait", TokenType::TRAIT},
    {"load", TokenType::LOAD},
    {"destroy", TokenType::DESTROY},
    {"using", TokenType::USING},
    {"switch", TokenType::SWITCH},
    {"case", TokenType::CASE},
    {"default", TokenType::DEFAULT},
    {"true", TokenType::TRUE},
    {"false", TokenType::FALSE},
    {"nil", TokenType::NIL},
    {"this", TokenType::THIS},
    {"super", TokenType::SUPER},
    {"static", TokenType::STATIC}
};

Lexer::Lexer(const std::string &source) : source(source), currentIndex(0), line(1), column(1) {}

Token Lexer::nextToken() {
    skipWhitespace();
    
    int startColumn = column;
    
    if (isAtEnd()) {
        return {TokenType::END_OF_FILE, "", line, column};
    }
    
    char c = advance();
    
    if (std::isalpha(c) || c == '_') {
        return identifier(startColumn);
    }
    
    if (std::isdigit(c)) {
        return number(startColumn);
    }
    
    switch (c) {
        case '(': return {TokenType::LPAREN, "(", line, startColumn};
        case ')': return {TokenType::RPAREN, ")", line, startColumn};
        case '{': return {TokenType::LBRACE, "{", line, startColumn};
        case '}': return {TokenType::RBRACE, "}", line, startColumn};
        case '[': return {TokenType::LBRACKET, "[", line, startColumn};
        case ']': return {TokenType::RBRACKET, "]", line, startColumn};
        case ',': return {TokenType::COMMA, ",", line, startColumn};
        case '.': return {TokenType::DOT, ".", line, startColumn};
        case ';': return {TokenType::SEMICOLON, ";", line, startColumn};
        case '+':
            if (match('+')) {
                return {TokenType::PLUS_PLUS, "++", line, startColumn};
            } else if (match('=')) {
                return {TokenType::PLUS_EQUAL, "+=", line, startColumn};
            } else {
                return {TokenType::PLUS, "+", line, startColumn};
            }
        case '-':
            if (match('-')) {
                return {TokenType::MINUS_MINUS, "--", line, startColumn};
            } else if (match('=')) {
                return {TokenType::MINUS_EQUAL, "-=", line, startColumn};
            } else {
                return {TokenType::MINUS, "-", line, startColumn};
            }
        case '*':
            if (match('=')) {
                return {TokenType::STAR_EQUAL, "*=", line, startColumn};
            } else {
                return {TokenType::STAR, "*", line, startColumn};
            }
        case '/': 
            if (match('=')) {
                return {TokenType::SLASH_EQUAL, "/=", line, startColumn};
            } else if (match('/')) {
                // Comment extends to the end of the line
                while (peek() != '\n' && !isAtEnd()) {
                    advance();
                }
                return nextToken();
            } else {
                return {TokenType::SLASH, "/", line, startColumn};
            }
        case '%': return {TokenType::PERCENT, "%", line, startColumn};
        case '=': 
            if (match('=')) {
                return {TokenType::EQUAL_EQUAL, "==", line, startColumn};
            } else if (match('>')) {
                return {TokenType::ARROW, "=>", line, startColumn};
            } else {
                return {TokenType::ASSIGN, "=", line, startColumn};
            }
        case '!': 
            if (match('=')) {
                return {TokenType::BANG_EQUAL, "!=", line, startColumn};
            } else {
                return {TokenType::BANG, "!", line, startColumn};
            }
        case '<': 
            if (match('=')) {
                return {TokenType::LESS_EQUAL, "<=", line, startColumn};
            } else if (match('<')) {
                return {TokenType::SHIFT_LEFT, "<<", line, startColumn};
            } else {
                return {TokenType::LESS, "<", line, startColumn};
            }
        case '>': 
            if (match('=')) {
                return {TokenType::GREATER_EQUAL, ">=", line, startColumn};
            } else if (match('>')) {
                return {TokenType::SHIFT_RIGHT, ">>", line, startColumn};
            } else {
                return {TokenType::GREATER, ">", line, startColumn};
            }
        case '"': return string(startColumn);
        case '?': return {TokenType::QUESTION, "?", line, startColumn};
        case ':':
            if (match(':')) {
                return {TokenType::COLON_COLON, "::", line, startColumn};
            }
            return {TokenType::COLON, ":", line, startColumn};
        case '&':
            if (match('&')) {
                return {TokenType::AND, "&&", line, startColumn};
            } else {
                return {TokenType::BITWISE_AND, "&", line, startColumn};
            }
        case '|':
            if (match('|')) {
                return {TokenType::OR, "||", line, startColumn};
            } else {
                return {TokenType::BITWISE_OR, "|", line, startColumn};
            }
        case '^': return {TokenType::BITWISE_XOR, "^", line, startColumn};
        case '~': return {TokenType::BITWISE_NOT, "~", line, startColumn};
    }
    
    return {TokenType::UNKNOWN, std::string(1, c), line, startColumn};
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
                column = 1;
                currentIndex++;
                break;
            default:
                return;
        }
    }
}

char Lexer::advance() {
    char c = source[currentIndex++];
    column++;
    return c;
}

bool Lexer::match(char expected) {
    if (isAtEnd() || source[currentIndex] != expected) {
        return false;
    }
    
    currentIndex++;
    column++;
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

Token Lexer::identifier(int startColumn) {
    size_t start = currentIndex - 1;
    
    while (std::isalnum(peek()) || peek() == '_') {
        advance();
    }
    
    std::string text = source.substr(start, currentIndex - start);
    
    auto it = keywords.find(text);
    if (it != keywords.end()) {
        return {it->second, text, line, startColumn};
    }
    
    return {TokenType::IDENTIFIER, text, line, startColumn};
}

Token Lexer::number(int startColumn) {
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
    return {TokenType::NUMBER, text, line, startColumn};
}

Token Lexer::string(int startColumn) {
    std::string value = "";
    
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n') {
            line++;
            column = 1;
        }
        char c = peek();
        advance();
        
        if (c == '\\' && !isAtEnd()) {
            char escape = peek();
            advance();
            switch (escape) {
                case 'n': value += '\n'; break;
                case 'r': value += '\r'; break;
                case 't': value += '\t'; break;
                case '\\': value += '\\'; break;
                case '"': value += '"'; break;
                case 'e': value += '\x1b'; break; // \e for Escape is useful
                case 'x': {
                    if (currentIndex + 1 < source.length()) {
                        std::string hexStr = source.substr(currentIndex, 2);
                        bool isHex = true;
                        for (char h : hexStr) {
                            if (!std::isxdigit(h)) isHex = false;
                        }
                        if (isHex) {
                            char hexChar = static_cast<char>(std::stoi(hexStr, nullptr, 16));
                            value += hexChar;
                            advance();
                            advance();
                            break;
                        }
                    }
                    value += "\\x";
                    break;
                }
                default:
                    value += '\\';
                    value += escape;
                    break;
            }
        } else {
            value += c;
        }
    }
    
    if (isAtEnd()) {
        
        return {TokenType::UNKNOWN, "", line, startColumn};
    }
    
    // Consume the closing "
    advance();
    
    return {TokenType::STRING, value, line, startColumn};
}

Token Lexer::scanToken() {
    skipWhitespace();
    
    int startColumn = column;
    
    if (isAtEnd()) {
        return {TokenType::END_OF_FILE, "", line, column};
    }
    
    char c = advance();
    
    if (std::isalpha(c)) {
        return identifier(startColumn);
    }
    
    if (std::isdigit(c)) {
        return number(startColumn);
    }
    
    // Other token types would be handled here
    
    return {TokenType::UNKNOWN, std::string(1, c), line};
}
