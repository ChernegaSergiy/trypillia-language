#ifndef STD_OS_H
#define STD_OS_H

#include "../../vm/VM.h"
#include "../../symbol/SymbolTable.h"

namespace StdLib {
namespace OSModule {
    extern std::vector<std::string> commandLineArgs;
    void registerSymbols(SymbolTable* scope);
    void registerAll(VM* vm);
}
}

#endif
