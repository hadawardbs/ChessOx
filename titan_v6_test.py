import chess
import time
import os
from titan_brain import HyperMCTS_Titan

def run_test():
    print(">>> Oxta V6 Sentience Test (VOC & SW-PUCB) <<<")
    # Clean logs
    if os.path.exists("search_metrics.csv"):
        os.remove("search_metrics.csv")

    mcts = HyperMCTS_Titan(time_limit=5.0, logging=True) # High limit to test stopping

    # Load positions
    fens = []
    try:
        with open("tests/positions.epd") as f:
            fens = [l.strip() for l in f if l.strip()]
    except:
        fens = [chess.STARTING_FEN]

    total_time = 0
    total_moves = 0

    for i, fen in enumerate(fens):
        board = chess.Board(fen)
        print(f"\nPosition {i+1}: {fen}")

        start = time.time()
        move = mcts.search(board)
        elapsed = time.time() - start

        print(f"Move: {move} | Time: {elapsed:.3f}s")
        total_time += elapsed
        total_moves += 1

    print(f"\nAverage Time: {total_time/total_moves:.3f}s")
    mcts.close()

if __name__ == "__main__":
    run_test()
