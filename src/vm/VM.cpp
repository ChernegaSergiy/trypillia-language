#include "VM.h"
#include <iostream>

VM::VM() {
    resetStack();
}

VM::~VM() {
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

InterpretResult VM::interpret(Chunk* chunk) {
    this->chunk = chunk;
    this->ip = chunk->code.data();
    return run();
}

#define READ_BYTE() (*ip++)
#define READ_CONSTANT() (chunk->constants[READ_BYTE()])
#define READ_SHORT() \
    (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))

InterpretResult VM::run() {
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
            case static_cast<uint8_t>(OpCode::OP_EQUAL): {
                VMValue b = pop();
                VMValue a = pop();
                if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                    push(std::get<double>(a) == std::get<double>(b));
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
                ip += offset;
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
                    ip += offset;
                }
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_LOOP): {
                uint16_t offset = READ_SHORT();
                ip -= offset;
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
            case static_cast<uint8_t>(OpCode::OP_RETURN): {
                if (!stack.empty()) {
                    VMValue result = pop();
                    if (std::holds_alternative<double>(result)) {
                        std::cout << "Result: " << std::get<double>(result) << std::endl;
                    }
                }
                return InterpretResult::INTERPRET_OK;
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
