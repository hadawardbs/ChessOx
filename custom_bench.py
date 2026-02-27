import time
import chess
import subprocess
from titan_brain import HyperMCTS_Titan, find_engine

with open('debug.txt', 'w') as f:
    f.write('Starting custom bench...\n')
    f.flush()
    try:
        mcts = HyperMCTS_Titan(headless=True, time_limit=1.0)
        f.write('MCTS engine intialized.\n')
        f.flush()
        board = chess.Board()
        f.write('Starting search on starting FEN.\n')
        f.flush()
        move = mcts.search(board)
        f.write(f'Search complete. Move: {move.uci()}\n')
    except Exception as e:
        f.write(f'Error: {e}\n')
