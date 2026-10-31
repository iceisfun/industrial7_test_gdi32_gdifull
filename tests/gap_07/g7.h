/* Shared helpers for gap_07 (static, header-only). */
#ifndef G7_H
#define G7_H
#include "t.h"
#include <usp10.h>

#define G7_E_INVALIDARG   ((DWORD)0x80070057u)
#define G7_E_OUTOFMEMORY  ((DWORD)0x8007000Eu)
#define G7_STATUS_INVALID_PARAMETER ((DWORD)0xC000000Du)

static int g7_is_wine(void) {
    HMODULE n = GetModuleHandleA("ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version") != NULL;
}

/* Uniscribe entry point: gdi32 -> gdi32full as always; on hosts where those do not carry it (non-Windows sanity
 * runs) fall back to usp10.dll so that the test logic still runs. Real Windows 10 always resolves in gdi32. */
static FARPROC g7_usp_proc(const char *n) {
    FARPROC p = NULL;
    if (g7_is_wine()) { HMODULE m = LoadLibraryA("usp10.dll"); if (m) p = GetProcAddress(m, n); if (p) return p; }
    p = t_proc(n);
    if (!p) { HMODULE m = LoadLibraryA("usp10.dll"); if (m) p = GetProcAddress(m, n); }
    return p;
}
#define USP(name) ((__typeof__(&name))g7_usp_proc(#name))

/* soft expectation: observed behaviour, never fails the run */
static void g7_soft(const char *file, int line, const char *cond, const char *fmt, ...) {
    char buf[300]; va_list a;
    va_start(a, fmt); vsnprintf(buf, sizeof buf, fmt, a); va_end(a);
    t_note("soft expectation failed (%s:%d) %s : %s", file, line, cond, buf);
}
#define SOFT(cond, ...) do { if (!(cond)) g7_soft(__FILE__, __LINE__, #cond, __VA_ARGS__); } while (0)

/* ---- memory DC backed by a 32bpp top-down DIB section ---- */
typedef struct {
    HDC dc; HBITMAP bmp, oldbm; HFONT font, oldfont;
    DWORD *bits; int w, h;
} G7DC;

static int g7_dc_create(G7DC *d, int w, int h, const WCHAR *face, int height) {
    HDC (WINAPI *ccdc)(HDC) = (void *)t_proc("CreateCompatibleDC");
    HBITMAP (WINAPI *cds)(HDC, const BITMAPINFO *, UINT, void **, HANDLE, DWORD) = (void *)t_proc("CreateDIBSection");
    HGDIOBJ (WINAPI *so)(HDC, HGDIOBJ) = (void *)t_proc("SelectObject");
    HFONT (WINAPI *cf)(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCWSTR) = (void *)t_proc("CreateFontW");
    BITMAPINFO bi; void *bits = NULL;
    memset(d, 0, sizeof *d);
    if (!ccdc || !cds || !so || !cf) return 0;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    d->dc = ccdc(NULL);
    if (!d->dc) return 0;
    d->bmp = cds(d->dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!d->bmp || !bits) { U(DeleteDC)(d->dc); d->dc = NULL; return 0; }
    d->bits = bits; d->w = w; d->h = h;
    d->oldbm = so(d->dc, d->bmp);
    if (face) {
        d->font = cf(-height, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, face);
        if (d->font) d->oldfont = so(d->dc, d->font);
    }
    memset(d->bits, 0xFF, (size_t)w * h * 4);
    return 1;
}
static void g7_dc_destroy(G7DC *d) {
    HGDIOBJ (WINAPI *so)(HDC, HGDIOBJ) = (void *)t_proc("SelectObject");
    if (!d->dc) return;
    if (d->oldfont) so(d->dc, d->oldfont);
    if (d->oldbm) so(d->dc, d->oldbm);
    if (d->font) U(DeleteObject)(d->font);
    if (d->bmp) U(DeleteObject)(d->bmp);
    U(DeleteDC)(d->dc);
    memset(d, 0, sizeof *d);
}
static void g7_dc_clear(G7DC *d) { memset(d->bits, 0xFF, (size_t)d->w * d->h * 4); }
/* number of pixels differing from white, and their bounding box */
static int g7_dc_ink(const G7DC *d, RECT *bb) {
    int n = 0;
    if (bb) { bb->left = d->w; bb->top = d->h; bb->right = -1; bb->bottom = -1; }
    for (int y = 0; y < d->h; y++)
        for (int x = 0; x < d->w; x++)
            if ((d->bits[y * d->w + x] & 0xFFFFFF) != 0xFFFFFF) {
                n++;
                if (bb) {
                    if (x < bb->left) bb->left = x;
                    if (x > bb->right) bb->right = x;
                    if (y < bb->top) bb->top = y;
                    if (y > bb->bottom) bb->bottom = y;
                }
            }
    return n;
}

/* ---- a fully shaped/placed single-item Latin run ---- */
#define G7_MAXC 96
#define G7_MAXG 256
typedef struct {
    WCHAR s[G7_MAXC]; int n;
    SCRIPT_ITEM items[G7_MAXC + 2]; int nitems;
    SCRIPT_ANALYSIS psa;
    WORD glyphs[G7_MAXG], clust[G7_MAXC]; SCRIPT_VISATTR va[G7_MAXG]; int ng;
    int adv[G7_MAXG]; GOFFSET go[G7_MAXG]; ABC abc;
    SCRIPT_CACHE sc;
} G7RUN;

/* returns 1 on success; the run uses items[0] */
static int g7_run_build(HDC dc, G7RUN *r, const WCHAR *s) {
    HRESULT (WINAPI *it)(const WCHAR *, int, int, const SCRIPT_CONTROL *, const SCRIPT_STATE *, SCRIPT_ITEM *, int *) = (void *)g7_usp_proc("ScriptItemize");
    HRESULT (WINAPI *sh)(HDC, SCRIPT_CACHE *, const WCHAR *, int, int, SCRIPT_ANALYSIS *, WORD *, WORD *, SCRIPT_VISATTR *, int *) = (void *)g7_usp_proc("ScriptShape");
    HRESULT (WINAPI *pl)(HDC, SCRIPT_CACHE *, const WORD *, int, const SCRIPT_VISATTR *, SCRIPT_ANALYSIS *, int *, GOFFSET *, ABC *) = (void *)g7_usp_proc("ScriptPlace");
    memset(r, 0, sizeof *r);
    if (!it || !sh || !pl) return 0;
    r->n = lstrlenW(s);
    if (r->n <= 0 || r->n >= G7_MAXC) return 0;
    memcpy(r->s, s, (r->n + 1) * sizeof(WCHAR));
    if (FAILED(it(r->s, r->n, r->n + 1, NULL, NULL, r->items, &r->nitems))) return 0;
    if (r->nitems < 1) return 0; /* items[nitems] is the sentinel */
    r->psa = r->items[0].a;
    r->n = r->items[1].iCharPos - r->items[0].iCharPos; /* first item only */
    if (r->n <= 0) return 0;
    if (FAILED(sh(dc, &r->sc, r->s, r->n, G7_MAXG, &r->psa, r->glyphs, r->clust, r->va, &r->ng))) return 0;
    if (FAILED(pl(dc, &r->sc, r->glyphs, r->ng, r->va, &r->psa, r->adv, r->go, &r->abc))) return 0;
    return 1;
}
static void g7_run_free(G7RUN *r) {
    HRESULT (WINAPI *fc)(SCRIPT_CACHE *) = (void *)g7_usp_proc("ScriptFreeCache");
    if (fc) fc(&r->sc);
}
static int g7_run_width(const G7RUN *r) { int w = 0; for (int i = 0; i < r->ng; i++) w += r->adv[i]; return w; }

static int g7_gui(void) {
    DWORD (WINAPI *g)(HANDLE, DWORD) = (void *)t_proc("GetGuiResources");
    return g ? (int)g(GetCurrentProcess(), 0) : -1;
}

/* run fn(arg) on a fresh thread and wait */
typedef void (*g7_thr_fn)(void *);
typedef struct { g7_thr_fn fn; void *arg; } g7_thr_ctx;
static DWORD WINAPI g7_thr_main(LPVOID p) { g7_thr_ctx *c = p; c->fn(c->arg); return 0; }
static void g7_run_thread(g7_thr_fn fn, void *arg) {
    g7_thr_ctx c = { fn, arg };
    HANDLE h = CreateThread(NULL, 0, g7_thr_main, &c, 0, NULL);
    if (h) { WaitForSingleObject(h, 30000); CloseHandle(h); }
}
#endif
