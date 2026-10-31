/* shared helpers for the dib area (static, header-only) */
#ifndef DIBH_H
#define DIBH_H
#include "t.h"

typedef struct {
    BITMAPINFOHEADER h;
    union { RGBQUAD q[256]; DWORD m[3]; WORD idx[256]; RGBTRIPLE tr[256]; };
} BMI;
#define PB(b) ((BITMAPINFO *)(b))

static int dib_is_wine(void) {
    HMODULE n = GetModuleHandleA("ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version") != NULL;
}
static int dib_stride(int w, int bpp) { return ((w * bpp + 31) / 32) * 4; }

static void bmi_init(BMI *b, int w, int h, int bpp, DWORD comp) {
    memset(b, 0, sizeof *b);
    b->h.biSize = sizeof(BITMAPINFOHEADER);
    b->h.biWidth = w; b->h.biHeight = h;
    b->h.biPlanes = 1; b->h.biBitCount = (WORD)bpp; b->h.biCompression = comp;
}
/* fills an 8bpp/4bpp/1bpp table with a distinctive pattern */
static void bmi_pal(BMI *b, int n) {
    for (int i = 0; i < n && i < 256; i++) {
        b->q[i].rgbBlue = (BYTE)i; b->q[i].rgbGreen = (BYTE)(255 - i);
        b->q[i].rgbRed = (BYTE)(i ^ 0x55); b->q[i].rgbReserved = 0;
    }
}
static int pal_n(int bpp) { return bpp <= 8 ? 1 << bpp : 0; }

/* create a DIB section (hdc NULL, DIB_RGB_COLORS). */
static HBITMAP mkdib(int w, int h, int bpp, void **bits) {
    BMI b; void *p = NULL;
    bmi_init(&b, w, h, bpp, BI_RGB);
    if (bpp <= 8) bmi_pal(&b, 1 << bpp);
    HBITMAP hb = U(CreateDIBSection)(NULL, PB(&b), DIB_RGB_COLORS, &p, NULL, 0);
    if (bits) *bits = p;
    return hb;
}
/* memory DC with a DIB selected; *old receives the previous bitmap */
static HDC mkdc(HBITMAP hb, HGDIOBJ *old) {
    HDC dc = U(CreateCompatibleDC)(NULL);
    if (!dc) return NULL;
    HGDIOBJ o = U(SelectObject)(dc, hb);
    if (old) *old = o;
    return dc;
}
static void freedc(HDC dc, HGDIOBJ old) {
    if (!dc) return;
    if (old) U(SelectObject)(dc, old);
    U(DeleteDC)(dc);
}
static int gdicount(void) {
    return (int)U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
}
static DWORD rgbq(BYTE r, BYTE g, BYTE b) { return RGB(r, g, b); }

#define NEED_GDI() do { REQUIRE(U(CreateDIBSection)); REQUIRE(U(CreateCompatibleDC)); \
    REQUIRE(U(DeleteDC)); REQUIRE(U(SelectObject)); REQUIRE(U(DeleteObject)); \
    REQUIRE(U(GetObjectW)); REQUIRE(U(GetPixel)); REQUIRE(U(SetPixelV)); REQUIRE(U(GdiFlush)); } while (0)
#endif
