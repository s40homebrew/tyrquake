#!/usr/bin/env python3
"""Generate veneers for manually audited RM-839 02.33 functions.
Names describe recovered roles, not an original export table. The launcher
checks the matching short firmware fingerprints on the phone.
"""

import json
from pathlib import Path

P = Path(__file__).resolve().parents[1]
abi = json.loads((P / "docs/abi.json").read_text())
from quake_paths import SDK_ROOT

abi.update(
    json.loads((SDK_ROOT / "homebrew/runtime/audio/rm839_0233_abi.json").read_text())
)
s = [".syntax unified", ".arm", ".text"]
imports = {}
for name, record in abi.items():
    address = int(record["address"], 16)
    imports[name] = address
    s += [
        f'.section .text.{name},"ax",%progbits',
        ".balign 4",
        f".global {name}",
        f".hidden {name}",
        f".type {name},%function",
        f"{name}:",
        "ldr ip,[pc,#0]",
        "bx ip",
        f".word 0x{address:08x}",
        f".size {name},.-{name}",
    ]
(P / "build/imports.S").write_text("\n".join(s) + "\n")
(P / "build/imports.json").write_text(json.dumps(imports, indent=2) + "\n")
print("Generated", len(imports), "RM-839 02.33 ABI entries")
