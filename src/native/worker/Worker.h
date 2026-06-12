#ifndef TRYPILLIA_WORKER_H
#define TRYPILLIA_WORKER_H

#include "../../symbol/SymbolTable.h"
#include "../../vm/VM.h"

namespace StdLib {
namespace WorkerModule {
void registerAll(VM *vm);
void registerSymbols(SymbolTable *scope);
} // namespace WorkerModule
} // namespace StdLib

#endif
