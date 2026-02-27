import os
import sys
import struct
import math
import subprocess
import random

# TitanTrainer: Texel Tuning Implementation
# Tunes weights.bin using local search (Hill Climbing / SPSA-like) against a dataset.

ENGINE_PATH = "./build/OxtaCore"
DATA_FILE = "tuning_data.epd"
WEIGHTS_FILE = "weights.bin"

# Tuning Config
K_SCALE = 1.0 # Sigmoid scale. Usually ~ 1.5 - 2.0 for CP
LEARNING_RATE = 10
ITERATIONS = 50 # Tiny for demo. Real: 1000+

# Weights Structure Layout (Must match C++ EvalWeights struct)
# int Material[7] (0, P, N, B, R, Q, K) -> 7 ints
# int PassedPawn[8] -> 8 ints
# int Mobility[7] -> 7 ints
# PSTs: 64 ints each.
# We need to know the EXACT binary layout.
# Assuming standard 32-bit int (4 bytes).

# Let's define the layout map
LAYOUT = [
    ("Material", 7),
    ("PassedPawn", 8),
    ("Mobility", 7),
    ("PawnPST_MG", 64),
    ("PawnPST_EG", 64),
    ("KnightPST_MG", 64),
    # Add others if struct has them
]

def load_data():
    data = []
    if not os.path.exists(DATA_FILE): return []
    with open(DATA_FILE) as f:
        for line in f:
            parts = line.strip().split(';')
            if len(parts) >= 2:
                fen = parts[0].strip()
                score = int(float(parts[1].strip()))
                data.append((fen, score))
    return data

def get_engine_eval(engine_proc, fen):
    # Send position, get eval
    engine_proc.stdin.write(f"position fen {fen}\n")
    engine_proc.stdin.write("eval\n")
    engine_proc.stdin.flush()

    while True:
        line = engine_proc.stdout.readline()
        if not line: break
        line = line.strip()
        if line.startswith("score cp"):
            return int(line.split()[2])
    return 0

def sigmoid(s):
    return 1.0 / (1.0 + math.exp(-K_SCALE * s / 400.0))

def compute_error(data, engine_proc):
    total_error = 0.0
    count = 0
    for fen, target_score in data:
        eval_score = get_engine_eval(engine_proc, fen)

        # Convert both to win probability?
        # Texel usually minimizes (Result - Sigmoid(Score))^2
        # Here we have Stockfish Score (CP).
        # So we minimize (Sigmoid(SF) - Sigmoid(Oxta))^2

        p_target = sigmoid(target_score)
        p_eval = sigmoid(eval_score)

        err = (p_target - p_eval) ** 2
        total_error += err
        count += 1

    return total_error / count if count > 0 else 0

def read_weights():
    # If file doesn't exist, we can't tune specific values easily without knowing defaults.
    # But C++ struct has defaults.
    # We will let C++ write the defaults first?
    # Or we construct a buffer of 0s and hope?
    # Better: We modify main.cpp to DUMP weights if command is given.
    # For now, let's assume we start from scratch or existing binary.
    if os.path.exists(WEIGHTS_FILE):
        with open(WEIGHTS_FILE, "rb") as f:
            return bytearray(f.read())
    else:
        # Create buffer based on layout size
        total_ints = sum(cnt for _, cnt in LAYOUT)
        return bytearray(total_ints * 4)

def write_weights(buffer):
    with open(WEIGHTS_FILE, "wb") as f:
        f.write(buffer)

def tune():
    print("Starting TitanTrainer (Texel Tuning)...")
    data = load_data()
    if not data:
        print("No data found.")
        return

    # Start Engine
    engine = subprocess.Popen([ENGINE_PATH], stdin=subprocess.PIPE, stdout=subprocess.PIPE, universal_newlines=True, bufsize=1)

    # Initial Error
    # We need to make sure engine reloads weights.
    # Actually, OxtaCore loads weights on startup.
    # So we must restart engine every time we change weights? Yes, for this simple implementation.
    # That is SLOW.
    # Optimized way: 'setoption name Tuner value ...'
    # But user wants "Calibrate NOW".
    # We will do a few iterations restarting engine.

    engine.terminate()

    weights = read_weights()

    # Simple Hill Climbing / Random Perturbation
    best_error = 1.0 # Init high

    # Baseline
    write_weights(weights)
    p = subprocess.Popen([ENGINE_PATH], stdin=subprocess.PIPE, stdout=subprocess.PIPE, universal_newlines=True, bufsize=1)
    best_error = compute_error(data, p)
    p.terminate()
    print(f"Baseline MSE: {best_error:.6f}")

    for i in range(ITERATIONS):
        # Mutate
        candidate = bytearray(weights)

        # Pick a random parameter index
        total_ints = len(candidate) // 4
        idx = random.randint(0, total_ints - 1)

        # Read int
        val = struct.unpack_from("i", candidate, idx * 4)[0]

        # Perturb
        delta = random.choice([-10, 10, -20, 20])
        val += delta

        # Write back
        struct.pack_into("i", candidate, idx * 4, val)

        # Save
        write_weights(candidate)

        # Test
        p = subprocess.Popen([ENGINE_PATH], stdin=subprocess.PIPE, stdout=subprocess.PIPE, universal_newlines=True, bufsize=1)
        err = compute_error(data, p)
        p.terminate()

        if err < best_error:
            print(f"Iter {i}: Improved! {best_error:.6f} -> {err:.6f}")
            best_error = err
            weights = candidate # Keep mutation
        else:
            # Revert (just don't update weights)
            pass

    # Final save
    write_weights(weights)
    print("Tuning Complete.")

if __name__ == "__main__":
    tune()
