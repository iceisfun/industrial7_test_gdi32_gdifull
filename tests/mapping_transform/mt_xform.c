#include "mt.h"

static XFORM xf(float a, float b, float c, float d, float e, float f)
{
    XFORM x; x.eM11 = a; x.eM12 = b; x.eM21 = c; x.eM22 = d; x.eDx = e; x.eDy = f; return x;
}
static HDC adv_dc(void)
{
    HDC dc = mt_dc();
    if (dc) U(SetGraphicsMode)(dc, GM_ADVANCED);
    return dc;
}

/* ---------------- graphics mode ---------------- */
TEST(mapping_gfxmode_default_and_prev)
{
    REQUIRE(U(GetGraphicsMode)); REQUIRE(U(SetGraphicsMode));
    HDC dc = mt_dc();
    MT_EQ(U(GetGraphicsMode)(dc), GM_COMPATIBLE);
    MT_EQ(U(SetGraphicsMode)(dc, GM_ADVANCED), GM_COMPATIBLE);
    MT_EQ(U(GetGraphicsMode)(dc), GM_ADVANCED);
    MT_EQ(U(SetGraphicsMode)(dc, GM_ADVANCED), GM_ADVANCED);
    MT_EQ(U(SetGraphicsMode)(dc, GM_COMPATIBLE), GM_ADVANCED);
    MT_EQ(U(SetGraphicsMode)(dc, GM_COMPATIBLE), GM_COMPATIBLE);
    MT_EQ(U(GetGraphicsMode)(dc), GM_COMPATIBLE);
    mt_free_dc(dc);
}

TEST(mapping_gfxmode_invalid_values)
{
    REQUIRE(U(GetGraphicsMode)); REQUIRE(U(SetGraphicsMode));
    static const int bad[] = { 0, 3, 4, -1, 100, 0x10001, INT_MAX, INT_MIN };
    for (int i = 0; i < 8; i++) {
        HDC dc = mt_dc();
        U(SetGraphicsMode)(dc, GM_ADVANCED);
        SetLastError(0xdeadbeef);
        int r = U(SetGraphicsMode)(dc, bad[i]);
        CHECKF(r == 0, "SetGraphicsMode(%d) = %d", bad[i], r);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
        MT_EQ(U(GetGraphicsMode)(dc), GM_ADVANCED);
        mt_free_dc(dc);
    }
}

TEST(mapping_gfxmode_invalid_dc)
{
    REQUIRE(U(GetGraphicsMode)); REQUIRE(U(SetGraphicsMode));
    HDC dead = mt_dc(); mt_free_dc(dead);
    HDC bad[] = { NULL, (HDC)1, (HDC)0xdeadbeef, dead };
    for (int i = 0; i < 4; i++) {
        SetLastError(0xdeadbeef);
        int r = U(SetGraphicsMode)(bad[i], GM_ADVANCED);
        CHECKF(r == 0, "idx %d", i);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
        SetLastError(0xdeadbeef);
        r = U(GetGraphicsMode)(bad[i]);
        CHECKF(r == 0, "Get idx %d = %d", i, r);
        t_note("GetGraphicsMode bad idx %d err %lu", i, GetLastError());
    }
}

/* ---------------- Set/GetWorldTransform ---------------- */
TEST(mapping_wt_compat_mode_rejects_set)
{
    REQUIRE(U(SetWorldTransform)); REQUIRE(U(GetWorldTransform));
    HDC dc = mt_dc();
    XFORM x = xf(1, 0, 0, 1, 0, 0), g;
    SetLastError(0xdeadbeef);
    CHECK(!U(SetWorldTransform)(dc, &x));
    t_note("SetWorldTransform compat err %lu", GetLastError());
    x = xf(2, 0, 0, 2, 5, 5);
    CHECK(!U(SetWorldTransform)(dc, &x));
    CHECK(U(GetWorldTransform)(dc, &g));
    XF_IS(g, 1, 0, 0, 1, 0, 0);
    mt_free_dc(dc);
}

TEST(mapping_wt_default_identity)
{
    REQUIRE(U(GetWorldTransform));
    HDC dc = mt_dc();
    XFORM g = xf(9, 9, 9, 9, 9, 9);
    CHECK(U(GetWorldTransform)(dc, &g));
    XF_IS(g, 1, 0, 0, 1, 0, 0);
    U(SetGraphicsMode)(dc, GM_ADVANCED);
    g = xf(9, 9, 9, 9, 9, 9);
    CHECK(U(GetWorldTransform)(dc, &g));
    XF_IS(g, 1, 0, 0, 1, 0, 0);
    mt_free_dc(dc);
}

TEST(mapping_wt_set_get_roundtrip)
{
    REQUIRE(U(SetWorldTransform)); REQUIRE(U(GetWorldTransform));
    static const float t[][6] = {
        { 1, 0, 0, 1, 0, 0 }, { 2, 0, 0, 3, 0, 0 }, { 1, 0, 0, 1, 10, -20 }, { -1, 0, 0, 1, 63, 0 }, { 1, 0, 0, -1, 0, 63 },
        { 0, 1, -1, 0, 0, 0 }, { 0, -1, 1, 0, 0, 0 }, { 1, 0, 1, 1, 0, 0 }, { 1, 1, 0, 1, 0, 0 }, { 0.5f, 0, 0, 0.5f, 0, 0 },
        { 0.25f, 0.125f, -0.5f, 2, 1.5f, -2.5f }, { 1e-3f, 0, 0, 1e3f, 0, 0 }, { 1, 0, 0, 1, 1e6f, -1e6f }, { 0.70710678f, 0.70710678f, -0.70710678f, 0.70710678f, 3, 4 },
        { 1e10f, 0, 0, 1e-10f, 0, 0 }, { -3, 0, 0, -3, 0, 0 },
    };
    HDC dc = adv_dc();
    for (int i = 0; i < (int)(sizeof t / sizeof t[0]); i++) {
        XFORM x = xf(t[i][0], t[i][1], t[i][2], t[i][3], t[i][4], t[i][5]), g = xf(7, 7, 7, 7, 7, 7);
        BOOL r = U(SetWorldTransform)(dc, &x);
        CHECKF(r, "row %d set", i);
        CHECKF(U(GetWorldTransform)(dc, &g), "row %d get", i);
        CHECKF(g.eM11 == x.eM11 && g.eM12 == x.eM12 && g.eM21 == x.eM21 && g.eM22 == x.eM22 && g.eDx == x.eDx && g.eDy == x.eDy,
               "row %d roundtrip got %g %g %g %g %g %g", i, g.eM11, g.eM12, g.eM21, g.eM22, g.eDx, g.eDy);
    }
    mt_free_dc(dc);
}

TEST(mapping_wt_singular_rejected)
{
    REQUIRE(U(SetWorldTransform)); REQUIRE(U(GetWorldTransform));
    static const float t[][6] = {
        { 1, 2, 1, 2, 0, 0 }, { 0, 0, 0, 0, 5, 5 }, { 1, 0, 0, 0, 0, 0 }, { 0, 0, 0, 1, 0, 0 }, { 2, 4, 1, 2, 0, 0 },
        { 0, 1, 0, 1, 0, 0 }, { 1, 1, 1, 1, 10, 10 }, { 3, 0, 3, 0, 0, 0 },
    };
    HDC dc = adv_dc();
    XFORM good = xf(2, 0, 0, 2, 1, 1);
    CHECK(U(SetWorldTransform)(dc, &good));
    for (int i = 0; i < (int)(sizeof t / sizeof t[0]); i++) {
        XFORM x = xf(t[i][0], t[i][1], t[i][2], t[i][3], t[i][4], t[i][5]), g;
        SetLastError(0xdeadbeef);
        BOOL r = U(SetWorldTransform)(dc, &x);
        CHECKF(!r, "row %d singular accepted", i);
        t_note("singular row %d err %lu", i, GetLastError());
        U(GetWorldTransform)(dc, &g);
        CHECKF(g.eM11 == 2 && g.eM22 == 2 && g.eDx == 1, "row %d changed state", i);
    }
    mt_free_dc(dc);
}

TEST(mapping_wt_nonfinite_values)
{
    REQUIRE(U(SetWorldTransform)); REQUIRE(U(GetWorldTransform));
    HDC dc = adv_dc();
    union { unsigned u; float f; } nan_ = { 0x7fc00000 }, inf_ = { 0x7f800000 };
    XFORM x = xf(nan_.f, 0, 0, 1, 0, 0), g;
    BOOL r = U(SetWorldTransform)(dc, &x);
    U(GetWorldTransform)(dc, &g);
    t_note("NaN xform ret %d -> %g", r, g.eM11);
    x = xf(inf_.f, 0, 0, 1, 0, 0);
    r = U(SetWorldTransform)(dc, &x);
    U(GetWorldTransform)(dc, &g);
    t_note("Inf xform ret %d -> %g", r, g.eM11);
    x = xf(1, 0, 0, 1, nan_.f, 0);
    r = U(SetWorldTransform)(dc, &x);
    U(GetWorldTransform)(dc, &g);
    t_note("NaN offset ret %d -> %g", r, g.eDx);
    mt_free_dc(dc);
}

TEST(mapping_wt_null_and_invalid_dc)
{
    REQUIRE(U(SetWorldTransform)); REQUIRE(U(GetWorldTransform));
    HDC dc = adv_dc();
    XFORM x = xf(1, 0, 0, 1, 0, 0), g;
    if (!mt_is_wine()) {
        BOOL r = U(SetWorldTransform)(dc, NULL);
        CHECK(!r);
        r = U(GetWorldTransform)(dc, NULL);
        CHECK(!r);
    }
    HDC dead = mt_dc(); mt_free_dc(dead);
    HDC bad[] = { NULL, (HDC)1, (HDC)0xdeadbeef, dead };
    for (int i = 0; i < 4; i++) {
        CHECKF(!U(SetWorldTransform)(bad[i], &x), "set idx %d", i);
        g = xf(5, 5, 5, 5, 5, 5);
        CHECKF(!U(GetWorldTransform)(bad[i], &g), "get idx %d", i);
        CHECKF(g.eM11 == 5, "get idx %d touched output", i);
    }
    mt_free_dc(dc);
}

TEST(mapping_wt_survives_mapmode_and_gfxmode)
{
    REQUIRE(U(SetWorldTransform)); REQUIRE(U(GetWorldTransform));
    HDC dc = adv_dc();
    XFORM x = xf(3, 0, 0, 4, 5, 6), g;
    CHECK(U(SetWorldTransform)(dc, &x));
    U(SetMapMode)(dc, MM_LOMETRIC);
    U(GetWorldTransform)(dc, &g); XF_IS(g, 3, 0, 0, 4, 5, 6);
    U(SetMapMode)(dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(dc, 3, 3, NULL);
    U(GetWorldTransform)(dc, &g); XF_IS(g, 3, 0, 0, 4, 5, 6);
    U(SetMapMode)(dc, MM_TEXT);
    U(GetWorldTransform)(dc, &g); XF_IS(g, 3, 0, 0, 4, 5, 6);
    CHECK(U(SetGraphicsMode)(dc, GM_COMPATIBLE) == GM_ADVANCED);
    U(GetWorldTransform)(dc, &g); XF_IS(g, 3, 0, 0, 4, 5, 6);
    LP_IS(dc, 10, 10, 35, 46);
    mt_free_dc(dc);
}

/* ---------------- ModifyWorldTransform ---------------- */
TEST(mapping_mwt_identity)
{
    REQUIRE(U(ModifyWorldTransform)); REQUIRE(U(SetWorldTransform));
    HDC dc = adv_dc();
    XFORM x = xf(2, 1, 1, 2, 3, 4), g, junk = xf(9, 9, 9, 9, 9, 9);
    CHECK(U(SetWorldTransform)(dc, &x));
    CHECK(U(ModifyWorldTransform)(dc, NULL, MWT_IDENTITY));
    U(GetWorldTransform)(dc, &g); XF_IS(g, 1, 0, 0, 1, 0, 0);
    CHECK(U(SetWorldTransform)(dc, &x));
    CHECK(U(ModifyWorldTransform)(dc, &junk, MWT_IDENTITY));
    U(GetWorldTransform)(dc, &g); XF_IS(g, 1, 0, 0, 1, 0, 0);
    mt_free_dc(dc);
}

TEST(mapping_mwt_set)
{
    REQUIRE(U(ModifyWorldTransform)); REQUIRE(U(SetWorldTransform));
    HDC dc = adv_dc();
    XFORM x = xf(2, 0, 0, 3, 4, 5), g;
    CHECK(U(ModifyWorldTransform)(dc, &x, MWT_SET));
    U(GetWorldTransform)(dc, &g); XF_IS(g, 2, 0, 0, 3, 4, 5);
    XFORM sing = xf(1, 1, 1, 1, 0, 0);
    BOOL r = U(ModifyWorldTransform)(dc, &sing, MWT_SET);
    t_note("MWT_SET singular ret %d", r);
    mt_free_dc(dc);
}

TEST(mapping_mwt_left_right_order)
{
    REQUIRE(U(ModifyWorldTransform)); REQUIRE(U(SetWorldTransform));
    HDC dc = adv_dc();
    XFORM cur = xf(2, 0, 0, 2, 0, 0), t = xf(1, 0, 0, 1, 10, 0), g;
    CHECK(U(SetWorldTransform)(dc, &cur));
    CHECK(U(ModifyWorldTransform)(dc, &t, MWT_LEFTMULTIPLY));    /* new = t * cur: translate first, then scale */
    U(GetWorldTransform)(dc, &g); XF_IS(g, 2, 0, 0, 2, 20, 0);
    LP_IS(dc, 1, 0, 22, 0);
    CHECK(U(SetWorldTransform)(dc, &cur));
    CHECK(U(ModifyWorldTransform)(dc, &t, MWT_RIGHTMULTIPLY));   /* new = cur * t: scale first, then translate */
    U(GetWorldTransform)(dc, &g); XF_IS(g, 2, 0, 0, 2, 10, 0);
    LP_IS(dc, 1, 0, 12, 0);
    mt_free_dc(dc);
}

TEST(mapping_mwt_left_right_general)
{
    REQUIRE(U(ModifyWorldTransform)); REQUIRE(U(SetWorldTransform));
    HDC dc = adv_dc();
    XFORM a = xf(1, 2, 3, 4, 5, 6), b = xf(7, 8, 9, 10, 11, 12), g;
    /* a*b : [a11 a12;a21 a22;dx dy] x [b..] */
    float e11 = 1 * 7 + 2 * 9, e12 = 1 * 8 + 2 * 10, e21 = 3 * 7 + 4 * 9, e22 = 3 * 8 + 4 * 10;
    float edx = 5 * 7 + 6 * 9 + 11, edy = 5 * 8 + 6 * 10 + 12;
    CHECK(U(SetWorldTransform)(dc, &a));
    CHECK(U(ModifyWorldTransform)(dc, &b, MWT_RIGHTMULTIPLY));
    U(GetWorldTransform)(dc, &g); XF_IS(g, e11, e12, e21, e22, edx, edy);
    /* b*a */
    float f11 = 7 * 1 + 8 * 3, f12 = 7 * 2 + 8 * 4, f21 = 9 * 1 + 10 * 3, f22 = 9 * 2 + 10 * 4;
    float fdx = 11 * 1 + 12 * 3 + 5, fdy = 11 * 2 + 12 * 4 + 6;
    CHECK(U(SetWorldTransform)(dc, &a));
    CHECK(U(ModifyWorldTransform)(dc, &b, MWT_LEFTMULTIPLY));
    U(GetWorldTransform)(dc, &g); XF_IS(g, f11, f12, f21, f22, fdx, fdy);
    mt_free_dc(dc);
}

TEST(mapping_mwt_chain_builds_rotation_about_point)
{
    REQUIRE(U(ModifyWorldTransform)); REQUIRE(U(LPtoDP));
    HDC dc = adv_dc();
    XFORM tr1 = xf(1, 0, 0, 1, -10, -10), rot = xf(0, 1, -1, 0, 0, 0), tr2 = xf(1, 0, 0, 1, 10, 10);
    CHECK(U(ModifyWorldTransform)(dc, NULL, MWT_IDENTITY));
    CHECK(U(ModifyWorldTransform)(dc, &tr1, MWT_RIGHTMULTIPLY));
    CHECK(U(ModifyWorldTransform)(dc, &rot, MWT_RIGHTMULTIPLY));
    CHECK(U(ModifyWorldTransform)(dc, &tr2, MWT_RIGHTMULTIPLY));
    LP_IS(dc, 10, 10, 10, 10);
    LP_IS(dc, 20, 10, 10, 20);
    LP_IS(dc, 10, 20, 0, 10);
    mt_free_dc(dc);
}

TEST(mapping_mwt_invalid_mode)
{
    REQUIRE(U(ModifyWorldTransform)); REQUIRE(U(SetWorldTransform));
    static const DWORD bad[] = { 0, 5, 6, 100, 0x10001, 0x7fffffff, 0xffffffff };
    HDC dc = adv_dc();
    XFORM cur = xf(2, 0, 0, 2, 1, 1), x = xf(3, 0, 0, 3, 0, 0), g;
    for (int i = 0; i < 7; i++) {
        U(SetWorldTransform)(dc, &cur);
        SetLastError(0xdeadbeef);
        BOOL r = U(ModifyWorldTransform)(dc, &x, bad[i]);
        CHECKF(!r, "mode %lu accepted", bad[i]);
        t_note("MWT mode %lu err %lu", bad[i], GetLastError());
        U(GetWorldTransform)(dc, &g);
        XF_IS(g, 2, 0, 0, 2, 1, 1);
    }
    mt_free_dc(dc);
}

TEST(mapping_mwt_requires_advanced)
{
    REQUIRE(U(ModifyWorldTransform));
    HDC dc = mt_dc();
    XFORM x = xf(2, 0, 0, 2, 0, 0), g;
    SetLastError(0xdeadbeef);
    t_note("GM_COMPATIBLE: LEFT %d RIGHT %d SET %d", U(ModifyWorldTransform)(dc, &x, MWT_LEFTMULTIPLY),
           U(ModifyWorldTransform)(dc, &x, MWT_RIGHTMULTIPLY), U(ModifyWorldTransform)(dc, &x, MWT_SET));
    BOOL r = U(ModifyWorldTransform)(dc, NULL, MWT_IDENTITY);
    t_note("MWT_IDENTITY in GM_COMPATIBLE ret %d", r);
    CHECK(!U(SetWorldTransform)(dc, &x));
    mt_free_dc(dc);
}

TEST(mapping_mwt_null_xform_and_invalid_dc)
{
    REQUIRE(U(ModifyWorldTransform));
    HDC dc = adv_dc();
    if (!mt_is_wine()) {
        CHECK(!U(ModifyWorldTransform)(dc, NULL, MWT_LEFTMULTIPLY));
        CHECK(!U(ModifyWorldTransform)(dc, NULL, MWT_RIGHTMULTIPLY));
        CHECK(!U(ModifyWorldTransform)(dc, NULL, MWT_SET));
    }
    XFORM x = xf(1, 0, 0, 1, 0, 0);
    HDC dead = mt_dc(); mt_free_dc(dead);
    HDC bad[] = { NULL, (HDC)1, (HDC)0xdeadbeef, dead };
    for (int i = 0; i < 4; i++) {
        CHECKF(!U(ModifyWorldTransform)(bad[i], &x, MWT_SET), "idx %d", i);
        CHECKF(!U(ModifyWorldTransform)(bad[i], &x, MWT_LEFTMULTIPLY), "idx %d", i);
        CHECKF(!U(ModifyWorldTransform)(bad[i], NULL, MWT_IDENTITY), "idx %d", i);
    }
    mt_free_dc(dc);
}

TEST(mapping_mwt_repeated_scale_accumulates)
{
    REQUIRE(U(ModifyWorldTransform));
    HDC dc = adv_dc();
    XFORM s = xf(2, 0, 0, 2, 0, 0), g;
    for (int i = 0; i < 10; i++) CHECK(U(ModifyWorldTransform)(dc, &s, MWT_RIGHTMULTIPLY));
    U(GetWorldTransform)(dc, &g); XF_IS(g, 1024, 0, 0, 1024, 0, 0);
    for (int i = 0; i < 10; i++) CHECK(U(ModifyWorldTransform)(dc, &s, MWT_LEFTMULTIPLY));
    U(GetWorldTransform)(dc, &g); XF_IS(g, 1048576, 0, 0, 1048576, 0, 0);
    mt_free_dc(dc);
}

/* ---------------- CombineTransform ---------------- */
TEST(mapping_combine_basic)
{
    REQUIRE(U(CombineTransform));
    XFORM a = xf(2, 0, 0, 2, 0, 0), b = xf(1, 0, 0, 1, 10, 0), o = xf(9, 9, 9, 9, 9, 9);
    CHECK(U(CombineTransform)(&o, &a, &b));  /* a then b */
    XF_IS(o, 2, 0, 0, 2, 10, 0);
    CHECK(U(CombineTransform)(&o, &b, &a));
    XF_IS(o, 2, 0, 0, 2, 20, 0);
}

TEST(mapping_combine_general_matrix)
{
    REQUIRE(U(CombineTransform));
    XFORM a = xf(1, 2, 3, 4, 5, 6), b = xf(7, 8, 9, 10, 11, 12), o;
    CHECK(U(CombineTransform)(&o, &a, &b));
    XF_IS(o, 1 * 7 + 2 * 9, 1 * 8 + 2 * 10, 3 * 7 + 4 * 9, 3 * 8 + 4 * 10, 5 * 7 + 6 * 9 + 11, 5 * 8 + 6 * 10 + 12);
    CHECK(U(CombineTransform)(&o, &b, &a));
    XF_IS(o, 7 * 1 + 8 * 3, 7 * 2 + 8 * 4, 9 * 1 + 10 * 3, 9 * 2 + 10 * 4, 11 * 1 + 12 * 3 + 5, 11 * 2 + 12 * 4 + 6);
}

TEST(mapping_combine_identity_neutral)
{
    REQUIRE(U(CombineTransform));
    XFORM id = xf(1, 0, 0, 1, 0, 0), m = xf(0.5f, -2, 3, 0.25f, 7, -8), o;
    CHECK(U(CombineTransform)(&o, &m, &id)); XF_IS(o, 0.5f, -2, 3, 0.25f, 7, -8);
    CHECK(U(CombineTransform)(&o, &id, &m)); XF_IS(o, 0.5f, -2, 3, 0.25f, 7, -8);
    CHECK(U(CombineTransform)(&o, &id, &id)); XF_IS(o, 1, 0, 0, 1, 0, 0);
}

TEST(mapping_combine_aliasing)
{
    REQUIRE(U(CombineTransform));
    XFORM a = xf(2, 0, 0, 3, 1, 1), b = xf(1, 0, 0, 1, 4, 5);
    XFORM o = a;
    CHECK(U(CombineTransform)(&o, &o, &b));
    XF_IS(o, 2, 0, 0, 3, 5, 6);
    o = b;
    CHECK(U(CombineTransform)(&o, &a, &o));
    XF_IS(o, 2, 0, 0, 3, 5, 6);
    o = a;
    CHECK(U(CombineTransform)(&o, &o, &o));
    XF_IS(o, 4, 0, 0, 9, 3, 4);
}

TEST(mapping_combine_rotation_composition)
{
    REQUIRE(U(CombineTransform));
    XFORM r90 = xf(0, 1, -1, 0, 0, 0), o, t;
    CHECK(U(CombineTransform)(&o, &r90, &r90)); XF_IS(o, -1, 0, 0, -1, 0, 0);
    t = o;
    CHECK(U(CombineTransform)(&o, &t, &t)); XF_IS(o, 1, 0, 0, 1, 0, 0);
}

TEST(mapping_combine_singular_result)
{
    REQUIRE(U(CombineTransform));
    XFORM a = xf(1, 0, 0, 0, 0, 0), b = xf(1, 0, 0, 1, 0, 0), o = xf(9, 9, 9, 9, 9, 9);
    BOOL r = U(CombineTransform)(&o, &a, &b);
    t_note("combine with singular input ret %d -> %g %g %g %g", r, o.eM11, o.eM12, o.eM21, o.eM22);
}

TEST(mapping_combine_null_args)
{
    REQUIRE(U(CombineTransform));
    if (mt_is_wine()) { t_skip("wine"); return; }
    XFORM a = xf(1, 0, 0, 1, 0, 0), o = xf(9, 9, 9, 9, 9, 9);
    CHECK(!U(CombineTransform)(&o, NULL, &a));
    CHECK(!U(CombineTransform)(&o, &a, NULL));
    CHECK(!U(CombineTransform)(&o, NULL, NULL));
    CHECK(o.eM11 == 9);
}

TEST(mapping_combine_matches_modify)
{
    REQUIRE(U(CombineTransform)); REQUIRE(U(ModifyWorldTransform));
    HDC dc = adv_dc();
    XFORM a = xf(0.5f, 1, -1, 2, 3, 4), b = xf(2, 0.25f, 0, 1, -5, 6), o, g;
    CHECK(U(SetWorldTransform)(dc, &a));
    CHECK(U(ModifyWorldTransform)(dc, &b, MWT_RIGHTMULTIPLY));
    CHECK(U(CombineTransform)(&o, &a, &b));
    U(GetWorldTransform)(dc, &g);
    XF_IS(g, o.eM11, o.eM12, o.eM21, o.eM22, o.eDx, o.eDy);
    mt_free_dc(dc);
}

/* ---------------- effect on LPtoDP/DPtoLP ---------------- */
TEST(mapping_wxf_lptodp_table)
{
    REQUIRE(U(LPtoDP)); REQUIRE(U(SetWorldTransform));
    static const struct { float m[6]; int lx, ly, dx, dy; } t[] = {
        { {1,0,0,1,10,20},   5, 5, 15, 25 },
        { {2,0,0,3,0,0},     5, 5, 10, 15 },
        { {0,1,-1,0,0,0},    10, 0, 0, 10 },
        { {0,1,-1,0,0,0},    0, 10, -10, 0 },
        { {0,-1,1,0,0,0},    10, 0, 0, -10 },
        { {-1,0,0,1,0,0},    10, 4, -10, 4 },
        { {1,0,0,-1,0,0},    10, 4, 10, -4 },
        { {1,0,1,1,0,0},     3, 4, 7, 4 },    /* x' = x + y */
        { {1,1,0,1,0,0},     3, 4, 3, 7 },    /* y' = x + y */
        { {0.5f,0,0,0.5f,0,0}, 10, 20, 5, 10 },
        { {1,0,0,1,-100.4f,0}, 0, 0, -100, 0 },
        { {1,0,0,1,100.6f,0},  0, 0, 101, 0 },
        { {3,0,0,3,1,1},     -2, -3, -5, -8 },
        { {0,2,-2,0,5,5},    1, 1, 3, 7 },
    };
    for (int i = 0; i < (int)(sizeof t / sizeof t[0]); i++) {
        HDC dc = adv_dc();
        XFORM x = xf(t[i].m[0], t[i].m[1], t[i].m[2], t[i].m[3], t[i].m[4], t[i].m[5]);
        CHECK(U(SetWorldTransform)(dc, &x));
        POINT p = { t[i].lx, t[i].ly };
        CHECK(U(LPtoDP)(dc, &p, 1));
        CHECKF(p.x == t[i].dx && p.y == t[i].dy, "row %d got %ld,%ld want %d,%d", i, p.x, p.y, t[i].dx, t[i].dy);
        mt_free_dc(dc);
    }
}

TEST(mapping_wxf_dptolp_inverse)
{
    REQUIRE(U(DPtoLP)); REQUIRE(U(SetWorldTransform));
    static const struct { float m[6]; int lx, ly; } t[] = {
        { {1,0,0,1,10,20}, 5, 5 }, { {2,0,0,2,0,0}, 7, -9 }, { {0,1,-1,0,3,4}, 11, 13 }, { {-1,0,0,1,0,0}, 6, 6 },
        { {1,0,1,1,0,0}, 3, 4 }, { {0.5f,0,0,0.5f,0,0}, 20, 40 }, { {1,2,0,1,5,5}, 3, 8 },
    };
    for (int i = 0; i < (int)(sizeof t / sizeof t[0]); i++) {
        HDC dc = adv_dc();
        XFORM x = xf(t[i].m[0], t[i].m[1], t[i].m[2], t[i].m[3], t[i].m[4], t[i].m[5]);
        U(SetWorldTransform)(dc, &x);
        POINT p = { t[i].lx, t[i].ly };
        U(LPtoDP)(dc, &p, 1);
        BOOL r = U(DPtoLP)(dc, &p, 1);
        CHECKF(r && p.x == t[i].lx && p.y == t[i].ly, "row %d roundtrip got %ld,%ld want %d,%d", i, p.x, p.y, t[i].lx, t[i].ly);
        mt_free_dc(dc);
    }
}

TEST(mapping_wxf_combined_with_page_transform)
{
    REQUIRE(U(LPtoDP)); REQUIRE(U(SetWorldTransform));
    HDC dc = adv_dc();
    XFORM x = xf(2, 0, 0, 2, 0, 0);
    U(SetWorldTransform)(dc, &x);
    U(SetViewportOrgEx)(dc, 100, 200, NULL);
    LP_IS(dc, 10, 10, 120, 220);
    U(SetWindowOrgEx)(dc, 5, 5, NULL);   /* world result is page space; window org subtracted afterwards */
    LP_IS(dc, 10, 10, 115, 215);
    U(SetMapMode)(dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(dc, 1, 1, NULL);
    U(SetViewportExtEx)(dc, 3, 3, NULL);
    LP_IS(dc, 10, 10, 145, 245);
    DP_IS(dc, 145, 245, 10, 10);
    mt_free_dc(dc);
}

TEST(mapping_wxf_not_applied_to_text_origin_semantics)
{
    REQUIRE(U(LPtoDP)); REQUIRE(U(SetWorldTransform));
    HDC dc = adv_dc();
    XFORM x = xf(1, 0, 0, 1, 7, 9);
    U(SetWorldTransform)(dc, &x);
    LP_IS(dc, 0, 0, 7, 9);
    ORG_IS(dc, GetWindowOrgEx, 0, 0);
    ORG_IS(dc, GetViewportOrgEx, 0, 0);
    EXT_IS(dc, GetWindowExtEx, 1, 1);
    mt_free_dc(dc);
}

TEST(mapping_wxf_compat_mode_still_applies)
{
    REQUIRE(U(LPtoDP)); REQUIRE(U(SetWorldTransform));
    HDC dc = adv_dc();
    XFORM x = xf(2, 0, 0, 2, 0, 0);
    U(SetWorldTransform)(dc, &x);
    MT_EQ(U(SetGraphicsMode)(dc, GM_COMPATIBLE), GM_ADVANCED);
    LP_IS(dc, 5, 5, 10, 10);
    mt_free_dc(dc);
}

TEST(mapping_wxf_array_conversion)
{
    REQUIRE(U(LPtoDP)); REQUIRE(U(SetWorldTransform));
    HDC dc = adv_dc();
    XFORM x = xf(0, 1, -1, 0, 100, 0);
    U(SetWorldTransform)(dc, &x);
    POINT p[4] = { {0,0}, {10,0}, {10,10}, {0,10} };
    CHECK(U(LPtoDP)(dc, p, 4));
    CHECK(p[0].x == 100 && p[0].y == 0);
    CHECK(p[1].x == 100 && p[1].y == 10);
    CHECK(p[2].x == 90 && p[2].y == 10);
    CHECK(p[3].x == 90 && p[3].y == 0);
    mt_free_dc(dc);
}

TEST(mapping_wxf_large_values_rounding)
{
    REQUIRE(U(LPtoDP)); REQUIRE(U(SetWorldTransform));
    HDC dc = adv_dc();
    XFORM x = xf(1, 0, 0, 1, 0.4f, -0.4f);
    U(SetWorldTransform)(dc, &x);
    LP_IS(dc, 5, 5, 5, 5);
    x = xf(1, 0, 0, 1, 0.6f, -0.6f);
    U(SetWorldTransform)(dc, &x);
    LP_IS(dc, 5, 5, 6, 4);
    x = xf(1000.0f, 0, 0, 1000.0f, 0, 0);
    U(SetWorldTransform)(dc, &x);
    LP_IS(dc, 1000, -1000, 1000000, -1000000);
    mt_free_dc(dc);
}

/* ---------------- SaveDC / RestoreDC ---------------- */
TEST(mapping_savedc_restores_mapping)
{
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = mt_dc();
    U(SetMapMode)(dc, MM_ANISOTROPIC);
    U(SetWindowOrgEx)(dc, 1, 2, NULL); U(SetViewportOrgEx)(dc, 3, 4, NULL);
    U(SetWindowExtEx)(dc, 5, 6, NULL); U(SetViewportExtEx)(dc, 7, 8, NULL);
    int lvl = U(SaveDC)(dc);
    CHECK(lvl >= 1);
    U(SetMapMode)(dc, MM_TWIPS);
    U(SetWindowOrgEx)(dc, 11, 12, NULL); U(SetViewportOrgEx)(dc, 13, 14, NULL);
    CHECK(U(RestoreDC)(dc, -1));
    MT_EQ(U(GetMapMode)(dc), MM_ANISOTROPIC);
    ORG_IS(dc, GetWindowOrgEx, 1, 2);
    ORG_IS(dc, GetViewportOrgEx, 3, 4);
    EXT_IS(dc, GetWindowExtEx, 5, 6);
    EXT_IS(dc, GetViewportExtEx, 7, 8);
    mt_free_dc(dc);
}

TEST(mapping_savedc_restores_extents_after_change)
{
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = mt_dc();
    U(SetMapMode)(dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(dc, 5, 6, NULL); U(SetViewportExtEx)(dc, 7, 8, NULL);
    int lvl = U(SaveDC)(dc);
    U(SetWindowExtEx)(dc, 50, 60, NULL); U(SetViewportExtEx)(dc, 70, 80, NULL);
    EXT_IS(dc, GetWindowExtEx, 50, 60);
    CHECK(U(RestoreDC)(dc, lvl));
    EXT_IS(dc, GetWindowExtEx, 5, 6);
    EXT_IS(dc, GetViewportExtEx, 7, 8);
    mt_free_dc(dc);
}

TEST(mapping_savedc_restores_world_and_gfxmode)
{
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = adv_dc();
    XFORM a = xf(2, 0, 0, 2, 1, 1), b = xf(3, 0, 0, 3, 2, 2), g;
    U(SetWorldTransform)(dc, &a);
    int lvl = U(SaveDC)(dc);
    U(SetWorldTransform)(dc, &b);
    U(SetGraphicsMode)(dc, GM_COMPATIBLE);
    CHECK(U(RestoreDC)(dc, lvl));
    MT_EQ(U(GetGraphicsMode)(dc), GM_ADVANCED);
    U(GetWorldTransform)(dc, &g); XF_IS(g, 2, 0, 0, 2, 1, 1);
    mt_free_dc(dc);
}

TEST(mapping_savedc_nested_levels)
{
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = mt_dc();
    int l1 = U(SaveDC)(dc);
    U(SetMapMode)(dc, MM_LOMETRIC);
    int l2 = U(SaveDC)(dc);
    U(SetMapMode)(dc, MM_HIMETRIC);
    int l3 = U(SaveDC)(dc);
    U(SetMapMode)(dc, MM_TWIPS);
    CHECKF(l1 == 1 && l2 == 2 && l3 == 3, "levels %d %d %d", l1, l2, l3);
    CHECK(U(RestoreDC)(dc, -1)); MT_EQ(U(GetMapMode)(dc), MM_HIMETRIC);
    CHECK(U(RestoreDC)(dc, -1)); MT_EQ(U(GetMapMode)(dc), MM_LOMETRIC);
    CHECK(U(RestoreDC)(dc, -1)); MT_EQ(U(GetMapMode)(dc), MM_TEXT);
    mt_free_dc(dc);
}

TEST(mapping_savedc_restore_absolute_level_pops_all_above)
{
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = mt_dc();
    int l1 = U(SaveDC)(dc);
    U(SetMapMode)(dc, MM_LOMETRIC);
    U(SaveDC)(dc);
    U(SetMapMode)(dc, MM_HIMETRIC);
    U(SaveDC)(dc);
    U(SetMapMode)(dc, MM_TWIPS);
    CHECK(U(RestoreDC)(dc, l1));
    MT_EQ(U(GetMapMode)(dc), MM_TEXT);
    SetLastError(0xdeadbeef);
    CHECK(!U(RestoreDC)(dc, l1));    /* already popped */
    mt_free_dc(dc);
}

TEST(mapping_savedc_restore_invalid_levels)
{
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = mt_dc();
    U(SetMapMode)(dc, MM_TWIPS);
    CHECK(!U(RestoreDC)(dc, -1));
    CHECK(!U(RestoreDC)(dc, 1));
    CHECK(!U(RestoreDC)(dc, 0));
    CHECK(!U(RestoreDC)(dc, 99));
    BOOL rr = U(RestoreDC)(dc, INT_MIN);
    t_note("RestoreDC(INT_MIN) ret %d", rr);
    MT_EQ(U(GetMapMode)(dc), MM_TWIPS);
    int l = U(SaveDC)(dc);
    CHECK(!U(RestoreDC)(dc, l + 1));
    CHECK(!U(RestoreDC)(dc, -2));
    MT_EQ(U(GetMapMode)(dc), MM_TWIPS);
    CHECK(U(RestoreDC)(dc, l));
    mt_free_dc(dc);
}

TEST(mapping_savedc_does_not_restore_current_position_semantics)
{
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    HDC dc = mt_dc();
    U(MoveToEx)(dc, 3, 4, NULL);
    int l = U(SaveDC)(dc);
    U(MoveToEx)(dc, 30, 40, NULL);
    U(RestoreDC)(dc, l);
    POINT p; CHECK(U(GetCurrentPositionEx)(dc, &p));
    t_note("current pos after RestoreDC: %ld,%ld", p.x, p.y);
    mt_free_dc(dc);
}

/* ---------------- current position ---------------- */
TEST(mapping_curpos_is_logical)
{
    REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    HDC dc = mt_dc();
    POINT p = {0x7777, 0x7777};
    CHECK(U(GetCurrentPositionEx)(dc, &p)); CHECK(p.x == 0 && p.y == 0);
    CHECK(U(MoveToEx)(dc, 10, 20, &p)); CHECK(p.x == 0 && p.y == 0);
    CHECK(U(MoveToEx)(dc, 30, 40, &p)); CHECK(p.x == 10 && p.y == 20);
    U(SetViewportOrgEx)(dc, 100, 100, NULL);
    U(SetMapMode)(dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(dc, 1, 1, NULL); U(SetViewportExtEx)(dc, 4, 4, NULL);
    CHECK(U(MoveToEx)(dc, 7, 9, NULL));
    CHECK(U(GetCurrentPositionEx)(dc, &p)); CHECK(p.x == 7 && p.y == 9);
    mt_free_dc(dc);
}

TEST(mapping_curpos_with_flipped_axes)
{
    REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    HDC dc = mt_dc();
    U(SetMapMode)(dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(dc, 1, 1, NULL); U(SetViewportExtEx)(dc, -1, -1, NULL);
    U(SetViewportOrgEx)(dc, 50, 50, NULL);
    POINT p;
    U(MoveToEx)(dc, 10, 20, NULL);
    CHECK(U(GetCurrentPositionEx)(dc, &p)); CHECK(p.x == 10 && p.y == 20);
    mt_free_dc(dc);
}

TEST(mapping_curpos_with_world_transform)
{
    REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    HDC dc = adv_dc();
    XFORM x = xf(0, 1, -1, 0, 0, 0);
    U(SetWorldTransform)(dc, &x);
    POINT p;
    U(MoveToEx)(dc, 10, 0, NULL);
    CHECK(U(GetCurrentPositionEx)(dc, &p));
    CHECKF(p.x == 10 && p.y == 0, "got %ld,%ld", p.x, p.y);
    mt_free_dc(dc);
}

TEST(mapping_curpos_after_mapping_change_observed)
{
    REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    HDC dc = mt_dc();
    U(MoveToEx)(dc, 10, 20, NULL);
    U(SetViewportOrgEx)(dc, 50, 60, NULL);
    POINT p; U(GetCurrentPositionEx)(dc, &p);
    t_note("current pos after viewport org change: %ld,%ld", p.x, p.y);
    U(SetMapMode)(dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(dc, 1, 1, NULL); U(SetViewportExtEx)(dc, 2, 2, NULL);
    U(GetCurrentPositionEx)(dc, &p);
    t_note("current pos after scale change: %ld,%ld", p.x, p.y);
    mt_free_dc(dc);
}

TEST(mapping_curpos_invalid_dc)
{
    REQUIRE(U(GetCurrentPositionEx)); REQUIRE(U(MoveToEx));
    HDC dead = mt_dc(); mt_free_dc(dead);
    HDC bad[] = { NULL, (HDC)1, (HDC)0xdeadbeef, dead };
    for (int i = 0; i < 4; i++) {
        POINT p = {0x7777, 0x7777};
        SetLastError(0xdeadbeef);
        CHECKF(!U(GetCurrentPositionEx)(bad[i], &p), "get idx %d", i);
        EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
        CHECKF(!U(MoveToEx)(bad[i], 1, 1, &p), "move idx %d", i);
    }
}

/* ---------------- layout ---------------- */
TEST(mapping_layout_default_and_set)
{
    REQUIRE(U(SetLayout)); REQUIRE(U(GetLayout));
    mt_dib d;
    if (!mt_dib_make(&d, 100, 50)) { t_skip("dib"); return; }
    MT_EQ(U(GetLayout)(d.dc), 0);
    MT_EQ(U(SetLayout)(d.dc, LAYOUT_RTL), 0);
    MT_EQ(U(GetLayout)(d.dc), LAYOUT_RTL);
    MT_EQ(U(SetLayout)(d.dc, LAYOUT_RTL), LAYOUT_RTL);
    MT_EQ(U(SetLayout)(d.dc, 0), LAYOUT_RTL);
    MT_EQ(U(GetLayout)(d.dc), 0);
    mt_dib_free(&d);
}

TEST(mapping_layout_rtl_mirrors_lptodp)
{
    REQUIRE(U(SetLayout)); REQUIRE(U(LPtoDP)); REQUIRE(U(DPtoLP));
    mt_dib d;
    if (!mt_dib_make(&d, 100, 50)) { t_skip("dib"); return; }
    U(SetLayout)(d.dc, LAYOUT_RTL);
    LP_IS(d.dc, 0, 0, 99, 0);
    LP_IS(d.dc, 10, 5, 89, 5);
    LP_IS(d.dc, 99, 49, 0, 49);
    LP_IS(d.dc, -1, 0, 100, 0);
    DP_IS(d.dc, 89, 5, 10, 5);
    DP_IS(d.dc, 0, 0, 99, 0);
    U(SetLayout)(d.dc, 0);
    LP_IS(d.dc, 10, 5, 10, 5);
    mt_dib_free(&d);
}

TEST(mapping_layout_rtl_leaves_transform_getters)
{
    REQUIRE(U(SetLayout));
    mt_dib d;
    if (!mt_dib_make(&d, 100, 50)) { t_skip("dib"); return; }
    U(SetLayout)(d.dc, LAYOUT_RTL);
    ORG_IS(d.dc, GetViewportOrgEx, 0, 0);
    ORG_IS(d.dc, GetWindowOrgEx, 0, 0);
    XFORM g; CHECK(U(GetWorldTransform)(d.dc, &g)); XF_IS(g, 1, 0, 0, 1, 0, 0);
    t_note("mapmode under RTL = %d", U(GetMapMode)(d.dc));
    EXT_IS(d.dc, GetWindowExtEx, 1, 1);
    EXT_IS(d.dc, GetViewportExtEx, 1, 1);
    U(SetViewportOrgEx)(d.dc, 10, 5, NULL);
    ORG_IS(d.dc, GetViewportOrgEx, 10, 5);
    mt_dib_free(&d);
}

TEST(mapping_layout_rtl_with_origin_observed)
{
    REQUIRE(U(SetLayout)); REQUIRE(U(LPtoDP));
    mt_dib d;
    if (!mt_dib_make(&d, 100, 50)) { t_skip("dib"); return; }
    U(SetLayout)(d.dc, LAYOUT_RTL);
    U(SetViewportOrgEx)(d.dc, 10, 0, NULL);
    POINT p = { 0, 0 }; U(LPtoDP)(d.dc, &p, 1);
    t_note("RTL viewport org 10: LPtoDP(0,0) = %ld,%ld", p.x, p.y);
    U(SetViewportOrgEx)(d.dc, 0, 0, NULL);
    U(SetWindowOrgEx)(d.dc, 10, 0, NULL);
    p.x = p.y = 0; U(LPtoDP)(d.dc, &p, 1);
    t_note("RTL window org 10: LPtoDP(0,0) = %ld,%ld", p.x, p.y);
    mt_dib_free(&d);
}

TEST(mapping_layout_flags_roundtrip)
{
    REQUIRE(U(SetLayout)); REQUIRE(U(GetLayout));
    mt_dib d;
    if (!mt_dib_make(&d, 100, 50)) { t_skip("dib"); return; }
    DWORD r = U(SetLayout)(d.dc, LAYOUT_RTL | LAYOUT_BITMAPORIENTATIONPRESERVED);
    CHECKF(r == 0, "prev %lx", r);
    t_note("GetLayout after RTL|BPO = 0x%lx", U(GetLayout)(d.dc));
    r = U(SetLayout)(d.dc, LAYOUT_BITMAPORIENTATIONPRESERVED);
    t_note("SetLayout(BPO) prev 0x%lx now 0x%lx", r, U(GetLayout)(d.dc));
    U(SetLayout)(d.dc, 0);
    r = U(SetLayout)(d.dc, 0x100);
    t_note("SetLayout(0x100) ret 0x%lx now 0x%lx", r, U(GetLayout)(d.dc));
    mt_dib_free(&d);
}

TEST(mapping_layout_bpo_disables_mirroring)
{
    REQUIRE(U(SetLayout)); REQUIRE(U(LPtoDP));
    mt_dib d;
    if (!mt_dib_make(&d, 100, 50)) { t_skip("dib"); return; }
    U(SetLayout)(d.dc, LAYOUT_RTL | LAYOUT_BITMAPORIENTATIONPRESERVED);
    POINT p = { 10, 5 }; U(LPtoDP)(d.dc, &p, 1);
    t_note("RTL|BPO LPtoDP(10,5) = %ld,%ld (mirrored would be 89,5)", p.x, p.y);
    mt_dib_free(&d);
}

TEST(mapping_layout_invalid_dc)
{
    REQUIRE(U(SetLayout)); REQUIRE(U(GetLayout));
    HDC dead = mt_dc(); mt_free_dc(dead);
    HDC bad[] = { NULL, (HDC)1, (HDC)0xdeadbeef, dead };
    for (int i = 0; i < 4; i++) {
        SetLastError(0xdeadbeef);
        DWORD r = U(SetLayout)(bad[i], LAYOUT_RTL);
        CHECKF(r == GDI_ERROR, "SetLayout idx %d = %lx", i, r);
        EXPECT_ERR(0, bad[i] == dead ? ERROR_INVALID_PARAMETER : ERROR_INVALID_HANDLE);
        SetLastError(0xdeadbeef);
        r = U(GetLayout)(bad[i]);
        t_note("GetLayout idx %d = 0x%lx", i, r);
        EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    }
}

TEST(mapping_layout_saved_by_savedc)
{
    REQUIRE(U(SetLayout)); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    mt_dib d;
    if (!mt_dib_make(&d, 100, 50)) { t_skip("dib"); return; }
    int l = U(SaveDC)(d.dc);
    U(SetLayout)(d.dc, LAYOUT_RTL);
    U(RestoreDC)(d.dc, l);
    DWORD g = U(GetLayout)(d.dc);
    t_note("layout after RestoreDC: %lu", g);
    CHECK(g == 0);
    mt_dib_free(&d);
}

TEST(mapping_layout_rtl_double_toggle_identity)
{
    REQUIRE(U(SetLayout)); REQUIRE(U(LPtoDP));
    mt_dib d;
    if (!mt_dib_make(&d, 64, 64)) { t_skip("dib"); return; }
    for (int i = 0; i < 10; i++) {
        U(SetLayout)(d.dc, LAYOUT_RTL);
        U(SetLayout)(d.dc, 0);
    }
    LP_IS(d.dc, 17, 9, 17, 9);
    mt_dib_free(&d);
}
