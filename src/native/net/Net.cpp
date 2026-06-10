#include "Net.h"
#include <curl/curl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <vector>

namespace StdLib {
namespace Net {

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

    static VM* currentVM = nullptr;

    // --- Socket implementation ---
    struct SocketData {
        int fd = -1;
    };

    static void freeSocket(void* nativeData) {
        SocketData* data = static_cast<SocketData*>(nativeData);
        if (data) {
            if (data->fd != -1) close(data->fd);
            delete data;
        }
    }

    static VMValue socketConnect(int argCount, VMValue* args) {
        if (argCount != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<double>(args[1])) return nullptr;
        std::string host = std::get<std::string>(args[0]);
        int port = std::get<double>(args[1]);

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) return nullptr;

        struct hostent* server = gethostbyname(host.c_str());
        if (server == nullptr) {
            close(sock);
            return nullptr;
        }

        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        bcopy((char*)server->h_addr, (char*)&serv_addr.sin_addr.s_addr, server->h_length);
        serv_addr.sin_port = htons(port);

        if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            close(sock);
            return nullptr;
        }

        SocketData* data = new SocketData{sock};
        auto socketClass = std::get<std::shared_ptr<ObjClass>>(currentVM->globals["Socket"]);
        auto instance = std::make_shared<ObjInstance>(socketClass);
        instance->nativeData = data;
        instance->freeFn = freeSocket;
        return instance;
    }

    static VMValue socketSend(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::string>(args[0])) return false;
        VMValue receiver = args[-1];
        if (!std::holds_alternative<std::shared_ptr<ObjInstance>>(receiver)) return false;

        auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);
        SocketData* data = static_cast<SocketData*>(instance->nativeData);
        if (!data || data->fd == -1) return false;

        std::string payload = std::get<std::string>(args[0]);
        ssize_t n = write(data->fd, payload.c_str(), payload.length());
        return (n >= 0);
    }

    static VMValue socketRecv(int argCount, VMValue* args) {
        int maxLen = 4096;
        if (argCount == 1 && std::holds_alternative<double>(args[0])) {
            maxLen = std::get<double>(args[0]);
        } else if (argCount != 0) return nullptr;

        VMValue receiver = args[-1];
        if (!std::holds_alternative<std::shared_ptr<ObjInstance>>(receiver)) return nullptr;

        auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);
        SocketData* data = static_cast<SocketData*>(instance->nativeData);
        if (!data || data->fd == -1) return nullptr;

        std::vector<char> buffer(maxLen);
        ssize_t n = read(data->fd, buffer.data(), maxLen);
        if (n < 0) return nullptr;

        return std::string(buffer.data(), n);
    }

    static VMValue socketClose(int argCount, VMValue* args) {
        if (argCount != 0) return false;
        VMValue receiver = args[-1];
        if (!std::holds_alternative<std::shared_ptr<ObjInstance>>(receiver)) return false;

        auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);
        SocketData* data = static_cast<SocketData*>(instance->nativeData);
        if (data && data->fd != -1) {
            close(data->fd);
            data->fd = -1;
        }
        return true;
    }

    void registerAll(VM* vm) {
        currentVM = vm;
        auto httpClass = std::make_shared<ObjClass>("Http");
        httpClass->statics["get"] = std::make_shared<ObjNative>("get", 1, httpGet);
        httpClass->statics["post"] = std::make_shared<ObjNative>("post", 2, httpPost);
        vm->globals["Http"] = httpClass;

        auto socketClass = std::make_shared<ObjClass>("Socket");
        socketClass->statics["connect"] = std::make_shared<ObjNative>("connect", 2, socketConnect);
        socketClass->methods["send"] = std::make_shared<ObjNative>("send", 1, socketSend);
        socketClass->methods["recv"] = std::make_shared<ObjNative>("recv", -1, socketRecv);
        socketClass->methods["close"] = std::make_shared<ObjNative>("close", 0, socketClose);
        vm->globals["Socket"] = socketClass;
    }

    void registerSymbols(SymbolTable* scope) {
        Symbol sym;
        sym.name = "Http";
        sym.type = "class";
        sym.isConst = true;
        scope->define(sym);

        Symbol symSocket;
        symSocket.name = "Socket";
        symSocket.type = "class";
        symSocket.isConst = true;
        scope->define(symSocket);
    }

}
}
