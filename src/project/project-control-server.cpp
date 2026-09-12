// 09 12 2026
/* purpose
* Implements the loopback control server that forwards agent commands to
* ProjectControl.
* Does NOT own project logic.
*/
#include "project/project-control-server.h"

#include "project/project-control.h"

#include <atomic>
#include <cstdio>
#include <string>
#include <thread>

#include <winsock2.h>
#include <ws2tcpip.h>

namespace Project {

struct ControlServer::Impl {
    std::atomic<bool> running{false};
    std::uint16_t port = 0;
    SOCKET listenSocket = INVALID_SOCKET;
    std::thread worker;
};

namespace {

void serveClient(SOCKET client)
{
    std::string buffer;
    char chunk[1024];
    for (;;) {
        const int bytes = recv(client, chunk, sizeof(chunk), 0);
        if (bytes <= 0)
            break;
        buffer.append(chunk, chunk + bytes);
        std::size_t newline = buffer.find('\n');
        while (newline != std::string::npos) {
            std::string command = buffer.substr(0, newline);
            buffer.erase(0, newline + 1);
            if (!command.empty() && command.back() == '\r')
                command.pop_back();
            std::string response = ProjectControl::instance().handle(command);
            response += '\n';
            send(client, response.data(), (int)response.size(), 0);
            newline = buffer.find('\n');
        }
    }
}

void serverMain(ControlServer::Impl* impl)
{
    while (impl->running.load()) {
        sockaddr_in from{};
        int fromLen = sizeof(from);
        SOCKET client = accept(impl->listenSocket, (sockaddr*)&from, &fromLen);
        if (client == INVALID_SOCKET)
            break;  // listen socket closed
        serveClient(client);
        closesocket(client);
    }
}

} // namespace

ControlServer& ControlServer::instance()
{
    static ControlServer server;
    return server;
}

bool ControlServer::start(std::uint16_t port)
{
    if (impl_ && impl_->running.load())
        return true;

    if (!impl_)
        impl_ = new Impl();

    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::printf("[PROJECT CONTROL] WSAStartup failed\n");
        return false;
    }

    impl_->listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (impl_->listenSocket == INVALID_SOCKET) {
        std::printf("[PROJECT CONTROL] socket failed error=%d\n", WSAGetLastError());
        WSACleanup();
        return false;
    }

    const char loopback[] = "127.0.0.1";
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, loopback, &address.sin_addr);

    int reuse = 1;
    setsockopt(impl_->listenSocket, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&reuse, sizeof(reuse));

    if (bind(impl_->listenSocket, (sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        std::printf("[PROJECT CONTROL] bind 127.0.0.1:%u failed error=%d\n",
                    port, WSAGetLastError());
        closesocket(impl_->listenSocket);
        impl_->listenSocket = INVALID_SOCKET;
        WSACleanup();
        return false;
    }
    if (listen(impl_->listenSocket, 4) == SOCKET_ERROR) {
        std::printf("[PROJECT CONTROL] listen failed error=%d\n", WSAGetLastError());
        closesocket(impl_->listenSocket);
        impl_->listenSocket = INVALID_SOCKET;
        WSACleanup();
        return false;
    }

    impl_->port = port;
    impl_->running = true;
    impl_->worker = std::thread(&serverMain, impl_);
    std::printf("[PROJECT CONTROL] listening on 127.0.0.1:%u\n", port);
    return true;
}

void ControlServer::stop()
{
    if (!impl_)
        return;
    impl_->running = false;
    if (impl_->listenSocket != INVALID_SOCKET) {
        closesocket(impl_->listenSocket);
        impl_->listenSocket = INVALID_SOCKET;
    }
    if (impl_->worker.joinable())
        impl_->worker.join();
    WSACleanup();
    std::printf("[PROJECT CONTROL] stopped\n");
}

bool ControlServer::running() const
{
    return impl_ && impl_->running.load();
}

std::uint16_t ControlServer::port() const
{
    return impl_ ? impl_->port : 0;
}

} // namespace Project
