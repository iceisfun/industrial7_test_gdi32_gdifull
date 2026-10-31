/* Shared helpers for the drawing_prims area: DIBSection memory DCs and pixel scanning. */
#ifndef DP_H
#define DP_H
#include "t.h"

typedef struct {
    HDC dc; HBITMAP bmp, obmp; HGDIOBJ open, obr;
    BYTE *bits; int w, h, bpp, stride;
    HGDIOBJ own[1100]; int nown;
} DP;

#define SOFT(c) do { if (!(c)) t_note("soft %s:%d: %s", __FILE__, __LINE__, #c); } while (0)
#define SOFT_ERR(e) do { DWORD e_ = GetLastError(); if (e_ != (DWORD)(e)) t_note("soft %s:%d: last error %lu want %lu", __FILE__, __LINE__, e_, (DWORD)(e)); } while (0)

static int dp_wine(void) {
    HMODULE n = GetModuleHandleA("ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version") != NULL;
}
static int dp_have(void) {
    return U(CreateDIBSection) && U(CreateCompatibleDC) && U(SelectObject) && U(DeleteObject) && U(DeleteDC) &&
           U(CreatePen) && U(GdiFlush) && U(MoveToEx) && U(LineTo) && U(Rectangle) && U(GetStockObject) &&
           U(SetROP2) && U(PatBlt) && U(CreateSolidBrush) && U(GetCurrentPositionEx);
}
#define DP_NEED() do { if (!dp_have()) { t_skip("missing gdi exports"); return; } } while (0)
#define WCHECK(c, ...) do { if (dp_wine()) { if (!(c)) t_note("wine differs: %s:%d %s", __FILE__, __LINE__, #c); } else CHECKF(c, __VA_ARGS__); } while (0)
#define NOT_WINE(why) do { if (dp_wine()) { t_skip("wine: %s", why); return; } } while (0)

static int dp_open_pal(DP *d, int w, int h, int bpp, const RGBQUAD *pal, int npal) {
    struct { BITMAPINFOHEADER h; union { DWORD m[3]; RGBQUAD pal[256]; } u; } bi;
    memset(d, 0, sizeof *d);
    memset(&bi, 0, sizeof bi);
    int realbpp = bpp == 15 ? 16 : bpp;
    bi.h.biSize = sizeof bi.h; bi.h.biWidth = w; bi.h.biHeight = -h; bi.h.biPlanes = 1;
    bi.h.biBitCount = realbpp;
    if (bpp == 16) { bi.h.biCompression = BI_BITFIELDS; bi.u.m[0] = 0xF800; bi.u.m[1] = 0x07E0; bi.u.m[2] = 0x001F; }
    else if (bpp == 15) { bi.h.biCompression = BI_BITFIELDS; bi.u.m[0] = 0x7C00; bi.u.m[1] = 0x03E0; bi.u.m[2] = 0x001F; }
    else bi.h.biCompression = BI_RGB;
    UINT usage = DIB_RGB_COLORS;
    void *bits = NULL;
    BITMAPINFO *pbi = (BITMAPINFO *)&bi;
    if (bpp == 1 || bpp == 8) {
        int n = bpp == 1 ? 2 : 256;
        bi.h.biClrUsed = n;
        if (pal) memcpy(bi.u.pal, pal, (npal > n ? n : npal) * sizeof(RGBQUAD));
        else if (bpp == 1) { bi.u.pal[1].rgbRed = bi.u.pal[1].rgbGreen = bi.u.pal[1].rgbBlue = 255; }
        else for (int i = 0; i < 256; i++) bi.u.pal[i].rgbRed = bi.u.pal[i].rgbGreen = bi.u.pal[i].rgbBlue = i;
    }
    HDC dc = U(CreateCompatibleDC)(NULL);
    if (!dc) return 0;
    if (bpp == 16 || bpp == 15) {
        /* BITMAPINFOHEADER followed by three masks: layout already matches */
    }
    HBITMAP bm = U(CreateDIBSection)(dc, pbi, usage, &bits, NULL, 0);
    if (!bm || !bits) { U(DeleteDC)(dc); return 0; }
    d->dc = dc; d->bmp = bm; d->bits = bits; d->w = w; d->h = h; d->bpp = realbpp;
    d->stride = ((w * realbpp + 31) / 32) * 4;
    d->obmp = U(SelectObject)(dc, bm);
    d->open = U(GetCurrentObject)(dc, OBJ_PEN);
    d->obr = U(GetCurrentObject)(dc, OBJ_BRUSH);
    return 1;
}
static void dp_fill32(DP *d, COLORREF c) {
    DWORD v = (GetRValue(c) << 16) | (GetGValue(c) << 8) | GetBValue(c);
    U(GdiFlush)();
    for (int y = 0; y < d->h; y++) { DWORD *r = (DWORD *)(d->bits + y * d->stride); for (int x = 0; x < d->w; x++) r[x] = v; }
}
/* default open: white-cleared */
static int dp_open(DP *d, int w, int h, int bpp) {
    if (!dp_open_pal(d, w, h, bpp, NULL, 0)) return 0;
    if (bpp == 32) dp_fill32(d, RGB(255, 255, 255));
    else { U(PatBlt)(d->dc, 0, 0, w, h, WHITENESS); U(GdiFlush)(); }
    return 1;
}
static void dp_close(DP *d) {
    if (!d->dc) return;
    U(SelectObject)(d->dc, U(GetStockObject)(BLACK_PEN));
    U(SelectObject)(d->dc, U(GetStockObject)(WHITE_BRUSH));
    U(SelectObject)(d->dc, d->obmp);
    for (int i = 0; i < d->nown; i++) U(DeleteObject)(d->own[i]);
    U(DeleteObject)(d->bmp);
    U(DeleteDC)(d->dc);
    memset(d, 0, sizeof *d);
}
static HPEN dp_pen(DP *d, int style, int width, COLORREF c) {
    HPEN p = U(CreatePen)(style, width, c);
    if (p && d->nown < 1100) { d->own[d->nown++] = p; U(SelectObject)(d->dc, p); }
    return p;
}
static HPEN dp_xpen(DP *d, DWORD style, DWORD width, COLORREF c) {
    LOGBRUSH lb = { BS_SOLID, c, 0 };
    HPEN p = U(ExtCreatePen)(style, width, &lb, 0, NULL);
    if (p && d->nown < 1100) { d->own[d->nown++] = p; U(SelectObject)(d->dc, p); }
    return p;
}
static HBRUSH dp_brush(DP *d, COLORREF c) {
    HBRUSH b = U(CreateSolidBrush)(c);
    if (b && d->nown < 1100) { d->own[d->nown++] = b; U(SelectObject)(d->dc, b); }
    return b;
}
static void dp_stock(DP *d, int obj) { U(SelectObject)(d->dc, U(GetStockObject)(obj)); }
static void dp_black(DP *d) { dp_pen(d, PS_SOLID, 1, RGB(0, 0, 0)); }

/* raw pixel value (index / 565 word / 0xRRGGBB) */
static DWORD dp_raw(DP *d, int x, int y) {
    U(GdiFlush)();
    if (x < 0 || y < 0 || x >= d->w || y >= d->h) return 0xDEADBEEF;
    BYTE *r = d->bits + y * d->stride;
    switch (d->bpp) {
    case 32: return ((DWORD *)r)[x] & 0xFFFFFF;
    case 24: return (r[x * 3 + 2] << 16) | (r[x * 3 + 1] << 8) | r[x * 3];
    case 16: return ((WORD *)r)[x];
    case 8: return r[x];
    case 1: return (r[x >> 3] >> (7 - (x & 7))) & 1;
    }
    return 0;
}
static COLORREF dp_px(DP *d, int x, int y) { /* 32bpp only */
    DWORD v = dp_raw(d, x, y);
    return RGB((v >> 16) & 255, (v >> 8) & 255, v & 255);
}
#define WHITE_C RGB(255, 255, 255)
#define BLACK_C RGB(0, 0, 0)
#define RED_C   RGB(255, 0, 0)
#define GREEN_C RGB(0, 255, 0)
#define BLUE_C  RGB(0, 0, 255)

static int dp_cnt(DP *d, DWORD raw) {
    int n = 0;
    for (int y = 0; y < d->h; y++) for (int x = 0; x < d->w; x++) if (dp_raw(d, x, y) == raw) n++;
    return n;
}
static DWORD dp_craw(COLORREF c) { return (GetRValue(c) << 16) | (GetGValue(c) << 8) | GetBValue(c); }
static int dp_cntc(DP *d, COLORREF c) { return dp_cnt(d, dp_craw(c)); }
/* count pixels != bg, bbox in rc (right/bottom exclusive); rc empty (0,0,0,0) if none */
static int dp_bbox(DP *d, DWORD bg, RECT *rc) {
    int n = 0, x0 = 1 << 30, y0 = 1 << 30, x1 = -1, y1 = -1;
    for (int y = 0; y < d->h; y++) for (int x = 0; x < d->w; x++)
        if (dp_raw(d, x, y) != bg) { n++; if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
    if (rc) { if (n) { rc->left = x0; rc->top = y0; rc->right = x1 + 1; rc->bottom = y1 + 1; } else memset(rc, 0, sizeof *rc); }
    return n;
}
static int dp_ink(DP *d, RECT *rc) { return dp_bbox(d, 0xFFFFFF, rc); }  /* 32bpp, white bg */
/* number of set pixels in a row / column span (32bpp white bg) */
static int dp_row(DP *d, int y, int x0, int x1) { int n = 0; for (int x = x0; x < x1; x++) if (dp_raw(d, x, y) != 0xFFFFFF) n++; return n; }
static int dp_col(DP *d, int x, int y0, int y1) { int n = 0; for (int y = y0; y < y1; y++) if (dp_raw(d, x, y) != 0xFFFFFF) n++; return n; }
static void dp_clr(DP *d) { dp_fill32(d, WHITE_C); }
static int dp_inside(DP *d, int x, int y) { return dp_raw(d, x, y) != 0xFFFFFF; }
/* mirror symmetry mismatch count about vertical / horizontal axis of box [l,r)x[t,b) */
static int dp_mirx(DP *d, int l, int t, int r, int b) {
    int n = 0;
    for (int y = t; y < b; y++) for (int x = l; x < r; x++)
        if ((dp_raw(d, x, y) != 0xFFFFFF) != (dp_raw(d, l + r - 1 - x, y) != 0xFFFFFF)) n++;
    return n;
}
static int dp_miry(DP *d, int l, int t, int r, int b) {
    int n = 0;
    for (int y = t; y < b; y++) for (int x = l; x < r; x++)
        if ((dp_raw(d, x, y) != 0xFFFFFF) != (dp_raw(d, x, t + b - 1 - y) != 0xFFFFFF)) n++;
    return n;
}
/* bitmap of ink for comparisons */
static void dp_snap(DP *d, BYTE *out) { for (int y = 0; y < d->h; y++) for (int x = 0; x < d->w; x++) out[y * d->w + x] = dp_raw(d, x, y) != 0xFFFFFF; }
static int dp_diff(DP *d, const BYTE *a) { int n = 0; for (int y = 0; y < d->h; y++) for (int x = 0; x < d->w; x++) if ((dp_raw(d, x, y) != 0xFFFFFF) != !!a[y * d->w + x]) n++; return n; }

static POINT dp_cur(DP *d) { POINT p = { -777, -777 }; U(GetCurrentPositionEx)(d->dc, &p); return p; }
static BOOL dp_line(DP *d, int x0, int y0, int x1, int y1) { U(MoveToEx)(d->dc, x0, y0, NULL); return U(LineTo)(d->dc, x1, y1); }
#endif
