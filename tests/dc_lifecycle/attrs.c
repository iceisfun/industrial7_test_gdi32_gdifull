/* GetCurrentObject, DC brush/pen colour, GetDCOrgEx, batching, bounds rect, gamma ramp */
#include "dcl.h"

/* ---------------------------------------------------------- GetCurrentObject */

TEST(dcl_curobj_valid_types_memdc) {
    REQUIRE(U(GetCurrentObject));
    HDC dc = dcl_mem();
    static const struct { UINT t; const char *n; } t[] = {
        { OBJ_PEN, "OBJ_PEN" }, { OBJ_BRUSH, "OBJ_BRUSH" }, { OBJ_PAL, "OBJ_PAL" }, { OBJ_FONT, "OBJ_FONT" },
        { OBJ_BITMAP, "OBJ_BITMAP" }, { OBJ_EXTPEN, "OBJ_EXTPEN" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        HGDIOBJ o = U(GetCurrentObject)(dc, t[i].t);
        CHECKF(o != NULL, "%s", t[i].n);
    }
    HGDIOBJ cs = U(GetCurrentObject)(dc, OBJ_COLORSPACE);
    t_note("OBJ_COLORSPACE on memdc -> %p", cs);
    U(DeleteDC)(dc);
}

TEST(dcl_curobj_valid_types_screen) {
    REQUIRE(U(GetCurrentObject));
    HDC dc = dcl_screen();
    static const UINT t[] = { OBJ_PEN, OBJ_BRUSH, OBJ_PAL, OBJ_FONT, OBJ_EXTPEN };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) CHECKF(U(GetCurrentObject)(dc, t[i]) != NULL, "type %u", t[i]);
    HGDIOBJ bm = U(GetCurrentObject)(dc, OBJ_BITMAP);
    t_note("OBJ_BITMAP on screen DC -> %p", bm);
    dcl_release_screen(dc);
}

TEST(dcl_curobj_invalid_types) {
    REQUIRE(U(GetCurrentObject));
    HDC dc = dcl_mem();
    static const UINT bad[] = { 0, OBJ_DC, OBJ_METADC, OBJ_REGION, OBJ_METAFILE, OBJ_MEMDC, OBJ_ENHMETADC, OBJ_ENHMETAFILE, 15, 16, 17, 100, 255, 256, 0x10001, 0x7fffffff, 0x80000000, 0xffffffff };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0xdeadbeef);
        HGDIOBJ o = U(GetCurrentObject)(dc, bad[i]);
        CHECKF(o == NULL, "type %u returned %p", bad[i], o);
        DWORD e = GetLastError();
        if (!dcl_wine()) CHECKF(e == ERROR_INVALID_PARAMETER, "type %u error %lu", bad[i], e);
    }
    U(DeleteDC)(dc);
}

TEST(dcl_curobj_invalid_handle) {
    REQUIRE(U(GetCurrentObject));
    static const UINT t[] = { OBJ_PEN, OBJ_BRUSH, OBJ_PAL, OBJ_FONT, OBJ_BITMAP, OBJ_EXTPEN, OBJ_COLORSPACE };
    static const ULONG_PTR h[] = { 0, 0xdeadbeef, 0xffffffff, 1 };
    for (unsigned j = 0; j < sizeof h / sizeof *h; j++)
        for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
            HGDIOBJ o = U(GetCurrentObject)((HDC)h[j], t[i]);
            CHECKF(o == NULL, "handle 0x%llx type %u returned %p", (unsigned long long)h[j], t[i], o);
        }
}

TEST(dcl_curobj_deleted_dc) {
    REQUIRE(U(GetCurrentObject));
    HDC dc = dcl_mem();
    U(DeleteDC)(dc);
    static const UINT t[] = { OBJ_PEN, OBJ_BRUSH, OBJ_PAL, OBJ_FONT, OBJ_BITMAP, OBJ_EXTPEN };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) CHECKF(U(GetCurrentObject)(dc, t[i]) == NULL, "type %u", t[i]);
}

TEST(dcl_curobj_non_dc_handle) {
    REQUIRE(U(GetCurrentObject));
    HGDIOBJ b = U(CreateSolidBrush)(0);
    CHECK(U(GetCurrentObject)((HDC)b, OBJ_PEN) == NULL);
    CHECK(U(GetCurrentObject)((HDC)b, OBJ_BRUSH) == NULL);
    CHECK(U(GetCurrentObject)((HDC)b, OBJ_FONT) == NULL);
    U(DeleteObject)(b);
}

TEST(dcl_curobj_pen_roundtrip) {
    REQUIRE(U(GetCurrentObject));
    HDC dc = dcl_mem();
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(1, 2, 3));
    HGDIOBJ old = U(SelectObject)(dc, p);
    EQ(U(GetCurrentObject)(dc, OBJ_PEN), p);
    EQ(U(GetCurrentObject)(dc, OBJ_EXTPEN), p);
    EQ(old, U(GetStockObject)(BLACK_PEN));
    EQ(U(SelectObject)(dc, old), p);
    EQ(U(GetCurrentObject)(dc, OBJ_PEN), old);
    U(DeleteDC)(dc); U(DeleteObject)(p);
}

TEST(dcl_curobj_extpen_roundtrip) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(U(ExtCreatePen));
    HDC dc = dcl_mem();
    LOGBRUSH lb = { BS_SOLID, RGB(3, 2, 1), 0 };
    HPEN p = U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID, 4, &lb, 0, NULL);
    HGDIOBJ old = U(SelectObject)(dc, p);
    EQ(U(GetCurrentObject)(dc, OBJ_EXTPEN), p);
    EQ(U(GetCurrentObject)(dc, OBJ_PEN), p);
    U(SelectObject)(dc, old);
    U(DeleteDC)(dc); U(DeleteObject)(p);
}

TEST(dcl_curobj_brush_kinds) {
    REQUIRE(U(GetCurrentObject));
    HDC dc = dcl_mem();
    HBITMAP pat = U(CreateBitmap)(8, 8, 1, 1, NULL);
    HBRUSH br[4] = { U(CreateSolidBrush)(RGB(1, 2, 3)), U(CreateHatchBrush)(HS_CROSS, RGB(4, 5, 6)), U(CreatePatternBrush)(pat), (HBRUSH)U(GetStockObject)(NULL_BRUSH) };
    HGDIOBJ old = U(SelectObject)(dc, br[0]);
    for (int i = 0; i < 4; i++) {
        CHECKF(br[i] != NULL, "brush %d", i);
        U(SelectObject)(dc, br[i]);
        EQ(U(GetCurrentObject)(dc, OBJ_BRUSH), br[i]);
    }
    U(SelectObject)(dc, old);
    U(DeleteDC)(dc);
    U(DeleteObject)(br[0]); U(DeleteObject)(br[1]); U(DeleteObject)(br[2]); U(DeleteObject)(pat);
}

TEST(dcl_curobj_font_roundtrip) {
    REQUIRE(U(GetCurrentObject));
    HDC dc = dcl_mem();
    HFONT f = U(CreateFontA)(18, 0, 0, 0, 400, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, "Tahoma");
    HGDIOBJ old = U(SelectObject)(dc, f);
    EQ(U(GetCurrentObject)(dc, OBJ_FONT), f);
    EQ(U(SelectObject)(dc, old), f);
    EQ(U(GetCurrentObject)(dc, OBJ_FONT), old);
    U(DeleteDC)(dc); U(DeleteObject)(f);
}

TEST(dcl_curobj_stock_fonts_selectable) {
    REQUIRE(U(GetCurrentObject));
    HDC dc = dcl_mem();
    static const int st[] = { OEM_FIXED_FONT, ANSI_FIXED_FONT, ANSI_VAR_FONT, SYSTEM_FONT, DEVICE_DEFAULT_FONT, SYSTEM_FIXED_FONT, DEFAULT_GUI_FONT };
    HGDIOBJ old = U(GetCurrentObject)(dc, OBJ_FONT);
    for (unsigned i = 0; i < sizeof st / sizeof *st; i++) {
        HGDIOBJ s = U(GetStockObject)(st[i]);
        CHECKF(s != NULL, "stock %d", st[i]);
        U(SelectObject)(dc, s);
        EQ(U(GetCurrentObject)(dc, OBJ_FONT), s);
    }
    U(SelectObject)(dc, old);
    U(DeleteDC)(dc);
}

TEST(dcl_curobj_bitmap_roundtrip) {
    REQUIRE(U(GetCurrentObject));
    HDC dc = dcl_mem();
    HBITMAP bm = U(CreateBitmap)(5, 5, 1, 1, NULL);
    HGDIOBJ def = U(GetCurrentObject)(dc, OBJ_BITMAP);
    HGDIOBJ old = U(SelectObject)(dc, bm);
    EQ(old, def);
    EQ(U(GetCurrentObject)(dc, OBJ_BITMAP), bm);
    EQ(U(SelectObject)(dc, old), bm);
    EQ(U(GetCurrentObject)(dc, OBJ_BITMAP), def);
    U(DeleteDC)(dc); U(DeleteObject)(bm);
}

TEST(dcl_curobj_palette_roundtrip) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(U(CreatePalette)); REQUIRE(U(SelectPalette));
    HDC dc = dcl_mem();
    struct { WORD v, n; PALETTEENTRY e[4]; } lp;
    memset(&lp, 0, sizeof lp); lp.v = 0x300; lp.n = 4;
    lp.e[1].peRed = 255; lp.e[2].peGreen = 255; lp.e[3].peBlue = 255;
    HPALETTE pal = U(CreatePalette)((LOGPALETTE *)&lp);
    CHECK(pal != NULL);
    HPALETTE old = U(SelectPalette)(dc, pal, TRUE);
    CHECK(old != NULL);
    EQ(U(GetCurrentObject)(dc, OBJ_PAL), pal);
    EQ(U(SelectPalette)(dc, old, TRUE), pal);
    EQ(U(GetCurrentObject)(dc, OBJ_PAL), old);
    U(DeleteDC)(dc); U(DeleteObject)(pal);
}

TEST(dcl_curobj_default_palette_is_stock) {
    REQUIRE(U(GetCurrentObject));
    HDC dc = dcl_screen();
    HGDIOBJ p = U(GetCurrentObject)(dc, OBJ_PAL);
    if (p != U(GetStockObject)(DEFAULT_PALETTE)) t_note("screen DC default palette %p != stock %p", p, U(GetStockObject)(DEFAULT_PALETTE));
    CHECK(p != NULL);
    dcl_release_screen(dc);
}

TEST(dcl_curobj_dc_brush_pen_stock) {
    REQUIRE(U(GetCurrentObject));
    HDC dc = dcl_mem();
    HGDIOBJ ob = U(SelectObject)(dc, U(GetStockObject)(DC_BRUSH));
    HGDIOBJ op = U(SelectObject)(dc, U(GetStockObject)(DC_PEN));
    EQ(U(GetCurrentObject)(dc, OBJ_BRUSH), U(GetStockObject)(DC_BRUSH));
    EQ(U(GetCurrentObject)(dc, OBJ_PEN), U(GetStockObject)(DC_PEN));
    U(SelectObject)(dc, ob); U(SelectObject)(dc, op);
    U(DeleteDC)(dc);
}

TEST(dcl_curobj_per_dc) {
    REQUIRE(U(GetCurrentObject));
    HDC a = dcl_mem(), b = dcl_mem();
    HPEN p = U(CreatePen)(PS_DOT, 1, 0);
    HGDIOBJ oa = U(SelectObject)(a, p);
    EQ(U(GetCurrentObject)(a, OBJ_PEN), p);
    EQ(U(GetCurrentObject)(b, OBJ_PEN), U(GetStockObject)(BLACK_PEN));
    /* pens may be selected into several DCs at once */
    HGDIOBJ ob = U(SelectObject)(b, p);
    CHECK(ob != NULL);
    EQ(U(GetCurrentObject)(b, OBJ_PEN), p);
    U(SelectObject)(a, oa); U(SelectObject)(b, ob);
    U(DeleteDC)(a); U(DeleteDC)(b); U(DeleteObject)(p);
}

TEST(dcl_curobj_screen_dc_select_pen) {
    REQUIRE(U(GetCurrentObject));
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    HPEN p = U(CreatePen)(PS_SOLID, 2, RGB(9, 9, 9));
    HGDIOBJ old = U(SelectObject)(dc, p);
    EQ(U(GetCurrentObject)(dc, OBJ_PEN), p);
    U(SelectObject)(dc, old);
    U(DeleteDC)(dc); U(DeleteObject)(p);
}

/* ------------------------------------------------------ DC brush / pen colour */

TEST(dcl_dcbrush_default_white) {
    REQUIRE(U(GetDCBrushColor));
    HDC dc = dcl_mem();
    EQ(U(GetDCBrushColor)(dc), RGB(255, 255, 255));
    U(DeleteDC)(dc);
    dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    EQ(U(GetDCBrushColor)(dc), RGB(255, 255, 255));
    U(DeleteDC)(dc);
}

TEST(dcl_dcpen_default_black) {
    REQUIRE(U(GetDCPenColor));
    HDC dc = dcl_mem();
    EQ(U(GetDCPenColor)(dc), RGB(0, 0, 0));
    U(DeleteDC)(dc);
    dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    EQ(U(GetDCPenColor)(dc), RGB(0, 0, 0));
    U(DeleteDC)(dc);
}

TEST(dcl_dcbrush_set_returns_previous) {
    REQUIRE(U(SetDCBrushColor)); REQUIRE(U(GetDCBrushColor));
    HDC dc = dcl_mem();
    EQ(U(SetDCBrushColor)(dc, RGB(1, 2, 3)), RGB(255, 255, 255));
    EQ(U(GetDCBrushColor)(dc), RGB(1, 2, 3));
    EQ(U(SetDCBrushColor)(dc, RGB(4, 5, 6)), RGB(1, 2, 3));
    EQ(U(SetDCBrushColor)(dc, 0), RGB(4, 5, 6));
    EQ(U(GetDCBrushColor)(dc), 0);
    EQ(U(SetDCBrushColor)(dc, RGB(255, 255, 255)), 0);
    EQ(U(GetDCBrushColor)(dc), RGB(255, 255, 255));
    U(DeleteDC)(dc);
}

TEST(dcl_dcpen_set_returns_previous) {
    REQUIRE(U(SetDCPenColor)); REQUIRE(U(GetDCPenColor));
    HDC dc = dcl_mem();
    EQ(U(SetDCPenColor)(dc, RGB(10, 20, 30)), 0);
    EQ(U(GetDCPenColor)(dc), RGB(10, 20, 30));
    EQ(U(SetDCPenColor)(dc, RGB(40, 50, 60)), RGB(10, 20, 30));
    EQ(U(SetDCPenColor)(dc, RGB(255, 255, 255)), RGB(40, 50, 60));
    EQ(U(GetDCPenColor)(dc), RGB(255, 255, 255));
    U(DeleteDC)(dc);
}

TEST(dcl_dcbrush_pen_independent) {
    REQUIRE(U(SetDCBrushColor)); REQUIRE(U(SetDCPenColor));
    HDC dc = dcl_mem();
    U(SetDCBrushColor)(dc, RGB(1, 1, 1));
    EQ(U(GetDCPenColor)(dc), 0);
    U(SetDCPenColor)(dc, RGB(2, 2, 2));
    EQ(U(GetDCBrushColor)(dc), RGB(1, 1, 1));
    U(DeleteDC)(dc);
}

TEST(dcl_dcbrush_per_dc) {
    REQUIRE(U(SetDCBrushColor));
    HDC a = dcl_mem(), b = dcl_mem();
    U(SetDCBrushColor)(a, RGB(7, 7, 7));
    EQ(U(GetDCBrushColor)(b), RGB(255, 255, 255));
    U(SetDCBrushColor)(b, RGB(8, 8, 8));
    EQ(U(GetDCBrushColor)(a), RGB(7, 7, 7));
    U(DeleteDC)(a); U(DeleteDC)(b);
}

TEST(dcl_dcbrush_colour_table) {
    REQUIRE(U(SetDCBrushColor));
    static const COLORREF c[] = { 0, 0xffffff, 0x0000ff, 0x00ff00, 0xff0000, 0x010203, 0x7f7f7f, 0x808080, 0xfefefe, 0x000001, 0x010000 };
    HDC dc = dcl_mem();
    COLORREF prev = RGB(255, 255, 255);
    for (unsigned i = 0; i < sizeof c / sizeof *c; i++) {
        COLORREF r = U(SetDCBrushColor)(dc, c[i]);
        CHECKF(r == prev, "step %u: previous 0x%lx want 0x%lx", i, r, prev);
        CHECKF(U(GetDCBrushColor)(dc) == c[i], "step %u get", i);
        prev = c[i];
    }
    U(DeleteDC)(dc);
}

TEST(dcl_dcpen_colour_table) {
    REQUIRE(U(SetDCPenColor));
    static const COLORREF c[] = { 0xffffff, 0, 0x0000ff, 0x00ff00, 0xff0000, 0x010203, 0x7f7f7f, 0x808080 };
    HDC dc = dcl_mem();
    COLORREF prev = 0;
    for (unsigned i = 0; i < sizeof c / sizeof *c; i++) {
        COLORREF r = U(SetDCPenColor)(dc, c[i]);
        CHECKF(r == prev, "step %u: previous 0x%lx want 0x%lx", i, r, prev);
        CHECKF(U(GetDCPenColor)(dc) == c[i], "step %u get", i);
        prev = c[i];
    }
    U(DeleteDC)(dc);
}

TEST(dcl_dcbrush_high_byte_values) {
    REQUIRE(U(SetDCBrushColor));
    static const COLORREF c[] = { 0x01000005, 0x02123456, 0x10ffffff, 0x80000000, 0xffffffff };
    HDC dc = dcl_mem();
    for (unsigned i = 0; i < sizeof c / sizeof *c; i++) {
        U(SetDCBrushColor)(dc, c[i]);
        t_note("SetDCBrushColor(0x%08lx) -> Get 0x%08lx", c[i], U(GetDCBrushColor)(dc));
    }
    U(SetDCBrushColor)(dc, 0);
    U(DeleteDC)(dc);
}

TEST(dcl_dcbrush_invalid_handles) {
    REQUIRE(U(SetDCBrushColor)); REQUIRE(U(GetDCBrushColor));
    static const ULONG_PTR h[] = { 0, 0xdeadbeef, 0xffffffff, 1 };
    for (unsigned i = 0; i < sizeof h / sizeof *h; i++) {
        SET_ERR(0xdeadbeef);
        COLORREF r = U(GetDCBrushColor)((HDC)h[i]);
        CHECKF(r == CLR_INVALID, "get 0x%llx -> 0x%lx", (unsigned long long)h[i], r);
        DCL_WERR(ERROR_INVALID_PARAMETER);
        SET_ERR(0xdeadbeef);
        r = U(SetDCBrushColor)((HDC)h[i], RGB(1, 2, 3));
        CHECKF(r == CLR_INVALID, "set 0x%llx -> 0x%lx", (unsigned long long)h[i], r);
        DCL_WERR(ERROR_INVALID_PARAMETER);
    }
}

TEST(dcl_dcpen_invalid_handles) {
    REQUIRE(U(SetDCPenColor)); REQUIRE(U(GetDCPenColor));
    static const ULONG_PTR h[] = { 0, 0xdeadbeef, 0xffffffff, 1 };
    for (unsigned i = 0; i < sizeof h / sizeof *h; i++) {
        SET_ERR(0xdeadbeef);
        COLORREF r = U(GetDCPenColor)((HDC)h[i]);
        CHECKF(r == CLR_INVALID, "get 0x%llx -> 0x%lx", (unsigned long long)h[i], r);
        DCL_WERR(ERROR_INVALID_PARAMETER);
        SET_ERR(0xdeadbeef);
        r = U(SetDCPenColor)((HDC)h[i], RGB(1, 2, 3));
        CHECKF(r == CLR_INVALID, "set 0x%llx -> 0x%lx", (unsigned long long)h[i], r);
        DCL_WERR(ERROR_INVALID_PARAMETER);
    }
}

TEST(dcl_dccolors_deleted_dc) {
    REQUIRE(U(SetDCPenColor));
    HDC dc = dcl_mem();
    U(DeleteDC)(dc);
    EQ(U(GetDCBrushColor)(dc), CLR_INVALID);
    EQ(U(GetDCPenColor)(dc), CLR_INVALID);
    EQ(U(SetDCBrushColor)(dc, 1), CLR_INVALID);
    EQ(U(SetDCPenColor)(dc, 1), CLR_INVALID);
}

TEST(dcl_dccolors_non_dc_object) {
    REQUIRE(U(SetDCPenColor));
    HGDIOBJ b = U(CreateSolidBrush)(0), p = U(CreatePen)(PS_SOLID, 1, 0), bm = U(CreateBitmap)(1, 1, 1, 1, NULL);
    HGDIOBJ o[3] = { b, p, bm };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0xdeadbeef);
        EQ(U(GetDCBrushColor)((HDC)o[i]), CLR_INVALID);
        DCL_WERR(ERROR_INVALID_PARAMETER);
        EQ(U(GetDCPenColor)((HDC)o[i]), CLR_INVALID);
        EQ(U(SetDCBrushColor)((HDC)o[i], 5), CLR_INVALID);
        EQ(U(SetDCPenColor)((HDC)o[i], 5), CLR_INVALID);
        U(DeleteObject)(o[i]);
    }
}

TEST(dcl_dcbrush_draw_patblt) {
    REQUIRE(U(SetDCBrushColor)); REQUIRE(U(PatBlt));
    HBITMAP bmp, old; DWORD *bits;
    HDC dc = dcl_mem32(16, 16, &bmp, &old, &bits);
    HGDIOBJ ob = U(SelectObject)(dc, U(GetStockObject)(DC_BRUSH));
    U(SetDCBrushColor)(dc, RGB(255, 0, 0));
    U(PatBlt)(dc, 0, 0, 8, 16, PATCOPY);
    U(SetDCBrushColor)(dc, RGB(0, 0, 255));
    U(PatBlt)(dc, 8, 0, 8, 16, PATCOPY);
    U(GdiFlush)();
    EQ(bits[0], DCL_RGB32(255, 0, 0));
    EQ(bits[7], DCL_RGB32(255, 0, 0));
    EQ(bits[8], DCL_RGB32(0, 0, 255));
    EQ(bits[15 * 16 + 15], DCL_RGB32(0, 0, 255));
    EQ(bits[15 * 16 + 7], DCL_RGB32(255, 0, 0));
    U(SelectObject)(dc, ob);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_dcbrush_draw_many_colours) {
    REQUIRE(U(SetDCBrushColor)); REQUIRE(U(PatBlt));
    HBITMAP bmp, old; DWORD *bits;
    HDC dc = dcl_mem32(8, 8, &bmp, &old, &bits);
    HGDIOBJ ob = U(SelectObject)(dc, U(GetStockObject)(DC_BRUSH));
    int bad = 0;
    for (int i = 0; i < 256; i += 5) {
        COLORREF c = RGB(i, 255 - i, (i * 7) & 255);
        U(SetDCBrushColor)(dc, c);
        U(PatBlt)(dc, 0, 0, 8, 8, PATCOPY);
        U(GdiFlush)();
        if (bits[3 * 8 + 3] != DCL_RGB32(i, 255 - i, (i * 7) & 255)) bad++;
    }
    CHECK(bad == 0);
    U(SelectObject)(dc, ob);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_dcbrush_set_while_not_selected) {
    REQUIRE(U(SetDCBrushColor)); REQUIRE(U(PatBlt));
    HBITMAP bmp, old; DWORD *bits;
    HDC dc = dcl_mem32(8, 8, &bmp, &old, &bits);
    U(SetDCBrushColor)(dc, RGB(0, 255, 0));
    HGDIOBJ ob = U(SelectObject)(dc, U(GetStockObject)(DC_BRUSH));
    U(PatBlt)(dc, 0, 0, 8, 8, PATCOPY);
    U(GdiFlush)();
    EQ(bits[0], DCL_RGB32(0, 255, 0));
    U(SelectObject)(dc, ob);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_dcbrush_does_not_affect_other_brush) {
    REQUIRE(U(SetDCBrushColor)); REQUIRE(U(PatBlt));
    HBITMAP bmp, old; DWORD *bits;
    HDC dc = dcl_mem32(8, 8, &bmp, &old, &bits);
    HBRUSH br = U(CreateSolidBrush)(RGB(10, 20, 30));
    HGDIOBJ ob = U(SelectObject)(dc, br);
    U(SetDCBrushColor)(dc, RGB(200, 100, 50));
    U(PatBlt)(dc, 0, 0, 8, 8, PATCOPY);
    U(GdiFlush)();
    EQ(bits[0], DCL_RGB32(10, 20, 30));
    U(SelectObject)(dc, ob);
    U(DeleteObject)(br);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_dcpen_draw_line) {
    REQUIRE(U(SetDCPenColor)); REQUIRE(U(LineTo));
    HBITMAP bmp, old; DWORD *bits;
    HDC dc = dcl_mem32(16, 8, &bmp, &old, &bits);
    HGDIOBJ op = U(SelectObject)(dc, U(GetStockObject)(DC_PEN));
    U(SetDCPenColor)(dc, RGB(0, 200, 0));
    U(MoveToEx)(dc, 1, 2, NULL);
    U(LineTo)(dc, 10, 2);
    U(SetDCPenColor)(dc, RGB(200, 0, 100));
    U(MoveToEx)(dc, 1, 5, NULL);
    U(LineTo)(dc, 10, 5);
    U(GdiFlush)();
    EQ(bits[2 * 16 + 0], 0);
    EQ(bits[2 * 16 + 1], DCL_RGB32(0, 200, 0));
    EQ(bits[2 * 16 + 9], DCL_RGB32(0, 200, 0));
    EQ(bits[2 * 16 + 10], 0); /* end point excluded */
    EQ(bits[5 * 16 + 5], DCL_RGB32(200, 0, 100));
    EQ(bits[3 * 16 + 5], 0);
    U(SelectObject)(dc, op);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_dcpen_rectangle_outline_and_fill) {
    REQUIRE(U(SetDCPenColor)); REQUIRE(U(SetDCBrushColor)); REQUIRE(U(Rectangle));
    HBITMAP bmp, old; DWORD *bits;
    HDC dc = dcl_mem32(12, 12, &bmp, &old, &bits);
    HGDIOBJ op = U(SelectObject)(dc, U(GetStockObject)(DC_PEN));
    HGDIOBJ ob = U(SelectObject)(dc, U(GetStockObject)(DC_BRUSH));
    U(SetDCPenColor)(dc, RGB(255, 255, 0));
    U(SetDCBrushColor)(dc, RGB(0, 255, 255));
    U(Rectangle)(dc, 2, 2, 10, 10);
    U(GdiFlush)();
    EQ(bits[2 * 12 + 2], DCL_RGB32(255, 255, 0));
    EQ(bits[2 * 12 + 9], DCL_RGB32(255, 255, 0));
    EQ(bits[9 * 12 + 2], DCL_RGB32(255, 255, 0));
    EQ(bits[5 * 12 + 5], DCL_RGB32(0, 255, 255));
    EQ(bits[1 * 12 + 1], 0);
    EQ(bits[10 * 12 + 10], 0);
    U(SelectObject)(dc, op); U(SelectObject)(dc, ob);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_dcpen_fillrect_with_dcbrush) {
    REQUIRE(U(SetDCBrushColor)); REQUIRE(U(FillRect));
    HBITMAP bmp, old; DWORD *bits;
    HDC dc = dcl_mem32(8, 8, &bmp, &old, &bits);
    U(SetDCBrushColor)(dc, RGB(17, 34, 51));
    RECT r = { 1, 1, 5, 5 };
    U(FillRect)(dc, &r, (HBRUSH)U(GetStockObject)(DC_BRUSH));
    U(GdiFlush)();
    EQ(bits[1 * 8 + 1], DCL_RGB32(17, 34, 51));
    EQ(bits[4 * 8 + 4], DCL_RGB32(17, 34, 51));
    EQ(bits[5 * 8 + 5], 0);
    EQ(bits[0], 0);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_dcbrush_default_white_draws_white) {
    REQUIRE(U(PatBlt));
    HBITMAP bmp, old; DWORD *bits;
    HDC dc = dcl_mem32(4, 4, &bmp, &old, &bits);
    HGDIOBJ ob = U(SelectObject)(dc, U(GetStockObject)(DC_BRUSH));
    U(PatBlt)(dc, 0, 0, 4, 4, PATCOPY);
    U(GdiFlush)();
    EQ(bits[5], DCL_RGB32(255, 255, 255));
    U(SelectObject)(dc, ob);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_dcpen_default_black_draws_black) {
    REQUIRE(U(LineTo));
    HBITMAP bmp, old; DWORD *bits;
    HDC dc = dcl_mem32(8, 4, &bmp, &old, &bits);
    for (int i = 0; i < 32; i++) bits[i] = 0x00aabbcc;
    HGDIOBJ op = U(SelectObject)(dc, U(GetStockObject)(DC_PEN));
    U(MoveToEx)(dc, 0, 1, NULL);
    U(LineTo)(dc, 8, 1);
    U(GdiFlush)();
    EQ(bits[1 * 8 + 3], 0);
    EQ(bits[2 * 8 + 3], 0x00aabbcc);
    U(SelectObject)(dc, op);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_dccolors_gdi_count_stable) {
    REQUIRE(U(SetDCBrushColor)); REQUIRE(U(GetGuiResources));
    HDC dc = dcl_mem();
    HGDIOBJ ob = U(SelectObject)(dc, U(GetStockObject)(DC_BRUSH));
    HGDIOBJ op = U(SelectObject)(dc, U(GetStockObject)(DC_PEN));
    U(SetDCBrushColor)(dc, 1); U(SetDCPenColor)(dc, 1);
    DWORD before = dcl_gdicount();
    for (int i = 0; i < 1000; i++) { U(SetDCBrushColor)(dc, RGB(i & 255, 0, 0)); U(SetDCPenColor)(dc, RGB(0, i & 255, 0)); }
    EQ(dcl_gdicount(), before);
    U(SelectObject)(dc, ob); U(SelectObject)(dc, op);
    U(DeleteDC)(dc);
}

/* ------------------------------------------------------------- GetDCOrgEx */

TEST(dcl_dcorg_memdc_zero) {
    REQUIRE(U(GetDCOrgEx));
    HDC dc = dcl_mem();
    POINT p = { 77, 88 };
    CHECK(U(GetDCOrgEx)(dc, &p));
    CHECK(p.x == 0 && p.y == 0);
    U(DeleteDC)(dc);
}

TEST(dcl_dcorg_memdc_dib_zero) {
    REQUIRE(U(GetDCOrgEx));
    HBITMAP bmp, old; DWORD *bits;
    HDC dc = dcl_mem32(10, 10, &bmp, &old, &bits);
    POINT p = { 77, 88 };
    CHECK(U(GetDCOrgEx)(dc, &p));
    CHECK(p.x == 0 && p.y == 0);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_dcorg_unaffected_by_viewport) {
    REQUIRE(U(GetDCOrgEx));
    HDC dc = dcl_mem();
    U(SetMapMode)(dc, MM_ANISOTROPIC);
    U(SetViewportOrgEx)(dc, 50, 60, NULL);
    U(SetWindowOrgEx)(dc, -5, -6, NULL);
    POINT p = { 77, 88 };
    CHECK(U(GetDCOrgEx)(dc, &p));
    CHECK(p.x == 0 && p.y == 0);
    U(DeleteDC)(dc);
}

TEST(dcl_dcorg_invalid_handle) {
    REQUIRE(U(GetDCOrgEx));
    POINT p = { 1, 2 };
    CHECK(!U(GetDCOrgEx)(NULL, &p));
    CHECK(!U(GetDCOrgEx)((HDC)(ULONG_PTR)0xdeadbeef, &p));
    CHECK(!U(GetDCOrgEx)((HDC)(ULONG_PTR)0xffffffff, &p));
}

TEST(dcl_dcorg_deleted_dc) {
    REQUIRE(U(GetDCOrgEx));
    HDC dc = dcl_mem();
    U(DeleteDC)(dc);
    POINT p = { 1, 2 };
    CHECK(!U(GetDCOrgEx)(dc, &p));
}

TEST(dcl_dcorg_non_dc_object) {
    REQUIRE(U(GetDCOrgEx));
    HGDIOBJ b = U(CreateSolidBrush)(0);
    POINT p = { 1, 2 };
    CHECK(!U(GetDCOrgEx)((HDC)b, &p));
    U(DeleteObject)(b);
}

TEST(dcl_dcorg_window_client) {
    if (dcl_wine()) { t_skip("emulator hangs on window show"); return; }
    REQUIRE(U(GetDCOrgEx)); REQUIRE(U(GetDC)); REQUIRE(U(ClientToScreen)); REQUIRE(U(ShowWindow));
    HWND w = dcl_window(120, 90, 140, 110);
    if (!w) { t_skip("no window"); return; }
    U(ShowWindow)(w, SW_SHOWNA);
    t_pump();
    HDC dc = U(GetDC)(w);
    POINT o = { -1, -1 }, c = { 0, 0 };
    CHECK(U(GetDCOrgEx)(dc, &o));
    U(ClientToScreen)(w, &c);
    CHECKF(o.x == c.x && o.y == c.y, "DC origin (%ld,%ld) client (%ld,%ld)", o.x, o.y, c.x, c.y);
    U(ReleaseDC)(w, dc);
    U(DestroyWindow)(w);
}

TEST(dcl_dcorg_window_dc) {
    if (dcl_wine()) { t_skip("emulator hangs on window show"); return; }
    REQUIRE(U(GetDCOrgEx)); REQUIRE(U(GetWindowDC)); REQUIRE(U(GetWindowRect)); REQUIRE(U(ShowWindow));
    HWND w = dcl_window(130, 100, 140, 110);
    if (!w) { t_skip("no window"); return; }
    U(ShowWindow)(w, SW_SHOWNA);
    t_pump();
    HDC dc = U(GetWindowDC)(w);
    POINT o = { -1, -1 };
    RECT r;
    CHECK(U(GetDCOrgEx)(dc, &o));
    U(GetWindowRect)(w, &r);
    CHECKF(o.x == r.left && o.y == r.top, "DC origin (%ld,%ld) window (%ld,%ld)", o.x, o.y, r.left, r.top);
    U(ReleaseDC)(w, dc);
    U(DestroyWindow)(w);
}

TEST(dcl_dcorg_screen_dc_note) {
    REQUIRE(U(GetDCOrgEx));
    if (dcl_wine()) { t_skip("emulator hangs on display-DC origin"); return; }
    HDC a = dcl_screen(), b = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    POINT p = { -1, -1 }, q = { -1, -1 };
    CHECK(U(GetDCOrgEx)(a, &p));
    CHECK(U(GetDCOrgEx)(b, &q));
    t_note("GetDC(NULL) origin (%ld,%ld); CreateDC origin (%ld,%ld)", p.x, p.y, q.x, q.y);
    U(DeleteDC)(b); dcl_release_screen(a);
}

/* ------------------------------------------------------------- batch / flush */

TEST(dcl_gdiflush_returns_true) {
    REQUIRE(U(GdiFlush));
    CHECK(U(GdiFlush)());
    CHECK(U(GdiFlush)());
}

TEST(dcl_gdiflush_makes_dib_current) {
    REQUIRE(U(GdiFlush)); REQUIRE(U(SetPixel));
    HBITMAP bmp, old; DWORD *bits;
    HDC dc = dcl_mem32(4, 4, &bmp, &old, &bits);
    U(SetPixelV)(dc, 1, 1, RGB(1, 2, 3));
    U(GdiFlush)();
    EQ(bits[1 * 4 + 1], DCL_RGB32(1, 2, 3));
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_batchlimit_default_and_range) {
    REQUIRE(U(GdiGetBatchLimit)); REQUIRE(U(GdiSetBatchLimit));
    DWORD orig = U(GdiGetBatchLimit)();
    CHECKF(orig >= 1 && orig <= 20, "limit %lu", orig);
    t_note("default GdiGetBatchLimit = %lu", orig);
    U(GdiSetBatchLimit)(orig);
}

TEST(dcl_batchlimit_set_returns_previous) {
    REQUIRE(U(GdiGetBatchLimit)); REQUIRE(U(GdiSetBatchLimit));
    DWORD orig = U(GdiGetBatchLimit)();
    WEQ(U(GdiSetBatchLimit)(5), orig);
    WEQ(U(GdiGetBatchLimit)(), 5);
    WEQ(U(GdiSetBatchLimit)(7), 5);
    WEQ(U(GdiGetBatchLimit)(), 7);
    U(GdiSetBatchLimit)(orig);
    WEQ(U(GdiGetBatchLimit)(), orig);
}

TEST(dcl_batchlimit_full_range) {
    REQUIRE(U(GdiGetBatchLimit)); REQUIRE(U(GdiSetBatchLimit));
    DWORD orig = U(GdiGetBatchLimit)();
    for (DWORD i = 1; i <= 20; i++) {
        U(GdiSetBatchLimit)(i);
        WEQ(U(GdiGetBatchLimit)(), i);
    }
    U(GdiSetBatchLimit)(orig);
}

TEST(dcl_batchlimit_zero_means_default) {
    REQUIRE(U(GdiGetBatchLimit)); REQUIRE(U(GdiSetBatchLimit));
    DWORD orig = U(GdiGetBatchLimit)();
    U(GdiSetBatchLimit)(3);
    WEQ(U(GdiSetBatchLimit)(0), 3);
    WEQ(U(GdiGetBatchLimit)(), 20);
    U(GdiSetBatchLimit)(orig);
}

TEST(dcl_batchlimit_out_of_range_rejected) {
    REQUIRE(U(GdiGetBatchLimit)); REQUIRE(U(GdiSetBatchLimit));
    DWORD orig = U(GdiGetBatchLimit)();
    U(GdiSetBatchLimit)(9);
    static const DWORD bad[] = { 21, 22, 100, 255, 256, 0x10000, 0x7fffffff, 0x80000000, 0xffffffff };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        DWORD r = U(GdiSetBatchLimit)(bad[i]);
        DCL_WCHECK(r == 0);
        DCL_WCHECK(U(GdiGetBatchLimit)() == 9);
    }
    U(GdiSetBatchLimit)(orig);
}

TEST(dcl_batchlimit_unchanged_by_drawing) {
    REQUIRE(U(GdiGetBatchLimit)); REQUIRE(U(GdiSetBatchLimit));
    DWORD orig = U(GdiGetBatchLimit)();
    HBITMAP bmp, old; DWORD *bits;
    HDC dc = dcl_mem32(16, 16, &bmp, &old, &bits);
    U(GdiSetBatchLimit)(2);
    for (int i = 0; i < 50; i++) U(SetPixelV)(dc, i % 16, i / 16, RGB(i, i, i));
    U(GdiFlush)();
    DCL_WCHECK(U(GdiGetBatchLimit)() == 2);
    EQ(bits[0], 0);
    EQ(bits[1], DCL_RGB32(1, 1, 1));
    EQ(bits[2 * 16 + 1], DCL_RGB32(33, 33, 33));
    U(GdiSetBatchLimit)(orig);
    dcl_free_mem(dc, bmp, old);
}

/* ------------------------------------------------------------- bounds rect */

static HDC bounds_dc(HBITMAP *bmp, HBITMAP *old, int w, int h) {
    HDC dc = dcl_mem32(w, h, bmp, old, NULL);
    return dc;
}

TEST(dcl_bounds_null_args) {
    REQUIRE(U(GetBoundsRect)); REQUIRE(U(SetBoundsRect));
    HBITMAP bmp, old; HDC dc = bounds_dc(&bmp, &old, 8, 8);
    EQ(U(GetBoundsRect)(dc, NULL, 0), 0);
    EQ(U(GetBoundsRect)(dc, NULL, ~0U), 0);
    EQ(U(GetBoundsRect)(NULL, NULL, 0), 0);
    EQ(U(GetBoundsRect)(NULL, NULL, ~0U), 0);
    EQ(U(SetBoundsRect)(NULL, NULL, 0), 0);
    EQ(U(SetBoundsRect)(NULL, NULL, ~0U), 0);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_bounds_invalid_handle) {
    REQUIRE(U(GetBoundsRect)); REQUIRE(U(SetBoundsRect));
    RECT r = { 1, 2, 3, 4 };
    EQ(U(GetBoundsRect)((HDC)(ULONG_PTR)0xdeadbeef, &r, 0), 0);
    EQ(U(SetBoundsRect)((HDC)(ULONG_PTR)0xdeadbeef, &r, DCB_SET), 0);
    EQ(U(GetBoundsRect)(NULL, &r, 0), 0);
    EQ(U(SetBoundsRect)(NULL, &r, DCB_RESET), 0);
    HDC dc = dcl_mem(); U(DeleteDC)(dc);
    EQ(U(GetBoundsRect)(dc, &r, 0), 0);
    EQ(U(SetBoundsRect)(dc, &r, DCB_SET), 0);
}

TEST(dcl_bounds_initial_state) {
    REQUIRE(U(GetBoundsRect));
    HBITMAP bmp, old; HDC dc = bounds_dc(&bmp, &old, 16, 16);
    RECT r = { 9, 9, 9, 9 };
    EQ(U(GetBoundsRect)(dc, &r, 0), DCB_RESET);
    CHECK(r.left == 0 && r.top == 0 && r.right == 0 && r.bottom == 0);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_bounds_initial_state_screen) {
    REQUIRE(U(GetBoundsRect));
    if (dcl_wine()) { t_skip("emulator hangs on display-DC bounds"); return; }
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    RECT r = { 9, 9, 9, 9 };
    UINT t = U(GetBoundsRect)(dc, &r, 0);
    CHECKF(t == DCB_RESET, "type %x", t);
    U(DeleteDC)(dc);
}

TEST(dcl_bounds_enable_returns_previous_state) {
    REQUIRE(U(SetBoundsRect));
    HBITMAP bmp, old; HDC dc = bounds_dc(&bmp, &old, 16, 16);
    EQ(U(SetBoundsRect)(dc, NULL, DCB_RESET | DCB_ENABLE), DCB_RESET | DCB_DISABLE);
    EQ(U(SetBoundsRect)(dc, NULL, DCB_RESET | DCB_ENABLE), DCB_RESET | DCB_ENABLE);
    EQ(U(SetBoundsRect)(dc, NULL, DCB_RESET | DCB_DISABLE), DCB_RESET | DCB_ENABLE);
    EQ(U(SetBoundsRect)(dc, NULL, DCB_RESET | DCB_DISABLE), DCB_RESET | DCB_DISABLE);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_bounds_set_and_get) {
    REQUIRE(U(SetBoundsRect)); REQUIRE(U(GetBoundsRect));
    HBITMAP bmp, old; HDC dc = bounds_dc(&bmp, &old, 200, 200);
    RECT s = { 10, 20, 40, 50 }, r;
    EQ(U(SetBoundsRect)(dc, &s, DCB_SET), DCB_RESET | DCB_DISABLE);
    memset(&r, 0, sizeof r);
    EQ(U(GetBoundsRect)(dc, &r, 0), DCB_SET);
    CHECK(r.left == 10 && r.top == 20 && r.right == 40 && r.bottom == 50);
    EQ(U(SetBoundsRect)(dc, &s, DCB_SET), DCB_SET | DCB_DISABLE);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_bounds_get_with_reset_flag) {
    REQUIRE(U(SetBoundsRect)); REQUIRE(U(GetBoundsRect));
    HBITMAP bmp, old; HDC dc = bounds_dc(&bmp, &old, 100, 100);
    RECT s = { 5, 6, 30, 40 }, r;
    U(SetBoundsRect)(dc, &s, DCB_SET);
    EQ(U(GetBoundsRect)(dc, &r, DCB_RESET), DCB_SET);
    CHECK(r.left == 5 && r.top == 6 && r.right == 30 && r.bottom == 40);
    memset(&r, 0x55, sizeof r);
    EQ(U(GetBoundsRect)(dc, &r, 0), DCB_RESET);
    CHECK(r.left == 0 && r.top == 0 && r.right == 0 && r.bottom == 0);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_bounds_null_rect_with_reset) {
    REQUIRE(U(SetBoundsRect)); REQUIRE(U(GetBoundsRect));
    HBITMAP bmp, old; HDC dc = bounds_dc(&bmp, &old, 100, 100);
    RECT s = { 5, 6, 30, 40 }, r;
    U(SetBoundsRect)(dc, &s, DCB_SET);
    EQ(U(GetBoundsRect)(dc, NULL, DCB_RESET), 0);
    EQ(U(GetBoundsRect)(dc, &r, 0), DCB_RESET);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_bounds_empty_rect_resets) {
    REQUIRE(U(SetBoundsRect)); REQUIRE(U(GetBoundsRect));
    HBITMAP bmp, old; HDC dc = bounds_dc(&bmp, &old, 100, 100);
    RECT s = { 20, 20, 10, 10 }, r;
    U(SetBoundsRect)(dc, &s, DCB_SET);
    EQ(U(GetBoundsRect)(dc, &r, 0), DCB_RESET);
    CHECK(r.left == 0 && r.top == 0 && r.right == 0 && r.bottom == 0);
    RECT z = { 20, 20, 20, 20 };
    U(SetBoundsRect)(dc, &z, DCB_SET);
    t_note("zero-area rect DCB_SET -> state %x", U(GetBoundsRect)(dc, &r, 0));
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_bounds_anisotropic_mapping) {
    REQUIRE(U(SetBoundsRect)); REQUIRE(U(GetBoundsRect));
    HBITMAP bmp, old; HDC dc = bounds_dc(&bmp, &old, 200, 200);
    RECT s = { 10, 20, 40, 50 }, r;
    U(SetBoundsRect)(dc, &s, DCB_SET);
    U(SetMapMode)(dc, MM_ANISOTROPIC);
    U(SetViewportExtEx)(dc, 2, 2, NULL);
    EQ(U(GetBoundsRect)(dc, &r, 0), DCB_SET);
    CHECK(r.left == 5 && r.top == 10 && r.right == 20 && r.bottom == 25);
    U(SetViewportOrgEx)(dc, 20, 30, NULL);
    EQ(U(GetBoundsRect)(dc, &r, 0), DCB_SET);
    CHECK(r.left == -5 && r.top == -5 && r.right == 10 && r.bottom == 10);
    /* set in logical coordinates and read back in device space */
    U(SetBoundsRect)(dc, &s, DCB_SET);
    U(SetMapMode)(dc, MM_TEXT);
    U(SetViewportOrgEx)(dc, 0, 0, NULL);
    EQ(U(GetBoundsRect)(dc, &r, 0), DCB_SET);
    CHECK(r.left == 40 && r.top == 70 && r.right == 100 && r.bottom == 130);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_bounds_draw_requires_enable) {
    REQUIRE(U(SetBoundsRect)); REQUIRE(U(GetBoundsRect)); REQUIRE(U(Rectangle));
    HBITMAP bmp, old; HDC dc = bounds_dc(&bmp, &old, 64, 64);
    RECT r;
    U(Rectangle)(dc, 5, 5, 10, 10);
    EQ(U(GetBoundsRect)(dc, &r, 0), DCB_RESET);
    U(SetBoundsRect)(dc, NULL, DCB_RESET | DCB_ENABLE);
    U(Rectangle)(dc, 2, 2, 4, 4);
    EQ(U(GetBoundsRect)(dc, &r, DCB_RESET), DCB_SET);
    CHECK(r.left == 2 && r.top == 2 && r.right == 4 && r.bottom == 4);
    EQ(U(GetBoundsRect)(dc, &r, 0), DCB_RESET);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_bounds_line_accumulates) {
    REQUIRE(U(SetBoundsRect)); REQUIRE(U(GetBoundsRect)); REQUIRE(U(LineTo));
    HBITMAP bmp, old; HDC dc = bounds_dc(&bmp, &old, 256, 256);
    RECT r;
    U(SetBoundsRect)(dc, NULL, DCB_RESET | DCB_ENABLE);
    U(MoveToEx)(dc, 10, 10, NULL);
    U(LineTo)(dc, 20, 20);
    EQ(U(GetBoundsRect)(dc, &r, 0), DCB_SET);
    CHECK(r.left == 10 && r.top == 10 && r.right == 21 && r.bottom == 21);
    U(LineTo)(dc, 30, 25);
    U(GetBoundsRect)(dc, &r, 0);
    CHECK(r.left == 10 && r.top == 10 && r.right == 31 && r.bottom == 26);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_bounds_accumulate_flag) {
    REQUIRE(U(SetBoundsRect)); REQUIRE(U(GetBoundsRect)); REQUIRE(U(LineTo));
    HBITMAP bmp, old; HDC dc = bounds_dc(&bmp, &old, 256, 256);
    RECT r, a = { 8, 8, 23, 23 };
    U(SetBoundsRect)(dc, NULL, DCB_RESET | DCB_ENABLE);
    U(MoveToEx)(dc, 10, 10, NULL);
    U(LineTo)(dc, 20, 20);
    U(SetBoundsRect)(dc, &a, DCB_ACCUMULATE);
    EQ(U(GetBoundsRect)(dc, &r, 0), DCB_SET);
    CHECK(r.left == 8 && r.top == 8 && r.right == 23 && r.bottom == 23);
    RECT b = { 30, 3, 40, 12 };
    U(SetBoundsRect)(dc, &b, DCB_ACCUMULATE);
    U(GetBoundsRect)(dc, &r, 0);
    CHECK(r.left == 8 && r.top == 3 && r.right == 40 && r.bottom == 23);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_bounds_accumulate_into_empty) {
    REQUIRE(U(SetBoundsRect)); REQUIRE(U(GetBoundsRect));
    HBITMAP bmp, old; HDC dc = bounds_dc(&bmp, &old, 100, 100);
    RECT r, a = { 4, 5, 14, 15 };
    U(SetBoundsRect)(dc, &a, DCB_ACCUMULATE);
    UINT t = U(GetBoundsRect)(dc, &r, 0);
    t_note("accumulate into reset bounds: type %x rect (%ld,%ld,%ld,%ld)", t, r.left, r.top, r.right, r.bottom);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_bounds_disable_stops_accumulation) {
    REQUIRE(U(SetBoundsRect)); REQUIRE(U(GetBoundsRect)); REQUIRE(U(LineTo));
    HBITMAP bmp, old; HDC dc = bounds_dc(&bmp, &old, 256, 256);
    RECT r;
    U(SetBoundsRect)(dc, NULL, DCB_RESET | DCB_ENABLE);
    U(MoveToEx)(dc, 10, 10, NULL);
    U(LineTo)(dc, 20, 20);
    U(SetBoundsRect)(dc, NULL, DCB_DISABLE);
    U(LineTo)(dc, 40, 40);
    EQ(U(GetBoundsRect)(dc, &r, 0), DCB_SET);
    CHECK(r.left == 10 && r.top == 10 && r.right == 21 && r.bottom == 21);
    U(SetBoundsRect)(dc, NULL, DCB_ENABLE);
    U(LineTo)(dc, 50, 45);
    U(GetBoundsRect)(dc, &r, 0);
    CHECK(r.left == 10 && r.top == 10 && r.right == 51 && r.bottom == 46);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_bounds_clamped_to_surface) {
    REQUIRE(U(SetBoundsRect)); REQUIRE(U(GetBoundsRect)); REQUIRE(U(LineTo));
    HBITMAP bmp, old; HDC dc = bounds_dc(&bmp, &old, 256, 256);
    RECT r;
    U(SetBoundsRect)(dc, NULL, DCB_RESET | DCB_ENABLE);
    U(MoveToEx)(dc, 6, 6, NULL);
    U(LineTo)(dc, 55, 30);
    U(LineTo)(dc, 300, 30);
    U(GetBoundsRect)(dc, &r, 0);
    CHECK(r.left == 6 && r.top == 6 && r.right == 256 && r.bottom == 31);
    U(LineTo)(dc, -300, -300);
    U(GetBoundsRect)(dc, &r, 0);
    CHECK(r.left == 0 && r.top == 0 && r.right == 256 && r.bottom == 31);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_bounds_not_saved_by_savedc) {
    REQUIRE(U(SetBoundsRect)); REQUIRE(U(GetBoundsRect)); REQUIRE(U(LineTo));
    HBITMAP bmp, old; HDC dc = bounds_dc(&bmp, &old, 256, 256);
    RECT r;
    U(SetBoundsRect)(dc, NULL, DCB_RESET | DCB_ENABLE);
    U(MoveToEx)(dc, 10, 10, NULL);
    U(LineTo)(dc, 20, 20);
    int lvl = U(SaveDC)(dc);
    U(LineTo)(dc, 30, 25);
    U(RestoreDC)(dc, lvl);
    EQ(U(GetBoundsRect)(dc, &r, 0), DCB_SET);
    CHECK(r.left == 10 && r.top == 10 && r.right == 31 && r.bottom == 26);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_bounds_screen_dc_flags_roundtrip) {
    REQUIRE(U(SetBoundsRect));
    if (dcl_wine()) { t_skip("emulator hangs on display-DC bounds"); return; }
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    UINT r = U(SetBoundsRect)(dc, NULL, DCB_RESET | DCB_ENABLE);
    CHECKF(r == (DCB_RESET | DCB_DISABLE), "got %x", r);
    r = U(SetBoundsRect)(dc, NULL, DCB_RESET | DCB_DISABLE);
    CHECKF(r == (DCB_RESET | DCB_ENABLE), "got %x", r);
    U(DeleteDC)(dc);
}

TEST(dcl_bounds_reserved_flag_bits_ignored) {
    REQUIRE(U(SetBoundsRect)); REQUIRE(U(GetBoundsRect));
    HBITMAP bmp, old; HDC dc = bounds_dc(&bmp, &old, 64, 64);
    RECT s = { 1, 2, 3, 4 }, r;
    /* bits 0x4000 and 0x8000 are masked off before the call */
    UINT a = U(SetBoundsRect)(dc, &s, DCB_SET | 0x8000 | 0x4000);
    EQ(a, DCB_RESET | DCB_DISABLE);
    EQ(U(GetBoundsRect)(dc, &r, 0x8000 | 0x4000), DCB_SET);
    CHECK(r.left == 1 && r.top == 2 && r.right == 3 && r.bottom == 4);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_bounds_per_dc) {
    REQUIRE(U(SetBoundsRect)); REQUIRE(U(GetBoundsRect));
    HBITMAP b1, o1, b2, o2;
    HDC a = bounds_dc(&b1, &o1, 32, 32), b = bounds_dc(&b2, &o2, 32, 32);
    RECT s = { 1, 1, 9, 9 }, r;
    U(SetBoundsRect)(a, &s, DCB_SET);
    EQ(U(GetBoundsRect)(b, &r, 0), DCB_RESET);
    dcl_free_mem(a, b1, o1); dcl_free_mem(b, b2, o2);
}

TEST(dcl_bounds_changing_bitmap) {
    REQUIRE(U(SetBoundsRect)); REQUIRE(U(GetBoundsRect));
    HBITMAP b1, o1; HDC dc = bounds_dc(&b1, &o1, 64, 64);
    RECT s = { 6, 6, 50, 50 }, r;
    U(SetBoundsRect)(dc, &s, DCB_SET);
    HBITMAP small = dcl_dib(dc, 20, 20, 32, NULL);
    U(SelectObject)(dc, small);
    UINT t = U(GetBoundsRect)(dc, &r, 0);
    t_note("after selecting a smaller DIB: type %x rect (%ld,%ld,%ld,%ld)", t, r.left, r.top, r.right, r.bottom);
    U(SelectObject)(dc, b1);
    U(DeleteObject)(small);
    dcl_free_mem(dc, b1, o1);
}

/* ---------------------------------------------------------------- gamma ramp */

TEST(dcl_gamma_get_null_ramp) {
    REQUIRE(U(GetDeviceGammaRamp));
    if (dcl_wine()) { t_skip("emulator faults on a NULL ramp and wedges the display lock"); return; }
    HDC dc = dcl_screen();
    SET_ERR(0xdeadbeef);
    BOOL r = U(GetDeviceGammaRamp)(dc, NULL);
    DCL_WCHECK(!r);
    DCL_WERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xdeadbeef);
    DCL_WCHECK(!U(GetDeviceGammaRamp)(NULL, NULL));
    DCL_WERR(ERROR_INVALID_PARAMETER);
    dcl_release_screen(dc);
}

TEST(dcl_gamma_set_null_ramp) {
    REQUIRE(U(SetDeviceGammaRamp));
    HDC dc = dcl_screen();
    SET_ERR(0xdeadbeef);
    BOOL r = U(SetDeviceGammaRamp)(dc, NULL);
    DCL_WCHECK(!r);
    DCL_WERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xdeadbeef);
    DCL_WCHECK(!U(SetDeviceGammaRamp)(NULL, NULL));
    DCL_WERR(ERROR_INVALID_PARAMETER);
    dcl_release_screen(dc);
}

TEST(dcl_gamma_get_screen_dc) {
    REQUIRE(U(GetDeviceGammaRamp));
    HDC dc = dcl_screen();
    static WORD ramp[3][256];
    memset(ramp, 0xcd, sizeof ramp);
    BOOL r = U(GetDeviceGammaRamp)(dc, ramp);
    t_note("GetDeviceGammaRamp(screen DC) -> %d error %lu", r, GetLastError());
    if (r) {
        t_note("ramp[0][0]=%u ramp[0][255]=%u ramp[1][128]=%u ramp[2][255]=%u", ramp[0][0], ramp[0][255], ramp[1][128], ramp[2][255]);
        CHECK(ramp[0][255] >= ramp[0][0]);
        CHECK(ramp[1][255] >= ramp[1][0]);
        CHECK(ramp[2][255] >= ramp[2][0]);
    }
    dcl_release_screen(dc);
}

TEST(dcl_gamma_get_createdc) {
    REQUIRE(U(GetDeviceGammaRamp));
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    static WORD ramp[3][256];
    BOOL r = U(GetDeviceGammaRamp)(dc, ramp);
    t_note("GetDeviceGammaRamp(CreateDC) -> %d error %lu", r, GetLastError());
    U(DeleteDC)(dc);
}

TEST(dcl_gamma_get_memdc) {
    REQUIRE(U(GetDeviceGammaRamp));
    HDC dc = dcl_mem();
    static WORD ramp[3][256];
    BOOL r = U(GetDeviceGammaRamp)(dc, ramp);
    t_note("GetDeviceGammaRamp(memdc) -> %d error %lu", r, GetLastError());
    U(DeleteDC)(dc);
}

TEST(dcl_gamma_get_invalid_dc) {
    REQUIRE(U(GetDeviceGammaRamp));
    static WORD ramp[3][256];
    CHECK(!U(GetDeviceGammaRamp)(NULL, ramp));
    CHECK(!U(GetDeviceGammaRamp)((HDC)(ULONG_PTR)0xdeadbeef, ramp));
    HDC dc = dcl_mem(); U(DeleteDC)(dc);
    CHECK(!U(GetDeviceGammaRamp)(dc, ramp));
    HGDIOBJ b = U(CreateSolidBrush)(0);
    CHECK(!U(GetDeviceGammaRamp)((HDC)b, ramp));
    U(DeleteObject)(b);
}

TEST(dcl_gamma_set_invalid_dc) {
    /* Only with invalid handles: never touches the display. */
    REQUIRE(U(SetDeviceGammaRamp)); REQUIRE(U(GetDeviceGammaRamp));
    static WORD ramp[3][256];
    HDC scr = dcl_screen();
    if (!U(GetDeviceGammaRamp)(scr, ramp)) { dcl_release_screen(scr); t_skip("no readable ramp"); return; }
    dcl_release_screen(scr);
    CHECK(!U(SetDeviceGammaRamp)(NULL, ramp));
    CHECK(!U(SetDeviceGammaRamp)((HDC)(ULONG_PTR)0xdeadbeef, ramp));
    HDC dc = dcl_mem(); U(DeleteDC)(dc);
    CHECK(!U(SetDeviceGammaRamp)(dc, ramp));
}

TEST(dcl_gamma_get_repeatable) {
    REQUIRE(U(GetDeviceGammaRamp));
    static WORD a[3][256], b[3][256];
    HDC dc = dcl_screen();
    if (!U(GetDeviceGammaRamp)(dc, a)) { dcl_release_screen(dc); t_skip("no readable ramp"); return; }
    CHECK(U(GetDeviceGammaRamp)(dc, b));
    CHECK(!memcmp(a, b, sizeof a));
    dcl_release_screen(dc);
}

TEST(dcl_gamma_get_same_on_two_dcs) {
    REQUIRE(U(GetDeviceGammaRamp));
    static WORD a[3][256], b[3][256];
    HDC d1 = dcl_screen(), d2 = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    if (U(GetDeviceGammaRamp)(d1, a) && U(GetDeviceGammaRamp)(d2, b))
        CHECK(!memcmp(a, b, sizeof a));
    U(DeleteDC)(d2); dcl_release_screen(d1);
}
