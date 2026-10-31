/* AbortPath, CloseFigure, StrokePath, FillPath, StrokeAndFillPath, FloodFill, LineDDA. */
#include "gap10.h"

#define NEED_PATH() do { NEED_MD(); REQUIRE(U(BeginPath)); REQUIRE(U(EndPath)); REQUIRE(U(AbortPath)); REQUIRE(U(MoveToEx)); \
    REQUIRE(U(LineTo)); REQUIRE(U(GetPath)); } while (0)

static HDC mk(void) { return U(CreateCompatibleDC)(NULL); }
static int npts(HDC dc) { return U(GetPath)(dc, NULL, NULL, 0); }
static void tri(HDC dc, int x0, int y0, int x1, int y1, int x2, int y2) {
    U(MoveToEx)(dc, x0, y0, NULL); U(LineTo)(dc, x1, y1); U(LineTo)(dc, x2, y2);
}
static void square_path(HDC dc, int l, int t, int r, int b) {
    U(BeginPath)(dc);
    U(MoveToEx)(dc, l, t, NULL); U(LineTo)(dc, r, t); U(LineTo)(dc, r, b); U(LineTo)(dc, l, b);
    U(CloseFigure)(dc);
    U(EndPath)(dc);
}

/* ---------------- AbortPath ---------------- */
TEST(gap10_abortpath_invalid_dc) {
    REQUIRE(U(AbortPath));
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef, (HDC)(ULONG_PTR)0xFFFFFFFF }; unsigned i;
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        CLR();
        CHECKF(!U(AbortPath)(bad[i]), "dc %p", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "dc %p err %lu", bad[i], GetLastError());
    }
}
TEST(gap10_abortpath_wrong_object_type) {
    REQUIRE(U(AbortPath)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    HBRUSH b = U(CreateSolidBrush)(0);
    CLR();
    CHECK(!U(AbortPath)((HDC)b));
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    U(DeleteObject)(b);
}
TEST(gap10_abortpath_deleted_dc) {
    REQUIRE(U(AbortPath)); NEED_MD();
    HDC dc = mk(); U(DeleteDC)(dc);
    CLR();
    CHECK(!U(AbortPath)(dc));
    t_note("deleted DC: err %lu", GetLastError());
}
TEST(gap10_abortpath_discards_open_bracket) {
    NEED_PATH();
    HDC dc = mk();
    CHECK(U(BeginPath)(dc));
    tri(dc, 1, 1, 10, 1, 10, 10);
    CHECK(U(AbortPath)(dc));
    CLR();
    CHECK(!U(EndPath)(dc));   /* no bracket any more */
    EXPECT_EQ(npts(dc), -1);
    U(DeleteDC)(dc);
}
TEST(gap10_abortpath_discards_closed_path) {
    NEED_PATH(); REQUIRE(U(StrokePath));
    HDC dc = mk();
    square_path(dc, 1, 1, 10, 10);
    EXPECT_EQ(npts(dc), 4);
    CHECK(U(AbortPath)(dc));
    EXPECT_EQ(npts(dc), -1);
    CLR();
    CHECK(!U(StrokePath)(dc));
    U(DeleteDC)(dc);
}
TEST(gap10_abortpath_without_path_observed) {
    NEED_PATH();
    HDC dc = mk(); BOOL r;
    CLR();
    r = U(AbortPath)(dc);
    t_note("AbortPath with no path -> %d err %lu", r, GetLastError());
    r = U(AbortPath)(dc);
    t_note("second AbortPath -> %d", r);
    U(DeleteDC)(dc);
}
TEST(gap10_abortpath_then_new_path_works) {
    NEED_PATH();
    HDC dc = mk();
    U(BeginPath)(dc); tri(dc, 0, 0, 5, 0, 5, 5); U(AbortPath)(dc);
    CHECK(U(BeginPath)(dc));
    U(MoveToEx)(dc, 2, 2, NULL); U(LineTo)(dc, 9, 2);
    CHECK(U(EndPath)(dc));
    EXPECT_EQ(npts(dc), 2);
    U(DeleteDC)(dc);
}
TEST(gap10_abortpath_in_bracket_allows_new_begin) {
    NEED_PATH();
    HDC dc = mk();
    U(BeginPath)(dc);
    U(AbortPath)(dc);
    CHECK(U(BeginPath)(dc));
    U(AbortPath)(dc);
    U(DeleteDC)(dc);
}
TEST(gap10_abortpath_prevents_fill_pixels) {
    NEED_PATH(); REQUIRE(U(FillPath));
    MD m;
    if (!md_create(&m, 20, 20, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    HBRUSH b = U(CreateSolidBrush)(RED_); HGDIOBJ ob = U(SelectObject)(m.dc, b);
    square_path(m.dc, 2, 2, 15, 15);
    U(AbortPath)(m.dc);
    CLR();
    CHECK(!U(FillPath)(m.dc));
    EXPECT_EQ(md_diff(&m, WHITE_), 0);
    U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(gap10_abortpath_keeps_dc_attributes) {
    NEED_PATH(); REQUIRE(U(SetBkColor)); REQUIRE(U(GetBkColor)); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = mk(); int id;
    U(SetBkColor)(dc, 0x445566);
    id = U(SaveDC)(dc);
    U(BeginPath)(dc); U(AbortPath)(dc);
    EXPECT_EQ(U(GetBkColor)(dc), 0x445566);
    CHECK(U(RestoreDC)(dc, id));
    U(DeleteDC)(dc);
}
TEST(gap10_abortpath_repeated_cycles) {
    NEED_PATH();
    HDC dc = mk(); int i, ok = 1;
    for (i = 0; i < 500; i++) {
        if (!U(BeginPath)(dc)) ok = 0;
        tri(dc, i % 7, 0, 10, 10, 0, 10);
        if (!U(AbortPath)(dc)) ok = 0;
    }
    CHECK(ok);
    EXPECT_EQ(npts(dc), -1);
    U(DeleteDC)(dc);
}
TEST(gap10_abortpath_after_closed_path_selected_in_clip) {
    NEED_PATH(); REQUIRE(U(SelectClipPath));
    HDC dc = mk();
    square_path(dc, 0, 0, 8, 8);
    CHECK(U(SelectClipPath)(dc, RGN_COPY));
    EXPECT_EQ(npts(dc), -1);    /* path consumed */
    CLR();
    U(AbortPath)(dc);
    U(DeleteDC)(dc);
}

/* ---------------- CloseFigure ---------------- */
TEST(gap10_closefigure_invalid_dc) {
    REQUIRE(U(CloseFigure));
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef }; unsigned i;
    for (i = 0; i < 3; i++) { CLR(); CHECKF(!U(CloseFigure)(bad[i]), "dc %p", bad[i]); t_note("CloseFigure(%p) err %lu", bad[i], GetLastError()); }
}
TEST(gap10_closefigure_wrong_object_type) {
    REQUIRE(U(CloseFigure)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    HBRUSH b = U(CreateSolidBrush)(0);
    CHECK(!U(CloseFigure)((HDC)b));
    U(DeleteObject)(b);
}
TEST(gap10_closefigure_outside_bracket_fails) {
    NEED_PATH();
    HDC dc = mk();
    CLR();
    CHECK(!U(CloseFigure)(dc));
    t_note("CloseFigure outside bracket: err %lu", GetLastError());
    U(DeleteDC)(dc);
}
TEST(gap10_closefigure_after_endpath_fails) {
    NEED_PATH();
    HDC dc = mk();
    U(BeginPath)(dc); tri(dc, 0, 0, 5, 0, 5, 5); U(EndPath)(dc);
    CLR();
    CHECK(!U(CloseFigure)(dc));
    EXPECT_EQ(npts(dc), 3);
    U(AbortPath)(dc);
    U(DeleteDC)(dc);
}
TEST(gap10_closefigure_marks_last_point) {
    NEED_PATH();
    HDC dc = mk(); POINT pt[8]; BYTE ty[8];
    U(BeginPath)(dc);
    tri(dc, 0, 0, 10, 0, 10, 10);
    CHECK(U(CloseFigure)(dc));
    CHECK(U(EndPath)(dc));
    EXPECT_EQ(U(GetPath)(dc, pt, ty, 8), 3);
    EXPECT_EQ(ty[0], PT_MOVETO);
    EXPECT_EQ(ty[1], PT_LINETO);
    EXPECT_EQ(ty[2], PT_LINETO | PT_CLOSEFIGURE);
    EXPECT_EQ(pt[2].x, 10); EXPECT_EQ(pt[2].y, 10);
    U(AbortPath)(dc);
    U(DeleteDC)(dc);
}
TEST(gap10_closefigure_two_figures) {
    NEED_PATH();
    HDC dc = mk(); POINT pt[16]; BYTE ty[16]; int n;
    U(BeginPath)(dc);
    tri(dc, 0, 0, 10, 0, 10, 10); U(CloseFigure)(dc);
    tri(dc, 20, 20, 30, 20, 30, 30); U(CloseFigure)(dc);
    U(EndPath)(dc);
    n = U(GetPath)(dc, pt, ty, 16);
    EXPECT_EQ(n, 6);
    if (n == 6) {
        EXPECT_EQ(ty[0], PT_MOVETO); EXPECT_EQ(ty[2], PT_LINETO | PT_CLOSEFIGURE);
        EXPECT_EQ(ty[3], PT_MOVETO); EXPECT_EQ(ty[5], PT_LINETO | PT_CLOSEFIGURE);
        EXPECT_EQ(pt[3].x, 20);
    }
    U(DeleteDC)(dc);
}
TEST(gap10_closefigure_not_called_leaves_open) {
    NEED_PATH();
    HDC dc = mk(); POINT pt[4]; BYTE ty[4];
    U(BeginPath)(dc); tri(dc, 0, 0, 10, 0, 10, 10); U(EndPath)(dc);
    EXPECT_EQ(U(GetPath)(dc, pt, ty, 4), 3);
    EXPECT_EQ(ty[2], PT_LINETO);
    U(DeleteDC)(dc);
}
TEST(gap10_closefigure_twice_observed) {
    NEED_PATH();
    HDC dc = mk(); POINT pt[8]; BYTE ty[8]; int n;
    U(BeginPath)(dc); tri(dc, 0, 0, 10, 0, 10, 10);
    CHECK(U(CloseFigure)(dc));
    BOOL r2 = U(CloseFigure)(dc);
    t_note("second CloseFigure with no new figure -> %d", r2);
    U(EndPath)(dc);
    n = U(GetPath)(dc, pt, ty, 8);
    t_note("points after double close: %d", n);
    CHECK(n >= 3);
    U(DeleteDC)(dc);
}
TEST(gap10_closefigure_empty_bracket_observed) {
    NEED_PATH();
    HDC dc = mk(); BOOL r;
    U(BeginPath)(dc);
    CLR();
    r = U(CloseFigure)(dc);
    t_note("CloseFigure on empty bracket -> %d err %lu", r, GetLastError());
    U(EndPath)(dc);
    t_note("points: %d", npts(dc));
    U(DeleteDC)(dc);
}
TEST(gap10_closefigure_after_moveto_only_observed) {
    NEED_PATH();
    HDC dc = mk(); BOOL r;
    U(BeginPath)(dc); U(MoveToEx)(dc, 5, 5, NULL);
    r = U(CloseFigure)(dc);
    U(EndPath)(dc);
    t_note("MoveTo+CloseFigure -> %d, points %d", r, npts(dc));
    U(DeleteDC)(dc);
}
TEST(gap10_closefigure_single_line_closes_polygon_fill) {
    NEED_PATH(); REQUIRE(U(FillPath));
    MD m; HBRUSH b; HGDIOBJ ob;
    if (!md_create(&m, 24, 24, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    b = U(CreateSolidBrush)(BLUE_); ob = U(SelectObject)(m.dc, b);
    U(BeginPath)(m.dc); tri(m.dc, 2, 2, 20, 2, 20, 20); U(CloseFigure)(m.dc); U(EndPath)(m.dc);
    CHECK(U(FillPath)(m.dc));
    EXPECT_EQ(md_px(&m, 17, 6), BLUE_);
    EXPECT_EQ(md_px(&m, 4, 18), WHITE_);
    U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(gap10_closefigure_stroke_draws_closing_edge) {
    NEED_PATH(); REQUIRE(U(StrokePath)); REQUIRE(U(CreatePen)); REQUIRE(U(CreateRectRgn));
    MD m; HPEN p, op; int closed_edge, open_edge;
    if (!md_create(&m, 32, 32, 32)) { t_skip("no DIB"); return; }
    p = U(CreatePen)(PS_SOLID, 3, RED_); op = U(SelectObject)(m.dc, p);
    /* open: U shape; no closing edge on x=4 */
    md_clear(&m, WHITE_);
    U(BeginPath)(m.dc); U(MoveToEx)(m.dc, 4, 6, NULL); U(LineTo)(m.dc, 4, 26); U(LineTo)(m.dc, 26, 26); U(LineTo)(m.dc, 26, 6); U(EndPath)(m.dc);
    U(StrokePath)(m.dc);
    open_edge = md_count(&m, 8, 5, 23, 8, RED_);
    md_clear(&m, WHITE_);
    U(BeginPath)(m.dc); U(MoveToEx)(m.dc, 4, 6, NULL); U(LineTo)(m.dc, 4, 26); U(LineTo)(m.dc, 26, 26); U(LineTo)(m.dc, 26, 6);
    U(CloseFigure)(m.dc); U(EndPath)(m.dc);
    U(StrokePath)(m.dc);
    closed_edge = md_count(&m, 8, 5, 23, 8, RED_);
    EXPECT_EQ(open_edge, 0);
    CHECKF(closed_edge > 30, "closing edge pixels %d", closed_edge);
    EXPECT_EQ(md_px(&m, 15, 6), RED_);
    U(SelectObject)(m.dc, op); U(DeleteObject)(p);
    md_destroy(&m);
}

/* ---------------- StrokePath ---------------- */
TEST(gap10_strokepath_invalid_dc) {
    REQUIRE(U(StrokePath));
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef }; unsigned i;
    for (i = 0; i < 3; i++) { CLR(); CHECKF(!U(StrokePath)(bad[i]), "dc %p", bad[i]); t_note("StrokePath(%p) err %lu", bad[i], GetLastError()); }
}
TEST(gap10_strokepath_no_path_fails) {
    NEED_PATH(); REQUIRE(U(StrokePath));
    HDC dc = mk();
    CLR();
    CHECK(!U(StrokePath)(dc));
    t_note("no path: err %lu", GetLastError());
    U(DeleteDC)(dc);
}
TEST(gap10_strokepath_open_bracket_fails) {
    NEED_PATH(); REQUIRE(U(StrokePath));
    HDC dc = mk();
    U(BeginPath)(dc); tri(dc, 0, 0, 5, 0, 5, 5);
    CLR();
    CHECK(!U(StrokePath)(dc));
    t_note("open bracket: err %lu", GetLastError());
    U(AbortPath)(dc);
    U(DeleteDC)(dc);
}
TEST(gap10_strokepath_consumes_path) {
    NEED_PATH(); REQUIRE(U(StrokePath));
    HDC dc = mk();
    square_path(dc, 1, 1, 9, 9);
    CHECK(U(StrokePath)(dc));
    EXPECT_EQ(npts(dc), -1);
    CLR();
    CHECK(!U(StrokePath)(dc));
    U(DeleteDC)(dc);
}
TEST(gap10_strokepath_horizontal_line_pixels) {
    NEED_PATH(); REQUIRE(U(StrokePath)); REQUIRE(U(CreatePen));
    int bpps[] = { 32, 24, 16 }; unsigned k;
    for (k = 0; k < 3; k++) {
        MD m; HPEN p, op; int x;
        if (!md_create(&m, 40, 24, bpps[k])) continue;
        md_clear(&m, WHITE_);
        p = U(CreatePen)(PS_SOLID, 5, BLUE_); op = U(SelectObject)(m.dc, p);
        U(BeginPath)(m.dc); U(MoveToEx)(m.dc, 6, 12, NULL); U(LineTo)(m.dc, 34, 12); U(EndPath)(m.dc);
        CHECK(U(StrokePath)(m.dc));
        for (x = 10; x < 30; x++) {
            CHECKF(md_px(&m, x, 12) == BLUE_, "bpp %d x=%d centre", bpps[k], x);
            CHECKF(md_px(&m, x, 11) == BLUE_ && md_px(&m, x, 13) == BLUE_, "bpp %d x=%d near", bpps[k], x);
            CHECKF(md_px(&m, x, 8) == WHITE_ && md_px(&m, x, 16) == WHITE_, "bpp %d x=%d far", bpps[k], x);
        }
        EXPECT_EQ(md_px(&m, 2, 12), WHITE_); EXPECT_EQ(md_px(&m, 38, 12), WHITE_);
        U(SelectObject)(m.dc, op); U(DeleteObject)(p);
        md_destroy(&m);
    }
}
TEST(gap10_strokepath_does_not_fill_interior) {
    NEED_PATH(); REQUIRE(U(StrokePath)); REQUIRE(U(CreatePen));
    MD m; HPEN p, op; HBRUSH b; HGDIOBJ ob;
    if (!md_create(&m, 40, 40, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    p = U(CreatePen)(PS_SOLID, 3, RED_); op = U(SelectObject)(m.dc, p);
    b = U(CreateSolidBrush)(GREEN_); ob = U(SelectObject)(m.dc, b);
    square_path(m.dc, 5, 5, 34, 34);
    CHECK(U(StrokePath)(m.dc));
    EXPECT_EQ(md_px(&m, 20, 20), WHITE_);
    EXPECT_EQ(md_count(&m, 0, 0, 40, 40, GREEN_), 0);
    EXPECT_EQ(md_px(&m, 20, 5), RED_); EXPECT_EQ(md_px(&m, 5, 20), RED_);
    EXPECT_EQ(md_px(&m, 34, 20), RED_); EXPECT_EQ(md_px(&m, 20, 34), RED_);
    U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, op); U(DeleteObject)(b); U(DeleteObject)(p);
    md_destroy(&m);
}
TEST(gap10_strokepath_null_pen_draws_nothing) {
    NEED_PATH(); REQUIRE(U(StrokePath)); REQUIRE(U(GetStockObject));
    MD m; HGDIOBJ op;
    if (!md_create(&m, 20, 20, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    op = U(SelectObject)(m.dc, U(GetStockObject)(NULL_PEN));
    square_path(m.dc, 2, 2, 17, 17);
    BOOL r = U(StrokePath)(m.dc);
    t_note("StrokePath with NULL_PEN -> %d", r);
    EXPECT_EQ(md_diff(&m, WHITE_), 0);
    U(SelectObject)(m.dc, op);
    md_destroy(&m);
}
TEST(gap10_strokepath_geometric_pen_wide) {
    NEED_PATH(); REQUIRE(U(StrokePath)); REQUIRE(U(ExtCreatePen));
    MD m; HPEN p, op; LOGBRUSH lb; int y;
    if (!md_create(&m, 30, 40, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    lb.lbStyle = BS_SOLID; lb.lbColor = RED_; lb.lbHatch = 0;
    p = U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_FLAT, 9, &lb, 0, NULL);
    CHECK(p != NULL);
    if (p) {
        op = U(SelectObject)(m.dc, p);
        U(BeginPath)(m.dc); U(MoveToEx)(m.dc, 15, 5, NULL); U(LineTo)(m.dc, 15, 35); U(EndPath)(m.dc);
        CHECK(U(StrokePath)(m.dc));
        for (y = 10; y < 30; y++) {
            CHECKF(md_px(&m, 15, y) == RED_ && md_px(&m, 12, y) == RED_ && md_px(&m, 18, y) == RED_, "y=%d", y);
            CHECKF(md_px(&m, 8, y) == WHITE_ && md_px(&m, 22, y) == WHITE_, "y=%d outside", y);
        }
        U(SelectObject)(m.dc, op); U(DeleteObject)(p);
    }
    md_destroy(&m);
}
TEST(gap10_strokepath_respects_clip) {
    NEED_PATH(); REQUIRE(U(StrokePath)); REQUIRE(U(CreatePen)); REQUIRE(U(IntersectClipRect));
    MD m; HPEN p, op; int x;
    if (!md_create(&m, 40, 20, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    U(IntersectClipRect)(m.dc, 10, 0, 20, 20);
    p = U(CreatePen)(PS_SOLID, 3, RED_); op = U(SelectObject)(m.dc, p);
    U(BeginPath)(m.dc); U(MoveToEx)(m.dc, 0, 10, NULL); U(LineTo)(m.dc, 39, 10); U(EndPath)(m.dc);
    CHECK(U(StrokePath)(m.dc));
    for (x = 0; x < 40; x++) {
        COLORREF c = md_px(&m, x, 10);
        CHECKF(c == ((x >= 10 && x < 20) ? RED_ : WHITE_), "x=%d %06lx", x, c);
    }
    U(SelectObject)(m.dc, op); U(DeleteObject)(p);
    md_destroy(&m);
}
TEST(gap10_strokepath_curve_path_marks_pixels) {
    NEED_PATH(); REQUIRE(U(StrokePath)); REQUIRE(U(CreatePen)); REQUIRE(U(Ellipse));
    MD m; HPEN p, op;
    if (!md_create(&m, 40, 40, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    p = U(CreatePen)(PS_SOLID, 1, RED_); op = U(SelectObject)(m.dc, p);
    U(BeginPath)(m.dc); U(Ellipse)(m.dc, 4, 4, 36, 36); U(EndPath)(m.dc);
    CHECK(U(StrokePath)(m.dc));
    EXPECT_EQ(md_px(&m, 20, 20), WHITE_);
    int n = md_count(&m, 0, 0, 40, 40, RED_);
    CHECKF(n > 80 && n < 200, "outline pixels %d", n);
    EXPECT_EQ(md_px(&m, 0, 0), WHITE_);
    U(SelectObject)(m.dc, op); U(DeleteObject)(p);
    md_destroy(&m);
}
TEST(gap10_strokepath_gdi_object_count_stable) {
    NEED_PATH(); REQUIRE(U(StrokePath));
    if (!g10_gdicount()) { t_skip("no GetGuiResources"); return; }
    HDC dc = mk(); DWORD before = g10_gdicount(); int i;
    for (i = 0; i < 200; i++) { square_path(dc, 0, 0, 5, 5); U(StrokePath)(dc); }
    EXPECT_EQ(g10_gdicount(), before);
    U(DeleteDC)(dc);
}

/* ---------------- FillPath ---------------- */
TEST(gap10_fillpath_invalid_dc) {
    REQUIRE(U(FillPath));
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef }; unsigned i;
    for (i = 0; i < 3; i++) { CLR(); CHECKF(!U(FillPath)(bad[i]), "dc %p", bad[i]); t_note("FillPath(%p) err %lu", bad[i], GetLastError()); }
}
TEST(gap10_fillpath_no_path_fails) {
    NEED_PATH(); REQUIRE(U(FillPath));
    HDC dc = mk();
    CLR();
    CHECK(!U(FillPath)(dc));
    t_note("no path: err %lu", GetLastError());
    U(DeleteDC)(dc);
}
TEST(gap10_fillpath_open_bracket_fails) {
    NEED_PATH(); REQUIRE(U(FillPath));
    HDC dc = mk();
    U(BeginPath)(dc); tri(dc, 0, 0, 5, 0, 5, 5);
    CHECK(!U(FillPath)(dc));
    U(AbortPath)(dc);
    U(DeleteDC)(dc);
}
TEST(gap10_fillpath_consumes_path) {
    NEED_PATH(); REQUIRE(U(FillPath));
    HDC dc = mk();
    square_path(dc, 1, 1, 9, 9);
    CHECK(U(FillPath)(dc));
    EXPECT_EQ(npts(dc), -1);
    CHECK(!U(FillPath)(dc));
    U(DeleteDC)(dc);
}
TEST(gap10_fillpath_rect_pixels) {
    NEED_PATH(); REQUIRE(U(FillPath));
    int bpps[] = { 32, 24, 16 }; unsigned k;
    for (k = 0; k < 3; k++) {
        MD m; HBRUSH b; HGDIOBJ ob;
        if (!md_create(&m, 32, 32, bpps[k])) continue;
        md_clear(&m, WHITE_);
        b = U(CreateSolidBrush)(RED_); ob = U(SelectObject)(m.dc, b);
        square_path(m.dc, 10, 10, 20, 20);
        CHECK(U(FillPath)(m.dc));
        CHECKF(md_count(&m, 11, 11, 19, 19, RED_) == 64, "bpp %d interior", bpps[k]);
        CHECKF(md_count(&m, 0, 0, 32, 8, RED_) == 0 && md_count(&m, 0, 22, 32, 32, RED_) == 0, "bpp %d outside rows", bpps[k]);
        CHECKF(md_count(&m, 0, 0, 8, 32, RED_) == 0 && md_count(&m, 22, 0, 32, 32, RED_) == 0, "bpp %d outside cols", bpps[k]);
        int n = md_count(&m, 0, 0, 32, 32, RED_);
        if (n != 100) t_note("bpp %d: filled %d pixels for 10x10 path", bpps[k], n);
        CHECKF(n >= 64 && n <= 144, "bpp %d filled %d", bpps[k], n);
        U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
        md_destroy(&m);
    }
}
TEST(gap10_fillpath_rect_exact_edges_observed) {
    NEED_PATH(); REQUIRE(U(FillPath));
    MD m; HBRUSH b; HGDIOBJ ob;
    if (!md_create(&m, 32, 32, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    b = U(CreateSolidBrush)(RED_); ob = U(SelectObject)(m.dc, b);
    square_path(m.dc, 10, 10, 20, 20);
    U(FillPath)(m.dc);
    t_note("edges: (10,10)=%06lx (19,19)=%06lx (20,20)=%06lx (20,10)=%06lx (10,20)=%06lx",
           md_px(&m, 10, 10), md_px(&m, 19, 19), md_px(&m, 20, 20), md_px(&m, 20, 10), md_px(&m, 10, 20));
    U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(gap10_fillpath_open_figure_auto_closed) {
    NEED_PATH(); REQUIRE(U(FillPath));
    MD m; HBRUSH b; HGDIOBJ ob;
    if (!md_create(&m, 32, 32, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    b = U(CreateSolidBrush)(GREEN_); ob = U(SelectObject)(m.dc, b);
    U(BeginPath)(m.dc); tri(m.dc, 4, 4, 28, 4, 28, 28); U(EndPath)(m.dc);   /* no CloseFigure */
    CHECK(U(FillPath)(m.dc));
    EXPECT_EQ(md_px(&m, 22, 10), GREEN_);
    EXPECT_EQ(md_px(&m, 8, 24), WHITE_);
    U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    md_destroy(&m);
}
static void star(HDC dc) {
    U(BeginPath)(dc);
    U(MoveToEx)(dc, 30, 4, NULL); U(LineTo)(dc, 46, 52); U(LineTo)(dc, 6, 22); U(LineTo)(dc, 54, 22); U(LineTo)(dc, 14, 52);
    U(CloseFigure)(dc);
    U(EndPath)(dc);
}
TEST(gap10_fillpath_polyfillmode_star) {
    NEED_PATH(); REQUIRE(U(FillPath)); REQUIRE(U(SetPolyFillMode));
    int modes[2] = { ALTERNATE, WINDING }; unsigned k;
    for (k = 0; k < 2; k++) {
        MD m; HBRUSH b; HGDIOBJ ob; COLORREF c;
        if (!md_create(&m, 60, 60, 32)) continue;
        md_clear(&m, WHITE_);
        b = U(CreateSolidBrush)(RED_); ob = U(SelectObject)(m.dc, b);
        U(SetPolyFillMode)(m.dc, modes[k]);
        star(m.dc);
        CHECK(U(FillPath)(m.dc));
        c = md_px(&m, 30, 33);   /* pentagon in the centre */
        CHECKF(c == (modes[k] == WINDING ? RED_ : WHITE_), "mode %d centre %06lx", modes[k], c);
        CHECKF(md_px(&m, 30, 12) == RED_, "mode %d tip", modes[k]);
        EXPECT_EQ(md_px(&m, 2, 2), WHITE_);
        U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
        md_destroy(&m);
    }
}
TEST(gap10_fillpath_pattern_brush_hatch) {
    NEED_PATH(); REQUIRE(U(FillPath)); REQUIRE(U(CreateHatchBrush)); REQUIRE(U(SetBkMode)); REQUIRE(U(SetBkColor));
    MD m; HBRUSH b; HGDIOBJ ob; int red, white, other;
    if (!md_create(&m, 32, 32, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    b = U(CreateHatchBrush)(HS_DIAGCROSS, RED_); ob = U(SelectObject)(m.dc, b);
    U(SetBkColor)(m.dc, BLUE_); U(SetBkMode)(m.dc, OPAQUE);
    square_path(m.dc, 0, 0, 32, 32);
    CHECK(U(FillPath)(m.dc));
    red = md_count(&m, 0, 0, 32, 32, RED_); white = md_count(&m, 0, 0, 32, 32, WHITE_);
    other = md_count(&m, 0, 0, 32, 32, BLUE_);
    CHECKF(red > 0 && other > 0 && white == 0, "red %d blue %d white %d", red, other, white);
    CHECKF(red + other == 1024, "red+blue %d", red + other);
    U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(gap10_fillpath_transparent_bk_hatch) {
    NEED_PATH(); REQUIRE(U(FillPath)); REQUIRE(U(CreateHatchBrush)); REQUIRE(U(SetBkMode));
    MD m; HBRUSH b; HGDIOBJ ob;
    if (!md_create(&m, 32, 32, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    b = U(CreateHatchBrush)(HS_HORIZONTAL, RED_); ob = U(SelectObject)(m.dc, b);
    U(SetBkMode)(m.dc, TRANSPARENT);
    square_path(m.dc, 0, 0, 32, 32);
    CHECK(U(FillPath)(m.dc));
    int red = md_count(&m, 0, 0, 32, 32, RED_), white = md_count(&m, 0, 0, 32, 32, WHITE_);
    CHECKF(red > 0 && white > 0 && red + white == 1024, "red %d white %d", red, white);
    U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(gap10_fillpath_null_brush_observed) {
    NEED_PATH(); REQUIRE(U(FillPath)); REQUIRE(U(GetStockObject));
    MD m; HGDIOBJ ob;
    if (!md_create(&m, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    ob = U(SelectObject)(m.dc, U(GetStockObject)(NULL_BRUSH));
    square_path(m.dc, 2, 2, 12, 12);
    BOOL r = U(FillPath)(m.dc);
    t_note("FillPath with NULL_BRUSH -> %d", r);
    EXPECT_EQ(md_diff(&m, WHITE_), 0);
    U(SelectObject)(m.dc, ob);
    md_destroy(&m);
}
TEST(gap10_fillpath_rop2_xor) {
    NEED_PATH(); REQUIRE(U(FillPath)); REQUIRE(U(SetROP2));
    MD m; HBRUSH b; HGDIOBJ ob;
    if (!md_create(&m, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    b = U(CreateSolidBrush)(0x00FF00FF); ob = U(SelectObject)(m.dc, b);
    U(SetROP2)(m.dc, R2_XORPEN);
    square_path(m.dc, 2, 2, 12, 12);
    CHECK(U(FillPath)(m.dc));
    EXPECT_EQ(md_px(&m, 6, 6), 0x0000FF00);   /* white xor magenta = green */
    EXPECT_EQ(md_px(&m, 14, 14), WHITE_);
    U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(gap10_fillpath_respects_clip_rgn) {
    NEED_PATH(); REQUIRE(U(FillPath)); REQUIRE(U(IntersectClipRect));
    MD m; HBRUSH b; HGDIOBJ ob;
    if (!md_create(&m, 32, 32, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    U(IntersectClipRect)(m.dc, 8, 8, 16, 16);
    b = U(CreateSolidBrush)(RED_); ob = U(SelectObject)(m.dc, b);
    square_path(m.dc, 0, 0, 32, 32);
    CHECK(U(FillPath)(m.dc));
    EXPECT_EQ(md_count(&m, 0, 0, 32, 32, RED_), 64);
    EXPECT_EQ(md_px(&m, 8, 8), RED_); EXPECT_EQ(md_px(&m, 15, 15), RED_); EXPECT_EQ(md_px(&m, 16, 16), WHITE_);
    U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(gap10_fillpath_with_viewport_transform) {
    NEED_PATH(); REQUIRE(U(FillPath)); REQUIRE(U(SetViewportOrgEx)); REQUIRE(U(SetMapMode));
    MD m; HBRUSH b; HGDIOBJ ob;
    if (!md_create(&m, 32, 32, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    b = U(CreateSolidBrush)(RED_); ob = U(SelectObject)(m.dc, b);
    U(SetViewportOrgEx)(m.dc, 10, 5, NULL);
    square_path(m.dc, 0, 0, 8, 8);
    CHECK(U(FillPath)(m.dc));
    EXPECT_EQ(md_px(&m, 13, 8), RED_); EXPECT_EQ(md_px(&m, 11, 6), RED_);
    EXPECT_EQ(md_px(&m, 3, 3), WHITE_); EXPECT_EQ(md_px(&m, 20, 20), WHITE_);
    U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(gap10_fillpath_two_disjoint_figures) {
    NEED_PATH(); REQUIRE(U(FillPath));
    MD m; HBRUSH b; HGDIOBJ ob;
    if (!md_create(&m, 40, 20, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    b = U(CreateSolidBrush)(RED_); ob = U(SelectObject)(m.dc, b);
    U(BeginPath)(m.dc);
    U(MoveToEx)(m.dc, 2, 2, NULL); U(LineTo)(m.dc, 12, 2); U(LineTo)(m.dc, 12, 12); U(LineTo)(m.dc, 2, 12); U(CloseFigure)(m.dc);
    U(MoveToEx)(m.dc, 24, 4, NULL); U(LineTo)(m.dc, 36, 4); U(LineTo)(m.dc, 36, 16); U(LineTo)(m.dc, 24, 16); U(CloseFigure)(m.dc);
    U(EndPath)(m.dc);
    CHECK(U(FillPath)(m.dc));
    EXPECT_EQ(md_px(&m, 6, 6), RED_); EXPECT_EQ(md_px(&m, 30, 10), RED_);
    EXPECT_EQ(md_px(&m, 18, 8), WHITE_);
    U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(gap10_fillpath_gdi_object_count_stable) {
    NEED_PATH(); REQUIRE(U(FillPath));
    if (!g10_gdicount()) { t_skip("no GetGuiResources"); return; }
    HDC dc = mk(); DWORD before = g10_gdicount(); int i;
    for (i = 0; i < 200; i++) { square_path(dc, 0, 0, 5, 5); U(FillPath)(dc); }
    EXPECT_EQ(g10_gdicount(), before);
    U(DeleteDC)(dc);
}

/* ---------------- StrokeAndFillPath ---------------- */
TEST(gap10_safp_invalid_dc) {
    REQUIRE(U(StrokeAndFillPath));
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef }; unsigned i;
    for (i = 0; i < 3; i++) {
        CLR(); CHECKF(!U(StrokeAndFillPath)(bad[i]), "dc %p", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "dc %p err %lu", bad[i], GetLastError());
    }
}
TEST(gap10_safp_wrong_object_type) {
    REQUIRE(U(StrokeAndFillPath)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    HBRUSH b = U(CreateSolidBrush)(0);
    CLR();
    CHECK(!U(StrokeAndFillPath)((HDC)b));
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    U(DeleteObject)(b);
}
TEST(gap10_safp_no_path_fails) {
    NEED_PATH(); REQUIRE(U(StrokeAndFillPath));
    HDC dc = mk();
    CLR();
    CHECK(!U(StrokeAndFillPath)(dc));
    t_note("no path err %lu", GetLastError());
    U(DeleteDC)(dc);
}
TEST(gap10_safp_open_bracket_fails) {
    NEED_PATH(); REQUIRE(U(StrokeAndFillPath));
    HDC dc = mk();
    U(BeginPath)(dc); tri(dc, 0, 0, 9, 0, 9, 9);
    CHECK(!U(StrokeAndFillPath)(dc));
    U(AbortPath)(dc);
    U(DeleteDC)(dc);
}
TEST(gap10_safp_consumes_path) {
    NEED_PATH(); REQUIRE(U(StrokeAndFillPath));
    HDC dc = mk();
    square_path(dc, 0, 0, 9, 9);
    CHECK(U(StrokeAndFillPath)(dc));
    EXPECT_EQ(npts(dc), -1);
    CHECK(!U(StrokeAndFillPath)(dc));
    U(DeleteDC)(dc);
}
TEST(gap10_safp_fill_and_outline_pixels) {
    NEED_PATH(); REQUIRE(U(StrokeAndFillPath)); REQUIRE(U(CreatePen));
    int bpps[] = { 32, 24, 16 }; unsigned k;
    for (k = 0; k < 3; k++) {
        MD m; HPEN p, op; HBRUSH b; HGDIOBJ ob;
        if (!md_create(&m, 48, 48, bpps[k])) continue;
        md_clear(&m, WHITE_);
        p = U(CreatePen)(PS_SOLID, 5, BLUE_); op = U(SelectObject)(m.dc, p);
        b = U(CreateSolidBrush)(RED_); ob = U(SelectObject)(m.dc, b);
        square_path(m.dc, 10, 10, 38, 38);
        CHECK(U(StrokeAndFillPath)(m.dc));
        CHECKF(md_px(&m, 24, 24) == RED_, "bpp %d interior", bpps[k]);
        CHECKF(md_px(&m, 18, 22) == RED_ && md_px(&m, 30, 30) == RED_, "bpp %d interior2", bpps[k]);
        CHECKF(md_px(&m, 24, 10) == BLUE_ && md_px(&m, 10, 24) == BLUE_, "bpp %d outline a", bpps[k]);
        CHECKF(md_px(&m, 38, 24) == BLUE_ && md_px(&m, 24, 38) == BLUE_, "bpp %d outline b", bpps[k]);
        CHECKF(md_px(&m, 24, 8) == BLUE_ && md_px(&m, 24, 12) == BLUE_, "bpp %d outline thick", bpps[k]);
        CHECKF(md_px(&m, 2, 2) == WHITE_ && md_px(&m, 45, 45) == WHITE_ && md_px(&m, 24, 3) == WHITE_, "bpp %d outside", bpps[k]);
        U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, op); U(DeleteObject)(b); U(DeleteObject)(p);
        md_destroy(&m);
    }
}
TEST(gap10_safp_null_pen_equals_fillpath) {
    NEED_PATH(); REQUIRE(U(StrokeAndFillPath)); REQUIRE(U(FillPath)); REQUIRE(U(GetStockObject));
    MD a, b2; HBRUSH br; HGDIOBJ oa, ob, pa, pb; int x, y, same = 1;
    if (!md_create(&a, 32, 32, 32) || !md_create(&b2, 32, 32, 32)) { t_skip("no DIB"); return; }
    md_clear(&a, WHITE_); md_clear(&b2, WHITE_);
    br = U(CreateSolidBrush)(GREEN_);
    oa = U(SelectObject)(a.dc, br); ob = U(SelectObject)(b2.dc, br);
    pa = U(SelectObject)(a.dc, U(GetStockObject)(NULL_PEN)); pb = U(SelectObject)(b2.dc, U(GetStockObject)(NULL_PEN));
    star(a.dc); star(b2.dc);
    CHECK(U(StrokeAndFillPath)(a.dc)); CHECK(U(FillPath)(b2.dc));
    for (y = 0; y < 32; y++) for (x = 0; x < 32; x++) if (md_px(&a, x, y) != md_px(&b2, x, y)) same = 0;
    CHECK(same);
    U(SelectObject)(a.dc, oa); U(SelectObject)(b2.dc, ob); U(SelectObject)(a.dc, pa); U(SelectObject)(b2.dc, pb);
    U(DeleteObject)(br);
    md_destroy(&a); md_destroy(&b2);
}
TEST(gap10_safp_open_figure_closed) {
    NEED_PATH(); REQUIRE(U(StrokeAndFillPath)); REQUIRE(U(GetStockObject));
    MD m; HBRUSH b; HGDIOBJ ob, op;
    if (!md_create(&m, 32, 32, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    b = U(CreateSolidBrush)(RED_); ob = U(SelectObject)(m.dc, b);
    op = U(SelectObject)(m.dc, U(GetStockObject)(NULL_PEN));
    U(BeginPath)(m.dc); tri(m.dc, 4, 4, 28, 4, 28, 28); U(EndPath)(m.dc);
    CHECK(U(StrokeAndFillPath)(m.dc));
    EXPECT_EQ(md_px(&m, 22, 10), RED_);
    EXPECT_EQ(md_px(&m, 8, 24), WHITE_);
    U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, op); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(gap10_safp_vs_stroke_then_fill_order) {
    NEED_PATH(); REQUIRE(U(StrokeAndFillPath)); REQUIRE(U(CreatePen));
    /* stroke is drawn over the fill: the pen colour wins on the outline pixels even where the brush also covers them */
    MD m; HPEN p, op; HBRUSH b; HGDIOBJ ob;
    if (!md_create(&m, 32, 32, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    p = U(CreatePen)(PS_SOLID, 7, BLUE_); op = U(SelectObject)(m.dc, p);
    b = U(CreateSolidBrush)(RED_); ob = U(SelectObject)(m.dc, b);
    square_path(m.dc, 8, 8, 24, 24);
    CHECK(U(StrokeAndFillPath)(m.dc));
    EXPECT_EQ(md_px(&m, 16, 8), BLUE_);
    EXPECT_EQ(md_px(&m, 16, 10), BLUE_);
    EXPECT_EQ(md_px(&m, 16, 16), RED_);
    U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, op); U(DeleteObject)(b); U(DeleteObject)(p);
    md_destroy(&m);
}
TEST(gap10_safp_gdi_object_count_stable) {
    NEED_PATH(); REQUIRE(U(StrokeAndFillPath));
    if (!g10_gdicount()) { t_skip("no GetGuiResources"); return; }
    HDC dc = mk(); DWORD before = g10_gdicount(); int i;
    for (i = 0; i < 200; i++) { square_path(dc, 0, 0, 5, 5); U(StrokeAndFillPath)(dc); }
    EXPECT_EQ(g10_gdicount(), before);
    U(DeleteDC)(dc);
}

/* ---------------- FloodFill (== ExtFloodFill(FLOODFILLBORDER)) ---------------- */
static void frame(MD *m, int l, int t, int r, int b, COLORREF c) {   /* hollow 1px rectangle outline */
    md_rect(m, l, t, r - l, 1, c); md_rect(m, l, b - 1, r - l, 1, c);
    md_rect(m, l, t, 1, b - t, c); md_rect(m, r - 1, t, 1, b - t, c);
}
TEST(gap10_flood_invalid_dc) {
    REQUIRE(U(FloodFill));
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef }; unsigned i;
    for (i = 0; i < 3; i++) {
        CLR(); CHECKF(!U(FloodFill)(bad[i], 1, 1, 0), "dc %p", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "dc %p err %lu", bad[i], GetLastError());
    }
}
TEST(gap10_flood_wrong_object_type) {
    REQUIRE(U(FloodFill)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    HBRUSH b = U(CreateSolidBrush)(0);
    CLR();
    CHECK(!U(FloodFill)((HDC)b, 0, 0, 0));
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    U(DeleteObject)(b);
}
TEST(gap10_flood_fills_inside_border) {
    NEED_MD(); REQUIRE(U(FloodFill));
    int bpps[] = { 32, 24, 16 }; unsigned k;
    for (k = 0; k < 3; k++) {
        MD m; HBRUSH b; HGDIOBJ ob;
        if (!md_create(&m, 32, 32, bpps[k])) continue;
        md_clear(&m, WHITE_);
        frame(&m, 8, 8, 24, 24, RED_);
        b = U(CreateSolidBrush)(BLUE_); ob = U(SelectObject)(m.dc, b);
        CHECK(U(FloodFill)(m.dc, 16, 16, RED_));
        CHECKF(md_count(&m, 9, 9, 23, 23, BLUE_) == 14 * 14, "bpp %d inside %d", bpps[k], md_count(&m, 9, 9, 23, 23, BLUE_));
        CHECKF(md_count(&m, 0, 0, 32, 32, RED_) == 60, "bpp %d border intact %d", bpps[k], md_count(&m, 0, 0, 32, 32, RED_));
        CHECKF(md_px(&m, 2, 2) == WHITE_ && md_px(&m, 28, 28) == WHITE_, "bpp %d outside", bpps[k]);
        U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
        md_destroy(&m);
    }
}
TEST(gap10_flood_outside_start_fills_outside) {
    NEED_MD(); REQUIRE(U(FloodFill));
    MD m; HBRUSH b; HGDIOBJ ob;
    if (!md_create(&m, 32, 32, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    frame(&m, 8, 8, 24, 24, RED_);
    b = U(CreateSolidBrush)(GREEN_); ob = U(SelectObject)(m.dc, b);
    CHECK(U(FloodFill)(m.dc, 1, 1, RED_));
    EXPECT_EQ(md_px(&m, 0, 0), GREEN_); EXPECT_EQ(md_px(&m, 31, 31), GREEN_);
    EXPECT_EQ(md_px(&m, 16, 16), WHITE_);
    EXPECT_EQ(md_count(&m, 9, 9, 23, 23, GREEN_), 0);
    U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(gap10_flood_start_on_border_color_fails) {
    NEED_MD(); REQUIRE(U(FloodFill));
    MD m; HBRUSH b; HGDIOBJ ob;
    if (!md_create(&m, 32, 32, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    frame(&m, 8, 8, 24, 24, RED_);
    b = U(CreateSolidBrush)(BLUE_); ob = U(SelectObject)(m.dc, b);
    CLR();
    BOOL r = U(FloodFill)(m.dc, 8, 8, RED_);
    CHECK(!r);
    EXPECT_EQ(md_diff(&m, WHITE_), md_count(&m, 0, 0, 32, 32, RED_));
    U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(gap10_flood_outside_surface_fails) {
    NEED_MD(); REQUIRE(U(FloodFill));
    MD m; HBRUSH b; HGDIOBJ ob; int pts[][2] = { {-1, 5}, {5, -1}, {32, 5}, {5, 32}, {1000, 1000}, {-1000, -1000}, {INT_MAX, 0}, {0, INT_MIN} }; unsigned i;
    if (!md_create(&m, 32, 32, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    b = U(CreateSolidBrush)(BLUE_); ob = U(SelectObject)(m.dc, b);
    for (i = 0; i < sizeof pts / sizeof *pts; i++)
        CHECKF(!U(FloodFill)(m.dc, pts[i][0], pts[i][1], RED_), "pt %d,%d", pts[i][0], pts[i][1]);
    EXPECT_EQ(md_diff(&m, WHITE_), 0);
    U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(gap10_flood_no_border_fills_everything) {
    NEED_MD(); REQUIRE(U(FloodFill));
    MD m; HBRUSH b; HGDIOBJ ob;
    if (!md_create(&m, 24, 24, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    b = U(CreateSolidBrush)(BLUE_); ob = U(SelectObject)(m.dc, b);
    CHECK(U(FloodFill)(m.dc, 12, 12, RED_));
    EXPECT_EQ(md_count(&m, 0, 0, 24, 24, BLUE_), 24 * 24);
    U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(gap10_flood_leaky_border_spreads) {
    NEED_MD(); REQUIRE(U(FloodFill));
    MD m; HBRUSH b; HGDIOBJ ob;
    if (!md_create(&m, 32, 32, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    frame(&m, 8, 8, 24, 24, RED_);
    md_rect(&m, 16, 8, 1, 1, WHITE_);        /* gap in the top edge */
    b = U(CreateSolidBrush)(BLUE_); ob = U(SelectObject)(m.dc, b);
    CHECK(U(FloodFill)(m.dc, 16, 16, RED_));
    EXPECT_EQ(md_px(&m, 1, 1), BLUE_);
    EXPECT_EQ(md_px(&m, 16, 8), BLUE_);
    U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(gap10_flood_diagonal_gap_not_crossed) {
    NEED_MD(); REQUIRE(U(FloodFill));
    /* 4-connected fill must not leak through a diagonal corner */
    MD m; HBRUSH b; HGDIOBJ ob;
    if (!md_create(&m, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    int i;
    for (i = 0; i < 16; i++) { md_rect(&m, i, 15 - i, 1, 1, RED_); }       /* anti-diagonal line */
    b = U(CreateSolidBrush)(BLUE_); ob = U(SelectObject)(m.dc, b);
    CHECK(U(FloodFill)(m.dc, 2, 2, RED_));
    EXPECT_EQ(md_px(&m, 2, 2), BLUE_);
    EXPECT_EQ(md_px(&m, 13, 13), WHITE_);
    U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(gap10_flood_respects_clip) {
    NEED_MD(); REQUIRE(U(FloodFill)); REQUIRE(U(IntersectClipRect));
    MD m; HBRUSH b; HGDIOBJ ob;
    if (!md_create(&m, 32, 32, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    frame(&m, 4, 4, 28, 28, RED_);
    U(IntersectClipRect)(m.dc, 0, 0, 16, 32);
    b = U(CreateSolidBrush)(BLUE_); ob = U(SelectObject)(m.dc, b);
    CHECK(U(FloodFill)(m.dc, 10, 10, RED_));
    EXPECT_EQ(md_px(&m, 10, 10), BLUE_);
    EXPECT_EQ(md_px(&m, 20, 10), WHITE_);
    CLR();
    CHECK(!U(FloodFill)(m.dc, 20, 10, RED_));   /* start point outside the clip region */
    U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(gap10_flood_equals_extfloodfill_border) {
    NEED_MD(); REQUIRE(U(FloodFill)); REQUIRE(U(ExtFloodFill));
    MD a, b2; HBRUSH br; HGDIOBJ oa, ob; int x, y, same = 1;
    if (!md_create(&a, 32, 32, 32) || !md_create(&b2, 32, 32, 32)) { t_skip("no DIB"); return; }
    md_clear(&a, WHITE_); md_clear(&b2, WHITE_);
    frame(&a, 6, 6, 26, 26, RED_); frame(&b2, 6, 6, 26, 26, RED_);
    br = U(CreateSolidBrush)(GREEN_);
    oa = U(SelectObject)(a.dc, br); ob = U(SelectObject)(b2.dc, br);
    BOOL r1 = U(FloodFill)(a.dc, 15, 15, RED_), r2 = U(ExtFloodFill)(b2.dc, 15, 15, RED_, FLOODFILLBORDER);
    EXPECT_EQ(r1, r2);
    for (y = 0; y < 32; y++) for (x = 0; x < 32; x++) if (md_px(&a, x, y) != md_px(&b2, x, y)) same = 0;
    CHECK(same);
    U(SelectObject)(a.dc, oa); U(SelectObject)(b2.dc, ob); U(DeleteObject)(br);
    md_destroy(&a); md_destroy(&b2);
}
TEST(gap10_flood_uses_current_brush_pattern) {
    NEED_MD(); REQUIRE(U(FloodFill)); REQUIRE(U(CreateHatchBrush)); REQUIRE(U(SetBkColor));
    MD m; HBRUSH b; HGDIOBJ ob;
    if (!md_create(&m, 32, 32, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    frame(&m, 4, 4, 28, 28, GREEN_);
    b = U(CreateHatchBrush)(HS_VERTICAL, RED_); ob = U(SelectObject)(m.dc, b);
    U(SetBkColor)(m.dc, BLUE_);
    CHECK(U(FloodFill)(m.dc, 16, 16, GREEN_));
    int red = md_count(&m, 5, 5, 27, 27, RED_), blue = md_count(&m, 5, 5, 27, 27, BLUE_);
    CHECKF(red > 0 && blue > 0 && red + blue == 22 * 22, "red %d blue %d", red, blue);
    U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(gap10_flood_color_with_high_byte_observed) {
    NEED_MD(); REQUIRE(U(FloodFill));
    MD m; HBRUSH b; HGDIOBJ ob;
    if (!md_create(&m, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    frame(&m, 2, 2, 14, 14, RED_);
    b = U(CreateSolidBrush)(BLUE_); ob = U(SelectObject)(m.dc, b);
    BOOL r = U(FloodFill)(m.dc, 8, 8, RED_ | 0xFF000000);
    t_note("border colour with high byte 0xFF -> %d (inside blue count %d)", r, md_count(&m, 3, 3, 13, 13, BLUE_));
    U(SelectObject)(m.dc, ob); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(gap10_flood_mono_bitmap) {
    NEED_MD(); REQUIRE(U(FloodFill)); REQUIRE(U(CreateBitmap)); REQUIRE(U(GetStockObject));
    HDC dc = mk(); HBITMAP bm = U(CreateBitmap)(16, 16, 1, 1, NULL); HGDIOBJ ob = U(SelectObject)(dc, bm), obr;
    int x, y, black = 0;
    U(PatBlt)(dc, 0, 0, 16, 16, WHITENESS);
    U(MoveToEx)(dc, 0, 0, NULL);
    for (x = 3; x < 13; x++) { U(SetPixel)(dc, x, 3, 0); U(SetPixel)(dc, x, 12, 0); }
    for (y = 3; y < 13; y++) { U(SetPixel)(dc, 3, y, 0); U(SetPixel)(dc, 12, y, 0); }
    obr = U(SelectObject)(dc, U(GetStockObject)(BLACK_BRUSH));
    CHECK(U(FloodFill)(dc, 8, 8, 0));
    for (y = 0; y < 16; y++) for (x = 0; x < 16; x++) if (U(GetPixel)(dc, x, y) == 0) black++;
    EXPECT_EQ(black, 10 * 4 - 4 + 8 * 8);   /* outline (36) + filled interior (64) */
    EXPECT_EQ(U(GetPixel)(dc, 1, 1), WHITE_);
    U(SelectObject)(dc, obr); U(SelectObject)(dc, ob); U(DeleteObject)(bm); U(DeleteDC)(dc);
}

/* ---------------- LineDDA ---------------- */
typedef struct { int n; int x[1024], y[1024]; LPARAM lp; int stop_after; } DDA;
static void CALLBACK dda_cb(int x, int y, LPARAM lp) {
    DDA *d = (DDA *)lp;
    if (d->n < 1024) { d->x[d->n] = x; d->y[d->n] = y; }
    d->n++;
}
/* Reference: n = max(|dx|,|dy|) points starting at (x0,y0), the end point is not visited. */
static int ref_dda(int x0, int y0, int x1, int y1, int *xs, int *ys, int cap) {
    int dx = x1 - x0, dy = y1 - y0, adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    int sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1, n = 0, i, err;
    if (ady < adx) {
        err = adx / 2;
        for (i = adx; i > 0; i--) {
            if (n < cap) { xs[n] = x0; ys[n] = y0; } n++;
            x0 += sx; err -= ady;
            if (err < 0) { err += adx; y0 += sy; }
        }
    } else {
        err = ady / 2;
        for (i = ady; i > 0; i--) {
            if (n < cap) { xs[n] = x0; ys[n] = y0; } n++;
            y0 += sy; err -= adx;
            if (err < 0) { err += ady; x0 += sx; }
        }
    }
    return n;
}
static void dda_compare(int x0, int y0, int x1, int y1) {
    static DDA d; static int rx[1024], ry[1024]; int rn, i, same = 1;
    memset(&d, 0, sizeof d);
    BOOL r = U(LineDDA)(x0, y0, x1, y1, dda_cb, (LPARAM)&d);
    rn = ref_dda(x0, y0, x1, y1, rx, ry, 1024);
    CHECKF(r, "(%d,%d)-(%d,%d) returned FALSE", x0, y0, x1, y1);
    CHECKF(d.n == rn, "(%d,%d)-(%d,%d) count %d want %d", x0, y0, x1, y1, d.n, rn);
    for (i = 0; i < rn && i < d.n && i < 1024; i++) if (d.x[i] != rx[i] || d.y[i] != ry[i]) { same = 0; t_note("first diff at %d: (%d,%d) vs (%d,%d)", i, d.x[i], d.y[i], rx[i], ry[i]); break; }
    CHECKF(same, "(%d,%d)-(%d,%d) points differ", x0, y0, x1, y1);
}
TEST(gap10_linedda_null_proc) {
    REQUIRE(U(LineDDA)); SKIP_WINE("LineDDA calls a NULL callback");
    CLR();
    CHECK(!U(LineDDA)(0, 0, 10, 10, NULL, 0));
    CHECK(!U(LineDDA)(5, 5, 5, 5, NULL, 0));
    t_note("NULL proc: err %lu", GetLastError());
}
TEST(gap10_linedda_horizontal_exact) {
    REQUIRE(U(LineDDA));
    DDA d; int i;
    memset(&d, 0, sizeof d);
    CHECK(U(LineDDA)(3, 7, 9, 7, dda_cb, (LPARAM)&d));
    EXPECT_EQ(d.n, 6);
    for (i = 0; i < 6 && i < d.n; i++) { CHECKF(d.x[i] == 3 + i && d.y[i] == 7, "pt %d (%d,%d)", i, d.x[i], d.y[i]); }
}
TEST(gap10_linedda_vertical_exact) {
    REQUIRE(U(LineDDA));
    DDA d; int i;
    memset(&d, 0, sizeof d);
    CHECK(U(LineDDA)(4, 2, 4, 10, dda_cb, (LPARAM)&d));
    EXPECT_EQ(d.n, 8);
    for (i = 0; i < 8 && i < d.n; i++) CHECKF(d.x[i] == 4 && d.y[i] == 2 + i, "pt %d", i);
}
TEST(gap10_linedda_reverse_direction) {
    REQUIRE(U(LineDDA));
    DDA d; int i;
    memset(&d, 0, sizeof d);
    CHECK(U(LineDDA)(9, 7, 3, 7, dda_cb, (LPARAM)&d));
    EXPECT_EQ(d.n, 6);
    for (i = 0; i < 6 && i < d.n; i++) CHECKF(d.x[i] == 9 - i && d.y[i] == 7, "pt %d", i);
}
TEST(gap10_linedda_endpoint_excluded) {
    REQUIRE(U(LineDDA));
    DDA d;
    memset(&d, 0, sizeof d);
    U(LineDDA)(0, 0, 10, 3, dda_cb, (LPARAM)&d);
    CHECK(d.n == 10);
    CHECK(!(d.x[d.n - 1] == 10 && d.y[d.n - 1] == 3));
    CHECK(d.x[0] == 0 && d.y[0] == 0);
}
TEST(gap10_linedda_zero_length) {
    REQUIRE(U(LineDDA));
    DDA d;
    memset(&d, 0, sizeof d);
    BOOL r = U(LineDDA)(5, 5, 5, 5, dda_cb, (LPARAM)&d);
    EXPECT_EQ(d.n, 0);
    t_note("zero-length LineDDA returned %d", r);
}
TEST(gap10_linedda_single_step) {
    REQUIRE(U(LineDDA));
    DDA d;
    memset(&d, 0, sizeof d);
    CHECK(U(LineDDA)(5, 5, 6, 5, dda_cb, (LPARAM)&d));
    EXPECT_EQ(d.n, 1); EXPECT_EQ(d.x[0], 5); EXPECT_EQ(d.y[0], 5);
    memset(&d, 0, sizeof d);
    CHECK(U(LineDDA)(5, 5, 6, 6, dda_cb, (LPARAM)&d));
    EXPECT_EQ(d.n, 1);
}
TEST(gap10_linedda_diagonal_all_octants) {
    REQUIRE(U(LineDDA));
    int dx[] = { 1, 1, 0, -1, -1, -1, 0, 1 }, dy[] = { 0, 1, 1, 1, 0, -1, -1, -1 }; int i;
    for (i = 0; i < 8; i++) dda_compare(20, 20, 20 + dx[i] * 15, 20 + dy[i] * 15);
}
TEST(gap10_linedda_shallow_slopes_match_bresenham) {
    REQUIRE(U(LineDDA));
    int t[][4] = { {0,0,10,3}, {0,0,10,1}, {0,0,17,5}, {0,0,100,7}, {0,0,99,98}, {3,3,50,40}, {0,0,10,9} }; unsigned i;
    for (i = 0; i < sizeof t / sizeof *t; i++) dda_compare(t[i][0], t[i][1], t[i][2], t[i][3]);
}
TEST(gap10_linedda_steep_slopes_match_bresenham) {
    REQUIRE(U(LineDDA));
    int t[][4] = { {0,0,3,10}, {0,0,1,10}, {0,0,5,17}, {0,0,7,100}, {2,2,40,50}, {0,0,9,10} }; unsigned i;
    for (i = 0; i < sizeof t / sizeof *t; i++) dda_compare(t[i][0], t[i][1], t[i][2], t[i][3]);
}
TEST(gap10_linedda_negative_directions_match) {
    REQUIRE(U(LineDDA));
    int t[][4] = { {10,10,0,7}, {10,10,0,13}, {0,0,-10,-3}, {0,0,-3,-10}, {0,0,-10,3}, {0,0,3,-10}, {-5,-5,-20,-8}, {50,50,40,10} }; unsigned i;
    for (i = 0; i < sizeof t / sizeof *t; i++) dda_compare(t[i][0], t[i][1], t[i][2], t[i][3]);
}
TEST(gap10_linedda_negative_origin_coordinates) {
    REQUIRE(U(LineDDA));
    dda_compare(-100, -100, -90, -95);
    dda_compare(-1, -1, 1, 1);
    dda_compare(-8, 4, 8, -4);
}
TEST(gap10_linedda_equal_dx_dy_prefers_diagonal) {
    REQUIRE(U(LineDDA));
    DDA d; int i;
    memset(&d, 0, sizeof d);
    CHECK(U(LineDDA)(0, 0, 8, 8, dda_cb, (LPARAM)&d));
    EXPECT_EQ(d.n, 8);
    for (i = 0; i < 8 && i < d.n; i++) CHECKF(d.x[i] == i && d.y[i] == i, "pt %d (%d,%d)", i, d.x[i], d.y[i]);
}
static void CALLBACK dda_lparam_cb(int x, int y, LPARAM lp) { LPARAM *p = (LPARAM *)lp; p[0]++; p[1] = lp; }
TEST(gap10_linedda_lparam_value) {
    REQUIRE(U(LineDDA));
    LPARAM st[2] = { 0, 0 };
    U(LineDDA)(0, 0, 6, 2, dda_lparam_cb, (LPARAM)st);
    EXPECT_EQ(st[0], 6);
    EXPECT_EQ(st[1], (LPARAM)st);
}
static void CALLBACK dda_big_cb(int x, int y, LPARAM lp) { (*(long *)lp)++; }
TEST(gap10_linedda_large_lines_counted) {
    REQUIRE(U(LineDDA));
    long n = 0;
    CHECK(U(LineDDA)(0, 0, 50000, 3, dda_big_cb, (LPARAM)&n));
    EXPECT_EQ(n, 50000);
    n = 0;
    CHECK(U(LineDDA)(0, 0, 7, -60000, dda_big_cb, (LPARAM)&n));
    EXPECT_EQ(n, 60000);
    n = 0;
    CHECK(U(LineDDA)(-30000, -30000, 30000, 30000, dda_big_cb, (LPARAM)&n));
    EXPECT_EQ(n, 60000);
}
static void CALLBACK dda_ranges_cb(int x, int y, LPARAM lp) {
    int *r = (int *)lp;   /* min/max tracking */
    if (r[4]++ == 0) { r[0] = r[1] = x; r[2] = r[3] = y; }
    if (x < r[0]) r[0] = x;
    if (x > r[1]) r[1] = x;
    if (y < r[2]) r[2] = y;
    if (y > r[3]) r[3] = y;
}
TEST(gap10_linedda_stays_in_bounding_box) {
    REQUIRE(U(LineDDA));
    int r[5] = { 0 };
    U(LineDDA)(10, 40, 90, 12, dda_ranges_cb, (LPARAM)r);
    CHECK(r[0] == 10 && r[1] == 89);
    CHECK(r[2] >= 12 && r[2] <= 14 && r[3] == 40);
    EXPECT_EQ(r[4], 80);
}
TEST(gap10_linedda_symmetry_under_swap_observed) {
    REQUIRE(U(LineDDA));
    static DDA a, b; int i, same = 1;
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    U(LineDDA)(0, 0, 20, 7, dda_cb, (LPARAM)&a);
    U(LineDDA)(20, 7, 0, 0, dda_cb, (LPARAM)&b);
    EXPECT_EQ(a.n, b.n);
    for (i = 0; i < a.n && i < 1024; i++) if (a.x[i] != b.x[a.n - 1 - i] + 0 || a.y[i] != b.y[a.n - 1 - i]) same = 0;
    t_note("LineDDA reverse symmetric: %d (end point excluded, so off by one is expected)", same);
}
TEST(gap10_linedda_no_dc_required_and_no_error) {
    REQUIRE(U(LineDDA));
    DDA d; memset(&d, 0, sizeof d);
    CLR();
    U(LineDDA)(0, 0, 4, 4, dda_cb, (LPARAM)&d);
    EXPECT_EQ(GetLastError(), ERRS);
}
static void CALLBACK dda_nest_cb(int x, int y, LPARAM lp) {
    DDA *d = (DDA *)lp;
    if (d->n < 1024) { d->x[d->n] = x; d->y[d->n] = y; }
    d->n++;
}
TEST(gap10_linedda_reentrant_in_callback) {
    REQUIRE(U(LineDDA));
    DDA d; memset(&d, 0, sizeof d);
    U(LineDDA)(0, 0, 3, 0, dda_nest_cb, (LPARAM)&d);
    U(LineDDA)(0, 0, 0, 3, dda_nest_cb, (LPARAM)&d);
    EXPECT_EQ(d.n, 6);
}
TEST(gap10_linedda_gdi32_forwarder_same) {
    REQUIRE(UG(LineDDA));
    DDA d; memset(&d, 0, sizeof d);
    CHECK(UG(LineDDA)(0, 0, 9, 4, dda_cb, (LPARAM)&d));
    EXPECT_EQ(d.n, 9);
    if (UF(LineDDA)) { DDA e; memset(&e, 0, sizeof e); UF(LineDDA)(0, 0, 9, 4, dda_cb, (LPARAM)&e); EXPECT_EQ(e.n, 9); CHECK(!memcmp(d.x, e.x, 9 * sizeof(int))); }
}
