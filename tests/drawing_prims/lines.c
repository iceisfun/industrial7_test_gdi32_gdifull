/* MoveToEx / LineTo: pixel-exact checks on a 32bpp DIBSection memory DC. */
#include "dp.h"

#define W 64
#define H 64

TEST(dprim_line_horizontal_endpoint_excluded) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    CHECK(dp_line(&d, 5, 10, 25, 10));
    for (int x = 0; x < W; x++) {
        int want = (x >= 5 && x < 25);
        CHECKF((dp_raw(&d, x, 10) == 0) == want, "x=%d", x);
    }
    EXPECT_EQ(dp_ink(&d, NULL), 20);
    dp_close(&d);
}
TEST(dprim_line_horizontal_reversed) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    CHECK(dp_line(&d, 25, 10, 5, 10));
    for (int x = 0; x < W; x++) {
        int want = (x > 5 && x <= 25);
        CHECKF((dp_raw(&d, x, 10) == 0) == want, "x=%d", x);
    }
    EXPECT_EQ(dp_ink(&d, NULL), 20);
    dp_close(&d);
}
TEST(dprim_line_vertical_endpoint_excluded) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    CHECK(dp_line(&d, 7, 3, 7, 30));
    for (int y = 0; y < H; y++) CHECKF((dp_raw(&d, 7, y) == 0) == (y >= 3 && y < 30), "y=%d", y);
    EXPECT_EQ(dp_ink(&d, NULL), 27);
    dp_close(&d);
}
TEST(dprim_line_vertical_reversed) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    CHECK(dp_line(&d, 7, 30, 7, 3));
    for (int y = 0; y < H; y++) CHECKF((dp_raw(&d, 7, y) == 0) == (y > 3 && y <= 30), "y=%d", y);
    dp_close(&d);
}
TEST(dprim_line_diag_down_right) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    CHECK(dp_line(&d, 4, 4, 20, 20));
    for (int i = 0; i < 16; i++) CHECKF(dp_raw(&d, 4 + i, 4 + i) == 0, "i=%d", i);
    CHECK(dp_raw(&d, 20, 20) != 0);
    EXPECT_EQ(dp_ink(&d, NULL), 16);
    dp_close(&d);
}
TEST(dprim_line_diag_up_left) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    CHECK(dp_line(&d, 20, 20, 4, 4));
    for (int i = 1; i <= 16; i++) CHECKF(dp_raw(&d, 4 + i, 4 + i) == 0, "i=%d", i);
    CHECK(dp_raw(&d, 4, 4) != 0);
    EXPECT_EQ(dp_ink(&d, NULL), 16);
    dp_close(&d);
}
TEST(dprim_line_diag_down_left) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    CHECK(dp_line(&d, 30, 5, 14, 21));
    for (int i = 0; i < 16; i++) CHECKF(dp_raw(&d, 30 - i, 5 + i) == 0, "i=%d", i);
    CHECK(dp_raw(&d, 14, 21) != 0);
    EXPECT_EQ(dp_ink(&d, NULL), 16);
    dp_close(&d);
}
TEST(dprim_line_diag_up_right) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    CHECK(dp_line(&d, 14, 21, 30, 5));
    for (int i = 0; i < 16; i++) CHECKF(dp_raw(&d, 14 + i, 21 - i) == 0, "i=%d", i);
    CHECK(dp_raw(&d, 30, 5) != 0);
    dp_close(&d);
}
TEST(dprim_line_zero_length) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    CHECK(dp_line(&d, 10, 10, 10, 10));
    EXPECT_EQ(dp_ink(&d, NULL), 0);
    dp_close(&d);
}
TEST(dprim_line_one_pixel_long) {
    static const int dx[] = { 1, -1, 0, 0, 1, -1, 1, -1 };
    static const int dy[] = { 0, 0, 1, -1, 1, 1, -1, -1 };
    for (int i = 0; i < 8; i++) {
        DP d; DP_NEED();
        CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
        CHECK(dp_line(&d, 20, 20, 20 + dx[i], 20 + dy[i]));
        EXPECT_EQ(dp_ink(&d, NULL), 1);
        CHECKF(dp_raw(&d, 20, 20) == 0, "dir %d", i);
        dp_close(&d);
    }
}
TEST(dprim_line_pixel_count_equals_major_axis) {
    static const int v[][4] = { {2,2,30,9}, {2,2,9,30}, {30,2,2,9}, {2,30,9,2}, {5,5,55,6}, {5,5,6,55}, {10,3,40,50}, {40,50,10,3}, {3,3,60,60}, {0,0,63,1} };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        DP d; DP_NEED();
        CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
        dp_line(&d, v[i][0], v[i][1], v[i][2], v[i][3]);
        int dx = abs(v[i][2] - v[i][0]), dy = abs(v[i][3] - v[i][1]);
        CHECKF(dp_ink(&d, NULL) == (dx > dy ? dx : dy), "line %u got %d", i, dp_ink(&d, NULL));
        dp_close(&d);
    }
}
TEST(dprim_line_start_pixel_drawn_end_not) {
    static const int v[][4] = { {2,2,30,9}, {2,2,9,30}, {30,2,2,9}, {2,30,9,2}, {10,3,40,50}, {40,50,10,3}, {1,1,40,1}, {1,1,1,40} };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        DP d; DP_NEED();
        CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
        dp_line(&d, v[i][0], v[i][1], v[i][2], v[i][3]);
        CHECKF(dp_raw(&d, v[i][0], v[i][1]) == 0, "start %u", i);
        CHECKF(dp_raw(&d, v[i][2], v[i][3]) != 0, "end %u", i);
        dp_close(&d);
    }
}
TEST(dprim_line_shallow_one_pixel_per_column) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    dp_line(&d, 2, 5, 50, 20);
    for (int x = 2; x < 50; x++) CHECKF(dp_col(&d, x, 0, H) == 1, "x=%d n=%d", x, dp_col(&d, x, 0, H));
    dp_close(&d);
}
TEST(dprim_line_steep_one_pixel_per_row) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    dp_line(&d, 5, 2, 20, 50);
    for (int y = 2; y < 50; y++) CHECKF(dp_row(&d, y, 0, W) == 1, "y=%d n=%d", y, dp_row(&d, y, 0, W));
    dp_close(&d);
}
TEST(dprim_line_monotonic_no_gaps) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    dp_line(&d, 3, 40, 58, 7);
    int prev = -1;
    for (int x = 4; x <= 58; x++) {
        int y = -1;
        for (int yy = 0; yy < H; yy++) if (dp_raw(&d, x, yy) == 0) y = yy;
        if (x < 58 && x > 3) CHECKF(y >= 0, "x=%d", x);
        if (prev >= 0 && y >= 0) CHECKF(prev - y >= 0 && prev - y <= 1, "x=%d prev=%d y=%d", x, prev, y);
        prev = y;
    }
    dp_close(&d);
}
TEST(dprim_line_reverse_same_pixel_count) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    dp_line(&d, 3, 4, 50, 30);
    int a = dp_ink(&d, NULL);
    dp_clr(&d);
    dp_line(&d, 50, 30, 3, 4);
    int b = dp_ink(&d, NULL);
    EXPECT_EQ(a, b);
    dp_close(&d);
}
TEST(dprim_line_reverse_overlap_note) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    dp_line(&d, 3, 4, 50, 30);
    BYTE *s = malloc(W * H); dp_snap(&d, s);
    dp_clr(&d);
    dp_line(&d, 50, 30, 3, 4);
    t_note("forward vs reverse line: %d differing pixels", dp_diff(&d, s));
    free(s);
    dp_close(&d);
}
TEST(dprim_line_current_pos_updates) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 3, 4, NULL);
    POINT p = dp_cur(&d); EXPECT_EQ(p.x, 3); EXPECT_EQ(p.y, 4);
    CHECK(U(LineTo)(d.dc, 20, 30));
    p = dp_cur(&d); EXPECT_EQ(p.x, 20); EXPECT_EQ(p.y, 30);
    CHECK(U(LineTo)(d.dc, 20, 50));
    p = dp_cur(&d); EXPECT_EQ(p.x, 20); EXPECT_EQ(p.y, 50);
    dp_close(&d);
}
TEST(dprim_line_lineto_chain_continuous) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 5, 5, NULL);
    U(LineTo)(d.dc, 30, 5); U(LineTo)(d.dc, 30, 30); U(LineTo)(d.dc, 5, 30); U(LineTo)(d.dc, 5, 5);
    EXPECT_EQ(dp_ink(&d, NULL), 100);
    CHECK(dp_raw(&d, 30, 5) == 0 && dp_raw(&d, 30, 30) == 0 && dp_raw(&d, 5, 30) == 0 && dp_raw(&d, 5, 5) == 0);
    dp_close(&d);
}
TEST(dprim_line_moveto_old_pos) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32));
    POINT old = { -5, -5 };
    CHECK(U(MoveToEx)(d.dc, 11, 12, &old));
    EXPECT_EQ(old.x, 0); EXPECT_EQ(old.y, 0);
    CHECK(U(MoveToEx)(d.dc, 21, 22, &old));
    EXPECT_EQ(old.x, 11); EXPECT_EQ(old.y, 12);
    CHECK(U(MoveToEx)(d.dc, 31, 32, NULL));
    POINT p = dp_cur(&d); EXPECT_EQ(p.x, 31); EXPECT_EQ(p.y, 32);
    dp_close(&d);
}
TEST(dprim_line_moveto_no_ink) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, 1, 1, NULL); U(MoveToEx)(d.dc, 50, 50, NULL);
    EXPECT_EQ(dp_ink(&d, NULL), 0);
    dp_close(&d);
}
TEST(dprim_line_negative_current_pos) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(MoveToEx)(d.dc, -10, 5, NULL);
    CHECK(U(LineTo)(d.dc, 20, 5));
    for (int x = 0; x < W; x++) CHECKF((dp_raw(&d, x, 5) == 0) == (x < 20), "x=%d", x);
    POINT p = dp_cur(&d); EXPECT_EQ(p.x, 20);
    dp_close(&d);
}
TEST(dprim_line_clipped_left_top) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    dp_line(&d, -30, -30, 10, 10);
    EXPECT_EQ(dp_ink(&d, NULL), 10);
    for (int i = 0; i < 10; i++) CHECKF(dp_raw(&d, i, i) == 0, "i=%d", i);
    dp_close(&d);
}
TEST(dprim_line_clipped_right_bottom) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    dp_line(&d, 54, 54, 200, 200);
    EXPECT_EQ(dp_ink(&d, NULL), 10);
    dp_close(&d);
}
TEST(dprim_line_clipped_horizontal_passes_through) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    dp_line(&d, -100, 20, 300, 20);
    EXPECT_EQ(dp_ink(&d, NULL), W);
    EXPECT_EQ(dp_row(&d, 20, 0, W), W);
    dp_close(&d);
}
TEST(dprim_line_clipped_vertical_passes_through) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    dp_line(&d, 20, 500, 20, -500);
    EXPECT_EQ(dp_ink(&d, NULL), H);
    dp_close(&d);
}
TEST(dprim_line_completely_outside) {
    static const int v[][4] = { {-50,-50,-10,-10}, {100,0,200,50}, {0,100,60,100}, {-20,10,-1,10}, {64,0,64,60}, {0,64,60,64}, {-1,-1,-1,60} };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        DP d; DP_NEED();
        CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
        CHECKF(dp_line(&d, v[i][0], v[i][1], v[i][2], v[i][3]), "%u", i);
        EXPECT_EQ(dp_ink(&d, NULL), 0);
        dp_close(&d);
    }
}
TEST(dprim_line_clipped_equals_unclipped_subset) {
    /* a clipped line must match the same line drawn on a larger surface, cropped */
    DP a, b; DP_NEED();
    CHECK(dp_open(&a, W, H, 32)); CHECK(dp_open(&b, 3 * W, 3 * H, 32));
    dp_black(&a); dp_black(&b);
    dp_line(&a, -20, 10, 80, 50);
    dp_line(&b, -20 + W, 10 + H, 80 + W, 50 + H);
    int bad = 0;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) if ((dp_raw(&a, x, y) == 0) != (dp_raw(&b, x + W, y + H) == 0)) bad++;
    CHECKF(bad == 0, "bad=%d", bad);
    dp_close(&a); dp_close(&b);
}
TEST(dprim_line_pen_color) {
    static const COLORREF cs[] = { RGB(255,0,0), RGB(0,255,0), RGB(0,0,255), RGB(1,2,3), RGB(128,64,32), RGB(254,254,254), RGB(255,255,0), RGB(0,0,0) };
    for (unsigned i = 0; i < sizeof cs / sizeof cs[0]; i++) {
        DP d; DP_NEED();
        CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_SOLID, 1, cs[i]);
        dp_line(&d, 5, 5, 25, 5);
        CHECKF(dp_px(&d, 10, 5) == cs[i], "i=%u got %06lx", i, (unsigned long)dp_px(&d, 10, 5));
        EXPECT_EQ(dp_cntc(&d, cs[i]), 20);
        dp_close(&d);
    }
}
TEST(dprim_line_pen_width0_is_one_pixel) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_pen(&d, PS_SOLID, 0, RGB(255, 0, 0));
    dp_line(&d, 5, 5, 25, 5);
    EXPECT_EQ(dp_cntc(&d, RGB(255, 0, 0)), 20);
    EXPECT_EQ(dp_ink(&d, NULL), 20);
    dp_close(&d);
}
TEST(dprim_line_null_pen_draws_nothing) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_stock(&d, NULL_PEN);
    CHECK(dp_line(&d, 5, 5, 40, 30));
    EXPECT_EQ(dp_ink(&d, NULL), 0);
    POINT p = dp_cur(&d); EXPECT_EQ(p.x, 40); EXPECT_EQ(p.y, 30);
    dp_close(&d);
}
TEST(dprim_line_stock_pens) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32));
    dp_stock(&d, BLACK_PEN); dp_line(&d, 0, 3, 10, 3);
    CHECK(dp_px(&d, 2, 3) == BLACK_C);
    dp_stock(&d, WHITE_PEN); dp_line(&d, 0, 6, 10, 6);
    CHECK(dp_px(&d, 2, 6) == WHITE_C);
    EXPECT_EQ(dp_ink(&d, NULL), 10);
    dp_close(&d);
}
TEST(dprim_line_ignores_brush) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d); dp_brush(&d, RGB(255, 0, 0));
    dp_line(&d, 5, 5, 40, 5);
    EXPECT_EQ(dp_cntc(&d, RGB(255, 0, 0)), 0);
    dp_close(&d);
}
TEST(dprim_line_viewport_org_shifts) {
    DP d; DP_NEED(); REQUIRE(U(SetViewportOrgEx));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(SetViewportOrgEx)(d.dc, 7, 9, NULL);
    dp_line(&d, 0, 0, 20, 0);
    for (int x = 0; x < W; x++) CHECKF((dp_raw(&d, x, 9) == 0) == (x >= 7 && x < 27), "x=%d", x);
    EXPECT_EQ(dp_ink(&d, NULL), 20);
    U(SetViewportOrgEx)(d.dc, 0, 0, NULL);
    dp_close(&d);
}
TEST(dprim_line_window_org_shifts) {
    DP d; DP_NEED(); REQUIRE(U(SetWindowOrgEx));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(SetWindowOrgEx)(d.dc, -10, -12, NULL);
    dp_line(&d, 0, 0, 0, 20);
    for (int y = 0; y < H; y++) CHECKF((dp_raw(&d, 10, y) == 0) == (y >= 12 && y < 32), "y=%d", y);
    U(SetWindowOrgEx)(d.dc, 0, 0, NULL);
    dp_close(&d);
}
TEST(dprim_line_current_pos_logical_units) {
    DP d; DP_NEED(); REQUIRE(U(SetViewportOrgEx));
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(SetViewportOrgEx)(d.dc, 7, 9, NULL);
    U(MoveToEx)(d.dc, 3, 4, NULL);
    POINT p = dp_cur(&d); EXPECT_EQ(p.x, 3); EXPECT_EQ(p.y, 4);
    U(LineTo)(d.dc, 13, 4);
    EXPECT_EQ(dp_raw(&d, 10, 13), 0);
    U(SetViewportOrgEx)(d.dc, 0, 0, NULL);
    dp_close(&d);
}
TEST(dprim_line_invalid_dc_null) {
    DP_NEED();
    SetLastError(0);
    EXPECT_EQ(U(LineTo)(NULL, 5, 5), 0);
    SOFT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0);
    EXPECT_EQ(U(MoveToEx)(NULL, 5, 5, NULL), 0);
    SOFT_ERR(ERROR_INVALID_HANDLE);
}
TEST(dprim_line_invalid_dc_garbage) {
    DP_NEED();
    HDC bogus[] = { (HDC)(ULONG_PTR)0x1234, (HDC)(ULONG_PTR)0xFFFFFFFF, (HDC)(ULONG_PTR)0x00010001, (HDC)(ULONG_PTR)4 };
    for (unsigned i = 0; i < 4; i++) {
        SetLastError(0);
        CHECKF(!U(LineTo)(bogus[i], 1, 1), "%u", i);
        SOFT_ERR(ERROR_INVALID_HANDLE);
        CHECKF(!U(MoveToEx)(bogus[i], 1, 1, NULL), "%u", i);
    }
}
TEST(dprim_line_wrong_object_type) {
    DP_NEED();
    HBRUSH b = U(CreateSolidBrush)(0);
    SetLastError(0);
    CHECK(!U(LineTo)((HDC)b, 1, 1));
    SOFT_ERR(ERROR_INVALID_HANDLE);
    CHECK(!U(MoveToEx)((HDC)b, 1, 1, NULL));
    U(DeleteObject)(b);
}
TEST(dprim_line_deleted_dc) {
    DP_NEED();
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(dc != NULL);
    CHECK(U(DeleteDC)(dc));
    SetLastError(0);
    CHECK(!U(LineTo)(dc, 3, 3));
    SOFT_ERR(ERROR_INVALID_HANDLE);
    CHECK(!U(MoveToEx)(dc, 3, 3, NULL));
}
TEST(dprim_line_moveto_old_pos_untouched_on_failure) {
    DP_NEED();
    POINT p = { 123, 456 };
    CHECK(!U(MoveToEx)(NULL, 1, 1, &p));
    t_note("MoveToEx(NULL) old-point output: %ld,%ld", p.x, p.y);
}
TEST(dprim_line_fresh_dc_default_pos) {
    DP_NEED();
    HDC dc = U(CreateCompatibleDC)(NULL);
    POINT p = { 9, 9 };
    CHECK(U(GetCurrentPositionEx)(dc, &p));
    EXPECT_EQ(p.x, 0); EXPECT_EQ(p.y, 0);
    U(DeleteDC)(dc);
}
TEST(dprim_line_current_pos_survives_select_bitmap) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32));
    U(MoveToEx)(d.dc, 12, 13, NULL);
    HBITMAP b2 = U(CreateCompatibleBitmap) ? NULL : NULL; (void)b2;
    HGDIOBJ o = U(SelectObject)(d.dc, d.obmp);
    U(SelectObject)(d.dc, o);
    POINT p = dp_cur(&d);
    t_note("current position after reselect: %ld,%ld", p.x, p.y);
    dp_close(&d);
}
TEST(dprim_line_huge_coordinates_no_crash) {
    static const int big[] = { 1 << 30, -(1 << 30), 0x7FFFFFFF, (int)0x80000000, 1 << 24, -(1 << 24), 1 << 20 };
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    for (unsigned i = 0; i < sizeof big / sizeof big[0]; i++) {
        BOOL r = dp_line(&d, 0, 5, big[i], 5);
        r = dp_line(&d, big[i], big[i], 10, 10);
        r = dp_line(&d, 5, 0, 5, big[i]);
        (void)r;
    }
    t_note("huge-coordinate lines left %d ink pixels", dp_ink(&d, NULL));
    dp_close(&d);
}
TEST(dprim_line_huge_coords_horizontal_visible_part) {
    DP d; DP_NEED(); NOT_WINE("huge coordinates corrupt wine heap");
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    dp_line(&d, -(1 << 20), 8, 1 << 20, 8);
    EXPECT_EQ(dp_row(&d, 8, 0, W), W);
    dp_close(&d);
}
TEST(dprim_line_return_true_when_clipped) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    CHECK(dp_line(&d, -1000, -1000, -900, -900));
    dp_close(&d);
}
TEST(dprim_line_many_repeated) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    for (int i = 0; i < 2000; i++) dp_line(&d, 0, i % H, W, i % H);
    EXPECT_EQ(dp_ink(&d, NULL), W * H);
    dp_close(&d);
}
TEST(dprim_line_overdraw_idempotent) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    dp_line(&d, 3, 4, 50, 30);
    int a = dp_ink(&d, NULL);
    dp_line(&d, 3, 4, 50, 30);
    EXPECT_EQ(dp_ink(&d, NULL), a);
    dp_close(&d);
}
TEST(dprim_line_pen_change_between_segments) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32));
    dp_pen(&d, PS_SOLID, 1, RED_C); U(MoveToEx)(d.dc, 2, 2, NULL); U(LineTo)(d.dc, 12, 2);
    dp_pen(&d, PS_SOLID, 1, BLUE_C); U(LineTo)(d.dc, 12, 12);
    EXPECT_EQ(dp_cntc(&d, RED_C), 10);
    EXPECT_EQ(dp_cntc(&d, BLUE_C), 10);
    dp_close(&d);
}
TEST(dprim_line_cross_symmetry) {
    /* four rays from a centre, same length */
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    dp_line(&d, 32, 32, 52, 32); dp_line(&d, 32, 32, 12, 32);
    dp_line(&d, 32, 32, 32, 52); dp_line(&d, 32, 32, 32, 12);
    EXPECT_EQ(dp_ink(&d, NULL), 77);
    dp_close(&d);
}
TEST(dprim_line_all_octants_count) {
    for (int a = 0; a < 16; a++) {
        static const int dx[16] = { 20, 20, 20, 10, 0, -10, -20, -20, -20, -20, -20, -10, 0, 10, 20, 20 };
        static const int dy[16] = { 0, 5, 10, 20, 20, 20, 20, 10, 0, -10, -20, -20, -20, -20, -20, -10 };
        DP d; DP_NEED();
        CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
        dp_line(&d, 32, 32, 32 + dx[a], 32 + dy[a]);
        int m = abs(dx[a]) > abs(dy[a]) ? abs(dx[a]) : abs(dy[a]);
        CHECKF(dp_ink(&d, NULL) == m, "dir %d got %d", a, dp_ink(&d, NULL));
        CHECKF(dp_raw(&d, 32, 32) == 0, "dir %d", a);
        dp_close(&d);
    }
}
TEST(dprim_line_rop2_not_affects_current_pos) {
    DP d; DP_NEED();
    CHECK(dp_open(&d, W, H, 32)); dp_black(&d);
    U(SetROP2)(d.dc, R2_NOT);
    dp_line(&d, 4, 4, 14, 4);
    POINT p = dp_cur(&d); EXPECT_EQ(p.x, 14);
    dp_close(&d);
}
TEST(dprim_line_gdi_object_leak) {
    DP_NEED(); REQUIRE(U(GetGuiResources));
    DWORD before = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    for (int i = 0; i < 20; i++) { DP d; dp_open(&d, 16, 16, 32); dp_black(&d); dp_line(&d, 0, 0, 10, 10); dp_close(&d); }
    DWORD after = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    CHECKF(after == before, "before=%lu after=%lu", before, after);
}
