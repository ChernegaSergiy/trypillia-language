#include "Promise.h"
#include <csignal>

namespace StdLib {
namespace PromiseModule {

static VMValue resolveNative(int argCount, VMValue *args) {
    if (argCount < 1) return nullptr;
    VM *vm = currentVM;
    ObjPromise *promise = nullptr;
    if (!vm->frames.empty()) {
        auto &frame = vm->frames.back();
        int base = frame.stackStart;
        int idx = base;
        if (idx >= 0 && idx < (int)(vm->stackTop - vm->stack)) {
            VMValue maybePromise = vm->stack[idx];
            if (maybePromise.isPromise()) {
                promise = maybePromise.asPromise();
            }
        }
    }
    if (promise && !promise->resolved) {
        promise->value = args[0];
        promise->resolved = true;
        if (promise->onFulfilled) {
            vm->push(VMValue(promise->onFulfilled));
            vm->push(promise->value);
            vm->callClosure(VMValue(promise->onFulfilled), 1, vm->stackTop - 1);
        }
    }
    return nullptr;
}

static VMValue rejectNative(int argCount, VMValue *args) {
    if (argCount < 1) return nullptr;
    VM *vm = currentVM;
    ObjPromise *promise = nullptr;
    if (!vm->frames.empty()) {
        auto &frame = vm->frames.back();
        int base = frame.stackStart;
        int idx = base;
        if (idx >= 0 && idx < (int)(vm->stackTop - vm->stack)) {
            VMValue maybePromise = vm->stack[idx];
            if (maybePromise.isPromise()) {
                promise = maybePromise.asPromise();
            }
        }
    }
    if (promise && !promise->resolved) {
        promise->value = args[0];
        promise->resolved = true;
        if (promise->onRejected) {
            vm->push(VMValue(promise->onRejected));
            vm->push(promise->value);
            vm->callClosure(VMValue(promise->onRejected), 1, vm->stackTop - 1);
        }
    }
    return nullptr;
}

static VMValue thenNative(int argCount, VMValue *args) {
    if (argCount < 1) return nullptr;
    VM *vm = currentVM;
    if (vm->stackTop - vm->stack < 1) return nullptr;
    VMValue self = vm->stackTop[-1];
    if (!self.isPromise()) return nullptr;
    ObjPromise *promise = self.asPromise();

    if (promise->resolved) {
        if (promise->onFulfilled && args[0].isClosure()) {
            vm->push(args[0]);
            vm->push(promise->value);
            vm->callClosure(args[0], 1, vm->stackTop - 1);
        }
        return nullptr;
    }

    if (args[0].isClosure()) promise->onFulfilled = args[0].asClosure();
    if (argCount >= 2 && args[1].isClosure()) promise->onRejected = args[1].asClosure();
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
    VMValue promiseVal(promise);

    auto resolveFn = new ObjNative("resolve", 1, resolveNative);
    auto rejectFn = new ObjNative("reject", 1, rejectNative);

    vm->push(promiseVal);
    vm->push(VMValue(executor));
    VMValue resolveVal(resolveFn);
    VMValue rejectVal(rejectFn);
    vm->push(resolveVal);
    vm->push(rejectVal);
    vm->callClosure(VMValue(executor), 2, vm->stackTop - 2);

    vm->stackTop[-3] = promiseVal;
    vm->pop();
    vm->pop();

    return nullptr;
}

void registerAll(VM *vm) {
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
