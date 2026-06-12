#ifndef TRYPILLIA_WEBSOCKET_H
#define TRYPILLIA_WEBSOCKET_H

#include "../../symbol/SymbolTable.h"
#include "../../vm/VM.h"

namespace StdLib {
namespace WebSocketModule {
void registerAll(VM *vm);
void registerSymbols(SymbolTable *scope);
} // namespace WebSocketModule
} // namespace StdLib

#endif
