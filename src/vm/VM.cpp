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

InterpretResult VM::interpret(Chunk* chunk) {
    this->chunk = chunk;
    this->ip = chunk->code.data();
    return run();
}

// Допоміжна макрос-функція для читання наступного байту (опкоду)
#define READ_BYTE() (*ip++)
// Читання константи з масиву констант за індексом
#define READ_CONSTANT() (chunk->constants[READ_BYTE()])

InterpretResult VM::run() {
    for (;;) {
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case static_cast<uint8_t>(OpCode::OP_CONSTANT): {
                VMValue constant = READ_CONSTANT();
                push(constant);
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
                }
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_RETURN): {
                // Тимчасово просто виводимо результат
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
