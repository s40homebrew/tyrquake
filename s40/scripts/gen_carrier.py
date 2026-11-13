#!/usr/bin/env python3
"""Generate and verify the RM-839 02.33 Thumb lookupswitch carrier.

Each Java match value is the byte-swapped native little-endian word. Sorted
match entries contain one useful Thumb instruction and one branch. Native
branches establish execution order independently of Java's signed case order.
Four otherwise skipped JVM jump-offset words supply the real even cache start,
positive byte length, ARM cache helper, and independent ARM/Thumb entry.
"""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]

# Sorted layout, determined by byte-swapped signed Java match keys.
# useful instruction, branch destination entry (None = final return)
ENTRIES = [
    (0x4280, 8),   # cmp r0,r0; go to push (first, harmless sorted key)
    (0x4790, 6),   # blx r2; after cache helper, load the native entry
    (0x4798, 9),   # blx r3; after payload, go to pop {r4,pc}
    (0x4800, 7),   # ldr r0,[pc,#0]; real even payload/cache start at +40
    (0x4900, 5),   # ldr r1,[pc,#0]; positive payload byte length at +48
    (0x4A00, 1),   # ldr r2,[pc,#0]; ARM cache helper at +56
    (0x4B00, 2),   # ldr r3,[pc,#0]; independent native entry at +64
    (0x1C04, 4),   # movs r4,r0; preserve start across the cache call
    (0xB510, 3),   # push {r4,lr}; preserves alignment and return
    (0xBD10, None),# pop {r4,pc}; return through the ARM11 native bridge
]


def bswap32(value):
    return int.from_bytes(value.to_bytes(4, "little"), "big")


def signed(value):
    return value - 0x100000000 if value & 0x80000000 else value


def build():
    native = []
    matches = []
    for i, (instruction, destination) in enumerate(ENTRIES):
        address = 12 + 8 * i
        if destination is None:
            branch = 0
        else:
            target = 12 + 8 * destination
            displacement = target - (address + 2 + 4)
            assert displacement % 2 == 0
            immediate = displacement // 2
            # Thumb B T2 has a signed 11-bit immediate (in halfwords).
            # An 8-bit mask turns backward branches into forward jumps.
            assert -1024 <= immediate <= 1023
            branch = 0xE000 | (immediate & 0x7FF)
        native_word = (branch << 16) | instruction
        match = bswap32(native_word)
        native.append(native_word)
        matches.append(match)
    assert matches == sorted(matches, key=signed)
    assert len(set(matches)) == len(matches)
    return native, matches


def main():
    native, matches = build()
    cases = "\n".join(f"            case {signed(match)}: return 0;" for match in matches)
    expected = "\n".join(
        f"            if (index == {i}) return 0x{word:08X};" for i, word in enumerate(native)
    )
    template = (ROOT / "src/QuakeLauncher.java.in").read_text()
    template = template.replace("            // @CARRIER_CASES@", cases)
    template = template.replace("            // @EXPECTED_CARRIER_WORDS@", expected)
    abi=json.loads((ROOT/'docs/abi.json').read_text())
    checks=[]
    for name,record in abi.items():
        address=int(record['address'],16)&~3
        word=int(record['aligned_word'],16)
        checks.append(f'        if (readWord(0x{address:08X}) != 0x{word:08X}) return 17; // {name}')
    template=template.replace('        // @ROM_CHECKS@','\n'.join(checks))
    (ROOT / "build/QuakeLauncher.java").write_text(template)
    layout = {
        "entry_offset": 12,
        "literal_offsets": [40, 48, 56, 64],
        "native_words": [f"0x{x:08X}" for x in native],
        "java_match_values": [signed(x) for x in matches],
    }
    (ROOT / "build/carrier_layout.json").write_text(json.dumps(layout, indent=2) + "\n")
    print("Generated 10-entry Thumb carrier; literals at bytecode offsets 40, 48, 56, 64")


if __name__ == "__main__":
    main()
