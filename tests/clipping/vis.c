#include "clip.h"

#define NEED_VIS() do { NEED_CLIP(); REQUIRE(U(PtVisible)); REQUIRE(U(RectVisible)); } while (0)
static int pv(HDC dc, int x, int y) { return U(PtVisible)(dc, x, y); }
static int rv(HDC dc, int l, int t, int r, int b) { RECT rc = {l, t, r, b}; return U(RectVisible)(dc, &rc); }

TEST(clipping_vis_pt_basic_edges) {
    NEED_VIS(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    U(IntersectClipRect)(m.dc, 10, 10, 20, 20);
    CHECK(pv(m.dc, 10, 10) == 1); CHECK(pv(m.dc, 19, 19) == 1); CHECK(pv(m.dc, 10, 19) == 1); CHECK(pv(m.dc, 19, 10) == 1);
    CHECK(pv(m.dc, 20, 19) == 0); CHECK(pv(m.dc, 19, 20) == 0); CHECK(pv(m.dc, 9, 10) == 0); CHECK(pv(m.dc, 10, 9) == 0);
    CHECK(pv(m.dc, 20, 20) == 0); CHECK(pv(m.dc, 9, 9) == 0); CHECK(pv(m.dc, 15, 15) == 1);
    mdc_destroy(&m);
}
TEST(clipping_vis_pt_no_clip_surface) {
    NEED_VIS(); MDC m; CHECK(mdc_create(&m, 10, 10)); if (!m.dc) return;
    CHECK(pv(m.dc, 0, 0) == 1); CHECK(pv(m.dc, 9, 9) == 1); CHECK(pv(m.dc, 5, 5) == 1);
    int a = pv(m.dc, 10, 9), b = pv(m.dc, -1, 0), c = pv(m.dc, 0, 10);
    t_note("PtVisible outside surface: %d %d %d", a, b, c);
    CHECK(a == 0 && b == 0 && c == 0);
    mdc_destroy(&m);
}
TEST(clipping_vis_pt_extremes) {
    NEED_VIS(); MDC m; CHECK(mdc_create(&m, 10, 10)); if (!m.dc) return;
    CHECK(pv(m.dc, INT_MAX, INT_MAX) == 0); CHECK(pv(m.dc, INT_MIN, INT_MIN) == 0);
    CHECK(pv(m.dc, INT_MAX, 0) == 0); CHECK(pv(m.dc, 0, INT_MIN) == 0);
    mdc_destroy(&m);
}
TEST(clipping_vis_pt_null_clip_region) {
    NEED_VIS(); MDC m; CHECK(mdc_create(&m, 16, 16)); if (!m.dc) return;
    HRGN e = rr(0, 0, 0, 0); U(SelectClipRgn)(m.dc, e);
    int cnt = 0; for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) cnt += pv(m.dc, x, y);
    EXPECT_EQ(cnt, 0);
    CHECK(rv(m.dc, 0, 0, 16, 16) == 0);
    U(DeleteObject)(e); mdc_destroy(&m);
}
TEST(clipping_vis_pt_invalid_dc) {
    NEED_VIS();
    int a = pv(NULL, 0, 0), b = pv((HDC)(ULONG_PTR)0x1111, 0, 0);
    t_note("PtVisible(NULL)=%d PtVisible(bogus)=%d", a, b);
    CHECK(a != 1 && b != 1);
    RECT r = {0, 0, 4, 4};
    a = U(RectVisible)(NULL, &r); b = U(RectVisible)((HDC)(ULONG_PTR)0x1111, &r);
    t_note("RectVisible(NULL)=%d RectVisible(bogus)=%d", a, b);
    CHECK(a != 1 && b != 1);
}
TEST(clipping_vis_pt_deleted_dc) {
    NEED_VIS(); HDC dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    int a = pv(dc, 0, 0); RECT r = {0, 0, 1, 1}; int b = U(RectVisible)(dc, &r);
    CHECK(a != 1 && b != 1);
}
TEST(clipping_vis_rect_edges) {
    NEED_VIS(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    U(IntersectClipRect)(m.dc, 10, 10, 20, 20);
    CHECK(rv(m.dc, 0, 0, 10, 10) == 0);     /* touches the corner only */
    CHECK(rv(m.dc, 0, 0, 11, 11) == 1);     /* one pixel overlap */
    CHECK(rv(m.dc, 19, 19, 40, 40) == 1);
    CHECK(rv(m.dc, 20, 20, 40, 40) == 0);
    CHECK(rv(m.dc, 0, 0, 64, 10) == 0);
    CHECK(rv(m.dc, 0, 0, 64, 64) == 1);     /* encloses the clip */
    CHECK(rv(m.dc, 12, 12, 14, 14) == 1);   /* inside */
    CHECK(rv(m.dc, 30, 30, 40, 40) == 0);
    CHECK(rv(m.dc, -100, -100, -50, -50) == 0);
    mdc_destroy(&m);
}
TEST(clipping_vis_rect_empty_and_inverted) {
    NEED_VIS(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    int a = rv(m.dc, 10, 10, 10, 20), b = rv(m.dc, 10, 10, 20, 10), c = rv(m.dc, 0, 0, 0, 0);
    int d = rv(m.dc, 40, 40, 10, 10), e = rv(m.dc, 40, 10, 10, 40);
    t_note("empty/inverted RectVisible: %d %d %d inv %d %d", a, b, c, d, e);
    CHECK(a == 0 && b == 0 && c == 0);
    mdc_destroy(&m);
}
TEST(clipping_vis_rect_hole) {
    NEED_VIS(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    U(ExcludeClipRect)(m.dc, 20, 20, 40, 40);
    CHECK(rv(m.dc, 25, 25, 35, 35) == 0);
    CHECK(rv(m.dc, 20, 20, 40, 40) == 0);
    CHECK(rv(m.dc, 19, 25, 21, 35) == 1);
    CHECK(rv(m.dc, 0, 0, 64, 64) == 1);
    CHECK(pv(m.dc, 30, 30) == 0 && pv(m.dc, 19, 30) == 1 && pv(m.dc, 40, 30) == 1);
    mdc_destroy(&m);
}
TEST(clipping_vis_grid_vs_region_ellipse) {
    NEED_VIS(); REQUIRE(U(CreateEllipticRgn)); MDC m; CHECK(mdc_create(&m, 48, 48)); if (!m.dc) return;
    HRGN e = U(CreateEllipticRgn)(3, 5, 44, 40); U(SelectClipRgn)(m.dc, e);
    int bad = 0, badr = 0;
    for (int y = 0; y < 48; y++) for (int x = 0; x < 48; x++) {
        int in = U(PtInRegion)(e, x, y) ? 1 : 0;
        if ((pv(m.dc, x, y) ? 1 : 0) != in) bad++;
        if ((rv(m.dc, x, y, x + 1, y + 1) ? 1 : 0) != in) badr++;
    }
    CHECKF(bad == 0, "PtVisible mismatches %d", bad);
    CHECKF(badr == 0, "RectVisible mismatches %d", badr);
    U(DeleteObject)(e); mdc_destroy(&m);
}
TEST(clipping_vis_grid_vs_pixels_polygon) {
    NEED_VIS(); REQUIRE(U(CreatePolygonRgn)); MDC m; CHECK(mdc_create(&m, 48, 48)); if (!m.dc) return;
    POINT p[] = {{4, 4}, {44, 10}, {20, 44}};
    HRGN e = U(CreatePolygonRgn)(p, 3, WINDING); U(SelectClipRgn)(m.dc, e);
    U(PatBlt)(m.dc, 0, 0, 48, 48, WHITENESS);
    int bad = 0;
    for (int y = 0; y < 48; y++) for (int x = 0; x < 48; x++)
        if ((pv(m.dc, x, y) ? 1 : 0) != (mdc_px(&m, x, y) == 0xFFFFFF)) bad++;
    CHECKF(bad == 0, "bad %d", bad);
    U(DeleteObject)(e); mdc_destroy(&m);
}
TEST(clipping_vis_viewport_org_logical) {
    NEED_VIS(); REQUIRE(U(SetViewportOrgEx)); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN s = rr(10, 10, 20, 20); U(SelectClipRgn)(m.dc, s);
    U(SetViewportOrgEx)(m.dc, 5, 7, NULL);                  /* logical (x,y) -> device (x+5,y+7) */
    CHECK(pv(m.dc, 5, 3) == 1); CHECK(pv(m.dc, 14, 12) == 1); CHECK(pv(m.dc, 15, 13) == 0); CHECK(pv(m.dc, 4, 3) == 0); CHECK(pv(m.dc, 5, 2) == 0);
    CHECK(rv(m.dc, 0, 0, 5, 3) == 0); CHECK(rv(m.dc, 0, 0, 6, 4) == 1);
    CHECK(rv(m.dc, 15, 13, 30, 30) == 0);
    U(DeleteObject)(s); mdc_destroy(&m);
}
TEST(clipping_vis_window_org_logical) {
    NEED_VIS(); REQUIRE(U(SetWindowOrgEx)); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN s = rr(10, 10, 20, 20); U(SelectClipRgn)(m.dc, s);
    U(SetWindowOrgEx)(m.dc, 10, 10, NULL);                  /* logical (x,y) -> device (x-10,y-10) */
    CHECK(pv(m.dc, 20, 20) == 1); CHECK(pv(m.dc, 29, 29) == 1); CHECK(pv(m.dc, 30, 30) == 0); CHECK(pv(m.dc, 19, 20) == 0);
    CHECK(rv(m.dc, 0, 0, 21, 21) == 1); CHECK(rv(m.dc, 0, 0, 20, 20) == 0);
    U(DeleteObject)(s); mdc_destroy(&m);
}
TEST(clipping_vis_scale_logical) {
    NEED_VIS(); REQUIRE(U(SetMapMode)); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN s = rr(10, 10, 20, 20); U(SelectClipRgn)(m.dc, s);
    U(SetMapMode)(m.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(m.dc, 1, 1, NULL); U(SetViewportExtEx)(m.dc, 2, 2, NULL);   /* device = 2*logical */
    CHECK(pv(m.dc, 5, 5) == 1); CHECK(pv(m.dc, 9, 9) == 1); CHECK(pv(m.dc, 10, 10) == 0); CHECK(pv(m.dc, 4, 5) == 0);
    CHECK(rv(m.dc, 0, 0, 5, 5) == 0); CHECK(rv(m.dc, 0, 0, 6, 6) == 1);
    U(SetMapMode)(m.dc, MM_TEXT);
    U(DeleteObject)(s); mdc_destroy(&m);
}
TEST(clipping_vis_world_transform) {
    NEED_VIS(); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform)); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN s = rr(10, 10, 20, 20); U(SelectClipRgn)(m.dc, s);
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM x = { 1.0f, 0, 0, 1.0f, 100.0f, 0 };            /* logical x+100 -> device */
    U(SetWorldTransform)(m.dc, &x);
    CHECK(pv(m.dc, -90, 12) == 1); CHECK(pv(m.dc, 12, 12) == 0); CHECK(pv(m.dc, -81, 19) == 1); CHECK(pv(m.dc, -80, 19) == 0);
    CHECK(rv(m.dc, -100, 0, -89, 11) == 1); CHECK(rv(m.dc, -100, 0, -90, 11) == 0);
    XFORM id = { 1, 0, 0, 1, 0, 0 }; U(SetWorldTransform)(m.dc, &id);
    U(DeleteObject)(s); mdc_destroy(&m);
}
TEST(clipping_vis_meta_region) {
    NEED_VIS(); REQUIRE(U(SetMetaRgn)); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    U(IntersectClipRect)(m.dc, 10, 10, 30, 30); U(SetMetaRgn)(m.dc);
    HRGN s = rr(0, 0, 64, 64); U(SelectClipRgn)(m.dc, s);
    CHECK(pv(m.dc, 15, 15) == 1); CHECK(pv(m.dc, 5, 5) == 0); CHECK(pv(m.dc, 30, 30) == 0); CHECK(rv(m.dc, 31, 0, 64, 64) == 0);
    U(DeleteObject)(s); mdc_destroy(&m);
}
TEST(clipping_vis_after_save_restore) {
    NEED_VIS(); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    U(IntersectClipRect)(m.dc, 10, 10, 30, 30);
    U(SaveDC)(m.dc); U(IntersectClipRect)(m.dc, 0, 0, 15, 15);
    CHECK(pv(m.dc, 20, 20) == 0);
    U(RestoreDC)(m.dc, -1);
    CHECK(pv(m.dc, 20, 20) == 1);
    mdc_destroy(&m);
}
TEST(clipping_vis_after_offset) {
    NEED_VIS(); REQUIRE(U(OffsetClipRgn)); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    U(IntersectClipRect)(m.dc, 10, 10, 20, 20);
    U(OffsetClipRgn)(m.dc, 10, 10);
    CHECK(pv(m.dc, 15, 15) == 0); CHECK(pv(m.dc, 25, 25) == 1);
    mdc_destroy(&m);
}

/* ---- small / large surfaces -------------------------------------------- */
TEST(clipping_surface_createbitmap_zero) {
    NEED_CLIP(); REQUIRE(U(CreateBitmap));
    HBITMAP b = U(CreateBitmap)(0, 0, 1, 1, NULL);
    t_note("CreateBitmap(0,0) -> %p", (void *)b);
    if (!b) return;
    HDC dc = U(CreateCompatibleDC)(NULL); HGDIOBJ old = U(SelectObject)(dc, b);
    RECT r; int t = U(GetClipBox)(dc, &r);
    t_note("clip box of zero-size bitmap type=%d (%ld,%ld,%ld,%ld)", t, r.left, r.top, r.right, r.bottom);
    CHECK(t == SIMPLEREGION); EXPECT_RECT(r, 0, 0, 1, 1);
    U(SelectObject)(dc, old); U(DeleteObject)(b); U(DeleteDC)(dc);
}
TEST(clipping_surface_1x1_pixel) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 1, 1)); if (!m.dc) return;
    HRGN s = rr(0, 0, 1, 1);
    EXPECT_EQ(U(SelectClipRgn)(m.dc, s), SIMPLEREGION);
    U(PatBlt)(m.dc, -5, -5, 10, 10, WHITENESS);
    EXPECT_EQ(mdc_px(&m, 0, 0), 0xFFFFFF);
    EXPECT_EQ(U(ExcludeClipRect)(m.dc, 0, 0, 1, 1), NULLREGION);
    mdc_clear(&m, 0); U(PatBlt)(m.dc, 0, 0, 1, 1, WHITENESS);
    EXPECT_EQ(mdc_px(&m, 0, 0), 0);
    U(DeleteObject)(s); mdc_destroy(&m);
}
TEST(clipping_surface_wide_strip) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 4096, 1)); if (!m.dc) return;
    RECT r; EXPECT_EQ(U(GetClipBox)(m.dc, &r), SIMPLEREGION); EXPECT_RECT(r, 0, 0, 4096, 1);
    U(IntersectClipRect)(m.dc, 4000, 0, 4096, 1);
    U(PatBlt)(m.dc, 0, 0, 4096, 1, WHITENESS);
    EXPECT_EQ(mdc_count(&m, 0xFFFFFF), 96);
    mdc_destroy(&m);
}
TEST(clipping_surface_tall_strip) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 1, 4096)); if (!m.dc) return;
    RECT r; EXPECT_EQ(U(GetClipBox)(m.dc, &r), SIMPLEREGION); EXPECT_RECT(r, 0, 0, 1, 4096);
    U(ExcludeClipRect)(m.dc, 0, 100, 1, 4000);
    U(PatBlt)(m.dc, 0, 0, 1, 4096, WHITENESS);
    EXPECT_EQ(mdc_count(&m, 0xFFFFFF), 100 + 96);
    mdc_destroy(&m);
}
TEST(clipping_surface_large_2048) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 2048, 2048)); if (!m.dc) return;
    RECT r; EXPECT_EQ(U(GetClipBox)(m.dc, &r), SIMPLEREGION); EXPECT_RECT(r, 0, 0, 2048, 2048);
    U(IntersectClipRect)(m.dc, 2000, 2000, 5000, 5000);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 2000, 2000, 2048, 2048);
    U(PatBlt)(m.dc, 0, 0, 2048, 2048, WHITENESS);
    EXPECT_EQ(mdc_count(&m, 0xFFFFFF), 48 * 48);
    mdc_destroy(&m);
}
TEST(clipping_surface_region_many_rects) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 128, 128)); if (!m.dc) return;
    HRGN acc = rr(0, 0, 0, 0);
    for (int i = 0; i < 64; i++) { HRGN t = rr(i * 2, i * 2, i * 2 + 2, i * 2 + 2); U(CombineRgn)(acc, acc, t, RGN_OR); U(DeleteObject)(t); }
    EXPECT_EQ(U(SelectClipRgn)(m.dc, acc), COMPLEXREGION);
    U(PatBlt)(m.dc, 0, 0, 128, 128, WHITENESS);
    EXPECT_EQ(mdc_count(&m, 0xFFFFFF), 64 * 4);
    int bad; CHECK(mdc_matches_region(&m, acc, 0xFFFFFF, &bad));
    U(DeleteObject)(acc); mdc_destroy(&m);
}

/* ---- other DC kinds ---------------------------------------------------- */
TEST(clipping_dc_enhmetafile) {
    NEED_CLIP(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC dc = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    CHECK(dc != NULL); if (!dc) return;
    RECT r; int t = U(GetClipBox)(dc, &r);
    t_note("emf dc GetClipBox type=%d (%ld,%ld,%ld,%ld)", t, r.left, r.top, r.right, r.bottom);
    int a = U(IntersectClipRect)(dc, 10, 10, 50, 50);
    int b = U(ExcludeClipRect)(dc, 20, 20, 30, 30);
    HRGN s = rr(0, 0, 10, 10);
    int c = U(SelectClipRgn)(dc, s);
    int d = U(SelectClipRgn)(dc, NULL);
    t_note("emf dc: intersect=%d exclude=%d select=%d selectnull=%d", a, b, c, d);
    U(DeleteObject)(s);
    HENHMETAFILE h = U(CloseEnhMetaFile)(dc);
    CHECK(h != NULL);
    if (h) CHECK(U(DeleteEnhMetaFile)(h));
}
TEST(clipping_dc_screen_dc_roundtrip) {
    NEED_CLIP(); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC dc = U(GetDC)(NULL); if (!dc) { t_skip("no screen dc"); return; }
    RECT r; int t = U(GetClipBox)(dc, &r);
    t_note("screen dc clip box type=%d (%ld,%ld,%ld,%ld)", t, r.left, r.top, r.right, r.bottom);
    if (t == 0 && clip_is_wine()) { t_skip("wine without usable display"); U(ReleaseDC)(NULL, dc); return; }
    CHECK(t == SIMPLEREGION || t == COMPLEXREGION);
    EXPECT_EQ(U(GetClipRgn)(dc, NULL), 0);
    HRGN s = rr(10, 10, 20, 20);
    int ret = U(SelectClipRgn)(dc, s);
    CHECKF(ret == SIMPLEREGION, "ret %d", ret);
    U(GetClipBox)(dc, &r);
    CHECK(r.left >= 10 && r.top >= 10 && r.right <= 20 && r.bottom <= 20);
    EXPECT_EQ(U(SelectClipRgn)(dc, NULL) != 0, 1);
    U(DeleteObject)(s); U(ReleaseDC)(NULL, dc);
}
TEST(clipping_dc_information_context) {
    NEED_CLIP(); REQUIRE(U(CreateICA));
    HDC ic = U(CreateICA)("DISPLAY", NULL, NULL, NULL);
    if (!ic) { t_skip("no IC"); return; }
    RECT r; int t = U(GetClipBox)(ic, &r);
    HRGN s = rr(0, 0, 8, 8);
    int a = U(SelectClipRgn)(ic, s), b = U(IntersectClipRect)(ic, 0, 0, 4, 4);
    t_note("IC: clipbox type=%d select=%d intersect=%d", t, a, b);
    U(DeleteObject)(s); U(DeleteDC)(ic);
}
TEST(clipping_dc_printer_error_path) {
    NEED_CLIP(); REQUIRE(U(CreateDCA));
    HDC dc = U(CreateDCA)("WINSPOOL", "No Such Printer 7f3a", NULL, NULL);   /* must fail: no printer is touched */
    SET_ERR(0);
    if (dc) { t_note("unexpected: CreateDC accepted a bogus printer"); U(DeleteDC)(dc); return; }
    CHECK(dc == NULL);
    HDC dc2 = U(CreateDCA)("NO_SUCH_DRIVER_9x", NULL, NULL, NULL);
    CHECK(dc2 == NULL);
}
TEST(clipping_dc_createcompatible_inherits_nothing) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    U(IntersectClipRect)(m.dc, 10, 10, 20, 20);
    HDC c = U(CreateCompatibleDC)(m.dc);
    RECT r; EXPECT_EQ(U(GetClipBox)(c, &r), SIMPLEREGION); EXPECT_RECT(r, 0, 0, 1, 1);
    EXPECT_EQ(U(GetClipRgn)(c, NULL), 0);
    U(DeleteDC)(c); mdc_destroy(&m);
}

/* ---- window DCs -------------------------------------------------------- */
static HWND mkwin(DWORD style) {
    HWND w = t_make_window(NULL, style, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 120, 120, 200, 150, NULL);
    if (!w) t_note("CreateWindow failed err=%lu", GetLastError());
    t_pump();
    return w;
}
#define NEED_WIN() do { NEED_CLIP(); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC)); REQUIRE(U(GetWindowDC)); REQUIRE(U(DestroyWindow)); \
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetClientRect)); REQUIRE(U(IsWindowVisible)); } while (0)
TEST(clipping_win_client_dc_box) {
    NEED_WIN(); HWND w = mkwin(WS_POPUP | WS_VISIBLE); if (!w) { t_skip("no window"); return; }
    HDC dc = U(GetDC)(w); CHECK(dc != NULL);
    RECT r, cr; int t = U(GetClipBox)(dc, &r); U(GetClientRect)(w, &cr);
    t_note("client DC clip box type=%d (%ld,%ld,%ld,%ld) client (%ld,%ld)", t, r.left, r.top, r.right, r.bottom, cr.right, cr.bottom);
    CHECK(t != 0);
    CHECK(t == NULLREGION || (r.left >= 0 && r.top >= 0 && r.right <= cr.right && r.bottom <= cr.bottom));
    EXPECT_EQ(U(GetClipRgn)(dc, NULL), 0);
    U(ReleaseDC)(w, dc); U(DestroyWindow)(w);
}
TEST(clipping_win_dc_intersect_subset) {
    NEED_WIN(); HWND w = mkwin(WS_POPUP | WS_VISIBLE); if (!w) { t_skip("no window"); return; }
    HDC dc = U(GetDC)(w);
    RECT r;
    int ret = U(IntersectClipRect)(dc, 10, 10, 50, 40);
    CHECK(ret != 0);
    int t = U(GetClipBox)(dc, &r);
    CHECK(t == NULLREGION || (r.left >= 10 && r.top >= 10 && r.right <= 50 && r.bottom <= 40));
    t_note("window DC intersect ret=%d box (%ld,%ld,%ld,%ld)", ret, r.left, r.top, r.right, r.bottom);
    CHECK(U(GetClipRgn)(dc, NULL) == -1);
    U(ReleaseDC)(w, dc); U(DestroyWindow)(w);
}
TEST(clipping_win_dc_clip_reset_after_release) {
    NEED_WIN(); HWND w = mkwin(WS_POPUP | WS_VISIBLE); if (!w) { t_skip("no window"); return; }
    HDC dc = U(GetDC)(w); U(IntersectClipRect)(dc, 10, 10, 50, 40);
    U(ReleaseDC)(w, dc);
    dc = U(GetDC)(w);
    EXPECT_EQ(U(GetClipRgn)(dc, NULL), 0);
    U(ReleaseDC)(w, dc); U(DestroyWindow)(w);
}
TEST(clipping_win_windowdc_includes_nonclient) {
    NEED_WIN(); HWND w = mkwin(WS_OVERLAPPEDWINDOW | WS_VISIBLE); if (!w) { t_skip("no window"); return; }
    HDC cdc = U(GetDC)(w), wdc = U(GetWindowDC)(w);
    RECT a, b; U(GetClipBox)(cdc, &a); U(GetClipBox)(wdc, &b);
    t_note("client box (%ld,%ld,%ld,%ld) window box (%ld,%ld,%ld,%ld)", a.left, a.top, a.right, a.bottom, b.left, b.top, b.right, b.bottom);
    CHECK((b.right - b.left) >= (a.right - a.left) || (b.right == b.left));
    U(ReleaseDC)(w, cdc); U(ReleaseDC)(w, wdc); U(DestroyWindow)(w);
}
TEST(clipping_win_getdcex_intersectrgn) {
    NEED_WIN(); REQUIRE(U(GetDCEx)); HWND w = mkwin(WS_POPUP | WS_VISIBLE); if (!w) { t_skip("no window"); return; }
    HRGN s = rr(10, 10, 40, 40);
    HDC dc = U(GetDCEx)(w, s, DCX_INTERSECTRGN | DCX_CACHE);
    CHECK(dc != NULL);
    if (!dc) { U(DeleteObject)(s); U(DestroyWindow)(w); return; }
    RECT r; int t = U(GetClipBox)(dc, &r);
    t_note("GetDCEx intersect: type=%d (%ld,%ld,%ld,%ld)", t, r.left, r.top, r.right, r.bottom);
    CHECK(t == NULLREGION || (r.left >= 10 && r.top >= 10 && r.right <= 40 && r.bottom <= 40));
    CHECK(pv(dc, 5, 5) == 0); CHECK(pv(dc, 100, 100) == 0);
    U(ReleaseDC)(w, dc);            /* the region is owned by the system after a successful GetDCEx */
    U(DestroyWindow)(w);
}
TEST(clipping_win_getdcex_excludergn) {
    NEED_WIN(); REQUIRE(U(GetDCEx)); HWND w = mkwin(WS_POPUP | WS_VISIBLE); if (!w) { t_skip("no window"); return; }
    HRGN s = rr(10, 10, 40, 40);
    HDC dc = U(GetDCEx)(w, s, DCX_EXCLUDERGN | DCX_CACHE);
    CHECK(dc != NULL);
    if (!dc) { U(DeleteObject)(s); U(DestroyWindow)(w); return; }
    CHECK(pv(dc, 20, 20) == 0);
    U(ReleaseDC)(w, dc); U(DestroyWindow)(w);
}
TEST(clipping_win_getdcex_flags_no_crash) {
    NEED_WIN(); REQUIRE(U(GetDCEx)); HWND w = mkwin(WS_POPUP | WS_VISIBLE); if (!w) { t_skip("no window"); return; }
    static const DWORD fl[] = { DCX_CACHE, DCX_WINDOW | DCX_CACHE, DCX_CLIPCHILDREN | DCX_CACHE, DCX_CLIPSIBLINGS | DCX_CACHE,
                                DCX_PARENTCLIP | DCX_CACHE, DCX_NORESETATTRS | DCX_CACHE, DCX_LOCKWINDOWUPDATE | DCX_CACHE };
    for (unsigned i = 0; i < sizeof fl / sizeof fl[0]; i++) {
        HDC dc = U(GetDCEx)(w, NULL, fl[i]);
        CHECKF(dc != NULL, "flags %lx", (unsigned long)fl[i]);
        if (dc) { RECT r; int t = U(GetClipBox)(dc, &r); CHECKF(t != 0, "flags %lx", (unsigned long)fl[i]); U(ReleaseDC)(w, dc); }
    }
    U(DestroyWindow)(w);
}
TEST(clipping_win_clipchildren) {
    NEED_WIN(); REQUIRE(U(GetDCEx)); HWND p = mkwin(WS_POPUP | WS_VISIBLE); if (!p) { t_skip("no window"); return; }
    HWND c = t_make_window(NULL, WS_CHILD | WS_VISIBLE, 0, 20, 20, 60, 60, p); t_pump();
    if (!c) { t_skip("no child"); U(DestroyWindow)(p); return; }
    HDC a = U(GetDCEx)(p, NULL, DCX_CACHE | DCX_CLIPCHILDREN), b = U(GetDCEx)(p, NULL, DCX_CACHE);
    CHECK(pv(a, 50, 50) == 0);                       /* under the child: excluded */
    t_note("parent DC without CLIPCHILDREN sees child area: %d", pv(b, 50, 50));
    U(ReleaseDC)(p, a); U(ReleaseDC)(p, b);
    U(DestroyWindow)(p);
}
TEST(clipping_win_window_region) {
    NEED_WIN(); REQUIRE(U(SetWindowRgn)); REQUIRE(U(CreateEllipticRgn)); HWND w = mkwin(WS_POPUP | WS_VISIBLE); if (!w) { t_skip("no window"); return; }
    HRGN e = U(CreateEllipticRgn)(0, 0, 200, 150);
    CHECK(U(SetWindowRgn)(w, e, TRUE) != 0);          /* region owned by the system now */
    t_pump();
    HDC dc = U(GetDC)(w);
    CHECK(pv(dc, 1, 1) == 0); CHECK(pv(dc, 198, 148) == 0);
    t_note("window region centre visible: %d", pv(dc, 100, 75));
    RECT r; int t = U(GetClipBox)(dc, &r);
    CHECK(t == NULLREGION || (r.right - r.left <= 200 && r.bottom - r.top <= 150));
    U(ReleaseDC)(w, dc); U(DestroyWindow)(w);
}
TEST(clipping_win_selectclip_on_window_dc) {
    NEED_WIN(); HWND w = mkwin(WS_POPUP | WS_VISIBLE); if (!w) { t_skip("no window"); return; }
    HDC dc = U(GetDC)(w); HRGN s = rr(20, 20, 60, 60);
    int ret = U(SelectClipRgn)(dc, s);
    CHECK(ret == SIMPLEREGION || ret == NULLREGION || ret == COMPLEXREGION);
    HRGN g = rr(0, 0, 0, 0);
    EXPECT_EQ(U(GetClipRgn)(dc, g), 1);
    RECT r; rgn_box(g, &r);
    t_note("window DC clip rgn box (%ld,%ld,%ld,%ld)", r.left, r.top, r.right, r.bottom);
    EXPECT_EQ(U(SelectClipRgn)(dc, NULL) != 0, 1);
    EXPECT_EQ(U(GetClipRgn)(dc, g), 0);
    U(DeleteObject)(g); U(DeleteObject)(s); U(ReleaseDC)(w, dc); U(DestroyWindow)(w);
}
TEST(clipping_win_no_gdi_leak) {
    NEED_WIN(); REQUIRE(U(GetGuiResources)); HWND w = mkwin(WS_POPUP | WS_VISIBLE); if (!w) { t_skip("no window"); return; }
    DWORD before = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    for (int i = 0; i < 200; i++) {
        HDC dc = U(GetDC)(w); HRGN s = rr(0, 0, 50, 50);
        U(SelectClipRgn)(dc, s); U(IntersectClipRect)(dc, 5, 5, 40, 40); U(ExcludeClipRect)(dc, 10, 10, 20, 20);
        U(DeleteObject)(s); U(ReleaseDC)(w, dc);
    }
    DWORD after = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    CHECKF(after <= before + 1, "before %lu after %lu", before, after);
    U(DestroyWindow)(w);
}

/* ---- threads ----------------------------------------------------------- */
typedef struct { HDC dc; HRGN rgn; int ret1, ret2; RECT box; } TD;
static DWORD WINAPI thr_clip(LPVOID p) {
    TD *d = p;
    d->ret1 = U(IntersectClipRect)(d->dc, 8, 8, 24, 24);
    d->ret2 = U(SelectClipRgn)(d->dc, d->rgn);
    U(GetClipBox)(d->dc, &d->box);
    return 0;
}
TEST(clipping_thread_dc_used_in_other_thread) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    TD d = { m.dc, rr(30, 30, 40, 40), 0, 0, {0} };
    HANDLE h = CreateThread(NULL, 0, thr_clip, &d, 0, NULL);
    CHECK(h != NULL); if (!h) { mdc_destroy(&m); return; }
    WaitForSingleObject(h, 10000); CloseHandle(h);
    t_note("cross-thread: intersect=%d select=%d", d.ret1, d.ret2);
    CHECK(d.ret1 == SIMPLEREGION);
    CHECK(d.ret2 == SIMPLEREGION);
    EXPECT_RECT(d.box, 30, 30, 40, 40);
    RECT r; U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 30, 30, 40, 40);
    U(DeleteObject)(d.rgn); mdc_destroy(&m);
}
static DWORD WINAPI thr_mkrgn(LPVOID p) { *(HRGN *)p = rr(3, 4, 13, 14); return 0; }
TEST(clipping_thread_region_outlives_thread) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN r = NULL; HANDLE h = CreateThread(NULL, 0, thr_mkrgn, &r, 0, NULL);
    WaitForSingleObject(h, 10000); CloseHandle(h);
    CHECK(r != NULL);
    if (r) {
        EXPECT_EQ(U(SelectClipRgn)(m.dc, r), SIMPLEREGION);
        RECT b; U(GetClipBox)(m.dc, &b); EXPECT_RECT(b, 3, 4, 13, 14);
        CHECK(U(DeleteObject)(r));
    }
    mdc_destroy(&m);
}
static DWORD WINAPI thr_getbox(LPVOID p) { TD *d = p; d->ret1 = U(GetClipBox)(d->dc, &d->box); d->ret2 = U(GetClipRgn)(d->dc, d->rgn); return 0; }
TEST(clipping_thread_read_clip_in_other_thread) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    U(IntersectClipRect)(m.dc, 5, 6, 25, 26);
    TD d = { m.dc, rr(0, 0, 0, 0), 0, 0, {0} };
    HANDLE h = CreateThread(NULL, 0, thr_getbox, &d, 0, NULL);
    WaitForSingleObject(h, 10000); CloseHandle(h);
    CHECK(d.ret1 == SIMPLEREGION); EXPECT_RECT(d.box, 5, 6, 25, 26); CHECK(d.ret2 == 1);
    RECT b; rgn_box(d.rgn, &b); EXPECT_RECT(b, 5, 6, 25, 26);
    U(DeleteObject)(d.rgn); mdc_destroy(&m);
}
TEST(clipping_thread_window_dc_other_thread_note) {
    NEED_WIN(); HWND w = mkwin(WS_POPUP | WS_VISIBLE); if (!w) { t_skip("no window"); return; }
    HDC dc = U(GetDC)(w);
    TD d = { dc, rr(1, 1, 5, 5), 0, 0, {0} };
    HANDLE h = CreateThread(NULL, 0, thr_getbox, &d, 0, NULL);
    WaitForSingleObject(h, 10000); CloseHandle(h);
    t_note("window DC read from foreign thread: boxtype=%d", d.ret1);
    U(DeleteObject)(d.rgn); U(ReleaseDC)(w, dc); U(DestroyWindow)(w);
}
