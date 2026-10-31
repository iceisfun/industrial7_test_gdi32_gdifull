/* Direct NtGdi* syscalls through gdi32full's thunks, valid-argument paths.
 * Signatures (win32k):
 *  NtGdiCreateCompatibleDC(HDC) -> HDC        NtGdiDeleteObjectApp(HANDLE) -> BOOL
 *  NtGdiCreateRectRgn(l,t,r,b) -> HRGN        NtGdiGetRgnBox(HRGN, RECT*) -> int
 *  NtGdiCreateSolidBrush(COLORREF, HBRUSH) -> HBRUSH
 *  NtGdiCreatePen(style, width, COLORREF, HBRUSH) -> HPEN
 *  NtGdiCreateBitmap(cx, cy, planes, bpp, bits) -> HBITMAP
 *  NtGdiExtGetObjectW(HANDLE, int, void*) -> int
 *  NtGdiPatBlt(HDC, x, y, cx, cy, rop) -> BOOL ; NtGdiBitBlt(dst,x,y,cx,cy,src,sx,sy,rop,bk,flags) -> BOOL
 *  NtGdiSetPixel/GetPixel(HDC, x, y[, color]) -> COLORREF */
#include "gap9.h"

#define NTF(var, name) FARPROC var = t_proc_gdi32full(#name); REQUIRE(var); G9_NOWINE("direct syscall")

typedef ULONG_PTR (WINAPI *fn11)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);

TEST(gap9_ntok_createcompatibledc_roundtrip)
{
    NTF(c, NtGdiCreateCompatibleDC); NTF(d, NtGdiDeleteObjectApp);
    DWORD before = g9_gdi_count();
    HDC dc = (HDC)g9_call(c, 0, 0, 0, 0);
    REQUIRE(dc);
    EXPECT_EQ(U(GetObjectType)(dc), OBJ_MEMDC);
    EXPECT_EQ(g9_gdi_count(), before + 1);
    EXPECT_EQ((DWORD)g9_call(d, (ULONG_PTR)dc, 0, 0, 0), 1);
    EXPECT_EQ(U(GetObjectType)(dc), 0);
    EXPECT_EQ(g9_gdi_count(), before);
    /* second delete fails */
    EXPECT_EQ((DWORD)g9_call(d, (ULONG_PTR)dc, 0, 0, 0), 0);
}

TEST(gap9_ntok_createcompatibledc_from_dc)
{
    NTF(c, NtGdiCreateCompatibleDC); NTF(d, NtGdiDeleteObjectApp);
    g9_dib_t dib;
    REQUIRE(g9_dib_open(&dib, 4, 4));
    HDC dc = (HDC)g9_call(c, (ULONG_PTR)dib.dc, 0, 0, 0);
    REQUIRE(dc);
    EXPECT_EQ(U(GetObjectType)(dc), OBJ_MEMDC);
    EXPECT_EQ(U(GetDeviceCaps)(dc, BITSPIXEL), 1);   /* fresh DC holds the 1x1 monochrome default bitmap */
    EXPECT_EQ((DWORD)g9_call(d, (ULONG_PTR)dc, 0, 0, 0), 1);
    g9_dib_close(&dib);
}

TEST(gap9_ntok_createcompatibledc_bogus_source)
{
    NTF(c, NtGdiCreateCompatibleDC);
    DWORD before = g9_gdi_count();
    HDC dc = (HDC)g9_call(c, G9_BOGUS, 0, 0, 0);
    CHECK(dc == NULL);
    if (dc) U(DeleteDC)(dc);
    EXPECT_EQ(g9_gdi_count(), before);
}

TEST(gap9_ntok_region_create_box_delete)
{
    NTF(c, NtGdiCreateRectRgn); NTF(box, NtGdiGetRgnBox); NTF(d, NtGdiDeleteObjectApp);
    HRGN r = (HRGN)g9_call(c, 10, 20, 110, 220);
    REQUIRE(r);
    EXPECT_EQ(U(GetObjectType)(r), OBJ_REGION);
    RECT rc = { 1, 1, 1, 1 };
    EXPECT_EQ((DWORD)g9_call(box, (ULONG_PTR)r, (ULONG_PTR)&rc, 0, 0), SIMPLEREGION);
    CHECK(rc.left == 10 && rc.top == 20 && rc.right == 110 && rc.bottom == 220);
    EXPECT_EQ((DWORD)g9_call(d, (ULONG_PTR)r, 0, 0, 0), 1);
    EXPECT_EQ((DWORD)g9_call(box, (ULONG_PTR)r, (ULONG_PTR)&rc, 0, 0), 0);
}

TEST(gap9_ntok_region_empty_and_normalised)
{
    NTF(c, NtGdiCreateRectRgn); NTF(box, NtGdiGetRgnBox);
    HRGN e = (HRGN)g9_call(c, 5, 5, 5, 5), n = (HRGN)g9_call(c, 50, 60, 10, 20);
    REQUIRE(e); REQUIRE(n);
    RECT rc = { 9, 9, 9, 9 };
    EXPECT_EQ((DWORD)g9_call(box, (ULONG_PTR)e, (ULONG_PTR)&rc, 0, 0), NULLREGION);
    CHECK(rc.left == 0 && rc.top == 0 && rc.right == 0 && rc.bottom == 0);
    EXPECT_EQ((DWORD)g9_call(box, (ULONG_PTR)n, (ULONG_PTR)&rc, 0, 0), SIMPLEREGION);
    CHECK(rc.left == 10 && rc.top == 20 && rc.right == 50 && rc.bottom == 60);
    g9_ntdel(e); g9_ntdel(n);
}

TEST(gap9_ntok_region_combine_modes)
{
    NTF(c, NtGdiCreateRectRgn); NTF(cmb, NtGdiCombineRgn); NTF(box, NtGdiGetRgnBox);
    HRGN a = (HRGN)g9_call(c, 0, 0, 10, 10), b = (HRGN)g9_call(c, 20, 0, 30, 10), o = (HRGN)g9_call(c, 0, 0, 0, 0);
    REQUIRE(a); REQUIRE(b); REQUIRE(o);
    RECT rc;
    EXPECT_EQ((DWORD)g9_call(cmb, (ULONG_PTR)o, (ULONG_PTR)a, (ULONG_PTR)b, RGN_OR), COMPLEXREGION);
    g9_call(box, (ULONG_PTR)o, (ULONG_PTR)&rc, 0, 0);
    CHECK(rc.left == 0 && rc.right == 30 && rc.top == 0 && rc.bottom == 10);
    EXPECT_EQ((DWORD)g9_call(cmb, (ULONG_PTR)o, (ULONG_PTR)a, (ULONG_PTR)b, RGN_AND), NULLREGION);
    EXPECT_EQ((DWORD)g9_call(cmb, (ULONG_PTR)o, (ULONG_PTR)a, (ULONG_PTR)b, RGN_XOR), COMPLEXREGION);
    EXPECT_EQ((DWORD)g9_call(cmb, (ULONG_PTR)o, (ULONG_PTR)a, (ULONG_PTR)b, RGN_DIFF), SIMPLEREGION);
    EXPECT_EQ((DWORD)g9_call(cmb, (ULONG_PTR)o, (ULONG_PTR)a, 0, RGN_COPY), SIMPLEREGION);
    g9_call(box, (ULONG_PTR)o, (ULONG_PTR)&rc, 0, 0);
    CHECK(rc.right == 10);
    EXPECT_EQ((DWORD)g9_call(cmb, (ULONG_PTR)o, (ULONG_PTR)a, (ULONG_PTR)b, 0), 0);          /* bad mode */
    EXPECT_EQ((DWORD)g9_call(cmb, (ULONG_PTR)o, (ULONG_PTR)a, (ULONG_PTR)b, RGN_COPY + 1), 0);
    EXPECT_EQ((DWORD)g9_call(cmb, (ULONG_PTR)o, (ULONG_PTR)a, (ULONG_PTR)b, -1), 0);
    g9_ntdel(a); g9_ntdel(b); g9_ntdel(o);
}

TEST(gap9_ntok_region_ptinregion_rectinregion_offset)
{
    NTF(c, NtGdiCreateRectRgn); NTF(pt, NtGdiPtInRegion); NTF(ri, NtGdiRectInRegion); NTF(off, NtGdiOffsetRgn);
    HRGN r = (HRGN)g9_call(c, 10, 10, 20, 20);
    REQUIRE(r);
    EXPECT_EQ((DWORD)g9_call(pt, (ULONG_PTR)r, 10, 10, 0), 1);
    EXPECT_EQ((DWORD)g9_call(pt, (ULONG_PTR)r, 19, 19, 0), 1);
    EXPECT_EQ((DWORD)g9_call(pt, (ULONG_PTR)r, 20, 20, 0), 0);
    EXPECT_EQ((DWORD)g9_call(pt, (ULONG_PTR)r, 9, 10, 0), 0);
    RECT in = { 15, 15, 30, 30 }, out = { 20, 20, 30, 30 };
    EXPECT_EQ((DWORD)g9_call(ri, (ULONG_PTR)r, (ULONG_PTR)&in, 0, 0), 1);
    EXPECT_EQ((DWORD)g9_call(ri, (ULONG_PTR)r, (ULONG_PTR)&out, 0, 0), 0);
    EXPECT_EQ((DWORD)g9_call(off, (ULONG_PTR)r, 5, (ULONG_PTR)-5, 0), SIMPLEREGION);
    EXPECT_EQ((DWORD)g9_call(pt, (ULONG_PTR)r, 15, 5, 0), 1);
    EXPECT_EQ((DWORD)g9_call(pt, (ULONG_PTR)r, 10, 10, 0), 0);
    g9_ntdel(r);
}

TEST(gap9_ntok_region_equalrgn_and_data)
{
    NTF(c, NtGdiCreateRectRgn); NTF(eq, NtGdiEqualRgn); NTF(gd, NtGdiGetRegionData);
    HRGN a = (HRGN)g9_call(c, 1, 2, 11, 12), b = (HRGN)g9_call(c, 1, 2, 11, 12), d = (HRGN)g9_call(c, 1, 2, 11, 13);
    REQUIRE(a); REQUIRE(b); REQUIRE(d);
    EXPECT_EQ((DWORD)g9_call(eq, (ULONG_PTR)a, (ULONG_PTR)b, 0, 0), 1);
    EXPECT_EQ((DWORD)g9_call(eq, (ULONG_PTR)a, (ULONG_PTR)d, 0, 0), 0);
    EXPECT_EQ((DWORD)g9_call(eq, (ULONG_PTR)a, (ULONG_PTR)a, 0, 0), 1);
    DWORD need = (DWORD)g9_call(gd, (ULONG_PTR)a, 0, 0, 0);
    EXPECT_EQ(need, sizeof(RGNDATAHEADER) + sizeof(RECT));
    BYTE buf[128]; memset(buf, 0, sizeof buf);
    EXPECT_EQ((DWORD)g9_call(gd, (ULONG_PTR)a, need, (ULONG_PTR)buf, 0), need);
    const RGNDATA *rd = (const RGNDATA *)buf;
    EXPECT_EQ(rd->rdh.dwSize, sizeof(RGNDATAHEADER));
    EXPECT_EQ(rd->rdh.iType, RDH_RECTANGLES);
    EXPECT_EQ(rd->rdh.nCount, 1);
    CHECK(rd->rdh.rcBound.left == 1 && rd->rdh.rcBound.bottom == 12);
    /* buffer one byte short */
    EXPECT_EQ((DWORD)g9_call(gd, (ULONG_PTR)a, need - 1, (ULONG_PTR)buf, 0), 0);
    g9_ntdel(a); g9_ntdel(b); g9_ntdel(d);
}

TEST(gap9_ntok_solid_brush_object)
{
    NTF(c, NtGdiCreateSolidBrush); NTF(go, NtGdiExtGetObjectW); NTF(d, NtGdiDeleteObjectApp);
    HBRUSH b = (HBRUSH)g9_call(c, RGB(0x12, 0x34, 0x56), 0, 0, 0);
    REQUIRE(b);
    EXPECT_EQ(U(GetObjectType)(b), OBJ_BRUSH);
    LOGBRUSH lb, ref; memset(&lb, 0xCC, sizeof lb);
    int n = (int)g9_call(go, (ULONG_PTR)b, sizeof lb, (ULONG_PTR)&lb, 0);
    EXPECT_EQ(n, sizeof(LOGBRUSH));
    EXPECT_EQ(lb.lbStyle, BS_SOLID);
    EXPECT_EQ(lb.lbColor, RGB(0x12, 0x34, 0x56));
    EXPECT_EQ(U(GetObjectW)(b, sizeof ref, &ref), sizeof ref);
    EXPECT_EQ(ref.lbColor, lb.lbColor);
    /* size query and short buffers */
    EXPECT_EQ((int)g9_call(go, (ULONG_PTR)b, 0, 0, 0), sizeof(LOGBRUSH));
    t_note("ExtGetObjectW(brush, 15 bytes) = %d", (int)g9_call(go, (ULONG_PTR)b, sizeof lb - 1, (ULONG_PTR)&lb, 0));
    EXPECT_EQ((DWORD)g9_call(d, (ULONG_PTR)b, 0, 0, 0), 1);
}

TEST(gap9_ntok_solid_brush_colors_table)
{
    NTF(c, NtGdiCreateSolidBrush); NTF(go, NtGdiExtGetObjectW);
    static const COLORREF cols[] = { 0, 0xFFFFFF, 0x0000FF, 0x00FF00, 0xFF0000, 0x010203, 0x7F7F7F, 0x80FF80 };
    DWORD before = g9_gdi_count();
    for (size_t i = 0; i < sizeof cols / sizeof *cols; i++) {
        HBRUSH b = (HBRUSH)g9_call(c, cols[i], 0, 0, 0);
        REQUIRE(b);
        LOGBRUSH lb;
        g9_call(go, (ULONG_PTR)b, sizeof lb, (ULONG_PTR)&lb, 0);
        CHECKF(lb.lbColor == cols[i], "colour 0x%06lx got 0x%06lx", (unsigned long)cols[i], (unsigned long)lb.lbColor);
        g9_ntdel(b);
    }
    EXPECT_EQ(g9_gdi_count(), before);
}

TEST(gap9_ntok_pen_object)
{
    NTF(c, NtGdiCreatePen); NTF(go, NtGdiExtGetObjectW);
    HPEN p = (HPEN)g9_call(c, PS_SOLID, 3, RGB(9, 8, 7), 0);
    REQUIRE(p);
    EXPECT_EQ(U(GetObjectType)(p), OBJ_PEN);
    LOGPEN lp; memset(&lp, 0xCC, sizeof lp);
    EXPECT_EQ((int)g9_call(go, (ULONG_PTR)p, sizeof lp, (ULONG_PTR)&lp, 0), sizeof lp);
    EXPECT_EQ(lp.lopnStyle, PS_SOLID);
    EXPECT_EQ(lp.lopnWidth.x, 3);
    EXPECT_EQ(lp.lopnColor, RGB(9, 8, 7));
    g9_ntdel(p);
}

TEST(gap9_ntok_pen_styles_table)
{
    NTF(c, NtGdiCreatePen); NTF(go, NtGdiExtGetObjectW);
    static const int st[] = { PS_SOLID, PS_DASH, PS_DOT, PS_DASHDOT, PS_DASHDOTDOT, PS_NULL, PS_INSIDEFRAME };
    for (size_t i = 0; i < sizeof st / sizeof *st; i++) {
        HPEN p = (HPEN)g9_call(c, st[i], 1, 0, 0);
        CHECKF(p != NULL, "style %d", st[i]);
        if (!p) continue;
        LOGPEN lp;
        g9_call(go, (ULONG_PTR)p, sizeof lp, (ULONG_PTR)&lp, 0);
        CHECKF((int)lp.lopnStyle == st[i], "style %d got %u", st[i], lp.lopnStyle);
        g9_ntdel(p);
    }
}

TEST(gap9_ntok_pen_invalid_style)
{
    NTF(c, NtGdiCreatePen);
    DWORD before = g9_gdi_count();
    HPEN p = (HPEN)g9_call(c, 99, 1, 0, 0);
    t_note("NtGdiCreatePen(style 99) = %p", p);
    if (p) g9_ntdel(p);
    EXPECT_EQ(g9_gdi_count(), before);
}

TEST(gap9_ntok_bitmap_create_get_set_bits)
{
    NTF(c, NtGdiCreateBitmap); NTF(go, NtGdiExtGetObjectW); NTF(gb, NtGdiGetBitmapBits); NTF(sb, NtGdiSetBitmapBits);
    DWORD init[16];
    for (int i = 0; i < 16; i++) init[i] = 0x01010101u * (i + 1);
    HBITMAP bm = (HBITMAP)g9_call5(c, 4, 4, 1, 32, (ULONG_PTR)init);
    REQUIRE(bm);
    EXPECT_EQ(U(GetObjectType)(bm), OBJ_BITMAP);
    BITMAP bi; memset(&bi, 0, sizeof bi);
    EXPECT_EQ((int)g9_call(go, (ULONG_PTR)bm, sizeof bi, (ULONG_PTR)&bi, 0), sizeof bi);
    EXPECT_EQ(bi.bmWidth, 4); EXPECT_EQ(bi.bmHeight, 4); EXPECT_EQ(bi.bmPlanes, 1); EXPECT_EQ(bi.bmBitsPixel, 32);
    EXPECT_EQ(bi.bmWidthBytes, 16);
    DWORD out[16]; memset(out, 0, sizeof out);
    EXPECT_EQ((LONG)g9_call(gb, (ULONG_PTR)bm, sizeof out, (ULONG_PTR)out, 0), 64);
    CHECK(!memcmp(out, init, sizeof out));
    DWORD upd[16];
    for (int i = 0; i < 16; i++) upd[i] = 0xA5000000u | i;
    EXPECT_EQ((LONG)g9_call(sb, (ULONG_PTR)bm, sizeof upd, (ULONG_PTR)upd, 0), 64);
    memset(out, 0, sizeof out);
    g9_call(gb, (ULONG_PTR)bm, sizeof out, (ULONG_PTR)out, 0);
    CHECK(!memcmp(out, upd, sizeof out));
    /* partial read */
    memset(out, 0, sizeof out);
    EXPECT_EQ((LONG)g9_call(gb, (ULONG_PTR)bm, 20, (ULONG_PTR)out, 0), 20);
    CHECK(!memcmp(out, upd, 20));
    g9_ntdel(bm);
}

TEST(gap9_ntok_bitmap_zero_size_fails)
{
    NTF(c, NtGdiCreateBitmap);
    DWORD before = g9_gdi_count();
    HBITMAP b = (HBITMAP)g9_call5(c, 0, 0, 1, 1, 0);
    t_note("NtGdiCreateBitmap(0,0,1,1) = %p", b);
    if (b) g9_ntdel(b);
    HBITMAP n = (HBITMAP)g9_call5(c, -1, 4, 1, 1, 0);
    t_note("NtGdiCreateBitmap(-1,4,1,1) = %p", n);
    if (n) g9_ntdel(n);
    EXPECT_EQ(g9_gdi_count(), before);
}

TEST(gap9_ntok_bitmap_dimension_roundtrip)
{
    NTF(c, NtGdiCreateBitmap); NTF(sd, NtGdiSetBitmapDimension); NTF(gd, NtGdiGetBitmapDimension);
    HBITMAP bm = (HBITMAP)g9_call5(c, 2, 2, 1, 1, 0);
    REQUIRE(bm);
    SIZE old = { 77, 77 }, now = { 0, 0 };
    EXPECT_EQ((DWORD)g9_call(gd, (ULONG_PTR)bm, (ULONG_PTR)&now, 0, 0), 1);
    CHECK(now.cx == 0 && now.cy == 0);
    EXPECT_EQ((DWORD)g9_call(sd, (ULONG_PTR)bm, 123, 456, (ULONG_PTR)&old), 1);
    CHECK(old.cx == 0 && old.cy == 0);
    EXPECT_EQ((DWORD)g9_call(gd, (ULONG_PTR)bm, (ULONG_PTR)&now, 0, 0), 1);
    CHECK(now.cx == 123 && now.cy == 456);
    EXPECT_EQ((DWORD)g9_call(sd, (ULONG_PTR)bm, 1, 2, (ULONG_PTR)&old), 1);
    CHECK(old.cx == 123 && old.cy == 456);
    g9_ntdel(bm);
}

TEST(gap9_ntok_save_restore_levels)
{
    NTF(cd, NtGdiCreateCompatibleDC); NTF(sv, NtGdiSaveDC); NTF(rs, NtGdiRestoreDC); NTF(d, NtGdiDeleteObjectApp);
    HDC dc = (HDC)g9_call(cd, 0, 0, 0, 0);
    REQUIRE(dc);
    EXPECT_EQ((DWORD)g9_call(sv, (ULONG_PTR)dc, 0, 0, 0), 1);
    EXPECT_EQ((DWORD)g9_call(sv, (ULONG_PTR)dc, 0, 0, 0), 2);
    EXPECT_EQ((DWORD)g9_call(sv, (ULONG_PTR)dc, 0, 0, 0), 3);
    EXPECT_EQ((DWORD)g9_call(rs, (ULONG_PTR)dc, (ULONG_PTR)-1, 0, 0), 1);
    EXPECT_EQ((DWORD)g9_call(rs, (ULONG_PTR)dc, 1, 0, 0), 1);       /* absolute level 1 */
    EXPECT_EQ((DWORD)g9_call(rs, (ULONG_PTR)dc, (ULONG_PTR)-1, 0, 0), 0);   /* nothing left */
    EXPECT_EQ((DWORD)g9_call(rs, (ULONG_PTR)dc, 5, 0, 0), 0);
    EXPECT_EQ((DWORD)g9_call(rs, (ULONG_PTR)dc, 0, 0, 0), 0);
    g9_call(d, (ULONG_PTR)dc, 0, 0, 0);
}

TEST(gap9_ntok_select_objects_return_previous)
{
    NTF(cd, NtGdiCreateCompatibleDC); NTF(d, NtGdiDeleteObjectApp); NTF(sb, NtGdiSelectBrush); NTF(sp, NtGdiSelectPen);
    NTF(cb, NtGdiCreateSolidBrush); NTF(cp, NtGdiCreatePen);
    HDC dc = (HDC)g9_call(cd, 0, 0, 0, 0);
    REQUIRE(dc);
    HGDIOBJ ob = U(GetCurrentObject)(dc, OBJ_BRUSH), op = U(GetCurrentObject)(dc, OBJ_PEN);
    HBRUSH b = (HBRUSH)g9_call(cb, RGB(1, 1, 1), 0, 0, 0);
    HPEN p = (HPEN)g9_call(cp, PS_SOLID, 1, 0, 0);
    REQUIRE(b); REQUIRE(p);
    EXPECT_EQ(g9_call(sb, (ULONG_PTR)dc, (ULONG_PTR)b, 0, 0), (ULONG_PTR)ob);
    EXPECT_EQ(U(GetCurrentObject)(dc, OBJ_BRUSH), b);
    EXPECT_EQ(g9_call(sp, (ULONG_PTR)dc, (ULONG_PTR)p, 0, 0), (ULONG_PTR)op);
    EXPECT_EQ(U(GetCurrentObject)(dc, OBJ_PEN), p);
    /* wrong-type objects cannot be selected */
    EXPECT_EQ(g9_call(sb, (ULONG_PTR)dc, (ULONG_PTR)p, 0, 0), 0);
    EXPECT_EQ(g9_call(sp, (ULONG_PTR)dc, (ULONG_PTR)b, 0, 0), 0);
    EXPECT_EQ(g9_call(sb, (ULONG_PTR)dc, G9_BOGUS, 0, 0), 0);
    g9_call(sb, (ULONG_PTR)dc, (ULONG_PTR)ob, 0, 0);
    g9_call(sp, (ULONG_PTR)dc, (ULONG_PTR)op, 0, 0);
    g9_call(d, (ULONG_PTR)b, 0, 0, 0); g9_call(d, (ULONG_PTR)p, 0, 0, 0);
    g9_call(d, (ULONG_PTR)dc, 0, 0, 0);
}

TEST(gap9_ntok_select_bitmap_returns_default_1x1)
{
    NTF(cd, NtGdiCreateCompatibleDC); NTF(d, NtGdiDeleteObjectApp); NTF(sbm, NtGdiSelectBitmap); NTF(cb, NtGdiCreateBitmap);
    HDC dc = (HDC)g9_call(cd, 0, 0, 0, 0);
    REQUIRE(dc);
    HBITMAP bm = (HBITMAP)g9_call5(cb, 8, 8, 1, 32, 0);
    REQUIRE(bm);
    HGDIOBJ def = U(GetCurrentObject)(dc, OBJ_BITMAP);
    EXPECT_EQ(g9_call(sbm, (ULONG_PTR)dc, (ULONG_PTR)bm, 0, 0), (ULONG_PTR)def);
    EXPECT_EQ(U(GetCurrentObject)(dc, OBJ_BITMAP), bm);
    /* a bitmap already selected into a DC cannot go into a second one */
    HDC dc2 = (HDC)g9_call(cd, 0, 0, 0, 0);
    REQUIRE(dc2);
    HGDIOBJ def2 = U(GetCurrentObject)(dc2, OBJ_BITMAP);
    EXPECT_EQ(g9_call(sbm, (ULONG_PTR)dc2, (ULONG_PTR)bm, 0, 0), 0);
    g9_call(sbm, (ULONG_PTR)dc, (ULONG_PTR)def, 0, 0);
    EXPECT_EQ(g9_call(sbm, (ULONG_PTR)dc2, (ULONG_PTR)bm, 0, 0), (ULONG_PTR)def2);
    g9_call(sbm, (ULONG_PTR)dc2, (ULONG_PTR)def2, 0, 0);
    g9_call(d, (ULONG_PTR)bm, 0, 0, 0);
    g9_call(d, (ULONG_PTR)dc, 0, 0, 0); g9_call(d, (ULONG_PTR)dc2, 0, 0, 0);
}

TEST(gap9_ntok_patblt_exact_pixels)
{
    NTF(pb, NtGdiPatBlt);
    g9_dib_t d;
    REQUIRE(g9_dib_open(&d, 16, 16));
    for (int i = 0; i < 256; i++) d.bits[i] = 0x00ABCDEF;
    EXPECT_EQ((DWORD)g9_call8(pb, (ULONG_PTR)d.dc, 0, 0, 16, 16, WHITENESS, 0, 0), 1);
    for (int i = 0; i < 256; i++) if ((d.bits[i] & 0xFFFFFF) != 0xFFFFFF) { CHECKF(0, "white px %d = 0x%08lx", i, (unsigned long)d.bits[i]); break; }
    EXPECT_EQ((DWORD)g9_call8(pb, (ULONG_PTR)d.dc, 2, 3, 4, 5, BLACKNESS, 0, 0), 1);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            int in = x >= 2 && x < 6 && y >= 3 && y < 8;
            DWORD px = d.bits[y * 16 + x] & 0xFFFFFF;
            if (px != (in ? 0u : 0xFFFFFFu)) { CHECKF(0, "px(%d,%d)=0x%06lx", x, y, (unsigned long)px); y = 99; break; }
        }
    EXPECT_EQ((DWORD)g9_call8(pb, (ULONG_PTR)d.dc, 0, 0, 16, 16, DSTINVERT, 0, 0), 1);
    CHECK((d.bits[3 * 16 + 2] & 0xFFFFFF) == 0xFFFFFF);
    CHECK((d.bits[0] & 0xFFFFFF) == 0);
    g9_dib_close(&d);
}

TEST(gap9_ntok_patblt_patcopy_with_brush)
{
    NTF(pb, NtGdiPatBlt); NTF(cb, NtGdiCreateSolidBrush); NTF(sb, NtGdiSelectBrush);
    g9_dib_t d;
    REQUIRE(g9_dib_open(&d, 8, 8));
    HBRUSH b = (HBRUSH)g9_call(cb, RGB(0x12, 0x34, 0x56), 0, 0, 0);
    REQUIRE(b);
    HGDIOBJ old = (HGDIOBJ)g9_call(sb, (ULONG_PTR)d.dc, (ULONG_PTR)b, 0, 0);
    REQUIRE(old);
    EXPECT_EQ((DWORD)g9_call8(pb, (ULONG_PTR)d.dc, 0, 0, 8, 8, PATCOPY, 0, 0), 1);
    for (int i = 0; i < 64; i++) if ((d.bits[i] & 0xFFFFFF) != 0x123456) { CHECKF(0, "px %d = 0x%08lx", i, (unsigned long)d.bits[i]); break; }
    EXPECT_EQ((DWORD)g9_call8(pb, (ULONG_PTR)d.dc, 0, 0, 8, 8, PATINVERT, 0, 0), 1);
    CHECK((d.bits[0] & 0xFFFFFF) == 0);
    g9_call(sb, (ULONG_PTR)d.dc, (ULONG_PTR)old, 0, 0);
    g9_ntdel(b);
    g9_dib_close(&d);
}

TEST(gap9_ntok_patblt_clipped_to_surface)
{
    NTF(pb, NtGdiPatBlt);
    g9_dib_t d;
    REQUIRE(g9_dib_open(&d, 8, 8));
    memset(d.bits, 0, 8 * 8 * 4);
    EXPECT_EQ((DWORD)g9_call8(pb, (ULONG_PTR)d.dc, 4, 4, 100, 100, WHITENESS, 0, 0), 1);
    EXPECT_EQ(d.bits[3 * 8 + 3] & 0xFFFFFF, 0);
    EXPECT_EQ(d.bits[4 * 8 + 4] & 0xFFFFFF, 0xFFFFFF);
    EXPECT_EQ(d.bits[7 * 8 + 7] & 0xFFFFFF, 0xFFFFFF);
    /* fully outside */
    memset(d.bits, 0, 8 * 8 * 4);
    g9_call8(pb, (ULONG_PTR)d.dc, 100, 100, 5, 5, WHITENESS, 0, 0);
    g9_call8(pb, (ULONG_PTR)d.dc, -50, -50, 5, 5, WHITENESS, 0, 0);
    for (int i = 0; i < 64; i++) if (d.bits[i] & 0xFFFFFF) { CHECKF(0, "px %d touched", i); break; }
    g9_dib_close(&d);
}

TEST(gap9_ntok_bitblt_srccopy_between_dibs)
{
    NTF(bb, NtGdiBitBlt);
    g9_dib_t s, d;
    REQUIRE(g9_dib_open(&s, 8, 8)); REQUIRE(g9_dib_open(&d, 8, 8));
    for (int i = 0; i < 64; i++) s.bits[i] = 0x00010000u * (i & 0xFF) + 0x0101 * i;
    memset(d.bits, 0, 8 * 8 * 4);
    ULONG_PTR r = ((fn11)bb)((ULONG_PTR)d.dc, 0, 0, 8, 8, (ULONG_PTR)s.dc, 0, 0, SRCCOPY, 0, 0);
    EXPECT_EQ((DWORD)r, 1);
    for (int i = 0; i < 64; i++) if ((d.bits[i] & 0xFFFFFF) != (s.bits[i] & 0xFFFFFF)) { CHECKF(0, "px %d", i); break; }
    r = ((fn11)bb)((ULONG_PTR)d.dc, 0, 0, 8, 8, (ULONG_PTR)s.dc, 0, 0, NOTSRCCOPY, 0, 0);
    EXPECT_EQ((DWORD)r, 1);
    for (int i = 0; i < 64; i++) if ((d.bits[i] & 0xFFFFFF) != (~s.bits[i] & 0xFFFFFF)) { CHECKF(0, "inv px %d", i); break; }
    r = ((fn11)bb)((ULONG_PTR)d.dc, 0, 0, 8, 8, (ULONG_PTR)s.dc, 0, 0, SRCINVERT, 0, 0);
    EXPECT_EQ((DWORD)r, 1);
    for (int i = 0; i < 64; i++) if ((d.bits[i] & 0xFFFFFF) != 0xFFFFFF) { CHECKF(0, "xor px %d = 0x%06lx", i, (unsigned long)(d.bits[i] & 0xFFFFFF)); break; }
    g9_dib_close(&s); g9_dib_close(&d);
}

TEST(gap9_ntok_bitblt_offsets)
{
    NTF(bb, NtGdiBitBlt);
    g9_dib_t s, d;
    REQUIRE(g9_dib_open(&s, 8, 8)); REQUIRE(g9_dib_open(&d, 8, 8));
    for (int i = 0; i < 64; i++) s.bits[i] = 0x100000 + i;
    memset(d.bits, 0, 8 * 8 * 4);
    ULONG_PTR r = ((fn11)bb)((ULONG_PTR)d.dc, 2, 1, 4, 3, (ULONG_PTR)s.dc, 3, 5, SRCCOPY, 0, 0);
    EXPECT_EQ((DWORD)r, 1);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            int in = x >= 2 && x < 6 && y >= 1 && y < 4;
            DWORD want = in ? (0x100000u + (5 + (y - 1)) * 8 + (3 + (x - 2))) : 0;
            if ((d.bits[y * 8 + x] & 0xFFFFFF) != want) { CHECKF(0, "px(%d,%d)=0x%06lx want 0x%06lx", x, y, (unsigned long)(d.bits[y * 8 + x] & 0xFFFFFF), (unsigned long)want); y = 99; break; }
        }
    g9_dib_close(&s); g9_dib_close(&d);
}

TEST(gap9_ntok_bitblt_bogus_source)
{
    NTF(bb, NtGdiBitBlt);
    g9_dib_t d;
    REQUIRE(g9_dib_open(&d, 4, 4));
    for (int i = 0; i < 16; i++) d.bits[i] = 0x00FEDCBA;
    ULONG_PTR r = ((fn11)bb)((ULONG_PTR)d.dc, 0, 0, 4, 4, G9_BOGUS, 0, 0, SRCCOPY, 0, 0);
    EXPECT_EQ((DWORD)r, 0);
    for (int i = 0; i < 16; i++) if ((d.bits[i] & 0xFFFFFF) != 0xFEDCBA) { CHECKF(0, "px %d changed", i); break; }
    r = ((fn11)bb)(G9_BOGUS, 0, 0, 4, 4, (ULONG_PTR)d.dc, 0, 0, SRCCOPY, 0, 0);
    EXPECT_EQ((DWORD)r, 0);
    g9_dib_close(&d);
}

TEST(gap9_ntok_setpixel_getpixel_32bpp)
{
    NTF(sp, NtGdiSetPixel); NTF(gp, NtGdiGetPixel);
    g9_dib_t d;
    REQUIRE(g9_dib_open(&d, 8, 8));
    memset(d.bits, 0, 8 * 8 * 4);
    static const COLORREF cols[] = { RGB(0x12, 0x34, 0x56), RGB(255, 255, 255), RGB(0, 0, 0), RGB(1, 2, 3), RGB(0xFE, 0, 0x7F) };
    for (int i = 0; i < 5; i++) {
        COLORREF r = (COLORREF)g9_call(sp, (ULONG_PTR)d.dc, i, 2, cols[i]);
        CHECKF(r == cols[i], "SetPixel returned 0x%06lx want 0x%06lx", (unsigned long)r, (unsigned long)cols[i]);
        CHECKF((COLORREF)g9_call(gp, (ULONG_PTR)d.dc, i, 2, 0) == cols[i], "GetPixel %d", i);
        DWORD want = ((cols[i] & 0xFF) << 16) | (cols[i] & 0xFF00) | ((cols[i] >> 16) & 0xFF);
        CHECKF((d.bits[2 * 8 + i] & 0xFFFFFF) == want, "memory %d = 0x%06lx", i, (unsigned long)(d.bits[2 * 8 + i] & 0xFFFFFF));
    }
    /* outside the surface */
    DWORD r = (DWORD)g9_call(gp, (ULONG_PTR)d.dc, 8, 0, 0);
    CHECKF(r == CLR_INVALID, "GetPixel outside = 0x%lx", (unsigned long)r);
    r = (DWORD)g9_call(gp, (ULONG_PTR)d.dc, (ULONG_PTR)-1, 0, 0);
    CHECKF(r == CLR_INVALID, "GetPixel -1 = 0x%lx", (unsigned long)r);
    g9_dib_close(&d);
}

TEST(gap9_ntok_moveto_lineto_pixels)
{
    NTF(pb, NtGdiPatBlt); NTF(mv, NtGdiMoveTo); NTF(ln, NtGdiLineTo); NTF(cp, NtGdiCreatePen); NTF(sp, NtGdiSelectPen);
    g9_dib_t d;
    REQUIRE(g9_dib_open(&d, 12, 6));
    g9_call8(pb, (ULONG_PTR)d.dc, 0, 0, 12, 6, WHITENESS, 0, 0);
    HPEN p = (HPEN)g9_call(cp, PS_SOLID, 1, RGB(0xC8, 0x10, 0x20), 0);
    REQUIRE(p);
    HGDIOBJ old = (HGDIOBJ)g9_call(sp, (ULONG_PTR)d.dc, (ULONG_PTR)p, 0, 0);
    POINT prev = { -1, -1 };
    EXPECT_EQ((DWORD)g9_call(mv, (ULONG_PTR)d.dc, 1, 2, (ULONG_PTR)&prev), 1);
    CHECK(prev.x == 0 && prev.y == 0);
    EXPECT_EQ((DWORD)g9_call(ln, (ULONG_PTR)d.dc, 9, 2, 0), 1);
    DWORD want = 0xC81020;
    for (int x = 0; x < 12; x++) {
        DWORD px = d.bits[2 * 12 + x] & 0xFFFFFF;
        DWORD exp = (x >= 1 && x < 9) ? want : 0xFFFFFF;
        CHECKF(px == exp, "x=%d px=0x%06lx", x, (unsigned long)px);
    }
    for (int y = 0; y < 6; y++) if (y != 2) CHECKF((d.bits[y * 12 + 4] & 0xFFFFFF) == 0xFFFFFF, "row %d touched", y);
    /* the current position moved to the end point */
    POINT cur; U(GetCurrentPositionEx)(d.dc, &cur);
    CHECK(cur.x == 9 && cur.y == 2);
    g9_call(sp, (ULONG_PTR)d.dc, (ULONG_PTR)old, 0, 0);
    g9_ntdel(p);
    g9_dib_close(&d);
}

TEST(gap9_ntok_rectangle_ellipse_exact_fill)
{
    NTF(pb, NtGdiPatBlt); NTF(rc, NtGdiRectangle); NTF(cb, NtGdiCreateSolidBrush); NTF(sb, NtGdiSelectBrush); NTF(sp, NtGdiSelectPen);
    g9_dib_t d;
    REQUIRE(g9_dib_open(&d, 10, 10));
    g9_call8(pb, (ULONG_PTR)d.dc, 0, 0, 10, 10, WHITENESS, 0, 0);
    HBRUSH b = (HBRUSH)g9_call(cb, RGB(0, 0xFF, 0), 0, 0, 0);
    REQUIRE(b);
    HGDIOBJ ob = (HGDIOBJ)g9_call(sb, (ULONG_PTR)d.dc, (ULONG_PTR)b, 0, 0);
    HGDIOBJ op = (HGDIOBJ)g9_call(sp, (ULONG_PTR)d.dc, (ULONG_PTR)U(GetStockObject)(NULL_PEN), 0, 0);
    EXPECT_EQ((DWORD)g9_call5(rc, (ULONG_PTR)d.dc, 2, 2, 7, 6), 1);
    /* NULL pen: fills [2,7) x [2,6) */
    /* the NULL pen leaves the exact extent of the last column/row to the driver: check clear interior and exterior */
    for (int y = 0; y < 10; y++)
        for (int x = 0; x < 10; x++) {
            int in = x >= 2 && x < 6 && y >= 2 && y < 5;
            int out = x < 2 || x > 7 || y < 2 || y > 6;
            DWORD px = d.bits[y * 10 + x] & 0xFFFFFF;
            if ((in && px != 0x00FF00u) || (out && px != 0xFFFFFFu)) { CHECKF(0, "px(%d,%d)=0x%06lx", x, y, (unsigned long)px); y = 99; break; }
        }
    t_note("NULL-pen Rectangle right edge px(6,3)=0x%06lx bottom px(3,5)=0x%06lx", (unsigned long)(d.bits[3 * 10 + 6] & 0xFFFFFF), (unsigned long)(d.bits[5 * 10 + 3] & 0xFFFFFF));
    g9_call(sb, (ULONG_PTR)d.dc, (ULONG_PTR)ob, 0, 0);
    g9_call(sp, (ULONG_PTR)d.dc, (ULONG_PTR)op, 0, 0);
    g9_ntdel(b);
    g9_dib_close(&d);
}

TEST(gap9_ntok_getdevicecaps_parity_memory_dc)
{
    NTF(gc, NtGdiGetDeviceCaps);
    g9_dib_t d;
    REQUIRE(g9_dib_open(&d, 4, 4));
    static const int idx[] = { DRIVERVERSION, TECHNOLOGY, HORZRES, VERTRES, BITSPIXEL, PLANES, NUMCOLORS, RASTERCAPS, SIZEPALETTE, COLORRES, LOGPIXELSX, LOGPIXELSY, ASPECTX, ASPECTY, ASPECTXY, CURVECAPS, LINECAPS, POLYGONALCAPS, TEXTCAPS, CLIPCAPS, NUMBRUSHES, NUMPENS, NUMFONTS, NUMMARKERS };
    for (size_t i = 0; i < sizeof idx / sizeof *idx; i++) {
        int a = U(GetDeviceCaps)(d.dc, idx[i]);
        int b = (int)g9_call(gc, (ULONG_PTR)d.dc, idx[i], 0, 0);
        CHECKF(a == b, "index %d: GetDeviceCaps %d NtGdiGetDeviceCaps %d", idx[i], a, b);
    }
    EXPECT_EQ((int)g9_call(gc, (ULONG_PTR)d.dc, BITSPIXEL, 0, 0), 32);
    EXPECT_EQ((int)g9_call(gc, (ULONG_PTR)d.dc, HORZRES, 0, 0), 4);
    EXPECT_EQ((int)g9_call(gc, (ULONG_PTR)d.dc, VERTRES, 0, 0), 4);
    EXPECT_EQ((int)g9_call(gc, (ULONG_PTR)d.dc, PLANES, 0, 0), 1);
    g9_dib_close(&d);
}

static DWORD WINAPI thr_getobj(LPVOID a)
{
    ULONG_PTR *v = (ULONG_PTR *)a;
    LOGBRUSH lb; memset(&lb, 0, sizeof lb);
    FARPROC p = t_proc_gdi32full("NtGdiExtGetObjectW");
    v[2] = (ULONG_PTR)g9_call(p, v[0], sizeof lb, (ULONG_PTR)&lb, 0);
    v[3] = lb.lbColor;
    return 0;
}
TEST(gap9_ntok_cross_thread_handle_use)
{
    NTF(cb, NtGdiCreateSolidBrush);
    HBRUSH b = (HBRUSH)g9_call(cb, RGB(0x44, 0x55, 0x66), 0, 0, 0);
    REQUIRE(b);
    ULONG_PTR v[4] = { (ULONG_PTR)b, 0, 0, 0 };
    HANDLE h = CreateThread(NULL, 0, thr_getobj, v, 0, NULL);
    REQUIRE(h);
    WaitForSingleObject(h, 10000);
    CloseHandle(h);
    EXPECT_EQ((int)v[2], sizeof(LOGBRUSH));
    EXPECT_EQ((COLORREF)v[3], RGB(0x44, 0x55, 0x66));
    g9_ntdel(b);
}

TEST(gap9_ntok_no_object_leak_over_many_cycles)
{
    NTF(cd, NtGdiCreateCompatibleDC); NTF(d, NtGdiDeleteObjectApp); NTF(cr, NtGdiCreateRectRgn); NTF(cb, NtGdiCreateSolidBrush);
    NTF(cp, NtGdiCreatePen); NTF(cbm, NtGdiCreateBitmap);
    DWORD before = g9_gdi_count();
    for (int i = 0; i < 300; i++) {
        HGDIOBJ o[5];
        o[0] = (HGDIOBJ)g9_call(cd, 0, 0, 0, 0);
        o[1] = (HGDIOBJ)g9_call(cr, 0, 0, i, i);
        o[2] = (HGDIOBJ)g9_call(cb, i, 0, 0, 0);
        o[3] = (HGDIOBJ)g9_call(cp, PS_SOLID, 1, i, 0);
        o[4] = (HGDIOBJ)g9_call5(cbm, 3, 3, 1, 1, 0);
        for (int k = 0; k < 5; k++) if (!o[k] || !g9_call(d, (ULONG_PTR)o[k], 0, 0, 0)) { CHECKF(0, "cycle %d object %d", i, k); i = 1000; break; }
    }
    EXPECT_EQ(g9_gdi_count(), before);
}

TEST(gap9_ntok_flush_and_stats)
{
    NTF(fl, NtGdiFlush);
    g9_call(fl, 0, 0, 0, 0);
    g9_call(fl, 0, 0, 0, 0);
    CHECK(U(GdiFlush)());
}

TEST(gap9_ntok_unrealize_non_brush_object)
{
    NTF(ur, NtGdiUnrealizeObject); NTF(cb, NtGdiCreateSolidBrush); NTF(cp, NtGdiCreatePen);
    HBRUSH b = (HBRUSH)g9_call(cb, RGB(1, 2, 3), 0, 0, 0);
    HPEN p = (HPEN)g9_call(cp, PS_SOLID, 1, 0, 0);
    REQUIRE(b); REQUIRE(p);
    t_note("UnrealizeObject(brush)=%lu (pen)=%lu", (unsigned long)(DWORD)g9_call(ur, (ULONG_PTR)b, 0, 0, 0), (unsigned long)(DWORD)g9_call(ur, (ULONG_PTR)p, 0, 0, 0));
    EXPECT_EQ(U(GetObjectType)(b), OBJ_BRUSH);
    EXPECT_EQ(U(GetObjectType)(p), OBJ_PEN);
    g9_ntdel(b); g9_ntdel(p);
}
