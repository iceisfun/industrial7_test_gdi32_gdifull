#include "to.h"
/* Parameter validation: TextOut / ExtTextOut / PolyTextOut. */

static void err_is(DWORD want) { DWORD e = GetLastError(); CHECKF(e == want, "GetLastError got %lu want %lu", e, want); }

TEST(text_output_textouta_zero_count_true) {
    NEED_TEXT();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    SetLastError(0);
    CHECK(U(TextOutA)(c.dc, 0, 0, "abc", 0) == TRUE);
    CHECK(U(TextOutA)(c.dc, 0, 0, NULL, 0) == TRUE);
    CHECK(U(TextOutA)(c.dc, 0, 0, "", 0) == TRUE);
    /* count 0 returns before the DC is looked at */
    CHECK(U(TextOutA)(NULL, 0, 0, "abc", 0) == TRUE);
    CHECK(U(TextOutA)((HDC)(ULONG_PTR)0x1234, 0, 0, "abc", 0) == TRUE);
    to_close(&c);
}
TEST(text_output_textoutw_zero_count_true) {
    NEED_TEXT();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    SetLastError(0);
    CHECK(U(TextOutW)(c.dc, 0, 0, L"abc", 0) == TRUE);
    CHECK(U(TextOutW)(c.dc, 0, 0, NULL, 0) == TRUE);
    CHECK(U(TextOutW)(c.dc, 0, 0, L"", 0) == TRUE);
    CHECK(U(TextOutW)(NULL, 0, 0, L"abc", 0) == TRUE);
    to_close(&c);
}
TEST(text_output_textouta_negative_count) {
    NEED_TEXT();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    static const int cnt[] = { -1, -2, -100, -0x7fffffff, (int)0x80000000 };
    for (int i = 0; i < ARRAYSZ(cnt); i++) {
        SetLastError(0);
        BOOL r = U(TextOutA)(c.dc, 0, 0, "abc", cnt[i]);
        CHECKF(r == FALSE, "count %d ret %d", cnt[i], r);
        err_is(ERROR_INVALID_PARAMETER);
    }
    to_close(&c);
}
TEST(text_output_textoutw_negative_count) {
    NEED_TEXT();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    static const int cnt[] = { -1, -2, -100, -0x7fffffff, (int)0x80000000 };
    for (int i = 0; i < ARRAYSZ(cnt); i++) {
        SetLastError(0);
        BOOL r = U(TextOutW)(c.dc, 0, 0, L"abc", cnt[i]);
        CHECKF(r == FALSE, "count %d ret %d", cnt[i], r);
        err_is(ERROR_INVALID_PARAMETER);
    }
    to_close(&c);
}
TEST(text_output_textouta_null_string_positive_count) {
    NEED_TEXT();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    SetLastError(0);
    CHECK(U(TextOutA)(c.dc, 0, 0, NULL, 1) == FALSE);
    err_is(ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(U(TextOutA)(c.dc, 0, 0, NULL, 100) == FALSE);
    err_is(ERROR_INVALID_PARAMETER);
    to_close(&c);
}
TEST(text_output_textoutw_null_string_positive_count) {
    NEED_TEXT();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    if (to_wine()) { t_skip("wine faults on NULL string"); to_close(&c); return; }
    SetLastError(0);
    CHECK(U(TextOutW)(c.dc, 0, 0, NULL, 1) == FALSE);
    err_is(ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(U(TextOutW)(c.dc, 0, 0, NULL, 100) == FALSE);
    err_is(ERROR_INVALID_PARAMETER);
    to_close(&c);
}
TEST(text_output_textouta_null_dc) {
    NEED_TEXT();
    SetLastError(0);
    CHECK(U(TextOutA)(NULL, 0, 0, "abc", 3) == FALSE);
    t_note("TextOutA(NULL dc) err=%lu", GetLastError());
}
TEST(text_output_textoutw_null_dc) {
    NEED_TEXT();
    SetLastError(0);
    CHECK(U(TextOutW)(NULL, 0, 0, L"abc", 3) == FALSE);
    t_note("TextOutW(NULL dc) err=%lu", GetLastError());
    /* non-DC-typed handle goes through the client DC lookup: ERROR_INVALID_HANDLE */
    SetLastError(0);
    HGDIOBJ f = U(GetStockObject)(SYSTEM_FONT);
    CHECK(U(TextOutW)((HDC)f, 0, 0, L"abc", 3) == FALSE);
    err_is(ERROR_INVALID_HANDLE);
}
TEST(text_output_textout_bogus_handles) {
    NEED_TEXT();
    static const ULONG_PTR h[] = { 1, 0x1234, 0xffffffff, 0x00010001, 0x7fffffff };
    for (int i = 0; i < ARRAYSZ(h); i++) {
        SetLastError(0);
        CHECKF(U(TextOutA)((HDC)h[i], 0, 0, "abc", 3) == FALSE, "A %zx", (size_t)h[i]);
        SetLastError(0);
        CHECKF(U(TextOutW)((HDC)h[i], 0, 0, L"abc", 3) == FALSE, "W %zx", (size_t)h[i]);
    }
}
TEST(text_output_textout_deleted_dc) {
    NEED_TEXT();
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(dc != NULL);
    CHECK(U(DeleteDC)(dc));
    SetLastError(0);
    CHECK(U(TextOutA)(dc, 0, 0, "abc", 3) == FALSE);
    CHECK(U(TextOutW)(dc, 0, 0, L"abc", 3) == FALSE);
    CHECK(U(ExtTextOutA)(dc, 0, 0, 0, NULL, "abc", 3, NULL) == FALSE);
    CHECK(U(ExtTextOutW)(dc, 0, 0, 0, NULL, L"abc", 3, NULL) == FALSE);
    SIZE sz = {7, 7};
    CHECK(U(GetTextExtentPoint32A)(dc, "abc", 3, &sz) == FALSE);
    CHECK(U(GetTextExtentPoint32W)(dc, L"abc", 3, &sz) == FALSE);
}
TEST(text_output_textout_returns_true_ok) {
    NEED_TEXT();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FIXED_FONT), 0);
    CHECK(U(TextOutA)(c.dc, 0, 0, "abc", 3) == TRUE);
    CHECK(U(TextOutW)(c.dc, 0, 0, L"abc", 3) == TRUE);
    CHECK(U(TextOutA)(c.dc, 0, 0, "a", 1) == TRUE);
    CHECK(U(TextOutW)(c.dc, 0, 0, L" ", 1) == TRUE);
    to_close(&c);
}
TEST(text_output_textout_count_beyond_string_a) {
    /* count larger than the terminator is not looked at: embedded chars after NUL are drawn/measured */
    NEED_TEXT();
    TCtx c; if (!to_open(&c, 128, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FIXED_FONT), 0);
    TEXTMETRICA tm; if (!to_fixed(c.dc, &tm)) { t_skip("no fixed font"); to_close(&c); return; }
    char buf[8] = { 'a', 'b', 0, 'c', 'd', 0, 0, 0 };
    SIZE s2, s5;
    CHECK(U(GetTextExtentPoint32A)(c.dc, buf, 2, &s2));
    CHECK(U(GetTextExtentPoint32A)(c.dc, buf, 5, &s5));
    CHECKF(s2.cx == 2 * tm.tmAveCharWidth, "s2 %ld", s2.cx);
    CHECKF(s5.cx == 5 * tm.tmAveCharWidth, "s5 %ld (NUL measured as one cell)", s5.cx);
    to_close(&c);
}
TEST(text_output_exttextout_options_rect_ignored_without_flags) {
    /* lprect is ignored unless ETO_OPAQUE/ETO_CLIPPED: a garbage-but-readable rect is harmless */
    NEED_TEXT();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FIXED_FONT), 0);
    RECT r = { 30, 30, 5, 5 };
    CHECK(U(ExtTextOutA)(c.dc, 2, 2, 0, &r, "ab", 2, NULL));
    CHECK(U(ExtTextOutW)(c.dc, 2, 2, 0, &r, L"ab", 2, NULL));
    CHECK(U(ExtTextOutA)(c.dc, 2, 2, ETO_GLYPH_INDEX & 0, NULL, "ab", 2, NULL));
    to_close(&c);
}
TEST(text_output_exttextout_zero_count_no_flags) {
    NEED_TEXT();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    SetLastError(0);
    BOOL a = U(ExtTextOutA)(c.dc, 0, 0, 0, NULL, "abc", 0, NULL);
    BOOL w = U(ExtTextOutW)(c.dc, 0, 0, 0, NULL, L"abc", 0, NULL);
    t_note("ExtTextOut count 0, no flags: A=%d W=%d", a, w);
    BOOL an = U(ExtTextOutA)(c.dc, 0, 0, 0, NULL, NULL, 0, NULL);
    BOOL wn = U(ExtTextOutW)(c.dc, 0, 0, 0, NULL, NULL, 0, NULL);
    t_note("ExtTextOut NULL string count 0: A=%d W=%d", an, wn);
    to_close(&c);
}
TEST(text_output_exttextout_opaque_empty_string_fills_rect) {
    NEED_TEXT();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FIXED_FONT), 0);
    U(SetBkColor)(c.dc, RGB(255, 0, 0));
    RECT r = { 10, 5, 30, 20 };
    BOOL b = U(ExtTextOutW)(c.dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL);
    CHECK(b);
    RECT ink; int n = to_ink(&c, to_white(&c), &ink);
    CHECKF(n == 20 * 15, "filled %d", n);
    CHECKF(ink.left == 10 && ink.top == 5 && ink.right == 29 && ink.bottom == 19, "bbox %ld,%ld,%ld,%ld", ink.left, ink.top, ink.right, ink.bottom);
    CHECK(to_px(&c, 10, 5) == 0xff0000);
    CHECK(to_px(&c, 29, 19) == 0xff0000);
    to_clear(&c);
    b = U(ExtTextOutA)(c.dc, 0, 0, ETO_OPAQUE, &r, "", 0, NULL);
    CHECK(b);
    CHECK(to_ink(&c, to_white(&c), &ink) == 20 * 15);
    to_close(&c);
}
TEST(text_output_exttextout_opaque_null_rect) {
    NEED_TEXT();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FIXED_FONT), 0);
    U(SetBkColor)(c.dc, RGB(255, 0, 0));
    /* ETO_OPAQUE/CLIPPED with a NULL rect: the flags are dropped, text drawn with the plain bk mode */
    SetLastError(0);
    BOOL b = U(ExtTextOutA)(c.dc, 2, 2, ETO_OPAQUE, NULL, "ab", 2, NULL);
    CHECK(b);
    RECT ink; to_ink(&c, to_white(&c), &ink);
    int reds = to_count(&c, 0xff0000, 0, 0, 64, 32);
    CHECKF(reds == 0, "bk mode TRANSPARENT: no opaque fill expected, got %d red", reds);
    b = U(ExtTextOutW)(c.dc, 2, 2, ETO_CLIPPED, NULL, L"ab", 2, NULL);
    CHECK(b);
    b = U(ExtTextOutW)(c.dc, 2, 2, ETO_CLIPPED | ETO_OPAQUE, NULL, L"ab", 2, NULL);
    CHECK(b);
    to_close(&c);
}
TEST(text_output_exttextout_pdy_requires_dx_a) {
    NEED_TEXT();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FIXED_FONT), 0);
    /* The A entry point rejects ETO_PDY without a dx array */
    SetLastError(0);
    BOOL b = U(ExtTextOutA)(c.dc, 2, 2, ETO_PDY, NULL, "ab", 2, NULL);
    CHECK(b == FALSE);
    int dx[4] = { 8, 0, 8, 0 };
    b = U(ExtTextOutA)(c.dc, 2, 2, ETO_PDY, NULL, "ab", 2, dx);
    CHECK(b == TRUE);
    to_close(&c);
}
TEST(text_output_exttextout_pdy_null_dx_w) {
    NEED_TEXT();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FIXED_FONT), 0);
    SetLastError(0);
    BOOL b = U(ExtTextOutW)(c.dc, 2, 2, ETO_PDY, NULL, L"ab", 2, NULL);
    t_note("ExtTextOutW PDY with NULL dx -> %d err=%lu", b, GetLastError());
    to_close(&c);
}
TEST(text_output_exttextout_bogus_dc_all_flags) {
    NEED_TEXT();
    RECT r = { 0, 0, 10, 10 };
    static const UINT fl[] = { 0, ETO_OPAQUE, ETO_CLIPPED, ETO_OPAQUE | ETO_CLIPPED, ETO_GLYPH_INDEX, ETO_RTLREADING };
    for (int i = 0; i < ARRAYSZ(fl); i++) {
        SetLastError(0);
        CHECKF(U(ExtTextOutW)((HDC)(ULONG_PTR)0x1234, 0, 0, fl[i], &r, L"abc", 3, NULL) == FALSE, "flags %x", fl[i]);
        SetLastError(0);
        CHECKF(U(ExtTextOutA)((HDC)(ULONG_PTR)0x1234, 0, 0, fl[i], &r, "abc", 3, NULL) == FALSE, "A flags %x", fl[i]);
    }
    SetLastError(0);
    CHECK(U(ExtTextOutW)(NULL, 0, 0, 0, NULL, L"abc", 3, NULL) == FALSE);
    SetLastError(0);
    CHECK(U(ExtTextOutA)(NULL, 0, 0, 0, NULL, "abc", 3, NULL) == FALSE);
}
TEST(text_output_exttextout_huge_count_with_null_string) {
    NEED_TEXT();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    if (to_wine()) { t_skip("wine faults on NULL string"); to_close(&c); return; }
    SetLastError(0);
    BOOL b = U(ExtTextOutW)(c.dc, 0, 0, 0, NULL, NULL, 5, NULL);
    CHECK(b == FALSE);
    t_note("ExtTextOutW NULL string count 5 err=%lu", GetLastError());
    SetLastError(0);
    b = U(ExtTextOutW)(c.dc, 0, 0, 0, NULL, L"abc", 0xffffffffu, NULL);
    CHECK(b == FALSE);
    t_note("ExtTextOutW count 0xffffffff err=%lu", GetLastError());
    b = U(ExtTextOutW)(c.dc, 0, 0, 0, NULL, L"abc", 0x80000000u, NULL);
    CHECK(b == FALSE);
    to_close(&c);
}
TEST(text_output_exttextout_count_limit_8192) {
    /* GDI refuses very long runs; 8192 is the documented ExtTextOut maximum for W */
    NEED_TEXT();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FIXED_FONT), 0);
    static WCHAR big[8200];
    for (int i = 0; i < 8200; i++) big[i] = L'a';
    SetLastError(0);
    BOOL b1 = U(ExtTextOutW)(c.dc, 0, 0, 0, NULL, big, 8192, NULL);
    t_note("ExtTextOutW count 8192 -> %d", b1);
    SetLastError(0);
    BOOL b2 = U(ExtTextOutW)(c.dc, 0, 0, 0, NULL, big, 8200, NULL);
    t_note("ExtTextOutW count 8200 -> %d err=%lu", b2, GetLastError());
    to_close(&c);
}
TEST(text_output_exttextout_opaque_clipped_rect_unordered) {
    NEED_TEXT();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FIXED_FONT), 0);
    U(SetBkColor)(c.dc, RGB(0, 255, 0));
    RECT r = { 30, 20, 10, 5 }; /* inverted */
    SetLastError(0);
    BOOL b = U(ExtTextOutW)(c.dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL);
    RECT ink; int n = to_ink(&c, to_white(&c), &ink);
    t_note("inverted opaque rect: ret=%d painted=%d", b, n);
    to_close(&c);
}
TEST(text_output_exttextout_opaque_empty_rect) {
    NEED_TEXT();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FIXED_FONT), 0);
    U(SetBkColor)(c.dc, RGB(0, 255, 0));
    RECT r = { 10, 10, 10, 10 };
    BOOL b = U(ExtTextOutW)(c.dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL);
    RECT ink;
    CHECK(to_ink(&c, to_white(&c), &ink) == 0);
    t_note("empty opaque rect ret=%d", b);
    to_close(&c);
}
TEST(text_output_exttextout_extra_flag_bits) {
    NEED_TEXT();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FIXED_FONT), 0);
    static const UINT fl[] = { 0x1, 0x8, 0x40, 0x80, 0x4000, 0x8000, 0x10000, 0x80000000, 0xffffffff };
    for (int i = 0; i < ARRAYSZ(fl); i++) {
        RECT r = { 0, 0, 20, 10 };
        SetLastError(0);
        BOOL b = U(ExtTextOutW)(c.dc, 2, 2, fl[i], &r, L"ab", 2, NULL);
        t_note("ExtTextOutW options 0x%x -> %d err=%lu", fl[i], b, GetLastError());
    }
    to_close(&c);
}
/* ---- PolyTextOut ---- */
TEST(text_output_polytextout_zero_strings) {
    NEED_TEXT(); REQUIRE(U(PolyTextOutA)); REQUIRE(U(PolyTextOutW));
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    SetLastError(0);
    CHECK(U(PolyTextOutA)(c.dc, NULL, 0) == TRUE);
    CHECK(U(PolyTextOutW)(c.dc, NULL, 0) == TRUE);
    POLYTEXTW pt[1]; memset(pt, 0, sizeof pt);
    CHECK(U(PolyTextOutW)(c.dc, pt, 0) == TRUE);
    to_close(&c);
}
TEST(text_output_polytextout_negative_count) {
    NEED_TEXT(); REQUIRE(U(PolyTextOutA)); REQUIRE(U(PolyTextOutW));
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    POLYTEXTW pt; memset(&pt, 0, sizeof pt);
    static const int n[] = { -1, -5, (int)0x80000000 };
    for (int i = 0; i < ARRAYSZ(n); i++) {
        SetLastError(0);
        CHECK(U(PolyTextOutW)(c.dc, &pt, n[i]) == FALSE);
        err_is(ERROR_INVALID_PARAMETER);
        SetLastError(0);
        CHECK(U(PolyTextOutA)(c.dc, (POLYTEXTA *)&pt, n[i]) == FALSE);
        err_is(ERROR_INVALID_PARAMETER);
    }
    to_close(&c);
}
TEST(text_output_polytextout_null_string_with_count_w) {
    NEED_TEXT(); REQUIRE(U(PolyTextOutW));
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    POLYTEXTW pt[2]; memset(pt, 0, sizeof pt);
    pt[0].lpstr = L"ab"; pt[0].n = 2;
    pt[1].lpstr = NULL; pt[1].n = 3;
    if (to_wine()) { t_skip("wine faults on NULL string"); to_close(&c); return; }
    SetLastError(0);
    CHECK(U(PolyTextOutW)(c.dc, pt, 2) == FALSE);
    err_is(ERROR_INVALID_PARAMETER);
    to_close(&c);
}
TEST(text_output_polytextout_null_string_zero_len_ok) {
    NEED_TEXT(); REQUIRE(U(PolyTextOutW)); REQUIRE(U(PolyTextOutA));
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    POLYTEXTW pt[1]; memset(pt, 0, sizeof pt);
    SetLastError(0);
    BOOL b = U(PolyTextOutW)(c.dc, pt, 1);
    t_note("PolyTextOutW single NULL/0 entry -> %d err=%lu", b, GetLastError());
    to_close(&c);
}
TEST(text_output_polytextout_bogus_dc) {
    NEED_TEXT(); REQUIRE(U(PolyTextOutW)); REQUIRE(U(PolyTextOutA));
    POLYTEXTW pt; memset(&pt, 0, sizeof pt); pt.lpstr = L"a"; pt.n = 1;
    SetLastError(0);
    CHECK(U(PolyTextOutW)(NULL, &pt, 1) == FALSE);
    err_is(ERROR_INVALID_HANDLE);
    SetLastError(0);
    CHECK(U(PolyTextOutA)(NULL, (POLYTEXTA *)&pt, 1) == FALSE);
    err_is(ERROR_INVALID_HANDLE);
    /* zero strings returns TRUE even for a bogus DC */
    CHECK(U(PolyTextOutW)(NULL, &pt, 0) == TRUE);
}
TEST(text_output_polytextout_draws_both_strings) {
    NEED_TEXT(); REQUIRE(U(PolyTextOutW)); REQUIRE(U(PolyTextOutA));
    TCtx c; if (!to_open(&c, 128, 64, 32)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FIXED_FONT), 0);
    TEXTMETRICA tm; if (!to_fixed(c.dc, &tm)) { t_skip("no fixed font"); to_close(&c); return; }
    U(SetTextColor)(c.dc, RGB(0, 0, 0));
    POLYTEXTW pt[2]; memset(pt, 0, sizeof pt);
    pt[0].x = 4; pt[0].y = 2; pt[0].lpstr = L"HH"; pt[0].n = 2;
    pt[1].x = 4; pt[1].y = 30; pt[1].lpstr = L"II"; pt[1].n = 2;
    CHECK(U(PolyTextOutW)(c.dc, pt, 2));
    RECT ink; int n = to_ink(&c, to_white(&c), &ink);
    CHECK(n > 0);
    CHECKF(to_count(&c, 0, 4, 2, 4 + 2 * tm.tmAveCharWidth, 2 + tm.tmHeight) > 0, "first string has ink");
    CHECKF(to_count(&c, 0, 4, 30, 4 + 2 * tm.tmAveCharWidth, 30 + tm.tmHeight) > 0, "second string has ink");
    CHECKF(ink.left >= 4 && ink.right < 4 + 2 * tm.tmAveCharWidth, "x range %ld..%ld", ink.left, ink.right);
    to_close(&c);
}
TEST(text_output_polytextout_a_matches_w) {
    NEED_TEXT(); REQUIRE(U(PolyTextOutW)); REQUIRE(U(PolyTextOutA));
    TCtx c; if (!to_open(&c, 128, 64, 32)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FIXED_FONT), 0);
    POLYTEXTA pa[2]; POLYTEXTW pw[2]; memset(pa, 0, sizeof pa); memset(pw, 0, sizeof pw);
    pa[0].x = 3; pa[0].y = 3; pa[0].lpstr = "Test"; pa[0].n = 4;
    pa[1].x = 3; pa[1].y = 30; pa[1].lpstr = "Poly"; pa[1].n = 4;
    pw[0].x = 3; pw[0].y = 3; pw[0].lpstr = L"Test"; pw[0].n = 4;
    pw[1].x = 3; pw[1].y = 30; pw[1].lpstr = L"Poly"; pw[1].n = 4;
    CHECK(U(PolyTextOutA)(c.dc, pa, 2));
    static unsigned a[128 * 64];
    for (int y = 0; y < 64; y++) for (int x = 0; x < 128; x++) a[y * 128 + x] = to_px(&c, x, y);
    to_clear(&c);
    CHECK(U(PolyTextOutW)(c.dc, pw, 2));
    int diff = 0;
    for (int y = 0; y < 64; y++) for (int x = 0; x < 128; x++) if (a[y * 128 + x] != to_px(&c, x, y)) diff++;
    CHECKF(diff == 0, "%d differing pixels", diff);
    to_close(&c);
}
TEST(text_output_polytextout_opaque_rect_per_string) {
    NEED_TEXT(); REQUIRE(U(PolyTextOutW));
    TCtx c; if (!to_open(&c, 128, 64, 32)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FIXED_FONT), 0);
    U(SetBkColor)(c.dc, RGB(0, 0, 255));
    POLYTEXTW pt[2]; memset(pt, 0, sizeof pt);
    pt[0].x = 4; pt[0].y = 4; pt[0].lpstr = L" "; pt[0].n = 1; pt[0].uiFlags = ETO_OPAQUE;
    pt[0].rcl.left = 10; pt[0].rcl.top = 10; pt[0].rcl.right = 20; pt[0].rcl.bottom = 14;
    pt[1].x = 4; pt[1].y = 4; pt[1].lpstr = L" "; pt[1].n = 1; pt[1].uiFlags = ETO_OPAQUE;
    pt[1].rcl.left = 50; pt[1].rcl.top = 30; pt[1].rcl.right = 60; pt[1].rcl.bottom = 40;
    CHECK(U(PolyTextOutW)(c.dc, pt, 2));
    CHECKF(to_count(&c, 0x0000ff, 10, 10, 20, 14) == 40, "rect 1 filled");
    CHECKF(to_count(&c, 0x0000ff, 50, 30, 60, 40) == 100, "rect 2 filled");
    to_close(&c);
}
TEST(text_output_polytextout_many_strings) {
    NEED_TEXT(); REQUIRE(U(PolyTextOutW));
    TCtx c; if (!to_open(&c, 128, 64, 32)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FIXED_FONT), 0);
    POLYTEXTW pt[40]; memset(pt, 0, sizeof pt);
    for (int i = 0; i < 40; i++) { pt[i].x = (i % 8) * 10; pt[i].y = (i / 8) * 12; pt[i].lpstr = L"M"; pt[i].n = 1; }
    CHECK(U(PolyTextOutW)(c.dc, pt, 40));
    RECT ink; CHECK(to_ink(&c, to_white(&c), &ink) > 40);
    to_close(&c);
}
TEST(text_output_polytextout_a_negative_and_null) {
    NEED_TEXT(); REQUIRE(U(PolyTextOutA));
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    POLYTEXTA pa; memset(&pa, 0, sizeof pa);
    pa.lpstr = NULL; pa.n = 2;
    SetLastError(0);
    CHECK(U(PolyTextOutA)(c.dc, &pa, 1) == FALSE);
    err_is(ERROR_INVALID_PARAMETER);
    to_close(&c);
}
