#include "Compiler.h"
#include "../utils/ErrorHandling.h"
#include "../lexer/Lexer.h"
#include "../parser/Parser.h"
#include <iostream>
#include <fstream>

class CompilerVisitor : public ASTVisitor {
private:
    Chunk* chunk;

    struct Local {
        std::string name;
        int depth;
    };
    std::vector<Local> locals;
    int scopeDepth = 0;

    void beginScope() {
        scopeDepth++;
    }

    void endScope() {
        scopeDepth--;
        while (locals.size() > 0 && locals.back().depth > scopeDepth) {
            emitByte(static_cast<uint8_t>(OpCode::OP_POP));
            locals.pop_back();
        }
    }

    int resolveLocal(const std::string& name) {
        for (int i = locals.size() - 1; i >= 0; i--) {
            if (locals[i].name == name) {
                return i;
            }
        }
        return -1;
    }

public:
    std::string currentClassName = "";
    std::string currentParentName = "";

    struct LoopContext {
        int startOffset;
        int scopeDepth;
        std::vector<int> breakJumps;
        std::vector<int> continueJumps;
    };
    std::vector<LoopContext> loops;

    CompilerVisitor(Chunk* chunk) : chunk(chunk) {
        locals.push_back({"", 0});
    }

    void emitByte(uint8_t byte) {
        chunk->write(byte, 1);
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
        emitByte(0xff);
        emitByte(0xff);
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

    void emitLoop(int loopStart) {
        emitByte(static_cast<uint8_t>(OpCode::OP_LOOP));
        int offset = chunk->code.size() - loopStart + 2;
        if (offset > 65535) ErrorHandling::reportError("Loop body too large.");
        emitByte((offset >> 8) & 0xff);
        emitByte(offset & 0xff);
    }

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

    void visit(VariableExpr* node) override {
        int arg = resolveLocal(node->name.lexeme);
        if (arg != -1) {
            emitBytes(static_cast<uint8_t>(OpCode::OP_GET_LOCAL), (uint8_t)arg);
        } else {
            emitBytes(static_cast<uint8_t>(OpCode::OP_GET_GLOBAL), chunk->addConstant(node->name.lexeme));
        }
    }
    void visit(AssignExpr* node) override {
        node->value->accept(this);
        int arg = resolveLocal(node->name.lexeme);
        if (arg != -1) {
            emitBytes(static_cast<uint8_t>(OpCode::OP_SET_LOCAL), (uint8_t)arg);
        } else {
            emitBytes(static_cast<uint8_t>(OpCode::OP_SET_GLOBAL), chunk->addConstant(node->name.lexeme));
        }
    }
    void visit(CompoundAssignExpr* node) override {
        int arg = resolveLocal(node->name.lexeme);
        if (arg != -1) {
            emitBytes(static_cast<uint8_t>(OpCode::OP_GET_LOCAL), (uint8_t)arg);
        } else {
            emitBytes(static_cast<uint8_t>(OpCode::OP_GET_GLOBAL), chunk->addConstant(node->name.lexeme));
        }
        
        node->value->accept(this);
        
        switch (node->op.type) {
            case TokenType::PLUS_EQUAL: emitByte(static_cast<uint8_t>(OpCode::OP_ADD)); break;
            case TokenType::MINUS_EQUAL: emitByte(static_cast<uint8_t>(OpCode::OP_SUBTRACT)); break;
            case TokenType::STAR_EQUAL: emitByte(static_cast<uint8_t>(OpCode::OP_MULTIPLY)); break;
            case TokenType::SLASH_EQUAL: emitByte(static_cast<uint8_t>(OpCode::OP_DIVIDE)); break;
            default: break;
        }
        
        if (arg != -1) {
            emitBytes(static_cast<uint8_t>(OpCode::OP_SET_LOCAL), (uint8_t)arg);
        } else {
            emitBytes(static_cast<uint8_t>(OpCode::OP_SET_GLOBAL), chunk->addConstant(node->name.lexeme));
        }
    }
    void visit(CallExpr* node) override {
        node->callee->accept(this);
        for (auto& arg : node->arguments) {
            arg->accept(this);
        }
        emitBytes(static_cast<uint8_t>(OpCode::OP_CALL), node->arguments.size());
    }
    void visit(VarStmt* node) override {
        if (node->initializer) {
            node->initializer->accept(this);
        } else {
            emitByte(static_cast<uint8_t>(OpCode::OP_NIL));
        }
        
        if (scopeDepth > 0) {
            locals.push_back({node->name.lexeme, scopeDepth});
        } else {
            emitBytes(static_cast<uint8_t>(OpCode::OP_DEFINE_GLOBAL), chunk->addConstant(node->name.lexeme));
        }
    }
    void visit(BlockStmt* node) override {
        beginScope();
        for (auto& stmt : node->statements) {
            stmt->accept(this);
        }
        endScope();
    }
    void visit(IfStmt* node) override {
        node->condition->accept(this);

        int thenJump = emitJump(static_cast<uint8_t>(OpCode::OP_JUMP_IF_FALSE));
        emitByte(static_cast<uint8_t>(OpCode::OP_POP));

        node->thenBranch->accept(this);

        int elseJump = emitJump(static_cast<uint8_t>(OpCode::OP_JUMP));

        patchJump(thenJump);
        emitByte(static_cast<uint8_t>(OpCode::OP_POP));

        if (node->elseBranch) {
            node->elseBranch->accept(this);
        }
        patchJump(elseJump);
    }
    void visit(WhileStmt* node) override {
        int loopStart = chunk->code.size();
        loops.push_back({loopStart, scopeDepth, {}, {}});

        node->condition->accept(this);
        int exitJump = emitJump(static_cast<uint8_t>(OpCode::OP_JUMP_IF_FALSE));
        emitByte(static_cast<uint8_t>(OpCode::OP_POP));
        
        node->body->accept(this);
        
        LoopContext loop = loops.back();
        loops.pop_back();

        for (int continueJump : loop.continueJumps) {
            patchJump(continueJump);
        }

        emitLoop(loopStart);
        patchJump(exitJump);
        emitByte(static_cast<uint8_t>(OpCode::OP_POP));

        for (int breakJump : loop.breakJumps) {
            patchJump(breakJump);
        }
    }
    void visit(DoWhileStmt* node) override {
        int loopStart = chunk->code.size();
        loops.push_back({loopStart, scopeDepth, {}, {}});

        node->body->accept(this);

        LoopContext loop = loops.back();
        loops.pop_back();

        for (int continueJump : loop.continueJumps) {
            patchJump(continueJump);
        }

        node->condition->accept(this);
        int exitJump = emitJump(static_cast<uint8_t>(OpCode::OP_JUMP_IF_FALSE));
        emitByte(static_cast<uint8_t>(OpCode::OP_POP));
        emitLoop(loopStart);
        patchJump(exitJump);
        emitByte(static_cast<uint8_t>(OpCode::OP_POP));

        for (int breakJump : loop.breakJumps) {
            patchJump(breakJump);
        }
    }
    void visit(ReturnStmt* node) override {
        if (node->value) {
            node->value->accept(this);
        } else {
            emitByte(static_cast<uint8_t>(OpCode::OP_NIL));
        }
        emitByte(static_cast<uint8_t>(OpCode::OP_RETURN));
    }
    void visit(BreakStmt* node) override {
        if (loops.empty()) {
            std::cerr << "Cannot 'break' outside of a loop." << std::endl;
            return;
        }
        int loopScopeDepth = loops.back().scopeDepth;
        for (int i = locals.size() - 1; i >= 0 && locals[i].depth > loopScopeDepth; i--) {
            emitByte(static_cast<uint8_t>(OpCode::OP_POP));
        }
        int jump = emitJump(static_cast<uint8_t>(OpCode::OP_JUMP));
        loops.back().breakJumps.push_back(jump);
    }
    void visit(ContinueStmt* node) override {
        if (loops.empty()) {
            std::cerr << "Cannot 'continue' outside of a loop." << std::endl;
            return;
        }
        int loopScopeDepth = loops.back().scopeDepth;
        for (int i = locals.size() - 1; i >= 0 && locals[i].depth > loopScopeDepth; i--) {
            emitByte(static_cast<uint8_t>(OpCode::OP_POP));
        }
        int jump = emitJump(static_cast<uint8_t>(OpCode::OP_JUMP));
        loops.back().continueJumps.push_back(jump);
    }
    void visit(ForStmt* node) override {
        beginScope();
        if (node->initializer) node->initializer->accept(this);
        
        int loopStart = chunk->code.size();
        loops.push_back({loopStart, scopeDepth, {}, {}});

        int exitJump = -1;
        
        if (node->condition) {
            node->condition->accept(this);
            exitJump = emitJump(static_cast<uint8_t>(OpCode::OP_JUMP_IF_FALSE));
            emitByte(static_cast<uint8_t>(OpCode::OP_POP));
        }
        
        node->body->accept(this);
        
        LoopContext loop = loops.back();
        loops.pop_back();

        for (int continueJump : loop.continueJumps) {
            patchJump(continueJump);
        }
        
        if (node->increment) {
            node->increment->accept(this);
            emitByte(static_cast<uint8_t>(OpCode::OP_POP));
        }
        
        emitLoop(loopStart);
        
        if (exitJump != -1) {
            patchJump(exitJump);
            emitByte(static_cast<uint8_t>(OpCode::OP_POP));
        }

        for (int breakJump : loop.breakJumps) {
            patchJump(breakJump);
        }
        
        endScope();
    }
    void visit(ForeachStmt* node) override {
        beginScope();

        node->iterable->accept(this);
        locals.push_back({"<iterable>", scopeDepth});
        int iterableLocal = locals.size() - 1;

        emitBytes(static_cast<uint8_t>(OpCode::OP_CONSTANT), chunk->addConstant(0.0));
        locals.push_back({"<index>", scopeDepth});
        int indexLocal = locals.size() - 1;
        
        int loopStart = chunk->code.size();
        loops.push_back({loopStart, scopeDepth, {}, {}});

        emitBytes(static_cast<uint8_t>(OpCode::OP_GET_LOCAL), iterableLocal);
        emitBytes(static_cast<uint8_t>(OpCode::OP_GET_LOCAL), indexLocal);
        emitByte(static_cast<uint8_t>(OpCode::OP_ITER_HAS_NEXT));
        
        int exitJump = emitJump(static_cast<uint8_t>(OpCode::OP_JUMP_IF_FALSE));
        emitByte(static_cast<uint8_t>(OpCode::OP_POP));
        
        beginScope();
        
        emitBytes(static_cast<uint8_t>(OpCode::OP_GET_LOCAL), iterableLocal);
        emitBytes(static_cast<uint8_t>(OpCode::OP_GET_LOCAL), indexLocal);
        emitByte(static_cast<uint8_t>(OpCode::OP_INDEX_GET));
        
        locals.push_back({node->name.lexeme, scopeDepth});
        
        node->body->accept(this);
        
        endScope();
        
        LoopContext loop = loops.back();
        loops.pop_back();
        for (int continueJump : loop.continueJumps) {
            patchJump(continueJump);
        }
        
        emitBytes(static_cast<uint8_t>(OpCode::OP_GET_LOCAL), indexLocal);
        emitBytes(static_cast<uint8_t>(OpCode::OP_CONSTANT), chunk->addConstant(1.0));
        emitByte(static_cast<uint8_t>(OpCode::OP_ADD));
        emitBytes(static_cast<uint8_t>(OpCode::OP_SET_LOCAL), indexLocal);
        emitByte(static_cast<uint8_t>(OpCode::OP_POP));
        
        emitLoop(loopStart);
        
        patchJump(exitJump);
        emitByte(static_cast<uint8_t>(OpCode::OP_POP));
        
        for (int breakJump : loop.breakJumps) {
            patchJump(breakJump);
        }
        
        endScope();
    }
    void visit(SwitchStmt* node) override {
        node->expression->accept(this);

        std::vector<int> endJumps;

        for (auto& case_ : node->cases) {
            if (case_.value) {
                emitByte(static_cast<uint8_t>(OpCode::OP_DUP));
                case_.value->accept(this);
                emitByte(static_cast<uint8_t>(OpCode::OP_EQUAL));

                int nextCaseJump = emitJump(static_cast<uint8_t>(OpCode::OP_JUMP_IF_FALSE));
                emitByte(static_cast<uint8_t>(OpCode::OP_POP)); 

                for (auto* stmt : case_.body) {
                    stmt->accept(this);
                }

                endJumps.push_back(emitJump(static_cast<uint8_t>(OpCode::OP_JUMP)));

                patchJump(nextCaseJump);
                emitByte(static_cast<uint8_t>(OpCode::OP_POP)); 
            } else {
                for (auto* stmt : case_.body) {
                    stmt->accept(this);
                }
            }
        }

        for (int jump : endJumps) {
            patchJump(jump);
        }

        emitByte(static_cast<uint8_t>(OpCode::OP_POP));
    }
    void visit(UnaryExpr* node) override {
        node->right->accept(this);
        switch (node->op.type) {
            case TokenType::BANG: emitByte(static_cast<uint8_t>(OpCode::OP_NOT)); break;
            case TokenType::MINUS: emitByte(static_cast<uint8_t>(OpCode::OP_NEGATE)); break;
            default: break;
        }
    }
    void visit(ThisExpr* node) override {
        for (int i = locals.size() - 1; i >= 0; i--) {
            if (locals[i].name == "this") {
                emitBytes(static_cast<uint8_t>(OpCode::OP_GET_LOCAL), i);
                return;
            }
        }
    }
    void visit(SuperExpr* node) override {
        if (currentParentName.empty()) {
            std::cerr << "Cannot use 'super' in a class with no superclass." << std::endl;
            return;
        }
        for (int i = locals.size() - 1; i >= 0; i--) {
            if (locals[i].name == "this") {
                emitBytes(static_cast<uint8_t>(OpCode::OP_GET_LOCAL), i);
                break;
            }
        }
        emitBytes(static_cast<uint8_t>(OpCode::OP_GET_GLOBAL), chunk->addConstant(currentParentName));
        emitBytes(static_cast<uint8_t>(OpCode::OP_GET_SUPER), chunk->addConstant(node->method.lexeme));
    }
    void visit(GetExpr* node) override {
        node->object->accept(this);
        emitBytes(static_cast<uint8_t>(OpCode::OP_PROPERTY_GET), chunk->addConstant(node->name.lexeme));
    }
    void visit(SetExpr* node) override {
        node->object->accept(this);
        node->value->accept(this);
        emitBytes(static_cast<uint8_t>(OpCode::OP_PROPERTY_SET), chunk->addConstant(node->name.lexeme));
    }
    void visit(PostfixExpr* node) override {
        int arg = resolveLocal(node->name.lexeme);
        if (arg != -1) {
            emitBytes(static_cast<uint8_t>(OpCode::OP_GET_LOCAL), (uint8_t)arg);
            emitBytes(static_cast<uint8_t>(OpCode::OP_GET_LOCAL), (uint8_t)arg);
        } else {
            emitBytes(static_cast<uint8_t>(OpCode::OP_GET_GLOBAL), chunk->addConstant(node->name.lexeme));
            emitBytes(static_cast<uint8_t>(OpCode::OP_GET_GLOBAL), chunk->addConstant(node->name.lexeme));
        }
        
        emitBytes(static_cast<uint8_t>(OpCode::OP_CONSTANT), chunk->addConstant(1.0));
        
        if (node->op.type == TokenType::PLUS_PLUS) {
            emitByte(static_cast<uint8_t>(OpCode::OP_ADD));
        } else {
            emitByte(static_cast<uint8_t>(OpCode::OP_SUBTRACT));
        }
        
        if (arg != -1) {
            emitBytes(static_cast<uint8_t>(OpCode::OP_SET_LOCAL), (uint8_t)arg);
        } else {
            emitBytes(static_cast<uint8_t>(OpCode::OP_SET_GLOBAL), chunk->addConstant(node->name.lexeme));
        }
        emitByte(static_cast<uint8_t>(OpCode::OP_POP));
    }
    void visit(TernaryExpr* node) override {
        node->condition->accept(this);

        int thenJump = emitJump(static_cast<uint8_t>(OpCode::OP_JUMP_IF_FALSE));
        emitByte(static_cast<uint8_t>(OpCode::OP_POP));

        node->thenBranch->accept(this);

        int elseJump = emitJump(static_cast<uint8_t>(OpCode::OP_JUMP));

        patchJump(thenJump);
        emitByte(static_cast<uint8_t>(OpCode::OP_POP));

        node->elseBranch->accept(this);
        
        patchJump(elseJump);
    }
    void visit(ListExpr* node) override {
        for (auto& el : node->elements) {
            el->accept(this);
        }
        emitBytes(static_cast<uint8_t>(OpCode::OP_BUILD_LIST), node->elements.size());
    }
    void visit(IndexGetExpr* node) override {
        node->object->accept(this);
        node->index->accept(this);
        emitByte(static_cast<uint8_t>(OpCode::OP_INDEX_GET));
    }
    void visit(IndexSetExpr* node) override {
        node->object->accept(this);
        node->index->accept(this);
        node->value->accept(this);
        emitByte(static_cast<uint8_t>(OpCode::OP_INDEX_SET));
    }
    void visit(FunctionNode* node) override {
        auto func = std::make_shared<ObjFunction>();
        func->name = node->name;
        func->arity = node->params.size();
        func->chunk = std::make_shared<Chunk>();

        CompilerVisitor funcCompiler(func->chunk.get());
        
        funcCompiler.beginScope();
        for (const auto& param : node->params) {
            funcCompiler.locals.push_back({param, 1});
        }
        
        for (auto& stmt : node->body) {
            stmt->accept(&funcCompiler);
        }
        
        funcCompiler.emitByte(static_cast<uint8_t>(OpCode::OP_NIL));
        funcCompiler.emitByte(static_cast<uint8_t>(OpCode::OP_RETURN));
        
        emitConstant(func);
        emitBytes(static_cast<uint8_t>(OpCode::OP_DEFINE_GLOBAL), chunk->addConstant(node->name));
    }
    void visit(FieldDeclNode* node) override {}
    VMAccessModifier getVMAccessModifier(AccessModifier am) {
        if (am == AccessModifier::PRIVATE) return VMAccessModifier::PRIVATE;
        if (am == AccessModifier::PROTECTED) return VMAccessModifier::PROTECTED;
        return VMAccessModifier::PUBLIC;
    }

    void compileMethod(FunctionNode* node, ClassNode* classNode = nullptr) {
        auto func = std::make_shared<ObjFunction>();
        func->name = node->name;
        func->arity = node->params.size();
        func->chunk = std::make_shared<Chunk>();
        func->accessModifier = getVMAccessModifier(node->accessModifier);
        func->enclosingClassName = currentClassName;

        CompilerVisitor funcCompiler(func->chunk.get());
        
        funcCompiler.currentClassName = currentClassName;
        funcCompiler.currentParentName = currentParentName;
        
        funcCompiler.locals[0].name = "this";
        
        funcCompiler.beginScope();
        
        for (const auto& param : node->params) {
            funcCompiler.locals.push_back({param, 1});
        }

        if (node->name == "init" && classNode) {
            for (auto field : classNode->fields) {
                funcCompiler.emitBytes(static_cast<uint8_t>(OpCode::OP_GET_LOCAL), 0);
                if (field->initializer) {
                    field->initializer->accept(&funcCompiler);
                } else {
                    funcCompiler.emitByte(static_cast<uint8_t>(OpCode::OP_NIL));
                }
                funcCompiler.emitBytes(static_cast<uint8_t>(OpCode::OP_PROPERTY_SET), funcCompiler.chunk->addConstant(field->name));
                funcCompiler.emitByte(static_cast<uint8_t>(OpCode::OP_POP));
            }
        }
        
        for (auto& stmt : node->body) {
            stmt->accept(&funcCompiler);
        }
        
        if (node->name == "init") {
            funcCompiler.emitBytes(static_cast<uint8_t>(OpCode::OP_GET_LOCAL), 0);
            funcCompiler.emitByte(static_cast<uint8_t>(OpCode::OP_RETURN));
        } else {
            funcCompiler.emitByte(static_cast<uint8_t>(OpCode::OP_NIL));
            funcCompiler.emitByte(static_cast<uint8_t>(OpCode::OP_RETURN));
        }
        
        emitConstant(func);
        if (node->isStatic) {
            emitBytes(static_cast<uint8_t>(OpCode::OP_STATIC_METHOD), chunk->addConstant(node->name));
        } else if (node->isAbstract) {
            emitBytes(static_cast<uint8_t>(OpCode::OP_ABSTRACT_METHOD), chunk->addConstant(node->name));
        } else {
            emitBytes(static_cast<uint8_t>(OpCode::OP_METHOD), chunk->addConstant(node->name));
        }
    }

    void visit(ClassNode* node) override {
        std::string enclosingClassName = currentClassName;
        std::string enclosingParentName = currentParentName;
        currentClassName = node->name;
        currentParentName = node->parentName;

        if (node->isAbstract) {
            emitBytes(static_cast<uint8_t>(OpCode::OP_ABSTRACT_CLASS), chunk->addConstant(node->name));
        } else {
            emitBytes(static_cast<uint8_t>(OpCode::OP_CLASS), chunk->addConstant(node->name));
        }
        emitBytes(static_cast<uint8_t>(OpCode::OP_DEFINE_GLOBAL), chunk->addConstant(node->name));
        
        if (!node->parentName.empty()) {
            emitBytes(static_cast<uint8_t>(OpCode::OP_GET_GLOBAL), chunk->addConstant(node->name));
            emitBytes(static_cast<uint8_t>(OpCode::OP_GET_GLOBAL), chunk->addConstant(node->parentName));
            emitByte(static_cast<uint8_t>(OpCode::OP_INHERIT));
        }

        for (auto& traitName : node->interfaceNames) {
            emitBytes(static_cast<uint8_t>(OpCode::OP_GET_GLOBAL), chunk->addConstant(node->name));
            emitBytes(static_cast<uint8_t>(OpCode::OP_GET_GLOBAL), chunk->addConstant(traitName));
            emitByte(static_cast<uint8_t>(OpCode::OP_MIXIN));
        }
        
        emitBytes(static_cast<uint8_t>(OpCode::OP_GET_GLOBAL), chunk->addConstant(node->name));
        for (auto field : node->fields) {
            emitBytes(static_cast<uint8_t>(OpCode::OP_FIELD_MODIFIER), chunk->addConstant(field->name));
            emitByte(static_cast<uint8_t>(getVMAccessModifier(field->accessModifier)));
        }
        bool hasInit = false;
        for (auto& method : node->methods) {
            if (method->name == "init") hasInit = true;
            compileMethod(method, node);
        }
        if (!hasInit) {
            auto dummyInit = new FunctionNode("init", {}, {});
            compileMethod(dummyInit, node);
            delete dummyInit;
        }
        emitByte(static_cast<uint8_t>(OpCode::OP_POP));
        
        currentClassName = enclosingClassName;
        currentParentName = enclosingParentName;
    }
    void visit(InterfaceNode* node) override {
        emitBytes(static_cast<uint8_t>(OpCode::OP_CLASS), chunk->addConstant(node->name));
        emitBytes(static_cast<uint8_t>(OpCode::OP_DEFINE_GLOBAL), chunk->addConstant(node->name));
        
        emitBytes(static_cast<uint8_t>(OpCode::OP_GET_GLOBAL), chunk->addConstant(node->name));
        for (auto& method : node->methods) {
            compileMethod(method, nullptr);
        }
        emitByte(static_cast<uint8_t>(OpCode::OP_POP));
    }
    void visit(TraitNode* node) override {
        emitBytes(static_cast<uint8_t>(OpCode::OP_CLASS), chunk->addConstant(node->name));
        emitBytes(static_cast<uint8_t>(OpCode::OP_DEFINE_GLOBAL), chunk->addConstant(node->name));
        
        emitBytes(static_cast<uint8_t>(OpCode::OP_GET_GLOBAL), chunk->addConstant(node->name));
        for (auto& method : node->methods) {
            compileMethod(method);
        }
        emitByte(static_cast<uint8_t>(OpCode::OP_POP));
    }
    void visit(StaticGetExpr* node) override {
        int arg = resolveLocal(node->className.lexeme);
        if (arg != -1) {
            emitBytes(static_cast<uint8_t>(OpCode::OP_GET_LOCAL), (uint8_t)arg);
        } else {
            emitBytes(static_cast<uint8_t>(OpCode::OP_GET_GLOBAL), chunk->addConstant(node->className.lexeme));
        }
        emitBytes(static_cast<uint8_t>(OpCode::OP_PROPERTY_GET), chunk->addConstant(node->memberName.lexeme));
    }
    void visit(StaticCallExpr* node) override {
        int arg = resolveLocal(node->className.lexeme);
        if (arg != -1) {
            emitBytes(static_cast<uint8_t>(OpCode::OP_GET_LOCAL), (uint8_t)arg);
        } else {
            emitBytes(static_cast<uint8_t>(OpCode::OP_GET_GLOBAL), chunk->addConstant(node->className.lexeme));
        }
        emitBytes(static_cast<uint8_t>(OpCode::OP_PROPERTY_GET), chunk->addConstant(node->memberName.lexeme));
        
        for (auto& argExpr : node->arguments) {
            argExpr->accept(this);
        }
        emitBytes(static_cast<uint8_t>(OpCode::OP_CALL), node->arguments.size());
    }
    void visit(StaticSetExpr* node) override {
        node->value->accept(this);
        int arg = resolveLocal(node->className.lexeme);
        if (arg != -1) {
            emitBytes(static_cast<uint8_t>(OpCode::OP_GET_LOCAL), (uint8_t)arg);
        } else {
            emitBytes(static_cast<uint8_t>(OpCode::OP_GET_GLOBAL), chunk->addConstant(node->className.lexeme));
        }
        emitBytes(static_cast<uint8_t>(OpCode::OP_PROPERTY_SET), chunk->addConstant(node->memberName.lexeme));
    }
    void visit(LoadStmt* node) override {
        std::string path = node->filename.lexeme;
        if (path.size() >= 2 && path.front() == '"' && path.back() == '"') {
            path = path.substr(1, path.size() - 2);
        }
        
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Compile error: Could not load file '" << path << "'" << std::endl;
            return;
        }
        
        std::string source((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
                            
        Lexer lexer(source);
        Parser parser(lexer);
        ASTNode* ast = parser.parse();
        
        if (ast) {
            ast->accept(this);
            delete ast;
        }
    }

    void visit(DictExpr* node) override {
        for (auto& pair : node->elements) {
            pair.first->accept(this);
            pair.second->accept(this);
        }
        emitBytes(static_cast<uint8_t>(OpCode::OP_BUILD_MAP), node->elements.size());
    }
};

std::shared_ptr<ObjFunction> Compiler::compile(ASTNode* ast) {
    if (!ast) {
        ErrorHandling::reportError("AST is null");
        return nullptr;
    }

    auto script = std::make_shared<ObjFunction>();
    script->name = "<script>";
    script->chunk = std::make_shared<Chunk>();

    CompilerVisitor visitor(script->chunk.get());
    ast->accept(&visitor);

    visitor.emitByte(static_cast<uint8_t>(OpCode::OP_NIL));
    visitor.emitByte(static_cast<uint8_t>(OpCode::OP_RETURN));

    return script;
}
