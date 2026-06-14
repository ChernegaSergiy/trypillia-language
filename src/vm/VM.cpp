#include "VM.h"
#include <iostream>
#include <cmath>

VMValue::VMValue(const std::string& s) {
    Obj* obj = new ObjString(s);
    val = SIGN_BIT | QNAN | (uint64_t)(uintptr_t)obj;
    obj->retain();
}

VMValue::VMValue(const char* s) {
    Obj* obj = new ObjString(std::string(s));
    val = SIGN_BIT | QNAN | (uint64_t)(uintptr_t)obj;
    obj->retain();
}
#include "../native/StdLib.h"

static bool isMethodAbstract(const VMValue &method) {
    if (method.isClosure())
        return method.asClosure()->function->isAbstract;
    if (method.isNative())
        return method.asNative()->isAbstract;
    return false;
}

static VMAccessModifier getMethodAccessModifier(const VMValue &method) {
    if (method.isClosure())
        return method.asClosure()->function->accessModifier;
    if (method.isNative())
        return method.asNative()->accessModifier;
    return VMAccessModifier::PUBLIC;
}

static std::string getMethodName(const VMValue &method) {
    if (method.isClosure())
        return method.asClosure()->function->name;
    if (method.isNative())
        return method.asNative()->name;
    return "";
}

static int getMethodMinArity(const VMValue &method) {
    if (method.isClosure())
        return method.asClosure()->function->arity;
    if (method.isNative())
        return method.asNative()->arity;
    return -1;
}

static int getMethodMaxArity(const VMValue &method) {
    if (method.isClosure())
        return method.asClosure()->function->maxArity;
    if (method.isNative())
        return method.asNative()->arity;
    return -1;
}

namespace {
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

bool checkAccess(VMAccessModifier modifier, ObjClass* klass, const std::string &callerClass) {
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
} // namespace

extern "C" double jit_call_helper(void* vm_ptr, double callee_val, double* args, int argCount) {
    VM* vm = static_cast<VM*>(vm_ptr);
    VMValue callee;
    memcpy(&callee, &callee_val, sizeof(double));
    VMValue result = nullptr;
    
    std::vector<VMValue> vmArgs(argCount);
    for (int i = 0; i < argCount; i++) {
        // The double contains raw VMValue bits
        double argD = args[i];
        memcpy(&vmArgs[i], &argD, sizeof(double));
    }
    
    if (callee.isNative()) {
        auto native = callee.asNative();
        result = native->function(argCount, vmArgs.data());
    } else if (callee.isClosure()) {
        result = vm->callClosure(callee, argCount, vmArgs.data());
    }
    
    double ret;
    memcpy(&ret, &result, sizeof(double));
    return ret;
}

extern "C" double jit_get_global_helper(void* vm_ptr, const char* name) {
    VM* vm = static_cast<VM*>(vm_ptr);
    auto it = vm->globals.find(name);
    VMValue result = nullptr;
    if (it != vm->globals.end()) {
        result = it->second;
    }
    double ret;
    memcpy(&ret, &result, sizeof(double));
    return ret;
}

extern "C" void jit_set_global_helper(void* vm_ptr, const char* name, double val_d) {
    VM* vm = static_cast<VM*>(vm_ptr);
    VMValue val;
    memcpy(&val, &val_d, sizeof(double));
    vm->globals[name] = val;
}

extern "C" double jit_index_get_helper(void* vm_ptr, double object_val, double index_val) {
    VMValue obj, idx;
    memcpy(&obj, &object_val, sizeof(double));
    memcpy(&idx, &index_val, sizeof(double));

    if (obj.isList()) {
        if (idx.isNumber()) {
            int index = static_cast<int>(idx.asNumber());
            auto list = obj.asList();
            if (index >= 0 && index < list->elements.size()) {
                VMValue res = list->elements[index];
                double ret;
                memcpy(&ret, &res, sizeof(double));
                return ret;
            }
        }
    } else if (obj.isString()) {
        if (idx.isNumber()) {
            int index = static_cast<int>(idx.asNumber());
            auto str = obj.asString()->flatten();
            if (index >= 0 && index < str.length()) {
                VMValue res = VMValue(std::string(1, str[index]));
                double ret;
                memcpy(&ret, &res, sizeof(double));
                return ret;
            }
        }
    } else if (obj.isMap()) {
        auto map = obj.asMap();
        if (map->values.count(idx)) {
            VMValue res = map->values[idx];
            double ret;
            memcpy(&ret, &res, sizeof(double));
            return ret;
        }
    }
    
    // Return nil
    VMValue nilVal = nullptr;
    double ret;
    memcpy(&ret, &nilVal, sizeof(double));
    return ret;
}

extern "C" double jit_index_set_helper(void* vm_ptr, double object_val, double index_val, double value_val) {
    VMValue obj, idx, val;
    memcpy(&obj, &object_val, sizeof(double));
    memcpy(&idx, &index_val, sizeof(double));
    memcpy(&val, &value_val, sizeof(double));

    if (obj.isList()) {
        if (idx.isNumber()) {
            int index = static_cast<int>(idx.asNumber());
            auto list = obj.asList();
            if (index >= 0 && index < list->elements.size()) {
                list->elements[index] = val;
            }
        }
    } else if (obj.isMap()) {
        auto map = obj.asMap();
        map->values[idx] = val;
    }
    return value_val;
}

VM::VM() {
    StdLib::registerAll(this);
}

VM::~VM() {
}

void VM::defineNative(const std::string &name, int arity, NativeFn function) {
    globals[name] = new ObjNative(name, arity, function);
}

void VM::resetStack() {
    stack.clear();
}

void VM::push(VMValue value) {
    stack.push_back(value);
}

VMValue VM::pop() {
    VMValue value = stack.back();
    stack.pop_back();
    return value;
}

VMValue VM::peek(int distance) {
    return stack[stack.size() - 1 - distance];
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
    newFrame.stackStart = static_cast<int>(stack.size() - argCount - 1);
    frames.push_back(newFrame);

    InterpretResult result = run(initialFrameCount);
    if (result == InterpretResult::INTERPRET_RUNTIME_ERROR) {
        return nullptr;
    }

    return pop();
}

ObjUpvalue* VM::captureUpvalue(VMValue *local) {
    ObjUpvalue* prevUpvalue = nullptr;
    ObjUpvalue* upvalue = openUpvalues;
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

InterpretResult VM::interpret(ObjFunction* function) {
    stack.clear();
    frames.clear();
    openUpvalues = nullptr;

    auto closure = new ObjClosure(function);
    push(closure);

    CallFrame frame;
    frame.closure = closure;
    frame.ip = function->chunk->code.data();
    frame.stackStart = 0;
    frames.push_back(frame);

    return run(0);
}

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() (frame->closure->function->chunk->constants[READ_BYTE()])
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

InterpretResult VM::runtimeError(const std::string &message) {
    std::cerr << "\n ૮ ˶ᵔ ᵕ ᵔ˶ ა \n / づ 📝 ♡ \n\n";
    std::cerr << "Panic: " << message << "\n\n";
    std::cerr << "Traceback (most recent call last):\n";

    for (int i = static_cast<int>(frames.size()) - 1; i >= 0; i--) {
        CallFrame *frame = &frames[i];
        ObjFunction* function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk->code.data() - 1;
        int line = function->chunk->lines[static_cast<int>(instruction)];

        std::cerr << "  at ";
        if (function->name.empty() || function->name == "<script>") {
            std::cerr << "<main>";
        } else {
            std::cerr << function->name << "()";
        }

        std::string fname = function->filename.empty() ? "<unknown>" : function->filename;
        // if fname has path, extract only basename for cleaner output like 'test.try'
        size_t lastSlash = fname.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            fname = fname.substr(lastSlash + 1);
        }

        std::cerr << " in " << fname << ":" << line << "\n";
    }
    std::cerr << std::endl;
    resetStack();
    return InterpretResult::INTERPRET_RUNTIME_ERROR;
}

InterpretResult VM::run(int targetFrameDepth) {
    CallFrame *frame = &frames.back();

    for (;;) {
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
        case static_cast<uint8_t>(OpCode::OP_NOP):
            break;
        case static_cast<uint8_t>(OpCode::OP_CONSTANT): {
            VMValue constant = READ_CONSTANT();
            push(constant);
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_TRUE): {
            push(true);
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_FALSE): {
            push(false);
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_NIL): {
            push(nullptr);
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_ADD): {
            VMValue b = pop();
            VMValue a = pop();
            if (a.isNumber() && b.isNumber()) {
                push(a.asNumber() + b.asNumber());
            } else if (a.isString() && b.isString()) {
                push(new ObjString(a.asString(), b.asString()));
            } else if (a.isString() && b.isNumber()) {
                double val = b.asNumber();
                std::string strVal = std::to_string(val);
                strVal.erase(strVal.find_last_not_of('0') + 1, std::string::npos);
                if (strVal.back() == '.')
                    strVal.pop_back();
                push(new ObjString(a.asString(), new ObjString(strVal)));
            } else if (a.isNumber() && b.isString()) {
                double val = a.asNumber();
                std::string strVal = std::to_string(val);
                strVal.erase(strVal.find_last_not_of('0') + 1, std::string::npos);
                if (strVal.back() == '.')
                    strVal.pop_back();
                push(new ObjString(new ObjString(strVal), b.asString()));
            } else {
                return runtimeError(std::string("Operands must be numbers or strings."));
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_SUBTRACT): {
            VMValue b = pop();
            VMValue a = pop();
            if (a.isNumber() && b.isNumber()) {
                push(a.asNumber() - b.asNumber());
            } else {
                return runtimeError("Operands must be numbers.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_MULTIPLY): {
            VMValue b = pop();
            VMValue a = pop();
            if (a.isNumber() && b.isNumber()) {
                push(a.asNumber() * b.asNumber());
            } else {
                return runtimeError("Operands must be numbers.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_DIVIDE): {
            VMValue b = pop();
            VMValue a = pop();
            if (a.isNumber() && b.isNumber()) {
                push(a.asNumber() / b.asNumber());
            } else {
                return runtimeError("Operands must be numbers.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_MOD): {
            VMValue b = pop();
            VMValue a = pop();
            if (a.isNumber() && b.isNumber()) {
                push(std::fmod(a.asNumber(), b.asNumber()));
            } else {
                return runtimeError("Operands must be numbers.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_BIT_AND): {
            VMValue b = pop();
            VMValue a = pop();
            if (a.isNumber() && b.isNumber()) {
                push(static_cast<double>(static_cast<int32_t>(a.asNumber()) &
                                         static_cast<int32_t>(b.asNumber())));
            } else {
                return runtimeError("Operands must be numbers.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_BIT_OR): {
            VMValue b = pop();
            VMValue a = pop();
            if (a.isNumber() && b.isNumber()) {
                push(static_cast<double>(static_cast<int32_t>(a.asNumber()) |
                                         static_cast<int32_t>(b.asNumber())));
            } else {
                return runtimeError("Operands must be numbers.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_BIT_XOR): {
            VMValue b = pop();
            VMValue a = pop();
            if (a.isNumber() && b.isNumber()) {
                push(static_cast<double>(static_cast<int32_t>(a.asNumber()) ^
                                         static_cast<int32_t>(b.asNumber())));
            } else {
                return runtimeError("Operands must be numbers.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_BIT_SHIFT_LEFT): {
            VMValue b = pop();
            VMValue a = pop();
            if (a.isNumber() && b.isNumber()) {
                push(static_cast<double>(static_cast<int32_t>(a.asNumber())
                                         << static_cast<int32_t>(b.asNumber())));
            } else {
                return runtimeError("Operands must be numbers.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_BIT_SHIFT_RIGHT): {
            VMValue b = pop();
            VMValue a = pop();
            if (a.isNumber() && b.isNumber()) {
                push(static_cast<double>(static_cast<int32_t>(a.asNumber()) >>
                                         static_cast<int32_t>(b.asNumber())));
            } else {
                return runtimeError("Operands must be numbers.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_EQUAL): {
            VMValue b = pop();
            VMValue a = pop();
            if (a.isNumber() && b.isNumber()) {
                push(a.asNumber() == b.asNumber());
            } else if (a.isString() && b.isString()) {
                push(a.asString()->flatten() == b.asString()->flatten());
            } else if (a.isBool() && b.isBool()) {
                push(a.asBool() == b.asBool());
            } else if (a.isNil() && b.isNil()) {
                push(true);
            } else {
                push(false);
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_LESS): {
            VMValue b = pop();
            VMValue a = pop();
            if (a.isNumber() && b.isNumber()) {
                push(a.asNumber() < b.asNumber());
            } else {
                return runtimeError("Operands must be numbers.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_GREATER): {
            VMValue b = pop();
            VMValue a = pop();
            if (a.isNumber() && b.isNumber()) {
                push(a.asNumber() > b.asNumber());
            } else {
                return runtimeError("Operands must be numbers.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_NOT): {
            VMValue a = pop();
            if (a.isBool()) {
                push(!a.asBool());
            } else if (a.isNil()) {
                push(true);
            } else {
                push(false);
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_BIT_NOT): {
            VMValue value = pop();
            if (value.isNumber()) {
                push(static_cast<double>(~static_cast<int32_t>(value.asNumber())));
            } else {
                return runtimeError("Operand must be a number.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_NEGATE): {
            VMValue a = pop();
            if (a.isNumber()) {
                push(-a.asNumber());
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_JUMP): {
            uint16_t offset = READ_SHORT();
            frame->ip += offset;
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_JUMP_IF_FALSE): {
            uint16_t offset = READ_SHORT();
            VMValue condition = peek(0);
            bool isFalsy = false;
            if (condition.isBool()) {
                isFalsy = !condition.asBool();
            } else if (condition.isNil()) {
                isFalsy = true;
            }

            if (isFalsy) {
                frame->ip += offset;
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_GET_LOCAL): {
            uint8_t slot = READ_BYTE();
            push(stack[frame->stackStart + slot]);
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_SET_LOCAL): {
            uint8_t slot = READ_BYTE();
            stack[frame->stackStart + slot] = peek(0);
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_GET_UPVALUE): {
            uint8_t slot = READ_BYTE();
            push(*frame->closure->upvalues[slot]->location);
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_SET_UPVALUE): {
            uint8_t slot = READ_BYTE();
            *frame->closure->upvalues[slot]->location = peek(0);
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_CLOSE_UPVALUE): {
            closeUpvalues(&stack.back());
            pop();
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_CLOSURE): {
            VMValue funcVal = READ_CONSTANT();
            auto function = funcVal.asFunction();
            auto closure = new ObjClosure(function);
            push(closure);
            for (int i = 0; i < function->upvalueCount; i++) {
                uint8_t isLocal = READ_BYTE();
                uint8_t index = READ_BYTE();
                if (isLocal) {
                    closure->upvalues.push_back(captureUpvalue(&stack[frame->stackStart + index]));
                } else {
                    closure->upvalues.push_back(frame->closure->upvalues[index]);
                }
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_LOOP): {
            uint16_t offset = READ_SHORT();
            frame->ip -= offset;
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_ITER_HAS_NEXT): {
            VMValue indexVal = pop();
            VMValue iterableVal = pop();
            if (indexVal.isNumber()) {
                int index = static_cast<int>(indexVal.asNumber());
                if (iterableVal.isList()) {
                    auto list = iterableVal.asList();
                    push(index < list->elements.size());
                    break;
                } else if (iterableVal.isString()) {
                    auto str = iterableVal.asString()->flatten();
                    push(index < utf8_length(str));
                    break;
                }
            }
            return runtimeError(std::string("Invalid operand types for iteration."));
        }
        case static_cast<uint8_t>(OpCode::OP_DUP): {
            push(peek(0));
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_DEFINE_GLOBAL): {
            std::string name = READ_CONSTANT().asString()->flatten();
            globals[name] = pop();
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_GET_GLOBAL): {
            std::string name = READ_CONSTANT().asString()->flatten();
            if (globals.find(name) == globals.end()) {
                return runtimeError(std::string("Undefined variable '") + name + "'.");
            }
            push(globals[name]);
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_SET_GLOBAL): {
            std::string name = READ_CONSTANT().asString()->flatten();
            if (globals.find(name) == globals.end()) {
                return runtimeError(std::string("Undefined variable '") + name + "'.");
            }
            globals[name] = peek(0);
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_POP): {
            pop();
            break;
        }

        case static_cast<uint8_t>(OpCode::OP_BUILD_LIST): {
            uint8_t count = READ_BYTE();
            std::vector<VMValue> elements(count);
            for (int i = count - 1; i >= 0; i--) {
                elements[i] = pop();
            }
            push(new ObjList(elements));
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_BUILD_MAP): {
            uint8_t count = READ_BYTE();
            auto map = new ObjMap();
            for (int i = count - 1; i >= 0; i--) {
                VMValue value = pop();
                VMValue key = pop();
                map->values[key] = value;
            }
            push(map);
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_INDEX_GET): {
            VMValue index = pop();
            VMValue listVal = pop();
            if (listVal.isList()) {
                auto list = listVal.asList();
                if (index.isNumber()) {
                    int i = static_cast<int>(index.asNumber());
                    if (i >= 0 && i < static_cast<int>(list->elements.size())) {
                        push(list->elements[i]);
                    } else {
                        return runtimeError(std::string("Index out of bounds."));
                    }
                } else {
                    return runtimeError(std::string("List index must be a number."));
                }
            } else if (listVal.isString()) {
                auto str = listVal.asString()->flatten();
                if (index.isNumber()) {
                    int i = static_cast<int>(index.asNumber());
                    int len = utf8_length(str);
                    if (i >= 0 && i < len) {
                        push(utf8_char_at(str, i));
                    } else {
                        return runtimeError(std::string("String index out of bounds."));
                    }
                } else {
                    return runtimeError(std::string("String index must be a number."));
                }
            } else if (listVal.isMap()) {
                auto map = listVal.asMap();
                if (map->values.count(index)) {
                    push(map->values[index]);
                } else {
                    push(nullptr); // Return nil for missing keys
                }
            } else {
                return runtimeError(std::string("Can only index into lists, maps, or strings."));
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_INDEX_SET): {
            VMValue value = pop();
            VMValue index = pop();
            VMValue listVal = pop();
            if (listVal.isList()) {
                auto list = listVal.asList();
                if (index.isNumber()) {
                    int i = static_cast<int>(index.asNumber());
                    if (i >= 0 && i < static_cast<int>(list->elements.size())) {
                        list->elements[i] = value;
                        push(value);
                    } else {
                        return runtimeError(std::string("Index out of bounds."));
                    }
                } else {
                    return runtimeError(std::string("List index must be a number."));
                }
            } else if (listVal.isMap()) {
                auto map = listVal.asMap();
                map->values[index] = value;
                push(value);
            } else {
                return runtimeError(std::string("Can only set elements in lists or maps."));
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_CLASS): {
            std::string name = READ_CONSTANT().asString()->flatten();
            push(new ObjClass(name));
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_ABSTRACT_CLASS): {
            std::string name = READ_CONSTANT().asString()->flatten();
            auto klass = new ObjClass(name);
            klass->isAbstract = true;
            push(klass);
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_INHERIT): {
            VMValue superclassVal = pop();
            VMValue subclassVal = pop();
            if (!superclassVal.isClass()) {
                return runtimeError(std::string("Superclass must be a class."));
            }
            auto subclass = subclassVal.asClass();
            auto superclass = superclassVal.asClass();
            subclass->superclass = superclass;
            for (auto const &[name, mod] : superclass->fieldModifiers) {
                subclass->fieldModifiers[name] = mod;
            }
            for (auto const &[name, method] : superclass->methods) {
                subclass->methods[name] = method;
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_MIXIN): {
            VMValue mixinVal = pop();
            VMValue targetVal = pop();
            if (!mixinVal.isClass()) {
                return runtimeError(std::string("Mixin must be a class/trait."));
            }
            auto targetClass = targetVal.asClass();
            auto mixinClass = mixinVal.asClass();
            for (auto const &[name, method] : mixinClass->methods) {
                targetClass->methods[name] = method;
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_GET_SUPER): {
            std::string methodName = READ_CONSTANT().asString()->flatten();
            VMValue superclassVal = pop();
            VMValue receiverVal = pop();
            auto superclass = superclassVal.asClass();
            auto receiver = receiverVal.asInstance();
            if (superclass->methods.count(methodName)) {
                auto method = superclass->methods[methodName];
                push(new ObjBoundMethod(receiver, method));
            } else {
                return runtimeError(std::string("Undefined superclass method '") + methodName + "'.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_METHOD): {
            std::string name = READ_CONSTANT().asString()->flatten();
            VMValue methodVal = pop();
            VMValue classVal = peek(0);
            auto method = methodVal;
            auto klass = classVal.asClass();
            klass->methods[name] = method;
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_ABSTRACT_METHOD): {
            std::string name = READ_CONSTANT().asString()->flatten();
            VMValue methodVal = pop();
            VMValue classVal = peek(0);
            auto method = methodVal;
            if (method.isClosure())
                method.asClosure()->function->isAbstract = true;
            else
                method.asNative()->isAbstract = true;
            auto klass = classVal.asClass();
            klass->methods[name] = method;
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_STATIC_METHOD): {
            std::string name = READ_CONSTANT().asString()->flatten();
            VMValue methodVal = pop();
            VMValue classVal = peek(0);
            auto klass = classVal.asClass();
            klass->statics[name] = methodVal;
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_FIELD_MODIFIER): {
            std::string name = READ_CONSTANT().asString()->flatten();
            VMAccessModifier modifier = static_cast<VMAccessModifier>(READ_BYTE());
            VMValue classVal = peek(0);
            auto klass = classVal.asClass();
            klass->fieldModifiers[name] = modifier;
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_PROPERTY_GET): {
            std::string name = READ_CONSTANT().asString()->flatten();
            VMValue instanceVal = peek(0);
            std::string callerClass = frame->closure ? frame->closure->function->enclosingClassName : "";

            if (instanceVal.isInstance()) {
                auto instance = instanceVal.asInstance();
                if (instance->fields.count(name)) {
                    VMAccessModifier mod = VMAccessModifier::PUBLIC;
                    if (instance->klass->fieldModifiers.count(name))
                        mod = instance->klass->fieldModifiers[name];
                    if (!checkAccess(mod, instance->klass, callerClass)) {
                        return runtimeError(std::string("Access error: Cannot access '") + name + "'.");
                    }
                    pop();
                    push(instance->fields[name]);
                } else if (instance->klass->methods.count(name)) {
                    auto method = instance->klass->methods[name];
                    VMAccessModifier mod = VMAccessModifier::PUBLIC;
                    if (method.isClosure()) {
                        mod = method.asClosure()->function->accessModifier;
                    }
                    if (!checkAccess(mod, instance->klass, callerClass)) {
                        return runtimeError(std::string("Access error: Cannot access method '") + name + "'.");
                    }
                    pop(); // instance
                    push(new ObjBoundMethod(instance, method));
                } else {
                    return runtimeError(std::string("Undefined property '") + name + "'.");
                }
            } else if (instanceVal.isClass()) {
                auto klass = instanceVal.asClass();
                if (klass->statics.count(name)) {
                    auto methodVal = klass->statics[name];
                    if (methodVal.isClosure()) {
                        auto func = methodVal.asClosure()->function;
                        if (!checkAccess(func->accessModifier, klass, callerClass)) {
                            return runtimeError(std::string("Access error: Cannot access static method '") + name +
                                                "'.");
                        }
                    }
                    // statics (fields) access modifier check can be added here if static fields have modifiers.
                    pop();
                    push(klass->statics[name]);
                } else {
                    return runtimeError(std::string("Undefined static property '") + name + "'.");
                }
            } else if (instanceVal.isString()) {
                if (globals.count("String")) {
                    auto klass = globals["String"].asClass();
                    if (klass->statics.count(name)) {
                        pop(); // pop string
                        push(new ObjBoundMethod(instanceVal, klass->statics[name]));
                        break;
                    }
                }
                return runtimeError(std::string("Undefined property '") + name + "' on String.");
            } else if (instanceVal.isList()) {
                if (globals.count("List")) {
                    auto klass = globals["List"].asClass();
                    if (klass->statics.count(name)) {
                        pop(); // pop list
                        push(new ObjBoundMethod(instanceVal, klass->statics[name]));
                        break;
                    }
                }
                return runtimeError(std::string("Undefined property '") + name + "' on List.");
            } else if (instanceVal.isMap()) {
                if (globals.count("Map")) {
                    auto klass = globals["Map"].asClass();
                    if (klass->statics.count(name)) {
                        pop(); // pop map
                        push(new ObjBoundMethod(instanceVal, klass->statics[name]));
                        break;
                    }
                }
                return runtimeError(std::string("Undefined property '") + name + "' on Map.");
            } else {
                return runtimeError(std::string("Only instances and classes have properties."));
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_PROPERTY_SET): {
            std::string name = READ_CONSTANT().asString()->flatten();
            VMValue value = pop();
            VMValue instanceVal = pop();
            std::string callerClass = frame->closure ? frame->closure->function->enclosingClassName : "";

            if (instanceVal.isInstance()) {
                auto instance = instanceVal.asInstance();
                VMAccessModifier mod = VMAccessModifier::PUBLIC;
                if (instance->klass->fieldModifiers.count(name))
                    mod = instance->klass->fieldModifiers[name];
                if (!checkAccess(mod, instance->klass, callerClass)) {
                    return runtimeError(std::string("Access error: Cannot set '") + name + "'.");
                }
                instance->fields[name] = value;
                push(value);
            } else if (instanceVal.isClass()) {
                auto klass = instanceVal.asClass();
                klass->statics[name] = value;
                push(value);
            } else {
                return runtimeError(std::string("Only instances and classes have properties."));
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_CALL): {
            uint8_t argCount = READ_BYTE();
            VMValue callee = peek(argCount);
            if (callee.isClosure()) {
                auto closure = callee.asClosure();
                auto function = closure->function;
                if (function->arity != -1 && (argCount < function->arity || argCount > function->maxArity)) {
                    std::string expected = function->arity == function->maxArity 
                        ? std::to_string(function->arity) 
                        : std::to_string(function->arity) + "-" + std::to_string(function->maxArity);
                    return runtimeError(std::string("Expected ") + expected +
                                        " arguments but got " + std::to_string(argCount) + ".");
                }
                while (function->maxArity != -1 && argCount < function->maxArity) {
                    push(nullptr);
                    argCount++;
                }
                if (frames.size() == 256) {
                    return runtimeError(std::string("Stack overflow."));
                }

                JitFunc nativeJitFunc = nullptr;
                auto funcPtr = function;
                if (compiledFuncs.count(funcPtr)) {
                    nativeJitFunc = compiledFuncs[funcPtr];
                } else {
                    funcPtr->callCount++;
                    if (funcPtr->callCount >= 50) {
                        nativeJitFunc = jit.compileMathFunction(function);
                        compiledFuncs[funcPtr] = nativeJitFunc; // caches nullptr if incompatible
                    }
                }

                if (nativeJitFunc) {
                    bool canRunJit = true;
                    std::vector<double> jitArgs(256, 0.0);
                    for (int i = 0; i < argCount; ++i) {
                        VMValue arg = peek(argCount - i - 1); // Top of stack is last argument
                        if (arg.isNumber()) {
                            jitArgs[i] = arg.asNumber();
                        } else {
                            canRunJit = false;
                            break;
                        }
                    }

                    if (canRunJit) {
                        double result = nativeJitFunc(this, jitArgs.data(), argCount);
                        stack.resize(stack.size() - argCount - 1);
                        push(result);
                        break; // Skip standard frame push!
                    }
                }
                CallFrame newFrame;
                newFrame.closure = closure;
                newFrame.ip = function->chunk->code.data();
                newFrame.stackStart = static_cast<int>(stack.size() - argCount - 1);
                frames.push_back(newFrame);
                frame = &frames.back();
            } else if (callee.isNative()) {
                auto native = callee.asNative();
                if (native->arity != -1 && argCount != native->arity) {
                    return runtimeError(std::string("Expected ") + std::to_string(native->arity) +
                                        " arguments but got " + std::to_string(argCount) + ".");
                }
                VMValue result = native->function(argCount, stack.data() + stack.size() - argCount);
                stack.resize(stack.size() - argCount - 1);
                push(result);
                frame = &frames.back();
            } else if (callee.isClass()) {
                auto klass = callee.asClass();
                if (klass->isAbstract) {
                    return runtimeError(std::string("Cannot instantiate abstract class '") + klass->name + "'.");
                }
                for (auto const &[name, method] : klass->methods) {
                    if (isMethodAbstract(method)) {
                        return runtimeError(std::string("Cannot instantiate class '") + klass->name +
                                            "' because abstract method '" + name + "' is not implemented.");
                    }
                }
                auto instance = new ObjInstance(klass);

                stack[stack.size() - argCount - 1] = instance;

                if (klass->methods.count("init")) {
                    auto initMethod = klass->methods["init"];
                    int minArity = getMethodMinArity(initMethod);
                    int maxArity = getMethodMaxArity(initMethod);
                    if (minArity != -1 && (argCount < minArity || argCount > maxArity)) {
                        std::string expected = minArity == maxArity 
                            ? std::to_string(minArity) 
                            : std::to_string(minArity) + "-" + std::to_string(maxArity);
                        return runtimeError(std::string("Expected ") + expected +
                                            " arguments but got " + std::to_string(argCount) + ".");
                    }
                    while (maxArity != -1 && argCount < maxArity) {
                        push(nullptr);
                        argCount++;
                    }

                    if (initMethod.isClosure()) {
                        auto closure = initMethod.asClosure();
                        auto func = closure->function;
                        CallFrame newFrame;
                        newFrame.closure = closure;
                        newFrame.ip = func->chunk->code.data();
                        newFrame.stackStart = static_cast<int>(stack.size() - argCount - 1);
                        frames.push_back(newFrame);
                        frame = &frames.back();
                    } else {
                        auto native = initMethod.asNative();
                        native->function(argCount, stack.data() + stack.size() - argCount);
                        stack.resize(stack.size() - argCount); // leave the instance on stack
                        frame = &frames.back();
                    }
                } else if (argCount != 0) {
                    return runtimeError(std::string("Expected 0 arguments but got ") + std::to_string(argCount) + ".");
                }
            } else if (callee.isBoundMethod()) {
                auto bound = callee.asBoundMethod();
                auto function = bound->method;
                if (isMethodAbstract(function)) {
                    return runtimeError(std::string("Cannot call abstract method '") + getMethodName(function) + "'.");
                }
                int minArity = getMethodMinArity(function);
                int maxArity = getMethodMaxArity(function);
                if (function.isNative()) {
                    if (!bound->receiver.isInstance()) {
                        if (minArity != -1) minArity -= 1;
                        if (maxArity != -1) maxArity -= 1;
                    }
                }
                if (minArity != -1 && (argCount < minArity || argCount > maxArity)) {
                    std::string expected = minArity == maxArity 
                        ? std::to_string(minArity) 
                        : std::to_string(minArity) + "-" + std::to_string(maxArity);
                    return runtimeError(std::string("Expected ") + expected +
                                        " arguments but got " + std::to_string(argCount) + ".");
                }
                while (maxArity != -1 && argCount < maxArity) {
                    push(nullptr);
                    argCount++;
                }
                if (frames.size() == 256) {
                    return runtimeError(std::string("Stack overflow."));
                }
                stack[stack.size() - argCount - 1] = bound->receiver;

                if (function.isClosure()) {
                    auto closure = function.asClosure();
                    auto func = closure->function;
                    CallFrame newFrame;
                    newFrame.closure = closure;
                    newFrame.ip = func->chunk->code.data();
                    newFrame.stackStart = static_cast<int>(stack.size() - argCount - 1);
                    frames.push_back(newFrame);
                    frame = &frames.back();
                } else {
                    auto native = function.asNative();
                    int passedArgCount = argCount;
                    VMValue *argsPtr;
                    if (!bound->receiver.isInstance()) {
                        passedArgCount += 1;
                        argsPtr = stack.data() + stack.size() - argCount - 1; // Primitive methods expect receiver at args[0]
                    } else {
                        argsPtr = stack.data() + stack.size() - argCount; // Instance methods expect receiver at args[-1]
                    }
                    VMValue result = native->function(passedArgCount, argsPtr);
                    stack.resize(stack.size() - argCount - 1);
                    push(result);
                    frame = &frames.back();
                }
            } else {
                return runtimeError(std::string("Can only call functions and classes."));
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_RETURN): {
            VMValue result = pop();
            closeUpvalues(&stack[frame->stackStart]);
            int newStackSize = frame->stackStart;
            frames.pop_back();

            if (frames.size() == targetFrameDepth) {
                stack.resize(newStackSize);
                push(result);
                return InterpretResult::INTERPRET_OK;
            }

            stack.resize(newStackSize);
            push(result);
            frame = &frames.back();
            break;
        }
        default:
            return runtimeError(std::string("Unknown opcode"));
        }
    }
}

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
