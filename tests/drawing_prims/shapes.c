/* Rectangle / RoundRect / Polygon / PolyPolygon on 32bpp DIBSection memory DCs. */
#include "dp.h"
#define W 100
#define H 100

static void star(POINT *p, int cx, int cy, int r) {
    for (int k = 0; k < 5; k++) {
        int i = (k * 2) % 5;
        double a = -3.14159265358979 / 2 + i * 2 * 3.14159265358979 / 5;
        p[k].x = cx + (int)(r * __builtin_cos(a) + (__builtin_cos(a) >= 0 ? 0.5 : -0.5));
        p[k].y = cy + (int)(r * __builtin_sin(a) + (__builtin_sin(a) >= 0 ? 0.5 : -0.5));
    }
}

/* ---------------- Rectangle ---------------- */
TEST(dprim_rect_pen_outline_exact) {
    static const int sz[][2] = { {3,3}, {4,4}, {5,3}, {10,10}, {30,20}, {50,7}, {2,9} };
    for (unsigned i = 0; i < sizeof sz / sizeof sz[0]; i++) {
        DP d; DP_NEED(); REQUIRE(U(Rectangle));
        CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_stock(&d, NULL_BRUSH);
        int w = sz[i][0], h = sz[i][1];
        CHECK(U(Rectangle)(d.dc, 20, 30, 20 + w, 30 + h));
        RECT rc; int n = dp_ink(&d, &rc);
        int want = (w == 1 || h == 1) ? w * h : (w == 2 || h == 2) ? w * h : 2 * (w + h) - 4;
        CHECKF(rc.left == 20 && rc.top == 30 && rc.right == 20 + w && rc.bottom == 30 + h, "%dx%d bbox %ld %ld %ld %ld", w, h, rc.left, rc.top, rc.right, rc.bottom);
        CHECKF(n == want, "%dx%d ink %d want %d", w, h, n, want);
        dp_close(&d);
    }
}
TEST(dprim_rect_pen_and_brush_exact) {
    static const int sz[][2] = { {3,3}, {4,4}, {5,3}, {10,10}, {30,20}, {50,7}, {17,41} };
    for (unsigned i = 0; i < sizeof sz / sizeof sz[0]; i++) {
        DP d; DP_NEED();
        CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
        int w = sz[i][0], h = sz[i][1];
        CHECK(U(Rectangle)(d.dc, 20, 30, 20 + w, 30 + h));
        int b = dp_cntc(&d, BLACK_C), r = dp_cntc(&d, RED_C);
        CHECKF(b == 2 * (w + h) - 4, "%dx%d black %d", w, h, b);
        CHECKF(r == (w - 2) * (h - 2), "%dx%d red %d", w, h, r);
        dp_close(&d);
    }
}
TEST(dprim_rect_corner_pixels) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    U(Rectangle)(d.dc, 10, 10, 40, 30);
    CHECK(dp_px(&d, 10, 10) == BLACK_C);
    CHECK(dp_px(&d, 39, 10) == BLACK_C);
    CHECK(dp_px(&d, 10, 29) == BLACK_C);
    CHECK(dp_px(&d, 39, 29) == BLACK_C);
    CHECK(dp_px(&d, 40, 29) == WHITE_C);
    CHECK(dp_px(&d, 39, 30) == WHITE_C);
    CHECK(dp_px(&d, 9, 10) == WHITE_C);
    CHECK(dp_px(&d, 10, 9) == WHITE_C);
    CHECK(dp_px(&d, 11, 11) == RED_C);
    CHECK(dp_px(&d, 38, 28) == RED_C);
    dp_close(&d);
}
TEST(dprim_rect_null_pen_fill_extent) {
    static const int sz[][2] = { {3,3}, {10,10}, {30,20}, {7,3}, {3,7} };
    for (unsigned i = 0; i < sizeof sz / sizeof sz[0]; i++) {
        DP d; DP_NEED();
        CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
        int w = sz[i][0], h = sz[i][1];
        CHECK(U(Rectangle)(d.dc, 20, 30, 20 + w, 30 + h));
        RECT rc; int n = dp_ink(&d, &rc);
        CHECKF(n > 0 && rc.left >= 19 && rc.left <= 20 && rc.top >= 29 && rc.top <= 30, "%dx%d top-left %ld,%ld", w, h, rc.left, rc.top);
        CHECKF(rc.right <= 21 + w && rc.bottom <= 31 + h, "%dx%d bbox %ld %ld", w, h, rc.right, rc.bottom);
        t_note("NULL_PEN Rectangle %dx%d fills %d px (bbox %ldx%ld)", w, h, n, rc.right - rc.left, rc.bottom - rc.top);
        dp_close(&d);
    }
}
TEST(dprim_rect_null_pen_null_brush_nothing) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_stock(&d, NULL_BRUSH);
    CHECK(U(Rectangle)(d.dc, 10, 10, 40, 40));
    EXPECT_EQ(dp_ink(&d, NULL), 0);
    dp_close(&d);
}
TEST(dprim_rect_null_brush_hollow) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_stock(&d, NULL_BRUSH);
    U(Rectangle)(d.dc, 10, 10, 40, 40);
    CHECK(dp_px(&d, 20, 20) == WHITE_C);
    dp_close(&d);
}
TEST(dprim_rect_null_brush_preserves_background) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_fill32(&d, RGB(10, 20, 30)); dp_black(&d); dp_stock(&d, NULL_BRUSH);
    U(Rectangle)(d.dc, 10, 10, 40, 40);
    CHECK(dp_px(&d, 20, 20) == RGB(10, 20, 30));
    dp_close(&d);
}
TEST(dprim_rect_does_not_move_current_pos) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 3, 4, NULL);
    U(Rectangle)(d.dc, 10, 10, 40, 40);
    POINT c = dp_cur(&d); EXPECT_EQ(c.x, 3); EXPECT_EQ(c.y, 4);
    dp_close(&d);
}
TEST(dprim_rect_empty_and_tiny) {
    static const int r[][4] = { {10,10,10,10}, {10,10,11,11}, {10,10,12,12}, {10,10,10,20}, {10,10,20,10}, {10,10,11,20}, {10,10,12,20} };
    for (unsigned i = 0; i < sizeof r / sizeof r[0]; i++) {
        DP d; DP_NEED();
        CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
        BOOL ok = U(Rectangle)(d.dc, r[i][0], r[i][1], r[i][2], r[i][3]);
        t_note("Rectangle(%d,%d,%d,%d) -> %d, black=%d red=%d", r[i][0], r[i][1], r[i][2], r[i][3], ok, dp_cntc(&d, BLACK_C), dp_cntc(&d, RED_C));
        dp_close(&d);
    }
}
TEST(dprim_rect_1x1_draws_one_pixel_or_none) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    U(Rectangle)(d.dc, 10, 10, 11, 11);
    int n = dp_ink(&d, NULL);
    CHECKF(n <= 1, "n=%d", n);
    dp_close(&d);
}
TEST(dprim_rect_2x2_all_pen) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    U(Rectangle)(d.dc, 10, 10, 12, 12);
    EXPECT_EQ(dp_cntc(&d, BLACK_C), 4);
    EXPECT_EQ(dp_cntc(&d, RED_C), 0);
    dp_close(&d);
}
TEST(dprim_rect_reversed_coords) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    U(Rectangle)(d.dc, 10, 10, 40, 30);
    BYTE *a = malloc(W * H); dp_snap(&d, a);
    dp_clr(&d);
    BOOL r = U(Rectangle)(d.dc, 40, 30, 10, 10);
    RECT rc; int n = dp_ink(&d, &rc);
    t_note("reversed Rectangle ret=%d ink=%d diff=%d bbox=%ld,%ld,%ld,%ld", r, n, dp_diff(&d, a), rc.left, rc.top, rc.right, rc.bottom);
    CHECK(r);
    free(a);
    dp_close(&d);
}
TEST(dprim_rect_clipped_partial) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    U(Rectangle)(d.dc, -10, -10, 30, 20);
    EXPECT_EQ(dp_cntc(&d, BLACK_C), 49);
    EXPECT_EQ(dp_cntc(&d, RED_C), 29 * 19);
    dp_close(&d);
}
TEST(dprim_rect_outside_surface) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    CHECK(U(Rectangle)(d.dc, 200, 200, 300, 300));
    CHECK(U(Rectangle)(d.dc, -300, -300, -200, -200));
    CHECK(U(Rectangle)(d.dc, 100, 0, 150, 50));
    EXPECT_EQ(dp_ink(&d, NULL), 0);
    dp_close(&d);
}
TEST(dprim_rect_covers_whole_surface) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    U(Rectangle)(d.dc, -5, -5, W + 5, H + 5);
    EXPECT_EQ(dp_cntc(&d, RED_C), W * H);
    dp_close(&d);
}
TEST(dprim_rect_huge_coords) {
    DP d; DP_NEED(); NOT_WINE("huge coordinates corrupt wine heap");
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    BOOL r = U(Rectangle)(d.dc, -(1 << 30), -(1 << 30), 1 << 30, 1 << 30);
    t_note("Rectangle +-2^30 -> %d red=%d", r, dp_cntc(&d, RED_C));
    r = U(Rectangle)(d.dc, (int)0x80000000, (int)0x80000000, 0x7FFFFFFF, 0x7FFFFFFF);
    t_note("Rectangle int limits -> %d red=%d", r, dp_cntc(&d, RED_C));
    r = U(Rectangle)(d.dc, 0, 0, 0x7FFFFFFF, 0x7FFFFFFF);
    dp_close(&d);
}
TEST(dprim_rect_translate_invariant) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    U(Rectangle)(d.dc, 5, 5, 25, 20);
    int b0 = dp_cntc(&d, BLACK_C), r0 = dp_cntc(&d, RED_C);
    dp_clr(&d);
    U(Rectangle)(d.dc, 55, 60, 75, 75);
    EXPECT_EQ(dp_cntc(&d, BLACK_C), b0);
    EXPECT_EQ(dp_cntc(&d, RED_C), r0);
    dp_close(&d);
}
TEST(dprim_rect_invalid_dc) {
    DP_NEED();
    SetLastError(0);
    CHECK(!U(Rectangle)(NULL, 0, 0, 10, 10));
    SOFT_ERR(ERROR_INVALID_HANDLE);
    CHECK(!U(Rectangle)((HDC)(ULONG_PTR)0x1234, 0, 0, 10, 10));
    HDC dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    SetLastError(0);
    CHECK(!U(Rectangle)(dc, 0, 0, 10, 10));
    SOFT_ERR(ERROR_INVALID_HANDLE);
    HBRUSH b = U(CreateSolidBrush)(0);
    CHECK(!U(Rectangle)((HDC)b, 0, 0, 10, 10));
    U(DeleteObject)(b);
}
TEST(dprim_rect_dashed_pen_note) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_DASH, 1, BLACK_C); dp_stock(&d, NULL_BRUSH);
    U(Rectangle)(d.dc, 10, 10, 60, 40);
    int n = dp_ink(&d, NULL);
    CHECKF(n > 20 && n < 2 * (50 + 30) - 4, "n=%d", n);
    dp_close(&d);
}
TEST(dprim_rect_pen_color_edges) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_SOLID, 1, BLUE_C); dp_brush(&d, GREEN_C);
    U(Rectangle)(d.dc, 10, 10, 20, 20);
    EXPECT_EQ(dp_cntc(&d, BLUE_C), 36);
    EXPECT_EQ(dp_cntc(&d, GREEN_C), 64);
    dp_close(&d);
}
TEST(dprim_rect_wide_pen_extent) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_SOLID, 5, BLACK_C); dp_stock(&d, NULL_BRUSH);
    U(Rectangle)(d.dc, 30, 30, 70, 60);
    RECT rc; int n = dp_ink(&d, &rc);
    t_note("5px pen Rectangle(30,30,70,60) bbox %ld,%ld,%ld,%ld ink=%d", rc.left, rc.top, rc.right, rc.bottom, n);
    CHECK(rc.left >= 26 && rc.left <= 30 && rc.top >= 26 && rc.top <= 30);
    CHECK(rc.right >= 70 && rc.right <= 74 && rc.bottom >= 60 && rc.bottom <= 64);
    CHECK(n > 2 * (40 + 30) * 4);
    dp_close(&d);
}
TEST(dprim_rect_repeated_stable) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    for (int i = 0; i < 500; i++) U(Rectangle)(d.dc, 10, 10, 40, 40);
    EXPECT_EQ(dp_cntc(&d, BLACK_C), 116);
    EXPECT_EQ(dp_cntc(&d, RED_C), 784);
    dp_close(&d);
}

/* ---------------- RoundRect ---------------- */
TEST(dprim_roundrect_inside_box) {
    DP d; DP_NEED(); REQUIRE(U(RoundRect));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_stock(&d, NULL_BRUSH);
    CHECK(U(RoundRect)(d.dc, 10, 15, 70, 55, 16, 12));
    RECT rc; int n = dp_ink(&d, &rc);
    CHECK(n > 100);
    CHECKF(rc.left == 10 && rc.top == 15 && rc.right <= 70 && rc.bottom <= 55, "%ld %ld %ld %ld", rc.left, rc.top, rc.right, rc.bottom);
    dp_close(&d);
}
TEST(dprim_roundrect_corners_cut) {
    DP d; DP_NEED(); REQUIRE(U(RoundRect));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    U(RoundRect)(d.dc, 10, 10, 70, 50, 20, 20);
    CHECK(dp_px(&d, 10, 10) == WHITE_C);
    CHECK(dp_px(&d, 69, 10) == WHITE_C);
    CHECK(dp_px(&d, 10, 49) == WHITE_C);
    CHECK(dp_px(&d, 69, 49) == WHITE_C);
    CHECK(dp_px(&d, 40, 10) == BLACK_C);
    CHECK(dp_px(&d, 10, 30) == BLACK_C);
    CHECK(dp_px(&d, 40, 30) == RED_C);
    dp_close(&d);
}
TEST(dprim_roundrect_area_vs_rect) {
    DP d; DP_NEED(); REQUIRE(U(RoundRect));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    U(RoundRect)(d.dc, 10, 10, 70, 50, 20, 20);
    int n = dp_cntc(&d, RED_C);
    double ex = 60.0 * 40.0 - (4.0 - 3.14159265) * 100.0;
    CHECKF(n > ex - 120 && n < ex + 160, "n=%d expect ~%.0f", n, ex);
    dp_close(&d);
}
TEST(dprim_roundrect_zero_ellipse_like_rectangle) {
    DP d; DP_NEED(); REQUIRE(U(RoundRect));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    U(Rectangle)(d.dc, 10, 10, 50, 40);
    BYTE *a = malloc(W * H); dp_snap(&d, a);
    dp_clr(&d);
    CHECK(U(RoundRect)(d.dc, 10, 10, 50, 40, 0, 0));
    int df = dp_diff(&d, a);
    t_note("RoundRect(0,0) vs Rectangle diff=%d", df);
    CHECK(df == 0);
    free(a);
    dp_close(&d);
}
TEST(dprim_roundrect_ellipse_dims_clamped) {
    DP d; DP_NEED(); REQUIRE(U(RoundRect));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_stock(&d, NULL_BRUSH);
    U(RoundRect)(d.dc, 10, 10, 50, 40, 40, 30);
    BYTE *a = malloc(W * H); dp_snap(&d, a);
    dp_clr(&d);
    U(RoundRect)(d.dc, 10, 10, 50, 40, 400, 300);
    int df = dp_diff(&d, a);
    t_note("RoundRect oversize ellipse vs exact size diff=%d", df);
    CHECK(df <= 4);
    dp_clr(&d);
    U(Ellipse)(d.dc, 10, 10, 50, 40);
    df = dp_diff(&d, a);
    t_note("RoundRect(w,h) vs Ellipse diff=%d", df);
    free(a);
    dp_close(&d);
}
TEST(dprim_roundrect_negative_ellipse_dims) {
    DP d; DP_NEED(); REQUIRE(U(RoundRect));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    BOOL r = U(RoundRect)(d.dc, 10, 10, 50, 40, -10, -10);
    t_note("RoundRect negative ellipse dims -> %d ink=%d", r, dp_ink(&d, NULL));
    r = U(RoundRect)(d.dc, 10, 10, 50, 40, -10, 10);
    t_note("RoundRect mixed ellipse dims -> %d", r);
    dp_close(&d);
}
TEST(dprim_roundrect_symmetric) {
    DP d; DP_NEED(); REQUIRE(U(RoundRect));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_stock(&d, NULL_BRUSH);
    U(RoundRect)(d.dc, 10, 10, 70, 50, 20, 20);
    int mx = dp_mirx(&d, 10, 10, 70, 50), my = dp_miry(&d, 10, 10, 70, 50);
    t_note("RoundRect mirror mismatch x=%d y=%d", mx, my);
    CHECK(mx < 10 && my < 10);
    dp_close(&d);
}
TEST(dprim_roundrect_does_not_move_current_pos) {
    DP d; DP_NEED(); REQUIRE(U(RoundRect));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 3, 4, NULL);
    U(RoundRect)(d.dc, 10, 10, 70, 50, 20, 20);
    POINT c = dp_cur(&d); EXPECT_EQ(c.x, 3); EXPECT_EQ(c.y, 4);
    dp_close(&d);
}
TEST(dprim_roundrect_invalid_dc) {
    DP_NEED(); REQUIRE(U(RoundRect));
    SetLastError(0);
    CHECK(!U(RoundRect)(NULL, 0, 0, 10, 10, 2, 2));
    SOFT_ERR(ERROR_INVALID_HANDLE);
    HDC dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    CHECK(!U(RoundRect)(dc, 0, 0, 10, 10, 2, 2));
}
TEST(dprim_roundrect_huge_no_crash) {
    DP d; DP_NEED(); NOT_WINE("huge coordinates corrupt wine heap"); REQUIRE(U(RoundRect));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    U(RoundRect)(d.dc, -(1 << 30), -(1 << 30), 1 << 30, 1 << 30, 20, 20);
    U(RoundRect)(d.dc, 0, 0, 50, 50, 1 << 30, 1 << 30);
    U(RoundRect)(d.dc, 0, 0, 50, 50, 0x7FFFFFFF, 0x7FFFFFFF);
    U(RoundRect)(d.dc, 0, 0, 50, 50, (int)0x80000000, (int)0x80000000);
    dp_close(&d);
}
TEST(dprim_roundrect_degenerate) {
    DP d; DP_NEED(); REQUIRE(U(RoundRect));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    BOOL r;
    r = U(RoundRect)(d.dc, 10, 10, 10, 10, 4, 4); t_note("RoundRect empty -> %d ink=%d", r, dp_ink(&d, NULL));
    r = U(RoundRect)(d.dc, 10, 10, 10, 40, 4, 4); t_note("RoundRect zero width -> %d ink=%d", r, dp_ink(&d, NULL));
    r = U(RoundRect)(d.dc, 10, 10, 40, 10, 4, 4); t_note("RoundRect zero height -> %d ink=%d", r, dp_ink(&d, NULL));
    dp_close(&d);
}

/* ---------------- Polygon ---------------- */
TEST(dprim_polygon_square_null_pen_exact) {
    DP d; DP_NEED(); REQUIRE(U(Polygon));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    POINT p[4] = { {10, 10}, {30, 10}, {30, 30}, {10, 30} };
    CHECK(U(Polygon)(d.dc, p, 4));
    RECT rc; int n = dp_bbox(&d, 0xFFFFFF, &rc);
    CHECKF(n == 400 && rc.left == 10 && rc.top == 10 && rc.right == 30 && rc.bottom == 30, "n=%d bbox %ld %ld %ld %ld", n, rc.left, rc.top, rc.right, rc.bottom);
    dp_close(&d);
}
TEST(dprim_polygon_square_pen_outline) {
    DP d; DP_NEED(); REQUIRE(U(Polygon));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    POINT p[4] = { {10, 10}, {30, 10}, {30, 30}, {10, 30} };
    CHECK(U(Polygon)(d.dc, p, 4));
    RECT rc; int n = dp_ink(&d, &rc);
    CHECK(rc.left == 10 && rc.top == 10);
    t_note("Polygon square with pen: bbox right=%ld bottom=%ld black=%d red=%d", rc.right, rc.bottom, dp_cntc(&d, BLACK_C), dp_cntc(&d, RED_C));
    CHECK(rc.right >= 30 && rc.right <= 31 && rc.bottom >= 30 && rc.bottom <= 31);
    CHECK(dp_px(&d, 20, 20) == RED_C);
    dp_close(&d);
}
TEST(dprim_polygon_triangle_area) {
    DP d; DP_NEED(); REQUIRE(U(Polygon));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    POINT p[3] = { {10, 10}, {50, 10}, {10, 50} };
    CHECK(U(Polygon)(d.dc, p, 3));
    int n = dp_cntc(&d, RED_C);
    CHECKF(n > 740 && n < 880, "n=%d (800)", n);
    CHECK(dp_px(&d, 15, 15) == RED_C);
    CHECK(dp_px(&d, 45, 45) == WHITE_C);
    dp_close(&d);
}
TEST(dprim_polygon_orientation_independent) {
    DP d; DP_NEED(); REQUIRE(U(Polygon));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    POINT p[3] = { {10, 10}, {50, 10}, {10, 50} }, q[3] = { {10, 10}, {10, 50}, {50, 10} };
    U(Polygon)(d.dc, p, 3);
    BYTE *a = malloc(W * H); dp_snap(&d, a);
    dp_clr(&d);
    U(Polygon)(d.dc, q, 3);
    EXPECT_EQ(dp_diff(&d, a), 0);
    free(a);
    dp_close(&d);
}
TEST(dprim_polygon_start_vertex_independent) {
    DP d; DP_NEED(); REQUIRE(U(Polygon));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    POINT p[4] = { {10, 10}, {50, 20}, {45, 50}, {15, 40} };
    POINT q[4] = { {45, 50}, {15, 40}, {10, 10}, {50, 20} };
    U(Polygon)(d.dc, p, 4);
    BYTE *a = malloc(W * H); dp_snap(&d, a);
    dp_clr(&d);
    U(Polygon)(d.dc, q, 4);
    EXPECT_EQ(dp_diff(&d, a), 0);
    free(a);
    dp_close(&d);
}
TEST(dprim_polygon_explicit_close_same_fill) {
    DP d; DP_NEED(); REQUIRE(U(Polygon));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    POINT p[4] = { {10, 10}, {50, 20}, {45, 50}, {15, 40} };
    POINT q[5] = { {10, 10}, {50, 20}, {45, 50}, {15, 40}, {10, 10} };
    U(Polygon)(d.dc, p, 4);
    BYTE *a = malloc(W * H); dp_snap(&d, a);
    dp_clr(&d);
    U(Polygon)(d.dc, q, 5);
    EXPECT_EQ(dp_diff(&d, a), 0);
    free(a);
    dp_close(&d);
}
TEST(dprim_polygon_invalid_counts) {
    DP d; DP_NEED(); REQUIRE(U(Polygon));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    POINT p[4] = { {10, 10}, {50, 20}, {45, 50}, {15, 40} };
    SetLastError(0);
    CHECK(!U(Polygon)(d.dc, p, 0));
    CHECK(!U(Polygon)(d.dc, p, 1));
    if (!dp_wine()) { BOOL r0 = U(Polygon)(d.dc, p, -1); SOFT(!r0); }
    EXPECT_EQ(dp_ink(&d, NULL), 0);
    dp_close(&d);
}
TEST(dprim_polygon_two_points) {
    DP d; DP_NEED(); REQUIRE(U(Polygon));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    POINT p[2] = { {10, 10}, {40, 10} };
    BOOL r = U(Polygon)(d.dc, p, 2);
    t_note("Polygon 2 points -> %d ink=%d", r, dp_ink(&d, NULL));
    CHECK(r);
    CHECK(dp_cntc(&d, RED_C) == 0);
    dp_close(&d);
}
TEST(dprim_polygon_degenerate_collinear) {
    DP d; DP_NEED(); REQUIRE(U(Polygon));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    POINT p[3] = { {10, 10}, {40, 10}, {70, 10} };
    BOOL r = U(Polygon)(d.dc, p, 3);
    CHECK(r);
    EXPECT_EQ(dp_ink(&d, NULL), 0);
    dp_close(&d);
}
TEST(dprim_polygon_invalid_dc) {
    DP_NEED(); REQUIRE(U(Polygon));
    POINT p[3] = { {10, 10}, {50, 20}, {45, 50} };
    SetLastError(0);
    CHECK(!U(Polygon)(NULL, p, 3));
    SOFT_ERR(ERROR_INVALID_HANDLE);
    HDC dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    CHECK(!U(Polygon)(dc, p, 3));
}
TEST(dprim_polygon_does_not_move_current_pos) {
    DP d; DP_NEED(); REQUIRE(U(Polygon));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 3, 4, NULL);
    POINT p[3] = { {10, 10}, {50, 20}, {45, 50} };
    U(Polygon)(d.dc, p, 3);
    POINT c = dp_cur(&d); EXPECT_EQ(c.x, 3); EXPECT_EQ(c.y, 4);
    dp_close(&d);
}
TEST(dprim_polygon_clipped_fill) {
    DP d; DP_NEED(); REQUIRE(U(Polygon));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    POINT p[4] = { {-100, -100}, {300, -100}, {300, 300}, {-100, 300} };
    CHECK(U(Polygon)(d.dc, p, 4));
    EXPECT_EQ(dp_cntc(&d, RED_C), W * H);
    dp_close(&d);
}
TEST(dprim_polygon_huge_coords) {
    DP d; DP_NEED(); NOT_WINE("huge coordinates corrupt wine heap"); REQUIRE(U(Polygon));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    POINT p[3] = { {-(1 << 30), -(1 << 30)}, {1 << 30, 0}, {0, 1 << 30} };
    BOOL r = U(Polygon)(d.dc, p, 3);
    t_note("Polygon 2^30 -> %d red=%d", r, dp_cntc(&d, RED_C));
    POINT q[3] = { {0x7FFFFFFF, 0x7FFFFFFF}, {(int)0x80000000, 0}, {0, (int)0x80000000} };
    r = U(Polygon)(d.dc, q, 3);
    dp_close(&d);
}
TEST(dprim_polygon_fill_mode_get_set) {
    DP d; DP_NEED(); REQUIRE(U(SetPolyFillMode)); REQUIRE(U(GetPolyFillMode));
    CHECK(dp_open(&d, W, H, 32));
    int r;
    r = U(GetPolyFillMode)(d.dc); EXPECT_EQ(r, ALTERNATE);
    r = U(SetPolyFillMode)(d.dc, WINDING); EXPECT_EQ(r, ALTERNATE);
    r = U(GetPolyFillMode)(d.dc); EXPECT_EQ(r, WINDING);
    r = U(SetPolyFillMode)(d.dc, ALTERNATE); EXPECT_EQ(r, WINDING);
    r = U(SetPolyFillMode)(d.dc, 0); EXPECT_EQ(r, 0);
    r = U(SetPolyFillMode)(d.dc, 3); EXPECT_EQ(r, 0);
    r = U(SetPolyFillMode)(d.dc, -1); EXPECT_EQ(r, 0);
    r = U(GetPolyFillMode)(d.dc); EXPECT_EQ(r, ALTERNATE);
    r = U(GetPolyFillMode)(NULL); EXPECT_EQ(r, 0);
    r = U(SetPolyFillMode)(NULL, WINDING); EXPECT_EQ(r, 0);
    dp_close(&d);
}
TEST(dprim_polygon_star_alternate_center_hole) {
    DP d; DP_NEED(); REQUIRE(U(Polygon)); REQUIRE(U(SetPolyFillMode));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    POINT p[5]; star(p, 50, 52, 45);
    U(SetPolyFillMode)(d.dc, ALTERNATE);
    CHECK(U(Polygon)(d.dc, p, 5));
    CHECK(dp_px(&d, 50, 52) == WHITE_C);
    CHECK(dp_cntc(&d, RED_C) > 500);
    dp_close(&d);
}
TEST(dprim_polygon_star_winding_center_filled) {
    DP d; DP_NEED(); REQUIRE(U(Polygon)); REQUIRE(U(SetPolyFillMode));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    POINT p[5]; star(p, 50, 52, 45);
    U(SetPolyFillMode)(d.dc, WINDING);
    CHECK(U(Polygon)(d.dc, p, 5));
    CHECK(dp_px(&d, 50, 52) == RED_C);
    dp_close(&d);
}
TEST(dprim_polygon_star_winding_larger_than_alternate) {
    DP d; DP_NEED(); REQUIRE(U(Polygon)); REQUIRE(U(SetPolyFillMode));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    POINT p[5]; star(p, 50, 52, 45);
    U(SetPolyFillMode)(d.dc, ALTERNATE); U(Polygon)(d.dc, p, 5);
    int a = dp_cntc(&d, RED_C);
    dp_clr(&d);
    U(SetPolyFillMode)(d.dc, WINDING); U(Polygon)(d.dc, p, 5);
    int w = dp_cntc(&d, RED_C);
    /* inner pentagon of a regular pentagram is ~ 0.1 of the filled area; exact ratio is rasteriser dependent */
    CHECKF(w > a + 150, "alternate %d winding %d", a, w);
    t_note("pentagram alternate=%d winding=%d", a, w);
    dp_close(&d);
}
TEST(dprim_polygon_star_fill_mode_reverse_orientation) {
    DP d; DP_NEED(); REQUIRE(U(Polygon)); REQUIRE(U(SetPolyFillMode));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    POINT p[5], q[5]; star(p, 50, 52, 45);
    for (int i = 0; i < 5; i++) q[i] = p[4 - i];
    U(SetPolyFillMode)(d.dc, WINDING);
    U(Polygon)(d.dc, p, 5);
    int a = dp_cntc(&d, RED_C);
    dp_clr(&d);
    U(Polygon)(d.dc, q, 5);
    int b = dp_cntc(&d, RED_C);
    CHECKF(a == b, "%d vs %d", a, b);
    dp_close(&d);
}
TEST(dprim_polygon_bowtie_modes) {
    DP d; DP_NEED(); REQUIRE(U(Polygon)); REQUIRE(U(SetPolyFillMode));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    POINT p[4] = { {10, 10}, {60, 60}, {60, 10}, {10, 60} };
    U(SetPolyFillMode)(d.dc, ALTERNATE); U(Polygon)(d.dc, p, 4);
    int a = dp_cntc(&d, RED_C);
    dp_clr(&d);
    U(SetPolyFillMode)(d.dc, WINDING); U(Polygon)(d.dc, p, 4);
    int w = dp_cntc(&d, RED_C);
    CHECKF(a == w, "bowtie alternate %d winding %d", a, w);
    CHECK(dp_px(&d, 35, 15) == WHITE_C);
    CHECK(dp_px(&d, 15, 35) == RED_C);
    CHECK(dp_px(&d, 55, 35) == RED_C);
    dp_close(&d);
}
TEST(dprim_polygon_nested_square_modes) {
    /* inner square drawn in the same direction: winding fills it, alternate leaves a hole */
    DP d; DP_NEED(); REQUIRE(U(Polygon)); REQUIRE(U(SetPolyFillMode));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    POINT p[10] = { {10, 10}, {70, 10}, {70, 70}, {10, 70}, {10, 10}, {30, 30}, {50, 30}, {50, 50}, {30, 50}, {30, 30} };
    U(SetPolyFillMode)(d.dc, ALTERNATE); U(Polygon)(d.dc, p, 10);
    int ca = dp_px(&d, 40, 40) == RED_C;
    dp_clr(&d);
    U(SetPolyFillMode)(d.dc, WINDING); U(Polygon)(d.dc, p, 10);
    int cw = dp_px(&d, 40, 40) == RED_C;
    CHECKF(!ca, "alternate center filled");
    CHECKF(cw, "winding center empty");
    dp_close(&d);
}

/* ---------------- PolyPolygon ---------------- */
TEST(dprim_polypolygon_two_squares) {
    DP d; DP_NEED(); REQUIRE(U(PolyPolygon));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    POINT p[8] = { {10, 10}, {30, 10}, {30, 30}, {10, 30}, {50, 50}, {80, 50}, {80, 70}, {50, 70} };
    INT c[2] = { 4, 4 };
    CHECK(U(PolyPolygon)(d.dc, p, c, 2));
    EXPECT_EQ(dp_cntc(&d, RED_C), 400 + 600);
    dp_close(&d);
}
TEST(dprim_polypolygon_overlap_alternate_hole) {
    DP d; DP_NEED(); REQUIRE(U(PolyPolygon)); REQUIRE(U(SetPolyFillMode));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    POINT p[8] = { {10, 10}, {50, 10}, {50, 50}, {10, 50}, {30, 30}, {70, 30}, {70, 70}, {30, 70} };
    INT c[2] = { 4, 4 };
    U(SetPolyFillMode)(d.dc, ALTERNATE);
    CHECK(U(PolyPolygon)(d.dc, p, c, 2));
    CHECK(dp_px(&d, 40, 40) == WHITE_C);
    CHECK(dp_px(&d, 20, 20) == RED_C);
    CHECK(dp_px(&d, 60, 60) == RED_C);
    EXPECT_EQ(dp_cntc(&d, RED_C), 2 * (1600 - 400));
    dp_close(&d);
}
TEST(dprim_polypolygon_overlap_winding_filled) {
    DP d; DP_NEED(); REQUIRE(U(PolyPolygon)); REQUIRE(U(SetPolyFillMode));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    POINT p[8] = { {10, 10}, {50, 10}, {50, 50}, {10, 50}, {30, 30}, {70, 30}, {70, 70}, {30, 70} };
    INT c[2] = { 4, 4 };
    U(SetPolyFillMode)(d.dc, WINDING);
    CHECK(U(PolyPolygon)(d.dc, p, c, 2));
    CHECK(dp_px(&d, 40, 40) == RED_C);
    EXPECT_EQ(dp_cntc(&d, RED_C), 2 * 1600 - 400);
    dp_close(&d);
}
TEST(dprim_polypolygon_invalid_counts) {
    DP d; DP_NEED(); REQUIRE(U(PolyPolygon));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    POINT p[8] = { {10, 10}, {30, 10}, {30, 30}, {10, 30}, {50, 50}, {80, 50}, {80, 70}, {50, 70} };
    INT c0[2] = { 4, 1 }, c1[1] = { 0 }, c2[2] = { 4, -4 };
    BOOL r;
    r = U(PolyPolygon)(d.dc, p, c0, 2); t_note("PolyPolygon with a 1-point polygon -> %d ink=%d", r, dp_ink(&d, NULL));
    dp_clr(&d);
    r = U(PolyPolygon)(d.dc, p, c1, 1); t_note("PolyPolygon with 0-point polygon -> %d", r);
    if (!dp_wine()) { r = U(PolyPolygon)(d.dc, p, c2, 2); t_note("PolyPolygon with negative count -> %d", r); }
    r = U(PolyPolygon)(d.dc, p, c0, 0); t_note("PolyPolygon cPoly=0 -> %d", r);
    if (!dp_wine()) { r = U(PolyPolygon)(d.dc, p, c0, -1); t_note("PolyPolygon cPoly=-1 -> %d", r); }
    dp_close(&d);
}
TEST(dprim_polypolygon_invalid_dc) {
    DP_NEED(); REQUIRE(U(PolyPolygon));
    POINT p[4] = { {10, 10}, {30, 10}, {30, 30}, {10, 30} };
    INT c[1] = { 4 };
    SetLastError(0);
    CHECK(!U(PolyPolygon)(NULL, p, c, 1));
    SOFT_ERR(ERROR_INVALID_HANDLE);
}
TEST(dprim_polypolygon_single_equals_polygon) {
    DP d; DP_NEED(); REQUIRE(U(PolyPolygon)); REQUIRE(U(Polygon));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    POINT p[4] = { {10, 10}, {50, 20}, {45, 50}, {15, 40} };
    INT c[1] = { 4 };
    U(Polygon)(d.dc, p, 4);
    BYTE *a = malloc(W * H); dp_snap(&d, a);
    dp_clr(&d);
    U(PolyPolygon)(d.dc, p, c, 1);
    EXPECT_EQ(dp_diff(&d, a), 0);
    free(a);
    dp_close(&d);
}
TEST(dprim_polypolygon_does_not_move_current_pos) {
    DP d; DP_NEED(); REQUIRE(U(PolyPolygon));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 3, 4, NULL);
    POINT p[4] = { {10, 10}, {30, 10}, {30, 30}, {10, 30} };
    INT c[1] = { 4 };
    U(PolyPolygon)(d.dc, p, c, 1);
    POINT cp = dp_cur(&d); EXPECT_EQ(cp.x, 3); EXPECT_EQ(cp.y, 4);
    dp_close(&d);
}
TEST(dprim_polypolygon_many_polygons) {
    DP d; DP_NEED(); REQUIRE(U(PolyPolygon));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    POINT p[40]; INT c[10];
    for (int i = 0; i < 10; i++) {
        int x = (i % 5) * 20 + 2, y = (i / 5) * 20 + 2;
        p[i * 4 + 0] = (POINT){ x, y }; p[i * 4 + 1] = (POINT){ x + 10, y };
        p[i * 4 + 2] = (POINT){ x + 10, y + 10 }; p[i * 4 + 3] = (POINT){ x, y + 10 };
        c[i] = 4;
    }
    CHECK(U(PolyPolygon)(d.dc, p, c, 10));
    EXPECT_EQ(dp_cntc(&d, RED_C), 1000);
    dp_close(&d);
}
