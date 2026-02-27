import pygame
import chess
import os
import urllib.request
import threading
import requests
import time
import sys

# Config (Default to localhost, override with arg)
SERVER_URL = "http://localhost:5000"

if len(sys.argv) > 1:
    SERVER_URL = sys.argv[1]
    # Add http if missing
    if not SERVER_URL.startswith("http"):
        SERVER_URL = "http://" + SERVER_URL

print(f"Connecting to Game Server at: {SERVER_URL}")

WIDTH, HEIGHT = 600, 600
BOARD_SIZE = 512
SQ_SIZE = BOARD_SIZE // 8
OFFSET_X = (WIDTH - BOARD_SIZE) // 2
OFFSET_Y = (HEIGHT - BOARD_SIZE) // 2
COLOR_LIGHT = (235, 236, 208)
COLOR_DARK = (119, 149, 86)
IMAGES = {}

def load_images():
    pieces = ['wP', 'wR', 'wN', 'wB', 'wQ', 'wK', 'bP', 'bR', 'bN', 'bB', 'bQ', 'bK']
    if not os.path.exists("images"): os.makedirs("images")
    base_url = "https://images.chesscomfiles.com/chess-themes/pieces/neo/150/"
    for piece in pieces:
        path = os.path.join("images", f"{piece}.png")
        if not os.path.exists(path):
            try:
                c = "w" if piece[0] == 'w' else 'b'
                r = piece[1].lower()
                urllib.request.urlretrieve(f"{base_url}{c}{r}.png", path)
            except: pass
        try:
            img = pygame.image.load(path)
            IMAGES[piece] = pygame.transform.scale(img, (SQ_SIZE, SQ_SIZE))
        except: pass

class RemoteClient:
    def __init__(self):
        pygame.init()
        self.screen = pygame.display.set_mode((WIDTH, HEIGHT))
        pygame.display.set_caption(f"OxtaCore Client -> {SERVER_URL}")
        self.clock = pygame.time.Clock()
        self.board = chess.Board()
        self.selected_sq = None
        self.player_color = chess.WHITE
        self.thinking = False

    def draw_board(self):
        self.screen.fill((40, 40, 40))
        for r in range(8):
            for c in range(8):
                color = COLOR_LIGHT if (r + c) % 2 == 0 else COLOR_DARK
                rect = pygame.Rect(OFFSET_X + c * SQ_SIZE, OFFSET_Y + r * SQ_SIZE, SQ_SIZE, SQ_SIZE)
                pygame.draw.rect(self.screen, color, rect)
                if self.selected_sq is not None:
                    sel_col = chess.square_file(self.selected_sq)
                    sel_row = 7 - chess.square_rank(self.selected_sq)
                    if sel_col == c and sel_row == r:
                        s = pygame.Surface((SQ_SIZE, SQ_SIZE))
                        s.set_alpha(100)
                        s.fill((255, 255, 0))
                        self.screen.blit(s, rect)
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

        for sq, piece in self.board.piece_map().items():
            c = chess.square_file(sq)
            r = 7 - chess.square_rank(sq)
            name = ('w' if piece.color else 'b') + piece.symbol().upper()
            if name in IMAGES:
                self.screen.blit(IMAGES[name], (OFFSET_X + c * SQ_SIZE, OFFSET_Y + r * SQ_SIZE))

    def request_move(self):
        self.thinking = True
        try:
            fen = self.board.fen()
            resp = requests.post(f"{SERVER_URL}/move", json={"fen": fen, "time_limit": 2.0}, timeout=30)
            if resp.status_code == 200:
                data = resp.json()
                move_uci = data.get("best_move")
                if move_uci:
                    self.board.push(chess.Move.from_uci(move_uci))
                else:
                    print("Server returned no move.")
            else:
                print(f"Server Error: {resp.status_code}")
        except Exception as e:
            print(f"Connection Error: {e}")
        self.thinking = False

    def run(self):
        load_images()
        running = True
        while running:
            self.clock.tick(60)

            if not self.board.is_game_over() and self.board.turn != self.player_color and not self.thinking:
                threading.Thread(target=self.request_move).start()

            for event in pygame.event.get():
                if event.type == pygame.QUIT: running = False
                elif event.type == pygame.MOUSEBUTTONDOWN and self.board.turn == self.player_color:
                    x, y = pygame.mouse.get_pos()
                    c = (x - OFFSET_X) // SQ_SIZE
                    r = (y - OFFSET_Y) // SQ_SIZE
                    if 0 <= c < 8 and 0 <= r < 8:
                        sq = chess.square(c, 7 - r)
                        if self.selected_sq is None:
                            if self.board.piece_at(sq) and self.board.piece_at(sq).color == self.player_color:
                                self.selected_sq = sq
                        else:
                            move = chess.Move(self.selected_sq, sq)
                            if move not in self.board.legal_moves:
                                move = chess.Move(self.selected_sq, sq, promotion=chess.QUEEN)
                            if move in self.board.legal_moves:
                                self.board.push(move)
                                self.selected_sq = None
                            else:
                                if self.board.piece_at(sq) and self.board.piece_at(sq).color == self.player_color:
                                    self.selected_sq = sq
                                else:
                                    self.selected_sq = None
            self.draw_board()
            pygame.display.flip()
        pygame.quit()

if __name__ == "__main__":
    client = RemoteClient()
    client.run()
