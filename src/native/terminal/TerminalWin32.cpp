#ifdef _WIN32
#include "Terminal.h"
#include <iostream>
#include <stdio.h>
#include <string>
#include <signal.h>
#include <stdlib.h>
#include <vector>
#include <windows.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

namespace StdLib {
namespace TerminalModule {

    thread_local VM* currentVM = nullptr;
    static DWORD orig_out_mode;
    static DWORD orig_in_mode;
    static bool inRawMode = false;
    static bool atexitRegistered = false;

    static void restore_terminal_state() {
        if (inRawMode) {
            SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), orig_out_mode);
            SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), orig_in_mode);
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
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
            std::vector<VMValue> listElements;
            listElements.push_back(static_cast<double>(csbi.dwCursorPosition.X + 1));
            listElements.push_back(static_cast<double>(csbi.dwCursorPosition.Y + 1));
            return std::make_shared<ObjList>(listElements);
        }
        return nullptr;
    }

    static VMValue terminalEnableRawMode(int argCount, VMValue* args) {
        if (!inRawMode) {
            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
            GetConsoleMode(hOut, &orig_out_mode);
            GetConsoleMode(hIn, &orig_in_mode);
            
            DWORD outMode = orig_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            DWORD inMode = orig_in_mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
            
            SetConsoleMode(hOut, outMode);
            SetConsoleMode(hIn, inMode);
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
        restore_terminal_state();
        return nullptr;
    }

    static VMValue terminalReadChar(int argCount, VMValue* args) {
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        INPUT_RECORD ir;
        DWORD read;
        while (ReadConsoleInput(hIn, &ir, 1, &read) && read > 0) {
            if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
                if (ir.Event.KeyEvent.wVirtualKeyCode == VK_UP) return std::string("up");
                if (ir.Event.KeyEvent.wVirtualKeyCode == VK_DOWN) return std::string("down");
                if (ir.Event.KeyEvent.wVirtualKeyCode == VK_LEFT) return std::string("left");
                if (ir.Event.KeyEvent.wVirtualKeyCode == VK_RIGHT) return std::string("right");
                if (ir.Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE) return std::string("escape");
                
                char c = ir.Event.KeyEvent.uChar.AsciiChar;
                if (c != 0) return std::string(1, c);
            }
        }
        return nullptr;
    }

    static VMValue terminalReadByte(int argCount, VMValue* args) {
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        INPUT_RECORD ir;
        DWORD read;
        while (ReadConsoleInput(hIn, &ir, 1, &read) && read > 0) {
            if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
                return static_cast<double>(ir.Event.KeyEvent.uChar.AsciiChar);
            }
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
#endif
