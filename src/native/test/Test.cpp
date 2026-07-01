#include "Test.h"
#include <csignal>

namespace StdLib {
namespace Test {

static std::string valueToString(const VMValue &val) {
    return val.toString(true);
}

static void getSourceLocation(VM *vm, std::string &filename, int &line) {
    if (!vm || vm->frames.empty())
        return;
    auto &frame = vm->frames.back();
    auto func = frame.closure->function;
    filename = func->filename;
    auto ip = frame.ip;
    auto codeStart = func->chunk->code.data();
    if (ip > codeStart) {
        size_t instruction = (ip - codeStart) - 1;
        if (instruction < func->chunk->lines.size())
            line = func->chunk->lines[instruction];
    }
}

static void failAssertion(VM *vm, const std::string &message) {
    std::cerr << message << std::endl;
    if (vm->catchJumpEnabled) {
        siglongjmp(vm->catchJmpBuf, 1);
    } else if (vm->assertJumpEnabled) {
        siglongjmp(vm->assertJmpBuf, 1);
    }
}

static VMValue assertNative(int argCount, VMValue *args) {
    if (argCount < 1) {
        failAssertion(currentVM, "FAIL: assert requires at least 1 argument (condition)");
        return nullptr;
    }

    VM *vm = currentVM;
    bool condition = args[0].isBool() ? args[0].asBool() : !args[0].isNil();

    if (!condition) {
        std::string filename;
        int line = 0;
        getSourceLocation(vm, filename, line);

        std::string msg;
        if (argCount >= 2 && args[1].isString()) {
            msg = " — " + args[1].asString()->flatten();
        }

        std::string loc;
        if (!filename.empty())
            loc = filename + ":" + std::to_string(line) + " ";

        failAssertion(vm, "FAIL: " + loc + "assertion failed" + msg);
    }

    return nullptr;
}

static VMValue assertEqNative(int argCount, VMValue *args) {
    if (argCount < 2) {
        failAssertion(currentVM, "FAIL: assertEq requires at least 2 arguments (actual, expected)");
        return nullptr;
    }

    VM *vm = currentVM;
    VMValue actual = args[0];
    VMValue expected = args[1];

    if (!(actual == expected)) {
        std::string filename;
        int line = 0;
        getSourceLocation(vm, filename, line);

        std::string msg;
        if (argCount >= 3 && args[2].isString()) {
            msg = " — " + args[2].asString()->flatten();
        }

        std::string loc;
        if (!filename.empty())
            loc = filename + ":" + std::to_string(line) + " ";

        failAssertion(vm, "FAIL: " + loc + "expected " + valueToString(expected) + " but got " +
                           valueToString(actual) + msg);
    }

    return nullptr;
}

static VMValue assertNeqNative(int argCount, VMValue *args) {
    if (argCount < 2) {
        failAssertion(currentVM, "FAIL: assertNeq requires at least 2 arguments (actual, unexpected)");
        return nullptr;
    }

    VM *vm = currentVM;
    VMValue actual = args[0];
    VMValue unexpected = args[1];

    if (actual == unexpected) {
        std::string filename;
        int line = 0;
        getSourceLocation(vm, filename, line);

        std::string msg;
        if (argCount >= 3 && args[2].isString()) {
            msg = " — " + args[2].asString()->flatten();
        }

        std::string loc;
        if (!filename.empty())
            loc = filename + ":" + std::to_string(line) + " ";

        failAssertion(vm, "FAIL: " + loc + "expected " + valueToString(actual) + " to differ from " +
                           valueToString(unexpected) + msg);
    }

    return nullptr;
}

static VMValue assertThrowsNative(int argCount, VMValue *args) {
    if (argCount < 1) {
        failAssertion(currentVM, "FAIL: assertThrows requires at least 1 argument (function)");
        return nullptr;
    }

    VM *vm = currentVM;
    VMValue fn = args[0];

    if (!fn.isClosure()) {
        failAssertion(vm, "FAIL: assertThrows argument must be a function");
        return nullptr;
    }

    auto closure = fn.asClosure();

    auto savedStackTop = vm->stackTop;
    auto savedFrameCount = vm->frames.size();

    if (sigsetjmp(vm->catchJmpBuf, 1) == 0) {
        vm->catchJumpEnabled = true;
        vm->suppressRuntimeErrors = true;

        vm->push(fn);

        CallFrame frame;
        frame.closure = closure;
        frame.ip = closure->function->chunk->code.data();
        frame.stackStart = static_cast<int>((vm->stackTop - vm->stack) - 1);
        vm->frames.push_back(frame);

        InterpretResult result = vm->run(savedFrameCount);

        vm->catchJumpEnabled = false;
        vm->suppressRuntimeErrors = false;
        vm->stackTop = savedStackTop;
        vm->frames.resize(savedFrameCount);

        if (result == InterpretResult::INTERPRET_OK) {
            std::string filename;
            int line = 0;
            getSourceLocation(vm, filename, line);
            std::string loc;
            if (!filename.empty())
                loc = filename + ":" + std::to_string(line) + " ";

            std::string msg;
            if (argCount >= 2 && args[1].isString()) {
                msg = " — " + args[1].asString()->flatten();
            }

            failAssertion(vm, "FAIL: " + loc + "expected function to throw but it did not" + msg);
        }
    } else {
        vm->catchJumpEnabled = false;
        vm->suppressRuntimeErrors = false;
        vm->stackTop = savedStackTop;
        vm->frames.resize(savedFrameCount);
    }

    return nullptr;
}

void registerAll(VM *vm) {
    vm->defineNative("assert", -1, assertNative);
    vm->defineNative("assertEq", -1, assertEqNative);
    vm->defineNative("assertNeq", -1, assertNeqNative);
    vm->defineNative("assertThrows", -1, assertThrowsNative);
}

void registerSymbols(SymbolTable *scope) {
    auto addFunc = [&](const std::string &name) {
        Symbol sym;
        sym.name = name;
        sym.type = "function";
        sym.isConst = true;
        scope->define(sym);
    };
    addFunc("assert");
    addFunc("assertEq");
    addFunc("assertNeq");
    addFunc("assertThrows");
}

} // namespace Test
} // namespace StdLib
