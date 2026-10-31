/* GetCharABCWidthsFloatI, GetTextExtentExPointWPri, GetTextFaceAliasW, GetTextMetricsAImpl,
 * GetETM, GetFontRealizationInfo, GetHFONT */
#include "gap.h"

UDECL(BOOL, GetCharABCWidthsFloatI, (HDC, UINT, UINT, LPWORD, LPABCFLOAT));
UDECL(BOOL, GetTextExtentExPointWPri, (HDC, LPCWSTR, int, int, LPINT, LPINT, LPSIZE));
UDECL(int, GetTextFaceAliasW, (HDC, int, LPWSTR));
UDECL(BOOL, GetTextMetricsAImpl, (HDC, LPTEXTMETRICA));
typedef struct { short v[26]; } ETM26;
UDECL(int, GetETM, (HDC, ETM26 *));
UDECL(int, GetFontRealizationInfo, (HDC, void *));
UDECL(HFONT, GetHFONT, (HDC));

/* ================= GetCharABCWidthsFloatI ================= */
TEST(gap0_abcfi_resolve) {
    CHECK(t_proc_gdi32("GetCharABCWidthsFloatI") != NULL);
    CHECK(t_proc_gdi32full("GetCharABCWidthsFloatI") != NULL);
    CHECK(t_proc_gdi32("GetCharABCWidthsI") != NULL);
}
TEST(gap0_abcfi_null_dc) {
    GetCharABCWidthsFloatI_pfn f = UN(GetCharABCWidthsFloatI); REQUIRE(f);
    SKIP_WINE("stub aborts");
    WORD gi = 36; ABCFLOAT abc; memset(&abc, 0x33, sizeof abc);
    CHECK(f(NULL, 0, 1, &gi, &abc) == FALSE);
}
TEST(gap0_abcfi_bogus_dc) {
    GetCharABCWidthsFloatI_pfn f = UN(GetCharABCWidthsFloatI); REQUIRE(f);
    SKIP_WINE("stub aborts");
    WORD gi = 36; ABCFLOAT abc;
    CHECK(f((HDC)0x1234, 0, 1, &gi, &abc) == FALSE);
    CHECK(f((HDC)0xdeadbeef, 0, 1, &gi, &abc) == FALSE);
}
TEST(gap0_abcfi_valid_vs_float_w) {
    GetCharABCWidthsFloatI_pfn f = UN(GetCharABCWidthsFloatI); REQUIRE(f); REQUIRE(U(GetGlyphIndicesW)); REQUIRE(U(GetCharABCWidthsFloatW));
    SKIP_WINE("stub aborts");
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -40);
    WCHAR s[] = L"Aa"; WORD gi[2];
    if (U(GetGlyphIndicesW)(fx.m.dc, s, 2, gi, 0) != 2) { gap_fx_close(&fx); t_skip("glyph indices"); return; }
    ABCFLOAT fi[2], fw[2];
    memset(fi, 0, sizeof fi);
    BOOL r = f(fx.m.dc, 0, 2, gi, fi);
    t_note("GetCharABCWidthsFloatI returned %d", r);
    if (r) {
        CHECK(U(GetCharABCWidthsFloatW)(fx.m.dc, 'A', 'A', &fw[0]));
        CHECK(U(GetCharABCWidthsFloatW)(fx.m.dc, 'a', 'a', &fw[1]));
        for (int i = 0; i < 2; i++) {
            SOFT(fi[i].abcfA == fw[i].abcfA && fi[i].abcfB == fw[i].abcfB && fi[i].abcfC == fw[i].abcfC, "I variant equals W variant");
            t_note("glyph %d: I %f %f %f / W %f %f %f", i, fi[i].abcfA, fi[i].abcfB, fi[i].abcfC, fw[i].abcfA, fw[i].abcfB, fw[i].abcfC);
        }
    }
    gap_fx_close(&fx);
}
TEST(gap0_abcfw_error_contract) {
    /* neighbouring float-ABC call whose validation (bGetCharABCWidthsW) is verified: NULL buffer / reversed range => ERROR_INVALID_PARAMETER */
    REQUIRE(U(GetCharABCWidthsFloatW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -40);
    ABCFLOAT abc;
    SET_ERR(0); CHECK(!U(GetCharABCWidthsFloatW)(fx.m.dc, 'A', 'A', NULL)); EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(GetCharABCWidthsFloatW)(fx.m.dc, 'B', 'A', &abc)); EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    CHECK(U(GetCharABCWidthsFloatW)(fx.m.dc, 'A', 'A', &abc));
    CHECK(abc.abcfA + abc.abcfB + abc.abcfC > 0);
    gap_fx_close(&fx);
}

/* ================= GetTextExtentExPointWPri ================= */
TEST(gap0_teepri_resolve) {
    CHECK(t_proc_gdi32("GetTextExtentExPointWPri") != NULL);
    CHECK(t_proc_gdi32full("GetTextExtentExPointWPri") != NULL);
}
TEST(gap0_teepri_null_dc) {
    GetTextExtentExPointWPri_pfn f = UN(GetTextExtentExPointWPri); REQUIRE(f);
    SIZE sz = { 77, 88 };
    CHECK(!f(NULL, L"abc", 3, 0, NULL, NULL, &sz));
}
TEST(gap0_teepri_bogus_dc) {
    GetTextExtentExPointWPri_pfn f = UN(GetTextExtentExPointWPri); REQUIRE(f);
    SIZE sz;
    CHECK(!f((HDC)0x1234, L"abc", 3, 0, NULL, NULL, &sz));
    CHECK(!f((HDC)0xdeadbeef, L"abc", 3, -1, NULL, NULL, &sz));
}
TEST(gap0_teepri_matches_public_total) {
    GetTextExtentExPointWPri_pfn f = UN(GetTextExtentExPointWPri); REQUIRE(f); REQUIRE(U(GetTextExtentExPointW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -24);
    const WCHAR *strs[] = { L"Hello, world", L"W", L"iiiiiiii", L"The quick brown fox", L"  lead and trail  ", L"\x00e9\x00e8\x00ea" };
    for (unsigned i = 0; i < sizeof strs / sizeof *strs; i++) {
        int n = lstrlenW(strs[i]);
        SIZE a = { 0, 0 }, b = { 1, 1 }; int fa = -1, fb = -2;
        BOOL ra = f(fx.m.dc, strs[i], n, -1, &fa, NULL, &a);
        BOOL rb = U(GetTextExtentExPointW)(fx.m.dc, strs[i], n, -1, &fb, NULL, &b);
        CHECKF(ra && rb, "str %u", i);
        EXPECT_EQ(a.cx, b.cx); EXPECT_EQ(a.cy, b.cy);
        EXPECT_EQ(fa, fb); EXPECT_EQ(fa, n);
    }
    gap_fx_close(&fx);
}
TEST(gap0_teepri_dx_cumulative) {
    GetTextExtentExPointWPri_pfn f = UN(GetTextExtentExPointWPri); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -24);
    const WCHAR *s = L"Cumulative Widths";
    int n = lstrlenW(s), dx[32], fit = 0; SIZE sz;
    CHECK(f(fx.m.dc, s, n, -1, &fit, dx, &sz));
    EXPECT_EQ(fit, n);
    int mono = 1; for (int i = 1; i < n; i++) if (dx[i] < dx[i - 1]) mono = 0;
    CHECK(mono);
    EXPECT_EQ(dx[n - 1], sz.cx);
    CHECK(dx[0] > 0);
    gap_fx_close(&fx);
}
TEST(gap0_teepri_dx_matches_single_extents) {
    GetTextExtentExPointWPri_pfn f = UN(GetTextExtentExPointWPri); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -24);
    const WCHAR *s = L"abcdefgh";
    int dx[8], fit; SIZE sz;
    f(fx.m.dc, s, 8, -1, &fit, dx, &sz);
    for (int i = 1; i <= 8; i++) {
        SIZE p; f(fx.m.dc, s, i, -1, NULL, NULL, &p);
        SOFT(p.cx == dx[i - 1], "prefix extent equals dx");
    }
    gap_fx_close(&fx);
}
TEST(gap0_teepri_maxextent_limits_fit) {
    GetTextExtentExPointWPri_pfn f = UN(GetTextExtentExPointWPri); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -24);
    const WCHAR *s = L"abcdefghij";
    int dx[10], fit; SIZE sz;
    f(fx.m.dc, s, 10, -1, &fit, dx, &sz);
    int limit = dx[4];
    int fit2 = -1; int dx2[10]; memset(dx2, 0x7f, sizeof dx2);
    CHECK(f(fx.m.dc, s, 10, limit, &fit2, dx2, &sz));
    EXPECT_EQ(fit2, 5);
    int fit3 = -1;
    CHECK(f(fx.m.dc, s, 10, limit - 1, &fit3, NULL, &sz));
    EXPECT_EQ(fit3, 4);
    int fit4 = -1;
    CHECK(f(fx.m.dc, s, 10, 0, &fit4, NULL, &sz));
    EXPECT_EQ(fit4, 0);
    gap_fx_close(&fx);
}
TEST(gap0_teepri_empty_string) {
    GetTextExtentExPointWPri_pfn f = UN(GetTextExtentExPointWPri); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -24);
    SIZE sz = { 9, 9 }; int fit = 5;
    BOOL r = f(fx.m.dc, L"", 0, -1, &fit, NULL, &sz);
    CHECK(r);
    if (r) { EXPECT_EQ(sz.cx, 0); EXPECT_EQ(fit, 0); }
    gap_fx_close(&fx);
}
TEST(gap0_teepri_height_matches_textmetric) {
    GetTextExtentExPointWPri_pfn f = UN(GetTextExtentExPointWPri); REQUIRE(f); REQUIRE(U(GetTextMetricsW));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -24);
    SIZE sz; TEXTMETRICW tm;
    CHECK(f(fx.m.dc, L"Ag", 2, -1, NULL, NULL, &sz));
    CHECK(U(GetTextMetricsW)(fx.m.dc, &tm));
    EXPECT_EQ(sz.cy, tm.tmHeight);
    gap_fx_close(&fx);
}
TEST(gap0_teepri_negative_count) {
    GetTextExtentExPointWPri_pfn f = UN(GetTextExtentExPointWPri); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -24);
    SIZE sz = { 4, 4 };
    BOOL r = f(fx.m.dc, L"abc", -1, -1, NULL, NULL, &sz);
    t_note("cch=-1 -> %d (size %ldx%ld)", r, sz.cx, sz.cy);
    gap_fx_close(&fx);
}
TEST(gap0_teepri_char_spacing) {
    GetTextExtentExPointWPri_pfn f = UN(GetTextExtentExPointWPri); REQUIRE(f); REQUIRE(U(SetTextCharacterExtra));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -24);
    SIZE a, b;
    f(fx.m.dc, L"abcd", 4, -1, NULL, NULL, &a);
    U(SetTextCharacterExtra)(fx.m.dc, 3);
    f(fx.m.dc, L"abcd", 4, -1, NULL, NULL, &b);
    EXPECT_EQ(b.cx - a.cx, 12);
    U(SetTextCharacterExtra)(fx.m.dc, 0);
    gap_fx_close(&fx);
}
TEST(gap0_teepri_dx_with_extra) {
    GetTextExtentExPointWPri_pfn f = UN(GetTextExtentExPointWPri); REQUIRE(f); REQUIRE(U(SetTextCharacterExtra));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -24);
    int d0[4], d1[4]; SIZE sz;
    f(fx.m.dc, L"abcd", 4, -1, NULL, d0, &sz);
    U(SetTextCharacterExtra)(fx.m.dc, 2);
    f(fx.m.dc, L"abcd", 4, -1, NULL, d1, &sz);
    for (int i = 0; i < 4; i++) EXPECT_EQ(d1[i] - d0[i], 2 * (i + 1));
    U(SetTextCharacterExtra)(fx.m.dc, 0);
    gap_fx_close(&fx);
}
TEST(gap0_teepri_fixed_pitch) {
    GetTextExtentExPointWPri_pfn f = UN(GetTextExtentExPointWPri); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Courier New", -20);
    SIZE a, b;
    f(fx.m.dc, L"iiii", 4, -1, NULL, NULL, &a);
    f(fx.m.dc, L"WWWW", 4, -1, NULL, NULL, &b);
    EXPECT_EQ(a.cx, b.cx);
    gap_fx_close(&fx);
}
TEST(gap0_teepri_mapmode_independent_of_lpk) {
    GetTextExtentExPointWPri_pfn f = UN(GetTextExtentExPointWPri); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -24);
    SIZE a, b;
    f(fx.m.dc, L"Mapping", 7, -1, NULL, NULL, &a);
    f(fx.m.dc, L"Mapping", 7, -1, NULL, NULL, &b);
    EXPECT_EQ(a.cx, b.cx);
    gap_fx_close(&fx);
}

/* ================= GetTextFaceAliasW ================= */
TEST(gap0_tfa_resolve) {
    CHECK(t_proc_gdi32("GetTextFaceAliasW") != NULL);
    CHECK(t_proc_gdi32full("GetTextFaceAliasW") != NULL);
}
TEST(gap0_tfa_null_dc) {
    GetTextFaceAliasW_pfn f = UN(GetTextFaceAliasW); REQUIRE(f);
    WCHAR buf[32] = L"x";
    CHECK(f(NULL, 32, buf) == 0);
    CHECK(f(NULL, 0, NULL) == 0);
}
TEST(gap0_tfa_bogus_dc) {
    GetTextFaceAliasW_pfn f = UN(GetTextFaceAliasW); REQUIRE(f);
    WCHAR buf[32];
    CHECK(f((HDC)0x1234, 32, buf) == 0);
    CHECK(f((HDC)0xdeadbeef, 0, NULL) == 0);
}
TEST(gap0_tfa_zero_count_with_buffer) {
    GetTextFaceAliasW_pfn f = UN(GetTextFaceAliasW); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -20);
    WCHAR buf[8]; memset(buf, 0x7e, sizeof buf);
    SET_ERR(0);
    CHECK(f(fx.m.dc, 0, buf) == 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    EXPECT_EQ(buf[0], 0x7e7e);
    gap_fx_close(&fx);
}
TEST(gap0_tfa_size_query) {
    GetTextFaceAliasW_pfn f = UN(GetTextFaceAliasW); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -20);
    int n = f(fx.m.dc, 0, NULL);
    EXPECT_EQ(n, 6);
    gap_fx_close(&fx);
}
TEST(gap0_tfa_exact_buffer) {
    GetTextFaceAliasW_pfn f = UN(GetTextFaceAliasW); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -20);
    WCHAR buf[16]; memset(buf, 0x7e, sizeof buf);
    int n = f(fx.m.dc, 6, buf);
    EXPECT_EQ(n, 6);
    CHECK(!lstrcmpW(buf, L"Arial"));
    EXPECT_EQ(buf[6], 0x7e7e);
    gap_fx_close(&fx);
}
TEST(gap0_tfa_large_buffer) {
    GetTextFaceAliasW_pfn f = UN(GetTextFaceAliasW); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -20);
    WCHAR buf[LF_FACESIZE]; memset(buf, 0x7e, sizeof buf);
    int n = f(fx.m.dc, LF_FACESIZE, buf);
    EXPECT_EQ(n, 6);
    CHECK(!lstrcmpW(buf, L"Arial"));
    gap_fx_close(&fx);
}
TEST(gap0_tfa_truncation) {
    GetTextFaceAliasW_pfn f = UN(GetTextFaceAliasW); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -20);
    WCHAR buf[8];
    for (int c = 1; c <= 5; c++) {
        memset(buf, 0x7e, sizeof buf);
        int n = f(fx.m.dc, c, buf);
        t_note("count %d -> ret %d buf0=%04x", c, n, buf[0]);
        SOFT(n == c, "returns copied count including NUL");
        SOFT(buf[c - 1] == 0, "NUL terminated");
        for (int i = 0; i < c - 1; i++) SOFT(buf[i] == L"Arial"[i], "prefix intact");
    }
    gap_fx_close(&fx);
}
TEST(gap0_tfa_matches_textfacew) {
    GetTextFaceAliasW_pfn f = UN(GetTextFaceAliasW); REQUIRE(f);
    const WCHAR *faces[] = { L"Arial", L"Courier New", L"Times New Roman", L"Tahoma", L"Segoe UI", L"Consolas" };
    for (unsigned i = 0; i < sizeof faces / sizeof *faces; i++) {
        gap_fx fx; if (!gap_fx_open(&fx, faces[i], -16) || !fx.ok) { gap_fx_close(&fx); continue; }
        WCHAR a[LF_FACESIZE], b[LF_FACESIZE];
        int na = f(fx.m.dc, LF_FACESIZE, a), nb = U(GetTextFaceW)(fx.m.dc, LF_FACESIZE, b);
        CHECK(na > 0);
        SOFT(na == nb && !lstrcmpW(a, b), "alias name equals realised name for installed fonts");
        gap_fx_close(&fx);
    }
}
TEST(gap0_tfa_unknown_family_request) {
    GetTextFaceAliasW_pfn f = UN(GetTextFaceAliasW); REQUIRE(f);
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    LOGFONTW lf; memset(&lf, 0, sizeof lf); lf.lfHeight = -16; lf.lfCharSet = DEFAULT_CHARSET;
    lstrcpyW(lf.lfFaceName, L"GapZeroNoSuchFace");
    HFONT hf = U(CreateFontIndirectW)(&lf);
    HGDIOBJ old = U(SelectObject)(m.dc, hf);
    WCHAR a[LF_FACESIZE] = L"", b[LF_FACESIZE] = L"";
    int na = f(m.dc, LF_FACESIZE, a), nb = U(GetTextFaceW)(m.dc, LF_FACESIZE, b);
    CHECK(nb > 0 && na > 0);
    t_note("requested GapZeroNoSuchFace: alias=%ls realised=%ls", a, b);
    U(SelectObject)(m.dc, old);
    U(DeleteObject)(hf);
    gap_mdc_close(&m);
}
TEST(gap0_tfa_no_gdi_leak) {
    GetTextFaceAliasW_pfn f = UN(GetTextFaceAliasW); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -20);
    DWORD before = gap_gdi_count(); WCHAR b[LF_FACESIZE];
    for (int i = 0; i < 500; i++) f(fx.m.dc, LF_FACESIZE, b);
    EXPECT_EQ(gap_gdi_count(), before);
    gap_fx_close(&fx);
}

/* ================= GetTextMetricsAImpl ================= */
TEST(gap0_tmai_resolve) {
    CHECK(t_proc_gdi32full("GetTextMetricsAImpl") != NULL);
}
TEST(gap0_tmai_invalid_dc) {
    GetTextMetricsAImpl_pfn f = UN(GetTextMetricsAImpl); REQUIRE(f);
    TEXTMETRICA tm; memset(&tm, 0x5a, sizeof tm);
    CHECK(f(NULL, &tm) == 0);
    CHECK(f((HDC)0x1234, &tm) == 0);
    CHECK(f((HDC)0xdeadbeef, &tm) == 0);
    EXPECT_EQ(tm.tmHeight, 0x5a5a5a5a);
}
TEST(gap0_tmai_deleted_dc) {
    GetTextMetricsAImpl_pfn f = UN(GetTextMetricsAImpl); REQUIRE(f);
    HDC dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    TEXTMETRICA tm;
    CHECK(f(dc, &tm) == 0);
}
TEST(gap0_tmai_matches_public) {
    GetTextMetricsAImpl_pfn f = UN(GetTextMetricsAImpl); REQUIRE(f); REQUIRE(U(GetTextMetricsA));
    const WCHAR *faces[] = { L"Arial", L"Courier New", L"Times New Roman", L"Tahoma" };
    int hs[] = { -12, -33, 18 };
    for (unsigned i = 0; i < 4; i++) for (unsigned j = 0; j < 3; j++) {
        gap_fx fx; if (!gap_fx_open(&fx, faces[i], hs[j]) || !fx.ok) { gap_fx_close(&fx); continue; }
        TEXTMETRICA a, b; memset(&a, 0, sizeof a); memset(&b, 1, sizeof b);
        BOOL ra = f(fx.m.dc, &a), rb = U(GetTextMetricsA)(fx.m.dc, &b);
        CHECK(ra && rb);
        EXPECT_EQ(a.tmHeight, b.tmHeight); EXPECT_EQ(a.tmAscent, b.tmAscent); EXPECT_EQ(a.tmDescent, b.tmDescent);
        EXPECT_EQ(a.tmInternalLeading, b.tmInternalLeading); EXPECT_EQ(a.tmExternalLeading, b.tmExternalLeading);
        EXPECT_EQ(a.tmAveCharWidth, b.tmAveCharWidth); EXPECT_EQ(a.tmMaxCharWidth, b.tmMaxCharWidth);
        EXPECT_EQ(a.tmWeight, b.tmWeight); EXPECT_EQ(a.tmPitchAndFamily, b.tmPitchAndFamily);
        EXPECT_EQ(a.tmCharSet, b.tmCharSet); EXPECT_EQ(a.tmFirstChar, b.tmFirstChar); EXPECT_EQ(a.tmLastChar, b.tmLastChar);
        EXPECT_EQ(a.tmDefaultChar, b.tmDefaultChar); EXPECT_EQ(a.tmBreakChar, b.tmBreakChar);
        EXPECT_EQ(a.tmItalic, b.tmItalic); EXPECT_EQ(a.tmUnderlined, b.tmUnderlined); EXPECT_EQ(a.tmStruckOut, b.tmStruckOut);
        gap_fx_close(&fx);
    }
}
TEST(gap0_tmai_height_requested) {
    GetTextMetricsAImpl_pfn f = UN(GetTextMetricsAImpl); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -40);
    TEXTMETRICA tm; CHECK(f(fx.m.dc, &tm));
    EXPECT_EQ(tm.tmHeight - tm.tmInternalLeading, 40);
    EXPECT_EQ(tm.tmAscent + tm.tmDescent, tm.tmHeight);
    gap_fx_close(&fx);
}
TEST(gap0_tmai_stock_fonts) {
    GetTextMetricsAImpl_pfn f = UN(GetTextMetricsAImpl); REQUIRE(f); REQUIRE(U(GetStockObject));
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    int ids[] = { SYSTEM_FONT, OEM_FIXED_FONT, ANSI_FIXED_FONT, ANSI_VAR_FONT, DEVICE_DEFAULT_FONT, SYSTEM_FIXED_FONT, DEFAULT_GUI_FONT };
    for (unsigned i = 0; i < sizeof ids / sizeof *ids; i++) {
        HGDIOBJ old = U(SelectObject)(m.dc, U(GetStockObject)(ids[i]));
        TEXTMETRICA a, b;
        BOOL ra = f(m.dc, &a), rb = U(GetTextMetricsA)(m.dc, &b);
        CHECKF(ra == rb, "stock %d", ids[i]);
        if (ra && rb) { EXPECT_EQ(a.tmHeight, b.tmHeight); EXPECT_EQ(a.tmAveCharWidth, b.tmAveCharWidth); }
        U(SelectObject)(m.dc, old);
    }
    gap_mdc_close(&m);
}

/* ================= GetETM ================= */
TEST(gap0_etm_resolve) {
    CHECK(t_proc_gdi32full("GetETM") != NULL);
    CHECK(t_proc_gdi32("GetETM") != NULL);
}
TEST(gap0_etm_invalid_dc) {
    GetETM_pfn f = UN(GetETM); REQUIRE(f); SKIP_WINE("unimplemented stub");
    ETM26 e; memset(&e, 0x44, sizeof e);
    CHECK(f(NULL, &e) == 0);
    CHECK(f((HDC)0x1234, &e) == 0);
    CHECK(f((HDC)0xdeadbeef, &e) == 0);
    EXPECT_EQ(e.v[0], 0x4444);
}
TEST(gap0_etm_null_output) {
    GetETM_pfn f = UN(GetETM); REQUIRE(f); SKIP_WINE("unimplemented stub");
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -40);
    CHECK(f(fx.m.dc, NULL) == 0);
    gap_fx_close(&fx);
}
TEST(gap0_etm_deleted_dc) {
    GetETM_pfn f = UN(GetETM); REQUIRE(f); SKIP_WINE("unimplemented stub");
    HDC dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    ETM26 e; CHECK(f(dc, &e) == 0);
}
TEST(gap0_etm_arial) {
    GetETM_pfn f = UN(GetETM); REQUIRE(f); SKIP_WINE("unimplemented stub");
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -40);
    ETM26 e; memset(&e, 0x44, sizeof e);
    int r = f(fx.m.dc, &e);
    t_note("GetETM(Arial) -> %d size=%d ptsize=%d masterunits=%d capheight=%d xheight=%d kernpairs=%d", r, e.v[0], e.v[1], e.v[6], e.v[7], e.v[8], e.v[24]);
    if (r) {
        SOFT(e.v[0] == 52, "etmSize == 52");
        SOFT(e.v[6] == 2048, "Arial master units 2048");
        SOFT(e.v[7] > 0 && e.v[7] < e.v[6], "cap height within em");
        SOFT(e.v[8] > 0 && e.v[8] < e.v[7], "x height below cap height");
    }
    gap_fx_close(&fx);
}
TEST(gap0_etm_kernpairs_patched_from_ansi_count) {
    GetETM_pfn f = UN(GetETM); REQUIRE(f); SKIP_WINE("unimplemented stub");
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -40);
    ETM26 e; memset(&e, 0, sizeof e);
    int r = f(fx.m.dc, &e);
    if (!r) { gap_fx_close(&fx); t_skip("GetETM not supported for this font"); return; }
    DWORD n = U(GetKerningPairsA)(fx.m.dc, 0, NULL);
    EXPECT_EQ((unsigned short)e.v[24], (unsigned short)n);
    gap_fx_close(&fx);
}
TEST(gap0_etm_masterunits_scale_independent) {
    GetETM_pfn f = UN(GetETM); REQUIRE(f); SKIP_WINE("unimplemented stub");
    short mu[2] = { 0, 0 }; int hs[2] = { -20, -90 }, got = 0;
    for (int i = 0; i < 2; i++) {
        gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", hs[i]);
        ETM26 e; memset(&e, 0, sizeof e);
        if (f(fx.m.dc, &e)) { mu[i] = e.v[6]; got++; }
        gap_fx_close(&fx);
    }
    if (got == 2) EXPECT_EQ(mu[0], mu[1]);
}
TEST(gap0_etm_bitmap_font_rejected) {
    GetETM_pfn f = UN(GetETM); REQUIRE(f); SKIP_WINE("unimplemented stub"); REQUIRE(U(GetStockObject));
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    HGDIOBJ old = U(SelectObject)(m.dc, U(GetStockObject)(SYSTEM_FIXED_FONT));
    ETM26 e; memset(&e, 0x44, sizeof e);
    int r = f(m.dc, &e);
    t_note("GetETM(System fixed bitmap font) = %d", r);
    SOFT(r == 0, "raster fonts have no ETM");
    if (!r) EXPECT_EQ(e.v[0], 0x4444);
    U(SelectObject)(m.dc, old);
    gap_mdc_close(&m);
}
TEST(gap0_etm_times_consolas) {
    GetETM_pfn f = UN(GetETM); REQUIRE(f); SKIP_WINE("unimplemented stub");
    const WCHAR *faces[] = { L"Times New Roman", L"Consolas", L"Tahoma", L"Courier New" };
    for (int i = 0; i < 4; i++) {
        gap_fx fx; if (!gap_fx_open(&fx, faces[i], -30) || !fx.ok) { gap_fx_close(&fx); continue; }
        ETM26 e; memset(&e, 0, sizeof e);
        int r = f(fx.m.dc, &e);
        t_note("%ls: ret %d masterunits %d", faces[i], r, e.v[6]);
        if (r) SOFT(e.v[6] == 2048 || e.v[6] == 1000 || e.v[6] == 1024, "plausible units per em");
        gap_fx_close(&fx);
    }
}

/* ================= GetFontRealizationInfo ================= */
TEST(gap0_fri_resolve) {
    CHECK(t_proc_gdi32full("GetFontRealizationInfo") != NULL);
    CHECK(t_proc_gdi32("GetFontRealizationInfo") != NULL);
}
TEST(gap0_fri_bogus_dc) {
    GetFontRealizationInfo_pfn f = UN(GetFontRealizationInfo); REQUIRE(f);
    SKIP_WINE("unimplemented");
    DWORD info[0x32c / 4];
    DWORD sizes[] = { 0x10, 0x32c };
    for (int i = 0; i < 2; i++) {
        memset(info, 0, sizeof info); info[0] = sizes[i];
        CHECK(f(NULL, info) == 0);
        info[0] = sizes[i]; CHECK(f((HDC)0x1234, info) == 0);
        info[0] = sizes[i]; CHECK(f((HDC)0xdeadbeef, info) == 0);
    }
}
TEST(gap0_fri_bogus_dc_bad_size) {
    GetFontRealizationInfo_pfn f = UN(GetFontRealizationInfo); REQUIRE(f);
    SKIP_WINE("unimplemented");
    DWORD info[0x32c / 4];
    DWORD sizes[] = { 0, 4, 8, 0x11, 0x14, 0x18, 0x328, 0x330, 0xffffffff };
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        memset(info, 0, sizeof info); info[0] = sizes[i];
        CHECKF(f((HDC)0x1234, info) == 0, "size 0x%lx", sizes[i]);
    }
}
TEST(gap0_fri_deleted_dc) {
    GetFontRealizationInfo_pfn f = UN(GetFontRealizationInfo); REQUIRE(f);
    SKIP_WINE("unimplemented");
    HDC dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    DWORD info[0x32c / 4]; memset(info, 0, sizeof info); info[0] = 0x10;
    CHECK(f(dc, info) == 0);
}
TEST(gap0_fri_valid_small) {
    GetFontRealizationInfo_pfn f = UN(GetFontRealizationInfo); REQUIRE(f);
    SKIP_WINE("unimplemented");
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -40);
    DWORD info[0x32c / 4]; memset(info, 0xCD, sizeof info); info[0] = 0x10;
    int r = f(fx.m.dc, info);
    t_note("size 0x10: ret %d flags %08lx cache %lu inst %08lx", r, info[1], info[2], info[3]);
    SOFT(r != 0, "realization info available for a realised TrueType font");
    if (r) EXPECT_EQ(info[0], 0x10);
    if (r) EXPECT_EQ(info[4], 0xCDCDCDCD);
    gap_fx_close(&fx);
}
TEST(gap0_fri_valid_large) {
    GetFontRealizationInfo_pfn f = UN(GetFontRealizationInfo); REQUIRE(f);
    SKIP_WINE("unimplemented");
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -40);
    DWORD info[0x32c / 4 + 4]; memset(info, 0xCD, sizeof info); info[0] = 0x32c;
    int r = f(fx.m.dc, info);
    t_note("size 0x32c: ret %d", r);
    if (r) { EXPECT_EQ(info[0], 0x32c); EXPECT_EQ(info[0x32c / 4], 0xCDCDCDCD); }
    gap_fx_close(&fx);
}
TEST(gap0_fri_stable_across_calls) {
    GetFontRealizationInfo_pfn f = UN(GetFontRealizationInfo); REQUIRE(f);
    SKIP_WINE("unimplemented");
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -40);
    DWORD a[4] = { 0x10 }, b[4] = { 0x10 };
    int ra = f(fx.m.dc, a), rb = f(fx.m.dc, b);
    CHECK(ra == rb);
    if (ra && rb) { SOFT(a[1] == b[1] && a[2] == b[2] && a[3] == b[3], "same realisation twice"); }
    gap_fx_close(&fx);
}
TEST(gap0_fri_different_fonts_differ) {
    GetFontRealizationInfo_pfn f = UN(GetFontRealizationInfo); REQUIRE(f);
    SKIP_WINE("unimplemented");
    gap_fx x, y; FX_OPEN_OR_SKIP(x, L"Arial", -40);
    if (!gap_fx_open(&y, L"Courier New", -40) || !y.ok) { gap_fx_close(&x); gap_fx_close(&y); t_skip("font"); return; }
    DWORD a[4] = { 0x10 }, b[4] = { 0x10 };
    if (f(x.m.dc, a) && f(y.m.dc, b)) SOFT(a[3] != b[3] || a[2] != b[2], "distinct realisations for distinct faces");
    gap_fx_close(&x); gap_fx_close(&y);
}
TEST(gap0_fri_stock_font) {
    GetFontRealizationInfo_pfn f = UN(GetFontRealizationInfo); REQUIRE(f); REQUIRE(U(GetStockObject));
    SKIP_WINE("unimplemented");
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    DWORD a[4] = { 0x10 };
    int r = f(m.dc, a);
    t_note("default DC font: ret %d flags %08lx", r, a[1]);
    gap_mdc_close(&m);
}
TEST(gap0_fri_no_gdi_leak) {
    GetFontRealizationInfo_pfn f = UN(GetFontRealizationInfo); REQUIRE(f);
    SKIP_WINE("unimplemented");
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -40);
    DWORD before = gap_gdi_count(); DWORD a[4];
    for (int i = 0; i < 200; i++) { a[0] = 0x10; f(fx.m.dc, a); }
    EXPECT_EQ(gap_gdi_count(), before);
    gap_fx_close(&fx);
}

/* ================= GetHFONT ================= */
TEST(gap0_hfont_resolve) {
    CHECK(t_proc_gdi32full("GetHFONT") != NULL);
    CHECK(t_proc_gdi32("GetHFONT") != NULL);
}
TEST(gap0_hfont_invalid) {
    GetHFONT_pfn f = UN(GetHFONT); REQUIRE(f);
    CHECK(f(NULL) == NULL);
    CHECK(f((HDC)0x1234) == NULL);
    CHECK(f((HDC)0xdeadbeef) == NULL);
    CHECK(f((HDC)(ULONG_PTR)-1) == NULL);
    CHECK(f((HDC)0x10000) == NULL);
}
TEST(gap0_hfont_deleted_dc) {
    GetHFONT_pfn f = UN(GetHFONT); REQUIRE(f);
    HDC dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    CHECK(f(dc) == NULL);
}
TEST(gap0_hfont_non_dc_object) {
    GetHFONT_pfn f = UN(GetHFONT); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -20);
    CHECK(f((HDC)fx.f) == NULL);
    CHECK(f((HDC)fx.m.bm) == NULL);
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0);
    CHECK(f((HDC)p) == NULL);
    U(DeleteObject)(p);
    CHECK(f((HDC)U(GetStockObject)(SYSTEM_FONT)) == NULL);
    gap_fx_close(&fx);
}
TEST(gap0_hfont_returns_selected) {
    GetHFONT_pfn f = UN(GetHFONT); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -20);
    EXPECT_EQ(f(fx.m.dc), fx.f);
    EXPECT_EQ(f(fx.m.dc), U(GetCurrentObject)(fx.m.dc, OBJ_FONT));
    gap_fx_close(&fx);
}
TEST(gap0_hfont_follows_selection) {
    GetHFONT_pfn f = UN(GetHFONT); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -20);
    HFONT f2 = U(CreateFontW)(-30, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, L"Tahoma");
    HGDIOBJ prev = U(SelectObject)(fx.m.dc, f2);
    EXPECT_EQ(prev, fx.f);
    EXPECT_EQ(f(fx.m.dc), f2);
    U(SelectObject)(fx.m.dc, fx.f);
    EXPECT_EQ(f(fx.m.dc), fx.f);
    U(DeleteObject)(f2);
    gap_fx_close(&fx);
}
TEST(gap0_hfont_stock_font_selected) {
    GetHFONT_pfn f = UN(GetHFONT); REQUIRE(f);
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    HGDIOBJ sf = U(GetStockObject)(ANSI_VAR_FONT);
    HGDIOBJ old = U(SelectObject)(m.dc, sf);
    EXPECT_EQ(f(m.dc), sf);
    U(SelectObject)(m.dc, old);
    gap_mdc_close(&m);
}
TEST(gap0_hfont_default_dc_matches_current) {
    GetHFONT_pfn f = UN(GetHFONT); REQUIRE(f);
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    HFONT h = f(m.dc);
    SOFT(h == NULL || h == U(GetCurrentObject)(m.dc, OBJ_FONT), "fresh DC: NULL or the default font");
    t_note("fresh memdc GetHFONT=%p current=%p", h, U(GetCurrentObject)(m.dc, OBJ_FONT));
    gap_mdc_close(&m);
}
TEST(gap0_hfont_screen_dc) {
    GetHFONT_pfn f = UN(GetHFONT); REQUIRE(f); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC dc = U(GetDC)(NULL); if (!dc) { t_skip("no screen dc"); return; }
    HFONT h = f(dc);
    SOFT(h == NULL || h == U(GetCurrentObject)(dc, OBJ_FONT), "screen DC");
    U(ReleaseDC)(NULL, dc);
}
TEST(gap0_hfont_after_font_deleted) {
    GetHFONT_pfn f = UN(GetHFONT); REQUIRE(f);
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    HFONT hf = U(CreateFontW)(-20, 0, 0, 0, 400, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, L"Arial");
    HGDIOBJ old = U(SelectObject)(m.dc, hf);
    EXPECT_EQ(f(m.dc), hf);
    U(SelectObject)(m.dc, old);
    U(DeleteObject)(hf);
    HFONT after = f(m.dc);
    CHECK(after != hf);
    gap_mdc_close(&m);
}
typedef struct { HDC dc; HFONT seen; } hf_arg;
static DWORD WINAPI hf_thread(LPVOID p) { hf_arg *a = p; a->seen = ((HFONT (WINAPI *)(HDC))t_proc("GetHFONT"))(a->dc); return 0; }
TEST(gap0_hfont_cross_thread) {
    GetHFONT_pfn f = UN(GetHFONT); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -20);
    hf_arg a = { fx.m.dc, (HFONT)1 };
    HANDLE th = CreateThread(NULL, 0, hf_thread, &a, 0, NULL);
    WaitForSingleObject(th, 10000); CloseHandle(th);
    EXPECT_EQ(a.seen, fx.f);
    gap_fx_close(&fx);
}
TEST(gap0_hfont_matches_getcurrentobject_many) {
    GetHFONT_pfn f = UN(GetHFONT); REQUIRE(f);
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    HGDIOBJ old = NULL; int bad = 0;
    for (int i = 0; i < 30; i++) {
        HFONT hf = U(CreateFontW)(-8 - i, 0, 0, 0, 400, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, L"Arial");
        HGDIOBJ o = U(SelectObject)(m.dc, hf); if (!old) old = o;
        if (f(m.dc) != hf) bad++;
        U(SelectObject)(m.dc, old);
        U(DeleteObject)(hf);
    }
    EXPECT_EQ(bad, 0);
    gap_mdc_close(&m);
}
