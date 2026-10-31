/* helpers for the gap_02 area (all static) */
#ifndef G2_H
#define G2_H
#include "t.h"
#include <wingdi.h>

static int g2_wine(void) {
    HMODULE n = GetModuleHandleA("ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version") != NULL;
}
/* no-op on Windows */
#define SKIP_ON_WINE(why) do { if (g2_wine()) { t_skip("wine: " why); return; } } while (0)

/* gdi32.dll export only, / gdi32full.dll export only, typed by UDECL pfn */
#define G32(name)  ((name##_pfn)t_proc_gdi32(#name))
#define G32F(name) ((name##_pfn)t_proc_gdi32full(#name))

/* GetGuiResources is a stub (0) under wine */
#define EXPECT_GDI(v) do { if (g2_gdi_count() != 0) EXPECT_EQ(g2_gdi_count(), (v)); } while (0)

/* gdi32full "...Impl" export; under wine (which has no such exports) falls back to the public gdi32 function
 * so that the test logic can still be exercised. On Windows only the real export is used. */
static FARPROC g2_impl(const char *impl, const char *pub) {
    FARPROC p = t_proc_gdi32full(impl);
    if (!p && g2_wine()) p = t_proc(pub);
    return p;
}
#define IMPL(base) ((__typeof__(&base))g2_impl(#base "Impl", #base))

#define PTR(x) ((void *)(ULONG_PTR)(x))

/* A 32bpp top-down DIB section selected into a memory DC. */
typedef struct { HDC dc; HBITMAP bmp, old; DWORD *bits; int w, h; } G2DC;

static int g2_mk(G2DC *d, int w, int h) {
    memset(d, 0, sizeof *d);
    if (!U(CreateCompatibleDC) || !U(CreateDIBSection) || !U(SelectObject)) return 0;
    d->dc = U(CreateCompatibleDC)(NULL);
    if (!d->dc) return 0;
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL;
    d->bmp = U(CreateDIBSection)(d->dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!d->bmp) { U(DeleteDC)(d->dc); d->dc = NULL; return 0; }
    d->bits = bits; d->w = w; d->h = h;
    d->old = U(SelectObject)(d->dc, d->bmp);
    memset(bits, 0, (size_t)w * h * 4);
    return 1;
}
static void g2_free(G2DC *d) {
    if (d->dc) {
        if (d->old) U(SelectObject)(d->dc, d->old);
        U(DeleteDC)(d->dc);
    }
    if (d->bmp) U(DeleteObject)(d->bmp);
    memset(d, 0, sizeof *d);
}
static void g2_fill(G2DC *d, DWORD v) {
    for (int i = 0; i < d->w * d->h; i++) d->bits[i] = v;
}
static DWORD g2_px(G2DC *d, int x, int y) {
    U(GdiFlush)();
    return d->bits[y * d->w + x] & 0xffffff;
}
static int g2_count(G2DC *d, DWORD v) {
    int n = 0;
    U(GdiFlush)();
    for (int i = 0; i < d->w * d->h; i++) if ((d->bits[i] & 0xffffff) == v) n++;
    return n;
}
/* COLORREF -> DIB pixel value */
#define G2_PX(r, g, b) (((DWORD)(r) << 16) | ((DWORD)(g) << 8) | (DWORD)(b))

static DWORD g2_gdi_count(void) {
    return U(GetGuiResources) ? U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS) : 0;
}

/* handles that are certainly not valid GDI objects (object-type bits are neither DC=1 nor METADC16=0x66) */
static const ULONG_PTR g2_junk[] = { 0, 1, (ULONG_PTR)-1, 0xdeadbeef, 0x12345678, 0x7fffffff, 0xffffffff, 0x00400000 };
#define G2_NJUNK ((int)(sizeof g2_junk / sizeof g2_junk[0]))

/* a handle that was valid and has been deleted */
static HDC g2_stale_dc(void) {
    HDC d = U(CreateCompatibleDC)(NULL);
    if (d) U(DeleteDC)(d);
    return d;
}
#endif
