/* Shared helpers for the gap_10 area (all static, no external state). */
#ifndef GAP10_H
#define GAP10_H
#include "t.h"
#include <math.h>
#include <limits.h>

#define ERRS 0x7777u
#define CLR() SetLastError(ERRS)

static int g10_wine(void) {
    HMODULE n = GetModuleHandleA("ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version") != NULL;
}
#define SKIP_WINE(why) do { if (g10_wine()) { t_skip("wine: " why); return; } } while (0)

static DWORD g10_gdicount(void) {
    DWORD (WINAPI *f)(HANDLE, DWORD) = (void *)t_proc("GetGuiResources");
    return f ? f(GetCurrentProcess(), 0 /*GR_GDIOBJECTS*/) : 0;
}

/* Memory DC holding a top-down DIB section of 16/24/32 bpp. */
typedef struct { HDC dc; HBITMAP bmp, old; void *bits; int w, h, bpp; } MD;

#define NEED_MD() do { \
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(CreateDIBSection)); REQUIRE(U(SelectObject)); \
    REQUIRE(U(DeleteObject)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetPixel)); \
    REQUIRE(U(CreateSolidBrush)); REQUIRE(U(PatBlt)); } while (0)

static int md_create(MD *m, int w, int h, int bpp) {
    BITMAPINFO bi;
    memset(m, 0, sizeof *m);
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = (WORD)bpp;
    bi.bmiHeader.biCompression = BI_RGB;
    m->w = w; m->h = h; m->bpp = bpp;
    m->dc = U(CreateCompatibleDC)(NULL);
    if (!m->dc) return 0;
    m->bmp = U(CreateDIBSection)(m->dc, &bi, DIB_RGB_COLORS, &m->bits, NULL, 0);
    if (!m->bmp) { U(DeleteDC)(m->dc); m->dc = NULL; return 0; }
    m->old = U(SelectObject)(m->dc, m->bmp);
    return 1;
}
static void md_destroy(MD *m) {
    if (!m->dc) return;
    U(SelectObject)(m->dc, m->old);
    U(DeleteObject)(m->bmp);
    U(DeleteDC)(m->dc);
    m->dc = NULL;
}
static void md_rect(MD *m, int x, int y, int w, int h, COLORREF c) {
    HBRUSH b = U(CreateSolidBrush)(c);
    HGDIOBJ o = U(SelectObject)(m->dc, b);
    U(PatBlt)(m->dc, x, y, w, h, PATCOPY);
    U(SelectObject)(m->dc, o);
    U(DeleteObject)(b);
}
static void md_clear(MD *m, COLORREF c) { md_rect(m, 0, 0, m->w, m->h, c); }
static COLORREF md_px(MD *m, int x, int y) { return U(GetPixel)(m->dc, x, y); }
/* Count pixels equal to c inside the rectangle. */
static int md_count(MD *m, int x0, int y0, int x1, int y1, COLORREF c) {
    int x, y, n = 0;
    for (y = y0; y < y1; y++) for (x = x0; x < x1; x++) if (md_px(m, x, y) == c) n++;
    return n;
}
/* Number of pixels that differ from c in the whole surface. */
static int md_diff(MD *m, COLORREF c) { return m->w * m->h - md_count(m, 0, 0, m->w, m->h, c); }

#define WHITE_ 0x00FFFFFFu
#define BLACK_ 0x00000000u
#define RED_   0x000000FFu   /* COLORREF 0x00BBGGRR */
#define GREEN_ 0x0000FF00u
#define BLUE_  0x00FF0000u

typedef struct { void (*fn)(void *); void *arg; } G10_THR;
static DWORD WINAPI g10_thr_main(LPVOID p) { G10_THR *t = p; t->fn(t->arg); return 0; }
static void g10_run_thread(void (*fn)(void *), void *arg) {
    G10_THR t; t.fn = fn; t.arg = arg;
    HANDLE h = CreateThread(NULL, 0, g10_thr_main, &t, 0, NULL);
    if (h) { WaitForSingleObject(h, 30000); CloseHandle(h); }
}
#endif
