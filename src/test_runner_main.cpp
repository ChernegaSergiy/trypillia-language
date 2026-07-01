#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

static std::string execAndCapture(const std::string &cmd) {
    std::string result;
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe)
        return result;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    return result;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <test-file> [test-file...]" << std::endl;
        return 1;
    }

    const char *trypilliaBin = nullptr;
    {
        std::string selfPath(argv[0]);
        auto slash = selfPath.find_last_of("/\\");
        std::string dir = (slash == std::string::npos) ? "." : selfPath.substr(0, slash);
        std::string candidate = dir + "/trypillia";
        if (FILE *f = fopen(candidate.c_str(), "r")) {
            fclose(f);
            trypilliaBin = strdup(candidate.c_str());
        } else {
            trypilliaBin = "trypillia";
        }
    }

    int passed = 0;
    int failed = 0;

    for (int i = 1; i < argc; i++) {
        std::string cmd = std::string(trypilliaBin) + " " + argv[i] + " 2>/dev/null";
        int exitCode = system(cmd.c_str());
        int status = WEXITSTATUS(exitCode);

        if (status == 0) {
            passed++;
            std::cout << "ok " << i << " — " << argv[i] << std::endl;
        } else {
            failed++;
            std::cout << "not ok " << i << " — " << argv[i] << std::endl;
            std::string output = execAndCapture(cmd);
            if (!output.empty()) {
                std::cout << "# " << output << std::endl;
            }
        }
    }

    std::cout << "1.." << (argc - 1) << std::endl;
    std::cout << "# " << (passed + failed) << " tests, " << passed << " passed, " << failed << " failed"
              << std::endl;

    return failed;
}
