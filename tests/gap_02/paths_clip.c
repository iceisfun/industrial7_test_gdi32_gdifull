/* BeginPathImpl / EndPathImpl / CloseFigureImpl / FillPathImpl / LineToImpl /
 * GetClipBoxImpl / IntersectClipRectImpl / ExtSelectClipRgnImpl.
 * Rendering checks use 32bpp DIB sections in memory DCs only. */
#include "g2.h"

#define WHITE_PX 0xffffff
#define RED_PX   G2_PX(255, 0, 0)

static int white_dib(G2DC *d, int w, int h) {
    if (!g2_mk(d, w, h)) return 0;
    g2_fill(d, WHITE_PX);
    return 1;
}
/* every EMF/WMF record type count */
static int emf_count(HENHMETAFILE h, DWORD type) {
    UINT n = U(GetEnhMetaFileBits)(h, 0, NULL);
    if (!n) return -1;
    BYTE *b = malloc(n);
    if (!b) return -1;
    U(GetEnhMetaFileBits)(h, n, b);
    int cnt = 0; DWORD off = 0;
    while (off + 8 <= n) {
        DWORD t = *(DWORD *)(b + off), sz = *(DWORD *)(b + off + 4);
        if (sz < 8 || off + sz > n) break;
        if (t == type) cnt++;
        off += sz;
    }
    free(b);
    return cnt;
}
static BOOL rc_eq(const RECT *r, int l, int t, int rr, int b) { return r->left == l && r->top == t && r->right == rr && r->bottom == b; }
#define BOX(dc, rc) (memset(&(rc), 0x7e, sizeof(rc)), IMPL(GetClipBox)((dc), &(rc)))

/* closed rectangle polygon through MoveTo/LineTo/CloseFigure (inside a path bracket) */
static void poly_rect(HDC dc, int l, int t, int r, int b) {
    U(MoveToEx)(dc, l, t, NULL);
    IMPL(LineTo)(dc, r, t); IMPL(LineTo)(dc, r, b); IMPL(LineTo)(dc, l, b);
    IMPL(CloseFigure)(dc);
}

/* ====================================================== paths ====================================================== */
#define NEED_PATH() do { REQUIRE(IMPL(BeginPath)); REQUIRE(IMPL(EndPath)); REQUIRE(IMPL(CloseFigure)); REQUIRE(IMPL(FillPath)); \
    REQUIRE(IMPL(LineTo)); REQUIRE(U(MoveToEx)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(SelectObject)); REQUIRE(U(CreateCompatibleDC)); } while (0)

TEST(gap2_path_begin_end_memdc) {
    NEED_PATH();
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(IMPL(BeginPath)(dc));
    CHECK(IMPL(EndPath)(dc));
    U(DeleteDC)(dc);
}
TEST(gap2_path_invalid_handles_all) {
    NEED_PATH();
    for (int i = 0; i < G2_NJUNK; i++) {
        HDC h = (HDC)g2_junk[i];
        SetLastError(0); CHECKF(!IMPL(BeginPath)(h), "BeginPath %d", i);   EXPECT_ERR(0, ERROR_INVALID_HANDLE);
        SetLastError(0); CHECKF(!IMPL(EndPath)(h), "EndPath %d", i);       EXPECT_ERR(0, ERROR_INVALID_HANDLE);
        SetLastError(0); CHECKF(!IMPL(CloseFigure)(h), "CloseFigure %d", i); EXPECT_ERR(0, ERROR_INVALID_HANDLE);
        SetLastError(0); CHECKF(!IMPL(FillPath)(h), "FillPath %d", i);     EXPECT_ERR(0, ERROR_INVALID_HANDLE);
        SetLastError(0); CHECKF(!IMPL(LineTo)(h, 1, 1), "LineTo %d", i);   EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    }
}
TEST(gap2_path_stale_dc) {
    NEED_PATH();
    HDC dc = g2_stale_dc();
    CHECK(!IMPL(BeginPath)(dc)); CHECK(!IMPL(EndPath)(dc)); CHECK(!IMPL(CloseFigure)(dc));
    CHECK(!IMPL(FillPath)(dc)); CHECK(!IMPL(LineTo)(dc, 1, 1));
}
TEST(gap2_path_non_dc_objects) {
    NEED_PATH();
    HGDIOBJ o[3] = { U(CreateSolidBrush)(0), U(CreateRectRgn)(0, 0, 1, 1), U(CreateBitmap)(1, 1, 1, 1, NULL) };
    for (int i = 0; i < 3; i++) {
        HDC h = (HDC)o[i];
        CHECKF(!IMPL(BeginPath)(h), "%d", i); CHECKF(!IMPL(EndPath)(h), "%d", i); CHECKF(!IMPL(CloseFigure)(h), "%d", i);
        CHECKF(!IMPL(FillPath)(h), "%d", i); CHECKF(!IMPL(LineTo)(h, 1, 1), "%d", i);
        CHECKF(U(GetObjectType)(o[i]) != 0, "obj %d damaged", i);
        U(DeleteObject)(o[i]);
    }
}
TEST(gap2_path_end_without_begin) {
    NEED_PATH();
    HDC dc = U(CreateCompatibleDC)(NULL);
    SetLastError(0);
    BOOL r = IMPL(EndPath)(dc);
    CHECK(!r);
    t_note("EndPath without BeginPath: err %lu", GetLastError());
    U(DeleteDC)(dc);
}
TEST(gap2_path_close_figure_without_path) {
    NEED_PATH();
    HDC dc = U(CreateCompatibleDC)(NULL);
    SetLastError(0);
    BOOL r = IMPL(CloseFigure)(dc);
    CHECK(!r);
    t_note("CloseFigure outside a path: err %lu", GetLastError());
    U(DeleteDC)(dc);
}
TEST(gap2_path_fill_without_path) {
    NEED_PATH();
    HDC dc = U(CreateCompatibleDC)(NULL);
    SetLastError(0);
    BOOL r = IMPL(FillPath)(dc);
    CHECK(!r);
    t_note("FillPath without path: err %lu", GetLastError());
    U(DeleteDC)(dc);
}
TEST(gap2_path_fill_open_bracket) {
    NEED_PATH();
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(IMPL(BeginPath)(dc));
    U(MoveToEx)(dc, 0, 0, NULL); IMPL(LineTo)(dc, 4, 4);
    SetLastError(0);
    BOOL r = IMPL(FillPath)(dc);
    t_note("FillPath inside an open bracket -> %d err %lu", r, GetLastError());
    U(AbortPath)(dc);
    U(DeleteDC)(dc);
}
TEST(gap2_path_begin_twice) {
    NEED_PATH();
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(IMPL(BeginPath)(dc));
    CHECK(IMPL(BeginPath)(dc));
    CHECK(IMPL(EndPath)(dc));
    SetLastError(0);
    CHECK(!IMPL(EndPath)(dc));   /* no open bracket any more */
    U(DeleteDC)(dc);
}
TEST(gap2_path_rectangle_fill_exact) {
    NEED_PATH();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
    HGDIOBJ ob = U(SelectObject)(d.dc, br);
    CHECK(IMPL(BeginPath)(d.dc));
    CHECK(U(Rectangle)(d.dc, 2, 3, 7, 9));
    CHECK(IMPL(EndPath)(d.dc));
    CHECK(IMPL(FillPath)(d.dc));
    t_note("Rectangle() path fill covers %d px (bounds 5x6 = 30)", g2_count(&d, RED_PX));
    CHECK(g2_px(&d, 4, 5) == RED_PX);
    CHECK(g2_px(&d, 1, 5) == WHITE_PX && g2_px(&d, 4, 2) == WHITE_PX && g2_px(&d, 7, 5) == WHITE_PX && g2_px(&d, 4, 9) == WHITE_PX);
    CHECK(g2_count(&d, RED_PX) <= 30 && g2_count(&d, RED_PX) >= 12);
    U(SelectObject)(d.dc, ob); U(DeleteObject)(br);
    g2_free(&d);
}
TEST(gap2_path_polygon_fill_closefigure) {
    NEED_PATH();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
    HGDIOBJ ob = U(SelectObject)(d.dc, br);
    CHECK(IMPL(BeginPath)(d.dc));
    U(MoveToEx)(d.dc, 2, 2, NULL);
    CHECK(IMPL(LineTo)(d.dc, 10, 2));
    CHECK(IMPL(LineTo)(d.dc, 10, 8));
    CHECK(IMPL(LineTo)(d.dc, 2, 8));
    CHECK(IMPL(CloseFigure)(d.dc));
    CHECK(IMPL(EndPath)(d.dc));
    CHECK(IMPL(FillPath)(d.dc));
    G2_EQ(g2_count(&d, RED_PX), 8 * 6);
    CHECK(g2_px(&d, 2, 2) == RED_PX && g2_px(&d, 9, 7) == RED_PX);
    CHECK(g2_px(&d, 10, 5) == WHITE_PX && g2_px(&d, 5, 8) == WHITE_PX && g2_px(&d, 1, 5) == WHITE_PX);
    U(SelectObject)(d.dc, ob); U(DeleteObject)(br);
    g2_free(&d);
}
TEST(gap2_path_fill_closes_open_figure) {
    NEED_PATH();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
    HGDIOBJ ob = U(SelectObject)(d.dc, br);
    CHECK(IMPL(BeginPath)(d.dc));
    U(MoveToEx)(d.dc, 2, 2, NULL);
    IMPL(LineTo)(d.dc, 10, 2); IMPL(LineTo)(d.dc, 10, 8); IMPL(LineTo)(d.dc, 2, 8);   /* no CloseFigure */
    CHECK(IMPL(EndPath)(d.dc));
    CHECK(IMPL(FillPath)(d.dc));
    G2_EQ(g2_count(&d, RED_PX), 8 * 6);
    U(SelectObject)(d.dc, ob); U(DeleteObject)(br);
    g2_free(&d);
}
TEST(gap2_path_fill_discards_path) {
    NEED_PATH();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
    HGDIOBJ ob = U(SelectObject)(d.dc, br);
    IMPL(BeginPath)(d.dc); poly_rect(d.dc, 1, 1, 4, 4); IMPL(EndPath)(d.dc);
    CHECK(IMPL(FillPath)(d.dc));
    SetLastError(0);
    CHECK(!IMPL(FillPath)(d.dc));   /* the path was consumed */
    G2_EQ(g2_count(&d, RED_PX), 9);
    U(SelectObject)(d.dc, ob); U(DeleteObject)(br);
    g2_free(&d);
}
TEST(gap2_path_fill_modes_alternate_winding) {
    NEED_PATH(); REQUIRE(U(SetPolyFillMode));
    static const int modes[2] = { ALTERNATE, WINDING };
    static const int want[2] = { 12 * 12 - 4 * 4, 12 * 12 };
    for (int m = 0; m < 2; m++) {
        G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
        HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
        HGDIOBJ ob = U(SelectObject)(d.dc, br);
        U(SetPolyFillMode)(d.dc, modes[m]);
        IMPL(BeginPath)(d.dc);
        U(MoveToEx)(d.dc, 0, 0, NULL); IMPL(LineTo)(d.dc, 12, 0); IMPL(LineTo)(d.dc, 12, 12); IMPL(LineTo)(d.dc, 0, 12); IMPL(CloseFigure)(d.dc);
        U(MoveToEx)(d.dc, 4, 4, NULL); IMPL(LineTo)(d.dc, 8, 4); IMPL(LineTo)(d.dc, 8, 8); IMPL(LineTo)(d.dc, 4, 8); IMPL(CloseFigure)(d.dc);
        IMPL(EndPath)(d.dc);
        CHECK(IMPL(FillPath)(d.dc));
        CHECKF(g2_count(&d, RED_PX) == want[m], "mode %d: %d red, want %d", modes[m], g2_count(&d, RED_PX), want[m]);
        U(SelectObject)(d.dc, ob); U(DeleteObject)(br);
        g2_free(&d);
    }
}
TEST(gap2_path_fill_uses_brush_not_pen) {
    NEED_PATH(); REQUIRE(U(CreatePen));
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
    HPEN pen = U(CreatePen)(PS_SOLID, 1, RGB(0, 0, 255));
    HGDIOBJ ob = U(SelectObject)(d.dc, br), op = U(SelectObject)(d.dc, pen);
    IMPL(BeginPath)(d.dc); poly_rect(d.dc, 2, 2, 8, 8); IMPL(EndPath)(d.dc);
    CHECK(IMPL(FillPath)(d.dc));
    G2_EQ(g2_count(&d, G2_PX(0, 0, 255)), 0);
    G2_EQ(g2_count(&d, RED_PX), 36);
    U(SelectObject)(d.dc, ob); U(SelectObject)(d.dc, op); U(DeleteObject)(br); U(DeleteObject)(pen);
    g2_free(&d);
}
TEST(gap2_path_fill_null_brush) {
    NEED_PATH(); REQUIRE(U(GetStockObject));
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HGDIOBJ ob = U(SelectObject)(d.dc, U(GetStockObject)(NULL_BRUSH));
    IMPL(BeginPath)(d.dc); U(Rectangle)(d.dc, 2, 2, 8, 8); IMPL(EndPath)(d.dc);
    CHECK(IMPL(FillPath)(d.dc));
    G2_EQ(g2_count(&d, WHITE_PX), 256);
    U(SelectObject)(d.dc, ob);
    g2_free(&d);
}
TEST(gap2_path_fill_respects_clip) {
    NEED_PATH(); REQUIRE(U(IntersectClipRect));
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
    HGDIOBJ ob = U(SelectObject)(d.dc, br);
    U(IntersectClipRect)(d.dc, 4, 4, 8, 8);
    IMPL(BeginPath)(d.dc); poly_rect(d.dc, 0, 0, 16, 16); IMPL(EndPath)(d.dc);
    CHECK(IMPL(FillPath)(d.dc));
    G2_EQ(g2_count(&d, RED_PX), 16);
    U(SelectObject)(d.dc, ob); U(DeleteObject)(br);
    g2_free(&d);
}
TEST(gap2_path_fill_with_dc_brush_color) {
    NEED_PATH(); REQUIRE(U(SetDCBrushColor)); REQUIRE(U(GetStockObject));
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HGDIOBJ ob = U(SelectObject)(d.dc, U(GetStockObject)(DC_BRUSH));
    U(SetDCBrushColor)(d.dc, RGB(0, 255, 0));
    IMPL(BeginPath)(d.dc); poly_rect(d.dc, 1, 1, 5, 5); IMPL(EndPath)(d.dc);
    CHECK(IMPL(FillPath)(d.dc));
    G2_EQ(g2_count(&d, G2_PX(0, 255, 0)), 16);
    U(SelectObject)(d.dc, ob);
    g2_free(&d);
}
TEST(gap2_path_drawing_suppressed_in_bracket) {
    NEED_PATH(); REQUIRE(U(GetStockObject));
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HGDIOBJ op = U(SelectObject)(d.dc, U(GetStockObject)(BLACK_PEN));
    IMPL(BeginPath)(d.dc);
    U(MoveToEx)(d.dc, 0, 5, NULL); IMPL(LineTo)(d.dc, 12, 5);
    U(Rectangle)(d.dc, 2, 2, 9, 9);
    IMPL(EndPath)(d.dc);
    G2_EQ(g2_count(&d, WHITE_PX), 256);
    U(SelectObject)(d.dc, op);
    g2_free(&d);
}
TEST(gap2_path_drawing_resumes_after_end) {
    NEED_PATH(); REQUIRE(U(GetStockObject));
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HGDIOBJ op = U(SelectObject)(d.dc, U(GetStockObject)(BLACK_PEN));
    IMPL(BeginPath)(d.dc); U(Rectangle)(d.dc, 1, 1, 4, 4); IMPL(EndPath)(d.dc);
    U(MoveToEx)(d.dc, 0, 10, NULL);
    CHECK(IMPL(LineTo)(d.dc, 6, 10));
    G2_EQ(g2_count(&d, 0), 6);
    U(SelectObject)(d.dc, op);
    g2_free(&d);
}
TEST(gap2_path_getpath_points_and_types) {
    NEED_PATH(); REQUIRE(U(GetPath));
    HDC dc = U(CreateCompatibleDC)(NULL);
    IMPL(BeginPath)(dc);
    U(MoveToEx)(dc, 1, 1, NULL); IMPL(LineTo)(dc, 5, 1); IMPL(LineTo)(dc, 5, 5);
    CHECK(IMPL(CloseFigure)(dc));
    CHECK(IMPL(EndPath)(dc));
    int n = U(GetPath)(dc, NULL, NULL, 0);
    G2_EQ(n, 3);
    POINT pts[4] = { { -1, -1 }, { -1, -1 }, { -1, -1 }, { -1, -1 } }; BYTE ty[4] = { 0xee, 0xee, 0xee, 0xee };
    G2_EQ(U(GetPath)(dc, pts, ty, 4), 3);
    CHECK(pts[0].x == 1 && pts[0].y == 1 && pts[1].x == 5 && pts[1].y == 1 && pts[2].x == 5 && pts[2].y == 5);
    CHECK(ty[0] == PT_MOVETO);
    CHECK(ty[1] == PT_LINETO);
    CHECK(ty[2] == (PT_LINETO | PT_CLOSEFIGURE));
    CHECK(ty[3] == 0xee);
    U(AbortPath)(dc);
    U(DeleteDC)(dc);
}
TEST(gap2_path_closefigure_marks_only_last_point) {
    NEED_PATH(); REQUIRE(U(GetPath));
    HDC dc = U(CreateCompatibleDC)(NULL);
    IMPL(BeginPath)(dc);
    U(MoveToEx)(dc, 0, 0, NULL); IMPL(LineTo)(dc, 4, 0); IMPL(LineTo)(dc, 4, 4);
    IMPL(CloseFigure)(dc);
    U(MoveToEx)(dc, 10, 10, NULL); IMPL(LineTo)(dc, 14, 10);
    IMPL(EndPath)(dc);
    POINT pts[8]; BYTE ty[8];
    int n = U(GetPath)(dc, pts, ty, 8);
    G2_EQ(n, 5);
    if (n == 5) {
        CHECK(ty[0] == PT_MOVETO && ty[1] == PT_LINETO && ty[2] == (PT_LINETO | PT_CLOSEFIGURE));
        CHECK(ty[3] == PT_MOVETO && ty[4] == PT_LINETO);
    }
    U(AbortPath)(dc);
    U(DeleteDC)(dc);
}
TEST(gap2_path_double_closefigure) {
    NEED_PATH();
    HDC dc = U(CreateCompatibleDC)(NULL);
    IMPL(BeginPath)(dc);
    U(MoveToEx)(dc, 0, 0, NULL); IMPL(LineTo)(dc, 4, 0); IMPL(LineTo)(dc, 4, 4);
    CHECK(IMPL(CloseFigure)(dc));
    SetLastError(0);
    BOOL r = IMPL(CloseFigure)(dc);
    t_note("second CloseFigure -> %d err %lu", r, GetLastError());
    U(AbortPath)(dc);
    U(DeleteDC)(dc);
}
TEST(gap2_path_closefigure_on_closed_bracket_fails) {
    NEED_PATH();
    HDC dc = U(CreateCompatibleDC)(NULL);
    IMPL(BeginPath)(dc);
    U(MoveToEx)(dc, 0, 0, NULL); IMPL(LineTo)(dc, 4, 0);
    IMPL(EndPath)(dc);
    SetLastError(0);
    CHECK(!IMPL(CloseFigure)(dc));
    t_note("CloseFigure after EndPath: err %lu", GetLastError());
    U(AbortPath)(dc);
    U(DeleteDC)(dc);
}
TEST(gap2_path_rectangle_point_count) {
    NEED_PATH(); REQUIRE(U(GetPath));
    HDC dc = U(CreateCompatibleDC)(NULL);
    IMPL(BeginPath)(dc); U(Rectangle)(dc, 2, 2, 8, 8); IMPL(EndPath)(dc);
    int n = U(GetPath)(dc, NULL, NULL, 0);
    t_note("Rectangle path point count %d", n);
    CHECK(n >= 4);
    U(AbortPath)(dc);
    U(DeleteDC)(dc);
}
TEST(gap2_path_cp_inside_bracket) {
    NEED_PATH(); REQUIRE(U(GetCurrentPositionEx));
    HDC dc = U(CreateCompatibleDC)(NULL);
    IMPL(BeginPath)(dc);
    U(MoveToEx)(dc, 3, 4, NULL);
    IMPL(LineTo)(dc, 9, 11);
    POINT p = { -1, -1 };
    CHECK(U(GetCurrentPositionEx)(dc, &p));
    CHECK(p.x == 9 && p.y == 11);
    IMPL(EndPath)(dc);
    U(AbortPath)(dc);
    U(DeleteDC)(dc);
}
TEST(gap2_path_bracket_state_per_dc) {
    NEED_PATH();
    HDC a = U(CreateCompatibleDC)(NULL), b = U(CreateCompatibleDC)(NULL);
    CHECK(IMPL(BeginPath)(a));
    SetLastError(0);
    CHECK(!IMPL(EndPath)(b));   /* b never began a bracket */
    CHECK(IMPL(EndPath)(a));
    U(AbortPath)(a);
    U(DeleteDC)(a); U(DeleteDC)(b);
}
TEST(gap2_path_delete_dc_with_open_bracket) {
    NEED_PATH();
    HDC dc = U(CreateCompatibleDC)(NULL);
    IMPL(BeginPath)(dc);
    U(MoveToEx)(dc, 0, 0, NULL); IMPL(LineTo)(dc, 3, 3);
    DWORD base = g2_gdi_count();
    CHECK(U(DeleteDC)(dc));
    if (base) G2_EQ(g2_gdi_count(), base - 1);
}
TEST(gap2_path_savedc_restores_path_state) {
    NEED_PATH(); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    int s = U(SaveDC)(dc);
    IMPL(BeginPath)(dc);
    U(MoveToEx)(dc, 0, 0, NULL); IMPL(LineTo)(dc, 5, 5);
    CHECK(U(RestoreDC)(dc, s));
    SetLastError(0);
    BOOL r = IMPL(EndPath)(dc);
    t_note("EndPath after RestoreDC dropped the bracket -> %d", r);
    U(AbortPath)(dc);
    U(DeleteDC)(dc);
}
TEST(gap2_path_wmf_dc_returns_false) {
    NEED_PATH(); REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    SKIP_ON_WINE("WMF path handling differs");
    HDC m = U(CreateMetaFileW)(NULL);
    if (!m) { t_skip("no WMF DC"); return; }
    SetLastError(0x1111);
    CHECK(!IMPL(BeginPath)(m)); CHECK(!IMPL(EndPath)(m)); CHECK(!IMPL(CloseFigure)(m)); CHECK(!IMPL(FillPath)(m));
    EXPECT_ERR(0, 0x1111);   /* refused without setting an error */
    HMETAFILE h = U(CloseMetaFile)(m); if (h) U(DeleteMetaFile)(h);
}
TEST(gap2_path_emf_records) {
    NEED_PATH(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits));
    SKIP_ON_WINE("wine records different EMF stream");
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    CHECK(IMPL(BeginPath)(m));
    U(MoveToEx)(m, 1, 1, NULL);
    CHECK(IMPL(LineTo)(m, 10, 1));
    CHECK(IMPL(LineTo)(m, 10, 10));
    CHECK(IMPL(CloseFigure)(m));
    CHECK(IMPL(EndPath)(m));
    CHECK(IMPL(FillPath)(m));
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    CHECK(h != NULL); if (!h) return;
    G2_EQ(emf_count(h, EMR_BEGINPATH), 1);
    G2_EQ(emf_count(h, EMR_ENDPATH), 1);
    G2_EQ(emf_count(h, EMR_CLOSEFIGURE), 1);
    G2_EQ(emf_count(h, EMR_FILLPATH), 1);
    G2_EQ(emf_count(h, EMR_LINETO), 2);
    G2_EQ(emf_count(h, EMR_MOVETOEX), 1);
    U(DeleteEnhMetaFile)(h);
}
TEST(gap2_path_emf_stroke_free_paths) {
    NEED_PATH(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits));
    SKIP_ON_WINE("wine records different EMF stream");
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    for (int i = 0; i < 4; i++) { IMPL(BeginPath)(m); IMPL(EndPath)(m); }
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    CHECK(h != NULL); if (!h) return;
    G2_EQ(emf_count(h, EMR_BEGINPATH), 4);
    G2_EQ(emf_count(h, EMR_ENDPATH), 4);
    U(DeleteEnhMetaFile)(h);
}
TEST(gap2_path_gdi_count_balanced) {
    NEED_PATH();
    DWORD base = g2_gdi_count();
    for (int i = 0; i < 200; i++) {
        HDC dc = U(CreateCompatibleDC)(NULL);
        IMPL(BeginPath)(dc); U(MoveToEx)(dc, 0, 0, NULL); IMPL(LineTo)(dc, i, i); IMPL(EndPath)(dc); IMPL(FillPath)(dc);
        U(DeleteDC)(dc);
    }
    if (base) G2_EQ(g2_gdi_count(), base);
}

/* ====================================================== LineTo ====================================================== */
#define NEED_LT() do { REQUIRE(IMPL(LineTo)); REQUIRE(U(MoveToEx)); REQUIRE(U(CreatePen)); REQUIRE(U(SelectObject)); REQUIRE(U(GetCurrentPositionEx)); } while (0)
static HPEN black_pen(G2DC *d, HGDIOBJ *old, int w, COLORREF c) {
    HPEN p = U(CreatePen)(PS_SOLID, w, c);
    *old = U(SelectObject)(d->dc, p);
    return p;
}
TEST(gap2_lineto_horizontal_exclusive_end) {
    NEED_LT();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HGDIOBJ op; HPEN p = black_pen(&d, &op, 1, 0);
    U(MoveToEx)(d.dc, 2, 3, NULL);
    CHECK(IMPL(LineTo)(d.dc, 8, 3));
    G2_EQ(g2_count(&d, 0), 6);
    for (int x = 0; x < 16; x++) CHECKF(g2_px(&d, x, 3) == ((x >= 2 && x < 8) ? 0 : WHITE_PX), "x=%d", x);
    U(SelectObject)(d.dc, op); U(DeleteObject)(p); g2_free(&d);
}
TEST(gap2_lineto_horizontal_leftwards) {
    NEED_LT();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HGDIOBJ op; HPEN p = black_pen(&d, &op, 1, 0);
    U(MoveToEx)(d.dc, 8, 3, NULL);
    CHECK(IMPL(LineTo)(d.dc, 2, 3));
    G2_EQ(g2_count(&d, 0), 6);
    for (int x = 0; x < 16; x++) CHECKF(g2_px(&d, x, 3) == ((x > 2 && x <= 8) ? 0 : WHITE_PX), "x=%d", x);
    U(SelectObject)(d.dc, op); U(DeleteObject)(p); g2_free(&d);
}
TEST(gap2_lineto_vertical) {
    NEED_LT();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HGDIOBJ op; HPEN p = black_pen(&d, &op, 1, 0);
    U(MoveToEx)(d.dc, 5, 1, NULL);
    CHECK(IMPL(LineTo)(d.dc, 5, 10));
    G2_EQ(g2_count(&d, 0), 9);
    for (int y = 0; y < 16; y++) CHECKF(g2_px(&d, 5, y) == ((y >= 1 && y < 10) ? 0 : WHITE_PX), "y=%d", y);
    U(SelectObject)(d.dc, op); U(DeleteObject)(p); g2_free(&d);
}
TEST(gap2_lineto_diagonal_45) {
    NEED_LT();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HGDIOBJ op; HPEN p = black_pen(&d, &op, 1, 0);
    U(MoveToEx)(d.dc, 1, 1, NULL);
    CHECK(IMPL(LineTo)(d.dc, 9, 9));
    G2_EQ(g2_count(&d, 0), 8);
    for (int i = 1; i < 9; i++) CHECKF(g2_px(&d, i, i) == 0, "diag %d", i);
    CHECK(g2_px(&d, 9, 9) == WHITE_PX);
    U(SelectObject)(d.dc, op); U(DeleteObject)(p); g2_free(&d);
}
TEST(gap2_lineto_anti_diagonal) {
    NEED_LT();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HGDIOBJ op; HPEN p = black_pen(&d, &op, 1, 0);
    U(MoveToEx)(d.dc, 10, 2, NULL);
    CHECK(IMPL(LineTo)(d.dc, 2, 10));
    G2_EQ(g2_count(&d, 0), 8);
    for (int i = 0; i < 8; i++) CHECKF(g2_px(&d, 10 - i, 2 + i) == 0, "anti %d", i);
    CHECK(g2_px(&d, 2, 10) == WHITE_PX);
    U(SelectObject)(d.dc, op); U(DeleteObject)(p); g2_free(&d);
}
TEST(gap2_lineto_zero_length) {
    NEED_LT();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HGDIOBJ op; HPEN p = black_pen(&d, &op, 1, 0);
    U(MoveToEx)(d.dc, 4, 4, NULL);
    CHECK(IMPL(LineTo)(d.dc, 4, 4));
    G2_EQ(g2_count(&d, 0), 0);
    U(SelectObject)(d.dc, op); U(DeleteObject)(p); g2_free(&d);
}
TEST(gap2_lineto_updates_current_position) {
    NEED_LT();
    HDC dc = U(CreateCompatibleDC)(NULL);
    POINT p;
    U(MoveToEx)(dc, 7, 9, NULL);
    CHECK(IMPL(LineTo)(dc, 20, -5));
    CHECK(U(GetCurrentPositionEx)(dc, &p)); CHECK(p.x == 20 && p.y == -5);
    CHECK(IMPL(LineTo)(dc, -3000, 4000));
    CHECK(U(GetCurrentPositionEx)(dc, &p)); CHECK(p.x == -3000 && p.y == 4000);
    U(DeleteDC)(dc);
}
TEST(gap2_lineto_starts_at_origin_by_default) {
    NEED_LT();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HGDIOBJ op; HPEN p = black_pen(&d, &op, 1, 0);
    CHECK(IMPL(LineTo)(d.dc, 5, 0));
    G2_EQ(g2_count(&d, 0), 5);
    for (int x = 0; x < 5; x++) CHECK(g2_px(&d, x, 0) == 0);
    U(SelectObject)(d.dc, op); U(DeleteObject)(p); g2_free(&d);
}
TEST(gap2_lineto_chain_polyline) {
    NEED_LT();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HGDIOBJ op; HPEN p = black_pen(&d, &op, 1, RGB(0, 0, 255));
    U(MoveToEx)(d.dc, 1, 1, NULL);
    IMPL(LineTo)(d.dc, 6, 1); IMPL(LineTo)(d.dc, 6, 6); IMPL(LineTo)(d.dc, 1, 6); IMPL(LineTo)(d.dc, 1, 1);
    G2_EQ(g2_count(&d, G2_PX(0, 0, 255)), 20);
    U(SelectObject)(d.dc, op); U(DeleteObject)(p); g2_free(&d);
}
TEST(gap2_lineto_pen_color_exact) {
    NEED_LT();
    static const COLORREF cs[] = { RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255), RGB(12, 34, 56), RGB(255, 255, 255), 0 };
    for (unsigned i = 0; i < sizeof cs / sizeof cs[0]; i++) {
        G2DC d; if (!white_dib(&d, 8, 8)) { CHECK(0); return; }
        g2_fill(&d, G2_PX(128, 128, 128));
        HGDIOBJ op; HPEN p = black_pen(&d, &op, 1, cs[i]);
        U(MoveToEx)(d.dc, 0, 2, NULL); IMPL(LineTo)(d.dc, 8, 2);
        DWORD want = G2_PX(GetRValue(cs[i]), GetGValue(cs[i]), GetBValue(cs[i]));
        CHECKF(g2_px(&d, 3, 2) == want, "color %u: %06lx want %06lx", i, g2_px(&d, 3, 2), want);
        U(SelectObject)(d.dc, op); U(DeleteObject)(p); g2_free(&d);
    }
}
TEST(gap2_lineto_null_pen_moves_only) {
    NEED_LT(); REQUIRE(U(GetStockObject));
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HGDIOBJ op = U(SelectObject)(d.dc, U(GetStockObject)(NULL_PEN));
    U(MoveToEx)(d.dc, 1, 1, NULL);
    CHECK(IMPL(LineTo)(d.dc, 9, 9));
    G2_EQ(g2_count(&d, WHITE_PX), 256);
    POINT p; U(GetCurrentPositionEx)(d.dc, &p); CHECK(p.x == 9 && p.y == 9);
    U(SelectObject)(d.dc, op); g2_free(&d);
}
TEST(gap2_lineto_dc_pen_color) {
    NEED_LT(); REQUIRE(U(GetStockObject)); REQUIRE(U(SetDCPenColor));
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HGDIOBJ op = U(SelectObject)(d.dc, U(GetStockObject)(DC_PEN));
    U(SetDCPenColor)(d.dc, RGB(10, 200, 30));
    U(MoveToEx)(d.dc, 0, 4, NULL); IMPL(LineTo)(d.dc, 6, 4);
    G2_EQ(g2_count(&d, G2_PX(10, 200, 30)), 6);
    U(SelectObject)(d.dc, op); g2_free(&d);
}
TEST(gap2_lineto_rop2_variants) {
    NEED_LT(); REQUIRE(U(SetROP2));
    static const struct { int rop; DWORD dst, want; } t[] = {
        { R2_BLACK, 0xffffff, 0x000000 }, { R2_WHITE, 0x000000, 0xffffff }, { R2_NOP, 0x123456, 0x123456 },
        { R2_NOT, 0x123456, 0xedcba9 }, { R2_COPYPEN, 0x123456, 0x0000ff /*pen red*/ },
        { R2_NOTCOPYPEN, 0x123456, 0xffff00 }, { R2_XORPEN, 0x123456, 0x123456 ^ 0xff }, { R2_MASKPEN, 0xffffff, 0x0000ff },
        { R2_MERGEPEN, 0x100000, 0x1000ff }, { R2_MERGEPENNOT, 0x000000, 0xffffff },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        G2DC d; if (!g2_mk(&d, 8, 8)) { CHECK(0); return; }
        g2_fill(&d, t[i].dst);
        HGDIOBJ op; HPEN p = black_pen(&d, &op, 1, RGB(255, 0, 0));   /* pen = 0xff0000 in COLORREF => 0x0000ff? no: DIB value is 0xff0000 */
        U(SetROP2)(d.dc, t[i].rop);
        U(MoveToEx)(d.dc, 0, 1, NULL); IMPL(LineTo)(d.dc, 8, 1);
        DWORD pen = 0xff0000, dst = t[i].dst, want;
        switch (t[i].rop) {
        case R2_BLACK: want = 0; break;
        case R2_WHITE: want = 0xffffff; break;
        case R2_NOP: want = dst; break;
        case R2_NOT: want = ~dst & 0xffffff; break;
        case R2_COPYPEN: want = pen; break;
        case R2_NOTCOPYPEN: want = ~pen & 0xffffff; break;
        case R2_XORPEN: want = dst ^ pen; break;
        case R2_MASKPEN: want = dst & pen; break;
        case R2_MERGEPEN: want = dst | pen; break;
        default: want = (pen | ~dst) & 0xffffff; break;   /* R2_MERGEPENNOT */
        }
        CHECKF(g2_px(&d, 3, 1) == want, "rop %d: %06lx want %06lx", t[i].rop, g2_px(&d, 3, 1), want);
        CHECKF(g2_px(&d, 3, 3) == t[i].dst, "rop %d touched another row", t[i].rop);
        U(SelectObject)(d.dc, op); U(DeleteObject)(p); g2_free(&d);
    }
}
TEST(gap2_lineto_clipped_to_clip_rect) {
    NEED_LT(); REQUIRE(U(IntersectClipRect));
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HGDIOBJ op; HPEN p = black_pen(&d, &op, 1, 0);
    U(IntersectClipRect)(d.dc, 4, 0, 9, 16);
    U(MoveToEx)(d.dc, 0, 5, NULL); IMPL(LineTo)(d.dc, 16, 5);
    G2_EQ(g2_count(&d, 0), 5);
    CHECK(g2_px(&d, 3, 5) == WHITE_PX && g2_px(&d, 4, 5) == 0 && g2_px(&d, 8, 5) == 0 && g2_px(&d, 9, 5) == WHITE_PX);
    POINT cp; U(GetCurrentPositionEx)(d.dc, &cp); CHECK(cp.x == 16 && cp.y == 5);
    U(SelectObject)(d.dc, op); U(DeleteObject)(p); g2_free(&d);
}
TEST(gap2_lineto_entirely_outside_bitmap) {
    NEED_LT();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HGDIOBJ op; HPEN p = black_pen(&d, &op, 1, 0);
    U(MoveToEx)(d.dc, -100, -100, NULL);
    CHECK(IMPL(LineTo)(d.dc, -50, -100));
    CHECK(IMPL(LineTo)(d.dc, 1000, 2000));
    CHECK(IMPL(LineTo)(d.dc, 16, 100));
    int n = g2_count(&d, 0);
    t_note("pixels from far diagonal: %d", n);
    U(SelectObject)(d.dc, op); U(DeleteObject)(p); g2_free(&d);
}
TEST(gap2_lineto_huge_coordinates) {
    NEED_LT();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HGDIOBJ op; HPEN p = black_pen(&d, &op, 1, 0);
    U(MoveToEx)(d.dc, 0, 8, NULL);
    CHECK(IMPL(LineTo)(d.dc, 0x7fffff, 8));   /* far beyond the bitmap, still within 28-bit range */
    G2_EQ(g2_count(&d, 0), 16);
    POINT cp; U(GetCurrentPositionEx)(d.dc, &cp); CHECK(cp.x == 0x7fffff && cp.y == 8);
    U(MoveToEx)(d.dc, 0, 9, NULL);
    BOOL r = IMPL(LineTo)(d.dc, INT_MAX, 9);
    t_note("LineTo(INT_MAX) -> %d", r);
    r = IMPL(LineTo)(d.dc, INT_MIN, 9);
    t_note("LineTo(INT_MIN) -> %d", r);
    U(SelectObject)(d.dc, op); U(DeleteObject)(p); g2_free(&d);
}
TEST(gap2_lineto_thick_pen_horizontal) {
    NEED_LT();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HGDIOBJ op; HPEN p = black_pen(&d, &op, 3, 0);
    U(MoveToEx)(d.dc, 2, 8, NULL);
    CHECK(IMPL(LineTo)(d.dc, 12, 8));
    int n = g2_count(&d, 0);
    t_note("3px pen horizontal run: %d pixels", n);
    CHECK(n >= 24 && n <= 40);
    CHECK(g2_px(&d, 6, 7) == 0 && g2_px(&d, 6, 8) == 0 && g2_px(&d, 6, 9) == 0);
    CHECK(g2_px(&d, 6, 6) == WHITE_PX && g2_px(&d, 6, 10) == WHITE_PX);
    U(SelectObject)(d.dc, op); U(DeleteObject)(p); g2_free(&d);
}
TEST(gap2_lineto_anisotropic_scale) {
    NEED_LT(); REQUIRE(U(SetMapMode)); REQUIRE(U(SetWindowExtEx)); REQUIRE(U(SetViewportExtEx));
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HGDIOBJ op; HPEN p = black_pen(&d, &op, 0, 0);   /* width 0: always one device pixel */
    U(SetMapMode)(d.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(d.dc, 1, 1, NULL); U(SetViewportExtEx)(d.dc, 2, 2, NULL);
    U(MoveToEx)(d.dc, 1, 1, NULL);
    CHECK(IMPL(LineTo)(d.dc, 4, 1));
    G2_EQ(g2_count(&d, 0), 6);
    for (int x = 0; x < 16; x++) CHECKF(g2_px(&d, x, 2) == ((x >= 2 && x < 8) ? 0 : WHITE_PX), "x=%d", x);
    POINT cp; U(GetCurrentPositionEx)(d.dc, &cp); CHECK(cp.x == 4 && cp.y == 1);
    U(SelectObject)(d.dc, op); U(DeleteObject)(p); g2_free(&d);
}
TEST(gap2_lineto_viewport_origin_offset) {
    NEED_LT(); REQUIRE(U(SetViewportOrgEx));
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HGDIOBJ op; HPEN p = black_pen(&d, &op, 1, 0);
    U(SetViewportOrgEx)(d.dc, 5, 7, NULL);
    U(MoveToEx)(d.dc, 0, 0, NULL); CHECK(IMPL(LineTo)(d.dc, 4, 0));
    for (int x = 0; x < 16; x++) CHECKF(g2_px(&d, x, 7) == ((x >= 5 && x < 9) ? 0 : WHITE_PX), "x=%d", x);
    U(SelectObject)(d.dc, op); U(DeleteObject)(p); g2_free(&d);
}
TEST(gap2_lineto_world_transform_scale) {
    NEED_LT(); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform));
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HGDIOBJ op; HPEN p = black_pen(&d, &op, 0, 0);
    U(SetGraphicsMode)(d.dc, GM_ADVANCED);
    XFORM xf = { 2.0f, 0, 0, 2.0f, 1.0f, 1.0f };
    CHECK(U(SetWorldTransform)(d.dc, &xf));
    U(MoveToEx)(d.dc, 1, 1, NULL); CHECK(IMPL(LineTo)(d.dc, 4, 1));
    int n = g2_count(&d, 0);
    t_note("world-transformed run %d px", n);
    CHECK(n >= 5 && n <= 7);
    CHECK(g2_px(&d, 4, 3) == 0 && g2_px(&d, 6, 3) == 0);
    U(SelectObject)(d.dc, op); U(DeleteObject)(p); g2_free(&d);
}
TEST(gap2_lineto_between_dcs_independent) {
    NEED_LT();
    G2DC a, b; if (!white_dib(&a, 8, 8) || !white_dib(&b, 8, 8)) { CHECK(0); return; }
    HGDIOBJ opa; HPEN pa = black_pen(&a, &opa, 1, 0);
    U(MoveToEx)(a.dc, 0, 0, NULL); U(MoveToEx)(b.dc, 0, 4, NULL);
    IMPL(LineTo)(a.dc, 8, 0);
    POINT cp; U(GetCurrentPositionEx)(b.dc, &cp); CHECK(cp.x == 0 && cp.y == 4);
    G2_EQ(g2_count(&b, 0), 0);
    U(SelectObject)(a.dc, opa); U(DeleteObject)(pa); g2_free(&a); g2_free(&b);
}
TEST(gap2_lineto_wmf_dc_records) {
    NEED_LT(); REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile)); REQUIRE(U(GetMetaFileBitsEx));
    SKIP_ON_WINE("WMF DC LineTo differs");
    HDC m = U(CreateMetaFileW)(NULL);
    if (!m) { t_skip("no WMF DC"); return; }
    U(MoveToEx)(m, 1, 1, NULL);
    BOOL r = IMPL(LineTo)(m, 10, 10);
    CHECK(r);
    HMETAFILE h = U(CloseMetaFile)(m);
    CHECK(h != NULL); if (!h) return;
    UINT n = U(GetMetaFileBitsEx)(h, 0, NULL);
    BYTE *b = malloc(n); U(GetMetaFileBitsEx)(h, n, b);
    int lt = 0; DWORD off = 18;
    while (off + 6 <= n) { DWORD sz = *(DWORD *)(b + off) * 2; WORD f = *(WORD *)(b + off + 4); if (sz < 6 || off + sz > n) break; if (f == 0x0213) lt++; off += sz; }
    free(b);
    G2_EQ(lt, 1);   /* META_LINETO */
    U(DeleteMetaFile)(h);
}
TEST(gap2_lineto_emf_dc_records) {
    NEED_LT(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits));
    SKIP_ON_WINE("wine records different EMF stream");
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    U(MoveToEx)(m, 1, 1, NULL);
    for (int i = 0; i < 5; i++) CHECK(IMPL(LineTo)(m, 10 + i, 10));
    POINT cp; U(GetCurrentPositionEx)(m, &cp); CHECK(cp.x == 14 && cp.y == 10);
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    CHECK(h != NULL); if (!h) return;
    G2_EQ(emf_count(h, EMR_LINETO), 5);
    U(DeleteEnhMetaFile)(h);
}
TEST(gap2_lineto_repeated_100_calls) {
    NEED_LT();
    G2DC d; if (!white_dib(&d, 128, 4)) { CHECK(0); return; }
    HGDIOBJ op; HPEN p = black_pen(&d, &op, 1, 0);
    U(MoveToEx)(d.dc, 0, 1, NULL);
    for (int i = 1; i <= 100; i++) if (!IMPL(LineTo)(d.dc, i, 1)) { CHECKF(0, "call %d failed", i); break; }
    G2_EQ(g2_count(&d, 0), 100);
    U(SelectObject)(d.dc, op); U(DeleteObject)(p); g2_free(&d);
}
TEST(gap2_lineto_matches_public_export) {
    NEED_LT(); REQUIRE(U(LineTo));
    G2DC a, b; if (!white_dib(&a, 16, 16) || !white_dib(&b, 16, 16)) { CHECK(0); return; }
    HGDIOBJ opa, opb; HPEN pa = black_pen(&a, &opa, 2, RGB(5, 6, 7)), pb = black_pen(&b, &opb, 2, RGB(5, 6, 7));
    U(MoveToEx)(a.dc, 1, 2, NULL); U(MoveToEx)(b.dc, 1, 2, NULL);
    U(LineTo)(a.dc, 13, 9); IMPL(LineTo)(b.dc, 13, 9);
    CHECK(!memcmp(a.bits, b.bits, 16 * 16 * 4));
    U(SelectObject)(a.dc, opa); U(SelectObject)(b.dc, opb); U(DeleteObject)(pa); U(DeleteObject)(pb);
    g2_free(&a); g2_free(&b);
}

/* ====================================================== clipping ====================================================== */
#define NEED_CLIP() do { REQUIRE(IMPL(GetClipBox)); REQUIRE(IMPL(IntersectClipRect)); REQUIRE(IMPL(ExtSelectClipRgn)); \
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(SelectObject)); } while (0)

TEST(gap2_clipbox_initial_dib) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 12)) { CHECK(0); return; }
    RECT rc;
    G2_EQ(BOX(d.dc, rc), SIMPLEREGION);
    CHECK(rc_eq(&rc, 0, 0, 16, 12));
    g2_free(&d);
}
TEST(gap2_clipbox_default_memdc_one_pixel) {
    NEED_CLIP();
    HDC dc = U(CreateCompatibleDC)(NULL);
    RECT rc;
    G2_EQ(BOX(dc, rc), SIMPLEREGION);
    CHECK(rc_eq(&rc, 0, 0, 1, 1));
    U(DeleteDC)(dc);
}
TEST(gap2_clipbox_sizes_table) {
    NEED_CLIP();
    static const int sz[][2] = { {1, 1}, {2, 3}, {7, 5}, {31, 1}, {1, 31}, {64, 64}, {100, 17}, {257, 3} };
    for (unsigned i = 0; i < sizeof sz / sizeof sz[0]; i++) {
        G2DC d; if (!g2_mk(&d, sz[i][0], sz[i][1])) { CHECK(0); continue; }
        RECT rc;
        CHECKF(BOX(d.dc, rc) == SIMPLEREGION, "size %d", i);
        CHECKF(rc_eq(&rc, 0, 0, sz[i][0], sz[i][1]), "size %d box %ld,%ld,%ld,%ld", i, rc.left, rc.top, rc.right, rc.bottom);
        g2_free(&d);
    }
}
TEST(gap2_clipbox_invalid_handles) {
    NEED_CLIP();
    for (int i = 0; i < G2_NJUNK; i++) {
        RECT rc = { 1, 2, 3, 4 };
        int r = IMPL(GetClipBox)((HDC)g2_junk[i], &rc);
        CHECKF(r == 0, "handle %#llx -> %d", (unsigned long long)g2_junk[i], r);
    }
    RECT rc = { 1, 2, 3, 4 };
    CHECK(IMPL(GetClipBox)(g2_stale_dc(), &rc) == 0);
}
TEST(gap2_clipbox_non_dc_objects) {
    NEED_CLIP();
    HGDIOBJ o[2] = { U(CreateSolidBrush)(0), U(CreateRectRgn)(0, 0, 5, 5) };
    for (int i = 0; i < 2; i++) { RECT rc; CHECKF(IMPL(GetClipBox)((HDC)o[i], &rc) == 0, "obj %d", i); U(DeleteObject)(o[i]); }
}
TEST(gap2_clipbox_after_intersect) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    RECT rc;
    G2_EQ(IMPL(IntersectClipRect)(d.dc, 4, 5, 12, 10), SIMPLEREGION);
    G2_EQ(BOX(d.dc, rc), SIMPLEREGION);
    CHECK(rc_eq(&rc, 4, 5, 12, 10));
    G2_EQ(IMPL(IntersectClipRect)(d.dc, 8, 0, 20, 8), SIMPLEREGION);
    G2_EQ(BOX(d.dc, rc), SIMPLEREGION);
    CHECK(rc_eq(&rc, 8, 5, 12, 8));
    g2_free(&d);
}
TEST(gap2_clipbox_null_region_zero_rect) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    RECT rc;
    G2_EQ(IMPL(IntersectClipRect)(d.dc, 2, 2, 6, 6), SIMPLEREGION);
    G2_EQ(IMPL(IntersectClipRect)(d.dc, 8, 8, 12, 12), NULLREGION);
    G2_EQ(BOX(d.dc, rc), NULLREGION);
    CHECK(rc_eq(&rc, 0, 0, 0, 0));
    g2_free(&d);
}
TEST(gap2_clipbox_complex_region_bounds) {
    NEED_CLIP(); REQUIRE(U(ExcludeClipRect));
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    RECT rc;
    CHECK(U(ExcludeClipRect)(d.dc, 4, 4, 8, 8) == COMPLEXREGION);
    G2_EQ(BOX(d.dc, rc), COMPLEXREGION);
    CHECK(rc_eq(&rc, 0, 0, 16, 16));
    g2_free(&d);
}
TEST(gap2_clipbox_clip_outside_surface) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    RECT rc;
    HRGN r = U(CreateRectRgn)(100, 100, 120, 120);
    int rt = IMPL(ExtSelectClipRgn)(d.dc, r, RGN_COPY);
    int bt = BOX(d.dc, rc);
    t_note("clip entirely off-surface: select -> %d, box -> %d (%ld,%ld,%ld,%ld)", rt, bt, rc.left, rc.top, rc.right, rc.bottom);
    CHECK(bt == NULLREGION || bt == SIMPLEREGION);
    U(DeleteObject)(r);
    g2_free(&d);
}
TEST(gap2_clipbox_exceeding_surface_is_clamped) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    RECT rc;
    G2_EQ(IMPL(IntersectClipRect)(d.dc, -100, -100, 100, 100), SIMPLEREGION);
    G2_EQ(BOX(d.dc, rc), SIMPLEREGION);
    CHECK(rc_eq(&rc, 0, 0, 16, 16));
    HRGN r = U(CreateRectRgn)(-50, -50, 8, 8);
    G2_EQ(IMPL(ExtSelectClipRgn)(d.dc, r, RGN_COPY), SIMPLEREGION);
    G2_EQ(BOX(d.dc, rc), SIMPLEREGION);
    CHECK(rc_eq(&rc, 0, 0, 8, 8));
    U(DeleteObject)(r);
    g2_free(&d);
}
TEST(gap2_clipbox_buffer_fully_overwritten) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    IMPL(IntersectClipRect)(d.dc, 3, 3, 3, 9);   /* empty */
    RECT rc = { 77, 78, 79, 80 };
    G2_EQ(IMPL(GetClipBox)(d.dc, &rc), NULLREGION);
    CHECK(rc_eq(&rc, 0, 0, 0, 0));
    g2_free(&d);
}
TEST(gap2_clipbox_savedc_restore) {
    NEED_CLIP(); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    RECT rc;
    int s = U(SaveDC)(d.dc);
    IMPL(IntersectClipRect)(d.dc, 5, 5, 6, 6);
    G2_EQ(BOX(d.dc, rc), SIMPLEREGION);
    CHECK(rc_eq(&rc, 5, 5, 6, 6));
    CHECK(U(RestoreDC)(d.dc, s));
    G2_EQ(BOX(d.dc, rc), SIMPLEREGION);
    CHECK(rc_eq(&rc, 0, 0, 16, 16));
    g2_free(&d);
}
TEST(gap2_clipbox_logical_units_follow_mapping) {
    NEED_CLIP(); REQUIRE(U(SetMapMode)); REQUIRE(U(SetWindowExtEx)); REQUIRE(U(SetViewportExtEx));
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    U(SetMapMode)(d.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(d.dc, 1, 1, NULL); U(SetViewportExtEx)(d.dc, 2, 2, NULL);
    RECT rc;
    G2_EQ(BOX(d.dc, rc), SIMPLEREGION);
    CHECK(rc_eq(&rc, 0, 0, 8, 8));
    G2_EQ(IMPL(IntersectClipRect)(d.dc, 1, 1, 3, 3), SIMPLEREGION);
    G2_EQ(BOX(d.dc, rc), SIMPLEREGION);
    CHECK(rc_eq(&rc, 1, 1, 3, 3));
    U(PatBlt)(d.dc, -100, -100, 500, 500, BLACKNESS);
    G2_EQ(g2_count(&d, 0), 16);   /* device rect (2,2)-(6,6) */
    CHECK(g2_px(&d, 2, 2) == 0 && g2_px(&d, 5, 5) == 0 && g2_px(&d, 6, 6) == WHITE_PX && g2_px(&d, 1, 1) == WHITE_PX);
    g2_free(&d);
}
TEST(gap2_clipbox_viewport_origin) {
    NEED_CLIP(); REQUIRE(U(SetViewportOrgEx));
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    U(SetViewportOrgEx)(d.dc, 4, 6, NULL);
    RECT rc;
    G2_EQ(BOX(d.dc, rc), SIMPLEREGION);
    CHECK(rc_eq(&rc, -4, -6, 12, 10));
    g2_free(&d);
}
TEST(gap2_clipbox_emf_dc) {
    NEED_CLIP(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    RECT rc = { 9, 9, 9, 9 };
    int r = IMPL(GetClipBox)(m, &rc);
    t_note("EMF DC clip box -> %d (%ld,%ld,%ld,%ld)", r, rc.left, rc.top, rc.right, rc.bottom);
    HENHMETAFILE h = U(CloseEnhMetaFile)(m); if (h) U(DeleteEnhMetaFile)(h);
}
TEST(gap2_clipbox_matches_public_export) {
    NEED_CLIP(); REQUIRE(U(GetClipBox));
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    IMPL(IntersectClipRect)(d.dc, 3, 4, 9, 11);
    RECT a, b; int ra = U(GetClipBox)(d.dc, &a), rb = IMPL(GetClipBox)(d.dc, &b);
    CHECK(ra == rb); CHECK(!memcmp(&a, &b, sizeof a));
    g2_free(&d);
}

/* ---------- IntersectClipRect ---------- */
TEST(gap2_intersect_invalid_handles) {
    NEED_CLIP();
    for (int i = 0; i < G2_NJUNK; i++) {
        SetLastError(0);
        int r = IMPL(IntersectClipRect)((HDC)g2_junk[i], 0, 0, 5, 5);
        CHECKF(r == 0, "handle %#llx -> %d", (unsigned long long)g2_junk[i], r);
        EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    }
    CHECK(IMPL(IntersectClipRect)(g2_stale_dc(), 0, 0, 5, 5) == 0);
}
TEST(gap2_intersect_non_dc_objects) {
    NEED_CLIP();
    HGDIOBJ o[2] = { U(CreateSolidBrush)(0), U(CreateRectRgn)(0, 0, 5, 5) };
    for (int i = 0; i < 2; i++) { CHECKF(IMPL(IntersectClipRect)((HDC)o[i], 0, 0, 2, 2) == 0, "obj %d", i); U(DeleteObject)(o[i]); }
}
TEST(gap2_intersect_exact_pixels) {
    NEED_CLIP();
    static const int r[][4] = { {0, 0, 16, 16}, {1, 1, 2, 2}, {3, 4, 9, 5}, {15, 15, 16, 16}, {0, 0, 1, 16}, {5, 0, 6, 16}, {2, 3, 14, 13} };
    for (unsigned i = 0; i < sizeof r / sizeof r[0]; i++) {
        G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
        CHECKF(IMPL(IntersectClipRect)(d.dc, r[i][0], r[i][1], r[i][2], r[i][3]) == SIMPLEREGION, "rect %u", i);
        U(PatBlt)(d.dc, 0, 0, 16, 16, BLACKNESS);
        int want = (r[i][2] - r[i][0]) * (r[i][3] - r[i][1]);
        CHECKF(g2_count(&d, 0) == want, "rect %u: %d px want %d", i, g2_count(&d, 0), want);
        g2_free(&d);
    }
}
TEST(gap2_intersect_empty_rects) {
    NEED_CLIP();
    SKIP_ON_WINE("wine reports SIMPLEREGION for empty rects");
    static const int r[][4] = { {4, 4, 4, 8}, {4, 4, 8, 4}, {4, 4, 4, 4}, {0, 0, 0, 0} };
    for (unsigned i = 0; i < sizeof r / sizeof r[0]; i++) {
        G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
        CHECKF(IMPL(IntersectClipRect)(d.dc, r[i][0], r[i][1], r[i][2], r[i][3]) == NULLREGION, "rect %u", i);
        U(PatBlt)(d.dc, 0, 0, 16, 16, BLACKNESS);
        CHECKF(g2_count(&d, 0) == 0, "rect %u drew pixels", i);
        g2_free(&d);
    }
}
TEST(gap2_intersect_inverted_rect) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    int r = IMPL(IntersectClipRect)(d.dc, 10, 10, 4, 4);
    RECT rc; int b = BOX(d.dc, rc);
    t_note("inverted rect: ret %d, box %d (%ld,%ld,%ld,%ld)", r, b, rc.left, rc.top, rc.right, rc.bottom);
    CHECK(r == NULLREGION || r == SIMPLEREGION);
    g2_free(&d);
}
TEST(gap2_intersect_cumulative_narrowing) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    for (int i = 0; i < 6; i++) G2_EQ(IMPL(IntersectClipRect)(d.dc, i, i, 16 - i, 16 - i), SIMPLEREGION);
    RECT rc; BOX(d.dc, rc);
    CHECK(rc_eq(&rc, 5, 5, 11, 11));
    g2_free(&d);
}
TEST(gap2_intersect_never_grows) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    IMPL(IntersectClipRect)(d.dc, 4, 4, 8, 8);
    IMPL(IntersectClipRect)(d.dc, 0, 0, 16, 16);
    RECT rc; BOX(d.dc, rc);
    CHECK(rc_eq(&rc, 4, 4, 8, 8));
    g2_free(&d);
}
TEST(gap2_intersect_after_exclude_is_complex) {
    NEED_CLIP(); REQUIRE(U(ExcludeClipRect));
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    U(ExcludeClipRect)(d.dc, 6, 6, 10, 10);
    G2_EQ(IMPL(IntersectClipRect)(d.dc, 0, 0, 16, 16), COMPLEXREGION);
    G2_EQ(IMPL(IntersectClipRect)(d.dc, 6, 6, 10, 10), NULLREGION);
    g2_free(&d);
}
TEST(gap2_intersect_extreme_values) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    int r = IMPL(IntersectClipRect)(d.dc, INT_MIN, INT_MIN, INT_MAX, INT_MAX);
    RECT rc; int b = BOX(d.dc, rc);
    t_note("extreme rect: ret %d, box %d (%ld,%ld,%ld,%ld)", r, b, rc.left, rc.top, rc.right, rc.bottom);
    r = IMPL(IntersectClipRect)(d.dc, -1000000, -1000000, 1000000, 1000000);
    t_note("1e6 rect: ret %d", r);
    g2_free(&d);
}
TEST(gap2_intersect_wmf_dc) {
    NEED_CLIP(); REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    SKIP_ON_WINE("WMF clip differs");
    HDC m = U(CreateMetaFileW)(NULL);
    if (!m) { t_skip("no WMF DC"); return; }
    int r = IMPL(IntersectClipRect)(m, 0, 0, 10, 10);
    t_note("IntersectClipRect(WMF DC) -> %d", r);
    CHECK(r != 0);
    HMETAFILE h = U(CloseMetaFile)(m); if (h) U(DeleteMetaFile)(h);
}
TEST(gap2_intersect_emf_dc_record) {
    NEED_CLIP(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits));
    SKIP_ON_WINE("wine records different EMF stream");
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    int r = IMPL(IntersectClipRect)(m, 5, 5, 50, 50);
    t_note("IntersectClipRect(EMF DC) -> %d", r);
    CHECK(r != 0);
    IMPL(IntersectClipRect)(m, 10, 10, 20, 20);
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    CHECK(h != NULL); if (!h) return;
    G2_EQ(emf_count(h, EMR_INTERSECTCLIPRECT), 2);
    U(DeleteEnhMetaFile)(h);
}
TEST(gap2_intersect_matches_public_export) {
    NEED_CLIP(); REQUIRE(U(IntersectClipRect));
    G2DC a, b; if (!white_dib(&a, 16, 16) || !white_dib(&b, 16, 16)) { CHECK(0); return; }
    static const int r[][4] = { {1, 2, 9, 9}, {5, 5, 5, 9}, {-5, -5, 30, 30}, {8, 8, 12, 12} };
    for (unsigned i = 0; i < 4; i++) {
        int x = U(IntersectClipRect)(a.dc, r[i][0], r[i][1], r[i][2], r[i][3]);
        int y = IMPL(IntersectClipRect)(b.dc, r[i][0], r[i][1], r[i][2], r[i][3]);
        CHECKF(x == y, "step %u: %d vs %d", i, x, y);
    }
    g2_free(&a); g2_free(&b);
}
TEST(gap2_intersect_mono_bitmap_dc) {
    NEED_CLIP();
    HDC dc = U(CreateCompatibleDC)(NULL);
    HBITMAP bm = U(CreateBitmap)(20, 10, 1, 1, NULL);
    HGDIOBJ old = U(SelectObject)(dc, bm);
    RECT rc;
    G2_EQ(BOX(dc, rc), SIMPLEREGION);
    CHECK(rc_eq(&rc, 0, 0, 20, 10));
    G2_EQ(IMPL(IntersectClipRect)(dc, 5, 2, 30, 30), SIMPLEREGION);
    G2_EQ(BOX(dc, rc), SIMPLEREGION);
    CHECK(rc_eq(&rc, 5, 2, 20, 10));
    U(SelectObject)(dc, old); U(DeleteObject)(bm); U(DeleteDC)(dc);
}
TEST(gap2_intersect_per_bit_depth) {
    NEED_CLIP(); REQUIRE(U(CreateDIBSection));
    static const int bpps[] = { 1, 4, 8, 16, 24, 32 };
    for (unsigned i = 0; i < sizeof bpps / sizeof bpps[0]; i++) {
        HDC dc = U(CreateCompatibleDC)(NULL);
        struct { BITMAPINFOHEADER h; RGBQUAD pal[256]; } bi; memset(&bi, 0, sizeof bi);
        bi.h.biSize = sizeof bi.h; bi.h.biWidth = 20; bi.h.biHeight = -10; bi.h.biPlanes = 1; bi.h.biBitCount = bpps[i];
        void *bits;
        HBITMAP bm = U(CreateDIBSection)(dc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
        if (!bm) { U(DeleteDC)(dc); CHECKF(0, "no %d bpp DIB", bpps[i]); continue; }
        HGDIOBJ old = U(SelectObject)(dc, bm);
        RECT rc;
        CHECKF(IMPL(IntersectClipRect)(dc, 2, 3, 15, 8) == SIMPLEREGION, "%d bpp", bpps[i]);
        BOX(dc, rc);
        CHECKF(rc_eq(&rc, 2, 3, 15, 8), "%d bpp box", bpps[i]);
        U(SelectObject)(dc, old); U(DeleteObject)(bm); U(DeleteDC)(dc);
    }
}

/* ---------- ExtSelectClipRgn ---------- */
TEST(gap2_extsel_invalid_dc_handles) {
    NEED_CLIP();
    HRGN r = U(CreateRectRgn)(0, 0, 4, 4);
    for (int i = 0; i < G2_NJUNK; i++) {
        SetLastError(0);
        int x = IMPL(ExtSelectClipRgn)((HDC)g2_junk[i], r, RGN_COPY);
        CHECKF(x == 0, "handle %#llx -> %d", (unsigned long long)g2_junk[i], x);
        EXPECT_ERR(x, ERROR_INVALID_HANDLE);
    }
    CHECK(IMPL(ExtSelectClipRgn)(g2_stale_dc(), r, RGN_COPY) == 0);
    CHECK(IMPL(ExtSelectClipRgn)(NULL, NULL, RGN_COPY) == 0);
    CHECK(U(GetObjectType)(r) == OBJ_REGION);
    U(DeleteObject)(r);
}
TEST(gap2_extsel_bad_region_handle) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HGDIOBJ br = U(CreateSolidBrush)(0);
    SetLastError(0);
    int r = IMPL(ExtSelectClipRgn)(d.dc, (HRGN)br, RGN_COPY);
    CHECK(r == 0);
    t_note("brush as region: err %lu", GetLastError());
    for (int i = 1; i < G2_NJUNK; i++) {
        r = IMPL(ExtSelectClipRgn)(d.dc, (HRGN)g2_junk[i], RGN_COPY);
        CHECKF(r == 0, "bogus region %#llx -> %d", (unsigned long long)g2_junk[i], r);
    }
    RECT rc; G2_EQ(BOX(d.dc, rc), SIMPLEREGION); CHECK(rc_eq(&rc, 0, 0, 16, 16));
    U(DeleteObject)(br); g2_free(&d);
}
TEST(gap2_extsel_deleted_region_handle) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HRGN r = U(CreateRectRgn)(0, 0, 4, 4); U(DeleteObject)(r);
    CHECK(IMPL(ExtSelectClipRgn)(d.dc, r, RGN_COPY) == 0);
    RECT rc; G2_EQ(BOX(d.dc, rc), SIMPLEREGION); CHECK(rc_eq(&rc, 0, 0, 16, 16));
    g2_free(&d);
}
TEST(gap2_extsel_copy_rect_region) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HRGN r = U(CreateRectRgn)(2, 3, 10, 12);
    G2_EQ(IMPL(ExtSelectClipRgn)(d.dc, r, RGN_COPY), SIMPLEREGION);
    RECT rc; G2_EQ(BOX(d.dc, rc), SIMPLEREGION); CHECK(rc_eq(&rc, 2, 3, 10, 12));
    U(PatBlt)(d.dc, 0, 0, 16, 16, BLACKNESS);
    G2_EQ(g2_count(&d, 0), 8 * 9);
    U(DeleteObject)(r); g2_free(&d);
}
TEST(gap2_extsel_region_is_copied) {
    NEED_CLIP(); REQUIRE(U(SetRectRgn));
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HRGN r = U(CreateRectRgn)(2, 3, 10, 12);
    IMPL(ExtSelectClipRgn)(d.dc, r, RGN_COPY);
    U(SetRectRgn)(r, 0, 0, 1, 1);
    RECT rc; BOX(d.dc, rc); CHECK(rc_eq(&rc, 2, 3, 10, 12));
    CHECK(U(DeleteObject)(r));
    BOX(d.dc, rc); CHECK(rc_eq(&rc, 2, 3, 10, 12));
    g2_free(&d);
}
TEST(gap2_extsel_copy_null_removes_clip) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    IMPL(IntersectClipRect)(d.dc, 4, 4, 8, 8);
    int r = IMPL(ExtSelectClipRgn)(d.dc, NULL, RGN_COPY);
    t_note("RGN_COPY with NULL region -> %d", r);
    CHECK(r == SIMPLEREGION || r == NULLREGION);
    RECT rc; G2_EQ(BOX(d.dc, rc), SIMPLEREGION); CHECK(rc_eq(&rc, 0, 0, 16, 16));
    g2_free(&d);
}
TEST(gap2_extsel_empty_region_clips_everything) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HRGN r = U(CreateRectRgn)(5, 5, 5, 5);
    G2_EQ(IMPL(ExtSelectClipRgn)(d.dc, r, RGN_COPY), NULLREGION);
    RECT rc; G2_EQ(BOX(d.dc, rc), NULLREGION);
    U(PatBlt)(d.dc, 0, 0, 16, 16, BLACKNESS);
    G2_EQ(g2_count(&d, 0), 0);
    U(DeleteObject)(r); g2_free(&d);
}
TEST(gap2_extsel_and) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HRGN a = U(CreateRectRgn)(2, 3, 10, 12), b = U(CreateRectRgn)(6, 6, 14, 14);
    IMPL(ExtSelectClipRgn)(d.dc, a, RGN_COPY);
    G2_EQ(IMPL(ExtSelectClipRgn)(d.dc, b, RGN_AND), SIMPLEREGION);
    RECT rc; BOX(d.dc, rc); CHECK(rc_eq(&rc, 6, 6, 10, 12));
    U(PatBlt)(d.dc, 0, 0, 16, 16, BLACKNESS);
    G2_EQ(g2_count(&d, 0), 4 * 6);
    U(DeleteObject)(a); U(DeleteObject)(b); g2_free(&d);
}
TEST(gap2_extsel_and_disjoint) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HRGN a = U(CreateRectRgn)(0, 0, 4, 4), b = U(CreateRectRgn)(8, 8, 12, 12);
    IMPL(ExtSelectClipRgn)(d.dc, a, RGN_COPY);
    G2_EQ(IMPL(ExtSelectClipRgn)(d.dc, b, RGN_AND), NULLREGION);
    U(DeleteObject)(a); U(DeleteObject)(b); g2_free(&d);
}
TEST(gap2_extsel_or) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HRGN a = U(CreateRectRgn)(0, 0, 4, 4), b = U(CreateRectRgn)(8, 8, 12, 12);
    IMPL(ExtSelectClipRgn)(d.dc, a, RGN_COPY);
    G2_EQ(IMPL(ExtSelectClipRgn)(d.dc, b, RGN_OR), COMPLEXREGION);
    RECT rc; BOX(d.dc, rc); CHECK(rc_eq(&rc, 0, 0, 12, 12));
    U(PatBlt)(d.dc, 0, 0, 16, 16, BLACKNESS);
    G2_EQ(g2_count(&d, 0), 16 + 16);
    U(DeleteObject)(a); U(DeleteObject)(b); g2_free(&d);
}
TEST(gap2_extsel_or_adjacent_merges_to_simple) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HRGN a = U(CreateRectRgn)(0, 0, 4, 4), b = U(CreateRectRgn)(4, 0, 8, 4);
    IMPL(ExtSelectClipRgn)(d.dc, a, RGN_COPY);
    G2_EQ(IMPL(ExtSelectClipRgn)(d.dc, b, RGN_OR), SIMPLEREGION);
    RECT rc; BOX(d.dc, rc); CHECK(rc_eq(&rc, 0, 0, 8, 4));
    U(DeleteObject)(a); U(DeleteObject)(b); g2_free(&d);
}
TEST(gap2_extsel_xor) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HRGN a = U(CreateRectRgn)(0, 0, 8, 8), b = U(CreateRectRgn)(4, 4, 12, 12);
    IMPL(ExtSelectClipRgn)(d.dc, a, RGN_COPY);
    G2_EQ(IMPL(ExtSelectClipRgn)(d.dc, b, RGN_XOR), COMPLEXREGION);
    U(PatBlt)(d.dc, 0, 0, 16, 16, BLACKNESS);
    if (!g2_wine()) {   /* wine's RGN_XOR on a clip region differs */
        G2_EQ(g2_count(&d, 0), 64 + 64 - 2 * 16);
        CHECK(g2_px(&d, 5, 5) == WHITE_PX && g2_px(&d, 1, 1) == 0 && g2_px(&d, 10, 10) == 0);
    }
    U(DeleteObject)(a); U(DeleteObject)(b); g2_free(&d);
}
TEST(gap2_extsel_diff) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HRGN a = U(CreateRectRgn)(0, 0, 8, 8), b = U(CreateRectRgn)(4, 4, 12, 12);
    IMPL(ExtSelectClipRgn)(d.dc, a, RGN_COPY);
    G2_EQ(IMPL(ExtSelectClipRgn)(d.dc, b, RGN_DIFF), COMPLEXREGION);
    U(PatBlt)(d.dc, 0, 0, 16, 16, BLACKNESS);
    G2_EQ(g2_count(&d, 0), 64 - 16);
    CHECK(g2_px(&d, 5, 5) == WHITE_PX && g2_px(&d, 1, 1) == 0);
    U(DeleteObject)(a); U(DeleteObject)(b); g2_free(&d);
}
TEST(gap2_extsel_diff_everything_gives_null) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HRGN a = U(CreateRectRgn)(2, 2, 6, 6), b = U(CreateRectRgn)(0, 0, 16, 16);
    IMPL(ExtSelectClipRgn)(d.dc, a, RGN_COPY);
    G2_EQ(IMPL(ExtSelectClipRgn)(d.dc, b, RGN_DIFF), NULLREGION);
    U(DeleteObject)(a); U(DeleteObject)(b); g2_free(&d);
}
TEST(gap2_extsel_invalid_modes) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HRGN r = U(CreateRectRgn)(2, 2, 6, 6);
    static const int modes[] = { 0, 6, 7, 8, 100, -1, INT_MIN, INT_MAX, 0x10001 };
    for (unsigned i = 0; i < sizeof modes / sizeof modes[0]; i++) {
        SetLastError(0);
        int x = IMPL(ExtSelectClipRgn)(d.dc, r, modes[i]);
        CHECKF(x == 0, "mode %d -> %d", modes[i], x);
    }
    RECT rc; G2_EQ(BOX(d.dc, rc), SIMPLEREGION); CHECK(rc_eq(&rc, 0, 0, 16, 16));
    U(DeleteObject)(r); g2_free(&d);
}
TEST(gap2_extsel_modes_leave_clip_unchanged_on_error) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HRGN r = U(CreateRectRgn)(2, 2, 6, 6), a = U(CreateRectRgn)(8, 8, 12, 12);
    IMPL(ExtSelectClipRgn)(d.dc, a, RGN_COPY);
    CHECK(IMPL(ExtSelectClipRgn)(d.dc, r, 99) == 0);
    RECT rc; BOX(d.dc, rc); CHECK(rc_eq(&rc, 8, 8, 12, 12));
    U(DeleteObject)(r); U(DeleteObject)(a); g2_free(&d);
}
TEST(gap2_extsel_and_without_prior_clip) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HRGN r = U(CreateRectRgn)(2, 2, 6, 6);
    G2_EQ(IMPL(ExtSelectClipRgn)(d.dc, r, RGN_AND), SIMPLEREGION);
    RECT rc; BOX(d.dc, rc); CHECK(rc_eq(&rc, 2, 2, 6, 6));
    U(DeleteObject)(r); g2_free(&d);
}
TEST(gap2_extsel_or_xor_diff_without_prior_clip_observed) {
    NEED_CLIP();
    static const int modes[] = { RGN_OR, RGN_XOR, RGN_DIFF };
    for (int i = 0; i < 3; i++) {
        G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
        HRGN r = U(CreateRectRgn)(2, 2, 6, 6);
        int x = IMPL(ExtSelectClipRgn)(d.dc, r, modes[i]);
        RECT rc; int b = BOX(d.dc, rc);
        t_note("mode %d without prior clip: ret %d, box %d (%ld,%ld,%ld,%ld)", modes[i], x, b, rc.left, rc.top, rc.right, rc.bottom);
        U(DeleteObject)(r); g2_free(&d);
    }
}
TEST(gap2_extsel_ellipse_region_complex) {
    NEED_CLIP(); REQUIRE(U(CreateEllipticRgn));
    G2DC d; if (!white_dib(&d, 32, 32)) { CHECK(0); return; }
    HRGN r = U(CreateEllipticRgn)(2, 2, 30, 30);
    G2_EQ(IMPL(ExtSelectClipRgn)(d.dc, r, RGN_COPY), COMPLEXREGION);
    RECT rc; G2_EQ(BOX(d.dc, rc), COMPLEXREGION);
    CHECKF(rc.left >= 2 && rc.left <= 3 && rc.top >= 2 && rc.top <= 3 && rc.right >= 29 && rc.right <= 30 && rc.bottom >= 29 && rc.bottom <= 30,
           "ellipse box %ld,%ld,%ld,%ld", rc.left, rc.top, rc.right, rc.bottom);
    U(PatBlt)(d.dc, 0, 0, 32, 32, BLACKNESS);
    int n = g2_count(&d, 0);
    CHECK(n > 500 && n < 784);
    CHECK(g2_px(&d, 16, 16) == 0 && g2_px(&d, 2, 2) == WHITE_PX && g2_px(&d, 29, 29) == WHITE_PX);
    U(DeleteObject)(r); g2_free(&d);
}
TEST(gap2_extsel_same_region_multiple_dcs) {
    NEED_CLIP();
    G2DC a, b; if (!white_dib(&a, 16, 16) || !white_dib(&b, 16, 16)) { CHECK(0); return; }
    HRGN r = U(CreateRectRgn)(1, 1, 5, 5);
    G2_EQ(IMPL(ExtSelectClipRgn)(a.dc, r, RGN_COPY), SIMPLEREGION);
    G2_EQ(IMPL(ExtSelectClipRgn)(b.dc, r, RGN_COPY), SIMPLEREGION);
    U(PatBlt)(a.dc, 0, 0, 16, 16, BLACKNESS);
    G2_EQ(g2_count(&a, 0), 16); G2_EQ(g2_count(&b, 0), 0);
    CHECK(U(DeleteObject)(r));
    g2_free(&a); g2_free(&b);
}
TEST(gap2_extsel_savedc_restore) {
    NEED_CLIP(); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    HRGN a = U(CreateRectRgn)(1, 1, 5, 5), b = U(CreateRectRgn)(8, 8, 12, 12);
    IMPL(ExtSelectClipRgn)(d.dc, a, RGN_COPY);
    int s = U(SaveDC)(d.dc);
    IMPL(ExtSelectClipRgn)(d.dc, b, RGN_COPY);
    RECT rc; BOX(d.dc, rc); CHECK(rc_eq(&rc, 8, 8, 12, 12));
    U(RestoreDC)(d.dc, s);
    BOX(d.dc, rc); CHECK(rc_eq(&rc, 1, 1, 5, 5));
    U(DeleteObject)(a); U(DeleteObject)(b); g2_free(&d);
}
TEST(gap2_extsel_region_in_device_units) {
    NEED_CLIP(); REQUIRE(U(SetMapMode)); REQUIRE(U(SetWindowExtEx)); REQUIRE(U(SetViewportExtEx));
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    U(SetMapMode)(d.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(d.dc, 1, 1, NULL); U(SetViewportExtEx)(d.dc, 2, 2, NULL);
    HRGN r = U(CreateRectRgn)(4, 4, 12, 12);
    IMPL(ExtSelectClipRgn)(d.dc, r, RGN_COPY);
    U(PatBlt)(d.dc, -100, -100, 500, 500, BLACKNESS);
    G2_EQ(g2_count(&d, 0), 64);   /* region coordinates are device units */
    CHECK(g2_px(&d, 4, 4) == 0 && g2_px(&d, 11, 11) == 0 && g2_px(&d, 12, 12) == WHITE_PX);
    RECT rc; BOX(d.dc, rc); CHECK(rc_eq(&rc, 2, 2, 6, 6));   /* but the box is reported in logical units */
    U(DeleteObject)(r); g2_free(&d);
}
TEST(gap2_extsel_emf_dc_record) {
    NEED_CLIP(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits));
    SKIP_ON_WINE("wine records different EMF stream");
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    HRGN r = U(CreateRectRgn)(1, 1, 9, 9);
    int x = IMPL(ExtSelectClipRgn)(m, r, RGN_COPY);
    t_note("ExtSelectClipRgn(EMF DC) -> %d", x);
    IMPL(ExtSelectClipRgn)(m, NULL, RGN_COPY);
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    CHECK(h != NULL);
    if (h) { G2_EQ(emf_count(h, EMR_EXTSELECTCLIPRGN), 2); U(DeleteEnhMetaFile)(h); }
    U(DeleteObject)(r);
}
TEST(gap2_extsel_wmf_dc) {
    NEED_CLIP(); REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    SKIP_ON_WINE("WMF clip differs");
    HDC m = U(CreateMetaFileW)(NULL);
    if (!m) { t_skip("no WMF DC"); return; }
    HRGN r = U(CreateRectRgn)(1, 1, 9, 9);
    int x = IMPL(ExtSelectClipRgn)(m, r, RGN_COPY);
    t_note("ExtSelectClipRgn(WMF DC) -> %d", x);
    HMETAFILE h = U(CloseMetaFile)(m); if (h) U(DeleteMetaFile)(h);
    U(DeleteObject)(r);
}
TEST(gap2_extsel_matches_public_export) {
    NEED_CLIP(); REQUIRE(U(ExtSelectClipRgn));
    G2DC a, b; if (!white_dib(&a, 16, 16) || !white_dib(&b, 16, 16)) { CHECK(0); return; }
    HRGN r1 = U(CreateRectRgn)(2, 2, 10, 10), r2 = U(CreateRectRgn)(6, 6, 14, 14);
    static const int modes[] = { RGN_COPY, RGN_AND, RGN_OR, RGN_XOR, RGN_DIFF, RGN_COPY, 9 };
    for (unsigned i = 0; i < sizeof modes / sizeof modes[0]; i++) {
        HRGN r = (i & 1) ? r2 : r1;
        int x = U(ExtSelectClipRgn)(a.dc, r, modes[i]), y = IMPL(ExtSelectClipRgn)(b.dc, r, modes[i]);
        CHECKF(x == y, "step %u mode %d: %d vs %d", i, modes[i], x, y);
    }
    RECT ra, rb; U(GetClipBox)(a.dc, &ra); IMPL(GetClipBox)(b.dc, &rb); CHECK(!memcmp(&ra, &rb, sizeof ra));
    U(DeleteObject)(r1); U(DeleteObject)(r2); g2_free(&a); g2_free(&b);
}
TEST(gap2_extsel_many_cycles_gdi_balanced) {
    NEED_CLIP();
    G2DC d; if (!white_dib(&d, 16, 16)) { CHECK(0); return; }
    DWORD base = g2_gdi_count();
    for (int i = 0; i < 500; i++) {
        HRGN r = U(CreateRectRgn)(0, 0, 1 + i % 15, 1 + i % 13);
        IMPL(ExtSelectClipRgn)(d.dc, r, RGN_COPY);
        U(DeleteObject)(r);
    }
    IMPL(ExtSelectClipRgn)(d.dc, NULL, RGN_COPY);
    if (base) G2_EQ(g2_gdi_count(), base);
    g2_free(&d);
}
