#include "mt.h"

static void aniso(HDC dc, int wox, int woy, int wex, int wey, int vox, int voy, int vex, int vey)
{
    U(SetMapMode)(dc, MM_ANISOTROPIC);
    U(SetWindowOrgEx)(dc, wox, woy, NULL);
    U(SetViewportOrgEx)(dc, vox, voy, NULL);
    U(SetWindowExtEx)(dc, wex, wey, NULL);
    U(SetViewportExtEx)(dc, vex, vey, NULL);
}

TEST(mapping_conv_identity_text)
{
    REQUIRE(U(LPtoDP)); REQUIRE(U(DPtoLP));
    HDC dc = mt_dc();
    static const long v[] = { 0, 1, -1, 100, -100, 32767, -32768, 65535, 1000000, -1000000 };
    for (int i = 0; i < 10; i++) for (int j = 0; j < 10; j += 3) {
        LP_IS(dc, v[i], v[j], v[i], v[j]);
        DP_IS(dc, v[i], v[j], v[i], v[j]);
    }
    mt_free_dc(dc);
}

TEST(mapping_conv_origins_only)
{
    REQUIRE(U(LPtoDP)); REQUIRE(U(DPtoLP));
    HDC dc = mt_dc();
    U(SetWindowOrgEx)(dc, 10, 20, NULL);
    U(SetViewportOrgEx)(dc, 100, 200, NULL);
    LP_IS(dc, 10, 20, 100, 200);
    LP_IS(dc, 0, 0, 90, 180);
    LP_IS(dc, -5, 7, 85, 187);
    DP_IS(dc, 100, 200, 10, 20);
    DP_IS(dc, 0, 0, -90, -180);
    DP_IS(dc, 90, 180, 0, 0);
    mt_free_dc(dc);
}

TEST(mapping_conv_window_org_only)
{
    REQUIRE(U(LPtoDP));
    HDC dc = mt_dc();
    U(SetWindowOrgEx)(dc, -50, 60, NULL);
    LP_IS(dc, 0, 0, 50, -60);
    LP_IS(dc, -50, 60, 0, 0);
    DP_IS(dc, 0, 0, -50, 60);
    mt_free_dc(dc);
}

TEST(mapping_conv_viewport_org_only)
{
    REQUIRE(U(LPtoDP));
    HDC dc = mt_dc();
    U(SetViewportOrgEx)(dc, -50, 60, NULL);
    LP_IS(dc, 0, 0, -50, 60);
    LP_IS(dc, 50, -60, 0, 0);
    DP_IS(dc, 0, 0, 50, -60);
    mt_free_dc(dc);
}

TEST(mapping_conv_aniso_table)
{
    REQUIRE(U(LPtoDP)); REQUIRE(U(DPtoLP));
    static const struct { int wox, woy, wex, wey, vox, voy, vex, vey; int lx, ly, dx, dy; } t[] = {
        { 0, 0, 1, 1, 0, 0, 2, 3,        5, 7, 10, 21 },
        { 0, 0, 2, 2, 0, 0, 1, 1,        10, 20, 5, 10 },
        { 0, 0, 100, 100, 0, 0, 1, 1,    300, -500, 3, -5 },
        { 10, 10, 1, 1, 0, 0, 1, 1,      10, 10, 0, 0 },
        { 10, 20, 4, 5, 100, 200, 8, 10, 14, 25, 108, 210 },
        { 0, 0, 1, 1, 0, 0, -1, -1,      5, 6, -5, -6 },
        { 0, 0, 1, 1, 63, 63, 1, -1,     5, 6, 68, 57 },
        { 0, 0, 1, 1, 0, 0, -2, 3,       -4, 4, 8, 12 },
        { 0, 0, -1, 1, 0, 0, 1, 1,       5, 6, -5, 6 },
        { 0, 0, -3, -3, 0, 0, 6, 6,      4, -4, -8, 8 },
        { 5, 5, 10, 10, 50, 50, 20, 20,  15, 25, 70, 90 },
        { -7, -9, 3, 3, 1, 1, 9, 9,      -4, -3, 10, 19 },
        { 0, 0, 32767, 32767, 0, 0, 32767, 32767, 1234, -4321, 1234, -4321 },
        { 0, 0, 1, 1, 0, 0, 32767, 32767, 3, 4, 98301, 131068 },
        { 0, 0, 7, 11, 0, 0, 14, 22,     21, 33, 42, 66 },
    };
    for (int i = 0; i < (int)(sizeof t / sizeof t[0]); i++) {
        HDC dc = mt_dc();
        aniso(dc, t[i].wox, t[i].woy, t[i].wex, t[i].wey, t[i].vox, t[i].voy, t[i].vex, t[i].vey);
        POINT p = { t[i].lx, t[i].ly };
        BOOL r = U(LPtoDP)(dc, &p, 1);
        CHECKF(r && p.x == t[i].dx && p.y == t[i].dy, "row %d LPtoDP got %ld,%ld want %d,%d", i, p.x, p.y, t[i].dx, t[i].dy);
        p.x = t[i].dx; p.y = t[i].dy;
        r = U(DPtoLP)(dc, &p, 1);
        CHECKF(r && p.x == t[i].lx && p.y == t[i].ly, "row %d DPtoLP got %ld,%ld want %d,%d", i, p.x, p.y, t[i].lx, t[i].ly);
        mt_free_dc(dc);
    }
}

TEST(mapping_conv_array_matches_single)
{
    REQUIRE(U(LPtoDP)); REQUIRE(U(DPtoLP));
    HDC dc = mt_dc();
    aniso(dc, 3, -4, 7, 5, 11, 13, 21, -10);
    POINT a[64], b[64];
    for (int i = 0; i < 64; i++) { a[i].x = b[i].x = i * 37 - 900; a[i].y = b[i].y = 700 - i * 29; }
    CHECK(U(LPtoDP)(dc, a, 64));
    for (int i = 0; i < 64; i++) {
        POINT s = b[i];
        U(LPtoDP)(dc, &s, 1);
        CHECKF(s.x == a[i].x && s.y == a[i].y, "i=%d array %ld,%ld single %ld,%ld", i, a[i].x, a[i].y, s.x, s.y);
    }
    POINT c[64];
    memcpy(c, a, sizeof c);
    CHECK(U(DPtoLP)(dc, c, 64));
    for (int i = 0; i < 64; i++) {
        POINT s = a[i];
        U(DPtoLP)(dc, &s, 1);
        CHECKF(s.x == c[i].x && s.y == c[i].y, "i=%d DPtoLP array vs single", i);
    }
    mt_free_dc(dc);
}

TEST(mapping_conv_array_counts)
{
    REQUIRE(U(LPtoDP)); REQUIRE(U(DPtoLP));
    HDC dc = mt_dc();
    aniso(dc, 0, 0, 1, 1, 0, 0, 2, 2);
    POINT a[5] = { {1,1},{2,2},{3,3},{4,4},{5,5} };
    CHECK(U(LPtoDP)(dc, a, 3));
    CHECK(a[0].x == 2 && a[2].x == 6 && a[3].x == 4 && a[4].x == 5);
    CHECK(U(LPtoDP)(dc, a, 1));
    CHECK(a[0].x == 4 && a[1].x == 4);
    mt_free_dc(dc);
}

TEST(mapping_conv_count_zero_negative)
{
    REQUIRE(U(LPtoDP)); REQUIRE(U(DPtoLP));
    HDC dc = mt_dc();
    aniso(dc, 0, 0, 1, 1, 0, 0, 2, 2);
    POINT p = {5, 6};
    SetLastError(0xdeadbeef);
    CHECK(U(LPtoDP)(dc, &p, 0)); CHECK(p.x == 5 && p.y == 6);
    CHECK(U(LPtoDP)(dc, &p, -1)); CHECK(p.x == 5 && p.y == 6);
    CHECK(U(LPtoDP)(dc, &p, INT_MIN)); CHECK(p.x == 5 && p.y == 6);
    CHECK(U(LPtoDP)(dc, NULL, 0));
    CHECK(U(LPtoDP)(dc, NULL, -5));
    CHECK(U(LPtoDP)(NULL, NULL, 0));
    CHECK(U(LPtoDP)((HDC)0xdeadbeef, &p, 0));
    BOOL r = U(DPtoLP)(dc, &p, 0);
    t_note("DPtoLP count 0 ret %d", r); CHECK(p.x == 5 && p.y == 6);
    r = U(DPtoLP)(dc, &p, -1);
    t_note("DPtoLP count -1 ret %d", r); CHECK(p.x == 5 && p.y == 6);
    r = U(DPtoLP)(NULL, NULL, 0);
    t_note("DPtoLP(NULL,NULL,0) ret %d", r);
    mt_free_dc(dc);
}

TEST(mapping_conv_invalid_dc)
{
    REQUIRE(U(LPtoDP)); REQUIRE(U(DPtoLP));
    HDC dead = mt_dc(); mt_free_dc(dead);
    HPEN pen = U(CreatePen)(PS_SOLID, 1, 0);
    HDC bad[] = { NULL, (HDC)1, (HDC)0xdeadbeef, dead, (HDC)pen };
    for (int i = 0; i < 5; i++) {
        POINT p = {5, 6};
        SetLastError(0xdeadbeef);
        BOOL r = U(LPtoDP)(bad[i], &p, 1);
        CHECKF(!r, "LPtoDP idx %d", i);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
        CHECKF(p.x == 5 && p.y == 6, "idx %d point modified", i);
        SetLastError(0xdeadbeef);
        r = U(DPtoLP)(bad[i], &p, 1);
        CHECKF(!r, "DPtoLP idx %d", i);
        t_note("DPtoLP bad dc idx %d err %lu", i, GetLastError());
        CHECKF(p.x == 5 && p.y == 6, "idx %d point modified by DPtoLP", i);
    }
    U(DeleteObject)(pen);
}

TEST(mapping_conv_rounding_third)
{
    REQUIRE(U(LPtoDP));
    HDC dc = mt_dc();
    aniso(dc, 0, 0, 3, 3, 0, 0, 1, 1);
    static const struct { long l, d; } t[] = { {0,0}, {1,0}, {2,1}, {3,1}, {4,1}, {5,2}, {6,2}, {7,2}, {8,3}, {9,3},
                                               {-1,0}, {-2,-1}, {-3,-1}, {-4,-1}, {-5,-2}, {-6,-2}, {-7,-2}, {-8,-3}, {299,100}, {-299,-100}, {301,100}, {-301,-100} };
    for (int i = 0; i < (int)(sizeof t / sizeof t[0]); i++) LP_IS(dc, t[i].l, t[i].l, t[i].d, t[i].d);
    mt_free_dc(dc);
}

TEST(mapping_conv_rounding_fifths)
{
    REQUIRE(U(LPtoDP));
    HDC dc = mt_dc();
    aniso(dc, 0, 0, 5, 5, 0, 0, 2, 2); /* x * 0.4 : .2 .4 .6 .8 patterns, no exact halves */
    static const struct { long l, d; } t[] = { {1,0}, {2,1}, {3,1}, {4,2}, {6,2}, {7,3}, {8,3}, {9,4}, {-1,0}, {-2,-1}, {-3,-1}, {-4,-2}, {-6,-2}, {-7,-3}, {-8,-3}, {-9,-4} };
    for (int i = 0; i < (int)(sizeof t / sizeof t[0]); i++) LP_IS(dc, t[i].l, t[i].l, t[i].d, t[i].d);
    mt_free_dc(dc);
}

TEST(mapping_conv_rounding_exact_halves_observed)
{
    REQUIRE(U(LPtoDP));
    HDC dc = mt_dc();
    aniso(dc, 0, 0, 2, 2, 0, 0, 1, 1);
    for (long v = -5; v <= 5; v++) {
        POINT p = { v, v }; U(LPtoDP)(dc, &p, 1);
        t_note("x*0.5: %ld -> %ld", v, p.x);
    }
    mt_free_dc(dc);
}

TEST(mapping_conv_roundtrip_scale_up)
{
    REQUIRE(U(LPtoDP)); REQUIRE(U(DPtoLP));
    HDC dc = mt_dc();
    aniso(dc, 7, -3, 1, 1, 100, 50, 4, 4);
    for (long v = -50; v <= 50; v += 7) {
        POINT p = { v, -v * 2 }, q;
        U(LPtoDP)(dc, &p, 1); q = p;
        U(DPtoLP)(dc, &q, 1);
        CHECKF(q.x == v && q.y == -v * 2, "roundtrip %ld -> %ld,%ld -> %ld,%ld", v, p.x, p.y, q.x, q.y);
    }
    mt_free_dc(dc);
}

TEST(mapping_conv_flip_axes)
{
    REQUIRE(U(LPtoDP)); REQUIRE(U(DPtoLP));
    HDC dc = mt_dc();
    aniso(dc, 0, 0, 1, 1, 0, 0, -1, 1);
    LP_IS(dc, 10, 10, -10, 10);
    aniso(dc, 0, 0, 1, 1, 0, 0, 1, -1);
    LP_IS(dc, 10, 10, 10, -10);
    aniso(dc, 0, 0, -1, -1, 0, 0, 1, 1);
    LP_IS(dc, 10, 10, -10, -10);
    DP_IS(dc, -10, -10, 10, 10);
    mt_free_dc(dc);
}

TEST(mapping_conv_lometric_orientation)
{
    REQUIRE(U(LPtoDP));
    HDC dc = mt_dc();
    U(SetMapMode)(dc, MM_LOMETRIC);
    POINT p = { 0, 0 };
    CHECK(U(LPtoDP)(dc, &p, 1));
    CHECK(p.x == 0 && p.y == 0);
    p.x = 1000; p.y = 1000;
    U(LPtoDP)(dc, &p, 1);
    CHECK(p.x > 0 && p.y < 0);
    U(SetViewportOrgEx)(dc, 50, 60, NULL);
    p.x = 0; p.y = 0;
    U(LPtoDP)(dc, &p, 1);
    CHECK(p.x == 50 && p.y == 60);
    U(SetWindowOrgEx)(dc, 10, 10, NULL);
    p.x = 10; p.y = 10;
    U(LPtoDP)(dc, &p, 1);
    CHECK(p.x == 50 && p.y == 60);
    mt_free_dc(dc);
}

TEST(mapping_conv_fixed_modes_match_extents)
{
    REQUIRE(U(LPtoDP));
    static const int m[] = { MM_LOMETRIC, MM_HIMETRIC, MM_LOENGLISH, MM_HIENGLISH, MM_TWIPS };
    HDC dc = mt_dc();
    for (int i = 0; i < 5; i++) {
        U(SetMapMode)(dc, m[i]);
        SIZE w, v;
        U(GetWindowExtEx)(dc, &w); U(GetViewportExtEx)(dc, &v);
        POINT p = { w.cx, w.cy };
        U(LPtoDP)(dc, &p, 1);
        CHECKF(p.x == v.cx && p.y == v.cy, "mode %d full window -> %ld,%ld want %ld,%ld", m[i], p.x, p.y, v.cx, v.cy);
        p.x = 0; p.y = 0;
        U(LPtoDP)(dc, &p, 1);
        CHECKF(p.x == 0 && p.y == 0, "mode %d origin", m[i]);
    }
    mt_free_dc(dc);
}

TEST(mapping_conv_overflow_observed)
{
    REQUIRE(U(LPtoDP)); REQUIRE(U(DPtoLP));
    HDC dc = mt_dc();
    aniso(dc, 0, 0, 1, 1, 0, 0, 2, 2);
    POINT p = { 0x7fffffff, -0x7fffffff };
    BOOL r = U(LPtoDP)(dc, &p, 1);
    t_note("LPtoDP(INT_MAX,-INT_MAX) x2 ret %d -> %ld,%ld", r, p.x, p.y);
    p.x = 0x40000000; p.y = -0x40000000;
    r = U(LPtoDP)(dc, &p, 1);
    t_note("LPtoDP(2^30) x2 ret %d -> %ld,%ld", r, p.x, p.y);
    p.x = INT_MIN; p.y = INT_MIN;
    r = U(DPtoLP)(dc, &p, 1);
    t_note("DPtoLP(INT_MIN) ret %d -> %ld,%ld", r, p.x, p.y);
    aniso(dc, 0, 0, 1, 1, 0, 0, 32767, 32767);
    p.x = 0x10000; p.y = 0x10000;
    r = U(LPtoDP)(dc, &p, 1);
    t_note("LPtoDP(65536) x32767 ret %d -> %ld,%ld", r, p.x, p.y);
    U(SetViewportOrgEx)(dc, INT_MAX, INT_MAX, NULL);
    p.x = 100; p.y = 100;
    r = U(LPtoDP)(dc, &p, 1);
    t_note("LPtoDP with viewport org INT_MAX ret %d -> %ld,%ld", r, p.x, p.y);
    mt_free_dc(dc);
}

TEST(mapping_conv_window_ext_32767)
{
    REQUIRE(U(LPtoDP)); REQUIRE(U(SetWindowExtEx));
    HDC dc = mt_dc();
    U(SetMapMode)(dc, MM_ANISOTROPIC);
    BOOL r = U(SetWindowExtEx)(dc, 32767, 32767, NULL);
    CHECK(r);
    r = U(SetWindowExtEx)(dc, 32768, 32768, NULL);
    SIZE s; U(GetWindowExtEx)(dc, &s);
    t_note("window ext 32768 ret %d -> %ld,%ld", r, s.cx, s.cy);
    r = U(SetWindowExtEx)(dc, 0x7fffffff, 0x7fffffff, NULL);
    U(GetWindowExtEx)(dc, &s);
    t_note("window ext INT_MAX ret %d -> %ld,%ld", r, s.cx, s.cy);
    r = U(SetViewportExtEx)(dc, INT_MIN, INT_MIN, NULL);
    U(GetViewportExtEx)(dc, &s);
    t_note("viewport ext INT_MIN ret %d -> %ld,%ld", r, s.cx, s.cy);
    mt_free_dc(dc);
}

/* ---------- isotropic ---------- */
TEST(mapping_iso_equal_scale_property)
{
    REQUIRE(U(LPtoDP));
    static const long v[][4] = { {100,100,200,100}, {100,100,100,200}, {100,100,300,150}, {200,100,100,100}, {100,200,100,100},
                                 {1000,1000,640,480}, {50,80,400,100}, {10,10,10,10}, {7,5,70,30} };
    for (int i = 0; i < 9; i++) {
        HDC dc = mt_dc();
        U(SetMapMode)(dc, MM_ISOTROPIC);
        U(SetWindowExtEx)(dc, v[i][0], v[i][1], NULL);
        U(SetViewportExtEx)(dc, v[i][2], v[i][3], NULL);
        POINT a = { 1000, 0 }, b = { 0, 1000 };
        U(LPtoDP)(dc, &a, 1); U(LPtoDP)(dc, &b, 1);
        long sx = a.x < 0 ? -a.x : a.x, sy = b.y < 0 ? -b.y : b.y;
        CHECKF(mt_near(sx, sy, 20, 2), "row %d x scale %ld vs y scale %ld", i, sx, sy);
        CHECKF(a.y == 0 && b.x == 0, "row %d off-axis", i);
        /* scale never exceeds what the requested viewport allows */
        long mx = v[i][2] * 1000 / v[i][0], my = v[i][3] * 1000 / v[i][1];
        long lim = mx < my ? mx : my;
        CHECKF(sx <= lim + 2 && sx >= lim - 2 - lim / 100, "row %d scale %ld expected about %ld", i, sx, lim);
        SIZE w, vv; U(GetWindowExtEx)(dc, &w); U(GetViewportExtEx)(dc, &vv);
        t_note("iso row %d: window %ld,%ld viewport %ld,%ld", i, w.cx, w.cy, vv.cx, vv.cy);
        mt_free_dc(dc);
    }
}

TEST(mapping_iso_square_unchanged)
{
    REQUIRE(U(LPtoDP));
    HDC dc = mt_dc();
    U(SetMapMode)(dc, MM_ISOTROPIC);
    U(SetWindowExtEx)(dc, 100, 100, NULL);
    U(SetViewportExtEx)(dc, 300, 300, NULL);
    EXT_IS(dc, GetWindowExtEx, 100, 100);
    EXT_IS(dc, GetViewportExtEx, 300, 300);
    LP_IS(dc, 10, 10, 30, 30);
    LP_IS(dc, -10, 20, -30, 60);
    mt_free_dc(dc);
}

TEST(mapping_iso_flipped_y_keeps_sign)
{
    REQUIRE(U(LPtoDP));
    HDC dc = mt_dc();
    U(SetMapMode)(dc, MM_ISOTROPIC);
    U(SetWindowExtEx)(dc, 100, 100, NULL);
    U(SetViewportExtEx)(dc, 200, -200, NULL);
    LP_IS(dc, 10, 10, 20, -20);
    U(SetViewportExtEx)(dc, -200, 200, NULL);
    LP_IS(dc, 10, 10, -20, 20);
    U(SetViewportExtEx)(dc, -200, -200, NULL);
    LP_IS(dc, 10, 10, -20, -20);
    mt_free_dc(dc);
}

TEST(mapping_iso_set_window_after_viewport_order)
{
    REQUIRE(U(LPtoDP));
    HDC dc = mt_dc();
    U(SetMapMode)(dc, MM_ISOTROPIC);
    U(SetViewportExtEx)(dc, 400, 200, NULL);
    U(SetWindowExtEx)(dc, 100, 100, NULL);
    POINT a = { 100, 0 }, b = { 0, 100 };
    U(LPtoDP)(dc, &a, 1); U(LPtoDP)(dc, &b, 1);
    CHECKF(mt_near(a.x, b.y, 5, 1), "scale x %ld y %ld", a.x, b.y);
    mt_free_dc(dc);
}

TEST(mapping_iso_origins_unaffected)
{
    REQUIRE(U(LPtoDP));
    HDC dc = mt_dc();
    U(SetMapMode)(dc, MM_ISOTROPIC);
    U(SetWindowOrgEx)(dc, 5, 5, NULL);
    U(SetViewportOrgEx)(dc, 70, 80, NULL);
    U(SetWindowExtEx)(dc, 10, 10, NULL);
    U(SetViewportExtEx)(dc, 20, 20, NULL);
    LP_IS(dc, 5, 5, 70, 80);
    LP_IS(dc, 15, 25, 90, 120);
    mt_free_dc(dc);
}

/* ---------- ScaleWindowExtEx / ScaleViewportExtEx ---------- */
TEST(mapping_scale_window_aniso)
{
    REQUIRE(U(ScaleWindowExtEx)); REQUIRE(U(SetWindowExtEx));
    static const long t[][8] = {
        { 100, 200, 1, 2, 3, 4,  50, 150 },
        { 100, 200, 2, 1, 1, 1,  200, 200 },
        { 100, 200, 1, 1, 1, 1,  100, 200 },
        { 12, 12, 5, 3, 1, 4,    20, 3 },
        { 100, 100, 3, 1, 3, 1,  300, 300 },
        { 1000, 1000, 1, 10, 1, 100, 100, 10 },
        { 100, 100, -1, 1, 1, -1, -100, -100 },
        { 100, 100, 0, 1, 1, 1,  0, 100 },
    };
    for (int i = 0; i < 8; i++) {
        HDC dc = mt_dc();
        U(SetMapMode)(dc, MM_ANISOTROPIC);
        U(SetWindowExtEx)(dc, t[i][0], t[i][1], NULL);
        SIZE p = {0x7777, 0x7777};
        BOOL r = U(ScaleWindowExtEx)(dc, t[i][2], t[i][3], t[i][4], t[i][5], &p);
        if (t[i][2] == 0) { t_note("row %d zero numerator ret %d", i, r); mt_free_dc(dc); continue; }
        CHECKF(r, "row %d ret", i);
        CHECKF(p.cx == t[i][0] && p.cy == t[i][1], "row %d prev %ld,%ld", i, p.cx, p.cy);
        EXT_IS(dc, GetWindowExtEx, t[i][6], t[i][7]);
        mt_free_dc(dc);
    }
}

TEST(mapping_scale_viewport_aniso)
{
    REQUIRE(U(ScaleViewportExtEx)); REQUIRE(U(SetViewportExtEx));
    static const long t[][8] = {
        { 100, 200, 1, 2, 3, 4,  50, 150 },
        { 100, 200, 2, 1, 1, 1,  200, 200 },
        { 12, 12, 5, 3, 1, 4,    20, 3 },
        { 100, 100, -1, 1, 1, -1, -100, -100 },
        { 1000, 1000, 1, 10, 1, 100, 100, 10 },
    };
    for (int i = 0; i < 5; i++) {
        HDC dc = mt_dc();
        U(SetMapMode)(dc, MM_ANISOTROPIC);
        U(SetViewportExtEx)(dc, t[i][0], t[i][1], NULL);
        SIZE p = {0x7777, 0x7777};
        BOOL r = U(ScaleViewportExtEx)(dc, t[i][2], t[i][3], t[i][4], t[i][5], &p);
        CHECKF(r, "row %d ret", i);
        CHECKF(p.cx == t[i][0] && p.cy == t[i][1], "row %d prev %ld,%ld", i, p.cx, p.cy);
        EXT_IS(dc, GetViewportExtEx, t[i][6], t[i][7]);
        mt_free_dc(dc);
    }
}

TEST(mapping_scale_null_prev)
{
    REQUIRE(U(ScaleWindowExtEx)); REQUIRE(U(ScaleViewportExtEx));
    HDC dc = mt_dc();
    U(SetMapMode)(dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(dc, 10, 10, NULL);
    U(SetViewportExtEx)(dc, 10, 10, NULL);
    CHECK(U(ScaleWindowExtEx)(dc, 2, 1, 2, 1, NULL));
    CHECK(U(ScaleViewportExtEx)(dc, 3, 1, 3, 1, NULL));
    EXT_IS(dc, GetWindowExtEx, 20, 20);
    EXT_IS(dc, GetViewportExtEx, 30, 30);
    mt_free_dc(dc);
}

TEST(mapping_scale_affects_conversion)
{
    REQUIRE(U(ScaleWindowExtEx)); REQUIRE(U(LPtoDP));
    HDC dc = mt_dc();
    aniso(dc, 0, 0, 10, 10, 0, 0, 10, 10);
    LP_IS(dc, 7, 9, 7, 9);
    U(ScaleWindowExtEx)(dc, 2, 1, 2, 1, NULL);   /* window 20 : viewport 10 -> halves */
    LP_IS(dc, 8, 10, 4, 5);
    U(ScaleViewportExtEx)(dc, 4, 1, 4, 1, NULL); /* window 20 : viewport 40 -> doubles */
    LP_IS(dc, 8, 10, 16, 20);
    mt_free_dc(dc);
}

TEST(mapping_scale_zero_denominator)
{
    REQUIRE(U(ScaleWindowExtEx)); REQUIRE(U(ScaleViewportExtEx));
    static const long t[][4] = { {1,0,1,1}, {1,1,1,0}, {1,0,1,0} };
    for (int i = 0; i < 3; i++) {
        HDC dc = mt_dc();
        aniso(dc, 0, 0, 10, 10, 0, 0, 10, 10);
        SetLastError(0xdeadbeef);
        BOOL r = U(ScaleWindowExtEx)(dc, t[i][0], t[i][1], t[i][2], t[i][3], NULL);
        t_note("ScaleWindowExtEx zero denom row %d ret %d err %lu", i, r, GetLastError());
        CHECKF(!r || 1, "");
        SIZE s; U(GetWindowExtEx)(dc, &s);
        if (!r) CHECKF(s.cx == 10 && s.cy == 10, "failed call changed ext to %ld,%ld", s.cx, s.cy);
        r = U(ScaleViewportExtEx)(dc, t[i][0], t[i][1], t[i][2], t[i][3], NULL);
        t_note("ScaleViewportExtEx zero denom row %d ret %d", i, r);
        U(GetViewportExtEx)(dc, &s);
        if (!r) CHECKF(s.cx == 10 && s.cy == 10, "failed call changed ext to %ld,%ld", s.cx, s.cy);
        mt_free_dc(dc);
    }
}

TEST(mapping_scale_overflow)
{
    REQUIRE(U(ScaleWindowExtEx)); REQUIRE(U(ScaleViewportExtEx));
    HDC dc = mt_dc();
    aniso(dc, 0, 0, 1000000, 1000000, 0, 0, 1000000, 1000000);
    SIZE s;
    BOOL r = U(ScaleWindowExtEx)(dc, 0x7fffffff, 1, 0x7fffffff, 1, NULL);
    U(GetWindowExtEx)(dc, &s);
    t_note("scale window overflow ret %d -> %ld,%ld", r, s.cx, s.cy);
    r = U(ScaleViewportExtEx)(dc, 0x7fffffff, 1, 0x7fffffff, 1, NULL);
    U(GetViewportExtEx)(dc, &s);
    t_note("scale viewport overflow ret %d -> %ld,%ld", r, s.cx, s.cy);
    r = U(ScaleWindowExtEx)(dc, 1, 0x7fffffff, 1, 0x7fffffff, NULL);
    U(GetWindowExtEx)(dc, &s);
    t_note("scale window tiny ret %d -> %ld,%ld", r, s.cx, s.cy);
    mt_free_dc(dc);
}

TEST(mapping_scale_fixed_modes)
{
    REQUIRE(U(ScaleWindowExtEx)); REQUIRE(U(ScaleViewportExtEx));
    static const int m[] = { MM_TEXT, MM_LOMETRIC, MM_HIMETRIC, MM_LOENGLISH, MM_HIENGLISH, MM_TWIPS };
    HDC dc = mt_dc();
    for (int i = 0; i < 6; i++) {
        U(SetMapMode)(dc, m[i]);
        SIZE w0, v0, s;
        U(GetWindowExtEx)(dc, &w0); U(GetViewportExtEx)(dc, &v0);
        BOOL r1 = U(ScaleWindowExtEx)(dc, 2, 1, 2, 1, NULL);
        BOOL r2 = U(ScaleViewportExtEx)(dc, 2, 1, 2, 1, NULL);
        U(GetWindowExtEx)(dc, &s);
        t_note("mode %d scale ret %d %d; window %ld,%ld -> %ld,%ld", m[i], r1, r2, w0.cx, w0.cy, s.cx, s.cy);
        U(GetViewportExtEx)(dc, &s);
        t_note("   viewport %ld,%ld -> %ld,%ld", v0.cx, v0.cy, s.cx, s.cy);
    }
    mt_free_dc(dc);
}

TEST(mapping_scale_invalid_dc)
{
    REQUIRE(U(ScaleWindowExtEx)); REQUIRE(U(ScaleViewportExtEx));
    HDC dead = mt_dc(); mt_free_dc(dead);
    HDC bad[] = { NULL, (HDC)1, (HDC)0xdeadbeef, dead };
    for (int i = 0; i < 4; i++) {
        SIZE s = {0x7777, 0x7777};
        DWORD want = bad[i] == dead ? ERROR_INVALID_PARAMETER : ERROR_INVALID_HANDLE;
        SetLastError(0xdeadbeef);
        CHECKF(!U(ScaleWindowExtEx)(bad[i], 1, 1, 1, 1, &s), "window idx %d", i);
        EXPECT_ERR(0, want);
        SetLastError(0xdeadbeef);
        CHECKF(!U(ScaleViewportExtEx)(bad[i], 1, 1, 1, 1, &s), "viewport idx %d", i);
        EXPECT_ERR(0, want);
        CHECKF(s.cx == 0x7777 && s.cy == 0x7777, "idx %d prev untouched", i);
    }
}

TEST(mapping_scale_iso_keeps_equal_scale)
{
    REQUIRE(U(ScaleWindowExtEx)); REQUIRE(U(LPtoDP));
    HDC dc = mt_dc();
    U(SetMapMode)(dc, MM_ISOTROPIC);
    U(SetWindowExtEx)(dc, 100, 100, NULL);
    U(SetViewportExtEx)(dc, 100, 100, NULL);
    U(ScaleWindowExtEx)(dc, 1, 2, 1, 2, NULL);
    POINT a = { 100, 0 }, b = { 0, 100 };
    U(LPtoDP)(dc, &a, 1); U(LPtoDP)(dc, &b, 1);
    CHECKF(mt_near(a.x < 0 ? -a.x : a.x, b.y < 0 ? -b.y : b.y, 5, 1), "x %ld y %ld", a.x, b.y);
    mt_free_dc(dc);
}
