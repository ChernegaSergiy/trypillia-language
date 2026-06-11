#include <iostream>
#include <fstream>
#include <string>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "semantic/SemanticAnalyzer.h"
#include "vm/Compiler.h"
#include "vm/VM.h"
#include "native/os/OS.h"
#include "vm/serializer/Serializer.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " [compile] <file> [output.tryc]" << std::endl;
        return 1;
    }

    std::string command = argv[1];
    bool compileOnly = false;
    std::string inputFile;
    std::string outputFile;

    if (command == "compile") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " compile <file.try> [output.tryc]" << std::endl;
            return 1;
        }
        compileOnly = true;
        inputFile = argv[2];
        if (argc >= 4) {
            outputFile = argv[3];
        } else {
            outputFile = inputFile + "c"; // Default .try -> .tryc
        }
    } else {
        inputFile = argv[1];
        for (int i = 2; i < argc; i++) {
            StdLib::OSModule::commandLineArgs.push_back(argv[i]);
        }
    }

    std::shared_ptr<ObjFunction> function;

    // Check if it's a precompiled bytecode file
    if (inputFile.length() > 5 && inputFile.substr(inputFile.length() - 5) == ".tryc") {
        function = Serializer::loadFromFile(inputFile);
        if (!function) {
            std::cerr << "Error: Could not load bytecode from " << inputFile << std::endl;
            return 1;
        }
    } else {
        std::ifstream sourceFile(inputFile);
        if (!sourceFile.is_open()) {
            std::cerr << "Error: Could not open source file: " << inputFile << std::endl;
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
        compiler.currentFilename = inputFile;
        function = compiler.compile(ast);
    }

    if (function) {
        if (compileOnly) {
            if (Serializer::saveToFile(function, outputFile)) {
                std::cout << "Successfully compiled to " << outputFile << std::endl;
            } else {
                std::cerr << "Error writing to " << outputFile << std::endl;
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
