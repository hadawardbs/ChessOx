import time
import chess
import os
import sys
# from selenium import webdriver ... (Mocking Selenium for headless environment)
# Since I cannot run Chrome in this container, I will create a CLI bot interface
# that simulates the "Bot" loop.

from titan_brain import HyperMCTS_Titan

class CLI_Bot:
    def __init__(self):
        self.brain = HyperMCTS_Titan(time_limit=1.0)
        self.board = chess.Board()

    def run(self):
        print(">>> TITAN BOT CLI <<<")
        print("Type moves (e2e4) or 'go' to let bot play.")

        while not self.board.is_game_over():
            print(f"\n{self.board}")
            user_input = input("Move/Cmd: ").strip()

            if user_input == "quit":
                break
            elif user_input == "go":
                move = self.brain.search(self.board)
                if move:
                    print(f"Bot plays: {move.uci()}")
                    self.board.push(move)
                else:
                    print("Bot resigned (no move).")
            else:
                try:
                    move = chess.Move.from_uci(user_input)
                    if move in self.board.legal_moves:
                        self.board.push(move)
                    else:
                        print("Illegal move.")
                except:
                    print("Invalid format.")

        print("Game Over:", self.board.result())
        self.brain.close()

if __name__ == "__main__":
    bot = CLI_Bot()
    bot.run()
