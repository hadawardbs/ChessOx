import json
import os
import chess
import chess.polyglot
import math

MEMORY_FILE = "episodic_memory.json"

class EpisodicMemory:
    def __init__(self):
        self.memory = {}
        self.load()

    def load(self):
        if os.path.exists(MEMORY_FILE):
            try:
                with open(MEMORY_FILE, "r") as f:
                    self.memory = json.load(f)
            except:
                self.memory = {}

    def save(self):
        try:
            with open(MEMORY_FILE, "w") as f:
                json.dump(self.memory, f, indent=2)
        except: pass

    def _get_key(self, board):
        return str(chess.polyglot.zobrist_hash(board))

    def remember(self, board, failure_type, weight=1.0, vector=None):
        key = self._get_key(board)
        if key not in self.memory:
            self.memory[key] = {}

        current = self.memory[key].get(failure_type, 0.0)
        self.memory[key][failure_type] = current + weight
        if vector:
            self.memory[key]["vector"] = vector
        self.save()

    def recall(self, board):
        key = self._get_key(board)
        return self.memory.get(key, {})

    def recall_similar(self, vector, threshold=0.2):
        # Semantic Recall: Find failures in similar positions
        if not vector: return {}

        aggregated = {}
        for k, v in self.memory.items():
            if "vector" in v:
                stored_vec = v["vector"]
                # Euclidean distance
                dist = math.sqrt(sum((a-b)**2 for a,b in zip(vector, stored_vec)))
                if dist < threshold:
                    # Merge failures
                    for fail_type, val in v.items():
                        if fail_type != "vector":
                            aggregated[fail_type] = aggregated.get(fail_type, 0.0) + val
        return aggregated

# Global Instance
memory = EpisodicMemory()
