#include "ast/ASTOptimizer.h"
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "semantic/SemanticAnalyzer.h"
#include "vm/Compiler.h"
#include "vm/VM.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <test-file> [test-file...]" << std::endl;
        return 1;
    }

    int passed = 0;
    int failed = 0;

    for (int i = 1; i < argc; i++) {
        std::string path = argv[i];

        std::ifstream sourceFile(path);
        if (!sourceFile.is_open()) {
            std::cout << "not ok " << i << " — " << path << std::endl;
            std::cout << "# Could not open file" << std::endl;
            failed++;
            continue;
        }
        std::string source((std::istreambuf_iterator<char>(sourceFile)), std::istreambuf_iterator<char>());

        Lexer lexer(source);
        Parser parser(lexer);
        ASTNode *ast = parser.parse();
        if (!ast) {
            std::cout << "not ok " << i << " — " << path << std::endl;
            std::cout << "# Parse error" << std::endl;
            failed++;
            continue;
        }

        ASTOptimizer::optimize(ast);

        SemanticAnalyzer semanticAnalyzer;
        semanticAnalyzer.currentFilename = path;
        SymbolTable *globals = semanticAnalyzer.analyze(ast);
        if (!globals) {
            std::cout << "not ok " << i << " — " << path << std::endl;
            std::cout << "# Semantic analysis error" << std::endl;
            failed++;
            continue;
        }

        Compiler compiler;
        compiler.currentFilename = path;
        ObjFunction *function = compiler.compile(ast, globals);
        delete globals;

        if (!function) {
            std::cout << "not ok " << i << " — " << path << std::endl;
            std::cout << "# Compilation error" << std::endl;
            failed++;
            continue;
        }

        std::stringstream capturedOut;
        std::stringstream capturedErr;
        auto oldBufOut = std::cout.rdbuf(capturedOut.rdbuf());
        auto oldBufErr = std::cerr.rdbuf(capturedErr.rdbuf());

        VM vm;
        InterpretResult result = vm.interpret(function);

        std::cout.rdbuf(oldBufOut);
        std::cerr.rdbuf(oldBufErr);

        if (result == InterpretResult::INTERPRET_OK) {
            passed++;
            std::cout << "ok " << i << " — " << path << std::endl;
        } else {
            failed++;
            std::cout << "not ok " << i << " — " << path << std::endl;
            std::string errOutput = capturedErr.str();
            if (!errOutput.empty()) {
                std::cout << "# " << errOutput;
                if (errOutput.back() != '\n')
                    std::cout << std::endl;
            }
        }
    }

    std::cout << "1.." << (argc - 1) << std::endl;
    std::cout << "# " << (passed + failed) << " tests, " << passed << " passed, " << failed << " failed"
              << std::endl;

    return failed;
}
