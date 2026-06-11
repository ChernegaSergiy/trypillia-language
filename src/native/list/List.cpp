#include "List.h"
#include <algorithm>
#include <string>

namespace StdLib {
namespace ListModule {

    thread_local VM* currentVM = nullptr;

    static VMValue listLength(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::shared_ptr<ObjList>>(args[0])) return nullptr;
        return (double)std::get<std::shared_ptr<ObjList>>(args[0])->elements.size();
    }

    static VMValue listPush(int argCount, VMValue* args) {
        if (argCount != 2 || !std::holds_alternative<std::shared_ptr<ObjList>>(args[0])) return nullptr;
        auto list = std::get<std::shared_ptr<ObjList>>(args[0]);
        list->elements.push_back(args[1]);
        return args[0];
    }

    static VMValue listPop(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::shared_ptr<ObjList>>(args[0])) return nullptr;
        auto list = std::get<std::shared_ptr<ObjList>>(args[0]);
        if (list->elements.empty()) return nullptr;
        VMValue last = list->elements.back();
        list->elements.pop_back();
        return last;
    }

    static VMValue listInsert(int argCount, VMValue* args) {
        if (argCount != 3 || !std::holds_alternative<std::shared_ptr<ObjList>>(args[0]) || 
            !std::holds_alternative<double>(args[1])) return nullptr;
        auto list = std::get<std::shared_ptr<ObjList>>(args[0]);
        int index = std::get<double>(args[1]);
        if (index < 0 || index > list->elements.size()) return nullptr;
        
        list->elements.insert(list->elements.begin() + index, args[2]);
        return args[0];
    }

    static VMValue listRemove(int argCount, VMValue* args) {
        if (argCount != 2 || !std::holds_alternative<std::shared_ptr<ObjList>>(args[0]) || 
            !std::holds_alternative<double>(args[1])) return nullptr;
        auto list = std::get<std::shared_ptr<ObjList>>(args[0]);
        int index = std::get<double>(args[1]);
        if (index < 0 || index >= list->elements.size()) return nullptr;
        
        VMValue removed = list->elements[index];
        list->elements.erase(list->elements.begin() + index);
        return removed;
    }

    static VMValue listReverse(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::shared_ptr<ObjList>>(args[0])) return nullptr;
        auto list = std::get<std::shared_ptr<ObjList>>(args[0]);
        std::reverse(list->elements.begin(), list->elements.end());
        return args[0];
    }

    static VMValue listJoin(int argCount, VMValue* args) {
        if (argCount != 2 || !std::holds_alternative<std::shared_ptr<ObjList>>(args[0]) || 
            !std::holds_alternative<std::string>(args[1])) return nullptr;
        auto list = std::get<std::shared_ptr<ObjList>>(args[0]);
        std::string delim = std::get<std::string>(args[1]);
        
        std::string result = "";
        for (size_t i = 0; i < list->elements.size(); i++) {
            if (std::holds_alternative<std::string>(list->elements[i])) {
                result += std::get<std::string>(list->elements[i]);
            } else if (std::holds_alternative<double>(list->elements[i])) {
                result += std::to_string(std::get<double>(list->elements[i])); // Basic cast
            }
            // More types can be converted...
            
            if (i < list->elements.size() - 1) {
                result += delim;
            }
        }
        return result;
    }

    static VMValue listMap(int argCount, VMValue* args) {
        if (argCount != 2 || !std::holds_alternative<std::shared_ptr<ObjList>>(args[0])) return nullptr;
        auto list = std::get<std::shared_ptr<ObjList>>(args[0]);
        VMValue closure = args[1];
        
        std::vector<VMValue> result;
        result.reserve(list->elements.size());
        
        for (size_t i = 0; i < list->elements.size(); i++) {
            VMValue arg = list->elements[i];
            VMValue res = currentVM->callClosure(closure, 1, &arg);
            result.push_back(res);
        }
        return std::make_shared<ObjList>(result);
    }

    static VMValue listFilter(int argCount, VMValue* args) {
        if (argCount != 2 || !std::holds_alternative<std::shared_ptr<ObjList>>(args[0])) return nullptr;
        auto list = std::get<std::shared_ptr<ObjList>>(args[0]);
        VMValue closure = args[1];
        
        std::vector<VMValue> result;
        
        for (size_t i = 0; i < list->elements.size(); i++) {
            VMValue arg = list->elements[i];
            VMValue res = currentVM->callClosure(closure, 1, &arg);
            bool isTruthy = false;
            if (std::holds_alternative<bool>(res)) {
                isTruthy = std::get<bool>(res);
            } else if (!std::holds_alternative<std::nullptr_t>(res)) {
                isTruthy = true;
            }
            if (isTruthy) {
                result.push_back(arg);
            }
        }
        return std::make_shared<ObjList>(result);
    }

    static VMValue listForEach(int argCount, VMValue* args) {
        if (argCount != 2 || !std::holds_alternative<std::shared_ptr<ObjList>>(args[0])) return nullptr;
        auto list = std::get<std::shared_ptr<ObjList>>(args[0]);
        VMValue closure = args[1];
        
        for (size_t i = 0; i < list->elements.size(); i++) {
            VMValue arg = list->elements[i];
            currentVM->callClosure(closure, 1, &arg);
        }
        return nullptr;
    }

    void registerAll(VM* vm) {
        currentVM = vm;
        auto listClass = std::make_shared<ObjClass>("List");
        
        listClass->statics["length"] = std::make_shared<ObjNative>("length", 1, listLength);
        listClass->statics["push"] = std::make_shared<ObjNative>("push", 2, listPush);
        listClass->statics["pop"] = std::make_shared<ObjNative>("pop", 1, listPop);
        listClass->statics["insert"] = std::make_shared<ObjNative>("insert", 3, listInsert);
        listClass->statics["remove"] = std::make_shared<ObjNative>("remove", 2, listRemove);
        listClass->statics["reverse"] = std::make_shared<ObjNative>("reverse", 1, listReverse);
        listClass->statics["join"] = std::make_shared<ObjNative>("join", 2, listJoin);
        listClass->statics["map"] = std::make_shared<ObjNative>("map", 2, listMap);
        listClass->statics["filter"] = std::make_shared<ObjNative>("filter", 2, listFilter);
        listClass->statics["forEach"] = std::make_shared<ObjNative>("forEach", 2, listForEach);

        vm->globals["List"] = listClass;
    }

    void registerSymbols(SymbolTable* scope) {
        Symbol sym;
        sym.name = "List";
        sym.type = "class";
        sym.isConst = true;
        scope->define(sym);
    }
}
}
