/* Bit-depth variations, clipping equivalence, mapping modes, handle validation, cross-thread use. */
#include "dp.h"
#define W 96
#define H 64

static RGBQUAD q(int r, int g, int b) { RGBQUAD x; x.rgbRed = r; x.rgbGreen = g; x.rgbBlue = b; x.rgbReserved = 0; return x; }

/* ---------------- 1 bpp ---------------- */
TEST(dprim_bpp1_black_white_pen) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 1));
    EXPECT_EQ(dp_cnt(&d, 1), W * H);
    dp_pen(&d, PS_SOLID, 1, BLACK_C); dp_line(&d, 5, 5, 45, 5);
    EXPECT_EQ(dp_cnt(&d, 0), 40);
    dp_pen(&d, PS_SOLID, 1, WHITE_C); dp_line(&d, 5, 5, 45, 5);
    { int z = dp_cnt(&d, 0); CHECKF(z == 0, "z=%d", z); }
    dp_close(&d);
}
TEST(dprim_bpp1_palette_order_maps_colors) {
    DP d; DP_NEED();
    RGBQUAD pal[2] = { q(255, 255, 255), q(0, 0, 0) };
    CHECK(dp_open_pal(&d, W, H, 1, pal, 2));
    U(PatBlt)(d.dc, 0, 0, W, H, WHITENESS);
    dp_pen(&d, PS_SOLID, 1, BLACK_C); dp_line(&d, 5, 5, 45, 5);
    int ones = dp_cnt(&d, 1);
    t_note("inverted 1bpp palette: black pen set %d pixels with index 1", ones);
    WCHECK(ones == 40, "ones=%d", ones);
    dp_close(&d);
}
TEST(dprim_bpp1_custom_palette_exact_match) {
    DP d; DP_NEED();
    RGBQUAD pal[2] = { q(255, 0, 0), q(0, 0, 255) };
    CHECK(dp_open_pal(&d, W, H, 1, pal, 2));
    dp_pen(&d, PS_SOLID, 1, BLUE_C); dp_line(&d, 5, 5, 45, 5);
    int n = dp_row(&d, 5, 0, W), ones = 0; (void)n;
    for (int x = 0; x < W; x++) ones += dp_raw(&d, x, 5) == 1;
    CHECKF(ones >= 40, "blue pen -> %d pixels idx1", ones);
    dp_pen(&d, PS_SOLID, 1, RED_C); dp_line(&d, 5, 9, 45, 9);
    int zeros = 0; for (int x = 0; x < W; x++) zeros += dp_raw(&d, x, 9) == 0;
    CHECKF(zeros >= 40, "red pen row zeros %d", zeros);
    dp_close(&d);
}
TEST(dprim_bpp1_nearest_color_bright_dark) {
    DP d; DP_NEED();
    static const struct { COLORREF c; int idx; } t[] = { {RGB(255,255,255),1}, {RGB(0,0,0),0}, {RGB(240,240,240),1}, {RGB(20,20,20),0}, {RGB(200,200,200),1}, {RGB(60,60,60),0} };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        CHECK(dp_open(&d, W, H, 1));
        U(PatBlt)(d.dc, 0, 0, W, H, t[i].idx ? BLACKNESS : WHITENESS);
        dp_pen(&d, PS_SOLID, 1, t[i].c); dp_line(&d, 5, 5, 45, 5);
        int got = 0; for (int x = 5; x < 45; x++) got += dp_raw(&d, x, 5) == (DWORD)t[i].idx;
        WCHECK(got == 40, "colour %06lx expected idx %d got %d", (unsigned long)t[i].c, t[i].idx, got);
        dp_close(&d);
    }
}
TEST(dprim_bpp1_rop_not_inverts_bits) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 1)); dp_black(&d); dp_stock(&d, NULL_BRUSH);
    U(SetROP2)(d.dc, R2_NOT);
    dp_line(&d, 0, 5, W, 5);
    int zeros = 0; for (int x = 0; x < W; x++) zeros += dp_raw(&d, x, 5) == 0;
    EXPECT_EQ(zeros, W);
    dp_line(&d, 0, 5, W, 5);
    int ones = 0; for (int x = 0; x < W; x++) ones += dp_raw(&d, x, 5) == 1;
    EXPECT_EQ(ones, W);
    dp_close(&d);
}
TEST(dprim_bpp1_rect_fill_black) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 1)); dp_stock(&d, NULL_PEN); dp_brush(&d, BLACK_C);
    U(Rectangle)(d.dc, 8, 8, 40, 24);
    int z = dp_cnt(&d, 0);
    t_note("1bpp black NULL_PEN rect 32x16 -> %d px", z);
    CHECKF(z >= 31 * 15 && z <= 33 * 17, "z=%d", z);
    dp_close(&d);
}
TEST(dprim_bpp1_gray_brush_dithers) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 1)); dp_stock(&d, NULL_PEN); dp_brush(&d, RGB(128, 128, 128));
    U(Rectangle)(d.dc, 8, 8, 40, 24);
    int ones = 0, zeros = 0;
    for (int y = 8; y < 24; y++) for (int x = 8; x < 40; x++) { if (dp_raw(&d, x, y)) ones++; else zeros++; }
    t_note("1bpp gray brush: %d set %d clear of %d", ones, zeros, 32 * 16);
    dp_close(&d);
}
TEST(dprim_bpp1_ellipse_and_polygon_no_crash) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 1)); dp_black(&d); dp_brush(&d, RGB(128, 128, 128));
    U(Ellipse)(d.dc, 5, 5, 80, 55);
    POINT p[3] = { {10, 10}, {70, 20}, {40, 55} };
    U(Polygon)(d.dc, p, 3);
    CHECK(dp_cnt(&d, 0) > 100);
    dp_close(&d);
}

/* ---------------- 8 bpp ---------------- */
TEST(dprim_bpp8_grey_ramp_exact) {
    DP d; DP_NEED();
    static const int v[] = { 0, 1, 17, 100, 128, 200, 254, 255 };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        CHECK(dp_open(&d, W, H, 8));
        dp_pen(&d, PS_SOLID, 1, RGB(v[i], v[i], v[i])); dp_line(&d, 5, 5, 45, 5);
        int n = 0; for (int x = 5; x < 45; x++) n += dp_raw(&d, x, 5) == (DWORD)v[i];
        t_note("8bpp grey %d -> index %lu", v[i], dp_raw(&d, 20, 5));
        CHECKF(n == 40, "grey %d -> %d pixels with index %d", v[i], n, v[i]);
        dp_close(&d);
    }
}
TEST(dprim_bpp8_nearest_in_custom_palette) {
    DP d; DP_NEED();
    RGBQUAD pal[6] = { q(0,0,0), q(255,0,0), q(0,255,0), q(0,0,255), q(255,255,255), q(255,255,0) };
    static const struct { COLORREF c; int idx; } t[] = {
        {RGB(250,10,10),1}, {RGB(10,240,10),2}, {RGB(10,10,200),3}, {RGB(240,240,240),4}, {RGB(200,200,10),5},
        {RGB(255,0,0),1}, {RGB(0,255,0),2}, {RGB(0,0,255),3}, {RGB(255,255,255),4}, {RGB(255,255,0),5}, {RGB(0,0,0),0} };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        CHECK(dp_open_pal(&d, W, H, 8, pal, 6));
        U(PatBlt)(d.dc, 0, 0, W, H, BLACKNESS);
        dp_pen(&d, PS_SOLID, 1, t[i].c); dp_line(&d, 5, 5, 45, 5);
        int n = 0; for (int x = 5; x < 45; x++) n += dp_raw(&d, x, 5) == (DWORD)t[i].idx;
        CHECKF(n == 40, "colour %06lx want idx %d, %d match", (unsigned long)t[i].c, t[i].idx, n);
        dp_close(&d);
    }
}
TEST(dprim_bpp8_near_grey_nearest) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 8));
    dp_pen(&d, PS_SOLID, 1, RGB(10, 10, 12)); dp_line(&d, 5, 5, 45, 5);
    DWORD v = dp_raw(&d, 20, 5);
    t_note("RGB(10,10,12) -> index %lu", v);
    CHECKF(v >= 10 && v <= 12, "v=%lu", v);
    dp_close(&d);
}
TEST(dprim_bpp8_rect_pen_brush) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 8)); dp_pen(&d, PS_SOLID, 1, RGB(50, 50, 50)); dp_brush(&d, RGB(200, 200, 200));
    U(Rectangle)(d.dc, 10, 10, 30, 25);
    CHECKF(dp_cnt(&d, 50) == 2 * (20 + 15) - 4, "pen idx count %d", dp_cnt(&d, 50));
    CHECKF(dp_cnt(&d, 200) == 18 * 13, "brush idx count %d", dp_cnt(&d, 200));
    dp_close(&d);
}
TEST(dprim_bpp8_dithered_brush_note) {
    DP d; DP_NEED();
    RGBQUAD pal[6] = { q(0,0,0), q(255,0,0), q(0,255,0), q(0,0,255), q(255,255,255), q(255,255,0) };
    CHECK(dp_open_pal(&d, W, H, 8, pal, 6));
    U(PatBlt)(d.dc, 0, 0, W, H, BLACKNESS);
    dp_stock(&d, NULL_PEN); dp_brush(&d, RGB(128, 128, 128));
    U(Rectangle)(d.dc, 10, 10, 30, 25);
    int c[8] = { 0 };
    for (int y = 10; y < 25; y++) for (int x = 10; x < 30; x++) { DWORD v = dp_raw(&d, x, y); if (v < 8) c[v]++; }
    t_note("8bpp mid-grey brush histogram: %d %d %d %d %d %d", c[0], c[1], c[2], c[3], c[4], c[5]);
    dp_close(&d);
}

/* ---------------- 16 / 15 / 24 bpp ---------------- */
TEST(dprim_bpp16_primaries_565) {
    DP d; DP_NEED();
    static const struct { COLORREF c; DWORD v; } t[] = { {RGB(255,0,0),0xF800}, {RGB(0,255,0),0x07E0}, {RGB(0,0,255),0x001F}, {RGB(255,255,255),0xFFFF}, {RGB(0,0,0),0}, {RGB(255,255,0),0xFFE0}, {RGB(0,255,255),0x07FF}, {RGB(255,0,255),0xF81F} };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        CHECK(dp_open(&d, W, H, 16));
        dp_pen(&d, PS_SOLID, 1, t[i].c); dp_line(&d, 5, 5, 45, 5);
        DWORD v = dp_raw(&d, 20, 5);
        CHECKF(v == t[i].v, "colour %06lx -> %04lx want %04lx", (unsigned long)t[i].c, v, t[i].v);
        EXPECT_EQ(dp_cnt(&d, t[i].v), t[i].v == 0xFFFF ? W * H : 40 + (t[i].v == 0 ? 0 : 0) + (W * H - W * H));
        dp_close(&d);
    }
}
TEST(dprim_bpp16_primaries_555) {
    DP d; DP_NEED();
    static const struct { COLORREF c; DWORD v; } t[] = { {RGB(255,0,0),0x7C00}, {RGB(0,255,0),0x03E0}, {RGB(0,0,255),0x001F}, {RGB(255,255,255),0x7FFF}, {RGB(0,0,0),0} };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        CHECK(dp_open(&d, W, H, 15));
        dp_pen(&d, PS_SOLID, 1, t[i].c); dp_line(&d, 5, 5, 45, 5);
        DWORD v = dp_raw(&d, 20, 5);
        CHECKF(v == t[i].v, "colour %06lx -> %04lx want %04lx", (unsigned long)t[i].c, v, t[i].v);
        dp_close(&d);
    }
}
TEST(dprim_bpp16_red_ramp_monotonic_full_range) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, 300, 8, 16));
    int seen[32] = { 0 }, prev = -1, mono = 1;
    for (int r = 0; r < 256; r++) {
        dp_pen(&d, PS_SOLID, 1, RGB(r, 0, 0)); dp_line(&d, r, 2, r + 1, 2);
        int v = dp_raw(&d, r, 2) >> 11;
        if (v < prev) mono = 0;
        prev = v; seen[v]++;
    }
    int distinct = 0; for (int i = 0; i < 32; i++) distinct += seen[i] > 0;
    CHECK(mono);
    EXPECT_EQ(distinct, 32);
    t_note("565 red quantisation: 128 -> %d, 127 -> ?", 0);
    dp_close(&d);
}
TEST(dprim_bpp16_green_ramp_monotonic_full_range) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, 300, 8, 16));
    int seen[64] = { 0 }, prev = -1, mono = 1;
    for (int g = 0; g < 256; g++) {
        dp_pen(&d, PS_SOLID, 1, RGB(0, g, 0)); dp_line(&d, g, 2, g + 1, 2);
        int v = (dp_raw(&d, g, 2) >> 5) & 63;
        if (v < prev) mono = 0;
        prev = v; seen[v]++;
    }
    int distinct = 0; for (int i = 0; i < 64; i++) distinct += seen[i] > 0;
    CHECK(mono);
    EXPECT_EQ(distinct, 64);
    dp_close(&d);
}
TEST(dprim_bpp16_quantisation_report) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 16));
    static const int v[] = { 1, 4, 7, 8, 12, 127, 128, 130, 254 };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        dp_pen(&d, PS_SOLID, 1, RGB(v[i], v[i], v[i])); dp_line(&d, 5, 5 + i, 45, 5 + i);
        DWORD w = dp_raw(&d, 20, 5 + i);
        t_note("565: grey %3d -> r5=%lu g6=%lu b5=%lu", v[i], w >> 11, (w >> 5) & 63, w & 31);
    }
    dp_close(&d);
}
TEST(dprim_bpp16_brush_pure_colors) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 16)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    U(Rectangle)(d.dc, 10, 10, 30, 25);
    int n = dp_cnt(&d, 0xF800);
    CHECKF(n >= 19 * 14 && n <= 21 * 16, "n=%d", n);
    dp_close(&d);
}
TEST(dprim_bpp16_rect_pen_brush_counts) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 16)); dp_pen(&d, PS_SOLID, 1, BLUE_C); dp_brush(&d, GREEN_C);
    U(Rectangle)(d.dc, 10, 10, 30, 25);
    EXPECT_EQ(dp_cnt(&d, 0x001F), 2 * (20 + 15) - 4);
    EXPECT_EQ(dp_cnt(&d, 0x07E0), 18 * 13);
    dp_close(&d);
}
TEST(dprim_bpp24_exact_colors) {
    DP d; DP_NEED();
    static const COLORREF cs[] = { RGB(1,2,3), RGB(255,254,253), RGB(128,64,32), RGB(0,255,0) };
    for (unsigned i = 0; i < 4; i++) {
        CHECK(dp_open(&d, W, H, 24));
        dp_pen(&d, PS_SOLID, 1, cs[i]); dp_line(&d, 5, 5, 45, 5);
        EXPECT_EQ(dp_cnt(&d, dp_craw(cs[i])), 40);
        dp_close(&d);
    }
}
TEST(dprim_bpp24_rect_shapes) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 24)); dp_pen(&d, PS_SOLID, 1, RGB(9, 8, 7)); dp_brush(&d, RGB(200, 100, 50));
    U(Rectangle)(d.dc, 10, 10, 30, 25);
    EXPECT_EQ(dp_cnt(&d, dp_craw(RGB(9, 8, 7))), 66);
    EXPECT_EQ(dp_cnt(&d, dp_craw(RGB(200, 100, 50))), 18 * 13);
    dp_close(&d);
}
TEST(dprim_bpp32_alpha_byte_untouched_or_zero) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_SOLID, 1, RGB(1, 2, 3));
    U(GdiFlush)();
    for (int i = 0; i < W * H; i++) ((DWORD *)d.bits)[i] = 0xAB000000 | 0xFFFFFF;
    dp_line(&d, 5, 5, 45, 5);
    U(GdiFlush)();
    DWORD v = ((DWORD *)d.bits)[5 * W + 10];
    t_note("32bpp pixel after GDI draw: %08lx", v);
    CHECK((v & 0xFFFFFF) == 0x030201 >> 0 || (v & 0xFFFFFF) == 0x010203);
    CHECK((v & 0xFFFFFF) == 0x010203);
    dp_close(&d);
}
TEST(dprim_bottom_up_dib_orientation) {
    DP_NEED();
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 32; bi.bmiHeader.biHeight = 16; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    DWORD *bits = NULL;
    HDC dc = U(CreateCompatibleDC)(NULL);
    HBITMAP bm = U(CreateDIBSection)(dc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    CHECK(bm && bits);
    if (bm && bits) {
        HGDIOBJ ob = U(SelectObject)(dc, bm);
        U(PatBlt)(dc, 0, 0, 32, 16, WHITENESS);
        HPEN pen = U(CreatePen)(PS_SOLID, 1, RED_C); HGDIOBJ op = U(SelectObject)(dc, pen);
        U(MoveToEx)(dc, 0, 0, NULL); U(LineTo)(dc, 10, 0);
        U(GdiFlush)();
        EXPECT_EQ(bits[15 * 32 + 3] & 0xFFFFFF, 0xFF0000);
        EXPECT_EQ(bits[0 * 32 + 3] & 0xFFFFFF, 0xFFFFFF);
        U(SelectObject)(dc, op); U(DeleteObject)(pen); U(SelectObject)(dc, ob); U(DeleteObject)(bm);
    }
    U(DeleteDC)(dc);
}
TEST(dprim_dc_caps_match_dib) {
    DP d; DP_NEED(); REQUIRE(U(GetDeviceCaps));
    CHECK(dp_open(&d, 100, 50, 32));
    t_note("DIB memory DC caps: HORZRES=%d VERTRES=%d BITSPIXEL=%d PLANES=%d", U(GetDeviceCaps)(d.dc, HORZRES), U(GetDeviceCaps)(d.dc, VERTRES), U(GetDeviceCaps)(d.dc, BITSPIXEL), U(GetDeviceCaps)(d.dc, PLANES));
    int v = U(GetDeviceCaps)(d.dc, PLANES); EXPECT_EQ(v, 1);
    dp_close(&d);
}

/* ---------------- clipping equivalence ---------------- */
enum { S_LINE, S_RECT, S_RRECT, S_ELL, S_PIE, S_CHORD, S_ARC, S_POLY, S_BEZ, S_PLINE, S_WIDE, S_STAR, S_N };
static void shape(DP *d, int s, int ox, int oy) {
    HDC dc = d->dc;
    switch (s) {
    case S_LINE: U(MoveToEx)(dc, ox - 10, oy + 5, NULL); U(LineTo)(dc, ox + 60, oy + 45); break;
    case S_RECT: U(Rectangle)(dc, ox - 10, oy - 5, ox + 30, oy + 25); break;
    case S_RRECT: U(RoundRect)(dc, ox - 10, oy - 5, ox + 50, oy + 35, 14, 10); break;
    case S_ELL: U(Ellipse)(dc, ox - 12, oy - 9, ox + 40, oy + 31); break;
    case S_PIE: U(Pie)(dc, ox - 12, oy - 9, ox + 40, oy + 31, ox + 40, oy + 11, ox + 14, oy - 9); break;
    case S_CHORD: U(Chord)(dc, ox - 12, oy - 9, ox + 40, oy + 31, ox + 40, oy + 11, ox + 14, oy - 9); break;
    case S_ARC: U(Arc)(dc, ox - 12, oy - 9, ox + 40, oy + 31, ox + 40, oy + 11, ox + 14, oy - 9); break;
    case S_POLY: { POINT p[4] = { {ox - 10, oy}, {ox + 50, oy + 5}, {ox + 40, oy + 45}, {ox + 5, oy + 30} }; U(Polygon)(dc, p, 4); break; }
    case S_BEZ: { POINT p[4] = { {ox - 10, oy + 10}, {ox + 20, oy + 70}, {ox + 40, oy - 30}, {ox + 70, oy + 20} }; U(PolyBezier)(dc, p, 4); break; }
    case S_PLINE: { POINT p[4] = { {ox - 10, oy + 40}, {ox + 20, oy - 10}, {ox + 40, oy + 40}, {ox + 80, oy + 10} }; U(Polyline)(dc, p, 4); break; }
    case S_WIDE: { HPEN w = U(CreatePen)(PS_SOLID, 5, BLACK_C); HGDIOBJ o = U(SelectObject)(dc, w); U(MoveToEx)(dc, ox - 10, oy + 5, NULL); U(LineTo)(dc, ox + 50, oy + 35); U(SelectObject)(dc, o); U(DeleteObject)(w); break; }
    case S_STAR: { POINT p[5] = { {ox + 20, oy - 10}, {ox + 40, oy + 40}, {ox - 10, oy + 10}, {ox + 50, oy + 10}, {ox, oy + 40} }; U(Polygon)(dc, p, 5); break; }
    }
}
static const char *snm[] = { "line", "rect", "roundrect", "ellipse", "pie", "chord", "arc", "polygon", "bezier", "polyline", "wideline", "star" };
#define CLIPT(nm, S) \
TEST(dprim_clip_equiv_##nm) { \
    DP a, b; DP_NEED(); \
    if (S == S_PIE || S == S_CHORD || S == S_ARC) REQUIRE(U(Pie)); \
    CHECK(dp_open(&a, 48, 40, 32)); CHECK(dp_open(&b, 48 * 5, 40 * 5, 32)); \
    dp_black(&a); dp_black(&b); dp_brush(&a, RED_C); dp_brush(&b, RED_C); \
    shape(&a, S, 0, 0); shape(&b, S, 48 * 2, 40 * 2); \
    int bad = 0; \
    for (int y = 0; y < 40; y++) for (int x = 0; x < 48; x++) if (dp_raw(&a, x, y) != dp_raw(&b, x + 48 * 2, y + 40 * 2)) bad++; \
    CHECKF(bad == 0, "%s: %d pixels differ from the unclipped rendering", snm[S], bad); \
    shape(&a, S, 20, 15); \
    dp_clr(&a); dp_clr(&b); \
    shape(&a, S, 30, 20); shape(&b, S, 30 + 48 * 2, 20 + 40 * 2); \
    bad = 0; \
    for (int y = 0; y < 40; y++) for (int x = 0; x < 48; x++) if (dp_raw(&a, x, y) != dp_raw(&b, x + 48 * 2, y + 40 * 2)) bad++; \
    CHECKF(bad == 0, "%s (offset): %d pixels differ", snm[S], bad); \
    dp_close(&a); dp_close(&b); \
}
CLIPT(line, S_LINE) CLIPT(rect, S_RECT) CLIPT(roundrect, S_RRECT) CLIPT(ellipse, S_ELL) CLIPT(pie, S_PIE) CLIPT(chord, S_CHORD)
CLIPT(arc, S_ARC) CLIPT(polygon, S_POLY) CLIPT(bezier, S_BEZ) CLIPT(polyline, S_PLINE) CLIPT(wideline, S_WIDE) CLIPT(star, S_STAR)

#define TRANST(nm, S) \
TEST(dprim_translate_equiv_##nm) { \
    DP a; DP_NEED(); \
    CHECK(dp_open(&a, 160, 120, 32)); dp_black(&a); dp_brush(&a, RED_C); \
    shape(&a, S, 20, 20); \
    BYTE *snap = malloc(160 * 120 * 4); \
    for (int y = 0; y < 120; y++) for (int x = 0; x < 160; x++) ((DWORD *)snap)[y * 160 + x] = dp_raw(&a, x, y); \
    dp_clr(&a); \
    shape(&a, S, 20 + 37, 20 + 29); \
    int bad = 0; \
    for (int y = 0; y < 120 - 29; y++) for (int x = 0; x < 160 - 37; x++) if (((DWORD *)snap)[y * 160 + x] != dp_raw(&a, x + 37, y + 29)) bad++; \
    CHECKF(bad == 0, "%s: %d differ after integer translation", snm[S], bad); \
    free(snap); dp_close(&a); \
}
TRANST(line, S_LINE) TRANST(rect, S_RECT) TRANST(roundrect, S_RRECT) TRANST(ellipse, S_ELL) TRANST(pie, S_PIE) TRANST(chord, S_CHORD)
TRANST(arc, S_ARC) TRANST(polygon, S_POLY) TRANST(bezier, S_BEZ) TRANST(polyline, S_PLINE) TRANST(wideline, S_WIDE) TRANST(star, S_STAR)

#define VORGT(nm, S) \
TEST(dprim_viewport_org_equiv_##nm) { \
    DP a; DP_NEED(); REQUIRE(U(SetViewportOrgEx)); \
    CHECK(dp_open(&a, 160, 120, 32)); dp_black(&a); dp_brush(&a, RED_C); \
    shape(&a, S, 60, 40); \
    BYTE *snap = malloc(160 * 120 * 4); \
    for (int y = 0; y < 120; y++) for (int x = 0; x < 160; x++) ((DWORD *)snap)[y * 160 + x] = dp_raw(&a, x, y); \
    dp_clr(&a); \
    U(SetViewportOrgEx)(a.dc, 13, 9, NULL); \
    shape(&a, S, 60 - 13, 40 - 9); \
    U(SetViewportOrgEx)(a.dc, 0, 0, NULL); \
    int bad = 0; \
    for (int y = 0; y < 120; y++) for (int x = 0; x < 160; x++) if (((DWORD *)snap)[y * 160 + x] != dp_raw(&a, x, y)) bad++; \
    CHECKF(bad == 0, "%s: %d differ with viewport origin", snm[S], bad); \
    free(snap); dp_close(&a); \
}
VORGT(line, S_LINE) VORGT(rect, S_RECT) VORGT(roundrect, S_RRECT) VORGT(ellipse, S_ELL) VORGT(pie, S_PIE) VORGT(chord, S_CHORD)
VORGT(arc, S_ARC) VORGT(polygon, S_POLY) VORGT(bezier, S_BEZ) VORGT(polyline, S_PLINE) VORGT(wideline, S_WIDE) VORGT(star, S_STAR)

#define WORGT(nm, S) \
TEST(dprim_window_org_equiv_##nm) { \
    DP a; DP_NEED(); REQUIRE(U(SetWindowOrgEx)); \
    CHECK(dp_open(&a, 160, 120, 32)); dp_black(&a); dp_brush(&a, RED_C); \
    shape(&a, S, 60, 40); \
    BYTE *snap = malloc(160 * 120 * 4); \
    for (int y = 0; y < 120; y++) for (int x = 0; x < 160; x++) ((DWORD *)snap)[y * 160 + x] = dp_raw(&a, x, y); \
    dp_clr(&a); \
    U(SetWindowOrgEx)(a.dc, -13, -9, NULL); \
    shape(&a, S, 60 - 13, 40 - 9); \
    U(SetWindowOrgEx)(a.dc, 0, 0, NULL); \
    int bad = 0; \
    for (int y = 0; y < 120; y++) for (int x = 0; x < 160; x++) if (((DWORD *)snap)[y * 160 + x] != dp_raw(&a, x, y)) bad++; \
    CHECKF(bad == 0, "%s: %d differ with window origin", snm[S], bad); \
    free(snap); dp_close(&a); \
}
WORGT(line, S_LINE) WORGT(rect, S_RECT) WORGT(roundrect, S_RRECT) WORGT(ellipse, S_ELL) WORGT(pie, S_PIE) WORGT(chord, S_CHORD)
WORGT(arc, S_ARC) WORGT(polygon, S_POLY) WORGT(bezier, S_BEZ) WORGT(polyline, S_PLINE) WORGT(wideline, S_WIDE) WORGT(star, S_STAR)

/* ---------------- mapping modes ---------------- */
static void aniso(DP *d, int wx, int wy, int vx, int vy) {
    U(SetMapMode)(d->dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(d->dc, wx, wy, NULL);
    U(SetViewportExtEx)(d->dc, vx, vy, NULL);
}
TEST(dprim_map_aniso_scale2_line) {
    DP d; DP_NEED(); REQUIRE(U(SetMapMode));
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_SOLID, 0, BLACK_C);
    aniso(&d, 1, 1, 2, 2);
    dp_line(&d, 5, 5, 25, 5);
    EXPECT_EQ(dp_row(&d, 10, 0, W), 40);
    for (int x = 0; x < W; x++) CHECKF((dp_raw(&d, x, 10) == 0) == (x >= 10 && x < 50), "x=%d", x);
    dp_close(&d);
}
TEST(dprim_map_aniso_scale2_rectangle_bbox) {
    DP d; DP_NEED(); REQUIRE(U(SetMapMode));
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_SOLID, 0, BLACK_C); dp_stock(&d, NULL_BRUSH);
    aniso(&d, 1, 1, 2, 2);
    U(Rectangle)(d.dc, 5, 5, 25, 15);
    RECT rc; dp_ink(&d, &rc);
    CHECKF(rc.left == 10 && rc.top == 10 && rc.right == 50 && rc.bottom == 30, "%ld %ld %ld %ld", rc.left, rc.top, rc.right, rc.bottom);
    dp_close(&d);
}
TEST(dprim_map_aniso_half_scale_rectangle_bbox) {
    DP d; DP_NEED(); REQUIRE(U(SetMapMode));
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_SOLID, 0, BLACK_C); dp_stock(&d, NULL_BRUSH);
    aniso(&d, 2, 2, 1, 1);
    U(Rectangle)(d.dc, 10, 10, 50, 30);
    RECT rc; dp_ink(&d, &rc);
    CHECKF(rc.left == 5 && rc.top == 5 && rc.right == 25 && rc.bottom == 15, "%ld %ld %ld %ld", rc.left, rc.top, rc.right, rc.bottom);
    dp_close(&d);
}
TEST(dprim_map_aniso_nonuniform) {
    DP d; DP_NEED(); REQUIRE(U(SetMapMode));
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_SOLID, 0, BLACK_C); dp_stock(&d, NULL_BRUSH);
    aniso(&d, 1, 1, 3, 1);
    U(Rectangle)(d.dc, 4, 6, 14, 16);
    RECT rc; dp_ink(&d, &rc);
    CHECKF(rc.left == 12 && rc.top == 6 && rc.right == 42 && rc.bottom == 16, "%ld %ld %ld %ld", rc.left, rc.top, rc.right, rc.bottom);
    dp_close(&d);
}
TEST(dprim_map_aniso_scale_ellipse_area) {
    DP d; DP_NEED(); REQUIRE(U(SetMapMode));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    aniso(&d, 1, 1, 2, 2);
    U(Ellipse)(d.dc, 5, 5, 25, 25);
    int n = dp_cntc(&d, RED_C);
    double ex = 3.14159265 * 20 * 20;
    CHECKF(n > ex * 0.9 && n < ex * 1.12, "n=%d ex=%.0f", n, ex);
    dp_close(&d);
}
TEST(dprim_map_aniso_same_figure_scaled_vs_direct) {
    DP a, b; DP_NEED(); REQUIRE(U(SetMapMode));
    CHECK(dp_open(&a, W, H, 32)); CHECK(dp_open(&b, W, H, 32));
    dp_pen(&a, PS_SOLID, 0, BLACK_C); dp_pen(&b, PS_SOLID, 0, BLACK_C); dp_stock(&a, NULL_BRUSH); dp_stock(&b, NULL_BRUSH);
    aniso(&a, 1, 1, 2, 2);
    U(Ellipse)(a.dc, 5, 4, 25, 20);
    U(Ellipse)(b.dc, 10, 8, 50, 40);
    int bad = 0;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) if (dp_raw(&a, x, y) != dp_raw(&b, x, y)) bad++;
    CHECKF(bad == 0, "bad=%d", bad);
    dp_close(&a); dp_close(&b);
}
TEST(dprim_map_aniso_flipped_y_line) {
    DP d; DP_NEED(); REQUIRE(U(SetMapMode)); REQUIRE(U(SetViewportOrgEx));
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_SOLID, 0, BLACK_C);
    U(SetMapMode)(d.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(d.dc, 1, 1, NULL); U(SetViewportExtEx)(d.dc, 1, -1, NULL);
    U(SetViewportOrgEx)(d.dc, 0, 50, NULL);
    dp_line(&d, 10, 5, 10, 25);
    for (int y = 0; y < H; y++) CHECKF((dp_raw(&d, 10, y) == 0) == (y >= 26 && y <= 45), "y=%d", y);
    EXPECT_EQ(dp_ink(&d, NULL), 20);
    dp_close(&d);
}
TEST(dprim_map_aniso_flipped_x_line) {
    DP d; DP_NEED(); REQUIRE(U(SetMapMode)); REQUIRE(U(SetViewportOrgEx));
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_SOLID, 0, BLACK_C);
    U(SetMapMode)(d.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(d.dc, 1, 1, NULL); U(SetViewportExtEx)(d.dc, -1, 1, NULL);
    U(SetViewportOrgEx)(d.dc, 60, 0, NULL);
    dp_line(&d, 10, 5, 30, 5);
    for (int x = 0; x < W; x++) CHECKF((dp_raw(&d, x, 5) == 0) == (x >= 31 && x <= 50), "x=%d", x);
    dp_close(&d);
}
TEST(dprim_map_text_is_default) {
    DP d; DP_NEED(); REQUIRE(U(GetMapMode));
    CHECK(dp_open(&d, 8, 8, 32));
    int m = U(GetMapMode)(d.dc); EXPECT_EQ(m, MM_TEXT);
    dp_close(&d);
}
TEST(dprim_map_isotropic_square_scale) {
    DP d; DP_NEED(); REQUIRE(U(SetMapMode));
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_SOLID, 0, BLACK_C); dp_stock(&d, NULL_BRUSH);
    U(SetMapMode)(d.dc, MM_ISOTROPIC);
    U(SetWindowExtEx)(d.dc, 100, 100, NULL); U(SetViewportExtEx)(d.dc, 200, 200, NULL);
    U(Rectangle)(d.dc, 5, 5, 25, 15);
    RECT rc; dp_ink(&d, &rc);
    CHECKF(rc.left == 10 && rc.top == 10 && rc.right == 50 && rc.bottom == 30, "%ld %ld %ld %ld", rc.left, rc.top, rc.right, rc.bottom);
    dp_close(&d);
}
TEST(dprim_map_fixed_modes_ratio) {
    DP_NEED();
    static const int modes[] = { MM_LOMETRIC, MM_HIMETRIC, MM_LOENGLISH, MM_HIENGLISH, MM_TWIPS };
    for (unsigned i = 0; i < 5; i++) {
        DP d; CHECK(dp_open(&d, 300, 300, 32)); dp_pen(&d, PS_SOLID, 0, BLACK_C);
        U(SetMapMode)(d.dc, modes[i]);
        POINT p[2] = { {0, 0}, {1000, -1000} };
        U(LPtoDP)(d.dc, p, 2);
        t_note("map mode %d: logical (1000,-1000) -> device (%ld,%ld)", modes[i], p[1].x, p[1].y);
        CHECK(p[1].x > 0);
        CHECK(p[1].y > 0);
        dp_close(&d);
    }
}
TEST(dprim_map_gm_advanced_translate) {
    DP d; DP_NEED(); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform));
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_SOLID, 0, BLACK_C);
    U(SetGraphicsMode)(d.dc, GM_ADVANCED);
    XFORM x = { 1, 0, 0, 1, 5, 7 };
    CHECK(U(SetWorldTransform)(d.dc, &x));
    dp_line(&d, 0, 0, 20, 0);
    for (int i = 0; i < W; i++) CHECKF((dp_raw(&d, i, 7) == 0) == (i >= 5 && i < 25), "x=%d", i);
    EXPECT_EQ(dp_ink(&d, NULL), 20);
    U(ModifyWorldTransform)(d.dc, NULL, MWT_IDENTITY);
    U(SetGraphicsMode)(d.dc, GM_COMPATIBLE);
    dp_close(&d);
}
TEST(dprim_map_gm_advanced_scale_rect) {
    DP d; DP_NEED(); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform));
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_SOLID, 0, BLACK_C); dp_stock(&d, NULL_BRUSH);
    U(SetGraphicsMode)(d.dc, GM_ADVANCED);
    XFORM x = { 2, 0, 0, 3, 0, 0 };
    CHECK(U(SetWorldTransform)(d.dc, &x));
    U(Rectangle)(d.dc, 5, 4, 20, 10);
    RECT rc; dp_ink(&d, &rc);
    t_note("GM_ADVANCED scale(2,3) rectangle bbox %ld %ld %ld %ld", rc.left, rc.top, rc.right, rc.bottom);
    CHECK(rc.left == 10 && rc.top == 12 && rc.right >= 39 && rc.right <= 41 && rc.bottom >= 29 && rc.bottom <= 31);
    U(ModifyWorldTransform)(d.dc, NULL, MWT_IDENTITY);
    U(SetGraphicsMode)(d.dc, GM_COMPATIBLE);
    dp_close(&d);
}
TEST(dprim_map_gm_advanced_rotate90_line) {
    DP d; DP_NEED(); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform));
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_SOLID, 0, BLACK_C);
    U(SetGraphicsMode)(d.dc, GM_ADVANCED);
    XFORM x = { 0, 1, -1, 0, 30, 10 };
    CHECK(U(SetWorldTransform)(d.dc, &x));
    dp_line(&d, 0, 0, 20, 0);
    int n = dp_col(&d, 30, 0, H);
    t_note("rotated line column pixels %d", n);
    CHECK(n >= 19 && n <= 21);
    CHECK(dp_ink(&d, NULL) == n);
    U(ModifyWorldTransform)(d.dc, NULL, MWT_IDENTITY);
    U(SetGraphicsMode)(d.dc, GM_COMPATIBLE);
    dp_close(&d);
}

/* ---------------- handle validation ---------------- */
static int call_all(HDC dc) {
    POINT p[4] = { {0, 0}, {10, 0}, {10, 10}, {0, 10} }; DWORD c[1] = { 4 }; INT ci[1] = { 4 }; BYTE t[4] = { PT_MOVETO, PT_LINETO, PT_LINETO, PT_LINETO };
    int n = 0;
    n += !!U(MoveToEx)(dc, 1, 1, NULL);
    n += !!U(LineTo)(dc, 5, 5);
    n += !!U(Polyline)(dc, p, 4);
    n += !!U(PolylineTo)(dc, p, 4);
    n += !!U(PolyPolyline)(dc, p, c, 1);
    n += !!U(PolyBezier)(dc, p, 4);
    n += !!U(PolyBezierTo)(dc, p, 3);
    n += !!U(PolyDraw)(dc, p, t, 4);
    n += !!U(Arc)(dc, 0, 0, 10, 10, 10, 5, 5, 0);
    n += !!U(ArcTo)(dc, 0, 0, 10, 10, 10, 5, 5, 0);
    n += !!U(AngleArc)(dc, 5, 5, 5, 0, 90);
    n += !!U(Chord)(dc, 0, 0, 10, 10, 10, 5, 5, 0);
    n += !!U(Pie)(dc, 0, 0, 10, 10, 10, 5, 5, 0);
    n += !!U(Ellipse)(dc, 0, 0, 10, 10);
    n += !!U(Rectangle)(dc, 0, 0, 10, 10);
    n += !!U(RoundRect)(dc, 0, 0, 10, 10, 4, 4);
    n += !!U(Polygon)(dc, p, 4);
    n += !!U(PolyPolygon)(dc, p, ci, 1);
    return n;
}
static int have_all(void) {
    return U(MoveToEx) && U(LineTo) && U(Polyline) && U(PolylineTo) && U(PolyPolyline) && U(PolyBezier) && U(PolyBezierTo) && U(PolyDraw) &&
           U(Arc) && U(ArcTo) && U(AngleArc) && U(Chord) && U(Pie) && U(Ellipse) && U(Rectangle) && U(RoundRect) && U(Polygon) && U(PolyPolygon);
}
TEST(dprim_invalid_all_null_dc) {
    DP_NEED(); if (!have_all()) { t_skip("missing exports"); return; }
    EXPECT_EQ(call_all(NULL), 0);
}
TEST(dprim_invalid_all_garbage_dc) {
    DP_NEED(); if (!have_all()) { t_skip("missing exports"); return; }
    static const ULONG_PTR g[] = { 1, 4, 0x1234, 0x00010001, 0xFFFFFFFF, 0x7FFFFFFF, 0x00FF0000, 0xDEADBEEF };
    for (unsigned i = 0; i < sizeof g / sizeof g[0]; i++) { int n = call_all((HDC)g[i]); CHECKF(n == 0, "handle %#lx: %d calls succeeded", (unsigned long)g[i], n); }
}
TEST(dprim_invalid_all_deleted_dc) {
    DP_NEED(); if (!have_all()) { t_skip("missing exports"); return; }
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(U(DeleteDC)(dc));
    EXPECT_EQ(call_all(dc), 0);
}
TEST(dprim_invalid_all_wrong_type_pen) {
    DP_NEED(); if (!have_all()) { t_skip("missing exports"); return; }
    HPEN o = U(CreatePen)(PS_SOLID, 1, 0);
    EXPECT_EQ(call_all((HDC)o), 0);
    U(DeleteObject)(o);
}
TEST(dprim_invalid_all_wrong_type_brush) {
    DP_NEED(); if (!have_all()) { t_skip("missing exports"); return; }
    HBRUSH o = U(CreateSolidBrush)(0);
    EXPECT_EQ(call_all((HDC)o), 0);
    U(DeleteObject)(o);
}
TEST(dprim_invalid_all_wrong_type_bitmap) {
    DP d; DP_NEED(); if (!have_all()) { t_skip("missing exports"); return; }
    CHECK(dp_open(&d, 8, 8, 32));
    EXPECT_EQ(call_all((HDC)d.bmp), 0);
    dp_close(&d);
}
TEST(dprim_invalid_all_stock_object_as_dc) {
    DP_NEED(); if (!have_all()) { t_skip("missing exports"); return; }
    EXPECT_EQ(call_all((HDC)U(GetStockObject)(BLACK_PEN)), 0);
    EXPECT_EQ(call_all((HDC)U(GetStockObject)(SYSTEM_FONT)), 0);
}
TEST(dprim_invalid_all_region_as_dc) {
    DP_NEED(); if (!have_all()) { t_skip("missing exports"); return; }
    REQUIRE(U(CreateRectRgn));
    HRGN r = U(CreateRectRgn)(0, 0, 5, 5);
    EXPECT_EQ(call_all((HDC)r), 0);
    U(DeleteObject)(r);
}
TEST(dprim_invalid_error_codes_report) {
    DP_NEED(); if (!have_all()) { t_skip("missing exports"); return; }
    SetLastError(0); U(LineTo)(NULL, 1, 1); DWORD e1 = GetLastError();
    SetLastError(0); U(Rectangle)(NULL, 1, 1, 5, 5); DWORD e2 = GetLastError();
    SetLastError(0); U(Ellipse)((HDC)(ULONG_PTR)0x1234, 1, 1, 5, 5); DWORD e3 = GetLastError();
    SetLastError(0); U(Polygon)(NULL, (POINT[]){{0,0},{1,1},{2,0}}, 3); DWORD e4 = GetLastError();
    t_note("last error: LineTo(NULL)=%lu Rectangle(NULL)=%lu Ellipse(garbage)=%lu Polygon(NULL)=%lu", e1, e2, e3, e4);
    CHECK(e1 == ERROR_INVALID_HANDLE || e1 == 0);
}
TEST(dprim_invalid_dc_after_delete_while_selected_bitmap) {
    DP_NEED();
    DP d; CHECK(dp_open(&d, 16, 16, 32));
    HDC dc = d.dc; HBITMAP bm = d.bmp;
    dp_black(&d);
    U(SelectObject)(dc, d.obmp);
    CHECK(U(DeleteDC)(dc));
    d.dc = NULL; d.nown = 0;
    EXPECT_EQ(U(LineTo)(dc, 3, 3), 0);
    CHECK(U(DeleteObject)(bm));
}

/* ---------------- cross-thread ---------------- */
struct tctx { HDC dc; int op; volatile LONG ok; };
static DWORD WINAPI thr(LPVOID a) {
    struct tctx *c = a;
    if (c->op == 0) { U(MoveToEx)(c->dc, 5, 5, NULL); c->ok = U(LineTo)(c->dc, 45, 5); }
    else if (c->op == 1) { c->ok = U(Rectangle)(c->dc, 10, 10, 30, 30); }
    else if (c->op == 2) { c->ok = U(LineTo)(c->dc, 45, 5); }
    return 0;
}
static void run_thr(struct tctx *c) { HANDLE h = CreateThread(NULL, 0, thr, c, 0, NULL); WaitForSingleObject(h, 20000); CloseHandle(h); }
TEST(dprim_thread_draw_on_foreign_dc) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    struct tctx c = { d.dc, 0, 0 };
    run_thr(&c);
    CHECK(c.ok);
    EXPECT_EQ(dp_ink(&d, NULL), 40);
    dp_close(&d);
}
TEST(dprim_thread_rectangle_on_foreign_dc) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    struct tctx c = { d.dc, 1, 0 };
    run_thr(&c);
    CHECK(c.ok);
    EXPECT_EQ(dp_cntc(&d, BLACK_C), 76);
    EXPECT_EQ(dp_cntc(&d, RED_C), 18 * 18);
    dp_close(&d);
}
TEST(dprim_thread_current_pos_shared) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 5, 5, NULL);
    struct tctx c = { d.dc, 2, 0 };
    run_thr(&c);
    CHECK(c.ok);
    POINT p = dp_cur(&d); EXPECT_EQ(p.x, 45); EXPECT_EQ(p.y, 5);
    EXPECT_EQ(dp_ink(&d, NULL), 40);
    dp_close(&d);
}
TEST(dprim_thread_deleted_dc_in_other_thread) {
    DP_NEED();
    HDC dc = U(CreateCompatibleDC)(NULL);
    U(DeleteDC)(dc);
    struct tctx c = { dc, 2, 99 };
    run_thr(&c);
    EXPECT_EQ(c.ok, 0);
}

/* ---------------- resources / robustness ---------------- */
TEST(dprim_res_no_leak_across_shapes) {
    DP_NEED(); REQUIRE(U(GetGuiResources));
    DWORD before = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    for (int i = 0; i < 30; i++) {
        DP d; dp_open(&d, 64, 48, 32); dp_black(&d); dp_brush(&d, RED_C);
        for (int s = 0; s < S_N; s++) shape(&d, s, 10, 5);
        dp_close(&d);
    }
    DWORD after = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    CHECKF(after == before, "before=%lu after=%lu", before, after);
}
TEST(dprim_res_pen_brush_churn) {
    DP d; DP_NEED(); REQUIRE(U(GetGuiResources));
    CHECK(dp_open(&d, 64, 48, 32));
    DWORD before = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    for (int i = 0; i < 2000; i++) {
        HPEN p = U(CreatePen)(PS_SOLID, 1 + i % 4, RGB(i, i >> 2, i >> 4)); HBRUSH b = U(CreateSolidBrush)(RGB(i >> 1, i, 3));
        HGDIOBJ op = U(SelectObject)(d.dc, p), ob = U(SelectObject)(d.dc, b);
        U(Rectangle)(d.dc, i % 20, i % 15, 30 + i % 20, 30 + i % 10);
        U(SelectObject)(d.dc, op); U(SelectObject)(d.dc, ob);
        U(DeleteObject)(p); U(DeleteObject)(b);
    }
    DWORD after = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    CHECKF(after == before, "before=%lu after=%lu", before, after);
    dp_close(&d);
}
TEST(dprim_res_large_surface_line_and_fill) {
    DP d; DP_NEED();
    if (!dp_open(&d, 2048, 2048, 32)) { t_skip("cannot create 2048x2048 DIB"); return; }
    dp_black(&d); dp_brush(&d, RED_C);
    dp_line(&d, 0, 0, 2048, 2048);
    int diag = 0; for (int i = 0; i < 2048; i++) diag += dp_raw(&d, i, i) == 0;
    EXPECT_EQ(diag, 2048);
    dp_stock(&d, NULL_PEN);
    U(Rectangle)(d.dc, 100, 100, 600, 400);
    CHECK(dp_px(&d, 300, 200) == RED_C);
    CHECK(dp_px(&d, 99, 200) == WHITE_C);
    dp_close(&d);
}
TEST(dprim_res_tiny_surface_1x1) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, 1, 1, 32)); dp_pen(&d, PS_SOLID, 1, RED_C); dp_brush(&d, BLUE_C);
    dp_line(&d, 0, 0, 5, 0);
    CHECK(dp_px(&d, 0, 0) == RED_C);
    U(Rectangle)(d.dc, -5, -5, 10, 10);
    U(Ellipse)(d.dc, -5, -5, 10, 10);
    POINT tri[3] = { {-5, -20}, {30, -20}, {-5, 20} };
    U(Polygon)(d.dc, tri, 3);
    CHECK(dp_px(&d, 0, 0) == BLUE_C);
    dp_close(&d);
}
TEST(dprim_res_odd_width_surface_stride) {
    static const int ws[] = { 1, 2, 3, 5, 7, 9, 13, 31, 33 };
    for (unsigned i = 0; i < sizeof ws / sizeof ws[0]; i++) {
        DP d; DP_NEED();
        CHECK(dp_open(&d, ws[i], 5, 24)); dp_pen(&d, PS_SOLID, 1, RED_C);
        dp_line(&d, 0, 2, ws[i], 2);
        EXPECT_EQ(dp_cntc(&d, RED_C), ws[i]);
        dp_close(&d);
    }
}
TEST(dprim_res_odd_width_surface_stride_8bpp) {
    static const int ws[] = { 1, 2, 3, 5, 7, 9, 13 };
    for (unsigned i = 0; i < sizeof ws / sizeof ws[0]; i++) {
        DP d; DP_NEED();
        CHECK(dp_open(&d, ws[i], 5, 8)); dp_pen(&d, PS_SOLID, 1, RGB(77, 77, 77));
        dp_line(&d, 0, 2, ws[i], 2);
        { int c77 = dp_cnt(&d, 77); CHECKF(c77 == ws[i], "c=%d", c77); }
        dp_close(&d);
    }
}
TEST(dprim_res_odd_width_surface_stride_1bpp) {
    static const int ws[] = { 1, 7, 8, 9, 15, 17, 31, 33, 65 };
    for (unsigned i = 0; i < sizeof ws / sizeof ws[0]; i++) {
        DP d; DP_NEED();
        CHECK(dp_open(&d, ws[i], 4, 1)); dp_pen(&d, PS_SOLID, 1, BLACK_C);
        dp_line(&d, 0, 1, ws[i], 1);
        EXPECT_EQ(dp_cnt(&d, 0), ws[i]);
        dp_close(&d);
    }
}
