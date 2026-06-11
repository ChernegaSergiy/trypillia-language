#include "Terminal.h"
#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <string>
#include <signal.h>
#include <stdlib.h>

namespace StdLib {
namespace TerminalModule {

    thread_local VM* currentVM = nullptr;
    static struct termios orig_termios;
    static bool inRawMode = false;
    static bool atexitRegistered = false;

    static void restore_terminal_state() {
        if (inRawMode) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
            inRawMode = false;
        }
    }

    static void terminal_signal_handler(int sig) {
        restore_terminal_state();
        exit(128 + sig);
    }

    static VMValue terminalClear(int argCount, VMValue* args) {
        std::cout << "\033[2J\033[H";
        std::cout.flush();
        return nullptr;
    }

    static VMValue terminalCursorTo(int argCount, VMValue* args) {
        if (argCount != 2 || !std::holds_alternative<double>(args[0]) || !std::holds_alternative<double>(args[1])) return nullptr;
        int x = static_cast<int>(std::get<double>(args[0]));
        int y = static_cast<int>(std::get<double>(args[1]));
        std::cout << "\033[" << y << ";" << x << "H";
        std::cout.flush();
        return nullptr;
    }

    static VMValue terminalGetCursor(int argCount, VMValue* args) {
        // We must be in raw mode to read the response cleanly
        bool wasRaw = inRawMode;
        if (!wasRaw) {
            struct termios raw;
            tcgetattr(STDIN_FILENO, &orig_termios);
            raw = orig_termios;
            raw.c_lflag &= ~(ECHO | ICANON);
            raw.c_cc[VMIN] = 0;
            raw.c_cc[VTIME] = 2; // 200ms timeout
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        }

        std::cout << "\033[6n";
        std::cout.flush();

        char buf[32];
        unsigned int i = 0;
        while (i < sizeof(buf) - 1) {
            if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
            if (buf[i] == 'R') break;
            i++;
        }
        buf[i] = '\0';

        if (!wasRaw) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        }

        int rows = 0, cols = 0;
        if (buf[0] == '\033' && buf[1] == '[') {
            sscanf(&buf[2], "%d;%d", &rows, &cols);
        }

        std::vector<VMValue> listElements;
        listElements.push_back((double)cols);
        listElements.push_back((double)rows);
        return std::make_shared<ObjList>(listElements);
    }

    static VMValue terminalEnableRawMode(int argCount, VMValue* args) {
        if (!inRawMode) {
            tcgetattr(STDIN_FILENO, &orig_termios);
            struct termios raw = orig_termios;
            raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
            raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
            raw.c_cflag |= (CS8);
            raw.c_cc[VMIN] = 0;
            raw.c_cc[VTIME] = 1; // 100ms timeout
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
            inRawMode = true;

            if (!atexitRegistered) {
                atexit(restore_terminal_state);
                signal(SIGINT, terminal_signal_handler);
                signal(SIGTERM, terminal_signal_handler);
                atexitRegistered = true;
            }
        }
        return nullptr;
    }

    static VMValue terminalDisableRawMode(int argCount, VMValue* args) {
        if (inRawMode) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
            inRawMode = false;
        }
        return nullptr;
    }

    static VMValue terminalReadChar(int argCount, VMValue* args) {
        char c;
        if (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == '\x1b') {
                char seq[3];
                if (read(STDIN_FILENO, &seq[0], 1) == 1 && read(STDIN_FILENO, &seq[1], 1) == 1) {
                    if (seq[0] == '[') {
                        switch (seq[1]) {
                            case 'A': return std::string("up");
                            case 'B': return std::string("down");
                            case 'C': return std::string("right");
                            case 'D': return std::string("left");
                        }
                    }
                }
                return std::string("escape");
            }
            std::string s(1, c);
            return s;
        }
        return nullptr;
    }

    static VMValue terminalReadByte(int argCount, VMValue* args) {
        unsigned char c;
        if (read(STDIN_FILENO, &c, 1) == 1) {
            return static_cast<double>(c);
        }
        return nullptr;
    }

    static VMValue terminalColor(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::string>(args[0])) return nullptr;
        std::string color = std::get<std::string>(args[0]);
        std::string code = "\033[0m"; // default reset
        
        if (color == "black") code = "\033[30m";
        else if (color == "red") code = "\033[31m";
        else if (color == "green") code = "\033[32m";
        else if (color == "yellow") code = "\033[33m";
        else if (color == "blue") code = "\033[34m";
        else if (color == "magenta") code = "\033[35m";
        else if (color == "cyan") code = "\033[36m";
        else if (color == "white") code = "\033[37m";
        
        std::cout << code;
        std::cout.flush();
        return nullptr;
    }

    static VMValue terminalReset(int argCount, VMValue* args) {
        std::cout << "\033[0m";
        std::cout.flush();
        return nullptr;
    }

    static VMValue terminalWrite(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::string>(args[0])) return nullptr;
        std::cout << std::get<std::string>(args[0]);
        std::cout.flush();
        return nullptr;
    }

    void registerAll(VM* vm) {
        currentVM = vm;
        auto terminalClass = std::make_shared<ObjClass>("Terminal");
        
        terminalClass->statics["clear"] = std::make_shared<ObjNative>("clear", 0, terminalClear);
        terminalClass->statics["cursorTo"] = std::make_shared<ObjNative>("cursorTo", 2, terminalCursorTo);
        terminalClass->statics["getCursor"] = std::make_shared<ObjNative>("getCursor", 0, terminalGetCursor);
        terminalClass->statics["enableRawMode"] = std::make_shared<ObjNative>("enableRawMode", 0, terminalEnableRawMode);
        terminalClass->statics["disableRawMode"] = std::make_shared<ObjNative>("disableRawMode", 0, terminalDisableRawMode);
        terminalClass->statics["readChar"] = std::make_shared<ObjNative>("readChar", 0, terminalReadChar);
        terminalClass->statics["readByte"] = std::make_shared<ObjNative>("readByte", 0, terminalReadByte);
        terminalClass->statics["color"] = std::make_shared<ObjNative>("color", 1, terminalColor);
        terminalClass->statics["reset"] = std::make_shared<ObjNative>("reset", 0, terminalReset);
        terminalClass->statics["write"] = std::make_shared<ObjNative>("write", 1, terminalWrite);

        vm->globals["Terminal"] = terminalClass;
    }

    void registerSymbols(SymbolTable* scope) {
        Symbol sym;
        sym.name = "Terminal";
        sym.type = "class";
        sym.isConst = true;
        scope->define(sym);
    }

}
}
