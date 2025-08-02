#pragma once
#include "piece.hpp"
#include <string>
#include <vector>

std::vector<Piece> chunk_file(const std::string &filename);
void print_piece_hashes(const std::vector<Piece> &pieces);
