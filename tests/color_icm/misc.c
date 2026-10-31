#include "cih.h"
#include <math.h>

/* ---------------- GetCurrentObject / GetObject on a fresh DC ---------------- */
typedef struct { UINT type; int objtype; const char *name; } ci_cur;
static const ci_cur ci_cur_tab[] = {
    { OBJ_PEN, OBJ_PEN, "pen" }, { OBJ_BRUSH, OBJ_BRUSH, "brush" }, { OBJ_PAL, OBJ_PAL, "palette" },
    { OBJ_FONT, OBJ_FONT, "font" }, { OBJ_BITMAP, OBJ_BITMAP, "bitmap" }, { OBJ_EXTPEN, OBJ_PEN, "extpen-alias" },
    { OBJ_COLORSPACE, OBJ_COLORSPACE, "colorspace" },
};
static void ci_cur_check_dc(HDC dc, const char *what) {
    for (unsigned i = 0; i < sizeof ci_cur_tab / sizeof *ci_cur_tab; i++) {
        if (ci_cur_tab[i].type == OBJ_COLORSPACE && ci_stubbed()) continue;
        HGDIOBJ o = U(GetCurrentObject)(dc, ci_cur_tab[i].type);
        CHECKF(o != NULL, "%s: %s null", what, ci_cur_tab[i].name);
        if (o) CHECKF(U(GetObjectType)(o) == (DWORD)ci_cur_tab[i].objtype, "%s: %s type=%lu", what, ci_cur_tab[i].name, U(GetObjectType)(o));
    }
}
TEST(color_icm_curobj_fresh_mono_memdc) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(U(GetObjectType)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC d = U(CreateCompatibleDC)(NULL);
    ci_cur_check_dc(d, "mono memdc");
    U(DeleteDC)(d);
}
TEST(color_icm_curobj_fresh_dib_memdc) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(U(GetObjectType));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    ci_cur_check_dc(m.dc, "dib memdc");
    ci_mem_close(&m);
}
TEST(color_icm_curobj_screen_dc) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(U(GetObjectType)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    if (!d) { t_skip("no screen dc"); return; }
    ci_cur_check_dc(d, "screen dc");
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_curobj_defaults_are_stock) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(U(GetStockObject)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC d = U(CreateCompatibleDC)(NULL);
    EXPECT_EQ(U(GetCurrentObject)(d, OBJ_PEN), U(GetStockObject)(BLACK_PEN));
    EXPECT_EQ(U(GetCurrentObject)(d, OBJ_BRUSH), U(GetStockObject)(WHITE_BRUSH));
    EXPECT_EQ(U(GetCurrentObject)(d, OBJ_PAL), U(GetStockObject)(DEFAULT_PALETTE));
    HGDIOBJ f = U(GetCurrentObject)(d, OBJ_FONT);
    HGDIOBJ sf = U(GetStockObject)(SYSTEM_FONT);
    if (f != sf) t_note("default DC font is not the SYSTEM_FONT stock handle");
    if (!ci_stubbed()) EXPECT_EQ(U(GetCurrentObject)(d, OBJ_COLORSPACE), U(GetStockObject)(CI_STOCK_COLORSPACE));
    U(DeleteDC)(d);
}
TEST(color_icm_curobj_extpen_equals_pen) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC d = U(CreateCompatibleDC)(NULL);
    EXPECT_EQ(U(GetCurrentObject)(d, OBJ_EXTPEN), U(GetCurrentObject)(d, OBJ_PEN));
    U(DeleteDC)(d);
}
TEST(color_icm_curobj_invalid_types) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC d = U(CreateCompatibleDC)(NULL);
    static const UINT t[] = { 0, 3, 4, 8, 9, 10, 12, 13, 15, 16, 17, 100, 0xffff, 0x10000, 0x80000001u, 0xffffffffu };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        SetLastError(0xdeadbeef);
        HGDIOBJ o = U(GetCurrentObject)(d, t[i]);
        CHECKF(o == NULL, "type=%u", t[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "type=%u err=%lu", t[i], GetLastError());
    }
    U(DeleteDC)(d);
}
TEST(color_icm_curobj_bad_dc) {
    REQUIRE(U(GetCurrentObject));
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef, ci_bogus_dc() };
    static const UINT t[] = { OBJ_PEN, OBJ_BRUSH, OBJ_PAL, OBJ_FONT, OBJ_BITMAP, OBJ_EXTPEN, OBJ_COLORSPACE };
    for (unsigned i = 0; i < 4; i++)
        for (unsigned k = 0; k < sizeof t / sizeof *t; k++)
            CHECKF(U(GetCurrentObject)(bad[i], t[k]) == NULL, "dc#%u type=%u", i, t[k]);
}
TEST(color_icm_curobj_wrong_handle_kind) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(U(CreatePen)); REQUIRE(U(DeleteObject));
    HGDIOBJ pen = U(CreatePen)(PS_SOLID, 1, 0);
    CHECK(U(GetCurrentObject)((HDC)pen, OBJ_PEN) == NULL);
    CHECK(U(GetCurrentObject)((HDC)pen, OBJ_BITMAP) == NULL);
    U(DeleteObject)(pen);
}
TEST(color_icm_curobj_tracks_selection) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(U(SelectObject)); REQUIRE(U(CreatePen)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    HGDIOBJ pen = U(CreatePen)(PS_DASH, 1, 0x00ff00), br = U(CreateSolidBrush)(0x0000ff);
    HGDIOBJ op = U(SelectObject)(m.dc, pen), ob = U(SelectObject)(m.dc, br);
    EXPECT_EQ(op, U(GetStockObject)(BLACK_PEN));
    EXPECT_EQ(ob, U(GetStockObject)(WHITE_BRUSH));
    EXPECT_EQ(U(GetCurrentObject)(m.dc, OBJ_PEN), pen);
    EXPECT_EQ(U(GetCurrentObject)(m.dc, OBJ_EXTPEN), pen);
    EXPECT_EQ(U(GetCurrentObject)(m.dc, OBJ_BRUSH), br);
    U(SelectObject)(m.dc, op); U(SelectObject)(m.dc, ob);
    U(DeleteObject)(pen); U(DeleteObject)(br);
    ci_mem_close(&m);
}
TEST(color_icm_curobj_dib_bitmap_identity) {
    REQUIRE(U(GetCurrentObject));
    ci_mem m; if (!ci_mem_open(&m, 5, 3)) { t_skip("no memdc"); return; }
    EXPECT_EQ(U(GetCurrentObject)(m.dc, OBJ_BITMAP), m.bm);
    ci_mem_close(&m);
}

/* GetObject on each current object of a fresh memory DC */
TEST(color_icm_getobject_current_pen) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(U(GetObjectW)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC d = U(CreateCompatibleDC)(NULL);
    HGDIOBJ p = U(GetCurrentObject)(d, OBJ_PEN);
    LOGPEN lp; memset(&lp, 0xcc, sizeof lp);
    EXPECT_EQ(U(GetObjectW)(p, sizeof lp, &lp), sizeof(LOGPEN));
    EXPECT_EQ(lp.lopnStyle, PS_SOLID);
    EXPECT_EQ(lp.lopnWidth.x, 0);
    EXPECT_EQ(lp.lopnColor, 0);
    EXPECT_EQ(U(GetObjectW)(p, 0, NULL), sizeof(LOGPEN));
    BYTE big[256];
    EXPECT_EQ(U(GetObjectW)(p, sizeof big, big), sizeof(LOGPEN));
    U(DeleteDC)(d);
}
TEST(color_icm_getobject_current_brush) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(U(GetObjectW)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC d = U(CreateCompatibleDC)(NULL);
    HGDIOBJ b = U(GetCurrentObject)(d, OBJ_BRUSH);
    LOGBRUSH lb; memset(&lb, 0xcc, sizeof lb);
    EXPECT_EQ(U(GetObjectW)(b, sizeof lb, &lb), sizeof(LOGBRUSH));
    EXPECT_EQ(lb.lbStyle, BS_SOLID);
    EXPECT_EQ(lb.lbColor, 0x00ffffff);
    EXPECT_EQ(U(GetObjectW)(b, 0, NULL), sizeof(LOGBRUSH));
    U(DeleteDC)(d);
}
TEST(color_icm_getobject_current_font) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(U(GetObjectW)); REQUIRE(U(GetObjectA)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC d = U(CreateCompatibleDC)(NULL);
    HGDIOBJ f = U(GetCurrentObject)(d, OBJ_FONT);
    LOGFONTW lw; LOGFONTA la;
    EXPECT_EQ(U(GetObjectW)(f, sizeof lw, &lw), sizeof(LOGFONTW));
    EXPECT_EQ(U(GetObjectA)(f, sizeof la, &la), sizeof(LOGFONTA));
    EXPECT_EQ(U(GetObjectW)(f, 0, NULL), sizeof(LOGFONTW));
    EXPECT_EQ(U(GetObjectA)(f, 0, NULL), sizeof(LOGFONTA));
    CHECK(lw.lfFaceName[0] != 0);
    U(DeleteDC)(d);
}
TEST(color_icm_getobject_current_palette) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(U(GetObjectW)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC d = U(CreateCompatibleDC)(NULL);
    HGDIOBJ p = U(GetCurrentObject)(d, OBJ_PAL);
    WORD n = 0xffff;
    EXPECT_EQ(U(GetObjectW)(p, sizeof n, &n), sizeof(WORD));
    EXPECT_EQ(n, 20);
    U(DeleteDC)(d);
}
TEST(color_icm_getobject_current_bitmap_mono) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(U(GetObjectW)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC d = U(CreateCompatibleDC)(NULL);
    HGDIOBJ b = U(GetCurrentObject)(d, OBJ_BITMAP);
    BITMAP bm; memset(&bm, 0xcc, sizeof bm);
    EXPECT_EQ(U(GetObjectW)(b, sizeof bm, &bm), sizeof(BITMAP));
    EXPECT_EQ(bm.bmType, 0);
    EXPECT_EQ(bm.bmWidth, 1);
    EXPECT_EQ(bm.bmHeight, 1);
    EXPECT_EQ(bm.bmPlanes, 1);
    EXPECT_EQ(bm.bmBitsPixel, 1);
    EXPECT_EQ(bm.bmWidthBytes, 2);
    EXPECT_EQ(bm.bmBits, NULL);
    U(DeleteDC)(d);
}
TEST(color_icm_getobject_current_bitmap_dib) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(U(GetObjectW));
    ci_mem m; if (!ci_mem_open(&m, 7, 5)) { t_skip("no memdc"); return; }
    HGDIOBJ b = U(GetCurrentObject)(m.dc, OBJ_BITMAP);
    BITMAP bm; memset(&bm, 0xcc, sizeof bm);
    EXPECT_EQ(U(GetObjectW)(b, sizeof bm, &bm), sizeof(BITMAP));
    EXPECT_EQ(bm.bmWidth, 7);
    EXPECT_EQ(bm.bmHeight, 5);
    EXPECT_EQ(bm.bmBitsPixel, 32);
    EXPECT_EQ(bm.bmWidthBytes, 28);
    EXPECT_EQ(bm.bmBits, m.bits);
    DIBSECTION ds; memset(&ds, 0xcc, sizeof ds);
    EXPECT_EQ(U(GetObjectW)(b, sizeof ds, &ds), sizeof(DIBSECTION));
    EXPECT_EQ(ds.dsBmih.biWidth, 7);
    t_note("DIBSECTION biHeight for top-down 5-row DIB = %ld", ds.dsBmih.biHeight);
    CHECK(ds.dsBmih.biHeight == 5 || ds.dsBmih.biHeight == -5);
    EXPECT_EQ(ds.dsBmih.biBitCount, 32);
    ci_mem_close(&m);
}
TEST(color_icm_getobject_current_colorspace) {
    CI_NOWINE();
    REQUIRE(U(GetCurrentObject)); REQUIRE(U(GetObjectW)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC d = U(CreateCompatibleDC)(NULL);
    HGDIOBJ c = U(GetCurrentObject)(d, OBJ_COLORSPACE);
    LOGCOLORSPACEW lc; memset(&lc, 0, sizeof lc);
    EXPECT_EQ(U(GetObjectW)(c, sizeof lc, &lc), sizeof(LOGCOLORSPACEW));
    EXPECT_EQ(lc.lcsSignature, 0x50534F43);
    U(DeleteDC)(d);
}
TEST(color_icm_getobject_buffer_size_probe) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(U(GetObjectW)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC d = U(CreateCompatibleDC)(NULL);
    static const struct { UINT t; int full; const char *n; } tab[] = {
        { OBJ_PEN, sizeof(LOGPEN), "pen" }, { OBJ_BRUSH, sizeof(LOGBRUSH), "brush" }, { OBJ_BITMAP, sizeof(BITMAP), "bitmap" },
        { OBJ_FONT, sizeof(LOGFONTW), "font" },
    };
    BYTE *buf = calloc(1, 512);
    for (unsigned i = 0; i < 4; i++) {
        HGDIOBJ o = U(GetCurrentObject)(d, tab[i].t);
        int r1 = U(GetObjectW)(o, tab[i].full - 1, buf), r0 = U(GetObjectW)(o, 1, buf);
        t_note("%s: GetObject(full-1)=%d GetObject(1)=%d (full=%d)", tab[i].n, r1, r0, tab[i].full);
        CHECKF(r1 >= 0 && r1 < tab[i].full + 1, "%s r1=%d", tab[i].n, r1);
    }
    free(buf);
    U(DeleteDC)(d);
}
TEST(color_icm_getobject_invalid_handle_kinds) {
    REQUIRE(U(GetObjectW)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    BYTE buf[600];
    HDC d = U(CreateCompatibleDC)(NULL);
    EXPECT_EQ(U(GetObjectW)(NULL, sizeof buf, buf), 0);
    EXPECT_EQ(U(GetObjectW)((HGDIOBJ)(ULONG_PTR)0xdeadbeef, sizeof buf, buf), 0);
    EXPECT_EQ(U(GetObjectW)((HGDIOBJ)d, sizeof buf, buf), 0);
    U(DeleteDC)(d);
}

/* ---------------- CombineTransform ---------------- */
static void ci_mul(XFORM *o, const XFORM *a, const XFORM *b) {
    XFORM r;
    r.eM11 = a->eM11 * b->eM11 + a->eM12 * b->eM21;
    r.eM12 = a->eM11 * b->eM12 + a->eM12 * b->eM22;
    r.eM21 = a->eM21 * b->eM11 + a->eM22 * b->eM21;
    r.eM22 = a->eM21 * b->eM12 + a->eM22 * b->eM22;
    r.eDx = a->eDx * b->eM11 + a->eDy * b->eM21 + b->eDx;
    r.eDy = a->eDx * b->eM12 + a->eDy * b->eM22 + b->eDy;
    *o = r;
}
static int ci_xeq(const XFORM *a, const XFORM *b) { return memcmp(a, b, sizeof *a) == 0 || (a->eM11 == b->eM11 && a->eM12 == b->eM12 && a->eM21 == b->eM21 && a->eM22 == b->eM22 && a->eDx == b->eDx && a->eDy == b->eDy); }
static const XFORM ci_ident = { 1, 0, 0, 1, 0, 0 };
static const XFORM ci_xs[] = {
    { 1, 0, 0, 1, 0, 0 }, { 2, 0, 0, 3, 5, 7 }, { 0.5f, 0, 0, 0.5f, 0, 0 }, { 0, 1, -1, 0, 0, 0 }, { 0, 1, -1, 0, 10, -4 },
    { 1.5f, 0.25f, -0.75f, 2, 3.5f, -1.25f }, { -1, 0, 0, 1, 100, 0 }, { 0, 0, 0, 0, 0, 0 }, { 4, 2, 1, 3, -8, 16 },
    { 1, 1, 0, 1, 0, 0 }, { 8, 0, 0, 8, 1024, -1024 },
};
TEST(color_icm_combine_table_exact) {
    REQUIRE(U(CombineTransform));
    int n = sizeof ci_xs / sizeof *ci_xs, bad = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            XFORM o, e;
            memset(&o, 0xcc, sizeof o);
            BOOL r = U(CombineTransform)(&o, &ci_xs[i], &ci_xs[j]);
            ci_mul(&e, &ci_xs[i], &ci_xs[j]);
            if (!r || !ci_xeq(&o, &e)) bad++;
        }
    EXPECT_EQ(bad, 0);
}
TEST(color_icm_combine_identity_neutral) {
    REQUIRE(U(CombineTransform));
    for (unsigned i = 0; i < sizeof ci_xs / sizeof *ci_xs; i++) {
        XFORM o;
        CHECK(U(CombineTransform)(&o, &ci_xs[i], &ci_ident)); CHECKF(ci_xeq(&o, &ci_xs[i]), "right ident i=%u", i);
        CHECK(U(CombineTransform)(&o, &ci_ident, &ci_xs[i])); CHECKF(ci_xeq(&o, &ci_xs[i]), "left ident i=%u", i);
    }
}
TEST(color_icm_combine_order_matters) {
    REQUIRE(U(CombineTransform));
    XFORM a = { 1, 0, 0, 1, 10, 0 }, b = { 2, 0, 0, 2, 0, 0 }, ab, ba;
    CHECK(U(CombineTransform)(&ab, &a, &b));
    CHECK(U(CombineTransform)(&ba, &b, &a));
    EXPECT_EQ(ab.eDx == 20.0f, 1);   /* translate then scale */
    EXPECT_EQ(ba.eDx == 10.0f, 1);   /* scale then translate */
}
TEST(color_icm_combine_alias_out_first) {
    REQUIRE(U(CombineTransform));
    XFORM a = ci_xs[5], b = ci_xs[8], e;
    ci_mul(&e, &a, &b);
    CHECK(U(CombineTransform)(&a, &a, &b));
    CHECK(ci_xeq(&a, &e));
}
TEST(color_icm_combine_alias_out_second) {
    REQUIRE(U(CombineTransform));
    XFORM a = ci_xs[5], b = ci_xs[8], e;
    ci_mul(&e, &a, &b);
    CHECK(U(CombineTransform)(&b, &a, &b));
    CHECK(ci_xeq(&b, &e));
}
TEST(color_icm_combine_alias_all) {
    REQUIRE(U(CombineTransform));
    XFORM a = ci_xs[1], e;
    ci_mul(&e, &a, &a);
    CHECK(U(CombineTransform)(&a, &a, &a));
    CHECK(ci_xeq(&a, &e));
}
TEST(color_icm_combine_associative_exact) {
    REQUIRE(U(CombineTransform));
    XFORM a = ci_xs[1], b = ci_xs[3], c = ci_xs[4], ab, bc, l, r;
    CHECK(U(CombineTransform)(&ab, &a, &b)); CHECK(U(CombineTransform)(&l, &ab, &c));
    CHECK(U(CombineTransform)(&bc, &b, &c)); CHECK(U(CombineTransform)(&r, &a, &bc));
    CHECK(ci_xeq(&l, &r));
}
TEST(color_icm_combine_null_pointers) {
    REQUIRE(U(CombineTransform));
    XFORM a = ci_xs[1], o;
    EXPECT_EQ(U(CombineTransform)(NULL, &a, &a), 0);
    EXPECT_EQ(U(CombineTransform)(&o, NULL, &a), 0);
    EXPECT_EQ(U(CombineTransform)(&o, &a, NULL), 0);
    EXPECT_EQ(U(CombineTransform)(NULL, NULL, NULL), 0);
}
TEST(color_icm_combine_null_leaves_output) {
    REQUIRE(U(CombineTransform));
    XFORM a = ci_xs[1], o; memset(&o, 0x5a, sizeof o);
    XFORM keep = o;
    U(CombineTransform)(&o, NULL, &a);
    CHECK(memcmp(&o, &keep, sizeof o) == 0);
}
TEST(color_icm_combine_zero_matrix) {
    REQUIRE(U(CombineTransform));
    XFORM z = { 0, 0, 0, 0, 0, 0 }, a = ci_xs[5], o;
    CHECK(U(CombineTransform)(&o, &a, &z));
    CHECK(o.eM11 == 0 && o.eM12 == 0 && o.eM21 == 0 && o.eM22 == 0);
    CHECK(o.eDx == 0 && o.eDy == 0);
}
TEST(color_icm_combine_nan_inf_probe) {
    REQUIRE(U(CombineTransform));
    union { DWORD u; float f; } nan = { 0x7fc00000 }, inf = { 0x7f800000 }, ninf = { 0xff800000 };
    float specials[] = { nan.f, inf.f, ninf.f, 3.4e38f, -3.4e38f, 1e-45f };
    for (unsigned i = 0; i < 6; i++) {
        XFORM a = ci_ident, o; memset(&o, 0, sizeof o);
        a.eM11 = specials[i]; a.eDx = specials[(i + 1) % 6];
        XFORM b = ci_xs[1];
        BOOL r = U(CombineTransform)(&o, &a, &b);
        t_note("Combine special[%u]=%g: r=%d out=%g,%g,%g,%g,%g,%g", i, specials[i], r, o.eM11, o.eM12, o.eM21, o.eM22, o.eDx, o.eDy);
    }
}
TEST(color_icm_combine_overflow_probe) {
    REQUIRE(U(CombineTransform));
    XFORM a = { 3.0e38f, 0, 0, 3.0e38f, 0, 0 }, o;
    BOOL r = U(CombineTransform)(&o, &a, &a);
    t_note("Combine overflow: r=%d eM11=%g", r, o.eM11);
}

/* ---------------- Set/Get/ModifyWorldTransform ---------------- */
TEST(color_icm_xform_compat_mode_rejects) {
    REQUIRE(U(SetWorldTransform)); REQUIRE(U(GetGraphicsMode));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    EXPECT_EQ(U(GetGraphicsMode)(m.dc), GM_COMPATIBLE);
    XFORM xf = ci_xs[1];
    EXPECT_EQ(U(SetWorldTransform)(m.dc, &xf), 0);
    EXPECT_EQ(U(ModifyWorldTransform)(m.dc, &xf, MWT_LEFTMULTIPLY), 0);
    EXPECT_EQ(U(ModifyWorldTransform)(m.dc, &xf, MWT_RIGHTMULTIPLY), 0);
    EXPECT_EQ(U(ModifyWorldTransform)(m.dc, &xf, MWT_SET), 0);
    ci_mem_close(&m);
}
TEST(color_icm_xform_get_in_compat_mode) {
    REQUIRE(U(GetWorldTransform));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    XFORM xf; memset(&xf, 0xcc, sizeof xf);
    BOOL r = U(GetWorldTransform)(m.dc, &xf);
    t_note("GetWorldTransform in compat mode r=%d", r);
    if (r) CHECK(ci_xeq(&xf, &ci_ident));
    ci_mem_close(&m);
}
TEST(color_icm_xform_roundtrip_table) {
    REQUIRE(U(SetWorldTransform)); REQUIRE(U(GetWorldTransform)); REQUIRE(U(SetGraphicsMode));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    for (unsigned i = 0; i < sizeof ci_xs / sizeof *ci_xs; i++) {
        XFORM in = ci_xs[i], out; memset(&out, 0xcc, sizeof out);
        BOOL r = U(SetWorldTransform)(m.dc, &in);
        BOOL g = U(GetWorldTransform)(m.dc, &out);
        CHECKF(g, "i=%u get", i);
        if (r) CHECKF(ci_xeq(&in, &out), "i=%u out=%g,%g,%g,%g,%g,%g", i, out.eM11, out.eM12, out.eM21, out.eM22, out.eDx, out.eDy);
        else t_note("SetWorldTransform rejected table entry %u (singular/degenerate?)", i);
    }
    ci_mem_close(&m);
}
TEST(color_icm_xform_advanced_default_identity) {
    REQUIRE(U(GetWorldTransform)); REQUIRE(U(SetGraphicsMode));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    int oldgm = U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    EXPECT_EQ(oldgm, GM_COMPATIBLE);
    XFORM xf;
    CHECK(U(GetWorldTransform)(m.dc, &xf));
    CHECK(ci_xeq(&xf, &ci_ident));
    ci_mem_close(&m);
}
TEST(color_icm_xform_set_null_xform) {
    REQUIRE(U(SetWorldTransform)); REQUIRE(U(SetGraphicsMode));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    EXPECT_EQ(U(SetWorldTransform)(m.dc, NULL), 0);
    XFORM xf; U(GetWorldTransform)(m.dc, &xf);
    CHECK(ci_xeq(&xf, &ci_ident));
    ci_mem_close(&m);
}
TEST(color_icm_xform_get_null_xform) {
    CI_NOWINE_ABORT();
    REQUIRE(U(GetWorldTransform)); REQUIRE(U(SetGraphicsMode));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    EXPECT_EQ(U(GetWorldTransform)(m.dc, NULL), 0);
    ci_mem_close(&m);
}
TEST(color_icm_xform_bad_dcs) {
    REQUIRE(U(SetWorldTransform)); REQUIRE(U(GetWorldTransform)); REQUIRE(U(ModifyWorldTransform));
    XFORM xf = ci_xs[1];
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)0xdeadbeef, ci_bogus_dc() };
    for (unsigned i = 0; i < 3; i++) {
        CHECKF(U(SetWorldTransform)(bad[i], &xf) == 0, "set i=%u", i);
        CHECKF(U(GetWorldTransform)(bad[i], &xf) == 0, "get i=%u", i);
        CHECKF(U(ModifyWorldTransform)(bad[i], &xf, MWT_SET) == 0, "mod i=%u", i);
    }
}
TEST(color_icm_xform_set_null_dc_lasterror) {
    REQUIRE(U(SetWorldTransform));
    XFORM xf = ci_xs[1];
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(SetWorldTransform)(NULL, &xf), 0);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}
TEST(color_icm_xform_modify_modes) {
    REQUIRE(U(ModifyWorldTransform)); REQUIRE(U(GetWorldTransform)); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM base = ci_xs[1], op = ci_xs[5], got, exp;
    CHECK(U(SetWorldTransform)(m.dc, &base));
    CHECK(U(ModifyWorldTransform)(m.dc, &op, MWT_LEFTMULTIPLY));
    CHECK(U(GetWorldTransform)(m.dc, &got));
    ci_mul(&exp, &op, &base);
    CHECKF(ci_xeq(&got, &exp), "left got %g,%g,%g,%g,%g,%g", got.eM11, got.eM12, got.eM21, got.eM22, got.eDx, got.eDy);
    CHECK(U(SetWorldTransform)(m.dc, &base));
    CHECK(U(ModifyWorldTransform)(m.dc, &op, MWT_RIGHTMULTIPLY));
    CHECK(U(GetWorldTransform)(m.dc, &got));
    ci_mul(&exp, &base, &op);
    CHECKF(ci_xeq(&got, &exp), "right got %g,%g,%g,%g,%g,%g", got.eM11, got.eM12, got.eM21, got.eM22, got.eDx, got.eDy);
    CHECK(U(ModifyWorldTransform)(m.dc, &op, MWT_SET));
    CHECK(U(GetWorldTransform)(m.dc, &got));
    CHECK(ci_xeq(&got, &op));
    CHECK(U(ModifyWorldTransform)(m.dc, NULL, MWT_IDENTITY));
    CHECK(U(GetWorldTransform)(m.dc, &got));
    CHECK(ci_xeq(&got, &ci_ident));
    ci_mem_close(&m);
}
TEST(color_icm_xform_modify_invalid_modes) {
    REQUIRE(U(ModifyWorldTransform)); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(GetWorldTransform)); REQUIRE(U(SetWorldTransform));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM base = ci_xs[1], op = ci_xs[5], got;
    U(SetWorldTransform)(m.dc, &base);
    static const DWORD modes[] = { 0, 5, 6, 100, 0x80000000u, 0xffffffffu };
    for (unsigned i = 0; i < sizeof modes / sizeof *modes; i++) {
        SetLastError(0xdeadbeef);
        CHECKF(U(ModifyWorldTransform)(m.dc, &op, modes[i]) == 0, "mode=%lu", modes[i]);
    }
    U(GetWorldTransform)(m.dc, &got);
    CHECK(ci_xeq(&got, &base));
    ci_mem_close(&m);
}
TEST(color_icm_xform_modify_null_xform_modes) {
    REQUIRE(U(ModifyWorldTransform)); REQUIRE(U(SetGraphicsMode));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    EXPECT_EQ(U(ModifyWorldTransform)(m.dc, NULL, MWT_LEFTMULTIPLY), 0);
    EXPECT_EQ(U(ModifyWorldTransform)(m.dc, NULL, MWT_RIGHTMULTIPLY), 0);
    EXPECT_EQ(U(ModifyWorldTransform)(m.dc, NULL, MWT_SET), 0);
    ci_mem_close(&m);
}
TEST(color_icm_xform_nan_inf_probe) {
    REQUIRE(U(SetWorldTransform)); REQUIRE(U(GetWorldTransform)); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(ModifyWorldTransform));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    union { DWORD u; float f; } nan = { 0x7fc00000 }, inf = { 0x7f800000 }, ninf = { 0xff800000 }, snan = { 0x7f800001 };
    float specials[] = { nan.f, inf.f, ninf.f, snan.f, 3.4e38f, -3.4e38f, 1e-45f, -0.0f };
    for (unsigned i = 0; i < 8; i++) {
        for (int field = 0; field < 6; field++) {
            XFORM xf = ci_ident, got;
            ((float *)&xf)[field] = specials[i];
            U(ModifyWorldTransform)(m.dc, NULL, MWT_IDENTITY);
            SetLastError(0xdeadbeef);
            BOOL r = U(SetWorldTransform)(m.dc, &xf);
            DWORD e = GetLastError();
            memset(&got, 0, sizeof got);
            BOOL g = U(GetWorldTransform)(m.dc, &got);
            CHECKF(g, "get after special i=%u field=%d", i, field);
            if (field == 0) t_note("SetWorldTransform eM11=%g -> %d (err=%lu) readback eM11=%g", specials[i], r, e, got.eM11);
            else if (field == 4) t_note("SetWorldTransform eDx=%g -> %d (err=%lu) readback eDx=%g", specials[i], r, e, got.eDx);
            if (!r) CHECKF(ci_xeq(&got, &ci_ident), "failed set modified transform i=%u field=%d", i, field);
        }
    }
    U(ModifyWorldTransform)(m.dc, NULL, MWT_IDENTITY);
    ci_mem_close(&m);
}
TEST(color_icm_xform_singular_probe) {
    REQUIRE(U(SetWorldTransform)); REQUIRE(U(GetWorldTransform)); REQUIRE(U(SetGraphicsMode));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM z = { 0, 0, 0, 0, 0, 0 }, got;
    BOOL r = U(SetWorldTransform)(m.dc, &z);
    U(GetWorldTransform)(m.dc, &got);
    t_note("SetWorldTransform(zero matrix) = %d, readback eM11=%g", r, got.eM11);
    XFORM sing = { 1, 2, 2, 4, 0, 0 };
    r = U(SetWorldTransform)(m.dc, &sing);
    t_note("SetWorldTransform(singular) = %d", r);
    U(ModifyWorldTransform)(m.dc, NULL, MWT_IDENTITY);
    ci_mem_close(&m);
}
TEST(color_icm_xform_savedc_restores) {
    REQUIRE(U(SetWorldTransform)); REQUIRE(U(GetWorldTransform)); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM a = ci_xs[1], b = ci_xs[5], got;
    U(SetWorldTransform)(m.dc, &a);
    int id = U(SaveDC)(m.dc);
    U(SetWorldTransform)(m.dc, &b);
    CHECK(U(RestoreDC)(m.dc, id));
    CHECK(U(GetWorldTransform)(m.dc, &got));
    CHECK(ci_xeq(&got, &a));
    ci_mem_close(&m);
}
TEST(color_icm_xform_graphicsmode_invalid) {
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(GetGraphicsMode));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    static const int modes[] = { 0, 3, 4, -1, 100, 0x7fffffff };
    for (unsigned i = 0; i < sizeof modes / sizeof *modes; i++) {
        SetLastError(0xdeadbeef);
        int r = U(SetGraphicsMode)(m.dc, modes[i]);
        CHECKF(r == 0, "mode=%d r=%d", modes[i], r);
        if (r == 0 && i < 2) t_note("SetGraphicsMode(%d) err=%lu", modes[i], GetLastError());
    }
    EXPECT_EQ(U(GetGraphicsMode)(m.dc), GM_COMPATIBLE);
    ci_mem_close(&m);
}
TEST(color_icm_xform_graphicsmode_roundtrip) {
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(GetGraphicsMode));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    int a = U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    int b = U(GetGraphicsMode)(m.dc);
    int c = U(SetGraphicsMode)(m.dc, GM_COMPATIBLE);
    int d = U(GetGraphicsMode)(m.dc);
    EXPECT_EQ(a, GM_COMPATIBLE); EXPECT_EQ(b, GM_ADVANCED); EXPECT_EQ(c, GM_ADVANCED); EXPECT_EQ(d, GM_COMPATIBLE);
    ci_mem_close(&m);
}
TEST(color_icm_xform_graphicsmode_bad_dc) {
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(GetGraphicsMode));
    EXPECT_EQ(U(SetGraphicsMode)(NULL, GM_ADVANCED), 0);
    EXPECT_EQ(U(GetGraphicsMode)(NULL), 0);
}
static int ci_px(const ci_mem *m, int x, int y) { return ((DWORD *)m->bits)[y * m->w + x] & 0x00ffffff; }
TEST(color_icm_xform_render_scale_translate) {
    REQUIRE(U(SetWorldTransform)); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(PatBlt)); REQUIRE(U(GdiFlush));
    ci_mem m; if (!ci_mem_open(&m, 16, 16)) { t_skip("no memdc"); return; }
    U(PatBlt)(m.dc, 0, 0, 16, 16, WHITENESS);
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM xf = { 2, 0, 0, 2, 1, 1 };
    CHECK(U(SetWorldTransform)(m.dc, &xf));
    CHECK(U(PatBlt)(m.dc, 2, 2, 3, 3, BLACKNESS));   /* device rect [5,11) x [5,11) */
    U(GdiFlush)();
    EXPECT_EQ(ci_px(&m, 5, 5), 0);
    EXPECT_EQ(ci_px(&m, 10, 10), 0);
    EXPECT_EQ(ci_px(&m, 7, 8), 0);
    EXPECT_EQ(ci_px(&m, 4, 5), 0xffffff);
    EXPECT_EQ(ci_px(&m, 5, 4), 0xffffff);
    EXPECT_EQ(ci_px(&m, 11, 10), 0xffffff);
    EXPECT_EQ(ci_px(&m, 10, 11), 0xffffff);
    EXPECT_EQ(ci_px(&m, 2, 2), 0xffffff);
    ci_mem_close(&m);
}
TEST(color_icm_xform_render_identity_after_reset) {
    REQUIRE(U(SetWorldTransform)); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(PatBlt)); REQUIRE(U(GdiFlush)); REQUIRE(U(ModifyWorldTransform));
    ci_mem m; if (!ci_mem_open(&m, 16, 16)) { t_skip("no memdc"); return; }
    U(PatBlt)(m.dc, 0, 0, 16, 16, WHITENESS);
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM xf = { 3, 0, 0, 3, 5, 5 };
    U(SetWorldTransform)(m.dc, &xf);
    U(ModifyWorldTransform)(m.dc, NULL, MWT_IDENTITY);
    U(PatBlt)(m.dc, 1, 1, 2, 2, BLACKNESS);
    U(GdiFlush)();
    EXPECT_EQ(ci_px(&m, 1, 1), 0);
    EXPECT_EQ(ci_px(&m, 2, 2), 0);
    EXPECT_EQ(ci_px(&m, 3, 3), 0xffffff);
    EXPECT_EQ(ci_px(&m, 0, 0), 0xffffff);
    ci_mem_close(&m);
}
TEST(color_icm_xform_render_negative_scale) {
    REQUIRE(U(SetWorldTransform)); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(PatBlt)); REQUIRE(U(GdiFlush));
    ci_mem m; if (!ci_mem_open(&m, 16, 16)) { t_skip("no memdc"); return; }
    U(PatBlt)(m.dc, 0, 0, 16, 16, WHITENESS);
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM xf = { -1, 0, 0, 1, 15, 0 };   /* mirror horizontally */
    CHECK(U(SetWorldTransform)(m.dc, &xf));
    CHECK(U(PatBlt)(m.dc, 0, 0, 4, 2, BLACKNESS));
    U(GdiFlush)();
    t_note("mirrored fill: px(14,0)=%06x px(15,0)=%06x px(11,0)=%06x px(12,0)=%06x", ci_px(&m, 14, 0), ci_px(&m, 15, 0), ci_px(&m, 11, 0), ci_px(&m, 12, 0));
    EXPECT_EQ(ci_px(&m, 0, 5), 0xffffff);
    EXPECT_EQ(ci_px(&m, 8, 0), 0xffffff);
    ci_mem_close(&m);
}

/* ---------------- MAKEROP4 ---------------- */
TEST(color_icm_rop4_macro_values) {
    /* MAKEROP4(fore, back): background ROP3 index goes in the top byte, foreground ROP3 code stays in place */
    EXPECT_EQ(MAKEROP4(SRCCOPY, 0x00AA0029), 0xAA000000 | 0x00CC0020);
    EXPECT_EQ(MAKEROP4(SRCCOPY, BLACKNESS), 0x00CC0020);
    EXPECT_EQ(MAKEROP4(SRCCOPY, SRCCOPY), 0xCC000000 | 0x00CC0020);
    EXPECT_EQ(MAKEROP4(0, 0), 0);
    EXPECT_EQ(MAKEROP4(0x00F00021, 0x00FF0062), 0xFF000000 | 0x00F00021);
    EXPECT_EQ(MAKEROP4(PATCOPY, 0x00AA0029) & 0x00ffffff, 0x00F00021);
    EXPECT_EQ(MAKEROP4(PATCOPY, 0x00AA0029) >> 24, 0xAA);
    EXPECT_EQ(MAKEROP4(0x00AA0029, SRCCOPY) >> 24, 0xCC);
}
TEST(color_icm_rop3_constants) {
    EXPECT_EQ(SRCCOPY, 0x00CC0020); EXPECT_EQ(SRCPAINT, 0x00EE0086); EXPECT_EQ(SRCAND, 0x008800C6);
    EXPECT_EQ(SRCINVERT, 0x00660046); EXPECT_EQ(SRCERASE, 0x00440328); EXPECT_EQ(NOTSRCCOPY, 0x00330008);
    EXPECT_EQ(NOTSRCERASE, 0x001100A6); EXPECT_EQ(MERGECOPY, 0x00C000CA); EXPECT_EQ(MERGEPAINT, 0x00BB0226);
    EXPECT_EQ(PATCOPY, 0x00F00021); EXPECT_EQ(PATPAINT, 0x00FB0A09); EXPECT_EQ(PATINVERT, 0x005A0049);
    EXPECT_EQ(DSTINVERT, 0x00550009); EXPECT_EQ(BLACKNESS, 0x00000042); EXPECT_EQ(WHITENESS, 0x00FF0062);
    EXPECT_EQ(NOMIRRORBITMAP, 0x80000000u); EXPECT_EQ(CAPTUREBLT, 0x40000000);
}
TEST(color_icm_rop4_maskblt_mask_selects_rop) {
    REQUIRE(U(MaskBlt)); REQUIRE(U(CreateBitmap)); REQUIRE(U(GdiFlush)); REQUIRE(U(DeleteObject));
    ci_mem src, dst;
    if (!ci_mem_open(&src, 4, 4)) { t_skip("no memdc"); return; }
    if (!ci_mem_open(&dst, 4, 4)) { ci_mem_close(&src); t_skip("no memdc"); return; }
    for (int i = 0; i < 16; i++) { ((DWORD *)src.bits)[i] = 0x00100000 + i * 0x101; ((DWORD *)dst.bits)[i] = 0x00abcdef; }
    BYTE mbits[8] = { 0xA0, 0, 0x50, 0, 0xA0, 0, 0x50, 0 };   /* rows: 1010 / 0101 / 1010 / 0101 */
    HBITMAP mask = U(CreateBitmap)(4, 4, 1, 1, mbits);
    BOOL r = U(MaskBlt)(dst.dc, 0, 0, 4, 4, src.dc, 0, 0, mask, 0, 0, MAKEROP4(SRCCOPY, 0x00AA0029));
    U(GdiFlush)();
    CHECK(r);
    int bad = 0;
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++) {
            int bit = (mbits[y * 2] >> (7 - x)) & 1;
            int want = bit ? (0x00100000 + (y * 4 + x) * 0x101) : 0x00abcdef;
            if (ci_px(&dst, x, y) != want) bad++;
        }
    EXPECT_EQ(bad, 0);
    U(DeleteObject)(mask);
    ci_mem_close(&src); ci_mem_close(&dst);
}
TEST(color_icm_rop4_maskblt_background_blackness) {
    REQUIRE(U(MaskBlt)); REQUIRE(U(CreateBitmap)); REQUIRE(U(GdiFlush)); REQUIRE(U(DeleteObject));
    ci_mem src, dst;
    if (!ci_mem_open(&src, 4, 4)) { t_skip("no memdc"); return; }
    if (!ci_mem_open(&dst, 4, 4)) { ci_mem_close(&src); t_skip("no memdc"); return; }
    for (int i = 0; i < 16; i++) { ((DWORD *)src.bits)[i] = 0x00336699; ((DWORD *)dst.bits)[i] = 0x00abcdef; }
    BYTE mbits[8] = { 0xF0, 0, 0xF0, 0, 0x00, 0, 0x00, 0 };   /* top two rows: foreground; bottom two: background */
    HBITMAP mask = U(CreateBitmap)(4, 4, 1, 1, mbits);
    CHECK(U(MaskBlt)(dst.dc, 0, 0, 4, 4, src.dc, 0, 0, mask, 0, 0, MAKEROP4(SRCCOPY, BLACKNESS)));
    U(GdiFlush)();
    EXPECT_EQ(ci_px(&dst, 0, 0), 0x336699);
    EXPECT_EQ(ci_px(&dst, 3, 1), 0x336699);
    EXPECT_EQ(ci_px(&dst, 0, 2), 0);
    EXPECT_EQ(ci_px(&dst, 3, 3), 0);
    U(DeleteObject)(mask);
    ci_mem_close(&src); ci_mem_close(&dst);
}
TEST(color_icm_rop4_maskblt_background_whiteness) {
    REQUIRE(U(MaskBlt)); REQUIRE(U(CreateBitmap)); REQUIRE(U(GdiFlush)); REQUIRE(U(DeleteObject));
    ci_mem src, dst;
    if (!ci_mem_open(&src, 4, 4)) { t_skip("no memdc"); return; }
    if (!ci_mem_open(&dst, 4, 4)) { ci_mem_close(&src); t_skip("no memdc"); return; }
    for (int i = 0; i < 16; i++) { ((DWORD *)src.bits)[i] = 0x00336699; ((DWORD *)dst.bits)[i] = 0x00abcdef; }
    BYTE mbits[8] = { 0, 0, 0, 0, 0xF0, 0, 0xF0, 0 };
    HBITMAP mask = U(CreateBitmap)(4, 4, 1, 1, mbits);
    CHECK(U(MaskBlt)(dst.dc, 0, 0, 4, 4, src.dc, 0, 0, mask, 0, 0, MAKEROP4(SRCCOPY, WHITENESS)));
    U(GdiFlush)();
    EXPECT_EQ(ci_px(&dst, 1, 0), 0xffffff);
    EXPECT_EQ(ci_px(&dst, 2, 1), 0xffffff);
    EXPECT_EQ(ci_px(&dst, 1, 2), 0x336699);
    U(DeleteObject)(mask);
    ci_mem_close(&src); ci_mem_close(&dst);
}
TEST(color_icm_rop4_maskblt_bad_mask_kind) {
    REQUIRE(U(MaskBlt)); REQUIRE(U(CreateBitmap)); REQUIRE(U(DeleteObject));
    ci_mem src, dst;
    if (!ci_mem_open(&src, 4, 4)) { t_skip("no memdc"); return; }
    if (!ci_mem_open(&dst, 4, 4)) { ci_mem_close(&src); t_skip("no memdc"); return; }
    /* a colour mask bitmap is not a valid mask */
    HBITMAP color = U(CreateBitmap)(4, 4, 1, 32, NULL);
    SetLastError(0xdeadbeef);
    BOOL r = U(MaskBlt)(dst.dc, 0, 0, 4, 4, src.dc, 0, 0, color, 0, 0, MAKEROP4(SRCCOPY, 0x00AA0029));
    t_note("MaskBlt with 32bpp mask = %d err=%lu", r, GetLastError());
    CHECK(r == 0);
    U(DeleteObject)(color);
    ci_mem_close(&src); ci_mem_close(&dst);
}

/* ---------------- trivial helper exports from gdi32full ---------------- */
UDECL(void, EngGetCurrentCodePage, (PUSHORT, PUSHORT));
UDECL(UINT, GetCodePage, (HDC));
UDECL(int, GdiGetCharDimensions, (HDC, TEXTMETRICW *, LONG *));
TEST(color_icm_helper_eng_codepage) {
    REQUIRE(UN(EngGetCurrentCodePage));
    USHORT oem = 0xbeef, ansi = 0xbeef;
    UN(EngGetCurrentCodePage)(&oem, &ansi);
    EXPECT_EQ(ansi, (USHORT)GetACP());
    EXPECT_EQ(oem, (USHORT)GetOEMCP());
}
TEST(color_icm_helper_getcodepage_bad_dc) {
    REQUIRE(UN(GetCodePage));
    SetLastError(0xdeadbeef);
    UINT r = UN(GetCodePage)(NULL);
    EXPECT_EQ(r, GetACP());
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef);
    r = UN(GetCodePage)(ci_bogus_dc());
    EXPECT_EQ(r, GetACP());
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
}
TEST(color_icm_helper_getcodepage_fresh_dc) {
    REQUIRE(UN(GetCodePage)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC d = U(CreateCompatibleDC)(NULL);
    UINT r = UN(GetCodePage)(d);
    t_note("GetCodePage(fresh memdc)=%u ACP=%u", r, GetACP());
    CHECK(r != 0xffffffffu);
    U(DeleteDC)(d);
}
TEST(color_icm_helper_chardimensions_bad_dc) {
    REQUIRE(UN(GdiGetCharDimensions));
    LONG h = 0x1234;
    EXPECT_EQ(UN(GdiGetCharDimensions)(NULL, NULL, NULL), 0);
    EXPECT_EQ(UN(GdiGetCharDimensions)(NULL, NULL, &h), 0);
    EXPECT_EQ(h, 0x1234);
    EXPECT_EQ(UN(GdiGetCharDimensions)(ci_bogus_dc(), NULL, NULL), 0);
}
static HFONT ci_font(const WCHAR *face, int h, int charset, int pitch) {
    LOGFONTW lf; memset(&lf, 0, sizeof lf);
    lf.lfHeight = h; lf.lfWeight = FW_NORMAL; lf.lfCharSet = (BYTE)charset; lf.lfPitchAndFamily = (BYTE)pitch;
    lf.lfQuality = ANTIALIASED_QUALITY;
    lstrcpyW(lf.lfFaceName, face);
    return U(CreateFontIndirectW)(&lf);
}
TEST(color_icm_helper_chardimensions_variable_pitch) {
    REQUIRE(UN(GdiGetCharDimensions)); REQUIRE(U(CreateFontIndirectW)); REQUIRE(U(GetTextExtentPoint32W)); REQUIRE(U(GetTextMetricsW));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    static const int heights[] = { -8, -11, -12, -16, -24, -40 };
    for (unsigned i = 0; i < sizeof heights / sizeof *heights; i++) {
        HFONT f = ci_font(L"Arial", heights[i], ANSI_CHARSET, 0);
        HGDIOBJ old = U(SelectObject)(m.dc, f);
        TEXTMETRICW tm, tm2; LONG height = 0;
        SIZE sz;
        U(GetTextExtentPoint32W)(m.dc, L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", 52, &sz);
        int want = (sz.cx / 26 + 1) / 2;
        memset(&tm, 0, sizeof tm);
        int r = UN(GdiGetCharDimensions)(m.dc, &tm, &height);
        U(GetTextMetricsW)(m.dc, &tm2);
        CHECKF(r == want, "h=%d got %d want %d", heights[i], r, want);
        CHECKF(height == tm2.tmHeight, "h=%d height %ld vs %ld", heights[i], height, tm2.tmHeight);
        CHECKF(tm.tmHeight == tm2.tmHeight && tm.tmAveCharWidth == tm2.tmAveCharWidth, "h=%d tm mismatch", heights[i]);
        int r2 = UN(GdiGetCharDimensions)(m.dc, NULL, NULL);
        CHECKF(r2 == r, "NULL out params changed result %d vs %d", r2, r);
        U(SelectObject)(m.dc, old);
        U(DeleteObject)(f);
    }
    ci_mem_close(&m);
}
TEST(color_icm_helper_chardimensions_fixed_pitch) {
    REQUIRE(UN(GdiGetCharDimensions)); REQUIRE(U(CreateFontIndirectW)); REQUIRE(U(GetTextMetricsW));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    HFONT f = ci_font(L"Courier New", -16, ANSI_CHARSET, FIXED_PITCH);
    HGDIOBJ old = U(SelectObject)(m.dc, f);
    TEXTMETRICW tm; LONG h = 0;
    U(GetTextMetricsW)(m.dc, &tm);
    int r = UN(GdiGetCharDimensions)(m.dc, NULL, &h);
    if (!(tm.tmPitchAndFamily & TMPF_FIXED_PITCH)) EXPECT_EQ(r, tm.tmAveCharWidth);
    else t_note("Courier New reports variable-pitch flag set (TMPF_FIXED_PITCH means variable)");
    EXPECT_EQ(h, tm.tmHeight);
    U(SelectObject)(m.dc, old);
    U(DeleteObject)(f);
    ci_mem_close(&m);
}
TEST(color_icm_helper_chardimensions_repeatable) {
    REQUIRE(UN(GdiGetCharDimensions)); REQUIRE(U(CreateFontIndirectW));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    HFONT f = ci_font(L"Tahoma", -13, ANSI_CHARSET, 0);
    HGDIOBJ old = U(SelectObject)(m.dc, f);
    int first = UN(GdiGetCharDimensions)(m.dc, NULL, NULL);
    for (int i = 0; i < 100; i++) if (UN(GdiGetCharDimensions)(m.dc, NULL, NULL) != first) { CHECKF(0, "iter %d differs", i); break; }
    CHECK(first > 0);
    U(SelectObject)(m.dc, old);
    U(DeleteObject)(f);
    ci_mem_close(&m);
}

/* ---------------- text charset ---------------- */
TEST(color_icm_charset_bad_dc) {
    REQUIRE(U(GetTextCharset)); REQUIRE(U(GetTextCharsetInfo));
    FONTSIGNATURE fs;
    EXPECT_EQ(U(GetTextCharset)(NULL), DEFAULT_CHARSET);
    EXPECT_EQ(U(GetTextCharsetInfo)(NULL, &fs, 0), DEFAULT_CHARSET);
    EXPECT_EQ(U(GetTextCharset)(ci_bogus_dc()), DEFAULT_CHARSET);
    EXPECT_EQ(U(GetTextCharset)((HDC)(ULONG_PTR)0xdeadbeef), DEFAULT_CHARSET);
}
static const struct { int cs; const WCHAR *face; int hard; const char *name; } ci_charsets[] = {
    { ANSI_CHARSET, L"Arial", 1, "ansi" }, { EASTEUROPE_CHARSET, L"Arial", 1, "easteurope" }, { RUSSIAN_CHARSET, L"Arial", 1, "russian" },
    { GREEK_CHARSET, L"Arial", 1, "greek" }, { TURKISH_CHARSET, L"Arial", 1, "turkish" }, { BALTIC_CHARSET, L"Arial", 1, "baltic" },
    { HEBREW_CHARSET, L"Arial", 0, "hebrew" }, { ARABIC_CHARSET, L"Arial", 0, "arabic" }, { VIETNAMESE_CHARSET, L"Arial", 0, "vietnamese" },
    { ANSI_CHARSET, L"Times New Roman", 1, "tnr-ansi" }, { RUSSIAN_CHARSET, L"Times New Roman", 1, "tnr-russian" },
    { ANSI_CHARSET, L"Tahoma", 1, "tahoma-ansi" }, { ANSI_CHARSET, L"Courier New", 1, "courier-ansi" }, { SYMBOL_CHARSET, L"Symbol", 1, "symbol" },
};
TEST(color_icm_charset_selected_font_table) {
    REQUIRE(U(GetTextCharset)); REQUIRE(U(GetTextCharsetInfo)); REQUIRE(U(CreateFontIndirectW));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    for (unsigned i = 0; i < sizeof ci_charsets / sizeof *ci_charsets; i++) {
        HFONT f = ci_font(ci_charsets[i].face, -16, ci_charsets[i].cs, 0);
        HGDIOBJ old = U(SelectObject)(m.dc, f);
        FONTSIGNATURE fs; memset(&fs, 0, sizeof fs);
        int a = U(GetTextCharset)(m.dc), b = U(GetTextCharsetInfo)(m.dc, &fs, 0);
        CHECKF(a == b, "%s: GetTextCharset=%d Info=%d", ci_charsets[i].name, a, b);
        if (ci_charsets[i].hard) CHECKF(a == ci_charsets[i].cs, "%s: got %d want %d", ci_charsets[i].name, a, ci_charsets[i].cs);
        else if (a != ci_charsets[i].cs) t_note("%s: requested %d got %d", ci_charsets[i].name, ci_charsets[i].cs, a);
        U(SelectObject)(m.dc, old);
        U(DeleteObject)(f);
    }
    ci_mem_close(&m);
}
TEST(color_icm_charset_fontsig_latin1) {
    REQUIRE(U(GetTextCharsetInfo)); REQUIRE(U(CreateFontIndirectW));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    HFONT f = ci_font(L"Arial", -16, ANSI_CHARSET, 0);
    HGDIOBJ old = U(SelectObject)(m.dc, f);
    FONTSIGNATURE fs; memset(&fs, 0, sizeof fs);
    U(GetTextCharsetInfo)(m.dc, &fs, 0);
    CHECK(fs.fsCsb[0] & FS_LATIN1);
    CHECK(fs.fsCsb[0] & FS_LATIN2);
    CHECK(fs.fsCsb[0] & FS_CYRILLIC);
    CHECK(fs.fsCsb[0] & FS_GREEK);
    U(SelectObject)(m.dc, old);
    U(DeleteObject)(f);
    ci_mem_close(&m);
}
TEST(color_icm_charset_null_fontsig) {
    REQUIRE(U(GetTextCharsetInfo)); REQUIRE(U(CreateFontIndirectW));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    HFONT f = ci_font(L"Arial", -16, GREEK_CHARSET, 0);
    HGDIOBJ old = U(SelectObject)(m.dc, f);
    EXPECT_EQ(U(GetTextCharsetInfo)(m.dc, NULL, 0), U(GetTextCharset)(m.dc));
    U(SelectObject)(m.dc, old);
    U(DeleteObject)(f);
    ci_mem_close(&m);
}
TEST(color_icm_charset_marlett_symbol) {
    REQUIRE(U(GetTextCharset)); REQUIRE(U(CreateFontIndirectW));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    HFONT f = ci_font(L"Marlett", -16, DEFAULT_CHARSET, 0);
    HGDIOBJ old = U(SelectObject)(m.dc, f);
    EXPECT_EQ(U(GetTextCharset)(m.dc), SYMBOL_CHARSET);
    U(SelectObject)(m.dc, old);
    U(DeleteObject)(f);
    ci_mem_close(&m);
}
TEST(color_icm_charset_stock_fonts_probe) {
    REQUIRE(U(GetTextCharset)); REQUIRE(U(GetStockObject));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    static const int st[] = { OEM_FIXED_FONT, ANSI_FIXED_FONT, ANSI_VAR_FONT, SYSTEM_FONT, DEVICE_DEFAULT_FONT, SYSTEM_FIXED_FONT, DEFAULT_GUI_FONT };
    for (unsigned i = 0; i < sizeof st / sizeof *st; i++) {
        HGDIOBJ old = U(SelectObject)(m.dc, U(GetStockObject)(st[i]));
        int cs = U(GetTextCharset)(m.dc);
        t_note("stock font %d charset %d", st[i], cs);
        CHECK(cs >= 0 && cs <= 255);
        U(SelectObject)(m.dc, old);
    }
    ci_mem_close(&m);
}
TEST(color_icm_charset_default_font_ansi_or_oem) {
    REQUIRE(U(GetTextCharset));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    int cs = U(GetTextCharset)(m.dc);
    t_note("default DC font charset=%d", cs);
    CHECK(cs >= 0 && cs <= 255);
    ci_mem_close(&m);
}

/* ---------------- TranslateCharsetInfo ---------------- */
static const struct { DWORD cs; DWORD acp; DWORD fs0; int hard; } ci_tci[] = {
    { ANSI_CHARSET, 1252, FS_LATIN1, 1 }, { EASTEUROPE_CHARSET, 1250, FS_LATIN2, 1 }, { RUSSIAN_CHARSET, 1251, FS_CYRILLIC, 1 },
    { GREEK_CHARSET, 1253, FS_GREEK, 1 }, { TURKISH_CHARSET, 1254, FS_TURKISH, 1 }, { HEBREW_CHARSET, 1255, FS_HEBREW, 1 },
    { ARABIC_CHARSET, 1256, FS_ARABIC, 1 }, { BALTIC_CHARSET, 1257, FS_BALTIC, 1 }, { VIETNAMESE_CHARSET, 1258, FS_VIETNAMESE, 1 },
    { THAI_CHARSET, 874, FS_THAI, 1 }, { SHIFTJIS_CHARSET, 932, FS_JISJAPAN, 1 }, { GB2312_CHARSET, 936, FS_CHINESESIMP, 1 },
    { HANGEUL_CHARSET, 949, FS_WANSUNG, 1 }, { CHINESEBIG5_CHARSET, 950, FS_CHINESETRAD, 1 },
    { JOHAB_CHARSET, 1361, FS_JOHAB, 0 }, { MAC_CHARSET, 10000, 0, 0 }, { SYMBOL_CHARSET, 42, (DWORD)FS_SYMBOL, 0 }, { OEM_CHARSET, 0, 0, 0 },
};
TEST(color_icm_tci_by_charset_table) {
    REQUIRE(U(TranslateCharsetInfo));
    for (unsigned i = 0; i < sizeof ci_tci / sizeof *ci_tci; i++) {
        CHARSETINFO ci; memset(&ci, 0xcc, sizeof ci);
        BOOL r = U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)ci_tci[i].cs, &ci, TCI_SRCCHARSET);
        if (ci_tci[i].hard) {
            CHECKF(r, "charset %lu", ci_tci[i].cs);
            if (r) {
                CHECKF(ci.ciCharset == ci_tci[i].cs, "charset %lu -> %u", ci_tci[i].cs, ci.ciCharset);
                CHECKF(ci.ciACP == ci_tci[i].acp, "charset %lu acp %u want %lu", ci_tci[i].cs, ci.ciACP, ci_tci[i].acp);
                CHECKF(ci.fs.fsCsb[0] == ci_tci[i].fs0, "charset %lu csb0 %08lx want %08lx", ci_tci[i].cs, ci.fs.fsCsb[0], ci_tci[i].fs0);
                CHECK(ci.fs.fsCsb[1] == 0);
                CHECK(ci.fs.fsUsb[0] == 0 && ci.fs.fsUsb[1] == 0 && ci.fs.fsUsb[2] == 0 && ci.fs.fsUsb[3] == 0);
            }
        } else t_note("charset %lu: r=%d acp=%u csb0=%08lx", ci_tci[i].cs, r, r ? ci.ciACP : 0, r ? ci.fs.fsCsb[0] : 0);
    }
}
TEST(color_icm_tci_by_codepage_table) {
    REQUIRE(U(TranslateCharsetInfo));
    for (unsigned i = 0; i < sizeof ci_tci / sizeof *ci_tci; i++) {
        if (!ci_tci[i].hard) continue;
        CHARSETINFO ci; memset(&ci, 0xcc, sizeof ci);
        BOOL r = U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)ci_tci[i].acp, &ci, TCI_SRCCODEPAGE);
        CHECKF(r, "cp %lu", ci_tci[i].acp);
        if (r) {
            CHECKF(ci.ciCharset == ci_tci[i].cs, "cp %lu -> charset %u want %lu", ci_tci[i].acp, ci.ciCharset, ci_tci[i].cs);
            CHECKF(ci.ciACP == ci_tci[i].acp, "cp %lu acp %u", ci_tci[i].acp, ci.ciACP);
            CHECKF(ci.fs.fsCsb[0] == ci_tci[i].fs0, "cp %lu csb0", ci_tci[i].acp);
        }
    }
}
TEST(color_icm_tci_by_fontsig_table) {
    REQUIRE(U(TranslateCharsetInfo));
    for (unsigned i = 0; i < sizeof ci_tci / sizeof *ci_tci; i++) {
        if (!ci_tci[i].hard) continue;
        DWORD src[2] = { ci_tci[i].fs0, 0 };
        CHARSETINFO ci; memset(&ci, 0xcc, sizeof ci);
        BOOL r = U(TranslateCharsetInfo)(src, &ci, TCI_SRCFONTSIG);
        CHECKF(r, "fs %08lx", src[0]);
        if (r) {
            CHECKF(ci.ciCharset == ci_tci[i].cs, "fs %08lx -> charset %u want %lu", src[0], ci.ciCharset, ci_tci[i].cs);
            CHECKF(ci.ciACP == ci_tci[i].acp, "fs %08lx acp %u", src[0], ci.ciACP);
        }
    }
}
TEST(color_icm_tci_charset_high_bits_masked) {
    REQUIRE(U(TranslateCharsetInfo));
    CHARSETINFO ci;
    /* the source value is a 16-bit quantity */
    BOOL r = U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)0x12340000, &ci, TCI_SRCCHARSET);
    CHECK(r);
    if (r) EXPECT_EQ(ci.ciCharset, ANSI_CHARSET);
    r = U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)(0xabcd0000 | 1251), &ci, TCI_SRCCODEPAGE);
    CHECK(r);
    if (r) EXPECT_EQ(ci.ciCharset, RUSSIAN_CHARSET);
}
TEST(color_icm_tci_unknown_values) {
    REQUIRE(U(TranslateCharsetInfo));
    CHARSETINFO ci;
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)3, &ci, TCI_SRCCHARSET), 0);
    EXPECT_EQ(U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)0x7777, &ci, TCI_SRCCHARSET), 0);
    EXPECT_EQ(U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)1, &ci, TCI_SRCCODEPAGE), 0);
    t_note("TCI_SRCCODEPAGE 65001 -> %d", U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)65001, &ci, TCI_SRCCODEPAGE));
    DWORD fs[2] = { 0, 0 };
    EXPECT_EQ(U(TranslateCharsetInfo)(fs, &ci, TCI_SRCFONTSIG), 0);
    DWORD fs2[2] = { 0x00000003, 0 };   /* two bits: no single table entry matches */
    EXPECT_EQ(U(TranslateCharsetInfo)(fs2, &ci, TCI_SRCFONTSIG), 0);
}
TEST(color_icm_tci_invalid_flags) {
    REQUIRE(U(TranslateCharsetInfo));
    CHARSETINFO ci;
    static const DWORD flags[] = { 0, 4, 5, 8, 0x10, 0x100, 0x2000, 0x80000000u, 0xffffffffu };
    for (unsigned i = 0; i < sizeof flags / sizeof *flags; i++) {
        SetLastError(0xdeadbeef);
        CHECKF(U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)0, &ci, flags[i]) == 0, "flags=%lx", flags[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "flags=%lx err=%lu", flags[i], GetLastError());
    }
}
TEST(color_icm_tci_null_output) {
    CI_NOWINE_ABORT();
    REQUIRE(U(TranslateCharsetInfo));
    EXPECT_EQ(U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)ANSI_CHARSET, NULL, TCI_SRCCHARSET), 0);
    EXPECT_EQ(U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)1252, NULL, TCI_SRCCODEPAGE), 0);
}
TEST(color_icm_tci_locale) {
    REQUIRE(U(TranslateCharsetInfo));
    static const struct { WORD lang; const char *n; } t[] = {
        { MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), "en-US" }, { MAKELANGID(LANG_RUSSIAN, SUBLANG_DEFAULT), "ru" },
        { MAKELANGID(LANG_GREEK, SUBLANG_DEFAULT), "el" }, { MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT), "ja" },
    };
    for (unsigned i = 0; i < 4; i++) {
        CHARSETINFO ci; memset(&ci, 0, sizeof ci);
        BOOL r = U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)MAKELCID(t[i].lang, SORT_DEFAULT), &ci, TCI_SRCLOCALE);
        t_note("TCI_SRCLOCALE %s: r=%d charset=%u acp=%u", t[i].n, r, r ? ci.ciCharset : 0, r ? ci.ciACP : 0);
    }
}
TEST(color_icm_tci_charset_roundtrip_all_bytes) {
    REQUIRE(U(TranslateCharsetInfo));
    int found = 0;
    for (DWORD c = 0; c < 256; c++) {
        CHARSETINFO ci;
        if (U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)c, &ci, TCI_SRCCHARSET)) {
            found++;
            CHECKF(ci.ciCharset == c, "c=%lu -> %u", c, ci.ciCharset);
            CHARSETINFO ci2;
            if (c != SYMBOL_CHARSET && U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)ci.ciACP, &ci2, TCI_SRCCODEPAGE))
                CHECKF(ci2.ciACP == ci.ciACP, "c=%lu cp roundtrip", c);
        }
    }
    CHECKF(found >= 14 && found <= 20, "found=%d", found);
}
