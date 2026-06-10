#include "core/Core.h"
#include "math/Math.h"
#include "fs/FS.h"
#include "net/Net.h"
#include "json/Json.h"

namespace StdLib {
    void registerAll(VM* vm) {
        Core::registerAll(vm);
        Math::registerAll(vm);
        FS::registerAll(vm);
        Net::registerAll(vm);
        Json::registerAll(vm);
    }

    void registerSymbols(SymbolTable* scope) {
        Core::registerSymbols(scope);
        Math::registerSymbols(scope);
        FS::registerSymbols(scope);
        Net::registerSymbols(scope);
        Json::registerSymbols(scope);
    }
}
