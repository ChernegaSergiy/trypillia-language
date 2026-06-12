#ifndef TRYPILLIA_NATIVE_NET_H
#define TRYPILLIA_NATIVE_NET_H

#include "../../symbol/SymbolTable.h"
#include "../../vm/VM.h"

namespace StdLib {
namespace Net {
void registerAll(VM *vm);
void registerSymbols(SymbolTable *scope);
} // namespace Net
} // namespace StdLib

#endif
