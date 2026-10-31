/* Table-driven checks over every simple per-DC attribute (get/set pairs). */
#include "dca.h"

UDECL(int, SetRelAbs, (HDC, int));
UDECL(int, GetRelAbs, (HDC, DWORD));

#define SETW(nm, fn) static DWORD s_##nm(HDC d, DWORD v) { return (DWORD)U(fn)(d, v); }
#define GETW(nm, fn) static DWORD g_##nm(HDC d) { return (DWORD)U(fn)(d); }
SETW(bkcolor, SetBkColor)       GETW(bkcolor, GetBkColor)
SETW(textcolor, SetTextColor)   GETW(textcolor, GetTextColor)
SETW(bkmode, SetBkMode)         GETW(bkmode, GetBkMode)
SETW(rop2, SetROP2)             GETW(rop2, GetROP2)
SETW(polyfill, SetPolyFillMode) GETW(polyfill, GetPolyFillMode)
SETW(stretch, SetStretchBltMode) GETW(stretch, GetStretchBltMode)
SETW(arcdir, SetArcDirection)   GETW(arcdir, GetArcDirection)
SETW(textalign, SetTextAlign)   GETW(textalign, GetTextAlign)
SETW(charextra, SetTextCharacterExtra) GETW(charextra, GetTextCharacterExtra)
SETW(layout, SetLayout)         GETW(layout, GetLayout)
SETW(gfxmode, SetGraphicsMode)  GETW(gfxmode, GetGraphicsMode)
SETW(mapper, SetMapperFlags)
SETW(dcbrush, SetDCBrushColor)  GETW(dcbrush, GetDCBrushColor)
SETW(dcpen, SetDCPenColor)      GETW(dcpen, GetDCPenColor)
static DWORD s_relabs(HDC d, DWORD v) { return (DWORD)UN(SetRelAbs)(d, (int)v); }
static DWORD g_relabs(HDC d) { return (DWORD)UN(GetRelAbs)(d, 0); }
/* no GetMapperFlags export: read through a set of 0 and put the value back */
static DWORD g_mapper(HDC d) {
    DWORD v = U(SetMapperFlags)(d, 0);
    if (v != 0xffffffff && v != 0) U(SetMapperFlags)(d, v);
    return v;
}
#define AV2(nm, a, b) static int av_##nm(void) { return U(a) && U(b); }
AV2(bkcolor, SetBkColor, GetBkColor) AV2(textcolor, SetTextColor, GetTextColor)
AV2(bkmode, SetBkMode, GetBkMode) AV2(rop2, SetROP2, GetROP2)
AV2(polyfill, SetPolyFillMode, GetPolyFillMode) AV2(stretch, SetStretchBltMode, GetStretchBltMode)
AV2(arcdir, SetArcDirection, GetArcDirection) AV2(textalign, SetTextAlign, GetTextAlign)
AV2(charextra, SetTextCharacterExtra, GetTextCharacterExtra) AV2(layout, SetLayout, GetLayout)
AV2(gfxmode, SetGraphicsMode, GetGraphicsMode) AV2(dcbrush, SetDCBrushColor, GetDCBrushColor)
AV2(dcpen, SetDCPenColor, GetDCPenColor)
static int av_mapper(void) { return U(SetMapperFlags) != NULL; }
static int av_relabs(void) { return UN(SetRelAbs) && UN(GetRelAbs); }

#define M1 0xffffffffu
const DcaAttr dca_attrs[] = {
 /* 0 */ { "bkcolor", s_bkcolor, g_bkcolor, av_bkcolor, 0xFFFFFF, {0, 0xFFFFFF, 0x123456, 0xFF0000, 0x00FF00, 0x0000FF, 0x010203, 0xFEFEFE}, 8, M1, M1, 6, 0x57, 1, 1 },
 /* 1 */ { "textcolor", s_textcolor, g_textcolor, av_textcolor, 0, {0, 0xFFFFFF, 0x123456, 0xFF0000, 0x00FF00, 0x0000FF, 0x010203, 0xFEFEFE}, 8, M1, M1, 0, 0, 1, 1 },
 /* 2 */ { "bkmode", s_bkmode, g_bkmode, av_bkmode, OPAQUE, {TRANSPARENT, OPAQUE}, 2, 0, 0, 6, 0x57, 1, 1 },
 /* 3 */ { "rop2", s_rop2, g_rop2, av_rop2, R2_COPYPEN, {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}, 16, 0, 0, 6, 0x57, 1, 1 },
 /* 4 */ { "polyfill", s_polyfill, g_polyfill, av_polyfill, ALTERNATE, {ALTERNATE, WINDING}, 2, 0, 0, 6, 0x57, 1, 1 },
 /* 5 */ { "stretch", s_stretch, g_stretch, av_stretch, BLACKONWHITE, {BLACKONWHITE, WHITEONBLACK, COLORONCOLOR, HALFTONE}, 4, 0, 0, 6, 0x57, 1, 1 },
 /* 6 */ { "arcdir", s_arcdir, g_arcdir, av_arcdir, AD_COUNTERCLOCKWISE, {AD_COUNTERCLOCKWISE, AD_CLOCKWISE}, 2, 0, 0, 0, 0, 0, 0 },
 /* 7 */ { "textalign", s_textalign, g_textalign, av_textalign, 0, {0, TA_RIGHT, TA_CENTER, TA_BOTTOM, TA_BASELINE, TA_UPDATECP, TA_CENTER|TA_BOTTOM, TA_RIGHT|TA_BASELINE|TA_UPDATECP, TA_RTLREADING|TA_CENTER}, 9, M1, M1, 6, 0x57, 1, 1 },
 /* 8 */ { "charextra", s_charextra, g_charextra, av_charextra, 0, {0, 1, -1, 10, -10, 1000, -1000, 100000, -100000}, 9, 0x80000000u, 0x80000000u, 0x57, 0x57, 1, 1 },
 /* 9 */ { "layout", s_layout, g_layout, av_layout, 0, {0, LAYOUT_RTL, LAYOUT_BITMAPORIENTATIONPRESERVED, LAYOUT_RTL|LAYOUT_BITMAPORIENTATIONPRESERVED}, 4, M1, M1, 6, 0x57, 0, 0 },
 /* 10 */ { "gfxmode", s_gfxmode, g_gfxmode, av_gfxmode, GM_COMPATIBLE, {GM_COMPATIBLE, GM_ADVANCED}, 2, 0, 0, 0x57, 0x57, 1, 0 },
 /* 11 */ { "mapper", s_mapper, g_mapper, av_mapper, 0, {0, 1}, 2, M1, M1, 0x57, 0x57, 0, 0 },
 /* 12 */ { "dcbrush", s_dcbrush, g_dcbrush, av_dcbrush, 0xFFFFFF, {0, 0xFFFFFF, 0x123456, 0xFF0000, 0x00FF00, 0x0000FF}, 6, M1, M1, 0x57, 0x57, 0, 0 },
 /* 13 */ { "dcpen", s_dcpen, g_dcpen, av_dcpen, 0, {0, 0xFFFFFF, 0x123456, 0xFF0000, 0x00FF00, 0x0000FF}, 6, M1, M1, 0x57, 0x57, 0, 0 },
 /* 14 */ { "relabs", s_relabs, g_relabs, av_relabs, ABSOLUTE, {ABSOLUTE, RELATIVE}, 2, 0, 0, 0, 0, 0, 0 },
};
const int dca_nattrs = sizeof dca_attrs / sizeof dca_attrs[0];

#define NEED(a) do { if (!(a)->avail()) { t_skip("missing export for %s", (a)->name); return; } } while (0)
#define NEEDMDC(m) MDC m; do { if (!mdc_make(&m, 16, 16)) { t_skip("cannot create DIB memory DC"); return; } } while (0)
#define SOFT(a, cond, ...) do { if ((a)->rt_savedc) CHECKF(cond, __VA_ARGS__); else if (!(cond)) t_note("%s: " #cond " (unverified behaviour)", (a)->name); } while (0)

static void b_default_memdc(const DcaAttr *a) {
    NEED(a); NEEDMDC(m);
    EXPECT_EQ(a->get(m.dc), a->def);
    mdc_free(&m);
}
static void b_default_cdc_null(const DcaAttr *a) {
    NEED(a);
    HDC d = U(CreateCompatibleDC)(NULL);
    CHECK(d != NULL);
    if (d) { EXPECT_EQ(a->get(d), a->def); U(DeleteDC)(d); }
}
static void b_default_cdc_screen(const DcaAttr *a) {
    NEED(a);
    HDC s = dca_screen(); if (!s) { t_skip("no screen DC"); return; }
    HDC d = U(CreateCompatibleDC)(s);
    CHECK(d != NULL);
    if (d) { EXPECT_EQ(a->get(d), a->def); U(DeleteDC)(d); }
    dca_unscreen(s);
}
static void b_default_screen(const DcaAttr *a) {
    NEED(a);
    HDC s = dca_screen(); if (!s) { t_skip("no screen DC"); return; }
    DWORD v = a->get(s);
    if (a->def == v) CHECK(1); else CHECKF(0, "%s on screen DC: got 0x%lx want 0x%lx", a->name, v, a->def);
    dca_unscreen(s);
}
static void b_default_createdc(const DcaAttr *a) {
    NEED(a);
    HDC d = U(CreateDCW)(L"DISPLAY", NULL, NULL, NULL);
    if (!d) { t_skip("CreateDC DISPLAY failed"); return; }
    EXPECT_EQ(a->get(d), a->def);
    U(DeleteDC)(d);
    HDC i = U(CreateICW)(L"DISPLAY", NULL, NULL, NULL);
    if (i) { EXPECT_EQ(a->get(i), a->def); U(DeleteDC)(i); }
}
static void b_chain(const DcaAttr *a) {
    NEED(a); NEEDMDC(m);
    DWORD last = a->def;
    for (int i = 0; i < a->nv; i++) {
        DWORD prev = a->set(m.dc, a->vals[i]);
        CHECKF(prev == last, "%s val[%d]=0x%lx prev got 0x%lx want 0x%lx", a->name, i, a->vals[i], prev, last);
        CHECKF(a->get(m.dc) == a->vals[i], "%s val[%d] get got 0x%lx", a->name, i, a->get(m.dc));
        last = a->vals[i];
    }
    /* and backwards */
    for (int i = a->nv - 1; i >= 0; i--) {
        DWORD prev = a->set(m.dc, a->vals[i]);
        CHECKF(prev == last, "%s back val[%d] prev got 0x%lx want 0x%lx", a->name, i, prev, last);
        last = a->vals[i];
    }
    mdc_free(&m);
}
static void b_same_value(const DcaAttr *a) {
    NEED(a); NEEDMDC(m);
    DWORD v = dca_nondef(a);
    a->set(m.dc, v);
    for (int i = 0; i < 4; i++) {
        DWORD p = a->set(m.dc, v);
        CHECKF(p == v, "%s repeat %d prev got 0x%lx want 0x%lx", a->name, i, p, v);
        EXPECT_EQ(a->get(m.dc), v);
    }
    mdc_free(&m);
}
static void b_reset_default(const DcaAttr *a) {
    NEED(a); NEEDMDC(m);
    for (int i = 0; i < a->nv; i++) {
        a->set(m.dc, a->vals[i]);
        DWORD p = a->set(m.dc, a->def);
        CHECKF(p == a->vals[i], "%s reset prev got 0x%lx want 0x%lx", a->name, p, a->vals[i]);
        EXPECT_EQ(a->get(m.dc), a->def);
    }
    mdc_free(&m);
}
static void b_get_repeat(const DcaAttr *a) {
    NEED(a); NEEDMDC(m);
    DWORD v = dca_nondef(a);
    a->set(m.dc, v);
    for (int i = 0; i < 50; i++) if (a->get(m.dc) != v) { CHECKF(0, "%s get changed at %d", a->name, i); break; }
    CHECK(1);
    mdc_free(&m);
}
static void b_isolation(const DcaAttr *a) {
    NEED(a); MDC m1, m2;
    if (!mdc_make(&m1, 8, 8)) { t_skip("mdc"); return; }
    if (!mdc_make(&m2, 8, 8)) { mdc_free(&m1); t_skip("mdc"); return; }
    DWORD v = dca_nondef(a);
    a->set(m1.dc, v);
    EXPECT_EQ(a->get(m1.dc), v);
    EXPECT_EQ(a->get(m2.dc), a->def);
    DWORD w = dca_nondef2(a, v);
    if (w != v) {
        a->set(m2.dc, w);
        EXPECT_EQ(a->get(m1.dc), v);
        EXPECT_EQ(a->get(m2.dc), w);
    }
    HDC s = dca_screen();
    if (s) { EXPECT_EQ(a->get(s), a->def); dca_unscreen(s); }
    mdc_free(&m1); mdc_free(&m2);
}
static void b_savedc(const DcaAttr *a) {
    NEED(a); NEEDMDC(m);
    DWORD base = a->get(m.dc), v = dca_nondef(a);
    int n = U(SaveDC)(m.dc);
    CHECK(n > 0);
    a->set(m.dc, v);
    EXPECT_EQ(a->get(m.dc), v);
    CHECK(U(RestoreDC)(m.dc, -1));
    SOFT(a, a->get(m.dc) == base, "%s after RestoreDC got 0x%lx want 0x%lx", a->name, a->get(m.dc), base);
    /* absolute index */
    n = U(SaveDC)(m.dc);
    a->set(m.dc, v);
    CHECK(U(RestoreDC)(m.dc, n));
    SOFT(a, a->get(m.dc) == base, "%s after RestoreDC(abs) got 0x%lx", a->name, a->get(m.dc));
    mdc_free(&m);
}
static void b_savedc_nested(const DcaAttr *a) {
    NEED(a); NEEDMDC(m);
    if (a->nv < 3) { mdc_free(&m); CHECK(1); return; }
    DWORD base = a->get(m.dc);
    DWORD v1 = a->vals[0] != base ? a->vals[0] : a->vals[1];
    DWORD v2 = a->vals[2];
    if (v2 == v1 || v2 == base) v2 = a->vals[a->nv - 1];
    a->set(m.dc, v1);
    int s1 = U(SaveDC)(m.dc);
    a->set(m.dc, v2);
    int s2 = U(SaveDC)(m.dc);
    a->set(m.dc, base);
    CHECK(s1 > 0 && s2 > s1);
    CHECK(U(RestoreDC)(m.dc, -1));
    SOFT(a, a->get(m.dc) == v2, "%s level2 got 0x%lx want 0x%lx", a->name, a->get(m.dc), v2);
    CHECK(U(RestoreDC)(m.dc, -1));
    SOFT(a, a->get(m.dc) == v1, "%s level1 got 0x%lx want 0x%lx", a->name, a->get(m.dc), v1);
    mdc_free(&m);
}
static void b_savedc_skip_levels(const DcaAttr *a) {
    NEED(a); NEEDMDC(m);
    DWORD base = a->get(m.dc), v = dca_nondef(a);
    int s1 = U(SaveDC)(m.dc);
    a->set(m.dc, v);
    int s2 = U(SaveDC)(m.dc);
    a->set(m.dc, base);
    int s3 = U(SaveDC)(m.dc);
    a->set(m.dc, v);
    CHECK(s1 > 0 && s2 > s1 && s3 > s2);
    CHECK(U(RestoreDC)(m.dc, s2));          /* pops levels s3 and s2 */
    SOFT(a, a->get(m.dc) == v, "%s after restore to level 2 got 0x%lx", a->name, a->get(m.dc));
    CHECK(U(RestoreDC)(m.dc, s1));
    SOFT(a, a->get(m.dc) == base, "%s after restore to level 1 got 0x%lx", a->name, a->get(m.dc));
    mdc_free(&m);
}
static void b_select_stock(const DcaAttr *a) {
    NEED(a); NEEDMDC(m);
    DWORD v = dca_nondef(a);
    a->set(m.dc, v);
    static const int stock[] = { BLACK_PEN, WHITE_PEN, NULL_PEN, DC_PEN, WHITE_BRUSH, BLACK_BRUSH, NULL_BRUSH, DC_BRUSH,
                                 SYSTEM_FONT, DEFAULT_GUI_FONT, ANSI_FIXED_FONT, OEM_FIXED_FONT, SYSTEM_FIXED_FONT, DEFAULT_PALETTE };
    HGDIOBJ op = U(GetCurrentObject)(m.dc, OBJ_PEN), ob = U(GetCurrentObject)(m.dc, OBJ_BRUSH), of = U(GetCurrentObject)(m.dc, OBJ_FONT);
    for (unsigned i = 0; i < sizeof stock / sizeof stock[0]; i++) {
        HGDIOBJ o = U(GetStockObject)(stock[i]);
        if (!o) continue;
        if (stock[i] == DEFAULT_PALETTE) { if (U(SelectPalette)) U(SelectPalette)(m.dc, (HPALETTE)o, TRUE); }
        else U(SelectObject)(m.dc, o);
        DWORD g = a->get(m.dc);
        CHECKF(g == v, "%s after selecting stock %d: 0x%lx want 0x%lx", a->name, stock[i], g, v);
    }
    U(SelectObject)(m.dc, op); U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, of);
    EXPECT_EQ(a->get(m.dc), v);
    mdc_free(&m);
}
static void b_select_objects(const DcaAttr *a) {
    NEED(a); NEEDMDC(m);
    DWORD v = dca_nondef(a);
    a->set(m.dc, v);
    HPEN pen = U(CreatePen)(PS_SOLID, 3, 0x0000ff);
    HBRUSH br = U(CreateSolidBrush)(0x00ff00);
    HFONT f = U(CreateFontA)(20, 0, 0, 0, 700, 1, 0, 0, 0, 0, 0, 0, 0, "Arial");
    HBITMAP bm = U(CreateBitmap)(10, 10, 1, 1, NULL);
    CHECK(pen && br && f && bm);
    HGDIOBJ op = U(SelectObject)(m.dc, pen), ob = U(SelectObject)(m.dc, br), of = U(SelectObject)(m.dc, f);
    EXPECT_EQ(a->get(m.dc), v);
    HGDIOBJ obm = U(SelectObject)(m.dc, bm);     /* swap the memory bitmap */
    EXPECT_EQ(a->get(m.dc), v);
    U(SelectObject)(m.dc, obm);
    U(SelectObject)(m.dc, op); U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, of);
    U(DeleteObject)(pen); U(DeleteObject)(br); U(DeleteObject)(f); U(DeleteObject)(bm);
    EXPECT_EQ(a->get(m.dc), v);
    mdc_free(&m);
}
static void b_null_dc(const DcaAttr *a) {
    NEED(a);
    SetLastError(0);
    DWORD r = a->set(NULL, dca_nondef(a));
    DWORD e = GetLastError();
    CHECKF(r == a->bad_set, "%s set(NULL) ret 0x%lx want 0x%lx", a->name, r, a->bad_set);
    if (a->err_null) CHECKF(e == a->err_null, "%s set(NULL) err %lu want %lu", a->name, e, a->err_null);
    else t_note("%s set(NULL) last error = %lu", a->name, e);
    SetLastError(0xABCD);
    r = a->get(NULL);
    e = GetLastError();
    CHECKF(r == a->bad_get, "%s get(NULL) ret 0x%lx want 0x%lx", a->name, r, a->bad_get);
    if (a->get_noerr) CHECKF(e == 0xABCD, "%s get(NULL) touched last error: %lu", a->name, e);
}
static void b_stale_dc(const DcaAttr *a) {
    NEED(a); NEEDMDC(m);
    HDC d = m.dc;
    a->set(d, dca_nondef(a));
    mdc_free(&m);
    SetLastError(0);
    DWORD r = a->set(d, dca_nondef(a));
    DWORD e = GetLastError();
    CHECKF(r == a->bad_set, "%s set(stale) ret 0x%lx want 0x%lx", a->name, r, a->bad_set);
    if (a->err_stale) CHECKF(e == a->err_stale, "%s set(stale) err %lu want %lu", a->name, e, a->err_stale);
    else t_note("%s set(stale) last error = %lu", a->name, e);
    SetLastError(0xABCD);
    r = a->get(d);
    e = GetLastError();
    CHECKF(r == a->bad_get, "%s get(stale) ret 0x%lx want 0x%lx", a->name, r, a->bad_get);
    if (a->get_noerr) CHECKF(e == 0xABCD, "%s get(stale) touched last error: %lu", a->name, e);
}
static void b_garbage_dc(const DcaAttr *a) {
    NEED(a);
    for (int i = 0; i < DCA_NGARBAGE; i++) {
        HDC h = (HDC)dca_garbage[i];
        SetLastError(0);
        DWORD r = a->set(h, dca_nondef(a));
        DWORD e = GetLastError();
        CHECKF(r == a->bad_set, "%s set(garbage %d) ret 0x%lx want 0x%lx", a->name, i, r, a->bad_set);
        if (e == 0) t_note("%s set(garbage %d) left last error 0", a->name, i);
        r = a->get(h);
        CHECKF(r == a->bad_get, "%s get(garbage %d) ret 0x%lx want 0x%lx", a->name, i, r, a->bad_get);
    }
}
static void b_wrong_type(const DcaAttr *a) {
    NEED(a);
    WrongSet w; dca_wrong_make(&w);
    for (int i = 0; i < w.n; i++) {
        if (!w.h[i]) continue;
        SetLastError(0);
        DWORD r = a->set((HDC)w.h[i], dca_nondef(a));
        DWORD e = GetLastError();
        CHECKF(r == a->bad_set, "%s set(%s) ret 0x%lx want 0x%lx", a->name, w.nm[i], r, a->bad_set);
        if (!e) t_note("%s set(%s) did not set last error", a->name, w.nm[i]);
        r = a->get((HDC)w.h[i]);
        CHECKF(r == a->bad_get, "%s get(%s) ret 0x%lx want 0x%lx", a->name, w.nm[i], r, a->bad_get);
    }
    dca_wrong_free(&w);
}
static void b_failed_set_keeps(const DcaAttr *a) {
    NEED(a); NEEDMDC(m);
    DWORD v = dca_nondef(a);
    a->set(m.dc, v);
    a->set(NULL, dca_nondef2(a, v));
    a->set((HDC)(ULONG_PTR)0xDEADBEEF, dca_nondef2(a, v));
    EXPECT_EQ(a->get(m.dc), v);
    mdc_free(&m);
}

typedef struct { const DcaAttr *a; DWORD val, ret, seen; HDC dc; } TCtx;
static DWORD WINAPI thr_set(LPVOID p) {
    TCtx *c = p;
    c->ret = c->a->set(c->dc, c->val);
    c->seen = c->a->get(c->dc);
    return 0;
}
static void b_thread(const DcaAttr *a) {
    NEED(a); NEEDMDC(m);
    DWORD v = dca_nondef(a);
    TCtx c = { a, v, 0xfeedf00d, 0xfeedf00d, m.dc };
    HANDLE t = CreateThread(NULL, 0, thr_set, &c, 0, NULL);
    CHECK(t != NULL);
    if (t) { WaitForSingleObject(t, 10000); CloseHandle(t); }
    CHECKF(c.ret == a->def, "%s thread set prev got 0x%lx", a->name, c.ret);
    CHECKF(c.seen == v, "%s thread get got 0x%lx", a->name, c.seen);
    EXPECT_EQ(a->get(m.dc), v);
    mdc_free(&m);
}
typedef struct { const DcaAttr *a; HDC dc; HBITMAP bmp; } TMk;
static DWORD WINAPI thr_make(LPVOID p) {
    TMk *c = p;
    c->dc = U(CreateCompatibleDC)(NULL);
    if (c->dc) c->a->set(c->dc, dca_nondef(c->a));
    return 0;
}
static void b_thread_made_dc(const DcaAttr *a) {
    NEED(a);
    TMk c = { a, NULL, NULL };
    HANDLE t = CreateThread(NULL, 0, thr_make, &c, 0, NULL);
    CHECK(t != NULL);
    if (t) { WaitForSingleObject(t, 10000); CloseHandle(t); }
    CHECK(c.dc != NULL);
    if (!c.dc) return;
    /* the thread has exited; a GDI DC is process owned and survives */
    EXPECT_EQ(U(GetObjectType)(c.dc), OBJ_MEMDC);
    EXPECT_EQ(a->get(c.dc), dca_nondef(a));
    CHECK(U(DeleteDC)(c.dc));
}
static void b_no_leak(const DcaAttr *a) {
    NEED(a);
    if (!U(GetGuiResources)) { t_skip("no GetGuiResources"); return; }
    DWORD before = dca_gui();
    for (int i = 0; i < 40; i++) {
        HDC d = U(CreateCompatibleDC)(NULL);
        if (!d) { CHECK(0); break; }
        for (int j = 0; j < a->nv; j++) a->set(d, a->vals[j]);
        int s = U(SaveDC)(d); U(RestoreDC)(d, s);
        U(DeleteDC)(d);
    }
    DWORD after = dca_gui();
    CHECKF(after == before, "%s GDI objects before %lu after %lu", a->name, before, after);
}
static void b_loop(const DcaAttr *a) {
    NEED(a); NEEDMDC(m);
    DWORD last = a->def;
    int bad = 0;
    for (int i = 0; i < 2000; i++) {
        DWORD v = a->vals[i % a->nv];
        if (a->set(m.dc, v) != last) bad++;
        last = v;
    }
    CHECKF(bad == 0, "%s loop mismatches %d", a->name, bad);
    EXPECT_EQ(a->get(m.dc), last);
    mdc_free(&m);
}
static void b_emf_dc(const DcaAttr *a) {
    NEED(a);
    if (!U(CreateEnhMetaFileW) || !U(CloseEnhMetaFile) || !U(DeleteEnhMetaFile)) { t_skip("no EMF"); return; }
    HDC d = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!d) { t_skip("CreateEnhMetaFile failed"); return; }
    DWORD v = dca_nondef(a);
    DWORD g0 = a->get(d);
    if (g0 != a->def) t_note("%s default on EMF DC = 0x%lx", a->name, g0);
    DWORD p = a->set(d, v);
    if (p != g0) t_note("%s EMF set prev 0x%lx vs get 0x%lx", a->name, p, g0);
    DWORD g1 = a->get(d);
    if (g1 != v) t_note("%s EMF get after set = 0x%lx want 0x%lx", a->name, g1, v);
    HENHMETAFILE e = U(CloseEnhMetaFile)(d);
    CHECK(e != NULL);
    if (e) U(DeleteEnhMetaFile)(e);
}

#define BEH(X, i, nm) \
    X(i, nm, default_memdc) X(i, nm, default_cdc_null) X(i, nm, default_cdc_screen) X(i, nm, default_screen) \
    X(i, nm, default_createdc) X(i, nm, chain) X(i, nm, same_value) X(i, nm, reset_default) X(i, nm, get_repeat) \
    X(i, nm, isolation) X(i, nm, savedc) X(i, nm, savedc_nested) X(i, nm, savedc_skip_levels) \
    X(i, nm, select_stock) X(i, nm, select_objects) X(i, nm, null_dc) X(i, nm, stale_dc) X(i, nm, garbage_dc) \
    X(i, nm, wrong_type) X(i, nm, failed_set_keeps) X(i, nm, thread) X(i, nm, thread_made_dc) X(i, nm, no_leak) \
    X(i, nm, loop) X(i, nm, emf_dc)
#define GEN(i, nm, beh) TEST(dca_##nm##_##beh) { b_##beh(&dca_attrs[i]); }
#define EACH(i, nm) BEH(GEN, i, nm)
EACH(0, bkcolor) EACH(1, textcolor) EACH(2, bkmode) EACH(3, rop2) EACH(4, polyfill) EACH(5, stretch)
EACH(6, arcdir) EACH(7, textalign) EACH(8, charextra) EACH(9, layout) EACH(10, gfxmode) EACH(11, mapper)
EACH(12, dcbrush) EACH(13, dcpen) EACH(14, relabs)
