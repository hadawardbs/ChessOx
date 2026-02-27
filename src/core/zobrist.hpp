#ifndef OXTA_CORE_ZOBRIST_HPP
#define OXTA_CORE_ZOBRIST_HPP

#include "types.hpp"
#include "../util/rand.hpp"

namespace Oxta::Zobrist {

// Zobrist Keys
extern Bitboard psq[PIECE_NB][SQUARE_NB];
extern Bitboard en_passant[FILE_NB + 1]; // +1 for no en-passant
extern Bitboard castling[16]; // 4 bits for castling rights (WK, WQ, BK, BQ)
extern Bitboard side_to_move;

// Initialize Zobrist keys
inline void init() {
    Util::PRNG rng(1070372); // Fixed seed for reproducibility

    for (int p = 0; p < PIECE_NB; ++p) {
        for (int s = 0; s < SQUARE_NB; ++s) {
            psq[p][s] = rng.next();
        }
    }

    for (int f = 0; f <= FILE_NB; ++f) {
        en_passant[f] = rng.next();
    }

    for (int c = 0; c < 16; ++c) {
        castling[c] = rng.next();
    }

    side_to_move = rng.next();
}

} // namespace Oxta::Zobrist

#endif // OXTA_CORE_ZOBRIST_HPP
