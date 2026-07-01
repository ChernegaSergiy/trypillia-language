#include "Promise.h"
#include <csignal>

namespace StdLib {
namespace PromiseModule {

static void scheduleResolve(VM *vm, ObjPromise *promise, VMValue value, ObjPromise *nextPromise,
                            ObjClosure *onFulfilled, ObjClosure *onRejected) {
    if (onFulfilled) {
        ObjFunction *func = new ObjFunction();
        func->arity = 0;
        func->maxArity = 0;
        func->name = "promise_microtask";
        ObjClosure *closure = new ObjClosure(func);

        auto *callerFrame = &vm->frames.back();
        auto savedIp = callerFrame->ip;
        auto savedStack = vm->stackTop;

        vm->push(VMValue(onFulfilled));
        vm->push(value);
        vm->callClosure(VMValue(onFulfilled), 1, vm->stackTop - 1);

        if (vm->stackTop > savedStack) {
            VMValue result = vm->pop();
            vm->push(result);
            if (nextPromise) {
                nextPromise->value = result;
                nextPromise->resolved = true;
                for (size_t i = 0; i < nextPromise->thenHandlers.size(); i += 3) {
                    auto hOnFulfilled = nextPromise->thenHandlers[i];
                    auto hNextPromise = nextPromise->thenHandlers[i + 2];
                    scheduleResolve(vm, nextPromise, result,
                                    hNextPromise.isPromise() ? hNextPromise.asPromise() : nullptr,
                                    hOnFulfilled.isClosure() ? hOnFulfilled.asClosure() : nullptr,
                                    nullptr);
                }
                nextPromise->thenHandlers.clear();
            }
        }
    }
}

static VMValue resolveNative(int argCount, VMValue *args) {
    if (argCount < 1) return nullptr;
    VM *vm = currentVM;
    auto it = vm->globals.find("__promise_pending");
    if (it == vm->globals.end() || !it->second.isPromise()) return nullptr;
    ObjPromise *promise = it->second.asPromise();
    if (promise->resolved) return nullptr;
    promise->value = args[0];
    promise->resolved = true;
    vm->globals.erase("__promise_pending");

    auto handlers = std::move(promise->thenHandlers);
    promise->thenHandlers.clear();
    for (size_t i = 0; i < handlers.size(); i += 3) {
        auto hOnFulfilled = handlers[i];
        auto hOnRejected = handlers[i + 1];
        auto hNextPromise = handlers[i + 2];
        scheduleResolve(vm, promise, promise->value,
                        hNextPromise.isPromise() ? hNextPromise.asPromise() : nullptr,
                        hOnFulfilled.isClosure() ? hOnFulfilled.asClosure() : nullptr,
                        hOnRejected.isClosure() ? hOnRejected.asClosure() : nullptr);
    }

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
    vm->globals.erase("__promise_pending");

    auto handlers = std::move(promise->thenHandlers);
    promise->thenHandlers.clear();
    for (size_t i = 0; i < handlers.size(); i += 3) {
        auto hOnRejected = handlers[i + 1];
        auto hNextPromise = handlers[i + 2];
        if (hOnRejected.isClosure() && hNextPromise.isPromise()) {
            scheduleResolve(vm, promise, promise->value,
                            hNextPromise.asPromise(),
                            nullptr, hOnRejected.asClosure());
        }
    }

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

    auto *newPromise = new ObjPromise();

    if (promise->resolved) {
        if (argCount > off && args[off].isClosure()) {
            scheduleResolve(vm, promise, promise->value, newPromise,
                            args[off].asClosure(), nullptr);
        }
        return VMValue(newPromise);
    }

    promise->thenHandlers.push_back(argCount > off && args[off].isClosure() ? args[off] : VMValue(nullptr));
    promise->thenHandlers.push_back(argCount > off + 1 && args[off + 1].isClosure() ? args[off + 1] : VMValue(nullptr));
    promise->thenHandlers.push_back(VMValue(newPromise));
    return VMValue(newPromise);
}

static VMValue resolveStaticNative(int argCount, VMValue *args) {
    auto *promise = new ObjPromise();
    promise->value = argCount >= 1 ? args[0] : VMValue(nullptr);
    promise->resolved = true;
    return VMValue(promise);
}

static VMValue rejectStaticNative(int argCount, VMValue *args) {
    auto *promise = new ObjPromise();
    promise->value = argCount >= 1 ? args[0] : VMValue(nullptr);
    promise->resolved = true;
    return VMValue(promise);
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

    auto resolveStatic = new ObjNative("resolve", 1, resolveStaticNative);
    vm->globals["__promise_resolve"] = VMValue(resolveStatic);
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
