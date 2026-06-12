#ifndef TRYPILLIA_COMPILER_H
#define TRYPILLIA_COMPILER_H

#include "../ast/AST.h"
#include "../symbol/SymbolTable.h"
#include "Chunk.h"
#include <string>

class Compiler {
  public:
    Compiler() = default;
    ~Compiler() = default;

    std::string currentFilename = "<unknown>";
    std::shared_ptr<ObjFunction> compile(ASTNode *ast, SymbolTable *globals = nullptr);
};

#endif // TRYPILLIA_COMPILER_H
