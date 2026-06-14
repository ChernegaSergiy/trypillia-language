#include "JIT.h"
#include <vector>
#include <iostream>
#include <cstring>
#include <set>
#include "OpCode.h"
#include "UniversalEmitter.h"

JitFunc JITCompiler::compileMathFunction(ObjFunction* function) {
    if (!function || !function->chunk) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);

    UniversalEmitter emitter(function->arity);

    int maxLocal = 0;
    std::set<int> capturedLocals;
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
                   op == static_cast<uint8_t>(OpCode::OP_CALL) ||
                   op == static_cast<uint8_t>(OpCode::OP_BUILD_LIST) ||
                   op == static_cast<uint8_t>(OpCode::OP_BUILD_MAP) ||
                   op == static_cast<uint8_t>(OpCode::OP_PROPERTY_GET) ||
                   op == static_cast<uint8_t>(OpCode::OP_PROPERTY_SET) ||
                   op == static_cast<uint8_t>(OpCode::OP_GET_UPVALUE) ||
                   op == static_cast<uint8_t>(OpCode::OP_SET_UPVALUE) ||
                   op == static_cast<uint8_t>(OpCode::OP_CLASS) ||
                   op == static_cast<uint8_t>(OpCode::OP_ABSTRACT_CLASS) ||
                   op == static_cast<uint8_t>(OpCode::OP_GET_SUPER) ||
                   op == static_cast<uint8_t>(OpCode::OP_METHOD) ||
                   op == static_cast<uint8_t>(OpCode::OP_ABSTRACT_METHOD) ||
                   op == static_cast<uint8_t>(OpCode::OP_STATIC_METHOD)) {
            i += 2;
        } else if (op == static_cast<uint8_t>(OpCode::OP_FIELD_MODIFIER)) {
            i += 3;
        } else if (op == static_cast<uint8_t>(OpCode::OP_CLOSURE)) {
            uint8_t constIdx = function->chunk->code[i+1];
            VMValue funcVal = function->chunk->constants[constIdx];
            int upvalueCount = funcVal.asFunction()->upvalueCount;
            const uint8_t* upvalueBytes = &function->chunk->code[i + 2];
            for (int j = 0; j < upvalueCount; j++) {
                if (upvalueBytes[j * 2]) { // isLocal
                    capturedLocals.insert(upvalueBytes[j * 2 + 1]);
                }
            }
            i += 2 + 2 * upvalueCount;
        } else {
            i += 1;
        }
    }

    if (maxLocal + 1 > 256) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);

    std::vector<int> capturedVec(capturedLocals.begin(), capturedLocals.end());
    emitter.setCapturedLocals(capturedVec);
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
                if (sp >= 256) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitGetLocal(sp, slot);
                sp++;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_SET_LOCAL): {
                uint8_t slot = function->chunk->code[++i];
                if (sp == 0) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitSetLocal(slot, sp - 1);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_GET_GLOBAL): {
                uint8_t constantIdx = function->chunk->code[++i];
                VMValue constant = function->chunk->constants[constantIdx];
                if (!constant.isString()) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                if (sp >= 256) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                std::string name = constant.asString()->flatten();
                emitter.emitGetGlobal(name, sp);
                sp++;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_SET_GLOBAL): {
                uint8_t constantIdx = function->chunk->code[++i];
                VMValue constant = function->chunk->constants[constantIdx];
                if (!constant.isString()) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                if (sp < 1) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                std::string name = constant.asString()->flatten();
                // Value is at sp - 1
                emitter.emitSetGlobal(name, sp - 1);
                // Value remains on stack
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_DEFINE_GLOBAL): {
                uint8_t constantIdx = function->chunk->code[++i];
                VMValue constant = function->chunk->constants[constantIdx];
                if (!constant.isString()) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                if (sp < 1) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
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
                    return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                }
                
                emitter.emitCallDynamic(calleeSp, calleeSp, argCount);
                
                sp = calleeSp + 1; // Result is at calleeSp
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_CONSTANT): {
                uint8_t idx = function->chunk->code[++i];
                VMValue val = function->chunk->constants[idx];
                double raw;
                memcpy(&raw, &val, sizeof(double));
                if (sp >= 256) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitLoadConst(sp, raw);
                sp++;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_ADD): {
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitAdd(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_SUBTRACT): {
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitSub(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_MULTIPLY): {
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitMul(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_DIVIDE): {
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitDiv(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_MOD): {
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitMod(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_BIT_AND): {
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitBitAnd(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_BIT_OR): {
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitBitOr(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_BIT_XOR): {
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitBitXor(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_BIT_SHIFT_LEFT): {
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitBitShl(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_BIT_SHIFT_RIGHT): {
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitBitShr(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_BIT_NOT): {
                if (sp < 1) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitBitNot(sp - 1);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_GREATER): {
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitCmpGt(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_GREATER_EQUAL): {
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitCmpGe(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_LESS): {
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitCmpLt(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_LESS_EQUAL): {
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitCmpLe(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_EQUAL): {
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitCmpEq(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_NOT_EQUAL): {
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitCmpNe(sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_NOT): {
                if (sp < 1) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitNot(sp - 1);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_NEGATE): {
                if (sp < 1) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitNegate(sp - 1);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_POP): {
                if (sp == 0) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_INDEX_GET): {
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                // object is at sp - 2, index is at sp - 1
                emitter.emitIndexGet(sp - 2, sp - 2, sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_INDEX_SET): {
                if (sp < 3) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
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
                if (sp < 1) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
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
                if (sp > 0) {
                    emitter.emitReturnValue(sp - 1);
                } else {
                    emitter.emitLoadConst(0, 0.0);
                    emitter.emitReturnValue(0);
                }
                emitter.emitCloseUpvalue(0);
                emitter.emitEpilogue(maxLocal + 1);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_NIL): {
                if (sp >= 256) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitLoadConst(sp, 0.0);
                sp++;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_TRUE): {
                if (sp >= 256) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitLoadConst(sp, 1.0);
                sp++;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_FALSE): {
                if (sp >= 256) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitLoadConst(sp, 0.0);
                sp++;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_DUP): {
                if (sp >= 256) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitMove(sp, sp - 1);
                sp++;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_BUILD_LIST): {
                uint8_t count = function->chunk->code[++i];
                if (sp < count) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                if (sp - count + 1 >= 256) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitBuildList(sp - count, count);
                sp = sp - count + 1;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_BUILD_MAP): {
                uint8_t count = function->chunk->code[++i];
                if (sp < count * 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                if (sp - (count * 2) + 1 >= 256) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitBuildMap(sp - (count * 2), count);
                sp = sp - (count * 2) + 1;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_PROPERTY_GET): {
                uint8_t constIdx = function->chunk->code[++i];
                VMValue nameVal = function->chunk->constants[constIdx];
                if (!nameVal.isString()) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                if (sp < 1) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                std::string name = nameVal.asString()->flatten();
                emitter.emitPropertyGet(sp - 1, name);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_PROPERTY_SET): {
                uint8_t constIdx = function->chunk->code[++i];
                VMValue nameVal = function->chunk->constants[constIdx];
                if (!nameVal.isString()) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                std::string name = nameVal.asString()->flatten();
                emitter.emitPropertySet(sp - 2, name);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_ITER_HAS_NEXT): {
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitIterHasNext(sp - 2);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_GET_UPVALUE): {
                uint8_t slot = function->chunk->code[++i];
                if (sp >= 256) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitGetUpvalue(sp, slot);
                sp++;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_SET_UPVALUE): {
                uint8_t slot = function->chunk->code[++i];
                if (sp < 1) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitSetUpvalue(slot, sp - 1);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_CLOSE_UPVALUE): {
                if (sp < 1) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitCloseUpvalue(sp - 1);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_CLOSURE): {
                uint8_t constIdx = function->chunk->code[++i];
                VMValue funcVal = function->chunk->constants[constIdx];
                int upvalueCount = funcVal.asFunction()->upvalueCount;
                const uint8_t* upvalueBytes = &function->chunk->code[i + 1];
                i += 2 * upvalueCount;
                if (sp >= 256) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                double raw;
                memcpy(&raw, &funcVal, sizeof(double));
                emitter.emitCreateClosure(sp, raw, upvalueBytes, upvalueCount);
                sp++;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_CLASS): {
                uint8_t constIdx = function->chunk->code[++i];
                VMValue nameVal = function->chunk->constants[constIdx];
                if (!nameVal.isString()) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                std::string name = nameVal.asString()->flatten();
                if (sp >= 256) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitCreateClass(sp, name);
                sp++;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_ABSTRACT_CLASS): {
                uint8_t constIdx = function->chunk->code[++i];
                VMValue nameVal = function->chunk->constants[constIdx];
                if (!nameVal.isString()) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                std::string name = nameVal.asString()->flatten();
                if (sp >= 256) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitCreateAbstractClass(sp, name);
                sp++;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_INHERIT): {
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitInherit(sp - 2);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_MIXIN): {
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitMixin(sp - 2);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_GET_SUPER): {
                uint8_t constIdx = function->chunk->code[++i];
                VMValue nameVal = function->chunk->constants[constIdx];
                if (!nameVal.isString()) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                std::string name = nameVal.asString()->flatten();
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitGetSuper(sp - 2, name);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_METHOD): {
                uint8_t constIdx = function->chunk->code[++i];
                VMValue nameVal = function->chunk->constants[constIdx];
                if (!nameVal.isString()) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                std::string name = nameVal.asString()->flatten();
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitBindMethod(sp - 2, name, false);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_ABSTRACT_METHOD): {
                uint8_t constIdx = function->chunk->code[++i];
                VMValue nameVal = function->chunk->constants[constIdx];
                if (!nameVal.isString()) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                std::string name = nameVal.asString()->flatten();
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitBindMethod(sp - 2, name, true);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_STATIC_METHOD): {
                uint8_t constIdx = function->chunk->code[++i];
                VMValue nameVal = function->chunk->constants[constIdx];
                if (!nameVal.isString()) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                std::string name = nameVal.asString()->flatten();
                if (sp < 2) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitBindStaticMethod(sp - 2, name);
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_FIELD_MODIFIER): {
                uint8_t constIdx = function->chunk->code[++i];
                VMValue nameVal = function->chunk->constants[constIdx];
                if (!nameVal.isString()) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                std::string name = nameVal.asString()->flatten();
                uint8_t modifier = function->chunk->code[++i];
                if (sp < 1) return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
                emitter.emitFieldModifier(sp - 1, name, modifier);
                break;
            }
            default:
                return (printf("JIT Abort at line %d\n", __LINE__), nullptr);
        }
    }

    emitter.bindLabel(function->chunk->code.size());
    return emitter.finalize();
}
