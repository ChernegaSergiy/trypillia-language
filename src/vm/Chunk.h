#ifndef TRYPILLIA_CHUNK_H
#define TRYPILLIA_CHUNK_H

#include "OpCode.h"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
enum class VMAccessModifier { PUBLIC, PRIVATE, PROTECTED };

class Chunk;

struct ObjFunction;
struct ObjClosure;
struct ObjUpvalue;
struct ObjNative;
struct ObjList;
struct ObjMap;
struct ObjClass;
struct ObjInstance;
struct ObjBoundMethod;

using VMValue =
    std::variant<std::nullptr_t, bool, double, std::string, std::shared_ptr<ObjFunction>, std::shared_ptr<ObjClosure>,
                 std::shared_ptr<ObjNative>, std::shared_ptr<ObjList>, std::shared_ptr<ObjMap>,
                 std::shared_ptr<ObjClass>, std::shared_ptr<ObjInstance>, std::shared_ptr<ObjBoundMethod>>;

using NativeFn = VMValue (*)(int argCount, VMValue *args);

struct VMValueHash {
    std::size_t operator()(const VMValue &v) const {
        if (std::holds_alternative<double>(v))
            return std::hash<double>{}(std::get<double>(v));
        if (std::holds_alternative<std::string>(v))
            return std::hash<std::string>{}(std::get<std::string>(v));
        if (std::holds_alternative<bool>(v))
            return std::hash<bool>{}(std::get<bool>(v));
        if (std::holds_alternative<std::nullptr_t>(v))
            return 0;
        return 0;
    }
};

struct VMValueEqual {
    bool operator()(const VMValue &a, const VMValue &b) const {
        return a == b;
    }
};

struct ObjClass {
    std::string name;
    std::unordered_map<std::string, VMValue> methods;
    std::shared_ptr<ObjClass> superclass;
    bool isAbstract = false;
    std::unordered_map<std::string, VMValue> statics;
    std::unordered_map<std::string, VMAccessModifier> fieldModifiers;
    ObjClass(std::string name) : name(name), superclass(nullptr) {
    }
};

struct ObjInstance {
    std::shared_ptr<ObjClass> klass;
    std::unordered_map<std::string, VMValue> fields;

    // Native resource binding
    void *nativeData = nullptr;
    void (*freeFn)(void *) = nullptr;

    ObjInstance(std::shared_ptr<ObjClass> k) : klass(k) {
    }

    ~ObjInstance() {
        if (nativeData && freeFn) {
            freeFn(nativeData);
            nativeData = nullptr;
        }
    }
};

struct ObjBoundMethod {
    std::shared_ptr<ObjInstance> receiver;
    VMValue method;
    ObjBoundMethod(std::shared_ptr<ObjInstance> r, VMValue m) : receiver(r), method(m) {
    }
};

struct ObjList {
    std::vector<VMValue> elements;
    ObjList(const std::vector<VMValue> &e) : elements(e) {
    }
};

struct ObjMap {
    std::unordered_map<VMValue, VMValue, VMValueHash, VMValueEqual> values;
};

struct ObjNative {
    std::string name;
    int arity;
    NativeFn function;
    bool isAbstract = false;
    VMAccessModifier accessModifier = VMAccessModifier::PUBLIC;

    ObjNative(std::string n, int a, NativeFn f) : name(n), arity(a), function(f) {
    }
};

struct ObjFunction {
    std::string name;
    int arity;
    std::shared_ptr<Chunk> chunk;
    bool isAbstract = false;
    std::unordered_map<std::string, VMValue> statics;
    VMAccessModifier accessModifier = VMAccessModifier::PUBLIC;
    std::string enclosingClassName = "";
    std::string filename = "";
    int upvalueCount = 0;

    ObjFunction() : arity(0), upvalueCount(0) {
    }
};

struct ObjUpvalue {
    VMValue *location;
    VMValue closed;
    std::shared_ptr<ObjUpvalue> next;

    ObjUpvalue(VMValue *slot) : location(slot), closed(nullptr), next(nullptr) {
    }
};

struct ObjClosure {
    std::shared_ptr<ObjFunction> function;
    std::vector<std::shared_ptr<ObjUpvalue>> upvalues;

    ObjClosure(std::shared_ptr<ObjFunction> f) : function(f) {
    }
};

class Chunk {
  public:
    std::vector<uint8_t> code;
    std::vector<VMValue> constants;
    std::vector<int> lines;

    Chunk() = default;

    void write(uint8_t byte, int line) {
        code.push_back(byte);
        lines.push_back(line);
    }

    void writeOp(OpCode op, int line) {
        write(static_cast<uint8_t>(op), line);
    }

    int addConstant(VMValue value) {
        // Simple deduplication to avoid exceeding 255 constants
        if (std::holds_alternative<std::string>(value) || std::holds_alternative<double>(value) ||
            std::holds_alternative<bool>(value)) {
            for (size_t i = 0; i < constants.size(); i++) {
                if (constants[i] == value) {
                    return static_cast<int>(i);
                }
            }
        }
        constants.push_back(value);
        return static_cast<int>(constants.size() - 1);
    }
};

#endif // TRYPILLIA_CHUNK_H
