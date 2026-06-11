#include "Map.h"

namespace StdLib {
namespace MapModule {

    thread_local VM* currentVM = nullptr;

    static VMValue mapKeys(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::shared_ptr<ObjMap>>(args[0])) return nullptr;
        auto map = std::get<std::shared_ptr<ObjMap>>(args[0]);
        std::vector<VMValue> keys;
        for (const auto& pair : map->values) {
            keys.push_back(pair.first);
        }
        return std::make_shared<ObjList>(keys);
    }

    static VMValue mapValues(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::shared_ptr<ObjMap>>(args[0])) return nullptr;
        auto map = std::get<std::shared_ptr<ObjMap>>(args[0]);
        std::vector<VMValue> values;
        for (const auto& pair : map->values) {
            values.push_back(pair.second);
        }
        return std::make_shared<ObjList>(values);
    }

    static VMValue mapHas(int argCount, VMValue* args) {
        if (argCount != 2 || !std::holds_alternative<std::shared_ptr<ObjMap>>(args[0])) return nullptr;
        auto map = std::get<std::shared_ptr<ObjMap>>(args[0]);
        return map->values.count(args[1]) > 0;
    }

    static VMValue mapRemove(int argCount, VMValue* args) {
        if (argCount != 2 || !std::holds_alternative<std::shared_ptr<ObjMap>>(args[0])) return nullptr;
        auto map = std::get<std::shared_ptr<ObjMap>>(args[0]);
        if (map->values.count(args[1]) > 0) {
            VMValue val = map->values[args[1]];
            map->values.erase(args[1]);
            return val;
        }
        return nullptr;
    }

    void registerAll(VM* vm) {
        currentVM = vm;
        auto mapClass = std::make_shared<ObjClass>("Map");
        
        mapClass->statics["keys"] = std::make_shared<ObjNative>("keys", 1, mapKeys);
        mapClass->statics["values"] = std::make_shared<ObjNative>("values", 1, mapValues);
        mapClass->statics["has"] = std::make_shared<ObjNative>("has", 2, mapHas);
        mapClass->statics["remove"] = std::make_shared<ObjNative>("remove", 2, mapRemove);

        vm->globals["Map"] = mapClass;
    }

    void registerSymbols(SymbolTable* scope) {
        Symbol sym;
        sym.name = "Map";
        sym.type = "class";
        sym.isConst = true;
        scope->define(sym);
    }

}
}
