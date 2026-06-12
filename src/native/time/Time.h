#ifndef STD_TIME_H
#define STD_TIME_H

#include "../../symbol/SymbolTable.h"
#include "../../vm/VM.h"

namespace StdLib {
namespace TimeModule {
void registerSymbols(SymbolTable *scope);
void registerAll(VM *vm);
} // namespace TimeModule
} // namespace StdLib

#endif
