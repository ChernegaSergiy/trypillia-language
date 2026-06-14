#include "JIT.h"
#include <vector>
#include <iostream>
#include <cstring>
#include "OpCode.h"
#include "UniversalEmitter.h"

JitFunc JITCompiler::compileMathFunction(std::shared_ptr<ObjFunction> function) {
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
    std::vector<std::string> stackGlobalNames(16, "");

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
                stackGlobalNames[sp] = "";
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
                if (!std::holds_alternative<std::shared_ptr<ObjString>>(constant)) return nullptr;
                if (sp >= 8) return nullptr;
                stackGlobalNames[sp] = std::get<std::shared_ptr<ObjString>>(constant)->flatten();
                // We don't emit anything for the function object itself, we just track its name
                sp++;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_CALL): {
                uint8_t argCount = function->chunk->code[++i];
                int calleeSp = sp - argCount - 1;
                if (calleeSp < 0) {
                    printf("JIT Abort: calleeSp < 0\n");
                    return nullptr;
                }
                
                std::string calleeName = stackGlobalNames[calleeSp];
                if (calleeName == "") {
                    printf("JIT Abort: calleeName is empty at sp=%d\n", calleeSp);
                    return nullptr; // Only global function calls supported
                }
                
                printf("JIT: Compiling OP_CALL to %s\n", calleeName.c_str());
                emitter.emitCallGlobal(calleeName, calleeSp, argCount);
                
                sp = calleeSp + 1; // Result is at calleeSp
                stackGlobalNames[calleeSp] = "";
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_CONSTANT): {
                uint8_t idx = function->chunk->code[++i];
                VMValue val = function->chunk->constants[idx];
                if (std::holds_alternative<double>(val)) {
                    double d = std::get<double>(val);
                    if (sp >= 8) return nullptr;
                    stackGlobalNames[sp] = "";
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
                stackGlobalNames[sp - 1] = "";
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_SUBTRACT): {
                if (sp < 2) return nullptr;
                emitter.emitSub(sp - 2, sp - 1);
                sp--;
                stackGlobalNames[sp - 1] = "";
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_MULTIPLY): {
                if (sp < 2) return nullptr;
                emitter.emitMul(sp - 2, sp - 1);
                sp--;
                stackGlobalNames[sp - 1] = "";
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_DIVIDE): {
                if (sp < 2) return nullptr;
                emitter.emitDiv(sp - 2, sp - 1);
                sp--;
                stackGlobalNames[sp - 1] = "";
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_LESS): {
                if (sp < 2) return nullptr;
                emitter.emitCmpLt(sp - 2, sp - 1);
                sp--;
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
