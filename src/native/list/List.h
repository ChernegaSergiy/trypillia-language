#ifndef STD_LIST_H
#define STD_LIST_H

#include "../../symbol/SymbolTable.h"
#include "../../vm/VM.h"

namespace StdLib {
namespace ListModule {
void registerSymbols(SymbolTable *scope);
void registerAll(VM *vm);
} // namespace ListModule
} // namespace StdLib

#endif
