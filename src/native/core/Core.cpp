#include "Core.h"
#include <iostream>
#include <time.h>
#include <cctype>

namespace StdLib {
namespace Core {

    static VMValue printNative(int argCount, VMValue* args) {
        for (int i = 0; i < argCount; i++) {
            if (std::holds_alternative<double>(args[i])) {
                std::cout << std::get<double>(args[i]);
            } else if (std::holds_alternative<std::string>(args[i])) {
                std::cout << std::get<std::string>(args[i]);
            } else if (std::holds_alternative<bool>(args[i])) {
                std::cout << (std::get<bool>(args[i]) ? "true" : "false");
            } else if (std::holds_alternative<std::shared_ptr<ObjList>>(args[i])) {
                std::cout << "[list]";
            } else if (std::holds_alternative<std::shared_ptr<ObjMap>>(args[i])) {
                std::cout << "{map}";
            } else if (std::holds_alternative<std::shared_ptr<ObjClass>>(args[i])) {
                std::cout << "<class " << std::get<std::shared_ptr<ObjClass>>(args[i])->name << ">";
            } else if (std::holds_alternative<std::shared_ptr<ObjInstance>>(args[i])) {
                std::cout << "<instance of " << std::get<std::shared_ptr<ObjInstance>>(args[i])->klass->name << ">";
            } else if (std::holds_alternative<std::shared_ptr<ObjBoundMethod>>(args[i])) {
                std::cout << "<bound method " << std::get<std::shared_ptr<ObjBoundMethod>>(args[i])->method->name << ">";
            } else if (std::holds_alternative<std::nullptr_t>(args[i])) {
                std::cout << "nil";
            }
            if (i < argCount - 1) std::cout << " ";
        }
        std::cout << std::endl;
        return nullptr;
    }

    static VMValue clockNative(int argCount, VMValue* args) {
        return (double)clock() / CLOCKS_PER_SEC;
    }

    static VMValue lenNative(int argCount, VMValue* args) {
        if (argCount != 1) return nullptr;
        if (std::holds_alternative<std::shared_ptr<ObjList>>(args[0])) {
            return (double)std::get<std::shared_ptr<ObjList>>(args[0])->elements.size();
        } else if (std::holds_alternative<std::string>(args[0])) {
            return (double)std::get<std::string>(args[0]).length();
        }
        return nullptr;
    }

    static VMValue pushNative(int argCount, VMValue* args) {
        if (argCount != 2) return nullptr;
        if (std::holds_alternative<std::shared_ptr<ObjList>>(args[0])) {
            auto list = std::get<std::shared_ptr<ObjList>>(args[0]);
            list->elements.push_back(args[1]);
            return args[0];
        }
        return nullptr;
    }

    static VMValue substringNative(int argCount, VMValue* args) {
        if (argCount != 3) return nullptr;
        if (!std::holds_alternative<std::string>(args[0]) || 
            !std::holds_alternative<double>(args[1]) || 
            !std::holds_alternative<double>(args[2])) return nullptr;
            
        std::string str = std::get<std::string>(args[0]);
        int start = std::get<double>(args[1]);
        int length = std::get<double>(args[2]);
        
        if (start < 0 || start >= str.length()) return std::string("");
        return str.substr(start, length);
    }

    static VMValue toUpperNative(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::string>(args[0])) return nullptr;
        std::string str = std::get<std::string>(args[0]);
        for (char& c : str) c = std::toupper(c);
        return str;
    }

    static VMValue toLowerNative(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::string>(args[0])) return nullptr;
        std::string str = std::get<std::string>(args[0]);
        for (char& c : str) c = std::tolower(c);
        return str;
    }

    void registerAll(VM* vm) {
        vm->defineNative("print", -1, printNative);
        vm->defineNative("clock", 0, clockNative);
        vm->defineNative("len", 1, lenNative);
        vm->defineNative("push", 2, pushNative);
        vm->defineNative("substring", 3, substringNative);
        vm->defineNative("toUpper", 1, toUpperNative);
        vm->defineNative("toLower", 1, toLowerNative);
    }

    void registerSymbols(SymbolTable* scope) {
        auto addFunc = [&](const std::string& name) {
            Symbol sym;
            sym.name = name;
            sym.type = "function";
            sym.isConst = true;
            scope->define(sym);
        };
        addFunc("print");
        addFunc("clock");
        addFunc("len");
        addFunc("push");
        addFunc("substring");
        addFunc("toUpper");
        addFunc("toLower");
    }

}
}
