/* Rectangle, PolyDraw, PolyBezierTo, PolylineTo, StrokePath, SelectClipPath (+ Impl exports) */
#include "g3.h"

#define IMPLS(var, pub) for (int i_ = 0; i_ < 2; i_++) { var = (__typeof__(&pub))g3_fn(#pub, i_); \
    if (!var) { t_note(#pub " impl %d missing", i_); continue; }

#define WHITE RGB(255,255,255)
#define BLACK RGB(0,0,0)
#define RED   RGB(255,0,0)
#define BLUE  RGB(0,0,255)

/* white 1px pen selected into m.dc */
typedef struct { HPEN pen; HGDIOBJ old; } PENSEL;
static void pen_on(G3DC *m, PENSEL *p, int style, int w, COLORREF c) {
    p->pen = U(CreatePen)(style, w, c);
    p->old = U(SelectObject)(m->dc, p->pen);
}
static void pen_off(G3DC *m, PENSEL *p) { U(SelectObject)(m->dc, p->old); U(DeleteObject)(p->pen); }

/* ---------- Rectangle ---------- */
TEST(gap3_rect_pen_and_brush) {
    __typeof__(&Rectangle) R;
    IMPLS(R, Rectangle)
        G3DC m; if (!g3dc_make(&m, 24, 24)) { CHECK(0); return; }
        PENSEL ps; pen_on(&m, &ps, PS_SOLID, 1, RED);
        HBRUSH br = U(CreateSolidBrush)(BLUE);
        HGDIOBJ ob = U(SelectObject)(m.dc, br);
        g3dc_clear(&m, BLACK);
        CHECK(R(m.dc, 4, 4, 14, 12));
        /* outline occupies [l,r) x [t,b); interior is brush */
        PXC(m, 4, 4, RED); PXC(m, 13, 4, RED); PXC(m, 4, 11, RED); PXC(m, 13, 11, RED);
        PXC(m, 8, 4, RED); PXC(m, 8, 11, RED); PXC(m, 4, 8, RED); PXC(m, 13, 8, RED);
        PXC(m, 5, 5, BLUE); PXC(m, 12, 10, BLUE); PXC(m, 8, 8, BLUE);
        PXC(m, 14, 8, BLACK); PXC(m, 8, 12, BLACK); PXC(m, 3, 8, BLACK); PXC(m, 8, 3, BLACK);
        PXC(m, 14, 12, BLACK);
        EQ(g3dc_count(&m, RED), 2 * 10 + 2 * 6);
        EQ(g3dc_count(&m, BLUE), 8 * 6);
        U(SelectObject)(m.dc, ob); U(DeleteObject)(br);
        pen_off(&m, &ps);
        g3dc_free(&m);
    }
}
TEST(gap3_rect_null_brush) {
    G3DC m; if (!g3dc_make(&m, 20, 20)) { CHECK(0); return; }
    PENSEL ps; pen_on(&m, &ps, PS_SOLID, 1, RED);
    HGDIOBJ ob = U(SelectObject)(m.dc, U(GetStockObject)(NULL_BRUSH));
    g3dc_clear(&m, BLACK);
    CHECK(U(Rectangle)(m.dc, 2, 2, 10, 10));
    EQ(g3dc_count(&m, RED), 4 * 8 - 4);
    PXC(m, 5, 5, BLACK);
    U(SelectObject)(m.dc, ob);
    pen_off(&m, &ps);
    g3dc_free(&m);
}
TEST(gap3_rect_null_pen) {
    G3DC m; if (!g3dc_make(&m, 20, 20)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(BLUE);
    HGDIOBJ ob = U(SelectObject)(m.dc, br);
    HGDIOBJ op = U(SelectObject)(m.dc, U(GetStockObject)(NULL_PEN));
    g3dc_clear(&m, BLACK);
    CHECK(U(Rectangle)(m.dc, 2, 2, 10, 10));
    int n = g3dc_count(&m, BLUE);
    t_note("NULL_PEN rectangle 8x8 fills %d pixels", n);
    CHECK(n > 0);
    PXC(m, 5, 5, BLUE);
    U(SelectObject)(m.dc, op); U(SelectObject)(m.dc, ob); U(DeleteObject)(br);
    g3dc_free(&m);
}
TEST(gap3_rect_inverted_coords) {
    G3DC m; if (!g3dc_make(&m, 20, 20)) { CHECK(0); return; }
    PENSEL ps; pen_on(&m, &ps, PS_SOLID, 1, RED);
    HGDIOBJ ob = U(SelectObject)(m.dc, U(GetStockObject)(NULL_BRUSH));
    g3dc_clear(&m, BLACK);
    CHECK(U(Rectangle)(m.dc, 10, 10, 2, 2));
    int n = g3dc_count(&m, RED);
    t_note("inverted rectangle outline pixels = %d", n);
    CHECK(n > 0);
    U(SelectObject)(m.dc, ob);
    pen_off(&m, &ps);
    g3dc_free(&m);
}
TEST(gap3_rect_degenerate) {
    G3DC m; if (!g3dc_make(&m, 20, 20)) { CHECK(0); return; }
    PENSEL ps; pen_on(&m, &ps, PS_SOLID, 1, RED);
    g3dc_clear(&m, BLACK);
    BOOL r = U(Rectangle)(m.dc, 5, 5, 5, 5);
    t_note("Rectangle(5,5,5,5) = %d, pixels %d", r, g3dc_count(&m, RED));
    r = U(Rectangle)(m.dc, 5, 5, 6, 6);
    t_note("Rectangle(5,5,6,6) = %d, pixels %d", r, g3dc_count(&m, RED));
    r = U(Rectangle)(m.dc, 2, 5, 12, 5);
    t_note("Rectangle(2,5,12,5) = %d", r);
    pen_off(&m, &ps);
    g3dc_free(&m);
}
TEST(gap3_rect_invalid_dc) {
    __typeof__(&Rectangle) R;
    IMPLS(R, Rectangle)
        SET_ERR(0);
        EQ(R(NULL, 0, 0, 5, 5), 0);
        WERR(G3_ERR_INVALID_HANDLE);
        HDC st = g3_stale_dc();
        EQ(R(st, 0, 0, 5, 5), 0);
        EQ(R((HDC)U(GetStockObject)(BLACK_PEN), 0, 0, 5, 5), 0);
    }
}
TEST(gap3_rect_clipped_to_surface) {
    G3DC m; if (!g3dc_make(&m, 12, 12)) { CHECK(0); return; }
    PENSEL ps; pen_on(&m, &ps, PS_SOLID, 1, RED);
    HBRUSH br = U(CreateSolidBrush)(BLUE);
    HGDIOBJ ob = U(SelectObject)(m.dc, br);
    g3dc_clear(&m, BLACK);
    CHECK(U(Rectangle)(m.dc, -5, -5, 6, 6));
    PXC(m, 0, 0, BLUE); PXC(m, 4, 4, BLUE);
    PXC(m, 5, 5, RED); PXC(m, 5, 0, RED); PXC(m, 0, 5, RED);
    PXC(m, 6, 6, BLACK);
    EQ(g3dc_count(&m, RED), 11);
    CHECK(U(Rectangle)(m.dc, 8, 8, 1000, 1000));
    CHECK(U(Rectangle)(m.dc, INT_MIN / 2, INT_MIN / 2, INT_MAX / 2, INT_MAX / 2));
    U(SelectObject)(m.dc, ob); U(DeleteObject)(br);
    pen_off(&m, &ps);
    g3dc_free(&m);
}
TEST(gap3_rect_wide_pen) {
    G3DC m; if (!g3dc_make(&m, 30, 30)) { CHECK(0); return; }
    PENSEL ps; pen_on(&m, &ps, PS_INSIDEFRAME, 3, RED);
    HGDIOBJ ob = U(SelectObject)(m.dc, U(GetStockObject)(NULL_BRUSH));
    g3dc_clear(&m, BLACK);
    CHECK(U(Rectangle)(m.dc, 5, 5, 25, 25));
    /* inside-frame pen: border stays within [l,r) x [t,b) */
    SOFT(g3dc_get(&m, 5, 5) == g3_px(RED)); PXC(m, 7, 15, RED); PXC(m, 24, 15, RED);
    PXC(m, 4, 15, BLACK); PXC(m, 25, 15, BLACK); PXC(m, 15, 4, BLACK); PXC(m, 15, 25, BLACK);
    PXC(m, 15, 15, BLACK); PXC(m, 8, 15, BLACK);
    U(SelectObject)(m.dc, ob);
    pen_off(&m, &ps);
    g3dc_free(&m);
}
TEST(gap3_rect_does_not_move_cp) {
    G3DC m; if (!g3dc_make(&m, 20, 20)) { CHECK(0); return; }
    POINT p;
    U(MoveToEx)(m.dc, 7, 8, NULL);
    U(Rectangle)(m.dc, 1, 1, 5, 5);
    U(GetCurrentPositionEx)(m.dc, &p);
    EQ(p.x, 7); EQ(p.y, 8);
    g3dc_free(&m);
}
TEST(gap3_rect_rop2) {
    G3DC m; if (!g3dc_make(&m, 16, 16)) { CHECK(0); return; }
    PENSEL ps; pen_on(&m, &ps, PS_SOLID, 1, RGB(0xff,0xff,0xff));
    HGDIOBJ ob = U(SelectObject)(m.dc, U(GetStockObject)(NULL_BRUSH));
    g3dc_clear(&m, RGB(0x0f,0xf0,0x55));
    U(SetROP2)(m.dc, R2_XORPEN);
    CHECK(U(Rectangle)(m.dc, 2, 2, 10, 10));
    PXC(m, 2, 2, RGB(0xf0,0x0f,0xaa));
    PXC(m, 6, 6, RGB(0x0f,0xf0,0x55));
    U(SetROP2)(m.dc, R2_NOP);
    g3dc_clear(&m, BLACK);
    CHECK(U(Rectangle)(m.dc, 2, 2, 10, 10));
    EQ(g3dc_count(&m, BLACK), 256);
    U(SelectObject)(m.dc, ob);
    pen_off(&m, &ps);
    g3dc_free(&m);
}
TEST(gap3_rect_world_transform) {
    G3DC m; if (!g3dc_make(&m, 40, 40)) { CHECK(0); return; }
    PENSEL ps; pen_on(&m, &ps, PS_SOLID, 1, RED);
    HGDIOBJ ob = U(SelectObject)(m.dc, U(GetStockObject)(NULL_BRUSH));
    XFORM x = { 1, 0, 0, 1, 10, 10 };
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    U(SetWorldTransform)(m.dc, &x);
    g3dc_clear(&m, BLACK);
    CHECK(U(Rectangle)(m.dc, 2, 2, 8, 8));
    PXC(m, 12, 12, RED); SOFT(g3dc_get(&m, 17, 17) == g3_px(RED)); PXC(m, 2, 2, BLACK);
    U(SelectObject)(m.dc, ob);
    pen_off(&m, &ps);
    g3dc_free(&m);
}
TEST(gap3_rect_mapmode_flip) {
    G3DC m; if (!g3dc_make(&m, 20, 20)) { CHECK(0); return; }
    PENSEL ps; pen_on(&m, &ps, PS_SOLID, 1, RED);
    HGDIOBJ ob = U(SelectObject)(m.dc, U(GetStockObject)(NULL_BRUSH));
    U(SetMapMode)(m.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(m.dc, 1, -1, NULL);
    U(SetViewportExtEx)(m.dc, 1, 1, NULL);
    U(SetViewportOrgEx)(m.dc, 0, 19, NULL);
    g3dc_clear(&m, BLACK);
    CHECK(U(Rectangle)(m.dc, 2, 2, 8, 8));
    int n = g3dc_count(&m, RED);
    t_note("flipped y rectangle pixels = %d", n);
    CHECK(n > 0);
    U(SelectObject)(m.dc, ob);
    pen_off(&m, &ps);
    g3dc_free(&m);
}
TEST(gap3_rect_into_path) {
    G3DC m; if (!g3dc_make(&m, 20, 20)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(BLUE);
    HGDIOBJ ob = U(SelectObject)(m.dc, br);
    g3dc_clear(&m, BLACK);
    CHECK(U(BeginPath)(m.dc));
    CHECK(U(Rectangle)(m.dc, 3, 3, 9, 9));
    CHECK(U(EndPath)(m.dc));
    EQ(g3dc_count(&m, BLUE), 0);            /* nothing drawn while recording */
    CHECK(U(FillPath)(m.dc));
    PXC(m, 5, 5, BLUE); PXC(m, 2, 5, BLACK);
    U(SelectObject)(m.dc, ob); U(DeleteObject)(br);
    g3dc_free(&m);
}
TEST(gap3_rect_gdicount) {
    DWORD b = g3_gui(); int i;
    G3DC m; if (!g3dc_make(&m, 10, 10)) { CHECK(0); return; }
    for (i = 0; i < 2000; i++) U(Rectangle)(m.dc, i % 7, i % 5, 9, 9);
    g3dc_free(&m);
    EQ(g3_gui(), b);
}
static void rect_thread(void *p) { HDC d = p; U(Rectangle)(d, 1, 1, 5, 5); }
TEST(gap3_rect_cross_thread) {
    G3DC m; if (!g3dc_make(&m, 10, 10)) { CHECK(0); return; }
    PENSEL ps; pen_on(&m, &ps, PS_SOLID, 1, RED);
    g3dc_clear(&m, BLACK);
    g3_run_thread(rect_thread, m.dc);
    PXC(m, 1, 1, RED);
    pen_off(&m, &ps);
    g3dc_free(&m);
}
TEST(gap3_rect_1bpp_dc) {
    HDC d = g3_mem();
    HBITMAP bm = U(CreateBitmap)(16, 16, 1, 1, NULL);
    HGDIOBJ ob = U(SelectObject)(d, bm);
    U(PatBlt)(d, 0, 0, 16, 16, WHITENESS);
    CHECK(U(Rectangle)(d, 2, 2, 10, 10));
    EQ(U(GetPixel)(d, 2, 2), 0);              /* black pen */
    EQ(U(GetPixel)(d, 5, 5), 0xffffff);       /* white brush */
    EQ(U(GetPixel)(d, 12, 12), 0xffffff);
    U(SelectObject)(d, ob); U(DeleteObject)(bm); U(DeleteDC)(d);
}

/* ---------- PolylineTo ---------- */
TEST(gap3_polylineto_basic) {
    __typeof__(&PolylineTo) P;
    IMPLS(P, PolylineTo)
        G3DC m; if (!g3dc_make(&m, 32, 16)) { CHECK(0); return; }
        PENSEL ps; pen_on(&m, &ps, PS_SOLID, 1, WHITE);
        POINT pts[3] = { {10, 3}, {10, 9}, {20, 9} }, cp;
        g3dc_clear(&m, BLACK);
        U(MoveToEx)(m.dc, 2, 3, NULL);
        CHECK(P(m.dc, pts, 3));
        U(GetCurrentPositionEx)(m.dc, &cp);
        EQ(cp.x, 20); EQ(cp.y, 9);
        PXC(m, 2, 3, WHITE); PXC(m, 9, 3, WHITE); PXC(m, 10, 3, WHITE);
        PXC(m, 10, 8, WHITE); PXC(m, 10, 9, WHITE); PXC(m, 19, 9, WHITE);
        PXC(m, 20, 9, BLACK); PXC(m, 1, 3, BLACK); PXC(m, 5, 4, BLACK);
        pen_off(&m, &ps);
        g3dc_free(&m);
    }
}
TEST(gap3_polylineto_count_bounds) {
    __typeof__(&PolylineTo) P;
    IMPLS(P, PolylineTo)
        G3DC m; if (!g3dc_make(&m, 16, 16)) { CHECK(0); return; }
        POINT pt = { 8, 8 };
        SET_ERR(0);
        EQ(P(m.dc, &pt, 0), 0);
        CHECK(P(m.dc, &pt, 1));
        g3dc_free(&m);
    }
}
TEST(gap3_polylineto_invalid_dc) {
    __typeof__(&PolylineTo) P;
    IMPLS(P, PolylineTo)
        POINT pt[2] = { {1,1}, {2,2} };
        SET_ERR(0);
        EQ(P(NULL, pt, 2), 0);
        WERR(G3_ERR_INVALID_HANDLE);
        HDC st = g3_stale_dc();
        EQ(P(st, pt, 2), 0);
        EQ(P((HDC)U(GetStockObject)(BLACK_PEN), pt, 2), 0);
    }
}
TEST(gap3_polylineto_null_points) {
    G3_WINE_SKIP("NULL point array");
    HDC d = g3_mem();
    BOOL r = U(PolylineTo)(d, NULL, 2);
    t_note("PolylineTo(NULL,2) = %d", r);
    CHECK(!r);
    r = U(PolylineTo)(d, NULL, 0);
    CHECK(!r);
    U(DeleteDC)(d);
}
TEST(gap3_polylineto_sequence_continues) {
    G3DC m; if (!g3dc_make(&m, 32, 8)) { CHECK(0); return; }
    PENSEL ps; pen_on(&m, &ps, PS_SOLID, 1, WHITE);
    POINT a = { 10, 4 }, b = { 20, 4 };
    g3dc_clear(&m, BLACK);
    U(MoveToEx)(m.dc, 0, 4, NULL);
    CHECK(U(PolylineTo)(m.dc, &a, 1));
    CHECK(U(PolylineTo)(m.dc, &b, 1));
    EQ(g3dc_count_rect(&m, 0, 4, 20, 5, WHITE), 20);
    PXC(m, 20, 4, BLACK);
    pen_off(&m, &ps);
    g3dc_free(&m);
}
TEST(gap3_polylineto_in_path) {
    G3DC m; if (!g3dc_make(&m, 24, 24)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(BLUE);
    HGDIOBJ ob = U(SelectObject)(m.dc, br);
    POINT pts[3] = { {16, 4}, {16, 16}, {4, 16} };
    g3dc_clear(&m, BLACK);
    CHECK(U(BeginPath)(m.dc));
    U(MoveToEx)(m.dc, 4, 4, NULL);
    CHECK(U(PolylineTo)(m.dc, pts, 3));
    CHECK(U(CloseFigure)(m.dc));
    CHECK(U(EndPath)(m.dc));
    CHECK(U(FillPath)(m.dc));
    PXC(m, 10, 10, BLUE); PXC(m, 20, 20, BLACK); PXC(m, 2, 2, BLACK);
    U(SelectObject)(m.dc, ob); U(DeleteObject)(br);
    g3dc_free(&m);
}
TEST(gap3_polylineto_large) {
    G3DC m; if (!g3dc_make(&m, 64, 64)) { CHECK(0); return; }
    POINT *pts = malloc(sizeof(POINT) * 20000); int i;
    for (i = 0; i < 20000; i++) { pts[i].x = i & 63; pts[i].y = (i >> 3) & 63; }
    CHECK(U(PolylineTo)(m.dc, pts, 20000));
    free(pts);
    g3dc_free(&m);
}

/* ---------- PolyBezierTo ---------- */
TEST(gap3_bezierto_straight_line) {
    __typeof__(&PolyBezierTo) P;
    IMPLS(P, PolyBezierTo)
        G3DC m; if (!g3dc_make(&m, 40, 12)) { CHECK(0); return; }
        PENSEL ps; pen_on(&m, &ps, PS_SOLID, 1, WHITE);
        POINT pts[3] = { {12, 5}, {24, 5}, {36, 5} }, cp;
        g3dc_clear(&m, BLACK);
        U(MoveToEx)(m.dc, 2, 5, NULL);
        CHECK(P(m.dc, pts, 3));
        U(GetCurrentPositionEx)(m.dc, &cp);
        EQ(cp.x, 36); EQ(cp.y, 5);
        PXC(m, 2, 5, WHITE); PXC(m, 20, 5, WHITE); PXC(m, 35, 5, WHITE);
        PXC(m, 36, 5, BLACK); PXC(m, 20, 4, BLACK); PXC(m, 20, 6, BLACK);
        pen_off(&m, &ps);
        g3dc_free(&m);
    }
}
TEST(gap3_bezierto_count_rules) {
    __typeof__(&PolyBezierTo) P;
    IMPLS(P, PolyBezierTo)
        G3DC m; if (!g3dc_make(&m, 16, 16)) { CHECK(0); return; }
        POINT pts[7] = { {1,1},{2,2},{3,3},{4,4},{5,5},{6,6},{7,7} };
        SET_ERR(0);
        EQ(P(m.dc, pts, 0), 0);
        EQ(P(m.dc, pts, 1), 0);
        EQ(P(m.dc, pts, 2), 0);
        EQ(P(m.dc, pts, 4), 0);
        EQ(P(m.dc, pts, 5), 0);
        EQ(P(m.dc, pts, 7), 0);
        CHECK(P(m.dc, pts, 3));
        CHECK(P(m.dc, pts, 6));
        g3dc_free(&m);
    }
}
TEST(gap3_bezierto_failure_keeps_cp) {
    G3DC m; if (!g3dc_make(&m, 16, 16)) { CHECK(0); return; }
    POINT pts[4] = { {9,9},{9,9},{9,9},{9,9} }, cp;
    U(MoveToEx)(m.dc, 3, 4, NULL);
    EQ(U(PolyBezierTo)(m.dc, pts, 4), 0);
    U(GetCurrentPositionEx)(m.dc, &cp);
    EQ(cp.x, 3); EQ(cp.y, 4);
    g3dc_free(&m);
}
TEST(gap3_bezierto_invalid_dc) {
    __typeof__(&PolyBezierTo) P;
    IMPLS(P, PolyBezierTo)
        POINT pts[3] = { {1,1},{2,2},{3,3} };
        SET_ERR(0);
        EQ(P(NULL, pts, 3), 0);
        WERR(G3_ERR_INVALID_HANDLE);
        HDC st = g3_stale_dc();
        EQ(P(st, pts, 3), 0);
        EQ(P((HDC)U(GetStockObject)(WHITE_BRUSH), pts, 3), 0);
    }
}
TEST(gap3_bezierto_curve_pixels) {
    G3DC m; if (!g3dc_make(&m, 40, 40)) { CHECK(0); return; }
    PENSEL ps; pen_on(&m, &ps, PS_SOLID, 1, WHITE);
    POINT pts[3] = { {2, 30}, {38, 30}, {38, 2} };
    g3dc_clear(&m, BLACK);
    U(MoveToEx)(m.dc, 2, 2, NULL);
    CHECK(U(PolyBezierTo)(m.dc, pts, 3));
    PXC(m, 2, 2, WHITE);
    int n = g3dc_count(&m, WHITE);
    CHECK(n > 30);
    EQ(g3dc_count_rect(&m, 0, 0, 40, 1, WHITE), 0);   /* nothing above y=2 */
    /* the curve lies within its control hull */
    EQ(g3dc_count_rect(&m, 0, 31, 40, 40, WHITE), 0);
    pen_off(&m, &ps);
    g3dc_free(&m);
}
TEST(gap3_bezierto_in_path_stroke) {
    G3DC m; if (!g3dc_make(&m, 40, 12)) { CHECK(0); return; }
    PENSEL ps; pen_on(&m, &ps, PS_SOLID, 1, WHITE);
    POINT pts[3] = { {12, 5}, {24, 5}, {36, 5} };
    g3dc_clear(&m, BLACK);
    CHECK(U(BeginPath)(m.dc));
    U(MoveToEx)(m.dc, 2, 5, NULL);
    CHECK(U(PolyBezierTo)(m.dc, pts, 3));
    CHECK(U(EndPath)(m.dc));
    CHECK(U(StrokePath)(m.dc));
    PXC(m, 10, 5, WHITE); PXC(m, 30, 5, WHITE); PXC(m, 20, 8, BLACK);
    pen_off(&m, &ps);
    g3dc_free(&m);
}

/* ---------- PolyDraw ---------- */
TEST(gap3_polydraw_lines) {
    __typeof__(&PolyDraw) P;
    IMPLS(P, PolyDraw)
        G3DC m; if (!g3dc_make(&m, 32, 16)) { CHECK(0); return; }
        PENSEL ps; pen_on(&m, &ps, PS_SOLID, 1, WHITE);
        POINT pts[3] = { {2, 2}, {12, 2}, {12, 10} }; BYTE ty[3] = { PT_MOVETO, PT_LINETO, PT_LINETO };
        POINT cp;
        g3dc_clear(&m, BLACK);
        CHECK(P(m.dc, pts, ty, 3));
        U(GetCurrentPositionEx)(m.dc, &cp);
        EQ(cp.x, 12); EQ(cp.y, 10);
        PXC(m, 2, 2, WHITE); PXC(m, 11, 2, WHITE); PXC(m, 12, 2, WHITE); PXC(m, 12, 9, WHITE);
        PXC(m, 12, 10, BLACK); PXC(m, 1, 2, BLACK); PXC(m, 6, 3, BLACK);
        pen_off(&m, &ps);
        g3dc_free(&m);
    }
}
TEST(gap3_polydraw_closefigure) {
    G3DC m; if (!g3dc_make(&m, 24, 24)) { CHECK(0); return; }
    PENSEL ps; pen_on(&m, &ps, PS_SOLID, 1, WHITE);
    POINT pts[4] = { {4, 4}, {16, 4}, {16, 16}, {4, 16} };
    BYTE ty[4] = { PT_MOVETO, PT_LINETO, PT_LINETO, PT_LINETO | PT_CLOSEFIGURE };
    g3dc_clear(&m, BLACK);
    CHECK(U(PolyDraw)(m.dc, pts, ty, 4));
    PXC(m, 4, 10, WHITE);     /* closing edge x=4 */
    PXC(m, 10, 4, WHITE); PXC(m, 16, 10, WHITE); PXC(m, 10, 16, WHITE);
    PXC(m, 10, 10, BLACK);
    POINT cp; U(GetCurrentPositionEx)(m.dc, &cp);
    t_note("cp after closefigure = (%ld,%ld)", cp.x, cp.y);
    pen_off(&m, &ps);
    g3dc_free(&m);
}
TEST(gap3_polydraw_moveto_gap) {
    G3DC m; if (!g3dc_make(&m, 32, 8)) { CHECK(0); return; }
    PENSEL ps; pen_on(&m, &ps, PS_SOLID, 1, WHITE);
    POINT pts[4] = { {2, 3}, {8, 3}, {14, 3}, {20, 3} };
    BYTE ty[4] = { PT_MOVETO, PT_LINETO, PT_MOVETO, PT_LINETO };
    g3dc_clear(&m, BLACK);
    CHECK(U(PolyDraw)(m.dc, pts, ty, 4));
    PXC(m, 5, 3, WHITE); PXC(m, 10, 3, BLACK); PXC(m, 17, 3, WHITE); PXC(m, 20, 3, BLACK);
    pen_off(&m, &ps);
    g3dc_free(&m);
}
TEST(gap3_polydraw_bezier) {
    G3DC m; if (!g3dc_make(&m, 40, 12)) { CHECK(0); return; }
    PENSEL ps; pen_on(&m, &ps, PS_SOLID, 1, WHITE);
    POINT pts[4] = { {2, 5}, {12, 5}, {24, 5}, {36, 5} };
    BYTE ty[4] = { PT_MOVETO, PT_BEZIERTO, PT_BEZIERTO, PT_BEZIERTO };
    g3dc_clear(&m, BLACK);
    CHECK(U(PolyDraw)(m.dc, pts, ty, 4));
    PXC(m, 2, 5, WHITE); PXC(m, 20, 5, WHITE); PXC(m, 35, 5, WHITE); PXC(m, 36, 5, BLACK);
    pen_off(&m, &ps);
    g3dc_free(&m);
}
TEST(gap3_polydraw_bad_types) {
    G3DC m; if (!g3dc_make(&m, 16, 16)) { CHECK(0); return; }
    POINT pts[3] = { {1,1}, {5,5}, {9,9} };
    static const BYTE bad[] = { 0, 3, 5, 7, 0x10, 0x55, 0x80, 0xff };
    unsigned k;
    for (k = 0; k < sizeof bad; k++) {
        BYTE ty[3] = { PT_MOVETO, bad[k], PT_LINETO };
        SET_ERR(0);
        BOOL r = U(PolyDraw)(m.dc, pts, ty, 3);
        t_note("PolyDraw type 0x%02x = %d", bad[k], r);
    }
    BYTE ty2[3] = { PT_LINETO, PT_LINETO, PT_LINETO };
    BOOL r2 = U(PolyDraw)(m.dc, pts, ty2, 3);
    t_note("PolyDraw without leading MOVETO = %d", r2);
    g3dc_free(&m);
}
TEST(gap3_polydraw_counts) {
    __typeof__(&PolyDraw) P;
    IMPLS(P, PolyDraw)
        G3DC m; if (!g3dc_make(&m, 16, 16)) { CHECK(0); return; }
        POINT pts[2] = { {1,1}, {5,5} }; BYTE ty[2] = { PT_MOVETO, PT_LINETO };
        SET_ERR(0);
        SOFT(P(m.dc, pts, ty, 0) == 0);
        CHECK(P(m.dc, pts, ty, 2));
        CHECK(P(m.dc, pts, ty, 1));
        g3dc_free(&m);
    }
}
TEST(gap3_polydraw_invalid_dc) {
    __typeof__(&PolyDraw) P;
    IMPLS(P, PolyDraw)
        POINT pts[2] = { {1,1}, {5,5} }; BYTE ty[2] = { PT_MOVETO, PT_LINETO };
        SET_ERR(0);
        EQ(P(NULL, pts, ty, 2), 0);
        WERR(G3_ERR_INVALID_HANDLE);
        HDC st = g3_stale_dc();
        EQ(P(st, pts, ty, 2), 0);
        EQ(P((HDC)U(GetStockObject)(BLACK_PEN), pts, ty, 2), 0);
    }
}
TEST(gap3_polydraw_null_arrays) {
    G3_WINE_SKIP("NULL arrays");
    HDC d = g3_mem(); POINT pts[2] = { {1,1}, {5,5} }; BYTE ty[2] = { PT_MOVETO, PT_LINETO };
    BOOL r;
    r = U(PolyDraw)(d, NULL, ty, 2); t_note("PolyDraw(NULL pts) = %d", r); CHECK(!r);
    r = U(PolyDraw)(d, pts, NULL, 2); t_note("PolyDraw(NULL types) = %d", r); CHECK(!r);
    U(DeleteDC)(d);
}
TEST(gap3_polydraw_in_path_clip) {
    G3DC m; if (!g3dc_make(&m, 24, 24)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(BLUE);
    HGDIOBJ ob = U(SelectObject)(m.dc, br);
    POINT pts[4] = { {4, 4}, {16, 4}, {16, 16}, {4, 16} };
    BYTE ty[4] = { PT_MOVETO, PT_LINETO, PT_LINETO, PT_LINETO | PT_CLOSEFIGURE };
    g3dc_clear(&m, BLACK);
    CHECK(U(BeginPath)(m.dc));
    CHECK(U(PolyDraw)(m.dc, pts, ty, 4));
    CHECK(U(EndPath)(m.dc));
    CHECK(U(FillPath)(m.dc));
    PXC(m, 10, 10, BLUE); PXC(m, 2, 2, BLACK); PXC(m, 18, 18, BLACK);
    U(SelectObject)(m.dc, ob); U(DeleteObject)(br);
    g3dc_free(&m);
}

/* ---------- StrokePath ---------- */
TEST(gap3_strokepath_basic) {
    __typeof__(&StrokePath) S;
    IMPLS(S, StrokePath)
        G3DC m; if (!g3dc_make(&m, 40, 12)) { CHECK(0); return; }
        PENSEL ps; pen_on(&m, &ps, PS_SOLID, 1, WHITE);
        g3dc_clear(&m, BLACK);
        CHECK(U(BeginPath)(m.dc));
        U(MoveToEx)(m.dc, 4, 5, NULL); U(LineTo)(m.dc, 30, 5);
        CHECK(U(EndPath)(m.dc));
        EQ(g3dc_count(&m, WHITE), 0);
        CHECK(S(m.dc));
        PXC(m, 10, 5, WHITE); PXC(m, 20, 5, WHITE); PXC(m, 10, 4, BLACK); PXC(m, 10, 6, BLACK);
        PXC(m, 33, 5, BLACK);
        /* the path is consumed */
        SET_ERR(0);
        EQ(S(m.dc), 0);
        pen_off(&m, &ps);
        g3dc_free(&m);
    }
}
TEST(gap3_strokepath_no_path) {
    __typeof__(&StrokePath) S;
    IMPLS(S, StrokePath)
        HDC d = g3_mem();
        SET_ERR(0);
        EQ(S(d), 0);
        t_note("StrokePath w/o path: err=%lu", GetLastError());
        U(DeleteDC)(d);
    }
}
TEST(gap3_strokepath_open_bracket) {
    HDC d = g3_mem();
    CHECK(U(BeginPath)(d));
    U(MoveToEx)(d, 1, 1, NULL); U(LineTo)(d, 5, 5);
    SET_ERR(0);
    EQ(U(StrokePath)(d), 0);                 /* path still open */
    t_note("StrokePath during open bracket: err=%lu", GetLastError());
    U(AbortPath)(d);
    U(DeleteDC)(d);
}
TEST(gap3_strokepath_invalid_dc) {
    __typeof__(&StrokePath) S;
    IMPLS(S, StrokePath)
        SET_ERR(0);
        EQ(S(NULL), 0);
        WERR(G3_ERR_INVALID_HANDLE);
        HDC st = g3_stale_dc();
        EQ(S(st), 0);
        EQ(S((HDC)U(GetStockObject)(BLACK_PEN)), 0);
    }
}
TEST(gap3_strokepath_wide_pen) {
    G3DC m; if (!g3dc_make(&m, 40, 20)) { CHECK(0); return; }
    PENSEL ps; pen_on(&m, &ps, PS_SOLID, 5, WHITE);
    g3dc_clear(&m, BLACK);
    CHECK(U(BeginPath)(m.dc));
    U(MoveToEx)(m.dc, 5, 10, NULL); U(LineTo)(m.dc, 35, 10);
    CHECK(U(EndPath)(m.dc));
    CHECK(U(StrokePath)(m.dc));
    PXC(m, 20, 10, WHITE); PXC(m, 20, 8, WHITE); PXC(m, 20, 12, WHITE);
    PXC(m, 20, 6, BLACK); PXC(m, 20, 14, BLACK);
    pen_off(&m, &ps);
    g3dc_free(&m);
}
TEST(gap3_strokepath_closed_figure) {
    G3DC m; if (!g3dc_make(&m, 24, 24)) { CHECK(0); return; }
    PENSEL ps; pen_on(&m, &ps, PS_SOLID, 1, WHITE);
    g3dc_clear(&m, BLACK);
    CHECK(U(BeginPath)(m.dc));
    U(MoveToEx)(m.dc, 4, 4, NULL); U(LineTo)(m.dc, 16, 4); U(LineTo)(m.dc, 16, 16);
    CHECK(U(CloseFigure)(m.dc));
    CHECK(U(EndPath)(m.dc));
    CHECK(U(StrokePath)(m.dc));
    PXC(m, 10, 4, WHITE); PXC(m, 16, 10, WHITE); PXC(m, 10, 10, WHITE);   /* diagonal closing edge */
    PXC(m, 6, 14, BLACK);
    pen_off(&m, &ps);
    g3dc_free(&m);
}
TEST(gap3_strokepath_null_pen) {
    G3DC m; if (!g3dc_make(&m, 24, 24)) { CHECK(0); return; }
    HGDIOBJ op = U(SelectObject)(m.dc, U(GetStockObject)(NULL_PEN));
    g3dc_clear(&m, BLACK);
    CHECK(U(BeginPath)(m.dc));
    U(MoveToEx)(m.dc, 4, 4, NULL); U(LineTo)(m.dc, 16, 4);
    CHECK(U(EndPath)(m.dc));
    BOOL r = U(StrokePath)(m.dc);
    t_note("StrokePath with NULL_PEN = %d", r);
    EQ(g3dc_count(&m, BLACK), 576);
    U(SelectObject)(m.dc, op);
    g3dc_free(&m);
}
TEST(gap3_strokepath_does_not_fill) {
    G3DC m; if (!g3dc_make(&m, 24, 24)) { CHECK(0); return; }
    PENSEL ps; pen_on(&m, &ps, PS_SOLID, 1, RED);
    HBRUSH br = U(CreateSolidBrush)(BLUE);
    HGDIOBJ ob = U(SelectObject)(m.dc, br);
    g3dc_clear(&m, BLACK);
    CHECK(U(BeginPath)(m.dc));
    U(Rectangle)(m.dc, 4, 4, 16, 16);
    CHECK(U(EndPath)(m.dc));
    CHECK(U(StrokePath)(m.dc));
    EQ(g3dc_count(&m, BLUE), 0);
    PXC(m, 4, 8, RED); PXC(m, 10, 10, BLACK);
    U(SelectObject)(m.dc, ob); U(DeleteObject)(br);
    pen_off(&m, &ps);
    g3dc_free(&m);
}
TEST(gap3_strokepath_clip_applies) {
    G3DC m; if (!g3dc_make(&m, 40, 12)) { CHECK(0); return; }
    PENSEL ps; pen_on(&m, &ps, PS_SOLID, 1, WHITE);
    HRGN rg = U(CreateRectRgn)(10, 0, 20, 12);
    U(SelectClipRgn)(m.dc, rg);
    g3dc_clear(&m, BLACK);
    CHECK(U(BeginPath)(m.dc));
    U(MoveToEx)(m.dc, 0, 5, NULL); U(LineTo)(m.dc, 39, 5);
    CHECK(U(EndPath)(m.dc));
    CHECK(U(StrokePath)(m.dc));
    EQ(g3dc_count(&m, WHITE), 10);
    PXC(m, 9, 5, BLACK); PXC(m, 10, 5, WHITE); PXC(m, 19, 5, WHITE); PXC(m, 20, 5, BLACK);
    U(SelectClipRgn)(m.dc, NULL); U(DeleteObject)(rg);
    pen_off(&m, &ps);
    g3dc_free(&m);
}

/* ---------- SelectClipPath ---------- */
static void make_square_path(HDC d, int l, int t, int r, int b) {
    U(BeginPath)(d);
    U(MoveToEx)(d, l, t, NULL); U(LineTo)(d, r, t); U(LineTo)(d, r, b); U(LineTo)(d, l, b);
    U(CloseFigure)(d);
    U(EndPath)(d);
}
TEST(gap3_clippath_copy) {
    __typeof__(&SelectClipPath) S;
    IMPLS(S, SelectClipPath)
        G3DC m; if (!g3dc_make(&m, 24, 24)) { CHECK(0); return; }
        RECT cb;
        make_square_path(m.dc, 4, 4, 12, 12);
        CHECK(S(m.dc, RGN_COPY));
        EQ(U(GetClipBox)(m.dc, &cb), SIMPLEREGION);
        EQ(cb.left, 4); EQ(cb.top, 4); EQ(cb.right, 12); EQ(cb.bottom, 12);
        HBRUSH br = U(CreateSolidBrush)(RED); RECT all = { 0, 0, 24, 24 };
        g3dc_clear(&m, BLACK);
        U(FillRect)(m.dc, &all, br);
        EQ(g3dc_count(&m, RED), 64);
        PXC(m, 4, 4, RED); PXC(m, 11, 11, RED); PXC(m, 12, 12, BLACK); PXC(m, 3, 3, BLACK);
        U(DeleteObject)(br);
        g3dc_free(&m);
    }
}
TEST(gap3_clippath_consumes_path) {
    G3DC m; if (!g3dc_make(&m, 24, 24)) { CHECK(0); return; }
    make_square_path(m.dc, 4, 4, 12, 12);
    CHECK(U(SelectClipPath)(m.dc, RGN_COPY));
    SET_ERR(0);
    EQ(U(SelectClipPath)(m.dc, RGN_COPY), 0);
    t_note("second SelectClipPath err=%lu", GetLastError());
    EQ(U(FillPath)(m.dc), 0);
    g3dc_free(&m);
}
TEST(gap3_clippath_modes) {
    G3DC m; if (!g3dc_make(&m, 32, 32)) { CHECK(0); return; }
    RECT cb; HRGN base = U(CreateRectRgn)(0, 0, 10, 10);
    struct { int mode; int area; } tc[] = { { RGN_AND, 4 }, { RGN_OR, 100 + 100 - 4 }, { RGN_XOR, 100 + 100 - 8 }, { RGN_DIFF, 96 } };
    unsigned k;
    HBRUSH br = U(CreateSolidBrush)(RED); RECT all = { 0, 0, 32, 32 };
    for (k = 0; k < 4; k++) {
        U(SelectClipRgn)(m.dc, base);
        make_square_path(m.dc, 8, 8, 18, 18);
        BOOL r = U(SelectClipPath)(m.dc, tc[k].mode);
        CHECKF(r, "mode %d", tc[k].mode);
        g3dc_clear(&m, BLACK);
        U(FillRect)(m.dc, &all, br);
        EQ(g3dc_count(&m, RED), tc[k].area);
    }
    (void)cb;
    U(SelectClipRgn)(m.dc, NULL);
    U(DeleteObject)(base); U(DeleteObject)(br);
    g3dc_free(&m);
}
TEST(gap3_clippath_bad_mode) {
    __typeof__(&SelectClipPath) S;
    IMPLS(S, SelectClipPath)
        G3DC m; if (!g3dc_make(&m, 16, 16)) { CHECK(0); return; }
        static const int bad[] = { 0, 6, 7, -1, 100, INT_MAX, INT_MIN };
        unsigned k;
        for (k = 0; k < sizeof bad / sizeof *bad; k++) {
            make_square_path(m.dc, 2, 2, 8, 8);
            SET_ERR(0);
            BOOL r = S(m.dc, bad[k]);
            CHECKF(!r, "mode %d accepted", bad[k]);
            t_note("SelectClipPath mode %d -> %d err %lu", bad[k], r, GetLastError());
            U(AbortPath)(m.dc);
        }
        g3dc_free(&m);
    }
}
TEST(gap3_clippath_no_path) {
    __typeof__(&SelectClipPath) S;
    IMPLS(S, SelectClipPath)
        HDC d = g3_mem();
        SET_ERR(0);
        EQ(S(d, RGN_COPY), 0);
        t_note("SelectClipPath w/o path err=%lu", GetLastError());
        U(DeleteDC)(d);
    }
}
TEST(gap3_clippath_open_bracket) {
    HDC d = g3_mem();
    U(BeginPath)(d);
    U(MoveToEx)(d, 1, 1, NULL); U(LineTo)(d, 5, 5);
    EQ(U(SelectClipPath)(d, RGN_COPY), 0);
    U(AbortPath)(d);
    U(DeleteDC)(d);
}
TEST(gap3_clippath_invalid_dc) {
    __typeof__(&SelectClipPath) S;
    IMPLS(S, SelectClipPath)
        SET_ERR(0);
        EQ(S(NULL, RGN_COPY), 0);
        WERR(G3_ERR_INVALID_HANDLE);
        HDC st = g3_stale_dc();
        EQ(S(st, RGN_COPY), 0);
        EQ(S((HDC)U(GetStockObject)(WHITE_BRUSH), RGN_COPY), 0);
    }
}
TEST(gap3_clippath_empty_path) {
    HDC d = g3_mem();
    U(BeginPath)(d);
    U(EndPath)(d);
    BOOL r = U(SelectClipPath)(d, RGN_COPY);
    RECT cb;
    int t = U(GetClipBox)(d, &cb);
    t_note("empty path clip: ret %d, type %d", r, t);
    U(DeleteDC)(d);
}
TEST(gap3_clippath_text_path) {
    REQUIRE(U(TextOutW));
    G3DC m; if (!g3dc_make(&m, 64, 32)) { CHECK(0); return; }
    HFONT f = U(CreateFontW)(-24, 0, 0, 0, 700, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, L"Arial");
    HGDIOBJ of = U(SelectObject)(m.dc, f);
    U(SetBkMode)(m.dc, TRANSPARENT);
    CHECK(U(BeginPath)(m.dc));
    U(TextOutW)(m.dc, 2, 2, L"W", 1);
    CHECK(U(EndPath)(m.dc));
    CHECK(U(SelectClipPath)(m.dc, RGN_COPY));
    RECT cb; int t = U(GetClipBox)(m.dc, &cb);
    CHECK(t == SIMPLEREGION || t == COMPLEXREGION);
    CHECK(cb.right > cb.left && cb.bottom > cb.top);
    CHECK(cb.left >= 0 && cb.right <= 64);
    U(SelectClipRgn)(m.dc, NULL);
    U(SelectObject)(m.dc, of); U(DeleteObject)(f);
    g3dc_free(&m);
}
TEST(gap3_clippath_save_restore) {
    G3DC m; if (!g3dc_make(&m, 24, 24)) { CHECK(0); return; }
    RECT cb;
    U(SaveDC)(m.dc);
    make_square_path(m.dc, 4, 4, 12, 12);
    CHECK(U(SelectClipPath)(m.dc, RGN_COPY));
    U(RestoreDC)(m.dc, -1);
    EQ(U(GetClipBox)(m.dc, &cb), SIMPLEREGION);
    EQ(cb.left, 0); EQ(cb.right, 24);
    g3dc_free(&m);
}
TEST(gap3_clippath_wmf_dc) {
    REQUIRE(U(CreateMetaFileW));
    HDC mf = U(CreateMetaFileW)(NULL);
    if (!mf) { t_skip("no wmf dc"); return; }
    BOOL r = U(SelectClipPath)(mf, RGN_COPY);
    t_note("SelectClipPath on wmf dc = %d", r);
    HMETAFILE h = U(CloseMetaFile)(mf);
    if (h) U(DeleteMetaFile)(h);
}
