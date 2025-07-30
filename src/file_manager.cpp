#include "file_manager.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <openssl/sha.h>
#include <sstream>

constexpr size_t PIECE_SIZE = 256 * 1024;

std::string sha1_hash(const std::vector<char> &data) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char *>(data.data()), data.size(),
         hash);

    std::ostringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

std::vector<Piece> chunk_file(const std::string &filename) {
    std::ifstream file(filename, std::ios::binary);
    std::vector<Piece> pieces;

    if (!file) {
        std::cerr << "Error: Failed to open file: " << filename << "\n";
        return pieces;
    }

    int index = 0;
    while (file.good()) {
        std::vector<char> buffer(PIECE_SIZE);
        file.read(buffer.data(), PIECE_SIZE);
        std::streamsize bytesRead = file.gcount();
        buffer.resize(bytesRead);

        if (bytesRead == 0) {
            break;
        }

        std::string hash = sha1_hash(buffer);
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
