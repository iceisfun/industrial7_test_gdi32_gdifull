#include "clip.h"

/* ---- initial clip state ------------------------------------------------ */
TEST(clipping_initial_default_memdc) {
    NEED_CLIP();
    HDC dc = U(CreateCompatibleDC)(NULL); CHECK(dc != NULL); if (!dc) return;
    RECT r;
    int ret = U(GetClipBox)(dc, &r);
    CHECKF(ret == SIMPLEREGION, "ret %d", ret);
    EXPECT_RECT(r, 0, 0, 1, 1);            /* default 1x1 monochrome bitmap */
    HRGN h = rr(5, 5, 6, 6);
    EXPECT_EQ(U(GetClipRgn)(dc, h), 0);
    U(DeleteObject)(h);
    U(DeleteDC)(dc);
}

#define SIZE_TEST(nm, W, H) TEST(clipping_initial_dib_##nm) { \
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, W, H)); if (!m.dc) return; \
    RECT r; int ret = U(GetClipBox)(m.dc, &r); \
    CHECKF(ret == SIMPLEREGION, "ret %d", ret); EXPECT_RECT(r, 0, 0, W, H); \
    HRGN h = rr(0, 0, 0, 0); EXPECT_EQ(U(GetClipRgn)(m.dc, h), 0); U(DeleteObject)(h); \
    mdc_destroy(&m); }
SIZE_TEST(1x1, 1, 1)
SIZE_TEST(2x3, 2, 3)
SIZE_TEST(16x16, 16, 16)
SIZE_TEST(100x50, 100, 50)
SIZE_TEST(257x1, 257, 1)
SIZE_TEST(1x300, 1, 300)
SIZE_TEST(640x480, 640, 480)
SIZE_TEST(31x33, 31, 33)

TEST(clipping_initial_after_bitmap_swap) {
    NEED_CLIP();
    MDC a, b; CHECK(mdc_create(&a, 40, 30)); CHECK(mdc_create(&b, 20, 10)); if (!a.dc || !b.dc) return;
    RECT r;
    /* select b's bitmap into a's DC: clip box follows the new surface */
    U(SelectObject)(b.dc, b.old);
    U(SelectObject)(a.dc, b.bmp);
    int ret = U(GetClipBox)(a.dc, &r);
    CHECK(ret == SIMPLEREGION);
    EXPECT_RECT(r, 0, 0, 20, 10);
    U(SelectObject)(a.dc, a.bmp);
    U(GetClipBox)(a.dc, &r);
    EXPECT_RECT(r, 0, 0, 40, 30);
    U(SelectObject)(b.dc, b.bmp);
    mdc_destroy(&a); mdc_destroy(&b);
}

TEST(clipping_getclipbox_invalid_dc) {
    NEED_CLIP();
    RECT r;
    SET_ERR(0); memset(&r, 0x55, sizeof r);
    EXPECT_EQ(U(GetClipBox)(NULL, &r), 0);
    SET_ERR(0);
    EXPECT_EQ(U(GetClipBox)((HDC)(ULONG_PTR)0x12345, &r), 0);
    HRGN h = rr(0, 0, 4, 4);
    SET_ERR(0);
    EXPECT_EQ(U(GetClipBox)((HDC)h, &r), 0);          /* region handle used as DC */
    U(DeleteObject)(h);
    HBRUSH b = U(CreateSolidBrush) ? U(CreateSolidBrush)(0) : NULL;
    if (b) { EXPECT_EQ(U(GetClipBox)((HDC)b, &r), 0); U(DeleteObject)(b); }
}
TEST(clipping_getclipbox_deleted_dc) {
    NEED_CLIP();
    HDC dc = U(CreateCompatibleDC)(NULL); RECT r;
    U(DeleteDC)(dc);
    EXPECT_EQ(U(GetClipBox)(dc, &r), 0);
    EXPECT_EQ(U(GetClipRgn)(dc, NULL), -1);
    HRGN h = rr(0, 0, 4, 4);
    EXPECT_EQ(U(SelectClipRgn)(dc, h), 0);
    EXPECT_EQ(U(IntersectClipRect)(dc, 0, 0, 1, 1), 0);
    EXPECT_EQ(U(ExcludeClipRect)(dc, 0, 0, 1, 1), 0);
    U(DeleteObject)(h);
}

/* ---- GetClipRgn -------------------------------------------------------- */
TEST(clipping_getcliprgn_null_dc) {
    NEED_CLIP();
    EXPECT_EQ(U(GetClipRgn)(NULL, NULL), -1);
    HRGN h = rr(1, 2, 3, 4);
    EXPECT_EQ(U(GetClipRgn)(NULL, h), -1);
    RECT r; rgn_box(h, &r); EXPECT_RECT(r, 1, 2, 3, 4);   /* untouched */
    U(DeleteObject)(h);
}
TEST(clipping_getcliprgn_null_rgn_no_clip) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 16, 16)); if (!m.dc) return;
    EXPECT_EQ(U(GetClipRgn)(m.dc, NULL), 0);
    mdc_destroy(&m);
}
TEST(clipping_getcliprgn_null_rgn_with_clip) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 16, 16)); if (!m.dc) return;
    HRGN h = rr(2, 2, 8, 8);
    U(SelectClipRgn)(m.dc, h);
    EXPECT_EQ(U(GetClipRgn)(m.dc, NULL), -1);
    U(DeleteObject)(h);
    mdc_destroy(&m);
}
TEST(clipping_getcliprgn_no_clip_leaves_region) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 16, 16)); if (!m.dc) return;
    HRGN h = rr(1, 2, 3, 4);
    EXPECT_EQ(U(GetClipRgn)(m.dc, h), 0);
    RECT r; rgn_box(h, &r); EXPECT_RECT(r, 1, 2, 3, 4);
    U(DeleteObject)(h);
    mdc_destroy(&m);
}
TEST(clipping_getcliprgn_roundtrip_rect) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN set = rr(10, 12, 30, 40), got = rr(0, 0, 0, 0);
    U(SelectClipRgn)(m.dc, set);
    EXPECT_EQ(U(GetClipRgn)(m.dc, got), 1);
    RECT r; int t = rgn_box(got, &r);
    CHECK(t == SIMPLEREGION); EXPECT_RECT(r, 10, 12, 30, 40);
    if (U(EqualRgn)) CHECK(U(EqualRgn)(set, got));
    U(DeleteObject)(set); U(DeleteObject)(got);
    mdc_destroy(&m);
}
TEST(clipping_getcliprgn_overwrites_target) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN set = rr(10, 10, 20, 20), got = rr(40, 40, 60, 60);
    U(SelectClipRgn)(m.dc, set);
    EXPECT_EQ(U(GetClipRgn)(m.dc, got), 1);
    RECT r; rgn_box(got, &r); EXPECT_RECT(r, 10, 10, 20, 20);
    U(DeleteObject)(set); U(DeleteObject)(got);
    mdc_destroy(&m);
}
TEST(clipping_getcliprgn_empty_clip_is_1) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN set = rr(100, 100, 100, 100), got = rr(1, 1, 5, 5);
    EXPECT_EQ(U(SelectClipRgn)(m.dc, set), NULLREGION);
    EXPECT_EQ(U(GetClipRgn)(m.dc, got), 1);
    RECT r; int t = rgn_box(got, &r); CHECK(t == NULLREGION);
    U(DeleteObject)(set); U(DeleteObject)(got);
    mdc_destroy(&m);
}
TEST(clipping_getcliprgn_wrong_object_type) {
    NEED_CLIP(); REQUIRE(U(CreateSolidBrush));
    MDC m; CHECK(mdc_create(&m, 16, 16)); if (!m.dc) return;
    HRGN set = rr(2, 2, 8, 8); U(SelectClipRgn)(m.dc, set);
    HBRUSH b = U(CreateSolidBrush)(0x112233);
    int ret = U(GetClipRgn)(m.dc, (HRGN)b);
    t_note("GetClipRgn(clip set, brush) -> %d", ret);
    CHECK(ret != 1);
    ret = U(GetClipRgn)(m.dc, (HRGN)(ULONG_PTR)0x4242);
    t_note("GetClipRgn(clip set, bogus) -> %d", ret);
    CHECK(ret != 1);
    U(DeleteObject)(b); U(DeleteObject)(set);
    mdc_destroy(&m);
}

/* ---- SelectClipRgn ----------------------------------------------------- */
TEST(clipping_selectclip_return_codes) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    HRGN e = rr(0, 0, 0, 0), s = rr(10, 10, 20, 20);
    HRGN c = rr(0, 0, 10, 10), c2 = rr(20, 20, 30, 30);
    U(CombineRgn)(c, c, c2, RGN_OR);
    EXPECT_EQ(U(SelectClipRgn)(m.dc, e), NULLREGION);
    EXPECT_EQ(U(SelectClipRgn)(m.dc, s), SIMPLEREGION);
    EXPECT_EQ(U(SelectClipRgn)(m.dc, c), COMPLEXREGION);
    EXPECT_EQ(U(SelectClipRgn)(m.dc, NULL), SIMPLEREGION);
    U(DeleteObject)(e); U(DeleteObject)(s); U(DeleteObject)(c); U(DeleteObject)(c2);
    mdc_destroy(&m);
}
TEST(clipping_selectclip_null_resets) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 50, 40)); if (!m.dc) return;
    HRGN s = rr(5, 5, 10, 10); RECT r;
    U(SelectClipRgn)(m.dc, s);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 5, 5, 10, 10);
    U(SelectClipRgn)(m.dc, NULL);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 0, 0, 50, 40);
    EXPECT_EQ(U(GetClipRgn)(m.dc, NULL), 0);
    U(DeleteObject)(s);
    mdc_destroy(&m);
}
TEST(clipping_selectclip_null_twice) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 50, 40)); if (!m.dc) return;
    EXPECT_EQ(U(SelectClipRgn)(m.dc, NULL), SIMPLEREGION);
    EXPECT_EQ(U(SelectClipRgn)(m.dc, NULL), SIMPLEREGION);
    mdc_destroy(&m);
}
TEST(clipping_selectclip_region_copied) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN s = rr(10, 10, 20, 20); RECT r;
    U(SelectClipRgn)(m.dc, s);
    U(SetRectRgn)(s, 0, 0, 60, 60);               /* change the original */
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 10, 10, 20, 20);
    U(DeleteObject)(s);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 10, 10, 20, 20);
    mdc_destroy(&m);
}
TEST(clipping_selectclip_survives_region_delete_draw) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 32, 32)); if (!m.dc) return;
    HRGN s = rr(4, 4, 12, 12);
    U(SelectClipRgn)(m.dc, s);
    CHECK(U(DeleteObject)(s));
    CHECK(U(PatBlt)(m.dc, 0, 0, 32, 32, WHITENESS));
    RECT cr = {4, 4, 12, 12};
    EXPECT_EQ(mdc_inside(&m, &cr), 64);
    EXPECT_EQ(mdc_outside(&m, &cr), 0);
    mdc_destroy(&m);
}
TEST(clipping_selectclip_same_region_twice) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 32, 32)); if (!m.dc) return;
    HRGN s = rr(4, 4, 12, 12);
    EXPECT_EQ(U(SelectClipRgn)(m.dc, s), SIMPLEREGION);
    EXPECT_EQ(U(SelectClipRgn)(m.dc, s), SIMPLEREGION);
    U(DeleteObject)(s);
    mdc_destroy(&m);
}
TEST(clipping_selectclip_invalid_dc) {
    NEED_CLIP(); HRGN s = rr(0, 0, 4, 4);
    SET_ERR(0);
    EXPECT_EQ(U(SelectClipRgn)(NULL, s), 0);
    t_note("SelectClipRgn(NULL dc) lasterror %lu", GetLastError());
    EXPECT_EQ(U(SelectClipRgn)((HDC)(ULONG_PTR)0x1234, s), 0);
    EXPECT_EQ(U(SelectClipRgn)(NULL, NULL), 0);
    EXPECT_EQ(U(SelectClipRgn)((HDC)s, s), 0);
    U(DeleteObject)(s);
}
TEST(clipping_selectclip_invalid_region) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 32, 32)); if (!m.dc) return;
    SET_ERR(0);
    int ret = U(SelectClipRgn)(m.dc, (HRGN)(ULONG_PTR)0x4444);
    EXPECT_EQ(ret, 0);
    t_note("SelectClipRgn(bogus rgn) lasterror %lu", GetLastError());
    RECT r; U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 0, 0, 32, 32);
    mdc_destroy(&m);
}
TEST(clipping_selectclip_wrong_object_type) {
    NEED_CLIP(); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(CreatePen));
    MDC m; CHECK(mdc_create(&m, 32, 32)); if (!m.dc) return;
    HBRUSH b = U(CreateSolidBrush)(0); HPEN p = U(CreatePen)(PS_SOLID, 1, 0);
    EXPECT_EQ(U(SelectClipRgn)(m.dc, (HRGN)b), 0);
    EXPECT_EQ(U(SelectClipRgn)(m.dc, (HRGN)p), 0);
    EXPECT_EQ(U(SelectClipRgn)(m.dc, (HRGN)m.bmp), 0);
    EXPECT_EQ(U(SelectClipRgn)(m.dc, (HRGN)m.dc), 0);
    RECT r; U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 0, 0, 32, 32);
    U(DeleteObject)(b); U(DeleteObject)(p);
    mdc_destroy(&m);
}
TEST(clipping_selectclip_deleted_region) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 32, 32)); if (!m.dc) return;
    HRGN s = rr(1, 1, 5, 5); U(DeleteObject)(s);
    EXPECT_EQ(U(SelectClipRgn)(m.dc, s), 0);
    mdc_destroy(&m);
}
TEST(clipping_selectclip_region_outside_surface) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 32, 32)); if (!m.dc) return;
    HRGN s = rr(100, 100, 200, 200); RECT r;
    int ret = U(SelectClipRgn)(m.dc, s);
    CHECKF(ret == NULLREGION || ret == SIMPLEREGION, "ret %d", ret);
    int t = U(GetClipBox)(m.dc, &r); t_note("outside region: ret=%d boxtype=%d", ret, t);
    CHECK(t == NULLREGION || (r.left >= 0 && r.top >= 0));
    U(DeleteObject)(s);
    HRGN n = rr(-50, -50, -10, -10);
    ret = U(SelectClipRgn)(m.dc, n);
    CHECKF(ret == NULLREGION || ret == SIMPLEREGION, "ret %d", ret);
    U(DeleteObject)(n);
    mdc_destroy(&m);
}
TEST(clipping_selectclip_region_larger_than_surface) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 32, 32)); if (!m.dc) return;
    HRGN s = rr(-100, -100, 200, 200); RECT r;
    int ret = U(SelectClipRgn)(m.dc, s);
    CHECK(ret == SIMPLEREGION);
    U(GetClipBox)(m.dc, &r);
    t_note("clip box of oversize region on 32x32 DIB: (%ld,%ld,%ld,%ld)", r.left, r.top, r.right, r.bottom);
    CHECK(r.left <= 0 && r.top <= 0 && r.right >= 32 && r.bottom >= 32);
    HRGN g = rr(0, 0, 0, 0); U(GetClipRgn)(m.dc, g); rgn_box(g, &r);
    t_note("GetClipRgn box: (%ld,%ld,%ld,%ld)", r.left, r.top, r.right, r.bottom);
    U(DeleteObject)(g); U(DeleteObject)(s);
    mdc_destroy(&m);
}
TEST(clipping_selectclip_partial_overlap_box) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 32, 32)); if (!m.dc) return;
    HRGN s = rr(20, 20, 50, 50); RECT r;
    EXPECT_EQ(U(SelectClipRgn)(m.dc, s), SIMPLEREGION);
    U(GetClipBox)(m.dc, &r);
    EXPECT_RECT(r, 20, 20, 32, 32);               /* clipped to the surface */
    U(DeleteObject)(s);
    mdc_destroy(&m);
}
TEST(clipping_selectclip_many_regions_sequence) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 128, 128)); if (!m.dc) return;
    for (int i = 0; i < 50; i++) {
        HRGN s = rr(i, i * 2, i + 20, i * 2 + 10); RECT r;
        int ret = U(SelectClipRgn)(m.dc, s);
        CHECKF(ret == SIMPLEREGION, "i=%d ret=%d", i, ret);
        U(GetClipBox)(m.dc, &r);
        CHECKF(rect_is(&r, i, i * 2, i + 20, i * 2 + 10), "i=%d", i);
        U(DeleteObject)(s);
    }
    mdc_destroy(&m);
}
TEST(clipping_selectclip_region_reuse_after_modify) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN s = rr(0, 0, 1, 1); RECT r;
    for (int i = 1; i <= 20; i++) {
        U(SetRectRgn)(s, 0, 0, i, i);
        U(SelectClipRgn)(m.dc, s);
        U(GetClipBox)(m.dc, &r);
        CHECKF(rect_is(&r, 0, 0, i, i), "i=%d", i);
    }
    U(DeleteObject)(s);
    mdc_destroy(&m);
}

/* ---- Intersect / Exclude ----------------------------------------------- */
TEST(clipping_intersect_return_codes) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    EXPECT_EQ(U(IntersectClipRect)(m.dc, 10, 10, 50, 50), SIMPLEREGION);
    EXPECT_EQ(U(IntersectClipRect)(m.dc, 20, 20, 30, 30), SIMPLEREGION);
    EXPECT_EQ(U(IntersectClipRect)(m.dc, 60, 60, 70, 70), NULLREGION);
    RECT r; int t = U(GetClipBox)(m.dc, &r); CHECK(t == NULLREGION);
    mdc_destroy(&m);
}
TEST(clipping_intersect_box_progress) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    RECT r;
    U(IntersectClipRect)(m.dc, 10, 10, 60, 60); U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 10, 10, 60, 60);
    U(IntersectClipRect)(m.dc, 30, 0, 90, 40);  U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 30, 10, 60, 40);
    U(IntersectClipRect)(m.dc, 0, 0, 100, 100); U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 30, 10, 60, 40);
    mdc_destroy(&m);
}
TEST(clipping_intersect_whole_surface_is_simple) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    EXPECT_EQ(U(IntersectClipRect)(m.dc, 0, 0, 100, 100), SIMPLEREGION);
    EXPECT_EQ(U(IntersectClipRect)(m.dc, -1000, -1000, 1000, 1000), SIMPLEREGION);
    mdc_destroy(&m);
}
TEST(clipping_intersect_empty_rect) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    EXPECT_EQ(U(IntersectClipRect)(m.dc, 10, 10, 10, 50), NULLREGION);
    EXPECT_EQ(U(SelectClipRgn)(m.dc, NULL), SIMPLEREGION);
    EXPECT_EQ(U(IntersectClipRect)(m.dc, 10, 10, 50, 10), NULLREGION);
    EXPECT_EQ(U(SelectClipRgn)(m.dc, NULL), SIMPLEREGION);
    EXPECT_EQ(U(IntersectClipRect)(m.dc, 0, 0, 0, 0), NULLREGION);
    mdc_destroy(&m);
}
TEST(clipping_intersect_inverted_rect) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    RECT r;
    int ret = U(IntersectClipRect)(m.dc, 50, 50, 10, 10);
    int t = U(GetClipBox)(m.dc, &r);
    t_note("IntersectClipRect(50,50,10,10): ret=%d box type=%d (%ld,%ld,%ld,%ld)", ret, t, r.left, r.top, r.right, r.bottom);
    CHECK(ret == NULLREGION || ret == SIMPLEREGION);
    U(SelectClipRgn)(m.dc, NULL);
    ret = U(IntersectClipRect)(m.dc, 50, 10, 10, 50);
    t_note("IntersectClipRect(50,10,10,50): ret=%d", ret);
    CHECK(ret != 0);
    mdc_destroy(&m);
}
TEST(clipping_intersect_negative_coords) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    RECT r;
    EXPECT_EQ(U(IntersectClipRect)(m.dc, -20, -20, 30, 30), SIMPLEREGION);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 0, 0, 30, 30);
    U(SelectClipRgn)(m.dc, NULL);
    { int ret = U(IntersectClipRect)(m.dc, -20, -20, -5, -5); t_note("Intersect outside surface ret=%d", ret); CHECK(ret == NULLREGION || ret == SIMPLEREGION); }
    mdc_destroy(&m);
}
TEST(clipping_intersect_extreme_coords) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    RECT r;
    int ret = U(IntersectClipRect)(m.dc, INT_MIN, INT_MIN, INT_MAX, INT_MAX);
    int t = U(GetClipBox)(m.dc, &r);
    t_note("Intersect(INT_MIN..INT_MAX): ret=%d type=%d (%ld,%ld,%ld,%ld)", ret, t, r.left, r.top, r.right, r.bottom);
    CHECK(ret == SIMPLEREGION || ret == NULLREGION || ret == 0);
    U(SelectClipRgn)(m.dc, NULL);
    ret = U(IntersectClipRect)(m.dc, 0, 0, INT_MAX, INT_MAX);
    U(GetClipBox)(m.dc, &r);
    t_note("Intersect(0,0,INT_MAX,INT_MAX): ret=%d (%ld,%ld,%ld,%ld)", ret, r.left, r.top, r.right, r.bottom);
    CHECK(ret != 0);
    CHECK(r.left == 0 && r.top == 0 && r.right <= 64 && r.bottom <= 64);
    U(SelectClipRgn)(m.dc, NULL);
    ret = U(IntersectClipRect)(m.dc, INT_MIN, INT_MIN, 10, 10);
    U(GetClipBox)(m.dc, &r);
    t_note("Intersect(INT_MIN,INT_MIN,10,10): ret=%d (%ld,%ld,%ld,%ld)", ret, r.left, r.top, r.right, r.bottom);
    U(SelectClipRgn)(m.dc, NULL);
    ret = U(IntersectClipRect)(m.dc, 0x7FFFFFF0, 0x7FFFFFF0, INT_MAX, INT_MAX);
    t_note("Intersect(near INT_MAX): ret=%d", ret);
    U(SelectClipRgn)(m.dc, NULL);
    ret = U(ExcludeClipRect)(m.dc, INT_MIN, INT_MIN, INT_MAX, INT_MAX);
    t_note("Exclude(INT_MIN..INT_MAX): ret=%d", ret);
    U(SelectClipRgn)(m.dc, NULL);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 0, 0, 64, 64);
    mdc_destroy(&m);
}
TEST(clipping_exclude_return_codes) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    EXPECT_EQ(U(ExcludeClipRect)(m.dc, 40, 40, 60, 60), COMPLEXREGION);    /* hole */
    U(SelectClipRgn)(m.dc, NULL);
    EXPECT_EQ(U(ExcludeClipRect)(m.dc, 0, 0, 100, 50), SIMPLEREGION);      /* half */
    EXPECT_EQ(U(ExcludeClipRect)(m.dc, 0, 0, 100, 100), NULLREGION);
    U(SelectClipRgn)(m.dc, NULL);
    EXPECT_EQ(U(ExcludeClipRect)(m.dc, -10, -10, 200, 200), NULLREGION);
    U(SelectClipRgn)(m.dc, NULL);
    EXPECT_EQ(U(ExcludeClipRect)(m.dc, 200, 200, 300, 300), SIMPLEREGION); /* outside: no change */
    mdc_destroy(&m);
}
TEST(clipping_exclude_box) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    RECT r;
    U(ExcludeClipRect)(m.dc, 0, 0, 100, 30); U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 0, 30, 100, 100);
    U(ExcludeClipRect)(m.dc, 0, 0, 30, 100); U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 30, 30, 100, 100);
    U(ExcludeClipRect)(m.dc, 40, 40, 50, 50); U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 30, 30, 100, 100);
    mdc_destroy(&m);
}
TEST(clipping_exclude_empty_rect_noop) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    RECT r;
    int ret = U(ExcludeClipRect)(m.dc, 10, 10, 10, 10);
    CHECKF(ret == SIMPLEREGION, "ret %d", ret);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 0, 0, 100, 100);
    ret = U(ExcludeClipRect)(m.dc, 10, 10, 50, 10);
    CHECKF(ret == SIMPLEREGION, "ret %d", ret);
    mdc_destroy(&m);
}
TEST(clipping_exclude_inverted_rect) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    RECT r;
    int ret = U(ExcludeClipRect)(m.dc, 60, 60, 40, 40);
    U(GetClipBox)(m.dc, &r);
    t_note("ExcludeClipRect inverted: ret=%d box (%ld,%ld,%ld,%ld)", ret, r.left, r.top, r.right, r.bottom);
    CHECK(ret != 0);
    mdc_destroy(&m);
}
TEST(clipping_exclude_creates_complex_pixels) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 32, 32)); if (!m.dc) return;
    U(ExcludeClipRect)(m.dc, 8, 8, 24, 24);
    U(PatBlt)(m.dc, 0, 0, 32, 32, WHITENESS);
    CHECK(mdc_count(&m, 0xFFFFFF) == 32 * 32 - 16 * 16);
    CHECK(mdc_px(&m, 7, 7) == 0xFFFFFF && mdc_px(&m, 8, 8) == 0 && mdc_px(&m, 23, 23) == 0 && mdc_px(&m, 24, 24) == 0xFFFFFF);
    mdc_destroy(&m);
}
TEST(clipping_intersect_then_exclude_same_rect_is_empty) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 32, 32)); if (!m.dc) return;
    U(IntersectClipRect)(m.dc, 4, 4, 20, 20);
    EXPECT_EQ(U(ExcludeClipRect)(m.dc, 4, 4, 20, 20), NULLREGION);
    mdc_destroy(&m);
}
TEST(clipping_intersect_exclude_invalid_dc) {
    NEED_CLIP();
    EXPECT_EQ(U(IntersectClipRect)(NULL, 0, 0, 1, 1), 0);
    EXPECT_EQ(U(ExcludeClipRect)(NULL, 0, 0, 1, 1), 0);
    EXPECT_EQ(U(IntersectClipRect)((HDC)(ULONG_PTR)0x777, 0, 0, 1, 1), 0);
    EXPECT_EQ(U(ExcludeClipRect)((HDC)(ULONG_PTR)0x777, 0, 0, 1, 1), 0);
}
TEST(clipping_intersect_after_select_region) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    HRGN s = rr(10, 10, 60, 60); RECT r;
    U(SelectClipRgn)(m.dc, s);
    EXPECT_EQ(U(IntersectClipRect)(m.dc, 30, 30, 90, 90), SIMPLEREGION);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 30, 30, 60, 60);
    U(DeleteObject)(s);
    mdc_destroy(&m);
}
TEST(clipping_getcliprgn_after_intersect) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    U(IntersectClipRect)(m.dc, 10, 20, 30, 40);
    HRGN g = rr(0, 0, 0, 0); RECT r;
    EXPECT_EQ(U(GetClipRgn)(m.dc, g), 1);
    rgn_box(g, &r); EXPECT_RECT(r, 10, 20, 30, 40);
    U(DeleteObject)(g);
    mdc_destroy(&m);
}
TEST(clipping_getcliprgn_after_exclude_is_complex) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    U(ExcludeClipRect)(m.dc, 10, 20, 30, 40);
    HRGN g = rr(0, 0, 0, 0); RECT r;
    EXPECT_EQ(U(GetClipRgn)(m.dc, g), 1);
    int t = rgn_box(g, &r); CHECK(t == COMPLEXREGION);
    CHECK(!U(PtInRegion)(g, 15, 25)); CHECK(U(PtInRegion)(g, 5, 25)); CHECK(U(PtInRegion)(g, 30, 40));
    U(DeleteObject)(g);
    mdc_destroy(&m);
}

/* ---- OffsetClipRgn ----------------------------------------------------- */
TEST(clipping_offset_basic) {
    NEED_CLIP(); REQUIRE(U(OffsetClipRgn)); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    HRGN s = rr(10, 10, 30, 30); RECT r;
    U(SelectClipRgn)(m.dc, s);
    EXPECT_EQ(U(OffsetClipRgn)(m.dc, 5, 7), SIMPLEREGION);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 15, 17, 35, 37);
    EXPECT_EQ(U(OffsetClipRgn)(m.dc, -15, -17), SIMPLEREGION);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 0, 0, 20, 20);
    U(DeleteObject)(s);
    mdc_destroy(&m);
}
TEST(clipping_offset_zero) {
    NEED_CLIP(); REQUIRE(U(OffsetClipRgn)); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    HRGN s = rr(10, 10, 30, 30); RECT r;
    U(SelectClipRgn)(m.dc, s);
    EXPECT_EQ(U(OffsetClipRgn)(m.dc, 0, 0), SIMPLEREGION);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 10, 10, 30, 30);
    U(DeleteObject)(s);
    mdc_destroy(&m);
}
TEST(clipping_offset_out_of_surface) {
    NEED_CLIP(); REQUIRE(U(OffsetClipRgn)); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    HRGN s = rr(10, 10, 30, 30); RECT r;
    U(SelectClipRgn)(m.dc, s);
    int ret = U(OffsetClipRgn)(m.dc, 200, 0);
    int t = U(GetClipBox)(m.dc, &r);
    t_note("offset out: ret=%d boxtype=%d", ret, t);
    CHECK(ret == NULLREGION || ret == SIMPLEREGION);
    t_note("offset back ret=%d", U(OffsetClipRgn)(m.dc, -200, 0));
    U(GetClipBox)(m.dc, &r);
    t_note("offset out and back: box (%ld,%ld,%ld,%ld)", r.left, r.top, r.right, r.bottom);
    U(DeleteObject)(s);
    mdc_destroy(&m);
}
TEST(clipping_offset_region_not_affected) {
    NEED_CLIP(); REQUIRE(U(OffsetClipRgn)); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    HRGN s = rr(10, 10, 30, 30); RECT r;
    U(SelectClipRgn)(m.dc, s);
    U(OffsetClipRgn)(m.dc, 20, 20);
    rgn_box(s, &r); EXPECT_RECT(r, 10, 10, 30, 30);
    U(DeleteObject)(s);
    mdc_destroy(&m);
}
TEST(clipping_offset_no_clip_region) {
    NEED_CLIP(); REQUIRE(U(OffsetClipRgn)); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    RECT r;
    int ret = U(OffsetClipRgn)(m.dc, 10, 10);
    int g = U(GetClipRgn)(m.dc, NULL);
    U(GetClipBox)(m.dc, &r);
    t_note("OffsetClipRgn without clip: ret=%d GetClipRgn=%d box (%ld,%ld,%ld,%ld)", ret, g, r.left, r.top, r.right, r.bottom);
    CHECK(ret == SIMPLEREGION || ret == NULLREGION || ret == COMPLEXREGION);
    mdc_destroy(&m);
}
TEST(clipping_offset_complex) {
    NEED_CLIP(); REQUIRE(U(OffsetClipRgn)); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    U(ExcludeClipRect)(m.dc, 40, 40, 60, 60);
    EXPECT_EQ(U(OffsetClipRgn)(m.dc, 1, 1), COMPLEXREGION);
    U(PatBlt)(m.dc, 0, 0, 100, 100, WHITENESS);
    CHECK(mdc_px(&m, 40, 40) == 0xFFFFFF || mdc_px(&m, 40, 40) == 0);   /* edge: see next checks */
    CHECK(mdc_px(&m, 50, 50) == 0 && mdc_px(&m, 60, 60) == 0 && mdc_px(&m, 61, 61) == 0xFFFFFF && mdc_px(&m, 41, 41) == 0);
    mdc_destroy(&m);
}
TEST(clipping_offset_invalid_dc) {
    NEED_CLIP(); REQUIRE(U(OffsetClipRgn));
    EXPECT_EQ(U(OffsetClipRgn)(NULL, 1, 1), 0);
    EXPECT_EQ(U(OffsetClipRgn)((HDC)(ULONG_PTR)0x999, 1, 1), 0);
}
TEST(clipping_offset_large_values) {
    NEED_CLIP(); REQUIRE(U(OffsetClipRgn)); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    HRGN s = rr(10, 10, 30, 30); U(SelectClipRgn)(m.dc, s);
    int ret = U(OffsetClipRgn)(m.dc, INT_MAX, INT_MAX);
    t_note("OffsetClipRgn(INT_MAX) ret=%d", ret);
    ret = U(OffsetClipRgn)(m.dc, INT_MIN, INT_MIN);
    t_note("OffsetClipRgn(INT_MIN) ret=%d", ret);
    U(DeleteObject)(s);
    mdc_destroy(&m);
}

/* ---- loops ------------------------------------------------------------- */
TEST(clipping_many_intersects_monotonic) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 512, 512)); if (!m.dc) return;
    RECT prev = {0, 0, 512, 512}, r;
    for (int i = 0; i < 200; i++) {
        U(IntersectClipRect)(m.dc, i, i / 2, 512 - i, 512 - i / 3);
        U(GetClipBox)(m.dc, &r);
        if (!(r.left >= prev.left && r.top >= prev.top && r.right <= prev.right && r.bottom <= prev.bottom)) {
            if (r.right > r.left && r.bottom > r.top) CHECKF(0, "grew at i=%d", i);
        }
        if (r.right > r.left && r.bottom > r.top) prev = r;
        else break;
    }
    mdc_destroy(&m);
}
TEST(clipping_many_excludes_stay_valid) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 256, 256)); if (!m.dc) return;
    int bad = 0;
    for (int i = 0; i < 100; i++) {
        int ret = U(ExcludeClipRect)(m.dc, (i * 7) % 250, (i * 13) % 250, (i * 7) % 250 + 3, (i * 13) % 250 + 3);
        if (ret == 0) bad++;
    }
    CHECK(bad == 0);
    RECT r; int t = U(GetClipBox)(m.dc, &r); CHECK(t == COMPLEXREGION || t == SIMPLEREGION);
    mdc_destroy(&m);
}
TEST(clipping_select_null_loop) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN s = rr(1, 1, 20, 20); int bad = 0;
    for (int i = 0; i < 1000; i++) {
        if (U(SelectClipRgn)(m.dc, s) != SIMPLEREGION) bad++;
        if (U(SelectClipRgn)(m.dc, NULL) != SIMPLEREGION) bad++;
    }
    CHECK(bad == 0);
    U(DeleteObject)(s);
    mdc_destroy(&m);
}
TEST(clipping_gdi_object_leak_selectclip) {
    NEED_CLIP(); REQUIRE(U(GetGuiResources));
    MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    DWORD before = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    for (int i = 0; i < 500; i++) {
        HRGN s = rr(0, 0, 10 + i % 10, 10);
        U(SelectClipRgn)(m.dc, s);
        U(DeleteObject)(s);
        U(IntersectClipRect)(m.dc, 0, 0, 20, 20);
        U(ExcludeClipRect)(m.dc, 2, 2, 4, 4);
    }
    DWORD after = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    CHECKF(after <= before + 1, "before %lu after %lu", before, after);
    mdc_destroy(&m);
}
