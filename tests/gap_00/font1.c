/* GetKerningPairs, GetGlyphOutline(+Wow), GetCharABCWidthsFloatI, GetTextExtentExPointWPri,
 * GetTextFaceAliasW, GetTextMetricsAImpl, GetETM, GetFontRealizationInfo, GetHFONT */
#include "gap.h"

UDECL(DWORD, GetGlyphOutlineWow, (HDC, UINT, UINT, LPGLYPHMETRICS, DWORD, LPVOID, const MAT2 *));
UDECL(BOOL, GetCharABCWidthsFloatI, (HDC, UINT, UINT, LPWORD, LPABCFLOAT));
UDECL(BOOL, GetTextExtentExPointWPri, (HDC, LPCWSTR, int, int, LPINT, LPINT, LPSIZE));
UDECL(int, GetTextFaceAliasW, (HDC, int, LPWSTR));
UDECL(BOOL, GetTextMetricsAImpl, (HDC, LPTEXTMETRICA));
typedef struct { short v[26]; } ETM26;
UDECL(int, GetETM, (HDC, ETM26 *));
typedef struct { DWORD cbSize, flags, cacheNum, instanceId; BYTE pad[0x32c - 16]; } REALINFO;
UDECL(int, GetFontRealizationInfo, (HDC, void *));
UDECL(HFONT, GetHFONT, (HDC));

static const MAT2 ID2 = { {0, 1}, {0, 0}, {0, 0}, {0, 1} };

/* ================= GetKerningPairs ================= */
TEST(gap0_kern_resolve) {
    CHECK(t_proc_gdi32("GetKerningPairs") != NULL);
    CHECK(t_proc_gdi32("GetKerningPairsA") != NULL);
    CHECK(t_proc_gdi32("GetKerningPairsW") != NULL);
    CHECK(t_proc_gdi32full("GetKerningPairsW") != NULL);
    CHECK(t_proc_gdi32full("GetKerningPairsA") != NULL);
    SOFT(t_proc_gdi32full("GetKerningPairs") == NULL, "GetKerningPairs is a gdi32.dll-only name");
}
TEST(gap0_kern_null_dc) {
    REQUIRE(UG(GetKerningPairs)); REQUIRE(U(GetKerningPairsW));
    KERNINGPAIR kp[4];
    CHECK(UG(GetKerningPairs)(NULL, 0, NULL) == 0);
    CHECK(UG(GetKerningPairs)(NULL, 4, kp) == 0);
    CHECK(U(GetKerningPairsW)(NULL, 0, NULL) == 0);
    CHECK(U(GetKerningPairsW)(NULL, 4, kp) == 0);
    CHECK(U(GetKerningPairsA)(NULL, 4, kp) == 0);
}
TEST(gap0_kern_bogus_dc) {
    REQUIRE(U(GetKerningPairsW));
    ULONG_PTR h[] = { 0x1234, 0xdeadbeef, 0x10000, 0x7fff0000 };
    for (unsigned i = 0; i < sizeof h / sizeof *h; i++) {
        CHECKF(U(GetKerningPairsW)((HDC)h[i], 0, NULL) == 0, "W %llx", (unsigned long long)h[i]);
        CHECKF(U(GetKerningPairsA)((HDC)h[i], 0, NULL) == 0, "A %llx", (unsigned long long)h[i]);
        CHECKF(UG(GetKerningPairs)((HDC)h[i], 0, NULL) == 0, "plain %llx", (unsigned long long)h[i]);
    }
}
TEST(gap0_kern_zero_count_with_buffer_w) {
    REQUIRE(U(GetKerningPairsW));
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    KERNINGPAIR kp; memset(&kp, 0x77, sizeof kp);
    SET_ERR(0);
    CHECK(U(GetKerningPairsW)(m.dc, 0, &kp) == 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    EXPECT_EQ(kp.wFirst, 0x7777);
    gap_mdc_close(&m);
}
TEST(gap0_kern_zero_count_with_buffer_a) {
    REQUIRE(U(GetKerningPairsA));
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    KERNINGPAIR kp;
    SET_ERR(0);
    CHECK(U(GetKerningPairsA)(m.dc, 0, &kp) == 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    gap_mdc_close(&m);
}
TEST(gap0_kern_zero_count_with_buffer_plain) {
    REQUIRE(UG(GetKerningPairs));
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    KERNINGPAIR kp;
    SET_ERR(0);
    CHECK(UG(GetKerningPairs)(m.dc, 0, &kp) == 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    gap_mdc_close(&m);
}
TEST(gap0_kern_arial_count_query) {
    REQUIRE(U(GetKerningPairsW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -40);
    DWORD w = U(GetKerningPairsW)(fx.m.dc, 0, NULL);
    DWORD a = U(GetKerningPairsA)(fx.m.dc, 0, NULL);
    DWORD p = UG(GetKerningPairs)(fx.m.dc, 0, NULL);
    t_note("Arial kern pairs W=%lu A=%lu", w, a);
    SOFT(w > 0, "Arial has kerning pairs");
    CHECK(a <= w);
    EXPECT_EQ(p, a);
    DWORD w2 = U(GetKerningPairsW)(fx.m.dc, 0, NULL);
    EXPECT_EQ(w, w2);
    gap_fx_close(&fx);
}
TEST(gap0_kern_arial_nonnull_count_nobuffer) {
    REQUIRE(U(GetKerningPairsW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -40);
    DWORD w = U(GetKerningPairsW)(fx.m.dc, 0, NULL);
    DWORD w5 = U(GetKerningPairsW)(fx.m.dc, 5, NULL);
    EXPECT_EQ(w, w5);
    gap_fx_close(&fx);
}
TEST(gap0_kern_arial_fill_exact) {
    REQUIRE(U(GetKerningPairsW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -40);
    DWORD n = U(GetKerningPairsW)(fx.m.dc, 0, NULL);
    if (!n || n > 100000) { gap_fx_close(&fx); t_skip("no kerning pairs"); return; }
    KERNINGPAIR *kp = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (n + 2) * sizeof *kp);
    kp[n].wFirst = 0xABCD; kp[n].wSecond = 0xDCBA; kp[n].iKernAmount = 0x1234;
    DWORD got = U(GetKerningPairsW)(fx.m.dc, n, kp);
    EXPECT_EQ(got, n);
    EXPECT_EQ(kp[n].wFirst, 0xABCD); EXPECT_EQ(kp[n].wSecond, 0xDCBA); EXPECT_EQ(kp[n].iKernAmount, 0x1234);
    DWORD zero = 0, nonzero = 0, big = 0;
    for (DWORD i = 0; i < got; i++) {
        if (!kp[i].wFirst || !kp[i].wSecond) zero++;
        if (kp[i].iKernAmount) nonzero++;
        if (kp[i].iKernAmount > 40 || kp[i].iKernAmount < -40) big++;
    }
    CHECK(zero == 0);
    SOFT(nonzero > 0, "some pair has non-zero amount");
    SOFT(big == 0, "kerning amounts within one em");
    HeapFree(GetProcessHeap(), 0, kp);
    gap_fx_close(&fx);
}
TEST(gap0_kern_arial_larger_buffer) {
    REQUIRE(U(GetKerningPairsW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -40);
    DWORD n = U(GetKerningPairsW)(fx.m.dc, 0, NULL);
    if (!n || n > 100000) { gap_fx_close(&fx); t_skip("no kerning pairs"); return; }
    KERNINGPAIR *kp = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (n + 50) * sizeof *kp);
    DWORD got = U(GetKerningPairsW)(fx.m.dc, n + 50, kp);
    EXPECT_EQ(got, n);
    HeapFree(GetProcessHeap(), 0, kp);
    gap_fx_close(&fx);
}
TEST(gap0_kern_arial_too_small_w) {
    REQUIRE(U(GetKerningPairsW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -40);
    DWORD n = U(GetKerningPairsW)(fx.m.dc, 0, NULL);
    if (n < 3) { gap_fx_close(&fx); t_skip("too few pairs"); return; }
    KERNINGPAIR *kp = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n * sizeof *kp);
    DWORD r = U(GetKerningPairsW)(fx.m.dc, n - 1, kp);
    t_note("buffer one too small: ret %lu err %lu", r, GetLastError());
    SOFT(r == 0, "too-small buffer yields 0");
    r = U(GetKerningPairsW)(fx.m.dc, 1, kp);
    SOFT(r == 0, "buffer of 1 yields 0");
    HeapFree(GetProcessHeap(), 0, kp);
    gap_fx_close(&fx);
}
TEST(gap0_kern_arial_ab_consistency) {
    REQUIRE(U(GetKerningPairsW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -40);
    DWORD nw = U(GetKerningPairsW)(fx.m.dc, 0, NULL), na = U(GetKerningPairsA)(fx.m.dc, 0, NULL);
    if (!nw || !na || nw > 100000) { gap_fx_close(&fx); t_skip("no pairs"); return; }
    KERNINGPAIR *w = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, nw * sizeof *w);
    KERNINGPAIR *a = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, na * sizeof *a);
    CHECK(U(GetKerningPairsW)(fx.m.dc, nw, w) == nw);
    DWORD ra = U(GetKerningPairsA)(fx.m.dc, na, a);
    EXPECT_EQ(ra, na);
    /* every ANSI pair must exist in the wide list with the same amount (ASCII only compared) */
    int missing = 0;
    for (DWORD i = 0; i < ra; i++) {
        if (a[i].wFirst > 127 || a[i].wSecond > 127) continue;
        int found = 0;
        for (DWORD j = 0; j < nw && !found; j++) if (w[j].wFirst == a[i].wFirst && w[j].wSecond == a[i].wSecond && w[j].iKernAmount == a[i].iKernAmount) found = 1;
        if (!found) missing++;
    }
    EXPECT_EQ(missing, 0);
    HeapFree(GetProcessHeap(), 0, w); HeapFree(GetProcessHeap(), 0, a);
    gap_fx_close(&fx);
}
TEST(gap0_kern_courier_new) {
    REQUIRE(U(GetKerningPairsW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Courier New", -20);
    DWORD n = U(GetKerningPairsW)(fx.m.dc, 0, NULL);
    t_note("Courier New kern pairs: %lu", n);
    SOFT(n == 0, "monospaced font has no kerning");
    gap_fx_close(&fx);
}
TEST(gap0_kern_stock_bitmap_font) {
    REQUIRE(U(GetKerningPairsW)); REQUIRE(U(GetStockObject));
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    HGDIOBJ old = U(SelectObject)(m.dc, U(GetStockObject)(SYSTEM_FONT));
    DWORD n = U(GetKerningPairsW)(m.dc, 0, NULL);
    t_note("System font kern pairs: %lu", n);
    SOFT(n == 0, "System font has no kerning");
    U(SelectObject)(m.dc, old);
    gap_mdc_close(&m);
}
TEST(gap0_kern_size_independent_count) {
    REQUIRE(U(GetKerningPairsW));
    DWORD counts[3]; int heights[3] = { -10, -40, -100 };
    for (int i = 0; i < 3; i++) {
        gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", heights[i]);
        counts[i] = U(GetKerningPairsW)(fx.m.dc, 0, NULL);
        gap_fx_close(&fx);
    }
    SOFT(counts[0] == counts[1] && counts[1] == counts[2], "pair count independent of size");
    t_note("counts %lu %lu %lu", counts[0], counts[1], counts[2]);
}
TEST(gap0_kern_amount_scales_with_size) {
    REQUIRE(U(GetKerningPairsW));
    gap_fx a, b; FX_OPEN_OR_SKIP(a, L"Arial", -20);
    if (!gap_fx_open(&b, L"Arial", -80) || !b.ok) { gap_fx_close(&a); gap_fx_close(&b); t_skip("font"); return; }
    DWORD n = U(GetKerningPairsW)(a.m.dc, 0, NULL);
    if (n && n < 100000 && U(GetKerningPairsW)(b.m.dc, 0, NULL) == n) {
        KERNINGPAIR *ka = HeapAlloc(GetProcessHeap(), 0, n * sizeof *ka), *kb = HeapAlloc(GetProcessHeap(), 0, n * sizeof *kb);
        U(GetKerningPairsW)(a.m.dc, n, ka); U(GetKerningPairsW)(b.m.dc, n, kb);
        int bad = 0;
        for (DWORD i = 0; i < n; i++) {
            if (ka[i].wFirst != kb[i].wFirst || ka[i].wSecond != kb[i].wSecond) { bad++; continue; }
            if (ka[i].iKernAmount * 4 - kb[i].iKernAmount > 6 || kb[i].iKernAmount - ka[i].iKernAmount * 4 > 6) bad++;
        }
        SOFT(bad == 0, "amounts scale ~4x between 20px and 80px");
        HeapFree(GetProcessHeap(), 0, ka); HeapFree(GetProcessHeap(), 0, kb);
    }
    gap_fx_close(&a); gap_fx_close(&b);
}

/* ================= GetGlyphOutline ================= */
TEST(gap0_ggo_resolve) {
    CHECK(t_proc_gdi32("GetGlyphOutline") != NULL);
    CHECK(t_proc_gdi32("GetGlyphOutlineA") != NULL);
    CHECK(t_proc_gdi32("GetGlyphOutlineW") != NULL);
    CHECK(t_proc_gdi32full("GetGlyphOutlineA") != NULL);
    CHECK(t_proc_gdi32full("GetGlyphOutlineW") != NULL);
    CHECK(t_proc_gdi32full("GetGlyphOutlineWow") != NULL);
    CHECK(t_proc_gdi32("GetGlyphOutlineWow") != NULL);
}
TEST(gap0_ggo_null_dc) {
    REQUIRE(UG(GetGlyphOutline)); REQUIRE(U(GetGlyphOutlineW));
    GLYPHMETRICS gm;
    CHECK(UG(GetGlyphOutline)(NULL, 'A', GGO_METRICS, &gm, 0, NULL, &ID2) == GDI_ERROR);
    CHECK(U(GetGlyphOutlineA)(NULL, 'A', GGO_METRICS, &gm, 0, NULL, &ID2) == GDI_ERROR);
    CHECK(U(GetGlyphOutlineW)(NULL, 'A', GGO_METRICS, &gm, 0, NULL, &ID2) == GDI_ERROR);
}
TEST(gap0_ggo_bogus_dc) {
    REQUIRE(U(GetGlyphOutlineW));
    ULONG_PTR h[] = { 0x1234, 0xdeadbeef, 0x10000, 0x7fff0000 };
    for (unsigned i = 0; i < sizeof h / sizeof *h; i++) {
        GLYPHMETRICS gm;
        CHECKF(U(GetGlyphOutlineW)((HDC)h[i], 'A', GGO_METRICS, &gm, 0, NULL, &ID2) == GDI_ERROR, "W %llx", (unsigned long long)h[i]);
        CHECKF(U(GetGlyphOutlineA)((HDC)h[i], 'A', GGO_NATIVE, &gm, 0, NULL, &ID2) == GDI_ERROR, "A %llx", (unsigned long long)h[i]);
    }
}
TEST(gap0_ggo_deleted_dc) {
    REQUIRE(U(GetGlyphOutlineW)); REQUIRE(U(CreateCompatibleDC));
    HDC dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    GLYPHMETRICS gm;
    CHECK(U(GetGlyphOutlineW)(dc, 'A', GGO_METRICS, &gm, 0, NULL, &ID2) == GDI_ERROR);
}
TEST(gap0_ggo_null_metrics_w) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -40);
    SET_ERR(0x1357);
    CHECK(U(GetGlyphOutlineW)(fx.m.dc, 'A', GGO_METRICS, NULL, 0, NULL, &ID2) == GDI_ERROR);
    EXPECT_EQ(GetLastError(), 0x1357);
    gap_fx_close(&fx);
}
TEST(gap0_ggo_null_mat2_w) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -40);
    GLYPHMETRICS gm; memset(&gm, 0x55, sizeof gm);
    SET_ERR(0x2468);
    CHECK(U(GetGlyphOutlineW)(fx.m.dc, 'A', GGO_METRICS, &gm, 0, NULL, NULL) == GDI_ERROR);
    EXPECT_EQ(GetLastError(), 0x2468);
    EXPECT_EQ(gm.gmBlackBoxX, 0x55555555);
    gap_fx_close(&fx);
}
TEST(gap0_ggo_null_both_w) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -40);
    CHECK(U(GetGlyphOutlineW)(fx.m.dc, 'A', GGO_METRICS, NULL, 0, NULL, NULL) == GDI_ERROR);
    gap_fx_close(&fx);
}
TEST(gap0_ggo_null_args_a) {
    REQUIRE(U(GetGlyphOutlineA));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -40);
    GLYPHMETRICS gm;
    CHECK(U(GetGlyphOutlineA)(fx.m.dc, 'A', GGO_METRICS, NULL, 0, NULL, &ID2) == GDI_ERROR);
    CHECK(U(GetGlyphOutlineA)(fx.m.dc, 'A', GGO_METRICS, &gm, 0, NULL, NULL) == GDI_ERROR);
    gap_fx_close(&fx);
}
TEST(gap0_ggo_metrics_basic) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -100);
    GLYPHMETRICS gm; memset(&gm, 0, sizeof gm);
    DWORD r = U(GetGlyphOutlineW)(fx.m.dc, 'A', GGO_METRICS, &gm, 0, NULL, &ID2);
    CHECK(r != GDI_ERROR);
    CHECK(gm.gmBlackBoxX > 0 && gm.gmBlackBoxX < 200);
    CHECK(gm.gmBlackBoxY > 0 && gm.gmBlackBoxY < 200);
    CHECK(gm.gmCellIncX > 0 && gm.gmCellIncX < 200);
    EXPECT_EQ(gm.gmCellIncY, 0);
    CHECK(gm.gmptGlyphOrigin.y > 0);
    CHECK(gm.gmptGlyphOrigin.y >= (LONG)gm.gmBlackBoxY - 1);
    gap_fx_close(&fx);
}
TEST(gap0_ggo_metrics_a_w_parity) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -60);
    const char *chars = "AgW.i";
    for (const char *c = chars; *c; c++) {
        GLYPHMETRICS a, w, p;
        DWORD ra = U(GetGlyphOutlineA)(fx.m.dc, *c, GGO_METRICS, &a, 0, NULL, &ID2);
        DWORD rw = U(GetGlyphOutlineW)(fx.m.dc, *c, GGO_METRICS, &w, 0, NULL, &ID2);
        DWORD rp = UG(GetGlyphOutline)(fx.m.dc, *c, GGO_METRICS, &p, 0, NULL, &ID2);
        EXPECT_EQ(ra, rw); EXPECT_EQ(ra, rp);
        CHECKF(!memcmp(&a, &w, sizeof a), "A/W metrics for '%c'", *c);
        CHECKF(!memcmp(&a, &p, sizeof a), "A/plain metrics for '%c'", *c);
    }
    gap_fx_close(&fx);
}
TEST(gap0_ggo_wow_matches_a) {
    GetGlyphOutlineWow_pfn wow = UN(GetGlyphOutlineWow); REQUIRE(wow); SKIP_WINE("unimplemented stub"); REQUIRE(U(GetGlyphOutlineA));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -60);
    GLYPHMETRICS a, b;
    DWORD ra = U(GetGlyphOutlineA)(fx.m.dc, 'Q', GGO_METRICS, &a, 0, NULL, &ID2);
    DWORD rb = wow(fx.m.dc, 'Q', GGO_METRICS, &b, 0, NULL, &ID2);
    EXPECT_EQ(ra, rb);
    CHECK(!memcmp(&a, &b, sizeof a));
    gap_fx_close(&fx);
}
TEST(gap0_ggo_wow_invalid) {
    GetGlyphOutlineWow_pfn wow = UN(GetGlyphOutlineWow); REQUIRE(wow); SKIP_WINE("unimplemented stub");
    GLYPHMETRICS gm;
    CHECK(wow(NULL, 'A', GGO_METRICS, &gm, 0, NULL, &ID2) == GDI_ERROR);
    CHECK(wow((HDC)0x1234, 'A', GGO_NATIVE, &gm, 0, NULL, &ID2) == GDI_ERROR);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -20);
    CHECK(wow(fx.m.dc, 'A', GGO_METRICS, NULL, 0, NULL, &ID2) == GDI_ERROR);
    CHECK(wow(fx.m.dc, 'A', GGO_METRICS, &gm, 0, NULL, NULL) == GDI_ERROR);
    gap_fx_close(&fx);
}
TEST(gap0_ggo_metrics_vs_charwidth) {
    REQUIRE(U(GetGlyphOutlineW)); REQUIRE(U(GetCharWidth32W));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -80);
    const WCHAR chars[] = L"AaBbHiMmWw.,0";
    for (const WCHAR *c = chars; *c; c++) {
        GLYPHMETRICS gm; INT w = 0;
        DWORD r = U(GetGlyphOutlineW)(fx.m.dc, *c, GGO_METRICS, &gm, 0, NULL, &ID2);
        CHECK(r != GDI_ERROR);
        CHECK(U(GetCharWidth32W)(fx.m.dc, *c, *c, &w));
        SOFT((int)gm.gmCellIncX == w, "cellIncX equals GetCharWidth32");
    }
    gap_fx_close(&fx);
}
TEST(gap0_ggo_metrics_vs_abc) {
    REQUIRE(U(GetGlyphOutlineW)); REQUIRE(U(GetCharABCWidthsW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -80);
    const WCHAR chars[] = L"AjfgT";
    for (const WCHAR *c = chars; *c; c++) {
        GLYPHMETRICS gm; ABC abc;
        U(GetGlyphOutlineW)(fx.m.dc, *c, GGO_METRICS, &gm, 0, NULL, &ID2);
        CHECK(U(GetCharABCWidthsW)(fx.m.dc, *c, *c, &abc));
        SOFT(gm.gmptGlyphOrigin.x == abc.abcA, "origin.x == abcA");
        SOFT((UINT)abc.abcB == gm.gmBlackBoxX, "abcB == black box width");
        SOFT((int)gm.gmCellIncX == abc.abcA + (int)abc.abcB + abc.abcC, "A+B+C == cellIncX");
    }
    gap_fx_close(&fx);
}
TEST(gap0_ggo_space_metrics) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -80);
    GLYPHMETRICS gm;
    DWORD r = U(GetGlyphOutlineW)(fx.m.dc, L' ', GGO_METRICS, &gm, 0, NULL, &ID2);
    CHECK(r != GDI_ERROR);
    CHECK(gm.gmCellIncX > 0);
    t_note("space: ret %lu bb %ux%u inc %d", r, gm.gmBlackBoxX, gm.gmBlackBoxY, gm.gmCellIncX);
    gap_fx_close(&fx);
}
TEST(gap0_ggo_space_native_empty) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -80);
    GLYPHMETRICS gm;
    DWORD r = U(GetGlyphOutlineW)(fx.m.dc, L' ', GGO_NATIVE, &gm, 0, NULL, &ID2);
    SOFT(r == 0, "blank glyph has no outline data");
    r = U(GetGlyphOutlineW)(fx.m.dc, L' ', GGO_BITMAP, &gm, 0, NULL, &ID2);
    SOFT(r == 0 || r == GDI_ERROR || r >= 4, "blank bitmap");
    t_note("space bitmap ret %lu", r);
    gap_fx_close(&fx);
}
TEST(gap0_ggo_bitmap_size_formula) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -48);
    const WCHAR chars[] = L"AgWl.M@#";
    for (const WCHAR *c = chars; *c; c++) {
        GLYPHMETRICS gm;
        DWORD r = U(GetGlyphOutlineW)(fx.m.dc, *c, GGO_BITMAP, &gm, 0, NULL, &ID2);
        CHECKF(r != GDI_ERROR, "glyph %c", (char)*c);
        if (r == GDI_ERROR) continue;
        DWORD pitch = ((gm.gmBlackBoxX + 31) / 32) * 4;
        CHECKF(r == pitch * gm.gmBlackBoxY, "glyph %c ret %lu pitch %lu h %u", (char)*c, r, pitch, gm.gmBlackBoxY);
    }
    gap_fx_close(&fx);
}
TEST(gap0_ggo_bitmap_fill_and_content) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -48);
    GLYPHMETRICS g0, g1;
    DWORD sz = U(GetGlyphOutlineW)(fx.m.dc, 'H', GGO_BITMAP, &g0, 0, NULL, &ID2);
    if (sz == GDI_ERROR || sz == 0 || sz > 65536) { gap_fx_close(&fx); t_skip("no bitmap"); return; }
    BYTE *buf = HeapAlloc(GetProcessHeap(), 0, sz + 16);
    memset(buf, 0xEE, sz + 16);
    DWORD r = U(GetGlyphOutlineW)(fx.m.dc, 'H', GGO_BITMAP, &g1, sz, buf, &ID2);
    EXPECT_EQ(r, sz);
    CHECK(!memcmp(&g0, &g1, sizeof g0));
    for (int i = 0; i < 16; i++) CHECK(buf[sz + i] == 0xEE);
    DWORD ones = 0; for (DWORD i = 0; i < sz; i++) { BYTE b = buf[i]; while (b) { ones += b & 1; b >>= 1; } }
    CHECK(ones > 0);
    CHECK(ones < g0.gmBlackBoxX * g0.gmBlackBoxY);
    /* 'H' has two vertical stems: first row must contain set pixels at both left and right edges */
    SOFT(buf[0] & 0x80, "top-left pixel of H set");
    HeapFree(GetProcessHeap(), 0, buf);
    gap_fx_close(&fx);
}
TEST(gap0_ggo_bitmap_buffer_too_small) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -48);
    GLYPHMETRICS gm;
    DWORD sz = U(GetGlyphOutlineW)(fx.m.dc, 'H', GGO_BITMAP, &gm, 0, NULL, &ID2);
    if (sz == GDI_ERROR || sz < 8) { gap_fx_close(&fx); t_skip("no bitmap"); return; }
    BYTE *buf = HeapAlloc(GetProcessHeap(), 0, sz);
    memset(buf, 0xEE, sz);
    DWORD r = U(GetGlyphOutlineW)(fx.m.dc, 'H', GGO_BITMAP, &gm, sz - 1, buf, &ID2);
    SOFT(r == GDI_ERROR, "one byte short returns GDI_ERROR");
    r = U(GetGlyphOutlineW)(fx.m.dc, 'H', GGO_BITMAP, &gm, 1, buf, &ID2);
    SOFT(r == GDI_ERROR, "1-byte buffer returns GDI_ERROR");
    HeapFree(GetProcessHeap(), 0, buf);
    gap_fx_close(&fx);
}
TEST(gap0_ggo_bitmap_deterministic) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -32);
    GLYPHMETRICS g; DWORD sz = U(GetGlyphOutlineW)(fx.m.dc, 'R', GGO_BITMAP, &g, 0, NULL, &ID2);
    if (sz == GDI_ERROR || sz == 0 || sz > 65536) { gap_fx_close(&fx); t_skip("no bitmap"); return; }
    BYTE *a = HeapAlloc(GetProcessHeap(), 0, sz), *b = HeapAlloc(GetProcessHeap(), 0, sz);
    CHECK(U(GetGlyphOutlineW)(fx.m.dc, 'R', GGO_BITMAP, &g, sz, a, &ID2) == sz);
    CHECK(U(GetGlyphOutlineW)(fx.m.dc, 'R', GGO_BITMAP, &g, sz, b, &ID2) == sz);
    CHECK(!memcmp(a, b, sz));
    HeapFree(GetProcessHeap(), 0, a); HeapFree(GetProcessHeap(), 0, b);
    gap_fx_close(&fx);
}
TEST(gap0_ggo_gray_sizes) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -48);
    UINT fmts[] = { GGO_GRAY2_BITMAP, GGO_GRAY4_BITMAP, GGO_GRAY8_BITMAP };
    for (int i = 0; i < 3; i++) {
        GLYPHMETRICS gm;
        DWORD r = U(GetGlyphOutlineW)(fx.m.dc, 'g', fmts[i], &gm, 0, NULL, &ID2);
        CHECKF(r != GDI_ERROR, "fmt %u", fmts[i]);
        if (r == GDI_ERROR) continue;
        CHECK(r > 0);
        CHECK(r % 4 == 0);
        SOFT(r == ((gm.gmBlackBoxX + 3) & ~3u) * gm.gmBlackBoxY, "gray size = dword rows");
    }
    gap_fx_close(&fx);
}
TEST(gap0_ggo_gray_values_range) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -48);
    struct { UINT f; BYTE max; } t[] = { { GGO_GRAY2_BITMAP, 4 }, { GGO_GRAY4_BITMAP, 16 }, { GGO_GRAY8_BITMAP, 64 } };
    for (int i = 0; i < 3; i++) {
        GLYPHMETRICS gm;
        DWORD sz = U(GetGlyphOutlineW)(fx.m.dc, 'O', t[i].f, &gm, 0, NULL, &ID2);
        if (sz == GDI_ERROR || !sz || sz > 65536) continue;
        BYTE *b = HeapAlloc(GetProcessHeap(), 0, sz);
        DWORD r = U(GetGlyphOutlineW)(fx.m.dc, 'O', t[i].f, &gm, sz, b, &ID2);
        EXPECT_EQ(r, sz);
        int over = 0, nz = 0;
        for (DWORD k = 0; k < sz; k++) { if (b[k] > t[i].max) over++; if (b[k]) nz++; }
        CHECKF(over == 0, "fmt %u has %d out-of-range pixels", t[i].f, over);
        CHECK(nz > 0);
        HeapFree(GetProcessHeap(), 0, b);
    }
    gap_fx_close(&fx);
}
TEST(gap0_ggo_native_structure) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -100);
    const WCHAR chars[] = L"AOgB&";
    for (const WCHAR *c = chars; *c; c++) {
        GLYPHMETRICS gm;
        DWORD sz = U(GetGlyphOutlineW)(fx.m.dc, *c, GGO_NATIVE, &gm, 0, NULL, &ID2);
        CHECKF(sz != GDI_ERROR && sz >= sizeof(TTPOLYGONHEADER), "glyph %c size %lu", (char)*c, sz);
        if (sz == GDI_ERROR || sz > 65536 || sz < sizeof(TTPOLYGONHEADER)) continue;
        BYTE *b = HeapAlloc(GetProcessHeap(), 0, sz);
        DWORD r = U(GetGlyphOutlineW)(fx.m.dc, *c, GGO_NATIVE, &gm, sz, b, &ID2);
        EXPECT_EQ(r, sz);
        DWORD off = 0; int polys = 0, ok = 1;
        while (off + sizeof(TTPOLYGONHEADER) <= sz) {
            TTPOLYGONHEADER *h = (TTPOLYGONHEADER *)(b + off);
            if (h->dwType != TT_POLYGON_TYPE || h->cb < sizeof *h || off + h->cb > sz) { ok = 0; break; }
            DWORD cur = sizeof *h;
            while (cur + sizeof(TTPOLYCURVE) <= h->cb) {
                TTPOLYCURVE *pc = (TTPOLYCURVE *)((BYTE *)h + cur);
                if (pc->wType != TT_PRIM_LINE && pc->wType != TT_PRIM_QSPLINE) { ok = 0; break; }
                if (pc->cpfx == 0) { ok = 0; break; }
                cur += sizeof(WORD) * 2 + pc->cpfx * sizeof(POINTFX);
            }
            if (cur != h->cb) ok = 0;
            if (!ok) break;
            off += h->cb; polys++;
        }
        CHECKF(ok && off == sz, "glyph %c polygon walk (off %lu sz %lu)", (char)*c, off, sz);
        CHECK(polys >= 1);
        HeapFree(GetProcessHeap(), 0, b);
    }
    gap_fx_close(&fx);
}
TEST(gap0_ggo_native_polygon_counts) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -100);
    struct { WCHAR c; int polys; } t[] = { { L'I', 1 }, { L'O', 2 }, { L'l', 1 }, { L'i', 2 } };
    for (unsigned i = 0; i < 4; i++) {
        GLYPHMETRICS gm;
        DWORD sz = U(GetGlyphOutlineW)(fx.m.dc, t[i].c, GGO_NATIVE, &gm, 0, NULL, &ID2);
        if (sz == GDI_ERROR || sz > 65536 || !sz) continue;
        BYTE *b = HeapAlloc(GetProcessHeap(), 0, sz);
        U(GetGlyphOutlineW)(fx.m.dc, t[i].c, GGO_NATIVE, &gm, sz, b, &ID2);
        DWORD off = 0; int n = 0;
        while (off + sizeof(TTPOLYGONHEADER) <= sz) { TTPOLYGONHEADER *h = (TTPOLYGONHEADER *)(b + off); if (!h->cb) break; off += h->cb; n++; }
        SOFT(n == t[i].polys, "contour count");
        t_note("'%c' contours %d", (char)t[i].c, n);
        HeapFree(GetProcessHeap(), 0, b);
    }
    gap_fx_close(&fx);
}
TEST(gap0_ggo_bezier_structure) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -100);
    GLYPHMETRICS gm;
    DWORD sz = U(GetGlyphOutlineW)(fx.m.dc, 'S', GGO_BEZIER, &gm, 0, NULL, &ID2);
    CHECK(sz != GDI_ERROR);
    if (sz == GDI_ERROR || !sz || sz > 65536) { gap_fx_close(&fx); return; }
    BYTE *b = HeapAlloc(GetProcessHeap(), 0, sz);
    CHECK(U(GetGlyphOutlineW)(fx.m.dc, 'S', GGO_BEZIER, &gm, sz, b, &ID2) == sz);
    DWORD off = 0; int bad = 0;
    while (off + sizeof(TTPOLYGONHEADER) <= sz) {
        TTPOLYGONHEADER *h = (TTPOLYGONHEADER *)(b + off);
        if (h->dwType != TT_POLYGON_TYPE || h->cb < sizeof *h) { bad++; break; }
        DWORD cur = sizeof *h;
        while (cur + 4 <= h->cb) {
            TTPOLYCURVE *pc = (TTPOLYCURVE *)((BYTE *)h + cur);
            if (pc->wType == TT_PRIM_QSPLINE) bad++;
            if (pc->wType != TT_PRIM_LINE && pc->wType != TT_PRIM_CSPLINE) bad++;
            cur += 4 + pc->cpfx * sizeof(POINTFX);
        }
        off += h->cb;
    }
    EXPECT_EQ(bad, 0);
    EXPECT_EQ(off, sz);
    HeapFree(GetProcessHeap(), 0, b);
    gap_fx_close(&fx);
}
TEST(gap0_ggo_unhinted_flag) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -100);
    GLYPHMETRICS gm;
    DWORD a = U(GetGlyphOutlineW)(fx.m.dc, 'B', GGO_NATIVE, &gm, 0, NULL, &ID2);
    DWORD b = U(GetGlyphOutlineW)(fx.m.dc, 'B', GGO_NATIVE | GGO_UNHINTED, &gm, 0, NULL, &ID2);
    CHECK(a != GDI_ERROR && b != GDI_ERROR);
    SOFT(a == b, "hinting does not change the outline size of B at 100px");
    DWORD m1 = U(GetGlyphOutlineW)(fx.m.dc, 'B', GGO_METRICS | GGO_UNHINTED, &gm, 0, NULL, &ID2);
    CHECK(m1 != GDI_ERROR);
    gap_fx_close(&fx);
}
TEST(gap0_ggo_mat2_scale) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -50);
    MAT2 dbl = { {0, 2}, {0, 0}, {0, 0}, {0, 2} };
    GLYPHMETRICS g1, g2;
    CHECK(U(GetGlyphOutlineW)(fx.m.dc, 'H', GGO_METRICS, &g1, 0, NULL, &ID2) != GDI_ERROR);
    CHECK(U(GetGlyphOutlineW)(fx.m.dc, 'H', GGO_METRICS, &g2, 0, NULL, &dbl) != GDI_ERROR);
    SOFT((int)g2.gmBlackBoxX >= (int)g1.gmBlackBoxX * 2 - 2 && (int)g2.gmBlackBoxX <= (int)g1.gmBlackBoxX * 2 + 2, "x doubles");
    SOFT((int)g2.gmBlackBoxY >= (int)g1.gmBlackBoxY * 2 - 2 && (int)g2.gmBlackBoxY <= (int)g1.gmBlackBoxY * 2 + 2, "y doubles");
    SOFT((int)g2.gmCellIncX >= (int)g1.gmCellIncX * 2 - 2 && (int)g2.gmCellIncX <= (int)g1.gmCellIncX * 2 + 2, "advance doubles");
    gap_fx_close(&fx);
}
TEST(gap0_ggo_mat2_rotate90) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -50);
    MAT2 rot = { {0, 0}, {0, -1}, {0, 1}, {0, 0} };
    GLYPHMETRICS g1, g2;
    CHECK(U(GetGlyphOutlineW)(fx.m.dc, 'L', GGO_METRICS, &g1, 0, NULL, &ID2) != GDI_ERROR);
    DWORD r = U(GetGlyphOutlineW)(fx.m.dc, 'L', GGO_METRICS, &g2, 0, NULL, &rot);
    CHECK(r != GDI_ERROR);
    if (r != GDI_ERROR) {
        SOFT((int)g2.gmBlackBoxX >= (int)g1.gmBlackBoxY - 2 && (int)g2.gmBlackBoxX <= (int)g1.gmBlackBoxY + 2, "bbox swapped");
        SOFT(g2.gmCellIncX == 0 || g2.gmCellIncY != 0, "advance rotated");
    }
    gap_fx_close(&fx);
}
TEST(gap0_ggo_mat2_zero) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -50);
    MAT2 z; memset(&z, 0, sizeof z);
    GLYPHMETRICS gm;
    DWORD r = U(GetGlyphOutlineW)(fx.m.dc, 'H', GGO_METRICS, &gm, 0, NULL, &z);
    t_note("zero matrix: ret %lu", r);
    gap_fx_close(&fx);
}
TEST(gap0_ggo_mat2_identity_equal_metrics_native) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -50);
    GLYPHMETRICS a, b;
    DWORD ra = U(GetGlyphOutlineW)(fx.m.dc, 'K', GGO_METRICS, &a, 0, NULL, &ID2);
    DWORD rb = U(GetGlyphOutlineW)(fx.m.dc, 'K', GGO_NATIVE, &b, 0, NULL, &ID2);
    CHECK(ra != GDI_ERROR && rb != GDI_ERROR);
    SOFT(a.gmCellIncX == b.gmCellIncX, "cell inc independent of format");
    gap_fx_close(&fx);
}
TEST(gap0_ggo_glyph_index) {
    REQUIRE(U(GetGlyphOutlineW)); REQUIRE(U(GetGlyphIndicesW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -60);
    WCHAR ch = L'A'; WORD gi = 0;
    if (U(GetGlyphIndicesW)(fx.m.dc, &ch, 1, &gi, GGI_MARK_NONEXISTING_GLYPHS) != 1 || gi == 0xFFFF) { gap_fx_close(&fx); t_skip("no glyph index"); return; }
    GLYPHMETRICS gc, gx;
    DWORD r1 = U(GetGlyphOutlineW)(fx.m.dc, 'A', GGO_METRICS, &gc, 0, NULL, &ID2);
    DWORD r2 = U(GetGlyphOutlineW)(fx.m.dc, gi, GGO_METRICS | GGO_GLYPH_INDEX, &gx, 0, NULL, &ID2);
    EXPECT_EQ(r1, r2);
    CHECK(!memcmp(&gc, &gx, sizeof gc));
    GLYPHMETRICS gxa;
    DWORD r3 = U(GetGlyphOutlineA)(fx.m.dc, gi, GGO_METRICS | GGO_GLYPH_INDEX, &gxa, 0, NULL, &ID2);
    EXPECT_EQ(r1, r3);
    CHECK(!memcmp(&gc, &gxa, sizeof gc));
    gap_fx_close(&fx);
}
TEST(gap0_ggo_glyph_index_high_bits_w) {
    REQUIRE(U(GetGlyphOutlineW)); REQUIRE(U(GetGlyphIndicesW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -60);
    WCHAR ch = L'B'; WORD gi = 0;
    if (U(GetGlyphIndicesW)(fx.m.dc, &ch, 1, &gi, 0) != 1) { gap_fx_close(&fx); t_skip("no glyph index"); return; }
    GLYPHMETRICS g1, g2;
    DWORD r1 = U(GetGlyphOutlineW)(fx.m.dc, gi, GGO_METRICS | GGO_GLYPH_INDEX, &g1, 0, NULL, &ID2);
    DWORD r2 = U(GetGlyphOutlineW)(fx.m.dc, 0x10000u | gi, GGO_METRICS | GGO_GLYPH_INDEX, &g2, 0, NULL, &ID2);
    EXPECT_EQ(r1, r2);
    SOFT(!memcmp(&g1, &g2, sizeof g1), "upper 16 bits of uChar ignored");
    r2 = U(GetGlyphOutlineW)(fx.m.dc, 0xABCD0000u | 'B', GGO_METRICS, &g2, 0, NULL, &ID2);
    SOFT(r2 == r1, "upper 16 bits ignored for char lookup");
    gap_fx_close(&fx);
}
TEST(gap0_ggo_glyph_index_invalid) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -60);
    GLYPHMETRICS gm;
    DWORD r = U(GetGlyphOutlineW)(fx.m.dc, 0xFFFF, GGO_METRICS | GGO_GLYPH_INDEX, &gm, 0, NULL, &ID2);
    t_note("glyph index 0xFFFF: %lu", r);
    r = U(GetGlyphOutlineW)(fx.m.dc, 0, GGO_METRICS | GGO_GLYPH_INDEX, &gm, 0, NULL, &ID2);
    CHECK(r != GDI_ERROR);   /* glyph 0 (.notdef) always exists in a TrueType font */
    gap_fx_close(&fx);
}
TEST(gap0_ggo_a_sbcs_high_byte_ignored) {
    REQUIRE(U(GetGlyphOutlineA));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -60);
    if (U(GetTextCharset)(fx.m.dc) != ANSI_CHARSET || GetACP() != 1252) { gap_fx_close(&fx); t_skip("not an ANSI/1252 setup"); return; }
    GLYPHMETRICS g1, g2;
    DWORD r1 = U(GetGlyphOutlineA)(fx.m.dc, 'A', GGO_METRICS, &g1, 0, NULL, &ID2);
    DWORD r2 = U(GetGlyphOutlineA)(fx.m.dc, 0x4100 | 'A', GGO_METRICS, &g2, 0, NULL, &ID2);
    EXPECT_EQ(r1, r2);
    CHECK(!memcmp(&g1, &g2, sizeof g1));
    gap_fx_close(&fx);
}
TEST(gap0_ggo_a_vs_w_extended) {
    REQUIRE(U(GetGlyphOutlineA));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -60);
    if (GetACP() != 1252) { gap_fx_close(&fx); t_skip("ACP not 1252"); return; }
    /* 0xE9 is e-acute in cp1252 = U+00E9 */
    GLYPHMETRICS ga, gw;
    DWORD ra = U(GetGlyphOutlineA)(fx.m.dc, 0xE9, GGO_METRICS, &ga, 0, NULL, &ID2);
    DWORD rw = U(GetGlyphOutlineW)(fx.m.dc, 0xE9, GGO_METRICS, &gw, 0, NULL, &ID2);
    EXPECT_EQ(ra, rw);
    CHECK(!memcmp(&ga, &gw, sizeof ga));
    /* 0x80 is the euro sign in cp1252 = U+20AC */
    ra = U(GetGlyphOutlineA)(fx.m.dc, 0x80, GGO_METRICS, &ga, 0, NULL, &ID2);
    rw = U(GetGlyphOutlineW)(fx.m.dc, 0x20AC, GGO_METRICS, &gw, 0, NULL, &ID2);
    EXPECT_EQ(ra, rw);
    CHECK(!memcmp(&ga, &gw, sizeof ga));
    gap_fx_close(&fx);
}
TEST(gap0_ggo_missing_glyph) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -60);
    GLYPHMETRICS gm;
    DWORD r = U(GetGlyphOutlineW)(fx.m.dc, 0xE000, GGO_METRICS, &gm, 0, NULL, &ID2);
    t_note("private-use char: %lu (default glyph is used on Windows)", r);
    SOFT(r != GDI_ERROR, "missing glyph maps to .notdef");
    gap_fx_close(&fx);
}
TEST(gap0_ggo_bitmap_font_fails_or_works) {
    REQUIRE(U(GetGlyphOutlineW)); REQUIRE(U(GetStockObject));
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    HGDIOBJ old = U(SelectObject)(m.dc, U(GetStockObject)(SYSTEM_FONT));
    GLYPHMETRICS gm;
    DWORD r = U(GetGlyphOutlineW)(m.dc, 'A', GGO_METRICS, &gm, 0, NULL, &ID2);
    t_note("System (raster) font GGO_METRICS: %lu", r);
    r = U(GetGlyphOutlineW)(m.dc, 'A', GGO_NATIVE, &gm, 0, NULL, &ID2);
    SOFT(r == GDI_ERROR, "raster font has no native outline");
    U(SelectObject)(m.dc, old);
    gap_mdc_close(&m);
}
TEST(gap0_ggo_bad_format_values) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -60);
    GLYPHMETRICS gm;
    UINT bad[] = { 6, 7, 9, 0x10, 0x40, 0xFF, 0x100, 0x1000, 0x80000000 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        DWORD r = U(GetGlyphOutlineW)(fx.m.dc, 'A', bad[i], &gm, 0, NULL, &ID2);
        t_note("format 0x%x -> %lu", bad[i], r);
    }
    gap_fx_close(&fx);
}
TEST(gap0_ggo_no_gdi_leak) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -30);
    DWORD before = gap_gdi_count();
    GLYPHMETRICS gm;
    for (int i = 0; i < 500; i++) U(GetGlyphOutlineW)(fx.m.dc, 'A' + (i % 26), GGO_METRICS, &gm, 0, NULL, &ID2);
    EXPECT_EQ(gap_gdi_count(), before);
    gap_fx_close(&fx);
}
TEST(gap0_ggo_font_size_scales_metrics) {
    REQUIRE(U(GetGlyphOutlineW));
    UINT w[3] = { 0 }; int hs[3] = { -20, -40, -80 };
    for (int i = 0; i < 3; i++) {
        gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", hs[i]);
        GLYPHMETRICS gm; U(GetGlyphOutlineW)(fx.m.dc, 'M', GGO_METRICS, &gm, 0, NULL, &ID2);
        w[i] = gm.gmBlackBoxX;
        gap_fx_close(&fx);
    }
    CHECK(w[0] < w[1] && w[1] < w[2]);
    SOFT((int)w[2] >= (int)w[1] * 2 - 3 && (int)w[2] <= (int)w[1] * 2 + 3, "scales linearly");
}
TEST(gap0_ggo_courier_fixed_advance) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Courier New", -40);
    GLYPHMETRICS a, b, c;
    U(GetGlyphOutlineW)(fx.m.dc, 'i', GGO_METRICS, &a, 0, NULL, &ID2);
    U(GetGlyphOutlineW)(fx.m.dc, 'W', GGO_METRICS, &b, 0, NULL, &ID2);
    U(GetGlyphOutlineW)(fx.m.dc, ' ', GGO_METRICS, &c, 0, NULL, &ID2);
    EXPECT_EQ(a.gmCellIncX, b.gmCellIncX);
    EXPECT_EQ(a.gmCellIncX, c.gmCellIncX);
    gap_fx_close(&fx);
}
TEST(gap0_ggo_world_transform_effect) {
    REQUIRE(U(GetGlyphOutlineW)); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -50);
    GLYPHMETRICS g1, g2;
    U(GetGlyphOutlineW)(fx.m.dc, 'H', GGO_METRICS, &g1, 0, NULL, &ID2);
    U(SetGraphicsMode)(fx.m.dc, GM_ADVANCED);
    XFORM x = { 2, 0, 0, 2, 0, 0 };
    U(SetWorldTransform)(fx.m.dc, &x);
    DWORD r = U(GetGlyphOutlineW)(fx.m.dc, 'H', GGO_METRICS, &g2, 0, NULL, &ID2);
    t_note("with world scale 2: ret %lu bb %u -> %u", r, g1.gmBlackBoxX, g2.gmBlackBoxX);
    gap_fx_close(&fx);
}
TEST(gap0_ggo_mapmode_effect) {
    REQUIRE(U(GetGlyphOutlineW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -50);
    GLYPHMETRICS g1, g2;
    U(GetGlyphOutlineW)(fx.m.dc, 'H', GGO_METRICS, &g1, 0, NULL, &ID2);
    U(SetMapMode)(fx.m.dc, MM_TEXT);
    U(GetGlyphOutlineW)(fx.m.dc, 'H', GGO_METRICS, &g2, 0, NULL, &ID2);
    CHECK(!memcmp(&g1, &g2, sizeof g1));
    gap_fx_close(&fx);
}
