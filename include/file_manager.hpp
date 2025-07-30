#pragma once
#include <string>
#include <vector>

struct Piece {
    int index;
    std::vector<char> data;
    std::string hash;
};

std::vector<Piece> chunk_file(const std::string &filename);
void print_piece_hashes(const std::vector<Piece> &pieces);
