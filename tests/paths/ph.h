/* Shared helpers for the paths area (static, header-only). */
#ifndef PH_H
#define PH_H
#include "t.h"
#include <math.h>

#define PT_MOVE PT_MOVETO
#define PT_LINE PT_LINETO
#define PT_BEZ  PT_BEZIERTO
#define PT_CLOSE PT_CLOSEFIGURE
#define WHITE 0x00FFFFFFu

/* Everything the common tests need; skips the test if any export is missing. */
#define REQ_BASE() do { \
    REQUIRE(U(BeginPath)); REQUIRE(U(EndPath)); REQUIRE(U(AbortPath)); REQUIRE(U(GetPath)); \
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(CreateDIBSection)); \
    REQUIRE(U(SelectObject)); REQUIRE(U(DeleteObject)); REQUIRE(U(LineTo)); REQUIRE(U(MoveToEx)); \
    REQUIRE(U(FillPath)); REQUIRE(U(StrokePath)); REQUIRE(U(StrokeAndFillPath)); \
    REQUIRE(U(CloseFigure)); REQUIRE(U(Polygon)); REQUIRE(U(Polyline)); REQUIRE(U(Rectangle)); \
    REQUIRE(U(Ellipse)); REQUIRE(U(GetStockObject)); REQUIRE(U(GdiFlush)); REQUIRE(U(PatBlt)); \
    REQUIRE(U(CreatePen)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(PathToRegion)); \
    REQUIRE(U(WidenPath)); REQUIRE(U(FlattenPath)); REQUIRE(U(SelectClipPath)); \
    REQUIRE(U(GetCurrentPositionEx)); \
} while (0)

static int ph_is_wine(void) {
    HMODULE n = GetModuleHandleA("ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version") != NULL;
}

/* 32bpp (or other depth) top-down DIB section memory DC. */
typedef struct {
    HDC dc; HBITMAP bmp, old; unsigned char *bits; int w, h, bpp, stride;
} MD;

static int md_create_bpp(MD *m, int w, int h, int bpp) {
    BYTE buf[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)];
    BITMAPINFO *bi = (BITMAPINFO *)buf; void *bits = NULL;
    memset(buf, 0, sizeof buf); memset(m, 0, sizeof *m);
    bi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi->bmiHeader.biWidth = w; bi->bmiHeader.biHeight = -h;
    bi->bmiHeader.biPlanes = 1; bi->bmiHeader.biBitCount = bpp; bi->bmiHeader.biCompression = BI_RGB;
    if (bpp <= 8) {
        bi->bmiColors[0].rgbRed = bi->bmiColors[0].rgbGreen = bi->bmiColors[0].rgbBlue = 0;
        bi->bmiColors[1].rgbRed = bi->bmiColors[1].rgbGreen = bi->bmiColors[1].rgbBlue = 255;
        bi->bmiHeader.biClrUsed = bpp == 8 ? 256 : 2;
        if (bpp == 8) { int i; for (i = 2; i < 256; i++) bi->bmiColors[i].rgbRed = bi->bmiColors[i].rgbGreen = bi->bmiColors[i].rgbBlue = 255; }
    }
    m->dc = U(CreateCompatibleDC)(NULL);
    if (!m->dc) return 0;
    m->bmp = U(CreateDIBSection)(NULL, bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!m->bmp || !bits) { U(DeleteDC)(m->dc); m->dc = NULL; return 0; }
    m->old = (HBITMAP)U(SelectObject)(m->dc, m->bmp);
    m->bits = bits; m->w = w; m->h = h; m->bpp = bpp; m->stride = ((w * bpp + 31) / 32) * 4;
    return 1;
}
static int md_create(MD *m, int w, int h) { return md_create_bpp(m, w, h, 32); }
static void md_destroy(MD *m) {
    if (!m->dc) return;
    U(SelectObject)(m->dc, m->old);
    U(DeleteObject)(m->bmp);
    U(DeleteDC)(m->dc);
    memset(m, 0, sizeof *m);
}
static void md_clear(MD *m, unsigned c) {
    int x, y;
    U(GdiFlush)();
    for (y = 0; y < m->h; y++) for (x = 0; x < m->w; x++) ((unsigned *)(m->bits + y * m->stride))[x] = c;
}
/* 32bpp pixel as 0x00RRGGBB */
static unsigned md_px(MD *m, int x, int y) {
    U(GdiFlush)();
    return ((unsigned *)(m->bits + y * m->stride))[x] & 0xFFFFFF;
}
static int md_count(MD *m, unsigned c) {
    int x, y, n = 0;
    for (y = 0; y < m->h; y++) for (x = 0; x < m->w; x++) if (md_px(m, x, y) == c) n++;
    return n;
}
static int md_count_not(MD *m, unsigned c) { return m->w * m->h - md_count(m, c); }
/* bounding box of pixels != bg ; returns count */
static int md_bbox(MD *m, unsigned bg, RECT *r) {
    int x, y, n = 0; r->left = r->top = 0x7fffffff; r->right = r->bottom = -1;
    for (y = 0; y < m->h; y++) for (x = 0; x < m->w; x++) if (md_px(m, x, y) != bg) {
        n++;
        if (x < r->left) r->left = x;
        if (y < r->top) r->top = y;
        if (x + 1 > r->right) r->right = x + 1;
        if (y + 1 > r->bottom) r->bottom = y + 1;
    }
    return n;
}
static int md_same(MD *a, MD *b) {
    int x, y;
    for (y = 0; y < a->h; y++) for (x = 0; x < a->w; x++) if (md_px(a, x, y) != md_px(b, x, y)) return 0;
    return 1;
}
static int md_rect_is(MD *m, int l, int t, int r, int b, unsigned in, unsigned out) {
    int x, y;
    for (y = 0; y < m->h; y++) for (x = 0; x < m->w; x++) {
        unsigned want = (x >= l && x < r && y >= t && y < b) ? in : out;
        if (md_px(m, x, y) != want) return 0;
    }
    return 1;
}

/* Path snapshot */
#define PD_MAX 8192
typedef struct { int n; POINT pt[PD_MAX]; BYTE ty[PD_MAX]; } PD;
static PD g_pd;
/* Returns the count from GetPath(0), and fills g_pd when it is in range. */
static int pd_get(HDC dc) {
    int n = U(GetPath)(dc, NULL, NULL, 0);
    g_pd.n = 0;
    if (n > 0 && n <= PD_MAX) {
        int r = U(GetPath)(dc, g_pd.pt, g_pd.ty, n);
        if (r != n) return -2;
        g_pd.n = n;
    }
    return n;
}
static int pd_near(int i, int x, int y, int tol) {
    return i < g_pd.n && abs((int)g_pd.pt[i].x - x) <= tol && abs((int)g_pd.pt[i].y - y) <= tol;
}
/* Structural invariants any path from GetPath must obey. Returns 1 if ok. */
static int pd_valid(void) {
    int i, run = 0;
    if (g_pd.n == 0) return 1;
    if ((g_pd.ty[0] & ~PT_CLOSEFIGURE) != PT_MOVETO) return 0;
    for (i = 0; i < g_pd.n; i++) {
        BYTE b = g_pd.ty[i] & ~PT_CLOSEFIGURE;
        if (b != PT_MOVETO && b != PT_LINETO && b != PT_BEZIERTO) return 0;
        if (b == PT_BEZIERTO) run++;
        else { if (run % 3) return 0; run = 0; }
        if (i > 0 && (g_pd.ty[i - 1] & PT_CLOSEFIGURE) && b != PT_MOVETO) return 0;
    }
    return run % 3 == 0;
}
static int pd_count_type(BYTE base) {
    int i, n = 0;
    for (i = 0; i < g_pd.n; i++) if ((g_pd.ty[i] & ~PT_CLOSEFIGURE) == base) n++;
    return n;
}
static int pd_count_close(void) {
    int i, n = 0;
    for (i = 0; i < g_pd.n; i++) if (g_pd.ty[i] & PT_CLOSEFIGURE) n++;
    return n;
}

static int gui_objs(void) {
    DWORD (WINAPI *g)(HANDLE, DWORD) = (void *)t_proc("GetGuiResources");
    return g ? (int)g(GetCurrentProcess(), 0) : -1;
}

/* Close and evaluate: begin path, run nothing; helper to fetch a closed path of a shape */
#define LE_IS(v) (GetLastError() == (DWORD)(v))

#define BP(dc)   U(BeginPath)(dc)
#define EP(dc)   U(EndPath)(dc)
#define AP(dc)   U(AbortPath)(dc)
#define CF(dc)   U(CloseFigure)(dc)
#define FP(dc)   U(FillPath)(dc)
#define SP(dc)   U(StrokePath)(dc)
#define SFP(dc)  U(StrokeAndFillPath)(dc)
#define LT(dc,x,y) U(LineTo)(dc,x,y)
#define MT(dc,x,y) U(MoveToEx)(dc,x,y,NULL)
#define GPN(dc)  U(GetPath)(dc,NULL,NULL,0)
#define POLYGON4(dc,l,t,r,b) do { POINT q_[4] = {{l,t},{r,t},{r,b},{l,b}}; U(Polygon)(dc,q_,4); } while (0)
#endif
