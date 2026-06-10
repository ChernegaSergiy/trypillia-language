#ifndef TRYPILLIA_JSON_H
#define TRYPILLIA_JSON_H

#include "../../vm/VM.h"
#include "../../symbol/SymbolTable.h"

namespace StdLib {
namespace Json {
    void registerAll(VM* vm);
    void registerSymbols(SymbolTable* scope);
}
}

#endif
