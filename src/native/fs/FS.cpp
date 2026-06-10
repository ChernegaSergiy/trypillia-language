#include "FS.h"
#include <fstream>
#include <sstream>

namespace StdLib {
namespace FS {

    static VM* currentVM = nullptr;

    static void freeFile(void* nativeData) {
        std::fstream* file = static_cast<std::fstream*>(nativeData);
        if (file) {
            if (file->is_open()) file->close();
            delete file;
        }
    }

    static VMValue fileOpen(int argCount, VMValue* args) {
        if (argCount < 1 || argCount > 2 || !std::holds_alternative<std::string>(args[0])) return nullptr;
        std::string path = std::get<std::string>(args[0]);
        std::string mode = "r";
        if (argCount == 2 && std::holds_alternative<std::string>(args[1])) {
            mode = std::get<std::string>(args[1]);
        }

        std::ios_base::openmode fmode = std::ios_base::in;
        if (mode == "w") fmode = std::ios_base::out;
        else if (mode == "a") fmode = std::ios_base::app;
        else if (mode == "rw") fmode = std::ios_base::in | std::ios_base::out;

        std::fstream* file = new std::fstream(path, fmode);
        if (!file->is_open()) {
            delete file;
            return nullptr; // Later return Error object
        }

        auto fileClass = std::get<std::shared_ptr<ObjClass>>(currentVM->globals["File"]);
        auto instance = std::make_shared<ObjInstance>(fileClass);
        instance->nativeData = file;
        instance->freeFn = freeFile;

        return instance;
    }

    static VMValue fileRead(int argCount, VMValue* args) {
        if (argCount != 0) return nullptr;
        // The implicit "this" receiver is at args[-1] because the VM pushes receiver before args
        VMValue receiver = args[-1];
        if (!std::holds_alternative<std::shared_ptr<ObjInstance>>(receiver)) return nullptr;
        
        auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);
        std::fstream* file = static_cast<std::fstream*>(instance->nativeData);
        if (!file || !file->is_open()) return nullptr;
        
        std::stringstream buffer;
        buffer << file->rdbuf();
        return buffer.str();
    }

    static VMValue fileWrite(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::string>(args[0])) return false;
        VMValue receiver = args[-1];
        if (!std::holds_alternative<std::shared_ptr<ObjInstance>>(receiver)) return false;
        
        auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);
        std::fstream* file = static_cast<std::fstream*>(instance->nativeData);
        if (!file || !file->is_open()) return false;
        
        std::string content = std::get<std::string>(args[0]);
        *file << content;
        return true;
    }

    static VMValue fileClose(int argCount, VMValue* args) {
        if (argCount != 0) return false;
        VMValue receiver = args[-1];
        if (!std::holds_alternative<std::shared_ptr<ObjInstance>>(receiver)) return false;
        
        auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);
        std::fstream* file = static_cast<std::fstream*>(instance->nativeData);
        if (file && file->is_open()) {
            file->close();
        }
        return true;
    }

    void registerAll(VM* vm) {
        currentVM = vm;
        auto fileClass = std::make_shared<ObjClass>("File");
        fileClass->statics["open"] = std::make_shared<ObjNative>("open", -1, fileOpen);
        
        fileClass->methods["read"] = std::make_shared<ObjNative>("read", 0, fileRead);
        fileClass->methods["write"] = std::make_shared<ObjNative>("write", 1, fileWrite);
        fileClass->methods["close"] = std::make_shared<ObjNative>("close", 0, fileClose);
        
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
