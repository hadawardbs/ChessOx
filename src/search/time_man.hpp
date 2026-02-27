#ifndef OXTA_SEARCH_TIME_MAN_HPP
#define OXTA_SEARCH_TIME_MAN_HPP

#include "../core/types.hpp"
#include "../core/moves.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>

namespace Oxta::Search {

class TimeManager {
public:
    void init(int time_left, int inc, int moves_to_go = 0) {
        int moves = (moves_to_go > 0) ? moves_to_go : 40;
        int optimal = time_left / moves + inc;

        soft_limit = optimal;
        hard_limit = time_left / 2;

        if (soft_limit > hard_limit) soft_limit = hard_limit;

        start_time = std::chrono::high_resolution_clock::now();
        best_move_changes = 0;
        current_best_move = Move::NONE;
    }

    long elapsed() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
    }

    void update_best_move(Move new_move) {
        if (new_move != current_best_move) {
            current_best_move = new_move;
            best_move_changes++;
        }
    }

    bool should_stop() {
        long t = elapsed();

        // Stability check: If best move unstable, extend time
        int limit = soft_limit;
        if (best_move_changes > 4) {
            limit *= 2; // Extend if chaotic
        }
        if (limit > hard_limit) limit = hard_limit;

        return t > limit;
    }

    bool hard_stop() {
        return elapsed() > hard_limit;
    }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    int soft_limit = 0;
    int hard_limit = 0;
    Move current_best_move = Move::NONE;
    int best_move_changes = 0;
};

} // namespace Oxta::Search

#endif // OXTA_SEARCH_TIME_MAN_HPP
