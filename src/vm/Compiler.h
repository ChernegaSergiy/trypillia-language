#ifndef TRYPILLIA_COMPILER_H
#define TRYPILLIA_COMPILER_H

#include <string>
#include "../ast/AST.h"
#include "Chunk.h"

class Compiler {
public:
    Compiler() = default;
    ~Compiler() = default;

    std::string currentFilename = "<unknown>";
    std::shared_ptr<ObjFunction> compile(ASTNode* ast);
};

#endif // TRYPILLIA_COMPILER_H
