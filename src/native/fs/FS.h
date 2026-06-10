#ifndef TRYPILLIA_NATIVE_FS_H
#define TRYPILLIA_NATIVE_FS_H

#include "../../vm/VM.h"
#include "../../symbol/SymbolTable.h"

namespace StdLib {
    namespace FS {
        void registerAll(VM* vm);
        void registerSymbols(SymbolTable* scope);
    }
}

#endif
