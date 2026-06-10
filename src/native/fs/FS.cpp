#include "FS.h"
#include <fstream>
#include <sstream>

namespace StdLib {
namespace FS {

    static VMValue fileRead(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::string>(args[0])) return nullptr;
        std::string path = std::get<std::string>(args[0]);
        std::ifstream file(path);
        if (!file.is_open()) return nullptr; // Or throw a runtime error in the future
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    static VMValue fileWrite(int argCount, VMValue* args) {
        if (argCount != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) return false;
        std::string path = std::get<std::string>(args[0]);
        std::string content = std::get<std::string>(args[1]);
        
        std::ofstream file(path);
        if (!file.is_open()) return false;
        
        file << content;
        file.close();
        return true;
    }

    void registerAll(VM* vm) {
        auto fileClass = std::make_shared<ObjClass>("File");
        fileClass->statics["read"] = std::make_shared<ObjNative>("read", 1, fileRead);
        fileClass->statics["write"] = std::make_shared<ObjNative>("write", 2, fileWrite);
        vm->globals["File"] = fileClass;
    }

    void registerSymbols(SymbolTable* scope) {
        Symbol sym;
        sym.name = "File";
        sym.type = "class";
        sym.isConst = true;
        scope->define(sym);
    }

}
}
