#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <string>
#include <unordered_map>

class Symbol {
public:
    std::string name;
    std::string type;
    bool isConst;
};

class SymbolTable {
    std::unordered_map<std::string, Symbol> symbols;
    SymbolTable* parent;
    
public:
    SymbolTable(SymbolTable* parent = nullptr) : parent(parent) {}
    
    bool define(const Symbol& symbol) {
        if (symbols.count(symbol.name)) return false;
        symbols[symbol.name] = symbol;
        return true;
    }
    
    Symbol* resolve(const std::string& name) {
        if (symbols.count(name)) return &symbols[name];
        if (parent) return parent->resolve(name);
        return nullptr;
    }
};

#endif // SYMBOL_TABLE_H
