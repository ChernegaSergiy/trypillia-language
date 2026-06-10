#ifndef STD_TIME_H
#define STD_TIME_H

#include "../../vm/VM.h"
#include "../../symbol/SymbolTable.h"

namespace StdLib {
namespace TimeModule {
    void registerSymbols(SymbolTable* scope);
    void registerAll(VM* vm);
}
}

#endif
