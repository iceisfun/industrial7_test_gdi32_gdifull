/* Shared helpers for the regions area (static, header-only). */
#ifndef RG_H
#define RG_H
#include "t.h"
#include <stdlib.h>

#ifndef NULLREGION
#define NULLREGION 1
#endif

static int rg_is_wine(void) {
    HMODULE n = GetModuleHandleA("ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version") != NULL;
}

typedef struct { HDC dc; HBITMAP bmp, old; DWORD *bits; int w, h; } RgDib;

/* 32bpp top-down DIBSection memory DC filled with 'fill' (0x00RRGGBB). */
static int rg_dib(RgDib *d, int w, int h, DWORD fill) {
    memset(d, 0, sizeof *d);
    if (!U(CreateCompatibleDC) || !U(CreateDIBSection) || !U(SelectObject)) return 0;
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL;
    d->dc = U(CreateCompatibleDC)(NULL);
    if (!d->dc) return 0;
    d->bmp = U(CreateDIBSection)(d->dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!d->bmp || !bits) return 0;
    d->old = (HBITMAP)U(SelectObject)(d->dc, d->bmp);
    d->bits = (DWORD *)bits; d->w = w; d->h = h;
    for (int i = 0; i < w * h; i++) d->bits[i] = fill;
    return 1;
}
static void rg_dib_free(RgDib *d) {
    if (d->dc && d->old) U(SelectObject)(d->dc, d->old);
    if (d->bmp) U(DeleteObject)(d->bmp);
    if (d->dc) U(DeleteDC)(d->dc);
    memset(d, 0, sizeof *d);
}
static void rg_clear(RgDib *d, DWORD fill) { for (int i = 0; i < d->w * d->h; i++) d->bits[i] = fill; }
static DWORD rg_px(RgDib *d, int x, int y) { return d->bits[y * d->w + x] & 0xFFFFFF; }
static int rg_count(RgDib *d, DWORD c) {
    int n = 0; for (int i = 0; i < d->w * d->h; i++) if ((d->bits[i] & 0xFFFFFF) == c) n++; return n;
}
/* number of pixels equal to c inside [l,r)x[t,b) */
static int rg_count_in(RgDib *d, DWORD c, int l, int t, int r, int b) {
    int n = 0;
    for (int y = t; y < b; y++) for (int x = l; x < r; x++)
        if (x >= 0 && y >= 0 && x < d->w && y < d->h && rg_px(d, x, y) == c) n++;
    return n;
}
/* true if every pixel inside rect equals c */
static int rg_all_in(RgDib *d, DWORD c, int l, int t, int r, int b) {
    return rg_count_in(d, c, l, t, r, b) == (r - l) * (b - t);
}

static HRGN rg_rect(int l, int t, int r, int b) { return U(CreateRectRgn)(l, t, r, b); }
static int rg_type(HRGN h) { RECT rc; return U(GetRgnBox)(h, &rc); }

/* grid snapshot of a region over [0,N)x[0,N) via PtInRegion */
#define RG_N 64
typedef struct { unsigned char c[RG_N][RG_N]; } RgGrid;
static void rg_grid_of(HRGN h, RgGrid *g) {
    for (int y = 0; y < RG_N; y++) for (int x = 0; x < RG_N; x++) g->c[y][x] = U(PtInRegion)(h, x, y) ? 1 : 0;
}
static void rg_grid_rect(RgGrid *g, int l, int t, int r, int b) {
    for (int y = t; y < b; y++) for (int x = l; x < r; x++)
        if (x >= 0 && y >= 0 && x < RG_N && y < RG_N) g->c[y][x] = 1;
}
static int rg_grid_cnt(const RgGrid *g) { int n = 0; for (int y = 0; y < RG_N; y++) for (int x = 0; x < RG_N; x++) n += g->c[y][x]; return n; }
static int rg_grid_eq(const RgGrid *a, const RgGrid *b) { return !memcmp(a, b, sizeof *a); }
/* bounding box of grid; returns 0 if empty */
static int rg_grid_bbox(const RgGrid *g, RECT *rc) {
    int l = RG_N, t = RG_N, r = -1, b = -1;
    for (int y = 0; y < RG_N; y++) for (int x = 0; x < RG_N; x++) if (g->c[y][x]) {
        if (x < l) l = x; if (x > r) r = x; if (y < t) t = y; if (y > b) b = y; }
    if (r < 0) { memset(rc, 0, sizeof *rc); return 0; }
    rc->left = l; rc->top = t; rc->right = r + 1; rc->bottom = b + 1; return 1;
}
/* expected region type of a grid: 1 empty, 2 single rect, 3 otherwise */
static int rg_grid_type(const RgGrid *g) {
    RECT bb; if (!rg_grid_bbox(g, &bb)) return NULLREGION;
    int area = (bb.right - bb.left) * (bb.bottom - bb.top);
    return rg_grid_cnt(g) == area ? SIMPLEREGION : COMPLEXREGION;
}

static unsigned rg_seed;
static unsigned rg_rand(void) { rg_seed = rg_seed * 1103515245u + 12345u; return (rg_seed >> 16) & 0x7FFF; }

/* region data fetch (malloc'd) */
static RGNDATA *rg_getdata(HRGN h, DWORD *size) {
    DWORD n = U(GetRegionData)(h, 0, NULL);
    if (!n) { if (size) *size = 0; return NULL; }
    RGNDATA *d = (RGNDATA *)malloc(n);
    DWORD r = U(GetRegionData)(h, n, d);
    if (size) *size = r;
    if (!r) { free(d); return NULL; }
    return d;
}
static RECT *rg_rects(RGNDATA *d) { return (RECT *)d->Buffer; }
#endif
