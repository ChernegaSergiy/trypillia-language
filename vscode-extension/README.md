# Trypillia VS Code Extension

This directory contains the source code for the Trypillia language VS Code extension. It provides integration with the language server (`trypillia-lsp`) and basic syntax highlighting.

## Directory Structure

```
vscode-extension/
+-- src/                           # TypeScript frontend/client
|   \-- extension.ts               # Extension entry point & LSP client setup
+-- syntaxes/                      # TextMate configurations
|   \-- trypillia.tmLanguage.json  # Syntax highlighting rules
+-- language-configuration.json    # Language rules (brackets, comments)
+-- package.json                   # VS Code extension manifest
\-- tsconfig.json                  # TypeScript compiler configuration
```

## Development and Build

Node.js and `npm` are required to work with the extension.

1. **Install dependencies:**
   ```bash
   npm install
   ```

2. **Compile the extension:**
   ```bash
   npm run compile
   ```

3. **Run in development mode:**
   Open this directory in VS Code and press `F5` to launch a new Extension Development Host window with the extension loaded.

## LSP Integration

The extension client is configured to spawn `trypillia-lsp` as a local process. Ensure that the language server binary (`trypillia-lsp`) is built and available in your system path (or configured via extension settings), otherwise diagnostics and autocomplete will not function.
