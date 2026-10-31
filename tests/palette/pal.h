/* Shared static helpers for the palette area. */
#ifndef PAL_H
#define PAL_H
#include "t.h"
#include <stdlib.h>

static int pal_is_wine(void) {
    HMODULE h = GetModuleHandleA("ntdll.dll");
    return h && GetProcAddress(h, "wine_get_version") != NULL;
}

/* Build a palette of n entries: entry i = (i, 3i, 7i) with the given flags. */
static HPALETTE pal_make_flags(unsigned n, BYTE flags) {
    LOGPALETTE *lp = (LOGPALETTE *)calloc(1, sizeof(LOGPALETTE) + (n ? n - 1 : 0) * sizeof(PALETTEENTRY) + 8);
    HPALETTE h;
    unsigned i;
    if (!lp || !U(CreatePalette)) return NULL;
    lp->palVersion = 0x300;
    lp->palNumEntries = (WORD)n;
    for (i = 0; i < n; i++) {
        lp->palPalEntry[i].peRed = (BYTE)i;
        lp->palPalEntry[i].peGreen = (BYTE)(i * 3);
        lp->palPalEntry[i].peBlue = (BYTE)(i * 7);
        lp->palPalEntry[i].peFlags = flags;
    }
    h = U(CreatePalette)(lp);
    free(lp);
    return h;
}
static HPALETTE pal_make(unsigned n) { return pal_make_flags(n, 0); }

/* Palette from explicit COLORREF-like triples (flags 0). */
static HPALETTE pal_make_rgb(const COLORREF *c, unsigned n) {
    LOGPALETTE *lp = (LOGPALETTE *)calloc(1, sizeof(LOGPALETTE) + n * sizeof(PALETTEENTRY));
    HPALETTE h;
    unsigned i;
    lp->palVersion = 0x300;
    lp->palNumEntries = (WORD)n;
    for (i = 0; i < n; i++) {
        lp->palPalEntry[i].peRed = GetRValue(c[i]);
        lp->palPalEntry[i].peGreen = GetGValue(c[i]);
        lp->palPalEntry[i].peBlue = GetBValue(c[i]);
    }
    h = U(CreatePalette)(lp);
    free(lp);
    return h;
}

static int pal_count(HPALETTE h) {
    return (int)U(GetPaletteEntries)(h, 0, 0, NULL);
}

static DWORD gdi_count(void) {
    if (!U(GetGuiResources)) return 0;
    return U(GetGuiResources)(GetCurrentProcess(), 0 /* GR_GDIOBJECTS */);
}

/* Memory DC with a DIB section. bpp 8 gets a grey ramp color table. */
struct dibdc {
    HDC dc;
    HBITMAP bmp, old;
    BYTE *bits;
    int w, h, stride;
};

static int dibdc_create(struct dibdc *d, int bpp, int w, int h) {
    struct { BITMAPINFOHEADER hdr; RGBQUAD pal[256]; } bi;
    HDC dc;
    void *bits = NULL;
    memset(d, 0, sizeof *d);
    if (!U(CreateCompatibleDC) || !U(CreateDIBSection) || !U(SelectObject)) return 0;
    memset(&bi, 0, sizeof bi);
    bi.hdr.biSize = sizeof(BITMAPINFOHEADER);
    bi.hdr.biWidth = w;
    bi.hdr.biHeight = -h;
    bi.hdr.biPlanes = 1;
    bi.hdr.biBitCount = (WORD)bpp;
    bi.hdr.biCompression = BI_RGB;
    if (bpp <= 8) {
        int i, n = 1 << bpp;
        bi.hdr.biClrUsed = n;
        for (i = 0; i < n; i++) {
            bi.pal[i].rgbRed = bi.pal[i].rgbGreen = bi.pal[i].rgbBlue = (BYTE)(bpp == 8 ? i : (i ? 255 : 0));
        }
    }
    dc = U(CreateCompatibleDC)(NULL);
    if (!dc) return 0;
    d->bmp = U(CreateDIBSection)(dc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!d->bmp) { U(DeleteDC)(dc); return 0; }
    d->dc = dc;
    d->old = (HBITMAP)U(SelectObject)(dc, d->bmp);
    d->bits = (BYTE *)bits;
    d->w = w; d->h = h;
    d->stride = ((w * bpp + 31) / 32) * 4;
    return 1;
}
static void dibdc_free(struct dibdc *d) {
    if (!d->dc) return;
    U(SelectObject)(d->dc, d->old);
    U(DeleteObject)(d->bmp);
    U(DeleteDC)(d->dc);
    memset(d, 0, sizeof *d);
}

/* Set the whole DIB color table to the given colors (rest black). */
static UINT dib_set_table(HDC dc, const COLORREF *c, unsigned n, unsigned start) {
    RGBQUAD q[256];
    unsigned i;
    memset(q, 0, sizeof q);
    for (i = 0; i < n; i++) {
        q[i].rgbRed = GetRValue(c[i]); q[i].rgbGreen = GetGValue(c[i]); q[i].rgbBlue = GetBValue(c[i]);
    }
    return U(SetDIBColorTable)(dc, start, n, q);
}

static HDC pal_screen_dc(void) { return U(GetDC) ? U(GetDC)(NULL) : NULL; }
static void pal_release_screen_dc(HDC dc) { if (dc && U(ReleaseDC)) U(ReleaseDC)(NULL, dc); }

/* single-evaluation versions (call sites often pass function calls) */
#undef EXPECT_EQ
#undef EXPECT_NE
#define EXPECT_EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)
#define EXPECT_NE(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ != b_, __FILE__, __LINE__, #a " != " #b, "got 0x%llx", (unsigned long long)a_); } while (0)

static void dib_clear_table(HDC dc) {
    RGBQUAD q[256];
    memset(q, 0, sizeof q);
    U(SetDIBColorTable)(dc, 0, 256, q);
}

#define PAL_BOGUS_HANDLE ((HANDLE)(ULONG_PTR)0x7fff1234)
#endif
