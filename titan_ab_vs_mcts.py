import chess
import time
import subprocess
import os
import sys
from titan_brain import HyperMCTS_Titan, find_engine

ENGINE_PATH = find_engine()

def play_game(fen, white_engine, black_engine, time_limit):
    board = chess.Board(fen)
    moves = []

    # Reset engines?
    # AB engine needs "ucinewgame"?
    # MCTS creates new tree every search.

    while not board.is_game_over(claim_draw=True):
        stm = board.turn

        if stm == chess.WHITE:
            engine = white_engine
            name = "White"
        else:
            engine = black_engine
            name = "Black"

        start = time.time()
        if hasattr(engine, 'search'): # MCTS Wrapper
            move = engine.search(board)
        else: # Raw Subprocess (AB)
            engine.stdin.write(f"position fen {board.fen()}\n")
            engine.stdin.write(f"go movetime {int(time_limit * 1000)}\n")
            engine.stdin.flush()

            move = None
            while True:
                line = engine.stdout.readline()
                if not line: break
                if line.startswith("bestmove"):
                    move_str = line.split()[1]
                    if move_str == "(none)": move = None # Should not happen
                    else: move = chess.Move.from_uci(move_str)
                    break

        elapsed = time.time() - start

        if move is None:
            print(f"Error: {name} returned no move.")
            return "Error"

        board.push(move)
        moves.append(move.uci())
        print(f"{move.uci()} ", end="", flush=True)

        # Check excessive length
        if len(moves) > 200:
            return "1/2-1/2" # Force draw

    return board.result(claim_draw=True)

def run_suite():
    print(">>> SCIENTIFIC MATCH: AB (OxtaCore) vs MCTS (HyperMCTS) <<<")
    print(f"Binary: {ENGINE_PATH}")

    # Init AB Engine
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
    ab_proc.stdin.flush()
    # Wait for readyok
    while True:
        if "readyok" in ab_proc.stdout.readline(): break

    # Init MCTS
    mcts = HyperMCTS_Titan(time_limit=1.0, logging=False)

    # Load Positions
    fens = []
    try:
        with open("tests/positions.epd") as f:
            for line in f:
                if line.strip(): fens.append(line.strip())
    except:
        fens = [chess.STARTING_FEN]

    print(f"Loaded {len(fens)} positions.")

    score_ab = 0
    score_mcts = 0

    for i, fen in enumerate(fens):
        print(f"\nPosition {i+1}:")

        # Game 1: AB White, MCTS Black
        res = play_game(fen, ab_proc, mcts, 1.0)
        print(f"  Game 1 (AB vs MCTS): {res}")
        if res == "1-0": score_ab += 1
        elif res == "0-1": score_mcts += 1
        else:
            score_ab += 0.5
            score_mcts += 0.5

        # Game 2: MCTS White, AB Black
        res = play_game(fen, mcts, ab_proc, 1.0)
        print(f"  Game 2 (MCTS vs AB): {res}")
        if res == "1-0": score_mcts += 1
        elif res == "0-1": score_ab += 1
        else:
            score_ab += 0.5
            score_mcts += 0.5

    print("\n>>> FINAL SCORE <<<")
    print(f"AB (OxtaCore): {score_ab}")
    print(f"MCTS (Titan):  {score_mcts}")

    mcts.close()
    ab_proc.terminate()

if __name__ == "__main__":
    run_suite()
