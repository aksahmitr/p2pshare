#include "file_manager.hpp"
#include "sha1_util.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

std::vector<Piece> chunk_file(const std::string &filename, size_t piece_size) {
    std::ifstream file(filename, std::ios::binary);
    std::vector<Piece> pieces;

    if (!file) {
        perror("Error: Failed to open file");
        return pieces;
    }

    int index = 0;
    while (file.good()) {
        std::vector<char> buffer(piece_size);
        file.read(buffer.data(), piece_size);
        std::streamsize bytesRead = file.gcount();
        buffer.resize(bytesRead);

        if (bytesRead == 0) {
            break;
        }

        std::string hash = sha1_hex(buffer);
        pieces.push_back({index++, buffer, hash});
    }

    if (file.bad()) {
        throw std::runtime_error("I/O error while reading file: " + filename);
    }

    return pieces;
}

void print_piece_hashes(const std::vector<Piece> &pieces) {
    for (const auto &p : pieces) {
        std::cout << "Piece " << p.index << ": " << p.hash << "\n";
    }
}
