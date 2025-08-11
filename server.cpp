// server.cpp
// Minimal multithreaded TCP echo server in C++ (POSIX / Linux)
// Compile: g++ -std=c++17 server.cpp -pthread -o server
// Run: ./server 8080

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

static void log(const std::string &s) {
    using namespace std::chrono;
    auto now = system_clock::to_time_t(system_clock::now());
    std::cout << "[" << std::put_time(std::localtime(&now), "%F %T") << "] " << s << "\n";
}

void handle_client(int client_fd, sockaddr_in client_addr) {
    char addr_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, addr_str, sizeof(addr_str));
    int client_port = ntohs(client_addr.sin_port);
    log("Connected: " + std::string(addr_str) + ":" + std::to_string(client_port));

    const size_t BUF_SZ = 4096;
    std::vector<char> buf(BUF_SZ);

    while (true) {
        ssize_t n = recv(client_fd, buf.data(), (int)BUF_SZ, 0);
        if (n > 0) {
            // Echo back
            ssize_t sent = 0;
            while (sent < n) {
                ssize_t s = send(client_fd, buf.data() + sent, (size_t)(n - sent), 0);
                if (s < 0) {
                    log("Error in send(): " + std::string(std::strerror(errno)));
                    close(client_fd);
                    return;
                }
                sent += s;
            }
        } else if (n == 0) {
            // Peer closed connection
            log("Client disconnected: " + std::string(addr_str) + ":" + std::to_string(client_port));
            break;
        } else {
            // Error
            if (errno == EINTR) continue; // interrupted, try again
            log("Error in recv(): " + std::string(std::strerror(errno)));
            break;
        }
    }

    close(client_fd);
}

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc >= 2) port = std::stoi(argv[1]);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        std::cerr << "socket() failed: " << std::strerror(errno) << "\n";
        return 1;
    }

    // Allow quick restart: SO_REUSEADDR
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "setsockopt() failed: " << std::strerror(errno) << "\n";
        close(listen_fd);
        return 1;
    }

    sockaddr_in srv_addr{};
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = INADDR_ANY; // 0.0.0.0
    srv_addr.sin_port = htons(port);

    if (bind(listen_fd, (sockaddr*)&srv_addr, sizeof(srv_addr)) < 0) {
        std::cerr << "bind() failed: " << std::strerror(errno) << "\n";
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, SOMAXCONN) < 0) {
        std::cerr << "listen() failed: " << std::strerror(errno) << "\n";
        close(listen_fd);
        return 1;
    }

    log("Listening on port " + std::to_string(port));

    // Accept loop
    while (true) {
        sockaddr_in client_addr{};
        socklen_t addrlen = sizeof(client_addr);
        int client_fd = accept(listen_fd, (sockaddr*)&client_addr, &addrlen);
        if (client_fd < 0) {
            if (errno == EINTR) continue; // interrupted system call; continue accepting
            std::cerr << "accept() failed: " << std::strerror(errno) << "\n";
            break;
        }

        // Spawn a detached thread to handle the client
        std::thread t(handle_client, client_fd, client_addr);
        t.detach(); // let it run independently
    }

    close(listen_fd);
    return 0;
}
