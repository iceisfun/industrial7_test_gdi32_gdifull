#include "to.h"
/* Additional text_output coverage: state preservation, parity, repeatability, edge flags. */

#define NEED_X() do { NEED_TEXT(); REQUIRE(U(GetTextAlign)); REQUIRE(U(SetTextAlign)); REQUIRE(U(GetTextColor)); \
    REQUIRE(U(GetBkColor)); REQUIRE(U(GetBkMode)); REQUIRE(U(GetROP2)); REQUIRE(U(SetTextCharacterExtra)); \
    REQUIRE(U(GetTextCharacterExtra)); REQUIRE(U(GetTextExtentPointA)); REQUIRE(U(GetTextExtentExPointW)); } while (0)

static int same_bits(const TCtx *a, const TCtx *b) {
    return a->w == b->w && a->h == b->h && a->bpp == b->bpp && !memcmp(a->bits, b->bits, (size_t)a->stride * a->h);
}

TEST(text_output_extra_dc_state_unchanged_by_textout) {
    NEED_X();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    U(SetTextColor)(c.dc, RGB(10, 20, 30));
    U(SetBkColor)(c.dc, RGB(200, 100, 50));
    U(SetBkMode)(c.dc, OPAQUE);
    U(SetROP2)(c.dc, R2_XORPEN);
    U(SetTextAlign)(c.dc, TA_RIGHT | TA_BASELINE);
    U(SetTextCharacterExtra)(c.dc, 0);
    U(TextOutA)(c.dc, 40, 20, "abc", 3);
    U(TextOutW)(c.dc, 40, 20, L"abc", 3);
    U(ExtTextOutW)(c.dc, 40, 20, ETO_OPAQUE, &(RECT){ 0, 0, 10, 10 }, L"abc", 3, NULL);
    CHECK(U(GetTextColor)(c.dc) == RGB(10, 20, 30));
    CHECK(U(GetBkColor)(c.dc) == RGB(200, 100, 50));
    CHECK(U(GetBkMode)(c.dc) == OPAQUE);
    CHECK(U(GetROP2)(c.dc) == R2_XORPEN);
    CHECK(U(GetTextAlign)(c.dc) == (TA_RIGHT | TA_BASELINE));
    CHECK(U(GetTextCharacterExtra)(c.dc) == 0);
    to_close(&c);
}
TEST(text_output_extra_textout_equals_exttextout_w) {
    NEED_X();
    static const char *face[] = { "Arial", "Courier New", "Tahoma" };
    for (int f = 0; f < 3; f++) {
        TCtx a, b;
        if (!to_open(&a, 120, 40, 32)) { CHECK(0); return; }
        if (!to_open(&b, 120, 40, 32)) { CHECK(0); to_close(&a); return; }
        to_use_font(&a, to_mkfont(face[f], -16, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
        to_use_font(&b, to_mkfont(face[f], -16, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
        U(SetTextColor)(a.dc, 0); U(SetTextColor)(b.dc, 0);
        CHECK(U(TextOutW)(a.dc, 3, 4, L"Parity Test", 11));
        CHECK(U(ExtTextOutW)(b.dc, 3, 4, 0, NULL, L"Parity Test", 11, NULL));
        CHECKF(same_bits(&a, &b), "%s: TextOutW differs from ExtTextOutW", face[f]);
        to_clear(&a); to_clear(&b);
        CHECK(U(TextOutA)(a.dc, 3, 4, "Parity Test", 11));
        CHECK(U(ExtTextOutA)(b.dc, 3, 4, 0, NULL, "Parity Test", 11, NULL));
        CHECKF(same_bits(&a, &b), "%s: TextOutA differs from ExtTextOutA", face[f]);
        to_close(&a); to_close(&b);
    }
}
TEST(text_output_extra_extent_is_repeatable) {
    NEED_X();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Times New Roman", -17, FW_BOLD, 0, DEFAULT_QUALITY), 1);
    SIZE first = { 0, 0 }, s;
    CHECK(U(GetTextExtentPoint32W)(c.dc, L"Repeatable extent", 17, &first));
    for (int i = 0; i < 300; i++) {
        s.cx = s.cy = -1;
        if (!U(GetTextExtentPoint32W)(c.dc, L"Repeatable extent", 17, &s) || s.cx != first.cx || s.cy != first.cy) {
            CHECKF(0, "iteration %d: %ldx%ld vs %ldx%ld", i, s.cx, s.cy, first.cx, first.cy);
            break;
        }
    }
    CHECK(first.cx > 0 && first.cy > 0);
    to_close(&c);
}
TEST(text_output_extra_trailing_space_is_measured) {
    NEED_X();
    TCtx c; TEXTMETRICA tm; if (!to_fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT, 64, 32)) return;
    SIZE a, b;
    CHECK(U(GetTextExtentPoint32A)(c.dc, "a", 1, &a));
    CHECK(U(GetTextExtentPoint32A)(c.dc, "a   ", 4, &b));
    CHECKF(b.cx == a.cx + 3 * tm.tmAveCharWidth, "%ld vs %ld", b.cx, a.cx);
    CHECK(U(GetTextExtentPoint32A)(c.dc, "   ", 3, &b));
    CHECK(b.cx == 3 * tm.tmAveCharWidth && b.cy == tm.tmHeight);
    to_close(&c);
}
TEST(text_output_extra_opaque_empty_rect_fills_nothing) {
    NEED_X();
    TCtx c; if (!to_open(&c, 40, 40, 32)) { CHECK(0); return; }
    U(SetBkColor)(c.dc, RGB(0, 0, 0));
    RECT r = { 10, 10, 10, 20 };
    CHECK(U(ExtTextOutW)(c.dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL));
    RECT ink; CHECK(to_ink(&c, to_white(&c), &ink) == 0);
    r.left = 10; r.right = 20; r.top = 10; r.bottom = 10;
    CHECK(U(ExtTextOutW)(c.dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL));
    CHECK(to_ink(&c, to_white(&c), &ink) == 0);
    /* a one-pixel rect fills exactly one pixel */
    r = (RECT){ 5, 6, 6, 7 };
    CHECK(U(ExtTextOutW)(c.dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL));
    CHECK(to_ink(&c, to_white(&c), &ink) == 1 && ink.left == 5 && ink.top == 6);
    CHECK(to_px(&c, 5, 6) == 0);
    to_close(&c);
}
TEST(text_output_extra_opaque_rect_clipped_to_surface) {
    NEED_X();
    TCtx c; if (!to_open(&c, 20, 20, 32)) { CHECK(0); return; }
    U(SetBkColor)(c.dc, RGB(0, 0, 0));
    RECT r = { -50, -50, 8, 8 };
    CHECK(U(ExtTextOutW)(c.dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL));
    RECT ink; CHECK(to_ink(&c, to_white(&c), &ink) == 64);
    CHECK(ink.left == 0 && ink.top == 0 && ink.right == 7 && ink.bottom == 7);
    to_clear(&c);
    r = (RECT){ 15, 15, 5000, 5000 };
    CHECK(U(ExtTextOutW)(c.dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL));
    CHECK(to_ink(&c, to_white(&c), &ink) == 25);
    to_close(&c);
}
TEST(text_output_extra_opaque_uses_bkcolor_even_if_transparent_mode) {
    NEED_X();
    TCtx c; if (!to_open(&c, 20, 20, 32)) { CHECK(0); return; }
    U(SetBkMode)(c.dc, TRANSPARENT);
    U(SetBkColor)(c.dc, RGB(255, 0, 0));
    CHECK(U(ExtTextOutW)(c.dc, 0, 0, ETO_OPAQUE, &(RECT){ 2, 2, 6, 6 }, NULL, 0, NULL));
    CHECK(to_px(&c, 3, 3) == to_cref2px(RGB(255, 0, 0)));
    CHECK(to_px(&c, 6, 6) == 0xffffff);
    CHECK(U(GetBkMode)(c.dc) == TRANSPARENT);
    to_close(&c);
}
TEST(text_output_extra_unknown_eto_flags_noted) {
    NEED_X();
    TCtx c; if (!to_open(&c, 40, 20, 32)) { CHECK(0); return; }
    static const UINT fl[] = { 0x80000000, 0x00000004, 0x00000008, 0x00400000, 0xffffffff };
    for (int i = 0; i < ARRAYSZ(fl); i++) {
        SetLastError(0);
        BOOL r = U(ExtTextOutW)(c.dc, 1, 1, fl[i], &(RECT){ 0, 0, 20, 10 }, L"ab", 2, NULL);
        t_note("ExtTextOutW flags %08x -> %d err=%lu", fl[i], r, GetLastError());
    }
    to_close(&c);
}
TEST(text_output_extra_mapmode_anisotropic_extent_scales) {
    NEED_X(); REQUIRE(U(SetMapMode)); REQUIRE(U(SetWindowExtEx)); REQUIRE(U(SetViewportExtEx));
    TCtx c; TEXTMETRICA tm; if (!to_fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT, 128, 64)) return;
    SIZE dev, lg;
    CHECK(U(GetTextExtentPoint32A)(c.dc, "abcdefgh", 8, &dev));
    CHECK(U(SetMapMode)(c.dc, MM_ANISOTROPIC) != 0);
    U(SetWindowExtEx)(c.dc, 1, 1, NULL);
    U(SetViewportExtEx)(c.dc, 2, 2, NULL);
    CHECK(U(GetTextExtentPoint32A)(c.dc, "abcdefgh", 8, &lg));
    t_note("device %ldx%ld, logical (2:1 viewport) %ldx%ld", dev.cx, dev.cy, lg.cx, lg.cy);
    if (!to_wine()) CHECKF(labs(lg.cx - dev.cx / 2) <= 1 && labs(lg.cy - dev.cy / 2) <= 1, "logical %ldx%ld", lg.cx, lg.cy);
    U(SetMapMode)(c.dc, MM_TEXT);
    CHECK(U(GetTextExtentPoint32A)(c.dc, "abcdefgh", 8, &lg));
    CHECK(lg.cx == dev.cx && lg.cy == dev.cy);
    to_close(&c);
}
TEST(text_output_extra_exex_maxextent_zero) {
    NEED_X();
    TCtx c; TEXTMETRICA tm; if (!to_fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT, 128, 64)) return;
    int fit = -5, dx[8]; SIZE s = { -1, -1 };
    SetLastError(0);
    BOOL r = U(GetTextExtentExPointW)(c.dc, L"abcdefg", 7, 0, &fit, dx, &s);
    t_note("maxExtent 0 -> ret %d fit %d size %ldx%ld err %lu", r, fit, s.cx, s.cy, GetLastError());
    CHECK(r == TRUE);
    CHECKF(fit == 0, "fit %d", fit);
    to_close(&c);
}
TEST(text_output_extra_exex_exact_boundary_fit) {
    NEED_X();
    TCtx c; TEXTMETRICA tm; if (!to_fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT, 128, 64)) return;
    int aw = tm.tmAveCharWidth;
    for (int n = 1; n <= 6; n++) {
        int fit = -5, dx[8]; SIZE s;
        CHECK(U(GetTextExtentExPointW)(c.dc, L"abcdefg", 7, n * aw, &fit, dx, &s));
        CHECKF(fit == n, "maxExtent exactly %d cells: fit %d", n, fit);
        CHECK(U(GetTextExtentExPointW)(c.dc, L"abcdefg", 7, n * aw - 1, &fit, dx, &s));
        CHECKF(fit == n - 1, "maxExtent %d cells - 1: fit %d", n, fit);
    }
    to_close(&c);
}
TEST(text_output_extra_charextra_roundtrip_values) {
    NEED_X();
    TCtx c; if (!to_open(&c, 16, 16, 32)) { CHECK(0); return; }
    static const int v[] = { 1, 5, -3, 100, 0 };
    int prev = 0;
    for (int i = 0; i < ARRAYSZ(v); i++) {
        int old = U(SetTextCharacterExtra)(c.dc, v[i]);
        CHECKF(old == prev, "set %d returned %d want %d", v[i], old, prev);
        CHECKF(U(GetTextCharacterExtra)(c.dc) == v[i], "get after %d", v[i]);
        prev = v[i];
    }
    to_close(&c);
}
TEST(text_output_extra_align_cp_only_with_updatecp) {
    NEED_X(); REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    TCtx c; TEXTMETRICA tm; if (!to_fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT, 128, 64)) return;
    U(MoveToEx)(c.dc, 9, 11, NULL);
    U(TextOutA)(c.dc, 50, 20, "xyz", 3);
    POINT p; U(GetCurrentPositionEx)(c.dc, &p);
    CHECK(p.x == 9 && p.y == 11);
    U(SetTextAlign)(c.dc, TA_UPDATECP);
    U(MoveToEx)(c.dc, 9, 11, NULL);
    U(TextOutA)(c.dc, 999, 999, "xyz", 3);
    U(GetCurrentPositionEx)(c.dc, &p);
    CHECKF(p.x == 9 + 3 * tm.tmAveCharWidth && p.y == 11, "cp %ld,%ld", p.x, p.y);
    U(SetTextAlign)(c.dc, TA_UPDATECP | TA_LEFT | TA_TOP);
    to_close(&c);
}
TEST(text_output_extra_textout_clip_to_surface_no_crash_all_depths) {
    NEED_X();
    static const int bpp[] = { 1, 8, 16, 24, 32 };
    for (int i = 0; i < 5; i++) {
        TCtx c; if (!to_open(&c, 17, 13, bpp[i])) { CHECKF(0, "open %d", bpp[i]); continue; }
        to_use_font(&c, to_mkfont("Arial", -40, FW_BOLD, 0, NONANTIALIASED_QUALITY), 1);
        U(SetTextColor)(c.dc, 0);
        CHECKF(U(TextOutA)(c.dc, -30, -10, "WWWW", 4), "bpp %d", bpp[i]);
        CHECKF(U(TextOutA)(c.dc, 10, 5, "WWWW", 4), "bpp %d", bpp[i]);
        to_close(&c);
    }
}
TEST(text_output_extra_gdi_count_stable_after_text_batch) {
    NEED_X();
    int before = to_gdi_count();
    if (before < 0) { t_skip("no GetGuiResources"); return; }
    for (int i = 0; i < 20; i++) {
        TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
        to_use_font(&c, to_mkfont("Arial", -12 - i, FW_NORMAL, 0, DEFAULT_QUALITY), 1);
        SIZE s;
        U(TextOutW)(c.dc, 0, 0, L"leak", 4);
        U(GetTextExtentPoint32W)(c.dc, L"leak", 4, &s);
        U(ExtTextOutA)(c.dc, 0, 0, ETO_OPAQUE, &(RECT){ 0, 0, 5, 5 }, "leak", 4, NULL);
        to_close(&c);
    }
    CHECK(to_gdi_count() == before);
}
