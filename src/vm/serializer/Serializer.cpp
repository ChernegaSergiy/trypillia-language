#include "Serializer.h"
#include <iostream>

#define MAGIC "TRYC"
#define VERSION 1

enum ValueType {
    VAL_NIL = 0,
    VAL_BOOL = 1,
    VAL_DOUBLE = 2,
    VAL_STRING = 3,
    VAL_FUNCTION = 4
};

bool Serializer::saveToFile(const std::shared_ptr<ObjFunction>& function, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return false;

    out.write(MAGIC, 4);
    uint32_t version = VERSION;
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));

    writeFunction(out, function);
    return true;
}

std::shared_ptr<ObjFunction> Serializer::loadFromFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return nullptr;

    char magic[4];
    in.read(magic, 4);
    if (std::string(magic, 4) != MAGIC) return nullptr;

    uint32_t version;
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != VERSION) return nullptr;

    return readFunction(in);
}

void Serializer::writeFunction(std::ofstream& out, const std::shared_ptr<ObjFunction>& function) {
    writeString(out, function->name);
    out.write(reinterpret_cast<const char*>(&function->arity), sizeof(function->arity));
    out.write(reinterpret_cast<const char*>(&function->isAbstract), sizeof(function->isAbstract));
    // statics is not needed for serialized bytecode functions (built at runtime or empty)
    writeChunk(out, function->chunk);
}

std::shared_ptr<ObjFunction> Serializer::readFunction(std::ifstream& in) {
    auto function = std::make_shared<ObjFunction>();
    function->name = readString(in);
    in.read(reinterpret_cast<char*>(&function->arity), sizeof(function->arity));
    in.read(reinterpret_cast<char*>(&function->isAbstract), sizeof(function->isAbstract));
    function->chunk = readChunk(in);
    return function;
}

void Serializer::writeChunk(std::ofstream& out, const std::shared_ptr<Chunk>& chunk) {
    uint32_t codeSize = chunk->code.size();
    out.write(reinterpret_cast<const char*>(&codeSize), sizeof(codeSize));
    if (codeSize > 0) out.write(reinterpret_cast<const char*>(chunk->code.data()), codeSize);

    uint32_t linesSize = chunk->lines.size();
    out.write(reinterpret_cast<const char*>(&linesSize), sizeof(linesSize));
    if (linesSize > 0) out.write(reinterpret_cast<const char*>(chunk->lines.data()), linesSize * sizeof(int));

    uint32_t constSize = chunk->constants.size();
    out.write(reinterpret_cast<const char*>(&constSize), sizeof(constSize));
    for (const auto& val : chunk->constants) {
        writeValue(out, val);
    }
}

std::shared_ptr<Chunk> Serializer::readChunk(std::ifstream& in) {
    auto chunk = std::make_shared<Chunk>();

    uint32_t codeSize;
    in.read(reinterpret_cast<char*>(&codeSize), sizeof(codeSize));
    if (codeSize > 0) {
        chunk->code.resize(codeSize);
        in.read(reinterpret_cast<char*>(chunk->code.data()), codeSize);
    }

    uint32_t linesSize;
    in.read(reinterpret_cast<char*>(&linesSize), sizeof(linesSize));
    if (linesSize > 0) {
        chunk->lines.resize(linesSize);
        in.read(reinterpret_cast<char*>(chunk->lines.data()), linesSize * sizeof(int));
    }

    uint32_t constSize;
    in.read(reinterpret_cast<char*>(&constSize), sizeof(constSize));
    for (uint32_t i = 0; i < constSize; ++i) {
        chunk->constants.push_back(readValue(in));
    }

    return chunk;
}

void Serializer::writeValue(std::ofstream& out, const VMValue& value) {
    if (std::holds_alternative<std::nullptr_t>(value)) {
        uint8_t type = VAL_NIL;
        out.write(reinterpret_cast<const char*>(&type), 1);
    } else if (std::holds_alternative<bool>(value)) {
        uint8_t type = VAL_BOOL;
        out.write(reinterpret_cast<const char*>(&type), 1);
        bool b = std::get<bool>(value);
        out.write(reinterpret_cast<const char*>(&b), sizeof(b));
    } else if (std::holds_alternative<double>(value)) {
        uint8_t type = VAL_DOUBLE;
        out.write(reinterpret_cast<const char*>(&type), 1);
        double d = std::get<double>(value);
        out.write(reinterpret_cast<const char*>(&d), sizeof(d));
    } else if (std::holds_alternative<std::string>(value)) {
        uint8_t type = VAL_STRING;
        out.write(reinterpret_cast<const char*>(&type), 1);
        writeString(out, std::get<std::string>(value));
    } else if (std::holds_alternative<std::shared_ptr<ObjFunction>>(value)) {
        uint8_t type = VAL_FUNCTION;
        out.write(reinterpret_cast<const char*>(&type), 1);
        writeFunction(out, std::get<std::shared_ptr<ObjFunction>>(value));
    } else {
        // Unknown type, just write nil to avoid crashing (should not happen for constants)
        uint8_t type = VAL_NIL;
        out.write(reinterpret_cast<const char*>(&type), 1);
    }
}

VMValue Serializer::readValue(std::ifstream& in) {
    uint8_t type;
    in.read(reinterpret_cast<char*>(&type), 1);

    switch (type) {
        case VAL_NIL: return nullptr;
        case VAL_BOOL: {
            bool b;
            in.read(reinterpret_cast<char*>(&b), sizeof(b));
            return b;
        }
        case VAL_DOUBLE: {
            double d;
            in.read(reinterpret_cast<char*>(&d), sizeof(d));
            return d;
        }
        case VAL_STRING: return readString(in);
        case VAL_FUNCTION: return readFunction(in);
        default: return nullptr;
    }
}

void Serializer::writeString(std::ofstream& out, const std::string& str) {
    uint32_t len = str.length();
    out.write(reinterpret_cast<const char*>(&len), sizeof(len));
    if (len > 0) out.write(str.c_str(), len);
}

std::string Serializer::readString(std::ifstream& in) {
    uint32_t len;
    in.read(reinterpret_cast<char*>(&len), sizeof(len));
    if (len == 0) return "";
    std::string str(len, '\0');
    in.read(&str[0], len);
    return str;
}
