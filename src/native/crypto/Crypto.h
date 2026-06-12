#ifndef TRYPILLIA_CRYPTO_H
#define TRYPILLIA_CRYPTO_H

#include "../../symbol/SymbolTable.h"
#include "../../vm/VM.h"

namespace StdLib {
namespace CryptoModule {
void registerAll(VM *vm);
void registerSymbols(SymbolTable *scope);
} // namespace CryptoModule
} // namespace StdLib

#endif
