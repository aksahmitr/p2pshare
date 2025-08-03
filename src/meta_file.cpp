#include "meta_file.hpp"
#include "file_manager.hpp"
#include "sha1_util.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

MetaFile MetaFile::generate(const std::vector<Piece> &pieces, size_t piece_size,
                            const std::string &file_name) {

    std::vector<std::string> hashes;
    for (const auto &p : pieces) {
        hashes.push_back(p.hash);
    }

    size_t file_sz = 0;
    for (const auto &p : pieces) {
        file_sz += p.data.size();
    }

    return MetaFile{file_name, file_sz, piece_size, hashes};
}

void MetaFile::save_to_file(const std::string &path) const {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to write meta file: " + path);
    }

    out << file_name << "\n";
    out << file_size << "\n";
    out << piece_size << "\n";
    for (const auto &hash : piece_hashes) {
        out << hash << "\n";
    }
}

MetaFile MetaFile::load_from_file(const std::string &path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Failed to read meta file: " + path);
    }

    MetaFile meta;
    std::string file_size, piece_size;

    std::getline(in, meta.file_name);
    std::getline(in, file_size);
    std::getline(in, piece_size);

    meta.file_size = std::stoull(file_size);
    meta.piece_size = std::stoull(piece_size);

    std::string hash;
    while (std::getline(in, hash)) {
        meta.piece_hashes.push_back(hash);
    }

    return meta;
}
