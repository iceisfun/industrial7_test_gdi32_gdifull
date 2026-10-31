#include "pal.h"

/* ---- COLORREF round trips ---- */
TEST(palette_bkcolor_default_and_roundtrip) {
    REQUIRE(U(SetBkColor)); REQUIRE(U(GetBkColor));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 4, 4)) { t_skip("no dib"); return; }
    EXPECT_EQ(U(GetBkColor)(d.dc), RGB(255, 255, 255));
    static const COLORREF cs[] = {0, RGB(1, 2, 3), RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255), RGB(18, 52, 86), RGB(255, 255, 255)};
    COLORREF prev = RGB(255, 255, 255);
    for (unsigned i = 0; i < sizeof cs / sizeof *cs; i++) {
        EXPECT_EQ(U(SetBkColor)(d.dc, cs[i]), prev);
        EXPECT_EQ(U(GetBkColor)(d.dc), cs[i]);
        prev = cs[i];
    }
    dibdc_free(&d);
}

TEST(palette_textcolor_default_and_roundtrip) {
    REQUIRE(U(SetTextColor)); REQUIRE(U(GetTextColor));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 4, 4)) { t_skip("no dib"); return; }
    EXPECT_EQ(U(GetTextColor)(d.dc), 0);
    static const COLORREF cs[] = {RGB(255, 255, 255), RGB(9, 8, 7), RGB(0, 0, 0), RGB(200, 100, 50)};
    COLORREF prev = 0;
    for (unsigned i = 0; i < sizeof cs / sizeof *cs; i++) {
        EXPECT_EQ(U(SetTextColor)(d.dc, cs[i]), prev);
        EXPECT_EQ(U(GetTextColor)(d.dc), cs[i]);
        prev = cs[i];
    }
    dibdc_free(&d);
}

TEST(palette_bk_text_invalid_dc) {
    REQUIRE(U(SetBkColor)); REQUIRE(U(GetBkColor)); REQUIRE(U(SetTextColor)); REQUIRE(U(GetTextColor));
    SetLastError(0);
    EXPECT_EQ(U(SetBkColor)(NULL, 0), CLR_INVALID);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    EXPECT_EQ(U(SetBkColor)((HDC)PAL_BOGUS_HANDLE, 0), CLR_INVALID);
    EXPECT_EQ(U(GetBkColor)(NULL), CLR_INVALID);
    EXPECT_EQ(U(GetBkColor)((HDC)PAL_BOGUS_HANDLE), CLR_INVALID);
    EXPECT_EQ(U(SetTextColor)(NULL, 0), CLR_INVALID);
    EXPECT_EQ(U(GetTextColor)(NULL), CLR_INVALID);
    EXPECT_EQ(U(GetTextColor)((HDC)PAL_BOGUS_HANDLE), CLR_INVALID);
}

TEST(palette_bk_text_wrong_handle_type) {
    REQUIRE(U(SetBkColor)); REQUIRE(U(GetBkColor)); REQUIRE(U(SetTextColor)); REQUIRE(U(GetTextColor)); REQUIRE(U(DeleteObject));
    HPALETTE p = pal_make(2);
    EXPECT_EQ(U(SetBkColor)((HDC)p, 1), CLR_INVALID);
    EXPECT_EQ(U(GetBkColor)((HDC)p), CLR_INVALID);
    EXPECT_EQ(U(SetTextColor)((HDC)p, 1), CLR_INVALID);
    EXPECT_EQ(U(GetTextColor)((HDC)p), CLR_INVALID);
    U(DeleteObject)(p);
}

TEST(palette_bk_text_deleted_dc) {
    REQUIRE(U(SetBkColor)); REQUIRE(U(GetTextColor)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    U(DeleteDC)(dc);
    EXPECT_EQ(U(SetBkColor)(dc, 0), CLR_INVALID);
    EXPECT_EQ(U(GetTextColor)(dc), CLR_INVALID);
}

TEST(palette_bk_color_flag_forms) {
    REQUIRE(U(SetBkColor)); REQUIRE(U(GetBkColor));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 4, 4)) { t_skip("no dib"); return; }
    static const COLORREF forms[] = {0x01000003, 0x02112233, 0x10000004, 0x03112233, 0xff112233, 0x00ffffff};
    for (unsigned i = 0; i < sizeof forms / sizeof *forms; i++) {
        U(SetBkColor)(d.dc, forms[i]);
        t_note("SetBkColor(0x%08lx) -> GetBkColor 0x%08lx", (unsigned long)forms[i], (unsigned long)U(GetBkColor)(d.dc));
    }
    U(SetBkColor)(d.dc, RGB(1, 2, 3));
    EXPECT_EQ(U(GetBkColor)(d.dc), RGB(1, 2, 3));
    dibdc_free(&d);
}

TEST(palette_text_color_flag_forms) {
    REQUIRE(U(SetTextColor)); REQUIRE(U(GetTextColor));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 4, 4)) { t_skip("no dib"); return; }
    static const COLORREF forms[] = {0x01000003, 0x02112233, 0x10000004, 0xff112233};
    for (unsigned i = 0; i < sizeof forms / sizeof *forms; i++) {
        U(SetTextColor)(d.dc, forms[i]);
        t_note("SetTextColor(0x%08lx) -> GetTextColor 0x%08lx", (unsigned long)forms[i], (unsigned long)U(GetTextColor)(d.dc));
    }
    dibdc_free(&d);
}

TEST(palette_bk_independent_per_dc) {
    REQUIRE(U(SetBkColor)); REQUIRE(U(GetBkColor));
    struct dibdc a, b;
    if (!dibdc_create(&a, 32, 4, 4) || !dibdc_create(&b, 32, 4, 4)) { t_skip("no dib"); return; }
    U(SetBkColor)(a.dc, RGB(1, 1, 1));
    U(SetBkColor)(b.dc, RGB(2, 2, 2));
    EXPECT_EQ(U(GetBkColor)(a.dc), RGB(1, 1, 1));
    EXPECT_EQ(U(GetBkColor)(b.dc), RGB(2, 2, 2));
    dibdc_free(&a); dibdc_free(&b);
}

TEST(palette_bk_save_restore_dc) {
    REQUIRE(U(SetBkColor)); REQUIRE(U(GetBkColor)); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(SetTextColor)); REQUIRE(U(GetTextColor));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 4, 4)) { t_skip("no dib"); return; }
    U(SetBkColor)(d.dc, RGB(10, 20, 30));
    U(SetTextColor)(d.dc, RGB(30, 20, 10));
    int id = U(SaveDC)(d.dc);
    U(SetBkColor)(d.dc, RGB(99, 99, 99));
    U(SetTextColor)(d.dc, RGB(88, 88, 88));
    CHECK(U(RestoreDC)(d.dc, id));
    EXPECT_EQ(U(GetBkColor)(d.dc), RGB(10, 20, 30));
    EXPECT_EQ(U(GetTextColor)(d.dc), RGB(30, 20, 10));
    dibdc_free(&d);
}

TEST(palette_dcbrush_dcpen_defaults) {
    REQUIRE(U(GetDCBrushColor)); REQUIRE(U(GetDCPenColor));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 4, 4)) { t_skip("no dib"); return; }
    EXPECT_EQ(U(GetDCBrushColor)(d.dc), RGB(255, 255, 255));
    EXPECT_EQ(U(GetDCPenColor)(d.dc), RGB(0, 0, 0));
    dibdc_free(&d);
}

TEST(palette_dcbrush_dcpen_roundtrip) {
    REQUIRE(U(GetDCBrushColor)); REQUIRE(U(GetDCPenColor)); REQUIRE(U(SetDCBrushColor)); REQUIRE(U(SetDCPenColor));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 4, 4)) { t_skip("no dib"); return; }
    EXPECT_EQ(U(SetDCBrushColor)(d.dc, RGB(1, 2, 3)), RGB(255, 255, 255));
    EXPECT_EQ(U(SetDCBrushColor)(d.dc, RGB(4, 5, 6)), RGB(1, 2, 3));
    EXPECT_EQ(U(GetDCBrushColor)(d.dc), RGB(4, 5, 6));
    EXPECT_EQ(U(SetDCPenColor)(d.dc, RGB(7, 8, 9)), RGB(0, 0, 0));
    EXPECT_EQ(U(SetDCPenColor)(d.dc, RGB(10, 11, 12)), RGB(7, 8, 9));
    EXPECT_EQ(U(GetDCPenColor)(d.dc), RGB(10, 11, 12));
    /* independent of bk/text color */
    EXPECT_EQ(U(GetBkColor)(d.dc), RGB(255, 255, 255));
    dibdc_free(&d);
}

TEST(palette_dcbrush_dcpen_invalid_dc) {
    REQUIRE(U(GetDCBrushColor)); REQUIRE(U(GetDCPenColor)); REQUIRE(U(SetDCBrushColor)); REQUIRE(U(SetDCPenColor));
    EXPECT_EQ(U(SetDCBrushColor)(NULL, 0), CLR_INVALID);
    EXPECT_EQ(U(SetDCPenColor)(NULL, 0), CLR_INVALID);
    EXPECT_EQ(U(GetDCBrushColor)(NULL), CLR_INVALID);
    EXPECT_EQ(U(GetDCPenColor)(NULL), CLR_INVALID);
    EXPECT_EQ(U(SetDCBrushColor)((HDC)PAL_BOGUS_HANDLE, 0), CLR_INVALID);
    EXPECT_EQ(U(GetDCPenColor)((HDC)PAL_BOGUS_HANDLE), CLR_INVALID);
}

TEST(palette_dcbrush_fill_32bpp) {
    REQUIRE(U(SetDCBrushColor)); REQUIRE(U(PatBlt)); REQUIRE(U(GetStockObject)); REQUIRE(U(SelectObject));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 8, 8)) { t_skip("no dib"); return; }
    HGDIOBJ old = U(SelectObject)(d.dc, U(GetStockObject)(DC_BRUSH));
    static const COLORREF cs[] = {RGB(10, 20, 30), RGB(255, 0, 0), RGB(1, 254, 3), RGB(0, 0, 0)};
    for (unsigned i = 0; i < sizeof cs / sizeof *cs; i++) {
        U(SetDCBrushColor)(d.dc, cs[i]);
        CHECK(U(PatBlt)(d.dc, 0, 0, 8, 8, PATCOPY));
        for (int y = 0; y < 8; y += 7)
            for (int x = 0; x < 8; x += 7) {
                BYTE *px = d.bits + y * d.stride + x * 4;
                CHECKF(px[0] == GetBValue(cs[i]) && px[1] == GetGValue(cs[i]) && px[2] == GetRValue(cs[i]), "color %u at %d,%d", i, x, y);
            }
    }
    U(SelectObject)(d.dc, old);
    dibdc_free(&d);
}

TEST(palette_dcpen_line_32bpp) {
    REQUIRE(U(SetDCPenColor)); REQUIRE(U(MoveToEx)); REQUIRE(U(LineTo)); REQUIRE(U(GetStockObject)); REQUIRE(U(SelectObject));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 8, 8)) { t_skip("no dib"); return; }
    HGDIOBJ old = U(SelectObject)(d.dc, U(GetStockObject)(DC_PEN));
    U(SetDCPenColor)(d.dc, RGB(200, 100, 50));
    U(MoveToEx)(d.dc, 0, 3, NULL);
    U(LineTo)(d.dc, 8, 3);
    for (int x = 0; x < 8; x++) {
        BYTE *px = d.bits + 3 * d.stride + x * 4;
        CHECKF(px[0] == 50 && px[1] == 100 && px[2] == 200, "x=%d", x);
    }
    BYTE *other = d.bits + 2 * d.stride;
    EXPECT_EQ(other[0] | other[1] | other[2], 0);
    U(SelectObject)(d.dc, old);
    dibdc_free(&d);
}

/* ---- Pixel level COLORREF on 32bpp ---- */
TEST(palette_setpixel_32bpp_exact) {
    REQUIRE(U(SetPixel)); REQUIRE(U(GetPixel));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 8, 8)) { t_skip("no dib"); return; }
    static const COLORREF cs[] = {0, RGB(1, 2, 3), RGB(255, 255, 255), RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255), RGB(123, 45, 67)};
    for (unsigned i = 0; i < sizeof cs / sizeof *cs; i++) {
        COLORREF r = U(SetPixel)(d.dc, i, 1, cs[i]);
        EXPECT_EQ(r, cs[i]);
        EXPECT_EQ(U(GetPixel)(d.dc, i, 1), cs[i]);
        BYTE *px = d.bits + d.stride + 4 * i;
        EXPECT_EQ(px[0], GetBValue(cs[i]));
        EXPECT_EQ(px[1], GetGValue(cs[i]));
        EXPECT_EQ(px[2], GetRValue(cs[i]));
    }
    dibdc_free(&d);
}

TEST(palette_setpixel_32bpp_paletterbg) {
    REQUIRE(U(SetPixelV)); REQUIRE(U(GetPixel));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 8, 8)) { t_skip("no dib"); return; }
    U(SetPixelV)(d.dc, 0, 0, PALETTERGB(10, 20, 30));
    EXPECT_EQ(U(GetPixel)(d.dc, 0, 0), RGB(10, 20, 30));
    U(SetPixelV)(d.dc, 1, 0, PALETTERGB(255, 255, 255));
    EXPECT_EQ(U(GetPixel)(d.dc, 1, 0), RGB(255, 255, 255));
    dibdc_free(&d);
}

TEST(palette_setpixel_32bpp_paletteindex) {
    REQUIRE(U(SetPixelV)); REQUIRE(U(GetPixel));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 8, 8)) { t_skip("no dib"); return; }
    for (int i = 0; i < 20; i++) {
        U(SetPixelV)(d.dc, i % 8, i / 8, PALETTEINDEX(i));
        t_note("PALETTEINDEX(%d) on 32bpp -> %06lx", i, (unsigned long)U(GetPixel)(d.dc, i % 8, i / 8));
    }
    dibdc_free(&d);
}

TEST(palette_setpixel_32bpp_paletteindex_logical) {
    REQUIRE(U(SetPixelV)); REQUIRE(U(GetPixel)); REQUIRE(U(SelectPalette)); REQUIRE(U(RealizePalette)); REQUIRE(U(DeleteObject));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 8, 8)) { t_skip("no dib"); return; }
    COLORREF c[] = {RGB(10, 20, 30), RGB(40, 50, 60), RGB(70, 80, 90)};
    HPALETTE p = pal_make_rgb(c, 3);
    HPALETTE o = U(SelectPalette)(d.dc, p, FALSE);
    U(RealizePalette)(d.dc);
    for (int i = 0; i < 3; i++) {
        U(SetPixelV)(d.dc, i, 0, PALETTEINDEX(i));
        COLORREF r = U(GetPixel)(d.dc, i, 0);
        if (r != c[i]) t_note("PALETTEINDEX(%d) with logical palette on 32bpp -> %06lx (want %06lx)", i, (unsigned long)r, (unsigned long)c[i]);
    }
    U(SelectPalette)(d.dc, o, FALSE);
    U(DeleteObject)(p);
    dibdc_free(&d);
}

TEST(palette_getpixel_out_of_range) {
    REQUIRE(U(GetPixel));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 4, 4)) { t_skip("no dib"); return; }
    EXPECT_EQ(U(GetPixel)(d.dc, -1, 0), CLR_INVALID);
    EXPECT_EQ(U(GetPixel)(d.dc, 0, -1), CLR_INVALID);
    EXPECT_EQ(U(GetPixel)(d.dc, 4, 0), CLR_INVALID);
    EXPECT_EQ(U(GetPixel)(d.dc, 0, 4), CLR_INVALID);
    EXPECT_NE(U(GetPixel)(d.dc, 3, 3), CLR_INVALID);
    dibdc_free(&d);
}

TEST(palette_setpixel_out_of_range) {
    REQUIRE(U(SetPixel)); REQUIRE(U(SetPixelV));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 4, 4)) { t_skip("no dib"); return; }
    t_note("SetPixel(-1,0)=%08lx SetPixel(4,4)=%08lx SetPixelV(10,10)=%d",
           (unsigned long)U(SetPixel)(d.dc, -1, 0, 0), (unsigned long)U(SetPixel)(d.dc, 4, 4, 0), U(SetPixelV)(d.dc, 10, 10, 0));
    /* nothing may have been written */
    EXPECT_EQ(*(DWORD *)d.bits, 0);
    dibdc_free(&d);
}

TEST(palette_opaque_fill_bkcolor_32bpp) {
    REQUIRE(U(ExtTextOutW)); REQUIRE(U(SetBkColor));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 8, 8)) { t_skip("no dib"); return; }
    RECT rc = {1, 1, 5, 4};
    U(SetBkColor)(d.dc, RGB(11, 22, 33));
    CHECK(U(ExtTextOutW)(d.dc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL));
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            BYTE *px = d.bits + y * d.stride + x * 4;
            int in = x >= 1 && x < 5 && y >= 1 && y < 4;
            if (in) CHECKF(px[0] == 33 && px[1] == 22 && px[2] == 11, "in %d,%d", x, y);
            else CHECKF((px[0] | px[1] | px[2]) == 0, "out %d,%d", x, y);
        }
    dibdc_free(&d);
}

TEST(palette_opaque_fill_bkcolor_8bpp_exact) {
    REQUIRE(U(ExtTextOutW)); REQUIRE(U(SetBkColor));
    struct dibdc d;
    if (!dibdc_create(&d, 8, 8, 8)) { t_skip("no dib"); return; }
    dib_clear_table(d.dc);
    COLORREF tab[] = {RGB(0, 0, 0), RGB(12, 34, 56), RGB(200, 100, 50), RGB(255, 255, 255)};
    dib_set_table(d.dc, tab, 4, 0);
    RECT rc = {0, 0, 3, 3};
    for (int k = 1; k < 4; k++) {
        U(SetBkColor)(d.dc, tab[k]);
        U(ExtTextOutW)(d.dc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL);
        EXPECT_EQ(d.bits[0], k);
        EXPECT_EQ(d.bits[2 * d.stride + 2], k);
        EXPECT_EQ(d.bits[3], 0);          /* x=3 outside rect: unchanged (0 initially, last filled stays) */
        d.bits[3] = 0;
    }
    dibdc_free(&d);
}

TEST(palette_getbkcolor_8bpp_not_quantized) {
    REQUIRE(U(SetBkColor)); REQUIRE(U(GetBkColor));
    struct dibdc d;
    if (!dibdc_create(&d, 8, 4, 4)) { t_skip("no dib"); return; }
    U(SetBkColor)(d.dc, RGB(3, 5, 7));
    EXPECT_EQ(U(GetBkColor)(d.dc), RGB(3, 5, 7));       /* stored as set, not as realized */
    dibdc_free(&d);
}

TEST(palette_mono_dc_colors) {
    REQUIRE(U(SetPixel)); REQUIRE(U(GetPixel)); REQUIRE(U(CreateBitmap)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(SelectObject)); REQUIRE(U(DeleteObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, NULL), old = (HBITMAP)U(SelectObject)(dc, bm);
    U(SetPixelV)(dc, 0, 0, RGB(255, 255, 255));
    U(SetPixelV)(dc, 1, 0, RGB(0, 0, 0));
    U(SetPixelV)(dc, 2, 0, RGB(250, 250, 250));
    U(SetPixelV)(dc, 3, 0, RGB(5, 5, 5));
    EXPECT_EQ(U(GetPixel)(dc, 0, 0), RGB(255, 255, 255));
    EXPECT_EQ(U(GetPixel)(dc, 1, 0), RGB(0, 0, 0));
    EXPECT_EQ(U(GetPixel)(dc, 2, 0), RGB(255, 255, 255));
    EXPECT_EQ(U(GetPixel)(dc, 3, 0), RGB(0, 0, 0));
    U(SelectObject)(dc, old);
    U(DeleteObject)(bm);
    U(DeleteDC)(dc);
}

TEST(palette_brush_logbrush_colorref_forms) {
    REQUIRE(U(CreateSolidBrush)); REQUIRE(U(GetObjectW)); REQUIRE(U(DeleteObject));
    static const COLORREF cs[] = {RGB(1, 2, 3), PALETTERGB(4, 5, 6), PALETTEINDEX(7), 0x10000008};
    for (unsigned i = 0; i < sizeof cs / sizeof *cs; i++) {
        HBRUSH b = U(CreateSolidBrush)(cs[i]);
        CHECKF(b != NULL, "form %u", i);
        LOGBRUSH lb;
        memset(&lb, 0, sizeof lb);
        EXPECT_EQ(U(GetObjectW)(b, sizeof lb, &lb), sizeof lb);
        EXPECT_EQ(lb.lbStyle, BS_SOLID);
        if (i < 2) EXPECT_EQ(lb.lbColor, cs[i]);
        else if (lb.lbColor != cs[i]) t_note("brush color 0x%08lx reads back 0x%08lx", (unsigned long)cs[i], (unsigned long)lb.lbColor);
        U(DeleteObject)(b);
    }
}

TEST(palette_pen_logpen_colorref_forms) {
    REQUIRE(U(CreatePen)); REQUIRE(U(GetObjectW)); REQUIRE(U(DeleteObject));
    static const COLORREF cs[] = {RGB(1, 2, 3), PALETTERGB(4, 5, 6), PALETTEINDEX(7)};
    for (unsigned i = 0; i < sizeof cs / sizeof *cs; i++) {
        HPEN p = U(CreatePen)(PS_SOLID, 1, cs[i]);
        LOGPEN lp;
        memset(&lp, 0, sizeof lp);
        EXPECT_EQ(U(GetObjectW)(p, sizeof lp, &lp), sizeof lp);
        if (i < 2) EXPECT_EQ(lp.lopnColor, cs[i]);
        else if (lp.lopnColor != cs[i]) t_note("pen color 0x%08lx reads back 0x%08lx", (unsigned long)cs[i], (unsigned long)lp.lopnColor);
        U(DeleteObject)(p);
    }
}

TEST(palette_paletterbg_brush_fill_32bpp) {
    REQUIRE(U(CreateSolidBrush)); REQUIRE(U(PatBlt)); REQUIRE(U(SelectObject)); REQUIRE(U(DeleteObject));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 4, 4)) { t_skip("no dib"); return; }
    HBRUSH b = U(CreateSolidBrush)(PALETTERGB(9, 99, 199));
    HGDIOBJ old = U(SelectObject)(d.dc, b);
    CHECK(U(PatBlt)(d.dc, 0, 0, 4, 4, PATCOPY));
    EXPECT_EQ(d.bits[0], 199);
    EXPECT_EQ(d.bits[1], 99);
    EXPECT_EQ(d.bits[2], 9);
    U(SelectObject)(d.dc, old);
    U(DeleteObject)(b);
    dibdc_free(&d);
}

TEST(palette_solid_brush_8bpp_nearest) {
    REQUIRE(U(CreateSolidBrush)); REQUIRE(U(PatBlt)); REQUIRE(U(SelectObject)); REQUIRE(U(DeleteObject));
    struct dibdc d;
    if (!dibdc_create(&d, 8, 4, 4)) { t_skip("no dib"); return; }
    dib_clear_table(d.dc);
    COLORREF tab[] = {RGB(0, 0, 0), RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255), RGB(255, 255, 255)};
    dib_set_table(d.dc, tab, 5, 0);
    static const struct { COLORREF c; BYTE idx; } t[] = {
        {RGB(255, 0, 0), 1}, {RGB(0, 255, 0), 2}, {RGB(0, 0, 255), 3}, {RGB(255, 255, 255), 4},
        {RGB(240, 20, 10), 1}, {RGB(10, 250, 20), 2}, {RGB(20, 20, 250), 3}, {RGB(250, 250, 240), 4},
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        HBRUSH b = U(CreateSolidBrush)(t[i].c);
        HGDIOBJ old = U(SelectObject)(d.dc, b);
        U(PatBlt)(d.dc, 0, 0, 4, 4, PATCOPY);
        U(SelectObject)(d.dc, old);
        U(DeleteObject)(b);
        CHECKF(d.bits[0] == t[i].idx, "case %u got %u", i, d.bits[0]);
        CHECKF(d.bits[3 * d.stride + 3] == t[i].idx, "case %u corner", i);
    }
    dibdc_free(&d);
}

TEST(palette_nearest_color_vs_pixel_8bpp) {
    REQUIRE(U(GetNearestColor)); REQUIRE(U(SetPixel)); REQUIRE(U(GetPixel));
    struct dibdc d;
    if (!dibdc_create(&d, 8, 4, 4)) { t_skip("no dib"); return; }
    dib_clear_table(d.dc);
    COLORREF tab[] = {RGB(0, 0, 0), RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255), RGB(255, 255, 255)};
    dib_set_table(d.dc, tab, 5, 0);
    COLORREF q[] = {RGB(250, 4, 4), RGB(4, 250, 4), RGB(4, 4, 250), RGB(240, 240, 240)};
    for (unsigned i = 0; i < 4; i++) {
        COLORREF n = U(GetNearestColor)(d.dc, q[i]);
        COLORREF p = U(SetPixel)(d.dc, i, 0, q[i]);
        COLORREF g = U(GetPixel)(d.dc, i, 0);
        EXPECT_EQ(p, g);
        if (n != g) t_note("GetNearestColor=%06lx but pixel=%06lx", (unsigned long)n, (unsigned long)g);
    }
    dibdc_free(&d);
}

/* ---- ColorAdjustment ---- */
static void ca_fill_default(COLORADJUSTMENT *ca) {
    memset(ca, 0, sizeof *ca);
    ca->caSize = sizeof *ca;
    ca->caRedGamma = ca->caGreenGamma = ca->caBlueGamma = 10000;
    ca->caReferenceBlack = 0;
    ca->caReferenceWhite = 10000;
}

TEST(palette_coloradjust_struct_size) {
    CHECK(sizeof(COLORADJUSTMENT) == 24);
    CHECK(offsetof(COLORADJUSTMENT, caRedGamma) == 6);
    CHECK(offsetof(COLORADJUSTMENT, caReferenceBlack) == 12);
    CHECK(offsetof(COLORADJUSTMENT, caContrast) == 16);
}

TEST(palette_coloradjust_defaults) {
    REQUIRE(U(GetColorAdjustment));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 4, 4)) { t_skip("no dib"); return; }
    COLORADJUSTMENT ca;
    memset(&ca, 0xcc, sizeof ca);
    BOOL r = U(GetColorAdjustment)(d.dc, &ca);
    CHECK(r);
    if (r) {
        EXPECT_EQ(ca.caSize, 24);
        COLORADJUSTMENT def;
        ca_fill_default(&def);
        if (memcmp(&ca, &def, sizeof ca)) {
            t_note("default COLORADJUSTMENT: flags=%x illum=%u gamma=%u/%u/%u black=%u white=%u contrast=%d bright=%d color=%d tint=%d",
                   ca.caFlags, ca.caIlluminantIndex, ca.caRedGamma, ca.caGreenGamma, ca.caBlueGamma, ca.caReferenceBlack,
                   ca.caReferenceWhite, ca.caContrast, ca.caBrightness, ca.caColorfulness, ca.caRedGreenTint);
        }
        EXPECT_EQ(ca.caRedGamma, 10000);
        EXPECT_EQ(ca.caGreenGamma, 10000);
        EXPECT_EQ(ca.caBlueGamma, 10000);
        EXPECT_EQ(ca.caContrast, 0);
        EXPECT_EQ(ca.caBrightness, 0);
        EXPECT_EQ(ca.caColorfulness, 0);
        EXPECT_EQ(ca.caRedGreenTint, 0);
    }
    dibdc_free(&d);
}

TEST(palette_coloradjust_get_invalid_dc) {
    REQUIRE(U(GetColorAdjustment));
    COLORADJUSTMENT ca;
    CHECK(!U(GetColorAdjustment)(NULL, &ca));
    CHECK(!U(GetColorAdjustment)((HDC)PAL_BOGUS_HANDLE, &ca));
    HPALETTE p = pal_make(2);
    CHECK(!U(GetColorAdjustment)((HDC)p, &ca));
    U(DeleteObject)(p);
}

TEST(palette_coloradjust_get_null_buffer) {
    REQUIRE(U(GetColorAdjustment)); REQUIRE(U(CreateCompatibleDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    SetLastError(0);
    BOOL r = U(GetColorAdjustment)(dc, NULL);
    t_note("GetColorAdjustment(dc,NULL)=%d err=%lu", r, GetLastError());
    U(DeleteDC)(dc);
}

TEST(palette_coloradjust_set_invalid_dc) {
    REQUIRE(U(SetColorAdjustment));
    COLORADJUSTMENT ca;
    ca_fill_default(&ca);
    SetLastError(0);
    CHECK(!U(SetColorAdjustment)(NULL, &ca));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    CHECK(!U(SetColorAdjustment)((HDC)PAL_BOGUS_HANDLE, &ca));
    HPALETTE p = pal_make(2);
    CHECK(!U(SetColorAdjustment)((HDC)p, &ca));
    U(DeleteObject)(p);
}

TEST(palette_coloradjust_set_deleted_dc) {
    REQUIRE(U(SetColorAdjustment)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    U(DeleteDC)(dc);
    COLORADJUSTMENT ca;
    ca_fill_default(&ca);
    CHECK(!U(SetColorAdjustment)(dc, &ca));
}

TEST(palette_coloradjust_roundtrip) {
    REQUIRE(U(GetColorAdjustment)); REQUIRE(U(SetColorAdjustment));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 4, 4)) { t_skip("no dib"); return; }
    COLORADJUSTMENT orig, ca, got;
    CHECK(U(GetColorAdjustment)(d.dc, &orig));
    ca_fill_default(&ca);
    ca.caRedGamma = 12000; ca.caGreenGamma = 9000; ca.caBlueGamma = 20000;
    ca.caContrast = 10; ca.caBrightness = -20; ca.caColorfulness = 30; ca.caRedGreenTint = -40;
    ca.caReferenceBlack = 100; ca.caReferenceWhite = 9500;
    BOOL s = U(SetColorAdjustment)(d.dc, &ca);
    t_note("SetColorAdjustment(valid)=%d", s);
    if (s) {
        memset(&got, 0, sizeof got);
        CHECK(U(GetColorAdjustment)(d.dc, &got));
        CHECK(!memcmp(&got, &ca, sizeof ca));
    }
    U(SetColorAdjustment)(d.dc, &orig);
    dibdc_free(&d);
}

TEST(palette_coloradjust_independent_per_dc) {
    REQUIRE(U(GetColorAdjustment)); REQUIRE(U(SetColorAdjustment));
    struct dibdc a, b;
    if (!dibdc_create(&a, 32, 4, 4) || !dibdc_create(&b, 32, 4, 4)) { t_skip("no dib"); return; }
    COLORADJUSTMENT ca, gb;
    ca_fill_default(&ca);
    ca.caContrast = 25;
    if (U(SetColorAdjustment)(a.dc, &ca)) {
        CHECK(U(GetColorAdjustment)(b.dc, &gb));
        EXPECT_EQ(gb.caContrast, 0);
    }
    dibdc_free(&a); dibdc_free(&b);
}

TEST(palette_coloradjust_invalid_values_table) {
    REQUIRE(U(GetColorAdjustment)); REQUIRE(U(SetColorAdjustment));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 4, 4)) { t_skip("no dib"); return; }
    COLORADJUSTMENT orig, ca;
    U(GetColorAdjustment)(d.dc, &orig);
    struct { const char *what; void (*mod)(COLORADJUSTMENT *); } t[] = {
        {"caSize=0",    NULL}, {"gamma<2500", NULL}, {"gamma>65000", NULL}, {"black>4000", NULL},
        {"white<6000",  NULL}, {"contrast=101", NULL}, {"brightness=-101", NULL}, {"colorful=101", NULL},
        {"tint=-101", NULL}, {"illuminant=9", NULL}, {"caSize=48", NULL},
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        ca_fill_default(&ca);
        switch (i) {
        case 0: ca.caSize = 0; break;
        case 1: ca.caRedGamma = 2499; break;
        case 2: ca.caGreenGamma = 65001; break;
        case 3: ca.caReferenceBlack = 4001; break;
        case 4: ca.caReferenceWhite = 5999; break;
        case 5: ca.caContrast = 101; break;
        case 6: ca.caBrightness = -101; break;
        case 7: ca.caColorfulness = 101; break;
        case 8: ca.caRedGreenTint = -101; break;
        case 9: ca.caIlluminantIndex = 9; break;
        case 10: ca.caSize = 48; break;
        }
        SetLastError(0);
        BOOL r = U(SetColorAdjustment)(d.dc, &ca);
        t_note("SetColorAdjustment(%s)=%d err=%lu", t[i].what, r, GetLastError());
        if (r) U(SetColorAdjustment)(d.dc, &orig);
    }
    dibdc_free(&d);
}

TEST(palette_coloradjust_set_null_struct) {
    REQUIRE(U(SetColorAdjustment)); REQUIRE(U(CreateCompatibleDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    SetLastError(0);
    BOOL r = U(SetColorAdjustment)(dc, NULL);
    t_note("SetColorAdjustment(dc,NULL)=%d err=%lu", r, GetLastError());
    U(DeleteDC)(dc);
}

TEST(palette_coloradjust_halftone_flags) {
    REQUIRE(U(GetColorAdjustment)); REQUIRE(U(SetColorAdjustment));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 4, 4)) { t_skip("no dib"); return; }
    COLORADJUSTMENT orig, ca, got;
    U(GetColorAdjustment)(d.dc, &orig);
    static const WORD flags[] = {CA_NEGATIVE, CA_LOG_FILTER, CA_NEGATIVE | CA_LOG_FILTER};
    for (unsigned i = 0; i < 3; i++) {
        ca_fill_default(&ca);
        ca.caFlags = flags[i];
        if (U(SetColorAdjustment)(d.dc, &ca)) {
            memset(&got, 0, sizeof got);
            U(GetColorAdjustment)(d.dc, &got);
            EXPECT_EQ(got.caFlags, flags[i]);
        } else t_note("flags 0x%x rejected", flags[i]);
    }
    U(SetColorAdjustment)(d.dc, &orig);
    dibdc_free(&d);
}

/* ---- Gamma ---- */
TEST(palette_gamma_get_null_ramp) {
    REQUIRE(U(GetDeviceGammaRamp)); REQUIRE(U(CreateCompatibleDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    SetLastError(0);
    CHECK(!U(GetDeviceGammaRamp)(dc, NULL));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(!U(GetDeviceGammaRamp)(NULL, NULL));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    U(DeleteDC)(dc);
}

TEST(palette_gamma_set_null_ramp) {
    REQUIRE(U(SetDeviceGammaRamp)); REQUIRE(U(CreateCompatibleDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    SetLastError(0);
    CHECK(!U(SetDeviceGammaRamp)(dc, NULL));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(!U(SetDeviceGammaRamp)(NULL, NULL));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    U(DeleteDC)(dc);
}

TEST(palette_gamma_set_null_ramp_screen) {
    REQUIRE(U(SetDeviceGammaRamp));
    HDC dc = pal_screen_dc();
    if (!dc) { t_skip("no screen dc"); return; }
    SetLastError(0);
    CHECK(!U(SetDeviceGammaRamp)(dc, NULL));        /* never applies anything */
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    pal_release_screen_dc(dc);
}

TEST(palette_gamma_get_invalid_dc) {
    REQUIRE(U(GetDeviceGammaRamp));
    WORD ramp[3][256];
    memset(ramp, 0, sizeof ramp);
    CHECK(!U(GetDeviceGammaRamp)(NULL, ramp));
    CHECK(!U(GetDeviceGammaRamp)((HDC)PAL_BOGUS_HANDLE, ramp));
}

TEST(palette_gamma_get_memdc) {
    REQUIRE(U(GetDeviceGammaRamp)); REQUIRE(U(CreateCompatibleDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    WORD ramp[3][256];
    memset(ramp, 0, sizeof ramp);
    SetLastError(0);
    BOOL r = U(GetDeviceGammaRamp)(dc, ramp);
    t_note("GetDeviceGammaRamp(memdc)=%d err=%lu", r, GetLastError());
    U(DeleteDC)(dc);
}

TEST(palette_gamma_get_screen_monotonic) {
    REQUIRE(U(GetDeviceGammaRamp));
    HDC dc = pal_screen_dc();
    if (!dc) { t_skip("no screen dc"); return; }
    static WORD ramp[3][256];
    memset(ramp, 0, sizeof ramp);
    BOOL r = U(GetDeviceGammaRamp)(dc, ramp);
    if (!r) t_note("GetDeviceGammaRamp(screen) failed err=%lu", GetLastError());
    else {
        for (int c = 0; c < 3; c++) {
            int mono = 1;
            for (int i = 1; i < 256; i++) if (ramp[c][i] < ramp[c][i - 1]) mono = 0;
            if (!mono) t_note("gamma ramp channel %d not monotonic", c);
        }
        t_note("gamma ramp ends: r %u..%u g %u..%u b %u..%u", ramp[0][0], ramp[0][255], ramp[1][0], ramp[1][255], ramp[2][0], ramp[2][255]);
    }
    pal_release_screen_dc(dc);
}

TEST(palette_gamma_get_repeatable) {
    REQUIRE(U(GetDeviceGammaRamp));
    HDC dc = pal_screen_dc();
    if (!dc) { t_skip("no screen dc"); return; }
    static WORD a[3][256], b[3][256];
    BOOL r1 = U(GetDeviceGammaRamp)(dc, a), r2 = U(GetDeviceGammaRamp)(dc, b);
    EXPECT_EQ(r1, r2);
    if (r1 && r2) CHECK(!memcmp(a, b, sizeof a));
    pal_release_screen_dc(dc);
}

/* ---- Gamut / color matching ---- */
TEST(palette_gamut_null_args) {
    if (pal_is_wine()) { t_skip("unimplemented under wine"); return; }
    REQUIRE(U(CheckColorsInGamut)); REQUIRE(U(CreateCompatibleDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    RGBTRIPLE rgb[2] = {{1, 2, 3}, {4, 5, 6}};
    BYTE out[2];
    SetLastError(0);
    CHECK(!U(CheckColorsInGamut)(dc, NULL, out, 2));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(!U(CheckColorsInGamut)(dc, rgb, NULL, 2));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(!U(CheckColorsInGamut)(dc, rgb, out, 0));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(!U(CheckColorsInGamut)(NULL, NULL, NULL, 0));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    U(DeleteDC)(dc);
}

TEST(palette_gamut_invalid_dc) {
    if (pal_is_wine()) { t_skip("unimplemented under wine"); return; }
    REQUIRE(U(CheckColorsInGamut));
    RGBTRIPLE rgb[2] = {{1, 2, 3}, {4, 5, 6}};
    BYTE out[2] = {0xaa, 0xaa};
    CHECK(!U(CheckColorsInGamut)(NULL, rgb, out, 2));
    CHECK(!U(CheckColorsInGamut)((HDC)PAL_BOGUS_HANDLE, rgb, out, 2));
}

TEST(palette_gamut_valid_memdc) {
    if (pal_is_wine()) { t_skip("unimplemented under wine"); return; }
    REQUIRE(U(CheckColorsInGamut));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 4, 4)) { t_skip("no dib"); return; }
    RGBTRIPLE rgb[3] = {{0, 0, 0}, {128, 128, 128}, {255, 255, 255}};
    BYTE out[8];
    memset(out, 0xaa, sizeof out);
    SetLastError(0);
    BOOL r = U(CheckColorsInGamut)(d.dc, rgb, out, 3);
    t_note("CheckColorsInGamut(memdc)=%d err=%lu out=%02x %02x %02x", r, GetLastError(), out[0], out[1], out[2]);
    dibdc_free(&d);
}

TEST(palette_colormatch_invalid) {
    if (pal_is_wine()) { t_skip("unimplemented under wine"); return; }
    REQUIRE(U(ColorMatchToTarget)); REQUIRE(U(CreateCompatibleDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    SetLastError(0);
    CHECK(!U(ColorMatchToTarget)(dc, NULL, 1));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(!U(ColorMatchToTarget)(NULL, NULL, 1));
    CHECK(!U(ColorMatchToTarget)(dc, (HDC)PAL_BOGUS_HANDLE, 1));
    CHECK(!U(ColorMatchToTarget)((HDC)PAL_BOGUS_HANDLE, dc, 1));
    HPALETTE p = pal_make(2);
    CHECK(!U(ColorMatchToTarget)(dc, (HDC)p, 1));
    U(DeleteObject)(p);
    U(DeleteDC)(dc);
}

TEST(palette_colormatch_icm_off) {
    if (pal_is_wine()) { t_skip("unimplemented under wine"); return; }
    REQUIRE(U(ColorMatchToTarget)); REQUIRE(U(CreateCompatibleDC));
    HDC a = U(CreateCompatibleDC)(NULL), b = U(CreateCompatibleDC)(NULL);
    static const DWORD actions[] = {CS_ENABLE, CS_DISABLE, CS_DELETE_TRANSFORM, 0, 99};
    for (unsigned i = 0; i < sizeof actions / sizeof *actions; i++) {
        SetLastError(0);
        BOOL r = U(ColorMatchToTarget)(a, b, actions[i]);
        CHECK(!r);
        t_note("ColorMatchToTarget(action=%lu)=%d err=%lu", (unsigned long)actions[i], r, GetLastError());
    }
    U(DeleteDC)(a); U(DeleteDC)(b);
}

TEST(palette_correctpalette_invalid_args) {
    if (pal_is_wine()) { t_skip("unimplemented under wine"); return; }
    REQUIRE(U(ColorCorrectPalette)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HPALETTE p = pal_make(16);
    struct { HDC dc; HPALETTE p; DWORD first, num; const char *w; } t[] = {
        {NULL, p, 0, 4, "null dc"}, {dc, NULL, 0, 4, "null pal"}, {dc, p, 0, 0, "num 0"},
        {dc, p, 0, 0x10001, "num>65536"}, {dc, p, 0x10000, 1, "first 0x10000"}, {dc, p, 0xffff, 2, "first+num>0x10000"},
        {dc, p, 1, 0x10000, "first+num>0x10000 b"}, {dc, p, 0xffffffff, 1, "first huge"}, {dc, p, 0, 0xffffffff, "num huge"},
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        SetLastError(0);
        BOOL r = U(ColorCorrectPalette)(t[i].dc, t[i].p, t[i].first, t[i].num);
        CHECKF(!r, "%s", t[i].w);
        DWORD e = GetLastError();
        CHECKF(e == ERROR_INVALID_PARAMETER, "%s: err %lu", t[i].w, (unsigned long)e);
    }
    U(DeleteObject)(p);
    U(DeleteDC)(dc);
}

TEST(palette_correctpalette_bad_dc) {
    if (pal_is_wine()) { t_skip("unimplemented under wine"); return; }
    REQUIRE(U(ColorCorrectPalette));
    HPALETTE p = pal_make(16);
    SetLastError(0);
    CHECK(!U(ColorCorrectPalette)((HDC)PAL_BOGUS_HANDLE, p, 0, 16));
    t_note("ColorCorrectPalette(bogus dc) err=%lu", GetLastError());
    CHECK(!U(ColorCorrectPalette)((HDC)p, p, 0, 16));
    U(DeleteObject)(p);
}

TEST(palette_correctpalette_memdc_no_icm) {
    if (pal_is_wine()) { t_skip("unimplemented under wine"); return; }
    REQUIRE(U(ColorCorrectPalette));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 4, 4)) { t_skip("no dib"); return; }
    HPALETTE p = pal_make(16);
    SetLastError(0);
    BOOL r = U(ColorCorrectPalette)(d.dc, p, 0, 16);
    t_note("ColorCorrectPalette(memdc, 16 entries)=%d err=%lu", r, GetLastError());
    U(DeleteObject)(p);
    dibdc_free(&d);
}

TEST(palette_correctpalette_stock_default) {
    if (pal_is_wine()) { t_skip("unimplemented under wine"); return; }
    REQUIRE(U(ColorCorrectPalette)); REQUIRE(U(GetStockObject));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 4, 4)) { t_skip("no dib"); return; }
    SetLastError(0);
    BOOL r = U(ColorCorrectPalette)(d.dc, (HPALETTE)U(GetStockObject)(DEFAULT_PALETTE), 0, 20);
    t_note("ColorCorrectPalette(default palette)=%d err=%lu", r, GetLastError());
    dibdc_free(&d);
}

/* ---- A/W parity of gdi32 vs gdi32full resolution ---- */
TEST(palette_exports_dual_resolution) {
    static const char *const names[] = {
        "CreatePalette", "CreateHalftonePalette", "GetPaletteEntries", "SetPaletteEntries", "ResizePalette",
        "AnimatePalette", "SelectPalette", "RealizePalette", "GetNearestPaletteIndex", "GetNearestColor",
        "GetSystemPaletteEntries", "GetSystemPaletteUse", "SetSystemPaletteUse", "UpdateColors",
        "GetColorAdjustment", "SetColorAdjustment", "GetDeviceGammaRamp", "SetDeviceGammaRamp",
        "CheckColorsInGamut", "ColorMatchToTarget", "ColorCorrectPalette", "UnrealizeObject",
    };
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++) {
        FARPROC g = t_proc_gdi32(names[i]);
        CHECKF(g != NULL, "gdi32 export %s", names[i]);
        FARPROC f = t_proc_gdi32full(names[i]);
        /* On Win10 most are forwarded: both resolve; either way the call result must agree */
        if (!f) t_note("%s not exported by gdi32full", names[i]);
    }
}

TEST(palette_dual_entry_same_behaviour) {
    REQUIRE(UG(GetPaletteEntries)); REQUIRE(UG(CreatePalette)); REQUIRE(UG(DeleteObject));
    HPALETTE p = UG(CreatePalette) ? pal_make(10) : NULL;
    EXPECT_EQ(UG(GetPaletteEntries)(p, 0, 0, NULL), 10);
    if (UF(GetPaletteEntries)) EXPECT_EQ(UF(GetPaletteEntries)(p, 0, 0, NULL), 10);
    if (UF(CreatePalette)) {
        LOGPALETTE lp = {0x300, 1, {{1, 2, 3, 0}}};
        HPALETTE q = UF(CreatePalette)(&lp);
        CHECK(q != NULL);
        EXPECT_EQ(UG(GetPaletteEntries)(q, 0, 0, NULL), 1);
        UG(DeleteObject)(q);
    }
    UG(DeleteObject)(p);
}
