#include "rg.h"

typedef int (WINAPI *combine_fn)(HRGN, HRGN, HRGN, int);

/* build a random region (1..4 rects) and mirror it in a grid */
static HRGN rand_region(RgGrid *g, int maxrects) {
    HRGN h = rg_rect(0, 0, 0, 0), t = rg_rect(0, 0, 0, 0);
    memset(g, 0, sizeof *g);
    int n = 1 + rg_rand() % maxrects;
    for (int i = 0; i < n; i++) {
        int l = rg_rand() % 60, tp = rg_rand() % 60;
        int r = l + 1 + rg_rand() % 30, b = tp + 1 + rg_rand() % 30;
        if (r > RG_N) r = RG_N;
        if (b > RG_N) b = RG_N;
        U(SetRectRgn)(t, l, tp, r, b);
        U(CombineRgn)(h, h, t, RGN_OR);
        rg_grid_rect(g, l, tp, r, b);
    }
    U(DeleteObject)(t);
    return h;
}
static void grid_op(RgGrid *d, const RgGrid *a, const RgGrid *b, int mode) {
    for (int y = 0; y < RG_N; y++) for (int x = 0; x < RG_N; x++) {
        int p = a->c[y][x], q = b->c[y][x], r = 0;
        switch (mode) {
        case RGN_AND: r = p & q; break;
        case RGN_OR: r = p | q; break;
        case RGN_XOR: r = p ^ q; break;
        case RGN_DIFF: r = p & !q; break;
        case RGN_COPY: r = p; break;
        }
        d->c[y][x] = r;
    }
}
static const char *modename(int m) { static const char *n[] = {"?", "AND", "OR", "XOR", "DIFF", "COPY"}; return m >= 1 && m <= 5 ? n[m] : "?"; }

/* ---- basic semantics on simple operands ---- */
TEST(regions_combine_and) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN d = rg_rect(0, 0, 1, 1), a = rg_rect(0, 0, 20, 20), b = rg_rect(10, 10, 30, 30);
    RECT rc;
    CHECK(U(CombineRgn)(d, a, b, RGN_AND) == SIMPLEREGION);
    U(GetRgnBox)(d, &rc);
    CHECK(rc.left == 10 && rc.top == 10 && rc.right == 20 && rc.bottom == 20);
    U(DeleteObject)(d); U(DeleteObject)(a); U(DeleteObject)(b);
}
TEST(regions_combine_and_disjoint_and_touching) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN d = rg_rect(0, 0, 1, 1), a = rg_rect(0, 0, 10, 10), b = rg_rect(20, 20, 30, 30), c = rg_rect(10, 0, 20, 10);
    RECT rc;
    CHECK(U(CombineRgn)(d, a, b, RGN_AND) == NULLREGION);
    CHECK(U(GetRgnBox)(d, &rc) == NULLREGION);
    U(SetRectRgn)(d, 0, 0, 100, 100);
    CHECK(U(CombineRgn)(d, a, c, RGN_AND) == NULLREGION);    /* edge-touching */
    CHECK(!U(PtInRegion)(d, 5, 5) && !U(PtInRegion)(d, 10, 5));
    U(DeleteObject)(d); U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(c);
}
TEST(regions_combine_or) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN d = rg_rect(0, 0, 1, 1), a = rg_rect(0, 0, 10, 10), b = rg_rect(20, 20, 30, 30), c = rg_rect(10, 0, 20, 10), e = rg_rect(2, 2, 5, 5);
    CHECK(U(CombineRgn)(d, a, b, RGN_OR) == COMPLEXREGION);
    CHECK(U(CombineRgn)(d, a, c, RGN_OR) == SIMPLEREGION);   /* adjacent, same height */
    RECT rc; U(GetRgnBox)(d, &rc);
    CHECK(rc.left == 0 && rc.right == 20 && rc.top == 0 && rc.bottom == 10);
    CHECK(U(CombineRgn)(d, a, e, RGN_OR) == SIMPLEREGION);   /* contained */
    CHECK(U(EqualRgn)(d, a));
    U(DeleteObject)(d); U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(c); U(DeleteObject)(e);
}
TEST(regions_combine_xor) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN d = rg_rect(0, 0, 1, 1), a = rg_rect(0, 0, 20, 20), b = rg_rect(0, 0, 20, 20), c = rg_rect(10, 10, 30, 30);
    CHECK(U(CombineRgn)(d, a, b, RGN_XOR) == NULLREGION);
    CHECK(U(CombineRgn)(d, a, c, RGN_XOR) == COMPLEXREGION);
    CHECK(U(PtInRegion)(d, 5, 5) && U(PtInRegion)(d, 25, 25) && !U(PtInRegion)(d, 15, 15));
    U(DeleteObject)(d); U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(c);
}
TEST(regions_combine_diff) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN d = rg_rect(0, 0, 1, 1), a = rg_rect(0, 0, 20, 20), c = rg_rect(10, 10, 30, 30), big = rg_rect(-5, -5, 50, 50), mid = rg_rect(5, 5, 15, 15);
    CHECK(U(CombineRgn)(d, a, c, RGN_DIFF) == COMPLEXREGION);
    CHECK(U(PtInRegion)(d, 5, 5) && U(PtInRegion)(d, 15, 5) && U(PtInRegion)(d, 5, 15) && !U(PtInRegion)(d, 15, 15));
    CHECK(U(CombineRgn)(d, a, big, RGN_DIFF) == NULLREGION);
    CHECK(U(CombineRgn)(d, big, a, RGN_DIFF) == COMPLEXREGION);
    CHECK(U(CombineRgn)(d, a, mid, RGN_DIFF) == COMPLEXREGION);
    CHECK(!U(PtInRegion)(d, 10, 10) && U(PtInRegion)(d, 4, 10) && U(PtInRegion)(d, 15, 10));
    HRGN s = rg_rect(30, 30, 40, 40);
    CHECK(U(CombineRgn)(d, a, s, RGN_DIFF) == SIMPLEREGION);
    CHECK(U(EqualRgn)(d, a));
    U(DeleteObject)(s);
    U(DeleteObject)(d); U(DeleteObject)(a); U(DeleteObject)(c); U(DeleteObject)(big); U(DeleteObject)(mid);
}
TEST(regions_combine_diff_is_ordered) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN d1 = rg_rect(0, 0, 1, 1), d2 = rg_rect(0, 0, 1, 1), a = rg_rect(0, 0, 20, 20), b = rg_rect(10, 0, 40, 20);
    CHECK(U(CombineRgn)(d1, a, b, RGN_DIFF) == SIMPLEREGION);
    CHECK(U(CombineRgn)(d2, b, a, RGN_DIFF) == SIMPLEREGION);
    CHECK(!U(EqualRgn)(d1, d2));
    RECT r1, r2; U(GetRgnBox)(d1, &r1); U(GetRgnBox)(d2, &r2);
    CHECK(r1.left == 0 && r1.right == 10 && r2.left == 20 && r2.right == 40);
    U(DeleteObject)(d1); U(DeleteObject)(d2); U(DeleteObject)(a); U(DeleteObject)(b);
}
TEST(regions_combine_copy) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN d = rg_rect(0, 0, 1, 1), a = rg_rect(3, 3, 9, 9), e = rg_rect(0, 0, 0, 0), ell = U(CreateEllipticRgn)(0, 0, 20, 20);
    CHECK(U(CombineRgn)(d, a, NULL, RGN_COPY) == SIMPLEREGION);
    CHECK(U(EqualRgn)(d, a));
    CHECK(U(CombineRgn)(d, e, NULL, RGN_COPY) == NULLREGION);
    CHECK(U(EqualRgn)(d, e));
    CHECK(U(CombineRgn)(d, ell, a, RGN_COPY) == COMPLEXREGION);   /* src2 ignored */
    CHECK(U(EqualRgn)(d, ell));
    U(DeleteObject)(d); U(DeleteObject)(a); U(DeleteObject)(e); U(DeleteObject)(ell);
}
TEST(regions_combine_copy_independent) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN d = rg_rect(0, 0, 1, 1), a = rg_rect(3, 3, 9, 9);
    U(CombineRgn)(d, a, NULL, RGN_COPY);
    U(OffsetRgn)(d, 100, 100);
    RECT rc; U(GetRgnBox)(a, &rc);
    CHECK(rc.left == 3 && rc.right == 9);
    U(SetRectRgn)(a, 0, 0, 2, 2);
    U(GetRgnBox)(d, &rc);
    CHECK(rc.left == 103 && rc.right == 109);
    U(DeleteObject)(d); U(DeleteObject)(a);
}

/* ---- empty operands ---- */
TEST(regions_combine_empty_operands) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN e = rg_rect(0, 0, 0, 0), e2 = rg_rect(9, 9, 9, 20), a = rg_rect(0, 0, 10, 10), d = rg_rect(0, 0, 1, 1);
    CHECK(U(CombineRgn)(d, e, e2, RGN_AND) == NULLREGION);
    CHECK(U(CombineRgn)(d, e, e2, RGN_OR) == NULLREGION);
    CHECK(U(CombineRgn)(d, e, e2, RGN_XOR) == NULLREGION);
    CHECK(U(CombineRgn)(d, e, e2, RGN_DIFF) == NULLREGION);
    CHECK(U(CombineRgn)(d, a, e, RGN_AND) == NULLREGION);
    CHECK(U(CombineRgn)(d, e, a, RGN_AND) == NULLREGION);
    CHECK(U(CombineRgn)(d, a, e, RGN_OR) == SIMPLEREGION && U(EqualRgn)(d, a));
    CHECK(U(CombineRgn)(d, e, a, RGN_OR) == SIMPLEREGION && U(EqualRgn)(d, a));
    CHECK(U(CombineRgn)(d, a, e, RGN_XOR) == SIMPLEREGION && U(EqualRgn)(d, a));
    CHECK(U(CombineRgn)(d, e, a, RGN_XOR) == SIMPLEREGION && U(EqualRgn)(d, a));
    CHECK(U(CombineRgn)(d, a, e, RGN_DIFF) == SIMPLEREGION && U(EqualRgn)(d, a));
    CHECK(U(CombineRgn)(d, e, a, RGN_DIFF) == NULLREGION);
    U(DeleteObject)(e); U(DeleteObject)(e2); U(DeleteObject)(a); U(DeleteObject)(d);
}

/* ---- same handle as destination / sources ---- */
TEST(regions_combine_dest_is_src1) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    static const int modes[] = {RGN_AND, RGN_OR, RGN_XOR, RGN_DIFF, RGN_COPY};
    for (int i = 0; i < 5; i++) {
        HRGN a = rg_rect(0, 0, 20, 20), b = rg_rect(10, 10, 30, 30), ref = rg_rect(0, 0, 1, 1);
        int r1 = U(CombineRgn)(ref, a, b, modes[i]);
        int r2 = U(CombineRgn)(a, a, b, modes[i]);
        CHECKF(r1 == r2 && U(EqualRgn)(a, ref), "mode %s", modename(modes[i]));
        U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(ref);
    }
}
TEST(regions_combine_dest_is_src2) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    static const int modes[] = {RGN_AND, RGN_OR, RGN_XOR, RGN_DIFF};
    for (int i = 0; i < 4; i++) {
        HRGN a = rg_rect(0, 0, 20, 20), b = rg_rect(10, 10, 30, 30), ref = rg_rect(0, 0, 1, 1);
        int r1 = U(CombineRgn)(ref, a, b, modes[i]);
        int r2 = U(CombineRgn)(b, a, b, modes[i]);
        CHECKF(r1 == r2 && U(EqualRgn)(b, ref), "mode %s", modename(modes[i]));
        U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(ref);
    }
}
TEST(regions_combine_all_same_handle) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN a = U(CreateEllipticRgn)(0, 0, 30, 30), ref = U(CreateEllipticRgn)(0, 0, 30, 30), e = rg_rect(0, 0, 0, 0);
    { int rr = U(CombineRgn)(a, a, a, RGN_AND); CHECKF(rr == COMPLEXREGION, "AND ret %d", rr); CHECK(U(EqualRgn)(a, ref)); }
    CHECK(U(CombineRgn)(a, a, a, RGN_OR) == COMPLEXREGION && U(EqualRgn)(a, ref));
    CHECK(U(CombineRgn)(a, a, a, RGN_COPY) == COMPLEXREGION && U(EqualRgn)(a, ref));
    CHECK(U(CombineRgn)(a, a, a, RGN_XOR) == NULLREGION && U(EqualRgn)(a, e));
    U(CombineRgn)(a, ref, NULL, RGN_COPY);
    CHECK(U(CombineRgn)(a, a, a, RGN_DIFF) == NULLREGION && U(EqualRgn)(a, e));
    U(DeleteObject)(a); U(DeleteObject)(ref); U(DeleteObject)(e);
}
TEST(regions_combine_self_copy) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN a = U(CreateEllipticRgn)(0, 0, 30, 30), ref = U(CreateEllipticRgn)(0, 0, 30, 30);
    CHECK(U(CombineRgn)(a, a, NULL, RGN_COPY) == COMPLEXREGION);
    CHECK(U(EqualRgn)(a, ref));
    U(DeleteObject)(a); U(DeleteObject)(ref);
}

/* ---- invalid parameters ---- */
TEST(regions_combine_invalid_mode) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN d = rg_rect(0, 0, 5, 5), a = rg_rect(0, 0, 10, 10), b = rg_rect(5, 5, 15, 15);
    static const int bad[] = {0, 6, 7, -1, 100, 0x10000, (int)0x80000000};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        int r = U(CombineRgn)(d, a, b, bad[i]);
        CHECKF(r == 0, "mode %d -> %d", bad[i], r);
        RECT rc; U(GetRgnBox)(d, &rc);
        if (rc.right != 5) t_note("dest modified by invalid mode %d: right=%ld", bad[i], rc.right);
    }
    U(DeleteObject)(d); U(DeleteObject)(a); U(DeleteObject)(b);
}
TEST(regions_combine_null_handles) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN d = rg_rect(0, 0, 5, 5), a = rg_rect(0, 0, 10, 10);
    for (int m = RGN_AND; m <= RGN_COPY; m++) {
        CHECKF(U(CombineRgn)(NULL, a, a, m) == 0, "dest NULL mode %d", m);
        CHECKF(U(CombineRgn)(d, NULL, a, m) == 0, "src1 NULL mode %d", m);
        if (m != RGN_COPY) CHECKF(U(CombineRgn)(d, a, NULL, m) == 0, "src2 NULL mode %d", m);
    }
    CHECK(U(CombineRgn)(NULL, NULL, NULL, RGN_OR) == 0);
    RECT rc; U(GetRgnBox)(d, &rc);
    CHECK(rc.right == 5);
    U(DeleteObject)(d); U(DeleteObject)(a);
}
TEST(regions_combine_wrong_types) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(CreateCompatibleDC));
    HBRUSH br = U(CreateSolidBrush)(0);
    HDC dc = U(CreateCompatibleDC)(NULL);
    HRGN a = rg_rect(0, 0, 10, 10);
    CHECK(U(CombineRgn)((HRGN)br, a, a, RGN_OR) == 0);
    CHECK(U(CombineRgn)(a, (HRGN)br, a, RGN_OR) == 0);
    CHECK(U(CombineRgn)(a, a, (HRGN)br, RGN_OR) == 0);
    CHECK(U(CombineRgn)((HRGN)dc, a, a, RGN_OR) == 0);
    CHECK(U(CombineRgn)(a, (HRGN)dc, a, RGN_COPY) == 0);
    CHECK(rg_type(a) == SIMPLEREGION);
    U(DeleteObject)(br); U(DeleteDC)(dc); U(DeleteObject)(a);
}
TEST(regions_combine_dest_content_irrelevant) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN a = rg_rect(0, 0, 10, 10), b = rg_rect(5, 5, 15, 15);
    HRGN d1 = rg_rect(0, 0, 0, 0), d2 = U(CreateEllipticRgn)(-50, -50, 500, 500);
    int r1 = U(CombineRgn)(d1, a, b, RGN_XOR), r2 = U(CombineRgn)(d2, a, b, RGN_XOR);
    CHECK(r1 == r2 && U(EqualRgn)(d1, d2));
    U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(d1); U(DeleteObject)(d2);
}
TEST(regions_combine_sources_unchanged) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN a = U(CreateEllipticRgn)(0, 0, 40, 40), b = rg_rect(10, 10, 50, 30), d = rg_rect(0, 0, 0, 0);
    HRGN ac = U(CreateEllipticRgn)(0, 0, 40, 40), bc = rg_rect(10, 10, 50, 30);
    for (int m = RGN_AND; m <= RGN_COPY; m++) {
        U(CombineRgn)(d, a, b, m);
        CHECKF(U(EqualRgn)(a, ac) && U(EqualRgn)(b, bc), "mode %d", m);
    }
    U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(d); U(DeleteObject)(ac); U(DeleteObject)(bc);
}

/* ---- algebraic identities on random operands ---- */
TEST(regions_combine_identities) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    rg_seed = 4242;
    for (int it = 0; it < 40; it++) {
        RgGrid ga, gb; HRGN a = rand_region(&ga, 4), b = rand_region(&gb, 4);
        HRGN d1 = rg_rect(0, 0, 0, 0), d2 = rg_rect(0, 0, 0, 0), d3 = rg_rect(0, 0, 0, 0), d4 = rg_rect(0, 0, 0, 0);
        /* commutativity */
        U(CombineRgn)(d1, a, b, RGN_OR); U(CombineRgn)(d2, b, a, RGN_OR);
        CHECKF(U(EqualRgn)(d1, d2), "OR commutes it %d", it);
        U(CombineRgn)(d1, a, b, RGN_AND); U(CombineRgn)(d2, b, a, RGN_AND);
        CHECKF(U(EqualRgn)(d1, d2), "AND commutes it %d", it);
        U(CombineRgn)(d1, a, b, RGN_XOR); U(CombineRgn)(d2, b, a, RGN_XOR);
        CHECKF(U(EqualRgn)(d1, d2), "XOR commutes it %d", it);
        /* A xor B == (A - B) | (B - A) */
        U(CombineRgn)(d2, a, b, RGN_DIFF); U(CombineRgn)(d3, b, a, RGN_DIFF); U(CombineRgn)(d4, d2, d3, RGN_OR);
        CHECKF(U(EqualRgn)(d1, d4), "xor decomposition it %d", it);
        /* A | B == (A xor B) | (A & B) */
        U(CombineRgn)(d2, a, b, RGN_AND); U(CombineRgn)(d3, d1, d2, RGN_OR); U(CombineRgn)(d4, a, b, RGN_OR);
        CHECKF(U(EqualRgn)(d3, d4), "or decomposition it %d", it);
        /* A - A == empty, A & A == A */
        CHECKF(U(CombineRgn)(d1, a, a, RGN_DIFF) == NULLREGION, "A-A it %d", it);
        U(CombineRgn)(d1, a, a, RGN_AND);
        CHECKF(U(EqualRgn)(d1, a), "A&A it %d", it);
        /* (A - B) & B empty */
        U(CombineRgn)(d1, a, b, RGN_DIFF);
        CHECKF(U(CombineRgn)(d2, d1, b, RGN_AND) == NULLREGION, "(A-B)&B it %d", it);
        U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(d1); U(DeleteObject)(d2); U(DeleteObject)(d3); U(DeleteObject)(d4);
    }
}

/* ---- randomized comparison with a reference grid ---- */
static void stress_run(unsigned seed, int iters, int maxrects) {
    rg_seed = seed;
    static const int modes[] = {RGN_AND, RGN_OR, RGN_XOR, RGN_DIFF, RGN_COPY};
    int fails = 0;
    for (int it = 0; it < iters && fails < 5; it++) {
        RgGrid ga, gb, ge, gr;
        HRGN a = rand_region(&ga, maxrects), b = rand_region(&gb, maxrects);
        HRGN d = rg_rect(0, 0, 0, 0);
        for (int mi = 0; mi < 5; mi++) {
            int m = modes[mi];
            grid_op(&ge, &ga, &gb, m);
            int ret = U(CombineRgn)(d, a, b, m);
            rg_grid_of(d, &gr);
            int ok = rg_grid_eq(&ge, &gr) && ret == rg_grid_type(&ge);
            RECT bb, rc;
            if (ok) {
                int ty = U(GetRgnBox)(d, &rc);
                ok = ty == ret;
                if (ok && rg_grid_bbox(&ge, &bb)) ok = !memcmp(&bb, &rc, sizeof rc);
            }
            if (!ok) { fails++; CHECKF(0, "seed %u it %d mode %s ret %d expect %d", seed, it, modename(m), ret, rg_grid_type(&ge)); }
            else CHECK(1);
        }
        U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(d);
    }
}
TEST(regions_stress_rect_algebra_a) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(SetRectRgn)); REQUIRE(U(PtInRegion)); REQUIRE(U(DeleteObject));
    stress_run(1, 60, 3);
}
TEST(regions_stress_rect_algebra_b) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(SetRectRgn)); REQUIRE(U(PtInRegion)); REQUIRE(U(DeleteObject));
    stress_run(777, 50, 6);
}
TEST(regions_stress_rect_algebra_many_rects) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(SetRectRgn)); REQUIRE(U(PtInRegion)); REQUIRE(U(DeleteObject));
    stress_run(31337, 25, 12);
}
TEST(regions_stress_inplace_chain) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(SetRectRgn)); REQUIRE(U(PtInRegion)); REQUIRE(U(DeleteObject));
    rg_seed = 2024;
    HRGN acc = rg_rect(0, 0, 0, 0), t = rg_rect(0, 0, 0, 0);
    RgGrid g; memset(&g, 0, sizeof g);
    static const int modes[] = {RGN_OR, RGN_XOR, RGN_DIFF, RGN_AND, RGN_OR, RGN_OR, RGN_XOR};
    for (int i = 0; i < 80; i++) {
        int l = rg_rand() % 60, tp = rg_rand() % 60, r = l + 1 + rg_rand() % 40, b = tp + 1 + rg_rand() % 40;
        if (r > RG_N) r = RG_N;
        if (b > RG_N) b = RG_N;
        int m = modes[rg_rand() % 7];
        RgGrid gt; memset(&gt, 0, sizeof gt); rg_grid_rect(&gt, l, tp, r, b);
        RgGrid ge; grid_op(&ge, &g, &gt, m);
        if (m == RGN_AND && i % 3) { m = RGN_OR; grid_op(&ge, &g, &gt, m); }   /* keep the accumulator alive */
        U(SetRectRgn)(t, l, tp, r, b);
        int ret = U(CombineRgn)(acc, acc, t, m);
        RgGrid gr; rg_grid_of(acc, &gr);
        CHECKF(rg_grid_eq(&gr, &ge) && ret == rg_grid_type(&ge), "step %d mode %s ret %d", i, modename(m), ret);
        g = ge;
    }
    U(DeleteObject)(acc); U(DeleteObject)(t);
}
TEST(regions_stress_offset_vs_grid) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(OffsetRgn)); REQUIRE(U(PtInRegion)); REQUIRE(U(DeleteObject));
    rg_seed = 55;
    for (int it = 0; it < 20; it++) {
        RgGrid ga, gm; HRGN a = rand_region(&ga, 4);
        int dx = (int)(rg_rand() % 21) - 10, dy = (int)(rg_rand() % 21) - 10;
        memset(&gm, 0, sizeof gm);
        for (int y = 0; y < RG_N; y++) for (int x = 0; x < RG_N; x++) if (ga.c[y][x]) {
            int nx = x + dx, ny = y + dy; if (nx >= 0 && ny >= 0 && nx < RG_N && ny < RG_N) gm.c[ny][nx] = 1; }
        U(OffsetRgn)(a, dx, dy);
        RgGrid gr; rg_grid_of(a, &gr);
        CHECKF(rg_grid_eq(&gr, &gm), "it %d off %d,%d", it, dx, dy);
        U(DeleteObject)(a);
    }
}
TEST(regions_stress_regiondata_roundtrip) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(ExtCreateRegion)); REQUIRE(U(GetRegionData)); REQUIRE(U(PtInRegion)); REQUIRE(U(DeleteObject));
    rg_seed = 9001;
    for (int it = 0; it < 30; it++) {
        RgGrid ga; HRGN a = rand_region(&ga, 8);
        DWORD sz; RGNDATA *d = rg_getdata(a, &sz);
        CHECK(d != NULL);
        if (d) {
            HRGN h = U(ExtCreateRegion)(NULL, sz, d);
            CHECK(h != NULL);
            if (h) {
                RgGrid gh; rg_grid_of(h, &gh);
                CHECKF(rg_grid_eq(&ga, &gh) && U(EqualRgn)(a, h), "it %d", it);
                U(DeleteObject)(h);
            }
            free(d);
        }
        U(DeleteObject)(a);
    }
}
TEST(regions_stress_ellipse_algebra) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(CreateEllipticRgn)); REQUIRE(U(PtInRegion)); REQUIRE(U(DeleteObject));
    HRGN a = U(CreateEllipticRgn)(0, 0, 40, 40), b = U(CreateEllipticRgn)(20, 10, 60, 50), d = rg_rect(0, 0, 0, 0);
    RgGrid ga, gb, ge, gr; rg_grid_of(a, &ga); rg_grid_of(b, &gb);
    for (int m = RGN_AND; m <= RGN_COPY; m++) {
        grid_op(&ge, &ga, &gb, m);
        int ret = U(CombineRgn)(d, a, b, m);
        rg_grid_of(d, &gr);
        CHECKF(rg_grid_eq(&ge, &gr) && ret == rg_grid_type(&ge), "mode %s ret %d", modename(m), ret);
    }
    U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(d);
}
TEST(regions_combine_return_vs_box) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    rg_seed = 123;
    for (int it = 0; it < 40; it++) {
        RgGrid ga, gb; HRGN a = rand_region(&ga, 3), b = rand_region(&gb, 3), d = rg_rect(0, 0, 0, 0);
        int m = RGN_AND + it % 5;
        int ret = U(CombineRgn)(d, a, b, m);
        RECT rc; int ty = U(GetRgnBox)(d, &rc);
        CHECKF(ret == ty, "it %d", it);
        U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(d);
    }
}
TEST(regions_combine_extreme_coords) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN a = rg_rect(-100000, -100000, 100000, 100000), b = rg_rect(-1, -1, 1, 1), d = rg_rect(0, 0, 1, 1);
    CHECK(U(CombineRgn)(d, a, b, RGN_DIFF) == COMPLEXREGION);
    CHECK(!U(PtInRegion)(d, 0, 0) && !U(PtInRegion)(d, -1, -1) && U(PtInRegion)(d, 1, 1) && U(PtInRegion)(d, -2, -2));
    CHECK(U(CombineRgn)(d, d, b, RGN_OR) == SIMPLEREGION);
    CHECK(U(EqualRgn)(d, a));
    U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(d);
}
TEST(regions_combine_checkerboard) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetRegionData));
    HRGN acc = rg_rect(0, 0, 0, 0), t = rg_rect(0, 0, 0, 0);
    int cnt = 0;
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) if ((x + y) & 1) { U(SetRectRgn)(t, x * 4, y * 4, x * 4 + 4, y * 4 + 4); U(CombineRgn)(acc, acc, t, RGN_OR); cnt++; }
    CHECK(cnt == 32);
    DWORD sz; RGNDATA *d = rg_getdata(acc, &sz);
    CHECK(d && d->rdh.nCount == 32);
    free(d);
    RgGrid g; rg_grid_of(acc, &g);
    CHECK(rg_grid_cnt(&g) == 32 * 16);
    CHECK(U(PtInRegion)(acc, 4, 0) && !U(PtInRegion)(acc, 0, 0) && !U(PtInRegion)(acc, 4, 4) && U(PtInRegion)(acc, 7, 7 - 4 + 0));
    U(DeleteObject)(acc); U(DeleteObject)(t);
}
