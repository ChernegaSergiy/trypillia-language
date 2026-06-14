#include "Value.h"
#include <string>

// To support ObjString instantiation, we might need a full definition of ObjString.
// If it's defined in VM.h or somewhere else, we might need to include it.
// Assuming it's defined in some header like "VM.h" or "Value.h" includes it.
// Actually, looking at VM.cpp, it included "VM.h". Let's just include "VM.h" which has ObjString.
#include "VM.h"

VMValue::VMValue(const std::string& s) {
    Obj* obj = new ObjString(s);
    val = SIGN_BIT | QNAN | (uint64_t)(uintptr_t)obj;
    obj->retain();
}

VMValue::VMValue(const char* s) {
    Obj* obj = new ObjString(std::string(s));
    val = SIGN_BIT | QNAN | (uint64_t)(uintptr_t)obj;
    obj->retain();
}
