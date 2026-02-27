import chess
import math

# --- Causal Inference ---
class CausalEngine:
    def __init__(self):
        pass

    def get_causal_cone(self, board, square):
        """
        Identifies the set of squares that are causally relevant to a given square.
        Relevant = Attacking, Defending, or Blocking access to.
        """
        cone = set()
        cone.add(square)

        # 1. Direct Attackers/Defenders
        attackers = board.attackers(chess.WHITE, square) | board.attackers(chess.BLACK, square)
        cone.update(attackers)

        # 2. Blockers (Ray pieces)
        # Simplified: Add squares between slider attackers and target
        for atk in attackers:
            pt = board.piece_type_at(atk)
            if pt in [chess.BISHOP, chess.ROOK, chess.QUEEN]:
                between = chess.SquareSet.between(atk, square)
                for b_sq in between:
                    cone.add(b_sq)

        return cone

    def prune_non_causal(self, board, moves, goal_square):
        """
        Filters moves that do not interact with the causal cone of the goal_square.
        Used for 'Panic Mode' pruning.
        """
        cone = self.get_causal_cone(board, goal_square)
        relevant_moves = []

        for m in moves:
            # Move is relevant if:
            # 1. It lands in the cone (capture/block/defend)
            # 2. It starts in the cone (piece moving away, opening line)
            # 3. It checks the king (always relevant)
            if m.to_square in cone or m.from_square in cone or board.gives_check(m):
                relevant_moves.append(m)

        # Fallback if pruning is too aggressive
        if not relevant_moves:
            return moves

        return relevant_moves

# --- Topological Data Analysis ---
class TopologicalScanner:
    def __init__(self):
        pass

    def compute_betti_numbers(self, board):
        """
        Computes simplified Betti numbers for the Pawn Structure.
        b0: Number of connected components (Pawn Islands)
        b1: Number of holes (Enclosed areas control by enemy?) - simplified to 'Locked Chains'
        """
        # 1. Pawn Islands (b0)
        w_pawns = list(board.pieces(chess.PAWN, chess.WHITE))
        b_pawns = list(board.pieces(chess.PAWN, chess.BLACK))

        b0_white = self._count_islands(w_pawns)
        b0_black = self._count_islands(b_pawns)

        # 2. Locked Chains (b1 proxy)
        # Find pawns blocked by enemy pawns directly
        b1_white = self._count_locked(board, w_pawns, 1)
        b1_black = self._count_locked(board, b_pawns, -1)

        return {
            "white": {"b0": b0_white, "b1": b1_white},
            "black": {"b0": b0_black, "b1": b1_black}
        }

    def _count_islands(self, squares):
        if not squares: return 0

        # Simple clustering based on file adjacency
        files = sorted(list(set(chess.square_file(sq) for sq in squares)))
        if not files: return 0

        islands = 1
        for i in range(len(files) - 1):
            if files[i+1] > files[i] + 1:
                islands += 1
        return islands

    def _count_locked(self, board, squares, direction):
        locked = 0
        for sq in squares:
            target = chess.square(chess.square_file(sq), chess.square_rank(sq) + direction)
            if 0 <= target < 64:
                occ = board.piece_at(target)
                if occ and occ.piece_type == chess.PAWN and occ.color != board.color_at(sq):
                    locked += 1
        return locked

    def get_embedding(self, board):
        """
        Returns a scalar adjustment based on topology.
        Positive = Good Structure.
        """
        betti = self.compute_betti_numbers(board)

        w_score = -0.1 * betti["white"]["b0"] + 0.05 * betti["white"]["b1"] # Fewer islands good, locked chains complex
        b_score = -0.1 * betti["black"]["b0"] + 0.05 * betti["black"]["b1"]

        return (w_score - b_score) if board.turn == chess.WHITE else (b_score - w_score)
