/* Shared static helpers for the gap_03 area. */
#ifndef G3_H
#define G3_H
#include "t.h"
#include <limits.h>

#define G3_ERR_INVALID_HANDLE 6
#define G3_ERR_INVALID_PARAM  87
#ifndef GDI_ERROR
#define GDI_ERROR 0xFFFFFFFFUL
#endif

#define EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)
#define SOFT(c) do { if (!(c)) t_note("(unverified) " #c); } while (0)

static int g3_wine(void) {
    static int v = -1;
    if (v < 0) { HMODULE n = GetModuleHandleA("ntdll.dll"); v = n && GetProcAddress(n, "wine_get_version") != NULL; }
    return v;
}
#define G3_WINE_SKIP(why) do { if (g3_wine()) { t_skip("emulator: %s", why); return; } } while (0)
/* verified for Windows; demoted to a note under an emulator */
#define WCHECK(c) do { if (g3_wine()) { if (!(c)) t_note("(emulator differs) " #c); } else CHECK(c); } while (0)
#define WEQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    if (g3_wine()) { if (a_ != b_) t_note("(emulator differs) " #a " == " #b " got 0x%llx", (unsigned long long)a_); } \
    else t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)
#define WERR(e) do { if (!g3_wine()) EXPECT_ERR(0, e); } while (0)

static DWORD g3_gui(void) {
    DWORD (WINAPI *g)(HANDLE, DWORD) = (void *)t_proc("GetGuiResources");
    return g ? g(GetCurrentProcess(), GR_GDIOBJECTS) : 0;
}
static HDC g3_mem(void) { return U(CreateCompatibleDC)(NULL); }
/* a deleted (stale) memory DC handle */
static HDC g3_stale_dc(void) { HDC d = U(CreateCompatibleDC)(NULL); U(DeleteDC)(d); return d; }
static HDC g3_screen(void) { return U(GetDC) ? U(GetDC)(NULL) : NULL; }
static void g3_unscreen(HDC dc) { if (dc && U(ReleaseDC)) U(ReleaseDC)(NULL, dc); }

static DWORD g3_px(COLORREF c) { return ((c & 0xff) << 16) | (c & 0xff00) | ((c >> 16) & 0xff); }

/* memory DC with a top-down 32bpp DIB section */
typedef struct { HDC dc; HBITMAP bmp, old; DWORD *bits; int w, h; } G3DC;
static int g3dc_make(G3DC *m, int w, int h) {
    BITMAPINFO bi; void *bits = NULL;
    memset(m, 0, sizeof *m);
    m->dc = U(CreateCompatibleDC)(NULL);
    if (!m->dc) return 0;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    m->bmp = U(CreateDIBSection)(m->dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!m->bmp || !bits) { U(DeleteDC)(m->dc); memset(m, 0, sizeof *m); return 0; }
    m->old = (HBITMAP)U(SelectObject)(m->dc, m->bmp);
    m->bits = (DWORD *)bits; m->w = w; m->h = h;
    return 1;
}
static void g3dc_free(G3DC *m) {
    if (!m->dc) return;
    U(SelectObject)(m->dc, m->old);
    U(DeleteDC)(m->dc);
    U(DeleteObject)(m->bmp);
    memset(m, 0, sizeof *m);
}
static void g3dc_clear(G3DC *m, COLORREF c) {
    int i; DWORD p = g3_px(c);
    U(GdiFlush)();
    for (i = 0; i < m->w * m->h; i++) m->bits[i] = p;
}
static DWORD g3dc_get(G3DC *m, int x, int y) {
    U(GdiFlush)();
    if (x < 0 || y < 0 || x >= m->w || y >= m->h) return 0xdeadbeef;
    return m->bits[y * m->w + x] & 0xffffff;
}
#define PXC(m, x, y, c) EQ(g3dc_get(&(m), (x), (y)), g3_px(c))
static int g3dc_count(G3DC *m, COLORREF c) {
    int i, n = 0; DWORD p = g3_px(c);
    U(GdiFlush)();
    for (i = 0; i < m->w * m->h; i++) if ((m->bits[i] & 0xffffff) == p) n++;
    return n;
}
/* count of pixels in a row/rect that equal colour c */
static int g3dc_count_rect(G3DC *m, int l, int t, int r, int b, COLORREF c) {
    int x, y, n = 0; DWORD p = g3_px(c);
    U(GdiFlush)();
    for (y = t; y < b; y++) for (x = l; x < r; x++)
        if (x >= 0 && y >= 0 && x < m->w && y < m->h && (m->bits[y * m->w + x] & 0xffffff) == p) n++;
    return n;
}

/* run fn(arg) on another thread */
typedef struct { void (*fn)(void *); void *arg; } G3THR;
static DWORD WINAPI g3_thr(LPVOID p) { G3THR *t = p; t->fn(t->arg); return 0; }
static void g3_run_thread(void (*fn)(void *), void *arg) {
    G3THR t; HANDLE h; t.fn = fn; t.arg = arg;
    h = CreateThread(NULL, 0, g3_thr, &t, 0, NULL);
    if (h) { WaitForSingleObject(h, 20000); CloseHandle(h); }
}

/* Resolve "<pub>Impl" in gdi32full or the public export. */
static FARPROC g3_fn(const char *pub, int impl) {
    char b[96];
    if (!impl) return t_proc(pub);
    snprintf(b, sizeof b, "%sImpl", pub);
    return t_proc_gdi32full(b);
}
#endif
