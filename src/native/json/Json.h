#ifndef TRYPILLIA_JSON_H
#define TRYPILLIA_JSON_H

#include "../../symbol/SymbolTable.h"
#include "../../vm/VM.h"

namespace StdLib {
namespace Json {
void registerAll(VM *vm);
void registerSymbols(SymbolTable *scope);
} // namespace Json
} // namespace StdLib

#endif
