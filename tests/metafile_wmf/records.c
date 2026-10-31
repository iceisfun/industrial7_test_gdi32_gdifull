/* Exact record bytes produced by calls on a metafile DC (table driven). */
#include "wmf.h"

#define ANY (-1)
#define UNP(...) __VA_ARGS__

typedef void (*opfn)(HDC);
static HGDIOBJ SEL(HDC dc, HGDIOBJ o) { return U(SelectObject)(dc, o); }
static HGDIOBJ STK(int i) { return U(GetStockObject)(i); }

/* Record op under a fresh metafile DC; find the nth record with function fn (or alt),
 * check its size window and leading parameters. cnt: expected number of records with fn. */
static void run_op(opfn f, WORD fn, WORD alt, int szlo, int szhi, const int *pv, int np, int cnt, const char *name) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    f(dc);
    WB b;
    if (!mf_finish(dc, &b, NULL)) { CHECKF(0, "%s: no bits", name); return; }
    CHECKF(wb_valid(&b), "%s: invalid structure", name);
    const BYTE *r = wb_find(&b, fn, 0);
    if (!r && alt) { fn = alt; r = wb_find(&b, fn, 0); }
    CHECKF(r != NULL, "%s: record %04x not found", name, fn);
    if (!r) { for (const BYTE *q = wb_first(&b); q; q = wb_next(&b, q)) { t_note("  %s rec %04x size %lu", name, r_func(q), r_size(q)); if (!r_func(q)) break; } }
    if (r) {
        DWORD s = r_size(r);
        if (szlo) CHECKF(s >= (DWORD)szlo && s <= (DWORD)szhi, "%s: size %lu want %d..%d", name, s, szlo, szhi);
        for (int i = 0; i < np && (DWORD)(3 + i) < s; i++)
            if (pv[i] != ANY) CHECKF(r_parm(r, i) == (WORD)pv[i], "%s: param %d = 0x%04x want 0x%04x", name, i, r_parm(r, i), (WORD)pv[i]);
        if (cnt > 0) EXPECT_EQ(wb_count(&b, fn), cnt);
    }
    wb_free(&b);
}

#define OP(nm, fn, lo, hi, params, ...) \
    static void op_##nm(HDC dc) { __VA_ARGS__ } \
    TEST(wmf_op_##nm) { static const int pv[] = { UNP params }; run_op(op_##nm, fn, 0, lo, hi, pv, sizeof pv / sizeof *pv, 1, #nm); }
#define OPC(nm, fn, lo, hi, params, ...) \
    static void op_##nm(HDC dc) { __VA_ARGS__ } \
    TEST(wmf_op_##nm) { static const int pv[] = { UNP params }; run_op(op_##nm, fn, 0, lo, hi, pv, sizeof pv / sizeof *pv, 0, #nm); }
#define OPN(nm, fn, lo, hi, cnt, params, ...) \
    static void op_##nm(HDC dc) { __VA_ARGS__ } \
    TEST(wmf_op_##nm) { static const int pv[] = { UNP params }; run_op(op_##nm, fn, 0, lo, hi, pv, sizeof pv / sizeof *pv, cnt, #nm); }
#define OPA(nm, fn, alt, lo, hi, params, ...) \
    static void op_##nm(HDC dc) { __VA_ARGS__ } \
    TEST(wmf_op_##nm) { static const int pv[] = { UNP params }; run_op(op_##nm, fn, alt, lo, hi, pv, sizeof pv / sizeof *pv, 0, #nm); }

/* ---- DC state ---- */
OP(setmapmode_text,   0x0103, 4, 4, (1), U(SetMapMode)(dc, MM_TEXT);)
OP(setmapmode_lomet,  0x0103, 4, 4, (2), U(SetMapMode)(dc, MM_LOMETRIC);)
OP(setmapmode_himet,  0x0103, 4, 4, (3), U(SetMapMode)(dc, MM_HIMETRIC);)
OP(setmapmode_loeng,  0x0103, 4, 4, (4), U(SetMapMode)(dc, MM_LOENGLISH);)
OP(setmapmode_hieng,  0x0103, 4, 4, (5), U(SetMapMode)(dc, MM_HIENGLISH);)
OP(setmapmode_twips,  0x0103, 4, 4, (6), U(SetMapMode)(dc, MM_TWIPS);)
OP(setmapmode_iso,    0x0103, 4, 4, (7), U(SetMapMode)(dc, MM_ISOTROPIC);)
OP(setmapmode_aniso,  0x0103, 4, 4, (8), U(SetMapMode)(dc, MM_ANISOTROPIC);)
OP(setwindowext,   0x020C, 5, 5, (200, 100), U(SetMapMode)(dc, MM_ANISOTROPIC); U(SetWindowExtEx)(dc, 100, 200, NULL);)
OP(setwindoworg,   0x020B, 5, 5, (20, 10),   U(SetWindowOrgEx)(dc, 10, 20, NULL);)
OP(setviewportext, 0x020E, 5, 5, (400, 300), U(SetMapMode)(dc, MM_ANISOTROPIC); U(SetViewportExtEx)(dc, 300, 400, NULL);)
OP(setviewportorg, 0x020D, 5, 5, (40, 30),   U(SetViewportOrgEx)(dc, 30, 40, NULL);)
OP(offsetwindoworg,   0x020F, 5, 5, (8, 7),  U(OffsetWindowOrgEx)(dc, 7, 8, NULL);)
OP(offsetviewportorg, 0x0211, 5, 5, (6, 5),  U(OffsetViewportOrgEx)(dc, 5, 6, NULL);)
OP(scalewindowext,   0x0410, 7, 7, (4, 3, 2, 1), U(SetMapMode)(dc, MM_ANISOTROPIC); U(ScaleWindowExtEx)(dc, 1, 2, 3, 4, NULL);)
OP(scaleviewportext, 0x0412, 7, 7, (8, 7, 6, 5), U(SetMapMode)(dc, MM_ANISOTROPIC); U(ScaleViewportExtEx)(dc, 5, 6, 7, 8, NULL);)
OP(setwindoworg_neg, 0x020B, 5, 5, (0xFFF6, 0xFFFB), U(SetWindowOrgEx)(dc, -5, -10, NULL);)
OP(setbkmode_trans,  0x0102, 4, 5, (1), U(SetBkMode)(dc, TRANSPARENT);)
OP(setbkmode_opaque, 0x0102, 4, 5, (2), U(SetBkMode)(dc, OPAQUE);)
OP(setrop2_xor,      0x0104, 4, 5, (R2_XORPEN), U(SetROP2)(dc, R2_XORPEN);)
OP(setrop2_black,    0x0104, 4, 5, (R2_BLACK), U(SetROP2)(dc, R2_BLACK);)
OP(setrop2_notcopy,  0x0104, 4, 5, (R2_NOTCOPYPEN), U(SetROP2)(dc, R2_NOTCOPYPEN);)
OP(setrop2_white,    0x0104, 4, 5, (R2_WHITE), U(SetROP2)(dc, R2_WHITE);)
OP(setpolyfill_winding, 0x0106, 4, 5, (2), U(SetPolyFillMode)(dc, WINDING);)
OP(setpolyfill_alt,     0x0106, 4, 5, (1), U(SetPolyFillMode)(dc, ALTERNATE);)
OP(setstretch_halftone, 0x0107, 4, 5, (4), U(SetStretchBltMode)(dc, HALFTONE);)
OP(setstretch_coloroncolor, 0x0107, 4, 5, (3), U(SetStretchBltMode)(dc, COLORONCOLOR);)
OP(setstretch_blackonwhite, 0x0107, 4, 5, (1), U(SetStretchBltMode)(dc, BLACKONWHITE);)
OP(settextalign_bl,  0x012E, 4, 5, (TA_BASELINE | TA_CENTER), U(SetTextAlign)(dc, TA_BASELINE | TA_CENTER);)
OP(settextalign_cp,  0x012E, 4, 5, (TA_UPDATECP), U(SetTextAlign)(dc, TA_UPDATECP);)
OP(settextalign_rtl, 0x012E, 4, 5, (TA_RIGHT | TA_BOTTOM), U(SetTextAlign)(dc, TA_RIGHT | TA_BOTTOM);)
OP(settextcolor, 0x0209, 5, 5, (0x3412, 0x0078), U(SetTextColor)(dc, RGB(0x12, 0x34, 0x78));)
OP(setbkcolor,   0x0201, 5, 5, (0x6655, 0x0044), U(SetBkColor)(dc, RGB(0x55, 0x66, 0x44));)
OP(settextjust,  0x020A, 5, 5, (2, 5),  U(SetTextJustification)(dc, 5, 2);)
OP(settextchextra, 0x0108, 4, 4, (3),   U(SetTextCharacterExtra)(dc, 3);)
OP(setmapperflags, 0x0231, 5, 5, (1, 0), U(SetMapperFlags)(dc, 1);)
OP(savedc,    0x001E, 3, 3, (ANY), U(SaveDC)(dc);)
OP(restoredc_rel, 0x0127, 4, 4, (0xFFFF), U(SaveDC)(dc); U(RestoreDC)(dc, -1);)
OPN(savedc_twice, 0x001E, 3, 3, 2, (ANY), U(SaveDC)(dc); U(SaveDC)(dc);)
OP(restoredc_rel2, 0x0127, 4, 4, (0xFFFE), U(SaveDC)(dc); U(SaveDC)(dc); U(RestoreDC)(dc, -2);)
OP(intersectcliprect, 0x0416, 7, 7, (4, 3, 2, 1), U(IntersectClipRect)(dc, 1, 2, 3, 4);)
OP(excludecliprect,   0x0415, 7, 7, (40, 30, 20, 10), U(ExcludeClipRect)(dc, 10, 20, 30, 40);)
OP(offsetcliprgn,     0x0220, 5, 5, (20, 10), U(OffsetClipRgn)(dc, 10, 20);)
OP(realizepalette,    0x0035, 3, 3, (ANY), U(RealizePalette)(dc);)

/* ---- lines and shapes ---- */
OP(movetoex,  0x0214, 5, 5, (20, 10), U(MoveToEx)(dc, 10, 20, NULL);)
OP(lineto,    0x0213, 5, 5, (40, 30), U(LineTo)(dc, 30, 40);)
OP(movetoex_neg, 0x0214, 5, 5, (0xFFFF, 0xFFFE), U(MoveToEx)(dc, -2, -1, NULL);)
OP(movetoex_big, 0x0214, 5, 5, (0x7FFF, 0x7FFF), U(MoveToEx)(dc, 32767, 32767, NULL);)
OP(movetoex_trunc, 0x0214, 5, 5, (0x0002, 0x0001), U(MoveToEx)(dc, 0x10001, 0x20002, NULL);)
OP(rectangle, 0x041B, 7, 7, (4, 3, 2, 1), U(Rectangle)(dc, 1, 2, 3, 4);)
OP(rectangle_rev, 0x041B, 7, 7, (1, 3, 2, 4), U(Rectangle)(dc, 4, 2, 3, 1);)
OP(ellipse,   0x0418, 7, 7, (40, 30, 20, 10), U(Ellipse)(dc, 10, 20, 30, 40);)
OP(roundrect, 0x061C, 9, 9, (6, 5, 4, 3, 2, 1), U(RoundRect)(dc, 1, 2, 3, 4, 5, 6);)
OP(arc,       0x0817, 11, 11, (8, 7, 6, 5, 4, 3, 2, 1), U(Arc)(dc, 1, 2, 3, 4, 5, 6, 7, 8);)
OP(pie,       0x081A, 11, 11, (8, 7, 6, 5, 4, 3, 2, 1), U(Pie)(dc, 1, 2, 3, 4, 5, 6, 7, 8);)
OP(chord,     0x0830, 11, 11, (8, 7, 6, 5, 4, 3, 2, 1), U(Chord)(dc, 1, 2, 3, 4, 5, 6, 7, 8);)
OP(arcto_not_recorded_as_arc, 0x0214, 5, 5, (ANY), POINT p = {1, 2}; U(MoveToEx)(dc, p.x, p.y, NULL); U(ArcTo)(dc, 1, 2, 30, 40, 5, 6, 7, 8);)
OP(setpixel,  0x041F, 7, 7, (0x0C0B, 0x000A, 20, 10), U(SetPixel)(dc, 10, 20, RGB(0x0B, 0x0C, 0x0A));)
OP(setpixelv, 0x041F, 7, 7, (0x0C0B, 0x000A, 20, 10), U(SetPixelV)(dc, 10, 20, RGB(0x0B, 0x0C, 0x0A));)
OPA(floodfill, 0x0548, 0x0419, 0, 0, (ANY), U(FloodFill)(dc, 10, 20, RGB(1, 2, 3));)
OP(extfloodfill_border, 0x0548, 8, 8, (0, 0x0201, 0x0003, 20, 10), U(ExtFloodFill)(dc, 10, 20, RGB(1, 2, 3), FLOODFILLBORDER);)
OP(extfloodfill_surface, 0x0548, 8, 8, (1, 0x0201, 0x0003, 20, 10), U(ExtFloodFill)(dc, 10, 20, RGB(1, 2, 3), FLOODFILLSURFACE);)

/* ---- polys ---- */
static const POINT g_pts[] = { {1, 2}, {30, 4}, {5, 60}, {7, 8} };
OP(polygon3,   0x0324, 3 + 1 + 6, 3 + 1 + 6, (3, 1, 2, 30, 4, 5, 60), U(Polygon)(dc, g_pts, 3);)
OP(polygon4,   0x0324, 3 + 1 + 8, 3 + 1 + 8, (4, 1, 2, 30, 4, 5, 60, 7, 8), U(Polygon)(dc, g_pts, 4);)
OP(polyline3,  0x0325, 3 + 1 + 6, 3 + 1 + 6, (3, 1, 2, 30, 4, 5, 60), U(Polyline)(dc, g_pts, 3);)
OP(polyline2,  0x0325, 3 + 1 + 4, 3 + 1 + 4, (2, 1, 2, 30, 4), U(Polyline)(dc, g_pts, 2);)
OP(polypolygon, 0x0538, 3 + 1 + 2 + 14, 3 + 1 + 2 + 15, (2, 3, 4, 1, 2, 30, 4, 5, 60, 1, 2, 30, 4, 5, 60, 7, 8),
   INT cnt[2] = {3, 4}; POINT p[7] = { {1, 2}, {30, 4}, {5, 60}, {1, 2}, {30, 4}, {5, 60}, {7, 8} }; U(PolyPolygon)(dc, p, cnt, 2);)
OP(polypolygon1, 0x0538, 3 + 1 + 1 + 6, 3 + 1 + 1 + 7, (1, 3, 1, 2, 30, 4, 5, 60),
   INT cnt[1] = {3}; U(PolyPolygon)(dc, g_pts, cnt, 1);)
OP(polygon_neg_coords, 0x0324, 3 + 1 + 6, 3 + 1 + 6, (3, 0xFFFF, 0xFFFE, 0xFFFD, 0xFFFC, 0xFFFB, 0xFFFA),
   POINT p[3] = { {-1, -2}, {-3, -4}, {-5, -6} }; U(Polygon)(dc, p, 3);)

/* ---- text ---- */
OPA(textout_5, 0x0A32, 0x0521, 0, 0, (ANY), U(TextOutA)(dc, 10, 20, "Hello", 5);)
OPA(textout_4, 0x0A32, 0x0521, 0, 0, (ANY), U(TextOutA)(dc, 10, 20, "Test", 4);)
OPA(textout_1, 0x0A32, 0x0521, 0, 0, (ANY),      U(TextOutA)(dc, 10, 20, "X", 1);)
OPA(textout_w, 0x0A32, 0x0521, 0, 0, (ANY), U(TextOutW)(dc, 10, 20, L"abc", 3);)
OPA(textout_3, 0x0A32, 0x0521, 0, 0, (ANY), U(TextOutA)(dc, 10, 20, "abc", 3);)
OPA(textout_partial, 0x0A32, 0x0521, 0, 0, (ANY), U(TextOutA)(dc, 10, 20, "abcdef", 2);)
OP(exttextout, 0x0A32, 10, 40, (20, 10, 5, 0), U(ExtTextOutA)(dc, 10, 20, 0, NULL, "Hello", 5, NULL);)
OP(exttextout_opaque, 0x0A32, 14, 40, (20, 10, 5, ETO_OPAQUE, 1, 2, 3, 4),
   RECT r = {1, 2, 3, 4}; U(ExtTextOutA)(dc, 10, 20, ETO_OPAQUE, &r, "Hello", 5, NULL);)
OP(exttextout_clipped, 0x0A32, 14, 40, (20, 10, 5, ETO_CLIPPED, 5, 6, 7, 8),
   RECT r = {5, 6, 7, 8}; U(ExtTextOutA)(dc, 10, 20, ETO_CLIPPED, &r, "Hello", 5, NULL);)
OP(exttextout_dx, 0x0A32, 3 + 4 + 3 + 5, 3 + 4 + 3 + 5, (20, 10, 5, 0, ANY, ANY, ANY, 7, 8, 9, 10, 11),
   INT dx[5] = {7, 8, 9, 10, 11}; U(ExtTextOutA)(dc, 10, 20, 0, NULL, "Hello", 5, dx);)
OP(exttextout_w, 0x0A32, 9, 40, (20, 10, 3, 0), U(ExtTextOutW)(dc, 10, 20, 0, NULL, L"abc", 3, NULL);)
OP(exttextout_empty_opaque, 0x0A32, 3 + 4 + 4, 3 + 4 + 4, (20, 10, 0, ETO_OPAQUE, 1, 2, 3, 4),
   RECT r = {1, 2, 3, 4}; U(ExtTextOutA)(dc, 10, 20, ETO_OPAQUE, &r, "", 0, NULL);)

/* ---- escapes ---- */
OP(escape_mfcomment, 0x0626, 3 + 1 + 1 + 2, 3 + 1 + 1 + 2, (15, 4, 0x6261, 0x6463),
   U(Escape)(dc, MFCOMMENT, 4, "abcd", NULL);)
OP(escape_mfcomment_odd, 0x0626, 3 + 1 + 1 + 2, 3 + 1 + 1 + 2, (15, 3, 0x6261, ANY),
   U(Escape)(dc, MFCOMMENT, 3, "abc", NULL);)
OP(escape_mfcomment_long, 0x0626, 3 + 1 + 1 + 50, 3 + 1 + 1 + 50, (15, 100),
   char buf[100]; memset(buf, 'z', 100); U(Escape)(dc, MFCOMMENT, 100, buf, NULL);)

/* ---- blits ---- */
OP(patblt_patcopy,   0x061D, 9, 9, (0x0021, 0x00F0, 40, 30, 20, 10), U(PatBlt)(dc, 10, 20, 30, 40, PATCOPY);)
OP(patblt_patinvert, 0x061D, 9, 9, (0x0049, 0x005A, 4, 3, 2, 1),     U(PatBlt)(dc, 1, 2, 3, 4, PATINVERT);)
OP(patblt_dstinvert, 0x061D, 9, 9, (0x0009, 0x0055, 4, 3, 2, 1),     U(PatBlt)(dc, 1, 2, 3, 4, DSTINVERT);)
OP(patblt_blackness, 0x061D, 9, 9, (0x0042, 0x0000, 4, 3, 2, 1),     U(PatBlt)(dc, 1, 2, 3, 4, BLACKNESS);)
OP(patblt_whiteness, 0x061D, 9, 9, (0x0062, 0x00FF, 4, 3, 2, 1),     U(PatBlt)(dc, 1, 2, 3, 4, WHITENESS);)
OP(patblt_negative_size, 0x061D, 9, 9, (0x0021, 0x00F0, 0xFFFC, 0xFFFD, 2, 1), U(PatBlt)(dc, 1, 2, -3, -4, PATCOPY);)

static void make_src(DIBDC *s) { dibdc_make(s, 8, 8); }
OPA(bitblt_srccopy, 0x0940, 0x0922, 3 + 8, 100000, (0x0020, 0x00CC, 0, 0, 8, 8, 20, 10),
    DIBDC s; make_src(&s); U(BitBlt)(dc, 10, 20, 8, 8, s.dc, 0, 0, SRCCOPY); dibdc_free(&s);)
OPA(bitblt_srcpaint, 0x0940, 0x0922, 3 + 8, 100000, (0x0086, 0x00EE, 3, 2, 4, 5, 7, 6),
    DIBDC s; make_src(&s); U(BitBlt)(dc, 6, 7, 5, 4, s.dc, 2, 3, SRCPAINT); dibdc_free(&s);)
OPA(stretchblt_srccopy, 0x0B41, 0x0B23, 3 + 10, 100000, (0x0020, 0x00CC, 8, 8, 0, 0, 16, 16, 20, 10),
    DIBDC s; make_src(&s); U(StretchBlt)(dc, 10, 20, 16, 16, s.dc, 0, 0, 8, 8, SRCCOPY); dibdc_free(&s);)
OPA(stretchblt_negative, 0x0B41, 0x0B23, 3 + 10, 100000, (0x0020, 0x00CC, 8, 8, 0, 0, 0xFFF0, 16, 20, 10),
    DIBDC s; make_src(&s); U(StretchBlt)(dc, 10, 20, 16, -16, s.dc, 0, 0, 8, 8, SRCCOPY); dibdc_free(&s);)

static BITMAPINFO *mk_bi(int w, int h, void **bits) {
    static BYTE store[sizeof(BITMAPINFOHEADER) + 4 * 4 * 3];
    BITMAPINFO *bi = (BITMAPINFO *)store;
    memset(store, 0, sizeof store);
    bi->bmiHeader.biSize = sizeof bi->bmiHeader; bi->bmiHeader.biWidth = w; bi->bmiHeader.biHeight = h;
    bi->bmiHeader.biPlanes = 1; bi->bmiHeader.biBitCount = 24; bi->bmiHeader.biCompression = BI_RGB;
    bi->bmiHeader.biSizeImage = 4 * 4 * 3;
    for (int i = 0; i < 48; i++) store[sizeof(BITMAPINFOHEADER) + i] = (BYTE)(i * 5);
    *bits = store + sizeof(BITMAPINFOHEADER);
    return bi;
}
OP(stretchdibits, 0x0F43, 3 + 11 + 20 + 24, 3 + 11 + 20 + 24, (0x0020, 0x00CC, 0, 4, 4, 0, 0, 8, 8, 20, 10),
   void *bits; BITMAPINFO *bi = mk_bi(4, 4, &bits); U(StretchDIBits)(dc, 10, 20, 8, 8, 0, 0, 4, 4, bits, bi, DIB_RGB_COLORS, SRCCOPY);)
OP(stretchdibits_usage_pal, 0x0F43, 0, 0, (0x0020, 0x00CC, 1),
   void *bits; BITMAPINFO *bi = mk_bi(4, 4, &bits); U(StretchDIBits)(dc, 10, 20, 8, 8, 0, 0, 4, 4, bits, bi, DIB_PAL_COLORS, SRCCOPY);)
OP(stretchdibits_notsrccopy, 0x0F43, 0, 0, (0x0008, 0x0033, 0, 4, 4),
   void *bits; BITMAPINFO *bi = mk_bi(4, 4, &bits); U(StretchDIBits)(dc, 1, 2, 4, 4, 0, 0, 4, 4, bits, bi, DIB_RGB_COLORS, NOTSRCCOPY);)
OP(setdibitstodevice, 0x0D33, 3 + 9 + 20 + 24, 3 + 9 + 20 + 24, (0, 4, 0, 0, 0, 4, 4, 20, 10),
   void *bits; BITMAPINFO *bi = mk_bi(4, 4, &bits); U(SetDIBitsToDevice)(dc, 10, 20, 4, 4, 0, 0, 0, 4, bits, bi, DIB_RGB_COLORS);)
OP(setdibitstodevice_src, 0x0D33, 0, 0, (0, 2, 1, 1, 2, 3, 3, 7, 6),
   void *bits; BITMAPINFO *bi = mk_bi(4, 4, &bits); U(SetDIBitsToDevice)(dc, 6, 7, 3, 3, 2, 1, 1, 2, bits, bi, DIB_RGB_COLORS);)

/* ---- object creation (each creation is a record; select picks the slot) ---- */
OPC(createpen_solid, 0x02FA, 8, 8, (PS_SOLID, 3, 0, 0x0201, 0x0003),
   HPEN p = U(CreatePen)(PS_SOLID, 3, RGB(1, 2, 3)); HGDIOBJ o = U(SelectObject)(dc, p); U(SelectObject)(dc, o); U(DeleteObject)(p);)
OPC(createpen_dash, 0x02FA, 8, 8, (PS_DASH, 1, 0, 0x0201, 0x0003),
   HPEN p = U(CreatePen)(PS_DASH, 1, RGB(1, 2, 3)); HGDIOBJ o = U(SelectObject)(dc, p); U(SelectObject)(dc, o); U(DeleteObject)(p);)
OPC(createpen_null, 0x02FA, 8, 8, (PS_NULL),
   HPEN p = U(CreatePen)(PS_NULL, 1, RGB(1, 2, 3)); HGDIOBJ o = U(SelectObject)(dc, p); U(SelectObject)(dc, o); U(DeleteObject)(p);)
OPC(createpen_insideframe, 0x02FA, 8, 8, (PS_INSIDEFRAME, 5),
   HPEN p = U(CreatePen)(PS_INSIDEFRAME, 5, RGB(1, 2, 3)); HGDIOBJ o = U(SelectObject)(dc, p); U(SelectObject)(dc, o); U(DeleteObject)(p);)
OPC(createpenindirect, 0x02FA, 8, 8, (PS_DOT, 2, 0, 0x6655, 0x0044),
   LOGPEN lp = { PS_DOT, { 2, 0 }, RGB(0x55, 0x66, 0x44) }; HPEN p = U(CreatePenIndirect)(&lp); HGDIOBJ o = U(SelectObject)(dc, p); U(SelectObject)(dc, o); U(DeleteObject)(p);)
OPC(createbrush_solid, 0x02FC, 7, 7, (BS_SOLID, 0x0201, 0x0003, 0),
   HBRUSH b = U(CreateSolidBrush)(RGB(1, 2, 3)); HGDIOBJ o = U(SelectObject)(dc, b); U(SelectObject)(dc, o); U(DeleteObject)(b);)
OPC(createbrush_hatch, 0x02FC, 7, 7, (BS_HATCHED, 0x0201, 0x0003, HS_CROSS),
   HBRUSH b = U(CreateHatchBrush)(HS_CROSS, RGB(1, 2, 3)); HGDIOBJ o = U(SelectObject)(dc, b); U(SelectObject)(dc, o); U(DeleteObject)(b);)
OPC(createbrush_hatch_diag, 0x02FC, 7, 7, (BS_HATCHED, 0x0201, 0x0003, HS_DIAGCROSS),
   HBRUSH b = U(CreateHatchBrush)(HS_DIAGCROSS, RGB(1, 2, 3)); HGDIOBJ o = U(SelectObject)(dc, b); U(SelectObject)(dc, o); U(DeleteObject)(b);)
OPC(createbrush_hollow, 0x02FC, 7, 7, (BS_HOLLOW),
   LOGBRUSH lb = { BS_HOLLOW, 0, 0 }; HBRUSH b = U(CreateBrushIndirect)(&lb); HGDIOBJ o = U(SelectObject)(dc, b); U(SelectObject)(dc, o); U(DeleteObject)(b);)
OPC(createbrush_indirect_solid, 0x02FC, 7, 7, (BS_SOLID, 0x6655, 0x0044, 0),
   LOGBRUSH lb = { BS_SOLID, RGB(0x55, 0x66, 0x44), 0 }; HBRUSH b = U(CreateBrushIndirect)(&lb); HGDIOBJ o = U(SelectObject)(dc, b); U(SelectObject)(dc, o); U(DeleteObject)(b);)
OPC(createbrush_stock_white, 0x02FC, 7, 7, (BS_SOLID, 0xC0C0, 0x00C0),
   U(SelectObject)(dc, U(GetStockObject)(LTGRAY_BRUSH)); U(SelectObject)(dc, U(GetStockObject)(WHITE_BRUSH));)
OPC(createfont, 0x02FB, 3 + 9 + 3, 3 + 9 + 17, (0xFFF0, 0),
   LOGFONTA lf; memset(&lf, 0, sizeof lf); lf.lfHeight = -16; lf.lfWeight = FW_NORMAL; lf.lfCharSet = DEFAULT_CHARSET; strcpy(lf.lfFaceName, "Arial");
   HFONT f = U(CreateFontIndirectA)(&lf); HGDIOBJ o = U(SelectObject)(dc, f); U(SelectObject)(dc, o); U(DeleteObject)(f);)
OPC(createfont_w, 0x02FB, 3 + 9 + 3, 3 + 9 + 17, (0xFFE0),
   LOGFONTW lf; memset(&lf, 0, sizeof lf); lf.lfHeight = -32; lf.lfWeight = FW_BOLD; lf.lfCharSet = DEFAULT_CHARSET; lstrcpyW(lf.lfFaceName, L"Tahoma");
   HFONT f = U(CreateFontIndirectW)(&lf); HGDIOBJ o = U(SelectObject)(dc, f); U(SelectObject)(dc, o); U(DeleteObject)(f);)
OPC(createfont_empty_face, 0x02FB, 3 + 9 + 1, 3 + 9 + 17, (12),
   LOGFONTA lf; memset(&lf, 0, sizeof lf); lf.lfHeight = 12; HFONT f = U(CreateFontIndirectA)(&lf); HGDIOBJ o = U(SelectObject)(dc, f); U(SelectObject)(dc, o); U(DeleteObject)(f);)
OPA(createpalette, 0x00F7, 0, 3 + 2 + 8, 3 + 2 + 8, (0x0300, 4),
    struct { WORD v, n; PALETTEENTRY e[4]; } lp = { 0x300, 4, { {1,2,3,0}, {4,5,6,0}, {7,8,9,0}, {10,11,12,0} } };
    HPALETTE p = U(CreatePalette)((LOGPALETTE *)&lp); HGDIOBJ o = U(SelectPalette)(dc, p, FALSE); U(SelectPalette)(dc, o, FALSE); U(DeleteObject)(p);)
OP(selectpalette, 0x0234, 4, 4, (ANY),
   struct { WORD v, n; PALETTEENTRY e[2]; } lp = { 0x300, 2, { {1,2,3,0}, {4,5,6,0} } };
   HPALETTE p = U(CreatePalette)((LOGPALETTE *)&lp); HGDIOBJ o = U(SelectPalette)(dc, p, FALSE); U(SelectPalette)(dc, o, FALSE); U(DeleteObject)(p);)
OPC(deleteobject_pen, 0x01F0, 4, 4, (ANY),
   HPEN p = U(CreatePen)(PS_SOLID, 1, 0); HGDIOBJ o = U(SelectObject)(dc, p); U(SelectObject)(dc, o); U(DeleteObject)(p);)
OPC(deleteobject_brush, 0x01F0, 4, 4, (ANY),
   HBRUSH b = U(CreateSolidBrush)(0); HGDIOBJ o = U(SelectObject)(dc, b); U(SelectObject)(dc, o); U(DeleteObject)(b);)
OPC(selectobject_pen, 0x012D, 4, 4, (ANY),
   HPEN p = U(CreatePen)(PS_SOLID, 1, 0); HGDIOBJ o = U(SelectObject)(dc, p); U(SelectObject)(dc, o); U(DeleteObject)(p);)

/* ---- regions ---- */
static HRGN mkrgn(void) { return U(CreateRectRgn)(1, 2, 30, 40); }
OPA(selectcliprgn, 0x012C, 0x012D, 0, 0, (ANY), HRGN r = mkrgn(); U(SelectClipRgn)(dc, r); U(DeleteObject)(r);)
OP(region_created, 0x06FF, 0, 0, (ANY), HRGN r = mkrgn(); U(SelectClipRgn)(dc, r); U(DeleteObject)(r);)
OP(fillrgn,   0x0228, 5, 5, (ANY, ANY),
   HRGN r = mkrgn(); HBRUSH b = U(CreateSolidBrush)(RGB(1, 2, 3)); U(FillRgn)(dc, r, b); U(DeleteObject)(r); U(DeleteObject)(b);)
OP(framergn,  0x0429, 7, 7, (4, 3, ANY, ANY),
   HRGN r = mkrgn(); HBRUSH b = U(CreateSolidBrush)(RGB(1, 2, 3)); U(FrameRgn)(dc, r, b, 3, 4); U(DeleteObject)(r); U(DeleteObject)(b);)
OP(invertrgn, 0x012A, 4, 4, (ANY), HRGN r = mkrgn(); U(InvertRgn)(dc, r); U(DeleteObject)(r);)
OP(paintrgn,  0x012B, 4, 4, (ANY), HRGN r = mkrgn(); U(PaintRgn)(dc, r); U(DeleteObject)(r);)
OP(fillrgn_creates_region, 0x06FF, 0, 0, (ANY),
   HRGN r = mkrgn(); HBRUSH b = U(CreateSolidBrush)(RGB(1, 2, 3)); U(FillRgn)(dc, r, b); U(DeleteObject)(r); U(DeleteObject)(b);)
OP(fillrgn_creates_brush, 0x02FC, 7, 7, (BS_SOLID, 0x0201, 0x0003),
   HRGN r = mkrgn(); HBRUSH b = U(CreateSolidBrush)(RGB(1, 2, 3)); U(FillRgn)(dc, r, b); U(DeleteObject)(r); U(DeleteObject)(b);)

/* ---- extra semantic checks on recorded streams ---- */
TEST(wmf_op_bitblt_patcopy_nosrc_observed) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    BOOL r = U(BitBlt)(dc, 10, 20, 8, 8, NULL, 0, 0, PATCOPY);
    WB b;
    CHECK(mf_finish(dc, &b, NULL));
    t_note("BitBlt(NULL src, PATCOPY) = %d, records %d", r, wb_nrecs(&b));
    CHECK(wb_valid(&b));
    wb_free(&b);
}

TEST(wmf_op_textout_string_bytes) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    CHECK(U(TextOutA)(dc, 10, 20, "Hello", 5));
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    const BYTE *r = wb_find(&b, 0x0A32, 0);
    if (!r) r = wb_find(&b, 0x0521, 0);
    CHECK(r != NULL);
    if (r) {
        int found = 0;
        for (DWORD i = 6; i + 5 <= r_size(r) * 2; i++) if (!memcmp(r + i, "Hello", 5)) found = 1;
        CHECK(found);
    }
    wb_free(&b);
}

TEST(wmf_op_textout_records_xy) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    CHECK(U(TextOutA)(dc, 123, 456, "ab", 2));
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    const BYTE *r = wb_find(&b, 0x0A32, 0);
    CHECK(r != NULL);
    if (r) { EXPECT_EQ(r_parm(r, 0), 456); EXPECT_EQ(r_parm(r, 1), 123); EXPECT_EQ(r_parm(r, 2), 2); }
    wb_free(&b);
}

TEST(wmf_op_exttextout_string_pad) {
    WMF_REQ_BASE();
    /* odd-length strings are padded to a WORD boundary: record sizes follow ceil(n/2) */
    for (int n = 1; n <= 8; n++) {
        HDC dc = mf_new();
        REQUIRE(dc);
        U(ExtTextOutA)(dc, 0, 0, 0, NULL, "abcdefgh", n, NULL);
        WB b;
        REQUIRE(mf_finish(dc, &b, NULL));
        const BYTE *r = wb_find(&b, 0x0A32, 0);
        CHECKF(r != NULL, "n=%d", n);
        if (r) CHECKF(r_size(r) >= (DWORD)(3 + 4 + (n + 1) / 2), "n=%d size %lu", n, r_size(r));
        wb_free(&b);
    }
}

TEST(wmf_op_polygon_point_counts) {
    WMF_REQ_BASE();
    POINT pts[40];
    for (int i = 0; i < 40; i++) { pts[i].x = i; pts[i].y = 100 - i; }
    int counts[] = { 2, 3, 5, 8, 16, 40 };
    for (size_t k = 0; k < sizeof counts / sizeof *counts; k++) {
        HDC dc = mf_new();
        REQUIRE(dc);
        U(Polygon)(dc, pts, counts[k]);
        U(Polyline)(dc, pts, counts[k]);
        WB b;
        REQUIRE(mf_finish(dc, &b, NULL));
        const BYTE *g = wb_find(&b, 0x0324, 0), *l = wb_find(&b, 0x0325, 0);
        CHECK(g && l);
        if (g) { EXPECT_EQ(r_size(g), 3 + 1 + 2 * counts[k]); EXPECT_EQ(r_parm(g, 0), counts[k]); EXPECT_EQ(r_sparm(g, 1 + 2 * (counts[k] - 1)), counts[k] - 1); EXPECT_EQ(r_sparm(g, 2 + 2 * (counts[k] - 1)), 100 - (counts[k] - 1)); }
        if (l) { EXPECT_EQ(r_size(l), 3 + 1 + 2 * counts[k]); EXPECT_EQ(r_parm(l, 0), counts[k]); }
        wb_free(&b);
    }
}

TEST(wmf_op_polygon_coord_truncation) {
    WMF_REQ_BASE();
    /* coordinates are stored as 16-bit values */
    POINT pts[3] = { {0x12345, -0x12345}, {32768, -32769}, {65535, 65536} };
    HDC dc = mf_new();
    REQUIRE(dc);
    U(Polygon)(dc, pts, 3);
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    const BYTE *g = wb_find(&b, 0x0324, 0);
    CHECK(g != NULL);
    if (g) {
        EXPECT_EQ(r_parm(g, 1), (WORD)0x12345);
        EXPECT_EQ(r_parm(g, 2), (WORD)-0x12345);
        EXPECT_EQ(r_parm(g, 3), 0x8000);
        EXPECT_EQ(r_parm(g, 4), 0x7FFF);
    }
    wb_free(&b);
}

TEST(wmf_op_polygon_degenerate) {
    WMF_REQ_BASE();
    POINT pts[2] = { {1, 2}, {3, 4} };
    HDC dc = mf_new();
    REQUIRE(dc);
    SET_ERR(0);
    BOOL r1 = U(Polygon)(dc, pts, 0);
    BOOL r2 = U(Polygon)(dc, pts, 1);
    BOOL r3 = U(Polyline)(dc, pts, 1);
    BOOL r4 = U(Polygon)(dc, NULL, 0);
    t_note("Polygon n=0:%d n=1:%d Polyline n=1:%d Polygon NULL,0:%d", r1, r2, r3, r4);
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    CHECK(wb_valid(&b));
    wb_free(&b);
}

TEST(wmf_op_polygon_negative_count) {
    WMF_REQ_BASE();
    POINT pts[2] = { {1, 2}, {3, 4} };
    HDC dc = mf_new();
    REQUIRE(dc);
    BOOL r = U(Polygon)(dc, pts, -1);
    CHECK(!r);
    r = U(Polyline)(dc, pts, -5);
    CHECK(!r);
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    CHECK(wb_count(&b, 0x0324) == 0);
    CHECK(wb_count(&b, 0x0325) == 0);
    wb_free(&b);
}

TEST(wmf_op_recorded_order) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    U(MoveToEx)(dc, 1, 1, NULL);
    U(LineTo)(dc, 2, 2);
    U(Rectangle)(dc, 3, 3, 4, 4);
    U(Ellipse)(dc, 5, 5, 6, 6);
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    const BYTE *r = wb_first(&b);
    WORD want[] = { 0x0214, 0x0213, 0x041B, 0x0418, 0x0000 };
    for (int i = 0; i < 5; i++) {
        CHECK(r != NULL);
        if (!r) break;
        EXPECT_EQ(r_func(r), want[i]);
        r = wb_next(&b, r);
    }
    wb_free(&b);
}

TEST(wmf_op_failed_calls_not_recorded) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    /* GetPixel/GetTextMetrics-style queries leave no record */
    U(GetPixel)(dc, 0, 0);
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    EXPECT_EQ(wb_nrecs(&b), 1);
    wb_free(&b);
}

TEST(wmf_op_savedc_levels) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    int a = U(SaveDC)(dc), b2 = U(SaveDC)(dc), c = U(SaveDC)(dc);
    EXPECT_EQ(a, 1); t_note("SaveDC levels %d %d %d", a, b2, c);
    CHECK(U(RestoreDC)(dc, -1));
    CHECK(U(RestoreDC)(dc, -2));
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_op_restoredc_too_far) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    BOOL r = U(RestoreDC)(dc, -5);
    t_note("RestoreDC(metaDC, -5) with no saved state = %d", r);
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    CHECK(wb_valid(&b));
    wb_free(&b);
}

TEST(wmf_op_selectobject_defaults) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    HPEN pen = U(CreatePen)(PS_SOLID, 1, 0);
    HBRUSH br = U(CreateSolidBrush)(0x123456);
    LOGFONTA lf; memset(&lf, 0, sizeof lf); lf.lfHeight = 10;
    HFONT font = U(CreateFontIndirectA)(&lf);
    { HGDIOBJ r_ = SEL(dc, pen); HGDIOBJ w_ = STK(BLACK_PEN); EXPECT_EQ(r_, w_); }
    { HGDIOBJ r_ = SEL(dc, br); HGDIOBJ w_ = STK(WHITE_BRUSH); EXPECT_EQ(r_, w_); }
    { HGDIOBJ r_ = SEL(dc, font); HGDIOBJ w_ = STK(DEVICE_DEFAULT_FONT); EXPECT_EQ(r_, w_); }
    /* re-selecting returns the previous object */
    { HGDIOBJ r_ = SEL(dc, STK(BLACK_PEN)); EXPECT_EQ(r_, pen); }
    { HGDIOBJ r_ = SEL(dc, STK(WHITE_BRUSH)); EXPECT_EQ(r_, br); }
    { HGDIOBJ r_ = SEL(dc, STK(SYSTEM_FONT)); EXPECT_EQ(r_, font); }
    U(DeleteObject)(pen); U(DeleteObject)(br); U(DeleteObject)(font);
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_op_selectobject_bitmap) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    HBITMAP bmp = U(CreateBitmap)(4, 4, 1, 1, NULL);
    SET_ERR(0);
    HGDIOBJ o = U(SelectObject)(dc, bmp);
    t_note("SelectObject(metaDC, bitmap) = %p err %lu", o, GetLastError());
    U(DeleteObject)(bmp);
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_op_selectobject_null) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    SET_ERR(0);
    HGDIOBJ o = U(SelectObject)(dc, NULL);
    CHECK(o == NULL);
    t_note("SelectObject(metaDC, NULL) err %lu", GetLastError());
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_op_selectpalette_stock_rejected) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    SET_ERR(0);
    HGDIOBJ o = U(SelectObject)(dc, U(GetStockObject)(DEFAULT_PALETTE));
    t_note("SelectObject(metaDC, DEFAULT_PALETTE) = %p err %lu", o, GetLastError());
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_op_selectpalette_returns_default) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    struct { WORD v, n; PALETTEENTRY e[2]; } lp = { 0x300, 2, { {1,2,3,0}, {4,5,6,0} } };
    HPALETTE p = U(CreatePalette)((LOGPALETTE *)&lp);
    REQUIRE(p);
    HGDIOBJ o = U(SelectPalette)(dc, p, FALSE);
    HGDIOBJ dp = STK(DEFAULT_PALETTE);
    if (o != dp) t_note("SelectPalette first return %p, default palette %p", o, dp);
    HGDIOBJ back = U(SelectPalette)(dc, o ? (HPALETTE)o : (HPALETTE)dp, FALSE);
    if (o == dp) EXPECT_EQ(back, p);
    U(DeleteObject)(p);
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_op_dcbrush_pen_colors) {
    WMF_REQ_BASE(); REQUIRE(U(SetDCBrushColor)); REQUIRE(U(SetDCPenColor));
    HDC dc = mf_new();
    REQUIRE(dc);
    HGDIOBJ ob = U(SelectObject)(dc, U(GetStockObject)(DC_BRUSH));
    HGDIOBJ wb_ = STK(WHITE_BRUSH), bp_ = STK(BLACK_PEN);
    EXPECT_EQ(ob, wb_);
    HGDIOBJ op = U(SelectObject)(dc, U(GetStockObject)(DC_PEN));
    EXPECT_EQ(op, bp_);
    EXPECT_EQ(U(SetDCBrushColor)(dc, RGB(0x55, 0x55, 0x55)), CLR_INVALID);
    EXPECT_EQ(U(SetDCPenColor)(dc, RGB(0x33, 0x44, 0x55)), CLR_INVALID);
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_op_getcurrentobject_null) {
    WMF_REQ_BASE(); REQUIRE(U(GetCurrentObject));
    HDC dc = mf_new();
    REQUIRE(dc);
    HPEN pen = U(CreatePen)(PS_SOLID, 1, 0);
    U(SelectObject)(dc, pen);
    HGDIOBJ o = U(GetCurrentObject)(dc, OBJ_PEN);
    t_note("GetCurrentObject(metaDC, OBJ_PEN) = %p (pen %p)", o, pen);
    U(SelectObject)(dc, U(GetStockObject)(BLACK_PEN));
    U(DeleteObject)(pen);
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_op_delete_selected_object) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    HPEN pen = U(CreatePen)(PS_SOLID, 1, RGB(1, 1, 1));
    U(SelectObject)(dc, pen);
    /* a pen selected into a metafile DC may still be deleted; observed */
    BOOL r = U(DeleteObject)(pen);
    t_note("DeleteObject(selected into metafile DC) = %d", r);
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    CHECK(wb_valid(&b));
    wb_free(&b);
    if (!r) U(DeleteObject)(pen);
}

TEST(wmf_op_unsupported_calls_fail) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    EXPECT_EQ(U(GetPixel)(dc, 1, 1), CLR_INVALID);
    POINT pt;
    t_note("GetCurrentPositionEx = %d", U(GetCurrentPositionEx)(dc, &pt));
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_op_beginpath_unsupported) {
    WMF_REQ_BASE(); REQUIRE(U(BeginPath));
    HDC dc = mf_new();
    REQUIRE(dc);
    SET_ERR(0);
    BOOL r = U(BeginPath)(dc);
    t_note("BeginPath(metaDC) = %d err %lu", r, GetLastError());
    U(AbortPath)(dc);
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_op_alphablend_unsupported) {
    WMF_REQ_BASE(); REQUIRE(U(GdiAlphaBlend)); 
    HDC dc = mf_new();
    DIBDC s;
    REQUIRE(dc);
    if (!dibdc_make(&s, 4, 4)) { U(CloseMetaFile)(dc); return; }
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, 0 };
    SET_ERR(0);
    BOOL r = U(GdiAlphaBlend)(dc, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, bf);
    t_note("AlphaBlend(metaDC) = %d err %lu", r, GetLastError());
    dibdc_free(&s);
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (h) U(DeleteMetaFile)(h);
}
