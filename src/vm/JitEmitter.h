#pragma once
#include <cstdint>
#include <stddef.h>

typedef double (*JitFunc)(void*, double*, int);

// Abstract base class for machine code generation
class JitEmitter {
public:
    virtual ~JitEmitter() = default;

    // Memory and invocation management
    virtual void emitPrologue(int maxLocals) = 0;
    virtual void emitEpilogue(int maxLocals) = 0;

    // Stack operations (stackOffset is a relative index in our virtual stack)
    virtual void emitLoadConst(int stackOffset, double value) = 0;
    virtual void emitGetLocal(int stackOffset, int localSlot) = 0;
    virtual void emitSetLocal(int localSlot, int stackOffset) = 0;
    virtual void emitMove(int targetOffset, int srcOffset) = 0;

    // Arithmetic operations
    virtual void emitAdd(int targetOffset, int srcOffset) = 0;
    virtual void emitSub(int targetOffset, int srcOffset) = 0;
    virtual void emitMul(int targetOffset, int srcOffset) = 0;
    virtual void emitDiv(int targetOffset, int srcOffset) = 0;

    // Bitwise operations
    virtual void emitBitAnd(int targetOffset, int srcOffset) = 0;
    virtual void emitBitOr(int targetOffset, int srcOffset) = 0;
    virtual void emitBitXor(int targetOffset, int srcOffset) = 0;
    virtual void emitBitNot(int targetOffset) = 0;
    virtual void emitBitShl(int targetOffset, int srcOffset) = 0;
    virtual void emitBitShr(int targetOffset, int srcOffset) = 0;

    // Comparisons (write result as 1.0 (true) or 0.0 (false))
    virtual void emitCmpEq(int targetOffset, int srcOffset) = 0;
    virtual void emitCmpNe(int targetOffset, int srcOffset) = 0;
    virtual void emitCmpLt(int targetOffset, int srcOffset) = 0;
    virtual void emitCmpLe(int targetOffset, int srcOffset) = 0;
    virtual void emitCmpGt(int targetOffset, int srcOffset) = 0;
    virtual void emitCmpGe(int targetOffset, int srcOffset) = 0;

    // Unary operations
    virtual void emitNot(int targetOffset) = 0;
    virtual void emitNegate(int targetOffset) = 0;

    // Calls
    virtual void emitCallGlobal(const std::string& name, int targetOffset, int argCount) = 0;

    // Control Flow
    virtual void bindLabel(size_t byteCodeIndex) = 0;
    virtual void emitJump(size_t targetByteCodeIndex) = 0;
    virtual void emitJumpIfFalse(int stackOffset, size_t targetByteCodeIndex) = 0;

    // Finalize and return function pointer
    virtual JitFunc finalize() = 0;
};
