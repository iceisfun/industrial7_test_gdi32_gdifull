/* Shared helpers for the clipping area (all static). */
#ifndef CLIP_H
#define CLIP_H
#include "t.h"

/* single-evaluation variant (calls with side effects are common here) */
#undef EXPECT_EQ
#define EXPECT_EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)

#ifndef RGN_COPY
#define RGN_COPY 5
#endif
#ifndef CLR_INVALID
#define CLR_INVALID 0xFFFFFFFF
#endif

typedef int (WINAPI *clip_getrandomrgn_t)(HDC, HRGN, INT);
#define RNDRGN() ((clip_getrandomrgn_t)t_proc("GetRandomRgn"))

#define NEED_CLIP() do { \
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(CreateRectRgn)); \
    REQUIRE(U(DeleteObject)); REQUIRE(U(SelectClipRgn)); REQUIRE(U(GetClipBox)); \
    REQUIRE(U(GetClipRgn)); REQUIRE(U(IntersectClipRect)); REQUIRE(U(ExcludeClipRect)); \
    REQUIRE(U(SelectObject)); REQUIRE(U(CreateDIBSection)); REQUIRE(U(GetRgnBox)); \
    REQUIRE(U(PatBlt)); REQUIRE(U(SetRectRgn)); REQUIRE(U(PtInRegion)); REQUIRE(U(CombineRgn)); } while (0)

static int clip_is_wine(void) {
    HMODULE n = GetModuleHandleA("ntdll.dll");
    { HKEY k; if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Wine", 0, KEY_READ, &k) == 0) { RegCloseKey(k); return 1; } }
    return n && (GetProcAddress(n, "wine_get_version") != NULL ||
                 GetProcAddress(n, "wine_get_host_version") != NULL ||
                 GetProcAddress(n, "__wine_init_unix_call") != NULL ||
                 GetProcAddress(n, "wine_server_call") != NULL);
}

typedef struct {
    HDC dc; HBITMAP bmp, old; DWORD *bits; int w, h;
} MDC;

/* 32bpp top-down DIB section selected into a fresh memory DC; zero filled. */
static int mdc_create(MDC *m, int w, int h) {
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    memset(m, 0, sizeof *m);
    m->w = w; m->h = h;
    m->dc = U(CreateCompatibleDC)(NULL);
    if (!m->dc) return 0;
    m->bmp = U(CreateDIBSection)(m->dc, &bi, DIB_RGB_COLORS, (void **)&m->bits, NULL, 0);
    if (!m->bmp) { U(DeleteDC)(m->dc); m->dc = NULL; return 0; }
    m->old = (HBITMAP)U(SelectObject)(m->dc, m->bmp);
    memset(m->bits, 0, (size_t)w * h * 4);
    return 1;
}
static void mdc_destroy(MDC *m) {
    if (!m->dc) return;
    U(SelectObject)(m->dc, m->old);
    U(DeleteObject)(m->bmp);
    U(DeleteDC)(m->dc);
    m->dc = NULL;
}
static void mdc_clear(MDC *m, DWORD v) { for (int i = 0; i < m->w * m->h; i++) m->bits[i] = v; }
static DWORD mdc_px(const MDC *m, int x, int y) { return m->bits[y * m->w + x] & 0xFFFFFF; }
static int mdc_count(const MDC *m, DWORD v) {
    int n = 0; for (int i = 0; i < m->w * m->h; i++) if ((m->bits[i] & 0xFFFFFF) == v) n++;
    return n;
}
/* Count pixels differing from 0 outside rect r (clip rect) */
static int mdc_outside(const MDC *m, const RECT *r) {
    int n = 0;
    for (int y = 0; y < m->h; y++) for (int x = 0; x < m->w; x++)
        if (!(x >= r->left && x < r->right && y >= r->top && y < r->bottom) && (m->bits[y * m->w + x] & 0xFFFFFF)) n++;
    return n;
}
static int mdc_inside(const MDC *m, const RECT *r) {
    int n = 0;
    for (int y = r->top; y < r->bottom; y++) for (int x = r->left; x < r->right; x++)
        if (y >= 0 && y < m->h && x >= 0 && x < m->w && (m->bits[y * m->w + x] & 0xFFFFFF)) n++;
    return n;
}
static int rgn_box(HRGN r, RECT *rc) { memset(rc, 0xCC, sizeof *rc); return U(GetRgnBox)(r, rc); }
static int rect_is(const RECT *r, int l, int t, int rr, int b) {
    return r->left == l && r->top == t && r->right == rr && r->bottom == b;
}
#define EXPECT_RECT(r, l, t, rr, b) CHECKF(rect_is(&(r), l, t, rr, b), "got (%ld,%ld,%ld,%ld) want (%d,%d,%d,%d)", \
    (r).left, (r).top, (r).right, (r).bottom, (int)(l), (int)(t), (int)(rr), (int)(b))

/* Make a plain rect region. */
static HRGN rr(int l, int t, int r, int b) { return U(CreateRectRgn)(l, t, r, b); }

/* Compare every pixel of an MDC (filled via PatBlt BLACKNESS->WHITENESS) against PtInRegion. */
static int mdc_matches_region(const MDC *m, HRGN rgn, DWORD v, int *bad) {
    int n = 0;
    for (int y = 0; y < m->h; y++) for (int x = 0; x < m->w; x++) {
        int in = U(PtInRegion)(rgn, x, y) ? 1 : 0;
        int set = (m->bits[y * m->w + x] & 0xFFFFFF) == v;
        if (in != set) n++;
    }
    if (bad) *bad = n;
    return n == 0;
}
#endif
