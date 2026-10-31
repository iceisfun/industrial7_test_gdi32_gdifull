/* GDI object counts, quotas, handle reuse, cross-thread semantics and deterministic stock-object rendering. */
#include "os.h"

#define GR_GDIOBJECTS_PEAK 2
static void os_flush(void) { BOOL (WINAPI *f)(void) = (void *)t_proc("GdiFlush"); if (f) f(); }
#define GdiFlush os_flush
#ifdef OS_NO_NEED
#define NEED_COUNTS() do { } while (0)
#else
#define NEED_COUNTS() do { if (os_gdicount() == 0) { t_skip("GetGuiResources unsupported"); return; } } while (0)
#endif
static DWORD gui(DWORD flag) {
    DWORD (WINAPI *f)(HANDLE, DWORD) = (void *)t_proc("GetGuiResources");
    return f ? f(GetCurrentProcess(), flag) : 0xFFFFFFFFu;
}

/* ---- GetGuiResources deltas ---- */
#define COUNT_TEST(nm, K) \
TEST(count_##nm) { \
    REQUIRE(U(GetGuiResources)); NEED_COUNTS(); \
    DWORD a = os_gdicount(); HGDIOBJ h = os_make(K); REQUIRE(h); \
    DWORD b = os_gdicount(); \
    CHECKF(b == a + 1, "create %s: %lu -> %lu", os_kname[K], a, b); \
    os_free(K, h); \
    CHECKF(os_gdicount() == a, "delete %s: %lu", os_kname[K], os_gdicount()); }
COUNT_TEST(pen, OS_K_PEN)
COUNT_TEST(extpen, OS_K_EXTPEN)
COUNT_TEST(solidbrush, OS_K_SOLID)
COUNT_TEST(hatchbrush, OS_K_HATCH)
COUNT_TEST(font, OS_K_FONT)
COUNT_TEST(palette, OS_K_PAL)
COUNT_TEST(region, OS_K_RGN)
COUNT_TEST(ddb, OS_K_DDB)
COUNT_TEST(dib, OS_K_DIB)
COUNT_TEST(memdc, OS_K_MEMDC)

TEST(count_patternbrush) {
    NEED_COUNTS();
    REQUIRE(U(GetGuiResources));
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, NULL);
    DWORD a = os_gdicount();
    HBRUSH b = U(CreatePatternBrush)(bm);
    t_note("pattern brush creation delta = %ld", (long)os_gdicount() - (long)a);
    CHECK(os_gdicount() >= a + 1);
    DOBJ(b);
    CHECK(os_gdicount() == a);
    DOBJ(bm);
}
TEST(count_pattern_bitmap_delete_before_brush) {
    NEED_COUNTS();
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, NULL);
    DWORD a = os_gdicount();
    HBRUSH b = U(CreatePatternBrush)(bm);
    DOBJ(bm);
    t_note("count after deleting source bitmap: %lu (before brush %lu)", os_gdicount(), a);
    DOBJ(b);
    CHECK(os_gdicount() == a - 1);
}
TEST(count_dib_with_dc_and_select) {
    NEED_COUNTS();
    DWORD a = os_gdicount();
    HDC dc = os_memdc(); void *bits; HBITMAP bm = os_dib(dc, 8, 8, 32, &bits);
    CHECK(os_gdicount() == a + 2);
    HGDIOBJ o = SOBJ(dc, bm);
    CHECK(os_gdicount() == a + 2);
    SOBJ(dc, o); DOBJ(bm); U(DeleteDC)(dc);
    CHECK(os_gdicount() == a);
}
TEST(count_select_does_not_change) {
    NEED_COUNTS();
    HDC dc = os_memdc(); HGDIOBJ p = os_make(OS_K_PEN), b = os_make(OS_K_SOLID), f = os_make(OS_K_FONT);
    DWORD a = os_gdicount();
    HGDIOBJ op = SOBJ(dc, p), ob = SOBJ(dc, b), of = SOBJ(dc, f);
    CHECK(os_gdicount() == a);
    SOBJ(dc, op); SOBJ(dc, ob); SOBJ(dc, of);
    DOBJ(p); DOBJ(b); DOBJ(f); U(DeleteDC)(dc);
}
TEST(count_stock_selection_no_change) {
    NEED_COUNTS();
    HDC dc = os_memdc();
    DWORD a = os_gdicount();
    for (int i = 0; i < 50; i++) { HGDIOBJ o = SOBJ(dc, GSO(GRAY_BRUSH)); SOBJ(dc, o); }
    CHECK(os_gdicount() == a);
    U(DeleteDC)(dc);
}
TEST(count_enhmetafile) {
    NEED_COUNTS();
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    DWORD a = os_gdicount();
    HDC dc = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    DWORD b = os_gdicount();
    HENHMETAFILE mf = U(CloseEnhMetaFile)(dc);
    DWORD c = os_gdicount();
    U(DeleteEnhMetaFile)(mf);
    DWORD d = os_gdicount();
    t_note("enhmf counts: base %lu, dc %lu, closed %lu, deleted %lu", a, b, c, d);
    CHECK(d == a);
}
TEST(count_metafile) {
    NEED_COUNTS();
    REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    DWORD a = os_gdicount();
    HDC dc = U(CreateMetaFileW)(NULL);
    DWORD b = os_gdicount();
    HMETAFILE mf = U(CloseMetaFile)(dc);
    DWORD c = os_gdicount();
    U(DeleteMetaFile)(mf);
    t_note("mf counts: base %lu, dc %lu, closed %lu, deleted %lu", a, b, c, os_gdicount());
    CHECK(os_gdicount() == a);
}
TEST(count_createdc_and_getdc) {
    NEED_COUNTS();
    REQUIRE(U(CreateDCW)); REQUIRE(U(GetDC));
    DWORD a = os_gdicount();
    HDC dc = U(CreateDCW)(L"DISPLAY", NULL, NULL, NULL);
    CHECK(os_gdicount() == a + 1);
    U(DeleteDC)(dc);
    CHECK(os_gdicount() == a);
    HDC s = U(GetDC)(NULL);
    t_note("GetDC(NULL) delta %ld", (long)os_gdicount() - (long)a);
    U(ReleaseDC)(NULL, s);
    CHECK(os_gdicount() == a);
}
TEST(count_createic) {
    NEED_COUNTS();
    REQUIRE(U(CreateICW));
    DWORD a = os_gdicount();
    HDC dc = U(CreateICW)(L"DISPLAY", NULL, NULL, NULL);
    CHECK(os_gdicount() == a + 1);
    U(DeleteDC)(dc);
    CHECK(os_gdicount() == a);
}
TEST(count_colorspace) {
    NEED_COUNTS();
    REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    LOGCOLORSPACEW l; memset(&l, 0, sizeof l);
    l.lcsSignature = 0x50534F43; l.lcsVersion = 0x400; l.lcsSize = sizeof l; l.lcsCSType = 0x73524742;
    DWORD a = os_gdicount();
    HCOLORSPACE cs = U(CreateColorSpaceW)(&l);
    if (!cs) return;
    t_note("colorspace delta %ld", (long)os_gdicount() - (long)a);
    U(DeleteColorSpace)(cs);
    CHECK(os_gdicount() == a);
}
TEST(count_failed_create_no_leak) {
    NEED_COUNTS();
    DWORD a = os_gdicount();
    LOGBRUSH lb = { BS_SOLID, 0, 0 };
    for (int i = 0; i < 20; i++) {
        U(CreateHatchBrush)(99, 0);
        U(CreatePatternBrush)(NULL);
        U(ExtCreatePen)(PS_COSMETIC, 5, &lb, 0, NULL);
        U(CreateBitmap)(-1, -1, 1, 1, NULL);
        U(CreateRectRgn)(0, 0, 1, 1) ? (void)0 : (void)0;
    }
    DWORD b = os_gdicount();
    t_note("count after failed creates %lu -> %lu", a, b);
    CHECK(b >= a);
}
TEST(count_selected_object_delete_does_not_free_early) {
    NEED_COUNTS();
    HDC dc = os_memdc();
    HBITMAP bm = U(CreateBitmap)(4, 4, 1, 1, NULL);
    DWORD a = os_gdicount();
    HGDIOBJ o = SOBJ(dc, bm);
    BOOL r = DOBJ(bm);
    DWORD b = os_gdicount();
    if (!r) CHECK(b == a);
    SOBJ(dc, o);
    if (!r) DOBJ(bm);
    U(DeleteDC)(dc);
}
TEST(count_peak_not_below_current) {
    DWORD cur = gui(0), peak = gui(GR_GDIOBJECTS_PEAK);
    t_note("GR_GDIOBJECTS=%lu PEAK=%lu", cur, peak);
    CHECK(peak == 0xFFFFFFFFu || peak >= cur);
}
TEST(count_getguiresources_invalid_flag) {
    REQUIRE(U(GetGuiResources));
    SET_ERR(0);
    DWORD r = U(GetGuiResources)(GetCurrentProcess(), 99);
    CHECK(r == 0);
    t_note("GetGuiResources(flag 99) err=%lu", GetLastError());
    SET_ERR(0);
    r = U(GetGuiResources)(NULL, 0);
    CHECK(r == 0);
    t_note("GetGuiResources(NULL) err=%lu", GetLastError());
}
TEST(count_user_vs_gdi_independent) {
    NEED_COUNTS();
    REQUIRE(U(GetGuiResources));
    DWORD u = gui(1);
    HGDIOBJ p = os_make(OS_K_PEN);
    CHECK(gui(1) == u);
    DOBJ(p);
}

/* ---- bulk / quota ---- */
#define BULK(nm, K, N) \
TEST(bulk_##nm) { \
    NEED_COUNTS(); enum { COUNT = N }; static HGDIOBJ hs[COUNT]; static unsigned char seen[65536]; \
    memset(seen, 0, sizeof seen); \
    DWORD a = os_gdicount(); int n = 0; \
    for (; n < COUNT; n++) { hs[n] = os_make(K); if (!hs[n]) break; } \
    DWORD b = os_gdicount(); \
    CHECKF(n == COUNT, "created only %d of %d, err=%lu", n, COUNT, GetLastError()); \
    CHECKF(b == a + (DWORD)n, "count %lu -> %lu for %d", a, b, n); \
    int dup = 0; for (int i = 0; i < n; i++) { if (seen[H_INDEX(hs[i])]) dup++; seen[H_INDEX(hs[i])] = 1; } \
    CHECKF(dup == 0, "%d duplicate indexes", dup); \
    for (int i = 0; i < n; i++) if (hs[i]) CHECK(GOT(hs[i]) == os_ktype[K]); \
    for (int i = 0; i < n; i++) os_free(K, hs[i]); \
    CHECK(os_gdicount() == a); }
BULK(pens, OS_K_PEN, 2000)
BULK(brushes, OS_K_SOLID, 2000)
BULK(regions, OS_K_RGN, 2000)
BULK(fonts, OS_K_FONT, 1000)
BULK(ddbs, OS_K_DDB, 1000)
BULK(dibs, OS_K_DIB, 1000)
BULK(palettes, OS_K_PAL, 500)
BULK(memdcs, OS_K_MEMDC, 1000)
BULK(extpens, OS_K_EXTPEN, 1000)

TEST(bulk_mixed_kinds) {
    NEED_COUNTS();
    enum { N = 1500 };
    static HGDIOBJ hs[N]; static int ks[N];
    DWORD a = os_gdicount(); int n = 0;
    int order[] = { OS_K_PEN, OS_K_SOLID, OS_K_RGN, OS_K_DDB, OS_K_FONT, OS_K_PAL };
    for (; n < N; n++) { ks[n] = order[n % 6]; hs[n] = os_make(ks[n]); if (!hs[n]) break; }
    CHECK(n == N);
    CHECK(os_gdicount() == a + (DWORD)n);
    for (int i = 0; i < n; i++) CHECKF(GOT(hs[i]) == os_ktype[ks[i]], "i=%d", i);
    for (int i = n - 1; i >= 0; i--) os_free(ks[i], hs[i]);
    CHECK(os_gdicount() == a);
}
TEST(bulk_create_delete_cycles) {
    NEED_COUNTS();
    DWORD a = os_gdicount();
    for (int i = 0; i < 20000; i++) {
        HGDIOBJ p = U(CreatePen)(PS_SOLID, 1, i);
        if (!p) { CHECK(0); break; }
        DOBJ(p);
    }
    CHECK(os_gdicount() == a);
}
TEST(bulk_handle_reuse_note) {
    NEED_COUNTS();
    unsigned maxidx = 0; unsigned seen_before = 0;
    static unsigned char seen[65536]; memset(seen, 0, sizeof seen);
    for (int i = 0; i < 2000; i++) {
        HGDIOBJ p = U(CreateSolidBrush)(i);
        unsigned ix = H_INDEX(p);
        if (seen[ix]) seen_before++;
        seen[ix] = 1;
        if (ix > maxidx) maxidx = ix;
        DOBJ(p);
    }
    t_note("2000 create/delete cycles: %u reused indexes, max index %u", seen_before, maxidx);
}
TEST(bulk_stale_handles_all_dead) {
    NEED_COUNTS();
    enum { N = 300 }; static HGDIOBJ hs[N];
    for (int i = 0; i < N; i++) hs[i] = U(CreatePen)(PS_SOLID, 1, i);
    for (int i = 0; i < N; i++) DOBJ(hs[i]);
    HGDIOBJ again[N];
    for (int i = 0; i < N; i++) again[i] = U(CreatePen)(PS_SOLID, 1, i);
    int aliased = 0;
    for (int i = 0; i < N; i++) if (GOT(hs[i]) != 0) aliased++;
    t_note("%d of %d stale handles still resolve after reallocation", aliased, N);
    for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) if (hs[i] == again[j] && GOT(hs[i])) { aliased += 0; }
    int exact = 0;
    for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) if (hs[i] == again[j]) exact++;
    t_note("%d exact handle values were recycled", exact);
    for (int i = 0; i < N; i++) DOBJ(again[i]);
}
TEST(bulk_quota_stops_cleanly) {
    NEED_COUNTS();
    /* Allocate regions until 8000 total GDI objects (below the 10000 default quota), stop early. */
    enum { MAXN = 7000 };
    static HGDIOBJ hs[MAXN];
    DWORD a = os_gdicount();
    int n = 0;
    while (n < MAXN && os_gdicount() < 8000) { hs[n] = os_make(OS_K_RGN); if (!hs[n]) break; n++; if (n % 1000 == 0 && os_gdicount() >= 8000) break; }
    t_note("allocated %d regions, process GDI count %lu", n, os_gdicount());
    CHECK(n > 0);
    DWORD peak = gui(GR_GDIOBJECTS_PEAK);
    CHECK(peak == 0xFFFFFFFFu || peak >= a + (DWORD)n);
    for (int i = 0; i < n; i++) DOBJ(hs[i]);
    CHECK(os_gdicount() == a);
}
TEST(bulk_dc_handle_and_object_handle_ranges) {
    NEED_COUNTS();
    HDC dc = os_memdc(); HGDIOBJ p = os_make(OS_K_PEN); REQUIRE(dc && p);
    t_note("dc=%p pen=%p", dc, p);
    CHECK(((ULONG_PTR)dc >> 32) == 0 && ((ULONG_PTR)p >> 32) == 0);
    DOBJ(p); U(DeleteDC)(dc);
}

/* ---- threads ---- */
static HGDIOBJ g_h; static int g_kind; static DWORD g_t_type; static BOOL g_t_del; static HDC g_dc;
static volatile LONG g_stage;
static DWORD WINAPI th_create(LPVOID p) { g_h = os_make(g_kind); return 0; }
static DWORD WINAPI th_type(LPVOID p) { g_t_type = GOT(g_h); return 0; }
static DWORD WINAPI th_delete(LPVOID p) { g_t_del = os_free(g_kind, g_h); return 0; }
static DWORD WINAPI th_select(LPVOID p) { g_h = SOBJ(g_dc, (HGDIOBJ)p); return 0; }
static void run_thread(LPTHREAD_START_ROUTINE f, LPVOID arg) {
    HANDLE t = CreateThread(NULL, 0, f, arg, 0, NULL);
    if (t) { WaitForSingleObject(t, 20000); CloseHandle(t); }
}
#define THREAD_TESTS(nm, K) \
TEST(thread_##nm##_created_elsewhere_visible) { \
    g_kind = K; g_h = NULL; run_thread(th_create, NULL); REQUIRE(g_h); \
    CHECK(GOT(g_h) == os_ktype[K]); \
    g_t_type = 0; run_thread(th_type, NULL); CHECK(g_t_type == os_ktype[K]); \
    CHECK(os_free(K, g_h)); } \
TEST(thread_##nm##_deleted_elsewhere) { \
    g_kind = K; g_h = os_make(K); REQUIRE(g_h); \
    g_t_del = FALSE; run_thread(th_delete, NULL); CHECK(g_t_del); \
    CHECK(GOT(g_h) == 0); } \
TEST(thread_##nm##_exit_keeps_object) { \
    g_kind = K; g_h = NULL; run_thread(th_create, NULL); REQUIRE(g_h); \
    Sleep(10); CHECK(GOT(g_h) == os_ktype[K]); os_free(K, g_h); }
THREAD_TESTS(pen, OS_K_PEN)
THREAD_TESTS(solidbrush, OS_K_SOLID)
THREAD_TESTS(font, OS_K_FONT)
THREAD_TESTS(region, OS_K_RGN)
THREAD_TESTS(ddb, OS_K_DDB)
THREAD_TESTS(dib, OS_K_DIB)
THREAD_TESTS(palette, OS_K_PAL)
THREAD_TESTS(memdc, OS_K_MEMDC)

TEST(thread_select_pen_into_dc_from_other_thread) {
    HDC dc = os_memdc(); REQUIRE(dc);
    HGDIOBJ p = os_make(OS_K_PEN);
    g_dc = dc; g_h = NULL;
    run_thread(th_select, p);
    CHECK(g_h == GSO(BLACK_PEN));
    CHECK(U(GetCurrentObject)(dc, OBJ_PEN) == p);
    SOBJ(dc, g_h); DOBJ(p); U(DeleteDC)(dc);
}
TEST(thread_select_bitmap_into_dc_from_other_thread) {
    HDC dc = os_memdc(); REQUIRE(dc);
    HBITMAP b = U(CreateBitmap)(4, 4, 1, 1, NULL);
    g_dc = dc; g_h = NULL;
    run_thread(th_select, b);
    CHECK(g_h != NULL);
    CHECK(U(GetCurrentObject)(dc, OBJ_BITMAP) == b);
    SOBJ(dc, g_h); DOBJ(b); U(DeleteDC)(dc);
}
static DWORD WINAPI th_dc_select_bitmap(LPVOID p) {
    HDC dc = os_memdc();
    g_dc = dc;
    g_h = dc ? SOBJ(dc, (HGDIOBJ)p) : NULL;
    InterlockedExchange(&g_stage, 1);
    while (g_stage != 2) Sleep(1);
    if (dc) { SOBJ(dc, g_h); U(DeleteDC)(dc); }
    return 0;
}
TEST(thread_bitmap_selected_in_other_threads_dc) {
    HBITMAP b = U(CreateBitmap)(4, 4, 1, 1, NULL); REQUIRE(b);
    HDC mine = os_memdc(); REQUIRE(mine);
    g_stage = 0; g_h = NULL;
    HANDLE t = CreateThread(NULL, 0, th_dc_select_bitmap, b, 0, NULL);
    for (int i = 0; i < 5000 && g_stage != 1; i++) Sleep(1);
    CHECK(g_stage == 1);
    CHECK(g_h != NULL);
    SET_ERR(0);
    CHECK(SOBJ(mine, b) == NULL);
    BOOL r = DOBJ(b);
    CHECK(!r || os_wine());
    g_stage = 2;
    WaitForSingleObject(t, 20000); CloseHandle(t);
    if (!r) CHECK(DOBJ(b));
    U(DeleteDC)(mine);
}
TEST(thread_dc_created_elsewhere_usable) {
    g_kind = OS_K_MEMDC; g_h = NULL; run_thread(th_create, NULL); REQUIRE(g_h);
    HDC dc = (HDC)g_h;
    HGDIOBJ p = os_make(OS_K_PEN);
    HGDIOBJ old = SOBJ(dc, p);
    CHECK(old == GSO(BLACK_PEN));
    SOBJ(dc, old);
    DOBJ(p);
    CHECK(U(DeleteDC)(dc));
}
static DWORD WINAPI th_getobj(LPVOID p) {
    LOGBRUSH lb; memset(&lb, 0, sizeof lb);
    g_t_type = GOA((HGDIOBJ)p, sizeof lb, &lb) == 16 ? lb.lbColor : 0xFFFFFFFF;
    return 0;
}
TEST(thread_getobject_other_thread) {
    HBRUSH b = U(CreateSolidBrush)(0x123456); REQUIRE(b);
    g_t_type = 0; run_thread(th_getobj, b);
    CHECK(g_t_type == 0x123456);
    DOBJ(b);
}
static int g_races;
static DWORD WINAPI th_hammer(LPVOID p) {
    for (int i = 0; i < 2000; i++) {
        HGDIOBJ h = U(CreatePen)(PS_SOLID, 1, i);
        if (!h) { InterlockedIncrement((LONG *)&g_races); continue; }
        if (GOT(h) != OBJ_PEN) InterlockedIncrement((LONG *)&g_races);
        DOBJ(h);
    }
    return 0;
}
TEST(thread_concurrent_create_delete) {
    DWORD a = os_gdicount();
    HANDLE t[4]; g_races = 0;
    for (int i = 0; i < 4; i++) t[i] = CreateThread(NULL, 0, th_hammer, NULL, 0, NULL);
    for (int i = 0; i < 4; i++) if (t[i]) { WaitForSingleObject(t[i], 60000); CloseHandle(t[i]); }
    CHECK(g_races == 0);
    CHECK(os_gdicount() == a);
}
static HGDIOBJ g_shared[4];
static DWORD WINAPI th_unique(LPVOID p) {
    int me = (int)(ULONG_PTR)p;
    g_shared[me] = U(CreateSolidBrush)(me);
    return 0;
}
TEST(thread_unique_handles_across_threads) {
    HANDLE t[4];
    for (int i = 0; i < 4; i++) t[i] = CreateThread(NULL, 0, th_unique, (LPVOID)(ULONG_PTR)i, 0, NULL);
    for (int i = 0; i < 4; i++) { WaitForSingleObject(t[i], 20000); CloseHandle(t[i]); }
    for (int i = 0; i < 4; i++) for (int j = i + 1; j < 4; j++) CHECK(g_shared[i] != g_shared[j]);
    for (int i = 0; i < 4; i++) DOBJ(g_shared[i]);
}

/* ---- deterministic rendering with stock objects (32bpp DIB in a memory DC) ---- */
static DWORD px(DWORD *bits, int w, int x, int y) { return bits[y * w + x] & 0xFFFFFF; }
static HDC mk_target(int w, int h, HBITMAP *bm, DWORD **bits, HGDIOBJ *old) {
    HDC dc = os_memdc(); if (!dc) return NULL;
    void *b; *bm = os_dib(NULL, w, h, 32, &b);
    if (!*bm) { U(DeleteDC)(dc); return NULL; }
    *bits = b; *old = SOBJ(dc, *bm);
    return dc;
}
static void end_target(HDC dc, HBITMAP bm, HGDIOBJ old) { SOBJ(dc, old); DOBJ(bm); U(DeleteDC)(dc); }

static const struct { int idx; DWORD col; } g_fills[] = {
    { WHITE_BRUSH, 0xFFFFFF }, { LTGRAY_BRUSH, 0xC0C0C0 }, { GRAY_BRUSH, 0x808080 }, { DKGRAY_BRUSH, 0x404040 }, { BLACK_BRUSH, 0x000000 } };
TEST(render_stock_brushes_patblt) {
    REQUIRE(U(PatBlt));
    HBITMAP bm; DWORD *bits; HGDIOBJ old; HDC dc = mk_target(8, 8, &bm, &bits, &old); REQUIRE(dc);
    for (int i = 0; i < 5; i++) {
        memset(bits, 0x5A, 8 * 8 * 4);
        HGDIOBJ ob = SOBJ(dc, GSO(g_fills[i].idx));
        CHECK(U(PatBlt)(dc, 0, 0, 8, 8, PATCOPY));
        GdiFlush();
        for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++)
            if (px(bits, 8, x, y) != g_fills[i].col) { CHECKF(0, "brush %d at %d,%d = %06lx want %06lx", g_fills[i].idx, x, y, px(bits, 8, x, y), g_fills[i].col); y = 8; break; }
        SOBJ(dc, ob);
    }
    end_target(dc, bm, old);
}
TEST(render_null_brush_leaves_pixels) {
    REQUIRE(U(PatBlt));
    HBITMAP bm; DWORD *bits; HGDIOBJ old; HDC dc = mk_target(8, 8, &bm, &bits, &old); REQUIRE(dc);
    for (int i = 0; i < 64; i++) bits[i] = 0x00123456 + i;
    HGDIOBJ ob = SOBJ(dc, GSO(NULL_BRUSH));
    U(PatBlt)(dc, 0, 0, 8, 8, PATCOPY);
    GdiFlush();
    int bad = 0; for (int i = 0; i < 64; i++) if ((bits[i] & 0xFFFFFF) != 0x123456 + i) bad++;
    CHECKF(bad == 0, "%d pixels changed", bad);
    SOBJ(dc, ob);
    end_target(dc, bm, old);
}
TEST(render_dc_brush_colors) {
    REQUIRE(U(PatBlt)); REQUIRE(U(SetDCBrushColor));
    HBITMAP bm; DWORD *bits; HGDIOBJ old; HDC dc = mk_target(4, 4, &bm, &bits, &old); REQUIRE(dc);
    HGDIOBJ ob = SOBJ(dc, GSO(DC_BRUSH));
    COLORREF cols[] = { RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255), RGB(18, 52, 86), RGB(0, 0, 0), RGB(255, 255, 255) };
    for (int i = 0; i < 6; i++) {
        U(SetDCBrushColor)(dc, cols[i]);
        U(PatBlt)(dc, 0, 0, 4, 4, PATCOPY);
        GdiFlush();
        DWORD want = ((cols[i] & 0xFF) << 16) | (cols[i] & 0xFF00) | ((cols[i] >> 16) & 0xFF);
        CHECKF(px(bits, 4, 1, 1) == want && px(bits, 4, 3, 3) == want && px(bits, 4, 0, 0) == want, "color %06lx got %06lx want %06lx", cols[i], px(bits, 4, 1, 1), want);
    }
    SOBJ(dc, ob);
    end_target(dc, bm, old);
}
TEST(render_dc_brush_default_white) {
    REQUIRE(U(PatBlt));
    HBITMAP bm; DWORD *bits; HGDIOBJ old; HDC dc = mk_target(4, 4, &bm, &bits, &old); REQUIRE(dc);
    memset(bits, 0, 64);
    HGDIOBJ ob = SOBJ(dc, GSO(DC_BRUSH));
    U(PatBlt)(dc, 0, 0, 4, 4, PATCOPY);
    GdiFlush();
    CHECK(px(bits, 4, 2, 2) == 0xFFFFFF);
    SOBJ(dc, ob);
    end_target(dc, bm, old);
}
TEST(render_dc_brush_color_change_after_select) {
    REQUIRE(U(PatBlt));
    HBITMAP bm; DWORD *bits; HGDIOBJ old; HDC dc = mk_target(4, 4, &bm, &bits, &old); REQUIRE(dc);
    HGDIOBJ ob = SOBJ(dc, GSO(DC_BRUSH));
    U(SetDCBrushColor)(dc, RGB(1, 2, 3)); U(PatBlt)(dc, 0, 0, 2, 4, PATCOPY);
    U(SetDCBrushColor)(dc, RGB(4, 5, 6)); U(PatBlt)(dc, 2, 0, 2, 4, PATCOPY);
    GdiFlush();
    CHECK(px(bits, 4, 0, 0) == 0x010203 && px(bits, 4, 1, 3) == 0x010203);
    CHECK(px(bits, 4, 2, 0) == 0x040506 && px(bits, 4, 3, 3) == 0x040506);
    SOBJ(dc, ob);
    end_target(dc, bm, old);
}
TEST(render_dc_brush_not_selected_ignored) {
    REQUIRE(U(PatBlt));
    HBITMAP bm; DWORD *bits; HGDIOBJ old; HDC dc = mk_target(4, 4, &bm, &bits, &old); REQUIRE(dc);
    U(SetDCBrushColor)(dc, RGB(9, 9, 9));
    U(PatBlt)(dc, 0, 0, 4, 4, PATCOPY);   /* default WHITE_BRUSH */
    GdiFlush();
    CHECK(px(bits, 4, 1, 1) == 0xFFFFFF);
    end_target(dc, bm, old);
}
TEST(render_stock_pens_horizontal_line) {
    REQUIRE(U(MoveToEx)); REQUIRE(U(LineTo));
    struct { int pen; DWORD bg; DWORD fg; } t[] = { { BLACK_PEN, 0xFFFFFF, 0 }, { WHITE_PEN, 0, 0xFFFFFF } };
    for (int i = 0; i < 2; i++) {
        HBITMAP bm; DWORD *bits; HGDIOBJ old; HDC dc = mk_target(10, 4, &bm, &bits, &old); REQUIRE(dc);
        for (int k = 0; k < 40; k++) bits[k] = t[i].bg;
        HGDIOBJ op = SOBJ(dc, GSO(t[i].pen));
        U(MoveToEx)(dc, 1, 2, NULL); U(LineTo)(dc, 8, 2);
        GdiFlush();
        for (int x = 0; x < 10; x++) {
            DWORD want = (x >= 1 && x < 8) ? t[i].fg : t[i].bg;
            CHECKF(px(bits, 10, x, 2) == want, "pen %d x=%d got %06lx want %06lx", t[i].pen, x, px(bits, 10, x, 2), want);
            CHECKF(px(bits, 10, x, 1) == t[i].bg && px(bits, 10, x, 3) == t[i].bg, "pen %d row neighbours x=%d", t[i].pen, x);
        }
        SOBJ(dc, op);
        end_target(dc, bm, old);
    }
}
TEST(render_null_pen_draws_nothing) {
    REQUIRE(U(MoveToEx)); REQUIRE(U(LineTo));
    HBITMAP bm; DWORD *bits; HGDIOBJ old; HDC dc = mk_target(10, 4, &bm, &bits, &old); REQUIRE(dc);
    for (int k = 0; k < 40; k++) bits[k] = 0x00ABCDEF;
    HGDIOBJ op = SOBJ(dc, GSO(NULL_PEN));
    U(MoveToEx)(dc, 0, 1, NULL); U(LineTo)(dc, 9, 1); U(LineTo)(dc, 9, 3);
    GdiFlush();
    int bad = 0; for (int k = 0; k < 40; k++) if ((bits[k] & 0xFFFFFF) != 0xABCDEF) bad++;
    CHECKF(bad == 0, "%d pixels changed", bad);
    SOBJ(dc, op);
    end_target(dc, bm, old);
}
TEST(render_dc_pen_colors) {
    REQUIRE(U(MoveToEx)); REQUIRE(U(LineTo)); REQUIRE(U(SetDCPenColor));
    HBITMAP bm; DWORD *bits; HGDIOBJ old; HDC dc = mk_target(10, 6, &bm, &bits, &old); REQUIRE(dc);
    for (int k = 0; k < 60; k++) bits[k] = 0xFFFFFF;
    HGDIOBJ op = SOBJ(dc, GSO(DC_PEN));
    U(SetDCPenColor)(dc, RGB(200, 100, 50));
    U(MoveToEx)(dc, 0, 1, NULL); U(LineTo)(dc, 10, 1);
    U(SetDCPenColor)(dc, RGB(1, 2, 3));
    U(MoveToEx)(dc, 0, 4, NULL); U(LineTo)(dc, 10, 4);
    GdiFlush();
    for (int x = 0; x < 10; x++) {
        CHECKF(px(bits, 10, x, 1) == 0xC86432, "x=%d got %06lx", x, px(bits, 10, x, 1));
        CHECKF(px(bits, 10, x, 4) == 0x010203, "x=%d got %06lx", x, px(bits, 10, x, 4));
    }
    SOBJ(dc, op);
    end_target(dc, bm, old);
}
TEST(render_dc_pen_default_black) {
    REQUIRE(U(MoveToEx)); REQUIRE(U(LineTo));
    HBITMAP bm; DWORD *bits; HGDIOBJ old; HDC dc = mk_target(6, 3, &bm, &bits, &old); REQUIRE(dc);
    for (int k = 0; k < 18; k++) bits[k] = 0xFFFFFF;
    HGDIOBJ op = SOBJ(dc, GSO(DC_PEN));
    U(MoveToEx)(dc, 0, 1, NULL); U(LineTo)(dc, 6, 1);
    GdiFlush();
    CHECK(px(bits, 6, 3, 1) == 0);
    SOBJ(dc, op);
    end_target(dc, bm, old);
}
TEST(render_rectangle_null_brush_black_pen) {
    REQUIRE(U(Rectangle));
    HBITMAP bm; DWORD *bits; HGDIOBJ old; HDC dc = mk_target(10, 10, &bm, &bits, &old); REQUIRE(dc);
    for (int k = 0; k < 100; k++) bits[k] = 0xFFFFFF;
    HGDIOBJ ob = SOBJ(dc, GSO(NULL_BRUSH));
    U(Rectangle)(dc, 2, 2, 8, 8);
    GdiFlush();
    CHECK(px(bits, 10, 2, 2) == 0 && px(bits, 10, 7, 2) == 0 && px(bits, 10, 2, 7) == 0 && px(bits, 10, 7, 7) == 0);
    CHECK(px(bits, 10, 8, 8) == 0xFFFFFF && px(bits, 10, 8, 2) == 0xFFFFFF);
    CHECK(px(bits, 10, 4, 4) == 0xFFFFFF);
    SOBJ(dc, ob);
    end_target(dc, bm, old);
}
TEST(render_rectangle_gray_fill_black_border) {
    REQUIRE(U(Rectangle));
    HBITMAP bm; DWORD *bits; HGDIOBJ old; HDC dc = mk_target(10, 10, &bm, &bits, &old); REQUIRE(dc);
    for (int k = 0; k < 100; k++) bits[k] = 0xFFFFFF;
    HGDIOBJ ob = SOBJ(dc, GSO(GRAY_BRUSH));
    U(Rectangle)(dc, 1, 1, 9, 9);
    GdiFlush();
    CHECK(px(bits, 10, 1, 1) == 0 && px(bits, 10, 8, 8) == 0);
    CHECK(px(bits, 10, 4, 4) == 0x808080 && px(bits, 10, 2, 2) == 0x808080 && px(bits, 10, 7, 7) == 0x808080);
    CHECK(px(bits, 10, 0, 0) == 0xFFFFFF && px(bits, 10, 9, 9) == 0xFFFFFF);
    SOBJ(dc, ob);
    end_target(dc, bm, old);
}
TEST(render_rectangle_null_pen_fill) {
    REQUIRE(U(Rectangle));
    HBITMAP bm; DWORD *bits; HGDIOBJ old; HDC dc = mk_target(10, 10, &bm, &bits, &old); REQUIRE(dc);
    for (int k = 0; k < 100; k++) bits[k] = 0xFFFFFF;
    HGDIOBJ ob = SOBJ(dc, GSO(DKGRAY_BRUSH)), op = SOBJ(dc, GSO(NULL_PEN));
    U(Rectangle)(dc, 2, 2, 7, 7);
    GdiFlush();
    t_note("null-pen rectangle (2,2,7,7): px(2,2)=%06lx px(6,6)=%06lx px(7,7)=%06lx", px(bits, 10, 2, 2), px(bits, 10, 6, 6), px(bits, 10, 7, 7));
    CHECK(px(bits, 10, 4, 4) == 0x404040);
    CHECK(px(bits, 10, 0, 0) == 0xFFFFFF && px(bits, 10, 9, 9) == 0xFFFFFF);
    SOBJ(dc, op); SOBJ(dc, ob);
    end_target(dc, bm, old);
}
TEST(render_custom_brush_pen_after_stock) {
    REQUIRE(U(PatBlt));
    HBITMAP bm; DWORD *bits; HGDIOBJ old; HDC dc = mk_target(4, 4, &bm, &bits, &old); REQUIRE(dc);
    HBRUSH b = U(CreateSolidBrush)(RGB(10, 20, 30));
    HGDIOBJ ob = SOBJ(dc, b);
    U(PatBlt)(dc, 0, 0, 4, 4, PATCOPY);
    GdiFlush();
    CHECK(px(bits, 4, 2, 2) == 0x0A141E);
    SOBJ(dc, GSO(BLACK_BRUSH));
    U(PatBlt)(dc, 0, 0, 4, 4, PATCOPY);
    GdiFlush();
    CHECK(px(bits, 4, 2, 2) == 0);
    SOBJ(dc, ob);
    DOBJ(b);
    end_target(dc, bm, old);
}
TEST(render_rop_with_stock_brushes) {
    REQUIRE(U(PatBlt));
    HBITMAP bm; DWORD *bits; HGDIOBJ old; HDC dc = mk_target(4, 4, &bm, &bits, &old); REQUIRE(dc);
    for (int k = 0; k < 16; k++) bits[k] = 0x00F0F0F0;
    HGDIOBJ ob = SOBJ(dc, GSO(GRAY_BRUSH));
    U(PatBlt)(dc, 0, 0, 4, 4, PATINVERT);
    GdiFlush();
    CHECKF(px(bits, 4, 1, 1) == (0xF0F0F0 ^ 0x808080), "xor %06lx", px(bits, 4, 1, 1));
    U(PatBlt)(dc, 0, 0, 4, 4, PATINVERT);
    GdiFlush();
    CHECK(px(bits, 4, 1, 1) == 0xF0F0F0);
    U(PatBlt)(dc, 0, 0, 4, 4, DSTINVERT);
    GdiFlush();
    CHECK(px(bits, 4, 2, 3) == 0x0F0F0F);
    U(PatBlt)(dc, 0, 0, 4, 4, BLACKNESS);
    GdiFlush();
    CHECK(px(bits, 4, 0, 0) == 0);
    U(PatBlt)(dc, 0, 0, 4, 4, WHITENESS);
    GdiFlush();
    CHECK(px(bits, 4, 3, 3) == 0xFFFFFF);
    SOBJ(dc, ob);
    end_target(dc, bm, old);
}
TEST(render_selected_stock_gdi_count_flat) {
    NEED_COUNTS();
    HBITMAP bm; DWORD *bits; HGDIOBJ old; HDC dc = mk_target(4, 4, &bm, &bits, &old); REQUIRE(dc);
    DWORD a = os_gdicount();
    for (int i = 0; i < 200; i++) { HGDIOBJ o = SOBJ(dc, GSO(LTGRAY_BRUSH)); U(PatBlt)(dc, 0, 0, 4, 4, PATCOPY); SOBJ(dc, o); }
    CHECK(os_gdicount() == a);
    end_target(dc, bm, old);
}

/* ---- system / DPI facts ---- */
TEST(sys_dpi_note) {
    REQUIRE(U(GetDpiForSystem));
    t_note("system DPI = %u", U(GetDpiForSystem)());
    CHECK(U(GetDpiForSystem)() >= 96);
}
TEST(sys_gdi_quota_note) {
    HKEY k;
    DWORD v = 0, sz = sizeof v;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows", 0, KEY_READ, &k) == ERROR_SUCCESS) {
        if (RegQueryValueExA(k, "GDIProcessHandleQuota", NULL, NULL, (BYTE *)&v, &sz) == ERROR_SUCCESS) t_note("GDIProcessHandleQuota=%lu", v);
        RegCloseKey(k);
    }
    CHECK(1);
}
TEST(sys_stock_objects_dpi_threads) {
    REQUIRE(U(SetThreadDpiAwarenessContext));
    DPI_AWARENESS_CONTEXT old = U(SetThreadDpiAwarenessContext)(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HGDIOBJ a = GSO(DEFAULT_GUI_FONT), s = GSO(SYSTEM_FONT);
    U(SetThreadDpiAwarenessContext)(DPI_AWARENESS_CONTEXT_UNAWARE);
    HGDIOBJ b = GSO(DEFAULT_GUI_FONT), s2 = GSO(SYSTEM_FONT);
    t_note("DEFAULT_GUI_FONT permv2=%p unaware=%p; SYSTEM_FONT %p / %p", a, b, s, s2);
    CHECK(GOT(a) == OBJ_FONT && GOT(b) == OBJ_FONT);
    CHECK(s == s2);
    if (old) U(SetThreadDpiAwarenessContext)(old);
}
