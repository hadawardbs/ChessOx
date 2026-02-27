#ifndef OXTA_SEARCH_HEURISTICS_HPP
#define OXTA_SEARCH_HEURISTICS_HPP

#include "../core/types.hpp"
#include "../core/moves.hpp"
#include <algorithm>
#include <cstring>

namespace Oxta::Search {

// Killer Moves: Store 2 killer moves per ply
struct KillerMoves {
    Move killers[MAX_PLY][2];

    KillerMoves() {
        clear();
    }

    void clear() {
        std::memset(killers, 0, sizeof(killers));
    }

    void add(Move m, int ply) {
        if (ply >= MAX_PLY) return;
        if (m != killers[ply][0]) {
            killers[ply][1] = killers[ply][0];
            killers[ply][0] = m;
        }
    }

    bool is_killer(Move m, int ply) const {
        if (ply >= MAX_PLY) return false;
        return m == killers[ply][0] || m == killers[ply][1];
    }
};

// History Heuristic: Score[Color][From][To]
struct HistoryHeuristic {
    int16_t table[2][64][64]; // [Side][From][To]

    HistoryHeuristic() {
        clear();
    }

    void clear() {
        std::memset(table, 0, sizeof(table));
    }

    // Age history scores (call at start of each new search)
    void age() {
        for (int c = 0; c < 2; ++c)
            for (int f = 0; f < 64; ++f)
                for (int t = 0; t < 64; ++t)
                    table[c][f][t] = table[c][f][t] * 15 / 16;
    }

    void update(Color side, Move m, int depth) {
        int bonus = depth * depth;
        int16_t& entry = table[side][m.from()][m.to()];

        // Gravity formula: pull toward zero + add bonus
        entry += bonus - entry * abs(bonus) / 512;
    }

    int get(Color side, Move m) const {
        return table[side][m.from()][m.to()];
    }
};

// Countermove Heuristic: best response to opponent's last move
// Indexed by [piece_that_moved][to_square]
struct CounterMoveTable {
    Move table[PIECE_NB][64];

    CounterMoveTable() { clear(); }

    void clear() {
        std::memset(table, 0, sizeof(table));
    }

    void update(Piece prev_piece, Square prev_to, Move response) {
        if (prev_piece != NO_PIECE && prev_piece < PIECE_NB && prev_to < 64) {
            table[prev_piece][prev_to] = response;
        }
    }

    Move get(Piece prev_piece, Square prev_to) const {
        if (prev_piece != NO_PIECE && prev_piece < PIECE_NB && prev_to < 64) {
            return table[prev_piece][prev_to];
        }
        return Move::NONE;
    }
};

} // namespace Oxta::Search

#endif // OXTA_SEARCH_HEURISTICS_HPP

