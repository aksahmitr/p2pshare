#include "file_manager.hpp"
#include "meta_file.hpp"
#include "peer_client.hpp"
#include "peer_server.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

constexpr size_t PIECE_SIZE = 256 * 1024;

std::vector<Piece> download_file(const MetaFile &meta,
                                 const std::vector<std::string> &peers,
                                 const std::string &out_path) {
    std::vector<Piece> pieces(meta.piece_hashes.size());

    for (size_t i = 0; i < meta.piece_hashes.size(); ++i) {
        bool success = false;
        for (const auto &peer : peers) {
            size_t colon = peer.find(':');
            if (colon == std::string::npos)
                continue;
            std::string ip = peer.substr(0, colon);
            int port = std::stoi(peer.substr(colon + 1));

            try {
                Piece p = request_piece(ip, port, i, meta.piece_hashes[i]);
                pieces[i] = std::move(p);
                std::cout << "Downloaded piece " << i << " from " << peer
                          << "\n";
                success = true;
                break;
            } catch (const std::exception &e) {
                std::cerr << "Failed to get piece " << i << " from " << peer
                          << " - " << e.what() << "\n";
            }
        }

        if (!success) {
            throw std::runtime_error("Failed to download piece #" +
                                     std::to_string(i));
        }
        std::cout << "[" << i + 1 << "/" << meta.piece_hashes.size()
                  << "] Downloaded piece " << i << "\n";
    }

    std::ofstream out(out_path, std::ios::binary);
    for (const auto &p : pieces) {
        out.write(p.data.data(), p.data.size());
    }

    std::cout << "File assembled at: " << out_path << "\n";

    return pieces;
}

std::vector<std::string> parse_peers(const std::string &arg) {
    std::vector<std::string> result;
    std::stringstream ss(arg);
    std::string token;
    while (std::getline(ss, token, ',')) {
        result.push_back(token);
    }
    return result;
}

int main(int argc, char *argv[]) {
    std::string file_path, meta_path, out_dir;
    int port = -1;
    std::vector<std::string> peers;
    bool is_seeder = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--file" && i + 1 < argc) {
            file_path = argv[++i];
        } else if (arg == "--meta" && i + 1 < argc) {
            meta_path = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--peers" && i + 1 < argc) {
            peers = parse_peers(argv[++i]);
        } else if (arg == "--out-dir" && i + 1 < argc) {
            out_dir = argv[++i];
        } else if (arg == "--seed") {
            is_seeder = true;
        } else {
            std::cerr << "Unknown or incomplete argument: " << arg << "\n";
            return 1;
        }
    }

    if (meta_path.empty() || port == -1 || (is_seeder && file_path.empty()) ||
        (!is_seeder && peers.empty()) || (!is_seeder && out_dir.empty())) {
        std::cerr
            << "Usage:\n"
            << "  Seeder:     p2pshare --seed --file <file> --meta <meta> "
               "--port <port>\n"
            << "  Downloader: p2pshare --meta <meta> --port <port> "
               "--peers ip:port,... --out-dir <dir>\n";
        return 1;
    }

    std::vector<Piece> pieces;
    MetaFile meta;
    if (is_seeder) {
        if (!std::filesystem::exists(file_path)) {
            std::cerr << "Seeder file does not exist: " << file_path << "\n";
            return 1;
        }

        pieces = chunk_file(file_path, PIECE_SIZE);
        meta = MetaFile::generate(
            pieces, PIECE_SIZE,
            std::filesystem::path(file_path).filename().string());
        meta.save_to_file(meta_path);
    } else {
        if (peers.empty()) {
            std::cerr << "No peers specified for download\n";
            return 1;
        }

        meta = MetaFile::load_from_file(meta_path);

        try {
            std::filesystem::path full_out_path =
                std::filesystem::path(out_dir) / meta.file_name;
            pieces = download_file(meta, peers, full_out_path);
        } catch (const std::exception &e) {
            std::cerr << "Download failed: " << e.what() << "\n";
            return 1;
        }
    }

    start_peer_server(pieces, port);
    return 0;
}
