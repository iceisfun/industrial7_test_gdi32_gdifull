/* GetKerningPairs A/W. */
#include "gc.h"

#define NEED_K() do { REQUIRE(U(GetKerningPairsW)); REQUIRE(U(GetKerningPairsA)); GC_REQ_BASE(); REQUIRE(U(GetFontData)); REQUIRE(U(GetGlyphIndicesW)); } while (0)
static KERNINGPAIR kp[0x10000 / 4 * 4 + 64];
static KERNINGPAIR kp2[sizeof kp / sizeof kp[0]];

static int kfind(const KERNINGPAIR *p, DWORD n, WORD a, WORD b, int *val)
{
    DWORD i; for (i = 0; i < n; i++) if (p[i].wFirst == a && p[i].wSecond == b) { *val = p[i].iKernAmount; return 1; }
    return 0;
}

TEST(kern_count_query_text_fonts)
{
    static const struct { const char *face; int expect_pairs; } fs[] = { {"Times New Roman", 1}, {"Arial", 0}, {"Tahoma", 0}, {"Segoe UI", -1}, {"Courier New", -1}, {"Consolas", -1} };
    GC g; unsigned i; NEED_K();
    for (i = 0; i < 6; i++) {
        DWORD w, a;
        if (!gc_open(&g, fs[i].face, -16)) continue;
        w = U(GetKerningPairsW)(g.dc, 0, NULL); a = U(GetKerningPairsA)(g.dc, 0, NULL);
        CHECKF(w != GDI_ERROR, "%s W", fs[i].face);
        if (fs[i].expect_pairs > 0) { if (gc_wine()) t_note("%s: W=%lu A=%lu", fs[i].face, w, a); else CHECKF(w > 0 && a > 0, "%s: W=%lu A=%lu", fs[i].face, w, a); }
        else t_note("%s kerning pairs: W=%lu A=%lu", fs[i].face, w, a);
        if (fs[i].expect_pairs == 0 && w == 0) t_note("%s has no kerning pairs on this system", fs[i].face);
        CHECKF(a <= w, "%s: A count %lu > W count %lu", fs[i].face, a, w);
        gc_close(&g);
    }
}

TEST(kern_count_independent_of_size)
{
    static const int hs[] = { -8, -12, -24, -72 }; GC g; unsigned i; DWORD first = 0; NEED_K();
    for (i = 0; i < 4; i++) {
        DWORD n;
        if (!gc_open(&g, "Times New Roman", hs[i])) continue;
        n = U(GetKerningPairsW)(g.dc, 0, NULL);
        if (!first) first = n; else CHECKF(n == first, "size %d count %lu vs %lu", hs[i], n, first);
        gc_close(&g);
    }
}

TEST(kern_zero_npairs_with_buffer_is_error)
{
    GC g; KERNINGPAIR one; DWORD r, e; NEED_K();
    if (!gc_open(&g, "Times New Roman", -16)) return;
    SetLastError(GC_ERRSENT); r = U(GetKerningPairsW)(g.dc, 0, &one); e = GetLastError();
    EXPECT_EQ(r, 0); EXPECT_EQ(e, ERROR_INVALID_PARAMETER);
    SetLastError(GC_ERRSENT); r = U(GetKerningPairsA)(g.dc, 0, &one); e = GetLastError();
    EXPECT_EQ(r, 0); EXPECT_EQ(e, ERROR_INVALID_PARAMETER);
    gc_close(&g);
}

TEST(kern_zero_npairs_with_buffer_ignores_dc)
{
    KERNINGPAIR one; DWORD r, e; NEED_K();
    SetLastError(GC_ERRSENT); r = U(GetKerningPairsW)(NULL, 0, &one); e = GetLastError();
    EXPECT_EQ(r, 0); EXPECT_EQ(e, ERROR_INVALID_PARAMETER);
    SetLastError(GC_ERRSENT); r = U(GetKerningPairsA)(NULL, 0, &one); e = GetLastError();
    EXPECT_EQ(r, 0); EXPECT_EQ(e, ERROR_INVALID_PARAMETER);
}

TEST(kern_invalid_dc_returns_zero)
{
    KERNINGPAIR one[4]; HDC dc; HGDIOBJ pen; NEED_K(); REQUIRE(U(CreatePen));
    EXPECT_EQ(U(GetKerningPairsW)(NULL, 0, NULL), 0);
    EXPECT_EQ(U(GetKerningPairsA)(NULL, 0, NULL), 0);
    EXPECT_EQ(U(GetKerningPairsW)(NULL, 4, one), 0);
    EXPECT_EQ(U(GetKerningPairsA)(NULL, 4, one), 0);
    EXPECT_EQ(U(GetKerningPairsW)((HDC)(ULONG_PTR)0x12345, 0, NULL), 0);
    EXPECT_EQ(U(GetKerningPairsA)((HDC)(ULONG_PTR)0x12345, 0, NULL), 0);
    dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    EXPECT_EQ(U(GetKerningPairsW)(dc, 0, NULL), 0);
    EXPECT_EQ(U(GetKerningPairsA)(dc, 0, NULL), 0);
    pen = U(CreatePen)(PS_SOLID, 1, 0);
    EXPECT_EQ(U(GetKerningPairsW)((HDC)pen, 0, NULL), 0);
    EXPECT_EQ(U(GetKerningPairsA)((HDC)pen, 0, NULL), 0);
    U(DeleteObject)(pen);
}

TEST(kern_full_fetch_matches_count)
{
    GC g; DWORD n, r; NEED_K();
    if (!gc_open(&g, "Times New Roman", -16)) return;
    n = U(GetKerningPairsW)(g.dc, 0, NULL);
    if (n == 0 || n > sizeof kp / sizeof kp[0] - 8) { t_note("count %lu", n); gc_close(&g); return; }
    memset(kp, 0xCD, sizeof kp);
    r = U(GetKerningPairsW)(g.dc, n, kp);
    CHECKF(r == n, "fetch returned %lu want %lu", r, n);
    CHECK(kp[n].wFirst == 0xCDCD);
    r = U(GetKerningPairsW)(g.dc, n + 50, kp);
    CHECKF(r == n, "oversize buffer returned %lu want %lu", r, n);
    gc_close(&g);
}

TEST(kern_W_small_buffer_observed)
{
    GC g; DWORD n, r; NEED_K();
    if (!gc_open(&g, "Times New Roman", -16)) return;
    n = U(GetKerningPairsW)(g.dc, 0, NULL);
    if (n < 3) { gc_close(&g); return; }
    SetLastError(GC_ERRSENT); r = U(GetKerningPairsW)(g.dc, n - 1, kp);
    t_note("W with nPairs=count-1 -> %lu err %lu", r, GetLastError());
    SetLastError(GC_ERRSENT); r = U(GetKerningPairsW)(g.dc, 1, kp);
    t_note("W with nPairs=1 -> %lu err %lu", r, GetLastError());
    gc_close(&g);
}

TEST(kern_A_small_buffer_truncates)
{
    GC g; DWORD n, r; NEED_K();
    if (!gc_open(&g, "Times New Roman", -16)) return;
    n = U(GetKerningPairsA)(g.dc, 0, NULL);
    if (n < 4 || n > sizeof kp / sizeof kp[0]) { gc_close(&g); return; }
    /* decompiled: the A wrapper copies up to nPairs entries and stops */
    r = U(GetKerningPairsA)(g.dc, n - 1, kp); CHECKF(r == n - 1, "n-1: %lu", r);
    r = U(GetKerningPairsA)(g.dc, 1, kp); CHECKF(r == 1, "1: %lu", r);
    r = U(GetKerningPairsA)(g.dc, n, kp); CHECKF(r == n, "exact: %lu", r);
    r = U(GetKerningPairsA)(g.dc, n + 7, kp); CHECKF(r == n, "oversize: %lu", r);
    gc_close(&g);
}

TEST(kern_pairs_agree_with_kern_table)
{
    static const char *const fs[] = { "Times New Roman", "Arial", "Tahoma" }; GC g; unsigned f; NEED_K();
    for (f = 0; f < 3; f++) {
        DWORD n, i, kl = 0, cl = 0; BYTE *kern, *cmap; int upem, bad = 0, checked = 0;
        if (!gc_open(&g, fs[f], -16)) continue;
        upem = gc_upem(g.dc); gc_close(&g);
        if (upem <= 0 || !gc_open(&g, fs[f], -upem)) continue;
        kern = gc_table(g.dc, "kern", &kl); cmap = gc_table(g.dc, "cmap", &cl);
        n = U(GetKerningPairsW)(g.dc, 0, NULL);
        if (!kern || !cmap || n == 0 || n > sizeof kp / sizeof kp[0]) { t_note("%s: no kern table / pairs (%lu)", fs[f], n); free(kern); free(cmap); gc_close(&g); continue; }
        CHECK(U(GetKerningPairsW)(g.dc, n, kp) == n);
        for (i = 0; i < n; i++) {
            int tv = 0; unsigned ga = gc_cmap(cmap, cl, kp[i].wFirst), gb = gc_cmap(cmap, cl, kp[i].wSecond);
            if (!ga || !gb) continue;
            checked++;
            if (!gc_kern_lookup(kern, kl, ga, gb, &tv) || tv != kp[i].iKernAmount) {
                if (bad++ < 3) t_note("%s pair %04x,%04x amount %d, table %d", fs[f], kp[i].wFirst, kp[i].wSecond, kp[i].iKernAmount, tv);
            }
        }
        CHECKF(bad == 0, "%s: %d of %d pairs disagree with the kern table at em size", fs[f], bad, checked);
        CHECK(checked > 100);
        free(kern); free(cmap); gc_close(&g);
    }
}

TEST(kern_AV_is_negative_in_latin_fonts)
{
    static const char *const fs[] = { "Times New Roman", "Arial", "Tahoma", "Segoe UI" }; GC g; unsigned f; NEED_K();
    for (f = 0; f < 4; f++) {
        DWORD n; int v;
        if (!gc_open(&g, fs[f], -16)) continue;
        n = U(GetKerningPairsW)(g.dc, 0, NULL);
        if (n == 0 || n > sizeof kp / sizeof kp[0]) { t_note("%s: %lu pairs", fs[f], n); CHECKF(f != 0 || gc_wine(), "Times New Roman has no kerning pairs"); gc_close(&g); continue; }
        U(GetKerningPairsW)(g.dc, n, kp);
        if (!kfind(kp, n, 'A', 'V', &v)) { CHECKF(0, "%s: no AV pair", fs[f]); gc_close(&g); continue; }
        CHECKF(v < 0, "%s AV = %d", fs[f], v);
        if (kfind(kp, n, 'V', 'A', &v)) CHECKF(v < 0, "%s VA = %d", fs[f], v); else t_note("%s has no VA pair", fs[f]);
        { static const char pr[][2] = { {'A','T'}, {'T','A'}, {'A','Y'}, {'Y','A'}, {'L','T'}, {'T','o'}, {'P','.'}, {'F',','} };
          int k; for (k = 0; k < 8; k++) if (kfind(kp, n, pr[k][0], pr[k][1], &v)) { if (v >= 0) t_note("%s %c%c = %d", fs[f], pr[k][0], pr[k][1], v); } else t_note("%s has no %c%c pair", fs[f], pr[k][0], pr[k][1]); }
        gc_close(&g);
    }
}

TEST(kern_amount_scales_with_size)
{
    GC a, b; DWORD na, nb; int va = 0, vb = 0; NEED_K();
    if (!gc_open(&a, "Times New Roman", -16)) return;
    if (!gc_open(&b, "Times New Roman", -48)) { gc_close(&a); return; }
    na = U(GetKerningPairsW)(a.dc, 0, NULL); nb = U(GetKerningPairsW)(b.dc, 0, NULL);
    if (na && na == nb && na <= sizeof kp / sizeof kp[0]) {
        U(GetKerningPairsW)(a.dc, na, kp); U(GetKerningPairsW)(b.dc, nb, kp2);
        CHECK(kfind(kp, na, 'A', 'V', &va) && kfind(kp2, nb, 'A', 'V', &vb));
        CHECKF(abs(vb - 3 * va) <= 3 && va < 0, "AV: 16px %d, 48px %d", va, vb);
        CHECKF(!memcmp(kp, kp2, 0) && kp[0].wFirst == kp2[0].wFirst && kp[0].wSecond == kp2[0].wSecond, "pair order differs between sizes");
    }
    gc_close(&a); gc_close(&b);
}

TEST(kern_A_pairs_are_subset_of_W_pairs)
{
    GC g; DWORD nw, na, i; NEED_K();
    if (!gc_open(&g, "Times New Roman", -20)) return;
    nw = U(GetKerningPairsW)(g.dc, 0, NULL); na = U(GetKerningPairsA)(g.dc, 0, NULL);
    if (!nw || nw > sizeof kp / sizeof kp[0] || na > sizeof kp2 / sizeof kp2[0]) { gc_close(&g); return; }
    CHECK(U(GetKerningPairsW)(g.dc, nw, kp) == nw); CHECK(U(GetKerningPairsA)(g.dc, na, kp2) == na);
    for (i = 0; i < na; i++) {
        int v; CHECKF(kp2[i].wFirst < 256 && kp2[i].wSecond < 256, "A pair %lu has %x,%x", i, kp2[i].wFirst, kp2[i].wSecond);
        if (!kfind(kp, nw, kp2[i].wFirst, kp2[i].wSecond, &v)) { CHECKF(kp2[i].wFirst >= 0x80 || kp2[i].wSecond >= 0x80, "A pair %c%c not in W list", kp2[i].wFirst, kp2[i].wSecond); }
        else CHECKF(v == kp2[i].iKernAmount, "A pair %x,%x amount %d vs %d", kp2[i].wFirst, kp2[i].wSecond, kp2[i].iKernAmount, v);
    }
    gc_close(&g);
}

TEST(kern_pair_list_order_and_uniqueness_observed)
{
    GC g; DWORD n, i, unsorted = 0, dups = 0; NEED_K();
    if (!gc_open(&g, "Arial", -16)) return;
    n = U(GetKerningPairsW)(g.dc, 0, NULL);
    if (!n || n > sizeof kp / sizeof kp[0]) { gc_close(&g); return; }
    U(GetKerningPairsW)(g.dc, n, kp);
    for (i = 1; i < n; i++) {
        unsigned long long a = ((unsigned long long)kp[i - 1].wFirst << 16) | kp[i - 1].wSecond, b = ((unsigned long long)kp[i].wFirst << 16) | kp[i].wSecond;
        if (b < a) unsorted++; else if (b == a) dups++;
    }
    t_note("Arial %lu pairs: %lu out of order, %lu duplicates", n, unsorted, dups);
    gc_close(&g);
}

TEST(kern_no_zero_char_pairs)
{
    GC g; DWORD n, i; NEED_K();
    if (!gc_open(&g, "Times New Roman", -16)) return;
    n = U(GetKerningPairsW)(g.dc, 0, NULL);
    if (!n || n > sizeof kp / sizeof kp[0]) { gc_close(&g); return; }
    U(GetKerningPairsW)(g.dc, n, kp);
    for (i = 0; i < n; i++) CHECKF(kp[i].wFirst != 0 && kp[i].wSecond != 0, "pair %lu is %x,%x", i, kp[i].wFirst, kp[i].wSecond);
    gc_close(&g);
}

TEST(kern_amounts_are_small_relative_to_em)
{
    GC g; DWORD n, i; NEED_K();
    if (!gc_open(&g, "Times New Roman", -100)) return;
    n = U(GetKerningPairsW)(g.dc, 0, NULL);
    if (!n || n > sizeof kp / sizeof kp[0]) { gc_close(&g); return; }
    U(GetKerningPairsW)(g.dc, n, kp);
    for (i = 0; i < n; i++) CHECKF(abs(kp[i].iKernAmount) < 100, "pair %x,%x = %d", kp[i].wFirst, kp[i].wSecond, kp[i].iKernAmount);
    gc_close(&g);
}

TEST(kern_repeat_calls_identical)
{
    GC g; DWORD n; int i; NEED_K();
    if (!gc_open(&g, "Arial", -16)) return;
    n = U(GetKerningPairsW)(g.dc, 0, NULL);
    if (!n || n > sizeof kp / sizeof kp[0]) { gc_close(&g); return; }
    U(GetKerningPairsW)(g.dc, n, kp);
    for (i = 0; i < 5; i++) { CHECK(U(GetKerningPairsW)(g.dc, n, kp2) == n); CHECK(!memcmp(kp, kp2, n * sizeof kp[0])); }
    gc_close(&g);
}

TEST(kern_bold_italic_variants_observed)
{
    GC g; LOGFONTA lf; HFONT f; unsigned v; NEED_K();
    if (!gc_open(&g, "Times New Roman", -20)) return;
    for (v = 0; v < 3; v++) {
        DWORD n;
        memset(&lf, 0, sizeof lf); lf.lfHeight = -20; lf.lfWeight = v == 1 ? FW_BOLD : FW_NORMAL; lf.lfItalic = v == 2; lstrcpyA(lf.lfFaceName, "Times New Roman");
        f = U(CreateFontIndirectA)(&lf); CHECK(gc_setfont(&g, f));
        n = U(GetKerningPairsW)(g.dc, 0, NULL);
        CHECKF((n > 0 && n != GDI_ERROR) || gc_wine(), "variant %u: %lu", v, n);
    }
    gc_close(&g);
}

TEST(kern_symbolic_and_raster_fonts_observed)
{
    GC g; DWORD n; NEED_K();
    if (gc_open(&g, "Symbol", -16)) { n = U(GetKerningPairsW)(g.dc, 0, NULL); t_note("Symbol pairs: %lu", n); gc_close(&g); }
    if (gc_open(&g, "Marlett", -16)) { n = U(GetKerningPairsW)(g.dc, 0, NULL); t_note("Marlett pairs: %lu", n); gc_close(&g); }
    if (gc_open_ex(&g, "Terminal", -12, 400, OEM_CHARSET, 0)) { n = U(GetKerningPairsW)(g.dc, 0, NULL); t_note("Terminal pairs: %lu", n); gc_close(&g); }
}

TEST(kern_default_dc_font_observed)
{
    HDC dc; DWORD n; NEED_K();
    dc = U(CreateCompatibleDC)(NULL);
    n = U(GetKerningPairsW)(dc, 0, NULL);
    t_note("default font pairs: %lu", n);
    U(DeleteDC)(dc);
}

TEST(kern_characterplacement_usekerning_observed)
{
    GC g; DWORD n, r0, r1; int v = 0; GCP_RESULTSW res; int dx[4]; WCHAR out[4]; static const WCHAR s[] = L"AV"; NEED_K(); REQUIRE(U(GetCharacterPlacementW));
    if (!gc_open(&g, "Times New Roman", -48)) return;
    n = U(GetKerningPairsW)(g.dc, 0, NULL);
    if (!n || n > sizeof kp / sizeof kp[0]) { gc_close(&g); return; }
    U(GetKerningPairsW)(g.dc, n, kp);
    kfind(kp, n, 'A', 'V', &v);
    memset(&res, 0, sizeof res); res.lStructSize = sizeof res; res.lpOutString = out; res.lpDx = dx; res.nGlyphs = 4;
    r0 = U(GetCharacterPlacementW)(g.dc, s, 2, 0, &res, 0);
    memset(&res, 0, sizeof res); res.lStructSize = sizeof res; res.lpOutString = out; res.lpDx = dx; res.nGlyphs = 4;
    r1 = U(GetCharacterPlacementW)(g.dc, s, 2, 0, &res, GCP_USEKERNING);
    t_note("AV extent plain %lu, kerned %lu, pair amount %d", LOWORD(r0), LOWORD(r1), v);
    CHECK(r0 != 0 && r1 != 0);
    gc_close(&g);
}

TEST(kern_no_gdi_leak)
{
    GC g; int before, after, i; NEED_K();
    if (!gc_open(&g, "Times New Roman", -16)) return;
    before = gc_gdicount();
    for (i = 0; i < 300; i++) { U(GetKerningPairsW)(g.dc, 0, NULL); U(GetKerningPairsA)(g.dc, 0, NULL); }
    after = gc_gdicount();
    if (before >= 0) CHECKF(before == after, "%d -> %d", before, after);
    gc_close(&g);
}
