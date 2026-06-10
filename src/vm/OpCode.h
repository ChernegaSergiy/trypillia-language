#ifndef TRYPILLIA_OPCODE_H
#define TRYPILLIA_OPCODE_H

#include <cstdint>

// Опкоди (операційні коди) для нашої віртуальної машини
enum class OpCode : uint8_t {
    OP_CONSTANT,      // Завантажити константу на стек
    OP_NIL,           // Завантажити nil на стек
    OP_TRUE,          // Завантажити true на стек
    OP_FALSE,         // Завантажити false на стек
    
    OP_POP,           // Видалити верхнє значення зі стеку
    OP_GET_LOCAL,     // Отримати локальну змінну
    OP_SET_LOCAL,     // Встановити локальну змінну
    OP_GET_GLOBAL,    // Отримати глобальну змінну
    OP_DEFINE_GLOBAL, // Створити глобальну змінну
    OP_SET_GLOBAL,    // Встановити значення глобальної змінної

    // Арифметичні операції
    OP_ADD,           // Додавання (+)
    OP_SUBTRACT,      // Віднімання (-)
    OP_MULTIPLY,      // Множення (*)
    OP_DIVIDE,        // Ділення (/)
    
    // Логічні операції та порівняння
    OP_NOT,           // Логічне НЕ (!)
    OP_NEGATE,        // Арифметичне заперечення (-)
    OP_EQUAL,         // Рівність (==)
    OP_GREATER,       // Більше (>)
    OP_LESS,          // Менше (<)
    
    // Керування потоком виконання
    OP_JUMP,          // Безумовний перехід
    OP_JUMP_IF_FALSE, // Перехід, якщо умова хибна (false)
    OP_LOOP,          // Перехід назад (для циклів)
    
    // Вивід і повернення
    OP_PRINT,         // Вивід у консоль
    OP_RETURN         // Повернення з функції
};

#endif // TRYPILLIA_OPCODE_H
