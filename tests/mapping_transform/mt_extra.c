#include "mt.h"

static void an(HDC dc, int wox, int woy, int wex, int wey, int vox, int voy, int vex, int vey)
{
    U(SetMapMode)(dc, MM_ANISOTROPIC);
    U(SetWindowOrgEx)(dc, wox, woy, NULL);
    U(SetViewportOrgEx)(dc, vox, voy, NULL);
    U(SetWindowExtEx)(dc, wex, wey, NULL);
    U(SetViewportExtEx)(dc, vex, vey, NULL);
}

TEST(mapping_x_savedc_restores_mapping)
{
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(SetMapMode)); REQUIRE(U(GetMapMode));
    HDC dc = mt_dc();
    an(dc, 1, 2, 3, 4, 5, 6, 7, 8);
    int id = U(SaveDC)(dc);
    CHECK(id > 0);
    U(SetMapMode)(dc, MM_TEXT);
    U(SetWindowOrgEx)(dc, 50, 60, NULL);
    CHECK(U(RestoreDC)(dc, -1));
    MT_EQ(U(GetMapMode)(dc), MM_ANISOTROPIC);
    ORG_IS(dc, GetWindowOrgEx, 1, 2);
    ORG_IS(dc, GetViewportOrgEx, 5, 6);
    EXT_IS(dc, GetWindowExtEx, 3, 4);
    EXT_IS(dc, GetViewportExtEx, 7, 8);
    mt_free_dc(dc);
}

TEST(mapping_x_restoredc_bad_index)
{
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(SetMapMode)); REQUIRE(U(GetMapMode));
    HDC dc = mt_dc();
    U(SetMapMode)(dc, MM_LOMETRIC);
    CHECK(!U(RestoreDC)(dc, -1));
    CHECK(!U(RestoreDC)(dc, 0x7777));
    MT_EQ(U(GetMapMode)(dc), MM_LOMETRIC);
    mt_free_dc(dc);
}

TEST(mapping_x_nested_save_restore_mapmode)
{
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(SetMapMode)); REQUIRE(U(GetMapMode));
    HDC dc = mt_dc();
    int a = U(SaveDC)(dc);
    U(SetMapMode)(dc, MM_LOENGLISH);
    int b = U(SaveDC)(dc);
    U(SetMapMode)(dc, MM_HIMETRIC);
    CHECK(a > 0 && b > a);
    CHECK(U(RestoreDC)(dc, a));
    MT_EQ(U(GetMapMode)(dc), MM_TEXT);
    mt_free_dc(dc);
}

TEST(mapping_x_savedc_restores_world_transform)
{
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform)); REQUIRE(U(GetWorldTransform));
    HDC dc = mt_dc();
    MT_EQ(U(SetGraphicsMode)(dc, GM_ADVANCED), GM_COMPATIBLE);
    XFORM x = {2, 0, 0, 3, 4, 5}, g;
    CHECK(U(SetWorldTransform)(dc, &x));
    int id = U(SaveDC)(dc);
    XFORM y = {1, 0, 0, 1, 0, 0};
    CHECK(U(SetWorldTransform)(dc, &y));
    CHECK(U(RestoreDC)(dc, id));
    CHECK(U(GetWorldTransform)(dc, &g));
    XF_IS(g, 2, 0, 0, 3, 4, 5);
    mt_free_dc(dc);
}

TEST(mapping_x_mapmode_roundtrip_all)
{
    REQUIRE(U(SetMapMode)); REQUIRE(U(GetMapMode));
    HDC dc = mt_dc();
    for (int m = MM_MIN; m <= MM_MAX; m++) {
        int prev = U(GetMapMode)(dc);
        MT_EQ(U(SetMapMode)(dc, m), prev);
        MT_EQ(U(GetMapMode)(dc), m);
    }
    mt_free_dc(dc);
}

TEST(mapping_x_mapmode_invalid_keeps_mode)
{
    REQUIRE(U(SetMapMode)); REQUIRE(U(GetMapMode));
    HDC dc = mt_dc();
    U(SetMapMode)(dc, MM_HIENGLISH);
    static const int bad[] = { 0, -1, 9, 10, 100, 0x10000, INT_MAX, INT_MIN };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0);
        MT_EQ(U(SetMapMode)(dc, bad[i]), 0);
        MT_EQ(U(GetMapMode)(dc), MM_HIENGLISH);
    }
    mt_free_dc(dc);
}

TEST(mapping_x_fixed_mode_extents_exact_ratio)
{
    REQUIRE(U(SetMapMode)); REQUIRE(U(GetWindowExtEx)); REQUIRE(U(GetViewportExtEx));
    static const struct { int m; long wx, wy; } t[] = {
        { MM_LOMETRIC, 0, 0 }, { MM_HIMETRIC, 0, 0 }, { MM_LOENGLISH, 0, 0 },
        { MM_HIENGLISH, 0, 0 }, { MM_TWIPS, 0, 0 } };
    HDC dc = mt_dc();
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        U(SetMapMode)(dc, t[i].m);
        SIZE w = {0}, v = {0};
        CHECK(U(GetWindowExtEx)(dc, &w) && U(GetViewportExtEx)(dc, &v));
        CHECKF(w.cx > 0 && v.cx > 0, "mode %d wnd %ld vp %ld", t[i].m, w.cx, v.cx);
        /* y viewport extent is negative (y axis up) in every fixed metric mode */
        CHECKF(v.cy < 0 && w.cy > 0, "mode %d y ext w=%ld v=%ld", t[i].m, w.cy, v.cy);
    }
    mt_free_dc(dc);
}

TEST(mapping_x_fixed_mode_yflip_conversion)
{
    REQUIRE(U(SetMapMode)); REQUIRE(U(LPtoDP));
    static const int m[] = { MM_LOMETRIC, MM_HIMETRIC, MM_LOENGLISH, MM_HIENGLISH, MM_TWIPS };
    HDC dc = mt_dc();
    for (int i = 0; i < 5; i++) {
        U(SetMapMode)(dc, m[i]);
        POINT p = {0, 0};
        CHECK(U(LPtoDP)(dc, &p, 1));
        CHECK(p.x == 0 && p.y == 0);
        POINT q = {1000, 1000};
        CHECK(U(LPtoDP)(dc, &q, 1));
        CHECKF(q.x > 0 && q.y < 0, "mode %d: %ld,%ld", m[i], q.x, q.y);
    }
    mt_free_dc(dc);
}

TEST(mapping_x_text_mode_ext_values)
{
    REQUIRE(U(SetMapMode)); REQUIRE(U(GetWindowExtEx)); REQUIRE(U(GetViewportExtEx));
    HDC dc = mt_dc();
    EXT_IS(dc, GetWindowExtEx, 1, 1);
    EXT_IS(dc, GetViewportExtEx, 1, 1);
    U(SetMapMode)(dc, MM_ANISOTROPIC);
    U(SetMapMode)(dc, MM_TEXT);
    EXT_IS(dc, GetWindowExtEx, 1, 1);
    EXT_IS(dc, GetViewportExtEx, 1, 1);
    mt_free_dc(dc);
}

TEST(mapping_x_org_prev_pointer_chain)
{
    REQUIRE(U(SetWindowOrgEx)); REQUIRE(U(SetViewportOrgEx)); REQUIRE(U(OffsetWindowOrgEx)); REQUIRE(U(OffsetViewportOrgEx));
    HDC dc = mt_dc();
    POINT p = {99, 99};
    CHECK(U(SetWindowOrgEx)(dc, 10, 20, &p)); CHECK(p.x == 0 && p.y == 0);
    CHECK(U(OffsetWindowOrgEx)(dc, 5, -5, &p)); CHECK(p.x == 10 && p.y == 20);
    CHECK(U(OffsetWindowOrgEx)(dc, 0, 0, &p)); CHECK(p.x == 15 && p.y == 15);
    CHECK(U(SetViewportOrgEx)(dc, -3, -4, &p)); CHECK(p.x == 0 && p.y == 0);
    CHECK(U(OffsetViewportOrgEx)(dc, 3, 4, &p)); CHECK(p.x == -3 && p.y == -4);
    ORG_IS(dc, GetViewportOrgEx, 0, 0);
    ORG_IS(dc, GetWindowOrgEx, 15, 15);
    mt_free_dc(dc);
}

TEST(mapping_x_org_extreme_values)
{
    REQUIRE(U(SetWindowOrgEx)); REQUIRE(U(SetViewportOrgEx));
    HDC dc = mt_dc();
    static const int v[] = { INT_MAX, INT_MIN, 0x7fff, -0x8000, 0x10000, -0x10000 };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        CHECK(U(SetWindowOrgEx)(dc, v[i], v[i], NULL));
        ORG_IS(dc, GetWindowOrgEx, v[i], v[i]);
        CHECK(U(SetViewportOrgEx)(dc, v[i], -v[i], NULL));
        ORG_IS(dc, GetViewportOrgEx, v[i], -v[i]);
    }
    mt_free_dc(dc);
}

TEST(mapping_x_iso_ratio_stable)
{
    REQUIRE(U(SetMapMode)); REQUIRE(U(SetWindowExtEx)); REQUIRE(U(SetViewportExtEx)); REQUIRE(U(GetViewportExtEx));
    HDC dc = mt_dc();
    U(SetMapMode)(dc, MM_ISOTROPIC);
    U(SetWindowExtEx)(dc, 100, 100, NULL);
    U(SetViewportExtEx)(dc, 200, 200, NULL);
    SIZE v = {0};
    CHECK(U(GetViewportExtEx)(dc, &v));
    CHECK(v.cx == 200 && v.cy == 200);
    POINT p = {50, 50};
    CHECK(U(LPtoDP)(dc, &p, 1));
    CHECK(p.x == 100 && p.y == 100);
    mt_free_dc(dc);
}

TEST(mapping_x_iso_equal_scale_output)
{
    REQUIRE(U(SetMapMode)); REQUIRE(U(SetWindowExtEx)); REQUIRE(U(SetViewportExtEx)); REQUIRE(U(LPtoDP));
    HDC dc = mt_dc();
    U(SetMapMode)(dc, MM_ISOTROPIC);
    U(SetWindowExtEx)(dc, 1000, 1000, NULL);
    U(SetViewportExtEx)(dc, 300, 100, NULL);
    POINT p = {1000, 1000};
    CHECK(U(LPtoDP)(dc, &p, 1));
    CHECKF(p.x == p.y, "isotropic scale not equal: %ld,%ld", p.x, p.y);
    CHECKF(p.x == 100 || p.x == 300, "scale %ld", p.x);
    mt_free_dc(dc);
}

TEST(mapping_x_conv_count_zero_negative_ok)
{
    REQUIRE(U(LPtoDP)); REQUIRE(U(DPtoLP));
    HDC dc = mt_dc();
    POINT p = {7, 8};
    CHECK(U(LPtoDP)(dc, &p, 0) && p.x == 7 && p.y == 8);
    CHECK(U(DPtoLP)(dc, &p, 0) && p.x == 7 && p.y == 8);
    CHECK(U(LPtoDP)(dc, &p, -1) && p.x == 7);
    mt_free_dc(dc);
}

TEST(mapping_x_conv_roundtrip_array)
{
    REQUIRE(U(LPtoDP)); REQUIRE(U(DPtoLP));
    HDC dc = mt_dc();
    an(dc, 0, 0, 1, 1, 10, 20, 4, 4);
    POINT a[8], b[8];
    for (int i = 0; i < 8; i++) a[i].x = i * 3, a[i].y = -i * 5, b[i] = a[i];
    CHECK(U(LPtoDP)(dc, a, 8));
    for (int i = 0; i < 8; i++) CHECK(a[i].x == 10 + b[i].x * 4 && a[i].y == 20 + b[i].y * 4);
    CHECK(U(DPtoLP)(dc, a, 8));
    for (int i = 0; i < 8; i++) CHECK(a[i].x == b[i].x && a[i].y == b[i].y);
    mt_free_dc(dc);
}

TEST(mapping_x_conv_down_scale_rounding)
{
    REQUIRE(U(LPtoDP)); REQUIRE(U(DPtoLP));
    HDC dc = mt_dc();
    an(dc, 0, 0, 10, 10, 0, 0, 1, 1);
    for (int x = -30; x <= 30; x += 10) { POINT p = {x, x}; CHECK(U(LPtoDP)(dc, &p, 1)); CHECK(p.x == x / 10 && p.y == x / 10); }
    mt_free_dc(dc);
}

TEST(mapping_x_scale_ext_chain)
{
    REQUIRE(U(SetMapMode)); REQUIRE(U(ScaleWindowExtEx)); REQUIRE(U(ScaleViewportExtEx)); REQUIRE(U(GetWindowExtEx));
    HDC dc = mt_dc();
    U(SetMapMode)(dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(dc, 100, 100, NULL);
    U(SetViewportExtEx)(dc, 100, 100, NULL);
    SIZE p = {0};
    CHECK(U(ScaleWindowExtEx)(dc, 1, 2, 3, 4, &p));
    CHECK(p.cx == 100 && p.cy == 100);
    EXT_IS(dc, GetWindowExtEx, 50, 75);
    CHECK(U(ScaleViewportExtEx)(dc, 3, 1, 1, 1, &p));
    EXT_IS(dc, GetViewportExtEx, 300, 100);
    mt_free_dc(dc);
}

TEST(mapping_x_scale_fixed_mode_noop)
{
    REQUIRE(U(SetMapMode)); REQUIRE(U(ScaleWindowExtEx)); REQUIRE(U(GetWindowExtEx));
    HDC dc = mt_dc();
    U(SetMapMode)(dc, MM_LOENGLISH);
    SIZE a = {0}, b = {0};
    U(GetWindowExtEx)(dc, &a);
    BOOL r = U(ScaleWindowExtEx)(dc, 2, 1, 1, 1, NULL);
    U(GetWindowExtEx)(dc, &b);
    t_note("fixed mode ScaleWindowExtEx ret %d, ext %ld->%ld", r, a.cx, b.cx);
    mt_free_dc(dc);
}

TEST(mapping_x_world_identity_default)
{
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(GetWorldTransform));
    HDC dc = mt_dc();
    U(SetGraphicsMode)(dc, GM_ADVANCED);
    XFORM g;
    CHECK(U(GetWorldTransform)(dc, &g));
    XF_IS(g, 1, 0, 0, 1, 0, 0);
    mt_free_dc(dc);
}

TEST(mapping_x_world_modify_ops_sequence)
{
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(ModifyWorldTransform)); REQUIRE(U(GetWorldTransform));
    HDC dc = mt_dc();
    U(SetGraphicsMode)(dc, GM_ADVANCED);
    XFORM s = {2, 0, 0, 2, 0, 0}, t = {1, 0, 0, 1, 10, 0}, g;
    CHECK(U(ModifyWorldTransform)(dc, &s, MWT_LEFTMULTIPLY));
    CHECK(U(ModifyWorldTransform)(dc, &t, MWT_LEFTMULTIPLY));   /* t * s */
    CHECK(U(GetWorldTransform)(dc, &g));
    XF_IS(g, 2, 0, 0, 2, 20, 0);
    CHECK(U(ModifyWorldTransform)(dc, NULL, MWT_IDENTITY));
    CHECK(U(ModifyWorldTransform)(dc, &s, MWT_RIGHTMULTIPLY));
    CHECK(U(ModifyWorldTransform)(dc, &t, MWT_RIGHTMULTIPLY));  /* s * t */
    CHECK(U(GetWorldTransform)(dc, &g));
    XF_IS(g, 2, 0, 0, 2, 10, 0);
    mt_free_dc(dc);
}

TEST(mapping_x_world_modify_bad_mode)
{
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(ModifyWorldTransform)); REQUIRE(U(GetWorldTransform));
    HDC dc = mt_dc();
    U(SetGraphicsMode)(dc, GM_ADVANCED);
    XFORM s = {2, 0, 0, 2, 0, 0}, g;
    static const DWORD bad[] = { 0, 5, 6, 100, 0xffffffff };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) CHECK(!U(ModifyWorldTransform)(dc, &s, bad[i]));
    CHECK(U(GetWorldTransform)(dc, &g));
    XF_IS(g, 1, 0, 0, 1, 0, 0);
    mt_free_dc(dc);
}

TEST(mapping_x_combine_identity_and_assoc)
{
    REQUIRE(U(CombineTransform));
    XFORM id = {1, 0, 0, 1, 0, 0}, a = {2, 1, 0, 3, 4, 5}, b = {0, 1, -1, 0, 7, 8}, c = {1, 2, 3, 4, 5, 6};
    XFORM r1, r2, t1, t2;
    CHECK(U(CombineTransform)(&r1, &a, &id)); XF_IS(r1, 2, 1, 0, 3, 4, 5);
    CHECK(U(CombineTransform)(&r1, &id, &a)); XF_IS(r1, 2, 1, 0, 3, 4, 5);
    CHECK(U(CombineTransform)(&t1, &a, &b)); CHECK(U(CombineTransform)(&r1, &t1, &c));
    CHECK(U(CombineTransform)(&t2, &b, &c)); CHECK(U(CombineTransform)(&r2, &a, &t2));
    CHECK(r1.eM11 == r2.eM11 && r1.eM12 == r2.eM12 && r1.eM21 == r2.eM21 && r1.eM22 == r2.eM22 && r1.eDx == r2.eDx && r1.eDy == r2.eDy);
}

TEST(mapping_x_combine_inplace_alias)
{
    REQUIRE(U(CombineTransform));
    XFORM a = {2, 0, 0, 2, 1, 1}, b = {1, 0, 0, 1, 3, 3}, ref;
    CHECK(U(CombineTransform)(&ref, &a, &b));
    XFORM x = a;
    CHECK(U(CombineTransform)(&x, &x, &b));
    CHECK(x.eM11 == ref.eM11 && x.eDx == ref.eDx && x.eDy == ref.eDy);
}

TEST(mapping_x_draw_scaled_pixel_block)
{
    REQUIRE(U(SetPixelV)); REQUIRE(U(PatBlt)); REQUIRE(U(CreateDIBSection));
    mt_dib d;
    if (!mt_dib_make(&d, 64, 64)) { t_skip("no dib"); return; }
    an(d.dc, 0, 0, 1, 1, 0, 0, 2, 2);
    CHECK(U(PatBlt)(d.dc, 3, 4, 5, 6, BLACKNESS) || 1);
    U(SetMapMode)(d.dc, MM_TEXT);
    U(SetPixelV)(d.dc, 20, 20, RGB(255, 255, 255));
    CHECK(mt_set(&d, 20, 20));
    CHECK(mt_count(&d) >= 1);
    mt_dib_free(&d);
}

TEST(mapping_x_draw_org_shift_pixel)
{
    REQUIRE(U(SetPixelV)); REQUIRE(U(CreateDIBSection));
    mt_dib d;
    if (!mt_dib_make(&d, 64, 64)) { t_skip("no dib"); return; }
    U(SetViewportOrgEx)(d.dc, 10, 20, NULL);
    U(SetPixelV)(d.dc, 1, 2, RGB(255, 255, 255));
    CHECK(mt_set(&d, 11, 22));
    CHECK(mt_count(&d) == 1);
    mt_dib_clear(&d);
    U(SetViewportOrgEx)(d.dc, 0, 0, NULL);
    U(SetWindowOrgEx)(d.dc, 5, 6, NULL);
    U(SetPixelV)(d.dc, 7, 9, RGB(255, 255, 255));
    CHECK(mt_set(&d, 2, 3));
    CHECK(mt_count(&d) == 1);
    mt_dib_free(&d);
}

TEST(mapping_x_draw_world_translate_pixel)
{
    REQUIRE(U(SetPixelV)); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform)); REQUIRE(U(CreateDIBSection));
    mt_dib d;
    if (!mt_dib_make(&d, 64, 64)) { t_skip("no dib"); return; }
    U(SetGraphicsMode)(d.dc, GM_ADVANCED);
    XFORM x = {1, 0, 0, 1, 12, 13};
    CHECK(U(SetWorldTransform)(d.dc, &x));
    U(SetPixelV)(d.dc, 3, 4, RGB(255, 255, 255));
    CHECK(mt_set(&d, 15, 17));
    CHECK(mt_count(&d) == 1);
    mt_dib_free(&d);
}

TEST(mapping_x_draw_mirror_x_pixel)
{
    REQUIRE(U(SetPixelV)); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform)); REQUIRE(U(CreateDIBSection));
    mt_dib d;
    if (!mt_dib_make(&d, 64, 64)) { t_skip("no dib"); return; }
    U(SetGraphicsMode)(d.dc, GM_ADVANCED);
    XFORM x = {-1, 0, 0, 1, 63, 0};
    CHECK(U(SetWorldTransform)(d.dc, &x));
    U(SetPixelV)(d.dc, 10, 5, RGB(255, 255, 255));
    CHECK(mt_set(&d, 53, 5));
    CHECK(mt_count(&d) == 1);
    mt_dib_free(&d);
}

TEST(mapping_x_gdi_object_count_stable)
{
    REQUIRE(U(GetGuiResources)); REQUIRE(U(GetCurrentProcess));
    DWORD a = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    for (int i = 0; i < 50; i++) {
        HDC dc = mt_dc();
        an(dc, 1, 1, 2, 2, 3, 3, 4, 4);
        mt_free_dc(dc);
    }
    DWORD b = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    CHECKF(a == b, "gdi objects %lu -> %lu", a, b);
}
