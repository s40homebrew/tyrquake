/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RENDER_S40_H
#define RENDER_S40_H
#include <stdint.h>

/* Convert inverse depth to Quake's 1.31 accumulator. Scaling a binary32
 * value by 2^31 is exact in this range, so extract the integer directly.
 * Keep the original soft-float path for non-finite/out-of-range inputs. */
static inline int S40_DepthFixed(float value) {
    union { float f; uint32_t u; } bits = { value };
    unsigned exponent = (bits.u >> 23) & 255;
    if (exponent < 96) return 0;
    if (exponent >= 127) return (int)(value * 32768.0f * 65536.0f);
    uint32_t magnitude = (bits.u & 0x7fffff) | 0x800000;
    magnitude = exponent >= 119 ? magnitude << (exponent - 119)
                               : magnitude >> (119 - exponent);
    return (int)(bits.u >> 31 ? 0u - magnitude : magnitude);
}

/* Exact truncating signed division for the short tail of a pixel span.
 * A rounded-down reciprocal can underestimate by one; correct that using
 * the integer remainder. ARM11 has UMULL, but no integer divide instruction. */
static inline int S40_SpanDivide(int value, unsigned divisor) {
    static const uint32_t reciprocal[32] = {
        0, 0, 2147483648u, 1431655765u, 1073741824u, 858993459u,
        715827882u, 613566756u, 536870912u, 477218588u, 429496729u,
        390451572u, 357913941u, 330382099u, 306783378u, 286331153u,
        268435456u, 252645135u, 238609294u, 226050910u, 214748364u,
        204522252u, 195225786u, 186737708u, 178956970u, 171798691u,
        165191049u, 159072862u, 153391689u, 148102320u, 143165576u,
        138547332u
    };
    if (divisor == 1) return value;
    if (divisor < 2 || divisor > 31) return value / (int)divisor;
    uint32_t magnitude = value < 0 ? 0u - (uint32_t)value : (uint32_t)value;
    uint32_t quotient = ((uint64_t)magnitude * reciprocal[divisor]) >> 32;
    quotient += magnitude - quotient * divisor >= divisor;
    return (int)(value < 0 ? 0u - quotient : quotient);
}
#endif
