#!/usr/bin/env python3
from pathlib import Path

path = Path(__file__).resolve().parents[2] / ".github/workflows/ci.yml"
text = path.read_text(encoding="utf-8")
old = "          grep -Fq '/usr/bin/system-monitor --install-capability' \"$tmp/control/postinst\"\n"
new = "          grep -Fq '/Programs/system-monitor --install-capability' \"$tmp/control/postinst\"\n"
if text.count(old) != 1:
    raise SystemExit("Unexpected System Monitor postinst release gate")
path.write_text(text.replace(old, new, 1), encoding="utf-8")
