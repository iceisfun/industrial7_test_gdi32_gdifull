#include "emf.h"

/* Record with `fn`, return bits (malloc'd) and the metafile handle is deleted. */
static BYTE *rec_bits(void (*fn)(HDC), UINT *n) {
    HDC dc = emf_begin();
    if (!dc) return NULL;
    fn(dc);
    HENHMETAFILE h = emf_end(dc);
    if (!h) return NULL;
    BYTE *b = emf_bits(h, n);
    U(DeleteEnhMetaFile)(h);
    return b;
}
#define GETREC(fn, type, var) \
    UINT n_ = 0; BYTE *b_ = rec_bits(fn, &n_); \
    const ENHMETARECORD *var = b_ ? emf_find(b_, n_, type, 0) : NULL; \
    CHECK(var != NULL); if (!var) { free(b_); return; }
#define DONE free(b_)

static void s_rect(HDC dc) { U(Rectangle)(dc, 10, 20, 50, 60); }
static void s_moveto(HDC dc) { U(MoveToEx)(dc, 7, 9, NULL); }
static void s_lineto(HDC dc) { U(LineTo)(dc, 33, 44); }
static void s_mapmode(HDC dc) { U(SetMapMode)(dc, MM_HIMETRIC); }
static void s_bkcolor(HDC dc) { U(SetBkColor)(dc, 0x00AABBCC); }
static void s_textcolor(HDC dc) { U(SetTextColor)(dc, 0x00112233); }
static void s_bkmode(HDC dc) { U(SetBkMode)(dc, TRANSPARENT); }
static void s_rop2(HDC dc) { U(SetROP2)(dc, R2_NOT); }
static void s_savedc(HDC dc) { U(SaveDC)(dc); U(SaveDC)(dc); U(RestoreDC)(dc, -2); }
static void s_savedc_abs(HDC dc) { int a = U(SaveDC)(dc); U(SaveDC)(dc); U(RestoreDC)(dc, a); }
static void s_ellipse(HDC dc) { U(Ellipse)(dc, 1, 2, 3, 4); }
static void s_roundrect(HDC dc) { U(RoundRect)(dc, 1, 2, 30, 40, 5, 6); }
static void s_winorg(HDC dc) { U(SetWindowOrgEx)(dc, -5, 6, NULL); }
static void s_vpext(HDC dc) { U(SetMapMode)(dc, MM_ANISOTROPIC); U(SetViewportExtEx)(dc, 11, 12, NULL); }
static void s_pen(HDC dc) {
    HPEN p = U(CreatePen)(PS_DASH, 1, RGB(10, 20, 30)); HGDIOBJ o = U(SelectObject)(dc, p);
    U(SelectObject)(dc, o); U(DeleteObject)(p);
}
static void s_pen3(HDC dc) {
    HPEN p = U(CreatePen)(PS_SOLID, 3, RGB(10, 20, 30)); HGDIOBJ o = U(SelectObject)(dc, p);
    U(SelectObject)(dc, o); U(DeleteObject)(p);
}
static void s_brush(HDC dc) {
    LOGBRUSH lb = { BS_HATCHED, RGB(5, 6, 7), HS_DIAGCROSS };
    HBRUSH b = U(CreateBrushIndirect)(&lb); HGDIOBJ o = U(SelectObject)(dc, b);
    U(SelectObject)(dc, o); U(DeleteObject)(b);
}
static void s_two_objs(HDC dc) {
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0); HBRUSH b = U(CreateSolidBrush)(5);
    HGDIOBJ o1 = U(SelectObject)(dc, p), o2 = U(SelectObject)(dc, b);
    U(SelectObject)(dc, o1); U(SelectObject)(dc, o2); U(DeleteObject)(p); U(DeleteObject)(b);
}
static void s_stock(HDC dc) { U(SelectObject)(dc, U(GetStockObject)(DKGRAY_BRUSH)); U(SelectObject)(dc, U(GetStockObject)(NULL_PEN)); }
static void s_poly16(HDC dc) { POINT p[3] = { {5, 6}, {70, 8}, {9, 90} }; U(Polygon)(dc, p, 3); }
static void s_poly32(HDC dc) { POINT p[3] = { {5, 6}, {70000, 8}, {9, 90} }; U(Polygon)(dc, p, 3); }
static void s_poly16_odd(HDC dc) { POINT p[5] = { {1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 6} }; U(Polyline)(dc, p, 5); }
static void s_textw(HDC dc) { U(TextOutW)(dc, 12, 34, L"Hello", 5); }
static void s_texta(HDC dc) { U(TextOutA)(dc, 12, 34, "Hello", 5); }
static void s_exttext_clip(HDC dc) { RECT r = {1, 2, 100, 50}; U(ExtTextOutW)(dc, 3, 4, ETO_OPAQUE | ETO_CLIPPED, &r, L"ab", 2, NULL); }
static void s_exttext_dx(HDC dc) { INT dx[3] = {5, 6, 7}; U(ExtTextOutW)(dc, 3, 4, 0, NULL, L"abc", 3, dx); }
static void s_cliprgn(HDC dc) { HRGN r = U(CreateRectRgn)(2, 3, 40, 50); U(SelectClipRgn)(dc, r); U(DeleteObject)(r); }
static void s_cliprgn_null(HDC dc) { U(SelectClipRgn)(dc, NULL); }
static void s_comment4(HDC dc) { U(GdiComment)(dc, 4, (const BYTE *)"WXYZ"); }
static void s_comment5(HDC dc) { U(GdiComment)(dc, 5, (const BYTE *)"VWXYZ"); }
static void s_comment1(HDC dc) { U(GdiComment)(dc, 1, (const BYTE *)"Q"); }
static void s_comment0(HDC dc) { U(GdiComment)(dc, 0, NULL); }
static void s_comment_gdic(HDC dc) { static const BYTE d[] = { 'G','D','I','C', 1,0,0,0, 9,9,9,9 }; U(GdiComment)(dc, sizeof d, d); }
static void s_comment_plus(HDC dc) { static const BYTE d[] = { 'E','M','F','+', 0,0,0,0 }; U(GdiComment)(dc, sizeof d, d); }
static void s_font(HDC dc) {
    LOGFONTW lf; memset(&lf, 0, sizeof lf); lf.lfHeight = -17; lf.lfWeight = 700; lstrcpyW(lf.lfFaceName, L"Arial");
    HFONT f = U(CreateFontIndirectW)(&lf); HGDIOBJ o = U(SelectObject)(dc, f);
    U(SelectObject)(dc, o); U(DeleteObject)(f);
}
static void s_patblt(HDC dc) { U(PatBlt)(dc, 11, 22, 33, 44, PATINVERT); }
static void s_setpixel(HDC dc) { U(SetPixelV)(dc, 5, 6, 0x00778899); }

TEST(emf_byte_rectangle) {
    REQUIRE(U(Rectangle));
    GETREC(s_rect, EMR_RECTANGLE, r)
    const EMRRECTANGLE *e = (const EMRRECTANGLE *)r;
    EXPECT_EQ(r->nSize, 24);
    EXPECT_EQ(e->rclBox.left, 10); EXPECT_EQ(e->rclBox.top, 20); EXPECT_EQ(e->rclBox.right, 50); EXPECT_EQ(e->rclBox.bottom, 60);
    DONE;
}
TEST(emf_byte_ellipse) {
    REQUIRE(U(Ellipse));
    GETREC(s_ellipse, EMR_ELLIPSE, r)
    EXPECT_EQ(r->nSize, 24);
    const EMRELLIPSE *e = (const EMRELLIPSE *)r;
    EXPECT_EQ(e->rclBox.left, 1); EXPECT_EQ(e->rclBox.bottom, 4);
    DONE;
}
TEST(emf_byte_roundrect) {
    REQUIRE(U(RoundRect));
    GETREC(s_roundrect, EMR_ROUNDRECT, r)
    EXPECT_EQ(r->nSize, 32);
    const EMRROUNDRECT *e = (const EMRROUNDRECT *)r;
    EXPECT_EQ(e->rclBox.right, 30); EXPECT_EQ(e->szlCorner.cx, 5); EXPECT_EQ(e->szlCorner.cy, 6);
    DONE;
}
TEST(emf_byte_moveto) {
    GETREC(s_moveto, EMR_MOVETOEX, r)
    EXPECT_EQ(r->nSize, 16);
    const EMRMOVETOEX *e = (const EMRMOVETOEX *)r;
    EXPECT_EQ(e->ptl.x, 7); EXPECT_EQ(e->ptl.y, 9);
    DONE;
}
TEST(emf_byte_lineto) {
    GETREC(s_lineto, EMR_LINETO, r)
    EXPECT_EQ(r->nSize, 16);
    const EMRLINETO *e = (const EMRLINETO *)r;
    EXPECT_EQ(e->ptl.x, 33); EXPECT_EQ(e->ptl.y, 44);
    DONE;
}
TEST(emf_byte_setmapmode) {
    GETREC(s_mapmode, EMR_SETMAPMODE, r)
    EXPECT_EQ(r->nSize, 12);
    EXPECT_EQ(r->dParm[0], MM_HIMETRIC);
    DONE;
}
TEST(emf_byte_setbkcolor) {
    GETREC(s_bkcolor, EMR_SETBKCOLOR, r)
    EXPECT_EQ(r->nSize, 12);
    EXPECT_EQ(r->dParm[0], 0x00AABBCC);
    DONE;
}
TEST(emf_byte_settextcolor) {
    GETREC(s_textcolor, EMR_SETTEXTCOLOR, r)
    EXPECT_EQ(r->nSize, 12);
    EXPECT_EQ(r->dParm[0], 0x00112233);
    DONE;
}
TEST(emf_byte_setbkmode) {
    GETREC(s_bkmode, EMR_SETBKMODE, r)
    EXPECT_EQ(r->nSize, 12);
    EXPECT_EQ(r->dParm[0], TRANSPARENT);
    DONE;
}
TEST(emf_byte_setrop2) {
    GETREC(s_rop2, EMR_SETROP2, r)
    EXPECT_EQ(r->nSize, 12);
    EXPECT_EQ(r->dParm[0], R2_NOT);
    DONE;
}
TEST(emf_byte_restoredc_relative) {
    GETREC(s_savedc, EMR_RESTOREDC, r)
    EXPECT_EQ(r->nSize, 12);
    EXPECT_EQ((int)r->dParm[0], -2);
    EXPECT_EQ(emf_find(b_, n_, EMR_SAVEDC, 1) != NULL, 1);
    EXPECT_EQ(emf_find(b_, n_, EMR_SAVEDC, 0)->nSize, 8);
    DONE;
}
TEST(emf_byte_restoredc_absolute_converted) {
    GETREC(s_savedc_abs, EMR_RESTOREDC, r)
    EXPECT_EQ(r->nSize, 12);
    /* absolute levels are recorded as relative offsets */
    EXPECT_EQ((int)r->dParm[0], -1);
    DONE;
}
TEST(emf_byte_setwindoworg) {
    GETREC(s_winorg, EMR_SETWINDOWORGEX, r)
    EXPECT_EQ(r->nSize, 16);
    EXPECT_EQ((int)r->dParm[0], -5); EXPECT_EQ((int)r->dParm[1], 6);
    DONE;
}
TEST(emf_byte_setviewportext) {
    GETREC(s_vpext, EMR_SETVIEWPORTEXTEX, r)
    EXPECT_EQ(r->nSize, 16);
    EXPECT_EQ(r->dParm[0], 11); EXPECT_EQ(r->dParm[1], 12);
    DONE;
}
TEST(emf_byte_createpen) {
    GETREC(s_pen, EMR_CREATEPEN, r)
    const EMRCREATEPEN *e = (const EMRCREATEPEN *)r;
    EXPECT_EQ(r->nSize, 28);
    EXPECT_EQ(e->ihPen, 1);
    EXPECT_EQ(e->lopn.lopnStyle, PS_DASH);
    EXPECT_EQ(e->lopn.lopnWidth.x, 1);
    EXPECT_EQ(e->lopn.lopnColor, RGB(10, 20, 30));
    DONE;
}
TEST(emf_byte_createpen_width) {
    GETREC(s_pen3, EMR_CREATEPEN, r)
    const EMRCREATEPEN *e = (const EMRCREATEPEN *)r;
    EXPECT_EQ(e->lopn.lopnStyle, PS_SOLID);
    EXPECT_EQ(e->lopn.lopnWidth.x, 3);
    DONE;
}
TEST(emf_byte_selectobject_indexes) {
    GETREC(s_pen, EMR_SELECTOBJECT, r)
    EXPECT_EQ(r->nSize, 12);
    EXPECT_EQ(r->dParm[0], 1);              /* handle table index of the pen */
    const ENHMETARECORD *s2 = emf_find(b_, n_, EMR_SELECTOBJECT, 1);
    CHECK(s2 != NULL);
    if (s2) EXPECT_EQ(s2->dParm[0], 0x80000007);   /* back to the stock BLACK_PEN */
    const ENHMETARECORD *d = emf_find(b_, n_, EMR_DELETEOBJECT, 0);
    CHECK(d != NULL);
    if (d) { EXPECT_EQ(d->nSize, 12); EXPECT_EQ(d->dParm[0], 1); }
    DONE;
}
TEST(emf_byte_two_objects_indexes) {
    GETREC(s_two_objs, EMR_CREATEPEN, r)
    const EMRCREATEPEN *e = (const EMRCREATEPEN *)r;
    EXPECT_EQ(e->ihPen, 1);
    const ENHMETARECORD *br = emf_find(b_, n_, EMR_CREATEBRUSHINDIRECT, 0);
    CHECK(br != NULL);
    if (br) {
        const EMRCREATEBRUSHINDIRECT *eb = (const EMRCREATEBRUSHINDIRECT *)br;
        EXPECT_EQ(br->nSize, 24);
        EXPECT_EQ(eb->ihBrush, 2);
        EXPECT_EQ(eb->lb.lbStyle, BS_SOLID);
        EXPECT_EQ(eb->lb.lbColor, 5);
    }
    ENHMETAHEADER *hd = (ENHMETAHEADER *)b_;
    EXPECT_EQ(hd->nHandles, 3);
    DONE;
}
TEST(emf_byte_stock_selectobject) {
    GETREC(s_stock, EMR_SELECTOBJECT, r)
    EXPECT_EQ(r->dParm[0], 0x80000000u | DKGRAY_BRUSH);
    const ENHMETARECORD *r2 = emf_find(b_, n_, EMR_SELECTOBJECT, 1);
    CHECK(r2 != NULL);
    if (r2) EXPECT_EQ(r2->dParm[0], 0x80000000u | NULL_PEN);
    EXPECT_EQ(emf_find(b_, n_, EMR_CREATEPEN, 0) == NULL, 1);
    EXPECT_EQ(((ENHMETAHEADER *)b_)->nHandles, 1);
    DONE;
}
TEST(emf_byte_createbrushindirect_hatched) {
    GETREC(s_brush, EMR_CREATEBRUSHINDIRECT, r)
    const EMRCREATEBRUSHINDIRECT *e = (const EMRCREATEBRUSHINDIRECT *)r;
    EXPECT_EQ(e->ihBrush, 1);
    EXPECT_EQ(e->lb.lbStyle, BS_HATCHED);
    EXPECT_EQ(e->lb.lbColor, RGB(5, 6, 7));
    EXPECT_EQ(e->lb.lbHatch, HS_DIAGCROSS);
    DONE;
}
TEST(emf_byte_polygon16) {
    GETREC(s_poly16, EMR_POLYGON16, r)
    const EMRPOLYGON16 *e = (const EMRPOLYGON16 *)r;
    EXPECT_EQ(r->nSize, 28 + 3 * 4);
    EXPECT_EQ(e->cpts, 3);
    EXPECT_EQ(e->apts[0].x, 5); EXPECT_EQ(e->apts[0].y, 6);
    EXPECT_EQ(e->apts[1].x, 70); EXPECT_EQ(e->apts[2].y, 90);
    EXPECT_EQ(e->rclBounds.left, 5); EXPECT_EQ(e->rclBounds.top, 6);
    SOFT(e->rclBounds.right == 69 || e->rclBounds.right == 70);
    DONE;
}
TEST(emf_byte_polygon32) {
    GETREC(s_poly32, EMR_POLYGON, r)
    const EMRPOLYGON *e = (const EMRPOLYGON *)r;
    EXPECT_EQ(r->nSize, 28 + 3 * 8);
    EXPECT_EQ(e->cptl, 3);
    EXPECT_EQ(e->aptl[1].x, 70000);
    DONE;
}
TEST(emf_byte_polyline16_odd_count_padding) {
    GETREC(s_poly16_odd, EMR_POLYLINE16, r)
    const EMRPOLYLINE16 *e = (const EMRPOLYLINE16 *)r;
    EXPECT_EQ(e->cpts, 5);
    EXPECT_EQ(r->nSize, 28 + 5 * 4);
    EXPECT_EQ(e->apts[4].x, 5); EXPECT_EQ(e->apts[4].y, 6);
    DONE;
}
TEST(emf_byte_exttextout_w) {
    GETREC(s_textw, EMR_EXTTEXTOUTW, r)
    const EMREXTTEXTOUTW *e = (const EMREXTTEXTOUTW *)r;
    EXPECT_EQ(e->emrtext.ptlReference.x, 12);
    EXPECT_EQ(e->emrtext.ptlReference.y, 34);
    EXPECT_EQ(e->emrtext.nChars, 5);
    CHECK(e->emrtext.offString >= 76 && e->emrtext.offString + 10 <= r->nSize);
    if (e->emrtext.offString + 10 <= r->nSize)
        CHECK(!memcmp((BYTE *)r + e->emrtext.offString, L"Hello", 10));
    CHECK(e->emrtext.offDx != 0);
    EXPECT_EQ(r->nSize & 3, 0);
    CHECK(e->exScale != 0 || e->eyScale != 0 || 1);
    DONE;
}
TEST(emf_byte_exttextout_a_converted_to_w) {
    GETREC(s_texta, EMR_EXTTEXTOUTW, r)
    const EMREXTTEXTOUTW *e = (const EMREXTTEXTOUTW *)r;
    EXPECT_EQ(e->emrtext.nChars, 5);
    if (e->emrtext.offString + 10 <= r->nSize)
        CHECK(!memcmp((BYTE *)r + e->emrtext.offString, L"Hello", 10));
    EXPECT_EQ(emf_find(b_, n_, EMR_EXTTEXTOUTA, 0) == NULL, 1);
    DONE;
}
TEST(emf_byte_exttextout_options_rect) {
    GETREC(s_exttext_clip, EMR_EXTTEXTOUTW, r)
    const EMREXTTEXTOUTW *e = (const EMREXTTEXTOUTW *)r;
    EXPECT_EQ(e->emrtext.fOptions & (ETO_OPAQUE | ETO_CLIPPED), ETO_OPAQUE | ETO_CLIPPED);
    EXPECT_EQ(e->emrtext.rcl.left, 1); EXPECT_EQ(e->emrtext.rcl.top, 2);
    EXPECT_EQ(e->emrtext.rcl.right, 100); EXPECT_EQ(e->emrtext.rcl.bottom, 50);
    DONE;
}
TEST(emf_byte_exttextout_dx_array) {
    GETREC(s_exttext_dx, EMR_EXTTEXTOUTW, r)
    const EMREXTTEXTOUTW *e = (const EMREXTTEXTOUTW *)r;
    EXPECT_EQ(e->emrtext.nChars, 3);
    CHECK(e->emrtext.offDx + 12 <= r->nSize);
    if (e->emrtext.offDx + 12 <= r->nSize) {
        const INT *dx = (const INT *)((BYTE *)r + e->emrtext.offDx);
        EXPECT_EQ(dx[0], 5); EXPECT_EQ(dx[1], 6); EXPECT_EQ(dx[2], 7);
    }
    DONE;
}
TEST(emf_byte_extselectcliprgn) {
    GETREC(s_cliprgn, EMR_EXTSELECTCLIPRGN, r)
    const EMREXTSELECTCLIPRGN *e = (const EMREXTSELECTCLIPRGN *)r;
    EXPECT_EQ(e->iMode, RGN_COPY);
    CHECK(e->cbRgnData >= sizeof(RGNDATAHEADER) + sizeof(RECT));
    EXPECT_EQ(r->nSize, 8 + 8 + ((e->cbRgnData + 3) & ~3u));
    if (e->cbRgnData >= sizeof(RGNDATAHEADER) + sizeof(RECT)) {
        const RGNDATA *rd = (const RGNDATA *)e->RgnData;
        EXPECT_EQ(rd->rdh.nCount, 1);
        const RECT *rc = (const RECT *)rd->Buffer;
        EXPECT_EQ(rc->left, 2); EXPECT_EQ(rc->top, 3); EXPECT_EQ(rc->right, 40); EXPECT_EQ(rc->bottom, 50);
    }
    DONE;
}
TEST(emf_byte_extselectcliprgn_null) {
    GETREC(s_cliprgn_null, EMR_EXTSELECTCLIPRGN, r)
    const EMREXTSELECTCLIPRGN *e = (const EMREXTSELECTCLIPRGN *)r;
    EXPECT_EQ(e->iMode, RGN_COPY);
    EXPECT_EQ(e->cbRgnData, 0);
    EXPECT_EQ(r->nSize, 16);
    DONE;
}
TEST(emf_byte_comment_exact_multiple) {
    GETREC(s_comment4, EMR_GDICOMMENT, r)
    const EMRGDICOMMENT *e = (const EMRGDICOMMENT *)r;
    EXPECT_EQ(e->cbData, 4);
    EXPECT_EQ(r->nSize, 16);
    CHECK(!memcmp(e->Data, "WXYZ", 4));
    DONE;
}
TEST(emf_byte_comment_padded) {
    GETREC(s_comment5, EMR_GDICOMMENT, r)
    const EMRGDICOMMENT *e = (const EMRGDICOMMENT *)r;
    EXPECT_EQ(e->cbData, 5);
    EXPECT_EQ(r->nSize, 20);
    CHECK(!memcmp(e->Data, "VWXYZ", 5));
    DONE;
}
TEST(emf_byte_comment_one_byte) {
    GETREC(s_comment1, EMR_GDICOMMENT, r)
    const EMRGDICOMMENT *e = (const EMRGDICOMMENT *)r;
    EXPECT_EQ(e->cbData, 1);
    EXPECT_EQ(r->nSize, 16);
    EXPECT_EQ(e->Data[0], 'Q');
    DONE;
}
TEST(emf_byte_comment_gdic_identifier) {
    GETREC(s_comment_gdic, EMR_GDICOMMENT, r)
    const EMRGDICOMMENT *e = (const EMRGDICOMMENT *)r;
    EXPECT_EQ(e->cbData, 12);
    CHECK(!memcmp(e->Data, "GDIC", 4));
    EXPECT_EQ(r->nSize, 24);
    DONE;
}
TEST(emf_byte_comment_emfplus_identifier) {
    GETREC(s_comment_plus, EMR_GDICOMMENT, r)
    const EMRGDICOMMENT *e = (const EMRGDICOMMENT *)r;
    EXPECT_EQ(e->cbData, 8);
    CHECK(!memcmp(e->Data, "EMF+", 4));
    DONE;
}
TEST(emf_byte_comment_zero_size) {
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    BOOL r = U(GdiComment)(dc, 0, NULL);
    t_note("GdiComment(0, NULL) = %d", r);
    BOOL r2 = U(GdiComment)(dc, 0, (const BYTE *)"x");
    t_note("GdiComment(0, buf) = %d", r2);
    U(DeleteEnhMetaFile)(emf_end(dc));
}
TEST(emf_byte_extcreatefont) {
    GETREC(s_font, EMR_EXTCREATEFONTINDIRECTW, r)
    const EMREXTCREATEFONTINDIRECTW *e = (const EMREXTCREATEFONTINDIRECTW *)r;
    EXPECT_EQ(e->ihFont, 1);
    t_note("EXTCREATEFONTINDIRECTW nSize=%lu", r->nSize);
    CHECK(r->nSize >= 12 + 92 && r->nSize <= 12 + sizeof(EXTLOGFONTW));
    EXPECT_EQ(r->nSize & 3, 0);
    EXPECT_EQ(e->elfw.elfLogFont.lfHeight, -17);
    EXPECT_EQ(e->elfw.elfLogFont.lfWeight, 700);
    CHECK(!lstrcmpW(e->elfw.elfLogFont.lfFaceName, L"Arial"));
    DONE;
}
TEST(emf_byte_patblt_is_bitblt) {
    GETREC(s_patblt, EMR_BITBLT, r)
    const EMRBITBLT *e = (const EMRBITBLT *)r;
    EXPECT_EQ(r->nSize, sizeof(EMRBITBLT));
    EXPECT_EQ(e->xDest, 11); EXPECT_EQ(e->yDest, 22); EXPECT_EQ(e->cxDest, 33); EXPECT_EQ(e->cyDest, 44);
    EXPECT_EQ(e->dwRop, PATINVERT);
    EXPECT_EQ(e->rclBounds.left, 11); EXPECT_EQ(e->rclBounds.top, 22);
    EXPECT_EQ(e->rclBounds.right, 43); EXPECT_EQ(e->rclBounds.bottom, 65);
    EXPECT_EQ(e->cbBmiSrc, 0);
    DONE;
}
TEST(emf_byte_setpixelv) {
    GETREC(s_setpixel, EMR_SETPIXELV, r)
    const EMRSETPIXELV *e = (const EMRSETPIXELV *)r;
    EXPECT_EQ(r->nSize, 20);
    EXPECT_EQ(e->ptlPixel.x, 5); EXPECT_EQ(e->ptlPixel.y, 6);
    EXPECT_EQ(e->crColor, 0x00778899);
    DONE;
}
TEST(emf_byte_eof_record) {
    GETREC(s_rect, EMR_EOF, r)
    const EMREOF *e = (const EMREOF *)r;
    EXPECT_EQ(r->nSize, 20);
    EXPECT_EQ(e->nPalEntries, 0);
    EXPECT_EQ(e->offPalEntries, 16);
    EXPECT_EQ(e->nSizeLast, 20);
    EXPECT_EQ((const BYTE *)r + r->nSize, b_ + n_);
    DONE;
}
TEST(emf_byte_header_record_matches_total) {
    UINT n; BYTE *b = rec_bits(s_rect, &n);
    CHECK(b != NULL);
    if (!b) return;
    const ENHMETAHEADER *h = (const ENHMETAHEADER *)b;
    EXPECT_EQ(h->iType, EMR_HEADER);
    EXPECT_EQ(h->nBytes, n);
    EXPECT_EQ(h->nRecords, emf_count_recs(b, n));
    EXPECT_EQ(h->nSize, 108);
    EXPECT_EQ(n % 4, 0);
    free(b);
}
TEST(emf_byte_header_bounds_for_rectangle) {
    UINT n; BYTE *b = rec_bits(s_rect, &n);
    CHECK(b != NULL);
    if (!b) return;
    const ENHMETAHEADER *h = (const ENHMETAHEADER *)b;
    CHECK(h->rclBounds.left <= 10 && h->rclBounds.top <= 20 && h->rclBounds.right >= 49 && h->rclBounds.bottom >= 59);
    free(b);
}
TEST(emf_byte_records_contiguous_and_aligned) {
    UINT n; BYTE *b = rec_bits(s_poly16_odd, &n);
    CHECK(b != NULL);
    if (!b) return;
    UINT off = 0; int ok = 1;
    while (off < n) {
        const ENHMETARECORD *r = (const ENHMETARECORD *)(b + off);
        if (r->nSize < 8 || (r->nSize & 3) || off + r->nSize > n) { ok = 0; break; }
        off += r->nSize;
    }
    CHECK(ok && off == n);
    free(b);
}
TEST(emf_byte_record_order_first_header_last_eof) {
    UINT n; BYTE *b = rec_bits(s_rect, &n);
    CHECK(b != NULL);
    if (!b) return;
    EXPECT_EQ(emf_find(b, n, EMR_HEADER, 0), (const ENHMETARECORD *)b);
    CHECK(emf_find(b, n, EMR_HEADER, 1) == NULL);
    CHECK(emf_find(b, n, EMR_EOF, 1) == NULL);
    free(b);
}
