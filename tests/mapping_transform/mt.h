#ifndef MT_H
#define MT_H
#include "t.h"
#include <limits.h>

#ifndef MWT_SET
#define MWT_SET 4
#endif
#ifndef GM_COMPATIBLE
#define GM_COMPATIBLE 1
#define GM_ADVANCED 2
#endif
#ifndef LAYOUT_BITMAPORIENTATIONPRESERVED
#define LAYOUT_BITMAPORIENTATIONPRESERVED 8
#endif
#ifndef GDI_ERROR
#define GDI_ERROR 0xFFFFFFFFUL
#endif

static int mt_is_wine(void)
{
    HMODULE n = GetModuleHandleA("ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version") != NULL;
}

static HDC mt_dc(void) { return U(CreateCompatibleDC)(NULL); }
static void mt_free_dc(HDC dc) { if (dc) U(DeleteDC)(dc); }

typedef struct { HDC dc; HBITMAP bmp, old; unsigned *bits; int w, h; } mt_dib;

/* 32bpp top-down DIB section selected into a fresh memory DC, zero-filled. */
static int mt_dib_make(mt_dib *d, int w, int h)
{
    BITMAPINFO bi; void *bits = NULL;
    memset(d, 0, sizeof *d);
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    d->dc = mt_dc();
    if (!d->dc) return 0;
    d->bmp = U(CreateDIBSection)(d->dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!d->bmp || !bits) { mt_free_dc(d->dc); d->dc = NULL; return 0; }
    d->old = (HBITMAP)U(SelectObject)(d->dc, d->bmp);
    d->bits = bits; d->w = w; d->h = h;
    memset(bits, 0, (size_t)w * h * 4);
    return 1;
}
static void mt_dib_free(mt_dib *d)
{
    if (!d->dc) return;
    U(SelectObject)(d->dc, d->old);
    U(DeleteObject)(d->bmp);
    mt_free_dc(d->dc);
    d->dc = NULL;
}
static void mt_dib_clear(mt_dib *d)
{
    U(GdiFlush)();
    memset(d->bits, 0, (size_t)d->w * d->h * 4);
}
/* pixel (RGB, alpha ignored); -1 when outside */
static long mt_px(mt_dib *d, int x, int y)
{
    if (x < 0 || y < 0 || x >= d->w || y >= d->h) return -1;
    return (long)(d->bits[y * d->w + x] & 0xFFFFFF);
}
static int mt_count(mt_dib *d)
{
    int n = 0, i;
    U(GdiFlush)();
    for (i = 0; i < d->w * d->h; i++) if (d->bits[i] & 0xFFFFFF) n++;
    return n;
}
static int mt_bbox(mt_dib *d, RECT *r)
{
    int x, y, any = 0;
    U(GdiFlush)();
    SetRect(r, d->w, d->h, -1, -1);
    for (y = 0; y < d->h; y++) for (x = 0; x < d->w; x++)
        if (d->bits[y * d->w + x] & 0xFFFFFF) {
            any = 1;
            if (x < r->left) r->left = x;
            if (y < r->top) r->top = y;
            if (x > r->right) r->right = x;
            if (y > r->bottom) r->bottom = y;
        }
    return any;
}
/* pixel set test, flushes first */
static int mt_set(mt_dib *d, int x, int y) { U(GdiFlush)(); return mt_px(d, x, y) > 0; }

static int mt_near(long got, long want, long pct_x10, long abs_tol)
{
    long diff = got - want; if (diff < 0) diff = -diff;
    long w = want < 0 ? -want : want;
    return diff <= abs_tol || diff * 1000 <= w * pct_x10;
}

#define MT_EQ(a, b) do { long long a_ = (long long)(a), b_ = (long long)(b); CHECKF(a_ == b_, #a " == " #b " got %lld want %lld", a_, b_); } while (0)
#define EXT_IS(dc, fn, cx_, cy_) do { SIZE s_ = {0x7777, 0x7777}; \
    BOOL r_ = U(fn)((dc), &s_); \
    CHECKF(r_ && s_.cx == (cx_) && s_.cy == (cy_), #fn " ok=%d got %ld,%ld want %ld,%ld", r_, s_.cx, s_.cy, (long)(cx_), (long)(cy_)); } while (0)
#define ORG_IS(dc, fn, x_, y_) do { POINT p_ = {0x7777, 0x7777}; \
    BOOL r_ = U(fn)((dc), &p_); \
    CHECKF(r_ && p_.x == (x_) && p_.y == (y_), #fn " ok=%d got %ld,%ld want %ld,%ld", r_, p_.x, p_.y, (long)(x_), (long)(y_)); } while (0)
#define LP_IS(dc, x_, y_, ex_, ey_) do { POINT p_ = {(x_), (y_)}; \
    BOOL r_ = U(LPtoDP)((dc), &p_, 1); \
    CHECKF(r_ && p_.x == (ex_) && p_.y == (ey_), "LPtoDP(%ld,%ld) ok=%d got %ld,%ld want %ld,%ld", (long)(x_), (long)(y_), r_, p_.x, p_.y, (long)(ex_), (long)(ey_)); } while (0)
#define DP_IS(dc, x_, y_, ex_, ey_) do { POINT p_ = {(x_), (y_)}; \
    BOOL r_ = U(DPtoLP)((dc), &p_, 1); \
    CHECKF(r_ && p_.x == (ex_) && p_.y == (ey_), "DPtoLP(%ld,%ld) ok=%d got %ld,%ld want %ld,%ld", (long)(x_), (long)(y_), r_, p_.x, p_.y, (long)(ex_), (long)(ey_)); } while (0)
#define XF_IS(xf, a, b, c, d, e, f) CHECKF((xf).eM11 == (a) && (xf).eM12 == (b) && (xf).eM21 == (c) && (xf).eM22 == (d) && (xf).eDx == (e) && (xf).eDy == (f), \
    "xform got %g %g %g %g %g %g want %g %g %g %g %g %g", (xf).eM11, (xf).eM12, (xf).eM21, (xf).eM22, (xf).eDx, (xf).eDy, (double)(a), (double)(b), (double)(c), (double)(d), (double)(e), (double)(f))
#endif
