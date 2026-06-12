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
                {"definitionProvider", false}
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
