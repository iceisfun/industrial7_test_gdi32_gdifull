/* PolyFillMode, StretchBltMode, ArcDirection, MiterLimit, RelAbs: values and rendering effects. */
#include "dca.h"

UDECL(int, SetRelAbs, (HDC, int));
UDECL(int, GetRelAbs, (HDC, DWORD));

static const COLORREF WHITE_ = RGB(255, 255, 255), BLACK_ = RGB(0, 0, 0);

/* ---------- PolyFillMode ---------- */
static const POINT star[5] = { {50, 7}, {76, 88}, {7, 38}, {93, 38}, {24, 88} };

static int star_fill_pixel(int mode, int px, int py, int via_path) {
    MDC m; if (!mdc_make(&m, 100, 100)) return -1;
    mdc_clear(&m, WHITE_);
    HGDIOBJ ob = U(SelectObject)(m.dc, U(GetStockObject)(BLACK_BRUSH));
    HGDIOBJ op = U(SelectObject)(m.dc, U(GetStockObject)(NULL_PEN));
    U(SetPolyFillMode)(m.dc, mode);
    if (via_path) {
        U(BeginPath)(m.dc); U(Polygon)(m.dc, star, 5); U(EndPath)(m.dc);
        U(FillPath)(m.dc);
    } else U(Polygon)(m.dc, star, 5);
    int filled = mdc_get(&m, px, py) == dca_px(BLACK_);
    U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, op);
    mdc_free(&m);
    return filled;
}
#define NEEDPOLY() do { REQUIRE(U(SetPolyFillMode)); REQUIRE(U(Polygon)); REQUIRE(U(GetStockObject)); } while (0)
TEST(dca_polyfill_star_center_alternate_is_hole) { NEEDPOLY(); int r = star_fill_pixel(ALTERNATE, 50, 52, 0); if (r < 0) { t_skip("mdc"); return; } CHECK(r == 0); }
TEST(dca_polyfill_star_center_winding_is_filled) { NEEDPOLY(); int r = star_fill_pixel(WINDING, 50, 52, 0); if (r < 0) { t_skip("mdc"); return; } CHECK(r == 1); }
TEST(dca_polyfill_star_arm_alternate) { NEEDPOLY(); int r = star_fill_pixel(ALTERNATE, 50, 15, 0); if (r < 0) { t_skip("mdc"); return; } CHECK(r == 1); }
TEST(dca_polyfill_star_arm_winding) { NEEDPOLY(); int r = star_fill_pixel(WINDING, 50, 15, 0); if (r < 0) { t_skip("mdc"); return; } CHECK(r == 1); }
TEST(dca_polyfill_star_outside_both) {
    NEEDPOLY();
    int a = star_fill_pixel(ALTERNATE, 3, 3, 0), w = star_fill_pixel(WINDING, 96, 96, 0);
    if (a < 0 || w < 0) { t_skip("mdc"); return; }
    CHECK(a == 0 && w == 0);
}
TEST(dca_polyfill_path_center_alternate) {
    NEEDPOLY(); REQUIRE(U(BeginPath)); REQUIRE(U(EndPath)); REQUIRE(U(FillPath));
    int r = star_fill_pixel(ALTERNATE, 50, 52, 1); if (r < 0) { t_skip("mdc"); return; } CHECK(r == 0);
}
TEST(dca_polyfill_path_center_winding) {
    NEEDPOLY(); REQUIRE(U(BeginPath)); REQUIRE(U(EndPath)); REQUIRE(U(FillPath));
    int r = star_fill_pixel(WINDING, 50, 52, 1); if (r < 0) { t_skip("mdc"); return; } CHECK(r == 1);
}
TEST(dca_polyfill_region_ignores_dc_mode) {
    REQUIRE(U(CreatePolygonRgn)); REQUIRE(U(PtInRegion)); REQUIRE(U(SetPolyFillMode));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    for (int dcmode = 1; dcmode <= 2; dcmode++) {
        U(SetPolyFillMode)(m.dc, dcmode);
        HRGN ra = U(CreatePolygonRgn)(star, 5, ALTERNATE), rw = U(CreatePolygonRgn)(star, 5, WINDING);
        CHECK(ra && rw);
        CHECKF(!U(PtInRegion)(ra, 50, 52), "alternate region center, dc mode %d", dcmode);
        CHECKF(U(PtInRegion)(rw, 50, 52), "winding region center, dc mode %d", dcmode);
        U(DeleteObject)(ra); U(DeleteObject)(rw);
    }
    mdc_free(&m);
}
static void polypoly_case(int mode, int reverse2, int expect_overlap_filled) {
    REQUIRE(U(SetPolyFillMode)); REQUIRE(U(PolyPolygon));
    MDC m; if (!mdc_make(&m, 70, 70)) { t_skip("mdc"); return; }
    mdc_clear(&m, WHITE_);
    POINT pts[8] = { {10,10},{40,10},{40,40},{10,40},  {25,25},{55,25},{55,55},{25,55} };
    if (reverse2) { POINT t = pts[5]; pts[5] = pts[7]; pts[7] = t; }
    INT cnt[2] = { 4, 4 };
    HGDIOBJ ob = U(SelectObject)(m.dc, U(GetStockObject)(BLACK_BRUSH));
    HGDIOBJ op = U(SelectObject)(m.dc, U(GetStockObject)(NULL_PEN));
    U(SetPolyFillMode)(m.dc, mode);
    CHECK(U(PolyPolygon)(m.dc, pts, cnt, 2));
    CHECKF(mdc_get(&m, 15, 15) == dca_px(BLACK_), "first only");
    CHECKF(mdc_get(&m, 50, 50) == dca_px(BLACK_), "second only");
    CHECKF(mdc_get(&m, 5, 5) == dca_px(WHITE_), "outside");
    int filled = mdc_get(&m, 32, 32) == dca_px(BLACK_);
    CHECKF(filled == expect_overlap_filled, "overlap filled=%d want %d", filled, expect_overlap_filled);
    U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, op);
    mdc_free(&m);
}
TEST(dca_polyfill_polypolygon_alternate_overlap_hole) { polypoly_case(ALTERNATE, 0, 0); }
TEST(dca_polyfill_polypolygon_winding_same_orientation) { polypoly_case(WINDING, 0, 1); }
TEST(dca_polyfill_polypolygon_winding_opposite_orientation) { polypoly_case(WINDING, 1, 0); }
TEST(dca_polyfill_polypolygon_alternate_opposite_orientation) { polypoly_case(ALTERNATE, 1, 0); }
TEST(dca_polyfill_invalid_values_observed) {
    REQUIRE(U(SetPolyFillMode)); REQUIRE(U(GetPolyFillMode));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    static const int bad[] = { 0, 3, -1, 100, INT_MAX, INT_MIN };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        U(SetPolyFillMode)(m.dc, WINDING);
        SetLastError(0);
        int r = U(SetPolyFillMode)(m.dc, bad[i]);
        DWORD e = GetLastError();
        t_note("SetPolyFillMode(%d): ret=%d err=%lu Get=%d", bad[i], r, e, U(GetPolyFillMode)(m.dc));
        U(SetPolyFillMode)(m.dc, ALTERNATE);
        CHECKF(U(GetPolyFillMode)(m.dc) == ALTERNATE, "recover after %d", bad[i]);
    }
    mdc_free(&m);
}
TEST(dca_polyfill_mode_changes_apply_to_next_call) {
    NEEDPOLY();
    MDC m; if (!mdc_make(&m, 100, 100)) { t_skip("mdc"); return; }
    HGDIOBJ ob = U(SelectObject)(m.dc, U(GetStockObject)(BLACK_BRUSH));
    HGDIOBJ op = U(SelectObject)(m.dc, U(GetStockObject)(NULL_PEN));
    for (int i = 0; i < 4; i++) {
        int mode = (i & 1) ? WINDING : ALTERNATE;
        mdc_clear(&m, WHITE_);
        U(SetPolyFillMode)(m.dc, mode);
        U(Polygon)(m.dc, star, 5);
        int filled = mdc_get(&m, 50, 52) == dca_px(BLACK_);
        CHECKF(filled == (mode == WINDING), "iteration %d mode %d filled %d", i, mode, filled);
    }
    U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, op);
    mdc_free(&m);
}

/* ---------- StretchBltMode ---------- */
typedef struct { HDC dc; HBITMAP bm, old; BYTE *bits; } Mono;
static int mono_make(Mono *m, int w, int h) {
    struct { BITMAPINFOHEADER h; RGBQUAD pal[2]; } bi;
    memset(&bi, 0, sizeof bi);
    bi.h.biSize = sizeof bi.h; bi.h.biWidth = w; bi.h.biHeight = -h; bi.h.biPlanes = 1; bi.h.biBitCount = 1; bi.h.biClrUsed = 2;
    bi.pal[1].rgbRed = bi.pal[1].rgbGreen = bi.pal[1].rgbBlue = 255;
    m->dc = U(CreateCompatibleDC)(NULL);
    if (!m->dc) return 0;
    void *bits = NULL;
    m->bm = U(CreateDIBSection)(m->dc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!m->bm) { U(DeleteDC)(m->dc); return 0; }
    m->bits = bits; m->old = U(SelectObject)(m->dc, m->bm);
    return 1;
}
static void mono_free(Mono *m) { U(SelectObject)(m->dc, m->old); U(DeleteObject)(m->bm); U(DeleteDC)(m->dc); }
static int stretch_mono(int mode, BYTE srcbyte, int srcw, int dstw, BYTE *out) {
    Mono s, d;
    if (!mono_make(&s, 32, 1)) return 0;
    if (!mono_make(&d, 32, 1)) { mono_free(&s); return 0; }
    memset(s.bits, 0, 4); memset(d.bits, 0, 4);
    s.bits[0] = srcbyte;
    U(GdiFlush)();
    int pm = U(SetStretchBltMode)(d.dc, mode);
    (void)pm;
    BOOL ok = U(StretchBlt)(d.dc, 0, 0, dstw, 1, s.dc, 0, 0, srcw, 1, SRCCOPY);
    U(GdiFlush)();
    *out = d.bits[0];
    mono_free(&s); mono_free(&d);
    return ok ? 1 : -1;
}
#define NEEDSTRETCH() do { REQUIRE(U(SetStretchBltMode)); REQUIRE(U(StretchBlt)); REQUIRE(U(CreateDIBSection)); } while (0)
TEST(dca_stretch_mono_blackonwhite_keeps_black) {
    NEEDSTRETCH(); BYTE o; int r = stretch_mono(BLACKONWHITE, 0xAA, 8, 4, &o);
    if (r == 0) { t_skip("setup"); return; }
    CHECK(r == 1); CHECKF((o & 0xF0) == 0x00, "got 0x%02x", o);
}
TEST(dca_stretch_mono_whiteonblack_keeps_white) {
    NEEDSTRETCH(); BYTE o; int r = stretch_mono(WHITEONBLACK, 0xAA, 8, 4, &o);
    if (r == 0) { t_skip("setup"); return; }
    CHECK(r == 1); CHECKF((o & 0xF0) == 0xF0, "got 0x%02x", o);
}
TEST(dca_stretch_mono_blackonwhite_all_white_stays_white) {
    NEEDSTRETCH(); BYTE o; int r = stretch_mono(BLACKONWHITE, 0xFF, 8, 4, &o);
    if (r == 0) { t_skip("setup"); return; }
    CHECK(r == 1); CHECKF((o & 0xF0) == 0xF0, "got 0x%02x", o);
}
TEST(dca_stretch_mono_whiteonblack_all_black_stays_black) {
    NEEDSTRETCH(); BYTE o; int r = stretch_mono(WHITEONBLACK, 0x00, 8, 4, &o);
    if (r == 0) { t_skip("setup"); return; }
    CHECK(r == 1); CHECKF((o & 0xF0) == 0x00, "got 0x%02x", o);
}
TEST(dca_stretch_mono_blackonwhite_pair_pattern) {
    /* source 11 10 01 00 -> AND of each pair: 1 0 0 0 */
    NEEDSTRETCH(); BYTE o; int r = stretch_mono(BLACKONWHITE, 0xE4, 8, 4, &o);
    if (r == 0) { t_skip("setup"); return; }
    CHECK(r == 1); CHECKF((o & 0xF0) == 0x80, "got 0x%02x", o);
}
TEST(dca_stretch_mono_whiteonblack_pair_pattern) {
    /* source 11 10 01 00 -> OR of each pair: 1 1 1 0 */
    NEEDSTRETCH(); BYTE o; int r = stretch_mono(WHITEONBLACK, 0xE4, 8, 4, &o);
    if (r == 0) { t_skip("setup"); return; }
    CHECK(r == 1); CHECKF((o & 0xF0) == 0xE0, "got 0x%02x", o);
}
TEST(dca_stretch_mono_coloroncolor_observed) {
    NEEDSTRETCH(); BYTE o; int r = stretch_mono(COLORONCOLOR, 0xAA, 8, 4, &o);
    if (r == 0) { t_skip("setup"); return; }
    CHECK(r == 1); t_note("COLORONCOLOR 10101010 8->4 = 0x%02x", o);
    CHECKF((o & 0xF0) == 0x00 || (o & 0xF0) == 0xF0, "COLORONCOLOR must pick one pixel per pair, got 0x%02x", o);
}
TEST(dca_stretch_mono_halftone_observed) {
    NEEDSTRETCH(); BYTE o; int r = stretch_mono(HALFTONE, 0xAA, 8, 4, &o);
    if (r == 0) { t_skip("setup"); return; }
    t_note("HALFTONE 10101010 8->4 ret=%d = 0x%02x", r, o);
}
TEST(dca_stretch_mono_modes_independent_of_enlarge) {
    NEEDSTRETCH();
    /* enlarging 4 -> 8: every mode replicates pixels */
    for (int mode = 1; mode <= 3; mode++) {
        BYTE o; int r = stretch_mono(mode, 0xA0, 4, 8, &o);
        if (r == 0) { t_skip("setup"); return; }
        CHECKF(r == 1 && o == 0xCC, "mode %d: 1010 -> 0x%02x (want 0xCC)", mode, o);
    }
}
TEST(dca_stretch_mode_invalid_values_observed) {
    REQUIRE(U(SetStretchBltMode)); REQUIRE(U(GetStretchBltMode));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    static const int bad[] = { 0, 5, 6, 100, -1, INT_MAX, INT_MIN };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        U(SetStretchBltMode)(m.dc, COLORONCOLOR);
        SetLastError(0);
        int r = U(SetStretchBltMode)(m.dc, bad[i]);
        DWORD e = GetLastError();
        t_note("SetStretchBltMode(%d): ret=%d err=%lu Get=%d", bad[i], r, e, U(GetStretchBltMode)(m.dc));
        U(SetStretchBltMode)(m.dc, HALFTONE);
        CHECKF(U(GetStretchBltMode)(m.dc) == HALFTONE, "recover after %d", bad[i]);
    }
    mdc_free(&m);
}
TEST(dca_stretch_mode_legacy_aliases) {
    REQUIRE(U(SetStretchBltMode)); REQUIRE(U(GetStretchBltMode));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    static const int v[] = { STRETCH_ANDSCANS, STRETCH_ORSCANS, STRETCH_DELETESCANS, STRETCH_HALFTONE };
    static const int w[] = { BLACKONWHITE, WHITEONBLACK, COLORONCOLOR, HALFTONE };
    for (int i = 0; i < 4; i++) {
        U(SetStretchBltMode)(m.dc, v[i]);
        CHECKF(U(GetStretchBltMode)(m.dc) == w[i], "alias %d", i);
    }
    mdc_free(&m);
}
TEST(dca_stretch_mode_set_by_stretchdibits_not_changed) {
    REQUIRE(U(SetStretchBltMode)); REQUIRE(U(GetStretchBltMode)); REQUIRE(U(StretchBlt));
    MDC a, b; if (!mdc_make(&a, 16, 16)) { t_skip("mdc"); return; }
    if (!mdc_make(&b, 16, 16)) { mdc_free(&a); t_skip("mdc"); return; }
    U(SetStretchBltMode)(a.dc, HALFTONE);
    U(StretchBlt)(a.dc, 0, 0, 8, 8, b.dc, 0, 0, 16, 16, SRCCOPY);
    EXPECT_EQ(U(GetStretchBltMode)(a.dc), HALFTONE);
    EXPECT_EQ(U(GetStretchBltMode)(b.dc), BLACKONWHITE);
    mdc_free(&a); mdc_free(&b);
}

/* ---------- ArcDirection ---------- */
static int arc_hits(int dir, int cx, int cy, int box) {
    int n = 0;
    (void)dir;
    return cx + cy + box + n;
}
static int near_ink(MDC *m, int px, int py) {
    for (int y = py - 1; y <= py + 1; y++) for (int x = px - 1; x <= px + 1; x++)
        if (mdc_get(m, x, y) == dca_px(BLACK_)) return 1;
    return 0;
}
static void arc_case(int dir, int *tr, int *bl) {
    MDC m; mdc_make(&m, 100, 100);
    mdc_clear(&m, WHITE_);
    HGDIOBJ op = U(SelectObject)(m.dc, U(GetStockObject)(BLACK_PEN));
    U(SetArcDirection)(m.dc, dir);
    CHECK(U(Arc)(m.dc, 10, 10, 90, 90, 90, 50, 50, 10));
    *tr = near_ink(&m, 78, 22);
    *bl = near_ink(&m, 22, 78);
    U(SelectObject)(m.dc, op);
    mdc_free(&m);
}
#define NEEDARC() do { REQUIRE(U(SetArcDirection)); REQUIRE(U(Arc)); REQUIRE(U(GetStockObject)); } while (0)
TEST(dca_arcdir_counterclockwise_draws_short_arc) {
    NEEDARC(); int tr, bl; arc_case(AD_COUNTERCLOCKWISE, &tr, &bl);
    CHECKF(tr == 1 && bl == 0, "top-right %d bottom-left %d", tr, bl);
}
TEST(dca_arcdir_clockwise_draws_long_arc) {
    NEEDARC(); int tr, bl; arc_case(AD_CLOCKWISE, &tr, &bl);
    CHECKF(tr == 0 && bl == 1, "top-right %d bottom-left %d", tr, bl);
}
TEST(dca_arcdir_default_is_counterclockwise_pixels) {
    NEEDARC();
    MDC m; if (!mdc_make(&m, 100, 100)) { t_skip("mdc"); return; }
    mdc_clear(&m, WHITE_);
    HGDIOBJ op = U(SelectObject)(m.dc, U(GetStockObject)(BLACK_PEN));
    U(Arc)(m.dc, 10, 10, 90, 90, 90, 50, 50, 10);
    CHECK(near_ink(&m, 78, 22) && !near_ink(&m, 22, 78));
    U(SelectObject)(m.dc, op);
    mdc_free(&m);
}
TEST(dca_arcdir_switching_between_draws) {
    NEEDARC();
    for (int i = 0; i < 4; i++) {
        int tr, bl, dir = (i & 1) ? AD_CLOCKWISE : AD_COUNTERCLOCKWISE;
        arc_case(dir, &tr, &bl);
        CHECKF(tr == (dir == AD_COUNTERCLOCKWISE) && bl == (dir == AD_CLOCKWISE), "i=%d dir=%d tr=%d bl=%d", i, dir, tr, bl);
    }
}
static void pie_case(int dir, int chord, int *tr, int *bl) {
    MDC m; mdc_make(&m, 100, 100);
    mdc_clear(&m, WHITE_);
    HGDIOBJ ob = U(SelectObject)(m.dc, U(GetStockObject)(BLACK_BRUSH));
    HGDIOBJ op = U(SelectObject)(m.dc, U(GetStockObject)(NULL_PEN));
    U(SetArcDirection)(m.dc, dir);
    if (chord) U(Chord)(m.dc, 10, 10, 90, 90, 90, 50, 50, 10);
    else U(Pie)(m.dc, 10, 10, 90, 90, 90, 50, 50, 10);
    *tr = mdc_get(&m, chord ? 76 : 70, chord ? 24 : 30) == dca_px(BLACK_);
    *bl = mdc_get(&m, 30, 70) == dca_px(BLACK_);
    U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, op);
    mdc_free(&m);
}
TEST(dca_arcdir_pie_counterclockwise) { NEEDARC(); REQUIRE(U(Pie)); int a, b; pie_case(AD_COUNTERCLOCKWISE, 0, &a, &b); CHECKF(a == 1 && b == 0, "%d %d", a, b); }
TEST(dca_arcdir_pie_clockwise) { NEEDARC(); REQUIRE(U(Pie)); int a, b; pie_case(AD_CLOCKWISE, 0, &a, &b); CHECKF(a == 0 && b == 1, "%d %d", a, b); }
TEST(dca_arcdir_chord_counterclockwise) { NEEDARC(); REQUIRE(U(Chord)); int a, b; pie_case(AD_COUNTERCLOCKWISE, 1, &a, &b); CHECKF(a == 1 && b == 0, "%d %d", a, b); }
TEST(dca_arcdir_chord_clockwise) { NEEDARC(); REQUIRE(U(Chord)); int a, b; pie_case(AD_CLOCKWISE, 1, &a, &b); CHECKF(a == 0 && b == 1, "%d %d", a, b); }
TEST(dca_arcdir_invalid_values_observed) {
    REQUIRE(U(SetArcDirection)); REQUIRE(U(GetArcDirection));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    static const int bad[] = { 0, 3, -1, 100, INT_MAX, INT_MIN };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        U(SetArcDirection)(m.dc, AD_CLOCKWISE);
        SetLastError(0);
        int r = U(SetArcDirection)(m.dc, bad[i]);
        DWORD e = GetLastError();
        t_note("SetArcDirection(%d): ret=%d err=%lu Get=%d", bad[i], r, e, U(GetArcDirection)(m.dc));
        U(SetArcDirection)(m.dc, AD_COUNTERCLOCKWISE);
        CHECKF(U(GetArcDirection)(m.dc) == AD_COUNTERCLOCKWISE, "recover after %d", bad[i]);
    }
    mdc_free(&m);
}
TEST(dca_arcdir_arcto_current_position) {
    REQUIRE(U(SetArcDirection)); REQUIRE(U(ArcTo)); REQUIRE(U(GetCurrentPositionEx)); REQUIRE(U(MoveToEx));
    MDC m; if (!mdc_make(&m, 100, 100)) { t_skip("mdc"); return; }
    for (int dir = 1; dir <= 2; dir++) {
        U(SetArcDirection)(m.dc, dir);
        U(MoveToEx)(m.dc, 0, 0, NULL);
        CHECK(U(ArcTo)(m.dc, 10, 10, 90, 90, 90, 50, 50, 10));
        POINT p = { -1, -1 };
        CHECK(U(GetCurrentPositionEx)(m.dc, &p));
        /* the arc ends at the radial of the end point; its end lies on the circle toward (50,10) */
        CHECKF(p.x >= 49 && p.x <= 51 && p.y >= 9 && p.y <= 11, "dir %d: pos (%ld,%ld) not at arc end", dir, p.x, p.y);
    }
    mdc_free(&m);
}

/* ---------- MiterLimit ---------- */
UDECL(int, SetMiterLimitX, (HDC, FLOAT, PFLOAT));
static int miter_tip(float limit) {
    MDC m; if (!mdc_make(&m, 100, 80)) return -1;
    mdc_clear(&m, WHITE_);
    LOGBRUSH lb = { BS_SOLID, BLACK_, 0 };
    HPEN pen = U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID | PS_JOIN_MITER | PS_ENDCAP_FLAT, 10, &lb, 0, NULL);
    HGDIOBJ op = U(SelectObject)(m.dc, pen);
    CHECK(U(SetMiterLimit)(m.dc, limit, NULL));
    POINT pts[3] = { {20, 60}, {50, 20}, {80, 60} };
    CHECK(U(Polyline)(m.dc, pts, 3));
    int tip = mdc_get(&m, 50, 14) == dca_px(BLACK_);
    int body = mdc_get(&m, 50, 20) == dca_px(BLACK_);
    CHECK(body);
    U(SelectObject)(m.dc, op); U(DeleteObject)(pen);
    mdc_free(&m);
    return tip;
}
#define NEEDMITER() do { REQUIRE(U(SetMiterLimit)); REQUIRE(U(GetMiterLimit)); } while (0)
TEST(dca_miter_default_is_ten) {
    NEEDMITER();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    float f = -1;
    CHECK(U(GetMiterLimit)(m.dc, &f));
    CHECKF(f == 10.0f, "default miter limit %g", f);
    mdc_free(&m);
}
TEST(dca_miter_set_returns_previous) {
    NEEDMITER();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    float old = -1;
    CHECK(U(SetMiterLimit)(m.dc, 4.0f, &old));
    CHECKF(old == 10.0f, "old %g", old);
    CHECK(U(SetMiterLimit)(m.dc, 1.0f, &old));
    CHECKF(old == 4.0f, "old %g", old);
    CHECK(U(SetMiterLimit)(m.dc, 10.0f, &old));
    CHECKF(old == 1.0f, "old %g", old);
    mdc_free(&m);
}
TEST(dca_miter_null_old_pointer_allowed) {
    NEEDMITER();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    CHECK(U(SetMiterLimit)(m.dc, 3.5f, NULL));
    float f = -1; CHECK(U(GetMiterLimit)(m.dc, &f));
    CHECKF(f == 3.5f, "got %g", f);
    mdc_free(&m);
}
TEST(dca_miter_value_table_roundtrip) {
    NEEDMITER();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    static const float v[] = { 1.0f, 1.5f, 2.0f, 5.0f, 10.0f, 100.0f, 1000.25f, 1.0001f };
    float last = 10.0f;
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        float old = -1, g = -1;
        CHECK(U(SetMiterLimit)(m.dc, v[i], &old));
        CHECKF(old == last, "i=%u old %g want %g", i, old, last);
        CHECK(U(GetMiterLimit)(m.dc, &g));
        CHECKF(g == v[i], "i=%u get %g want %g", i, g, v[i]);
        last = v[i];
    }
    mdc_free(&m);
}
TEST(dca_miter_below_one_observed) {
    NEEDMITER();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    static const float v[] = { 0.0f, 0.5f, 0.9999f, -1.0f, -100.0f };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        U(SetMiterLimit)(m.dc, 5.0f, NULL);
        SetLastError(0);
        float old = -7;
        BOOL ok = U(SetMiterLimit)(m.dc, v[i], &old);
        DWORD e = GetLastError();
        float g = -7; U(GetMiterLimit)(m.dc, &g);
        t_note("SetMiterLimit(%g): ok=%d err=%lu old=%g now=%g", v[i], ok, e, old, g);
    }
    mdc_free(&m);
}
TEST(dca_miter_special_floats_observed) {
    NEEDMITER();
    DCA_WINE_SKIP("non-finite miter values are not exercised under the emulator");
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    union { DWORD u; float f; } nan = { 0x7FC00000 }, inf = { 0x7F800000 }, ninf = { 0xFF800000 };
    const float v[] = { nan.f, inf.f, ninf.f };
    for (int i = 0; i < 3; i++) {
        U(SetMiterLimit)(m.dc, 5.0f, NULL);
        BOOL ok = U(SetMiterLimit)(m.dc, v[i], NULL);
        float g = -7; U(GetMiterLimit)(m.dc, &g);
        t_note("SetMiterLimit(special %d): ok=%d now=%g", i, ok, g);
        U(SetMiterLimit)(m.dc, 10.0f, NULL);
    }
    mdc_free(&m);
}
TEST(dca_miter_bad_dc) {
    NEEDMITER();
    float f = -1;
    SetLastError(0);
    CHECK(!U(SetMiterLimit)(NULL, 5.0f, &f));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SetLastError(0);
    CHECK(!U(GetMiterLimit)(NULL, &f));
    t_note("GetMiterLimit(NULL) err=%lu", GetLastError());
    for (int i = 0; i < DCA_NGARBAGE; i++) {
        CHECKF(!U(SetMiterLimit)((HDC)dca_garbage[i], 5.0f, &f), "set garbage %d", i);
        CHECKF(!U(GetMiterLimit)((HDC)dca_garbage[i], &f), "get garbage %d", i);
    }
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    HDC d = m.dc; mdc_free(&m);
    SetLastError(0);
    CHECK(!U(SetMiterLimit)(d, 5.0f, &f));
    t_note("SetMiterLimit(stale) err=%lu", GetLastError());
    CHECK(!U(GetMiterLimit)(d, &f));
    WrongSet w; dca_wrong_make(&w);
    for (int i = 0; i < w.n; i++) if (w.h[i]) {
        CHECKF(!U(SetMiterLimit)((HDC)w.h[i], 5.0f, &f), "set %s", w.nm[i]);
        CHECKF(!U(GetMiterLimit)((HDC)w.h[i], &f), "get %s", w.nm[i]);
    }
    dca_wrong_free(&w);
}
TEST(dca_miter_get_null_pointer) {
    NEEDMITER();
    DCA_WINE_SKIP("NULL out pointer faults in the emulator");
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    SetLastError(0);
    BOOL ok = U(GetMiterLimit)(m.dc, NULL);
    t_note("GetMiterLimit(dc, NULL): ok=%d err=%lu", ok, GetLastError());
    mdc_free(&m);
}
TEST(dca_miter_isolated_per_dc) {
    NEEDMITER();
    MDC a, b; if (!mdc_make(&a, 8, 8)) { t_skip("mdc"); return; }
    if (!mdc_make(&b, 8, 8)) { mdc_free(&a); t_skip("mdc"); return; }
    U(SetMiterLimit)(a.dc, 3.0f, NULL);
    float f = -1;
    CHECK(U(GetMiterLimit)(b.dc, &f));
    CHECKF(f == 10.0f, "other DC %g", f);
    mdc_free(&a); mdc_free(&b);
}
TEST(dca_miter_savedc_observed) {
    NEEDMITER();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    int s = U(SaveDC)(m.dc);
    U(SetMiterLimit)(m.dc, 3.0f, NULL);
    U(RestoreDC)(m.dc, s);
    float f = -1; U(GetMiterLimit)(m.dc, &f);
    t_note("miter limit after RestoreDC: %g (10 = restored)", f);
    mdc_free(&m);
}
TEST(dca_miter_pixels_bevel_when_limit_low) {
    NEEDMITER(); REQUIRE(U(ExtCreatePen)); REQUIRE(U(Polyline));
    int r = miter_tip(1.5f); if (r < 0) { t_skip("mdc"); return; }
    CHECKF(r == 0, "tip drawn with limit 1.5 (ratio 1.67 exceeds the limit)");
}
TEST(dca_miter_pixels_miter_when_limit_high) {
    NEEDMITER(); REQUIRE(U(ExtCreatePen)); REQUIRE(U(Polyline));
    int r = miter_tip(2.0f); if (r < 0) { t_skip("mdc"); return; }
    CHECKF(r == 1, "tip missing with limit 2.0");
}
TEST(dca_miter_pixels_default_limit_mitres) {
    REQUIRE(U(ExtCreatePen)); REQUIRE(U(Polyline)); REQUIRE(U(SetMiterLimit));
    int r = miter_tip(10.0f); if (r < 0) { t_skip("mdc"); return; }
    CHECK(r == 1);
}
TEST(dca_miter_pixels_limit_one_always_bevels) {
    NEEDMITER(); REQUIRE(U(ExtCreatePen)); REQUIRE(U(Polyline));
    int r = miter_tip(1.0f); if (r < 0) { t_skip("mdc"); return; }
    CHECK(r == 0);
}
TEST(dca_miter_pixels_change_between_draws) {
    NEEDMITER(); REQUIRE(U(ExtCreatePen)); REQUIRE(U(Polyline));
    for (int i = 0; i < 4; i++) {
        float lim = (i & 1) ? 1.2f : 3.0f;
        int r = miter_tip(lim); if (r < 0) { t_skip("mdc"); return; }
        CHECKF(r == !(i & 1), "i=%d limit %g tip=%d", i, lim, r);
    }
}
TEST(dca_miter_ignored_by_cosmetic_pen) {
    NEEDMITER();
    MDC m; if (!mdc_make(&m, 100, 80)) { t_skip("mdc"); return; }
    mdc_clear(&m, WHITE_);
    HGDIOBJ op = U(SelectObject)(m.dc, U(GetStockObject)(BLACK_PEN));
    U(SetMiterLimit)(m.dc, 1.0f, NULL);
    POINT pts[3] = { {20, 60}, {50, 20}, {80, 60} };
    U(Polyline)(m.dc, pts, 3);
    CHECK(mdc_get(&m, 50, 20) == dca_px(BLACK_));
    U(SelectObject)(m.dc, op);
    mdc_free(&m);
}

/* ---------- RelAbs ---------- */
TEST(dca_relabs_constants_and_default) {
    REQUIRE(UN(GetRelAbs));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    EXPECT_EQ(UN(GetRelAbs)(m.dc, 0), ABSOLUTE);
    mdc_free(&m);
}
TEST(dca_relabs_set_returns_previous) {
    REQUIRE(UN(GetRelAbs)); REQUIRE(UN(SetRelAbs));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    int p = UN(SetRelAbs)(m.dc, RELATIVE);
    CHECKF(p == ABSOLUTE, "prev %d", p);
    EXPECT_EQ(UN(GetRelAbs)(m.dc, 0), RELATIVE);
    p = UN(SetRelAbs)(m.dc, ABSOLUTE);
    CHECKF(p == RELATIVE, "prev %d", p);
    mdc_free(&m);
}
TEST(dca_relabs_invalid_values_observed) {
    REQUIRE(UN(GetRelAbs)); REQUIRE(UN(SetRelAbs));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    static const int bad[] = { 0, 3, -1, 100 };
    for (unsigned i = 0; i < 4; i++) {
        UN(SetRelAbs)(m.dc, ABSOLUTE);
        SetLastError(0);
        int r = UN(SetRelAbs)(m.dc, bad[i]);
        t_note("SetRelAbs(%d): ret=%d err=%lu Get=%d", bad[i], r, GetLastError(), UN(GetRelAbs)(m.dc, 0));
        UN(SetRelAbs)(m.dc, ABSOLUTE);
        CHECKF(UN(GetRelAbs)(m.dc, 0) == ABSOLUTE, "recover after %d", bad[i]);
    }
    mdc_free(&m);
}
TEST(dca_relabs_does_not_change_lineto) {
    REQUIRE(UN(SetRelAbs)); REQUIRE(U(LineTo)); REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    MDC m; if (!mdc_make(&m, 64, 64)) { t_skip("mdc"); return; }
    UN(SetRelAbs)(m.dc, RELATIVE);
    U(MoveToEx)(m.dc, 10, 10, NULL);
    U(LineTo)(m.dc, 30, 20);
    POINT p; U(GetCurrentPositionEx)(m.dc, &p);
    CHECKF(p.x == 30 && p.y == 20, "pos (%ld,%ld)", p.x, p.y);
    mdc_free(&m);
}
