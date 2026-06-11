#include "OS.h"
#include "../StdLib.h"
#include <cstdlib>
#include <filesystem>
#include <array>
#include <memory>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

namespace StdLib {
namespace OSModule {
    
    std::vector<std::string> commandLineArgs;

    thread_local VM* currentVM = nullptr;

    static VMValue osGetEnv(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::string>(args[0])) return nullptr;
        std::string name = std::get<std::string>(args[0]);
        const char* val = std::getenv(name.c_str());
        if (val == nullptr) {
            return makeResultErr(currentVM, "Environment variable not found");
        }
        return makeResultOk(currentVM, std::string(val));
    }

    static VMValue osCwd(int argCount, VMValue* args) {
        try {
            return makeResultOk(currentVM, std::filesystem::current_path().string());
        } catch (...) {
            return makeResultErr(currentVM, "Failed to get current working directory");
        }
    }

    static VMValue osExec(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::string>(args[0])) return nullptr;
        std::string cmd = std::get<std::string>(args[0]);
        
        std::array<char, 128> buffer;
        std::string result;
        
        // Use POPEN to run the command and read its stdout
        auto deleter = [](FILE* f) { PCLOSE(f); };
        std::unique_ptr<FILE, decltype(deleter)> pipe(POPEN(cmd.c_str(), "r"), deleter);
        if (!pipe) {
            return makeResultErr(currentVM, "popen() failed!");
        }
        
        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        
        return makeResultOk(currentVM, result);
    }

    static VMValue osExit(int argCount, VMValue* args) {
        int exitCode = 0;
        if (argCount == 1 && std::holds_alternative<double>(args[0])) {
            exitCode = static_cast<int>(std::get<double>(args[0]));
        }
        std::exit(exitCode);
        return nullptr; // Unreachable
    }

    static VMValue osArgs(int argCount, VMValue* args) {
        std::vector<VMValue> listElements;
        for (const auto& arg : commandLineArgs) {
            listElements.push_back(arg);
        }
        return std::make_shared<ObjList>(listElements);
    }

    void registerAll(VM* vm) {
        currentVM = vm;
        auto osClass = std::make_shared<ObjClass>("OS");
        
        osClass->statics["getEnv"] = std::make_shared<ObjNative>("getEnv", 1, osGetEnv);
        osClass->statics["cwd"] = std::make_shared<ObjNative>("cwd", 0, osCwd);
        osClass->statics["exec"] = std::make_shared<ObjNative>("exec", 1, osExec);
        osClass->statics["exit"] = std::make_shared<ObjNative>("exit", -1, osExit); // -1 means variable number of args (0 or 1)
        osClass->statics["args"] = std::make_shared<ObjNative>("args", 0, osArgs);

        vm->globals["OS"] = osClass;
    }

    void registerSymbols(SymbolTable* scope) {
        Symbol sym;
        sym.name = "OS";
        sym.type = "class";
        sym.isConst = true;
        scope->define(sym);
    }
}
}
