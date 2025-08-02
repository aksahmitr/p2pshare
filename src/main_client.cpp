#include "peer_client.hpp"
#include <fstream>
#include <iostream>

int main(int argc, char *argv[]) {
    if (argc != 5) {
        std::cerr
            << "Usage: ./peer_client <ip> <port> <index> <expected_hash>\n";
        return 1;
    }

    std::string ip = argv[1];
    int port = std::stoi(argv[2]);
    int index = std::stoi(argv[3]);
    std::string expected_hash = argv[4];

    try {
        Piece p = request_piece(ip, port, index, expected_hash);
        std::cout << "Received piece #" << p.index << " (" << p.data.size()
                  << " bytes)\n";
        std::ofstream out("received_piece_" + std::to_string(index) + ".bin",
                          std::ios::binary);
        out.write(p.data.data(), p.data.size());
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}
