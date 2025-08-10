#include "kademlia_node.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <deque>
#include <iostream>
#include <queue>
#include <random>
#include <set>
#include <thread>
#include <unistd.h>

namespace {

uint64_t random_txn_id() {
    static std::mt19937_64 rng(std::random_device{}());
    return rng();
}

std::array<uint8_t, NODE_ID_BYTES>
get_xor(const std::array<uint8_t, NODE_ID_BYTES> &a,
        const std::array<uint8_t, NODE_ID_BYTES> &b) {
    std::array<uint8_t, NODE_ID_BYTES> out{};
    for (size_t i = 0; i < NODE_ID_BYTES; ++i) {
        out[i] = a[i] ^ b[i];
    }
    return out;
}

uint8_t hex_char_to_int(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    throw std::invalid_argument("Invalid hex character");
}

void hex_to_bytes(const std::string &hex, uint8_t *out, size_t out_len) {
    if (hex.size() != out_len * 2) {
        throw std::invalid_argument("Hex string wrong length");
    }
    for (size_t i = 0; i < out_len; ++i) {
        out[i] = (hex_char_to_int(hex[2 * i]) << 4) |
                 hex_char_to_int(hex[2 * i + 1]);
    }
}

std::string sockaddr_ip_to_string(const sockaddr_in &addr) {
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr.sin_addr), ip_str, sizeof(ip_str));
    return std::string(ip_str);
}

std::string ip_to_string(const in_addr &addr) {
    char buffer[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &addr, buffer, INET_ADDRSTRLEN) == nullptr) {
        return {};
    }
    return std::string(buffer);
}
} // namespace

void KademliaNode::handle_ping(const RpcHeader &hdr,
                               const sockaddr_in &src_addr, socklen_t src_len,
                               int sock) const {
    std::cerr << "PING REQ from " << ip_to_string(src_addr.sin_addr) << "\n";
    RpcHeader reply_hdr;
    reply_hdr.msg_type = 1;
    reply_hdr.txn_id = hdr.txn_id;

    sendto(sock, reinterpret_cast<char *>(&reply_hdr), sizeof(reply_hdr), 0,
           (sockaddr *)&src_addr, src_len);
}

void KademliaNode::handle_find_node(const RpcHeader &hdr, const char *buffer,
                                    ssize_t recv_len,
                                    const sockaddr_in &src_addr,
                                    socklen_t src_len, int sock) const {
    if (recv_len < static_cast<ssize_t>(sizeof(RpcHeader) + NODE_ID_BYTES)) {
        return;
    }

    const uint8_t *target_ptr =
        reinterpret_cast<const uint8_t *>(buffer + sizeof(RpcHeader));

    std::array<uint8_t, NODE_ID_BYTES> target_id;
    std::copy(target_ptr, target_ptr + NODE_ID_BYTES, target_id.begin());

    std::cerr << "FIND_NODE REQ " << byte_array_to_string(target_id) << " from "
              << ip_to_string(src_addr.sin_addr) << "\n";

    auto closest = find_node(target_id);

    std::ostringstream ss;
    for (const auto &contact : closest) {
        for (auto b : contact.id) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
        }
        ss << ' ' << contact.addr << '\n';
    }
    std::string out = ss.str();

    RpcHeader reply_hdr;
    reply_hdr.msg_type = 3;
    reply_hdr.txn_id = hdr.txn_id;

    std::string payload;
    payload.reserve(sizeof(reply_hdr) + out.size());
    payload.append(reinterpret_cast<char *>(&reply_hdr), sizeof(reply_hdr));
    payload.append(reinterpret_cast<const char *>(out.data()), out.size());

    sendto(sock, payload.data(), payload.size(), 0, (sockaddr *)&src_addr,
           src_len);
}

void KademliaNode::handle_find_value(const RpcHeader &hdr, const char *buffer,
                                     ssize_t recv_len,
                                     const sockaddr_in &src_addr,
                                     socklen_t src_len, int sock) const {
    if (recv_len < static_cast<ssize_t>(sizeof(RpcHeader) + NODE_ID_BYTES)) {
        return;
    }

    const uint8_t *target_ptr =
        reinterpret_cast<const uint8_t *>(buffer + sizeof(RpcHeader));

    std::array<uint8_t, NODE_ID_BYTES> target_key;
    std::copy(target_ptr, target_ptr + NODE_ID_BYTES, target_key.begin());

    auto val_opt = find_value(target_key);

    if (val_opt) {
        std::cerr << "FIND_VALUE REQ " << byte_array_to_string(target_key)
                  << " from " << ip_to_string(src_addr.sin_addr)
                  << " (FOUND)\n";

        RpcHeader reply_hdr;
        reply_hdr.msg_type = 5;
        reply_hdr.txn_id = hdr.txn_id;

        std::ostringstream ss;
        for (const auto &ip : *val_opt) {
            ss << ip << '\n';
        }
        std::string val_str = ss.str();

        std::string payload = std::string(reinterpret_cast<char *>(&reply_hdr),
                                          sizeof(reply_hdr)) +
                              std::string(1, static_cast<char>(1)) + val_str;

        sendto(sock, payload.data(), payload.size(), 0, (sockaddr *)&src_addr,
               src_len);
    } else {
        std::cerr << "FIND_VALUE REQ " << byte_array_to_string(target_key)
                  << " from " << ip_to_string(src_addr.sin_addr)
                  << " (NOT FOUND)\n";

        auto closest = find_node(target_key);

        std::ostringstream ss;
        for (const auto &contact : closest) {
            for (auto b : contact.id) {
                ss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
            }
            ss << ' ' << contact.addr << '\n';
        }
        std::string out = ss.str();

        RpcHeader reply_hdr;
        reply_hdr.msg_type = 5;
        reply_hdr.txn_id = hdr.txn_id;

        std::string payload = std::string(reinterpret_cast<char *>(&reply_hdr),
                                          sizeof(reply_hdr)) +
                              std::string(1, static_cast<char>(0)) + out;

        sendto(sock, payload.data(), payload.size(), 0, (sockaddr *)&src_addr,
               src_len);
    }
}

void KademliaNode::handle_store(const RpcHeader &hdr, const char *buffer,
                                ssize_t recv_len, const sockaddr_in &src_addr,
                                socklen_t src_len, int sock) {
    if (recv_len < static_cast<ssize_t>(sizeof(RpcHeader) + NODE_ID_BYTES)) {
        return;
    }

    const uint8_t *key_ptr =
        reinterpret_cast<const uint8_t *>(buffer + sizeof(RpcHeader));

    std::array<uint8_t, NODE_ID_BYTES> key;
    std::copy(key_ptr, key_ptr + NODE_ID_BYTES, key.begin());

    size_t ip_len = recv_len - (sizeof(RpcHeader) + NODE_ID_BYTES);
    std::string ip_str(buffer + sizeof(RpcHeader) + NODE_ID_BYTES, ip_len);

    std::cerr << "STORE REQ KEY: " << byte_array_to_string(key)
              << " VALUE: " << ip_str << " from "
              << ip_to_string(src_addr.sin_addr) << "\n";

    store(key, ip_str);
}

KademliaNode::KademliaNode(const std::array<uint8_t, NODE_ID_BYTES> &id)
    : self_id(id), routing_table(KADEMLIA_K) {}

void KademliaNode::store(const std::array<uint8_t, 20UL> &key,
                         const std::string &value) {
    local_store[key].push_back(value);
    std::cerr << "STORE LOCAL KEY: " << byte_array_to_string(key)
              << " VALUE: " << value << "\n";
}

size_t KademliaNode::get_bucket_index(
    const std::array<uint8_t, NODE_ID_BYTES> &id) const {
    auto dist = get_xor(self_id, id);
    for (size_t i = 0; i < NODE_ID_BYTES; ++i) {
        uint8_t byte = dist[i];
        if (byte != 0) {
            int remaining_bits = static_cast<int>((NODE_ID_BYTES - i - 1) * 8);
            for (int bit = 7; bit >= 0; --bit) {
                if (byte & (1u << bit)) {
                    return remaining_bits + bit;
                }
            }
        }
    }
    return 0;
}

void KademliaNode::add_contact(const Contact &c) {
    if (c.id == self_id) {
        return;
    }

    std::cerr << "ADD CONTACT LOCAL " << byte_array_to_string(c.id) << " "
              << c.addr << "\n";
    size_t idx = get_bucket_index(c.id);

    std::lock_guard<std::mutex> lock(routing_table.bucket_mutexes[idx]);

    auto &bucket = routing_table.buckets[idx];

    auto it = std::find_if(bucket.begin(), bucket.end(),
                           [&](const Contact &x) { return x.id == c.id; });
    if (it != bucket.end()) {
        Contact tmp = std::move(*it);
        bucket.erase(it);
        bucket.push_back(std::move(tmp));
    } else {
        if (bucket.size() < routing_table.k) {
            bucket.push_back(c);
        } else {
            auto oldest = std::move(bucket.front());
            bucket.pop_front();
            if (ping(oldest.addr)) {
                bucket.push_back(std::move(oldest));
            } else {
                bucket.push_back(c);
            }
        }
    }
}

void KademliaNode::add_contact_async(const Contact &c) {
    std::thread(&KademliaNode::add_contact, this, c).detach();
}

std::vector<Contact> KademliaNode::find_node(
    const std::array<uint8_t, NODE_ID_BYTES> &target) const {

    std::vector<Contact> allContacts;
    for (const auto &bucket : routing_table.buckets) {
        allContacts.insert(allContacts.end(), bucket.begin(), bucket.end());
    }

    std::sort(allContacts.begin(), allContacts.end(),
              [target](const Contact &a, const Contact &b) {
                  return get_xor(a.id, target) < get_xor(b.id, target);
              });

    if (allContacts.size() > KADEMLIA_K) {
        allContacts.resize(KADEMLIA_K);
    }

    return allContacts;
}

void KademliaNode::run_listener_async() {
    std::thread(&KademliaNode::run_listener, this).detach();
}

void KademliaNode::run_listener() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(KADEMLIA_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        return;
    }

    char buffer[1024];

    while (true) {
        sockaddr_in src_addr{};
        socklen_t src_len = sizeof(src_addr);

        ssize_t recv_len = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                                    (sockaddr *)&src_addr, &src_len);

        if (recv_len < 0) {
            perror("recvfrom");
            continue;
        }

        if (recv_len < static_cast<ssize_t>(sizeof(RpcHeader))) {
            continue;
        }

        RpcHeader hdr;
        memcpy(&hdr, buffer, sizeof(RpcHeader));

        add_contact_async({hdr.node_id, sockaddr_ip_to_string(src_addr)});

        switch (hdr.msg_type) {
        case 0: { // PING
            handle_ping(hdr, src_addr, src_len, sock);
            break;
        }
        case 2: { // FIND_NODE
            handle_find_node(hdr, buffer, recv_len, src_addr, src_len, sock);
            break;
        }
        case 4: { // FIND_VALUE
            handle_find_value(hdr, buffer, recv_len, src_addr, src_len, sock);
            break;
        }
        case 6: { // STORE
            handle_store(hdr, buffer, recv_len, src_addr, src_len, sock);
            break;
        }
        }
    }

    close(sock);
}

std::optional<std::vector<std::string>>
KademliaNode::find_value(const std::array<uint8_t, NODE_ID_BYTES> &key) const {
    auto it = local_store.find(key);
    if (it != local_store.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<std::variant<std::vector<std::string>, std::vector<Contact>>>
KademliaNode::request_find_value(
    const Contact &c, const std::array<uint8_t, NODE_ID_BYTES> &key) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return std::nullopt;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(KADEMLIA_PORT);
    if (inet_pton(AF_INET, c.addr.c_str(), &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return std::nullopt;
    }

    RpcHeader hdr;
    hdr.msg_type = 4;
    hdr.txn_id = random_txn_id();
    hdr.node_id = self_id;

    std::string payload;
    payload.reserve(sizeof(hdr) + NODE_ID_BYTES);
    payload.append(reinterpret_cast<char *>(&hdr), sizeof(hdr));
    payload.append(reinterpret_cast<const char *>(key.data()), NODE_ID_BYTES);

    if (sendto(sock, payload.data(), payload.size(), 0,
               reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        perror("sendto");
        close(sock);
        return std::nullopt;
    }

    timeval tv{};
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char buffer[1024];
    sockaddr_in src_addr{};
    socklen_t src_len = sizeof(src_addr);
    ssize_t len = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                           reinterpret_cast<sockaddr *>(&src_addr), &src_len);

    close(sock);

    if (len >= static_cast<ssize_t>(sizeof(RpcHeader))) {
        RpcHeader reply_hdr;
        memcpy(&reply_hdr, buffer, sizeof(RpcHeader));

        add_contact_async({reply_hdr.node_id, sockaddr_ip_to_string(src_addr)});

        if (reply_hdr.msg_type == 5 && reply_hdr.txn_id == hdr.txn_id) {
            uint8_t type = static_cast<uint8_t>(buffer[sizeof(reply_hdr)]);

            std::string msg(buffer + sizeof(reply_hdr) + 1,
                            len - sizeof(reply_hdr) - 1);
            std::istringstream ss(msg);
            std::string line;

            if (type == 0) {
                std::vector<Contact> result;

                while (std::getline(ss, line)) {
                    std::istringstream line_ss(line);
                    std::string nodeid_hex, ip;

                    line_ss >> nodeid_hex >> ip;

                    std::array<uint8_t, NODE_ID_BYTES> node_id;
                    hex_to_bytes(nodeid_hex, node_id.data(), NODE_ID_BYTES);
                    result.emplace_back(node_id, ip);
                }
                return result;
            } else if (type == 1) {
                std::vector<std::string> result;

                while (std::getline(ss, line)) {
                    std::istringstream line_ss(line);
                    std::string addr;

                    line_ss >> addr;
                    result.push_back(addr);
                }
                return result;
            }
        }
    }

    return std::nullopt;
}

std::optional<Contact> KademliaNode::ping(const std::string &ip) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return std::nullopt;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(KADEMLIA_PORT);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return std::nullopt;
    }

    RpcHeader hdr;
    hdr.msg_type = 0;
    hdr.txn_id = random_txn_id();
    hdr.node_id = self_id;

    if (sendto(sock, reinterpret_cast<char *>(&hdr), sizeof(hdr), 0,
               reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        perror("sendto");
        close(sock);
        return std::nullopt;
    }

    timeval tv{};
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char buffer[1024];
    sockaddr_in src_addr{};
    socklen_t src_len = sizeof(src_addr);
    ssize_t len = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                           reinterpret_cast<sockaddr *>(&src_addr), &src_len);

    close(sock);

    if (len >= static_cast<ssize_t>(sizeof(RpcHeader))) {
        RpcHeader reply_hdr;
        memcpy(&reply_hdr, buffer, sizeof(RpcHeader));

        add_contact_async({reply_hdr.node_id, sockaddr_ip_to_string(src_addr)});

        if (reply_hdr.msg_type == 1 && reply_hdr.txn_id == hdr.txn_id) {
            return Contact(reply_hdr.node_id, ip);
        }
    }

    return std::nullopt;
}

void KademliaNode::request_store(const Contact &c,
                                 const std::array<uint8_t, NODE_ID_BYTES> &key,
                                 const std::string &value) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(KADEMLIA_PORT);
    if (inet_pton(AF_INET, c.addr.c_str(), &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return;
    }

    RpcHeader hdr;
    hdr.msg_type = 6;
    hdr.txn_id = random_txn_id();
    hdr.node_id = self_id;

    std::string payload;
    payload.reserve(sizeof(hdr) + NODE_ID_BYTES + value.size());
    payload.append(reinterpret_cast<char *>(&hdr), sizeof(hdr));
    payload.append(reinterpret_cast<const char *>(key.data()), NODE_ID_BYTES);
    payload.append(value.data(), value.size());

    if (sendto(sock, payload.data(), payload.size(), 0,
               reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        perror("sendto");
        close(sock);
        return;
    }

    close(sock);
}

std::optional<std::vector<Contact>> KademliaNode::request_find_node(
    const Contact &c, const std::array<uint8_t, NODE_ID_BYTES> &target) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return std::nullopt;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(KADEMLIA_PORT);
    if (inet_pton(AF_INET, c.addr.c_str(), &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return std::nullopt;
    }

    RpcHeader hdr;
    hdr.msg_type = 2;
    hdr.txn_id = random_txn_id();
    hdr.node_id = self_id;

    std::string payload;
    payload.reserve(sizeof(hdr) + NODE_ID_BYTES);
    payload.append(reinterpret_cast<char *>(&hdr), sizeof(hdr));
    payload.append(reinterpret_cast<const char *>(target.data()),
                   NODE_ID_BYTES);

    if (sendto(sock, payload.data(), payload.size(), 0,
               reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        perror("sendto");
        close(sock);
        return std::nullopt;
    }

    timeval tv{};
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char buffer[1024];
    sockaddr_in src_addr{};
    socklen_t src_len = sizeof(src_addr);
    ssize_t len = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                           reinterpret_cast<sockaddr *>(&src_addr), &src_len);

    close(sock);

    if (len >= static_cast<ssize_t>(sizeof(RpcHeader))) {
        RpcHeader reply_hdr;
        memcpy(&reply_hdr, buffer, sizeof(RpcHeader));

        add_contact_async({reply_hdr.node_id, sockaddr_ip_to_string(src_addr)});

        if (reply_hdr.msg_type == 3 && reply_hdr.txn_id == hdr.txn_id) {
            std::string msg(buffer + sizeof(reply_hdr),
                            len - sizeof(reply_hdr));
            std::istringstream ss(msg);
            std::string line;

            std::vector<Contact> result;

            while (std::getline(ss, line)) {
                std::istringstream line_ss(line);
                std::string nodeid_hex, ip;

                line_ss >> nodeid_hex >> ip;

                std::array<uint8_t, NODE_ID_BYTES> node_id;
                hex_to_bytes(nodeid_hex, node_id.data(), NODE_ID_BYTES);
                result.emplace_back(node_id, ip);
            }
            return result;
        }
    }

    return std::nullopt;
}

std::vector<Contact> KademliaNode::iterative_find_node(
    const std::array<uint8_t, NODE_ID_BYTES> &target) {
    std::set<std::array<uint8_t, NODE_ID_BYTES>> queried;

    std::vector<Contact> current = find_node(target);

    auto cmp = [target](const Contact &a, const Contact &b) {
        return get_xor(a.id, target) < get_xor(b.id, target);
    };

    std::set<Contact, decltype(cmp)> result(current.begin(), current.end(),
                                            cmp);
    bool valid = true;

    while (valid) {
        std::vector<Contact> candidates;

        int cnt = 0;
        for (const auto &i : result) {
            if (cnt >= ALPHA) {
                break;
            }
            if (queried.count(i.id)) {
                continue;
            }
            auto current = request_find_node(i, target);
            if (current) {
                candidates.insert(candidates.end(), current->begin(),
                                  current->end());
                queried.insert(i.id);
                ++cnt;
            }
        }

        valid = false;
        for (const auto &i : candidates) {
            if (result.size() < KADEMLIA_K) {
                valid = true;
                result.insert(i);
            } else if (cmp(i, *result.rbegin())) {
                valid = true;
                result.erase(std::prev(result.end()));
                result.insert(i);
            }
        }
    }

    return std::vector(result.begin(), result.end());
}

std::optional<std::vector<std::string>> KademliaNode::iterative_find_value(
    const std::array<uint8_t, NODE_ID_BYTES> &key) {
    std::set<std::array<uint8_t, NODE_ID_BYTES>> queried;

    auto self = find_value(key);

    if (self) {
        return self;
    }

    auto current = find_node(key);

    auto cmp = [key](const Contact &a, const Contact &b) {
        return get_xor(a.id, key) < get_xor(b.id, key);
    };

    std::set<Contact, decltype(cmp)> result(current.begin(), current.end(),
                                            cmp);
    bool valid = true;

    while (valid) {
        std::vector<Contact> candidates;

        int cnt = 0;
        for (const auto &i : result) {
            if (cnt >= ALPHA) {
                break;
            }
            if (queried.count(i.id)) {
                continue;
            }
            auto current = request_find_value(i, key);
            if (current) {
                if (std::holds_alternative<std::vector<std::string>>(
                        *current)) {
                    return std::get<std::vector<std::string>>(*current);
                } else {
                    auto &contacts = std::get<std::vector<Contact>>(*current);
                    candidates.insert(candidates.end(), contacts.begin(),
                                      contacts.end());
                    queried.insert(i.id);
                    ++cnt;
                }
            }
        }

        valid = false;
        for (const auto &i : candidates) {
            if (result.size() < KADEMLIA_K) {
                valid = true;
                result.insert(i);
            } else if (cmp(i, *result.rbegin())) {
                valid = true;
                result.erase(std::prev(result.end()));
                result.insert(i);
            }
        }
    }

    return std::nullopt;
}

void KademliaNode::bootstrap(const std::string &ip) {
    auto c = ping(ip);
    if (!c) {
        std::runtime_error("Bootstrap failed");
        return;
    }
    add_contact(*c);
    auto closest = iterative_find_node(self_id);
    for (const auto &i : closest) {
        add_contact(i);
    }
};

void KademliaNode::iterative_store(const std::array<uint8_t, 20UL> &key,
                                   const std::string &value) {
    std::set<std::array<uint8_t, NODE_ID_BYTES>> queried;

    store(key, value);

    std::vector<Contact> current = find_node(key);

    auto cmp = [key](const Contact &a, const Contact &b) {
        return get_xor(a.id, key) < get_xor(b.id, key);
    };

    std::set<Contact, decltype(cmp)> result(current.begin(), current.end(),
                                            cmp);
    bool valid = true;

    while (valid) {
        std::vector<Contact> candidates;

        int cnt = 0;
        for (const auto &i : result) {
            if (cnt >= ALPHA) {
                break;
            }
            if (queried.count(i.id)) {
                continue;
            }
            request_store(i, key, value);
            auto current = request_find_node(i, key);
            if (current) {
                candidates.insert(candidates.end(), current->begin(),
                                  current->end());
                queried.insert(i.id);
                ++cnt;
            }
        }

        valid = false;
        for (const auto &i : candidates) {
            if (result.size() < KADEMLIA_K) {
                valid = true;
                result.insert(i);
            } else if (cmp(i, *result.rbegin())) {
                valid = true;
                result.erase(std::prev(result.end()));
                result.insert(i);
            }
        }
    }

    return;
}