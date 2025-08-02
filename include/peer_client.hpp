#pragma once
#include "piece.hpp"
#include <string>

Piece request_piece(const std::string &ip, int port, int piece_index,
                    const std::string &expected_hash);
