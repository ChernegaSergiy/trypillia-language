#include "Json.h"
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <cctype>

namespace StdLib {
namespace Json {

    // Simple JSON Stringifier
    static std::string stringifyValue(const VMValue& val) {
        if (std::holds_alternative<std::nullptr_t>(val)) return "null";
        if (std::holds_alternative<bool>(val)) return std::get<bool>(val) ? "true" : "false";
        if (std::holds_alternative<double>(val)) {
            std::stringstream ss;
            ss << std::get<double>(val);
            return ss.str();
        }
        if (std::holds_alternative<std::string>(val)) {
            std::string s = std::get<std::string>(val);
            std::string res = "\"";
            for (char c : s) {
                if (c == '"') res += "\\\"";
                else if (c == '\\') res += "\\\\";
                else if (c == '\n') res += "\\n";
                else if (c == '\r') res += "\\r";
                else if (c == '\t') res += "\\t";
                else res += c;
            }
            res += "\"";
            return res;
        }
        if (std::holds_alternative<std::shared_ptr<ObjList>>(val)) {
            auto list = std::get<std::shared_ptr<ObjList>>(val);
            std::string res = "[";
            for (size_t i = 0; i < list->elements.size(); ++i) {
                res += stringifyValue(list->elements[i]);
                if (i < list->elements.size() - 1) res += ",";
            }
            res += "]";
            return res;
        }
        if (std::holds_alternative<std::shared_ptr<ObjMap>>(val)) {
            auto map = std::get<std::shared_ptr<ObjMap>>(val);
            std::string res = "{";
            bool first = true;
            for (auto const& [k, v] : map->values) {
                if (!first) res += ",";
                first = false;
                if (std::holds_alternative<std::string>(k)) {
                    res += stringifyValue(k) + ":" + stringifyValue(v);
                } else {
                    // JSON keys must be strings
                    std::stringstream ss;
                    if (std::holds_alternative<double>(k)) ss << std::get<double>(k);
                    else if (std::holds_alternative<bool>(k)) ss << (std::get<bool>(k) ? "true" : "false");
                    else if (std::holds_alternative<std::nullptr_t>(k)) ss << "null";
                    res += "\"" + ss.str() + "\":" + stringifyValue(v);
                }
            }
            res += "}";
            return res;
        }
        return "null"; // Default fallback
    }

    static VMValue jsonStringify(int argCount, VMValue* args) {
        if (argCount != 1) return nullptr;
        return stringifyValue(args[0]);
    }

    // Simple JSON Parser
    class JsonParser {
        const std::string& src;
        size_t pos = 0;

        void skipWhitespace() {
            while (pos < src.length() && std::isspace(src[pos])) pos++;
        }

        VMValue parseString() {
            pos++; // skip "
            std::string res;
            while (pos < src.length() && src[pos] != '"') {
                if (src[pos] == '\\' && pos + 1 < src.length()) {
                    pos++;
                    if (src[pos] == 'n') res += '\n';
                    else if (src[pos] == 'r') res += '\r';
                    else if (src[pos] == 't') res += '\t';
                    else res += src[pos];
                } else {
                    res += src[pos];
                }
                pos++;
            }
            if (pos < src.length()) pos++; // skip "
            return res;
        }

        VMValue parseNumber() {
            size_t start = pos;
            while (pos < src.length() && (std::isdigit(src[pos]) || src[pos] == '.' || src[pos] == '-' || src[pos] == 'e' || src[pos] == 'E' || src[pos] == '+')) {
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
                if (src[pos] != '"') break;
                VMValue key = parseString();
                skipWhitespace();
                if (src[pos] != ':') break;
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
        JsonParser(const std::string& src) : src(src) {}

        VMValue parseValue() {
            skipWhitespace();
            if (pos >= src.length()) return nullptr;
            char c = src[pos];
            if (c == '"') return parseString();
            if (c == '[') return parseArray();
            if (c == '{') return parseObject();
            if (std::isdigit(c) || c == '-') return parseNumber();
            if (src.compare(pos, 4, "true") == 0) { pos += 4; return true; }
            if (src.compare(pos, 5, "false") == 0) { pos += 5; return false; }
            if (src.compare(pos, 4, "null") == 0) { pos += 4; return nullptr; }
            return nullptr;
        }
    };

    static VMValue jsonParse(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::string>(args[0])) return nullptr;
        std::string jsonStr = std::get<std::string>(args[0]);
        JsonParser parser(jsonStr);
        return parser.parseValue();
    }

    void registerAll(VM* vm) {
        auto jsonClass = std::make_shared<ObjClass>("Json");
        jsonClass->statics["stringify"] = std::make_shared<ObjNative>("stringify", 1, jsonStringify);
        jsonClass->statics["parse"] = std::make_shared<ObjNative>("parse", 1, jsonParse);
        vm->globals["Json"] = jsonClass;
    }

    void registerSymbols(SymbolTable* scope) {
        Symbol sym;
        sym.name = "Json";
        sym.type = "class";
        sym.isConst = true;
        scope->define(sym);
    }
}
}
