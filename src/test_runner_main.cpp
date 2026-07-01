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

static bool readTestResults(VM &vm, std::vector<std::string> &names, std::vector<bool> &results) {
    auto namesIt = vm.globals.find("__test_names");
    auto resultsIt = vm.globals.find("__test_results");
    if (namesIt == vm.globals.end() || !namesIt->second.isList())
        return false;
    if (resultsIt == vm.globals.end() || !resultsIt->second.isList())
        return false;

    auto namesList = namesIt->second.asList();
    auto resultsList = resultsIt->second.asList();

    for (size_t i = 0; i < namesList->elements.size() && i < resultsList->elements.size(); i++) {
        if (namesList->elements[i].isString()) {
            names.push_back(namesList->elements[i].asString()->flatten());
            results.push_back(resultsList->elements[i].isBool() ? resultsList->elements[i].asBool() : false);
        }
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <test-file> [test-file...]" << std::endl;
        return 1;
    }

    int totalTests = 0;
    int totalPassed = 0;
    int totalFailed = 0;
    int tapCounter = 0;

    for (int i = 1; i < argc; i++) {
        std::string path = argv[i];

        std::ifstream sourceFile(path);
        if (!sourceFile.is_open()) {
            std::cout << "not ok " << (++tapCounter) << " \xe2\x80\x94 " << path << std::endl;
            std::cout << "# Could not open file" << std::endl;
            totalFailed++;
            continue;
        }
        std::string source((std::istreambuf_iterator<char>(sourceFile)), std::istreambuf_iterator<char>());

        Lexer lexer(source);
        Parser parser(lexer);
        ASTNode *ast = parser.parse();
        if (!ast) {
            std::cout << "not ok " << (++tapCounter) << " \xe2\x80\x94 " << path << std::endl;
            std::cout << "# Parse error" << std::endl;
            totalFailed++;
            continue;
        }

        ASTOptimizer::optimize(ast);

        SemanticAnalyzer semanticAnalyzer;
        semanticAnalyzer.currentFilename = path;
        SymbolTable *globals = semanticAnalyzer.analyze(ast);
        if (!globals) {
            std::cout << "not ok " << (++tapCounter) << " \xe2\x80\x94 " << path << std::endl;
            std::cout << "# Semantic analysis error" << std::endl;
            totalFailed++;
            continue;
        }

        Compiler compiler;
        compiler.currentFilename = path;
        ObjFunction *function = compiler.compile(ast, globals);
        delete globals;

        if (!function) {
            std::cout << "not ok " << (++tapCounter) << " \xe2\x80\x94 " << path << std::endl;
            std::cout << "# Compilation error" << std::endl;
            totalFailed++;
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

        std::vector<std::string> testNames;
        std::vector<bool> testResults;
        bool hasFrameworkResults = readTestResults(vm, testNames, testResults);

        if (hasFrameworkResults && !testNames.empty()) {
            for (size_t j = 0; j < testNames.size(); j++) {
                tapCounter++;
                if (testResults[j]) {
                    std::cout << "ok " << tapCounter << " \xe2\x80\x94 " << testNames[j] << std::endl;
                    totalPassed++;
                } else {
                    std::cout << "not ok " << tapCounter << " \xe2\x80\x94 " << testNames[j] << std::endl;
                    std::string errOutput = capturedErr.str();
                    if (!errOutput.empty()) {
                        std::cout << "# " << errOutput;
                        if (errOutput.back() != '\n')
                            std::cout << std::endl;
                    }
                    totalFailed++;
                }
            }
            totalTests += (int)testNames.size();
        } else {
            totalTests++;
            if (result == InterpretResult::INTERPRET_OK) {
                tapCounter++;
                totalPassed++;
                std::cout << "ok " << tapCounter << " \xe2\x80\x94 " << path << std::endl;
            } else {
                tapCounter++;
                totalFailed++;
                std::cout << "not ok " << tapCounter << " \xe2\x80\x94 " << path << std::endl;
                std::string errOutput = capturedErr.str();
                if (!errOutput.empty()) {
                    std::cout << "# " << errOutput;
                    if (errOutput.back() != '\n')
                        std::cout << std::endl;
                }
            }
        }
    }

    if (totalTests > 0) {
        std::cout << "1.." << tapCounter << std::endl;
        std::cout << "# " << totalTests << " tests, " << totalPassed << " passed, " << totalFailed << " failed"
                  << std::endl;
    }

    return totalFailed;
}
