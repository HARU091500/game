"""Build/run the portable C++ core in a temporary directory. No Unreal is invoked."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++"), help="C++17 compiler executable (GCC/Clang)")
    parser.add_argument("--sanitize", action="store_true", help="Enable UndefinedBehaviorSanitizer, including signed overflow")
    parser.add_argument("--suite", choices=("economy", "session", "endgame", "all"), default="economy", help="Portable core test suite (default preserves economy-only behavior)")
    args = parser.parse_args()
    compiler = shutil.which(args.cxx)
    if compiler is None:
        parser.error(f"Compiler not found in Cloud: {args.cxx}")
    root = Path(__file__).resolve().parents[1]
    include = root / "Plugins/LastStorage/Source/LastStorageRuntime/Public"
    with tempfile.TemporaryDirectory(prefix="laststorage-economy-") as folder:
        suites = ("economy", "session", "endgame") if args.suite == "all" else (args.suite,)
        for suite in suites:
            executable = Path(folder) / (suite + "_tests")
            command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic", "-fno-exceptions", "-fno-rtti"]
            if args.sanitize:
                command += ["-fsanitize=undefined", "-fno-sanitize-recover=all", "-g"]
            command += ["-I", str(include), str(root / "Tests" / (suite + "_tests.cpp")), "-o", str(executable)]
            print(f"Building portable C++17 {suite} tests (not an Unreal build)", flush=True)
            build = subprocess.run(command, check=False)
            if build.returncode != 0:
                return build.returncode
            run = subprocess.run([str(executable)], check=False)
            if run.returncode != 0:
                return run.returncode
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
