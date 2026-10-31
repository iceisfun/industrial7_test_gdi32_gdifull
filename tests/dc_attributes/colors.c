/* SetBkColor / SetTextColor / SetBkMode / SetROP2: value semantics and exact rendering on DIB memory DCs. */
#include "dca.h"

#define BG   RGB(0xC3, 0x5A, 0x96)
#define PEN  RGB(0x3C, 0xF0, 0xA5)

/* ---------- colors: raw storage, palette-relative values ---------- */
static const COLORREF pal_rel[] = { 0x01000000, 0x01000005, 0x010000FF, 0x01FFFFFF, 0x02000000, 0x02000007, 0x02FFFFFF, 0x0200FF00 };
#define NPALREL ((int)(sizeof pal_rel / sizeof pal_rel[0]))

TEST(dca_bkcolor_palette_relative_roundtrip) {
    REQUIRE(U(SetBkColor)); REQUIRE(U(GetBkColor));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    COLORREF last = U(GetBkColor)(m.dc);
    for (int i = 0; i < NPALREL; i++) {
        COLORREF p = U(SetBkColor)(m.dc, pal_rel[i]);
        CHECKF(p == last, "i=%d prev 0x%lx want 0x%lx", i, p, last);
        CHECKF(U(GetBkColor)(m.dc) == pal_rel[i], "i=%d get 0x%lx", i, U(GetBkColor)(m.dc));
        last = pal_rel[i];
    }
    mdc_free(&m);
}
TEST(dca_textcolor_palette_relative_roundtrip) {
    REQUIRE(U(SetTextColor)); REQUIRE(U(GetTextColor));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    COLORREF last = U(GetTextColor)(m.dc);
    for (int i = 0; i < NPALREL; i++) {
        COLORREF p = U(SetTextColor)(m.dc, pal_rel[i]);
        CHECKF(p == last, "i=%d prev 0x%lx want 0x%lx", i, p, last);
        CHECKF(U(GetTextColor)(m.dc) == pal_rel[i], "i=%d get 0x%lx", i, U(GetTextColor)(m.dc));
        last = pal_rel[i];
    }
    mdc_free(&m);
}
TEST(dca_bkcolor_high_byte_values) {
    REQUIRE(U(SetBkColor)); REQUIRE(U(GetBkColor));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    static const COLORREF v[] = { 0x03123456, 0x04123456, 0x08123456, 0x10123456, 0x80123456, 0xFE123456, 0xFF123456 };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        U(SetBkColor)(m.dc, 0x00ABCDEF);
        COLORREF p = U(SetBkColor)(m.dc, v[i]);
        CHECKF(p == 0x00ABCDEF, "prev 0x%lx", p);
        COLORREF g = U(GetBkColor)(m.dc);
        if (g != v[i]) t_note("SetBkColor(0x%08lx) -> GetBkColor 0x%08lx", v[i], g);
    }
    mdc_free(&m);
}
TEST(dca_bkcolor_clr_invalid) {
    REQUIRE(U(SetBkColor)); REQUIRE(U(GetBkColor));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    U(SetBkColor)(m.dc, 0x00102030);
    SetLastError(0);
    COLORREF p = U(SetBkColor)(m.dc, CLR_INVALID);
    CHECKF(p == 0x00102030, "prev 0x%lx", p);
    t_note("SetBkColor(CLR_INVALID): GetBkColor=0x%08lx err=%lu", U(GetBkColor)(m.dc), GetLastError());
    U(SetBkColor)(m.dc, 0x00FFFFFF);
    EXPECT_EQ(U(GetBkColor)(m.dc), 0x00FFFFFF);
    mdc_free(&m);
}
TEST(dca_textcolor_clr_invalid) {
    REQUIRE(U(SetTextColor)); REQUIRE(U(GetTextColor));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    U(SetTextColor)(m.dc, 0x00102030);
    SetLastError(0);
    COLORREF p = U(SetTextColor)(m.dc, CLR_INVALID);
    CHECKF(p == 0x00102030, "prev 0x%lx", p);
    t_note("SetTextColor(CLR_INVALID): GetTextColor=0x%08lx err=%lu", U(GetTextColor)(m.dc), GetLastError());
    U(SetTextColor)(m.dc, 0);
    EXPECT_EQ(U(GetTextColor)(m.dc), 0);
    mdc_free(&m);
}
TEST(dca_bk_and_text_color_independent) {
    REQUIRE(U(SetBkColor)); REQUIRE(U(GetBkColor)); REQUIRE(U(SetTextColor)); REQUIRE(U(GetTextColor));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    U(SetBkColor)(m.dc, 0x111111);
    U(SetTextColor)(m.dc, 0x222222);
    EXPECT_EQ(U(GetBkColor)(m.dc), 0x111111);
    EXPECT_EQ(U(GetTextColor)(m.dc), 0x222222);
    U(SetBkColor)(m.dc, 0x333333);
    EXPECT_EQ(U(GetTextColor)(m.dc), 0x222222);
    U(SetTextColor)(m.dc, 0x444444);
    EXPECT_EQ(U(GetBkColor)(m.dc), 0x333333);
    mdc_free(&m);
}
TEST(dca_color_all_grey_levels) {
    REQUIRE(U(SetBkColor)); REQUIRE(U(GetBkColor)); REQUIRE(U(SetTextColor)); REQUIRE(U(GetTextColor));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    int bad = 0;
    for (int i = 0; i < 256; i++) {
        COLORREF c = RGB(i, 255 - i, (i * 7) & 255);
        U(SetBkColor)(m.dc, c); U(SetTextColor)(m.dc, ~c & 0xffffff);
        if (U(GetBkColor)(m.dc) != c || U(GetTextColor)(m.dc) != (~c & 0xffffff)) bad++;
    }
    CHECKF(bad == 0, "%d mismatches", bad);
    mdc_free(&m);
}
TEST(dca_bkcolor_not_rounded_by_dib_depth) {
    REQUIRE(U(SetBkColor)); REQUIRE(U(GetBkColor));
    /* colors not representable in a 16bpp or 1bpp surface are still reported unchanged */
    HDC s = dca_screen(); if (!s) { t_skip("no screen"); return; }
    HDC d = U(CreateCompatibleDC)(s);
    HBITMAP mono = U(CreateBitmap)(4, 4, 1, 1, NULL);
    HGDIOBJ old = U(SelectObject)(d, mono);
    U(SetBkColor)(d, 0x010203);
    EXPECT_EQ(U(GetBkColor)(d), 0x010203);
    U(SetTextColor)(d, 0x0A0B0C);
    EXPECT_EQ(U(GetTextColor)(d), 0x0A0B0C);
    U(SelectObject)(d, old); U(DeleteObject)(mono); U(DeleteDC)(d);
    dca_unscreen(s);
}

/* ---------- BkMode / ROP2 invalid values ---------- */
TEST(dca_bkmode_invalid_values_observed) {
    REQUIRE(U(SetBkMode)); REQUIRE(U(GetBkMode));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    static const int bad[] = { 0, 3, -1, 100, INT_MAX, INT_MIN, 0x10001 };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        U(SetBkMode)(m.dc, OPAQUE);
        SetLastError(0);
        int r = U(SetBkMode)(m.dc, bad[i]);
        DWORD e = GetLastError();
        int g = U(GetBkMode)(m.dc);
        t_note("SetBkMode(%d): ret=%d err=%lu GetBkMode=%d", bad[i], r, e, g);
        U(SetBkMode)(m.dc, TRANSPARENT);
        CHECKF(U(GetBkMode)(m.dc) == TRANSPARENT, "recover after invalid %d", bad[i]);
    }
    mdc_free(&m);
}
TEST(dca_rop2_invalid_values_observed) {
    REQUIRE(U(SetROP2)); REQUIRE(U(GetROP2));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    static const int bad[] = { 0, 17, 18, 255, 256, -1, INT_MAX, INT_MIN };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        U(SetROP2)(m.dc, R2_XORPEN);
        SetLastError(0);
        int r = U(SetROP2)(m.dc, bad[i]);
        DWORD e = GetLastError();
        int g = U(GetROP2)(m.dc);
        t_note("SetROP2(%d): ret=%d err=%lu GetROP2=%d", bad[i], r, e, g);
        U(SetROP2)(m.dc, R2_NOT);
        CHECKF(U(GetROP2)(m.dc) == R2_NOT, "recover after invalid %d", bad[i]);
    }
    mdc_free(&m);
}
TEST(dca_rop2_constants) {
    CHECK(R2_BLACK == 1 && R2_NOTMERGEPEN == 2 && R2_MASKNOTPEN == 3 && R2_NOTCOPYPEN == 4);
    CHECK(R2_MASKPENNOT == 5 && R2_NOT == 6 && R2_XORPEN == 7 && R2_NOTMASKPEN == 8);
    CHECK(R2_MASKPEN == 9 && R2_NOTXORPEN == 10 && R2_NOP == 11 && R2_MERGENOTPEN == 12);
    CHECK(R2_COPYPEN == 13 && R2_MERGEPENNOT == 14 && R2_MERGEPEN == 15 && R2_WHITE == 16 && R2_LAST == 16);
    CHECK(TRANSPARENT == 1 && OPAQUE == 2 && BKMODE_LAST == 2);
    CHECK(CLR_INVALID == 0xFFFFFFFF);
}

/* ---------- ROP2 pixel table (32bpp DIB, solid cosmetic pen / solid brush) ---------- */
static DWORD rop2_apply(int rop, DWORD P, DWORD D) {
    DWORD r;
    switch (rop) {
    case R2_BLACK: r = 0; break;
    case R2_NOTMERGEPEN: r = ~(D | P); break;
    case R2_MASKNOTPEN: r = ~P & D; break;
    case R2_NOTCOPYPEN: r = ~P; break;
    case R2_MASKPENNOT: r = P & ~D; break;
    case R2_NOT: r = ~D; break;
    case R2_XORPEN: r = P ^ D; break;
    case R2_NOTMASKPEN: r = ~(P & D); break;
    case R2_MASKPEN: r = P & D; break;
    case R2_NOTXORPEN: r = ~(P ^ D); break;
    case R2_NOP: r = D; break;
    case R2_MERGENOTPEN: r = ~P | D; break;
    case R2_COPYPEN: r = P; break;
    case R2_MERGEPENNOT: r = P | ~D; break;
    case R2_MERGEPEN: r = P | D; break;
    default: r = 0xffffffff; break;
    }
    return r & 0xffffff;
}
static void rop2_pen_case(int rop) {
    REQUIRE(U(SetROP2)); REQUIRE(U(GetROP2)); REQUIRE(U(CreatePen)); REQUIRE(U(MoveToEx)); REQUIRE(U(LineTo));
    MDC m; if (!mdc_make(&m, 40, 12)) { t_skip("mdc"); return; }
    mdc_clear(&m, BG);
    HPEN pen = U(CreatePen)(PS_SOLID, 1, PEN);
    HGDIOBJ op = U(SelectObject)(m.dc, pen);
    { int pr_ = U(SetROP2)(m.dc, rop); EXPECT_EQ(pr_, R2_COPYPEN); }
    EXPECT_EQ(U(GetROP2)(m.dc), rop);
    CHECK(U(MoveToEx)(m.dc, 2, 5, NULL));
    CHECK(U(LineTo)(m.dc, 30, 5));
    DWORD want = rop2_apply(rop, dca_px(PEN), dca_px(BG));
    int xs[] = { 2, 3, 10, 20, 29 };
    for (int i = 0; i < 5; i++) {
        DWORD g = mdc_get(&m, xs[i], 5);
        CHECKF(g == want, "rop %d x=%d got 0x%06lx want 0x%06lx", rop, xs[i], g, want);
    }
    CHECKF(mdc_get(&m, 30, 5) == dca_px(BG), "LineTo end pixel must stay untouched");
    CHECKF(mdc_get(&m, 15, 4) == dca_px(BG) && mdc_get(&m, 15, 6) == dca_px(BG), "neighbour rows untouched");
    U(SelectObject)(m.dc, op); U(DeleteObject)(pen);
    mdc_free(&m);
}
static void rop2_brush_case(int rop) {
    REQUIRE(U(SetROP2)); REQUIRE(U(Rectangle)); REQUIRE(U(CreateSolidBrush));
    MDC m; if (!mdc_make(&m, 40, 20)) { t_skip("mdc"); return; }
    mdc_clear(&m, BG);
    HBRUSH br = U(CreateSolidBrush)(PEN);
    HGDIOBJ ob = U(SelectObject)(m.dc, br);
    HGDIOBJ op = U(SelectObject)(m.dc, U(GetStockObject)(NULL_PEN));
    U(SetROP2)(m.dc, rop);
    CHECK(U(Rectangle)(m.dc, 5, 5, 25, 15));
    DWORD want = rop2_apply(rop, dca_px(PEN), dca_px(BG));
    DWORD g = mdc_get(&m, 12, 9);
    if (g != want) t_note("Rectangle fill with R2 %d: got 0x%06lx want 0x%06lx", rop, g, want);
    CHECKF(mdc_get(&m, 30, 9) == dca_px(BG), "outside untouched");
    U(SelectObject)(m.dc, op); U(SelectObject)(m.dc, ob); U(DeleteObject)(br);
    mdc_free(&m);
}
#define ROPT(name, v) \
    TEST(dca_rop2_pen_pixel_##name) { rop2_pen_case(v); } \
    TEST(dca_rop2_brush_pixel_##name) { rop2_brush_case(v); }
ROPT(black, R2_BLACK) ROPT(notmergepen, R2_NOTMERGEPEN) ROPT(masknotpen, R2_MASKNOTPEN) ROPT(notcopypen, R2_NOTCOPYPEN)
ROPT(maskpennot, R2_MASKPENNOT) ROPT(not, R2_NOT) ROPT(xorpen, R2_XORPEN) ROPT(notmaskpen, R2_NOTMASKPEN)
ROPT(maskpen, R2_MASKPEN) ROPT(notxorpen, R2_NOTXORPEN) ROPT(nop, R2_NOP) ROPT(mergenotpen, R2_MERGENOTPEN)
ROPT(copypen, R2_COPYPEN) ROPT(mergepennot, R2_MERGEPENNOT) ROPT(mergepen, R2_MERGEPEN) ROPT(white, R2_WHITE)

TEST(dca_rop2_xor_twice_restores) {
    REQUIRE(U(SetROP2)); REQUIRE(U(CreatePen)); REQUIRE(U(MoveToEx)); REQUIRE(U(LineTo));
    MDC m; if (!mdc_make(&m, 40, 12)) { t_skip("mdc"); return; }
    mdc_clear(&m, BG);
    HPEN pen = U(CreatePen)(PS_SOLID, 1, PEN);
    HGDIOBJ op = U(SelectObject)(m.dc, pen);
    U(SetROP2)(m.dc, R2_XORPEN);
    for (int pass = 0; pass < 2; pass++) {
        U(MoveToEx)(m.dc, 2, 5, NULL); U(LineTo)(m.dc, 30, 5);
        DWORD g = mdc_get(&m, 10, 5);
        DWORD want = pass == 0 ? (dca_px(BG) ^ dca_px(PEN)) : dca_px(BG);
        CHECKF(g == want, "pass %d got 0x%06lx want 0x%06lx", pass, g, want);
    }
    U(SelectObject)(m.dc, op); U(DeleteObject)(pen);
    mdc_free(&m);
}
TEST(dca_rop2_not_twice_restores) {
    REQUIRE(U(SetROP2)); REQUIRE(U(CreatePen)); REQUIRE(U(MoveToEx)); REQUIRE(U(LineTo));
    MDC m; if (!mdc_make(&m, 40, 12)) { t_skip("mdc"); return; }
    mdc_clear(&m, BG);
    HPEN pen = U(CreatePen)(PS_SOLID, 1, PEN);
    HGDIOBJ op = U(SelectObject)(m.dc, pen);
    U(SetROP2)(m.dc, R2_NOT);
    U(MoveToEx)(m.dc, 2, 5, NULL); U(LineTo)(m.dc, 30, 5);
    CHECK(mdc_get(&m, 10, 5) == (~dca_px(BG) & 0xffffff));
    U(MoveToEx)(m.dc, 2, 5, NULL); U(LineTo)(m.dc, 30, 5);
    CHECK(mdc_get(&m, 10, 5) == dca_px(BG));
    U(SelectObject)(m.dc, op); U(DeleteObject)(pen);
    mdc_free(&m);
}
TEST(dca_rop2_ignored_by_patblt) {
    REQUIRE(U(SetROP2)); REQUIRE(U(PatBlt));
    MDC m; if (!mdc_make(&m, 16, 16)) { t_skip("mdc"); return; }
    mdc_clear(&m, BG);
    HBRUSH br = U(CreateSolidBrush)(PEN);
    HGDIOBJ ob = U(SelectObject)(m.dc, br);
    U(SetROP2)(m.dc, R2_BLACK);
    CHECK(U(PatBlt)(m.dc, 0, 0, 8, 8, PATCOPY));
    CHECKF(mdc_get(&m, 3, 3) == dca_px(PEN), "PatBlt PATCOPY unaffected by SetROP2, got 0x%06lx", mdc_get(&m, 3, 3));
    U(SelectObject)(m.dc, ob); U(DeleteObject)(br);
    mdc_free(&m);
}

/* ---------- BkMode / BkColor rendering ---------- */
static void row_stats(MDC *m, int y, int x0, int x1, COLORREF a, COLORREF b, COLORREF c, int *na, int *nb, int *nc, int *nother) {
    *na = *nb = *nc = *nother = 0;
    for (int x = x0; x < x1; x++) {
        DWORD p = mdc_get(m, x, y);
        if (p == dca_px(a)) (*na)++; else if (p == dca_px(b)) (*nb)++; else if (p == dca_px(c)) (*nc)++; else (*nother)++;
    }
}
TEST(dca_bkmode_opaque_dashed_line_fills_gaps) {
    REQUIRE(U(SetBkMode)); REQUIRE(U(SetBkColor)); REQUIRE(U(CreatePen)); REQUIRE(U(LineTo)); REQUIRE(U(MoveToEx));
    MDC m; if (!mdc_make(&m, 80, 12)) { t_skip("mdc"); return; }
    const COLORREF back = RGB(0x40, 0x40, 0x40), red = RGB(255, 0, 0), grn = RGB(0, 255, 0);
    mdc_clear(&m, back);
    HPEN pen = U(CreatePen)(PS_DASH, 1, red);
    HGDIOBJ op = U(SelectObject)(m.dc, pen);
    U(SetBkColor)(m.dc, grn);
    { int pr_ = U(SetBkMode)(m.dc, OPAQUE); EXPECT_EQ(pr_, OPAQUE); }
    U(MoveToEx)(m.dc, 2, 5, NULL); U(LineTo)(m.dc, 70, 5);
    int nr, ng, nb, no;
    row_stats(&m, 5, 2, 70, red, grn, back, &nr, &ng, &nb, &no);
    CHECKF(nr > 0 && ng > 0, "red=%d green=%d", nr, ng);
    CHECKF(nb == 0 && no == 0, "back=%d other=%d", nb, no);
    U(SelectObject)(m.dc, op); U(DeleteObject)(pen);
    mdc_free(&m);
}
TEST(dca_bkmode_transparent_dashed_line_keeps_gaps) {
    REQUIRE(U(SetBkMode)); REQUIRE(U(SetBkColor)); REQUIRE(U(CreatePen)); REQUIRE(U(LineTo)); REQUIRE(U(MoveToEx));
    MDC m; if (!mdc_make(&m, 80, 12)) { t_skip("mdc"); return; }
    const COLORREF back = RGB(0x40, 0x40, 0x40), red = RGB(255, 0, 0), grn = RGB(0, 255, 0);
    mdc_clear(&m, back);
    HPEN pen = U(CreatePen)(PS_DASH, 1, red);
    HGDIOBJ op = U(SelectObject)(m.dc, pen);
    U(SetBkColor)(m.dc, grn);
    { int pr_ = U(SetBkMode)(m.dc, TRANSPARENT); EXPECT_EQ(pr_, OPAQUE); }
    U(MoveToEx)(m.dc, 2, 5, NULL); U(LineTo)(m.dc, 70, 5);
    int nr, ng, nb, no;
    row_stats(&m, 5, 2, 70, red, grn, back, &nr, &ng, &nb, &no);
    CHECKF(nr > 0 && nb > 0, "red=%d back=%d", nr, nb);
    CHECKF(ng == 0 && no == 0, "green=%d other=%d", ng, no);
    U(SelectObject)(m.dc, op); U(DeleteObject)(pen);
    mdc_free(&m);
}
TEST(dca_bkmode_hatch_brush_opaque) {
    REQUIRE(U(SetBkMode)); REQUIRE(U(SetBkColor)); REQUIRE(U(CreateHatchBrush)); REQUIRE(U(PatBlt));
    MDC m; if (!mdc_make(&m, 32, 32)) { t_skip("mdc"); return; }
    const COLORREF back = RGB(0x40, 0x40, 0x40), red = RGB(255, 0, 0), grn = RGB(0, 255, 0);
    mdc_clear(&m, back);
    HBRUSH hb = U(CreateHatchBrush)(HS_CROSS, red);
    HGDIOBJ ob = U(SelectObject)(m.dc, hb);
    U(SetBkColor)(m.dc, grn); U(SetBkMode)(m.dc, OPAQUE);
    CHECK(U(PatBlt)(m.dc, 0, 0, 32, 32, PATCOPY));
    int nr = 0, ng = 0, nb = 0, no = 0;
    for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++) {
        DWORD p = mdc_get(&m, x, y);
        if (p == dca_px(red)) nr++; else if (p == dca_px(grn)) ng++; else if (p == dca_px(back)) nb++; else no++;
    }
    CHECKF(nr > 0 && ng > 0 && nb == 0 && no == 0, "red=%d green=%d back=%d other=%d", nr, ng, nb, no);
    U(SelectObject)(m.dc, ob); U(DeleteObject)(hb);
    mdc_free(&m);
}
TEST(dca_bkmode_hatch_brush_transparent) {
    REQUIRE(U(SetBkMode)); REQUIRE(U(SetBkColor)); REQUIRE(U(CreateHatchBrush)); REQUIRE(U(PatBlt));
    MDC m; if (!mdc_make(&m, 32, 32)) { t_skip("mdc"); return; }
    const COLORREF back = RGB(0x40, 0x40, 0x40), red = RGB(255, 0, 0), grn = RGB(0, 255, 0);
    mdc_clear(&m, back);
    HBRUSH hb = U(CreateHatchBrush)(HS_CROSS, red);
    HGDIOBJ ob = U(SelectObject)(m.dc, hb);
    U(SetBkColor)(m.dc, grn); U(SetBkMode)(m.dc, TRANSPARENT);
    CHECK(U(PatBlt)(m.dc, 0, 0, 32, 32, PATCOPY));
    int nr = 0, ng = 0, nb = 0, no = 0;
    for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++) {
        DWORD p = mdc_get(&m, x, y);
        if (p == dca_px(red)) nr++; else if (p == dca_px(grn)) ng++; else if (p == dca_px(back)) nb++; else no++;
    }
    CHECKF(nr > 0 && nb > 0 && ng == 0 && no == 0, "red=%d green=%d back=%d other=%d", nr, ng, nb, no);
    U(SelectObject)(m.dc, ob); U(DeleteObject)(hb);
    mdc_free(&m);
}
static void eto_fill(int mode, COLORREF bk) {
    REQUIRE(U(SetBkMode)); REQUIRE(U(SetBkColor)); REQUIRE(U(ExtTextOutA));
    MDC m; if (!mdc_make(&m, 32, 24)) { t_skip("mdc"); return; }
    mdc_clear(&m, RGB(0x10, 0x20, 0x30));
    U(SetBkColor)(m.dc, bk); U(SetBkMode)(m.dc, mode);
    RECT r = { 4, 3, 20, 15 };
    CHECK(U(ExtTextOutA)(m.dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL));
    for (int y = 0; y < 24; y++) for (int x = 0; x < 32; x++) {
        int in = x >= 4 && x < 20 && y >= 3 && y < 15;
        DWORD want = in ? dca_px(bk) : dca_px(RGB(0x10, 0x20, 0x30));
        if (mdc_get(&m, x, y) != want) { CHECKF(0, "mode %d (%d,%d) got 0x%06lx want 0x%06lx", mode, x, y, mdc_get(&m, x, y), want); goto done; }
    }
    CHECK(1);
done:
    mdc_free(&m);
}
TEST(dca_bkcolor_eto_opaque_rect_opaque_mode) { eto_fill(OPAQUE, RGB(255, 128, 0)); }
TEST(dca_bkcolor_eto_opaque_rect_transparent_mode) { eto_fill(TRANSPARENT, RGB(1, 2, 3)); }
TEST(dca_bkcolor_eto_opaque_rect_white) { eto_fill(OPAQUE, RGB(255, 255, 255)); }
TEST(dca_bkcolor_eto_opaque_rect_black) { eto_fill(OPAQUE, RGB(0, 0, 0)); }
TEST(dca_bkcolor_eto_opaque_rect_follows_changes) {
    REQUIRE(U(SetBkColor)); REQUIRE(U(ExtTextOutA));
    MDC m; if (!mdc_make(&m, 16, 16)) { t_skip("mdc"); return; }
    RECT r = { 0, 0, 4, 4 };
    static const COLORREF c[] = { RGB(1,2,3), RGB(250,251,252), RGB(17,200,99), RGB(0,0,255) };
    for (int i = 0; i < 4; i++) {
        U(SetBkColor)(m.dc, c[i]);
        OffsetRect(&r, 0, i ? 4 : 0);
        U(ExtTextOutA)(m.dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL);
        CHECKF(mdc_get(&m, 1, r.top + 1) == dca_px(c[i]), "i=%d", i);
    }
    mdc_free(&m);
}

/* raster font text: every touched pixel is either text colour or (OPAQUE) back colour */
static void text_colors(int mode) {
    REQUIRE(U(SetBkMode)); REQUIRE(U(SetBkColor)); REQUIRE(U(SetTextColor)); REQUIRE(U(TextOutA)); REQUIRE(U(GetStockObject));
    MDC m; if (!mdc_make(&m, 40, 32)) { t_skip("mdc"); return; }
    const COLORREF back = RGB(0x40, 0x40, 0x40), red = RGB(255, 0, 0), grn = RGB(0, 255, 0);
    mdc_clear(&m, back);
    HGDIOBJ of = U(SelectObject)(m.dc, U(GetStockObject)(SYSTEM_FIXED_FONT));
    U(SetBkMode)(m.dc, mode); U(SetBkColor)(m.dc, grn); U(SetTextColor)(m.dc, red);
    CHECK(U(TextOutA)(m.dc, 2, 2, "HE", 2));
    int nr = 0, ng = 0, nb = 0, no = 0;
    for (int y = 0; y < 32; y++) for (int x = 0; x < 40; x++) {
        DWORD p = mdc_get(&m, x, y);
        if (p == dca_px(red)) nr++; else if (p == dca_px(grn)) ng++; else if (p == dca_px(back)) nb++; else no++;
    }
    if (no) t_note("font rendering produced %d off-palette pixels (smoothing?)", no);
    CHECKF(nr > 0, "no text pixels");
    if (mode == OPAQUE) CHECKF(ng > 0 && nb > 0 /* outside cell */ && (no == 0 || dca_wine()), "opaque: green=%d back=%d other=%d", ng, nb, no);
    else CHECKF(ng == 0, "transparent: green=%d", ng);
    U(SelectObject)(m.dc, of);
    mdc_free(&m);
}
TEST(dca_textcolor_raster_text_opaque) { text_colors(OPAQUE); }
TEST(dca_textcolor_raster_text_transparent) { text_colors(TRANSPARENT); }
TEST(dca_textcolor_changes_between_calls) {
    REQUIRE(U(SetTextColor)); REQUIRE(U(TextOutA)); REQUIRE(U(SetBkMode));
    MDC m; if (!mdc_make(&m, 40, 40)) { t_skip("mdc"); return; }
    const COLORREF back = RGB(0x40, 0x40, 0x40);
    mdc_clear(&m, back);
    HGDIOBJ of = U(SelectObject)(m.dc, U(GetStockObject)(SYSTEM_FIXED_FONT));
    U(SetBkMode)(m.dc, TRANSPARENT);
    static const COLORREF c[] = { RGB(255, 0, 0), RGB(0, 0, 255) };
    for (int i = 0; i < 2; i++) {
        U(SetTextColor)(m.dc, c[i]);
        U(TextOutA)(m.dc, 2, 2 + i * 18, "M", 1);
        int hit = 0, other = 0;
        for (int y = 2 + i * 18; y < 2 + i * 18 + 16; y++) for (int x = 0; x < 20; x++) {
            DWORD p = mdc_get(&m, x, y);
            if (p == dca_px(c[i])) hit++; else if (p != dca_px(back)) other++;
        }
        CHECKF(hit > 0 && other == 0, "i=%d hit=%d other=%d", i, hit, other);
    }
    U(SelectObject)(m.dc, of);
    mdc_free(&m);
}

/* ---------- attributes are independent of surface depth ---------- */
static HDC mk_bpp_dc(int bpp, HBITMAP *bm, HGDIOBJ *old) {
    HDC d = U(CreateCompatibleDC)(NULL);
    if (!d) return NULL;
    struct { BITMAPINFOHEADER h; RGBQUAD pal[256]; } bi;
    memset(&bi, 0, sizeof bi);
    bi.h.biSize = sizeof bi.h; bi.h.biWidth = 8; bi.h.biHeight = -8; bi.h.biPlanes = 1; bi.h.biBitCount = bpp;
    bi.h.biCompression = BI_RGB; bi.h.biClrUsed = bpp <= 8 ? (1u << bpp) : 0;
    if (bpp <= 8) for (unsigned i = 0; i < bi.h.biClrUsed; i++) { bi.pal[i].rgbRed = bi.pal[i].rgbGreen = bi.pal[i].rgbBlue = (BYTE)(i * 255 / (bi.h.biClrUsed - 1)); }
    void *bits;
    *bm = U(CreateDIBSection)(d, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!*bm) { U(DeleteDC)(d); return NULL; }
    *old = U(SelectObject)(d, *bm);
    return d;
}
extern const DcaAttr dca_attrs[]; extern const int dca_nattrs;
static void depth_case(int bpp) {
    HBITMAP bm; HGDIOBJ old;
    HDC d = mk_bpp_dc(bpp, &bm, &old);
    if (!d) { t_skip("cannot create %dbpp DIB", bpp); return; }
    for (int i = 0; i < dca_nattrs; i++) {
        const DcaAttr *a = &dca_attrs[i];
        if (!a->avail()) continue;
        CHECKF(a->get(d) == a->def, "%dbpp %s default 0x%lx", bpp, a->name, a->get(d));
        DWORD v = dca_nondef(a);
        a->set(d, v);
        CHECKF(a->get(d) == v, "%dbpp %s roundtrip 0x%lx", bpp, a->name, a->get(d));
        a->set(d, a->def);
    }
    /* colours that cannot be represented are reported back unchanged */
    U(SetBkColor)(d, 0x010203);
    CHECKF(U(GetBkColor)(d) == 0x010203, "%dbpp bk exactness", bpp);
    U(SetBkColor)(d, 0xFFFFFF);
    U(SelectObject)(d, old); U(DeleteObject)(bm); U(DeleteDC)(d);
}
TEST(dca_depth_1bpp) { depth_case(1); }
TEST(dca_depth_4bpp) { depth_case(4); }
TEST(dca_depth_8bpp) { depth_case(8); }
TEST(dca_depth_16bpp) { depth_case(16); }
TEST(dca_depth_24bpp) { depth_case(24); }
TEST(dca_depth_32bpp) { depth_case(32); }
