#include "ep.h"

/* ---- GDI batching ---- */
TEST(ep_batch_default_is_20) {
    REQUIRE(U(GdiGetBatchLimit));
    DWORD v = U(GdiGetBatchLimit)();
    CHECKF(v == 20, "default batch limit %lu", v);
}
TEST(ep_batch_get_repeatable) {
    REQUIRE(U(GdiGetBatchLimit));
    DWORD a = U(GdiGetBatchLimit)();
    for (int i = 0; i < 100; i++) if (U(GdiGetBatchLimit)() != a) { CHECK(0); break; }
}
TEST(ep_batch_set_returns_previous_and_applies) {
    REQUIRE(U(GdiSetBatchLimit)); REQUIRE(U(GdiGetBatchLimit));
    DWORD orig = U(GdiGetBatchLimit)();
    DWORD prev = U(GdiSetBatchLimit)(5);
    CHECKF(prev == orig, "prev %lu orig %lu", prev, orig);
    CHECK(U(GdiGetBatchLimit)() == 5);
    prev = U(GdiSetBatchLimit)(orig);
    CHECKF(prev == 5, "prev %lu", prev);
    CHECK(U(GdiGetBatchLimit)() == orig);
}
#define BATCH_VAL(v) \
TEST(ep_batch_set_value_##v) { REQUIRE(U(GdiSetBatchLimit)); DWORD orig = U(GdiGetBatchLimit)(); \
    DWORD prev = U(GdiSetBatchLimit)(v); CHECKF(prev == orig, "prev %lu orig %lu", prev, orig); CHECK(U(GdiGetBatchLimit)() == v); U(GdiSetBatchLimit)(orig); }
BATCH_VAL(1) BATCH_VAL(2) BATCH_VAL(3) BATCH_VAL(7) BATCH_VAL(10) BATCH_VAL(16) BATCH_VAL(19) BATCH_VAL(20)
#define BATCH_BAD(n, v) \
TEST(ep_batch_set_invalid_##n) { REQUIRE(U(GdiSetBatchLimit)); DWORD orig = U(GdiGetBatchLimit)(); \
    DWORD r = U(GdiSetBatchLimit)(v); CHECKF(r == 0, "returned %lu", r); CHECKF(U(GdiGetBatchLimit)() == orig, "limit changed to %lu", U(GdiGetBatchLimit)()); U(GdiSetBatchLimit)(orig); }
BATCH_BAD(21, 21) BATCH_BAD(22, 22) BATCH_BAD(32, 32) BATCH_BAD(100, 100) BATCH_BAD(255, 255) BATCH_BAD(65536, 65536) BATCH_BAD(0x7fffffff, 0x7fffffff) BATCH_BAD(0x80000000, 0x80000000u) BATCH_BAD(max, 0xffffffffu)
TEST(ep_batch_set_zero_resets_to_default) {
    REQUIRE(U(GdiSetBatchLimit));
    DWORD orig = U(GdiGetBatchLimit)();
    U(GdiSetBatchLimit)(3);
    DWORD prev = U(GdiSetBatchLimit)(0);
    CHECKF(prev == 3, "prev %lu", prev);
    CHECKF(U(GdiGetBatchLimit)() == 20, "limit %lu", U(GdiGetBatchLimit)());
    U(GdiSetBatchLimit)(orig);
}
TEST(ep_batch_set_zero_when_default) {
    REQUIRE(U(GdiSetBatchLimit));
    DWORD orig = U(GdiGetBatchLimit)();
    U(GdiSetBatchLimit)(0);
    DWORD prev = U(GdiSetBatchLimit)(0);
    CHECKF(prev == 20, "prev %lu", prev);
    U(GdiSetBatchLimit)(orig);
}
TEST(ep_batch_chain_all_values) {
    REQUIRE(U(GdiSetBatchLimit));
    DWORD orig = U(GdiGetBatchLimit)();
    DWORD last = orig;
    for (DWORD v = 1; v <= 20; v++) {
        DWORD prev = U(GdiSetBatchLimit)(v);
        if (prev != last) { CHECKF(0, "v=%lu prev %lu want %lu", v, prev, last); break; }
        last = v;
    }
    U(GdiSetBatchLimit)(orig);
}
TEST(ep_batch_invalid_does_not_disturb_previous_chain) {
    REQUIRE(U(GdiSetBatchLimit));
    DWORD orig = U(GdiGetBatchLimit)();
    U(GdiSetBatchLimit)(9);
    CHECK(U(GdiSetBatchLimit)(500) == 0);
    CHECK(U(GdiSetBatchLimit)(4) == 9);
    U(GdiSetBatchLimit)(orig);
}
TEST(ep_batch_drawing_still_works_at_limit_1) {
    REQUIRE(U(GdiSetBatchLimit)); REQUIRE(U(SetPixelV));
    DWORD orig = U(GdiGetBatchLimit)();
    HBITMAP b, o; void *bits;
    HDC m = ep_memdc32(8, 8, &b, &o, &bits);
    CHECK(m != NULL);
    if (m) {
        U(GdiSetBatchLimit)(1);
        for (int i = 0; i < 8; i++) U(SetPixelV)(m, i, i, RGB(255, 0, 0));
        CHECK(U(GdiFlush)());
        DWORD *px = (DWORD *)bits;
        for (int i = 0; i < 8; i++) CHECKF((px[i * 8 + i] & 0xFFFFFF) == 0xFF0000, "px %d = %08lx", i, px[i * 8 + i]);
        ep_memdc_free(m, b, o);
    }
    U(GdiSetBatchLimit)(orig);
}
TEST(ep_batch_drawing_flushed_by_gdiflush_default) {
    REQUIRE(U(SetPixelV)); REQUIRE(U(GdiFlush));
    HBITMAP b, o; void *bits;
    HDC m = ep_memdc32(4, 4, &b, &o, &bits);
    if (m) {
        U(SetPixelV)(m, 2, 1, RGB(1, 2, 3));
        U(GdiFlush)();
        CHECK((((DWORD *)bits)[1 * 4 + 2] & 0xFFFFFF) == 0x010203);
        ep_memdc_free(m, b, o);
    }
}
static DWORD g_batch_thread_seen;
static DWORD WINAPI batch_thread(LPVOID p) { g_batch_thread_seen = U(GdiGetBatchLimit)(); return 0; }
TEST(ep_batch_limit_visible_in_other_thread_note) {
    REQUIRE(U(GdiSetBatchLimit));
    DWORD orig = U(GdiGetBatchLimit)();
    U(GdiSetBatchLimit)(7);
    HANDLE h = CreateThread(NULL, 0, batch_thread, NULL, 0, NULL);
    WaitForSingleObject(h, 10000); CloseHandle(h);
    t_note("other thread sees batch limit %lu (set 7)", g_batch_thread_seen);
    CHECKF(g_batch_thread_seen == 7, "other thread sees %lu", g_batch_thread_seen); /* the limit lives in a gdi32full global, not in the TEB */
    U(GdiSetBatchLimit)(orig);
}
TEST(ep_gdiflush_returns_true) {
    REQUIRE(U(GdiFlush));
    CHECK(U(GdiFlush)());
}
TEST(ep_gdiflush_repeated) {
    REQUIRE(U(GdiFlush));
    for (int i = 0; i < 1000; i++) if (!U(GdiFlush)()) { CHECKF(0, "iter %d", i); break; }
}
TEST(ep_gdiflush_does_not_set_error) {
    REQUIRE(U(GdiFlush));
    SET_ERR(0x5a5a);
    U(GdiFlush)();
    DWORD e = GetLastError();
    if (e != 0x5a5a) t_note("GdiFlush changed last error to %lu", e);
}
static DWORD WINAPI flush_thread(LPVOID p) { *(BOOL *)p = U(GdiFlush)(); return 0; }
TEST(ep_gdiflush_other_thread) {
    REQUIRE(U(GdiFlush));
    BOOL r = FALSE;
    HANDLE h = CreateThread(NULL, 0, flush_thread, &r, 0, NULL);
    WaitForSingleObject(h, 10000); CloseHandle(h);
    CHECK(r);
}
TEST(ep_gdiflush_after_drawing_screen_dc_clip_nothing) {
    REQUIRE(U(GdiFlush));
    HDC d = ep_scr();
    U(GetPixel)(d, 0, 0);
    CHECK(U(GdiFlush)());
    ep_scr_free(d);
}
TEST(ep_gdi_exports_present) {
    CHECK(UG(GdiFlush) != NULL);
    CHECK(UG(GdiGetBatchLimit) != NULL);
    CHECK(UG(GdiSetBatchLimit) != NULL);
    CHECK(UG(CancelDC) != NULL);
    CHECK(UG(Escape) != NULL);
    CHECK(UG(ExtEscape) != NULL);
    CHECK(UG(DrawEscape) != NULL);
    CHECK(UG(StartDocA) != NULL && UG(StartDocW) != NULL);
    CHECK(UG(EndDoc) != NULL && UG(AbortDoc) != NULL);
    CHECK(UG(StartPage) != NULL && UG(EndPage) != NULL);
    CHECK(UG(SetAbortProc) != NULL);
    CHECK(UG(ResetDCA) != NULL && UG(ResetDCW) != NULL);
    CHECK(UG(CreateICA) != NULL && UG(CreateICW) != NULL);
}

/* ---- CancelDC ---- */
TEST(ep_canceldc_null) {
    REQUIRE(U(CancelDC));
    SET_ERR(0);
    BOOL r = U(CancelDC)(NULL);
    CHECKF(r == FALSE, "got %d", r);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
}
TEST(ep_canceldc_bogus) {
    REQUIRE(U(CancelDC));
    SET_ERR(0);
    BOOL r = U(CancelDC)(EP_BOGUS_DC);
    CHECKF(r == FALSE, "got %d", r);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
}
TEST(ep_canceldc_bogus2) {
    REQUIRE(U(CancelDC));
    SET_ERR(0);
    BOOL r = U(CancelDC)(EP_BOGUS_DC2);
    t_note("CancelDC(0xDEADBEEF) -> %d err %lu", r, GetLastError());
}
TEST(ep_canceldc_screen_note) {
    REQUIRE(U(CancelDC));
    HDC d = ep_scr();
    SET_ERR(0);
    BOOL r = U(CancelDC)(d);
    t_note("CancelDC(screen) -> %d err %lu", r, GetLastError());
    CHECK(r == TRUE || r == FALSE);
    CHECK(U(GetDeviceCaps)(d, TECHNOLOGY) == DT_RASDISPLAY);
    ep_scr_free(d);
}
TEST(ep_canceldc_mem_note) {
    REQUIRE(U(CancelDC));
    HDC m = U(CreateCompatibleDC)(NULL);
    SET_ERR(0);
    BOOL r = U(CancelDC)(m);
    t_note("CancelDC(mem) -> %d err %lu", r, GetLastError());
    COLORREF c = U(SetPixel)(m, 0, 0, 0);
    (void)c;
    CHECK(U(DeleteDC)(m));
}
TEST(ep_canceldc_does_not_break_drawing) {
    REQUIRE(U(CancelDC));
    HBITMAP b, o; void *bits;
    HDC m = ep_memdc32(4, 4, &b, &o, &bits);
    if (m) {
        U(CancelDC)(m);
        U(SetPixelV)(m, 1, 1, RGB(9, 8, 7));
        U(GdiFlush)();
        CHECK((((DWORD *)bits)[5] & 0xFFFFFF) == 0x090807);
        ep_memdc_free(m, b, o);
    }
}
TEST(ep_canceldc_deleted) {
    REQUIRE(U(CancelDC));
    HDC m = U(CreateCompatibleDC)(NULL); U(DeleteDC)(m);
    SET_ERR(0);
    BOOL r = U(CancelDC)(m);
    CHECKF(r == FALSE, "got %d", r);
}

/* ---- EUDC ---- */
typedef DWORD (WINAPI *GetEUDCTimeStamp_pfn)(void);
typedef DWORD (WINAPI *GetEUDCTimeStampExW_pfn)(LPCWSTR);
TEST(ep_eudc_timestamp_probe) {
    GetEUDCTimeStamp_pfn f = (GetEUDCTimeStamp_pfn)t_proc("GetEUDCTimeStamp");
    REQUIRE(f);
    DWORD a = f(), b = f();
    t_note("GetEUDCTimeStamp = %lu", a);
    CHECKF(a == b, "unstable %lu vs %lu", a, b);
}
TEST(ep_eudc_timestampex_null) {
    GetEUDCTimeStampExW_pfn f = (GetEUDCTimeStampExW_pfn)t_proc("GetEUDCTimeStampExW");
    REQUIRE(f);
    DWORD a = f(NULL), b = f(NULL);
    t_note("GetEUDCTimeStampExW(NULL) = %lu", a);
    CHECKF(a == b, "unstable %lu vs %lu", a, b);
}
TEST(ep_eudc_timestampex_facenames) {
    GetEUDCTimeStampExW_pfn f = (GetEUDCTimeStampExW_pfn)t_proc("GetEUDCTimeStampExW");
    REQUIRE(f);
    static const WCHAR *names[] = { L"", L"Arial", L"System", L"NoSuchFaceNameXYZ", L"\x65b0\x7d30\x660e\x9ad4" };
    for (unsigned i = 0; i < 5; i++) {
        DWORD a = f(names[i]), b = f(names[i]);
        t_note("GetEUDCTimeStampExW(\"%ls\") = %lu", names[i], a);
        CHECKF(a == b, "name %u unstable", i);
    }
}
TEST(ep_eudc_timestampex_long_name) {
    GetEUDCTimeStampExW_pfn f = (GetEUDCTimeStampExW_pfn)t_proc("GetEUDCTimeStampExW");
    REQUIRE(f);
    WCHAR buf[300]; for (int i = 0; i < 299; i++) buf[i] = L'A'; buf[299] = 0;
    DWORD a = f(buf);
    t_note("GetEUDCTimeStampExW(299 chars) = %lu", a);
    CHECK(a == f(buf));
}
TEST(ep_eudc_timestamp_matches_ex_null_note) {
    GetEUDCTimeStamp_pfn f = (GetEUDCTimeStamp_pfn)t_proc("GetEUDCTimeStamp");
    GetEUDCTimeStampExW_pfn g = (GetEUDCTimeStampExW_pfn)t_proc("GetEUDCTimeStampExW");
    REQUIRE(f); REQUIRE(g);
    DWORD a = f(), b = g(NULL);
    if (a != b) t_note("GetEUDCTimeStamp %lu != Ex(NULL) %lu", a, b);
}
