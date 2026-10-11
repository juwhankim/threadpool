"""Build the standalone C++17 examples, strict-warning tests and benchmark."""
import argparse
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--compiler", default="g++")
parser.add_argument("--test", action="store_true", help="Run correctness checks after building.")
args = parser.parse_args()
build = root / "build"
build.mkdir(exist_ok=True)
suffix = ".exe" if os.name == "nt" else ""
# Treat warning diagnostics as errors in every standalone translation unit.
flags = ["-std=c++17", "-O2", "-pthread", "-Wall", "-Wextra", "-Wpedantic",
         "-Wconversion", "-Wshadow", "-Werror", "-Iinclude"]
for name, source in [("threadpool_tests", "test/correctness.cpp"),
                     ("threadpool_benchmark", "test/benchmark.cpp"),
                     ("threadpool_example", "examples/basic.cpp")]:
    output = build / (name + suffix)
    subprocess.run([args.compiler, *flags, source, "-o", str(output)], cwd=root, check=True)
    print(f"Built {output}", flush=True)
if args.test:
    subprocess.run([str(build / ("threadpool_tests" + suffix))], cwd=root, check=True)
    subprocess.run([str(build / ("threadpool_example" + suffix))], cwd=root, check=True)
