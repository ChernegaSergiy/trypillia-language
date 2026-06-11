#include "Net.h"
#ifdef HAS_CURL
#include <curl/curl.h>
#endif
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include "../StdLib.h"
#include <iostream>

namespace StdLib {
namespace Net {

    thread_local VM* currentVM = nullptr;

#ifdef HAS_CURL
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        size_t totalSize = size * nmemb;
        userp->append((char*)contents, totalSize);
        return totalSize;
    }
#endif

    static VMValue httpGet(int argCount, VMValue* args) {
#ifdef HAS_CURL
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
            
            if (res != CURLE_OK) return makeResultErr(currentVM, curl_easy_strerror(res));
            return makeResultOk(currentVM, readBuffer);
        }
        return makeResultErr(currentVM, "Failed to initialize CURL");
#else
        return makeResultErr(currentVM, "HTTP module was disabled during compilation (CURL not found)");
#endif
    }

    static VMValue httpPost(int argCount, VMValue* args) {
#ifdef HAS_CURL
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
            
            if (res != CURLE_OK) return makeResultErr(currentVM, curl_easy_strerror(res));
            return makeResultOk(currentVM, readBuffer);
        }
        return makeResultErr(currentVM, "Failed to initialize CURL");
#else
        return makeResultErr(currentVM, "HTTP module was disabled during compilation (CURL not found)");
#endif
    }

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

    static VMValue httpServe(int argCount, VMValue* args) {
        if (argCount != 2 || !std::holds_alternative<double>(args[0])) return makeResultErr(currentVM, "Invalid arguments");
        
        int port = static_cast<int>(std::get<double>(args[0]));
        VMValue callback = args[1];

        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
            return makeResultErr(currentVM, "Bind failed");
        }
        
        if (listen(server_fd, 10) < 0) {
            return makeResultErr(currentVM, "Listen failed");
        }

        std::cout << "[HttpServer] Listening on port " << port << "..." << std::endl;

        while (true) {
            int new_socket = accept(server_fd, nullptr, nullptr);
            if (new_socket < 0) continue;

            char buffer[4096] = {0};
            read(new_socket, buffer, 4096);
            
            std::string req(buffer);
            std::string method = "";
            std::string path = "";
            
            size_t space1 = req.find(' ');
            if (space1 != std::string::npos) {
                method = req.substr(0, space1);
                size_t space2 = req.find(' ', space1 + 1);
                if (space2 != std::string::npos) {
                    path = req.substr(space1 + 1, space2 - space1 - 1);
                }
            }

            auto reqMap = std::make_shared<ObjMap>();
            reqMap->values["method"] = method;
            reqMap->values["path"] = path;
            
            VMValue cbArgs[1] = { reqMap };
            
            VMValue result = currentVM->callClosure(callback, 1, cbArgs);

            std::string responseBody = "Internal Server Error";
            if (std::holds_alternative<std::string>(result)) {
                responseBody = std::get<std::string>(result);
            } else if (std::holds_alternative<std::shared_ptr<ObjMap>>(result)) {
                auto map = std::get<std::shared_ptr<ObjMap>>(result);
                if (map->values.count("body")) {
                    responseBody = std::get<std::string>(map->values["body"]);
                }
            }

            std::string response = "HTTP/1.1 200 OK\r\n"
                                   "Content-Type: text/html\r\n"
                                   "Connection: close\r\n"
                                   "Content-Length: " + std::to_string(responseBody.length()) + "\r\n\r\n" +
                                   responseBody;

            write(new_socket, response.c_str(), response.length());
            close(new_socket);
        }

        return nullptr;
    }

    static VMValue socketConnect(int argCount, VMValue* args) {
        if (argCount != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<double>(args[1])) return nullptr;
        std::string host = std::get<std::string>(args[0]);
        int port = std::get<double>(args[1]);

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) return makeResultErr(currentVM, "Failed to create socket");

        struct hostent* server = gethostbyname(host.c_str());
        if (server == nullptr) {
            close(sock);
            return makeResultErr(currentVM, "Failed to resolve host: " + host);
        }

        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        bcopy((char*)server->h_addr, (char*)&serv_addr.sin_addr.s_addr, server->h_length);
        serv_addr.sin_port = htons(port);

        if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            close(sock);
            return makeResultErr(currentVM, "Failed to connect to host");
        }

        SocketData* data = new SocketData{sock};
        auto socketClass = std::get<std::shared_ptr<ObjClass>>(currentVM->globals["Socket"]);
        auto instance = std::make_shared<ObjInstance>(socketClass);
        instance->nativeData = data;
        instance->freeFn = freeSocket;
        return makeResultOk(currentVM, instance);
    }

    static VMValue socketSend(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::string>(args[0])) return false;
        VMValue receiver = args[-1];
        if (!std::holds_alternative<std::shared_ptr<ObjInstance>>(receiver)) return false;

        auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);
        SocketData* data = static_cast<SocketData*>(instance->nativeData);
        if (!data || data->fd == -1) return makeResultErr(currentVM, "Socket is closed");

        std::string payload = std::get<std::string>(args[0]);
        ssize_t n = write(data->fd, payload.c_str(), payload.length());
        if (n < 0) return makeResultErr(currentVM, "Failed to send data");
        return makeResultOk(currentVM, true);
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
        if (!data || data->fd == -1) return makeResultErr(currentVM, "Socket is closed");

        std::vector<char> buffer(maxLen);
        ssize_t n = read(data->fd, buffer.data(), maxLen);
        if (n < 0) return makeResultErr(currentVM, "Failed to receive data");

        return makeResultOk(currentVM, std::string(buffer.data(), n));
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
        return makeResultOk(currentVM, true);
    }

    void registerAll(VM* vm) {
        currentVM = vm;
        auto httpClass = std::make_shared<ObjClass>("Http");
        httpClass->statics["get"] = std::make_shared<ObjNative>("get", 1, httpGet);
        httpClass->statics["post"] = std::make_shared<ObjNative>("post", 2, httpPost);
        httpClass->statics["serve"] = std::make_shared<ObjNative>("serve", 2, httpServe);
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
