#include "Json.h"
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace StdLib {
namespace Json {

static std::string stringifyValue(const VMValue &val, int indent = -1, int currentIndent = 0) {
    if (std::holds_alternative<std::nullptr_t>(val))
        return "null";
    if (std::holds_alternative<bool>(val))
        return std::get<bool>(val) ? "true" : "false";
    if (std::holds_alternative<double>(val)) {
        double d = std::get<double>(val);
        if (d == static_cast<double>(static_cast<long long>(d))) {
            return std::to_string(static_cast<long long>(d));
        }
        std::stringstream ss;
        ss << d;
        return ss.str();
    }
    if (std::holds_alternative<std::shared_ptr<ObjString>>(val)) {
        std::string s = std::get<std::shared_ptr<ObjString>>(val)->flatten();
        std::string res = "\"";
        for (char c : s) {
            if (c == '"')
                res += "\\\"";
            else if (c == '\\')
                res += "\\\\";
            else if (c == '\n')
                res += "\\n";
            else if (c == '\r')
                res += "\\r";
            else if (c == '\t')
                res += "\\t";
            else
                res += c;
        }
        res += "\"";
        return res;
    }

    std::string nl = (indent >= 0) ? "\n" : "";
    std::string sp = (indent >= 0) ? " " : "";

    if (std::holds_alternative<std::shared_ptr<ObjList>>(val)) {
        auto list = std::get<std::shared_ptr<ObjList>>(val);
        if (list->elements.empty())
            return "[]";

        std::string res = "[" + nl;
        int nextIndent = (indent >= 0) ? currentIndent + indent : 0;
        std::string indentStr = (indent >= 0) ? std::string(nextIndent, ' ') : "";
        std::string endIndentStr = (indent >= 0) ? std::string(currentIndent, ' ') : "";

        for (size_t i = 0; i < list->elements.size(); ++i) {
            res += indentStr + stringifyValue(list->elements[i], indent, nextIndent);
            if (i < list->elements.size() - 1)
                res += "," + nl;
            else
                res += nl;
        }
        res += endIndentStr + "]";
        return res;
    }
    if (std::holds_alternative<std::shared_ptr<ObjMap>>(val)) {
        auto map = std::get<std::shared_ptr<ObjMap>>(val);
        if (map->values.empty())
            return "{}";

        std::string res = "{" + nl;
        int nextIndent = (indent >= 0) ? currentIndent + indent : 0;
        std::string indentStr = (indent >= 0) ? std::string(nextIndent, ' ') : "";
        std::string endIndentStr = (indent >= 0) ? std::string(currentIndent, ' ') : "";

        bool first = true;
        for (auto const &[k, v] : map->values) {
            if (!first)
                res += "," + nl;
            first = false;

            res += indentStr;
            if (std::holds_alternative<std::shared_ptr<ObjString>>(k)) {
                res += stringifyValue(k, -1, 0) + ":" + sp + stringifyValue(v, indent, nextIndent);
            } else {
                std::stringstream ss;
                if (std::holds_alternative<double>(k)) {
                    double dk = std::get<double>(k);
                    if (dk == static_cast<double>(static_cast<long long>(dk))) {
                        ss << static_cast<long long>(dk);
                    } else {
                        ss << dk;
                    }
                } else if (std::holds_alternative<bool>(k))
                    ss << (std::get<bool>(k) ? "true" : "false");
                else if (std::holds_alternative<std::nullptr_t>(k))
                    ss << "null";
                res += "\"" + ss.str() + "\":" + sp + stringifyValue(v, indent, nextIndent);
            }
        }
        res += nl + endIndentStr + "}";
        return res;
    }
    return "null"; // Default fallback
}

static VMValue jsonStringify(int argCount, VMValue *args) {
    if (argCount < 1 || argCount > 2)
        return nullptr;
    int indent = -1;
    if (argCount == 2 && std::holds_alternative<double>(args[1])) {
        indent = static_cast<int>(std::get<double>(args[1]));
    }
    return stringifyValue(args[0], indent, 0);
}

// Simple JSON Parser
class JsonParser {
    const std::string &src;
    size_t pos = 0;

    void skipWhitespace() {
        while (pos < src.length() && std::isspace(src[pos]))
            pos++;
    }

    VMValue parseString() {
        pos++; // skip "
        std::string res;
        while (pos < src.length() && src[pos] != '"') {
            if (src[pos] == '\\' && pos + 1 < src.length()) {
                pos++;
                if (src[pos] == 'n')
                    res += '\n';
                else if (src[pos] == 'r')
                    res += '\r';
                else if (src[pos] == 't')
                    res += '\t';
                else if (src[pos] == 'u' && pos + 4 < src.length()) {
                    std::string hexStr = src.substr(pos + 1, 4);
                    try {
                        int codePoint = std::stoi(hexStr, nullptr, 16);
                        if (codePoint <= 0x7F) {
                            res += static_cast<char>(codePoint);
                        } else if (codePoint <= 0x7FF) {
                            res += static_cast<char>(0xC0 | ((codePoint >> 6) & 0x1F));
                            res += static_cast<char>(0x80 | (codePoint & 0x3F));
                        } else if (codePoint <= 0xFFFF) {
                            res += static_cast<char>(0xE0 | ((codePoint >> 12) & 0x0F));
                            res += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
                            res += static_cast<char>(0x80 | (codePoint & 0x3F));
                        } else if (codePoint <= 0x10FFFF) {
                            res += static_cast<char>(0xF0 | ((codePoint >> 18) & 0x07));
                            res += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
                            res += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
                            res += static_cast<char>(0x80 | (codePoint & 0x3F));
                        }
                    } catch (...) {
                        res += src[pos];
                    }
                    pos += 4;
                } else
                    res += src[pos];
            } else {
                res += src[pos];
            }
            pos++;
        }
        if (pos < src.length())
            pos++; // skip "
        return res;
    }

    VMValue parseNumber() {
        size_t start = pos;
        while (pos < src.length() && (std::isdigit(src[pos]) || src[pos] == '.' || src[pos] == '-' || src[pos] == 'e' ||
                                      src[pos] == 'E' || src[pos] == '+')) {
            pos++;
        }
        std::string numStr = src.substr(start, pos - start);
        try {
            return std::stod(numStr);
        } catch (...) {
            return 0.0;
        }
    }

    VMValue parseArray() {
        pos++; // skip [
        skipWhitespace();
        std::vector<VMValue> elements;
        if (pos < src.length() && src[pos] == ']') {
            pos++;
            return std::make_shared<ObjList>(elements);
        }
        while (pos < src.length()) {
            elements.push_back(parseValue());
            skipWhitespace();
            if (pos < src.length() && src[pos] == ',') {
                pos++;
                skipWhitespace();
            } else if (pos < src.length() && src[pos] == ']') {
                pos++;
                break;
            } else {
                break; // Unexpected character
            }
        }
        return std::make_shared<ObjList>(elements);
    }

    VMValue parseObject() {
        pos++; // skip {
        skipWhitespace();
        auto map = std::make_shared<ObjMap>();
        if (pos < src.length() && src[pos] == '}') {
            pos++;
            return map;
        }
        while (pos < src.length()) {
            skipWhitespace();
            if (src[pos] != '"')
                break;
            VMValue key = parseString();
            skipWhitespace();
            if (src[pos] != ':')
                break;
            pos++;
            skipWhitespace();
            VMValue value = parseValue();
            map->values[key] = value;
            skipWhitespace();
            if (pos < src.length() && src[pos] == ',') {
                pos++;
            } else if (pos < src.length() && src[pos] == '}') {
                pos++;
                break;
            } else {
                break;
            }
        }
        return map;
    }

  public:
    JsonParser(const std::string &src) : src(src) {
    }

    VMValue parseValue() {
        skipWhitespace();
        if (pos >= src.length())
            return nullptr;
        char c = src[pos];
        if (c == '"')
            return parseString();
        if (c == '[')
            return parseArray();
        if (c == '{')
            return parseObject();
        if (std::isdigit(c) || c == '-')
            return parseNumber();
        if (src.compare(pos, 4, "true") == 0) {
            pos += 4;
            return true;
        }
        if (src.compare(pos, 5, "false") == 0) {
            pos += 5;
            return false;
        }
        if (src.compare(pos, 4, "null") == 0) {
            pos += 4;
            return nullptr;
        }
        return nullptr;
    }
};

static VMValue jsonParse(int argCount, VMValue *args) {
    if (argCount != 1 || !std::holds_alternative<std::shared_ptr<ObjString>>(args[0]))
        return nullptr;
    std::string jsonStr = std::get<std::shared_ptr<ObjString>>(args[0])->flatten();
    JsonParser parser(jsonStr);
    return parser.parseValue();
}

void registerAll(VM *vm) {
    auto jsonClass = std::make_shared<ObjClass>("Json");
    jsonClass->statics["stringify"] = std::make_shared<ObjNative>("stringify", -1, jsonStringify);
    jsonClass->statics["parse"] = std::make_shared<ObjNative>("parse", 1, jsonParse);
    vm->globals["Json"] = jsonClass;
}

void registerSymbols(SymbolTable *scope) {
    Symbol sym;
    sym.name = "Json";
    sym.type = "class";
    sym.isConst = true;
    scope->define(sym);
}
} // namespace Json
} // namespace StdLib
