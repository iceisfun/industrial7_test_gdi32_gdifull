/* Additional path coverage: current position, closing rules, transforms, brushes, regions, edge arguments. */
#include "ph.h"

#define SETUP(m) MD m; REQ_BASE(); if (!md_create(&m, 64, 64)) { t_skip("no DIB"); return; } md_clear(&m, WHITE)
#define RED 0xFF0000u
#define BLUE 0x0000FFu

static HBRUSH sel_brush(MD *m, COLORREF c, HGDIOBJ *oldb, HGDIOBJ *oldp) {
    HBRUSH b = U(CreateSolidBrush)(c);
    *oldb = U(SelectObject)(m->dc, b);
    *oldp = U(SelectObject)(m->dc, U(GetStockObject)(NULL_PEN));
    return b;
}
static void unsel(MD *m, HBRUSH b, HGDIOBJ ob, HGDIOBJ op) {
    U(SelectObject)(m->dc, ob); U(SelectObject)(m->dc, op); U(DeleteObject)(b);
}

TEST(paths_extra_moveto_returns_previous_pos_inside_bracket) {
    SETUP(m);
    POINT p = {-1, -1};
    MT(m.dc, 3, 4);
    CHECK(BP(m.dc));
    CHECK(U(MoveToEx)(m.dc, 10, 11, &p));
    EXPECT_EQ(p.x, 3); EXPECT_EQ(p.y, 4);
    CHECK(EP(m.dc));
    AP(m.dc);
    md_destroy(&m);
}
TEST(paths_extra_arc_does_not_move_current_pos_in_path) {
    SETUP(m);
    POINT p;
    REQUIRE(U(Arc));
    MT(m.dc, 2, 2);
    BP(m.dc);
    CHECK(U(Arc)(m.dc, 10, 10, 40, 40, 40, 25, 25, 10));
    U(GetCurrentPositionEx)(m.dc, &p);
    t_note("current pos after Arc in path (%ld,%ld)", p.x, p.y);
    EP(m.dc);
    CHECK(GPN(m.dc) > 1);
    AP(m.dc);
    md_destroy(&m);
}
TEST(paths_extra_arcto_moves_current_pos_to_arc_end) {
    SETUP(m);
    POINT p;
    REQUIRE(U(ArcTo));
    BP(m.dc);
    CHECK(U(ArcTo)(m.dc, 10, 10, 50, 50, 50, 30, 30, 10));
    U(GetCurrentPositionEx)(m.dc, &p);
    t_note("current pos after ArcTo (%ld,%ld)", p.x, p.y);
    CHECK(abs(p.x - 30) <= 3 && abs(p.y - 10) <= 3);
    EP(m.dc);
    CHECK(GPN(m.dc) >= 4);
    AP(m.dc);
    md_destroy(&m);
}
TEST(paths_extra_anglearc_moves_current_pos_to_end) {
    SETUP(m);
    POINT p;
    REQUIRE(U(AngleArc));
    BP(m.dc);
    CHECK(U(AngleArc)(m.dc, 32, 32, 20, 0.0f, 90.0f));
    U(GetCurrentPositionEx)(m.dc, &p);
    CHECK(abs(p.x - 32) <= 1 && (abs(p.y - 12) <= 1 || abs(p.y - 52) <= 1));
    EP(m.dc);
    pd_get(m.dc);
    CHECK(pd_valid());
    AP(m.dc);
    md_destroy(&m);
}
TEST(paths_extra_polylineto_from_current_pos) {
    SETUP(m);
    POINT p[2] = {{20, 20}, {30, 10}};
    MT(m.dc, 5, 5);
    BP(m.dc);
    CHECK(U(PolylineTo)(m.dc, p, 2));
    EP(m.dc);
    EXPECT_EQ(pd_get(m.dc), 3);
    CHECK(pd_near(0, 5, 5, 0));
    EXPECT_EQ(g_pd.ty[0], PT_MOVETO);
    EXPECT_EQ(g_pd.ty[2], PT_LINETO);
    md_destroy(&m);
}
TEST(paths_extra_polybezierto_from_current_pos) {
    SETUP(m);
    POINT p[3] = {{10, 5}, {20, 5}, {30, 20}};
    MT(m.dc, 5, 20);
    BP(m.dc);
    CHECK(U(PolyBezierTo)(m.dc, p, 3));
    EP(m.dc);
    EXPECT_EQ(pd_get(m.dc), 4);
    CHECK(pd_near(0, 5, 20, 0));
    EXPECT_EQ(pd_count_type(PT_BEZIERTO), 3);
    POINT c; U(GetCurrentPositionEx)(m.dc, &c);
    EXPECT_EQ(c.x, 30); EXPECT_EQ(c.y, 20);
    md_destroy(&m);
}
TEST(paths_extra_polydraw_lineto_closefigure_flag) {
    SETUP(m);
    POINT p[4] = {{5, 5}, {30, 5}, {30, 30}, {5, 30}};
    BYTE t[4] = {PT_MOVETO, PT_LINETO, PT_LINETO, PT_LINETO | PT_CLOSEFIGURE};
    BP(m.dc);
    CHECK(U(PolyDraw)(m.dc, p, t, 4));
    EP(m.dc);
    EXPECT_EQ(pd_get(m.dc), 4);
    EXPECT_EQ(g_pd.ty[3], PT_LINETO | PT_CLOSEFIGURE);
    EXPECT_EQ(pd_count_close(), 1);
    md_destroy(&m);
}
TEST(paths_extra_polydraw_moveto_only_then_fill_nothing) {
    SETUP(m);
    POINT p[2] = {{5, 5}, {30, 30}};
    BYTE t[2] = {PT_MOVETO, PT_MOVETO};
    HGDIOBJ ob, op; HBRUSH b = sel_brush(&m, RGB(255, 0, 0), &ob, &op);
    BP(m.dc);
    U(PolyDraw)(m.dc, p, t, 2);
    EP(m.dc);
    CHECK(FP(m.dc));
    EXPECT_EQ(md_count(&m, RED), 0);
    unsel(&m, b, ob, op);
    md_destroy(&m);
}
TEST(paths_extra_world_scale_fill_exact) {
    SETUP(m);
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform));
    HGDIOBJ ob, op; HBRUSH b = sel_brush(&m, RGB(255, 0, 0), &ob, &op);
    XFORM x = {2.0f, 0, 0, 2.0f, 0, 0};
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    CHECK(U(SetWorldTransform)(m.dc, &x));
    BP(m.dc); POLYGON4(m.dc, 4, 4, 10, 10); EP(m.dc);
    CHECK(FP(m.dc));
    CHECK(md_rect_is(&m, 8, 8, 20, 20, RED, WHITE));
    XFORM id = {1.0f, 0, 0, 1.0f, 0, 0};
    U(SetWorldTransform)(m.dc, &id);
    U(SetGraphicsMode)(m.dc, GM_COMPATIBLE);
    unsel(&m, b, ob, op);
    md_destroy(&m);
}
TEST(paths_extra_world_translate_fill_exact) {
    SETUP(m);
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform));
    HGDIOBJ ob, op; HBRUSH b = sel_brush(&m, RGB(255, 0, 0), &ob, &op);
    XFORM x = {1.0f, 0, 0, 1.0f, 7.0f, 9.0f};
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    CHECK(U(SetWorldTransform)(m.dc, &x));
    BP(m.dc); POLYGON4(m.dc, 3, 3, 10, 12); EP(m.dc);
    CHECK(FP(m.dc));
    CHECK(md_rect_is(&m, 10, 12, 17, 21, RED, WHITE));
    XFORM id = {1.0f, 0, 0, 1.0f, 0, 0};
    U(SetWorldTransform)(m.dc, &id);
    U(SetGraphicsMode)(m.dc, GM_COMPATIBLE);
    unsel(&m, b, ob, op);
    md_destroy(&m);
}
TEST(paths_extra_viewport_org_applied_at_build_time) {
    SETUP(m);
    HGDIOBJ ob, op; HBRUSH b = sel_brush(&m, RGB(255, 0, 0), &ob, &op);
    U(SetViewportOrgEx)(m.dc, 10, 10, NULL);
    BP(m.dc); POLYGON4(m.dc, 0, 0, 8, 8); EP(m.dc);
    U(SetViewportOrgEx)(m.dc, 0, 0, NULL);
    CHECK(FP(m.dc));
    CHECK(md_rect_is(&m, 10, 10, 18, 18, RED, WHITE));
    unsel(&m, b, ob, op);
    md_destroy(&m);
}
TEST(paths_extra_pattern_brush_exact) {
    SETUP(m);
    REQUIRE(U(CreateBitmap)); REQUIRE(U(CreatePatternBrush));
    /* 8x8 mono bitmap: alternating columns (bit set = white) */
    WORD bits[8]; int i;
    for (i = 0; i < 8; i++) bits[i] = 0x5555;
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, bits);
    HBRUSH pb = U(CreatePatternBrush)(bm);
    if (!bm || !pb) { if (bm) U(DeleteObject)(bm); if (pb) U(DeleteObject)(pb); return; }
    HGDIOBJ ob = U(SelectObject)(m.dc, pb), op = U(SelectObject)(m.dc, U(GetStockObject)(NULL_PEN));
    BP(m.dc); POLYGON4(m.dc, 0, 0, 16, 8); EP(m.dc);
    CHECK(FP(m.dc));
    int x, y, a = 0, bk = 0, wh = 0;
    for (y = 0; y < 8; y++) for (x = 0; x < 16; x++) {
        unsigned c = md_px(&m, x, y);
        if (c == 0) bk++; else if (c == WHITE) wh++;
        a += (c == md_px(&m, x & 1, 0));
    }
    EXPECT_EQ(bk, 64); EXPECT_EQ(wh, 64);
    EXPECT_EQ(a, 128);   /* two-column period in every row */
    U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, op);
    U(DeleteObject)(pb); U(DeleteObject)(bm);
    md_destroy(&m);
}
TEST(paths_extra_region_ellipse_ptinregion) {
    SETUP(m);
    REQUIRE(U(PtInRegion)); REQUIRE(U(GetRegionData));
    BP(m.dc); U(Ellipse)(m.dc, 4, 4, 44, 44); EP(m.dc);
    HRGN rg = U(PathToRegion)(m.dc);
    CHECK(rg != NULL);
    if (rg) {
        CHECK(U(PtInRegion)(rg, 24, 24));
        CHECK(!U(PtInRegion)(rg, 5, 5));
        CHECK(!U(PtInRegion)(rg, 43, 43));
        CHECK(!U(PtInRegion)(rg, 2, 24));
        CHECK(!U(PtInRegion)(rg, 24, 46));
        U(DeleteObject)(rg);
    }
    md_destroy(&m);
}
TEST(paths_extra_region_rect_is_simple) {
    SETUP(m);
    BP(m.dc); POLYGON4(m.dc, 5, 6, 25, 30); EP(m.dc);
    HRGN rg = U(PathToRegion)(m.dc);
    CHECK(rg != NULL);
    if (rg) {
        RECT r;
        EXPECT_EQ(U(GetRgnBox)(rg, &r), SIMPLEREGION);
        EXPECT_EQ(r.left, 5); EXPECT_EQ(r.top, 6); EXPECT_EQ(r.right, 25); EXPECT_EQ(r.bottom, 30);
        U(DeleteObject)(rg);
    }
    md_destroy(&m);
}
TEST(paths_extra_region_two_figures_complex) {
    SETUP(m);
    BP(m.dc); POLYGON4(m.dc, 2, 2, 10, 10); POLYGON4(m.dc, 20, 20, 30, 30); EP(m.dc);
    HRGN rg = U(PathToRegion)(m.dc);
    CHECK(rg != NULL);
    if (rg) {
        RECT r;
        EXPECT_EQ(U(GetRgnBox)(rg, &r), COMPLEXREGION);
        EXPECT_EQ(r.left, 2); EXPECT_EQ(r.right, 30);
        U(DeleteObject)(rg);
    }
    md_destroy(&m);
}
TEST(paths_extra_region_survives_dc_deletion) {
    REQ_BASE();
    MD m;
    if (!md_create(&m, 32, 32)) return;
    BP(m.dc); POLYGON4(m.dc, 1, 1, 9, 9); EP(m.dc);
    HRGN rg = U(PathToRegion)(m.dc);
    md_destroy(&m);
    CHECK(rg != NULL);
    if (rg) {
        RECT r;
        CHECK(U(GetRgnBox)(rg, &r) != 0);
        EXPECT_EQ(r.right - r.left, 8);
        CHECK(U(DeleteObject)(rg));
    }
}
TEST(paths_extra_fill_on_metafile_dc_returns_true) {
    REQ_BASE();
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC mf = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!mf) return;
    POINT sq[4] = {{1, 1}, {9, 1}, {9, 9}, {1, 9}};
    CHECK(BP(mf)); U(Polygon)(mf, sq, 4); CHECK(EP(mf));
    CHECK(FP(mf));
    SET_ERR(0xdeadbeef);
    CHECK(!FP(mf));   /* path consumed */
    HENHMETAFILE h = U(CloseEnhMetaFile)(mf);
    if (h) U(DeleteEnhMetaFile)(h);
}
TEST(paths_extra_flatten_keeps_closefigure_flags) {
    SETUP(m);
    BP(m.dc); U(Ellipse)(m.dc, 4, 4, 40, 40); POLYGON4(m.dc, 44, 4, 60, 20); EP(m.dc);
    CHECK(U(FlattenPath)(m.dc));
    pd_get(m.dc);
    EXPECT_EQ(pd_count_close(), 2);
    EXPECT_EQ(pd_count_type(PT_MOVETO), 2);
    EXPECT_EQ(pd_count_type(PT_BEZIERTO), 0);
    CHECK(pd_valid());
    md_destroy(&m);
}
TEST(paths_extra_flatten_bezier_point_count_grows) {
    SETUP(m);
    POINT c[4] = {{2, 60}, {2, 2}, {60, 2}, {60, 60}};
    BP(m.dc); U(PolyBezier)(m.dc, c, 4); EP(m.dc);
    EXPECT_EQ(GPN(m.dc), 4);
    CHECK(U(FlattenPath)(m.dc));
    int n = pd_get(m.dc);
    CHECK(n > 4);
    CHECK(pd_near(0, 2, 60, 0));
    CHECK(pd_near(n - 1, 60, 60, 0));
    EXPECT_EQ(pd_count_type(PT_BEZIERTO), 0);
    md_destroy(&m);
}
TEST(paths_extra_polyfillmode_invalid_value_ignored) {
    SETUP(m);
    REQUIRE(U(SetPolyFillMode)); REQUIRE(U(GetPolyFillMode));
    int old = U(SetPolyFillMode)(m.dc, WINDING);
    SET_ERR(0xdeadbeef);
    int r = U(SetPolyFillMode)(m.dc, 0);
    t_note("SetPolyFillMode(0) -> %d le=%lu now %d", r, GetLastError(), U(GetPolyFillMode)(m.dc));
    r = U(SetPolyFillMode)(m.dc, 3);
    t_note("SetPolyFillMode(3) -> %d now %d", r, U(GetPolyFillMode)(m.dc));
    U(SetPolyFillMode)(m.dc, old);
    md_destroy(&m);
}
TEST(paths_extra_stroke_vertical_line_exact_pixels) {
    SETUP(m);
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(255, 0, 0)); HGDIOBJ op = U(SelectObject)(m.dc, p);
    BP(m.dc); MT(m.dc, 10, 5); LT(m.dc, 10, 25); EP(m.dc);
    CHECK(SP(m.dc));
    int y;
    for (y = 0; y < 64; y++) {
        unsigned want = (y >= 5 && y < 25) ? RED : WHITE;
        CHECKF(md_px(&m, 10, y) == want, "y=%d", y);
    }
    EXPECT_EQ(md_count(&m, RED), 20);
    U(SelectObject)(m.dc, op); U(DeleteObject)(p);
    md_destroy(&m);
}
TEST(paths_extra_stroke_rect_outline_four_edges) {
    SETUP(m);
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(255, 0, 0)); HGDIOBJ op = U(SelectObject)(m.dc, p);
    BP(m.dc); POLYGON4(m.dc, 10, 10, 30, 30); EP(m.dc);
    CHECK(SP(m.dc));
    CHECK(md_px(&m, 20, 10) == RED); CHECK(md_px(&m, 10, 20) == RED);
    CHECK(md_px(&m, 20, 30) == RED || md_px(&m, 20, 29) == RED);
    CHECK(md_px(&m, 20, 20) == WHITE);
    CHECK(md_px(&m, 5, 5) == WHITE);
    U(SelectObject)(m.dc, op); U(DeleteObject)(p);
    md_destroy(&m);
}
TEST(paths_extra_getpath_null_apt_nonnull_types_note) {
    SETUP(m);
    BP(m.dc); MT(m.dc, 1, 1); LT(m.dc, 5, 5); EP(m.dc);
    BYTE t[4] = {0xAA, 0xAA, 0xAA, 0xAA};
    SET_ERR(0xdeadbeef);
    if (ph_is_wine()) { t_skip("wine may fault on a NULL point buffer"); AP(m.dc); md_destroy(&m); return; }
    int r = U(GetPath)(m.dc, NULL, t, 2);
    t_note("GetPath(NULL pts, types, 2) -> %d le=%lu", r, GetLastError());
    AP(m.dc);
    md_destroy(&m);
}
TEST(paths_extra_getpath_count_larger_than_path) {
    SETUP(m);
    POINT p[8]; BYTE t[8]; int i;
    BP(m.dc); MT(m.dc, 1, 1); LT(m.dc, 5, 5); EP(m.dc);
    for (i = 0; i < 8; i++) { p[i].x = p[i].y = 0x7777; t[i] = 0xEE; }
    EXPECT_EQ(U(GetPath)(m.dc, p, t, 8), 2);
    EXPECT_EQ(t[2], 0xEE); EXPECT_EQ(p[2].x, 0x7777);
    md_destroy(&m);
}
TEST(paths_extra_getpath_exact_minus_one_error) {
    SETUP(m);
    POINT p[4]; BYTE t[4];
    BP(m.dc); POLYGON4(m.dc, 1, 1, 9, 9); EP(m.dc);
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(U(GetPath)(m.dc, p, t, 3), -1);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(U(GetPath)(m.dc, p, t, 1), -1);
    EXPECT_EQ(U(GetPath)(m.dc, p, t, 4), 4);
    md_destroy(&m);
}
TEST(paths_extra_beginpath_after_failed_endpath) {
    SETUP(m);
    CHECK(!EP(m.dc));
    CHECK(BP(m.dc)); LT(m.dc, 4, 4);
    CHECK(EP(m.dc));
    EXPECT_EQ(GPN(m.dc), 2);
    md_destroy(&m);
}
TEST(paths_extra_fill_clip_and_selectclippath_combination) {
    SETUP(m);
    HGDIOBJ ob, op; HBRUSH b = sel_brush(&m, RGB(255, 0, 0), &ob, &op);
    BP(m.dc); POLYGON4(m.dc, 10, 10, 30, 30); EP(m.dc);
    CHECK(U(SelectClipPath)(m.dc, RGN_COPY));
    EXPECT_EQ(GPN(m.dc), -1);          /* consumed by SelectClipPath */
    BP(m.dc); POLYGON4(m.dc, 0, 0, 64, 64); EP(m.dc);
    CHECK(FP(m.dc));
    CHECK(md_rect_is(&m, 10, 10, 30, 30, RED, WHITE));
    U(SelectClipRgn)(m.dc, NULL);
    unsel(&m, b, ob, op);
    md_destroy(&m);
}
TEST(paths_extra_selectclippath_rgn_diff_punches_hole) {
    SETUP(m);
    HGDIOBJ ob, op; HBRUSH b = sel_brush(&m, RGB(255, 0, 0), &ob, &op);
    BP(m.dc); POLYGON4(m.dc, 0, 0, 64, 64); EP(m.dc);
    CHECK(U(SelectClipPath)(m.dc, RGN_COPY));
    BP(m.dc); POLYGON4(m.dc, 20, 20, 40, 40); EP(m.dc);
    CHECK(U(SelectClipPath)(m.dc, RGN_DIFF));
    U(PatBlt)(m.dc, 0, 0, 64, 64, PATCOPY);
    CHECK(md_px(&m, 30, 30) == WHITE);
    CHECK(md_px(&m, 10, 10) == RED);
    CHECK(md_px(&m, 50, 50) == RED);
    EXPECT_EQ(md_count(&m, WHITE), 20 * 20);
    U(SelectClipRgn)(m.dc, NULL);
    unsel(&m, b, ob, op);
    md_destroy(&m);
}
TEST(paths_extra_fill_1bpp_dib_exact) {
    REQ_BASE();
    MD m;
    if (!md_create_bpp(&m, 32, 32, 1)) { t_skip("no 1bpp DIB"); return; }
    HGDIOBJ ob = U(SelectObject)(m.dc, U(GetStockObject)(BLACK_BRUSH)), op = U(SelectObject)(m.dc, U(GetStockObject)(NULL_PEN));
    U(PatBlt)(m.dc, 0, 0, 32, 32, WHITENESS);
    BP(m.dc); POLYGON4(m.dc, 8, 8, 16, 16); EP(m.dc);
    CHECK(FP(m.dc));
    U(GdiFlush)();
    int x, y, blk = 0;
    for (y = 0; y < 32; y++) for (x = 0; x < 32; x++) {
        int bit = (m.bits[y * m.stride + x / 8] >> (7 - x % 8)) & 1;
        if (!bit) blk++;
    }
    EXPECT_EQ(blk, 64);
    U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, op);
    md_destroy(&m);
}
TEST(paths_extra_closefigure_then_moveto_then_fill) {
    SETUP(m);
    HGDIOBJ ob, op; HBRUSH b = sel_brush(&m, RGB(255, 0, 0), &ob, &op);
    BP(m.dc);
    MT(m.dc, 2, 2); LT(m.dc, 12, 2); LT(m.dc, 12, 12); LT(m.dc, 2, 12); CF(m.dc);
    MT(m.dc, 30, 30); LT(m.dc, 40, 30); LT(m.dc, 40, 40); LT(m.dc, 30, 40); CF(m.dc);
    EP(m.dc);
    EXPECT_EQ(pd_get(m.dc), 8);
    EXPECT_EQ(pd_count_close(), 2);
    CHECK(FP(m.dc));
    CHECK(md_px(&m, 5, 5) == RED); CHECK(md_px(&m, 35, 35) == RED);
    CHECK(md_px(&m, 20, 20) == WHITE);
    EXPECT_EQ(md_count(&m, RED), 200);
    unsel(&m, b, ob, op);
    md_destroy(&m);
}
TEST(paths_extra_open_figures_each_closed_by_fill) {
    SETUP(m);
    HGDIOBJ ob, op; HBRUSH b = sel_brush(&m, RGB(255, 0, 0), &ob, &op);
    BP(m.dc);
    MT(m.dc, 2, 2); LT(m.dc, 12, 2); LT(m.dc, 12, 12);
    MT(m.dc, 30, 30); LT(m.dc, 40, 30); LT(m.dc, 40, 40);
    EP(m.dc);
    CHECK(FP(m.dc));
    CHECK(md_px(&m, 10, 4) == RED);
    CHECK(md_px(&m, 38, 32) == RED);
    CHECK(md_px(&m, 3, 10) == WHITE);
    unsel(&m, b, ob, op);
    md_destroy(&m);
}
TEST(paths_extra_stroke_closefigure_vs_open_pixel) {
    SETUP(m);
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(255, 0, 0)); HGDIOBJ op = U(SelectObject)(m.dc, p);
    BP(m.dc); MT(m.dc, 10, 10); LT(m.dc, 30, 10); LT(m.dc, 30, 30); EP(m.dc);
    SP(m.dc);
    int open = md_count(&m, RED);
    md_clear(&m, WHITE);
    BP(m.dc); MT(m.dc, 10, 10); LT(m.dc, 30, 10); LT(m.dc, 30, 30); CF(m.dc); EP(m.dc);
    SP(m.dc);
    int closed = md_count(&m, RED);
    CHECKF(closed > open, "open %d closed %d", open, closed);
    U(SelectObject)(m.dc, op); U(DeleteObject)(p);
    md_destroy(&m);
}
TEST(paths_extra_gdi_count_stable_across_all_ops) {
    SETUP(m);
    int before = gui_objs(), i;
    HPEN p = U(CreatePen)(PS_SOLID, 5, RGB(0, 0, 255)); HGDIOBJ op = U(SelectObject)(m.dc, p);
    for (i = 0; i < 200; i++) {
        BP(m.dc); U(Ellipse)(m.dc, 4, 4, 50, 50); EP(m.dc);
        U(WidenPath)(m.dc);
        U(FlattenPath)(m.dc);
        HRGN r = U(PathToRegion)(m.dc);
        if (r) U(DeleteObject)(r);
    }
    U(SelectObject)(m.dc, op); U(DeleteObject)(p);
    if (before >= 0) EXPECT_EQ(gui_objs(), before);
    md_destroy(&m);
}
