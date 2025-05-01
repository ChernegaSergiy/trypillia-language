#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "AST.h"

class Interpreter {
public:
    void execute(ASTNode* ast);
};

#endif // INTERPRETER_H
