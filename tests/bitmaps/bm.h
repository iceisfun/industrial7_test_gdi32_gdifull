/* Shared helpers for the bitmaps area (all static). */
#ifndef BM_H
#define BM_H
#include "t.h"

#define BM_DEFAULT_BITMAP 21
#define BM_CLR_INVALID 0xFFFFFFFFu

static int bm_is_wine(void) {
    return GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version") != NULL;
}

/* Require the common set of exports. */
#define BM_REQ() do { \
    REQUIRE(U(CreateBitmap)); REQUIRE(U(CreateBitmapIndirect)); REQUIRE(U(CreateCompatibleBitmap)); \
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(SelectObject)); REQUIRE(U(DeleteObject)); REQUIRE(U(DeleteDC)); \
    REQUIRE(U(GetObjectA)); REQUIRE(U(GetObjectType)); REQUIRE(U(GetStockObject)); } while (0)
#define BM_REQ_DIB() do { BM_REQ(); REQUIRE(U(CreateDIBSection)); REQUIRE(U(GdiFlush)); } while (0)
#define BM_REQ_PIX() do { BM_REQ_DIB(); REQUIRE(U(GetPixel)); REQUIRE(U(SetPixel)); REQUIRE(U(SetPixelV)); } while (0)

static int bm_row_bytes(int w, int bpp) { return ((w * bpp + 15) >> 4) * 2; }

static unsigned bm_gui_count(void) {
    DWORD (WINAPI *g)(HANDLE, DWORD) = (void *)t_proc("GetGuiResources");
    return g ? g(GetCurrentProcess(), 0) : 0;
}

/* A DIBSection selected into its own memory DC. */
typedef struct { HDC dc; HBITMAP bm, old; unsigned char *bits; int w, h, bpp; } dib_t;

/* mode: 0 = default (BI_RGB); 1 = 16bpp 565 bitfields; 2 = 32bpp bitfields (default masks); bit 8: top-down.
 * pal: optional RGBQUAD table (n entries) for <=8bpp, else NULL for a black/white-ish default. */
static int dib_open_ex(dib_t *d, int w, int h, int bpp, int mode, const RGBQUAD *pal, int npal) {
    struct { BITMAPINFOHEADER h; RGBQUAD c[256]; } bi;
    memset(&bi, 0, sizeof bi);
    memset(d, 0, sizeof *d);
    bi.h.biSize = sizeof(BITMAPINFOHEADER);
    bi.h.biWidth = w; bi.h.biHeight = (mode & 0x100) ? -h : h;
    bi.h.biPlanes = 1; bi.h.biBitCount = bpp; bi.h.biCompression = BI_RGB;
    int m = mode & 0xff;
    if (m == 1 && bpp == 16) {
        bi.h.biCompression = BI_BITFIELDS;
        DWORD *mk = (DWORD *)bi.c; mk[0] = 0xF800; mk[1] = 0x07E0; mk[2] = 0x001F;
    } else if (m == 2 && bpp == 32) {
        bi.h.biCompression = BI_BITFIELDS;
        DWORD *mk = (DWORD *)bi.c; mk[0] = 0xFF0000; mk[1] = 0x00FF00; mk[2] = 0x0000FF;
    } else if (bpp <= 8) {
        int n = 1 << bpp, i;
        bi.h.biClrUsed = n;
        if (pal) for (i = 0; i < n && i < npal; i++) bi.c[i] = pal[i];
        else if (bpp == 1) { bi.c[1].rgbRed = bi.c[1].rgbGreen = bi.c[1].rgbBlue = 255; }
    }
    d->dc = U(CreateCompatibleDC)(NULL);
    if (!d->dc) return 0;
    void *bits = NULL;
    d->bm = U(CreateDIBSection)(d->dc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!d->bm) { U(DeleteDC)(d->dc); d->dc = NULL; return 0; }
    d->bits = bits; d->w = w; d->h = h; d->bpp = bpp;
    d->old = U(SelectObject)(d->dc, d->bm);
    return 1;
}
static int dib_open(dib_t *d, int w, int h, int bpp) { return dib_open_ex(d, w, h, bpp, 0, NULL, 0); }
static void dib_close(dib_t *d) {
    if (d->dc) { if (d->old) U(SelectObject)(d->dc, d->old); U(DeleteDC)(d->dc); }
    if (d->bm) U(DeleteObject)(d->bm);
    memset(d, 0, sizeof *d);
}
static void dib_fill32(dib_t *d, DWORD v) {
    DWORD *p = (DWORD *)d->bits; int i;
    for (i = 0; i < d->w * d->h; i++) p[i] = v;
}
static DWORD dib_px32(dib_t *d, int x, int y) {  /* y in memory order (row 0 of bits) */
    U(GdiFlush)();
    return ((DWORD *)d->bits)[y * d->w + x];
}
/* Memory DC with a DDB selected. */
typedef struct { HDC dc; HBITMAP bm, old; } ddb_t;
static int ddb_open(ddb_t *d, int w, int h, int planes, int bpp, const void *bits) {
    memset(d, 0, sizeof *d);
    d->dc = U(CreateCompatibleDC)(NULL);
    if (!d->dc) return 0;
    d->bm = U(CreateBitmap)(w, h, planes, bpp, bits);
    if (!d->bm) { U(DeleteDC)(d->dc); d->dc = NULL; return 0; }
    d->old = U(SelectObject)(d->dc, d->bm);
    return 1;
}
static void ddb_close(ddb_t *d) {
    if (d->dc) { if (d->old) U(SelectObject)(d->dc, d->old); U(DeleteDC)(d->dc); }
    if (d->bm) U(DeleteObject)(d->bm);
    memset(d, 0, sizeof *d);
}
static int bm_getobj(HBITMAP h, BITMAP *bm) {
    memset(bm, 0xCC, sizeof *bm);
    return U(GetObjectA)(h, sizeof *bm, bm);
}
#endif
