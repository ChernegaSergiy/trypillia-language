#ifndef STD_TERMINAL_H
#define STD_TERMINAL_H

#include "../../symbol/SymbolTable.h"
#include "../../vm/VM.h"

namespace StdLib {
namespace TerminalModule {
void registerSymbols(SymbolTable *scope);
void registerAll(VM *vm);
} // namespace TerminalModule
} // namespace StdLib

#endif
