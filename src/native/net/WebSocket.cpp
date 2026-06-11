#include "WebSocket.h"
#include "../crypto/Crypto.h"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define close closesocket
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include <cstring>
#include <vector>
#include <iostream>
#include <sstream>
#include "../StdLib.h"

namespace StdLib {
namespace WebSocketModule {

    thread_local VM* currentVM = nullptr;

    struct WSServerData {
        int fd = -1;
    };

    struct WSClientData {
        int fd = -1;
    };

    static void freeServer(void* nativeData) {
        WSServerData* data = static_cast<WSServerData*>(nativeData);
        if (data) {
            if (data->fd != -1) close(data->fd);
            delete data;
        }
    }

    static void freeClient(void* nativeData) {
        WSClientData* data = static_cast<WSClientData*>(nativeData);
        if (data) {
            if (data->fd != -1) close(data->fd);
            delete data;
        }
    }

    static VMValue wsServerCreate(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<double>(args[0])) return makeResultErr(currentVM, "Invalid port");
        int port = static_cast<int>(std::get<double>(args[0]));

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
        
        if (listen(server_fd, 100) < 0) {
            return makeResultErr(currentVM, "Listen failed");
        }

        WSServerData* data = new WSServerData{server_fd};
        auto klass = std::get<std::shared_ptr<ObjClass>>(currentVM->globals["WebSocketServer"]);
        auto instance = std::make_shared<ObjInstance>(klass);
        instance->nativeData = data;
        instance->freeFn = freeServer;

        return makeResultOk(currentVM, instance);
    }

    // Parse HTTP headers to find Sec-WebSocket-Key
    static std::string extractHeader(const std::string& request, const std::string& header) {
        size_t pos = request.find(header + ": ");
        if (pos == std::string::npos) return "";
        pos += header.length() + 2;
        size_t end = request.find("\r\n", pos);
        if (end == std::string::npos) return "";
        return request.substr(pos, end - pos);
    }

    // We can manually call sha1Base64 from Crypto
    extern VMValue cryptoSha1Base64(int argCount, VMValue* args);

    static VMValue wsServerAccept(int argCount, VMValue* args) {
        if (argCount != 0) return nullptr;
        VMValue receiver = args[-1];
        auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);
        WSServerData* data = static_cast<WSServerData*>(instance->nativeData);
        
        if (!data || data->fd == -1) return makeResultErr(currentVM, "Server closed");

        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int new_socket = accept(data->fd, (struct sockaddr *)&client_addr, &client_len);
        
        if (new_socket < 0) return makeResultErr(currentVM, "Accept failed");

        // Read HTTP request for handshake
        char buffer[4096] = {0};
        ssize_t bytes_read = read(new_socket, buffer, 4095);
        if (bytes_read <= 0) {
            close(new_socket);
            return makeResultErr(currentVM, "Failed to read handshake");
        }

        std::string request(buffer);
        std::string wsKey = extractHeader(request, "Sec-WebSocket-Key");
        if (wsKey.empty()) {
            std::string bad = "HTTP/1.1 400 Bad Request\r\n\r\n";
            write(new_socket, bad.c_str(), bad.length());
            close(new_socket);
            return makeResultErr(currentVM, "Not a websocket request");
        }

        // Generate Sec-WebSocket-Accept
        std::string magic = wsKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        
        // Use Crypto module's SHA1 + Base64 logic natively!
        // We have to mock VMValue to call it directly
        // Wait, CryptoModule::cryptoSha1Base64 is static. 
        // We'll implement it inline to avoid linking issues, or we can just use the public class.
        VMValue shaArg[1] = { magic };
        VMValue acceptKeyVal = currentVM->globals["Crypto"];
        auto cryptoClass = std::get<std::shared_ptr<ObjClass>>(acceptKeyVal);
        auto shaFn = std::get<std::shared_ptr<ObjNative>>(cryptoClass->statics["sha1Base64"]);
        VMValue result = shaFn->function(1, shaArg);
        
        std::string acceptKey = "";
        if (std::holds_alternative<std::shared_ptr<ObjInstance>>(result)) {
            auto resInst = std::get<std::shared_ptr<ObjInstance>>(result);
            acceptKey = std::get<std::string>(resInst->fields["value"]);
        }

        std::string response = "HTTP/1.1 101 Switching Protocols\r\n"
                               "Upgrade: websocket\r\n"
                               "Connection: Upgrade\r\n"
                               "Sec-WebSocket-Accept: " + acceptKey + "\r\n\r\n";
                               
        write(new_socket, response.c_str(), response.length());

        return makeResultOk(currentVM, (double)new_socket);
    }

    static VMValue wsFromFd(int argCount, VMValue* args) {
        if (argCount != 1) return nullptr;
        int fd = -1;
        if (std::holds_alternative<double>(args[0])) {
            fd = static_cast<int>(std::get<double>(args[0]));
        } else if (std::holds_alternative<std::string>(args[0])) {
            fd = std::stoi(std::get<std::string>(args[0]));
        } else {
            return nullptr;
        }

        WSClientData* data = new WSClientData{fd};
        auto klass = std::get<std::shared_ptr<ObjClass>>(currentVM->globals["WebSocket"]);
        auto instance = std::make_shared<ObjInstance>(klass);
        instance->nativeData = data;
        instance->freeFn = freeClient;

        return makeResultOk(currentVM, instance);
    }

    static VMValue wsSend(int argCount, VMValue* args) {
        if (argCount != 1 || !std::holds_alternative<std::string>(args[0])) return nullptr;
        VMValue receiver = args[-1];
        auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);
        WSClientData* data = static_cast<WSClientData*>(instance->nativeData);
        
        if (!data || data->fd == -1) return makeResultErr(currentVM, "Socket closed");

        std::string msg = std::get<std::string>(args[0]);
        size_t len = msg.length();
        std::vector<uint8_t> frame;
        frame.push_back(0x81); // FIN + Text frame

        if (len <= 125) {
            frame.push_back(static_cast<uint8_t>(len));
        } else if (len <= 65535) {
            frame.push_back(126);
            frame.push_back((len >> 8) & 0xFF);
            frame.push_back(len & 0xFF);
        } else {
            frame.push_back(127);
            for (int i = 7; i >= 0; i--) {
                frame.push_back((len >> (i * 8)) & 0xFF);
            }
        }

        frame.insert(frame.end(), msg.begin(), msg.end());
        
        ssize_t sent = write(data->fd, frame.data(), frame.size());
        if (sent < 0) return makeResultErr(currentVM, "Write failed");
        
        return makeResultOk(currentVM, true);
    }

    static VMValue wsRecv(int argCount, VMValue* args) {
        if (argCount != 0) return nullptr;
        VMValue receiver = args[-1];
        auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);
        WSClientData* data = static_cast<WSClientData*>(instance->nativeData);
        
        if (!data || data->fd == -1) return makeResultErr(currentVM, "Socket closed");

        uint8_t header[2];
        ssize_t n = read(data->fd, header, 2);
        if (n <= 0) return makeResultErr(currentVM, "Connection closed");

        bool fin = (header[0] & 0x80) != 0;
        uint8_t opcode = header[0] & 0x0F;
        bool masked = (header[1] & 0x80) != 0;
        uint64_t payload_len = header[1] & 0x7F;

        if (opcode == 0x8) {
            // Close frame
            close(data->fd);
            data->fd = -1;
            return makeResultErr(currentVM, "Client closed connection");
        }

        if (payload_len == 126) {
            uint8_t ext[2];
            read(data->fd, ext, 2);
            payload_len = (ext[0] << 8) | ext[1];
        } else if (payload_len == 127) {
            uint8_t ext[8];
            read(data->fd, ext, 8);
            payload_len = 0;
            for (int i = 0; i < 8; i++) {
                payload_len = (payload_len << 8) | ext[i];
            }
        }

        uint8_t mask[4] = {0};
        if (masked) {
            read(data->fd, mask, 4);
        }

        std::vector<uint8_t> payload(payload_len);
        size_t total_read = 0;
        while (total_read < payload_len) {
            ssize_t r = read(data->fd, payload.data() + total_read, payload_len - total_read);
            if (r <= 0) return makeResultErr(currentVM, "Read error");
            total_read += r;
        }

        if (masked) {
            for (size_t i = 0; i < payload_len; i++) {
                payload[i] ^= mask[i % 4];
            }
        }

        std::string result(payload.begin(), payload.end());
        return makeResultOk(currentVM, result);
    }

    static VMValue wsClose(int argCount, VMValue* args) {
        if (argCount != 0) return nullptr;
        VMValue receiver = args[-1];
        auto instance = std::get<std::shared_ptr<ObjInstance>>(receiver);
        WSClientData* data = static_cast<WSClientData*>(instance->nativeData);
        
        if (data && data->fd != -1) {
            uint8_t frame[] = {0x88, 0x00}; // Close frame
            write(data->fd, frame, 2);
            close(data->fd);
            data->fd = -1;
        }
        return makeResultOk(currentVM, true);
    }

    void registerAll(VM* vm) {
        currentVM = vm;
        
        auto serverClass = std::make_shared<ObjClass>("WebSocketServer");
        serverClass->statics["create"] = std::make_shared<ObjNative>("create", 1, wsServerCreate);
        serverClass->methods["accept"] = std::make_shared<ObjNative>("accept", 0, wsServerAccept);
        vm->globals["WebSocketServer"] = serverClass;

        auto wsClass = std::make_shared<ObjClass>("WebSocket");
        wsClass->statics["fromFd"] = std::make_shared<ObjNative>("fromFd", 1, wsFromFd);
        wsClass->methods["send"] = std::make_shared<ObjNative>("send", 1, wsSend);
        wsClass->methods["recv"] = std::make_shared<ObjNative>("recv", 0, wsRecv);
        wsClass->methods["close"] = std::make_shared<ObjNative>("close", 0, wsClose);
        vm->globals["WebSocket"] = wsClass;
    }

    void registerSymbols(SymbolTable* scope) {
        Symbol symServer;
        symServer.name = "WebSocketServer";
        symServer.type = "class";
        symServer.isConst = true;
        scope->define(symServer);

        Symbol symWs;
        symWs.name = "WebSocket";
        symWs.type = "class";
        symWs.isConst = true;
        scope->define(symWs);
    }

}
}
