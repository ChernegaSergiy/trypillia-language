import re

def patch_fs():
    with open("src/native/fs/FS.cpp", "r") as f:
        content = f.read()

    # Add StdLib.h
    content = re.sub(
        r'#include <filesystem>',
        r'#include <filesystem>\n#include "../StdLib.h"',
        content
    )

    # fileOpen
    content = re.sub(
        r'return nullptr; // Later return Error object',
        r'return makeResultErr(currentVM, "Failed to open file: " + path);',
        content
    )
    content = re.sub(
        r'return instance;\n    }',
        r'return makeResultOk(currentVM, instance);\n    }',
        content
    )

    # fileRead
    content = re.sub(
        r'if \(!file \|\| !file->is_open\(\)\) return nullptr;',
        r'if (!file || !file->is_open()) return makeResultErr(currentVM, "File is not open");',
        content
    )
    content = re.sub(
        r'return buffer\.str\(\);\n    }',
        r'return makeResultOk(currentVM, buffer.str());\n    }',
        content
    )

    # fileWrite
    content = re.sub(
        r'if \(!file \|\| !file->is_open\(\)\) return false;',
        r'if (!file || !file->is_open()) return makeResultErr(currentVM, "File is not open");',
        content
    )
    content = re.sub(
        r'\*file << content;\n\s*return true;\n    }',
        r'*file << content;\n        return makeResultOk(currentVM, true);\n    }',
        content
    )

    # fileClose
    content = re.sub(
        r'        return true;\n    }\n\n    static VMValue fileExists',
        r'        return makeResultOk(currentVM, true);\n    }\n\n    static VMValue fileExists',
        content
    )
    
    with open("src/native/fs/FS.cpp", "w") as f:
        f.write(content)

def patch_net():
    with open("src/native/net/Net.cpp", "r") as f:
        content = f.read()

    content = re.sub(
        r'#include <vector>',
        r'#include <vector>\n#include "../StdLib.h"',
        content
    )

    # socketConnect
    content = re.sub(
        r'if \(sock < 0\) return nullptr;',
        r'if (sock < 0) return makeResultErr(currentVM, "Failed to create socket");',
        content
    )
    content = re.sub(
        r'if \(server == nullptr\) {\n\s*close\(sock\);\n\s*return nullptr;\n\s*}',
        r'if (server == nullptr) {\n            close(sock);\n            return makeResultErr(currentVM, "Failed to resolve host: " + host);\n        }',
        content
    )
    content = re.sub(
        r'if \(connect\(sock, \(struct sockaddr\*\)&serv_addr, sizeof\(serv_addr\)\) < 0\) {\n\s*close\(sock\);\n\s*return nullptr;\n\s*}',
        r'if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {\n            close(sock);\n            return makeResultErr(currentVM, "Failed to connect to host");\n        }',
        content
    )
    content = re.sub(
        r'return instance;\n    }',
        r'return makeResultOk(currentVM, instance);\n    }',
        content
    )

    # socketSend
    content = re.sub(
        r'if \(!data \|\| data->fd == -1\) return false;',
        r'if (!data || data->fd == -1) return makeResultErr(currentVM, "Socket is closed");',
        content
    )
    content = re.sub(
        r'return \(n >= 0\);\n    }',
        r'if (n < 0) return makeResultErr(currentVM, "Failed to send data");\n        return makeResultOk(currentVM, true);\n    }',
        content
    )

    # socketRecv
    content = re.sub(
        r'if \(!data \|\| data->fd == -1\) return nullptr;',
        r'if (!data || data->fd == -1) return makeResultErr(currentVM, "Socket is closed");',
        content
    )
    content = re.sub(
        r'if \(n < 0\) return nullptr;',
        r'if (n < 0) return makeResultErr(currentVM, "Failed to receive data");',
        content
    )
    content = re.sub(
        r'return std::string\(buffer\.data\(\), n\);\n    }',
        r'return makeResultOk(currentVM, std::string(buffer.data(), n));\n    }',
        content
    )

    # socketClose
    content = re.sub(
        r'        return true;\n    }\n\n    void registerAll',
        r'        return makeResultOk(currentVM, true);\n    }\n\n    void registerAll',
        content
    )

    with open("src/native/net/Net.cpp", "w") as f:
        f.write(content)

patch_fs()
patch_net()
