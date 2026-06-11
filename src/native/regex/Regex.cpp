#include "Regex.h"
#include <regex>

namespace StdLib {
namespace RegexModule {

    thread_local VM* currentVM = nullptr;

    static VMValue regexTest(int argCount, VMValue* args) {
        if (argCount != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) {
            return nullptr;
        }
        std::string pattern = std::get<std::string>(args[0]);
        std::string text = std::get<std::string>(args[1]);
        
        try {
            std::regex re(pattern);
            return std::regex_search(text, re);
        } catch (const std::regex_error&) {
            return nullptr;
        }
    }

    static VMValue regexMatch(int argCount, VMValue* args) {
        if (argCount != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) {
            return nullptr;
        }
        std::string pattern = std::get<std::string>(args[0]);
        std::string text = std::get<std::string>(args[1]);
        
        try {
            std::regex re(pattern);
            std::smatch match;
            if (std::regex_search(text, match, re)) {
                std::vector<VMValue> results;
                for (size_t i = 0; i < match.size(); ++i) {
                    results.push_back(match.str(i));
                }
                return std::make_shared<ObjList>(results);
            }
        } catch (const std::regex_error&) {
            return nullptr;
        }
        return nullptr;
    }

    static VMValue regexReplace(int argCount, VMValue* args) {
        if (argCount != 3 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1]) || !std::holds_alternative<std::string>(args[2])) {
            return nullptr;
        }
        std::string pattern = std::get<std::string>(args[0]);
        std::string text = std::get<std::string>(args[1]);
        std::string replacement = std::get<std::string>(args[2]);
        
        try {
            std::regex re(pattern);
            return std::regex_replace(text, re, replacement);
        } catch (const std::regex_error&) {
            return nullptr;
        }
    }

    void registerAll(VM* vm) {
        currentVM = vm;
        auto regexClass = std::make_shared<ObjClass>("Regex");
        
        regexClass->statics["test"] = std::make_shared<ObjNative>("test", 2, regexTest);
        regexClass->statics["match"] = std::make_shared<ObjNative>("match", 2, regexMatch);
        regexClass->statics["replace"] = std::make_shared<ObjNative>("replace", 3, regexReplace);

        vm->globals["Regex"] = regexClass;
    }

    void registerSymbols(SymbolTable* scope) {
        Symbol sym;
        sym.name = "Regex";
        sym.type = "class";
        sym.isConst = true;
        scope->define(sym);
    }

}
}
