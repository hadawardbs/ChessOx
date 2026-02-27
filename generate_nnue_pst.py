import struct
import numpy as np

# NNUE Architecture matches src/nnue/layers.hpp
# 768 -> 256 (Feature Transformer)
# 512 -> 32  (Layer 1)
# 32 -> 32   (Layer 2)
# 32 -> 1    (Output)

HIDDEN1 = 256
INPUT_SIZE = 768

feature_biases = np.zeros(HIDDEN1, dtype=np.int16)
feature_weights = np.zeros((HIDDEN1, INPUT_SIZE), dtype=np.int16)

layer1_biases = np.zeros(32, dtype=np.int16)
layer1_weights = np.zeros((32, HIDDEN1 * 2), dtype=np.int16)

layer2_biases = np.zeros(32, dtype=np.int16)
layer2_weights = np.zeros((32, 32), dtype=np.int16)

output_biases = np.zeros(1, dtype=np.int16)
output_weights = np.zeros((1, 32), dtype=np.int16)

# Piece mappings: P=1..K=6. Enemy P=9..K=14
# We use node 0 for Friendly Material, node 1 for Enemy Material
piece_vals = {
    0: 100,  # P
    1: 300,  # N
    2: 300,  # B
    3: 500,  # R
    4: 900,  # Q
    5: 0     # K
}

for p in range(6):
    val = piece_vals[p] // 16
    for s in range(64):
        # Center bonus for N and P
        bonus = 0
        if p == 0 or p == 1:
            r, c = s // 8, s % 8
            dist = abs(r - 3.5) + abs(c - 3.5)
            bonus = int(7 - dist)
        
        # Friendly
        idx_friendly = 64 * p + s
        feature_weights[0, idx_friendly] = val + bonus
        
        # Enemy mapping in array
        idx_enemy = 64 * (p + 6) + s
        feature_weights[1, idx_enemy] = val + bonus

# Layer 1: Pass through to nodes 0 and 1
layer1_weights[0, 0] = 1 # us[0]: Friendly Mat
layer1_weights[1, 1] = 1 # us[1]: Enemy Mat

# Layer 2: Pass through
layer2_weights[0, 0] = 1
layer2_weights[1, 1] = 1

# Output: Output = Friendly - Enemy. Scaled by 256 (since we divided by 16, and C++ divides by 16 again. 16*16 = 256)
# Wait, C++ output is final_out[0]/16.
# We want final_out[0] to be CP * 16.
# Currently L2 out is CP / 16. So we need to multiply by 16 * 16 = 256.
output_weights[0, 0] = 256
output_weights[0, 1] = -256

# Write to nnue.bin directly
with open("nnue.bin", "wb") as f:
    f.write(feature_biases.tobytes())
    f.write(feature_weights.tobytes())
    
    f.write(layer1_biases.tobytes())
    f.write(layer1_weights.tobytes())
    
    f.write(layer2_biases.tobytes())
    f.write(layer2_weights.tobytes())
    
    f.write(output_biases.tobytes())
    f.write(output_weights.tobytes())

print("NNUE generated successfully.")
