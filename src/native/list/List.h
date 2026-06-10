#ifndef STD_LIST_H
#define STD_LIST_H

#include "../../vm/VM.h"
#include "../../symbol/SymbolTable.h"

namespace StdLib {
namespace ListModule {
    void registerSymbols(SymbolTable* scope);
    void registerAll(VM* vm);
}
}

#endif
