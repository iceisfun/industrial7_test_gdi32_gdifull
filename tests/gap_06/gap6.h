#ifndef GAP6_H
#define GAP6_H
#include "t.h"
#include <usp10.h>

#define HR_INVALIDARG   ((HRESULT)0x80070057)
#define HR_OUTOFMEMORY  ((HRESULT)0x8007000E)
#define HR_NOTINFONT    ((HRESULT)0x80040200)
#define ARRSZ(a) ((int)(sizeof(a) / sizeof((a)[0])))

#define EXPECT_HR(call, want) do { \
    HRESULT hr_ = (call); \
    CHECKF(hr_ == (HRESULT)(want), "%s -> 0x%08lx want 0x%08lx", #call, (unsigned long)hr_, (unsigned long)(HRESULT)(want)); \
  } while (0)

static int g6_is_wine(void) {
    HMODULE n = GetModuleHandleA("ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version") != NULL;
}

#define WINE_SKIP(why) do { if (g6_is_wine()) { t_skip("wine: %s", why); return; } } while (0)

/* Memory DC with a 32bpp top-down DIB section and a selected font. */
typedef struct {
    HDC dc; HBITMAP bmp, oldbmp; HFONT font, oldfont; DWORD *bits; int w, h;
} G6;

static int g6_open(G6 *c, int w, int h, const WCHAR *face, int px) {
    memset(c, 0, sizeof *c);
    if (!U(CreateCompatibleDC) || !U(CreateDIBSection) || !U(SelectObject) || !U(CreateFontW)) return 0;
    c->dc = U(CreateCompatibleDC)(NULL);
    if (!c->dc) return 0;
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL;
    c->bmp = U(CreateDIBSection)(c->dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!c->bmp || !bits) { U(DeleteDC)(c->dc); c->dc = NULL; return 0; }
    c->bits = bits; c->w = w; c->h = h;
    c->oldbmp = U(SelectObject)(c->dc, c->bmp);
    c->font = U(CreateFontW)(-px, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH, face);
    if (c->font) c->oldfont = U(SelectObject)(c->dc, c->font);
    memset(c->bits, 0xff, (size_t)w * h * 4);
    return 1;
}
static void g6_clear(G6 *c) { memset(c->bits, 0xff, (size_t)c->w * c->h * 4); }
static void g6_close(G6 *c) {
    if (!c->dc) return;
    if (c->oldfont) U(SelectObject)(c->dc, c->oldfont);
    if (c->oldbmp) U(SelectObject)(c->dc, c->oldbmp);
    if (c->font) U(DeleteObject)(c->font);
    if (c->bmp) U(DeleteObject)(c->bmp);
    U(DeleteDC)(c->dc);
    memset(c, 0, sizeof *c);
}
/* count of non-white pixels inside [x0,x1) x [y0,y1) (clamped) */
static int g6_ink(G6 *c, int x0, int y0, int x1, int y1) {
    int n = 0;
    if (x0 < 0) { x0 = 0; }
    if (y0 < 0) { y0 = 0; }
    if (x1 > c->w) { x1 = c->w; }
    if (y1 > c->h) { y1 = c->h; }
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            if ((c->bits[y * c->w + x] & 0xffffff) != 0xffffff) n++;
    return n;
}
#define G6_OPEN(c, w, h, face, px) G6 c; do { if (!g6_open(&c, w, h, face, px)) { t_skip("cannot create memory DC"); return; } } while (0)

/* itemize a string; returns number of items (sentinel excluded) or -1 */
static int g6_itemize(const WCHAR *s, int n, SCRIPT_ITEM *items, int max, SCRIPT_STATE *st) {
    int cItems = 0;
    HRESULT hr = U(ScriptItemize)(s, n, max, NULL, st, items, &cItems);
    return hr == S_OK ? cItems : -1;
}

/* Shape+place a run of one item with an uncached script cache. */
typedef struct {
    SCRIPT_CACHE cache; SCRIPT_ANALYSIS sa; int nch;
    WORD glyphs[256], clust[256]; SCRIPT_VISATTR va[256]; int ng;
    int adv[256]; GOFFSET off[256]; ABC abc;
} G6Run;
static HRESULT g6_shape_place(G6 *c, G6Run *r, const WCHAR *s, int n) {
    SCRIPT_ITEM it[8]; memset(r, 0, sizeof *r);
    int ni = g6_itemize(s, n, it, 8, NULL);
    if (ni < 1) return E_FAIL;
    r->sa = it[0].a; r->nch = n;
    HRESULT hr = U(ScriptShape)(c->dc, &r->cache, s, n, 256, &r->sa, r->glyphs, r->clust, r->va, &r->ng);
    if (hr != S_OK) return hr;
    return U(ScriptPlace)(c->dc, &r->cache, r->glyphs, r->ng, r->va, &r->sa, r->adv, r->off, &r->abc);
}
#endif
