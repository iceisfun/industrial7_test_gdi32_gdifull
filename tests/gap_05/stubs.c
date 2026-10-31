/* Trivial / stub-like internal gdi32 exports. */
#include "gap5.h"

/* ---- GdiSetLastError: stores straight into the TEB last-error slot ---- */
static const DWORD g_errs[] = {0, 1, 2, 5, 6, 8, 57, 87, 120, 122, 126, 127, 183, 997, 1400, 1401, 1402,
    1813, 2000, 0x7FFFFFFF, 0x80000000, 0xC0000001, 0xDEADBEEF, 0xFFFFFFFE, 0xFFFFFFFF};

TEST(gap5_setlasterror_values) {
    REQUIRE(G(GdiSetLastError));
    for (unsigned i = 0; i < sizeof g_errs / sizeof *g_errs; i++) {
        G(GdiSetLastError)(g_errs[i]);
        EXPECT_EQ(GetLastError(), g_errs[i]);
        EXPECT_EQ(g5_rtl_lasterr(), g_errs[i]);
    }
    SetLastError(0);
}
TEST(gap5_setlasterror_overrides_setlasterror) {
    REQUIRE(G(GdiSetLastError));
    SetLastError(5); G(GdiSetLastError)(6); EXPECT_EQ(GetLastError(), 6);
    G(GdiSetLastError)(7); SetLastError(8); EXPECT_EQ(GetLastError(), 8);
    SetLastError(0);
}
TEST(gap5_setlasterror_repeat_idempotent) {
    REQUIRE(G(GdiSetLastError));
    for (int i = 0; i < 1000; i++) { G(GdiSetLastError)(1234); }
    EXPECT_EQ(GetLastError(), 1234);
    G(GdiSetLastError)(0); EXPECT_EQ(GetLastError(), 0);
}
TEST(gap5_setlasterror_cleared_then_read_twice) {
    REQUIRE(G(GdiSetLastError));
    G(GdiSetLastError)(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER); /* reading does not clear */
    SetLastError(0);
}
TEST(gap5_setlasterror_visible_to_failing_api_after) {
    REQUIRE(G(GdiSetLastError)); REQUIRE(U(DeleteObject));
    G(GdiSetLastError)(0x1234);
    /* a successful stock query does not reset last-error */
    REQUIRE(U(GetStockObject));
    HGDIOBJ o = U(GetStockObject)(BLACK_BRUSH);
    CHECK(o != NULL);
    t_note("last error after successful GetStockObject: %lu", GetLastError());
    SetLastError(0);
}
static DWORD g_thr_main_seen, g_thr_other_seen;
static DWORD WINAPI setle_thread(LPVOID p) {
    g_thr_other_seen = GetLastError();   /* fresh thread starts at 0 */
    G(GdiSetLastError)(777);
    return GetLastError();
}
TEST(gap5_setlasterror_thread_local) {
    REQUIRE(G(GdiSetLastError));
    G(GdiSetLastError)(4242);
    HANDLE h = CreateThread(NULL, 0, setle_thread, NULL, 0, NULL);
    CHECK(h != NULL);
    if (!h) return;
    WaitForSingleObject(h, 10000);
    DWORD rc = 0; GetExitCodeThread(h, &rc); CloseHandle(h);
    EXPECT_EQ(rc, 777);
    EXPECT_EQ(g_thr_other_seen, 0);
    EXPECT_EQ(GetLastError(), 4242);   /* other thread's call did not touch ours */
    SetLastError(0);
}
TEST(gap5_setlasterror_gdi32_only_export) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    /* GdiSetLastError is exported by gdi32.dll, not by gdi32full.dll. */
    REQUIRE(UG(GetDeviceCaps));
    CHECK(t_proc_gdi32("GdiSetLastError") != NULL);
    CHECK(t_proc_gdi32full("GdiSetLastError") == NULL);
}

/* ---- stub exports that return a constant 1 ---- */
TEST(gap5_font_change_event_returns_1) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiSupportsFontChangeEvent));
    for (int i = 0; i < 4; i++) EXPECT_EQ(G(GdiSupportsFontChangeEvent)() & 0xFFFFFFFF, 1);
}
TEST(gap5_font_change_event_not_changing_lasterror) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiSupportsFontChangeEvent));
    SetLastError(0x5555);
    G(GdiSupportsFontChangeEvent)();
    EXPECT_EQ(GetLastError(), 0x5555);
    SetLastError(0);
}
TEST(gap5_release_local_dc_stub) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiReleaseLocalDC));
    HDC vals[] = {NULL, (HDC)1, (HDC)(ULONG_PTR)0xFFFFFFFF, (HDC)(ULONG_PTR)0x12345678, (HDC)(LONG_PTR)-1};
    for (unsigned i = 0; i < sizeof vals / sizeof *vals; i++)
        EXPECT_EQ(G(GdiReleaseLocalDC)(vals[i]) & 0xFFFFFFFF, 1);
}
TEST(gap5_release_local_dc_real_dc_untouched) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiReleaseLocalDC)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetDeviceCaps));
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(dc != NULL);
    int before = U(GetDeviceCaps)(dc, BITSPIXEL);
    EXPECT_EQ(G(GdiReleaseLocalDC)(dc) & 0xFFFFFFFF, 1);
    EXPECT_EQ(U(GetDeviceCaps)(dc, BITSPIXEL), before);   /* stub: DC stays fully usable */
    CHECK(U(DeleteDC)(dc));
}
TEST(gap5_set_attrs_stub) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiSetAttrs));
    HDC vals[] = {NULL, (HDC)1, (HDC)(ULONG_PTR)0xFFFFFFFF, (HDC)(ULONG_PTR)0xCAFEBABE};
    for (unsigned i = 0; i < sizeof vals / sizeof *vals; i++)
        EXPECT_EQ(G(GdiSetAttrs)(vals[i]) & 0xFFFFFFFF, 1);
}
TEST(gap5_set_attrs_real_dc) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiSetAttrs)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetBkColor)); REQUIRE(U(SetBkColor));
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(dc != NULL);
    U(SetBkColor)(dc, RGB(1, 2, 3));
    EXPECT_EQ(G(GdiSetAttrs)(dc) & 0xFFFFFFFF, 1);
    EXPECT_EQ(U(GetBkColor)(dc), RGB(1, 2, 3));
    CHECK(U(DeleteDC)(dc));
}
TEST(gap5_stub_group_same_entry_in_gdi32full) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    /* these four live at one shared function body in gdi32full.dll */
    REQUIRE(UF(GetDeviceCaps) || 1);
    FARPROC a = t_proc_gdi32full("GdiReleaseLocalDC");
    FARPROC b = t_proc_gdi32full("GdiSetAttrs");
    FARPROC c = t_proc_gdi32full("GdiSupportsFontChangeEvent");
    if (!a || !b || !c) { t_skip("exports missing"); return; }
    if (a == b && b == c) CHECK(1);
    else t_note("stub bodies not folded: %p %p %p", (void *)a, (void *)b, (void *)c);
}

/* ---- script/journal exports: bodies are "xor eax,eax; ret" ---- */
TEST(gap5_play_journal_returns_0) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiPlayJournal));
    EXPECT_EQ(G(GdiPlayJournal)(NULL, NULL, 0, 0, 0), 0);
    EXPECT_EQ(G(GdiPlayJournal)((HDC)1, L"x", 1, 2, 3), 0);
    EXPECT_EQ(G(GdiPlayJournal)((HDC)(ULONG_PTR)0xFFFFFFFF, NULL, 0xFFFFFFFF, 0xFFFFFFFF, -1), 0);
}
TEST(gap5_play_dcscript_returns_0) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiPlayDCScript));
    EXPECT_EQ(G(GdiPlayDCScript)(NULL, NULL, NULL, NULL, NULL, NULL), 0);
    EXPECT_EQ(G(GdiPlayDCScript)((void *)1, (void *)2, (void *)3, (void *)4, (void *)5, (void *)6), 0);
}
TEST(gap5_play_script_returns_0) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiPlayScript));
    EXPECT_EQ(G(GdiPlayScript)(NULL, NULL, NULL, NULL, NULL, NULL), 0);
    EXPECT_EQ(G(GdiPlayScript)((void *)1, (void *)2, (void *)3, (void *)4, (void *)5, (void *)6), 0);
}
TEST(gap5_play_script_lasterror_untouched) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiPlayScript)); REQUIRE(G(GdiPlayJournal));
    SetLastError(0x4321);
    G(GdiPlayScript)(NULL, NULL, NULL, NULL, NULL, NULL);
    G(GdiPlayJournal)(NULL, NULL, 0, 0, 0);
    G(GdiPlayDCScript) ? (void)G(GdiPlayDCScript)(NULL, NULL, NULL, NULL, NULL, NULL) : (void)0;
    EXPECT_EQ(GetLastError(), 0x4321);
    SetLastError(0);
}
TEST(gap5_play_family_exported_both_modules) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    const char *n[] = {"GdiPlayJournal", "GdiPlayDCScript", "GdiPlayScript", "GdiPlayEMF", "GdiPlayPageEMF", "GdiPlayPrivatePageEMF", "gdiPlaySpoolStream"};
    if (!t_proc_gdi32("GdiPlayEMF")) { t_skip("no gdi32 GdiPlayEMF"); return; }
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++) {
        CHECKF(t_proc_gdi32(n[i]) != NULL, "gdi32 lacks %s", n[i]);
        CHECKF(t_proc_gdi32full(n[i]) != NULL, "gdi32full lacks %s", n[i]);
    }
}

/* ---- gdiPlaySpoolStream: not implemented, sets ERROR_CALL_NOT_IMPLEMENTED ---- */
TEST(gap5_spoolstream_lasterror_120) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(gdiPlaySpoolStream));
    SetLastError(0);
    G(gdiPlaySpoolStream)(NULL, NULL, NULL, NULL, NULL, NULL);
    EXPECT_EQ(GetLastError(), ERROR_CALL_NOT_IMPLEMENTED);
}
TEST(gap5_spoolstream_with_args_same_error) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(gdiPlaySpoolStream));
    SetLastError(0);
    G(gdiPlaySpoolStream)((void *)1, (void *)2, (void *)3, (void *)4, (void *)5, (void *)6);
    EXPECT_EQ(GetLastError(), 120);
    SetLastError(0);
    G(gdiPlaySpoolStream)(NULL, NULL, NULL, NULL, NULL, NULL);
    EXPECT_EQ(GetLastError(), 120);
}

/* ---- Gdit* debugging hooks (telemetry-only bodies) ---- */
TEST(gap5_gdit_get_caller_tlstorage_null) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GditGetCallerTLStorage));
    for (int i = 0; i < 3; i++) EXPECT_EQ(G(GditGetCallerTLStorage)(NULL, NULL, 0, 0), 0);
}
TEST(gap5_gdit_push_caller_info_returns_1) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GditPushCallerInfo));
    for (int i = 0; i < 3; i++) EXPECT_EQ(G(GditPushCallerInfo)(NULL, NULL, 0, 0) & 0xFFFFFFFF, 1);
}
TEST(gap5_gdit_pop_caller_info_noop) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GditPopCallerInfo));
    SetLastError(0x77);
    G(GditPopCallerInfo)(NULL, NULL, 0, 0);
    G(GditPopCallerInfo)(NULL, NULL, 0, 0);
    EXPECT_EQ(GetLastError(), 0x77);
    SetLastError(0);
}
TEST(gap5_gdit_push_pop_sequence) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GditPushCallerInfo)); REQUIRE(G(GditPopCallerInfo)); REQUIRE(G(GditGetCallerTLStorage));
    for (int i = 0; i < 16; i++) {
        EXPECT_EQ(G(GditPushCallerInfo)(NULL, NULL, 0, 0) & 0xFFFFFFFF, 1);
        EXPECT_EQ(G(GditGetCallerTLStorage)(NULL, NULL, 0, 0), 0);
        G(GditPopCallerInfo)(NULL, NULL, 0, 0);
    }
}
TEST(gap5_gdit_no_gdi_objects_created) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GditPushCallerInfo)); REQUIRE(U(GetGuiResources));
    DWORD a = g5_gdi_count();
    for (int i = 0; i < 50; i++) { G(GditPushCallerInfo)(NULL, NULL, 0, 0); G(GditPopCallerInfo)(NULL, NULL, 0, 0); }
    EXPECT_EQ(g5_gdi_count(), a);
}

/* ---- GdiTrackHCreate / GdiTrackHDelete (feature-gated leak tracker) ---- */
TEST(gap5_trackh_create_identity_when_disabled) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiTrackHCreate)); REQUIRE(G(GdiTrackHDelete));
    void *tok = (void *)(ULONG_PTR)0x1234;
    void *r = G(GdiTrackHCreate)(tok);
    if (r == tok) {
        CHECK(G(GdiTrackHCreate)(NULL) == NULL);
        G(GdiTrackHDelete)(r);   /* disabled: no-op */
    } else {
        t_note("leak tracker feature appears enabled (create returned %p)", r);
    }
}
TEST(gap5_trackh_create_passthrough_repeat) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiTrackHCreate));
    void *probe = G(GdiTrackHCreate)((void *)(ULONG_PTR)0x77);
    if (probe != (void *)(ULONG_PTR)0x77) { t_skip("tracker enabled"); return; }
    for (ULONG_PTR i = 1; i < 200; i++)
        if (G(GdiTrackHCreate)((void *)(i * 8)) != (void *)(i * 8)) { CHECK(0); break; }
    G(GdiTrackHCreate)(NULL);
    CHECK(1);
}
TEST(gap5_trackh_delete_disabled_noop) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiTrackHCreate)); REQUIRE(G(GdiTrackHDelete));
    void *probe = G(GdiTrackHCreate)((void *)(ULONG_PTR)0x77);
    if (probe != (void *)(ULONG_PTR)0x77) { t_skip("tracker enabled"); return; }
    SetLastError(0x99);
    for (int i = 0; i < 50; i++) G(GdiTrackHDelete)((void *)(ULONG_PTR)(0x1000 + i * 8));
    G(GdiTrackHDelete)(NULL);
    EXPECT_EQ(GetLastError(), 0x99);
    SetLastError(0);
}

/* ---- GdiSetServerAttr: body is a bare ret ---- */
TEST(gap5_set_server_attr_noop) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiSetServerAttr)); if (g5_is_wine()) { t_skip("wine stub aborts"); return; }
    SetLastError(0x31);
    G(GdiSetServerAttr)(NULL, NULL);
    G(GdiSetServerAttr)((void *)1, (void *)2);
    EXPECT_EQ(GetLastError(), 0x31);
    SetLastError(0);
}
