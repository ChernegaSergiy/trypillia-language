#include <iostream>
#include <fstream>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "semantic/SemanticAnalyzer.h"
#include "vm/Compiler.h"
#include "vm/VM.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <source_file>" << std::endl;
        return 1;
    }

    std::ifstream sourceFile(argv[1]);
    if (!sourceFile.is_open()) {
        std::cerr << "Error: Could not open source file." << std::endl;
        return 1;
    }
    
    std::string sourceCode((std::istreambuf_iterator<char>(sourceFile)),
                            std::istreambuf_iterator<char>());

    Lexer lexer(sourceCode);
    Parser parser(lexer);
    ASTNode* ast = parser.parse();

    SemanticAnalyzer semanticAnalyzer;
    semanticAnalyzer.analyze(ast);

    Compiler compiler;
    std::shared_ptr<ObjFunction> function = compiler.compile(ast);
    
    if (function) {
        std::cout << "\n--- Bytecode VM Execution ---\n";
        VM vm;
        vm.interpret(function);
    }

    return 0;
}
