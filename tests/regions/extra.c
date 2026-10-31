#include "rg.h"

TEST(regions_extra_ptin_rectin_null_region) {
    REQUIRE(U(PtInRegion)); REQUIRE(U(RectInRegion));
    CHECK(U(PtInRegion)(NULL, 0, 0) == FALSE);
    CHECK(U(PtInRegion)((HRGN)(ULONG_PTR)0x1234, 0, 0) == FALSE);
    RECT rc = {0, 0, 4, 4};
    CHECK(U(RectInRegion)(NULL, &rc) == FALSE);
    CHECK(U(RectInRegion)((HRGN)(ULONG_PTR)0x1234, &rc) == FALSE);
}

TEST(regions_extra_getregiondata_null_region) {
    REQUIRE(U(GetRegionData));
    CHECK(U(GetRegionData)(NULL, 0, NULL) == 0);
    BYTE buf[64]; memset(buf, 0xCC, sizeof buf);
    CHECK(U(GetRegionData)(NULL, sizeof buf, (RGNDATA *)buf) == 0);
    CHECK(U(GetRegionData)((HRGN)(ULONG_PTR)0x1234, sizeof buf, (RGNDATA *)buf) == 0);
}

TEST(regions_extra_ellipse_translation_invariant) {
    REQUIRE(U(CreateEllipticRgn)); REQUIRE(U(OffsetRgn)); REQUIRE(U(EqualRgn)); REQUIRE(U(DeleteObject));
    static const int sz[][2] = {{5, 5}, {8, 13}, {20, 20}, {31, 17}};
    for (unsigned i = 0; i < sizeof sz / sizeof sz[0]; i++) {
        HRGN a = U(CreateEllipticRgn)(0, 0, sz[i][0], sz[i][1]);
        HRGN b = U(CreateEllipticRgn)(37, -11, 37 + sz[i][0], -11 + sz[i][1]);
        CHECK(a && b);
        if (a && b) {
            U(OffsetRgn)(a, 37, -11);
            CHECKF(U(EqualRgn)(a, b), "ellipse %d offset equal", (int)i);
        }
        if (a) U(DeleteObject)(a);
        if (b) U(DeleteObject)(b);
    }
}

TEST(regions_extra_roundrect_translation_invariant) {
    REQUIRE(U(CreateRoundRectRgn)); REQUIRE(U(OffsetRgn)); REQUIRE(U(EqualRgn)); REQUIRE(U(DeleteObject));
    HRGN a = U(CreateRoundRectRgn)(0, 0, 30, 20, 8, 6);
    HRGN b = U(CreateRoundRectRgn)(-5, 9, 25, 29, 8, 6);
    CHECK(a && b);
    if (a && b) {
        U(OffsetRgn)(a, -5, 9);
        CHECK(U(EqualRgn)(a, b));
    }
    if (a) U(DeleteObject)(a);
    if (b) U(DeleteObject)(b);
}

TEST(regions_extra_xor_self_is_null_or_self) {
    REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetRgnBox));
    HRGN a = U(CreateEllipticRgn)(0, 0, 25, 25), b = rg_rect(0, 0, 1, 1), c = rg_rect(0, 0, 1, 1);
    HRGN a2 = U(CreateEllipticRgn)(0, 0, 25, 25);
    CHECK(U(CombineRgn)(b, a, a, RGN_XOR) == NULLREGION);
    CHECK(U(CombineRgn)(c, a, a, RGN_DIFF) == NULLREGION);
    RECT rc; CHECK(U(GetRgnBox)(b, &rc) == NULLREGION);
    CHECK(U(CombineRgn)(b, a, a2, RGN_AND) == COMPLEXREGION);
    if (!rg_is_wine()) CHECK(U(EqualRgn)(a, b));
    CHECK(U(CombineRgn)(c, a, a2, RGN_OR) == COMPLEXREGION);
    if (!rg_is_wine()) CHECK(U(EqualRgn)(a, c));
    U(DeleteObject)(a); U(DeleteObject)(a2); U(DeleteObject)(b); U(DeleteObject)(c);
}

TEST(regions_extra_getregiondata_exact_sizes) {
    REQUIRE(U(GetRegionData)); REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    HRGN a = rg_rect(0, 0, 10, 10), b = rg_rect(20, 20, 30, 30);
    U(CombineRgn)(a, a, b, RGN_OR);
    DWORD need = U(GetRegionData)(a, 0, NULL);
    CHECK(need == sizeof(RGNDATAHEADER) + 2 * sizeof(RECT));
    BYTE *buf = (BYTE *)calloc(1, need + 16);
    if (buf) {
        RGNDATA *d = (RGNDATA *)buf;
        CHECK(U(GetRegionData)(a, need, d) == need);
        CHECK(d->rdh.nCount == 2);
        CHECK(U(GetRegionData)(a, need + 16, d) == need);
        U(DeleteObject)(b);
        free(buf);
    }
    U(DeleteObject)(a);
}
