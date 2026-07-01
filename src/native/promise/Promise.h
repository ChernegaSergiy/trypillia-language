#ifndef STD_PROMISE_H
#define STD_PROMISE_H

#include "../../symbol/SymbolTable.h"
#include "../../vm/VM.h"

namespace StdLib {
namespace PromiseModule {
void registerSymbols(SymbolTable *scope);
void registerAll(VM *vm);
} // namespace PromiseModule
} // namespace StdLib

#endif