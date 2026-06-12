#include "LSP.h"
#include <iostream>

int main(int argc, char **argv) {
    // Disable synchronization with C I/O to improve performance
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    std::string docsPath = "src/lsp/native_docs.json";
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--docs" && i + 1 < argc) {
            docsPath = argv[i + 1];
            break;
        }
    }

    trypillia::LSPServer server(docsPath);
    server.run();

    return 0;
}
