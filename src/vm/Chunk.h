#ifndef TRYPILLIA_CHUNK_H
#define TRYPILLIA_CHUNK_H

#include <vector>
#include <cstdint>
#include <variant>
#include <string>
#include <string>
#include <memory>
#include <unordered_map>
#include "OpCode.h"

class Chunk;

struct ObjFunction;
struct ObjNative;
struct ObjList;
struct ObjClass;
struct ObjInstance;

using VMValue = std::variant<std::nullptr_t, bool, double, std::string, std::shared_ptr<ObjFunction>, std::shared_ptr<ObjNative>, std::shared_ptr<ObjList>, std::shared_ptr<ObjClass>, std::shared_ptr<ObjInstance>>;

using NativeFn = VMValue(*)(int argCount, VMValue* args);

struct ObjClass {
    std::string name;
    std::unordered_map<std::string, std::shared_ptr<ObjFunction>> methods;
    ObjClass(std::string name) : name(name) {}
};

struct ObjInstance {
    std::shared_ptr<ObjClass> klass;
    std::unordered_map<std::string, VMValue> fields;
    ObjInstance(std::shared_ptr<ObjClass> k) : klass(k) {}
};

struct ObjList {
    std::vector<VMValue> elements;
    ObjList(const std::vector<VMValue>& e) : elements(e) {}
};

struct ObjNative {
    std::string name;
    int arity;
    NativeFn function;

    ObjNative(std::string n, int a, NativeFn f) : name(n), arity(a), function(f) {}
};

struct ObjFunction {
    std::string name;
    int arity;
    std::shared_ptr<Chunk> chunk;

    ObjFunction() : arity(0) {}
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
        constants.push_back(value);
        return constants.size() - 1;
    }
};

#endif // TRYPILLIA_CHUNK_H
