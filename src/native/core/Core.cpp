#include "Core.h"
#include <iostream>
#include <time.h>
#include <cctype>

namespace StdLib {
namespace Core {

    static VM* currentVM = nullptr;

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
                auto boundMethod = std::get<std::shared_ptr<ObjBoundMethod>>(args[i])->method;
                if (std::holds_alternative<std::shared_ptr<ObjFunction>>(boundMethod)) {
                    std::cout << "<bound method " << std::get<std::shared_ptr<ObjFunction>>(boundMethod)->name << ">";
                } else {
                    std::cout << "<bound method " << std::get<std::shared_ptr<ObjNative>>(boundMethod)->name << ">";
                }
            } else if (std::holds_alternative<std::nullptr_t>(args[i])) {
                std::cout << "nil";
            }
            if (i < argCount - 1) std::cout << " ";
        }
        std::cout << std::endl;
        return nullptr;
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

    // --- Error ---
    static VMValue errorInit(int argCount, VMValue* args) {
        VMValue receiver = args[-1];
        if (!std::holds_alternative<std::shared_ptr<ObjInstance>>(receiver)) return nullptr;
        auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);
        
        instance->fields["message"] = argCount > 0 ? args[0] : VMValue(std::string("Unknown Error"));
        instance->fields["code"] = argCount > 1 ? args[1] : VMValue(0.0);
        return nullptr;
    }

    // --- Result ---
    static VMValue resultOk(int argCount, VMValue* args) {
        if (argCount != 1) return nullptr;
        auto klass = std::get<std::shared_ptr<ObjClass>>(currentVM->globals["Result"]);
        auto instance = std::make_shared<ObjInstance>(klass);
        instance->fields["value"] = args[0];
        instance->fields["isOk"] = true;
        return instance;
    }

    static VMValue resultErr(int argCount, VMValue* args) {
        if (argCount != 1) return nullptr;
        auto klass = std::get<std::shared_ptr<ObjClass>>(currentVM->globals["Result"]);
        auto instance = std::make_shared<ObjInstance>(klass);
        instance->fields["error"] = args[0];
        instance->fields["isOk"] = false;
        return instance;
    }

    static VMValue resultIsOk(int argCount, VMValue* args) {
        VMValue receiver = args[-1];
        auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);
        return instance->fields["isOk"];
    }

    static VMValue resultIsErr(int argCount, VMValue* args) {
        VMValue receiver = args[-1];
        auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);
        return !std::get<bool>(instance->fields["isOk"]);
    }

    static VMValue resultUnwrap(int argCount, VMValue* args) {
        VMValue receiver = args[-1];
        auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);
        if (!std::get<bool>(instance->fields["isOk"])) {
            std::cerr << "Called unwrap() on an Err value!" << std::endl;
            exit(1); // Panic
        }
        return instance->fields["value"];
    }

    static VMValue resultUnwrapErr(int argCount, VMValue* args) {
        VMValue receiver = args[-1];
        auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);
        if (std::get<bool>(instance->fields["isOk"])) {
            std::cerr << "Called unwrapErr() on an Ok value!" << std::endl;
            exit(1); // Panic
        }
        return instance->fields["error"];
    }

    void registerAll(VM* vm) {
        currentVM = vm;
        vm->defineNative("print", -1, printNative);
        vm->defineNative("len", 1, lenNative);
        vm->defineNative("push", 2, pushNative);

        auto errorClass = std::make_shared<ObjClass>("Error");
        errorClass->methods["init"] = std::make_shared<ObjNative>("init", -1, errorInit);
        vm->globals["Error"] = errorClass;

        auto resultClass = std::make_shared<ObjClass>("Result");
        resultClass->statics["ok"] = std::make_shared<ObjNative>("ok", 1, resultOk);
        resultClass->statics["err"] = std::make_shared<ObjNative>("err", 1, resultErr);
        resultClass->methods["isOk"] = std::make_shared<ObjNative>("isOk", 0, resultIsOk);
        resultClass->methods["isErr"] = std::make_shared<ObjNative>("isErr", 0, resultIsErr);
        resultClass->methods["unwrap"] = std::make_shared<ObjNative>("unwrap", 0, resultUnwrap);
        resultClass->methods["unwrapErr"] = std::make_shared<ObjNative>("unwrapErr", 0, resultUnwrapErr);
        vm->globals["Result"] = resultClass;
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
        addFunc("len");
        addFunc("push");

        auto addClass = [&](const std::string& name) {
            Symbol sym;
            sym.name = name;
            sym.type = "class";
            sym.isConst = true;
            scope->define(sym);
        };
        addClass("Error");
        addClass("Result");
    }

}
}
