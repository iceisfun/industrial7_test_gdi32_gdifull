#include "clip.h"

#define NEED_MAP() do { NEED_CLIP(); REQUIRE(U(SetViewportOrgEx)); REQUIRE(U(SetWindowOrgEx)); REQUIRE(U(SetMapMode)); \
    REQUIRE(U(SetWindowExtEx)); REQUIRE(U(SetViewportExtEx)); REQUIRE(U(OffsetClipRgn)); } while (0)

static void dev_box(HDC dc, RECT *r) {      /* clip box in device units via GetClipRgn when a clip exists */
    HRGN g = rr(0, 0, 0, 0);
    int ret = U(GetClipRgn)(dc, g);
    if (ret == 1) rgn_box(g, r); else memset(r, 0, sizeof *r);
    U(DeleteObject)(g);
}

/* ---- viewport / window origin ----------------------------------------- */
TEST(clipping_map_vporg_selectclip_is_device) {
    NEED_MAP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    U(SetViewportOrgEx)(m.dc, 10, 20, NULL);
    HRGN s = rr(0, 0, 30, 30); U(SelectClipRgn)(m.dc, s);
    U(PatBlt)(m.dc, -100, -100, 400, 400, WHITENESS);
    RECT cr = {0, 0, 30, 30};
    EXPECT_EQ(mdc_inside(&m, &cr), 900);
    EXPECT_EQ(mdc_outside(&m, &cr), 0);
    U(DeleteObject)(s); mdc_destroy(&m);
}
TEST(clipping_map_vporg_clipbox_logical) {
    NEED_MAP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    HRGN s = rr(10, 20, 40, 60); RECT r;
    U(SelectClipRgn)(m.dc, s);
    U(SetViewportOrgEx)(m.dc, 5, 7, NULL);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 5, 13, 35, 53);
    U(SetViewportOrgEx)(m.dc, -5, -7, NULL);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 15, 27, 45, 67);
    U(DeleteObject)(s); mdc_destroy(&m);
}
TEST(clipping_map_windoworg_clipbox_logical) {
    NEED_MAP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    HRGN s = rr(10, 20, 40, 60); RECT r;
    U(SelectClipRgn)(m.dc, s);
    U(SetWindowOrgEx)(m.dc, 5, 7, NULL);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 15, 27, 45, 67);
    U(DeleteObject)(s); mdc_destroy(&m);
}
TEST(clipping_map_vporg_intersect_logical) {
    NEED_MAP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    RECT r;
    U(SetViewportOrgEx)(m.dc, 10, 20, NULL);
    EXPECT_EQ(U(IntersectClipRect)(m.dc, 0, 0, 30, 30), SIMPLEREGION);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 0, 0, 30, 30);
    dev_box(m.dc, &r);
    t_note("device clip rgn box (%ld,%ld,%ld,%ld) (expected 10,20,40,50)", r.left, r.top, r.right, r.bottom);
    U(SetViewportOrgEx)(m.dc, 0, 0, NULL);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 10, 20, 40, 50);
    mdc_destroy(&m);
}
TEST(clipping_map_vporg_intersect_pixels) {
    NEED_MAP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    U(SetViewportOrgEx)(m.dc, 10, 20, NULL);
    U(IntersectClipRect)(m.dc, 0, 0, 30, 30);
    U(SetViewportOrgEx)(m.dc, 0, 0, NULL);
    U(PatBlt)(m.dc, 0, 0, 100, 100, WHITENESS);
    RECT cr = {10, 20, 40, 50};
    EXPECT_EQ(mdc_inside(&m, &cr), 900);
    EXPECT_EQ(mdc_outside(&m, &cr), 0);
    mdc_destroy(&m);
}
TEST(clipping_map_vporg_exclude_logical) {
    NEED_MAP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    U(SetViewportOrgEx)(m.dc, 50, 50, NULL);
    EXPECT_EQ(U(ExcludeClipRect)(m.dc, -50, -50, 0, 50), SIMPLEREGION);   /* left half */
    U(SetViewportOrgEx)(m.dc, 0, 0, NULL);
    RECT r; U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 50, 0, 100, 100);
    mdc_destroy(&m);
}
TEST(clipping_map_vporg_offset_unaffected) {
    NEED_MAP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    HRGN s = rr(10, 10, 30, 30); RECT r;
    U(SelectClipRgn)(m.dc, s);
    U(SetViewportOrgEx)(m.dc, 40, 40, NULL);
    EXPECT_EQ(U(OffsetClipRgn)(m.dc, 5, 6), SIMPLEREGION);
    U(SetViewportOrgEx)(m.dc, 0, 0, NULL);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 15, 16, 35, 36);
    U(DeleteObject)(s); mdc_destroy(&m);
}
TEST(clipping_map_clip_stays_when_origin_changes) {
    NEED_MAP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    HRGN s = rr(10, 10, 30, 30); U(SelectClipRgn)(m.dc, s);
    U(SetViewportOrgEx)(m.dc, 50, 50, NULL);
    U(SetWindowOrgEx)(m.dc, -3, -3, NULL);
    U(PatBlt)(m.dc, -1000, -1000, 5000, 5000, WHITENESS);
    RECT cr = {10, 10, 30, 30};
    EXPECT_EQ(mdc_inside(&m, &cr), 400); EXPECT_EQ(mdc_outside(&m, &cr), 0);
    U(DeleteObject)(s); mdc_destroy(&m);
}
TEST(clipping_map_setmapmode_keeps_clip) {
    NEED_MAP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    HRGN s = rr(10, 10, 30, 30); U(SelectClipRgn)(m.dc, s);
    static const int modes[] = { MM_LOMETRIC, MM_HIMETRIC, MM_LOENGLISH, MM_HIENGLISH, MM_TWIPS, MM_ISOTROPIC, MM_ANISOTROPIC, MM_TEXT };
    for (unsigned i = 0; i < 8; i++) {
        U(SetMapMode)(m.dc, modes[i]);
        mdc_clear(&m, 0);
        U(SetViewportOrgEx)(m.dc, 0, 0, NULL);
        U(SetWindowOrgEx)(m.dc, 0, 0, NULL);
        /* PatBlt in logical space is huge enough to cover regardless of scale/flip */
        U(PatBlt)(m.dc, -100000, -100000, 200000, 200000, WHITENESS);
        RECT cr = {10, 10, 30, 30};
        CHECKF(mdc_inside(&m, &cr) == 400 && mdc_outside(&m, &cr) == 0, "mapmode %d in=%d out=%d", modes[i], mdc_inside(&m, &cr), mdc_outside(&m, &cr));
    }
    U(SetMapMode)(m.dc, MM_TEXT);
    U(DeleteObject)(s); mdc_destroy(&m);
}

/* ---- scaling ----------------------------------------------------------- */
static void set_scale(HDC dc, int wx, int wy, int vx, int vy) {
    U(SetMapMode)(dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(dc, wx, wy, NULL);
    U(SetViewportExtEx)(dc, vx, vy, NULL);
}
TEST(clipping_map_scale2_intersect) {
    NEED_MAP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    set_scale(m.dc, 1, 1, 2, 2); RECT r;
    EXPECT_EQ(U(IntersectClipRect)(m.dc, 5, 5, 15, 15), SIMPLEREGION);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 5, 5, 15, 15);
    U(SetMapMode)(m.dc, MM_TEXT);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 10, 10, 30, 30);
    mdc_destroy(&m);
}
TEST(clipping_map_scale_half_intersect) {
    NEED_MAP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    set_scale(m.dc, 2, 2, 1, 1); RECT r;
    EXPECT_EQ(U(IntersectClipRect)(m.dc, 20, 20, 100, 100), SIMPLEREGION);
    U(SetMapMode)(m.dc, MM_TEXT);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 10, 10, 50, 50);
    mdc_destroy(&m);
}
TEST(clipping_map_scale3_exclude) {
    NEED_MAP(); MDC m; CHECK(mdc_create(&m, 99, 99)); if (!m.dc) return;
    set_scale(m.dc, 1, 1, 3, 3); RECT r;
    U(ExcludeClipRect)(m.dc, 0, 0, 33, 11);           /* device 0..99 x 0..33 */
    U(SetMapMode)(m.dc, MM_TEXT);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 0, 33, 99, 99);
    mdc_destroy(&m);
}
TEST(clipping_map_scale_offset) {
    NEED_MAP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    HRGN s = rr(10, 10, 30, 30); U(SelectClipRgn)(m.dc, s);
    set_scale(m.dc, 1, 1, 2, 3);
    EXPECT_EQ(U(OffsetClipRgn)(m.dc, 5, 4), SIMPLEREGION);
    U(SetMapMode)(m.dc, MM_TEXT);
    RECT r; U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 20, 22, 40, 42);
    U(DeleteObject)(s); mdc_destroy(&m);
}
TEST(clipping_map_scale_selectclip_not_scaled) {
    NEED_MAP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    set_scale(m.dc, 1, 1, 4, 4);
    HRGN s = rr(8, 8, 40, 40); RECT r;
    U(SelectClipRgn)(m.dc, s);
    U(SetMapMode)(m.dc, MM_TEXT);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 8, 8, 40, 40);
    U(DeleteObject)(s); mdc_destroy(&m);
}
TEST(clipping_map_scale_clipbox_logical) {
    NEED_MAP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    HRGN s = rr(8, 8, 40, 40); RECT r;
    U(SelectClipRgn)(m.dc, s);
    set_scale(m.dc, 1, 1, 4, 4);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 2, 2, 10, 10);
    U(DeleteObject)(s); mdc_destroy(&m);
}
TEST(clipping_map_flip_y_intersect_size) {
    NEED_MAP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    set_scale(m.dc, 1, 1, 1, -1);
    U(SetViewportOrgEx)(m.dc, 0, 50, NULL);
    int ret = U(IntersectClipRect)(m.dc, 0, 0, 20, 20);
    CHECKF(ret == SIMPLEREGION, "ret %d", ret);
    U(SetMapMode)(m.dc, MM_TEXT); U(SetViewportOrgEx)(m.dc, 0, 0, NULL);
    RECT r; U(GetClipBox)(m.dc, &r);
    t_note("flipped-y intersect device box (%ld,%ld,%ld,%ld)", r.left, r.top, r.right, r.bottom);
    CHECK(r.right - r.left == 20 && r.bottom - r.top == 20);
    CHECK(r.left == 0 && r.bottom <= 51 && r.top >= 29);
    mdc_destroy(&m);
}
TEST(clipping_map_flip_x_intersect_size) {
    NEED_MAP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    set_scale(m.dc, 1, 1, -1, 1);
    U(SetViewportOrgEx)(m.dc, 60, 0, NULL);
    int ret = U(IntersectClipRect)(m.dc, 0, 0, 20, 20);
    CHECKF(ret == SIMPLEREGION, "ret %d", ret);
    U(SetMapMode)(m.dc, MM_TEXT); U(SetViewportOrgEx)(m.dc, 0, 0, NULL);
    RECT r; U(GetClipBox)(m.dc, &r);
    t_note("flipped-x intersect device box (%ld,%ld,%ld,%ld)", r.left, r.top, r.right, r.bottom);
    CHECK(r.right - r.left == 20 && r.bottom - r.top == 20);
    mdc_destroy(&m);
}
TEST(clipping_map_lometric_flips_y) {
    NEED_MAP(); MDC m; CHECK(mdc_create(&m, 400, 400)); if (!m.dc) return;
    U(SetMapMode)(m.dc, MM_LOMETRIC);
    U(SetViewportOrgEx)(m.dc, 0, 300, NULL);
    int ret = U(IntersectClipRect)(m.dc, 0, 0, 500, 500);     /* 50 mm square, y grows upward */
    t_note("LOMETRIC intersect ret=%d", ret);
    CHECK(ret == SIMPLEREGION || ret == NULLREGION);
    U(SetMapMode)(m.dc, MM_TEXT); U(SetViewportOrgEx)(m.dc, 0, 0, NULL);
    RECT r; U(GetClipBox)(m.dc, &r);
    t_note("LOMETRIC device box (%ld,%ld,%ld,%ld)", r.left, r.top, r.right, r.bottom);
    CHECK(r.left == 0 && r.bottom <= 301 && r.top < 300);
    mdc_destroy(&m);
}
TEST(clipping_map_modes_empty_rect_stays_empty) {
    NEED_MAP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    static const int modes[] = { MM_LOMETRIC, MM_HIMETRIC, MM_LOENGLISH, MM_HIENGLISH, MM_TWIPS };
    for (unsigned i = 0; i < 5; i++) {
        U(SelectClipRgn)(m.dc, NULL);
        U(SetMapMode)(m.dc, modes[i]);
        int ret = U(IntersectClipRect)(m.dc, 10, 10, 10, 10);
        CHECKF(ret == NULLREGION, "mode %d ret %d", modes[i], ret);
    }
    U(SetMapMode)(m.dc, MM_TEXT);
    mdc_destroy(&m);
}

/* ---- world transform --------------------------------------------------- */
#define NEED_XF() do { NEED_MAP(); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform)); REQUIRE(U(ModifyWorldTransform)); } while (0)
TEST(clipping_xform_scale_translate_intersect) {
    NEED_XF(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    CHECK(U(SetGraphicsMode)(m.dc, GM_ADVANCED) != 0);
    XFORM x = { 2.0f, 0, 0, 2.0f, 5.0f, 7.0f };
    CHECK(U(SetWorldTransform)(m.dc, &x));
    EXPECT_EQ(U(IntersectClipRect)(m.dc, 1, 1, 11, 11), SIMPLEREGION);
    U(ModifyWorldTransform)(m.dc, NULL, MWT_IDENTITY);
    RECT r; U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 7, 9, 27, 29);
    mdc_destroy(&m);
}
TEST(clipping_xform_clipbox_logical) {
    NEED_XF(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    HRGN s = rr(10, 20, 50, 60); U(SelectClipRgn)(m.dc, s);
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM x = { 2.0f, 0, 0, 2.0f, 0, 0 };
    U(SetWorldTransform)(m.dc, &x);
    RECT r; U(GetClipBox)(m.dc, &r);
    EXPECT_RECT(r, 5, 10, 25, 30);
    U(ModifyWorldTransform)(m.dc, NULL, MWT_IDENTITY);
    U(DeleteObject)(s); mdc_destroy(&m);
}
TEST(clipping_xform_selectclip_device) {
    NEED_XF(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM x = { 3.0f, 0, 0, 3.0f, 11.0f, 13.0f };
    U(SetWorldTransform)(m.dc, &x);
    HRGN s = rr(10, 10, 40, 40); U(SelectClipRgn)(m.dc, s);
    U(ModifyWorldTransform)(m.dc, NULL, MWT_IDENTITY);
    RECT r; U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 10, 10, 40, 40);
    U(DeleteObject)(s); mdc_destroy(&m);
}
TEST(clipping_xform_exclude) {
    NEED_XF(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM x = { 2.0f, 0, 0, 2.0f, 0, 0 };
    U(SetWorldTransform)(m.dc, &x);
    EXPECT_EQ(U(ExcludeClipRect)(m.dc, 0, 0, 50, 25), SIMPLEREGION);      /* device 0..100 x 0..50 */
    U(ModifyWorldTransform)(m.dc, NULL, MWT_IDENTITY);
    RECT r; U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 0, 50, 100, 100);
    mdc_destroy(&m);
}
TEST(clipping_xform_offset) {
    NEED_XF(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    HRGN s = rr(10, 10, 30, 30); U(SelectClipRgn)(m.dc, s);
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM x = { 2.0f, 0, 0, 2.0f, 0, 0 };
    U(SetWorldTransform)(m.dc, &x);
    U(OffsetClipRgn)(m.dc, 3, 4);
    U(ModifyWorldTransform)(m.dc, NULL, MWT_IDENTITY);
    RECT r; U(GetClipBox)(m.dc, &r);
    t_note("offset under 2x world transform: (%ld,%ld,%ld,%ld)", r.left, r.top, r.right, r.bottom);
    CHECK(r.right - r.left == 20 && r.bottom - r.top == 20);
    U(DeleteObject)(s); mdc_destroy(&m);
}
TEST(clipping_xform_rot90_intersect) {
    NEED_XF(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM x = { 0, 1.0f, -1.0f, 0, 50.0f, 10.0f };     /* (x,y) -> (50-y, 10+x) */
    CHECK(U(SetWorldTransform)(m.dc, &x));
    int ret = U(IntersectClipRect)(m.dc, 0, 0, 10, 20);
    t_note("rot90 intersect ret=%d", ret);
    CHECK(ret == SIMPLEREGION || ret == COMPLEXREGION);
    U(ModifyWorldTransform)(m.dc, NULL, MWT_IDENTITY);
    U(PatBlt)(m.dc, 0, 0, 100, 100, WHITENESS);
    CHECK(mdc_px(&m, 40, 15) == 0xFFFFFF);          /* well inside 30..50 x 10..20 */
    CHECK(mdc_px(&m, 10, 10) == 0 && mdc_px(&m, 60, 15) == 0 && mdc_px(&m, 40, 40) == 0);
    mdc_destroy(&m);
}
TEST(clipping_xform_rot45_intersect_no_crash) {
    NEED_XF(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM x = { 0.7071f, 0.7071f, -0.7071f, 0.7071f, 50.0f, 10.0f };
    U(SetWorldTransform)(m.dc, &x);
    int ret = U(IntersectClipRect)(m.dc, 0, 0, 30, 30);
    RECT r; int t = U(GetClipBox)(m.dc, &r);
    t_note("rot45 intersect ret=%d boxtype=%d (%ld,%ld,%ld,%ld)", ret, t, r.left, r.top, r.right, r.bottom);
    CHECK(ret != 0);
    U(ModifyWorldTransform)(m.dc, NULL, MWT_IDENTITY);
    U(PatBlt)(m.dc, 0, 0, 100, 100, WHITENESS);
    CHECK(mdc_px(&m, 50, 31) == 0xFFFFFF);          /* centre of the rotated square */
    CHECK(mdc_px(&m, 5, 90) == 0);
    mdc_destroy(&m);
}
TEST(clipping_xform_does_not_change_device_clip) {
    NEED_XF(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    HRGN s = rr(10, 10, 30, 30); U(SelectClipRgn)(m.dc, s);
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    XFORM x = { 5.0f, 0, 0, 5.0f, 1.0f, 1.0f };
    U(SetWorldTransform)(m.dc, &x);
    U(ModifyWorldTransform)(m.dc, NULL, MWT_IDENTITY);
    U(PatBlt)(m.dc, 0, 0, 100, 100, WHITENESS);
    RECT cr = {10, 10, 30, 30};
    EXPECT_EQ(mdc_inside(&m, &cr), 400); EXPECT_EQ(mdc_outside(&m, &cr), 0);
    U(DeleteObject)(s); mdc_destroy(&m);
}

/* ---- metaregion / GetRandomRgn ----------------------------------------- */
#define NEED_META() do { NEED_CLIP(); REQUIRE(U(SetMetaRgn)); REQUIRE(U(GetMetaRgn)); REQUIRE(RNDRGN()); } while (0)
TEST(clipping_meta_initially_none) {
    NEED_META(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN g = rr(3, 3, 9, 9); RECT r;
    int ret = U(GetMetaRgn)(m.dc, g);
    t_note("GetMetaRgn on fresh DC: %d", ret);
    CHECK(ret == 0);
    rgn_box(g, &r); EXPECT_RECT(r, 3, 3, 9, 9);
    U(DeleteObject)(g); mdc_destroy(&m);
}
TEST(clipping_meta_set_basic) {
    NEED_META(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN g = rr(0, 0, 0, 0); RECT r;
    U(IntersectClipRect)(m.dc, 10, 10, 50, 50);
    int ret = U(SetMetaRgn)(m.dc);
    CHECKF(ret == SIMPLEREGION, "SetMetaRgn ret %d", ret);
    EXPECT_EQ(U(GetClipRgn)(m.dc, NULL), 0);          /* clip reset */
    ret = U(GetMetaRgn)(m.dc, g);
    CHECKF(ret == 1, "GetMetaRgn ret %d", ret);
    rgn_box(g, &r); EXPECT_RECT(r, 10, 10, 50, 50);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 10, 10, 50, 50);
    U(DeleteObject)(g); mdc_destroy(&m);
}
TEST(clipping_meta_set_without_clip) {
    NEED_META(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    int ret = U(SetMetaRgn)(m.dc);
    t_note("SetMetaRgn without clip ret=%d", ret);
    RECT r; U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 0, 0, 64, 64);
    mdc_destroy(&m);
}
TEST(clipping_meta_intersects_later_clip) {
    NEED_META(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    RECT r;
    U(IntersectClipRect)(m.dc, 10, 10, 50, 50); U(SetMetaRgn)(m.dc);
    U(IntersectClipRect)(m.dc, 0, 0, 30, 30);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 10, 10, 30, 30);
    HRGN s = rr(0, 0, 64, 64); U(SelectClipRgn)(m.dc, s);          /* cannot escape the meta region */
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 10, 10, 50, 50);
    U(SelectClipRgn)(m.dc, NULL);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 10, 10, 50, 50);
    U(DeleteObject)(s); mdc_destroy(&m);
}
TEST(clipping_meta_nested_set_intersects) {
    NEED_META(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    RECT r; HRGN g = rr(0, 0, 0, 0);
    U(IntersectClipRect)(m.dc, 0, 0, 40, 40); U(SetMetaRgn)(m.dc);
    U(IntersectClipRect)(m.dc, 20, 20, 64, 64);
    int ret = U(SetMetaRgn)(m.dc);
    CHECKF(ret == SIMPLEREGION, "ret %d", ret);
    U(GetMetaRgn)(m.dc, g); rgn_box(g, &r); EXPECT_RECT(r, 20, 20, 40, 40);
    U(DeleteObject)(g); mdc_destroy(&m);
}
TEST(clipping_meta_nested_disjoint_is_empty) {
    NEED_META(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    U(IntersectClipRect)(m.dc, 0, 0, 20, 20); U(SetMetaRgn)(m.dc);
    U(SelectClipRgn)(m.dc, NULL);
    HRGN s = rr(30, 30, 60, 60);
    U(SelectClipRgn)(m.dc, s);
    int ret = U(SetMetaRgn)(m.dc);
    t_note("disjoint SetMetaRgn ret=%d", ret);
    CHECK(ret == NULLREGION);
    RECT r; int t = U(GetClipBox)(m.dc, &r); CHECK(t == NULLREGION);
    U(PatBlt)(m.dc, 0, 0, 64, 64, WHITENESS);
    EXPECT_EQ(mdc_count(&m, 0xFFFFFF), 0);
    U(DeleteObject)(s); mdc_destroy(&m);
}
TEST(clipping_meta_drawing_pixels) {
    NEED_META(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    U(IntersectClipRect)(m.dc, 8, 8, 40, 40); U(SetMetaRgn)(m.dc);
    HRGN s = rr(20, 20, 60, 60); U(SelectClipRgn)(m.dc, s);
    U(PatBlt)(m.dc, 0, 0, 64, 64, WHITENESS);
    RECT cr = {20, 20, 40, 40};
    EXPECT_EQ(mdc_inside(&m, &cr), 400); EXPECT_EQ(mdc_outside(&m, &cr), 0);
    U(DeleteObject)(s); mdc_destroy(&m);
}
TEST(clipping_meta_save_restore) {
    NEED_META(); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    U(SaveDC)(m.dc);
    U(IntersectClipRect)(m.dc, 8, 8, 40, 40); U(SetMetaRgn)(m.dc);
    HRGN g = rr(0, 0, 0, 0);
    EXPECT_EQ(U(GetMetaRgn)(m.dc, g), 1);
    CHECK(U(RestoreDC)(m.dc, -1));
    EXPECT_EQ(U(GetMetaRgn)(m.dc, g), 0);
    RECT r; U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 0, 0, 64, 64);
    U(DeleteObject)(g); mdc_destroy(&m);
}
TEST(clipping_meta_set_invalid_dc) {
    NEED_META();
    EXPECT_EQ(U(SetMetaRgn)(NULL), 0);
    EXPECT_EQ(U(SetMetaRgn)((HDC)(ULONG_PTR)0x1357), 0);
    HRGN g = rr(0, 0, 1, 1);
    int ret = U(GetMetaRgn)(NULL, g);
    t_note("GetMetaRgn(NULL dc) = %d", ret);
    CHECK(ret <= 0);
    U(DeleteObject)(g);
}
TEST(clipping_meta_get_null_region) {
    NEED_META(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    int ret = U(GetMetaRgn)(m.dc, NULL);
    t_note("GetMetaRgn(no meta, NULL rgn) = %d", ret);
    U(IntersectClipRect)(m.dc, 8, 8, 40, 40); U(SetMetaRgn)(m.dc);
    ret = U(GetMetaRgn)(m.dc, NULL);
    t_note("GetMetaRgn(meta set, NULL rgn) = %d", ret);
    CHECK(ret != 1);
    mdc_destroy(&m);
}
TEST(clipping_meta_get_bogus_region) {
    NEED_META(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    U(IntersectClipRect)(m.dc, 8, 8, 40, 40); U(SetMetaRgn)(m.dc);
    int ret = U(GetMetaRgn)(m.dc, (HRGN)(ULONG_PTR)0x6666);
    t_note("GetMetaRgn(bogus rgn) = %d", ret);
    CHECK(ret != 1);
    mdc_destroy(&m);
}
TEST(clipping_meta_equals_random2) {
    NEED_META(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN a = rr(0, 0, 0, 0), b = rr(0, 0, 0, 0);
    int ra = U(GetMetaRgn)(m.dc, a), rb = RNDRGN()(m.dc, b, 2);
    CHECKF(ra == rb, "%d vs %d", ra, rb);
    U(IntersectClipRect)(m.dc, 8, 8, 40, 40); U(SetMetaRgn)(m.dc);
    ra = U(GetMetaRgn)(m.dc, a); rb = RNDRGN()(m.dc, b, 2);
    CHECKF(ra == rb, "%d vs %d", ra, rb);
    CHECK(U(EqualRgn) && U(EqualRgn)(a, b));
    U(DeleteObject)(a); U(DeleteObject)(b); mdc_destroy(&m);
}
TEST(clipping_random_clip_flag1) {
    NEED_META(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN g = rr(7, 7, 8, 8); RECT r;
    EXPECT_EQ(RNDRGN()(m.dc, g, 1), 0);
    rgn_box(g, &r); EXPECT_RECT(r, 7, 7, 8, 8);
    U(IntersectClipRect)(m.dc, 4, 5, 30, 31);
    EXPECT_EQ(RNDRGN()(m.dc, g, 1), 1);
    rgn_box(g, &r); EXPECT_RECT(r, 4, 5, 30, 31);
    U(DeleteObject)(g); mdc_destroy(&m);
}
TEST(clipping_random_flag1_null_rgn) {
    NEED_META(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    int a = RNDRGN()(m.dc, NULL, 1);
    t_note("GetRandomRgn(no clip, NULL, 1) = %d", a);
    U(IntersectClipRect)(m.dc, 4, 5, 30, 31);
    int b = RNDRGN()(m.dc, NULL, 1);
    t_note("GetRandomRgn(clip, NULL, 1) = %d", b);
    CHECK(b != 1);
    mdc_destroy(&m);
}
TEST(clipping_random_flag2_meta) {
    NEED_META(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN g = rr(0, 0, 0, 0); RECT r;
    EXPECT_EQ(RNDRGN()(m.dc, g, 2), 0);
    U(IntersectClipRect)(m.dc, 4, 5, 30, 31); U(SetMetaRgn)(m.dc);
    EXPECT_EQ(RNDRGN()(m.dc, g, 2), 1);
    rgn_box(g, &r); EXPECT_RECT(r, 4, 5, 30, 31);
    EXPECT_EQ(RNDRGN()(m.dc, g, 1), 0);
    U(DeleteObject)(g); mdc_destroy(&m);
}
TEST(clipping_random_flag3_combined) {
    NEED_META(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN g = rr(0, 0, 0, 0); RECT r;
    int ret = RNDRGN()(m.dc, g, 3);
    t_note("flag3 on fresh memory DC: %d", ret);
    U(IntersectClipRect)(m.dc, 4, 5, 30, 31);
    EXPECT_EQ(RNDRGN()(m.dc, g, 3), 1);
    rgn_box(g, &r); EXPECT_RECT(r, 4, 5, 30, 31);
    U(SetMetaRgn)(m.dc);
    U(IntersectClipRect)(m.dc, 10, 0, 64, 20);
    EXPECT_EQ(RNDRGN()(m.dc, g, 3), 1);
    rgn_box(g, &r); EXPECT_RECT(r, 10, 5, 30, 20);
    EXPECT_EQ(RNDRGN()(m.dc, g, 1), 1);
    rgn_box(g, &r); EXPECT_RECT(r, 10, 0, 64, 20);
    EXPECT_EQ(RNDRGN()(m.dc, g, 2), 1);
    rgn_box(g, &r); EXPECT_RECT(r, 4, 5, 30, 31);
    U(DeleteObject)(g); mdc_destroy(&m);
}
TEST(clipping_random_flag4_system) {
    NEED_META(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN g = rr(0, 0, 0, 0); RECT r;
    int ret = RNDRGN()(m.dc, g, 4);
    int t = rgn_box(g, &r);
    t_note("flag4 (SYSRGN) on memory DC: ret=%d type=%d box (%ld,%ld,%ld,%ld)", ret, t, r.left, r.top, r.right, r.bottom);
    CHECK(ret == 0 || ret == 1 || ret == -1);
    U(DeleteObject)(g); mdc_destroy(&m);
}
TEST(clipping_random_invalid_flags) {
    NEED_META(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN g = rr(2, 2, 3, 3);
    static const int fl[] = { 0, 5, 6, 8, -1, 0x100, INT_MAX, INT_MIN };
    for (unsigned i = 0; i < sizeof fl / sizeof fl[0]; i++) {
        SET_ERR(0);
        int ret = RNDRGN()(m.dc, g, fl[i]);
        t_note("GetRandomRgn flag %d -> %d (err %lu)", fl[i], ret, GetLastError());
        CHECKF(ret <= 0, "flag %d ret %d", fl[i], ret);
    }
    U(DeleteObject)(g); mdc_destroy(&m);
}
TEST(clipping_random_invalid_dc) {
    NEED_META(); HRGN g = rr(2, 2, 3, 3);
    for (int f = 1; f <= 4; f++) {
        int ret = RNDRGN()(NULL, g, f);
        CHECKF(ret <= 0, "flag %d ret %d", f, ret);
        ret = RNDRGN()((HDC)(ULONG_PTR)0x2468, g, f);
        CHECKF(ret <= 0, "flag %d bogus ret %d", f, ret);
    }
    U(DeleteObject)(g);
}
TEST(clipping_random_invalid_region) {
    NEED_META(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    U(IntersectClipRect)(m.dc, 4, 5, 30, 31);
    for (int f = 1; f <= 3; f++) {
        int ret = RNDRGN()(m.dc, (HRGN)(ULONG_PTR)0x8888, f);
        t_note("GetRandomRgn(bogus rgn, %d) = %d", f, ret);
        if (f < 3) CHECKF(ret != 1, "flag %d", f);
    }
    mdc_destroy(&m);
}
TEST(clipping_random_matches_getcliprgn) {
    NEED_META(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN g = rr(0, 0, 0, 0);
    CHECK((RNDRGN()(m.dc, g, 1) != 0) == (U(GetClipRgn)(m.dc, g) == 1));
    U(IntersectClipRect)(m.dc, 4, 5, 30, 31);
    CHECK((RNDRGN()(m.dc, g, 1) != 0) == (U(GetClipRgn)(m.dc, g) == 1));
    U(DeleteObject)(g); mdc_destroy(&m);
}
