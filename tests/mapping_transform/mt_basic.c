#include "mt.h"

static const int fixed_modes[] = { MM_LOMETRIC, MM_HIMETRIC, MM_LOENGLISH, MM_HIENGLISH, MM_TWIPS };
#define NFIXED 5

TEST(mapping_mapmode_default)
{
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(GetMapMode)); REQUIRE(U(GetWindowExtEx));
    REQUIRE(U(GetViewportExtEx)); REQUIRE(U(GetWindowOrgEx)); REQUIRE(U(GetViewportOrgEx));
    HDC dc = mt_dc();
    CHECK(dc != NULL);
    EXPECT_EQ(U(GetMapMode)(dc), MM_TEXT);
    EXT_IS(dc, GetWindowExtEx, 1, 1);
    EXT_IS(dc, GetViewportExtEx, 1, 1);
    ORG_IS(dc, GetWindowOrgEx, 0, 0);
    ORG_IS(dc, GetViewportOrgEx, 0, 0);
    mt_free_dc(dc);
}

TEST(mapping_mapmode_default_on_dib)
{
    REQUIRE(U(GetMapMode)); REQUIRE(U(CreateDIBSection)); REQUIRE(U(GdiFlush));
    mt_dib d;
    if (!mt_dib_make(&d, 16, 16)) { t_skip("no dib"); return; }
    EXPECT_EQ(U(GetMapMode)(d.dc), MM_TEXT);
    EXT_IS(d.dc, GetWindowExtEx, 1, 1);
    EXT_IS(d.dc, GetViewportExtEx, 1, 1);
    mt_dib_free(&d);
}

TEST(mapping_mapmode_each_mode_roundtrip)
{
    REQUIRE(U(SetMapMode)); REQUIRE(U(GetMapMode));
    HDC dc = mt_dc();
    int m;
    for (m = MM_TEXT; m <= MM_ANISOTROPIC; m++) {
        int prev = U(GetMapMode)(dc);
        SetLastError(0xdeadbeef);
        int r = U(SetMapMode)(dc, m);
        CHECKF(r == prev, "mode %d: prev got %d want %d", m, r, prev);
        CHECKF(U(GetMapMode)(dc) == m, "mode %d get=%d", m, U(GetMapMode)(dc));
    }
    mt_free_dc(dc);
}

TEST(mapping_mapmode_prev_chain)
{
    REQUIRE(U(SetMapMode));
    HDC dc = mt_dc();
    static const int seq[] = { MM_LOMETRIC, MM_TWIPS, MM_ANISOTROPIC, MM_ISOTROPIC, MM_HIMETRIC, MM_TEXT, MM_LOENGLISH };
    int prev = MM_TEXT, i;
    for (i = 0; i < 7; i++) {
        MT_EQ(U(SetMapMode)(dc, seq[i]), prev);
        prev = seq[i];
    }
    mt_free_dc(dc);
}

TEST(mapping_mapmode_same_mode_returns_current)
{
    REQUIRE(U(SetMapMode));
    HDC dc = mt_dc();
    MT_EQ(U(SetMapMode)(dc, MM_TEXT), MM_TEXT);
    U(SetMapMode)(dc, MM_HIENGLISH);
    MT_EQ(U(SetMapMode)(dc, MM_HIENGLISH), MM_HIENGLISH);
    MT_EQ(U(SetMapMode)(dc, MM_HIENGLISH), MM_HIENGLISH);
    EXPECT_EQ(U(GetMapMode)(dc), MM_HIENGLISH);
    mt_free_dc(dc);
}

TEST(mapping_mapmode_invalid_values)
{
    REQUIRE(U(SetMapMode)); REQUIRE(U(GetMapMode));
    static const int bad[] = { 0, 9, 10, -1, 100, 255, 256, 0x10000, 0x10001, INT_MAX, INT_MIN, 0x7fff };
    int i;
    for (i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++) {
        HDC dc = mt_dc();
        U(SetMapMode)(dc, MM_HIMETRIC);
        SetLastError(0xdeadbeef);
        int r = U(SetMapMode)(dc, bad[i]);
        CHECKF(r == 0, "SetMapMode(%d) returned %d", bad[i], r);
        CHECKF(U(GetMapMode)(dc) == MM_HIMETRIC, "mode changed to %d by invalid %d", U(GetMapMode)(dc), bad[i]);
        t_note("SetMapMode(%d) last error %lu", bad[i], GetLastError());
        mt_free_dc(dc);
    }
}

TEST(mapping_mapmode_invalid_dc)
{
    REQUIRE(U(SetMapMode)); REQUIRE(U(GetMapMode)); REQUIRE(U(CreatePen)); REQUIRE(U(DeleteObject));
    HPEN pen = U(CreatePen)(PS_SOLID, 1, 0);
    HDC dead = mt_dc(); mt_free_dc(dead);
    HDC bad[] = { NULL, (HDC)1, (HDC)0xdeadbeef, (HDC)(ULONG_PTR)0x7fffffff, (HDC)pen, dead, (HDC)(ULONG_PTR)0x00010000 };
    int i;
    for (i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++) {
        SetLastError(0xdeadbeef);
        int r = U(SetMapMode)(bad[i], MM_TEXT);
        CHECKF(r == 0, "idx %d SetMapMode=%d", i, r);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
        SetLastError(0xdeadbeef);
        r = U(GetMapMode)(bad[i]);
        CHECKF(r == 0, "idx %d GetMapMode=%d", i, r);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    }
    U(DeleteObject)(pen);
}

TEST(mapping_mapmode_stock_objects_are_not_dcs)
{
    REQUIRE(U(SetMapMode)); REQUIRE(U(GetStockObject));
    int i;
    for (i = 0; i <= 19; i++) {
        HGDIOBJ o = U(GetStockObject)(i);
        if (!o) continue;
        SetLastError(0xdeadbeef);
        CHECKF(U(SetMapMode)((HDC)o, MM_TEXT) == 0, "stock %d", i);
        EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    }
}

TEST(mapping_mapmode_fixed_viewport_ext)
{
    REQUIRE(U(SetMapMode)); REQUIRE(U(GetDeviceCaps));
    HDC dc = mt_dc();
    long rx = U(GetDeviceCaps)(dc, HORZRES), ry = U(GetDeviceCaps)(dc, VERTRES);
    CHECK(rx > 0 && ry > 0);
    int i;
    for (i = 0; i < NFIXED; i++) {
        U(SetMapMode)(dc, fixed_modes[i]);
        SIZE s = {0, 0};
        CHECK(U(GetViewportExtEx)(dc, &s));
        CHECKF(s.cx == rx && s.cy == -ry, "mode %d viewport ext %ld,%ld want %ld,%ld", fixed_modes[i], s.cx, s.cy, rx, -ry);
        SIZE w = {0, 0};
        CHECK(U(GetWindowExtEx)(dc, &w));
        CHECKF(w.cx > 0 && w.cy > 0, "mode %d window ext %ld,%ld", fixed_modes[i], w.cx, w.cy);
    }
    mt_free_dc(dc);
}

/* one logical unit of each fixed mode corresponds to a known physical size */
TEST(mapping_mapmode_fixed_units_vs_dpi)
{
    REQUIRE(U(SetMapMode)); REQUIRE(U(GetDeviceCaps)); REQUIRE(U(LPtoDP));
    static const struct { int mode; long per_inch; } t[] = {
        { MM_LOMETRIC, 254 }, { MM_HIMETRIC, 2540 }, { MM_LOENGLISH, 100 }, { MM_HIENGLISH, 1000 }, { MM_TWIPS, 1440 } };
    HDC dc = mt_dc();
    long dx = U(GetDeviceCaps)(dc, LOGPIXELSX), dy = U(GetDeviceCaps)(dc, LOGPIXELSY);
    CHECK(dx > 0 && dy > 0);
    t_note("LOGPIXELS %ld x %ld HORZSIZE %d VERTSIZE %d HORZRES %d VERTRES %d", dx, dy,
           U(GetDeviceCaps)(dc, HORZSIZE), U(GetDeviceCaps)(dc, VERTSIZE), U(GetDeviceCaps)(dc, HORZRES), U(GetDeviceCaps)(dc, VERTRES));
    int i;
    for (i = 0; i < 5; i++) {
        U(SetMapMode)(dc, t[i].mode);
        POINT p = { t[i].per_inch, t[i].per_inch };
        CHECK(U(LPtoDP)(dc, &p, 1));
        CHECKF(mt_near(p.x, dx, 30, 2), "mode %d: one inch x = %ld px, dpi %ld", t[i].mode, p.x, dx);
        CHECKF(mt_near(-p.y, dy, 30, 2), "mode %d: one inch y = %ld px, dpi %ld", t[i].mode, p.y, dy);
        CHECKF(p.y < 0, "mode %d y axis points up", t[i].mode);
    }
    mt_free_dc(dc);
}

TEST(mapping_mapmode_fixed_window_ext_vs_size)
{
    REQUIRE(U(SetMapMode)); REQUIRE(U(GetDeviceCaps));
    /* window extent is either HORZSIZE-based or dpi-based depending on release; both agree within rounding of mm */
    static const struct { int mode; long mul_num, mul_den; } t[] = {
        { MM_LOMETRIC, 10, 1 }, { MM_HIMETRIC, 100, 1 }, { MM_LOENGLISH, 1000, 254 }, { MM_HIENGLISH, 10000, 254 }, { MM_TWIPS, 14400, 254 } };
    HDC dc = mt_dc();
    long hs = U(GetDeviceCaps)(dc, HORZSIZE), vs = U(GetDeviceCaps)(dc, VERTSIZE);
    int i;
    for (i = 0; i < 5; i++) {
        U(SetMapMode)(dc, t[i].mode);
        SIZE w = {0, 0};
        U(GetWindowExtEx)(dc, &w);
        long ex = hs * t[i].mul_num / t[i].mul_den, ey = vs * t[i].mul_num / t[i].mul_den;
        CHECKF(mt_near(w.cx, ex, 30, 3), "mode %d cx %ld expected ~%ld", t[i].mode, w.cx, ex);
        CHECKF(mt_near(w.cy, ey, 30, 3), "mode %d cy %ld expected ~%ld", t[i].mode, w.cy, ey);
    }
    mt_free_dc(dc);
}

TEST(mapping_mapmode_fixed_ratios)
{
    REQUIRE(U(SetMapMode));
    HDC dc = mt_dc();
    SIZE lo, hi, loe, hie, tw;
    U(SetMapMode)(dc, MM_LOMETRIC); U(GetWindowExtEx)(dc, &lo);
    U(SetMapMode)(dc, MM_HIMETRIC); U(GetWindowExtEx)(dc, &hi);
    U(SetMapMode)(dc, MM_LOENGLISH); U(GetWindowExtEx)(dc, &loe);
    U(SetMapMode)(dc, MM_HIENGLISH); U(GetWindowExtEx)(dc, &hie);
    U(SetMapMode)(dc, MM_TWIPS); U(GetWindowExtEx)(dc, &tw);
    CHECKF(mt_near(hi.cx, lo.cx * 10, 10, 10), "himetric %ld vs lometric %ld", hi.cx, lo.cx);
    CHECKF(mt_near(hi.cy, lo.cy * 10, 10, 10), "himetric %ld vs lometric %ld", hi.cy, lo.cy);
    CHECKF(mt_near(hie.cx, loe.cx * 10, 10, 10), "hienglish %ld vs loenglish %ld", hie.cx, loe.cx);
    CHECKF(mt_near(tw.cx, hie.cx * 144 / 100, 10, 10), "twips %ld vs hienglish %ld", tw.cx, hie.cx);
    CHECKF(mt_near(lo.cx * 100, loe.cx * 254, 10, 300), "lometric %ld vs loenglish %ld", lo.cx, loe.cx);
    mt_free_dc(dc);
}

TEST(mapping_mapmode_back_to_text_resets_extents)
{
    REQUIRE(U(SetMapMode));
    HDC dc = mt_dc();
    for (int i = 0; i < NFIXED; i++) {
        U(SetMapMode)(dc, fixed_modes[i]);
        U(SetMapMode)(dc, MM_TEXT);
        EXT_IS(dc, GetWindowExtEx, 1, 1);
        EXT_IS(dc, GetViewportExtEx, 1, 1);
    }
    U(SetMapMode)(dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(dc, 50, 60, NULL);
    U(SetViewportExtEx)(dc, 70, 80, NULL);
    U(SetMapMode)(dc, MM_TEXT);
    EXT_IS(dc, GetWindowExtEx, 1, 1);
    EXT_IS(dc, GetViewportExtEx, 1, 1);
    mt_free_dc(dc);
}

TEST(mapping_mapmode_to_anisotropic_keeps_extents)
{
    REQUIRE(U(SetMapMode));
    HDC dc = mt_dc();
    for (int i = 0; i < NFIXED; i++) {
        U(SetMapMode)(dc, MM_TEXT);
        U(SetMapMode)(dc, fixed_modes[i]);
        SIZE w0, v0, w1, v1;
        U(GetWindowExtEx)(dc, &w0); U(GetViewportExtEx)(dc, &v0);
        MT_EQ(U(SetMapMode)(dc, MM_ANISOTROPIC), fixed_modes[i]);
        U(GetWindowExtEx)(dc, &w1); U(GetViewportExtEx)(dc, &v1);
        CHECKF(w0.cx == w1.cx && w0.cy == w1.cy, "mode %d window ext changed", fixed_modes[i]);
        CHECKF(v0.cx == v1.cx && v0.cy == v1.cy, "mode %d viewport ext changed", fixed_modes[i]);
    }
    mt_free_dc(dc);
}

TEST(mapping_mapmode_to_isotropic_keeps_extents)
{
    REQUIRE(U(SetMapMode));
    HDC dc = mt_dc();
    U(SetMapMode)(dc, MM_LOMETRIC);
    SIZE w0, v0, w1, v1;
    U(GetWindowExtEx)(dc, &w0); U(GetViewportExtEx)(dc, &v0);
    MT_EQ(U(SetMapMode)(dc, MM_ISOTROPIC), MM_LOMETRIC);
    U(GetWindowExtEx)(dc, &w1); U(GetViewportExtEx)(dc, &v1);
    t_note("isotropic switch: window %ld,%ld -> %ld,%ld viewport %ld,%ld -> %ld,%ld", w0.cx, w0.cy, w1.cx, w1.cy, v0.cx, v0.cy, v1.cx, v1.cy);
    CHECK(w1.cx != 0 && w1.cy != 0 && v1.cx != 0 && v1.cy != 0);
    mt_free_dc(dc);
}

TEST(mapping_ext_ignored_in_text_mode)
{
    REQUIRE(U(SetWindowExtEx)); REQUIRE(U(SetViewportExtEx));
    HDC dc = mt_dc();
    SIZE prev = {0x7777, 0x7777};
    SetLastError(0xdeadbeef);
    BOOL r = U(SetWindowExtEx)(dc, 123, 456, &prev);
    CHECK(r);
    CHECK(prev.cx == 1 && prev.cy == 1);
    EXT_IS(dc, GetWindowExtEx, 1, 1);
    prev.cx = prev.cy = 0x7777;
    r = U(SetViewportExtEx)(dc, 321, 654, &prev);
    CHECK(r);
    CHECK(prev.cx == 1 && prev.cy == 1);
    EXT_IS(dc, GetViewportExtEx, 1, 1);
    EXPECT_EQ(U(GetMapMode)(dc), MM_TEXT);
    mt_free_dc(dc);
}

TEST(mapping_ext_ignored_in_fixed_modes)
{
    REQUIRE(U(SetWindowExtEx)); REQUIRE(U(SetViewportExtEx));
    HDC dc = mt_dc();
    for (int i = 0; i < NFIXED; i++) {
        U(SetMapMode)(dc, fixed_modes[i]);
        SIZE w0, v0, p = {0x7777, 0x7777};
        U(GetWindowExtEx)(dc, &w0); U(GetViewportExtEx)(dc, &v0);
        BOOL r = U(SetWindowExtEx)(dc, 1111, 2222, &p);
        t_note("mode %d SetWindowExtEx ret %d", fixed_modes[i], r);
        EXT_IS(dc, GetWindowExtEx, w0.cx, w0.cy);
        r = U(SetViewportExtEx)(dc, 3333, 4444, &p);
        t_note("mode %d SetViewportExtEx ret %d", fixed_modes[i], r);
        EXT_IS(dc, GetViewportExtEx, v0.cx, v0.cy);
        CHECKF(U(GetMapMode)(dc) == fixed_modes[i], "mode switched");
    }
    mt_free_dc(dc);
}

TEST(mapping_ext_anisotropic_set_get)
{
    REQUIRE(U(SetWindowExtEx)); REQUIRE(U(SetViewportExtEx));
    static const long v[][4] = { {1,1,1,1}, {100,200,300,400}, {7,3,1,9}, {32767,32767,1,1}, {1,1,32767,32767},
                                 {1000,2000,3,5}, {65535,1,1,65535}, {-1,-1,1,1}, {1,1,-1,-1}, {-100,200,300,-400}, {-1,1,1,-1} };
    HDC dc = mt_dc();
    U(SetMapMode)(dc, MM_ANISOTROPIC);
    for (int i = 0; i < (int)(sizeof v / sizeof v[0]); i++) {
        BOOL r1 = U(SetWindowExtEx)(dc, v[i][0], v[i][1], NULL);
        BOOL r2 = U(SetViewportExtEx)(dc, v[i][2], v[i][3], NULL);
        CHECKF(r1 && r2, "row %d: set ret %d %d", i, r1, r2);
        EXT_IS(dc, GetWindowExtEx, v[i][0], v[i][1]);
        EXT_IS(dc, GetViewportExtEx, v[i][2], v[i][3]);
    }
    mt_free_dc(dc);
}

TEST(mapping_ext_prev_out)
{
    REQUIRE(U(SetWindowExtEx)); REQUIRE(U(SetViewportExtEx));
    HDC dc = mt_dc();
    U(SetMapMode)(dc, MM_ANISOTROPIC);
    SIZE p = {0x7777, 0x7777};
    CHECK(U(SetWindowExtEx)(dc, 10, 20, &p)); CHECK(p.cx == 1 && p.cy == 1);
    CHECK(U(SetWindowExtEx)(dc, 30, 40, &p)); CHECK(p.cx == 10 && p.cy == 20);
    CHECK(U(SetWindowExtEx)(dc, 50, 60, NULL));
    CHECK(U(SetWindowExtEx)(dc, 70, 80, &p)); CHECK(p.cx == 50 && p.cy == 60);
    CHECK(U(SetViewportExtEx)(dc, 11, 22, &p)); CHECK(p.cx == 1 && p.cy == 1);
    CHECK(U(SetViewportExtEx)(dc, 33, 44, &p)); CHECK(p.cx == 11 && p.cy == 22);
    CHECK(U(SetViewportExtEx)(dc, 55, 66, NULL));
    CHECK(U(SetViewportExtEx)(dc, 77, 88, &p)); CHECK(p.cx == 55 && p.cy == 66);
    mt_free_dc(dc);
}

TEST(mapping_ext_zero_values)
{
    REQUIRE(U(SetWindowExtEx)); REQUIRE(U(SetViewportExtEx)); REQUIRE(U(LPtoDP));
    static const long v[][2] = { {0,0}, {0,5}, {5,0} };
    for (int i = 0; i < 3; i++) {
        HDC dc = mt_dc();
        U(SetMapMode)(dc, MM_ANISOTROPIC);
        U(SetWindowExtEx)(dc, 10, 10, NULL);
        U(SetViewportExtEx)(dc, 10, 10, NULL);
        SetLastError(0xdeadbeef);
        BOOL r = U(SetWindowExtEx)(dc, v[i][0], v[i][1], NULL);
        t_note("SetWindowExtEx(%ld,%ld) ret %d err %lu", v[i][0], v[i][1], r, GetLastError());
        SIZE s; U(GetWindowExtEx)(dc, &s);
        t_note("  window ext now %ld,%ld", s.cx, s.cy);
        POINT p = {5, 5};
        r = U(LPtoDP)(dc, &p, 1);
        t_note("  LPtoDP ret %d -> %ld,%ld", r, p.x, p.y);
        mt_free_dc(dc);
        dc = mt_dc();
        U(SetMapMode)(dc, MM_ANISOTROPIC);
        r = U(SetViewportExtEx)(dc, v[i][0], v[i][1], NULL);
        t_note("SetViewportExtEx(%ld,%ld) ret %d", v[i][0], v[i][1], r);
        mt_free_dc(dc);
    }
}

TEST(mapping_ext_isotropic_set_get_roundtrip_before_adjust)
{
    REQUIRE(U(SetWindowExtEx)); REQUIRE(U(SetViewportExtEx));
    HDC dc = mt_dc();
    U(SetMapMode)(dc, MM_ISOTROPIC);
    SIZE p;
    CHECK(U(SetWindowExtEx)(dc, 100, 100, &p));
    EXT_IS(dc, GetWindowExtEx, 100, 100);
    CHECK(U(SetViewportExtEx)(dc, 200, 200, &p));
    EXT_IS(dc, GetViewportExtEx, 200, 200);
    EXT_IS(dc, GetWindowExtEx, 100, 100);
    mt_free_dc(dc);
}

TEST(mapping_ext_invalid_dc)
{
    REQUIRE(U(SetWindowExtEx)); REQUIRE(U(SetViewportExtEx)); REQUIRE(U(GetWindowExtEx)); REQUIRE(U(GetViewportExtEx));
    HDC dead = mt_dc(); mt_free_dc(dead);
    HPEN pen = U(CreatePen)(PS_SOLID, 1, 0);
    HDC bad[] = { NULL, (HDC)1, (HDC)0xdeadbeef, dead, (HDC)pen };
    for (int i = 0; i < 5; i++) {
        SIZE s = {0x7777, 0x7777};
        SetLastError(0xdeadbeef);
        DWORD want = bad[i] == dead ? ERROR_INVALID_PARAMETER : ERROR_INVALID_HANDLE;
        BOOL r = U(SetWindowExtEx)(bad[i], 5, 5, &s);
        CHECKF(!r, "idx %d SetWindowExtEx", i);
        EXPECT_ERR(r, want);
        SetLastError(0xdeadbeef);
        r = U(SetViewportExtEx)(bad[i], 5, 5, &s);
        CHECKF(!r, "idx %d SetViewportExtEx", i);
        EXPECT_ERR(r, want);
        CHECKF(s.cx == 0x7777 && s.cy == 0x7777, "idx %d prev untouched", i);
        r = U(GetWindowExtEx)(bad[i], &s);
        CHECKF(!r, "idx %d GetWindowExtEx", i);
        r = U(GetViewportExtEx)(bad[i], &s);
        CHECKF(!r, "idx %d GetViewportExtEx", i);
    }
    U(DeleteObject)(pen);
}

TEST(mapping_org_set_get_table)
{
    REQUIRE(U(SetWindowOrgEx)); REQUIRE(U(SetViewportOrgEx));
    static const long v[][2] = { {0,0}, {1,1}, {-1,-1}, {100,-200}, {-32768,32767}, {32768,-32769}, {65536,65536},
                                 {1000000,-1000000}, {12345678,-87654321}, {0,1}, {1,0} };
    for (int m = 0; m < 3; m++) {
        HDC dc = mt_dc();
        U(SetMapMode)(dc, m == 0 ? MM_TEXT : m == 1 ? MM_LOMETRIC : MM_ANISOTROPIC);
        for (int i = 0; i < (int)(sizeof v / sizeof v[0]); i++) {
            CHECK(U(SetWindowOrgEx)(dc, v[i][0], v[i][1], NULL));
            ORG_IS(dc, GetWindowOrgEx, v[i][0], v[i][1]);
            CHECK(U(SetViewportOrgEx)(dc, v[i][1], v[i][0], NULL));
            ORG_IS(dc, GetViewportOrgEx, v[i][1], v[i][0]);
        }
        mt_free_dc(dc);
    }
}

TEST(mapping_org_extreme_values)
{
    REQUIRE(U(SetWindowOrgEx)); REQUIRE(U(SetViewportOrgEx));
    static const long v[] = { INT_MAX, INT_MIN, INT_MAX - 1, INT_MIN + 1, 0x40000000, -0x40000000 };
    HDC dc = mt_dc();
    for (int i = 0; i < 6; i++) {
        BOOL r = U(SetWindowOrgEx)(dc, v[i], v[i], NULL);
        POINT p = {0, 0}; U(GetWindowOrgEx)(dc, &p);
        t_note("SetWindowOrgEx(%ld) ret %d -> %ld,%ld", v[i], r, p.x, p.y);
        r = U(SetViewportOrgEx)(dc, v[i], v[i], NULL);
        U(GetViewportOrgEx)(dc, &p);
        t_note("SetViewportOrgEx(%ld) ret %d -> %ld,%ld", v[i], r, p.x, p.y);
    }
    U(SetWindowOrgEx)(dc, 0, 0, NULL); U(SetViewportOrgEx)(dc, 0, 0, NULL);
    mt_free_dc(dc);
}

TEST(mapping_org_prev_out)
{
    REQUIRE(U(SetWindowOrgEx)); REQUIRE(U(SetViewportOrgEx));
    HDC dc = mt_dc();
    POINT p = {0x7777, 0x7777};
    CHECK(U(SetWindowOrgEx)(dc, 5, 6, &p)); CHECK(p.x == 0 && p.y == 0);
    CHECK(U(SetWindowOrgEx)(dc, 7, 8, &p)); CHECK(p.x == 5 && p.y == 6);
    CHECK(U(SetWindowOrgEx)(dc, 9, 10, NULL));
    CHECK(U(SetWindowOrgEx)(dc, 11, 12, &p)); CHECK(p.x == 9 && p.y == 10);
    CHECK(U(SetViewportOrgEx)(dc, 15, 16, &p)); CHECK(p.x == 0 && p.y == 0);
    CHECK(U(SetViewportOrgEx)(dc, 17, 18, &p)); CHECK(p.x == 15 && p.y == 16);
    CHECK(U(SetViewportOrgEx)(dc, 19, 20, NULL));
    CHECK(U(SetViewportOrgEx)(dc, 21, 22, &p)); CHECK(p.x == 19 && p.y == 20);
    /* same value twice */
    CHECK(U(SetViewportOrgEx)(dc, 21, 22, &p)); CHECK(p.x == 21 && p.y == 22);
    CHECK(U(SetWindowOrgEx)(dc, 11, 12, &p)); CHECK(p.x == 11 && p.y == 12);
    mt_free_dc(dc);
}

TEST(mapping_org_get_null_and_invalid)
{
    REQUIRE(U(GetWindowOrgEx)); REQUIRE(U(GetViewportOrgEx));
    HDC dc = mt_dc();
    if (!mt_is_wine()) {
        CHECK(!U(GetWindowOrgEx)(dc, NULL));
        CHECK(!U(GetViewportOrgEx)(dc, NULL));
    }
    HDC dead = mt_dc(); mt_free_dc(dead);
    HDC bad[] = { NULL, (HDC)1, (HDC)0xdeadbeef, dead };
    for (int i = 0; i < 4; i++) {
        POINT p = {0x7777, 0x7777};
        CHECKF(!U(GetWindowOrgEx)(bad[i], &p), "idx %d", i);
        CHECKF(p.x == 0x7777 && p.y == 0x7777, "idx %d GetWindowOrgEx touched out", i);
        CHECKF(!U(GetViewportOrgEx)(bad[i], &p), "idx %d", i);
        CHECKF(p.x == 0x7777 && p.y == 0x7777, "idx %d GetViewportOrgEx touched out", i);
    }
    mt_free_dc(dc);
}

TEST(mapping_org_set_invalid_dc)
{
    REQUIRE(U(SetWindowOrgEx)); REQUIRE(U(SetViewportOrgEx));
    HDC dead = mt_dc(); mt_free_dc(dead);
    HDC bad[] = { NULL, (HDC)1, (HDC)0xdeadbeef, dead };
    for (int i = 0; i < 4; i++) {
        POINT p = {0x7777, 0x7777};
        SetLastError(0xdeadbeef);
        CHECKF(!U(SetWindowOrgEx)(bad[i], 1, 1, &p), "window idx %d", i);
        if (bad[i] != dead) EXPECT_ERR(0, ERROR_INVALID_HANDLE);
        SetLastError(0xdeadbeef);
        CHECKF(!U(SetViewportOrgEx)(bad[i], 1, 1, &p), "viewport idx %d", i);
        if (bad[i] != dead) EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    }
}

TEST(mapping_org_survive_mapmode_changes)
{
    REQUIRE(U(SetWindowOrgEx)); REQUIRE(U(SetViewportOrgEx));
    HDC dc = mt_dc();
    U(SetWindowOrgEx)(dc, 11, 22, NULL);
    U(SetViewportOrgEx)(dc, 33, 44, NULL);
    for (int i = 0; i < NFIXED; i++) {
        U(SetMapMode)(dc, fixed_modes[i]);
        POINT w, v;
        U(GetWindowOrgEx)(dc, &w); U(GetViewportOrgEx)(dc, &v);
        t_note("mode %d: window org %ld,%ld viewport org %ld,%ld", fixed_modes[i], w.x, w.y, v.x, v.y);
    }
    mt_free_dc(dc);
}

TEST(mapping_org_set_in_every_mode)
{
    REQUIRE(U(SetWindowOrgEx)); REQUIRE(U(SetViewportOrgEx));
    HDC dc = mt_dc();
    for (int m = MM_TEXT; m <= MM_ANISOTROPIC; m++) {
        U(SetMapMode)(dc, m);
        CHECKF(U(SetWindowOrgEx)(dc, m * 10, -m * 10, NULL), "mode %d", m);
        CHECKF(U(SetViewportOrgEx)(dc, -m * 7, m * 7, NULL), "mode %d", m);
        ORG_IS(dc, GetWindowOrgEx, m * 10, -m * 10);
        ORG_IS(dc, GetViewportOrgEx, -m * 7, m * 7);
    }
    mt_free_dc(dc);
}

TEST(mapping_offset_window_org)
{
    REQUIRE(U(OffsetWindowOrgEx)); REQUIRE(U(SetWindowOrgEx));
    HDC dc = mt_dc();
    POINT p = {0x7777, 0x7777};
    CHECK(U(OffsetWindowOrgEx)(dc, 10, 20, &p)); CHECK(p.x == 0 && p.y == 0);
    ORG_IS(dc, GetWindowOrgEx, 10, 20);
    CHECK(U(OffsetWindowOrgEx)(dc, -3, 4, &p)); CHECK(p.x == 10 && p.y == 20);
    ORG_IS(dc, GetWindowOrgEx, 7, 24);
    CHECK(U(OffsetWindowOrgEx)(dc, 0, 0, &p)); CHECK(p.x == 7 && p.y == 24);
    ORG_IS(dc, GetWindowOrgEx, 7, 24);
    CHECK(U(OffsetWindowOrgEx)(dc, 1, 1, NULL));
    ORG_IS(dc, GetWindowOrgEx, 8, 25);
    CHECK(U(OffsetWindowOrgEx)(dc, -100000, 100000, NULL));
    ORG_IS(dc, GetWindowOrgEx, -99992, 100025);
    mt_free_dc(dc);
}

TEST(mapping_offset_viewport_org)
{
    REQUIRE(U(OffsetViewportOrgEx)); REQUIRE(U(SetViewportOrgEx));
    HDC dc = mt_dc();
    POINT p = {0x7777, 0x7777};
    CHECK(U(OffsetViewportOrgEx)(dc, 10, 20, &p)); CHECK(p.x == 0 && p.y == 0);
    ORG_IS(dc, GetViewportOrgEx, 10, 20);
    CHECK(U(OffsetViewportOrgEx)(dc, -3, 4, &p)); CHECK(p.x == 10 && p.y == 20);
    ORG_IS(dc, GetViewportOrgEx, 7, 24);
    CHECK(U(OffsetViewportOrgEx)(dc, 0, 0, &p)); CHECK(p.x == 7 && p.y == 24);
    CHECK(U(OffsetViewportOrgEx)(dc, 1, 1, NULL));
    ORG_IS(dc, GetViewportOrgEx, 8, 25);
    CHECK(U(OffsetViewportOrgEx)(dc, 100000, -100000, NULL));
    ORG_IS(dc, GetViewportOrgEx, 100008, -99975);
    mt_free_dc(dc);
}

TEST(mapping_offset_cumulative)
{
    REQUIRE(U(OffsetViewportOrgEx)); REQUIRE(U(OffsetWindowOrgEx));
    HDC dc = mt_dc();
    for (int i = 0; i < 1000; i++) { U(OffsetViewportOrgEx)(dc, 1, 2, NULL); U(OffsetWindowOrgEx)(dc, -2, -1, NULL); }
    ORG_IS(dc, GetViewportOrgEx, 1000, 2000);
    ORG_IS(dc, GetWindowOrgEx, -2000, -1000);
    mt_free_dc(dc);
}

TEST(mapping_offset_in_every_mode)
{
    REQUIRE(U(OffsetViewportOrgEx)); REQUIRE(U(OffsetWindowOrgEx));
    for (int m = MM_TEXT; m <= MM_ANISOTROPIC; m++) {
        HDC dc = mt_dc();
        U(SetMapMode)(dc, m);
        CHECKF(U(OffsetViewportOrgEx)(dc, 5, 6, NULL), "mode %d", m);
        CHECKF(U(OffsetWindowOrgEx)(dc, 7, 8, NULL), "mode %d", m);
        ORG_IS(dc, GetViewportOrgEx, 5, 6);
        ORG_IS(dc, GetWindowOrgEx, 7, 8);
        mt_free_dc(dc);
    }
}

TEST(mapping_offset_invalid_dc)
{
    REQUIRE(U(OffsetViewportOrgEx)); REQUIRE(U(OffsetWindowOrgEx));
    HDC dead = mt_dc(); mt_free_dc(dead);
    HDC bad[] = { NULL, (HDC)1, (HDC)0xdeadbeef, dead };
    for (int i = 0; i < 4; i++) {
        POINT p = {0x7777, 0x7777};
        SetLastError(0xdeadbeef);
        CHECKF(!U(OffsetViewportOrgEx)(bad[i], 1, 1, &p), "viewport idx %d", i);
        if (bad[i] != dead) EXPECT_ERR(0, ERROR_INVALID_HANDLE);
        SetLastError(0xdeadbeef);
        CHECKF(!U(OffsetWindowOrgEx)(bad[i], 1, 1, &p), "window idx %d", i);
        if (bad[i] != dead) EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    }
}

TEST(mapping_offset_overflow_values)
{
    REQUIRE(U(OffsetViewportOrgEx)); REQUIRE(U(OffsetWindowOrgEx)); REQUIRE(U(SetViewportOrgEx));
    HDC dc = mt_dc();
    U(SetViewportOrgEx)(dc, INT_MAX - 5, 0, NULL);
    BOOL r = U(OffsetViewportOrgEx)(dc, 10, 0, NULL);
    POINT p; U(GetViewportOrgEx)(dc, &p);
    t_note("viewport org overflow: ret %d -> %ld", r, p.x);
    U(SetViewportOrgEx)(dc, 0, 0, NULL);
    r = U(OffsetWindowOrgEx)(dc, INT_MAX, INT_MAX, NULL);
    r = U(OffsetWindowOrgEx)(dc, INT_MAX, INT_MAX, NULL);
    U(GetWindowOrgEx)(dc, &p);
    t_note("window org overflow: ret %d -> %ld,%ld", r, p.x, p.y);
    mt_free_dc(dc);
}

TEST(mapping_dcs_independent)
{
    REQUIRE(U(SetMapMode)); REQUIRE(U(SetWindowOrgEx));
    HDC a = mt_dc(), b = mt_dc();
    U(SetMapMode)(a, MM_TWIPS);
    U(SetWindowOrgEx)(a, 5, 5, NULL);
    EXPECT_EQ(U(GetMapMode)(b), MM_TEXT);
    ORG_IS(b, GetWindowOrgEx, 0, 0);
    EXPECT_EQ(U(GetMapMode)(a), MM_TWIPS);
    mt_free_dc(a); mt_free_dc(b);
}

TEST(mapping_new_dc_no_inherit)
{
    REQUIRE(U(SetMapMode));
    for (int i = 0; i < 20; i++) {
        HDC dc = mt_dc();
        EXPECT_EQ(U(GetMapMode)(dc), MM_TEXT);
        U(SetMapMode)(dc, MM_TWIPS);
        U(SetViewportOrgEx)(dc, 9, 9, NULL);
        mt_free_dc(dc);
    }
}

TEST(mapping_no_object_leak)
{
    REQUIRE(U(GetGuiResources)); REQUIRE(U(SetMapMode));
    DWORD before = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    for (int i = 0; i < 300; i++) {
        HDC dc = mt_dc();
        U(SetMapMode)(dc, MM_ANISOTROPIC);
        U(SetWindowExtEx)(dc, 5, 5, NULL);
        U(SetViewportOrgEx)(dc, i, i, NULL);
        mt_free_dc(dc);
    }
    DWORD after = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    CHECKF(before == after, "gdi objects %lu -> %lu", before, after);
}

static DWORD WINAPI xthread(LPVOID p)
{
    HDC dc = (HDC)p;
    int prev = U(SetMapMode)(dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(dc, 4, 4, NULL);
    U(SetViewportExtEx)(dc, 8, 8, NULL);
    POINT pt = {10, 10};
    BOOL ok = U(LPtoDP)(dc, &pt, 1);
    return (prev == MM_TEXT) | (ok << 1) | ((pt.x == 20 && pt.y == 20) << 2);
}

TEST(mapping_cross_thread_dc)
{
    REQUIRE(U(SetMapMode)); REQUIRE(U(LPtoDP));
    HDC dc = mt_dc();
    HANDLE h = CreateThread(NULL, 0, xthread, dc, 0, NULL);
    CHECK(h != NULL);
    if (h) {
        WaitForSingleObject(h, 10000);
        DWORD code = 0; GetExitCodeThread(h, &code); CloseHandle(h);
        CHECKF(code == 7, "thread result %lu", code);
        EXPECT_EQ(U(GetMapMode)(dc), MM_ANISOTROPIC);
        EXT_IS(dc, GetViewportExtEx, 8, 8);
    }
    mt_free_dc(dc);
}
