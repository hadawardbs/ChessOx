#ifndef OXTA_UTIL_PERFT_HPP
#define OXTA_UTIL_PERFT_HPP

#include "../core/position.hpp"
#include "../core/movegen.hpp"
#include "../core/attacks.hpp"
#include <iostream>
#include <chrono>

namespace Oxta::Perft {

// Check if square is attacked by side 'by'
inline bool is_square_attacked(const Position& pos, Square s, Color by) {
    // Pawn attacks
    if (by == WHITE) {
        if (Attacks::pawn_attacks(s, BLACK) & pos.pieces(W_PAWN)) return true;
    } else {
        if (Attacks::pawn_attacks(s, WHITE) & pos.pieces(B_PAWN)) return true;
    }

    // Knight
    if (Attacks::knight_attacks(s) & pos.pieces(by == WHITE ? W_KNIGHT : B_KNIGHT)) return true;

    // King
    if (Attacks::king_attacks(s) & pos.pieces(by == WHITE ? W_KING : B_KING)) return true;

    // Sliders
    Bitboard occ = pos.pieces();
    Bitboard rooks = pos.pieces(by == WHITE ? W_ROOK : B_ROOK) | pos.pieces(by == WHITE ? W_QUEEN : B_QUEEN);
    if (Attacks::rook_attacks(s, occ) & rooks) return true;

    Bitboard bishops = pos.pieces(by == WHITE ? W_BISHOP : B_BISHOP) | pos.pieces(by == WHITE ? W_QUEEN : B_QUEEN);
    if (Attacks::bishop_attacks(s, occ) & bishops) return true;

    return false;
}

inline bool is_check(const Position& pos, Color side) {
    Square ksq = Bitboards::lsb(pos.pieces(side == WHITE ? W_KING : B_KING));
    return is_square_attacked(pos, ksq, ~side);
}

inline uint64_t perft(Position& pos, int depth) {
    if (depth == 0) return 1;

    uint64_t nodes = 0;
    Oxta::MoveList moves = MoveGen::generate_all(pos);

    for (int i = 0; i < moves.size(); ++i) {
        Move m = moves[i];

        if (m.is_castle()) {
             if (is_check(pos, pos.side_to_move())) continue;
             Square kFrom = m.from();
             Square kTo = m.to();
             // Simple average for mid square if on same rank/file
             // Castling is horizontal.
             // e1=4, g1=6 -> mid=5 (f1)
             // e1=4, c1=2 -> mid=3 (d1)
             // e8=60, g8=62 -> mid=61 (f8)
             // e8=60, c8=58 -> mid=59 (d8)
             // This math works because enum is 0-63.
             Square mid = static_cast<Square>((kFrom + kTo) / 2);
             if (is_square_attacked(pos, mid, ~pos.side_to_move())) continue;
        }

        Position::StateInfo st;
        pos.do_move(m, st);

        if (!is_check(pos, ~pos.side_to_move())) {
            nodes += perft(pos, depth - 1);
        }

        pos.undo_move(m, st);
    }
    return nodes;
}

inline void run_benchmark(Position& pos, int depth) {
    auto start = std::chrono::high_resolution_clock::now();
    uint64_t nodes = perft(pos, depth);
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> diff = end - start;
    double nps = (diff.count() > 0) ? (nodes / diff.count()) : 0.0;

    std::cout << "Depth: " << depth << std::endl;
    std::cout << "Nodes: " << nodes << std::endl;
    std::cout << "Time:  " << diff.count() << " s" << std::endl;
    std::cout << "NPS:   " << static_cast<uint64_t>(nps) << std::endl;
}

} // namespace Oxta::Perft

#endif // OXTA_UTIL_PERFT_HPP
