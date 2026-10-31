/* Shared helpers for the dc_attributes area. Everything static except the attribute table. */
#ifndef DCA_H
#define DCA_H
#include "t.h"
#include <limits.h>

/* ---- environment ---- */
static int dca_wine(void) {
    static int v = -1;
    if (v < 0) v = GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version") != NULL;
    return v;
}
/* skip a test body (no-op on Windows) when a call is known to abort under an emulator */
#define DCA_WINE_SKIP(why) do { if (dca_wine()) { t_skip("emulator: %s", why); return; } } while (0)

static DWORD dca_gui(void) {
    if (!U(GetGuiResources)) return 0;
    return U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
}
static HDC dca_screen(void) { return U(GetDC) ? U(GetDC)(NULL) : NULL; }
static void dca_unscreen(HDC dc) { if (dc && U(ReleaseDC)) U(ReleaseDC)(NULL, dc); }

/* COLORREF -> 0x00RRGGBB as stored in a 32bpp DIB */
static DWORD dca_px(COLORREF c) { return ((c & 0xff) << 16) | (c & 0xff00) | ((c >> 16) & 0xff); }

/* ---- memory DC with a 32bpp top-down DIB section ---- */
typedef struct { HDC dc; HBITMAP bmp, old; DWORD *bits; int w, h; } MDC;

static int mdc_make(MDC *m, int w, int h) {
    memset(m, 0, sizeof *m);
    if (!U(CreateCompatibleDC) || !U(CreateDIBSection) || !U(SelectObject)) return 0;
    m->dc = U(CreateCompatibleDC)(NULL);
    if (!m->dc) return 0;
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL;
    m->bmp = U(CreateDIBSection)(m->dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!m->bmp || !bits) { U(DeleteDC)(m->dc); m->dc = NULL; return 0; }
    m->old = (HBITMAP)U(SelectObject)(m->dc, m->bmp);
    m->bits = (DWORD *)bits; m->w = w; m->h = h;
    return 1;
}
static void mdc_free(MDC *m) {
    if (!m->dc) return;
    U(SelectObject)(m->dc, m->old);
    U(DeleteDC)(m->dc);
    U(DeleteObject)(m->bmp);
    memset(m, 0, sizeof *m);
}
static DWORD mdc_get(MDC *m, int x, int y) {
    if (U(GdiFlush)) U(GdiFlush)();
    if (x < 0 || y < 0 || x >= m->w || y >= m->h) return 0xdeadbeef;
    return m->bits[y * m->w + x] & 0xffffff;
}
/* fill the DIB memory directly with a COLORREF */
static void mdc_clear(MDC *m, COLORREF c) {
    if (U(GdiFlush)) U(GdiFlush)();
    DWORD p = dca_px(c);
    for (int i = 0; i < m->w * m->h; i++) m->bits[i] = p;
}
/* count pixels not equal to the COLORREF background */
static int mdc_count_ne(MDC *m, COLORREF bg) {
    if (U(GdiFlush)) U(GdiFlush)();
    DWORD p = dca_px(bg); int n = 0;
    for (int i = 0; i < m->w * m->h; i++) if ((m->bits[i] & 0xffffff) != p) n++;
    return n;
}
/* bounding box of pixels not equal to bg; returns count */
static int mdc_bbox(MDC *m, COLORREF bg, RECT *r) {
    if (U(GdiFlush)) U(GdiFlush)();
    DWORD p = dca_px(bg); int n = 0;
    r->left = r->top = 0x7fffffff; r->right = r->bottom = -1;
    for (int y = 0; y < m->h; y++) for (int x = 0; x < m->w; x++)
        if ((m->bits[y * m->w + x] & 0xffffff) != p) {
            n++;
            if (x < r->left) r->left = x;
            if (x > r->right) r->right = x;
            if (y < r->top) r->top = y;
            if (y > r->bottom) r->bottom = y;
        }
    return n;
}

/* ---- attribute table (attrs.c) ---- */
typedef struct DcaAttr {
    const char *name;
    DWORD (*set)(HDC, DWORD);      /* returns previous value / failure value */
    DWORD (*get)(HDC);
    int (*avail)(void);
    DWORD def;                     /* default on a fresh memory DC */
    DWORD vals[16]; int nv;        /* valid values, vals[0] may equal def */
    DWORD bad_set, bad_get;        /* return of setter/getter on an invalid HDC */
    DWORD err_null;                /* expected last error from the setter with HDC==NULL (0 = unverified) */
    DWORD err_stale;               /* expected last error with a deleted DC (0 = unverified) */
    int get_noerr;                 /* getter provably does not touch the last error on an invalid HDC */
    int rt_savedc;                 /* SaveDC/RestoreDC round trip verified as hard check */
} DcaAttr;
extern const DcaAttr dca_attrs[];
extern const int dca_nattrs;

/* a value from the attribute's list that differs from the default */
static DWORD dca_nondef(const DcaAttr *a) {
    for (int i = 0; i < a->nv; i++) if (a->vals[i] != a->def) return a->vals[i];
    return a->def;
}
static DWORD dca_nondef2(const DcaAttr *a, DWORD not1) {
    for (int i = 0; i < a->nv; i++) if (a->vals[i] != not1) return a->vals[i];
    return not1;
}

/* a set of handles of the wrong type; caller deletes with dca_wrong_free */
typedef struct { HGDIOBJ h[8]; const char *nm[8]; int n; } WrongSet;
static void dca_wrong_make(WrongSet *w) {
    memset(w, 0, sizeof *w);
    w->nm[w->n] = "HBITMAP"; w->h[w->n++] = U(CreateBitmap)(2, 2, 1, 1, NULL);
    w->nm[w->n] = "HPEN"; w->h[w->n++] = U(CreatePen)(PS_SOLID, 1, 0);
    w->nm[w->n] = "HBRUSH"; w->h[w->n++] = U(CreateSolidBrush)(0x123456);
    w->nm[w->n] = "HFONT"; w->h[w->n++] = U(CreateFontA)(12, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, "Arial");
    w->nm[w->n] = "HRGN"; w->h[w->n++] = U(CreateRectRgn)(0, 0, 4, 4);
    w->nm[w->n] = "stock pen"; w->h[w->n++] = U(GetStockObject)(BLACK_PEN);
    w->nm[w->n] = "stock brush"; w->h[w->n++] = U(GetStockObject)(WHITE_BRUSH);
    w->nm[w->n] = "stock font"; w->h[w->n++] = U(GetStockObject)(SYSTEM_FONT);
}
static void dca_wrong_free(WrongSet *w) {
    for (int i = 0; i < 5; i++) if (w->h[i]) U(DeleteObject)(w->h[i]);
}
static const ULONG_PTR dca_garbage[] = { 0, 1, 0xDEADBEEFull, (ULONG_PTR)-1, 0x10000, 0x7f0000, 0xFFFF0000ull };
#define DCA_NGARBAGE ((int)(sizeof dca_garbage / sizeof dca_garbage[0]))

#endif
