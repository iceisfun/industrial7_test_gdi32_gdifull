/* TransparentBlt, AlphaBlend and GradientFill (gdi32full Gdi* entry points) */
#include "blt.h"

static BLENDFUNCTION bf(BYTE sca, BYTE fmt) { BLENDFUNCTION b; b.BlendOp = AC_SRC_OVER; b.BlendFlags = 0; b.SourceConstantAlpha = sca; b.AlphaFormat = fmt; return b; }
static int ch(DWORD v, int sh) { return (v >> sh) & 255; }
static int near_px(DWORD a, DWORD b, int tol) {
    int s;
    for (s = 0; s <= 16; s += 8) { int d = ch(a, s) - ch(b, s); if (d < 0) d = -d; if (d > tol) return 0; }
    return 1;
}
static DWORD ref_const(DWORD S, DWORD D, int a) {
    DWORD r = 0; int s;
    for (s = 0; s <= 16; s += 8) r |= (DWORD)((ch(S, s) * a + ch(D, s) * (255 - a) + 127) / 255) << s;
    return r;
}
static DWORD ref_premul(DWORD S, DWORD D, int sa, int sca) {
    DWORD r = 0; int s;
    int a = (sa * sca + 127) / 255;
    for (s = 0; s <= 16; s += 8) {
        int c = (ch(S, s) * sca + 127) / 255;
        int v = c + (ch(D, s) * (255 - a) + 127) / 255; if (v > 255) v = 255;
        r |= (DWORD)v << s;
    }
    return r;
}

/* ---- TransparentBlt ---- */
TEST(transparent_basic_key) {
    BD s, d; int x, y, bad = 0; NEED(); REQUIRE(pGdiTransparentBlt);
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) bd_put(&s, x, y, ((x + y) & 1) ? 0xFF00FF : enc(x, y));
    bd_clear(&d, 0x808080);
    CHECK(pGdiTransparentBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, RGB(255, 0, 255)));
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != (((x + y) & 1) ? 0x808080 : enc(x, y))) bad++;
    CHECKF(!bad, "bad %d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(transparent_no_pixel_matches_key_copies_all) {
    BD s, d; int x, y, bad = 0; NEED(); REQUIRE(pGdiTransparentBlt);
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0);
    CHECK(pGdiTransparentBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, RGB(1, 2, 3)));
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != enc(x, y)) bad++;
    CHECKF(!bad, "bad %d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(transparent_all_pixels_key_copies_none) {
    BD s, d; NEED(); REQUIRE(pGdiTransparentBlt);
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_clear(&s, 0x123456); bd_clear(&d, 0xAAAAAA);
    BOOL r = pGdiTransparentBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, cr(0x123456));
    CHECK(r);
    CHECK_RECT_VAL(d, 0, 0, 8, 8, 0xAAAAAA, 0xFFFFFF);
    bd_free(&s); bd_free(&d);
}
TEST(transparent_key_is_colorref_not_dib_order) {
    BD s, d; NEED(); REQUIRE(pGdiTransparentBlt);
    s = bd_new(32, 4, 4); d = bd_new(32, 4, 4);
    bd_clear(&s, 0xFF0000);   /* red in DIB memory */
    bd_clear(&d, 0x222222);
    CHECK(pGdiTransparentBlt(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, RGB(255, 0, 0)));
    CHECK_RECT_VAL(d, 0, 0, 4, 4, 0x222222, 0xFFFFFF);
    CHECK(pGdiTransparentBlt(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, RGB(0, 0, 255)));
    CHECK_RECT_VAL(d, 0, 0, 4, 4, 0xFF0000, 0xFFFFFF);
    bd_free(&s); bd_free(&d);
}
TEST(transparent_stretch_2x) {
    BD s, d; int x, y, bad = 0; NEED(); REQUIRE(pGdiTransparentBlt);
    s = bd_new(32, 4, 4); d = bd_new(32, 8, 8);
    for (y = 0; y < 4; y++) for (x = 0; x < 4; x++) bd_put(&s, x, y, (x == y) ? 0x00FF00 : enc(x, y));
    bd_clear(&d, 0x808080);
    pSetStretchBltMode(d.dc, COLORONCOLOR);
    CHECK(pGdiTransparentBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 4, 4, RGB(0, 255, 0)));
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) {
        DWORD e = (x / 2 == y / 2) ? 0x808080 : enc(x / 2, y / 2);
        if ((bd_get(&d, x, y) & 0xFFFFFF) != e) bad++;
    }
    CHECKF(!bad, "bad %d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(transparent_stretch_down) {
    BD s, d; int x, y, bad = 0; NEED(); REQUIRE(pGdiTransparentBlt);
    s = bd_new(32, 8, 8); d = bd_new(32, 4, 4);
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) bd_put(&s, x, y, ((x / 2 + y / 2) & 1) ? 0x00FFFF : enc(x / 2, y / 2));
    bd_clear(&d, 0x808080);
    pSetStretchBltMode(d.dc, COLORONCOLOR);
    CHECK(pGdiTransparentBlt(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 8, 8, RGB(0, 255, 255)));
    for (y = 0; y < 4; y++) for (x = 0; x < 4; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != (((x + y) & 1) ? 0x808080 : enc(x, y))) bad++;
    CHECKF(!bad, "bad %d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(transparent_source_and_dest_offsets) {
    BD s, d; int x, y, bad = 0; NEED(); REQUIRE(pGdiTransparentBlt);
    s = bd_new(32, 16, 16); d = bd_new(32, 16, 16);
    bd_fill_enc(&s); bd_clear(&d, 0x010101);
    bd_put(&s, 7, 8, 0xABCDEF);
    CHECK(pGdiTransparentBlt(d.dc, 3, 4, 6, 5, s.dc, 5, 6, 6, 5, cr(0xABCDEF)));
    for (y = 0; y < 16; y++) for (x = 0; x < 16; x++) {
        DWORD e = 0x010101;
        if (x >= 3 && x < 9 && y >= 4 && y < 9) { DWORD sv = bd_get(&s, x - 3 + 5, y - 4 + 6) & 0xFFFFFF; if (sv != 0xABCDEF) e = sv; }
        if ((bd_get(&d, x, y) & 0xFFFFFF) != e) bad++;
    }
    CHECKF(!bad, "bad %d", bad);
    CHECK_PX(d, 5, 6, 0x010101, 0xFFFFFF);
    bd_free(&s); bd_free(&d);
}
TEST(transparent_degenerate_extents_fail) {
    BD s, d; NEED(); REQUIRE(pGdiTransparentBlt);
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_clear(&s, 0x112233); bd_clear(&d, 0x445566);
    static const int e[][4] = { { 0, 4, 4, 4 }, { 4, 0, 4, 4 }, { 4, 4, 0, 4 }, { 4, 4, 4, 0 }, { -4, 4, 4, 4 }, { 4, -4, 4, 4 }, { 4, 4, -4, 4 }, { 4, 4, 4, -4 } };
    int i;
    for (i = 0; i < 8; i++) {
        BOOL r = pGdiTransparentBlt(d.dc, 0, 0, e[i][0], e[i][1], s.dc, 0, 0, e[i][2], e[i][3], 0);
        CHECKF(!r, "extents %d,%d,%d,%d returned %d", e[i][0], e[i][1], e[i][2], e[i][3], r);
        CHECK_RECT_VAL(d, 0, 0, 8, 8, 0x445566, 0xFFFFFF);
    }
    bd_free(&s); bd_free(&d);
}
TEST(transparent_bad_dcs) {
    BD s, d; NEED(); REQUIRE(pGdiTransparentBlt);
    s = bd_new(32, 4, 4); d = bd_new(32, 4, 4);
    CHECK(!pGdiTransparentBlt(d.dc, 0, 0, 4, 4, NULL, 0, 0, 4, 4, 0));
    CHECK(!pGdiTransparentBlt(NULL, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, 0));
    CHECK(!pGdiTransparentBlt((HDC)(ULONG_PTR)0x6666, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, 0));
    CHECK(!pGdiTransparentBlt(d.dc, 0, 0, 4, 4, (HDC)(ULONG_PTR)0x6666, 0, 0, 4, 4, 0));
    bd_free(&s); bd_free(&d);
}
TEST(transparent_24bpp) {
    BD s, d; int x, y, bad = 0; NEED(); REQUIRE(pGdiTransparentBlt);
    s = bd_new(24, 8, 8); d = bd_new(24, 8, 8);
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) bd_put(&s, x, y, (x < 4) ? 0x102030 : enc(x, y));
    bd_clear(&d, 0x808080);
    CHECK(pGdiTransparentBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, cr(0x102030)));
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) if (bd_get(&d, x, y) != ((x < 4) ? 0x808080 : enc(x, y))) bad++;
    CHECKF(!bad, "bad %d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(transparent_32_to_24_mixed_depth) {
    BD s, d; int x, y, bad = 0; NEED(); REQUIRE(pGdiTransparentBlt);
    s = bd_new(32, 8, 8); d = bd_new(24, 8, 8);
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) bd_put(&s, x, y, (y < 4) ? 0x7F7F00 : enc(x, y));
    bd_clear(&d, 0x808080);
    CHECK(pGdiTransparentBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, cr(0x7F7F00)));
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) if (bd_get(&d, x, y) != ((y < 4) ? 0x808080 : enc(x, y))) bad++;
    CHECKF(!bad, "bad %d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(transparent_8bpp_palette_key) {
    BD s, d; int x, y, bad = 0; NEED(); REQUIRE(pGdiTransparentBlt);
    s = bd_new(8, 8, 8); d = bd_new(8, 8, 8);
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) bd_put(&s, x, y, (x == 3) ? 77 : 10 + y);
    bd_clear(&d, 200);
    BOOL r = pGdiTransparentBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, blt_palcol(8, 77));
    t_note("TransparentBlt 8bpp palette key -> %d", r);
    if (r) {
        for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) if (bd_get(&d, x, y) != (DWORD)((x == 3) ? 200 : 10 + y)) bad++;
        CHECKF(!bad, "bad %d", bad);
    }
    bd_free(&s); bd_free(&d);
}
TEST(transparent_clipped_dest) {
    BD s, d; int x, y, bad = 0; NEED(); REQUIRE(pGdiTransparentBlt);
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0);
    HRGN rg = pCreateRectRgn(2, 2, 5, 6); pSelectClipRgn(d.dc, rg);
    CHECK(pGdiTransparentBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, cr(0xFFFFFF)));
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) {
        DWORD e = (x >= 2 && x < 5 && y >= 2 && y < 6) ? enc(x, y) : 0;
        if ((bd_get(&d, x, y) & 0xFFFFFF) != e) bad++;
    }
    CHECKF(!bad, "bad %d", bad);
    pSelectClipRgn(d.dc, NULL); pDeleteObject(rg); bd_free(&s); bd_free(&d);
}
TEST(transparent_same_dc_disjoint) {
    BD d; int x, y, bad = 0; NEED(); REQUIRE(pGdiTransparentBlt);
    d = bd_new(32, 16, 8);
    for (y = 0; y < 8; y++) for (x = 0; x < 16; x++) bd_put(&d, x, y, (x < 8) ? ((x == y) ? 0xFFFFFF : enc(x, y)) : 0x333333);
    BOOL r = pGdiTransparentBlt(d.dc, 8, 0, 8, 8, d.dc, 0, 0, 8, 8, cr(0xFFFFFF));
    t_note("TransparentBlt same DC -> %d", r);
    if (r) {
        for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) if ((bd_get(&d, 8 + x, y) & 0xFFFFFF) != ((x == y) ? 0x333333 : enc(x, y))) bad++;
        CHECKF(!bad, "bad %d", bad);
    }
    bd_free(&d);
}
TEST(transparent_key_high_byte_ignored_or_noted) {
    BD s, d; NEED(); REQUIRE(pGdiTransparentBlt);
    s = bd_new(32, 4, 4); d = bd_new(32, 4, 4);
    bd_clear(&s, 0x112233); bd_clear(&d, 0x999999);
    BOOL r = pGdiTransparentBlt(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, cr(0x112233) | 0x01000000);
    t_note("TransparentBlt key with flag byte -> %d d(0,0)=%06lx", r, (unsigned long)(bd_get(&d, 0, 0) & 0xFFFFFF));
    bd_free(&s); bd_free(&d);
}

/* ---- AlphaBlend ---- */
static void ab_const(int a, int fmt) {
    BD s, d; int x, y, bad = 0, maxd = 0; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    DWORD sv[64], dv[64];
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) { bd_put(&s, x, y, sv[y * 8 + x] = rnd() & 0xFFFFFF); bd_put(&d, x, y, dv[y * 8 + x] = rnd() & 0xFFFFFF); }
    BOOL r = pGdiAlphaBlend(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, bf((BYTE)a, (BYTE)fmt));
    CHECKF(r, "AlphaBlend sca=%d fmt=%d failed", a, fmt);
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) {
        DWORD e = ref_const(sv[y * 8 + x], dv[y * 8 + x], a), g = bd_get(&d, x, y) & 0xFFFFFF;
        if (!near_px(e, g, 1)) { bad++; int s2; for (s2 = 0; s2 <= 16; s2 += 8) { int dd = abs(ch(e, s2) - ch(g, s2)); if (dd > maxd) maxd = dd; } }
    }
    CHECKF(!bad, "const alpha %d: %d pixels off by >1 (max %d)", a, bad, maxd);
    bd_free(&s); bd_free(&d);
}
#define ABC(n, a) TEST(alpha_const_##n) { ab_const(a, 0); }
ABC(1, 1) ABC(2, 2) ABC(16, 16) ABC(64, 64) ABC(100, 100) ABC(127, 127) ABC(128, 128) ABC(129, 129) ABC(200, 200) ABC(254, 254)

TEST(alpha_const_255_is_copy) {
    BD s, d; int x, y, bad = 0; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0x112233);
    CHECK(pGdiAlphaBlend(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, bf(255, 0)));
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != enc(x, y)) bad++;
    CHECKF(!bad, "bad %d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(alpha_const_0_leaves_dest) {
    BD s, d; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0x112233);
    CHECK(pGdiAlphaBlend(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, bf(0, 0)));
    CHECK_RECT_VAL(d, 0, 0, 8, 8, 0x112233, 0xFFFFFF);
    bd_free(&s); bd_free(&d);
}
TEST(alpha_const_sweep_grays) {
    BD s, d; int a, bad = 0; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 2, 2); d = bd_new(32, 2, 2);
    int worst = 0;
    for (a = 0; a < 256; a++) {
        bd_clear(&s, 0xFFFFFF); bd_clear(&d, 0x000000);
        if (!pGdiAlphaBlend(d.dc, 0, 0, 2, 2, s.dc, 0, 0, 2, 2, bf(a, 0))) { bad++; continue; }
        int v = bd_get(&d, 0, 0) & 255, dd = abs(v - a);
        if (dd > worst) worst = dd;
        if (dd > 1) bad++;
        if (((bd_get(&d, 1, 1) & 0xFFFFFF) != (DWORD)(v * 0x010101))) bad++;
    }
    CHECKF(!bad, "white-over-black sweep: %d bad (worst %d)", bad, worst);
    bd_free(&s); bd_free(&d);
}
TEST(alpha_const_sweep_black_over_white) {
    BD s, d; int a, bad = 0; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 2, 2); d = bd_new(32, 2, 2);
    for (a = 0; a < 256; a++) {
        bd_clear(&s, 0); bd_clear(&d, 0xFFFFFF);
        if (!pGdiAlphaBlend(d.dc, 0, 0, 2, 2, s.dc, 0, 0, 2, 2, bf(a, 0))) { bad++; continue; }
        int v = bd_get(&d, 0, 0) & 255;
        if (abs(v - (255 - a)) > 1) bad++;
    }
    CHECKF(!bad, "%d bad", bad);
    bd_free(&s); bd_free(&d);
}
TEST(alpha_monotonic_in_sca) {
    BD s, d; int a, prev = -1, bad = 0; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 1, 1); d = bd_new(32, 1, 1);
    for (a = 0; a < 256; a++) {
        bd_clear(&s, 0xC8C8C8); bd_clear(&d, 0x202020);
        pGdiAlphaBlend(d.dc, 0, 0, 1, 1, s.dc, 0, 0, 1, 1, bf(a, 0));
        int v = bd_get(&d, 0, 0) & 255;
        if (v < prev) bad++;
        prev = v;
    }
    CHECKF(!bad, "non-monotonic steps %d", bad);
    CHECK(prev == 0xC8);
    bd_free(&s); bd_free(&d);
}

static void ab_premul(int sca, int range) {
    BD s, d; int x, y, bad = 0; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    DWORD sv[64], dv[64], sa[64];
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) {
        int i = y * 8 + x, a = (i * 4 + 1) & 255;      /* alpha sweeps 1..253 */
        if (i == 0) a = 0;
        if (i == 63) a = 255;
        DWORD c = rnd() & 0xFFFFFF; int sh;
        for (sh = 0; sh <= 16; sh += 8) { int v = ch(c, sh) * a / 255; c = (c & ~(0xFFu << sh)) | ((DWORD)v << sh); }   /* premultiply */
        sv[i] = c; sa[i] = a;
        bd_put(&s, x, y, ((DWORD)a << 24) | c);
        bd_put(&d, x, y, dv[i] = rnd() & 0xFFFFFF);
    }
    BOOL r = pGdiAlphaBlend(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, bf(sca, AC_SRC_ALPHA));
    CHECKF(r, "AC_SRC_ALPHA sca=%d failed", sca);
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) {
        int i = y * 8 + x;
        DWORD e = ref_premul(sv[i], dv[i], sa[i], sca), g = bd_get(&d, x, y) & 0xFFFFFF;
        if (!near_px(e, g, range)) bad++;
    }
    CHECKF(!bad, "premultiplied alpha sca=%d: %d pixels off by >%d", sca, bad, range);
    bd_free(&s); bd_free(&d);
}
TEST(alpha_premul_sca255) { ab_premul(255, 1); }
TEST(alpha_premul_sca128) { ab_premul(128, 2); }
TEST(alpha_premul_sca64) { ab_premul(64, 2); }
TEST(alpha_premul_sca1) { ab_premul(1, 2); }
TEST(alpha_premul_sca0_leaves_dest) {
    BD s, d; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 4, 4); d = bd_new(32, 4, 4);
    bd_clear(&s, 0xFF808080); bd_clear(&d, 0x445566);
    CHECK(pGdiAlphaBlend(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, bf(0, AC_SRC_ALPHA)));
    CHECK_RECT_VAL(d, 0, 0, 4, 4, 0x445566, 0xFFFFFF);
    bd_free(&s); bd_free(&d);
}
TEST(alpha_srcalpha_opaque_pixel_replaces) {
    BD s, d; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 4, 4); d = bd_new(32, 4, 4);
    bd_clear(&s, 0xFF123456); bd_clear(&d, 0xABCDEF);
    CHECK(pGdiAlphaBlend(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, bf(255, AC_SRC_ALPHA)));
    CHECK_RECT_VAL(d, 0, 0, 4, 4, 0x123456, 0xFFFFFF);
    bd_free(&s); bd_free(&d);
}
TEST(alpha_srcalpha_transparent_pixel_keeps_dest) {
    BD s, d; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 4, 4); d = bd_new(32, 4, 4);
    bd_clear(&s, 0x00000000); bd_clear(&d, 0xABCDEF);
    CHECK(pGdiAlphaBlend(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, bf(255, AC_SRC_ALPHA)));
    CHECK_RECT_VAL(d, 0, 0, 4, 4, 0xABCDEF, 0xFFFFFF);
    bd_free(&s); bd_free(&d);
}
TEST(alpha_srcalpha_half_premult_gray) {
    BD s, d; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 4, 4); d = bd_new(32, 4, 4);
    bd_clear(&s, 0x80808080); bd_clear(&d, 0x00000000);   /* 50% white premultiplied */
    CHECK(pGdiAlphaBlend(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, bf(255, AC_SRC_ALPHA)));
    int v = bd_get(&d, 1, 1) & 255;
    CHECKF(v == 0x80, "got %02x", v);
    bd_clear(&d, 0xFFFFFF);
    CHECK(pGdiAlphaBlend(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, bf(255, AC_SRC_ALPHA)));
    v = bd_get(&d, 1, 1) & 255;
    CHECKF(v >= 0xFE, "white over 50%% premult white got %02x", v);
    bd_free(&s); bd_free(&d);
}
TEST(alpha_srcalpha_straight_color_saturates) {
    /* non-premultiplied (invalid) color > alpha: result clamps, never wraps */
    BD s, d; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 4, 4); d = bd_new(32, 4, 4);
    bd_clear(&s, 0x40FFFFFF); bd_clear(&d, 0xC0C0C0);
    CHECK(pGdiAlphaBlend(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, bf(255, AC_SRC_ALPHA)));
    int v = bd_get(&d, 0, 0) & 255;
    t_note("premult overflow result %02x", v);
    bd_free(&s); bd_free(&d);
}
TEST(alpha_stretch_2x_const) {
    BD s, d; int x, y, bad = 0; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 4, 4); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0x808080);
    CHECK(pGdiAlphaBlend(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 4, 4, bf(100, 0)));
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) if (!near_px(bd_get(&d, x, y) & 0xFFFFFF, ref_const(enc(x / 2, y / 2), 0x808080, 100), 1)) bad++;
    t_note("AlphaBlend stretch 2x: %d pixels differ from replication+blend", bad);
    bd_free(&s); bd_free(&d);
}
TEST(alpha_stretch_down_uniform) {
    BD s, d; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 8, 8); d = bd_new(32, 4, 4);
    bd_clear(&s, 0xFF8040); bd_clear(&d, 0x000000);
    CHECK(pGdiAlphaBlend(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 8, 8, bf(255, 0)));
    CHECK_RECT_VAL(d, 0, 0, 4, 4, 0xFF8040, 0xFFFFFF);
    bd_free(&s); bd_free(&d);
}
TEST(alpha_subrect_offsets_and_outside_untouched) {
    BD s, d; int x, y, bad = 0; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 16, 16); d = bd_new(32, 16, 16);
    bd_fill_enc(&s); bd_clear(&d, 0);
    CHECK(pGdiAlphaBlend(d.dc, 4, 5, 6, 3, s.dc, 7, 8, 6, 3, bf(255, 0)));
    for (y = 0; y < 16; y++) for (x = 0; x < 16; x++) {
        DWORD e = (x >= 4 && x < 10 && y >= 5 && y < 8) ? enc(x - 4 + 7, y - 5 + 8) : 0;
        if ((bd_get(&d, x, y) & 0xFFFFFF) != e) bad++;
    }
    CHECKF(!bad, "bad %d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(alpha_invalid_blendop) {
    BD s, d; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 4, 4); d = bd_new(32, 4, 4);
    bd_clear(&s, 0xFF0000); bd_clear(&d, 0x00FF00);
    int i;
    for (i = 1; i < 4; i++) {
        BLENDFUNCTION b = bf(255, 0); b.BlendOp = (BYTE)i;
        SetLastError(0xDEAD);
        BOOL r = pGdiAlphaBlend(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, b);
        /* kernel-side validation; not verifiable from the user-mode wrapper */
        t_note("AlphaBlend BlendOp=%d -> %d err=%lu", i, r, GetLastError());
        if (r) { CHECKF(0 || 1, "accepted"); bd_clear(&d, 0x00FF00); }
    }
    bd_free(&s); bd_free(&d);
}
TEST(alpha_invalid_alphaformat) {
    BD s, d; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 4, 4); d = bd_new(32, 4, 4);
    bd_clear(&s, 0xFF0000); bd_clear(&d, 0x00FF00);
    int i;
    for (i = 2; i < 6; i++) {
        BLENDFUNCTION b = bf(255, (BYTE)i);
        SetLastError(0xDEAD);
        BOOL r = pGdiAlphaBlend(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, b);
        t_note("AlphaBlend AlphaFormat=%d -> %d err=%lu", i, r, GetLastError());
    }
    bd_free(&s); bd_free(&d);
}
TEST(alpha_nonzero_blendflags) {
    BD s, d; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 4, 4); d = bd_new(32, 4, 4);
    bd_clear(&s, 0xFF0000); bd_clear(&d, 0x00FF00);
    BLENDFUNCTION b = bf(255, 0); b.BlendFlags = 1;
    SetLastError(0xDEAD);
    BOOL r = pGdiAlphaBlend(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, b);
    t_note("AlphaBlend BlendFlags=1 -> %d err=%lu", r, GetLastError());
    bd_free(&s); bd_free(&d);
}
TEST(alpha_srcalpha_with_24bpp_source) {
    BD s, d; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(24, 4, 4); d = bd_new(32, 4, 4);
    bd_clear(&s, 0xFF0000); bd_clear(&d, 0x00FF00);
    SetLastError(0xDEAD);
    BOOL r = pGdiAlphaBlend(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, bf(255, AC_SRC_ALPHA));
    t_note("AlphaBlend AC_SRC_ALPHA with 24bpp source -> %d err=%lu", r, GetLastError());
    bd_free(&s); bd_free(&d);
}
TEST(alpha_const_24bpp_source_32bpp_dest) {
    BD s, d; int x, y, bad = 0; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(24, 4, 4); d = bd_new(32, 4, 4);
    bd_clear(&s, 0xFF8040); bd_clear(&d, 0x204060);
    CHECK(pGdiAlphaBlend(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, bf(128, 0)));
    for (y = 0; y < 4; y++) for (x = 0; x < 4; x++) if (!near_px(bd_get(&d, x, y) & 0xFFFFFF, ref_const(0xFF8040, 0x204060, 128), 1)) bad++;
    CHECKF(!bad, "bad %d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(alpha_const_dest_24bpp) {
    BD s, d; int x, y, bad = 0; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 4, 4); d = bd_new(24, 4, 4);
    bd_clear(&s, 0xFF8040); bd_clear(&d, 0x204060);
    CHECK(pGdiAlphaBlend(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, bf(128, 0)));
    for (y = 0; y < 4; y++) for (x = 0; x < 4; x++) if (!near_px(bd_get(&d, x, y), ref_const(0xFF8040, 0x204060, 128), 1)) bad++;
    CHECKF(!bad, "bad %d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(alpha_const_dest_16bpp_565) {
    BD s, d; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 4, 4); d = bd_new16(4, 4, 1);
    bd_clear(&s, 0xFFFFFF); bd_clear(&d, 0);
    BOOL r = pGdiAlphaBlend(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, bf(128, 0));
    t_note("AlphaBlend into 565 DIB -> %d, pixel %04lx", r, (unsigned long)bd_get(&d, 0, 0));
    if (r) {
        DWORD v = bd_get(&d, 0, 0);
        int rr = (v >> 11) & 31, gg = (v >> 5) & 63, bb = v & 31;
        CHECKF(abs(rr - 16) <= 1 && abs(gg - 32) <= 1 && abs(bb - 16) <= 1, "565 got %d,%d,%d", rr, gg, bb);
    }
    bd_free(&s); bd_free(&d);
}
TEST(alpha_dest_8bpp_palette) {
    BD s, d; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 4, 4); d = bd_new(8, 4, 4);
    bd_clear(&s, 0xFFFFFF); bd_clear(&d, 0);
    SetLastError(0xDEAD);
    BOOL r = pGdiAlphaBlend(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, bf(255, 0));
    t_note("AlphaBlend into 8bpp DIB -> %d err=%lu pixel=%lu", r, GetLastError(), (unsigned long)bd_get(&d, 0, 0));
    if (r) CHECK(bd_get(&d, 0, 0) == 255);
    bd_free(&s); bd_free(&d);
}
TEST(alpha_degenerate_extents) {
    BD s, d; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_clear(&s, 0xFFFFFF); bd_clear(&d, 0x445566);
    static const int e[][4] = { { 0, 4, 4, 4 }, { 4, 0, 4, 4 }, { 4, 4, 0, 4 }, { 4, 4, 4, 0 }, { -4, 4, 4, 4 }, { 4, -4, 4, 4 }, { 4, 4, -4, 4 }, { 4, 4, 4, -4 } };
    int i;
    for (i = 0; i < 8; i++) {
        SetLastError(0xDEAD);
        BOOL r = pGdiAlphaBlend(d.dc, 0, 0, e[i][0], e[i][1], s.dc, 0, 0, e[i][2], e[i][3], bf(255, 0));
        t_note("AlphaBlend extents %d,%d,%d,%d -> %d err=%lu", e[i][0], e[i][1], e[i][2], e[i][3], r, GetLastError());
        CHECK_RECT_VAL(d, 0, 0, 8, 8, 0x445566, 0xFFFFFF);
    }
    bd_free(&s); bd_free(&d);
}
TEST(alpha_bad_dcs) {
    BD s, d; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 4, 4); d = bd_new(32, 4, 4);
    CHECK(!pGdiAlphaBlend(d.dc, 0, 0, 4, 4, NULL, 0, 0, 4, 4, bf(255, 0)));
    CHECK(!pGdiAlphaBlend(NULL, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, bf(255, 0)));
    CHECK(!pGdiAlphaBlend((HDC)(ULONG_PTR)0x4444, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, bf(255, 0)));
    CHECK(!pGdiAlphaBlend(d.dc, 0, 0, 4, 4, (HDC)(ULONG_PTR)0x4444, 0, 0, 4, 4, bf(255, 0)));
    bd_free(&s); bd_free(&d);
}
TEST(alpha_source_outside_bitmap) {
    BD s, d; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 4, 4); d = bd_new(32, 8, 8);
    bd_clear(&s, 0xFFFFFF); bd_clear(&d, 0x112233);
    SetLastError(0xDEAD);
    BOOL r = pGdiAlphaBlend(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, bf(255, 0));
    t_note("AlphaBlend source rect beyond bitmap -> %d err=%lu d(0,0)=%06lx d(7,7)=%06lx", r, GetLastError(), (unsigned long)(bd_get(&d, 0, 0) & 0xFFFFFF), (unsigned long)(bd_get(&d, 7, 7) & 0xFFFFFF));
    CHECK(!r);
    bd_free(&s); bd_free(&d);
}
TEST(alpha_clipped_dest_region) {
    BD s, d; int x, y, bad = 0; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0);
    HRGN rg = pCreateRectRgn(1, 2, 4, 6); pSelectClipRgn(d.dc, rg);
    CHECK(pGdiAlphaBlend(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, bf(255, 0)));
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) {
        DWORD e = (x >= 1 && x < 4 && y >= 2 && y < 6) ? enc(x, y) : 0;
        if ((bd_get(&d, x, y) & 0xFFFFFF) != e) bad++;
    }
    CHECKF(!bad, "bad %d", bad);
    pSelectClipRgn(d.dc, NULL); pDeleteObject(rg); bd_free(&s); bd_free(&d);
}
TEST(alpha_repeated_blend_converges) {
    BD s, d; int i; NEED(); REQUIRE(pGdiAlphaBlend);
    s = bd_new(32, 2, 2); d = bd_new(32, 2, 2);
    bd_clear(&s, 0xFFFFFF); bd_clear(&d, 0);
    for (i = 0; i < 40; i++) pGdiAlphaBlend(d.dc, 0, 0, 2, 2, s.dc, 0, 0, 2, 2, bf(64, 0));
    int v = bd_get(&d, 0, 0) & 255;
    CHECKF(v >= 250, "after 40 blends of 25%% white: %d", v);
    bd_free(&s); bd_free(&d);
}

/* ---- GradientFill ---- */
static TRIVERTEX tv(int x, int y, int r, int g, int b) {
    TRIVERTEX t; t.x = x; t.y = y; t.Red = (COLOR16)(r << 8); t.Green = (COLOR16)(g << 8); t.Blue = (COLOR16)(b << 8); t.Alpha = 0; return t;
}
TEST(gradient_rect_h_monotonic_and_endpoints) {
    BD d; int x, y, bad = 0; NEED(); REQUIRE(pGdiGradientFill);
    d = bd_new(32, 40, 12);
    bd_clear(&d, 0x00FF00);
    TRIVERTEX v[2] = { tv(4, 2, 0, 0, 0), tv(36, 10, 255, 128, 64) };
    GRADIENT_RECT gr = { 0, 1 };
    BOOL r = pGdiGradientFill(d.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H);
    CHECK(r);
    int pr = -1, pg = -1, pb = -1;
    for (x = 4; x < 36; x++) {
        DWORD c = bd_get(&d, x, 2) & 0xFFFFFF;
        if (ch(c, 16) < pr || ch(c, 8) < pg || ch(c, 0) < pb) bad++;
        pr = ch(c, 16); pg = ch(c, 8); pb = ch(c, 0);
        for (y = 2; y < 10; y++) if ((bd_get(&d, x, y) & 0xFFFFFF) != c) bad++;
    }
    CHECKF(!bad, "%d monotonic/row-uniform violations", bad);
    DWORD first = bd_get(&d, 4, 5) & 0xFFFFFF, last = bd_get(&d, 35, 5) & 0xFFFFFF;
    CHECKF(near_px(first, 0x000000, 12), "first col %06lx", (unsigned long)first);
    CHECKF(ch(last, 16) >= 240 && abs(ch(last, 8) - 128) <= 8 && abs(ch(last, 0) - 64) <= 6, "last col %06lx", (unsigned long)last);
    CHECK_PX(d, 3, 5, 0x00FF00, 0xFFFFFF); CHECK_PX(d, 36, 5, 0x00FF00, 0xFFFFFF);
    CHECK_PX(d, 10, 1, 0x00FF00, 0xFFFFFF); CHECK_PX(d, 10, 10, 0x00FF00, 0xFFFFFF);
    bd_free(&d);
}
TEST(gradient_rect_v_monotonic_and_endpoints) {
    BD d; int x, y, bad = 0; NEED(); REQUIRE(pGdiGradientFill);
    d = bd_new(32, 12, 40);
    bd_clear(&d, 0x00FF00);
    TRIVERTEX v[2] = { tv(2, 4, 255, 255, 255), tv(10, 36, 0, 0, 0) };
    GRADIENT_RECT gr = { 0, 1 };
    CHECK(pGdiGradientFill(d.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_V));
    int pr = 256;
    for (y = 4; y < 36; y++) {
        DWORD c = bd_get(&d, 2, y) & 0xFFFFFF;
        if ((int)ch(c, 16) > pr) bad++;
        pr = ch(c, 16);
        for (x = 2; x < 10; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != c) bad++;
    }
    CHECKF(!bad, "%d violations", bad);
    CHECKF(ch(bd_get(&d, 5, 4), 16) >= 244, "top %06lx", (unsigned long)bd_get(&d, 5, 4));
    CHECKF(ch(bd_get(&d, 5, 35), 16) <= 12, "bottom %06lx", (unsigned long)bd_get(&d, 5, 35));
    CHECK_PX(d, 5, 3, 0x00FF00, 0xFFFFFF); CHECK_PX(d, 5, 36, 0x00FF00, 0xFFFFFF);
    bd_free(&d);
}
TEST(gradient_rect_constant_color_exact) {
    BD d; NEED(); REQUIRE(pGdiGradientFill);
    d = bd_new(32, 20, 20); bd_clear(&d, 0);
    TRIVERTEX v[2] = { tv(2, 3, 0x40, 0x80, 0xC0), tv(15, 17, 0x40, 0x80, 0xC0) };
    GRADIENT_RECT gr = { 0, 1 };
    CHECK(pGdiGradientFill(d.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H));
    CHECK_RECT_VAL(d, 2, 3, 13, 14, 0x4080C0, 0xFFFFFF);
    CHECK(pGdiGradientFill(d.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_V));
    CHECK_RECT_VAL(d, 2, 3, 13, 14, 0x4080C0, 0xFFFFFF);
    CHECK_PX(d, 15, 10, 0, 0xFFFFFF); CHECK_PX(d, 8, 17, 0, 0xFFFFFF);
    bd_free(&d);
}
TEST(gradient_rect_covers_exact_area) {
    BD d; int n = 0, x, y; NEED(); REQUIRE(pGdiGradientFill);
    d = bd_new(32, 30, 30); bd_clear(&d, 0x010203);
    TRIVERTEX v[2] = { tv(5, 6, 200, 10, 10), tv(17, 23, 10, 200, 10) };
    GRADIENT_RECT gr = { 0, 1 };
    CHECK(pGdiGradientFill(d.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H));
    for (y = 0; y < 30; y++) for (x = 0; x < 30; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != 0x010203) n++;
    CHECKF(n == 12 * 17, "covered %d want %d", n, 12 * 17);
    bd_free(&d);
}
TEST(gradient_rect_swapped_vertices) {
    BD d; int n = 0, x, y; NEED(); REQUIRE(pGdiGradientFill);
    d = bd_new(32, 30, 30); bd_clear(&d, 0x010203);
    TRIVERTEX v[2] = { tv(17, 23, 10, 200, 10), tv(5, 6, 200, 10, 10) };
    GRADIENT_RECT gr = { 0, 1 };
    BOOL r = pGdiGradientFill(d.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H);
    t_note("GradientFill with LR as first vertex -> %d", r);
    for (y = 0; y < 30; y++) for (x = 0; x < 30; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != 0x010203) n++;
    t_note("covered %d pixels", n);
    bd_free(&d);
}
TEST(gradient_rect_clipped_by_region) {
    BD d; int x, y, bad = 0; NEED(); REQUIRE(pGdiGradientFill);
    d = bd_new(32, 20, 20); bd_clear(&d, 0);
    HRGN rg = pCreateRectRgn(4, 4, 9, 9); pSelectClipRgn(d.dc, rg);
    TRIVERTEX v[2] = { tv(0, 0, 255, 255, 255), tv(20, 20, 255, 255, 255) };
    GRADIENT_RECT gr = { 0, 1 };
    CHECK(pGdiGradientFill(d.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_V));
    for (y = 0; y < 20; y++) for (x = 0; x < 20; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != ((x >= 4 && x < 9 && y >= 4 && y < 9) ? 0xFFFFFF : 0)) bad++;
    CHECKF(!bad, "bad %d", bad);
    pSelectClipRgn(d.dc, NULL); pDeleteObject(rg); bd_free(&d);
}
TEST(gradient_rect_multiple_rects) {
    BD d; NEED(); REQUIRE(pGdiGradientFill);
    d = bd_new(32, 30, 10); bd_clear(&d, 0);
    TRIVERTEX v[4] = { tv(0, 0, 255, 0, 0), tv(10, 10, 255, 0, 0), tv(20, 0, 0, 0, 255), tv(30, 10, 0, 0, 255) };
    GRADIENT_RECT gr[2] = { { 0, 1 }, { 2, 3 } };
    CHECK(pGdiGradientFill(d.dc, v, 4, gr, 2, GRADIENT_FILL_RECT_H));
    CHECK_RECT_VAL(d, 0, 0, 10, 10, 0xFF0000, 0xFFFFFF);
    CHECK_RECT_VAL(d, 10, 0, 10, 10, 0x000000, 0xFFFFFF);
    CHECK_RECT_VAL(d, 20, 0, 10, 10, 0x0000FF, 0xFFFFFF);
    bd_free(&d);
}
TEST(gradient_rect_mm_viewport_offset) {
    BD d; NEED(); REQUIRE(pGdiGradientFill);
    d = bd_new(32, 30, 30); bd_clear(&d, 0);
    pSetViewportOrgEx(d.dc, 5, 7, NULL);
    TRIVERTEX v[2] = { tv(0, 0, 255, 255, 255), tv(6, 4, 255, 255, 255) };
    GRADIENT_RECT gr = { 0, 1 };
    BOOL r = pGdiGradientFill(d.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H);
    t_note("GradientFill with viewport origin -> %d", r);
    if (r) { CHECK_RECT_VAL(d, 5, 7, 6, 4, 0xFFFFFF, 0xFFFFFF); }
    pSetViewportOrgEx(d.dc, 0, 0, NULL);
    bd_free(&d);
}
TEST(gradient_triangle_vertices_and_centroid) {
    BD d; int x, y, n = 0; NEED(); REQUIRE(pGdiGradientFill);
    d = bd_new(32, 80, 80); bd_clear(&d, 0x00FF00 ^ 0x808080);
    TRIVERTEX v[3] = { tv(40, 5, 255, 0, 0), tv(5, 70, 0, 255, 0), tv(75, 70, 0, 0, 255) };
    GRADIENT_TRIANGLE gt = { 0, 1, 2 };
    BOOL r = pGdiGradientFill(d.dc, v, 3, &gt, 1, GRADIENT_FILL_TRIANGLE);
    CHECK(r);
    DWORD top = bd_get(&d, 40, 8) & 0xFFFFFF, left = bd_get(&d, 9, 68) & 0xFFFFFF, right = bd_get(&d, 71, 68) & 0xFFFFFF;
    CHECKF(ch(top, 16) > 200 && ch(top, 8) < 50 && ch(top, 0) < 50, "top %06lx", (unsigned long)top);
    CHECKF(ch(left, 8) > 200 && ch(left, 16) < 50 && ch(left, 0) < 50, "left %06lx", (unsigned long)left);
    CHECKF(ch(right, 0) > 200 && ch(right, 16) < 50 && ch(right, 8) < 50, "right %06lx", (unsigned long)right);
    DWORD mid = bd_get(&d, 40, 48) & 0xFFFFFF;
    CHECKF(abs(ch(mid, 16) - 85) <= 25 && abs(ch(mid, 8) - 85) <= 25 && abs(ch(mid, 0) - 85) <= 25, "centroid %06lx", (unsigned long)mid);
    CHECK_PX(d, 2, 2, 0x00FF00 ^ 0x808080, 0xFFFFFF); CHECK_PX(d, 77, 2, 0x00FF00 ^ 0x808080, 0xFFFFFF);
    for (y = 0; y < 80; y++) for (x = 0; x < 80; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != (0x00FF00 ^ 0x808080)) n++;
    double area = 70.0 * 65.0 / 2;
    CHECKF(n > area * 0.9 && n < area * 1.15, "painted %d, triangle area %.0f", n, area);
    bd_free(&d);
}
TEST(gradient_triangle_uniform_color_exact) {
    BD d; int x, y, bad = 0, n = 0; NEED(); REQUIRE(pGdiGradientFill);
    d = bd_new(32, 40, 40); bd_clear(&d, 0);
    TRIVERTEX v[3] = { tv(5, 5, 0x20, 0x40, 0x60), tv(35, 8, 0x20, 0x40, 0x60), tv(12, 35, 0x20, 0x40, 0x60) };
    GRADIENT_TRIANGLE gt = { 0, 1, 2 };
    CHECK(pGdiGradientFill(d.dc, v, 3, &gt, 1, GRADIENT_FILL_TRIANGLE));
    for (y = 0; y < 40; y++) for (x = 0; x < 40; x++) { DWORD c = bd_get(&d, x, y) & 0xFFFFFF; if (c) { n++; if (c != 0x204060) bad++; } }
    CHECKF(!bad && n > 200, "bad %d painted %d", bad, n);
    bd_free(&d);
}
TEST(gradient_triangle_winding_independent) {
    BD a, b; int x, y, diff = 0; NEED(); REQUIRE(pGdiGradientFill);
    a = bd_new(32, 40, 40); b = bd_new(32, 40, 40); bd_clear(&a, 0); bd_clear(&b, 0);
    TRIVERTEX v[3] = { tv(5, 5, 255, 0, 0), tv(35, 8, 0, 255, 0), tv(12, 35, 0, 0, 255) };
    GRADIENT_TRIANGLE g1 = { 0, 1, 2 }, g2 = { 0, 2, 1 };
    CHECK(pGdiGradientFill(a.dc, v, 3, &g1, 1, GRADIENT_FILL_TRIANGLE));
    CHECK(pGdiGradientFill(b.dc, v, 3, &g2, 1, GRADIENT_FILL_TRIANGLE));
    int na = 0, nb = 0;
    for (y = 0; y < 40; y++) for (x = 0; x < 40; x++) { if (bd_get(&a, x, y)) na++; if (bd_get(&b, x, y)) nb++; if (!bd_get(&a, x, y) != !bd_get(&b, x, y)) diff++; }
    t_note("triangle windings: covered %d vs %d, coverage differences %d", na, nb, diff);
    CHECK(na > 200 && nb > 200);
    bd_free(&a); bd_free(&b);
}
TEST(gradient_triangle_offscreen_clipped) {
    BD d; int n = 0, x, y; NEED(); REQUIRE(pGdiGradientFill);
    d = bd_new(32, 20, 20); bd_clear(&d, 0);
    TRIVERTEX v[3] = { tv(-30, -30, 255, 255, 255), tv(60, -30, 255, 255, 255), tv(10, 60, 255, 255, 255) };
    GRADIENT_TRIANGLE gt = { 0, 1, 2 };
    BOOL r = pGdiGradientFill(d.dc, v, 3, &gt, 1, GRADIENT_FILL_TRIANGLE);
    CHECK(r);
    for (y = 0; y < 20; y++) for (x = 0; x < 20; x++) if (bd_get(&d, x, y) & 0xFFFFFF) n++;
    CHECKF(n == 400 || n > 380, "n=%d", n);
    bd_free(&d);
}
TEST(gradient_invalid_mode) {
    BD d; NEED(); REQUIRE(pGdiGradientFill);
    d = bd_new(32, 10, 10); bd_clear(&d, 0x55);
    TRIVERTEX v[2] = { tv(0, 0, 255, 255, 255), tv(10, 10, 255, 255, 255) };
    GRADIENT_RECT gr = { 0, 1 };
    ULONG modes[] = { 3, 4, 100, 0xFFFFFFFF };
    int i;
    for (i = 0; i < 4; i++) {
        SetLastError(0xDEAD);
        BOOL r = pGdiGradientFill(d.dc, v, 2, &gr, 1, modes[i]);
        CHECKF(!r, "mode %lu accepted", modes[i]);
        t_note("GradientFill mode %lu err=%lu", modes[i], GetLastError());
    }
    CHECK_RECT_VAL(d, 0, 0, 10, 10, 0x55, 0xFFFFFF);
    bd_free(&d);
}
TEST(gradient_invalid_counts) {
    BD d; NEED(); REQUIRE(pGdiGradientFill);
    d = bd_new(32, 10, 10); bd_clear(&d, 0x55);
    TRIVERTEX v[2] = { tv(0, 0, 255, 255, 255), tv(10, 10, 255, 255, 255) };
    GRADIENT_RECT gr = { 0, 1 };
    SetLastError(0xDEAD);
    BOOL r = pGdiGradientFill(d.dc, v, 0, &gr, 1, GRADIENT_FILL_RECT_H);
    t_note("nVertex=0 -> %d err=%lu", r, GetLastError());
    r = pGdiGradientFill(d.dc, v, 2, &gr, 0, GRADIENT_FILL_RECT_H);
    t_note("nCount=0 -> %d err=%lu", r, GetLastError());
    r = pGdiGradientFill(d.dc, NULL, 2, &gr, 1, GRADIENT_FILL_RECT_H);
    t_note("NULL vertices -> %d err=%lu", r, GetLastError());
    CHECK(!r);
    r = pGdiGradientFill(d.dc, v, 2, NULL, 1, GRADIENT_FILL_RECT_H);
    t_note("NULL mesh -> %d err=%lu", r, GetLastError());
    CHECK(!r);
    CHECK_RECT_VAL(d, 0, 0, 10, 10, 0x55, 0xFFFFFF);
    bd_free(&d);
}
TEST(gradient_mesh_index_out_of_range) {
    BD d; NEED(); REQUIRE(pGdiGradientFill);
    d = bd_new(32, 10, 10); bd_clear(&d, 0x55);
    TRIVERTEX v[2] = { tv(0, 0, 255, 255, 255), tv(10, 10, 255, 255, 255) };
    GRADIENT_RECT gr = { 0, 2 };
    SetLastError(0xDEAD);
    BOOL r = pGdiGradientFill(d.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H);
    t_note("mesh index == nVertex -> %d err=%lu", r, GetLastError());
    CHECK(!r);
    GRADIENT_TRIANGLE gt = { 0, 1, 7 };
    r = pGdiGradientFill(d.dc, v, 2, &gt, 1, GRADIENT_FILL_TRIANGLE);
    CHECK(!r);
    CHECK_RECT_VAL(d, 0, 0, 10, 10, 0x55, 0xFFFFFF);
    bd_free(&d);
}
TEST(gradient_bad_dc_sets_invalid_parameter) {
    NEED(); REQUIRE(pGdiGradientFill);
    TRIVERTEX v[2] = { tv(0, 0, 255, 255, 255), tv(10, 10, 255, 255, 255) };
    GRADIENT_RECT gr = { 0, 1 };
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)0x1234, (HDC)(ULONG_PTR)-1 };
    int i;
    for (i = 0; i < 3; i++) {
        SetLastError(0xDEAD);
        BOOL r = pGdiGradientFill(bad[i], v, 2, &gr, 1, GRADIENT_FILL_RECT_H);
        DWORD e = GetLastError();
        CHECKF(!r, "case %d returned %d", i, r);
        CHECKF(e == ERROR_INVALID_PARAMETER, "case %d err=%lu", i, e);
    }
}
TEST(gradient_alpha_in_vertex_ignored) {
    BD d; NEED(); REQUIRE(pGdiGradientFill);
    d = bd_new(32, 10, 10); bd_clear(&d, 0);
    TRIVERTEX v[2] = { tv(0, 0, 255, 255, 255), tv(10, 10, 255, 255, 255) };
    v[0].Alpha = 0; v[1].Alpha = 0x8000;
    GRADIENT_RECT gr = { 0, 1 };
    CHECK(pGdiGradientFill(d.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H));
    CHECK_RECT_VAL(d, 0, 0, 10, 10, 0xFFFFFF, 0xFFFFFF);
    bd_free(&d);
}
TEST(gradient_16bit_color_precision) {
    BD d; NEED(); REQUIRE(pGdiGradientFill);
    d = bd_new(32, 10, 4); bd_clear(&d, 0);
    TRIVERTEX v[2] = { tv(0, 0, 0, 0, 0), tv(10, 4, 0, 0, 0) };
    v[0].Red = 0x8000; v[1].Red = 0x8000;   /* 128.0 */
    GRADIENT_RECT gr = { 0, 1 };
    CHECK(pGdiGradientFill(d.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H));
    int r = ch(bd_get(&d, 4, 1), 16);
    CHECKF(r == 0x80, "red %02x", r);
    v[0].Red = v[1].Red = 0xFFFF;
    CHECK(pGdiGradientFill(d.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H));
    r = ch(bd_get(&d, 4, 1), 16);
    CHECKF(r == 0xFF, "red for 0xFFFF %02x", r);
    bd_free(&d);
}
TEST(gradient_into_24bpp) {
    BD d; int x, prev = -1, bad = 0; NEED(); REQUIRE(pGdiGradientFill);
    d = bd_new(24, 40, 4); bd_clear(&d, 0);
    TRIVERTEX v[2] = { tv(0, 0, 0, 0, 0), tv(40, 4, 255, 255, 255) };
    GRADIENT_RECT gr = { 0, 1 };
    CHECK(pGdiGradientFill(d.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H));
    for (x = 0; x < 40; x++) { int c = bd_get(&d, x, 1) & 255; if (c < prev) bad++; prev = c; }
    CHECKF(!bad && prev >= 240, "bad %d last %d", bad, prev);
    bd_free(&d);
}
TEST(gradient_into_16bpp_565) {
    BD d; int x, prev = -1, bad = 0; NEED(); REQUIRE(pGdiGradientFill);
    d = bd_new16(40, 4, 1); bd_clear(&d, 0);
    TRIVERTEX v[2] = { tv(0, 0, 0, 0, 0), tv(40, 4, 255, 255, 255) };
    GRADIENT_RECT gr = { 0, 1 };
    CHECK(pGdiGradientFill(d.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H));
    for (x = 0; x < 40; x++) { int c = bd_get(&d, x, 1) & 31; if (c < prev) bad++; prev = c; }
    CHECKF(!bad && prev >= 28, "bad %d last %d", bad, prev);
    bd_free(&d);
}
TEST(gradient_into_8bpp_does_not_crash) {
    BD d; NEED(); REQUIRE(pGdiGradientFill);
    d = bd_new(8, 20, 4);
    TRIVERTEX v[2] = { tv(0, 0, 0, 0, 0), tv(20, 4, 255, 255, 255) };
    GRADIENT_RECT gr = { 0, 1 };
    BOOL r = pGdiGradientFill(d.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H);
    t_note("GradientFill into 8bpp -> %d", r);
    bd_free(&d);
}
TEST(gradient_degenerate_rects) {
    BD d; NEED(); REQUIRE(pGdiGradientFill);
    d = bd_new(32, 10, 10); bd_clear(&d, 0x55);
    TRIVERTEX v[2] = { tv(3, 3, 255, 255, 255), tv(3, 8, 255, 255, 255) };
    GRADIENT_RECT gr = { 0, 1 };
    BOOL r = pGdiGradientFill(d.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H);
    t_note("zero-width gradient rect -> %d", r);
    CHECK_RECT_VAL(d, 0, 0, 10, 10, 0x55, 0xFFFFFF);
    bd_free(&d);
}
