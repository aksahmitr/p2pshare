#pragma once
#include <string>
#include <vector>

struct MetaFile {
    std::string file_name;
    size_t file_size;
    size_t piece_size;
    std::vector<std::string> piece_hashes;

    static MetaFile generate(const std::string &path, size_t piece_size);
    static MetaFile load_from_file(const std::string &path);
    void save_to_file(const std::string &path) const;
};
