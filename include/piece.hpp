#pragma once
#include <string>
#include <vector>

struct Piece {
    int index;
    std::vector<char> data;
    std::string hash;
};