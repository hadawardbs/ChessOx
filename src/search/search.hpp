#ifndef OXTA_SEARCH_SEARCH_HPP
#define OXTA_SEARCH_SEARCH_HPP

#include "../core/position.hpp"
#include "../core/eval.hpp"
#include "../core/see.hpp"
#include "../memory/tt.hpp"
#include "../util/perft.hpp"
#include "heuristics.hpp"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <atomic>
#include <cmath>
#include <vector>
#include <numeric>

namespace Oxta::Search {

constexpr int INF = 30000;
constexpr int CHECKMATE = 29000;

struct SearchLimits {
    int depth = 0;
    int nodes = 0;
    int movetime = 0;
    bool infinite = false;
};

struct SearchInfo {
    std::atomic<uint64_t> nodes {0};
    std::chrono::time_point<std::chrono::high_resolution_clock> start;
    std::atomic<bool> stop {false};

    void reset() {
        nodes = 0;
        start = std::chrono::high_resolution_clock::now();
        stop = false;
    }

    long elapsed() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
    }
};

class Searcher {
public:
    bool silent = false;
    SearchLimits current_limits;

    Searcher() { }

    Searcher(TranspositionTable* shared_tt) {
        tt_ptr = shared_tt;
        owns_tt = false;
    }

    TranspositionTable* tt_ptr = nullptr;
    bool owns_tt = false;

    void set_tt(TranspositionTable* t) {
        if (owns_tt && tt_ptr) { delete tt_ptr; tt_ptr = nullptr; }
        tt_ptr = t;
        owns_tt = false;
    }

    ~Searcher() {
        if (owns_tt && tt_ptr) delete tt_ptr;
    }

    void resize_tt(size_t mb) {
        if (!tt_ptr) { tt_ptr = new TranspositionTable(); owns_tt = true; }
        tt_ptr->resize(mb);
    }

    KillerMoves killers;
    HistoryHeuristic history;
    CounterMoveTable countermoves;

    // Search stack for improving detection
    struct SearchStack {
        int static_eval = 0;
        Move current_move = Move::NONE;
        Piece moved_piece = NO_PIECE;
    };
    SearchStack ss[MAX_PLY + 4];

    std::string uci_move(Move m) {
        std::string s = "";
        s += square_to_string(m.from());
        s += square_to_string(m.to());
        if (m.is_promotion()) {
            PieceType pt = m.promotion_type();
            if (pt == QUEEN) s += 'q';
            else if (pt == ROOK) s += 'r';
            else if (pt == BISHOP) s += 'b';
            else if (pt == KNIGHT) s += 'n';
        }
        return s;
    }

    std::string square_to_string(Square s) {
        std::string res = "";
        res += (char)('a' + (s % 8));
        res += (char)('1' + (s / 8));
        return res;
    }

    void iter_deep(Position& pos, SearchLimits limits) {
        if (!tt_ptr) resize_tt(64);
        current_limits = limits;

        info.reset();
        if (tt_ptr) tt_ptr->new_search();
        killers.clear();
        history.age(); // Age history instead of clearing (preserves learning)
        countermoves.clear();
        for (auto& s : ss) s = SearchStack{};

        int max_depth = limits.depth;
        if (max_depth == 0 && (limits.movetime > 0 || limits.infinite)) {
            max_depth = MAX_PLY;
        }

        int prev_score = 0;

        for (int depth = 1; depth <= max_depth; ++depth) {
            int score = 0;
            int delta = 25;
            int alpha_w = -INF;
            int beta_w = INF;

            if (depth >= 5) {
                alpha_w = std::max(-INF, prev_score - delta);
                beta_w = std::min(INF, prev_score + delta);
            }

            while (true) {
                score = negamax(pos, depth, alpha_w, beta_w, 0, true);

                if (info.stop) break;

                if (score <= alpha_w) {
                    alpha_w = std::max(-INF, alpha_w - delta);
                    delta += delta / 2;
                } else if (score >= beta_w) {
                    beta_w = std::min(INF, beta_w + delta);
                    delta += delta / 2;
                } else {
                    break;
                }
                
                if (std::abs(alpha_w) >= INF && std::abs(beta_w) >= INF) break;
            }

            // Feature 10: Blunder Detection & Extension
            if (depth > 1) {
                int delta = std::abs(score - prev_score);
                if (delta > 300) {
                    // Could extend search time here
                }
            }

            // Lightweight metrics (no re-evaluation)
            int eg = std::abs(score - prev_score);

            prev_score = score;

            if (info.stop) break;

            long time = info.elapsed();

            if (!silent) {
                uint64_t nps = (time > 0) ? (info.nodes * 1000 / time) : 0;
                std::cout << "info depth " << depth
                          << " score cp " << score
                          << " nodes " << info.nodes
                          << " nps " << nps
                          << " time " << time
                          << " pv";

                if (tt_ptr) {
                    Position temp = pos;
                    for (int i = 0; i < depth; ++i) {
                        Move m; int16_t v; uint8_t d; TTFlag f;
                        if (tt_ptr->probe(temp.key(), m, v, d, f) && m != Move::NONE) {
                            std::cout << " " << uci_move(m);
                            Position::StateInfo st;
                            Oxta::MoveList moves = MoveGen::generate_all(temp);
                            bool legal = false;
                            for(int j=0; j<moves.size(); ++j) if(moves[j] == m) legal = true;
                            if(!legal) break;
                            temp.do_move(m, st);
                        } else {
                            break;
                        }
                    }
                }
                std::cout << " string eg " << eg;
                std::cout << std::endl << std::flush;
            }

            if (limits.movetime > 0 && time > limits.movetime) {
                info.stop = true;
                break;
            }
            if (std::abs(score) > CHECKMATE - 100) {
                 break;
            }
        }

        if (!silent) {
            Move best = Move::NONE;
            if (tt_ptr) {
                Move m; int16_t v; uint8_t d; TTFlag f;
                if (tt_ptr->probe(pos.key(), m, v, d, f)) best = m;
            }
            if (best == Move::NONE) {
                Oxta::MoveList moves = MoveGen::generate_all(pos);
                score_moves(moves, Move::NONE, 0, pos.side_to_move(), pos);
                int best_sc = -30000;
                for(int j=0; j<moves.size(); ++j) {
                    Position::StateInfo st;
                    pos.do_move(moves[j], st);
                    if(!Perft::is_check(pos, ~pos.side_to_move())) {
                         if (moves.scores[j] > best_sc) { best_sc = moves.scores[j]; best = moves[j]; }
                    }
                    pos.undo_move(moves[j], st);
                }
            }
            std::cout << "bestmove " << (best != Move::NONE ? uci_move(best) : "0000") << std::endl << std::flush;
        }
    }

    SearchInfo info;

private:
    int quiescence(Position& pos, int alpha, int beta, int ply = 0) {
        info.nodes++;
        if ((info.nodes & 2047) == 0) check_limits();

        bool in_check = Perft::is_check(pos, pos.side_to_move());

        int stand_pat = -INF;
        if (!in_check) {
            stand_pat = Eval::evaluate(pos);
            if (stand_pat >= beta) return beta;
            if (alpha < stand_pat) alpha = stand_pat;

            // Delta pruning: if even capturing a queen can't raise alpha, skip
            if (stand_pat + 1050 < alpha) return alpha;
        }

        // If in check, generate all moves (evasions); otherwise captures only
        Oxta::MoveList moves = in_check ? MoveGen::generate_all(pos) : MoveGen::generate_captures(pos);
        score_moves(moves, Move::NONE, 0, pos.side_to_move(), pos);

        int legal_moves = 0;
        for (int i = 0; i < moves.size(); ++i) {
            int best_idx = i;
            int best_sc = -30000;
            for(int j=i; j<moves.size(); ++j) {
                if (moves.scores[j] > best_sc) { best_sc = moves.scores[j]; best_idx = j; }
            }
            std::swap(moves.moves[i], moves.moves[best_idx]);
            std::swap(moves.scores[i], moves.scores[best_idx]);

            Move m = moves[i];
            if (!in_check && !m.is_capture()) continue;

            // SEE Pruning: Skip bad captures (not when in check)
            if (!in_check && !SEE::see_ge(pos, m, -50)) continue;

            Position::StateInfo st;
            pos.do_move(m, st);

            if (!Perft::is_check(pos, ~pos.side_to_move())) {
                legal_moves++;
                int score = -quiescence(pos, -beta, -alpha, ply + 1);
                pos.undo_move(m, st);

                if (score >= beta) return beta;
                if (score > alpha) alpha = score;
            } else {
                pos.undo_move(m, st);
            }
        }

        // Checkmate detection in QSearch
        if (in_check && legal_moves == 0) return -CHECKMATE + ply;

        return alpha;
    }

    int negamax(Position& pos, int depth, int alpha, int beta, int ply, bool is_pv) {
        info.nodes++;
        if ((info.nodes & 2047) == 0) check_limits();
        if (info.stop) return 0;

        bool root = (ply == 0);
        bool in_check = Perft::is_check(pos, pos.side_to_move());

        if (in_check) depth++;

        if (!root && depth <= 0) {
            return quiescence(pos, alpha, beta, ply);
        }

        if (!root) {
            alpha = std::max(alpha, -CHECKMATE + ply);
            beta = std::min(beta, CHECKMATE - ply);
            if (alpha >= beta) return alpha;
        }

        int static_eval = Eval::evaluate(pos);
        ss[ply].static_eval = static_eval;

        // Improving: are we doing better than 2 plies ago?
        bool improving = (ply >= 2 && static_eval > ss[ply - 2].static_eval);

        // Razoring
        if (!root && !is_pv && !in_check && depth <= 3) {
            int razor_margin = depth * 150;
            if (static_eval + razor_margin < beta) {
                int qscore = quiescence(pos, alpha, beta, ply);
                if (qscore < beta) return qscore;
            }
        }

        // Reverse Futility Pruning (static eval too good)
        if (!root && !is_pv && !in_check && depth <= 6 &&
            static_eval - (100 - 20 * improving) * depth >= beta && std::abs(beta) < CHECKMATE - 1000) {
             return static_eval;
        }

        // Null Move Pruning — adaptive R based on eval
        if (!root && !in_check && depth >= 3 && !is_pv && static_eval >= beta) {
            int R = 3 + depth / 6 + std::min(3, (static_eval - beta) / 200);
            Position::StateInfo st;
            pos.do_null_move(st);
            int score = -negamax(pos, depth - R - 1, -beta, -beta + 1, ply + 1, false);
            pos.undo_null_move(st);
            if (score >= beta) return beta;
        }

        // Multi-cut pruning
        if (!root && !is_pv && !in_check && depth >= 5 && std::abs(beta) < CHECKMATE - 1000) {
            int margin = 200;
            int score = negamax(pos, depth - 4, beta + margin - 1, beta + margin, ply+1, false);
            if (score >= beta + margin) return beta;
        }

        Move tt_move = Move::NONE;
        if (tt_ptr) {
            Move m; int16_t v; uint8_t d; TTFlag f;
            if (tt_ptr->probe(pos.key(), m, v, d, f)) {
                tt_move = m;
                if (d >= depth && !root && !is_pv) {
                    if (f == TT_EXACT) return v;
                    if (f == TT_LOWER && v >= beta) return v;
                    if (f == TT_UPPER && v <= alpha) return v;
                }
            }
        }

        // IIR: reduce depth when we have no hash move (poor ordering)
        if (tt_move == Move::NONE && depth >= 4 && !root) {
            depth--;
        }

        // Get countermove for move ordering
        Move counter_move = Move::NONE;
        if (ply > 0) {
            counter_move = countermoves.get(ss[ply-1].moved_piece, ss[ply-1].current_move.to());
        }

        Oxta::MoveList moves = MoveGen::generate_all(pos);
        score_moves(moves, tt_move, ply, pos.side_to_move(), pos, counter_move);

        int best_score = -INF;
        Move best_move = Move::NONE;
        TTFlag flag = TT_UPPER;
        int legal_moves = 0;
        int moves_searched = 0;

        for (int i = 0; i < moves.size(); ++i) {
            int best_idx = i;
            int best_sc = -30000;
            for(int j=i; j<moves.size(); ++j) {
                if (moves.scores[j] > best_sc) { best_sc = moves.scores[j]; best_idx = j; }
            }
            std::swap(moves.moves[i], moves.moves[best_idx]);
            std::swap(moves.scores[i], moves.scores[best_idx]);

            Move move = moves[i];

            bool quiet = !move.is_capture() && !move.is_promotion();

            // Futility Pruning: if static_eval + margin < alpha, skip quiet moves
            if (!root && !is_pv && !in_check && quiet && depth <= 6 && moves_searched > 0) {
                int futility = static_eval + 120 * depth;
                if (futility <= alpha) continue;
            }

            // LMP: Late Move Pruning
            if (!root && !is_pv && !in_check && quiet && depth <= 8) {
                int lmp_count = (3 + depth * depth) / (2 - improving);
                if (moves_searched > lmp_count) continue;
            }

            // Store move info for countermove heuristic
            ss[ply].current_move = move;
            ss[ply].moved_piece = pos.piece_on(move.from());

            Position::StateInfo st;
            pos.do_move(move, st);

            if (!Perft::is_check(pos, ~pos.side_to_move())) {
                legal_moves++;
                moves_searched++;

                int score;
                int reduction = 0;

                if (depth >= 3 && moves_searched > 1 && quiet && !is_pv && !in_check) {
                    reduction = 1 + (std::log(depth) * std::log(moves_searched) / 2);
                    // History-based adjustment: good history = less reduction
                    reduction -= history.get(pos.side_to_move(), move) / 5000;
                    // Improving: if position is getting better, reduce less
                    if (!improving) reduction++;
                    // Killer bonus
                    if (killers.is_killer(move, ply)) reduction--;
                    // Clamp
                    reduction = std::clamp(reduction, 0, depth - 2);
                }

                if (moves_searched == 1) {
                    score = -negamax(pos, depth - 1, -beta, -alpha, ply + 1, is_pv);
                } else {
                    score = -negamax(pos, depth - 1 - reduction, -alpha - 1, -alpha, ply + 1, false);
                    if (score > alpha && reduction > 0) {
                        score = -negamax(pos, depth - 1, -alpha - 1, -alpha, ply + 1, false);
                    }
                    if (score > alpha && score < beta) {
                        score = -negamax(pos, depth - 1, -beta, -alpha, ply + 1, true);
                    }
                }

                pos.undo_move(move, st);

                if (info.stop) return 0;

                if (score > best_score) {
                    best_score = score;
                    best_move = move;
                    if (score > alpha) {
                        alpha = score;
                        flag = TT_EXACT;
                        if (quiet) {
                            history.update(pos.side_to_move(), move, depth);
                            killers.add(move, ply);
                        }
                    }
                }

                if (alpha >= beta) {
                    flag = TT_LOWER;
                    if (quiet) {
                        history.update(pos.side_to_move(), move, depth);
                        killers.add(move, ply);
                        // Update countermove
                        if (ply > 0) {
                            countermoves.update(ss[ply-1].moved_piece, ss[ply-1].current_move.to(), move);
                        }
                    }
                    break;
                }
            } else {
                pos.undo_move(move, st);
            }
        }

        if (legal_moves == 0) {
            if (in_check) return -CHECKMATE + ply;
            else return 0;
        }

        if (tt_ptr) tt_ptr->save(pos.key(), best_move, best_score, depth, flag);

        return best_score;
    }

    void check_limits() {
        if (current_limits.movetime > 0) {
            if (info.elapsed() > current_limits.movetime) {
                info.stop = true;
            }
        }
        if (current_limits.nodes > 0) {
            if (info.nodes >= (uint64_t)current_limits.nodes) {
                info.stop = true;
            }
        }
    }

    void score_moves(Oxta::MoveList& ml, Move tt_move, int ply, Color side, const Position& pos, Move counter_move = Move::NONE) {
        for (int i = 0; i < ml.size(); ++i) {
            Move m = ml[i];
            int score = 0;

            if (m == tt_move) {
                score = 30000;
            } else if (m.is_capture()) {
                int victim = 0;
                if (m.is_en_passant()) victim = 100;
                else {
                    Piece p = pos.piece_on(m.to());
                    if (p != NO_PIECE) victim = SeeValues[type_of_piece(p)];
                }

                Piece p_from = pos.piece_on(m.from());
                int attacker = (p_from != NO_PIECE) ? SeeValues[type_of_piece(p_from)] : 0;

                score = 20000 + victim * 10 - attacker;
            } else {
                if (killers.is_killer(m, ply)) score = 9000;
                else if (m == counter_move) score = 7000; // Countermove bonus
                else score = history.get(side, m);

                // King Safety: penalize non-castle king moves
                Piece p_from = pos.piece_on(m.from());
                if (type_of_piece(p_from) == KING) {
                    if (!m.is_castle()) score -= 50;
                }
            }
            ml.scores[i] = score;
        }
    }
};

} // namespace Oxta::Search

#endif // OXTA_SEARCH_SEARCH_HPP
