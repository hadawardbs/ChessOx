#ifndef OXTA_CORE_UCI_HPP
#define OXTA_CORE_UCI_HPP

#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include "position.hpp"
#include "../search/search.hpp"
#include "../search/thread.hpp"
#include "../search/time_man.hpp"

namespace Oxta {

class UCI {
public:
    UCI() {
        pos.set("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    }

    void loop() {
        std::string line, token;

        std::cout << "id name OxtaCore 1.2 Hybrid" << std::endl;
        std::cout << "id author Jules" << std::endl;
        std::cout << "option name Hash type spin default 64 min 1 max 1024" << std::endl;
        std::cout << "option name Threads type spin default 1 min 1 max 128" << std::endl;
        std::cout << "uciok" << std::endl;

        while (std::getline(std::cin, line)) {
            std::stringstream ss(line);
            ss >> token;

            if (token == "uci") {
                std::cout << "id name OxtaCore 1.2 Hybrid" << std::endl;
                std::cout << "id author Jules" << std::endl;
                std::cout << "option name Hash type spin default 64 min 1 max 1024" << std::endl;
                std::cout << "option name Threads type spin default 1 min 1 max 128" << std::endl;
                std::cout << "uciok" << std::endl;
            } else if (token == "isready") {
                std::cout << "readyok" << std::endl;
            } else if (token == "quit") {
                break;
            } else if (token == "position") {
                parse_position(ss);
            } else if (token == "go") {
                parse_go(ss);
            } else if (token == "stop") {
                pool.stop();
            } else if (token == "eval") {
                // New eval command for Hybrid MCTS Rollouts
                int score = Eval::evaluate(pos);
                std::cout << "score cp " << score << std::endl;
            } else if (token == "setoption") {
                std::string name, value_token;
                ss >> name >> name; // "name" "Hash"
                if (name == "Hash") {
                    ss >> value_token >> value_token; // "value" "64"
                    int mb = std::stoi(value_token);
                    pool.resize_tt(mb);
                }
            } else if (token == "ucinewgame") {
                pool.resize_tt(64);
            }
        }
    }

private:
    Position pos;
    Search::ThreadPool pool;
    Search::Searcher aux_searcher;

    void parse_position(std::stringstream& ss) {
        std::string token;
        ss >> token;

        if (token == "startpos") {
            pos.set("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
            ss >> token;
        } else if (token == "fen") {
            std::string fen;
            while (ss >> token && token != "moves") {
                fen += token + " ";
            }
            pos.set(fen);
        }

        if (token == "moves") {
            std::string move_str;
            while (ss >> move_str) {
                Oxta::MoveList moves = MoveGen::generate_all(pos);
                for (int i = 0; i < moves.size(); ++i) {
                    if (aux_searcher.uci_move(moves[i]) == move_str) {
                        Position::StateInfo st;
                        pos.do_move(moves[i], st);
                        break;
                    }
                }
            }
        }
    }

    void parse_go(std::stringstream& ss) {
        Search::SearchLimits limits;
        std::string token;
        int wtime = 0, btime = 0, winc = 0, binc = 0;

        while (ss >> token) {
            if (token == "depth") ss >> limits.depth;
            else if (token == "nodes") ss >> limits.nodes;
            else if (token == "movetime") ss >> limits.movetime;
            else if (token == "infinite") limits.infinite = true;
            else if (token == "wtime") ss >> wtime;
            else if (token == "btime") ss >> btime;
            else if (token == "winc") ss >> winc;
            else if (token == "binc") ss >> binc;
        }

        // Use TimeManager for wtime/btime
        if (limits.movetime == 0 && !limits.infinite && limits.depth == 0) {
            int time_left = (pos.side_to_move() == WHITE) ? wtime : btime;
            int inc = (pos.side_to_move() == WHITE) ? winc : binc;
            if (time_left > 0) {
                Search::TimeManager tm;
                tm.init(time_left, inc);
                limits.movetime = std::max(50, time_left / 40 + inc * 3 / 4);
                // Hard cap: never use more than 1/5 of remaining time
                if (limits.movetime > time_left / 5) limits.movetime = time_left / 5;
            }
        }

        if (limits.depth == 0 && limits.movetime == 0 && !limits.infinite) limits.depth = 6;

        pool.start_search(pos, limits);
    }
};

} // namespace Oxta

#endif // OXTA_CORE_UCI_HPP
