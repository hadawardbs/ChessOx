import chess
import math

class HTNPlanner:
    def __init__(self):
        self.current_task = "START_GAME"
        self.plan_history = []

        # Strategic Concepts (Methods)
        self.center_squares = [chess.E4, chess.D4, chess.E5, chess.D5]
        self.extended_center = [chess.C3, chess.D3, chess.E3, chess.F3,
                                chess.C6, chess.D6, chess.E6, chess.F6]

    def decompose(self, board):
        """
        Decomposes the high-level Game State into a specific Task and Suggests Moves.
        Returns: (TaskName, List[chess.Move], BonusWeight)
        """
        # 1. Determine High-Level Phase
        phase = self._get_phase(board)

        # 2. Select Method based on Phase and State
        if phase == "OPENING":
            return self._plan_opening(board)
        elif phase == "MIDGAME":
            return self._plan_midgame(board)
        else:
            return self._plan_endgame(board)

    def _get_phase(self, board):
        # Simple Phase Detection based on Move Count and Material
        if board.fullmove_number < 10:
            return "OPENING"

        queens = len(board.pieces(chess.QUEEN, chess.WHITE)) + len(board.pieces(chess.QUEEN, chess.BLACK))
        minors = len(board.pieces(chess.KNIGHT, chess.WHITE)) + \
                 len(board.pieces(chess.BISHOP, chess.WHITE)) + \
                 len(board.pieces(chess.KNIGHT, chess.BLACK)) + \
                 len(board.pieces(chess.BISHOP, chess.BLACK))

        if queens == 0 and minors < 4:
            return "ENDGAME"

        return "MIDGAME"

    # --- OPENING METHODS ---
    def _plan_opening(self, board):
        # Task 1: Develop Minors
        undeveloped_minors = self._get_undeveloped_minors(board)
        if undeveloped_minors:
            return ("DEVELOP_MINORS", self._generate_development_moves(board), 0.5)

        # Task 2: Control Center
        if not self._is_center_controlled(board):
            return ("CONTROL_CENTER", self._generate_center_moves(board), 0.4)

        # Task 3: King Safety (Castle)
        if self._needs_castling(board):
            return ("CASTLE_KING", self._generate_castling_moves(board), 0.6)

        return ("FLEXIBLE_DEVELOPMENT", [], 0.1)

    def _get_undeveloped_minors(self, board):
        color = board.turn
        # Correctly using rank indices (0-7)
        backrank = 0 if color == chess.WHITE else 7
        minors = []
        for pt in [chess.KNIGHT, chess.BISHOP]:
            for sq in board.pieces(pt, color):
                if chess.square_rank(sq) == backrank:
                    minors.append(sq)
        return minors

    def _generate_development_moves(self, board):
        moves = []
        for m in board.legal_moves:
            # Move minor from backrank to active square
            if board.piece_type_at(m.from_square) in [chess.KNIGHT, chess.BISHOP]:
                # Don't move back to backrank
                if chess.square_rank(m.to_square) != chess.square_rank(m.from_square):
                     moves.append(m)
        return moves

    def _is_center_controlled(self, board):
        # Simple heuristic: Do we have pawns/pieces attacking center?
        color = board.turn
        control = 0
        for sq in self.center_squares:
            if board.is_attacked_by(color, sq):
                control += 1
        return control >= 2

    def _generate_center_moves(self, board):
        moves = []
        for m in board.legal_moves:
            if m.to_square in self.center_squares or m.to_square in self.extended_center:
                moves.append(m)
        return moves

    def _needs_castling(self, board):
        color = board.turn
        if board.has_castling_rights(color):
            # If king is still in center files e, d
            king_sq = board.king(color)
            if king_sq is not None and chess.square_file(king_sq) in [3, 4]:
                return True
        return False

    def _generate_castling_moves(self, board):
        moves = []
        for m in board.legal_moves:
            if board.is_castling(m):
                moves.append(m)
        return moves

    # --- MIDGAME METHODS ---
    def _plan_midgame(self, board):
        # Task 1: Attack King (if unsafe)
        enemy_king_safety = self._assess_king_safety(board, not board.turn)
        if enemy_king_safety < 0.5: # Weak King
            return ("ATTACK_KING", self._generate_attack_moves(board), 0.6)

        # Task 2: Improve Piece Activity
        return ("IMPROVE_ACTIVITY", self._generate_activity_moves(board), 0.3)

    def _assess_king_safety(self, board, color):
        king_sq = board.king(color)
        if king_sq is None: return 0.0

        # Count defenders
        defenders = len(board.attackers(color, king_sq))
        # Count attackers
        attackers = len(board.attackers(not color, king_sq))

        # Simple Ratio
        safety = 1.0
        if attackers > defenders: safety -= 0.3

        # Pawn Shield
        rank = chess.square_rank(king_sq)
        file = chess.square_file(king_sq)
        direction = 1 if color == chess.WHITE else -1
        shield_sqs = [
            chess.square(file, rank + direction) if 0 <= rank + direction <= 7 else None,
            chess.square(file - 1, rank + direction) if 0 <= rank + direction <= 7 and file > 0 else None,
            chess.square(file + 1, rank + direction) if 0 <= rank + direction <= 7 and file < 7 else None,
        ]
        shield = sum(1 for s in shield_sqs if s is not None and
                         board.piece_at(s) is not None and
                         board.piece_type_at(s) == chess.PAWN and
                         board.color_at(s) == color)

        if shield < 2: safety -= 0.2
        return max(0.0, safety)

    def _generate_attack_moves(self, board):
        moves = []
        enemy_king = board.king(not board.turn)
        if enemy_king is None: return []

        king_zone_files = range(max(0, chess.square_file(enemy_king)-2), min(8, chess.square_file(enemy_king)+3))
        king_zone_ranks = range(max(0, chess.square_rank(enemy_king)-2), min(8, chess.square_rank(enemy_king)+3))

        for m in board.legal_moves:
            if board.gives_check(m):
                moves.append(m)
                continue

            # Moves ending near enemy king
            if chess.square_file(m.to_square) in king_zone_files and \
               chess.square_rank(m.to_square) in king_zone_ranks:
                moves.append(m)

        return moves

    def _generate_activity_moves(self, board):
        # Moves that increase mobility or control center
        moves = []
        for m in board.legal_moves:
            # Simple heuristic: Forward moves for pieces
            if board.piece_type_at(m.from_square) != chess.PAWN:
                # White moves up rank, Black moves down
                curr_rank = chess.square_rank(m.from_square)
                next_rank = chess.square_rank(m.to_square)
                if board.turn == chess.WHITE and next_rank > curr_rank:
                    moves.append(m)
                elif board.turn == chess.BLACK and next_rank < curr_rank:
                    moves.append(m)
        return moves

    # --- ENDGAME METHODS ---
    def _plan_endgame(self, board):
        # Task 1: Push Passed Pawns
        passed_pawns = self._get_passed_pawns(board)
        if passed_pawns:
            return ("PROMOTE_PAWN", self._generate_pawn_pushes(board, passed_pawns), 0.7)

        # Task 2: Activate King
        return ("ACTIVATE_KING", self._generate_king_moves(board), 0.5)

    def _get_passed_pawns(self, board):
        passed = []
        color = board.turn
        pawns = board.pieces(chess.PAWN, color)
        enemy_pawns = board.pieces(chess.PAWN, not color)

        for sq in pawns:
            file = chess.square_file(sq)
            rank = chess.square_rank(sq)

            # Check files ahead
            is_passed = True
            check_files = [file]
            if file > 0: check_files.append(file - 1)
            if file < 7: check_files.append(file + 1)

            direction = 1 if color == chess.WHITE else -1

            # Look ahead for blockers
            for ep in enemy_pawns:
                efile = chess.square_file(ep)
                erank = chess.square_rank(ep)
                if efile in check_files:
                    if (color == chess.WHITE and erank > rank) or \
                       (color == chess.BLACK and erank < rank):
                        is_passed = False
                        break
            if is_passed:
                passed.append(sq)
        return passed

    def _generate_pawn_pushes(self, board, pawns):
        moves = []
        for m in board.legal_moves:
            if m.from_square in pawns:
                moves.append(m)
        return moves

    def _generate_king_moves(self, board):
        moves = []
        for m in board.legal_moves:
            if board.piece_type_at(m.from_square) == chess.KING:
                moves.append(m)
        return moves
