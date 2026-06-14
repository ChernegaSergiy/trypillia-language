#pragma once
#include "JitEmitter.h"
#include "sljitLir.h"
#include <map>
#include <vector>
#include <cstring>
#include <iostream>
#include <string>

extern "C" double jit_call_helper(void* vm_ptr, double callee_val, double* args, int argCount);
extern "C" double jit_get_global_helper(void* vm_ptr, const char* name);
extern "C" void jit_set_global_helper(void* vm_ptr, const char* name, double val_d);
extern "C" double jit_index_get_helper(void* vm_ptr, double object_val, double index_val);
extern "C" double jit_index_set_helper(void* vm_ptr, double object_val, double index_val, double value_val);
extern "C" double jit_mod_helper(double a, double b);
extern "C" double jit_build_list_helper(double* args, int count);
extern "C" double jit_build_map_helper(double* args, int count);
extern "C" double jit_property_get_helper(void* vm_ptr, double object_val, const char* name);
extern "C" double jit_property_set_helper(void* vm_ptr, double object_val, const char* name, double value_val);
extern "C" double jit_iter_has_next_helper(double index_val, double iterable_val);
extern "C" double jit_create_class_helper(void* vm, const char* name);
extern "C" double jit_create_abstract_class_helper(void* vm, const char* name);
extern "C" double jit_bind_method_helper(double class_val, double method_val, const char* name, int isAbstract);
extern "C" double jit_bind_static_method_helper(double class_val, double method_val, const char* name);
extern "C" void jit_inherit_helper(double subclass_val, double superclass_val);
extern "C" void jit_mixin_helper(double target_val, double mixin_val);
extern "C" double jit_get_super_helper(double receiver_val, double superclass_val, const char* name);
extern "C" void jit_field_modifier_helper(double class_val, const char* name, int modifier);
extern "C" double jit_create_closure_helper(void* vm, double func_val, double* upvalue_data, int upvalue_count);
extern "C" double jit_get_upvalue_helper(void* vm_ptr, int slot);
extern "C" void jit_set_upvalue_helper(void* vm_ptr, int slot, double val);
extern "C" void jit_close_upvalue_helper(void* vm_ptr, double* addr);

class UniversalEmitter : public JitEmitter {
private:
    struct sljit_compiler* compiler;
    int funcArity;
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
    UniversalEmitter(int arity = 0) : funcArity(arity) {
        compiler = sljit_create_compiler(NULL);
    }

    ~UniversalEmitter() {
        if (compiler) sljit_free_compiler(compiler);
    }

    std::set<int> capturedSlots;
    void setCapturedLocals(const std::vector<int>& slots) override {
        capturedSlots.clear();
        for (int s : slots) capturedSlots.insert(s);
    }

    void emitPrologue(int maxLocals) override {
        // f = return type (float), P = arg1 (void* vm), P = arg2 (double* args), W = arg3 (int argCount)
        sljit_emit_enter(compiler, 0, SLJIT_ARGS3(F64, W, P, W), 4 | SLJIT_ENTER_FLOAT(SLJIT_NUMBER_OF_FLOAT_REGISTERS), 3, 0);

        for (int j = 0; j < maxLocals; ++j) {
            if (!capturedSlots.count(j)) {
                sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                    getFloatReg(j), 0, 
                    SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
            }
        }
    }

    void emitEpilogue(int maxLocals) override {
        for (int j = 0; j < maxLocals; ++j) {
            if (!capturedSlots.count(j)) {
                sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                    SLJIT_MEM1(SLJIT_S1), j * sizeof(double), 
                    getFloatReg(j), 0);
            }
        }
        
        sljit_emit_return(compiler, SLJIT_MOV_F64, getFloatReg(0), 0);
    }

    void emitLoadConst(int stackOffset, double value) override {
        sljit_emit_fset64(compiler, getFloatReg(8 + stackOffset), value);
    }

    void emitGetLocal(int stackOffset, int localSlot) override {
        if (capturedSlots.count(localSlot)) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                getFloatReg(8 + stackOffset), 0, 
                SLJIT_MEM1(SLJIT_S1), localSlot * sizeof(double));
        } else {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                getFloatReg(8 + stackOffset), 0, 
                getFloatReg(localSlot), 0);
        }
    }

    void emitSetLocal(int localSlot, int stackOffset) override {
        if (capturedSlots.count(localSlot)) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                SLJIT_MEM1(SLJIT_S1), localSlot * sizeof(double), 
                getFloatReg(8 + stackOffset), 0);
        } else {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                getFloatReg(localSlot), 0, 
                getFloatReg(8 + stackOffset), 0);
        }
    }

    void emitMove(int targetOffset, int srcOffset) override {
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
            getFloatReg(8 + targetOffset), 0, 
            getFloatReg(8 + srcOffset), 0);
    }

    void emitReturnValue(int stackOffset) {
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
            getFloatReg(0), 0, 
            getFloatReg(8 + stackOffset), 0);
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

    void emitBitAnd(int targetOffset, int srcOffset) override {
        sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R0, 0, getFloatReg(8 + targetOffset), 0);
        sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R1, 0, getFloatReg(8 + srcOffset), 0);
        sljit_emit_op2(compiler, SLJIT_AND, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
        sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_SW, getFloatReg(8 + targetOffset), 0, SLJIT_R0, 0);
    }

    void emitBitOr(int targetOffset, int srcOffset) override {
        sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R0, 0, getFloatReg(8 + targetOffset), 0);
        sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R1, 0, getFloatReg(8 + srcOffset), 0);
        sljit_emit_op2(compiler, SLJIT_OR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
        sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_SW, getFloatReg(8 + targetOffset), 0, SLJIT_R0, 0);
    }

    void emitBitXor(int targetOffset, int srcOffset) override {
        sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R0, 0, getFloatReg(8 + targetOffset), 0);
        sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R1, 0, getFloatReg(8 + srcOffset), 0);
        sljit_emit_op2(compiler, SLJIT_XOR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
        sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_SW, getFloatReg(8 + targetOffset), 0, SLJIT_R0, 0);
    }

    void emitBitNot(int targetOffset) override {
        sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R0, 0, getFloatReg(8 + targetOffset), 0);
        sljit_emit_op2(compiler, SLJIT_XOR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, -1);
        sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_SW, getFloatReg(8 + targetOffset), 0, SLJIT_R0, 0);
    }

    void emitBitShl(int targetOffset, int srcOffset) override {
        sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R0, 0, getFloatReg(8 + targetOffset), 0);
        sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R1, 0, getFloatReg(8 + srcOffset), 0);
        sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
        sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_SW, getFloatReg(8 + targetOffset), 0, SLJIT_R0, 0);
    }

    void emitBitShr(int targetOffset, int srcOffset) override {
        sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R0, 0, getFloatReg(8 + targetOffset), 0);
        sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R1, 0, getFloatReg(8 + srcOffset), 0);
        sljit_emit_op2(compiler, SLJIT_ASHR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
        sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_SW, getFloatReg(8 + targetOffset), 0, SLJIT_R0, 0);
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

    void emitCallDynamic(int targetOffset, int calleeOffset, int argCount) override {
        for (int j = 0; j <= targetOffset; ++j) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double), 
                getFloatReg(8 + j), 0);
        }

        for (int j = 0; j < argCount; ++j) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                SLJIT_MEM1(SLJIT_S1), (targetOffset + 1 + j) * sizeof(double), 
                getFloatReg(8 + targetOffset + 1 + j), 0);
        }

        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, getFloatReg(8 + calleeOffset), 0);

        sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S1, 0, SLJIT_IMM, (targetOffset + 1) * sizeof(double));
        
        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, argCount);

        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS4(F64, W, F64, P, W), SLJIT_IMM, (sljit_sw)jit_call_helper);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
            SLJIT_MEM1(SLJIT_S1), targetOffset * sizeof(double), 
            SLJIT_FR0, 0);

        for (int j = 0; j <= targetOffset; ++j) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                getFloatReg(8 + j), 0, 
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
        }
    }

    void emitGetGlobal(const std::string& name, int targetOffset) override {
        for (int j = 0; j <= targetOffset; ++j) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S1), j * sizeof(double), getFloatReg(8 + j), 0);
        }

        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);

        char* cname = strdup(name.c_str());
        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)cname);

        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS2(F64, W, W), SLJIT_IMM, (sljit_sw)jit_get_global_helper);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S1), targetOffset * sizeof(double), SLJIT_FR0, 0);

        for (int j = 0; j <= targetOffset; ++j) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, getFloatReg(8 + j), 0, SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
        }
    }

    void emitSetGlobal(const std::string& name, int sourceOffset) override {
        for (int j = 0; j <= sourceOffset; ++j) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S1), j * sizeof(double), getFloatReg(8 + j), 0);
        }

        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);

        char* cname = strdup(name.c_str());
        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)cname);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, getFloatReg(8 + sourceOffset), 0);

        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS3V(W, W, F64), SLJIT_IMM, (sljit_sw)jit_set_global_helper);

        for (int j = 0; j <= sourceOffset; ++j) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, getFloatReg(8 + j), 0, SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
        }
    }

    void emitIndexGet(int targetOffset, int objectOffset, int indexOffset) override {
        for (int j = 0; j <= targetOffset; ++j) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double), 
                getFloatReg(8 + j), 0);
        }

        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
        
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, getFloatReg(8 + objectOffset), 0);
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, getFloatReg(8 + indexOffset), 0);

        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS3(F64, W, F64, F64), SLJIT_IMM, (sljit_sw)jit_index_get_helper);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
            SLJIT_MEM1(SLJIT_S1), targetOffset * sizeof(double), 
            SLJIT_FR0, 0);

        for (int j = 0; j <= targetOffset; ++j) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                getFloatReg(8 + j), 0, 
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
        }
    }

    void emitIndexSet(int objectOffset, int indexOffset, int valueOffset) override {
        int maxOffset = std::max(std::max(objectOffset, indexOffset), valueOffset);
        for (int j = 0; j <= maxOffset; ++j) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double), 
                getFloatReg(8 + j), 0);
        }

        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, getFloatReg(8 + objectOffset), 0);
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, getFloatReg(8 + indexOffset), 0);
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR2, 0, getFloatReg(8 + valueOffset), 0);

        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS4(F64, W, F64, F64, F64), SLJIT_IMM, (sljit_sw)jit_index_set_helper);
        
        for (int j = 0; j <= maxOffset; ++j) {
            sljit_emit_fop1(compiler, SLJIT_MOV_F64, 
                getFloatReg(8 + j), 0, 
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
            getFloatReg(8 + targetOffset), 0, 
            getFloatReg(8 + srcOffset), 0);
        sljit_emit_fset64(compiler, getFloatReg(8 + targetOffset), 0.0);
        struct sljit_jump* jumpDone = sljit_emit_jump(compiler, SLJIT_JUMP);
        sljit_set_label(jumpEq, sljit_emit_label(compiler));
        sljit_emit_fset64(compiler, getFloatReg(8 + targetOffset), 1.0);
        sljit_set_label(jumpDone, sljit_emit_label(compiler));
    }

    void emitCmpGt(int targetOffset, int srcOffset) override {
        struct sljit_jump* jumpGt = sljit_emit_fcmp(compiler, SLJIT_F_GREATER, 
            getFloatReg(8 + targetOffset), 0, 
            getFloatReg(8 + srcOffset), 0);
        sljit_emit_fset64(compiler, getFloatReg(8 + targetOffset), 0.0);
        struct sljit_jump* jumpDone = sljit_emit_jump(compiler, SLJIT_JUMP);
        sljit_set_label(jumpGt, sljit_emit_label(compiler));
        sljit_emit_fset64(compiler, getFloatReg(8 + targetOffset), 1.0);
        sljit_set_label(jumpDone, sljit_emit_label(compiler));
    }

    void emitNot(int targetOffset) override {
        sljit_emit_fset64(compiler, SLJIT_FR0, 0.0);
        struct sljit_jump* jumpEq = sljit_emit_fcmp(compiler, SLJIT_F_EQUAL, 
            getFloatReg(8 + targetOffset), 0, 
            SLJIT_FR0, 0);
        sljit_emit_fset64(compiler, getFloatReg(8 + targetOffset), 0.0);
        struct sljit_jump* jumpDone = sljit_emit_jump(compiler, SLJIT_JUMP);
        sljit_set_label(jumpEq, sljit_emit_label(compiler));
        sljit_emit_fset64(compiler, getFloatReg(8 + targetOffset), 1.0);
        sljit_set_label(jumpDone, sljit_emit_label(compiler));
    }

    void emitNegate(int targetOffset) override {
        sljit_emit_fset64(compiler, SLJIT_FR0, 0.0);
        sljit_emit_fop2(compiler, SLJIT_SUB_F64, 
            getFloatReg(8 + targetOffset), 0, 
            SLJIT_FR0, 0, 
            getFloatReg(8 + targetOffset), 0);
    }
    
    void emitReturn(int stackOffset) {
        printf("JIT: emitReturn generated for stackOffset %d\n", stackOffset);
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, getFloatReg(8 + stackOffset), 0);
        sljit_emit_return(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0);
    }

    void emitCmpNe(int targetOffset, int srcOffset) override {
        struct sljit_jump* jumpNe = sljit_emit_fcmp(compiler, SLJIT_F_NOT_EQUAL,
            getFloatReg(8 + targetOffset), 0,
            getFloatReg(8 + srcOffset), 0);
        sljit_emit_fset64(compiler, getFloatReg(8 + targetOffset), 0.0);
        struct sljit_jump* jumpDone = sljit_emit_jump(compiler, SLJIT_JUMP);
        sljit_set_label(jumpNe, sljit_emit_label(compiler));
        sljit_emit_fset64(compiler, getFloatReg(8 + targetOffset), 1.0);
        sljit_set_label(jumpDone, sljit_emit_label(compiler));
    }

    void emitCmpLe(int targetOffset, int srcOffset) override {
        struct sljit_jump* jumpLe = sljit_emit_fcmp(compiler, SLJIT_F_LESS_EQUAL,
            getFloatReg(8 + targetOffset), 0,
            getFloatReg(8 + srcOffset), 0);
        sljit_emit_fset64(compiler, getFloatReg(8 + targetOffset), 0.0);
        struct sljit_jump* jumpDone = sljit_emit_jump(compiler, SLJIT_JUMP);
        sljit_set_label(jumpLe, sljit_emit_label(compiler));
        sljit_emit_fset64(compiler, getFloatReg(8 + targetOffset), 1.0);
        sljit_set_label(jumpDone, sljit_emit_label(compiler));
    }

    void emitCmpGe(int targetOffset, int srcOffset) override {
        struct sljit_jump* jumpGe = sljit_emit_fcmp(compiler, SLJIT_F_GREATER_EQUAL,
            getFloatReg(8 + targetOffset), 0,
            getFloatReg(8 + srcOffset), 0);
        sljit_emit_fset64(compiler, getFloatReg(8 + targetOffset), 0.0);
        struct sljit_jump* jumpDone = sljit_emit_jump(compiler, SLJIT_JUMP);
        sljit_set_label(jumpGe, sljit_emit_label(compiler));
        sljit_emit_fset64(compiler, getFloatReg(8 + targetOffset), 1.0);
        sljit_set_label(jumpDone, sljit_emit_label(compiler));
    }

    void emitMod(int targetOffset, int srcOffset) override {
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, getFloatReg(8 + targetOffset), 0);
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, getFloatReg(8 + srcOffset), 0);
        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS2(F64, F64, F64), SLJIT_IMM, (sljit_sw)jit_mod_helper);
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, getFloatReg(8 + targetOffset), 0, SLJIT_FR0, 0);
    }

    void emitBuildList(int targetOffset, int count) override {
        int maxOffset = targetOffset + count - 1;
        for (int j = 0; j <= maxOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double),
                getFloatReg(8 + j), 0);

        sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S1, 0, SLJIT_IMM, targetOffset * sizeof(double));
        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, count);
        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS2(F64, P, W), SLJIT_IMM, (sljit_sw)jit_build_list_helper);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64,
            SLJIT_MEM1(SLJIT_S1), targetOffset * sizeof(double),
            SLJIT_FR0, 0);

        for (int j = 0; j <= maxOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                getFloatReg(8 + j), 0,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
    }

    void emitBuildMap(int targetOffset, int count) override {
        int maxOffset = targetOffset + (count * 2) - 1;
        for (int j = 0; j <= maxOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double),
                getFloatReg(8 + j), 0);

        sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S1, 0, SLJIT_IMM, targetOffset * sizeof(double));
        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, count);
        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS2(F64, P, W), SLJIT_IMM, (sljit_sw)jit_build_map_helper);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64,
            SLJIT_MEM1(SLJIT_S1), targetOffset * sizeof(double),
            SLJIT_FR0, 0);

        for (int j = 0; j <= maxOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                getFloatReg(8 + j), 0,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
    }

    void emitPropertyGet(int objectOffset, const std::string& name) override {
        for (int j = 0; j <= objectOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double),
                getFloatReg(8 + j), 0);

        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, getFloatReg(8 + objectOffset), 0);
        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)strdup(name.c_str()));
        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS3(F64, W, F64, W), SLJIT_IMM, (sljit_sw)jit_property_get_helper);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64,
            SLJIT_MEM1(SLJIT_S1), objectOffset * sizeof(double),
            SLJIT_FR0, 0);

        for (int j = 0; j <= objectOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                getFloatReg(8 + j), 0,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
    }

    void emitPropertySet(int objectOffset, const std::string& name) override {
        for (int j = 0; j <= objectOffset + 1; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double),
                getFloatReg(8 + j), 0);

        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, getFloatReg(8 + objectOffset), 0);
        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)strdup(name.c_str()));
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, getFloatReg(8 + objectOffset + 1), 0);
        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS4(F64, W, F64, W, F64), SLJIT_IMM, (sljit_sw)jit_property_set_helper);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64,
            SLJIT_MEM1(SLJIT_S1), objectOffset * sizeof(double),
            SLJIT_FR0, 0);

        for (int j = 0; j <= objectOffset + 1; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                getFloatReg(8 + j), 0,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
    }

    void emitIterHasNext(int targetOffset) override {
        for (int j = 0; j <= targetOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double),
                getFloatReg(8 + j), 0);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, getFloatReg(8 + targetOffset - 1), 0);
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, getFloatReg(8 + targetOffset), 0);
        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS2(F64, F64, F64), SLJIT_IMM, (sljit_sw)jit_iter_has_next_helper);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64,
            SLJIT_MEM1(SLJIT_S1), targetOffset * sizeof(double),
            SLJIT_FR0, 0);

        for (int j = 0; j <= targetOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                getFloatReg(8 + j), 0,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
    }

    void emitCreateClass(int targetOffset, const std::string& name) override {
        for (int j = 0; j <= targetOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double),
                getFloatReg(8 + j), 0);

        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
        char* cname = strdup(name.c_str());
        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)cname);

        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS2(F64, W, W), SLJIT_IMM, (sljit_sw)jit_create_class_helper);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64,
            SLJIT_MEM1(SLJIT_S1), targetOffset * sizeof(double),
            SLJIT_FR0, 0);

        for (int j = 0; j <= targetOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                getFloatReg(8 + j), 0,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
    }

    void emitCreateAbstractClass(int targetOffset, const std::string& name) override {
        for (int j = 0; j <= targetOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double),
                getFloatReg(8 + j), 0);

        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
        char* cname = strdup(name.c_str());
        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)cname);

        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS2(F64, W, W), SLJIT_IMM, (sljit_sw)jit_create_abstract_class_helper);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64,
            SLJIT_MEM1(SLJIT_S1), targetOffset * sizeof(double),
            SLJIT_FR0, 0);

        for (int j = 0; j <= targetOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                getFloatReg(8 + j), 0,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
    }

    void emitBindMethod(int targetOffset, const std::string& name, bool isAbstract) override {
        int maxOffset = targetOffset + 1;
        for (int j = 0; j <= maxOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double),
                getFloatReg(8 + j), 0);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, getFloatReg(8 + targetOffset), 0);
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, getFloatReg(8 + targetOffset + 1), 0);
        char* cname = strdup(name.c_str());
        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)cname);
        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, isAbstract ? 1 : 0);

        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS4(F64, F64, F64, W, W), SLJIT_IMM, (sljit_sw)jit_bind_method_helper);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64,
            SLJIT_MEM1(SLJIT_S1), targetOffset * sizeof(double),
            SLJIT_FR0, 0);

        for (int j = 0; j <= maxOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                getFloatReg(8 + j), 0,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
    }

    void emitBindStaticMethod(int targetOffset, const std::string& name) override {
        int maxOffset = targetOffset + 1;
        for (int j = 0; j <= maxOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double),
                getFloatReg(8 + j), 0);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, getFloatReg(8 + targetOffset), 0);
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, getFloatReg(8 + targetOffset + 1), 0);
        char* cname = strdup(name.c_str());
        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)cname);

        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS3(F64, F64, F64, W), SLJIT_IMM, (sljit_sw)jit_bind_static_method_helper);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64,
            SLJIT_MEM1(SLJIT_S1), targetOffset * sizeof(double),
            SLJIT_FR0, 0);

        for (int j = 0; j <= maxOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                getFloatReg(8 + j), 0,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
    }

    void emitInherit(int targetOffset) override {
        int maxOffset = targetOffset + 1;
        for (int j = 0; j <= maxOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double),
                getFloatReg(8 + j), 0);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, getFloatReg(8 + targetOffset + 1), 0);
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, getFloatReg(8 + targetOffset), 0);

        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS2V(F64, F64), SLJIT_IMM, (sljit_sw)jit_inherit_helper);

        for (int j = 0; j <= maxOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                getFloatReg(8 + j), 0,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
    }

    void emitMixin(int targetOffset) override {
        int maxOffset = targetOffset + 1;
        for (int j = 0; j <= maxOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double),
                getFloatReg(8 + j), 0);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, getFloatReg(8 + targetOffset + 1), 0);
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, getFloatReg(8 + targetOffset), 0);

        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS2V(F64, F64), SLJIT_IMM, (sljit_sw)jit_mixin_helper);

        for (int j = 0; j <= maxOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                getFloatReg(8 + j), 0,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
    }

    void emitGetSuper(int targetOffset, const std::string& name) override {
        int maxOffset = targetOffset + 1;
        for (int j = 0; j <= maxOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double),
                getFloatReg(8 + j), 0);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, getFloatReg(8 + targetOffset), 0);
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, getFloatReg(8 + targetOffset + 1), 0);
        char* cname = strdup(name.c_str());
        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)cname);

        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS3(F64, F64, F64, W), SLJIT_IMM, (sljit_sw)jit_get_super_helper);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64,
            SLJIT_MEM1(SLJIT_S1), targetOffset * sizeof(double),
            SLJIT_FR0, 0);

        for (int j = 0; j <= maxOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                getFloatReg(8 + j), 0,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
    }

    void emitFieldModifier(int targetOffset, const std::string& name, uint8_t modifier) override {
        for (int j = 0; j <= targetOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double),
                getFloatReg(8 + j), 0);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, getFloatReg(8 + targetOffset), 0);
        char* cname = strdup(name.c_str());
        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)cname);
        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, modifier);

        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS3V(F64, W, W), SLJIT_IMM, (sljit_sw)jit_field_modifier_helper);

        for (int j = 0; j <= targetOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                getFloatReg(8 + j), 0,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
    }

    void emitCreateClosure(int targetOffset, double funcRaw, const uint8_t* upvalueBytes, int upvalueCount) override {
        int dataOffset = targetOffset + 1;
        for (int j = 0; j < targetOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double),
                getFloatReg(8 + j), 0);

        // Store upvalue data in scratch buffer: 2 doubles per upvalue [metadata, addr/value]
        for (int j = 0; j < upvalueCount; j++) {
            bool isLocal = upvalueBytes[j * 2];
            int index = upvalueBytes[j * 2 + 1];
            int packed = (isLocal << 8) | index;

            // Metadata
            sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, packed);
            sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_SW, SLJIT_FR2, 0, SLJIT_R0, 0);
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                SLJIT_MEM1(SLJIT_S1), (dataOffset + j * 2) * sizeof(double),
                SLJIT_FR2, 0);

            if (isLocal) {
                // Pass address of the stack slot
                sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S1, 0, SLJIT_IMM, index * sizeof(double));
                sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), (dataOffset + j * 2 + 1) * sizeof(double), SLJIT_R0, 0);
            }
        }

        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
        sljit_emit_fset64(compiler, SLJIT_FR0, funcRaw);
        sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S1, 0, SLJIT_IMM, dataOffset * sizeof(double));
        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, upvalueCount);

        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS4(F64, W, F64, P, W), SLJIT_IMM, (sljit_sw)jit_create_closure_helper);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64,
            SLJIT_MEM1(SLJIT_S1), targetOffset * sizeof(double),
            SLJIT_FR0, 0);

        for (int j = 0; j < targetOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                getFloatReg(8 + j), 0,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
    }

    void emitGetUpvalue(int targetOffset, int slot) override {
        for (int j = 0; j <= targetOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double),
                getFloatReg(8 + j), 0);

        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, slot);

        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS2(F64, W, W), SLJIT_IMM, (sljit_sw)jit_get_upvalue_helper);

        sljit_emit_fop1(compiler, SLJIT_MOV_F64,
            SLJIT_MEM1(SLJIT_S1), targetOffset * sizeof(double),
            SLJIT_FR0, 0);

        for (int j = 0; j <= targetOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                getFloatReg(8 + j), 0,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
    }

    void emitSetUpvalue(int slot, int sourceOffset) override {
        for (int j = 0; j <= sourceOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double),
                getFloatReg(8 + j), 0);

        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, slot);
        sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, getFloatReg(8 + sourceOffset), 0);

        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS3V(W, W, F64), SLJIT_IMM, (sljit_sw)jit_set_upvalue_helper);

        for (int j = 0; j <= sourceOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                getFloatReg(8 + j), 0,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
    }

    void emitCloseUpvalue(int stackOffset) override {
        for (int j = 0; j <= stackOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double),
                getFloatReg(8 + j), 0);

        sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
        sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S1, 0, SLJIT_IMM, stackOffset * sizeof(double));

        sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS2V(W, P), SLJIT_IMM, (sljit_sw)jit_close_upvalue_helper);

        for (int j = 0; j <= stackOffset; ++j)
            sljit_emit_fop1(compiler, SLJIT_MOV_F64,
                getFloatReg(8 + j), 0,
                SLJIT_MEM1(SLJIT_S1), j * sizeof(double));
    }
};
