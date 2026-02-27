import chess
import time
from titan_brain import HyperMCTS_Titan

def run():
    # Two engines
    white = HyperMCTS_Titan(time_limit=0.1, cores=1) # Fast
    black = HyperMCTS_Titan(time_limit=0.1, cores=1)

    board = chess.Board()
    while not board.is_game_over():
        print(f"\n{board}")
        engine = white if board.turn == chess.WHITE else black
        move = engine.search(board)
        if not move:
            print("Resign")
            break
        board.push(move)
        print(f"Move: {move.uci()}")

    print("Game Over:", board.result())
    white.close()
    black.close()

if __name__ == "__main__":
    run()
