/* Shared helpers for the pens_brushes area (static; included by every file). */
#ifndef PB_H
#define PB_H
#include "t.h"
#include <limits.h>

#define GetStockObject(i) (U(GetStockObject) ? U(GetStockObject)(i) : (HGDIOBJ)NULL)
#define NEED_MEM() do { REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); \
    REQUIRE(U(CreateDIBSection)); REQUIRE(U(SelectObject)); REQUIRE(U(DeleteObject)); \
    REQUIRE(U(GdiFlush)); REQUIRE(U(GetObjectW)); } while (0)

static int pb_wine(void)
{
    HMODULE n = GetModuleHandleA("ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version") != NULL;
}

typedef struct { HDC dc; HBITMAP bmp, old; BYTE *bits; int w, h, bpp, stride; } PBM;

/* top-down DIB section selected into a fresh memory DC. bpp: 1,16(555),24,32 */
static int pb_mem(PBM *m, int w, int h, int bpp)
{
    union { BITMAPINFO bi; char pad[sizeof(BITMAPINFOHEADER) + 8 * 4]; } u;
    memset(&u, 0, sizeof u);
    memset(m, 0, sizeof *m);
    u.bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    u.bi.bmiHeader.biWidth = w;
    u.bi.bmiHeader.biHeight = -h;
    u.bi.bmiHeader.biPlanes = 1;
    u.bi.bmiHeader.biBitCount = (WORD)bpp;
    u.bi.bmiHeader.biCompression = BI_RGB;
    if (bpp == 1) {
        u.bi.bmiHeader.biClrUsed = 2;
        u.bi.bmiColors[0].rgbBlue = u.bi.bmiColors[0].rgbGreen = u.bi.bmiColors[0].rgbRed = 0;
        u.bi.bmiColors[1].rgbBlue = u.bi.bmiColors[1].rgbGreen = u.bi.bmiColors[1].rgbRed = 255;
    }
    m->w = w; m->h = h; m->bpp = bpp;
    m->stride = ((w * bpp + 31) / 32) * 4;
    m->dc = U(CreateCompatibleDC)(NULL);
    if (!m->dc) return 0;
    m->bmp = U(CreateDIBSection)(m->dc, &u.bi, DIB_RGB_COLORS, (void **)&m->bits, NULL, 0);
    if (!m->bmp || !m->bits) { U(DeleteDC)(m->dc); m->dc = NULL; return 0; }
    m->old = U(SelectObject)(m->dc, m->bmp);
    return 1;
}
static void pb_free(PBM *m)
{
    if (!m->dc) return;
    U(SelectObject)(m->dc, m->old);
    U(DeleteObject)(m->bmp);
    U(DeleteDC)(m->dc);
    m->dc = NULL;
}
/* raw pixel (after GdiFlush) */
static unsigned pb_get(PBM *m, int x, int y)
{
    BYTE *p = m->bits + (size_t)y * m->stride;
    switch (m->bpp) {
    case 1:  return (p[x >> 3] >> (7 - (x & 7))) & 1;
    case 16: return ((WORD *)p)[x];
    case 24: return p[x * 3] | (p[x * 3 + 1] << 8) | (p[x * 3 + 2] << 16);
    default: return ((DWORD *)p)[x] & 0xffffff;
    }
}
static void pb_set(PBM *m, int x, int y, unsigned v)
{
    BYTE *p = m->bits + (size_t)y * m->stride;
    switch (m->bpp) {
    case 1:  if (v & 1) p[x >> 3] |= 0x80 >> (x & 7); else p[x >> 3] &= ~(0x80 >> (x & 7)); break;
    case 16: ((WORD *)p)[x] = (WORD)v; break;
    case 24: p[x * 3] = v; p[x * 3 + 1] = v >> 8; p[x * 3 + 2] = v >> 16; break;
    default: ((DWORD *)p)[x] = v; break;
    }
}
static void pb_clear(PBM *m, unsigned v)
{
    for (int y = 0; y < m->h; y++) for (int x = 0; x < m->w; x++) pb_set(m, x, y, v);
}
/* raw pixel value for an exact RGB colour on this format (only meaningful for 24/32; 16 = 555 truncation) */
static unsigned pb_raw(PBM *m, COLORREF c)
{
    unsigned r = GetRValue(c), g = GetGValue(c), b = GetBValue(c);
    switch (m->bpp) {
    case 1:  return (r || g || b) ? 1 : 0;
    case 16: return ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
    default: return (r << 16) | (g << 8) | b;
    }
}
static void pb_sync(void) { if (U(GdiFlush)) U(GdiFlush)(); }

/* count pixels != v inside the whole surface */
static int pb_count_ne(PBM *m, unsigned v)
{
    int n = 0;
    pb_sync();
    for (int y = 0; y < m->h; y++) for (int x = 0; x < m->w; x++) if (pb_get(m, x, y) != v) n++;
    return n;
}
static int pb_count_eq(PBM *m, unsigned v)
{
    return m->w * m->h - pb_count_ne(m, v);
}

static DWORD pb_gdi_count(void)
{
    DWORD (WINAPI *ggr)(HANDLE, DWORD) = (void *)t_proc("GetGuiResources");
    return ggr ? ggr(GetCurrentProcess(), 0 /* GR_GDIOBJECTS */) : 0;
}
#endif
