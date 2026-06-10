#include "core/Core.h"
#include "math/Math.h"
#include "fs/FS.h"
#include "net/Net.h"
#include "json/Json.h"
#include "string/String.h"
#include "time/Time.h"
#include "list/List.h"
#include "os/OS.h"
#include "random/Random.h"
#include "terminal/Terminal.h"

namespace StdLib {
    void registerAll(VM* vm) {
        Core::registerAll(vm);
        Math::registerAll(vm);
        FS::registerAll(vm);
        Net::registerAll(vm);
        Json::registerAll(vm);
        StringModule::registerAll(vm);
        TimeModule::registerAll(vm);
        ListModule::registerAll(vm);
        OSModule::registerAll(vm);
        RandomModule::registerAll(vm);
        TerminalModule::registerAll(vm);
    }

    void registerSymbols(SymbolTable* scope) {
        Core::registerSymbols(scope);
        Math::registerSymbols(scope);
        FS::registerSymbols(scope);
        Net::registerSymbols(scope);
        Json::registerSymbols(scope);
        StringModule::registerSymbols(scope);
        TimeModule::registerSymbols(scope);
        ListModule::registerSymbols(scope);
        OSModule::registerSymbols(scope);
        RandomModule::registerSymbols(scope);
        TerminalModule::registerSymbols(scope);
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
