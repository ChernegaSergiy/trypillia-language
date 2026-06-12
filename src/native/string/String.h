#ifndef STD_STRING_H
#define STD_STRING_H

#include "../../symbol/SymbolTable.h"
#include "../../vm/VM.h"

namespace StdLib {
namespace StringModule {
void registerSymbols(SymbolTable *scope);
void registerAll(VM *vm);
} // namespace StringModule
} // namespace StdLib

#endif
