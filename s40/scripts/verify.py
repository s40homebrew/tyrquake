#!/usr/bin/env python3
"""Static release checks only: no ARM code, JAR, or firmware is executed."""

import json
import struct
import subprocess
import zipfile
from pathlib import Path

from quake_paths import llvm_tool


P = Path(__file__).resolve().parents[1]
B = P / "build"
jar = P / "dist/RM839NativeQuake.jar"
checks = 0


def require(ok, message):
    global checks
    if not ok:
        raise SystemExit("FAIL: " + message)
    checks += 1


with zipfile.ZipFile(jar) as archive:
    require(archive.testzip() is None, "JAR CRC")
    require("META-INF/MANIFEST.MF" in archive.namelist(), "MIDlet manifest")
    for info in archive.infolist():
        data = archive.read(info)
        require(
            not any(s in data for s in [b"/Users/", b"/home/"]),
            "no builder metadata: " + info.filename,
        )
        if info.filename.endswith(".class"):
            require(
                data[:8] == bytes.fromhex("cafebabe0003002d"),
                "preverified CLDC 45.3: " + info.filename,
            )
    outer = archive.read("QuakeLauncher.class")
    require(b"StackMap" in outer, "launcher has CLDC stack maps")
    for name in ["FirmwareResolver.class", "Rm839Memory.class", "AudioPolicyBindings.class"]:
        require(name in archive.namelist(), "packaged audio discovery dependency: " + name)
    for name in ["native.bin", "quake.bin"]:
        require(archive.read(name) == (B / name).read_bytes(), "packaged " + name)

# Match actual class bytes, including JVM jump offsets, rather than just a
# generated assembly file. This catches the old 8-bit backward-branch bug.
layout = json.loads((B / "carrier_layout.json").read_text())
words = [int(x, 16) for x in layout["native_words"]]
carrier = bytearray(bytes.fromhex("1aab0000") + struct.pack(">II", 111, 10))
for i, word in enumerate(words):
    carrier += struct.pack("<I", word) + struct.pack(">I", 91 + 2 * i)
carrier += bytes.fromhex("03ac") * 10 + bytes.fromhex("04ac")
require(outer.count(carrier) == 1, "exact 114-byte compiled switch carrier")
for source, target in [
    (0, 8),
    (1, 6),
    (2, 9),
    (3, 7),
    (4, 5),
    (5, 1),
    (6, 2),
    (7, 4),
    (8, 3),
]:
    offset = 14 + source * 8
    branch = struct.unpack_from("<H", carrier, offset)[0]
    displacement = ((branch & 0x7FF) ^ 0x400) - 0x400
    require(
        branch & 0xF800 == 0xE000 and offset + 4 + displacement * 2 == 12 + target * 8,
        f"carrier branch {source}->{target}",
    )
require(words[9] == 0xBD10, "carrier returns through pop r4,pc")

native = (B / "native.bin").read_bytes()
require(0 < len(native) <= 3072 and len(native) % 4 == 0, "loader bounds/alignment")
relocations = subprocess.check_output(
    [llvm_tool("llvm-readobj"), "--relocations", str(B / "loader.elf")], text=True
)
require("Relocations [\n]" in relocations, "self-relocating loader has no fixups")
symbols = subprocess.check_output(
    [llvm_tool("llvm-nm"), "-u", str(B / "quake.elf")], text=True
)
require(not symbols.strip(), "no unresolved game imports")
imports = json.loads((B / "imports.json").read_text())
require(not any(name.startswith("fw_audio_") for name in imports),
        "audio is resolved at runtime, not emitted as fixed veneers")
data = (B / "quake.bin").read_bytes()
magic, version, size, memory, entry, count, stack, reserved = struct.unpack_from(
    "<8I", data
)
require(
    magic == 0x30443453 and version == 1 and stack == 196608 and reserved == 0,
    "image header",
)
require(
    len(data) == 32 + size + 4 * count
    and 0 <= entry < size <= memory <= 3 * 1024 * 1024
    and memory % 4 == 0
    and count <= 65536,
    "image sizes",
)
rel = struct.unpack_from(f"<{count}I", data, 32 + size)
require(
    len(set(rel)) == count and all(x % 4 == 0 and x + 4 <= size for x in rel),
    "unique initialized relative fixups",
)
jad = jar.with_suffix(".jad").read_text()
require(
    f"MIDlet-Jar-Size: {jar.stat().st_size}\n" in jad
    and f"MIDlet-Jar-URL: {jar.name}\n" in jad,
    "JAD artifact metadata",
)
print(
    f"{checks} static checks passed; {jar.stat().st_size} byte JAR, {len(native)} byte loader."
)
print("Package checks only; handset testing remains separate.")
