#include "rg.h"

/* ---- CreateRectRgn ---- */
typedef struct { int l, t, r, b; int el, et, er, eb; int type; } RectCase;
static const RectCase rect_cases[] = {
    {10, 10, 20, 20, 10, 10, 20, 20, SIMPLEREGION},
    {20, 20, 10, 10, 10, 10, 20, 20, SIMPLEREGION},   /* fully inverted */
    {20, 10, 10, 20, 10, 10, 20, 20, SIMPLEREGION},   /* x inverted */
    {10, 20, 20, 10, 10, 10, 20, 20, SIMPLEREGION},   /* y inverted */
    {0, 0, 1, 1, 0, 0, 1, 1, SIMPLEREGION},
    {-5, -5, 5, 5, -5, -5, 5, 5, SIMPLEREGION},
    {-20, -30, -10, -15, -20, -30, -10, -15, SIMPLEREGION},
    {0, 0, 1000, 1, 0, 0, 1000, 1, SIMPLEREGION},
    {0, 0, 1, 1000, 0, 0, 1, 1000, SIMPLEREGION},
    {-1000, -1000, 1000, 1000, -1000, -1000, 1000, 1000, SIMPLEREGION},
    {0, 0, 4096, 4096, 0, 0, 4096, 4096, SIMPLEREGION},
    {100000, 100000, 100001, 100001, 100000, 100000, 100001, 100001, SIMPLEREGION},
    {-100000, -100000, 100000, 100000, -100000, -100000, 100000, 100000, SIMPLEREGION},
    {1, 1, 32767, 32767, 1, 1, 32767, 32767, SIMPLEREGION},
    {-32768, -32768, -1, -1, -32768, -32768, -1, -1, SIMPLEREGION},
    {1000000, 1000000, 1000010, 1000010, 1000000, 1000000, 1000010, 1000010, SIMPLEREGION},
};
TEST(regions_createrect_table) {
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(GetRgnBox)); REQUIRE(U(DeleteObject));
    for (unsigned i = 0; i < sizeof rect_cases / sizeof *rect_cases; i++) {
        const RectCase *c = &rect_cases[i];
        HRGN h = U(CreateRectRgn)(c->l, c->t, c->r, c->b);
        CHECKF(h != NULL, "case %u", i);
        if (!h) continue;
        RECT rc = {-1, -1, -1, -1};
        int t = U(GetRgnBox)(h, &rc);
        CHECKF(t == c->type, "case %u type %d", i, t);
        CHECKF(rc.left == c->el && rc.top == c->et && rc.right == c->er && rc.bottom == c->eb,
               "case %u box %ld,%ld,%ld,%ld", i, rc.left, rc.top, rc.right, rc.bottom);
        CHECK(U(DeleteObject)(h));
    }
}

TEST(regions_createrect_empty) {
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(GetRgnBox)); REQUIRE(U(DeleteObject));
    static const int e[][4] = {{5, 5, 5, 5}, {0, 0, 0, 0}, {5, 5, 5, 10}, {5, 5, 10, 5}, {-3, -3, -3, -3}, {0, 0, 0, 10}, {0, 0, 10, 0}};
    for (int i = 0; i < 7; i++) {
        HRGN h = U(CreateRectRgn)(e[i][0], e[i][1], e[i][2], e[i][3]);
        CHECKF(h != NULL, "case %d", i);
        if (!h) continue;
        RECT rc = {1, 2, 3, 4};
        CHECKF(U(GetRgnBox)(h, &rc) == NULLREGION, "case %d", i);
        if (rc.left || rc.top || rc.right || rc.bottom)
            t_note("empty region case %d box = %ld,%ld,%ld,%ld", i, rc.left, rc.top, rc.right, rc.bottom);
        CHECK(rc.left == rc.right || rc.top == rc.bottom);
        CHECK(!U(PtInRegion)(h, e[i][0], e[i][1]));
        CHECK(U(DeleteObject)(h));
    }
}

TEST(regions_createrect_distinct_handles) {
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(DeleteObject));
    HRGN a = U(CreateRectRgn)(0, 0, 10, 10), b = U(CreateRectRgn)(0, 0, 10, 10);
    CHECK(a && b && a != b);
    CHECK(U(DeleteObject)(a)); CHECK(U(DeleteObject)(b));
}

TEST(regions_createrect_indirect) {
    REQUIRE(U(CreateRectRgnIndirect)); REQUIRE(U(GetRgnBox)); REQUIRE(U(DeleteObject));
    for (unsigned i = 0; i < sizeof rect_cases / sizeof *rect_cases; i++) {
        const RectCase *c = &rect_cases[i];
        RECT in = {c->l, c->t, c->r, c->b}, rc = {0};
        HRGN h = U(CreateRectRgnIndirect)(&in);
        CHECKF(h != NULL, "case %u", i);
        if (!h) continue;
        CHECK(U(GetRgnBox)(h, &rc) == c->type);
        CHECKF(rc.left == c->el && rc.top == c->et && rc.right == c->er && rc.bottom == c->eb, "case %u", i);
        CHECK(in.left == c->l && in.top == c->t && in.right == c->r && in.bottom == c->b); /* input untouched */
        CHECK(U(DeleteObject)(h));
    }
}

TEST(regions_createrect_indirect_empty) {
    REQUIRE(U(CreateRectRgnIndirect)); REQUIRE(U(GetRgnBox)); REQUIRE(U(DeleteObject));
    RECT in = {4, 4, 4, 9};
    HRGN h = U(CreateRectRgnIndirect)(&in);
    CHECK(h != NULL);
    if (h) { CHECK(rg_type(h) == NULLREGION); U(DeleteObject)(h); }
}

/* ---- SetRectRgn ---- */
TEST(regions_setrect_basic) {
    REQUIRE(U(SetRectRgn)); REQUIRE(U(CreateRectRgn)); REQUIRE(U(GetRgnBox)); REQUIRE(U(DeleteObject));
    HRGN h = rg_rect(0, 0, 1, 1);
    RECT rc;
    CHECK(U(SetRectRgn)(h, 5, 6, 25, 36) != 0);
    CHECK(U(GetRgnBox)(h, &rc) == SIMPLEREGION);
    CHECK(rc.left == 5 && rc.top == 6 && rc.right == 25 && rc.bottom == 36);
    CHECK(U(SetRectRgn)(h, 25, 36, 5, 6) != 0);   /* inverted */
    CHECK(U(GetRgnBox)(h, &rc) == SIMPLEREGION);
    CHECK(rc.left == 5 && rc.top == 6 && rc.right == 25 && rc.bottom == 36);
    CHECK(U(SetRectRgn)(h, 7, 7, 7, 20) != 0);    /* empty */
    CHECK(U(GetRgnBox)(h, &rc) == NULLREGION);
    CHECK(U(SetRectRgn)(h, -9, -9, -1, -1) != 0);
    CHECK(U(GetRgnBox)(h, &rc) == SIMPLEREGION);
    CHECK(rc.left == -9 && rc.bottom == -1);
    U(DeleteObject)(h);
}

TEST(regions_setrect_complex_to_simple) {
    REQUIRE(U(SetRectRgn)); REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN a = rg_rect(0, 0, 10, 10), b = rg_rect(20, 20, 30, 30);
    CHECK(U(CombineRgn)(a, a, b, RGN_OR) == COMPLEXREGION);
    CHECK(U(SetRectRgn)(a, 1, 2, 3, 4) != 0);
    RECT rc;
    CHECK(U(GetRgnBox)(a, &rc) == SIMPLEREGION);
    CHECK(rc.left == 1 && rc.top == 2 && rc.right == 3 && rc.bottom == 4);
    CHECK(U(PtInRegion)(a, 1, 2) && !U(PtInRegion)(a, 25, 25));
    U(DeleteObject)(a); U(DeleteObject)(b);
}

TEST(regions_setrect_invalid) {
    REQUIRE(U(SetRectRgn)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject)); REQUIRE(U(CreatePen));
    SET_ERR(0);
    CHECK(U(SetRectRgn)(NULL, 0, 0, 5, 5) == 0);
    CHECK(U(SetRectRgn)((HRGN)(ULONG_PTR)0x1234, 0, 0, 5, 5) == 0);
    HBRUSH br = U(CreateSolidBrush)(0);
    CHECK(U(SetRectRgn)((HRGN)br, 0, 0, 5, 5) == 0);
    U(DeleteObject)(br);
    HRGN h = rg_rect(0, 0, 4, 4);
    U(DeleteObject)(h);
    CHECK(U(SetRectRgn)(h, 0, 0, 5, 5) == 0);  /* deleted */
}

/* ---- Elliptic ---- */
static void check_ellipse(HRGN h, int l, int t, int r, int b, const char *what) {
    RECT rc; int ty = U(GetRgnBox)(h, &rc);
    CHECKF(ty == SIMPLEREGION || ty == COMPLEXREGION, "%s type %d", what, ty);
    /* path-built regions: right/bottom edge may fall one pixel short of the nominal rect */
    CHECKF(rc.left == l && rc.top == t && rc.right >= r - 1 && rc.right <= r && rc.bottom >= b - 1 && rc.bottom <= b, "%s box %ld,%ld,%ld,%ld", what, rc.left, rc.top, rc.right, rc.bottom);
    if (rc.right != r || rc.bottom != b) t_note("%s box %ld,%ld,%ld,%ld for rect %d,%d,%d,%d", what, rc.left, rc.top, rc.right, rc.bottom, l, t, r, b);
    /* symmetry about both axes (within the half-open box) */
    int bad = 0, w = rc.right - rc.left, hh = rc.bottom - rc.top, area = 0;
    r = rc.right; b = rc.bottom;
    for (int y = t; y < b; y++) for (int x = l; x < r; x++) {
        int in = U(PtInRegion)(h, x, y) ? 1 : 0;
        area += in;
        int mx = l + (w - 1 - (x - l));
        int my = t + (hh - 1 - (y - t));
        if (in != (U(PtInRegion)(h, mx, y) ? 1 : 0)) bad++;
        if (in != (U(PtInRegion)(h, x, my) ? 1 : 0)) bad++;
    }
    if (w >= 4 && hh >= 4) t_note("%s symmetry mismatches %d", what, bad);
    /* area close to pi/4*w*h */
    double ex = 3.14159265 * w * hh / 4.0;
    CHECKF(area > ex * 0.80 && area < ex * 1.20 + 2, "%s area %d expected ~%.1f", what, area, ex);
    /* corner pixel of the bounding box is outside for sizeable ellipses */
    if (w >= 8 && hh >= 8) CHECK(!U(PtInRegion)(h, l, t) && !U(PtInRegion)(h, r - 1, b - 1));
    /* centre inside */
    CHECK(U(PtInRegion)(h, l + w / 2, t + hh / 2));
    /* nothing outside the box */
    CHECK(!U(PtInRegion)(h, l - 1, t + hh / 2) && !U(PtInRegion)(h, r, t + hh / 2));
    CHECK(!U(PtInRegion)(h, l + w / 2, t - 1) && !U(PtInRegion)(h, l + w / 2, b));
}
static const int ell_cases[][4] = {
    {0, 0, 16, 16}, {0, 0, 32, 16}, {0, 0, 16, 32}, {5, 7, 45, 31}, {-20, -20, 20, 20},
    {0, 0, 100, 100}, {10, 10, 11, 11}, {0, 0, 64, 64}, {3, 3, 40, 9},
};
TEST(regions_elliptic_table) {
    REQUIRE(U(CreateEllipticRgn)); REQUIRE(U(GetRgnBox)); REQUIRE(U(PtInRegion)); REQUIRE(U(DeleteObject));
    for (unsigned i = 0; i < sizeof ell_cases / sizeof *ell_cases; i++) {
        const int *c = ell_cases[i];
        HRGN h = U(CreateEllipticRgn)(c[0], c[1], c[2], c[3]);
        CHECKF(h != NULL, "case %u", i);
        if (!h) continue;
        char nm[32]; sprintf(nm, "ell%u", i);
        if (c[2] - c[0] > 1) check_ellipse(h, c[0], c[1], c[2], c[3], nm);
        U(DeleteObject)(h);
    }
}
TEST(regions_elliptic_indirect_equals_direct) {
    REQUIRE(U(CreateEllipticRgn)); REQUIRE(U(CreateEllipticRgnIndirect)); REQUIRE(U(EqualRgn)); REQUIRE(U(DeleteObject));
    for (unsigned i = 0; i < sizeof ell_cases / sizeof *ell_cases; i++) {
        const int *c = ell_cases[i];
        RECT r = {c[0], c[1], c[2], c[3]};
        HRGN a = U(CreateEllipticRgn)(c[0], c[1], c[2], c[3]), b = U(CreateEllipticRgnIndirect)(&r);
        CHECKF(a && b && U(EqualRgn)(a, b), "case %u", i);
        if (a) U(DeleteObject)(a);
        if (b) U(DeleteObject)(b);
    }
}
TEST(regions_elliptic_inverted) {
    REQUIRE(U(CreateEllipticRgn)); REQUIRE(U(EqualRgn)); REQUIRE(U(DeleteObject));
    HRGN a = U(CreateEllipticRgn)(0, 0, 30, 20), b = U(CreateEllipticRgn)(30, 20, 0, 0);
    CHECK(a && b);
    if (a && b) {
        RECT rc;
        CHECK(U(GetRgnBox)(b, &rc) != 0);
        CHECK(rc.left == 0 && rc.top == 0 && rc.right >= 29 && rc.right <= 30 && rc.bottom >= 19 && rc.bottom <= 20);
        CHECK(U(EqualRgn)(a, b));
    }
    if (a) U(DeleteObject)(a);
    if (b) U(DeleteObject)(b);
}
TEST(regions_elliptic_empty) {
    REQUIRE(U(CreateEllipticRgn)); REQUIRE(U(DeleteObject));
    static const int e[][4] = {{5, 5, 5, 5}, {0, 0, 0, 10}, {0, 0, 10, 0}};
    for (int i = 0; i < 3; i++) {
        HRGN h = U(CreateEllipticRgn)(e[i][0], e[i][1], e[i][2], e[i][3]);
        CHECKF(h != NULL, "case %d", i);
        if (h) { t_note("empty ellipse case %d type %d", i, rg_type(h)); U(DeleteObject)(h); }
    }
}
TEST(regions_elliptic_circle_symmetry_strict) {
    REQUIRE(U(CreateEllipticRgn)); REQUIRE(U(DeleteObject));
    /* Circles must be symmetric under a transpose about the diagonal. */
    for (int d = 4; d <= 40; d += 6) {
        HRGN h = U(CreateEllipticRgn)(0, 0, d, d);
        if (!h) { CHECK(0); continue; }
        int bad = 0;
        for (int y = 0; y < d; y++) for (int x = 0; x < d; x++)
            if (U(PtInRegion)(h, x, y) != U(PtInRegion)(h, y, x)) bad++;
        CHECKF(bad == 0, "diameter %d transpose mismatches %d", d, bad);
        U(DeleteObject)(h);
    }
}
TEST(regions_elliptic_monotonic_rows) {
    REQUIRE(U(CreateEllipticRgn)); REQUIRE(U(DeleteObject));
    HRGN h = U(CreateEllipticRgn)(0, 0, 60, 40);
    CHECK(h != NULL);
    if (!h) return;
    int prev = -1, widest = 0, widest_row = 0;
    for (int y = 0; y < 20; y++) {   /* upper half: widths non-decreasing */
        int n = 0; for (int x = 0; x < 60; x++) n += U(PtInRegion)(h, x, y) ? 1 : 0;
        CHECKF(n >= prev, "row %d width %d < prev %d", y, n, prev);
        prev = n; if (n > widest) { widest = n; widest_row = y; }
    }
    CHECK(widest >= 56);
    U(DeleteObject)(h);
}

/* ---- RoundRect ---- */
TEST(regions_roundrect_basic) {
    REQUIRE(U(CreateRoundRectRgn)); REQUIRE(U(GetRgnBox)); REQUIRE(U(DeleteObject));
    HRGN h = U(CreateRoundRectRgn)(10, 10, 60, 50, 16, 16);
    CHECK(h != NULL);
    if (!h) return;
    RECT rc;
    CHECK(U(GetRgnBox)(h, &rc) == COMPLEXREGION);
    CHECK(rc.left == 10 && rc.top == 10 && rc.right >= 59 && rc.right <= 60 && rc.bottom >= 49 && rc.bottom <= 50);
    if (rc.right != 60 || rc.bottom != 50) t_note("roundrect box %ld,%ld,%ld,%ld for 10,10,60,50", rc.left, rc.top, rc.right, rc.bottom);
    CHECK(!U(PtInRegion)(h, 10, 10));
    CHECK(!U(PtInRegion)(h, rc.right - 1, 10) && !U(PtInRegion)(h, 10, rc.bottom - 1) && !U(PtInRegion)(h, rc.right - 1, rc.bottom - 1));
    CHECK(U(PtInRegion)(h, 35, 30) && U(PtInRegion)(h, 35, 10) && U(PtInRegion)(h, 10, 30));
    CHECK(U(PtInRegion)(h, rc.right - 1, 30) && U(PtInRegion)(h, 35, rc.bottom - 1));
    U(DeleteObject)(h);
}
TEST(regions_roundrect_zero_radius) {
    REQUIRE(U(CreateRoundRectRgn)); REQUIRE(U(GetRgnBox)); REQUIRE(U(DeleteObject));
    HRGN h = U(CreateRoundRectRgn)(0, 0, 20, 10, 0, 0);
    CHECK(h != NULL);
    if (!h) return;
    RECT rc;
    int t = U(GetRgnBox)(h, &rc);
    CHECKF(t == SIMPLEREGION, "type %d", t);
    CHECK(rc.left == 0 && rc.top == 0 && rc.right >= 19 && rc.right <= 20 && rc.bottom >= 9 && rc.bottom <= 10);
    CHECK(U(PtInRegion)(h, 0, 0) && U(PtInRegion)(h, 18, 8));
    U(DeleteObject)(h);
}
TEST(regions_roundrect_symmetry) {
    REQUIRE(U(CreateRoundRectRgn)); REQUIRE(U(DeleteObject));
    static const int c[][6] = {{0, 0, 40, 40, 10, 10}, {0, 0, 50, 30, 20, 10}, {0, 0, 31, 31, 8, 8}, {0, 0, 64, 32, 64, 32}, {0, 0, 40, 40, 100, 100}};
    for (int i = 0; i < 5; i++) {
        HRGN h = U(CreateRoundRectRgn)(c[i][0], c[i][1], c[i][2], c[i][3], c[i][4], c[i][5]);
        CHECKF(h != NULL, "case %d", i);
        if (!h) continue;
        int w = c[i][2], hh = c[i][3], bad = 0;
        for (int y = 0; y < hh; y++) for (int x = 0; x < w; x++) {
            int in = U(PtInRegion)(h, x, y) ? 1 : 0;
            if (in != (U(PtInRegion)(h, w - 1 - x, y) ? 1 : 0)) bad++;
            if (in != (U(PtInRegion)(h, x, hh - 1 - y) ? 1 : 0)) bad++;
        }
        if (bad) t_note("roundrect case %d asymmetric by %d samples", i, bad);
        /* region is always inside bounds and never larger than the rect */
        RgGrid g; rg_grid_of(h, &g);
        CHECK(rg_grid_cnt(&g) <= w * hh);
        CHECK(rg_grid_cnt(&g) >= w * hh / 2);
        U(DeleteObject)(h);
    }
}
TEST(regions_roundrect_inverted_and_indirect) {
    REQUIRE(U(CreateRoundRectRgn)); REQUIRE(U(EqualRgn)); REQUIRE(U(DeleteObject));
    HRGN a = U(CreateRoundRectRgn)(0, 0, 40, 30, 10, 10), b = U(CreateRoundRectRgn)(40, 30, 0, 0, 10, 10);
    CHECK(a && b);
    if (a && b) { RECT rc; U(GetRgnBox)(b, &rc); CHECK(rc.right >= 39 && rc.right <= 40 && rc.bottom >= 29 && rc.bottom <= 30 && rc.left == 0 && rc.top == 0); CHECK(U(EqualRgn)(a, b)); }
    if (a) U(DeleteObject)(a);
    if (b) U(DeleteObject)(b);
}
TEST(regions_roundrect_negative_radius) {
    REQUIRE(U(CreateRoundRectRgn)); REQUIRE(U(DeleteObject));
    HRGN h = U(CreateRoundRectRgn)(0, 0, 30, 30, -10, -10);
    /* observed only: negative radii are not documented */
    t_note("negative-radius roundrect region -> %s", h ? "valid" : "NULL");
    if (h) U(DeleteObject)(h);
}
TEST(regions_roundrect_empty) {
    REQUIRE(U(CreateRoundRectRgn)); REQUIRE(U(DeleteObject));
    HRGN h = U(CreateRoundRectRgn)(5, 5, 5, 20, 4, 4);
    if (h) { t_note("empty roundrect type %d", rg_type(h)); U(DeleteObject)(h); }
    else t_note("empty roundrect region returned NULL");
}

/* ---- Polygon ---- */
TEST(regions_polygon_triangle) {
    REQUIRE(U(CreatePolygonRgn)); REQUIRE(U(GetRgnBox)); REQUIRE(U(DeleteObject));
    POINT p[3] = {{0, 0}, {20, 0}, {0, 20}};
    for (int mode = ALTERNATE; mode <= WINDING; mode++) {
        HRGN h = U(CreatePolygonRgn)(p, 3, mode);
        CHECKF(h != NULL, "mode %d", mode);
        if (!h) continue;
        RECT rc; int t = U(GetRgnBox)(h, &rc);
        CHECK(t == COMPLEXREGION);
        CHECK(rc.left == 0 && rc.top == 0 && rc.right == 20 && rc.bottom == 20);
        CHECK(U(PtInRegion)(h, 1, 1) && U(PtInRegion)(h, 0, 0) && !U(PtInRegion)(h, 19, 19) && !U(PtInRegion)(h, 15, 15));
        RgGrid g; rg_grid_of(h, &g);
        int n = rg_grid_cnt(&g);
        CHECKF(n >= 170 && n <= 230, "area %d", n);
        U(DeleteObject)(h);
    }
}
TEST(regions_polygon_square_exact) {
    REQUIRE(U(CreatePolygonRgn)); REQUIRE(U(GetRgnBox)); REQUIRE(U(DeleteObject));
    POINT p[4] = {{10, 10}, {30, 10}, {30, 30}, {10, 30}};
    HRGN h = U(CreatePolygonRgn)(p, 4, ALTERNATE);
    CHECK(h != NULL);
    if (!h) return;
    RECT rc; int t = U(GetRgnBox)(h, &rc);
    CHECK(t == SIMPLEREGION);
    CHECK(rc.left == 10 && rc.top == 10 && rc.right == 30 && rc.bottom == 30);
    CHECK(U(PtInRegion)(h, 10, 10) && U(PtInRegion)(h, 29, 29));
    CHECK(!U(PtInRegion)(h, 30, 20) && !U(PtInRegion)(h, 20, 30) && !U(PtInRegion)(h, 9, 20));
    U(DeleteObject)(h);
}
TEST(regions_polygon_fillmode_star) {
    REQUIRE(U(CreatePolygonRgn)); REQUIRE(U(DeleteObject));
    /* pentagram: centre is a hole with ALTERNATE, filled with WINDING */
    POINT p[5] = {{50, 0}, {81, 95}, {0, 36}, {100, 36}, {19, 95}};
    HRGN alt = U(CreatePolygonRgn)(p, 5, ALTERNATE), win = U(CreatePolygonRgn)(p, 5, WINDING);
    CHECK(alt && win);
    if (alt && win) {
        CHECK(!U(PtInRegion)(alt, 50, 50));
        CHECK(U(PtInRegion)(win, 50, 50));
        CHECK(U(PtInRegion)(alt, 50, 10) && U(PtInRegion)(win, 50, 10));
        RgGrid a, w; rg_grid_of(alt, &a); rg_grid_of(win, &w);
        for (int y = 0; y < RG_N; y++) for (int x = 0; x < RG_N; x++) if (a.c[y][x]) CHECK(w.c[y][x]);
    }
    if (alt) U(DeleteObject)(alt);
    if (win) U(DeleteObject)(win);
}
TEST(regions_polygon_degenerate) {
    REQUIRE(U(CreatePolygonRgn)); REQUIRE(U(DeleteObject));
    POINT line[2] = {{0, 0}, {10, 10}}, pt1[1] = {{5, 5}}, same[3] = {{4, 4}, {4, 4}, {4, 4}};
    POINT col[3] = {{0, 0}, {5, 5}, {10, 10}};
    HRGN h;
    h = U(CreatePolygonRgn)(line, 2, ALTERNATE);
    t_note("2-point polygon region -> %s", h ? "valid" : "NULL");
    if (h) { CHECK(rg_type(h) == NULLREGION); U(DeleteObject)(h); }
    h = U(CreatePolygonRgn)(col, 3, ALTERNATE);
    t_note("collinear polygon region -> %s", h ? "valid" : "NULL");
    if (h) { CHECK(rg_type(h) == NULLREGION); U(DeleteObject)(h); }
    h = U(CreatePolygonRgn)(same, 3, WINDING);
    if (h) { CHECK(rg_type(h) == NULLREGION); U(DeleteObject)(h); }
    SET_ERR(0);
    h = U(CreatePolygonRgn)(pt1, 1, ALTERNATE);
    t_note("1-point polygon region -> %s", h ? "valid" : "NULL");
    if (h) U(DeleteObject)(h);
}
TEST(regions_polygon_bad_mode) {
    REQUIRE(U(CreatePolygonRgn)); REQUIRE(U(DeleteObject));
    POINT p[3] = {{0, 0}, {20, 0}, {0, 20}};
    int modes[] = {0, 3, -1, 100};
    for (int i = 0; i < 4; i++) {
        HRGN h = U(CreatePolygonRgn)(p, 3, modes[i]);
        t_note("polygon region mode %d -> %s", modes[i], h ? "valid" : "NULL");
        if (h) U(DeleteObject)(h);
    }
}
TEST(regions_polygon_invalid_count) {
    REQUIRE(U(CreatePolygonRgn));
    POINT p[3] = {{0, 0}, {20, 0}, {0, 20}};
    HRGN h = U(CreatePolygonRgn)(p, 0, ALTERNATE);
    t_note("count 0 -> %s", h ? "valid" : "NULL");
    if (h) U(DeleteObject)(h);
    h = U(CreatePolygonRgn)(p, -1, ALTERNATE);
    CHECK(h == NULL);
    if (h) U(DeleteObject)(h);
    h = U(CreatePolygonRgn)(NULL, 0, ALTERNATE);
    t_note("NULL,0 -> %s", h ? "valid" : "NULL");
    if (h) U(DeleteObject)(h);
}
TEST(regions_polygon_negative_coords) {
    REQUIRE(U(CreatePolygonRgn)); REQUIRE(U(DeleteObject));
    POINT p[4] = {{-20, -20}, {-5, -20}, {-5, -5}, {-20, -5}};
    HRGN h = U(CreatePolygonRgn)(p, 4, WINDING);
    CHECK(h != NULL);
    if (!h) return;
    RECT rc; CHECK(U(GetRgnBox)(h, &rc) == SIMPLEREGION);
    CHECK(rc.left == -20 && rc.top == -20 && rc.right == -5 && rc.bottom == -5);
    U(DeleteObject)(h);
}
TEST(regions_polygon_closed_explicit) {
    REQUIRE(U(CreatePolygonRgn)); REQUIRE(U(EqualRgn)); REQUIRE(U(DeleteObject));
    POINT a[3] = {{0, 0}, {20, 0}, {0, 20}}, b[4] = {{0, 0}, {20, 0}, {0, 20}, {0, 0}};
    HRGN ha = U(CreatePolygonRgn)(a, 3, ALTERNATE), hb = U(CreatePolygonRgn)(b, 4, ALTERNATE);
    CHECK(ha && hb && U(EqualRgn)(ha, hb));
    if (ha) U(DeleteObject)(ha);
    if (hb) U(DeleteObject)(hb);
}
TEST(regions_polygon_winding_orientation) {
    REQUIRE(U(CreatePolygonRgn)); REQUIRE(U(EqualRgn)); REQUIRE(U(DeleteObject));
    POINT cw[4] = {{0, 0}, {20, 0}, {20, 20}, {0, 20}}, ccw[4] = {{0, 0}, {0, 20}, {20, 20}, {20, 0}};
    HRGN a = U(CreatePolygonRgn)(cw, 4, WINDING), b = U(CreatePolygonRgn)(ccw, 4, WINDING);
    CHECK(a && b && U(EqualRgn)(a, b));
    if (a) U(DeleteObject)(a);
    if (b) U(DeleteObject)(b);
}

/* ---- PolyPolygon ---- */
TEST(regions_polypolygon_two_squares) {
    REQUIRE(U(CreatePolyPolygonRgn)); REQUIRE(U(GetRgnBox)); REQUIRE(U(DeleteObject));
    POINT p[8] = {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {20, 20}, {30, 20}, {30, 30}, {20, 30}};
    INT cnt[2] = {4, 4};
    HRGN h = U(CreatePolyPolygonRgn)(p, cnt, 2, ALTERNATE);
    CHECK(h != NULL);
    if (!h) return;
    RECT rc; CHECK(U(GetRgnBox)(h, &rc) == COMPLEXREGION);
    CHECK(rc.left == 0 && rc.top == 0 && rc.right == 30 && rc.bottom == 30);
    CHECK(U(PtInRegion)(h, 5, 5) && U(PtInRegion)(h, 25, 25) && !U(PtInRegion)(h, 15, 15));
    U(DeleteObject)(h);
}
TEST(regions_polypolygon_nested_modes) {
    REQUIRE(U(CreatePolyPolygonRgn)); REQUIRE(U(DeleteObject));
    /* outer square CW and inner square CW: ALTERNATE punches a hole, WINDING does not */
    POINT p[8] = {{0, 0}, {40, 0}, {40, 40}, {0, 40}, {10, 10}, {30, 10}, {30, 30}, {10, 30}};
    INT cnt[2] = {4, 4};
    HRGN alt = U(CreatePolyPolygonRgn)(p, cnt, 2, ALTERNATE), win = U(CreatePolyPolygonRgn)(p, cnt, 2, WINDING);
    CHECK(alt && win);
    if (alt && win) {
        CHECK(!U(PtInRegion)(alt, 20, 20) && U(PtInRegion)(alt, 5, 5));
        CHECK(U(PtInRegion)(win, 20, 20) && U(PtInRegion)(win, 5, 5));
        CHECK(rg_type(win) == SIMPLEREGION);
    }
    /* inner square reversed: winding also leaves a hole */
    POINT q[8] = {{0, 0}, {40, 0}, {40, 40}, {0, 40}, {10, 10}, {10, 30}, {30, 30}, {30, 10}};
    HRGN rev = U(CreatePolyPolygonRgn)(q, cnt, 2, WINDING);
    CHECK(rev != NULL);
    if (rev) { CHECK(!U(PtInRegion)(rev, 20, 20) && U(PtInRegion)(rev, 5, 5)); U(DeleteObject)(rev); }
    if (alt) U(DeleteObject)(alt);
    if (win) U(DeleteObject)(win);
}
TEST(regions_polypolygon_single_equals_polygon) {
    REQUIRE(U(CreatePolyPolygonRgn)); REQUIRE(U(CreatePolygonRgn)); REQUIRE(U(EqualRgn)); REQUIRE(U(DeleteObject));
    POINT p[5] = {{50, 0}, {81, 95}, {0, 36}, {100, 36}, {19, 95}};
    INT cnt[1] = {5};
    HRGN a = U(CreatePolyPolygonRgn)(p, cnt, 1, WINDING), b = U(CreatePolygonRgn)(p, 5, WINDING);
    CHECK(a && b && U(EqualRgn)(a, b));
    if (a) U(DeleteObject)(a);
    if (b) U(DeleteObject)(b);
}
TEST(regions_polypolygon_invalid) {
    REQUIRE(U(CreatePolyPolygonRgn));
    POINT p[4] = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
    INT cnt[1] = {4};
    HRGN h = U(CreatePolyPolygonRgn)(p, cnt, 0, ALTERNATE);
    t_note("polycount 0 -> %s", h ? "valid" : "NULL");
    if (h) U(DeleteObject)(h);
    h = U(CreatePolyPolygonRgn)(p, cnt, -1, ALTERNATE);
    t_note("polycount -1 -> %s", h ? "valid" : "NULL");
    if (h) U(DeleteObject)(h);
    INT neg[1] = {-4};
    h = U(CreatePolyPolygonRgn)(p, neg, 1, ALTERNATE);
    t_note("negative vertex count -> %s", h ? "valid" : "NULL");
    if (h) U(DeleteObject)(h);
}
TEST(regions_polypolygon_polygon_with_one_point) {
    REQUIRE(U(CreatePolyPolygonRgn)); REQUIRE(U(DeleteObject));
    POINT p[5] = {{0, 0}, {10, 0}, {10, 10}, {0, 10}, {50, 50}};
    INT cnt[2] = {4, 1};
    HRGN h = U(CreatePolyPolygonRgn)(p, cnt, 2, ALTERNATE);
    t_note("polypolygon with 1-point polygon -> %s", h ? "valid" : "NULL");
    if (h) U(DeleteObject)(h);
}

/* ---- ExtCreateRegion ---- */
static RGNDATA *mkdata(const RECT *r, int n, int *size) {
    int sz = sizeof(RGNDATAHEADER) + n * sizeof(RECT);
    RGNDATA *d = (RGNDATA *)calloc(1, sz + 16);
    d->rdh.dwSize = sizeof(RGNDATAHEADER); d->rdh.iType = RDH_RECTANGLES; d->rdh.nCount = n;
    d->rdh.nRgnSize = n * sizeof(RECT);
    RECT bb = {0};
    for (int i = 0; i < n; i++) {
        memcpy(d->Buffer + i * sizeof(RECT), &r[i], sizeof(RECT));
        if (i == 0) bb = r[0];
        else { if (r[i].left < bb.left) bb.left = r[i].left; if (r[i].top < bb.top) bb.top = r[i].top;
               if (r[i].right > bb.right) bb.right = r[i].right; if (r[i].bottom > bb.bottom) bb.bottom = r[i].bottom; }
    }
    d->rdh.rcBound = bb;
    *size = sz; return d;
}
TEST(regions_extcreate_two_rects) {
    REQUIRE(U(ExtCreateRegion)); REQUIRE(U(GetRgnBox)); REQUIRE(U(DeleteObject));
    RECT r[2] = {{0, 0, 10, 10}, {20, 20, 30, 30}};
    int sz; RGNDATA *d = mkdata(r, 2, &sz);
    HRGN h = U(ExtCreateRegion)(NULL, sz, d);
    CHECK(h != NULL);
    if (h) {
        RECT rc; CHECK(U(GetRgnBox)(h, &rc) == COMPLEXREGION);
        CHECK(rc.left == 0 && rc.top == 0 && rc.right == 30 && rc.bottom == 30);
        CHECK(U(PtInRegion)(h, 5, 5) && U(PtInRegion)(h, 25, 25) && !U(PtInRegion)(h, 15, 15));
        U(DeleteObject)(h);
    }
    free(d);
}
TEST(regions_extcreate_single_rect_fastpath) {
    REQUIRE(U(ExtCreateRegion)); REQUIRE(U(GetRgnBox)); REQUIRE(U(DeleteObject));
    RECT r[1] = {{3, 4, 13, 24}};
    int sz; RGNDATA *d = mkdata(r, 1, &sz);
    HRGN h = U(ExtCreateRegion)(NULL, sz, d);
    CHECK(h != NULL);
    if (h) { RECT rc; CHECK(U(GetRgnBox)(h, &rc) == SIMPLEREGION);
             CHECK(rc.left == 3 && rc.top == 4 && rc.right == 13 && rc.bottom == 24); U(DeleteObject)(h); }
    /* inverted rect in data */
    RECT inv[1] = {{13, 24, 3, 4}};
    free(d); d = mkdata(inv, 1, &sz);
    h = U(ExtCreateRegion)(NULL, sz, d);
    t_note("ExtCreateRegion with inverted rect -> %s", h ? "valid" : "NULL");
    if (h) { RECT rc; U(GetRgnBox)(h, &rc); t_note("box %ld,%ld,%ld,%ld", rc.left, rc.top, rc.right, rc.bottom); U(DeleteObject)(h); }
    free(d);
}
TEST(regions_extcreate_null_and_small_count) {
    REQUIRE(U(ExtCreateRegion));
    RECT r[1] = {{3, 4, 13, 24}};
    int sz; RGNDATA *d = mkdata(r, 1, &sz);
    SET_ERR(0);
    CHECK(U(ExtCreateRegion)(NULL, sz, NULL) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(U(ExtCreateRegion)(NULL, 31, d) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(U(ExtCreateRegion)(NULL, 0, d) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(U(ExtCreateRegion)(NULL, 32, d) == NULL);   /* header only but nCount==1 */
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(U(ExtCreateRegion)(NULL, 47, d) == NULL);   /* one byte short of a rect */
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    free(d);
}
TEST(regions_extcreate_bad_header) {
    REQUIRE(U(ExtCreateRegion)); REQUIRE(U(DeleteObject));
    RECT r[2] = {{0, 0, 10, 10}, {20, 20, 30, 30}};
    int sz; RGNDATA *d = mkdata(r, 2, &sz);
    HRGN h;
    d->rdh.dwSize = 16;
    h = U(ExtCreateRegion)(NULL, sz, d); CHECK(h == NULL); if (h) U(DeleteObject)(h);
    d->rdh.dwSize = 31;
    h = U(ExtCreateRegion)(NULL, sz, d); CHECK(h == NULL); if (h) U(DeleteObject)(h);
    d->rdh.dwSize = 0;
    h = U(ExtCreateRegion)(NULL, sz, d); CHECK(h == NULL); if (h) U(DeleteObject)(h);
    d->rdh.dwSize = 32; d->rdh.iType = 0;
    h = U(ExtCreateRegion)(NULL, sz, d); t_note("iType 0 -> %s", h ? "valid" : "NULL"); if (h) U(DeleteObject)(h);
    d->rdh.iType = 2;
    h = U(ExtCreateRegion)(NULL, sz, d); t_note("iType 2 -> %s", h ? "valid" : "NULL"); if (h) U(DeleteObject)(h);
    d->rdh.iType = RDH_RECTANGLES; d->rdh.nCount = 1000;   /* more rects than bytes supplied */
    h = U(ExtCreateRegion)(NULL, sz, d); CHECK(h == NULL); if (h) U(DeleteObject)(h);
    d->rdh.nCount = 3;
    h = U(ExtCreateRegion)(NULL, sz, d); CHECK(h == NULL); if (h) U(DeleteObject)(h);
    d->rdh.nCount = 0xFFFFFFFF;
    h = U(ExtCreateRegion)(NULL, sz, d); CHECK(h == NULL); if (h) U(DeleteObject)(h);
    free(d);
}
TEST(regions_extcreate_count_zero) {
    REQUIRE(U(ExtCreateRegion)); REQUIRE(U(DeleteObject));
    RECT r[1] = {{0, 0, 1, 1}};
    int sz; RGNDATA *d = mkdata(r, 1, &sz);
    d->rdh.nCount = 0; d->rdh.nRgnSize = 0;
    HRGN h = U(ExtCreateRegion)(NULL, sz, d);
    t_note("nCount 0 -> %s", h ? "valid" : "NULL");
    if (h) { CHECK(rg_type(h) == NULLREGION); U(DeleteObject)(h); }
    free(d);
}
TEST(regions_extcreate_extra_buffer_ok) {
    REQUIRE(U(ExtCreateRegion)); REQUIRE(U(DeleteObject));
    RECT r[2] = {{0, 0, 10, 10}, {20, 20, 30, 30}};
    int sz; RGNDATA *d = mkdata(r, 2, &sz);
    HRGN h = U(ExtCreateRegion)(NULL, sz + 16, d);
    CHECK(h != NULL);
    if (h) U(DeleteObject)(h);
    free(d);
}
TEST(regions_extcreate_unsorted_rects) {
    REQUIRE(U(ExtCreateRegion)); REQUIRE(U(DeleteObject));
    /* rectangles listed bottom-to-top: observed behaviour only */
    RECT r[2] = {{20, 20, 30, 30}, {0, 0, 10, 10}};
    int sz; RGNDATA *d = mkdata(r, 2, &sz);
    HRGN h = U(ExtCreateRegion)(NULL, sz, d);
    t_note("unsorted rects -> %s", h ? "valid" : "NULL");
    if (h) {
        t_note("pt(5,5)=%d pt(25,25)=%d", U(PtInRegion)(h, 5, 5), U(PtInRegion)(h, 25, 25));
        U(DeleteObject)(h);
    }
    free(d);
}
TEST(regions_extcreate_xform_translate) {
    REQUIRE(U(ExtCreateRegion)); REQUIRE(U(GetRgnBox)); REQUIRE(U(DeleteObject));
    RECT r[2] = {{0, 0, 10, 10}, {20, 20, 30, 30}};
    int sz; RGNDATA *d = mkdata(r, 2, &sz);
    XFORM x = {1, 0, 0, 1, 100, 200};
    HRGN h = U(ExtCreateRegion)(&x, sz, d);
    CHECK(h != NULL);
    if (h) {
        RECT rc; CHECK(U(GetRgnBox)(h, &rc) == COMPLEXREGION);
        CHECK(rc.left == 100 && rc.top == 200 && rc.right == 130 && rc.bottom == 230);
        CHECK(U(PtInRegion)(h, 105, 205) && !U(PtInRegion)(h, 5, 5));
        U(DeleteObject)(h);
    }
    free(d);
}
TEST(regions_extcreate_xform_scale) {
    REQUIRE(U(ExtCreateRegion)); REQUIRE(U(GetRgnBox)); REQUIRE(U(DeleteObject));
    RECT r[2] = {{0, 0, 10, 10}, {20, 20, 30, 30}};
    int sz; RGNDATA *d = mkdata(r, 2, &sz);
    XFORM x = {2, 0, 0, 3, 0, 0};
    HRGN h = U(ExtCreateRegion)(&x, sz, d);
    CHECK(h != NULL);
    if (h) {
        RECT rc; CHECK(U(GetRgnBox)(h, &rc) == COMPLEXREGION);
        CHECK(rc.left == 0 && rc.top == 0 && rc.right == 60 && rc.bottom == 90);
        CHECK(U(PtInRegion)(h, 19, 29) && !U(PtInRegion)(h, 20, 29) && !U(PtInRegion)(h, 19, 30));
        CHECK(U(PtInRegion)(h, 40, 60) && U(PtInRegion)(h, 59, 89));
        U(DeleteObject)(h);
    }
    free(d);
}
TEST(regions_extcreate_xform_identity_single) {
    REQUIRE(U(ExtCreateRegion)); REQUIRE(U(GetRgnBox)); REQUIRE(U(DeleteObject));
    RECT r[1] = {{3, 4, 13, 24}};
    int sz; RGNDATA *d = mkdata(r, 1, &sz);
    XFORM x = {1, 0, 0, 1, 0, 0};
    HRGN h = U(ExtCreateRegion)(&x, sz, d);
    CHECK(h != NULL);
    if (h) { RECT rc; CHECK(U(GetRgnBox)(h, &rc) == SIMPLEREGION);
             CHECK(rc.left == 3 && rc.top == 4 && rc.right == 13 && rc.bottom == 24); U(DeleteObject)(h); }
    free(d);
}
TEST(regions_extcreate_xform_rotate90) {
    REQUIRE(U(ExtCreateRegion)); REQUIRE(U(GetRgnBox)); REQUIRE(U(DeleteObject));
    RECT r[1] = {{0, 0, 10, 20}};
    int sz; RGNDATA *d = mkdata(r, 1, &sz);
    XFORM x = {0, 1, -1, 0, 0, 0};  /* x' = -y, y' = x */
    HRGN h = U(ExtCreateRegion)(&x, sz, d);
    CHECK(h != NULL);
    if (h) {
        RECT rc; int t = U(GetRgnBox)(h, &rc);
        CHECK(t == SIMPLEREGION);
        CHECK(rc.left == -20 && rc.top == 0 && rc.right == 0 && rc.bottom == 10);
        U(DeleteObject)(h);
    }
    free(d);
}
TEST(regions_extcreate_xform_rotate45_bounds) {
    REQUIRE(U(ExtCreateRegion)); REQUIRE(U(GetRgnBox)); REQUIRE(U(DeleteObject));
    RECT r[1] = {{0, 0, 20, 20}};
    int sz; RGNDATA *d = mkdata(r, 1, &sz);
    float c = 0.70710678f;
    XFORM x = {c, c, -c, c, 0, 0};
    HRGN h = U(ExtCreateRegion)(&x, sz, d);
    CHECK(h != NULL);
    if (h) {
        RECT rc; int t = U(GetRgnBox)(h, &rc);
        CHECK(t == COMPLEXREGION || t == SIMPLEREGION);
        CHECKF(rc.left >= -15 && rc.left <= -13 && rc.right >= 13 && rc.right <= 15, "x %ld..%ld", rc.left, rc.right);
        CHECKF(rc.top >= 0 && rc.top <= 1 && rc.bottom >= 28 && rc.bottom <= 30, "y %ld..%ld", rc.top, rc.bottom);
        RgGrid g; rg_grid_of(h, &g);
        t_note("rotated 45deg region cells=%d (box area %ld)", rg_grid_cnt(&g), (rc.right - rc.left) * (rc.bottom - rc.top));
        U(DeleteObject)(h);
    }
    free(d);
}
TEST(regions_extcreate_xform_zero_matrix) {
    REQUIRE(U(ExtCreateRegion)); REQUIRE(U(DeleteObject));
    RECT r[2] = {{0, 0, 10, 10}, {20, 20, 30, 30}};
    int sz; RGNDATA *d = mkdata(r, 2, &sz);
    XFORM x = {0, 0, 0, 0, 0, 0};
    HRGN h = U(ExtCreateRegion)(&x, sz, d);
    t_note("singular xform -> %s", h ? "valid" : "NULL");
    if (h) { t_note("type %d", rg_type(h)); U(DeleteObject)(h); }
    free(d);
}
TEST(regions_extcreate_roundtrip_getregiondata) {
    REQUIRE(U(ExtCreateRegion)); REQUIRE(U(GetRegionData)); REQUIRE(U(EqualRgn)); REQUIRE(U(DeleteObject));
    HRGN a = rg_rect(0, 0, 10, 10), b = rg_rect(5, 5, 25, 15), c = rg_rect(30, 0, 40, 40);
    U(CombineRgn)(a, a, b, RGN_OR); U(CombineRgn)(a, a, c, RGN_OR);
    DWORD sz; RGNDATA *d = rg_getdata(a, &sz);
    CHECK(d != NULL);
    if (d) {
        HRGN h = U(ExtCreateRegion)(NULL, sz, d);
        CHECK(h != NULL && U(EqualRgn)(a, h));
        if (h) U(DeleteObject)(h);
        free(d);
    }
    U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(c);
}
TEST(regions_extcreate_overlapping_rects_union) {
    REQUIRE(U(ExtCreateRegion)); REQUIRE(U(DeleteObject));
    RECT r[2] = {{0, 0, 20, 20}, {10, 10, 30, 30}};
    int sz; RGNDATA *d = mkdata(r, 2, &sz);
    HRGN h = U(ExtCreateRegion)(NULL, sz, d);
    t_note("overlapping rects in RGNDATA -> %s", h ? "valid" : "NULL");
    if (h) {
        t_note("pt(15,15)=%d pt(25,25)=%d pt(5,25)=%d", U(PtInRegion)(h, 15, 15), U(PtInRegion)(h, 25, 25), U(PtInRegion)(h, 5, 25));
        U(DeleteObject)(h);
    }
    free(d);
}

/* ---- object type / handle basics ---- */
TEST(regions_objecttype) {
    REQUIRE(U(GetObjectType)); REQUIRE(U(DeleteObject));
    HRGN h = rg_rect(0, 0, 5, 5);
    CHECK(U(GetObjectType)((HGDIOBJ)h) == OBJ_REGION);
    CHECK(U(DeleteObject)(h));
    SET_ERR(0);
    CHECK(U(GetObjectType)((HGDIOBJ)h) == 0);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    HRGN e = rg_rect(1, 1, 1, 1);
    CHECK(U(GetObjectType)((HGDIOBJ)e) == OBJ_REGION);
    U(DeleteObject)(e);
    HRGN el = U(CreateEllipticRgn)(0, 0, 10, 10);
    CHECK(U(GetObjectType)((HGDIOBJ)el) == OBJ_REGION);
    U(DeleteObject)(el);
}
TEST(regions_getobject_region) {
    REQUIRE(U(GetObjectW)); REQUIRE(U(DeleteObject));
    HRGN h = rg_rect(0, 0, 5, 5);
    char buf[64] = {0};
    int r = U(GetObjectW)(h, sizeof buf, buf);
    t_note("GetObjectW(region) = %d", r);
    CHECK(r == 0);
    r = U(GetObjectW)(h, 0, NULL);
    CHECK(r == 0);
    U(DeleteObject)(h);
}
TEST(regions_delete_twice) {
    REQUIRE(U(DeleteObject));
    HRGN h = rg_rect(0, 0, 5, 5);
    CHECK(U(DeleteObject)(h));
    SET_ERR(0);
    CHECK(!U(DeleteObject)(h));
    CHECK(!U(DeleteObject)(NULL));
    CHECK(rg_type(h) == 0);
}
TEST(regions_deleted_handle_use) {
    REQUIRE(U(DeleteObject)); REQUIRE(U(CombineRgn)); REQUIRE(U(OffsetRgn)); REQUIRE(U(EqualRgn));
    HRGN h = rg_rect(0, 0, 5, 5), ok = rg_rect(0, 0, 5, 5);
    U(DeleteObject)(h);
    RECT rc = {0};
    CHECK(U(GetRgnBox)(h, &rc) == 0);
    CHECK(U(PtInRegion)(h, 1, 1) == FALSE);
    CHECK(U(RectInRegion)(h, &(RECT){0, 0, 5, 5}) == FALSE);
    CHECK(U(OffsetRgn)(h, 1, 1) == 0);
    CHECK(U(EqualRgn)(h, ok) == FALSE);
    CHECK(U(CombineRgn)(ok, h, ok, RGN_OR) == 0);
    CHECK(U(CombineRgn)(h, ok, ok, RGN_OR) == 0);
    CHECK(U(GetRegionData)(h, 0, NULL) == 0);
    U(DeleteObject)(ok);
}
TEST(regions_wrong_object_types) {
    REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject)); REQUIRE(U(CreatePen));
    HBRUSH br = U(CreateSolidBrush)(RGB(1, 2, 3));
    HPEN pn = U(CreatePen)(PS_SOLID, 1, 0);
    HRGN ok = rg_rect(0, 0, 5, 5);
    RECT rc;
    CHECK(U(GetRgnBox)((HRGN)br, &rc) == 0);
    CHECK(U(PtInRegion)((HRGN)pn, 1, 1) == FALSE);
    CHECK(U(OffsetRgn)((HRGN)br, 1, 1) == 0);
    CHECK(U(EqualRgn)((HRGN)br, ok) == FALSE);
    CHECK(U(CombineRgn)(ok, (HRGN)br, ok, RGN_OR) == 0);
    CHECK(U(CombineRgn)((HRGN)pn, ok, ok, RGN_OR) == 0);
    CHECK(U(GetRegionData)((HRGN)br, 0, NULL) == 0);
    CHECK(rg_type(ok) == SIMPLEREGION);
    U(DeleteObject)(br); U(DeleteObject)(pn); U(DeleteObject)(ok);
}
TEST(regions_bogus_handles) {
    REQUIRE(U(GetRgnBox));
    HRGN bogus[] = {NULL, (HRGN)(ULONG_PTR)1, (HRGN)(ULONG_PTR)0xFFFFFFFF, (HRGN)(ULONG_PTR)0x12345678, (HRGN)(ULONG_PTR)0xDEADBEEF};
    HRGN ok = rg_rect(0, 0, 5, 5);
    for (int i = 0; i < 5; i++) {
        RECT rc;
        CHECKF(U(GetRgnBox)(bogus[i], &rc) == 0, "bogus %d", i);
        CHECKF(U(PtInRegion)(bogus[i], 0, 0) == FALSE, "bogus %d", i);
        CHECKF(U(OffsetRgn)(bogus[i], 1, 1) == 0, "bogus %d", i);
        CHECKF(U(EqualRgn)(bogus[i], ok) == FALSE, "bogus %d", i);
        CHECKF(U(CombineRgn)(bogus[i], ok, ok, RGN_OR) == 0, "bogus %d", i);
        CHECKF(U(CombineRgn)(ok, bogus[i], ok, RGN_OR) == 0, "bogus %d", i);
        CHECKF(U(CombineRgn)(ok, ok, bogus[i], RGN_OR) == 0, "bogus %d", i);
        CHECKF(U(GetRegionData)(bogus[i], 0, NULL) == 0, "bogus %d", i);
    }
    U(DeleteObject)(ok);
}
