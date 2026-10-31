/* Pixel access, rect helpers, flood fill, LineDDA, hatch/pattern brushes, clip regions, determinism. */
#include "dp.h"
#define W 64
#define H 64

static HGDIOBJ dp_own1(DP *d, HGDIOBJ o) {
    if (o && d->nown < 1100) { d->own[d->nown++] = o; U(SelectObject)(d->dc, o); }
    return o;
}

/* ---------------- SetPixel / GetPixel ---------------- */
TEST(dprim_px_32_set_get_roundtrip) {
    DP d; DP_NEED(); REQUIRE(U(SetPixel)); REQUIRE(U(GetPixel));
    CHECK(dp_open(&d, W, H, 32));
    COLORREF r = U(SetPixel)(d.dc, 3, 4, RGB(1, 2, 3));
    EXPECT_EQ(r, RGB(1, 2, 3));
    EXPECT_EQ(dp_raw(&d, 3, 4), 0x010203);
    COLORREF g = U(GetPixel)(d.dc, 3, 4); EXPECT_EQ(g, RGB(1, 2, 3));
    EXPECT_EQ(dp_ink(&d, NULL), 1);
    g = U(GetPixel)(d.dc, 4, 4); EXPECT_EQ(g, RGB(255, 255, 255));
    dp_close(&d);
}
TEST(dprim_px_32_setpixelv_returns_true) {
    DP d; DP_NEED(); REQUIRE(U(SetPixelV)); REQUIRE(U(GetPixel));
    CHECK(dp_open(&d, W, H, 32));
    CHECK(U(SetPixelV)(d.dc, 10, 11, RGB(9, 8, 7)));
    EXPECT_EQ(dp_raw(&d, 10, 11), 0x090807);
    EXPECT_EQ(U(GetPixel)(d.dc, 10, 11), RGB(9, 8, 7));
    dp_close(&d);
}
TEST(dprim_px_32_every_corner) {
    DP d; DP_NEED(); REQUIRE(U(SetPixelV));
    CHECK(dp_open(&d, W, H, 32));
    int xs[4] = { 0, W - 1, 0, W - 1 }, ys[4] = { 0, 0, H - 1, H - 1 };
    for (int i = 0; i < 4; i++) CHECK(U(SetPixelV)(d.dc, xs[i], ys[i], RGB(10 + i, 20, 30)));
    for (int i = 0; i < 4; i++) EXPECT_EQ(dp_raw(&d, xs[i], ys[i]), (DWORD)((10 + i) << 16 | 20 << 8 | 30));
    EXPECT_EQ(dp_ink(&d, NULL), 4);
    dp_close(&d);
}
TEST(dprim_px_get_outside_is_clr_invalid) {
    DP d; DP_NEED(); REQUIRE(U(GetPixel));
    CHECK(dp_open(&d, W, H, 32));
    EXPECT_EQ(U(GetPixel)(d.dc, -1, 0), CLR_INVALID);
    EXPECT_EQ(U(GetPixel)(d.dc, 0, -1), CLR_INVALID);
    EXPECT_EQ(U(GetPixel)(d.dc, W, 0), CLR_INVALID);
    EXPECT_EQ(U(GetPixel)(d.dc, 0, H), CLR_INVALID);
    EXPECT_EQ(U(GetPixel)(d.dc, 1 << 30, -(1 << 30)), CLR_INVALID);
    dp_close(&d);
}
TEST(dprim_px_set_outside_no_change) {
    DP d; DP_NEED(); REQUIRE(U(SetPixel)); REQUIRE(U(SetPixelV));
    CHECK(dp_open(&d, W, H, 32));
    COLORREF r = U(SetPixel)(d.dc, -1, 5, RGB(0, 0, 0));
    if (r != CLR_INVALID) t_note("SetPixel outside returned %08lx", (unsigned long)r);
    BOOL b = U(SetPixelV)(d.dc, W, 5, RGB(0, 0, 0)); SOFT(!b);
    U(SetPixelV)(d.dc, 5, H + 100, RGB(0, 0, 0));
    EXPECT_EQ(dp_ink(&d, NULL), 0);
    dp_close(&d);
}
TEST(dprim_px_invalid_dc) {
    DP_NEED(); REQUIRE(U(SetPixel)); REQUIRE(U(GetPixel)); REQUIRE(U(SetPixelV));
    HDC dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    EXPECT_EQ(U(GetPixel)(NULL, 0, 0), CLR_INVALID);
    EXPECT_EQ(U(GetPixel)(dc, 0, 0), CLR_INVALID);
    EXPECT_EQ(U(SetPixel)(NULL, 0, 0, 0), CLR_INVALID);
    EXPECT_EQ(U(SetPixel)(dc, 0, 0, 0), CLR_INVALID);
    CHECK(!U(SetPixelV)(NULL, 0, 0, 0));
    CHECK(!U(SetPixelV)(dc, 0, 0, 0));
}
TEST(dprim_px_24_exact) {
    DP d; DP_NEED(); REQUIRE(U(SetPixel)); REQUIRE(U(GetPixel));
    CHECK(dp_open(&d, W, H, 24));
    EXPECT_EQ(U(SetPixel)(d.dc, 7, 7, RGB(0x12, 0x34, 0x56)), RGB(0x12, 0x34, 0x56));
    EXPECT_EQ(dp_raw(&d, 7, 7), 0x123456);
    EXPECT_EQ(U(GetPixel)(d.dc, 7, 7), RGB(0x12, 0x34, 0x56));
    dp_close(&d);
}
TEST(dprim_px_16_primaries) {
    DP d; DP_NEED(); REQUIRE(U(SetPixel)); REQUIRE(U(GetPixel));
    CHECK(dp_open(&d, W, H, 16));
    U(SetPixel)(d.dc, 1, 1, RGB(255, 0, 0)); U(SetPixel)(d.dc, 2, 1, RGB(0, 255, 0)); U(SetPixel)(d.dc, 3, 1, RGB(0, 0, 255));
    EXPECT_EQ(dp_raw(&d, 1, 1), 0xF800); EXPECT_EQ(dp_raw(&d, 2, 1), 0x07E0); EXPECT_EQ(dp_raw(&d, 3, 1), 0x001F);
    EXPECT_EQ(U(GetPixel)(d.dc, 1, 1), RGB(255, 0, 0));
    EXPECT_EQ(U(GetPixel)(d.dc, 3, 1), RGB(0, 0, 255));
    COLORREF mid = U(GetPixel)(d.dc, 2, 1); EXPECT_EQ(mid, RGB(0, 255, 0));
    dp_close(&d);
}
TEST(dprim_px_8_grey_ramp) {
    DP d; DP_NEED(); REQUIRE(U(SetPixel)); REQUIRE(U(GetPixel));
    CHECK(dp_open_pal(&d, W, H, 8, NULL, 0));
    EXPECT_EQ(U(SetPixel)(d.dc, 2, 2, RGB(100, 100, 100)), RGB(100, 100, 100));
    EXPECT_EQ(dp_raw(&d, 2, 2), 100);
    COLORREF r = U(SetPixel)(d.dc, 3, 2, RGB(100, 101, 99));
    t_note("SetPixel near grey returned %06lx", (unsigned long)r);
    DWORD v = dp_raw(&d, 3, 2); CHECKF(v >= 99 && v <= 101, "idx %lu", (unsigned long)v);
    EXPECT_EQ(U(GetPixel)(d.dc, 2, 2), RGB(100, 100, 100));
    dp_close(&d);
}
TEST(dprim_px_1_black_white) {
    DP d; DP_NEED(); REQUIRE(U(SetPixel)); REQUIRE(U(GetPixel));
    CHECK(dp_open_pal(&d, 16, 16, 1, NULL, 0));
    U(PatBlt)(d.dc, 0, 0, 16, 16, WHITENESS);
    EXPECT_EQ(dp_raw(&d, 1, 1), 1);
    U(SetPixel)(d.dc, 1, 1, RGB(0, 0, 0));
    EXPECT_EQ(dp_raw(&d, 1, 1), 0);
    EXPECT_EQ(dp_raw(&d, 2, 1), 1);
    EXPECT_EQ(U(GetPixel)(d.dc, 1, 1), RGB(0, 0, 0));
    EXPECT_EQ(U(GetPixel)(d.dc, 2, 1), RGB(255, 255, 255));
    dp_close(&d);
}
TEST(dprim_px_setpixel_ignores_pen_and_brush) {
    DP d; DP_NEED(); REQUIRE(U(SetPixelV));
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_SOLID, 1, RED_C); dp_brush(&d, BLUE_C);
    U(SetPixelV)(d.dc, 5, 5, GREEN_C);
    EXPECT_EQ(dp_px(&d, 5, 5), GREEN_C); EXPECT_EQ(dp_ink(&d, NULL), 1);
    dp_close(&d);
}
TEST(dprim_px_setpixel_does_not_move_pos) {
    DP d; DP_NEED(); REQUIRE(U(SetPixelV));
    CHECK(dp_open(&d, W, H, 32));
    U(MoveToEx)(d.dc, 11, 12, NULL);
    U(SetPixelV)(d.dc, 5, 5, 0);
    POINT p = dp_cur(&d); EXPECT_EQ(p.x, 11); EXPECT_EQ(p.y, 12);
    dp_close(&d);
}
TEST(dprim_px_mapping_logical_coords) {
    DP d; DP_NEED(); REQUIRE(U(SetPixelV)); REQUIRE(U(SetViewportOrgEx));
    CHECK(dp_open(&d, W, H, 32));
    U(SetViewportOrgEx)(d.dc, 10, 20, NULL);
    U(SetPixelV)(d.dc, 1, 2, 0);
    EXPECT_EQ(dp_raw(&d, 11, 22), 0); EXPECT_EQ(dp_ink(&d, NULL), 1);
    EXPECT_EQ(U(GetPixel)(d.dc, 1, 2), RGB(0, 0, 0));
    U(SetViewportOrgEx)(d.dc, 0, 0, NULL);
    dp_close(&d);
}

/* ---------------- FillRect / FrameRect / InvertRect ---------------- */
TEST(dprim_rc_fillrect_exact) {
    DP d; DP_NEED(); REQUIRE(U(FillRect));
    CHECK(dp_open(&d, W, H, 32));
    HBRUSH b = (HBRUSH)dp_own1(&d, U(CreateSolidBrush)(RED_C)); U(SelectObject)(d.dc, d.obr);
    RECT rc = { 10, 12, 30, 25 };
    int r = U(FillRect)(d.dc, &rc, b); CHECK(r != 0);
    RECT bb; int n = dp_ink(&d, &bb);
    EXPECT_EQ(n, 20 * 13); EXPECT_EQ(bb.left, 10); EXPECT_EQ(bb.top, 12); EXPECT_EQ(bb.right, 30); EXPECT_EQ(bb.bottom, 25);
    EXPECT_EQ(dp_cntc(&d, RED_C), 260);
    dp_close(&d);
}
TEST(dprim_rc_fillrect_empty_and_reversed) {
    DP d; DP_NEED(); REQUIRE(U(FillRect));
    CHECK(dp_open(&d, W, H, 32));
    HBRUSH b = (HBRUSH)dp_own1(&d, U(CreateSolidBrush)(RED_C)); U(SelectObject)(d.dc, d.obr);
    RECT e = { 10, 10, 10, 30 }, v = { 30, 10, 10, 30 }, h = { 10, 30, 30, 10 };
    U(FillRect)(d.dc, &e, b);
    EXPECT_EQ(dp_ink(&d, NULL), 0);
    U(FillRect)(d.dc, &v, b); U(FillRect)(d.dc, &h, b);
    t_note("FillRect with reversed rects painted %d px", dp_ink(&d, NULL));
    dp_close(&d);
}
TEST(dprim_rc_fillrect_clipped_to_surface) {
    DP d; DP_NEED(); REQUIRE(U(FillRect));
    CHECK(dp_open(&d, W, H, 32));
    HBRUSH b = (HBRUSH)dp_own1(&d, U(CreateSolidBrush)(BLUE_C)); U(SelectObject)(d.dc, d.obr);
    RECT rc = { -100, -100, 5, 5 };
    U(FillRect)(d.dc, &rc, b);
    EXPECT_EQ(dp_ink(&d, NULL), 25);
    RECT big = { -(1 << 30), -(1 << 30), 1 << 30, 1 << 30 };
    U(FillRect)(d.dc, &big, b);
    EXPECT_EQ(dp_cntc(&d, BLUE_C), W * H);
    dp_close(&d);
}
TEST(dprim_rc_framerect_exact) {
    DP d; DP_NEED(); REQUIRE(U(FrameRect));
    CHECK(dp_open(&d, W, H, 32));
    HBRUSH b = (HBRUSH)dp_own1(&d, U(CreateSolidBrush)(GREEN_C)); U(SelectObject)(d.dc, d.obr);
    RECT rc = { 8, 9, 40, 30 };
    int r = U(FrameRect)(d.dc, &rc, b); CHECK(r != 0);
    int n = dp_ink(&d, NULL);
    EXPECT_EQ(n, 2 * 32 + 2 * 21 - 4);
    CHECK(dp_px(&d, 8, 9) == GREEN_C); CHECK(dp_px(&d, 39, 29) == GREEN_C);
    CHECK(dp_px(&d, 40, 29) == WHITE_C); CHECK(dp_px(&d, 39, 30) == WHITE_C);
    CHECK(dp_px(&d, 9, 10) == WHITE_C);
    dp_close(&d);
}
TEST(dprim_rc_invertrect_exact) {
    DP d; DP_NEED(); REQUIRE(U(InvertRect));
    CHECK(dp_open(&d, W, H, 32));
    RECT rc = { 4, 4, 20, 10 };
    CHECK(U(InvertRect)(d.dc, &rc));
    EXPECT_EQ(dp_cntc(&d, BLACK_C), 16 * 6);
    CHECK(U(InvertRect)(d.dc, &rc));
    EXPECT_EQ(dp_ink(&d, NULL), 0);
    dp_close(&d);
}
TEST(dprim_rc_invertrect_on_colour) {
    DP d; DP_NEED(); REQUIRE(U(InvertRect)); REQUIRE(U(FillRect));
    CHECK(dp_open(&d, W, H, 32)); dp_fill32(&d, RGB(0x12, 0x34, 0x56));
    RECT rc = { 0, 0, 8, 8 };
    U(InvertRect)(d.dc, &rc);
    EXPECT_EQ(dp_px(&d, 0, 0), RGB(0xED, 0xCB, 0xA9)); EXPECT_EQ(dp_px(&d, 8, 8), RGB(0x12, 0x34, 0x56));
    dp_close(&d);
}

/* ---------------- ExtFloodFill ---------------- */
static void flood_box(DP *d) {
    dp_pen(d, PS_SOLID, 1, BLACK_C); dp_stock(d, NULL_BRUSH);
    U(Rectangle)(d->dc, 10, 10, 40, 30);
}
TEST(dprim_flood_border_fills_interior) {
    DP d; DP_NEED(); REQUIRE(U(ExtFloodFill));
    CHECK(dp_open(&d, W, H, 32)); flood_box(&d);
    EXPECT_EQ(dp_cntc(&d, BLACK_C), 2 * 30 + 2 * 20 - 4);
    dp_brush(&d, RED_C);
    CHECK(U(ExtFloodFill)(d.dc, 20, 20, BLACK_C, FLOODFILLBORDER));
    EXPECT_EQ(dp_cntc(&d, RED_C), 28 * 18);
    EXPECT_EQ(dp_cntc(&d, BLACK_C), 96);
    CHECK(dp_px(&d, 5, 5) == WHITE_C);
    dp_close(&d);
}
TEST(dprim_flood_surface_fills_interior) {
    DP d; DP_NEED(); REQUIRE(U(ExtFloodFill));
    CHECK(dp_open(&d, W, H, 32)); flood_box(&d);
    dp_brush(&d, BLUE_C);
    CHECK(U(ExtFloodFill)(d.dc, 20, 20, WHITE_C, FLOODFILLSURFACE));
    EXPECT_EQ(dp_cntc(&d, BLUE_C), 28 * 18);
    dp_close(&d);
}
TEST(dprim_flood_outside_region_fills_outside) {
    DP d; DP_NEED(); REQUIRE(U(ExtFloodFill));
    CHECK(dp_open(&d, W, H, 32)); flood_box(&d);
    dp_brush(&d, RED_C);
    CHECK(U(ExtFloodFill)(d.dc, 2, 2, BLACK_C, FLOODFILLBORDER));
    EXPECT_EQ(dp_cntc(&d, RED_C), W * H - 30 * 20);
    dp_close(&d);
}
TEST(dprim_flood_start_on_border_fails) {
    DP d; DP_NEED(); REQUIRE(U(ExtFloodFill));
    CHECK(dp_open(&d, W, H, 32)); flood_box(&d);
    dp_brush(&d, RED_C);
    CHECK(!U(ExtFloodFill)(d.dc, 10, 10, BLACK_C, FLOODFILLBORDER));
    CHECK(!U(ExtFloodFill)(d.dc, 20, 20, BLACK_C, FLOODFILLSURFACE));
    EXPECT_EQ(dp_cntc(&d, RED_C), 0);
    dp_close(&d);
}
TEST(dprim_flood_start_outside_surface_fails) {
    DP d; DP_NEED(); REQUIRE(U(ExtFloodFill));
    CHECK(dp_open(&d, W, H, 32)); flood_box(&d);
    dp_brush(&d, RED_C);
    CHECK(!U(ExtFloodFill)(d.dc, -5, -5, BLACK_C, FLOODFILLBORDER));
    CHECK(!U(ExtFloodFill)(d.dc, W + 5, 3, BLACK_C, FLOODFILLBORDER));
    EXPECT_EQ(dp_cntc(&d, RED_C), 0);
    dp_close(&d);
}
TEST(dprim_flood_invalid_dc) {
    DP_NEED(); REQUIRE(U(ExtFloodFill));
    HDC dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    CHECK(!U(ExtFloodFill)(NULL, 1, 1, 0, FLOODFILLBORDER));
    CHECK(!U(ExtFloodFill)(dc, 1, 1, 0, FLOODFILLBORDER));
}
TEST(dprim_flood_leak_in_open_outline_fills_all) {
    DP d; DP_NEED(); REQUIRE(U(ExtFloodFill));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[4] = { {10, 10}, {40, 10}, {40, 30}, {10, 30} };
    U(Polyline)(d.dc, p, 4);
    dp_brush(&d, RED_C);
    CHECK(U(ExtFloodFill)(d.dc, 20, 20, BLACK_C, FLOODFILLBORDER));
    int ink = dp_cntc(&d, BLACK_C);
    EXPECT_EQ(dp_cntc(&d, RED_C), W * H - ink);
    dp_close(&d);
}

/* ---------------- LineDDA ---------------- */
struct dda { POINT p[300]; int n; };
static void CALLBACK dda_cb(int x, int y, LPARAM lp) { struct dda *s = (struct dda *)lp; if (s->n < 300) { s->p[s->n].x = x; s->p[s->n].y = y; } s->n++; }
TEST(dprim_dda_walks_line) {
    DP_NEED(); REQUIRE(U(LineDDA));
    static const int ln[][4] = { {0,0,10,0}, {0,0,0,10}, {0,0,10,10}, {3,4,40,9}, {5,5,-20,12}, {50,50,10,20}, {7,7,7,7}, {0,0,-9,-9} };
    for (unsigned i = 0; i < sizeof ln / sizeof ln[0]; i++) {
        struct dda s; s.n = 0;
        CHECK(U(LineDDA)(ln[i][0], ln[i][1], ln[i][2], ln[i][3], (LINEDDAPROC)dda_cb, (LPARAM)&s));
        int major = max(abs(ln[i][2] - ln[i][0]), abs(ln[i][3] - ln[i][1]));
        CHECKF(s.n == major || s.n == major + 1, "line %u n=%d major=%d", i, s.n, major);
        if (s.n > 0) { CHECK(s.p[0].x == ln[i][0] && s.p[0].y == ln[i][1]); }
        for (int k = 1; k < s.n && k < 300; k++)
            CHECKF(abs(s.p[k].x - s.p[k - 1].x) <= 1 && abs(s.p[k].y - s.p[k - 1].y) <= 1, "line %u step %d", i, k);
        t_note("LineDDA line %u emitted %d points (major %d)", i, s.n, major);
    }
}
TEST(dprim_dda_matches_lineto_pixels) {
    DP d; DP_NEED(); REQUIRE(U(LineDDA));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    struct dda s; s.n = 0;
    U(LineDDA)(5, 5, 50, 20, (LINEDDAPROC)dda_cb, (LPARAM)&s);
    dp_line(&d, 5, 5, 50, 20);
    int miss = 0;
    for (int k = 0; k < s.n; k++) if (dp_raw(&d, s.p[k].x, s.p[k].y) != 0) miss++;
    t_note("LineDDA points not inked by LineTo: %d of %d", miss, s.n);
    CHECKF(miss <= 1, "miss %d", miss);
    dp_close(&d);
}
TEST(dprim_dda_no_dc_needed) {
    DP_NEED(); REQUIRE(U(LineDDA));
    struct dda s; s.n = 0;
    CHECK(U(LineDDA)(0, 0, 100, 3, (LINEDDAPROC)dda_cb, (LPARAM)&s));
    CHECKF(s.n >= 100, "n=%d", s.n);
}

/* ---------------- hatch brushes ---------------- */
static int hatch_rows(DP *d, int *rows, int max) {
    int n = 0;
    for (int y = 0; y < d->h; y++) if (dp_row(d, y, 0, d->w) > 0 && n < max) rows[n++] = y;
    return n;
}
TEST(dprim_hatch_horizontal_rows) {
    DP d; DP_NEED(); REQUIRE(U(CreateHatchBrush)); REQUIRE(U(SetBkMode));
    CHECK(dp_open(&d, 16, 16, 32));
    dp_own1(&d, U(CreateHatchBrush)(HS_HORIZONTAL, BLACK_C)); U(SetBkMode)(d.dc, TRANSPARENT);
    CHECK(U(PatBlt)(d.dc, 0, 0, 16, 16, PATCOPY));
    int rows[16]; int n = hatch_rows(&d, rows, 16);
    EXPECT_EQ(n, 2);
    if (n == 2) { EXPECT_EQ(rows[1] - rows[0], 8); EXPECT_EQ(dp_row(&d, rows[0], 0, 16), 16); }
    EXPECT_EQ(dp_ink(&d, NULL), 32);
    dp_close(&d);
}
TEST(dprim_hatch_vertical_cols) {
    DP d; DP_NEED(); REQUIRE(U(CreateHatchBrush));
    CHECK(dp_open(&d, 16, 16, 32));
    dp_own1(&d, U(CreateHatchBrush)(HS_VERTICAL, BLACK_C)); U(SetBkMode)(d.dc, TRANSPARENT);
    CHECK(U(PatBlt)(d.dc, 0, 0, 16, 16, PATCOPY));
    int cols = 0, first = -1, second = -1;
    for (int x = 0; x < 16; x++) if (dp_col(&d, x, 0, 16) > 0) { cols++; if (first < 0) first = x; else second = x; }
    EXPECT_EQ(cols, 2); EXPECT_EQ(second - first, 8);
    EXPECT_EQ(dp_ink(&d, NULL), 32);
    dp_close(&d);
}
TEST(dprim_hatch_cross_count) {
    DP d; DP_NEED(); REQUIRE(U(CreateHatchBrush));
    CHECK(dp_open(&d, 16, 16, 32));
    dp_own1(&d, U(CreateHatchBrush)(HS_CROSS, BLACK_C)); U(SetBkMode)(d.dc, TRANSPARENT);
    CHECK(U(PatBlt)(d.dc, 0, 0, 16, 16, PATCOPY));
    EXPECT_EQ(dp_ink(&d, NULL), 60);
    dp_close(&d);
}
TEST(dprim_hatch_diagonals_equal_density) {
    DP d; DP_NEED(); REQUIRE(U(CreateHatchBrush));
    CHECK(dp_open(&d, 16, 16, 32));
    dp_own1(&d, U(CreateHatchBrush)(HS_FDIAGONAL, BLACK_C)); U(SetBkMode)(d.dc, TRANSPARENT);
    CHECK(U(PatBlt)(d.dc, 0, 0, 16, 16, PATCOPY));
    int a = dp_ink(&d, NULL);
    U(SelectObject)(d.dc, d.obr); dp_clr(&d);
    dp_own1(&d, U(CreateHatchBrush)(HS_BDIAGONAL, BLACK_C));
    CHECK(U(PatBlt)(d.dc, 0, 0, 16, 16, PATCOPY));
    int b = dp_ink(&d, NULL);
    EXPECT_EQ(a, 32); EXPECT_EQ(b, 32);
    dp_close(&d);
}
TEST(dprim_hatch_opaque_uses_bk_color) {
    DP d; DP_NEED(); REQUIRE(U(CreateHatchBrush));
    CHECK(dp_open(&d, 16, 16, 32));
    dp_own1(&d, U(CreateHatchBrush)(HS_HORIZONTAL, BLACK_C));
    U(SetBkMode)(d.dc, OPAQUE); U(SetBkColor)(d.dc, RED_C);
    CHECK(U(PatBlt)(d.dc, 0, 0, 16, 16, PATCOPY));
    EXPECT_EQ(dp_cntc(&d, BLACK_C), 32); EXPECT_EQ(dp_cntc(&d, RED_C), 224);
    dp_close(&d);
}
TEST(dprim_hatch_brush_origin_shifts_rows) {
    DP d; DP_NEED(); REQUIRE(U(CreateHatchBrush)); REQUIRE(U(SetBrushOrgEx));
    int rows0[16], rows1[16];
    CHECK(dp_open(&d, 16, 16, 32));
    dp_own1(&d, U(CreateHatchBrush)(HS_HORIZONTAL, BLACK_C)); U(SetBkMode)(d.dc, TRANSPARENT);
    U(PatBlt)(d.dc, 0, 0, 16, 16, PATCOPY);
    int n0 = hatch_rows(&d, rows0, 16);
    dp_clr(&d);
    POINT old = { -1, -1 };
    CHECK(U(SetBrushOrgEx)(d.dc, 0, 3, &old));
    EXPECT_EQ(old.x, 0); EXPECT_EQ(old.y, 0);
    U(PatBlt)(d.dc, 0, 0, 16, 16, PATCOPY);
    int n1 = hatch_rows(&d, rows1, 16);
    EXPECT_EQ(n0, 2); EXPECT_EQ(n1, 2);
    if (n0 == 2 && n1 == 2) EXPECT_EQ(rows1[0] % 8, (rows0[0] + 3) % 8);
    U(SetBrushOrgEx)(d.dc, 0, 0, NULL);
    dp_close(&d);
}
TEST(dprim_brushorg_get_set_roundtrip) {
    DP d; DP_NEED(); REQUIRE(U(SetBrushOrgEx)); REQUIRE(U(GetBrushOrgEx));
    CHECK(dp_open(&d, 8, 8, 32));
    POINT p = { 5, 5 };
    CHECK(U(GetBrushOrgEx)(d.dc, &p)); EXPECT_EQ(p.x, 0); EXPECT_EQ(p.y, 0);
    CHECK(U(SetBrushOrgEx)(d.dc, 3, 4, NULL));
    CHECK(U(GetBrushOrgEx)(d.dc, &p)); EXPECT_EQ(p.x, 3); EXPECT_EQ(p.y, 4);
    POINT o; CHECK(U(SetBrushOrgEx)(d.dc, -2, 100, &o)); EXPECT_EQ(o.x, 3); EXPECT_EQ(o.y, 4);
    CHECK(U(GetBrushOrgEx)(d.dc, &p)); EXPECT_EQ(p.x, -2); EXPECT_EQ(p.y, 100);
    CHECK(!U(SetBrushOrgEx)(NULL, 1, 1, NULL));
    U(SetBrushOrgEx)(d.dc, 0, 0, NULL);
    dp_close(&d);
}

/* ---------------- pattern brushes ---------------- */
TEST(dprim_pattern_mono_uses_text_and_bk) {
    DP d; DP_NEED(); REQUIRE(U(CreateBitmap)); REQUIRE(U(CreatePatternBrush));
    BYTE bits[16]; for (int i = 0; i < 8; i++) { bits[i * 2] = 0xAA; bits[i * 2 + 1] = 0; }
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, bits); CHECK(bm);
    HBRUSH br = U(CreatePatternBrush)(bm); CHECK(br);
    CHECK(dp_open(&d, 16, 16, 32));
    dp_own1(&d, br);
    U(SetTextColor)(d.dc, RED_C); U(SetBkColor)(d.dc, BLUE_C);
    CHECK(U(PatBlt)(d.dc, 0, 0, 16, 16, PATCOPY));
    EXPECT_EQ(dp_cntc(&d, RED_C), 128); EXPECT_EQ(dp_cntc(&d, BLUE_C), 128);
    EXPECT_EQ(dp_px(&d, 0, 0), BLUE_C);  /* bit 1 = background */
    EXPECT_EQ(dp_px(&d, 1, 0), RED_C);
    EXPECT_EQ(dp_px(&d, 8, 5), BLUE_C);
    U(SetTextColor)(d.dc, BLACK_C); U(SetBkColor)(d.dc, WHITE_C);
    dp_close(&d);
    U(DeleteObject)(bm);
}
TEST(dprim_pattern_mono_rectangle_null_pen) {
    DP d; DP_NEED(); REQUIRE(U(CreateBitmap)); REQUIRE(U(CreatePatternBrush));
    BYTE bits[16]; for (int i = 0; i < 8; i++) { bits[i * 2] = (i & 1) ? 0x00 : 0xFF; bits[i * 2 + 1] = 0; }
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, bits);
    HBRUSH br = U(CreatePatternBrush)(bm); CHECK(br);
    CHECK(dp_open(&d, 16, 16, 32));
    dp_own1(&d, br); dp_stock(&d, NULL_PEN);
    U(Rectangle)(d.dc, 0, 0, 17, 17);
    EXPECT_EQ(dp_cntc(&d, BLACK_C), 128);  /* odd rows are text colour (0 bits) */
    CHECK(dp_px(&d, 3, 1) == BLACK_C); CHECK(dp_px(&d, 3, 0) == WHITE_C);
    dp_close(&d);
    U(DeleteObject)(bm);
}
static COLORREF pat_col(int x, int y) { return RGB(16 + x * 20, 16 + y * 20, 40 + x + y); }
static void *make_pat_dib(void) {
    static struct { BITMAPINFOHEADER h; DWORD px[64]; } bi;
    memset(&bi, 0, sizeof bi);
    bi.h.biSize = sizeof bi.h; bi.h.biWidth = 8; bi.h.biHeight = 8; bi.h.biPlanes = 1; bi.h.biBitCount = 32;
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
        COLORREF c = pat_col(x, y);
        bi.px[(7 - y) * 8 + x] = (GetRValue(c) << 16) | (GetGValue(c) << 8) | GetBValue(c);
    }
    return &bi;
}
TEST(dprim_pattern_dib_origin0_exact) {
    DP d; DP_NEED(); REQUIRE(U(CreateDIBPatternBrushPt));
    CHECK(dp_open(&d, 24, 24, 32));
    HBRUSH br = U(CreateDIBPatternBrushPt)(make_pat_dib(), DIB_RGB_COLORS); CHECK(br);
    dp_own1(&d, br);
    CHECK(U(PatBlt)(d.dc, 0, 0, 24, 24, PATCOPY));
    int bad = 0;
    for (int y = 0; y < 24; y++) for (int x = 0; x < 24; x++) if (dp_px(&d, x, y) != pat_col(x % 8, y % 8)) bad++;
    EXPECT_EQ(bad, 0);
    dp_close(&d);
}
TEST(dprim_pattern_dib_origin_shift) {
    DP d; DP_NEED(); REQUIRE(U(CreateDIBPatternBrushPt)); REQUIRE(U(SetBrushOrgEx));
    CHECK(dp_open(&d, 24, 24, 32));
    HBRUSH br = U(CreateDIBPatternBrushPt)(make_pat_dib(), DIB_RGB_COLORS); CHECK(br);
    dp_own1(&d, br);
    U(SetBrushOrgEx)(d.dc, 3, 2, NULL);
    U(PatBlt)(d.dc, 0, 0, 24, 24, PATCOPY);
    int bad = 0;
    for (int y = 0; y < 24; y++) for (int x = 0; x < 24; x++)
        if (dp_px(&d, x, y) != pat_col(((x - 3) % 8 + 8) % 8, ((y - 2) % 8 + 8) % 8)) bad++;
    if (bad) t_note("DIB pattern with origin (3,2): %d pixels differ from the expected phase", bad);
    SOFT(bad == 0);
    U(SetBrushOrgEx)(d.dc, 0, 0, NULL);
    dp_close(&d);
}
TEST(dprim_pattern_dib_patblt_patinvert) {
    DP d; DP_NEED(); REQUIRE(U(CreateDIBPatternBrushPt));
    CHECK(dp_open(&d, 16, 16, 32));
    HBRUSH br = U(CreateDIBPatternBrushPt)(make_pat_dib(), DIB_RGB_COLORS); dp_own1(&d, br);
    U(PatBlt)(d.dc, 0, 0, 16, 16, PATINVERT);
    int bad = 0;
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
        COLORREF c = pat_col(x % 8, y % 8);
        if (dp_px(&d, x, y) != RGB(255 - GetRValue(c), 255 - GetGValue(c), 255 - GetBValue(c))) bad++;
    }
    EXPECT_EQ(bad, 0);
    dp_close(&d);
}
TEST(dprim_pattern_dib_invalid_usage) {
    DP_NEED(); REQUIRE(U(CreateDIBPatternBrushPt));
    SetLastError(0);
    HBRUSH b = U(CreateDIBPatternBrushPt)(NULL, DIB_RGB_COLORS);
    CHECK(b == NULL);
    if (b) U(DeleteObject)(b);
}

/* ---------------- clip regions ---------------- */
TEST(dprim_clip_elliptic_region_exact_cover) {
    DP d; DP_NEED(); REQUIRE(U(CreateEllipticRgn)); REQUIRE(U(SelectClipRgn)); REQUIRE(U(PtInRegion));
    CHECK(dp_open(&d, W, H, 32));
    HRGN rg = U(CreateEllipticRgn)(10, 8, 50, 40); CHECK(rg);
    int r = U(SelectClipRgn)(d.dc, rg); EXPECT_EQ(r, COMPLEXREGION);
    CHECK(U(PatBlt)(d.dc, 0, 0, W, H, BLACKNESS));
    int bad = 0, in = 0;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        int want = U(PtInRegion)(rg, x, y) ? 1 : 0; in += want;
        if ((dp_raw(&d, x, y) == 0) != want) bad++;
    }
    EXPECT_EQ(bad, 0); CHECK(in > 800);
    U(SelectClipRgn)(d.dc, NULL);
    U(DeleteObject)(rg);
    dp_close(&d);
}
TEST(dprim_clip_rect_region_clips_line_and_shapes) {
    DP d; DP_NEED(); REQUIRE(U(IntersectClipRect));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    int r = U(IntersectClipRect)(d.dc, 10, 10, 30, 30); EXPECT_EQ(r, SIMPLEREGION);
    dp_line(&d, 0, 20, 60, 20);
    EXPECT_EQ(dp_ink(&d, NULL), 20);
    dp_clr(&d);
    dp_brush(&d, RED_C); U(Ellipse)(d.dc, -50, -50, 150, 150);
    EXPECT_EQ(dp_cntc(&d, RED_C) + dp_cntc(&d, BLACK_C), 400);
    RECT bb; dp_ink(&d, &bb); EXPECT_EQ(bb.left, 10); EXPECT_EQ(bb.right, 30); EXPECT_EQ(bb.top, 10); EXPECT_EQ(bb.bottom, 30);
    U(SelectClipRgn)(d.dc, NULL);
    dp_close(&d);
}
TEST(dprim_clip_exclude_rect_leaves_hole) {
    DP d; DP_NEED(); REQUIRE(U(ExcludeClipRect));
    CHECK(dp_open(&d, W, H, 32));
    int r = U(ExcludeClipRect)(d.dc, 10, 10, 20, 20); EXPECT_EQ(r, COMPLEXREGION);
    U(PatBlt)(d.dc, 0, 0, W, H, BLACKNESS);
    EXPECT_EQ(dp_ink(&d, NULL), W * H - 100);
    CHECK(dp_px(&d, 15, 15) == WHITE_C); CHECK(dp_px(&d, 20, 15) == BLACK_C); CHECK(dp_px(&d, 9, 15) == BLACK_C);
    U(SelectClipRgn)(d.dc, NULL);
    dp_close(&d);
}
TEST(dprim_clip_empty_intersection_draws_nothing) {
    DP d; DP_NEED(); REQUIRE(U(IntersectClipRect));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    int r = U(IntersectClipRect)(d.dc, 10, 10, 20, 20); EXPECT_EQ(r, SIMPLEREGION);
    r = U(IntersectClipRect)(d.dc, 40, 40, 50, 50); EXPECT_EQ(r, NULLREGION);
    dp_line(&d, 0, 45, 60, 45); dp_line(&d, 0, 15, 60, 15);
    U(Rectangle)(d.dc, 0, 0, 60, 60);
    EXPECT_EQ(dp_ink(&d, NULL), 0);
    U(SelectClipRgn)(d.dc, NULL);
    dp_line(&d, 0, 45, 60, 45);
    EXPECT_EQ(dp_ink(&d, NULL), 60);
    dp_close(&d);
}
TEST(dprim_clip_getcliprgn_box_after_intersect) {
    DP d; DP_NEED(); REQUIRE(U(IntersectClipRect)); REQUIRE(U(GetClipBox));
    CHECK(dp_open(&d, W, H, 32));
    RECT rc; int r = U(GetClipBox)(d.dc, &rc);
    EXPECT_EQ(r, SIMPLEREGION); EXPECT_EQ(rc.left, 0); EXPECT_EQ(rc.top, 0); EXPECT_EQ(rc.right, W); EXPECT_EQ(rc.bottom, H);
    U(IntersectClipRect)(d.dc, 5, 6, 30, 31);
    r = U(GetClipBox)(d.dc, &rc);
    EXPECT_EQ(r, SIMPLEREGION); EXPECT_EQ(rc.left, 5); EXPECT_EQ(rc.top, 6); EXPECT_EQ(rc.right, 30); EXPECT_EQ(rc.bottom, 31);
    U(SelectClipRgn)(d.dc, NULL);
    dp_close(&d);
}
TEST(dprim_clip_polygon_and_bezier_respect_clip) {
    DP d; DP_NEED(); REQUIRE(U(IntersectClipRect));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    U(IntersectClipRect)(d.dc, 20, 20, 40, 40);
    POINT p[4] = { {0, 0}, {60, 0}, {60, 60}, {0, 60} };
    U(Polygon)(d.dc, p, 4);
    RECT bb; int n = dp_ink(&d, &bb);
    EXPECT_EQ(n, 400); EXPECT_EQ(bb.left, 20); EXPECT_EQ(bb.right, 40);
    dp_clr(&d);
    POINT bz[4] = { {0, 0}, {10, 60}, {50, 0}, {60, 60} };
    U(PolyBezier)(d.dc, bz, 4);
    int bad = 0;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) if (dp_inside(&d, x, y) && (x < 20 || x >= 40 || y < 20 || y >= 40)) bad++;
    EXPECT_EQ(bad, 0);
    U(SelectClipRgn)(d.dc, NULL);
    dp_close(&d);
}

/* ---------------- determinism ---------------- */
static void draw_scene(DP *d) {
    HPEN p = dp_pen(d, PS_SOLID, 1, RGB(0, 0, 128)); (void)p;
    dp_brush(d, RGB(200, 50, 50));
    HDC dc = d->dc;
    U(Ellipse)(dc, 3, 3, 40, 30);
    U(RoundRect)(dc, 30, 20, 90, 70, 17, 11);
    U(Pie)(dc, 60, 2, 110, 40, 100, 5, 70, 38);
    U(Chord)(dc, 2, 40, 50, 90, 3, 50, 45, 80);
    POINT bz[7] = { {5, 95}, {20, 60}, {40, 120}, {60, 95}, {80, 60}, {95, 120}, {110, 90} };
    U(PolyBezier)(dc, bz, 7);
    POINT st[5] = { {70, 80}, {90, 120}, {55, 100}, {95, 100}, {60, 120} };
    U(SetPolyFillMode)(dc, ALTERNATE); U(Polygon)(dc, st, 5);
    U(Arc)(dc, 10, 10, 100, 100, 100, 55, 10, 55);
    dp_line(d, 0, 0, 120, 119);
}
TEST(dprim_scene_deterministic_across_surfaces) {
    DP a, b; DP_NEED(); REQUIRE(U(RoundRect)); REQUIRE(U(Pie)); REQUIRE(U(Chord)); REQUIRE(U(Arc)); REQUIRE(U(PolyBezier)); REQUIRE(U(Polygon)); REQUIRE(U(Ellipse));
    CHECK(dp_open(&a, 120, 120, 32)); CHECK(dp_open(&b, 120, 120, 32));
    draw_scene(&a); draw_scene(&b);
    U(GdiFlush)();
    CHECK(memcmp(a.bits, b.bits, 120 * 4 * 120) == 0);
    CHECK(dp_ink(&a, NULL) > 1000);
    dp_close(&a); dp_close(&b);
}
TEST(dprim_scene_second_pass_is_idempotent) {
    DP a; DP_NEED(); REQUIRE(U(RoundRect)); REQUIRE(U(Pie)); REQUIRE(U(Chord)); REQUIRE(U(Arc)); REQUIRE(U(PolyBezier)); REQUIRE(U(Polygon)); REQUIRE(U(Ellipse));
    CHECK(dp_open(&a, 120, 120, 32));
    draw_scene(&a); U(GdiFlush)();
    BYTE *snap = malloc(120 * 4 * 120); CHECK(snap);
    if (!snap) { dp_close(&a); return; }
    memcpy(snap, a.bits, 120 * 4 * 120);
    /* every figure is repainted in the same order with the same pen and brush: result must not drift
       when the same paint sequence is applied again over the identical result, except where later
       figures overlapped earlier ones, so only compare a figure drawn last */
    dp_clr(&a);
    U(Ellipse)(a.dc, 3, 3, 40, 30); U(GdiFlush)();
    BYTE *s1 = malloc(120 * 4 * 120); memcpy(s1, a.bits, 120 * 4 * 120);
    U(Ellipse)(a.dc, 3, 3, 40, 30); U(GdiFlush)();
    CHECK(memcmp(s1, a.bits, 120 * 4 * 120) == 0);
    free(s1); free(snap);
    dp_close(&a);
}
TEST(dprim_scene_matches_stock_default_objects) {
    DP d; DP_NEED(); REQUIRE(U(GetCurrentObject));
    CHECK(dp_open(&d, 8, 8, 32));
    CHECK(U(GetCurrentObject)(d.dc, OBJ_PEN) == U(GetStockObject)(BLACK_PEN));
    CHECK(U(GetCurrentObject)(d.dc, OBJ_BRUSH) == U(GetStockObject)(WHITE_BRUSH));
    dp_close(&d);
}
TEST(dprim_savedc_restores_pen_for_drawing) {
    DP d; DP_NEED(); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_SOLID, 1, BLUE_C);
    int s = U(SaveDC)(d.dc); CHECK(s > 0);
    dp_pen(&d, PS_SOLID, 1, RED_C);
    dp_line(&d, 5, 5, 20, 5);
    CHECK(U(RestoreDC)(d.dc, -1));
    dp_line(&d, 5, 9, 20, 9);
    EXPECT_EQ(dp_cntc(&d, RED_C), 15); EXPECT_EQ(dp_cntc(&d, BLUE_C), 15);
    dp_close(&d);
}
TEST(dprim_savedc_restores_rop2_and_org) {
    DP d; DP_NEED(); REQUIRE(U(SaveDC)); REQUIRE(U(GetROP2));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(SaveDC)(d.dc);
    U(SetROP2)(d.dc, R2_NOT); U(SetViewportOrgEx)(d.dc, 7, 7, NULL);
    U(RestoreDC)(d.dc, -1);
    EXPECT_EQ(U(GetROP2)(d.dc), R2_COPYPEN);
    dp_line(&d, 5, 5, 15, 5);
    EXPECT_EQ(dp_raw(&d, 5, 5), 0);
    EXPECT_EQ(dp_ink(&d, NULL), 10);
    dp_close(&d);
}
TEST(dprim_pen_cosmetic_equals_createpen1) {
    DP a, b; DP_NEED(); REQUIRE(U(ExtCreatePen));
    CHECK(dp_open(&a, W, H, 32)); CHECK(dp_open(&b, W, H, 32));
    dp_pen(&a, PS_SOLID, 1, BLACK_C);
    dp_xpen(&b, PS_COSMETIC | PS_SOLID, 1, BLACK_C);
    static const int ln[][4] = { {2,3,60,50}, {60,3,2,50}, {3,60,50,2}, {5,5,50,7}, {5,5,7,50} };
    for (unsigned i = 0; i < 5; i++) { dp_line(&a, ln[i][0], ln[i][1], ln[i][2], ln[i][3]); dp_line(&b, ln[i][0], ln[i][1], ln[i][2], ln[i][3]); }
    U(GdiFlush)();
    CHECK(memcmp(a.bits, b.bits, W * 4 * H) == 0);
    dp_close(&a); dp_close(&b);
}
TEST(dprim_batch_limit_set_returns_previous) {
    DP_NEED(); REQUIRE(U(GdiSetBatchLimit)); REQUIRE(U(GdiGetBatchLimit));
    DWORD orig = U(GdiGetBatchLimit)();
    t_note("default batch limit %lu", (unsigned long)orig);
    DWORD prev = U(GdiSetBatchLimit)(1); EXPECT_EQ(prev, orig);
    EXPECT_EQ(U(GdiGetBatchLimit)(), 1);
    DP d; CHECK(dp_open(&d, 16, 16, 32)); dp_black(&d);
    dp_line(&d, 1, 1, 10, 1);
    EXPECT_EQ(dp_ink(&d, NULL), 9);
    dp_close(&d);
    U(GdiSetBatchLimit)(orig);
    EXPECT_EQ(U(GdiGetBatchLimit)(), orig);
}
TEST(dprim_gdiflush_true) {
    DP_NEED();
    CHECK(U(GdiFlush)());
    CHECK(U(GdiFlush)());
}
TEST(dprim_path_fill_rectangle_extent) {
    DP d; DP_NEED(); REQUIRE(U(BeginPath)); REQUIRE(U(EndPath)); REQUIRE(U(FillPath));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    CHECK(U(BeginPath)(d.dc));
    U(Rectangle)(d.dc, 10, 10, 30, 25);
    CHECK(U(EndPath)(d.dc));
    CHECK(U(FillPath)(d.dc));
    RECT bb; int n = dp_ink(&d, &bb);
    t_note("FillPath rect 20x15 -> %d px bbox %ld,%ld-%ld,%ld", n, bb.left, bb.top, bb.right, bb.bottom);
    CHECKF(n >= 19 * 14 && n <= 21 * 16, "n=%d", n);
    CHECK(bb.left >= 9 && bb.left <= 10 && bb.top >= 9 && bb.top <= 10);
    dp_close(&d);
}
TEST(dprim_path_stroke_without_path_fails) {
    DP d; DP_NEED(); REQUIRE(U(StrokePath)); REQUIRE(U(FillPath));
    CHECK(dp_open(&d, 16, 16, 32)); dp_black(&d);
    CHECK(!U(StrokePath)(d.dc));
    CHECK(!U(FillPath)(d.dc));
    EXPECT_EQ(dp_ink(&d, NULL), 0);
    dp_close(&d);
}
