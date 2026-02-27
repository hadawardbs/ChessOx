import os
import subprocess
import sys
import platform

def build():
    print(">>> Oxta Build System <<<")

    # 1. Create Build Directory
    if not os.path.exists("build"):
        os.makedirs("build")
        print("Created build/ directory.")

    # 2. Run CMake
    print("Running CMake...")
    try:
        if platform.system() == "Windows":
            subprocess.check_call(["cmake", ".."], cwd="build", shell=True)
        else:
            subprocess.check_call(["cmake", ".."], cwd="build")
    except subprocess.CalledProcessError:
        print("Error: CMake failed. Please ensure CMake is installed.")
        sys.exit(1)

    # 3. Compile
    print("Compiling...")
    try:
        if platform.system() == "Windows":
            subprocess.check_call(["cmake", "--build", ".", "--config", "Release"], cwd="build", shell=True)
        else:
            subprocess.check_call(["make", "-j4"], cwd="build")
    except subprocess.CalledProcessError:
        print("Error: Compilation failed.")
        sys.exit(1)

    print("\n>>> Build Successful! <<<")
    print("Binary located at: build/OxtaCore")

if __name__ == "__main__":
    build()
