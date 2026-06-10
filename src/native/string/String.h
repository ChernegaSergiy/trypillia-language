#ifndef STD_STRING_H
#define STD_STRING_H

#include "../../vm/VM.h"
#include "../../symbol/SymbolTable.h"

namespace StdLib {
namespace StringModule {
    void registerSymbols(SymbolTable* scope);
    void registerAll(VM* vm);
}
}

#endif
