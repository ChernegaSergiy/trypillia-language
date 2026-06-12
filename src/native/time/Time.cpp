#include "Time.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>

namespace StdLib {
namespace TimeModule {

thread_local VM *currentVM = nullptr;

static VMValue timeNow(int argCount, VMValue *args) {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    double seconds = std::chrono::duration_cast<std::chrono::duration<double>>(duration).count();
    return seconds;
}

static VMValue timeSleep(int argCount, VMValue *args) {
    if (argCount != 1 || !std::holds_alternative<double>(args[0]))
        return nullptr;
    double ms = std::get<double>(args[0]);
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(ms)));
    return nullptr;
}

static VMValue timeFormat(int argCount, VMValue *args) {
    if (argCount != 2 || !std::holds_alternative<double>(args[0]) || !std::holds_alternative<std::string>(args[1]))
        return nullptr;
    double timestamp = std::get<double>(args[0]);
    std::string format = std::get<std::string>(args[1]);

    std::time_t time = static_cast<std::time_t>(timestamp);
    std::tm *tm_info = std::localtime(&time);

    std::ostringstream ss;
    ss << std::put_time(tm_info, format.c_str());
    return ss.str();
}

void registerAll(VM *vm) {
    currentVM = vm;
    auto timeClass = std::make_shared<ObjClass>("Time");

    timeClass->statics["now"] = std::make_shared<ObjNative>("now", 0, timeNow);
    timeClass->statics["sleep"] = std::make_shared<ObjNative>("sleep", 1, timeSleep);
    timeClass->statics["format"] = std::make_shared<ObjNative>("format", 2, timeFormat);

    vm->globals["Time"] = timeClass;
}

void registerSymbols(SymbolTable *scope) {
    Symbol sym;
    sym.name = "Time";
    sym.type = "class";
    sym.isConst = true;
    scope->define(sym);
}
} // namespace TimeModule
} // namespace StdLib
