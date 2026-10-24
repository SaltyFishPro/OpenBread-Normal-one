#!/usr/bin/env python3
"""Check that firmware logging goes through the unified src/log/Log.h macros.

Any Serial.print/printf/println/vprintf outside src/log/Log.h is a
violation. Run from the repository root:

    python tools/log_check.py
"""

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
ALLOWED = {"src/log/Log.h"}
PATTERN = re.compile(r"\bSerial\s*\.\s*(?:print|printf|println|vprintf)\b")


def main() -> int:
    violations = []
    for path in sorted(SRC.rglob("*")):
        if path.suffix not in (".cpp", ".h", ".hpp", ".ino"):
            continue
        rel = path.relative_to(ROOT).as_posix()
        if rel in ALLOWED:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for lineno, line in enumerate(text.splitlines(), 1):
            if PATTERN.search(line):
                violations.append(f"{rel}:{lineno}: {line.strip()}")
    if violations:
        print("Direct Serial log calls outside src/log/Log.h:")
        print("\n".join(violations))
        return 1
    print("OK: no direct Serial log calls outside src/log/Log.h")
    return 0


if __name__ == "__main__":
    sys.exit(main())
