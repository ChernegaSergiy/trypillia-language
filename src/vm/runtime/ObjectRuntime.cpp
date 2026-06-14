#include "ObjectRuntime.h"
#include "../VM.h"
#include <string>

bool isMethodAbstract(const VMValue &method) {
    if (method.isClosure())
        return method.asClosure()->function->isAbstract;
    if (method.isNative())
        return method.asNative()->isAbstract;
    return false;
}

VMAccessModifier getMethodAccessModifier(const VMValue &method) {
    if (method.isClosure())
        return method.asClosure()->function->accessModifier;
    if (method.isNative())
        return method.asNative()->accessModifier;
    return VMAccessModifier::PUBLIC;
}

std::string getMethodName(const VMValue &method) {
    if (method.isClosure())
        return method.asClosure()->function->name;
    if (method.isNative())
        return method.asNative()->name;
    return "";
}

int getMethodMinArity(const VMValue &method) {
    if (method.isClosure())
        return method.asClosure()->function->arity;
    if (method.isNative())
        return method.asNative()->arity;
    return -1;
}

int getMethodMaxArity(const VMValue &method) {
    if (method.isClosure())
        return method.asClosure()->function->maxArity;
    if (method.isNative())
        return method.asNative()->arity;
    return -1;
}

int utf8_length(const std::string &str) {
    int length = 0;
    for (size_t i = 0; i < str.length(); i++) {
        if ((str[i] & 0xC0) != 0x80) {
            length++;
        }
    }
    return length;
}

std::string utf8_char_at(const std::string &str, int index) {
    int current_index = 0;
    for (size_t i = 0; i < str.length(); i++) {
        if ((str[i] & 0xC0) != 0x80) {
            if (current_index == index) {
                size_t j = i + 1;
                while (j < str.length() && (str[j] & 0xC0) == 0x80) {
                    j++;
                }
                return str.substr(i, j - i);
            }
            current_index++;
        }
    }
    return "";
}

bool checkAccess(VMAccessModifier modifier, ObjClass *klass, const std::string &callerClass) {
    if (modifier == VMAccessModifier::PUBLIC)
        return true;
    if (modifier == VMAccessModifier::PRIVATE)
        return klass->name == callerClass;
    if (modifier == VMAccessModifier::PROTECTED) {
        if (klass->name == callerClass)
            return true;
        auto current = klass;
        while (current) {
            if (current->name == callerClass)
                return true;
            current = current->superclass;
        }
        return false;
    }
    return true;
}

VMValue VM::callClosure(VMValue closureVal, int argCount, VMValue *args) {
    if (!closureVal.isClosure())
        return nullptr;
    auto closure = closureVal.asClosure();

    int initialFrameCount = static_cast<int>(frames.size());

    push(closureVal);
    for (int i = 0; i < argCount; i++) {
        push(args[i]);
    }

    CallFrame newFrame;
    newFrame.closure = closure;
    newFrame.ip = closure->function->chunk->code.data();
    newFrame.stackStart = static_cast<int>((stackTop - stack) - argCount - 1);
    frames.push_back(newFrame);

    InterpretResult result = run(initialFrameCount);
    if (result == InterpretResult::INTERPRET_RUNTIME_ERROR) {
        return nullptr;
    }

    return pop();
}

VMValue VM::instantiateClass(VMValue classVal, int argCount, VMValue *args) {
    if (!classVal.isClass())
        return nullptr;
    auto klass = classVal.asClass();
    if (klass->isAbstract)
        return nullptr;
    for (auto const &[name, method] : klass->methods) {
        if (isMethodAbstract(method))
            return nullptr;
    }
    auto instance = new ObjInstance(klass);

    if (klass->methods.count("init")) {
        auto initMethod = klass->methods["init"];
        int minArity = getMethodMinArity(initMethod);
        int maxArity = getMethodMaxArity(initMethod);
        if (minArity != -1 && (argCount < minArity || argCount > maxArity))
            return nullptr;
        while (maxArity != -1 && argCount < maxArity) {
            // Pad with nil
            VMValue nilVal = nullptr;
            // We can't easily pad here, rely on caller
            return nullptr;
        }

        if (initMethod.isClosure()) {
            push(instance);
            for (int i = 0; i < argCount; i++)
                push(args[i]);
            auto closure = initMethod.asClosure();
            CallFrame newFrame;
            newFrame.closure = closure;
            newFrame.ip = closure->function->chunk->code.data();
            newFrame.stackStart = static_cast<int>((stackTop - stack) - argCount - 1);
            frames.push_back(newFrame);
            int initialDepth = static_cast<int>(frames.size() - 1);
            InterpretResult res = run(initialDepth);
            frames.pop_back();
            if (res == InterpretResult::INTERPRET_RUNTIME_ERROR)
                return nullptr;
            return instance;
        }
    }
    return instance;
}

ObjUpvalue *VM::captureUpvalue(VMValue *local) {
    ObjUpvalue *prevUpvalue = nullptr;
    ObjUpvalue *upvalue = openUpvalues;
    while (upvalue != nullptr && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != nullptr && upvalue->location == local) {
        return upvalue;
    }

    auto createdUpvalue = new ObjUpvalue(local);
    createdUpvalue->next = upvalue;

    if (prevUpvalue == nullptr) {
        openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}

void VM::closeUpvalues(VMValue *last) {
    while (openUpvalues != nullptr && openUpvalues->location >= last) {
        auto upvalue = openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        openUpvalues = upvalue->next;
    }
}

void VM::defineNative(const std::string &name, int arity, NativeFn function) {
    globals[name] = new ObjNative(name, arity, function);
}
