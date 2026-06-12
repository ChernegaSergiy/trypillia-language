#include "LSP.h"
#include <iostream>

int main(int argc, char** argv) {
    // Disable synchronization with C I/O to improve performance
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    trypillia::LSPServer server;
    server.run();

    return 0;
}
