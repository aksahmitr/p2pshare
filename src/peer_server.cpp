#include "peer_server.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

void handle_client(int client_sock, const std::vector<Piece> &pieces) {
    int requested_index;
    ssize_t bytes =
        recv(client_sock, &requested_index, sizeof(requested_index), 0);
    if (bytes <= 0) {
        close(client_sock);
        return;
    }

    if (requested_index < 0 ||
        requested_index >= static_cast<int>(pieces.size())) {
        std::cerr << "Invalid piece requested: " << requested_index << "\n";
        close(client_sock);
        return;
    }

    const Piece &p = pieces[requested_index];
    int data_size = p.data.size();

    send(client_sock, &data_size, sizeof(data_size), 0);
    send(client_sock, p.data.data(), data_size, 0);

    close(client_sock);
}

void start_peer_server(const std::vector<Piece> &pieces, int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) <
        0) {
        perror("bind");
        close(server_fd);
        return;
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        return;
    }

    std::cout << "Peer server started on port " << port << "...\n";

    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        std::thread(handle_client, client_fd, std::cref(pieces)).detach();
    }
}
