#include "nnue.hpp"
#include <fstream>
#include <iostream>

namespace Oxta::NNUE {

std::unique_ptr<Network> GlobalNet;
bool IsLoaded = false;

void init(const std::string& filename) {
    GlobalNet = std::make_unique<Network>();
    if (GlobalNet->load(filename)) {
        IsLoaded = true;
        std::cout << "info string NNUE loaded from " << filename << std::endl;
    } else {
        IsLoaded = false;
        std::cout << "info string NNUE load failed, using HCE" << std::endl;
    }
}

int evaluate(const Position& pos) {
    if (!IsLoaded) return 0;

    // For simplicity, we refresh accumulator every time.
    // Full incremental updates require integration into Position/StateInfo.
    Accumulator acc;
    GlobalNet->refresh_accumulator(pos, acc);

    return GlobalNet->evaluate(pos, acc);
}

// Network Implementation

bool Network::load(const std::string& filename) {
    std::ifstream f(filename, std::ios::binary);
    if (!f.is_open()) return false;

    // VERY Basic binary load.
    // In reality, NNUE files have headers and specific layouts.
    // We assume a RAW dump of our architecture for this "Reference Implementation".
    // 1. Feature Transformer
    f.read((char*)feature_biases, sizeof(feature_biases));
    f.read((char*)feature_weights, sizeof(feature_weights));

    // 2. Layer 1
    f.read((char*)layer1.biases, sizeof(layer1.biases));
    f.read((char*)layer1.weights, sizeof(layer1.weights));

    // 3. Layer 2
    f.read((char*)layer2.biases, sizeof(layer2.biases));
    f.read((char*)layer2.weights, sizeof(layer2.weights));

    // 4. Output
    f.read((char*)output.biases, sizeof(output.biases));
    f.read((char*)output.weights, sizeof(output.weights));

    return f.good();
}

void Network::refresh_accumulator(const Position& pos, Accumulator& acc) const {
    acc.init(feature_biases);

    // Scan all pieces
    Bitboard pieces = pos.pieces();
    while (pieces) {
        Square s = Bitboards::pop_lsb(pieces);
        Piece p = pos.piece_on(s);
        int piece_idx = p; // 0..11 usually (minus NO_PIECE?)
        // Assuming Piece is 1..12, we map to 0..11?
        // Let's assume standard index: 0=WP, 1=WN...
        // Piece enum usually: NO=0, WP=1 ...
        // We use p-1 if NO_PIECE is 0.
        if (p == NO_PIECE) continue;

        // Input Feature Index mapping (dense 0..11):
        // White Pieces: 1..6 -> 0..5
        // Black Pieces: 9..14 -> 6..11
        int w_type = (p > 8) ? (p - 3) : (p - 1);
        int p_flipped = p ^ 8;
        int b_type = (p_flipped > 8) ? (p_flipped - 3) : (p_flipped - 1);

        int idx_w = 64 * w_type + s;
        int idx_b = 64 * b_type + (s ^ 56); // Flip color and square

        // Add Weights
        for (int i = 0; i < HIDDEN1; ++i) {
            acc.white[i] += feature_weights[i * INPUT_SIZE + idx_w];
            acc.black[i] += feature_weights[i * INPUT_SIZE + idx_b];
        }
    }
}

int Network::evaluate(const Position& pos, const Accumulator& acc) {
    int32_t hidden1_out[HIDDEN1 * 2];

    // Apply CReLU and Concatenate
    // Perspective logic: Side to move comes first?
    const auto& us = (pos.side_to_move() == WHITE) ? acc.white : acc.black;
    const auto& them = (pos.side_to_move() == WHITE) ? acc.black : acc.white;

    for (int i = 0; i < HIDDEN1; ++i) {
        hidden1_out[i] = crelu(us[i]);
        hidden1_out[HIDDEN1 + i] = crelu(them[i]);
    }

    // Layer 1
    int32_t l1_out[32];
    int16_t l1_in[HIDDEN1 * 2];
    // Quantize/Pack?
    // Standard NNUE clips to 127 or similar?
    // Let's assume int16 input for LinearLayer.
    // CReLU output is int32 (0..255). We cast to int16.
    for(int i=0; i<HIDDEN1*2; ++i) l1_in[i] = (int16_t)hidden1_out[i];

    layer1.forward_from_16(l1_in, l1_out);

    // Layer 2
    int32_t l2_out[32];
    int16_t l2_in[32];
    for(int i=0; i<32; ++i) l2_in[i] = crelu(l1_out[i]); // Apply Activation

    layer2.forward_from_16(l2_in, l2_out);

    // Output
    int32_t final_out[1];
    int16_t final_in[32];
    for(int i=0; i<32; ++i) final_in[i] = crelu(l2_out[i]);

    output.forward_from_16(final_in, final_out);

    // Scale output (usually result is centipawns or similar)
    // Depends on training. Assume 1 output unit ~ cp.
    return final_out[0] / 16; // De-scale
}

} // namespace Oxta::NNUE
