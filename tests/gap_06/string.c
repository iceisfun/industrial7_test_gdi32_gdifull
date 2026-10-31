#include "gap6.h"
/* ScriptString* family: analysis objects, size/logattr/outchar accessors,
 * logical widths, caret positions, ordering, drawing on a DIB-section DC. */

#define NEED(f) REQUIRE(U(f))
#define SSA_STD (SSA_GLYPHS | SSA_FALLBACK)

static HRESULT sa_make(G6 *g, const void *s, int n, int charset, DWORD flags, int reqw, SCRIPT_STRING_ANALYSIS *out) {
    return U(ScriptStringAnalyse)(g->dc, s, n, n * 3 / 2 + 16, charset, flags, reqw, NULL, NULL, NULL, NULL, NULL, out);
}

/* ---------------- ScriptStringAnalyse ---------------- */
TEST(gap6_str_analyse_null_pssa) {
    NEED(ScriptStringAnalyse);
    WINE_SKIP("no NULL validation");
    G6_OPEN(g, 128, 40, L"Arial", 20);
    HRESULT hr = U(ScriptStringAnalyse)(g.dc, L"abc", 3, 20, -1, SSA_STD, 0, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECKF(hr == HR_INVALIDARG, "hr 0x%08lx", (unsigned long)hr);
    g6_close(&g);
}
TEST(gap6_str_analyse_invalid_string_args) {
    NEED(ScriptStringAnalyse);
    WINE_SKIP("lenient parameter validation");
    G6_OPEN(g, 128, 40, L"Arial", 20);
    static WCHAR buf[8] = L"abcdefg";
    static const int bad[] = { 0, -1, -2, 0x10000, 0x10001, 0x7fffffff, (int)0x80000000 };
    for (int i = 0; i < ARRSZ(bad); i++) {
        SCRIPT_STRING_ANALYSIS ssa = (SCRIPT_STRING_ANALYSIS)(ULONG_PTR)0x1234;
        HRESULT hr = U(ScriptStringAnalyse)(g.dc, buf, bad[i], 32, -1, SSA_STD, 0, NULL, NULL, NULL, NULL, NULL, &ssa);
        CHECKF(hr == HR_INVALIDARG, "cString %d -> 0x%08lx", bad[i], (unsigned long)hr);
        CHECKF(ssa == NULL, "cString %d: *pssa not cleared (%p)", bad[i], ssa);
    }
    SCRIPT_STRING_ANALYSIS ssa = (SCRIPT_STRING_ANALYSIS)(ULONG_PTR)0x1234;
    HRESULT hr = U(ScriptStringAnalyse)(g.dc, NULL, 3, 32, -1, SSA_STD, 0, NULL, NULL, NULL, NULL, NULL, &ssa);
    CHECKF(hr == HR_INVALIDARG, "NULL string -> 0x%08lx", (unsigned long)hr);
    CHECK(ssa == NULL);
    g6_close(&g);
}
TEST(gap6_str_analyse_basic_unicode) {
    NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(ScriptString_pcOutChars); NEED(ScriptString_pSize);
    G6_OPEN(g, 128, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"Hello", 5, -1, SSA_STD, 0, &ssa), S_OK);
    CHECK(ssa != NULL);
    if (ssa) {
        const int *pc = U(ScriptString_pcOutChars)(ssa);
        const SIZE *ps = U(ScriptString_pSize)(ssa);
        CHECK(pc != NULL && ps != NULL);
        if (pc) CHECK(*pc == 5);
        if (ps) { CHECK(ps->cx > 0); CHECK(ps->cy > 0); t_note("pSize %ldx%ld", ps->cx, ps->cy); }
        EXPECT_HR(U(ScriptStringFree)(&ssa), S_OK);
        if (!g6_is_wine()) CHECK(ssa == NULL);
    }
    g6_close(&g);
}
TEST(gap6_str_analyse_size_matches_text_extent) {
    NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(ScriptString_pSize); NEED(GetTextExtentPoint32W); NEED(GetTextMetricsW);
    G6_OPEN(g, 256, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    const WCHAR *s = L"Hello World";
    EXPECT_HR(sa_make(&g, s, 11, -1, SSA_STD, 0, &ssa), S_OK);
    SIZE sz; CHECK(U(GetTextExtentPoint32W)(g.dc, s, 11, &sz));
    TEXTMETRICW tm; CHECK(U(GetTextMetricsW)(g.dc, &tm));
    const SIZE *ps = ssa ? U(ScriptString_pSize)(ssa) : NULL;
    if (ps) {
        t_note("pSize %ldx%ld ext %ldx%ld tmHeight %ld", ps->cx, ps->cy, sz.cx, sz.cy, tm.tmHeight);
        CHECK(ps->cx >= sz.cx - 6 && ps->cx <= sz.cx + 6);
        CHECK(ps->cy == tm.tmHeight);
    } else CHECK(0);
    if (ssa) U(ScriptStringFree)(&ssa);
    g6_close(&g);
}
TEST(gap6_str_analyse_ansi_unicode_parity) {
    NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(ScriptString_pSize); NEED(ScriptString_pcOutChars);
    WINE_SKIP("ANSI strings rejected");
    G6_OPEN(g, 256, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS w = NULL, a = NULL;
    EXPECT_HR(sa_make(&g, L"Hello, World", 12, -1, SSA_STD, 0, &w), S_OK);
    EXPECT_HR(sa_make(&g, "Hello, World", 12, ANSI_CHARSET, SSA_STD, 0, &a), S_OK);
    if (w && a) {
        CHECK(U(ScriptString_pSize)(w)->cx == U(ScriptString_pSize)(a)->cx);
        CHECK(U(ScriptString_pSize)(w)->cy == U(ScriptString_pSize)(a)->cy);
        CHECK(*U(ScriptString_pcOutChars)(w) == *U(ScriptString_pcOutChars)(a));
    } else CHECK(0);
    if (w) U(ScriptStringFree)(&w);
    if (a) U(ScriptStringFree)(&a);
    g6_close(&g);
}
TEST(gap6_str_analyse_ansi_high_char_parity) {
    NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(ScriptString_pSize);
    WINE_SKIP("ANSI strings rejected");
    G6_OPEN(g, 256, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS w = NULL, a = NULL;
    EXPECT_HR(sa_make(&g, L"caf\x00e9", 4, -1, SSA_STD, 0, &w), S_OK);
    EXPECT_HR(sa_make(&g, "caf\xe9", 4, ANSI_CHARSET, SSA_STD, 0, &a), S_OK);
    if (w && a) CHECK(U(ScriptString_pSize)(w)->cx == U(ScriptString_pSize)(a)->cx);
    else CHECK(0);
    if (w) U(ScriptStringFree)(&w);
    if (a) U(ScriptStringFree)(&a);
    g6_close(&g);
}
TEST(gap6_str_analyse_single_char) {
    NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(ScriptString_pcOutChars);
    G6_OPEN(g, 128, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"x", 1, -1, SSA_STD, 0, &ssa), S_OK);
    if (ssa) { CHECK(*U(ScriptString_pcOutChars)(ssa) == 1); U(ScriptStringFree)(&ssa); } else CHECK(0);
    g6_close(&g);
}
TEST(gap6_str_analyse_long_string) {
    NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(ScriptString_pcOutChars); NEED(ScriptString_pSize);
    G6_OPEN(g, 64, 40, L"Arial", 14);
    static WCHAR buf[2049];
    for (int i = 0; i < 2048; i++) buf[i] = L'a' + (i % 26);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, buf, 2048, -1, SSA_STD, 0, &ssa), S_OK);
    if (ssa) {
        CHECK(*U(ScriptString_pcOutChars)(ssa) == 2048);
        CHECK(U(ScriptString_pSize)(ssa)->cx > 2048);
        U(ScriptStringFree)(&ssa);
    } else CHECK(0);
    g6_close(&g);
}
TEST(gap6_str_analyse_clip_limits_out_chars) {
    NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(ScriptString_pcOutChars); NEED(ScriptString_pSize);
    WINE_SKIP("SSA_CLIP unsupported");
    G6_OPEN(g, 256, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    const WCHAR *s = L"Hello World Hello World";
    EXPECT_HR(sa_make(&g, s, 23, -1, SSA_STD | SSA_CLIP, 40, &ssa), S_OK);
    if (ssa) {
        int c = *U(ScriptString_pcOutChars)(ssa);
        t_note("SSA_CLIP reqw 40: cOutChars=%d cx=%ld", c, U(ScriptString_pSize)(ssa)->cx);
        CHECK(c >= 0 && c < 23);
        CHECK(U(ScriptString_pSize)(ssa)->cx <= 40 + 2);
        U(ScriptStringFree)(&ssa);
    } else CHECK(0);
    g6_close(&g);
}
TEST(gap6_str_analyse_no_clip_keeps_all) {
    NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(ScriptString_pcOutChars);
    G6_OPEN(g, 256, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"Hello World Hello World", 23, -1, SSA_STD, 40, &ssa), S_OK);
    if (ssa) { CHECK(*U(ScriptString_pcOutChars)(ssa) == 23); U(ScriptStringFree)(&ssa); } else CHECK(0);
    g6_close(&g);
}
TEST(gap6_str_analyse_password_hides_content) {
    NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(ScriptString_pSize);
    WINE_SKIP("SSA_PASSWORD unsupported");
    G6_OPEN(g, 256, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS a = NULL, b = NULL;
    EXPECT_HR(sa_make(&g, L"iii", 3, -1, SSA_STD | SSA_PASSWORD, 0, &a), S_OK);
    EXPECT_HR(sa_make(&g, L"WWW", 3, -1, SSA_STD | SSA_PASSWORD, 0, &b), S_OK);
    if (a && b) {
        CHECKF(U(ScriptString_pSize)(a)->cx == U(ScriptString_pSize)(b)->cx, "password widths %ld vs %ld",
               U(ScriptString_pSize)(a)->cx, U(ScriptString_pSize)(b)->cx);
    } else CHECK(0);
    if (a) U(ScriptStringFree)(&a);
    if (b) U(ScriptStringFree)(&b);
    /* without the flag the widths differ */
    EXPECT_HR(sa_make(&g, L"iii", 3, -1, SSA_STD, 0, &a), S_OK);
    EXPECT_HR(sa_make(&g, L"WWW", 3, -1, SSA_STD, 0, &b), S_OK);
    if (a && b) CHECK(U(ScriptString_pSize)(a)->cx != U(ScriptString_pSize)(b)->cx);
    if (a) U(ScriptStringFree)(&a);
    if (b) U(ScriptStringFree)(&b);
    g6_close(&g);
}
TEST(gap6_str_analyse_cglyphs_small) {
    NEED(ScriptStringAnalyse); NEED(ScriptStringFree);
    G6_OPEN(g, 128, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    static const int cg[] = { 0, 1, 2, -1 };
    for (int i = 0; i < ARRSZ(cg); i++) {
        ssa = NULL;
        HRESULT hr = U(ScriptStringAnalyse)(g.dc, L"Hello", 5, cg[i], -1, SSA_STD, 0, NULL, NULL, NULL, NULL, NULL, &ssa);
        t_note("cGlyphs=%d -> 0x%08lx", cg[i], (unsigned long)hr);
        CHECK(SUCCEEDED(hr) ? ssa != NULL : ssa == NULL);
        if (ssa) U(ScriptStringFree)(&ssa);
    }
    g6_close(&g);
}
TEST(gap6_str_analyse_repeat_no_gdi_leak) {
    NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(GetGuiResources);
    G6_OPEN(g, 128, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    sa_make(&g, L"warm up", 7, -1, SSA_STD, 0, &ssa); U(ScriptStringFree)(&ssa);
    DWORD before = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    for (int i = 0; i < 200; i++) {
        ssa = NULL;
        if (sa_make(&g, L"leak check text", 15, -1, SSA_STD | SSA_BREAK, 0, &ssa) != S_OK) { CHECK(0); break; }
        U(ScriptStringFree)(&ssa);
    }
    DWORD after = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    CHECKF(after <= before + 2, "GDI objects %lu -> %lu", before, after);
    g6_close(&g);
}
TEST(gap6_str_analyse_bidi_mixed) {
    NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(ScriptString_pcOutChars); NEED(ScriptString_pSize);
    G6_OPEN(g, 256, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    const WCHAR *s = L"ab \x05d0\x05d1\x05d2 cd";
    EXPECT_HR(sa_make(&g, s, 9, -1, SSA_STD, 0, &ssa), S_OK);
    if (ssa) {
        CHECK(*U(ScriptString_pcOutChars)(ssa) == 9);
        CHECK(U(ScriptString_pSize)(ssa)->cx > 0);
        U(ScriptStringFree)(&ssa);
    } else CHECK(0);
    g6_close(&g);
}

/* ---------------- ScriptStringFree / Validate / accessors ---------------- */
TEST(gap6_str_free_invalid) {
    NEED(ScriptStringFree);
    WINE_SKIP("no NULL validation");
    EXPECT_HR(U(ScriptStringFree)(NULL), HR_INVALIDARG);
    SCRIPT_STRING_ANALYSIS none = NULL;
    EXPECT_HR(U(ScriptStringFree)(&none), HR_INVALIDARG);
    CHECK(none == NULL);
}
TEST(gap6_str_free_twice) {
    NEED(ScriptStringAnalyse); NEED(ScriptStringFree);
    WINE_SKIP("free does not clear handle");
    G6_OPEN(g, 128, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"twice", 5, -1, SSA_STD, 0, &ssa), S_OK);
    EXPECT_HR(U(ScriptStringFree)(&ssa), S_OK);
    CHECK(ssa == NULL);
    EXPECT_HR(U(ScriptStringFree)(&ssa), HR_INVALIDARG);
    g6_close(&g);
}
TEST(gap6_str_validate_invalid) {
    NEED(ScriptStringValidate);
    WINE_SKIP("no NULL validation");
    EXPECT_HR(U(ScriptStringValidate)(NULL), HR_INVALIDARG);
}
TEST(gap6_str_validate_fresh) {
    NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(ScriptStringValidate);
    G6_OPEN(g, 128, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"valid", 5, -1, SSA_STD, 0, &ssa), S_OK);
    if (ssa) {
        EXPECT_HR(U(ScriptStringValidate)(ssa), S_OK);
        EXPECT_HR(U(ScriptStringValidate)(ssa), S_OK);
        U(ScriptStringFree)(&ssa);
    } else CHECK(0);
    g6_close(&g);
}
TEST(gap6_str_validate_without_glyph_flags) {
    NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(ScriptStringValidate);
    G6_OPEN(g, 128, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    HRESULT hr = sa_make(&g, L"valid", 5, -1, 0, 0, &ssa);
    if (SUCCEEDED(hr) && ssa) {
        HRESULT v = U(ScriptStringValidate)(ssa);
        t_note("Validate on flag-less analysis -> 0x%08lx", (unsigned long)v);
        CHECK(v == S_OK || v == S_FALSE || v == HR_INVALIDARG);
        U(ScriptStringFree)(&ssa);
    } else t_note("flag-less analyse -> 0x%08lx", (unsigned long)hr);
    g6_close(&g);
}
TEST(gap6_str_accessors_null) {
    NEED(ScriptString_pSize); NEED(ScriptString_pcOutChars); NEED(ScriptString_pLogAttr);
    CHECK(U(ScriptString_pSize)(NULL) == NULL);
    CHECK(U(ScriptString_pcOutChars)(NULL) == NULL);
    CHECK(U(ScriptString_pLogAttr)(NULL) == NULL);
}
TEST(gap6_str_accessors_stable_pointers) {
    NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(ScriptString_pSize); NEED(ScriptString_pcOutChars); NEED(ScriptString_pLogAttr);
    G6_OPEN(g, 128, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"stable", 6, -1, SSA_STD | SSA_BREAK, 0, &ssa), S_OK);
    if (ssa) {
        CHECK(U(ScriptString_pSize)(ssa) == U(ScriptString_pSize)(ssa));
        CHECK(U(ScriptString_pcOutChars)(ssa) == U(ScriptString_pcOutChars)(ssa));
        CHECK(U(ScriptString_pLogAttr)(ssa) == U(ScriptString_pLogAttr)(ssa));
        CHECK(U(ScriptString_pSize)(ssa) != NULL);
        U(ScriptStringFree)(&ssa);
    } else CHECK(0);
    g6_close(&g);
}
TEST(gap6_str_logattr_with_break_flag) {
    NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(ScriptString_pLogAttr);
    G6_OPEN(g, 256, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"Hello world", 11, -1, SSA_STD | SSA_BREAK, 0, &ssa), S_OK);
    if (ssa) {
        const SCRIPT_LOGATTR *la = U(ScriptString_pLogAttr)(ssa);
        CHECK(la != NULL);
        if (la) {
            for (int i = 0; i < 11; i++) CHECKF(la[i].fCharStop == 1, "fCharStop[%d]", i);
            for (int i = 0; i < 11; i++) CHECKF(la[i].fWhiteSpace == (i == 5), "fWhiteSpace[%d]", i);
            CHECK(la[6].fSoftBreak == 1);
        }
        U(ScriptStringFree)(&ssa);
    } else CHECK(0);
    g6_close(&g);
}
TEST(gap6_str_logattr_without_break_flag) {
    NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(ScriptString_pLogAttr);
    G6_OPEN(g, 256, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"Hello world", 11, -1, SSA_STD, 0, &ssa), S_OK);
    if (ssa) {
        const SCRIPT_LOGATTR *la = U(ScriptString_pLogAttr)(ssa);
        t_note("pLogAttr without SSA_BREAK = %p", la);
        U(ScriptStringFree)(&ssa);
    }
    g6_close(&g);
}

/* ---------------- ScriptStringGetLogicalWidths ---------------- */
TEST(gap6_str_logwidths_invalid) {
    NEED(ScriptStringGetLogicalWidths); NEED(ScriptStringAnalyse); NEED(ScriptStringFree);
    WINE_SKIP("no NULL validation");
    int dx[8];
    EXPECT_HR(U(ScriptStringGetLogicalWidths)(NULL, dx), HR_INVALIDARG);
    G6_OPEN(g, 128, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"abc", 3, -1, SSA_STD, 0, &ssa), S_OK);
    if (ssa) {
        EXPECT_HR(U(ScriptStringGetLogicalWidths)(ssa, NULL), HR_INVALIDARG);
        U(ScriptStringFree)(&ssa);
    }
    g6_close(&g);
}
TEST(gap6_str_logwidths_sum_matches_size) {
    NEED(ScriptStringGetLogicalWidths); NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(ScriptString_pSize);
    G6_OPEN(g, 256, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"Hello", 5, -1, SSA_STD, 0, &ssa), S_OK);
    if (ssa) {
        int dx[8] = {0}; int sum = 0;
        EXPECT_HR(U(ScriptStringGetLogicalWidths)(ssa, dx), S_OK);
        for (int i = 0; i < 5; i++) { CHECKF(dx[i] > 0, "dx[%d]=%d", i, dx[i]); sum += dx[i]; }
        CHECKF(sum == U(ScriptString_pSize)(ssa)->cx, "sum %d cx %ld", sum, U(ScriptString_pSize)(ssa)->cx);
        CHECK(dx[5] == 0);
        U(ScriptStringFree)(&ssa);
    } else CHECK(0);
    g6_close(&g);
}
TEST(gap6_str_logwidths_space_vs_wide) {
    NEED(ScriptStringGetLogicalWidths); NEED(ScriptStringAnalyse); NEED(ScriptStringFree);
    G6_OPEN(g, 256, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"iW", 2, -1, SSA_STD, 0, &ssa), S_OK);
    if (ssa) {
        int dx[4] = {0};
        EXPECT_HR(U(ScriptStringGetLogicalWidths)(ssa, dx), S_OK);
        CHECK(dx[0] < dx[1]);
        U(ScriptStringFree)(&ssa);
    } else CHECK(0);
    g6_close(&g);
}

/* ---------------- ScriptStringCPtoX ---------------- */
TEST(gap6_str_cptox_invalid) {
    NEED(ScriptStringCPtoX); NEED(ScriptStringAnalyse); NEED(ScriptStringFree);
    WINE_SKIP("no NULL validation");
    int x = 77;
    EXPECT_HR(U(ScriptStringCPtoX)(NULL, 0, FALSE, &x), HR_INVALIDARG);
    CHECK(x == 77);
    G6_OPEN(g, 128, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"abc", 3, -1, SSA_STD, 0, &ssa), S_OK);
    if (ssa) {
        EXPECT_HR(U(ScriptStringCPtoX)(ssa, 0, FALSE, NULL), HR_INVALIDARG);
        U(ScriptStringFree)(&ssa);
    }
    g6_close(&g);
}
TEST(gap6_str_cptox_monotone_and_total) {
    NEED(ScriptStringCPtoX); NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(ScriptString_pSize);
    G6_OPEN(g, 256, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"Hello", 5, -1, SSA_STD, 0, &ssa), S_OK);
    if (ssa) {
        int x0 = -1;
        EXPECT_HR(U(ScriptStringCPtoX)(ssa, 0, FALSE, &x0), S_OK);
        CHECK(x0 == 0);
        int prev = 0;
        for (int i = 0; i < 5; i++) {
            int xl = -1, xt = -1;
            EXPECT_HR(U(ScriptStringCPtoX)(ssa, i, FALSE, &xl), S_OK);
            EXPECT_HR(U(ScriptStringCPtoX)(ssa, i, TRUE, &xt), S_OK);
            CHECKF(xl >= prev, "leading[%d]=%d prev %d", i, xl, prev);
            CHECKF(xt > xl, "trailing[%d]=%d leading %d", i, xt, xl);
            prev = xt;
            if (i < 4) {
                int xn = -1;
                EXPECT_HR(U(ScriptStringCPtoX)(ssa, i + 1, FALSE, &xn), S_OK);
                CHECKF(xn == xt, "leading[%d]=%d trailing[%d]=%d", i + 1, xn, i, xt);
            }
        }
        CHECKF(prev == U(ScriptString_pSize)(ssa)->cx, "end %d cx %ld", prev, U(ScriptString_pSize)(ssa)->cx);
        U(ScriptStringFree)(&ssa);
    } else CHECK(0);
    g6_close(&g);
}
TEST(gap6_str_cptox_matches_logical_widths) {
    NEED(ScriptStringCPtoX); NEED(ScriptStringGetLogicalWidths); NEED(ScriptStringAnalyse); NEED(ScriptStringFree);
    G6_OPEN(g, 256, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"Widths", 6, -1, SSA_STD, 0, &ssa), S_OK);
    if (ssa) {
        int dx[8] = {0}, acc = 0;
        EXPECT_HR(U(ScriptStringGetLogicalWidths)(ssa, dx), S_OK);
        for (int i = 0; i < 6; i++) {
            int xl = -1;
            EXPECT_HR(U(ScriptStringCPtoX)(ssa, i, FALSE, &xl), S_OK);
            CHECKF(xl == acc, "cp %d x %d expected %d", i, xl, acc);
            acc += dx[i];
        }
        U(ScriptStringFree)(&ssa);
    } else CHECK(0);
    g6_close(&g);
}

/* ---------------- ScriptStringGetOrder ---------------- */
TEST(gap6_str_order_invalid) {
    NEED(ScriptStringGetOrder); NEED(ScriptStringAnalyse); NEED(ScriptStringFree);
    WINE_SKIP("no NULL validation");
    UINT ord[8];
    EXPECT_HR(U(ScriptStringGetOrder)(NULL, ord), HR_INVALIDARG);
    G6_OPEN(g, 128, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"abc", 3, -1, SSA_STD, 0, &ssa), S_OK);
    if (ssa) {
        EXPECT_HR(U(ScriptStringGetOrder)(ssa, NULL), HR_INVALIDARG);
        U(ScriptStringFree)(&ssa);
    }
    g6_close(&g);
}
TEST(gap6_str_order_requires_glyphs_flag) {
    NEED(ScriptStringGetOrder); NEED(ScriptStringAnalyse); NEED(ScriptStringFree);
    G6_OPEN(g, 128, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    HRESULT hr = sa_make(&g, L"abc", 3, -1, SSA_FALLBACK, 0, &ssa);
    if (SUCCEEDED(hr) && ssa) {
        UINT ord[8];
        HRESULT o = U(ScriptStringGetOrder)(ssa, ord);
        t_note("GetOrder without SSA_GLYPHS -> 0x%08lx", (unsigned long)o);
        if (!g6_is_wine()) CHECK(o == HR_INVALIDARG);
        U(ScriptStringFree)(&ssa);
    } else t_note("analyse without SSA_GLYPHS -> 0x%08lx", (unsigned long)hr);
    g6_close(&g);
}
TEST(gap6_str_order_ltr_identity) {
    NEED(ScriptStringGetOrder); NEED(ScriptStringAnalyse); NEED(ScriptStringFree);
    G6_OPEN(g, 128, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"abcd", 4, -1, SSA_STD, 0, &ssa), S_OK);
    if (ssa) {
        UINT ord[8]; memset(ord, 0xff, sizeof ord);
        EXPECT_HR(U(ScriptStringGetOrder)(ssa, ord), S_OK);
        for (UINT i = 0; i < 4; i++) CHECKF(ord[i] == i, "ord[%u]=%u", i, ord[i]);
        CHECK(ord[4] == 0xffffffffu);
        U(ScriptStringFree)(&ssa);
    } else CHECK(0);
    g6_close(&g);
}
TEST(gap6_str_order_rtl_reversed) {
    NEED(ScriptStringGetOrder); NEED(ScriptStringAnalyse); NEED(ScriptStringFree);
    WINE_SKIP("no visual reordering");
    G6_OPEN(g, 128, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"\x05d0\x05d1\x05d2", 3, -1, SSA_STD | SSA_RTL, 0, &ssa), S_OK);
    if (ssa) {
        UINT ord[8]; memset(ord, 0xff, sizeof ord);
        EXPECT_HR(U(ScriptStringGetOrder)(ssa, ord), S_OK);
        CHECKF(ord[0] == 2 && ord[1] == 1 && ord[2] == 0, "ord %u %u %u", ord[0], ord[1], ord[2]);
        U(ScriptStringFree)(&ssa);
    } else CHECK(0);
    g6_close(&g);
}
TEST(gap6_str_order_mixed_bidi) {
    NEED(ScriptStringGetOrder); NEED(ScriptStringAnalyse); NEED(ScriptStringFree);
    WINE_SKIP("no visual reordering");
    G6_OPEN(g, 128, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    /* "ab" + two Hebrew letters: the Hebrew pair is displayed reversed */
    EXPECT_HR(sa_make(&g, L"ab\x05d0\x05d1", 4, -1, SSA_STD, 0, &ssa), S_OK);
    if (ssa) {
        UINT ord[8]; memset(ord, 0xff, sizeof ord);
        EXPECT_HR(U(ScriptStringGetOrder)(ssa, ord), S_OK);
        CHECKF(ord[0] == 0 && ord[1] == 1 && ord[2] == 3 && ord[3] == 2, "ord %u %u %u %u", ord[0], ord[1], ord[2], ord[3]);
        U(ScriptStringFree)(&ssa);
    } else CHECK(0);
    g6_close(&g);
}
TEST(gap6_str_order_is_permutation) {
    NEED(ScriptStringGetOrder); NEED(ScriptStringAnalyse); NEED(ScriptStringFree);
    G6_OPEN(g, 256, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    const WCHAR *s = L"a\x05d0\x05d1 b\x05d2 c";
    EXPECT_HR(sa_make(&g, s, 8, -1, SSA_STD, 0, &ssa), S_OK);
    if (ssa) {
        UINT ord[16]; memset(ord, 0xff, sizeof ord);
        EXPECT_HR(U(ScriptStringGetOrder)(ssa, ord), S_OK);
        int seen[8] = {0};
        for (int i = 0; i < 8; i++) { if (ord[i] < 8) seen[ord[i]]++; else CHECKF(0, "ord[%d]=%u", i, ord[i]); }
        for (int i = 0; i < 8; i++) CHECKF(seen[i] == 1, "logical %d seen %d", i, seen[i]);
        U(ScriptStringFree)(&ssa);
    } else CHECK(0);
    g6_close(&g);
}

/* ---------------- ScriptStringOut ---------------- */
TEST(gap6_str_out_invalid) {
    NEED(ScriptStringOut); NEED(ScriptStringAnalyse); NEED(ScriptStringFree);
    WINE_SKIP("no NULL validation");
    EXPECT_HR(U(ScriptStringOut)(NULL, 0, 0, 0, NULL, 0, 0, FALSE), HR_INVALIDARG);
    G6_OPEN(g, 128, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"abc", 3, -1, SSA_STD, 0, &ssa), S_OK);
    if (ssa) {
        /* ETO_CLIPPED without a rectangle */
        EXPECT_HR(U(ScriptStringOut)(ssa, 0, 0, ETO_CLIPPED, NULL, 0, 0, FALSE), HR_INVALIDARG);
        EXPECT_HR(U(ScriptStringOut)(ssa, 0, 0, ETO_CLIPPED | ETO_OPAQUE, NULL, 0, 0, FALSE), HR_INVALIDARG);
        U(ScriptStringFree)(&ssa);
    }
    g6_close(&g);
}
TEST(gap6_str_out_requires_glyph_analysis) {
    NEED(ScriptStringOut); NEED(ScriptStringAnalyse); NEED(ScriptStringFree);
    G6_OPEN(g, 128, 40, L"Arial", 20);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    HRESULT hr = sa_make(&g, L"abc", 3, -1, SSA_FALLBACK, 0, &ssa);
    if (SUCCEEDED(hr) && ssa) {
        HRESULT o = U(ScriptStringOut)(ssa, 0, 0, 0, NULL, 0, 0, FALSE);
        t_note("ScriptStringOut without SSA_GLYPHS -> 0x%08lx", (unsigned long)o);
        if (!g6_is_wine()) CHECK(o == HR_INVALIDARG);
        U(ScriptStringFree)(&ssa);
    }
    g6_close(&g);
}
TEST(gap6_str_out_draws_within_bounds) {
    NEED(ScriptStringOut); NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(ScriptString_pSize); NEED(SetBkMode);
    G6_OPEN(g, 160, 60, L"Arial", 20);
    U(SetBkMode)(g.dc, TRANSPARENT);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"HHHH", 4, -1, SSA_STD, 0, &ssa), S_OK);
    if (ssa) {
        const SIZE *ps = U(ScriptString_pSize)(ssa);
        int x = 12, y = 10, cx = ps->cx, cy = ps->cy;
        EXPECT_HR(U(ScriptStringOut)(ssa, x, y, 0, NULL, 0, 0, FALSE), S_OK);
        int inside = g6_ink(&g, x - 2, y - 2, x + cx + 2, y + cy + 2);
        int total = g6_ink(&g, 0, 0, g.w, g.h);
        CHECK(inside > 20);
        CHECKF(inside == total, "ink inside %d total %d (box %d,%d %dx%d)", inside, total, x, y, cx, cy);
        U(ScriptStringFree)(&ssa);
    } else CHECK(0);
    g6_close(&g);
}
TEST(gap6_str_out_moves_with_origin) {
    NEED(ScriptStringOut); NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(SetBkMode);
    G6_OPEN(g, 200, 60, L"Arial", 20);
    U(SetBkMode)(g.dc, TRANSPARENT);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"HH", 2, -1, SSA_STD, 0, &ssa), S_OK);
    if (ssa) {
        EXPECT_HR(U(ScriptStringOut)(ssa, 4, 4, 0, NULL, 0, 0, FALSE), S_OK);
        DWORD *first = (DWORD *)malloc((size_t)g.w * g.h * 4);
        memcpy(first, g.bits, (size_t)g.w * g.h * 4);
        g6_clear(&g);
        EXPECT_HR(U(ScriptStringOut)(ssa, 4 + 100, 4, 0, NULL, 0, 0, FALSE), S_OK);
        int same = 1;
        for (int yy = 0; yy < g.h && same; yy++)
            for (int xx = 0; xx < 90; xx++)
                if (first[yy * g.w + xx] != g.bits[yy * g.w + xx + 100]) { same = 0; break; }
        CHECK(same);
        free(first);
        U(ScriptStringFree)(&ssa);
    } else CHECK(0);
    g6_close(&g);
}
TEST(gap6_str_out_opaque_fills_rect) {
    NEED(ScriptStringOut); NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(SetBkMode); NEED(SetBkColor);
    WINE_SKIP("ETO_OPAQUE fill incomplete");
    G6_OPEN(g, 160, 60, L"Arial", 20);
    U(SetBkMode)(g.dc, OPAQUE);
    U(SetBkColor)(g.dc, RGB(255, 0, 0));
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"HHH", 3, -1, SSA_STD, 0, &ssa), S_OK);
    if (ssa) {
        RECT rc = { 20, 10, 120, 50 };
        EXPECT_HR(U(ScriptStringOut)(ssa, 24, 14, ETO_OPAQUE, &rc, 0, 0, FALSE), S_OK);
        int in = g6_ink(&g, rc.left, rc.top, rc.right, rc.bottom);
        int all = g6_ink(&g, 0, 0, g.w, g.h);
        CHECKF(in == 100 * 40, "opaque rect ink %d want %d", in, 100 * 40);
        CHECKF(all == in, "ink outside rect: total %d inside %d", all, in);
        U(ScriptStringFree)(&ssa);
    } else CHECK(0);
    g6_close(&g);
}
TEST(gap6_str_out_clipped_to_rect) {
    NEED(ScriptStringOut); NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(SetBkMode);
    WINE_SKIP("ETO_CLIPPED not honoured");
    G6_OPEN(g, 200, 60, L"Arial", 24);
    U(SetBkMode)(g.dc, TRANSPARENT);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"HHHHHH", 6, -1, SSA_STD, 0, &ssa), S_OK);
    if (ssa) {
        RECT rc = { 30, 10, 60, 40 };
        EXPECT_HR(U(ScriptStringOut)(ssa, 10, 10, ETO_CLIPPED, &rc, 0, 0, FALSE), S_OK);
        int in = g6_ink(&g, rc.left, rc.top, rc.right, rc.bottom);
        int all = g6_ink(&g, 0, 0, g.w, g.h);
        CHECK(in > 10);
        CHECKF(all == in, "ink leaked outside clip: total %d inside %d", all, in);
        U(ScriptStringFree)(&ssa);
    } else CHECK(0);
    g6_close(&g);
}
TEST(gap6_str_out_offscreen_origin_is_safe) {
    NEED(ScriptStringOut); NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(SetBkMode);
    G6_OPEN(g, 100, 40, L"Arial", 20);
    U(SetBkMode)(g.dc, TRANSPARENT);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"Offscreen", 9, -1, SSA_STD, 0, &ssa), S_OK);
    if (ssa) {
        static const int pos[][2] = { { -500, -500 }, { 1000, 1000 }, { -20, 5 }, { 90, 30 }, { 0x3fffffff, 0 }, { 0, -0x3fffffff } };
        for (int i = 0; i < ARRSZ(pos); i++) {
            HRESULT hr = U(ScriptStringOut)(ssa, pos[i][0], pos[i][1], 0, NULL, 0, 0, FALSE);
            CHECKF(hr == S_OK, "pos %d,%d -> 0x%08lx", pos[i][0], pos[i][1], (unsigned long)hr);
        }
        U(ScriptStringFree)(&ssa);
    } else CHECK(0);
    g6_close(&g);
}
TEST(gap6_str_out_selection_and_disabled_stay_in_bounds) {
    NEED(ScriptStringOut); NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(ScriptString_pSize); NEED(SetBkMode);
    G6_OPEN(g, 200, 60, L"Arial", 20);
    U(SetBkMode)(g.dc, TRANSPARENT);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"Select", 6, -1, SSA_STD, 0, &ssa), S_OK);
    if (ssa) {
        const SIZE *ps = U(ScriptString_pSize)(ssa);
        EXPECT_HR(U(ScriptStringOut)(ssa, 10, 10, 0, NULL, 1, 4, FALSE), S_OK);
        CHECK(g6_ink(&g, 0, 0, g.w, g.h) == g6_ink(&g, 8, 8, 10 + ps->cx + 3, 10 + ps->cy + 3));
        g6_clear(&g);
        EXPECT_HR(U(ScriptStringOut)(ssa, 10, 10, 0, NULL, 0, 0, TRUE), S_OK);
        int in = g6_ink(&g, 8, 8, 10 + ps->cx + 4, 10 + ps->cy + 4);
        CHECK(in > 0);
        CHECK(in == g6_ink(&g, 0, 0, g.w, g.h));
        g6_clear(&g);
        EXPECT_HR(U(ScriptStringOut)(ssa, 10, 10, 0, NULL, 4, 1, FALSE), S_OK);   /* min > max: no selection */
        U(ScriptStringFree)(&ssa);
    } else CHECK(0);
    g6_close(&g);
}
TEST(gap6_str_out_repeatable_same_pixels) {
    NEED(ScriptStringOut); NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(SetBkMode);
    G6_OPEN(g, 160, 50, L"Arial", 20);
    U(SetBkMode)(g.dc, TRANSPARENT);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    EXPECT_HR(sa_make(&g, L"Same", 4, -1, SSA_STD, 0, &ssa), S_OK);
    if (ssa) {
        EXPECT_HR(U(ScriptStringOut)(ssa, 5, 5, 0, NULL, 0, 0, FALSE), S_OK);
        DWORD *a = (DWORD *)malloc((size_t)g.w * g.h * 4);
        memcpy(a, g.bits, (size_t)g.w * g.h * 4);
        g6_clear(&g);
        EXPECT_HR(U(ScriptStringOut)(ssa, 5, 5, 0, NULL, 0, 0, FALSE), S_OK);
        CHECK(memcmp(a, g.bits, (size_t)g.w * g.h * 4) == 0);
        free(a);
        U(ScriptStringFree)(&ssa);
    } else CHECK(0);
    g6_close(&g);
}
TEST(gap6_str_out_bitmap_font) {
    NEED(ScriptStringOut); NEED(ScriptStringAnalyse); NEED(ScriptStringFree); NEED(SetBkMode);
    G6_OPEN(g, 160, 50, L"Terminal", 12);
    U(SetBkMode)(g.dc, TRANSPARENT);
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    HRESULT hr = sa_make(&g, L"Bitmap", 6, -1, SSA_STD, 0, &ssa);
    t_note("analyse on bitmap font -> 0x%08lx", (unsigned long)hr);
    if (SUCCEEDED(hr) && ssa) {
        HRESULT o = U(ScriptStringOut)(ssa, 5, 5, 0, NULL, 0, 0, FALSE);
        t_note("ScriptStringOut on bitmap font -> 0x%08lx", (unsigned long)o);
        CHECK(SUCCEEDED(o));
        if (SUCCEEDED(o)) CHECK(g6_ink(&g, 0, 0, g.w, g.h) > 0);
        U(ScriptStringFree)(&ssa);
    }
    g6_close(&g);
}
