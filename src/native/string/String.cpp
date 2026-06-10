#include "String.h"
#include <cctype>
#include <algorithm>
#include <string>

namespace StdLib {
namespace StringModule {

    static VM* currentVM = nullptr;

    static VMValue stringLength(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::string>(args[0])) return nullptr;
        return (double)std::get<std::string>(args[0]).length();
    }

    static VMValue stringSubstring(int argCount, VMValue* args) {
        if (argCount != 3 || !std::holds_alternative<std::string>(args[0]) || 
            !std::holds_alternative<double>(args[1]) || 
            !std::holds_alternative<double>(args[2])) return nullptr;
            
        std::string str = std::get<std::string>(args[0]);
        int start = std::get<double>(args[1]);
        int length = std::get<double>(args[2]);
        
        if (start < 0 || start >= str.length()) return std::string("");
        return str.substr(start, length);
    }

    static VMValue stringToUpper(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::string>(args[0])) return nullptr;
        std::string str = std::get<std::string>(args[0]);
        for (char& c : str) c = std::toupper(c);
        return str;
    }

    static VMValue stringToLower(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::string>(args[0])) return nullptr;
        std::string str = std::get<std::string>(args[0]);
        for (char& c : str) c = std::tolower(c);
        return str;
    }

    static VMValue stringTrim(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::string>(args[0])) return nullptr;
        std::string str = std::get<std::string>(args[0]);
        
        auto start = str.begin();
        while (start != str.end() && std::isspace(*start)) {
            start++;
        }
        
        auto end = str.end();
        do {
            end--;
        } while (std::distance(start, end) > 0 && std::isspace(*end));
        
        return std::string(start, end + 1);
    }

    static VMValue stringSplit(int argCount, VMValue* args) {
        if (argCount != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) return nullptr;
        
        std::string str = std::get<std::string>(args[0]);
        std::string delim = std::get<std::string>(args[1]);
        
        auto list = std::make_shared<ObjList>(std::vector<VMValue>{});
        size_t start = 0;
        size_t end = str.find(delim);
        
        while (end != std::string::npos) {
            list->elements.push_back(str.substr(start, end - start));
            start = end + delim.length();
            end = str.find(delim, start);
        }
        list->elements.push_back(str.substr(start));
        
        return list;
    }

    static VMValue stringReplace(int argCount, VMValue* args) {
        if (argCount != 3 || !std::holds_alternative<std::string>(args[0]) || 
            !std::holds_alternative<std::string>(args[1]) || 
            !std::holds_alternative<std::string>(args[2])) return nullptr;
            
        std::string str = std::get<std::string>(args[0]);
        std::string search = std::get<std::string>(args[1]);
        std::string replace = std::get<std::string>(args[2]);
        
        if (search.empty()) return str;
        
        size_t pos = 0;
        while ((pos = str.find(search, pos)) != std::string::npos) {
            str.replace(pos, search.length(), replace);
            pos += replace.length();
        }
        
        return str;
    }

    static VMValue stringIndexOf(int argCount, VMValue* args) {
        if (argCount != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) return nullptr;
        std::string str = std::get<std::string>(args[0]);
        std::string search = std::get<std::string>(args[1]);
        
        size_t pos = str.find(search);
        if (pos == std::string::npos) return (double)-1;
        return (double)pos;
    }

    static VMValue stringIncludes(int argCount, VMValue* args) {
        if (argCount != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) return nullptr;
        std::string str = std::get<std::string>(args[0]);
        std::string search = std::get<std::string>(args[1]);
        
        return str.find(search) != std::string::npos;
    }

    static VMValue stringStartsWith(int argCount, VMValue* args) {
        if (argCount != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) return nullptr;
        std::string str = std::get<std::string>(args[0]);
        std::string prefix = std::get<std::string>(args[1]);
        
        return str.rfind(prefix, 0) == 0;
    }

    static VMValue stringEndsWith(int argCount, VMValue* args) {
        if (argCount != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) return nullptr;
        std::string str = std::get<std::string>(args[0]);
        std::string suffix = std::get<std::string>(args[1]);
        
        if (str.length() >= suffix.length()) {
            return (0 == str.compare(str.length() - suffix.length(), suffix.length(), suffix));
        } else {
            return false;
        }
    }

    void registerAll(VM* vm) {
        currentVM = vm;
        auto stringClass = std::make_shared<ObjClass>("String");
        
        stringClass->statics["length"] = std::make_shared<ObjNative>("length", 1, stringLength);
        stringClass->statics["substring"] = std::make_shared<ObjNative>("substring", 3, stringSubstring);
        stringClass->statics["toUpper"] = std::make_shared<ObjNative>("toUpper", 1, stringToUpper);
        stringClass->statics["toLower"] = std::make_shared<ObjNative>("toLower", 1, stringToLower);
        stringClass->statics["trim"] = std::make_shared<ObjNative>("trim", 1, stringTrim);
        stringClass->statics["split"] = std::make_shared<ObjNative>("split", 2, stringSplit);
        stringClass->statics["replace"] = std::make_shared<ObjNative>("replace", 3, stringReplace);
        stringClass->statics["indexOf"] = std::make_shared<ObjNative>("indexOf", 2, stringIndexOf);
        stringClass->statics["includes"] = std::make_shared<ObjNative>("includes", 2, stringIncludes);
        stringClass->statics["startsWith"] = std::make_shared<ObjNative>("startsWith", 2, stringStartsWith);
        stringClass->statics["endsWith"] = std::make_shared<ObjNative>("endsWith", 2, stringEndsWith);

        vm->globals["String"] = stringClass;
    }

    void registerSymbols(SymbolTable* scope) {
        Symbol sym;
        sym.name = "String";
        sym.type = "class";
        sym.isConst = true;
        scope->define(sym);
    }
}
}
