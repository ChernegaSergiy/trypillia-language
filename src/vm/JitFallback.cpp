#include "VM.h"
#include <iostream>

extern "C" void jit_fallback_helper(VM *vm, int opcode) {
    // This will be called by JIT to handle complex opcodes!
    // For now, it's just a stub.
    // We will implement the bridge here.
}
