# Trypillia JIT Benchmarks

This benchmark compares the execution speed of a floating-point math loop across various popular programming languages and the **Trypillia** language.

### Test Code

The benchmark performs 1,000,000 iterations of the following mathematical expression:

```javascript
sum = 0.0;
i = 0.0;
while (i < 1000000.0) {
    sum = sum + (i * 3.14 + 10.0 - 0.5);
    i = i + 1.0;
}
```

### Results

| Language / Engine | Execution Time (seconds) | Notes |
| :--- | :--- | :--- |
| **Rust** (`--release`) | `0.00101` | Vectorized via LLVM (AVX) |
| **C++** (`-O3`) | `0.00105` | Vectorized via GCC (AVX) |
| **Node.js** (V8) | `0.00519` | TurboFan JIT |
| **Trypillia (JIT)** | `0.00606` | Custom AsmJit Static Register Allocator Backend |
| **PHP** (8.x JIT) | `0.01202` | Zend JIT |
| **Python** (3.12) | `0.09264` | Standard Interpreter |
| **Trypillia (Interpreter)** | `6.54792` | Base AST/Bytecode Interpreter |

### Analysis

Thanks to a custom-built JIT compiler with Static Register Allocation, Trypillia executes mathematical operations entirely within the CPU's physical registers (`xmm`), bypassing main memory access during the loop's execution.

This allows the **Trypillia JIT** to match the scalar execution speed of industrial-grade engines like V8 (Node.js) and outperform Python by a factor of 15. Rust and C++ are faster solely due to automatic vectorization (SIMD), which processes multiple loop iterations per CPU cycle.
