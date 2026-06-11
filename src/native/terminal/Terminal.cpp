#include "Terminal.h"
#include <iostream>
#include <string>
#include <vector>
#include <signal.h>

#ifdef _WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace StdLib {
namespace TerminalModule {

    thread_local VM* currentVM = nullptr;
#ifdef _WIN32
    static DWORD orig_in, orig_out;
#else
    static struct termios orig_termios;
#endif
    static bool inRaw = false;

    static void restore() {
        if (!inRaw) return;
#ifdef _WIN32
        SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), orig_in);
        SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), orig_out);
#else
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
#endif
        inRaw = false;
    }

    static VMValue terminalEnableRawMode(int argCount, VMValue* args) {
        if (inRaw) return nullptr;
#ifdef _WIN32
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        GetConsoleMode(hIn, &orig_in);
        GetConsoleMode(hOut, &orig_out);
        SetConsoleMode(hIn, orig_in & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
        SetConsoleMode(hOut, orig_out | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
        tcgetattr(STDIN_FILENO, &orig_termios);
        struct termios raw = orig_termios;
        raw.c_lflag &= ~(ECHO | ICANON);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
#endif
        inRaw = true; atexit(restore); return nullptr;
    }

    static VMValue terminalReadChar(int argCount, VMValue* args) {
#ifdef _WIN32
        INPUT_RECORD ir; DWORD n;
        while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &ir, 1, &n) && n > 0) {
            if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
                char c = ir.Event.KeyEvent.uChar.AsciiChar;
                if (c) return std::string(1, c);
            }
        }
#else
        char c; if (read(STDIN_FILENO, &c, 1) == 1) return std::string(1, c);
#endif
        return nullptr;
    }

    static VMValue terminalWrite(int argCount, VMValue* args) {
        if (argCount == 1 && std::holds_alternative<std::string>(args[0])) {
            std::cout << std::get<std::string>(args[0]) << std::flush;
        }
        return nullptr;
    }

    void registerAll(VM* vm) {
        currentVM = vm;
        auto cls = std::make_shared<ObjClass>("Terminal");
        cls->statics["enableRawMode"] = std::make_shared<ObjNative>("enableRawMode", 0, terminalEnableRawMode);
        cls->statics["disableRawMode"] = std::make_shared<ObjNative>("disableRawMode", 0, [](int, VMValue*){ restore(); return (VMValue)nullptr; });
        cls->statics["readChar"] = std::make_shared<ObjNative>("readChar", 0, terminalReadChar);
        cls->statics["write"] = std::make_shared<ObjNative>("write", 1, terminalWrite);
        vm->globals["Terminal"] = cls;
    }

    void registerSymbols(SymbolTable* scope) {
        Symbol s; s.name = "Terminal"; s.type = "class"; s.isConst = true; scope->define(s);
    }
}
}
