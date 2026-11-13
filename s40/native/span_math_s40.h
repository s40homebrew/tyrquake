/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SPAN_MATH_S40_H
#define SPAN_MATH_S40_H
#include <stdint.h>

/* A surface's three binary32 coefficients share a binary exponent. Keep
 * 44 significant integer bits, leaving room for 240x320 screen coordinates
 * and cancellation. Setup is per surface, not per scanline/pixel. */
typedef struct { int64_t origin, du, dv; int scale; } s40_plane;

static inline int64_t S40_PlaneCoefficient(uint32_t bits, unsigned maxexp) {
    unsigned exponent = (bits >> 23) & 255;
    if (!exponent) return 0;
    uint64_t magnitude = (bits & 0x7fffff) | 0x800000;
    int shift = 20 + (int)exponent - (int)maxexp;
    magnitude = shift >= 0 ? magnitude << shift :
                shift > -64 ? magnitude >> -shift : 0;
    return bits >> 31 ? -(int64_t)magnitude : (int64_t)magnitude;
}

static inline int S40_MakePlane(s40_plane *p, float origin, float du, float dv) {
    union { float f; uint32_t u; } a = {origin}, b = {du}, c = {dv};
    unsigned ae = (a.u >> 23) & 255, be = (b.u >> 23) & 255, ce = (c.u >> 23) & 255;
    unsigned e = ae > be ? ae : be;
    if (ce > e) e = ce;
    if (e == 255) return 0;
    /* Subnormal coefficients are outside the map renderer's normal range;
     * let the original float path handle them instead of flushing to zero. */
    if ((!ae && (a.u & 0x7fffffff)) || (!be && (b.u & 0x7fffffff)) ||
        (!ce && (c.u & 0x7fffffff))) return 0;
    p->scale = (int)e - 170;
    p->origin = S40_PlaneCoefficient(a.u, e);
    p->du = S40_PlaneCoefficient(b.u, e);
    p->dv = S40_PlaneCoefficient(c.u, e);
    return 1;
}

static inline int64_t S40_PlaneAt(const s40_plane *p, int u, int v) {
    return p->origin + p->du * u + p->dv * v;
}

/* Normalize a nonzero unsigned integer into [2^30,2^31). */
static inline uint32_t S40_Normalize(uint64_t value, int *shift) {
    *shift = 33 - __builtin_clzll(value);
    return *shift >= 0 ? value >> *shift : value << -*shift;
}

/* A 256-entry seed and two integer Newton steps give about 30 useful bits
 * of reciprocal precision. No division or floating-point helper is called
 * at a perspective endpoint. Table entries are round(2^40/(513+2*i)). */
static const uint32_t s40_reciprocal[256] = {
    2143297520u, 2134974035u, 2126714947u, 2118519514u, 2110387001u, 2102316688u, 2094307862u, 2086359825u,
    2078471886u, 2070643367u, 2062873598u, 2055161921u, 2047507687u, 2039910256u, 2032368998u, 2024883292u,
    2017452528u, 2010076102u, 2002753420u, 1995483898u, 1988266958u, 1981102032u, 1973988560u, 1966925989u,
    1959913775u, 1952951381u, 1946038279u, 1939173947u, 1932357870u, 1925589541u, 1918868460u, 1912194135u,
    1905566079u, 1898983813u, 1892446864u, 1885954765u, 1879507056u, 1873103284u, 1866743001u, 1860425766u,
    1854151143u, 1847918702u, 1841728020u, 1835578677u, 1829470263u, 1823402368u, 1817374591u, 1811386537u,
    1805437812u, 1799528032u, 1793656815u, 1787823785u, 1782028570u, 1776270804u, 1770550125u, 1764866176u,
    1759218604u, 1753607062u, 1748031205u, 1742490694u, 1736985194u, 1731514374u, 1726077909u, 1720675474u,
    1715306752u, 1709971427u, 1704669190u, 1699399734u, 1694162755u, 1688957954u, 1683785035u, 1678643707u,
    1673533680u, 1668454670u, 1663406396u, 1658388579u, 1653400944u, 1648443220u, 1643515139u, 1638616435u,
    1633746847u, 1628906115u, 1624093985u, 1619310203u, 1614554519u, 1609826688u, 1605126464u, 1600453607u,
    1595807878u, 1591189042u, 1586596865u, 1582031119u, 1577491575u, 1572978008u, 1568490197u, 1564027920u,
    1559590961u, 1555179106u, 1550792141u, 1546429856u, 1542092045u, 1537778500u, 1533489021u, 1529223404u,
    1524981453u, 1520762971u, 1516567762u, 1512395637u, 1508246403u, 1504119874u, 1500015863u, 1495934187u,
    1491874665u, 1487837115u, 1483821360u, 1479827224u, 1475854534u, 1471903116u, 1467972801u, 1464063419u,
    1460174804u, 1456306792u, 1452459218u, 1448631921u, 1444824741u, 1441037520u, 1437270102u, 1433522331u,
    1429794054u, 1426085120u, 1422395379u, 1418724681u, 1415072880u, 1411439830u, 1407825388u, 1404229410u,
    1400651755u, 1397092284u, 1393550859u, 1390027342u, 1386521599u, 1383033494u, 1379562896u, 1376109672u,
    1372673693u, 1369254829u, 1365852954u, 1362467940u, 1359099664u, 1355748000u, 1352412826u, 1349094022u,
    1345791466u, 1342505040u, 1339234626u, 1335980107u, 1332741367u, 1329518292u, 1326310769u, 1323118686u,
    1319941930u, 1316780393u, 1313633964u, 1310502536u, 1307386002u, 1304284256u, 1301197193u, 1298124708u,
    1295066699u, 1292023064u, 1288993702u, 1285978512u, 1282977395u, 1279990254u, 1277016989u, 1274057506u,
    1271111708u, 1268179501u, 1265260791u, 1262355485u, 1259463491u, 1256584717u, 1253719074u, 1250866471u,
    1248026819u, 1245200031u, 1242386020u, 1239584699u, 1236795982u, 1234019784u, 1231256022u, 1228504612u,
    1225765471u, 1223038518u, 1220323671u, 1217620850u, 1214929975u, 1212250968u, 1209583749u, 1206928241u,
    1204284368u, 1201652052u, 1199031219u, 1196421793u, 1193823700u, 1191236866u, 1188661219u, 1186096686u,
    1183543195u, 1181000674u, 1178469054u, 1175948265u, 1173438237u, 1170938901u, 1168450189u, 1165972034u,
    1163504368u, 1161047125u, 1158600240u, 1156163646u, 1153737280u, 1151321076u, 1148914972u, 1146518903u,
    1144132807u, 1141756623u, 1139390288u, 1137033741u, 1134686922u, 1132349771u, 1130022228u, 1127704234u,
    1125395730u, 1123096658u, 1120806960u, 1118526580u, 1116255460u, 1113993544u, 1111740776u, 1109497102u,
    1107262465u, 1105036812u, 1102820088u, 1100612240u, 1098413215u, 1096222959u, 1094041421u, 1091868548u,
    1089704289u, 1087548593u, 1085401409u, 1083262687u, 1081132377u, 1079010430u, 1076896795u, 1074791425u,
};
static inline uint32_t S40_Reciprocal(uint32_t denominator) {
    uint32_t x = s40_reciprocal[(denominator >> 22) - 256];
    for (int i = 0; i < 2; ++i) {
        int32_t error = (int32_t)(0x80000000u -
                         (uint32_t)(((uint64_t)denominator * x) >> 30));
        x += (int32_t)(((int64_t)x * error) >> 31);
    }
    return x;
}

/* Produce the existing 16.16 texture coordinate, with the same texture
 * edge clamps. Wide intermediates prevent overflow before the clamp. */
static inline int S40_TextureCoordinate(int64_t value, int scale,
                                       uint32_t reciprocal, int zscale,
                                       int adjust, int minimum, int maximum) {
    int64_t coordinate = 0;
    if (value) {
        uint64_t magnitude = value < 0 ? 0u - (uint64_t)value : (uint64_t)value;
        /* Upper 64 bits of a 64x32 product, without a 128-bit type or a
         * separate normalization of each texture coordinate. */
        uint64_t upper = (magnitude >> 32) * reciprocal +
                         (((uint64_t)(uint32_t)magnitude * reciprocal) >> 32);
        int right = 13 - scale + zscale;
        if (right >= 0) {
            if (right < 64) coordinate = upper >> right;
        } else {
            /* Unusually small coefficients: retain the low product bits. */
            int shift;
            uint32_t mantissa = S40_Normalize(magnitude, &shift);
            right = 45 - (scale + shift - zscale);
            if (right < 29) return value < 0 ? minimum : maximum;
            if (right < 64) coordinate = ((uint64_t)mantissa * reciprocal) >> right;
        }
        if (value < 0) coordinate = -coordinate;
    }
    coordinate += adjust;
    return coordinate < minimum ? minimum : coordinate > maximum ? maximum : (int)coordinate;
}

/* Evaluate the same depth plane once per scanline in integer arithmetic.
 * The actual depth stepping and packed two-pixel stores remain unchanged. */
static inline int S40_PlaneDepth(const s40_plane *p, int u, int v, int *depth) {
    int64_t value = S40_PlaneAt(p, u, v);
    uint64_t magnitude = value < 0 ? 0u - (uint64_t)value : (uint64_t)value;
    int right = -31 - p->scale;
    if (right < 0) return 0;
    magnitude = right < 64 ? magnitude >> right : 0;
    if (magnitude > 0x7fffffffu) return 0;
    *depth = value < 0 ? -(int)magnitude : (int)magnitude;
    return 1;
}
#endif
