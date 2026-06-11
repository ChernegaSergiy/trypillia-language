#include "Net.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define close closesocket
#define write(s, b, l) send(s, b, static_cast<int>(l), 0)
#define read(s, b, l) recv(s, b, static_cast<int>(l), 0)
typedef int ssize_t;
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif
#include <cstring>
#include <vector>
#include "../StdLib.h"
#include <iostream>

#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>

namespace StdLib {
namespace Net {

    thread_local VM* currentVM = nullptr;

    static bool parseUrl(const std::string& url, std::string& host, int& port, std::string& path) {
        size_t pos = 0;
        if (url.find("http://") == 0) {
            pos = 7;
            port = 80;
        } else if (url.find("https://") == 0) {
            pos = 8;
            port = 443;
        } else {
            return false;
        }

        size_t pathPos = url.find('/', pos);
        size_t colonPos = url.find(':', pos);

        if (pathPos == std::string::npos) {
            path = "/";
            pathPos = url.length();
        } else {
            path = url.substr(pathPos);
        }

        if (colonPos != std::string::npos && colonPos < pathPos) {
            host = url.substr(pos, colonPos - pos);
            try {
                port = std::stoi(url.substr(colonPos + 1, pathPos - colonPos - 1));
            } catch (...) {
                return false;
            }
        } else {
            host = url.substr(pos, pathPos - pos);
        }

        return true;
    }

    static std::string makeHttpRequest(const std::string& host, int port, const std::string& requestStr, bool& success, std::string& errorMsg) {
        if (port == 443) {
            mbedtls_net_context server_fd;
            mbedtls_entropy_context entropy;
            mbedtls_ctr_drbg_context ctr_drbg;
            mbedtls_ssl_context ssl;
            mbedtls_ssl_config conf;

            mbedtls_net_init(&server_fd);
            mbedtls_ssl_init(&ssl);
            mbedtls_ssl_config_init(&conf);
            mbedtls_ctr_drbg_init(&ctr_drbg);
            mbedtls_entropy_init(&entropy);

            const char* pers = "trypillia_https";
            if (mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char*)pers, strlen(pers)) != 0) {
                errorMsg = "Failed to seed random number generator.";
                success = false;
                return "";
            }

            if (mbedtls_net_connect(&server_fd, host.c_str(), std::to_string(port).c_str(), MBEDTLS_NET_PROTO_TCP) != 0) {
                errorMsg = "Failed to connect to host (HTTPS).";
                success = false;
                return "";
            }

            if (mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
                errorMsg = "Failed to set SSL config.";
                success = false;
                return "";
            }

            mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
            mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

            if (mbedtls_ssl_setup(&ssl, &conf) != 0) {
                errorMsg = "Failed to setup SSL.";
                success = false;
                return "";
            }

            mbedtls_ssl_set_hostname(&ssl, host.c_str());
            mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

            while (true) {
                int ret = mbedtls_ssl_handshake(&ssl);
                if (ret == 0) break;
                if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                    errorMsg = "SSL handshake failed.";
                    success = false;
                    return "";
                }
            }

            int ret;
            size_t written = 0;
            while (written < requestStr.length()) {
                ret = mbedtls_ssl_write(&ssl, (const unsigned char*)requestStr.c_str() + written, requestStr.length() - written);
                if (ret <= 0) {
                    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                        errorMsg = "Failed to write via SSL.";
                        success = false;
                        return "";
                    }
                } else {
                    written += static_cast<size_t>(ret);
                }
            }

            std::string response;
            unsigned char buffer[4096];
            while (true) {
                ret = mbedtls_ssl_read(&ssl, buffer, sizeof(buffer));
                if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
                if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) break;
                if (ret < 0) break;
                if (ret == 0) break;
                response.append((char*)buffer, static_cast<size_t>(ret));
            }

            mbedtls_ssl_close_notify(&ssl);
            mbedtls_ssl_free(&ssl);
            mbedtls_ssl_config_free(&conf);
            mbedtls_ctr_drbg_free(&ctr_drbg);
            mbedtls_entropy_free(&entropy);
            mbedtls_net_free(&server_fd);

            success = true;
            return response;
        }

        int sock = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
        if (sock < 0) {
            success = false;
            errorMsg = "Failed to create socket.";
            return "";
        }

        struct hostent* server = gethostbyname(host.c_str());
        if (server == nullptr) {
            close(sock);
            success = false;
            errorMsg = "Failed to resolve host: " + host;
            return "";
        }

        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        memcpy((char*)&serv_addr.sin_addr.s_addr, (char*)server->h_addr, static_cast<size_t>(server->h_length));
        serv_addr.sin_port = htons(static_cast<uint16_t>(port));

        if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            close(sock);
            success = false;
            errorMsg = "Failed to connect to host.";
            return "";
        }

        if (write(sock, requestStr.c_str(), requestStr.length()) < 0) {
            close(sock);
            success = false;
            errorMsg = "Failed to write to socket.";
            return "";
        }

        std::string response;
        char buffer[4096];
        while (true) {
            ssize_t n = read(sock, buffer, sizeof(buffer));
            if (n <= 0) break;
            response.append(buffer, static_cast<size_t>(n));
        }
        
        close(sock);
        success = true;
        return response;
    }

    static VMValue httpGet(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::string>(args[0])) return nullptr;
        std::string url = std::get<std::string>(args[0]);
        
        std::string host, path;
        int port;
        if (!parseUrl(url, host, port, path)) {
            return makeResultErr(currentVM, "Invalid URL format.");
        }

        std::string requestStr = "GET " + path + " HTTP/1.1\r\n"
                                 "Host: " + host + "\r\n"
                                 "Connection: close\r\n"
                                 "User-Agent: Trypillia-Http-Client/1.0\r\n"
                                 "\r\n";

        bool success = false;
        std::string errorMsg;
        std::string response = makeHttpRequest(host, port, requestStr, success, errorMsg);

        if (!success) {
            return makeResultErr(currentVM, errorMsg);
        }

        size_t headerEnd = response.find("\r\n\r\n");
        if (headerEnd != std::string::npos) {
            response = response.substr(headerEnd + 4);
        }

        return makeResultOk(currentVM, response);
    }

    static VMValue httpPost(int argCount, VMValue* args) {
        if (argCount != 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) return nullptr;
        std::string url = std::get<std::string>(args[0]);
        std::string payload = std::get<std::string>(args[1]);
        
        std::string host, path;
        int port;
        if (!parseUrl(url, host, port, path)) {
            return makeResultErr(currentVM, "Invalid URL format.");
        }

        std::string requestStr = "POST " + path + " HTTP/1.1\r\n"
                                 "Host: " + host + "\r\n"
                                 "Content-Type: application/json\r\n"
                                 "Connection: close\r\n"
                                 "User-Agent: Trypillia-Http-Client/1.0\r\n"
                                 "Content-Length: " + std::to_string(payload.length()) + "\r\n"
                                 "\r\n" + payload;

        bool success = false;
        std::string errorMsg;
        std::string response = makeHttpRequest(host, port, requestStr, success, errorMsg);

        if (!success) {
            return makeResultErr(currentVM, errorMsg);
        }

        size_t headerEnd = response.find("\r\n\r\n");
        if (headerEnd != std::string::npos) {
            response = response.substr(headerEnd + 4);
        }

        return makeResultOk(currentVM, response);
    }

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

        int server_fd = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
        int opt = 1;
#ifdef _WIN32
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(static_cast<uint16_t>(port));

        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
            return makeResultErr(currentVM, "Bind failed");
        }
        
        if (listen(server_fd, 10) < 0) {
            return makeResultErr(currentVM, "Listen failed");
        }

        std::cout << "[HttpServer] Listening on port " << port << "..." << std::endl;

        while (true) {
            int new_socket = static_cast<int>(accept(server_fd, nullptr, nullptr));
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
        int port = static_cast<int>(std::get<double>(args[1]));

        int sock = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
        if (sock < 0) return makeResultErr(currentVM, "Failed to create socket");

        struct hostent* server = gethostbyname(host.c_str());
        if (server == nullptr) {
            close(sock);
            return makeResultErr(currentVM, "Failed to resolve host: " + host);
        }

        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        memcpy((char*)&serv_addr.sin_addr.s_addr, (char*)server->h_addr, static_cast<size_t>(server->h_length));
        serv_addr.sin_port = htons(static_cast<uint16_t>(port));

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
            maxLen = static_cast<int>(std::get<double>(args[0]));
        } else if (argCount != 0) return nullptr;

        VMValue receiver = args[-1];
        if (!std::holds_alternative<std::shared_ptr<ObjInstance>>(receiver)) return nullptr;

        auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);
        SocketData* data = static_cast<SocketData*>(instance->nativeData);
        if (!data || data->fd == -1) return makeResultErr(currentVM, "Socket is closed");

        std::vector<char> buffer(static_cast<size_t>(maxLen));
        ssize_t n = read(data->fd, buffer.data(), static_cast<size_t>(maxLen));
        if (n < 0) return makeResultErr(currentVM, "Failed to receive data");

        return makeResultOk(currentVM, std::string(buffer.data(), static_cast<size_t>(n)));
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
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
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
