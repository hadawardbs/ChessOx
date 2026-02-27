#ifndef OXTA_NNUE_NNUE_HPP
#define OXTA_NNUE_NNUE_HPP

#include "../core/position.hpp"
#include "layers.hpp"
#include <memory>
#include <array>

namespace Oxta::NNUE {

// Accumulator for Incremental Updates
// We store the state of the first hidden layer (256x2)
struct Accumulator {
    std::array<int16_t, HIDDEN1> white;
    std::array<int16_t, HIDDEN1> black;
    bool computed_white = false;
    bool computed_black = false;

    void init(const int16_t* biases) {
        std::copy(biases, biases + HIDDEN1, white.begin());
        std::copy(biases, biases + HIDDEN1, black.begin());
    }
};

class Network {
public:
    // Feature Transformer: 768 -> 256
    // We store weights as [256][768] roughly.
    // Actually standard NNUE uses HalfKP (King-Piece) which is [64*768] -> 256.
    // For simplicity of implementation "Synthesize", we use Simple 768 (HalfKA).

    int16_t feature_biases[HIDDEN1];
    int16_t feature_weights[HIDDEN1 * INPUT_SIZE]; // [Output][Input]

    // Output Layers
    // Hidden1 (512) -> 32
    // Hidden2 (32) -> 32
    // Output (32) -> 1
    // Simplification: Direct (512 -> 1) or small MLP.
    // Let's implement a standard 2-layer tail.

    LinearLayer<HIDDEN1 * 2, 32> layer1;
    LinearLayer<32, 32> layer2;
    LinearLayer<32, 1> output;

    bool load(const std::string& filename);
    int evaluate(const Position& pos, const Accumulator& acc);

    // Refresh accumulator from scratch
    void refresh_accumulator(const Position& pos, Accumulator& acc) const;
    // Update accumulator incrementally
    void update_accumulator(const Position& pos, Accumulator& next, const Accumulator& prev) const;
};

// Global Network Instance
extern std::unique_ptr<Network> GlobalNet;
extern bool IsLoaded;

// Helper to check and init
void init(const std::string& filename);
int evaluate(const Position& pos);

} // namespace Oxta::NNUE

#endif // OXTA_NNUE_NNUE_HPP
