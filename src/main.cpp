#include "lexer/Lexer.h"
#include "native/os/OS.h"
#include "parser/Parser.h"
#include "semantic/SemanticAnalyzer.h"
#include "vm/Compiler.h"
#include "vm/VM.h"
#include "vm/serializer/Serializer.h"
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
    std::string command;
    if (argc >= 2)
        command = argv[1];

    std::shared_ptr<ObjFunction> function;

    // 1. Check for embedded bytecode first (standalone executable)
    function = Serializer::loadEmbeddedBytecode(argv[0]);
    if (function) {
        // Run embedded bytecode
        for (int i = 1; i < argc; i++) {
            StdLib::OSModule::commandLineArgs.push_back(argv[i]);
        }
        std::cout << "\n--- Bytecode VM Execution (Standalone) ---\n";
        VM vm;
        vm.interpret(function);
        return 0;
    }

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " [build] <file> [output]" << std::endl;
        return 1;
    }

    bool buildStandalone = false;
    std::string inputFile;
    std::string outputFile;

    if (command == "build") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " build <file.try> [output]" << std::endl;
            return 1;
        }
        buildStandalone = true;
        inputFile = argv[2];
        if (argc >= 4) {
            outputFile = argv[3];
        } else {
            outputFile = "app";
        }
    } else {
        inputFile = argv[1];
        for (int i = 2; i < argc; i++) {
            StdLib::OSModule::commandLineArgs.push_back(argv[i]);
        }
    }

    std::ifstream sourceFile(inputFile);
    if (!sourceFile.is_open()) {
        std::cerr << "Error: Could not open source file: " << inputFile << std::endl;
        return 1;
    }
    std::string sourceCode((std::istreambuf_iterator<char>(sourceFile)), std::istreambuf_iterator<char>());

    Lexer lexer(sourceCode);
    Parser parser(lexer);
    ASTNode *ast = parser.parse();

    SemanticAnalyzer semanticAnalyzer;
    semanticAnalyzer.currentFilename = inputFile;
    SymbolTable *globals = semanticAnalyzer.analyze(ast);

    Compiler compiler;
    compiler.currentFilename = inputFile;
    function = compiler.compile(ast, globals);

    if (globals)
        delete globals;

    if (function) {
        if (buildStandalone) {
            if (Serializer::buildStandalone(function, argv[0], outputFile)) {
                std::cout << "Successfully built standalone executable: " << outputFile << std::endl;
            } else {
                std::cerr << "Error building standalone executable: " << outputFile << std::endl;
                return 1;
            }
        } else {
            std::cout << "\n--- Bytecode VM Execution ---\n";
            VM vm;
            vm.interpret(function);
        }
    }

    return 0;
}
