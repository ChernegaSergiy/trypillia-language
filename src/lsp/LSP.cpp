#include "LSP.h"
#include <sstream>
#include "../utils/ErrorHandling.h"

namespace trypillia {

void LSPServer::run() {
    while (isRunning) {
        std::string message = readMessage();
        if (message.empty()) {
            continue;
        }

        try {
            json parsedMessage = json::parse(message);
            handleMessage(parsedMessage);
        } catch (const json::parse_error& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
        }
    }
}

std::string LSPServer::readMessage() {
    std::string line;
    int contentLength = 0;

    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) {
            break; // Empty line separates headers from body
        }

        if (line.find("Content-Length: ") == 0) {
            contentLength = std::stoi(line.substr(16));
        }
    }

    if (contentLength > 0) {
        std::string content(contentLength, ' ');
        std::cin.read(&content[0], contentLength);
        return content;
    }

    return "";
}

void LSPServer::sendMessage(const json& message) {
    std::string content = message.dump();
    std::cout << "Content-Length: " << content.length() << "\r\n\r\n" << content << std::flush;
}

void LSPServer::handleMessage(const json& message) {
    if (message.contains("method")) {
        std::string method = message["method"];

        if (method == "initialize") {
            handleInitialize(message);
        } else if (method == "initialized") {
            handleInitialized(message);
        } else if (method == "textDocument/didOpen") {
            handleDidOpen(message);
        } else if (method == "textDocument/didChange") {
            handleDidChange(message);
        } else if (method == "textDocument/hover") {
            handleHover(message);
        } else if (method == "textDocument/definition") {
            handleDefinition(message);
        } else if (method == "textDocument/completion") {
            handleCompletion(message);
        } else if (method == "textDocument/signatureHelp") {
            handleSignatureHelp(message);
        } else if (method == "shutdown") {
            isRunning = false;
            json response = {
                {"jsonrpc", "2.0"},
                {"id", message["id"]},
                {"result", nullptr}
            };
            sendMessage(response);
        } else if (method == "exit") {
            isRunning = false;
        }
    }
}

void LSPServer::handleInitialize(const json& message) {
    json response = {
        {"jsonrpc", "2.0"},
        {"id", message["id"]},
        {"result", {
            {"capabilities", {
                {"textDocumentSync", 1}, // 1 = Full sync
                {"hoverProvider", true},
                {"definitionProvider", true},
                {"completionProvider", {
                    {"resolveProvider", false},
                    {"triggerCharacters", {"."}}
                }},
                {"signatureHelpProvider", {
                    {"triggerCharacters", {"(", ","}}
                }}
            }}
        }}
    };
    sendMessage(response);
}

void LSPServer::handleInitialized(const json& message) {
    // Client is fully initialized
}

void LSPServer::handleDidOpen(const json& message) {
    std::string uri = message["params"]["textDocument"]["uri"];
    std::string text = message["params"]["textDocument"]["text"];
    documents[uri] = text;
    publishDiagnostics(uri, text);
}

void LSPServer::handleDidChange(const json& message) {
    std::string uri = message["params"]["textDocument"]["uri"];
    std::string text = message["params"]["contentChanges"][0]["text"];
    documents[uri] = text;
    publishDiagnostics(uri, text);
}

void LSPServer::handleHover(const json& message) {
    std::string uri = message["params"]["textDocument"]["uri"];
    int line = message["params"]["position"]["line"];
    int character = message["params"]["position"]["character"];
    
    // We get the line from 0-indexed, but lexer uses 1-indexed
    int targetLine = line + 1;
    
    json result = nullptr;
    
    if (documents.find(uri) != documents.end()) {
        std::string text = documents[uri];
        Lexer lexer(text);
        
        Token targetToken = {TokenType::UNKNOWN, "", 0, 0};
        
        while (true) {
            Token t = lexer.nextToken();
            if (t.type == TokenType::END_OF_FILE) break;
            
            // Check if token covers the given position
            // t.column is 1-indexed. character is 0-indexed
            int tokenStartCol = t.column - 1;
            int tokenEndCol = tokenStartCol + t.lexeme.length();
            
            if (t.line == targetLine && character >= tokenStartCol && character <= tokenEndCol) {
                targetToken = t;
                break;
            }
        }
        
        if (targetToken.type != TokenType::UNKNOWN && targetToken.type != TokenType::END_OF_FILE) {
            std::string hoverText = "";
            
            // Simple type determination for hover
            if (targetToken.type == TokenType::IDENTIFIER) {
                hoverText = "**Identifier**: `" + targetToken.lexeme + "`";
            } else if (targetToken.type == TokenType::NUMBER) {
                hoverText = "**Number**: `" + targetToken.lexeme + "`";
            } else if (targetToken.type == TokenType::STRING) {
                hoverText = "**String Literal**";
            } else {
                hoverText = "**Keyword/Operator**: `" + targetToken.lexeme + "`";
            }
            
            result = {
                {"contents", {
                    {"kind", "markdown"},
                    {"value", hoverText}
                }}
            };
        }
    }
    
    json response = {
        {"jsonrpc", "2.0"},
        {"id", message["id"]},
        {"result", result}
    };
    
    sendMessage(response);
}

void LSPServer::handleDefinition(const json& message) {
    std::string uri = message["params"]["textDocument"]["uri"];
    int line = message["params"]["position"]["line"];
    int character = message["params"]["position"]["character"];
    
    int targetLine = line + 1;
    json result = nullptr;
    
    if (documents.find(uri) != documents.end()) {
        std::string text = documents[uri];
        Lexer lexer(text);
        
        Token targetToken = {TokenType::UNKNOWN, "", 0, 0};
        std::vector<Token> allTokens;
        
        while (true) {
            Token t = lexer.nextToken();
            if (t.type == TokenType::END_OF_FILE) break;
            allTokens.push_back(t);
            
            int tokenStartCol = t.column - 1;
            int tokenEndCol = tokenStartCol + t.lexeme.length();
            
            if (t.line == targetLine && character >= tokenStartCol && character <= tokenEndCol) {
                targetToken = t;
            }
        }
        
        if (targetToken.type == TokenType::IDENTIFIER) {
            // Find where this identifier was defined (let, const, fn, class, etc.)
            for (size_t i = 0; i < allTokens.size() - 1; ++i) {
                if ((allTokens[i].type == TokenType::LET ||
                     allTokens[i].type == TokenType::CONST ||
                     allTokens[i].type == TokenType::FN ||
                     allTokens[i].type == TokenType::CLASS ||
                     allTokens[i].type == TokenType::INTERFACE ||
                     allTokens[i].type == TokenType::TRAIT) && 
                    allTokens[i+1].type == TokenType::IDENTIFIER &&
                    allTokens[i+1].lexeme == targetToken.lexeme) {
                    
                    // Found definition!
                    Token defToken = allTokens[i+1];
                    result = {
                        {"uri", uri},
                        {"range", {
                            {"start", {{"line", defToken.line - 1}, {"character", defToken.column - 1}}},
                            {"end", {{"line", defToken.line - 1}, {"character", defToken.column - 1 + defToken.lexeme.length()}}}
                        }}
                    };
                    break;
                }
            }
        }
    }
    
    json response = {
        {"jsonrpc", "2.0"},
        {"id", message["id"]},
        {"result", result}
    };
    
    sendMessage(response);
}

void LSPServer::handleCompletion(const json& message) {
    // Basic completion items: keywords
    std::vector<std::string> keywords = {
        "if", "else", "while", "do", "for", "in", "return", "break", "continue", 
        "switch", "case", "default", "class", "fn", "let", "const", "abstract", 
        "interface", "trait", "implements", "public", "private", "protected", 
        "static", "virtual", "override", "load", "destroy", "using", "true", "false", "nil",
        "this", "super", "print"
    };
    
    json items = json::array();
    
    bool isMethodCompletion = false;
    if (message.contains("params") && message["params"].contains("context")) {
        if (message["params"]["context"]["triggerCharacter"] == ".") {
            isMethodCompletion = true;
        }
    }
    
    if (isMethodCompletion) {
        // Suggest common object/array/string methods
        std::vector<std::string> methods = {
            "length", "push", "pop", "shift", "unshift", "map", "filter", 
            "reduce", "toString", "split", "join", "keys", "values", "forEach"
        };
        for (const auto& m : methods) {
            items.push_back({
                {"label", m},
                {"kind", 2}, // Method
                {"detail", "Method"}
            });
        }
    } else {
        // Add keywords
        for (const auto& kw : keywords) {
            items.push_back({
                {"label", kw},
                {"kind", 14}, // Keyword
                {"detail", "Keyword"}
            });
        }
    
        // Add local variables and functions from the document
        if (message.contains("params") && message["params"].contains("textDocument")) {
            std::string uri = message["params"]["textDocument"]["uri"];
            if (documents.find(uri) != documents.end()) {
                std::string text = documents[uri];
                Lexer lexer(text);
                
                Token prevToken = {TokenType::UNKNOWN, "", 0, 0};
                std::vector<std::string> localIdentifiers;
                
                while (true) {
                    Token t = lexer.nextToken();
                    if (t.type == TokenType::END_OF_FILE) break;
                    
                    if (t.type == TokenType::IDENTIFIER) {
                        if (prevToken.type == TokenType::LET ||
                            prevToken.type == TokenType::CONST ||
                            prevToken.type == TokenType::FN ||
                            prevToken.type == TokenType::CLASS) {
                            
                            // Avoid duplicates
                            bool exists = false;
                            for (const auto& id : localIdentifiers) {
                                if (id == t.lexeme) { exists = true; break; }
                            }
                            
                            if (!exists) {
                                localIdentifiers.push_back(t.lexeme);
                                
                                int kind = 6; // Variable by default
                                std::string detail = "Variable";
                                if (prevToken.type == TokenType::FN) { kind = 3; detail = "Function"; } // Function
                                else if (prevToken.type == TokenType::CLASS) { kind = 7; detail = "Class"; } // Class
                                else if (prevToken.type == TokenType::CONST) { kind = 21; detail = "Constant"; } // Constant
                                
                                items.push_back({
                                    {"label", t.lexeme},
                                    {"kind", kind},
                                    {"detail", detail}
                                });
                            }
                        }
                    }
                    prevToken = t;
                }
            }
        }
    }
    
    json response = {
        {"jsonrpc", "2.0"},
        {"id", message["id"]},
        {"result", items}
    };
    
    sendMessage(response);
}

void LSPServer::handleSignatureHelp(const json& message) {
    std::string uri = message["params"]["textDocument"]["uri"];
    int line = message["params"]["position"]["line"];
    int character = message["params"]["position"]["character"];
    
    json result = nullptr;
    
    if (documents.find(uri) != documents.end()) {
        std::string text = documents[uri];
        
        // Extract line text up to the cursor
        std::string lineText = "";
        int currentLine = 0;
        size_t lineStart = 0;
        
        for (size_t i = 0; i < text.length(); ++i) {
            if (currentLine == line) {
                lineStart = i;
                break;
            }
            if (text[i] == '\n') currentLine++;
        }
        
        if (currentLine == line) {
            size_t len = character;
            if (lineStart + len > text.length()) len = text.length() - lineStart;
            lineText = text.substr(lineStart, len);
            
            // Find the last '(' and count commas after it
            int activeParameter = 0;
            int parenPos = -1;
            
            for (int i = lineText.length() - 1; i >= 0; --i) {
                if (lineText[i] == ')') {
                    break; // Ended, not in signature
                }
                if (lineText[i] == ',') {
                    activeParameter++;
                } else if (lineText[i] == '(') {
                    parenPos = i;
                    break;
                }
            }
            
            if (parenPos != -1) {
                // Find function name before '('
                std::string funcName = "";
                for (int i = parenPos - 1; i >= 0; --i) {
                    if (std::isspace(lineText[i])) {
                        if (!funcName.empty()) break;
                    } else if (std::isalnum(lineText[i]) || lineText[i] == '_') {
                        funcName = lineText[i] + funcName;
                    } else {
                        break;
                    }
                }
                
                if (!funcName.empty()) {
                    // Search document for 'fn funcName(param1, param2)'
                    Lexer lexer(text);
                    std::vector<std::string> params;
                    bool found = false;
                    
                    while (true) {
                        Token t = lexer.nextToken();
                        if (t.type == TokenType::END_OF_FILE) break;
                        
                        if (t.type == TokenType::FN) {
                            Token nameToken = lexer.nextToken();
                            if (nameToken.type == TokenType::IDENTIFIER && nameToken.lexeme == funcName) {
                                Token paren = lexer.nextToken();
                                if (paren.type == TokenType::LPAREN) {
                                    // Parse params
                                    while (true) {
                                        Token p = lexer.nextToken();
                                        if (p.type == TokenType::RPAREN || p.type == TokenType::END_OF_FILE) break;
                                        if (p.type == TokenType::IDENTIFIER) {
                                            params.push_back(p.lexeme);
                                        }
                                    }
                                    found = true;
                                    break;
                                }
                            }
                        }
                    }
                    
                    // Also support builtin 'print'
                    if (funcName == "print") {
                        params.push_back("message");
                        found = true;
                    }
                    
                    if (found) {
                        std::string label = funcName + "(";
                        json parameters = json::array();
                        
                        for (size_t i = 0; i < params.size(); ++i) {
                            parameters.push_back({
                                {"label", params[i]}
                            });
                            label += params[i];
                            if (i < params.size() - 1) label += ", ";
                        }
                        label += ")";
                        
                        result = {
                            {"signatures", {
                                {
                                    {"label", label},
                                    {"parameters", parameters}
                                }
                            }},
                            {"activeSignature", 0},
                            {"activeParameter", activeParameter}
                        };
                    }
                }
            }
        }
    }
    
    json response = {
        {"jsonrpc", "2.0"},
        {"id", message["id"]},
        {"result", result}
    };
    
    sendMessage(response);
}

void LSPServer::publishDiagnostics(const std::string& uri, const std::string& text) {
    json diagnostics = json::array();

    try {
        Lexer lexer(text);
        Parser parser(lexer);
        
        ErrorHandling::clearErrors();
        
        try {
            ASTNode* ast = parser.parse();
            delete ast;
        } catch (...) {
            // Exceptions might still be thrown
        }
        
        // Gather errors from ErrorHandling
        for (const auto& errMsg : ErrorHandling::getErrors()) {
            int line = 0;
            int col = 0;
            // Try to extract line and column number from "at line X" or "at line X:Y"
            size_t linePos = errMsg.find("at line ");
            if (linePos != std::string::npos) {
                size_t colonPos = errMsg.find(":", linePos + 8);
                if (colonPos != std::string::npos) {
                    line = std::stoi(errMsg.substr(linePos + 8, colonPos - (linePos + 8))) - 1;
                    size_t endPos = errMsg.find_first_not_of("0123456789", colonPos + 1);
                    col = std::stoi(errMsg.substr(colonPos + 1, endPos - (colonPos + 1))) - 1;
                } else {
                    line = std::stoi(errMsg.substr(linePos + 8)) - 1;
                }
                
                if (line < 0) line = 0;
                if (col < 0) col = 0;
            }
            
            json diagnostic;
            diagnostic["range"]["start"]["line"] = line;
            diagnostic["range"]["start"]["character"] = col;
            diagnostic["range"]["end"]["line"] = line;
            diagnostic["range"]["end"]["character"] = col > 0 ? col + 1 : 100;
            diagnostic["severity"] = 1;
            diagnostic["source"] = "trypillia";
            diagnostic["message"] = errMsg;
            
            diagnostics.push_back(diagnostic);
        }
    } catch (...) {
        // Catch any unhandled exceptions to prevent server crash
    }

    json notification = {
        {"jsonrpc", "2.0"},
        {"method", "textDocument/publishDiagnostics"},
        {"params", {
            {"uri", uri},
            {"diagnostics", diagnostics}
        }}
    };

    sendMessage(notification);
}

} // namespace trypillia
