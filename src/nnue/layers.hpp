#ifndef OXTA_NNUE_LAYERS_HPP
#define OXTA_NNUE_LAYERS_HPP

#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>

namespace Oxta::NNUE {

// Constants for Stockfish-compatible NNUE (HalfKP-like or similar)
// We will implement a simplified architecture: (Input 768 -> 256) x 2 -> 32 -> 32 -> 1
// Or standard HalfKP-256x2-32-32.

constexpr int INPUT_SIZE = 768; // 64 squares * 12 pieces
constexpr int HIDDEN1 = 256;
constexpr int QA = 255;
constexpr int QB = 64;
constexpr int Q_OUTPUT = 16;

// Clipped ReLU
inline int32_t crelu(int32_t x) {
    return std::max(0, std::min(x, (int32_t)QA));
}

// Linear Layer (Affine Transform)
template<int InDims, int OutDims>
struct LinearLayer {
    int16_t weights[OutDims * InDims]; // Flattened
    int16_t biases[OutDims];

    void forward(const int8_t* input, int32_t* output) const {
        for (int i = 0; i < OutDims; ++i) {
            int32_t sum = biases[i];
            for (int j = 0; j < InDims; ++j) {
                sum += weights[i * InDims + j] * input[j];
            }
            output[i] = sum;
        }
    }

    // Optimized forward for Accumulator output (int16 input, int32 output)
    void forward_from_16(const int16_t* input, int32_t* output) const {
         for (int i = 0; i < OutDims; ++i) {
            int32_t sum = biases[i];
            for (int j = 0; j < InDims; ++j) {
                sum += weights[i * InDims + j] * input[j];
            }
            output[i] = sum;
        }
    }
};

} // namespace Oxta::NNUE

#endif // OXTA_NNUE_LAYERS_HPP
