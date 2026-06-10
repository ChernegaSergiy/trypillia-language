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
        code << "(";
        node->condition->accept(this);
        code << " ? ";
        node->thenBranch->accept(this);
        code << " : ";
        node->elseBranch->accept(this);
        code << ")";
    }

    void visit(ListExpr* node) override {
        code << "std::vector<auto>{";
        for (size_t i = 0; i < node->elements.size(); i++) {
            if (i > 0) code << ", ";
            node->elements[i]->accept(this);
        }
        code << "}";
    }

    void visit(IndexGetExpr* node) override {
        node->object->accept(this);
        code << "[";
        node->index->accept(this);
        code << "]";
    }

    void visit(IndexSetExpr* node) override {
        node->object->accept(this);
        code << "[";
        node->index->accept(this);
        code << "] = ";
        node->value->accept(this);
    }

    void visit(ThisExpr* node) override {
        code << "this";
    }

    void visit(GetExpr* node) override {
        node->object->accept(this);
        code << "." << node->name.lexeme;
    }

    void visit(SetExpr* node) override {
        node->object->accept(this);
        code << "." << node->name.lexeme << " = ";
        node->value->accept(this);
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
        
        std::string type = node->isConst ? "const auto" : "auto";
        
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

    void visit(ReturnStmt* node) override {
        indent();
        code << "return";
        if (node->value != nullptr) {
            code << " ";
            node->value->accept(this);
        }
        code << ";\n";
    }

    void visit(BreakStmt* node) override {
        indent();
        code << "break;\n";
    }

    void visit(ContinueStmt* node) override {
        indent();
        code << "continue;\n";
    }

    void visit(ForStmt* node) override {
        indent();
        code << "for (";
        if (node->initializer != nullptr) {
            node->initializer->accept(this);
        } else {
            code << "; ";
        }
        if (node->condition != nullptr) {
            node->condition->accept(this);
        }
        code << "; ";
        if (node->increment != nullptr) {
            node->increment->accept(this);
        }
        code << ")";
        node->body->accept(this);
    }

    void visit(ForeachStmt* node) override {
        indent();
        code << "for (auto " << node->name.lexeme << " : ";
        node->iterable->accept(this);
        code << ")";
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
    
    void visit(FieldDeclNode* node) override {
    }
    
    void visit(ClassNode* node) override {
        code << "class " << node->name << " {\n";
        indentLevel++;
        
        // Check if we need a constructor (any field with initializer)
        bool hasInit = false;
        for (auto& field : node->fields) {
            if (field->initializer) { hasInit = true; break; }
        }
        
        // Gather methods by access
        std::vector<FunctionNode*> publicMethods, privateMethods;
        for (auto& method : node->methods) {
            if (method->accessModifier == AccessModifier::PRIVATE)
                privateMethods.push_back(method);
            else
                publicMethods.push_back(method);
        }
        
        // Gather fields by access
        std::vector<FieldDeclNode*> publicFields, privateFields;
        for (auto& field : node->fields) {
            if (field->accessModifier == AccessModifier::PRIVATE)
                privateFields.push_back(field);
            else
                publicFields.push_back(field);
        }
        
        // Generate constructor with initializer list
        if (hasInit) {
            indent();
            code << node->name << "()";
            bool first = true;
            for (auto& field : node->fields) {
                if (field->initializer) {
                    if (first) { code << " : "; first = false; }
                    else { code << ", "; }
                    code << field->name << "(";
                    field->initializer->accept(this);
                    code << ")";
                }
            }
            code << " {}\n";
        }
        
        // Public section
        code << "public:\n";
        for (auto& method : publicMethods) {
            indent();
            method->accept(this);
        }
        for (auto& field : publicFields) {
            indent();
            code << "auto " << field->name;
            if (field->initializer) {
                code << " = ";
                field->initializer->accept(this);
            }
            code << ";\n";
        }
        
        // Private section
        if (!privateMethods.empty() || !privateFields.empty()) {
            code << "private:\n";
            for (auto& method : privateMethods) {
                indent();
                method->accept(this);
            }
            for (auto& field : privateFields) {
                indent();
                code << "auto " << field->name;
                if (field->initializer) {
                    code << " = ";
                    field->initializer->accept(this);
                }
                code << ";\n";
            }
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
