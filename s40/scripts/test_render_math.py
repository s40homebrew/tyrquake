#!/usr/bin/env python3
"""Compare renderer arithmetic against the original using ARM soft-float code."""

import os
import struct
import subprocess
from pathlib import Path

from quake_paths import llvm_tool
from unicorn import UC_ARCH_ARM, UC_MODE_ARM, Uc
from unicorn.arm_const import UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_SP

P = Path(__file__).resolve().parents[1]
B = P / "build/render-math"
B.mkdir(parents=True, exist_ok=True)
source = B / "check.c"
source.write_text(r"""
#include "render_s40.h"
#include "span_math_s40.h"
static uint32_t seed = 0x3010233;
static uint32_t random_word(void) {
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    return seed;
}
__attribute__((noinline)) static int original_depth(float value) {
    double zi = value;
    return (int)(zi * 32768 * 65536);
}
__attribute__((noinline)) static int original_divide(int n, int d) { return n / d; }
static int difference(int a, int b) { return a > b ? a - b : b - a; }
static int check_planes(void) {
    for (unsigned i = 0; i < 65536; ++i) {
        /* Cover every table interval and both ends of the normalized range. */
        uint32_t denominator = 0x40000000u + (i << 14);
        for (unsigned j = 0; j < 3; ++j) {
            uint32_t d = denominator + (j == 0 ? 0 : j == 1 ? 16383 : random_word() & 16383);
            uint32_t exact = ((uint64_t)1 << 61) / d;
            if (difference(S40_Reciprocal(d), exact) > 3) return 6;
        }
    }
    for (unsigned i = 0; i < 8192; ++i) {
        /* Depths 2..32768, signed texture coefficients and oblique planes.
         * Compare to double evaluation, independently of the integer path. */
        float zo = 1.0f / (float)(2u << (i % 15));
        float zu = zo * ((int)(random_word() & 255) - 128) / 131072.0f;
        float zv = zo * ((int)(random_word() & 255) - 128) / 131072.0f;
        float so = zo * ((int)(random_word() & 32767) - 16384);
        float su = zo * ((int)(random_word() & 1023) - 512) / 16.0f;
        float sv = zo * ((int)(random_word() & 1023) - 512) / 16.0f;
        s40_plane z, s;
        if (!S40_MakePlane(&z, zo, zu, zv) || !S40_MakePlane(&s, so, su, sv)) return 7;
        for (int j = 0; j < 4; ++j) {
            int u = j == 0 ? 0 : j == 1 ? 255 : random_word() % 240;
            int v = j == 0 ? 0 : j == 1 ? 319 : random_word() % 320;
            double zd = (double)zo + (double)zu * u + (double)zv * v;
            double sd = (double)so + (double)su * u + (double)sv * v;
            int depth;
            if (!S40_PlaneDepth(&z, u, v, &depth) || difference(depth, (int)(zd * 2147483648.0)) > 1) return 8;
            int shift;
            uint32_t reciprocal = S40_Reciprocal(S40_Normalize(S40_PlaneAt(&z, u, v), &shift));
            double expected = sd / zd * 65536.0;
            if (expected < -2147483647.0 || expected > 2147483647.0) continue;
            int fixed = S40_TextureCoordinate(S40_PlaneAt(&s, u, v), s.scale, reciprocal,
                                             z.scale + shift, 0, -2147483647, 2147483647);
            if (difference(fixed, (int)expected) > 8) return 9;
            /* Texture edge clamps and large positive/negative offsets. */
            int adjust = (int)((random_word() & 32767) << 16) - 1073741824;
            fixed = S40_TextureCoordinate(S40_PlaneAt(&s, u, v), s.scale, reciprocal,
                                          z.scale + shift, adjust, 16, 16777215);
            expected = (int)expected + (double)adjust;
            int clamped = expected < 16 ? 16 : expected > 16777215 ? 16777215 : (int)expected;
            if (difference(fixed, clamped) > 8) return 10;
        }
    }
    s40_plane p;
    union { uint32_t u; float f; } nonfinite = {0x7f800000};
    if (S40_MakePlane(&p, nonfinite.f, 0, 0)) return 11;
    nonfinite.u = 0x7fc00000;
    if (S40_MakePlane(&p, 0, nonfinite.f, 0)) return 12;
    nonfinite.u = 1;
    if (S40_MakePlane(&p, 0, 0, nonfinite.f)) return 15;
    int depth;
    S40_MakePlane(&p, 2, 0, 0);
    if (S40_PlaneDepth(&p, 0, 0, &depth)) return 13;
    S40_MakePlane(&p, -0.9f, 0, 0);
    if (!S40_PlaneDepth(&p, 0, 0, &depth) || depth != original_depth(-0.9f)) return 14;
    return 0;
}
int check(void) {
    /* Every finite exponent below 1, both signs; mantissa endpoints and
     * deterministic random mantissas. Also the valid negative -1 endpoint. */
    for (unsigned exponent = 0; exponent < 127; exponent++) {
        for (unsigned sign = 0; sign < 2; sign++) {
            for (unsigned i = 0; i < 1024; i++) {
                union { uint32_t u; float f; } bits;
                unsigned fraction = i == 0 ? 0 : i == 1 ? 0x7fffff : random_word() & 0x7fffff;
                bits.u = (sign << 31) | (exponent << 23) | fraction;
                if (S40_DepthFixed(bits.f) != original_depth(bits.f)) return 1;
            }
        }
    }
    if (S40_DepthFixed(-1.0f) != original_depth(-1.0f)) return 2;
    const int endpoints[] = {0, 1, -1, 2147483647, (-2147483647 - 1)};
    for (unsigned d = 1; d <= 31; d++) {
        for (unsigned i = 0; i < 4096; i++) {
            int n = i < 5 ? endpoints[i] : (int)random_word();
            if (S40_SpanDivide(n, d) != original_divide(n, d)) return 3;
            /* Values immediately around exact multiples exercise the
             * reciprocal's one-unit correction on either side of zero. */
            n = (int)((random_word() & 0xffff) * d);
            for (int delta = -1; delta <= 1; delta++) {
                if (S40_SpanDivide(n + delta, d) != original_divide(n + delta, d)) return 4;
                if (S40_SpanDivide(-n + delta, d) != original_divide(-n + delta, d)) return 5;
            }
        }
    }
    return check_planes();
}
""")
cc = os.environ.get("CC", llvm_tool("clang"))
ld = os.environ.get("LD", llvm_tool("ld.lld"))
gcc = os.environ.get("ARM_GCC", "arm-none-eabi-gcc")
libgcc = subprocess.check_output([gcc, "-print-file-name=libgcc.a"], text=True).strip()
subprocess.run(
    [
        cc,
        "-target",
        "arm-none-eabi",
        "-march=armv6",
        "-marm",
        "-mfloat-abi=soft",
        "-O3",
        "-ffreestanding",
        "-fno-builtin",
        "-fwrapv",
        "-fno-strict-aliasing",
        "-ffunction-sections",
        "-fdata-sections",
        "-ffile-prefix-map=" + str(P.parent) + "=.",
        "-I" + str(P / "native"),
        "-c",
        str(source),
        "-o",
        str(B / "check.o"),
    ],
    check=True,
)
subprocess.run(
    [
        ld,
        "-Ttext=0x10000",
        "-e",
        "check",
        "--gc-sections",
        str(B / "check.o"),
        libgcc,
        "-o",
        str(B / "check.elf"),
    ],
    check=True,
)
elf = (B / "check.elf").read_bytes()
entry, phoff = struct.unpack_from("<II", elf, 24)
phsize, phnum = struct.unpack_from("<HH", elf, 42)
u = Uc(UC_ARCH_ARM, UC_MODE_ARM)
u.mem_map(0x10000, 0x100000)
u.mem_map(0x200000, 0x10000)
for i in range(phnum):
    kind, offset, address, _, size, _, _, _ = struct.unpack_from(
        "<8I", elf, phoff + i * phsize
    )
    if kind == 1 and size:
        u.mem_write(address, elf[offset : offset + size])
stop = 0x20FFF0
u.reg_write(UC_ARM_REG_SP, 0x20F000)
u.reg_write(UC_ARM_REG_LR, stop)
u.emu_start(entry, stop, timeout=10_000_000)
assert u.reg_read(UC_ARM_REG_PC) == stop, "Arithmetic test watchdog expired"
assert u.reg_read(UC_ARM_REG_R0) == 0, (
    f"Arithmetic mismatch: {u.reg_read(UC_ARM_REG_R0)}"
)
print(
    "PASS: 260,097 depth conversions and 888,832 signed span divisions match original ARM arithmetic."
)
print(
    "PASS: 196,608 reciprocals and 32,768 plane projections/depths against independent division/double references; clamps and fallback inputs."
)
