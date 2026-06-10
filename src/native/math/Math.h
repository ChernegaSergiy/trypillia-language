#ifndef TRYPILLIA_NATIVE_MATH_H
#define TRYPILLIA_NATIVE_MATH_H

#include "../../vm/VM.h"
#include "../../symbol/SymbolTable.h"

namespace StdLib {
    namespace Math {
        void registerAll(VM* vm);
        void registerSymbols(SymbolTable* scope);
    }
}

#endif
