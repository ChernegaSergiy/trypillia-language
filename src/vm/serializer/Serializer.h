#ifndef SERIALIZER_H
#define SERIALIZER_H

#include "../Chunk.h"
#include <fstream>
#include <string>
#include <memory>

class Serializer {
public:
    static bool saveToFile(const std::shared_ptr<ObjFunction>& function, const std::string& path);
    static std::shared_ptr<ObjFunction> loadFromFile(const std::string& path);

private:
    static void writeFunction(std::ofstream& out, const std::shared_ptr<ObjFunction>& function);
    static std::shared_ptr<ObjFunction> readFunction(std::ifstream& in);

    static void writeChunk(std::ofstream& out, const std::shared_ptr<Chunk>& chunk);
    static std::shared_ptr<Chunk> readChunk(std::ifstream& in);

    static void writeValue(std::ofstream& out, const VMValue& value);
    static VMValue readValue(std::ifstream& in);

    static void writeString(std::ofstream& out, const std::string& str);
    static std::string readString(std::ifstream& in);
};

#endif
