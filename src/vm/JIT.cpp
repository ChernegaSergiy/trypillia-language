#include "JIT.h"
#include <vector>
#include <iostream>
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

    std::vector<asmjit::x86::Vec> simStack;

    // A simple loop to read bytecode and emit x86 instructions
    for (size_t i = 0; i < function->chunk->code.size(); ++i) {
        uint8_t op = function->chunk->code[i];
        switch (op) {
            case static_cast<uint8_t>(OpCode::OP_GET_LOCAL): {
                uint8_t slot = function->chunk->code[++i];
                if (slot == 0) return nullptr; // Cannot access 'this' or closure in JIT yet
                asmjit::x86::Vec val = cc.new_xmm_sd();
                cc.movsd(val, asmjit::x86::ptr(argsPtr, (slot - 1) * sizeof(double)));
                simStack.push_back(val);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_CONSTANT): {
                uint8_t idx = function->chunk->code[++i];
                VMValue val = function->chunk->constants[idx];
                if (std::holds_alternative<double>(val)) {
                    double d = std::get<double>(val);
                    asmjit::x86::Vec res = cc.new_xmm_sd();
                    asmjit::x86::Mem mem = cc.new_double_const(asmjit::ConstPoolScope::kLocal, d);
                    cc.movsd(res, mem);
                    simStack.push_back(res);
                } else {
                    return nullptr; // Unsupported constant
                }
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_ADD): {
                if (simStack.size() < 2) return nullptr;
                asmjit::x86::Vec b = simStack.back(); simStack.pop_back();
                asmjit::x86::Vec a = simStack.back(); simStack.pop_back();
                asmjit::x86::Vec res = cc.new_xmm_sd();
                cc.movsd(res, a);
                cc.addsd(res, b);
                simStack.push_back(res);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_SUBTRACT): {
                if (simStack.size() < 2) return nullptr;
                asmjit::x86::Vec b = simStack.back(); simStack.pop_back();
                asmjit::x86::Vec a = simStack.back(); simStack.pop_back();
                asmjit::x86::Vec res = cc.new_xmm_sd();
                cc.movsd(res, a);
                cc.subsd(res, b);
                simStack.push_back(res);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_MULTIPLY): {
                if (simStack.size() < 2) return nullptr;
                asmjit::x86::Vec b = simStack.back(); simStack.pop_back();
                asmjit::x86::Vec a = simStack.back(); simStack.pop_back();
                asmjit::x86::Vec res = cc.new_xmm_sd();
                cc.movsd(res, a);
                cc.mulsd(res, b);
                simStack.push_back(res);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_DIVIDE): {
                if (simStack.size() < 2) return nullptr;
                asmjit::x86::Vec b = simStack.back(); simStack.pop_back();
                asmjit::x86::Vec a = simStack.back(); simStack.pop_back();
                asmjit::x86::Vec res = cc.new_xmm_sd();
                cc.movsd(res, a);
                cc.divsd(res, b);
                simStack.push_back(res);
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_RETURN): {
                if (simStack.empty()) {
                    asmjit::x86::Vec zero = cc.new_xmm_sd();
                    cc.xorps(zero, zero);
                    cc.ret(zero);
                } else {
                    asmjit::x86::Vec res = simStack.back(); simStack.pop_back();
                    cc.ret(res);
                }
                break;
            }
            case static_cast<uint8_t>(OpCode::OP_NIL): {
                // Jumps usually push nil for functions with no explicit return at the end.
                // We'll treat it as returning 0.0 for math JIT.
                asmjit::x86::Vec zero = cc.new_xmm_sd();
                cc.xorps(zero, zero);
                simStack.push_back(zero);
                break;
            }
            default:
                // Unsupported opcode, fallback to standard VM
                std::cout << "[JIT] Unsupported opcode: " << (int)op << std::endl;
                return nullptr;
        }
    }

    cc.end_func();
    cc.finalize();

    JitFunc fn;
    asmjit::Error err = rt.add(&fn, &code);
    if (err != asmjit::kErrorOk) {
        return nullptr;
    }
    std::cout << "[JIT] Successfully compiled function!" << std::endl;
    return fn;
}
