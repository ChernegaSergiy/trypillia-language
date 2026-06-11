#ifndef TRYPILLIA_CRYPTO_H
#define TRYPILLIA_CRYPTO_H

#include "../../vm/VM.h"
#include "../../symbol/SymbolTable.h"

namespace StdLib {
namespace CryptoModule {
    void registerAll(VM* vm);
    void registerSymbols(SymbolTable* scope);
}
}

#endif
