import subprocess
import time
import platform
import os
import sys

# Detect Engine Path
if platform.system() == "Windows":
    ENGINE_PATHS = [
        "build/Release/OxtaCore.exe",
        "build/OxtaCore.exe"
    ]
else:
    ENGINE_PATHS = ["build/OxtaCore"]

ENGINE = None
for p in ENGINE_PATHS:
    if os.path.exists(p):
        ENGINE = p
        break

if not ENGINE:
    print("Error: OxtaCore binary not found in build/ directory.")
    sys.exit(1)

print(f"Using Engine: {ENGINE}")

def send(p, cmd):
    p.stdin.write(cmd + "\n")
    p.stdin.flush()

p = subprocess.Popen(
    [ENGINE],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    text=True,
    bufsize=1
)

send(p, "uci")
while True:
    line = p.stdout.readline()
    if not line: break
    # print(line.strip())
    if "uciok" in line:
        break

send(p, "isready")
while True:
    line = p.stdout.readline()
    if not line: break
    if "readyok" in line:
        break

print("[SELFPLAY] Engine ready")

send(p, "position startpos")
send(p, "go movetime 200")

start = time.time()
while time.time() - start < 5: # Timeout 5s
    line = p.stdout.readline()
    if not line:
        break
    print(line.strip())
    if line.startswith("bestmove"):
        break

send(p, "quit")
p.wait()
