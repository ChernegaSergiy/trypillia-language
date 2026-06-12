#include "JIT.h"
#include <vector>
#include <iostream>
#include <map>
#include <cstring>
#include "OpCode.h"

JitFunc JITCompiler::compileMathFunction(std::shared_ptr<ObjFunction> function) {
    if (!function || !function->chunk) return nullptr;

    asmjit::CodeHolder code;
    code.init(rt.environment());
    
    // We use pure Assembler for 100% control over registers (Static Register Allocation)
    asmjit::x86::Assembler a(&code);

    std::map<size_t, asmjit::Label> labels;
    
    // Pass 1: Find jump targets and max locals
    int maxLocal = 0;
    for (size_t i = 0; i < function->chunk->code.size(); ) {
        uint8_t op = function->chunk->code[i];
        if (op == static_cast<uint8_t>(OpCode::OP_NOP)) {
            i += 1;
        } else if (op == static_cast<uint8_t>(OpCode::OP_JUMP) || op == static_cast<uint8_t>(OpCode::OP_JUMP_IF_FALSE)) {
            uint16_t offset = (function->chunk->code[i+1] << 8) | function->chunk->code[i+2];
            size_t target = i + 3 + offset;
            if (labels.find(target) == labels.end()) labels[target] = a.new_label();
            i += 3;
        } else if (op == static_cast<uint8_t>(OpCode::OP_LOOP)) {
            uint16_t offset = (function->chunk->code[i+1] << 8) | function->chunk->code[i+2];
            size_t target = i + 3 - offset;
            if (labels.find(target) == labels.end()) labels[target] = a.new_label();
            i += 3;
        } else if (op == static_cast<uint8_t>(OpCode::OP_GET_LOCAL) || op == static_cast<uint8_t>(OpCode::OP_SET_LOCAL)) {
            int slot = function->chunk->code[i+1];
            if (slot > maxLocal) maxLocal = slot;
            i += 2;
        } else if (op == static_cast<uint8_t>(OpCode::OP_CONSTANT)) {
            i += 2;
        } else {
            i += 1;
        }
    }

    if (maxLocal > 8) return nullptr; // We statically map up to 8 locals to xmm0-xmm7

    // --- PROLOGUE ---
    // Linux System V AMD64 ABI:
    // arg 1 (double* args) -> rdi
    // arg 2 (int argCount) -> rsi
    
    // Load initial values from argsPtr (rdi) into xmm0..xmm7
    for (int j = 0; j < maxLocal; ++j) {
        a.movsd(asmjit::x86::xmm(j), asmjit::x86::ptr(asmjit::x86::rdi, j * sizeof(double)));
    }

    int sp = 0; // stack pointer goes from 0 to 7. Maps to xmm8..xmm15

    // Pass 2: Emit code
    for (size_t i = 0; i < function->chunk->code.size(); ++i) {
        if (labels.count(i)) {
            a.bind(labels[i]);
        }
        
        uint8_t op = function->chunk->code[i];
        switch (op) {
            case static_cast<uint8_t>(OpCode::OP_NOP):
                break;
            case static_cast<uint8_t>(OpCode::OP_GET_LOCAL): {
                uint8_t slot = function->chunk->code[++i];
                if (slot == 0) return nullptr; // 'this' unsupported
                if (sp >= 8) return nullptr;
                a.movsd(asmjit::x86::xmm(8 + sp), asmjit::x86::xmm(slot - 1));
                sp++;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_SET_LOCAL): {
                uint8_t slot = function->chunk->code[++i];
                if (slot == 0) return nullptr;
                if (sp == 0) return nullptr;
                a.movsd(asmjit::x86::xmm(slot - 1), asmjit::x86::xmm(8 + sp - 1));
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_CONSTANT): {
                uint8_t idx = function->chunk->code[++i];
                VMValue val = function->chunk->constants[idx];
                if (std::holds_alternative<double>(val)) {
                    double d = std::get<double>(val);
                    if (sp >= 8) return nullptr;
                    uint64_t bits;
                    std::memcpy(&bits, &d, sizeof(double));
                    a.mov(asmjit::x86::rax, bits);
                    a.movq(asmjit::x86::xmm(8 + sp), asmjit::x86::rax);
                    sp++;
                } else {
                    return nullptr; 
                }
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_ADD): {
                if (sp < 2) return nullptr;
                a.addsd(asmjit::x86::xmm(8 + sp - 2), asmjit::x86::xmm(8 + sp - 1));
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_SUBTRACT): {
                if (sp < 2) return nullptr;
                a.subsd(asmjit::x86::xmm(8 + sp - 2), asmjit::x86::xmm(8 + sp - 1));
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_MULTIPLY): {
                if (sp < 2) return nullptr;
                a.mulsd(asmjit::x86::xmm(8 + sp - 2), asmjit::x86::xmm(8 + sp - 1));
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_DIVIDE): {
                if (sp < 2) return nullptr;
                a.divsd(asmjit::x86::xmm(8 + sp - 2), asmjit::x86::xmm(8 + sp - 1));
                sp--;
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_LESS): {
                if (sp < 2) return nullptr;
                
                // Compare (sp-2) < (sp-1)
                a.comisd(asmjit::x86::xmm(8 + sp - 2), asmjit::x86::xmm(8 + sp - 1));
                
                asmjit::Label lessTrue = a.new_label();
                asmjit::Label lessDone = a.new_label();
                
                a.jb(lessTrue);
                
                // False case: 0.0
                a.xorps(asmjit::x86::xmm(8 + sp - 2), asmjit::x86::xmm(8 + sp - 2));
                a.jmp(lessDone);
                
                a.bind(lessTrue);
                // True case: 1.0
                double one = 1.0;
                uint64_t oneBits;
                std::memcpy(&oneBits, &one, sizeof(double));
                a.mov(asmjit::x86::rax, oneBits);
                a.movq(asmjit::x86::xmm(8 + sp - 2), asmjit::x86::rax);
                
                a.bind(lessDone);
                sp--;
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
                a.jmp(labels[target]);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_JUMP_IF_FALSE): {
                uint16_t offset = (function->chunk->code[i+1] << 8) | function->chunk->code[i+2];
                size_t target = i + 3 + offset;
                i += 2;
                if (!labels.count(target)) return nullptr;
                
                if (sp < 1) return nullptr;
                a.xorps(asmjit::x86::xmm15, asmjit::x86::xmm15); // zero
                a.comisd(asmjit::x86::xmm(8 + sp - 1), asmjit::x86::xmm15);
                a.je(labels[target]);
                // no sp-- here
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_LOOP): {
                uint16_t offset = (function->chunk->code[i+1] << 8) | function->chunk->code[i+2];
                size_t target = i + 3 - offset;
                i += 2;
                if (!labels.count(target)) return nullptr;
                a.jmp(labels[target]);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_RETURN): {
                if (sp == 0) {
                    a.xorps(asmjit::x86::xmm0, asmjit::x86::xmm0);
                } else {
                    if ((8 + sp - 1) != 0) { // Don't self move
                        a.movsd(asmjit::x86::xmm0, asmjit::x86::xmm(8 + sp - 1));
                    }
                }
                
                // --- EPILOGUE ---
                // Write locals back to argsPtr (rdi) so VM can read them
                for (int j = 0; j < maxLocal; ++j) {
                    a.movsd(asmjit::x86::ptr(asmjit::x86::rdi, j * sizeof(double)), asmjit::x86::xmm(j));
                }
                
                a.ret();
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_NIL): {
                if (sp >= 8) return nullptr;
                a.xorps(asmjit::x86::xmm(8 + sp), asmjit::x86::xmm(8 + sp));
                sp++;
                break;
            }
            default:
                return nullptr;
        }
    }

    if (labels.count(function->chunk->code.size())) {
        a.bind(labels[function->chunk->code.size()]);
    }

    JitFunc fn;
    asmjit::Error err = rt.add(&fn, &code);
    if (err != asmjit::kErrorOk) {
        return nullptr;
    }
    return fn;
}
