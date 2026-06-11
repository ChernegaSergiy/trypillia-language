#ifndef TRYPILLIA_WORKER_H
#define TRYPILLIA_WORKER_H

#include "../../vm/VM.h"
#include "../../symbol/SymbolTable.h"

namespace StdLib {
namespace WorkerModule {
    void registerAll(VM* vm);
    void registerSymbols(SymbolTable* scope);
}
}

#endif
