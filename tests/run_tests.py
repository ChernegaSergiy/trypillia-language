#!/usr/bin/env python3
"""
Test runner for Trypillia language.
Runs each .try file through the interpreter and compares output to expected.
"""

import subprocess
import sys
import os
import re

TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
EXPECTED_DIR = os.path.join(TESTS_DIR, "expected")
INTERPRETER = os.path.join(TESTS_DIR, "..", "build", "trypillia")

GREEN = "\033[92m"
RED = "\033[91m"
YELLOW = "\033[93m"
RESET = "\033[0m"

def get_test_files():
    files = sorted(f for f in os.listdir(TESTS_DIR) if f.startswith("test_") and f.endswith(".try"))
    return files

def get_expected_file(test_file):
    return os.path.join(EXPECTED_DIR, test_file.replace(".try", ".txt"))

def run_test(test_file):
    try_path = os.path.join(TESTS_DIR, test_file)
    expected_file = get_expected_file(test_file)

    if not os.path.exists(expected_file):
        return None, None, f"Missing expected output file: {expected_file}"

    result = subprocess.run(
        [INTERPRETER, try_path],
        capture_output=True,
        text=True,
        timeout=30
    )

    actual = result.stdout
    # normalize trailing whitespace
    actual = actual.strip() + "\n"

    with open(expected_file, "r") as f:
        expected = f.read()
    expected = expected.strip() + "\n"

    actual_lines = actual.splitlines()
    expected_lines = expected.splitlines()

    if len(actual_lines) != len(expected_lines):
        return actual, expected, f"Line count mismatch: got {len(actual_lines)}, expected {len(expected_lines)}"

    for i, (a, e) in enumerate(zip(actual_lines, expected_lines)):
        if a != e:
            return actual, expected, f"Line {i+1} mismatch:\n  expected: {e!r}\n  actual:   {a!r}"

    return actual, expected, None

def main():
    if not os.path.exists(INTERPRETER):
        print(f"{RED}Error: Interpreter not found at {INTERPRETER}{RESET}")
        print("Build the project first: cmake --build build")
        sys.exit(1)

    tests = get_test_files()
    if not tests:
        print(f"{YELLOW}No test files found in {TESTS_DIR}{RESET}")
        sys.exit(0)

    passed = 0
    failed = 0
    missing = 0

    for test_file in tests:
        actual, expected, error = run_test(test_file)
        if error:
            if "Missing expected output" in error:
                print(f"{YELLOW}MISSING{r'es'} {test_file}")
                missing += 1
            else:
                print(f"{RED}FAIL  {RESET}{test_file}")
                print(f"  {error}")
                if actual is not None and expected is not None:
                    print(f"  --- actual ---\n{actual.rstrip()}")
                    print(f"  --- expected ---\n{expected.rstrip()}")
                failed += 1
        else:
            print(f"{GREEN}PASS  {RESET}{test_file}")
            passed += 1

    print(f"\n{'='*40}")
    total = passed + failed + missing
    print(f"Total: {total} | {GREEN}Passed: {passed}{RESET} | {RED}Failed: {failed}{RESET} | {YELLOW}Missing: {missing}{RESET}")

    return 0 if failed == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
