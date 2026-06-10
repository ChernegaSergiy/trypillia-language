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

    VMValue makeResultOk(VM* vm, VMValue value) {
        auto klass = std::get<std::shared_ptr<ObjClass>>(vm->globals["Result"]);
        auto instance = std::make_shared<ObjInstance>(klass);
        instance->fields["value"] = value;
        instance->fields["isOk"] = true;
        return instance;
    }

    VMValue makeResultErr(VM* vm, const std::string& message, double code) {
        auto errClass = std::get<std::shared_ptr<ObjClass>>(vm->globals["Error"]);
        auto errInst = std::make_shared<ObjInstance>(errClass);
        errInst->fields["message"] = message;
        errInst->fields["code"] = code;
        
        auto resClass = std::get<std::shared_ptr<ObjClass>>(vm->globals["Result"]);
        auto resInst = std::make_shared<ObjInstance>(resClass);
        resInst->fields["error"] = errInst;
        resInst->fields["isOk"] = false;
        return resInst;
    }
}
