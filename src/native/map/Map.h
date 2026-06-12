#ifndef STD_MAP_H
#define STD_MAP_H

#include "../../symbol/SymbolTable.h"
#include "../../vm/VM.h"

namespace StdLib {
namespace MapModule {
void registerSymbols(SymbolTable *scope);
void registerAll(VM *vm);
} // namespace MapModule
} // namespace StdLib

#endif
