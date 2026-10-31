/* GetCharWidthA, GetCharWidthFloatA, GetTextExtentExPointI, GetGlyphOutlineA, GetOutlineTextMetricsW, CreateFontIndirectExA. */
#include "gap10.h"

typedef struct { HDC dc; HFONT font, old; } FD;
static int fd_open(FD *f, const WCHAR *face, int height, int weight, int italic, int charset) {
    memset(f, 0, sizeof *f);
    f->dc = U(CreateCompatibleDC)(NULL);
    if (!f->dc) return 0;
    f->font = U(CreateFontW)(height, 0, 0, 0, weight, italic, 0, 0, charset, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, face);
    if (!f->font) { U(DeleteDC)(f->dc); f->dc = NULL; return 0; }
    f->old = (HFONT)U(SelectObject)(f->dc, f->font);
    return 1;
}
static void fd_close(FD *f) {
    if (!f->dc) return;
    U(SelectObject)(f->dc, f->old);
    U(DeleteObject)(f->font);
    U(DeleteDC)(f->dc);
    f->dc = NULL;
}
static int face_is(FD *f, const WCHAR *want) {
    WCHAR buf[LF_FACESIZE];
    if (!U(GetTextFaceW)) return 1;
    buf[0] = 0;
    U(GetTextFaceW)(f->dc, LF_FACESIZE, buf);
    return !lstrcmpiW(buf, want);
}
#define NEED_FD() do { REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(CreateFontW)); REQUIRE(U(SelectObject)); REQUIRE(U(DeleteObject)); REQUIRE(U(DeleteDC)); } while (0)
static int sbcs(void) { UINT a = GetACP(); return !(a == 932 || a == 936 || a == 949 || a == 950); }

/* ===================== GetCharWidthA ===================== */
#define CWA U(GetCharWidthA)
TEST(gap10_cwa_invalid_dc) {
    NEED_FD(); REQUIRE(CWA);
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef }; unsigned i; INT w[4];
    for (i = 0; i < 3; i++) {
        CLR();
        CHECKF(!CWA(bad[i], 65, 66, w), "dc %p", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "dc %p err %lu", bad[i], GetLastError());
    }
}
TEST(gap10_cwa_wrong_object_and_deleted_dc) {
    NEED_FD(); REQUIRE(CWA); REQUIRE(U(CreateSolidBrush));
    HBRUSH b = U(CreateSolidBrush)(0); INT w[2]; HDC dc = U(CreateCompatibleDC)(NULL);
    CLR(); CHECK(!CWA((HDC)b, 65, 65, w)); EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    U(DeleteObject)(b);
    U(DeleteDC)(dc);
    CLR(); CHECK(!CWA(dc, 65, 65, w)); EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
}
TEST(gap10_cwa_null_buffer) {
    NEED_FD(); REQUIRE(CWA); SKIP_WINE("NULL buffer is written");
    FD f; if (!fd_open(&f, L"Arial", -16, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CLR();
    CHECK(!CWA(f.dc, 65, 65, NULL));
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    fd_close(&f);
}
TEST(gap10_cwa_first_greater_than_last) {
    NEED_FD(); REQUIRE(CWA);
    FD f; INT w[4];
    if (!sbcs()) { t_skip("DBCS ACP"); return; }
    if (!fd_open(&f, L"Arial", -16, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CLR(); CHECK(!CWA(f.dc, 66, 65, w)); EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    CLR(); CHECK(!CWA(f.dc, 255, 0, w)); EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    fd_close(&f);
}
TEST(gap10_cwa_last_above_255) {
    NEED_FD(); REQUIRE(CWA);
    FD f; static INT w[1024];
    if (!sbcs()) { t_skip("DBCS ACP"); return; }
    if (!fd_open(&f, L"Arial", -16, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CLR(); CHECK(!CWA(f.dc, 0, 256, w)); EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    CLR(); CHECK(!CWA(f.dc, 0, 0x1000, w)); EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    CLR(); CHECK(!CWA(f.dc, 300, 301, w));
    CLR(); CHECK(!CWA(f.dc, 0, 0xFFFFFFFF, w));
    CLR(); CHECK(!CWA(f.dc, 0xFFFFFFFF, 0xFFFFFFFF, w));
    fd_close(&f);
}
TEST(gap10_cwa_single_char) {
    NEED_FD(); REQUIRE(CWA);
    FD f; INT w = -1; DWORD ch;
    if (!fd_open(&f, L"Arial", -16, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    for (ch = 'A'; ch <= 'Z'; ch++) { w = -1; CHECKF(CWA(f.dc, ch, ch, &w), "char %lu", ch); CHECKF(w > 0 && w < 40, "char %lu width %d", ch, w); }
    CHECK(CWA(f.dc, 0, 0, &w));
    CHECK(CWA(f.dc, 255, 255, &w));
    CHECK(CWA(f.dc, ' ', ' ', &w)); CHECKF(w > 0, "space width %d", w);
    fd_close(&f);
}
TEST(gap10_cwa_full_range_0_255) {
    NEED_FD(); REQUIRE(CWA);
    FD f; INT w[258]; int i;
    if (!fd_open(&f, L"Arial", -16, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    for (i = 0; i < 258; i++) w[i] = 0x5A5A5A5A;
    CHECK(CWA(f.dc, 0, 255, w));
    EXPECT_EQ(w[256], 0x5A5A5A5A); EXPECT_EQ(w[257], 0x5A5A5A5A);   /* nothing written beyond the range */
    for (i = 0; i < 256; i++) if (w[i] == 0x5A5A5A5A) { CHECKF(0, "entry %d not written", i); break; }
    for (i = 32; i < 127; i++) CHECKF(w[i] > 0, "printable %d width %d", i, w[i]);
    fd_close(&f);
}
TEST(gap10_cwa_exact_count_written) {
    NEED_FD(); REQUIRE(CWA);
    FD f; INT w[8]; int i;
    if (!fd_open(&f, L"Arial", -16, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    for (i = 0; i < 8; i++) w[i] = 0x7A7A7A7A;
    CHECK(CWA(f.dc, 'A', 'C', w));
    CHECK(w[0] != 0x7A7A7A7A && w[1] != 0x7A7A7A7A && w[2] != 0x7A7A7A7A);
    for (i = 3; i < 8; i++) CHECKF(w[i] == 0x7A7A7A7A, "slot %d overwritten", i);
    fd_close(&f);
}
TEST(gap10_cwa_arial_relative_widths) {
    NEED_FD(); REQUIRE(CWA);
    FD f; INT w[256];
    if (!fd_open(&f, L"Arial", -32, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CHECK(CWA(f.dc, 0, 255, w));
    CHECK(w['W'] > w['i']); CHECK(w['M'] > w['l']); CHECK(w['m'] > w['i']); CHECK(w['0'] == w['9']); CHECK(w['0'] == w['5']);
    CHECK(w[' '] < w['0']); CHECK(w['.'] < w['0']);
    fd_close(&f);
}
TEST(gap10_cwa_courier_is_fixed_pitch) {
    NEED_FD(); REQUIRE(CWA);
    FD f; INT w[256]; int i, same = 1;
    if (!fd_open(&f, L"Courier New", -16, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    if (!face_is(&f, L"Courier New")) { t_note("Courier New not selected"); fd_close(&f); return; }
    CHECK(CWA(f.dc, 32, 126, w));
    for (i = 1; i < 95; i++) if (w[i] != w[0]) same = 0;
    CHECK(same);
    fd_close(&f);
}
TEST(gap10_cwa_scales_with_height) {
    NEED_FD(); REQUIRE(CWA);
    FD a, b; INT wa = 0, wb = 0;
    if (!fd_open(&a, L"Arial", -16, 400, 0, ANSI_CHARSET) || !fd_open(&b, L"Arial", -64, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CHECK(CWA(a.dc, 'M', 'M', &wa)); CHECK(CWA(b.dc, 'M', 'M', &wb));
    CHECKF(wb >= wa * 3 && wb <= wa * 5, "16px %d, 64px %d", wa, wb);
    fd_close(&a); fd_close(&b);
}
TEST(gap10_cwa_bold_wider_than_regular) {
    NEED_FD(); REQUIRE(CWA);
    FD a, b; INT wa = 0, wb = 0;
    if (!fd_open(&a, L"Arial", -32, 400, 0, ANSI_CHARSET) || !fd_open(&b, L"Arial", -32, 700, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CWA(a.dc, 'a', 'a', &wa); CWA(b.dc, 'a', 'a', &wb);
    CHECKF(wb >= wa, "regular %d bold %d", wa, wb);
    fd_close(&a); fd_close(&b);
}
TEST(gap10_cwa_matches_wide_for_ascii) {
    NEED_FD(); REQUIRE(CWA); REQUIRE(U(GetCharWidthW));
    FD f; INT a[96], w[96]; int i;
    if (!sbcs()) { t_skip("DBCS ACP"); return; }
    if (!fd_open(&f, L"Arial", -20, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CHECK(CWA(f.dc, 32, 126, a)); CHECK(U(GetCharWidthW)(f.dc, 32, 126, w));
    for (i = 0; i < 95; i++) CHECKF(abs(a[i] - w[i]) <= 1, "char %d: A %d W %d", 32 + i, a[i], w[i]);
    fd_close(&f);
}
TEST(gap10_cwa_sum_matches_text_extent) {
    NEED_FD(); REQUIRE(CWA); REQUIRE(U(GetTextExtentPoint32A));
    FD f; INT w[3]; SIZE sz;
    if (!fd_open(&f, L"Arial", -24, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CHECK(CWA(f.dc, 'A', 'C', w)); CHECK(U(GetTextExtentPoint32A)(f.dc, "ABC", 3, &sz));
    int sum = w[0] + w[1] + w[2];
    CHECKF(abs(sum - sz.cx) <= 3, "sum %d extent %ld", sum, sz.cx);
    fd_close(&f);
}
TEST(gap10_cwa_stock_fonts_and_default) {
    NEED_FD(); REQUIRE(CWA); REQUIRE(U(GetStockObject));
    int stock[] = { SYSTEM_FONT, SYSTEM_FIXED_FONT, DEVICE_DEFAULT_FONT, OEM_FIXED_FONT, ANSI_FIXED_FONT, ANSI_VAR_FONT, DEFAULT_GUI_FONT }; unsigned i;
    HDC dc = U(CreateCompatibleDC)(NULL); INT w[256];
    for (i = 0; i < sizeof stock / sizeof *stock; i++) {
        HGDIOBJ old = U(SelectObject)(dc, U(GetStockObject)(stock[i]));
        CHECKF(CWA(dc, 0, 255, w), "stock %d", stock[i]);
        CHECKF(w['A'] > 0, "stock %d width(A) %d", stock[i], w['A']);
        U(SelectObject)(dc, old);
    }
    CHECK(CWA(dc, 'A', 'A', w));   /* default font in a fresh DC */
    U(DeleteDC)(dc);
}
TEST(gap10_cwa_fixedsys_fixed_pitch) {
    NEED_FD(); REQUIRE(CWA); REQUIRE(U(GetStockObject));
    HDC dc = U(CreateCompatibleDC)(NULL); HGDIOBJ old = U(SelectObject)(dc, U(GetStockObject)(SYSTEM_FIXED_FONT)); INT w[96]; int i, same = 1;
    CHECK(CWA(dc, 32, 126, w));
    for (i = 1; i < 95; i++) if (w[i] != w[0]) same = 0;
    CHECK(same);
    U(SelectObject)(dc, old); U(DeleteDC)(dc);
}
TEST(gap10_cwa_symbol_font_charset) {
    NEED_FD(); REQUIRE(CWA);
    FD f; INT w[256];
    if (!fd_open(&f, L"Symbol", -16, 400, 0, SYMBOL_CHARSET)) { t_skip("no font"); return; }
    BOOL r = CWA(f.dc, 0, 255, w);
    t_note("Symbol charset GetCharWidthA(0..255) -> %d", r);
    if (r) CHECK(w['a'] > 0);
    fd_close(&f);
}
TEST(gap10_cwa_other_acp_charsets_observed) {
    NEED_FD(); REQUIRE(CWA);
    BYTE cs[] = { GREEK_CHARSET, RUSSIAN_CHARSET, EASTEUROPE_CHARSET, TURKISH_CHARSET }; unsigned i;
    for (i = 0; i < sizeof cs; i++) {
        FD f; INT w[256];
        if (!fd_open(&f, L"Arial", -16, 400, 0, cs[i])) continue;
        CLR();
        BOOL r = CWA(f.dc, 0, 255, w);
        t_note("charset %d: ret %d err %lu w[A]=%d", cs[i], r, GetLastError(), r ? w['A'] : -1);
        fd_close(&f);
    }
}
TEST(gap10_cwa_mapmode_text_scaling_observed) {
    NEED_FD(); REQUIRE(CWA); REQUIRE(U(SetMapMode)); REQUIRE(U(SetWindowExtEx)); REQUIRE(U(SetViewportExtEx));
    FD f; INT w1 = 0, w2 = 0;
    if (!fd_open(&f, L"Arial", -32, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CWA(f.dc, 'M', 'M', &w1);
    U(SetMapMode)(f.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(f.dc, 1, 1, NULL); U(SetViewportExtEx)(f.dc, 2, 2, NULL);
    CWA(f.dc, 'M', 'M', &w2);
    t_note("MM_TEXT width %d, 2x viewport width %d", w1, w2);
    fd_close(&f);
}
TEST(gap10_cwa_repeated_stable) {
    NEED_FD(); REQUIRE(CWA);
    FD f; INT a[256], b[256]; int i, ok = 1;
    if (!fd_open(&f, L"Tahoma", -13, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CWA(f.dc, 0, 255, a);
    for (i = 0; i < 100; i++) { CWA(f.dc, 0, 255, b); if (memcmp(a, b, sizeof a)) ok = 0; }
    CHECK(ok);
    fd_close(&f);
}
TEST(gap10_cwa_no_gdi_leak) {
    NEED_FD(); REQUIRE(CWA);
    if (!g10_gdicount()) { t_skip("no GetGuiResources"); return; }
    FD f; INT w[256]; int i; DWORD before;
    if (!fd_open(&f, L"Arial", -16, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    before = g10_gdicount();
    for (i = 0; i < 200; i++) CWA(f.dc, 0, 255, w);
    EXPECT_EQ(g10_gdicount(), before);
    fd_close(&f);
}
TEST(gap10_cwa_gdi32_vs_gdi32full) {
    typedef BOOL (WINAPI *P)(HDC, UINT, UINT, LPINT);
    P a = (P)t_proc_gdi32("GetCharWidthA"), b = (P)t_proc_gdi32full("GetCharWidthA"); REQUIRE(a); NEED_FD();
    FD f; INT w1[4] = { 0 }, w2[4] = { 0 };
    if (!fd_open(&f, L"Arial", -16, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CHECK(a(f.dc, 'A', 'D', w1));
    if (b) { CHECK(b(f.dc, 'A', 'D', w2)); CHECK(!memcmp(w1, w2, sizeof w1)); }
    fd_close(&f);
}

/* ===================== GetCharWidthFloatA ===================== */
#define CWF U(GetCharWidthFloatA)
TEST(gap10_cwf_invalid_dc) {
    NEED_FD(); REQUIRE(CWF);
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef }; unsigned i; float w[4];
    for (i = 0; i < 3; i++) { CLR(); CHECKF(!CWF(bad[i], 65, 66, w), "dc %p", bad[i]); CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "dc %p err %lu", bad[i], GetLastError()); }
}
TEST(gap10_cwf_argument_validation) {
    NEED_FD(); REQUIRE(CWF); SKIP_WINE("NULL buffer is written");
    FD f; float w[300];
    if (!sbcs()) { t_skip("DBCS ACP"); return; }
    if (!fd_open(&f, L"Arial", -16, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CLR(); CHECK(!CWF(f.dc, 65, 65, NULL)); EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    CLR(); CHECK(!CWF(f.dc, 66, 65, w)); EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    CLR(); CHECK(!CWF(f.dc, 0, 256, w)); EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    CLR(); CHECK(!CWF(f.dc, 255, 300, w));
    CHECK(CWF(f.dc, 0, 255, w));
    CHECK(CWF(f.dc, 65, 65, w));
    fd_close(&f);
}
TEST(gap10_cwf_values_positive_and_close_to_int) {
    NEED_FD(); REQUIRE(CWF); REQUIRE(CWA);
    FD f; float fw[96]; INT iw[96]; int i;
    if (!sbcs()) { t_skip("DBCS ACP"); return; }
    if (!fd_open(&f, L"Arial", -20, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CHECK(CWF(f.dc, 32, 126, fw)); CHECK(CWA(f.dc, 32, 126, iw));
    for (i = 0; i < 95; i++) {
        CHECKF(fw[i] > 0.0f, "char %d float width %f", 32 + i, fw[i]);
        CHECKF(fabsf(fw[i] - (float)iw[i]) <= 1.01f, "char %d float %f int %d", 32 + i, fw[i], iw[i]);
    }
    fd_close(&f);
}
TEST(gap10_cwf_matches_wide_variant) {
    NEED_FD(); REQUIRE(CWF); REQUIRE(U(GetCharWidthFloatW));
    FD f; float a[26], b[26]; int i;
    if (!sbcs()) { t_skip("DBCS ACP"); return; }
    if (!fd_open(&f, L"Times New Roman", -22, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CHECK(CWF(f.dc, 'A', 'Z', a)); CHECK(U(GetCharWidthFloatW)(f.dc, 'A', 'Z', b));
    for (i = 0; i < 26; i++) CHECKF(fabsf(a[i] - b[i]) < 0.001f, "char %c: A %f W %f", 'A' + i, a[i], b[i]);
    fd_close(&f);
}
TEST(gap10_cwf_exact_range_written) {
    NEED_FD(); REQUIRE(CWF);
    FD f; float w[8]; int i; DWORD pat = 0x7FC12345;
    if (!fd_open(&f, L"Arial", -16, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    for (i = 0; i < 8; i++) memcpy(&w[i], &pat, 4);
    CHECK(CWF(f.dc, 'a', 'c', w));
    for (i = 3; i < 8; i++) { DWORD v; memcpy(&v, &w[i], 4); CHECKF(v == pat, "slot %d overwritten", i); }
    for (i = 0; i < 3; i++) { DWORD v; memcpy(&v, &w[i], 4); CHECKF(v != pat, "slot %d untouched", i); }
    fd_close(&f);
}
TEST(gap10_cwf_scales_roughly_linearly) {
    NEED_FD(); REQUIRE(CWF);
    FD a, b; float wa = 0, wb = 0;
    if (!fd_open(&a, L"Arial", -20, 400, 0, ANSI_CHARSET) || !fd_open(&b, L"Arial", -80, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CWF(a.dc, 'M', 'M', &wa); CWF(b.dc, 'M', 'M', &wb);
    CHECKF(wb / wa > 3.5f && wb / wa < 4.5f, "20px %f 80px %f", wa, wb);
    fd_close(&a); fd_close(&b);
}
TEST(gap10_cwf_fixed_pitch_same) {
    NEED_FD(); REQUIRE(CWF);
    FD f; float w[95]; int i, same = 1;
    if (!fd_open(&f, L"Courier New", -16, 400, 0, ANSI_CHARSET) || !face_is(&f, L"Courier New")) { t_skip("no Courier New"); return; }
    CHECK(CWF(f.dc, 32, 126, w));
    for (i = 1; i < 95; i++) if (fabsf(w[i] - w[0]) > 0.001f) same = 0;
    CHECK(same);
    fd_close(&f);
}
TEST(gap10_cwf_logical_units_under_mapping) {
    NEED_FD(); REQUIRE(CWF); REQUIRE(U(SetMapMode)); REQUIRE(U(SetWindowExtEx)); REQUIRE(U(SetViewportExtEx));
    FD f; float w1 = 0, w2 = 0;
    if (!fd_open(&f, L"Arial", -40, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CWF(f.dc, 'M', 'M', &w1);
    U(SetMapMode)(f.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(f.dc, 1, 1, NULL); U(SetViewportExtEx)(f.dc, 2, 1, NULL);
    CWF(f.dc, 'M', 'M', &w2);
    t_note("float width of 'M': %f (MM_TEXT) -> %f (x scale 2)", w1, w2);
    CHECK(w2 > 0);
    fd_close(&f);
}
TEST(gap10_cwf_repeated_stable) {
    NEED_FD(); REQUIRE(CWF);
    FD f; float a[256], b[256]; int i, ok = 1;
    if (!fd_open(&f, L"Segoe UI", -15, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CWF(f.dc, 0, 255, a);
    for (i = 0; i < 50; i++) { CWF(f.dc, 0, 255, b); if (memcmp(a, b, sizeof a)) ok = 0; }
    CHECK(ok);
    fd_close(&f);
}
TEST(gap10_cwf_gdi32_vs_gdi32full) {
    typedef BOOL (WINAPI *P)(HDC, UINT, UINT, PFLOAT);
    P a = (P)t_proc_gdi32("GetCharWidthFloatA"), b = (P)t_proc_gdi32full("GetCharWidthFloatA"); REQUIRE(a); NEED_FD();
    FD f; float w1[4] = { 0 }, w2[4] = { 0 };
    if (!fd_open(&f, L"Arial", -16, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CHECK(a(f.dc, 'A', 'D', w1));
    if (b) { CHECK(b(f.dc, 'A', 'D', w2)); CHECK(!memcmp(w1, w2, sizeof w1)); }
    fd_close(&f);
}

/* ===================== GetTextExtentExPointI ===================== */
#define TEI U(GetTextExtentExPointI)
static int glyphs(HDC dc, const WCHAR *s, WORD *idx) {
    int n = lstrlenW(s);
    return U(GetGlyphIndicesW) && U(GetGlyphIndicesW)(dc, s, n, idx, GGI_MARK_NONEXISTING_GLYPHS) == (DWORD)n;
}
#define NEED_TEI() do { NEED_FD(); REQUIRE(TEI); REQUIRE(U(GetGlyphIndicesW)); } while (0)
TEST(gap10_tei_invalid_dc) {
    NEED_FD(); REQUIRE(TEI);
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef }; unsigned i; WORD g[2] = { 36, 37 }; SIZE sz; INT fit, dx[2];
    for (i = 0; i < 3; i++) { CLR(); CHECKF(!TEI(bad[i], g, 2, 100, &fit, dx, &sz), "dc %p", bad[i]); t_note("dc %p err %lu", bad[i], GetLastError()); }
}
TEST(gap10_tei_null_size) {
    NEED_TEI(); SKIP_WINE("NULL lpSize is written");
    FD f; WORD g[3]; INT fit, dx[3];
    if (!fd_open(&f, L"Arial", -16, 400, 0, ANSI_CHARSET) || !glyphs(f.dc, L"ABC", g)) { t_skip("no font"); return; }
    CLR();
    BOOL r = TEI(f.dc, g, 3, 100, &fit, dx, NULL);
    t_note("NULL lpSize -> %d err %lu", r, GetLastError());
    fd_close(&f);
}
TEST(gap10_tei_basic_dx_monotonic_and_total) {
    NEED_TEI();
    FD f; WORD g[5]; INT fit = -1, dx[5]; SIZE sz; int i;
    if (!fd_open(&f, L"Arial", -20, 400, 0, ANSI_CHARSET) || !glyphs(f.dc, L"Hello", g)) { t_skip("no font"); return; }
    CHECK(TEI(f.dc, g, 5, 10000, &fit, dx, &sz));
    EXPECT_EQ(fit, 5);
    for (i = 1; i < 5; i++) CHECKF(dx[i] > dx[i - 1], "dx[%d]=%d <= dx[%d]=%d", i, dx[i], i - 1, dx[i - 1]);
    EXPECT_EQ(dx[4], sz.cx);
    CHECK(sz.cy > 0 && sz.cy < 40);
    fd_close(&f);
}
TEST(gap10_tei_zero_count) {
    NEED_TEI();
    FD f; WORD g[1] = { 0 }; INT fit = 77, dx[1] = { 77 }; SIZE sz = { 77, 77 };
    if (!fd_open(&f, L"Arial", -16, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CLR();
    BOOL r = TEI(f.dc, g, 0, 100, &fit, dx, &sz);
    t_note("count 0 -> %d fit %d size %ld,%ld dx0 %d", r, fit, sz.cx, sz.cy, dx[0]);
    if (r) { EXPECT_EQ(sz.cx, 0); EXPECT_EQ(fit, 0); }
    fd_close(&f);
}
TEST(gap10_tei_negative_count) {
    NEED_TEI();
    FD f; WORD g[2] = { 36, 37 }; INT fit, dx[2]; SIZE sz;
    if (!fd_open(&f, L"Arial", -16, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CLR();
    BOOL r = TEI(f.dc, g, -1, 100, &fit, dx, &sz);
    CHECK(!r);
    t_note("count -1 -> %d err %lu", r, GetLastError());
    CLR();
    r = TEI(f.dc, g, INT_MIN, 100, &fit, dx, &sz);
    CHECK(!r);
    fd_close(&f);
}
TEST(gap10_tei_optional_outputs_may_be_null) {
    NEED_TEI();
    FD f; WORD g[3]; INT fit, dx[3]; SIZE sz;
    if (!fd_open(&f, L"Arial", -16, 400, 0, ANSI_CHARSET) || !glyphs(f.dc, L"abc", g)) { t_skip("no font"); return; }
    CHECK(TEI(f.dc, g, 3, 1000, NULL, NULL, &sz));
    CHECK(TEI(f.dc, g, 3, 1000, &fit, NULL, &sz));
    CHECK(TEI(f.dc, g, 3, 1000, NULL, dx, &sz));
    CHECK(TEI(f.dc, g, 3, 1000, &fit, dx, &sz));
    fd_close(&f);
}
TEST(gap10_tei_fit_boundaries) {
    NEED_TEI();
    FD f; WORD g[6]; INT fit, dx[6]; SIZE sz; int k;
    if (!fd_open(&f, L"Arial", -24, 400, 0, ANSI_CHARSET) || !glyphs(f.dc, L"ABCDEF", g)) { t_skip("no font"); return; }
    CHECK(TEI(f.dc, g, 6, 100000, &fit, dx, &sz));
    EXPECT_EQ(fit, 6);
    for (k = 1; k < 6; k++) {
        INT fx = -1;
        CHECK(TEI(f.dc, g, 6, dx[k] - 1, &fx, NULL, &sz));
        CHECKF(fx <= k, "max=dx[%d]-1 -> fit %d", k, fx);
        CHECK(TEI(f.dc, g, 6, dx[k], &fx, NULL, &sz));
        CHECKF(fx == k || fx == k + 1, "max=dx[%d] -> fit %d", k, fx);
        if (k == 1) t_note("max == dx[k]: characters fit is %d (k=%d) => %s", fx, k, fx == k + 1 ? "inclusive" : "exclusive");
        CHECK(TEI(f.dc, g, 6, dx[k] + 1, &fx, NULL, &sz));
        CHECKF(fx >= k + 1 - 0 && fx <= k + 1, "max=dx[%d]+1 -> fit %d", k, fx);
    }
    fd_close(&f);
}
TEST(gap10_tei_fit_zero_and_one) {
    NEED_TEI();
    FD f; WORD g[3]; INT fit = -1, dx[3]; SIZE sz;
    if (!fd_open(&f, L"Arial", -24, 400, 0, ANSI_CHARSET) || !glyphs(f.dc, L"WWW", g)) { t_skip("no font"); return; }
    CHECK(TEI(f.dc, g, 3, 0, &fit, dx, &sz));
    EXPECT_EQ(fit, 0);
    CHECK(TEI(f.dc, g, 3, 1, &fit, dx, &sz));
    EXPECT_EQ(fit, 0);
    fd_close(&f);
}
TEST(gap10_tei_negative_max_extent_observed) {
    NEED_TEI();
    FD f; WORD g[3]; INT fit = -5, dx[3]; SIZE sz;
    if (!fd_open(&f, L"Arial", -24, 400, 0, ANSI_CHARSET) || !glyphs(f.dc, L"ABC", g)) { t_skip("no font"); return; }
    CLR();
    BOOL r = TEI(f.dc, g, 3, -1, &fit, dx, &sz);
    t_note("nMaxExtent -1 -> %d fit %d err %lu", r, fit, GetLastError());
    r = TEI(f.dc, g, 3, INT_MIN, &fit, dx, &sz);
    t_note("nMaxExtent INT_MIN -> %d fit %d", r, fit);
    r = TEI(f.dc, g, 3, INT_MAX, &fit, dx, &sz);
    CHECK(r); EXPECT_EQ(fit, 3);
    fd_close(&f);
}
TEST(gap10_tei_matches_textextentpointi) {
    NEED_TEI(); REQUIRE(U(GetTextExtentPointI));
    FD f; WORD g[6]; SIZE a, b; INT dx[6], fit;
    if (!fd_open(&f, L"Tahoma", -17, 400, 0, ANSI_CHARSET) || !glyphs(f.dc, L"Tahoma", g)) { t_skip("no font"); return; }
    CHECK(TEI(f.dc, g, 6, 100000, &fit, dx, &a)); CHECK(U(GetTextExtentPointI)(f.dc, g, 6, &b));
    EXPECT_EQ(a.cx, b.cx); EXPECT_EQ(a.cy, b.cy);
    fd_close(&f);
}
TEST(gap10_tei_matches_charwidth_i_sum) {
    NEED_TEI(); REQUIRE(U(GetCharWidthI));
    FD f; WORD g[5]; SIZE a; INT dx[5], fit, w[5]; int i, sum = 0;
    if (!fd_open(&f, L"Arial", -18, 400, 0, ANSI_CHARSET) || !glyphs(f.dc, L"World", g)) { t_skip("no font"); return; }
    CHECK(TEI(f.dc, g, 5, 100000, &fit, dx, &a));
    CHECK(U(GetCharWidthI)(f.dc, 0, 5, g, w));
    for (i = 0; i < 5; i++) { sum += w[i]; CHECKF(abs(dx[i] - sum) <= 1, "partial sum %d: dx %d widths %d", i, dx[i], sum); }
    fd_close(&f);
}
TEST(gap10_tei_matches_wide_text_extent) {
    NEED_TEI(); REQUIRE(U(GetTextExtentExPointW));
    FD f; WORD g[7]; SIZE a, b; INT dxa[7], dxb[7], fa, fb; int i;
    if (!fd_open(&f, L"Arial", -20, 400, 0, ANSI_CHARSET) || !glyphs(f.dc, L"Glyphs!", g)) { t_skip("no font"); return; }
    CHECK(TEI(f.dc, g, 7, 100000, &fa, dxa, &a));
    CHECK(U(GetTextExtentExPointW)(f.dc, L"Glyphs!", 7, 100000, &fb, dxb, &b));
    EXPECT_EQ(a.cx, b.cx); EXPECT_EQ(fa, fb);
    for (i = 0; i < 7; i++) CHECKF(dxa[i] == dxb[i], "dx[%d] %d vs %d", i, dxa[i], dxb[i]);
    fd_close(&f);
}
TEST(gap10_tei_linear_in_repeat_count) {
    NEED_TEI();
    FD f; WORD g[1]; WORD *many; INT *dx; SIZE s1, sn; INT fit; int n = 1000;
    if (!fd_open(&f, L"Arial", -16, 400, 0, ANSI_CHARSET) || !glyphs(f.dc, L"i", g)) { t_skip("no font"); return; }
    many = (WORD *)HeapAlloc(GetProcessHeap(), 0, n * sizeof(WORD)); dx = (INT *)HeapAlloc(GetProcessHeap(), 0, n * sizeof(INT));
    { int i; for (i = 0; i < n; i++) many[i] = g[0]; }
    CHECK(TEI(f.dc, g, 1, 100000, &fit, dx, &s1));
    CHECK(TEI(f.dc, many, n, INT_MAX, &fit, dx, &sn));
    CHECKF(abs(sn.cx - n * s1.cx) <= n / 8 + 1, "1 glyph %ld, %d glyphs %ld", s1.cx, n, sn.cx);
    EXPECT_EQ(fit, n);
    EXPECT_EQ(dx[n - 1], sn.cx);
    HeapFree(GetProcessHeap(), 0, many); HeapFree(GetProcessHeap(), 0, dx);
    fd_close(&f);
}
TEST(gap10_tei_height_equals_text_metrics) {
    NEED_TEI(); REQUIRE(U(GetTextMetricsW));
    FD f; WORD g[1]; SIZE sz; TEXTMETRICW tm; INT fit, dx[1];
    if (!fd_open(&f, L"Arial", -30, 400, 0, ANSI_CHARSET) || !glyphs(f.dc, L"H", g)) { t_skip("no font"); return; }
    CHECK(TEI(f.dc, g, 1, 1000, &fit, dx, &sz)); CHECK(U(GetTextMetricsW)(f.dc, &tm));
    EXPECT_EQ(sz.cy, tm.tmHeight);
    fd_close(&f);
}
TEST(gap10_tei_nonexistent_glyph_index_observed) {
    NEED_TEI();
    FD f; WORD g[3] = { 0, 0xFFFF, 0xFFFE }; INT fit = -1, dx[3] = { 0 }; SIZE sz = { 0 };
    if (!fd_open(&f, L"Arial", -16, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CLR();
    BOOL r = TEI(f.dc, g, 3, 1000, &fit, dx, &sz);
    t_note("glyphs {0,0xFFFF,0xFFFE} -> %d size %ld fit %d dx %d,%d,%d", r, sz.cx, fit, dx[0], dx[1], dx[2]);
    fd_close(&f);
}
TEST(gap10_tei_bitmap_font_observed) {
    NEED_TEI();
    HDC dc = U(CreateCompatibleDC)(NULL); WORD g[3] = { 'A', 'B', 'C' }; INT fit = -1, dx[3]; SIZE sz = { 0 };
    CLR();
    BOOL r = TEI(dc, g, 3, 1000, &fit, dx, &sz);
    t_note("default bitmap font with indices 'A','B','C' -> %d size %ld fit %d", r, sz.cx, fit);
    U(DeleteDC)(dc);
}
TEST(gap10_tei_different_fonts_differ) {
    NEED_TEI();
    FD a, b; WORD ga[3], gb[3]; SIZE sa, sb; INT fit, dx[3];
    if (!fd_open(&a, L"Arial", -24, 400, 0, ANSI_CHARSET) || !fd_open(&b, L"Courier New", -24, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    if (!glyphs(a.dc, L"iii", ga) || !glyphs(b.dc, L"iii", gb)) { t_skip("no glyph indices"); return; }
    CHECK(TEI(a.dc, ga, 3, 10000, &fit, dx, &sa)); CHECK(TEI(b.dc, gb, 3, 10000, &fit, dx, &sb));
    CHECKF(sb.cx > sa.cx, "Courier %ld vs Arial %ld", sb.cx, sa.cx);
    fd_close(&a); fd_close(&b);
}
TEST(gap10_tei_no_gdi_leak) {
    NEED_TEI();
    if (!g10_gdicount()) { t_skip("no GetGuiResources"); return; }
    FD f; WORD g[4]; SIZE sz; INT fit, dx[4]; int i; DWORD before;
    if (!fd_open(&f, L"Arial", -16, 400, 0, ANSI_CHARSET) || !glyphs(f.dc, L"abcd", g)) { t_skip("no font"); return; }
    before = g10_gdicount();
    for (i = 0; i < 200; i++) TEI(f.dc, g, 4, 50, &fit, dx, &sz);
    EXPECT_EQ(g10_gdicount(), before);
    fd_close(&f);
}
static void tei_thr(void *p) {
    FD f; WORD g[3]; SIZE sz; INT fit, dx[3]; LONG *out = (LONG *)p;
    if (!fd_open(&f, L"Arial", -20, 400, 0, ANSI_CHARSET) || !glyphs(f.dc, L"abc", g)) return;
    if (TEI(f.dc, g, 3, 1000, &fit, dx, &sz)) *out = sz.cx;
    fd_close(&f);
}
TEST(gap10_tei_other_thread_same_result) {
    NEED_TEI();
    FD f; WORD g[3]; SIZE sz; INT fit, dx[3]; LONG other = -1;
    if (!fd_open(&f, L"Arial", -20, 400, 0, ANSI_CHARSET) || !glyphs(f.dc, L"abc", g)) { t_skip("no font"); return; }
    CHECK(TEI(f.dc, g, 3, 1000, &fit, dx, &sz));
    g10_run_thread(tei_thr, &other);
    EXPECT_EQ(other, sz.cx);
    fd_close(&f);
}
TEST(gap10_tei_gdi32_vs_gdi32full) {
    typedef BOOL (WINAPI *P)(HDC, LPWORD, int, int, LPINT, LPINT, LPSIZE);
    P a = (P)t_proc_gdi32("GetTextExtentExPointI"), b = (P)t_proc_gdi32full("GetTextExtentExPointI"); REQUIRE(a); NEED_FD(); REQUIRE(U(GetGlyphIndicesW));
    FD f; WORD g[3]; SIZE s1, s2; INT fit, dx[3];
    if (!fd_open(&f, L"Arial", -16, 400, 0, ANSI_CHARSET) || !glyphs(f.dc, L"abc", g)) { t_skip("no font"); return; }
    CHECK(a(f.dc, g, 3, 1000, &fit, dx, &s1));
    if (b) { CHECK(b(f.dc, g, 3, 1000, &fit, dx, &s2)); EXPECT_EQ(s1.cx, s2.cx); }
    fd_close(&f);
}

/* ===================== GetGlyphOutlineA ===================== */
#define GGA U(GetGlyphOutlineA)
static MAT2 ident(void) { MAT2 m; memset(&m, 0, sizeof m); m.eM11.value = 1; m.eM22.value = 1; return m; }
#define NEED_GGO() do { NEED_FD(); REQUIRE(GGA); } while (0)
TEST(gap10_gga_invalid_dc) {
    NEED_GGO();
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef }; unsigned i; GLYPHMETRICS gm; MAT2 m = ident();
    for (i = 0; i < 3; i++) { CLR(); CHECKF(GGA(bad[i], 'A', GGO_METRICS, &gm, 0, NULL, &m) == GDI_ERROR, "dc %p", bad[i]); t_note("dc %p err %lu", bad[i], GetLastError()); }
}
TEST(gap10_gga_wrong_object_type) {
    NEED_GGO(); REQUIRE(U(CreateSolidBrush));
    HBRUSH b = U(CreateSolidBrush)(0); GLYPHMETRICS gm; MAT2 m = ident();
    CHECK(GGA((HDC)b, 'A', GGO_METRICS, &gm, 0, NULL, &m) == GDI_ERROR);
    U(DeleteObject)(b);
}
TEST(gap10_gga_null_gm_and_mat_observed) {
    NEED_GGO(); SKIP_WINE("NULL lpgm/lpmat2 dereferenced");
    FD f; GLYPHMETRICS gm; MAT2 m = ident();
    if (!fd_open(&f, L"Arial", -48, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CLR(); DWORD r = GGA(f.dc, 'A', GGO_METRICS, NULL, 0, NULL, &m);
    t_note("NULL lpgm -> 0x%lx", r);
    CLR(); r = GGA(f.dc, 'A', GGO_METRICS, &gm, 0, NULL, NULL);
    t_note("NULL lpmat2 -> 0x%lx err %lu", r, GetLastError());
    fd_close(&f);
}
TEST(gap10_gga_metrics_arial_A) {
    NEED_GGO();
    FD f; GLYPHMETRICS gm; MAT2 m = ident(); DWORD r;
    if (!fd_open(&f, L"Arial", -64, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    memset(&gm, 0xCC, sizeof gm);
    r = GGA(f.dc, 'A', GGO_METRICS, &gm, 0, NULL, &m);
    CHECK(r != GDI_ERROR);
    CHECK(gm.gmBlackBoxX > 20 && gm.gmBlackBoxX < 64);
    CHECK(gm.gmBlackBoxY > 30 && gm.gmBlackBoxY < 64);
    CHECK(gm.gmCellIncX > 20 && gm.gmCellIncX < 64);
    EXPECT_EQ(gm.gmCellIncY, 0);
    CHECK(gm.gmptGlyphOrigin.y > 0 && gm.gmptGlyphOrigin.y <= 64);
    t_note("GGO_METRICS ret 0x%lx box %lux%lu origin %ld,%ld inc %d", r, gm.gmBlackBoxX, gm.gmBlackBoxY, gm.gmptGlyphOrigin.x, gm.gmptGlyphOrigin.y, gm.gmCellIncX);
    fd_close(&f);
}
TEST(gap10_gga_metrics_same_as_wide) {
    NEED_GGO(); REQUIRE(U(GetGlyphOutlineW));
    FD f; GLYPHMETRICS a, b; MAT2 m = ident(); int c;
    if (!sbcs()) { t_skip("DBCS ACP"); return; }
    if (!fd_open(&f, L"Arial", -48, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    for (c = 'A'; c <= 'z'; c += 5) {
        memset(&a, 1, sizeof a); memset(&b, 2, sizeof b);
        DWORD ra = GGA(f.dc, c, GGO_METRICS, &a, 0, NULL, &m), rb = U(GetGlyphOutlineW)(f.dc, c, GGO_METRICS, &b, 0, NULL, &m);
        CHECKF(ra == rb, "char %d: ret %lx vs %lx", c, ra, rb);
        CHECKF(!memcmp(&a, &b, sizeof a), "char %d metrics differ", c);
    }
    fd_close(&f);
}
TEST(gap10_gga_high_ansi_char_matches_wide_in_1252) {
    NEED_GGO(); REQUIRE(U(GetGlyphOutlineW));
    FD f; GLYPHMETRICS a, b; MAT2 m = ident();
    if (GetACP() != 1252) { t_skip("ACP != 1252"); return; }
    if (!fd_open(&f, L"Arial", -48, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    DWORD ra = GGA(f.dc, 0xE9, GGO_METRICS, &a, 0, NULL, &m), rb = U(GetGlyphOutlineW)(f.dc, 0xE9, GGO_METRICS, &b, 0, NULL, &m);
    CHECK(ra != GDI_ERROR); EXPECT_EQ(ra, rb);
    CHECK(!memcmp(&a, &b, sizeof a));
    ra = GGA(f.dc, 0x80, GGO_METRICS, &a, 0, NULL, &m);       /* euro sign in cp1252 */
    rb = U(GetGlyphOutlineW)(f.dc, 0x20AC, GGO_METRICS, &b, 0, NULL, &m);
    CHECK(ra != GDI_ERROR); EXPECT_EQ(ra, rb);
    CHECK(!memcmp(&a, &b, sizeof a));
    fd_close(&f);
}
TEST(gap10_gga_native_size_query_then_fetch) {
    NEED_GGO();
    FD f; GLYPHMETRICS gm; MAT2 m = ident(); DWORD sz, got; BYTE *buf;
    if (!fd_open(&f, L"Arial", -64, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    sz = GGA(f.dc, 'B', GGO_NATIVE, &gm, 0, NULL, &m);
    CHECK(sz != GDI_ERROR && sz > sizeof(TTPOLYGONHEADER));
    buf = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sz + 16);
    memset(buf, 0xCD, sz + 16);
    got = GGA(f.dc, 'B', GGO_NATIVE, &gm, sz, buf, &m);
    EXPECT_EQ(got, sz);
    { int i; int ok = 1; for (i = 0; i < 16; i++) if (buf[sz + i] != 0xCD) ok = 0; CHECK(ok); }
    {   /* contour records add up to the whole buffer */
        DWORD off = 0; int cont = 0; int ok = 1;
        while (off < sz) {
            TTPOLYGONHEADER *h = (TTPOLYGONHEADER *)(buf + off);
            if (h->dwType != TT_POLYGON_TYPE || h->cb < sizeof *h || off + h->cb > sz) { ok = 0; break; }
            off += h->cb; cont++;
        }
        CHECK(ok); EXPECT_EQ(off, sz);
        CHECKF(cont == 3, "Arial 'B' has 3 contours, got %d", cont);
    }
    HeapFree(GetProcessHeap(), 0, buf);
    fd_close(&f);
}
TEST(gap10_gga_native_buffer_too_small) {
    NEED_GGO();
    FD f; GLYPHMETRICS gm; MAT2 m = ident(); DWORD sz; BYTE *buf;
    if (!fd_open(&f, L"Arial", -64, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    sz = GGA(f.dc, 'B', GGO_NATIVE, &gm, 0, NULL, &m);
    CHECK(sz != GDI_ERROR && sz > 8);
    buf = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sz);
    CHECK(GGA(f.dc, 'B', GGO_NATIVE, &gm, sz - 1, buf, &m) == GDI_ERROR);
    CHECK(GGA(f.dc, 'B', GGO_NATIVE, &gm, 1, buf, &m) == GDI_ERROR);
    CHECK(GGA(f.dc, 'B', GGO_NATIVE, &gm, sz, buf, &m) == sz);
    CHECK(GGA(f.dc, 'B', GGO_NATIVE, &gm, sz + 100, buf, &m) == sz);
    HeapFree(GetProcessHeap(), 0, buf);
    fd_close(&f);
}
TEST(gap10_gga_native_null_buffer_with_size) {
    NEED_GGO();
    FD f; GLYPHMETRICS gm; MAT2 m = ident(); DWORD sz, r;
    if (!fd_open(&f, L"Arial", -48, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    sz = GGA(f.dc, 'O', GGO_NATIVE, &gm, 0, NULL, &m);
    CLR();
    r = GGA(f.dc, 'O', GGO_NATIVE, &gm, 4096, NULL, &m);
    t_note("cjBuffer 4096 with NULL pvBuffer -> 0x%lx (size query was %lu)", r, sz);
    fd_close(&f);
}
TEST(gap10_gga_native_polygon_header_consistency) {
    NEED_GGO();
    FD f; GLYPHMETRICS gm; MAT2 m = ident(); DWORD sz; BYTE *buf; TTPOLYGONHEADER *h;
    if (!fd_open(&f, L"Arial", -64, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    sz = GGA(f.dc, 'A', GGO_NATIVE, &gm, 0, NULL, &m);
    CHECK(sz != GDI_ERROR);
    buf = (BYTE *)HeapAlloc(GetProcessHeap(), 0, sz);
    GGA(f.dc, 'A', GGO_NATIVE, &gm, sz, buf, &m);
    h = (TTPOLYGONHEADER *)buf;
    EXPECT_EQ(h->dwType, TT_POLYGON_TYPE);
    CHECK(h->cb >= sizeof *h);
    {
        struct { WORD wType; WORD cpfx; } *pl = (void *)(buf + sizeof *h);
        CHECK(pl->wType == TT_PRIM_LINE || pl->wType == TT_PRIM_QSPLINE);
        CHECK(pl->cpfx >= 1);
        CHECK(sizeof *h + 4 + pl->cpfx * sizeof(POINTFX) <= h->cb);
    }
    HeapFree(GetProcessHeap(), 0, buf);
    fd_close(&f);
}
TEST(gap10_gga_bitmap_size_formula) {
    NEED_GGO();
    FD f; GLYPHMETRICS gm; MAT2 m = ident(); DWORD sz, want; int c;
    if (!fd_open(&f, L"Arial", -40, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    for (c = 'A'; c <= 'Z'; c += 3) {
        sz = GGA(f.dc, c, GGO_BITMAP, &gm, 0, NULL, &m);
        CHECKF(sz != GDI_ERROR, "char %c", c);
        want = ((gm.gmBlackBoxX + 31) / 32) * 4 * gm.gmBlackBoxY;
        CHECKF(sz == want, "char %c: size %lu want %lu (box %lux%lu)", c, sz, want, gm.gmBlackBoxX, gm.gmBlackBoxY);
    }
    fd_close(&f);
}
TEST(gap10_gga_bitmap_contains_ink) {
    NEED_GGO();
    FD f; GLYPHMETRICS gm; MAT2 m = ident(); DWORD sz; BYTE *buf; DWORD i; int ink = 0;
    if (!fd_open(&f, L"Arial", -40, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    sz = GGA(f.dc, 'I', GGO_BITMAP, &gm, 0, NULL, &m);
    CHECK(sz != GDI_ERROR && sz > 0);
    buf = (BYTE *)HeapAlloc(GetProcessHeap(), 0, sz);
    CHECK(GGA(f.dc, 'I', GGO_BITMAP, &gm, sz, buf, &m) == sz);
    for (i = 0; i < sz; i++) { BYTE b = buf[i]; while (b) { ink += b & 1; b >>= 1; } }
    CHECKF(ink > 10, "set bits %d", ink);
    CHECKF((DWORD)ink <= gm.gmBlackBoxX * gm.gmBlackBoxY, "ink %d > box", ink);
    HeapFree(GetProcessHeap(), 0, buf);
    fd_close(&f);
}
TEST(gap10_gga_bitmap_buffer_too_small) {
    NEED_GGO();
    FD f; GLYPHMETRICS gm; MAT2 m = ident(); DWORD sz; BYTE buf[4096];
    if (!fd_open(&f, L"Arial", -40, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    sz = GGA(f.dc, 'M', GGO_BITMAP, &gm, 0, NULL, &m);
    CHECK(sz != GDI_ERROR && sz > 1 && sz < sizeof buf);
    CHECK(GGA(f.dc, 'M', GGO_BITMAP, &gm, sz - 1, buf, &m) == GDI_ERROR);
    CHECK(GGA(f.dc, 'M', GGO_BITMAP, &gm, sz, buf, &m) == sz);
    fd_close(&f);
}
TEST(gap10_gga_gray_bitmap_sizes_and_levels) {
    NEED_GGO();
    UINT fmts[] = { GGO_GRAY2_BITMAP, GGO_GRAY4_BITMAP, GGO_GRAY8_BITMAP }; unsigned maxv[] = { 4, 16, 64 }; unsigned k;
    FD f; MAT2 m = ident();
    if (!fd_open(&f, L"Arial", -40, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    for (k = 0; k < 3; k++) {
        GLYPHMETRICS gm; DWORD sz, want, i; BYTE *buf; unsigned mx = 0;
        sz = GGA(f.dc, 'S', fmts[k], &gm, 0, NULL, &m);
        CHECKF(sz != GDI_ERROR, "fmt %u", fmts[k]);
        if (sz == GDI_ERROR) continue;
        want = ((gm.gmBlackBoxX + 3) & ~3u) * gm.gmBlackBoxY;
        CHECKF(sz == want, "fmt %u size %lu want %lu (box %lux%lu)", fmts[k], sz, want, gm.gmBlackBoxX, gm.gmBlackBoxY);
        buf = (BYTE *)HeapAlloc(GetProcessHeap(), 0, sz);
        CHECK(GGA(f.dc, 'S', fmts[k], &gm, sz, buf, &m) == sz);
        for (i = 0; i < sz; i++) if (buf[i] > mx) mx = buf[i];
        CHECKF(mx > 0 && mx <= maxv[k], "fmt %u max level %u", fmts[k], mx);
        HeapFree(GetProcessHeap(), 0, buf);
    }
    fd_close(&f);
}
TEST(gap10_gga_space_glyph) {
    NEED_GGO();
    FD f; GLYPHMETRICS gm; MAT2 m = ident(); DWORD r;
    if (!fd_open(&f, L"Arial", -40, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    r = GGA(f.dc, ' ', GGO_METRICS, &gm, 0, NULL, &m);
    CHECK(r != GDI_ERROR);
    CHECK(gm.gmCellIncX > 0);
    r = GGA(f.dc, ' ', GGO_NATIVE, &gm, 0, NULL, &m);
    t_note("space GGO_NATIVE size %lu", r);
    CHECK(r == 0);
    r = GGA(f.dc, ' ', GGO_BITMAP, &gm, 0, NULL, &m);
    t_note("space GGO_BITMAP size %lu box %lux%lu", r, gm.gmBlackBoxX, gm.gmBlackBoxY);
    CHECK(r != GDI_ERROR);
    fd_close(&f);
}
TEST(gap10_gga_glyph_index_flag_matches_char) {
    NEED_GGO(); REQUIRE(U(GetGlyphIndicesW));
    FD f; GLYPHMETRICS a, b; MAT2 m = ident(); WORD gi;
    if (!fd_open(&f, L"Arial", -48, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CHECK(U(GetGlyphIndicesW)(f.dc, L"Q", 1, &gi, 0) == 1);
    DWORD ra = GGA(f.dc, 'Q', GGO_METRICS, &a, 0, NULL, &m), rb = GGA(f.dc, gi, GGO_METRICS | GGO_GLYPH_INDEX, &b, 0, NULL, &m);
    CHECK(ra != GDI_ERROR && rb != GDI_ERROR);
    CHECK(!memcmp(&a, &b, sizeof a));
    fd_close(&f);
}
TEST(gap10_gga_glyph_index_out_of_range) {
    NEED_GGO();
    FD f; GLYPHMETRICS gm; MAT2 m = ident();
    if (!fd_open(&f, L"Arial", -48, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    CLR();
    DWORD r = GGA(f.dc, 0xFFFF, GGO_METRICS | GGO_GLYPH_INDEX, &gm, 0, NULL, &m);
    t_note("glyph index 0xFFFF -> 0x%lx err %lu", r, GetLastError());
    r = GGA(f.dc, 0, GGO_METRICS | GGO_GLYPH_INDEX, &gm, 0, NULL, &m);
    CHECK(r != GDI_ERROR);
    fd_close(&f);
}
TEST(gap10_gga_scaling_matrix) {
    NEED_GGO();
    FD f; GLYPHMETRICS g1, g2; MAT2 m1 = ident(), m2 = ident();
    if (!fd_open(&f, L"Arial", -48, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    m2.eM11.value = 2; m2.eM22.value = 2;
    CHECK(GGA(f.dc, 'H', GGO_METRICS, &g1, 0, NULL, &m1) != GDI_ERROR);
    CHECK(GGA(f.dc, 'H', GGO_METRICS, &g2, 0, NULL, &m2) != GDI_ERROR);
    CHECKF(g2.gmBlackBoxX >= g1.gmBlackBoxX * 17 / 10 && g2.gmBlackBoxX <= g1.gmBlackBoxX * 23 / 10, "x %lu -> %lu", g1.gmBlackBoxX, g2.gmBlackBoxX);
    CHECKF(g2.gmBlackBoxY >= g1.gmBlackBoxY * 17 / 10 && g2.gmBlackBoxY <= g1.gmBlackBoxY * 23 / 10, "y %lu -> %lu", g1.gmBlackBoxY, g2.gmBlackBoxY);
    fd_close(&f);
}
TEST(gap10_gga_rotation_matrix_90) {
    NEED_GGO();
    FD f; GLYPHMETRICS g1, g2; MAT2 m1 = ident(), m2; memset(&m2, 0, sizeof m2);
    if (!fd_open(&f, L"Arial", -48, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    m2.eM12.value = 1; m2.eM21.value = -1;     /* swap axes */
    CHECK(GGA(f.dc, 'L', GGO_METRICS, &g1, 0, NULL, &m1) != GDI_ERROR);
    CHECK(GGA(f.dc, 'L', GGO_METRICS, &g2, 0, NULL, &m2) != GDI_ERROR);
    CHECKF(abs((int)g2.gmBlackBoxX - (int)g1.gmBlackBoxY) <= 2 && abs((int)g2.gmBlackBoxY - (int)g1.gmBlackBoxX) <= 2, "box %lux%lu -> %lux%lu", g1.gmBlackBoxX, g1.gmBlackBoxY, g2.gmBlackBoxX, g2.gmBlackBoxY);
    fd_close(&f);
}
TEST(gap10_gga_bitmap_font_native_fails) {
    NEED_GGO(); REQUIRE(U(GetStockObject));
    HDC dc = U(CreateCompatibleDC)(NULL); HGDIOBJ old = U(SelectObject)(dc, U(GetStockObject)(SYSTEM_FIXED_FONT)); GLYPHMETRICS gm; MAT2 m = ident();
    CLR();
    DWORD r = GGA(dc, 'A', GGO_NATIVE, &gm, 0, NULL, &m);
    CHECK(r == GDI_ERROR);
    r = GGA(dc, 'A', GGO_METRICS, &gm, 0, NULL, &m);
    t_note("raster font GGO_METRICS -> 0x%lx", r);
    r = GGA(dc, 'A', GGO_BITMAP, &gm, 0, NULL, &m);
    t_note("raster font GGO_BITMAP -> 0x%lx", r);
    U(SelectObject)(dc, old); U(DeleteDC)(dc);
}
TEST(gap10_gga_invalid_format_observed) {
    NEED_GGO();
    FD f; GLYPHMETRICS gm; MAT2 m = ident(); UINT fm[] = { 0x1234, 8, 9, 0x20, 0x40 }; unsigned i;
    if (!fd_open(&f, L"Arial", -48, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    for (i = 0; i < sizeof fm / sizeof *fm; i++) { CLR(); DWORD r = GGA(f.dc, 'A', fm[i], &gm, 0, NULL, &m); t_note("format 0x%x -> 0x%lx err %lu", fm[i], r, GetLastError()); }
    fd_close(&f);
}
TEST(gap10_gga_unhinted_flag) {
    NEED_GGO();
    FD f; GLYPHMETRICS g1, g2; MAT2 m = ident();
    if (!fd_open(&f, L"Arial", -17, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    DWORD a = GGA(f.dc, 'a', GGO_METRICS, &g1, 0, NULL, &m), b = GGA(f.dc, 'a', GGO_METRICS | GGO_UNHINTED, &g2, 0, NULL, &m);
    CHECK(a != GDI_ERROR && b != GDI_ERROR);
    CHECK(abs((int)g1.gmBlackBoxX - (int)g2.gmBlackBoxX) <= 2);
    fd_close(&f);
}
TEST(gap10_gga_italic_font_origin) {
    NEED_GGO();
    FD f; GLYPHMETRICS g1; MAT2 m = ident();
    if (!fd_open(&f, L"Arial", -48, 400, 1, ANSI_CHARSET)) { t_skip("no font"); return; }
    CHECK(GGA(f.dc, 'f', GGO_METRICS, &g1, 0, NULL, &m) != GDI_ERROR);
    CHECK(g1.gmBlackBoxX > 0);
    fd_close(&f);
}
TEST(gap10_gga_repeated_calls_deterministic) {
    NEED_GGO();
    FD f; GLYPHMETRICS g1, g2; MAT2 m = ident(); int i, ok = 1; DWORD s1, s2;
    if (!fd_open(&f, L"Times New Roman", -48, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    s1 = GGA(f.dc, 'g', GGO_BITMAP, &g1, 0, NULL, &m);
    for (i = 0; i < 100; i++) { s2 = GGA(f.dc, 'g', GGO_BITMAP, &g2, 0, NULL, &m); if (s1 != s2 || memcmp(&g1, &g2, sizeof g1)) ok = 0; }
    CHECK(ok);
    fd_close(&f);
}
TEST(gap10_gga_no_gdi_leak) {
    NEED_GGO();
    if (!g10_gdicount()) { t_skip("no GetGuiResources"); return; }
    FD f; GLYPHMETRICS gm; MAT2 m = ident(); int i; DWORD before; BYTE buf[2048];
    if (!fd_open(&f, L"Arial", -24, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    before = g10_gdicount();
    for (i = 0; i < 200; i++) GGA(f.dc, 'a' + (i % 26), GGO_BITMAP, &gm, sizeof buf, buf, &m);
    EXPECT_EQ(g10_gdicount(), before);
    fd_close(&f);
}
TEST(gap10_gga_gdi32_vs_gdi32full) {
    typedef DWORD (WINAPI *P)(HDC, UINT, UINT, LPGLYPHMETRICS, DWORD, LPVOID, const MAT2 *);
    P a = (P)t_proc_gdi32("GetGlyphOutlineA"), b = (P)t_proc_gdi32full("GetGlyphOutlineA"); REQUIRE(a); NEED_FD();
    FD f; GLYPHMETRICS g1, g2; MAT2 m = ident();
    if (!fd_open(&f, L"Arial", -48, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    DWORD r1 = a(f.dc, 'A', GGO_METRICS, &g1, 0, NULL, &m);
    if (b) { DWORD r2 = b(f.dc, 'A', GGO_METRICS, &g2, 0, NULL, &m); EXPECT_EQ(r1, r2); CHECK(!memcmp(&g1, &g2, sizeof g1)); }
    fd_close(&f);
}

/* ===================== GetOutlineTextMetricsW ===================== */
#define OTM U(GetOutlineTextMetricsW)
#define NEED_OTM() do { NEED_FD(); REQUIRE(OTM); REQUIRE(U(GetTextMetricsW)); } while (0)
static OUTLINETEXTMETRICW *get_otm(HDC dc, UINT *size) {
    UINT s = OTM(dc, 0, NULL);
    OUTLINETEXTMETRICW *p;
    if (!s) return NULL;
    p = (OUTLINETEXTMETRICW *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, s + 16);
    if (!p) return NULL;
    if (OTM(dc, s, p) != s) { HeapFree(GetProcessHeap(), 0, p); return NULL; }
    if (size) *size = s;
    return p;
}
TEST(gap10_otm_invalid_dc) {
    NEED_OTM();
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef }; unsigned i; OUTLINETEXTMETRICW o;
    for (i = 0; i < 3; i++) { CLR(); CHECKF(OTM(bad[i], 0, NULL) == 0, "size query dc %p", bad[i]); CHECKF(OTM(bad[i], sizeof o, &o) == 0, "fetch dc %p", bad[i]); }
}
TEST(gap10_otm_wrong_object_and_deleted_dc) {
    NEED_OTM(); REQUIRE(U(CreateSolidBrush));
    HBRUSH b = U(CreateSolidBrush)(0); HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(OTM((HDC)b, 0, NULL) == 0);
    U(DeleteObject)(b); U(DeleteDC)(dc);
    CHECK(OTM(dc, 0, NULL) == 0);
}
TEST(gap10_otm_size_query_arial) {
    NEED_OTM();
    FD f; UINT s1, s2;
    if (!fd_open(&f, L"Arial", -24, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    s1 = OTM(f.dc, 0, NULL);
    CHECKF(s1 > sizeof(OUTLINETEXTMETRICW), "size %u (struct %zu)", s1, sizeof(OUTLINETEXTMETRICW));
    s2 = OTM(f.dc, 100000, NULL);       /* cjCopy ignored when the buffer is NULL */
    EXPECT_EQ(s1, s2);
    s2 = OTM(f.dc, 1, NULL);
    EXPECT_EQ(s1, s2);
    t_note("OUTLINETEXTMETRICW size for Arial: %u", s1);
    fd_close(&f);
}
TEST(gap10_otm_fetch_fields) {
    NEED_OTM();
    FD f; UINT sz; OUTLINETEXTMETRICW *o; TEXTMETRICW tm;
    if (!fd_open(&f, L"Arial", -24, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    o = get_otm(f.dc, &sz);
    CHECK(o != NULL);
    if (!o) { fd_close(&f); return; }
    EXPECT_EQ(o->otmSize, sz);
    CHECK(U(GetTextMetricsW)(f.dc, &tm));
    EXPECT_EQ(o->otmTextMetrics.tmHeight, tm.tmHeight);
    EXPECT_EQ(o->otmTextMetrics.tmAscent, tm.tmAscent);
    EXPECT_EQ(o->otmTextMetrics.tmDescent, tm.tmDescent);
    EXPECT_EQ(o->otmTextMetrics.tmInternalLeading, tm.tmInternalLeading);
    EXPECT_EQ(o->otmTextMetrics.tmExternalLeading, tm.tmExternalLeading);
    EXPECT_EQ(o->otmTextMetrics.tmAveCharWidth, tm.tmAveCharWidth);
    EXPECT_EQ(o->otmTextMetrics.tmMaxCharWidth, tm.tmMaxCharWidth);
    EXPECT_EQ(o->otmTextMetrics.tmWeight, tm.tmWeight);
    EXPECT_EQ(o->otmTextMetrics.tmCharSet, tm.tmCharSet);
    EXPECT_EQ(o->otmEMSquare, 2048);
    CHECK(o->otmAscent > 0); CHECK(o->otmDescent < 0);
    EXPECT_EQ(o->otmItalicAngle, 0);
    CHECK(o->otmTextMetrics.tmPitchAndFamily & TMPF_TRUETYPE);
    CHECK(o->otmTextMetrics.tmPitchAndFamily & TMPF_VECTOR);
    CHECK(o->otmsUnderscoreSize != 0);
    CHECK(o->otmsXHeight > 0 && o->otmsCapEmHeight > 0);
    CHECK(o->otmsCapEmHeight > o->otmsXHeight);
    HeapFree(GetProcessHeap(), 0, o);
    fd_close(&f);
}
TEST(gap10_otm_names_via_offsets) {
    NEED_OTM();
    FD f; UINT sz; OUTLINETEXTMETRICW *o; const WCHAR *s;
    if (!fd_open(&f, L"Arial", -24, 400, 0, ANSI_CHARSET) || !face_is(&f, L"Arial")) { t_skip("no Arial"); fd_close(&f); return; }
    o = get_otm(f.dc, &sz);
    CHECK(o != NULL);
    if (!o) { fd_close(&f); return; }
    CHECK((UINT_PTR)o->otmpFamilyName < sz);
    CHECK((UINT_PTR)o->otmpFaceName < sz);
    CHECK((UINT_PTR)o->otmpStyleName < sz);
    CHECK((UINT_PTR)o->otmpFullName < sz);
    s = (const WCHAR *)((BYTE *)o + (UINT_PTR)o->otmpFamilyName); EXPECT_WSTR(s, L"Arial");
    s = (const WCHAR *)((BYTE *)o + (UINT_PTR)o->otmpFaceName); EXPECT_WSTR(s, L"Arial");
    s = (const WCHAR *)((BYTE *)o + (UINT_PTR)o->otmpStyleName); t_note("style '%ls'", s);
    s = (const WCHAR *)((BYTE *)o + (UINT_PTR)o->otmpFullName); t_note("full name '%ls'", s);
    HeapFree(GetProcessHeap(), 0, o);
    fd_close(&f);
}
TEST(gap10_otm_bold_and_italic_selection_bits) {
    NEED_OTM();
    FD a, b; OUTLINETEXTMETRICW *oa, *ob;
    if (!fd_open(&a, L"Arial", -24, 700, 0, ANSI_CHARSET) || !fd_open(&b, L"Arial", -24, 400, 1, ANSI_CHARSET)) { t_skip("no font"); return; }
    oa = get_otm(a.dc, NULL); ob = get_otm(b.dc, NULL);
    CHECK(oa && ob);
    if (oa) { CHECK(oa->otmfsSelection & 0x20); CHECK(!(oa->otmfsSelection & 1)); EXPECT_EQ(oa->otmTextMetrics.tmWeight, 700); HeapFree(GetProcessHeap(), 0, oa); }
    if (ob) { CHECK(ob->otmfsSelection & 1); t_note("italic angle %d slope %d/%d", ob->otmItalicAngle, ob->otmsCharSlopeRise, ob->otmsCharSlopeRun); CHECK(ob->otmTextMetrics.tmItalic != 0); HeapFree(GetProcessHeap(), 0, ob); }
    fd_close(&a); fd_close(&b);
}
TEST(gap10_otm_em_square_common_fonts) {
    NEED_OTM();
    const WCHAR *names[] = { L"Arial", L"Times New Roman", L"Courier New", L"Tahoma", L"Segoe UI", L"Consolas" }; unsigned i;
    for (i = 0; i < sizeof names / sizeof *names; i++) {
        FD f; OUTLINETEXTMETRICW *o;
        if (!fd_open(&f, names[i], -20, 400, 0, ANSI_CHARSET)) continue;
        if (!face_is(&f, names[i])) { t_note("%ls not available", names[i]); fd_close(&f); continue; }
        o = get_otm(f.dc, NULL);
        CHECKF(o != NULL, "%ls", names[i]);
        if (o) {
            CHECKF(o->otmEMSquare == 2048, "%ls EM %u", names[i], o->otmEMSquare);
            CHECKF(o->otmTextMetrics.tmPitchAndFamily & TMPF_TRUETYPE, "%ls not TT", names[i]);
            HeapFree(GetProcessHeap(), 0, o);
        }
        fd_close(&f);
    }
}
TEST(gap10_otm_fixed_pitch_flag_courier) {
    NEED_OTM();
    FD f; OUTLINETEXTMETRICW *o;
    if (!fd_open(&f, L"Courier New", -20, 400, 0, ANSI_CHARSET) || !face_is(&f, L"Courier New")) { t_skip("no Courier New"); fd_close(&f); return; }
    o = get_otm(f.dc, NULL);
    CHECK(o != NULL);
    if (o) {
        CHECK(!(o->otmTextMetrics.tmPitchAndFamily & TMPF_FIXED_PITCH));   /* TMPF_FIXED_PITCH is set for variable pitch (historic inversion) */
        EXPECT_EQ(o->otmPanoseNumber.bProportion, 9);
        HeapFree(GetProcessHeap(), 0, o);
    }
    fd_close(&f);
}
TEST(gap10_otm_buffer_sizes) {
    NEED_OTM();
    FD f; UINT sz; BYTE *buf; UINT r;
    if (!fd_open(&f, L"Arial", -24, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    sz = OTM(f.dc, 0, NULL);
    buf = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sz + 64);
    memset(buf, 0xAB, sz + 64);
    r = OTM(f.dc, sz, (OUTLINETEXTMETRICW *)buf);
    EXPECT_EQ(r, sz);
    { UINT i; int ok = 1; for (i = sz; i < sz + 64; i++) if (buf[i] != 0xAB) ok = 0; CHECK(ok); }
    memset(buf, 0xAB, sz + 64);
    r = OTM(f.dc, sz + 64, (OUTLINETEXTMETRICW *)buf);
    EXPECT_EQ(r, sz);
    memset(buf, 0xAB, sz + 64);
    CLR();
    r = OTM(f.dc, sz - 1, (OUTLINETEXTMETRICW *)buf);
    t_note("cjCopy = size-1 -> %u (err %lu)", r, GetLastError());
    r = OTM(f.dc, sizeof(OUTLINETEXTMETRICW), (OUTLINETEXTMETRICW *)buf);
    t_note("cjCopy = sizeof struct -> %u", r);
    r = OTM(f.dc, 4, (OUTLINETEXTMETRICW *)buf);
    t_note("cjCopy = 4 -> %u", r);
    r = OTM(f.dc, 0, (OUTLINETEXTMETRICW *)buf);
    t_note("cjCopy = 0 with buffer -> %u", r);
    HeapFree(GetProcessHeap(), 0, buf);
    fd_close(&f);
}
TEST(gap10_otm_bitmap_font_fails) {
    NEED_OTM(); REQUIRE(U(GetStockObject));
    int stock[] = { SYSTEM_FONT, SYSTEM_FIXED_FONT, OEM_FIXED_FONT }; unsigned i; HDC dc = U(CreateCompatibleDC)(NULL);
    for (i = 0; i < 3; i++) {
        HGDIOBJ old = U(SelectObject)(dc, U(GetStockObject)(stock[i])); OUTLINETEXTMETRICW o;
        CLR();
        UINT r = OTM(dc, 0, NULL);
        CHECKF(r == 0, "stock %d size query %u", stock[i], r);
        CHECKF(OTM(dc, sizeof o, &o) == 0, "stock %d fetch", stock[i]);
        U(SelectObject)(dc, old);
    }
    U(DeleteDC)(dc);
}
TEST(gap10_otm_terminal_raster_font_fails) {
    NEED_OTM();
    FD f; OUTLINETEXTMETRICW o;
    if (!fd_open(&f, L"Terminal", -12, 400, 0, OEM_CHARSET)) { t_skip("no font"); return; }
    if (!face_is(&f, L"Terminal")) { t_note("Terminal not selected"); fd_close(&f); return; }
    CHECK(OTM(f.dc, 0, NULL) == 0);
    CHECK(OTM(f.dc, sizeof o, &o) == 0);
    fd_close(&f);
}
TEST(gap10_otm_default_dc_font) {
    NEED_OTM();
    HDC dc = U(CreateCompatibleDC)(NULL);
    UINT r = OTM(dc, 0, NULL);
    t_note("default DC font OTM size: %u", r);
    U(DeleteDC)(dc);
}
TEST(gap10_otm_scales_with_height) {
    NEED_OTM();
    FD a, b; OUTLINETEXTMETRICW *oa, *ob;
    if (!fd_open(&a, L"Arial", -20, 400, 0, ANSI_CHARSET) || !fd_open(&b, L"Arial", -80, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    oa = get_otm(a.dc, NULL); ob = get_otm(b.dc, NULL);
    CHECK(oa && ob);
    if (oa && ob) {
        CHECK(ob->otmTextMetrics.tmHeight >= oa->otmTextMetrics.tmHeight * 3);
        CHECK(ob->otmAscent > oa->otmAscent);
        EXPECT_EQ(oa->otmEMSquare, ob->otmEMSquare);
        EXPECT_EQ(oa->otmSize, ob->otmSize);
    }
    if (oa) HeapFree(GetProcessHeap(), 0, oa);
    if (ob) HeapFree(GetProcessHeap(), 0, ob);
    fd_close(&a); fd_close(&b);
}
TEST(gap10_otm_font_stays_selected_and_repeatable) {
    NEED_OTM(); REQUIRE(U(GetCurrentObject));
    FD f; int i, ok = 1; UINT s0;
    if (!fd_open(&f, L"Arial", -24, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    s0 = OTM(f.dc, 0, NULL);
    for (i = 0; i < 100; i++) if (OTM(f.dc, 0, NULL) != s0) ok = 0;
    CHECK(ok);
    EXPECT_EQ(U(GetCurrentObject)(f.dc, OBJ_FONT), f.font);
    fd_close(&f);
}
TEST(gap10_otm_no_gdi_leak) {
    NEED_OTM();
    if (!g10_gdicount()) { t_skip("no GetGuiResources"); return; }
    FD f; int i; DWORD before; BYTE buf[2048];
    if (!fd_open(&f, L"Arial", -24, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    before = g10_gdicount();
    for (i = 0; i < 100; i++) OTM(f.dc, sizeof buf, (OUTLINETEXTMETRICW *)buf);
    EXPECT_EQ(g10_gdicount(), before);
    fd_close(&f);
}
static void otm_thr(void *p) {
    FD f;
    if (!fd_open(&f, L"Arial", -24, 400, 0, ANSI_CHARSET)) return;
    *(UINT *)p = OTM(f.dc, 0, NULL);
    fd_close(&f);
}
TEST(gap10_otm_other_thread_same_size) {
    NEED_OTM();
    FD f; UINT other = 0;
    if (!fd_open(&f, L"Arial", -24, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    g10_run_thread(otm_thr, &other);
    EXPECT_EQ(other, OTM(f.dc, 0, NULL));
    fd_close(&f);
}
TEST(gap10_otm_gdi32_vs_gdi32full) {
    typedef UINT (WINAPI *P)(HDC, UINT, LPOUTLINETEXTMETRICW);
    P a = (P)t_proc_gdi32("GetOutlineTextMetricsW"), b = (P)t_proc_gdi32full("GetOutlineTextMetricsW"); REQUIRE(a); NEED_FD();
    FD f;
    if (!fd_open(&f, L"Arial", -24, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    UINT r1 = a(f.dc, 0, NULL);
    if (b) EXPECT_EQ(r1, b(f.dc, 0, NULL));
    fd_close(&f);
}
TEST(gap10_otm_ansi_variant_size_smaller_or_equal) {
    NEED_OTM(); REQUIRE(U(GetOutlineTextMetricsA));
    FD f;
    if (!fd_open(&f, L"Arial", -24, 400, 0, ANSI_CHARSET)) { t_skip("no font"); return; }
    UINT w = OTM(f.dc, 0, NULL), a = U(GetOutlineTextMetricsA)(f.dc, 0, NULL);
    t_note("OTM W size %u, A size %u", w, a);
    CHECK(a > 0 && a <= w);
    fd_close(&f);
}

/* ===================== CreateFontIndirectExA ===================== */
#define CFX U(CreateFontIndirectExA)
static void efill(ENUMLOGFONTEXDVA *e, const char *face, int h, int weight, int charset) {
    memset(e, 0, sizeof *e);
    e->elfEnumLogfontEx.elfLogFont.lfHeight = h;
    e->elfEnumLogfontEx.elfLogFont.lfWeight = weight;
    e->elfEnumLogfontEx.elfLogFont.lfCharSet = (BYTE)charset;
    lstrcpyA(e->elfEnumLogfontEx.elfLogFont.lfFaceName, face);
}
#define NEED_CFX() do { REQUIRE(CFX); REQUIRE(U(GetObjectA)); REQUIRE(U(GetObjectW)); REQUIRE(U(DeleteObject)); NEED_FD(); } while (0)
TEST(gap10_cfx_null_returns_null) {
    NEED_CFX();
    CLR();
    CHECK(CFX(NULL) == NULL);
    t_note("NULL arg: err %lu", GetLastError());
}
TEST(gap10_cfx_basic_roundtrip_a) {
    NEED_CFX();
    ENUMLOGFONTEXDVA e; LOGFONTA out; HFONT h;
    efill(&e, "Arial", -20, 700, ANSI_CHARSET);
    e.elfEnumLogfontEx.elfLogFont.lfItalic = 1; e.elfEnumLogfontEx.elfLogFont.lfUnderline = 1; e.elfEnumLogfontEx.elfLogFont.lfStrikeOut = 1;
    e.elfEnumLogfontEx.elfLogFont.lfQuality = ANTIALIASED_QUALITY; e.elfEnumLogfontEx.elfLogFont.lfEscapement = 450; e.elfEnumLogfontEx.elfLogFont.lfOrientation = 450;
    h = CFX(&e);
    CHECK(h != NULL);
    if (!h) return;
    memset(&out, 0xCC, sizeof out);
    EXPECT_EQ(U(GetObjectA)(h, sizeof out, &out), sizeof(LOGFONTA));
    EXPECT_EQ(out.lfHeight, -20); EXPECT_EQ(out.lfWeight, 700); EXPECT_EQ(out.lfItalic, 1);
    EXPECT_EQ(out.lfUnderline, 1); EXPECT_EQ(out.lfStrikeOut, 1); EXPECT_EQ(out.lfCharSet, ANSI_CHARSET);
    EXPECT_EQ(out.lfEscapement, 450); EXPECT_EQ(out.lfOrientation, 450); EXPECT_EQ(out.lfQuality, ANTIALIASED_QUALITY);
    EXPECT_STR(out.lfFaceName, "Arial");
    U(DeleteObject)(h);
}
TEST(gap10_cfx_roundtrip_w_view) {
    NEED_CFX();
    ENUMLOGFONTEXDVA e; LOGFONTW out; HFONT h;
    efill(&e, "Courier New", 14, 400, DEFAULT_CHARSET);
    h = CFX(&e);
    CHECK(h != NULL);
    if (!h) return;
    memset(&out, 0xCC, sizeof out);
    EXPECT_EQ(U(GetObjectW)(h, sizeof out, &out), sizeof(LOGFONTW));
    EXPECT_EQ(out.lfHeight, 14);
    EXPECT_WSTR(out.lfFaceName, L"Courier New");
    U(DeleteObject)(h);
}
TEST(gap10_cfx_matches_createfontindirecta) {
    NEED_CFX(); REQUIRE(U(CreateFontIndirectA)); REQUIRE(U(GetTextMetricsW));
    ENUMLOGFONTEXDVA e; HFONT h1, h2; FD f1, f2; TEXTMETRICW t1, t2;
    efill(&e, "Arial", -24, 400, ANSI_CHARSET);
    h1 = CFX(&e); h2 = U(CreateFontIndirectA)(&e.elfEnumLogfontEx.elfLogFont);
    CHECK(h1 && h2);
    if (h1 && h2) {
        HDC d1 = U(CreateCompatibleDC)(NULL);
        HGDIOBJ o1 = U(SelectObject)(d1, h1);
        U(GetTextMetricsW)(d1, &t1);
        U(SelectObject)(d1, h2);
        U(GetTextMetricsW)(d1, &t2);
        CHECK(!memcmp(&t1, &t2, sizeof t1));
        U(SelectObject)(d1, o1); U(DeleteDC)(d1);
    }
    if (h1) U(DeleteObject)(h1);
    if (h2) U(DeleteObject)(h2);
    (void)f1; (void)f2;
}
TEST(gap10_cfx_matches_exw_object) {
    NEED_CFX(); REQUIRE(U(CreateFontIndirectExW));
    ENUMLOGFONTEXDVA e; ENUMLOGFONTEXDVW w; LOGFONTW oa, ob; HFONT ha, hb;
    memset(&w, 0, sizeof w);
    efill(&e, "Tahoma", -17, 400, ANSI_CHARSET);
    w.elfEnumLogfontEx.elfLogFont.lfHeight = -17; w.elfEnumLogfontEx.elfLogFont.lfWeight = 400; w.elfEnumLogfontEx.elfLogFont.lfCharSet = ANSI_CHARSET;
    lstrcpyW(w.elfEnumLogfontEx.elfLogFont.lfFaceName, L"Tahoma");
    ha = CFX(&e); hb = U(CreateFontIndirectExW)(&w);
    CHECK(ha && hb);
    if (ha && hb) {
        memset(&oa, 0, sizeof oa); memset(&ob, 0, sizeof ob);
        U(GetObjectW)(ha, sizeof oa, &oa); U(GetObjectW)(hb, sizeof ob, &ob);
        CHECK(oa.lfHeight == ob.lfHeight && oa.lfWeight == ob.lfWeight && oa.lfCharSet == ob.lfCharSet && oa.lfQuality == ob.lfQuality);
        CHECK(!lstrcmpW(oa.lfFaceName, ob.lfFaceName));
    }
    if (ha) U(DeleteObject)(ha);
    if (hb) U(DeleteObject)(hb);
}
TEST(gap10_cfx_zeroed_struct_returns_font) {
    NEED_CFX();
    ENUMLOGFONTEXDVA e; HFONT h;
    memset(&e, 0, sizeof e);
    h = CFX(&e);
    CHECK(h != NULL);
    if (h) U(DeleteObject)(h);
}
TEST(gap10_cfx_names_other_than_logfont_ignored) {
    NEED_CFX();
    ENUMLOGFONTEXDVA a, b; LOGFONTA oa, ob; HFONT ha, hb;
    efill(&a, "Arial", -16, 400, ANSI_CHARSET); b = a;
    lstrcpyA((char *)b.elfEnumLogfontEx.elfFullName, "Garbage Full Name");
    lstrcpyA((char *)b.elfEnumLogfontEx.elfStyle, "Garbage Style");
    lstrcpyA((char *)b.elfEnumLogfontEx.elfScript, "Garbage Script");
    ha = CFX(&a); hb = CFX(&b);
    CHECK(ha && hb);
    if (ha && hb) { memset(&oa, 0, sizeof oa); memset(&ob, 0, sizeof ob); U(GetObjectA)(ha, sizeof oa, &oa); U(GetObjectA)(hb, sizeof ob, &ob); CHECK(oa.lfHeight == ob.lfHeight && oa.lfWeight == ob.lfWeight && oa.lfCharSet == ob.lfCharSet); CHECK(!strcmp(oa.lfFaceName, ob.lfFaceName)); }
    if (ha) U(DeleteObject)(ha);
    if (hb) U(DeleteObject)(hb);
}
TEST(gap10_cfx_design_vector_zero_axes) {
    NEED_CFX();
    ENUMLOGFONTEXDVA e; HFONT h;
    efill(&e, "Arial", -16, 400, ANSI_CHARSET);
    e.elfDesignVector.dvReserved = STAMP_DESIGNVECTOR; e.elfDesignVector.dvNumAxes = 0;
    h = CFX(&e);
    CHECK(h != NULL);
    if (h) U(DeleteObject)(h);
}
TEST(gap10_cfx_design_vector_bad_stamp_ignored) {
    NEED_CFX();
    ENUMLOGFONTEXDVA e; HFONT h;
    efill(&e, "Arial", -16, 400, ANSI_CHARSET);
    e.elfDesignVector.dvReserved = 0x12345678; e.elfDesignVector.dvNumAxes = 5;
    e.elfDesignVector.dvValues[0] = 100; e.elfDesignVector.dvValues[1] = 200;
    h = CFX(&e);
    t_note("bad design vector stamp -> %s", h ? "font" : "NULL");
    if (h) U(DeleteObject)(h);
}
TEST(gap10_cfx_design_vector_too_many_axes_observed) {
    NEED_CFX();
    ENUMLOGFONTEXDVA e; HFONT h;
    efill(&e, "Arial", -16, 400, ANSI_CHARSET);
    e.elfDesignVector.dvReserved = STAMP_DESIGNVECTOR; e.elfDesignVector.dvNumAxes = 17;
    CLR();
    h = CFX(&e);
    t_note("dvNumAxes=17 -> %s err %lu", h ? "font" : "NULL", GetLastError());
    if (h) U(DeleteObject)(h);
    e.elfDesignVector.dvNumAxes = 0xFFFFFFFF;
    h = CFX(&e);
    t_note("dvNumAxes=0xFFFFFFFF -> %s", h ? "font" : "NULL");
    if (h) U(DeleteObject)(h);
}
TEST(gap10_cfx_select_and_face_name) {
    NEED_CFX(); REQUIRE(U(GetTextFaceA));
    ENUMLOGFONTEXDVA e; HFONT h; HDC dc = U(CreateCompatibleDC)(NULL); HGDIOBJ old; char buf[64];
    efill(&e, "Times New Roman", -20, 400, ANSI_CHARSET);
    h = CFX(&e);
    CHECK(h != NULL);
    if (h) {
        old = U(SelectObject)(dc, h);
        CHECK(old != NULL);
        buf[0] = 0; U(GetTextFaceA)(dc, sizeof buf, buf);
        EXPECT_STR(buf, "Times New Roman");
        U(SelectObject)(dc, old);
        U(DeleteObject)(h);
    }
    U(DeleteDC)(dc);
}
TEST(gap10_cfx_nonexistent_face_still_creates) {
    NEED_CFX(); REQUIRE(U(GetTextFaceA));
    ENUMLOGFONTEXDVA e; HFONT h; HDC dc = U(CreateCompatibleDC)(NULL); char buf[64]; LOGFONTA out;
    efill(&e, "NoSuchFace_g10_xyz", -20, 400, ANSI_CHARSET);
    h = CFX(&e);
    CHECK(h != NULL);
    if (h) {
        HGDIOBJ old = U(SelectObject)(dc, h);
        buf[0] = 0; U(GetTextFaceA)(dc, sizeof buf, buf);
        t_note("substituted face: '%s'", buf);
        CHECK(buf[0] != 0);
        U(GetObjectA)(h, sizeof out, &out);
        EXPECT_STR(out.lfFaceName, "NoSuchFace_g10_xyz");    /* the LOGFONT keeps the requested name */
        U(SelectObject)(dc, old); U(DeleteObject)(h);
    }
    U(DeleteDC)(dc);
}
TEST(gap10_cfx_face_name_length_boundary) {
    NEED_CFX();
    ENUMLOGFONTEXDVA e; HFONT h; LOGFONTA out; char name[LF_FACESIZE]; int i;
    for (i = 0; i < LF_FACESIZE - 1; i++) name[i] = 'A' + (i % 26);
    name[LF_FACESIZE - 1] = 0;
    efill(&e, "x", -12, 400, ANSI_CHARSET);
    memcpy(e.elfEnumLogfontEx.elfLogFont.lfFaceName, name, LF_FACESIZE);
    h = CFX(&e);
    CHECK(h != NULL);
    if (h) { memset(&out, 0, sizeof out); U(GetObjectA)(h, sizeof out, &out); EXPECT_STR(out.lfFaceName, name); U(DeleteObject)(h); }
}
TEST(gap10_cfx_height_extremes) {
    NEED_CFX();
    int hs[] = { 0, 1, -1, 1000, -1000, INT_MAX, INT_MIN, -32768 }; unsigned i;
    for (i = 0; i < sizeof hs / sizeof *hs; i++) {
        ENUMLOGFONTEXDVA e; HFONT h; LOGFONTA out;
        efill(&e, "Arial", hs[i], 400, ANSI_CHARSET);
        h = CFX(&e);
        CHECKF(h != NULL, "height %d", hs[i]);
        if (h) { memset(&out, 0, sizeof out); U(GetObjectA)(h, sizeof out, &out); t_note("height %d -> %ld", hs[i], out.lfHeight); U(DeleteObject)(h); }
    }
}
TEST(gap10_cfx_weight_values) {
    NEED_CFX();
    int ws[] = { 0, 100, 400, 700, 900, 1000, 5000, -1 }; unsigned i;
    for (i = 0; i < sizeof ws / sizeof *ws; i++) {
        ENUMLOGFONTEXDVA e; HFONT h; LOGFONTA out;
        efill(&e, "Arial", -16, ws[i], ANSI_CHARSET);
        h = CFX(&e);
        CHECKF(h != NULL, "weight %d", ws[i]);
        if (h) { memset(&out, 0, sizeof out); U(GetObjectA)(h, sizeof out, &out); CHECKF(out.lfWeight == ws[i], "weight %d -> %ld", ws[i], out.lfWeight); U(DeleteObject)(h); }
    }
}
TEST(gap10_cfx_charsets) {
    NEED_CFX();
    int cs[] = { ANSI_CHARSET, DEFAULT_CHARSET, SYMBOL_CHARSET, GREEK_CHARSET, RUSSIAN_CHARSET, OEM_CHARSET, 0x7F, 0xFF }; unsigned i;
    for (i = 0; i < sizeof cs / sizeof *cs; i++) {
        ENUMLOGFONTEXDVA e; HFONT h; LOGFONTA out;
        efill(&e, "Arial", -16, 400, cs[i]);
        h = CFX(&e);
        CHECKF(h != NULL, "charset %d", cs[i]);
        if (h) { memset(&out, 0, sizeof out); U(GetObjectA)(h, sizeof out, &out); CHECKF(out.lfCharSet == cs[i], "charset %d -> %d", cs[i], out.lfCharSet); U(DeleteObject)(h); }
    }
}
TEST(gap10_cfx_distinct_handles_and_delete) {
    NEED_CFX();
    ENUMLOGFONTEXDVA e; HFONT a, b;
    efill(&e, "Arial", -16, 400, ANSI_CHARSET);
    a = CFX(&e); b = CFX(&e);
    CHECK(a && b);
    CHECK(a != b);
    if (a) { CHECK(U(DeleteObject)(a)); CLR(); CHECK(!U(DeleteObject)(a)); }
    if (b) CHECK(U(DeleteObject)(b));
}
TEST(gap10_cfx_object_type) {
    NEED_CFX(); REQUIRE(U(GetObjectType));
    ENUMLOGFONTEXDVA e; HFONT h;
    efill(&e, "Arial", -16, 400, ANSI_CHARSET);
    h = CFX(&e);
    CHECK(h != NULL);
    if (h) { EXPECT_EQ(U(GetObjectType)(h), OBJ_FONT); U(DeleteObject)(h); }
}
TEST(gap10_cfx_getobject_a_buffer_sizes) {
    NEED_CFX();
    ENUMLOGFONTEXDVA e; HFONT h; BYTE big[256]; LOGFONTA out;
    efill(&e, "Arial", -16, 400, ANSI_CHARSET);
    h = CFX(&e);
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(U(GetObjectA)(h, 0, NULL), sizeof(LOGFONTA));
    EXPECT_EQ(U(GetObjectA)(h, sizeof big, big), sizeof(LOGFONTA));
    t_note("GetObjectA(h,10) -> %d", U(GetObjectA)(h, 10, &out));
    U(DeleteObject)(h);
}
TEST(gap10_cfx_no_gdi_leak) {
    NEED_CFX();
    if (!g10_gdicount()) { t_skip("no GetGuiResources"); return; }
    ENUMLOGFONTEXDVA e; HFONT h[200]; int i; DWORD before = g10_gdicount();
    for (i = 0; i < 200; i++) { efill(&e, "Arial", -10 - (i % 40), 400 + (i % 3) * 100, ANSI_CHARSET); h[i] = CFX(&e); }
    for (i = 0; i < 200; i++) if (h[i]) U(DeleteObject)(h[i]);
    EXPECT_EQ(g10_gdicount(), before);
}
static void cfx_thr(void *p) { ENUMLOGFONTEXDVA e; efill(&e, "Arial", -33, 400, ANSI_CHARSET); *(HFONT *)p = CFX(&e); }
TEST(gap10_cfx_cross_thread_use) {
    NEED_CFX();
    HFONT h = NULL; LOGFONTA out;
    g10_run_thread(cfx_thr, &h);
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(U(GetObjectA)(h, sizeof out, &out), sizeof out);
    EXPECT_EQ(out.lfHeight, -33);
    CHECK(U(DeleteObject)(h));
}
TEST(gap10_cfx_text_metrics_from_selected) {
    NEED_CFX(); REQUIRE(U(GetTextMetricsA));
    ENUMLOGFONTEXDVA e; HFONT h; HDC dc = U(CreateCompatibleDC)(NULL); TEXTMETRICA tm; HGDIOBJ old;
    efill(&e, "Arial", -40, 700, ANSI_CHARSET);
    h = CFX(&e);
    CHECK(h != NULL);
    if (h) {
        old = U(SelectObject)(dc, h);
        CHECK(U(GetTextMetricsA)(dc, &tm));
        EXPECT_EQ(tm.tmWeight, 700);
        CHECK(tm.tmHeight - tm.tmInternalLeading == 40);
        U(SelectObject)(dc, old); U(DeleteObject)(h);
    }
    U(DeleteDC)(dc);
}
TEST(gap10_cfx_gdi32_vs_gdi32full) {
    typedef HFONT (WINAPI *P)(const ENUMLOGFONTEXDVA *);
    P a = (P)t_proc_gdi32("CreateFontIndirectExA"), b = (P)t_proc_gdi32full("CreateFontIndirectExA"); REQUIRE(a); NEED_FD();
    ENUMLOGFONTEXDVA e; HFONT h1, h2;
    efill(&e, "Arial", -16, 400, ANSI_CHARSET);
    h1 = a(&e); CHECK(h1 != NULL);
    if (b) { h2 = b(&e); CHECK(h2 != NULL); if (h2) U(DeleteObject)(h2); }
    if (h1) U(DeleteObject)(h1);
    CHECK(a(NULL) == NULL);
}
