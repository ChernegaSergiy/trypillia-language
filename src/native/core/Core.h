#ifndef TRYPILLIA_NATIVE_CORE_H
#define TRYPILLIA_NATIVE_CORE_H

#include "../../symbol/SymbolTable.h"
#include "../../vm/VM.h"

namespace StdLib {
namespace Core {
void registerAll(VM *vm);
void registerSymbols(SymbolTable *scope);
} // namespace Core
} // namespace StdLib

#endif
