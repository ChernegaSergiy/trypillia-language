#ifndef TRYPILLIA_OPCODE_H
#define TRYPILLIA_OPCODE_H

#include <cstdint>

enum class OpCode : uint8_t {
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_DEFINE_GLOBAL,
    OP_SET_GLOBAL,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_NEGATE,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_BUILD_LIST,
    OP_INDEX_GET,
    OP_INDEX_SET,
    OP_PRINT,
    OP_RETURN,
    OP_DUP
};

#endif // TRYPILLIA_OPCODE_H
