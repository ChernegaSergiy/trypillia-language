#ifndef TRYPILLIA_COMPILER_H
#define TRYPILLIA_COMPILER_H

#include "../ast/AST.h"
#include "Chunk.h"

class Compiler {
public:
    Compiler() = default;
    ~Compiler() = default;

    // Компілює AST у байт-код Chunk
    Chunk* compile(ASTNode* ast);
};

#endif // TRYPILLIA_COMPILER_H
