#!/usr/bin/env python3
from pathlib import Path

path = Path(__file__).resolve().parents[1] / "tests/native_installer_safety.c"
data = path.read_bytes()
count = data.count(b"\x00")
if count != 2:
    raise SystemExit(f"Expected exactly 2 accidental NUL bytes, found {count}")
path.write_bytes(data.replace(b"\x00", b"\\0"))
