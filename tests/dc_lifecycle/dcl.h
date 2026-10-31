/* Shared static helpers for the dc_lifecycle area. */
#ifndef DCL_H
#define DCL_H
#include "t.h"
#include <limits.h>

/* Single-evaluation equality check (EXPECT_EQ evaluates its arguments twice). */
#define EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)
#define SOFT(c) do { if (!(c)) t_note("(unverified) " #c); } while (0)
#define DCL_ERR_INVALID_HANDLE 6
#define DCL_ERR_INVALID_PARAM  87

static int dcl_wine(void) {
    HMODULE n = GetModuleHandleA("ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version") != NULL;
}
/* A check that is verified for Windows but known to differ under the emulator: demoted to a note there. */
#define DCL_WCHECK(c) do { if (dcl_wine()) { if (!(c)) t_note("(emulator differs) " #c); } else CHECK(c); } while (0)

#define WEQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    if (dcl_wine()) { if (a_ != b_) t_note("(emulator differs) " #a " == " #b " got 0x%llx", (unsigned long long)a_); } \
    else t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)
#define DCL_WERR(e) do { if (!dcl_wine()) EXPECT_ERR(0, e); } while (0)

static DWORD dcl_gdicount(void) {
    DWORD (WINAPI *g)(HANDLE, DWORD) = (void *)t_proc("GetGuiResources");
    return g ? g(GetCurrentProcess(), GR_GDIOBJECTS) : 0;
}

static HDC dcl_mem(void) { return U(CreateCompatibleDC)(NULL); }
static HDC dcl_screen(void) { return U(GetDC)(NULL); }
static void dcl_release_screen(HDC dc) { U(ReleaseDC)(NULL, dc); }

/* Creates a DIB section of the given depth; *bits receives the pixel pointer (may be NULL). */
static HBITMAP dcl_dib(HDC dc, int w, int h, int bpp, void **bits) {
    union { BITMAPINFO bi; char raw[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)]; } u;
    void *p = NULL;
    memset(&u, 0, sizeof u);
    u.bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    u.bi.bmiHeader.biWidth = w;
    u.bi.bmiHeader.biHeight = -h; /* top-down */
    u.bi.bmiHeader.biPlanes = 1;
    u.bi.bmiHeader.biBitCount = (WORD)bpp;
    u.bi.bmiHeader.biCompression = BI_RGB;
    if (bpp <= 8) {
        int n = 1 << bpp, i;
        u.bi.bmiHeader.biClrUsed = n;
        for (i = 0; i < n; i++) {
            u.bi.bmiColors[i].rgbRed = u.bi.bmiColors[i].rgbGreen = u.bi.bmiColors[i].rgbBlue = (BYTE)(i * 255 / (n - 1));
        }
    }
    HBITMAP b = U(CreateDIBSection)(dc, &u.bi, DIB_RGB_COLORS, &p, NULL, 0);
    if (bits) *bits = p;
    return b;
}

/* 32bpp top-down DIB selected into a fresh memory DC. Returns the DC; fills *bmp, *old, *bits. */
static HDC dcl_mem32(int w, int h, HBITMAP *bmp, HBITMAP *old, DWORD **bits) {
    HDC dc = dcl_mem();
    void *p = NULL;
    *bmp = NULL; *old = NULL;
    if (!dc) return NULL;
    *bmp = dcl_dib(dc, w, h, 32, &p);
    if (!*bmp) { U(DeleteDC)(dc); return NULL; }
    *old = (HBITMAP)U(SelectObject)(dc, *bmp);
    if (bits) *bits = (DWORD *)p;
    if (p) memset(p, 0, (size_t)w * h * 4);
    return dc;
}
static void dcl_free_mem(HDC dc, HBITMAP bmp, HBITMAP old) {
    if (dc && old) U(SelectObject)(dc, old);
    if (bmp) U(DeleteObject)(bmp);
    if (dc) U(DeleteDC)(dc);
}
#define DCL_RGB32(r, g, b) ((DWORD)(((r) << 16) | ((g) << 8) | (b)))

static HWND dcl_window(int x, int y, int w, int h) {
    HWND hw = t_make_window(NULL, WS_POPUP, 0, x, y, w, h, NULL);
    return hw;
}
#endif
