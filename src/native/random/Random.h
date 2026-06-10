#ifndef STD_RANDOM_H
#define STD_RANDOM_H

#include "../../vm/VM.h"
#include "../../symbol/SymbolTable.h"

namespace StdLib {
namespace RandomModule {
    void registerSymbols(SymbolTable* scope);
    void registerAll(VM* vm);
}
}

#endif
