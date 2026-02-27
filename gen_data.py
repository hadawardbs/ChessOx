import chess
import chess.engine
import chess.pgn
import random
import os
import sys

# Data Generation for Texel Tuning
# Generates random positions and asks Stockfish for evaluation.

# Configuration
STOCKFISH_PATH = "stockfish" # Assumes stockfish is in PATH or current dir.
# If not in path, try to find it.
if os.path.exists("./stockfish"): STOCKFISH_PATH = "./stockfish"
elif os.path.exists("stockfish.exe"): STOCKFISH_PATH = "stockfish.exe"

OUTPUT_FILE = "tuning_data.epd"
POSITIONS_TO_GEN = 200 # Small batch for session demo. User should run 100k+.

def get_random_positions(num):
    positions = []
    board = chess.Board()

    while len(positions) < num:
        game_len = random.randint(10, 60)
        board.reset()

        # Play random moves
        for _ in range(game_len):
            if board.is_game_over(): break
            moves = list(board.legal_moves)
            if not moves: break
            board.push(random.choice(moves))

        if not board.is_game_over():
            # Only keep quiescent positions?
            # For HCE tuning, we want mix.
            # Avoid checkmate positions in dataset (eval is infinite)
            positions.append(board.fen())

    return positions

def evaluate_with_stockfish(fens):
    data = []
    try:
        # Limit threads to 1 for stability
        engine = chess.engine.SimpleEngine.popen_uci(STOCKFISH_PATH)
    except Exception as e:
        print(f"Error starting Stockfish: {e}")
        print("Please ensure Stockfish is installed/downloaded.")
        return []

    print(f"Evaluating {len(fens)} positions...")
    for i, fen in enumerate(fens):
        board = chess.Board(fen)
        try:
            info = engine.analyse(board, chess.engine.Limit(depth=8)) # Depth 8 is fast enough for basic HCE guidance
            score = info["score"].white()

            if score.is_mate():
                continue # Skip mates

            cp = score.score()

            # Filter huge scores (won games) to focus tuning on balanced positions
            if abs(cp) > 1000: continue

            # Format: FEN; Score; Result?
            # Texel format: FEN "result" or "score"
            # We will save: FEN ; score_cp
            data.append(f"{fen} ; {cp}")

            if i % 10 == 0:
                print(f"\rProgress: {i}/{len(fens)}", end="")

        except Exception as e:
            pass

    engine.quit()
    print("\nDone.")
    return data

if __name__ == "__main__":
    if len(sys.argv) > 1:
        count = int(sys.argv[1])
    else:
        count = POSITIONS_TO_GEN

    print(f"Generating {count} positions...")
    fens = get_random_positions(count * 2) # Gen extra to account for filtering

    # Check if we can run SF
    # For this environment, we might not have SF.
    # If no SF, we generate dummy data for the pipeline validation.
    has_sf = False
    try:
        p = subprocess.Popen([STOCKFISH_PATH, "uci"], stdout=subprocess.PIPE, stdin=subprocess.PIPE)
        p.terminate()
        has_sf = True
    except:
        pass

    if has_sf:
        data = evaluate_with_stockfish(fens[:count])
    else:
        print("Stockfish not found. Generating DUMMY data for pipeline test.")
        data = []
        for f in fens[:count]:
            # Fake score based on material to test the tuner logic
            b = chess.Board(f)
            # Simple heuristic
            score = 0
            score += len(b.pieces(chess.PAWN, chess.WHITE)) * 100
            score -= len(b.pieces(chess.PAWN, chess.BLACK)) * 100
            data.append(f"{f} ; {score}")

    with open(OUTPUT_FILE, "w") as f:
        for line in data:
            f.write(line + "\n")

    print(f"Saved {len(data)} positions to {OUTPUT_FILE}")
