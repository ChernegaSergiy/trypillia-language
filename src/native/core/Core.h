#ifndef TRYPILLIA_NATIVE_CORE_H
#define TRYPILLIA_NATIVE_CORE_H

#include "../../vm/VM.h"
#include "../../symbol/SymbolTable.h"

namespace StdLib {
    namespace Core {
        void registerAll(VM* vm);
        void registerSymbols(SymbolTable* scope);
    }
}

#endif
