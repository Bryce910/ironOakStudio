#!/usr/bin/env python3
"""Build the pinned CPU Jolt component used by Valwood's custom runtime.

Source is vendored under third_party/jolt, including its MIT license. No network,
GPU backend, sample assets or install/system-directory changes are required.
"""
import argparse
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--configure-only", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.jobs <= 8:
        parser.error("--jobs must be1..8")
    root = Path(__file__).resolve().parents[1]
    source = root / "third_party/jolt/Build"
    output = root / "build/physics"
    definitions = {
        "CMAKE_BUILD_TYPE": "Release", "DOUBLE_PRECISION": "ON",
        "GENERATE_DEBUG_SYMBOLS": "OFF", "INTERPROCEDURAL_OPTIMIZATION": "OFF",
        "JPH_BUILD_SHARED_LIBS": "OFF", "ENABLE_INSTALL": "OFF",
        "DEBUG_RENDERER_IN_DEBUG_AND_RELEASE": "OFF",
        "PROFILER_IN_DEBUG_AND_RELEASE": "OFF", "ENABLE_OBJECT_STREAM": "OFF",
        "TARGET_UNIT_TESTS": "OFF", "TARGET_HELLO_WORLD": "OFF",
        "TARGET_PERFORMANCE_TEST": "OFF", "TARGET_SAMPLES": "OFF", "TARGET_VIEWER": "OFF",
        "JPH_USE_DX12": "OFF", "JPH_USE_VK": "OFF", "JPH_USE_MTL": "OFF",
        "JPH_USE_CPU_COMPUTE": "OFF", "USE_SSE4_1": "OFF", "USE_SSE4_2": "OFF",
        "USE_AVX": "OFF", "USE_AVX2": "OFF", "USE_AVX512": "OFF", "USE_LZCNT": "OFF",
        "USE_TZCNT": "OFF", "USE_F16C": "OFF", "USE_FMADD": "OFF",
    }
    command = ["cmake", "-S", str(source), "-B", str(output)]
    command += [f"-D{name}={value}" for name, value in definitions.items()]
    subprocess.run(command, check=True)
    if not args.configure_only:
        subprocess.run(["cmake", "--build", str(output), "--target", "Jolt", "--parallel", str(args.jobs)], check=True)


if __name__ == "__main__":
    main()
