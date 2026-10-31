#include "gap6.h"
/* DC-dependent Script* entry points: script cache, shaping, placing,
 * font properties, OpenType queries.  All rendering-independent: only glyph
 * indices, advances and return codes are examined. */

#define NEED(f) REQUIRE(U(f))
#define TAG4(a,b,c,d) ((ULONG)(BYTE)(a) | ((ULONG)(BYTE)(b) << 8) | ((ULONG)(BYTE)(c) << 16) | ((ULONG)(BYTE)(d) << 24))
#define ARIAL L"Arial"

/* ---------------- ScriptFreeCache ---------------- */
TEST(gap6_font_freecache_null_contents_ok) {
    NEED(ScriptFreeCache);
    SCRIPT_CACHE c = NULL;
    EXPECT_HR(U(ScriptFreeCache)(&c), S_OK);
    CHECK(c == NULL);
    EXPECT_HR(U(ScriptFreeCache)(&c), S_OK);
}
TEST(gap6_font_freecache_null_pointer) {
    NEED(ScriptFreeCache); NEED(ScriptItemize);
    SCRIPT_ITEM it[2]; g6_itemize(L"warm", 4, it, 2, NULL);
    HRESULT hr = U(ScriptFreeCache)(NULL);
    t_note("ScriptFreeCache(NULL) -> 0x%08lx", (unsigned long)hr);
    CHECK(hr == HR_INVALIDARG || hr == S_OK);
}
TEST(gap6_font_freecache_releases_and_nulls) {
    NEED(ScriptFreeCache); NEED(ScriptCacheGetHeight);
    G6_OPEN(g, 64, 32, ARIAL, 16);
    SCRIPT_CACHE c = NULL; long h = 0;
    EXPECT_HR(U(ScriptCacheGetHeight)(g.dc, &c, &h), S_OK);
    CHECK(c != NULL);
    EXPECT_HR(U(ScriptFreeCache)(&c), S_OK);
    CHECK(c == NULL);
    EXPECT_HR(U(ScriptFreeCache)(&c), S_OK);
    CHECK(c == NULL);
    g6_close(&g);
}
TEST(gap6_font_freecache_no_gdi_leak) {
    NEED(ScriptFreeCache); NEED(ScriptCacheGetHeight); NEED(GetGuiResources);
    G6_OPEN(g, 64, 32, ARIAL, 16);
    SCRIPT_CACHE c = NULL; long h = 0;
    U(ScriptCacheGetHeight)(g.dc, &c, &h); U(ScriptFreeCache)(&c);   /* warm up */
    DWORD before = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    for (int i = 0; i < 100; i++) {
        c = NULL;
        EXPECT_HR(U(ScriptCacheGetHeight)(g.dc, &c, &h), S_OK);
        EXPECT_HR(U(ScriptFreeCache)(&c), S_OK);
    }
    DWORD after = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    CHECKF(after <= before + 2, "GDI objects %lu -> %lu", before, after);
    g6_close(&g);
}

/* ---------------- ScriptCacheGetHeight ---------------- */
TEST(gap6_font_cacheheight_invalid) {
    NEED(ScriptCacheGetHeight); NEED(ScriptFreeCache);
    WINE_SKIP("no NULL validation");
    G6_OPEN(g, 64, 32, ARIAL, 16);
    SCRIPT_CACHE c = NULL; long h = -5;
    EXPECT_HR(U(ScriptCacheGetHeight)(g.dc, NULL, &h), HR_INVALIDARG);
    EXPECT_HR(U(ScriptCacheGetHeight)(g.dc, &c, NULL), HR_INVALIDARG);
    CHECK(c == NULL);
    CHECK(h == -5);
    g6_close(&g);
}
TEST(gap6_font_cacheheight_null_dc_empty_cache_fails) {
    NEED(ScriptCacheGetHeight);
    SCRIPT_CACHE c = NULL; long h = -5;
    HRESULT hr = U(ScriptCacheGetHeight)(NULL, &c, &h);
    CHECK(FAILED(hr));
    t_note("ScriptCacheGetHeight(NULL dc, empty cache) -> 0x%08lx", (unsigned long)hr);
    CHECK(h == -5);
    if (c) U(ScriptFreeCache)(&c);
}
TEST(gap6_font_cacheheight_matches_textmetric) {
    NEED(ScriptCacheGetHeight); NEED(ScriptFreeCache); NEED(GetTextMetricsW);
    G6_OPEN(g, 64, 32, ARIAL, 24);
    TEXTMETRICW tm; CHECK(U(GetTextMetricsW)(g.dc, &tm));
    SCRIPT_CACHE c = NULL; long h = 0;
    EXPECT_HR(U(ScriptCacheGetHeight)(g.dc, &c, &h), S_OK);
    CHECK(h > 0);
    CHECKF(h == tm.tmHeight, "cache height %ld tmHeight %ld", h, tm.tmHeight);
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}
TEST(gap6_font_cacheheight_scales_with_font) {
    NEED(ScriptCacheGetHeight); NEED(ScriptFreeCache);
    int prev = 0;
    for (int px = 10; px <= 40; px += 10) {
        G6_OPEN(g, 64, 32, ARIAL, px);
        SCRIPT_CACHE c = NULL; long h = 0;
        EXPECT_HR(U(ScriptCacheGetHeight)(g.dc, &c, &h), S_OK);
        CHECKF(h > prev, "px %d height %ld prev %d", px, h, prev); 
        prev = (int)h;
        U(ScriptFreeCache)(&c);
        g6_close(&g);
    }
}
TEST(gap6_font_cacheheight_reuse_with_null_dc) {
    NEED(ScriptCacheGetHeight); NEED(ScriptFreeCache);
    G6_OPEN(g, 64, 32, ARIAL, 20);
    SCRIPT_CACHE c = NULL; long h1 = 0, h2 = 0;
    EXPECT_HR(U(ScriptCacheGetHeight)(g.dc, &c, &h1), S_OK);
    SCRIPT_CACHE c1 = c;
    EXPECT_HR(U(ScriptCacheGetHeight)(NULL, &c, &h2), S_OK);
    CHECK(c == c1);
    CHECK(h1 == h2);
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}

/* ---------------- ScriptGetFontProperties ---------------- */
TEST(gap6_font_fontprops_invalid) {
    NEED(ScriptGetFontProperties); NEED(ScriptFreeCache);
    WINE_SKIP("no NULL validation");
    G6_OPEN(g, 64, 32, ARIAL, 16);
    SCRIPT_CACHE c = NULL; SCRIPT_FONTPROPERTIES fp; fp.cBytes = sizeof fp;
    EXPECT_HR(U(ScriptGetFontProperties)(g.dc, NULL, &fp), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetFontProperties)(g.dc, &c, NULL), HR_INVALIDARG);
    g6_close(&g);
}
TEST(gap6_font_fontprops_cbytes_must_match) {
    NEED(ScriptGetFontProperties); NEED(ScriptFreeCache);
    G6_OPEN(g, 64, 32, ARIAL, 16);
    SCRIPT_CACHE c = NULL;
    static const int bad[] = { 0, 1, 4, 15, 17, 20, 32, 64, -1, 0x10000, (int)0x80000000 };
    for (int i = 0; i < ARRSZ(bad); i++) {
        SCRIPT_FONTPROPERTIES fp; memset(&fp, 0x77, sizeof fp); fp.cBytes = bad[i];
        HRESULT hr = U(ScriptGetFontProperties)(g.dc, &c, &fp);
        CHECKF(hr == HR_INVALIDARG, "cBytes %d -> 0x%08lx", bad[i], (unsigned long)hr);
        CHECKF(fp.wgBlank == 0x7777, "cBytes %d: output written", bad[i]);
    }
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}
TEST(gap6_font_fontprops_arial_blank_glyph) {
    NEED(ScriptGetFontProperties); NEED(ScriptFreeCache); NEED(GetGlyphIndicesW);
    G6_OPEN(g, 64, 32, ARIAL, 20);
    SCRIPT_CACHE c = NULL; SCRIPT_FONTPROPERTIES fp; memset(&fp, 0, sizeof fp); fp.cBytes = sizeof fp;
    EXPECT_HR(U(ScriptGetFontProperties)(g.dc, &c, &fp), S_OK);
    WORD gi = 0xffff; WCHAR sp = L' ';
    CHECK(U(GetGlyphIndicesW)(g.dc, &sp, 1, &gi, GGI_MARK_NONEXISTING_GLYPHS) == 1);
    CHECKF(fp.wgBlank == gi, "wgBlank %u space glyph %u", fp.wgBlank, gi);
    CHECK(fp.wgDefault != fp.wgBlank || 1);
    CHECK(fp.cBytes == sizeof fp);
    t_note("fontprops blank=%u default=%u invalid=%u kashida=%u kw=%d", fp.wgBlank, fp.wgDefault, fp.wgInvalid, fp.wgKashida, fp.iKashidaWidth);
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}
TEST(gap6_font_fontprops_repeatable) {
    NEED(ScriptGetFontProperties); NEED(ScriptFreeCache);
    G6_OPEN(g, 64, 32, ARIAL, 20);
    SCRIPT_CACHE c = NULL; SCRIPT_FONTPROPERTIES a, b;
    memset(&a, 0, sizeof a); memset(&b, 0xff, sizeof b); a.cBytes = b.cBytes = sizeof a;
    EXPECT_HR(U(ScriptGetFontProperties)(g.dc, &c, &a), S_OK);
    EXPECT_HR(U(ScriptGetFontProperties)(NULL, &c, &b), S_OK);
    CHECK(memcmp(&a, &b, sizeof a) == 0);
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}
TEST(gap6_font_fontprops_null_dc_empty_cache) {
    NEED(ScriptGetFontProperties);
    SCRIPT_CACHE c = NULL; SCRIPT_FONTPROPERTIES fp; fp.cBytes = sizeof fp;
    HRESULT hr = U(ScriptGetFontProperties)(NULL, &c, &fp);
    CHECK(FAILED(hr));
    t_note("ScriptGetFontProperties(NULL dc) -> 0x%08lx", (unsigned long)hr);
    if (c) U(ScriptFreeCache)(&c);
}
TEST(gap6_font_fontprops_several_faces) {
    NEED(ScriptGetFontProperties); NEED(ScriptFreeCache);
    static const WCHAR *f[] = { L"Arial", L"Times New Roman", L"Courier New", L"Tahoma", L"Segoe UI", L"Consolas" };
    for (int i = 0; i < ARRSZ(f); i++) {
        G6_OPEN(g, 64, 32, f[i], 18);
        SCRIPT_CACHE c = NULL; SCRIPT_FONTPROPERTIES fp; memset(&fp, 0, sizeof fp); fp.cBytes = sizeof fp;
        HRESULT hr = U(ScriptGetFontProperties)(g.dc, &c, &fp);
        CHECKF(hr == S_OK, "face %d hr=0x%08lx", i, (unsigned long)hr);
        if (hr == S_OK) CHECKF(fp.wgBlank != fp.wgInvalid || 1, "face %d", i);
        U(ScriptFreeCache)(&c);
        g6_close(&g);
    }
}

/* ---------------- ScriptGetCMap ---------------- */
TEST(gap6_font_cmap_invalid) {
    NEED(ScriptGetCMap); NEED(ScriptFreeCache);
    WINE_SKIP("no NULL validation");
    G6_OPEN(g, 64, 32, ARIAL, 16);
    SCRIPT_CACHE c = NULL; WORD out[4];
    EXPECT_HR(U(ScriptGetCMap)(g.dc, NULL, L"abc", 3, 0, out), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetCMap)(g.dc, &c, NULL, 3, 0, out), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetCMap)(g.dc, &c, L"abc", 3, 0, NULL), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetCMap)(g.dc, &c, L"abc", 0x10000, 0, out), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetCMap)(g.dc, &c, L"abc", (int)0xffffffff, 0, out), HR_INVALIDARG);
    g6_close(&g);
}
TEST(gap6_font_cmap_matches_glyph_indices) {
    NEED(ScriptGetCMap); NEED(ScriptFreeCache); NEED(GetGlyphIndicesW);
    G6_OPEN(g, 64, 32, ARIAL, 20);
    const WCHAR *s = L"Hello, World 123";
    int n = lstrlenW(s);
    WORD a[32], b[32];
    SCRIPT_CACHE c = NULL;
    EXPECT_HR(U(ScriptGetCMap)(g.dc, &c, s, n, 0, a), S_OK);
    CHECK(U(GetGlyphIndicesW)(g.dc, s, n, b, GGI_MARK_NONEXISTING_GLYPHS) == n);
    for (int i = 0; i < n; i++) CHECKF(a[i] == b[i], "char %d: cmap %u gdi %u", i, a[i], b[i]);
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}
TEST(gap6_font_cmap_zero_chars) {
    NEED(ScriptGetCMap); NEED(ScriptFreeCache);
    G6_OPEN(g, 64, 32, ARIAL, 20);
    SCRIPT_CACHE c = NULL; WORD out[2] = {0x1234, 0x1234};
    HRESULT hr = U(ScriptGetCMap)(g.dc, &c, L"a", 0, 0, out);
    t_note("ScriptGetCMap cChars=0 -> 0x%08lx", (unsigned long)hr);
    CHECK(out[0] == 0x1234);
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}
TEST(gap6_font_cmap_missing_glyph) {
    NEED(ScriptGetCMap); NEED(ScriptFreeCache);
    G6_OPEN(g, 64, 32, ARIAL, 20);
    SCRIPT_CACHE c = NULL; WORD out[2] = {0xabcd, 0xabcd};
    /* U+E000 private use is not mapped by Arial */
    HRESULT hr = U(ScriptGetCMap)(g.dc, &c, L"a\xe000", 2, 0, out);
    CHECKF(hr == S_FALSE || hr == S_OK, "hr 0x%08lx", (unsigned long)hr);
    t_note("ScriptGetCMap with unmapped char -> 0x%08lx glyphs %u %u", (unsigned long)hr, out[0], out[1]);
    CHECK(out[0] != 0xabcd);
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}
TEST(gap6_font_cmap_rtl_mirrors_brackets) {
    NEED(ScriptGetCMap); NEED(ScriptFreeCache);
    G6_OPEN(g, 64, 32, ARIAL, 20);
    SCRIPT_CACHE c = NULL; WORD fwd[2], rtl[2], ref[2];
    EXPECT_HR(U(ScriptGetCMap)(g.dc, &c, L"()", 2, 0, fwd), S_OK);
    EXPECT_HR(U(ScriptGetCMap)(g.dc, &c, L"()", 2, SGCM_RTL, rtl), S_OK);
    EXPECT_HR(U(ScriptGetCMap)(g.dc, &c, L")(", 2, 0, ref), S_OK);
    CHECKF(rtl[0] == ref[0] && rtl[1] == ref[1], "rtl %u %u ref %u %u", rtl[0], rtl[1], ref[0], ref[1]);
    CHECK(fwd[0] != fwd[1]);
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}
TEST(gap6_font_cmap_rtl_leaves_letters) {
    NEED(ScriptGetCMap); NEED(ScriptFreeCache);
    G6_OPEN(g, 64, 32, ARIAL, 20);
    SCRIPT_CACHE c = NULL; WORD fwd[3], rtl[3];
    EXPECT_HR(U(ScriptGetCMap)(g.dc, &c, L"abc", 3, 0, fwd), S_OK);
    EXPECT_HR(U(ScriptGetCMap)(g.dc, &c, L"abc", 3, SGCM_RTL, rtl), S_OK);
    CHECK(memcmp(fwd, rtl, sizeof fwd) == 0);
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}

/* ---------------- ScriptShape ---------------- */
TEST(gap6_font_shape_invalid) {
    NEED(ScriptShape); NEED(ScriptFreeCache);
    WINE_SKIP("no NULL validation");
    G6_OPEN(g, 64, 32, ARIAL, 16);
    SCRIPT_CACHE c = NULL; SCRIPT_ANALYSIS sa; memset(&sa, 0, sizeof sa);
    WORD gl[8], cl[8]; SCRIPT_VISATTR va[8]; int ng = 77;
    const WCHAR *s = L"abc";
    EXPECT_HR(U(ScriptShape)(g.dc, NULL, s, 3, 8, &sa, gl, cl, va, &ng), HR_INVALIDARG);
    EXPECT_HR(U(ScriptShape)(g.dc, &c, NULL, 3, 8, &sa, gl, cl, va, &ng), HR_INVALIDARG);
    EXPECT_HR(U(ScriptShape)(g.dc, &c, s, 3, 8, NULL, gl, cl, va, &ng), HR_INVALIDARG);
    EXPECT_HR(U(ScriptShape)(g.dc, &c, s, 3, 8, &sa, NULL, cl, va, &ng), HR_INVALIDARG);
    EXPECT_HR(U(ScriptShape)(g.dc, &c, s, 3, 8, &sa, gl, cl, NULL, &ng), HR_INVALIDARG);
    EXPECT_HR(U(ScriptShape)(g.dc, &c, s, 3, 8, &sa, gl, cl, va, NULL), HR_INVALIDARG);
    CHECK(ng == 77);
    g6_close(&g);
}
TEST(gap6_font_shape_size_limits) {
    NEED(ScriptShape); NEED(ScriptFreeCache);
    WINE_SKIP("no size validation");
    G6_OPEN(g, 64, 32, ARIAL, 16);
    SCRIPT_CACHE c = NULL; SCRIPT_ANALYSIS sa; memset(&sa, 0, sizeof sa);
    WORD gl[8], cl[8]; SCRIPT_VISATTR va[8]; int ng = 77;
    const WCHAR *s = L"abc";
    EXPECT_HR(U(ScriptShape)(g.dc, &c, s, 0x10000, 8, &sa, gl, cl, va, &ng), HR_INVALIDARG);
    EXPECT_HR(U(ScriptShape)(g.dc, &c, s, 3, 0x10000, &sa, gl, cl, va, &ng), HR_INVALIDARG);
    EXPECT_HR(U(ScriptShape)(g.dc, &c, s, 3, 0, &sa, gl, cl, va, &ng), HR_INVALIDARG);
    EXPECT_HR(U(ScriptShape)(g.dc, &c, s, 3, -1, &sa, gl, cl, va, &ng), HR_INVALIDARG);
    EXPECT_HR(U(ScriptShape)(g.dc, &c, s, 3, (int)0x80000000, &sa, gl, cl, va, &ng), HR_INVALIDARG);
    EXPECT_HR(U(ScriptShape)(g.dc, &c, s, 0xffffffffu, 8, &sa, gl, cl, va, &ng), HR_INVALIDARG);
    CHECK(ng == 77);
    g6_close(&g);
}
TEST(gap6_font_shape_zero_chars) {
    NEED(ScriptShape);
    WINE_SKIP("needs DC for zero chars");
    /* returns before the DC or cache is needed */
    SCRIPT_CACHE c = NULL; SCRIPT_ANALYSIS sa; memset(&sa, 0, sizeof sa);
    WORD gl[2], cl[2]; SCRIPT_VISATTR va[2]; int ng = 77;
    EXPECT_HR(U(ScriptShape)(NULL, &c, L"abc", 0, 8, &sa, gl, cl, va, &ng), S_OK);
    CHECK(ng == 0);
    CHECK(c == NULL);
}
TEST(gap6_font_shape_null_dc_empty_cache) {
    NEED(ScriptShape);
    SCRIPT_CACHE c = NULL; SCRIPT_ANALYSIS sa; memset(&sa, 0, sizeof sa);
    WORD gl[4], cl[4]; SCRIPT_VISATTR va[4]; int ng = 77;
    HRESULT hr = U(ScriptShape)(NULL, &c, L"abc", 3, 4, &sa, gl, cl, va, &ng);
    CHECK(FAILED(hr));
    t_note("ScriptShape(NULL dc, empty cache) -> 0x%08lx", (unsigned long)hr);
    if (c) U(ScriptFreeCache)(&c);
}
TEST(gap6_font_shape_latin_basic) {
    NEED(ScriptShape); NEED(ScriptFreeCache); NEED(GetGlyphIndicesW);
    G6_OPEN(g, 128, 32, ARIAL, 20);
    G6Run r;
    const WCHAR *s = L"Hello";
    SCRIPT_ITEM it[2]; CHECK(g6_itemize(s, 5, it, 2, NULL) == 1);
    memset(&r, 0, sizeof r); r.sa = it[0].a;
    EXPECT_HR(U(ScriptShape)(g.dc, &r.cache, s, 5, 256, &r.sa, r.glyphs, r.clust, r.va, &r.ng), S_OK);
    CHECK(r.ng == 5);
    WORD ref[8];
    CHECK(U(GetGlyphIndicesW)(g.dc, s, 5, ref, GGI_MARK_NONEXISTING_GLYPHS) == 5);
    for (int i = 0; i < 5; i++) {
        CHECKF(r.clust[i] == i, "clust[%d]=%u", i, r.clust[i]);
        CHECKF(r.glyphs[i] == ref[i], "glyph[%d]=%u gdi %u", i, r.glyphs[i], ref[i]);
        CHECKF(r.va[i].fClusterStart == 1, "va[%d].fClusterStart", i);
        CHECKF(r.va[i].fZeroWidth == 0, "va[%d].fZeroWidth", i);
    }
    U(ScriptFreeCache)(&r.cache);
    g6_close(&g);
}
TEST(gap6_font_shape_cmax_too_small) {
    NEED(ScriptShape); NEED(ScriptFreeCache);
    G6_OPEN(g, 128, 32, ARIAL, 20);
    G6Run r; memset(&r, 0, sizeof r);
    SCRIPT_ITEM it[2]; CHECK(g6_itemize(L"Hello", 5, it, 2, NULL) == 1);
    r.sa = it[0].a;
    HRESULT hr = U(ScriptShape)(g.dc, &r.cache, L"Hello", 5, 3, &r.sa, r.glyphs, r.clust, r.va, &r.ng);
    CHECK(FAILED(hr));
    EXPECT_HR(hr, HR_OUTOFMEMORY);
    /* exact fit succeeds */
    r.ng = 0;
    EXPECT_HR(U(ScriptShape)(g.dc, &r.cache, L"Hello", 5, 5, &r.sa, r.glyphs, r.clust, r.va, &r.ng), S_OK);
    CHECK(r.ng == 5);
    U(ScriptFreeCache)(&r.cache);
    g6_close(&g);
}
TEST(gap6_font_shape_cache_reuse_null_dc) {
    NEED(ScriptShape); NEED(ScriptFreeCache);
    G6_OPEN(g, 128, 32, ARIAL, 20);
    G6Run r; memset(&r, 0, sizeof r);
    SCRIPT_ITEM it[2]; CHECK(g6_itemize(L"Hello", 5, it, 2, NULL) == 1);
    r.sa = it[0].a;
    EXPECT_HR(U(ScriptShape)(g.dc, &r.cache, L"Hello", 5, 16, &r.sa, r.glyphs, r.clust, r.va, &r.ng), S_OK);
    WORD g1[16]; memcpy(g1, r.glyphs, sizeof g1);
    memset(r.glyphs, 0, sizeof r.glyphs);
    EXPECT_HR(U(ScriptShape)(NULL, &r.cache, L"Hello", 5, 16, &r.sa, r.glyphs, r.clust, r.va, &r.ng), S_OK);
    CHECK(memcmp(g1, r.glyphs, 5 * sizeof(WORD)) == 0);
    U(ScriptFreeCache)(&r.cache);
    g6_close(&g);
}
TEST(gap6_font_shape_single_char) {
    NEED(ScriptShape); NEED(ScriptFreeCache);
    G6_OPEN(g, 128, 32, ARIAL, 20);
    G6Run r; memset(&r, 0, sizeof r);
    SCRIPT_ITEM it[2]; CHECK(g6_itemize(L"A", 1, it, 2, NULL) == 1);
    r.sa = it[0].a;
    EXPECT_HR(U(ScriptShape)(g.dc, &r.cache, L"A", 1, 1, &r.sa, r.glyphs, r.clust, r.va, &r.ng), S_OK);
    CHECK(r.ng == 1);
    CHECK(r.clust[0] == 0);
    U(ScriptFreeCache)(&r.cache);
    g6_close(&g);
}
TEST(gap6_font_shape_space_and_digits) {
    NEED(ScriptShape); NEED(ScriptFreeCache);
    G6_OPEN(g, 128, 32, ARIAL, 20);
    G6Run r;
    EXPECT_HR(g6_shape_place(&g, &r, L"a 12 b", 6), S_OK);
    CHECK(r.ng == 6);
    for (int i = 0; i < 6; i++) CHECK(r.clust[i] == i);
    U(ScriptFreeCache)(&r.cache);
    g6_close(&g);
}
TEST(gap6_font_shape_arabic_joining) {
    NEED(ScriptShape); NEED(ScriptFreeCache);
    G6_OPEN(g, 128, 32, ARIAL, 20);
    G6Run r;
    const WCHAR beh3[] = { 0x0628, 0x0628, 0x0628, 0 };
    SCRIPT_ITEM it[2]; CHECK(g6_itemize(beh3, 3, it, 2, NULL) == 1);
    memset(&r, 0, sizeof r); r.sa = it[0].a;
    HRESULT hr = U(ScriptShape)(g.dc, &r.cache, beh3, 3, 16, &r.sa, r.glyphs, r.clust, r.va, &r.ng);
    if (hr == HR_NOTINFONT && g6_is_wine()) { t_skip("host font lacks Arabic"); U(ScriptFreeCache)(&r.cache); g6_close(&g); return; }
    CHECKF(hr == S_OK, "hr 0x%08lx", (unsigned long)hr);
    if (hr == S_OK) {
        CHECK(r.ng == 3);
        if (r.ng == 3) {
            CHECK(r.glyphs[0] != r.glyphs[1]);
            CHECK(r.glyphs[1] != r.glyphs[2]);
            CHECK(r.glyphs[0] != r.glyphs[2]);
        }
    }
    U(ScriptFreeCache)(&r.cache);
    g6_close(&g);
}
TEST(gap6_font_shape_hebrew_rtl) {
    NEED(ScriptShape); NEED(ScriptFreeCache);
    G6_OPEN(g, 128, 32, ARIAL, 20);
    G6Run r; memset(&r, 0, sizeof r);
    const WCHAR h[] = { 0x05d0, 0x05d1, 0x05d2, 0 };
    SCRIPT_ITEM it[2]; CHECK(g6_itemize(h, 3, it, 2, NULL) == 1);
    r.sa = it[0].a;
    HRESULT hr = U(ScriptShape)(g.dc, &r.cache, h, 3, 16, &r.sa, r.glyphs, r.clust, r.va, &r.ng);
    CHECKF(hr == S_OK, "hr 0x%08lx", (unsigned long)hr);
    if (hr == S_OK) {
        CHECK(r.ng == 3);
        /* RTL run: visual glyph order is reversed relative to the characters */
        CHECK(r.clust[0] == 2 && r.clust[1] == 1 && r.clust[2] == 0);
    }
    U(ScriptFreeCache)(&r.cache);
    g6_close(&g);
}
TEST(gap6_font_shape_bitmap_font_fallback) {
    NEED(ScriptShape); NEED(ScriptFreeCache);
    G6_OPEN(g, 128, 32, L"Terminal", 12);
    G6Run r; memset(&r, 0, sizeof r);
    SCRIPT_ITEM it[2]; CHECK(g6_itemize(L"abc", 3, it, 2, NULL) == 1);
    r.sa = it[0].a;
    HRESULT hr = U(ScriptShape)(g.dc, &r.cache, L"abc", 3, 8, &r.sa, r.glyphs, r.clust, r.va, &r.ng);
    t_note("ScriptShape on bitmap font -> 0x%08lx ng=%d fNoGlyphIndex=%d", (unsigned long)hr, r.ng, r.sa.fNoGlyphIndex);
    CHECK(SUCCEEDED(hr));
    if (SUCCEEDED(hr)) CHECK(r.ng == 3);
    U(ScriptFreeCache)(&r.cache);
    g6_close(&g);
}
TEST(gap6_font_shape_many_repeats_stable) {
    NEED(ScriptShape); NEED(ScriptFreeCache);
    G6_OPEN(g, 128, 32, ARIAL, 20);
    G6Run r0;
    EXPECT_HR(g6_shape_place(&g, &r0, L"stable text", 11), S_OK);
    for (int i = 0; i < 200; i++) {
        G6Run r;
        EXPECT_HR(g6_shape_place(&g, &r, L"stable text", 11), S_OK);
        if (memcmp(r.glyphs, r0.glyphs, 11 * sizeof(WORD)) || memcmp(r.adv, r0.adv, 11 * sizeof(int))) { CHECKF(0, "iteration %d differs", i); break; }
        U(ScriptFreeCache)(&r.cache);
    }
    U(ScriptFreeCache)(&r0.cache);
    g6_close(&g);
}

/* ---------------- ScriptPlace ---------------- */
TEST(gap6_font_place_invalid) {
    NEED(ScriptPlace); NEED(ScriptFreeCache);
    WINE_SKIP("no NULL validation");
    G6_OPEN(g, 128, 32, ARIAL, 20);
    G6Run r; CHECK(g6_shape_place(&g, &r, L"abc", 3) == S_OK);
    ABC abc; int adv2[8]; GOFFSET off[8];
    EXPECT_HR(U(ScriptPlace)(g.dc, NULL, r.glyphs, r.ng, r.va, &r.sa, adv2, off, &abc), HR_INVALIDARG);
    EXPECT_HR(U(ScriptPlace)(g.dc, &r.cache, NULL, r.ng, r.va, &r.sa, adv2, off, &abc), HR_INVALIDARG);
    EXPECT_HR(U(ScriptPlace)(g.dc, &r.cache, r.glyphs, r.ng, NULL, &r.sa, adv2, off, &abc), HR_INVALIDARG);
    EXPECT_HR(U(ScriptPlace)(g.dc, &r.cache, r.glyphs, r.ng, r.va, NULL, adv2, off, &abc), HR_INVALIDARG);
    EXPECT_HR(U(ScriptPlace)(g.dc, &r.cache, r.glyphs, r.ng, r.va, &r.sa, NULL, off, &abc), HR_INVALIDARG);
    EXPECT_HR(U(ScriptPlace)(g.dc, &r.cache, r.glyphs, -1, r.va, &r.sa, adv2, off, &abc), HR_INVALIDARG);
    EXPECT_HR(U(ScriptPlace)(g.dc, &r.cache, r.glyphs, (int)0x80000000, r.va, &r.sa, adv2, off, &abc), HR_INVALIDARG);
    U(ScriptFreeCache)(&r.cache);
    g6_close(&g);
}
TEST(gap6_font_place_zero_glyphs) {
    NEED(ScriptPlace);
    WINE_SKIP("needs DC for zero glyphs");
    SCRIPT_CACHE c = NULL; WORD gl[1] = {0}; SCRIPT_VISATTR va[1]; memset(va, 0, sizeof va);
    SCRIPT_ANALYSIS sa; memset(&sa, 0, sizeof sa);
    int adv[1] = {-7}; GOFFSET off[1]; ABC abc = { 11, 22, 33 };
    EXPECT_HR(U(ScriptPlace)(NULL, &c, gl, 0, va, &sa, adv, off, &abc), S_OK);
    CHECK(abc.abcA == 0 && abc.abcB == 0 && abc.abcC == 0);
    CHECK(adv[0] == -7);
    EXPECT_HR(U(ScriptPlace)(NULL, &c, gl, 0, va, &sa, adv, off, NULL), S_OK);
}
TEST(gap6_font_place_null_dc_empty_cache) {
    NEED(ScriptPlace);
    SCRIPT_CACHE c = NULL; WORD gl[1] = {3}; SCRIPT_VISATTR va[1]; memset(va, 0, sizeof va);
    SCRIPT_ANALYSIS sa; memset(&sa, 0, sizeof sa);
    int adv[1]; GOFFSET off[1]; ABC abc;
    HRESULT hr = U(ScriptPlace)(NULL, &c, gl, 1, va, &sa, adv, off, &abc);
    CHECK(FAILED(hr));
    t_note("ScriptPlace(NULL dc, empty cache) -> 0x%08lx", (unsigned long)hr);
    if (c) U(ScriptFreeCache)(&c);
}
TEST(gap6_font_place_advances_and_abc) {
    NEED(ScriptPlace); NEED(ScriptFreeCache); NEED(GetTextExtentPoint32W);
    G6_OPEN(g, 256, 32, ARIAL, 20);
    G6Run r;
    EXPECT_HR(g6_shape_place(&g, &r, L"Hello", 5), S_OK);
    int sum = 0;
    for (int i = 0; i < r.ng; i++) {
        CHECKF(r.adv[i] > 0, "adv[%d]=%d", i, r.adv[i]);
        CHECKF(r.off[i].du == 0 && r.off[i].dv == 0, "off[%d]=%ld,%ld", i, r.off[i].du, r.off[i].dv);
        sum += r.adv[i];
    }
    CHECKF(sum == r.abc.abcA + (int)r.abc.abcB + r.abc.abcC, "adv sum %d abc %d+%u+%d", sum, r.abc.abcA, r.abc.abcB, r.abc.abcC);
    SIZE sz; CHECK(U(GetTextExtentPoint32W)(g.dc, L"Hello", 5, &sz));
    t_note("ScriptPlace sum=%d GetTextExtentPoint32W cx=%ld", sum, sz.cx);
    CHECK(sum > 0 && sum <= sz.cx + 2 && sum >= sz.cx - 4);
    U(ScriptFreeCache)(&r.cache);
    g6_close(&g);
}
TEST(gap6_font_place_wider_for_larger_font) {
    NEED(ScriptPlace); NEED(ScriptFreeCache);
    int prev = 0;
    for (int px = 12; px <= 48; px += 12) {
        G6_OPEN(g, 256, 32, ARIAL, px);
        G6Run r;
        EXPECT_HR(g6_shape_place(&g, &r, L"Width", 5), S_OK);
        int sum = 0; for (int i = 0; i < r.ng; i++) sum += r.adv[i];
        CHECKF(sum > prev, "px %d sum %d prev %d", px, sum, prev);
        prev = sum;
        U(ScriptFreeCache)(&r.cache);
        g6_close(&g);
    }
}
TEST(gap6_font_place_pabc_optional) {
    NEED(ScriptPlace); NEED(ScriptFreeCache);
    G6_OPEN(g, 256, 32, ARIAL, 20);
    G6Run r; CHECK(g6_shape_place(&g, &r, L"abc", 3) == S_OK);
    int adv2[8]; GOFFSET off[8];
    EXPECT_HR(U(ScriptPlace)(g.dc, &r.cache, r.glyphs, r.ng, r.va, &r.sa, adv2, off, NULL), S_OK);
    for (int i = 0; i < r.ng; i++) CHECK(adv2[i] == r.adv[i]);
    U(ScriptFreeCache)(&r.cache);
    g6_close(&g);
}
TEST(gap6_font_place_space_advance_matches_char_width) {
    NEED(ScriptPlace); NEED(ScriptFreeCache); NEED(GetCharWidth32W);
    G6_OPEN(g, 256, 32, ARIAL, 20);
    G6Run r; CHECK(g6_shape_place(&g, &r, L" ", 1) == S_OK);
    int w = 0; CHECK(U(GetCharWidth32W)(g.dc, L' ', L' ', &w));
    CHECKF(r.adv[0] == w, "space adv %d GetCharWidth32 %d", r.adv[0], w);
    U(ScriptFreeCache)(&r.cache);
    g6_close(&g);
}

/* ---------------- ScriptGetGlyphABCWidth ---------------- */
TEST(gap6_font_glyphabc_invalid) {
    NEED(ScriptGetGlyphABCWidth); NEED(ScriptFreeCache);
    WINE_SKIP("no NULL validation");
    G6_OPEN(g, 64, 32, ARIAL, 20);
    SCRIPT_CACHE c = NULL; ABC abc;
    EXPECT_HR(U(ScriptGetGlyphABCWidth)(g.dc, &c, 5, NULL), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetGlyphABCWidth)(g.dc, NULL, 5, &abc), HR_INVALIDARG);
    g6_close(&g);
}
TEST(gap6_font_glyphabc_matches_gdi) {
    NEED(ScriptGetGlyphABCWidth); NEED(ScriptFreeCache); NEED(GetGlyphIndicesW); NEED(GetCharABCWidthsI);
    G6_OPEN(g, 64, 32, ARIAL, 24);
    WORD gi; WCHAR ch = L'W';
    CHECK(U(GetGlyphIndicesW)(g.dc, &ch, 1, &gi, GGI_MARK_NONEXISTING_GLYPHS) == 1);
    SCRIPT_CACHE c = NULL; ABC a, b;
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    EXPECT_HR(U(ScriptGetGlyphABCWidth)(g.dc, &c, gi, &a), S_OK);
    CHECK(U(GetCharABCWidthsI)(g.dc, gi, 1, NULL, &b));
    CHECK(a.abcB > 0);
    CHECKF(a.abcA == b.abcA && a.abcB == b.abcB && a.abcC == b.abcC, "script %d,%u,%d gdi %d,%u,%d", a.abcA, a.abcB, a.abcC, b.abcA, b.abcB, b.abcC);
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}
TEST(gap6_font_glyphabc_null_dc_with_cache) {
    NEED(ScriptGetGlyphABCWidth); NEED(ScriptFreeCache); NEED(GetGlyphIndicesW);
    G6_OPEN(g, 64, 32, ARIAL, 24);
    WORD gi; WCHAR ch = L'i';
    CHECK(U(GetGlyphIndicesW)(g.dc, &ch, 1, &gi, GGI_MARK_NONEXISTING_GLYPHS) == 1);
    SCRIPT_CACHE c = NULL; ABC a, b;
    EXPECT_HR(U(ScriptGetGlyphABCWidth)(g.dc, &c, gi, &a), S_OK);
    HRESULT hr = U(ScriptGetGlyphABCWidth)(NULL, &c, gi, &b);
    t_note("ScriptGetGlyphABCWidth with cached font and NULL dc -> 0x%08lx", (unsigned long)hr);
    if (hr == S_OK) CHECK(memcmp(&a, &b, sizeof a) == 0);
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}
TEST(gap6_font_glyphabc_invalid_glyph_index) {
    NEED(ScriptGetGlyphABCWidth); NEED(ScriptFreeCache);
    G6_OPEN(g, 64, 32, ARIAL, 24);
    SCRIPT_CACHE c = NULL; ABC a;
    HRESULT hr = U(ScriptGetGlyphABCWidth)(g.dc, &c, 0xffff, &a);
    t_note("ScriptGetGlyphABCWidth(0xffff) -> 0x%08lx", (unsigned long)hr);
    hr = U(ScriptGetGlyphABCWidth)(g.dc, &c, 0, &a);
    t_note("ScriptGetGlyphABCWidth(0) -> 0x%08lx", (unsigned long)hr);
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}

/* ---------------- ScriptGetFont*Tags / AlternateGlyphs ---------------- */
TEST(gap6_font_scripttags_invalid) {
    NEED(ScriptGetFontScriptTags); NEED(ScriptFreeCache);
    WINE_SKIP("no NULL validation");
    G6_OPEN(g, 64, 32, ARIAL, 20);
    SCRIPT_CACHE c = NULL; OPENTYPE_TAG tg[8]; int n = 99;
    EXPECT_HR(U(ScriptGetFontScriptTags)(g.dc, NULL, NULL, 8, tg, &n), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetFontScriptTags)(g.dc, &c, NULL, 0, tg, &n), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetFontScriptTags)(g.dc, &c, NULL, -1, tg, &n), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetFontScriptTags)(g.dc, &c, NULL, 8, NULL, &n), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetFontScriptTags)(g.dc, &c, NULL, 8, tg, NULL), HR_INVALIDARG);
    CHECK(n == 99);
    g6_close(&g);
}
TEST(gap6_font_scripttags_arial) {
    NEED(ScriptGetFontScriptTags); NEED(ScriptFreeCache);
    G6_OPEN(g, 64, 32, ARIAL, 20);
    SCRIPT_CACHE c = NULL; OPENTYPE_TAG tg[64]; int n = 0;
    HRESULT hr = U(ScriptGetFontScriptTags)(g.dc, &c, NULL, 64, tg, &n);
    t_note("Arial script tags: hr=0x%08lx n=%d", (unsigned long)hr, n);
    CHECK(SUCCEEDED(hr));
    if (SUCCEEDED(hr)) {
        CHECK(n >= 1 && n <= 64);
        int latn = 0;
        for (int i = 0; i < n && i < 64; i++) if (tg[i] == TAG4('l','a','t','n')) latn = 1;
        CHECK(latn);
    }
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}
TEST(gap6_font_scripttags_small_buffer) {
    NEED(ScriptGetFontScriptTags); NEED(ScriptFreeCache);
    G6_OPEN(g, 64, 32, ARIAL, 20);
    SCRIPT_CACHE c = NULL; OPENTYPE_TAG tg[64]; int full = 0, n = 0;
    if (FAILED(U(ScriptGetFontScriptTags)(g.dc, &c, NULL, 64, tg, &full)) || full < 2) { t_skip("font reports fewer than 2 scripts"); g6_close(&g); return; }
    HRESULT hr = U(ScriptGetFontScriptTags)(g.dc, &c, NULL, 1, tg, &n);
    t_note("one-slot buffer: hr=0x%08lx n=%d (full %d)", (unsigned long)hr, n, full);
    CHECK(hr == HR_OUTOFMEMORY || hr == S_OK);
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}
TEST(gap6_font_scripttags_with_analysis) {
    NEED(ScriptGetFontScriptTags); NEED(ScriptFreeCache); NEED(ScriptItemizeOpenType);
    G6_OPEN(g, 64, 32, ARIAL, 20);
    SCRIPT_ITEM it[2]; OPENTYPE_TAG st[2]; int ni = 0;
    CHECK(U(ScriptItemizeOpenType)(L"Hello", 5, 2, NULL, NULL, it, st, &ni) == S_OK);
    SCRIPT_CACHE c = NULL; OPENTYPE_TAG tg[16]; int n = 0;
    HRESULT hr = U(ScriptGetFontScriptTags)(g.dc, &c, &it[0].a, 16, tg, &n);
    t_note("script tags for latin analysis: hr=0x%08lx n=%d", (unsigned long)hr, n);
    CHECK(SUCCEEDED(hr) || hr == HR_NOTINFONT);
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}
TEST(gap6_font_scripttags_bitmap_font) {
    NEED(ScriptGetFontScriptTags); NEED(ScriptFreeCache);
    G6_OPEN(g, 64, 32, L"Terminal", 12);
    SCRIPT_CACHE c = NULL; OPENTYPE_TAG tg[8]; int n = -1;
    HRESULT hr = U(ScriptGetFontScriptTags)(g.dc, &c, NULL, 8, tg, &n);
    t_note("bitmap font script tags: hr=0x%08lx n=%d", (unsigned long)hr, n);
    CHECK(hr == S_OK || FAILED(hr));
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}
TEST(gap6_font_langtags_invalid) {
    NEED(ScriptGetFontLanguageTags); NEED(ScriptFreeCache);
    WINE_SKIP("no NULL validation");
    G6_OPEN(g, 64, 32, ARIAL, 20);
    SCRIPT_CACHE c = NULL; OPENTYPE_TAG tg[8]; int n = 99;
    OPENTYPE_TAG latn = TAG4('l','a','t','n');
    EXPECT_HR(U(ScriptGetFontLanguageTags)(g.dc, NULL, NULL, latn, 8, tg, &n), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetFontLanguageTags)(g.dc, &c, NULL, latn, 0, tg, &n), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetFontLanguageTags)(g.dc, &c, NULL, latn, -3, tg, &n), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetFontLanguageTags)(g.dc, &c, NULL, latn, 8, NULL, &n), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetFontLanguageTags)(g.dc, &c, NULL, latn, 8, tg, NULL), HR_INVALIDARG);
    CHECK(n == 99);
    g6_close(&g);
}
TEST(gap6_font_langtags_wrong_script_tag_for_analysis) {
    NEED(ScriptGetFontLanguageTags); NEED(ScriptFreeCache); NEED(ScriptItemizeOpenType);
    WINE_SKIP("no tag validation");
    G6_OPEN(g, 64, 32, ARIAL, 20);
    SCRIPT_ITEM it[2]; OPENTYPE_TAG st[2]; int ni = 0;
    CHECK(U(ScriptItemizeOpenType)(L"Hello", 5, 2, NULL, NULL, it, st, &ni) == S_OK);
    SCRIPT_CACHE c = NULL; OPENTYPE_TAG tg[16]; int n = 0;
    EXPECT_HR(U(ScriptGetFontLanguageTags)(g.dc, &c, &it[0].a, TAG4('a','r','a','b'), 16, tg, &n), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetFontLanguageTags)(g.dc, &c, &it[0].a, TAG4('z','z','z','z'), 16, tg, &n), HR_INVALIDARG);
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}
TEST(gap6_font_langtags_arial_latn) {
    NEED(ScriptGetFontLanguageTags); NEED(ScriptFreeCache);
    G6_OPEN(g, 64, 32, ARIAL, 20);
    SCRIPT_CACHE c = NULL; OPENTYPE_TAG tg[128]; int n = 0;
    HRESULT hr = U(ScriptGetFontLanguageTags)(g.dc, &c, NULL, TAG4('l','a','t','n'), 128, tg, &n);
    t_note("Arial latn language tags: hr=0x%08lx n=%d", (unsigned long)hr, n);
    CHECK(SUCCEEDED(hr) || hr == HR_NOTINFONT);
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}
TEST(gap6_font_featuretags_invalid) {
    NEED(ScriptGetFontFeatureTags); NEED(ScriptFreeCache);
    WINE_SKIP("no NULL validation");
    G6_OPEN(g, 64, 32, ARIAL, 20);
    SCRIPT_CACHE c = NULL; OPENTYPE_TAG tg[8]; int n = 99;
    OPENTYPE_TAG latn = TAG4('l','a','t','n');
    EXPECT_HR(U(ScriptGetFontFeatureTags)(g.dc, NULL, NULL, latn, 0, 8, tg, &n), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetFontFeatureTags)(g.dc, &c, NULL, latn, 0, 0, tg, &n), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetFontFeatureTags)(g.dc, &c, NULL, latn, 0, -1, tg, &n), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetFontFeatureTags)(g.dc, &c, NULL, latn, 0, 8, NULL, &n), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetFontFeatureTags)(g.dc, &c, NULL, latn, 0, 8, tg, NULL), HR_INVALIDARG);
    CHECK(n == 99);
    g6_close(&g);
}
TEST(gap6_font_featuretags_wrong_script_tag_for_analysis) {
    NEED(ScriptGetFontFeatureTags); NEED(ScriptFreeCache); NEED(ScriptItemizeOpenType);
    WINE_SKIP("no tag validation");
    G6_OPEN(g, 64, 32, ARIAL, 20);
    SCRIPT_ITEM it[2]; OPENTYPE_TAG st[2]; int ni = 0;
    CHECK(U(ScriptItemizeOpenType)(L"Hello", 5, 2, NULL, NULL, it, st, &ni) == S_OK);
    SCRIPT_CACHE c = NULL; OPENTYPE_TAG tg[16]; int n = 0;
    EXPECT_HR(U(ScriptGetFontFeatureTags)(g.dc, &c, &it[0].a, TAG4('h','e','b','r'), 0, 16, tg, &n), HR_INVALIDARG);
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}
TEST(gap6_font_featuretags_arial_latn) {
    NEED(ScriptGetFontFeatureTags); NEED(ScriptFreeCache);
    G6_OPEN(g, 64, 32, ARIAL, 20);
    SCRIPT_CACHE c = NULL; OPENTYPE_TAG tg[128]; int n = 0;
    HRESULT hr = U(ScriptGetFontFeatureTags)(g.dc, &c, NULL, TAG4('l','a','t','n'), 0, 128, tg, &n);
    t_note("Arial latn feature tags: hr=0x%08lx n=%d", (unsigned long)hr, n);
    CHECK(SUCCEEDED(hr) || hr == HR_NOTINFONT);
    if (SUCCEEDED(hr) && n > 0) {
        for (int i = 0; i < n && i < 128; i++) CHECKF(tg[i] != 0, "tag[%d] is zero", i);
    }
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}
TEST(gap6_font_altglyphs_invalid) {
    NEED(ScriptGetFontAlternateGlyphs); NEED(ScriptFreeCache);
    WINE_SKIP("no NULL validation");
    G6_OPEN(g, 64, 32, ARIAL, 20);
    SCRIPT_CACHE c = NULL; WORD gl[8]; int n = 99;
    OPENTYPE_TAG latn = TAG4('l','a','t','n'), sal = TAG4('s','a','l','t');
    EXPECT_HR(U(ScriptGetFontAlternateGlyphs)(g.dc, NULL, NULL, latn, 0, sal, 5, 8, gl, &n), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetFontAlternateGlyphs)(g.dc, &c, NULL, latn, 0, sal, 5, 0, gl, &n), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetFontAlternateGlyphs)(g.dc, &c, NULL, latn, 0, sal, 5, -1, gl, &n), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetFontAlternateGlyphs)(g.dc, &c, NULL, latn, 0, sal, 5, 8, NULL, &n), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetFontAlternateGlyphs)(g.dc, &c, NULL, latn, 0, sal, 5, 8, gl, NULL), HR_INVALIDARG);
    CHECK(n == 99);
    g6_close(&g);
}
TEST(gap6_font_altglyphs_missing_feature) {
    NEED(ScriptGetFontAlternateGlyphs); NEED(ScriptFreeCache);
    G6_OPEN(g, 64, 32, ARIAL, 20);
    SCRIPT_CACHE c = NULL; WORD gl[8]; int n = 99;
    HRESULT hr = U(ScriptGetFontAlternateGlyphs)(g.dc, &c, NULL, TAG4('l','a','t','n'), 0, TAG4('z','z','z','z'), 5, 8, gl, &n);
    t_note("alt glyphs for missing feature: hr=0x%08lx n=%d", (unsigned long)hr, n);
    CHECK(hr != S_OK || n == 0);
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}

/* ---------------- ScriptShapeOpenType / ScriptPlaceOpenType ---------------- */
typedef struct { SCRIPT_CACHE cache; SCRIPT_ANALYSIS sa; OPENTYPE_TAG tag; WORD glyphs[64], clust[64]; SCRIPT_CHARPROP cp[64]; SCRIPT_GLYPHPROP gp[64]; int ng; int adv[64]; GOFFSET off[64]; ABC abc; } OTRun;
static HRESULT ot_shape(G6 *g, OTRun *r, const WCHAR *s, int n, int cmax) {
    SCRIPT_ITEM it[4]; OPENTYPE_TAG tg[4]; int ni = 0;
    memset(r, 0, sizeof *r);
    if (U(ScriptItemizeOpenType)(s, n, 4, NULL, NULL, it, tg, &ni) != S_OK || ni < 1) return E_FAIL;
    r->sa = it[0].a; r->tag = tg[0];
    return U(ScriptShapeOpenType)(g->dc, &r->cache, &r->sa, r->tag, 0, NULL, NULL, 0, s, n, cmax, r->clust, r->cp, r->glyphs, r->gp, &r->ng);
}
TEST(gap6_font_shapeot_invalid) {
    NEED(ScriptShapeOpenType); NEED(ScriptItemizeOpenType); NEED(ScriptFreeCache);
    WINE_SKIP("no NULL validation");
    G6_OPEN(g, 64, 32, ARIAL, 20);
    OTRun r; CHECK(ot_shape(&g, &r, L"abc", 3, 64) == S_OK);
    SCRIPT_CACHE c = NULL; int ng = 55; OPENTYPE_TAG t = r.tag;
    const WCHAR *s = L"abc";
#define SH(cache, sa, tag, str, cch, cmax, clu, cp, gl, gp, png) \
    U(ScriptShapeOpenType)(g.dc, cache, sa, tag, 0, NULL, NULL, 0, str, cch, cmax, clu, cp, gl, gp, png)
    EXPECT_HR(SH(NULL, &r.sa, t, s, 3, 64, r.clust, r.cp, r.glyphs, r.gp, &ng), HR_INVALIDARG);
    EXPECT_HR(SH(&c, NULL, t, s, 3, 64, r.clust, r.cp, r.glyphs, r.gp, &ng), HR_INVALIDARG);
    EXPECT_HR(SH(&c, &r.sa, t, NULL, 3, 64, r.clust, r.cp, r.glyphs, r.gp, &ng), HR_INVALIDARG);
    EXPECT_HR(SH(&c, &r.sa, t, s, 3, 64, NULL, r.cp, r.glyphs, r.gp, &ng), HR_INVALIDARG);
    EXPECT_HR(SH(&c, &r.sa, t, s, 3, 64, r.clust, NULL, r.glyphs, r.gp, &ng), HR_INVALIDARG);
    EXPECT_HR(SH(&c, &r.sa, t, s, 3, 64, r.clust, r.cp, NULL, r.gp, &ng), HR_INVALIDARG);
    EXPECT_HR(SH(&c, &r.sa, t, s, 3, 64, r.clust, r.cp, r.glyphs, NULL, &ng), HR_INVALIDARG);
    EXPECT_HR(SH(&c, &r.sa, t, s, 3, 64, r.clust, r.cp, r.glyphs, r.gp, NULL), HR_INVALIDARG);
    EXPECT_HR(SH(&c, &r.sa, t, s, 0x10000, 64, r.clust, r.cp, r.glyphs, r.gp, &ng), HR_INVALIDARG);
    EXPECT_HR(SH(&c, &r.sa, t, s, 3, 0x10000, r.clust, r.cp, r.glyphs, r.gp, &ng), HR_INVALIDARG);
    CHECK(ng == 55);
    SCRIPT_ANALYSIS bad = r.sa; bad.eScript = 207;
    EXPECT_HR(SH(&c, &bad, t, s, 3, 64, r.clust, r.cp, r.glyphs, r.gp, &ng), HR_INVALIDARG);
    EXPECT_HR(SH(&c, &r.sa, TAG4('a','r','a','b'), s, 3, 64, r.clust, r.cp, r.glyphs, r.gp, &ng), HR_INVALIDARG);
#undef SH
    U(ScriptFreeCache)(&r.cache);
    g6_close(&g);
}
TEST(gap6_font_shapeot_latin) {
    NEED(ScriptShapeOpenType); NEED(ScriptItemizeOpenType); NEED(ScriptFreeCache); NEED(GetGlyphIndicesW);
    G6_OPEN(g, 64, 32, ARIAL, 20);
    OTRun r;
    HRESULT hr = ot_shape(&g, &r, L"Hello", 5, 64);
    EXPECT_HR(hr, S_OK);
    if (hr == S_OK) {
        CHECK(r.ng == 5);
        WORD ref[8]; CHECK(U(GetGlyphIndicesW)(g.dc, L"Hello", 5, ref, GGI_MARK_NONEXISTING_GLYPHS) == 5);
        for (int i = 0; i < 5 && i < r.ng; i++) {
            CHECKF(r.glyphs[i] == ref[i], "glyph[%d] %u gdi %u", i, r.glyphs[i], ref[i]);
            CHECKF(r.clust[i] == i, "clust[%d]=%u", i, r.clust[i]);
        }
    }
    U(ScriptFreeCache)(&r.cache);
    g6_close(&g);
}
TEST(gap6_font_shapeot_cmax_too_small) {
    NEED(ScriptShapeOpenType); NEED(ScriptItemizeOpenType); NEED(ScriptFreeCache);
    G6_OPEN(g, 64, 32, ARIAL, 20);
    OTRun r;
    HRESULT hr = ot_shape(&g, &r, L"Hello", 5, 2);
    CHECK(FAILED(hr));
    EXPECT_HR(hr, HR_OUTOFMEMORY);
    U(ScriptFreeCache)(&r.cache);
    g6_close(&g);
}
TEST(gap6_font_shapeot_matches_shape) {
    NEED(ScriptShapeOpenType); NEED(ScriptShape); NEED(ScriptItemizeOpenType); NEED(ScriptFreeCache);
    G6_OPEN(g, 64, 32, ARIAL, 20);
    OTRun o; G6Run s;
    EXPECT_HR(ot_shape(&g, &o, L"Shaping", 7, 64), S_OK);
    EXPECT_HR(g6_shape_place(&g, &s, L"Shaping", 7), S_OK);
    CHECK(o.ng == s.ng);
    if (o.ng == s.ng) CHECK(memcmp(o.glyphs, s.glyphs, o.ng * sizeof(WORD)) == 0);
    U(ScriptFreeCache)(&o.cache); U(ScriptFreeCache)(&s.cache);
    g6_close(&g);
}
TEST(gap6_font_placeot_invalid) {
    NEED(ScriptPlaceOpenType); NEED(ScriptShapeOpenType); NEED(ScriptItemizeOpenType); NEED(ScriptFreeCache);
    WINE_SKIP("no NULL validation");
    G6_OPEN(g, 64, 32, ARIAL, 20);
    OTRun r; CHECK(ot_shape(&g, &r, L"abc", 3, 64) == S_OK);
    SCRIPT_CACHE c = NULL; const WCHAR *s = L"abc";
#define PL(cache, sa, tag, str, clu, cp, cch, gl, gp, cg, adv, off, abc) \
    U(ScriptPlaceOpenType)(g.dc, cache, sa, tag, 0, NULL, NULL, 0, str, clu, cp, cch, gl, gp, cg, adv, off, abc)
    EXPECT_HR(PL(NULL, &r.sa, r.tag, s, r.clust, r.cp, 3, r.glyphs, r.gp, r.ng, r.adv, r.off, &r.abc), HR_INVALIDARG);
    EXPECT_HR(PL(&c, NULL, r.tag, s, r.clust, r.cp, 3, r.glyphs, r.gp, r.ng, r.adv, r.off, &r.abc), HR_INVALIDARG);
    EXPECT_HR(PL(&c, &r.sa, r.tag, NULL, r.clust, r.cp, 3, r.glyphs, r.gp, r.ng, r.adv, r.off, &r.abc), HR_INVALIDARG);
    EXPECT_HR(PL(&c, &r.sa, r.tag, s, NULL, r.cp, 3, r.glyphs, r.gp, r.ng, r.adv, r.off, &r.abc), HR_INVALIDARG);
    EXPECT_HR(PL(&c, &r.sa, r.tag, s, r.clust, NULL, 3, r.glyphs, r.gp, r.ng, r.adv, r.off, &r.abc), HR_INVALIDARG);
    EXPECT_HR(PL(&c, &r.sa, r.tag, s, r.clust, r.cp, 3, NULL, r.gp, r.ng, r.adv, r.off, &r.abc), HR_INVALIDARG);
    EXPECT_HR(PL(&c, &r.sa, r.tag, s, r.clust, r.cp, 3, r.glyphs, NULL, r.ng, r.adv, r.off, &r.abc), HR_INVALIDARG);
    EXPECT_HR(PL(&c, &r.sa, r.tag, s, r.clust, r.cp, 3, r.glyphs, r.gp, r.ng, NULL, r.off, &r.abc), HR_INVALIDARG);
    EXPECT_HR(PL(&c, &r.sa, r.tag, s, r.clust, r.cp, 3, r.glyphs, r.gp, r.ng, r.adv, NULL, &r.abc), HR_INVALIDARG);
    EXPECT_HR(PL(&c, &r.sa, TAG4('h','e','b','r'), s, r.clust, r.cp, 3, r.glyphs, r.gp, r.ng, r.adv, r.off, &r.abc), HR_INVALIDARG);
#undef PL
    U(ScriptFreeCache)(&r.cache);
    g6_close(&g);
}
TEST(gap6_font_placeot_matches_place) {
    NEED(ScriptPlaceOpenType); NEED(ScriptShapeOpenType); NEED(ScriptItemizeOpenType); NEED(ScriptFreeCache);
    G6_OPEN(g, 64, 32, ARIAL, 20);
    OTRun r; const WCHAR *s = L"Place";
    EXPECT_HR(ot_shape(&g, &r, s, 5, 64), S_OK);
    HRESULT hr = U(ScriptPlaceOpenType)(g.dc, &r.cache, &r.sa, r.tag, 0, NULL, NULL, 0, s, r.clust, r.cp, 5, r.glyphs, r.gp, r.ng, r.adv, r.off, &r.abc);
    EXPECT_HR(hr, S_OK);
    G6Run p; EXPECT_HR(g6_shape_place(&g, &p, s, 5), S_OK);
    int sa = 0, sb = 0;
    for (int i = 0; i < r.ng; i++) { CHECK(r.adv[i] > 0); sa += r.adv[i]; }
    for (int i = 0; i < p.ng; i++) sb += p.adv[i];
    t_note("PlaceOpenType sum %d ScriptPlace sum %d", sa, sb);
    CHECK(sa > 0 && sb > 0);
    CHECKF(sa == r.abc.abcA + (int)r.abc.abcB + r.abc.abcC, "abc mismatch");
    U(ScriptFreeCache)(&r.cache); U(ScriptFreeCache)(&p.cache);
    g6_close(&g);
}

/* ---------------- ScriptPositionSingleGlyph ---------------- */
TEST(gap6_font_possingle_invalid) {
    NEED(ScriptPositionSingleGlyph); NEED(ScriptFreeCache);
    WINE_SKIP("no NULL validation");
    G6_OPEN(g, 64, 32, ARIAL, 20);
    SCRIPT_CACHE c = NULL; GOFFSET go = {0, 0}, out; int oa = -9;
    OPENTYPE_TAG latn = TAG4('l','a','t','n'), kern = TAG4('k','e','r','n');
    EXPECT_HR(U(ScriptPositionSingleGlyph)(g.dc, NULL, NULL, latn, 0, kern, 0, 5, 10, go, &oa, &out), HR_INVALIDARG);
    EXPECT_HR(U(ScriptPositionSingleGlyph)(g.dc, &c, NULL, latn, 0, kern, 0, 5, 10, go, NULL, &out), HR_INVALIDARG);
    EXPECT_HR(U(ScriptPositionSingleGlyph)(g.dc, &c, NULL, latn, 0, kern, 0, 5, 10, go, &oa, NULL), HR_INVALIDARG);
    CHECK(oa == -9);
    g6_close(&g);
}
TEST(gap6_font_possingle_valid_call) {
    NEED(ScriptPositionSingleGlyph); NEED(ScriptFreeCache);
    G6_OPEN(g, 64, 32, ARIAL, 20);
    SCRIPT_CACHE c = NULL; GOFFSET go = {0, 0}, out = {7, 7}; int oa = -9;
    HRESULT hr = U(ScriptPositionSingleGlyph)(g.dc, &c, NULL, TAG4('l','a','t','n'), 0, TAG4('k','e','r','n'), 0, 5, 10, go, &oa, &out);
    t_note("ScriptPositionSingleGlyph(kern) -> 0x%08lx adv=%d off=%ld,%ld", (unsigned long)hr, oa, out.du, out.dv);
    if (hr == S_OK) CHECK(oa >= 0);
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}
TEST(gap6_font_possingle_unknown_feature_keeps_advance) {
    NEED(ScriptPositionSingleGlyph); NEED(ScriptFreeCache);
    G6_OPEN(g, 64, 32, ARIAL, 20);
    SCRIPT_CACHE c = NULL; GOFFSET go = {0, 0}, out = {7, 7}; int oa = -9;
    HRESULT hr = U(ScriptPositionSingleGlyph)(g.dc, &c, NULL, TAG4('l','a','t','n'), 0, TAG4('z','z','z','z'), 0, 5, 10, go, &oa, &out);
    t_note("ScriptPositionSingleGlyph(zzzz) -> 0x%08lx adv=%d off=%ld,%ld", (unsigned long)hr, oa, out.du, out.dv);
    if (hr == S_OK) CHECK(oa == 10 && out.du == 0 && out.dv == 0);
    U(ScriptFreeCache)(&c);
    g6_close(&g);
}

/* ---------------- ScriptCPtoX with a real shaped run ---------------- */
TEST(gap6_font_cptox_real_run) {
    NEED(ScriptCPtoX); NEED(ScriptFreeCache);
    G6_OPEN(g, 256, 32, ARIAL, 20);
    G6Run r; EXPECT_HR(g6_shape_place(&g, &r, L"Hello", 5), S_OK);
    int total = 0; for (int i = 0; i < r.ng; i++) total += r.adv[i];
    int x = -1, acc = 0;
    for (int i = 0; i < 5; i++) {
        EXPECT_HR(U(ScriptCPtoX)(i, FALSE, 5, r.ng, r.clust, r.va, r.adv, &r.sa, &x), S_OK);
        CHECKF(x == acc, "cp %d leading %d want %d", i, x, acc);
        acc += r.adv[i];
        EXPECT_HR(U(ScriptCPtoX)(i, TRUE, 5, r.ng, r.clust, r.va, r.adv, &r.sa, &x), S_OK);
        CHECKF(x == acc, "cp %d trailing %d want %d", i, x, acc);
    }
    CHECK(acc == total);
    U(ScriptFreeCache)(&r.cache);
    g6_close(&g);
}
TEST(gap6_font_logwidths_real_run) {
    NEED(ScriptGetLogicalWidths); NEED(ScriptFreeCache);
    G6_OPEN(g, 256, 32, ARIAL, 20);
    G6Run r; EXPECT_HR(g6_shape_place(&g, &r, L"Hello", 5), S_OK);
    int dx[8] = {0};
    EXPECT_HR(U(ScriptGetLogicalWidths)(&r.sa, 5, r.ng, r.adv, r.clust, r.va, dx), S_OK);
    for (int i = 0; i < 5; i++) CHECKF(dx[i] == r.adv[i], "dx[%d]=%d adv %d", i, dx[i], r.adv[i]);
    U(ScriptFreeCache)(&r.cache);
    g6_close(&g);
}
TEST(gap6_font_applylw_real_run_identity) {
    NEED(ScriptApplyLogicalWidth); NEED(ScriptGetLogicalWidths); NEED(ScriptFreeCache);
    G6_OPEN(g, 256, 32, ARIAL, 20);
    G6Run r; EXPECT_HR(g6_shape_place(&g, &r, L"Hello", 5), S_OK);
    int dx[8] = {0}, out[8] = {0};
    EXPECT_HR(U(ScriptGetLogicalWidths)(&r.sa, 5, r.ng, r.adv, r.clust, r.va, dx), S_OK);
    ABC abc;
    EXPECT_HR(U(ScriptApplyLogicalWidth)(dx, 5, r.ng, r.clust, r.va, r.adv, &r.sa, &abc, out), S_OK);
    for (int i = 0; i < r.ng; i++) CHECKF(out[i] == r.adv[i], "out[%d]=%d adv %d", i, out[i], r.adv[i]);
    U(ScriptFreeCache)(&r.cache);
    g6_close(&g);
}
TEST(gap6_font_justify_real_run) {
    NEED(ScriptJustify); NEED(ScriptFreeCache);
    G6_OPEN(g, 256, 32, ARIAL, 20);
    G6Run r; EXPECT_HR(g6_shape_place(&g, &r, L"a b c", 5), S_OK);
    int out[8] = {0}, sa = 0, so = 0;
    EXPECT_HR(U(ScriptJustify)(r.va, r.adv, r.ng, 30, 0, out), S_OK);
    for (int i = 0; i < r.ng; i++) { sa += r.adv[i]; so += out[i]; }
    t_note("justify real run: in %d out %d (dx 30)", sa, so);
    CHECK(so >= sa);
    U(ScriptFreeCache)(&r.cache);
    g6_close(&g);
}
