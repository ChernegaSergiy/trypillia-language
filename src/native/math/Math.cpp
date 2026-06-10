#include "Math.h"
#include <cmath>
#include <cstdlib>

namespace StdLib {
namespace Math {

    static VMValue mathSin(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<double>(args[0])) return nullptr;
        return std::sin(std::get<double>(args[0]));
    }

    static VMValue mathCos(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<double>(args[0])) return nullptr;
        return std::cos(std::get<double>(args[0]));
    }

    static VMValue mathRandom(int argCount, VMValue* args) {
        return (double)rand() / RAND_MAX;
    }

    void registerAll(VM* vm) {
        auto mathClass = std::make_shared<ObjClass>("Math");
        mathClass->statics["sin"] = std::make_shared<ObjNative>("sin", 1, mathSin);
        mathClass->statics["cos"] = std::make_shared<ObjNative>("cos", 1, mathCos);
        mathClass->statics["random"] = std::make_shared<ObjNative>("random", 0, mathRandom);
        mathClass->statics["PI"] = 3.14159265358979323846;
        vm->globals["Math"] = mathClass;
    }

    void registerSymbols(SymbolTable* scope) {
        Symbol sym;
        sym.name = "Math";
        sym.type = "class";
        sym.isConst = true;
        scope->define(sym);
    }

}
}
