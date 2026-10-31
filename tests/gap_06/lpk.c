#include "gap6.h"
/* Language Pack (Lpk*) entry points exported by gdi32full.dll and forwarded
 * from gdi32.dll.  These are called by user32 for complex-script text; only
 * the calls whose behaviour was established from the implementation are made:
 * early-out paths, DC-only helpers and a plain text draw on a DIB DC.
 * LpkTabbedTextOut (twelve undocumented arguments) is only resolved. */

#define NEED(f) REQUIRE(t_proc_gdi32full(#f))

typedef int (WINAPI *PSM_fn)(HDC, int, int, const WCHAR *, int, UINT);
typedef int (WINAPI *GDW_fn)(HDC, const void *, int, UINT, int);
typedef INT_PTR (WINAPI *PRES_fn)(void);

static FARPROC lpk_fn(const char *n) { return t_proc_gdi32full(n); }

TEST(gap6_lpk_exports_resolve) {
    WINE_SKIP("export layout differs (no gdi32full.dll)");
    static const char *names[] = { "LpkPresent", "LpkPSMTextOut", "LpkTabbedTextOut", "LpkUseGDIWidthCache",
                                   "LpkDrawTextEx", "LpkExtTextOut", "LpkGetCharacterPlacement",
                                   "LpkGetTextExtentExPoint", "LpkInitialize" };
    for (int i = 0; i < ARRSZ(names); i++) {
        FARPROC f = t_proc_gdi32full(names[i]);
        CHECKF(f != NULL, "gdi32full!%s missing", names[i]);
    }
}
TEST(gap6_lpk_gdi32_exports_resolve) {
    WINE_SKIP("export layout differs (no gdi32full.dll)");
    static const char *names[] = { "LpkPresent", "LpkPSMTextOut", "LpkTabbedTextOut", "LpkUseGDIWidthCache" };
    for (int i = 0; i < ARRSZ(names); i++) {
        FARPROC f = t_proc_gdi32(names[i]);
        CHECKF(f != NULL, "gdi32!%s missing", names[i]);
    }
}
TEST(gap6_lpk_gdi32_forwards_to_gdi32full) {
    static const char *names[] = { "LpkPresent", "LpkPSMTextOut", "LpkTabbedTextOut", "LpkUseGDIWidthCache" };
    for (int i = 0; i < ARRSZ(names); i++) {
        FARPROC a = t_proc_gdi32(names[i]), b = t_proc_gdi32full(names[i]);
        if (!a || !b) continue;
        if (a != b) t_note("%s: gdi32 %p differs from gdi32full %p", names[i], a, b);
        CHECKF(a == b || g6_is_wine(), "%s does not forward to gdi32full", names[i]);
    }
}
TEST(gap6_lpk_script_exports_in_both_modules) {
    WINE_SKIP("export layout differs (no gdi32full.dll)");
    static const char *names[] = {
        "ScriptApplyDigitSubstitution", "ScriptApplyLogicalWidth", "ScriptBreak", "ScriptCPtoX",
        "ScriptCacheGetHeight", "ScriptFreeCache", "ScriptGetCMap", "ScriptGetFontAlternateGlyphs",
        "ScriptGetFontFeatureTags", "ScriptGetFontLanguageTags", "ScriptGetFontProperties",
        "ScriptGetFontScriptTags", "ScriptGetGlyphABCWidth", "ScriptGetLogicalWidths", "ScriptGetProperties",
        "ScriptIsComplex", "ScriptItemize", "ScriptItemizeOpenType", "ScriptJustify", "ScriptLayout",
        "ScriptPlace", "ScriptPlaceOpenType", "ScriptPositionSingleGlyph", "ScriptRecordDigitSubstitution",
        "ScriptShape", "ScriptShapeOpenType", "ScriptStringAnalyse", "ScriptStringCPtoX", "ScriptStringFree",
        "ScriptStringGetLogicalWidths", "ScriptStringGetOrder", "ScriptStringOut", "ScriptString_pcOutChars",
        "ScriptString_pLogAttr", "ScriptString_pSize", "ScriptStringValidate",
    };
    for (int i = 0; i < ARRSZ(names); i++) {
        FARPROC a = t_proc_gdi32(names[i]), b = t_proc_gdi32full(names[i]);
        CHECKF(a != NULL, "gdi32!%s missing", names[i]);
        CHECKF(b != NULL, "gdi32full!%s missing", names[i]);
        if (a && b && a != b) t_note("%s: gdi32 %p vs gdi32full %p", names[i], a, b);
    }
}

TEST(gap6_lpk_present_harmless) {
    FARPROC f = lpk_fn("LpkPresent");
    REQUIRE(f);
    if (g6_is_wine()) { t_skip("wine: stub differs"); return; }
    /* the Windows 10 export is an empty routine (ret 0); no arguments are consumed */
    ((PRES_fn)f)();
    CHECK(1);
}

TEST(gap6_lpk_psm_zero_count_or_null_string) {
    PSM_fn f = (PSM_fn)lpk_fn("LpkPSMTextOut");
    REQUIRE(f);
    G6_OPEN(g, 128, 40, L"Arial", 20);
    CHECK(f(g.dc, 0, 0, L"abc", 0, 0) == 0);
    CHECK(f(g.dc, 0, 0, NULL, 3, 0) == 0);
    CHECK(f(g.dc, 0, 0, NULL, 0, 0) == 0);
    /* both early-outs fire before the DC is examined */
    CHECK(f(NULL, 0, 0, L"abc", 0, 0) == 0);
    CHECK(f(NULL, 0, 0, NULL, 5, 0) == 0);
    CHECK(f((HDC)(ULONG_PTR)0x1234, 1, 2, L"abc", 0, 0xffffffffu) == 0);
    CHECK(g6_ink(&g, 0, 0, g.w, g.h) == 0);
    g6_close(&g);
}
TEST(gap6_lpk_psm_draws_and_returns_width) {
    NEED(LpkPSMTextOut); REQUIRE(U(SetBkMode)); REQUIRE(U(GetTextExtentPoint32W));
    PSM_fn f = (PSM_fn)lpk_fn("LpkPSMTextOut");
    G6_OPEN(g, 200, 50, L"Arial", 20);
    U(SetBkMode)(g.dc, TRANSPARENT);
    int w = f(g.dc, 10, 8, L"HHHH", 4, 0);
    SIZE sz; CHECK(U(GetTextExtentPoint32W)(g.dc, L"HHHH", 4, &sz));
    t_note("LpkPSMTextOut width %d, GetTextExtentPoint32W %ld", w, sz.cx);
    if (w == 0) { t_note("language pack analysis unavailable, nothing drawn"); g6_close(&g); return; }
    CHECK(w > 0);
    CHECK(w >= sz.cx - 6 && w <= sz.cx + 6);
    int in = g6_ink(&g, 8, 6, 10 + w + 2, 8 + sz.cy + 2);
    CHECK(in > 20);
    CHECKF(in == g6_ink(&g, 0, 0, g.w, g.h), "ink outside text box");
    g6_close(&g);
}
TEST(gap6_lpk_psm_single_char_and_repeat) {
    NEED(LpkPSMTextOut);
    PSM_fn f = (PSM_fn)lpk_fn("LpkPSMTextOut");
    G6_OPEN(g, 100, 50, L"Arial", 20);
    int w1 = f(g.dc, 0, 0, L"W", 1, 0);
    if (w1 == 0) { t_note("language pack analysis unavailable"); g6_close(&g); return; }
    CHECK(w1 > 0);
    for (int i = 0; i < 50; i++) {
        int w = f(g.dc, 0, 0, L"W", 1, 0);
        if (w != w1) { CHECKF(0, "iteration %d width %d vs %d", i, w, w1); break; }
    }
    g6_close(&g);
}
TEST(gap6_lpk_psm_width_scales_with_length) {
    NEED(LpkPSMTextOut);
    PSM_fn f = (PSM_fn)lpk_fn("LpkPSMTextOut");
    G6_OPEN(g, 300, 50, L"Arial", 20);
    int w2 = f(g.dc, 0, 0, L"HH", 2, 0), w4 = f(g.dc, 0, 0, L"HHHH", 4, 0);
    if (w2 == 0) { t_note("language pack analysis unavailable"); g6_close(&g); return; }
    CHECKF(w4 == 2 * w2, "w4 %d w2 %d", w4, w2);
    g6_close(&g);
}

TEST(gap6_lpk_gdiwidthcache_layout_mismatch_returns_zero) {
    NEED(LpkUseGDIWidthCache); REQUIRE(U(SetLayout)); REQUIRE(U(GetLayout));
    GDW_fn f = (GDW_fn)lpk_fn("LpkUseGDIWidthCache");
    if (g6_is_wine()) { t_skip("wine: not implemented"); return; }
    G6_OPEN(g, 64, 32, L"Arial", 16);
    /* DC layout (LTR) differs from the layout bit in the flags (bit 8): never usable */
    CHECK(f(g.dc, NULL, 3, 0x100, 1) == 0);
    CHECK(f(g.dc, NULL, 3, 0x100, 0 + 1) == 0);
    DWORD old = U(SetLayout)(g.dc, LAYOUT_RTL);
    if (old != GDI_ERROR) {
        CHECK(U(GetLayout)(g.dc) & LAYOUT_RTL);
        CHECK(f(g.dc, NULL, 3, 0, 1) == 0);
        CHECK(f(g.dc, NULL, 1, 0, 1) == 0);
        int m = f(g.dc, NULL, 3, 0x100, 1);
        CHECK(m == 0 || m == 1);
        U(SetLayout)(g.dc, old);
    } else t_note("SetLayout failed on memory DC");
    g6_close(&g);
}
TEST(gap6_lpk_gdiwidthcache_matching_layout_boolean) {
    NEED(LpkUseGDIWidthCache);
    GDW_fn f = (GDW_fn)lpk_fn("LpkUseGDIWidthCache");
    if (g6_is_wine()) { t_skip("wine: not implemented"); return; }
    G6_OPEN(g, 64, 32, L"Arial", 16);
    int r = f(g.dc, NULL, 3, 0, 1);
    t_note("LpkUseGDIWidthCache(matching layout, Arial) = %d", r);
    CHECK(r == 0 || r == 1);
    int r2 = f(g.dc, NULL, 3, 0, 1);
    CHECK(r == r2);
    g6_close(&g);
}
TEST(gap6_lpk_tabbedtextout_resolved_only) {
    FARPROC f = lpk_fn("LpkTabbedTextOut");
    REQUIRE(f);
    CHECK(f != NULL);
    /* signature has twelve parameters and is not documented; calling it with
     * guessed arguments could write to arbitrary memory, so it is not invoked */
}
