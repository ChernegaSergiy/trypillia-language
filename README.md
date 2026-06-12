# Trypillia Programming Language

Trypillia is a modern, fast, and elegantly designed programming language built from the ground up to empower developers. With a clean syntax, robust standard library, and built-in LSP support, Trypillia is ready for your next big project.

## Features

- **Clean Syntax**: Familiar C-style syntax with modern conveniences (similar to JS/TypeScript and Rust).
- **Rich Standard Library**: Built-in support for Math, File System, Networking (TCP, HTTP, WebSockets), Regex, and more.
- **First-class LSP**: Ships with its own Language Server for VS Code, providing semantic highlighting, autocompletion, signature help, and live error checking.
- **Performant VM**: Runs on a custom-built Virtual Machine optimized for speed and execution safety.
- **Multithreading**: Native support for background Workers.

## Installation & Build

Trypillia is built using CMake and a modern C++ compiler.

```bash
# Clone the repository
git clone https://github.com/ChernegaSergiy/trypillia-language.git
cd trypillia-language

# Build the compiler and LSP server
mkdir build && cd build
cmake ..
make -j4
```

## Quick Start

Create a file named `hello.try`:

```trypillia
// Declare variables
let message = "Привіт, Трипілля!";
print(message);

// Define functions
fn calculate(a, b) {
    return a + b;
}

print(calculate(10, 20));
```

Run your code:
```bash
./build/trypillia hello.try
```

## IDE Support

Trypillia comes with an official **VS Code extension**. To install and use it:
1. Open the `vscode-extension` folder in VS Code.
2. Run `npm install` to install dependencies.
3. The extension will automatically connect to the `trypillia-lsp` binary from your `build` directory.

## Contributing

Contributions are welcome and appreciated! Here's how you can contribute:

1. Fork the project
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

Please make sure to update tests as appropriate and adhere to the existing coding style.

## License

This project is licensed under the CSSM Unlimited License v2.0 (CSSM-ULv2). See the [LICENSE](LICENSE) file for details.
