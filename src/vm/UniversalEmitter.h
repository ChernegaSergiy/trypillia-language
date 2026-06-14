#pragma once
#include "JitEmitter.h"
#include "sljitLir.h"
#include <map>
#include <vector>
#include <cstring>

class UniversalEmitter : public JitEmitter {
private:
    struct sljit_compiler* compiler;
    std::map<size_t, struct sljit_label*> labels;
    std::map<size_t, std::vector<struct sljit_jump*>> unresolvedJumps;

    sljit_s32 getFloatReg(int offset) {
        return SLJIT_FR0 + offset;
    }

    struct sljit_label* getLabel(size_t index) {
        if (labels.find(index) == labels.end()) {
            labels[index] = sljit_emit_label(compiler);
        }
        return labels[index];
    }

public:
    UniversalEmitter() {
        compiler = sljit_create_compiler(NULL);
    }

    ~UniversalEmitter() {
        if (compiler) sljit_free_compiler(compiler);
    }

    void emitPrologue(int maxLocals) override {
        // f = return type (float), P = arg1 (pointer)
        sljit_emit_enter(compiler, 0, SLJIT_ARGS1(F64, P), 3 | SLJIT_ENTER_FLOAT(SLJIT_NUMBER_OF_FLOAT_REGISTERS), 3, 0);

        for (int j = 0; j < maxLocals; ++j) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                getFloatReg(j), 0, 
                SLJIT_MEM1(SLJIT_S0), j * sizeof(double));
        }
    }

    void emitEpilogue(int maxLocals) override {
        for (int j = 0; j < maxLocals; ++j) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                SLJIT_MEM1(SLJIT_S0), j * sizeof(double), 
                getFloatReg(j), 0);
        }
        
        sljit_emit_return(compiler, SLJIT_MOV_F64, getFloatReg(8), 0);
    }

    void emitLoadConst(int stackOffset, double value) override {
        sljit_emit_fset64(compiler, getFloatReg(8 + stackOffset), value);
    }

    void emitGetLocal(int stackOffset, int localSlot) override {
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
            getFloatReg(8 + stackOffset), 0, 
            getFloatReg(localSlot - 1), 0);
    }

    void emitSetLocal(int localSlot, int stackOffset) override {
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
            getFloatReg(localSlot - 1), 0, 
            getFloatReg(8 + stackOffset), 0);
    }

    void emitMove(int targetOffset, int srcOffset) override {
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
            getFloatReg(8 + targetOffset), 0, 
            getFloatReg(8 + srcOffset), 0);
    }

    void emitAdd(int targetOffset, int srcOffset) override {
        sljit_emit_fop2(compiler, SLJIT_ADD_F64, 
            getFloatReg(8 + targetOffset), 0, 
            getFloatReg(8 + targetOffset), 0, 
            getFloatReg(8 + srcOffset), 0);
    }

    void emitSub(int targetOffset, int srcOffset) override {
        sljit_emit_fop2(compiler, SLJIT_SUB_F64, 
            getFloatReg(8 + targetOffset), 0, 
            getFloatReg(8 + targetOffset), 0, 
            getFloatReg(8 + srcOffset), 0);
    }

    void emitMul(int targetOffset, int srcOffset) override {
        sljit_emit_fop2(compiler, SLJIT_MUL_F64, 
            getFloatReg(8 + targetOffset), 0, 
            getFloatReg(8 + targetOffset), 0, 
            getFloatReg(8 + srcOffset), 0);
    }

    void emitDiv(int targetOffset, int srcOffset) override {
        sljit_emit_fop2(compiler, SLJIT_DIV_F64, 
            getFloatReg(8 + targetOffset), 0, 
            getFloatReg(8 + targetOffset), 0, 
            getFloatReg(8 + srcOffset), 0);
    }

    void emitCmpLt(int targetOffset, int srcOffset) override {
        struct sljit_jump* jumpLess = sljit_emit_fcmp(compiler, SLJIT_F_LESS, 
            getFloatReg(8 + targetOffset), 0, 
            getFloatReg(8 + srcOffset), 0);
        
        sljit_emit_fset64(compiler, getFloatReg(8 + targetOffset), 0.0);
        struct sljit_jump* jumpDone = sljit_emit_jump(compiler, SLJIT_JUMP);
        
        sljit_set_label(jumpLess, sljit_emit_label(compiler));
        sljit_emit_fset64(compiler, getFloatReg(8 + targetOffset), 1.0);
        
        sljit_set_label(jumpDone, sljit_emit_label(compiler));
    }

    void emitJump(size_t targetByteCodeIndex) override {
        struct sljit_jump* jump = sljit_emit_jump(compiler, SLJIT_JUMP);
        if (labels.count(targetByteCodeIndex)) {
            sljit_set_label(jump, labels[targetByteCodeIndex]);
        } else {
            unresolvedJumps[targetByteCodeIndex].push_back(jump);
        }
    }

    void emitJumpIfFalse(int stackOffset, size_t targetByteCodeIndex) override {
        sljit_emit_fset64(compiler, SLJIT_FR0, 0.0);
        struct sljit_jump* jump = sljit_emit_fcmp(compiler, SLJIT_F_EQUAL, 
            getFloatReg(8 + stackOffset), 0, 
            SLJIT_FR0, 0);
            
        if (labels.count(targetByteCodeIndex)) {
            sljit_set_label(jump, labels[targetByteCodeIndex]);
        } else {
            unresolvedJumps[targetByteCodeIndex].push_back(jump);
        }
    }

    void bindLabel(size_t byteCodeIndex) override {
        struct sljit_label* label = getLabel(byteCodeIndex);
        if (unresolvedJumps.count(byteCodeIndex)) {
            for (auto* jump : unresolvedJumps[byteCodeIndex]) {
                sljit_set_label(jump, label);
            }
            unresolvedJumps.erase(byteCodeIndex);
        }
    }

    JitFunc finalize() override {
        void* code = sljit_generate_code(compiler, 0, NULL);
        return reinterpret_cast<JitFunc>(code);
    }
    
    void emitCmpEq(int, int) override {}
    void emitCmpNe(int, int) override {}
    void emitCmpLe(int, int) override {}
    void emitCmpGt(int, int) override {}
    void emitCmpGe(int, int) override {}
};
