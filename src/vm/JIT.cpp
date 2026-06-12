#include "JIT.h"
#include <vector>
#include <iostream>
#include <map>
#include "OpCode.h"

JitFunc JITCompiler::compileMathFunction(std::shared_ptr<ObjFunction> function) {
    if (!function || !function->chunk) return nullptr;

    asmjit::CodeHolder code;
    code.init(rt.environment());
    asmjit::x86::Compiler cc(&code);

    // Signature: double func(double* args, int argCount)
    asmjit::FuncNode* funcNode = cc.add_func(asmjit::FuncSignature::build<double, double*, int>(asmjit::CallConvId::kCDecl));

    asmjit::x86::Gp argsPtr = cc.new_gp_ptr("argsPtr");
    asmjit::x86::Gp argCount = cc.new_gp32("argCount");
    funcNode->set_arg(0, argsPtr);
    funcNode->set_arg(1, argCount);

    // Allocate 256 doubles on the stack for the eval stack
    asmjit::x86::Gp evalStack = cc.new_gp_ptr("evalStack");
    // We use a local array
    asmjit::x86::Mem evalStackMem = cc.new_stack(256 * sizeof(double), 8);
    cc.lea(evalStack, evalStackMem);

    std::map<size_t, asmjit::Label> labels;
    
    // Pass 1: Find jump targets
    for (size_t i = 0; i < function->chunk->code.size(); ) {
        uint8_t op = function->chunk->code[i];
        if (op == static_cast<uint8_t>(OpCode::OP_JUMP) || op == static_cast<uint8_t>(OpCode::OP_JUMP_IF_FALSE)) {
            uint16_t offset = (function->chunk->code[i+1] << 8) | function->chunk->code[i+2];
            size_t target = i + 3 + offset;
            if (labels.find(target) == labels.end()) labels[target] = cc.new_label();
            i += 3;
        } else if (op == static_cast<uint8_t>(OpCode::OP_LOOP)) {
            uint16_t offset = (function->chunk->code[i+1] << 8) | function->chunk->code[i+2];
            size_t target = i + 3 - offset;
            if (labels.find(target) == labels.end()) labels[target] = cc.new_label();
            i += 3;
        } else if (op == static_cast<uint8_t>(OpCode::OP_GET_LOCAL) || op == static_cast<uint8_t>(OpCode::OP_SET_LOCAL) || op == static_cast<uint8_t>(OpCode::OP_CONSTANT)) {
            i += 2;
        } else {
            i += 1;
        }
    }

    int sp = 0;

    // Pass 2: Emit code
    for (size_t i = 0; i < function->chunk->code.size(); ++i) {
        if (labels.count(i)) {
            cc.bind(labels[i]);
        }
        
        uint8_t op = function->chunk->code[i];
        switch (op) {
            case static_cast<uint8_t>(OpCode::OP_GET_LOCAL): {
                uint8_t slot = function->chunk->code[++i];
                if (slot == 0) return nullptr; // 'this' unsupported
                asmjit::x86::Vec val = cc.new_xmm_sd();
                cc.movsd(val, asmjit::x86::ptr(argsPtr, (slot - 1) * sizeof(double)));
                cc.movsd(asmjit::x86::ptr(evalStack, sp * sizeof(double)), val);
                sp++;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_SET_LOCAL): {
                uint8_t slot = function->chunk->code[++i];
                if (slot == 0) return nullptr;
                asmjit::x86::Vec val = cc.new_xmm_sd();
                cc.movsd(val, asmjit::x86::ptr(evalStack, (sp - 1) * sizeof(double)));
                cc.movsd(asmjit::x86::ptr(argsPtr, (slot - 1) * sizeof(double)), val);
                break; // OP_SET_LOCAL does NOT pop in Trypillia!
            }
            case static_cast<uint8_t>(OpCode::OP_CONSTANT): {
                uint8_t idx = function->chunk->code[++i];
                VMValue val = function->chunk->constants[idx];
                if (std::holds_alternative<double>(val)) {
                    double d = std::get<double>(val);
                    asmjit::x86::Vec res = cc.new_xmm_sd();
                    asmjit::x86::Mem mem = cc.new_double_const(asmjit::ConstPoolScope::kLocal, d);
                    cc.movsd(res, mem);
                    cc.movsd(asmjit::x86::ptr(evalStack, sp * sizeof(double)), res);
                    sp++;
                } else {
                    return nullptr; 
                }
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_ADD): {
                if (sp < 2) return nullptr;
                asmjit::x86::Vec b = cc.new_xmm_sd();
                cc.movsd(b, asmjit::x86::ptr(evalStack, (sp - 1) * sizeof(double)));
                asmjit::x86::Vec a = cc.new_xmm_sd();
                cc.movsd(a, asmjit::x86::ptr(evalStack, (sp - 2) * sizeof(double)));
                cc.addsd(a, b);
                sp--;
                cc.movsd(asmjit::x86::ptr(evalStack, (sp - 1) * sizeof(double)), a);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_SUBTRACT): {
                if (sp < 2) return nullptr;
                asmjit::x86::Vec b = cc.new_xmm_sd();
                cc.movsd(b, asmjit::x86::ptr(evalStack, (sp - 1) * sizeof(double)));
                asmjit::x86::Vec a = cc.new_xmm_sd();
                cc.movsd(a, asmjit::x86::ptr(evalStack, (sp - 2) * sizeof(double)));
                cc.subsd(a, b);
                sp--;
                cc.movsd(asmjit::x86::ptr(evalStack, (sp - 1) * sizeof(double)), a);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_MULTIPLY): {
                if (sp < 2) return nullptr;
                asmjit::x86::Vec b = cc.new_xmm_sd();
                cc.movsd(b, asmjit::x86::ptr(evalStack, (sp - 1) * sizeof(double)));
                asmjit::x86::Vec a = cc.new_xmm_sd();
                cc.movsd(a, asmjit::x86::ptr(evalStack, (sp - 2) * sizeof(double)));
                cc.mulsd(a, b);
                sp--;
                cc.movsd(asmjit::x86::ptr(evalStack, (sp - 1) * sizeof(double)), a);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_DIVIDE): {
                if (sp < 2) return nullptr;
                asmjit::x86::Vec b = cc.new_xmm_sd();
                cc.movsd(b, asmjit::x86::ptr(evalStack, (sp - 1) * sizeof(double)));
                asmjit::x86::Vec a = cc.new_xmm_sd();
                cc.movsd(a, asmjit::x86::ptr(evalStack, (sp - 2) * sizeof(double)));
                cc.divsd(a, b);
                sp--;
                cc.movsd(asmjit::x86::ptr(evalStack, (sp - 1) * sizeof(double)), a);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_LESS): {
                if (sp < 2) return nullptr;
                asmjit::x86::Vec b = cc.new_xmm_sd();
                cc.movsd(b, asmjit::x86::ptr(evalStack, (sp - 1) * sizeof(double)));
                asmjit::x86::Vec a = cc.new_xmm_sd();
                cc.movsd(a, asmjit::x86::ptr(evalStack, (sp - 2) * sizeof(double)));
                
                // Compare a < b. If true, push 1.0, else 0.0
                asmjit::x86::Vec res = cc.new_xmm_sd();
                cc.xorps(res, res); // 0.0
                
                asmjit::x86::Vec one = cc.new_xmm_sd();
                asmjit::x86::Mem memOne = cc.new_double_const(asmjit::ConstPoolScope::kLocal, 1.0);
                cc.movsd(one, memOne);
                
                cc.comisd(a, b);
                // a < b -> Carry flag is 1 if true, 0 if false (using comisd). Wait, comisd sets CF if a < b.
                // Wait, ucomisd / comisd: a < b -> CF=1. 
                asmjit::Label lessTrue = cc.new_label();
                asmjit::Label lessDone = cc.new_label();
                
                cc.jb(lessTrue); // jump if a < b
                cc.jmp(lessDone);
                
                cc.bind(lessTrue);
                cc.movsd(res, one);
                
                cc.bind(lessDone);
                sp--;
                cc.movsd(asmjit::x86::ptr(evalStack, (sp - 1) * sizeof(double)), res);
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
                if (!labels.count(target)) return nullptr;
                cc.jmp(labels[target]);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_JUMP_IF_FALSE): {
                uint16_t offset = (function->chunk->code[i+1] << 8) | function->chunk->code[i+2];
                size_t target = i + 3 + offset;
                i += 2;
                if (!labels.count(target)) return nullptr;
                
                if (sp < 1) return nullptr;
                asmjit::x86::Vec cond = cc.new_xmm_sd();
                cc.movsd(cond, asmjit::x86::ptr(evalStack, (sp - 1) * sizeof(double)));
                
                asmjit::x86::Vec zero = cc.new_xmm_sd();
                cc.xorps(zero, zero);
                
                cc.comisd(cond, zero);
                // If cond is 0.0, ZF=1 or cond is false. We jump if false.
                cc.je(labels[target]);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_LOOP): {
                uint16_t offset = (function->chunk->code[i+1] << 8) | function->chunk->code[i+2];
                size_t target = i + 3 - offset;
                i += 2;
                if (!labels.count(target)) return nullptr;
                cc.jmp(labels[target]);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_RETURN): {
                if (sp == 0) {
                    asmjit::x86::Vec zero = cc.new_xmm_sd();
                    cc.xorps(zero, zero);
                    cc.ret(zero);
                } else {
                    asmjit::x86::Vec res = cc.new_xmm_sd();
                    cc.movsd(res, asmjit::x86::ptr(evalStack, (sp - 1) * sizeof(double)));
                    cc.ret(res);
                }
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_NIL): {
                asmjit::x86::Vec zero = cc.new_xmm_sd();
                cc.xorps(zero, zero);
                cc.movsd(asmjit::x86::ptr(evalStack, sp * sizeof(double)), zero);
                sp++;
                break;
            }
            default:
                return nullptr;
        }
    }

    if (labels.count(function->chunk->code.size())) {
        cc.bind(labels[function->chunk->code.size()]);
    }

    cc.end_func();
    cc.finalize();

    JitFunc fn;
    asmjit::Error err = rt.add(&fn, &code);
    if (err != asmjit::kErrorOk) {
        return nullptr;
    }
    return fn;
}
