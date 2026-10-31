#include "clip.h"

#define NEED_EXT() do { NEED_CLIP(); REQUIRE(U(ExtSelectClipRgn)); } while (0)

static int in_rect(const RECT *r, int x, int y) { return x >= r->left && x < r->right && y >= r->top && y < r->bottom; }

/* Combine an initial clip A with region B using `mode`, paint the surface white and compare
 * every pixel with the boolean expectation. */
static void combine_case(int mode, RECT a, RECT b, int want_ret, int a_via_ext) {
    MDC m; if (!mdc_create(&m, 64, 64)) { CHECK(0); return; }
    HRGN ra = rr(a.left, a.top, a.right, a.bottom), rb = rr(b.left, b.top, b.right, b.bottom);
    if (a_via_ext) U(ExtSelectClipRgn)(m.dc, ra, RGN_COPY); else U(SelectClipRgn)(m.dc, ra);
    int ret = U(ExtSelectClipRgn)(m.dc, rb, mode);
    CHECKF(ret == want_ret, "mode %d ret %d want %d", mode, ret, want_ret);
    U(PatBlt)(m.dc, 0, 0, 64, 64, WHITENESS);
    int bad = 0;
    for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) {
        int ia = in_rect(&a, x, y), ib = in_rect(&b, x, y), e = 0;
        switch (mode) {
        case RGN_AND: e = ia && ib; break;
        case RGN_OR: e = ia || ib; break;
        case RGN_XOR: e = ia != ib; break;
        case RGN_DIFF: e = ia && !ib; break;
        case RGN_COPY: e = ib; break;
        }
        if (e != (mdc_px(&m, x, y) == 0xFFFFFF)) bad++;
    }
    CHECKF(bad == 0, "mode %d: %d wrong pixels", mode, bad);
    /* the source region is never modified */
    RECT r; rgn_box(rb, &r); CHECK(rect_is(&r, b.left, b.top, b.right, b.bottom));
    U(DeleteObject)(ra); U(DeleteObject)(rb);
    mdc_destroy(&m);
}
#define OVL  {8, 8, 40, 40}, {24, 24, 56, 56}
#define DISJ {2, 2, 10, 10}, {30, 30, 50, 50}
#define NEST {8, 8, 56, 56}, {20, 20, 30, 30}
#define SAME {8, 8, 40, 40}, {8, 8, 40, 40}
#define CASE(nm, mode, geo, want, ext) TEST(clipping_ext_##nm) { NEED_EXT(); RECT g[2] = { geo }; combine_case(mode, g[0], g[1], want, ext); }

CASE(and_overlap,  RGN_AND,  OVL,  SIMPLEREGION, 0)
CASE(or_overlap,   RGN_OR,   OVL,  COMPLEXREGION, 0)
CASE(xor_overlap,  RGN_XOR,  OVL,  COMPLEXREGION, 0)
CASE(diff_overlap, RGN_DIFF, OVL,  COMPLEXREGION, 0)
CASE(copy_overlap, RGN_COPY, OVL,  SIMPLEREGION, 0)
CASE(and_disjoint,  RGN_AND,  DISJ, NULLREGION, 0)
CASE(or_disjoint,   RGN_OR,   DISJ, COMPLEXREGION, 0)
CASE(xor_disjoint,  RGN_XOR,  DISJ, COMPLEXREGION, 0)
CASE(diff_disjoint, RGN_DIFF, DISJ, SIMPLEREGION, 0)
CASE(copy_disjoint, RGN_COPY, DISJ, SIMPLEREGION, 0)
CASE(and_nested,  RGN_AND,  NEST, SIMPLEREGION, 0)
CASE(or_nested,   RGN_OR,   NEST, SIMPLEREGION, 0)
CASE(xor_nested,  RGN_XOR,  NEST, COMPLEXREGION, 0)
CASE(diff_nested, RGN_DIFF, NEST, COMPLEXREGION, 0)
CASE(and_same,  RGN_AND,  SAME, SIMPLEREGION, 0)
CASE(or_same,   RGN_OR,   SAME, SIMPLEREGION, 0)
CASE(xor_same,  RGN_XOR,  SAME, NULLREGION, 0)
CASE(diff_same, RGN_DIFF, SAME, NULLREGION, 0)
CASE(and_overlap_viaext,  RGN_AND,  OVL, SIMPLEREGION, 1)
CASE(or_overlap_viaext,   RGN_OR,   OVL, COMPLEXREGION, 1)
CASE(xor_overlap_viaext,  RGN_XOR,  OVL, COMPLEXREGION, 1)
CASE(diff_overlap_viaext, RGN_DIFF, OVL, COMPLEXREGION, 1)

TEST(clipping_ext_and_without_existing_clip) {
    NEED_EXT(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN b = rr(10, 10, 30, 30); RECT r;
    int ret = U(ExtSelectClipRgn)(m.dc, b, RGN_AND);
    CHECKF(ret == SIMPLEREGION, "ret %d", ret);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 10, 10, 30, 30);
    U(DeleteObject)(b); mdc_destroy(&m);
}
TEST(clipping_ext_diff_without_existing_clip) {
    NEED_EXT(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN b = rr(20, 20, 40, 40);
    int ret = U(ExtSelectClipRgn)(m.dc, b, RGN_DIFF);
    CHECKF(ret == COMPLEXREGION, "ret %d", ret);        /* surface minus a hole */
    U(PatBlt)(m.dc, 0, 0, 64, 64, WHITENESS);
    CHECK(mdc_px(&m, 5, 5) == 0xFFFFFF && mdc_px(&m, 30, 30) == 0 && mdc_px(&m, 40, 40) == 0xFFFFFF);
    U(DeleteObject)(b); mdc_destroy(&m);
}
TEST(clipping_ext_or_without_existing_clip) {
    NEED_EXT(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN b = rr(20, 20, 40, 40);
    int ret = U(ExtSelectClipRgn)(m.dc, b, RGN_OR);
    t_note("RGN_OR with no clip: ret=%d", ret);
    CHECK(ret != 0);
    RECT r; U(GetClipBox)(m.dc, &r);
    t_note("box (%ld,%ld,%ld,%ld) cliprgn=%d", r.left, r.top, r.right, r.bottom, U(GetClipRgn)(m.dc, NULL));
    U(DeleteObject)(b); mdc_destroy(&m);
}
TEST(clipping_ext_xor_without_existing_clip) {
    NEED_EXT(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN b = rr(20, 20, 40, 40);
    int ret = U(ExtSelectClipRgn)(m.dc, b, RGN_XOR);
    t_note("RGN_XOR with no clip: ret=%d", ret);
    CHECK(ret != 0);
    U(PatBlt)(m.dc, 0, 0, 64, 64, WHITENESS);
    t_note("px(30,30)=%lx px(5,5)=%lx", (unsigned long)mdc_px(&m, 30, 30), (unsigned long)mdc_px(&m, 5, 5));
    U(DeleteObject)(b); mdc_destroy(&m);
}
TEST(clipping_ext_copy_null_resets) {
    NEED_EXT(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN b = rr(10, 10, 20, 20); RECT r;
    U(SelectClipRgn)(m.dc, b);
    EXPECT_EQ(U(ExtSelectClipRgn)(m.dc, NULL, RGN_COPY), SIMPLEREGION);
    EXPECT_EQ(U(GetClipRgn)(m.dc, NULL), 0);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 0, 0, 64, 64);
    U(DeleteObject)(b); mdc_destroy(&m);
}
TEST(clipping_ext_null_other_modes) {
    NEED_EXT(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN b = rr(10, 10, 20, 20);
    static const int modes[] = { RGN_AND, RGN_OR, RGN_XOR, RGN_DIFF };
    for (unsigned i = 0; i < 4; i++) {
        U(SelectClipRgn)(m.dc, b);
        int ret = U(ExtSelectClipRgn)(m.dc, NULL, modes[i]);
        RECT r; U(GetClipBox)(m.dc, &r);
        t_note("ExtSelectClipRgn(NULL, mode %d) ret=%d box (%ld,%ld,%ld,%ld)", modes[i], ret, r.left, r.top, r.right, r.bottom);
    }
    U(DeleteObject)(b); mdc_destroy(&m);
}
TEST(clipping_ext_invalid_modes) {
    NEED_EXT(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN b = rr(10, 10, 20, 20), a = rr(0, 0, 30, 30); RECT r;
    static const int modes[] = { 0, 6, 7, -1, 100, 0x100, INT_MAX, INT_MIN };
    for (unsigned i = 0; i < sizeof modes / sizeof modes[0]; i++) {
        U(SelectClipRgn)(m.dc, a);
        int ret = U(ExtSelectClipRgn)(m.dc, b, modes[i]);
        CHECKF(ret == 0, "mode %d ret %d", modes[i], ret);
        U(GetClipBox)(m.dc, &r);
        CHECKF(rect_is(&r, 0, 0, 30, 30), "clip changed by invalid mode %d", modes[i]);
    }
    U(DeleteObject)(b); U(DeleteObject)(a); mdc_destroy(&m);
}
TEST(clipping_ext_invalid_dc_and_region) {
    NEED_EXT(); MDC m; CHECK(mdc_create(&m, 32, 32)); if (!m.dc) return;
    HRGN b = rr(1, 1, 2, 2);
    EXPECT_EQ(U(ExtSelectClipRgn)(NULL, b, RGN_COPY), 0);
    EXPECT_EQ(U(ExtSelectClipRgn)((HDC)(ULONG_PTR)0x1234, b, RGN_AND), 0);
    EXPECT_EQ(U(ExtSelectClipRgn)(m.dc, (HRGN)(ULONG_PTR)0x4444, RGN_COPY), 0);
    EXPECT_EQ(U(ExtSelectClipRgn)(m.dc, (HRGN)(ULONG_PTR)0x4444, RGN_AND), 0);
    EXPECT_EQ(U(ExtSelectClipRgn)(m.dc, (HRGN)m.bmp, RGN_COPY), 0);
    EXPECT_EQ(U(ExtSelectClipRgn)(NULL, NULL, RGN_COPY), 0);
    U(DeleteObject)(b); mdc_destroy(&m);
}
TEST(clipping_ext_copy_equals_selectclip) {
    NEED_EXT(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN b = rr(0, 0, 10, 10), c = rr(20, 20, 30, 30);
    U(CombineRgn)(b, b, c, RGN_OR);
    EXPECT_EQ(U(ExtSelectClipRgn)(m.dc, b, RGN_COPY), U(SelectClipRgn)(m.dc, b));
    U(DeleteObject)(b); U(DeleteObject)(c); mdc_destroy(&m);
}
TEST(clipping_ext_chain_modes) {
    NEED_EXT(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN a = rr(0, 0, 40, 40), b = rr(10, 10, 20, 20), c = rr(30, 30, 64, 64), d = rr(0, 0, 64, 5);
    U(ExtSelectClipRgn)(m.dc, a, RGN_COPY);
    U(ExtSelectClipRgn)(m.dc, b, RGN_DIFF);     /* A - B */
    U(ExtSelectClipRgn)(m.dc, c, RGN_OR);       /* + C */
    U(ExtSelectClipRgn)(m.dc, d, RGN_XOR);      /* ^ D */
    U(PatBlt)(m.dc, 0, 0, 64, 64, WHITENESS);
    RECT ra = {0, 0, 40, 40}, rb2 = {10, 10, 20, 20}, rc = {30, 30, 64, 64}, rd = {0, 0, 64, 5};
    int bad = 0;
    for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) {
        int e = (((in_rect(&ra, x, y) && !in_rect(&rb2, x, y)) || in_rect(&rc, x, y)) != in_rect(&rd, x, y));
        if (e != (mdc_px(&m, x, y) == 0xFFFFFF)) bad++;
    }
    CHECKF(bad == 0, "%d wrong pixels", bad);
    U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(c); U(DeleteObject)(d);
    mdc_destroy(&m);
}
TEST(clipping_ext_complex_shape_pixels) {
    NEED_EXT(); REQUIRE(U(CreateEllipticRgn)); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN e = U(CreateEllipticRgn)(4, 4, 60, 50);
    int ret = U(SelectClipRgn)(m.dc, e);
    CHECKF(ret == COMPLEXREGION, "ret %d", ret);
    U(PatBlt)(m.dc, 0, 0, 64, 64, WHITENESS);
    int bad; CHECK(mdc_matches_region(&m, e, 0xFFFFFF, &bad));
    CHECKF(bad == 0, "bad=%d", bad);
    U(DeleteObject)(e); mdc_destroy(&m);
}
TEST(clipping_ext_polygon_shape_pixels) {
    NEED_EXT(); REQUIRE(U(CreatePolygonRgn)); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    POINT p[] = { {5, 5}, {58, 12}, {30, 60}, {8, 40} };
    HRGN e = U(CreatePolygonRgn)(p, 4, ALTERNATE);
    EXPECT_EQ(U(SelectClipRgn)(m.dc, e), COMPLEXREGION);
    U(PatBlt)(m.dc, 0, 0, 64, 64, WHITENESS);
    int bad; CHECK(mdc_matches_region(&m, e, 0xFFFFFF, &bad));
    CHECKF(bad == 0, "bad=%d", bad);
    U(DeleteObject)(e); mdc_destroy(&m);
}
TEST(clipping_ext_clip_rgn_roundtrip_complex) {
    NEED_EXT(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN a = rr(0, 0, 20, 20), b = rr(30, 30, 50, 50), g = rr(0, 0, 0, 0);
    U(CombineRgn)(a, a, b, RGN_OR);
    U(SelectClipRgn)(m.dc, a);
    EXPECT_EQ(U(GetClipRgn)(m.dc, g), 1);
    REQUIRE(U(EqualRgn));
    CHECK(U(EqualRgn)(a, g));
    U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(g); mdc_destroy(&m);
}
TEST(clipping_ext_empty_region_and_or) {
    NEED_EXT(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN a = rr(5, 5, 25, 25), e = rr(0, 0, 0, 0); RECT r;
    U(SelectClipRgn)(m.dc, a);
    EXPECT_EQ(U(ExtSelectClipRgn)(m.dc, e, RGN_OR), SIMPLEREGION);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 5, 5, 25, 25);
    EXPECT_EQ(U(ExtSelectClipRgn)(m.dc, e, RGN_DIFF), SIMPLEREGION);
    EXPECT_EQ(U(ExtSelectClipRgn)(m.dc, e, RGN_XOR), SIMPLEREGION);
    EXPECT_EQ(U(ExtSelectClipRgn)(m.dc, e, RGN_AND), NULLREGION);
    U(DeleteObject)(a); U(DeleteObject)(e); mdc_destroy(&m);
}
TEST(clipping_ext_after_null_region_state_and) {
    NEED_EXT(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN a = rr(5, 5, 25, 25), b = rr(30, 30, 40, 40);
    U(SelectClipRgn)(m.dc, a);
    EXPECT_EQ(U(ExtSelectClipRgn)(m.dc, b, RGN_AND), NULLREGION);
    /* once empty, OR rebuilds */
    EXPECT_EQ(U(ExtSelectClipRgn)(m.dc, b, RGN_OR), SIMPLEREGION);
    RECT r; U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 30, 30, 40, 40);
    U(DeleteObject)(a); U(DeleteObject)(b); mdc_destroy(&m);
}

/* ---- SaveDC / RestoreDC with clip -------------------------------------- */
#define NEED_SAVE() do { NEED_CLIP(); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); } while (0)
TEST(clipping_save_levels_increment) {
    NEED_SAVE(); MDC m; CHECK(mdc_create(&m, 16, 16)); if (!m.dc) return;
    EXPECT_EQ(U(SaveDC)(m.dc), 1);
    EXPECT_EQ(U(SaveDC)(m.dc), 2);
    EXPECT_EQ(U(SaveDC)(m.dc), 3);
    CHECK(U(RestoreDC)(m.dc, -3));
    EXPECT_EQ(U(SaveDC)(m.dc), 1);
    CHECK(U(RestoreDC)(m.dc, -1));
    mdc_destroy(&m);
}
TEST(clipping_save_restore_clip) {
    NEED_SAVE(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    RECT r; HRGN a = rr(5, 5, 50, 50);
    U(SelectClipRgn)(m.dc, a);
    int lvl = U(SaveDC)(m.dc);
    U(IntersectClipRect)(m.dc, 10, 10, 20, 20);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 10, 10, 20, 20);
    CHECK(U(RestoreDC)(m.dc, lvl));
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 5, 5, 50, 50);
    U(DeleteObject)(a); mdc_destroy(&m);
}
TEST(clipping_save_restore_no_clip_state) {
    NEED_SAVE(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    RECT r; HRGN a = rr(5, 5, 50, 50);
    EXPECT_EQ(U(SaveDC)(m.dc), 1);
    U(SelectClipRgn)(m.dc, a);
    CHECK(U(RestoreDC)(m.dc, -1));
    EXPECT_EQ(U(GetClipRgn)(m.dc, NULL), 0);
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 0, 0, 64, 64);
    U(DeleteObject)(a); mdc_destroy(&m);
}
TEST(clipping_save_restore_clip_to_none_and_back) {
    NEED_SAVE(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    RECT r; HRGN a = rr(5, 5, 50, 50);
    U(SelectClipRgn)(m.dc, a);
    U(SaveDC)(m.dc);
    U(SelectClipRgn)(m.dc, NULL);
    EXPECT_EQ(U(GetClipRgn)(m.dc, NULL), 0);
    CHECK(U(RestoreDC)(m.dc, -1));
    { int g_ = U(GetClipRgn)(m.dc, NULL); t_note("GetClipRgn(dc,NULL) with clip -> %d", g_); }
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 5, 5, 50, 50);
    U(DeleteObject)(a); mdc_destroy(&m);
}
TEST(clipping_save_restore_absolute_level) {
    NEED_SAVE(); MDC m; CHECK(mdc_create(&m, 128, 128)); if (!m.dc) return;
    RECT r;
    for (int i = 0; i < 10; i++) {
        int lvl = U(SaveDC)(m.dc);
        CHECKF(lvl == i + 1, "lvl %d", lvl);
        U(IntersectClipRect)(m.dc, 0, 0, 100 - i * 5, 100 - i * 5);
    }
    CHECK(U(RestoreDC)(m.dc, 4));           /* back to the state saved as level 4 */
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 0, 0, 90, 90);
    EXPECT_EQ(U(SaveDC)(m.dc), 4);          /* levels above 4 are popped, 4 itself too */
    CHECK(U(RestoreDC)(m.dc, 1));
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 0, 0, 128, 128);
    mdc_destroy(&m);
}
TEST(clipping_save_restore_invalid_levels) {
    NEED_SAVE(); MDC m; CHECK(mdc_create(&m, 16, 16)); if (!m.dc) return;
    SET_ERR(0);
    CHECK(!U(RestoreDC)(m.dc, -1));                 /* nothing saved */
    CHECK(!U(RestoreDC)(m.dc, 1));
    U(SaveDC)(m.dc);
    CHECK(!U(RestoreDC)(m.dc, 2));
    CHECK(!U(RestoreDC)(m.dc, -2));
    CHECK(!U(RestoreDC)(m.dc, -100));
    CHECK(!U(RestoreDC)(m.dc, 100));
    t_note("RestoreDC(INT_MIN) -> %d", U(RestoreDC)(m.dc, INT_MIN));
    CHECK(U(RestoreDC)(m.dc, -1));
    mdc_destroy(&m);
}
TEST(clipping_save_restore_level_zero) {
    NEED_SAVE(); MDC m; CHECK(mdc_create(&m, 16, 16)); if (!m.dc) return;
    U(SaveDC)(m.dc);
    int r0 = U(RestoreDC)(m.dc, 0);
    t_note("RestoreDC(dc, 0) -> %d", r0);
    if (r0) { } else CHECK(U(RestoreDC)(m.dc, -1));
    mdc_destroy(&m);
}
TEST(clipping_save_restore_invalid_dc) {
    NEED_SAVE();
    EXPECT_EQ(U(SaveDC)(NULL), 0);
    EXPECT_EQ(U(SaveDC)((HDC)(ULONG_PTR)0x5555), 0);
    CHECK(!U(RestoreDC)(NULL, -1));
    CHECK(!U(RestoreDC)((HDC)(ULONG_PTR)0x5555, 1));
}
TEST(clipping_save_restore_pixels) {
    NEED_SAVE(); MDC m; CHECK(mdc_create(&m, 32, 32)); if (!m.dc) return;
    U(SaveDC)(m.dc);
    U(IntersectClipRect)(m.dc, 0, 0, 8, 8);
    U(PatBlt)(m.dc, 0, 0, 32, 32, WHITENESS);
    CHECK(mdc_count(&m, 0xFFFFFF) == 64);
    CHECK(U(RestoreDC)(m.dc, -1));
    mdc_clear(&m, 0);
    U(PatBlt)(m.dc, 0, 0, 32, 32, WHITENESS);
    CHECK(mdc_count(&m, 0xFFFFFF) == 1024);
    mdc_destroy(&m);
}
TEST(clipping_save_nested_exclude) {
    NEED_SAVE(); MDC m; CHECK(mdc_create(&m, 32, 32)); if (!m.dc) return;
    U(SaveDC)(m.dc); U(ExcludeClipRect)(m.dc, 0, 0, 16, 32);
    U(SaveDC)(m.dc); U(ExcludeClipRect)(m.dc, 16, 0, 32, 16);
    RECT r; EXPECT_EQ(U(GetClipBox)(m.dc, &r), SIMPLEREGION); EXPECT_RECT(r, 16, 16, 32, 32);
    CHECK(U(RestoreDC)(m.dc, -1));
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 16, 0, 32, 32);
    CHECK(U(RestoreDC)(m.dc, -1));
    U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 0, 0, 32, 32);
    mdc_destroy(&m);
}
TEST(clipping_save_many_levels) {
    NEED_SAVE(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    int bad = 0;
    for (int i = 0; i < 500; i++) { if (U(SaveDC)(m.dc) != i + 1) bad++; U(IntersectClipRect)(m.dc, 0, 0, 64 - i % 60, 64); }
    CHECK(bad == 0);
    CHECK(U(RestoreDC)(m.dc, 1));
    RECT r; U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 0, 0, 64, 64);
    mdc_destroy(&m);
}

/* ---- SelectClipPath ---------------------------------------------------- */
#define NEED_PATH() do { NEED_CLIP(); REQUIRE(U(SelectClipPath)); REQUIRE(U(BeginPath)); REQUIRE(U(EndPath)); \
    REQUIRE(U(MoveToEx)); REQUIRE(U(LineTo)); REQUIRE(U(CloseFigure)); REQUIRE(U(PtVisible)); } while (0)
static void path_square(HDC dc, int l, int t, int r, int b) {
    U(BeginPath)(dc);
    U(MoveToEx)(dc, l, t, NULL); U(LineTo)(dc, r, t); U(LineTo)(dc, r, b); U(LineTo)(dc, l, b);
    U(CloseFigure)(dc);
    U(EndPath)(dc);
}
TEST(clipping_path_copy_basic) {
    NEED_PATH(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    path_square(m.dc, 10, 10, 30, 30);
    SET_ERR(0);
    CHECK(U(SelectClipPath)(m.dc, RGN_COPY));
    RECT r; int t = U(GetClipBox)(m.dc, &r);
    CHECKF(t == SIMPLEREGION, "type %d", t);
    t_note("path square box (%ld,%ld,%ld,%ld)", r.left, r.top, r.right, r.bottom);
    CHECK(U(PtVisible)(m.dc, 15, 15) && U(PtVisible)(m.dc, 29, 29));
    CHECK(!U(PtVisible)(m.dc, 5, 5) && !U(PtVisible)(m.dc, 40, 40));
    mdc_destroy(&m);
}
TEST(clipping_path_pixels_inside_outside) {
    NEED_PATH(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    path_square(m.dc, 10, 10, 30, 30);
    CHECK(U(SelectClipPath)(m.dc, RGN_COPY));
    U(PatBlt)(m.dc, 0, 0, 64, 64, WHITENESS);
    CHECK(mdc_px(&m, 20, 20) == 0xFFFFFF && mdc_px(&m, 5, 5) == 0 && mdc_px(&m, 40, 40) == 0 && mdc_px(&m, 9, 20) == 0 && mdc_px(&m, 31, 20) == 0);
    int n = mdc_count(&m, 0xFFFFFF);
    t_note("square path pixel count %d (expect ~400)", n);
    CHECK(n >= 19 * 19 && n <= 21 * 21);
    mdc_destroy(&m);
}
TEST(clipping_path_consumed) {
    NEED_PATH(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    path_square(m.dc, 10, 10, 30, 30);
    CHECK(U(SelectClipPath)(m.dc, RGN_COPY));
    SET_ERR(0);
    CHECK(!U(SelectClipPath)(m.dc, RGN_COPY));             /* the path was discarded */
    t_note("second SelectClipPath lasterror %lu", GetLastError());
    mdc_destroy(&m);
}
TEST(clipping_path_none) {
    NEED_PATH(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    SET_ERR(0);
    CHECK(!U(SelectClipPath)(m.dc, RGN_COPY));
    t_note("SelectClipPath without path lasterror %lu", GetLastError());
    RECT r; U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 0, 0, 64, 64);
    mdc_destroy(&m);
}
TEST(clipping_path_still_open) {
    NEED_PATH(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    U(BeginPath)(m.dc);
    U(MoveToEx)(m.dc, 10, 10, NULL); U(LineTo)(m.dc, 30, 10); U(LineTo)(m.dc, 30, 30);
    SET_ERR(0);
    CHECK(!U(SelectClipPath)(m.dc, RGN_COPY));
    t_note("SelectClipPath on open bracket lasterror %lu", GetLastError());
    RECT r; U(GetClipBox)(m.dc, &r); EXPECT_RECT(r, 0, 0, 64, 64);
    U(EndPath)(m.dc);
    CHECK(U(SelectClipPath)(m.dc, RGN_COPY));              /* now closed */
    mdc_destroy(&m);
}
TEST(clipping_path_invalid_modes) {
    NEED_PATH(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    static const int modes[] = { 0, 6, -1, 100 };
    for (unsigned i = 0; i < 4; i++) {
        path_square(m.dc, 10, 10, 30, 30);
        SET_ERR(0);
        BOOL ok = U(SelectClipPath)(m.dc, modes[i]);
        CHECKF(!ok, "mode %d accepted", modes[i]);
        RECT r; U(GetClipBox)(m.dc, &r);
        CHECKF(rect_is(&r, 0, 0, 64, 64), "clip changed for mode %d", modes[i]);
        U(SelectClipPath)(m.dc, RGN_COPY);                  /* drain */
        U(SelectClipRgn)(m.dc, NULL);
    }
    mdc_destroy(&m);
}
TEST(clipping_path_invalid_dc) {
    NEED_PATH();
    CHECK(!U(SelectClipPath)(NULL, RGN_COPY));
    CHECK(!U(SelectClipPath)((HDC)(ULONG_PTR)0x4321, RGN_AND));
}
#define PATH_MODE_TEST(nm, mode) TEST(clipping_path_mode_##nm) { \
    NEED_PATH(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return; \
    RECT a = {0, 0, 40, 40}, b = {20, 20, 60, 60}; HRGN ra = rr(0, 0, 40, 40); \
    U(SelectClipRgn)(m.dc, ra); path_square(m.dc, 20, 20, 60, 60); \
    CHECK(U(SelectClipPath)(m.dc, mode)); U(PatBlt)(m.dc, 0, 0, 64, 64, WHITENESS); \
    int in_a_only = 0, in_b_only = 0, in_both = 0, none = 0; \
    for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) { \
        int pa = in_rect(&a, x, y), pb = in_rect(&b, x, y); int s = mdc_px(&m, x, y) == 0xFFFFFF; \
        if (pa && pb) in_both += s; else if (pa) in_a_only += s; else if (pb) in_b_only += s; else none += s; } \
    t_note("a_only=%d b_only=%d both=%d none=%d", in_a_only, in_b_only, in_both, none); \
    CHECK(none == 0); \
    U(DeleteObject)(ra); mdc_destroy(&m); }
PATH_MODE_TEST(and, RGN_AND)
PATH_MODE_TEST(or, RGN_OR)
PATH_MODE_TEST(xor, RGN_XOR)
PATH_MODE_TEST(diff, RGN_DIFF)
PATH_MODE_TEST(copy, RGN_COPY)

TEST(clipping_path_and_result) {
    NEED_PATH(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN ra = rr(0, 0, 40, 40); U(SelectClipRgn)(m.dc, ra);
    path_square(m.dc, 20, 20, 60, 60);
    CHECK(U(SelectClipPath)(m.dc, RGN_AND));
    CHECK(U(PtVisible)(m.dc, 30, 30)); CHECK(!U(PtVisible)(m.dc, 10, 10)); CHECK(!U(PtVisible)(m.dc, 50, 50));
    U(DeleteObject)(ra); mdc_destroy(&m);
}
TEST(clipping_path_diff_result) {
    NEED_PATH(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    HRGN ra = rr(0, 0, 40, 40); U(SelectClipRgn)(m.dc, ra);
    path_square(m.dc, 20, 20, 60, 60);
    CHECK(U(SelectClipPath)(m.dc, RGN_DIFF));
    CHECK(U(PtVisible)(m.dc, 10, 10)); CHECK(!U(PtVisible)(m.dc, 30, 30)); CHECK(!U(PtVisible)(m.dc, 50, 50));
    U(DeleteObject)(ra); mdc_destroy(&m);
}
TEST(clipping_path_empty_path) {
    NEED_PATH(); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    U(BeginPath)(m.dc); U(EndPath)(m.dc);
    SET_ERR(0);
    BOOL ok = U(SelectClipPath)(m.dc, RGN_COPY);
    RECT r; int t = U(GetClipBox)(m.dc, &r);
    t_note("empty path: ok=%d err=%lu boxtype=%d", ok, GetLastError(), t);
    mdc_destroy(&m);
}
TEST(clipping_path_bezier_and_ellipse) {
    NEED_PATH(); REQUIRE(U(Ellipse)); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    U(BeginPath)(m.dc); U(Ellipse)(m.dc, 4, 4, 60, 60); U(EndPath)(m.dc);
    CHECK(U(SelectClipPath)(m.dc, RGN_COPY));
    CHECK(U(PtVisible)(m.dc, 32, 32)); CHECK(!U(PtVisible)(m.dc, 5, 5)); CHECK(!U(PtVisible)(m.dc, 58, 5));
    { int g_ = U(GetClipRgn)(m.dc, NULL); t_note("GetClipRgn(dc,NULL) with clip -> %d", g_); }
    mdc_destroy(&m);
}
TEST(clipping_path_stroke_not_used) {
    NEED_PATH(); REQUIRE(U(Rectangle)); MDC m; CHECK(mdc_create(&m, 64, 64)); if (!m.dc) return;
    U(BeginPath)(m.dc); U(Rectangle)(m.dc, 10, 10, 40, 40); U(EndPath)(m.dc);
    CHECK(U(SelectClipPath)(m.dc, RGN_COPY));
    CHECK(U(PtVisible)(m.dc, 25, 25));         /* interior counts: it is a fill region */
    mdc_destroy(&m);
}
TEST(clipping_path_text_path_bounded) {
    NEED_PATH(); REQUIRE(U(TextOutA)); REQUIRE(U(CreateFontA)); MDC m; CHECK(mdc_create(&m, 128, 64)); if (!m.dc) return;
    HFONT f = U(CreateFontA)(40, 0, 0, 0, 700, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, "Arial");
    HGDIOBJ old = U(SelectObject)(m.dc, f);
    U(SetBkMode)(m.dc, TRANSPARENT);
    U(BeginPath)(m.dc); U(TextOutA)(m.dc, 2, 2, "HI", 2); U(EndPath)(m.dc);
    BOOL ok = U(SelectClipPath)(m.dc, RGN_COPY);
    CHECK(ok);
    RECT r; int t = U(GetClipBox)(m.dc, &r);
    t_note("text path clip type=%d box (%ld,%ld,%ld,%ld)", t, r.left, r.top, r.right, r.bottom);
    CHECK(t == SIMPLEREGION || t == COMPLEXREGION);
    CHECK(r.left >= 0 && r.top >= 0 && r.right <= 128 && r.bottom <= 64);
    U(SelectObject)(m.dc, old); U(DeleteObject)(f);
    mdc_destroy(&m);
}
