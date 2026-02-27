#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <fstream>
#include "core/uci.hpp"
#include "core/position.hpp"
#include "search/search.hpp"
#include "core/eval.hpp"
#include "core/bitboard.hpp"
#include "core/zobrist.hpp"
#include "core/attacks.hpp"
#include "nnue/nnue.hpp"

// Define global Weights
Oxta::Eval::EvalWeights Oxta::Eval::Weights;

using namespace Oxta;

int main() {
    // Initialize systems
    // std::cout << "info string Initializing Zobrist..." << std::endl;
    Zobrist::init();

    // std::cout << "info string Initializing Attacks..." << std::endl;
    Attacks::init();

    // std::cout << "info string Initializing Weights..." << std::endl;
    Eval::init();

    // Try to load NNUE
    Oxta::NNUE::init("nnue.bin");

    // Attempt to load external weights if available (Texel Tuning Hook)
    std::ifstream w_file("weights.bin", std::ios::binary);
    if (w_file) {
        w_file.read(reinterpret_cast<char*>(&Eval::Weights), sizeof(Eval::Weights));
        // std::cout << "info string Loaded external weights." << std::endl;
    }

    // Instantiate UCI and run loop
    UCI uci;
    uci.loop();

    return 0;
}
