#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstdio>
#include <mutex>
#include <thread>
#include "protocol.h"

std::mutex display;

void display_stats(const StatPacket& stats) {
    std::cout << "====================================\n";
    std::cout << "     SYSLENS SYSTEM MONITOR v1.0    \n";
    std::cout << "====================================\n";

    std::cout << stats << std::endl;
}

StatPacket receive_stat_packet(int socket) {
    StatPacket packet;
    uint16_t cpu_fixed;
    uint32_t net_mem, net_proc;

    read(socket, &cpu_fixed, sizeof(uint16_t));
    read(socket, &net_mem, sizeof(uint32_t));
    read(socket, &net_proc, sizeof(uint32_t));

    packet.cpu_load =  ntohs(cpu_fixed)/10.0;
    packet.mem_available = ntohl(net_mem);
    packet.processes_active = ntohl(net_proc);

    return packet;
}

void handle_client(int socket) {
    std::lock_guard<std::mutex> lock(display);
    StatPacket buffer = receive_stat_packet(socket);
    display_stats(buffer);
    close(socket);
}

int main() {
    // 1. Create the listening socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; // Listen on any available network interface
    addr.sin_port = htons(8080);

    // 2. Bind to the port and start listening
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 3);
    std::cout << "Server listening on 8080... Waiting for client." << std::endl;

    // 3. Accept a connection (This "blocks" / pauses the program here)
    while(true) {
        int socket = accept(server_fd, nullptr, nullptr);
        if (socket >= 0) {
            std::thread syslens_thread(handle_client, socket);
            syslens_thread.detach(); // run independently
        }
    }

    close(server_fd);
    return 0;
}

