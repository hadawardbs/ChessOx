import chess
import chess.engine
import sys
import json
import random
import os
import subprocess
import time

TITAN_CMD = "python3 titan_brain.py"
RESULTS_FILE = "coliseum_results.json"

def run_match(id1, id2, games=2):
    print(f"🏟️ COLISEUM MATCH: {id1} vs {id2} ({games} games) 🏟️")

    score_1 = 0
    score_2 = 0

    cmd1 = f"python3 titan_brain.py --identity {id1} --headless"
    cmd2 = f"python3 titan_brain.py --identity {id2} --headless"

    # Use STARTPOS only for simplicity/robustness in this version
    openings = [
        chess.STARTING_FEN,
        # "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1"
        # Removed custom FENs to avoid 'position startpos' logic bug without full FEN handling implementation
    ]

    for i in range(games):
        fen = openings[0] # Always startpos
        board = chess.Board(fen)

        white_id = id1 if i % 2 == 0 else id2
        black_id = id2 if i % 2 == 0 else id1

        white_cmd = cmd1 if white_id == id1 else cmd2
        black_cmd = cmd2 if white_id == id1 else cmd1

        print(f"Game {i+1}: {white_id} (White) vs {black_id} (Black)")

        try:
            p_white = subprocess.Popen(white_cmd.split(), stdin=subprocess.PIPE, stdout=subprocess.PIPE, universal_newlines=True, bufsize=1)
            p_black = subprocess.Popen(black_cmd.split(), stdin=subprocess.PIPE, stdout=subprocess.PIPE, universal_newlines=True, bufsize=1)

            for p in [p_white, p_black]:
                p.stdin.write("uci\n")
                p.stdin.flush()
                while True:
                    line = p.stdout.readline()
                    if not line or "uciok" in line: break

                p.stdin.write("isready\n")
                p.stdin.flush()
                while True:
                    line = p.stdout.readline()
                    if not line or "readyok" in line: break

            while not board.is_game_over():
                active = p_white if board.turn == chess.WHITE else p_black

                moves_str = " ".join([m.uci() for m in board.move_stack])
                if moves_str:
                    active.stdin.write(f"position startpos moves {moves_str}\n")
                else:
                    active.stdin.write(f"position startpos\n")

                active.stdin.write("go movetime 100\n")
                active.stdin.flush()

                best_move = None
                while True:
                    line = active.stdout.readline()
                    if not line: break
                    if line.startswith("bestmove"):
                        parts = line.split()
                        if len(parts) > 1:
                            best_move = parts[1]
                        break

                if not best_move or best_move == "(none)" or best_move == "0000":
                    print("Resignation/Crash")
                    break

                move = chess.Move.from_uci(best_move)
                if move in board.legal_moves:
                    board.push(move)
                else:
                    print(f"Illegal move: {best_move}")
                    break

            res = board.result()
            print(f"Result: {res}")

            p_white.terminate()
            p_black.terminate()

            if res == "1-0":
                if white_id == id1: score_1 += 1
                else: score_2 += 1
            elif res == "0-1":
                if white_id == id2: score_1 += 1
                else: score_2 += 1
            else:
                score_1 += 0.5
                score_2 += 0.5

        except Exception as e:
            print(f"Match Error: {e}")
            try:
                p_white.terminate()
                p_black.terminate()
            except: pass

    print(f"Final: {id1} {score_1} - {score_2} {id2}")

    log_entry = {
        "p1": id1, "p2": id2, "score1": score_1, "score2": score_2,
        "timestamp": time.time()
    }

    history = []
    if os.path.exists(RESULTS_FILE):
        with open(RESULTS_FILE) as f:
            try: history = json.load(f)
            except: pass
    history.append(log_entry)
    with open(RESULTS_FILE, "w") as f:
        json.dump(history, f)

if __name__ == "__main__":
    identities = ["TORTURER", "TECHNICAL", "SUFFOCATOR"]
    p1 = random.choice(identities)
    p2 = random.choice([i for i in identities if i != p1])
    run_match(p1, p2, games=2)
