#include "to.h"
/* Exact rendering on DIB-section memory DCs using non-antialiased fixed bitmap fonts. */

#define NEED_R() do { NEED_TEXT(); REQUIRE(U(SetTextAlign)); REQUIRE(U(GetTextAlign)); } while (0)
#define W_ 200
#define H_ 80
static unsigned snap[4][W_ * H_];

static int rctx(TCtx *c, TEXTMETRICA *tm) { return to_fixed_ctx(c, tm, SYSTEM_FIXED_FONT, W_, H_); }
static void snapshot(const TCtx *c, int slot) {
    for (int y = 0; y < c->h; y++) for (int x = 0; x < c->w; x++) snap[slot][y * c->w + x] = to_px(c, x, y);
}
static int snapdiff(const TCtx *c, int slot) {
    int d = 0;
    for (int y = 0; y < c->h; y++) for (int x = 0; x < c->w; x++) if (snap[slot][y * c->w + x] != to_px(c, x, y)) d++;
    return d;
}
/* only pixels of the listed colors (or white) are present */
static int off_palette(const TCtx *c, unsigned a, unsigned b) {
    int n = 0;
    for (int y = 0; y < c->h; y++) for (int x = 0; x < c->w; x++) { unsigned p = to_px(c, x, y); if (p != a && p != b) n++; }
    return n;
}

/* ---- color semantics ---- */
static void color_case(COLORREF cr) {
    NEED_R();
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetTextColor)(c.dc, cr);
    CHECK(U(TextOutA)(c.dc, 10, 10, "Color Test", 10));
    unsigned want = to_cref2px(cr);
    RECT ink; int n = to_ink(&c, 0xffffff, &ink);
    if (cr == RGB(255, 255, 255)) { CHECK(n == 0); }
    else {
        CHECK(n > 0);
        CHECKF(to_count(&c, want, 0, 0, W_, H_) == n, "color %06lx: %d of %d pixels exact", cr, to_count(&c, want, 0, 0, W_, H_), n);
    }
    to_close(&c);
}
#define COLOR_TEST(nm, cr) TEST(text_output_render_color_##nm) { color_case(cr); }
COLOR_TEST(black, RGB(0, 0, 0))
COLOR_TEST(red, RGB(255, 0, 0))
COLOR_TEST(green, RGB(0, 255, 0))
COLOR_TEST(blue, RGB(0, 0, 255))
COLOR_TEST(teal, RGB(0, 128, 128))
COLOR_TEST(odd, RGB(17, 99, 201))
COLOR_TEST(white_invisible, RGB(255, 255, 255))
COLOR_TEST(gray, RGB(128, 128, 128))

TEST(text_output_render_ink_stays_in_cell_box) {
    NEED_R();
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetTextColor)(c.dc, 0);
    static const char *strs[] = { "H", "Hello", "gjpqy_[]", "MWMWMW", "0123456789", "@#$%^&*()" };
    for (int i = 0; i < ARRAYSZ(strs); i++) {
        to_clear(&c);
        int n = (int)strlen(strs[i]);
        CHECK(U(TextOutA)(c.dc, 20, 20, strs[i], n));
        RECT ink; int cnt = to_ink(&c, 0xffffff, &ink);
        CHECKF(cnt > 0, "%s: no ink", strs[i]);
        CHECKF(ink.left >= 20 && ink.right < 20 + n * tm.tmAveCharWidth && ink.top >= 20 && ink.bottom < 20 + tm.tmHeight,
               "%s: ink %ld,%ld..%ld,%ld outside cell", strs[i], ink.left, ink.top, ink.right, ink.bottom);
    }
    to_close(&c);
}
TEST(text_output_render_opaque_fills_extent_exactly) {
    NEED_R();
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetBkMode)(c.dc, OPAQUE);
    U(SetBkColor)(c.dc, RGB(255, 0, 0));
    U(SetTextColor)(c.dc, RGB(0, 0, 255));
    CHECK(U(TextOutA)(c.dc, 10, 12, "Hello", 5));
    int w = 5 * tm.tmAveCharWidth, h = tm.tmHeight;
    RECT ink; int n = to_ink(&c, 0xffffff, &ink);
    CHECKF(n == w * h, "nonwhite %d want %d", n, w * h);
    CHECKF(ink.left == 10 && ink.top == 12 && ink.right == 10 + w - 1 && ink.bottom == 12 + h - 1, "bbox %ld,%ld,%ld,%ld", ink.left, ink.top, ink.right, ink.bottom);
    CHECK(off_palette(&c, 0xff0000, 0x0000ff) == to_count(&c, 0xffffff, 0, 0, W_, H_));
    CHECK(to_count(&c, 0x0000ff, 0, 0, W_, H_) > 0);
    CHECK(to_count(&c, 0xff0000, 0, 0, W_, H_) > 0);
    to_close(&c);
}
TEST(text_output_render_opaque_w_matches_a) {
    NEED_R();
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetBkMode)(c.dc, OPAQUE); U(SetBkColor)(c.dc, RGB(255, 255, 0)); U(SetTextColor)(c.dc, RGB(0, 0, 128));
    CHECK(U(TextOutA)(c.dc, 7, 9, "Parity 123", 10));
    snapshot(&c, 0); to_clear(&c);
    CHECK(U(TextOutW)(c.dc, 7, 9, L"Parity 123", 10));
    CHECKF(snapdiff(&c, 0) == 0, "A/W differ");
    to_clear(&c);
    CHECK(U(ExtTextOutA)(c.dc, 7, 9, 0, NULL, "Parity 123", 10, NULL));
    CHECKF(snapdiff(&c, 0) == 0, "ExtTextOutA differs");
    to_clear(&c);
    CHECK(U(ExtTextOutW)(c.dc, 7, 9, 0, NULL, L"Parity 123", 10, NULL));
    CHECKF(snapdiff(&c, 0) == 0, "ExtTextOutW differs");
    to_close(&c);
}
TEST(text_output_render_transparent_leaves_background) {
    NEED_R();
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(PatBlt)(c.dc, 0, 0, W_, H_, BLACKNESS);
    U(SetBkMode)(c.dc, TRANSPARENT);
    U(SetBkColor)(c.dc, RGB(255, 0, 0));
    U(SetTextColor)(c.dc, RGB(0, 255, 0));
    CHECK(U(TextOutA)(c.dc, 10, 10, "Transp", 6));
    CHECK(to_count(&c, 0xff0000, 0, 0, W_, H_) == 0);
    CHECK(to_count(&c, 0x00ff00, 0, 0, W_, H_) > 0);
    CHECK(off_palette(&c, 0x000000, 0x00ff00) == 0);
    to_close(&c);
}
TEST(text_output_render_bk_mode_toggle) {
    NEED_R();
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetBkColor)(c.dc, RGB(255, 0, 0));
    CHECK(U(SetBkMode)(c.dc, OPAQUE) == TRANSPARENT);
    CHECK(U(TextOutA)(c.dc, 10, 10, "ab", 2));
    int red1 = to_count(&c, 0xff0000, 0, 0, W_, H_);
    CHECK(red1 > 0);
    to_clear(&c);
    CHECK(U(SetBkMode)(c.dc, TRANSPARENT) == OPAQUE);
    CHECK(U(TextOutA)(c.dc, 10, 10, "ab", 2));
    CHECK(to_count(&c, 0xff0000, 0, 0, W_, H_) == 0);
    to_close(&c);
}
TEST(text_output_render_setbkcolor_changes_fill) {
    NEED_R();
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetBkMode)(c.dc, OPAQUE);
    static const COLORREF col[] = { RGB(255, 0, 0), RGB(0, 255, 0), RGB(10, 20, 30), RGB(250, 250, 128) };
    for (int i = 0; i < 4; i++) {
        to_clear(&c);
        U(SetBkColor)(c.dc, col[i]);
        U(SetTextColor)(c.dc, i == 0 ? RGB(0, 0, 0) : RGB(255, 255, 255));
        CHECK(U(TextOutA)(c.dc, 5, 5, " ", 1));
        /* a space glyph has no ink: the cell is entirely background */
        CHECKF(to_count(&c, to_cref2px(col[i]), 5, 5, 5 + tm.tmAveCharWidth, 5 + tm.tmHeight) == tm.tmAveCharWidth * tm.tmHeight, "case %d", i);
    }
    to_close(&c);
}
TEST(text_output_render_color_return_values) {
    NEED_R();
    TCtx c; if (!to_open(&c, 16, 16, 32)) { CHECK(0); return; }
    CHECK(U(GetTextColor)(c.dc) == RGB(0, 0, 0));
    CHECK(U(GetBkColor)(c.dc) == RGB(255, 255, 255));
    CHECK(U(SetTextColor)(c.dc, RGB(1, 2, 3)) == RGB(0, 0, 0));
    CHECK(U(GetTextColor)(c.dc) == RGB(1, 2, 3));
    CHECK(U(SetTextColor)(c.dc, RGB(9, 8, 7)) == RGB(1, 2, 3));
    CHECK(U(SetBkColor)(c.dc, RGB(4, 5, 6)) == RGB(255, 255, 255));
    CHECK(U(GetBkColor)(c.dc) == RGB(4, 5, 6));
    CHECK(U(SetBkColor)(c.dc, RGB(0, 0, 0)) == RGB(4, 5, 6));
    to_close(&c);
}
TEST(text_output_render_color_invalid_dc) {
    NEED_R();
    CHECK(U(SetTextColor)(NULL, RGB(1, 2, 3)) == CLR_INVALID);
    CHECK(U(GetTextColor)(NULL) == CLR_INVALID);
    CHECK(U(SetBkColor)(NULL, RGB(1, 2, 3)) == CLR_INVALID);
    CHECK(U(GetBkColor)(NULL) == CLR_INVALID);
    CHECK(U(SetTextColor)((HDC)(ULONG_PTR)0x1234, 0) == CLR_INVALID);
    CHECK(U(SetBkColor)((HDC)(ULONG_PTR)0x1234, 0) == CLR_INVALID);
    CHECK(U(SetBkMode)(NULL, OPAQUE) == 0);
    CHECK(U(GetBkMode)(NULL) == 0);
}
TEST(text_output_render_bkmode_invalid_values) {
    NEED_R();
    TCtx c; if (!to_open(&c, 16, 16, 32)) { CHECK(0); return; }
    CHECK(U(GetBkMode)(c.dc) == TRANSPARENT); /* to_open selected TRANSPARENT */
    static const int bad[] = { 0, 3, 4, -1, 100, 0x10000 };
    for (int i = 0; i < ARRAYSZ(bad); i++) {
        SetLastError(0);
        int r = U(SetBkMode)(c.dc, bad[i]);
        CHECKF(r == 0, "SetBkMode(%d)=%d", bad[i], r);
        CHECKF(U(GetBkMode)(c.dc) == TRANSPARENT, "mode after %d = %d", bad[i], U(GetBkMode)(c.dc));
    }
    to_close(&c);
}
TEST(text_output_render_per_depth_identical_mask) {
    NEED_R();
    int depth[3] = { 32, 24, 8 };
    static unsigned mask[3][W_ * H_];
    for (int d = 0; d < 3; d++) {
        TCtx c; TEXTMETRICA tm;
        if (!to_open(&c, W_, H_, depth[d])) { CHECK(0); return; }
        to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FIXED_FONT), 0);
        if (!to_fixed(c.dc, &tm) && !to_wine()) { t_skip("no fixed font"); to_close(&c); return; }
        U(SetTextColor)(c.dc, RGB(0, 0, 0));
        CHECK(U(TextOutA)(c.dc, 5, 5, "Depth: mask", 11));
        unsigned wh = to_white(&c);
        for (int y = 0; y < H_; y++) for (int x = 0; x < W_; x++) mask[d][y * W_ + x] = to_px(&c, x, y) != wh;
        to_close(&c);
    }
    for (int d = 1; d < 3; d++) {
        int diff = 0; for (int i = 0; i < W_ * H_; i++) diff += mask[0][i] != mask[d][i];
        CHECKF(diff == 0, "depth %d differs from 32bpp in %d px", depth[d], diff);
    }
}
TEST(text_output_render_mono_1bpp_mask) {
    NEED_R();
    TCtx c32, c1; TEXTMETRICA tm;
    if (!to_fixed_ctx(&c32, &tm, SYSTEM_FIXED_FONT, W_, H_)) return;
    U(SetTextColor)(c32.dc, 0);
    CHECK(U(TextOutA)(c32.dc, 5, 5, "Mono 1bpp", 9));
    if (!to_open(&c1, W_, H_, 1)) { to_close(&c32); CHECK(0); return; }
    to_use_font(&c1, (HFONT)U(GetStockObject)(SYSTEM_FIXED_FONT), 0);
    if (to_wine()) to_use_font(&c1, to_mkfont("Courier New", -16, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    U(SetTextColor)(c1.dc, RGB(0, 0, 0));
    CHECK(U(TextOutA)(c1.dc, 5, 5, "Mono 1bpp", 9));
    int diff = 0;
    for (int y = 0; y < H_; y++) for (int x = 0; x < W_; x++) diff += (to_px(&c32, x, y) != 0xffffff) != (to_px(&c1, x, y) == 0);
    if (to_wine()) t_note("1bpp mask differs in %d px (wine substitutes a TrueType face)", diff);
    else CHECKF(diff == 0, "1bpp mask differs in %d px", diff);
    CHECK(to_count(&c1, 0, 0, 0, W_, H_) > 0);
    to_close(&c1); to_close(&c32);
}
TEST(text_output_render_mono_white_text_on_black) {
    NEED_R();
    TCtx c; TEXTMETRICA tm; if (!to_open(&c, W_, H_, 1)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FIXED_FONT), 0);
    U(PatBlt)(c.dc, 0, 0, W_, H_, BLACKNESS);
    CHECK(to_count(&c, 0, 0, 0, W_, H_) == W_ * H_);
    U(SetTextColor)(c.dc, RGB(255, 255, 255));
    CHECK(U(TextOutA)(c.dc, 5, 5, "White", 5));
    CHECK(to_count(&c, 1, 0, 0, W_, H_) > 0);
    to_close(&c);
}
TEST(text_output_render_mono_opaque_colors) {
    NEED_R();
    TCtx c; TEXTMETRICA tm; if (!to_open(&c, W_, H_, 1)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FIXED_FONT), 0);
    U(GetTextMetricsA)(c.dc, &tm);
    U(SetBkMode)(c.dc, OPAQUE);
    U(SetBkColor)(c.dc, RGB(0, 0, 0));
    U(SetTextColor)(c.dc, RGB(255, 255, 255));
    CHECK(U(TextOutA)(c.dc, 8, 8, "   ", 3));
    CHECKF(to_count(&c, 0, 8, 8, 8 + 3 * tm.tmAveCharWidth, 8 + tm.tmHeight) == 3 * tm.tmAveCharWidth * tm.tmHeight, "black bk fill");
    CHECK(to_count(&c, 1, 8, 8, 8 + 3 * tm.tmAveCharWidth, 8 + tm.tmHeight) == 0);
    to_close(&c);
}
TEST(text_output_render_mono_text_color_nonmono) {
    NEED_R();
    TCtx c; if (!to_open(&c, W_, H_, 1)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FIXED_FONT), 0);
    U(SetTextColor)(c.dc, RGB(255, 0, 0));
    CHECK(U(TextOutA)(c.dc, 8, 8, "Red", 3));
    t_note("red text on 1bpp: black=%d white=%d", to_count(&c, 0, 0, 0, W_, H_), to_count(&c, 1, 0, 0, W_, H_));
    CHECK(U(GetTextColor)(c.dc) == RGB(255, 0, 0));
    to_close(&c);
}

/* ---- alignment ---- */
static void align_case(UINT h, UINT v) {
    NEED_R();
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetBkMode)(c.dc, OPAQUE); U(SetBkColor)(c.dc, RGB(0, 200, 0)); U(SetTextColor)(c.dc, RGB(0, 0, 0));
    CHECK(U(SetTextAlign)(c.dc, h | v) == TA_LEFT);
    CHECK(U(GetTextAlign)(c.dc) == (h | v));
    int n = 6, w = n * tm.tmAveCharWidth, ht = tm.tmHeight, x = 100, y = 40;
    CHECK(U(TextOutA)(c.dc, x, y, "Aligns", n));
    RECT ink; int cnt = to_ink(&c, 0xffffff, &ink);
    int left = h == TA_LEFT ? x : h == TA_RIGHT ? x - w : x - w / 2;
    int top = v == TA_TOP ? y : v == TA_BOTTOM ? y - ht : y - tm.tmAscent;
    CHECKF(cnt == w * ht, "area %d want %d", cnt, w * ht);
    CHECKF(ink.left == left && ink.right == left + w - 1, "x %ld..%ld want %d..%d", ink.left, ink.right, left, left + w - 1);
    CHECKF(ink.top == top && ink.bottom == top + ht - 1, "y %ld..%ld want %d..%d", ink.top, ink.bottom, top, top + ht - 1);
    to_close(&c);
}
#define ALIGN_TEST(nm, h, v) TEST(text_output_render_align_##nm) { align_case(h, v); }
ALIGN_TEST(left_top, TA_LEFT, TA_TOP)
ALIGN_TEST(left_bottom, TA_LEFT, TA_BOTTOM)
ALIGN_TEST(left_baseline, TA_LEFT, TA_BASELINE)
ALIGN_TEST(right_top, TA_RIGHT, TA_TOP)
ALIGN_TEST(right_bottom, TA_RIGHT, TA_BOTTOM)
ALIGN_TEST(right_baseline, TA_RIGHT, TA_BASELINE)
ALIGN_TEST(center_top, TA_CENTER, TA_TOP)
ALIGN_TEST(center_bottom, TA_CENTER, TA_BOTTOM)
ALIGN_TEST(center_baseline, TA_CENTER, TA_BASELINE)

TEST(text_output_render_align_updatecp_ignores_xy) {
    NEED_R(); REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetBkMode)(c.dc, OPAQUE); U(SetBkColor)(c.dc, RGB(255, 0, 0));
    U(SetTextAlign)(c.dc, TA_UPDATECP);
    U(MoveToEx)(c.dc, 30, 20, NULL);
    CHECK(U(TextOutA)(c.dc, 999, 999, "abc", 3));
    RECT ink; int n = to_ink(&c, 0xffffff, &ink);
    int w = 3 * tm.tmAveCharWidth;
    CHECKF(ink.left == 30 && ink.top == 20 && ink.right == 30 + w - 1, "bbox %ld,%ld,%ld,%ld", ink.left, ink.top, ink.right, ink.bottom);
    POINT p; CHECK(U(GetCurrentPositionEx)(c.dc, &p));
    CHECKF(p.x == 30 + w && p.y == 20, "cp %ld,%ld want %d,20", p.x, p.y, 30 + w);
    CHECK(U(TextOutW)(c.dc, -50, -50, L"de", 2));
    CHECK(U(GetCurrentPositionEx)(c.dc, &p));
    CHECKF(p.x == 30 + w + 2 * tm.tmAveCharWidth && p.y == 20, "cp2 %ld,%ld", p.x, p.y);
    to_close(&c);
}
TEST(text_output_render_cp_unchanged_without_updatecp) {
    NEED_R(); REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(MoveToEx)(c.dc, 3, 4, NULL);
    CHECK(U(TextOutA)(c.dc, 50, 50, "abc", 3));
    CHECK(U(ExtTextOutW)(c.dc, 50, 50, 0, NULL, L"abc", 3, NULL));
    POINT p; CHECK(U(GetCurrentPositionEx)(c.dc, &p));
    CHECK(p.x == 3 && p.y == 4);
    to_close(&c);
}
TEST(text_output_render_updatecp_exttextout_dx_advance) {
    NEED_R(); REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetTextAlign)(c.dc, TA_UPDATECP);
    U(MoveToEx)(c.dc, 10, 10, NULL);
    int dx[3] = { 20, 30, 7 };
    CHECK(U(ExtTextOutA)(c.dc, 0, 0, 0, NULL, "abc", 3, dx));
    POINT p; CHECK(U(GetCurrentPositionEx)(c.dc, &p));
    CHECKF(p.x == 10 + 57 && p.y == 10, "cp %ld,%ld want 67,10", p.x, p.y);
    to_close(&c);
}
TEST(text_output_render_updatecp_char_extra_advance) {
    NEED_R(); REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx)); REQUIRE(U(SetTextCharacterExtra));
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetTextAlign)(c.dc, TA_UPDATECP);
    U(SetTextCharacterExtra)(c.dc, 5);
    U(MoveToEx)(c.dc, 10, 10, NULL);
    CHECK(U(TextOutA)(c.dc, 0, 0, "abcd", 4));
    POINT p; CHECK(U(GetCurrentPositionEx)(c.dc, &p));
    CHECKF(p.x == 10 + 4 * (tm.tmAveCharWidth + 5), "cp %ld", p.x);
    U(SetTextCharacterExtra)(c.dc, 0);
    to_close(&c);
}
TEST(text_output_render_updatecp_empty_string_no_move) {
    NEED_R(); REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetTextAlign)(c.dc, TA_UPDATECP);
    U(MoveToEx)(c.dc, 10, 10, NULL);
    CHECK(U(TextOutA)(c.dc, 0, 0, "abc", 0));
    CHECK(U(TextOutW)(c.dc, 0, 0, L"abc", 0));
    POINT p; CHECK(U(GetCurrentPositionEx)(c.dc, &p));
    CHECK(p.x == 10 && p.y == 10);
    to_close(&c);
}
TEST(text_output_render_updatecp_sequential_composition) {
    NEED_R(); REQUIRE(U(MoveToEx));
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetTextColor)(c.dc, 0);
    CHECK(U(TextOutA)(c.dc, 10, 10, "abcdef", 6));
    snapshot(&c, 0); to_clear(&c);
    U(SetTextAlign)(c.dc, TA_UPDATECP);
    U(MoveToEx)(c.dc, 10, 10, NULL);
    CHECK(U(TextOutA)(c.dc, 0, 0, "ab", 2));
    CHECK(U(TextOutA)(c.dc, 0, 0, "cd", 2));
    CHECK(U(TextOutW)(c.dc, 0, 0, L"ef", 2));
    CHECKF(snapdiff(&c, 0) == 0, "split text differs from single draw");
    to_close(&c);
}
TEST(text_output_render_align_right_updatecp) {
    NEED_R(); REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetBkMode)(c.dc, OPAQUE); U(SetBkColor)(c.dc, RGB(255, 0, 0));
    U(SetTextAlign)(c.dc, TA_UPDATECP | TA_RIGHT);
    U(MoveToEx)(c.dc, 100, 10, NULL);
    CHECK(U(TextOutA)(c.dc, 0, 0, "abc", 3));
    RECT ink; to_ink(&c, 0xffffff, &ink);
    int w = 3 * tm.tmAveCharWidth;
    CHECKF(ink.right == 99 && ink.left == 100 - w, "ink %ld..%ld", ink.left, ink.right);
    POINT p; U(GetCurrentPositionEx)(c.dc, &p);
    t_note("TA_UPDATECP|TA_RIGHT cp after = %ld (start 100, w %d)", p.x, w);
    to_close(&c);
}
TEST(text_output_render_set_text_align_values) {
    NEED_R();
    TCtx c; if (!to_open(&c, 16, 16, 32)) { CHECK(0); return; }
    CHECK(U(GetTextAlign)(c.dc) == (TA_LEFT | TA_TOP | TA_NOUPDATECP));
    static const UINT ok[] = { TA_LEFT, TA_RIGHT, TA_CENTER, TA_TOP, TA_BOTTOM, TA_BASELINE, TA_UPDATECP, TA_RTLREADING,
        TA_RIGHT | TA_BOTTOM | TA_UPDATECP, TA_CENTER | TA_BASELINE | TA_RTLREADING };
    UINT prev = TA_LEFT;
    for (int i = 0; i < ARRAYSZ(ok); i++) {
        UINT r = U(SetTextAlign)(c.dc, ok[i]);
        CHECKF(r == prev, "prev 0x%x got 0x%x", prev, r);
        CHECKF(U(GetTextAlign)(c.dc) == ok[i], "get 0x%x -> 0x%x", ok[i], U(GetTextAlign)(c.dc));
        prev = ok[i];
    }
    to_close(&c);
}
TEST(text_output_render_set_text_align_invalid) {
    NEED_R();
    CHECK(U(SetTextAlign)(NULL, TA_LEFT) == GDI_ERROR);
    CHECK(U(GetTextAlign)(NULL) == GDI_ERROR);
    CHECK(U(SetTextAlign)((HDC)(ULONG_PTR)0x1234, TA_LEFT) == GDI_ERROR);
    TCtx c; if (!to_open(&c, 16, 16, 32)) { CHECK(0); return; }
    SetLastError(0);
    UINT r = U(SetTextAlign)(c.dc, 0xffffffff);
    t_note("SetTextAlign(0xffffffff) -> 0x%x, now 0x%x", r, U(GetTextAlign)(c.dc));
    U(SetTextAlign)(c.dc, 0x10000);
    t_note("SetTextAlign(0x10000) -> now 0x%x", U(GetTextAlign)(c.dc));
    to_close(&c);
}

/* ---- clipping ---- */
TEST(text_output_render_eto_clipped_confines_ink) {
    NEED_R();
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetTextColor)(c.dc, 0);
    RECT r = { 14, 12, 14 + 2 * tm.tmAveCharWidth + 1, 12 + tm.tmHeight - 3 };
    CHECK(U(ExtTextOutA)(c.dc, 10, 10, ETO_CLIPPED, &r, "MMMMMM", 6, NULL));
    RECT ink; int n = to_ink(&c, 0xffffff, &ink);
    CHECK(n > 0);
    CHECKF(ink.left >= r.left && ink.top >= r.top && ink.right < r.right && ink.bottom < r.bottom, "ink %ld,%ld..%ld,%ld rect %ld,%ld..%ld,%ld", ink.left, ink.top, ink.right, ink.bottom, r.left, r.top, r.right, r.bottom);
    /* unclipped version produces strictly more ink */
    snapshot(&c, 0); int clipped = n; to_clear(&c);
    CHECK(U(ExtTextOutA)(c.dc, 10, 10, 0, NULL, "MMMMMM", 6, NULL));
    CHECK(to_ink(&c, 0xffffff, &ink) > clipped);
    to_close(&c);
}
TEST(text_output_render_eto_clipped_is_subset_of_unclipped) {
    NEED_R();
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetTextColor)(c.dc, 0);
    CHECK(U(ExtTextOutA)(c.dc, 10, 10, 0, NULL, "Subset", 6, NULL));
    snapshot(&c, 0); to_clear(&c);
    RECT r = { 20, 10, 40, 22 };
    CHECK(U(ExtTextOutA)(c.dc, 10, 10, ETO_CLIPPED, &r, "Subset", 6, NULL));
    for (int y = 0; y < H_; y++) for (int x = 0; x < W_; x++) {
        unsigned p = to_px(&c, x, y), full = snap[0][y * W_ + x];
        int inr = x >= r.left && x < r.right && y >= r.top && y < r.bottom;
        CHECKF(p == (inr ? full : 0xffffff), "(%d,%d) p=%06x full=%06x inr=%d", x, y, p, full, inr);
        if (p != (inr ? full : 0xffffff)) { to_close(&c); return; }
    }
    to_close(&c);
}
TEST(text_output_render_eto_opaque_rect_and_text) {
    NEED_R();
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetBkColor)(c.dc, RGB(255, 0, 0)); U(SetTextColor)(c.dc, RGB(0, 0, 255));
    RECT r = { 20, 20, 120, 60 };
    CHECK(U(ExtTextOutA)(c.dc, 30, 30, ETO_OPAQUE, &r, "Rect", 4, NULL));
    RECT ink; int n = to_ink(&c, 0xffffff, &ink);
    CHECKF(n == 100 * 40, "area %d", n);
    CHECKF(ink.left == 20 && ink.top == 20 && ink.right == 119 && ink.bottom == 59, "bbox");
    CHECK(to_count(&c, 0x0000ff, 0, 0, W_, H_) > 0);
    CHECK(to_px(&c, 20, 20) == 0xff0000);
    to_close(&c);
}
TEST(text_output_render_eto_opaque_text_larger_than_rect) {
    NEED_R();
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetBkColor)(c.dc, RGB(255, 0, 0)); U(SetTextColor)(c.dc, RGB(0, 0, 255));
    RECT r = { 30, 20, 40, 30 };
    CHECK(U(ExtTextOutA)(c.dc, 20, 20, ETO_OPAQUE, &r, "Larger", 6, NULL));
    /* the rect is filled and the text is also painted; its own background cell is not (rect only) */
    RECT ink; to_ink(&c, 0xffffff, &ink);
    CHECK(to_px(&c, 35, 25) != 0xffffff);
    CHECK(to_count(&c, 0xff0000, 0, 0, W_, H_) >= 1);
    t_note("opaque rect smaller than text: ink bbox %ld,%ld..%ld,%ld", ink.left, ink.top, ink.right, ink.bottom);
    to_close(&c);
}
TEST(text_output_render_eto_opaque_clipped_combined) {
    NEED_R();
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetBkColor)(c.dc, RGB(255, 0, 0)); U(SetTextColor)(c.dc, RGB(0, 0, 255));
    RECT r = { 12, 12, 12 + 3 * tm.tmAveCharWidth, 12 + tm.tmHeight };
    CHECK(U(ExtTextOutA)(c.dc, 10, 10, ETO_OPAQUE | ETO_CLIPPED, &r, "WWWWWWWW", 8, NULL));
    RECT ink; int n = to_ink(&c, 0xffffff, &ink);
    CHECKF(ink.left == r.left && ink.top == r.top && ink.right == r.right - 1 && ink.bottom == r.bottom - 1, "bbox %ld,%ld,%ld,%ld", ink.left, ink.top, ink.right, ink.bottom);
    CHECK(n == (r.right - r.left) * (r.bottom - r.top));
    to_close(&c);
}
TEST(text_output_render_select_clip_rgn_clips_text) {
    NEED_R(); REQUIRE(U(CreateRectRgn)); REQUIRE(U(SelectClipRgn));
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetTextColor)(c.dc, 0);
    CHECK(U(TextOutA)(c.dc, 10, 10, "ClipRgn", 7));
    snapshot(&c, 0); to_clear(&c);
    HRGN rg = U(CreateRectRgn)(18, 0, 34, 80);
    U(SelectClipRgn)(c.dc, rg);
    CHECK(U(TextOutA)(c.dc, 10, 10, "ClipRgn", 7));
    int bad = 0;
    for (int y = 0; y < H_; y++) for (int x = 0; x < W_; x++) {
        unsigned want = (x >= 18 && x < 34) ? snap[0][y * W_ + x] : 0xffffff;
        bad += to_px(&c, x, y) != want;
    }
    CHECKF(bad == 0, "%d wrong px", bad);
    U(SelectClipRgn)(c.dc, NULL);
    U(DeleteObject)(rg);
    to_close(&c);
}
TEST(text_output_render_clip_empty_region_no_ink) {
    NEED_R(); REQUIRE(U(CreateRectRgn)); REQUIRE(U(SelectClipRgn));
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    HRGN rg = U(CreateRectRgn)(0, 0, 0, 0);
    U(SelectClipRgn)(c.dc, rg);
    U(SetBkMode)(c.dc, OPAQUE); U(SetBkColor)(c.dc, 0);
    BOOL b = U(TextOutA)(c.dc, 10, 10, "Hidden", 6);
    t_note("TextOut with empty clip -> %d", b);
    RECT ink; CHECK(to_ink(&c, 0xffffff, &ink) == 0);
    U(SelectClipRgn)(c.dc, NULL); U(DeleteObject)(rg);
    to_close(&c);
}
TEST(text_output_render_offscreen_positions_ok) {
    NEED_R();
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    static const POINT p[] = { {-1000, 10}, {10, -1000}, {5000, 5}, {5, 5000}, {-30, -30}, {W_ - 3, H_ - 3}, {0x7fffff00, 0}, {(int)0x80000100, 0} };
    for (int i = 0; i < ARRAYSZ(p); i++) {
        BOOL r = U(TextOutA)(c.dc, p[i].x, p[i].y, "Off", 3);
        t_note("TextOut at (%d,%d) -> %d", p[i].x, p[i].y, r);
    }
    to_close(&c);
}
TEST(text_output_render_partial_left_edge_pixels) {
    NEED_R();
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetTextColor)(c.dc, 0);
    int dx = tm.tmAveCharWidth;
    CHECK(U(TextOutA)(c.dc, 40, 10, "Edge", 4));
    snapshot(&c, 0); to_clear(&c);
    CHECK(U(TextOutA)(c.dc, 40 - dx, 10, "Edge", 4));
    int bad = 0;
    for (int y = 0; y < H_; y++) for (int x = 0; x < W_ - dx; x++) bad += to_px(&c, x, y) != snap[0][y * W_ + x + dx];
    CHECKF(bad == 0, "translation by one cell differs in %d px", bad);
    to_close(&c);
}
/* ---- lpDx / char extra / justification pixel placement ---- */
static void placed(TCtx *c, const char *s, const int *off, int n, int x0, int y) {
    for (int i = 0; i < n; i++) U(TextOutA)(c->dc, x0 + off[i], y, s + i, 1);
}
TEST(text_output_render_dx_array_places_cells) {
    NEED_R();
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetTextColor)(c.dc, 0);
    int dx[5] = { 20, 11, 30, 9, 15 };
    CHECK(U(ExtTextOutA)(c.dc, 8, 10, 0, NULL, "Hello", 5, dx));
    snapshot(&c, 0); to_clear(&c);
    int off[5] = { 0, 20, 31, 61, 70 };
    placed(&c, "Hello", off, 5, 8, 10);
    CHECKF(snapdiff(&c, 0) == 0, "dx placement differs");
    to_clear(&c);
    CHECK(U(ExtTextOutW)(c.dc, 8, 10, 0, NULL, L"Hello", 5, dx));
    CHECKF(snapdiff(&c, 0) == 0, "W dx placement differs");
    to_close(&c);
}
TEST(text_output_render_dx_negative_values) {
    NEED_R();
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetTextColor)(c.dc, 0);
    int dx[4] = { -3, 25, -5, 10 };
    CHECK(U(ExtTextOutA)(c.dc, 60, 10, 0, NULL, "wxyz", 4, dx));
    snapshot(&c, 0); to_clear(&c);
    int off[4] = { 0, -3, 22, 17 };
    placed(&c, "wxyz", off, 4, 60, 10);
    CHECKF(snapdiff(&c, 0) == 0, "negative dx placement differs");
    to_close(&c);
}
TEST(text_output_render_dx_zero_overstrikes) {
    NEED_R();
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetTextColor)(c.dc, 0);
    int dx[3] = { 0, 0, 0 };
    CHECK(U(ExtTextOutA)(c.dc, 20, 10, 0, NULL, "ABC", 3, dx));
    snapshot(&c, 0); to_clear(&c);
    int off[3] = { 0, 0, 0 };
    placed(&c, "ABC", off, 3, 20, 10);
    CHECKF(snapdiff(&c, 0) == 0, "zero dx");
    to_close(&c);
}
TEST(text_output_render_dx_equal_to_natural_matches_nodx) {
    NEED_R();
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetTextColor)(c.dc, 0);
    U(SetBkMode)(c.dc, OPAQUE); U(SetBkColor)(c.dc, RGB(200, 200, 255));
    int dx[6]; for (int i = 0; i < 6; i++) dx[i] = tm.tmAveCharWidth;
    CHECK(U(ExtTextOutA)(c.dc, 8, 10, 0, NULL, "Natura", 6, NULL));
    snapshot(&c, 0); to_clear(&c);
    CHECK(U(ExtTextOutA)(c.dc, 8, 10, 0, NULL, "Natura", 6, dx));
    CHECKF(snapdiff(&c, 0) == 0, "natural dx differs");
    to_close(&c);
}
TEST(text_output_render_dx_opaque_covers_dx_span) {
    NEED_R();
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetBkMode)(c.dc, OPAQUE); U(SetBkColor)(c.dc, RGB(255, 0, 0));
    int dx[3] = { 20, 20, 20 };
    CHECK(U(ExtTextOutA)(c.dc, 8, 10, 0, NULL, "   ", 3, dx));
    RECT ink; int n = to_ink(&c, 0xffffff, &ink);
    t_note("opaque bk with dx 20,20,20: ink bbox %ld..%ld (%d px)", ink.left, ink.right, n);
    CHECK(ink.left == 8);
    CHECK(ink.right >= 8 + 40 + tm.tmAveCharWidth - 1);
    to_close(&c);
}
TEST(text_output_render_char_extra_places_cells) {
    NEED_R(); REQUIRE(U(SetTextCharacterExtra));
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetTextColor)(c.dc, 0);
    int ex = 6;
    U(SetTextCharacterExtra)(c.dc, ex);
    CHECK(U(TextOutA)(c.dc, 8, 10, "Spread", 6));
    U(SetTextCharacterExtra)(c.dc, 0);
    snapshot(&c, 0); to_clear(&c);
    int off[6]; for (int i = 0; i < 6; i++) off[i] = i * (tm.tmAveCharWidth + ex);
    placed(&c, "Spread", off, 6, 8, 10);
    CHECKF(snapdiff(&c, 0) == 0, "char extra placement differs");
    to_close(&c);
}
TEST(text_output_render_char_extra_negative) {
    NEED_R(); REQUIRE(U(SetTextCharacterExtra));
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetTextColor)(c.dc, 0);
    U(SetTextCharacterExtra)(c.dc, -2);
    CHECK(U(TextOutA)(c.dc, 60, 10, "Tight", 5));
    U(SetTextCharacterExtra)(c.dc, 0);
    snapshot(&c, 0); to_clear(&c);
    int off[5]; for (int i = 0; i < 5; i++) off[i] = i * (tm.tmAveCharWidth - 2);
    placed(&c, "Tight", off, 5, 60, 10);
    CHECKF(snapdiff(&c, 0) == 0, "negative extra");
    to_close(&c);
}
TEST(text_output_render_char_extra_opaque_box_width) {
    NEED_R(); REQUIRE(U(SetTextCharacterExtra));
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetBkMode)(c.dc, OPAQUE); U(SetBkColor)(c.dc, RGB(255, 0, 0));
    U(SetTextCharacterExtra)(c.dc, 3);
    CHECK(U(TextOutA)(c.dc, 8, 10, "     ", 5));
    RECT ink; int n = to_ink(&c, 0xffffff, &ink);
    int w = 5 * (tm.tmAveCharWidth + 3);
    t_note("opaque box with char extra: %ld..%ld (expect width %d, area %d)", ink.left, ink.right, w, n);
    CHECK(ink.left == 8 && ink.right == 8 + w - 1 && n == w * tm.tmHeight);
    U(SetTextCharacterExtra)(c.dc, 0);
    to_close(&c);
}
TEST(text_output_render_justification_places_after_break) {
    NEED_R(); REQUIRE(U(SetTextJustification));
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetTextColor)(c.dc, 0);
    CHECK(U(SetTextJustification)(c.dc, 10, 1));
    CHECK(U(TextOutA)(c.dc, 8, 10, "a b", 3));
    CHECK(U(SetTextJustification)(c.dc, 0, 0));
    snapshot(&c, 0); to_clear(&c);
    int aw = tm.tmAveCharWidth;
    CHECK(U(TextOutA)(c.dc, 8, 10, "a", 1));
    CHECK(U(TextOutA)(c.dc, 8 + 2 * aw + 10, 10, "b", 1));
    CHECKF(snapdiff(&c, 0) == 0, "justified placement differs");
    to_close(&c);
}
TEST(text_output_render_justification_reset_on_zero_breaks) {
    NEED_R(); REQUIRE(U(SetTextJustification));
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetTextColor)(c.dc, 0);
    CHECK(U(TextOutA)(c.dc, 8, 10, "a b c", 5));
    snapshot(&c, 0); to_clear(&c);
    CHECK(U(SetTextJustification)(c.dc, 30, 2));
    CHECK(U(SetTextJustification)(c.dc, 0, 0));
    CHECK(U(TextOutA)(c.dc, 8, 10, "a b c", 5));
    CHECKF(snapdiff(&c, 0) == 0, "justification not reset");
    to_close(&c);
}
TEST(text_output_render_justification_advances_cp) {
    NEED_R(); REQUIRE(U(SetTextJustification)); REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    TCtx c; TEXTMETRICA tm; if (!rctx(&c, &tm)) return;
    U(SetTextAlign)(c.dc, TA_UPDATECP);
    U(MoveToEx)(c.dc, 10, 10, NULL);
    CHECK(U(SetTextJustification)(c.dc, 12, 2));
    CHECK(U(TextOutA)(c.dc, 0, 0, "a b c", 5));
    POINT p; U(GetCurrentPositionEx)(c.dc, &p);
    CHECKF(p.x == 10 + 5 * tm.tmAveCharWidth + 12, "cp %ld", p.x);
    U(SetTextJustification)(c.dc, 0, 0);
    to_close(&c);
}
