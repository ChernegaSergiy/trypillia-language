#include "CodeGenerator.h"
#include "../utils/ErrorHandling.h"
#include <iostream>
#include <sstream>
#include <map>

class CodeGenVisitor : public ASTVisitor {
private:
    std::stringstream code;
    int indentLevel = 0;
    
    void indent() {
        for (int i = 0; i < indentLevel; i++) {
            code << "  ";
        }
    }
    
    std::map<std::string, std::string> variables;
    
public:
    CodeGenVisitor() {}
    
    std::string getCode() {
        return code.str();
    }
    
    void visit(ProgramNode* node) override {
        code << "// Generated code\n";
        code << "#include <iostream>\n";
        code << "#include <string>\n";
        code << "#include <map>\n\n";
        
        code << "int main() {\n";
        indentLevel++;
        
        for (auto& decl : node->declarations) {
            decl->accept(this);
        }
        
        indent();
        code << "return 0;\n";
        indentLevel--;
        code << "}\n";
    }
    
    void visit(BinaryExpr* node) override {
        code << "(";
        node->left->accept(this);
        
        switch (node->op.type) {
            case TokenType::PLUS:
                code << " + ";
                break;
            case TokenType::MINUS:
                code << " - ";
                break;
            case TokenType::STAR:
                code << " * ";
                break;
            case TokenType::SLASH:
                code << " / ";
                break;
            case TokenType::PERCENT:
                code << " % ";
                break;
            case TokenType::EQUAL_EQUAL:
                code << " == ";
                break;
            case TokenType::BANG_EQUAL:
                code << " != ";
                break;
            case TokenType::LESS:
                code << " < ";
                break;
            case TokenType::LESS_EQUAL:
                code << " <= ";
                break;
            case TokenType::GREATER:
                code << " > ";
                break;
            case TokenType::GREATER_EQUAL:
                code << " >= ";
                break;
            case TokenType::AND:
                code << " && ";
                break;
            case TokenType::OR:
                code << " || ";
                break;
            default:
                ErrorHandling::reportError("Unsupported binary operator");
                break;
        }
        
        node->right->accept(this);
        code << ")";
    }

    void visit(UnaryExpr* node) override {
        code << "(";
        switch (node->op.type) {
            case TokenType::BANG:
                code << "!";
                break;
            case TokenType::MINUS:
                code << "-";
                break;
            case TokenType::PLUS_PLUS:
                code << "++";
                break;
            case TokenType::MINUS_MINUS:
                code << "--";
                break;
            default:
                break;
        }
        node->right->accept(this);
        code << ")";
    }

    void visit(PostfixExpr* node) override {
        code << node->name.lexeme;
        if (node->op.type == TokenType::PLUS_PLUS) {
            code << "++";
        } else {
            code << "--";
        }
    }

    void visit(TernaryExpr* node) override {
    }
    
    void visit(LiteralExpr* node) override {
        switch (node->value.type) {
            case TokenType::STRING:
                code << "\"" << node->value.lexeme << "\"";
                break;
            case TokenType::TRUE:
                code << "true";
                break;
            case TokenType::FALSE:
                code << "false";
                break;
            case TokenType::NIL:
                code << "nullptr";
                break;
            default:
                code << node->value.lexeme;
                break;
        }
    }
    
    void visit(VariableExpr* node) override {
        code << node->name.lexeme;
    }
    
    void visit(AssignExpr* node) override {
        code << node->name.lexeme << " = ";
        node->value->accept(this);
    }

    void visit(CompoundAssignExpr* node) override {
        code << node->name.lexeme << " ";
        switch (node->op.type) {
            case TokenType::PLUS_EQUAL:
                code << "+";
                break;
            case TokenType::MINUS_EQUAL:
                code << "-";
                break;
            case TokenType::STAR_EQUAL:
                code << "*";
                break;
            case TokenType::SLASH_EQUAL:
                code << "/";
                break;
            default:
                break;
        }
        code << "= ";
        node->value->accept(this);
    }
    
    void visit(CallExpr* node) override {
        // Generate code for the callee
        node->callee->accept(this);
        
        // Generate code for the arguments
        code << "(";
        for (size_t i = 0; i < node->arguments.size(); i++) {
            if (i > 0) {
                code << ", ";
            }
            node->arguments[i]->accept(this);
        }
        code << ")";
    }
    
    void visit(ExpressionStmt* node) override {
        indent();
        node->expression->accept(this);
        code << ";\n";
    }
    
    void visit(PrintStmt* node) override {
        indent();
        code << "std::cout << ";
        node->expression->accept(this);
        code << " << std::endl;\n";
    }
    
    void visit(VarStmt* node) override {
        indent();
        
        // Determine variable type
        std::string type = "auto";
        if (node->initializer) {
            // In a real compiler, we would do type inference here
            // For now, we'll just use 'auto'
        }
        
        code << type << " " << node->name.lexeme << " = ";
        
        if (node->initializer) {
            node->initializer->accept(this);
        } else {
            // Default initialization
            code << "{}";
        }
        
        code << ";\n";
        
        // Remember the variable and its type
        variables[node->name.lexeme] = type;
    }
    
    void visit(BlockStmt* node) override {
        indent();
        code << "{\n";
        indentLevel++;
        
        for (auto& stmt : node->statements) {
            stmt->accept(this);
        }
        
        indentLevel--;
        indent();
        code << "}\n";
    }
    
    void visit(IfStmt* node) override {
        indent();
        code << "if (";
        node->condition->accept(this);
        code << ") ";
        
        node->thenBranch->accept(this);
        
        if (node->elseBranch) {
            indent();
            code << "else ";
            node->elseBranch->accept(this);
        }
    }
    
    void visit(WhileStmt* node) override {
        indent();
        code << "while (";
        node->condition->accept(this);
        code << ") ";
        
        node->body->accept(this);
    }
    
    void visit(FunctionNode* node) override {
        // In a real compiler, we would determine the return type
        // For now, we'll just use 'auto'
        code << "auto " << node->name << "(";
        
        // Generate parameters
        for (size_t i = 0; i < node->params.size(); i++) {
            if (i > 0) {
                code << ", ";
            }
            code << "auto " << node->params[i];
        }
        
        code << ") {\n";
        indentLevel++;
        
        // Generate function body
        for (auto& stmt : node->body) {
            stmt->accept(this);
        }
        
        indentLevel--;
        code << "}\n\n";
    }
    
    void visit(ClassNode* node) override {
        code << "class " << node->name << " {\n";
        code << "public:\n";
        indentLevel++;
        
        // Generate methods
        for (auto& method : node->methods) {
            indent();
            method->accept(this);
        }
        
        indentLevel--;
        code << "};\n\n";
    }
};

void CodeGenerator::generate(ASTNode* ast) {
    if (!ast) {
        ErrorHandling::reportError("AST is null");
        return;
    }
    
    CodeGenVisitor visitor;
    ast->accept(&visitor);
    
    // For demonstration, we'll output the generated code to stdout
    // In a real compiler, we would write it to a file
    std::cout << "Generated C++ Code:\n";
    std::cout << "===================\n";
    std::cout << visitor.getCode() << std::endl;
}
