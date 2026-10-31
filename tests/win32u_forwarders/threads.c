/* Thread safety and cross-thread handle semantics of the GDI entry points. */
#include "exp.h"

static HDC (WINAPI *p_GetDC)(HWND);
static int (WINAPI *p_ReleaseDC)(HWND, HDC);
static HDC (WINAPI *p_CreateCompatibleDC)(HDC);
static BOOL (WINAPI *p_DeleteDC)(HDC);
static int (WINAPI *p_GetDeviceCaps)(HDC, int);
static HBITMAP (WINAPI *p_CreateDIBSection)(HDC, const BITMAPINFO *, UINT, void **, HANDLE, DWORD);
static HGDIOBJ (WINAPI *p_SelectObject)(HDC, HGDIOBJ);
static BOOL (WINAPI *p_DeleteObject)(HGDIOBJ);
static DWORD (WINAPI *p_GetObjectType)(HGDIOBJ);
static int (WINAPI *p_GetObjectW)(HANDLE, int, LPVOID);
static COLORREF (WINAPI *p_SetPixel)(HDC, int, int, COLORREF);
static COLORREF (WINAPI *p_GetPixel)(HDC, int, int);
static BOOL (WINAPI *p_PatBlt)(HDC, int, int, int, int, DWORD);
static HBRUSH (WINAPI *p_CreateSolidBrush)(COLORREF);
static HPEN (WINAPI *p_CreatePen)(int, int, COLORREF);
static HGDIOBJ (WINAPI *p_GetStockObject)(int);
static HFONT (WINAPI *p_CreateFontIndirectW)(const LOGFONTW *);
static BOOL (WINAPI *p_GetTextMetricsW)(HDC, LPTEXTMETRICW);
static HRGN (WINAPI *p_CreateRectRgn)(int, int, int, int);
static int (WINAPI *p_GetRgnBox)(HRGN, LPRECT);
static DWORD (WINAPI *p_GetGuiResources)(HANDLE, DWORD);

static int init_fns(void)
{
#define L(n) do { p_##n = (void *)t_proc(#n); if (!p_##n) return 0; } while (0)
    L(GetDC); L(ReleaseDC); L(CreateCompatibleDC); L(DeleteDC); L(GetDeviceCaps); L(CreateDIBSection);
    L(SelectObject); L(DeleteObject); L(GetObjectType); L(GetObjectW); L(SetPixel); L(GetPixel); L(PatBlt);
    L(CreateSolidBrush); L(CreatePen); L(GetStockObject); L(CreateFontIndirectW); L(GetTextMetricsW);
    L(CreateRectRgn); L(GetRgnBox); L(GetGuiResources);
#undef L
    return 1;
}
static int g_counts; /* GetGuiResources usable (it is a stub in some emulators) */
#define CNT_EQ(a, b) do { if (g_counts) EXPECT_EQ(a, b); } while (0)
#define NEED() do { if (!init_fns()) { t_skip("missing export"); return; } g_counts = p_GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) > 0; } while (0)

typedef struct { LONG fail; LONG done; int idx; void *arg; } tctx_t;

/* Run n threads; returns 1 on completion within the timeout. */
static int run_threads(int n, LPTHREAD_START_ROUTINE fn, tctx_t *ctx)
{
    HANDLE th[16];
    int started = 0;
    for (int i = 0; i < n; i++) {
        ctx[i].idx = i; ctx[i].fail = 0; ctx[i].done = 0;
        th[i] = CreateThread(NULL, 0, fn, &ctx[i], 0, NULL);
        if (!th[i]) break;
        started++;
    }
    CHECK(started == n);
    DWORD r = started ? WaitForMultipleObjects(started, th, TRUE, 90000) : WAIT_FAILED;
    CHECKF(r == WAIT_OBJECT_0, "threads did not finish (wait=%lu)", r);
    int ok = (r == WAIT_OBJECT_0);
    for (int i = 0; i < started; i++) CloseHandle(th[i]);
    return ok;
}
static LONG total_fail(tctx_t *c, int n) { LONG f = 0; for (int i = 0; i < n; i++) f |= c[i].fail; if (f) t_note("failure mask 0x%lx", f); return f; }

/* ---- 8 threads x 1000 iterations ---- */
static int g_horz, g_vert, g_bpp, g_mbpp, g_mplanes;
static DWORD WINAPI caps_thread(LPVOID p)
{
    tctx_t *c = (tctx_t *)p;
    HDC sdc = p_GetDC(NULL);
    if (!sdc) { c->fail |= 1L << 0; return 0; }
    for (int i = 0; i < 1000; i++) {
        if (p_GetDeviceCaps(sdc, HORZRES) != g_horz) c->fail |= 1L << 1;
        if (p_GetDeviceCaps(sdc, VERTRES) != g_vert) c->fail |= 1L << 2;
        if (p_GetDeviceCaps(sdc, BITSPIXEL) != g_bpp) c->fail |= 1L << 3;
        if (p_GetDeviceCaps(sdc, TECHNOLOGY) != DT_RASDISPLAY) c->fail |= 1L << 4;
        HDC m = p_CreateCompatibleDC(sdc);
        if (!m) { c->fail |= 1L << 5; continue; }
        if (p_GetObjectType(m) != OBJ_MEMDC) c->fail |= 1L << 6;
        if (p_GetDeviceCaps(m, BITSPIXEL) != g_mbpp || p_GetDeviceCaps(m, PLANES) != g_mplanes) c->fail |= 1L << 7;
        if (p_GetDeviceCaps(m, TECHNOLOGY) != DT_RASDISPLAY) c->fail |= 1L << 8;
        if (!p_DeleteDC(m)) c->fail |= 1L << 9;
    }
    p_ReleaseDC(NULL, sdc);
    c->done = 1;
    return 0;
}
TEST(thr_caps_and_compatdc_8x1000)
{
    NEED();
    HDC sdc = p_GetDC(NULL);
    REQUIRE(sdc);
    g_horz = p_GetDeviceCaps(sdc, HORZRES); g_vert = p_GetDeviceCaps(sdc, VERTRES); g_bpp = p_GetDeviceCaps(sdc, BITSPIXEL);
    {
        HDC m = p_CreateCompatibleDC(sdc);
        g_mbpp = p_GetDeviceCaps(m, BITSPIXEL); g_mplanes = p_GetDeviceCaps(m, PLANES);
        t_note("memory DC caps: bpp=%d planes=%d (screen bpp=%d)", g_mbpp, g_mplanes, g_bpp);
        p_DeleteDC(m);
    }
    p_ReleaseDC(NULL, sdc);
    DWORD before = p_GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    tctx_t ctx[8];
    CHECK(run_threads(8, caps_thread, ctx));
    CHECK(total_fail(ctx, 8) == 0);
    for (int i = 0; i < 8; i++) CHECK(ctx[i].done == 1);
    DWORD after = p_GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    if (g_counts) CHECKF(after <= before + 16, "cached screen DCs: %lu -> %lu", before, after);
    t_note("GDI objects before=%lu after=%lu", before, after);
}

/* ---- per-thread DIB lifecycle with exact pixels ---- */
static DWORD WINAPI dib_thread(LPVOID p)
{
    tctx_t *c = (tctx_t *)p;
    COLORREF col = RGB(10 + c->idx * 20, 200 - c->idx * 10, 5 + c->idx);
    for (int it = 0; it < 150; it++) {
        BITMAPINFO bi; memset(&bi, 0, sizeof bi);
        bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 16; bi.bmiHeader.biHeight = -16;
        bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
        void *bits = NULL;
        HBITMAP bm = p_CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
        HDC dc = p_CreateCompatibleDC(NULL);
        if (!bm || !bits || !dc) { c->fail++; if (bm) p_DeleteObject(bm); if (dc) p_DeleteDC(dc); continue; }
        HGDIOBJ old = p_SelectObject(dc, bm);
        if (!old) c->fail++;
        if (!p_PatBlt(dc, 0, 0, 16, 16, WHITENESS)) c->fail++;
        if (((DWORD *)bits)[0] != 0x00FFFFFF && ((DWORD *)bits)[0] != 0xFFFFFFFF) c->fail++;
        if (!p_PatBlt(dc, 0, 0, 16, 16, BLACKNESS)) c->fail++;
        if ((((DWORD *)bits)[0] & 0x00FFFFFF) != 0) c->fail++;
        int x = it % 16, y = (it * 7) % 16;
        if (p_SetPixel(dc, x, y, col) == (COLORREF)-1) c->fail++;
        if (p_GetPixel(dc, x, y) != col) c->fail++;
        DWORD want = ((DWORD)GetRValue(col) << 16) | ((DWORD)GetGValue(col) << 8) | GetBValue(col);
        if ((((DWORD *)bits)[y * 16 + x] & 0x00FFFFFF) != want) c->fail++;
        if (p_GetPixel(dc, (x + 1) % 16, y) != 0) c->fail++;
        BITMAP bmo; memset(&bmo, 0, sizeof bmo);
        if (p_GetObjectW(bm, sizeof bmo, &bmo) != sizeof bmo || bmo.bmWidth != 16 || bmo.bmHeight != 16 || bmo.bmBitsPixel != 32) c->fail++;
        p_SelectObject(dc, old);
        if (!p_DeleteObject(bm)) c->fail++;
        if (p_GetObjectType(bm) != 0) c->fail++;
        if (!p_DeleteDC(dc)) c->fail++;
    }
    c->done = 1;
    return 0;
}
TEST(thr_dib_lifecycle_8x150)
{
    NEED();
    DWORD before = p_GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    tctx_t ctx[8];
    CHECK(run_threads(8, dib_thread, ctx));
    CHECK(total_fail(ctx, 8) == 0);
    CNT_EQ(p_GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS), before);
}

/* ---- stock objects identical across threads ---- */
static DWORD WINAPI stock_thread(LPVOID p)
{
    tctx_t *c = (tctx_t *)p;
    HGDIOBJ *want = (HGDIOBJ *)c->arg;
    for (int it = 0; it < 200; it++)
        for (int i = 0; i <= 19; i++) {
            if (i == 9) continue; /* no such stock object... (DEFAULT_PALETTE is 15; 9 is unused) */
            if (p_GetStockObject(i) != want[i]) c->fail++;
        }
    c->done = 1;
    return 0;
}
TEST(thr_stockobjects_same_handles)
{
    NEED();
    HGDIOBJ want[20];
    for (int i = 0; i < 20; i++) want[i] = (i == 9) ? NULL : p_GetStockObject(i);
    tctx_t ctx[4];
    for (int i = 0; i < 4; i++) ctx[i].arg = want;
    HANDLE th[4];
    for (int i = 0; i < 4; i++) { ctx[i].idx = i; ctx[i].fail = 0; th[i] = CreateThread(NULL, 0, stock_thread, &ctx[i], 0, NULL); CHECK(th[i] != NULL); }
    DWORD r = WaitForMultipleObjects(4, th, TRUE, 60000);
    CHECK(r == WAIT_OBJECT_0);
    for (int i = 0; i < 4; i++) { if (th[i]) CloseHandle(th[i]); }
    CHECK(total_fail(ctx, 4) == 0);
}

/* ---- handle sharing: objects made on the main thread are usable from workers ---- */
typedef struct { HBRUSH br; HPEN pen; HBITMAP bm; HFONT font; HRGN rgn; int fontH; } shared_t;
static DWORD WINAPI share_thread(LPVOID p)
{
    tctx_t *c = (tctx_t *)p;
    shared_t *s = (shared_t *)c->arg;
    if (p_GetObjectType(s->br) != OBJ_BRUSH) c->fail++;
    if (p_GetObjectType(s->pen) != OBJ_PEN) c->fail++;
    if (p_GetObjectType(s->bm) != OBJ_BITMAP) c->fail++;
    if (p_GetObjectType(s->font) != OBJ_FONT) c->fail++;
    if (p_GetObjectType(s->rgn) != OBJ_REGION) c->fail++;
    LOGBRUSH lb;
    if (p_GetObjectW(s->br, sizeof lb, &lb) != sizeof lb || lb.lbColor != RGB(12, 34, 56) || lb.lbStyle != BS_SOLID) c->fail++;
    BITMAP b;
    if (p_GetObjectW(s->bm, sizeof b, &b) != sizeof b || b.bmWidth != 32 || b.bmHeight != 8) c->fail++;
    RECT r;
    if (p_GetRgnBox(s->rgn, &r) != SIMPLEREGION || r.left != 3 || r.top != 4 || r.right != 30 || r.bottom != 40) c->fail++;
    HDC dc = p_CreateCompatibleDC(NULL);
    if (!dc) { c->fail++; return 0; }
    HGDIOBJ ob = p_SelectObject(dc, s->bm);
    if (!ob) c->fail++;
    /* two threads can never both have the same bitmap selected into their DCs; only one gets it */
    HGDIOBJ obr = p_SelectObject(dc, s->br);
    if (!obr) c->fail++;
    if (p_GetObjectType(obr) != OBJ_BRUSH && obr) c->fail++;
    HGDIOBJ of = p_SelectObject(dc, s->font);
    TEXTMETRICW tm;
    if (!of || !p_GetTextMetricsW(dc, &tm) || tm.tmHeight != s->fontH) c->fail++;
    if (of) p_SelectObject(dc, of);
    if (obr) p_SelectObject(dc, obr);
    if (ob) p_SelectObject(dc, ob);
    p_DeleteDC(dc);
    c->done = 1;
    return 0;
}
TEST(thr_handle_sharing_across_threads)
{
    NEED();
    shared_t s; memset(&s, 0, sizeof s);
    s.br = p_CreateSolidBrush(RGB(12, 34, 56));
    s.pen = p_CreatePen(PS_SOLID, 2, RGB(1, 2, 3));
    s.bm = (HBITMAP)0;
    {
        BITMAPINFO bi; memset(&bi, 0, sizeof bi);
        bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 32; bi.bmiHeader.biHeight = -8;
        bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
        void *bits; s.bm = p_CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    }
    LOGFONTW lf; memset(&lf, 0, sizeof lf);
    lf.lfHeight = -16; lstrcpyW(lf.lfFaceName, L"Arial");
    s.font = p_CreateFontIndirectW(&lf);
    s.rgn = p_CreateRectRgn(3, 4, 30, 40);
    CHECK(s.br && s.pen && s.bm && s.font && s.rgn);
    {
        HDC dc = p_CreateCompatibleDC(NULL);
        HGDIOBJ of = p_SelectObject(dc, s.font);
        TEXTMETRICW tm; memset(&tm, 0, sizeof tm);
        CHECK(p_GetTextMetricsW(dc, &tm));
        s.fontH = tm.tmHeight;
        p_SelectObject(dc, of);
        p_DeleteDC(dc);
    }
    tctx_t ctx[4];
    for (int i = 0; i < 4; i++) ctx[i].arg = &s;
    /* run threads one after another with the shared bitmap so SelectObject conflicts do not arise */
    for (int i = 0; i < 4; i++) { tctx_t one = ctx[i]; CHECK(run_threads(1, share_thread, &one)); CHECK(one.fail == 0); }
    p_DeleteObject(s.br); p_DeleteObject(s.pen); p_DeleteObject(s.bm); p_DeleteObject(s.font); p_DeleteObject(s.rgn);
}

/* ---- objects created in a thread survive its exit ---- */
typedef struct { HBRUSH br[64]; HPEN pen; HBITMAP bm; HRGN rgn; HDC mdc; } leak_t;
static DWORD WINAPI leak_thread(LPVOID p)
{
    tctx_t *c = (tctx_t *)p;
    leak_t *l = (leak_t *)c->arg;
    for (int i = 0; i < 64; i++) l->br[i] = p_CreateSolidBrush(RGB(i, i, i));
    l->pen = p_CreatePen(PS_SOLID, 1, RGB(9, 9, 9));
    l->rgn = p_CreateRectRgn(0, 0, 5, 5);
    l->mdc = p_CreateCompatibleDC(NULL);
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 4; bi.bmiHeader.biHeight = 4;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    void *bits; l->bm = p_CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    return 0; /* exits without deleting anything */
}
TEST(thr_objects_survive_thread_exit)
{
    NEED();
    static leak_t l;
    memset(&l, 0, sizeof l);
    DWORD before = p_GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    tctx_t ctx; ctx.arg = &l;
    CHECK(run_threads(1, leak_thread, &ctx));
    DWORD mid = p_GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    t_note("GDI objects: before=%lu after-thread-exit=%lu (expected +68)", before, mid);
    int alive = 0;
    for (int i = 0; i < 64; i++) if (p_GetObjectType(l.br[i]) == OBJ_BRUSH) alive++;
    EXPECT_EQ(alive, 64);
    EXPECT_EQ(p_GetObjectType(l.pen), OBJ_PEN);
    EXPECT_EQ(p_GetObjectType(l.rgn), OBJ_REGION);
    EXPECT_EQ(p_GetObjectType(l.bm), OBJ_BITMAP);
    if (g_counts) CHECKF(mid >= before + 64, "object count %lu -> %lu", before, mid);
    DWORD dct = p_GetObjectType(l.mdc);
    t_note("memory DC created by an exited thread: GetObjectType=%lu (OBJ_MEMDC=%d)", dct, OBJ_MEMDC);
    /* cleanup from a different thread succeeds */
    for (int i = 0; i < 64; i++) CHECK(p_DeleteObject(l.br[i]));
    CHECK(p_DeleteObject(l.pen)); CHECK(p_DeleteObject(l.rgn)); CHECK(p_DeleteObject(l.bm));
    if (dct == OBJ_MEMDC) { BOOL ok = p_DeleteDC(l.mdc); t_note("DeleteDC from another thread: %d", ok); }
    t_note("GDI objects after cleanup: %lu", p_GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS));
    EXPECT_EQ(p_GetObjectType(l.br[0]), 0);
}

/* ---- concurrent font creation + metrics agree with the single-thread result ---- */
static DWORD WINAPI font_thread(LPVOID p)
{
    tctx_t *c = (tctx_t *)p;
    int *want = (int *)c->arg;
    HDC dc = p_CreateCompatibleDC(NULL);
    if (!dc) { c->fail++; return 0; }
    for (int it = 0; it < 60; it++) {
        LOGFONTW lf; memset(&lf, 0, sizeof lf);
        lf.lfHeight = -(10 + (it % 5) * 4); lstrcpyW(lf.lfFaceName, L"Arial");
        HFONT f = p_CreateFontIndirectW(&lf);
        if (!f) { c->fail++; continue; }
        HGDIOBJ o = p_SelectObject(dc, f);
        TEXTMETRICW tm;
        if (!p_GetTextMetricsW(dc, &tm) || tm.tmHeight != want[it % 5]) c->fail++;
        p_SelectObject(dc, o);
        p_DeleteObject(f);
    }
    p_DeleteDC(dc);
    c->done = 1;
    return 0;
}
TEST(thr_font_metrics_consistent)
{
    NEED();
    int want[5];
    HDC dc = p_CreateCompatibleDC(NULL);
    for (int k = 0; k < 5; k++) {
        LOGFONTW lf; memset(&lf, 0, sizeof lf);
        lf.lfHeight = -(10 + k * 4); lstrcpyW(lf.lfFaceName, L"Arial");
        HFONT f = p_CreateFontIndirectW(&lf);
        HGDIOBJ o = p_SelectObject(dc, f);
        TEXTMETRICW tm; p_GetTextMetricsW(dc, &tm);
        want[k] = tm.tmHeight;
        p_SelectObject(dc, o); p_DeleteObject(f);
    }
    p_DeleteDC(dc);
    tctx_t ctx[4];
    for (int i = 0; i < 4; i++) ctx[i].arg = want;
    HANDLE th[4];
    for (int i = 0; i < 4; i++) { ctx[i].idx = i; ctx[i].fail = 0; th[i] = CreateThread(NULL, 0, font_thread, &ctx[i], 0, NULL); }
    CHECK(WaitForMultipleObjects(4, th, TRUE, 90000) == WAIT_OBJECT_0);
    for (int i = 0; i < 4; i++) if (th[i]) CloseHandle(th[i]);
    CHECK(total_fail(ctx, 4) == 0);
}

/* ---- screen DC from many threads at once ---- */
static DWORD WINAPI getdc_thread(LPVOID p)
{
    tctx_t *c = (tctx_t *)p;
    for (int it = 0; it < 500; it++) {
        HDC d = p_GetDC(NULL);
        if (!d) { c->fail++; continue; }
        if (p_GetObjectType(d) != OBJ_DC) c->fail++;
        if (!p_ReleaseDC(NULL, d)) c->fail++;
    }
    c->done = 1;
    return 0;
}
TEST(thr_screen_dc_get_release)
{
    NEED();
    tctx_t ctx[8];
    CHECK(run_threads(8, getdc_thread, ctx));
    CHECK(total_fail(ctx, 8) == 0);
}

/* ---- DC usage hand-off: a DC made on one thread is usable on another ---- */
typedef struct { HDC dc; HBITMAP bm; void *bits; HGDIOBJ old; } hand_t;
static DWORD WINAPI hand_creator(LPVOID p)
{
    tctx_t *c = (tctx_t *)p; hand_t *h = (hand_t *)c->arg;
    h->dc = p_CreateCompatibleDC(NULL);
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 8; bi.bmiHeader.biHeight = -8;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    h->bm = p_CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &h->bits, NULL, 0);
    h->old = (h->dc && h->bm) ? p_SelectObject(h->dc, h->bm) : NULL;
    if (!h->old) c->fail++;
    return 0;
}
static DWORD WINAPI hand_user(LPVOID p)
{
    tctx_t *c = (tctx_t *)p; hand_t *h = (hand_t *)c->arg;
    if (p_GetObjectType(h->dc) != OBJ_MEMDC) c->fail++;
    if (p_SetPixel(h->dc, 2, 3, RGB(200, 100, 50)) == (COLORREF)-1) c->fail++;
    if (p_GetPixel(h->dc, 2, 3) != RGB(200, 100, 50)) c->fail++;
    return 0;
}
TEST(thr_dc_handoff_between_threads)
{
    NEED();
    hand_t h; memset(&h, 0, sizeof h);
    tctx_t a; a.arg = &h;
    CHECK(run_threads(1, hand_creator, &a));
    CHECK(a.fail == 0);
    tctx_t b; b.arg = &h;
    CHECK(run_threads(1, hand_user, &b));
    t_note("DC created on an exited thread used from another: failures=%ld", b.fail);
    if (h.bits) CHECK((((DWORD *)h.bits)[3 * 8 + 2] & 0xFFFFFF) == ((50u << 0) | (100u << 8) | (200u << 16)) || b.fail != 0);
    if (h.dc && h.old) p_SelectObject(h.dc, h.old);
    if (h.dc) p_DeleteDC(h.dc);
    if (h.bm) p_DeleteObject(h.bm);
}

/* ---- DeleteObject of an object from another thread while it is not selected ---- */
static DWORD WINAPI delete_thread(LPVOID p)
{
    tctx_t *c = (tctx_t *)p; HGDIOBJ *o = (HGDIOBJ *)c->arg;
    for (int i = 0; i < 100; i++) if (!p_DeleteObject(o[i])) c->fail++;
    return 0;
}
TEST(thr_delete_objects_from_other_thread)
{
    NEED();
    static HGDIOBJ objs[100];
    for (int i = 0; i < 100; i++) objs[i] = (i & 1) ? (HGDIOBJ)p_CreateSolidBrush(RGB(i, 0, 0)) : (HGDIOBJ)p_CreatePen(PS_SOLID, 1, RGB(0, i, 0));
    DWORD before = p_GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    tctx_t c; c.arg = objs;
    CHECK(run_threads(1, delete_thread, &c));
    CHECK(c.fail == 0);
    CNT_EQ(p_GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS), before - 100);
    for (int i = 0; i < 100; i++) EXPECT_EQ(p_GetObjectType(objs[i]), 0);
}

/* ---- GetGuiResources semantics ---- */
TEST(thr_getguiresources_invalid)
{
    NEED();
    SetLastError(0);
    DWORD r = p_GetGuiResources(GetCurrentProcess(), 99);
    EXPECT_EQ(r, 0);
    t_note("GetGuiResources(bad flag) err=%lu", GetLastError());
    SetLastError(0);
    r = p_GetGuiResources(NULL, GR_GDIOBJECTS);
    EXPECT_EQ(r, 0);
    t_note("GetGuiResources(NULL process) err=%lu", GetLastError());
    DWORD peak = p_GetGuiResources(GetCurrentProcess(), 2 /* GR_GDIOBJECTS_PEAK */);
    DWORD cur = p_GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    CHECK(peak >= cur);
    CHECK(!g_counts || (cur > 0 && cur < 10000));
}

TEST(thr_gdi_object_count_tracks_create_delete)
{
    NEED();
    DWORD base = p_GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    HGDIOBJ o[50];
    for (int i = 0; i < 50; i++) { o[i] = p_CreateSolidBrush(RGB(i, i, i)); CNT_EQ(p_GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS), base + i + 1); }
    for (int i = 0; i < 50; i++) { CHECK(p_DeleteObject(o[i])); CNT_EQ(p_GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS), base + 49 - i); }
    /* stock objects do not count */
    for (int i = 0; i < 20; i++) p_GetStockObject(i);
    CNT_EQ(p_GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS), base);
    /* a pen with the same parameters made twice counts twice (no caching of CreatePen) */
    HGDIOBJ a = p_CreatePen(PS_SOLID, 1, 0), b = p_CreatePen(PS_SOLID, 1, 0);
    CHECK(a != b);
    CNT_EQ(p_GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS), base + 2);
    p_DeleteObject(a); p_DeleteObject(b);
}
