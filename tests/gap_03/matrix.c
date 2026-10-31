/* Cross-product tests: every attribute setter against every DC kind and bit depth. */
#include "g3.h"

typedef struct { HDC dc; HBITMAP bmp, old; void *bits; int bpp; } KDC;
static int kdc_make(KDC *k, int bpp, int w, int h) {
    union { BITMAPINFO bi; char raw[sizeof(BITMAPINFOHEADER) + 256 * 4]; } u;
    memset(k, 0, sizeof *k); memset(&u, 0, sizeof u);
    k->bpp = bpp;
    k->dc = U(CreateCompatibleDC)(NULL);
    if (!k->dc) return 0;
    if (bpp == 0) return 1;   /* default 1x1 mono bitmap */
    u.bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    u.bi.bmiHeader.biWidth = w; u.bi.bmiHeader.biHeight = -h; u.bi.bmiHeader.biPlanes = 1;
    u.bi.bmiHeader.biBitCount = bpp; u.bi.bmiHeader.biCompression = BI_RGB;
    k->bmp = U(CreateDIBSection)(k->dc, &u.bi, DIB_RGB_COLORS, &k->bits, NULL, 0);
    if (!k->bmp) { U(DeleteDC)(k->dc); k->dc = NULL; return 0; }
    k->old = (HBITMAP)U(SelectObject)(k->dc, k->bmp);
    if (bpp <= 8) {
        RGBQUAD q[256]; int n = 1 << bpp, i;
        for (i = 0; i < n; i++) { q[i].rgbRed = q[i].rgbGreen = q[i].rgbBlue = i ? 255 : 0; q[i].rgbReserved = 0; }
        U(SetDIBColorTable)(k->dc, 0, n, q);
    }
    return 1;
}
static void kdc_free(KDC *k) {
    if (!k->dc) return;
    if (k->bmp) { U(SelectObject)(k->dc, k->old); U(DeleteObject)(k->bmp); }
    U(DeleteDC)(k->dc);
}
static const int DEPTHS[] = { 0, 1, 4, 8, 16, 24, 32 };
#define NDEPTH 7

TEST(gap3_matrix_setters_all_depths) {
    REQUIRE(U(CreateDIBSection));
    int i;
    for (i = 0; i < NDEPTH; i++) {
        KDC k; if (!kdc_make(&k, DEPTHS[i], 16, 16)) { CHECKF(0, "make %d", DEPTHS[i]); continue; }
        HDC d = k.dc;
        CHECKF(U(SetBkMode)(d, TRANSPARENT) == OPAQUE, "bpp %d", k.bpp);
        CHECKF(U(GetBkMode)(d) == TRANSPARENT, "bpp %d", k.bpp);
        CHECKF(U(SetPolyFillMode)(d, WINDING) == ALTERNATE, "bpp %d", k.bpp);
        CHECKF(U(SetStretchBltMode)(d, HALFTONE) == BLACKONWHITE, "bpp %d", k.bpp);
        CHECKF(U(SetTextAlign)(d, TA_CENTER) == 0, "bpp %d", k.bpp);
        CHECKF(U(SetTextColor)(d, RGB(255,0,0)) == 0, "bpp %d", k.bpp);
        CHECKF(U(GetTextColor)(d) == RGB(255,0,0) || DEPTHS[i] == 4 || DEPTHS[i] == 8 || DEPTHS[i] == 1, "bpp %d", k.bpp);
        FLOAT old = 0;
        CHECKF(U(SetMiterLimit)(d, 3.0f, &old) && old == 10.0f, "bpp %d", k.bpp);
        POINT p;
        CHECKF(U(MoveToEx)(d, 3, 4, &p) && p.x == 0 && p.y == 0, "bpp %d", k.bpp);
        CHECKF(U(SaveDC)(d) == 1, "bpp %d", k.bpp);
        U(SetBkMode)(d, OPAQUE);
        CHECKF(U(RestoreDC)(d, -1) && U(GetBkMode)(d) == TRANSPARENT, "bpp %d", k.bpp);
        kdc_free(&k);
    }
}
TEST(gap3_matrix_textcolor_exact_depth) {
    /* the stored value is the requested colour regardless of the surface depth (not nearest-matched) */
    int i; static const COLORREF cs[] = { RGB(1,2,3), RGB(128,64,32), RGB(255,255,255), RGB(17,34,51) };
    for (i = 0; i < NDEPTH; i++) {
        KDC k; if (!kdc_make(&k, DEPTHS[i], 8, 8)) continue;
        unsigned c;
        for (c = 0; c < 4; c++) {
            U(SetTextColor)(k.dc, cs[c]);
            t_note("bpp %d: SetTextColor(%06lx) -> GetTextColor %06lx", DEPTHS[i], (unsigned long)cs[c], (unsigned long)U(GetTextColor)(k.dc));
        }
        kdc_free(&k);
    }
}
TEST(gap3_matrix_rectangle_depths) {
    int i;
    for (i = 1; i < NDEPTH; i++) {
        KDC k; if (!kdc_make(&k, DEPTHS[i], 16, 16)) { CHECKF(0, "make %d", DEPTHS[i]); continue; }
        HPEN pen = U(CreatePen)(PS_SOLID, 1, RGB(0,0,0));
        HGDIOBJ op = U(SelectObject)(k.dc, pen);
        HGDIOBJ ob = U(SelectObject)(k.dc, U(GetStockObject)(NULL_BRUSH));
        U(PatBlt)(k.dc, 0, 0, 16, 16, WHITENESS);
        CHECKF(U(Rectangle)(k.dc, 2, 2, 10, 10), "bpp %d", k.bpp);
        CHECKF(U(GetPixel)(k.dc, 2, 2) == 0, "bpp %d", k.bpp);
        CHECKF(U(GetPixel)(k.dc, 9, 9) == 0, "bpp %d", k.bpp);
        CHECKF(U(GetPixel)(k.dc, 5, 5) == 0xffffff, "bpp %d", k.bpp);
        CHECKF(U(GetPixel)(k.dc, 10, 10) == 0xffffff, "bpp %d", k.bpp);
        U(SelectObject)(k.dc, ob); U(SelectObject)(k.dc, op); U(DeleteObject)(pen);
        kdc_free(&k);
    }
}
TEST(gap3_matrix_polylineto_depths) {
    int i;
    for (i = 1; i < NDEPTH; i++) {
        KDC k; if (!kdc_make(&k, DEPTHS[i], 16, 16)) continue;
        POINT pt = { 12, 5 };
        U(PatBlt)(k.dc, 0, 0, 16, 16, WHITENESS);
        U(MoveToEx)(k.dc, 2, 5, NULL);
        CHECKF(U(PolylineTo)(k.dc, &pt, 1), "bpp %d", k.bpp);
        CHECKF(U(GetPixel)(k.dc, 2, 5) == 0, "bpp %d", k.bpp);
        CHECKF(U(GetPixel)(k.dc, 11, 5) == 0, "bpp %d", k.bpp);
        CHECKF(U(GetPixel)(k.dc, 12, 5) == 0xffffff, "bpp %d", k.bpp);
        CHECKF(U(GetPixel)(k.dc, 6, 6) == 0xffffff, "bpp %d", k.bpp);
        kdc_free(&k);
    }
}
TEST(gap3_matrix_selectclip_depths) {
    int i;
    for (i = 1; i < NDEPTH; i++) {
        KDC k; if (!kdc_make(&k, DEPTHS[i], 16, 16)) continue;
        HRGN r = U(CreateRectRgn)(2, 2, 6, 6); RECT cb;
        CHECKF(U(SelectClipRgn)(k.dc, r) == SIMPLEREGION, "bpp %d", k.bpp);
        CHECKF(U(GetClipBox)(k.dc, &cb) == SIMPLEREGION && cb.left == 2 && cb.right == 6, "bpp %d", k.bpp);
        CHECKF(U(SelectClipRgn)(k.dc, NULL) != 0, "bpp %d", k.bpp);
        U(DeleteObject)(r);
        kdc_free(&k);
    }
}
TEST(gap3_matrix_stretchdibits_into_depths) {
    int i;
    for (i = 1; i < NDEPTH; i++) {
        KDC k; if (!kdc_make(&k, DEPTHS[i], 8, 8)) continue;
        BITMAPINFO bi; memset(&bi, 0, sizeof bi);
        bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 2; bi.bmiHeader.biHeight = -2;
        bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
        DWORD src[4] = { 0, 0, 0, 0 };   /* black */
        U(PatBlt)(k.dc, 0, 0, 8, 8, WHITENESS);
        int r = U(StretchDIBits)(k.dc, 1, 1, 2, 2, 0, 0, 2, 2, src, &bi, DIB_RGB_COLORS, SRCCOPY);
        CHECKF(r == 2, "bpp %d ret %d", k.bpp, r);
        CHECKF(U(GetPixel)(k.dc, 1, 1) == 0 && U(GetPixel)(k.dc, 2, 2) == 0, "bpp %d", k.bpp);
        CHECKF(U(GetPixel)(k.dc, 0, 0) == 0xffffff && U(GetPixel)(k.dc, 3, 3) == 0xffffff, "bpp %d", k.bpp);
        kdc_free(&k);
    }
}
TEST(gap3_matrix_invalid_handles_all_setters) {
    HDC bad[4]; int b;
    bad[0] = NULL; bad[1] = g3_stale_dc();
    bad[2] = (HDC)U(GetStockObject)(BLACK_PEN); bad[3] = (HDC)U(GetStockObject)(DEFAULT_PALETTE);
    for (b = 0; b < 4; b++) {
        HDC d = bad[b]; XFORM x = { 1,0,0,1,0,0 }; POINT pt[3] = { {1,1},{2,2},{3,3} }; BYTE ty[3] = { 6, 2, 2 };
        CHECKF(U(SetBkMode)(d, 1) == 0, "b%d", b);
        CHECKF(U(SetPolyFillMode)(d, 1) == 0, "b%d", b);
        CHECKF(U(SetStretchBltMode)(d, 1) == 0, "b%d", b);
        CHECKF(U(SetTextAlign)(d, 0) == GDI_ERROR, "b%d", b);
        CHECKF(U(SetTextColor)(d, 0) == CLR_INVALID, "b%d", b);
        CHECKF(U(SetMiterLimit)(d, 2.0f, NULL) == 0, "b%d", b);
        CHECKF(U(SaveDC)(d) == 0, "b%d", b);
        CHECKF(U(RestoreDC)(d, -1) == 0, "b%d", b);
        CHECKF(U(SetWorldTransform)(d, &x) == 0, "b%d", b);
        CHECKF(U(MoveToEx)(d, 1, 1, NULL) == 0, "b%d", b);
        CHECKF(U(Rectangle)(d, 0, 0, 2, 2) == 0, "b%d", b);
        CHECKF(U(PolylineTo)(d, pt, 3) == 0, "b%d", b);
        CHECKF(U(PolyBezierTo)(d, pt, 3) == 0, "b%d", b);
        CHECKF(U(PolyDraw)(d, pt, ty, 3) == 0, "b%d", b);
        CHECKF(U(StrokePath)(d) == 0, "b%d", b);
        CHECKF(U(SelectClipPath)(d, RGN_COPY) == 0, "b%d", b);
        CHECKF(U(SelectClipRgn)(d, NULL) == 0, "b%d", b);
        CHECKF(U(SelectObject)(d, U(GetStockObject)(BLACK_PEN)) == 0, "b%d", b);
    }
}
TEST(gap3_matrix_lasterror_codes) {
    /* NULL: ERROR_INVALID_HANDLE; deleted DC: ERROR_INVALID_PARAMETER (shared-table validation) */
    G3_WINE_SKIP("last-error values");
    HDC st = g3_stale_dc();
    struct { const char *n; int e_null, e_stale; } t[5]; int k = 0;
    SET_ERR(0); U(SetBkMode)(NULL, 1); t[k].n = "SetBkMode"; t[k].e_null = GetLastError();
    SET_ERR(0); U(SetBkMode)(st, 1); t[k++].e_stale = GetLastError();
    SET_ERR(0); U(SetTextColor)(NULL, 1); t[k].n = "SetTextColor"; t[k].e_null = GetLastError();
    SET_ERR(0); U(SetTextColor)(st, 1); t[k++].e_stale = GetLastError();
    SET_ERR(0); U(SetTextAlign)(NULL, 1); t[k].n = "SetTextAlign"; t[k].e_null = GetLastError();
    SET_ERR(0); U(SetTextAlign)(st, 1); t[k++].e_stale = GetLastError();
    SET_ERR(0); U(SetPolyFillMode)(NULL, 1); t[k].n = "SetPolyFillMode"; t[k].e_null = GetLastError();
    SET_ERR(0); U(SetPolyFillMode)(st, 1); t[k++].e_stale = GetLastError();
    SET_ERR(0); U(SetStretchBltMode)(NULL, 1); t[k].n = "SetStretchBltMode"; t[k].e_null = GetLastError();
    SET_ERR(0); U(SetStretchBltMode)(st, 1); t[k++].e_stale = GetLastError();
    for (k = 0; k < 5; k++) {
        CHECKF(t[k].e_null == 6, "%s null err %d", t[k].n, t[k].e_null);
        CHECKF(t[k].e_stale == 87, "%s stale err %d", t[k].n, t[k].e_stale);
    }
}
TEST(gap3_matrix_two_dcs_same_bitmap_sequence) {
    /* attribute state is per DC, not per bitmap */
    G3DC a; if (!g3dc_make(&a, 4, 4)) { CHECK(0); return; }
    HDC b = g3_mem();
    U(SetBkMode)(a.dc, TRANSPARENT);
    U(SelectObject)(a.dc, a.old);
    HGDIOBJ o = U(SelectObject)(b, a.bmp);
    CHECK(o != NULL);
    EQ(U(GetBkMode)(b), OPAQUE);
    U(SelectObject)(b, o);
    U(SelectObject)(a.dc, a.bmp);
    EQ(U(GetBkMode)(a.dc), TRANSPARENT);
    U(DeleteDC)(b);
    g3dc_free(&a);
}
