#include "to.h"
/* GetTextExtent* family. */

#define NEED_EXT() do { NEED_TEXT(); REQUIRE(U(GetTextExtentPointA)); REQUIRE(U(GetTextExtentPointW)); \
    REQUIRE(U(GetTextExtentExPointA)); REQUIRE(U(GetTextExtentExPointW)); REQUIRE(U(SetTextAlign)); } while (0)

/* A fixed-pitch bitmap font context: 8bpp-independent, 32bpp DIB. */
static int fixed_ctx(TCtx *c, TEXTMETRICA *tm, int stock) { return to_fixed_ctx(c, tm, stock, 256, 64); }
static int tt_ctx(TCtx *c, const char *face, int h) {
    if (!to_open(c, 64, 64, 32)) { CHECK(0); return 0; }
    to_use_font(c, to_mkfont(face, h, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    return 1;
}

TEST(text_output_ext32_empty_string_zero) {
    NEED_EXT();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    SIZE s = { 77, 88 };
    SetLastError(0);
    CHECK(U(GetTextExtentPoint32A)(c.dc, "", 0, &s) == TRUE);
    CHECK(s.cx == 0 && s.cy == 0);
    s.cx = 5; s.cy = 6;
    CHECK(U(GetTextExtentPoint32W)(c.dc, L"", 0, &s) == TRUE);
    CHECK(s.cx == 0 && s.cy == 0);
    s.cx = 5; s.cy = 6;
    CHECK(U(GetTextExtentPoint32W)(c.dc, NULL, 0, &s) == TRUE);
    CHECK(s.cx == 0 && s.cy == 0);
    s.cx = 5; s.cy = 6;
    CHECK(U(GetTextExtentPoint32A)(c.dc, NULL, 0, &s) == TRUE);
    CHECK(s.cx == 0 && s.cy == 0);
    /* count 0 never looks at the DC */
    s.cx = 5; s.cy = 6;
    CHECK(U(GetTextExtentPoint32W)(NULL, L"a", 0, &s) == TRUE);
    CHECK(s.cx == 0 && s.cy == 0);
    to_close(&c);
}
TEST(text_output_ext_point_empty_string_zero) {
    NEED_EXT();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    SIZE s = { 77, 88 };
    BOOL r = U(GetTextExtentPointA)(c.dc, "", 0, &s);
    t_note("GetTextExtentPointA empty ret=%d", r);
    CHECK(s.cx == 0 && s.cy == 0);
    s.cx = 5; s.cy = 6;
    r = U(GetTextExtentPointW)(c.dc, L"", 0, &s);
    t_note("GetTextExtentPointW empty ret=%d", r);
    CHECK(s.cx == 0 && s.cy == 0);
    to_close(&c);
}
TEST(text_output_ext32_negative_count) {
    NEED_EXT();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    static const int n[] = { -1, -2, -1000, (int)0x80000000 };
    for (int i = 0; i < ARRAYSZ(n); i++) {
        SIZE s = { 1234, 5678 };
        SetLastError(0);
        CHECKF(U(GetTextExtentPoint32A)(c.dc, "abc", n[i], &s) == FALSE, "A %d", n[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "A err %lu", GetLastError());
        SetLastError(0);
        CHECKF(U(GetTextExtentPoint32W)(c.dc, L"abc", n[i], &s) == FALSE, "W %d", n[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "W err %lu", GetLastError());
        SetLastError(0);
        CHECKF(U(GetTextExtentPointA)(c.dc, "abc", n[i], &s) == FALSE, "PointA %d", n[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "PointA err %lu", GetLastError());
        SetLastError(0);
        CHECKF(U(GetTextExtentPointW)(c.dc, L"abc", n[i], &s) == FALSE, "PointW %d", n[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "PointW err %lu", GetLastError());
    }
    to_close(&c);
}
TEST(text_output_ext32_bogus_dc_a) {
    NEED_EXT();
    static const ULONG_PTR h[] = { 0, 1, 0x1234, 0xffffffff };
    for (int i = 0; i < ARRAYSZ(h); i++) {
        SIZE s = { 1, 2 };
        SetLastError(0);
        CHECKF(U(GetTextExtentPoint32A)((HDC)h[i], "abc", 3, &s) == FALSE, "h=%zx", (size_t)h[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "err %lu", GetLastError());
    }
}
TEST(text_output_ext32_bogus_dc_w) {
    NEED_EXT();
    static const ULONG_PTR h[] = { 0, 1, 0x1234, 0xffffffff };
    for (int i = 0; i < ARRAYSZ(h); i++) {
        SIZE s = { 1, 2 };
        SetLastError(0);
        CHECKF(U(GetTextExtentPoint32W)((HDC)h[i], L"abc", 3, &s) == FALSE, "h=%zx", (size_t)h[i]);
        t_note("W bogus dc %zx err=%lu", (size_t)h[i], GetLastError());
        SetLastError(0);
        CHECKF(U(GetTextExtentPoint32W)((HDC)h[i], L"\x0430\x0431", 2, &s) == FALSE, "non-ascii h=%zx", (size_t)h[i]);
    }
}
TEST(text_output_ext32_wrong_object_type_dc) {
    NEED_EXT();
    SIZE s = { 1, 2 };
    HGDIOBJ obj[3] = { U(GetStockObject)(SYSTEM_FONT), U(GetStockObject)(WHITE_BRUSH), U(GetStockObject)(BLACK_PEN) };
    for (int i = 0; i < 3; i++) {
        SetLastError(0);
        CHECKF(U(GetTextExtentPoint32A)((HDC)obj[i], "abc", 3, &s) == FALSE, "A obj %d", i);
        SetLastError(0);
        CHECKF(U(GetTextExtentPoint32W)((HDC)obj[i], L"abc", 3, &s) == FALSE, "W obj %d", i);
    }
}
TEST(text_output_ext32_width_fixed_font_exact) {
    NEED_EXT();
    static const int fonts[] = { SYSTEM_FIXED_FONT, OEM_FIXED_FONT };
    for (int f = 0; f < 2; f++) {
        TCtx c; TEXTMETRICA tm; if (!fixed_ctx(&c, &tm, fonts[f])) continue;
        static const char *str = "The quick brown fox jumps over";
        for (int n = 1; n <= 30; n++) {
            SIZE a, w; WCHAR wb[64]; to_w(str, wb, 64);
            CHECK(U(GetTextExtentPoint32A)(c.dc, str, n, &a));
            CHECK(U(GetTextExtentPoint32W)(c.dc, wb, n, &w));
            CHECKF(a.cx == n * tm.tmAveCharWidth, "font %d n=%d A cx=%ld ave=%ld", fonts[f], n, a.cx, tm.tmAveCharWidth);
            CHECKF(w.cx == a.cx && w.cy == a.cy, "A/W differ n=%d", n);
            CHECKF(a.cy == tm.tmHeight, "cy %ld height %ld", a.cy, tm.tmHeight);
        }
        to_close(&c);
    }
}
TEST(text_output_ext_point_matches_ext32) {
    NEED_EXT();
    static const char *face[] = { "Arial", "Courier New", "Times New Roman", "Tahoma", "Segoe UI", "Consolas" };
    for (int f = 0; f < ARRAYSZ(face); f++) {
        TCtx c; if (!tt_ctx(&c, face[f], -16)) continue;
        SIZE a, b;
        CHECK(U(GetTextExtentPointA)(c.dc, "Hello, World", 12, &a));
        CHECK(U(GetTextExtentPoint32A)(c.dc, "Hello, World", 12, &b));
        CHECKF(a.cx == b.cx && a.cy == b.cy, "%s: %ldx%ld vs %ldx%ld", face[f], a.cx, a.cy, b.cx, b.cy);
        CHECK(U(GetTextExtentPointW)(c.dc, L"Hello, World", 12, &a));
        CHECK(U(GetTextExtentPoint32W)(c.dc, L"Hello, World", 12, &b));
        CHECKF(a.cx == b.cx && a.cy == b.cy, "%s W: %ldx%ld vs %ldx%ld", face[f], a.cx, a.cy, b.cx, b.cy);
        to_close(&c);
    }
}
TEST(text_output_ext32_a_w_parity_ascii) {
    NEED_EXT();
    static const char *face[] = { "Arial", "Courier New", "Times New Roman", "Tahoma", "Segoe UI", "Consolas", "Symbol" };
    static const int hs[] = { -8, -12, -20, -40 };
    for (int f = 0; f < ARRAYSZ(face); f++) for (int hi = 0; hi < 4; hi++) {
        TCtx c; if (!tt_ctx(&c, face[f], hs[hi])) continue;
        char s[96]; WCHAR w[96]; int n = 0;
        for (int ch = 0x20; ch < 0x7f; ch++) s[n++] = (char)ch;
        s[n] = 0; to_w(s, w, 96);
        SIZE a, b;
        CHECK(U(GetTextExtentPoint32A)(c.dc, s, n, &a));
        CHECK(U(GetTextExtentPoint32W)(c.dc, w, n, &b));
        CHECKF(a.cx == b.cx && a.cy == b.cy, "%s %d: A %ldx%ld W %ldx%ld", face[f], hs[hi], a.cx, a.cy, b.cx, b.cy);
        to_close(&c);
    }
}
TEST(text_output_ext32_cy_equals_tmheight) {
    NEED_EXT();
    static const char *face[] = { "Arial", "Courier New", "Times New Roman", "Tahoma", "Segoe UI", "Consolas" };
    for (int f = 0; f < ARRAYSZ(face); f++) {
        TCtx c; if (!tt_ctx(&c, face[f], -20)) continue;
        TEXTMETRICA tm; CHECK(U(GetTextMetricsA)(c.dc, &tm));
        SIZE s; CHECK(U(GetTextExtentPoint32A)(c.dc, "Xg", 2, &s));
        CHECKF(s.cy == tm.tmHeight, "%s cy %ld tmHeight %ld", face[f], s.cy, tm.tmHeight);
        CHECK(U(GetTextExtentPoint32A)(c.dc, "x", 1, &s));
        CHECKF(s.cy == tm.tmHeight, "%s cy(x) %ld", face[f], s.cy);
        to_close(&c);
    }
}
TEST(text_output_ext32_additive_fixed_font) {
    NEED_EXT();
    TCtx c; TEXTMETRICA tm; if (!fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT)) return;
    SIZE a, b, ab;
    CHECK(U(GetTextExtentPoint32A)(c.dc, "abc", 3, &a));
    CHECK(U(GetTextExtentPoint32A)(c.dc, "defg", 4, &b));
    CHECK(U(GetTextExtentPoint32A)(c.dc, "abcdefg", 7, &ab));
    CHECK(a.cx + b.cx == ab.cx);
    to_close(&c);
}
TEST(text_output_ext32_alignment_independent) {
    NEED_EXT();
    static const UINT al[] = { TA_LEFT, TA_RIGHT, TA_CENTER, TA_TOP, TA_BOTTOM, TA_BASELINE, TA_UPDATECP, TA_RIGHT | TA_BOTTOM, TA_CENTER | TA_BASELINE, TA_RTLREADING };
    TCtx c; if (!tt_ctx(&c, "Arial", -16)) return;
    SIZE ref; CHECK(U(GetTextExtentPoint32A)(c.dc, "Alignment", 9, &ref));
    for (int i = 0; i < ARRAYSZ(al); i++) {
        U(SetTextAlign)(c.dc, al[i]);
        SIZE s; CHECK(U(GetTextExtentPoint32A)(c.dc, "Alignment", 9, &s));
        CHECKF(s.cx == ref.cx && s.cy == ref.cy, "align 0x%x: %ldx%ld vs %ldx%ld", al[i], s.cx, s.cy, ref.cx, ref.cy);
    }
    to_close(&c);
}
TEST(text_output_ext32_does_not_move_cp) {
    NEED_EXT(); REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    TCtx c; TEXTMETRICA tm; if (!fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT)) return;
    U(SetTextAlign)(c.dc, TA_UPDATECP);
    U(MoveToEx)(c.dc, 11, 13, NULL);
    SIZE s; CHECK(U(GetTextExtentPoint32A)(c.dc, "abc", 3, &s));
    POINT p; CHECK(U(GetCurrentPositionEx)(c.dc, &p));
    CHECK(p.x == 11 && p.y == 13);
    to_close(&c);
}
TEST(text_output_ext32_char_extra_exact) {
    NEED_EXT(); REQUIRE(U(SetTextCharacterExtra)); REQUIRE(U(GetTextCharacterExtra));
    TCtx c; TEXTMETRICA tm; if (!fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT)) return;
    static const int ex[] = { 0, 1, 2, 5, 17, -1, -2, 100 };
    for (int i = 0; i < ARRAYSZ(ex); i++) {
        U(SetTextCharacterExtra)(c.dc, ex[i]);
        CHECKF(U(GetTextCharacterExtra)(c.dc) == ex[i], "roundtrip %d", ex[i]);
        SIZE s; CHECK(U(GetTextExtentPoint32A)(c.dc, "abcde", 5, &s));
        CHECKF(s.cx == 5 * (tm.tmAveCharWidth + ex[i]), "extra %d cx=%ld", ex[i], s.cx);
        CHECK(U(GetTextExtentPoint32W)(c.dc, L"abcde", 5, &s));
        CHECKF(s.cx == 5 * (tm.tmAveCharWidth + ex[i]), "W extra %d cx=%ld", ex[i], s.cx);
    }
    U(SetTextCharacterExtra)(c.dc, 0);
    to_close(&c);
}
TEST(text_output_char_extra_return_values) {
    NEED_EXT(); REQUIRE(U(SetTextCharacterExtra)); REQUIRE(U(GetTextCharacterExtra));
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    CHECK(U(GetTextCharacterExtra)(c.dc) == 0);
    CHECK(U(SetTextCharacterExtra)(c.dc, 3) == 0);
    CHECK(U(SetTextCharacterExtra)(c.dc, 7) == 3);
    CHECK(U(SetTextCharacterExtra)(c.dc, -4) == 7);
    CHECK(U(SetTextCharacterExtra)(c.dc, 0) == -4);
    SetLastError(0);
    CHECK(U(SetTextCharacterExtra)(NULL, 3) == (int)0x80000000);
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    /* INT_MIN is not an acceptable extra value even on a valid DC */
    SetLastError(0);
    CHECK(U(SetTextCharacterExtra)(c.dc, (int)0x80000000) == (int)0x80000000);
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    CHECK(U(GetTextCharacterExtra)(c.dc) == 0);
    CHECK(U(GetTextCharacterExtra)(NULL) == (int)0x80000000);
    CHECK(U(SetTextCharacterExtra)((HDC)(ULONG_PTR)0x1234, 3) == (int)0x80000000);
    to_close(&c);
}
TEST(text_output_char_extra_per_dc) {
    NEED_EXT(); REQUIRE(U(SetTextCharacterExtra)); REQUIRE(U(GetTextCharacterExtra));
    TCtx a, b; if (!to_open(&a, 32, 32, 32)) { CHECK(0); return; }
    if (!to_open(&b, 32, 32, 32)) { to_close(&a); CHECK(0); return; }
    U(SetTextCharacterExtra)(a.dc, 9);
    CHECK(U(GetTextCharacterExtra)(b.dc) == 0);
    CHECK(U(GetTextCharacterExtra)(a.dc) == 9);
    to_close(&a); to_close(&b);
}
TEST(text_output_char_extra_saved_restored) {
    NEED_EXT(); REQUIRE(U(SetTextCharacterExtra)); REQUIRE(U(GetTextCharacterExtra)); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    U(SetTextCharacterExtra)(c.dc, 4);
    int id = U(SaveDC)(c.dc);
    U(SetTextCharacterExtra)(c.dc, 10);
    CHECK(U(RestoreDC)(c.dc, id));
    CHECK(U(GetTextCharacterExtra)(c.dc) == 4);
    to_close(&c);
}
TEST(text_output_justification_extent) {
    NEED_EXT(); REQUIRE(U(SetTextJustification));
    TCtx c; TEXTMETRICA tm; if (!fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT)) return;
    const char *s = "a b c d"; /* 3 spaces */
    SIZE base; CHECK(U(GetTextExtentPoint32A)(c.dc, s, 7, &base));
    CHECK(U(SetTextJustification)(c.dc, 12, 3));
    SIZE j; CHECK(U(GetTextExtentPoint32A)(c.dc, s, 7, &j));
    CHECKF(j.cx == base.cx + 12, "justified %ld base %ld", j.cx, base.cx);
    CHECK(U(SetTextJustification)(c.dc, 0, 0));
    CHECK(U(GetTextExtentPoint32A)(c.dc, s, 7, &j));
    CHECK(j.cx == base.cx);
    to_close(&c);
}
TEST(text_output_justification_no_breaks_in_string) {
    NEED_EXT(); REQUIRE(U(SetTextJustification));
    TCtx c; TEXTMETRICA tm; if (!fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT)) return;
    SIZE base; CHECK(U(GetTextExtentPoint32A)(c.dc, "abcd", 4, &base));
    CHECK(U(SetTextJustification)(c.dc, 20, 2));
    SIZE j; CHECK(U(GetTextExtentPoint32A)(c.dc, "abcd", 4, &j));
    CHECKF(j.cx == base.cx, "no space chars -> unchanged (%ld vs %ld)", j.cx, base.cx);
    CHECK(U(SetTextJustification)(c.dc, 0, 0));
    to_close(&c);
}
TEST(text_output_justification_bogus_dc) {
    NEED_EXT(); REQUIRE(U(SetTextJustification));
    SetLastError(0);
    CHECK(U(SetTextJustification)(NULL, 5, 2) == FALSE);
    CHECK(U(SetTextJustification)((HDC)(ULONG_PTR)0x1234, 5, 2) == FALSE);
}
TEST(text_output_justification_zero_breaks_nonzero_extra) {
    NEED_EXT(); REQUIRE(U(SetTextJustification));
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    /* the values are stored unvalidated */
    CHECK(U(SetTextJustification)(c.dc, 10, 0) == TRUE);
    CHECK(U(SetTextJustification)(c.dc, -10, 3) == TRUE);
    CHECK(U(SetTextJustification)(c.dc, 10, -1) == TRUE);
    CHECK(U(SetTextJustification)(c.dc, 0, 0));
    to_close(&c);
}
TEST(text_output_ext_font_scaling_monotonic) {
    NEED_EXT();
    int last = 0;
    for (int h = 8; h <= 64; h += 8) {
        TCtx c; if (!tt_ctx(&c, "Arial", -h)) return;
        SIZE s; CHECK(U(GetTextExtentPoint32A)(c.dc, "Monotonic", 9, &s));
        CHECKF(s.cx > last, "h=%d cx=%ld last=%d", h, s.cx, last);
        last = s.cx;
        to_close(&c);
    }
}
TEST(text_output_ext_font_change_changes_extent) {
    NEED_EXT();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    HFONT f1 = to_mkfont("Arial", -12, FW_NORMAL, 0, NONANTIALIASED_QUALITY);
    HFONT f2 = to_mkfont("Arial", -36, FW_NORMAL, 0, NONANTIALIASED_QUALITY);
    to_use_font(&c, f1, 0);
    SIZE a, b; CHECK(U(GetTextExtentPoint32A)(c.dc, "Wide", 4, &a));
    U(SelectObject)(c.dc, f2);
    CHECK(U(GetTextExtentPoint32A)(c.dc, "Wide", 4, &b));
    CHECKF(b.cx > a.cx * 2 && b.cy > a.cy * 2, "%ldx%ld -> %ldx%ld", a.cx, a.cy, b.cx, b.cy);
    U(SelectObject)(c.dc, c.ofont);
    U(DeleteObject)(f1); U(DeleteObject)(f2);
    c.ofont = NULL;
    to_close(&c);
}
TEST(text_output_ext_bold_wider_than_regular) {
    NEED_EXT();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    HFONT f1 = to_mkfont("Arial", -24, FW_NORMAL, 0, NONANTIALIASED_QUALITY);
    HFONT f2 = to_mkfont("Arial", -24, FW_BOLD, 0, NONANTIALIASED_QUALITY);
    to_use_font(&c, f1, 0);
    SIZE a, b; CHECK(U(GetTextExtentPoint32A)(c.dc, "Bold text", 9, &a));
    U(SelectObject)(c.dc, f2);
    CHECK(U(GetTextExtentPoint32A)(c.dc, "Bold text", 9, &b));
    CHECKF(b.cx > a.cx, "bold %ld regular %ld", b.cx, a.cx);
    U(SelectObject)(c.dc, c.ofont);
    U(DeleteObject)(f1); U(DeleteObject)(f2);
    c.ofont = NULL;
    to_close(&c);
}
TEST(text_output_ext_courier_new_fixed_advance) {
    NEED_EXT();
    TCtx c; if (!tt_ctx(&c, "Courier New", -20)) return;
    SIZE a, b;
    CHECK(U(GetTextExtentPoint32A)(c.dc, "iiiiiiii", 8, &a));
    CHECK(U(GetTextExtentPoint32A)(c.dc, "WWWWWWWW", 8, &b));
    CHECKF(a.cx == b.cx, "i %ld W %ld", a.cx, b.cx);
    to_close(&c);
}
TEST(text_output_ext_escapement_no_effect_on_extent) {
    NEED_EXT();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    static const int esc[] = { 0, 450, 900, 1800, 2700, 3590 };
    SIZE ref = { 0, 0 };
    for (int i = 0; i < ARRAYSZ(esc); i++) {
        HFONT f = to_mkfont("Arial", -20, FW_NORMAL, esc[i], NONANTIALIASED_QUALITY);
        HFONT o = (HFONT)U(SelectObject)(c.dc, f);
        if (!c.ofont) c.ofont = o;
        SIZE s; CHECK(U(GetTextExtentPoint32A)(c.dc, "Rotate me", 9, &s));
        if (i == 0) ref = s;
        else CHECKF(s.cx == ref.cx && s.cy == ref.cy, "esc %d: %ldx%ld vs %ldx%ld", esc[i], s.cx, s.cy, ref.cx, ref.cy);
        U(SelectObject)(c.dc, c.ofont); U(DeleteObject)(f);
    }
    to_close(&c);
}
TEST(text_output_ext_zero_height_font_default_size) {
    NEED_EXT();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    HFONT f = U(CreateFontA)(0, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, NONANTIALIASED_QUALITY, 0, "Arial");
    CHECK(f != NULL);
    to_use_font(&c, f, 1);
    TEXTMETRICA tm; CHECK(U(GetTextMetricsA)(c.dc, &tm));
    CHECK(tm.tmHeight > 0);
    SIZE s; CHECK(U(GetTextExtentPoint32A)(c.dc, "Zero", 4, &s));
    CHECK(s.cx > 0 && s.cy == tm.tmHeight);
    to_close(&c);
}
TEST(text_output_ext_large_font_bounded) {
    NEED_EXT();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Arial", -1000, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    SIZE s; CHECK(U(GetTextExtentPoint32A)(c.dc, "Wi", 2, &s));
    CHECKF(s.cy >= 1000 && s.cy < 1500, "cy %ld", s.cy);
    CHECKF(s.cx > 800 && s.cx < 3000, "cx %ld", s.cx);
    to_close(&c);
}
TEST(text_output_ext_tiny_font) {
    NEED_EXT();
    TCtx c; if (!tt_ctx(&c, "Arial", -1)) return;
    SIZE s; CHECK(U(GetTextExtentPoint32A)(c.dc, "tiny", 4, &s));
    CHECKF(s.cx >= 0 && s.cy >= 0 && s.cx < 100, "tiny %ldx%ld", s.cx, s.cy);
    to_close(&c);
}
TEST(text_output_ext_embedded_nul_a) {
    NEED_EXT();
    TCtx c; TEXTMETRICA tm; if (!fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT)) return;
    char b[4] = { 'a', 0, 'b', 'c' };
    SIZE s; CHECK(U(GetTextExtentPoint32A)(c.dc, b, 4, &s));
    CHECKF(s.cx == 4 * tm.tmAveCharWidth, "cx %ld", s.cx);
    WCHAR w[4] = { L'a', 0, L'b', L'c' };
    CHECK(U(GetTextExtentPoint32W)(c.dc, w, 4, &s));
    CHECKF(s.cx == 4 * tm.tmAveCharWidth, "W cx %ld", s.cx);
    to_close(&c);
}
TEST(text_output_ext_control_chars_measured_as_cells) {
    NEED_EXT();
    TCtx c; TEXTMETRICA tm; if (!fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT)) return;
    SIZE s; CHECK(U(GetTextExtentPoint32A)(c.dc, "a\tb\nc", 5, &s));
    t_note("control chars: cx=%ld cells=%ld", s.cx, s.cx / tm.tmAveCharWidth);
    CHECK(s.cx > 0);
    to_close(&c);
}
TEST(text_output_ext_ptr_variants_same_size_struct) {
    NEED_EXT();
    CHECK(sizeof(SIZE) == 8);
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    struct { SIZE s; int guard; } g = { { 0, 0 }, 0x5a5a5a5a };
    CHECK(U(GetTextExtentPoint32A)(c.dc, "abc", 3, &g.s));
    CHECK(g.guard == 0x5a5a5a5a);
    CHECK(U(GetTextExtentPoint32W)(c.dc, L"abc", 3, &g.s));
    CHECK(g.guard == 0x5a5a5a5a);
    to_close(&c);
}
TEST(text_output_ext_long_string_scales_linearly) {
    NEED_EXT();
    TCtx c; TEXTMETRICA tm; if (!fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT)) return;
    static char s[5000]; static WCHAR w[5000];
    for (int i = 0; i < 4999; i++) { s[i] = 'a' + i % 26; w[i] = s[i]; }
    static const int n[] = { 1, 127, 128, 129, 130, 131, 132, 255, 256, 257, 1000, 4999 };
    for (int i = 0; i < ARRAYSZ(n); i++) {
        SIZE a, b;
        CHECKF(U(GetTextExtentPoint32A)(c.dc, s, n[i], &a), "A n=%d", n[i]);
        CHECKF(U(GetTextExtentPoint32W)(c.dc, w, n[i], &b), "W n=%d", n[i]);
        CHECKF(a.cx == n[i] * tm.tmAveCharWidth && b.cx == a.cx, "n=%d A=%ld W=%ld", n[i], a.cx, b.cx);
    }
    to_close(&c);
}
/* ---------- GetTextExtentExPoint ---------- */
TEST(text_output_extex_fit_counts_fixed) {
    NEED_EXT();
    TCtx c; TEXTMETRICA tm; if (!fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT)) return;
    int aw = tm.tmAveCharWidth;
    static const char *s = "abcdefghij";
    WCHAR w[16]; to_w(s, w, 16);
    int me[] = { 0, 1, aw - 1, aw, aw + 1, 3 * aw - 1, 3 * aw, 3 * aw + 1, 10 * aw - 1, 10 * aw, 10 * aw + 1, 1000 };
    int want[] = { 0, 0, 0, 1, 1, 2, 3, 3, 9, 10, 10, 10 };
    for (int i = 0; i < ARRAYSZ(me); i++) {
        int fit = -1; SIZE sz = { -1, -1 };
        CHECK(U(GetTextExtentExPointA)(c.dc, s, 10, me[i], &fit, NULL, &sz));
        CHECKF(fit == want[i], "A max=%d fit=%d want %d", me[i], fit, want[i]);
        fit = -1;
        CHECK(U(GetTextExtentExPointW)(c.dc, w, 10, me[i], &fit, NULL, &sz));
        CHECKF(fit == want[i], "W max=%d fit=%d want %d", me[i], fit, want[i]);
    }
    to_close(&c);
}
TEST(text_output_extex_size_is_fitted_part) {
    NEED_EXT();
    TCtx c; TEXTMETRICA tm; if (!fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT)) return;
    int aw = tm.tmAveCharWidth;
    int fit; SIZE sz = { -1, -1 };
    CHECK(U(GetTextExtentExPointA)(c.dc, "abcdefghij", 10, 4 * aw + 2, &fit, NULL, &sz));
    CHECK(fit == 4);
    t_note("size with maxExtent=%d: cx=%ld (docs: whole string)", 4 * aw + 2, sz.cx);
    CHECK(sz.cx == 4 * aw || sz.cx == 10 * aw);
    CHECK(sz.cy == tm.tmHeight);
    to_close(&c);
}
TEST(text_output_extex_unlimited_maxextent_minus1) {
    NEED_EXT();
    TCtx c; TEXTMETRICA tm; if (!fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT)) return;
    int fit = -5; SIZE sz;
    CHECK(U(GetTextExtentExPointA)(c.dc, "abcdefghij", 10, -1, &fit, NULL, &sz));
    CHECKF(fit == 10, "fit %d", fit);
    CHECK(sz.cx == 10 * tm.tmAveCharWidth);
    CHECK(U(GetTextExtentExPointW)(c.dc, L"abcdefghij", 10, -1, &fit, NULL, &sz));
    CHECKF(fit == 10, "W fit %d", fit);
    to_close(&c);
}
TEST(text_output_extex_maxextent_below_minus1_fails) {
    NEED_EXT();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    static const int me[] = { -2, -3, -1000, (int)0x80000000 };
    for (int i = 0; i < ARRAYSZ(me); i++) {
        int fit = 77; SIZE sz;
        SetLastError(0);
        CHECKF(U(GetTextExtentExPointA)(c.dc, "abc", 3, me[i], &fit, NULL, &sz) == FALSE, "A %d", me[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "A err %lu", GetLastError());
        SetLastError(0);
        BOOL rw = U(GetTextExtentExPointW)(c.dc, L"abc", 3, me[i], &fit, NULL, &sz);
        t_note("ExPointW maxExtent=%d ret=%d err=%lu", me[i], rw, GetLastError());
    }
    to_close(&c);
}
TEST(text_output_extex_negative_count_a) {
    NEED_EXT();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    static const int n[] = { -1, -50, (int)0x80000000, 0x2aaaaaab, 0x7fffffff };
    for (int i = 0; i < ARRAYSZ(n); i++) {
        int fit = 0; SIZE sz;
        if (to_wine() && n[i] > 0) continue;
        SetLastError(0);
        CHECKF(U(GetTextExtentExPointA)(c.dc, "abc", n[i], 100, &fit, NULL, &sz) == FALSE, "n=%d", n[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "n=%d err %lu", n[i], GetLastError());
    }
    to_close(&c);
}
TEST(text_output_extex_negative_count_w) {
    NEED_EXT();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    static const int n[] = { -1, -50, (int)0x80000000 };
    for (int i = 0; i < ARRAYSZ(n); i++) {
        int fit = 0; SIZE sz;
        SetLastError(0);
        CHECKF(U(GetTextExtentExPointW)(c.dc, L"abc", n[i], 100, &fit, NULL, &sz) == FALSE, "n=%d", n[i]);
        t_note("ExPointW count %d err %lu", n[i], GetLastError());
    }
    to_close(&c);
}
TEST(text_output_extex_zero_count) {
    NEED_EXT();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    int fit = 99; SIZE sz = { 9, 9 };
    SetLastError(0);
    BOOL a = U(GetTextExtentExPointA)(c.dc, "", 0, 100, &fit, NULL, &sz);
    t_note("ExPointA count 0 -> ret=%d fit=%d size=%ldx%ld", a, fit, sz.cx, sz.cy);
    fit = 99; sz.cx = sz.cy = 9;
    BOOL w = U(GetTextExtentExPointW)(c.dc, L"", 0, 100, &fit, NULL, &sz);
    t_note("ExPointW count 0 -> ret=%d fit=%d size=%ldx%ld", w, fit, sz.cx, sz.cy);
    to_close(&c);
}
TEST(text_output_extex_partial_dx_fixed) {
    NEED_EXT();
    TCtx c; TEXTMETRICA tm; if (!fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT)) return;
    int dx[12]; int fit; SIZE sz;
    memset(dx, 0xcc, sizeof dx);
    CHECK(U(GetTextExtentExPointA)(c.dc, "abcdefgh", 8, -1, &fit, dx, &sz));
    for (int i = 0; i < 8; i++) CHECKF(dx[i] == (i + 1) * tm.tmAveCharWidth, "dx[%d]=%d", i, dx[i]);
    CHECK(dx[7] == sz.cx);
    CHECK(dx[8] == (int)0xcccccccc);
    to_close(&c);
}
TEST(text_output_extex_partial_dx_monotonic_proportional) {
    NEED_EXT();
    static const char *face[] = { "Arial", "Times New Roman", "Tahoma", "Segoe UI" };
    for (int f = 0; f < ARRAYSZ(face); f++) {
        TCtx c; if (!tt_ctx(&c, face[f], -18)) continue;
        static const char *s = "Monotonic dx values";
        int n = (int)strlen(s);
        int dx[64], fit = 0; SIZE sz;
        CHECK(U(GetTextExtentExPointA)(c.dc, s, n, -1, &fit, dx, &sz));
        CHECK(fit == n);
        for (int i = 1; i < n; i++) CHECKF(dx[i] >= dx[i - 1], "%s dx[%d]=%d < %d", face[f], i, dx[i], dx[i - 1]);
        CHECKF(dx[n - 1] == sz.cx, "%s last %d total %ld", face[f], dx[n - 1], sz.cx);
        SIZE e; CHECK(U(GetTextExtentPoint32A)(c.dc, s, n, &e));
        CHECKF(e.cx == sz.cx, "%s ExPoint total %ld vs Point32 %ld", face[f], sz.cx, e.cx);
        for (int i = 0; i < n; i += 5) {
            SIZE p; CHECK(U(GetTextExtentPoint32A)(c.dc, s, i + 1, &p));
            CHECKF(p.cx == dx[i], "%s prefix %d: %ld vs dx %d", face[f], i + 1, p.cx, dx[i]);
        }
        to_close(&c);
    }
}
TEST(text_output_extex_dx_w_matches_a) {
    NEED_EXT();
    TCtx c; if (!tt_ctx(&c, "Arial", -16)) return;
    int da[32], dw[32], fa, fw; SIZE sa, sw;
    CHECK(U(GetTextExtentExPointA)(c.dc, "Compare A and W", 15, -1, &fa, da, &sa));
    CHECK(U(GetTextExtentExPointW)(c.dc, L"Compare A and W", 15, -1, &fw, dw, &sw));
    CHECK(fa == fw && sa.cx == sw.cx && sa.cy == sw.cy);
    CHECK(!memcmp(da, dw, 15 * sizeof(int)));
    to_close(&c);
}
TEST(text_output_extex_null_optionals) {
    NEED_EXT();
    TCtx c; if (!tt_ctx(&c, "Arial", -16)) return;
    SIZE sz; int fit = -1, dx[8];
    CHECK(U(GetTextExtentExPointA)(c.dc, "abc", 3, -1, NULL, NULL, &sz));
    CHECK(U(GetTextExtentExPointA)(c.dc, "abc", 3, 1000, NULL, dx, &sz));
    CHECK(U(GetTextExtentExPointA)(c.dc, "abc", 3, 1000, &fit, NULL, &sz));
    CHECK(U(GetTextExtentExPointW)(c.dc, L"abc", 3, -1, NULL, NULL, &sz));
    CHECK(U(GetTextExtentExPointW)(c.dc, L"abc", 3, 1000, NULL, dx, &sz));
    CHECK(U(GetTextExtentExPointW)(c.dc, L"abc", 3, 1000, &fit, NULL, &sz));
    to_close(&c);
}
TEST(text_output_extex_bogus_dc) {
    NEED_EXT();
    SIZE sz; int fit;
    SetLastError(0);
    CHECK(U(GetTextExtentExPointA)(NULL, "abc", 3, -1, &fit, NULL, &sz) == FALSE);
    CHECK(U(GetTextExtentExPointW)(NULL, L"abc", 3, -1, &fit, NULL, &sz) == FALSE);
    CHECK(U(GetTextExtentExPointA)((HDC)(ULONG_PTR)0x1234, "abc", 3, -1, &fit, NULL, &sz) == FALSE);
    CHECK(U(GetTextExtentExPointW)((HDC)(ULONG_PTR)0x1234, L"abc", 3, -1, &fit, NULL, &sz) == FALSE);
}
TEST(text_output_extex_fit_with_char_extra) {
    NEED_EXT(); REQUIRE(U(SetTextCharacterExtra));
    TCtx c; TEXTMETRICA tm; if (!fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT)) return;
    int cell = tm.tmAveCharWidth + 3;
    U(SetTextCharacterExtra)(c.dc, 3);
    int fit; SIZE sz;
    CHECK(U(GetTextExtentExPointA)(c.dc, "abcdefgh", 8, 4 * cell, &fit, NULL, &sz));
    CHECKF(fit == 4, "fit %d", fit);
    CHECK(U(GetTextExtentExPointA)(c.dc, "abcdefgh", 8, 4 * cell - 1, &fit, NULL, &sz));
    CHECKF(fit == 3, "fit %d", fit);
    U(SetTextCharacterExtra)(c.dc, 0);
    to_close(&c);
}
TEST(text_output_extex_dx_buffer_exact_size) {
    /* the dx array needs exactly n entries; guard words beyond must stay untouched */
    NEED_EXT();
    TCtx c; if (!tt_ctx(&c, "Arial", -16)) return;
    int buf[10]; for (int i = 0; i < 10; i++) buf[i] = 0x11223344;
    int fit; SIZE sz;
    CHECK(U(GetTextExtentExPointA)(c.dc, "abcde", 5, -1, &fit, buf, &sz));
    for (int i = 0; i < 5; i++) CHECK(buf[i] != 0x11223344);
    for (int i = 5; i < 10; i++) CHECK(buf[i] == 0x11223344);
    for (int i = 0; i < 10; i++) buf[i] = 0x11223344;
    CHECK(U(GetTextExtentExPointW)(c.dc, L"abcde", 5, -1, &fit, buf, &sz));
    for (int i = 5; i < 10; i++) CHECK(buf[i] == 0x11223344);
    to_close(&c);
}
/* ---------- glyph index variants ---------- */
TEST(text_output_extent_i_matches_glyph_widths) {
    NEED_EXT(); REQUIRE(U(GetTextExtentPointI)); REQUIRE(U(GetTextExtentExPointI)); REQUIRE(U(GetGlyphIndicesA));
    TCtx c; if (!tt_ctx(&c, "Arial", -20)) return;
    WORD gi[8];
    DWORD r = U(GetGlyphIndicesA)(c.dc, "Glyphs!", 7, gi, 0);
    CHECK(r == 7);
    SIZE si, sc;
    CHECK(U(GetTextExtentPointI)(c.dc, gi, 7, &si));
    CHECK(U(GetTextExtentPoint32A)(c.dc, "Glyphs!", 7, &sc));
    CHECKF(si.cx == sc.cx && si.cy == sc.cy, "I %ldx%ld vs %ldx%ld", si.cx, si.cy, sc.cx, sc.cy);
    int dx[8], fit; SIZE se;
    CHECK(U(GetTextExtentExPointI)(c.dc, gi, 7, -1, &fit, dx, &se));
    CHECK(fit == 7 && se.cx == sc.cx);
    CHECKF(dx[6] == se.cx, "last dx %d total %ld", dx[6], se.cx);
    to_close(&c);
}
TEST(text_output_extent_i_zero_count) {
    NEED_EXT(); REQUIRE(U(GetTextExtentPointI));
    TCtx c; if (!tt_ctx(&c, "Arial", -20)) return;
    WORD gi[1] = { 3 };
    SIZE s = { 9, 9 };
    BOOL r = U(GetTextExtentPointI)(c.dc, gi, 0, &s);
    t_note("GetTextExtentPointI count 0 -> %d size=%ldx%ld", r, s.cx, s.cy);
    s.cx = s.cy = 9;
    r = U(GetTextExtentPointI)(c.dc, gi, -1, &s);
    CHECK(r == FALSE);
    to_close(&c);
}
TEST(text_output_extent_i_bogus_dc) {
    NEED_EXT(); REQUIRE(U(GetTextExtentPointI)); REQUIRE(U(GetTextExtentExPointI));
    WORD gi[2] = { 3, 4 }; SIZE s; int fit;
    CHECK(U(GetTextExtentPointI)(NULL, gi, 2, &s) == FALSE);
    CHECK(U(GetTextExtentExPointI)(NULL, gi, 2, -1, &fit, NULL, &s) == FALSE);
}
TEST(text_output_extent_i_bitmap_font) {
    NEED_EXT(); REQUIRE(U(GetTextExtentPointI));
    TCtx c; TEXTMETRICA tm; if (!fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT)) return;
    WORD gi[3] = { 1, 2, 3 }; SIZE s = { -1, -1 };
    BOOL r = U(GetTextExtentPointI)(c.dc, gi, 3, &s);
    t_note("GetTextExtentPointI on bitmap font -> %d %ldx%ld", r, s.cx, s.cy);
    to_close(&c);
}
TEST(text_output_extent_i_symbol_font) {
    NEED_EXT(); REQUIRE(U(GetTextExtentPointI));
    TCtx c; if (!tt_ctx(&c, "Marlett", -20)) return;
    WORD gi[3] = { 1, 2, 3 }; SIZE s = { -1, -1 };
    BOOL r = U(GetTextExtentPointI)(c.dc, gi, 3, &s);
    CHECK(r);
    CHECK(s.cx > 0 && s.cy > 0);
    to_close(&c);
}
