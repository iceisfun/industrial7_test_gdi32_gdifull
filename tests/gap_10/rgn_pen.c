/* CreateRectRgnIndirect, CreateEllipticRgnIndirect, CreatePenIndirect. */
#include "gap10.h"

static RECT R(int l, int t, int r, int b) { RECT x; x.left = l; x.top = t; x.right = r; x.bottom = b; return x; }

/* Sum of rectangle areas from region data. */
static long rgn_area(HRGN r, int *nrects) {
    DWORD sz = U(GetRegionData)(r, 0, NULL);
    RGNDATA *d; long a = 0; DWORD i; RECT *rc;
    if (!sz) return -1;
    d = (RGNDATA *)HeapAlloc(GetProcessHeap(), 0, sz);
    if (!d) return -1;
    U(GetRegionData)(r, sz, d);
    rc = (RECT *)d->Buffer;
    for (i = 0; i < d->rdh.nCount; i++) a += (long)(rc[i].right - rc[i].left) * (rc[i].bottom - rc[i].top);
    if (nrects) *nrects = (int)d->rdh.nCount;
    HeapFree(GetProcessHeap(), 0, d);
    return a;
}
#define NEED_RGN() do { REQUIRE(U(CreateRectRgnIndirect)); REQUIRE(U(CreateRectRgn)); REQUIRE(U(GetRgnBox)); \
    REQUIRE(U(DeleteObject)); REQUIRE(U(PtInRegion)); REQUIRE(U(EqualRgn)); REQUIRE(U(GetRegionData)); } while (0)

/* ---------------- CreateRectRgnIndirect ---------------- */
TEST(gap10_crri_basic_box_and_type) {
    NEED_RGN();
    RECT in = R(10, 20, 110, 70), box; HRGN h = U(CreateRectRgnIndirect)(&in);
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(U(GetRgnBox)(h, &box), SIMPLEREGION);
    EXPECT_EQ(box.left, 10); EXPECT_EQ(box.top, 20); EXPECT_EQ(box.right, 110); EXPECT_EQ(box.bottom, 70);
    U(DeleteObject)(h);
}
TEST(gap10_crri_equal_to_createrectrgn) {
    NEED_RGN();
    int t[][4] = { {0,0,1,1}, {-5,-5,5,5}, {100,200,300,400}, {0,0,0,0}, {5,5,5,9}, {7,3,7,3}, {-100,-50,-1,-1} };
    unsigned i;
    for (i = 0; i < sizeof t / sizeof *t; i++) {
        RECT in = R(t[i][0], t[i][1], t[i][2], t[i][3]);
        HRGN a = U(CreateRectRgnIndirect)(&in), b = U(CreateRectRgn)(t[i][0], t[i][1], t[i][2], t[i][3]);
        CHECKF(a && b, "case %u", i);
        if (a && b) CHECKF(U(EqualRgn)(a, b), "case %u", i);
        if (a) U(DeleteObject)(a);
        if (b) U(DeleteObject)(b);
    }
}
TEST(gap10_crri_reversed_coords_normalised) {
    NEED_RGN();
    RECT in = R(110, 70, 10, 20), box; HRGN h = U(CreateRectRgnIndirect)(&in);
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(U(GetRgnBox)(h, &box), SIMPLEREGION);
    EXPECT_EQ(box.left, 10); EXPECT_EQ(box.top, 20); EXPECT_EQ(box.right, 110); EXPECT_EQ(box.bottom, 70);
    U(DeleteObject)(h);
    in = R(10, 70, 110, 20);
    h = U(CreateRectRgnIndirect)(&in);
    CHECK(h != NULL);
    if (h) { U(GetRgnBox)(h, &box); EXPECT_EQ(box.top, 20); EXPECT_EQ(box.bottom, 70); U(DeleteObject)(h); }
}
TEST(gap10_crri_empty_rect_is_nullregion) {
    NEED_RGN();
    RECT in = R(5, 5, 5, 5), box; HRGN h = U(CreateRectRgnIndirect)(&in);
    CHECK(h != NULL);
    if (!h) return;
    memset(&box, 0x55, sizeof box);
    EXPECT_EQ(U(GetRgnBox)(h, &box), NULLREGION);
    t_note("empty region box: %ld,%ld,%ld,%ld", box.left, box.top, box.right, box.bottom);
    CHECK(!U(PtInRegion)(h, 5, 5));
    U(DeleteObject)(h);
    in = R(0, 0, 10, 0);
    h = U(CreateRectRgnIndirect)(&in);
    CHECK(h != NULL);
    if (h) { EXPECT_EQ(U(GetRgnBox)(h, &box), NULLREGION); U(DeleteObject)(h); }
}
TEST(gap10_crri_ptinregion_edges) {
    NEED_RGN();
    RECT in = R(10, 10, 20, 20); HRGN h = U(CreateRectRgnIndirect)(&in);
    CHECK(h != NULL);
    if (!h) return;
    CHECK(U(PtInRegion)(h, 10, 10));
    CHECK(U(PtInRegion)(h, 19, 19));
    CHECK(!U(PtInRegion)(h, 20, 20));
    CHECK(!U(PtInRegion)(h, 20, 10));
    CHECK(!U(PtInRegion)(h, 10, 20));
    CHECK(!U(PtInRegion)(h, 9, 10));
    CHECK(!U(PtInRegion)(h, 10, 9));
    U(DeleteObject)(h);
}
TEST(gap10_crri_input_not_modified) {
    NEED_RGN();
    RECT in = R(30, 40, 5, 2), copy = in; HRGN h = U(CreateRectRgnIndirect)(&in);
    CHECK(h != NULL);
    CHECK(!memcmp(&in, &copy, sizeof in));
    if (h) U(DeleteObject)(h);
}
TEST(gap10_crri_regiondata_single_rect) {
    NEED_RGN();
    RECT in = R(3, 4, 13, 24); HRGN h = U(CreateRectRgnIndirect)(&in); int n = 0;
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(rgn_area(h, &n), 200);
    EXPECT_EQ(n, 1);
    U(DeleteObject)(h);
}
TEST(gap10_crri_extreme_coordinates_observed) {
    NEED_RGN();
    int t[][4] = { {-0x4000000, -0x4000000, 0x4000000, 0x4000000}, {INT_MIN, INT_MIN, INT_MAX, INT_MAX}, {0, 0, INT_MAX, INT_MAX}, {INT_MIN, 0, 0, 5} };
    unsigned i;
    for (i = 0; i < sizeof t / sizeof *t; i++) {
        RECT in = R(t[i][0], t[i][1], t[i][2], t[i][3]), box;
        HRGN h = U(CreateRectRgnIndirect)(&in);
        if (!h) { t_note("extreme %u: NULL", i); continue; }
        int ty = U(GetRgnBox)(h, &box);
        t_note("extreme %u: type %d box %ld,%ld,%ld,%ld", i, ty, box.left, box.top, box.right, box.bottom);
        CHECK(ty != 0);
        U(DeleteObject)(h);
    }
}
TEST(gap10_crri_object_type_and_lifetime) {
    NEED_RGN(); REQUIRE(U(GetObjectType));
    RECT in = R(0, 0, 4, 4), box; HRGN h = U(CreateRectRgnIndirect)(&in);
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(U(GetObjectType)(h), OBJ_REGION);
    CHECK(U(DeleteObject)(h));
    CHECK(!U(DeleteObject)(h));
    EXPECT_EQ(U(GetObjectType)(h), 0);
    EXPECT_EQ(U(GetRgnBox)(h, &box), 0);
}
TEST(gap10_crri_no_gdi_leak) {
    NEED_RGN();
    if (!g10_gdicount()) { t_skip("no GetGuiResources"); return; }
    DWORD before = g10_gdicount(); HRGN h[300]; int i; RECT in = R(0, 0, 10, 10);
    for (i = 0; i < 300; i++) h[i] = U(CreateRectRgnIndirect)(&in);
    CHECKF(g10_gdicount() >= before + 290, "count %lu", g10_gdicount());
    for (i = 0; i < 300; i++) if (h[i]) U(DeleteObject)(h[i]);
    EXPECT_EQ(g10_gdicount(), before);
}
static void crri_thr(void *p) { RECT in = R(1, 2, 31, 42); *(HRGN *)p = U(CreateRectRgnIndirect)(&in); }
TEST(gap10_crri_cross_thread) {
    NEED_RGN();
    HRGN h = NULL; RECT box;
    g10_run_thread(crri_thr, &h);
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(U(GetRgnBox)(h, &box), SIMPLEREGION);
    EXPECT_EQ(box.right, 31); EXPECT_EQ(box.bottom, 42);
    CHECK(U(DeleteObject)(h));
}
TEST(gap10_crri_as_clip_region_pixels) {
    NEED_RGN(); NEED_MD(); REQUIRE(U(SelectClipRgn));
    MD m; RECT in = R(4, 4, 12, 12); HRGN h;
    if (!md_create(&m, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    h = U(CreateRectRgnIndirect)(&in);
    CHECK(U(SelectClipRgn)(m.dc, h) != 0);
    md_rect(&m, 0, 0, 16, 16, RED_);
    U(SelectClipRgn)(m.dc, NULL);
    EXPECT_EQ(md_count(&m, 0, 0, 16, 16, RED_), 64);
    EXPECT_EQ(md_px(&m, 4, 4), RED_); EXPECT_EQ(md_px(&m, 11, 11), RED_);
    EXPECT_EQ(md_px(&m, 3, 4), WHITE_); EXPECT_EQ(md_px(&m, 12, 11), WHITE_);
    U(DeleteObject)(h);
    md_destroy(&m);
}

/* ---------------- CreateEllipticRgnIndirect ---------------- */
#define NEED_ELL() do { REQUIRE(U(CreateEllipticRgnIndirect)); REQUIRE(U(CreateEllipticRgn)); REQUIRE(U(GetRgnBox)); \
    REQUIRE(U(DeleteObject)); REQUIRE(U(PtInRegion)); REQUIRE(U(EqualRgn)); REQUIRE(U(GetRegionData)); } while (0)

TEST(gap10_ceri_basic_box_type) {
    NEED_ELL();
    RECT in = R(0, 0, 100, 60), box; HRGN h = U(CreateEllipticRgnIndirect)(&in);
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(U(GetRgnBox)(h, &box), COMPLEXREGION);
    EXPECT_EQ(box.left, 0); EXPECT_EQ(box.top, 0);
    CHECK(box.right >= 99 && box.right <= 100); CHECK(box.bottom >= 59 && box.bottom <= 60);
    t_note("ellipse box right/bottom %ld,%ld for rect 100x60", box.right, box.bottom);
    U(DeleteObject)(h);
}
TEST(gap10_ceri_equal_to_direct) {
    NEED_ELL();
    int t[][4] = { {0,0,50,50}, {10,20,90,40}, {-30,-30,30,30}, {3,3,4,4}, {0,0,200,10} };
    unsigned i;
    for (i = 0; i < sizeof t / sizeof *t; i++) {
        RECT in = R(t[i][0], t[i][1], t[i][2], t[i][3]);
        HRGN a = U(CreateEllipticRgnIndirect)(&in), b = U(CreateEllipticRgn)(t[i][0], t[i][1], t[i][2], t[i][3]);
        CHECKF(a && b, "case %u", i);
        if (a && b) CHECKF(U(EqualRgn)(a, b), "case %u", i);
        if (a) U(DeleteObject)(a);
        if (b) U(DeleteObject)(b);
    }
}
TEST(gap10_ceri_hit_testing) {
    NEED_ELL();
    RECT in = R(0, 0, 100, 100); HRGN h = U(CreateEllipticRgnIndirect)(&in);
    CHECK(h != NULL);
    if (!h) return;
    CHECK(U(PtInRegion)(h, 50, 50));
    CHECK(U(PtInRegion)(h, 25, 50));
    CHECK(U(PtInRegion)(h, 50, 5));
    CHECK(!U(PtInRegion)(h, 0, 0));
    CHECK(!U(PtInRegion)(h, 99, 0));
    CHECK(!U(PtInRegion)(h, 0, 99));
    CHECK(!U(PtInRegion)(h, 99, 99));
    CHECK(!U(PtInRegion)(h, 100, 50));
    CHECK(!U(PtInRegion)(h, -1, 50));
    CHECK(!U(PtInRegion)(h, 50, 100));
    U(DeleteObject)(h);
}
TEST(gap10_ceri_symmetry) {
    NEED_ELL();
    RECT in = R(0, 0, 41, 41); HRGN h = U(CreateEllipticRgnIndirect)(&in); int x, y, asym = 0;
    CHECK(h != NULL);
    if (!h) return;
    for (y = 0; y < 41; y++) for (x = 0; x < 41; x++)
        if (!U(PtInRegion)(h, x, y) != !U(PtInRegion)(h, 40 - x, y) || !U(PtInRegion)(h, x, y) != !U(PtInRegion)(h, x, 40 - y)) asym++;
    t_note("odd-size ellipse asymmetric pixels: %d", asym);
    U(DeleteObject)(h);
}
TEST(gap10_ceri_area_close_to_ideal) {
    NEED_ELL();
    RECT in = R(0, 0, 80, 80); HRGN h = U(CreateEllipticRgnIndirect)(&in); long a; int n;
    CHECK(h != NULL);
    if (!h) return;
    a = rgn_area(h, &n);
    CHECKF(a > 4800 && a < 5200, "area %ld (ideal 5027)", a);
    CHECKF(n >= 40, "rect count %d", n);
    U(DeleteObject)(h);
}
TEST(gap10_ceri_degenerate_observed) {
    NEED_ELL();
    int t[][4] = { {5,5,5,5}, {0,0,1,1}, {0,0,0,10}, {0,0,10,0}, {0,0,2,2}, {10,10,0,0}, {0,0,1,100} };
    unsigned i;
    for (i = 0; i < sizeof t / sizeof *t; i++) {
        RECT in = R(t[i][0], t[i][1], t[i][2], t[i][3]), box;
        HRGN h = U(CreateEllipticRgnIndirect)(&in);
        if (!h) { t_note("degenerate %u: NULL", i); continue; }
        int ty = U(GetRgnBox)(h, &box);
        t_note("degenerate %u: type %d box %ld,%ld,%ld,%ld", i, ty, box.left, box.top, box.right, box.bottom);
        U(DeleteObject)(h);
    }
}
TEST(gap10_ceri_reversed_equals_normal) {
    NEED_ELL();
    RECT a = R(10, 10, 70, 50), b = R(70, 50, 10, 10);
    HRGN h1 = U(CreateEllipticRgnIndirect)(&a), h2 = U(CreateEllipticRgnIndirect)(&b);
    CHECK(h1 && h2);
    if (h1 && h2) {
        RECT bx;
        U(GetRgnBox)(h2, &bx);
        EXPECT_EQ(bx.left, 10); EXPECT_EQ(bx.top, 10); CHECK(bx.right >= 69 && bx.right <= 70); CHECK(bx.bottom >= 49 && bx.bottom <= 50);
        CHECK(U(EqualRgn)(h1, h2));
    }
    if (h1) U(DeleteObject)(h1);
    if (h2) U(DeleteObject)(h2);
}
TEST(gap10_ceri_input_unchanged_and_lifetime) {
    NEED_ELL(); REQUIRE(U(GetObjectType));
    RECT in = R(9, 8, 7, 6), copy = in; HRGN h = U(CreateEllipticRgnIndirect)(&in);
    CHECK(h != NULL);
    CHECK(!memcmp(&in, &copy, sizeof in));
    if (!h) return;
    EXPECT_EQ(U(GetObjectType)(h), OBJ_REGION);
    CHECK(U(DeleteObject)(h));
    CHECK(!U(DeleteObject)(h));
}
TEST(gap10_ceri_fill_pixels_symmetric_bounds) {
    NEED_ELL(); NEED_MD(); REQUIRE(U(FillRgn));
    MD m; RECT in = R(2, 2, 34, 34); HRGN h; HBRUSH b; int n;
    if (!md_create(&m, 40, 40, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    h = U(CreateEllipticRgnIndirect)(&in);
    b = U(CreateSolidBrush)(BLUE_);
    CHECK(U(FillRgn)(m.dc, h, b));
    n = md_count(&m, 0, 0, 40, 40, BLUE_);
    CHECKF(n > 700 && n < 850, "filled %d (ideal 804)", n);
    EXPECT_EQ(md_px(&m, 18, 18), BLUE_);
    EXPECT_EQ(md_px(&m, 2, 2), WHITE_);
    EXPECT_EQ(md_px(&m, 33, 33), WHITE_);
    EXPECT_EQ(md_px(&m, 0, 0), WHITE_);
    EXPECT_EQ(md_px(&m, 39, 39), WHITE_);
    EXPECT_EQ(md_count(&m, 0, 0, 40, 2, BLUE_), 0);
    U(DeleteObject)(b); U(DeleteObject)(h);
    md_destroy(&m);
}
TEST(gap10_ceri_no_gdi_leak) {
    NEED_ELL();
    if (!g10_gdicount()) { t_skip("no GetGuiResources"); return; }
    DWORD before = g10_gdicount(); HRGN h[100]; int i; RECT in = R(0, 0, 33, 22);
    for (i = 0; i < 100; i++) h[i] = U(CreateEllipticRgnIndirect)(&in);
    for (i = 0; i < 100; i++) if (h[i]) U(DeleteObject)(h[i]);
    EXPECT_EQ(g10_gdicount(), before);
}
static void ceri_thr(void *p) { RECT in = R(0, 0, 20, 20); *(HRGN *)p = U(CreateEllipticRgnIndirect)(&in); }
TEST(gap10_ceri_cross_thread) {
    NEED_ELL();
    HRGN h = NULL;
    g10_run_thread(ceri_thr, &h);
    CHECK(h != NULL);
    if (h) { CHECK(U(PtInRegion)(h, 10, 10)); CHECK(U(DeleteObject)(h)); }
}

/* ---------------- CreatePenIndirect ---------------- */
#define NEED_PEN() do { REQUIRE(U(CreatePenIndirect)); REQUIRE(U(GetObjectW)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetStockObject)); } while (0)
static HPEN mkpen(int style, int w, COLORREF c) {
    LOGPEN lp; lp.lopnStyle = style; lp.lopnWidth.x = w; lp.lopnWidth.y = 0; lp.lopnColor = c;
    return U(CreatePenIndirect)(&lp);
}
TEST(gap10_cpi_solid_roundtrip) {
    NEED_PEN();
    COLORREF cs[] = { 0, 0xFFFFFF, 0x0000FF, 0x00FF00, 0xFF0000, 0x123456, 0xFEDCBA }; unsigned i;
    for (i = 0; i < sizeof cs / sizeof *cs; i++) {
        HPEN p = mkpen(PS_SOLID, 3, cs[i]); LOGPEN out;
        CHECKF(p != NULL, "color %06lx", cs[i]);
        if (!p) continue;
        memset(&out, 0xCC, sizeof out);
        EXPECT_EQ(U(GetObjectW)(p, sizeof out, &out), sizeof(LOGPEN));
        EXPECT_EQ(out.lopnStyle, PS_SOLID);
        EXPECT_EQ(out.lopnWidth.x, 3);
        EXPECT_EQ(out.lopnColor, cs[i]);
        U(DeleteObject)(p);
    }
}
TEST(gap10_cpi_styles_roundtrip) {
    NEED_PEN();
    int st[] = { PS_SOLID, PS_DASH, PS_DOT, PS_DASHDOT, PS_DASHDOTDOT, PS_INSIDEFRAME }; unsigned i;
    for (i = 0; i < sizeof st / sizeof *st; i++) {
        HPEN p = mkpen(st[i], 1, 0x112233); LOGPEN out;
        CHECKF(p != NULL, "style %d", st[i]);
        if (!p) continue;
        memset(&out, 0, sizeof out);
        U(GetObjectW)(p, sizeof out, &out);
        CHECKF(out.lopnStyle == (UINT)st[i], "style %d -> %u", st[i], out.lopnStyle);
        EXPECT_EQ(out.lopnColor, 0x112233);
        U(DeleteObject)(p);
    }
}
TEST(gap10_cpi_null_style_is_stock_pen) {
    NEED_PEN();
    HPEN p = mkpen(PS_NULL, 1, 0x00FF00);
    CHECK(p != NULL);
    EXPECT_EQ(p, U(GetStockObject)(NULL_PEN));
    EXPECT_EQ(mkpen(PS_NULL, 77, 0x1), U(GetStockObject)(NULL_PEN));
    if (p) { LOGPEN out; U(GetObjectW)(p, sizeof out, &out); EXPECT_EQ(out.lopnStyle, PS_NULL); }
}
TEST(gap10_cpi_invalid_styles_become_solid) {
    NEED_PEN();
    int st[] = { 7, 8, 9, 100, 0x100, -1, -100, INT_MIN, INT_MAX }; unsigned i;
    for (i = 0; i < sizeof st / sizeof *st; i++) {
        HPEN p = mkpen(st[i], 2, 0x0000FF); LOGPEN out;
        CHECKF(p != NULL, "style %d", st[i]);
        if (!p) continue;
        memset(&out, 0xCC, sizeof out);
        U(GetObjectW)(p, sizeof out, &out);
        CHECKF(out.lopnStyle == PS_SOLID, "style %d -> %u", st[i], out.lopnStyle);
        U(DeleteObject)(p);
    }
}
TEST(gap10_cpi_width_variants) {
    NEED_PEN();
    int w[] = { 0, 1, 2, 5, 16, 100, 1000 }; unsigned i;
    for (i = 0; i < sizeof w / sizeof *w; i++) {
        HPEN p = mkpen(PS_SOLID, w[i], 0x808080); LOGPEN out;
        CHECKF(p != NULL, "width %d", w[i]);
        if (!p) continue;
        memset(&out, 0xCC, sizeof out);
        U(GetObjectW)(p, sizeof out, &out);
        CHECKF(out.lopnWidth.x == w[i], "width %d -> %ld", w[i], out.lopnWidth.x);
        U(DeleteObject)(p);
    }
}
TEST(gap10_cpi_negative_width_observed) {
    NEED_PEN();
    int w[] = { -1, -5, INT_MIN }; unsigned i;
    for (i = 0; i < sizeof w / sizeof *w; i++) {
        HPEN p = mkpen(PS_SOLID, w[i], 0x808080); LOGPEN out;
        if (!p) { t_note("width %d: NULL", w[i]); continue; }
        memset(&out, 0, sizeof out);
        U(GetObjectW)(p, sizeof out, &out);
        t_note("width %d -> %ld", w[i], out.lopnWidth.x);
        U(DeleteObject)(p);
    }
}
TEST(gap10_cpi_width_y_ignored) {
    NEED_PEN();
    LOGPEN lp, out; HPEN p;
    lp.lopnStyle = PS_SOLID; lp.lopnWidth.x = 4; lp.lopnWidth.y = 99; lp.lopnColor = 0x445566;
    p = U(CreatePenIndirect)(&lp);
    CHECK(p != NULL);
    if (!p) return;
    memset(&out, 0xCC, sizeof out);
    U(GetObjectW)(p, sizeof out, &out);
    EXPECT_EQ(out.lopnWidth.x, 4);
    t_note("lopnWidth.y reported as %ld", out.lopnWidth.y);
    U(DeleteObject)(p);
}
TEST(gap10_cpi_matches_createpen) {
    NEED_PEN(); REQUIRE(U(CreatePen));
    int st[] = { PS_SOLID, PS_DASH, PS_DOT, PS_INSIDEFRAME }; unsigned i;
    for (i = 0; i < sizeof st / sizeof *st; i++) {
        HPEN a = mkpen(st[i], 1, 0x0A0B0C), b = U(CreatePen)(st[i], 1, 0x0A0B0C); LOGPEN oa, ob;
        CHECK(a && b);
        if (a && b) {
            memset(&oa, 1, sizeof oa); memset(&ob, 2, sizeof ob);
            U(GetObjectW)(a, sizeof oa, &oa); U(GetObjectW)(b, sizeof ob, &ob);
            CHECK(oa.lopnStyle == ob.lopnStyle && oa.lopnWidth.x == ob.lopnWidth.x && oa.lopnColor == ob.lopnColor);
        }
        if (a) U(DeleteObject)(a);
        if (b) U(DeleteObject)(b);
    }
}
TEST(gap10_cpi_object_type_and_select) {
    NEED_PEN(); NEED_MD(); REQUIRE(U(GetObjectType)); REQUIRE(U(GetCurrentObject));
    HPEN p = mkpen(PS_SOLID, 2, 0x010203);
    HDC dc = U(CreateCompatibleDC)(NULL); HGDIOBJ old;
    CHECK(p != NULL);
    if (!p) { U(DeleteDC)(dc); return; }
    EXPECT_EQ(U(GetObjectType)(p), OBJ_PEN);
    old = U(SelectObject)(dc, p);
    CHECK(old != NULL);
    EXPECT_EQ(U(GetCurrentObject)(dc, OBJ_PEN), p);
    U(SelectObject)(dc, old);
    U(DeleteDC)(dc);
    CHECK(U(DeleteObject)(p));
}
TEST(gap10_cpi_extpen_getobject_size) {
    NEED_PEN();
    HPEN p = mkpen(PS_SOLID, 1, 0);
    EXTLOGPEN ex;
    CHECK(p != NULL);
    if (!p) return;
    /* a plain pen reports a LOGPEN, never an EXTLOGPEN */
    EXPECT_EQ(U(GetObjectW)(p, sizeof ex, &ex), sizeof(LOGPEN));
    EXPECT_EQ(U(GetObjectW)(p, 0, NULL), sizeof(LOGPEN));
    U(DeleteObject)(p);
}
TEST(gap10_cpi_delete_twice) {
    NEED_PEN();
    HPEN p = mkpen(PS_DASH, 1, 0);
    CHECK(p != NULL);
    if (!p) return;
    CHECK(U(DeleteObject)(p));
    CHECK(!U(DeleteObject)(p));
    LOGPEN out;
    EXPECT_EQ(U(GetObjectW)(p, sizeof out, &out), 0);
}
TEST(gap10_cpi_stock_pens_not_deletable_observed) {
    NEED_PEN();
    HPEN np = mkpen(PS_NULL, 0, 0);
    BOOL r = U(DeleteObject)(np);
    t_note("DeleteObject(stock NULL_PEN) -> %d", r);
    LOGPEN out;
    EXPECT_EQ(U(GetObjectW)(U(GetStockObject)(NULL_PEN), sizeof out, &out), sizeof(LOGPEN));
}
TEST(gap10_cpi_draw_horizontal_line_pixels) {
    NEED_PEN(); NEED_MD(); REQUIRE(U(MoveToEx)); REQUIRE(U(LineTo));
    int bpps[] = { 32, 24, 16 }; unsigned k;
    for (k = 0; k < 3; k++) {
        MD m; HPEN p, op; int x;
        if (!md_create(&m, 32, 16, bpps[k])) { t_note("no %d bpp DIB", bpps[k]); continue; }
        md_clear(&m, WHITE_);
        p = mkpen(PS_SOLID, 1, RED_);
        op = U(SelectObject)(m.dc, p);
        U(MoveToEx)(m.dc, 4, 8, NULL);
        CHECK(U(LineTo)(m.dc, 20, 8));
        for (x = 4; x < 20; x++) CHECKF(md_px(&m, x, 8) == RED_, "bpp %d x=%d", bpps[k], x);
        CHECKF(md_px(&m, 20, 8) == WHITE_, "bpp %d endpoint excluded", bpps[k]);
        CHECKF(md_px(&m, 3, 8) == WHITE_, "bpp %d before start", bpps[k]);
        CHECKF(md_px(&m, 10, 7) == WHITE_ && md_px(&m, 10, 9) == WHITE_, "bpp %d neighbours", bpps[k]);
        CHECKF(md_count(&m, 0, 0, 32, 16, RED_) == 16, "bpp %d count %d", bpps[k], md_count(&m, 0, 0, 32, 16, RED_));
        U(SelectObject)(m.dc, op); U(DeleteObject)(p);
        md_destroy(&m);
    }
}
TEST(gap10_cpi_null_pen_draws_nothing) {
    NEED_PEN(); NEED_MD(); REQUIRE(U(MoveToEx)); REQUIRE(U(LineTo));
    MD m; HPEN p, op;
    if (!md_create(&m, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    p = mkpen(PS_NULL, 1, RED_);
    op = U(SelectObject)(m.dc, p);
    U(MoveToEx)(m.dc, 0, 0, NULL); U(LineTo)(m.dc, 15, 15);
    EXPECT_EQ(md_diff(&m, WHITE_), 0);
    U(SelectObject)(m.dc, op);
    md_destroy(&m);
}
TEST(gap10_cpi_wide_pen_center_row) {
    NEED_PEN(); NEED_MD(); REQUIRE(U(MoveToEx)); REQUIRE(U(LineTo));
    MD m; HPEN p, op; int x;
    if (!md_create(&m, 40, 20, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    p = mkpen(PS_SOLID, 5, BLUE_);
    op = U(SelectObject)(m.dc, p);
    U(MoveToEx)(m.dc, 8, 10, NULL); U(LineTo)(m.dc, 30, 10);
    for (x = 12; x < 26; x++) { CHECKF(md_px(&m, x, 10) == BLUE_, "x=%d", x); CHECKF(md_px(&m, x, 8) == BLUE_, "x=%d", x); CHECKF(md_px(&m, x, 12) == BLUE_, "x=%d", x); }
    EXPECT_EQ(md_px(&m, 15, 5), WHITE_); EXPECT_EQ(md_px(&m, 15, 15), WHITE_);
    EXPECT_EQ(md_px(&m, 15, 7), WHITE_); EXPECT_EQ(md_px(&m, 15, 13), WHITE_);
    U(SelectObject)(m.dc, op); U(DeleteObject)(p);
    md_destroy(&m);
}
TEST(gap10_cpi_dot_pen_leaves_gaps) {
    NEED_PEN(); NEED_MD(); REQUIRE(U(MoveToEx)); REQUIRE(U(LineTo)); REQUIRE(U(SetBkMode));
    MD m; HPEN p, op; int n;
    if (!md_create(&m, 64, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    U(SetBkMode)(m.dc, TRANSPARENT);
    p = mkpen(PS_DOT, 1, BLACK_);
    op = U(SelectObject)(m.dc, p);
    U(MoveToEx)(m.dc, 0, 4, NULL); U(LineTo)(m.dc, 60, 4);
    n = md_count(&m, 0, 4, 64, 5, BLACK_);
    CHECKF(n > 5 && n < 60, "dotted pixel count %d", n);
    EXPECT_EQ(md_count(&m, 0, 0, 64, 4, BLACK_), 0);
    EXPECT_EQ(md_count(&m, 0, 5, 64, 8, BLACK_), 0);
    U(SelectObject)(m.dc, op); U(DeleteObject)(p);
    md_destroy(&m);
}
TEST(gap10_cpi_no_gdi_leak) {
    NEED_PEN();
    if (!g10_gdicount()) { t_skip("no GetGuiResources"); return; }
    DWORD before = g10_gdicount(); HPEN h[300]; int i;
    for (i = 0; i < 300; i++) h[i] = mkpen(PS_SOLID, 1 + (i & 3), i);
    for (i = 0; i < 300; i++) if (h[i]) U(DeleteObject)(h[i]);
    EXPECT_EQ(g10_gdicount(), before);
}
static void cpi_thr(void *p) { *(HPEN *)p = mkpen(PS_SOLID, 6, 0xABCDEF); }
TEST(gap10_cpi_cross_thread) {
    NEED_PEN();
    HPEN h = NULL; LOGPEN out;
    g10_run_thread(cpi_thr, &h);
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(U(GetObjectW)(h, sizeof out, &out), sizeof out);
    EXPECT_EQ(out.lopnWidth.x, 6); EXPECT_EQ(out.lopnColor, 0xABCDEF);
    CHECK(U(DeleteObject)(h));
}
