#pragma once
#include "piece.hpp"
#include "routing_table.hpp"
#include <string>
#include <vector>

struct MetaFile {
    std::string file_name;
    size_t file_size;
    size_t piece_size;
    std::vector<std::string> piece_hashes;

    static MetaFile generate(const std::vector<Piece> &pieces,
                             size_t piece_size, const std::string &file_name);
    static MetaFile load_from_file(const std::string &path);
    void save_to_file(const std::string &path) const;
    std::array<uint8_t, NODE_ID_BYTES> hash() const;
};
