#ifndef TRYPILLIA_STDLIB_H
#define TRYPILLIA_STDLIB_H

#include "../vm/VM.h"
#include "../symbol/SymbolTable.h"

namespace StdLib {
    // Registers actual functions and objects in the VM
    void registerAll(VM* vm);
    
    // Registers symbol names in the compiler's semantic analyzer scope
    void registerSymbols(SymbolTable* scope);

    // Helpers for returning Result objects
    VMValue makeResultOk(VM* vm, VMValue value);
    VMValue makeResultErr(VM* vm, const std::string& message, double code = 0.0);
}

#endif
