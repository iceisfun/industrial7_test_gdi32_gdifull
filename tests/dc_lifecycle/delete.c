/* DeleteDC, ResetDC, GetObjectType on DCs, handle lifetime, cross-thread use */
#include "dcl.h"

/* ---------------------------------------------------------------- DeleteDC */

TEST(dcl_delete_basic_memdc) {
    REQUIRE(U(DeleteDC));
    HDC dc = dcl_mem();
    CHECK(dc != NULL);
    CHECK(U(DeleteDC)(dc));
}

TEST(dcl_delete_basic_createdc) {
    REQUIRE(U(DeleteDC));
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    CHECK(dc != NULL);
    CHECK(U(DeleteDC)(dc));
}

TEST(dcl_delete_basic_ic) {
    REQUIRE(U(DeleteDC));
    HDC dc = U(CreateICA)("DISPLAY", NULL, NULL, NULL);
    CHECK(dc != NULL);
    CHECK(U(DeleteDC)(dc));
}

TEST(dcl_delete_null) {
    REQUIRE(U(DeleteDC));
    SET_ERR(0xdeadbeef);
    BOOL r = U(DeleteDC)(NULL);
    CHECK(!r);
    t_note("DeleteDC(NULL) last error %lu", GetLastError());
}

TEST(dcl_delete_bogus_handles) {
    REQUIRE(U(DeleteDC));
    static const ULONG_PTR h[] = { 0xdeadbeef, 0x1, 0x2, 0xffffffff, 0x10001, 0x7fffffff };
    for (unsigned i = 0; i < sizeof h / sizeof *h; i++) {
        SET_ERR(0xdeadbeef);
        BOOL r = U(DeleteDC)((HDC)h[i]);
        CHECKF(!r, "handle 0x%llx", (unsigned long long)h[i]);
        t_note("DeleteDC(0x%llx) error %lu", (unsigned long long)h[i], GetLastError());
    }
}

TEST(dcl_delete_twice) {
    REQUIRE(U(DeleteDC));
    HDC dc = dcl_mem();
    CHECK(U(DeleteDC)(dc));
    SET_ERR(0xdeadbeef);
    CHECK(!U(DeleteDC)(dc));
    t_note("second DeleteDC error %lu", GetLastError());
    CHECK(!U(DeleteDC)(dc));
}

TEST(dcl_delete_twice_createdc) {
    REQUIRE(U(DeleteDC));
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    CHECK(U(DeleteDC)(dc));
    CHECK(!U(DeleteDC)(dc));
}

TEST(dcl_delete_stock_object_refused) {
    /* a stock object is not a DC; it must survive the attempt */
    REQUIRE(U(DeleteDC));
    HGDIOBJ st = U(GetStockObject)(BLACK_PEN);
    BOOL r = U(DeleteDC)((HDC)st);
    t_note("DeleteDC(stock pen) -> %d", r);
    EQ(U(GetObjectType)(st), OBJ_PEN);
}

TEST(dcl_delete_non_dc_objects) {
    REQUIRE(U(DeleteDC));
    HGDIOBJ o[3] = { U(CreateSolidBrush)(RGB(1, 2, 3)), U(CreatePen)(PS_SOLID, 1, 0), U(CreateBitmap)(2, 2, 1, 1, NULL) };
    for (int i = 0; i < 3; i++) {
        BOOL r = U(DeleteDC)((HDC)o[i]);
        DWORD t = U(GetObjectType)(o[i]);
        t_note("DeleteDC(object %d) -> %d; object type afterwards %lu", i, r, t);
        U(DeleteObject)(o[i]);
    }
}

TEST(dcl_delete_with_bitmap_selected) {
    REQUIRE(U(DeleteDC));
    HDC dc = dcl_mem();
    HBITMAP bm = U(CreateBitmap)(16, 16, 1, 1, NULL);
    HGDIOBJ old = U(SelectObject)(dc, bm);
    CHECK(old != NULL);
    CHECK(U(DeleteDC)(dc));
    /* bitmap survives and is selectable into another DC */
    EQ(U(GetObjectType)(bm), OBJ_BITMAP);
    HDC dc2 = dcl_mem();
    HGDIOBJ old2 = U(SelectObject)(dc2, bm);
    CHECK(old2 != NULL);
    U(SelectObject)(dc2, old2);
    CHECK(U(DeleteDC)(dc2));
    CHECK(U(DeleteObject)(bm));
}

TEST(dcl_delete_with_dib_selected) {
    REQUIRE(U(DeleteDC));
    HDC dc = dcl_mem();
    void *bits;
    HBITMAP bm = dcl_dib(dc, 8, 8, 32, &bits);
    U(SelectObject)(dc, bm);
    CHECK(U(DeleteDC)(dc));
    EQ(U(GetObjectType)(bm), OBJ_BITMAP);
    BITMAP b; CHECK(U(GetObjectW)(bm, sizeof b, &b) == sizeof b);
    CHECK(b.bmBits == bits);
    ((DWORD *)bits)[0] = 0x11223344; /* memory must still be mapped */
    CHECK(U(DeleteObject)(bm));
}

TEST(dcl_delete_with_pen_brush_font_selected) {
    REQUIRE(U(DeleteDC));
    HDC dc = dcl_mem();
    HPEN p = U(CreatePen)(PS_SOLID, 3, RGB(1, 2, 3));
    HBRUSH b = U(CreateSolidBrush)(RGB(4, 5, 6));
    HFONT f = U(CreateFontA)(16, 0, 0, 0, 400, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, "Arial");
    U(SelectObject)(dc, p); U(SelectObject)(dc, b); U(SelectObject)(dc, f);
    CHECK(U(DeleteDC)(dc));
    EQ(U(GetObjectType)(p), OBJ_PEN);
    EQ(U(GetObjectType)(b), OBJ_BRUSH);
    EQ(U(GetObjectType)(f), OBJ_FONT);
    CHECK(U(DeleteObject)(p)); CHECK(U(DeleteObject)(b)); CHECK(U(DeleteObject)(f));
}

TEST(dcl_delete_with_extpen_selected) {
    REQUIRE(U(DeleteDC)); REQUIRE(U(ExtCreatePen));
    HDC dc = dcl_mem();
    LOGBRUSH lb = { BS_SOLID, RGB(9, 9, 9), 0 };
    HPEN p = U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID, 5, &lb, 0, NULL);
    CHECK(p != NULL);
    U(SelectObject)(dc, p);
    EQ(U(GetCurrentObject)(dc, OBJ_EXTPEN), p);
    CHECK(U(DeleteDC)(dc));
    EQ(U(GetObjectType)(p), OBJ_EXTPEN);
    CHECK(U(DeleteObject)(p));
}

TEST(dcl_delete_object_while_selected) {
    REQUIRE(U(DeleteDC));
    HDC dc = dcl_mem();
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(1, 1, 1));
    HBRUSH b = U(CreateSolidBrush)(RGB(2, 2, 2));
    HBITMAP bm = U(CreateBitmap)(4, 4, 1, 1, NULL);
    HGDIOBJ op = U(SelectObject)(dc, p), ob = U(SelectObject)(dc, b), obm = U(SelectObject)(dc, bm);
    BOOL rp = U(DeleteObject)(p), rb = U(DeleteObject)(b), rbm = U(DeleteObject)(bm);
    t_note("DeleteObject while selected: pen=%d brush=%d bitmap=%d", rp, rb, rbm);
    t_note("types afterwards: pen=%lu brush=%lu bitmap=%lu", U(GetObjectType)(p), U(GetObjectType)(b), U(GetObjectType)(bm));
    U(SelectObject)(dc, op); U(SelectObject)(dc, ob); U(SelectObject)(dc, obm);
    U(DeleteObject)(p); U(DeleteObject)(b); U(DeleteObject)(bm);
    CHECK(U(DeleteDC)(dc));
}

TEST(dcl_delete_bitmap_selected_in_other_dc) {
    REQUIRE(U(DeleteDC));
    HDC a = dcl_mem(), b = dcl_mem();
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, NULL);
    HGDIOBJ oa = U(SelectObject)(a, bm);
    CHECK(oa != NULL);
    /* a bitmap can be selected into one DC only */
    HGDIOBJ ob = U(SelectObject)(b, bm);
    CHECK(ob == NULL);
    if (ob) U(SelectObject)(b, ob);
    U(SelectObject)(a, oa);
    ob = U(SelectObject)(b, bm);
    CHECK(ob != NULL);
    U(SelectObject)(b, ob);
    U(DeleteDC)(a); U(DeleteDC)(b);
    U(DeleteObject)(bm);
}

TEST(dcl_delete_window_dcs) {
    REQUIRE(U(DeleteDC)); REQUIRE(U(GetDC)); REQUIRE(U(GetWindowDC));
    HWND w = dcl_window(40, 40, 80, 60);
    if (!w) { t_skip("no window"); return; }
    HDC dc = U(GetDC)(w);
    EQ(U(GetObjectType)(dc), OBJ_DC);
    CHECK(U(DeleteDC)(dc));
    EQ(U(GetDeviceCaps)(dc, TECHNOLOGY), 0);
    dc = U(GetWindowDC)(w);
    EQ(U(GetObjectType)(dc), OBJ_DC);
    CHECK(U(DeleteDC)(dc));
    EQ(U(GetDeviceCaps)(dc, TECHNOLOGY), 0);
    U(DestroyWindow)(w);
}

TEST(dcl_delete_desktop_dcs) {
    REQUIRE(U(DeleteDC)); REQUIRE(U(GetDC)); REQUIRE(U(GetWindowDC)); REQUIRE(U(GetDesktopWindow));
    HWND d = U(GetDesktopWindow)();
    HDC dc = U(GetDC)(d);
    CHECK(dc != NULL);
    EQ(U(GetObjectType)(dc), OBJ_DC);
    CHECK(U(DeleteDC)(dc));
    EQ(U(GetDeviceCaps)(dc, TECHNOLOGY), 0);
    dc = U(GetDC)(NULL);
    CHECK(U(DeleteDC)(dc));
    dc = U(GetWindowDC)(d);
    CHECK(U(DeleteDC)(dc));
}

TEST(dcl_delete_then_release_window_dc) {
    REQUIRE(U(DeleteDC)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HWND w = dcl_window(40, 40, 80, 60);
    if (!w) { t_skip("no window"); return; }
    HDC dc = U(GetDC)(w);
    CHECK(U(DeleteDC)(dc));
    int r = U(ReleaseDC)(w, dc);
    t_note("ReleaseDC after DeleteDC -> %d", r);
    /* the window must still produce a fresh working DC */
    HDC dc2 = U(GetDC)(w);
    CHECK(dc2 != NULL);
    if (dc2) { EQ(U(GetObjectType)(dc2), OBJ_DC); U(ReleaseDC)(w, dc2); }
    U(DestroyWindow)(w);
}

TEST(dcl_delete_gdi_count_balanced) {
    REQUIRE(U(DeleteDC)); REQUIRE(U(GetGuiResources));
    DWORD before = dcl_gdicount();
    HDC d1 = dcl_mem(), d2 = U(CreateDCA)("DISPLAY", NULL, NULL, NULL), d3 = U(CreateICA)("DISPLAY", NULL, NULL, NULL);
    HBITMAP bm = U(CreateBitmap)(10, 10, 1, 1, NULL);
    HGDIOBJ old = U(SelectObject)(d1, bm);
    U(SaveDC)(d1); U(SaveDC)(d2);
    U(SelectObject)(d1, old);
    CHECK(U(DeleteDC)(d1)); CHECK(U(DeleteDC)(d2)); CHECK(U(DeleteDC)(d3));
    U(DeleteObject)(bm);
    EQ(dcl_gdicount(), before);
}

TEST(dcl_delete_dc_brush_pen_no_leak) {
    REQUIRE(U(DeleteDC)); REQUIRE(U(GetGuiResources));
    DWORD before = dcl_gdicount();
    for (int i = 0; i < 100; i++) {
        HDC dc = dcl_mem();
        U(SelectObject)(dc, U(GetStockObject)(DC_BRUSH));
        U(SelectObject)(dc, U(GetStockObject)(DC_PEN));
        U(SetDCBrushColor)(dc, RGB(i, i, i));
        U(SetDCPenColor)(dc, RGB(i, 0, i));
        U(DeleteDC)(dc);
    }
    EQ(dcl_gdicount(), before);
}

/* ---------------------------------------------------------------- ResetDC */

TEST(dcl_resetdc_null_handle) {
    REQUIRE(U(ResetDCA)); REQUIRE(U(ResetDCW));
    SET_ERR(0xdeadbeef);
    CHECK(U(ResetDCW)(NULL, NULL) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SET_ERR(0xdeadbeef);
    CHECK(U(ResetDCA)(NULL, NULL) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}

TEST(dcl_resetdc_bogus_handle) {
    REQUIRE(U(ResetDCA)); REQUIRE(U(ResetDCW));
    SET_ERR(0xdeadbeef);
    CHECK(U(ResetDCW)((HDC)(ULONG_PTR)0xdeadbeef, NULL) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SET_ERR(0xdeadbeef);
    CHECK(U(ResetDCA)((HDC)(ULONG_PTR)0xdeadbeef, NULL) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}

TEST(dcl_resetdc_deleted_dc) {
    REQUIRE(U(ResetDCA)); REQUIRE(U(ResetDCW));
    HDC dc = dcl_mem();
    U(DeleteDC)(dc);
    CHECK(U(ResetDCW)(dc, NULL) == NULL);
    CHECK(U(ResetDCA)(dc, NULL) == NULL);
}

TEST(dcl_resetdc_non_dc_objects) {
    REQUIRE(U(ResetDCA)); REQUIRE(U(ResetDCW));
    HGDIOBJ o[2] = { U(CreateSolidBrush)(0), U(CreateBitmap)(2, 2, 1, 1, NULL) };
    for (int i = 0; i < 2; i++) {
        CHECK(U(ResetDCW)((HDC)o[i], NULL) == NULL);
        CHECK(U(ResetDCA)((HDC)o[i], NULL) == NULL);
        U(DeleteObject)(o[i]);
    }
}

TEST(dcl_resetdc_memory_dc) {
    REQUIRE(U(ResetDCA)); REQUIRE(U(ResetDCW));
    HDC dc = dcl_mem();
    HDC r = U(ResetDCW)(dc, NULL);
    DCL_WCHECK(r == NULL);
    r = U(ResetDCA)(dc, NULL);
    DCL_WCHECK(r == NULL);
    /* the DC must be unharmed either way */
    EQ(U(GetObjectType)(dc), OBJ_MEMDC);
    U(DeleteDC)(dc);
}

TEST(dcl_resetdc_display_dc) {
    REQUIRE(U(ResetDCA)); REQUIRE(U(ResetDCW));
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    U(SetBkColor)(dc, RGB(5, 6, 7));
    HDC r = U(ResetDCW)(dc, NULL);
    DCL_WCHECK(r == NULL);
    t_note("ResetDCW(display DC, NULL) -> %p (dc %p)", (void *)r, (void *)dc);
    EQ(U(GetObjectType)(dc), OBJ_DC);
    U(DeleteDC)(dc);
}

TEST(dcl_resetdc_display_dc_with_devmode) {
    REQUIRE(U(ResetDCA)); REQUIRE(U(EnumDisplaySettingsA));
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    DEVMODEA dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    if (U(EnumDisplaySettingsA)(NULL, ENUM_CURRENT_SETTINGS, &dm)) {
        HDC r = U(ResetDCA)(dc, &dm);
        t_note("ResetDCA(display DC, current devmode) -> %p (dc %p)", (void *)r, (void *)dc);
        EQ(U(GetObjectType)(dc), OBJ_DC);
        EQ(U(GetDeviceCaps)(dc, TECHNOLOGY), DT_RASDISPLAY);
    }
    U(DeleteDC)(dc);
}

TEST(dcl_resetdc_metafile_dc) {
    REQUIRE(U(ResetDCA)); REQUIRE(U(CreateMetaFileA)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    HDC mf = U(CreateMetaFileA)(NULL);
    CHECK(mf != NULL);
    if (!mf) return;
    CHECK(U(ResetDCA)(mf, NULL) == NULL);
    CHECK(U(ResetDCW)(mf, NULL) == NULL);
    U(DeleteMetaFile)(U(CloseMetaFile)(mf));
}

/* ------------------------------------------------------------ GetObjectType */

TEST(dcl_objtype_dc_kinds) {
    REQUIRE(U(GetObjectType));
    HDC mem = dcl_mem(), dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL), ic = U(CreateICA)("DISPLAY", NULL, NULL, NULL), scr = dcl_screen();
    EQ(U(GetObjectType)(mem), OBJ_MEMDC);
    EQ(U(GetObjectType)(dc), OBJ_DC);
    EQ(U(GetObjectType)(ic), OBJ_DC);
    EQ(U(GetObjectType)(scr), OBJ_DC);
    U(DeleteDC)(mem); U(DeleteDC)(dc); U(DeleteDC)(ic); dcl_release_screen(scr);
}

TEST(dcl_objtype_memdc_with_bitmap) {
    REQUIRE(U(GetObjectType));
    HBITMAP bmp, old; DWORD *bits;
    HDC dc = dcl_mem32(4, 4, &bmp, &old, &bits);
    EQ(U(GetObjectType)(dc), OBJ_MEMDC);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_objtype_invalid) {
    REQUIRE(U(GetObjectType));
    EQ(U(GetObjectType)(NULL), 0);
    EQ(U(GetObjectType)((HGDIOBJ)(ULONG_PTR)0xdeadbeef), 0);
    EQ(U(GetObjectType)((HGDIOBJ)(ULONG_PTR)0xffffffff), 0);
    EQ(U(GetObjectType)((HGDIOBJ)(ULONG_PTR)1), 0);
    EQ(U(GetObjectType)((HGDIOBJ)(ULONG_PTR)0x10001), 0);
}

TEST(dcl_objtype_deleted_dc) {
    REQUIRE(U(GetObjectType));
    HDC mem = dcl_mem(), dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    U(DeleteDC)(mem); U(DeleteDC)(dc);
    EQ(U(GetObjectType)(mem), 0);
    EQ(U(GetObjectType)(dc), 0);
}

TEST(dcl_objtype_handle_reuse_not_confused) {
    REQUIRE(U(GetObjectType));
    for (int i = 0; i < 50; i++) {
        HDC a = dcl_mem();
        U(DeleteDC)(a);
        HDC b = dcl_mem();
        if (b == a) t_note("handle reused verbatim");
        EQ(U(GetObjectType)(b), OBJ_MEMDC);
        U(DeleteDC)(b);
    }
}

TEST(dcl_objtype_gdi_objects) {
    REQUIRE(U(GetObjectType));
    LOGPALETTE lp; memset(&lp, 0, sizeof lp); lp.palVersion = 0x300; lp.palNumEntries = 1;
    HGDIOBJ pen = U(CreatePen)(PS_SOLID, 1, 0), br = U(CreateSolidBrush)(0), bm = U(CreateBitmap)(1, 1, 1, 1, NULL), rg = U(CreateRectRgn)(0, 0, 1, 1);
    HGDIOBJ pal = U(CreatePalette)(&lp), fnt = U(CreateFontA)(12, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, "Arial");
    LOGBRUSH lb = { BS_SOLID, 0, 0 };
    HGDIOBJ ext = U(ExtCreatePen)(PS_GEOMETRIC, 2, &lb, 0, NULL);
    EQ(U(GetObjectType)(pen), OBJ_PEN);
    EQ(U(GetObjectType)(br), OBJ_BRUSH);
    EQ(U(GetObjectType)(bm), OBJ_BITMAP);
    EQ(U(GetObjectType)(rg), OBJ_REGION);
    EQ(U(GetObjectType)(pal), OBJ_PAL);
    EQ(U(GetObjectType)(fnt), OBJ_FONT);
    EQ(U(GetObjectType)(ext), OBJ_EXTPEN);
    U(DeleteObject)(pen); U(DeleteObject)(br); U(DeleteObject)(bm); U(DeleteObject)(rg); U(DeleteObject)(pal); U(DeleteObject)(fnt); U(DeleteObject)(ext);
}

TEST(dcl_objtype_stock_objects) {
    REQUIRE(U(GetObjectType));
    EQ(U(GetObjectType)(U(GetStockObject)(WHITE_BRUSH)), OBJ_BRUSH);
    EQ(U(GetObjectType)(U(GetStockObject)(NULL_BRUSH)), OBJ_BRUSH);
    EQ(U(GetObjectType)(U(GetStockObject)(DC_BRUSH)), OBJ_BRUSH);
    EQ(U(GetObjectType)(U(GetStockObject)(BLACK_PEN)), OBJ_PEN);
    EQ(U(GetObjectType)(U(GetStockObject)(NULL_PEN)), OBJ_PEN);
    EQ(U(GetObjectType)(U(GetStockObject)(DC_PEN)), OBJ_PEN);
    EQ(U(GetObjectType)(U(GetStockObject)(SYSTEM_FONT)), OBJ_FONT);
    EQ(U(GetObjectType)(U(GetStockObject)(DEFAULT_GUI_FONT)), OBJ_FONT);
    EQ(U(GetObjectType)(U(GetStockObject)(DEFAULT_PALETTE)), OBJ_PAL);
}

TEST(dcl_objtype_metafile_dcs) {
    REQUIRE(U(GetObjectType)); REQUIRE(U(CreateMetaFileA)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    REQUIRE(U(CreateEnhMetaFileA)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC mf = U(CreateMetaFileA)(NULL);
    CHECK(mf != NULL);
    if (mf) {
        EQ(U(GetObjectType)(mf), OBJ_METADC);
        HMETAFILE h = U(CloseMetaFile)(mf);
        CHECK(h != NULL);
        EQ(U(GetObjectType)(h), OBJ_METAFILE);
        EQ(U(GetObjectType)(mf), 0);
        U(DeleteMetaFile)(h);
    }
    HDC emf = U(CreateEnhMetaFileA)(NULL, NULL, NULL, NULL);
    CHECK(emf != NULL);
    if (emf) {
        EQ(U(GetObjectType)(emf), OBJ_ENHMETADC);
        HENHMETAFILE h = U(CloseEnhMetaFile)(emf);
        CHECK(h != NULL);
        EQ(U(GetObjectType)(h), OBJ_ENHMETAFILE);
        EQ(U(GetObjectType)(emf), 0);
        U(DeleteEnhMetaFile)(h);
    }
}

TEST(dcl_metafile_dc_caps_technology) {
    REQUIRE(U(CreateMetaFileA)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    HDC mf = U(CreateMetaFileA)(NULL);
    if (!mf) { CHECK(mf != NULL); return; }
    EQ(U(GetDeviceCaps)(mf, TECHNOLOGY), DT_METAFILE);
    for (int i = 0; i <= 121; i++) if (i != TECHNOLOGY) CHECKF(U(GetDeviceCaps)(mf, i) == 0, "metafile DC index %d = %d", i, U(GetDeviceCaps)(mf, i));
    U(DeleteMetaFile)(U(CloseMetaFile)(mf));
}

TEST(dcl_enhmetafile_dc_caps) {
    REQUIRE(U(CreateEnhMetaFileA)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC ref = dcl_screen();
    HDC emf = U(CreateEnhMetaFileA)(ref, NULL, NULL, NULL);
    if (!emf) { CHECK(emf != NULL); dcl_release_screen(ref); return; }
    EQ(U(GetDeviceCaps)(emf, TECHNOLOGY), DT_RASDISPLAY);
    for (int i = 0; i <= 121; i++) {
        if (i == VREFRESH) continue;
        CHECKF(U(GetDeviceCaps)(emf, i) == U(GetDeviceCaps)(ref, i), "emf DC index %d", i);
    }
    U(DeleteEnhMetaFile)(U(CloseEnhMetaFile)(emf));
    dcl_release_screen(ref);
}

TEST(dcl_compat_dc_from_enhmetafile_dc) {
    REQUIRE(U(CreateEnhMetaFileA)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC emf = U(CreateEnhMetaFileA)(NULL, NULL, NULL, NULL);
    HDC c = U(CreateCompatibleDC)(emf);
    CHECK(c != NULL);
    if (c) EQ(U(GetObjectType)(c), OBJ_MEMDC);
    if (c) U(DeleteDC)(c);
    U(DeleteEnhMetaFile)(U(CloseEnhMetaFile)(emf));
}

TEST(dcl_compat_dc_from_metafile_dc) {
    REQUIRE(U(CreateMetaFileA)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    HDC mf = U(CreateMetaFileA)(NULL);
    HDC c = U(CreateCompatibleDC)(mf);
    CHECK(c == NULL);
    if (c) U(DeleteDC)(c);
    U(DeleteMetaFile)(U(CloseMetaFile)(mf));
}

TEST(dcl_metafile_dc_savedc) {
    REQUIRE(U(CreateMetaFileA)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    HDC mf = U(CreateMetaFileA)(NULL);
    int a = U(SaveDC)(mf);
    BOOL r = U(RestoreDC)(mf, -1);
    t_note("metafile DC SaveDC=%d RestoreDC=%d", a, r);
    U(DeleteMetaFile)(U(CloseMetaFile)(mf));
}

TEST(dcl_enhmetafile_dc_savedc) {
    REQUIRE(U(CreateEnhMetaFileA)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC emf = U(CreateEnhMetaFileA)(NULL, NULL, NULL, NULL);
    int a = U(SaveDC)(emf);
    int b = U(SaveDC)(emf);
    BOOL r = U(RestoreDC)(emf, -1);
    t_note("enh metafile DC SaveDC=%d,%d RestoreDC=%d", a, b, r);
    CHECK(a == 1 && b == 2);
    U(DeleteEnhMetaFile)(U(CloseEnhMetaFile)(emf));
}

TEST(dcl_metafile_dc_deletedc_closes) {
    REQUIRE(U(CreateEnhMetaFileA)); REQUIRE(U(DeleteDC));
    DWORD before = dcl_gdicount();
    HDC emf = U(CreateEnhMetaFileA)(NULL, NULL, NULL, NULL);
    BOOL r = U(DeleteDC)(emf);
    t_note("DeleteDC on open enhanced metafile DC -> %d", r);
    if (!r) U(DeleteEnhMetaFile)(U(CloseEnhMetaFile)(emf));
    if (dcl_gdicount() != before) t_note("GDI count %lu -> %lu", before, dcl_gdicount());
}

/* -------------------------------------------------------- threads / lifetime */

static HDC g_th_dc;
static DWORD WINAPI th_create(LPVOID p) {
    g_th_dc = U(CreateCompatibleDC)(NULL);
    return 0;
}
TEST(dcl_thread_dc_outlives_thread) {
    REQUIRE(U(CreateCompatibleDC));
    g_th_dc = NULL;
    HANDLE h = CreateThread(NULL, 0, th_create, NULL, 0, NULL);
    WaitForSingleObject(h, 10000); CloseHandle(h);
    CHECK(g_th_dc != NULL);
    if (g_th_dc) {
        EQ(U(GetObjectType)(g_th_dc), OBJ_MEMDC);
        EQ(U(GetDeviceCaps)(g_th_dc, TECHNOLOGY), DT_RASDISPLAY);
        CHECK(U(DeleteDC)(g_th_dc));
    }
}

static DWORD WINAPI th_use(LPVOID p) {
    HDC dc = (HDC)p;
    int ok = 0;
    if (U(GetObjectType)(dc) == OBJ_MEMDC) ok++;
    if (U(SaveDC)(dc) == 1) ok++;
    U(SetBkColor)(dc, RGB(11, 22, 33));
    if (U(RestoreDC)(dc, -1)) ok++;
    if (U(GetBkColor)(dc) == RGB(255, 255, 255)) ok++;
    return ok;
}
TEST(dcl_thread_use_dc_from_other_thread) {
    REQUIRE(U(SaveDC));
    HDC dc = dcl_mem();
    HANDLE h = CreateThread(NULL, 0, th_use, dc, 0, NULL);
    WaitForSingleObject(h, 10000);
    DWORD code = 0; GetExitCodeThread(h, &code); CloseHandle(h);
    EQ(code, 4);
    CHECK(U(DeleteDC)(dc));
}

static DWORD WINAPI th_delete(LPVOID p) { return U(DeleteDC)((HDC)p); }
TEST(dcl_thread_delete_dc_from_other_thread) {
    REQUIRE(U(DeleteDC));
    HDC dc = dcl_mem();
    HANDLE h = CreateThread(NULL, 0, th_delete, dc, 0, NULL);
    WaitForSingleObject(h, 10000);
    DWORD code = 0; GetExitCodeThread(h, &code); CloseHandle(h);
    EQ(code, 1);
    EQ(U(GetObjectType)(dc), 0);
    CHECK(!U(DeleteDC)(dc));
}

static DWORD WINAPI th_probe_deleted(LPVOID p) {
    HDC dc = (HDC)p;
    return (U(GetObjectType)(dc) == 0 && U(SaveDC)(dc) == 0 && U(GetDeviceCaps)(dc, TECHNOLOGY) == 0);
}
TEST(dcl_thread_deleted_dc_invalid_everywhere) {
    REQUIRE(U(DeleteDC));
    HDC dc = dcl_mem();
    U(DeleteDC)(dc);
    HANDLE h = CreateThread(NULL, 0, th_probe_deleted, dc, 0, NULL);
    WaitForSingleObject(h, 10000);
    DWORD code = 0; GetExitCodeThread(h, &code); CloseHandle(h);
    EQ(code, 1);
}

static DWORD WINAPI th_gdi_roundtrip(LPVOID p) {
    int ok = 0;
    for (int i = 0; i < 200; i++) {
        HDC dc = U(CreateCompatibleDC)(NULL);
        if (dc && U(DeleteDC)(dc)) ok++;
    }
    return ok;
}
TEST(dcl_thread_parallel_create_delete) {
    REQUIRE(U(CreateCompatibleDC));
    DWORD before = dcl_gdicount();
    HANDLE h[4];
    for (int i = 0; i < 4; i++) h[i] = CreateThread(NULL, 0, th_gdi_roundtrip, NULL, 0, NULL);
    WaitForMultipleObjects(4, h, TRUE, 20000);
    for (int i = 0; i < 4; i++) { DWORD c = 0; GetExitCodeThread(h[i], &c); EQ(c, 200); CloseHandle(h[i]); }
    EQ(dcl_gdicount(), before);
}

TEST(dcl_gdi_handle_table_cycle) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(GetGuiResources));
    DWORD before = dcl_gdicount();
    HDC dcs[300]; int n = 0;
    for (int i = 0; i < 300; i++) { dcs[i] = U(CreateCompatibleDC)(NULL); if (dcs[i]) n++; }
    CHECK(n == 300);
    for (int i = 0; i < 300; i++) if (dcs[i]) U(DeleteDC)(dcs[i]);
    EQ(dcl_gdicount(), before);
}

TEST(dcl_gdi_count_peak_not_below_current) {
    REQUIRE(U(GetGuiResources));
    DWORD cur = dcl_gdicount();
    DWORD peak = U(GetGuiResources)(GetCurrentProcess(), 2 /* GR_GDIOBJECTS_PEAK */);
    if (!cur && dcl_wine()) { t_skip("GetGuiResources unsupported"); return; }
    CHECKF(peak >= cur, "peak %lu current %lu", peak, cur);
}

TEST(dcl_gdi_count_invalid_flag) {
    REQUIRE(U(GetGuiResources));
    SET_ERR(0xdeadbeef);
    DWORD r = U(GetGuiResources)(GetCurrentProcess(), 0xff);
    EQ(r, 0);
}
