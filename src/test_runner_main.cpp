#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <sys/wait.h>

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

static int execAndGetStatus(const std::string &cmd) {
    int ret = system(cmd.c_str());
    if (ret == -1)
        return -1;
    return WEXITSTATUS(ret);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <test-file> [test-file...]" << std::endl;
        return 1;
    }

    std::string trypilliaBin = "trypillia";
    {
        std::string selfPath(argv[0]);
        auto slash = selfPath.find_last_of("/\\");
        if (slash != std::string::npos) {
            std::string dir = selfPath.substr(0, slash);
            std::string candidate = dir + "/trypillia";
            if (FILE *f = fopen(candidate.c_str(), "r")) {
                fclose(f);
                trypilliaBin = candidate;
            }
        }
    }

    int passed = 0;
    int failed = 0;

    for (int i = 1; i < argc; i++) {
        std::string cmd = trypilliaBin + " " + argv[i] + " > /dev/null 2>&1";
        int exitCode = execAndGetStatus(cmd);
        bool ok = (exitCode == 0);

        if (ok) {
            passed++;
            std::cout << "ok " << i << " — " << argv[i] << std::endl;
        } else {
            failed++;
            std::cout << "not ok " << i << " — " << argv[i] << std::endl;
            std::string output = execAndCapture(trypilliaBin + " " + argv[i] + " 2>&1");
            if (!output.empty()) {
                std::cout << "# " << output;
                if (output.back() != '\n')
                    std::cout << std::endl;
            }
        }
    }

    std::cout << "1.." << (argc - 1) << std::endl;
    std::cout << "# " << (passed + failed) << " tests, " << passed << " passed, " << failed << " failed"
              << std::endl;

    return failed;
}
