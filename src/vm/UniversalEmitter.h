#pragma once
#include "JitEmitter.h"
#include "sljitLir.h"
#include <map>
#include <vector>
#include <cstring>
#include <iostream>
#include <string>

extern "C" double jit_invoke_helper(void* vm, const char* name, double* args, int argCount);
extern "C" double jit_index_get_helper(void* vm_ptr, double object_val, double index_val);
extern "C" double jit_index_set_helper(void* vm_ptr, double object_val, double index_val, double value_val);

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
        // f = return type (float), P = arg1 (void* vm), P = arg2 (double* args), W = arg3 (int argCount)
        sljit_emit_enter(compiler, 0, SLJIT_ARGS3(F64, W, P, W), 4 | SLJIT_ENTER_FLOAT(SLJIT_NUMBER_OF_FLOAT_REGISTERS), 3, 0);

        for (int j = 0; j < maxLocals; ++j) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                getFloatReg(j), 0, 
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
        }
    }

    void emitEpilogue(int maxLocals) override {
        for (int j = 0; j < maxLocals; ++j) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double), 
                getFloatReg(j), 0);
        }
        
        sljit_emit_return(compiler, SLJIT_MOV_F64, getFloatReg(0), 0);
    }

    void emitLoadConst(int stackOffset, double value) override {
        sljit_emit_fset64(compiler, getFloatReg(stackOffset), value);
    }

    void emitGetLocal(int stackOffset, int localSlot) override {
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
            getFloatReg(8 + stackOffset), 0, 
            getFloatReg(localSlot - 1), 0);
    }

    void emitSetLocal(int localSlot, int stackOffset) override {
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
            getFloatReg(localSlot - 1), 0, 
            getFloatReg(stackOffset), 0);
    }

    void emitMove(int targetOffset, int srcOffset) override {
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
            getFloatReg(targetOffset), 0, 
            getFloatReg(srcOffset), 0);
    }

    void emitAdd(int targetOffset, int srcOffset) override {
        sljit_emit_fop2(compiler, SLJIT_ADD_F64, 
            getFloatReg(targetOffset), 0, 
            getFloatReg(targetOffset), 0, 
            getFloatReg(srcOffset), 0);
    }

    void emitSub(int targetOffset, int srcOffset) override {
        sljit_emit_fop2(compiler, SLJIT_SUB_F64, 
            getFloatReg(targetOffset), 0, 
            getFloatReg(targetOffset), 0, 
            getFloatReg(srcOffset), 0);
    }

    void emitMul(int targetOffset, int srcOffset) override {
        sljit_emit_fop2(compiler, SLJIT_MUL_F64, 
            getFloatReg(targetOffset), 0, 
            getFloatReg(targetOffset), 0, 
            getFloatReg(srcOffset), 0);
    }

    void emitDiv(int targetOffset, int srcOffset) override {
        sljit_emit_fop2(compiler, SLJIT_DIV_F64, 
            getFloatReg(targetOffset), 0, 
            getFloatReg(targetOffset), 0, 
            getFloatReg(srcOffset), 0);
    }

    void emitBitAnd(int targetOffset, int srcOffset) override {
        sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R0, 0, getFloatReg(targetOffset), 0);
        sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R1, 0, getFloatReg(srcOffset), 0);
        sljit_emit_op2(compiler, SLJIT_AND, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
        sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_SW, getFloatReg(targetOffset), 0, SLJIT_R0, 0);
    }

    void emitBitOr(int targetOffset, int srcOffset) override {
        sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R0, 0, getFloatReg(targetOffset), 0);
        sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R1, 0, getFloatReg(srcOffset), 0);
        sljit_emit_op2(compiler, SLJIT_OR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
        sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_SW, getFloatReg(targetOffset), 0, SLJIT_R0, 0);
    }

    void emitBitXor(int targetOffset, int srcOffset) override {
        sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R0, 0, getFloatReg(targetOffset), 0);
        sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R1, 0, getFloatReg(srcOffset), 0);
        sljit_emit_op2(compiler, SLJIT_XOR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
        sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_SW, getFloatReg(targetOffset), 0, SLJIT_R0, 0);
    }

    void emitBitNot(int targetOffset) override {
        sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R0, 0, getFloatReg(targetOffset), 0);
        sljit_emit_op2(compiler, SLJIT_XOR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, -1);
        sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_SW, getFloatReg(targetOffset), 0, SLJIT_R0, 0);
    }

    void emitBitShl(int targetOffset, int srcOffset) override {
        sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R0, 0, getFloatReg(targetOffset), 0);
        sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R1, 0, getFloatReg(srcOffset), 0);
        sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
        sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_SW, getFloatReg(targetOffset), 0, SLJIT_R0, 0);
    }

    void emitBitShr(int targetOffset, int srcOffset) override {
        sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R0, 0, getFloatReg(targetOffset), 0);
        sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R1, 0, getFloatReg(srcOffset), 0);
        sljit_emit_op2(compiler, SLJIT_ASHR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
        sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_SW, getFloatReg(targetOffset), 0, SLJIT_R0, 0);
    }

    void emitCmpLt(int targetOffset, int srcOffset) override {
        struct sljit_jump* jumpLess = sljit_emit_fcmp(compiler, SLJIT_F_LESS, 
            getFloatReg(targetOffset), 0, 
            getFloatReg(srcOffset), 0);
        
        sljit_emit_fset64(compiler, getFloatReg(targetOffset), 0.0);
        struct sljit_jump* jumpDone = sljit_emit_jump(compiler, SLJIT_JUMP);
        
        sljit_set_label(jumpLess, sljit_emit_label(compiler));
        sljit_emit_fset64(compiler, getFloatReg(targetOffset), 1.0);
        
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
            getFloatReg(stackOffset), 0, 
            SLJIT_FR0, 0);
            
        if (labels.count(targetByteCodeIndex)) {
            sljit_set_label(jump, labels[targetByteCodeIndex]);
        } else {
            unresolvedJumps[targetByteCodeIndex].push_back(jump);
        }
    }

    void emitCallGlobal(const std::string& name, int targetOffset, int argCount) override {
        // Save all active stack slots to memory (SLJIT_S1 array)
        for (int j = 0; j <= targetOffset; ++j) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double), 
                getFloatReg(j), 0);
        }

        // Save arguments to memory
        for (int j = 0; j < argCount; ++j) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                SLJIT_MEM1(SLJIT_S1), (targetOffset + 1 + j) * sizeof(double), 
                getFloatReg(targetOffset + 1 + j), 0);
        }

        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);

        char* cname = strdup(name.c_str());
        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)cname);

        sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R2, 0, SLJIT_S1, 0, SLJIT_IMM, (targetOffset + 1) * sizeof(double));
        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, argCount);

        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS4(F64, W, W, P, W), SLJIT_IMM, (sljit_sw)jit_invoke_helper);

        // Save the return value (FR0) into the memory slot for targetOffset
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
            SLJIT_MEM1(SLJIT_S1), targetOffset * sizeof(double), 
            SLJIT_FR0, 0);

        // Restore active stack slots from memory (C function call clobbers FR registers)
        for (int j = 0; j <= targetOffset; ++j) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                getFloatReg(j), 0, 
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
        }
    }

    void emitIndexGet(int targetOffset, int objectOffset, int indexOffset) override {
        // Save all active stack slots to memory (SLJIT_S1 array)
        for (int j = 0; j <= targetOffset; ++j) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double), 
                getFloatReg(j), 0);
        }

        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0); // vm pointer
        
        // Pass object_val (R1) and index_val (R2) as float parameters.
        // wait, helper is: double jit_index_get_helper(void*, double, double)
        // Calling convention for F64 args: FR0 = arg2, FR1 = arg3
        // wait, the helper takes F64.
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, getFloatReg(objectOffset), 0);
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, getFloatReg(indexOffset), 0);

        // Define prototype for icall: double (void*, double, double) -> F64, W, F64, F64
        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS3(F64, W, F64, F64), SLJIT_IMM, (sljit_sw)jit_index_get_helper);

        // Save the return value (FR0) into memory
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
            SLJIT_MEM1(SLJIT_S1), targetOffset * sizeof(double), 
            SLJIT_FR0, 0);

        // Restore active stack slots from memory
        for (int j = 0; j <= targetOffset; ++j) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                getFloatReg(j), 0, 
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
        }
    }

    void emitIndexSet(int objectOffset, int indexOffset, int valueOffset) override {
        // Save all active stack slots
        int maxOffset = std::max(std::max(objectOffset, indexOffset), valueOffset);
        for (int j = 0; j <= maxOffset; ++j) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double), 
                getFloatReg(j), 0);
        }

        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, getFloatReg(objectOffset), 0);
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, getFloatReg(indexOffset), 0);
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR2, 0, getFloatReg(valueOffset), 0);

        // double jit_index_set_helper(void*, double, double, double) -> F64, W, F64, F64, F64
        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS4(F64, W, F64, F64, F64), SLJIT_IMM, (sljit_sw)jit_index_set_helper);

        // FR0 holds the return value (value_val), we don't strictly need to write it anywhere but we can replace valueOffset or just ignore.
        
        // Restore active stack slots
        for (int j = 0; j <= maxOffset; ++j) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                getFloatReg(j), 0, 
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
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
    void emitCmpEq(int targetOffset, int srcOffset) override {
        struct sljit_jump* jumpEq = sljit_emit_fcmp(compiler, SLJIT_F_EQUAL, 
            getFloatReg(targetOffset), 0, 
            getFloatReg(srcOffset), 0);
        sljit_emit_fset64(compiler, getFloatReg(targetOffset), 0.0);
        struct sljit_jump* jumpDone = sljit_emit_jump(compiler, SLJIT_JUMP);
        sljit_set_label(jumpEq, sljit_emit_label(compiler));
        sljit_emit_fset64(compiler, getFloatReg(targetOffset), 1.0);
        sljit_set_label(jumpDone, sljit_emit_label(compiler));
    }

    void emitCmpGt(int targetOffset, int srcOffset) override {
        struct sljit_jump* jumpGt = sljit_emit_fcmp(compiler, SLJIT_F_GREATER, 
            getFloatReg(targetOffset), 0, 
            getFloatReg(srcOffset), 0);
        sljit_emit_fset64(compiler, getFloatReg(targetOffset), 0.0);
        struct sljit_jump* jumpDone = sljit_emit_jump(compiler, SLJIT_JUMP);
        sljit_set_label(jumpGt, sljit_emit_label(compiler));
        sljit_emit_fset64(compiler, getFloatReg(targetOffset), 1.0);
        sljit_set_label(jumpDone, sljit_emit_label(compiler));
    }

    void emitNot(int targetOffset) override {
        sljit_emit_fset64(compiler, SLJIT_FR0, 0.0);
        struct sljit_jump* jumpEq = sljit_emit_fcmp(compiler, SLJIT_F_EQUAL, 
            getFloatReg(targetOffset), 0, 
            SLJIT_FR0, 0);
        sljit_emit_fset64(compiler, getFloatReg(targetOffset), 0.0);
        struct sljit_jump* jumpDone = sljit_emit_jump(compiler, SLJIT_JUMP);
        sljit_set_label(jumpEq, sljit_emit_label(compiler));
        sljit_emit_fset64(compiler, getFloatReg(targetOffset), 1.0);
        sljit_set_label(jumpDone, sljit_emit_label(compiler));
    }

    void emitNegate(int targetOffset) override {
        sljit_emit_fset64(compiler, SLJIT_FR0, 0.0);
        sljit_emit_fop2(compiler, SLJIT_SUB_F64, 
            getFloatReg(targetOffset), 0, 
            SLJIT_FR0, 0, 
            getFloatReg(targetOffset), 0);
    }
    
    void emitReturn(int stackOffset) {
        printf("JIT: emitReturn generated for stackOffset %d\n", stackOffset);
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, getFloatReg(stackOffset), 0);
        sljit_emit_return(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0);
    }

    void emitCmpNe(int, int) override {}
    void emitCmpLe(int, int) override {}
    void emitCmpGe(int, int) override {}
};
