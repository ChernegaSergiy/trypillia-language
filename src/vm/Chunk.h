#ifndef TRYPILLIA_CHUNK_H
#define TRYPILLIA_CHUNK_H

#include <vector>
#include <cstdint>
#include <variant>
#include <string>
#include <memory>
#include "OpCode.h"

using VMValue = std::variant<std::nullptr_t, bool, double, std::string>;

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
