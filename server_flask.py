from flask import Flask, request, jsonify
from titan_brain import HyperMCTS_Titan
import chess
import threading
import socket

app = Flask(__name__)

# Initialize Engine (Global)
engine = None
engine_lock = threading.Lock()

def get_ip_address():
    try:
        # Trick to get the primary IP address (Radmin usually)
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except:
        return "127.0.0.1"

def get_engine():
    global engine
    if engine is None:
        try:
            print("Starting OxtaCore Engine...")
            engine = HyperMCTS_Titan(time_limit=1.0, cores=4)
        except Exception as e:
            print(f"Failed to start engine: {e}")
            return None
    return engine

@app.route('/status', methods=['GET'])
def status():
    eng = get_engine()
    return jsonify({"status": "online" if eng else "error"})

@app.route('/move', methods=['POST'])
def move():
    data = request.json
    fen = data.get('fen')
    time_limit = data.get('time_limit', 1.0)

    if not fen:
        return jsonify({"error": "No FEN provided"}), 400

    eng = get_engine()
    if not eng:
        return jsonify({"error": "Engine not available"}), 500

    with engine_lock:
        eng.time_limit = float(time_limit)
        board = chess.Board(fen)

        if board.is_game_over():
            return jsonify({"best_move": None, "game_over": True})

        move = eng.search(board)

        if move:
            return jsonify({"best_move": move.uci()})
        else:
            return jsonify({"best_move": None, "error": "Search failed"})

@app.route('/eval', methods=['POST'])
def evaluate():
    data = request.json
    fen = data.get('fen')

    eng = get_engine()
    if not eng: return jsonify({"error": "Engine not available"}), 500

    with engine_lock:
        score = eng.get_eval(fen)
        return jsonify({"score": score})

if __name__ == '__main__':
    ip = get_ip_address()
    print("="*50)
    print(f" OXTA SERVER ONLINE")
    print(f" Local IP: http://{ip}:5000")
    print(f" Share this IP with your Radmin VPN friend!")
    print("="*50)
    app.run(host='0.0.0.0', port=5000)
