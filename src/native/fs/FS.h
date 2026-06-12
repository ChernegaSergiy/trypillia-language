#ifndef TRYPILLIA_NATIVE_FS_H
#define TRYPILLIA_NATIVE_FS_H

#include "../../symbol/SymbolTable.h"
#include "../../vm/VM.h"

namespace StdLib {
namespace FS {
void registerAll(VM *vm);
void registerSymbols(SymbolTable *scope);
} // namespace FS
} // namespace StdLib

#endif
