"""Runs every debugger suite. Exits non-zero if any check failed.

    GODOT_BIN=<godot editor binary> python tests/debugger/run.py [-v] [suite ...]
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import test_breakpoints
import test_exceptions
import test_stepping

SUITES = {
    "breakpoints": test_breakpoints.run,
    "stepping": test_stepping.run,
    "exceptions": test_exceptions.run,
}


def main(argv):
    verbose = "-v" in argv or "--verbose" in argv
    wanted = [a for a in argv if not a.startswith("-")] or list(SUITES)

    unknown = [name for name in wanted if name not in SUITES]
    if unknown:
        raise SystemExit(f"unknown suite(s): {', '.join(unknown)}\navailable: {', '.join(SUITES)}")

    results = [SUITES[name](verbose=verbose) for name in wanted]

    checks = sum(r.checks for r in results)
    failures = [(r.name, f) for r in results for f in r.failures]
    print(f"\n{checks - len(failures)}/{checks} checks passed", flush=True)
    for name, failure in failures:
        print(f"  FAIL  [{name}] {failure}", flush=True)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
