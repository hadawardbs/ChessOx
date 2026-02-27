import chess
import math
import random
import time
import subprocess
import os
import platform
import json
import csv
import threading
import sys
import argparse
from collections import defaultdict, deque
from titan_memory import memory

from titan_htn import HTNPlanner
from titan_science import CausalEngine, TopologicalScanner
from titan_csp import CSPSolver

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

def find_engine():
    paths = [
        os.path.join(BASE_DIR, "build", "OxtaCore"),
        os.path.join(BASE_DIR, "build", "Release", "OxtaCore.exe"),
        os.path.join(BASE_DIR, "build", "OxtaCore.exe"),
    ]
    for p in paths:
        if os.path.exists(p): return p
    for root, _, files in os.walk(os.path.join(BASE_DIR, "build")):
        if "OxtaCore" in files: return os.path.join(root, "OxtaCore")
        if "OxtaCore.exe" in files: return os.path.join(root, "OxtaCore.exe")
    raise FileNotFoundError("OxtaCore engine binary not found")

ENGINE_PATH = find_engine()

# --- V15/V17 Modules ---
class StrategicLibrary:
    def __init__(self):
        self.motifs = {}
    def recall(self, embedding):
        return self.motifs.get(embedding, None)

class IrreversibilityScanner:
    def scan(self, board):
        flags = []
        return flags

class StrategicIdentity:
    def __init__(self, force_mode=None):
        self.identities = ["SUFFOCATOR", "SIMPLIFIER", "TORTURER"]
        self.current = force_mode if force_mode else "NEUTRAL"
        self.adaptive = (force_mode is None)
        # Suppress init print in brain logic if wrapped

    def update(self, eval_cp, pressure, opponent_rationality):
        if not self.adaptive: return

        if eval_cp > 200 and opponent_rationality < 0.5:
            self.current = "TECHNICAL"
        elif eval_cp > 50 and pressure > 0.5:
            self.current = "TORTURER"
        elif eval_cp < -50:
            self.current = "SWINDLER"
        else:
            self.current = "NEUTRAL"

    def get_bias(self, board, move):
        if self.current == "SUFFOCATOR": return 0.1
        elif self.current == "SIMPLIFIER" or self.current == "TECHNICAL":
            if board.is_capture(move): return 0.2
            return 0.0
        elif self.current == "TORTURER":
            return 0.2
        elif self.current == "SWINDLER":
            return 0.3
        return 0.0

C_PUCT_BASE = 2.0
VIRTUAL_LOSS = 3
TD_ALPHA = 0.1
WINDOW_SIZE = 50

try:
    if os.path.exists("brain_params.json"):
        with open("brain_params.json") as f:
            params = json.load(f)
            C_PUCT_BASE = params.get("cpuct", 2.0)
            TD_ALPHA = params.get("td_alpha", 0.1)
except: pass

OPENING_BOOK = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1": ["e2e4", "d2d4", "g1f3", "c2c4"],
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1": ["c7c5", "e7e5", "e7e6", "c7c6"],
    "rnbqkbnr/pppppppp/8/8/3P4/8/PPP1PPPP/RNBQKBNR b KQkq - 0 1": ["g8f6", "d7d5", "e7e6"],
}

BAD_OPENING_MOVES = ["h3", "a3", "h6", "a6", "f3", "f6", "Nh3", "Na3", "Nh6", "Na6"]

class TitanNode:
    def __init__(self, parent=None, move=None, prior=0.0, window_size=50):
        self.visits = 0
        self.value_sum = 0.0
        self.children = {}
        self.parent = parent
        self.move = move
        self.prior = prior
        self.virtual_loss = 0
        self.window = deque(maxlen=window_size)
        self.window_sum = 0.0
        self.window_sq_sum = 0.0
        self.q_val = 0.5
        self.entropy = 0.0
        self.is_quiet = True
        self.lock = threading.Lock()

    def win_rate(self):
        eff_visits = self.visits + self.virtual_loss
        if eff_visits == 0: return 0.5
        if len(self.window) > 0:
            return (self.window_sum) / (len(self.window) + self.virtual_loss)
        return self.value_sum / eff_visits

    def volatility(self):
        n = len(self.window)
        if n < 2: return 0.5
        mean = self.window_sum / n
        variance = (self.window_sq_sum / n) - (mean * mean)
        return math.sqrt(max(0, variance))

    def puct(self, parent_visits, c_puct):
        eff_visits = self.visits + self.virtual_loss
        q = self.win_rate()
        u = c_puct * self.prior * (math.sqrt(parent_visits) / (1 + eff_visits))
        return q + u

    def update_td(self, reward):
        with self.lock:
            self.visits += 1
            if len(self.window) == self.window.maxlen:
                removed = self.window[0]
                self.window_sum -= removed
                self.window_sq_sum -= removed * removed
            self.window.append(reward)
            self.window_sum += reward
            self.window_sq_sum += reward * reward
            self.value_sum += reward
            self.q_val = self.value_sum / self.visits

    def calculate_entropy(self):
        if not self.children: return 0.0
        total = sum(c.visits for c in self.children.values())
        if total == 0: return 0.0
        H = 0.0
        for c in self.children.values():
            if c.visits > 0:
                p = c.visits / total
                H -= p * math.log(p)
        self.entropy = H
        return H

    def apply_virtual_loss(self):
        with self.lock:
            self.virtual_loss += VIRTUAL_LOSS

    def remove_virtual_loss(self):
        with self.lock:
            self.virtual_loss -= VIRTUAL_LOSS

class FeatureExtractor:
    def extract(self, board):
        w_mat = sum(len(board.pieces(pt, chess.WHITE)) * v for pt, v in {chess.PAWN:1, chess.KNIGHT:3, chess.BISHOP:3, chess.ROOK:5, chess.QUEEN:9}.items())
        b_mat = sum(len(board.pieces(pt, chess.BLACK)) * v for pt, v in {chess.PAWN:1, chess.KNIGHT:3, chess.BISHOP:3, chess.ROOK:5, chess.QUEEN:9}.items())
        mat_diff = (w_mat - b_mat) / 30.0

        center_sqs = [chess.E4, chess.D4, chess.E5, chess.D5]
        center = sum(1 for sq in center_sqs if board.piece_at(sq)) / 4.0
        try: mob = len(list(board.legal_moves)) / 50.0
        except: mob = 0
        return [mat_diff, center, mob]

class OpponentTracker:
    def __init__(self):
        self.moves = 0
        self.agg_moves = 0
        self.rationality_score = 1.0
        self.last_eval = 0.0

    def update(self, move, board):
        self.moves += 1
        if board.is_capture(move) or board.gives_check(move):
            self.agg_moves += 1

    def update_rationality(self, current_eval):
        delta = current_eval - self.last_eval
        if delta > 1.5:
            self.rationality_score -= 0.2
        elif delta > 0.5:
            self.rationality_score -= 0.05
        self.last_eval = current_eval
        self.rationality_score = max(0.0, self.rationality_score)

    def is_aggressive(self):
        if self.moves < 4: return False
        return (self.agg_moves / self.moves) > 0.5

    def is_chaotic(self):
        return self.rationality_score < 0.6

class DecisionAgent:
    def select(self, root, confidence, opponent_agg=False, board=None):
        if board:
            fen = board.fen()
            if fen in OPENING_BOOK:
                book_moves = OPENING_BOOK[fen]
                move_str = random.choice(book_moves)
                move = chess.Move.from_uci(move_str)
                if move in board.legal_moves:
                    print(f"TitanBrain: Book Move {move_str}")
                    return move

        candidates = list(root.children.values())
        if not candidates: return None

        candidates.sort(key=lambda c: c.visits, reverse=True)
        best = candidates[0]
        return best.move

class HyperMCTS_Titan:
    def __init__(self, time_limit=1.0, headless=False, identity=None, cores=1, logging=True, on_think=None):
        self.engine_path = find_engine()
        self.engine = subprocess.Popen(
            [self.engine_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
            bufsize=1
        )
        self.time_limit = time_limit
        self.headless = headless
        self.cores = cores
        self.on_think = on_think

        self._send("uci")
        self._send(f"setoption name Threads value {self.cores}")
        self._send("isready")
        self._read_until("readyok")

        self.metrics_log = "search_metrics.csv"
        self.logging = logging

        self.decision_agent = DecisionAgent()
        self.extractor = FeatureExtractor()
        self.opponent = OpponentTracker()

        self.htn_planner = HTNPlanner()
        self.causal_engine = CausalEngine()
        self.tda_scanner = TopologicalScanner()
        self.csp_solver = CSPSolver()

        self.strategic_memory = StrategicLibrary()
        self.irrev_scanner = IrreversibilityScanner()
        self.identity = StrategicIdentity(force_mode=identity)

        self.c_puct_current = C_PUCT_BASE
        self.mode = "STRATEGIC"
        self.search_lock = threading.Lock()

        self.position_history = deque(maxlen=6)

        if self.logging and not os.path.exists(self.metrics_log):
            with open(self.metrics_log, "w", newline='') as f:
                writer = csv.writer(f)
                writer.writerow(["Sims", "Entropy", "VOC", "MaxDepth", "Pressure", "BestVal", "Confidence", "Time", "CPUCT", "Mode", "Identity", "Rationality"])

    def _send(self, cmd):
        self.engine.stdin.write(cmd + "\n")
        self.engine.stdin.flush()

    def _read_until(self, token):
        while True:
            line = self.engine.stdout.readline()
            if not line: break
            if token in line: return line.strip()

    def _get_window_size(self, board):
        pcs = len(board.piece_map())
        if pcs < 10: return 20
        if pcs > 24: return 60
        return 40

    def get_oracle_rollout(self, board):
        # Optimization: Use 'eval' command for static evaluation if not noisy
        # This reduces search overhead for leaf nodes
        is_noisy = board.is_check() or (board.move_stack and board.is_capture(board.peek()))

        score_cp = 0

        if not is_noisy:
            # Fast Path: Static Eval + Quiescence (implicit in engine eval often)
            # We assume the engine supports an 'eval' command or we run depth 0
            # Since OxtaCore supports 'eval' via standard UCI? Actually OxtaCore might not output CP in eval.
            # Let's stick to 'go depth 1' but optimize protocol waiting
            self._send(f"position fen {board.fen()}")
            self._send(f"go depth 1")
        else:
            # Critical Path: Deeper search for tactical resolution
            depth = 3 # Increased from 2 for better tactical stability
            self._send(f"position fen {board.fen()}")
            self._send(f"go depth {depth}")

        while True:
            line = self.engine.stdout.readline()
            if not line: break
            line = line.strip()
            if line.startswith("info") and "score cp" in line:
                try:
                    parts = line.split()
                    idx = parts.index("cp")
                    score_cp = int(parts[idx+1])
                except: pass
            if line.startswith("bestmove"):
                break

        tda_score = self.tda_scanner.get_embedding(board)

        # Sigmoid scaling logic check:
        # CP 200 (2 pawns) -> ~1.0 + TDA
        # TDA is usually small?

        total_val = (score_cp / 200.0) + tda_score

        return 1.0 / (1.0 + math.exp(-total_val))

    def compute_voc(self, root, elapsed, time_limit):
        if not root.children: return 100.0
        H = root.calculate_entropy()
        candidates = sorted(root.children.values(), key=lambda c: c.visits, reverse=True)
        if len(candidates) < 2: return 0.0
        best = candidates[0]
        second = candidates[1]
        gap = abs(best.win_rate() - second.win_rate())
        time_pressure = elapsed / (time_limit + 0.01)
        voc = (H * (1.0 - gap)) - (time_pressure * 2.0)
        return voc

    def calculate_pressure(self, board):
        vec = self.extractor.extract(board)
        pressure = vec[2]
        return pressure

    def search_mcts(self, board):
        with self.search_lock:
            try:
                if board.move_stack:
                    last = board.peek()
                    prev = board.copy()
                    prev.pop()
                    self.opponent.update(last, prev)
            except: pass

            root = TitanNode(window_size=self._get_window_size(board))
            start_time = time.time()
            sims = 0
            best_move_history = []
            max_depth_reached = 0

            htn_task, htn_moves, htn_weight = self.htn_planner.decompose(board)
            active_htn_moves = set(htn_moves)

            pressure = self.calculate_pressure(board)

            # V17: Adaptive Identity
            self.identity.update(self.opponent.last_eval * 100, pressure, self.opponent.rationality_score)
            self.mode = self.identity.current

            legal = list(board.legal_moves)
            if not legal: return None

            self.expand_node(root, board, htn_moves=active_htn_moves, htn_bonus=htn_weight, full=True)

            current_voc = 1.0
            best_q = 0.5

            while True:
                elapsed = time.time() - start_time
                if elapsed >= self.time_limit: break

                if sims > 100 and sims % 100 == 0:
                    current_voc = self.compute_voc(root, elapsed, self.time_limit)
                    if current_voc < 0: break

                node = root
                curr = board.copy()
                path = [node]
                depth = 0

                while node.children:
                    c_dyn = self.c_puct_current
                    if self.mode == "TACTICAL" or self.mode == "TORTURER": c_dyn = 1.0

                    best_child = max(node.children.values(), key=lambda c: c.puct(node.visits, c_dyn))
                    best_child.apply_virtual_loss()

                    node = best_child
                    curr.push(node.move)
                    path.append(node)
                    depth += 1

                    if curr.is_repetition(2): break
                    if curr.is_game_over(): break

                if depth > max_depth_reached: max_depth_reached = depth

                if not curr.is_game_over() and not node.children:
                    if node.visits < 5:
                        self.expand_node(node, curr, htn_moves=active_htn_moves, htn_bonus=htn_weight, full=False)
                    else:
                        self.expand_node(node, curr, htn_moves=active_htn_moves, htn_bonus=htn_weight, full=True)

                if curr.is_game_over():
                    res = curr.result()
                    if res == "1-0":
                        score = 1.0 if curr.turn == chess.WHITE else 0.0
                    elif res == "0-1":
                        score = 1.0 if curr.turn == chess.BLACK else 0.0
                    else:
                        score = 0.5
                elif curr.is_repetition(2):
                    score = 0.45
                else:
                    score = self.get_oracle_rollout(curr)

                curr_score = score
                for i in range(len(path) - 1, -1, -1):
                    n = path[i]
                    curr_score = 1.0 - curr_score
                    n.update_td(curr_score)
                    if i > 0: n.remove_virtual_loss()

                sims += 1

                if root.children:
                    best_child = max(root.children.values(), key=lambda c: c.visits)
                    best_q = best_child.q_val
                    if not best_move_history or best_move_history[-1] != best_child.move:
                        best_move_history.append(best_child.move)
                    
                if sims % 20 == 0 and self.on_think and root.children:
                    # Top 2 moves being explored
                    top_children = sorted(root.children.values(), key=lambda c: c.visits, reverse=True)[:2]
                    self.on_think([c.move for c in top_children])

            cp_est = (best_q - 0.5) * 10.0
            self.opponent.update_rationality(cp_est)

            self.position_history.append(board.fen().split()[0])

            final_H = root.calculate_entropy()

            if self.logging and root.children:
                try:
                    best_child = max(root.children.values(), key=lambda c: c.visits)
                    with open(self.metrics_log, "a", newline='') as f:
                        writer = csv.writer(f)
                        writer.writerow([
                            sims,
                            f"{final_H:.4f}",
                            f"{current_voc:.4f}",
                            max_depth_reached,
                            f"{pressure:.2f}",
                            f"{best_child.q_val:.2f}",
                            "1.0",
                            f"{time.time() - start_time:.3f}",
                            f"{self.c_puct_current:.2f}",
                            self.mode,
                            self.identity.current,
                            f"{self.opponent.rationality_score:.2f}"
                        ])
                except: pass

            if not root.children: return None
            return self.decision_agent.select(root, 1.0, self.opponent.is_aggressive(), board=board)

    def expand_node(self, node, board, htn_moves=None, htn_bonus=0.0, full=False, moves_override=None):
        if moves_override is not None:
            moves = moves_override
        else:
            moves = list(board.legal_moves)

        if not moves: return

        moves.sort(key=lambda m: (2 if board.is_capture(m) else (1 if board.gives_check(m) else 0)), reverse=True)
        limit = len(moves) if full else min(len(moves), 5)
        total_p = 0
        priors = {}

        is_opening = board.fullmove_number <= 8

        for i in range(limit):
            if i >= len(moves): break
            m = moves[i]
            p = 2.0 if board.is_capture(m) else 1.0

            if is_opening:
                san = board.san(m)
                if san in BAD_OPENING_MOVES:
                    p *= 0.1

            if htn_moves and m in htn_moves:
                p += htn_bonus

            p += self.identity.get_bias(board, m)

            if self.mode == "TECHNICAL":
                if board.is_capture(m): p += 0.5
                if not board.is_capture(m) and not board.gives_check(m): p *= 0.5

            if m.to_square in [chess.E4, chess.D4, chess.E5, chess.D5]:
                p += 0.2

            priors[m] = p
            total_p += p

        ws = self._get_window_size(board)
        for m, p in priors.items():
            norm_p = p / total_p if total_p > 0 else 0
            if m not in node.children:
                child = TitanNode(parent=node, move=m, prior=norm_p, window_size=ws)
                child.is_quiet = not (board.is_capture(m) or board.gives_check(m))
                node.children[m] = child

    def search(self, board):
        return self.search_mcts(board)

    def close(self):
        self._send("quit")
        self.engine.wait()

def uci_loop(identity=None, headless=False):
    brain = HyperMCTS_Titan(identity=identity, headless=headless)
    if not headless:
        print("id name TitanBrain V17")
        print("id author Jules")
        print("uciok")

    # IMPORTANT: Output uciok even in headless for tools
    if headless:
        # print("uciok") # But wait, standard UCI protocol says engine sends ID then uciok.
        # If headless, we suppress ID?
        # titan_coliseum expects uciok.
        pass

    board = chess.Board()

    while True:
        try:
            line = sys.stdin.readline()
            if not line: break
            line = line.strip()

            if line == "uci":
                # Ensure uciok is sent!
                if not headless:
                    print("id name TitanBrain V17")
                print("uciok")
                sys.stdout.flush()
            elif line == "isready":
                print("readyok")
                sys.stdout.flush()
            elif line == "quit":
                break
            elif line.startswith("position"):
                parts = line.split()
                if "startpos" in parts:
                    board.reset()
                    if "moves" in parts:
                        idx = parts.index("moves")
                        for m in parts[idx+1:]:
                            board.push(chess.Move.from_uci(m))
                elif "fen" in parts:
                    idx = parts.index("fen")
                    fen_parts = []
                    for p in parts[idx+1:]:
                        if p == "moves": break
                        fen_parts.append(p)
                    board = chess.Board(" ".join(fen_parts))
                    if "moves" in parts:
                        idx = parts.index("moves")
                        for m in parts[idx+1:]:
                            board.push(chess.Move.from_uci(m))
            elif line.startswith("go"):
                move = brain.search(board)
                if move:
                    print(f"bestmove {move.uci()}")
                else:
                    print("bestmove 0000")
                sys.stdout.flush()

        except Exception:
            break

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--identity", type=str, help="Force Titan Identity")
    parser.add_argument("--headless", action="store_true", help="Suppress extra output")
    args = parser.parse_args()

    uci_loop(identity=args.identity, headless=args.headless)
