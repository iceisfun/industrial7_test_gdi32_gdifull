/* FillPath / StrokePath / StrokeAndFillPath / PathToRegion / SelectClipPath on DIB-section memory DCs. */
#include "ph.h"

#define PX(r, g, b) (((unsigned)(r) << 16) | ((unsigned)(g) << 8) | (unsigned)(b))
#define RED   PX(255, 0, 0)
#define BLUE  PX(0, 0, 255)
#define GREEN PX(0, 255, 0)
#define BLACK 0u
#define SETUP(m) MD m; REQ_BASE(); if (!md_create(&m, 32, 32)) { t_skip("no DIB"); return; } md_clear(&m, WHITE)

typedef struct { HGDIOBJ op, ob; HPEN pen; HBRUSH br; HDC dc; } SEL;
static void sel_set(SEL *s, HDC dc, HPEN pen, HBRUSH br) {
    s->dc = dc; s->pen = pen; s->br = br; s->op = s->ob = NULL;
    if (pen) s->op = U(SelectObject)(dc, pen);
    if (br) s->ob = U(SelectObject)(dc, br);
}
static void sel_end(SEL *s) {
    if (s->op) U(SelectObject)(s->dc, s->op);
    if (s->ob) U(SelectObject)(s->dc, s->ob);
    if (s->pen) U(DeleteObject)(s->pen);
    if (s->br) U(DeleteObject)(s->br);
}
static void fill_red(MD *m, SEL *s) { sel_set(s, m->dc, (HPEN)U(GetStockObject)(NULL_PEN), U(CreateSolidBrush)(RGB(255, 0, 0))); }

/* ---- FillPath -------------------------------------------------------- */
TEST(paths_fill_polygon_square_exact) {
    SETUP(m); SEL s; fill_red(&m, &s);
    BP(m.dc); POLYGON4(m.dc, 4, 4, 12, 12); EP(m.dc);
    CHECK(FP(m.dc));
    CHECK(md_rect_is(&m, 4, 4, 12, 12, RED, WHITE));
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_polygon_square_offsets) {
    SETUP(m); SEL s; fill_red(&m, &s);
    int rc[][4] = {{0, 0, 1, 1}, {0, 0, 32, 32}, {5, 7, 6, 20}, {10, 10, 11, 11}, {1, 1, 31, 3}, {2, 28, 30, 32}};
    unsigned i;
    for (i = 0; i < sizeof rc / sizeof rc[0]; i++) {
        md_clear(&m, WHITE);
        BP(m.dc); POLYGON4(m.dc, rc[i][0], rc[i][1], rc[i][2], rc[i][3]); EP(m.dc);
        CHECK(FP(m.dc));
        CHECKF(md_rect_is(&m, rc[i][0], rc[i][1], rc[i][2], rc[i][3], RED, WHITE), "rect %u", i);
    }
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_polygon_orientation_independent) {
    SETUP(m); SEL s; fill_red(&m, &s);
    POINT cw[4] = {{4, 4}, {12, 4}, {12, 12}, {4, 12}}, ccw[4] = {{4, 4}, {4, 12}, {12, 12}, {12, 4}};
    BP(m.dc); U(Polygon)(m.dc, cw, 4); EP(m.dc); CHECK(FP(m.dc));
    CHECK(md_rect_is(&m, 4, 4, 12, 12, RED, WHITE));
    md_clear(&m, WHITE);
    BP(m.dc); U(Polygon)(m.dc, ccw, 4); EP(m.dc); CHECK(FP(m.dc));
    CHECK(md_rect_is(&m, 4, 4, 12, 12, RED, WHITE));
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_triangle) {
    SETUP(m); SEL s; fill_red(&m, &s);
    POINT t[3] = {{0, 0}, {16, 0}, {0, 16}};
    BP(m.dc); U(Polygon)(m.dc, t, 3); EP(m.dc);
    CHECK(FP(m.dc));
    int n = md_count(&m, RED);
    CHECKF(n >= 110 && n <= 140, "n=%d", n);
    CHECK(md_px(&m, 1, 1) == RED); CHECK(md_px(&m, 14, 14) == WHITE); CHECK(md_px(&m, 20, 20) == WHITE);
    CHECK(md_px(&m, 0, 15) == RED || md_px(&m, 0, 14) == RED);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_open_polyline_is_closed) {
    SETUP(m); SEL s; fill_red(&m, &s);
    POINT t[3] = {{2, 2}, {20, 2}, {2, 20}};
    BP(m.dc); U(Polyline)(m.dc, t, 3); EP(m.dc);
    CHECK(FP(m.dc));
    CHECK(md_px(&m, 5, 5) == RED);
    CHECK(md_px(&m, 18, 18) == WHITE);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_open_lines_equal_closed_polygon) {
    SETUP(m); SEL s; fill_red(&m, &s);
    MD m2; if (!md_create(&m2, 32, 32)) { sel_end(&s); md_destroy(&m); return; }
    md_clear(&m2, WHITE);
    SEL s2; fill_red(&m2, &s2);
    POINT t[4] = {{3, 3}, {25, 5}, {20, 25}, {4, 18}};
    BP(m.dc); U(Polyline)(m.dc, t, 4); EP(m.dc); FP(m.dc);
    BP(m2.dc); U(Polygon)(m2.dc, t, 4); EP(m2.dc); FP(m2.dc);
    CHECK(md_same(&m, &m2));
    sel_end(&s2); md_destroy(&m2);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_rectangle_path_extent) {
    SETUP(m); SEL s; fill_red(&m, &s);
    BP(m.dc); U(Rectangle)(m.dc, 4, 4, 12, 12); EP(m.dc);
    CHECK(FP(m.dc));
    RECT r; int n = md_bbox(&m, WHITE, &r);
    t_note("Rectangle path fill bbox %ld,%ld,%ld,%ld (%d px)", r.left, r.top, r.right, r.bottom, n);
    CHECK(r.left >= 3 && r.left <= 4 && r.top >= 3 && r.top <= 4);
    CHECK(r.right >= 11 && r.right <= 13 && r.bottom >= 11 && r.bottom <= 13);
    CHECK(md_px(&m, 8, 8) == RED);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_rectangle_equals_rectangle_with_null_pen) {
    SETUP(m); SEL s; fill_red(&m, &s);
    MD m2; if (!md_create(&m2, 32, 32)) { sel_end(&s); md_destroy(&m); return; }
    md_clear(&m2, WHITE);
    SEL s2; fill_red(&m2, &s2);
    BP(m.dc); U(Rectangle)(m.dc, 4, 4, 12, 12); EP(m.dc); FP(m.dc);
    U(Rectangle)(m2.dc, 4, 4, 12, 12);
    RECT a, b; int na = md_bbox(&m, WHITE, &a), nb = md_bbox(&m2, WHITE, &b);
    t_note("fillpath rect %d px %ld,%ld,%ld,%ld vs direct %d px %ld,%ld,%ld,%ld", na, a.left, a.top, a.right, a.bottom, nb, b.left, b.top, b.right, b.bottom);
    CHECK(abs(na - nb) <= 17);
    sel_end(&s2); md_destroy(&m2);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_ellipse_path) {
    SETUP(m); SEL s; fill_red(&m, &s);
    BP(m.dc); U(Ellipse)(m.dc, 4, 4, 28, 28); EP(m.dc);
    CHECK(FP(m.dc));
    int n = md_count(&m, RED);
    CHECKF(n >= 400 && n <= 480, "n=%d", n);   /* pi*12^2 = 452 */
    CHECK(md_px(&m, 16, 16) == RED); CHECK(md_px(&m, 5, 5) == WHITE); CHECK(md_px(&m, 26, 26) == WHITE);
    CHECK(md_px(&m, 16, 5) == RED); CHECK(md_px(&m, 5, 16) == RED);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_ellipse_left_right_symmetry) {
    SETUP(m); SEL s; fill_red(&m, &s);
    BP(m.dc); U(Ellipse)(m.dc, 4, 4, 28, 28); EP(m.dc); FP(m.dc);
    int y, x, asym = 0;
    for (y = 0; y < 32; y++) for (x = 0; x < 32; x++) if ((md_px(&m, x, y) == RED) != (md_px(&m, 31 - x, y) == RED)) asym++;
    if (asym) t_note("ellipse fill asymmetric pixels: %d", asym);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_roundrect_path_corner_cut) {
    SETUP(m); SEL s; fill_red(&m, &s);
    BP(m.dc); U(RoundRect)(m.dc, 4, 4, 28, 28, 12, 12); EP(m.dc);
    CHECK(FP(m.dc));
    CHECK(md_px(&m, 16, 16) == RED); CHECK(md_px(&m, 4, 4) == WHITE); CHECK(md_px(&m, 27, 4) == WHITE);
    CHECK(md_px(&m, 16, 4) == RED); CHECK(md_px(&m, 4, 16) == RED);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_pie_and_chord) {
    SETUP(m); SEL s; fill_red(&m, &s);
    REQUIRE(U(Pie)); REQUIRE(U(Chord)); REQUIRE(U(SetArcDirection));
    BP(m.dc); U(Pie)(m.dc, 2, 2, 30, 30, 30, 16, 16, 2); EP(m.dc); CHECK(FP(m.dc));
    CHECK(md_px(&m, 20, 10) == RED);      /* inside the upper-right quarter */
    CHECK(md_px(&m, 8, 24) == WHITE);     /* lower left is outside the wedge */
    md_clear(&m, WHITE);
    BP(m.dc); U(Chord)(m.dc, 2, 2, 30, 30, 30, 16, 16, 2); EP(m.dc); CHECK(FP(m.dc));
    CHECK(md_px(&m, 24, 8) == RED);
    CHECK(md_px(&m, 16, 16) == WHITE);    /* the centre is on the far side of the chord */
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_alternate_same_orientation_hole) {
    SETUP(m); SEL s; fill_red(&m, &s);
    U(SetPolyFillMode)(m.dc, ALTERNATE);
    BP(m.dc); POLYGON4(m.dc, 2, 2, 30, 30); POLYGON4(m.dc, 10, 10, 22, 22); EP(m.dc);
    CHECK(FP(m.dc));
    CHECK(md_px(&m, 4, 4) == RED); CHECK(md_px(&m, 16, 16) == WHITE); CHECK(md_px(&m, 11, 11) == WHITE);
    CHECK(md_px(&m, 9, 16) == RED);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_winding_same_orientation_solid) {
    SETUP(m); SEL s; fill_red(&m, &s);
    U(SetPolyFillMode)(m.dc, WINDING);
    BP(m.dc); POLYGON4(m.dc, 2, 2, 30, 30); POLYGON4(m.dc, 10, 10, 22, 22); EP(m.dc);
    CHECK(FP(m.dc));
    CHECK(md_rect_is(&m, 2, 2, 30, 30, RED, WHITE));
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_winding_opposite_orientation_hole) {
    SETUP(m); SEL s; fill_red(&m, &s);
    POINT outer[4] = {{2, 2}, {30, 2}, {30, 30}, {2, 30}}, inner[4] = {{10, 10}, {10, 22}, {22, 22}, {22, 10}};
    U(SetPolyFillMode)(m.dc, WINDING);
    BP(m.dc); U(Polygon)(m.dc, outer, 4); U(Polygon)(m.dc, inner, 4); EP(m.dc);
    CHECK(FP(m.dc));
    CHECK(md_px(&m, 4, 4) == RED); CHECK(md_px(&m, 16, 16) == WHITE);
    md_clear(&m, WHITE);
    U(SetPolyFillMode)(m.dc, ALTERNATE);
    BP(m.dc); U(Polygon)(m.dc, outer, 4); U(Polygon)(m.dc, inner, 4); EP(m.dc);
    CHECK(FP(m.dc));
    CHECK(md_px(&m, 4, 4) == RED); CHECK(md_px(&m, 16, 16) == WHITE);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_star_winding_vs_alternate) {
    SETUP(m); SEL s; fill_red(&m, &s);
    POINT st[5] = {{16, 2}, {26, 30}, {2, 12}, {30, 12}, {6, 30}};
    U(SetPolyFillMode)(m.dc, ALTERNATE);
    BP(m.dc); U(Polygon)(m.dc, st, 5); EP(m.dc); FP(m.dc);
    int alt = md_count(&m, RED);
    CHECK(md_px(&m, 16, 16) == WHITE);        /* pentagon in the centre is a hole */
    md_clear(&m, WHITE);
    U(SetPolyFillMode)(m.dc, WINDING);
    BP(m.dc); U(Polygon)(m.dc, st, 5); EP(m.dc); FP(m.dc);
    int wind = md_count(&m, RED);
    CHECK(md_px(&m, 16, 16) == RED);
    CHECK(wind > alt);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_overlapping_rects_alternate) {
    SETUP(m); SEL s; fill_red(&m, &s);
    U(SetPolyFillMode)(m.dc, ALTERNATE);
    BP(m.dc); POLYGON4(m.dc, 2, 2, 18, 18); POLYGON4(m.dc, 10, 10, 28, 28); EP(m.dc); FP(m.dc);
    CHECK(md_px(&m, 5, 5) == RED); CHECK(md_px(&m, 24, 24) == RED);
    CHECK(md_px(&m, 14, 14) == WHITE);      /* overlap cancels */
    CHECK(md_px(&m, 24, 5) == WHITE);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_overlapping_rects_winding) {
    SETUP(m); SEL s; fill_red(&m, &s);
    U(SetPolyFillMode)(m.dc, WINDING);
    BP(m.dc); POLYGON4(m.dc, 2, 2, 18, 18); POLYGON4(m.dc, 10, 10, 28, 28); EP(m.dc); FP(m.dc);
    CHECK(md_px(&m, 14, 14) == RED); CHECK(md_px(&m, 5, 5) == RED); CHECK(md_px(&m, 24, 24) == RED);
    CHECK(md_px(&m, 24, 5) == WHITE);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_two_disjoint_figures) {
    SETUP(m); SEL s; fill_red(&m, &s);
    BP(m.dc); POLYGON4(m.dc, 1, 1, 8, 8); POLYGON4(m.dc, 20, 20, 30, 28); EP(m.dc); FP(m.dc);
    CHECK(md_px(&m, 4, 4) == RED); CHECK(md_px(&m, 25, 24) == RED);
    CHECK(md_px(&m, 15, 15) == WHITE);
    EXPECT_EQ(md_count(&m, RED), 7 * 7 + 10 * 8);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_fillmode_read_at_fill_time) {
    SETUP(m); SEL s; fill_red(&m, &s);
    U(SetPolyFillMode)(m.dc, WINDING);
    BP(m.dc); POLYGON4(m.dc, 2, 2, 30, 30); POLYGON4(m.dc, 10, 10, 22, 22); EP(m.dc);
    U(SetPolyFillMode)(m.dc, ALTERNATE);
    FP(m.dc);
    int hole = md_px(&m, 16, 16) == WHITE;
    t_note("fill mode changed between EndPath and FillPath: %s", hole ? "ALTERNATE used (hole)" : "WINDING used (solid)");
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_null_brush_draws_nothing) {
    SETUP(m); SEL s;
    sel_set(&s, m.dc, (HPEN)U(GetStockObject)(NULL_PEN), (HBRUSH)U(GetStockObject)(NULL_BRUSH));
    s.pen = NULL; s.br = NULL;
    BP(m.dc); POLYGON4(m.dc, 4, 4, 20, 20); EP(m.dc);
    CHECK(FP(m.dc));
    EXPECT_EQ(md_count_not(&m, WHITE), 0);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_ignores_pen) {
    SETUP(m); SEL s;
    sel_set(&s, m.dc, U(CreatePen)(PS_SOLID, 5, RGB(0, 0, 255)), U(CreateSolidBrush)(RGB(255, 0, 0)));
    BP(m.dc); POLYGON4(m.dc, 8, 8, 24, 24); EP(m.dc); FP(m.dc);
    EXPECT_EQ(md_count(&m, BLUE), 0);
    CHECK(md_rect_is(&m, 8, 8, 24, 24, RED, WHITE));
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_brush_at_fill_time) {
    SETUP(m); SEL s;
    sel_set(&s, m.dc, (HPEN)U(GetStockObject)(NULL_PEN), U(CreateSolidBrush)(RGB(255, 0, 0)));
    HBRUSH g = U(CreateSolidBrush)(RGB(0, 255, 0));
    BP(m.dc); POLYGON4(m.dc, 8, 8, 24, 24); EP(m.dc);
    U(SelectObject)(m.dc, g);       /* change brush after the path was built */
    FP(m.dc);
    CHECK(md_px(&m, 12, 12) == GREEN);
    U(SelectObject)(m.dc, s.br);
    U(DeleteObject)(g);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_dc_brush) {
    SETUP(m); SEL s;
    REQUIRE(U(SetDCBrushColor));
    sel_set(&s, m.dc, (HPEN)U(GetStockObject)(NULL_PEN), NULL);
    s.pen = NULL;
    HGDIOBJ ob = U(SelectObject)(m.dc, U(GetStockObject)(DC_BRUSH));
    U(SetDCBrushColor)(m.dc, RGB(0, 255, 0));
    BP(m.dc); POLYGON4(m.dc, 8, 8, 24, 24); EP(m.dc); FP(m.dc);
    CHECK(md_rect_is(&m, 8, 8, 24, 24, GREEN, WHITE));
    U(SelectObject)(m.dc, ob);
    U(SelectObject)(m.dc, s.op);
    md_destroy(&m);
}
TEST(paths_fill_rop2) {
    SETUP(m); SEL s; fill_red(&m, &s);
    U(SetROP2)(m.dc, R2_NOT);
    BP(m.dc); POLYGON4(m.dc, 8, 8, 24, 24); EP(m.dc); FP(m.dc);
    CHECK(md_px(&m, 12, 12) == BLACK);      /* NOT of white */
    U(SetROP2)(m.dc, R2_BLACK);
    BP(m.dc); POLYGON4(m.dc, 0, 0, 4, 4); EP(m.dc); FP(m.dc);
    CHECK(md_px(&m, 2, 2) == BLACK);
    U(SetROP2)(m.dc, R2_WHITE);
    BP(m.dc); POLYGON4(m.dc, 8, 8, 24, 24); EP(m.dc); FP(m.dc);
    CHECK(md_px(&m, 12, 12) == WHITE);
    U(SetROP2)(m.dc, R2_COPYPEN);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_honours_clip_region) {
    SETUP(m); SEL s; fill_red(&m, &s);
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(SelectClipRgn));
    HRGN c = U(CreateRectRgn)(10, 10, 20, 20);
    U(SelectClipRgn)(m.dc, c);
    BP(m.dc); POLYGON4(m.dc, 2, 2, 30, 30); EP(m.dc); FP(m.dc);
    CHECK(md_rect_is(&m, 10, 10, 20, 20, RED, WHITE));
    U(SelectClipRgn)(m.dc, NULL);
    U(DeleteObject)(c);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_clipped_at_bitmap_edges) {
    SETUP(m); SEL s; fill_red(&m, &s);
    BP(m.dc); POLYGON4(m.dc, -20, -20, 10, 10); POLYGON4(m.dc, 25, 25, 60, 60); EP(m.dc);
    CHECK(FP(m.dc));
    CHECK(md_px(&m, 0, 0) == RED); CHECK(md_px(&m, 9, 9) == RED); CHECK(md_px(&m, 10, 10) == WHITE);
    CHECK(md_px(&m, 31, 31) == RED); CHECK(md_px(&m, 24, 24) == WHITE);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_huge_coordinates) {
    SETUP(m); SEL s; fill_red(&m, &s);
    BP(m.dc); POLYGON4(m.dc, -100000, -100000, 100000, 100000); EP(m.dc);
    CHECK(FP(m.dc));
    CHECK(md_px(&m, 0, 0) == RED); CHECK(md_px(&m, 31, 31) == RED);
    EXPECT_EQ(md_count(&m, RED), 32 * 32);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_empty_path_ok) {
    SETUP(m); SEL s; fill_red(&m, &s);
    BP(m.dc); EP(m.dc);
    CHECK(FP(m.dc));
    EXPECT_EQ(md_count_not(&m, WHITE), 0);
    EXPECT_EQ(GPN(m.dc), -1);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_single_line_path_no_area) {
    SETUP(m); SEL s; fill_red(&m, &s);
    BP(m.dc); MT(m.dc, 2, 2); LT(m.dc, 28, 2); EP(m.dc);
    CHECK(FP(m.dc));
    EXPECT_EQ(md_count_not(&m, WHITE), 0);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_consumes_path) {
    SETUP(m); SEL s; fill_red(&m, &s);
    BP(m.dc); POLYGON4(m.dc, 4, 4, 12, 12); EP(m.dc);
    CHECK(FP(m.dc));
    EXPECT_EQ(GPN(m.dc), -1);
    SET_ERR(0xdeadbeef);
    CHECK(!FP(m.dc));
    t_note("FillPath twice le=%lu", GetLastError());
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_open_bracket_fails) {
    SETUP(m); SEL s; fill_red(&m, &s);
    BP(m.dc); POLYGON4(m.dc, 4, 4, 12, 12);
    SET_ERR(0xdeadbeef);
    CHECK(!FP(m.dc));
    t_note("FillPath in open bracket le=%lu", GetLastError());
    EXPECT_EQ(md_count_not(&m, WHITE), 0);
    AP(m.dc);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_no_path_fails) {
    SETUP(m); SEL s; fill_red(&m, &s);
    SET_ERR(0xdeadbeef);
    CHECK(!FP(m.dc));
    EXPECT_ERR(0, ERROR_CAN_NOT_COMPLETE);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_after_failed_fill_path_still_closed) {
    SETUP(m); SEL s; fill_red(&m, &s);
    BP(m.dc); POLYGON4(m.dc, 4, 4, 12, 12); EP(m.dc);
    int n = GPN(m.dc);
    CHECK(FP(m.dc));
    CHECK(n == 4);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_repeated_cycles_same_result) {
    SETUP(m); SEL s; fill_red(&m, &s);
    int i, ok = 1;
    for (i = 0; i < 200; i++) {
        md_clear(&m, WHITE);
        BP(m.dc); POLYGON4(m.dc, 3, 3, 13, 9); EP(m.dc);
        if (!FP(m.dc)) ok = 0;
        if (md_count(&m, RED) != 60) { ok = 0; break; }
    }
    CHECK(ok);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_fill_by_bit_depth) {
    REQ_BASE();
    int depths[] = {1, 8, 24, 32};
    unsigned i;
    for (i = 0; i < sizeof depths / sizeof depths[0]; i++) {
        MD m;
        if (!md_create_bpp(&m, 32, 32, depths[i])) { t_note("no %dbpp DIB", depths[i]); continue; }
        U(PatBlt)(m.dc, 0, 0, 32, 32, WHITENESS);
        HGDIOBJ op = U(SelectObject)(m.dc, U(GetStockObject)(NULL_PEN));
        HGDIOBJ ob = U(SelectObject)(m.dc, U(GetStockObject)(BLACK_BRUSH));
        BP(m.dc); POLYGON4(m.dc, 4, 4, 12, 12); EP(m.dc);
        CHECKF(FP(m.dc), "%dbpp", depths[i]);
        int x, y, bad = 0;
        for (y = 0; y < 32; y++) for (x = 0; x < 32; x++) {
            COLORREF c = U(GetPixel)(m.dc, x, y);
            COLORREF want = (x >= 4 && x < 12 && y >= 4 && y < 12) ? RGB(0, 0, 0) : RGB(255, 255, 255);
            if (c != want) bad++;
        }
        CHECKF(bad == 0, "%dbpp: %d bad pixels", depths[i], bad);
        U(SelectObject)(m.dc, op); U(SelectObject)(m.dc, ob);
        md_destroy(&m);
    }
}
TEST(paths_fill_16bpp_555) {
    REQ_BASE();
    MD m;
    if (!md_create_bpp(&m, 32, 32, 16)) { t_skip("no 16bpp DIB"); return; }
    U(PatBlt)(m.dc, 0, 0, 32, 32, WHITENESS);
    HBRUSH b = U(CreateSolidBrush)(RGB(255, 0, 0));
    HGDIOBJ op = U(SelectObject)(m.dc, U(GetStockObject)(NULL_PEN)), ob = U(SelectObject)(m.dc, b);
    BP(m.dc); POLYGON4(m.dc, 4, 4, 12, 12); EP(m.dc);
    CHECK(FP(m.dc));
    CHECK(U(GetPixel)(m.dc, 8, 8) == RGB(255, 0, 0));
    CHECK(U(GetPixel)(m.dc, 2, 2) == RGB(255, 255, 255));
    U(SelectObject)(m.dc, op); U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    md_destroy(&m);
}

/* ---- StrokePath ------------------------------------------------------ */
static void stroke_pen(MD *m, SEL *s, COLORREF c) { sel_set(s, m->dc, U(CreatePen)(PS_SOLID, 1, c), (HBRUSH)U(GetStockObject)(NULL_BRUSH)); s->br = NULL; }
TEST(paths_stroke_hline_matches_lineto) {
    SETUP(m); SEL s; stroke_pen(&m, &s, RGB(255, 0, 0));
    MD m2; if (!md_create(&m2, 32, 32)) { sel_end(&s); md_destroy(&m); return; }
    md_clear(&m2, WHITE);
    SEL s2; stroke_pen(&m2, &s2, RGB(255, 0, 0));
    BP(m.dc); MT(m.dc, 3, 6); LT(m.dc, 20, 6); EP(m.dc);
    CHECK(SP(m.dc));
    MT(m2.dc, 3, 6); LT(m2.dc, 20, 6);
    CHECK(md_same(&m, &m2));
    CHECK(md_px(&m, 3, 6) == RED); CHECK(md_px(&m, 10, 6) == RED);
    int n = md_count(&m, RED);
    CHECKF(n == 17 || n == 18, "n=%d", n);
    sel_end(&s2); md_destroy(&m2);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_stroke_vline_matches_lineto) {
    SETUP(m); SEL s; stroke_pen(&m, &s, RGB(255, 0, 0));
    MD m2; if (!md_create(&m2, 32, 32)) { sel_end(&s); md_destroy(&m); return; }
    md_clear(&m2, WHITE);
    SEL s2; stroke_pen(&m2, &s2, RGB(255, 0, 0));
    BP(m.dc); MT(m.dc, 6, 3); LT(m.dc, 6, 25); EP(m.dc);
    CHECK(SP(m.dc));
    MT(m2.dc, 6, 3); LT(m2.dc, 6, 25);
    CHECK(md_same(&m, &m2));
    sel_end(&s2); md_destroy(&m2);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_stroke_diagonal_matches_lineto) {
    SETUP(m); SEL s; stroke_pen(&m, &s, RGB(255, 0, 0));
    MD m2; if (!md_create(&m2, 32, 32)) { sel_end(&s); md_destroy(&m); return; }
    md_clear(&m2, WHITE);
    SEL s2; stroke_pen(&m2, &s2, RGB(255, 0, 0));
    BP(m.dc); MT(m.dc, 2, 2); LT(m.dc, 20, 20); EP(m.dc);
    CHECK(SP(m.dc));
    MT(m2.dc, 2, 2); LT(m2.dc, 20, 20);
    int na = md_count(&m, RED), nb = md_count(&m2, RED);
    if (!md_same(&m, &m2)) t_note("diagonal StrokePath differs from LineTo: %d vs %d px", na, nb);
    CHECK(abs(na - nb) <= 2);
    CHECK(md_px(&m, 10, 10) == RED);
    sel_end(&s2); md_destroy(&m2);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_stroke_only_pen_color_present) {
    SETUP(m); SEL s; stroke_pen(&m, &s, RGB(255, 0, 0));
    POINT t[4] = {{3, 3}, {25, 5}, {20, 25}, {4, 18}};
    BP(m.dc); U(Polygon)(m.dc, t, 4); EP(m.dc);
    CHECK(SP(m.dc));
    int red = md_count(&m, RED), white = md_count(&m, WHITE);
    EXPECT_EQ(red + white, 32 * 32);
    CHECK(red > 60);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_stroke_does_not_fill) {
    SETUP(m); SEL s;
    sel_set(&s, m.dc, U(CreatePen)(PS_SOLID, 1, RGB(255, 0, 0)), U(CreateSolidBrush)(RGB(0, 255, 0)));
    BP(m.dc); POLYGON4(m.dc, 4, 4, 20, 20); EP(m.dc);
    CHECK(SP(m.dc));
    EXPECT_EQ(md_count(&m, GREEN), 0);
    CHECK(md_px(&m, 12, 12) == WHITE);
    CHECK(md_px(&m, 4, 12) == RED); CHECK(md_px(&m, 12, 4) == RED);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_stroke_open_vs_closed_figure) {
    SETUP(m); SEL s; stroke_pen(&m, &s, RGB(255, 0, 0));
    POINT t[4] = {{4, 4}, {20, 4}, {20, 20}, {4, 20}};
    BP(m.dc); U(Polyline)(m.dc, t, 4); EP(m.dc); SP(m.dc);
    CHECK(md_px(&m, 4, 12) == WHITE);        /* open figure: left edge missing */
    md_clear(&m, WHITE);
    BP(m.dc); U(Polygon)(m.dc, t, 4); EP(m.dc); SP(m.dc);
    CHECK(md_px(&m, 4, 12) == RED);          /* Polygon closes it */
    md_clear(&m, WHITE);
    BP(m.dc); U(Polyline)(m.dc, t, 4); CF(m.dc); EP(m.dc); SP(m.dc);
    CHECK(md_px(&m, 4, 12) == RED);          /* CloseFigure closes it */
    sel_end(&s); md_destroy(&m);
}
TEST(paths_stroke_null_pen_draws_nothing) {
    SETUP(m); SEL s;
    sel_set(&s, m.dc, (HPEN)U(GetStockObject)(NULL_PEN), NULL); s.pen = NULL;
    BP(m.dc); POLYGON4(m.dc, 4, 4, 20, 20); EP(m.dc);
    BOOL r = SP(m.dc);
    t_note("StrokePath with NULL_PEN -> %d", r);
    EXPECT_EQ(md_count_not(&m, WHITE), 0);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_stroke_consumes_path) {
    SETUP(m); SEL s; stroke_pen(&m, &s, RGB(255, 0, 0));
    BP(m.dc); MT(m.dc, 2, 2); LT(m.dc, 20, 2); EP(m.dc);
    CHECK(SP(m.dc));
    EXPECT_EQ(GPN(m.dc), -1);
    CHECK(!SP(m.dc));
    sel_end(&s); md_destroy(&m);
}
TEST(paths_stroke_open_bracket_and_none_fail) {
    SETUP(m); SEL s; stroke_pen(&m, &s, RGB(255, 0, 0));
    SET_ERR(0xdeadbeef);
    CHECK(!SP(m.dc));
    EXPECT_ERR(0, ERROR_CAN_NOT_COMPLETE);
    BP(m.dc); MT(m.dc, 2, 2); LT(m.dc, 20, 2);
    CHECK(!SP(m.dc));
    EXPECT_EQ(md_count_not(&m, WHITE), 0);
    AP(m.dc);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_stroke_empty_path_ok) {
    SETUP(m); SEL s; stroke_pen(&m, &s, RGB(255, 0, 0));
    BP(m.dc); EP(m.dc);
    CHECK(SP(m.dc));
    EXPECT_EQ(md_count_not(&m, WHITE), 0);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_stroke_dash_pen_has_gaps) {
    SETUP(m); SEL s;
    sel_set(&s, m.dc, U(CreatePen)(PS_DASH, 1, RGB(255, 0, 0)), NULL); s.br = NULL;
    BP(m.dc); MT(m.dc, 0, 5); LT(m.dc, 31, 5); EP(m.dc);
    CHECK(SP(m.dc));
    int n = md_count(&m, RED);
    CHECKF(n > 8 && n < 31, "dash pixels %d", n);
    CHECK(md_px(&m, 0, 5) == RED);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_stroke_dot_pen_sparser_than_dash) {
    SETUP(m); SEL s;
    sel_set(&s, m.dc, U(CreatePen)(PS_DOT, 1, RGB(255, 0, 0)), NULL); s.br = NULL;
    BP(m.dc); MT(m.dc, 0, 5); LT(m.dc, 31, 5); EP(m.dc); SP(m.dc);
    int dot = md_count(&m, RED);
    CHECKF(dot > 4 && dot < 31, "dot pixels %d", dot);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_stroke_geometric_wide_line) {
    SETUP(m); SEL s;
    REQUIRE(U(ExtCreatePen));
    LOGBRUSH lb = {BS_SOLID, RGB(255, 0, 0), 0};
    sel_set(&s, m.dc, U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_FLAT, 5, &lb, 0, NULL), NULL); s.br = NULL;
    BP(m.dc); MT(m.dc, 4, 16); LT(m.dc, 28, 16); EP(m.dc);
    CHECK(SP(m.dc));
    CHECK(md_px(&m, 16, 16) == RED);
    CHECK(md_px(&m, 16, 14) == RED); CHECK(md_px(&m, 16, 18) == RED);
    CHECK(md_px(&m, 16, 12) == WHITE); CHECK(md_px(&m, 16, 20) == WHITE);
    RECT r; md_bbox(&m, WHITE, &r);
    t_note("5px geometric flat line bbox %ld,%ld,%ld,%ld", r.left, r.top, r.right, r.bottom);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_stroke_geometric_round_cap_extends) {
    SETUP(m); SEL s;
    REQUIRE(U(ExtCreatePen));
    LOGBRUSH lb = {BS_SOLID, RGB(255, 0, 0), 0};
    sel_set(&s, m.dc, U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND, 9, &lb, 0, NULL), NULL); s.br = NULL;
    BP(m.dc); MT(m.dc, 10, 16); LT(m.dc, 22, 16); EP(m.dc);
    CHECK(SP(m.dc));
    CHECK(md_px(&m, 7, 16) == RED);        /* cap sticks out past the end point */
    CHECK(md_px(&m, 25, 16) == RED);
    CHECK(md_px(&m, 4, 16) == WHITE);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_stroke_geometric_flat_cap_does_not_extend) {
    SETUP(m); SEL s;
    REQUIRE(U(ExtCreatePen));
    LOGBRUSH lb = {BS_SOLID, RGB(255, 0, 0), 0};
    sel_set(&s, m.dc, U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_FLAT, 9, &lb, 0, NULL), NULL); s.br = NULL;
    BP(m.dc); MT(m.dc, 10, 16); LT(m.dc, 22, 16); EP(m.dc);
    CHECK(SP(m.dc));
    CHECK(md_px(&m, 8, 16) == WHITE); CHECK(md_px(&m, 24, 16) == WHITE);
    CHECK(md_px(&m, 16, 16) == RED);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_stroke_square_cap_extends_flat_does_not) {
    SETUP(m); SEL s;
    REQUIRE(U(ExtCreatePen));
    LOGBRUSH lb = {BS_SOLID, RGB(255, 0, 0), 0};
    sel_set(&s, m.dc, U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_SQUARE, 8, &lb, 0, NULL), NULL); s.br = NULL;
    BP(m.dc); MT(m.dc, 10, 16); LT(m.dc, 22, 16); EP(m.dc);
    CHECK(SP(m.dc));
    CHECK(md_px(&m, 8, 16) == RED); CHECK(md_px(&m, 23, 16) == RED);
    CHECK(md_px(&m, 4, 16) == WHITE);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_stroke_selected_pen_at_stroke_time) {
    SETUP(m); SEL s; stroke_pen(&m, &s, RGB(255, 0, 0));
    HPEN b = U(CreatePen)(PS_SOLID, 1, RGB(0, 0, 255));
    BP(m.dc); MT(m.dc, 2, 6); LT(m.dc, 20, 6); EP(m.dc);
    U(SelectObject)(m.dc, b);
    SP(m.dc);
    CHECK(md_px(&m, 10, 6) == BLUE);
    EXPECT_EQ(md_count(&m, RED), 0);
    U(SelectObject)(m.dc, s.pen);
    U(DeleteObject)(b);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_stroke_bezier_connected) {
    SETUP(m); SEL s; stroke_pen(&m, &s, RGB(255, 0, 0));
    REQUIRE(U(PolyBezier));
    POINT p[4] = {{2, 28}, {2, 2}, {28, 2}, {28, 28}};
    BP(m.dc); U(PolyBezier)(m.dc, p, 4); EP(m.dc);
    CHECK(SP(m.dc));
    CHECK(md_px(&m, 2, 28) == RED || md_px(&m, 2, 27) == RED);
    int n = md_count(&m, RED);
    CHECKF(n > 40 && n < 120, "n=%d", n);
    sel_end(&s); md_destroy(&m);
}

/* ---- StrokeAndFillPath ----------------------------------------------- */
TEST(paths_sfp_fill_and_outline) {
    SETUP(m); SEL s;
    sel_set(&s, m.dc, U(CreatePen)(PS_SOLID, 1, RGB(0, 0, 255)), U(CreateSolidBrush)(RGB(255, 0, 0)));
    BP(m.dc); POLYGON4(m.dc, 4, 4, 20, 20); EP(m.dc);
    CHECK(SFP(m.dc));
    CHECK(md_px(&m, 12, 12) == RED);
    CHECK(md_px(&m, 4, 12) == BLUE); CHECK(md_px(&m, 12, 4) == BLUE);
    CHECK(md_px(&m, 12, 20) == BLUE); CHECK(md_px(&m, 20, 12) == BLUE);
    CHECK(md_px(&m, 2, 2) == WHITE);
    EXPECT_EQ(md_count_not(&m, WHITE), md_count(&m, RED) + md_count(&m, BLUE));
    sel_end(&s); md_destroy(&m);
}
TEST(paths_sfp_outline_over_fill) {
    SETUP(m); SEL s;
    sel_set(&s, m.dc, U(CreatePen)(PS_SOLID, 1, RGB(0, 0, 255)), U(CreateSolidBrush)(RGB(255, 0, 0)));
    BP(m.dc); POLYGON4(m.dc, 4, 4, 20, 20); EP(m.dc);
    SFP(m.dc);
    RECT r; md_bbox(&m, WHITE, &r);
    CHECK(r.left == 4 && r.top == 4);
    CHECK(r.right >= 20 && r.right <= 21 && r.bottom >= 20 && r.bottom <= 21);
    CHECK(md_px(&m, 5, 5) == RED);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_sfp_null_pen_is_fill_only) {
    SETUP(m); SEL s;
    sel_set(&s, m.dc, (HPEN)U(GetStockObject)(NULL_PEN), U(CreateSolidBrush)(RGB(255, 0, 0))); s.pen = NULL;
    BP(m.dc); POLYGON4(m.dc, 4, 4, 20, 20); EP(m.dc);
    CHECK(SFP(m.dc));
    CHECK(md_rect_is(&m, 4, 4, 20, 20, RED, WHITE));
    U(SelectObject)(m.dc, s.op);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_sfp_null_brush_is_stroke_only) {
    SETUP(m); SEL s;
    sel_set(&s, m.dc, U(CreatePen)(PS_SOLID, 1, RGB(0, 0, 255)), (HBRUSH)U(GetStockObject)(NULL_BRUSH)); s.br = NULL;
    BP(m.dc); POLYGON4(m.dc, 4, 4, 20, 20); EP(m.dc);
    CHECK(SFP(m.dc));
    CHECK(md_px(&m, 12, 12) == WHITE); CHECK(md_px(&m, 4, 12) == BLUE);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_sfp_consumes_and_errors) {
    SETUP(m); SEL s;
    sel_set(&s, m.dc, U(CreatePen)(PS_SOLID, 1, RGB(0, 0, 255)), U(CreateSolidBrush)(RGB(255, 0, 0)));
    SET_ERR(0xdeadbeef);
    CHECK(!SFP(m.dc));
    EXPECT_ERR(0, ERROR_CAN_NOT_COMPLETE);
    BP(m.dc); POLYGON4(m.dc, 4, 4, 20, 20);
    CHECK(!SFP(m.dc));
    EP(m.dc);
    CHECK(SFP(m.dc));
    EXPECT_EQ(GPN(m.dc), -1);
    CHECK(!SFP(m.dc));
    sel_end(&s); md_destroy(&m);
}
TEST(paths_sfp_insideframe_stays_inside) {
    SETUP(m); SEL s;
    sel_set(&s, m.dc, U(CreatePen)(PS_INSIDEFRAME, 4, RGB(0, 0, 255)), U(CreateSolidBrush)(RGB(255, 0, 0)));
    BP(m.dc); U(Rectangle)(m.dc, 6, 6, 26, 26); EP(m.dc);
    CHECK(SFP(m.dc));
    RECT r; md_bbox(&m, WHITE, &r);
    t_note("INSIDEFRAME sfp bbox %ld,%ld,%ld,%ld", r.left, r.top, r.right, r.bottom);
    CHECK(r.left <= 6 && r.top <= 6 && r.right >= 26 && r.bottom >= 26);
    CHECK(md_px(&m, 16, 16) == RED);
    CHECK(md_px(&m, 7, 16) == BLUE);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_sfp_wide_solid_pen_grows_outward) {
    SETUP(m); SEL s;
    sel_set(&s, m.dc, U(CreatePen)(PS_SOLID, 4, RGB(0, 0, 255)), U(CreateSolidBrush)(RGB(255, 0, 0)));
    BP(m.dc); POLYGON4(m.dc, 10, 10, 20, 20); EP(m.dc);
    CHECK(SFP(m.dc));
    RECT r; md_bbox(&m, WHITE, &r);
    t_note("SOLID w4 sfp bbox %ld,%ld,%ld,%ld", r.left, r.top, r.right, r.bottom);
    CHECK(r.left < 10 && r.top < 10 && r.right > 20 && r.bottom > 20);
    CHECK(md_px(&m, 15, 15) == RED);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_sfp_fill_mode_winding_hole) {
    SETUP(m); SEL s;
    sel_set(&s, m.dc, (HPEN)U(GetStockObject)(NULL_PEN), U(CreateSolidBrush)(RGB(255, 0, 0))); s.pen = NULL;
    U(SetPolyFillMode)(m.dc, ALTERNATE);
    BP(m.dc); POLYGON4(m.dc, 2, 2, 30, 30); POLYGON4(m.dc, 10, 10, 22, 22); EP(m.dc);
    CHECK(SFP(m.dc));
    CHECK(md_px(&m, 16, 16) == WHITE); CHECK(md_px(&m, 5, 5) == RED);
    sel_end(&s); md_destroy(&m);
}

/* ---- PathToRegion ---------------------------------------------------- */
TEST(paths_region_polygon_bounds) {
    SETUP(m);
    BP(m.dc); POLYGON4(m.dc, 4, 6, 12, 14); EP(m.dc);
    HRGN rg = U(PathToRegion)(m.dc);
    CHECK(rg != NULL);
    RECT r = {0}; int t = U(GetRgnBox)(rg, &r);
    EXPECT_EQ(t, SIMPLEREGION);
    CHECK(r.left == 4 && r.top == 6 && r.right == 12 && r.bottom == 14);
    CHECK(U(PtInRegion)(rg, 4, 6)); CHECK(U(PtInRegion)(rg, 11, 13));
    CHECK(!U(PtInRegion)(rg, 12, 13)); CHECK(!U(PtInRegion)(rg, 11, 14)); CHECK(!U(PtInRegion)(rg, 3, 6));
    U(DeleteObject)(rg);
    md_destroy(&m);
}
TEST(paths_region_consumes_path) {
    SETUP(m);
    BP(m.dc); POLYGON4(m.dc, 4, 6, 12, 14); EP(m.dc);
    HRGN rg = U(PathToRegion)(m.dc);
    CHECK(rg != NULL);
    EXPECT_EQ(GPN(m.dc), -1);
    CHECK(U(PathToRegion)(m.dc) == NULL);
    U(DeleteObject)(rg);
    md_destroy(&m);
}
TEST(paths_region_empty_path_fails) {
    SETUP(m);
    BP(m.dc); EP(m.dc);
    SET_ERR(0xdeadbeef);
    HRGN rg = U(PathToRegion)(m.dc);
    CHECK(rg == NULL);
    t_note("PathToRegion(empty) le=%lu", GetLastError());
    EXPECT_EQ(GPN(m.dc), -1);
    md_destroy(&m);
}
TEST(paths_region_open_bracket_fails) {
    SETUP(m);
    BP(m.dc); POLYGON4(m.dc, 4, 6, 12, 14);
    SET_ERR(0xdeadbeef);
    CHECK(U(PathToRegion)(m.dc) == NULL);
    t_note("PathToRegion(open) le=%lu", GetLastError());
    AP(m.dc);
    SET_ERR(0xdeadbeef);
    CHECK(U(PathToRegion)(m.dc) == NULL);
    md_destroy(&m);
}
TEST(paths_region_open_figure_closed_implicitly) {
    SETUP(m);
    POINT t[3] = {{2, 2}, {20, 2}, {2, 20}};
    BP(m.dc); U(Polyline)(m.dc, t, 3); EP(m.dc);
    HRGN rg = U(PathToRegion)(m.dc);
    CHECK(rg != NULL);
    if (rg) { CHECK(U(PtInRegion)(rg, 5, 5)); CHECK(!U(PtInRegion)(rg, 18, 18)); U(DeleteObject)(rg); }
    md_destroy(&m);
}
TEST(paths_region_matches_fillpath_polygons) {
    SETUP(m); SEL s; fill_red(&m, &s);
    POINT shapes[3][6] = {
        {{3, 3}, {28, 6}, {20, 27}, {5, 20}, {5, 20}, {5, 20}},
        {{16, 2}, {30, 28}, {2, 28}, {2, 28}, {2, 28}, {2, 28}},
        {{2, 2}, {30, 2}, {30, 30}, {20, 30}, {20, 10}, {2, 10}} };
    int cnt[3] = {5, 3, 6}, i;
    for (i = 0; i < 3; i++) {
        md_clear(&m, WHITE);
        BP(m.dc); U(Polygon)(m.dc, shapes[i], cnt[i]); EP(m.dc);
        HRGN rg = U(PathToRegion)(m.dc);
        BP(m.dc); U(Polygon)(m.dc, shapes[i], cnt[i]); EP(m.dc); FP(m.dc);
        int x, y, bad = 0;
        for (y = 0; y < 32; y++) for (x = 0; x < 32; x++)
            if (!!U(PtInRegion)(rg, x, y) != (md_px(&m, x, y) == RED)) bad++;
        CHECKF(bad == 0, "shape %d: %d pixels differ", i, bad);
        U(DeleteObject)(rg);
    }
    sel_end(&s); md_destroy(&m);
}
TEST(paths_region_matches_fillpath_ellipse) {
    SETUP(m); SEL s; fill_red(&m, &s);
    BP(m.dc); U(Ellipse)(m.dc, 3, 3, 29, 29); EP(m.dc);
    HRGN rg = U(PathToRegion)(m.dc);
    BP(m.dc); U(Ellipse)(m.dc, 3, 3, 29, 29); EP(m.dc); FP(m.dc);
    int x, y, bad = 0;
    for (y = 0; y < 32; y++) for (x = 0; x < 32; x++)
        if (!!U(PtInRegion)(rg, x, y) != (md_px(&m, x, y) == RED)) bad++;
    if (bad) t_note("ellipse region vs fill differ at %d pixels", bad);
    CHECK(bad <= 8);
    U(DeleteObject)(rg);
    sel_end(&s); md_destroy(&m);
}
TEST(paths_region_fill_mode_alternate_winding) {
    SETUP(m);
    U(SetPolyFillMode)(m.dc, ALTERNATE);
    BP(m.dc); POLYGON4(m.dc, 2, 2, 30, 30); POLYGON4(m.dc, 10, 10, 22, 22); EP(m.dc);
    HRGN a = U(PathToRegion)(m.dc);
    U(SetPolyFillMode)(m.dc, WINDING);
    BP(m.dc); POLYGON4(m.dc, 2, 2, 30, 30); POLYGON4(m.dc, 10, 10, 22, 22); EP(m.dc);
    HRGN w = U(PathToRegion)(m.dc);
    CHECK(a && w);
    if (a) { CHECK(!U(PtInRegion)(a, 16, 16)); CHECK(U(PtInRegion)(a, 4, 4)); U(DeleteObject)(a); }
    if (w) { CHECK(U(PtInRegion)(w, 16, 16)); U(DeleteObject)(w); }
    md_destroy(&m);
}
TEST(paths_region_device_coordinates_viewport_org) {
    SETUP(m);
    U(SetViewportOrgEx)(m.dc, 10, 20, NULL);
    BP(m.dc); POLYGON4(m.dc, 4, 4, 12, 12); EP(m.dc);
    HRGN rg = U(PathToRegion)(m.dc);
    RECT r = {0}; U(GetRgnBox)(rg, &r);
    CHECK(r.left == 14 && r.top == 24 && r.right == 22 && r.bottom == 32);
    U(DeleteObject)(rg);
    U(SetViewportOrgEx)(m.dc, 0, 0, NULL);
    md_destroy(&m);
}
TEST(paths_region_device_coordinates_scaled) {
    SETUP(m);
    U(SetMapMode)(m.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(m.dc, 1, 1, NULL); U(SetViewportExtEx)(m.dc, 2, 3, NULL);
    BP(m.dc); POLYGON4(m.dc, 2, 2, 6, 6); EP(m.dc);
    HRGN rg = U(PathToRegion)(m.dc);
    RECT r = {0}; U(GetRgnBox)(rg, &r);
    CHECK(r.left == 4 && r.top == 6 && r.right == 12 && r.bottom == 18);
    U(DeleteObject)(rg);
    U(SetMapMode)(m.dc, MM_TEXT);
    md_destroy(&m);
}
TEST(paths_region_flipped_mapping) {
    SETUP(m);
    U(SetMapMode)(m.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(m.dc, 1, 1, NULL); U(SetViewportExtEx)(m.dc, 1, -1, NULL);
    U(SetViewportOrgEx)(m.dc, 0, 31, NULL);
    BP(m.dc); POLYGON4(m.dc, 2, 2, 10, 10); EP(m.dc);
    HRGN rg = U(PathToRegion)(m.dc);
    CHECK(rg != NULL);
    RECT r = {0}; U(GetRgnBox)(rg, &r);
    t_note("flipped y region box %ld,%ld,%ld,%ld", r.left, r.top, r.right, r.bottom);
    CHECK(r.left == 2 && r.right == 10);
    CHECK(r.top >= 20 && r.bottom <= 31 && r.bottom - r.top >= 7 && r.bottom - r.top <= 9);
    U(DeleteObject)(rg);
    U(SetViewportOrgEx)(m.dc, 0, 0, NULL); U(SetMapMode)(m.dc, MM_TEXT);
    md_destroy(&m);
}
TEST(paths_region_no_gdi_leak) {
    SETUP(m);
    int before = gui_objs(), i;
    for (i = 0; i < 300; i++) {
        BP(m.dc); POLYGON4(m.dc, 4, 6, 12, 14); EP(m.dc);
        HRGN rg = U(PathToRegion)(m.dc);
        U(DeleteObject)(rg);
    }
    int after = gui_objs();
    if (before >= 0) EXPECT_EQ(after, before);
    md_destroy(&m);
}
TEST(paths_region_is_deletable_and_typed) {
    SETUP(m);
    BP(m.dc); POLYGON4(m.dc, 4, 6, 12, 14); EP(m.dc);
    HRGN rg = U(PathToRegion)(m.dc);
    EXPECT_EQ(U(GetObjectType)(rg), OBJ_REGION);
    CHECK(U(DeleteObject)(rg));
    EXPECT_EQ(U(GetObjectType)(rg), 0);
    md_destroy(&m);
}

/* ---- SelectClipPath -------------------------------------------------- */
TEST(paths_clip_copy_box) {
    SETUP(m);
    BP(m.dc); POLYGON4(m.dc, 4, 6, 12, 14); EP(m.dc);
    CHECK(U(SelectClipPath)(m.dc, RGN_COPY));
    RECT r = {0}; int t = U(GetClipBox)(m.dc, &r);
    EXPECT_EQ(t, SIMPLEREGION);
    CHECK(r.left == 4 && r.top == 6 && r.right == 12 && r.bottom == 14);
    EXPECT_EQ(GPN(m.dc), -1);
    U(SelectClipRgn)(m.dc, NULL);
    md_destroy(&m);
}
TEST(paths_clip_restricts_output_exactly) {
    SETUP(m);
    BP(m.dc); POLYGON4(m.dc, 4, 6, 12, 14); EP(m.dc);
    CHECK(U(SelectClipPath)(m.dc, RGN_COPY));
    HGDIOBJ ob = U(SelectObject)(m.dc, U(CreateSolidBrush)(RGB(255, 0, 0)));
    U(PatBlt)(m.dc, 0, 0, 32, 32, PATCOPY);
    HGDIOBJ b = U(SelectObject)(m.dc, ob);
    CHECK(md_rect_is(&m, 4, 6, 12, 14, RED, WHITE));
    U(DeleteObject)(b);
    U(SelectClipRgn)(m.dc, NULL);
    md_destroy(&m);
}
TEST(paths_clip_triangle_ptvisible) {
    SETUP(m);
    POINT t[3] = {{0, 0}, {20, 0}, {0, 20}};
    BP(m.dc); U(Polygon)(m.dc, t, 3); EP(m.dc);
    CHECK(U(SelectClipPath)(m.dc, RGN_COPY));
    CHECK(U(PtVisible)(m.dc, 2, 2)); CHECK(!U(PtVisible)(m.dc, 18, 18)); CHECK(!U(PtVisible)(m.dc, 25, 2));
    U(SelectClipRgn)(m.dc, NULL);
    md_destroy(&m);
}
TEST(paths_clip_modes_against_existing_clip) {
    SETUP(m);
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(SelectClipRgn)); REQUIRE(U(PtVisible));
    HRGN c = U(CreateRectRgn)(0, 0, 10, 10);
    struct { int mode; int in77, in22, in1212, in2020; } tb[] = {
        {RGN_AND,  1, 0, 0, 0},   /* path (5,5,15,15) AND clip (0,0,10,10) */
        {RGN_OR,   1, 1, 1, 0},
        {RGN_XOR,  0, 1, 1, 0},
        {RGN_DIFF, 0, 1, 0, 0},   /* clip minus path */
        {RGN_COPY, 1, 0, 1, 0},
    };
    unsigned i;
    for (i = 0; i < sizeof tb / sizeof tb[0]; i++) {
        U(SelectClipRgn)(m.dc, c);
        BP(m.dc); POLYGON4(m.dc, 5, 5, 15, 15); EP(m.dc);
        CHECKF(U(SelectClipPath)(m.dc, tb[i].mode), "mode %d", tb[i].mode);
        CHECKF(!!U(PtVisible)(m.dc, 7, 7) == tb[i].in77, "mode %d (7,7)", tb[i].mode);
        CHECKF(!!U(PtVisible)(m.dc, 2, 2) == tb[i].in22, "mode %d (2,2)", tb[i].mode);
        CHECKF(!!U(PtVisible)(m.dc, 12, 12) == tb[i].in1212, "mode %d (12,12)", tb[i].mode);
        CHECKF(!!U(PtVisible)(m.dc, 20, 20) == tb[i].in2020, "mode %d (20,20)", tb[i].mode);
    }
    U(SelectClipRgn)(m.dc, NULL);
    U(DeleteObject)(c);
    md_destroy(&m);
}
TEST(paths_clip_and_with_no_prior_clip) {
    SETUP(m);
    BP(m.dc); POLYGON4(m.dc, 5, 5, 15, 15); EP(m.dc);
    BOOL r = U(SelectClipPath)(m.dc, RGN_AND);
    t_note("RGN_AND with no clip -> %d", r);
    if (r) { CHECK(U(PtVisible)(m.dc, 7, 7)); CHECK(!U(PtVisible)(m.dc, 20, 20)); }
    U(SelectClipRgn)(m.dc, NULL);
    md_destroy(&m);
}
TEST(paths_clip_invalid_modes) {
    SETUP(m);
    int bad[] = {0, 6, 7, 100, -1};
    unsigned i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        BP(m.dc); POLYGON4(m.dc, 5, 5, 15, 15); EP(m.dc);
        SET_ERR(0xdeadbeef);
        BOOL r = U(SelectClipPath)(m.dc, bad[i]);
        t_note("SelectClipPath(mode %d) -> %d le=%lu", bad[i], r, GetLastError());
        CHECKF(!r, "mode %d accepted", bad[i]);
        AP(m.dc);
        U(SelectClipRgn)(m.dc, NULL);
    }
    md_destroy(&m);
}
TEST(paths_clip_open_bracket_and_none_fail) {
    SETUP(m);
    SET_ERR(0xdeadbeef);
    CHECK(!U(SelectClipPath)(m.dc, RGN_COPY));
    EXPECT_ERR(0, ERROR_CAN_NOT_COMPLETE);
    BP(m.dc); POLYGON4(m.dc, 5, 5, 15, 15);
    CHECK(!U(SelectClipPath)(m.dc, RGN_COPY));
    AP(m.dc);
    RECT r; EXPECT_NE(U(GetClipBox)(m.dc, &r), 0);
    md_destroy(&m);
}
TEST(paths_clip_empty_path_fails) {
    SETUP(m);
    BP(m.dc); EP(m.dc);
    SET_ERR(0xdeadbeef);
    CHECK(!U(SelectClipPath)(m.dc, RGN_COPY));
    t_note("SelectClipPath(empty) le=%lu", GetLastError());
    EXPECT_EQ(GPN(m.dc), -1);
    md_destroy(&m);
}
TEST(paths_clip_device_coordinates_scaled) {
    SETUP(m);
    U(SetMapMode)(m.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(m.dc, 1, 1, NULL); U(SetViewportExtEx)(m.dc, 2, 2, NULL);
    BP(m.dc); POLYGON4(m.dc, 2, 2, 6, 6); EP(m.dc);
    CHECK(U(SelectClipPath)(m.dc, RGN_COPY));
    U(SetMapMode)(m.dc, MM_TEXT);
    RECT r = {0}; U(GetClipBox)(m.dc, &r);
    CHECK(r.left == 4 && r.top == 4 && r.right == 12 && r.bottom == 12);
    U(SelectClipRgn)(m.dc, NULL);
    md_destroy(&m);
}
TEST(paths_clip_equals_path_region) {
    SETUP(m);
    POINT t[4] = {{3, 3}, {25, 5}, {20, 25}, {4, 18}};
    BP(m.dc); U(Polygon)(m.dc, t, 4); EP(m.dc);
    HRGN rg = U(PathToRegion)(m.dc);
    BP(m.dc); U(Polygon)(m.dc, t, 4); EP(m.dc);
    CHECK(U(SelectClipPath)(m.dc, RGN_COPY));
    int x, y, bad = 0;
    for (y = 0; y < 32; y++) for (x = 0; x < 32; x++)
        if (!!U(PtVisible)(m.dc, x, y) != !!U(PtInRegion)(rg, x, y)) bad++;
    CHECKF(bad == 0, "%d differ", bad);
    U(SelectClipRgn)(m.dc, NULL);
    U(DeleteObject)(rg);
    md_destroy(&m);
}
TEST(paths_clip_text_style_nested_use) {
    SETUP(m); SEL s; fill_red(&m, &s);
    /* clip to a circle, then fill a path that spans it */
    BP(m.dc); U(Ellipse)(m.dc, 4, 4, 28, 28); EP(m.dc);
    CHECK(U(SelectClipPath)(m.dc, RGN_COPY));
    BP(m.dc); POLYGON4(m.dc, 0, 0, 32, 32); EP(m.dc);
    CHECK(FP(m.dc));
    CHECK(md_px(&m, 16, 16) == RED); CHECK(md_px(&m, 4, 4) == WHITE); CHECK(md_px(&m, 0, 0) == WHITE);
    int n = md_count(&m, RED);
    CHECKF(n >= 400 && n <= 480, "n=%d", n);
    U(SelectClipRgn)(m.dc, NULL);
    sel_end(&s); md_destroy(&m);
}
