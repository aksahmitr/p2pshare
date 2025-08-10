#pragma once
#include "routing_table.hpp"
#include <cstdint>
#include <iomanip>
#include <map>
#include <netinet/in.h>
#include <optional>
#include <sstream>
#include <sys/socket.h>
#include <variant>
#include <vector>
constexpr size_t KADEMLIA_K = 20;
constexpr uint16_t KADEMLIA_PORT = 9001;
constexpr int ALPHA = 3;

struct RpcHeader {
    uint64_t txn_id;
    uint8_t msg_type;
    std::array<uint8_t, NODE_ID_BYTES> node_id;
};

template <size_t N>
std::string byte_array_to_string(const std::array<uint8_t, N> &arr) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    for (size_t i = 0; i < N; ++i) {
        oss << std::setw(2) << static_cast<int>(arr[i]);
    }

    return oss.str();
}

class KademliaNode {
  public:
    KademliaNode(const std::array<uint8_t, NODE_ID_BYTES> &id);

    void bootstrap(const std::string &ip);

    std::vector<Contact>
    iterative_find_node(const std::array<uint8_t, NODE_ID_BYTES> &target);

    std::optional<std::vector<std::string>>
    iterative_find_value(const std::array<uint8_t, NODE_ID_BYTES> &key);

    void iterative_store(const std::array<uint8_t, NODE_ID_BYTES> &key,
                         const std::string &value);

    void run_listener();

    void run_listener_async();

  private:
    std::array<uint8_t, NODE_ID_BYTES> self_id;
    RoutingTable routing_table;
    std::map<std::array<uint8_t, NODE_ID_BYTES>, std::vector<std::string>>
        local_store;

    size_t get_bucket_index(const std::array<uint8_t, NODE_ID_BYTES> &id) const;
    void add_contact(const Contact &c);
    void add_contact_async(const Contact &c);

    // PING
    std::optional<Contact> ping(const std::string &ip);

    void handle_ping(const RpcHeader &hdr, const sockaddr_in &src_addr,
                     socklen_t src_len, int sock) const;

    // FIND NODE
    std::optional<std::vector<Contact>>
    request_find_node(const Contact &c,
                      const std::array<uint8_t, NODE_ID_BYTES> &target);

    std::vector<Contact>
    find_node(const std::array<uint8_t, NODE_ID_BYTES> &target) const;

    void handle_find_node(const RpcHeader &hdr, const char *buffer,
                          ssize_t recv_len, const sockaddr_in &src_addr,
                          socklen_t src_len, int sock) const;

    // FIND VALUE
    std::optional<std::vector<std::string>>
    find_value(const std::array<uint8_t, NODE_ID_BYTES> &key) const;

    std::optional<std::variant<std::vector<std::string>, std::vector<Contact>>>
    request_find_value(const Contact &c,
                       const std::array<uint8_t, NODE_ID_BYTES> &key);

    void handle_find_value(const RpcHeader &hdr, const char *buffer,
                           ssize_t recv_len, const sockaddr_in &src_addr,
                           socklen_t src_len, int sock) const;

    // STORE

    void store(const std::array<uint8_t, NODE_ID_BYTES> &key,
               const std::string &value);

    void request_store(const Contact &c,
                       const std::array<uint8_t, NODE_ID_BYTES> &key,
                       const std::string &value);

    void handle_store(const RpcHeader &hdr, const char *buffer,
                      ssize_t recv_len, const sockaddr_in &src_addr,
                      socklen_t src_len, int sock);
};