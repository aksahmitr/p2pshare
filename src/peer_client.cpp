#include "peer_client.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <sha1_util.hpp>
#include <sys/socket.h>
#include <unistd.h>

Piece request_piece(const std::string &ip, int port, int piece_index,
                    const std::string &expected_hash) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        throw std::runtime_error("Socket creation failed");
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr);

    if (connect(sock, reinterpret_cast<sockaddr *>(&server_addr),
                sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        throw std::runtime_error("Connection failed");
    }

    send(sock, &piece_index, sizeof(piece_index), 0);

    int size = 0;
    recv(sock, &size, sizeof(size), 0);

    std::vector<char> data(size);
    size_t received = 0;
    while (received < (size_t)size) {
        ssize_t r = recv(sock, data.data() + received, size - received, 0);
        if (r <= 0)
            break;
        received += r;
    }

    close(sock);

    std::string hash = sha1_hex(data);
    if (hash != expected_hash) {
        std::cerr << "Hash mismatch for piece " << piece_index << "\n";
        throw std::runtime_error("Integrity check failed");
    }

    return {piece_index, std::move(data), hash};
}
