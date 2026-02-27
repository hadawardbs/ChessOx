import pygame
import chess
import os
import urllib.request
import threading
from titan_brain import HyperMCTS_Titan

# Configurações visuais
WIDTH, HEIGHT = 600, 600
BOARD_SIZE = 512
SQ_SIZE = BOARD_SIZE // 8
OFFSET_X = (WIDTH - BOARD_SIZE) // 2
OFFSET_Y = (HEIGHT - BOARD_SIZE) // 2
COLOR_LIGHT = (235, 236, 208)
COLOR_DARK = (119, 149, 86)
IMAGES = {}

# Carregar imagens das peças
def load_images():
    pieces = ['wP', 'wR', 'wN', 'wB', 'wQ', 'wK',
              'bP', 'bR', 'bN', 'bB', 'bQ', 'bK']

    if not os.path.exists("images"): os.makedirs("images")
    base_url = "https://images.chesscomfiles.com/chess-themes/pieces/neo/150/"

    for piece in pieces:
        path = os.path.join("images", f"{piece}.png")
        if not os.path.exists(path):
            try:
                c = "w" if piece[0] == 'w' else 'b'
                r = piece[1].lower()
                urllib.request.urlretrieve(f"{base_url}{c}{r}.png", path)
            except:
                pass
        try:
            img = pygame.image.load(path)
            IMAGES[piece] = pygame.transform.scale(img, (SQ_SIZE, SQ_SIZE))
        except:
            pass

# Classe de GUI
class ChessGUI:
    def __init__(self):
        pygame.init()
        self.screen = pygame.display.set_mode((WIDTH, HEIGHT))
        pygame.display.set_caption("OxtaCore - Human vs AI")
        self.clock = pygame.time.Clock()
        self.board = chess.Board()
        self.engine = HyperMCTS_Titan(time_limit=1.0)
        self.selected_sq = None
        self.player_color = chess.WHITE
        self.engine_thinking = False
        self.self_play_mode = False # Visual Self-Play

    def draw_board(self):
        self.screen.fill((40, 40, 40))
        # Lista de movimentos possíveis
        possible_moves = []
        if self.selected_sq is not None:
            possible_moves = [m for m in self.board.legal_moves if m.from_square == self.selected_sq]

        for r in range(8):
            for c in range(8):
                color = COLOR_LIGHT if (r + c) % 2 == 0 else COLOR_DARK
                rect = pygame.Rect(OFFSET_X + c * SQ_SIZE, OFFSET_Y + r * SQ_SIZE, SQ_SIZE, SQ_SIZE)
                pygame.draw.rect(self.screen, color, rect)

                # Destaque da peça selecionada
                if self.selected_sq is not None:
                    sel_col = chess.square_file(self.selected_sq)
                    sel_row = 7 - chess.square_rank(self.selected_sq)
                    if sel_col == c and sel_row == r:
                        s = pygame.Surface((SQ_SIZE, SQ_SIZE))
                        s.set_alpha(100)
                        s.fill((255, 255, 0))
                        self.screen.blit(s, rect)

                # Último movimento
                if self.board.move_stack:
                    last = self.board.peek()
                    for sq in [last.from_square, last.to_square]:
                        l_col = chess.square_file(sq)
                        l_row = 7 - chess.square_rank(sq)
                        if l_col == c and l_row == r:
                            s = pygame.Surface((SQ_SIZE, SQ_SIZE))
                            s.set_alpha(100)
                            s.fill((155, 255, 50))
                            self.screen.blit(s, rect)

                # Destacar movimentos possíveis
                for m in possible_moves:
                    to_sq = m.to_square
                    t_col = chess.square_file(to_sq)
                    t_row = 7 - chess.square_rank(to_sq)
                    if t_col == c and t_row == r:
                        s = pygame.Surface((SQ_SIZE, SQ_SIZE), pygame.SRCALPHA)
                        s.fill((100, 100, 255, 120))  # Azul transparente
                        self.screen.blit(s, rect)

        # Desenhar peças
        for sq, piece in self.board.piece_map().items():
            c = chess.square_file(sq)
            r = 7 - chess.square_rank(sq)
            name = ('w' if piece.color else 'b') + piece.symbol().upper()
            if name in IMAGES:
                self.screen.blit(IMAGES[name], (OFFSET_X + c * SQ_SIZE, OFFSET_Y + r * SQ_SIZE))

    def engine_move(self):
        self.engine_thinking = True
        move = self.engine.search(self.board)
        if move:
            self.board.push(move)
        else:
            print("AI Resigned")
            self.self_play_mode = False
        self.engine_thinking = False

    def run(self):
        load_images()
        running = True

        while running:
            self.clock.tick(60)

            # Logic:
            # If Self Play: Engine plays BOTH turns.
            # If Human vs AI: Engine plays if turn != player_color.

            should_play = False
            if not self.board.is_game_over() and not self.engine_thinking:
                if self.self_play_mode:
                    should_play = True
                elif self.board.turn != self.player_color:
                    should_play = True

            if should_play:
                threading.Thread(target=self.engine_move).start()

            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False

                elif event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_s:
                        self.self_play_mode = not self.self_play_mode
                        title = "OxtaCore - Self Play" if self.self_play_mode else "OxtaCore - Human vs AI"
                        pygame.display.set_caption(title)
                        print(f"Self Play Mode: {self.self_play_mode}")

                    elif event.key == pygame.K_r: # Reset
                        self.board.reset()
                        self.selected_sq = None
                        print("Game Reset")

                elif event.type == pygame.MOUSEBUTTONDOWN:
                    # Allow move only if Human Turn OR (Debug override)
                    if not self.self_play_mode and self.board.turn == self.player_color:
                        x, y = pygame.mouse.get_pos()
                        # Convert to board coords
                        c = (x - OFFSET_X) // SQ_SIZE
                        r = (y - OFFSET_Y) // SQ_SIZE

                        if 0 <= c < 8 and 0 <= r < 8:
                            sq = chess.square(c, 7 - r)

                            if self.selected_sq is None:
                                if self.board.piece_at(sq) and self.board.piece_at(sq).color == self.board.turn:
                                    self.selected_sq = sq
                            else:
                                move = chess.Move(self.selected_sq, sq)
                                # Auto-queen promotion
                                if move not in self.board.legal_moves:
                                    move = chess.Move(self.selected_sq, sq, promotion=chess.QUEEN)

                                if move in self.board.legal_moves:
                                    self.board.push(move)
                                    self.selected_sq = None
                                else:
                                    # Deselect or select new piece
                                    if self.board.piece_at(sq) and self.board.piece_at(sq).color == self.board.turn:
                                        self.selected_sq = sq
                                    else:
                                        self.selected_sq = None

            self.draw_board()
            pygame.display.flip()

        self.engine.close()
        pygame.quit()

if __name__ == "__main__":
    gui = ChessGUI()
    gui.run()
