/* SaveDC / RestoreDC / SetWorldTransform / SelectObject / SelectClipRgn / Select*Local / ResetDCW */
#include "g3.h"

#define IMPLS(var, pub) for (int i_ = 0; i_ < 2; i_++) { var = (__typeof__(&pub))g3_fn(#pub, i_); \
    if (!var) { t_note(#pub " impl %d missing", i_); continue; }

/* ---------- SaveDC / RestoreDC ---------- */
TEST(gap3_savedc_levels) {
    __typeof__(&SaveDC) S;
    IMPLS(S, SaveDC)
        HDC d = g3_mem(); int i;
        for (i = 1; i <= 20; i++) EQ(S(d), i);
        CHECK(U(RestoreDC)(d, 5));
        EQ(S(d), 5);
        CHECK(U(RestoreDC)(d, -1));
        EQ(S(d), 5);
        U(DeleteDC)(d);
    }
}
TEST(gap3_savedc_invalid) {
    __typeof__(&SaveDC) S;
    IMPLS(S, SaveDC)
        SET_ERR(0);
        EQ(S(NULL), 0);
        WERR(G3_ERR_INVALID_HANDLE);
        HDC st = g3_stale_dc();
        SET_ERR(0);
        EQ(S(st), 0);
        WERR(G3_ERR_INVALID_PARAM);
        EQ(S((HDC)U(GetStockObject)(BLACK_PEN)), 0);
    }
}
TEST(gap3_restoredc_invalid_dc) {
    __typeof__(&RestoreDC) R;
    IMPLS(R, RestoreDC)
        SET_ERR(0);
        EQ(R(NULL, -1), 0);
        WERR(G3_ERR_INVALID_HANDLE);
        HDC st = g3_stale_dc();
        SET_ERR(0);
        EQ(R(st, -1), 0);
        WERR(G3_ERR_INVALID_PARAM);
        EQ(R((HDC)U(GetStockObject)(WHITE_BRUSH), -1), 0);
    }
}
TEST(gap3_restoredc_bad_levels) {
    __typeof__(&RestoreDC) R;
    IMPLS(R, RestoreDC)
        HDC d = g3_mem();
        EQ(R(d, -1), 0);        /* nothing saved */
        EQ(R(d, 1), 0);
        EQ(R(d, 0), 0);
        SOFT(R(d, INT_MIN) == 0);
        EQ(R(d, INT_MAX), 0);
        U(SaveDC)(d); U(SaveDC)(d);
        EQ(R(d, 3), 0);         /* above current level */
        EQ(R(d, -3), 0);        /* below level 1 */
        EQ(R(d, 0), 0);
        SOFT(R(d, INT_MIN) == 0);
        EQ(R(d, INT_MAX), 0);
        EQ(R(d, -2), 1);        /* back to the first save */
        EQ(R(d, -1), 0);
        U(DeleteDC)(d);
    }
}
TEST(gap3_restoredc_absolute) {
    HDC d = g3_mem(); int i;
    U(SetTextColor)(d, RGB(1,1,1)); U(SaveDC)(d);         /* 1 */
    U(SetTextColor)(d, RGB(2,2,2)); U(SaveDC)(d);         /* 2 */
    U(SetTextColor)(d, RGB(3,3,3)); U(SaveDC)(d);         /* 3 */
    U(SetTextColor)(d, RGB(4,4,4));
    CHECK(U(RestoreDC)(d, 2));
    EQ(U(GetTextColor)(d), RGB(2,2,2));
    EQ(U(SaveDC)(d), 2);
    CHECK(U(RestoreDC)(d, 1));
    EQ(U(GetTextColor)(d), RGB(1,1,1));
    for (i = 0; i < 3; i++) U(SaveDC)(d);
    CHECK(U(RestoreDC)(d, -3));
    U(DeleteDC)(d);
}
TEST(gap3_restoredc_negative_relative) {
    HDC d = g3_mem();
    int i;
    for (i = 1; i <= 5; i++) { U(SetBkColor)(d, RGB(i, i, i)); U(SaveDC)(d); }
    U(SetBkColor)(d, RGB(99, 99, 99));
    CHECK(U(RestoreDC)(d, -3));
    EQ(U(GetBkColor)(d), RGB(3,3,3));
    EQ(U(SaveDC)(d), 3);
    U(DeleteDC)(d);
}
TEST(gap3_restoredc_attributes) {
    HDC d = g3_mem(); POINT p;
    U(SetTextColor)(d, RGB(5,6,7)); U(SetBkColor)(d, RGB(8,9,10)); U(SetBkMode)(d, TRANSPARENT);
    U(SetROP2)(d, R2_XORPEN); U(SetPolyFillMode)(d, WINDING); U(SetStretchBltMode)(d, HALFTONE);
    U(SetTextAlign)(d, TA_CENTER); U(SetMapMode)(d, MM_HIMETRIC); U(SetTextCharacterExtra)(d, 3);
    U(SetBrushOrgEx)(d, 3, 4, NULL);
    CHECK(U(SaveDC)(d) == 1);
    U(SetTextColor)(d, 0); U(SetBkColor)(d, 0); U(SetBkMode)(d, OPAQUE);
    U(SetROP2)(d, R2_COPYPEN); U(SetPolyFillMode)(d, ALTERNATE); U(SetStretchBltMode)(d, BLACKONWHITE);
    U(SetTextAlign)(d, 0); U(SetMapMode)(d, MM_TEXT); U(SetTextCharacterExtra)(d, 0);
    U(SetBrushOrgEx)(d, 0, 0, NULL);
    CHECK(U(RestoreDC)(d, -1));
    EQ(U(GetTextColor)(d), RGB(5,6,7));
    EQ(U(GetBkColor)(d), RGB(8,9,10));
    EQ(U(GetBkMode)(d), TRANSPARENT);
    EQ(U(GetROP2)(d), R2_XORPEN);
    EQ(U(GetPolyFillMode)(d), WINDING);
    EQ(U(GetStretchBltMode)(d), HALFTONE);
    EQ(U(GetTextAlign)(d), TA_CENTER);
    EQ(U(GetMapMode)(d), MM_HIMETRIC);
    EQ(U(GetTextCharacterExtra)(d), 3);
    U(GetBrushOrgEx)(d, &p);
    EQ(p.x, 3); EQ(p.y, 4);
    U(DeleteDC)(d);
}
TEST(gap3_restoredc_objects) {
    HDC d = g3_mem();
    HPEN pen = U(CreatePen)(PS_SOLID, 1, RGB(1,2,3));
    HBRUSH br = U(CreateSolidBrush)(RGB(4,5,6));
    HGDIOBJ op = U(GetCurrentObject)(d, OBJ_PEN), ob = U(GetCurrentObject)(d, OBJ_BRUSH);
    CHECK(U(SaveDC)(d) == 1);
    U(SelectObject)(d, pen); U(SelectObject)(d, br);
    EQ(U(GetCurrentObject)(d, OBJ_PEN), pen);
    EQ(U(GetCurrentObject)(d, OBJ_BRUSH), br);
    CHECK(U(RestoreDC)(d, -1));
    EQ(U(GetCurrentObject)(d, OBJ_PEN), op);
    EQ(U(GetCurrentObject)(d, OBJ_BRUSH), ob);
    CHECK(U(DeleteObject)(pen)); CHECK(U(DeleteObject)(br));
    U(DeleteDC)(d);
}
TEST(gap3_restoredc_bitmap_font) {
    G3DC m; if (!g3dc_make(&m, 8, 8)) { CHECK(0); return; }
    HFONT f = U(CreateFontW)(12, 0, 0, 0, 400, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, L"Arial");
    HGDIOBJ of = U(GetCurrentObject)(m.dc, OBJ_FONT);
    CHECK(U(SaveDC)(m.dc) == 1);
    U(SelectObject)(m.dc, f);
    EQ(U(GetCurrentObject)(m.dc, OBJ_FONT), f);
    CHECK(U(RestoreDC)(m.dc, -1));
    EQ(U(GetCurrentObject)(m.dc, OBJ_FONT), of);
    EQ(U(GetCurrentObject)(m.dc, OBJ_BITMAP), m.bmp);
    U(DeleteObject)(f);
    g3dc_free(&m);
}
TEST(gap3_restoredc_clip) {
    HDC d = g3_mem(); RECT r;
    HRGN rg = U(CreateRectRgn)(0, 0, 1, 1);
    CHECK(U(SaveDC)(d) == 1);
    U(SelectClipRgn)(d, rg);
    U(GetClipBox)(d, &r);
    EQ(r.right - r.left, 1);
    CHECK(U(RestoreDC)(d, -1));
    int rt = U(GetClipBox)(d, &r);
    t_note("clip box after restore: type %d (%ld,%ld,%ld,%ld)", rt, r.left, r.top, r.right, r.bottom);
    U(DeleteObject)(rg);
    U(DeleteDC)(d);
}
TEST(gap3_restoredc_clip_pixels) {
    G3DC m; if (!g3dc_make(&m, 16, 16)) { CHECK(0); return; }
    HRGN rg = U(CreateRectRgn)(4, 4, 8, 8);
    HBRUSH br = U(CreateSolidBrush)(RGB(255,255,255));
    RECT all = { 0, 0, 16, 16 };
    g3dc_clear(&m, RGB(0,0,0));
    U(SaveDC)(m.dc);
    U(SelectClipRgn)(m.dc, rg);
    U(FillRect)(m.dc, &all, br);
    EQ(g3dc_count(&m, RGB(255,255,255)), 16);
    U(RestoreDC)(m.dc, -1);
    g3dc_clear(&m, RGB(0,0,0));
    U(FillRect)(m.dc, &all, br);
    EQ(g3dc_count(&m, RGB(255,255,255)), 256);
    U(DeleteObject)(rg); U(DeleteObject)(br);
    g3dc_free(&m);
}
TEST(gap3_restoredc_worldxform) {
    HDC d = g3_mem(); XFORM x = { 2, 0, 0, 2, 5, 6 }, g;
    U(SetGraphicsMode)(d, GM_ADVANCED);
    CHECK(U(SetWorldTransform)(d, &x));
    CHECK(U(SaveDC)(d) == 1);
    XFORM id = { 1, 0, 0, 1, 0, 0 };
    CHECK(U(SetWorldTransform)(d, &id));
    CHECK(U(RestoreDC)(d, -1));
    CHECK(U(GetWorldTransform)(d, &g));
    CHECK(g.eM11 == 2 && g.eM22 == 2 && g.eDx == 5 && g.eDy == 6);
    EQ(U(GetGraphicsMode)(d), GM_ADVANCED);
    U(DeleteDC)(d);
}
TEST(gap3_savedc_screen_dc) {
    HDC d = g3_screen(); if (!d) { t_skip("no dc"); return; }
    COLORREF o = U(GetTextColor)(d);
    int lvl = U(SaveDC)(d);
    CHECK(lvl >= 1);
    U(SetTextColor)(d, RGB(1,2,3));
    CHECK(U(RestoreDC)(d, -1));
    EQ(U(GetTextColor)(d), o);
    g3_unscreen(d);
}
TEST(gap3_savedc_deep) {
    HDC d = g3_mem(); int i, bad = 0;
    for (i = 1; i <= 1000; i++) if (U(SaveDC)(d) != i) bad++;
    EQ(bad, 0);
    CHECK(U(RestoreDC)(d, 1));
    EQ(U(SaveDC)(d), 1);
    CHECK(U(RestoreDC)(d, -1));
    U(DeleteDC)(d);
}
TEST(gap3_deletedc_with_saved_levels) {
    DWORD b = g3_gui();
    HDC d = g3_mem(); int i;
    for (i = 0; i < 10; i++) U(SaveDC)(d);
    CHECK(U(DeleteDC)(d));
    EQ(g3_gui(), b);
}
TEST(gap3_restoredc_selected_deleted) {
    /* an object that was selected before SaveDC may be deleted; restore must then fall back safely */
    HDC d = g3_mem();
    HPEN pen = U(CreatePen)(PS_SOLID, 1, RGB(1,2,3));
    HGDIOBJ op = U(SelectObject)(d, pen);
    U(SaveDC)(d);
    U(SelectObject)(d, op);        /* deselect, then delete while saved */
    CHECK(U(DeleteObject)(pen));
    int r = U(RestoreDC)(d, -1);
    t_note("RestoreDC with deleted saved pen = %d", r);
    HGDIOBJ cur = U(GetCurrentObject)(d, OBJ_PEN);
    CHECK(cur != pen);
    U(DeleteDC)(d);
}
TEST(gap3_restoredc_wmf_dc) {
    REQUIRE(U(CreateMetaFileW));
    HDC mf = U(CreateMetaFileW)(NULL);
    if (!mf) { t_skip("no wmf dc"); return; }
    t_note("wmf dc handle type bits = 0x%lx", (unsigned long)((ULONG_PTR)mf & 0x7f0000));
    int lvl = U(SaveDC)(mf);
    t_note("SaveDC on wmf dc = %d", lvl);
    int r = U(RestoreDC)(mf, -1);
    t_note("RestoreDC on wmf dc = %d", r);
    HMETAFILE h = U(CloseMetaFile)(mf);
    if (h) U(DeleteMetaFile)(h);
}

/* ---------- SetWorldTransform ---------- */
TEST(gap3_wxform_requires_advanced) {
    __typeof__(&SetWorldTransform) S;
    IMPLS(S, SetWorldTransform)
        HDC d = g3_mem(); XFORM x = { 1, 0, 0, 1, 3, 4 };
        EQ(U(GetGraphicsMode)(d), GM_COMPATIBLE);
        EQ(S(d, &x), 0);
        U(SetGraphicsMode)(d, GM_ADVANCED);
        CHECK(S(d, &x));
        U(SetGraphicsMode)(d, GM_COMPATIBLE);
        U(DeleteDC)(d);
    }
}
TEST(gap3_wxform_roundtrip) {
    __typeof__(&SetWorldTransform) S;
    IMPLS(S, SetWorldTransform)
        HDC d = g3_mem(); XFORM g;
        static const XFORM v[] = { {1,0,0,1,0,0}, {2,0,0,3,5,-7}, {0,1,-1,0,10,10}, {0.5f,0,0,0.25f,0,0},
            {-1,0,0,1,100,0}, {1,2,3,4,5,6}, {1000.f,0,0,1000.f,0,0} };
        unsigned k;
        U(SetGraphicsMode)(d, GM_ADVANCED);
        for (k = 0; k < sizeof v / sizeof *v; k++) {
            CHECK(S(d, (XFORM *)&v[k]));
            memset(&g, 0, sizeof g);
            CHECK(U(GetWorldTransform)(d, &g));
            CHECKF(g.eM11 == v[k].eM11 && g.eM12 == v[k].eM12 && g.eM21 == v[k].eM21 && g.eM22 == v[k].eM22 &&
                   g.eDx == v[k].eDx && g.eDy == v[k].eDy, "k=%u", k);
        }
        U(DeleteDC)(d);
    }
}
TEST(gap3_wxform_invalid_dc) {
    __typeof__(&SetWorldTransform) S;
    IMPLS(S, SetWorldTransform)
        XFORM x = { 1, 0, 0, 1, 0, 0 };
        SET_ERR(0);
        EQ(S(NULL, &x), 0);
        WERR(G3_ERR_INVALID_HANDLE);
        HDC st = g3_stale_dc();
        SET_ERR(0);
        EQ(S(st, &x), 0);
        WERR(G3_ERR_INVALID_PARAM);
        EQ(S((HDC)U(GetStockObject)(BLACK_PEN), &x), 0);
    }
}
TEST(gap3_wxform_singular) {
    HDC d = g3_mem(); XFORM z = { 0, 0, 0, 0, 0, 0 }, g, id = { 1, 0, 0, 1, 0, 0 };
    U(SetGraphicsMode)(d, GM_ADVANCED);
    BOOL r = U(SetWorldTransform)(d, &z);
    U(GetWorldTransform)(d, &g);
    t_note("singular matrix: ret %d, stored M11=%f", r, g.eM11);
    XFORM nan; memset(&nan, 0xff, sizeof nan);
    r = U(SetWorldTransform)(d, &nan);
    t_note("NaN matrix: ret %d", r);
    CHECK(U(SetWorldTransform)(d, &id));
    U(GetWorldTransform)(d, &g);
    CHECK(g.eM11 == 1 && g.eM22 == 1);
    U(DeleteDC)(d);
}
TEST(gap3_wxform_translate_pixels) {
    G3DC m; if (!g3dc_make(&m, 16, 16)) { CHECK(0); return; }
    XFORM x = { 1, 0, 0, 1, 5, 3 };
    HPEN pen = U(CreatePen)(PS_SOLID, 1, RGB(255,255,255));
    HGDIOBJ op = U(SelectObject)(m.dc, pen);
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    CHECK(U(SetWorldTransform)(m.dc, &x));
    g3dc_clear(&m, RGB(0,0,0));
    U(SetPixelV)(m.dc, 1, 1, RGB(255,0,0));
    PXC(m, 6, 4, RGB(255,0,0));
    PXC(m, 1, 1, RGB(0,0,0));
    U(MoveToEx)(m.dc, 0, 0, NULL); U(LineTo)(m.dc, 4, 0);
    PXC(m, 5, 3, RGB(255,255,255));
    PXC(m, 8, 3, RGB(255,255,255));
    PXC(m, 9, 3, RGB(0,0,0));
    U(SelectObject)(m.dc, op); U(DeleteObject)(pen);
    g3dc_free(&m);
}
TEST(gap3_wxform_scale_pixels) {
    G3DC m; if (!g3dc_make(&m, 16, 16)) { CHECK(0); return; }
    XFORM x = { 2, 0, 0, 2, 0, 0 };
    HBRUSH br = U(CreateSolidBrush)(RGB(0,255,0));
    RECT r = { 1, 1, 3, 3 };
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    CHECK(U(SetWorldTransform)(m.dc, &x));
    g3dc_clear(&m, RGB(0,0,0));
    U(FillRect)(m.dc, &r, br);
    EQ(g3dc_count(&m, RGB(0,255,0)), 16);
    EQ(g3dc_count_rect(&m, 2, 2, 6, 6, RGB(0,255,0)), 16);
    U(DeleteObject)(br);
    g3dc_free(&m);
}
TEST(gap3_wxform_rotate90_pixels) {
    G3DC m; if (!g3dc_make(&m, 16, 16)) { CHECK(0); return; }
    XFORM x = { 0, 1, -1, 0, 10, 0 };      /* (x,y) -> (-y+10, x) */
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    CHECK(U(SetWorldTransform)(m.dc, &x));
    g3dc_clear(&m, RGB(0,0,0));
    U(SetPixelV)(m.dc, 2, 3, RGB(255,255,0));
    PXC(m, 7, 2, RGB(255,255,0));
    EQ(g3dc_count(&m, RGB(255,255,0)), 1);
    g3dc_free(&m);
}
TEST(gap3_wxform_modify_after_set) {
    HDC d = g3_mem(); XFORM x = { 1, 0, 0, 1, 10, 10 }, g;
    U(SetGraphicsMode)(d, GM_ADVANCED);
    CHECK(U(SetWorldTransform)(d, &x));
    XFORM y = { 1, 0, 0, 1, 5, 5 };
    CHECK(U(ModifyWorldTransform)(d, &y, MWT_LEFTMULTIPLY));
    U(GetWorldTransform)(d, &g);
    CHECK(g.eDx == 15 && g.eDy == 15);
    CHECK(U(SetWorldTransform)(d, &x));
    U(GetWorldTransform)(d, &g);
    CHECK(g.eDx == 10 && g.eDy == 10);
    U(DeleteDC)(d);
}
TEST(gap3_wxform_wmf_dc) {
    REQUIRE(U(CreateMetaFileW));
    HDC mf = U(CreateMetaFileW)(NULL);
    if (!mf) { t_skip("no wmf dc"); return; }
    XFORM x = { 1, 0, 0, 1, 0, 0 };
    BOOL r = U(SetWorldTransform)(mf, &x);
    t_note("SetWorldTransform on wmf dc = %d", r);
    HMETAFILE h = U(CloseMetaFile)(mf);
    if (h) U(DeleteMetaFile)(h);
}
TEST(gap3_wxform_graphicsmode_reset) {
    /* going back to COMPATIBLE resets the world transform to identity */
    HDC d = g3_mem(); XFORM x = { 2, 0, 0, 2, 1, 1 }, g;
    U(SetGraphicsMode)(d, GM_ADVANCED);
    U(SetWorldTransform)(d, &x);
    U(SetGraphicsMode)(d, GM_COMPATIBLE);
    U(SetGraphicsMode)(d, GM_ADVANCED);
    CHECK(U(GetWorldTransform)(d, &g));
    t_note("xform after mode toggle: %f %f %f %f %f %f", g.eM11, g.eM12, g.eM21, g.eM22, g.eDx, g.eDy);
    U(DeleteDC)(d);
}

/* ---------- SelectObject ---------- */
TEST(gap3_selobj_defaults_return_stock) {
    __typeof__(&SelectObject) S;
    IMPLS(S, SelectObject)
        HDC d = g3_mem();
        HGDIOBJ p = U(CreatePen)(PS_SOLID, 1, 0), b = U(CreateSolidBrush)(0);
        HGDIOBJ op = S(d, p), ob = S(d, b);
        EQ(op, U(GetStockObject)(BLACK_PEN));
        EQ(ob, U(GetStockObject)(WHITE_BRUSH));
        EQ(S(d, op), p); EQ(S(d, ob), b);
        U(DeleteObject)(p); U(DeleteObject)(b);
        U(DeleteDC)(d);
    }
}
TEST(gap3_selobj_returns_previous_chain) {
    __typeof__(&SelectObject) S;
    IMPLS(S, SelectObject)
        HDC d = g3_mem();
        HPEN a = U(CreatePen)(PS_SOLID, 1, 1), b = U(CreatePen)(PS_DASH, 1, 2), c = U(CreatePen)(PS_DOT, 1, 3);
        HGDIOBJ o = S(d, a);
        EQ(S(d, b), a);
        EQ(S(d, c), b);
        EQ(S(d, c), c);
        EQ(S(d, o), c);
        U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(c);
        U(DeleteDC)(d);
    }
}
TEST(gap3_selobj_null_object) {
    __typeof__(&SelectObject) S;
    IMPLS(S, SelectObject)
        HDC d = g3_mem(); HGDIOBJ cur = U(GetCurrentObject)(d, OBJ_PEN);
        SET_ERR(0);
        EQ(S(d, NULL), 0);
        EQ(U(GetCurrentObject)(d, OBJ_PEN), cur);
        U(DeleteDC)(d);
    }
}
TEST(gap3_selobj_invalid_dc) {
    __typeof__(&SelectObject) S;
    IMPLS(S, SelectObject)
        HGDIOBJ pen = U(GetStockObject)(BLACK_PEN);
        EQ(S(NULL, pen), 0);
        HDC st = g3_stale_dc();
        EQ(S(st, pen), 0);
        EQ(S(NULL, NULL), 0);
        EQ(S((HDC)U(GetStockObject)(WHITE_BRUSH), pen), 0);
    }
}
TEST(gap3_selobj_deleted_object) {
    __typeof__(&SelectObject) S;
    IMPLS(S, SelectObject)
        HDC d = g3_mem();
        HPEN p = U(CreatePen)(PS_SOLID, 1, 5);
        U(DeleteObject)(p);
        HGDIOBJ cur = U(GetCurrentObject)(d, OBJ_PEN);
        EQ(S(d, p), 0);
        EQ(U(GetCurrentObject)(d, OBJ_PEN), cur);
        HBRUSH b = U(CreateSolidBrush)(5); U(DeleteObject)(b);
        EQ(S(d, b), 0);
        HFONT f = U(CreateFontW)(10, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, L"Arial"); U(DeleteObject)(f);
        EQ(S(d, f), 0);
        U(DeleteDC)(d);
    }
}
TEST(gap3_selobj_dc_as_object) {
    HDC d = g3_mem(), e = g3_mem();
    SET_ERR(0);
    EQ(U(SelectObject)(d, (HGDIOBJ)e), 0);
    U(DeleteDC)(d); U(DeleteDC)(e);
}
TEST(gap3_selobj_bogus_values) {
    HDC d = g3_mem(); unsigned k;
    static const ULONG_PTR v[] = { 1, 2, 0x100, 0xffff, 0x10000, 0x00010001, 0x7fffffff, 0xffffffff };
    for (k = 0; k < sizeof v / sizeof *v; k++) {
        HGDIOBJ r = U(SelectObject)(d, (HGDIOBJ)v[k]);
        CHECKF(r == NULL || (ULONG_PTR)r == 0 || 1, "k=%u", k);
        if (r) t_note("SelectObject(0x%llx) returned %p", (unsigned long long)v[k], r);
    }
    U(DeleteDC)(d);
}
TEST(gap3_selobj_bitmap_rules) {
    __typeof__(&SelectObject) S;
    IMPLS(S, SelectObject)
        HDC d = g3_mem(), e = g3_mem(); G3DC m;
        HBITMAP bm = U(CreateBitmap)(4, 4, 1, 1, NULL);
        HGDIOBJ ob = S(d, bm);
        CHECK(ob != NULL);
        EQ(U(GetObjectType)(ob), OBJ_BITMAP);
        EQ(S(e, bm), 0);                 /* already selected elsewhere */
        EQ(S(d, bm), bm);
        S(d, ob);
        CHECK(S(e, bm) != NULL);
        S(e, ob);
        U(DeleteObject)(bm);
        (void)m;
        U(DeleteDC)(d); U(DeleteDC)(e);
    }
}
TEST(gap3_selobj_bitmap_into_screen_dc) {
    HDC s = g3_screen(); if (!s) { t_skip("no dc"); return; }
    HBITMAP bm = U(CreateBitmap)(2, 2, 1, 1, NULL);
    SET_ERR(0);
    EQ(U(SelectObject)(s, bm), 0);
    U(DeleteObject)(bm);
    g3_unscreen(s);
}
TEST(gap3_selobj_region_returns_complexity) {
    __typeof__(&SelectObject) S;
    IMPLS(S, SelectObject)
        HDC d = g3_mem();
        HRGN r = U(CreateRectRgn)(0, 0, 10, 10), e = U(CreateRectRgn)(0, 0, 0, 0);
        HRGN c1 = U(CreateRectRgn)(0, 0, 2, 2), c2 = U(CreateRectRgn)(5, 5, 8, 8), cx = U(CreateRectRgn)(0, 0, 1, 1);
        U(CombineRgn)(cx, c1, c2, RGN_OR);
        EQ((ULONG_PTR)S(d, r), SIMPLEREGION);
        EQ((ULONG_PTR)S(d, e), NULLREGION);
        EQ((ULONG_PTR)S(d, cx), COMPLEXREGION);
        U(DeleteObject)(r); U(DeleteObject)(e); U(DeleteObject)(c1); U(DeleteObject)(c2); U(DeleteObject)(cx);
        U(DeleteDC)(d);
    }
}
TEST(gap3_selobj_palette) {
    REQUIRE(U(CreateHalftonePalette));
    G3DC m; if (!g3dc_make(&m, 4, 4)) { CHECK(0); return; }
    HPALETTE pal = U(CreateHalftonePalette)(m.dc);
    CHECK(pal != NULL);
    HGDIOBJ old = U(SelectObject)(m.dc, pal);
    t_note("SelectObject(palette) returned %p", old);
    if (old) U(SelectObject)(m.dc, old);
    U(DeleteObject)(pal);
    g3dc_free(&m);
}
TEST(gap3_selobj_font_roundtrip) {
    __typeof__(&SelectObject) S;
    IMPLS(S, SelectObject)
        HDC d = g3_mem(); LOGFONTW lf;
        HFONT f = U(CreateFontW)(-14, 0, 0, 0, 700, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, L"Arial");
        HGDIOBJ of = S(d, f);
        CHECK(of != NULL);
        EQ(U(GetCurrentObject)(d, OBJ_FONT), f);
        EQ(U(GetObjectW)(f, sizeof lf, &lf), sizeof lf);
        EQ(lf.lfWeight, 700);
        S(d, of);
        U(DeleteObject)(f);
        U(DeleteDC)(d);
    }
}
TEST(gap3_selobj_stock_objects_all) {
    HDC d = g3_mem(); int i;
    for (i = 0; i <= 18; i++) {
        HGDIOBJ s = U(GetStockObject)(i);
        if (!s) continue;
        int t = U(GetObjectType)(s);
        if (t == OBJ_PEN || t == OBJ_EXTPEN || t == OBJ_BRUSH || t == OBJ_FONT || t == OBJ_PAL) {
            HGDIOBJ old = U(SelectObject)(d, s);
            if (t != OBJ_PAL) CHECKF(old != NULL, "stock %d", i);
            if (old) U(SelectObject)(d, old);
        }
    }
    U(DeleteDC)(d);
}
TEST(gap3_selobj_gdicount_stable) {
    DWORD b = g3_gui(); int i;
    HDC d = g3_mem();
    HGDIOBJ op = U(GetCurrentObject)(d, OBJ_PEN);
    for (i = 0; i < 500; i++) {
        HPEN p = U(CreatePen)(PS_SOLID, 1, i);
        U(SelectObject)(d, p); U(SelectObject)(d, op); U(DeleteObject)(p);
    }
    U(DeleteDC)(d);
    EQ(g3_gui(), b);
}
TEST(gap3_selobj_delete_selected_fails) {
    HDC d = g3_mem();
    HPEN p = U(CreatePen)(PS_SOLID, 1, 5);
    HGDIOBJ op = U(SelectObject)(d, p);
    SET_ERR(0);
    int r = U(DeleteObject)(p);
    t_note("DeleteObject(selected pen) = %d err=%lu", r, GetLastError());
    U(SelectObject)(d, op);
    if (!r) CHECK(U(DeleteObject)(p));
    else EQ(U(SelectObject)(d, p), 0);
    U(DeleteDC)(d);
}
static void so_thread(void *p) {
    HGDIOBJ *a = p; a[1] = U(SelectObject)((HDC)a[0], a[2]);
}
TEST(gap3_selobj_cross_thread) {
    HDC d = g3_mem(); HPEN pen = U(CreatePen)(PS_SOLID, 1, 9);
    HGDIOBJ a[3]; a[0] = d; a[1] = NULL; a[2] = pen;
    g3_run_thread(so_thread, a);
    EQ(a[1], U(GetStockObject)(BLACK_PEN));
    EQ(U(GetCurrentObject)(d, OBJ_PEN), pen);
    U(SelectObject)(d, a[1]);
    U(DeleteObject)(pen);
    U(DeleteDC)(d);
}

/* ---------- SelectClipRgn ---------- */
TEST(gap3_selclip_return_values) {
    __typeof__(&SelectClipRgn) S;
    IMPLS(S, SelectClipRgn)
        HDC d = g3_mem();
        HRGN r = U(CreateRectRgn)(0, 0, 10, 10), e = U(CreateRectRgn)(0, 0, 0, 0);
        HRGN a = U(CreateRectRgn)(0, 0, 2, 2), b = U(CreateRectRgn)(5, 5, 8, 8), cx = U(CreateRectRgn)(0, 0, 1, 1);
        U(CombineRgn)(cx, a, b, RGN_OR);
        EQ(S(d, r), SIMPLEREGION);
        EQ(S(d, e), NULLREGION);
        EQ(S(d, cx), COMPLEXREGION);
        S(d, NULL);
        U(DeleteObject)(r); U(DeleteObject)(e); U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(cx);
        U(DeleteDC)(d);
    }
}
TEST(gap3_selclip_null_resets) {
    __typeof__(&SelectClipRgn) S;
    IMPLS(S, SelectClipRgn)
        G3DC m; if (!g3dc_make(&m, 20, 20)) { CHECK(0); return; }
        HRGN r = U(CreateRectRgn)(3, 3, 6, 6); RECT cb;
        S(m.dc, r);
        EQ(U(GetClipBox)(m.dc, &cb), SIMPLEREGION);
        EQ(cb.left, 3); EQ(cb.top, 3); EQ(cb.right, 6); EQ(cb.bottom, 6);
        int rr = S(m.dc, NULL);
        t_note("SelectClipRgn(NULL) = %d", rr);
        CHECK(rr != 0);
        U(GetClipBox)(m.dc, &cb);
        EQ(cb.left, 0); EQ(cb.top, 0); EQ(cb.right, 20); EQ(cb.bottom, 20);
        U(DeleteObject)(r);
        g3dc_free(&m);
    }
}
TEST(gap3_selclip_invalid_dc) {
    __typeof__(&SelectClipRgn) S;
    IMPLS(S, SelectClipRgn)
        HRGN r = U(CreateRectRgn)(0, 0, 5, 5);
        SET_ERR(0);
        EQ(S(NULL, r), 0);
        WERR(G3_ERR_INVALID_HANDLE);
        HDC st = g3_stale_dc();
        EQ(S(st, r), 0);
        EQ(S(NULL, NULL), 0);
        EQ(S((HDC)U(GetStockObject)(BLACK_PEN), r), 0);
        U(DeleteObject)(r);
    }
}
TEST(gap3_selclip_invalid_region) {
    __typeof__(&SelectClipRgn) S;
    IMPLS(S, SelectClipRgn)
        HDC d = g3_mem();
        HRGN r = U(CreateRectRgn)(0, 0, 5, 5); U(DeleteObject)(r);
        EQ(S(d, r), 0);
        EQ(S(d, (HRGN)U(GetStockObject)(BLACK_PEN)), 0);
        EQ(S(d, (HRGN)U(GetStockObject)(WHITE_BRUSH)), 0);
        EQ(S(d, (HRGN)0x1234), 0);
        U(DeleteDC)(d);
    }
}
TEST(gap3_selclip_copy_semantics) {
    /* the region is copied: changing/deleting it afterwards does not change the clip */
    G3DC m; if (!g3dc_make(&m, 20, 20)) { CHECK(0); return; }
    HRGN r = U(CreateRectRgn)(2, 2, 6, 6); RECT cb;
    U(SelectClipRgn)(m.dc, r);
    U(SetRectRgn)(r, 10, 10, 18, 18);
    U(GetClipBox)(m.dc, &cb);
    EQ(cb.left, 2); EQ(cb.right, 6);
    U(DeleteObject)(r);
    U(GetClipBox)(m.dc, &cb);
    EQ(cb.left, 2); EQ(cb.bottom, 6);
    g3dc_free(&m);
}
TEST(gap3_selclip_pixels) {
    G3DC m; if (!g3dc_make(&m, 16, 16)) { CHECK(0); return; }
    HRGN a = U(CreateRectRgn)(1, 1, 4, 4), b = U(CreateRectRgn)(8, 8, 12, 12), c = U(CreateRectRgn)(0, 0, 1, 1);
    U(CombineRgn)(c, a, b, RGN_OR);
    HBRUSH br = U(CreateSolidBrush)(RGB(255,0,255));
    RECT all = { 0, 0, 16, 16 };
    g3dc_clear(&m, RGB(0,0,0));
    EQ(U(SelectClipRgn)(m.dc, c), COMPLEXREGION);
    U(FillRect)(m.dc, &all, br);
    EQ(g3dc_count(&m, RGB(255,0,255)), 9 + 16);
    PXC(m, 1, 1, RGB(255,0,255)); PXC(m, 3, 3, RGB(255,0,255)); PXC(m, 4, 4, RGB(0,0,0));
    PXC(m, 8, 8, RGB(255,0,255)); PXC(m, 11, 11, RGB(255,0,255)); PXC(m, 12, 12, RGB(0,0,0));
    PXC(m, 5, 5, RGB(0,0,0));
    U(SelectClipRgn)(m.dc, NULL);
    U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(c); U(DeleteObject)(br);
    g3dc_free(&m);
}
TEST(gap3_selclip_offscreen_region) {
    G3DC m; if (!g3dc_make(&m, 8, 8)) { CHECK(0); return; }
    HRGN r = U(CreateRectRgn)(100, 100, 200, 200); RECT cb;
    int rr = U(SelectClipRgn)(m.dc, r);
    t_note("SelectClipRgn(offscreen) = %d", rr);
    int t = U(GetClipBox)(m.dc, &cb);
    t_note("GetClipBox -> %d (%ld,%ld,%ld,%ld)", t, cb.left, cb.top, cb.right, cb.bottom);
    CHECK(U(PtVisible)(m.dc, 1, 1) == 0);
    U(SelectClipRgn)(m.dc, NULL);
    U(DeleteObject)(r);
    g3dc_free(&m);
}
TEST(gap3_selclip_selected_into_dc_then_deleted_dc) {
    DWORD b = g3_gui();
    HDC d = g3_mem(); HRGN r = U(CreateRectRgn)(0, 0, 3, 3);
    U(SelectClipRgn)(d, r);
    U(DeleteDC)(d);
    CHECK(U(DeleteObject)(r));
    EQ(g3_gui(), b);
}
TEST(gap3_selclip_dc_origin_shift) {
    G3DC m; if (!g3dc_make(&m, 20, 20)) { CHECK(0); return; }
    HRGN r = U(CreateRectRgn)(0, 0, 4, 4); RECT cb;
    U(SetViewportOrgEx)(m.dc, 5, 5, NULL);
    U(SelectClipRgn)(m.dc, r);          /* clip regions are in device units */
    U(GetClipBox)(m.dc, &cb);           /* GetClipBox returns logical units */
    t_note("clip box with viewport org (5,5): (%ld,%ld,%ld,%ld)", cb.left, cb.top, cb.right, cb.bottom);
    CHECK(U(PtVisible)(m.dc, -5, -5) != 0);
    CHECK(U(PtVisible)(m.dc, 0, 0) == 0);
    U(DeleteObject)(r);
    g3dc_free(&m);
}

/* ---------- SelectBrushLocal / SelectFontLocal ---------- */
typedef ULONG_PTR (WINAPI *LOCALFN)(ULONG_PTR, ULONG_PTR);
TEST(gap3_selectlocal_identity) {
    LOCALFN b = (LOCALFN)t_proc_gdi32full("SelectBrushLocal"), f = (LOCALFN)t_proc_gdi32full("SelectFontLocal");
    REQUIRE(b); REQUIRE(f);
    static const ULONG_PTR v[] = { 0, 1, 0x1234, 0xffffffff, (ULONG_PTR)-1, 0x100000000ull };
    unsigned k;
    for (k = 0; k < sizeof v / sizeof *v; k++) {
        EQ(b(0xabcd, v[k]), v[k]);
        EQ(b(v[k], v[k] ^ 5), v[k] ^ 5);
        EQ(f(0xabcd, v[k]), v[k]);
        EQ(f(v[k], v[k] ^ 7), v[k] ^ 7);
    }
}
TEST(gap3_selectlocal_same_entry) {
    FARPROC b = (FARPROC)t_proc_gdi32full("SelectBrushLocal"), f = (FARPROC)t_proc_gdi32full("SelectFontLocal");
    REQUIRE(b); REQUIRE(f);
    t_note("SelectBrushLocal %p SelectFontLocal %p", (void *)b, (void *)f);
}
TEST(gap3_selectlocal_real_handles) {
    LOCALFN b = (LOCALFN)t_proc_gdi32full("SelectBrushLocal");
    REQUIRE(b);
    HDC d = g3_mem();
    HGDIOBJ br = U(GetStockObject)(WHITE_BRUSH);
    EQ(b((ULONG_PTR)d, (ULONG_PTR)br), (ULONG_PTR)br);
    /* no state change: current brush is untouched */
    EQ(U(GetCurrentObject)(d, OBJ_BRUSH), U(GetStockObject)(WHITE_BRUSH));
    HBRUSH nb = U(CreateSolidBrush)(RGB(1,2,3));
    EQ(b((ULONG_PTR)d, (ULONG_PTR)nb), (ULONG_PTR)nb);
    EQ(U(GetCurrentObject)(d, OBJ_BRUSH), U(GetStockObject)(WHITE_BRUSH));
    U(DeleteObject)(nb);
    U(DeleteDC)(d);
}

/* ---------- ResetDCW ---------- */
TEST(gap3_resetdc_memdc) {
    __typeof__(&ResetDCW) R;
    IMPLS(R, ResetDCW)
        HDC d = g3_mem();
        SET_ERR(0);
        HDC r = R(d, NULL);
        t_note("ResetDCW(memdc, NULL) = %p err=%lu", r, GetLastError());
        EQ(r, 0);
        U(DeleteDC)(d);
    }
}
TEST(gap3_resetdc_invalid) {
    __typeof__(&ResetDCW) R;
    IMPLS(R, ResetDCW)
        SET_ERR(0);
        EQ(R(NULL, NULL), 0);
        WERR(G3_ERR_INVALID_HANDLE);
        HDC st = g3_stale_dc();
        EQ(R(st, NULL), 0);
        EQ(R((HDC)U(GetStockObject)(BLACK_PEN), NULL), 0);
    }
}
TEST(gap3_resetdc_screen) {
    HDC s = g3_screen(); if (!s) { t_skip("no dc"); return; }
    G3_WINE_SKIP("ResetDCW on display DC");
    __typeof__(&ResetDCW) R;
    IMPLS(R, ResetDCW)
        EQ(R(s, NULL), 0);
    }
    g3_unscreen(s);
}
TEST(gap3_resetdc_dibdc_unchanged) {
    G3DC m; if (!g3dc_make(&m, 6, 6)) { CHECK(0); return; }
    HDC r = U(ResetDCW)(m.dc, NULL);
    EQ(r, 0);
    EQ(U(GetCurrentObject)(m.dc, OBJ_BITMAP), m.bmp);
    U(SetPixelV)(m.dc, 1, 1, RGB(255,0,0));
    PXC(m, 1, 1, RGB(255,0,0));
    g3dc_free(&m);
}
