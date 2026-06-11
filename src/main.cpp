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
    std::string command;
    if (argc >= 2) command = argv[1];

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
        std::cerr << "Usage: " << argv[0] << " [compile|build] <file> [output]" << std::endl;
        return 1;
    }

    bool compileOnly = false;
    bool buildStandalone = false;
    std::string inputFile;
    std::string outputFile;

    if (command == "compile" || command == "build") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " " << command << " <file.try> [output]" << std::endl;
            return 1;
        }
        if (command == "build") buildStandalone = true;
        else compileOnly = true;
        
        inputFile = argv[2];
        if (argc >= 4) {
            outputFile = argv[3];
        } else {
            outputFile = buildStandalone ? "app" : inputFile + "c";
        }
    } else {
        inputFile = argv[1];
        for (int i = 2; i < argc; i++) {
            StdLib::OSModule::commandLineArgs.push_back(argv[i]);
        }
    }

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
        if (buildStandalone) {
            if (Serializer::buildStandalone(function, argv[0], outputFile)) {
                // chmod +x the output file using system call
                std::string chmodCmd = "chmod +x " + outputFile;
                int ret = system(chmodCmd.c_str());
                (void)ret; // suppress warning
                std::cout << "Successfully built standalone executable: " << outputFile << std::endl;
            } else {
                std::cerr << "Error building standalone executable: " << outputFile << std::endl;
                return 1;
            }
        } else if (compileOnly) {
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
