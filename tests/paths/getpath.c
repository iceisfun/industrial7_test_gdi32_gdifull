/* GetPath contract and the point/type output of every path-building primitive. */
#include "ph.h"

#define SETUP(m) MD m; REQ_BASE(); if (!md_create(&m, 64, 64)) { t_skip("no DIB"); return; }

/* ---- GetPath argument handling -------------------------------------- */
TEST(paths_getpath_size_query_matches_fetch) {
    SETUP(m);
    POINT pts[8]; BYTE ty[8];
    BP(m.dc); MT(m.dc, 1, 1); LT(m.dc, 5, 1); LT(m.dc, 5, 5); EP(m.dc);
    EXPECT_EQ(GPN(m.dc), 3);
    EXPECT_EQ(U(GetPath)(m.dc, pts, ty, 8), 3);
    EXPECT_EQ(pts[2].x, 5); EXPECT_EQ(pts[2].y, 5);
    md_destroy(&m);
}
TEST(paths_getpath_size_query_ignores_buffers) {
    SETUP(m);
    BP(m.dc); MT(m.dc, 1, 1); LT(m.dc, 5, 1); EP(m.dc);
    POINT pts[4] = {{77, 77}, {77, 77}, {77, 77}, {77, 77}}; BYTE ty[4] = {0x55, 0x55, 0x55, 0x55};
    EXPECT_EQ(U(GetPath)(m.dc, pts, ty, 0), 2);
    CHECK(pts[0].x == 77 && ty[0] == 0x55);   /* count 0 writes nothing */
    md_destroy(&m);
}
TEST(paths_getpath_size_query_repeatable) {
    SETUP(m);
    BP(m.dc); MT(m.dc, 1, 1); LT(m.dc, 5, 1); LT(m.dc, 5, 5); EP(m.dc);
    int i, ok = 1;
    for (i = 0; i < 50; i++) if (GPN(m.dc) != 3) ok = 0;
    CHECK(ok);
    md_destroy(&m);
}
TEST(paths_getpath_buffer_too_small) {
    SETUP(m);
    POINT pts[8]; BYTE ty[8];
    BP(m.dc); MT(m.dc, 1, 1); LT(m.dc, 5, 1); LT(m.dc, 5, 5); LT(m.dc, 1, 5); EP(m.dc);
    int cpt;
    for (cpt = 1; cpt < 4; cpt++) {
        SET_ERR(0xdeadbeef);
        EXPECT_EQ(U(GetPath)(m.dc, pts, ty, cpt), -1);
        EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    }
    EXPECT_EQ(GPN(m.dc), 4);   /* a failed fetch does not consume the path */
    md_destroy(&m);
}
TEST(paths_getpath_buffer_exact_and_larger) {
    SETUP(m);
    POINT pts[8]; BYTE ty[8];
    BP(m.dc); MT(m.dc, 1, 1); LT(m.dc, 5, 1); LT(m.dc, 5, 5); LT(m.dc, 1, 5); EP(m.dc);
    EXPECT_EQ(U(GetPath)(m.dc, pts, ty, 4), 4);
    EXPECT_EQ(U(GetPath)(m.dc, pts, ty, 5), 4);
    EXPECT_EQ(U(GetPath)(m.dc, pts, ty, 8), 4);
    EXPECT_EQ(GPN(m.dc), 4);   /* GetPath never consumes */
    md_destroy(&m);
}
TEST(paths_getpath_does_not_write_past_count) {
    SETUP(m);
    POINT pts[8]; BYTE ty[8]; int i;
    for (i = 0; i < 8; i++) { pts[i].x = pts[i].y = 0x5a5a; ty[i] = 0xee; }
    BP(m.dc); MT(m.dc, 1, 1); LT(m.dc, 5, 1); LT(m.dc, 5, 5); EP(m.dc);
    EXPECT_EQ(U(GetPath)(m.dc, pts, ty, 8), 3);
    for (i = 3; i < 8; i++) { CHECK(pts[i].x == 0x5a5a && pts[i].y == 0x5a5a); CHECK(ty[i] == 0xee); }
    md_destroy(&m);
}
TEST(paths_getpath_negative_count) {
    SETUP(m);
    POINT pts[4]; BYTE ty[4];
    BP(m.dc); MT(m.dc, 1, 1); LT(m.dc, 5, 1); EP(m.dc);
    SET_ERR(0xdeadbeef);
    int r = U(GetPath)(m.dc, pts, ty, -1);
    t_note("GetPath cpt=-1 -> %d le=%lu", r, GetLastError());
    CHECK(r != 2 || 1);
    EXPECT_EQ(GPN(m.dc), 2);
    md_destroy(&m);
}
TEST(paths_getpath_invalid_handles) {
    REQ_BASE();
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)0x1, (HDC)(ULONG_PTR)0xdeadbeef, (HDC)(ULONG_PTR)0x00010001, (HDC)(ULONG_PTR)-1 };
    int i;
    for (i = 0; i < 5; i++) {
        SET_ERR(0xdeadbeef);
        int r = GPN(bad[i]);
        CHECKF(r == -1, "handle %p -> %d", bad[i], r);
        if (i == 0) t_note("GetPath(NULL) le=%lu", GetLastError());
    }
}
TEST(paths_getpath_wrong_object_types) {
    REQ_BASE();
    HGDIOBJ objs[4];
    objs[0] = U(GetStockObject)(BLACK_PEN);
    objs[1] = U(GetStockObject)(WHITE_BRUSH);
    objs[2] = U(GetStockObject)(SYSTEM_FONT);
    objs[3] = U(CreateRectRgn) ? (HGDIOBJ)U(CreateRectRgn)(0, 0, 4, 4) : NULL;
    int i;
    for (i = 0; i < 4; i++) {
        if (!objs[i]) continue;
        EXPECT_EQ(GPN((HDC)objs[i]), -1);
    }
    if (objs[3]) U(DeleteObject)(objs[3]);
}
TEST(paths_getpath_zero_count_nonnull_buffers_empty_path) {
    SETUP(m);
    POINT pt; BYTE ty;
    BP(m.dc); EP(m.dc);
    EXPECT_EQ(U(GetPath)(m.dc, &pt, &ty, 1), 0);
    md_destroy(&m);
}
TEST(paths_getpath_open_with_buffers) {
    SETUP(m);
    POINT pts[4]; BYTE ty[4];
    BP(m.dc); LT(m.dc, 4, 4);
    EXPECT_EQ(U(GetPath)(m.dc, pts, ty, 4), -1);
    EXPECT_EQ(U(GetPath)(m.dc, NULL, NULL, 0), -1);
    AP(m.dc);
    md_destroy(&m);
}

/* ---- Line primitives ------------------------------------------------- */
TEST(paths_shape_moveto_only) {
    SETUP(m);
    BP(m.dc); MT(m.dc, 5, 6); EP(m.dc);
    int n = GPN(m.dc);
    CHECK(n == 0 || n == 1);
    if (n == 1) t_note("lone MoveToEx kept as a 1-point path");
    else t_note("lone MoveToEx dropped (count 0)");
    md_destroy(&m);
}
TEST(paths_shape_lineto_basic) {
    SETUP(m);
    BP(m.dc); MT(m.dc, 5, 6); LT(m.dc, 20, 30); EP(m.dc);
    EXPECT_EQ(pd_get(m.dc), 2);
    CHECK(pd_near(0, 5, 6, 0)); CHECK(pd_near(1, 20, 30, 0));
    EXPECT_EQ(g_pd.ty[0], PT_MOVETO); EXPECT_EQ(g_pd.ty[1], PT_LINETO);
    md_destroy(&m);
}
TEST(paths_shape_lineto_chain) {
    SETUP(m);
    int i;
    BP(m.dc); MT(m.dc, 0, 0);
    for (i = 1; i <= 10; i++) LT(m.dc, i * 3, i * 2);
    EP(m.dc);
    EXPECT_EQ(pd_get(m.dc), 11);
    for (i = 1; i <= 10; i++) { CHECK(pd_near(i, i * 3, i * 2, 0)); EXPECT_EQ(g_pd.ty[i], PT_LINETO); }
    CHECK(pd_valid());
    md_destroy(&m);
}
TEST(paths_shape_lineto_same_point_kept) {
    SETUP(m);
    BP(m.dc); MT(m.dc, 5, 5); LT(m.dc, 5, 5); LT(m.dc, 5, 5); EP(m.dc);
    int n = pd_get(m.dc);
    CHECK(n >= 1);
    t_note("degenerate lineto sequence -> %d points", n);
    CHECK(pd_valid());
    md_destroy(&m);
}
TEST(paths_shape_two_moveto_figures) {
    SETUP(m);
    BP(m.dc); MT(m.dc, 1, 1); LT(m.dc, 10, 1); MT(m.dc, 1, 20); LT(m.dc, 10, 20); EP(m.dc);
    EXPECT_EQ(pd_get(m.dc), 4);
    EXPECT_EQ(g_pd.ty[0], PT_MOVETO); EXPECT_EQ(g_pd.ty[1], PT_LINETO);
    EXPECT_EQ(g_pd.ty[2], PT_MOVETO); EXPECT_EQ(g_pd.ty[3], PT_LINETO);
    md_destroy(&m);
}
TEST(paths_shape_consecutive_movetos) {
    SETUP(m);
    BP(m.dc); MT(m.dc, 1, 1); MT(m.dc, 2, 2); MT(m.dc, 3, 3); LT(m.dc, 9, 9); EP(m.dc);
    int n = pd_get(m.dc);
    CHECK(n >= 2 && n <= 4);
    CHECK(pd_valid());
    t_note("3 MoveToEx + LineTo -> %d points", n);
    CHECK(pd_near(n - 1, 9, 9, 0));
    md_destroy(&m);
}
TEST(paths_shape_polyline) {
    SETUP(m);
    POINT p[5] = {{2, 2}, {10, 3}, {20, 30}, {5, 40}, {1, 1}};
    int k;
    for (k = 2; k <= 5; k++) {
        BP(m.dc); U(Polyline)(m.dc, p, k); EP(m.dc);
        EXPECT_EQ(pd_get(m.dc), k);
        EXPECT_EQ(g_pd.ty[0], PT_MOVETO);
        CHECK(pd_count_type(PT_LINETO) == k - 1);
        EXPECT_EQ(pd_count_close(), 0);
        CHECK(pd_near(k - 1, p[k - 1].x, p[k - 1].y, 0));
        AP(m.dc);
    }
    md_destroy(&m);
}
TEST(paths_shape_polyline_bad_counts) {
    SETUP(m);
    POINT p[3] = {{2, 2}, {10, 3}, {20, 30}};
    BP(m.dc);
    SET_ERR(0xdeadbeef);
    CHECK(!U(Polyline)(m.dc, p, 1));
    CHECK(!U(Polyline)(m.dc, p, 0));
    if (!ph_is_wine()) CHECK(!U(Polyline)(m.dc, p, -1));
    EP(m.dc);
    EXPECT_EQ(GPN(m.dc), 0);
    md_destroy(&m);
}
TEST(paths_shape_polygon) {
    SETUP(m);
    POINT p[5] = {{2, 2}, {30, 3}, {20, 30}, {5, 40}, {1, 20}};
    int k;
    for (k = 3; k <= 5; k++) {
        BP(m.dc); CHECK(U(Polygon)(m.dc, p, k)); EP(m.dc);
        EXPECT_EQ(pd_get(m.dc), k);
        EXPECT_EQ(g_pd.ty[0], PT_MOVETO);
        EXPECT_EQ(g_pd.ty[k - 1], PT_LINETO | PT_CLOSEFIGURE);
        EXPECT_EQ(pd_count_close(), 1);
        AP(m.dc);
    }
    md_destroy(&m);
}
TEST(paths_shape_polygon_two_points) {
    SETUP(m);
    POINT p[2] = {{2, 2}, {30, 3}};
    BP(m.dc);
    BOOL r = U(Polygon)(m.dc, p, 2);
    EP(m.dc);
    int n = GPN(m.dc);
    t_note("Polygon(2 points) -> %d, path points %d", r, n);
    CHECK(!r || n >= 2);
    md_destroy(&m);
}
TEST(paths_shape_polygon_bad_counts) {
    SETUP(m);
    POINT p[3] = {{2, 2}, {10, 3}, {20, 30}};
    BP(m.dc);
    CHECK(!U(Polygon)(m.dc, p, 0));
    if (!ph_is_wine()) CHECK(!U(Polygon)(m.dc, p, -1));
    CHECK(!U(Polygon)(m.dc, p, 1));
    EP(m.dc);
    EXPECT_EQ(GPN(m.dc), 0);
    md_destroy(&m);
}
TEST(paths_shape_polylineto) {
    SETUP(m);
    REQUIRE(U(PolylineTo));
    POINT p[3] = {{10, 10}, {20, 10}, {20, 20}};
    MT(m.dc, 3, 4);
    BP(m.dc); CHECK(U(PolylineTo)(m.dc, p, 3)); EP(m.dc);
    EXPECT_EQ(pd_get(m.dc), 4);
    CHECK(pd_near(0, 3, 4, 0)); CHECK(pd_near(3, 20, 20, 0));
    EXPECT_EQ(g_pd.ty[0], PT_MOVETO); EXPECT_EQ(pd_count_type(PT_LINETO), 3);
    POINT cur; U(GetCurrentPositionEx)(m.dc, &cur);
    EXPECT_EQ(cur.x, 20); EXPECT_EQ(cur.y, 20);
    md_destroy(&m);
}
TEST(paths_shape_polypolyline) {
    SETUP(m);
    REQUIRE(U(PolyPolyline));
    POINT p[7] = {{1, 1}, {9, 1}, {1, 5}, {9, 5}, {9, 9}, {2, 20}, {3, 22}};
    DWORD cnt[3] = {2, 3, 2};
    BP(m.dc); CHECK(U(PolyPolyline)(m.dc, p, cnt, 3)); EP(m.dc);
    EXPECT_EQ(pd_get(m.dc), 7);
    EXPECT_EQ(pd_count_type(PT_MOVETO), 3);
    EXPECT_EQ(pd_count_type(PT_LINETO), 4);
    EXPECT_EQ(g_pd.ty[0], PT_MOVETO); EXPECT_EQ(g_pd.ty[2], PT_MOVETO); EXPECT_EQ(g_pd.ty[5], PT_MOVETO);
    EXPECT_EQ(pd_count_close(), 0);
    md_destroy(&m);
}
TEST(paths_shape_polypolygon) {
    SETUP(m);
    REQUIRE(U(PolyPolygon));
    POINT p[7] = {{1, 1}, {9, 1}, {5, 9}, {20, 20}, {30, 20}, {30, 30}, {20, 30}};
    INT cnt[2] = {3, 4};
    BP(m.dc); CHECK(U(PolyPolygon)(m.dc, p, cnt, 2)); EP(m.dc);
    EXPECT_EQ(pd_get(m.dc), 7);
    EXPECT_EQ(pd_count_type(PT_MOVETO), 2);
    EXPECT_EQ(pd_count_close(), 2);
    EXPECT_EQ(g_pd.ty[2], PT_LINETO | PT_CLOSEFIGURE);
    EXPECT_EQ(g_pd.ty[6], PT_LINETO | PT_CLOSEFIGURE);
    md_destroy(&m);
}

/* ---- Rectangle family ------------------------------------------------ */
TEST(paths_shape_rectangle_ccw) {
    SETUP(m);
    BP(m.dc); CHECK(U(Rectangle)(m.dc, 20, 20, 40, 40)); EP(m.dc);
    EXPECT_EQ(pd_get(m.dc), 4);
    CHECK(pd_near(0, 39, 20, 1)); CHECK(pd_near(1, 20, 20, 1));
    CHECK(pd_near(2, 20, 39, 1)); CHECK(pd_near(3, 39, 39, 1));
    EXPECT_EQ(g_pd.ty[0], PT_MOVETO); EXPECT_EQ(g_pd.ty[1], PT_LINETO);
    EXPECT_EQ(g_pd.ty[2], PT_LINETO); EXPECT_EQ(g_pd.ty[3], PT_LINETO | PT_CLOSEFIGURE);
    md_destroy(&m);
}
TEST(paths_shape_rectangle_cw) {
    SETUP(m);
    REQUIRE(U(SetArcDirection));
    U(SetArcDirection)(m.dc, AD_CLOCKWISE);
    BP(m.dc); CHECK(U(Rectangle)(m.dc, 20, 20, 40, 40)); EP(m.dc);
    EXPECT_EQ(pd_get(m.dc), 4);
    CHECK(pd_near(0, 39, 39, 1)); CHECK(pd_near(1, 20, 39, 1));
    CHECK(pd_near(2, 20, 20, 1)); CHECK(pd_near(3, 39, 20, 1));
    EXPECT_EQ(g_pd.ty[3], PT_LINETO | PT_CLOSEFIGURE);
    md_destroy(&m);
}
TEST(paths_shape_rectangle_normalised) {
    SETUP(m);
    BP(m.dc); U(Rectangle)(m.dc, 55, 50, 30, 35); EP(m.dc);   /* right<left, bottom>top is irrelevant */
    EXPECT_EQ(pd_get(m.dc), 4);
    CHECK(pd_near(0, 54, 35, 0)); CHECK(pd_near(1, 30, 35, 0));
    CHECK(pd_near(2, 30, 49, 0)); CHECK(pd_near(3, 54, 49, 0));
    md_destroy(&m);
}
TEST(paths_shape_rectangle_degenerate) {
    SETUP(m);
    BP(m.dc); U(Rectangle)(m.dc, 40, 40, 41, 41); EP(m.dc);
    EXPECT_EQ(pd_get(m.dc), 4);
    CHECK(pd_near(0, 40, 40, 0)); CHECK(pd_near(3, 40, 40, 0));
    AP(m.dc);
    BP(m.dc); U(Rectangle)(m.dc, 10, 10, 10, 10); EP(m.dc);
    int n = pd_get(m.dc);
    t_note("empty rectangle -> %d points", n);
    CHECK(n == 0 || n == 4);
    md_destroy(&m);
}
TEST(paths_shape_rectangle_advanced_graphics_mode) {
    SETUP(m);
    REQUIRE(U(SetGraphicsMode));
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    BP(m.dc); U(Rectangle)(m.dc, 20, 20, 40, 40); EP(m.dc);
    EXPECT_EQ(pd_get(m.dc), 4);
    t_note("GM_ADVANCED rect first=(%ld,%ld) third=(%ld,%ld)", g_pd.pt[0].x, g_pd.pt[0].y, g_pd.pt[2].x, g_pd.pt[2].y);
    EXPECT_EQ(g_pd.ty[3], PT_LINETO | PT_CLOSEFIGURE);
    U(SetGraphicsMode)(m.dc, GM_COMPATIBLE);
    md_destroy(&m);
}
TEST(paths_shape_rectangle_multiple_in_one_path) {
    SETUP(m);
    BP(m.dc);
    U(Rectangle)(m.dc, 1, 1, 10, 10); U(Rectangle)(m.dc, 20, 20, 30, 30); U(Rectangle)(m.dc, 40, 40, 50, 50);
    EP(m.dc);
    EXPECT_EQ(pd_get(m.dc), 12);
    EXPECT_EQ(pd_count_type(PT_MOVETO), 3); EXPECT_EQ(pd_count_close(), 3);
    CHECK(pd_valid());
    md_destroy(&m);
}
TEST(paths_shape_rectangle_in_open_figure) {
    SETUP(m);
    BP(m.dc); MT(m.dc, 1, 1); LT(m.dc, 5, 5); U(Rectangle)(m.dc, 20, 20, 30, 30); EP(m.dc);
    int n = pd_get(m.dc);
    CHECK(n == 6);
    CHECK(pd_valid());
    md_destroy(&m);
}
TEST(paths_shape_rectangle_current_pos) {
    SETUP(m);
    POINT cur;
    MT(m.dc, 3, 3);
    BP(m.dc); U(Rectangle)(m.dc, 20, 20, 30, 30);
    U(GetCurrentPositionEx)(m.dc, &cur);
    t_note("pos after Rectangle in path: %ld,%ld", cur.x, cur.y);
    CHECK(cur.x == 3 && cur.y == 3);
    AP(m.dc);
    md_destroy(&m);
}
TEST(paths_shape_ellipse) {
    SETUP(m);
    BP(m.dc); CHECK(U(Ellipse)(m.dc, 20, 20, 40, 40)); EP(m.dc);
    EXPECT_EQ(pd_get(m.dc), 13);
    CHECK(pd_valid());
    EXPECT_EQ(g_pd.ty[0], PT_MOVETO);
    EXPECT_EQ(pd_count_type(PT_BEZIERTO), 12);
    EXPECT_EQ(g_pd.ty[12], PT_BEZIERTO | PT_CLOSEFIGURE);
    CHECK(pd_near(0, 39, 30, 1));
    CHECK(pd_near(12, 39, 30, 1));
    md_destroy(&m);
}
TEST(paths_shape_ellipse_cw_starts_same) {
    SETUP(m);
    U(SetArcDirection)(m.dc, AD_CLOCKWISE);
    BP(m.dc); U(Ellipse)(m.dc, 20, 20, 40, 40); EP(m.dc);
    EXPECT_EQ(pd_get(m.dc), 13);
    CHECK(pd_near(0, 39, 30, 1));
    t_note("cw ellipse pt1=(%ld,%ld)", g_pd.pt[1].x, g_pd.pt[1].y);
    md_destroy(&m);
}
TEST(paths_shape_ellipse_sizes) {
    SETUP(m);
    int sz[] = {1, 2, 3, 8, 21, 40};
    unsigned i;
    for (i = 0; i < sizeof sz / sizeof sz[0]; i++) {
        BP(m.dc); U(Ellipse)(m.dc, 5, 5, 5 + sz[i], 5 + sz[i]); EP(m.dc);
        int n = pd_get(m.dc);
        CHECKF(n == 13 || n == 0 || n == 4, "size %d -> %d points", sz[i], n);
        CHECK(pd_valid());
        AP(m.dc);
    }
    md_destroy(&m);
}
TEST(paths_shape_roundrect) {
    SETUP(m);
    BP(m.dc); CHECK(U(RoundRect)(m.dc, 20, 20, 40, 40, 10, 10)); EP(m.dc);
    int rn = pd_get(m.dc);
    if (rn != 16) t_note("RoundRect path has %d points (expected 16)", rn);
    CHECK(rn >= 13);
    CHECK(pd_valid());
    EXPECT_EQ(g_pd.ty[0], PT_MOVETO); EXPECT_EQ(g_pd.ty[rn - 1] & PT_CLOSEFIGURE, PT_CLOSEFIGURE);
    CHECK(pd_count_type(PT_BEZIERTO) >= 12);
    CHECK(pd_near(0, 39, 25, 1)); CHECK(pd_near(4, 25, 20, 1));
    md_destroy(&m);
}
TEST(paths_shape_roundrect_zero_corner_is_rectangle) {
    SETUP(m);
    BP(m.dc); U(RoundRect)(m.dc, 20, 20, 40, 40, 0, 0); EP(m.dc);
    int n = pd_get(m.dc);
    t_note("RoundRect(0,0) -> %d points", n);
    CHECK(n == 4 || n == 16);
    CHECK(pd_valid());
    md_destroy(&m);
}
TEST(paths_shape_roundrect_huge_corner_clamped) {
    SETUP(m);
    BP(m.dc); CHECK(U(RoundRect)(m.dc, 20, 20, 40, 40, 1000, 1000)); EP(m.dc);
    int n = pd_get(m.dc);
    CHECK(n > 0); CHECK(pd_valid());
    int i, ok = 1;
    for (i = 0; i < g_pd.n; i++) if (g_pd.pt[i].x < 19 || g_pd.pt[i].x > 41 || g_pd.pt[i].y < 19 || g_pd.pt[i].y > 41) ok = 0;
    CHECK(ok);
    md_destroy(&m);
}

/* ---- Arcs ------------------------------------------------------------ */
TEST(paths_shape_arc) {
    SETUP(m);
    BP(m.dc); CHECK(U(Arc)(m.dc, 20, 20, 60, 60, 60, 40, 40, 20)); EP(m.dc);
    int n = pd_get(m.dc);
    CHECK(n >= 4); CHECK(pd_valid());
    EXPECT_EQ(g_pd.ty[0], PT_MOVETO);
    EXPECT_EQ(pd_count_close(), 0);
    EXPECT_EQ((n - 1) % 3, 0);
    md_destroy(&m);
}
TEST(paths_shape_arc_quadrant_start_point) {
    SETUP(m);
    /* counter-clockwise from the east point to the north point */
    BP(m.dc); U(Arc)(m.dc, 20, 20, 60, 60, 60, 40, 40, 20); EP(m.dc);
    pd_get(m.dc);
    CHECK(pd_near(0, 59, 40, 2));
    CHECK(pd_near(g_pd.n - 1, 40, 20, 2));
    md_destroy(&m);
}
TEST(paths_shape_arc_full_circle) {
    SETUP(m);
    BP(m.dc); U(Arc)(m.dc, 20, 20, 60, 60, 60, 40, 60, 40); EP(m.dc);
    int n = pd_get(m.dc);
    CHECK(n >= 13 && (n - 1) % 3 == 0);
    t_note("full-circle Arc -> %d points", n);
    md_destroy(&m);
}
TEST(paths_shape_arc_clockwise_direction_differs) {
    SETUP(m);
    BP(m.dc); U(Arc)(m.dc, 20, 20, 60, 60, 60, 40, 40, 20); EP(m.dc);
    int ccw = pd_get(m.dc);
    POINT first = g_pd.pt[0], last = g_pd.pt[ccw - 1];
    AP(m.dc);
    U(SetArcDirection)(m.dc, AD_CLOCKWISE);
    BP(m.dc); U(Arc)(m.dc, 20, 20, 60, 60, 60, 40, 40, 20); EP(m.dc);
    int cw = pd_get(m.dc);
    CHECK(cw > ccw);   /* 270 degrees versus 90 degrees */
    CHECK(pd_near(0, first.x, first.y, 2));
    CHECK(pd_near(cw - 1, last.x, last.y, 2));
    md_destroy(&m);
}
TEST(paths_shape_chord) {
    SETUP(m);
    REQUIRE(U(Chord));
    BP(m.dc); CHECK(U(Chord)(m.dc, 20, 20, 60, 60, 60, 40, 40, 20)); EP(m.dc);
    int n = pd_get(m.dc);
    CHECK(n >= 4); CHECK(pd_valid());
    EXPECT_EQ(g_pd.ty[0], PT_MOVETO);
    CHECK(g_pd.ty[n - 1] & PT_CLOSEFIGURE);
    EXPECT_EQ(pd_count_close(), 1);
    md_destroy(&m);
}
TEST(paths_shape_pie) {
    SETUP(m);
    REQUIRE(U(Pie));
    BP(m.dc); CHECK(U(Pie)(m.dc, 20, 20, 60, 60, 60, 40, 40, 20)); EP(m.dc);
    int n = pd_get(m.dc);
    CHECK(n >= 5); CHECK(pd_valid());
    CHECK(g_pd.ty[n - 1] & PT_CLOSEFIGURE);
    EXPECT_EQ(pd_count_close(), 1);
    CHECK(pd_count_type(PT_LINETO) >= 1);
    md_destroy(&m);
}
TEST(paths_shape_arcto) {
    SETUP(m);
    REQUIRE(U(ArcTo));
    MT(m.dc, 3, 4);
    BP(m.dc); CHECK(U(ArcTo)(m.dc, 20, 20, 60, 60, 60, 40, 40, 20)); EP(m.dc);
    int n = pd_get(m.dc);
    CHECK(n >= 5); CHECK(pd_valid());
    CHECK(pd_near(0, 3, 4, 0));
    EXPECT_EQ(g_pd.ty[0], PT_MOVETO); EXPECT_EQ(g_pd.ty[1], PT_LINETO);
    EXPECT_EQ(g_pd.ty[2], PT_BEZIERTO);
    POINT cur; U(GetCurrentPositionEx)(m.dc, &cur);
    t_note("ArcTo end pos %ld,%ld", cur.x, cur.y);
    CHECK(abs((int)cur.x - 40) <= 2 && abs((int)cur.y - 20) <= 2);
    md_destroy(&m);
}
TEST(paths_shape_arcto_full_ellipse) {
    SETUP(m);
    REQUIRE(U(ArcTo));
    BP(m.dc); U(ArcTo)(m.dc, 200, 200, 400, 300, 200, 200, 400, 300); EP(m.dc);
    int n = pd_get(m.dc);
    CHECK(n >= 6);
    EXPECT_EQ(g_pd.ty[0], PT_MOVETO); EXPECT_EQ(g_pd.ty[1], PT_LINETO);
    CHECK(pd_valid());
    md_destroy(&m);
}
TEST(paths_shape_anglearc) {
    SETUP(m);
    REQUIRE(U(AngleArc));
    MT(m.dc, 3, 4);
    BP(m.dc); CHECK(U(AngleArc)(m.dc, 30, 30, 20, 0.0f, 90.0f)); EP(m.dc);
    int n = pd_get(m.dc);
    CHECK(n >= 5); CHECK(pd_valid());
    CHECK(pd_near(0, 3, 4, 0));
    EXPECT_EQ(g_pd.ty[1], PT_LINETO);
    CHECK(pd_near(1, 50, 30, 1));
    POINT cur; U(GetCurrentPositionEx)(m.dc, &cur);
    CHECK(abs((int)cur.x - 30) <= 1 && abs((int)cur.y - 10) <= 1);   /* end of a +90 degree sweep (y up on screen) */
    md_destroy(&m);
}
TEST(paths_shape_anglearc_negative_sweep) {
    SETUP(m);
    REQUIRE(U(AngleArc));
    BP(m.dc); MT(m.dc, 0, 0); CHECK(U(AngleArc)(m.dc, 30, 30, 20, 0.0f, -90.0f)); EP(m.dc);
    pd_get(m.dc);
    CHECK(pd_valid());
    POINT cur; U(GetCurrentPositionEx)(m.dc, &cur);
    CHECK(abs((int)cur.x - 30) <= 1 && abs((int)cur.y - 50) <= 1);
    md_destroy(&m);
}
TEST(paths_shape_anglearc_zero_sweep) {
    SETUP(m);
    REQUIRE(U(AngleArc));
    BP(m.dc); MT(m.dc, 0, 0);
    BOOL r = U(AngleArc)(m.dc, 30, 30, 20, 45.0f, 0.0f);
    EP(m.dc);
    int n = pd_get(m.dc);
    t_note("AngleArc sweep 0 -> %d, %d points", r, n);
    CHECK(r);
    md_destroy(&m);
}
TEST(paths_shape_anglearc_full_circle_and_more) {
    SETUP(m);
    REQUIRE(U(AngleArc));
    BP(m.dc); MT(m.dc, 0, 0); CHECK(U(AngleArc)(m.dc, 30, 30, 20, 0.0f, 360.0f)); EP(m.dc);
    int n1 = pd_get(m.dc); CHECK(pd_valid());
    AP(m.dc);
    BP(m.dc); MT(m.dc, 0, 0); CHECK(U(AngleArc)(m.dc, 30, 30, 20, 0.0f, 720.0f)); EP(m.dc);
    int n2 = pd_get(m.dc); CHECK(pd_valid());
    t_note("AngleArc 360 -> %d pts, 720 -> %d pts", n1, n2);
    CHECK(n2 >= n1);
    md_destroy(&m);
}
TEST(paths_shape_anglearc_zero_radius) {
    SETUP(m);
    REQUIRE(U(AngleArc));
    BP(m.dc); MT(m.dc, 5, 5);
    SET_ERR(0xdeadbeef);
    BOOL r = U(AngleArc)(m.dc, 30, 30, 0, 0.0f, 90.0f);
    t_note("AngleArc r=0 -> %d le=%lu", r, GetLastError());
    EP(m.dc);
    md_destroy(&m);
}

/* ---- Beziers --------------------------------------------------------- */
TEST(paths_shape_polybezier) {
    SETUP(m);
    REQUIRE(U(PolyBezier));
    POINT p[7] = {{1, 1}, {5, 20}, {10, 20}, {15, 1}, {20, 1}, {25, 20}, {30, 5}};
    BP(m.dc); CHECK(U(PolyBezier)(m.dc, p, 7)); EP(m.dc);
    EXPECT_EQ(pd_get(m.dc), 7);
    EXPECT_EQ(g_pd.ty[0], PT_MOVETO);
    EXPECT_EQ(pd_count_type(PT_BEZIERTO), 6);
    CHECK(pd_near(6, 30, 5, 0)); CHECK(pd_near(3, 15, 1, 0));
    md_destroy(&m);
}
TEST(paths_shape_polybezier_bad_counts) {
    SETUP(m);
    REQUIRE(U(PolyBezier));
    POINT p[7] = {{1, 1}, {5, 20}, {10, 20}, {15, 1}, {20, 1}, {25, 20}, {30, 5}};
    int bad[] = {0, 1, 2, 3, 5, 6, 8};
    unsigned i;
    BP(m.dc);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        if (bad[i] > 7) continue;
        CHECKF(!U(PolyBezier)(m.dc, p, bad[i]), "count %d", bad[i]);
    }
    EP(m.dc);
    EXPECT_EQ(GPN(m.dc), 0);
    md_destroy(&m);
}
TEST(paths_shape_polybezierto) {
    SETUP(m);
    REQUIRE(U(PolyBezierTo));
    POINT p[6] = {{5, 20}, {10, 20}, {15, 1}, {20, 1}, {25, 20}, {30, 5}};
    MT(m.dc, 1, 1);
    BP(m.dc); CHECK(U(PolyBezierTo)(m.dc, p, 6)); EP(m.dc);
    EXPECT_EQ(pd_get(m.dc), 7);
    CHECK(pd_near(0, 1, 1, 0)); CHECK(pd_near(6, 30, 5, 0));
    EXPECT_EQ(pd_count_type(PT_BEZIERTO), 6); EXPECT_EQ(g_pd.ty[0], PT_MOVETO);
    POINT cur; U(GetCurrentPositionEx)(m.dc, &cur); EXPECT_EQ(cur.x, 30); EXPECT_EQ(cur.y, 5);
    md_destroy(&m);
}
TEST(paths_shape_polybezierto_bad_counts) {
    SETUP(m);
    REQUIRE(U(PolyBezierTo));
    POINT p[6] = {{5, 20}, {10, 20}, {15, 1}, {20, 1}, {25, 20}, {30, 5}};
    int bad[] = {0, 1, 2, 4, 5};
    unsigned i;
    BP(m.dc);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) CHECKF(!U(PolyBezierTo)(m.dc, p, bad[i]), "count %d", bad[i]);
    EP(m.dc);
    md_destroy(&m);
}
TEST(paths_shape_bezier_then_line) {
    SETUP(m);
    REQUIRE(U(PolyBezier));
    POINT p[4] = {{1, 1}, {5, 20}, {10, 20}, {15, 1}};
    BP(m.dc); U(PolyBezier)(m.dc, p, 4); LT(m.dc, 30, 30); EP(m.dc);
    int n = pd_get(m.dc);
    CHECK(n == 5 || n == 6);
    if (n != 5) t_note("bezier then lineto -> %d points", n);
    EXPECT_EQ(g_pd.ty[n - 1], PT_LINETO);
    CHECK(pd_near(n - 1, 30, 30, 0));
    CHECK(pd_valid());
    md_destroy(&m);
}

/* ---- PolyDraw -------------------------------------------------------- */
TEST(paths_polydraw_basic_lines) {
    SETUP(m);
    REQUIRE(U(PolyDraw));
    POINT p[4] = {{5, 5}, {10, 5}, {10, 10}, {20, 20}};
    BYTE t[4] = {PT_MOVETO, PT_LINETO, PT_LINETO, PT_LINETO | PT_CLOSEFIGURE};
    BP(m.dc); CHECK(U(PolyDraw)(m.dc, p, t, 4)); EP(m.dc);
    EXPECT_EQ(pd_get(m.dc), 4);
    EXPECT_EQ(g_pd.ty[0], PT_MOVETO); EXPECT_EQ(g_pd.ty[3], PT_LINETO | PT_CLOSEFIGURE);
    md_destroy(&m);
}
TEST(paths_polydraw_bezier_triplet) {
    SETUP(m);
    REQUIRE(U(PolyDraw));
    POINT p[4] = {{5, 5}, {10, 20}, {20, 20}, {25, 5}};
    BYTE t[4] = {PT_MOVETO, PT_BEZIERTO, PT_BEZIERTO, PT_BEZIERTO};
    BP(m.dc); CHECK(U(PolyDraw)(m.dc, p, t, 4)); EP(m.dc);
    EXPECT_EQ(pd_get(m.dc), 4);
    EXPECT_EQ(pd_count_type(PT_BEZIERTO), 3);
    md_destroy(&m);
}
TEST(paths_polydraw_bezier_not_multiple_of_three) {
    SETUP(m);
    REQUIRE(U(PolyDraw));
    POINT p[5] = {{5, 5}, {10, 20}, {20, 20}, {25, 5}, {30, 30}};
    BYTE t2[3] = {PT_MOVETO, PT_BEZIERTO, PT_BEZIERTO};
    BYTE t4[5] = {PT_MOVETO, PT_BEZIERTO, PT_BEZIERTO, PT_BEZIERTO, PT_BEZIERTO};
    BP(m.dc);
    CHECK(!U(PolyDraw)(m.dc, p, t2, 3));
    CHECK(!U(PolyDraw)(m.dc, p, t4, 5));
    AP(m.dc);
    md_destroy(&m);
}
TEST(paths_polydraw_invalid_type_bytes) {
    SETUP(m);
    REQUIRE(U(PolyDraw));
    POINT p[3] = {{5, 5}, {10, 20}, {20, 20}};
    BYTE bad[] = {0x00, 0x01, 0x08, 0x0A, 0x10, 0x20, 0x40, 0x80, 0xFE, 0xFF};
    unsigned i;
    BP(m.dc);
    for (i = 0; i < sizeof bad; i++) {
        BYTE t[3] = {PT_MOVETO, PT_LINETO, bad[i]};
        SET_ERR(0xdeadbeef);
        BOOL r = U(PolyDraw)(m.dc, p, t, 3);
        CHECKF(!r, "type byte 0x%02x accepted", bad[i]);
    }
    AP(m.dc);
    md_destroy(&m);
}
TEST(paths_polydraw_valid_type_bytes) {
    SETUP(m);
    REQUIRE(U(PolyDraw));
    POINT p[3] = {{5, 5}, {10, 20}, {20, 20}};
    BYTE ok[] = {PT_MOVETO, PT_LINETO, PT_LINETO | PT_CLOSEFIGURE, PT_MOVETO | PT_CLOSEFIGURE};
    unsigned i;
    for (i = 0; i < sizeof ok; i++) {
        BYTE t[3] = {PT_MOVETO, PT_LINETO, ok[i]};
        BP(m.dc);
        BOOL r = U(PolyDraw)(m.dc, p, t, 3);
        if (ok[i] == (PT_MOVETO | PT_CLOSEFIGURE)) { if (!r) t_note("PolyDraw trailing MOVETO|CLOSEFIGURE rejected"); }
        else CHECKF(r, "type 0x%02x", ok[i]);
        EP(m.dc);
        if (r) CHECK(pd_get(m.dc) >= 2);
        AP(m.dc);
    }
    md_destroy(&m);
}
TEST(paths_polydraw_updates_current_position) {
    SETUP(m);
    REQUIRE(U(PolyDraw));
    POINT p[3] = {{5, 5}, {10, 20}, {20, 25}}, cur;
    BYTE t[3] = {PT_MOVETO, PT_LINETO, PT_LINETO};
    BP(m.dc); CHECK(U(PolyDraw)(m.dc, p, t, 3));
    U(GetCurrentPositionEx)(m.dc, &cur); EXPECT_EQ(cur.x, 20); EXPECT_EQ(cur.y, 25);
    EP(m.dc);
    md_destroy(&m);
}
TEST(paths_polydraw_outside_path_draws) {
    SETUP(m);
    REQUIRE(U(PolyDraw));
    POINT p[2] = {{2, 5}, {20, 5}};
    BYTE t[2] = {PT_MOVETO, PT_LINETO};
    md_clear(&m, WHITE);
    CHECK(U(PolyDraw)(m.dc, p, t, 2));
    CHECK(md_px(&m, 10, 5) == 0);
    EXPECT_EQ(GPN(m.dc), -1);
    md_destroy(&m);
}
TEST(paths_polydraw_zero_and_negative_count) {
    SETUP(m);
    REQUIRE(U(PolyDraw));
    POINT p[1] = {{2, 5}}; BYTE t[1] = {PT_MOVETO};
    BP(m.dc);
    BOOL r0 = U(PolyDraw)(m.dc, p, t, 0);
    t_note("PolyDraw count 0 -> %d", r0);
    AP(m.dc);
    md_destroy(&m);
}
TEST(paths_polydraw_first_lineto_uses_current_pos) {
    SETUP(m);
    REQUIRE(U(PolyDraw));
    POINT p[2] = {{12, 12}, {20, 20}};
    BYTE t[2] = {PT_LINETO, PT_LINETO};
    MT(m.dc, 4, 4);
    BP(m.dc); CHECK(U(PolyDraw)(m.dc, p, t, 2)); EP(m.dc);
    int n = pd_get(m.dc);
    EXPECT_EQ(n, 3);
    CHECK(pd_near(0, 4, 4, 0)); EXPECT_EQ(g_pd.ty[0], PT_MOVETO);
    md_destroy(&m);
}

/* ---- Combined / validity over many primitives ------------------------- */
TEST(paths_shape_all_primitives_valid) {
    SETUP(m);
    POINT pts[9] = {{10, 10}, {20, 10}, {10, 20}, {20, 20}, {30, 30}, {40, 20}, {20, 30}, {30, 40}, {10, 50}};
    BP(m.dc);
    LT(m.dc, 50, 50); MT(m.dc, 50, 50); LT(m.dc, 55, 55);
    U(Polyline)(m.dc, pts, 4); U(Arc)(m.dc, 21, 21, 39, 29, 39, 29, 21, 21);
    U(Chord)(m.dc, 42, 43, 57, 66, 39, 29, 21, 21); U(Pie)(m.dc, 52, 54, 65, 60, 39, 29, 21, 21);
    U(Polygon)(m.dc, pts, 9); U(Ellipse)(m.dc, 22, 33, 44, 56); U(Rectangle)(m.dc, 60, 40, 63, 45);
    U(RoundRect)(m.dc, 1, 1, 60, 60, 35, 45); U(PolyBezier)(m.dc, pts, 7); U(PolyBezierTo)(m.dc, pts, 9);
    LT(m.dc, 15, 15);
    EP(m.dc);
    int n = pd_get(m.dc);
    CHECK(n > 60);
    CHECK(pd_valid());
    t_note("all primitives -> %d points", n);
    md_destroy(&m);
}
TEST(paths_shape_valid_after_every_primitive) {
    SETUP(m);
    POINT pts[7] = {{10, 10}, {20, 10}, {10, 20}, {20, 20}, {30, 30}, {40, 20}, {20, 30}};
    int k, bad = 0;
    for (k = 0; k < 10; k++) {
        BP(m.dc);
        switch (k) {
        case 0: U(Rectangle)(m.dc, 5, 5, 30, 30); break;
        case 1: U(Ellipse)(m.dc, 5, 5, 30, 30); break;
        case 2: U(RoundRect)(m.dc, 5, 5, 30, 30, 8, 8); break;
        case 3: U(Arc)(m.dc, 5, 5, 30, 30, 5, 5, 30, 30); break;
        case 4: U(Chord)(m.dc, 5, 5, 30, 30, 5, 5, 30, 30); break;
        case 5: U(Pie)(m.dc, 5, 5, 30, 30, 5, 5, 30, 30); break;
        case 6: U(Polygon)(m.dc, pts, 7); break;
        case 7: U(Polyline)(m.dc, pts, 7); break;
        case 8: U(PolyBezier)(m.dc, pts, 7); break;
        case 9: MT(m.dc, 3, 3); U(ArcTo)(m.dc, 5, 5, 30, 30, 5, 5, 30, 30); break;
        }
        EP(m.dc);
        if (pd_get(m.dc) <= 0 || !pd_valid()) { bad++; t_note("primitive %d produced an invalid path", k); }
        AP(m.dc);
    }
    EXPECT_EQ(bad, 0);
    md_destroy(&m);
}
TEST(paths_shape_large_path) {
    SETUP(m);
    int i, ok = 1;
    BP(m.dc); MT(m.dc, 0, 0);
    for (i = 1; i <= 5000; i++) if (!LT(m.dc, i & 63, (i * 7) & 63)) ok = 0;
    CHECK(EP(m.dc));
    CHECK(ok);
    int n = GPN(m.dc);
    EXPECT_EQ(n, 5001);
    if (n == 5001) {
        POINT *pts = malloc(n * sizeof(POINT)); BYTE *ty = malloc(n);
        EXPECT_EQ(U(GetPath)(m.dc, pts, ty, n), n);
        CHECK(pts[5000].x == (5000 & 63) && pts[5000].y == ((5000 * 7) & 63));
        free(pts); free(ty);
    }
    md_destroy(&m);
}
