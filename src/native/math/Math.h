#ifndef TRYPILLIA_NATIVE_MATH_H
#define TRYPILLIA_NATIVE_MATH_H

#include "../../symbol/SymbolTable.h"
#include "../../vm/VM.h"

namespace StdLib {
namespace Math {
void registerAll(VM *vm);
void registerSymbols(SymbolTable *scope);
} // namespace Math
} // namespace StdLib

#endif
