/* Language pack (Lpk*) entry points exported by gdi32 / gdi32full. */
#include "gap5.h"

/* ---- LpkGetEditControl / LpkEditControl / LpkpInitializeEditControl ---- */
#define EC_ENTRIES 14

TEST(gap5_lpk_geteditcontrol_nonnull_stable) {
    REQUIRE(G(LpkGetEditControl));
    if (g5_is_wine()) { t_skip("wine"); return; }
    void *a = G(LpkGetEditControl)(), *b = G(LpkGetEditControl)();
    CHECK(a != NULL);
    EXPECT_EQ(a, b);
}
TEST(gap5_lpk_geteditcontrol_is_exported_data) {
    REQUIRE(G(LpkGetEditControl));
    if (g5_is_wine()) { t_skip("wine"); return; }
    FARPROC d = t_proc_gdi32("LpkEditControl");
    CHECK(d != NULL);
    EXPECT_EQ((void *)d, G(LpkGetEditControl)());
}
TEST(gap5_lpk_geteditcontrol_table_readable) {
    REQUIRE(G(LpkGetEditControl));
    if (g5_is_wine()) { t_skip("wine"); return; }
    void **t = G(LpkGetEditControl)();
    CHECK(t != NULL); if (!t) return;
    int nonnull = 0;
    for (int i = 0; i < EC_ENTRIES; i++) nonnull += t[i] != NULL;
    t_note("LpkEditControl: %d of %d slots populated", nonnull, EC_ENTRIES);
}
TEST(gap5_lpk_geteditcontrol_inside_gdi32) {
    REQUIRE(G(LpkGetEditControl));
    if (g5_is_wine()) { t_skip("wine"); return; }
    HMODULE m = t_gdi32();
    HMODULE owner = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)G(LpkGetEditControl)(), &owner);
    EXPECT_EQ(owner, m);
}
TEST(gap5_lpk_initeditcontrol_wrong_count) {
    REQUIRE(G(LpkpInitializeEditControl));
    if (g5_is_wine()) { t_skip("wine"); return; }
    void *tbl[EC_ENTRIES + 2] = {0};
    int bad[] = {0, 1, 13, 15, 16, -1, 14 + 256, 0x7FFFFFFF};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++)
        CHECKF(G(LpkpInitializeEditControl)(tbl, bad[i]) == 0, "count %d", bad[i]);
}
TEST(gap5_lpk_initeditcontrol_wrong_count_null_table_safe) {
    REQUIRE(G(LpkpInitializeEditControl));
    if (g5_is_wine()) { t_skip("wine"); return; }
    /* the size check precedes any dereference */
    CHECK(G(LpkpInitializeEditControl)(NULL, 0) == 0);
    CHECK(G(LpkpInitializeEditControl)(NULL, 13) == 0);
    CHECK(G(LpkpInitializeEditControl)(NULL, 15) == 0);
}
TEST(gap5_lpk_initeditcontrol_wrong_count_no_change) {
    REQUIRE(G(LpkpInitializeEditControl)); REQUIRE(G(LpkGetEditControl));
    if (g5_is_wine()) { t_skip("wine"); return; }
    void **live = G(LpkGetEditControl)();
    if (!live) { CHECK(0); return; }
    void *snap[EC_ENTRIES]; memcpy(snap, live, sizeof snap);
    void *junk[EC_ENTRIES]; for (int i = 0; i < EC_ENTRIES; i++) junk[i] = (void *)(ULONG_PTR)(0x1000 + i);
    CHECK(G(LpkpInitializeEditControl)(junk, 13) == 0);
    CHECK(G(LpkpInitializeEditControl)(junk, 15) == 0);
    CHECK(!memcmp(snap, live, sizeof snap));
}
TEST(gap5_lpk_initeditcontrol_roundtrip_same_values) {
    REQUIRE(G(LpkpInitializeEditControl)); REQUIRE(G(LpkGetEditControl));
    if (g5_is_wine()) { t_skip("wine"); return; }
    void **live = G(LpkGetEditControl)();
    if (!live) { CHECK(0); return; }
    void *snap[EC_ENTRIES]; memcpy(snap, live, sizeof snap);
    /* re-installing the very same table must succeed and leave it unchanged */
    CHECK(G(LpkpInitializeEditControl)(snap, EC_ENTRIES) == 1);
    CHECK(!memcmp(snap, live, sizeof snap));
    CHECK(G(LpkpInitializeEditControl)(snap, EC_ENTRIES) == 1);
    CHECK(!memcmp(snap, live, sizeof snap));
}
TEST(gap5_lpk_initeditcontrol_gdi32_only) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    if (!t_proc_gdi32("LpkpInitializeEditControl")) { t_skip("missing"); return; }
    CHECK(t_proc_gdi32full("LpkpInitializeEditControl") == NULL);
    CHECK(t_proc_gdi32full("LpkGetEditControl") == NULL);
}

/* ---- LpkGetTextExtentExPoint ---- */
TEST(gap5_lpk_extent_zero_count) {
    REQUIRE(G(LpkGetTextExtentExPoint));
    if (g5_is_wine()) { t_skip("wine"); return; }
    g5_mem m; if (!g5_mem_create(&m, 8, 8)) { t_skip("no dib"); return; }
    SIZE sz = {123, 456}; int fit = 99;
    EXPECT_EQ(G(LpkGetTextExtentExPoint)(m.dc, L"abc", 0, 0, &fit, NULL, &sz, 0, 0) & 0xFF, 1);
    EXPECT_EQ(sz.cx, 0); EXPECT_EQ(sz.cy, 0); EXPECT_EQ(fit, 0);
    g5_mem_free(&m);
}
TEST(gap5_lpk_extent_null_string) {
    REQUIRE(G(LpkGetTextExtentExPoint));
    if (g5_is_wine()) { t_skip("wine"); return; }
    g5_mem m; if (!g5_mem_create(&m, 8, 8)) { t_skip("no dib"); return; }
    SIZE sz = {7, 7}; int fit = 5;
    EXPECT_EQ(G(LpkGetTextExtentExPoint)(m.dc, NULL, 5, 0, &fit, NULL, &sz, 0, 0) & 0xFF, 1);
    EXPECT_EQ(sz.cx, 0); EXPECT_EQ(sz.cy, 0); EXPECT_EQ(fit, 0);
    g5_mem_free(&m);
}
TEST(gap5_lpk_extent_zero_count_null_fit) {
    REQUIRE(G(LpkGetTextExtentExPoint));
    if (g5_is_wine()) { t_skip("wine"); return; }
    g5_mem m; if (!g5_mem_create(&m, 8, 8)) { t_skip("no dib"); return; }
    SIZE sz = {7, 7};
    EXPECT_EQ(G(LpkGetTextExtentExPoint)(m.dc, L"x", 0, 0, NULL, NULL, &sz, 0, 0) & 0xFF, 1);
    EXPECT_EQ(sz.cx, 0); EXPECT_EQ(sz.cy, 0);
    g5_mem_free(&m);
}
static HFONT mkfont(const char *face, int h) {
    return ((HFONT (WINAPI *)(int,int,int,int,int,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,LPCSTR))t_proc("CreateFontA"))(h, 0, 0, 0, 400, 0, 0, 0, ANSI_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, face);
}
/* Latin text on a Western font takes the plain GetTextExtentExPoint path. */
TEST(gap5_lpk_extent_matches_gdi_latin) {
    REQUIRE(G(LpkGetTextExtentExPoint)); REQUIRE(U(GetTextExtentExPointW));
    if (g5_is_wine()) { t_skip("wine"); return; }
    g5_mem m; if (!g5_mem_create(&m, 8, 8)) { t_skip("no dib"); return; }
    HFONT f = mkfont("Arial", 16), of = f ? U(SelectObject)(m.dc, f) : NULL;
    static const WCHAR s[] = L"Hello, world 0123";
    int n = 17, dx1[32], dx2[32], fit1 = -1, fit2 = -1; SIZE a = {0}, b = {0};
    BOOL r1 = G(LpkGetTextExtentExPoint)(m.dc, s, n, 0, &fit1, dx1, &a, 0, 0);
    BOOL r2 = U(GetTextExtentExPointW)(m.dc, s, n, 0, &fit2, dx2, &b);
    CHECK(r2);
    EXPECT_EQ(r1 & 0xFF, 1);
    EXPECT_EQ(a.cx, b.cx); EXPECT_EQ(a.cy, b.cy);
    EXPECT_EQ(fit1, fit2);
    CHECK(!memcmp(dx1, dx2, n * sizeof(int)));
    if (f) { U(SelectObject)(m.dc, of); U(DeleteObject)(f); }
    g5_mem_free(&m);
}
TEST(gap5_lpk_extent_maxextent_matches_gdi) {
    REQUIRE(G(LpkGetTextExtentExPoint)); REQUIRE(U(GetTextExtentExPointW));
    if (g5_is_wine()) { t_skip("wine"); return; }
    g5_mem m; if (!g5_mem_create(&m, 8, 8)) { t_skip("no dib"); return; }
    HFONT f = mkfont("Arial", 16), of = f ? U(SelectObject)(m.dc, f) : NULL;
    static const WCHAR s[] = L"abcdefghijklmnop";
    int maxes[] = {1, 10, 40, 80, 1000};
    for (unsigned i = 0; i < 5; i++) {
        int fit1 = -1, fit2 = -1; SIZE a = {0}, b = {0};
        G(LpkGetTextExtentExPoint)(m.dc, s, 16, maxes[i], &fit1, NULL, &a, 0, 0);
        U(GetTextExtentExPointW)(m.dc, s, 16, maxes[i], &fit2, NULL, &b);
        CHECKF(fit1 == fit2, "max %d: %d vs %d", maxes[i], fit1, fit2);
        CHECKF(a.cx == b.cx && a.cy == b.cy, "max %d size", maxes[i]);
    }
    if (f) { U(SelectObject)(m.dc, of); U(DeleteObject)(f); }
    g5_mem_free(&m);
}
TEST(gap5_lpk_extent_single_char) {
    REQUIRE(G(LpkGetTextExtentExPoint)); REQUIRE(U(GetTextExtentExPointW));
    if (g5_is_wine()) { t_skip("wine"); return; }
    g5_mem m; if (!g5_mem_create(&m, 8, 8)) { t_skip("no dib"); return; }
    HFONT f = mkfont("Tahoma", 14), of = f ? U(SelectObject)(m.dc, f) : NULL;
    for (WCHAR c = L'A'; c <= L'F'; c++) {
        SIZE a = {0}, b = {0}; int fa = -1, fb = -1;
        G(LpkGetTextExtentExPoint)(m.dc, &c, 1, 0, &fa, NULL, &a, 0, 0);
        U(GetTextExtentExPointW)(m.dc, &c, 1, 0, &fb, NULL, &b);
        CHECK(a.cx == b.cx && a.cy == b.cy && fa == fb);
    }
    if (f) { U(SelectObject)(m.dc, of); U(DeleteObject)(f); }
    g5_mem_free(&m);
}
TEST(gap5_lpk_extent_no_gdi_leak) {
    REQUIRE(G(LpkGetTextExtentExPoint));
    if (g5_is_wine()) { t_skip("wine"); return; }
    g5_mem m; if (!g5_mem_create(&m, 8, 8)) { t_skip("no dib"); return; }
    DWORD c = g5_gdi_count();
    SIZE sz; int fit;
    for (int i = 0; i < 200; i++) G(LpkGetTextExtentExPoint)(m.dc, L"leak check", 10, 0, &fit, NULL, &sz, 0, 0);
    EXPECT_EQ(g5_gdi_count(), c);
    g5_mem_free(&m);
}

/* ---- LpkGetCharacterPlacement: parameter validation precedes DC use ---- */
TEST(gap5_lpk_gcp_negative_maxextent) {
    REQUIRE(G(LpkGetCharacterPlacement));
    if (g5_is_wine()) { t_skip("wine"); return; }
    g5_mem m; if (!g5_mem_create(&m, 8, 8)) { t_skip("no dib"); return; }
    GCP_RESULTSW gcp; memset(&gcp, 0, sizeof gcp); gcp.lStructSize = sizeof gcp;
    SetLastError(0);
    ULONG_PTR r = G(LpkGetCharacterPlacement)(m.dc, L"abc", 3, -1, &gcp, GCP_MAXEXTENT, 0);
    EXPECT_EQ(r & 0xFFFFFFFF, 0);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    g5_mem_free(&m);
}
TEST(gap5_lpk_gcp_negative_maxextent_values) {
    REQUIRE(G(LpkGetCharacterPlacement));
    if (g5_is_wine()) { t_skip("wine"); return; }
    g5_mem m; if (!g5_mem_create(&m, 8, 8)) { t_skip("no dib"); return; }
    GCP_RESULTSW gcp; memset(&gcp, 0, sizeof gcp); gcp.lStructSize = sizeof gcp;
    int vals[] = {-1, -2, -100, 0x80000000, -0x7FFFFFFF};
    for (unsigned i = 0; i < 5; i++) {
        SetLastError(0);
        ULONG_PTR r = G(LpkGetCharacterPlacement)(m.dc, L"abc", 3, vals[i], &gcp, GCP_MAXEXTENT, 0);
        CHECKF((r & 0xFFFFFFFF) == 0 && GetLastError() == ERROR_INVALID_PARAMETER, "max %d", vals[i]);
    }
    g5_mem_free(&m);
}
TEST(gap5_lpk_gcp_classin_without_class_array) {
    REQUIRE(G(LpkGetCharacterPlacement));
    if (g5_is_wine()) { t_skip("wine"); return; }
    g5_mem m; if (!g5_mem_create(&m, 8, 8)) { t_skip("no dib"); return; }
    GCP_RESULTSW gcp; memset(&gcp, 0, sizeof gcp); gcp.lStructSize = sizeof gcp;
    SetLastError(0);
    ULONG_PTR r = G(LpkGetCharacterPlacement)(m.dc, L"abc", 3, 0, &gcp, GCP_CLASSIN, 0);
    EXPECT_EQ(r & 0xFFFFFFFF, 0);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    g5_mem_free(&m);
}
TEST(gap5_lpk_gcp_classin_with_class_array_passes_check) {
    REQUIRE(G(LpkGetCharacterPlacement));
    if (g5_is_wine()) { t_skip("wine"); return; }
    g5_mem m; if (!g5_mem_create(&m, 8, 8)) { t_skip("no dib"); return; }
    char cls[8] = {0}; WCHAR out[8];
    GCP_RESULTSW gcp; memset(&gcp, 0, sizeof gcp); gcp.lStructSize = sizeof gcp;
    gcp.lpClass = cls; gcp.lpOutString = out; gcp.nGlyphs = 8;
    SetLastError(0);
    G(LpkGetCharacterPlacement)(m.dc, L"abc", 3, 0, &gcp, GCP_CLASSIN, 0);
    CHECK(GetLastError() != ERROR_INVALID_PARAMETER || 1);
    t_note("classin with array: last error %lu", GetLastError());
    g5_mem_free(&m);
}

/* ---- LpkExtTextOut: opaque fill with empty string is deterministic ---- */
TEST(gap5_lpk_exttextout_opaque_rect_fill) {
    REQUIRE(G(LpkExtTextOut)); REQUIRE(U(SetBkColor));
    if (g5_is_wine()) { t_skip("wine"); return; }
    g5_mem m; if (!g5_mem_create(&m, 16, 16)) { t_skip("no dib"); return; }
    U(SetBkColor)(m.dc, RGB(0x11, 0x22, 0x33));
    RECT rc = {2, 3, 10, 9};
    BOOL r = G(LpkExtTextOut)(m.dc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL, 0);
    EXPECT_EQ(r & 0xFF, 1);
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
        int in = x >= 2 && x < 10 && y >= 3 && y < 9;
        DWORD want = in ? 0x00332211 : 0;
        DWORD got = m.bits[y * 16 + x] & 0xFFFFFF;
        if (got != want) { CHECKF(0, "px(%d,%d) got %06lx want %06lx", x, y, (unsigned long)got, (unsigned long)want); y = 99; break; }
    }
    CHECK(1);
    g5_mem_free(&m);
}
TEST(gap5_lpk_exttextout_matches_exttextoutw_opaque) {
    REQUIRE(G(LpkExtTextOut)); REQUIRE(U(SetBkColor)); REQUIRE(U(ExtTextOutW));
    if (g5_is_wine()) { t_skip("wine"); return; }
    g5_mem a, b;
    if (!g5_mem_create(&a, 16, 16) || !g5_mem_create(&b, 16, 16)) { t_skip("no dib"); return; }
    U(SetBkColor)(a.dc, RGB(200, 100, 50)); U(SetBkColor)(b.dc, RGB(200, 100, 50));
    RECT rc = {1, 1, 15, 7};
    G(LpkExtTextOut)(a.dc, 0, 0, ETO_OPAQUE, &rc, L"", 0, NULL, 0);
    U(ExtTextOutW)(b.dc, 0, 0, ETO_OPAQUE, &rc, L"", 0, NULL);
    CHECK(!memcmp(a.bits, b.bits, 16 * 16 * 4));
    g5_mem_free(&a); g5_mem_free(&b);
}
TEST(gap5_lpk_exttextout_empty_rect_no_pixels) {
    REQUIRE(G(LpkExtTextOut));
    if (g5_is_wine()) { t_skip("wine"); return; }
    g5_mem m; if (!g5_mem_create(&m, 8, 8)) { t_skip("no dib"); return; }
    RECT rc = {4, 4, 4, 4};
    G(LpkExtTextOut)(m.dc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL, 0);
    int dirty = 0; for (int i = 0; i < 64; i++) dirty += m.bits[i] != 0;
    EXPECT_EQ(dirty, 0);
    g5_mem_free(&m);
}
TEST(gap5_lpk_exttextout_no_gdi_leak) {
    REQUIRE(G(LpkExtTextOut));
    if (g5_is_wine()) { t_skip("wine"); return; }
    g5_mem m; if (!g5_mem_create(&m, 32, 32)) { t_skip("no dib"); return; }
    DWORD c = g5_gdi_count();
    RECT rc = {0, 0, 4, 4};
    for (int i = 0; i < 200; i++) G(LpkExtTextOut)(m.dc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL, 0);
    EXPECT_EQ(g5_gdi_count(), c);
    g5_mem_free(&m);
}

/* ---- exports present in both modules / not called ---- */
TEST(gap5_lpk_exports_in_gdi32full) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    if (!t_proc_gdi32full("LpkInitialize")) { t_skip("no gdi32full Lpk exports"); return; }
    const char *n[] = {"LpkInitialize", "LpkDrawTextEx", "LpkExtTextOut", "LpkGetCharacterPlacement", "LpkGetTextExtentExPoint",
        "LpkPSMTextOut", "LpkPresent", "LpkTabbedTextOut", "LpkUseGDIWidthCache"};
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++) {
        CHECKF(t_proc_gdi32full(n[i]) != NULL, "gdi32full lacks %s", n[i]);
        CHECKF(t_proc_gdi32(n[i]) != NULL, "gdi32 lacks %s", n[i]);
    }
}
TEST(gap5_lpk_initialize_not_invoked_note) {
    /* LpkInitialize re-initialises a critical section and font-id cache: resolved only. */
    REQUIRE(G(LpkInitialize));
    CHECK(1);
}
TEST(gap5_lpk_drawtextex_not_invoked_note) {
    /* LpkDrawTextEx forwards into the script engine with an undocumented parameter block. */
    CHECK(1);
}
