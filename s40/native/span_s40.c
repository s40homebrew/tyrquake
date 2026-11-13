/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "quakedef.h"
#include "r_local.h"
#include "d_local.h"
#include "render_s40.h"
#include "span_math_s40.h"

/* Integer perspective setup only: retain Quake's 16-pixel affine spans,
 * surface cache, mipmaps, lighting and per-pixel 16.16 stepping. Returning
 * zero asks the original drawer to repaint this surface as a fallback. */
int S40_DrawSpans(espan_t *span) {
    s40_plane sp, tp, zp;
    if (!S40_MakePlane(&sp, d_sdivzorigin, d_sdivzstepu, d_sdivzstepv) ||
        !S40_MakePlane(&tp, d_tdivzorigin, d_tdivzstepu, d_tdivzstepv) ||
        !S40_MakePlane(&zp, d_ziorigin, d_zistepu, d_zistepv)) return 0;
    const byte *texture = cacheblock;
    const int width = cachewidth;
    do {
        if ((unsigned)span->u >= 240 || (unsigned)span->v >= 320 ||
            span->count < 1 || span->count > 240 - span->u) return 0;
        int64_t ns = S40_PlaneAt(&sp, span->u, span->v);
        int64_t nt = S40_PlaneAt(&tp, span->u, span->v);
        int64_t nz = S40_PlaneAt(&zp, span->u, span->v);
        if (nz <= 0) return 0;
        int shift;
        uint32_t reciprocal = S40_Reciprocal(S40_Normalize(nz, &shift));
        int zs = zp.scale + shift;
        int s = S40_TextureCoordinate(ns, sp.scale, reciprocal, zs, sadjust, 0, bbextents);
        int t = S40_TextureCoordinate(nt, tp.scale, reciprocal, zs, tadjust, 0, bbextentt);
        byte *dest = (byte *)d_viewbuffer + span->v * screenwidth + span->u;
        int count = span->count;
        while (count) {
            int pixels = count >= 16 ? 16 : count;
            /* Match Qb's endpoint at +16 even for an exact 16-pixel tail. */
            int step = pixels == 16 ? 16 : pixels - 1;
            ns += sp.du * step; nt += tp.du * step; nz += zp.du * step;
            if (nz <= 0) return 0;
            reciprocal = S40_Reciprocal(S40_Normalize(nz, &shift));
            zs = zp.scale + shift;
            int snext = S40_TextureCoordinate(ns, sp.scale, reciprocal, zs, sadjust, 16, bbextents);
            int tnext = S40_TextureCoordinate(nt, tp.scale, reciprocal, zs, tadjust, 16, bbextentt);
            int ds = pixels == 16 ? (snext - s) >> 4 :
                     pixels > 1 ? S40_SpanDivide(snext - s, pixels - 1) : 0;
            int dt = pixels == 16 ? (tnext - t) >> 4 :
                     pixels > 1 ? S40_SpanDivide(tnext - t, pixels - 1) : 0;
            /* Same unrolled affine pixel loop as the original Qb drawer. */
#define PIXEL(i) dest[i] = texture[(s >> 16) + (t >> 16) * width]; s += ds; t += dt
            dest += pixels;
            switch (pixels) {
                case 16: PIXEL(-16);
                case 15: PIXEL(-15);
                case 14: PIXEL(-14);
                case 13: PIXEL(-13);
                case 12: PIXEL(-12);
                case 11: PIXEL(-11);
                case 10: PIXEL(-10);
                case 9: PIXEL(-9);
                case 8: PIXEL(-8);
                case 7: PIXEL(-7);
                case 6: PIXEL(-6);
                case 5: PIXEL(-5);
                case 4: PIXEL(-4);
                case 3: PIXEL(-3);
                case 2: PIXEL(-2);
                case 1: PIXEL(-1);
            }
#undef PIXEL
            s = snext; t = tnext; count -= pixels;
        }
    } while ((span = span->pnext));
    return 1;
}
