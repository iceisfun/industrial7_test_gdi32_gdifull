/* StretchBlt and same-DC overlapping blits */
#include "blt.h"

static int count_v(BD *b, int x0, int y0, int w, int h, DWORD v) {
    int n = 0, x, y;
    for (y = y0; y < y0 + h; y++) for (x = x0; x < x0 + w; x++) if ((bd_get(b, x, y) & 0xFFFFFF) == (v & 0xFFFFFF)) n++;
    return n;
}
/* locate the destination position of every distinct source pixel (enc values); -1 when missing */
static void locate(BD *d, BD *s, int pos[][2]) {
    int x, y, sx, sy;
    for (sy = 0; sy < s->h; sy++) for (sx = 0; sx < s->w; sx++) { pos[sy * s->w + sx][0] = -1; pos[sy * s->w + sx][1] = -1; }
    for (y = 0; y < d->h; y++) for (x = 0; x < d->w; x++) {
        DWORD v = bd_get(d, x, y) & 0xFFFFFF;
        for (sy = 0; sy < s->h; sy++) for (sx = 0; sx < s->w; sx++) if (v == (bd_get(s, sx, sy) & 0xFFFFFF)) { pos[sy * s->w + sx][0] = x; pos[sy * s->w + sx][1] = y; }
    }
}

static void modes_setup(BD *s, int w, int h) { *s = bd_new(32, w, h); }

TEST(stretch_1to1_equals_bitblt) {
    BD s, d, e; int mode; NEED();
    s = bd_new(32, 16, 12); d = bd_new(32, 16, 12); e = bd_new(32, 16, 12);
    bd_fill_enc(&s);
    for (mode = 1; mode <= 4; mode++) {
        pSetStretchBltMode(d.dc, mode);
        bd_clear(&d, 0); bd_clear(&e, 0);
        CHECK(pStretchBlt(d.dc, 0, 0, 16, 12, s.dc, 0, 0, 16, 12, SRCCOPY));
        CHECK(pBitBlt(e.dc, 0, 0, 16, 12, s.dc, 0, 0, SRCCOPY));
        CHECKF(memcmp(d.raw, e.raw, (size_t)d.stride * 12) == 0, "mode %d 1:1 differs from BitBlt", mode);
        int x, y, bad = 0;
        for (y = 0; y < 12; y++) for (x = 0; x < 16; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != enc(x, y)) bad++;
        CHECKF(!bad, "mode %d: %d pixels", mode, bad);
    }
    bd_free(&s); bd_free(&d); bd_free(&e);
}
TEST(stretch_mode_default_and_roundtrip) {
    NEED();
    HDC dc = pCreateCompatibleDC(NULL);
    int def = pGetStretchBltMode(dc);
    t_note("default stretch mode %d", def);
    CHECK(def == BLACKONWHITE);
    int m;
    for (m = 1; m <= 4; m++) {
        int prev = pGetStretchBltMode(dc);
        int r = pSetStretchBltMode(dc, m);
        CHECKF(r == prev, "SetStretchBltMode(%d) returned %d prev %d", m, r, prev);
        CHECKF(pGetStretchBltMode(dc) == m, "get after set %d", m);
    }
    CHECK(STRETCH_ANDSCANS == BLACKONWHITE && STRETCH_ORSCANS == WHITEONBLACK && STRETCH_DELETESCANS == COLORONCOLOR && STRETCH_HALFTONE == HALFTONE);
    pDeleteDC(dc);
}
TEST(stretch_mode_invalid_values) {
    NEED();
    HDC dc = pCreateCompatibleDC(NULL);
    pSetStretchBltMode(dc, COLORONCOLOR);
    static const int bad[] = { 0, 5, 6, 100, -1, 0x10000 };
    int i;
    for (i = 0; i < 6; i++) {
        SetLastError(0xDEAD);
        int r = pSetStretchBltMode(dc, bad[i]);
        t_note("SetStretchBltMode(%d) -> %d err=%lu now=%d", bad[i], r, GetLastError(), pGetStretchBltMode(dc));
        pSetStretchBltMode(dc, COLORONCOLOR);
    }
    pDeleteDC(dc);
}
TEST(stretch_mode_bad_dc) {
    NEED();
    SetLastError(0xDEAD);
    CHECK(pSetStretchBltMode(NULL, COLORONCOLOR) == 0);
    SetLastError(0xDEAD);
    CHECK(pGetStretchBltMode(NULL) == 0);
    CHECK(pSetStretchBltMode((HDC)(ULONG_PTR)0x2468, COLORONCOLOR) == 0);
}
TEST(stretch_mode_is_per_dc) {
    NEED();
    HDC a = pCreateCompatibleDC(NULL), b = pCreateCompatibleDC(NULL);
    pSetStretchBltMode(a, HALFTONE); pSetStretchBltMode(b, COLORONCOLOR);
    CHECK(pGetStretchBltMode(a) == HALFTONE);
    CHECK(pGetStretchBltMode(b) == COLORONCOLOR);
    int id = pSaveDC(a);
    pSetStretchBltMode(a, WHITEONBLACK);
    pRestoreDC(a, id);
    CHECK(pGetStretchBltMode(a) == HALFTONE);
    pDeleteDC(a); pDeleteDC(b);
}

static void upscale(int mode, int factor_x, int factor_y, int exact) {
    BD s, d; int x, y, bad = 0; const int SW = 6, SH = 5;
    NEED();
    s = bd_new(32, SW, SH); d = bd_new(32, SW * factor_x + 4, SH * factor_y + 4);
    bd_fill_enc(&s); bd_clear(&d, 0x0A0B0C);
    pSetStretchBltMode(d.dc, mode);
    CHECK(pStretchBlt(d.dc, 2, 3, SW * factor_x, SH * factor_y, s.dc, 0, 0, SW, SH, SRCCOPY));
    for (y = 0; y < SH * factor_y; y++) for (x = 0; x < SW * factor_x; x++)
        if ((bd_get(&d, 2 + x, 3 + y) & 0xFFFFFF) != enc(x / factor_x, y / factor_y)) bad++;
    if (exact) CHECKF(!bad, "mode %d x%d,%d: %d pixels differ from replication", mode, factor_x, factor_y, bad);
    else if (bad) t_note("mode %d x%d,%d: %d pixels differ from replication", mode, factor_x, factor_y, bad);
    CHECK_PX(d, 1, 3, 0x0A0B0C, 0xFFFFFF); CHECK_PX(d, 2, 2, 0x0A0B0C, 0xFFFFFF);
    CHECK_PX(d, 2 + SW * factor_x, 3, 0x0A0B0C, 0xFFFFFF); CHECK_PX(d, 2, 3 + SH * factor_y, 0x0A0B0C, 0xFFFFFF);
    bd_free(&s); bd_free(&d);
}
TEST(stretch_up2x_coloroncolor) { upscale(COLORONCOLOR, 2, 2, 1); }
TEST(stretch_up2x_blackonwhite) { upscale(BLACKONWHITE, 2, 2, 1); }
TEST(stretch_up2x_whiteonblack) { upscale(WHITEONBLACK, 2, 2, 1); }
TEST(stretch_up2x_halftone) { upscale(HALFTONE, 2, 2, 0); }
TEST(stretch_up3x_coloroncolor) { upscale(COLORONCOLOR, 3, 3, 1); }
TEST(stretch_up4x_coloroncolor) { upscale(COLORONCOLOR, 4, 4, 1); }
TEST(stretch_up_x_only_coloroncolor) { upscale(COLORONCOLOR, 3, 1, 1); }
TEST(stretch_up_y_only_coloroncolor) { upscale(COLORONCOLOR, 1, 3, 1); }
TEST(stretch_up_x_only_blackonwhite) { upscale(BLACKONWHITE, 2, 1, 1); }

/* 2:1 reduction where each 2x2 source block is uniform: every mode must give that colour */
static void down_uniform(int mode, int exact) {
    BD s, d; int x, y, bad = 0; const int DW = 8, DH = 6;
    NEED();
    s = bd_new(32, DW * 2, DH * 2); d = bd_new(32, DW, DH);
    for (y = 0; y < DH * 2; y++) for (x = 0; x < DW * 2; x++) bd_put(&s, x, y, enc(x / 2, y / 2));
    bd_clear(&d, 0);
    pSetStretchBltMode(d.dc, mode);
    CHECK(pStretchBlt(d.dc, 0, 0, DW, DH, s.dc, 0, 0, DW * 2, DH * 2, SRCCOPY));
    for (y = 0; y < DH; y++) for (x = 0; x < DW; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != enc(x, y)) bad++;
    if (exact) CHECKF(!bad, "mode %d: %d pixels differ", mode, bad);
    else if (bad) t_note("mode %d uniform 2:1 down: %d differ", mode, bad);
    bd_free(&s); bd_free(&d);
}
TEST(stretch_down2x_uniform_coloroncolor) { down_uniform(COLORONCOLOR, 1); }
TEST(stretch_down2x_uniform_blackonwhite) { down_uniform(BLACKONWHITE, 1); }
TEST(stretch_down2x_uniform_whiteonblack) { down_uniform(WHITEONBLACK, 1); }
TEST(stretch_down2x_uniform_halftone) { down_uniform(HALFTONE, 0); }

TEST(stretch_halftone_uniform_source_stays_uniform) {
    BD s, d; NEED();
    s = bd_new(32, 40, 40); d = bd_new(32, 17, 13);
    bd_clear(&s, 0x6699CC); bd_clear(&d, 0);
    pSetStretchBltMode(d.dc, HALFTONE);
    pSetBrushOrgEx(d.dc, 0, 0, NULL);
    CHECK(pStretchBlt(d.dc, 0, 0, 17, 13, s.dc, 0, 0, 40, 40, SRCCOPY));
    CHECKF(count_v(&d, 0, 0, 17, 13, 0x6699CC) == 17 * 13, "n=%d", count_v(&d, 0, 0, 17, 13, 0x6699CC));
    bd_free(&s); bd_free(&d);
}
TEST(stretch_coloroncolor_uniform_source_nonintegral) {
    int mode; NEED();
    for (mode = 1; mode <= 4; mode++) {
        BD s = bd_new(32, 33, 31), d = bd_new(32, 20, 11);
        bd_clear(&s, 0xFEDCBA); bd_clear(&d, 0);
        pSetStretchBltMode(d.dc, mode);
        CHECK(pStretchBlt(d.dc, 0, 0, 20, 11, s.dc, 0, 0, 33, 31, SRCCOPY));
        CHECKF(count_v(&d, 0, 0, 20, 11, 0xFEDCBA) == 220, "mode %d n=%d", mode, count_v(&d, 0, 0, 20, 11, 0xFEDCBA));
        bd_free(&s); bd_free(&d);
    }
}
/* AND / OR of eliminated pixels is documented for BLACKONWHITE / WHITEONBLACK */
TEST(stretch_down2x_and_or_semantics) {
    BD s, d; int x, y; NEED();
    s = bd_new(32, 4, 2); d = bd_new(32, 2, 1);
    static const DWORD v[2][4] = { { 0xF0F0F0, 0x0FF00F, 0x123456, 0xFFFFFF }, { 0x3C3C3C, 0xFF00FF, 0x654321, 0x00FF00 } };
    for (y = 0; y < 2; y++) for (x = 0; x < 4; x++) bd_put(&s, x, y, v[y][x]);
    pSetStretchBltMode(d.dc, BLACKONWHITE);
    bd_clear(&d, 0);
    BOOL r = pStretchBlt(d.dc, 0, 0, 2, 1, s.dc, 0, 0, 4, 2, SRCCOPY);
    DWORD a0 = v[0][0] & v[0][1] & v[1][0] & v[1][1], a1 = v[0][2] & v[0][3] & v[1][2] & v[1][3];
    t_note("BLACKONWHITE 4x2->2x1: ret %d got %06lx %06lx; AND of block %06lx %06lx", r, (unsigned long)(bd_get(&d, 0, 0) & 0xFFFFFF), (unsigned long)(bd_get(&d, 1, 0) & 0xFFFFFF), (unsigned long)a0, (unsigned long)a1);
    CHECK(r);
    pSetStretchBltMode(d.dc, WHITEONBLACK);
    bd_clear(&d, 0);
    r = pStretchBlt(d.dc, 0, 0, 2, 1, s.dc, 0, 0, 4, 2, SRCCOPY);
    DWORD o0 = v[0][0] | v[0][1] | v[1][0] | v[1][1], o1 = v[0][2] | v[0][3] | v[1][2] | v[1][3];
    t_note("WHITEONBLACK 4x2->2x1: ret %d got %06lx %06lx; OR of block %06lx %06lx", r, (unsigned long)(bd_get(&d, 0, 0) & 0xFFFFFF), (unsigned long)(bd_get(&d, 1, 0) & 0xFFFFFF), (unsigned long)o0, (unsigned long)o1);
    CHECK(r);
    pSetStretchBltMode(d.dc, COLORONCOLOR);
    bd_clear(&d, 0);
    r = pStretchBlt(d.dc, 0, 0, 2, 1, s.dc, 0, 0, 4, 2, SRCCOPY);
    DWORD g0 = bd_get(&d, 0, 0) & 0xFFFFFF, g1 = bd_get(&d, 1, 0) & 0xFFFFFF;
    int ok0 = 0, ok1 = 0;
    for (y = 0; y < 2; y++) for (x = 0; x < 2; x++) { ok0 |= (g0 == v[y][x]); ok1 |= (g1 == v[y][x + 2]); }
    CHECKF(r && ok0 && ok1, "COLORONCOLOR picks an existing pixel of each block (got %06lx %06lx)", (unsigned long)g0, (unsigned long)g1);
    bd_free(&s); bd_free(&d);
}

/* ---- mirroring ---- */
static void mirror_case(int dxs, int dys, int sxs, int sys) {
    BD s, d; int pos[64][2]; int sx, sy, bad = 0; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 40, 40);
    bd_fill_enc(&s); bd_clear(&d, 0);
    int dx = 12, dy = 14, w = 8 * dxs, h = 8 * dys, srcx = sxs < 0 ? 7 : 0, srcy = sys < 0 ? 7 : 0;
    pSetStretchBltMode(d.dc, COLORONCOLOR);
    BOOL r = pStretchBlt(d.dc, dxs < 0 ? dx + 7 : dx, dys < 0 ? dy + 7 : dy, w, h, s.dc, srcx, srcy, 8 * sxs, 8 * sys, SRCCOPY);
    t_note("StretchBlt dest signs (%d,%d) src signs (%d,%d) -> %d", dxs, dys, sxs, sys, r);
    locate(&d, &s, pos);
    int hflip = (dxs * sxs) < 0, vflip = (dys * sys) < 0, cx = 0, cy = 0, first = 1;
    for (sy = 0; sy < 8; sy++) for (sx = 0; sx < 8; sx++) {
        int px_ = pos[sy * 8 + sx][0], py_ = pos[sy * 8 + sx][1];
        if (px_ < 0) { bad++; continue; }
        int kx = hflip ? px_ + sx : px_ - sx, ky = vflip ? py_ + sy : py_ - sy;
        if (first) { cx = kx; cy = ky; first = 0; } else if (kx != cx || ky != cy) bad++;
    }
    CHECKF(r, "ret %d", r);
    CHECKF(!bad, "signs (%d,%d)/(%d,%d): %d inconsistent/missing pixels (hflip=%d vflip=%d)", dxs, dys, sxs, sys, bad, hflip, vflip);
    t_note("mirror dest(%d,%d) src(%d,%d): origin const x=%d y=%d", dxs, dys, sxs, sys, cx, cy);
    bd_free(&s); bd_free(&d);
}
TEST(stretch_mirror_none) { mirror_case(1, 1, 1, 1); }
TEST(stretch_mirror_dest_w_negative) { mirror_case(-1, 1, 1, 1); }
TEST(stretch_mirror_dest_h_negative) { mirror_case(1, -1, 1, 1); }
TEST(stretch_mirror_dest_both_negative) { mirror_case(-1, -1, 1, 1); }
TEST(stretch_mirror_src_w_negative) { mirror_case(1, 1, -1, 1); }
TEST(stretch_mirror_src_h_negative) { mirror_case(1, 1, 1, -1); }
TEST(stretch_mirror_src_both_negative) { mirror_case(1, 1, -1, -1); }
TEST(stretch_mirror_both_w_negative_cancels) { mirror_case(-1, 1, -1, 1); }
TEST(stretch_mirror_dest_w_src_h_negative) { mirror_case(-1, 1, 1, -1); }

TEST(stretch_mirror_stretched_2x) {
    BD s, d; NEED();
    s = bd_new(32, 4, 4); d = bd_new(32, 20, 12);
    bd_fill_enc(&s); bd_clear(&d, 0);
    pSetStretchBltMode(d.dc, COLORONCOLOR);
    BOOL r = pStretchBlt(d.dc, 11, 1, -8, 8, s.dc, 0, 0, 4, 4, SRCCOPY);
    CHECK(r);
    int pos[16][2]; locate(&d, &s, pos);
    int sx, sy, bad = 0;
    for (sy = 0; sy < 4; sy++) for (sx = 0; sx < 4; sx++) {
        if (pos[sy * 4 + sx][0] < 0) { bad++; continue; }
        if (pos[sy * 4 + sx][1] != pos[0][1] + sy * 2 || pos[sy * 4 + sx][0] != pos[0][0] - sx * 2) bad++;
    }
    CHECKF(!bad, "%d inconsistent", bad);
    bd_free(&s); bd_free(&d);
}

/* ---- degenerate extents ---- */
TEST(stretch_zero_extents_leave_dest) {
    BD s, d; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_fill_enc(&s);
    static const int e[][4] = { { 0, 4, 4, 4 }, { 4, 0, 4, 4 }, { 4, 4, 0, 4 }, { 4, 4, 4, 0 }, { 0, 0, 0, 0 } };
    int i;
    for (i = 0; i < 5; i++) {
        bd_clear(&d, 0x77);
        SetLastError(0xDEAD);
        BOOL r = pStretchBlt(d.dc, 0, 0, e[i][0], e[i][1], s.dc, 0, 0, e[i][2], e[i][3], SRCCOPY);
        t_note("StretchBlt extents dw=%d dh=%d sw=%d sh=%d -> %d err=%lu", e[i][0], e[i][1], e[i][2], e[i][3], r, GetLastError());
        CHECK_RECT_VAL(d, 0, 0, 8, 8, 0x77, 0xFFFFFF);
    }
    bd_free(&s); bd_free(&d);
}
TEST(stretch_one_pixel_source_fills_dest) {
    BD s, d; int mode; NEED();
    s = bd_new(32, 4, 4); d = bd_new(32, 12, 12);
    bd_fill_enc(&s);
    for (mode = 1; mode <= 3; mode++) {
        bd_clear(&d, 0); pSetStretchBltMode(d.dc, mode);
        CHECK(pStretchBlt(d.dc, 1, 1, 9, 7, s.dc, 2, 3, 1, 1, SRCCOPY));
        CHECKF(count_v(&d, 1, 1, 9, 7, enc(2, 3)) == 63, "mode %d n=%d", mode, count_v(&d, 1, 1, 9, 7, enc(2, 3)));
        CHECK_PX(d, 0, 0, 0, 0xFFFFFF); CHECK_PX(d, 10, 8, 0, 0xFFFFFF);
    }
    bd_free(&s); bd_free(&d);
}
TEST(stretch_to_one_pixel) {
    BD s, d; int mode; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 4, 4);
    bd_clear(&s, 0x13579B);
    for (mode = 1; mode <= 4; mode++) {
        bd_clear(&d, 0); pSetStretchBltMode(d.dc, mode);
        CHECK(pStretchBlt(d.dc, 2, 2, 1, 1, s.dc, 0, 0, 8, 8, SRCCOPY));
        CHECKF(count_v(&d, 0, 0, 4, 4, 0x13579B) == 1, "mode %d n=%d", mode, count_v(&d, 0, 0, 4, 4, 0x13579B));
        CHECK_PX(d, 2, 2, 0x13579B, 0xFFFFFF);
    }
    bd_free(&s); bd_free(&d);
}
TEST(stretch_source_subrect_offset) {
    BD s, d; int x, y, bad = 0; NEED();
    s = bd_new(32, 16, 16); d = bd_new(32, 16, 16);
    bd_fill_enc(&s); bd_clear(&d, 0);
    pSetStretchBltMode(d.dc, COLORONCOLOR);
    CHECK(pStretchBlt(d.dc, 1, 2, 10, 8, s.dc, 5, 6, 5, 4, SRCCOPY));
    for (y = 0; y < 8; y++) for (x = 0; x < 10; x++) if ((bd_get(&d, 1 + x, 2 + y) & 0xFFFFFF) != enc(5 + x / 2, 6 + y / 2)) bad++;
    CHECKF(!bad, "bad %d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(stretch_dest_clipped_by_bitmap) {
    BD s, d; int x, y, bad = 0; NEED();
    s = bd_new(32, 4, 4); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0);
    pSetStretchBltMode(d.dc, COLORONCOLOR);
    CHECK(pStretchBlt(d.dc, -4, -2, 16, 16, s.dc, 0, 0, 4, 4, SRCCOPY));
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != enc((x + 4) / 4, (y + 2) / 4)) bad++;
    CHECKF(!bad, "bad %d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(stretch_dest_clipped_by_region) {
    BD s, d; int x, y, bad = 0; NEED();
    s = bd_new(32, 4, 4); d = bd_new(32, 16, 16);
    bd_fill_enc(&s); bd_clear(&d, 0);
    pSetStretchBltMode(d.dc, COLORONCOLOR);
    HRGN rg = pCreateRectRgn(3, 5, 9, 11); pSelectClipRgn(d.dc, rg);
    CHECK(pStretchBlt(d.dc, 0, 0, 16, 16, s.dc, 0, 0, 4, 4, SRCCOPY));
    for (y = 0; y < 16; y++) for (x = 0; x < 16; x++) {
        DWORD e = (x >= 3 && x < 9 && y >= 5 && y < 11) ? enc(x / 4, y / 4) : 0;
        if ((bd_get(&d, x, y) & 0xFFFFFF) != e) bad++;
    }
    CHECKF(!bad, "bad %d", bad);
    pSelectClipRgn(d.dc, NULL); pDeleteObject(rg);
    bd_free(&s); bd_free(&d);
}
TEST(stretch_with_rop_srcinvert) {
    BD s, d; int x, y, bad = 0; NEED();
    s = bd_new(32, 4, 4); d = bd_new(32, 8, 8);
    bd_fill_enc(&s);
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) bd_put(&d, x, y, 0x55AA55);
    pSetStretchBltMode(d.dc, COLORONCOLOR);
    CHECK(pStretchBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 4, 4, SRCINVERT));
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != (enc(x / 2, y / 2) ^ 0x55AA55)) bad++;
    CHECKF(!bad, "bad %d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(stretch_with_rop_notsrccopy) {
    BD s, d; int x, y, bad = 0; NEED();
    s = bd_new(32, 4, 4); d = bd_new(32, 12, 4);
    bd_fill_enc(&s);
    pSetStretchBltMode(d.dc, COLORONCOLOR);
    CHECK(pStretchBlt(d.dc, 0, 0, 12, 4, s.dc, 0, 0, 4, 4, NOTSRCCOPY));
    for (y = 0; y < 4; y++) for (x = 0; x < 12; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != (~enc(x / 3, y) & 0xFFFFFF)) bad++;
    CHECKF(!bad, "bad %d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(stretch_with_pattern_rop) {
    BD s, d; NEED();
    s = bd_new(32, 4, 4); d = bd_new(32, 8, 8);
    bd_clear(&s, 0xFFFFFF); bd_clear(&d, 0x102030);
    HBRUSH br = pCreateSolidBrush(0x00FF00); HGDIOBJ o = pSelectObject(d.dc, br);
    pSetStretchBltMode(d.dc, COLORONCOLOR);
    CHECK(pStretchBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 4, 4, PATCOPY));
    CHECK_RECT_VAL(d, 0, 0, 8, 8, 0x00FF00, 0xFFFFFF);
    CHECK(pStretchBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 4, 4, MERGECOPY));
    CHECK_RECT_VAL(d, 0, 0, 8, 8, 0x00FF00, 0xFFFFFF);
    pSelectObject(d.dc, o); pDeleteObject(br);
    bd_free(&s); bd_free(&d);
}
TEST(stretch_null_src_pattern_rop) {
    BD d; NEED(); d = bd_new(32, 8, 8);
    HBRUSH br = pCreateSolidBrush(0x0000FF); HGDIOBJ o = pSelectObject(d.dc, br);
    bd_clear(&d, 0);
    BOOL r = pStretchBlt(d.dc, 1, 1, 4, 4, NULL, 0, 0, 4, 4, PATCOPY);
    t_note("StretchBlt PATCOPY with NULL source -> %d", r);
    if (r) CHECK_RECT_VAL(d, 1, 1, 4, 4, 0xFF0000, 0xFFFFFF);
    r = pStretchBlt(d.dc, 1, 1, 4, 4, NULL, 0, 0, 4, 4, SRCCOPY);
    CHECK(!r);
    pSelectObject(d.dc, o); pDeleteObject(br); bd_free(&d);
}
TEST(stretch_bad_dc) {
    BD s; NEED(); s = bd_new(32, 4, 4);
    SetLastError(0xDEAD);
    CHECK(!pStretchBlt(NULL, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, SRCCOPY));
    t_note("StretchBlt NULL dest err=%lu", GetLastError());
    CHECK(!pStretchBlt(s.dc, 0, 0, 4, 4, (HDC)(ULONG_PTR)0x4242, 0, 0, 4, 4, SRCCOPY));
    CHECK(!pStretchBlt((HDC)(ULONG_PTR)0x4242, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, SRCCOPY));
    CHECK(!pStretchBlt(s.dc, 0, 0, 4, 4, (HDC)s.bm, 0, 0, 4, 4, SRCCOPY));
    bd_free(&s);
}
TEST(stretch_same_bitmap_overlap_up2x) {
    BD d; int x, y, bad = 0; NEED();
    d = bd_new(32, 32, 32);
    BD ref = bd_new(32, 32, 32);
    bd_fill_enc(&d); bd_fill_enc(&ref);
    pSetStretchBltMode(d.dc, COLORONCOLOR);
    CHECK(pStretchBlt(d.dc, 0, 0, 16, 16, d.dc, 0, 0, 8, 8, SRCCOPY));
    for (y = 0; y < 16; y++) for (x = 0; x < 16; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != enc(x / 2, y / 2)) bad++;
    CHECKF(!bad, "bad %d", bad);
    for (y = 0; y < 32; y++) for (x = 0; x < 32; x++) if ((x >= 16 || y >= 16) && bd_get(&d, x, y) != bd_get(&ref, x, y)) bad++;
    CHECKF(!bad, "outside changed %d", bad);
    bd_free(&d); bd_free(&ref);
}
TEST(stretch_same_bitmap_overlap_shift_up2x) {
    BD d; int x, y, bad = 0; NEED();
    d = bd_new(32, 32, 32);
    bd_fill_enc(&d);
    pSetStretchBltMode(d.dc, COLORONCOLOR);
    CHECK(pStretchBlt(d.dc, 2, 2, 20, 20, d.dc, 4, 4, 10, 10, SRCCOPY));
    for (y = 0; y < 20; y++) for (x = 0; x < 20; x++) if ((bd_get(&d, 2 + x, 2 + y) & 0xFFFFFF) != enc(4 + x / 2, 4 + y / 2)) bad++;
    CHECKF(!bad, "bad %d", bad);
    bd_free(&d);
}
TEST(stretch_same_bitmap_overlap_down2x) {
    BD d; int x, y, bad = 0; NEED();
    d = bd_new(32, 32, 32);
    for (y = 0; y < 32; y++) for (x = 0; x < 32; x++) bd_put(&d, x, y, enc(x / 2, y / 2));
    pSetStretchBltMode(d.dc, COLORONCOLOR);
    CHECK(pStretchBlt(d.dc, 0, 0, 16, 16, d.dc, 0, 0, 32, 32, SRCCOPY));
    for (y = 0; y < 16; y++) for (x = 0; x < 16; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != enc(x, y)) bad++;
    CHECKF(!bad, "bad %d", bad);
    bd_free(&d);
}

/* ---- same-DC overlapping BitBlt ---- */
static void shift_case(int dx, int dy) {
    BD d, ref; int x, y, bad = 0; NEED();
    d = bd_new(32, 24, 24); ref = bd_new(32, 24, 24);
    bd_fill_enc(&d); bd_fill_enc(&ref);
    int w = 16, h = 14, sx = dx < 0 ? 3 - dx : 3, sy = dy < 0 ? 3 - dy : 3;
    CHECK(pBitBlt(d.dc, sx + dx, sy + dy, w, h, d.dc, sx, sy, SRCCOPY));
    for (y = 0; y < 24; y++) for (x = 0; x < 24; x++) {
        int inx = x >= sx + dx && x < sx + dx + w && y >= sy + dy && y < sy + dy + h;
        DWORD e = inx ? (bd_get(&ref, x - dx, y - dy) & 0xFFFFFF) : (bd_get(&ref, x, y) & 0xFFFFFF);
        if ((bd_get(&d, x, y) & 0xFFFFFF) != e) bad++;
    }
    CHECKF(!bad, "shift (%d,%d): %d pixels differ", dx, dy, bad);
    bd_free(&d); bd_free(&ref);
}
TEST(overlap_bitblt_right) { shift_case(3, 0); }
TEST(overlap_bitblt_left) { shift_case(-3, 0); }
TEST(overlap_bitblt_down) { shift_case(0, 2); }
TEST(overlap_bitblt_up) { shift_case(0, -2); }
TEST(overlap_bitblt_right_down) { shift_case(3, 2); }
TEST(overlap_bitblt_left_up) { shift_case(-3, -2); }
TEST(overlap_bitblt_right_up) { shift_case(4, -3); }
TEST(overlap_bitblt_left_down) { shift_case(-4, 3); }
TEST(overlap_bitblt_one_pixel) { shift_case(1, 0); shift_case(0, 1); shift_case(-1, 0); shift_case(0, -1); }
TEST(overlap_bitblt_identity) { shift_case(0, 0); }
TEST(overlap_bitblt_srcinvert_self) {
    BD d; int x, y, bad = 0; NEED();
    d = bd_new(32, 8, 8); bd_fill_enc(&d);
    CHECK(pBitBlt(d.dc, 0, 0, 8, 8, d.dc, 0, 0, SRCINVERT));
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != 0) bad++;
    CHECKF(!bad, "bad %d", bad);
    bd_free(&d);
}
TEST(overlap_bitblt_srcinvert_shifted) {
    BD d, ref; int x, y, bad = 0; NEED();
    d = bd_new(32, 16, 8); ref = bd_new(32, 16, 8);
    bd_fill_enc(&d); bd_fill_enc(&ref);
    CHECK(pBitBlt(d.dc, 2, 0, 8, 8, d.dc, 0, 0, SRCINVERT));
    for (y = 0; y < 8; y++) for (x = 0; x < 16; x++) {
        DWORD e = (x >= 2 && x < 10) ? (bd_get(&ref, x, y) ^ bd_get(&ref, x - 2, y)) : bd_get(&ref, x, y);
        if ((bd_get(&d, x, y) & 0xFFFFFF) != (e & 0xFFFFFF)) bad++;
    }
    CHECKF(!bad, "bad %d", bad);
    bd_free(&d); bd_free(&ref);
}
TEST(overlap_bitblt_two_dcs_same_bitmap_not_possible) {
    /* a bitmap can be selected in only one DC at a time */
    NEED();
    BD a = bd_new(32, 4, 4);
    HDC dc2 = pCreateCompatibleDC(NULL);
    HGDIOBJ prev = pSelectObject(dc2, a.bm);
    t_note("select bitmap already selected elsewhere -> %p", prev);
    CHECK(prev == NULL);
    if (prev) pSelectObject(dc2, prev);
    pDeleteDC(dc2); bd_free(&a);
}
TEST(overlap_bitblt_24bpp) {
    BD d, ref; int x, y, bad = 0; NEED();
    d = bd_new(24, 20, 6); ref = bd_new(24, 20, 6);
    for (y = 0; y < 6; y++) for (x = 0; x < 20; x++) { DWORD v = rnd() & 0xFFFFFF; bd_put(&d, x, y, v); bd_put(&ref, x, y, v); }
    CHECK(pBitBlt(d.dc, 5, 0, 12, 6, d.dc, 2, 0, SRCCOPY));
    for (y = 0; y < 6; y++) for (x = 0; x < 20; x++) {
        DWORD e = (x >= 5 && x < 17) ? bd_get(&ref, x - 3, y) : bd_get(&ref, x, y);
        if (bd_get(&d, x, y) != e) bad++;
    }
    CHECKF(!bad, "bad %d", bad);
    bd_free(&d); bd_free(&ref);
}
TEST(overlap_bitblt_8bpp) {
    BD d, ref; int x, y, bad = 0; NEED();
    d = bd_new(8, 21, 6); ref = bd_new(8, 21, 6);
    for (y = 0; y < 6; y++) for (x = 0; x < 21; x++) { DWORD v = rnd() & 255; bd_put(&d, x, y, v); bd_put(&ref, x, y, v); }
    CHECK(pBitBlt(d.dc, 2, 1, 12, 5, d.dc, 5, 0, SRCCOPY));
    for (y = 0; y < 6; y++) for (x = 0; x < 21; x++) {
        DWORD e = (x >= 2 && x < 14 && y >= 1) ? bd_get(&ref, x + 3, y - 1) : bd_get(&ref, x, y);
        if (bd_get(&d, x, y) != e) bad++;
    }
    CHECKF(!bad, "bad %d", bad);
    bd_free(&d); bd_free(&ref);
}
TEST(overlap_bitblt_1bpp_unaligned) {
    BD d, ref; int x, y, bad = 0; NEED();
    d = bd_new(1, 37, 5); ref = bd_new(1, 37, 5);
    for (y = 0; y < 5; y++) for (x = 0; x < 37; x++) { DWORD v = rnd() & 1; bd_put(&d, x, y, v); bd_put(&ref, x, y, v); }
    CHECK(pBitBlt(d.dc, 3, 0, 29, 5, d.dc, 0, 0, SRCCOPY));
    for (y = 0; y < 5; y++) for (x = 0; x < 37; x++) {
        DWORD e = (x >= 3 && x < 32) ? bd_get(&ref, x - 3, y) : bd_get(&ref, x, y);
        if (bd_get(&d, x, y) != e) bad++;
    }
    CHECKF(!bad, "bad %d", bad);
    bd_free(&d); bd_free(&ref);
}
TEST(overlap_bitblt_4bpp_odd) {
    BD d, ref; int x, y, bad = 0; NEED();
    d = bd_new(4, 19, 4); ref = bd_new(4, 19, 4);
    for (y = 0; y < 4; y++) for (x = 0; x < 19; x++) { DWORD v = rnd() & 15; bd_put(&d, x, y, v); bd_put(&ref, x, y, v); }
    CHECK(pBitBlt(d.dc, 1, 0, 15, 4, d.dc, 4, 0, SRCCOPY));
    for (y = 0; y < 4; y++) for (x = 0; x < 19; x++) {
        DWORD e = (x >= 1 && x < 16) ? bd_get(&ref, x + 3, y) : bd_get(&ref, x, y);
        if (bd_get(&d, x, y) != e) bad++;
    }
    CHECKF(!bad, "bad %d", bad);
    bd_free(&d); bd_free(&ref);
}
