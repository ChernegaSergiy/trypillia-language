#include "Core.h"
#include <cctype>
#include <iostream>
#include <time.h>

namespace StdLib {
namespace Core {

thread_local VM *currentVM = nullptr;

static std::string stringify(const VMValue& val, bool inContainer = false) {
    if (std::holds_alternative<double>(val)) {
        std::string s = std::to_string(std::get<double>(val));
        s.erase(s.find_last_not_of('0') + 1, std::string::npos);
        if (s.back() == '.') s.pop_back();
        if (s.empty()) return "0";
        return s;
    } else if (std::holds_alternative<std::shared_ptr<ObjString>>(val)) {
        if (inContainer) {
            return "\"" + std::get<std::shared_ptr<ObjString>>(val)->flatten() + "\"";
        } else {
            return std::get<std::shared_ptr<ObjString>>(val)->flatten();
        }
    } else if (std::holds_alternative<bool>(val)) {
        return std::get<bool>(val) ? "true" : "false";
    } else if (std::holds_alternative<std::shared_ptr<ObjList>>(val)) {
        std::string s = "[";
        auto list = std::get<std::shared_ptr<ObjList>>(val);
        for (size_t i = 0; i < list->elements.size(); ++i) {
            s += stringify(list->elements[i], true);
            if (i < list->elements.size() - 1) s += ", ";
        }
        s += "]";
        return s;
    } else if (std::holds_alternative<std::shared_ptr<ObjMap>>(val)) {
        std::string s = "{";
        auto map = std::get<std::shared_ptr<ObjMap>>(val);
        size_t i = 0;
        for (auto const& [k, v] : map->values) {
            s += stringify(k, true) + ": " + stringify(v, true);
            if (i < map->values.size() - 1) s += ", ";
            i++;
        }
        s += "}";
        return s;
    } else if (std::holds_alternative<std::shared_ptr<ObjClass>>(val)) {
        return "<class " + std::get<std::shared_ptr<ObjClass>>(val)->name + ">";
    } else if (std::holds_alternative<std::shared_ptr<ObjInstance>>(val)) {
        return "<instance of " + std::get<std::shared_ptr<ObjInstance>>(val)->klass->name + ">";
    } else if (std::holds_alternative<std::shared_ptr<ObjBoundMethod>>(val)) {
        auto boundMethod = std::get<std::shared_ptr<ObjBoundMethod>>(val)->method;
        if (std::holds_alternative<std::shared_ptr<ObjFunction>>(boundMethod)) {
            return "<bound method " + std::get<std::shared_ptr<ObjFunction>>(boundMethod)->name + ">";
        } else {
            return "<bound method " + std::get<std::shared_ptr<ObjNative>>(boundMethod)->name + ">";
        }
    } else if (std::holds_alternative<std::nullptr_t>(val)) {
        return "nil";
    }
    return "unknown";
}

static VMValue printNative(int argCount, VMValue *args) {
    for (int i = 0; i < argCount; i++) {
        std::cout << stringify(args[i]);
        if (i < argCount - 1)
            std::cout << " ";
    }
    std::cout << std::endl;
    return nullptr;
}

static VMValue inputNative(int argCount, VMValue *args) {
    if (argCount == 1 && std::holds_alternative<std::shared_ptr<ObjString>>(args[0])) {
        std::cout << std::get<std::shared_ptr<ObjString>>(args[0])->flatten();
    }
    std::string line;
    std::getline(std::cin, line);
    return line;
}

// --- Error ---
static VMValue errorInit(int argCount, VMValue *args) {
    VMValue receiver = args[-1];
    if (!std::holds_alternative<std::shared_ptr<ObjInstance>>(receiver))
        return nullptr;
    auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);

    instance->fields["message"] = argCount > 0 ? args[0] : VMValue(std::string("Unknown Error"));
    instance->fields["code"] = argCount > 1 ? args[1] : VMValue(0.0);
    return nullptr;
}

// --- Result ---
static VMValue resultOk(int argCount, VMValue *args) {
    if (argCount != 1)
        return nullptr;
    auto klass = std::get<std::shared_ptr<ObjClass>>(currentVM->globals["Result"]);
    auto instance = std::make_shared<ObjInstance>(klass);
    instance->fields["value"] = args[0];
    instance->fields["isOk"] = true;
    return instance;
}

static VMValue resultErr(int argCount, VMValue *args) {
    if (argCount != 1)
        return nullptr;
    auto klass = std::get<std::shared_ptr<ObjClass>>(currentVM->globals["Result"]);
    auto instance = std::make_shared<ObjInstance>(klass);
    instance->fields["error"] = args[0];
    instance->fields["isOk"] = false;
    return instance;
}

static VMValue resultIsOk(int argCount, VMValue *args) {
    VMValue receiver = args[-1];
    auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);
    return instance->fields["isOk"];
}

static VMValue resultIsErr(int argCount, VMValue *args) {
    VMValue receiver = args[-1];
    auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);
    return !std::get<bool>(instance->fields["isOk"]);
}

static VMValue resultUnwrap(int argCount, VMValue *args) {
    VMValue receiver = args[-1];
    auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);
    if (!std::get<bool>(instance->fields["isOk"])) {
        std::cerr << "Called unwrap() on an Err value!" << std::endl;
        exit(1); // Panic
    }
    return instance->fields["value"];
}

static VMValue resultUnwrapErr(int argCount, VMValue *args) {
    VMValue receiver = args[-1];
    auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);
    if (std::get<bool>(instance->fields["isOk"])) {
        std::cerr << "Called unwrapErr() on an Ok value!" << std::endl;
        exit(1); // Panic
    }
    return instance->fields["error"];
}

// --- WeakRef ---
static VMValue weakRefInit(int argCount, VMValue *args) {
    VMValue receiver = args[-1];
    if (!std::holds_alternative<std::shared_ptr<ObjInstance>>(receiver))
        return nullptr;
    auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);

    if (argCount != 1) return nullptr;

    auto weakObj = std::make_shared<ObjWeakRef>();
    VMValue val = args[0];

    if (std::holds_alternative<std::shared_ptr<ObjString>>(val)) {
        weakObj->weakRef = std::weak_ptr<ObjString>(std::get<std::shared_ptr<ObjString>>(val));
    } else if (std::holds_alternative<std::shared_ptr<ObjList>>(val)) {
        weakObj->weakRef = std::weak_ptr<ObjList>(std::get<std::shared_ptr<ObjList>>(val));
    } else if (std::holds_alternative<std::shared_ptr<ObjMap>>(val)) {
        weakObj->weakRef = std::weak_ptr<ObjMap>(std::get<std::shared_ptr<ObjMap>>(val));
    } else if (std::holds_alternative<std::shared_ptr<ObjInstance>>(val)) {
        weakObj->weakRef = std::weak_ptr<ObjInstance>(std::get<std::shared_ptr<ObjInstance>>(val));
    } else if (std::holds_alternative<std::shared_ptr<ObjClosure>>(val)) {
        weakObj->weakRef = std::weak_ptr<ObjClosure>(std::get<std::shared_ptr<ObjClosure>>(val));
    } else if (std::holds_alternative<std::shared_ptr<ObjFunction>>(val)) {
        weakObj->weakRef = std::weak_ptr<ObjFunction>(std::get<std::shared_ptr<ObjFunction>>(val));
    } else if (std::holds_alternative<std::shared_ptr<ObjClass>>(val)) {
        weakObj->weakRef = std::weak_ptr<ObjClass>(std::get<std::shared_ptr<ObjClass>>(val));
    }

    instance->fields["_ref"] = weakObj;
    return nullptr;
}

static VMValue weakRefLock(int argCount, VMValue *args) {
    VMValue receiver = args[-1];
    auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);
    if (instance->fields.find("_ref") == instance->fields.end()) return nullptr;
    
    auto ref = instance->fields["_ref"];
    if (std::holds_alternative<std::shared_ptr<ObjWeakRef>>(ref)) {
        return std::get<std::shared_ptr<ObjWeakRef>>(ref)->lock();
    }
    return nullptr;
}

void registerAll(VM *vm) {
    currentVM = vm;
    vm->defineNative("print", -1, printNative);
    vm->defineNative("input", -1, inputNative);

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

    auto weakRefClass = std::make_shared<ObjClass>("WeakRef");
    weakRefClass->methods["init"] = std::make_shared<ObjNative>("init", 1, weakRefInit);
    weakRefClass->methods["lock"] = std::make_shared<ObjNative>("lock", 0, weakRefLock);
    vm->globals["WeakRef"] = weakRefClass;
}

void registerSymbols(SymbolTable *scope) {
    auto addFunc = [&](const std::string &name) {
        Symbol sym;
        sym.name = name;
        sym.type = "function";
        sym.isConst = true;
        scope->define(sym);
    };
    addFunc("print");
    addFunc("input");

    auto addClass = [&](const std::string &name) {
        Symbol sym;
        sym.name = name;
        sym.type = "class";
        sym.isConst = true;
        scope->define(sym);
    };
    addClass("Error");
    addClass("Result");
    addClass("WeakRef");
}

} // namespace Core
} // namespace StdLib
