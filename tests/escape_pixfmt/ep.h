#ifndef EP_H
#define EP_H
#include "t.h"

static inline int ep_is_wine(void) {
    HMODULE n = GetModuleHandleA("ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version") != NULL;
}
/* skip (no-op on Windows) when running under an emulator that aborts on the call */
#define EP_WINE_SKIP(why) do { if (ep_is_wine()) { t_skip("wine: " why); return; } } while (0)

static inline HDC ep_scr(void) { return U(GetDC)(NULL); }
static inline void ep_scr_free(HDC dc) { U(ReleaseDC)(NULL, dc); }

/* memory DC with a 32bpp top-down DIB section selected; *old receives the stock bitmap */
static inline HDC ep_memdc32(int w, int h, HBITMAP *bmp, HBITMAP *old, void **bits) {
    HDC dc = U(CreateCompatibleDC)(NULL);
    if (!dc) return NULL;
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void *b = NULL;
    HBITMAP hb = U(CreateDIBSection)(dc, &bi, DIB_RGB_COLORS, &b, NULL, 0);
    if (!hb) { U(DeleteDC)(dc); return NULL; }
    *old = (HBITMAP)U(SelectObject)(dc, hb);
    *bmp = hb; if (bits) *bits = b;
    return dc;
}
static inline void ep_memdc_free(HDC dc, HBITMAP bmp, HBITMAP old) {
    U(SelectObject)(dc, old); U(DeleteObject)(bmp); U(DeleteDC)(dc);
}
static inline DWORD ep_gui(void) { return U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS); }

/* a bogus, never-valid handle values */
#define EP_BOGUS_DC ((HDC)(ULONG_PTR)0x00001234)
#define EP_BOGUS_DC2 ((HDC)(ULONG_PTR)0xDEADBEEF)
#endif
