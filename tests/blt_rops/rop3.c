/* BitBlt with all 256 ternary raster operations, checked against a reference truth table. */
#include "blt.h"

static int rop_uses_src(unsigned code) { return ((code << 2) ^ code) & 0xCC; }

/* Run codes lo..hi at the given depth with random S, D and a solid brush P. */
static void rop_group(int bpp, int lo, int hi) {
    BD s, d; int code; const int N = 6;
    NEED();
    s = (bpp == 16) ? bd_new16(N, N, 0) : bd_new(bpp, N, N);
    d = (bpp == 16) ? bd_new16(N, N, 0) : bd_new(bpp, N, N);
    if (!bd_ok(&s) || !bd_ok(&d)) { CHECK(bd_ok(&s) && bd_ok(&d)); bd_free(&s); bd_free(&d); return; }
    DWORD mask = bd_mask(bpp);
    for (code = lo; code <= hi; code++) {
        int x, y, bad = 0; DWORD P, bx = 0, by = 0, got = 0, want = 0;
        COLORREF pc;
        if (bpp >= 24) { P = rnd() & 0xFFFFFF; pc = cr(P); }
        else if (bpp == 16) { P = (rnd() & 1) ? 0x7FFF : 0; pc = P ? RGB(255, 255, 255) : 0; }
        else { P = rnd() & mask; pc = blt_palcol(bpp, P); }
        for (y = 0; y < N; y++) for (x = 0; x < N; x++) { bd_put(&s, x, y, rnd() & mask); bd_put(&d, x, y, rnd() & mask); }
        HBRUSH br = pCreateSolidBrush(pc);
        HGDIOBJ ob = pSelectObject(d.dc, br);
        DWORD sv[36], dv[36];
        for (y = 0; y < N; y++) for (x = 0; x < N; x++) { sv[y * N + x] = bd_get(&s, x, y); dv[y * N + x] = bd_get(&d, x, y); }
        BOOL r = pBitBlt(d.dc, 0, 0, N, N, s.dc, 0, 0, ROP(code));
        pGdiFlush();
        for (y = 0; y < N && !bad; y++) for (x = 0; x < N; x++) {
            DWORD e = rop3(code, P, sv[y * N + x], dv[y * N + x]) & mask, g = bd_get(&d, x, y) & mask;
            if (e != g) { bad = 1; bx = x; by = y; got = g; want = e; break; }
        }
        CHECKF(r && !bad, "bpp %d rop %02X ret %d first diff (%lu,%lu) got 0x%lx want 0x%lx P=0x%lx", bpp, code, r, bx, by, got, want, P);
        pSelectObject(d.dc, ob); pDeleteObject(br);
    }
    bd_free(&s); bd_free(&d);
}

/* Solid S/D/P over every channel-bit combination (0 or all ones) at 32bpp. */
static void rop_solid(int lo, int hi) {
    BD s, d; int code, c;
    NEED();
    s = bd_new(32, 2, 2); d = bd_new(32, 2, 2);
    if (!bd_ok(&s) || !bd_ok(&d)) { CHECK(0); bd_free(&s); bd_free(&d); return; }
    for (code = lo; code <= hi; code++) for (c = 0; c < 8; c++) {
        DWORD P = (c & 4) ? 0xFFFFFF : 0, S = (c & 2) ? 0xFFFFFF : 0, D = (c & 1) ? 0xFFFFFF : 0;
        HBRUSH br = pCreateSolidBrush(cr(P)); HGDIOBJ ob = pSelectObject(d.dc, br);
        bd_clear(&s, S); bd_clear(&d, D);
        BOOL r = pBitBlt(d.dc, 0, 0, 2, 2, s.dc, 0, 0, ROP(code));
        DWORD e = (code >> c) & 1 ? 0xFFFFFF : 0;
        CHECKF(r && (bd_get(&d, 1, 1) & 0xFFFFFF) == e && (bd_get(&d, 0, 0) & 0xFFFFFF) == e,
               "rop %02X P=%d S=%d D=%d got 0x%lx want 0x%lx", code, !!(c & 4), !!(c & 2), !!(c & 1), (unsigned long)(bd_get(&d, 1, 1) & 0xFFFFFF), (unsigned long)e);
        pSelectObject(d.dc, ob); pDeleteObject(br);
    }
    bd_free(&s); bd_free(&d);
}

/* Pattern brush (8x8, random) with a non-zero brush origin; dest drawn at an offset. */
static void rop_pattern(int lo, int hi, int ox, int oy) {
    BD s, d, pt; int code, x, y; const int W = 20, H = 19, X0 = 3, Y0 = 2;
    NEED();
    s = bd_new(32, W, H); d = bd_new(32, 32, 32); pt = bd_new(32, 8, 8);
    if (!bd_ok(&s) || !bd_ok(&d) || !bd_ok(&pt)) { CHECK(0); bd_free(&s); bd_free(&d); bd_free(&pt); return; }
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) bd_put(&pt, x, y, rnd() & 0xFFFFFF);
    HBRUSH br = pCreatePatternBrush(pt.bm);
    CHECK(br != NULL);
    for (code = lo; code <= hi; code++) {
        int bad = 0; DWORD dv[32 * 32], sv[W * H];
        for (y = 0; y < H; y++) for (x = 0; x < W; x++) bd_put(&s, x, y, rnd() & 0xFFFFFF);
        for (y = 0; y < 32; y++) for (x = 0; x < 32; x++) bd_put(&d, x, y, rnd() & 0xFFFFFF);
        for (y = 0; y < H; y++) for (x = 0; x < W; x++) sv[y * W + x] = bd_get(&s, x, y);
        for (y = 0; y < 32; y++) for (x = 0; x < 32; x++) dv[y * 32 + x] = bd_get(&d, x, y);
        HGDIOBJ ob = pSelectObject(d.dc, br);
        pSetBrushOrgEx(d.dc, ox, oy, NULL);
        BOOL r = pBitBlt(d.dc, X0, Y0, W, H, s.dc, 0, 0, ROP(code));
        for (y = 0; y < 32 && !bad; y++) for (x = 0; x < 32; x++) {
            DWORD e = dv[y * 32 + x], g = bd_get(&d, x, y) & 0xFFFFFF;
            if (x >= X0 && x < X0 + W && y >= Y0 && y < Y0 + H)
                e = rop3(code, bd_get(&pt, ((x - ox) % 8 + 8) % 8, ((y - oy) % 8 + 8) % 8), sv[(y - Y0) * W + x - X0], dv[y * 32 + x]);
            e &= 0xFFFFFF;
            if (e != g) { bad = 1; CHECKF(0, "rop %02X origin (%d,%d) diff at (%d,%d) got 0x%lx want 0x%lx", code, ox, oy, x, y, (unsigned long)g, (unsigned long)e); break; }
        }
        CHECKF(r, "rop %02X ret %d", code, r);
        pSelectObject(d.dc, ob);
    }
    pDeleteObject(br);
    bd_free(&s); bd_free(&d); bd_free(&pt);
}

#define G32(n) TEST(blt_rop3_32_g##n) { rop_group(32, (n) * 16, (n) * 16 + 15); }
#define GS(n) TEST(blt_rop3_solid_g##n) { rop_solid((n) * 16, (n) * 16 + 15); }
#define GP(n) TEST(blt_rop3_pattern_g##n) { rop_pattern((n) * 16, (n) * 16 + 15, (n) % 4 == 0 ? 0 : (n) * 3 - 20, (n) % 3 == 0 ? 0 : 13 - (n) * 5); }
#define ALL16(M) M(0) M(1) M(2) M(3) M(4) M(5) M(6) M(7) M(8) M(9) M(10) M(11) M(12) M(13) M(14) M(15)
ALL16(G32)
ALL16(GS)
ALL16(GP)

TEST(blt_rop3_24bpp_00_3f) { rop_group(24, 0x00, 0x3F); }
TEST(blt_rop3_24bpp_40_7f) { rop_group(24, 0x40, 0x7F); }
TEST(blt_rop3_24bpp_80_bf) { rop_group(24, 0x80, 0xBF); }
TEST(blt_rop3_24bpp_c0_ff) { rop_group(24, 0xC0, 0xFF); }
TEST(blt_rop3_16bpp_00_3f) { rop_group(16, 0x00, 0x3F); }
TEST(blt_rop3_16bpp_40_7f) { rop_group(16, 0x40, 0x7F); }
TEST(blt_rop3_16bpp_80_bf) { rop_group(16, 0x80, 0xBF); }
TEST(blt_rop3_16bpp_c0_ff) { rop_group(16, 0xC0, 0xFF); }
TEST(blt_rop3_8bpp_00_7f) { rop_group(8, 0x00, 0x7F); }
TEST(blt_rop3_8bpp_80_ff) { rop_group(8, 0x80, 0xFF); }
TEST(blt_rop3_4bpp_00_7f) { rop_group(4, 0x00, 0x7F); }
TEST(blt_rop3_4bpp_80_ff) { rop_group(4, 0x80, 0xFF); }
TEST(blt_rop3_1bpp_all) { rop_group(1, 0x00, 0xFF); }

/* ---- named raster operations: explicit formulas, canonical constants ---- */
TEST(blt_rop_constants) {
    CHECK(SRCCOPY == 0x00CC0020); CHECK(SRCPAINT == 0x00EE0086); CHECK(SRCAND == 0x008800C6);
    CHECK(SRCINVERT == 0x00660046); CHECK(SRCERASE == 0x00440328); CHECK(NOTSRCCOPY == 0x00330008);
    CHECK(NOTSRCERASE == 0x001100A6); CHECK(MERGECOPY == 0x00C000CA); CHECK(MERGEPAINT == 0x00BB0226);
    CHECK(PATCOPY == 0x00F00021); CHECK(PATPAINT == 0x00FB0A09); CHECK(PATINVERT == 0x005A0049);
    CHECK(DSTINVERT == 0x00550009); CHECK(BLACKNESS == 0x00000042); CHECK(WHITENESS == 0x00FF0062);
    CHECK(CAPTUREBLT == 0x40000000); CHECK(NOMIRRORBITMAP == 0x80000000u);
    CHECK(MAKEROP4(0xCC0020, 0xAA0000) == (0xAA000000u | 0x00CC0020));
}

typedef DWORD (*named_fn)(DWORD P, DWORD S, DWORD D);
static void named_rop(DWORD rop, named_fn f, int use_src) {
    BD s, d; int x, y; const int N = 8;
    NEED();
    s = bd_new(32, N, N); d = bd_new(32, N, N);
    if (!bd_ok(&s) || !bd_ok(&d)) { CHECK(0); bd_free(&s); bd_free(&d); return; }
    DWORD P = 0x35A9C7, sv[64], dv[64];
    for (y = 0; y < N; y++) for (x = 0; x < N; x++) { bd_put(&s, x, y, sv[y * N + x] = rnd() & 0xFFFFFF); bd_put(&d, x, y, dv[y * N + x] = rnd() & 0xFFFFFF); }
    HBRUSH br = pCreateSolidBrush(cr(P)); HGDIOBJ ob = pSelectObject(d.dc, br);
    BOOL r = pBitBlt(d.dc, 0, 0, N, N, s.dc, 0, 0, rop);
    CHECKF(r, "BitBlt rop %08lx failed", (unsigned long)rop);
    int bad = 0;
    for (y = 0; y < N; y++) for (x = 0; x < N; x++)
        if ((bd_get(&d, x, y) & 0xFFFFFF) != (f(P, sv[y * N + x], dv[y * N + x]) & 0xFFFFFF)) bad++;
    CHECKF(!bad, "rop %08lx: %d pixels differ", (unsigned long)rop, bad);
    /* NULL source for source-less rops */
    if (!use_src) {
        for (y = 0; y < N; y++) for (x = 0; x < N; x++) bd_put(&d, x, y, dv[y * N + x]);
        r = pBitBlt(d.dc, 0, 0, N, N, NULL, 0, 0, rop);
        CHECKF(r, "BitBlt NULL src rop %08lx", (unsigned long)rop);
        bad = 0;
        for (y = 0; y < N; y++) for (x = 0; x < N; x++)
            if ((bd_get(&d, x, y) & 0xFFFFFF) != (f(P, 0, dv[y * N + x]) & 0xFFFFFF)) bad++;
        CHECKF(!bad, "NULL-src rop %08lx: %d pixels differ", (unsigned long)rop, bad);
    }
    pSelectObject(d.dc, ob); pDeleteObject(br);
    bd_free(&s); bd_free(&d);
}
#define NR(nm, src, expr) static DWORD nr_##nm(DWORD P, DWORD S, DWORD D) { (void)P;(void)S;(void)D; return expr; } \
    TEST(blt_named_##nm) { named_rop(nm, nr_##nm, src); }
NR(SRCCOPY, 1, S)
NR(SRCPAINT, 1, S | D)
NR(SRCAND, 1, S & D)
NR(SRCINVERT, 1, S ^ D)
NR(SRCERASE, 1, S & ~D)
NR(NOTSRCCOPY, 1, ~S)
NR(NOTSRCERASE, 1, ~(S | D))
NR(MERGECOPY, 1, S & P)
NR(MERGEPAINT, 1, ~S | D)
NR(PATCOPY, 0, P)
NR(PATPAINT, 1, P | ~S | D)
NR(PATINVERT, 0, P ^ D)
NR(DSTINVERT, 0, ~D)
NR(BLACKNESS, 0, 0)
NR(WHITENESS, 0, 0xFFFFFF)

/* ROP4 style flags accepted on a memory DC */
TEST(blt_rop_captureblt_flag) {
    BD s, d; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0x111111);
    BOOL r = pBitBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, SRCCOPY | CAPTUREBLT);
    CHECK(r);
    int bad = 0, x, y;
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != enc(x, y)) bad++;
    CHECKF(!bad, "%d pixels", bad);
    bd_free(&s); bd_free(&d);
}
TEST(blt_rop_nomirror_flag) {
    BD s, d; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0x111111);
    BOOL r = pBitBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, SRCCOPY | NOMIRRORBITMAP);
    CHECK(r);
    CHECK_PX(d, 5, 6, enc(5, 6), 0xFFFFFF);
    bd_free(&s); bd_free(&d);
}
TEST(blt_rop_both_flags) {
    BD s, d; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0);
    BOOL r = pBitBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, SRCCOPY | NOMIRRORBITMAP | CAPTUREBLT);
    CHECK(r);
    CHECK_PX(d, 7, 7, enc(7, 7), 0xFFFFFF);
    bd_free(&s); bd_free(&d);
}
TEST(blt_rop_invalid_high_bits) {
    BD s, d; NEED();
    s = bd_new(32, 4, 4); d = bd_new(32, 4, 4);
    static const DWORD bad[] = { 0x01000000, 0x02000000, 0x04000000, 0x08000000, 0x10000000, 0x20000000, 0x00000001 };
    int i;
    for (i = 0; i < 7; i++) {
        bd_clear(&s, 0xABCDEF); bd_clear(&d, 0x123456);
        SetLastError(0xDEAD);
        BOOL r = pBitBlt(d.dc, 0, 0, 4, 4, s.dc, 0, 0, SRCCOPY | bad[i]);
        t_note("BitBlt SRCCOPY|0x%08lx -> %d err=%lu dst=0x%06lx", (unsigned long)bad[i], r, GetLastError(), (unsigned long)(bd_get(&d, 0, 0) & 0xFFFFFF));
    }
    bd_free(&s); bd_free(&d);
}
TEST(blt_rop_unused_low_word) {
    /* For a ternary ROP only the high word selects the operation; check canonical vs bare index */
    BD s, d; NEED();
    s = bd_new(32, 4, 4); d = bd_new(32, 4, 4);
    bd_clear(&s, 0xABCDEF); bd_clear(&d, 0x123456);
    BOOL r = pBitBlt(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 0x00CC0000);
    t_note("BitBlt 0x00CC0000 -> %d dst=0x%06lx", r, (unsigned long)(bd_get(&d, 0, 0) & 0xFFFFFF));
    if (r) CHECK_PX(d, 0, 0, 0xABCDEF, 0xFFFFFF);
    bd_free(&s); bd_free(&d);
}

/* source-less ROP via BitBlt with NULL hdcSrc: every ROP that ignores S is accepted */
TEST(blt_null_src_sourceless_rops_all) {
    BD d; int code; NEED();
    d = bd_new(32, 4, 4);
    for (code = 0; code < 256; code++) {
        if (rop_uses_src(code)) continue;
        DWORD P = 0x2468AC, D = 0x13579B;
        HBRUSH br = pCreateSolidBrush(cr(P)); HGDIOBJ ob = pSelectObject(d.dc, br);
        bd_clear(&d, D);
        BOOL r = pBitBlt(d.dc, 0, 0, 4, 4, NULL, 0, 0, ROP(code));
        DWORD e = rop3(code, P, 0, D) & 0xFFFFFF;
        CHECKF(r && (bd_get(&d, 2, 2) & 0xFFFFFF) == e, "rop %02X ret %d got 0x%lx want 0x%lx", code, r, (unsigned long)(bd_get(&d, 2, 2) & 0xFFFFFF), (unsigned long)e);
        pSelectObject(d.dc, ob); pDeleteObject(br);
    }
    bd_free(&d);
}
TEST(blt_null_src_source_rops_fail) {
    BD d; int code; NEED();
    d = bd_new(32, 4, 4);
    for (code = 0; code < 256; code++) {
        if (!rop_uses_src(code)) continue;
        bd_clear(&d, 0x654321);
        BOOL r = pBitBlt(d.dc, 0, 0, 4, 4, NULL, 0, 0, ROP(code));
        CHECKF(!r, "rop %02X with NULL src succeeded", code);
        if (r) continue;
        CHECK_PX(d, 1, 1, 0x654321, 0xFFFFFF);
    }
    bd_free(&d);
}
