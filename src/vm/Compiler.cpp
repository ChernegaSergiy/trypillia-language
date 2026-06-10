#include "Compiler.h"
#include "../utils/ErrorHandling.h"
#include <iostream>

class CompilerVisitor : public ASTVisitor {
private:
    Chunk* chunk;

    void emitByte(uint8_t byte) {
        chunk->write(byte, 1); // Тимчасово рядок 1
    }

    void emitBytes(uint8_t byte1, uint8_t byte2) {
        emitByte(byte1);
        emitByte(byte2);
    }

    void emitConstant(VMValue value) {
        emitBytes(static_cast<uint8_t>(OpCode::OP_CONSTANT), chunk->addConstant(value));
    }

    int emitJump(uint8_t instruction) {
        emitByte(instruction);
        emitByte(0xff); // placeholder
        emitByte(0xff); // placeholder
        return chunk->code.size() - 2;
    }

    void patchJump(int offset) {
        int jump = chunk->code.size() - offset - 2;
        if (jump > 65535) {
            ErrorHandling::reportError("Too much code to jump over.");
        }
        chunk->code[offset] = (jump >> 8) & 0xff;
        chunk->code[offset + 1] = jump & 0xff;
    }

public:
    CompilerVisitor(Chunk* chunk) : chunk(chunk) {}

    void visit(ProgramNode* node) override {
        for (auto& decl : node->declarations) {
            decl->accept(this);
        }
    }

    void visit(BinaryExpr* node) override {
        node->left->accept(this);
        node->right->accept(this);

        switch (node->op.type) {
            case TokenType::PLUS: emitByte(static_cast<uint8_t>(OpCode::OP_ADD)); break;
            case TokenType::MINUS: emitByte(static_cast<uint8_t>(OpCode::OP_SUBTRACT)); break;
            case TokenType::STAR: emitByte(static_cast<uint8_t>(OpCode::OP_MULTIPLY)); break;
            case TokenType::SLASH: emitByte(static_cast<uint8_t>(OpCode::OP_DIVIDE)); break;
            case TokenType::EQUAL_EQUAL: emitByte(static_cast<uint8_t>(OpCode::OP_EQUAL)); break;
            case TokenType::LESS: emitByte(static_cast<uint8_t>(OpCode::OP_LESS)); break;
            case TokenType::GREATER: emitByte(static_cast<uint8_t>(OpCode::OP_GREATER)); break;
            // TODO: <=, >=, != 
            default: break; 
        }
    }

    void visit(LiteralExpr* node) override {
        if (node->value.type == TokenType::NUMBER) {
            emitConstant(std::stod(node->value.lexeme));
        } else if (node->value.type == TokenType::STRING) {
            emitConstant(node->value.lexeme);
        } else if (node->value.type == TokenType::TRUE) {
            emitByte(static_cast<uint8_t>(OpCode::OP_TRUE));
        } else if (node->value.type == TokenType::FALSE) {
            emitByte(static_cast<uint8_t>(OpCode::OP_FALSE));
        } else if (node->value.type == TokenType::NIL) {
            emitByte(static_cast<uint8_t>(OpCode::OP_NIL));
        }
    }

    void visit(ExpressionStmt* node) override {
        node->expression->accept(this);
        emitByte(static_cast<uint8_t>(OpCode::OP_POP));
    }

    void visit(PrintStmt* node) override {
        node->expression->accept(this);
        emitByte(static_cast<uint8_t>(OpCode::OP_PRINT));
    }

    // Заглушки для всіх інших методів ASTVisitor
    void visit(VariableExpr* node) override {}
    void visit(AssignExpr* node) override {}
    void visit(CompoundAssignExpr* node) override {}
    void visit(CallExpr* node) override {}
    void visit(VarStmt* node) override {}
    void visit(BlockStmt* node) override {}
    void visit(IfStmt* node) override {
        node->condition->accept(this);

        int thenJump = emitJump(static_cast<uint8_t>(OpCode::OP_JUMP_IF_FALSE));
        emitByte(static_cast<uint8_t>(OpCode::OP_POP)); // Очищаємо умову зі стеку

        node->thenBranch->accept(this);

        int elseJump = emitJump(static_cast<uint8_t>(OpCode::OP_JUMP));

        patchJump(thenJump);
        emitByte(static_cast<uint8_t>(OpCode::OP_POP)); // Очищаємо умову зі стеку, якщо стрибнули

        if (node->elseBranch) {
            node->elseBranch->accept(this);
        }
        patchJump(elseJump);
    }
    void visit(WhileStmt* node) override {}
    void visit(DoWhileStmt* node) override {}
    void visit(ReturnStmt* node) override {}
    void visit(BreakStmt* node) override {}
    void visit(ContinueStmt* node) override {}
    void visit(ForStmt* node) override {}
    void visit(ForeachStmt* node) override {}
    void visit(SwitchStmt* node) override {}
    void visit(UnaryExpr* node) override {
        node->right->accept(this);
        switch (node->op.type) {
            case TokenType::BANG: emitByte(static_cast<uint8_t>(OpCode::OP_NOT)); break;
            case TokenType::MINUS: emitByte(static_cast<uint8_t>(OpCode::OP_NEGATE)); break;
            default: break;
        }
    }
    void visit(ThisExpr* node) override {}
    void visit(SuperExpr* node) override {}
    void visit(GetExpr* node) override {}
    void visit(SetExpr* node) override {}
    void visit(PostfixExpr* node) override {}
    void visit(TernaryExpr* node) override {}
    void visit(ListExpr* node) override {}
    void visit(IndexGetExpr* node) override {}
    void visit(IndexSetExpr* node) override {}
    void visit(FunctionNode* node) override {}
    void visit(FieldDeclNode* node) override {}
    void visit(ClassNode* node) override {}
    void visit(InterfaceNode* node) override {}
    void visit(TraitNode* node) override {}
    void visit(StaticGetExpr* node) override {}
    void visit(StaticCallExpr* node) override {}
    void visit(StaticSetExpr* node) override {}
    void visit(LoadStmt* node) override {}
};

Chunk* Compiler::compile(ASTNode* ast) {
    if (!ast) return nullptr;

    Chunk* chunk = new Chunk();
    CompilerVisitor visitor(chunk);
    
    ast->accept(&visitor);
    
    chunk->writeOp(OpCode::OP_RETURN, 1);
    
    return chunk;
}
