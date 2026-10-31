/* SetTextAlign / SetTextCharacterExtra / SetTextJustification / SetLayout / SetGraphicsMode / SetMapperFlags */
#include "dca.h"

static const COLORREF CBACK = RGB(0x20, 0x20, 0x20), CTXT = RGB(255, 255, 255), CBK = RGB(200, 30, 30);

/* ---------- constants ---------- */
TEST(dca_textalign_constants) {
    CHECK(TA_NOUPDATECP == 0 && TA_UPDATECP == 1 && TA_LEFT == 0 && TA_RIGHT == 2 && TA_CENTER == 6);
    CHECK(TA_TOP == 0 && TA_BOTTOM == 8 && TA_BASELINE == 24 && TA_RTLREADING == 256);
    CHECK(VTA_BASELINE == TA_BASELINE && VTA_LEFT == TA_BOTTOM && VTA_RIGHT == TA_TOP && VTA_CENTER == TA_CENTER);
    CHECK(VTA_BOTTOM == TA_RIGHT && VTA_TOP == TA_LEFT);
    CHECK(GDI_ERROR == 0xFFFFFFFF);
    CHECK(LAYOUT_RTL == 1 && LAYOUT_BTT == 2 && LAYOUT_VBH == 4 && LAYOUT_ORIENTATIONMASK == 7 && LAYOUT_BITMAPORIENTATIONPRESERVED == 8);
    CHECK(GM_COMPATIBLE == 1 && GM_ADVANCED == 2);
    CHECK(MWT_IDENTITY == 1 && MWT_LEFTMULTIPLY == 2 && MWT_RIGHTMULTIPLY == 3);
    CHECK(AD_COUNTERCLOCKWISE == 1 && AD_CLOCKWISE == 2);
    CHECK(ALTERNATE == 1 && WINDING == 2 && POLYFILL_LAST == 2);
    CHECK(BLACKONWHITE == 1 && WHITEONBLACK == 2 && COLORONCOLOR == 3 && HALFTONE == 4 && STRETCH_ANDSCANS == 1 && STRETCH_ORSCANS == 2 && STRETCH_DELETESCANS == 3 && STRETCH_HALFTONE == 4);
    CHECK(ABSOLUTE == 1 && RELATIVE == 2);
}

/* ---------- TextAlign values ---------- */
TEST(dca_textalign_all_valid_combos) {
    REQUIRE(U(SetTextAlign)); REQUIRE(U(GetTextAlign));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    static const UINT h[] = { TA_LEFT, TA_RIGHT, TA_CENTER };
    static const UINT v[] = { TA_TOP, TA_BOTTOM, TA_BASELINE };
    static const UINT u[] = { TA_NOUPDATECP, TA_UPDATECP };
    static const UINT r[] = { 0, TA_RTLREADING };
    UINT last = 0; int bad = 0, n = 0;
    for (int a = 0; a < 3; a++) for (int b = 0; b < 3; b++) for (int c = 0; c < 2; c++) for (int d = 0; d < 2; d++) {
        UINT val = h[a] | v[b] | u[c] | r[d];
        UINT p = U(SetTextAlign)(m.dc, val);
        if (p != last) bad++;
        if (U(GetTextAlign)(m.dc) != val) bad++;
        last = val; n++;
    }
    CHECKF(bad == 0 && n == 36, "mismatches %d of %d", bad, n);
    mdc_free(&m);
}
TEST(dca_textalign_invalid_values_observed) {
    REQUIRE(U(SetTextAlign)); REQUIRE(U(GetTextAlign));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    static const UINT bad[] = { 0x20, 0x40, 0x10, 0x4, 0x7, 0xE | 0x10, 0x1F, 0xFF, 0x1000, 0xFFFF, 0x80000000, 0xFFFFFFFF, TA_CENTER | TA_RIGHT | TA_BASELINE | 0x20 };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        U(SetTextAlign)(m.dc, TA_RIGHT);
        SetLastError(0);
        UINT r = U(SetTextAlign)(m.dc, bad[i]);
        DWORD e = GetLastError();
        t_note("SetTextAlign(0x%x): ret=0x%x err=%lu GetTextAlign=0x%x", bad[i], r, e, U(GetTextAlign)(m.dc));
        U(SetTextAlign)(m.dc, TA_LEFT);
        CHECKF(U(GetTextAlign)(m.dc) == TA_LEFT, "recover after 0x%x", bad[i]);
    }
    mdc_free(&m);
}
TEST(dca_textalign_vta_aliases_roundtrip) {
    REQUIRE(U(SetTextAlign)); REQUIRE(U(GetTextAlign));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    static const UINT v[] = { VTA_BASELINE, VTA_LEFT, VTA_RIGHT, VTA_CENTER, VTA_BOTTOM, VTA_TOP };
    for (unsigned i = 0; i < 6; i++) {
        U(SetTextAlign)(m.dc, v[i]);
        CHECKF(U(GetTextAlign)(m.dc) == v[i], "i=%u", i);
    }
    mdc_free(&m);
}

/* ---------- TextAlign effect: exact opaque cell rectangle with a raster font ---------- */
typedef struct { int cx, cy, asc; RECT box; int n; } Cell;
static int draw_cell(MDC *m, UINT align, int x, int y, Cell *c) {
    HGDIOBJ of = U(SelectObject)(m->dc, U(GetStockObject)(SYSTEM_FIXED_FONT));
    U(SetBkMode)(m->dc, OPAQUE); U(SetBkColor)(m->dc, CBK); U(SetTextColor)(m->dc, CTXT);
    SIZE sz; TEXTMETRICA tm;
    if (!U(GetTextExtentPoint32A)(m->dc, "AB", 2, &sz) || !U(GetTextMetricsA)(m->dc, &tm)) { U(SelectObject)(m->dc, of); return 0; }
    c->cx = sz.cx; c->cy = sz.cy; c->asc = tm.tmAscent;
    mdc_clear(m, CBACK);
    U(SetTextAlign)(m->dc, align);
    BOOL ok = U(TextOutA)(m->dc, x, y, "AB", 2);
    U(SetTextAlign)(m->dc, 0);
    U(SelectObject)(m->dc, of);
    c->n = mdc_bbox(m, CBACK, &c->box);
    return ok;
}
static void align_case(UINT h, UINT v) {
    REQUIRE(U(SetTextAlign)); REQUIRE(U(TextOutA)); REQUIRE(U(GetTextExtentPoint32A)); REQUIRE(U(GetTextMetricsA)); REQUIRE(U(SetBkMode));
    MDC m; if (!mdc_make(&m, 120, 80)) { t_skip("mdc"); return; }
    Cell c; const int X = 60, Y = 40;
    CHECK(draw_cell(&m, h | v, X, Y, &c));
    int w = c.box.right - c.box.left + 1, ht = c.box.bottom - c.box.top + 1;
    CHECKF(w == c.cx && ht == c.cy, "cell %dx%d, extent %dx%d", w, ht, c.cx, c.cy);
    int el = h == TA_LEFT ? X : h == TA_RIGHT ? X - c.cx : X - c.cx / 2;
    int et = v == TA_TOP ? Y : v == TA_BOTTOM ? Y - c.cy : Y - c.asc;
    if (h == TA_CENTER) CHECKF(c.box.left == el || c.box.left == X - (c.cx + 1) / 2, "left %ld want %d", c.box.left, el);
    else CHECKF(c.box.left == el, "left %ld want %d", c.box.left, el);
    CHECKF(c.box.top == et, "top %ld want %d (cy=%d asc=%d)", c.box.top, et, c.cy, c.asc);
    mdc_free(&m);
}
TEST(dca_textalign_pixels_left_top) { align_case(TA_LEFT, TA_TOP); }
TEST(dca_textalign_pixels_left_bottom) { align_case(TA_LEFT, TA_BOTTOM); }
TEST(dca_textalign_pixels_left_baseline) { align_case(TA_LEFT, TA_BASELINE); }
TEST(dca_textalign_pixels_right_top) { align_case(TA_RIGHT, TA_TOP); }
TEST(dca_textalign_pixels_right_bottom) { align_case(TA_RIGHT, TA_BOTTOM); }
TEST(dca_textalign_pixels_right_baseline) { align_case(TA_RIGHT, TA_BASELINE); }
TEST(dca_textalign_pixels_center_top) { align_case(TA_CENTER, TA_TOP); }
TEST(dca_textalign_pixels_center_bottom) { align_case(TA_CENTER, TA_BOTTOM); }
TEST(dca_textalign_pixels_center_baseline) { align_case(TA_CENTER, TA_BASELINE); }

TEST(dca_textalign_updatecp_uses_and_advances_position) {
    REQUIRE(U(SetTextAlign)); REQUIRE(U(TextOutA)); REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx)); REQUIRE(U(GetTextExtentPoint32A));
    MDC m; if (!mdc_make(&m, 120, 80)) { t_skip("mdc"); return; }
    HGDIOBJ of = U(SelectObject)(m.dc, U(GetStockObject)(SYSTEM_FIXED_FONT));
    U(SetBkMode)(m.dc, OPAQUE); U(SetBkColor)(m.dc, CBK);
    mdc_clear(&m, CBACK);
    SIZE sz; U(GetTextExtentPoint32A)(m.dc, "AB", 2, &sz);
    U(MoveToEx)(m.dc, 17, 23, NULL);
    U(SetTextAlign)(m.dc, TA_UPDATECP);
    CHECK(U(TextOutA)(m.dc, 99, 70, "AB", 2));     /* x,y ignored */
    POINT p = { -1, -1 };
    CHECK(U(GetCurrentPositionEx)(m.dc, &p));
    CHECKF(p.x == 17 + sz.cx && p.y == 23, "pos (%ld,%ld) want (%ld,23)", p.x, p.y, 17 + sz.cx);
    RECT r; int n = mdc_bbox(&m, CBACK, &r);
    CHECKF(n > 0 && r.left == 17 && r.top == 23, "box left %ld top %ld", r.left, r.top);
    /* second call continues from the new position */
    CHECK(U(TextOutA)(m.dc, 0, 0, "AB", 2));
    CHECK(U(GetCurrentPositionEx)(m.dc, &p));
    CHECKF(p.x == 17 + 2 * sz.cx && p.y == 23, "pos2 (%ld,%ld)", p.x, p.y);
    U(SetTextAlign)(m.dc, 0);
    U(SelectObject)(m.dc, of);
    mdc_free(&m);
}
TEST(dca_textalign_noupdatecp_keeps_position) {
    REQUIRE(U(SetTextAlign)); REQUIRE(U(TextOutA)); REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    MDC m; if (!mdc_make(&m, 120, 80)) { t_skip("mdc"); return; }
    HGDIOBJ of = U(SelectObject)(m.dc, U(GetStockObject)(SYSTEM_FIXED_FONT));
    U(MoveToEx)(m.dc, 17, 23, NULL);
    U(SetTextAlign)(m.dc, TA_NOUPDATECP);
    CHECK(U(TextOutA)(m.dc, 40, 40, "AB", 2));
    POINT p = { -1, -1 };
    CHECK(U(GetCurrentPositionEx)(m.dc, &p));
    CHECKF(p.x == 17 && p.y == 23, "pos (%ld,%ld)", p.x, p.y);
    U(SelectObject)(m.dc, of);
    mdc_free(&m);
}
TEST(dca_textalign_updatecp_right_observed) {
    REQUIRE(U(SetTextAlign)); REQUIRE(U(TextOutA)); REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    MDC m; if (!mdc_make(&m, 120, 80)) { t_skip("mdc"); return; }
    HGDIOBJ of = U(SelectObject)(m.dc, U(GetStockObject)(SYSTEM_FIXED_FONT));
    U(MoveToEx)(m.dc, 60, 23, NULL);
    U(SetTextAlign)(m.dc, TA_UPDATECP | TA_RIGHT);
    U(TextOutA)(m.dc, 0, 0, "AB", 2);
    POINT p = { -1, -1 }; U(GetCurrentPositionEx)(m.dc, &p);
    t_note("TA_UPDATECP|TA_RIGHT after TextOut: pos (%ld,%ld) (started 60,23)", p.x, p.y);
    U(SetTextAlign)(m.dc, 0);
    U(SelectObject)(m.dc, of);
    mdc_free(&m);
}

/* ---------- TextCharacterExtra ---------- */
TEST(dca_charextra_extent_grows) {
    REQUIRE(U(SetTextCharacterExtra)); REQUIRE(U(GetTextExtentPoint32A));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    HGDIOBJ of = U(SelectObject)(m.dc, U(GetStockObject)(SYSTEM_FIXED_FONT));
    SIZE s0; U(GetTextExtentPoint32A)(m.dc, "ABCD", 4, &s0);
    static const int ex[] = { 1, 2, 5, 10, -1, -2, 0 };
    for (unsigned i = 0; i < sizeof ex / sizeof ex[0]; i++) {
        U(SetTextCharacterExtra)(m.dc, ex[i]);
        SIZE s; U(GetTextExtentPoint32A)(m.dc, "ABCD", 4, &s);
        CHECKF(s.cx == s0.cx + 4 * ex[i], "extra %d: cx %ld want %ld", ex[i], s.cx, s0.cx + 4 * ex[i]);
        CHECKF(s.cy == s0.cy, "cy changed %ld vs %ld", s.cy, s0.cy);
    }
    U(SelectObject)(m.dc, of);
    mdc_free(&m);
}
TEST(dca_charextra_extent_single_char_and_empty) {
    REQUIRE(U(SetTextCharacterExtra)); REQUIRE(U(GetTextExtentPoint32A));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    HGDIOBJ of = U(SelectObject)(m.dc, U(GetStockObject)(SYSTEM_FIXED_FONT));
    SIZE s0, s1, e;
    U(GetTextExtentPoint32A)(m.dc, "A", 1, &s0);
    U(SetTextCharacterExtra)(m.dc, 3);
    U(GetTextExtentPoint32A)(m.dc, "A", 1, &s1);
    CHECKF(s1.cx == s0.cx + 3, "1 char: %ld vs %ld", s1.cx, s0.cx);
    e.cx = e.cy = 77;
    CHECK(U(GetTextExtentPoint32A)(m.dc, "", 0, &e));
    CHECKF(e.cx == 0, "empty string cx=%ld", e.cx);
    U(SetTextCharacterExtra)(m.dc, 0);
    U(SelectObject)(m.dc, of);
    mdc_free(&m);
}
TEST(dca_charextra_text_cell_width) {
    REQUIRE(U(SetTextCharacterExtra)); REQUIRE(U(TextOutA)); REQUIRE(U(GetTextExtentPoint32A)); REQUIRE(U(SetBkMode));
    MDC m; if (!mdc_make(&m, 160, 40)) { t_skip("mdc"); return; }
    HGDIOBJ of = U(SelectObject)(m.dc, U(GetStockObject)(SYSTEM_FIXED_FONT));
    U(SetBkMode)(m.dc, OPAQUE); U(SetBkColor)(m.dc, CBK);
    static const int ex[] = { 0, 1, 4, -1 };
    for (unsigned i = 0; i < sizeof ex / sizeof ex[0]; i++) {
        mdc_clear(&m, CBACK);
        U(SetTextCharacterExtra)(m.dc, ex[i]);
        SIZE s; U(GetTextExtentPoint32A)(m.dc, "ABC", 3, &s);
        U(TextOutA)(m.dc, 10, 5, "ABC", 3);
        RECT r; int n = mdc_bbox(&m, CBACK, &r);
        int w = r.right - r.left + 1;
        CHECKF(n > 0 && r.left == 10, "extra %d left %ld", ex[i], r.left);
        if (ex[i] >= 0) CHECKF(w == s.cx, "extra %d: cell width %d, extent %ld", ex[i], w, s.cx);
        else if (w != s.cx) t_note("extra %d: cell width %d, extent %ld", ex[i], w, s.cx);
    }
    U(SetTextCharacterExtra)(m.dc, 0);
    U(SelectObject)(m.dc, of);
    mdc_free(&m);
}
TEST(dca_charextra_int_min_rejected) {
    REQUIRE(U(SetTextCharacterExtra)); REQUIRE(U(GetTextCharacterExtra));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    U(SetTextCharacterExtra)(m.dc, 7);
    SetLastError(0);
    int r = U(SetTextCharacterExtra)(m.dc, INT_MIN);
    EXPECT_EQ(r, INT_MIN);
    EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    EXPECT_EQ(U(GetTextCharacterExtra)(m.dc), 7);
    mdc_free(&m);
}
TEST(dca_charextra_extremes_observed) {
    REQUIRE(U(SetTextCharacterExtra)); REQUIRE(U(GetTextCharacterExtra));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    static const int v[] = { INT_MAX, INT_MIN + 1, 0x40000000, -0x40000000, 65535, -65536, 32768, -32769 };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        U(SetTextCharacterExtra)(m.dc, 0);
        U(SetTextCharacterExtra)(m.dc, v[i]);
        int g = U(GetTextCharacterExtra)(m.dc);
        if (g != v[i]) t_note("SetTextCharacterExtra(%d) -> Get %d", v[i], g);
    }
    U(SetTextCharacterExtra)(m.dc, 0);
    EXPECT_EQ(U(GetTextCharacterExtra)(m.dc), 0);
    mdc_free(&m);
}

/* ---------- TextJustification ---------- */
static int cell_width(MDC *m, const char *s, int n) {
    HGDIOBJ of = U(SelectObject)(m->dc, U(GetStockObject)(SYSTEM_FIXED_FONT));
    U(SetBkMode)(m->dc, OPAQUE); U(SetBkColor)(m->dc, CBK);
    mdc_clear(m, CBACK);
    U(TextOutA)(m->dc, 10, 5, s, n);
    RECT r; int c = mdc_bbox(m, CBACK, &r);
    U(SelectObject)(m->dc, of);
    return c ? r.right - r.left + 1 : -1;
}
TEST(dca_justification_return_values) {
    REQUIRE(U(SetTextJustification));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    CHECK(U(SetTextJustification)(m.dc, 10, 1));
    CHECK(U(SetTextJustification)(m.dc, 0, 0));
    CHECK(U(SetTextJustification)(m.dc, -5, 3));
    CHECK(U(SetTextJustification)(m.dc, 0, 0));
    CHECK(U(SetTextJustification)(m.dc, 100000, 100));
    CHECK(U(SetTextJustification)(m.dc, 0, 0));
    mdc_free(&m);
}
TEST(dca_justification_bad_dc) {
    REQUIRE(U(SetTextJustification));
    CHECK(!U(SetTextJustification)(NULL, 5, 1));
    for (int i = 0; i < DCA_NGARBAGE; i++) CHECKF(!U(SetTextJustification)((HDC)dca_garbage[i], 5, 1), "garbage %d", i);
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    HDC d = m.dc; mdc_free(&m);
    CHECK(!U(SetTextJustification)(d, 5, 1));
    WrongSet w; dca_wrong_make(&w);
    for (int i = 0; i < w.n; i++) if (w.h[i]) CHECKF(!U(SetTextJustification)((HDC)w.h[i], 5, 1), "%s", w.nm[i]);
    dca_wrong_free(&w);
}
TEST(dca_justification_widens_break_chars) {
    REQUIRE(U(SetTextJustification)); REQUIRE(U(TextOutA)); REQUIRE(U(SetBkMode));
    MDC m; if (!mdc_make(&m, 200, 40)) { t_skip("mdc"); return; }
    int base = cell_width(&m, "A B", 3);
    U(SetTextJustification)(m.dc, 10, 1);
    int w1 = cell_width(&m, "A B", 3);
    CHECKF(base > 0 && w1 == base + 10, "base %d justified %d", base, w1);
    U(SetTextJustification)(m.dc, 0, 0);
    int w2 = cell_width(&m, "A B", 3);
    CHECKF(w2 == base, "after reset %d vs %d", w2, base);
    mdc_free(&m);
}
TEST(dca_justification_two_breaks_total_extra) {
    REQUIRE(U(SetTextJustification)); REQUIRE(U(TextOutA)); REQUIRE(U(SetBkMode));
    MDC m; if (!mdc_make(&m, 200, 40)) { t_skip("mdc"); return; }
    int base = cell_width(&m, "A B C", 5);
    U(SetTextJustification)(m.dc, 8, 2);
    int w = cell_width(&m, "A B C", 5);
    CHECKF(w == base + 8, "base %d justified %d", base, w);
    U(SetTextJustification)(m.dc, 0, 0);
    mdc_free(&m);
}
TEST(dca_justification_no_break_chars_no_change) {
    REQUIRE(U(SetTextJustification)); REQUIRE(U(TextOutA)); REQUIRE(U(SetBkMode));
    MDC m; if (!mdc_make(&m, 200, 40)) { t_skip("mdc"); return; }
    int base = cell_width(&m, "ABC", 3);
    U(SetTextJustification)(m.dc, 10, 1);
    int w = cell_width(&m, "ABC", 3);
    CHECKF(w == base, "no spaces: base %d justified %d", base, w);
    U(SetTextJustification)(m.dc, 0, 0);
    mdc_free(&m);
}
TEST(dca_justification_negative_extra_observed) {
    REQUIRE(U(SetTextJustification)); REQUIRE(U(TextOutA)); REQUIRE(U(SetBkMode));
    MDC m; if (!mdc_make(&m, 200, 40)) { t_skip("mdc"); return; }
    int base = cell_width(&m, "A B", 3);
    U(SetTextJustification)(m.dc, -2, 1);
    int w = cell_width(&m, "A B", 3);
    t_note("justification extra=-2: base %d -> %d", base, w);
    U(SetTextJustification)(m.dc, 0, 0);
    mdc_free(&m);
}
TEST(dca_justification_persistence_after_textout_observed) {
    REQUIRE(U(SetTextJustification)); REQUIRE(U(TextOutA)); REQUIRE(U(SetBkMode));
    MDC m; if (!mdc_make(&m, 200, 40)) { t_skip("mdc"); return; }
    int base = cell_width(&m, "A B", 3);
    U(SetTextJustification)(m.dc, 6, 1);
    int w1 = cell_width(&m, "A B", 3);
    int w2 = cell_width(&m, "A B", 3);
    t_note("justification persistence: base %d, first %d, second %d", base, w1, w2);
    CHECKF(w1 == base + 6, "first call %d", w1);
    U(SetTextJustification)(m.dc, 0, 0);
    mdc_free(&m);
}
TEST(dca_justification_count_exceeds_breaks_observed) {
    REQUIRE(U(SetTextJustification)); REQUIRE(U(TextOutA)); REQUIRE(U(SetBkMode));
    MDC m; if (!mdc_make(&m, 200, 40)) { t_skip("mdc"); return; }
    int base = cell_width(&m, "A B", 3);
    U(SetTextJustification)(m.dc, 8, 4);
    int w = cell_width(&m, "A B", 3);
    t_note("justification extra=8 count=4, one space: base %d -> %d", base, w);
    U(SetTextJustification)(m.dc, 0, 0);
    mdc_free(&m);
}
TEST(dca_justification_isolated_per_dc) {
    REQUIRE(U(SetTextJustification)); REQUIRE(U(TextOutA)); REQUIRE(U(SetBkMode));
    MDC a, b; if (!mdc_make(&a, 200, 40)) { t_skip("mdc"); return; }
    if (!mdc_make(&b, 200, 40)) { mdc_free(&a); t_skip("mdc"); return; }
    int base = cell_width(&a, "A B", 3);
    U(SetTextJustification)(a.dc, 10, 1);
    int wb = cell_width(&b, "A B", 3);
    CHECKF(wb == base, "other DC affected: %d vs %d", wb, base);
    U(SetTextJustification)(a.dc, 0, 0);
    mdc_free(&a); mdc_free(&b);
}

/* ---------- Layout ---------- */
TEST(dca_layout_rtl_mirrors_patblt) {
    REQUIRE(U(SetLayout)); REQUIRE(U(GetLayout)); REQUIRE(U(PatBlt));
    MDC m; if (!mdc_make(&m, 40, 20)) { t_skip("mdc"); return; }
    mdc_clear(&m, CBACK);
    { ULONG_PTR v_ = (ULONG_PTR)U(SetLayout)(m.dc, LAYOUT_RTL); EXPECT_EQ(v_, 0); }
    EXPECT_EQ(U(GetLayout)(m.dc), LAYOUT_RTL);
    CHECK(U(PatBlt)(m.dc, 0, 0, 10, 10, BLACKNESS));
    RECT r; int n = mdc_bbox(&m, CBACK, &r);
    CHECKF(n == 100 && r.top == 0 && r.bottom == 9 && r.right - r.left == 9, "n=%d box (%ld,%ld)-(%ld,%ld)", n, r.left, r.top, r.right, r.bottom);
    if (r.left != 30) t_note("RTL PatBlt(0,0,10,10) on 40px bitmap covers columns %ld..%ld (pixel-exact mirror would be 30..39)", r.left, r.right);
    CHECKF(r.left >= 28 && r.right <= 39 && r.left > 20, "mirrored block not at the right edge: %ld..%ld", r.left, r.right);
    U(SetLayout)(m.dc, 0);
    mdc_clear(&m, CBACK);
    CHECK(U(PatBlt)(m.dc, 0, 0, 10, 10, BLACKNESS));
    n = mdc_bbox(&m, CBACK, &r);
    CHECKF(n == 100 && r.left == 0 && r.right == 9, "ltr after reset: n=%d left=%ld right=%ld", n, r.left, r.right);
    mdc_free(&m);
}
TEST(dca_layout_rtl_offset_rect) {
    REQUIRE(U(SetLayout)); REQUIRE(U(PatBlt));
    MDC m; if (!mdc_make(&m, 40, 20)) { t_skip("mdc"); return; }
    mdc_clear(&m, CBACK);
    U(SetLayout)(m.dc, LAYOUT_RTL);
    CHECK(U(PatBlt)(m.dc, 5, 2, 10, 4, BLACKNESS));
    RECT r; int n = mdc_bbox(&m, CBACK, &r);
    CHECKF(n == 40 && r.top == 2 && r.bottom == 5 && r.right - r.left == 9, "n=%d box (%ld,%ld)-(%ld,%ld)", n, r.left, r.top, r.right, r.bottom);
    if (r.left != 25) t_note("RTL PatBlt(5,2,10,4) columns %ld..%ld (pixel-exact mirror would be 25..34)", r.left, r.right);
    mdc_free(&m);
}
TEST(dca_layout_rtl_viewport_org_observed) {
    REQUIRE(U(SetLayout)); REQUIRE(U(PatBlt)); REQUIRE(U(SetViewportOrgEx));
    MDC m; if (!mdc_make(&m, 40, 20)) { t_skip("mdc"); return; }
    mdc_clear(&m, CBACK);
    U(SetLayout)(m.dc, LAYOUT_RTL);
    U(SetViewportOrgEx)(m.dc, 5, 0, NULL);
    U(PatBlt)(m.dc, 0, 0, 10, 10, BLACKNESS);
    RECT r; int n = mdc_bbox(&m, CBACK, &r);
    t_note("RTL + viewport org (5,0): PatBlt(0,0,10,10) -> n=%d columns %ld..%ld", n, r.left, r.right);
    CHECKF(n == 100, "n=%d", n);
    U(SetViewportOrgEx)(m.dc, 0, 0, NULL);
    mdc_free(&m);
}
TEST(dca_layout_bitmap_orientation_preserved_pixels) {
    REQUIRE(U(SetLayout)); REQUIRE(U(PatBlt)); REQUIRE(U(BitBlt));
    MDC m; if (!mdc_make(&m, 40, 20)) { t_skip("mdc"); return; }
    MDC src; if (!mdc_make(&src, 10, 10)) { mdc_free(&m); t_skip("mdc"); return; }
    /* source with a marker pixel at its left column */
    mdc_clear(&src, RGB(0, 0, 255));
    src.bits[0 * 10 + 0] = dca_px(RGB(255, 0, 0));
    mdc_clear(&m, CBACK);
    U(SetLayout)(m.dc, LAYOUT_RTL | LAYOUT_BITMAPORIENTATIONPRESERVED);
    CHECK(U(BitBlt)(m.dc, 0, 0, 10, 10, src.dc, 0, 0, SRCCOPY));
    RECT r; int n = mdc_bbox(&m, CBACK, &r);
    CHECKF(n == 100 && r.right - r.left == 9 && r.left > 20, "n=%d cols %ld..%ld", n, r.left, r.right);
    /* with BITMAPORIENTATIONPRESERVED the bitmap is not mirrored: marker stays at the left of the block */
    DWORD l = mdc_get(&m, r.left, 0), rr = mdc_get(&m, r.right, 0);
    t_note("RTL|PRESERVED BitBlt marker at left col: left=0x%06lx right=0x%06lx", l, rr);
    CHECKF(l == dca_px(RGB(255, 0, 0)), "marker pixel mirrored (left=0x%06lx)", l);
    mdc_free(&src); mdc_free(&m);
}
TEST(dca_layout_rtl_bitblt_mirrors_bitmap) {
    REQUIRE(U(SetLayout)); REQUIRE(U(BitBlt));
    MDC m; if (!mdc_make(&m, 40, 20)) { t_skip("mdc"); return; }
    MDC src; if (!mdc_make(&src, 10, 10)) { mdc_free(&m); t_skip("mdc"); return; }
    mdc_clear(&src, RGB(0, 0, 255));
    src.bits[0] = dca_px(RGB(255, 0, 0));
    mdc_clear(&m, CBACK);
    U(SetLayout)(m.dc, LAYOUT_RTL);
    CHECK(U(BitBlt)(m.dc, 0, 0, 10, 10, src.dc, 0, 0, SRCCOPY));
    DWORD l = mdc_get(&m, 30, 0), rr = mdc_get(&m, 38, 0);
    t_note("RTL BitBlt (no PRESERVED): marker left=0x%06lx right=0x%06lx (mirrored bitmap expected)", l, rr);
    CHECKF(mdc_get(&m, 35, 5) == dca_px(RGB(0, 0, 255)), "body pixel 0x%06lx", mdc_get(&m, 35, 5));
    mdc_free(&src); mdc_free(&m);
}
TEST(dca_layout_invalid_bits_observed) {
    REQUIRE(U(SetLayout)); REQUIRE(U(GetLayout));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    static const DWORD v[] = { LAYOUT_BTT, LAYOUT_VBH, LAYOUT_ORIENTATIONMASK, 0x10, 0x20, 0x100, 0xFFFF, 0x80000000, 0xFFFFFFFF, 0x7FFFFFF0 };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        U(SetLayout)(m.dc, 0);
        SetLastError(0);
        DWORD r = U(SetLayout)(m.dc, v[i]);
        DWORD e = GetLastError();
        DWORD g = U(GetLayout)(m.dc);
        t_note("SetLayout(0x%lx): ret=0x%lx err=%lu GetLayout=0x%lx", v[i], r, e, g);
        U(SetLayout)(m.dc, 0);
        CHECKF(U(GetLayout)(m.dc) == 0, "reset after 0x%lx got 0x%lx", v[i], U(GetLayout)(m.dc));
    }
    mdc_free(&m);
}
TEST(dca_layout_screen_dc_default) {
    REQUIRE(U(GetLayout));
    HDC s = dca_screen(); if (!s) { t_skip("no screen"); return; }
    EXPECT_EQ(U(GetLayout)(s), 0);
    dca_unscreen(s);
}
TEST(dca_layout_get_on_metafile_dc_observed) {
    REQUIRE(U(GetLayout)); REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    HDC d = U(CreateMetaFileW)(NULL);
    if (!d) { t_skip("no wmf dc"); return; }
    SetLastError(0);
    DWORD r = U(GetLayout)(d);
    t_note("GetLayout(WMF DC) = 0x%lx err=%lu", r, GetLastError());
    if (dca_wine()) { U(DeleteMetaFile)(U(CloseMetaFile)(d)); return; }
    EXPECT_EQ(r, GDI_ERROR);
    U(DeleteMetaFile)(U(CloseMetaFile)(d));
}

/* ---------- GraphicsMode / world transform ---------- */
TEST(dca_gfxmode_invalid_values) {
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(GetGraphicsMode));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    static const int bad[] = { 0, 3, 4, -1, 100, INT_MAX, INT_MIN };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0);
        int r = U(SetGraphicsMode)(m.dc, bad[i]);
        CHECKF(r == 0, "SetGraphicsMode(%d) ret %d", bad[i], r);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "SetGraphicsMode(%d) err %lu", bad[i], GetLastError());
        CHECKF(U(GetGraphicsMode)(m.dc) == GM_COMPATIBLE, "mode changed by invalid %d", bad[i]);
    }
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        int r = U(SetGraphicsMode)(m.dc, bad[i]);
        CHECKF(r == 0 && U(GetGraphicsMode)(m.dc) == GM_ADVANCED, "advanced kept after %d", bad[i]);
    }
    mdc_free(&m);
}
TEST(dca_gfxmode_setworldtransform_requires_advanced) {
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform)); REQUIRE(U(GetWorldTransform));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    XFORM x = { 2, 0, 0, 2, 5, 5 };
    SetLastError(0);
    BOOL ok = U(SetWorldTransform)(m.dc, &x);
    CHECK(!ok);
    t_note("SetWorldTransform in GM_COMPATIBLE: err=%lu", GetLastError());
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    CHECK(U(SetWorldTransform)(m.dc, &x));
    XFORM g = { 0 };
    CHECK(U(GetWorldTransform)(m.dc, &g));
    CHECKF(g.eM11 == 2 && g.eM12 == 0 && g.eM21 == 0 && g.eM22 == 2 && g.eDx == 5 && g.eDy == 5, "got %g %g %g %g %g %g", g.eM11, g.eM12, g.eM21, g.eM22, g.eDx, g.eDy);
    mdc_free(&m);
}
TEST(dca_gfxmode_same_mode_keeps_transform) {
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform)); REQUIRE(U(GetWorldTransform));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM x = { 1, 0, 0, 1, 3, 4 };
    U(SetWorldTransform)(m.dc, &x);
    int p = U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    CHECKF(p == GM_ADVANCED, "prev %d", p);
    XFORM g = { 0 };
    CHECK(U(GetWorldTransform)(m.dc, &g));
    CHECKF(g.eDx == 3 && g.eDy == 4, "transform lost: %g %g", g.eDx, g.eDy);
    mdc_free(&m);
}
TEST(dca_gfxmode_switch_back_transform_observed) {
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform)); REQUIRE(U(GetWorldTransform)); REQUIRE(U(PatBlt));
    MDC m; if (!mdc_make(&m, 40, 40)) { t_skip("mdc"); return; }
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM x = { 1, 0, 0, 1, 10, 10 };
    U(SetWorldTransform)(m.dc, &x);
    U(SetGraphicsMode)(m.dc, GM_COMPATIBLE);
    XFORM g = { 9, 9, 9, 9, 9, 9 };
    BOOL ok = U(GetWorldTransform)(m.dc, &g);
    t_note("GetWorldTransform after back to GM_COMPATIBLE: ok=%d %g %g %g %g %g %g", ok, g.eM11, g.eM12, g.eM21, g.eM22, g.eDx, g.eDy);
    mdc_clear(&m, CBACK);
    U(PatBlt)(m.dc, 0, 0, 4, 4, BLACKNESS);
    RECT r; int n = mdc_bbox(&m, CBACK, &r);
    t_note("PatBlt(0,0,4,4) in compatible after transform: n=%d at (%ld,%ld)", n, r.left, r.top);
    CHECKF(n == 16, "n=%d", n);
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    ok = U(GetWorldTransform)(m.dc, &g);
    t_note("GetWorldTransform after returning to GM_ADVANCED: ok=%d dx=%g dy=%g", ok, g.eDx, g.eDy);
    mdc_free(&m);
}
TEST(dca_gfxmode_translate_pixels) {
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform)); REQUIRE(U(PatBlt));
    MDC m; if (!mdc_make(&m, 40, 40)) { t_skip("mdc"); return; }
    mdc_clear(&m, CBACK);
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM x = { 1, 0, 0, 1, 5, 3 };
    CHECK(U(SetWorldTransform)(m.dc, &x));
    CHECK(U(PatBlt)(m.dc, 0, 0, 4, 4, BLACKNESS));
    RECT r; int n = mdc_bbox(&m, CBACK, &r);
    CHECKF(n == 16 && r.left == 5 && r.top == 3 && r.right == 8 && r.bottom == 6, "n=%d box (%ld,%ld)-(%ld,%ld)", n, r.left, r.top, r.right, r.bottom);
    mdc_free(&m);
}
TEST(dca_gfxmode_scale_pixels) {
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform)); REQUIRE(U(PatBlt));
    MDC m; if (!mdc_make(&m, 40, 40)) { t_skip("mdc"); return; }
    mdc_clear(&m, CBACK);
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM x = { 2, 0, 0, 3, 0, 0 };
    CHECK(U(SetWorldTransform)(m.dc, &x));
    CHECK(U(PatBlt)(m.dc, 1, 1, 3, 2, BLACKNESS));
    RECT r; int n = mdc_bbox(&m, CBACK, &r);
    CHECKF(n == 36 && r.left == 2 && r.top == 3 && r.right == 7 && r.bottom == 8, "n=%d box (%ld,%ld)-(%ld,%ld)", n, r.left, r.top, r.right, r.bottom);
    mdc_free(&m);
}
TEST(dca_gfxmode_modify_multiply_order) {
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform)); REQUIRE(U(ModifyWorldTransform)); REQUIRE(U(GetWorldTransform));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM s = { 2, 0, 0, 2, 0, 0 }, t = { 1, 0, 0, 1, 10, 0 }, g;
    U(SetWorldTransform)(m.dc, &s);
    CHECK(U(ModifyWorldTransform)(m.dc, &t, MWT_RIGHTMULTIPLY));
    CHECK(U(GetWorldTransform)(m.dc, &g));
    CHECKF(g.eM11 == 2 && g.eM22 == 2 && g.eDx == 10, "right-multiply: m11=%g dx=%g", g.eM11, g.eDx);
    U(SetWorldTransform)(m.dc, &s);
    CHECK(U(ModifyWorldTransform)(m.dc, &t, MWT_LEFTMULTIPLY));
    CHECK(U(GetWorldTransform)(m.dc, &g));
    CHECKF(g.eM11 == 2 && g.eM22 == 2 && g.eDx == 20, "left-multiply: m11=%g dx=%g", g.eM11, g.eDx);
    CHECK(U(ModifyWorldTransform)(m.dc, NULL, MWT_IDENTITY));
    CHECK(U(GetWorldTransform)(m.dc, &g));
    CHECKF(g.eM11 == 1 && g.eM12 == 0 && g.eM21 == 0 && g.eM22 == 1 && g.eDx == 0 && g.eDy == 0, "identity");
    mdc_free(&m);
}
TEST(dca_gfxmode_modify_requires_advanced) {
    REQUIRE(U(ModifyWorldTransform));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    XFORM t = { 1, 0, 0, 1, 10, 0 };
    SetLastError(0);
    BOOL ok = U(ModifyWorldTransform)(m.dc, &t, MWT_RIGHTMULTIPLY);
    t_note("ModifyWorldTransform in compatible mode: ok=%d err=%lu", ok, GetLastError());
    ok = U(ModifyWorldTransform)(m.dc, NULL, MWT_IDENTITY);
    t_note("ModifyWorldTransform(IDENTITY) in compatible mode: ok=%d", ok);
    mdc_free(&m);
}
TEST(dca_gfxmode_modify_bad_mode_value) {
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(ModifyWorldTransform));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM t = { 1, 0, 0, 1, 10, 0 };
    static const DWORD bad[] = { 0, 4, 100, 0xFFFFFFFF };
    for (unsigned i = 0; i < 4; i++) {
        SetLastError(0);
        BOOL ok = U(ModifyWorldTransform)(m.dc, &t, bad[i]);
        if (ok) t_note("ModifyWorldTransform mode %lu accepted", bad[i]);
    }
    mdc_free(&m);
}
TEST(dca_gfxmode_setworldtransform_null) {
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    DCA_WINE_SKIP("NULL XFORM faults in the emulator");
    SetLastError(0);
    BOOL ok = U(SetWorldTransform)(m.dc, NULL);
    CHECK(!ok);
    mdc_free(&m);
}
TEST(dca_gfxmode_singular_transform_observed) {
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM x = { 0, 0, 0, 0, 0, 0 };
    SetLastError(0);
    BOOL ok = U(SetWorldTransform)(m.dc, &x);
    t_note("SetWorldTransform(all zero): ok=%d err=%lu", ok, GetLastError());
    XFORM id = { 1, 0, 0, 1, 0, 0 };
    U(SetWorldTransform)(m.dc, &id);
    mdc_free(&m);
}
TEST(dca_gfxmode_transform_per_dc) {
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform)); REQUIRE(U(GetWorldTransform));
    MDC a, b; if (!mdc_make(&a, 8, 8)) { t_skip("mdc"); return; }
    if (!mdc_make(&b, 8, 8)) { mdc_free(&a); t_skip("mdc"); return; }
    U(SetGraphicsMode)(a.dc, GM_ADVANCED); U(SetGraphicsMode)(b.dc, GM_ADVANCED);
    XFORM x = { 1, 0, 0, 1, 7, 8 }, g;
    U(SetWorldTransform)(a.dc, &x);
    CHECK(U(GetWorldTransform)(b.dc, &g));
    CHECKF(g.eDx == 0 && g.eDy == 0 && g.eM11 == 1, "other DC transform %g %g", g.eDx, g.eDy);
    mdc_free(&a); mdc_free(&b);
}
TEST(dca_gfxmode_savedc_restores_transform) {
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform)); REQUIRE(U(GetWorldTransform));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM a = { 1, 0, 0, 1, 1, 2 }, b = { 3, 0, 0, 3, 9, 9 }, g;
    U(SetWorldTransform)(m.dc, &a);
    int s = U(SaveDC)(m.dc);
    U(SetWorldTransform)(m.dc, &b);
    CHECK(U(RestoreDC)(m.dc, s));
    CHECK(U(GetWorldTransform)(m.dc, &g));
    CHECKF(g.eDx == 1 && g.eDy == 2 && g.eM11 == 1, "restored transform %g %g %g", g.eDx, g.eDy, g.eM11);
    mdc_free(&m);
}

/* ---------- MapperFlags ---------- */
TEST(dca_mapper_invalid_flags) {
    REQUIRE(U(SetMapperFlags));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    static const DWORD bad[] = { 2, 3, 4, 0x80, 0xFFFF, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        U(SetMapperFlags)(m.dc, 1);
        SetLastError(0);
        DWORD r = U(SetMapperFlags)(m.dc, bad[i]);
        CHECKF(r == GDI_ERROR, "SetMapperFlags(0x%lx) ret 0x%lx", bad[i], r);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "SetMapperFlags(0x%lx) err %lu", bad[i], GetLastError());
        DWORD cur = U(SetMapperFlags)(m.dc, 1);
        CHECKF(cur == 1, "value changed by invalid flags: %lu", cur);
        U(SetMapperFlags)(m.dc, 0);
    }
    mdc_free(&m);
}
TEST(dca_mapper_valid_flags_chain) {
    REQUIRE(U(SetMapperFlags));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    CHECK(ASPECT_FILTERING == 1);
    { ULONG_PTR v_ = (ULONG_PTR)U(SetMapperFlags)(m.dc, ASPECT_FILTERING); EXPECT_EQ(v_, 0); }
    { ULONG_PTR v_ = (ULONG_PTR)U(SetMapperFlags)(m.dc, ASPECT_FILTERING); EXPECT_EQ(v_, 1); }
    { ULONG_PTR v_ = (ULONG_PTR)U(SetMapperFlags)(m.dc, 0); EXPECT_EQ(v_, 1); }
    { ULONG_PTR v_ = (ULONG_PTR)U(SetMapperFlags)(m.dc, 0); EXPECT_EQ(v_, 0); }
    mdc_free(&m);
}
TEST(dca_mapper_flags_do_not_leak_between_dcs) {
    REQUIRE(U(SetMapperFlags));
    MDC a, b; if (!mdc_make(&a, 8, 8)) { t_skip("mdc"); return; }
    if (!mdc_make(&b, 8, 8)) { mdc_free(&a); t_skip("mdc"); return; }
    U(SetMapperFlags)(a.dc, 1);
    { ULONG_PTR v_ = (ULONG_PTR)U(SetMapperFlags)(b.dc, 0); EXPECT_EQ(v_, 0); }
    { ULONG_PTR v_ = (ULONG_PTR)U(SetMapperFlags)(a.dc, 0); EXPECT_EQ(v_, 1); }
    mdc_free(&a); mdc_free(&b);
}
