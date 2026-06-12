#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include "json.hpp"
#include "../parser/Parser.h"
#include "../lexer/Lexer.h"

using json = nlohmann::json;

namespace trypillia {

class LSPServer {
public:
    LSPServer(const std::string& docsPath = "src/lsp/native_docs.json") : docsPath(docsPath) {}
    void run();

private:
    void handleMessage(const json& message);
    void handleInitialize(const json& message);
    void handleInitialized(const json& message);
    void handleDidOpen(const json& message);
    void handleDidChange(const json& message);
    void handleHover(const json& message);
    void handleDefinition(const json& message);
    void handleCompletion(const json& message);
    void handleSignatureHelp(const json& message);
    
    void publishDiagnostics(const std::string& uri, const std::string& text);
    
    void sendMessage(const json& message);
    std::string readMessage();

    bool isRunning = true;
    std::unordered_map<std::string, std::string> documents;
    json nativeDocs;
    std::string docsPath;
};

} // namespace trypillia
