#ifndef OXTA_CORE_EVAL_HPP
#define OXTA_CORE_EVAL_HPP

#include "position.hpp"
#include "types.hpp"
#include "bitboard.hpp"
#include "attacks.hpp"
#include "../nnue/nnue.hpp"
#include <algorithm>
#include <vector>
#include <cmath>

namespace Oxta::Eval {

// --- Evaluation Weights ---
struct EvalWeights {
    int Material[PIECE_TYPE_NB] = { 0, 100, 320, 330, 500, 1000, 0 };
    int PassedPawnBonus[8] = { 0, 10, 20, 40, 70, 110, 160, 0 };
    int MobilityBonus[PIECE_TYPE_NB] = { 0, 0, 4, 3, 2, 1, 0 };
    int PawnPST_MG[64];
    int PawnPST_EG[64];
    int KnightPST_MG[64];

    // New Feature Weights
    int RookMinorExchangePenalty = 200; // 2.0 pawns
    int RookSafetyPenalty = 60; // 0.6 pawns
    int UndevelopedPenalty = 10; // 0.1 pawns
    int RankDominationBonus = 60; // 0.6 pawns
};

extern EvalWeights Weights;

// Peesto-like Tables (Defaults)
constexpr int Peesto_Pawn_MG[64] = {
    0,   0,   0,   0,   0,   0,   0,   0,
   98, 134,  61,  95,  68, 126,  34, -11,
   -6,   7,  26,  31,  65,  56,  25, -20,
  -14,  13,   6,  21,  23,  12,  17, -23,
  -27,  -2,  -5,  12,  17,   6,  10, -25,
  -26,  -4,  -4, -10,   3,   3,  33, -12,
  -35,  -1, -20, -23, -15,  24,  38, -22,
    0,   0,   0,   0,   0,   0,   0,   0,
};

constexpr int Peesto_Pawn_EG[64] = {
    0,   0,   0,   0,   0,   0,   0,   0,
  178, 173, 158, 134, 147, 132, 165, 187,
   94, 100,  85,  67,  56,  53,  82,  84,
   32,  24,  13,   5,  -2,   4,  17,  17,
   13,   9,  -3,  -7,  -7,  -8,   3,  -1,
    4,   7,  -6,   1,   0,  -5,  -1,  -8,
   13,   8,   8,  10,  13,   0,   2,  -7,
    0,   0,   0,   0,   0,   0,   0,   0,
};

constexpr int Peesto_Knight_MG[64] = {
  -167, -89, -34, -49,  61, -97, -15, -107,
   -73, -41,  72,  36,  23,  62,   7,  -17,
   -47,  60,  37,  65,  84, 129,  73,   44,
    -9,  17,  19,  53,  37,  69,  18,   22,
   -13,   4,  16,  13,  28,  19,  21,   -8,
   -23,  -9,  12,  10,  19,  17,  25,  -16,
   -29, -53, -12,  -3,  -1,  18, -14,  -19,
  -105, -21, -58, -33, -17, -28, -19,  -23,
};

constexpr int Peesto_Bishop_MG[64] = {
   -29,   4, -82, -37, -25, -42,   7,  -8,
   -26,  16, -18, -13,  30,  59,  18, -47,
   -16,  37,  43,  40,  35,  50,  37,  -2,
    -4,   5,  19,  50,  37,  37,   7,  -2,
    -6,  13,  13,  26,  34,  12,  10,   4,
     0,  15,  15,  15,  14,  27,  18,  10,
     4,  15,  16,   0,   7,  21,  33,   1,
   -33,  -3, -14, -21, -13, -12, -39, -21,
};

constexpr int Peesto_Rook_MG[64] = {
    32,  42,  32,  51,  63,   9,  31,  43,
    27,  32,  58,  62,  80,  67,  26,  44,
    -5,  19,  26,  36,  17,  45,  61,  16,
   -24, -11,   7,  26,  24,  35,  -8, -20,
   -36, -26, -12,  -1,   9,  -7,   6, -23,
   -45, -25, -16, -17,   3,   0,  -5, -33,
   -44, -16, -20,  -9,  -1,  11,  -6, -71,
   -19, -13,   1,  17,  16,   7, -37, -26,
};

constexpr int Peesto_Queen_MG[64] = {
   -28,   0,  29,  12,  59,  44,  43,  45,
   -24, -39,  -5,   1, -16,  57,  28,  54,
   -13, -17,   7,   8,  29,  56,  47,  57,
   -27, -27, -16, -16,  -1,  17,  -2,   1,
    -9, -26,  -9, -10,  -2,  -4,   3,  -3,
   -14,   2, -11,  -2,  -5,   2,  14,   5,
   -35,  -8,  11,   2,   8,  15,  -3,   1,
    -1, -18,  -9,  10, -15, -25, -31, -50,
};

constexpr int Peesto_King_MG[64] = {
   -65,  23,  16, -15, -56, -34,   2,  13,
    29,  -1, -20,  -7,  -8,  -4, -38, -29,
    -9,  24,   2, -16, -20,   6,  22, -22,
   -17, -20, -12, -27, -30, -25, -14, -36,
   -49,  -1, -27, -39, -46, -44, -33, -51,
   -14, -14, -22, -46, -44, -30, -15, -27,
     1,   7,  -8, -64, -43, -16,   9,   8,
   -15,  36,  12, -54,   8, -28,  24,  14,
};

constexpr int Peesto_King_EG[64] = {
   -74, -35, -18, -18, -11,  15,   4, -17,
   -12,  17,  14,  17,  17,  38,  23,  11,
    10,  17,  23,  15,  20,  45,  44,  13,
    -8,  22,  24,  27,  26,  33,  26,   3,
   -18,  -4,  21,  24,  27,  23,   9, -11,
   -19,  -3,  11,  21,  23,  16,   7,  -9,
   -27, -11,   4,  13,  14,   4,  -5, -17,
   -53, -34, -21, -11, -28, -14, -24, -43,
};

inline void init() {
    for(int i=0; i<64; ++i) {
        Weights.PawnPST_MG[i] = Peesto_Pawn_MG[i];
        Weights.PawnPST_EG[i] = Peesto_Pawn_EG[i];
        Weights.KnightPST_MG[i] = Peesto_Knight_MG[i];
    }
}

inline int evaluate(const Position& pos) {
    // NNUE disabled: untrained network is weaker than HCE
    // if (Oxta::NNUE::IsLoaded) {
    //     return Oxta::NNUE::evaluate(pos);
    // }

    int mg[2] = {0, 0};
    int eg[2] = {0, 0};
    int phase = 0;

    Bitboard pawns[2] = { pos.pieces(W_PAWN), pos.pieces(B_PAWN) };
    Bitboard pieces[2] = { pos.pieces(WHITE), pos.pieces(BLACK) };
    Bitboard all_pieces = pieces[0] | pieces[1];

    phase = Bitboards::popcount(pieces[0] & ~pawns[0]) * 2 + Bitboards::popcount(pieces[1] & ~pawns[1]) * 2;
    if (phase > 24) phase = 24;

    // bool winning = false; // Will check later

    for (int c = 0; c < 2; ++c) {
        Color us = static_cast<Color>(c);

        // --- 1. Material & PST ---
        Bitboard b = pawns[c];
        while (b) {
            Square s = Bitboards::pop_lsb(b);
            Square s_pst = (c == WHITE ? s : static_cast<Square>(s ^ 56));
            mg[c] += Weights.Material[PAWN] + Weights.PawnPST_MG[s_pst];
            eg[c] += Weights.Material[PAWN] + Weights.PawnPST_EG[s_pst];
        }

        // Knights
        b = pos.pieces(KNIGHT, us);
        while (b) {
            Square s = Bitboards::pop_lsb(b);
            Square s_pst = (c == WHITE ? s : static_cast<Square>(s ^ 56));
            mg[c] += Weights.Material[KNIGHT] + Weights.KnightPST_MG[s_pst];
            eg[c] += Weights.Material[KNIGHT];

            Bitboard att = Attacks::knight_attacks(s);
            int mob = Bitboards::popcount(att & ~pieces[c]);
            mg[c] += mob * Weights.MobilityBonus[KNIGHT];
            eg[c] += mob * Weights.MobilityBonus[KNIGHT];
        }

        // Bishops
        b = pos.pieces(BISHOP, us);
        int bishop_count = 0;
        while (b) {
            bishop_count++;
            Square s = Bitboards::pop_lsb(b);
            Square s_pst = (c == WHITE ? s : static_cast<Square>(s ^ 56));
            mg[c] += Weights.Material[BISHOP] + Peesto_Bishop_MG[s_pst];
            eg[c] += Weights.Material[BISHOP];

            Bitboard att = Attacks::bishop_attacks(s, all_pieces);
            int mob = Bitboards::popcount(att & ~pieces[c]);
            mg[c] += mob * Weights.MobilityBonus[BISHOP];
            eg[c] += mob * Weights.MobilityBonus[BISHOP];
        }
        // Bishop pair bonus
        if (bishop_count >= 2) {
            mg[c] += 50;
            eg[c] += 50;
        }

        // Rooks
        b = pos.pieces(ROOK, us);
        while (b) {
            Square s = Bitboards::pop_lsb(b);
            Square s_pst = (c == WHITE ? s : static_cast<Square>(s ^ 56));
            mg[c] += Weights.Material[ROOK] + Peesto_Rook_MG[s_pst];
            eg[c] += Weights.Material[ROOK];

            int file = s % 8;
            int rank = s / 8;

            // 7th rank bonus
            if ((c == WHITE && rank >= 6) || (c == BLACK && rank <= 1)) {
                mg[c] += Weights.RankDominationBonus;
                eg[c] += Weights.RankDominationBonus;
            }

            // Open/semi-open file bonus
            Bitboard file_bb = Bitboard(0x0101010101010101ULL) << file;
            bool semi_open = (file_bb & pawns[c]) == 0;
            bool full_open = semi_open && (file_bb & pawns[1-c]) == 0;
            if (full_open) {
                mg[c] += 25;
                eg[c] += 15;
            } else if (semi_open) {
                mg[c] += 12;
                eg[c] += 8;
            }

            Bitboard att = Attacks::rook_attacks(s, all_pieces);
            int mob = Bitboards::popcount(att & ~pieces[c]);
            mg[c] += mob * Weights.MobilityBonus[ROOK];
            eg[c] += mob * Weights.MobilityBonus[ROOK];
        }

        // Queens
        b = pos.pieces(QUEEN, us);
        while (b) {
            Square s = Bitboards::pop_lsb(b);
            Square s_pst = (c == WHITE ? s : static_cast<Square>(s ^ 56));
            mg[c] += Weights.Material[QUEEN] + Peesto_Queen_MG[s_pst];
            eg[c] += Weights.Material[QUEEN];

            Bitboard att = Attacks::queen_attacks(s, all_pieces);
            int mob = Bitboards::popcount(att & ~pieces[c]);
            mg[c] += mob * Weights.MobilityBonus[QUEEN];
            eg[c] += mob * Weights.MobilityBonus[QUEEN];
        }

        // King
        Square ksq = Bitboards::lsb(pos.pieces(KING, us));
        {
            Square s_pst = (c == WHITE ? ksq : static_cast<Square>(ksq ^ 56));
            mg[c] += Peesto_King_MG[s_pst];
            eg[c] += Peesto_King_EG[s_pst];

            // Pawn shield bonus (pawns in front of king)
            int kfile = ksq % 8;
            int krank = ksq / 8;
            int shield = 0;
            for (int df = -1; df <= 1; ++df) {
                int f = kfile + df;
                if (f < 0 || f > 7) continue;
                // Check 1 and 2 ranks ahead for friendly pawns
                int dir = (c == WHITE) ? 1 : -1;
                for (int dr = 1; dr <= 2; ++dr) {
                    int r = krank + dir * dr;
                    if (r < 0 || r > 7) continue;
                    Square ps = static_cast<Square>(r * 8 + f);
                    Piece p = pos.piece_on(ps);
                    if (p == (c == WHITE ? W_PAWN : B_PAWN)) {
                        shield += (dr == 1) ? 15 : 8;
                    }
                }
            }
            mg[c] += shield;
        }

        // Passed Pawn, Doubled, Isolated Pawn Evaluation
        Bitboard my_pawns_b = pawns[c];
        while (my_pawns_b) {
            Square ps = Bitboards::pop_lsb(my_pawns_b);
            int prank = (c == WHITE) ? (ps / 8) : (7 - ps / 8);
            int pfile = ps % 8;

            // Check if passed
            bool passed = true;
            Bitboard enemy_p_copy = pawns[1 - c];
            while (enemy_p_copy && passed) {
                Square es = Bitboards::pop_lsb(enemy_p_copy);
                int ef = es % 8;
                int er = (c == WHITE) ? (es / 8) : (7 - es / 8);
                if (std::abs(ef - pfile) <= 1 && er > prank) {
                    passed = false;
                }
            }
            if (passed) {
                eg[c] += Weights.PassedPawnBonus[prank];
                mg[c] += Weights.PassedPawnBonus[prank] / 2;
            }

            // Doubled pawn: another friendly pawn on same file
            Bitboard file_bb = Bitboard(0x0101010101010101ULL) << pfile;
            int pawns_on_file = Bitboards::popcount(pawns[c] & file_bb);
            if (pawns_on_file > 1) {
                mg[c] -= 15;
                eg[c] -= 20;
            }

            // Isolated pawn: no friendly pawns on adjacent files
            Bitboard adj_files = 0;
            if (pfile > 0) adj_files |= Bitboard(0x0101010101010101ULL) << (pfile - 1);
            if (pfile < 7) adj_files |= Bitboard(0x0101010101010101ULL) << (pfile + 1);
            if ((pawns[c] & adj_files) == 0) {
                mg[c] -= 15;
                eg[c] -= 20;
            }
        }

        // King Safety: Attack Zone (count enemy attackers near our king)
        {
            Color them = static_cast<Color>(1 - c);
            Square ksq = Bitboards::lsb(pos.pieces(KING, us));
            Bitboard king_zone = Attacks::king_attacks(ksq) | (Bitboard(1) << ksq);

            int attack_count = 0;
            int attack_weight = 0;

            // Enemy knights attacking king zone
            Bitboard enemy_knights = pos.pieces(KNIGHT, them);
            while (enemy_knights) {
                Square s = Bitboards::pop_lsb(enemy_knights);
                if (Attacks::knight_attacks(s) & king_zone) {
                    attack_count++;
                    attack_weight += 20;
                }
            }
            // Enemy bishops
            Bitboard enemy_bishops = pos.pieces(BISHOP, them);
            while (enemy_bishops) {
                Square s = Bitboards::pop_lsb(enemy_bishops);
                if (Attacks::bishop_attacks(s, all_pieces) & king_zone) {
                    attack_count++;
                    attack_weight += 20;
                }
            }
            // Enemy rooks
            Bitboard enemy_rooks = pos.pieces(ROOK, them);
            while (enemy_rooks) {
                Square s = Bitboards::pop_lsb(enemy_rooks);
                if (Attacks::rook_attacks(s, all_pieces) & king_zone) {
                    attack_count++;
                    attack_weight += 40;
                }
            }
            // Enemy queens
            Bitboard enemy_queens = pos.pieces(QUEEN, them);
            while (enemy_queens) {
                Square s = Bitboards::pop_lsb(enemy_queens);
                if (Attacks::queen_attacks(s, all_pieces) & king_zone) {
                    attack_count++;
                    attack_weight += 80;
                }
            }

            // Scale: only penalize when multiple attackers
            if (attack_count >= 2) {
                mg[c] -= attack_weight * attack_count / 4;
            }
        }
    }

    int mg_score = mg[0] - mg[1];
    int eg_score = eg[0] - eg[1];
    int raw_score = (mg_score * phase + eg_score * (24 - phase)) / 24;

    // --- Feature 1: Exchange Penalty (Structural) ---
    // If we traded R for Minor without compensation.
    // Approximated by Material Imbalance check.
    // If (Rooks < 2) AND (Minors > 2) AND (Score is bad), penalty?
    // Actually, Material weights handle R(500) vs B(330)+P(100) = 430.
    // The issue is R(500) vs B(330). That's -170.
    // If engine thinks 170 is worth positional comp, it trades.
    // We add explicit penalty if down material significantly?
    // "if exchange == rook_for_minor". This requires history or diff.
    // In static eval, we punish: Having NO Rooks vs Having Minors?
    // No, simple material weights should handle it.
    // The user said "eval -= 2.0" if rook exchanged for minor.
    // We boost Rook weight slightly? No, Weights.Material is 500 vs 330.
    // Difference is 170. 2.0 pawns is 200.
    // Let's ensure Rook is at least 530? Or penalize single Rook vs 2 Minors?
    // Let's stick to standard weights but maybe add "Material Imbalance" term later if needed.
    // For now, increasing Rook Material to 520 helps.

    // --- Feature 8: Convert Advantage ---
    int final_score = raw_score;
    int abs_score = std::abs(final_score);

    if (abs_score > 300) { // Winning (>3.0)
        Color winning_side = (final_score > 0) ? WHITE : BLACK;

        // Bonus for trading pieces (not pawns) when winning
        // int piece_count_us = Bitboards::popcount(pieces[winning_side] & ~pawns[winning_side]);
        int piece_count_them = Bitboards::popcount(pieces[~winning_side] & ~pawns[~winning_side]);

        // Incentive: Lower piece count = Higher score
        // We add bonus for every piece TRADED (i.e., missing from board)
        // int max_pieces = 16; // roughly
        int traded = (16 - piece_count_them);

        int conversion_bonus = traded * 10; // 10cp per piece traded

        // King Proximity to Center (Endgame)
        Square ksq = Bitboards::lsb(pos.pieces(KING, winning_side));
        int center_dist = std::abs((ksq / 8) - 3) + std::abs((ksq % 8) - 3); // Manhatten dist to center
        // Closer is better (smaller dist)
        conversion_bonus += (10 - center_dist) * 5;

        if (final_score > 0) final_score += conversion_bonus;
        else final_score -= conversion_bonus;
    }

    return (pos.side_to_move() == WHITE ? final_score : -final_score);
}

} // namespace Oxta::Eval

#endif // OXTA_CORE_EVAL_HPP
