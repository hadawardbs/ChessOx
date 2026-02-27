import time
import chess
import random
import multiprocessing
import os
import sys
from selenium import webdriver
from selenium.webdriver.common.by import By
from webdriver_manager.firefox import GeckoDriverManager
from selenium.webdriver.firefox.service import Service as FirefoxService
from selenium.webdriver.common.action_chains import ActionChains
from selenium.webdriver.common.actions.mouse_button import MouseButton
from selenium.webdriver.common.actions.action_builder import ActionBuilder

# Import OxtaCore Brain path finder
from titan_brain import find_engine
import subprocess

class FastUCIEngine:
    def __init__(self, time_limit=1.0, cores=1, on_think=None):
        self.time_limit = time_limit
        self.on_think = on_think
        self.engine_path = find_engine()
        self.proc = subprocess.Popen(
            [self.engine_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
            bufsize=1
        )
        self.proc.stdin.write("uci\n")
        self.proc.stdin.write(f"setoption name Hash value 256\n")
        self.proc.stdin.write(f"setoption name Threads value {cores}\n")
        self.proc.stdin.write("isready\n")
        self.proc.stdin.flush()
        while True:
            line = self.proc.stdout.readline().strip()
            if line == "readyok": break

    def search(self, board, movetime_ms=None):
        """Search with a fixed movetime. Simple and reliable."""
        mt = movetime_ms or int(self.time_limit * 1000)
        self.proc.stdin.write(f"position fen {board.fen()}\n")
        self.proc.stdin.write(f"go movetime {mt}\n")
        self.proc.stdin.flush()

        best_move = None
        last_pv = None
        while True:
            line = self.proc.stdout.readline()
            if not line:
                break
            line = line.strip()
            if line.startswith("info") and " pv " in line:
                parts = line.split(" pv ")
                if len(parts) > 1:
                    last_pv = parts[1].strip().split()
            if line.startswith("bestmove"):
                parts = line.split()
                if len(parts) >= 2 and parts[1] != "0000":
                    best_move = chess.Move.from_uci(parts[1])
                break

        # Draw arrow ONCE after search
        if last_pv and self.on_think:
            parsed = []
            for m in last_pv[:2]:
                try: parsed.append(chess.Move.from_uci(m))
                except: pass
            if parsed:
                self.on_think(parsed)

        return best_move

    def close(self):
        try:
            self.proc.stdin.write("quit\n")
            self.proc.stdin.flush()
            self.proc.wait(timeout=2)
        except:
            pass

class ChessBot:
    def __init__(self, time_per_move=2.0):
        print(">>> INICIANDO TITAN BOT (SELENIUM) <<<")
        
        snap_tmp = os.path.expanduser("~/snap/firefox/common/tmp")
        if os.path.exists(os.path.dirname(snap_tmp)):
            if not os.path.exists(snap_tmp):
                os.makedirs(snap_tmp)
            os.environ["TMPDIR"] = snap_tmp

        options = webdriver.FirefoxOptions()
        options.binary_location = "/usr/bin/firefox"
        options.add_argument("-profile")
        options.add_argument("/home/oxta/snap/firefox/common/.mozilla/firefox/93rg2m8s.default/")
        options.set_preference("dom.webdriver.enabled", False)
        options.set_preference("useAutomationExtension", False)

        try:
            self.driver = webdriver.Firefox(service=FirefoxService(GeckoDriverManager().install()), options=options)
        except Exception as e:
            print(f"Erro ao iniciar Firefox Driver: {e}")
            print("Certifique-se que o Firefox está instalado.")
            sys.exit(1)

        self.driver.maximize_window()

        try:
            cpu_count = min(os.cpu_count() or 4, 4)  # cap at 4 physical cores
            self.engine = FastUCIEngine(time_limit=time_per_move, cores=cpu_count, on_think=self.draw_thought)
            print(f"Engine configurada: Hash=256MB, Threads={cpu_count}")
        except Exception as e:
            print(f"Erro ao iniciar Engine C++: {e}")
            self.driver.quit()
            sys.exit(1)

        self.my_color = None
        self.last_drawn = []

    def draw_thought(self, moves):
        if not moves or moves == self.last_drawn: return
        self.last_drawn = list(moves)
        
        try:
            board_elem = self.find_board_element()
            if not board_elem: return
            
            rect = self.driver.execute_script("return arguments[0].getBoundingClientRect();", board_elem)
            sq_size = rect['width'] / 8
            
            action = ActionBuilder(self.driver)
            
            # Clear previous arrows by left clicking the edge of the board slightly? Or just let them layer up.
            # actually drawing an arrow starting from same square to same square clears it.
            # but letting them layer up looks like "thinking" chaotic arrows.

            for m in moves:
                col_f = chess.square_file(m.from_square)
                row_f = chess.square_rank(m.from_square)
                col_t = chess.square_file(m.to_square)
                row_t = chess.square_rank(m.to_square)
                
                if self.my_color == chess.BLACK:
                    col_f, row_f = 7 - col_f, row_f
                    col_t, row_t = 7 - col_t, row_t
                else:
                    col_f, row_f = col_f, 7 - row_f
                    col_t, row_t = col_t, 7 - row_t
                    
                x1 = int(rect['left'] + (col_f * sq_size) + (sq_size / 2))
                y1 = int(rect['top'] + (row_f * sq_size) + (sq_size / 2))
                x2 = int(rect['left'] + (col_t * sq_size) + (sq_size / 2))
                y2 = int(rect['top'] + (row_t * sq_size) + (sq_size / 2))
                
                action.pointer_action.move_to_location(x1, y1)
                action.pointer_action.pointer_down(MouseButton.RIGHT)
                action.pointer_action.move_to_location(x2, y2)
                action.pointer_action.pointer_up(MouseButton.RIGHT)
                
            action.perform()
        except:
            pass

    def find_board_element(self):
        if hasattr(self, '_cached_board') and self._cached_board:
            try:
                if self._cached_board.size['width'] > 0:
                    return self._cached_board
            except:
                self._cached_board = None
        
        # Priority on custom elements which usually have the game state
        selectors = [
            (By.TAG_NAME, "chess-board"),
            (By.TAG_NAME, "wc-chess-board"),
            (By.CSS_SELECTOR, "div.board"),
            (By.ID, "board-layout-chessboard"),
            (By.CLASS_NAME, "board")
        ]
        for by_method, selector in selectors:
            try:
                elements = self.driver.find_elements(by_method, selector)
                for element in elements:
                    if element.size['width'] > 0:
                        # Print debug only if we find something new
                        if not hasattr(self, '_last_selector') or self._last_selector != selector:
                            print(f"DEBUG: Encontrei tabuleiro usando {by_method}: {selector} (Tag: {element.tag_name})")
                            self._last_selector = selector
                        self._cached_board = element
                        return element
            except:
                continue
        return None

    def get_board_state(self):
        """Returns (fen, color) or (None, None) using a unified script."""
        script = """
        const tags = ['chess-board', 'wc-chess-board'];
        let board = null;
        for (let t of tags) {
            board = document.querySelector(t);
            if (board) break;
        }
        if (!board) board = document.querySelector('.board');
        if (!board) return null;

        let fen = null;
        if (board.game && typeof board.game.getFEN === 'function') fen = board.game.getFEN();
        else if (board.gameSetup && board.gameSetup.fen) fen = board.gameSetup.fen;
        else if (typeof board.getFen === 'function') fen = board.getFen();
        else fen = board.getAttribute('position');

        if (!fen) return null;

        // Detect orientation
        let orientation = null;
        if (board.getAttribute('orientation')) orientation = board.getAttribute('orientation');
        else if (board.dataset.orientation) orientation = board.dataset.orientation;
        else if (board.classList.contains('flipped')) orientation = 'black';
        else if (board.game && board.game.getOptions) orientation = board.game.getOptions().orientation;
        
        // Fallback: Check piece positions if orientation attribute is missing
        if (!orientation) {
            // Find all pieces and check if black pieces are at the bottom (rank 1-2) or top (rank 7-8)
            // But an easier way: check the squares classes or labels
            const square8 = document.querySelector('.square-18'); // In a normal board, 18 is a1 or h8?
            // Actually, let's just look for the 'flipped' class on parents
            if (document.querySelector('.board-layout-main.flipped') || document.querySelector('.board-container.flipped')) {
                orientation = 'black';
            } else {
                orientation = 'white';
            }
        }

        return { fen: fen, orientation: orientation };
        """
        try:
            state = self.driver.execute_script(script)
            if not state or not state['fen']: return None, None
            
            color = chess.BLACK if state['orientation'] == 'black' else chess.WHITE
            return state['fen'], color
        except Exception as e:
            # print(f"DEBUG Error get_board_state: {e}")
            return None, None

    def detect_color(self):
        try:
            # Try multiple ways to get orientation (robustness)
            script = """
                const b = document.querySelector('chess-board') || document.querySelector('wc-chess-board') || document.querySelector('.board');
                if (!b) return null;
                
                // 1. Check standard attributes
                if (b.getAttribute('orientation')) return b.getAttribute('orientation');
                if (b.dataset.orientation) return b.dataset.orientation;
                
                // 2. Check CSS classes
                if (b.classList.contains('flipped')) return 'black';
                if (b.parentElement && (b.parentElement.classList.contains('flipped') || b.parentElement.classList.contains('board-flipped'))) return 'black';
                
                // 3. Check JS properties if it's a component
                if (b.game && b.game.getOptions) return b.game.getOptions().orientation;
                if (b.gameSetup && b.gameSetup.orientation) return b.gameSetup.orientation;
                
                // 4. Heuristic: check if coordinates are inverted? (Too complex for here, but let's check for "flipped" in class of parent)
                let boardLayout = document.querySelector('.board-layout-main');
                if (boardLayout && boardLayout.classList.contains('flipped')) return 'black';

                // Default if we find the board but no "flipped" sign
                return 'white'; 
            """
            orientation = self.driver.execute_script(script)
            
            if orientation == 'black':
                self.my_color = chess.BLACK
                print(">>> JOGANDO DE PRETAS (Detectado: black) <<<")
                return True
            elif orientation == 'white':
                self.my_color = chess.WHITE
                print(">>> JOGANDO DE BRANCAS (Detectado: white) <<<")
                return True
            
            return False
        except Exception as e:
            print(f"DEBUG Erro detect_color: {e}")
            return False

    def click_square_js(self, square_idx):
        try:
            board_elem = self.find_board_element()
            if not board_elem: return

            self.driver.execute_script("arguments[0].scrollIntoView({block: 'center'});", board_elem)

            col = chess.square_file(square_idx)
            row = chess.square_rank(square_idx)

            if self.my_color == chess.BLACK:
                viz_col = 7 - col
                viz_row = row
            else:
                viz_col = col
                viz_row = 7 - row

            rect = self.driver.execute_script("return arguments[0].getBoundingClientRect();", board_elem)
            sq_size = rect['width'] / 8

            x_offset = int((viz_col * sq_size) + (sq_size / 2))
            y_offset = int((viz_row * sq_size) + (sq_size / 2))
            
            # Using ActionChains instead of JS mouse events to guarantee realistic click dispatch
            actions = ActionChains(self.driver)
            actions.move_to_element_with_offset(board_elem, x_offset - int(rect['width']/2), y_offset - int(rect['height']/2))
            actions.click().perform()

        except Exception as e:
            print(f"Erro click: {e}")

    def make_move(self, move):
        print(f"Executando: {move.uci()}")
        self.click_square_js(move.from_square)
        time.sleep(0.05)
        self.click_square_js(move.to_square)

        if move.promotion:
            time.sleep(0.2)
            self.click_square_js(move.to_square)

    def run(self):
        print("Navegando para Chess.com...")
        self.driver.get("https://www.chess.com/play/online")
        print("\n=== SETUP ===")
        print("Faça login se necessário e selecione o bot.")
        print("Basta fazer seu primeiro lance (ou o bot fazer o dele) para a engine começar a jogar automaticamente!")

        while True:  # Outer loop: keeps running game after game
            print("\nAguardando o tabuleiro ser carregado e detectar a cor...")
            self._cached_board = None  # Reset cached board for new game

            attempts = 0
            while True:
                fen, color = self.get_board_state()
                if fen and color is not None:
                    self.my_color = color
                    print(f">>> JOGANDO DE {'BRANCAS' if color == chess.WHITE else 'PRETAS'} <<<")
                    print("Tabuleiro e cor detectados com sucesso!")
                    break
                else:
                    if attempts % 10 == 0:
                        print("DEBUG: Aguardando detecção do tabuleiro e cor...")
                    attempts += 1
                time.sleep(1)

            last_fen_str = ""
            self.last_drawn = []

            while True:
                try:
                    fen, color = self.get_board_state()
                    if not fen:
                        time.sleep(0.5)
                        continue

                    fen_position = fen.split(" ")[0]
                    if fen_position == last_fen_str:
                        time.sleep(0.05)
                        continue

                    last_fen_str = fen_position
                    board = chess.Board(fen)

                    if board.is_game_over():
                        result = board.result()
                        print(f"\n=== FIM DE JOGO: {result} ===")
                        print("Aguardando nova partida... (aperte 'Jogar Novamente' no Chess.com)")
                        time.sleep(3)
                        break

                    if board.turn == self.my_color:
                        print("Minha vez de jogar...")
                        best_move = self.engine.search(board)
                        if best_move:
                            self.make_move(best_move)
                        else:
                            print("Engine não retornou lance.")

                        time.sleep(0.1)
                    else:
                        time.sleep(0.05)

                except KeyboardInterrupt:
                    print("\nBot encerrado pelo usuário.")
                    self.engine.close()
                    return
                except Exception as e:
                    print(f"Erro loop: {e}")
                    time.sleep(1)

if __name__ == "__main__":
    multiprocessing.freeze_support()
    bot = ChessBot(time_per_move=0.5) # Bullet tuning (500ms per move)
    bot.run()
