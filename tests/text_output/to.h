/* Shared helpers for the text_output area (all static). */
#ifndef TO_H
#define TO_H
#include "t.h"

#define NEED_TEXT() do { REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(SelectObject)); \
    REQUIRE(U(DeleteObject)); REQUIRE(U(CreateDIBSection)); REQUIRE(U(TextOutA)); REQUIRE(U(TextOutW)); \
    REQUIRE(U(ExtTextOutA)); REQUIRE(U(ExtTextOutW)); REQUIRE(U(GetTextExtentPoint32A)); \
    REQUIRE(U(GetTextExtentPoint32W)); REQUIRE(U(GetStockObject)); REQUIRE(U(PatBlt)); \
    REQUIRE(U(GetTextMetricsA)); REQUIRE(U(SetBkMode)); REQUIRE(U(CreateFontA)); REQUIRE(U(SetTextColor)); REQUIRE(U(SetBkColor)); } while (0)

static int to_wine(void) {
    HMODULE n = GetModuleHandleA("ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version") != NULL;
}

typedef struct {
    HDC dc; HBITMAP bmp, obmp; HFONT font, ofont; BYTE *bits;
    int w, h, bpp, stride;
} TCtx;

/* top-down DIB section selected into a fresh memory DC; 1/8bpp get a gray color table (0 black .. 255 white; 1bpp 0/1). */
static int to_open(TCtx *c, int w, int h, int bpp) {
    struct { BITMAPINFOHEADER h; RGBQUAD q[256]; } bi;
    void *bits = NULL;
    memset(c, 0, sizeof *c);
    memset(&bi, 0, sizeof bi);
    bi.h.biSize = sizeof bi.h; bi.h.biWidth = w; bi.h.biHeight = -h; bi.h.biPlanes = 1; bi.h.biBitCount = bpp;
    bi.h.biCompression = BI_RGB;
    if (bpp == 1) { bi.q[0].rgbRed = bi.q[0].rgbGreen = bi.q[0].rgbBlue = 0; bi.q[1].rgbRed = bi.q[1].rgbGreen = bi.q[1].rgbBlue = 255; bi.h.biClrUsed = 2; }
    if (bpp == 8) { for (int i = 0; i < 256; i++) bi.q[i].rgbRed = bi.q[i].rgbGreen = bi.q[i].rgbBlue = (BYTE)i; bi.h.biClrUsed = 256; }
    c->dc = U(CreateCompatibleDC)(NULL);
    if (!c->dc) return 0;
    c->bmp = U(CreateDIBSection)(c->dc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!c->bmp || !bits) { U(DeleteDC)(c->dc); c->dc = NULL; return 0; }
    c->obmp = (HBITMAP)U(SelectObject)(c->dc, c->bmp);
    c->bits = bits; c->w = w; c->h = h; c->bpp = bpp;
    c->stride = ((w * bpp + 31) / 32) * 4;
    U(SetBkMode)(c->dc, TRANSPARENT);
    U(PatBlt)(c->dc, 0, 0, w, h, WHITENESS);
    return 1;
}
static void to_close(TCtx *c) {
    if (!c->dc) return;
    if (c->ofont) U(SelectObject)(c->dc, c->ofont);
    if (c->obmp) U(SelectObject)(c->dc, c->obmp);
    if (c->font) U(DeleteObject)(c->font);
    U(DeleteObject)(c->bmp);
    U(DeleteDC)(c->dc);
    memset(c, 0, sizeof *c);
}
static void to_use_font(TCtx *c, HFONT f, int owned) {
    HFONT o = (HFONT)U(SelectObject)(c->dc, f);
    if (!c->ofont) c->ofont = o;
    if (owned) { if (c->font) U(DeleteObject)(c->font); c->font = f; }
}
static HFONT to_mkfont(const char *face, int h, int weight, int esc, int quality) {
    return U(CreateFontA)(h, 0, esc, esc, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, quality, DEFAULT_PITCH, face);
}
static void to_clear(TCtx *c) { U(PatBlt)(c->dc, 0, 0, c->w, c->h, WHITENESS); }
/* returns 0x00RRGGBB for 32/24bpp; the index for 1/8bpp */
static unsigned to_px(const TCtx *c, int x, int y) {
    if (x < 0 || y < 0 || x >= c->w || y >= c->h) return 0xdeadbeef;
    const BYTE *p = c->bits + (size_t)y * c->stride;
    switch (c->bpp) {
    case 32: return ((const DWORD *)p)[x] & 0xffffff;
    case 24: return p[x * 3] | (p[x * 3 + 1] << 8) | (p[x * 3 + 2] << 16);
    case 8: return p[x];
    case 1: return (p[x >> 3] >> (7 - (x & 7))) & 1;
    }
    return 0;
}
static unsigned to_white(const TCtx *c) { return c->bpp == 1 ? 1 : c->bpp == 8 ? 255 : 0xffffff; }
static unsigned to_cref2px(COLORREF cr) { return ((cr & 0xff) << 16) | (cr & 0xff00) | ((cr >> 16) & 0xff); }
/* bbox of pixels != bg; returns count */
static int to_ink(const TCtx *c, unsigned bg, RECT *r) {
    int n = 0;
    r->left = r->top = 0x7fffffff; r->right = r->bottom = -1;
    for (int y = 0; y < c->h; y++) for (int x = 0; x < c->w; x++)
        if (to_px(c, x, y) != bg) {
            n++;
            if (x < r->left) r->left = x;
            if (x > r->right) r->right = x;
            if (y < r->top) r->top = y;
            if (y > r->bottom) r->bottom = y;
        }
    return n;
}
/* count pixels equal to v within [x0,x1) x [y0,y1) (clipped to bitmap) */
static int to_count(const TCtx *c, unsigned v, int x0, int y0, int x1, int y1) {
    int n = 0;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) if (x >= 0 && y >= 0 && x < c->w && y < c->h && to_px(c, x, y) == v) n++;
    return n;
}
/* fixed-pitch bitmap font metrics. returns 1 if usable (fixed, no overhang) */
static int to_fixed(HDC dc, TEXTMETRICA *tm) {
    memset(tm, 0, sizeof *tm);
    if (!U(GetTextMetricsA)(dc, tm)) return 0;
    if (tm->tmPitchAndFamily & TMPF_FIXED_PITCH) return 0; /* flag set == variable pitch */
    if (tm->tmPitchAndFamily & (TMPF_TRUETYPE | TMPF_VECTOR)) return 0;
    if (tm->tmOverhang != 0) return 0;
    if (tm->tmMaxCharWidth != tm->tmAveCharWidth) return 0;
    return 1;
}
static int to_gdi_count(void) {
    DWORD (WINAPI *g)(HANDLE, DWORD) = (void *)t_proc("GetGuiResources");
    return g ? (int)g(GetCurrentProcess(), 0) : -1;
}
static const WCHAR *to_w(const char *s, WCHAR *buf, int n) {
    int i = 0; for (; s[i] && i < n - 1; i++) buf[i] = (BYTE)s[i]; buf[i] = 0; return buf;
}
#define ARRAYSZ(a) ((int)(sizeof(a) / sizeof((a)[0])))
#endif
