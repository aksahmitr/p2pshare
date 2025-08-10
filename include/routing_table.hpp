#pragma once
#include <array>
#include <deque>
#include <mutex>
#include <stdint.h>
#include <string>
#include <vector>

constexpr size_t NODE_ID_BYTES = 20;

struct Contact {
    std::array<uint8_t, NODE_ID_BYTES> id;
    std::string addr;

    Contact() = default;

    Contact(const std::array<uint8_t, 20> &id, const std::string &addr)
        : id(id), addr(addr) {}

    bool operator==(const Contact &o) const {
        return id == o.id && addr == o.addr;
    }
};

struct RoutingTable {
    size_t k;
    std::vector<std::deque<Contact>> buckets;
    std::vector<std::mutex> bucket_mutexes;

    RoutingTable(size_t ksize)
        : k(ksize), buckets(NODE_ID_BYTES * 8),
          bucket_mutexes(NODE_ID_BYTES * 8) {}
};
