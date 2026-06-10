#ifndef TRYPILLIA_NATIVE_NET_H
#define TRYPILLIA_NATIVE_NET_H

#include "../../vm/VM.h"
#include "../../symbol/SymbolTable.h"

namespace StdLib {
    namespace Net {
        void registerAll(VM* vm);
        void registerSymbols(SymbolTable* scope);
    }
}

#endif
