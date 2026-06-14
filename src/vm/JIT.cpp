#include "JIT.h"
#include <vector>
#include <iostream>
#include <cstring>
#include "OpCode.h"
#include "UniversalEmitter.h"

JitFunc JITCompiler::compileMathFunction(ObjFunction* function) {
    if (!function || !function->chunk) return nullptr;

    UniversalEmitter emitter;

    int maxLocal = 0;
    for (size_t i = 0; i < function->chunk->code.size(); ) {
        uint8_t op = function->chunk->code[i];
        if (op == static_cast<uint8_t>(OpCode::OP_NOP)) {
            i += 1;
        } else if (op == static_cast<uint8_t>(OpCode::OP_JUMP) || op == static_cast<uint8_t>(OpCode::OP_JUMP_IF_FALSE)) {
            i += 3;
        } else if (op == static_cast<uint8_t>(OpCode::OP_LOOP)) {
            i += 3;
        } else if (op == static_cast<uint8_t>(OpCode::OP_GET_LOCAL) || op == static_cast<uint8_t>(OpCode::OP_SET_LOCAL)) {
            int slot = function->chunk->code[i+1];
            if (slot > maxLocal) maxLocal = slot;
            i += 2;
        } else if (op == static_cast<uint8_t>(OpCode::OP_CONSTANT) ||
                   op == static_cast<uint8_t>(OpCode::OP_GET_GLOBAL) ||
                   op == static_cast<uint8_t>(OpCode::OP_DEFINE_GLOBAL) ||
                   op == static_cast<uint8_t>(OpCode::OP_SET_GLOBAL) ||
                   op == static_cast<uint8_t>(OpCode::OP_CALL)) {
            i += 2;
        } else {
            i += 1;
        }
    }

    if (maxLocal + 1 > 8) return nullptr;

    emitter.emitPrologue(maxLocal + 1);

    // Initial SP includes the function itself (slot 0) and any arguments
    int sp = function->arity >= 0 ? function->arity + 1 : 1;

    for (size_t i = 0; i < function->chunk->code.size(); ++i) {
        emitter.bindLabel(i);
        
        uint8_t op = function->chunk->code[i];
        switch (op) {
            case static_cast<uint8_t>(OpCode::OP_NOP):
                break;
            case static_cast<uint8_t>(OpCode::OP_GET_LOCAL): {
                uint8_t slot = function->chunk->code[++i];
                if (slot == 0) return nullptr;
                if (sp >= 8) return nullptr;
                emitter.emitGetLocal(sp, slot);
                sp++;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_SET_LOCAL): {
                uint8_t slot = function->chunk->code[++i];
                if (slot == 0) return nullptr;
                if (sp == 0) return nullptr;
                emitter.emitSetLocal(slot, sp - 1);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_GET_GLOBAL): {
                uint8_t constantIdx = function->chunk->code[++i];
                VMValue constant = function->chunk->constants[constantIdx];
                if (!constant.isString()) return nullptr;
                if (sp >= 8) return nullptr;
                std::string name = constant.asString()->flatten();
                emitter.emitGetGlobal(name, sp);
                sp++;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_SET_GLOBAL): {
                uint8_t constantIdx = function->chunk->code[++i];
                VMValue constant = function->chunk->constants[constantIdx];
                if (!constant.isString()) return nullptr;
                if (sp < 1) return nullptr;
                std::string name = constant.asString()->flatten();
                // Value is at sp - 1
                emitter.emitSetGlobal(name, sp - 1);
                // Value remains on stack
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_DEFINE_GLOBAL): {
                uint8_t constantIdx = function->chunk->code[++i];
                VMValue constant = function->chunk->constants[constantIdx];
                if (!constant.isString()) return nullptr;
                if (sp < 1) return nullptr;
                std::string name = constant.asString()->flatten();
                // Value is at sp - 1
                emitter.emitSetGlobal(name, sp - 1);
                sp--; // Pop value
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_CALL): {
                uint8_t argCount = function->chunk->code[++i];
                int calleeSp = sp - argCount - 1;
                if (calleeSp < 0) {
                    return nullptr;
                }
                
                emitter.emitCallDynamic(calleeSp, calleeSp, argCount);
                
                sp = calleeSp + 1; // Result is at calleeSp
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_CONSTANT): {
                uint8_t idx = function->chunk->code[++i];
                VMValue val = function->chunk->constants[idx];
                if (val.isNumber()) {
                    double d = val.asNumber();
                    if (sp >= 8) return nullptr;
                    emitter.emitLoadConst(sp, d);
                    sp++;
                } else {
                    return nullptr; 
                }
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_ADD): {
                if (sp < 2) return nullptr;
                emitter.emitAdd(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_SUBTRACT): {
                if (sp < 2) return nullptr;
                emitter.emitSub(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_MULTIPLY): {
                if (sp < 2) return nullptr;
                emitter.emitMul(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_DIVIDE): {
                if (sp < 2) return nullptr;
                emitter.emitDiv(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_LESS): {
                if (sp < 2) return nullptr;
                emitter.emitCmpLt(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_BIT_AND): {
                if (sp < 2) return nullptr;
                emitter.emitBitAnd(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_BIT_OR): {
                if (sp < 2) return nullptr;
                emitter.emitBitOr(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_BIT_XOR): {
                if (sp < 2) return nullptr;
                emitter.emitBitXor(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_BIT_SHIFT_LEFT): {
                if (sp < 2) return nullptr;
                emitter.emitBitShl(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_BIT_SHIFT_RIGHT): {
                if (sp < 2) return nullptr;
                emitter.emitBitShr(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_BIT_NOT): {
                if (sp < 1) return nullptr;
                emitter.emitBitNot(sp - 1);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_GREATER): {
                if (sp < 2) return nullptr;
                emitter.emitCmpGt(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_EQUAL): {
                if (sp < 2) return nullptr;
                emitter.emitCmpEq(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_NOT): {
                if (sp < 1) return nullptr;
                emitter.emitNot(sp - 1);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_NEGATE): {
                if (sp < 1) return nullptr;
                emitter.emitNegate(sp - 1);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_POP): {
                if (sp == 0) return nullptr;
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_INDEX_GET): {
                if (sp < 2) return nullptr;
                // object is at sp - 2, index is at sp - 1
                emitter.emitIndexGet(sp - 2, sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_INDEX_SET): {
                if (sp < 3) return nullptr;
                // object is at sp - 3, index is at sp - 2, value is at sp - 1
                // value is left on stack
                emitter.emitIndexSet(sp - 3, sp - 2, sp - 1);
                emitter.emitMove(sp - 3, sp - 1); // move value to result slot
                sp -= 2;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_JUMP): {
                uint16_t offset = (function->chunk->code[i+1] << 8) | function->chunk->code[i+2];
                size_t target = i + 3 + offset;
                i += 2;
                emitter.emitJump(target);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_JUMP_IF_FALSE): {
                uint16_t offset = (function->chunk->code[i+1] << 8) | function->chunk->code[i+2];
                size_t target = i + 3 + offset;
                i += 2;
                if (sp < 1) return nullptr;
                emitter.emitJumpIfFalse(sp - 1, target);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_LOOP): {
                uint16_t offset = (function->chunk->code[i+1] << 8) | function->chunk->code[i+2];
                size_t target = i + 3 - offset;
                i += 2;
                emitter.emitJump(target);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_RETURN): {
                if (sp == 0) {
                    emitter.emitLoadConst(0, 0.0);
                } else if (sp - 1 != 0) {
                    emitter.emitMove(0, sp - 1);
                }
                emitter.emitEpilogue(maxLocal);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_NIL): {
                if (sp >= 8) return nullptr;
                emitter.emitLoadConst(sp, 0.0);
                sp++;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_TRUE): {
                if (sp >= 8) return nullptr;
                emitter.emitLoadConst(sp, 1.0);
                sp++;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_FALSE): {
                if (sp >= 8) return nullptr;
                emitter.emitLoadConst(sp, 0.0);
                sp++;
                break;
            }
            default:
                return nullptr;
        }
    }

    emitter.bindLabel(function->chunk->code.size());
    return emitter.finalize();
}
