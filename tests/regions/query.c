#include "rg.h"

/* ---- GetRgnBox return codes ---- */
TEST(regions_box_codes) {
    REQUIRE(U(GetRgnBox)); REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN s = rg_rect(0, 0, 10, 10), e = rg_rect(3, 3, 3, 3), c = rg_rect(0, 0, 10, 10), o = rg_rect(20, 20, 30, 30);
    U(CombineRgn)(c, c, o, RGN_OR);
    RECT rc;
    CHECK(U(GetRgnBox)(s, &rc) == SIMPLEREGION);
    CHECK(U(GetRgnBox)(e, &rc) == NULLREGION);
    CHECK(U(GetRgnBox)(c, &rc) == COMPLEXREGION);
    CHECK(U(GetRgnBox)(NULL, &rc) == 0);   /* ERROR */
    CHECK(SIMPLEREGION == 2 && COMPLEXREGION == 3 && NULLREGION == 1 && ERROR == 0);
    U(DeleteObject)(s); U(DeleteObject)(e); U(DeleteObject)(c); U(DeleteObject)(o);
}
TEST(regions_box_error_leaves_rect) {
    REQUIRE(U(GetRgnBox));
    RECT rc = {11, 22, 33, 44};
    CHECK(U(GetRgnBox)(NULL, &rc) == 0);
    t_note("rect after failed GetRgnBox: %ld,%ld,%ld,%ld", rc.left, rc.top, rc.right, rc.bottom);
}
TEST(regions_box_after_combine_shrinks) {
    REQUIRE(U(GetRgnBox)); REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN a = rg_rect(0, 0, 100, 100), b = rg_rect(50, 50, 200, 200);
    RECT rc;
    CHECK(U(CombineRgn)(a, a, b, RGN_AND) == SIMPLEREGION);
    U(GetRgnBox)(a, &rc);
    CHECK(rc.left == 50 && rc.top == 50 && rc.right == 100 && rc.bottom == 100);
    U(DeleteObject)(a); U(DeleteObject)(b);
}
TEST(regions_box_repeat_stable) {
    REQUIRE(U(GetRgnBox)); REQUIRE(U(DeleteObject));
    HRGN h = U(CreateEllipticRgn)(3, 4, 53, 44);
    RECT first, rc;
    U(GetRgnBox)(h, &first);
    for (int i = 0; i < 200; i++) {
        CHECK(U(GetRgnBox)(h, &rc) == COMPLEXREGION);
        if (memcmp(&rc, &first, sizeof rc)) { CHECK(0); break; }
    }
    U(DeleteObject)(h);
}

/* ---- PtInRegion ---- */
TEST(regions_ptin_rect_boundaries) {
    REQUIRE(U(PtInRegion)); REQUIRE(U(DeleteObject));
    HRGN h = rg_rect(10, 20, 30, 50);
    CHECK(U(PtInRegion)(h, 10, 20));      /* left/top inclusive */
    CHECK(U(PtInRegion)(h, 29, 49));      /* last pixel */
    CHECK(!U(PtInRegion)(h, 30, 49));     /* right exclusive */
    CHECK(!U(PtInRegion)(h, 29, 50));     /* bottom exclusive */
    CHECK(!U(PtInRegion)(h, 30, 50));
    CHECK(!U(PtInRegion)(h, 9, 20));
    CHECK(!U(PtInRegion)(h, 10, 19));
    CHECK(!U(PtInRegion)(h, 9, 19));
    CHECK(U(PtInRegion)(h, 10, 49) && U(PtInRegion)(h, 29, 20));
    CHECK(!U(PtInRegion)(h, -1, -1) && !U(PtInRegion)(h, 0x7FFFFFFF, 0x7FFFFFFF) && !U(PtInRegion)(h, (int)0x80000000, (int)0x80000000));
    U(DeleteObject)(h);
}
TEST(regions_ptin_negative_rect) {
    REQUIRE(U(PtInRegion)); REQUIRE(U(DeleteObject));
    HRGN h = rg_rect(-10, -10, 0, 0);
    CHECK(U(PtInRegion)(h, -10, -10) && U(PtInRegion)(h, -1, -1));
    CHECK(!U(PtInRegion)(h, 0, 0) && !U(PtInRegion)(h, -11, -5) && !U(PtInRegion)(h, -5, 0));
    U(DeleteObject)(h);
}
TEST(regions_ptin_unit_rect) {
    REQUIRE(U(PtInRegion)); REQUIRE(U(DeleteObject));
    HRGN h = rg_rect(7, 7, 8, 8);
    int n = 0;
    for (int y = 5; y < 10; y++) for (int x = 5; x < 10; x++) if (U(PtInRegion)(h, x, y)) { n++; CHECK(x == 7 && y == 7); }
    CHECK(n == 1);
    U(DeleteObject)(h);
}
TEST(regions_ptin_empty) {
    REQUIRE(U(PtInRegion)); REQUIRE(U(DeleteObject));
    HRGN h = rg_rect(0, 0, 0, 0);
    for (int y = -2; y < 3; y++) for (int x = -2; x < 3; x++) CHECK(!U(PtInRegion)(h, x, y));
    U(DeleteObject)(h);
}
TEST(regions_ptin_complex_hole) {
    REQUIRE(U(PtInRegion)); REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN a = rg_rect(0, 0, 30, 30), b = rg_rect(10, 10, 20, 20);
    CHECK(U(CombineRgn)(a, a, b, RGN_DIFF) == COMPLEXREGION);
    CHECK(U(PtInRegion)(a, 0, 0) && U(PtInRegion)(a, 29, 29) && U(PtInRegion)(a, 9, 15) && U(PtInRegion)(a, 20, 15));
    CHECK(!U(PtInRegion)(a, 10, 10) && !U(PtInRegion)(a, 19, 19) && !U(PtInRegion)(a, 15, 15));
    CHECK(U(PtInRegion)(a, 15, 9) && U(PtInRegion)(a, 15, 20));
    U(DeleteObject)(a); U(DeleteObject)(b);
}
TEST(regions_ptin_ellipse_center_corner) {
    REQUIRE(U(PtInRegion)); REQUIRE(U(CreateEllipticRgn)); REQUIRE(U(DeleteObject));
    HRGN h = U(CreateEllipticRgn)(0, 0, 100, 50);
    CHECK(U(PtInRegion)(h, 50, 25) && U(PtInRegion)(h, 10, 25) && U(PtInRegion)(h, 90, 25) && U(PtInRegion)(h, 50, 2) && U(PtInRegion)(h, 50, 47));
    CHECK(!U(PtInRegion)(h, 0, 0) && !U(PtInRegion)(h, 99, 0) && !U(PtInRegion)(h, 0, 49) && !U(PtInRegion)(h, 99, 49));
    CHECK(!U(PtInRegion)(h, 5, 5) && !U(PtInRegion)(h, 95, 45));
    U(DeleteObject)(h);
}

/* ---- RectInRegion ---- */
typedef struct { RECT r; int expect; } RirCase;
TEST(regions_rectin_table) {
    REQUIRE(U(RectInRegion)); REQUIRE(U(DeleteObject));
    HRGN h = rg_rect(10, 10, 20, 20);
    static const RirCase c[] = {
        {{10, 10, 20, 20}, 1}, {{0, 0, 100, 100}, 1}, {{12, 12, 14, 14}, 1}, {{19, 19, 40, 40}, 1},
        {{0, 0, 11, 11}, 1}, {{19, 0, 40, 11}, 1},
        {{0, 0, 10, 10}, 0}, {{20, 20, 30, 30}, 0}, {{0, 10, 10, 20}, 0}, {{20, 10, 30, 20}, 0},
        {{10, 0, 20, 10}, 0}, {{10, 20, 20, 30}, 0}, {{0, 0, 5, 5}, 0}, {{50, 50, 60, 60}, 0},
        {{-100, -100, -50, -50}, 0},
    };
    for (unsigned i = 0; i < sizeof c / sizeof *c; i++)
        CHECKF((U(RectInRegion)(h, &c[i].r) != 0) == c[i].expect, "case %u rect %ld,%ld,%ld,%ld", i, c[i].r.left, c[i].r.top, c[i].r.right, c[i].r.bottom);
    U(DeleteObject)(h);
}
TEST(regions_rectin_empty_rects) {
    REQUIRE(U(RectInRegion)); REQUIRE(U(DeleteObject));
    HRGN h = rg_rect(10, 10, 20, 20);
    RECT e1 = {15, 15, 15, 15}, e2 = {15, 10, 15, 20}, e3 = {10, 15, 20, 15};
    t_note("RectInRegion empty rects: %d %d %d", U(RectInRegion)(h, &e1), U(RectInRegion)(h, &e2), U(RectInRegion)(h, &e3));
    CHECK(!U(RectInRegion)(h, &e1));
    U(DeleteObject)(h);
}
TEST(regions_rectin_inverted_rects) {
    REQUIRE(U(RectInRegion)); REQUIRE(U(DeleteObject));
    HRGN h = rg_rect(10, 10, 20, 20);
    RECT in = {18, 18, 12, 12}, out = {5, 5, 0, 0}, half = {30, 15, 15, 12};
    t_note("RectInRegion inverted overlap=%d disjoint=%d half=%d", U(RectInRegion)(h, &in), U(RectInRegion)(h, &out), U(RectInRegion)(h, &half));
    CHECK(!U(RectInRegion)(h, &out));
    U(DeleteObject)(h);
}
TEST(regions_rectin_complex) {
    REQUIRE(U(RectInRegion)); REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN a = rg_rect(0, 0, 30, 30), b = rg_rect(10, 10, 20, 20);
    U(CombineRgn)(a, a, b, RGN_DIFF);
    RECT hole = {10, 10, 20, 20}, hole_in = {12, 12, 18, 18}, edge = {5, 5, 15, 15}, ring = {0, 0, 30, 30};
    CHECK(!U(RectInRegion)(a, &hole));
    CHECK(!U(RectInRegion)(a, &hole_in));
    CHECK(U(RectInRegion)(a, &edge));
    CHECK(U(RectInRegion)(a, &ring));
    U(DeleteObject)(a); U(DeleteObject)(b);
}
TEST(regions_rectin_two_islands_gap) {
    REQUIRE(U(RectInRegion)); REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN a = rg_rect(0, 0, 10, 10), b = rg_rect(20, 0, 30, 10);
    U(CombineRgn)(a, a, b, RGN_OR);
    RECT gap = {10, 0, 20, 10}, over = {9, 0, 21, 10};
    CHECK(!U(RectInRegion)(a, &gap));
    CHECK(U(RectInRegion)(a, &over));
    U(DeleteObject)(a); U(DeleteObject)(b);
}
TEST(regions_rectin_empty_region) {
    REQUIRE(U(RectInRegion)); REQUIRE(U(DeleteObject));
    HRGN h = rg_rect(0, 0, 0, 0);
    RECT r = {-100, -100, 100, 100};
    CHECK(!U(RectInRegion)(h, &r));
    U(DeleteObject)(h);
}
TEST(regions_rectin_vs_ptin_consistency) {
    REQUIRE(U(RectInRegion)); REQUIRE(U(PtInRegion)); REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    rg_seed = 99;
    for (int it = 0; it < 30; it++) {
        HRGN h = rg_rect(0, 0, 0, 0), t = rg_rect(0, 0, 0, 0);
        for (int k = 0; k < 3; k++) {
            int l = rg_rand() % 50, tp = rg_rand() % 50;
            U(SetRectRgn)(t, l, tp, l + 1 + rg_rand() % 14, tp + 1 + rg_rand() % 14);
            U(CombineRgn)(h, h, t, RGN_OR);
        }
        RgGrid g; rg_grid_of(h, &g);
        for (int q = 0; q < 20; q++) {
            int l = rg_rand() % 60, tp = rg_rand() % 60, r = l + 1 + rg_rand() % 8, b = tp + 1 + rg_rand() % 8;
            RECT rc = {l, tp, r, b};
            int any = 0;
            for (int y = tp; y < b; y++) for (int x = l; x < r; x++) if (x < RG_N && y < RG_N && g.c[y][x]) any = 1;
            CHECKF((U(RectInRegion)(h, &rc) != 0) == any, "it %d q %d", it, q);
        }
        U(DeleteObject)(h); U(DeleteObject)(t);
    }
}

/* ---- OffsetRgn ---- */
TEST(regions_offset_basic) {
    REQUIRE(U(OffsetRgn)); REQUIRE(U(GetRgnBox)); REQUIRE(U(DeleteObject));
    HRGN h = rg_rect(10, 10, 20, 20);
    RECT rc;
    CHECK(U(OffsetRgn)(h, 5, -3) == SIMPLEREGION);
    U(GetRgnBox)(h, &rc);
    CHECK(rc.left == 15 && rc.top == 7 && rc.right == 25 && rc.bottom == 17);
    CHECK(U(OffsetRgn)(h, 0, 0) == SIMPLEREGION);
    U(GetRgnBox)(h, &rc);
    CHECK(rc.left == 15 && rc.top == 7);
    CHECK(U(OffsetRgn)(h, -100, -100) == SIMPLEREGION);
    U(GetRgnBox)(h, &rc);
    CHECK(rc.left == -85 && rc.top == -93 && rc.right == -75 && rc.bottom == -83);
    CHECK(U(PtInRegion)(h, -85, -93) && !U(PtInRegion)(h, -75, -93));
    U(DeleteObject)(h);
}
TEST(regions_offset_empty) {
    REQUIRE(U(OffsetRgn)); REQUIRE(U(DeleteObject));
    HRGN h = rg_rect(5, 5, 5, 5);
    CHECK(U(OffsetRgn)(h, 10, 10) == NULLREGION);
    CHECK(rg_type(h) == NULLREGION);
    U(DeleteObject)(h);
}
TEST(regions_offset_complex) {
    REQUIRE(U(OffsetRgn)); REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN a = rg_rect(0, 0, 10, 10), b = rg_rect(20, 20, 30, 30);
    U(CombineRgn)(a, a, b, RGN_OR);
    CHECK(U(OffsetRgn)(a, 7, 9) == COMPLEXREGION);
    CHECK(U(PtInRegion)(a, 7, 9) && U(PtInRegion)(a, 27, 29) && !U(PtInRegion)(a, 0, 0) && !U(PtInRegion)(a, 20, 20));
    CHECK(U(OffsetRgn)(a, -7, -9) == COMPLEXREGION);
    CHECK(U(PtInRegion)(a, 0, 0) && U(PtInRegion)(a, 29, 29));
    U(DeleteObject)(a); U(DeleteObject)(b);
}
TEST(regions_offset_roundtrip_equal) {
    REQUIRE(U(OffsetRgn)); REQUIRE(U(EqualRgn)); REQUIRE(U(DeleteObject));
    HRGN a = U(CreateEllipticRgn)(0, 0, 40, 30), b = U(CreateEllipticRgn)(0, 0, 40, 30);
    CHECK(U(OffsetRgn)(a, 1000, -2000) != 0);
    CHECK(!U(EqualRgn)(a, b));
    CHECK(U(OffsetRgn)(a, -1000, 2000) != 0);
    CHECK(U(EqualRgn)(a, b));
    U(DeleteObject)(a); U(DeleteObject)(b);
}
TEST(regions_offset_shape_preserved) {
    REQUIRE(U(OffsetRgn)); REQUIRE(U(CreateEllipticRgn)); REQUIRE(U(DeleteObject));
    HRGN a = U(CreateEllipticRgn)(0, 0, 30, 20), b = U(CreateEllipticRgn)(0, 0, 30, 20);
    U(OffsetRgn)(b, 17, 13);
    int bad = 0;
    for (int y = 0; y < 20; y++) for (int x = 0; x < 30; x++) if (!!U(PtInRegion)(a, x, y) != !!U(PtInRegion)(b, x + 17, y + 13)) bad++;
    CHECK(bad == 0);
    U(DeleteObject)(a); U(DeleteObject)(b);
}
TEST(regions_offset_large) {
    REQUIRE(U(OffsetRgn)); REQUIRE(U(DeleteObject));
    HRGN h = rg_rect(0, 0, 10, 10);
    int r = U(OffsetRgn)(h, 1000000, 1000000);
    CHECK(r == SIMPLEREGION);
    RECT rc; U(GetRgnBox)(h, &rc);
    CHECK(rc.left == 1000000 && rc.right == 1000010);
    U(DeleteObject)(h);
}
TEST(regions_offset_overflow) {
    REQUIRE(U(OffsetRgn)); REQUIRE(U(DeleteObject));
    HRGN h = rg_rect(0, 0, 10, 10);
    SET_ERR(0);
    int r = U(OffsetRgn)(h, 0x7FFFFFF0, 0);
    t_note("OffsetRgn by 0x7FFFFFF0 -> %d, err %lu", r, GetLastError());
    RECT rc; int t = U(GetRgnBox)(h, &rc);
    t_note("after: type %d box %ld,%ld,%ld,%ld", t, rc.left, rc.top, rc.right, rc.bottom);
    U(DeleteObject)(h);
    h = rg_rect(0, 0, 10, 10);
    r = U(OffsetRgn)(h, (int)0x80000000, (int)0x80000000);
    t_note("OffsetRgn by INT_MIN -> %d", r);
    U(DeleteObject)(h);
}
TEST(regions_offset_many_steps) {
    REQUIRE(U(OffsetRgn)); REQUIRE(U(DeleteObject));
    HRGN h = rg_rect(0, 0, 4, 4);
    for (int i = 0; i < 1000; i++) CHECK(U(OffsetRgn)(h, 3, -2) == SIMPLEREGION);
    RECT rc; U(GetRgnBox)(h, &rc);
    CHECK(rc.left == 3000 && rc.top == -2000 && rc.right == 3004 && rc.bottom == -1996);
    U(DeleteObject)(h);
}

/* ---- EqualRgn ---- */
TEST(regions_equal_basic) {
    REQUIRE(U(EqualRgn)); REQUIRE(U(DeleteObject));
    HRGN a = rg_rect(0, 0, 10, 10), b = rg_rect(0, 0, 10, 10), c = rg_rect(0, 0, 10, 11), d = rg_rect(1, 0, 11, 10);
    CHECK(U(EqualRgn)(a, b));
    CHECK(U(EqualRgn)(a, a));
    CHECK(!U(EqualRgn)(a, c));
    CHECK(!U(EqualRgn)(a, d));
    CHECK(U(EqualRgn)(b, a));
    U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(c); U(DeleteObject)(d);
}
TEST(regions_equal_empty) {
    REQUIRE(U(EqualRgn)); REQUIRE(U(DeleteObject));
    HRGN a = rg_rect(0, 0, 0, 0), b = rg_rect(50, 50, 50, 90), c = rg_rect(0, 0, 1, 1);
    CHECK(U(EqualRgn)(a, b));
    CHECK(!U(EqualRgn)(a, c));
    U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(c);
}
TEST(regions_equal_different_construction) {
    REQUIRE(U(EqualRgn)); REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN a = rg_rect(0, 0, 20, 10), b = rg_rect(0, 0, 10, 10), c = rg_rect(10, 0, 20, 10);
    U(CombineRgn)(b, b, c, RGN_OR);
    CHECK(U(EqualRgn)(a, b));
    CHECK(rg_type(b) == SIMPLEREGION);
    HRGN d = rg_rect(0, 0, 20, 5), e = rg_rect(0, 5, 20, 10);
    U(CombineRgn)(d, d, e, RGN_OR);
    CHECK(U(EqualRgn)(a, d));
    HRGN f = rg_rect(0, 0, 20, 10), g = rg_rect(5, 3, 8, 6);
    U(CombineRgn)(f, f, g, RGN_DIFF);
    U(CombineRgn)(f, f, g, RGN_OR);
    CHECK(U(EqualRgn)(a, f));
    U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(c); U(DeleteObject)(d); U(DeleteObject)(e); U(DeleteObject)(f); U(DeleteObject)(g);
}
TEST(regions_equal_invalid) {
    REQUIRE(U(EqualRgn)); REQUIRE(U(DeleteObject));
    HRGN a = rg_rect(0, 0, 1, 1);
    CHECK(!U(EqualRgn)(NULL, a));
    CHECK(!U(EqualRgn)(a, NULL));
    CHECK(!U(EqualRgn)(NULL, NULL));
    U(DeleteObject)(a);
}
TEST(regions_equal_complex_same_box_different_shape) {
    REQUIRE(U(EqualRgn)); REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN a = rg_rect(0, 0, 10, 10), b = rg_rect(20, 20, 30, 30), c = rg_rect(0, 20, 10, 30), d = rg_rect(20, 0, 30, 10);
    U(CombineRgn)(a, a, b, RGN_OR);
    U(CombineRgn)(c, c, d, RGN_OR);
    CHECK(!U(EqualRgn)(a, c));
    U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(c); U(DeleteObject)(d);
}

/* ---- GetRegionData ---- */
TEST(regions_data_size_query) {
    REQUIRE(U(GetRegionData)); REQUIRE(U(DeleteObject));
    HRGN h = rg_rect(0, 0, 10, 10);
    DWORD n = U(GetRegionData)(h, 0, NULL);
    CHECK(n == sizeof(RGNDATAHEADER) + sizeof(RECT));
    CHECK(U(GetRegionData)(h, 12345, NULL) == n);
    CHECK(U(GetRegionData)(h, 1, NULL) == n);
    HRGN b = rg_rect(20, 20, 30, 30);
    U(CombineRgn)(h, h, b, RGN_OR);
    CHECK(U(GetRegionData)(h, 0, NULL) == sizeof(RGNDATAHEADER) + 2 * sizeof(RECT));
    U(DeleteObject)(h); U(DeleteObject)(b);
}
TEST(regions_data_empty_region) {
    REQUIRE(U(GetRegionData)); REQUIRE(U(DeleteObject));
    HRGN h = rg_rect(4, 4, 4, 4);
    DWORD n = U(GetRegionData)(h, 0, NULL);
    t_note("empty region data size %lu", n);
    CHECK(n >= sizeof(RGNDATAHEADER));
    if (n >= sizeof(RGNDATAHEADER)) {
        RGNDATA *d = (RGNDATA *)calloc(1, n + 16);
        DWORD r = U(GetRegionData)(h, n, d);
        CHECK(r != 0);
        CHECK(d->rdh.dwSize == sizeof(RGNDATAHEADER) && d->rdh.iType == RDH_RECTANGLES && d->rdh.nCount == 0);
        free(d);
    }
    U(DeleteObject)(h);
}
TEST(regions_data_header_fields) {
    REQUIRE(U(GetRegionData)); REQUIRE(U(DeleteObject));
    HRGN h = rg_rect(3, 4, 13, 24);
    DWORD sz; RGNDATA *d = rg_getdata(h, &sz);
    CHECK(d != NULL);
    if (d) {
        CHECK(sz == sizeof(RGNDATAHEADER) + sizeof(RECT));
        CHECK(d->rdh.dwSize == 32 && d->rdh.iType == RDH_RECTANGLES && d->rdh.nCount == 1);
        CHECK(d->rdh.rcBound.left == 3 && d->rdh.rcBound.top == 4 && d->rdh.rcBound.right == 13 && d->rdh.rcBound.bottom == 24);
        RECT *r = rg_rects(d);
        CHECK(r[0].left == 3 && r[0].top == 4 && r[0].right == 13 && r[0].bottom == 24);
        t_note("nRgnSize = %lu", d->rdh.nRgnSize);
        free(d);
    }
    U(DeleteObject)(h);
}
TEST(regions_data_buffer_too_small) {
    REQUIRE(U(GetRegionData)); REQUIRE(U(DeleteObject));
    HRGN h = rg_rect(0, 0, 10, 10);
    DWORD n = U(GetRegionData)(h, 0, NULL);
    unsigned char buf[256]; memset(buf, 0xCC, sizeof buf);
    CHECK(U(GetRegionData)(h, n - 1, (RGNDATA *)buf) == 0);
    CHECK(U(GetRegionData)(h, sizeof(RGNDATAHEADER), (RGNDATA *)buf) == 0);
    CHECK(U(GetRegionData)(h, 1, (RGNDATA *)buf) == 0);
    for (DWORD i = n; i < sizeof buf; i++) if (buf[i] != 0xCC) { CHECKF(0, "overrun at %lu", i); break; }
    memset(buf, 0xCC, sizeof buf);
    DWORD r = U(GetRegionData)(h, n, (RGNDATA *)buf);
    CHECK(r == n);
    for (DWORD i = n; i < sizeof buf; i++) if (buf[i] != 0xCC) { CHECKF(0, "overrun at %lu", i); break; }
    r = U(GetRegionData)(h, 200, (RGNDATA *)buf);
    t_note("GetRegionData with oversize count returns %lu (need %lu)", r, n);
    CHECK(r == n || r == 200);
    U(DeleteObject)(h);
}
TEST(regions_data_zero_count_with_buffer) {
    REQUIRE(U(GetRegionData)); REQUIRE(U(DeleteObject));
    HRGN h = rg_rect(0, 0, 10, 10);
    unsigned char buf[128]; memset(buf, 0xCC, sizeof buf);
    DWORD r = U(GetRegionData)(h, 0, (RGNDATA *)buf);
    t_note("GetRegionData(count 0, buffer) = %lu", r);
    CHECK(buf[0] == 0xCC);
    U(DeleteObject)(h);
}
static void check_bands(HRGN h, const char *what) {
    DWORD sz; RGNDATA *d = rg_getdata(h, &sz);
    if (!d) { CHECKF(0, "%s no data", what); return; }
    RECT *r = rg_rects(d); DWORD n = d->rdh.nCount;
    RECT box; U(GetRgnBox)(h, &box);
    CHECKF(sz == sizeof(RGNDATAHEADER) + n * sizeof(RECT), "%s size", what);
    CHECKF(!memcmp(&box, &d->rdh.rcBound, sizeof box) || n == 0, "%s rcBound", what);
    for (DWORD i = 0; i < n; i++) {
        CHECKF(r[i].left < r[i].right && r[i].top < r[i].bottom, "%s rect %lu empty", what, i);
        if (i) {
            int sameband = r[i].top == r[i - 1].top;
            if (sameband) {
                CHECKF(r[i].bottom == r[i - 1].bottom, "%s band bottoms", what);
                CHECKF(r[i].left > r[i - 1].right, "%s horizontal order/merge %lu", what, i);
            } else {
                CHECKF(r[i].top >= r[i - 1].bottom, "%s vertical order %lu", what, i);
            }
        }
    }
    /* rects reproduce the region exactly */
    RgGrid g, e; rg_grid_of(h, &g); memset(&e, 0, sizeof e);
    for (DWORD i = 0; i < n; i++) rg_grid_rect(&e, r[i].left, r[i].top, r[i].right, r[i].bottom);
    CHECKF(rg_grid_eq(&g, &e), "%s grid mismatch", what);
    free(d);
}
TEST(regions_data_lshape) {
    REQUIRE(U(GetRegionData)); REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN a = rg_rect(0, 0, 10, 10), b = rg_rect(0, 10, 5, 20);
    CHECK(U(CombineRgn)(a, a, b, RGN_OR) == COMPLEXREGION);
    DWORD sz; RGNDATA *d = rg_getdata(a, &sz);
    CHECK(d && d->rdh.nCount == 2);
    if (d && d->rdh.nCount == 2) {
        RECT *r = rg_rects(d);
        CHECK(r[0].left == 0 && r[0].top == 0 && r[0].right == 10 && r[0].bottom == 10);
        CHECK(r[1].left == 0 && r[1].top == 10 && r[1].right == 5 && r[1].bottom == 20);
    }
    free(d);
    U(DeleteObject)(a); U(DeleteObject)(b);
}
TEST(regions_data_plus_shape) {
    REQUIRE(U(GetRegionData)); REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN a = rg_rect(10, 0, 20, 30), b = rg_rect(0, 10, 30, 20);
    CHECK(U(CombineRgn)(a, a, b, RGN_OR) == COMPLEXREGION);
    DWORD sz; RGNDATA *d = rg_getdata(a, &sz);
    CHECK(d && d->rdh.nCount == 3);
    if (d && d->rdh.nCount == 3) {
        RECT *r = rg_rects(d);
        CHECK(r[0].left == 10 && r[0].top == 0 && r[0].right == 20 && r[0].bottom == 10);
        CHECK(r[1].left == 0 && r[1].top == 10 && r[1].right == 30 && r[1].bottom == 20);
        CHECK(r[2].left == 10 && r[2].top == 20 && r[2].right == 20 && r[2].bottom == 30);
    }
    free(d);
    U(DeleteObject)(a); U(DeleteObject)(b);
}
TEST(regions_data_same_band_order) {
    REQUIRE(U(GetRegionData)); REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN a = rg_rect(30, 0, 40, 10), b = rg_rect(0, 0, 5, 10), c = rg_rect(10, 0, 15, 10);
    U(CombineRgn)(a, a, b, RGN_OR); U(CombineRgn)(a, a, c, RGN_OR);
    DWORD sz; RGNDATA *d = rg_getdata(a, &sz);
    CHECK(d && d->rdh.nCount == 3);
    if (d && d->rdh.nCount == 3) {
        RECT *r = rg_rects(d);
        CHECK(r[0].left == 0 && r[1].left == 10 && r[2].left == 30);
        CHECK(r[0].top == 0 && r[2].bottom == 10);
    }
    free(d);
    U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(c);
}
TEST(regions_data_vertical_coalesce) {
    REQUIRE(U(GetRegionData)); REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN a = rg_rect(0, 0, 10, 5), b = rg_rect(0, 5, 10, 10);
    CHECK(U(CombineRgn)(a, a, b, RGN_OR) == SIMPLEREGION);
    DWORD sz; RGNDATA *d = rg_getdata(a, &sz);
    CHECK(d && d->rdh.nCount == 1);
    free(d);
    HRGN c = rg_rect(0, 0, 5, 10), e = rg_rect(5, 0, 10, 10);
    CHECK(U(CombineRgn)(c, c, e, RGN_OR) == SIMPLEREGION);
    d = rg_getdata(c, &sz);
    CHECK(d && d->rdh.nCount == 1);
    free(d);
    U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(c); U(DeleteObject)(e);
}
TEST(regions_data_band_invariants_shapes) {
    REQUIRE(U(GetRegionData)); REQUIRE(U(CreateEllipticRgn)); REQUIRE(U(CreateRoundRectRgn)); REQUIRE(U(DeleteObject));
    HRGN h;
    h = U(CreateEllipticRgn)(0, 0, 60, 40); check_bands(h, "ellipse"); U(DeleteObject)(h);
    h = U(CreateRoundRectRgn)(0, 0, 50, 50, 20, 20); check_bands(h, "roundrect"); U(DeleteObject)(h);
    POINT p[5] = {{30, 0}, {50, 60}, {0, 20}, {60, 20}, {10, 60}};
    h = U(CreatePolygonRgn)(p, 5, ALTERNATE); check_bands(h, "star"); U(DeleteObject)(h);
    h = U(CreatePolygonRgn)(p, 5, WINDING); check_bands(h, "starw"); U(DeleteObject)(h);
}
TEST(regions_data_ellipse_rect_count) {
    REQUIRE(U(GetRegionData)); REQUIRE(U(CreateEllipticRgn)); REQUIRE(U(DeleteObject));
    HRGN h = U(CreateEllipticRgn)(0, 0, 60, 40);
    DWORD sz; RGNDATA *d = rg_getdata(h, &sz);
    CHECK(d != NULL);
    if (d) {
        CHECKF(d->rdh.nCount >= 5 && d->rdh.nCount <= 40, "count %lu", d->rdh.nCount);
        free(d);
    }
    U(DeleteObject)(h);
}
TEST(regions_data_invalid_handles) {
    REQUIRE(U(GetRegionData));
    unsigned char buf[128];
    CHECK(U(GetRegionData)(NULL, 0, NULL) == 0);
    CHECK(U(GetRegionData)(NULL, sizeof buf, (RGNDATA *)buf) == 0);
    CHECK(U(GetRegionData)((HRGN)(ULONG_PTR)0xdeadbeef, 0, NULL) == 0);
}
TEST(regions_data_repeat_identical) {
    REQUIRE(U(GetRegionData)); REQUIRE(U(CreateEllipticRgn)); REQUIRE(U(DeleteObject));
    HRGN h = U(CreateEllipticRgn)(0, 0, 40, 40);
    DWORD s1, s2; RGNDATA *a = rg_getdata(h, &s1), *b = rg_getdata(h, &s2);
    CHECK(a && b && s1 == s2);
    if (a && b && s1 == s2) CHECK(!memcmp(a->Buffer, b->Buffer, s1 - sizeof(RGNDATAHEADER)));
    free(a); free(b);
    U(DeleteObject)(h);
}
TEST(regions_data_large_many_rects) {
    REQUIRE(U(GetRegionData)); REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN acc = rg_rect(0, 0, 0, 0), t = rg_rect(0, 0, 0, 0);
    for (int i = 0; i < 100; i++) { U(SetRectRgn)(t, i * 4, i * 4, i * 4 + 2, i * 4 + 2); U(CombineRgn)(acc, acc, t, RGN_OR); }
    DWORD sz; RGNDATA *d = rg_getdata(acc, &sz);
    CHECK(d && d->rdh.nCount == 100);
    if (d) {
        RECT *r = rg_rects(d);
        for (int i = 0; i < 100 && d->rdh.nCount == 100; i++)
            CHECK(r[i].left == i * 4 && r[i].top == i * 4 && r[i].right == i * 4 + 2 && r[i].bottom == i * 4 + 2);
        CHECK(d->rdh.rcBound.right == 398 && d->rdh.rcBound.bottom == 398);
    }
    free(d);
    U(DeleteObject)(acc); U(DeleteObject)(t);
}
