#include "attacks.hpp"
#include <vector>
#include <iostream>

namespace Oxta::Attacks {

namespace {

Bitboard pawn_table[COLOR_NB][SQUARE_NB];
Bitboard knight_table[SQUARE_NB];
Bitboard king_table[SQUARE_NB];

struct Magic {
    Bitboard mask;
    Bitboard* attacks;
};

Magic rook_magics[SQUARE_NB];
Magic bishop_magics[SQUARE_NB];

std::vector<Bitboard> rook_attack_data;
std::vector<Bitboard> bishop_attack_data;

// Helper to calculate sliding attacks on the fly (slow, for init)
Bitboard slider_attack_slow(Square sq, Bitboard occ, bool bishop) {
    Bitboard attacks = 0;
    int r, f;
    int tr = sq / 8;
    int tf = sq % 8;

    if (bishop) {
        for (r = tr + 1, f = tf + 1; r < 8 && f < 8; r++, f++) { attacks |= (1ULL << (r * 8 + f)); if (occ & (1ULL << (r * 8 + f))) break; }
        for (r = tr + 1, f = tf - 1; r < 8 && f >= 0; r++, f--) { attacks |= (1ULL << (r * 8 + f)); if (occ & (1ULL << (r * 8 + f))) break; }
        for (r = tr - 1, f = tf + 1; r >= 0 && f < 8; r--, f++) { attacks |= (1ULL << (r * 8 + f)); if (occ & (1ULL << (r * 8 + f))) break; }
        for (r = tr - 1, f = tf - 1; r >= 0 && f >= 0; r--, f--) { attacks |= (1ULL << (r * 8 + f)); if (occ & (1ULL << (r * 8 + f))) break; }
    } else {
        for (r = tr + 1; r < 8; r++) { attacks |= (1ULL << (r * 8 + tf)); if (occ & (1ULL << (r * 8 + tf))) break; }
        for (r = tr - 1; r >= 0; r--) { attacks |= (1ULL << (r * 8 + tf)); if (occ & (1ULL << (r * 8 + tf))) break; }
        for (f = tf + 1; f < 8; f++) { attacks |= (1ULL << (tr * 8 + f)); if (occ & (1ULL << (tr * 8 + f))) break; }
        for (f = tf - 1; f >= 0; f--) { attacks |= (1ULL << (tr * 8 + f)); if (occ & (1ULL << (tr * 8 + f))) break; }
    }
    return attacks;
}

Bitboard calc_mask(Square sq, bool bishop) {
    Bitboard attacks = 0;
    int r, f;
    int tr = sq / 8;
    int tf = sq % 8;

    if (bishop) {
        for (r = tr + 1, f = tf + 1; r < 7 && f < 7; r++, f++) attacks |= (1ULL << (r * 8 + f));
        for (r = tr + 1, f = tf - 1; r < 7 && f > 0; r++, f--) attacks |= (1ULL << (r * 8 + f));
        for (r = tr - 1, f = tf + 1; r > 0 && f < 7; r--, f++) attacks |= (1ULL << (r * 8 + f));
        for (r = tr - 1, f = tf - 1; r > 0 && f > 0; r--, f--) attacks |= (1ULL << (r * 8 + f));
    } else {
        for (r = tr + 1; r < 7; r++) attacks |= (1ULL << (r * 8 + tf));
        for (r = tr - 1; r > 0; r--) attacks |= (1ULL << (r * 8 + tf));
        for (f = tf + 1; f < 7; f++) attacks |= (1ULL << (tr * 8 + f));
        for (f = tf - 1; f > 0; f--) attacks |= (1ULL << (tr * 8 + f));
    }
    return attacks;
}

void init_sliders() {
    // Increased size to prevent overflow
    rook_attack_data.resize(0x100000);
    bishop_attack_data.resize(0x100000);

    int r_offset = 0;
    int b_offset = 0;

    for (int s = 0; s < SQUARE_NB; ++s) {
        // Rooks
        rook_magics[s].mask = calc_mask(static_cast<Square>(s), false);
        rook_magics[s].attacks = &rook_attack_data[r_offset];

        int r_bits = Bitboards::popcount(rook_magics[s].mask);
        int r_indices = 1 << r_bits;

        if (static_cast<size_t>(r_offset + r_indices) > rook_attack_data.size()) {
            std::cerr << "FATAL: Rook attack data overflow!" << std::endl;
            exit(1);
        }

        for (int i = 0; i < r_indices; ++i) {
             Bitboard mapped_occ = 0;
             Bitboard temp_mask = rook_magics[s].mask;
             Bitboard temp_val = i;
             while (temp_mask) {
                 Bitboard lsb = temp_mask & -temp_mask;
                 temp_mask &= ~lsb;
                 if (temp_val & 1) mapped_occ |= lsb;
                 temp_val >>= 1;
             }
             rook_magics[s].attacks[i] = slider_attack_slow(static_cast<Square>(s), mapped_occ, false);
        }
        r_offset += r_indices;

        // Bishops
        bishop_magics[s].mask = calc_mask(static_cast<Square>(s), true);
        bishop_magics[s].attacks = &bishop_attack_data[b_offset];

        int b_bits = Bitboards::popcount(bishop_magics[s].mask);
        int b_indices = 1 << b_bits;

        if (static_cast<size_t>(b_offset + b_indices) > bishop_attack_data.size()) {
            std::cerr << "FATAL: Bishop attack data overflow!" << std::endl;
            exit(1);
        }

        for (int i = 0; i < b_indices; ++i) {
             Bitboard temp_mask = bishop_magics[s].mask;
             Bitboard temp_val = i;
             Bitboard mapped_occ = 0;
             while (temp_mask) {
                 Bitboard lsb = temp_mask & -temp_mask;
                 temp_mask &= ~lsb;
                 if (temp_val & 1) mapped_occ |= lsb;
                 temp_val >>= 1;
             }
             bishop_magics[s].attacks[i] = slider_attack_slow(static_cast<Square>(s), mapped_occ, true);
        }
        b_offset += b_indices;
    }
}

} // namespace

void init() {
    // Pawns
    for (int s = 0; s < SQUARE_NB; ++s) {
        Bitboard b = (1ULL << s);
        pawn_table[WHITE][s] = ((b << 9) & ~0x0101010101010101ULL) | ((b << 7) & ~0x8080808080808080ULL);
        pawn_table[BLACK][s] = ((b >> 9) & ~0x8080808080808080ULL) | ((b >> 7) & ~0x0101010101010101ULL);
    }

    // Knights
    for (int s = 0; s < SQUARE_NB; ++s) {
        Bitboard att = 0;
        int r = s / 8;
        int f = s % 8;
        int jumps[8][2] = {{1,2}, {2,1}, {2,-1}, {1,-2}, {-1,-2}, {-2,-1}, {-2,1}, {-1,2}};
        for (auto& j : jumps) {
            int nr = r + j[0];
            int nf = f + j[1];
            if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
                att |= (1ULL << (nr * 8 + nf));
            }
        }
        knight_table[s] = att;
    }

    // Kings
    for (int s = 0; s < SQUARE_NB; ++s) {
         int r = s / 8;
         int f = s % 8;
         Bitboard att = 0;
         for (int dr = -1; dr <= 1; ++dr) {
             for (int df = -1; df <= 1; ++df) {
                 if (dr == 0 && df == 0) continue;
                 int nr = r + dr;
                 int nf = f + df;
                 if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
                     att |= (1ULL << (nr * 8 + nf));
                 }
             }
         }
         king_table[s] = att;
    }

    init_sliders();
}

Bitboard pawn_attacks(Square s, Color c) {
    return pawn_table[c][s];
}

Bitboard knight_attacks(Square s) {
    return knight_table[s];
}

Bitboard king_attacks(Square s) {
    return king_table[s];
}

Bitboard bishop_attacks(Square s, Bitboard occupancy) {
    // Safety check? No, too slow.
    return bishop_magics[s].attacks[Bitboards::pext(occupancy, bishop_magics[s].mask)];
}

Bitboard rook_attacks(Square s, Bitboard occupancy) {
    return rook_magics[s].attacks[Bitboards::pext(occupancy, rook_magics[s].mask)];
}

Bitboard queen_attacks(Square s, Bitboard occupancy) {
    return bishop_attacks(s, occupancy) | rook_attacks(s, occupancy);
}

} // namespace Oxta::Attacks
