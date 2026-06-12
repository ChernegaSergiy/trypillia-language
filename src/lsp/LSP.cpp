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
                {"hoverProvider", false},
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
            // Try to extract line number from "at line X"
            size_t linePos = errMsg.find("at line ");
            if (linePos != std::string::npos) {
                line = std::stoi(errMsg.substr(linePos + 8)) - 1;
                if (line < 0) line = 0;
            }
            
            json diagnostic;
            diagnostic["range"]["start"]["line"] = line;
            diagnostic["range"]["start"]["character"] = 0;
            diagnostic["range"]["end"]["line"] = line;
            diagnostic["range"]["end"]["character"] = 100;
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
