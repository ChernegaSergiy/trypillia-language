#include "VM.h"
#include "../native/StdLib.h"
#include <iostream>

static bool isMethodAbstract(const VMValue &method) {
    if (std::holds_alternative<std::shared_ptr<ObjClosure>>(method))
        return std::get<std::shared_ptr<ObjClosure>>(method)->function->isAbstract;
    if (std::holds_alternative<std::shared_ptr<ObjNative>>(method))
        return std::get<std::shared_ptr<ObjNative>>(method)->isAbstract;
    return false;
}

static VMAccessModifier getMethodAccessModifier(const VMValue &method) {
    if (std::holds_alternative<std::shared_ptr<ObjClosure>>(method))
        return std::get<std::shared_ptr<ObjClosure>>(method)->function->accessModifier;
    if (std::holds_alternative<std::shared_ptr<ObjNative>>(method))
        return std::get<std::shared_ptr<ObjNative>>(method)->accessModifier;
    return VMAccessModifier::PUBLIC;
}

static std::string getMethodName(const VMValue &method) {
    if (std::holds_alternative<std::shared_ptr<ObjClosure>>(method))
        return std::get<std::shared_ptr<ObjClosure>>(method)->function->name;
    if (std::holds_alternative<std::shared_ptr<ObjNative>>(method))
        return std::get<std::shared_ptr<ObjNative>>(method)->name;
    return "";
}

static int getMethodMinArity(const VMValue &method) {
    if (std::holds_alternative<std::shared_ptr<ObjClosure>>(method))
        return std::get<std::shared_ptr<ObjClosure>>(method)->function->arity;
    if (std::holds_alternative<std::shared_ptr<ObjNative>>(method))
        return std::get<std::shared_ptr<ObjNative>>(method)->arity;
    return -1;
}

static int getMethodMaxArity(const VMValue &method) {
    if (std::holds_alternative<std::shared_ptr<ObjClosure>>(method))
        return std::get<std::shared_ptr<ObjClosure>>(method)->function->maxArity;
    if (std::holds_alternative<std::shared_ptr<ObjNative>>(method))
        return std::get<std::shared_ptr<ObjNative>>(method)->arity;
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

bool checkAccess(VMAccessModifier modifier, std::shared_ptr<ObjClass> klass, const std::string &callerClass) {
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

VM::VM() {
    StdLib::registerAll(this);
}

VM::~VM() {
}

void VM::defineNative(const std::string &name, int arity, NativeFn function) {
    globals[name] = std::make_shared<ObjNative>(name, arity, function);
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
    if (!std::holds_alternative<std::shared_ptr<ObjClosure>>(closureVal))
        return nullptr;
    auto closure = std::get<std::shared_ptr<ObjClosure>>(closureVal);

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

std::shared_ptr<ObjUpvalue> VM::captureUpvalue(VMValue *local) {
    std::shared_ptr<ObjUpvalue> prevUpvalue = nullptr;
    std::shared_ptr<ObjUpvalue> upvalue = openUpvalues;
    while (upvalue != nullptr && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != nullptr && upvalue->location == local) {
        return upvalue;
    }

    auto createdUpvalue = std::make_shared<ObjUpvalue>(local);
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

InterpretResult VM::interpret(std::shared_ptr<ObjFunction> function) {
    stack.clear();
    frames.clear();
    openUpvalues = nullptr;

    auto closure = std::make_shared<ObjClosure>(function);
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
        std::shared_ptr<ObjFunction> function = frame->closure->function;
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
            if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                push(std::get<double>(a) + std::get<double>(b));
            } else if (std::holds_alternative<std::string>(a) && std::holds_alternative<std::string>(b)) {
                push(std::get<std::string>(a) + std::get<std::string>(b));
            } else if (std::holds_alternative<std::string>(a) && std::holds_alternative<double>(b)) {
                double val = std::get<double>(b);
                // Remove trailing zeros for integers
                std::string strVal = std::to_string(val);
                strVal.erase(strVal.find_last_not_of('0') + 1, std::string::npos);
                if (strVal.back() == '.')
                    strVal.pop_back();
                push(std::get<std::string>(a) + strVal);
            } else if (std::holds_alternative<double>(a) && std::holds_alternative<std::string>(b)) {
                double val = std::get<double>(a);
                std::string strVal = std::to_string(val);
                strVal.erase(strVal.find_last_not_of('0') + 1, std::string::npos);
                if (strVal.back() == '.')
                    strVal.pop_back();
                push(strVal + std::get<std::string>(b));
            } else {
                return runtimeError(std::string("Operands must be numbers or strings."));
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_SUBTRACT): {
            VMValue b = pop();
            VMValue a = pop();
            if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                push(std::get<double>(a) - std::get<double>(b));
            } else {
                return runtimeError("Operands must be numbers.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_MULTIPLY): {
            VMValue b = pop();
            VMValue a = pop();
            if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                push(std::get<double>(a) * std::get<double>(b));
            } else {
                return runtimeError("Operands must be numbers.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_DIVIDE): {
            VMValue b = pop();
            VMValue a = pop();
            if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                push(std::get<double>(a) / std::get<double>(b));
            } else {
                return runtimeError("Operands must be numbers.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_BIT_AND): {
            VMValue b = pop();
            VMValue a = pop();
            if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                push(static_cast<double>(static_cast<int32_t>(std::get<double>(a)) &
                                         static_cast<int32_t>(std::get<double>(b))));
            } else {
                return runtimeError("Operands must be numbers.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_BIT_OR): {
            VMValue b = pop();
            VMValue a = pop();
            if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                push(static_cast<double>(static_cast<int32_t>(std::get<double>(a)) |
                                         static_cast<int32_t>(std::get<double>(b))));
            } else {
                return runtimeError("Operands must be numbers.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_BIT_XOR): {
            VMValue b = pop();
            VMValue a = pop();
            if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                push(static_cast<double>(static_cast<int32_t>(std::get<double>(a)) ^
                                         static_cast<int32_t>(std::get<double>(b))));
            } else {
                return runtimeError("Operands must be numbers.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_BIT_SHIFT_LEFT): {
            VMValue b = pop();
            VMValue a = pop();
            if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                push(static_cast<double>(static_cast<int32_t>(std::get<double>(a))
                                         << static_cast<int32_t>(std::get<double>(b))));
            } else {
                return runtimeError("Operands must be numbers.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_BIT_SHIFT_RIGHT): {
            VMValue b = pop();
            VMValue a = pop();
            if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                push(static_cast<double>(static_cast<int32_t>(std::get<double>(a)) >>
                                         static_cast<int32_t>(std::get<double>(b))));
            } else {
                return runtimeError("Operands must be numbers.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_EQUAL): {
            VMValue b = pop();
            VMValue a = pop();
            if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                push(std::get<double>(a) == std::get<double>(b));
            } else if (std::holds_alternative<std::string>(a) && std::holds_alternative<std::string>(b)) {
                push(std::get<std::string>(a) == std::get<std::string>(b));
            } else if (std::holds_alternative<bool>(a) && std::holds_alternative<bool>(b)) {
                push(std::get<bool>(a) == std::get<bool>(b));
            } else {
                push(false);
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_LESS): {
            VMValue b = pop();
            VMValue a = pop();
            if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                push(std::get<double>(a) < std::get<double>(b));
            } else {
                return runtimeError("Operands must be numbers.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_GREATER): {
            VMValue b = pop();
            VMValue a = pop();
            if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                push(std::get<double>(a) > std::get<double>(b));
            } else {
                return runtimeError("Operands must be numbers.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_NOT): {
            VMValue a = pop();
            if (std::holds_alternative<bool>(a)) {
                push(!std::get<bool>(a));
            } else if (std::holds_alternative<std::nullptr_t>(a)) {
                push(true);
            } else {
                push(false);
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_BIT_NOT): {
            VMValue value = pop();
            if (std::holds_alternative<double>(value)) {
                push(static_cast<double>(~static_cast<int32_t>(std::get<double>(value))));
            } else {
                return runtimeError("Operand must be a number.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_NEGATE): {
            VMValue a = pop();
            if (std::holds_alternative<double>(a)) {
                push(-std::get<double>(a));
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
            if (std::holds_alternative<bool>(condition)) {
                isFalsy = !std::get<bool>(condition);
            } else if (std::holds_alternative<std::nullptr_t>(condition)) {
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
            auto function = std::get<std::shared_ptr<ObjFunction>>(funcVal);
            auto closure = std::make_shared<ObjClosure>(function);
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
            if (std::holds_alternative<double>(indexVal)) {
                int index = static_cast<int>(std::get<double>(indexVal));
                if (std::holds_alternative<std::shared_ptr<ObjList>>(iterableVal)) {
                    auto list = std::get<std::shared_ptr<ObjList>>(iterableVal);
                    push(index < list->elements.size());
                    break;
                } else if (std::holds_alternative<std::string>(iterableVal)) {
                    auto str = std::get<std::string>(iterableVal);
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
            std::string name = std::get<std::string>(READ_CONSTANT());
            globals[name] = pop();
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_GET_GLOBAL): {
            std::string name = std::get<std::string>(READ_CONSTANT());
            if (globals.find(name) == globals.end()) {
                return runtimeError(std::string("Undefined variable '") + name + "'.");
            }
            push(globals[name]);
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_SET_GLOBAL): {
            std::string name = std::get<std::string>(READ_CONSTANT());
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
            push(std::make_shared<ObjList>(elements));
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_BUILD_MAP): {
            uint8_t count = READ_BYTE();
            auto map = std::make_shared<ObjMap>();
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
            if (std::holds_alternative<std::shared_ptr<ObjList>>(listVal)) {
                auto list = std::get<std::shared_ptr<ObjList>>(listVal);
                if (std::holds_alternative<double>(index)) {
                    int i = static_cast<int>(std::get<double>(index));
                    if (i >= 0 && i < static_cast<int>(list->elements.size())) {
                        push(list->elements[i]);
                    } else {
                        return runtimeError(std::string("Index out of bounds."));
                    }
                } else {
                    return runtimeError(std::string("List index must be a number."));
                }
            } else if (std::holds_alternative<std::string>(listVal)) {
                auto str = std::get<std::string>(listVal);
                if (std::holds_alternative<double>(index)) {
                    int i = static_cast<int>(std::get<double>(index));
                    int len = utf8_length(str);
                    if (i >= 0 && i < len) {
                        push(utf8_char_at(str, i));
                    } else {
                        return runtimeError(std::string("String index out of bounds."));
                    }
                } else {
                    return runtimeError(std::string("String index must be a number."));
                }
            } else if (std::holds_alternative<std::shared_ptr<ObjMap>>(listVal)) {
                auto map = std::get<std::shared_ptr<ObjMap>>(listVal);
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
            if (std::holds_alternative<std::shared_ptr<ObjList>>(listVal)) {
                auto list = std::get<std::shared_ptr<ObjList>>(listVal);
                if (std::holds_alternative<double>(index)) {
                    int i = static_cast<int>(std::get<double>(index));
                    if (i >= 0 && i < static_cast<int>(list->elements.size())) {
                        list->elements[i] = value;
                        push(value);
                    } else {
                        return runtimeError(std::string("Index out of bounds."));
                    }
                } else {
                    return runtimeError(std::string("List index must be a number."));
                }
            } else if (std::holds_alternative<std::shared_ptr<ObjMap>>(listVal)) {
                auto map = std::get<std::shared_ptr<ObjMap>>(listVal);
                map->values[index] = value;
                push(value);
            } else {
                return runtimeError(std::string("Can only set elements in lists or maps."));
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_CLASS): {
            std::string name = std::get<std::string>(READ_CONSTANT());
            push(std::make_shared<ObjClass>(name));
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_ABSTRACT_CLASS): {
            std::string name = std::get<std::string>(READ_CONSTANT());
            auto klass = std::make_shared<ObjClass>(name);
            klass->isAbstract = true;
            push(klass);
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_INHERIT): {
            VMValue superclassVal = pop();
            VMValue subclassVal = pop();
            if (!std::holds_alternative<std::shared_ptr<ObjClass>>(superclassVal)) {
                return runtimeError(std::string("Superclass must be a class."));
            }
            auto subclass = std::get<std::shared_ptr<ObjClass>>(subclassVal);
            auto superclass = std::get<std::shared_ptr<ObjClass>>(superclassVal);
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
            if (!std::holds_alternative<std::shared_ptr<ObjClass>>(mixinVal)) {
                return runtimeError(std::string("Mixin must be a class/trait."));
            }
            auto targetClass = std::get<std::shared_ptr<ObjClass>>(targetVal);
            auto mixinClass = std::get<std::shared_ptr<ObjClass>>(mixinVal);
            for (auto const &[name, method] : mixinClass->methods) {
                targetClass->methods[name] = method;
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_GET_SUPER): {
            std::string methodName = std::get<std::string>(READ_CONSTANT());
            VMValue superclassVal = pop();
            VMValue receiverVal = pop();
            auto superclass = std::get<std::shared_ptr<ObjClass>>(superclassVal);
            auto receiver = std::get<std::shared_ptr<ObjInstance>>(receiverVal);
            if (superclass->methods.count(methodName)) {
                auto method = superclass->methods[methodName];
                push(std::make_shared<ObjBoundMethod>(receiver, method));
            } else {
                return runtimeError(std::string("Undefined superclass method '") + methodName + "'.");
            }
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_METHOD): {
            std::string name = std::get<std::string>(READ_CONSTANT());
            VMValue methodVal = pop();
            VMValue classVal = peek(0);
            auto method = methodVal;
            auto klass = std::get<std::shared_ptr<ObjClass>>(classVal);
            klass->methods[name] = method;
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_ABSTRACT_METHOD): {
            std::string name = std::get<std::string>(READ_CONSTANT());
            VMValue methodVal = pop();
            VMValue classVal = peek(0);
            auto method = methodVal;
            if (std::holds_alternative<std::shared_ptr<ObjClosure>>(method))
                std::get<std::shared_ptr<ObjClosure>>(method)->function->isAbstract = true;
            else
                std::get<std::shared_ptr<ObjNative>>(method)->isAbstract = true;
            auto klass = std::get<std::shared_ptr<ObjClass>>(classVal);
            klass->methods[name] = method;
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_STATIC_METHOD): {
            std::string name = std::get<std::string>(READ_CONSTANT());
            VMValue methodVal = pop();
            VMValue classVal = peek(0);
            auto klass = std::get<std::shared_ptr<ObjClass>>(classVal);
            klass->statics[name] = methodVal;
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_FIELD_MODIFIER): {
            std::string name = std::get<std::string>(READ_CONSTANT());
            VMAccessModifier modifier = static_cast<VMAccessModifier>(READ_BYTE());
            VMValue classVal = peek(0);
            auto klass = std::get<std::shared_ptr<ObjClass>>(classVal);
            klass->fieldModifiers[name] = modifier;
            break;
        }
        case static_cast<uint8_t>(OpCode::OP_PROPERTY_GET): {
            std::string name = std::get<std::string>(READ_CONSTANT());
            VMValue instanceVal = peek(0);
            std::string callerClass = frame->closure ? frame->closure->function->enclosingClassName : "";

            if (std::holds_alternative<std::shared_ptr<ObjInstance>>(instanceVal)) {
                auto instance = std::get<std::shared_ptr<ObjInstance>>(instanceVal);
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
                    if (std::holds_alternative<std::shared_ptr<ObjClosure>>(method)) {
                        mod = std::get<std::shared_ptr<ObjClosure>>(method)->function->accessModifier;
                    }
                    if (!checkAccess(mod, instance->klass, callerClass)) {
                        return runtimeError(std::string("Access error: Cannot access method '") + name + "'.");
                    }
                    pop(); // instance
                    push(std::make_shared<ObjBoundMethod>(instance, method));
                } else {
                    return runtimeError(std::string("Undefined property '") + name + "'.");
                }
            } else if (std::holds_alternative<std::shared_ptr<ObjClass>>(instanceVal)) {
                auto klass = std::get<std::shared_ptr<ObjClass>>(instanceVal);
                if (klass->statics.count(name)) {
                    auto methodVal = klass->statics[name];
                    if (std::holds_alternative<std::shared_ptr<ObjClosure>>(methodVal)) {
                        auto func = std::get<std::shared_ptr<ObjClosure>>(methodVal)->function;
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
            } else if (std::holds_alternative<std::string>(instanceVal)) {
                if (globals.count("String")) {
                    auto klass = std::get<std::shared_ptr<ObjClass>>(globals["String"]);
                    if (klass->statics.count(name)) {
                        pop(); // pop string
                        push(std::make_shared<ObjBoundMethod>(instanceVal, klass->statics[name]));
                        break;
                    }
                }
                return runtimeError(std::string("Undefined property '") + name + "' on String.");
            } else if (std::holds_alternative<std::shared_ptr<ObjList>>(instanceVal)) {
                if (globals.count("List")) {
                    auto klass = std::get<std::shared_ptr<ObjClass>>(globals["List"]);
                    if (klass->statics.count(name)) {
                        pop(); // pop list
                        push(std::make_shared<ObjBoundMethod>(instanceVal, klass->statics[name]));
                        break;
                    }
                }
                return runtimeError(std::string("Undefined property '") + name + "' on List.");
            } else if (std::holds_alternative<std::shared_ptr<ObjMap>>(instanceVal)) {
                if (globals.count("Map")) {
                    auto klass = std::get<std::shared_ptr<ObjClass>>(globals["Map"]);
                    if (klass->statics.count(name)) {
                        pop(); // pop map
                        push(std::make_shared<ObjBoundMethod>(instanceVal, klass->statics[name]));
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
            std::string name = std::get<std::string>(READ_CONSTANT());
            VMValue value = pop();
            VMValue instanceVal = pop();
            std::string callerClass = frame->closure ? frame->closure->function->enclosingClassName : "";

            if (std::holds_alternative<std::shared_ptr<ObjInstance>>(instanceVal)) {
                auto instance = std::get<std::shared_ptr<ObjInstance>>(instanceVal);
                VMAccessModifier mod = VMAccessModifier::PUBLIC;
                if (instance->klass->fieldModifiers.count(name))
                    mod = instance->klass->fieldModifiers[name];
                if (!checkAccess(mod, instance->klass, callerClass)) {
                    return runtimeError(std::string("Access error: Cannot set '") + name + "'.");
                }
                instance->fields[name] = value;
                push(value);
            } else if (std::holds_alternative<std::shared_ptr<ObjClass>>(instanceVal)) {
                auto klass = std::get<std::shared_ptr<ObjClass>>(instanceVal);
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
            if (std::holds_alternative<std::shared_ptr<ObjClosure>>(callee)) {
                auto closure = std::get<std::shared_ptr<ObjClosure>>(callee);
                auto function = closure->function;
                if (argCount != function->arity) {
                    return runtimeError(std::string("Expected ") + std::to_string(function->arity) +
                                        " arguments but got " + std::to_string(argCount) + ".");
                }
                if (frames.size() == 256) {
                    return runtimeError(std::string("Stack overflow."));
                }
                CallFrame newFrame;
                newFrame.closure = closure;
                newFrame.ip = function->chunk->code.data();
                newFrame.stackStart = static_cast<int>(stack.size() - argCount - 1);
                frames.push_back(newFrame);
                frame = &frames.back();
            } else if (std::holds_alternative<std::shared_ptr<ObjNative>>(callee)) {
                auto native = std::get<std::shared_ptr<ObjNative>>(callee);
                if (native->arity != -1 && argCount != native->arity) {
                    return runtimeError(std::string("Expected ") + std::to_string(native->arity) +
                                        " arguments but got " + std::to_string(argCount) + ".");
                }
                VMValue result = native->function(argCount, stack.data() + stack.size() - argCount);
                stack.resize(stack.size() - argCount - 1);
                push(result);
                frame = &frames.back();
            } else if (std::holds_alternative<std::shared_ptr<ObjClass>>(callee)) {
                auto klass = std::get<std::shared_ptr<ObjClass>>(callee);
                if (klass->isAbstract) {
                    return runtimeError(std::string("Cannot instantiate abstract class '") + klass->name + "'.");
                }
                for (auto const &[name, method] : klass->methods) {
                    if (isMethodAbstract(method)) {
                        return runtimeError(std::string("Cannot instantiate class '") + klass->name +
                                            "' because abstract method '" + name + "' is not implemented.");
                    }
                }
                auto instance = std::make_shared<ObjInstance>(klass);

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

                    if (std::holds_alternative<std::shared_ptr<ObjClosure>>(initMethod)) {
                        auto closure = std::get<std::shared_ptr<ObjClosure>>(initMethod);
                        auto func = closure->function;
                        CallFrame newFrame;
                        newFrame.closure = closure;
                        newFrame.ip = func->chunk->code.data();
                        newFrame.stackStart = static_cast<int>(stack.size() - argCount - 1);
                        frames.push_back(newFrame);
                        frame = &frames.back();
                    } else {
                        auto native = std::get<std::shared_ptr<ObjNative>>(initMethod);
                        native->function(argCount, stack.data() + stack.size() - argCount);
                        stack.resize(stack.size() - argCount); // leave the instance on stack
                        frame = &frames.back();
                    }
                } else if (argCount != 0) {
                    return runtimeError(std::string("Expected 0 arguments but got ") + std::to_string(argCount) + ".");
                }
            } else if (std::holds_alternative<std::shared_ptr<ObjBoundMethod>>(callee)) {
                auto bound = std::get<std::shared_ptr<ObjBoundMethod>>(callee);
                auto function = bound->method;
                if (isMethodAbstract(function)) {
                    return runtimeError(std::string("Cannot call abstract method '") + getMethodName(function) + "'.");
                }
                int minArity = getMethodMinArity(function);
                int maxArity = getMethodMaxArity(function);
                if (std::holds_alternative<std::shared_ptr<ObjNative>>(function)) {
                    if (!std::holds_alternative<std::shared_ptr<ObjInstance>>(bound->receiver)) {
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

                if (std::holds_alternative<std::shared_ptr<ObjClosure>>(function)) {
                    auto closure = std::get<std::shared_ptr<ObjClosure>>(function);
                    auto func = closure->function;
                    CallFrame newFrame;
                    newFrame.closure = closure;
                    newFrame.ip = func->chunk->code.data();
                    newFrame.stackStart = static_cast<int>(stack.size() - argCount - 1);
                    frames.push_back(newFrame);
                    frame = &frames.back();
                } else {
                    auto native = std::get<std::shared_ptr<ObjNative>>(function);
                    int passedArgCount = argCount;
                    VMValue *argsPtr;
                    if (!std::holds_alternative<std::shared_ptr<ObjInstance>>(bound->receiver)) {
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
