import chess
import chess.engine
import sys
import time
import subprocess
import os
import random

# Gating Protocol Script
# Runs a match between a "Challenger" (New) and "Champion" (Old/Stockfish)
# Usage: python titan_gating.py --challenger ./build/OxtaCore --champion stockfish

def find_engine():
    # Helper to find OxtaCore if not specified
    base = os.path.dirname(os.path.abspath(__file__))
    paths = [
        os.path.join(base, "build", "OxtaCore"),
        os.path.join(base, "build", "Release", "OxtaCore.exe"),
    ]
    for p in paths:
        if os.path.exists(p): return p
    return "stockfish" # Fallback

def play_match(challenger_cmd, champion_cmd, games=10, time_limit=1.0):
    print(f"--- OXTA GATING PROTOCOL ---")
    print(f"Challenger: {challenger_cmd}")
    print(f"Champion:   {champion_cmd}")
    print(f"Time Control: {time_limit}s/move")
    print(f"Games: {games}")
    print(f"----------------------------")

    score_challenger = 0
    score_champion = 0

    # Simple opening suite (FENs)
    openings = [
        chess.STARTING_FEN,
        "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1", # e4
        "rnbqkbnr/pppppppp/8/8/3P4/8/PPP1PPPP/RNBQKBNR b KQkq - 0 1", # d4
        "rnbqkbnr/pppppppp/8/8/2P5/8/PP1PPPPP/RNBQKBNR b KQkq - 0 1", # c4
        "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1", # Sicilian
    ]

    for i in range(games):
        fen = openings[i % len(openings)]
        board = chess.Board(fen)

        # Swap colors
        white_is_challenger = (i % 2 == 0)

        white_engine = challenger_cmd if white_is_challenger else champion_cmd
        black_engine = champion_cmd if white_is_challenger else challenger_cmd

        print(f"Game {i+1}: {'Challenger' if white_is_challenger else 'Champion'} (White) vs {'Champion' if white_is_challenger else 'Challenger'} (Black)")

        # Start engines
        try:
            p_white = subprocess.Popen(white_engine.split(), stdin=subprocess.PIPE, stdout=subprocess.PIPE, universal_newlines=True, bufsize=1)
            p_black = subprocess.Popen(black_engine.split(), stdin=subprocess.PIPE, stdout=subprocess.PIPE, universal_newlines=True, bufsize=1)
        except Exception as e:
            print(f"Error starting engines: {e}")
            return

        def send(proc, cmd):
            proc.stdin.write(cmd + "\n")
            proc.stdin.flush()

        def get_move(proc, board_fen, limit):
            send(proc, "uci") # Wake up / check protocol
            # We assume UCI compliance. TitanBrain might need a wrapper if tested here directly,
            # but usually we test the Engine binary (OxtaCore).
            # If testing TitanBrain.py, the command would be "python titan_brain.py" (needs UCI wrapper).

            send(proc, f"position fen {board_fen}")
            send(proc, f"go movetime {int(limit*1000)}")

            while True:
                line = proc.stdout.readline()
                if not line: return None
                if line.startswith("bestmove"):
                    return line.split()[1]

        # Initialize
        send(p_white, "isready")
        send(p_black, "isready")
        # In a real script, wait for readyok.
        # For simplicity/Oxta compliance, we sleep briefly or assume fast startup.
        time.sleep(0.1)

        while not board.is_game_over():
            proc = p_white if board.turn == chess.WHITE else p_black

            try:
                move_uci = get_move(proc, board.fen(), time_limit)
                if not move_uci:
                    print("Engine crashed or closed stream.")
                    break

                move = chess.Move.from_uci(move_uci)
                if move in board.legal_moves:
                    board.push(move)
                    # print(f"{board.peek()} ", end="", flush=True)
                else:
                    print(f"Illegal move {move_uci}")
                    break
            except Exception as e:
                print(f"Game Loop Error: {e}")
                break

        # Result
        res = board.result()
        print(f"Result: {res}")

        p_white.terminate()
        p_black.terminate()

        if res == "1-0":
            if white_is_challenger: score_challenger += 1
            else: score_champion += 1
        elif res == "0-1":
            if white_is_challenger: score_champion += 1
            else: score_challenger += 1
        else:
            score_challenger += 0.5
            score_champion += 0.5

    print(f"----------------------------")
    print(f"Final Score: Challenger {score_challenger} - {score_champion} Champion")
    if score_challenger >= score_champion:
        print("VERDICT: PASS (Candidate Accepted)")
        sys.exit(0)
    else:
        print("VERDICT: FAIL (Candidate Rejected)")
        sys.exit(1)

if __name__ == "__main__":
    eng = find_engine()
    # Default: Play against itself (Sanity Check) or Stockfish if available
    # For now, we mock the champion as the same engine to verify stability (50% score expected)
    # or use a trivial "random" player if no stockfish.

    # In Oxta CI, we would have a specific binary.
    # Here we default to playing Oxta vs Oxta to ensure it runs without crashing.

    play_match(eng, eng, games=2, time_limit=0.1)
