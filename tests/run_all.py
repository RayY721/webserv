#!/usr/bin/env python3
"""Run every test in tests/test_*.py.

A test is any function whose name starts with "test_".
It passes if it finishes without raising an exception,
and fails if an assert (or anything else) goes wrong.
"""

import importlib.util
import pathlib
import sys
import traceback

TESTS_DIR = pathlib.Path(__file__).resolve().parent


def load_module(path):
    spec = importlib.util.spec_from_file_location(path.stem, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    passed = 0
    failed = 0

    for path in sorted(TESTS_DIR.glob("test_*.py")):
        module = load_module(path)

        for name in sorted(dir(module)):
            test = getattr(module, name)
            if not name.startswith("test_") or not callable(test):
                continue

            label = f"{path.name}::{name}"
            try:
                test()
            except Exception:
                failed += 1
                print(f"FAIL  {label}")
                traceback.print_exc(limit=1)
            else:
                passed += 1
                print(f"PASS  {label}")

    print()
    print(f"{passed} passed, {failed} failed")

    if failed > 0:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
