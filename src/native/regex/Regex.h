#ifndef STD_REGEX_H
#define STD_REGEX_H

#include "../../symbol/SymbolTable.h"
#include "../../vm/VM.h"

namespace StdLib {
namespace RegexModule {
void registerSymbols(SymbolTable *scope);
void registerAll(VM *vm);
} // namespace RegexModule
} // namespace StdLib

#endif
