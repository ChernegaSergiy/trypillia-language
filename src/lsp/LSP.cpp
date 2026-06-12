#include "LSP.h"
#include "../utils/ErrorHandling.h"
#include <fstream>
#include <sstream>

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
        } catch (const json::parse_error &e) {
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

void LSPServer::sendMessage(const json &message) {
    std::string content = message.dump(-1, ' ', false, json::error_handler_t::replace);

    std::cout << "Content-Length: " << content.length() << "\r\n\r\n" << content;
    std::cout.flush();
}

void LSPServer::handleMessage(const json &message) {
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
        } else if (method == "textDocument/semanticTokens/full") {
            handleSemanticTokens(message);
        } else if (method == "shutdown") {
            isRunning = false;
            json response = {{"jsonrpc", "2.0"}, {"id", message["id"]}, {"result", nullptr}};
            sendMessage(response);
        } else if (method == "exit") {
            isRunning = false;
        }
    }
}

void LSPServer::handleInitialize(const json &message) {
    json response = {{"jsonrpc", "2.0"},
                     {"id", message["id"]},
                     {"result",
                      {{"capabilities",
                        {{"textDocumentSync", 1}, // 1 = Full sync
                         {"hoverProvider", true},
                         {"definitionProvider", true},
                         {"completionProvider", {{"resolveProvider", false}, {"triggerCharacters", {"."}}}},
                         {"signatureHelpProvider", {{"triggerCharacters", {"(", ","}}}},
                         {"semanticTokensProvider",
                          {{"legend",
                            {{"tokenTypes",
                              {"type", "class", "parameter", "variable", "property", "function", "method", "string",
                               "keyword", "number", "operator"}},
                             {"tokenModifiers", {"declaration", "defaultLibrary"}}}},
                           {"full", true}}}}}}}};
    sendMessage(response);
}

void LSPServer::handleInitialized(const json &message) {
    // Client is fully initialized
}

void LSPServer::handleDidOpen(const json &message) {
    std::string uri = message["params"]["textDocument"]["uri"];
    std::string text = message["params"]["textDocument"]["text"];
    documents[uri] = text;
    publishDiagnostics(uri, text);
}

void LSPServer::handleDidChange(const json &message) {
    std::string uri = message["params"]["textDocument"]["uri"];
    std::string text = message["params"]["contentChanges"][0]["text"];
    documents[uri] = text;
    publishDiagnostics(uri, text);
}

void LSPServer::handleHover(const json &message) {
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
            if (t.type == TokenType::END_OF_FILE)
                break;

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

            result = {{"contents", {{"kind", "markdown"}, {"value", hoverText}}}};
        }
    }

    json response = {{"jsonrpc", "2.0"}, {"id", message["id"]}, {"result", result}};

    sendMessage(response);
}

void LSPServer::handleDefinition(const json &message) {
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
            if (t.type == TokenType::END_OF_FILE)
                break;
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
                if ((allTokens[i].type == TokenType::LET || allTokens[i].type == TokenType::CONST ||
                     allTokens[i].type == TokenType::FN || allTokens[i].type == TokenType::CLASS ||
                     allTokens[i].type == TokenType::INTERFACE || allTokens[i].type == TokenType::TRAIT) &&
                    allTokens[i + 1].type == TokenType::IDENTIFIER && allTokens[i + 1].lexeme == targetToken.lexeme) {

                    // Found definition!
                    Token defToken = allTokens[i + 1];
                    result = {{"uri", uri},
                              {"range",
                               {{"start", {{"line", defToken.line - 1}, {"character", defToken.column - 1}}},
                                {"end",
                                 {{"line", defToken.line - 1},
                                  {"character", defToken.column - 1 + defToken.lexeme.length()}}}}}};
                    break;
                }
            }
        }
    }

    json response = {{"jsonrpc", "2.0"}, {"id", message["id"]}, {"result", result}};

    sendMessage(response);
}

void LSPServer::handleCompletion(const json &message) {
    // Basic completion items: keywords
    std::vector<std::string> keywords = {
        "if",      "else",       "while",   "do",      "for",       "in",     "return",  "break",    "continue",
        "switch",  "case",       "default", "class",   "fn",        "let",    "const",   "abstract", "interface",
        "trait",   "implements", "public",  "private", "protected", "static", "virtual", "override", "load",
        "destroy", "using",      "true",    "false",   "nil",       "this",   "super",   "print"};

    json items = json::array();

    bool isMethodCompletion = false;
    if (message.contains("params") && message["params"].contains("context")) {
        if (message["params"]["context"]["triggerCharacter"] == ".") {
            isMethodCompletion = true;
        }
    }

    if (isMethodCompletion) {
        // Suggest common object/array/string methods
        std::vector<std::string> methods = {"length", "push",     "pop",   "shift", "unshift", "map",    "filter",
                                            "reduce", "toString", "split", "join",  "keys",    "values", "forEach"};
        for (const auto &m : methods) {
            items.push_back({{"label", m},
                             {"kind", 2}, // Method
                             {"detail", "Method"}});
        }
    } else {
        // Add keywords
        for (const auto &kw : keywords) {
            items.push_back({{"label", kw},
                             {"kind", 14}, // Keyword
                             {"detail", "Keyword"}});
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
                    if (t.type == TokenType::END_OF_FILE)
                        break;

                    if (t.type == TokenType::IDENTIFIER) {
                        if (prevToken.type == TokenType::LET || prevToken.type == TokenType::CONST ||
                            prevToken.type == TokenType::FN || prevToken.type == TokenType::CLASS) {

                            // Avoid duplicates
                            bool exists = false;
                            for (const auto &id : localIdentifiers) {
                                if (id == t.lexeme) {
                                    exists = true;
                                    break;
                                }
                            }

                            if (!exists) {
                                localIdentifiers.push_back(t.lexeme);

                                int kind = 6; // Variable by default
                                std::string detail = "Variable";
                                if (prevToken.type == TokenType::FN) {
                                    kind = 3;
                                    detail = "Function";
                                } // Function
                                else if (prevToken.type == TokenType::CLASS) {
                                    kind = 7;
                                    detail = "Class";
                                } // Class
                                else if (prevToken.type == TokenType::CONST) {
                                    kind = 21;
                                    detail = "Constant";
                                } // Constant

                                items.push_back({{"label", t.lexeme}, {"kind", kind}, {"detail", detail}});
                            }
                        }
                    }
                    prevToken = t;
                }
            }
        }
    }

    json response = {{"jsonrpc", "2.0"}, {"id", message["id"]}, {"result", items}};

    sendMessage(response);
}

void LSPServer::handleSignatureHelp(const json &message) {
    std::string uri = message["params"]["textDocument"]["uri"];
    int line = message["params"]["position"]["line"];
    int character = message["params"]["position"]["character"];

    json result = nullptr;

    // Load native_docs.json if not already loaded
    if (nativeDocs.is_null()) {
        std::ifstream f(docsPath);
        if (f.is_open()) {
            f >> nativeDocs;
        } else {
            nativeDocs = json::object();
        }
    }

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
            if (text[i] == '\n')
                currentLine++;
        }

        if (currentLine == line) {
            size_t len = character;
            if (lineStart + len > text.length())
                len = text.length() - lineStart;
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
                        if (!funcName.empty())
                            break;
                    } else if (std::isalnum(lineText[i]) || lineText[i] == '_' || lineText[i] == '.') {
                        funcName = lineText[i] + funcName;
                    } else {
                        break;
                    }
                }

                if (!funcName.empty()) {
                    bool found = false;
                    std::string label = "";
                    std::string documentation = "";
                    json parameters = json::array();

                    // Check builtins first
                    std::string matchedKey = "";
                    if (nativeDocs.contains(funcName)) {
                        matchedKey = funcName;
                    } else {
                        size_t dotPos = funcName.find_last_of('.');
                        std::string shortName = (dotPos != std::string::npos) ? funcName.substr(dotPos + 1) : funcName;
                        std::string suffix = "." + shortName;
                        for (auto& el : nativeDocs.items()) {
                            std::string k = el.key();
                            if (k == shortName || (k.length() >= suffix.length() && k.compare(k.length() - suffix.length(), suffix.length(), suffix) == 0)) {
                                matchedKey = k;
                                break;
                            }
                        }
                    }

                    if (!matchedKey.empty()) {
                        auto info = nativeDocs[matchedKey];
                        label = info["signature"].get<std::string>();
                        documentation = info["doc"].get<std::string>();

                        if (info.contains("params")) {
                            for (const auto &p : info["params"]) {
                                parameters.push_back(
                                    {{"label", p["label"].get<std::string>()},
                                     {"documentation",
                                      {{"kind", "markdown"}, {"value", p["doc"].get<std::string>()}}}});
                            }
                        }
                        found = true;
                    } else {
                        // Search document for 'fn funcName(param1, param2)'
                        Lexer lexer(text);
                        std::vector<std::string> params;
                        int defLine = -1;

                        while (true) {
                            Token t = lexer.nextToken();
                            if (t.type == TokenType::END_OF_FILE)
                                break;

                            if (t.type == TokenType::FN) {
                                Token nameToken = lexer.nextToken();
                                if (nameToken.type == TokenType::IDENTIFIER && nameToken.lexeme == funcName) {
                                    defLine = nameToken.line - 1; // 0-indexed
                                    Token paren = lexer.nextToken();
                                    if (paren.type == TokenType::LPAREN) {
                                        // Parse params
                                        while (true) {
                                            Token p = lexer.nextToken();
                                            if (p.type == TokenType::RPAREN || p.type == TokenType::END_OF_FILE)
                                                break;
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

                        if (found) {
                            // Extract documentation from comments above the function
                            if (defLine > 0) {
                                std::string docStr = "";
                                std::istringstream iss(text);
                                std::string l;
                                std::vector<std::string> lines;
                                while (std::getline(iss, l))
                                    lines.push_back(l);

                                for (int i = defLine - 1; i >= 0; --i) {
                                    std::string cl = lines[i];
                                    // Trim leading whitespace
                                    size_t first = cl.find_first_not_of(" \t\r");
                                    if (first != std::string::npos && cl.substr(first, 2) == "//") {
                                        std::string commentLine = cl.substr(first + 2);
                                        // Trim leading space in comment
                                        if (!commentLine.empty() && commentLine[0] == ' ')
                                            commentLine = commentLine.substr(1);
                                        docStr = commentLine + "\n" + docStr;
                                    } else if (first == std::string::npos) {
                                        continue; // Empty line, keep looking
                                    } else {
                                        break; // Code line, stop looking
                                    }
                                }
                                if (!docStr.empty())
                                    documentation = docStr;
                            }

                            label = "fn " + funcName + "(";
                            for (size_t i = 0; i < params.size(); ++i) {
                                parameters.push_back(
                                    {{"label", params[i]},
                                     {"documentation",
                                      {{"kind", "markdown"}, {"value", "Аргумент `" + params[i] + "`"}}}});
                                label += params[i];
                                if (i < params.size() - 1)
                                    label += ", ";
                            }
                            label += ")";
                        }
                    }

                    if (found) {
                        json signature = {{"label", label}, {"parameters", parameters}};

                        if (!documentation.empty()) {
                            signature["documentation"] = {{"kind", "markdown"}, {"value", documentation}};
                        }

                        result = {
                            {"signatures", {signature}}, {"activeSignature", 0}, {"activeParameter", activeParameter}};
                    }
                }
            }
        }
    }

    json response = {{"jsonrpc", "2.0"}, {"id", message["id"]}, {"result", result}};

    sendMessage(response);
}
void LSPServer::publishDiagnostics(const std::string &uri, const std::string &text) {
    json diagnostics = json::array();

    try {
        Lexer lexer(text);
        Parser parser(lexer);

        ErrorHandling::clearErrors();

        try {
            ASTNode *ast = parser.parse();
            delete ast;
        } catch (...) {
            // Exceptions might still be thrown
        }

        // Gather errors from ErrorHandling
        for (const auto &errMsg : ErrorHandling::getErrors()) {
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

                if (line < 0)
                    line = 0;
                if (col < 0)
                    col = 0;
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

    json notification = {{"jsonrpc", "2.0"},
                         {"method", "textDocument/publishDiagnostics"},
                         {"params", {{"uri", uri}, {"diagnostics", diagnostics}}}};

    sendMessage(notification);
}

void LSPServer::handleSemanticTokens(const json &message) {
    std::string uri = message["params"]["textDocument"]["uri"];

    json data = json::array();

    if (documents.find(uri) != documents.end()) {
        std::string text = documents[uri];
        Lexer lexer(text);

        int prevLine = 1;
        int prevChar = 1;

        while (true) {
            Token t = lexer.nextToken();
            if (t.type == TokenType::END_OF_FILE)
                break;
            if (t.type == TokenType::UNKNOWN)
                continue; // Skip unknowns to avoid corrupting output

            int typeIdx = -1;

            // Map TokenType to semantic token type index
            switch (t.type) {
            case TokenType::IDENTIFIER:
                typeIdx = 3;
                break; // variable
            case TokenType::STRING:
                typeIdx = 7;
                break; // string
            case TokenType::NUMBER:
                typeIdx = 9;
                break; // number
            case TokenType::CLASS:
            case TokenType::FN:
            case TokenType::LET:
            case TokenType::CONST:
            case TokenType::VIRTUAL:
            case TokenType::OVERRIDE:
            case TokenType::PUBLIC:
            case TokenType::PRIVATE:
            case TokenType::PROTECTED:
            case TokenType::STATIC:
            case TokenType::IF:
            case TokenType::ELSE:
            case TokenType::WHILE:
            case TokenType::DO:
            case TokenType::FOR:
            case TokenType::IN:
            case TokenType::RETURN:
            case TokenType::BREAK:
            case TokenType::CONTINUE:
            case TokenType::SWITCH:
            case TokenType::CASE:
            case TokenType::DEFAULT:
            case TokenType::ABSTRACT:
            case TokenType::INTERFACE:
            case TokenType::TRAIT:
            case TokenType::IMPLEMENTS:
            case TokenType::LOAD:
            case TokenType::DESTROY:
            case TokenType::USING:
            case TokenType::TRUE:
            case TokenType::FALSE:
            case TokenType::NIL:
            case TokenType::THIS:
            case TokenType::SUPER:
                typeIdx = 8;
                break; // keyword
            case TokenType::PLUS:
            case TokenType::MINUS:
            case TokenType::STAR:
            case TokenType::SLASH:
            case TokenType::PERCENT:
            case TokenType::PLUS_PLUS:
            case TokenType::MINUS_MINUS:
            case TokenType::PLUS_EQUAL:
            case TokenType::MINUS_EQUAL:
            case TokenType::STAR_EQUAL:
            case TokenType::SLASH_EQUAL:
            case TokenType::ASSIGN:
            case TokenType::EQUAL_EQUAL:
            case TokenType::BANG:
            case TokenType::BANG_EQUAL:
            case TokenType::LESS:
            case TokenType::LESS_EQUAL:
            case TokenType::GREATER:
            case TokenType::GREATER_EQUAL:
            case TokenType::AND:
            case TokenType::OR:
            case TokenType::BITWISE_AND:
            case TokenType::BITWISE_OR:
            case TokenType::BITWISE_XOR:
            case TokenType::BITWISE_NOT:
            case TokenType::SHIFT_LEFT:
            case TokenType::SHIFT_RIGHT:
                typeIdx = 10;
                break; // operator
            default:
                break;
            }

            if (typeIdx != -1) {
                // Ensure length is valid UTF-16 characters or just bytes for simple ASCII
                // VS Code uses UTF-16 character offsets, so technically lexeme.length() is wrong for Cyrillic.
                // For simplicity, we just use the lexeme length (or 1 if it's messy).
                int length = t.lexeme.length();
                if (length < 0)
                    length = 1;

                int deltaLine = t.line - prevLine;
                int deltaChar = t.column - (deltaLine == 0 ? prevChar : 1);
                if (deltaChar < 0)
                    deltaChar = 0; // Guard against negative deltas

                data.push_back(deltaLine);
                data.push_back(deltaChar);
                data.push_back(length);
                data.push_back(typeIdx);
                data.push_back(0); // modifier

                prevLine = t.line;
                prevChar = t.column;
            }
        }
    }

    json response = {{"jsonrpc", "2.0"}, {"id", message["id"]}, {"result", {{"data", data}}}};

    sendMessage(response);
}

} // namespace trypillia
