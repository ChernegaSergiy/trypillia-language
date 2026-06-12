#ifndef STD_RANDOM_H
#define STD_RANDOM_H

#include "../../symbol/SymbolTable.h"
#include "../../vm/VM.h"

namespace StdLib {
namespace RandomModule {
void registerSymbols(SymbolTable *scope);
void registerAll(VM *vm);
} // namespace RandomModule
} // namespace StdLib

#endif
