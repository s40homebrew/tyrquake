#!/usr/bin/env python3
"""Reject incompatible CLDC classes, carriers and native payloads."""
import json
import os
from pathlib import Path
import struct
import subprocess
import zipfile

P = Path(__file__).resolve().parents[1]
B = P / 'build'
jar = P / 'dist/RM839NativeQuake.jar'


def require(ok, message):
    if not ok:
        raise SystemExit('Invalid build: ' + message)


with zipfile.ZipFile(jar) as archive:
    require(archive.testzip() is None, 'JAR CRC')
    for name in archive.namelist():
        if name.endswith('.class'):
            require(archive.read(name)[:8] == bytes.fromhex('cafebabe0003002d'),
                    'CLDC 45.3 class: ' + name)
    outer = archive.read('QuakeLauncher.class')
    require(b'StackMap' in outer, 'missing CLDC stack maps')
    for name in ['native.bin', 'quake.bin']:
        require(archive.read(name) == (B / name).read_bytes(), 'packaged ' + name)

# The native entry lives in the compiler's lookupswitch table. Its exact
# layout and Thumb branch targets must survive Java compilation/preverification.
words = [int(x, 16) for x in json.loads((B / 'carrier_layout.json').read_text())['native_words']]
carrier = bytearray(bytes.fromhex('1aab0000') + struct.pack('>II', 111, 10))
for i, word in enumerate(words):
    carrier += struct.pack('<I', word) + struct.pack('>I', 91 + 2 * i)
carrier += bytes.fromhex('03ac') * 10 + bytes.fromhex('04ac')
require(outer.count(carrier) == 1, 'compiled switch carrier')
for source, target in [(0, 8), (1, 6), (2, 9), (3, 7), (4, 5), (5, 1), (6, 2), (7, 4), (8, 3)]:
    offset = 14 + source * 8
    branch = struct.unpack_from('<H', carrier, offset)[0]
    displacement = ((branch & 0x7ff) ^ 0x400) - 0x400
    require(branch & 0xf800 == 0xe000 and offset + 4 + displacement * 2 == 12 + target * 8,
            f'carrier branch {source}->{target}')
require(words[9] == 0xbd10, 'carrier return')

native = (B / 'native.bin').read_bytes()
require(0 < len(native) <= 3072 and len(native) % 4 == 0, 'loader size/alignment')
llvm = Path(os.environ['LLVM'])
relocations = subprocess.check_output([str(llvm / 'llvm-readobj'), '--relocations', str(B / 'loader.elf')], text=True)
require('Relocations [\n]' in relocations, 'loader needs relocation')
data = (B / 'quake.bin').read_bytes()
magic, version, size, memory, entry, count, stack, reserved = struct.unpack_from('<8I', data)
require(magic == 0x30443453 and version == 1 and stack == 196608 and reserved == 0, 'image header')
require(len(data) == 32 + size + 4 * count and 0 < entry < size <= memory < 3 * 1024 * 1024, 'image bounds')
jad = jar.with_suffix('.jad').read_text()
require(f'MIDlet-Jar-Size: {jar.stat().st_size}\n' in jad and
        f'MIDlet-Jar-URL: {jar.name}\n' in jad, 'JAD metadata')
print('Verified CLDC carrier and native package.')
