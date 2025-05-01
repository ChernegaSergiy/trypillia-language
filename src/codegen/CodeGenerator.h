#ifndef CODE_GENERATOR_H
#define CODE_GENERATOR_H

#include "../ast/AST.h"

class CodeGenerator {
public:
    void generate(ASTNode* ast);
};

#endif // CODE_GENERATOR_H
