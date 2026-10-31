/* Path bracket state machine: BeginPath / EndPath / AbortPath / CloseFigure and error behaviour. */
#include "ph.h"

#define SETUP(m) MD m; REQ_BASE(); if (!md_create(&m, 32, 32)) { t_skip("no DIB"); return; }

TEST(paths_state_begin_end_basic) {
    SETUP(m);
    SET_ERR(0xdeadbeef);
    CHECK(BP(m.dc));
    CHECK(EP(m.dc));
    EXPECT_EQ(GPN(m.dc), 0);
    md_destroy(&m);
}
TEST(paths_state_end_without_begin) {
    SETUP(m);
    SET_ERR(0xdeadbeef);
    CHECK(!EP(m.dc));
    EXPECT_ERR(0, ERROR_CAN_NOT_COMPLETE);
    md_destroy(&m);
}
TEST(paths_state_end_twice) {
    SETUP(m);
    CHECK(BP(m.dc)); CHECK(EP(m.dc));
    SET_ERR(0xdeadbeef);
    CHECK(!EP(m.dc));
    EXPECT_ERR(0, ERROR_CAN_NOT_COMPLETE);
    CHECK(GPN(m.dc) != -1);   /* the closed path survives the failed EndPath */
    md_destroy(&m);
}
TEST(paths_state_end_after_abort) {
    SETUP(m);
    CHECK(BP(m.dc)); CHECK(AP(m.dc));
    SET_ERR(0xdeadbeef);
    CHECK(!EP(m.dc));
    EXPECT_ERR(0, ERROR_CAN_NOT_COMPLETE);
    md_destroy(&m);
}
TEST(paths_state_abort_without_path) {
    SETUP(m);
    SET_ERR(0xdeadbeef);
    BOOL r = AP(m.dc);
    if (!r) t_note("AbortPath without a path returned FALSE (le=%lu)", GetLastError());
    EXPECT_EQ(GPN(m.dc), -1);
    md_destroy(&m);
}
TEST(paths_state_abort_open_path) {
    SETUP(m);
    CHECK(BP(m.dc)); LT(m.dc, 5, 5);
    CHECK(AP(m.dc));
    EXPECT_EQ(GPN(m.dc), -1);
    /* further drawing is no longer recorded */
    LT(m.dc, 9, 9);
    EXPECT_EQ(GPN(m.dc), -1);
    md_destroy(&m);
}
TEST(paths_state_abort_closed_path) {
    SETUP(m);
    CHECK(BP(m.dc)); LT(m.dc, 5, 5); CHECK(EP(m.dc));
    CHECK(GPN(m.dc) > 0);
    CHECK(AP(m.dc));
    EXPECT_EQ(GPN(m.dc), -1);
    CHECK(!FP(m.dc));
    md_destroy(&m);
}
TEST(paths_state_abort_twice) {
    SETUP(m);
    CHECK(BP(m.dc)); CHECK(AP(m.dc));
    BOOL r = AP(m.dc);
    if (!r) t_note("second AbortPath returned FALSE");
    EXPECT_EQ(GPN(m.dc), -1);
    md_destroy(&m);
}
TEST(paths_state_nested_begin_discards_open) {
    SETUP(m);
    CHECK(BP(m.dc));
    MT(m.dc, 1, 1); LT(m.dc, 5, 5); LT(m.dc, 9, 1);
    CHECK(BP(m.dc));                 /* second BeginPath restarts */
    CHECK(EP(m.dc));
    EXPECT_EQ(GPN(m.dc), 0);
    CHECK(!EP(m.dc));                /* only one bracket is open */
    md_destroy(&m);
}
TEST(paths_state_nested_begin_triple) {
    SETUP(m);
    CHECK(BP(m.dc)); CHECK(BP(m.dc)); CHECK(BP(m.dc));
    LT(m.dc, 3, 3);
    CHECK(EP(m.dc));
    EXPECT_EQ(GPN(m.dc), 2);
    CHECK(!EP(m.dc));
    md_destroy(&m);
}
TEST(paths_state_begin_replaces_closed_path) {
    SETUP(m);
    CHECK(BP(m.dc)); MT(m.dc, 1, 1); LT(m.dc, 5, 5); LT(m.dc, 6, 9); CHECK(EP(m.dc));
    EXPECT_EQ(GPN(m.dc), 3);
    CHECK(BP(m.dc));
    EXPECT_EQ(GPN(m.dc), -1);        /* a path is being constructed */
    LT(m.dc, 10, 10);
    CHECK(EP(m.dc));
    EXPECT_EQ(GPN(m.dc), 2);
    md_destroy(&m);
}
TEST(paths_state_begin_after_fill) {
    SETUP(m);
    CHECK(BP(m.dc)); POLYGON4(m.dc, 2, 2, 8, 8); CHECK(EP(m.dc)); CHECK(FP(m.dc));
    EXPECT_EQ(GPN(m.dc), -1);
    CHECK(BP(m.dc)); CHECK(EP(m.dc));
    EXPECT_EQ(GPN(m.dc), 0);
    md_destroy(&m);
}
TEST(paths_state_getpath_open_is_error) {
    SETUP(m);
    CHECK(BP(m.dc)); LT(m.dc, 4, 4);
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(GPN(m.dc), -1);
    EXPECT_ERR(0, ERROR_CAN_NOT_COMPLETE);
    AP(m.dc);
    md_destroy(&m);
}
TEST(paths_state_getpath_nopath_is_error) {
    SETUP(m);
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(GPN(m.dc), -1);
    t_note("GetPath without path: le=%lu", GetLastError());
    md_destroy(&m);
}
TEST(paths_state_getpath_empty_closed) {
    SETUP(m);
    CHECK(BP(m.dc)); CHECK(EP(m.dc));
    EXPECT_EQ(GPN(m.dc), 0);
    EXPECT_EQ(U(GetPath)(m.dc, g_pd.pt, g_pd.ty, 5), 0);
    md_destroy(&m);
}
TEST(paths_state_closefigure_outside_bracket) {
    SETUP(m);
    SET_ERR(0xdeadbeef);
    CHECK(!CF(m.dc));
    EXPECT_ERR(0, ERROR_CAN_NOT_COMPLETE);
    md_destroy(&m);
}
TEST(paths_state_closefigure_after_end) {
    SETUP(m);
    CHECK(BP(m.dc)); LT(m.dc, 4, 4); CHECK(EP(m.dc));
    SET_ERR(0xdeadbeef);
    CHECK(!CF(m.dc));
    EXPECT_ERR(0, ERROR_CAN_NOT_COMPLETE);
    EXPECT_EQ(GPN(m.dc), 2);
    md_destroy(&m);
}
TEST(paths_state_closefigure_empty_path) {
    SETUP(m);
    CHECK(BP(m.dc));
    CHECK(CF(m.dc));
    CHECK(EP(m.dc));
    EXPECT_EQ(GPN(m.dc), 0);
    md_destroy(&m);
}
TEST(paths_state_closefigure_flags_last_point) {
    SETUP(m);
    CHECK(BP(m.dc)); MT(m.dc, 2, 2); LT(m.dc, 10, 2); LT(m.dc, 10, 10);
    CHECK(CF(m.dc)); CHECK(EP(m.dc));
    EXPECT_EQ(pd_get(m.dc), 3);
    EXPECT_EQ(g_pd.ty[0], PT_MOVETO);
    EXPECT_EQ(g_pd.ty[1], PT_LINETO);
    EXPECT_EQ(g_pd.ty[2], PT_LINETO | PT_CLOSEFIGURE);
    md_destroy(&m);
}
TEST(paths_state_closefigure_twice) {
    SETUP(m);
    CHECK(BP(m.dc)); MT(m.dc, 2, 2); LT(m.dc, 10, 2); LT(m.dc, 10, 10);
    CHECK(CF(m.dc)); CHECK(CF(m.dc)); CHECK(EP(m.dc));
    int n = pd_get(m.dc);
    if (n != 3) t_note("CloseFigure twice: count %d", n);
    CHECK(pd_valid());
    md_destroy(&m);
}
TEST(paths_state_closefigure_then_lineto_starts_new_figure) {
    SETUP(m);
    CHECK(BP(m.dc)); MT(m.dc, 2, 2); LT(m.dc, 10, 2); LT(m.dc, 10, 10); CF(m.dc);
    LT(m.dc, 20, 20);
    CHECK(EP(m.dc));
    int n = pd_get(m.dc);
    CHECK(n >= 5);
    CHECK(pd_valid());
    t_note("lineto after closefigure: n=%d", n);
    md_destroy(&m);
}
TEST(paths_state_drawing_after_end_not_recorded) {
    SETUP(m);
    md_clear(&m, WHITE);
    CHECK(BP(m.dc)); MT(m.dc, 1, 1); LT(m.dc, 6, 1); CHECK(EP(m.dc));
    EXPECT_EQ(GPN(m.dc), 2);
    MT(m.dc, 1, 10); LT(m.dc, 12, 10);
    EXPECT_EQ(GPN(m.dc), 2);
    /* and the line really was rendered on the DIB (default black pen) */
    CHECK(md_px(&m, 5, 10) == 0);
    AP(m.dc);
    md_destroy(&m);
}
TEST(paths_state_no_pixels_during_bracket) {
    SETUP(m);
    md_clear(&m, WHITE);
    CHECK(BP(m.dc));
    MT(m.dc, 1, 5); LT(m.dc, 20, 5);
    POLYGON4(m.dc, 3, 3, 15, 15);
    U(Ellipse)(m.dc, 2, 2, 20, 20);
    U(Rectangle)(m.dc, 4, 4, 9, 9);
    CHECK(EP(m.dc));
    EXPECT_EQ(md_count_not(&m, WHITE), 0);  /* nothing rendered while recording */
    AP(m.dc);
    md_destroy(&m);
}
TEST(paths_state_no_pixels_end_only) {
    SETUP(m);
    md_clear(&m, WHITE);
    CHECK(BP(m.dc)); POLYGON4(m.dc, 3, 3, 15, 15); CHECK(EP(m.dc));
    EXPECT_EQ(md_count_not(&m, WHITE), 0);
    md_destroy(&m);
}
TEST(paths_state_getdc_current_pos_kept) {
    SETUP(m);
    POINT p;
    MT(m.dc, 7, 9);
    CHECK(BP(m.dc));
    CHECK(U(GetCurrentPositionEx)(m.dc, &p)); EXPECT_EQ(p.x, 7); EXPECT_EQ(p.y, 9);
    LT(m.dc, 12, 15);
    CHECK(EP(m.dc));
    CHECK(U(GetCurrentPositionEx)(m.dc, &p)); EXPECT_EQ(p.x, 12); EXPECT_EQ(p.y, 15);
    md_destroy(&m);
}
TEST(paths_state_lineto_without_moveto_starts_at_current_pos) {
    SETUP(m);
    MT(m.dc, 7, 9);
    CHECK(BP(m.dc)); LT(m.dc, 12, 15); CHECK(EP(m.dc));
    EXPECT_EQ(pd_get(m.dc), 2);
    CHECK(pd_near(0, 7, 9, 0)); CHECK(pd_near(1, 12, 15, 0));
    EXPECT_EQ(g_pd.ty[0], PT_MOVETO); EXPECT_EQ(g_pd.ty[1], PT_LINETO);
    md_destroy(&m);
}
TEST(paths_state_savedc_restore_reopens_path) {
    SETUP(m);
    CHECK(BP(m.dc)); LT(m.dc, 10, 10);
    CHECK(U(WidenPath)(m.dc) == FALSE);
    int id = U(SaveDC)(m.dc);
    CHECK(id > 0);
    CHECK(EP(m.dc));
    CHECK(U(WidenPath)(m.dc));
    CHECK(U(RestoreDC)(m.dc, -1));
    /* path state is part of the saved DC state: the bracket is open again */
    CHECK(!U(WidenPath)(m.dc));
    CHECK(EP(m.dc));
    md_destroy(&m);
}
TEST(paths_state_savedc_restore_discards_new_bracket) {
    SETUP(m);
    CHECK(BP(m.dc)); LT(m.dc, 10, 10); CHECK(EP(m.dc));
    CHECK(U(SaveDC)(m.dc) > 0);
    CHECK(BP(m.dc));
    CHECK(U(RestoreDC)(m.dc, -1));
    CHECK(U(WidenPath)(m.dc));      /* the earlier closed path is back */
    md_destroy(&m);
}
TEST(paths_state_select_bitmap_keeps_path) {
    SETUP(m);
    CHECK(BP(m.dc)); LT(m.dc, 10, 10);
    U(SelectObject)(m.dc, m.old);
    CHECK(!U(WidenPath)(m.dc));
    U(SelectObject)(m.dc, m.bmp);
    CHECK(EP(m.dc));
    U(SelectObject)(m.dc, m.old);
    CHECK(U(WidenPath)(m.dc));
    U(SelectObject)(m.dc, m.bmp);
    md_destroy(&m);
}
TEST(paths_state_select_pen_brush_keeps_path) {
    SETUP(m);
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(1, 2, 3));
    HBRUSH b = U(CreateSolidBrush)(RGB(4, 5, 6));
    HGDIOBJ op = U(SelectObject)(m.dc, p), ob = U(SelectObject)(m.dc, b);
    CHECK(BP(m.dc)); MT(m.dc, 1, 1); LT(m.dc, 3, 3);
    U(SelectObject)(m.dc, U(GetStockObject)(BLACK_PEN));
    U(SelectObject)(m.dc, U(GetStockObject)(WHITE_BRUSH));
    LT(m.dc, 6, 3);
    CHECK(EP(m.dc));
    EXPECT_EQ(GPN(m.dc), 3);
    AP(m.dc);
    U(SelectObject)(m.dc, op); U(SelectObject)(m.dc, ob);
    U(DeleteObject)(p); U(DeleteObject)(b);
    md_destroy(&m);
}
TEST(paths_state_no_bitmap_selected_dc) {
    /* a fresh compatible DC holds the 1x1 monochrome default bitmap */
    REQ_BASE();
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(dc != NULL);
    CHECK(BP(dc)); U(MoveToEx)(dc, 0, 0, NULL); LT(dc, 5, 5); CHECK(EP(dc));
    EXPECT_EQ(GPN(dc), 2);
    CHECK(U(FlattenPath)(dc));
    CHECK(AP(dc));
    U(DeleteDC)(dc);
}
TEST(paths_state_deleted_dc_handle) {
    REQ_BASE();
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(BP(dc)); LT(dc, 5, 5); CHECK(EP(dc));
    U(DeleteDC)(dc);
    SET_ERR(0xdeadbeef);
    CHECK(!BP(dc));
    t_note("BeginPath(stale) le=%lu", GetLastError());
    CHECK(!EP(dc)); CHECK(!FP(dc)); CHECK(!SP(dc)); CHECK(!SFP(dc)); CHECK(!CF(dc));
    CHECK(U(PathToRegion)(dc) == NULL);
    EXPECT_EQ(GPN(dc), -1);
}
TEST(paths_state_many_cycles) {
    SETUP(m);
    int i, ok = 1;
    for (i = 0; i < 2000; i++) {
        if (!BP(m.dc)) ok = 0;
        LT(m.dc, i & 31, (i >> 2) & 31);
        if (!EP(m.dc)) ok = 0;
        if (GPN(m.dc) != 2) ok = 0;
        if (!AP(m.dc)) ok = 0;
    }
    CHECK(ok);
    md_destroy(&m);
}
TEST(paths_state_two_dcs_independent) {
    MD a, b; REQ_BASE();
    if (!md_create(&a, 16, 16)) return;
    if (!md_create(&b, 16, 16)) { md_destroy(&a); return; }
    CHECK(BP(a.dc)); LT(a.dc, 5, 5);
    CHECK(!EP(b.dc));                 /* b has no bracket */
    CHECK(BP(b.dc)); MT(b.dc, 1, 1); LT(b.dc, 2, 2); LT(b.dc, 3, 3);
    CHECK(EP(a.dc)); CHECK(EP(b.dc));
    EXPECT_EQ(GPN(a.dc), 2);
    EXPECT_EQ(GPN(b.dc), 3);
    AP(a.dc);
    EXPECT_EQ(GPN(b.dc), 3);
    AP(b.dc);
    md_destroy(&a); md_destroy(&b);
}
TEST(paths_state_two_dcs_independent_bitmaps_same_dib) {
    REQ_BASE();
    MD a;
    if (!md_create(&a, 16, 16)) return;
    HDC d2 = U(CreateCompatibleDC)(NULL);
    CHECK(BP(a.dc)); LT(a.dc, 5, 5); CHECK(EP(a.dc));
    CHECK(BP(d2)); CHECK(EP(d2));
    EXPECT_EQ(GPN(a.dc), 2);
    EXPECT_EQ(GPN(d2), 0);
    U(DeleteDC)(d2);
    md_destroy(&a);
}
