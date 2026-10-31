/* helpers local to the blt_rops area */
#ifndef BLT_H
#define BLT_H
#include "t.h"
#include <stdlib.h>

#ifndef CAPTUREBLT
#define CAPTUREBLT 0x40000000
#endif
#ifndef NOMIRRORBITMAP
#define NOMIRRORBITMAP 0x80000000
#endif
#ifndef LAYOUT_RTL
#define LAYOUT_RTL 1
#endif
#ifndef LAYOUT_BITMAPORIENTATIONPRESERVED
#define LAYOUT_BITMAPORIENTATIONPRESERVED 8
#endif

UDECL(BOOL, GdiTransparentBlt, (HDC, int, int, int, int, HDC, int, int, int, int, UINT));
UDECL(BOOL, GdiAlphaBlend, (HDC, int, int, int, int, HDC, int, int, int, int, BLENDFUNCTION));
UDECL(BOOL, GdiGradientFill, (HDC, PTRIVERTEX, ULONG, PVOID, ULONG, ULONG));

#define BLT_FUNCS(X) \
    X(BitBlt) X(PatBlt) X(StretchBlt) X(MaskBlt) X(PlgBlt) X(CreateCompatibleDC) X(DeleteDC) \
    X(CreateDIBSection) X(SelectObject) X(DeleteObject) X(CreateSolidBrush) X(CreatePatternBrush) \
    X(CreateBitmap) X(CreateCompatibleBitmap) X(SetBkColor) X(GetBkColor) X(SetTextColor) X(SetStretchBltMode) \
    X(GetStretchBltMode) X(SetBrushOrgEx) X(GetBrushOrgEx) X(SelectClipRgn) X(CreateRectRgn) \
    X(SetMapMode) X(SetWindowExtEx) X(SetViewportExtEx) X(SetViewportOrgEx) X(SetWindowOrgEx) X(SetLayout) X(GetLayout) \
    X(GdiFlush) X(GetPixel) X(SetPixel) X(SetBitmapBits) X(GetBitmapBits) X(GetDeviceCaps) X(SaveDC) X(RestoreDC) \
    X(GetCurrentObject) X(GetObjectA) X(CombineRgn) X(CreateHatchBrush) X(GetLayout) X(GetDC) X(ReleaseDC) X(GetGuiResources) X(SetDIBColorTable) X(GetStockObject) X(GetObjectType) X(SetBkMode) X(SetROP2) X(CreateDIBitmap) X(GetDIBits)
#define BLT_DECL(n) static __typeof__(&n) p##n;
BLT_FUNCS(BLT_DECL)
static GdiTransparentBlt_pfn pGdiTransparentBlt;
static GdiAlphaBlend_pfn pGdiAlphaBlend;
static GdiGradientFill_pfn pGdiGradientFill;

static int blt_have(void) {
    int ok = 1;
#define BLT_INIT(n) p##n = U(n); if (!p##n) { t_skip("missing export: " #n); ok = 0; }
    BLT_FUNCS(BLT_INIT)
    pGdiTransparentBlt = UN(GdiTransparentBlt);
    pGdiAlphaBlend = UN(GdiAlphaBlend);
    pGdiGradientFill = UN(GdiGradientFill);
    return ok;
}
#define NEED() do { if (!blt_have()) return; } while (0)

static int blt_is_wine(void) {
    return GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version") != NULL;
}

/* ---- DIBSection-backed memory DC ---- */
typedef struct BD {
    HDC dc; HBITMAP bm, old; BYTE *raw; int w, h, bpp, stride;
} BD;

typedef struct { BITMAPINFOHEADER h; RGBQUAD c[256]; } BI256;

static COLORREF blt_palcol(int bpp, int i) {
    if (bpp == 1) return i ? RGB(255, 255, 255) : RGB(0, 0, 0);
    if (bpp == 4) { if (i == 0) return 0; if (i == 15) return RGB(255, 255, 255); return RGB(i * 16, (i * 37) & 255, (i * 91) & 255); }
    if (i == 0) return 0;
    if (i == 255) return RGB(255, 255, 255);
    return RGB(i, (i * 37) & 255, (i * 91) & 255);
}

static BD bd_new(int bpp, int w, int h) {
    BD b; BI256 bi; void *bits = NULL; HDC sdc;
    memset(&b, 0, sizeof b); memset(&bi, 0, sizeof bi);
    bi.h.biSize = sizeof(BITMAPINFOHEADER); bi.h.biWidth = w; bi.h.biHeight = -h; bi.h.biPlanes = 1;
    bi.h.biBitCount = (WORD)bpp; bi.h.biCompression = BI_RGB;
    if (bpp <= 8) {
        int n = 1 << bpp, i;
        for (i = 0; i < n; i++) { COLORREF c = blt_palcol(bpp, i); bi.c[i].rgbRed = GetRValue(c); bi.c[i].rgbGreen = GetGValue(c); bi.c[i].rgbBlue = GetBValue(c); }
        bi.h.biClrUsed = n;
    }
    b.dc = pCreateCompatibleDC(NULL);
    b.bm = pCreateDIBSection(b.dc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    b.raw = bits; b.w = w; b.h = h; b.bpp = bpp; b.stride = ((w * bpp + 31) / 32) * 4;
    if (b.bm) b.old = pSelectObject(b.dc, b.bm);
    if (bits) memset(bits, 0, (size_t)b.stride * h);
    (void)sdc;
    return b;
}
/* 16bpp 555 (BI_RGB) or 565 (BI_BITFIELDS) */
static BD bd_new16(int w, int h, int is565) {
    struct { BITMAPINFOHEADER h; DWORD m[3]; } bi; BD b; void *bits = NULL;
    memset(&b, 0, sizeof b); memset(&bi, 0, sizeof bi);
    bi.h.biSize = sizeof(BITMAPINFOHEADER); bi.h.biWidth = w; bi.h.biHeight = -h; bi.h.biPlanes = 1; bi.h.biBitCount = 16;
    if (is565) { bi.h.biCompression = BI_BITFIELDS; bi.m[0] = 0xF800; bi.m[1] = 0x07E0; bi.m[2] = 0x001F; }
    b.dc = pCreateCompatibleDC(NULL);
    b.bm = pCreateDIBSection(b.dc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    b.raw = bits; b.w = w; b.h = h; b.bpp = 16; b.stride = ((w * 16 + 31) / 32) * 4;
    if (b.bm) b.old = pSelectObject(b.dc, b.bm);
    if (bits) memset(bits, 0, (size_t)b.stride * h);
    return b;
}
static void bd_free(BD *b) {
    if (b->dc) { if (b->old) pSelectObject(b->dc, b->old); }
    if (b->bm) pDeleteObject(b->bm);
    if (b->dc) pDeleteDC(b->dc);
    memset(b, 0, sizeof *b);
}
static int bd_ok(BD *b) { return b->dc && b->bm && b->raw; }

static DWORD bd_get(const BD *b, int x, int y) {
    const BYTE *r = b->raw + (size_t)y * b->stride;
    switch (b->bpp) {
    case 32: return ((const DWORD *)r)[x];
    case 24: return r[x * 3] | (r[x * 3 + 1] << 8) | (r[x * 3 + 2] << 16);
    case 16: return ((const WORD *)r)[x];
    case 8: return r[x];
    case 4: return (x & 1) ? (r[x / 2] & 15) : (r[x / 2] >> 4);
    default: return (r[x / 8] >> (7 - (x & 7))) & 1;
    }
}
static void bd_put(BD *b, int x, int y, DWORD v) {
    BYTE *r = b->raw + (size_t)y * b->stride;
    switch (b->bpp) {
    case 32: ((DWORD *)r)[x] = v; break;
    case 24: r[x * 3] = v; r[x * 3 + 1] = v >> 8; r[x * 3 + 2] = v >> 16; break;
    case 16: ((WORD *)r)[x] = (WORD)v; break;
    case 8: r[x] = (BYTE)v; break;
    case 4: if (x & 1) r[x / 2] = (r[x / 2] & 0xF0) | (v & 15); else r[x / 2] = (r[x / 2] & 0x0F) | ((v & 15) << 4); break;
    default: if (v & 1) r[x / 8] |= 0x80 >> (x & 7); else r[x / 8] &= ~(0x80 >> (x & 7)); break;
    }
}
static void bd_clear(BD *b, DWORD v) { int x, y; for (y = 0; y < b->h; y++) for (x = 0; x < b->w; x++) bd_put(b, x, y, v); }
static DWORD bd_mask(int bpp) { return bpp >= 24 ? 0xFFFFFF : bpp == 16 ? 0x7FFF : (1u << bpp) - 1; }
/* 0x00RRGGBB pixel value for a COLORREF */
static DWORD px(COLORREF c) { return (GetRValue(c) << 16) | (GetGValue(c) << 8) | GetBValue(c); }
static COLORREF cr(DWORD p) { return RGB((p >> 16) & 255, (p >> 8) & 255, p & 255); }

static DWORD g_seed = 12345;
static DWORD rnd(void) { g_seed ^= g_seed << 13; g_seed ^= g_seed >> 17; g_seed ^= g_seed << 5; return g_seed; }

/* Reference ternary raster operation: code is the high-word byte of the ROP3. */
static DWORD rop3(unsigned code, DWORD P, DWORD S, DWORD D) {
    DWORD r = 0; int i;
    for (i = 0; i < 8; i++) {
        if (code & (1u << i)) {
            DWORD m = ((i & 4) ? P : ~P) & ((i & 2) ? S : ~S) & ((i & 1) ? D : ~D);
            r |= m;
        }
    }
    return r;
}
#define ROP(code) ((DWORD)(code) << 16)

static int blt_cmp(const BD *b, int x0, int y0, int w, int h, DWORD (*fn)(int, int, void *), void *u, DWORD mask, int *bx, int *by, DWORD *got, DWORD *want) {
    int x, y;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        DWORD g = bd_get(b, x0 + x, y0 + y) & mask, e = fn(x, y, u) & mask;
        if (g != e) { *bx = x0 + x; *by = y0 + y; *got = g; *want = e; return 0; }
    }
    return 1;
}
/* check a rect of a BD for a constant (masked) value */
#define CHECK_RECT_VAL(b, x0, y0, w, h, v, mask) do { \
    int bx_ = -1, by_ = -1, x_, y_; DWORD g_ = 0, e_ = (DWORD)(v) & (mask); int ok_ = 1; \
    for (y_ = (y0); y_ < (y0) + (h) && ok_; y_++) for (x_ = (x0); x_ < (x0) + (w); x_++) { \
        DWORD gg_ = bd_get(&(b), x_, y_) & (mask); if (gg_ != e_) { ok_ = 0; bx_ = x_; by_ = y_; g_ = gg_; break; } } \
    CHECKF(ok_, "first diff at (%d,%d) got 0x%lx want 0x%lx", bx_, by_, (unsigned long)g_, (unsigned long)e_); } while (0)
#define CHECK_PX(b, x, y, v, mask) CHECKF((bd_get(&(b), (x), (y)) & (mask)) == ((DWORD)(v) & (mask)), "pixel (%d,%d) got 0x%lx want 0x%lx", (int)(x), (int)(y), (unsigned long)(bd_get(&(b), (x), (y)) & (mask)), (unsigned long)((DWORD)(v) & (mask)))

/* fill 32bpp image with unique per-pixel values: x,y encoded */
static DWORD enc(int x, int y) { return (((x * 7 + 1) & 255) << 16) | (((y * 5 + 2) & 255) << 8) | ((x * 3 + y * 11 + 9) & 255); }
static void bd_fill_enc(BD *b) { int x, y; for (y = 0; y < b->h; y++) for (x = 0; x < b->w; x++) bd_put(b, x, y, enc(x, y)); }
#endif
