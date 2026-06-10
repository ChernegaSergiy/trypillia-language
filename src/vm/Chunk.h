#ifndef TRYPILLIA_CHUNK_H
#define TRYPILLIA_CHUNK_H

#include <vector>
#include <cstdint>
#include <variant>
#include <string>
#include <memory>
#include "OpCode.h"

// Тимчасовий тип Value (згодом його можна оптимізувати, замінивши std::variant на щось легше)
using VMValue = std::variant<std::nullptr_t, bool, double, std::string>;

// Chunk — це блок (шматок) скомпільованого байт-коду
class Chunk {
public:
    std::vector<uint8_t> code;        // Масив байт-коду (інструкції)
    std::vector<VMValue> constants;   // Масив констант (числа, рядки тощо)
    std::vector<int> lines;           // Номери рядків для відлагодження

    Chunk() = default;

    // Додати байт до чанку
    void write(uint8_t byte, int line) {
        code.push_back(byte);
        lines.push_back(line);
    }

    // Додати опкод до чанку
    void writeOp(OpCode op, int line) {
        write(static_cast<uint8_t>(op), line);
    }

    // Додати константу і отримати її індекс
    int addConstant(VMValue value) {
        constants.push_back(value);
        return constants.size() - 1;
    }
};

#endif // TRYPILLIA_CHUNK_H
