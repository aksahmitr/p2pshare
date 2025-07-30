#include "file_manager.hpp"
#include <iostream>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ./p2pshare <file>\n";
        return 1;
    }

    auto pieces = chunk_file(argv[1]);
    print_piece_hashes(pieces);

    return 0;
}
