#include "core/Core.h"
#include "math/Math.h"
#include "fs/FS.h"
#include "net/Net.h"

namespace StdLib {
    // --- File Library Functions ---
    static VMValue fileRead(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::string>(args[0])) return nullptr;
        std::string path = std::get<std::string>(args[0]);
        std::ifstream file(path);
        if (!file.is_open()) return nullptr; // Or throw a runtime error in the future
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    static VMValue fileWrite(int argCount, VMValue* args) {
        if (argCount != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) return false;
        std::string path = std::get<std::string>(args[0]);
        std::string content = std::get<std::string>(args[1]);
        
        std::ofstream file(path);
        if (!file.is_open()) return false;
        
        file << content;
        file.close();
        return true;
    }

    // --- Http Library Functions ---
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        size_t totalSize = size * nmemb;
        userp->append((char*)contents, totalSize);
        return totalSize;
    }

    static VMValue httpGet(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::string>(args[0])) return nullptr;
        std::string url = std::get<std::string>(args[0]);
        
        CURL* curl;
        CURLcode res;
        std::string readBuffer;
        
        curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "Trypillia-Http-Client/1.0");
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // follow redirects
            
            res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            
            if (res != CURLE_OK) return nullptr;
            return readBuffer;
        }
        return nullptr;
    }

    static VMValue httpPost(int argCount, VMValue* args) {
        if (argCount != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) return nullptr;
        std::string url = std::get<std::string>(args[0]);
        std::string payload = std::get<std::string>(args[1]);
        
        CURL* curl;
        CURLcode res;
        std::string readBuffer;
        
        curl = curl_easy_init();
        if (curl) {
            struct curl_slist* headers = NULL;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "Trypillia-Http-Client/1.0");
            
            res = curl_easy_perform(curl);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            
            if (res != CURLE_OK) return nullptr;
            return readBuffer;
        }
        return nullptr;
    }

    // --- Main Registration functions ---
    void registerAll(VM* vm) {
        Core::registerAll(vm);
        Math::registerAll(vm);
        
        // Register File Class
        auto fileClass = std::make_shared<ObjClass>("File");
        fileClass->statics["read"] = std::make_shared<ObjNative>("read", 1, fileRead);
        fileClass->statics["write"] = std::make_shared<ObjNative>("write", 2, fileWrite);
        vm->globals["File"] = fileClass;

        // Register Http Class
        auto httpClass = std::make_shared<ObjClass>("Http");
        httpClass->statics["get"] = std::make_shared<ObjNative>("get", 1, httpGet);
        httpClass->statics["post"] = std::make_shared<ObjNative>("post", 2, httpPost);
        vm->globals["Http"] = httpClass;
    }

    void registerSymbols(SymbolTable* scope) {
        Core::registerSymbols(scope);
        Math::registerSymbols(scope);
        
        auto addClass = [&](const std::string& name) {
            Symbol sym;
            sym.name = name;
            sym.type = "class";
            sym.isConst = true;
            scope->define(sym);
        };

        addClass("File");
        addClass("Http");
    }
}
