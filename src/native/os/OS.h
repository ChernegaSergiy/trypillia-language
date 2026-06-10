#ifndef STD_OS_H
#define STD_OS_H

#include "../../vm/VM.h"
#include "../../symbol/SymbolTable.h"

namespace StdLib {
namespace OSModule {
    void registerSymbols(SymbolTable* scope);
    void registerAll(VM* vm);
}
}

#endif
