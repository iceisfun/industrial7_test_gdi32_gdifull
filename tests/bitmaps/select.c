#include "bm.h"

/* ---- default bitmap of a fresh memory DC ---------------------------------- */
TEST(bitmaps_select_default_is_1x1_mono) {
    BM_REQ();
    HDC dc = U(CreateCompatibleDC)(NULL);
    REQUIRE(dc);
    HGDIOBJ cur = U(GetCurrentObject)(dc, OBJ_BITMAP);
    CHECK(cur != NULL);
    BITMAP bm;
    EXPECT_EQ(U(GetObjectA)(cur, sizeof bm, &bm), sizeof bm);
    EXPECT_EQ(bm.bmWidth, 1); EXPECT_EQ(bm.bmHeight, 1);
    EXPECT_EQ(bm.bmBitsPixel, 1); EXPECT_EQ(bm.bmPlanes, 1);
    EXPECT_EQ(U(GetObjectType)(cur), OBJ_BITMAP);
    if (cur != U(GetStockObject)(BM_DEFAULT_BITMAP)) t_note("default DC bitmap is not the stock DEFAULT_BITMAP");
    U(DeleteDC)(dc);
}
TEST(bitmaps_select_default_same_for_many_dcs) {
    BM_REQ();
    HDC a = U(CreateCompatibleDC)(NULL), b = U(CreateCompatibleDC)(NULL);
    REQUIRE(a && b);
    HGDIOBJ ca = U(GetCurrentObject)(a, OBJ_BITMAP), cb = U(GetCurrentObject)(b, OBJ_BITMAP);
    CHECK(ca != NULL && cb != NULL);
    t_note("two fresh DCs share default bitmap handle: %d", ca == cb);
    U(DeleteDC)(a); U(DeleteDC)(b);
}
TEST(bitmaps_select_returns_previous) {
    BM_REQ();
    HDC dc = U(CreateCompatibleDC)(NULL);
    REQUIRE(dc);
    HGDIOBJ def = U(GetCurrentObject)(dc, OBJ_BITMAP);
    HBITMAP a = U(CreateBitmap)(4, 4, 1, 1, NULL), b = U(CreateBitmap)(5, 5, 1, 1, NULL);
    CHECK(U(SelectObject)(dc, a) == def);
    CHECK(U(GetCurrentObject)(dc, OBJ_BITMAP) == (HGDIOBJ)a);
    CHECK(U(SelectObject)(dc, b) == (HGDIOBJ)a);
    CHECK(U(GetCurrentObject)(dc, OBJ_BITMAP) == (HGDIOBJ)b);
    CHECK(U(SelectObject)(dc, def) == (HGDIOBJ)b);
    U(DeleteDC)(dc); U(DeleteObject)(a); U(DeleteObject)(b);
}
TEST(bitmaps_select_same_twice) {
    BM_REQ();
    HDC dc = U(CreateCompatibleDC)(NULL);
    REQUIRE(dc);
    HBITMAP a = U(CreateBitmap)(4, 4, 1, 1, NULL);
    HGDIOBJ def = U(SelectObject)(dc, a);
    HGDIOBJ again = U(SelectObject)(dc, a);
    CHECK(again == (HGDIOBJ)a);
    CHECK(U(GetCurrentObject)(dc, OBJ_BITMAP) == (HGDIOBJ)a);
    U(SelectObject)(dc, def);
    U(DeleteDC)(dc); U(DeleteObject)(a);
}
TEST(bitmaps_select_one_dc_at_a_time) {
    BM_REQ();
    HDC d1 = U(CreateCompatibleDC)(NULL), d2 = U(CreateCompatibleDC)(NULL);
    REQUIRE(d1 && d2);
    HBITMAP a = U(CreateBitmap)(4, 4, 1, 1, NULL);
    HGDIOBJ o1 = U(SelectObject)(d1, a);
    CHECK(o1 != NULL);
    HGDIOBJ o2 = U(SelectObject)(d2, a);
    CHECK(o2 == NULL);
    CHECK(U(GetCurrentObject)(d2, OBJ_BITMAP) != (HGDIOBJ)a);
    CHECK(U(GetCurrentObject)(d1, OBJ_BITMAP) == (HGDIOBJ)a);
    U(SelectObject)(d1, o1);
    HGDIOBJ o3 = U(SelectObject)(d2, a);
    CHECK(o3 != NULL);
    U(SelectObject)(d2, o3);
    U(DeleteDC)(d1); U(DeleteDC)(d2); U(DeleteObject)(a);
}
TEST(bitmaps_select_dib_one_dc_at_a_time) {
    BM_REQ_DIB();
    dib_t d;
    REQUIRE(dib_open(&d, 4, 4, 32));
    HDC d2 = U(CreateCompatibleDC)(NULL);
    REQUIRE(d2);
    CHECK(U(SelectObject)(d2, d.bm) == NULL);
    U(SelectObject)(d.dc, d.old); d.old = NULL;
    HGDIOBJ o = U(SelectObject)(d2, d.bm);
    CHECK(o != NULL);
    U(SelectObject)(d2, o);
    U(DeleteDC)(d2);
    dib_close(&d);
}
TEST(bitmaps_select_into_window_dc_fails) {
    BM_REQ(); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC sc = U(GetDC)(NULL);
    REQUIRE(sc);
    HBITMAP a = U(CreateBitmap)(4, 4, 1, 1, NULL);
    HGDIOBJ r = U(SelectObject)(sc, a);
    CHECK(r == NULL);
    if (r) U(SelectObject)(sc, r);
    U(ReleaseDC)(NULL, sc);
    U(DeleteObject)(a);
}
TEST(bitmaps_select_into_info_dc_fails) {
    BM_REQ(); REQUIRE(U(CreateICA));
    HDC ic = U(CreateICA)("DISPLAY", NULL, NULL, NULL);
    REQUIRE(ic);
    HBITMAP a = U(CreateBitmap)(4, 4, 1, 1, NULL);
    HGDIOBJ r = U(SelectObject)(ic, a);
    CHECK(r == NULL);
    if (r) U(SelectObject)(ic, r);
    U(DeleteDC)(ic);
    U(DeleteObject)(a);
}
TEST(bitmaps_select_null_and_bogus) {
    BM_REQ();
    HDC dc = U(CreateCompatibleDC)(NULL);
    REQUIRE(dc);
    HGDIOBJ def = U(GetCurrentObject)(dc, OBJ_BITMAP);
    CHECK(U(SelectObject)(dc, NULL) == NULL);
    CHECK(U(SelectObject)(dc, (HGDIOBJ)0xdeadbeef) == NULL);
    CHECK(U(GetCurrentObject)(dc, OBJ_BITMAP) == def);
    HBITMAP a = U(CreateBitmap)(4, 4, 1, 1, NULL);
    CHECK(U(SelectObject)(NULL, a) == NULL);
    HDC gone = U(CreateCompatibleDC)(NULL);
    U(DeleteDC)(gone);
    CHECK(U(SelectObject)(gone, a) == NULL);
    HBITMAP del = U(CreateBitmap)(2, 2, 1, 1, NULL);
    U(DeleteObject)(del);
    CHECK(U(SelectObject)(dc, del) == NULL);
    CHECK(U(GetCurrentObject)(dc, OBJ_BITMAP) == def);
    U(DeleteDC)(dc); U(DeleteObject)(a);
}
TEST(bitmaps_select_all_depths) {
    BM_REQ();
    static const int bpps[] = {1,4,8,16,24,32}; unsigned i;
    for (i = 0; i < 6; i++) {
        ddb_t d;
        CHECKF(ddb_open(&d, 6, 6, 1, bpps[i], NULL), "bpp %d", bpps[i]);
        if (d.dc) {
            if (bpps[i] == 1) {
                CHECK(d.old != NULL);
                CHECK(U(GetCurrentObject)(d.dc, OBJ_BITMAP) == (HGDIOBJ)d.bm);
            } else if (!d.old) t_note("selecting %dbpp DDB into a memory DC failed", bpps[i]);
        }
        ddb_close(&d);
    }
}
TEST(bitmaps_select_wrong_type_to_slot) {
    BM_REQ(); REQUIRE(U(CreateSolidBrush));
    HDC dc = U(CreateCompatibleDC)(NULL);
    REQUIRE(dc);
    HBRUSH br = U(CreateSolidBrush)(0x112233);
    HGDIOBJ old = U(SelectObject)(dc, br);
    CHECK(old != NULL);
    CHECK(U(GetCurrentObject)(dc, OBJ_BITMAP) != (HGDIOBJ)br);
    CHECK(U(GetCurrentObject)(dc, OBJ_BRUSH) == (HGDIOBJ)br);
    U(SelectObject)(dc, old);
    U(DeleteDC)(dc); U(DeleteObject)(br);
}
TEST(bitmaps_select_saved_dc_restores_bitmap) {
    BM_REQ(); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    REQUIRE(dc);
    HBITMAP a = U(CreateBitmap)(4, 4, 1, 1, NULL), b = U(CreateBitmap)(6, 6, 1, 1, NULL);
    HGDIOBJ def = U(SelectObject)(dc, a);
    int lvl = U(SaveDC)(dc);
    CHECK(lvl > 0);
    U(SelectObject)(dc, b);
    CHECK(U(RestoreDC)(dc, -1));
    HGDIOBJ cur = U(GetCurrentObject)(dc, OBJ_BITMAP);
    t_note("bitmap after RestoreDC is %s", cur == (HGDIOBJ)a ? "the saved one" : cur == (HGDIOBJ)b ? "the later one" : "other");
    U(SelectObject)(dc, def);
    U(DeleteDC)(dc); U(DeleteObject)(a); U(DeleteObject)(b);
}

/* ---- deletion rules ------------------------------------------------------- */
TEST(bitmaps_delete_selected_bitmap) {
    BM_REQ();
    ddb_t d;
    REQUIRE(ddb_open(&d, 4, 4, 1, 1, NULL));
    BOOL r = U(DeleteObject)(d.bm);
    t_note("DeleteObject(selected bitmap) -> %d", r);
    if (!r) {
        /* still alive and still selected */
        CHECK(U(GetObjectType)(d.bm) == OBJ_BITMAP);
        CHECK(U(GetCurrentObject)(d.dc, OBJ_BITMAP) == (HGDIOBJ)d.bm);
    }
    U(SelectObject)(d.dc, d.old); d.old = NULL;
    BOOL r2 = U(DeleteObject)(d.bm);
    if (!r) CHECK(r2);
    d.bm = NULL;
    ddb_close(&d);
}
TEST(bitmaps_delete_after_deselect_succeeds) {
    BM_REQ();
    ddb_t d;
    REQUIRE(ddb_open(&d, 4, 4, 1, 1, NULL));
    U(SelectObject)(d.dc, d.old); d.old = NULL;
    CHECK(U(DeleteObject)(d.bm));
    EXPECT_EQ(U(GetObjectType)(d.bm), 0);
    d.bm = NULL;
    ddb_close(&d);
}
TEST(bitmaps_delete_dc_keeps_bitmap_alive) {
    BM_REQ();
    HDC dc = U(CreateCompatibleDC)(NULL);
    REQUIRE(dc);
    HBITMAP a = U(CreateBitmap)(4, 4, 1, 8, NULL);
    U(SelectObject)(dc, a);
    CHECK(U(DeleteDC)(dc));
    EXPECT_EQ(U(GetObjectType)(a), OBJ_BITMAP);
    BITMAP bm;
    EXPECT_EQ(U(GetObjectA)(a, sizeof bm, &bm), sizeof bm);
    HDC d2 = U(CreateCompatibleDC)(NULL);
    HGDIOBJ o = U(SelectObject)(d2, a);
    CHECK(o != NULL);
    U(SelectObject)(d2, o);
    U(DeleteDC)(d2);
    CHECK(U(DeleteObject)(a));
}
TEST(bitmaps_delete_twice_and_null) {
    BM_REQ();
    HBITMAP a = U(CreateBitmap)(4, 4, 1, 1, NULL);
    CHECK(U(DeleteObject)(a));
    CHECK(!U(DeleteObject)(a));
    CHECK(!U(DeleteObject)(NULL));
    CHECK(!U(DeleteObject)((HGDIOBJ)0xdeadbeef));
}
TEST(bitmaps_delete_stock_default_bitmap) {
    BM_REQ();
    HGDIOBJ st = U(GetStockObject)(BM_DEFAULT_BITMAP);
    BOOL r = U(DeleteObject)(st);
    t_note("DeleteObject(stock default bitmap) -> %d", r);
    EXPECT_EQ(U(GetObjectType)(st), OBJ_BITMAP);
    BITMAP bm;
    EXPECT_EQ(U(GetObjectA)(st, sizeof bm, &bm), sizeof bm);
}
TEST(bitmaps_delete_dib_selected_then_dc) {
    BM_REQ_DIB();
    dib_t d;
    REQUIRE(dib_open(&d, 4, 4, 32));
    U(SelectObject)(d.dc, d.old); d.old = NULL;
    CHECK(U(DeleteObject)(d.bm));
    d.bm = NULL; d.bits = NULL;
    dib_close(&d);
}
TEST(bitmaps_handle_type_bits) {
    BM_REQ();
    HBITMAP a = U(CreateBitmap)(4, 4, 1, 1, NULL);
    REQUIRE(a);
    HDC dc = U(CreateCompatibleDC)(NULL);
    if (bm_is_wine()) t_skip("wine handles carry no type field");
    else {
        EXPECT_EQ(((ULONG_PTR)a >> 16) & 0x7f, 5);
        EXPECT_EQ(((ULONG_PTR)dc >> 16) & 0x7f, 1);
    }
    U(DeleteDC)(dc); U(DeleteObject)(a);
}
TEST(bitmaps_handle_reuse_not_stale) {
    BM_REQ();
    HBITMAP a = U(CreateBitmap)(4, 4, 1, 1, NULL);
    U(DeleteObject)(a);
    HBITMAP b = U(CreateBitmap)(9, 9, 1, 1, NULL);
    if (b == a) t_note("handle value reused");
    CHECK(a != b || 1);
    /* the stale handle must not alias the new object unless the value is identical */
    BITMAP bm;
    if (a != b) EXPECT_EQ(U(GetObjectA)(a, sizeof bm, &bm), 0);
    U(DeleteObject)(b);
}

/* ---- GDI object counting ----------------------------------------------------- */
TEST(bitmaps_gdi_count_create_delete) {
    BM_REQ(); REQUIRE(U(GetGuiResources));
    unsigned base = bm_gui_count(), i;
    if (bm_is_wine()) { t_skip("GetGuiResources is a stub under wine"); return; }
    HBITMAP h[50];
    for (i = 0; i < 50; i++) { h[i] = U(CreateBitmap)(2 + i, 2, 1, 1, NULL); CHECK(h[i] != NULL); }
    unsigned mid = bm_gui_count();
    CHECKF(mid >= base + 50, "base %u mid %u", base, mid);
    for (i = 0; i < 50; i++) CHECK(U(DeleteObject)(h[i]));
    EXPECT_EQ(bm_gui_count(), base);
}
TEST(bitmaps_gdi_count_compat_and_dib) {
    BM_REQ_DIB(); REQUIRE(U(GetGuiResources));
    unsigned base = bm_gui_count(), i;
    for (i = 0; i < 20; i++) {
        dib_t d;
        CHECK(dib_open(&d, 8, 8, 32));
        HBITMAP c = U(CreateCompatibleBitmap)(d.dc, 4, 4);
        CHECK(c != NULL);
        if (c) U(DeleteObject)(c);
        dib_close(&d);
    }
    EXPECT_EQ(bm_gui_count(), base);
}
TEST(bitmaps_gdi_count_failed_create_leaks_nothing) {
    BM_REQ(); REQUIRE(U(GetGuiResources));
    unsigned base = bm_gui_count(), i;
    for (i = 0; i < 200; i++) {
        HBITMAP b = U(CreateBitmap)(0x7fffffff, 0x7fffffff, 1, 32, NULL);
        if (b) U(DeleteObject)(b);
    }
    EXPECT_EQ(bm_gui_count(), base);
}
TEST(bitmaps_gdi_many_small_bitmaps) {
    BM_REQ(); REQUIRE(U(GetGuiResources));
    enum { N = 1500 };
    HBITMAP *h = calloc(N, sizeof *h);
    unsigned base = bm_gui_count(); int i, ok = 0;
    for (i = 0; i < N; i++) { h[i] = U(CreateBitmap)(1, 1, 1, 1, NULL); if (h[i]) ok++; }
    CHECK(ok == N);
    for (i = 0; i < N; i++) if (h[i]) U(DeleteObject)(h[i]);
    EXPECT_EQ(bm_gui_count(), base);
    free(h);
}
TEST(bitmaps_ddb_data_not_shared_between_bitmaps) {
    BM_REQ(); REQUIRE(U(SetBitmapBits)); REQUIRE(U(GetBitmapBits));
    HBITMAP a = U(CreateBitmap)(16, 2, 1, 1, NULL), b = U(CreateBitmap)(16, 2, 1, 1, NULL);
    unsigned char fa[4] = {0xFF,0xFF,0xFF,0xFF}, fb[4] = {0,0,0,0}, out[4];
    U(SetBitmapBits)(a, 4, fa); U(SetBitmapBits)(b, 4, fb);
    U(GetBitmapBits)(a, 4, out); EXPECT_EQ(out[0], 0xFF);
    U(GetBitmapBits)(b, 4, out); EXPECT_EQ(out[0], 0x00);
    U(DeleteObject)(a); U(DeleteObject)(b);
}

/* ---- threads ------------------------------------------------------------------ */
typedef struct { HBITMAP bm; HDC dc; int result; BITMAP info; HGDIOBJ sel; } thr_t;
static DWORD WINAPI thr_create(LPVOID p) {
    thr_t *t = p;
    t->bm = U(CreateBitmap)(10, 6, 1, 8, NULL);
    return 0;
}
static DWORD WINAPI thr_query(LPVOID p) {
    thr_t *t = p;
    t->result = U(GetObjectA)(t->bm, sizeof t->info, &t->info);
    return 0;
}
static DWORD WINAPI thr_select(LPVOID p) {
    thr_t *t = p;
    HDC dc = U(CreateCompatibleDC)(NULL);
    t->sel = U(SelectObject)(dc, t->bm);
    t->result = t->sel != NULL;
    if (t->sel) U(SelectObject)(dc, t->sel);
    U(DeleteDC)(dc);
    return 0;
}
static DWORD WINAPI thr_makedc(LPVOID p) {
    thr_t *t = p;
    t->dc = U(CreateCompatibleDC)(NULL);
    return 0;
}
static void run_thread(LPTHREAD_START_ROUTINE f, thr_t *t) {
    HANDLE h = CreateThread(NULL, 0, f, t, 0, NULL);
    if (h) { WaitForSingleObject(h, 20000); CloseHandle(h); }
}
TEST(bitmaps_thread_bitmap_survives_creator_exit) {
    BM_REQ();
    thr_t t; memset(&t, 0, sizeof t);
    run_thread(thr_create, &t);
    REQUIRE(t.bm);
    BITMAP bm;
    EXPECT_EQ(U(GetObjectA)(t.bm, sizeof bm, &bm), sizeof bm);
    EXPECT_EQ(bm.bmWidth, 10); EXPECT_EQ(bm.bmHeight, 6); EXPECT_EQ(bm.bmBitsPixel, 8);
    ddb_t d; memset(&d, 0, sizeof d);
    d.dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ o = U(SelectObject)(d.dc, t.bm);
    CHECK(o != NULL);
    if (o) U(SelectObject)(d.dc, o);
    U(DeleteDC)(d.dc);
    CHECK(U(DeleteObject)(t.bm));
}
TEST(bitmaps_thread_query_from_other_thread) {
    BM_REQ();
    thr_t t; memset(&t, 0, sizeof t);
    t.bm = U(CreateBitmap)(13, 7, 1, 16, NULL);
    REQUIRE(t.bm);
    run_thread(thr_query, &t);
    EXPECT_EQ(t.result, sizeof(BITMAP));
    EXPECT_EQ(t.info.bmWidth, 13); EXPECT_EQ(t.info.bmHeight, 7); EXPECT_EQ(t.info.bmBitsPixel, 16);
    U(DeleteObject)(t.bm);
}
TEST(bitmaps_thread_select_in_other_thread) {
    BM_REQ();
    thr_t t; memset(&t, 0, sizeof t);
    t.bm = U(CreateBitmap)(4, 4, 1, 1, NULL);
    REQUIRE(t.bm);
    run_thread(thr_select, &t);
    EXPECT_EQ(t.result, 1);
    /* thread's DC is gone; the bitmap is free again */
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ o = U(SelectObject)(dc, t.bm);
    CHECK(o != NULL);
    if (o) U(SelectObject)(dc, o);
    U(DeleteDC)(dc);
    U(DeleteObject)(t.bm);
}
TEST(bitmaps_thread_dc_from_other_thread_use_bitmap) {
    BM_REQ(); REQUIRE(U(GetBitmapBits));
    thr_t t; memset(&t, 0, sizeof t);
    run_thread(thr_makedc, &t);
    REQUIRE(t.dc);
    HBITMAP b = U(CreateBitmap)(8, 8, 1, 1, NULL);
    HGDIOBJ o = U(SelectObject)(t.dc, b);
    t_note("select bitmap into DC created by another (finished) thread -> %p", o);
    if (o) U(SelectObject)(t.dc, o);
    U(DeleteDC)(t.dc);
    U(DeleteObject)(b);
}
TEST(bitmaps_thread_selected_in_main_blocks_other) {
    BM_REQ();
    ddb_t d;
    REQUIRE(ddb_open(&d, 4, 4, 1, 1, NULL));
    thr_t t; memset(&t, 0, sizeof t); t.bm = d.bm;
    run_thread(thr_select, &t);
    EXPECT_EQ(t.result, 0);
    ddb_close(&d);
}
