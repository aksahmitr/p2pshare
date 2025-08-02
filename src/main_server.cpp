#include "file_manager.hpp"
#include "peer_server.hpp"
#include <iostream>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: ./p2p_share <file> <port>\n";
        return 1;
    }

    auto pieces = chunk_file(argv[1]);
    print_piece_hashes(pieces);

    start_peer_server(pieces, std::stoi(argv[2]));
    return 0;
}
