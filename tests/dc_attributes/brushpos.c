/* Brush origin, DC brush/pen colour, current position, and default selected objects. */
#include "dca.h"

static const COLORREF WH = RGB(255, 255, 255), BK = RGB(0, 0, 0), RD = RGB(255, 0, 0), BL = RGB(0, 0, 255);

/* ---------- brush origin ---------- */
#define NEEDORG() do { REQUIRE(U(SetBrushOrgEx)); REQUIRE(U(GetBrushOrgEx)); } while (0)

TEST(dca_brushorg_default_zero) {
    NEEDORG();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    POINT p = { 77, 77 };
    CHECK(U(GetBrushOrgEx)(m.dc, &p));
    CHECKF(p.x == 0 && p.y == 0, "(%ld,%ld)", p.x, p.y);
    HDC s = dca_screen();
    if (s) { p.x = p.y = 77; CHECK(U(GetBrushOrgEx)(s, &p)); CHECKF(p.x == 0 && p.y == 0, "screen (%ld,%ld)", p.x, p.y); dca_unscreen(s); }
    mdc_free(&m);
}
TEST(dca_brushorg_roundtrip_table) {
    NEEDORG();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    static const POINT v[] = { {1, 1}, {7, 7}, {8, 8}, {9, 15}, {100, 200}, {-1, -1}, {-5, -13}, {1000, -1000}, {65535, 65536}, {0, 0}, {3, 0}, {0, 3} };
    POINT last = { 0, 0 };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        POINT old = { -99, -99 }, g = { -99, -99 };
        CHECK(U(SetBrushOrgEx)(m.dc, v[i].x, v[i].y, &old));
        CHECKF(old.x == last.x && old.y == last.y, "i=%u old (%ld,%ld) want (%ld,%ld)", i, old.x, old.y, last.x, last.y);
        CHECK(U(GetBrushOrgEx)(m.dc, &g));
        CHECKF(g.x == v[i].x && g.y == v[i].y, "i=%u get (%ld,%ld) want (%ld,%ld)", i, g.x, g.y, v[i].x, v[i].y);
        last = v[i];
    }
    mdc_free(&m);
}
TEST(dca_brushorg_extremes_observed) {
    NEEDORG();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    static const POINT v[] = { {INT_MAX, INT_MIN}, {INT_MIN, INT_MAX}, {0x7FFF, 0x8000}, {-0x8000, -0x8001} };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        POINT g = { 1, 1 };
        BOOL ok = U(SetBrushOrgEx)(m.dc, v[i].x, v[i].y, NULL);
        U(GetBrushOrgEx)(m.dc, &g);
        t_note("SetBrushOrgEx(%d,%d): ok=%d -> (%ld,%ld)", v[i].x, v[i].y, ok, g.x, g.y);
    }
    U(SetBrushOrgEx)(m.dc, 0, 0, NULL);
    mdc_free(&m);
}
TEST(dca_brushorg_null_old_pointer) {
    NEEDORG();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    CHECK(U(SetBrushOrgEx)(m.dc, 4, 5, NULL));
    POINT g; CHECK(U(GetBrushOrgEx)(m.dc, &g));
    CHECKF(g.x == 4 && g.y == 5, "(%ld,%ld)", g.x, g.y);
    CHECK(U(SetBrushOrgEx)(m.dc, 4, 5, &g));             /* same value */
    CHECKF(g.x == 4 && g.y == 5, "old (%ld,%ld)", g.x, g.y);
    mdc_free(&m);
}
TEST(dca_brushorg_get_null_point_fails) {
    NEEDORG();
    DCA_WINE_SKIP("NULL out pointer faults in the emulator");
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    SetLastError(0);
    CHECK(!U(GetBrushOrgEx)(m.dc, NULL));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    mdc_free(&m);
}
TEST(dca_brushorg_bad_dc_set) {
    NEEDORG();
    POINT p = { 5, 5 };
    SetLastError(0);
    CHECK(!U(SetBrushOrgEx)(NULL, 1, 1, &p));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    CHECKF(p.x == 5 && p.y == 5, "old point written for NULL DC (%ld,%ld)", p.x, p.y);
    for (int i = 0; i < DCA_NGARBAGE; i++) CHECKF(!U(SetBrushOrgEx)((HDC)dca_garbage[i], 1, 1, &p), "garbage %d", i);
    WrongSet w; dca_wrong_make(&w);
    for (int i = 0; i < w.n; i++) if (w.h[i]) CHECKF(!U(SetBrushOrgEx)((HDC)w.h[i], 1, 1, &p), "%s", w.nm[i]);
    dca_wrong_free(&w);
}
TEST(dca_brushorg_bad_dc_get) {
    NEEDORG();
    POINT p = { 5, 5 };
    SetLastError(0);
    CHECK(!U(GetBrushOrgEx)(NULL, &p));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    CHECKF(p.x == 5 && p.y == 5, "point written for NULL DC (%ld,%ld)", p.x, p.y);
    for (int i = 0; i < DCA_NGARBAGE; i++) {
        SetLastError(0);
        CHECKF(!U(GetBrushOrgEx)((HDC)dca_garbage[i], &p), "garbage %d", i);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "garbage %d err %lu", i, GetLastError());
    }
    WrongSet w; dca_wrong_make(&w);
    for (int i = 0; i < w.n; i++) if (w.h[i]) CHECKF(!U(GetBrushOrgEx)((HDC)w.h[i], &p), "%s", w.nm[i]);
    dca_wrong_free(&w);
}
TEST(dca_brushorg_stale_dc) {
    NEEDORG();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    HDC d = m.dc; mdc_free(&m);
    POINT p = { 5, 5 };
    SetLastError(0);
    CHECK(!U(GetBrushOrgEx)(d, &p));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(!U(SetBrushOrgEx)(d, 1, 1, &p));
    t_note("SetBrushOrgEx(stale) err=%lu", GetLastError());
}
TEST(dca_brushorg_isolation) {
    NEEDORG();
    MDC a, b; if (!mdc_make(&a, 8, 8)) { t_skip("mdc"); return; }
    if (!mdc_make(&b, 8, 8)) { mdc_free(&a); t_skip("mdc"); return; }
    U(SetBrushOrgEx)(a.dc, 3, 4, NULL);
    POINT p; U(GetBrushOrgEx)(b.dc, &p);
    CHECKF(p.x == 0 && p.y == 0, "other (%ld,%ld)", p.x, p.y);
    U(SetBrushOrgEx)(b.dc, 6, 7, NULL);
    U(GetBrushOrgEx)(a.dc, &p);
    CHECKF(p.x == 3 && p.y == 4, "first (%ld,%ld)", p.x, p.y);
    mdc_free(&a); mdc_free(&b);
}
TEST(dca_brushorg_not_changed_by_select_or_mapping) {
    NEEDORG(); REQUIRE(U(SetMapMode)); REQUIRE(U(SetViewportOrgEx)); REQUIRE(U(SetWindowOrgEx));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    U(SetBrushOrgEx)(m.dc, 3, 4, NULL);
    HBRUSH b = U(CreateSolidBrush)(0x112233);
    HGDIOBJ ob = U(SelectObject)(m.dc, b);
    POINT p;
    U(GetBrushOrgEx)(m.dc, &p); CHECKF(p.x == 3 && p.y == 4, "after select (%ld,%ld)", p.x, p.y);
    U(SetMapMode)(m.dc, MM_LOMETRIC);
    U(GetBrushOrgEx)(m.dc, &p); CHECKF(p.x == 3 && p.y == 4, "after SetMapMode (%ld,%ld)", p.x, p.y);
    U(SetViewportOrgEx)(m.dc, 10, 10, NULL);
    U(SetWindowOrgEx)(m.dc, 20, 20, NULL);
    U(GetBrushOrgEx)(m.dc, &p); CHECKF(p.x == 3 && p.y == 4, "after org changes (%ld,%ld)", p.x, p.y);
    U(SetMapMode)(m.dc, MM_TEXT);
    U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    U(GetBrushOrgEx)(m.dc, &p); CHECKF(p.x == 3 && p.y == 4, "after delete (%ld,%ld)", p.x, p.y);
    mdc_free(&m);
}
TEST(dca_brushorg_unrealizeobject_leaves_origin) {
    NEEDORG(); REQUIRE(U(UnrealizeObject));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    HBRUSH b = U(CreateSolidBrush)(0x112233);
    U(SetBrushOrgEx)(m.dc, 6, 2, NULL);
    CHECK(U(UnrealizeObject)(b));
    POINT p; U(GetBrushOrgEx)(m.dc, &p);
    CHECKF(p.x == 6 && p.y == 2, "(%ld,%ld)", p.x, p.y);
    U(DeleteObject)(b);
    mdc_free(&m);
}
TEST(dca_brushorg_unrealizeobject_object_kinds) {
    REQUIRE(U(UnrealizeObject));
    HBRUSH b = U(CreateSolidBrush)(0x112233);
    HBRUSH pb = U(CreatePatternBrush)(U(CreateBitmap)(8, 8, 1, 1, NULL));
    HPEN pen = U(CreatePen)(PS_SOLID, 1, 0);
    HFONT f = U(CreateFontA)(12, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, "Arial");
    HRGN r = U(CreateRectRgn)(0, 0, 2, 2);
    HBITMAP bm = U(CreateBitmap)(2, 2, 1, 1, NULL);
    SetLastError(0);
    CHECK(U(UnrealizeObject)(b));
    CHECK(pb != NULL && U(UnrealizeObject)(pb));
    t_note("UnrealizeObject(pen)=%d", U(UnrealizeObject)(pen));
    t_note("UnrealizeObject(font)=%d", U(UnrealizeObject)(f));
    t_note("UnrealizeObject(region)=%d", U(UnrealizeObject)(r));
    t_note("UnrealizeObject(bitmap)=%d", U(UnrealizeObject)(bm));
    t_note("UnrealizeObject(stock brush)=%d", U(UnrealizeObject)(U(GetStockObject)(WHITE_BRUSH)));
    t_note("UnrealizeObject(stock pen)=%d", U(UnrealizeObject)(U(GetStockObject)(BLACK_PEN)));
    CHECK(!U(UnrealizeObject)(NULL));
    CHECK(!U(UnrealizeObject)((HGDIOBJ)(ULONG_PTR)0xDEADBEEF));
    U(DeleteObject)(b); U(DeleteObject)(pen); U(DeleteObject)(f); U(DeleteObject)(r); U(DeleteObject)(bm);
    if (pb) { HGDIOBJ tmp = pb; U(DeleteObject)(tmp); }
    CHECK(!U(UnrealizeObject)(b));            /* deleted */
}
TEST(dca_brushorg_unrealizeobject_palette) {
    REQUIRE(U(UnrealizeObject)); REQUIRE(U(CreatePalette));
    struct { WORD v, n; PALETTEENTRY e[2]; } lp = { 0x300, 2, { {1,2,3,0}, {4,5,6,0} } };
    HPALETTE p = U(CreatePalette)((LOGPALETTE *)&lp);
    CHECK(p != NULL);
    if (p) { CHECK(U(UnrealizeObject)(p)); U(DeleteObject)(p); }
}

static HBRUSH pattern_brush_marker(void) {
    struct { BITMAPINFOHEADER h; DWORD px[64]; } d;
    memset(&d, 0, sizeof d);
    d.h.biSize = sizeof d.h; d.h.biWidth = 8; d.h.biHeight = 8; d.h.biPlanes = 1; d.h.biBitCount = 32; d.h.biCompression = BI_RGB;
    for (int i = 0; i < 64; i++) d.px[i] = dca_px(BL);
    d.px[7 * 8 + 0] = dca_px(RD);              /* bottom-up: last stored row is the top row */
    return U(CreateDIBPatternBrushPt)(&d, DIB_RGB_COLORS);
}
static int org_pattern_mismatches(int ox, int oy) {
    MDC m; if (!mdc_make(&m, 24, 24)) return -1;
    mdc_clear(&m, WH);
    HBRUSH b = pattern_brush_marker();
    if (!b) { mdc_free(&m); return -2; }
    U(SetBrushOrgEx)(m.dc, ox, oy, NULL);
    HGDIOBJ ob = U(SelectObject)(m.dc, b);
    U(PatBlt)(m.dc, 0, 0, 24, 24, PATCOPY);
    int bad = 0;
    for (int y = 0; y < 24; y++) for (int x = 0; x < 24; x++) {
        int mark = (((x - ox) & 7) == 0) && (((y - oy) & 7) == 0);
        DWORD want = mark ? dca_px(RD) : dca_px(BL);
        if (mdc_get(&m, x, y) != want) bad++;
    }
    U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    mdc_free(&m);
    return bad;
}
#define ORGCASE(nm, ox, oy) TEST(dca_brushorg_pattern_pixels_##nm) { \
    REQUIRE(U(CreateDIBPatternBrushPt)); REQUIRE(U(SetBrushOrgEx)); REQUIRE(U(PatBlt)); \
    int r = org_pattern_mismatches(ox, oy); \
    if (r < 0) { t_skip("setup failed (%d)", r); return; } \
    CHECKF(r == 0, "org (%d,%d): %d pixels differ", ox, oy, r); }
ORGCASE(0_0, 0, 0) ORGCASE(3_2, 3, 2) ORGCASE(7_7, 7, 7) ORGCASE(8_8, 8, 8) ORGCASE(11_10, 11, 10)
ORGCASE(neg5_neg3, -5, -3) ORGCASE(100_201, 100, 201) ORGCASE(1_0, 1, 0) ORGCASE(0_5, 0, 5) ORGCASE(neg8_16, -8, 16)

TEST(dca_brushorg_pattern_changes_between_fills) {
    REQUIRE(U(CreateDIBPatternBrushPt)); REQUIRE(U(SetBrushOrgEx)); REQUIRE(U(PatBlt));
    MDC m; if (!mdc_make(&m, 24, 24)) { t_skip("mdc"); return; }
    HBRUSH b = pattern_brush_marker();
    if (!b) { mdc_free(&m); t_skip("pattern brush"); return; }
    HGDIOBJ ob = U(SelectObject)(m.dc, b);
    for (int i = 0; i < 3; i++) {
        int ox = i * 2 + 1, oy = i + 3;
        mdc_clear(&m, WH);
        U(SetBrushOrgEx)(m.dc, ox, oy, NULL);
        U(UnrealizeObject)(b);
        U(SelectObject)(m.dc, b);
        U(PatBlt)(m.dc, 0, 0, 24, 24, PATCOPY);
        CHECKF(mdc_get(&m, ox, oy) == dca_px(RD) && mdc_get(&m, ox + 8, oy + 8) == dca_px(RD) && mdc_get(&m, ox + 1, oy) == dca_px(BL),
               "i=%d marker not at (%d,%d)", i, ox, oy);
    }
    U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    mdc_free(&m);
}

/* ---------- DC brush / DC pen colour ---------- */
TEST(dca_dcbrush_pixels_follow_color) {
    REQUIRE(U(SetDCBrushColor)); REQUIRE(U(GetStockObject)); REQUIRE(U(PatBlt));
    MDC m; if (!mdc_make(&m, 16, 16)) { t_skip("mdc"); return; }
    HGDIOBJ ob = U(SelectObject)(m.dc, U(GetStockObject)(DC_BRUSH));
    static const COLORREF c[] = { RGB(255, 0, 0), RGB(0, 255, 0), RGB(1, 2, 3), RGB(250, 251, 252), 0 };
    for (unsigned i = 0; i < sizeof c / sizeof c[0]; i++) {
        U(SetDCBrushColor)(m.dc, c[i]);
        U(PatBlt)(m.dc, 0, 0, 16, 16, PATCOPY);
        CHECKF(mdc_get(&m, 5, 5) == dca_px(c[i]), "i=%u got 0x%06lx", i, mdc_get(&m, 5, 5));
    }
    U(SelectObject)(m.dc, ob);
    mdc_free(&m);
}
TEST(dca_dcbrush_default_white_fill) {
    REQUIRE(U(GetStockObject)); REQUIRE(U(PatBlt));
    MDC m; if (!mdc_make(&m, 16, 16)) { t_skip("mdc"); return; }
    mdc_clear(&m, BK);
    HGDIOBJ ob = U(SelectObject)(m.dc, U(GetStockObject)(DC_BRUSH));
    U(PatBlt)(m.dc, 0, 0, 16, 16, PATCOPY);
    CHECKF(mdc_get(&m, 5, 5) == 0xFFFFFF, "got 0x%06lx", mdc_get(&m, 5, 5));
    U(SelectObject)(m.dc, ob);
    mdc_free(&m);
}
TEST(dca_dcbrush_color_ignored_by_other_brushes) {
    REQUIRE(U(SetDCBrushColor)); REQUIRE(U(PatBlt)); REQUIRE(U(CreateSolidBrush));
    MDC m; if (!mdc_make(&m, 16, 16)) { t_skip("mdc"); return; }
    U(SetDCBrushColor)(m.dc, RD);
    HBRUSH b = U(CreateSolidBrush)(BL);
    HGDIOBJ ob = U(SelectObject)(m.dc, b);
    U(PatBlt)(m.dc, 0, 0, 16, 16, PATCOPY);
    CHECK(mdc_get(&m, 5, 5) == dca_px(BL));
    U(SelectObject)(m.dc, U(GetStockObject)(DC_BRUSH));
    U(PatBlt)(m.dc, 0, 0, 16, 16, PATCOPY);
    CHECK(mdc_get(&m, 5, 5) == dca_px(RD));
    U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    mdc_free(&m);
}
TEST(dca_dcbrush_color_persists_across_brush_switch) {
    REQUIRE(U(SetDCBrushColor)); REQUIRE(U(GetDCBrushColor));
    MDC m; if (!mdc_make(&m, 16, 16)) { t_skip("mdc"); return; }
    U(SetDCBrushColor)(m.dc, 0x445566);
    HGDIOBJ db = U(GetStockObject)(DC_BRUSH);
    HGDIOBJ o1 = U(SelectObject)(m.dc, db);
    U(SelectObject)(m.dc, U(GetStockObject)(BLACK_BRUSH));
    U(SelectObject)(m.dc, db);
    CHECK(U(GetDCBrushColor)(m.dc) == 0x445566);
    HGDIOBJ cur = U(GetCurrentObject)(m.dc, OBJ_BRUSH);
    CHECK(cur == db);
    U(SelectObject)(m.dc, o1);
    mdc_free(&m);
}
TEST(dca_dcbrush_color_per_dc_pixels) {
    REQUIRE(U(SetDCBrushColor)); REQUIRE(U(PatBlt));
    MDC a, b; if (!mdc_make(&a, 8, 8)) { t_skip("mdc"); return; }
    if (!mdc_make(&b, 8, 8)) { mdc_free(&a); t_skip("mdc"); return; }
    HGDIOBJ oa = U(SelectObject)(a.dc, U(GetStockObject)(DC_BRUSH)), ob = U(SelectObject)(b.dc, U(GetStockObject)(DC_BRUSH));
    U(SetDCBrushColor)(a.dc, RD); U(SetDCBrushColor)(b.dc, BL);
    U(PatBlt)(a.dc, 0, 0, 8, 8, PATCOPY); U(PatBlt)(b.dc, 0, 0, 8, 8, PATCOPY);
    CHECK(mdc_get(&a, 3, 3) == dca_px(RD) && mdc_get(&b, 3, 3) == dca_px(BL));
    U(SelectObject)(a.dc, oa); U(SelectObject)(b.dc, ob);
    mdc_free(&a); mdc_free(&b);
}
TEST(dca_dcbrush_clr_invalid_observed) {
    REQUIRE(U(SetDCBrushColor)); REQUIRE(U(GetDCBrushColor));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    U(SetDCBrushColor)(m.dc, 0x010203);
    SetLastError(0);
    COLORREF r = U(SetDCBrushColor)(m.dc, CLR_INVALID);
    t_note("SetDCBrushColor(CLR_INVALID): ret=0x%08lx Get=0x%08lx err=%lu", r, U(GetDCBrushColor)(m.dc), GetLastError());
    CHECK(r == 0x010203);
    U(SetDCBrushColor)(m.dc, 0xFFFFFF);
    mdc_free(&m);
}
TEST(dca_dcpen_pixels_follow_color) {
    REQUIRE(U(SetDCPenColor)); REQUIRE(U(GetStockObject)); REQUIRE(U(MoveToEx)); REQUIRE(U(LineTo));
    MDC m; if (!mdc_make(&m, 40, 16)) { t_skip("mdc"); return; }
    mdc_clear(&m, WH);
    HGDIOBJ op = U(SelectObject)(m.dc, U(GetStockObject)(DC_PEN));
    static const COLORREF c[] = { RGB(255, 0, 0), RGB(0, 200, 0), RGB(9, 8, 7), 0 };
    for (unsigned i = 0; i < 4; i++) {
        U(SetDCPenColor)(m.dc, c[i]);
        U(MoveToEx)(m.dc, 2, 3 + i * 3, NULL);
        U(LineTo)(m.dc, 30, 3 + i * 3);
        CHECKF(mdc_get(&m, 10, 3 + i * 3) == dca_px(c[i]), "i=%u got 0x%06lx", i, mdc_get(&m, 10, 3 + i * 3));
    }
    U(SelectObject)(m.dc, op);
    mdc_free(&m);
}
TEST(dca_dcpen_default_black) {
    REQUIRE(U(GetStockObject)); REQUIRE(U(MoveToEx)); REQUIRE(U(LineTo));
    MDC m; if (!mdc_make(&m, 40, 16)) { t_skip("mdc"); return; }
    mdc_clear(&m, WH);
    HGDIOBJ op = U(SelectObject)(m.dc, U(GetStockObject)(DC_PEN));
    U(MoveToEx)(m.dc, 2, 3, NULL); U(LineTo)(m.dc, 30, 3);
    CHECKF(mdc_get(&m, 10, 3) == 0, "got 0x%06lx", mdc_get(&m, 10, 3));
    U(SelectObject)(m.dc, op);
    mdc_free(&m);
}
TEST(dca_dcpen_does_not_affect_other_pens) {
    REQUIRE(U(SetDCPenColor)); REQUIRE(U(CreatePen)); REQUIRE(U(MoveToEx)); REQUIRE(U(LineTo));
    MDC m; if (!mdc_make(&m, 40, 16)) { t_skip("mdc"); return; }
    mdc_clear(&m, WH);
    U(SetDCPenColor)(m.dc, RD);
    HPEN p = U(CreatePen)(PS_SOLID, 1, BL);
    HGDIOBJ op = U(SelectObject)(m.dc, p);
    U(MoveToEx)(m.dc, 2, 3, NULL); U(LineTo)(m.dc, 30, 3);
    CHECK(mdc_get(&m, 10, 3) == dca_px(BL));
    U(SelectObject)(m.dc, op); U(DeleteObject)(p);
    mdc_free(&m);
}
TEST(dca_dcpen_and_brush_colors_independent) {
    REQUIRE(U(SetDCPenColor)); REQUIRE(U(SetDCBrushColor)); REQUIRE(U(GetDCPenColor)); REQUIRE(U(GetDCBrushColor));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    U(SetDCPenColor)(m.dc, 0x111111); U(SetDCBrushColor)(m.dc, 0x222222);
    CHECK(U(GetDCPenColor)(m.dc) == 0x111111 && U(GetDCBrushColor)(m.dc) == 0x222222);
    U(SetBkColor)(m.dc, 0x333333); U(SetTextColor)(m.dc, 0x444444);
    CHECK(U(GetDCPenColor)(m.dc) == 0x111111 && U(GetDCBrushColor)(m.dc) == 0x222222);
    mdc_free(&m);
}
TEST(dca_dcpen_rectangle_uses_both) {
    REQUIRE(U(SetDCPenColor)); REQUIRE(U(SetDCBrushColor)); REQUIRE(U(Rectangle));
    MDC m; if (!mdc_make(&m, 32, 32)) { t_skip("mdc"); return; }
    mdc_clear(&m, WH);
    HGDIOBJ op = U(SelectObject)(m.dc, U(GetStockObject)(DC_PEN)), ob = U(SelectObject)(m.dc, U(GetStockObject)(DC_BRUSH));
    U(SetDCPenColor)(m.dc, RD); U(SetDCBrushColor)(m.dc, BL);
    CHECK(U(Rectangle)(m.dc, 4, 4, 20, 20));
    CHECKF(mdc_get(&m, 4, 10) == dca_px(RD), "border 0x%06lx", mdc_get(&m, 4, 10));
    CHECKF(mdc_get(&m, 10, 10) == dca_px(BL), "fill 0x%06lx", mdc_get(&m, 10, 10));
    CHECKF(mdc_get(&m, 25, 25) == dca_px(WH), "outside");
    U(SelectObject)(m.dc, op); U(SelectObject)(m.dc, ob);
    mdc_free(&m);
}

/* ---------- default selected objects ---------- */
TEST(dca_defaults_selected_pen_and_brush_are_stock) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(U(GetStockObject));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    CHECK(U(GetCurrentObject)(m.dc, OBJ_PEN) == U(GetStockObject)(BLACK_PEN));
    CHECK(U(GetCurrentObject)(m.dc, OBJ_BRUSH) == U(GetStockObject)(WHITE_BRUSH));
    HGDIOBJ f = U(GetCurrentObject)(m.dc, OBJ_FONT);
    t_note("default font is SYSTEM_FONT stock: %d, DEFAULT_GUI_FONT: %d", f == U(GetStockObject)(SYSTEM_FONT), f == U(GetStockObject)(DEFAULT_GUI_FONT));
    CHECK(f != NULL);
    HGDIOBJ pal = U(GetCurrentObject)(m.dc, OBJ_PAL);
    t_note("default palette is DEFAULT_PALETTE: %d", pal == U(GetStockObject)(DEFAULT_PALETTE));
    mdc_free(&m);
}
TEST(dca_defaults_selected_objects_screen_dc) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(U(GetStockObject));
    HDC s = dca_screen(); if (!s) { t_skip("no screen"); return; }
    CHECK(U(GetCurrentObject)(s, OBJ_PEN) == U(GetStockObject)(BLACK_PEN));
    CHECK(U(GetCurrentObject)(s, OBJ_BRUSH) == U(GetStockObject)(WHITE_BRUSH));
    dca_unscreen(s);
}
TEST(dca_defaults_selecting_stock_returns_previous_stock) {
    REQUIRE(U(SelectObject)); REQUIRE(U(GetStockObject));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    HGDIOBJ p = U(SelectObject)(m.dc, U(GetStockObject)(WHITE_PEN));
    CHECK(p == U(GetStockObject)(BLACK_PEN));
    HGDIOBJ b = U(SelectObject)(m.dc, U(GetStockObject)(BLACK_BRUSH));
    CHECK(b == U(GetStockObject)(WHITE_BRUSH));
    U(SelectObject)(m.dc, p); U(SelectObject)(m.dc, b);
    mdc_free(&m);
}

/* ---------- current position ---------- */
#define NEEDPOS() do { REQUIRE(U(MoveToEx)); REQUIRE(U(LineTo)); REQUIRE(U(GetCurrentPositionEx)); } while (0)
static POINT cur(HDC d) { POINT p = { -12345, -12345 }; U(GetCurrentPositionEx)(d, &p); return p; }

TEST(dca_pos_initial_origin) {
    NEEDPOS();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    POINT p = { 9, 9 };
    CHECK(U(GetCurrentPositionEx)(m.dc, &p));
    CHECKF(p.x == 0 && p.y == 0, "(%ld,%ld)", p.x, p.y);
    HDC s = dca_screen();
    if (s) { p = cur(s); CHECKF(p.x == 0 && p.y == 0, "screen (%ld,%ld)", p.x, p.y); dca_unscreen(s); }
    mdc_free(&m);
}
TEST(dca_pos_moveto_returns_previous) {
    NEEDPOS();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    POINT old = { -1, -1 };
    CHECK(U(MoveToEx)(m.dc, 10, 20, &old));
    CHECKF(old.x == 0 && old.y == 0, "old (%ld,%ld)", old.x, old.y);
    CHECK(U(MoveToEx)(m.dc, -5, 7, &old));
    CHECKF(old.x == 10 && old.y == 20, "old (%ld,%ld)", old.x, old.y);
    CHECK(U(MoveToEx)(m.dc, 1, 1, NULL));
    POINT p = cur(m.dc);
    CHECKF(p.x == 1 && p.y == 1, "(%ld,%ld)", p.x, p.y);
    mdc_free(&m);
}
TEST(dca_pos_roundtrip_table) {
    NEEDPOS();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    static const POINT v[] = { {0, 0}, {1, 1}, {-1, -1}, {100, 200}, {-100, -200}, {32767, -32768}, {32768, 32768}, {65536, -65536}, {1000000, -1000000}, {0, 5}, {5, 0} };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        CHECK(U(MoveToEx)(m.dc, v[i].x, v[i].y, NULL));
        POINT p = cur(m.dc);
        CHECKF(p.x == v[i].x && p.y == v[i].y, "i=%u (%ld,%ld) want (%ld,%ld)", i, p.x, p.y, v[i].x, v[i].y);
    }
    mdc_free(&m);
}
TEST(dca_pos_extremes_observed) {
    NEEDPOS();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    static const POINT v[] = { {INT_MAX, INT_MIN}, {INT_MIN, INT_MAX}, {INT_MAX, INT_MAX}, {INT_MIN, INT_MIN} };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        BOOL ok = U(MoveToEx)(m.dc, v[i].x, v[i].y, NULL);
        POINT p = cur(m.dc);
        t_note("MoveToEx(%d,%d): ok=%d -> (%ld,%ld)", v[i].x, v[i].y, ok, p.x, p.y);
    }
    mdc_free(&m);
}
TEST(dca_pos_lineto_moves_even_offscreen) {
    NEEDPOS();
    MDC m; if (!mdc_make(&m, 32, 32)) { t_skip("mdc"); return; }
    mdc_clear(&m, WH);
    U(MoveToEx)(m.dc, 5, 5, NULL);
    CHECK(U(LineTo)(m.dc, 500, 400));
    POINT p = cur(m.dc);
    CHECKF(p.x == 500 && p.y == 400, "(%ld,%ld)", p.x, p.y);
    CHECK(U(LineTo)(m.dc, -50, -60));
    p = cur(m.dc);
    CHECKF(p.x == -50 && p.y == -60, "(%ld,%ld)", p.x, p.y);
    mdc_free(&m);
}
TEST(dca_pos_lineto_chain) {
    NEEDPOS();
    MDC m; if (!mdc_make(&m, 64, 64)) { t_skip("mdc"); return; }
    U(MoveToEx)(m.dc, 0, 0, NULL);
    for (int i = 1; i <= 20; i++) {
        CHECK(U(LineTo)(m.dc, i * 2, i * 3));
        POINT p = cur(m.dc);
        if (p.x != i * 2 || p.y != i * 3) { CHECKF(0, "i=%d (%ld,%ld)", i, p.x, p.y); break; }
    }
    mdc_free(&m);
}
TEST(dca_pos_moveto_does_not_draw_and_lineto_excludes_end) {
    NEEDPOS();
    MDC m; if (!mdc_make(&m, 32, 16)) { t_skip("mdc"); return; }
    mdc_clear(&m, WH);
    HGDIOBJ op = U(SelectObject)(m.dc, U(GetStockObject)(BLACK_PEN));
    U(MoveToEx)(m.dc, 5, 5, NULL);
    CHECKF(mdc_count_ne(&m, WH) == 0, "MoveToEx drew pixels");
    U(LineTo)(m.dc, 20, 5);
    CHECK(mdc_get(&m, 5, 5) == 0);
    CHECK(mdc_get(&m, 12, 5) == 0);
    CHECK(mdc_get(&m, 19, 5) == 0);
    CHECK(mdc_get(&m, 20, 5) == dca_px(WH));
    CHECK(mdc_get(&m, 4, 5) == dca_px(WH));
    CHECKF(mdc_count_ne(&m, WH) == 15, "pixel count %d", mdc_count_ne(&m, WH));
    U(SelectObject)(m.dc, op);
    mdc_free(&m);
}
TEST(dca_pos_polyline_does_not_move) {
    NEEDPOS(); REQUIRE(U(Polyline));
    MDC m; if (!mdc_make(&m, 64, 64)) { t_skip("mdc"); return; }
    U(MoveToEx)(m.dc, 7, 9, NULL);
    POINT pts[3] = { {10, 10}, {30, 10}, {30, 30} };
    CHECK(U(Polyline)(m.dc, pts, 3));
    POINT p = cur(m.dc);
    CHECKF(p.x == 7 && p.y == 9, "(%ld,%ld)", p.x, p.y);
    mdc_free(&m);
}
TEST(dca_pos_polylineto_moves_to_last) {
    NEEDPOS(); REQUIRE(U(PolylineTo));
    MDC m; if (!mdc_make(&m, 64, 64)) { t_skip("mdc"); return; }
    U(MoveToEx)(m.dc, 7, 9, NULL);
    POINT pts[3] = { {10, 10}, {30, 10}, {30, 30} };
    CHECK(U(PolylineTo)(m.dc, pts, 3));
    POINT p = cur(m.dc);
    CHECKF(p.x == 30 && p.y == 30, "(%ld,%ld)", p.x, p.y);
    mdc_free(&m);
}
TEST(dca_pos_polybezierto_moves_to_last) {
    NEEDPOS(); REQUIRE(U(PolyBezierTo)); REQUIRE(U(PolyBezier));
    MDC m; if (!mdc_make(&m, 64, 64)) { t_skip("mdc"); return; }
    U(MoveToEx)(m.dc, 5, 5, NULL);
    POINT pts[3] = { {10, 10}, {30, 10}, {40, 30} };
    CHECK(U(PolyBezierTo)(m.dc, pts, 3));
    POINT p = cur(m.dc);
    CHECKF(p.x == 40 && p.y == 30, "(%ld,%ld)", p.x, p.y);
    U(MoveToEx)(m.dc, 5, 5, NULL);
    POINT pb[4] = { {5, 5}, {10, 10}, {30, 10}, {40, 30} };
    CHECK(U(PolyBezier)(m.dc, pb, 4));
    p = cur(m.dc);
    if (p.x != 5 || p.y != 5) t_note("PolyBezier moved the current position to (%ld,%ld)", p.x, p.y);
    mdc_free(&m);
}
TEST(dca_pos_shapes_do_not_move) {
    NEEDPOS(); REQUIRE(U(Rectangle)); REQUIRE(U(Ellipse)); REQUIRE(U(RoundRect)); REQUIRE(U(Polygon)); REQUIRE(U(Arc)); REQUIRE(U(PatBlt));
    MDC m; if (!mdc_make(&m, 64, 64)) { t_skip("mdc"); return; }
    U(MoveToEx)(m.dc, 7, 9, NULL);
    U(Rectangle)(m.dc, 10, 10, 30, 30);
    U(Ellipse)(m.dc, 10, 10, 30, 30);
    U(RoundRect)(m.dc, 10, 10, 30, 30, 4, 4);
    POINT pts[3] = { {10, 10}, {30, 10}, {30, 30} };
    U(Polygon)(m.dc, pts, 3);
    U(Arc)(m.dc, 10, 10, 50, 50, 50, 30, 30, 10);
    U(PatBlt)(m.dc, 0, 0, 5, 5, BLACKNESS);
    if (U(FillRect)) { RECT r = { 0, 0, 4, 4 }; U(FillRect)(m.dc, &r, (HBRUSH)U(GetStockObject)(BLACK_BRUSH)); }
    POINT p = cur(m.dc);
    CHECKF(p.x == 7 && p.y == 9, "(%ld,%ld)", p.x, p.y);
    mdc_free(&m);
}
TEST(dca_pos_angle_arc_moves_to_end) {
    NEEDPOS(); REQUIRE(U(AngleArc));
    MDC m; if (!mdc_make(&m, 100, 100)) { t_skip("mdc"); return; }
    U(MoveToEx)(m.dc, 0, 0, NULL);
    CHECK(U(AngleArc)(m.dc, 50, 50, 20, 0.0f, 90.0f));
    POINT p = cur(m.dc);
    CHECKF(p.x >= 49 && p.x <= 51 && p.y >= 29 && p.y <= 31, "(%ld,%ld) want about (50,30)", p.x, p.y);
    mdc_free(&m);
}
TEST(dca_pos_textout_does_not_move_without_updatecp) {
    NEEDPOS(); REQUIRE(U(TextOutA));
    MDC m; if (!mdc_make(&m, 64, 32)) { t_skip("mdc"); return; }
    U(MoveToEx)(m.dc, 7, 9, NULL);
    U(TextOutA)(m.dc, 1, 1, "Hello", 5);
    if (U(ExtTextOutA)) U(ExtTextOutA)(m.dc, 1, 1, 0, NULL, "Hi", 2, NULL);
    POINT p = cur(m.dc);
    CHECKF(p.x == 7 && p.y == 9, "(%ld,%ld)", p.x, p.y);
    mdc_free(&m);
}
TEST(dca_pos_isolation) {
    NEEDPOS();
    MDC a, b; if (!mdc_make(&a, 8, 8)) { t_skip("mdc"); return; }
    if (!mdc_make(&b, 8, 8)) { mdc_free(&a); t_skip("mdc"); return; }
    U(MoveToEx)(a.dc, 3, 4, NULL);
    POINT p = cur(b.dc);
    CHECKF(p.x == 0 && p.y == 0, "b (%ld,%ld)", p.x, p.y);
    U(MoveToEx)(b.dc, 9, 9, NULL);
    p = cur(a.dc);
    CHECKF(p.x == 3 && p.y == 4, "a (%ld,%ld)", p.x, p.y);
    mdc_free(&a); mdc_free(&b);
}
TEST(dca_pos_survives_select_bitmap_observed) {
    NEEDPOS();
    MDC m; if (!mdc_make(&m, 16, 16)) { t_skip("mdc"); return; }
    U(MoveToEx)(m.dc, 6, 7, NULL);
    HBITMAP bm = U(CreateBitmap)(4, 4, 1, 1, NULL);
    HGDIOBJ old = U(SelectObject)(m.dc, bm);
    POINT p = cur(m.dc);
    t_note("position after selecting another bitmap: (%ld,%ld)", p.x, p.y);
    U(SelectObject)(m.dc, old); U(DeleteObject)(bm);
    p = cur(m.dc);
    CHECKF(p.x == 6 && p.y == 7, "(%ld,%ld)", p.x, p.y);
    mdc_free(&m);
}
TEST(dca_pos_savedc_restore_observed) {
    NEEDPOS();
    MDC m; if (!mdc_make(&m, 16, 16)) { t_skip("mdc"); return; }
    U(MoveToEx)(m.dc, 6, 7, NULL);
    int s = U(SaveDC)(m.dc);
    U(MoveToEx)(m.dc, 1, 2, NULL);
    CHECK(U(RestoreDC)(m.dc, s));
    POINT p = cur(m.dc);
    t_note("current position after RestoreDC: (%ld,%ld) (6,7 = restored, 1,2 = kept)", p.x, p.y);
    CHECK((p.x == 6 && p.y == 7) || (p.x == 1 && p.y == 2));
    mdc_free(&m);
}
TEST(dca_pos_mapmode_roundtrip) {
    NEEDPOS(); REQUIRE(U(SetMapMode));
    static const int modes[] = { MM_TEXT, MM_LOMETRIC, MM_HIMETRIC, MM_LOENGLISH, MM_HIENGLISH, MM_TWIPS };
    for (unsigned i = 0; i < sizeof modes / sizeof modes[0]; i++) {
        MDC m; if (!mdc_make(&m, 16, 16)) { t_skip("mdc"); return; }
        U(SetMapMode)(m.dc, modes[i]);
        CHECK(U(MoveToEx)(m.dc, 1234, -567, NULL));
        POINT p = cur(m.dc);
        CHECKF(abs((int)p.x - 1234) <= 10 && abs((int)p.y + 567) <= 10, "mode %d (%ld,%ld)", modes[i], p.x, p.y);
        POINT old;
        CHECK(U(MoveToEx)(m.dc, 0, 0, &old));
        CHECKF(abs((int)old.x - 1234) <= 10 && abs((int)old.y + 567) <= 10, "mode %d old (%ld,%ld)", modes[i], old.x, old.y);
        mdc_free(&m);
    }
}
TEST(dca_pos_after_viewport_change_observed) {
    NEEDPOS(); REQUIRE(U(SetViewportOrgEx)); REQUIRE(U(SetWindowOrgEx));
    MDC m; if (!mdc_make(&m, 16, 16)) { t_skip("mdc"); return; }
    U(MoveToEx)(m.dc, 20, 30, NULL);
    U(SetViewportOrgEx)(m.dc, 5, 7, NULL);
    POINT p = cur(m.dc);
    t_note("position after SetViewportOrgEx(5,7): (%ld,%ld) (20,30 = logical kept, 15,23 = device kept)", p.x, p.y);
    U(SetViewportOrgEx)(m.dc, 0, 0, NULL);
    U(SetWindowOrgEx)(m.dc, 4, 6, NULL);
    p = cur(m.dc);
    t_note("position after SetWindowOrgEx(4,6): (%ld,%ld)", p.x, p.y);
    mdc_free(&m);
}
TEST(dca_pos_world_transform_observed) {
    NEEDPOS(); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform));
    MDC m; if (!mdc_make(&m, 64, 64)) { t_skip("mdc"); return; }
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM x = { 1, 0, 0, 1, 10, 10 };
    U(SetWorldTransform)(m.dc, &x);
    U(MoveToEx)(m.dc, 5, 5, NULL);
    POINT p = cur(m.dc);
    CHECKF(p.x == 5 && p.y == 5, "logical position (%ld,%ld)", p.x, p.y);
    U(LineTo)(m.dc, 20, 5);
    p = cur(m.dc);
    CHECKF(p.x == 20 && p.y == 5, "after LineTo (%ld,%ld)", p.x, p.y);
    mdc_free(&m);
}
TEST(dca_pos_bad_dc) {
    NEEDPOS();
    POINT p = { 5, 5 };
    SetLastError(0);
    CHECK(!U(GetCurrentPositionEx)(NULL, &p));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    CHECKF(p.x == 5 && p.y == 5, "written (%ld,%ld)", p.x, p.y);
    for (int i = 0; i < DCA_NGARBAGE; i++) {
        SetLastError(0);
        CHECKF(!U(GetCurrentPositionEx)((HDC)dca_garbage[i], &p), "get garbage %d", i);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "get garbage %d err %lu", i, GetLastError());
        CHECKF(!U(MoveToEx)((HDC)dca_garbage[i], 1, 1, &p), "move garbage %d", i);
        CHECKF(!U(LineTo)((HDC)dca_garbage[i], 1, 1), "line garbage %d", i);
    }
    SetLastError(0);
    CHECK(!U(MoveToEx)(NULL, 1, 1, &p));
    t_note("MoveToEx(NULL) err=%lu", GetLastError());
    SetLastError(0);
    CHECK(!U(LineTo)(NULL, 1, 1));
    t_note("LineTo(NULL) err=%lu", GetLastError());
    WrongSet w; dca_wrong_make(&w);
    for (int i = 0; i < w.n; i++) if (w.h[i]) {
        CHECKF(!U(GetCurrentPositionEx)((HDC)w.h[i], &p), "get %s", w.nm[i]);
        CHECKF(!U(MoveToEx)((HDC)w.h[i], 1, 1, &p), "move %s", w.nm[i]);
        CHECKF(!U(LineTo)((HDC)w.h[i], 1, 1), "line %s", w.nm[i]);
    }
    dca_wrong_free(&w);
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    HDC d = m.dc; mdc_free(&m);
    SetLastError(0);
    CHECK(!U(GetCurrentPositionEx)(d, &p));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    CHECK(!U(MoveToEx)(d, 1, 1, &p));
    CHECK(!U(LineTo)(d, 1, 1));
}
TEST(dca_pos_get_null_point) {
    NEEDPOS();
    DCA_WINE_SKIP("NULL out pointer faults in the emulator");
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    SetLastError(0);
    CHECK(!U(GetCurrentPositionEx)(m.dc, NULL));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    mdc_free(&m);
}
TEST(dca_pos_moveto_old_point_on_failure_untouched) {
    NEEDPOS();
    POINT old = { 77, 88 };
    SetLastError(0);
    CHECK(!U(MoveToEx)(NULL, 1, 1, &old));
    t_note("old point after failed MoveToEx: (%ld,%ld)", old.x, old.y);
}
TEST(dca_pos_screen_dc_roundtrip_and_release) {
    NEEDPOS();
    HDC s = dca_screen(); if (!s) { t_skip("no screen"); return; }
    CHECK(U(MoveToEx)(s, 11, 22, NULL));
    POINT p = cur(s);
    CHECKF(p.x == 11 && p.y == 22, "(%ld,%ld)", p.x, p.y);
    dca_unscreen(s);
    s = dca_screen();
    p = cur(s);
    t_note("screen DC position after Release/Get: (%ld,%ld)", p.x, p.y);
    dca_unscreen(s);
}
