import json
import random
import os
import subprocess
import chess
from titan_brain import HyperMCTS_Titan, find_engine

# Tuning Parameters
PARAMS = {
    "cpuct": 1.5,
    "td_alpha": 0.1
}

# SPSA Config
ALPHA = 0.602
GAMMA = 0.101
A = 100
C = 0.1
ITERATIONS = 50

ENGINE_PATH = find_engine()

def get_fitness(params):
    # Write params
    with open("brain_params.json", "w") as f:
        json.dump(params, f)

    # Play 2 games vs Fixed AB (OxtaCore depth 4)
    # Using low depth AB as gatekeeper

    ab_proc = subprocess.Popen(
        [ENGINE_PATH],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1
    )
    ab_proc.stdin.write("uci\n")
    ab_proc.stdin.write("isready\n")
    while True:
        if "readyok" in ab_proc.stdout.readline(): break

    mcts = HyperMCTS_Titan(time_limit=0.5, logging=False) # Fast games

    score = 0
    # Random position from suite
    fens = [chess.STARTING_FEN]
    if os.path.exists("tests/positions.epd"):
        with open("tests/positions.epd") as f:
            fens = [l.strip() for l in f if l.strip()]

    fen = random.choice(fens)

    # Game 1: MCTS White
    res = play_one(fen, mcts, ab_proc)
    if res == "1-0": score += 1
    elif res == "1/2-1/2": score += 0.5

    # Game 2: MCTS Black
    res = play_one(fen, ab_proc, mcts) # returns result from White perspective
    if res == "0-1": score += 1
    elif res == "1/2-1/2": score += 0.5

    mcts.close()
    ab_proc.terminate()

    return score / 2.0 # 0.0 to 1.0

def play_one(fen, white, black):
    board = chess.Board(fen)
    moves = 0
    while not board.is_game_over(claim_draw=True) and moves < 100:
        if board.turn == chess.WHITE: engine = white
        else: engine = black

        if hasattr(engine, 'search'):
            move = engine.search(board)
        else:
            engine.stdin.write(f"position fen {board.fen()}\n")
            engine.stdin.write("go depth 4\n")
            engine.stdin.flush()
            move = None
            while True:
                line = engine.stdout.readline()
                if line.startswith("bestmove"):
                    move = chess.Move.from_uci(line.split()[1])
                    break

        board.push(move)
        moves += 1

    return board.result(claim_draw=True)

def tune():
    theta = list(PARAMS.values())
    keys = list(PARAMS.keys())

    print(f"Starting SPSA. Initial: {PARAMS}")

    for k in range(ITERATIONS):
        ck = C / (k + 1)**GAMMA
        ak = A / (k + 1 + A)**ALPHA

        delta = [random.choice([-1, 1]) for _ in theta]

        # Plus
        theta_plus = [t + ck*d for t, d in zip(theta, delta)]
        p_plus = dict(zip(keys, theta_plus))
        y_plus = get_fitness(p_plus)

        # Minus
        theta_minus = [t - ck*d for t, d in zip(theta, delta)]
        p_minus = dict(zip(keys, theta_minus))
        y_minus = get_fitness(p_minus)

        # Update
        ghat = [(y_plus - y_minus) / (2 * ck * d) for d in delta]
        theta = [t + ak * g for t, g in zip(theta, ghat)] # Gradient Ascent (maximize score)

        print(f"Iter {k}: Score {y_plus} vs {y_minus} -> New Theta: {theta}")

    print(f"Final Params: {dict(zip(keys, theta))}")

if __name__ == "__main__":
    tune()
