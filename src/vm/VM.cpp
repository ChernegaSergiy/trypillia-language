#include "VM.h"
#include <iostream>
#include <ctime>

VM::VM() {
    auto printNative = [](int argCount, VMValue* args) -> VMValue {
        for (int i = 0; i < argCount; i++) {
            if (std::holds_alternative<double>(args[i])) {
                std::cout << std::get<double>(args[i]);
            } else if (std::holds_alternative<std::string>(args[i])) {
                std::cout << std::get<std::string>(args[i]);
            } else if (std::holds_alternative<bool>(args[i])) {
                std::cout << (std::get<bool>(args[i]) ? "true" : "false");
            } else if (std::holds_alternative<std::shared_ptr<ObjList>>(args[i])) {
                std::cout << "[list]";
            } else if (std::holds_alternative<std::shared_ptr<ObjClass>>(args[i])) {
                std::cout << "<class " << std::get<std::shared_ptr<ObjClass>>(args[i])->name << ">";
            } else if (std::holds_alternative<std::shared_ptr<ObjInstance>>(args[i])) {
                std::cout << "<instance of " << std::get<std::shared_ptr<ObjInstance>>(args[i])->klass->name << ">";
            } else if (std::holds_alternative<std::shared_ptr<ObjBoundMethod>>(args[i])) {
                std::cout << "<bound method " << std::get<std::shared_ptr<ObjBoundMethod>>(args[i])->method->name << ">";
            } else if (std::holds_alternative<std::nullptr_t>(args[i])) {
                std::cout << "nil";
            }
            if (i < argCount - 1) std::cout << " ";
        }
        std::cout << std::endl;
        return nullptr;
    };
    
    auto clockNative = [](int argCount, VMValue* args) -> VMValue {
        return (double)clock() / CLOCKS_PER_SEC;
    };
    
    auto lenNative = [](int argCount, VMValue* args) -> VMValue {
        if (argCount != 1) return nullptr;
        if (std::holds_alternative<std::shared_ptr<ObjList>>(args[0])) {
            return (double)std::get<std::shared_ptr<ObjList>>(args[0])->elements.size();
        } else if (std::holds_alternative<std::string>(args[0])) {
            return (double)std::get<std::string>(args[0]).length();
        }
        return (double)0;
    };
    
    auto pushNative = [](int argCount, VMValue* args) -> VMValue {
        if (argCount != 2) return nullptr;
        if (std::holds_alternative<std::shared_ptr<ObjList>>(args[0])) {
            auto list = std::get<std::shared_ptr<ObjList>>(args[0]);
            list->elements.push_back(args[1]);
            return args[1];
        }
        return nullptr;
    };

    auto substringNative = [](int argCount, VMValue* args) -> VMValue {
        if (argCount != 3) return nullptr;
        if (std::holds_alternative<std::string>(args[0]) && std::holds_alternative<double>(args[1]) && std::holds_alternative<double>(args[2])) {
            std::string str = std::get<std::string>(args[0]);
            int start = std::get<double>(args[1]);
            int len = std::get<double>(args[2]);
            if (start >= 0 && start < str.length()) {
                return str.substr(start, len);
            }
        }
        return nullptr;
    };
    
    auto toUpperNative = [](int argCount, VMValue* args) -> VMValue {
        if (argCount != 1) return nullptr;
        if (std::holds_alternative<std::string>(args[0])) {
            std::string str = std::get<std::string>(args[0]);
            for (char& c : str) c = std::toupper(c);
            return str;
        }
        return nullptr;
    };
    
    auto toLowerNative = [](int argCount, VMValue* args) -> VMValue {
        if (argCount != 1) return nullptr;
        if (std::holds_alternative<std::string>(args[0])) {
            std::string str = std::get<std::string>(args[0]);
            for (char& c : str) c = std::tolower(c);
            return str;
        }
        return nullptr;
    };

    defineNative("print", -1, printNative);
    defineNative("clock", 0, clockNative);
    defineNative("len", 1, lenNative);
    defineNative("push", 2, pushNative);
    defineNative("substring", 3, substringNative);
    defineNative("toUpper", 1, toUpperNative);
    defineNative("toLower", 1, toLowerNative);
}

VM::~VM() {
}

void VM::defineNative(const std::string& name, int arity, NativeFn function) {
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

InterpretResult VM::interpret(std::shared_ptr<ObjFunction> function) {
    stack.clear();
    frames.clear();
    
    push(function);
    CallFrame frame;
    frame.function = function;
    frame.ip = function->chunk->code.data();
    frame.stackStart = 0;
    frames.push_back(frame);
    
    return run();
}

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() (frame->function->chunk->constants[READ_BYTE()])
#define READ_SHORT() \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

InterpretResult VM::run() {
    CallFrame* frame = &frames.back();
    
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
                } else {
                    std::cerr << "Operands must be two numbers or two strings." << std::endl;
                    return InterpretResult::INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_SUBTRACT): {
                VMValue b = pop();
                VMValue a = pop();
                if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                    push(std::get<double>(a) - std::get<double>(b));
                }
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_MULTIPLY): {
                VMValue b = pop();
                VMValue a = pop();
                if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                    push(std::get<double>(a) * std::get<double>(b));
                }
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_DIVIDE): {
                VMValue b = pop();
                VMValue a = pop();
                if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                    push(std::get<double>(a) / std::get<double>(b));
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
                }
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_GREATER): {
                VMValue b = pop();
                VMValue a = pop();
                if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                    push(std::get<double>(a) > std::get<double>(b));
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
            case static_cast<uint8_t>(OpCode::OP_LOOP): {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
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
                    std::cerr << "Undefined variable '" << name << "'." << std::endl;
                    return InterpretResult::INTERPRET_RUNTIME_ERROR;
                }
                push(globals[name]);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_SET_GLOBAL): {
                std::string name = std::get<std::string>(READ_CONSTANT());
                if (globals.find(name) == globals.end()) {
                    std::cerr << "Undefined variable '" << name << "'." << std::endl;
                    return InterpretResult::INTERPRET_RUNTIME_ERROR;
                }
                globals[name] = peek(0);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_POP): {
                pop();
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_PRINT): {
                VMValue value = pop();
                if (std::holds_alternative<double>(value)) {
                    std::cout << std::get<double>(value) << std::endl;
                } else if (std::holds_alternative<std::string>(value)) {
                    std::cout << std::get<std::string>(value) << std::endl;
                } else if (std::holds_alternative<bool>(value)) {
                    std::cout << (std::get<bool>(value) ? "true" : "false") << std::endl;
                } else if (std::holds_alternative<std::nullptr_t>(value)) {
                    std::cout << "nil" << std::endl;
                }
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
            case static_cast<uint8_t>(OpCode::OP_INDEX_GET): {
                VMValue index = pop();
                VMValue listVal = pop();
                if (std::holds_alternative<std::shared_ptr<ObjList>>(listVal)) {
                    auto list = std::get<std::shared_ptr<ObjList>>(listVal);
                    if (std::holds_alternative<double>(index)) {
                        int i = std::get<double>(index);
                        if (i >= 0 && i < list->elements.size()) {
                            push(list->elements[i]);
                        } else {
                            std::cerr << "Index out of bounds." << std::endl;
                            return InterpretResult::INTERPRET_RUNTIME_ERROR;
                        }
                    } else {
                        std::cerr << "List index must be a number." << std::endl;
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                } else {
                    std::cerr << "Can only index into lists." << std::endl;
                    return InterpretResult::INTERPRET_RUNTIME_ERROR;
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
                        int i = std::get<double>(index);
                        if (i >= 0 && i < list->elements.size()) {
                            list->elements[i] = value;
                            push(value);
                        } else {
                            std::cerr << "Index out of bounds." << std::endl;
                            return InterpretResult::INTERPRET_RUNTIME_ERROR;
                        }
                    } else {
                        std::cerr << "List index must be a number." << std::endl;
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                } else {
                    std::cerr << "Can only index into lists." << std::endl;
                    return InterpretResult::INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_CLASS): {
                std::string name = std::get<std::string>(READ_CONSTANT());
                push(std::make_shared<ObjClass>(name));
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_INHERIT): {
                VMValue superclassVal = pop();
                VMValue subclassVal = pop();
                if (!std::holds_alternative<std::shared_ptr<ObjClass>>(superclassVal)) {
                    std::cerr << "Superclass must be a class." << std::endl;
                    return InterpretResult::INTERPRET_RUNTIME_ERROR;
                }
                auto subclass = std::get<std::shared_ptr<ObjClass>>(subclassVal);
                auto superclass = std::get<std::shared_ptr<ObjClass>>(superclassVal);
                subclass->superclass = superclass;
                for (auto const& [name, method] : superclass->methods) {
                    subclass->methods[name] = method;
                }
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_METHOD): {
                std::string name = std::get<std::string>(READ_CONSTANT());
                VMValue methodVal = pop();
                VMValue classVal = peek(0);
                auto klass = std::get<std::shared_ptr<ObjClass>>(classVal);
                klass->methods[name] = std::get<std::shared_ptr<ObjFunction>>(methodVal);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_PROPERTY_GET): {
                std::string name = std::get<std::string>(READ_CONSTANT());
                VMValue instanceVal = peek(0);
                if (std::holds_alternative<std::shared_ptr<ObjInstance>>(instanceVal)) {
                    auto instance = std::get<std::shared_ptr<ObjInstance>>(instanceVal);
                    if (instance->fields.count(name)) {
                        pop();
                        push(instance->fields[name]);
                    } else if (instance->klass->methods.count(name)) {
                        auto method = instance->klass->methods[name];
                        pop();
                        push(std::make_shared<ObjBoundMethod>(instance, method));
                    } else {
                        std::cerr << "Undefined property '" << name << "'." << std::endl;
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                } else {
                    std::cerr << "Only instances have properties." << std::endl;
                    return InterpretResult::INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_PROPERTY_SET): {
                std::string name = std::get<std::string>(READ_CONSTANT());
                VMValue value = pop();
                VMValue instanceVal = pop();
                if (std::holds_alternative<std::shared_ptr<ObjInstance>>(instanceVal)) {
                    auto instance = std::get<std::shared_ptr<ObjInstance>>(instanceVal);
                    instance->fields[name] = value;
                    push(value);
                } else {
                    std::cerr << "Only instances have properties." << std::endl;
                    return InterpretResult::INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_CALL): {
                uint8_t argCount = READ_BYTE();
                VMValue callee = peek(argCount);
                if (std::holds_alternative<std::shared_ptr<ObjFunction>>(callee)) {
                    auto function = std::get<std::shared_ptr<ObjFunction>>(callee);
                    if (argCount != function->arity) {
                        std::cerr << "Expected " << function->arity << " arguments but got " << (int)argCount << "." << std::endl;
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                    if (frames.size() == 256) {
                        std::cerr << "Stack overflow." << std::endl;
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                    CallFrame newFrame;
                    newFrame.function = function;
                    newFrame.ip = function->chunk->code.data();
                    newFrame.stackStart = stack.size() - argCount - 1;
                    frames.push_back(newFrame);
                    frame = &frames.back();
                } else if (std::holds_alternative<std::shared_ptr<ObjNative>>(callee)) {
                    auto native = std::get<std::shared_ptr<ObjNative>>(callee);
                    if (native->arity != -1 && argCount != native->arity) {
                        std::cerr << "Expected " << native->arity << " arguments but got " << (int)argCount << "." << std::endl;
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                    VMValue result = native->function(argCount, &stack[stack.size() - argCount]);
                    stack.resize(stack.size() - argCount - 1);
                    push(result);
                } else if (std::holds_alternative<std::shared_ptr<ObjClass>>(callee)) {
                    auto klass = std::get<std::shared_ptr<ObjClass>>(callee);
                    auto instance = std::make_shared<ObjInstance>(klass);
                    stack.resize(stack.size() - argCount - 1);
                    push(instance);
                } else if (std::holds_alternative<std::shared_ptr<ObjBoundMethod>>(callee)) {
                    auto bound = std::get<std::shared_ptr<ObjBoundMethod>>(callee);
                    auto function = bound->method;
                    if (argCount != function->arity) {
                        std::cerr << "Expected " << function->arity << " arguments but got " << (int)argCount << "." << std::endl;
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                    if (frames.size() == 256) {
                        std::cerr << "Stack overflow." << std::endl;
                        return InterpretResult::INTERPRET_RUNTIME_ERROR;
                    }
                    stack[stack.size() - argCount - 1] = bound->receiver;
                    CallFrame newFrame;
                    newFrame.function = function;
                    newFrame.ip = function->chunk->code.data();
                    newFrame.stackStart = stack.size() - argCount - 1;
                    frames.push_back(newFrame);
                    frame = &frames.back();
                } else {
                    std::cerr << "Can only call functions and classes." << std::endl;
                    return InterpretResult::INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_RETURN): {
                VMValue result = pop();
                int newStackSize = frame->stackStart;
                frames.pop_back();
                if (frames.empty()) {
                    pop();
                    return InterpretResult::INTERPRET_OK;
                }
                stack.resize(newStackSize);
                push(result);
                frame = &frames.back();
                break;
            }
            default:
                std::cerr << "Unknown opcode" << std::endl;
                return InterpretResult::INTERPRET_RUNTIME_ERROR;
        }
    }
}

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
