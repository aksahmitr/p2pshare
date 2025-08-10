#include "file_manager.hpp"
#include "kademlia_node.hpp"
#include "meta_file.hpp"
#include "peer_client.hpp"
#include "peer_server.hpp"
#include <arpa/inet.h>
#include <filesystem>
#include <fstream>
#include <ifaddrs.h>
#include <iostream>
#include <netinet/in.h>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

constexpr size_t PIECE_SIZE = 256 * 1024;
constexpr int PORT = 9000;

std::string get_local_ip() {
    struct ifaddrs *ifaddr, *ifa;
    std::string ip;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return "";
    }

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr)
            continue;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            char addr[INET_ADDRSTRLEN];
            void *ptr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, ptr, addr, INET_ADDRSTRLEN);

            if (std::string(addr) != "127.0.0.1") {
                ip = addr;
                break;
            }
        }
    }

    freeifaddrs(ifaddr);
    return ip;
}

std::array<uint8_t, NODE_ID_BYTES> generate_random_id() {
    std::array<uint8_t, NODE_ID_BYTES> id;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dis(0, 255);

    for (auto &byte : id) {
        byte = dis(gen);
    }

    return id;
}

std::vector<Piece> download_file(const MetaFile &meta,
                                 const std::vector<std::string> &ip_list,
                                 const std::string &out_path) {
    std::vector<Piece> pieces(meta.piece_hashes.size());

    for (size_t i = 0; i < meta.piece_hashes.size(); ++i) {
        bool success = false;
        for (const auto &ip : ip_list) {
            try {
                Piece p = request_piece(ip, PORT, i, meta.piece_hashes[i]);
                pieces[i] = std::move(p);
                std::cout << "Downloaded piece " << i << " from " << ip << "\n";
                success = true;
                break;
            } catch (const std::exception &e) {
                std::cerr << "Failed to get piece " << i << " from " << ip
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

int main(int argc, char *argv[]) {
    std::string file_path, meta_path, out_dir, bootstrap;
    bool is_seeder = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--file" && i + 1 < argc) {
            file_path = argv[++i];
        } else if (arg == "--meta" && i + 1 < argc) {
            meta_path = argv[++i];
        } else if (arg == "--out-dir" && i + 1 < argc) {
            out_dir = argv[++i];
        } else if (arg == "--seed") {
            is_seeder = true;
        } else if (arg == "--bootstrap" && i + 1 < argc) {
            bootstrap = argv[++i];
        } else {
            std::cerr << "Unknown or incomplete argument: " << arg << "\n";
            return 1;
        }
    }

    if ((!is_seeder && bootstrap.empty()) ||
        (!is_seeder && meta_path.empty()) || (!is_seeder && out_dir.empty()) ||
        (is_seeder && !file_path.empty() && meta_path.empty())) {
        std::cerr << "Usage:\n"
                  << "  Seeder:     p2pshare --seed --file <file> --meta "
                     "<meta> --bootstrap <ip> \n"
                  << "  Downloader: p2pshare --meta <meta> --out-dir <dir>  "
                     "--bootstrap <ip>\n";
        return 1;
    }

    auto current_id = generate_random_id();

    std::cerr << "LOCAL IP : " << get_local_ip() << "\n";
    std::cerr << "LOCAL KADEMLIA ID : " << byte_array_to_string(current_id)
              << "\n";

    KademliaNode kademlia_node(current_id);
    kademlia_node.run_listener_async();

    if (!bootstrap.empty()) {
        kademlia_node.bootstrap(bootstrap);
    }

    std::vector<Piece> pieces;
    MetaFile meta;
    if (is_seeder) {
        if (!file_path.empty()) {
            if (!std::filesystem::exists(file_path)) {
                std::cerr << "Seeder file does not exist: " << file_path
                          << "\n";
                return 1;
            }

            pieces = chunk_file(file_path, PIECE_SIZE);
            meta = MetaFile::generate(
                pieces, PIECE_SIZE,
                std::filesystem::path(file_path).filename().string());
            meta.save_to_file(meta_path);
        }
    } else {
        meta = MetaFile::load_from_file(meta_path);
        std::array<uint8_t, NODE_ID_BYTES> key = meta.hash();
        auto peers = kademlia_node.iterative_find_value(key);

        if (!peers) {
            std::cerr << "Failed to find peers\n";
            return 1;
        }

        try {
            std::filesystem::path full_out_path =
                std::filesystem::path(out_dir) / meta.file_name;
            pieces = download_file(meta, *peers, full_out_path);
        } catch (const std::exception &e) {
            std::cerr << "Download failed: " << e.what() << "\n";
            return 1;
        }
    }

    if (!(is_seeder && file_path.empty())) {
        kademlia_node.iterative_store(meta.hash(), get_local_ip());
    }

    start_peer_server(pieces, PORT);
    return 0;
}
