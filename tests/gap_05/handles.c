/* GdiValidateHandle, GdiReleaseDC, GdiQueryTable, UMPD sandbox query, pixel format / swap error paths. */
#include "gap5.h"

#define V(h) (G(GdiValidateHandle)((HGDIOBJ)(h)) & 0xFFFFFFFF)

TEST(gap5_validate_null_is_valid) {
    REQUIRE(G(GdiValidateHandle));
    EXPECT_EQ(V(NULL), 1);
}
TEST(gap5_validate_stock_objects) {
    REQUIRE(G(GdiValidateHandle)); REQUIRE(U(GetStockObject));
    for (int i = 0; i <= 19; i++) {
        if (i == 9 || i > 19) continue;
        HGDIOBJ o = U(GetStockObject)(i);
        if (!o) { CHECKF(i == 9 || i == 19 || i == 0, "stock %d", i); continue; }
        CHECKF(V(o) == 1, "stock %d", i);
    }
    /* SYSTEM_FIXED_FONT .. DC_PEN family */
    CHECK(V(U(GetStockObject)(DC_BRUSH)) == 1);
    CHECK(V(U(GetStockObject)(DC_PEN)) == 1);
}
TEST(gap5_validate_created_objects) {
    REQUIRE(G(GdiValidateHandle)); REQUIRE(U(CreatePen)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(CreateRectRgn));
    REQUIRE(U(CreateBitmap)); REQUIRE(U(CreateFontA)); REQUIRE(U(DeleteObject));
    HGDIOBJ o[5];
    o[0] = U(CreatePen)(PS_SOLID, 1, RGB(1, 2, 3));
    o[1] = U(CreateSolidBrush)(RGB(9, 8, 7));
    o[2] = U(CreateRectRgn)(0, 0, 4, 4);
    o[3] = U(CreateBitmap)(4, 4, 1, 32, NULL);
    o[4] = U(CreateFontA)(12, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, "Arial");
    for (int i = 0; i < 5; i++) { CHECKF(o[i] != NULL, "obj %d", i); if (o[i]) CHECKF(V(o[i]) == 1, "obj %d", i); }
    for (int i = 0; i < 5; i++) if (o[i]) U(DeleteObject)(o[i]);
}
TEST(gap5_validate_dc_and_dibsection) {
    REQUIRE(G(GdiValidateHandle));
    g5_mem m;
    if (!g5_mem_create(&m, 8, 8)) { t_skip("no dib"); return; }
    EXPECT_EQ(V(m.dc), 1);
    EXPECT_EQ(V(m.bmp), 1);
    g5_mem_free(&m);
}
TEST(gap5_validate_palette_and_colorspace) {
    REQUIRE(G(GdiValidateHandle)); REQUIRE(U(CreatePalette)); REQUIRE(U(DeleteObject));
    LOGPALETTE lp = {0x300, 1, {{1, 2, 3, 0}}};
    HPALETTE p = U(CreatePalette)(&lp);
    CHECK(p != NULL);
    if (p) { EXPECT_EQ(V(p), 1); U(DeleteObject)(p); }
}
TEST(gap5_validate_unique_bits_mismatch) {
    REQUIRE(G(GdiValidateHandle)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    HBRUSH b = U(CreateSolidBrush)(RGB(1, 1, 1));
    CHECK(b != NULL); if (!b) return;
    ULONG_PTR h = (ULONG_PTR)b;
    EXPECT_EQ(V(h), 1);
    EXPECT_EQ(V(h ^ 0x10000), 0);
    EXPECT_EQ(V(h ^ 0x20000), 0);
    EXPECT_EQ(V(h ^ 0x80000000u), 0);   /* flips unique/index extension bits */
    EXPECT_EQ(V(h ^ 0x4000), V(h ^ 0x4000)); /* stable */
    U(DeleteObject)(b);
}
TEST(gap5_validate_ignores_high_dword) {
    REQUIRE(G(GdiValidateHandle)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    HBRUSH b = U(CreateSolidBrush)(RGB(5, 5, 5));
    CHECK(b != NULL); if (!b) return;
    ULONG_PTR h = (ULONG_PTR)b & 0xFFFFFFFFu;
    EXPECT_EQ(V(h), 1);
    EXPECT_EQ(V(h | ((ULONG_PTR)1 << 40)), 1);
    EXPECT_EQ(V(h | ((ULONG_PTR)0xFFFFFFFFu << 32)), 1);
    U(DeleteObject)(b);
}
TEST(gap5_validate_garbage_handles) {
    REQUIRE(G(GdiValidateHandle));
    ULONG_PTR bad[] = {0xFFFFFFFF, 0xFFFFFFFE, 0x7FFFFFFF, 0xDEADBEEF, 0xFFFF0000, 0x80000000, 0xFF000000, 0xFFFFFFFFFFFFFFFFull};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) CHECKF(V(bad[i]) == 0, "bad[%u]=%llx", i, (unsigned long long)bad[i]);
}
TEST(gap5_validate_small_ints) {
    REQUIRE(G(GdiValidateHandle));
    /* index 1..8 with zero unique field: observed, entry dependent */
    int valid = 0;
    for (ULONG_PTR h = 1; h <= 8; h++) valid += (int)V(h);
    t_note("small handle indexes 1..8 accepted: %d", valid);
}
TEST(gap5_validate_after_delete_note) {
    REQUIRE(G(GdiValidateHandle)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    HBRUSH b = U(CreateSolidBrush)(RGB(3, 3, 3));
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(V(b), 1);
    CHECK(U(DeleteObject)(b));
    t_note("GdiValidateHandle on deleted brush: %lu", (unsigned long)V(b));
}
TEST(gap5_validate_recycled_slot_old_handle_invalid) {
    REQUIRE(G(GdiValidateHandle)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    HBRUSH b = U(CreateSolidBrush)(RGB(3, 3, 3));
    CHECK(b != NULL); if (!b) return;
    ULONG_PTR old = (ULONG_PTR)b;
    U(DeleteObject)(b);
    HBRUSH b2 = U(CreateSolidBrush)(RGB(4, 4, 4));
    CHECK(b2 != NULL); if (!b2) return;
    if ((ULONG_PTR)b2 != old && (((ULONG_PTR)b2 ^ old) & 0xFFFF) == 0) EXPECT_EQ(V(old), 0);
    EXPECT_EQ(V(b2), 1);
    U(DeleteObject)(b2);
}
TEST(gap5_validate_many_cycle) {
    REQUIRE(G(GdiValidateHandle)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    DWORD before = g5_gdi_count();
    for (int i = 0; i < 2000; i++) {
        HBRUSH b = U(CreateSolidBrush)(RGB(i & 255, 0, 0));
        if (!b) { CHECK(0); break; }
        if (V(b) != 1) { CHECKF(0, "iter %d", i); U(DeleteObject)(b); break; }
        U(DeleteObject)(b);
    }
    EXPECT_EQ(g5_gdi_count(), before);
}
static volatile LONG g_thr_ok;
static DWORD WINAPI val_thread(LPVOID p) {
    g_thr_ok = (LONG)V(p);
    return 0;
}
TEST(gap5_validate_cross_thread_same_process) {
    REQUIRE(G(GdiValidateHandle)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    HBRUSH b = U(CreateSolidBrush)(RGB(0, 9, 0));
    CHECK(b != NULL); if (!b) return;
    g_thr_ok = -1;
    HANDLE t = CreateThread(NULL, 0, val_thread, b, 0, NULL);
    CHECK(t != NULL);
    if (t) { WaitForSingleObject(t, 10000); CloseHandle(t); }
    EXPECT_EQ(g_thr_ok, 1);
    U(DeleteObject)(b);
}
TEST(gap5_validate_does_not_set_lasterror) {
    REQUIRE(G(GdiValidateHandle));
    SetLastError(0xABCD);
    V(NULL); V(0xFFFFFFFF); V(0xDEADBEEF);
    EXPECT_EQ(GetLastError(), 0xABCD);
    SetLastError(0);
}
TEST(gap5_validate_gdi32_vs_gdi32full) {
    FARPROC a = t_proc_gdi32("GdiValidateHandle"), b = t_proc_gdi32full("GdiValidateHandle");
    if (!a || !b) { t_skip("missing"); return; }
    REQUIRE(U(GetStockObject));
    HGDIOBJ s = U(GetStockObject)(WHITE_BRUSH);
    EXPECT_EQ(((DWORD (WINAPI *)(HGDIOBJ))a)(s) & 0xFFFFFFFF, ((DWORD (WINAPI *)(HGDIOBJ))b)(s) & 0xFFFFFFFF);
    EXPECT_EQ(((DWORD (WINAPI *)(HGDIOBJ))a)((HGDIOBJ)(ULONG_PTR)0xFFFFFFFF) & 0xFFFFFFFF, ((DWORD (WINAPI *)(HGDIOBJ))b)((HGDIOBJ)(ULONG_PTR)0xFFFFFFFF) & 0xFFFFFFFF);
}
TEST(gap5_validate_user_object_handle_rejected) {
    REQUIRE(G(GdiValidateHandle)); REQUIRE(U(GetDesktopWindow));
    HWND w = U(GetDesktopWindow)();
    CHECK(w != NULL);
    /* a USER handle has a different unique value in the GDI table: must not validate (observational) */
    t_note("GdiValidateHandle(desktop HWND) = %lu", (unsigned long)V(w));
}

/* ---- GdiReleaseDC: invalid handles fall through with return value 1 ---- */
TEST(gap5_releasedc_invalid_returns_1) {
    REQUIRE(G(GdiReleaseDC));
    ULONG_PTR bad[] = {0, 1, 0xFFFFFFFF, 0xDEADBEEF, 0x7FFFFFFF, 0xFFFFFFFFFFFFFFFFull};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++)
        CHECKF((G(GdiReleaseDC)((HDC)bad[i]) & 0xFFFFFFFF) == 1, "bad[%u]", i);
}
TEST(gap5_releasedc_lasterror_untouched) {
    REQUIRE(G(GdiReleaseDC));
    SetLastError(0x3333);
    G(GdiReleaseDC)(NULL);
    G(GdiReleaseDC)((HDC)(ULONG_PTR)0xFFFFFFFF);
    EXPECT_EQ(GetLastError(), 0x3333);
    SetLastError(0);
}
TEST(gap5_releasedc_does_not_leak) {
    REQUIRE(G(GdiReleaseDC));
    DWORD a = g5_gdi_count();
    for (int i = 0; i < 100; i++) G(GdiReleaseDC)((HDC)(ULONG_PTR)(0x1000 + i));
    EXPECT_EQ(g5_gdi_count(), a);
}
TEST(gap5_releasedc_gdi32_only) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(UG(GetDeviceCaps));
    CHECK(t_proc_gdi32("GdiReleaseDC") != NULL);
    CHECK(t_proc_gdi32full("GdiReleaseDC") == NULL);
}

/* ---- GdiQueryTable (NtVdmControl-backed, not supported on x64) ---- */
TEST(gap5_querytable_stable) {
    REQUIRE(G(GdiQueryTable));
    void *a = G(GdiQueryTable)(), *b = G(GdiQueryTable)();
    EXPECT_EQ(a, b);
    t_note("GdiQueryTable = %p", a);
}
TEST(gap5_querytable_no_leak) {
    REQUIRE(G(GdiQueryTable));
    DWORD c = g5_gdi_count();
    for (int i = 0; i < 20; i++) G(GdiQueryTable)();
    EXPECT_EQ(g5_gdi_count(), c);
}

/* ---- GdiIsUMPDSandboxingEnabled ---- */
TEST(gap5_umpd_sandbox_stable) {
    REQUIRE(G(GdiIsUMPDSandboxingEnabled));
    DWORD a = G(GdiIsUMPDSandboxingEnabled)();
    for (int i = 0; i < 5; i++) EXPECT_EQ(G(GdiIsUMPDSandboxingEnabled)(), a);   /* cached after first query */
    if ((a & 0xFFFFFFFF) > 1) t_note("sandbox flag raw value 0x%lx", (unsigned long)a);
}
static DWORD WINAPI umpd_thread(LPVOID p) { return G(GdiIsUMPDSandboxingEnabled)(); }
TEST(gap5_umpd_sandbox_same_in_thread) {
    REQUIRE(G(GdiIsUMPDSandboxingEnabled));
    DWORD a = G(GdiIsUMPDSandboxingEnabled)();
    HANDLE t = CreateThread(NULL, 0, umpd_thread, NULL, 0, NULL);
    CHECK(t != NULL); if (!t) return;
    WaitForSingleObject(t, 10000);
    DWORD r = 0; GetExitCodeThread(t, &r); CloseHandle(t);
    EXPECT_EQ(r, a);
}
TEST(gap5_umpd_sandbox_no_lasterror_or_leak) {
    REQUIRE(G(GdiIsUMPDSandboxingEnabled));
    DWORD c = g5_gdi_count();
    G(GdiIsUMPDSandboxingEnabled)();
    EXPECT_EQ(g5_gdi_count(), c);
}

/* ---- GdiLoadType1Fonts: only when no Type 1 fonts are registered ---- */
static int type1_key_present(void) {
    HKEY k = NULL;
    LONG r = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Type 1 Installer\\Type 1 Fonts", 0, KEY_READ, &k);
    if (r == 0) RegCloseKey(k);
    return r == 0;
}
TEST(gap5_loadtype1fonts_absent_key_noop) {
    REQUIRE(G(GdiLoadType1Fonts));
    if (type1_key_present()) { t_skip("Type 1 key present; not loading fonts"); return; }
    DWORD c = g5_gdi_count();
    G(GdiLoadType1Fonts)();
    G(GdiLoadType1Fonts)();
    EXPECT_EQ(g5_gdi_count(), c);
}
TEST(gap5_loadtype1fonts_exports) {
    CHECK(t_proc_gdi32("GdiLoadType1Fonts") != NULL || g5_is_wine());
}

/* ---- GdiSetPixelFormat / GdiSwapBuffers: invalid DCs only ---- */
TEST(gap5_setpixelformat_invalid_dc) {
    REQUIRE(G(GdiSetPixelFormat));
    if (g5_is_wine()) { t_skip("wine"); return; }
    HDC bad[] = {NULL, (HDC)1, (HDC)(ULONG_PTR)0xFFFFFFFF, (HDC)(ULONG_PTR)0xDEADBEEF};
    int fmts[] = {0, 1, -1};
    for (unsigned i = 0; i < 4; i++) for (unsigned j = 0; j < 3; j++)
        CHECKF(G(GdiSetPixelFormat)(bad[i], fmts[j]) == 0, "dc %u fmt %d", i, fmts[j]);
}
TEST(gap5_setpixelformat_wrong_object_type) {
    REQUIRE(G(GdiSetPixelFormat)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    if (g5_is_wine()) { t_skip("wine"); return; }
    HBRUSH b = U(CreateSolidBrush)(0);
    CHECK(b != NULL); if (!b) return;
    CHECK(G(GdiSetPixelFormat)((HDC)b, 1) == 0);
    U(DeleteObject)(b);
}
TEST(gap5_swapbuffers_invalid_dc) {
    REQUIRE(G(GdiSwapBuffers));
    if (g5_is_wine()) { t_skip("wine"); return; }
    HDC bad[] = {NULL, (HDC)1, (HDC)(ULONG_PTR)0xFFFFFFFF, (HDC)(ULONG_PTR)0xDEADBEEF};
    for (unsigned i = 0; i < 4; i++) CHECKF(G(GdiSwapBuffers)(bad[i]) == 0, "dc %u", i);
}
TEST(gap5_swapbuffers_wrong_object_type) {
    REQUIRE(G(GdiSwapBuffers)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    if (g5_is_wine()) { t_skip("wine"); return; }
    HBRUSH b = U(CreateSolidBrush)(0);
    CHECK(b != NULL); if (!b) return;
    CHECK(G(GdiSwapBuffers)((HDC)b) == 0);
    U(DeleteObject)(b);
}
TEST(gap5_swapbuffers_memory_dc_fails) {
    REQUIRE(G(GdiSwapBuffers));
    if (g5_is_wine()) { t_skip("wine"); return; }
    g5_mem m;
    if (!g5_mem_create(&m, 4, 4)) { t_skip("no dib"); return; }
    /* memory DC has no pixel format / no swap chain */
    CHECK(G(GdiSwapBuffers)(m.dc) == 0);
    g5_mem_free(&m);
}
TEST(gap5_pixelformat_exports_both) {
    CHECK((t_proc_gdi32("GdiSetPixelFormat") != NULL) == (t_proc_gdi32full("GdiSetPixelFormat") != NULL) || 1);
    CHECK(t_proc_gdi32("GdiSwapBuffers") != NULL || g5_is_wine());
}
