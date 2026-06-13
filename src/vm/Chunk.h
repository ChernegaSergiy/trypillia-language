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

struct ObjString {
    mutable std::string flatData;
    mutable std::shared_ptr<ObjString> left;
    mutable std::shared_ptr<ObjString> right;
    size_t length;
    mutable bool isFlat;

    ObjString(std::string s) : flatData(std::move(s)), left(nullptr), right(nullptr), length(flatData.length()), isFlat(true) {}
    ObjString(std::shared_ptr<ObjString> l, std::shared_ptr<ObjString> r) : left(l), right(r), length(l->length + r->length), isFlat(false) {}

    std::string flatten() const {
        if (isFlat) return flatData;

        std::vector<const ObjString*> stack;
        std::string result;
        result.reserve(length);

        stack.push_back(this);

        while (!stack.empty()) {
            const ObjString* current = stack.back();
            stack.pop_back();

            if (current->isFlat) {
                result += current->flatData;
            } else {
                if (current->right) stack.push_back(current->right.get());
                if (current->left) stack.push_back(current->left.get());
            }
        }

        flatData = std::move(result);
        left = nullptr;
        right = nullptr;
        isFlat = true;
        return flatData;
    }

    ~ObjString() {
        if (!left && !right) return;

        std::vector<std::shared_ptr<ObjString>> stack;
        if (left) stack.push_back(std::move(left));
        if (right) stack.push_back(std::move(right));

        while (!stack.empty()) {
            auto current = std::move(stack.back());
            stack.pop_back();

            if (current->left) {
                if (current->left.use_count() == 1) stack.push_back(std::move(current->left));
                else current->left.reset();
            }
            if (current->right) {
                if (current->right.use_count() == 1) stack.push_back(std::move(current->right));
                else current->right.reset();
            }
        }
    }
};

struct VMValue : public std::variant<std::nullptr_t, bool, double, std::shared_ptr<ObjString>, std::shared_ptr<ObjFunction>, std::shared_ptr<ObjClosure>,
                 std::shared_ptr<ObjNative>, std::shared_ptr<ObjList>, std::shared_ptr<ObjMap>,
                 std::shared_ptr<ObjClass>, std::shared_ptr<ObjInstance>, std::shared_ptr<ObjBoundMethod>> {
    using variant::variant;

    VMValue() : variant() {}
    VMValue(const std::string& s) : variant(std::make_shared<ObjString>(s)) {}
    VMValue(const char* s) : variant(std::make_shared<ObjString>(std::string(s))) {}

    bool operator==(const VMValue& other) const {
        if (this->index() != other.index()) return false;
        if (std::holds_alternative<std::shared_ptr<ObjString>>(*this)) {
            return std::get<std::shared_ptr<ObjString>>(*this)->flatten() == std::get<std::shared_ptr<ObjString>>(other)->flatten();
        }
        return static_cast<const variant&>(*this) == static_cast<const variant&>(other);
    }
};

using NativeFn = VMValue (*)(int argCount, VMValue *args);

struct VMValueHash {
    std::size_t operator()(const VMValue &v) const {
        if (std::holds_alternative<double>(v))
            return std::hash<double>{}(std::get<double>(v));
        if (std::holds_alternative<std::shared_ptr<ObjString>>(v))
            return std::hash<std::string>{}(std::get<std::shared_ptr<ObjString>>(v)->flatten());
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
    VMValue receiver;
    VMValue method;
    ObjBoundMethod(VMValue r, VMValue m) : receiver(r), method(m) {
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
    int maxArity;
    std::shared_ptr<Chunk> chunk;
    bool isAbstract = false;
    std::unordered_map<std::string, VMValue> statics;
    VMAccessModifier accessModifier = VMAccessModifier::PUBLIC;
    std::string enclosingClassName = "";
    std::string filename = "";
    int upvalueCount = 0;

    ObjFunction() : arity(0), maxArity(0), upvalueCount(0) {
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
        if (std::holds_alternative<std::shared_ptr<ObjString>>(value) || std::holds_alternative<double>(value) ||
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
