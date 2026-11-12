"""Compile and run the C++ text trial on Cloud. This does not invoke Unreal Engine."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from mvp_fixture import ROOT, load_fixture, write_cpp_fixture


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++"), help="C++17 compiler executable on Cloud")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--demo", action="store_true", help="Automatically complete one deterministic sample round")
    mode.add_argument("--endgame-demo", action="store_true", help="Complete the normal round and three sequential endgame contracts")
    parser.add_argument("--sanitize", action="store_true", help="Enable UndefinedBehaviorSanitizer")
    args = parser.parse_args()
    compiler = shutil.which(args.cxx)
    if compiler is None:
        parser.error(f"Compiler not found on Cloud: {args.cxx}")
    try:
        fixture = load_fixture()
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.error(f"Invalid sample data: {error}")
    with tempfile.TemporaryDirectory(prefix="laststorage-session-") as folder:
        write_cpp_fixture(Path(folder) / "LastStorageMvpFixture.h", fixture)
        executable = Path(folder) / "laststorage_text_trial"
        include = ROOT / "Plugins/LastStorage/Source/LastStorageRuntime/Public"
        command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic", "-fno-exceptions", "-fno-rtti",
                   "-finput-charset=UTF-8", "-fexec-charset=UTF-8"]
        if args.sanitize:
            command += ["-fsanitize=undefined", "-fno-sanitize-recover=all", "-g"]
        source = "endgame_demo.cpp" if args.endgame_demo else "session_demo.cpp"
        command += ["-I", str(include), "-I", folder, str(ROOT / "Examples" / source), "-o", str(executable)]
        print("Building LAST STORAGE C++ text trial on Cloud (not a UE build)", flush=True)
        build = subprocess.run(command, check=False)
        if build.returncode:
            return build.returncode
        return subprocess.run([str(executable)] + (["--demo"] if args.demo else []), check=False).returncode


if __name__ == "__main__":
    raise SystemExit(main())
