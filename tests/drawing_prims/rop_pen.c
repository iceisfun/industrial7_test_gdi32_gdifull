/* ROP2 modes, dashed / geometric pens, caps and joins, bk mode, DC_PEN / DC_BRUSH. */
#include "dp.h"
#define W 128
#define H 96

static BYTE rop2_b(int mode, BYTE d, BYTE p) {
    switch (mode) {
    case R2_BLACK: return 0;
    case R2_NOTMERGEPEN: return ~(d | p);
    case R2_MASKNOTPEN: return d & ~p;
    case R2_NOTCOPYPEN: return ~p;
    case R2_MASKPENNOT: return p & ~d;
    case R2_NOT: return ~d;
    case R2_XORPEN: return d ^ p;
    case R2_NOTMASKPEN: return ~(d & p);
    case R2_MASKPEN: return d & p;
    case R2_NOTXORPEN: return ~(d ^ p);
    case R2_NOP: return d;
    case R2_MERGENOTPEN: return d | ~p;
    case R2_COPYPEN: return p;
    case R2_MERGEPENNOT: return p | ~d;
    case R2_MERGEPEN: return p | d;
    case R2_WHITE: return 255;
    }
    return 0;
}
static COLORREF rop2_c(int mode, COLORREF d, COLORREF p) {
    return RGB(rop2_b(mode, GetRValue(d), GetRValue(p)), rop2_b(mode, GetGValue(d), GetGValue(p)), rop2_b(mode, GetBValue(d), GetBValue(p)));
}
static const COLORREF dests[] = { RGB(255,255,255), RGB(0,0,0), RGB(0xCC,0x33,0x55), RGB(0x0F,0xF0,0x3C), RGB(0x80,0x80,0x80), RGB(255,0,0) };
static const COLORREF pens_[] = { RGB(255,255,255), RGB(0,0,0), RGB(0xAA,0x55,0xF0), RGB(0x12,0x34,0x56), RGB(0xFF,0x00,0x00), RGB(0x80,0x7F,0x01) };

static void rop_pen_check(int mode) {
    DP d; DP_NEED();
    for (unsigned i = 0; i < 6; i++) for (unsigned j = 0; j < 6; j++) {
        CHECK(dp_open(&d, 32, 8, 32));
        dp_fill32(&d, dests[i]);
        dp_pen(&d, PS_SOLID, 1, pens_[j]);
        int old = U(SetROP2)(d.dc, mode);
        EXPECT_EQ(old, R2_COPYPEN);
        dp_line(&d, 2, 3, 20, 3);
        COLORREF got = dp_px(&d, 10, 3), want = rop2_c(mode, dests[i], pens_[j]);
        CHECKF(got == want, "mode %d D=%06lx P=%06lx got %06lx want %06lx", mode, (unsigned long)dests[i], (unsigned long)pens_[j], (unsigned long)got, (unsigned long)want);
        CHECK(dp_px(&d, 10, 4) == dests[i]);
        CHECK(dp_px(&d, 25, 3) == dests[i]);
        dp_close(&d);
    }
}
static void rop_brush_check(int mode) {
    DP d; DP_NEED();
    for (unsigned i = 0; i < 6; i++) for (unsigned j = 0; j < 6; j++) {
        CHECK(dp_open(&d, 32, 16, 32));
        dp_fill32(&d, dests[i]);
        dp_stock(&d, NULL_PEN); dp_brush(&d, pens_[j]);
        U(SetROP2)(d.dc, mode);
        U(Rectangle)(d.dc, 4, 4, 20, 12);
        COLORREF got = dp_px(&d, 10, 8), want = rop2_c(mode, dests[i], pens_[j]);
        CHECKF(got == want, "mode %d D=%06lx B=%06lx got %06lx want %06lx", mode, (unsigned long)dests[i], (unsigned long)pens_[j], (unsigned long)got, (unsigned long)want);
        CHECK(dp_px(&d, 2, 8) == dests[i]);
        dp_close(&d);
    }
}
#define ROPT(nm, mode) \
    TEST(dprim_rop2_pen_##nm) { rop_pen_check(mode); } \
    TEST(dprim_rop2_brush_##nm) { rop_brush_check(mode); }
ROPT(black, R2_BLACK) ROPT(notmergepen, R2_NOTMERGEPEN) ROPT(masknotpen, R2_MASKNOTPEN) ROPT(notcopypen, R2_NOTCOPYPEN)
ROPT(maskpennot, R2_MASKPENNOT) ROPT(not, R2_NOT) ROPT(xorpen, R2_XORPEN) ROPT(notmaskpen, R2_NOTMASKPEN)
ROPT(maskpen, R2_MASKPEN) ROPT(notxorpen, R2_NOTXORPEN) ROPT(nop, R2_NOP) ROPT(mergenotpen, R2_MERGENOTPEN)
ROPT(copypen, R2_COPYPEN) ROPT(mergepennot, R2_MERGEPENNOT) ROPT(mergepen, R2_MERGEPEN) ROPT(white, R2_WHITE)

TEST(dprim_rop2_xor_twice_restores) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_fill32(&d, RGB(10, 200, 99)); dp_pen(&d, PS_SOLID, 1, RGB(77, 33, 250));
    U(SetROP2)(d.dc, R2_XORPEN);
    dp_line(&d, 5, 5, 60, 40);
    CHECK(dp_cntc(&d, RGB(10, 200, 99)) < W * H);
    dp_line(&d, 5, 5, 60, 40);
    EXPECT_EQ(dp_cntc(&d, RGB(10, 200, 99)), W * H);
    dp_close(&d);
}
TEST(dprim_rop2_not_twice_restores) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_fill32(&d, RGB(10, 200, 99)); dp_black(&d);
    U(SetROP2)(d.dc, R2_NOT);
    U(Rectangle)(d.dc, 10, 10, 50, 40);
    CHECK(dp_cntc(&d, RGB(10, 200, 99)) < W * H);
    U(Rectangle)(d.dc, 10, 10, 50, 40);
    EXPECT_EQ(dp_cntc(&d, RGB(10, 200, 99)), W * H);
    dp_close(&d);
}
TEST(dprim_rop2_nop_leaves_surface) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_SOLID, 1, RED_C); dp_brush(&d, BLUE_C);
    U(SetROP2)(d.dc, R2_NOP);
    U(Rectangle)(d.dc, 10, 10, 50, 40); dp_line(&d, 0, 0, 100, 90);
    U(Ellipse)(d.dc, 20, 20, 90, 70);
    EXPECT_EQ(dp_ink(&d, NULL), 0);
    dp_close(&d);
}
TEST(dprim_rop2_get_set) {
    DP d; DP_NEED(); REQUIRE(U(GetROP2));
    CHECK(dp_open(&d, 8, 8, 32));
    int r = U(GetROP2)(d.dc); EXPECT_EQ(r, R2_COPYPEN);
    for (int m = R2_BLACK; m <= R2_LAST; m++) {
        int prev = U(GetROP2)(d.dc);
        r = U(SetROP2)(d.dc, m); EXPECT_EQ(r, prev);
        r = U(GetROP2)(d.dc); EXPECT_EQ(r, m);
    }
    dp_close(&d);
}
TEST(dprim_rop2_invalid_modes) {
    DP d; DP_NEED(); REQUIRE(U(GetROP2));
    CHECK(dp_open(&d, 8, 8, 32));
    U(SetROP2)(d.dc, R2_XORPEN);
    static const int bad[] = { 0, -1, 17, 18, 100, 0x10000 + R2_XORPEN };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0);
        int r = U(SetROP2)(d.dc, bad[i]);
        t_note("SetROP2(%d) -> %d err=%lu now=%d", bad[i], r, GetLastError(), U(GetROP2)(d.dc));
        U(SetROP2)(d.dc, R2_XORPEN);
    }
    dp_close(&d);
}
TEST(dprim_rop2_invalid_dc) {
    DP_NEED(); REQUIRE(U(GetROP2));
    SetLastError(0);
    int r = U(SetROP2)(NULL, R2_NOT); EXPECT_EQ(r, 0);
    SOFT_ERR(ERROR_INVALID_HANDLE);
    r = U(GetROP2)(NULL); EXPECT_EQ(r, 0);
    HDC dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    r = U(SetROP2)(dc, R2_NOT); EXPECT_EQ(r, 0);
    r = U(GetROP2)(dc); EXPECT_EQ(r, 0);
}
TEST(dprim_rop2_per_dc) {
    DP a, b; DP_NEED(); REQUIRE(U(GetROP2));
    CHECK(dp_open(&a, 8, 8, 32)); CHECK(dp_open(&b, 8, 8, 32));
    U(SetROP2)(a.dc, R2_XORPEN);
    int r = U(GetROP2)(b.dc); EXPECT_EQ(r, R2_COPYPEN);
    dp_close(&a); dp_close(&b);
}
TEST(dprim_rop2_save_restore_dc) {
    DP d; DP_NEED(); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(GetROP2));
    CHECK(dp_open(&d, 8, 8, 32));
    int id = U(SaveDC)(d.dc);
    U(SetROP2)(d.dc, R2_NOT);
    CHECK(U(RestoreDC)(d.dc, id));
    int r = U(GetROP2)(d.dc); EXPECT_EQ(r, R2_COPYPEN);
    dp_close(&d);
}
TEST(dprim_rop2_xorpen_polygon_overlap) {
    DP d; DP_NEED(); REQUIRE(U(Polygon));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RGB(0xF0, 0, 0));
    U(SetROP2)(d.dc, R2_XORPEN);
    U(Rectangle)(d.dc, 10, 10, 50, 50); U(Rectangle)(d.dc, 30, 30, 70, 70);
    COLORREF x = RGB(255 ^ 0xF0, 255, 255);
    CHECK(dp_px(&d, 20, 20) == x);
    CHECK(dp_px(&d, 60, 60) == x);
    CHECK(dp_px(&d, 40, 40) == WHITE_C || dp_px(&d, 40, 40) == RGB(255, 255, 255));
    dp_close(&d);
}

/* ---------------- dashes ---------------- */
static void dash_pattern(DP *d, int y, int len, char *out) {
    for (int x = 0; x < len; x++) out[x] = dp_raw(d, x, y) == 0xFFFFFF ? '.' : '#';
    out[len] = 0;
}
#define DASHT(nm, style) \
TEST(dprim_dash_##nm##_count) { \
    DP d; DP_NEED(); \
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, style, 1, BLACK_C); \
    dp_line(&d, 0, 5, 100, 5); \
    int n = dp_row(&d, 5, 0, W); \
    CHECKF(n > 10 && n < 100, "n=%d", n); \
    CHECK(dp_raw(&d, 0, 5) == 0); \
    CHECK(dp_ink(&d, NULL) == n); \
    char pat[W + 1]; dash_pattern(&d, 5, 60, pat); t_note(#nm " pattern %s (%d/100)", pat, n); \
    dp_close(&d); \
} \
TEST(dprim_dash_##nm##_vertical_matches_horizontal) { \
    DP d; DP_NEED(); \
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, style, 1, BLACK_C); \
    dp_line(&d, 0, 5, 80, 5); dp_line(&d, 10, 0, 10, 80); \
    int h = dp_row(&d, 5, 0, 80), v = dp_col(&d, 10, 0, 80); \
    t_note(#nm " horizontal %d vertical %d (of 80)", h, v); \
    CHECKF(abs(h - v) <= 3, "h=%d v=%d", h, v); \
    dp_close(&d); \
} \
TEST(dprim_dash_##nm##_opaque_gaps) { \
    DP d; DP_NEED(); \
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, style, 1, BLACK_C); \
    U(SetBkColor)(d.dc, RED_C); U(SetBkMode)(d.dc, OPAQUE); \
    dp_line(&d, 0, 5, 100, 5); \
    int b = dp_cntc(&d, BLACK_C), r = dp_cntc(&d, RED_C); \
    CHECKF(b + r == 100, "black %d red %d", b, r); \
    CHECKF(r > 0 && b > 0, "black %d red %d", b, r); \
    dp_close(&d); \
} \
TEST(dprim_dash_##nm##_transparent_gaps) { \
    DP d; DP_NEED(); \
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, style, 1, BLACK_C); \
    U(SetBkColor)(d.dc, RED_C); U(SetBkMode)(d.dc, TRANSPARENT); \
    dp_line(&d, 0, 5, 100, 5); \
    EXPECT_EQ(dp_cntc(&d, RED_C), 0); \
    CHECK(dp_cntc(&d, BLACK_C) < 100 && dp_cntc(&d, BLACK_C) > 10); \
    dp_close(&d); \
} \
TEST(dprim_dash_##nm##_less_than_solid) { \
    DP d; DP_NEED(); \
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, style, 1, BLACK_C); \
    dp_line(&d, 5, 5, 105, 50); \
    CHECK(dp_ink(&d, NULL) < 100); \
    dp_close(&d); \
}
DASHT(dash, PS_DASH) DASHT(dot, PS_DOT) DASHT(dashdot, PS_DASHDOT) DASHT(dashdotdot, PS_DASHDOTDOT)

TEST(dprim_dash_ordering_note) {
    DP d; DP_NEED();
    int c[4]; int st[4] = { PS_DASH, PS_DOT, PS_DASHDOT, PS_DASHDOTDOT };
    for (int i = 0; i < 4; i++) {
        CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, st[i], 1, BLACK_C);
        dp_line(&d, 0, 5, 120, 5); c[i] = dp_ink(&d, NULL);
        dp_close(&d);
    }
    t_note("pixels in 120: dash=%d dot=%d dashdot=%d dashdotdot=%d", c[0], c[1], c[2], c[3]);
    CHECK(c[0] > c[1]);
    CHECK(c[2] < c[0]);
}
TEST(dprim_dash_reverse_direction_note) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_DASH, 1, BLACK_C);
    dp_line(&d, 0, 5, 100, 5);
    char a[W + 1], b[W + 1]; dash_pattern(&d, 5, 40, a);
    dp_clr(&d); dp_line(&d, 100, 5, 0, 5);
    dash_pattern(&d, 5, 40, b);
    t_note("fwd %s", a); t_note("rev %s", b);
    dp_close(&d);
}
TEST(dprim_dash_pattern_continues_across_lineto) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_DASH, 1, BLACK_C);
    dp_line(&d, 0, 5, 100, 5);
    int whole = dp_ink(&d, NULL);
    dp_clr(&d);
    U(MoveToEx)(d.dc, 0, 5, NULL); U(LineTo)(d.dc, 50, 5); U(LineTo)(d.dc, 100, 5);
    int split = dp_ink(&d, NULL);
    t_note("dash whole=%d split=%d", whole, split);
    CHECK(abs(whole - split) <= 4);
    dp_close(&d);
}
TEST(dprim_dash_ps_alternate) {
    DP d; DP_NEED(); REQUIRE(U(ExtCreatePen));
    CHECK(dp_open(&d, W, H, 32));
    LOGBRUSH lb = { BS_SOLID, BLACK_C, 0 };
    HPEN p = U(ExtCreatePen)(PS_COSMETIC | PS_ALTERNATE, 1, &lb, 0, NULL);
    CHECK(p != NULL);
    if (p) {
        d.own[d.nown++] = p; U(SelectObject)(d.dc, p);
        dp_line(&d, 0, 5, 100, 5);
        int n = dp_row(&d, 5, 0, W);
        CHECKF(n == 50, "n=%d", n);
        CHECK(dp_raw(&d, 0, 5) == 0);
        CHECK(dp_raw(&d, 1, 5) == 0xFFFFFF);
        CHECK(dp_raw(&d, 2, 5) == 0);
    }
    dp_close(&d);
}
TEST(dprim_dash_ps_userstyle_cosmetic) {
    DP d; DP_NEED(); REQUIRE(U(ExtCreatePen));
    CHECK(dp_open(&d, W, H, 32));
    LOGBRUSH lb = { BS_SOLID, BLACK_C, 0 };
    DWORD style[2] = { 4, 3 };
    HPEN p = U(ExtCreatePen)(PS_COSMETIC | PS_USERSTYLE, 1, &lb, 2, style);
    CHECK(p != NULL);
    if (p) {
        d.own[d.nown++] = p; U(SelectObject)(d.dc, p);
        dp_line(&d, 0, 5, 70, 5);
        char pat[W + 1]; dash_pattern(&d, 5, 30, pat); t_note("userstyle 4/3: %s", pat);
        int n = dp_row(&d, 5, 0, 70);
        t_note("userstyle 4/3 over 70 px -> %d on", n);
        CHECK(n > 20 && n < 60);
        CHECK(dp_raw(&d, 0, 5) == 0);
    }
    dp_close(&d);
}
TEST(dprim_dash_ps_userstyle_invalid) {
    DP_NEED(); REQUIRE(U(ExtCreatePen));
    LOGBRUSH lb = { BS_SOLID, 0, 0 };
    DWORD style[2] = { 4, 3 };
    SetLastError(0);
    HPEN p = U(ExtCreatePen)(PS_COSMETIC | PS_USERSTYLE, 1, &lb, 0, NULL);
    CHECK(p == NULL);
    if (p) U(DeleteObject)(p);
    p = U(ExtCreatePen)(PS_COSMETIC | PS_SOLID, 1, &lb, 2, style);
    CHECK(p == NULL);
    if (p) U(DeleteObject)(p);
    DWORD zero[2] = { 0, 0 };
    p = U(ExtCreatePen)(PS_COSMETIC | PS_USERSTYLE, 1, &lb, 2, zero);
    t_note("ExtCreatePen userstyle {0,0} -> %p", (void *)p);
    if (p) U(DeleteObject)(p);
}
TEST(dprim_dash_wide_pen_becomes_solid) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_SOLID, 3, BLACK_C);
    dp_line(&d, 10, 20, 90, 20);
    int solid = dp_ink(&d, NULL);
    dp_clr(&d);
    dp_pen(&d, PS_DASH, 3, BLACK_C);
    dp_line(&d, 10, 20, 90, 20);
    int dash = dp_ink(&d, NULL);
    t_note("width 3 solid=%d dash=%d", solid, dash);
    CHECK(dash == solid);
    dp_close(&d);
}
TEST(dprim_dash_null_pen_style) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_NULL, 1, BLACK_C);
    dp_line(&d, 10, 20, 90, 20);
    EXPECT_EQ(dp_ink(&d, NULL), 0);
    dp_close(&d);
}
TEST(dprim_dash_insideframe_width1_like_solid) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_INSIDEFRAME, 1, BLACK_C);
    dp_line(&d, 10, 20, 90, 20);
    EXPECT_EQ(dp_ink(&d, NULL), 80);
    dp_close(&d);
}
TEST(dprim_dash_dashed_rectangle_outline_bounded) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_DOT, 1, BLACK_C); dp_stock(&d, NULL_BRUSH);
    U(Rectangle)(d.dc, 10, 10, 90, 60);
    RECT rc; int n = dp_ink(&d, &rc);
    CHECK(n > 20 && n < 2 * (80 + 50) - 4);
    CHECK(rc.left >= 10 && rc.top >= 10 && rc.right <= 90 && rc.bottom <= 60);
    dp_close(&d);
}

/* ---------------- geometric pens ---------------- */
static int geo_area(DWORD style, int width, int x0, int y0, int x1, int y1) {
    DP d;
    if (!dp_open(&d, W, H, 32)) return -1;
    dp_xpen(&d, style, width, BLACK_C);
    dp_line(&d, x0, y0, x1, y1);
    int n = dp_ink(&d, NULL);
    dp_close(&d);
    return n;
}
TEST(dprim_geo_flat_cap_horizontal_area) {
    DP_NEED(); REQUIRE(U(ExtCreatePen));
    static const int ws[] = { 2, 3, 4, 5, 7, 9, 12 };
    for (unsigned i = 0; i < sizeof ws / sizeof ws[0]; i++) {
        int a = geo_area(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_FLAT, ws[i], 20, 40, 80, 40);
        CHECKF(a >= 60 * ws[i] - ws[i] && a <= 60 * ws[i] + ws[i], "width %d area %d", ws[i], a);
        t_note("flat cap width %d length 60 -> %d", ws[i], a);
    }
}
TEST(dprim_geo_flat_cap_vertical_area) {
    DP_NEED(); REQUIRE(U(ExtCreatePen));
    static const int ws[] = { 2, 3, 5, 9 };
    for (unsigned i = 0; i < sizeof ws / sizeof ws[0]; i++) {
        int a = geo_area(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_FLAT, ws[i], 40, 10, 40, 70);
        CHECKF(a >= 60 * ws[i] - ws[i] && a <= 60 * ws[i] + ws[i], "width %d area %d", ws[i], a);
    }
}
TEST(dprim_geo_cap_area_ordering) {
    DP_NEED(); REQUIRE(U(ExtCreatePen));
    int f = geo_area(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_FLAT, 11, 30, 40, 90, 40);
    int r = geo_area(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND, 11, 30, 40, 90, 40);
    int s = geo_area(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_SQUARE, 11, 30, 40, 90, 40);
    t_note("caps width 11 len 60: flat=%d round=%d square=%d", f, r, s);
    CHECK(f < r);
    CHECK(r < s);
    CHECK(s > 60 * 11 + 11 * 8);
    CHECK(f > 60 * 11 - 22 && f < 60 * 11 + 22);
}
TEST(dprim_geo_square_cap_extends_half_width) {
    DP d; DP_NEED(); REQUIRE(U(ExtCreatePen));
    CHECK(dp_open(&d, W, H, 32)); dp_xpen(&d, PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_SQUARE, 9, BLACK_C);
    dp_line(&d, 30, 40, 90, 40);
    RECT rc; dp_ink(&d, &rc);
    t_note("square cap bbox %ld %ld %ld %ld", rc.left, rc.top, rc.right, rc.bottom);
    CHECK(rc.left <= 27 && rc.left >= 24);
    CHECK(rc.right >= 92 && rc.right <= 96);
    CHECK(rc.bottom - rc.top == 9);
    dp_close(&d);
}
TEST(dprim_geo_flat_cap_bbox) {
    DP d; DP_NEED(); REQUIRE(U(ExtCreatePen));
    CHECK(dp_open(&d, W, H, 32)); dp_xpen(&d, PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_FLAT, 9, BLACK_C);
    dp_line(&d, 30, 40, 90, 40);
    RECT rc; dp_ink(&d, &rc);
    t_note("flat cap bbox %ld %ld %ld %ld", rc.left, rc.top, rc.right, rc.bottom);
    CHECK(rc.left >= 29 && rc.left <= 30);
    CHECK(rc.right >= 89 && rc.right <= 91);
    CHECK(rc.bottom - rc.top == 9);
    CHECK(rc.top >= 35 && rc.top <= 36);
    dp_close(&d);
}
TEST(dprim_geo_wide_cosmetic_createpen_horizontal_rows) {
    static const int ws[] = { 2, 3, 4, 5, 6, 7, 10 };
    for (unsigned i = 0; i < sizeof ws / sizeof ws[0]; i++) {
        DP d; DP_NEED();
        CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_SOLID, ws[i], BLACK_C);
        dp_line(&d, 30, 40, 90, 40);
        RECT rc; dp_ink(&d, &rc);
        CHECKF(rc.bottom - rc.top == ws[i], "width %d rows %ld", ws[i], rc.bottom - rc.top);
        t_note("CreatePen width %d line y=40 covers rows %ld..%ld", ws[i], rc.top, rc.bottom - 1);
        dp_close(&d);
    }
}
TEST(dprim_geo_wide_pen_diagonal_area) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_xpen(&d, PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_FLAT, 6, BLACK_C);
    dp_line(&d, 20, 20, 80, 80);
    int n = dp_ink(&d, NULL);
    double ex = 6.0 * 60.0 * 1.41421356;
    CHECKF(n > ex * 0.85 && n < ex * 1.3, "n=%d expected ~%.0f", n, ex);
    dp_close(&d);
}
TEST(dprim_geo_join_area_ordering) {
    int a[3]; DWORD js[3] = { PS_JOIN_BEVEL, PS_JOIN_ROUND, PS_JOIN_MITER };
    DP_NEED(); REQUIRE(U(ExtCreatePen)); REQUIRE(U(Polyline));
    for (int i = 0; i < 3; i++) {
        DP d; CHECK(dp_open(&d, W, H, 32));
        dp_xpen(&d, PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_FLAT | js[i], 15, BLACK_C);
        POINT p[3] = { {20, 20}, {80, 20}, {80, 70} };
        U(Polyline)(d.dc, p, 3);
        a[i] = dp_ink(&d, NULL);
        dp_close(&d);
    }
    t_note("joins width 15: bevel=%d round=%d miter=%d", a[0], a[1], a[2]);
    CHECK(a[0] < a[1]);
    CHECK(a[1] < a[2]);
}
TEST(dprim_geo_join_acute_miter_limit) {
    DP_NEED(); REQUIRE(U(ExtCreatePen)); REQUIRE(U(Polyline)); REQUIRE(U(SetMiterLimit));
    DP d; CHECK(dp_open(&d, W, H, 32));
    dp_xpen(&d, PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_FLAT | PS_JOIN_MITER, 8, BLACK_C);
    POINT p[3] = { {10, 40}, {100, 45}, {10, 50} };
    float old = 0;
    CHECK(U(SetMiterLimit)(d.dc, 1.0f, &old));
    t_note("default miter limit %g", old);
    U(Polyline)(d.dc, p, 3);
    RECT r1; int n1 = dp_ink(&d, &r1);
    dp_clr(&d);
    U(SetMiterLimit)(d.dc, 20.0f, NULL);
    U(Polyline)(d.dc, p, 3);
    RECT r2; int n2 = dp_ink(&d, &r2);
    t_note("miter limit 1 -> bbox right %ld area %d ; limit 20 -> right %ld area %d", r1.right, n1, r2.right, n2);
    CHECK(r2.right > r1.right);
    CHECK(n2 > n1);
    U(SetMiterLimit)(d.dc, old, NULL);
    dp_close(&d);
}
TEST(dprim_geo_miter_limit_get_set) {
    DP_NEED(); REQUIRE(U(SetMiterLimit)); REQUIRE(U(GetMiterLimit));
    DP d; CHECK(dp_open(&d, 8, 8, 32));
    float v = 0;
    CHECK(U(GetMiterLimit)(d.dc, &v));
    EXPECT_EQ((int)v, 10);
    float old = 0;
    CHECK(U(SetMiterLimit)(d.dc, 3.5f, &old)); CHECK(old == 10.0f);
    CHECK(U(GetMiterLimit)(d.dc, &v)); CHECK(v == 3.5f);
    SetLastError(0);
    BOOL r = U(SetMiterLimit)(d.dc, 0.5f, &old);
    t_note("SetMiterLimit(0.5) -> %d", r);
    r = U(SetMiterLimit)(d.dc, 0.0f, &old);
    t_note("SetMiterLimit(0) -> %d", r);
    CHECK(!U(SetMiterLimit)(NULL, 3.0f, &old));
    CHECK(!U(GetMiterLimit)(NULL, &v));
    dp_close(&d);
}
TEST(dprim_geo_pen_color_exact) {
    DP d; DP_NEED(); REQUIRE(U(ExtCreatePen));
    CHECK(dp_open(&d, W, H, 32)); dp_xpen(&d, PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_FLAT, 5, RGB(12, 34, 56));
    dp_line(&d, 20, 40, 80, 40);
    int n = dp_ink(&d, NULL);
    EXPECT_EQ(dp_cntc(&d, RGB(12, 34, 56)), n);
    dp_close(&d);
}
TEST(dprim_geo_polyline_wide_vs_thin_area) {
    DP d; DP_NEED(); REQUIRE(U(Polyline));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[4] = { {10, 10}, {60, 30}, {30, 60}, {100, 80} };
    U(Polyline)(d.dc, p, 4);
    int thin = dp_ink(&d, NULL);
    dp_clr(&d);
    dp_pen(&d, PS_SOLID, 4, BLACK_C);
    U(Polyline)(d.dc, p, 4);
    int wide = dp_ink(&d, NULL);
    t_note("polyline thin=%d wide4=%d", thin, wide);
    CHECK(wide > thin * 3 && wide < thin * 6);
    dp_close(&d);
}
TEST(dprim_geo_wide_ellipse_outline_thicker) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_stock(&d, NULL_BRUSH);
    U(Ellipse)(d.dc, 10, 10, 90, 80);
    int thin = dp_ink(&d, NULL);
    dp_clr(&d);
    dp_pen(&d, PS_SOLID, 5, BLACK_C);
    U(Ellipse)(d.dc, 10, 10, 90, 80);
    RECT rc; int wide = dp_ink(&d, &rc);
    t_note("ellipse thin=%d wide5=%d bbox %ld %ld %ld %ld", thin, wide, rc.left, rc.top, rc.right, rc.bottom);
    CHECK(wide > thin * 3);
    CHECK(rc.left >= 6 && rc.right <= 94 && rc.top >= 6 && rc.bottom <= 84);
    dp_close(&d);
}
TEST(dprim_geo_insideframe_stays_inside) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_INSIDEFRAME, 6, BLACK_C); dp_stock(&d, NULL_BRUSH);
    U(Rectangle)(d.dc, 20, 20, 80, 60);
    RECT rc; int n = dp_ink(&d, &rc);
    CHECKF(rc.left == 20 && rc.top == 20 && rc.right == 80 && rc.bottom == 60, "bbox %ld %ld %ld %ld", rc.left, rc.top, rc.right, rc.bottom);
    CHECKF(abs(n - (60 * 40 - 48 * 28)) <= 24, "n=%d want ~%d", n, 60 * 40 - 48 * 28);
    dp_close(&d);
}
TEST(dprim_geo_solid_wide_rectangle_straddles) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_SOLID, 6, BLACK_C); dp_stock(&d, NULL_BRUSH);
    U(Rectangle)(d.dc, 30, 30, 70, 60);
    RECT rc; dp_ink(&d, &rc);
    t_note("PS_SOLID width 6 rectangle bbox %ld %ld %ld %ld", rc.left, rc.top, rc.right, rc.bottom);
    CHECK(rc.left < 30 && rc.top < 30 && rc.right > 69 && rc.bottom > 59);
    dp_close(&d);
}

/* ---------------- DC_PEN / DC_BRUSH ---------------- */
TEST(dprim_dcpen_default_color) {
    DP d; DP_NEED(); REQUIRE(U(GetDCPenColor)); REQUIRE(U(SetDCPenColor));
    CHECK(dp_open(&d, 8, 8, 32));
    COLORREF c = U(GetDCPenColor)(d.dc);
    EXPECT_EQ(c, RGB(0, 0, 0));
    dp_close(&d);
}
TEST(dprim_dcbrush_default_color) {
    DP d; DP_NEED(); REQUIRE(U(GetDCBrushColor));
    CHECK(dp_open(&d, 8, 8, 32));
    COLORREF c = U(GetDCBrushColor)(d.dc);
    EXPECT_EQ(c, RGB(255, 255, 255));
    dp_close(&d);
}
TEST(dprim_dcpen_set_returns_previous) {
    DP d; DP_NEED(); REQUIRE(U(SetDCPenColor)); REQUIRE(U(GetDCPenColor));
    CHECK(dp_open(&d, 8, 8, 32));
    COLORREF prev = U(GetDCPenColor)(d.dc);
    COLORREF r = U(SetDCPenColor)(d.dc, RGB(1, 2, 3)); EXPECT_EQ(r, prev);
    r = U(SetDCPenColor)(d.dc, RGB(4, 5, 6)); EXPECT_EQ(r, RGB(1, 2, 3));
    r = U(GetDCPenColor)(d.dc); EXPECT_EQ(r, RGB(4, 5, 6));
    dp_close(&d);
}
TEST(dprim_dcpen_draws_with_dc_color) {
    DP d; DP_NEED(); REQUIRE(U(SetDCPenColor));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, DC_PEN);
    U(SetDCPenColor)(d.dc, RGB(200, 100, 50));
    dp_line(&d, 5, 5, 40, 5);
    EXPECT_EQ(dp_cntc(&d, RGB(200, 100, 50)), 35);
    U(SetDCPenColor)(d.dc, RGB(10, 20, 30));
    dp_line(&d, 5, 9, 40, 9);
    EXPECT_EQ(dp_cntc(&d, RGB(10, 20, 30)), 35);
    EXPECT_EQ(dp_cntc(&d, RGB(200, 100, 50)), 35);
    dp_close(&d);
}
TEST(dprim_dcbrush_fills_with_dc_color) {
    DP d; DP_NEED(); REQUIRE(U(SetDCBrushColor));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, DC_BRUSH); dp_stock(&d, NULL_PEN);
    U(SetDCBrushColor)(d.dc, RGB(200, 100, 50));
    U(Rectangle)(d.dc, 10, 10, 30, 30);
    int n = dp_cntc(&d, RGB(200, 100, 50));
    CHECKF(n >= 361 && n <= 441, "n=%d", n);
    U(SetDCBrushColor)(d.dc, RGB(1, 2, 3));
    U(Rectangle)(d.dc, 50, 10, 70, 30);
    CHECK(dp_cntc(&d, RGB(1, 2, 3)) >= 361);
    EXPECT_EQ(dp_cntc(&d, RGB(200, 100, 50)), n);
    dp_close(&d);
}
TEST(dprim_dcpen_not_selected_ignored) {
    DP d; DP_NEED(); REQUIRE(U(SetDCPenColor));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(SetDCPenColor)(d.dc, RED_C);
    dp_line(&d, 5, 5, 40, 5);
    EXPECT_EQ(dp_cntc(&d, RED_C), 0);
    EXPECT_EQ(dp_cntc(&d, BLACK_C), 35);
    dp_close(&d);
}
TEST(dprim_dcpen_color_survives_deselect) {
    DP d; DP_NEED(); REQUIRE(U(SetDCPenColor)); REQUIRE(U(GetDCPenColor));
    CHECK(dp_open(&d, 8, 8, 32)); dp_stock(&d, DC_PEN);
    U(SetDCPenColor)(d.dc, RGB(9, 8, 7));
    dp_stock(&d, BLACK_PEN);
    COLORREF c = U(GetDCPenColor)(d.dc); EXPECT_EQ(c, RGB(9, 8, 7));
    dp_close(&d);
}
TEST(dprim_dcpen_invalid_dc) {
    DP_NEED(); REQUIRE(U(SetDCPenColor)); REQUIRE(U(GetDCPenColor)); REQUIRE(U(SetDCBrushColor)); REQUIRE(U(GetDCBrushColor));
    SetLastError(0);
    COLORREF c;
    c = U(SetDCPenColor)(NULL, RED_C); EXPECT_EQ(c, CLR_INVALID);
    c = U(GetDCPenColor)(NULL); EXPECT_EQ(c, CLR_INVALID);
    c = U(SetDCBrushColor)(NULL, RED_C); EXPECT_EQ(c, CLR_INVALID);
    c = U(GetDCBrushColor)(NULL); EXPECT_EQ(c, CLR_INVALID);
    SOFT_ERR(ERROR_INVALID_HANDLE);
}
TEST(dprim_dcpen_color_masks_high_byte) {
    DP d; DP_NEED(); REQUIRE(U(SetDCPenColor)); REQUIRE(U(GetDCPenColor));
    CHECK(dp_open(&d, 8, 8, 32));
    U(SetDCPenColor)(d.dc, 0x01FFFFFF);
    COLORREF c = U(GetDCPenColor)(d.dc);
    t_note("SetDCPenColor(0x01FFFFFF) reads back 0x%08lx", (unsigned long)c);
    U(SetDCPenColor)(d.dc, 0x02112233);
    c = U(GetDCPenColor)(d.dc);
    t_note("SetDCPenColor(0x02112233) reads back 0x%08lx", (unsigned long)c);
    dp_close(&d);
}
TEST(dprim_dcpen_stock_object_types) {
    DP_NEED();
    HGDIOBJ p = U(GetStockObject)(DC_PEN), b = U(GetStockObject)(DC_BRUSH);
    CHECK(p && b);
    DWORD t = U(GetObjectType)(p); EXPECT_EQ(t, OBJ_PEN);
    t = U(GetObjectType)(b); EXPECT_EQ(t, OBJ_BRUSH);
    LOGPEN lp; memset(&lp, 0xCC, sizeof lp);
    int n = U(GetObjectA)(p, sizeof lp, &lp);
    t_note("GetObject(DC_PEN) -> %d style=%u width=%ld color=%06lx", n, lp.lopnStyle, lp.lopnWidth.x, (unsigned long)lp.lopnColor);
    EXPECT_EQ(n, sizeof lp);
    EXPECT_EQ(lp.lopnStyle, PS_SOLID);
}

/* ---------------- background mode ---------------- */
TEST(dprim_bkmode_does_not_affect_solid_line) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); U(SetBkColor)(d.dc, RED_C);
    U(SetBkMode)(d.dc, OPAQUE); dp_line(&d, 5, 5, 50, 5);
    U(SetBkMode)(d.dc, TRANSPARENT); dp_line(&d, 5, 9, 50, 9);
    EXPECT_EQ(dp_cntc(&d, RED_C), 0);
    EXPECT_EQ(dp_cntc(&d, BLACK_C), 90);
    dp_close(&d);
}
TEST(dprim_bkmode_dashed_rect_opaque_fills_gaps) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_DASH, 1, BLACK_C); dp_stock(&d, NULL_BRUSH);
    U(SetBkColor)(d.dc, GREEN_C); U(SetBkMode)(d.dc, OPAQUE);
    U(Rectangle)(d.dc, 10, 10, 60, 40);
    int b = dp_cntc(&d, BLACK_C), g = dp_cntc(&d, GREEN_C);
    CHECKF(b + g == 2 * (50 + 30) - 4, "black %d green %d", b, g);
    dp_close(&d);
}
TEST(dprim_bkmode_getset) {
    DP d; DP_NEED(); REQUIRE(U(GetBkMode)); REQUIRE(U(SetBkMode));
    CHECK(dp_open(&d, 8, 8, 32));
    int r = U(GetBkMode)(d.dc); EXPECT_EQ(r, OPAQUE);
    r = U(SetBkMode)(d.dc, TRANSPARENT); EXPECT_EQ(r, OPAQUE);
    r = U(SetBkMode)(d.dc, OPAQUE); EXPECT_EQ(r, TRANSPARENT);
    r = U(SetBkMode)(d.dc, 0); EXPECT_EQ(r, 0);
    r = U(SetBkMode)(d.dc, 3); EXPECT_EQ(r, 0);
    r = U(GetBkMode)(d.dc); EXPECT_EQ(r, OPAQUE);
    dp_close(&d);
}
TEST(dprim_bkcolor_default_white) {
    DP d; DP_NEED(); REQUIRE(U(GetBkColor));
    CHECK(dp_open(&d, 8, 8, 32));
    COLORREF c = U(GetBkColor)(d.dc); EXPECT_EQ(c, RGB(255, 255, 255));
    COLORREF r = U(SetBkColor)(d.dc, RGB(1, 2, 3)); EXPECT_EQ(r, RGB(255, 255, 255));
    c = U(GetBkColor)(d.dc); EXPECT_EQ(c, RGB(1, 2, 3));
    dp_close(&d);
}
