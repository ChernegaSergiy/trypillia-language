#ifndef TRYPILLIA_WEBSOCKET_H
#define TRYPILLIA_WEBSOCKET_H

#include "../../vm/VM.h"
#include "../../symbol/SymbolTable.h"

namespace StdLib {
namespace WebSocketModule {
    void registerAll(VM* vm);
    void registerSymbols(SymbolTable* scope);
}
}

#endif
