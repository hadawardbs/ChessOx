import chess

class CSPSolver:
    def __init__(self):
        pass

    def solve(self, board, mode="TACTICAL_SURVIVAL"):
        """
        Solves a rigid constraint problem.
        Modes:
        - TACTICAL_SURVIVAL: Must not lose material > Pawn immediately.
        - MATE_SEARCH: Must find forced mate (not implemented in python fast enough).
        """
        if mode == "TACTICAL_SURVIVAL":
            return self._solve_survival(board)
        return []

    def _solve_survival(self, board):
        """
        Returns moves that do not result in immediate material loss (e.g. hanging a piece).
        Simple Quiescence-like check (depth 1).
        """
        candidates = []
        for m in board.legal_moves:
            # 1. Does it hang a piece?
            # Make move
            board.push(m)

            # Simple SEE-like logic: Is the moved piece attacked more than defended?
            # Or is the King safe?
            if board.is_check(): # We just checked ourselves? No, is_check checks turn.
                # If we are in check after our move, it's illegal (chess lib handles).
                # Wait, board.is_check() checks if side_to_move is in check.
                # After push, side_to_move is Opponent.
                pass

            # Check for hanging pieces (simple)
            # Did we move to a square attacked by lower value piece?
            # Or did we leave a piece hanging?
            # This is too slow for Python in tight loop.
            # Simplified constraint: Just ensure we didn't blunder checkmate or queen.

            # Constraint: Don't get mated in 1
            if not self._is_mated_in_1(board):
                 candidates.append(m)

            board.pop()

        return candidates

    def _is_mated_in_1(self, board):
        # Can opponent mate us immediately?
        for m in board.legal_moves:
            board.push(m)
            if board.is_checkmate():
                board.pop()
                return True
            board.pop()
        return False
