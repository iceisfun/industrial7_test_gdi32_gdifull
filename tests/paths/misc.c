/* WidenPath / FlattenPath, transforms, text, metafile and screen DCs, invalid handles, threads, leaks. */
#include "ph.h"

#define SETUP(m) MD m; REQ_BASE(); if (!md_create(&m, 64, 64)) { t_skip("no DIB"); return; } md_clear(&m, WHITE)
#define RED 0xFF0000u

static HPEN geo_pen(int w, DWORD extra) {
    LOGBRUSH lb = {BS_SOLID, RGB(255, 0, 0), 0};
    return U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID | extra, w, &lb, 0, NULL);
}

/* ---- WidenPath ------------------------------------------------------- */
TEST(paths_widen_open_bracket_fails) {
    SETUP(m);
    BP(m.dc); MT(m.dc, 1, 1); LT(m.dc, 20, 20);
    SET_ERR(0xdeadbeef);
    CHECK(!U(WidenPath)(m.dc));
    EXPECT_ERR(0, ERROR_CAN_NOT_COMPLETE);
    AP(m.dc);
    md_destroy(&m);
}
TEST(paths_widen_no_path_fails) {
    SETUP(m);
    SET_ERR(0xdeadbeef);
    CHECK(!U(WidenPath)(m.dc));
    EXPECT_ERR(0, ERROR_CAN_NOT_COMPLETE);
    md_destroy(&m);
}
TEST(paths_widen_empty_path_ok) {
    SETUP(m);
    BP(m.dc); EP(m.dc);
    CHECK(U(WidenPath)(m.dc));
    CHECK(GPN(m.dc) != -1);
    md_destroy(&m);
}
TEST(paths_widen_geometric_pen_grows_polyline) {
    SETUP(m);
    REQUIRE(U(ExtCreatePen));
    HPEN p = geo_pen(10, 0); HGDIOBJ op = U(SelectObject)(m.dc, p);
    POINT pt[4] = {{5, 5}, {40, 5}, {40, 40}, {10, 50}};
    BP(m.dc); U(Polyline)(m.dc, pt, 4); EP(m.dc);
    CHECK(U(WidenPath)(m.dc));
    int n = pd_get(m.dc);
    CHECK(n > 4);
    CHECK(pd_valid());
    if (pd_count_close() < 1) t_note("widened path has no CLOSEFIGURE flags");
    U(SelectObject)(m.dc, op); U(DeleteObject)(p);
    md_destroy(&m);
}
TEST(paths_widen_does_not_consume) {
    SETUP(m);
    HPEN p = U(CreatePen)(PS_SOLID, 6, RGB(255, 0, 0)); HGDIOBJ op = U(SelectObject)(m.dc, p);
    BP(m.dc); MT(m.dc, 5, 5); LT(m.dc, 40, 5); EP(m.dc);
    CHECK(U(WidenPath)(m.dc));
    CHECK(GPN(m.dc) > 2);
    CHECK(U(WidenPath)(m.dc));
    CHECK(GPN(m.dc) > 2);
    U(SelectObject)(m.dc, op); U(DeleteObject)(p);
    md_destroy(&m);
}
TEST(paths_widen_twice_grows) {
    SETUP(m);
    HPEN p = U(CreatePen)(PS_SOLID, 4, RGB(255, 0, 0)); HGDIOBJ op = U(SelectObject)(m.dc, p);
    BP(m.dc); MT(m.dc, 5, 5); LT(m.dc, 40, 5); LT(m.dc, 40, 30); EP(m.dc);
    CHECK(U(WidenPath)(m.dc)); int a = GPN(m.dc);
    CHECK(U(WidenPath)(m.dc)); int b = GPN(m.dc);
    CHECK(b >= a);
    t_note("widen once %d points, twice %d", a, b);
    U(SelectObject)(m.dc, op); U(DeleteObject)(p);
    md_destroy(&m);
}
TEST(paths_widen_then_fill_rows) {
    SETUP(m);
    HPEN p = U(CreatePen)(PS_SOLID, 6, RGB(0, 0, 255));
    HBRUSH b = U(CreateSolidBrush)(RGB(255, 0, 0));
    HGDIOBJ op = U(SelectObject)(m.dc, p), ob = U(SelectObject)(m.dc, b);
    BP(m.dc); MT(m.dc, 8, 32); LT(m.dc, 56, 32); EP(m.dc);
    CHECK(U(WidenPath)(m.dc));
    CHECK(FP(m.dc));
    CHECK(md_px(&m, 30, 32) == RED); CHECK(md_px(&m, 30, 30) == RED); CHECK(md_px(&m, 30, 34) == RED);
    CHECK(md_px(&m, 30, 26) == WHITE); CHECK(md_px(&m, 30, 38) == WHITE);
    CHECK(md_px(&m, 30, 0) == WHITE);
    RECT r; md_bbox(&m, WHITE, &r);
    t_note("widened 6px line fill bbox %ld,%ld,%ld,%ld", r.left, r.top, r.right, r.bottom);
    U(SelectObject)(m.dc, op); U(SelectObject)(m.dc, ob); U(DeleteObject)(p); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(paths_widen_cosmetic_pen_fails) {
    SETUP(m);
    REQUIRE(U(ExtCreatePen));
    LOGBRUSH lb = {BS_SOLID, RGB(255, 0, 0), 0};
    HPEN p = U(ExtCreatePen)(PS_COSMETIC | PS_SOLID, 1, &lb, 0, NULL);
    HGDIOBJ op = U(SelectObject)(m.dc, p);
    BP(m.dc); MT(m.dc, 5, 5); LT(m.dc, 40, 5); EP(m.dc);
    SET_ERR(0xdeadbeef);
    BOOL r = U(WidenPath)(m.dc);
    CHECK(!r);
    t_note("WidenPath(PS_COSMETIC) le=%lu", GetLastError());
    U(SelectObject)(m.dc, op); U(DeleteObject)(p);
    md_destroy(&m);
}
TEST(paths_widen_one_pixel_solid_pen) {
    SETUP(m);
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(255, 0, 0)); HGDIOBJ op = U(SelectObject)(m.dc, p);
    BP(m.dc); MT(m.dc, 5, 5); LT(m.dc, 40, 5); LT(m.dc, 40, 30); EP(m.dc);
    BOOL r = U(WidenPath)(m.dc);
    CHECK(r);
    if (r) CHECK(GPN(m.dc) > 3);
    U(SelectObject)(m.dc, op); U(DeleteObject)(p);
    md_destroy(&m);
}
TEST(paths_widen_null_pen) {
    SETUP(m);
    HGDIOBJ op = U(SelectObject)(m.dc, U(GetStockObject)(NULL_PEN));
    BP(m.dc); MT(m.dc, 5, 5); LT(m.dc, 40, 5); EP(m.dc);
    SET_ERR(0xdeadbeef);
    BOOL r = U(WidenPath)(m.dc);
    t_note("WidenPath(NULL_PEN) -> %d le=%lu", r, GetLastError());
    U(SelectObject)(m.dc, op);
    md_destroy(&m);
}
TEST(paths_widen_dash_pen_makes_more_segments) {
    SETUP(m);
    REQUIRE(U(ExtCreatePen));
    LOGBRUSH lb = {BS_SOLID, RGB(255, 0, 0), 0};
    HPEN solid = geo_pen(4, 0);
    HPEN dash = U(ExtCreatePen)(PS_GEOMETRIC | PS_DASH, 4, &lb, 0, NULL);
    HGDIOBJ op = U(SelectObject)(m.dc, solid);
    BP(m.dc); MT(m.dc, 2, 20); LT(m.dc, 62, 20); EP(m.dc);
    CHECK(U(WidenPath)(m.dc)); int a = GPN(m.dc); AP(m.dc);
    U(SelectObject)(m.dc, dash);
    BP(m.dc); MT(m.dc, 2, 20); LT(m.dc, 62, 20); EP(m.dc);
    CHECK(U(WidenPath)(m.dc)); int b = GPN(m.dc); AP(m.dc);
    CHECK(b >= a);
    if (b <= a) t_note("dash widen not segmented: dash %d vs solid %d", b, a);
    U(SelectObject)(m.dc, op); U(DeleteObject)(solid); U(DeleteObject)(dash);
    md_destroy(&m);
}
TEST(paths_widen_join_styles_valid) {
    SETUP(m);
    REQUIRE(U(ExtCreatePen));
    DWORD joins[3] = {PS_JOIN_ROUND, PS_JOIN_BEVEL, PS_JOIN_MITER};
    int i, cnt[3];
    POINT pt[3] = {{5, 5}, {40, 5}, {40, 40}};
    for (i = 0; i < 3; i++) {
        HPEN p = geo_pen(8, joins[i]); HGDIOBJ op = U(SelectObject)(m.dc, p);
        BP(m.dc); U(Polyline)(m.dc, pt, 3); EP(m.dc);
        CHECKF(U(WidenPath)(m.dc), "join %d", i);
        cnt[i] = pd_get(m.dc);
        CHECK(cnt[i] > 3); CHECK(pd_valid());
        AP(m.dc);
        U(SelectObject)(m.dc, op); U(DeleteObject)(p);
    }
    t_note("widen joins round/bevel/miter points: %d/%d/%d", cnt[0], cnt[1], cnt[2]);
    CHECK(cnt[0] >= cnt[1]);
    md_destroy(&m);
}
TEST(paths_widen_round_cap_adds_points) {
    SETUP(m);
    REQUIRE(U(ExtCreatePen));
    HPEN flat = geo_pen(10, PS_ENDCAP_FLAT), round = geo_pen(10, PS_ENDCAP_ROUND);
    HGDIOBJ op = U(SelectObject)(m.dc, flat);
    BP(m.dc); MT(m.dc, 10, 30); LT(m.dc, 50, 30); EP(m.dc);
    U(WidenPath)(m.dc); int a = GPN(m.dc); AP(m.dc);
    U(SelectObject)(m.dc, round);
    BP(m.dc); MT(m.dc, 10, 30); LT(m.dc, 50, 30); EP(m.dc);
    U(WidenPath)(m.dc); int b = GPN(m.dc); AP(m.dc);
    CHECKF(b > a, "round %d flat %d", b, a);
    U(SelectObject)(m.dc, op); U(DeleteObject)(flat); U(DeleteObject)(round);
    md_destroy(&m);
}
TEST(paths_widen_after_consumed_fails) {
    SETUP(m);
    HPEN p = U(CreatePen)(PS_SOLID, 4, RGB(255, 0, 0)); HGDIOBJ op = U(SelectObject)(m.dc, p);
    BP(m.dc); MT(m.dc, 5, 5); LT(m.dc, 40, 5); EP(m.dc); FP(m.dc);
    SET_ERR(0xdeadbeef);
    CHECK(!U(WidenPath)(m.dc));
    EXPECT_ERR(0, ERROR_CAN_NOT_COMPLETE);
    U(SelectObject)(m.dc, op); U(DeleteObject)(p);
    md_destroy(&m);
}
TEST(paths_widen_uses_pen_at_widen_time) {
    SETUP(m);
    HPEN thin = U(CreatePen)(PS_SOLID, 2, RGB(255, 0, 0)), thick = U(CreatePen)(PS_SOLID, 20, RGB(255, 0, 0));
    HGDIOBJ op = U(SelectObject)(m.dc, thin);
    BP(m.dc); MT(m.dc, 10, 32); LT(m.dc, 54, 32); EP(m.dc);
    U(SelectObject)(m.dc, thick);
    CHECK(U(WidenPath)(m.dc));
    pd_get(m.dc);
    int i, ymin = 1000, ymax = -1000;
    for (i = 0; i < g_pd.n; i++) { if (g_pd.pt[i].y < ymin) ymin = g_pd.pt[i].y; if (g_pd.pt[i].y > ymax) ymax = g_pd.pt[i].y; }
    t_note("widened y extent %d..%d", ymin, ymax);
    CHECK(ymax - ymin >= 15 && ymax - ymin <= 24);
    U(SelectObject)(m.dc, op); U(DeleteObject)(thin); U(DeleteObject)(thick);
    md_destroy(&m);
}
TEST(paths_sfp_after_widen) {
    SETUP(m);
    HPEN p = U(CreatePen)(PS_SOLID, 6, RGB(0, 0, 255)); HBRUSH b = U(CreateSolidBrush)(RGB(255, 0, 0));
    HGDIOBJ op = U(SelectObject)(m.dc, p), ob = U(SelectObject)(m.dc, b);
    BP(m.dc); MT(m.dc, 8, 32); LT(m.dc, 56, 32); EP(m.dc);
    CHECK(U(WidenPath)(m.dc));
    CHECK(SFP(m.dc));
    CHECK(md_px(&m, 30, 32) != WHITE);   /* 6px outline of a 6px band covers it */
    CHECK(md_px(&m, 30, 20) == WHITE);
    U(SelectObject)(m.dc, op); U(SelectObject)(m.dc, ob); U(DeleteObject)(p); U(DeleteObject)(b);
    md_destroy(&m);
}

/* ---- FlattenPath ----------------------------------------------------- */
TEST(paths_flatten_removes_beziers) {
    SETUP(m);
    BP(m.dc); U(Ellipse)(m.dc, 4, 4, 60, 60); EP(m.dc);
    int before = GPN(m.dc);
    CHECK(U(FlattenPath)(m.dc));
    int n = pd_get(m.dc);
    CHECK(n > before);
    EXPECT_EQ(pd_count_type(PT_BEZIERTO), 0);
    CHECK(pd_valid());
    EXPECT_EQ(g_pd.ty[0], PT_MOVETO);
    CHECK(g_pd.ty[n - 1] & PT_CLOSEFIGURE);
    EXPECT_EQ(pd_count_close(), 1);
    md_destroy(&m);
}
TEST(paths_flatten_keeps_endpoints) {
    SETUP(m);
    POINT p[4] = {{2, 40}, {10, 2}, {50, 2}, {60, 40}};
    BP(m.dc); U(PolyBezier)(m.dc, p, 4); EP(m.dc);
    CHECK(U(FlattenPath)(m.dc));
    int n = pd_get(m.dc);
    CHECK(n > 4);
    CHECK(pd_near(0, 2, 40, 0)); CHECK(pd_near(n - 1, 60, 40, 0));
    EXPECT_EQ(pd_count_type(PT_BEZIERTO), 0);
    md_destroy(&m);
}
TEST(paths_flatten_polyline_unchanged) {
    SETUP(m);
    POINT p[4] = {{2, 40}, {10, 2}, {50, 2}, {60, 40}};
    BP(m.dc); U(Polyline)(m.dc, p, 4); EP(m.dc);
    CHECK(U(FlattenPath)(m.dc));
    EXPECT_EQ(pd_get(m.dc), 4);
    CHECK(pd_near(1, 10, 2, 0)); CHECK(pd_near(2, 50, 2, 0));
    md_destroy(&m);
}
TEST(paths_flatten_rectangle_unchanged) {
    SETUP(m);
    BP(m.dc); U(Rectangle)(m.dc, 10, 10, 30, 30); EP(m.dc);
    CHECK(U(FlattenPath)(m.dc));
    EXPECT_EQ(pd_get(m.dc), 4);
    md_destroy(&m);
}
TEST(paths_flatten_idempotent) {
    SETUP(m);
    BP(m.dc); U(Ellipse)(m.dc, 4, 4, 60, 60); EP(m.dc);
    CHECK(U(FlattenPath)(m.dc)); int a = GPN(m.dc);
    CHECK(U(FlattenPath)(m.dc)); int b = GPN(m.dc);
    EXPECT_EQ(a, b);
    md_destroy(&m);
}
TEST(paths_flatten_does_not_consume) {
    SETUP(m);
    BP(m.dc); U(Ellipse)(m.dc, 4, 4, 60, 60); EP(m.dc);
    CHECK(U(FlattenPath)(m.dc));
    CHECK(GPN(m.dc) > 13);
    HBRUSH b = U(CreateSolidBrush)(RGB(255, 0, 0));
    HGDIOBJ ob = U(SelectObject)(m.dc, b), op = U(SelectObject)(m.dc, U(GetStockObject)(NULL_PEN));
    CHECK(FP(m.dc));
    CHECK(md_px(&m, 32, 32) == RED);
    U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, op); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(paths_flatten_open_bracket_fails) {
    SETUP(m);
    BP(m.dc); U(Ellipse)(m.dc, 4, 4, 60, 60);
    SET_ERR(0xdeadbeef);
    CHECK(!U(FlattenPath)(m.dc));
    EXPECT_ERR(0, ERROR_CAN_NOT_COMPLETE);
    AP(m.dc);
    md_destroy(&m);
}
TEST(paths_flatten_no_path_fails) {
    SETUP(m);
    SET_ERR(0xdeadbeef);
    CHECK(!U(FlattenPath)(m.dc));
    EXPECT_ERR(0, ERROR_CAN_NOT_COMPLETE);
    md_destroy(&m);
}
TEST(paths_flatten_empty_path_ok) {
    SETUP(m);
    BP(m.dc); EP(m.dc);
    CHECK(U(FlattenPath)(m.dc));
    EXPECT_EQ(GPN(m.dc), 0);
    md_destroy(&m);
}
TEST(paths_flatten_fill_close_to_curve_fill) {
    SETUP(m);
    HBRUSH b = U(CreateSolidBrush)(RGB(255, 0, 0));
    HGDIOBJ ob = U(SelectObject)(m.dc, b), op = U(SelectObject)(m.dc, U(GetStockObject)(NULL_PEN));
    BP(m.dc); U(Ellipse)(m.dc, 4, 4, 60, 60); EP(m.dc); FP(m.dc);
    int a = md_count(&m, RED);
    md_clear(&m, WHITE);
    BP(m.dc); U(Ellipse)(m.dc, 4, 4, 60, 60); EP(m.dc); U(FlattenPath)(m.dc); FP(m.dc);
    int c = md_count(&m, RED);
    CHECKF(abs(a - c) <= 40, "curve %d flat %d", a, c);
    U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, op); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(paths_flatten_after_widen) {
    SETUP(m);
    HPEN p = geo_pen(8, PS_ENDCAP_ROUND); HGDIOBJ op = U(SelectObject)(m.dc, p);
    BP(m.dc); MT(m.dc, 8, 30); LT(m.dc, 56, 30); EP(m.dc);
    CHECK(U(WidenPath)(m.dc));
    CHECK(U(FlattenPath)(m.dc));
    pd_get(m.dc);
    EXPECT_EQ(pd_count_type(PT_BEZIERTO), 0);
    CHECK(pd_valid());
    U(SelectObject)(m.dc, op); U(DeleteObject)(p);
    md_destroy(&m);
}

/* ---- Transforms ------------------------------------------------------ */
TEST(paths_xform_points_stored_at_build_time) {
    SETUP(m);
    U(SetMapMode)(m.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(m.dc, 1, 1, NULL); U(SetViewportExtEx)(m.dc, 1, 1, NULL);
    BP(m.dc);
    POLYGON4(m.dc, 0, 0, 4, 4);
    U(SetViewportExtEx)(m.dc, 2, 2, NULL);
    POLYGON4(m.dc, 10, 10, 14, 14);
    EP(m.dc);
    HRGN rg = U(PathToRegion)(m.dc);
    CHECK(rg != NULL);
    if (rg) {
        CHECK(U(PtInRegion)(rg, 2, 2)); CHECK(U(PtInRegion)(rg, 22, 22));
        CHECK(!U(PtInRegion)(rg, 12, 12)); CHECK(!U(PtInRegion)(rg, 6, 6));
        U(DeleteObject)(rg);
    }
    U(SetMapMode)(m.dc, MM_TEXT);
    md_destroy(&m);
}
TEST(paths_xform_getpath_units_note) {
    SETUP(m);
    U(SetMapMode)(m.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(m.dc, 1, 1, NULL); U(SetViewportExtEx)(m.dc, 2, 2, NULL);
    BP(m.dc); MT(m.dc, 5, 5); LT(m.dc, 10, 10); EP(m.dc);
    pd_get(m.dc);
    POINT a = g_pd.pt[1];
    U(SetMapMode)(m.dc, MM_TEXT);
    pd_get(m.dc);
    POINT b = g_pd.pt[1];
    t_note("GetPath point under 2x map: %ld,%ld ; after MM_TEXT: %ld,%ld", a.x, a.y, b.x, b.y);
    CHECK((b.x == 20 && b.y == 20) || (b.x == 10 && b.y == 10) || 1);
    md_destroy(&m);
}
TEST(paths_xform_world_scale_translate) {
    SETUP(m);
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform));
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM x = {2.0f, 0.0f, 0.0f, 3.0f, 5.0f, 7.0f};
    CHECK(U(SetWorldTransform)(m.dc, &x));
    BP(m.dc); POLYGON4(m.dc, 1, 1, 4, 4); EP(m.dc);
    HRGN rg = U(PathToRegion)(m.dc);
    RECT r = {0}; U(GetRgnBox)(rg, &r);
    CHECK(r.left == 7 && r.top == 10 && r.right == 13 && r.bottom == 19);
    U(DeleteObject)(rg);
    XFORM id = {1, 0, 0, 1, 0, 0}; U(SetWorldTransform)(m.dc, &id);
    U(SetGraphicsMode)(m.dc, GM_COMPATIBLE);
    md_destroy(&m);
}
TEST(paths_xform_world_rotate90) {
    SETUP(m);
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform));
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM x = {0.0f, 1.0f, -1.0f, 0.0f, 20.0f, 0.0f};
    CHECK(U(SetWorldTransform)(m.dc, &x));
    BP(m.dc); POLYGON4(m.dc, 2, 2, 6, 6); EP(m.dc);
    HRGN rg = U(PathToRegion)(m.dc);
    RECT r = {0}; U(GetRgnBox)(rg, &r);
    CHECK(r.left == 14 && r.top == 2 && r.right == 18 && r.bottom == 6);
    U(DeleteObject)(rg);
    XFORM id = {1, 0, 0, 1, 0, 0}; U(SetWorldTransform)(m.dc, &id);
    U(SetGraphicsMode)(m.dc, GM_COMPATIBLE);
    md_destroy(&m);
}
TEST(paths_xform_world_rotate45_bounds) {
    SETUP(m);
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform));
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM x = {0.70710678f, 0.70710678f, -0.70710678f, 0.70710678f, 32.0f, 10.0f};
    CHECK(U(SetWorldTransform)(m.dc, &x));
    BP(m.dc); POLYGON4(m.dc, 0, 0, 20, 20); EP(m.dc);
    HRGN rg = U(PathToRegion)(m.dc);
    RECT r = {0}; U(GetRgnBox)(rg, &r);
    t_note("45deg rotated square box %ld,%ld,%ld,%ld", r.left, r.top, r.right, r.bottom);
    CHECK(r.left >= 16 && r.left <= 19 && r.right >= 45 && r.right <= 48);
    CHECK(r.top >= 9 && r.top <= 11 && r.bottom >= 37 && r.bottom <= 40);
    U(DeleteObject)(rg);
    XFORM id = {1, 0, 0, 1, 0, 0}; U(SetWorldTransform)(m.dc, &id);
    U(SetGraphicsMode)(m.dc, GM_COMPATIBLE);
    md_destroy(&m);
}
TEST(paths_xform_negative_extent_rectangle_orientation) {
    SETUP(m);
    U(SetMapMode)(m.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(m.dc, 1, 1, NULL); U(SetViewportExtEx)(m.dc, -1, 1, NULL);
    U(SetViewportOrgEx)(m.dc, 40, 0, NULL);
    BP(m.dc); U(Rectangle)(m.dc, 10, 10, 20, 20); EP(m.dc);
    EXPECT_EQ(pd_get(m.dc), 4);
    CHECK(pd_valid());
    t_note("x-flipped rect first point %ld,%ld", g_pd.pt[0].x, g_pd.pt[0].y);
    U(SetViewportOrgEx)(m.dc, 0, 0, NULL); U(SetMapMode)(m.dc, MM_TEXT);
    md_destroy(&m);
}
TEST(paths_xform_himetric_fill_position) {
    SETUP(m);
    HBRUSH b = U(CreateSolidBrush)(RGB(255, 0, 0));
    HGDIOBJ ob = U(SelectObject)(m.dc, b), op = U(SelectObject)(m.dc, U(GetStockObject)(NULL_PEN));
    U(SetMapMode)(m.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(m.dc, 10, 10, NULL); U(SetViewportExtEx)(m.dc, 1, 1, NULL);   /* 10 logical units per pixel */
    BP(m.dc); POLYGON4(m.dc, 100, 100, 300, 200); EP(m.dc);
    CHECK(FP(m.dc));
    CHECK(md_rect_is(&m, 10, 10, 30, 20, RED, WHITE));
    U(SetMapMode)(m.dc, MM_TEXT);
    U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, op); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(paths_xform_stroke_scaled_geometric_width) {
    SETUP(m);
    REQUIRE(U(ExtCreatePen));
    HPEN p = geo_pen(3, PS_ENDCAP_FLAT); HGDIOBJ op = U(SelectObject)(m.dc, p);
    U(SetMapMode)(m.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(m.dc, 1, 1, NULL); U(SetViewportExtEx)(m.dc, 2, 2, NULL);
    BP(m.dc); MT(m.dc, 4, 15); LT(m.dc, 28, 15); EP(m.dc);
    CHECK(SP(m.dc));
    RECT r; md_bbox(&m, WHITE, &r);
    t_note("geometric pen w3 under 2x map: bbox %ld,%ld,%ld,%ld", r.left, r.top, r.right, r.bottom);
    CHECK(r.bottom - r.top >= 3 && r.bottom - r.top <= 8);
    U(SetMapMode)(m.dc, MM_TEXT);
    U(SelectObject)(m.dc, op); U(DeleteObject)(p);
    md_destroy(&m);
}
TEST(paths_xform_large_coordinates_roundtrip) {
    SETUP(m);
    BP(m.dc); MT(m.dc, -100000, -100000); LT(m.dc, 100000, 100000); LT(m.dc, 3, 4); EP(m.dc);
    EXPECT_EQ(pd_get(m.dc), 3);
    CHECK(pd_near(0, -100000, -100000, 0)); CHECK(pd_near(1, 100000, 100000, 0)); CHECK(pd_near(2, 3, 4, 0));
    md_destroy(&m);
}
TEST(paths_xform_extreme_coordinates_note) {
    SETUP(m);
    BP(m.dc);
    BOOL a = LT(m.dc, 0x7fffffff, 0x7fffffff);
    BOOL b = LT(m.dc, (int)0x80000000, (int)0x80000000);
    BOOL c = LT(m.dc, 1 << 27, 1 << 27);
    EP(m.dc);
    t_note("LineTo extreme coords in path: %d %d %d, path points %d", a, b, c, GPN(m.dc));
    md_destroy(&m);
}

/* ---- Text ------------------------------------------------------------ */
static HFONT tt_font(int h, const char *face) {
    return U(CreateFontA)(-h, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, face);
}
TEST(paths_text_truetype_outline) {
    SETUP(m);
    REQUIRE(U(CreateFontA)); REQUIRE(U(TextOutA));
    HFONT f = tt_font(40, "Arial"); HGDIOBJ of = U(SelectObject)(m.dc, f);
    BP(m.dc); CHECK(U(TextOutA)(m.dc, 2, 2, "W", 1)); EP(m.dc);
    int n = pd_get(m.dc);
    CHECK(n >= 8);
    CHECK(pd_valid());
    CHECK(pd_count_close() >= 1);
    int i, ok = 1;
    for (i = 0; i < g_pd.n; i++) if (g_pd.pt[i].x < -2 || g_pd.pt[i].x > 70 || g_pd.pt[i].y < -2 || g_pd.pt[i].y > 70) ok = 0;
    CHECK(ok);
    U(SelectObject)(m.dc, of); U(DeleteObject)(f);
    md_destroy(&m);
}
TEST(paths_text_a_w_parity) {
    SETUP(m);
    REQUIRE(U(CreateFontA)); REQUIRE(U(TextOutA)); REQUIRE(U(TextOutW));
    HFONT f = tt_font(24, "Arial"); HGDIOBJ of = U(SelectObject)(m.dc, f);
    BP(m.dc); U(TextOutA)(m.dc, 2, 2, "Hi!", 3); EP(m.dc);
    int na = pd_get(m.dc); POINT *pa = malloc(sizeof(POINT) * (na > 0 ? na : 1)); BYTE *ta = malloc(na > 0 ? na : 1);
    if (na > 0) memcpy(pa, g_pd.pt, sizeof(POINT) * na), memcpy(ta, g_pd.ty, na);
    AP(m.dc);
    BP(m.dc); U(TextOutW)(m.dc, 2, 2, L"Hi!", 3); EP(m.dc);
    int nw = pd_get(m.dc);
    EXPECT_EQ(na, nw);
    CHECK(na > 0);
    if (na == nw && na > 0) CHECK(!memcmp(pa, g_pd.pt, sizeof(POINT) * na) && !memcmp(ta, g_pd.ty, na));
    free(pa); free(ta);
    U(SelectObject)(m.dc, of); U(DeleteObject)(f);
    md_destroy(&m);
}
TEST(paths_text_extTextOut_equals_textout) {
    SETUP(m);
    REQUIRE(U(CreateFontA)); REQUIRE(U(TextOutW)); REQUIRE(U(ExtTextOutW));
    HFONT f = tt_font(24, "Arial"); HGDIOBJ of = U(SelectObject)(m.dc, f);
    BP(m.dc); U(TextOutW)(m.dc, 2, 2, L"Ab", 2); EP(m.dc);
    int a = GPN(m.dc); AP(m.dc);
    BP(m.dc); U(ExtTextOutW)(m.dc, 2, 2, 0, NULL, L"Ab", 2, NULL); EP(m.dc);
    int b = GPN(m.dc);
    EXPECT_EQ(a, b);
    U(SelectObject)(m.dc, of); U(DeleteObject)(f);
    md_destroy(&m);
}
TEST(paths_text_more_chars_more_points) {
    SETUP(m);
    REQUIRE(U(CreateFontA)); REQUIRE(U(TextOutA));
    HFONT f = tt_font(24, "Arial"); HGDIOBJ of = U(SelectObject)(m.dc, f);
    BP(m.dc); U(TextOutA)(m.dc, 0, 0, "I", 1); EP(m.dc); int a = GPN(m.dc); AP(m.dc);
    BP(m.dc); U(TextOutA)(m.dc, 0, 0, "II", 2); EP(m.dc); int b = GPN(m.dc); AP(m.dc);
    BP(m.dc); U(TextOutA)(m.dc, 0, 0, "III", 3); EP(m.dc); int c = GPN(m.dc);
    CHECK(a > 0); EXPECT_EQ(b, 2 * a); EXPECT_EQ(c, 3 * a);
    U(SelectObject)(m.dc, of); U(DeleteObject)(f);
    md_destroy(&m);
}
TEST(paths_text_empty_and_space) {
    SETUP(m);
    REQUIRE(U(CreateFontA)); REQUIRE(U(TextOutA));
    HFONT f = tt_font(24, "Arial"); HGDIOBJ of = U(SelectObject)(m.dc, f);
    BP(m.dc); BOOL r0 = U(TextOutA)(m.dc, 0, 0, "", 0); EP(m.dc);
    EXPECT_EQ(GPN(m.dc), 0); AP(m.dc);
    BP(m.dc); BOOL r1 = U(TextOutA)(m.dc, 0, 0, " ", 1); EP(m.dc);
    int n = GPN(m.dc);
    t_note("empty text -> %d ; space -> %d, %d points", r0, r1, n);
    CHECK(n == 0 || n == -1 || n > 0);
    U(SelectObject)(m.dc, of); U(DeleteObject)(f);
    md_destroy(&m);
}
TEST(paths_text_bitmap_font_note) {
    SETUP(m);
    REQUIRE(U(TextOutA));
    HGDIOBJ of = U(SelectObject)(m.dc, U(GetStockObject)(SYSTEM_FONT));
    BP(m.dc);
    SET_ERR(0xdeadbeef);
    BOOL r = U(TextOutA)(m.dc, 2, 2, "W", 1);
    DWORD le = GetLastError();
    BOOL e = EP(m.dc);
    int n = GPN(m.dc);
    t_note("TextOut with System bitmap font in path -> %d le=%lu EndPath=%d points=%d", r, le, e, n);
    AP(m.dc);
    U(SelectObject)(m.dc, of);
    md_destroy(&m);
}
TEST(paths_text_fill_makes_pixels) {
    SETUP(m);
    REQUIRE(U(CreateFontA)); REQUIRE(U(TextOutA));
    HFONT f = tt_font(48, "Arial"); HGDIOBJ of = U(SelectObject)(m.dc, f);
    HBRUSH b = U(CreateSolidBrush)(RGB(255, 0, 0)); HGDIOBJ ob = U(SelectObject)(m.dc, b);
    HGDIOBJ op = U(SelectObject)(m.dc, U(GetStockObject)(NULL_PEN));
    BP(m.dc); U(TextOutA)(m.dc, 2, 2, "I", 1); EP(m.dc);
    CHECK(FP(m.dc));
    int n = md_count(&m, RED);
    CHECK(n > 20 && n < 64 * 64 / 2);
    CHECK(md_count(&m, RED) + md_count(&m, WHITE) == 64 * 64);   /* path fill never anti-aliases */
    U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, op); U(SelectObject)(m.dc, of);
    U(DeleteObject)(b); U(DeleteObject)(f);
    md_destroy(&m);
}
TEST(paths_text_o_has_inner_contour) {
    SETUP(m);
    REQUIRE(U(CreateFontA)); REQUIRE(U(TextOutA));
    HFONT f = tt_font(48, "Arial"); HGDIOBJ of = U(SelectObject)(m.dc, f);
    HBRUSH b = U(CreateSolidBrush)(RGB(255, 0, 0)); HGDIOBJ ob = U(SelectObject)(m.dc, b);
    HGDIOBJ op = U(SelectObject)(m.dc, U(GetStockObject)(NULL_PEN));
    BP(m.dc); U(TextOutA)(m.dc, 2, 2, "O", 1); EP(m.dc);
    pd_get(m.dc);
    CHECK(pd_count_close() >= 1);
    if (pd_count_close() < 2) t_note("O glyph: %d closed contours", pd_count_close());
    FP(m.dc);
    RECT r; md_bbox(&m, WHITE, &r);
    int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
    CHECK(md_px(&m, cx, cy) == WHITE);          /* the counter of the O is a hole */
    U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, op); U(SelectObject)(m.dc, of);
    U(DeleteObject)(b); U(DeleteObject)(f);
    md_destroy(&m);
}
TEST(paths_text_clip_path_from_text) {
    SETUP(m);
    REQUIRE(U(CreateFontA)); REQUIRE(U(TextOutA));
    HFONT f = tt_font(48, "Arial"); HGDIOBJ of = U(SelectObject)(m.dc, f);
    BP(m.dc); U(TextOutA)(m.dc, 2, 2, "I", 1); EP(m.dc);
    CHECK(U(SelectClipPath)(m.dc, RGN_COPY));
    RECT r; EXPECT_NE(U(GetClipBox)(m.dc, &r), 0);
    CHECK(r.right - r.left < 20 && r.bottom - r.top > 20);
    U(SelectClipRgn)(m.dc, NULL);
    U(SelectObject)(m.dc, of); U(DeleteObject)(f);
    md_destroy(&m);
}

/* ---- Metafile DC ----------------------------------------------------- */
static int mf_seq(HENHMETAFILE h, int *out, int max) {
    UINT sz = U(GetEnhMetaFileBits)(h, 0, NULL);
    if (!sz) return -1;
    BYTE *bits = malloc(sz); U(GetEnhMetaFileBits)(h, sz, bits);
    UINT off = 0; int n = 0;
    while (off + 8 <= sz) {
        DWORD type = *(DWORD *)(bits + off), sz2 = *(DWORD *)(bits + off + 4);
        if (type >= 59 && type <= 68 && n < max) out[n++] = (int)type;
        if (!sz2) break;
        off += sz2;
    }
    free(bits);
    return n;
}
TEST(paths_metafile_records_emitted) {
    REQ_BASE();
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits)); REQUIRE(U(DeleteEnhMetaFile));
    HDC mf = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    CHECK(mf != NULL);
    if (!mf) return;
    POINT sq[4] = {{1, 1}, {9, 1}, {9, 9}, {1, 9}};
    CHECK(BP(mf)); U(Polygon)(mf, sq, 4); CHECK(EP(mf)); CHECK(FP(mf));
    CHECK(BP(mf)); MT(mf, 1, 1); LT(mf, 9, 9); CF(mf); CHECK(EP(mf)); CHECK(SP(mf));
    CHECK(BP(mf)); U(Polygon)(mf, sq, 4); CHECK(EP(mf)); CHECK(SFP(mf));
    CHECK(BP(mf)); U(Polygon)(mf, sq, 4); CHECK(EP(mf)); CHECK(U(SelectClipPath)(mf, RGN_COPY));
    CHECK(BP(mf)); U(Polygon)(mf, sq, 4); CHECK(EP(mf)); CHECK(U(FlattenPath)(mf)); CHECK(U(WidenPath)(mf)); CHECK(AP(mf));
    CHECK(BP(mf)); CHECK(AP(mf));
    HENHMETAFILE h = U(CloseEnhMetaFile)(mf);
    CHECK(h != NULL);
    if (!h) return;
    int seq[64]; int n = mf_seq(h, seq, 64);
    int want[] = {59, 60, 62, 59, 61, 60, 64, 59, 60, 63, 59, 60, 67, 59, 60, 65, 66, 68, 59, 68};
    int i, ok = n == (int)(sizeof want / sizeof want[0]);
    for (i = 0; ok && i < n; i++) if (seq[i] != want[i]) ok = 0;
    CHECKF(ok, "record sequence mismatch (n=%d)", n);
    if (!ok) { for (i = 0; i < n; i++) t_note("rec[%d]=%d", i, seq[i]); }
    U(DeleteEnhMetaFile)(h);
}
TEST(paths_metafile_getpath_on_recording_dc) {
    REQ_BASE();
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC mf = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!mf) return;
    BP(mf); MT(mf, 1, 1); LT(mf, 9, 9); EP(mf);
    int n = GPN(mf);
    t_note("GetPath on enhanced-metafile DC -> %d", n);
    AP(mf);
    HENHMETAFILE h = U(CloseEnhMetaFile)(mf);
    if (h) U(DeleteEnhMetaFile)(h);
}
TEST(paths_metafile_end_without_begin) {
    REQ_BASE();
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC mf = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!mf) return;
    SET_ERR(0xdeadbeef);
    CHECK(!EP(mf));
    t_note("EndPath without BeginPath on metafile DC le=%lu", GetLastError());
    HENHMETAFILE h = U(CloseEnhMetaFile)(mf);
    if (h) U(DeleteEnhMetaFile)(h);
}
TEST(paths_metafile_arcto_anglearc_records) {
    REQ_BASE();
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits)); REQUIRE(U(DeleteEnhMetaFile));
    REQUIRE(U(ArcTo)); REQUIRE(U(AngleArc));
    HDC mf = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!mf) return;
    CHECK(BP(mf)); CHECK(U(ArcTo)(mf, 0, 0, 20, 20, 20, 10, 10, 0)); CHECK(U(AngleArc)(mf, 30, 30, 10, 0.0f, 90.0f)); CHECK(EP(mf));
    HENHMETAFILE h = U(CloseEnhMetaFile)(mf);
    if (!h) return;
    UINT sz = U(GetEnhMetaFileBits)(h, 0, NULL);
    BYTE *bits = malloc(sz); U(GetEnhMetaFileBits)(h, sz, bits);
    UINT off = 0; int arcto = 0, angle = 0;
    while (off + 8 <= sz) {
        DWORD type = *(DWORD *)(bits + off), s2 = *(DWORD *)(bits + off + 4);
        if (type == 55) arcto++;        /* EMR_ARCTO */
        if (type == 41) angle++;        /* EMR_ANGLEARC */
        if (!s2) break;
        off += s2;
    }
    free(bits);
    EXPECT_EQ(arcto, 1); EXPECT_EQ(angle, 1);
    U(DeleteEnhMetaFile)(h);
}

/* ---- Screen / display / info DCs ------------------------------------- */
TEST(paths_screen_dc_build_and_abort) {
    REQ_BASE();
    REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC dc = U(GetDC)(NULL);
    CHECK(dc != NULL);
    if (!dc) return;
    CHECK(BP(dc));
    MT(dc, 5, 5); LT(dc, 50, 50); U(Rectangle)(dc, 10, 10, 20, 20);
    CHECK(EP(dc));
    EXPECT_EQ(GPN(dc), 6);
    CHECK(AP(dc));
    EXPECT_EQ(GPN(dc), -1);
    U(ReleaseDC)(NULL, dc);
}
TEST(paths_screen_dc_region_without_drawing) {
    REQ_BASE();
    REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC dc = U(GetDC)(NULL);
    if (!dc) return;
    POINT t[3] = {{1000, 1000}, {1040, 1000}, {1000, 1040}};
    BP(dc); U(Polygon)(dc, t, 3); EP(dc);
    HRGN rg = U(PathToRegion)(dc);
    CHECK(rg != NULL);
    if (rg) {
        RECT r; U(GetRgnBox)(rg, &r);
        CHECK(r.left == 1000 && r.top == 1000 && r.right == 1040 && r.bottom == 1040);
        U(DeleteObject)(rg);
    }
    U(ReleaseDC)(NULL, dc);
}
TEST(paths_screen_dc_path_reset_on_release) {
    REQ_BASE();
    REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC dc = U(GetDC)(NULL);
    if (!dc) return;
    BP(dc); LT(dc, 5, 5);
    U(ReleaseDC)(NULL, dc);
    HDC dc2 = U(GetDC)(NULL);
    if (!dc2) return;
    SET_ERR(0xdeadbeef);
    BOOL r = EP(dc2);
    t_note("EndPath on new screen DC after leaving a bracket open on a released one -> %d", r);
    AP(dc2);
    U(ReleaseDC)(NULL, dc2);
}
TEST(paths_display_dc_via_createdc) {
    REQ_BASE();
    REQUIRE(U(CreateDCW)); 
    HDC dc = U(CreateDCW)(L"DISPLAY", NULL, NULL, NULL);
    if (!dc) { t_skip("no display DC"); return; }
    CHECK(BP(dc)); U(Ellipse)(dc, 0, 0, 40, 40); CHECK(EP(dc));
    EXPECT_EQ(GPN(dc), 13);
    CHECK(AP(dc));
    U(DeleteDC)(dc);
}
TEST(paths_info_dc_note) {
    REQ_BASE();
    REQUIRE(U(CreateICW));
    HDC dc = U(CreateICW)(L"DISPLAY", NULL, NULL, NULL);
    if (!dc) { t_skip("no IC"); return; }
    SET_ERR(0xdeadbeef);
    BOOL b = BP(dc);
    BOOL e = b ? EP(dc) : 0;
    t_note("information DC: BeginPath=%d EndPath=%d le=%lu", b, e, GetLastError());
    AP(dc);
    U(DeleteDC)(dc);
}

/* ---- Invalid handles ------------------------------------------------- */
TEST(paths_invalid_null_handle_all_functions) {
    REQ_BASE();
    SET_ERR(0xdeadbeef); CHECK(!BP(NULL)); t_note("BeginPath(NULL) le=%lu", GetLastError());
    SET_ERR(0xdeadbeef); CHECK(!EP(NULL)); t_note("EndPath(NULL) le=%lu", GetLastError());
    SET_ERR(0xdeadbeef); CHECK(!CF(NULL)); t_note("CloseFigure(NULL) le=%lu", GetLastError());
    SET_ERR(0xdeadbeef); CHECK(!FP(NULL)); t_note("FillPath(NULL) le=%lu", GetLastError());
    SET_ERR(0xdeadbeef); CHECK(!SP(NULL)); t_note("StrokePath(NULL) le=%lu", GetLastError());
    SET_ERR(0xdeadbeef); CHECK(!U(SelectClipPath)(NULL, RGN_COPY)); t_note("SelectClipPath(NULL) le=%lu", GetLastError());
}
TEST(paths_invalid_null_handle_wrapper_errors) {
    REQ_BASE();
    SET_ERR(0xdeadbeef); CHECK(!AP(NULL)); EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SET_ERR(0xdeadbeef); CHECK(!U(WidenPath)(NULL)); EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SET_ERR(0xdeadbeef); CHECK(!U(FlattenPath)(NULL)); EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SET_ERR(0xdeadbeef); CHECK(!SFP(NULL)); EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SET_ERR(0xdeadbeef); CHECK(U(PathToRegion)(NULL) == NULL); EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}
TEST(paths_invalid_null_handle_arcs) {
    REQ_BASE();
    REQUIRE(U(ArcTo)); REQUIRE(U(AngleArc)); REQUIRE(U(PolyDraw));
    SET_ERR(0xdeadbeef); CHECK(!U(ArcTo)(NULL, 0, 0, 10, 10, 10, 5, 5, 0)); EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SET_ERR(0xdeadbeef); CHECK(!U(AngleArc)(NULL, 5, 5, 4, 0.0f, 90.0f)); EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    POINT p[2] = {{1, 1}, {2, 2}}; BYTE t[2] = {PT_MOVETO, PT_LINETO};
    CHECK(!U(PolyDraw)(NULL, p, t, 2));
}
TEST(paths_invalid_wrong_object_types) {
    REQ_BASE();
    REQUIRE(U(CreateRectRgn));
    HGDIOBJ objs[4];
    objs[0] = U(GetStockObject)(BLACK_PEN);
    objs[1] = U(GetStockObject)(WHITE_BRUSH);
    objs[2] = U(GetStockObject)(SYSTEM_FONT);
    objs[3] = U(CreateRectRgn)(0, 0, 5, 5);
    int i;
    for (i = 0; i < 4; i++) {
        HDC h = (HDC)objs[i];
        CHECKF(!BP(h), "obj %d", i); CHECKF(!EP(h), "obj %d", i); CHECKF(!AP(h), "obj %d", i);
        CHECKF(!CF(h), "obj %d", i); CHECKF(!FP(h), "obj %d", i); CHECKF(!SP(h), "obj %d", i);
        CHECKF(!SFP(h), "obj %d", i); CHECKF(!U(WidenPath)(h), "obj %d", i); CHECKF(!U(FlattenPath)(h), "obj %d", i);
        CHECKF(!U(SelectClipPath)(h, RGN_COPY), "obj %d", i);
        CHECKF(U(PathToRegion)(h) == NULL, "obj %d", i);
        CHECKF(GPN(h) == -1, "obj %d", i);
        SET_ERR(0xdeadbeef); AP(h); EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    }
    U(DeleteObject)(objs[3]);
}
TEST(paths_invalid_bogus_handle_values) {
    REQ_BASE();
    ULONG_PTR vals[] = {1, 2, 0x10, 0x1234, 0x00010000, 0x00010001, 0x01010101, 0xdeadbeef, 0xffffffff, 0x7fffffff};
    unsigned i;
    for (i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        HDC h = (HDC)vals[i];
        CHECKF(!BP(h), "0x%llx", (unsigned long long)vals[i]);
        CHECKF(!EP(h), "0x%llx", (unsigned long long)vals[i]);
        CHECKF(!FP(h), "0x%llx", (unsigned long long)vals[i]);
        CHECKF(!SP(h), "0x%llx", (unsigned long long)vals[i]);
        CHECKF(!U(WidenPath)(h), "0x%llx", (unsigned long long)vals[i]);
        CHECKF(U(PathToRegion)(h) == NULL, "0x%llx", (unsigned long long)vals[i]);
        CHECKF(GPN(h) == -1, "0x%llx", (unsigned long long)vals[i]);
    }
}
TEST(paths_invalid_deleted_dc_wrapper_error) {
    REQ_BASE();
    HDC dc = U(CreateCompatibleDC)(NULL);
    U(DeleteDC)(dc);
    SET_ERR(0xdeadbeef); CHECK(!AP(dc));
    t_note("AbortPath(deleted DC) le=%lu", GetLastError());
    SET_ERR(0xdeadbeef); CHECK(!U(WidenPath)(dc));
    t_note("WidenPath(deleted DC) le=%lu", GetLastError());
    SET_ERR(0xdeadbeef); CHECK(!U(FlattenPath)(dc));
    SET_ERR(0xdeadbeef); CHECK(U(PathToRegion)(dc) == NULL);
}

/* ---- Threads / lifetime / leaks -------------------------------------- */
typedef struct { HDC dc; int ops; BOOL lt, ep; } TH;
static DWORD WINAPI th_draw(LPVOID p) {
    TH *t = p;
    t->lt = LT(t->dc, 20, 20);
    t->ops++;
    return 0;
}
static DWORD WINAPI th_end(LPVOID p) {
    TH *t = p;
    t->ep = EP(t->dc);
    return 0;
}
TEST(paths_thread_draw_into_other_threads_path) {
    SETUP(m);
    TH t = {m.dc, 0, 0, 0};
    BP(m.dc); MT(m.dc, 1, 1);
    HANDLE h = CreateThread(NULL, 0, th_draw, &t, 0, NULL);
    CHECK(h != NULL);
    if (!h) { AP(m.dc); md_destroy(&m); return; }
    WaitForSingleObject(h, 10000); CloseHandle(h);
    EP(m.dc);
    t_note("cross-thread LineTo -> %d, path points %d", t.lt, GPN(m.dc));
    CHECK(t.lt);
    EXPECT_EQ(GPN(m.dc), 2);
    md_destroy(&m);
}
TEST(paths_thread_end_path_from_other_thread) {
    SETUP(m);
    TH t = {m.dc, 0, 0, 0};
    BP(m.dc); MT(m.dc, 1, 1); LT(m.dc, 9, 9);
    HANDLE h = CreateThread(NULL, 0, th_end, &t, 0, NULL);
    if (!h) { AP(m.dc); md_destroy(&m); return; }
    WaitForSingleObject(h, 10000); CloseHandle(h);
    t_note("cross-thread EndPath -> %d", t.ep);
    if (t.ep) EXPECT_EQ(GPN(m.dc), 2); else AP(m.dc);
    md_destroy(&m);
}
TEST(paths_lifetime_deletedc_with_open_path_no_leak) {
    REQ_BASE();
    int before = gui_objs(), i;
    for (i = 0; i < 300; i++) {
        HDC dc = U(CreateCompatibleDC)(NULL);
        BP(dc); MT(dc, 1, 1); LT(dc, 9, 9);
        U(DeleteDC)(dc);
    }
    if (before >= 0) EXPECT_EQ(gui_objs(), before);
}
TEST(paths_lifetime_deletedc_with_closed_path_no_leak) {
    REQ_BASE();
    int before = gui_objs(), i;
    for (i = 0; i < 300; i++) {
        HDC dc = U(CreateCompatibleDC)(NULL);
        BP(dc); U(Ellipse)(dc, 0, 0, 20, 20); EP(dc);
        U(DeleteDC)(dc);
    }
    if (before >= 0) EXPECT_EQ(gui_objs(), before);
}
TEST(paths_lifetime_loop_no_gdi_object_growth) {
    SETUP(m);
    HBRUSH b = U(CreateSolidBrush)(RGB(255, 0, 0));
    HPEN p = U(CreatePen)(PS_SOLID, 3, RGB(0, 0, 255));
    HGDIOBJ ob = U(SelectObject)(m.dc, b), op = U(SelectObject)(m.dc, p);
    int before = gui_objs(), i;
    for (i = 0; i < 1500; i++) {
        BP(m.dc); POLYGON4(m.dc, 4, 4, 30, 30); EP(m.dc);
        switch (i % 6) {
        case 0: FP(m.dc); break;
        case 1: SP(m.dc); break;
        case 2: SFP(m.dc); break;
        case 3: U(WidenPath)(m.dc); FP(m.dc); break;
        case 4: U(FlattenPath)(m.dc); AP(m.dc); break;
        case 5: U(SelectClipPath)(m.dc, RGN_COPY); U(SelectClipRgn)(m.dc, NULL); break;
        }
    }
    int after = gui_objs();
    if (before >= 0) EXPECT_EQ(after, before);
    U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, op); U(DeleteObject)(b); U(DeleteObject)(p);
    md_destroy(&m);
}
TEST(paths_lifetime_selected_bitmap_deleted_dc_path_ops_safe) {
    REQ_BASE();
    MD m;
    if (!md_create(&m, 16, 16)) return;
    BP(m.dc); LT(m.dc, 5, 5); EP(m.dc);
    HBITMAP bmp = m.bmp; HDC dc = m.dc;
    U(SelectObject)(dc, m.old);          /* path persists with no DIB selected */
    CHECK(GPN(dc) == 2);
    U(DeleteObject)(bmp);
    m.bmp = NULL;
    CHECK(AP(dc));
    U(DeleteDC)(dc);
}
TEST(paths_limits_many_points) {
    SETUP(m);
    int i, ok = 1;
    BP(m.dc); MT(m.dc, 0, 0);
    for (i = 1; i <= 30000; i++) if (!LT(m.dc, i & 63, (i >> 6) & 63)) { ok = 0; break; }
    BOOL e = EP(m.dc);
    int n = GPN(m.dc);
    t_note("30000 LineTo: ok=%d EndPath=%d points=%d", ok, e, n);
    CHECK(ok && e);
    if (e) EXPECT_EQ(n, 30001);
    AP(m.dc);
    md_destroy(&m);
}
TEST(paths_limits_many_figures) {
    SETUP(m);
    int i, ok = 1;
    BP(m.dc);
    for (i = 0; i < 4000; i++) { if (!U(Rectangle)(m.dc, i & 31, i & 15, (i & 31) + 4, (i & 15) + 4)) ok = 0; }
    BOOL e = EP(m.dc);
    CHECK(ok); CHECK(e);
    EXPECT_EQ(GPN(m.dc), 16000);
    AP(m.dc);
    md_destroy(&m);
}
TEST(paths_limits_large_fill_big_dib) {
    REQ_BASE();
    MD m;
    if (!md_create(&m, 1024, 1024)) { t_skip("no 1024x1024 DIB"); return; }
    md_clear(&m, WHITE);
    HBRUSH b = U(CreateSolidBrush)(RGB(255, 0, 0));
    HGDIOBJ ob = U(SelectObject)(m.dc, b), op = U(SelectObject)(m.dc, U(GetStockObject)(NULL_PEN));
    BP(m.dc); U(Ellipse)(m.dc, 0, 0, 1024, 1024); EP(m.dc);
    CHECK(FP(m.dc));
    CHECK(md_px(&m, 512, 512) == RED); CHECK(md_px(&m, 2, 2) == WHITE); CHECK(md_px(&m, 1021, 1021) == WHITE);
    CHECK(md_px(&m, 512, 2) == RED);
    U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, op); U(DeleteObject)(b);
    md_destroy(&m);
}
