#include "Promise.h"
#include <csignal>

namespace StdLib {
namespace PromiseModule {

static VMValue resolveNative(int argCount, VMValue *args) {
    if (argCount < 1) return nullptr;
    VM *vm = currentVM;
    auto it = vm->globals.find("__promise_pending");
    if (it == vm->globals.end() || !it->second.isPromise()) return nullptr;
    ObjPromise *promise = it->second.asPromise();
    if (promise->resolved) return nullptr;
    promise->value = args[0];
    promise->resolved = true;
    if (promise->onFulfilled) {
        vm->push(VMValue(promise->onFulfilled));
        vm->push(promise->value);
        vm->callClosure(VMValue(promise->onFulfilled), 1, vm->stackTop - 1);
    }
    vm->globals.erase("__promise_pending");
    return nullptr;
}

static VMValue rejectNative(int argCount, VMValue *args) {
    if (argCount < 1) return nullptr;
    VM *vm = currentVM;
    auto it = vm->globals.find("__promise_pending");
    if (it == vm->globals.end() || !it->second.isPromise()) return nullptr;
    ObjPromise *promise = it->second.asPromise();
    if (promise->resolved) return nullptr;
    promise->value = args[0];
    promise->resolved = true;
    if (promise->onRejected) {
        vm->push(VMValue(promise->onRejected));
        vm->push(promise->value);
        vm->callClosure(VMValue(promise->onRejected), 1, vm->stackTop - 1);
    }
    vm->globals.erase("__promise_pending");
    return nullptr;
}

static VMValue thenNative(int argCount, VMValue *args) {
    VM *vm = currentVM;
    ObjPromise *promise = nullptr;

    int off = 0;
    if (argCount >= 1 && args[0].isPromise()) {
        promise = args[0].asPromise();
        off = 1;
    }
    if (!promise) return nullptr;

    if (promise->resolved) {
        if (argCount > off && args[off].isClosure()) {
            vm->push(args[off]);
            vm->push(promise->value);
            vm->callClosure(args[off], 1, vm->stackTop - 1);
        }
        return nullptr;
    }

    if (argCount > off && args[off].isClosure()) promise->onFulfilled = args[off].asClosure();
    if (argCount > off + 1 && args[off + 1].isClosure()) promise->onRejected = args[off + 1].asClosure();
    return nullptr;
}

static VMValue promiseConstructor(int argCount, VMValue *args) {
    VM *vm = currentVM;
    if (argCount < 1 || !args[0].isClosure()) {
        vm->runtimeError("Promise constructor requires a function(resolve, reject)");
        return nullptr;
    }
    ObjClosure *executor = args[0].asClosure();

    auto *promise = new ObjPromise();
    vm->globals["__promise_pending"] = VMValue(promise);

    auto resolveFn = new ObjNative("resolve", 1, resolveNative);
    auto rejectFn = new ObjNative("reject", 1, rejectNative);

    VMValue resolveVal(resolveFn);
    VMValue rejectVal(rejectFn);
    vm->callClosure(VMValue(executor), 2, &resolveVal);

    if (vm->globals.count("__promise_pending"))
        vm->globals.erase("__promise_pending");

    return VMValue(promise);
}

void registerAll(VM *vm) {
    auto thenFn = new ObjNative("then", -1, thenNative);
    vm->globals["__promise_then"] = VMValue(thenFn);

    vm->defineNative("Promise", 1, promiseConstructor);
}

void registerSymbols(SymbolTable *scope) {
    Symbol sym;
    sym.name = "Promise";
    sym.type = "function";
    sym.isConst = true;
    scope->define(sym);
}

} // namespace PromiseModule
} // namespace StdLib
