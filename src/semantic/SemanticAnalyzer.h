#ifndef SEMANTIC_ANALYZER_H
#define SEMANTIC_ANALYZER_H

#include "AST.h"

class SemanticAnalyzer {
public:
    void analyze(ASTNode* ast);
};

#endif // SEMANTIC_ANALYZER_H
