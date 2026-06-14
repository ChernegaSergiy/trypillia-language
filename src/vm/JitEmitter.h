#pragma once
#include <cstdint>
#include <stddef.h>

typedef double (*JitFunc)(double*, int);

// Абстрактний базовий клас для генерації машинного коду
class JitEmitter {
public:
    virtual ~JitEmitter() = default;

    // Керування пам'яттю та викликами
    virtual void emitPrologue(int maxLocals) = 0;
    virtual void emitEpilogue(int maxLocals) = 0;

    // Робота зі стеком (stackOffset - це відносний індекс у нашому віртуальному стеку)
    virtual void emitLoadConst(int stackOffset, double value) = 0;
    virtual void emitGetLocal(int stackOffset, int localSlot) = 0;
    virtual void emitSetLocal(int localSlot, int stackOffset) = 0;
    virtual void emitMove(int targetOffset, int srcOffset) = 0;

    // Арифметика
    virtual void emitAdd(int targetOffset, int srcOffset) = 0;
    virtual void emitSub(int targetOffset, int srcOffset) = 0;
    virtual void emitMul(int targetOffset, int srcOffset) = 0;
    virtual void emitDiv(int targetOffset, int srcOffset) = 0;

    // Порівняння (записують результат в 1 (true) або 0 (false))
    virtual void emitCmpEq(int targetOffset, int srcOffset) = 0;
    virtual void emitCmpNe(int targetOffset, int srcOffset) = 0;
    virtual void emitCmpLt(int targetOffset, int srcOffset) = 0;
    virtual void emitCmpLe(int targetOffset, int srcOffset) = 0;
    virtual void emitCmpGt(int targetOffset, int srcOffset) = 0;
    virtual void emitCmpGe(int targetOffset, int srcOffset) = 0;

    // Керування потоком виконання (Control Flow)
    virtual void bindLabel(size_t byteCodeIndex) = 0;
    virtual void emitJump(size_t targetByteCodeIndex) = 0;
    virtual void emitJumpIfFalse(int stackOffset, size_t targetByteCodeIndex) = 0;

    // Фіналізація та повернення вказівника на функцію
    virtual JitFunc finalize() = 0;
};
