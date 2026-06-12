#ifndef TRYPILLIA_JIT_H
#define TRYPILLIA_JIT_H

#include <asmjit/x86.h>
#include <memory>
#include <variant>
#include "Chunk.h"

// Signature for JIT-compiled mathematical functions:
// double func(double* args, int argCount)
typedef double (*JitFunc)(double*, int);

class JITCompiler {
public:
    JITCompiler() {}
    ~JITCompiler() {}

    // Tries to compile a chunk into native x86_64 code.
    // Returns nullptr if compilation is unsupported (e.g., uses strings or objects)
    JitFunc compileMathFunction(std::shared_ptr<ObjFunction> function);

    asmjit::JitRuntime rt;
};

#endif // TRYPILLIA_JIT_H
