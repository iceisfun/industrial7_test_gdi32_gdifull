#include "emf.h"

typedef void (*scene_fn)(HDC);

/* Record a scene, then verify the generic structure and that `exp` appears (in order) in the record list.
 * soft: a missing record is only logged. */
static void rec_run(scene_fn s, const DWORD *exp, int n, int soft) {
    DWORD g0 = emf_gdi_count();
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    s(dc);
    HENHMETAFILE h = emf_end(dc);
    CHECK(h != NULL);
    if (!h) return;
    emf_log l; emf_enum(h, &l);
    ENHMETAHEADER hd; U(GetEnhMetaFileHeader)(h, sizeof hd, &hd);
    CHECK(l.n >= 2);
    CHECK(l.n > 0 && l.type[0] == EMR_HEADER);
    CHECK(l.n > 0 && l.n <= MAXLOG && l.type[l.n - 1] == EMR_EOF);
    EXPECT_EQ(l.n, hd.nRecords);
    EXPECT_EQ(l.nobj, hd.nHandles);
    EXPECT_EQ(l.bad_dc, 0);
    DWORD total = 0; int sizes_ok = 1;
    for (int i = 0; i < l.n && i < MAXLOG; i++) {
        total += l.size[i];
        if (l.size[i] < 8 || (l.size[i] & 3)) sizes_ok = 0;
    }
    CHECK(sizes_ok);
    EXPECT_EQ(total, hd.nBytes);
    int found = emf_has_seq(&l, exp, n);
    if (soft) { if (!found) t_note("soft: expected record sequence missing (first=%lu)", (unsigned long)exp[0]); }
    else CHECKF(found, "expected sequence starting %lu not found in %d records", (unsigned long)exp[0], l.n);
    if (!found) {
        char tmp[512]; int p = 0;
        for (int i = 0; i < l.n && i < 40 && p < 480; i++) p += sprintf(tmp + p, "%lu ", (unsigned long)l.type[i]);
        t_note("record types: %s", tmp);
    }
    /* the metafile must be playable into a DIB without crashing */
    emf_dib d;
    if (emf_dib_make(&d, 64, 64, 0xFFFFFF)) {
        RECT rc = {0, 0, 64, 64};
        BOOL ok = U(PlayEnhMetaFile)(d.dc, h, &rc);
        SOFT(ok);
        emf_dib_free(&d);
    }
    U(DeleteEnhMetaFile)(h);
    EXPECT_EQ(emf_gdi_count(), g0);
}

#define RT(name, ...) \
    TEST(emf_rec_##name) { REQUIRE(U(CreateEnhMetaFileW)); static const DWORD e[] = { __VA_ARGS__ }; \
        rec_run(sc_##name, e, sizeof e / sizeof e[0], 0); }
#define RTS(name, ...) \
    TEST(emf_rec_##name) { REQUIRE(U(CreateEnhMetaFileW)); static const DWORD e[] = { __VA_ARGS__ }; \
        rec_run(sc_##name, e, sizeof e / sizeof e[0], 1); }

/* ---- state records ---- */
static void sc_setmapmode(HDC dc) { U(SetMapMode)(dc, MM_ANISOTROPIC); }
RT(setmapmode, EMR_SETMAPMODE)
static void sc_windowext(HDC dc) { U(SetMapMode)(dc, MM_ANISOTROPIC); U(SetWindowExtEx)(dc, 100, 200, NULL); }
RT(windowext, EMR_SETMAPMODE, EMR_SETWINDOWEXTEX)
static void sc_viewportext(HDC dc) { U(SetMapMode)(dc, MM_ANISOTROPIC); U(SetViewportExtEx)(dc, 100, 200, NULL); }
RT(viewportext, EMR_SETMAPMODE, EMR_SETVIEWPORTEXTEX)
static void sc_windoworg(HDC dc) { U(SetWindowOrgEx)(dc, 11, 22, NULL); }
RT(windoworg, EMR_SETWINDOWORGEX)
static void sc_viewportorg(HDC dc) { U(SetViewportOrgEx)(dc, 11, 22, NULL); }
RT(viewportorg, EMR_SETVIEWPORTORGEX)
static void sc_scaleviewport(HDC dc) { U(SetMapMode)(dc, MM_ANISOTROPIC); U(ScaleViewportExtEx)(dc, 1, 2, 3, 4, NULL); }
RT(scaleviewport, EMR_SCALEVIEWPORTEXTEX)
static void sc_scalewindow(HDC dc) { U(SetMapMode)(dc, MM_ANISOTROPIC); U(ScaleWindowExtEx)(dc, 1, 2, 3, 4, NULL); }
RT(scalewindow, EMR_SCALEWINDOWEXTEX)
static void sc_bkmode(HDC dc) { U(SetBkMode)(dc, TRANSPARENT); }
RT(bkmode, EMR_SETBKMODE)
static void sc_bkcolor(HDC dc) { U(SetBkColor)(dc, RGB(1, 2, 3)); }
RT(bkcolor, EMR_SETBKCOLOR)
static void sc_textcolor(HDC dc) { U(SetTextColor)(dc, RGB(4, 5, 6)); }
RT(textcolor, EMR_SETTEXTCOLOR)
static void sc_rop2(HDC dc) { U(SetROP2)(dc, R2_XORPEN); }
RT(rop2, EMR_SETROP2)
static void sc_polyfill(HDC dc) { U(SetPolyFillMode)(dc, WINDING); }
RT(polyfill, EMR_SETPOLYFILLMODE)
static void sc_stretchmode(HDC dc) { U(SetStretchBltMode)(dc, HALFTONE); }
RT(stretchmode, EMR_SETSTRETCHBLTMODE)
static void sc_textalign(HDC dc) { U(SetTextAlign)(dc, TA_BASELINE | TA_CENTER); }
RT(textalign, EMR_SETTEXTALIGN)
static void sc_mapperflags(HDC dc) { U(SetMapperFlags)(dc, 1); }
RT(mapperflags, EMR_SETMAPPERFLAGS)
static void sc_miterlimit(HDC dc) { U(SetMiterLimit)(dc, 5.0f, NULL); }
RT(miterlimit, EMR_SETMITERLIMIT)
static void sc_arcdirection(HDC dc) { U(SetArcDirection)(dc, AD_CLOCKWISE); }
RTS(arcdirection, EMR_SETARCDIRECTION)
static void sc_layout(HDC dc) { U(SetLayout)(dc, LAYOUT_RTL); }
RTS(layout, EMR_SETLAYOUT)
static void sc_brushorg(HDC dc) { U(SetBrushOrgEx)(dc, 3, 4, NULL); }
RT(brushorg, EMR_SETBRUSHORGEX)
static void sc_icmmode(HDC dc) { U(SetICMMode)(dc, ICM_OFF); }
RTS(icmmode, EMR_SETICMMODE)
static void sc_savedc(HDC dc) { U(SaveDC)(dc); U(RestoreDC)(dc, -1); }
RT(savedc, EMR_SAVEDC, EMR_RESTOREDC)
static void sc_savedc_abs(HDC dc) { int a = U(SaveDC)(dc); U(SaveDC)(dc); U(RestoreDC)(dc, a); }
RT(savedc_abs, EMR_SAVEDC, EMR_SAVEDC, EMR_RESTOREDC)
static void sc_worldxform(HDC dc) {
    XFORM x = {2, 0, 0, 2, 5, 5};
    U(SetGraphicsMode)(dc, GM_ADVANCED); U(SetWorldTransform)(dc, &x); U(ModifyWorldTransform)(dc, &x, MWT_RIGHTMULTIPLY);
    U(ModifyWorldTransform)(dc, NULL, MWT_IDENTITY);
}
RT(worldxform, EMR_SETWORLDTRANSFORM, EMR_MODIFYWORLDTRANSFORM, EMR_MODIFYWORLDTRANSFORM)
static void sc_worldxform_compat(HDC dc) { XFORM x = {2, 0, 0, 2, 5, 5}; U(SetWorldTransform)(dc, &x); U(Rectangle)(dc, 0, 0, 3, 3); }
RT(worldxform_compat, EMR_RECTANGLE)

/* ---- drawing records ---- */
static void sc_moveto(HDC dc) { U(MoveToEx)(dc, 7, 8, NULL); }
RT(moveto, EMR_MOVETOEX)
static void sc_lineto(HDC dc) { U(MoveToEx)(dc, 7, 8, NULL); U(LineTo)(dc, 70, 80); }
RT(lineto, EMR_MOVETOEX, EMR_LINETO)
static void sc_arc(HDC dc) { U(Arc)(dc, 0, 0, 50, 50, 50, 25, 25, 0); }
RT(arc, EMR_ARC)
static void sc_arcto(HDC dc) { U(ArcTo)(dc, 0, 0, 50, 50, 50, 25, 25, 0); }
RT(arcto, EMR_ARCTO)
static void sc_anglearc(HDC dc) { U(AngleArc)(dc, 30, 30, 20, 0, 90); }
RT(anglearc, EMR_ANGLEARC)
static void sc_chord(HDC dc) { U(Chord)(dc, 0, 0, 50, 50, 50, 25, 25, 0); }
RT(chord, EMR_CHORD)
static void sc_pie(HDC dc) { U(Pie)(dc, 0, 0, 50, 50, 50, 25, 25, 0); }
RT(pie, EMR_PIE)
static void sc_ellipse(HDC dc) { U(Ellipse)(dc, 1, 2, 40, 50); }
RT(ellipse, EMR_ELLIPSE)
static void sc_rectangle(HDC dc) { U(Rectangle)(dc, 10, 10, 50, 40); }
RT(rectangle, EMR_RECTANGLE)
static void sc_roundrect(HDC dc) { U(RoundRect)(dc, 10, 10, 50, 40, 8, 8); }
RT(roundrect, EMR_ROUNDRECT)
static const POINT P5[5] = { {1, 1}, {40, 2}, {45, 30}, {20, 50}, {2, 35} };
static void sc_polygon16(HDC dc) { U(Polygon)(dc, P5, 5); }
RT(polygon16, EMR_POLYGON16)
static void sc_polygon32(HDC dc) { POINT p[3] = { {0, 0}, {40000, 10}, {10, 40000} }; U(Polygon)(dc, p, 3); }
RT(polygon32, EMR_POLYGON)
static void sc_polygon_neg(HDC dc) { POINT p[3] = { {-100, -100}, {40, 10}, {10, 40} }; U(Polygon)(dc, p, 3); }
RT(polygon_neg, EMR_POLYGON16)
static void sc_polygon_negbig(HDC dc) { POINT p[3] = { {-40000, 0}, {40, 10}, {10, 40} }; U(Polygon)(dc, p, 3); }
RT(polygon_negbig, EMR_POLYGON)
static void sc_polygon_edge(HDC dc) { POINT p[3] = { {0, 0}, {32767, 10}, {10, -32768} }; U(Polygon)(dc, p, 3); }
RT(polygon_edge, EMR_POLYGON16)
static void sc_polygon_over(HDC dc) { POINT p[3] = { {0, 0}, {32768, 10}, {10, 20} }; U(Polygon)(dc, p, 3); }
RT(polygon_over, EMR_POLYGON)
static void sc_polyline16(HDC dc) { U(Polyline)(dc, P5, 5); }
RT(polyline16, EMR_POLYLINE16)
static void sc_polyline32(HDC dc) { POINT p[2] = { {0, 0}, {50000, 3} }; U(Polyline)(dc, p, 2); }
RT(polyline32, EMR_POLYLINE)
static void sc_polylineto16(HDC dc) { U(PolylineTo)(dc, P5, 5); }
RT(polylineto16, EMR_POLYLINETO16)
static void sc_polybezier16(HDC dc) { POINT p[4] = { {0, 0}, {10, 30}, {30, 30}, {40, 0} }; U(PolyBezier)(dc, p, 4); }
RT(polybezier16, EMR_POLYBEZIER16)
static void sc_polybezierto16(HDC dc) { POINT p[3] = { {10, 30}, {30, 30}, {40, 0} }; U(PolyBezierTo)(dc, p, 3); }
RT(polybezierto16, EMR_POLYBEZIERTO16)
static void sc_polybezier32(HDC dc) { POINT p[4] = { {0, 0}, {10, 30}, {30, 30}, {40000, 0} }; U(PolyBezier)(dc, p, 4); }
RT(polybezier32, EMR_POLYBEZIER)
static void sc_polypolygon16(HDC dc) {
    POINT p[6] = { {0, 0}, {20, 0}, {10, 15}, {30, 30}, {50, 30}, {40, 45} }; DWORD c[2] = {3, 3};
    U(PolyPolygon)(dc, p, (INT *)c, 2);
}
RT(polypolygon16, EMR_POLYPOLYGON16)
static void sc_polypolyline16(HDC dc) {
    POINT p[6] = { {0, 0}, {20, 0}, {10, 15}, {30, 30}, {50, 30}, {40, 45} }; DWORD c[2] = {3, 3};
    U(PolyPolyline)(dc, p, c, 2);
}
RT(polypolyline16, EMR_POLYPOLYLINE16)
static void sc_polydraw(HDC dc) {
    POINT p[3] = { {0, 0}, {20, 20}, {40, 0} }; BYTE t[3] = { PT_MOVETO, PT_LINETO, PT_LINETO };
    U(PolyDraw)(dc, p, t, 3);
}
RTS(polydraw, EMR_POLYDRAW16)
static void sc_setpixelv(HDC dc) { U(SetPixelV)(dc, 5, 6, RGB(255, 0, 0)); }
RT(setpixelv, EMR_SETPIXELV)
static void sc_setpixel(HDC dc) { U(SetPixel)(dc, 5, 6, RGB(255, 0, 0)); }
RT(setpixel, EMR_SETPIXELV)
static void sc_floodfill(HDC dc) { U(ExtFloodFill)(dc, 5, 6, RGB(0, 0, 0), FLOODFILLBORDER); }
RT(floodfill, EMR_EXTFLOODFILL)
static void sc_fillrgn(HDC dc) {
    HRGN r = U(CreateRectRgn)(0, 0, 30, 30); HBRUSH b = U(CreateSolidBrush)(RGB(1, 2, 3));
    U(FillRgn)(dc, r, b); U(FrameRgn)(dc, r, b, 2, 2); U(InvertRgn)(dc, r); U(PaintRgn)(dc, r);
    U(DeleteObject)(r); U(DeleteObject)(b);
}
RT(fillrgn, EMR_CREATEBRUSHINDIRECT, EMR_FILLRGN, EMR_FRAMERGN, EMR_INVERTRGN, EMR_PAINTRGN)
static void sc_fillrgn_only(HDC dc) { HRGN r = U(CreateRectRgn)(0, 0, 30, 30); U(FillRgn)(dc, r, (HBRUSH)U(GetStockObject)(GRAY_BRUSH)); U(DeleteObject)(r); }
RT(fillrgn_only, EMR_FILLRGN)
static void sc_gradient(HDC dc) {
    TRIVERTEX v[2] = { {0, 0, 0xff00, 0, 0, 0}, {20, 20, 0, 0xff00, 0, 0} }; GRADIENT_RECT gr = {0, 1};
    U(GradientFill)(dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H);
}
RT(gradient, EMR_GRADIENTFILL)

/* ---- clip records ---- */
static void sc_selectcliprgn(HDC dc) { HRGN r = U(CreateRectRgn)(0, 0, 20, 20); U(SelectClipRgn)(dc, r); U(SelectClipRgn)(dc, NULL); U(DeleteObject)(r); }
RT(selectcliprgn, EMR_EXTSELECTCLIPRGN, EMR_EXTSELECTCLIPRGN)
static void sc_extselectcliprgn(HDC dc) { HRGN r = U(CreateRectRgn)(0, 0, 20, 20); U(ExtSelectClipRgn)(dc, r, RGN_OR); U(DeleteObject)(r); }
RT(extselectcliprgn, EMR_EXTSELECTCLIPRGN)
static void sc_intersectclip(HDC dc) { U(IntersectClipRect)(dc, 1, 2, 30, 40); }
RT(intersectclip, EMR_INTERSECTCLIPRECT)
static void sc_excludeclip(HDC dc) { U(ExcludeClipRect)(dc, 1, 2, 30, 40); }
RT(excludeclip, EMR_EXCLUDECLIPRECT)
static void sc_offsetclip(HDC dc) { U(IntersectClipRect)(dc, 1, 2, 30, 40); U(OffsetClipRgn)(dc, 3, 4); }
RT(offsetclip, EMR_INTERSECTCLIPRECT, EMR_OFFSETCLIPRGN)
static void sc_metargn(HDC dc) { U(SetMetaRgn)(dc); }
RT(metargn, EMR_SETMETARGN)

/* ---- path records ---- */
static void sc_path_fill(HDC dc) { U(BeginPath)(dc); U(Rectangle)(dc, 1, 1, 30, 30); U(EndPath)(dc); U(FillPath)(dc); }
RT(path_fill, EMR_BEGINPATH, EMR_RECTANGLE, EMR_ENDPATH, EMR_FILLPATH)
static void sc_path_stroke(HDC dc) { U(BeginPath)(dc); U(Ellipse)(dc, 1, 1, 30, 30); U(EndPath)(dc); U(StrokePath)(dc); }
RT(path_stroke, EMR_BEGINPATH, EMR_ELLIPSE, EMR_ENDPATH, EMR_STROKEPATH)
static void sc_path_strokefill(HDC dc) { U(BeginPath)(dc); U(Rectangle)(dc, 1, 1, 30, 30); U(EndPath)(dc); U(StrokeAndFillPath)(dc); }
RT(path_strokefill, EMR_BEGINPATH, EMR_ENDPATH, EMR_STROKEANDFILLPATH)
static void sc_path_close(HDC dc) { U(BeginPath)(dc); U(MoveToEx)(dc, 1, 1, NULL); U(LineTo)(dc, 20, 1); U(LineTo)(dc, 20, 20); U(CloseFigure)(dc); U(EndPath)(dc); }
RT(path_close, EMR_BEGINPATH, EMR_MOVETOEX, EMR_LINETO, EMR_LINETO, EMR_CLOSEFIGURE, EMR_ENDPATH)
static void sc_path_clip(HDC dc) { U(BeginPath)(dc); U(Rectangle)(dc, 1, 1, 30, 30); U(EndPath)(dc); U(SelectClipPath)(dc, RGN_COPY); }
RT(path_clip, EMR_BEGINPATH, EMR_ENDPATH, EMR_SELECTCLIPPATH)
static void sc_path_abort(HDC dc) { U(BeginPath)(dc); U(Rectangle)(dc, 1, 1, 30, 30); U(AbortPath)(dc); }
RT(path_abort, EMR_BEGINPATH, EMR_RECTANGLE, EMR_ABORTPATH)
static void sc_path_flatten(HDC dc) { U(BeginPath)(dc); U(Ellipse)(dc, 1, 1, 30, 30); U(EndPath)(dc); U(FlattenPath)(dc); }
RT(path_flatten, EMR_FLATTENPATH)
static void sc_path_widen(HDC dc) { U(BeginPath)(dc); U(MoveToEx)(dc, 1, 1, NULL); U(LineTo)(dc, 30, 30); U(EndPath)(dc); U(WidenPath)(dc); }
RTS(path_widen, EMR_WIDENPATH)

/* ---- text records ---- */
static void sc_textout_w(HDC dc) { U(TextOutW)(dc, 5, 5, L"Hello", 5); }
RT(textout_w, EMR_EXTTEXTOUTW)
static void sc_textout_a(HDC dc) { U(TextOutA)(dc, 5, 5, "Hello", 5); }
RT(textout_a, EMR_EXTTEXTOUTW)
static void sc_exttextout(HDC dc) { RECT r = {0, 0, 50, 20}; U(ExtTextOutW)(dc, 5, 5, ETO_CLIPPED, &r, L"Hi", 2, NULL); }
RT(exttextout, EMR_EXTTEXTOUTW)
static void sc_exttextout_a(HDC dc) { U(ExtTextOutA)(dc, 5, 5, 0, NULL, "Hi", 2, NULL); }
RT(exttextout_a, EMR_EXTTEXTOUTW)
static void sc_polytextout(HDC dc) {
    POLYTEXTW pt = { 5, 5, 2, L"Hi", 0, {0, 0, 0, 0}, NULL };
    U(PolyTextOutW)(dc, &pt, 1);
}
RTS(polytextout, EMR_POLYTEXTOUTW)
static void sc_textout_empty(HDC dc) { U(TextOutW)(dc, 5, 5, L"", 0); }
RTS(textout_empty, EMR_EXTTEXTOUTW)
static void sc_font(HDC dc) {
    LOGFONTW lf; memset(&lf, 0, sizeof lf); lf.lfHeight = -12; lstrcpyW(lf.lfFaceName, L"Arial");
    HFONT f = U(CreateFontIndirectW)(&lf); HGDIOBJ o = U(SelectObject)(dc, f);
    U(TextOutW)(dc, 1, 1, L"x", 1);
    U(SelectObject)(dc, o); U(DeleteObject)(f);
}
RT(font, EMR_EXTCREATEFONTINDIRECTW, EMR_SELECTOBJECT, EMR_EXTTEXTOUTW, EMR_SELECTOBJECT, EMR_DELETEOBJECT)

/* ---- objects ---- */
static void sc_pen(HDC dc) {
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(1, 2, 3)); HGDIOBJ o = U(SelectObject)(dc, p);
    U(Rectangle)(dc, 1, 1, 9, 9);
    U(SelectObject)(dc, o); U(DeleteObject)(p);
}
RT(pen, EMR_CREATEPEN, EMR_SELECTOBJECT, EMR_RECTANGLE, EMR_SELECTOBJECT, EMR_DELETEOBJECT)
static void sc_extpen(HDC dc) {
    LOGBRUSH lb = { BS_SOLID, RGB(1, 2, 3), 0 };
    HPEN p = U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID, 5, &lb, 0, NULL); HGDIOBJ o = U(SelectObject)(dc, p);
    U(MoveToEx)(dc, 1, 1, NULL); U(LineTo)(dc, 9, 9);
    U(SelectObject)(dc, o); U(DeleteObject)(p);
}
RT(extpen, EMR_EXTCREATEPEN, EMR_SELECTOBJECT, EMR_LINETO, EMR_SELECTOBJECT, EMR_DELETEOBJECT)
static void sc_brush(HDC dc) {
    LOGBRUSH lb = { BS_SOLID, RGB(9, 9, 9), 0 };
    HBRUSH b = U(CreateBrushIndirect)(&lb); HGDIOBJ o = U(SelectObject)(dc, b);
    U(Rectangle)(dc, 1, 1, 9, 9);
    U(SelectObject)(dc, o); U(DeleteObject)(b);
}
RT(brush, EMR_CREATEBRUSHINDIRECT, EMR_SELECTOBJECT, EMR_RECTANGLE, EMR_SELECTOBJECT, EMR_DELETEOBJECT)
static void sc_hatch(HDC dc) {
    HBRUSH b = U(CreateHatchBrush)(HS_CROSS, RGB(9, 9, 9)); HGDIOBJ o = U(SelectObject)(dc, b);
    U(Rectangle)(dc, 1, 1, 9, 9); U(SelectObject)(dc, o); U(DeleteObject)(b);
}
RT(hatch, EMR_CREATEBRUSHINDIRECT, EMR_SELECTOBJECT)
static void sc_dibpattern(HDC dc) {
    struct { BITMAPINFOHEADER h; DWORD px[64]; } bi; memset(&bi, 0, sizeof bi);
    bi.h.biSize = sizeof bi.h; bi.h.biWidth = 8; bi.h.biHeight = 8; bi.h.biPlanes = 1; bi.h.biBitCount = 32;
    HBRUSH b = U(CreateDIBPatternBrushPt)(&bi, DIB_RGB_COLORS); HGDIOBJ o = U(SelectObject)(dc, b);
    U(Rectangle)(dc, 1, 1, 9, 9); U(SelectObject)(dc, o); U(DeleteObject)(b);
}
RT(dibpattern, EMR_CREATEDIBPATTERNBRUSHPT, EMR_SELECTOBJECT)
static void sc_monobrush(HDC dc) {
    WORD bits[8] = { 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55 };
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, bits); HBRUSH b = U(CreatePatternBrush)(bm);
    HGDIOBJ o = U(SelectObject)(dc, b); U(Rectangle)(dc, 1, 1, 9, 9);
    U(SelectObject)(dc, o); U(DeleteObject)(b); U(DeleteObject)(bm);
}
RT(monobrush, EMR_CREATEMONOBRUSH, EMR_SELECTOBJECT)
static void sc_stock(HDC dc) { U(SelectObject)(dc, U(GetStockObject)(GRAY_BRUSH)); U(SelectObject)(dc, U(GetStockObject)(NULL_PEN)); }
RT(stock, EMR_SELECTOBJECT, EMR_SELECTOBJECT)
static void sc_palette(HDC dc) {
    struct { WORD v, n; PALETTEENTRY e[2]; } lp = { 0x300, 2, { {1, 2, 3, 0}, {4, 5, 6, 0} } };
    HPALETTE p = U(CreatePalette)((LOGPALETTE *)&lp);
    HPALETTE o = U(SelectPalette)(dc, p, FALSE); U(RealizePalette)(dc);
    U(SelectPalette)(dc, o, FALSE); U(DeleteObject)(p);
}
RT(palette, EMR_CREATEPALETTE, EMR_SELECTPALETTE, EMR_REALIZEPALETTE, EMR_SELECTPALETTE, EMR_DELETEOBJECT)
static void sc_delete_unselected(HDC dc) {
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0); HGDIOBJ o = U(SelectObject)(dc, p); U(SelectObject)(dc, o); U(DeleteObject)(p);
}
RT(delete_unselected, EMR_CREATEPEN, EMR_SELECTOBJECT, EMR_SELECTOBJECT, EMR_DELETEOBJECT)

/* ---- blits ---- */
static void sc_patblt(HDC dc) { U(PatBlt)(dc, 1, 2, 30, 40, PATCOPY); }
RT(patblt, EMR_BITBLT)
static void sc_bitblt_mem(HDC dc) {
    HDC m = U(CreateCompatibleDC)(NULL); HBITMAP b = U(CreateCompatibleBitmap)(m, 16, 16);
    HGDIOBJ o = U(SelectObject)(m, b);
    U(BitBlt)(dc, 0, 0, 16, 16, m, 0, 0, SRCCOPY);
    U(SelectObject)(m, o); U(DeleteObject)(b); U(DeleteDC)(m);
}
RT(bitblt_mem, EMR_BITBLT)
static void sc_stretchblt(HDC dc) {
    HDC m = U(CreateCompatibleDC)(NULL); HBITMAP b = U(CreateBitmap)(8, 8, 1, 32, NULL);
    HGDIOBJ o = U(SelectObject)(m, b);
    U(StretchBlt)(dc, 0, 0, 32, 32, m, 0, 0, 8, 8, SRCCOPY);
    U(SelectObject)(m, o); U(DeleteObject)(b); U(DeleteDC)(m);
}
RT(stretchblt, EMR_STRETCHBLT)
static void sc_maskblt(HDC dc) {
    HDC m = U(CreateCompatibleDC)(NULL); HBITMAP b = U(CreateBitmap)(8, 8, 1, 32, NULL), mk = U(CreateBitmap)(8, 8, 1, 1, NULL);
    HGDIOBJ o = U(SelectObject)(m, b);
    U(MaskBlt)(dc, 0, 0, 8, 8, m, 0, 0, mk, 0, 0, MAKEROP4(0xAA0029, SRCCOPY));
    U(SelectObject)(m, o); U(DeleteObject)(b); U(DeleteObject)(mk); U(DeleteDC)(m);
}
RT(maskblt, EMR_MASKBLT)
static void sc_plgblt(HDC dc) {
    HDC m = U(CreateCompatibleDC)(NULL); HBITMAP b = U(CreateBitmap)(8, 8, 1, 32, NULL);
    HGDIOBJ o = U(SelectObject)(m, b); POINT p[3] = { {10, 0}, {30, 5}, {5, 20} };
    U(PlgBlt)(dc, p, m, 0, 0, 8, 8, NULL, 0, 0);
    U(SelectObject)(m, o); U(DeleteObject)(b); U(DeleteDC)(m);
}
RT(plgblt, EMR_PLGBLT)
static void sc_stretchdibits(HDC dc) {
    BITMAPINFO bi; DWORD px[16]; memset(&bi, 0, sizeof bi); memset(px, 0x40, sizeof px);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 4; bi.bmiHeader.biHeight = 4;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    U(StretchDIBits)(dc, 0, 0, 8, 8, 0, 0, 4, 4, px, &bi, DIB_RGB_COLORS, SRCCOPY);
}
RT(stretchdibits, EMR_STRETCHDIBITS)
static void sc_setdibtodev(HDC dc) {
    BITMAPINFO bi; DWORD px[16]; memset(&bi, 0, sizeof bi); memset(px, 0x40, sizeof px);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 4; bi.bmiHeader.biHeight = 4;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    U(SetDIBitsToDevice)(dc, 0, 0, 4, 4, 0, 0, 0, 4, px, &bi, DIB_RGB_COLORS);
}
RT(setdibtodev, EMR_SETDIBITSTODEVICE)
static void sc_alphablend(HDC dc) {
    HDC m = U(CreateCompatibleDC)(NULL); HBITMAP b = U(CreateBitmap)(8, 8, 1, 32, NULL); HGDIOBJ o = U(SelectObject)(m, b);
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 128, 0 };
    U(GdiAlphaBlend)(dc, 0, 0, 8, 8, m, 0, 0, 8, 8, bf);
    U(SelectObject)(m, o); U(DeleteObject)(b); U(DeleteDC)(m);
}
RTS(alphablend, EMR_ALPHABLEND)
static void sc_transparentblt(HDC dc) {
    HDC m = U(CreateCompatibleDC)(NULL); HBITMAP b = U(CreateBitmap)(8, 8, 1, 32, NULL); HGDIOBJ o = U(SelectObject)(m, b);
    U(GdiTransparentBlt)(dc, 0, 0, 8, 8, m, 0, 0, 8, 8, 0);
    U(SelectObject)(m, o); U(DeleteObject)(b); U(DeleteDC)(m);
}
RTS(transparentblt, EMR_TRANSPARENTBLT)

/* ---- comments ---- */
static void sc_comment(HDC dc) { static const BYTE d[8] = "ABCDEFG"; U(GdiComment)(dc, 8, d); }
RT(comment, EMR_GDICOMMENT)
static void sc_comment_many(HDC dc) {
    static const BYTE d[5] = "1234";
    for (int i = 1; i <= 5; i++) U(GdiComment)(dc, i, d);
}
RT(comment_many, EMR_GDICOMMENT, EMR_GDICOMMENT, EMR_GDICOMMENT, EMR_GDICOMMENT, EMR_GDICOMMENT)

/* ---- state-heavy combos ---- */
static void sc_everything(HDC dc) {
    sc_pen(dc); sc_brush(dc); sc_font(dc); sc_path_fill(dc); sc_polygon16(dc); sc_selectcliprgn(dc); sc_bitblt_mem(dc);
}
RT(everything, EMR_CREATEPEN, EMR_CREATEBRUSHINDIRECT, EMR_EXTCREATEFONTINDIRECTW, EMR_BEGINPATH, EMR_POLYGON16, EMR_EXTSELECTCLIPRGN, EMR_BITBLT)
static void sc_nothing(HDC dc) { (void)dc; }
RT(nothing, EMR_HEADER, EMR_EOF)
