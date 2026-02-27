#include <iostream>
#include "core/position.hpp"
#include "core/eval.hpp"
#include "core/attacks.hpp"
#include "core/zobrist.hpp"

using namespace Oxta;

// Define global Weights
Eval::EvalWeights Eval::Weights;

int main() {
    Zobrist::init();
    Attacks::init();
    Eval::init();

    Position pos;
    pos.set("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    int score = Eval::evaluate(pos);
    std::cout << "Evaluation: " << score << std::endl;
    
    return 0;
}
