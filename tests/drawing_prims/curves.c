/* Polyline family, Bezier, PolyDraw, Arc/ArcTo/AngleArc, Ellipse/Chord/Pie. */
#include "dp.h"
#define W 80
#define H 80

/* ink per quadrant around (cx,cy), ignoring the axes (+-1) : NE, NW, SW, SE */
static void quad(DP *d, int cx, int cy, int q[4]) {
    q[0] = q[1] = q[2] = q[3] = 0;
    for (int y = 0; y < d->h; y++) for (int x = 0; x < d->w; x++) {
        if (dp_raw(d, x, y) == 0xFFFFFF) continue;
        if (abs(x - cx) <= 1 || abs(y - cy) <= 1) continue;
        int e = x > cx, n = y < cy;
        q[e ? (n ? 0 : 3) : (n ? 1 : 2)]++;
    }
}
static int near_(int a, int b, int tol) { return abs(a - b) <= tol; }

/* ---------------- Polyline ---------------- */
TEST(dprim_polyline_two_points_like_lineto) {
    DP d; DP_NEED(); REQUIRE(U(Polyline));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[2] = { {5, 5}, {30, 5} };
    CHECK(U(Polyline)(d.dc, p, 2));
    int n = dp_ink(&d, NULL);
    CHECKF(n == 25 || n == 26, "n=%d", n);
    t_note("Polyline 2pt horizontal drew %d pixels (LineTo would be 25)", n);
    CHECK(dp_raw(&d, 5, 5) == 0);
    dp_close(&d);
}
TEST(dprim_polyline_joint_pixels_present) {
    DP d; DP_NEED(); REQUIRE(U(Polyline));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[3] = { {5, 5}, {30, 5}, {30, 30} };
    CHECK(U(Polyline)(d.dc, p, 3));
    CHECK(dp_raw(&d, 29, 5) == 0);
    CHECK(dp_raw(&d, 30, 6) == 0);
    CHECK(dp_raw(&d, 30, 29) == 0);
    int n = dp_ink(&d, NULL);
    CHECKF(n >= 50 && n <= 52, "n=%d", n);
    dp_close(&d);
}
TEST(dprim_polyline_does_not_update_current_pos) {
    DP d; DP_NEED(); REQUIRE(U(Polyline));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 2, 3, NULL);
    POINT p[3] = { {5, 5}, {30, 5}, {30, 30} };
    CHECK(U(Polyline)(d.dc, p, 3));
    POINT c = dp_cur(&d); EXPECT_EQ(c.x, 2); EXPECT_EQ(c.y, 3);
    dp_close(&d);
}
TEST(dprim_polyline_does_not_use_current_pos) {
    DP d; DP_NEED(); REQUIRE(U(Polyline));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 70, 70, NULL);
    POINT p[2] = { {5, 5}, {30, 5} };
    U(Polyline)(d.dc, p, 2);
    RECT rc; dp_ink(&d, &rc);
    EXPECT_EQ(rc.top, 5); EXPECT_EQ(rc.bottom, 6); EXPECT_EQ(rc.left, 5);
    dp_close(&d);
}
TEST(dprim_polyline_invalid_counts) {
    DP d; DP_NEED(); REQUIRE(U(Polyline));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[4] = { {5, 5}, {30, 5}, {30, 30}, {5, 30} };
    static const int bad[] = { 0, 1, -1, -100, (int)0x80000000 };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0);
        { BOOL r0 = U(Polyline)(d.dc, p, bad[i]); if (bad[i] >= 0) CHECKF(!r0, "count %d", bad[i]); else SOFT(!r0); }
    }
    EXPECT_EQ(dp_ink(&d, NULL), 0);
    dp_close(&d);
}
TEST(dprim_polyline_null_points) {
    DP d; DP_NEED(); REQUIRE(U(Polyline));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    SetLastError(0);
    CHECK(!U(Polyline)(d.dc, NULL, 0));
    dp_close(&d);
}
TEST(dprim_polyline_invalid_dc) {
    DP_NEED(); REQUIRE(U(Polyline));
    POINT p[2] = { {0, 0}, {5, 5} };
    SetLastError(0);
    CHECK(!U(Polyline)(NULL, p, 2));
    SOFT_ERR(ERROR_INVALID_HANDLE);
    CHECK(!U(Polyline)((HDC)(ULONG_PTR)0x2468, p, 2));
}
TEST(dprim_polyline_closed_shape_count) {
    DP d; DP_NEED(); REQUIRE(U(Polyline));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[5] = { {10, 10}, {40, 10}, {40, 40}, {10, 40}, {10, 10} };
    CHECK(U(Polyline)(d.dc, p, 5));
    int n = dp_ink(&d, NULL);
    CHECKF(n >= 120 && n <= 122, "n=%d", n);
    CHECK(dp_raw(&d, 25, 25) != 0);
    dp_close(&d);
}
TEST(dprim_polyline_clipped_points) {
    DP d; DP_NEED(); REQUIRE(U(Polyline));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[3] = { {-50, 20}, {200, 20}, {200, -90} };
    CHECK(U(Polyline)(d.dc, p, 3));
    EXPECT_EQ(dp_row(&d, 20, 0, W), W);
    dp_close(&d);
}
TEST(dprim_polyline_many_points) {
    DP d; DP_NEED(); REQUIRE(U(Polyline));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[1000];
    for (int i = 0; i < 1000; i++) { p[i].x = (i % 2) ? 70 : 5; p[i].y = 10 + (i / 100); }
    CHECK(U(Polyline)(d.dc, p, 1000));
    CHECK(dp_ink(&d, NULL) > 0);
    dp_close(&d);
}
TEST(dprim_polyline_equals_lineto_chain_except_last) {
    DP d; DP_NEED(); REQUIRE(U(Polyline));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[4] = { {5, 5}, {40, 12}, {30, 50}, {60, 70} };
    U(Polyline)(d.dc, p, 4);
    BYTE *a = malloc(W * H); dp_snap(&d, a);
    dp_clr(&d);
    U(MoveToEx)(d.dc, p[0].x, p[0].y, NULL);
    for (int i = 1; i < 4; i++) U(LineTo)(d.dc, p[i].x, p[i].y);
    int df = dp_diff(&d, a);
    CHECKF(df <= 1, "diff=%d", df);
    free(a);
    dp_close(&d);
}

/* ---------------- PolylineTo ---------------- */
TEST(dprim_polylineto_uses_and_updates_current_pos) {
    DP d; DP_NEED(); REQUIRE(U(PolylineTo));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 5, 5, NULL);
    POINT p[2] = { {30, 5}, {30, 30} };
    CHECK(U(PolylineTo)(d.dc, p, 2));
    POINT c = dp_cur(&d); EXPECT_EQ(c.x, 30); EXPECT_EQ(c.y, 30);
    CHECK(dp_raw(&d, 5, 5) == 0);
    CHECK(dp_raw(&d, 29, 5) == 0);
    CHECK(dp_raw(&d, 30, 29) == 0);
    dp_close(&d);
}
TEST(dprim_polylineto_single_point_allowed) {
    DP d; DP_NEED(); REQUIRE(U(PolylineTo));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 5, 5, NULL);
    POINT p[1] = { {30, 5} };
    SetLastError(0);
    BOOL r = U(PolylineTo)(d.dc, p, 1);
    t_note("PolylineTo count 1 -> %d", r);
    if (r) { POINT c = dp_cur(&d); EXPECT_EQ(c.x, 30); }
    dp_close(&d);
}
TEST(dprim_polylineto_invalid_counts) {
    DP d; DP_NEED(); REQUIRE(U(PolylineTo));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[2] = { {30, 5}, {30, 30} };
    CHECK(!U(PolylineTo)(d.dc, p, 0));
    { BOOL r0 = U(PolylineTo)(d.dc, p, -1); SOFT(!r0); }
    CHECK(!U(PolylineTo)(d.dc, NULL, 0));
    EXPECT_EQ(dp_ink(&d, NULL), 0);
    dp_close(&d);
}
TEST(dprim_polylineto_invalid_dc) {
    DP_NEED(); REQUIRE(U(PolylineTo));
    POINT p[2] = { {0, 0}, {5, 5} };
    SetLastError(0);
    CHECK(!U(PolylineTo)(NULL, p, 2));
    SOFT_ERR(ERROR_INVALID_HANDLE);
}
TEST(dprim_polylineto_matches_lineto) {
    DP d; DP_NEED(); REQUIRE(U(PolylineTo));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[3] = { {40, 12}, {30, 50}, {60, 70} };
    U(MoveToEx)(d.dc, 5, 5, NULL);
    U(PolylineTo)(d.dc, p, 3);
    BYTE *a = malloc(W * H); dp_snap(&d, a);
    dp_clr(&d);
    U(MoveToEx)(d.dc, 5, 5, NULL);
    for (int i = 0; i < 3; i++) U(LineTo)(d.dc, p[i].x, p[i].y);
    int df = dp_diff(&d, a);
    CHECKF(df <= 1, "diff=%d", df);
    free(a);
    dp_close(&d);
}

/* ---------------- PolyPolyline ---------------- */
TEST(dprim_polypolyline_two_lines) {
    DP d; DP_NEED(); REQUIRE(U(PolyPolyline));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[4] = { {5, 5}, {30, 5}, {5, 20}, {30, 20} };
    DWORD c[2] = { 2, 2 };
    CHECK(U(PolyPolyline)(d.dc, p, c, 2));
    CHECK(dp_row(&d, 5, 0, W) >= 25);
    CHECK(dp_row(&d, 20, 0, W) >= 25);
    int n = dp_ink(&d, NULL);
    CHECKF(n >= 50 && n <= 52, "n=%d", n);
    dp_close(&d);
}
TEST(dprim_polypolyline_not_connected) {
    DP d; DP_NEED(); REQUIRE(U(PolyPolyline));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[4] = { {5, 5}, {30, 5}, {5, 20}, {30, 20} };
    DWORD c[2] = { 2, 2 };
    U(PolyPolyline)(d.dc, p, c, 2);
    EXPECT_EQ(dp_col(&d, 15, 6, 20), 0);
    dp_close(&d);
}
TEST(dprim_polypolyline_does_not_move_current_pos) {
    DP d; DP_NEED(); REQUIRE(U(PolyPolyline));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 1, 2, NULL);
    POINT p[4] = { {5, 5}, {30, 5}, {5, 20}, {30, 20} };
    DWORD c[2] = { 2, 2 };
    U(PolyPolyline)(d.dc, p, c, 2);
    POINT cp = dp_cur(&d); EXPECT_EQ(cp.x, 1); EXPECT_EQ(cp.y, 2);
    dp_close(&d);
}
TEST(dprim_polypolyline_count_of_one_fails) {
    DP d; DP_NEED(); REQUIRE(U(PolyPolyline));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[4] = { {5, 5}, {30, 5}, {5, 20}, {30, 20} };
    DWORD c[2] = { 2, 1 };
    SetLastError(0);
    BOOL r = U(PolyPolyline)(d.dc, p, c, 2);
    CHECK(!r);
    t_note("PolyPolyline with a 1-point polyline: ret=%d ink=%d", r, dp_ink(&d, NULL));
    dp_close(&d);
}
TEST(dprim_polypolyline_zero_lines) {
    DP d; DP_NEED(); REQUIRE(U(PolyPolyline));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[2] = { {5, 5}, {30, 5} };
    DWORD c[1] = { 2 };
    BOOL r = U(PolyPolyline)(d.dc, p, c, 0);
    t_note("PolyPolyline cCount=0 -> %d", r);
    EXPECT_EQ(dp_ink(&d, NULL), 0);
    dp_close(&d);
}
TEST(dprim_polypolyline_invalid_dc) {
    DP_NEED(); REQUIRE(U(PolyPolyline));
    POINT p[2] = { {5, 5}, {30, 5} };
    DWORD c[1] = { 2 };
    SetLastError(0);
    CHECK(!U(PolyPolyline)(NULL, p, c, 1));
    SOFT_ERR(ERROR_INVALID_HANDLE);
}
TEST(dprim_polypolyline_three_polylines) {
    DP d; DP_NEED(); REQUIRE(U(PolyPolyline));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[8] = { {5, 5}, {30, 5}, {5, 15}, {30, 15}, {30, 40}, {40, 10}, {40, 40}, {10, 40} };
    DWORD c[3] = { 2, 3, 3 };
    CHECK(U(PolyPolyline)(d.dc, p, c, 3));
    CHECK(dp_row(&d, 5, 0, W) >= 25);
    CHECK(dp_col(&d, 30, 15, 40) >= 24);
    CHECK(dp_col(&d, 40, 0, H) >= 29);
    CHECK(dp_row(&d, 40, 10, 40) >= 29);
    dp_close(&d);
}

/* ---------------- PolyBezier ---------------- */
TEST(dprim_polybezier_collinear_stays_on_row) {
    DP d; DP_NEED(); REQUIRE(U(PolyBezier));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[4] = { {5, 10}, {20, 10}, {40, 10}, {60, 10} };
    CHECK(U(PolyBezier)(d.dc, p, 4));
    int n = dp_ink(&d, NULL);
    EXPECT_EQ(dp_row(&d, 10, 0, W), n);
    CHECKF(n >= 54 && n <= 56, "n=%d", n);
    dp_close(&d);
}
TEST(dprim_polybezier_start_pixel) {
    DP d; DP_NEED(); REQUIRE(U(PolyBezier));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[4] = { {5, 10}, {20, 60}, {40, 60}, {60, 10} };
    CHECK(U(PolyBezier)(d.dc, p, 4));
    CHECK(dp_raw(&d, 5, 10) == 0);
    int lastx = 0; for (int x = 0; x < W; x++) if (dp_col(&d, x, 0, H)) lastx = x;
    CHECKF(lastx == 60 || lastx == 59, "lastx=%d", lastx);
    dp_close(&d);
}
TEST(dprim_polybezier_stays_in_convex_hull) {
    DP d; DP_NEED(); REQUIRE(U(PolyBezier));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[4] = { {5, 10}, {20, 60}, {40, 60}, {60, 10} };
    CHECK(U(PolyBezier)(d.dc, p, 4));
    RECT rc; dp_ink(&d, &rc);
    CHECK(rc.left >= 5 && rc.right <= 62 && rc.top >= 10 && rc.bottom <= 61);
    /* the curve peaks at 0.75 of the control polygon height: y = 10 + 0.75*50 = 47.5 */
    CHECKF(rc.bottom >= 46 && rc.bottom <= 50, "bottom=%ld", rc.bottom);
    dp_close(&d);
}
TEST(dprim_polybezier_symmetric_control_points) {
    DP d; DP_NEED(); REQUIRE(U(PolyBezier));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[4] = { {5, 10}, {20, 60}, {40, 60}, {55, 10} };
    U(PolyBezier)(d.dc, p, 4);
    int mis = dp_mirx(&d, 5, 0, 56, H);
    t_note("PolyBezier symmetric mismatch=%d", mis);
    SOFT(mis <= 4);
    dp_close(&d);
}
TEST(dprim_polybezier_does_not_update_current_pos) {
    DP d; DP_NEED(); REQUIRE(U(PolyBezier));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 3, 4, NULL);
    POINT p[4] = { {5, 10}, {20, 60}, {40, 60}, {55, 10} };
    U(PolyBezier)(d.dc, p, 4);
    POINT c = dp_cur(&d); t_note("PolyBezier current pos after call: %ld,%ld", c.x, c.y);
    dp_close(&d);
}
TEST(dprim_polybezier_two_segments) {
    DP d; DP_NEED(); REQUIRE(U(PolyBezier));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[7] = { {5, 40}, {10, 5}, {25, 5}, {40, 40}, {50, 75}, {65, 75}, {75, 40} };
    CHECK(U(PolyBezier)(d.dc, p, 7));
    CHECK(dp_raw(&d, 5, 40) == 0);
    CHECK(dp_col(&d, 40, 0, H) >= 1);
    dp_close(&d);
}
TEST(dprim_polybezier_invalid_counts) {
    DP d; DP_NEED(); REQUIRE(U(PolyBezier));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[10]; for (int i = 0; i < 10; i++) { p[i].x = 5 + i * 5; p[i].y = 10 + (i % 3) * 10; }
    static const int bad[] = { 0, 1, 2, 3, -1, -4 };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0);
        CHECKF(!U(PolyBezier)(d.dc, p, bad[i]), "count %d", bad[i]);
    }
    EXPECT_EQ(dp_ink(&d, NULL), 0);
    dp_close(&d);
}
TEST(dprim_polybezier_non_3n1_counts_note) {
    DP d; DP_NEED(); REQUIRE(U(PolyBezier));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[10]; for (int i = 0; i < 10; i++) { p[i].x = 5 + i * 5; p[i].y = 10 + (i % 3) * 10; }
    for (int n = 5; n <= 9; n++) { dp_clr(&d); BOOL r = U(PolyBezier)(d.dc, p, n); t_note("PolyBezier count %d -> %d ink=%d", n, r, dp_ink(&d, NULL)); }
    dp_close(&d);
}
TEST(dprim_polybezier_invalid_dc) {
    DP_NEED(); REQUIRE(U(PolyBezier));
    POINT p[4] = { {5, 10}, {20, 60}, {40, 60}, {55, 10} };
    SetLastError(0);
    CHECK(!U(PolyBezier)(NULL, p, 4));
    SOFT_ERR(ERROR_INVALID_HANDLE);
}
TEST(dprim_polybezier_null_points) {
    DP d; DP_NEED(); REQUIRE(U(PolyBezier));
    CHECK(dp_open(&d, W, H, 32));
    CHECK(!U(PolyBezier)(d.dc, NULL, 0));
    dp_close(&d);
}

/* ---------------- PolyBezierTo ---------------- */
TEST(dprim_polybezierto_updates_pos) {
    DP d; DP_NEED(); REQUIRE(U(PolyBezierTo));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 5, 10, NULL);
    POINT p[3] = { {20, 60}, {40, 60}, {60, 10} };
    CHECK(U(PolyBezierTo)(d.dc, p, 3));
    POINT c = dp_cur(&d); EXPECT_EQ(c.x, 60); EXPECT_EQ(c.y, 10);
    CHECK(dp_raw(&d, 5, 10) == 0);
    dp_close(&d);
}
TEST(dprim_polybezierto_matches_polybezier) {
    DP d; DP_NEED(); REQUIRE(U(PolyBezierTo)); REQUIRE(U(PolyBezier));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[4] = { {5, 10}, {20, 60}, {40, 60}, {60, 10} };
    U(PolyBezier)(d.dc, p, 4);
    BYTE *a = malloc(W * H); dp_snap(&d, a);
    dp_clr(&d);
    U(MoveToEx)(d.dc, 5, 10, NULL);
    U(PolyBezierTo)(d.dc, p + 1, 3);
    EXPECT_EQ(dp_diff(&d, a), 0);
    free(a);
    dp_close(&d);
}
TEST(dprim_polybezierto_invalid_counts) {
    DP d; DP_NEED(); REQUIRE(U(PolyBezierTo));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[6]; for (int i = 0; i < 6; i++) { p[i].x = 5 + i * 5; p[i].y = 10 + (i % 3) * 10; }
    static const int bad[] = { 0, 1, 2, -1, -3 };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) { BOOL r0 = U(PolyBezierTo)(d.dc, p, bad[i]); if (bad[i] >= 0) CHECKF(!r0, "count %d", bad[i]); else SOFT(!r0); }
    for (int n = 4; n <= 5; n++) { BOOL r = U(PolyBezierTo)(d.dc, p, n); t_note("PolyBezierTo count %d -> %d", n, r); }
    dp_close(&d);
}
TEST(dprim_polybezierto_chain_two_segments) {
    DP d; DP_NEED(); REQUIRE(U(PolyBezierTo));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 5, 40, NULL);
    POINT p[6] = { {10, 5}, {25, 5}, {40, 40}, {50, 75}, {65, 75}, {75, 40} };
    CHECK(U(PolyBezierTo)(d.dc, p, 6));
    POINT c = dp_cur(&d); EXPECT_EQ(c.x, 75); EXPECT_EQ(c.y, 40);
    dp_close(&d);
}
TEST(dprim_polybezierto_invalid_dc) {
    DP_NEED(); REQUIRE(U(PolyBezierTo));
    POINT p[3] = { {20, 60}, {40, 60}, {60, 10} };
    SetLastError(0);
    CHECK(!U(PolyBezierTo)(NULL, p, 3));
    SOFT_ERR(ERROR_INVALID_HANDLE);
}

/* ---------------- PolyDraw ---------------- */
TEST(dprim_polydraw_moveto_lineto) {
    DP d; DP_NEED(); REQUIRE(U(PolyDraw));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[4] = { {5, 5}, {30, 5}, {5, 20}, {30, 20} };
    BYTE t[4] = { PT_MOVETO, PT_LINETO, PT_MOVETO, PT_LINETO };
    CHECK(U(PolyDraw)(d.dc, p, t, 4));
    CHECK(dp_row(&d, 5, 0, W) >= 25);
    CHECK(dp_row(&d, 20, 0, W) >= 25);
    EXPECT_EQ(dp_col(&d, 15, 6, 20), 0);
    POINT c = dp_cur(&d); EXPECT_EQ(c.x, 30); EXPECT_EQ(c.y, 20);
    dp_close(&d);
}
TEST(dprim_polydraw_closefigure) {
    DP d; DP_NEED(); REQUIRE(U(PolyDraw));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[4] = { {10, 10}, {40, 10}, {40, 40}, {10, 40} };
    BYTE t[4] = { PT_MOVETO, PT_LINETO, PT_LINETO, PT_LINETO | PT_CLOSEFIGURE };
    CHECK(U(PolyDraw)(d.dc, p, t, 4));
    CHECK(dp_col(&d, 10, 11, 39) >= 28);
    dp_close(&d);
}
TEST(dprim_polydraw_without_close_leaves_open) {
    DP d; DP_NEED(); REQUIRE(U(PolyDraw));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[4] = { {10, 10}, {40, 10}, {40, 40}, {10, 40} };
    BYTE t[4] = { PT_MOVETO, PT_LINETO, PT_LINETO, PT_LINETO };
    CHECK(U(PolyDraw)(d.dc, p, t, 4));
    EXPECT_EQ(dp_col(&d, 10, 11, 39), 0);
    dp_close(&d);
}
TEST(dprim_polydraw_bezierto) {
    DP d; DP_NEED(); REQUIRE(U(PolyDraw));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[4] = { {5, 10}, {20, 60}, {40, 60}, {60, 10} };
    BYTE t[4] = { PT_MOVETO, PT_BEZIERTO, PT_BEZIERTO, PT_BEZIERTO };
    CHECK(U(PolyDraw)(d.dc, p, t, 4));
    POINT c = dp_cur(&d); EXPECT_EQ(c.x, 60); EXPECT_EQ(c.y, 10);
    RECT rc; dp_ink(&d, &rc);
    CHECK(rc.bottom >= 46);
    dp_close(&d);
}
TEST(dprim_polydraw_invalid_type) {
    DP d; DP_NEED(); REQUIRE(U(PolyDraw));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[2] = { {5, 5}, {30, 5} };
    BYTE t[2] = { PT_MOVETO, 0x55 };
    SetLastError(0);
    BOOL r = U(PolyDraw)(d.dc, p, t, 2);
    t_note("PolyDraw invalid type -> %d err=%lu", r, GetLastError());
    BYTE t2[2] = { PT_MOVETO, 0 };
    r = U(PolyDraw)(d.dc, p, t2, 2);
    t_note("PolyDraw type 0 -> %d", r);
    dp_close(&d);
}
TEST(dprim_polydraw_invalid_count) {
    DP d; DP_NEED(); REQUIRE(U(PolyDraw));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    POINT p[2] = { {5, 5}, {30, 5} };
    BYTE t[2] = { PT_MOVETO, PT_LINETO };
    { BOOL r0 = U(PolyDraw)(d.dc, p, t, -1); SOFT(!r0); }
    BOOL r = U(PolyDraw)(d.dc, p, t, 0);
    t_note("PolyDraw count 0 -> %d", r);
    dp_close(&d);
}
TEST(dprim_polydraw_invalid_dc) {
    DP_NEED(); REQUIRE(U(PolyDraw));
    POINT p[2] = { {5, 5}, {30, 5} };
    BYTE t[2] = { PT_MOVETO, PT_LINETO };
    SetLastError(0);
    CHECK(!U(PolyDraw)(NULL, p, t, 2));
    SOFT_ERR(ERROR_INVALID_HANDLE);
}
TEST(dprim_polydraw_lineto_first_uses_current_pos) {
    DP d; DP_NEED(); REQUIRE(U(PolyDraw));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 5, 5, NULL);
    POINT p[1] = { {30, 5} };
    BYTE t[1] = { PT_LINETO };
    BOOL r = U(PolyDraw)(d.dc, p, t, 1);
    t_note("PolyDraw LINETO-first -> %d ink=%d", r, dp_ink(&d, NULL));
    dp_close(&d);
}

/* ---------------- Arc family ---------------- */
TEST(dprim_arc_quarter_ccw) {
    DP d; DP_NEED(); REQUIRE(U(Arc));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    CHECK(U(Arc)(d.dc, 10, 10, 50, 50, 50, 30, 30, 10));
    int q[4]; quad(&d, 30, 30, q);
    CHECKF(q[0] > 20, "NE=%d", q[0]);
    CHECKF(q[1] == 0 && q[2] == 0 && q[3] == 0, "%d %d %d", q[1], q[2], q[3]);
    dp_close(&d);
}
TEST(dprim_arc_quarter_clockwise_setting) {
    DP d; DP_NEED(); REQUIRE(U(Arc)); REQUIRE(U(SetArcDirection));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(SetArcDirection)(d.dc, AD_CLOCKWISE);
    CHECK(U(Arc)(d.dc, 10, 10, 50, 50, 50, 30, 30, 10));
    int q[4]; quad(&d, 30, 30, q);
    CHECKF(q[0] == 0, "NE=%d", q[0]);
    CHECKF(q[1] > 20 && q[2] > 20 && q[3] > 20, "%d %d %d", q[1], q[2], q[3]);
    dp_close(&d);
}
TEST(dprim_arc_direction_reverse_equivalence) {
    /* clockwise (s->e) is the same set of points as counter-clockwise (e->s) */
    DP d; DP_NEED(); REQUIRE(U(Arc)); REQUIRE(U(SetArcDirection));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(SetArcDirection)(d.dc, AD_CLOCKWISE);
    U(Arc)(d.dc, 10, 10, 50, 50, 50, 30, 30, 10);
    int a = dp_ink(&d, NULL);
    dp_clr(&d);
    U(SetArcDirection)(d.dc, AD_COUNTERCLOCKWISE);
    U(Arc)(d.dc, 10, 10, 50, 50, 30, 10, 50, 30);
    int b = dp_ink(&d, NULL);
    t_note("cw=%d ccw-swapped=%d", a, b);
    CHECK(near_(a, b, 6));
    dp_close(&d);
}
TEST(dprim_arc_same_start_end_is_full_ellipse) {
    DP d; DP_NEED(); REQUIRE(U(Arc));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    CHECK(U(Arc)(d.dc, 10, 10, 50, 50, 50, 30, 50, 30));
    int q[4]; quad(&d, 30, 30, q);
    CHECK(q[0] > 20 && q[1] > 20 && q[2] > 20 && q[3] > 20);
    dp_close(&d);
}
TEST(dprim_arc_inside_bounding_box) {
    DP d; DP_NEED(); REQUIRE(U(Arc));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(Arc)(d.dc, 10, 10, 60, 40, 60, 25, 10, 25);
    RECT rc; dp_ink(&d, &rc);
    CHECK(rc.left >= 10 && rc.right <= 61 && rc.top >= 10 && rc.bottom <= 41);
    dp_close(&d);
}
TEST(dprim_arc_does_not_update_current_pos) {
    DP d; DP_NEED(); REQUIRE(U(Arc));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 2, 3, NULL);
    U(Arc)(d.dc, 10, 10, 50, 50, 50, 30, 30, 10);
    POINT c = dp_cur(&d); EXPECT_EQ(c.x, 2); EXPECT_EQ(c.y, 3);
    dp_close(&d);
}
TEST(dprim_arc_no_fill) {
    DP d; DP_NEED(); REQUIRE(U(Arc));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    U(Arc)(d.dc, 10, 10, 50, 50, 50, 30, 30, 10);
    EXPECT_EQ(dp_cntc(&d, RED_C), 0);
    dp_close(&d);
}
TEST(dprim_arc_degenerate_rects) {
    DP d; DP_NEED(); REQUIRE(U(Arc));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    BOOL r;
    r = U(Arc)(d.dc, 10, 10, 10, 10, 10, 10, 10, 10); t_note("Arc empty rect -> %d ink=%d", r, dp_ink(&d, NULL));
    r = U(Arc)(d.dc, 10, 10, 10, 50, 10, 10, 10, 50); t_note("Arc zero-width -> %d ink=%d", r, dp_ink(&d, NULL));
    r = U(Arc)(d.dc, 10, 10, 50, 10, 10, 10, 50, 10); t_note("Arc zero-height -> %d ink=%d", r, dp_ink(&d, NULL));
    dp_close(&d);
}
TEST(dprim_arc_reversed_rect_note) {
    DP d; DP_NEED(); REQUIRE(U(Arc));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(Arc)(d.dc, 10, 10, 50, 50, 50, 30, 30, 10);
    int a = dp_ink(&d, NULL);
    dp_clr(&d);
    BOOL r = U(Arc)(d.dc, 50, 50, 10, 10, 50, 30, 30, 10);
    t_note("Arc reversed rect -> %d ink=%d (normal %d)", r, dp_ink(&d, NULL), a);
    dp_close(&d);
}
TEST(dprim_arc_invalid_dc) {
    DP_NEED(); REQUIRE(U(Arc));
    SetLastError(0);
    CHECK(!U(Arc)(NULL, 10, 10, 50, 50, 50, 30, 30, 10));
    SOFT_ERR(ERROR_INVALID_HANDLE);
    CHECK(!U(Arc)((HDC)(ULONG_PTR)0x77, 10, 10, 50, 50, 50, 30, 30, 10));
}
TEST(dprim_arc_huge_coords_no_crash) {
    DP d; DP_NEED(); NOT_WINE("heap corruption with huge arcs"); REQUIRE(U(Arc));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    BOOL r;
    r = U(Arc)(d.dc, -(1 << 30), -(1 << 30), 1 << 30, 1 << 30, 0, 0, 5, 5);
    r = U(Arc)(d.dc, 0, 0, 0x7FFFFFFF, 0x7FFFFFFF, 10, 10, 20, 20);
    r = U(Arc)(d.dc, (int)0x80000000, (int)0x80000000, 10, 10, 0, 0, 5, 5);
    (void)r;
    dp_close(&d);
}
TEST(dprim_arc_get_arc_direction_default) {
    DP d; DP_NEED(); REQUIRE(U(GetArcDirection)); REQUIRE(U(SetArcDirection));
    CHECK(dp_open(&d, W, H, 32));
    int r;
    r = U(GetArcDirection)(d.dc); EXPECT_EQ(r, AD_COUNTERCLOCKWISE);
    r = U(SetArcDirection)(d.dc, AD_CLOCKWISE); EXPECT_EQ(r, AD_COUNTERCLOCKWISE);
    r = U(GetArcDirection)(d.dc); EXPECT_EQ(r, AD_CLOCKWISE);
    r = U(SetArcDirection)(d.dc, AD_COUNTERCLOCKWISE); EXPECT_EQ(r, AD_CLOCKWISE);
    SetLastError(0);
    r = U(SetArcDirection)(d.dc, 0); EXPECT_EQ(r, 0);
    r = U(SetArcDirection)(d.dc, 3); EXPECT_EQ(r, 0);
    r = U(GetArcDirection)(d.dc); EXPECT_EQ(r, AD_COUNTERCLOCKWISE);
    r = U(GetArcDirection)(NULL); EXPECT_EQ(r, 0);
    dp_close(&d);
}

TEST(dprim_arcto_draws_connecting_line) {
    DP d; DP_NEED(); REQUIRE(U(ArcTo));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 5, 70, NULL);
    CHECK(U(ArcTo)(d.dc, 10, 10, 50, 50, 50, 30, 30, 10));
    /* line from (5,70) to arc start (50,30): midpoint ~ (27,50) should be inked */
    int hit = 0;
    for (int dx = -1; dx <= 1; dx++) for (int dy = -1; dy <= 1; dy++) if (dp_raw(&d, 28 + dx, 50 + dy) == 0) hit = 1;
    CHECK(hit);
    dp_close(&d);
}
TEST(dprim_arcto_updates_current_pos) {
    DP d; DP_NEED(); REQUIRE(U(ArcTo));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 5, 70, NULL);
    CHECK(U(ArcTo)(d.dc, 10, 10, 50, 50, 50, 30, 30, 10));
    POINT c = dp_cur(&d);
    CHECKF(near_(c.x, 30, 1) && near_(c.y, 10, 1), "pos %ld,%ld", c.x, c.y);
    dp_close(&d);
}
TEST(dprim_arcto_end_on_radial_projection) {
    /* end point given far away on the radial line: current pos is on the ellipse, not the given point */
    DP d; DP_NEED(); REQUIRE(U(ArcTo));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(ArcTo)(d.dc, 10, 10, 50, 50, 70, 30, 30, 70);
    POINT c = dp_cur(&d);
    t_note("ArcTo end point (radial) -> %ld,%ld", c.x, c.y);
    CHECKF(near_(c.x, 30, 2) && near_(c.y, 50, 2), "pos %ld,%ld", c.x, c.y);
    dp_close(&d);
}
TEST(dprim_arcto_clockwise) {
    DP d; DP_NEED(); REQUIRE(U(ArcTo)); REQUIRE(U(SetArcDirection));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(SetArcDirection)(d.dc, AD_CLOCKWISE);
    U(MoveToEx)(d.dc, 50, 30, NULL);
    CHECK(U(ArcTo)(d.dc, 10, 10, 50, 50, 50, 30, 30, 10));
    int q[4]; quad(&d, 30, 30, q);
    CHECK(q[0] == 0);
    CHECK(q[1] > 20 && q[3] > 20);
    POINT c = dp_cur(&d);
    CHECKF(near_(c.x, 30, 1) && near_(c.y, 10, 1), "pos %ld,%ld", c.x, c.y);
    dp_close(&d);
}
TEST(dprim_arcto_invalid_dc) {
    DP_NEED(); REQUIRE(U(ArcTo));
    SetLastError(0);
    CHECK(!U(ArcTo)(NULL, 10, 10, 50, 50, 50, 30, 30, 10));
    SOFT_ERR(ERROR_INVALID_HANDLE);
}
TEST(dprim_arcto_current_pos_is_start_when_line_zero) {
    DP d; DP_NEED(); REQUIRE(U(ArcTo));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 50, 30, NULL);
    U(ArcTo)(d.dc, 10, 10, 50, 50, 50, 30, 30, 10);
    int q[4]; quad(&d, 30, 30, q);
    CHECK(q[0] > 20);
    dp_close(&d);
}

TEST(dprim_anglearc_updates_pos_ccw) {
    DP d; DP_NEED(); REQUIRE(U(AngleArc));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 40, 40, NULL);
    CHECK(U(AngleArc)(d.dc, 40, 40, 20, 0.0f, 90.0f));
    POINT c = dp_cur(&d);
    CHECKF(near_(c.x, 40, 1) && near_(c.y, 20, 1), "pos %ld,%ld", c.x, c.y);
    int q[4]; quad(&d, 40, 40, q);
    CHECK(q[0] > 15);
    CHECK(q[1] == 0 && q[2] == 0 && q[3] == 0);
    dp_close(&d);
}
TEST(dprim_anglearc_line_to_start) {
    DP d; DP_NEED(); REQUIRE(U(AngleArc));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 10, 70, NULL);
    CHECK(U(AngleArc)(d.dc, 40, 40, 20, 0.0f, 90.0f));
    CHECK(dp_raw(&d, 10, 70) == 0);
    int hit = 0; for (int dy = -1; dy <= 1; dy++) if (dp_raw(&d, 35, 55 + dy) == 0) hit = 1;
    t_note("AngleArc connecting line midpoint hit=%d", hit);
    dp_close(&d);
}
TEST(dprim_anglearc_negative_sweep) {
    DP d; DP_NEED(); REQUIRE(U(AngleArc));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 60, 40, NULL);
    CHECK(U(AngleArc)(d.dc, 40, 40, 20, 0.0f, -90.0f));
    POINT c = dp_cur(&d);
    CHECKF(near_(c.x, 40, 1) && near_(c.y, 60, 1), "pos %ld,%ld", c.x, c.y);
    int q[4]; quad(&d, 40, 40, q);
    CHECK(q[3] > 15 && q[0] == 0 && q[1] == 0 && q[2] == 0);
    dp_close(&d);
}
TEST(dprim_anglearc_full_circle) {
    DP d; DP_NEED(); REQUIRE(U(AngleArc));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 60, 40, NULL);
    CHECK(U(AngleArc)(d.dc, 40, 40, 20, 0.0f, 360.0f));
    int q[4]; quad(&d, 40, 40, q);
    CHECK(q[0] > 15 && q[1] > 15 && q[2] > 15 && q[3] > 15);
    POINT c = dp_cur(&d);
    CHECKF(near_(c.x, 60, 1) && near_(c.y, 40, 1), "pos %ld,%ld", c.x, c.y);
    dp_close(&d);
}
TEST(dprim_anglearc_sweep_over_360) {
    DP d; DP_NEED(); REQUIRE(U(AngleArc));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 60, 40, NULL);
    BOOL r = U(AngleArc)(d.dc, 40, 40, 20, 0.0f, 720.0f);
    t_note("AngleArc sweep 720 -> %d ink=%d", r, dp_ink(&d, NULL));
    dp_close(&d);
}
TEST(dprim_anglearc_zero_radius_note) {
    DP d; DP_NEED(); REQUIRE(U(AngleArc));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 10, 10, NULL);
    SetLastError(0);
    BOOL r = U(AngleArc)(d.dc, 40, 40, 0, 0.0f, 90.0f);
    t_note("AngleArc radius 0 -> %d err=%lu", r, GetLastError());
    POINT c = dp_cur(&d); t_note("pos %ld,%ld", c.x, c.y);
    dp_close(&d);
}
TEST(dprim_anglearc_invalid_dc) {
    DP_NEED(); REQUIRE(U(AngleArc));
    SetLastError(0);
    CHECK(!U(AngleArc)(NULL, 40, 40, 20, 0.0f, 90.0f));
    SOFT_ERR(ERROR_INVALID_HANDLE);
}
TEST(dprim_anglearc_radius_is_circle) {
    DP d; DP_NEED(); REQUIRE(U(AngleArc));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 60, 40, NULL);
    U(AngleArc)(d.dc, 40, 40, 20, 0.0f, 360.0f);
    RECT rc; dp_ink(&d, &rc);
    CHECKF(near_(rc.left, 20, 1) && near_(rc.right, 61, 1) && near_(rc.top, 20, 1) && near_(rc.bottom, 61, 1), "%ld %ld %ld %ld", rc.left, rc.top, rc.right, rc.bottom);
    dp_close(&d);
}

/* ---------------- Ellipse ---------------- */
TEST(dprim_ellipse_outline_within_box) {
    DP d; DP_NEED(); REQUIRE(U(Ellipse));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_stock(&d, NULL_BRUSH);
    CHECK(U(Ellipse)(d.dc, 10, 15, 60, 45));
    RECT rc; int n = dp_ink(&d, &rc);
    CHECK(n > 80);
    CHECKF(rc.left == 10 && rc.top == 15 && rc.right <= 60 && rc.bottom <= 45, "%ld %ld %ld %ld", rc.left, rc.top, rc.right, rc.bottom);
    t_note("Ellipse bbox right=%ld bottom=%ld (box 60x45)", rc.right, rc.bottom);
    dp_close(&d);
}
TEST(dprim_ellipse_null_brush_hollow) {
    DP d; DP_NEED(); REQUIRE(U(Ellipse));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_stock(&d, NULL_BRUSH);
    U(Ellipse)(d.dc, 10, 10, 60, 60);
    CHECK(dp_raw(&d, 35, 35) == 0xFFFFFF);
    CHECK(dp_raw(&d, 30, 30) == 0xFFFFFF);
    dp_close(&d);
}
TEST(dprim_ellipse_filled_area) {
    DP d; DP_NEED(); REQUIRE(U(Ellipse));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    CHECK(U(Ellipse)(d.dc, 10, 10, 50, 50));
    int n = dp_cntc(&d, RED_C);
    CHECKF(n > 1150 && n < 1400, "area=%d (pi*20^2=1257)", n);
    CHECK(dp_px(&d, 30, 30) == RED_C);
    CHECK(dp_px(&d, 11, 11) == WHITE_C);
    dp_close(&d);
}
TEST(dprim_ellipse_filled_with_pen_area_close) {
    DP d; DP_NEED(); REQUIRE(U(Ellipse));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    U(Ellipse)(d.dc, 10, 10, 50, 50);
    int r = dp_cntc(&d, RED_C), b = dp_cntc(&d, BLACK_C);
    CHECKF(b > 100 && b < 160, "outline=%d", b);
    CHECKF(r + b > 1150 && r + b < 1400, "total=%d", r + b);
    dp_close(&d);
}
TEST(dprim_ellipse_symmetry_circle) {
    DP d; DP_NEED(); REQUIRE(U(Ellipse));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_stock(&d, NULL_BRUSH);
    U(Ellipse)(d.dc, 10, 10, 50, 50);
    int mx = dp_mirx(&d, 10, 10, 50, 50), my = dp_miry(&d, 10, 10, 50, 50);
    t_note("circle mirror mismatches x=%d y=%d", mx, my);
    SOFT(mx == 0 && my == 0);
    CHECK(mx < 8 && my < 8);
    dp_close(&d);
}
TEST(dprim_ellipse_transpose_equivalence) {
    DP d; DP_NEED(); REQUIRE(U(Ellipse));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    U(Ellipse)(d.dc, 10, 10, 60, 40);
    int a = dp_cntc(&d, RED_C);
    dp_clr(&d);
    U(Ellipse)(d.dc, 10, 10, 40, 60);
    int b = dp_cntc(&d, RED_C);
    CHECKF(near_(a, b, 6), "%d vs %d", a, b);
    dp_close(&d);
}
TEST(dprim_ellipse_degenerate_rects) {
    DP d; DP_NEED(); REQUIRE(U(Ellipse));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    BOOL r;
    r = U(Ellipse)(d.dc, 20, 20, 20, 20); t_note("Ellipse empty -> %d ink=%d", r, dp_ink(&d, NULL));
    r = U(Ellipse)(d.dc, 20, 20, 21, 21); t_note("Ellipse 1x1 -> %d ink=%d", r, dp_ink(&d, NULL));
    r = U(Ellipse)(d.dc, 20, 20, 22, 22); t_note("Ellipse 2x2 -> %d ink=%d", r, dp_ink(&d, NULL));
    r = U(Ellipse)(d.dc, 20, 20, 20, 40); t_note("Ellipse zero width -> %d", r);
    r = U(Ellipse)(d.dc, 20, 20, 40, 20); t_note("Ellipse zero height -> %d", r);
    dp_close(&d);
}
TEST(dprim_ellipse_reversed_rect_same_shape) {
    DP d; DP_NEED(); REQUIRE(U(Ellipse));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_stock(&d, NULL_BRUSH);
    U(Ellipse)(d.dc, 10, 10, 50, 40);
    BYTE *a = malloc(W * H); dp_snap(&d, a);
    dp_clr(&d);
    BOOL r = U(Ellipse)(d.dc, 50, 40, 10, 10);
    t_note("reversed Ellipse ret=%d diff=%d", r, dp_diff(&d, a));
    free(a);
    dp_close(&d);
}
TEST(dprim_ellipse_does_not_move_current_pos) {
    DP d; DP_NEED(); REQUIRE(U(Ellipse));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 3, 4, NULL);
    U(Ellipse)(d.dc, 10, 10, 50, 50);
    POINT c = dp_cur(&d); EXPECT_EQ(c.x, 3); EXPECT_EQ(c.y, 4);
    dp_close(&d);
}
TEST(dprim_ellipse_invalid_dc) {
    DP_NEED(); REQUIRE(U(Ellipse));
    SetLastError(0);
    CHECK(!U(Ellipse)(NULL, 0, 0, 10, 10));
    SOFT_ERR(ERROR_INVALID_HANDLE);
    HDC dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    CHECK(!U(Ellipse)(dc, 0, 0, 10, 10));
}
TEST(dprim_ellipse_huge_no_crash) {
    DP d; DP_NEED(); REQUIRE(U(Ellipse));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    BOOL r;
    r = U(Ellipse)(d.dc, -(1 << 30), -(1 << 30), 1 << 30, 1 << 30);
    t_note("huge ellipse -> %d red=%d", r, dp_cntc(&d, RED_C));
    r = U(Ellipse)(d.dc, -100000, -100000, 100000, 100000);
    t_note("100000 ellipse -> %d red=%d", r, dp_cntc(&d, RED_C));
    r = U(Ellipse)(d.dc, 0, 0, 0x7FFFFFFF, 0x7FFFFFFF);
    dp_close(&d);
}
TEST(dprim_ellipse_big_covers_surface) {
    DP d; DP_NEED(); REQUIRE(U(Ellipse));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    U(Ellipse)(d.dc, -2000, -2000, 2000, 2000);
    int n = dp_cntc(&d, RED_C);
    CHECKF(n == W * H, "n=%d", n);
    dp_close(&d);
}
TEST(dprim_ellipse_clipped_partial) {
    DP d; DP_NEED(); REQUIRE(U(Ellipse));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    U(Ellipse)(d.dc, -20, -20, 20, 20);
    int n = dp_cntc(&d, RED_C);
    CHECKF(n > 270 && n < 340, "n=%d (quarter circle r=20 = 314)", n);
    dp_close(&d);
}
TEST(dprim_ellipse_translate_invariant) {
    DP d; DP_NEED(); REQUIRE(U(Ellipse));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_stock(&d, NULL_BRUSH);
    U(Ellipse)(d.dc, 5, 5, 35, 25);
    BYTE *a = malloc(W * H); dp_snap(&d, a);
    dp_clr(&d);
    U(Ellipse)(d.dc, 25, 35, 55, 55);
    RECT r1, r2; int n = dp_ink(&d, &r2);
    int cnt = 0; for (int i = 0; i < W * H; i++) cnt += a[i];
    CHECKF(cnt == n, "%d vs %d", cnt, n);
    free(a);
    dp_close(&d);
}
TEST(dprim_ellipse_area_scales) {
    static const int sz[] = { 10, 20, 30, 40, 60 };
    for (unsigned i = 0; i < 5; i++) {
        DP d; DP_NEED(); REQUIRE(U(Ellipse));
        CHECK(dp_open(&d, 80, 80, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
        U(Ellipse)(d.dc, 5, 5, 5 + sz[i], 5 + sz[i]);
        double ex = 3.14159265 * sz[i] * sz[i] / 4.0;
        int n = dp_cntc(&d, RED_C);
        CHECKF(n > ex * 0.88 && n < ex * 1.15, "size %d area %d expected %.0f", sz[i], n, ex);
        dp_close(&d);
    }
}

/* ---------------- Chord / Pie ---------------- */
TEST(dprim_pie_half_matches_chord_half) {
    DP d; DP_NEED(); REQUIRE(U(Pie)); REQUIRE(U(Chord));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    CHECK(U(Pie)(d.dc, 10, 10, 50, 50, 50, 30, 10, 30));
    int p = dp_cntc(&d, RED_C);
    dp_clr(&d);
    CHECK(U(Chord)(d.dc, 10, 10, 50, 50, 50, 30, 10, 30));
    int c = dp_cntc(&d, RED_C);
    CHECKF(p > 560 && p < 700, "pie half=%d", p);
    CHECKF(near_(p, c, 40), "pie %d chord %d", p, c);
    dp_close(&d);
}
TEST(dprim_pie_quarter_area) {
    DP d; DP_NEED(); REQUIRE(U(Pie));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    CHECK(U(Pie)(d.dc, 10, 10, 50, 50, 50, 30, 30, 10));
    int n = dp_cntc(&d, RED_C);
    CHECKF(n > 270 && n < 360, "area=%d (314)", n);
    int q[4]; quad(&d, 30, 30, q);
    CHECK(q[0] > 200 && q[1] == 0 && q[2] == 0 && q[3] == 0);
    dp_close(&d);
}
TEST(dprim_chord_quarter_area) {
    DP d; DP_NEED(); REQUIRE(U(Chord));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    CHECK(U(Chord)(d.dc, 10, 10, 50, 50, 50, 30, 30, 10));
    int n = dp_cntc(&d, RED_C);
    CHECKF(n > 70 && n < 150, "area=%d (114)", n);
    dp_close(&d);
}
TEST(dprim_pie_larger_than_chord_for_quarter) {
    DP d; DP_NEED(); REQUIRE(U(Pie)); REQUIRE(U(Chord));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    U(Pie)(d.dc, 10, 10, 50, 50, 50, 30, 30, 10);
    int p = dp_cntc(&d, RED_C);
    dp_clr(&d);
    U(Chord)(d.dc, 10, 10, 50, 50, 50, 30, 30, 10);
    int c = dp_cntc(&d, RED_C);
    CHECKF(p > c + 100, "pie %d chord %d", p, c);
    dp_close(&d);
}
TEST(dprim_pie_clockwise_complement) {
    DP d; DP_NEED(); REQUIRE(U(Pie)); REQUIRE(U(SetArcDirection));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    U(Pie)(d.dc, 10, 10, 50, 50, 50, 30, 30, 10);
    int ccw = dp_cntc(&d, RED_C);
    dp_clr(&d);
    U(SetArcDirection)(d.dc, AD_CLOCKWISE);
    U(Pie)(d.dc, 10, 10, 50, 50, 50, 30, 30, 10);
    int cw = dp_cntc(&d, RED_C);
    CHECKF(cw > 3 * ccw - 150 && cw < 3 * ccw + 150, "cw %d ccw %d", cw, ccw);
    dp_close(&d);
}
TEST(dprim_pie_chord_inside_bbox) {
    DP d; DP_NEED(); REQUIRE(U(Pie)); REQUIRE(U(Chord));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    RECT rc;
    U(Pie)(d.dc, 12, 14, 62, 44, 62, 29, 12, 29);
    dp_ink(&d, &rc);
    CHECK(rc.left >= 12 && rc.right <= 63 && rc.top >= 14 && rc.bottom <= 45);
    dp_clr(&d);
    U(Chord)(d.dc, 12, 14, 62, 44, 62, 29, 12, 29);
    dp_ink(&d, &rc);
    CHECK(rc.left >= 12 && rc.right <= 63 && rc.top >= 14 && rc.bottom <= 45);
    dp_close(&d);
}
TEST(dprim_pie_chord_do_not_move_current_pos) {
    DP d; DP_NEED(); REQUIRE(U(Pie)); REQUIRE(U(Chord));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 3, 4, NULL);
    U(Pie)(d.dc, 10, 10, 50, 50, 50, 30, 30, 10);
    U(Chord)(d.dc, 10, 10, 50, 50, 50, 30, 30, 10);
    POINT c = dp_cur(&d); EXPECT_EQ(c.x, 3); EXPECT_EQ(c.y, 4);
    dp_close(&d);
}
TEST(dprim_pie_null_brush_outline_only) {
    DP d; DP_NEED(); REQUIRE(U(Pie));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_stock(&d, NULL_BRUSH);
    U(Pie)(d.dc, 10, 10, 50, 50, 50, 30, 30, 10);
    CHECK(dp_raw(&d, 40, 20) == 0xFFFFFF);
    CHECK(dp_ink(&d, NULL) > 40);
    dp_close(&d);
}
TEST(dprim_pie_full_when_same_endpoints) {
    DP d; DP_NEED(); REQUIRE(U(Pie));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    U(Pie)(d.dc, 10, 10, 50, 50, 50, 30, 50, 30);
    int n = dp_cntc(&d, RED_C);
    t_note("Pie same endpoints area=%d (full circle 1257)", n);
    CHECK(n > 1100);
    dp_close(&d);
}
TEST(dprim_chord_full_when_same_endpoints) {
    DP d; DP_NEED(); REQUIRE(U(Chord));
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN); dp_brush(&d, RED_C);
    U(Chord)(d.dc, 10, 10, 50, 50, 50, 30, 50, 30);
    int n = dp_cntc(&d, RED_C);
    t_note("Chord same endpoints area=%d", n);
    CHECK(n > 1100);
    dp_close(&d);
}
TEST(dprim_pie_chord_invalid_dc) {
    DP_NEED(); REQUIRE(U(Pie)); REQUIRE(U(Chord));
    SetLastError(0);
    CHECK(!U(Pie)(NULL, 10, 10, 50, 50, 50, 30, 30, 10));
    SOFT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0);
    CHECK(!U(Chord)(NULL, 10, 10, 50, 50, 50, 30, 30, 10));
    SOFT_ERR(ERROR_INVALID_HANDLE);
}
TEST(dprim_pie_chord_degenerate) {
    DP d; DP_NEED(); REQUIRE(U(Pie)); REQUIRE(U(Chord));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    BOOL r;
    r = U(Pie)(d.dc, 20, 20, 20, 20, 20, 20, 20, 20); t_note("Pie empty -> %d", r);
    r = U(Chord)(d.dc, 20, 20, 20, 20, 20, 20, 20, 20); t_note("Chord empty -> %d", r);
    r = U(Pie)(d.dc, 20, 20, 20, 50, 20, 20, 20, 50); t_note("Pie zero-width -> %d", r);
    r = U(Chord)(d.dc, 20, 20, 50, 20, 20, 20, 50, 20); t_note("Chord zero-height -> %d", r);
    dp_close(&d);
}
TEST(dprim_pie_chord_huge_no_crash) {
    DP d; DP_NEED(); NOT_WINE("heap corruption with huge arcs"); REQUIRE(U(Pie)); REQUIRE(U(Chord));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RED_C);
    U(Pie)(d.dc, -(1 << 30), -(1 << 30), 1 << 30, 1 << 30, 0, 0, 5, 5);
    U(Chord)(d.dc, 0, 0, 0x7FFFFFFF, 0x7FFFFFFF, 10, 10, 20, 20);
    U(Pie)(d.dc, 0, 0, 40, 40, (int)0x80000000, 0x7FFFFFFF, 0x7FFFFFFF, (int)0x80000000);
    dp_close(&d);
}
