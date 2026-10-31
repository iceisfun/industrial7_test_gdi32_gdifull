/* GdiTransparentBlt, GdiAlphaBlend, GdiGradientFill, MaskBlt, PlgBlt on DIB-section memory DCs. */
#include "gap10.h"

static COLORREF rgbq(DWORD argb) { return RGB((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF); }
static int near_c(COLORREF a, COLORREF b, int tol) {
    return abs((int)GetRValue(a) - (int)GetRValue(b)) <= tol && abs((int)GetGValue(a) - (int)GetGValue(b)) <= tol &&
           abs((int)GetBValue(a) - (int)GetBValue(b)) <= tol;
}
static int mk_split(MD *m, int w, int h, int bpp, COLORREF l, COLORREF r) {
    if (!md_create(m, w, h, bpp)) return 0;
    md_rect(m, 0, 0, w / 2, h, l);
    md_rect(m, w / 2, 0, w - w / 2, h, r);
    return 1;
}
#define NEED_BLT() NEED_MD()

/* ============================ GdiTransparentBlt ============================ */
#define NEED_TB() do { NEED_BLT(); REQUIRE(UG(GdiTransparentBlt) ? UG(GdiTransparentBlt) : U(GdiTransparentBlt)); } while (0)
#define TB U(GdiTransparentBlt)

TEST(gap10_tblt_nonpositive_sizes_fail_silently) {
    NEED_BLT(); REQUIRE(TB);
    MD d, s; int bad[][4] = { {0,4,4,4}, {4,0,4,4}, {4,4,0,4}, {4,4,4,0}, {-4,4,4,4}, {4,-4,4,4}, {4,4,-4,4}, {4,4,4,-4}, {0,0,0,0} }; unsigned i;
    if (!md_create(&d, 16, 16, 32) || !md_create(&s, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, RED_);
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        CLR();
        BOOL r = TB(d.dc, 0, 0, bad[i][0], bad[i][1], s.dc, 0, 0, bad[i][2], bad[i][3], BLUE_);
        CHECKF(!r, "case %u", i);
        CHECKF(GetLastError() == ERRS, "case %u err %lu", i, GetLastError());
    }
    EXPECT_EQ(md_diff(&d, WHITE_), 0);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_tblt_null_source_fails_silently) {
    NEED_BLT(); REQUIRE(TB);
    MD d;
    if (!md_create(&d, 16, 16, 32)) { t_skip("no DIB"); return; }
    CLR();
    CHECK(!TB(d.dc, 0, 0, 8, 8, NULL, 0, 0, 8, 8, 0));
    EXPECT_EQ(GetLastError(), ERRS);
    md_destroy(&d);
}
TEST(gap10_tblt_invalid_dest_sets_6) {
    NEED_BLT(); REQUIRE(TB);
    MD s;
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef, (HDC)(ULONG_PTR)0xFFFFFFFF }; unsigned i;
    if (!md_create(&s, 16, 16, 32)) { t_skip("no DIB"); return; }
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        CLR();
        CHECKF(!TB(bad[i], 0, 0, 8, 8, s.dc, 0, 0, 8, 8, 0), "dest %p", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "dest %p err %lu", bad[i], GetLastError());
    }
    md_destroy(&s);
}
TEST(gap10_tblt_dest_wrong_object_type) {
    NEED_BLT(); REQUIRE(TB);
    MD s; HBRUSH b = U(CreateSolidBrush)(0);
    if (!md_create(&s, 16, 16, 32)) { t_skip("no DIB"); return; }
    CLR();
    CHECK(!TB((HDC)b, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, 0));
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    U(DeleteObject)(b);
    md_destroy(&s);
}
TEST(gap10_tblt_invalid_source_fails) {
    NEED_BLT(); REQUIRE(TB);
    MD d; HDC bad[] = { (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef }; unsigned i;
    if (!md_create(&d, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_);
    for (i = 0; i < 2; i++) {
        CLR();
        CHECKF(!TB(d.dc, 0, 0, 8, 8, bad[i], 0, 0, 8, 8, 0), "src %p", bad[i]);
        t_note("bogus src %p: err %lu", bad[i], GetLastError());
    }
    EXPECT_EQ(md_diff(&d, WHITE_), 0);
    md_destroy(&d);
}
TEST(gap10_tblt_deleted_source_dc) {
    NEED_BLT(); REQUIRE(TB);
    MD d; HDC s = U(CreateCompatibleDC)(NULL);
    if (!md_create(&d, 16, 16, 32)) { t_skip("no DIB"); return; }
    U(DeleteDC)(s);
    CLR();
    CHECK(!TB(d.dc, 0, 0, 8, 8, s, 0, 0, 8, 8, 0));
    md_destroy(&d);
}
TEST(gap10_tblt_colorkey_basic_all_depths) {
    NEED_BLT(); REQUIRE(TB);
    int bpps[] = { 32, 24, 16 }; unsigned k;
    for (k = 0; k < 3; k++) {
        MD d, s;
        if (!md_create(&d, 16, 16, bpps[k]) || !mk_split(&s, 8, 8, bpps[k], RED_, GREEN_)) continue;
        md_clear(&d, WHITE_);
        CHECK(TB(d.dc, 4, 4, 8, 8, s.dc, 0, 0, 8, 8, RED_));
        CHECKF(md_count(&d, 4, 4, 8, 12, WHITE_) == 4 * 8, "bpp %d keyed half stays", bpps[k]);
        CHECKF(md_count(&d, 8, 4, 12, 12, GREEN_) == 4 * 8, "bpp %d opaque half copied", bpps[k]);
        CHECKF(md_diff(&d, WHITE_) == 32, "bpp %d total changed %d", bpps[k], md_diff(&d, WHITE_));
        md_destroy(&d); md_destroy(&s);
    }
}
TEST(gap10_tblt_cross_depth) {
    NEED_BLT(); REQUIRE(TB);
    int sb[] = { 32, 24, 16, 32 }, db[] = { 24, 32, 32, 16 }; unsigned k;
    for (k = 0; k < 4; k++) {
        MD d, s;
        if (!md_create(&d, 16, 16, db[k]) || !mk_split(&s, 8, 8, sb[k], BLUE_, GREEN_)) continue;
        md_clear(&d, WHITE_);
        CHECK(TB(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, BLUE_));
        CHECKF(md_count(&d, 4, 0, 8, 8, GREEN_) == 32, "src %d dst %d green", sb[k], db[k]);
        CHECKF(md_count(&d, 0, 0, 4, 8, WHITE_) == 32, "src %d dst %d keyed", sb[k], db[k]);
        md_destroy(&d); md_destroy(&s);
    }
}
TEST(gap10_tblt_key_not_present_copies_all) {
    NEED_BLT(); REQUIRE(TB);
    MD d, s;
    if (!md_create(&d, 16, 16, 32) || !mk_split(&s, 8, 8, 32, RED_, GREEN_)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_);
    CHECK(TB(d.dc, 2, 2, 8, 8, s.dc, 0, 0, 8, 8, BLUE_));
    EXPECT_EQ(md_count(&d, 2, 2, 6, 10, RED_), 32);
    EXPECT_EQ(md_count(&d, 6, 2, 10, 10, GREEN_), 32);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_tblt_all_keyed_leaves_dest) {
    NEED_BLT(); REQUIRE(TB);
    MD d, s;
    if (!md_create(&d, 16, 16, 32) || !md_create(&s, 8, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, 0x00336699); md_clear(&s, RED_);
    CHECK(TB(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, RED_));
    EXPECT_EQ(md_diff(&d, 0x00336699), 0);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_tblt_stretch_2x_pixels) {
    NEED_BLT(); REQUIRE(TB);
    MD d, s; int x, y;
    if (!md_create(&d, 32, 32, 32) || !mk_split(&s, 8, 8, 32, RED_, GREEN_)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_);
    CHECK(TB(d.dc, 0, 0, 16, 16, s.dc, 0, 0, 8, 8, RED_));
    for (y = 0; y < 16; y++) for (x = 0; x < 16; x++) {
        COLORREF want = x < 8 ? WHITE_ : GREEN_, got = md_px(&d, x, y);
        CHECKF(got == want, "(%d,%d) %06lx want %06lx", x, y, got, want);
    }
    EXPECT_EQ(md_px(&d, 16, 0), WHITE_); EXPECT_EQ(md_px(&d, 0, 16), WHITE_);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_tblt_shrink_half) {
    NEED_BLT(); REQUIRE(TB);
    MD d, s;
    if (!md_create(&d, 16, 16, 32) || !mk_split(&s, 16, 16, 32, RED_, GREEN_)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_);
    CHECK(TB(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 16, 16, RED_));
    EXPECT_EQ(md_px(&d, 1, 1), WHITE_); EXPECT_EQ(md_px(&d, 6, 6), GREEN_); EXPECT_EQ(md_px(&d, 7, 0), GREEN_);
    EXPECT_EQ(md_px(&d, 8, 8), WHITE_);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_tblt_source_offset) {
    NEED_BLT(); REQUIRE(TB);
    MD d, s;
    if (!md_create(&d, 16, 16, 32) || !mk_split(&s, 16, 8, 32, RED_, GREEN_)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_);
    CHECK(TB(d.dc, 0, 0, 8, 8, s.dc, 8, 0, 8, 8, RED_));   /* right half of the source, all green */
    EXPECT_EQ(md_count(&d, 0, 0, 8, 8, GREEN_), 64);
    md_clear(&d, WHITE_);
    CHECK(TB(d.dc, 0, 0, 8, 8, s.dc, 4, 0, 8, 8, RED_));   /* 4 red + 4 green columns */
    EXPECT_EQ(md_count(&d, 0, 0, 4, 8, WHITE_), 32);
    EXPECT_EQ(md_count(&d, 4, 0, 8, 8, GREEN_), 32);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_tblt_dest_clipped) {
    NEED_BLT(); REQUIRE(TB); REQUIRE(U(IntersectClipRect));
    MD d, s;
    if (!md_create(&d, 16, 16, 32) || !mk_split(&s, 8, 8, 32, BLUE_, GREEN_)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_);
    U(IntersectClipRect)(d.dc, 0, 0, 6, 16);
    CHECK(TB(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, BLUE_));
    EXPECT_EQ(md_count(&d, 4, 0, 6, 8, GREEN_), 16);
    EXPECT_EQ(md_count(&d, 6, 0, 8, 8, GREEN_), 0);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_tblt_partly_offscreen_dest) {
    NEED_BLT(); REQUIRE(TB);
    MD d, s;
    if (!md_create(&d, 16, 16, 32) || !md_create(&s, 8, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, GREEN_);
    CHECK(TB(d.dc, 12, 12, 8, 8, s.dc, 0, 0, 8, 8, RED_));
    EXPECT_EQ(md_count(&d, 12, 12, 16, 16, GREEN_), 16);
    EXPECT_EQ(md_diff(&d, WHITE_), 16);
    CHECK(TB(d.dc, -4, -4, 8, 8, s.dc, 0, 0, 8, 8, RED_));
    EXPECT_EQ(md_count(&d, 0, 0, 4, 4, GREEN_), 16);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_tblt_source_rect_outside_observed) {
    NEED_BLT(); REQUIRE(TB);
    MD d, s;
    if (!md_create(&d, 16, 16, 32) || !md_create(&s, 8, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, GREEN_);
    CLR();
    BOOL r = TB(d.dc, 0, 0, 8, 8, s.dc, 20, 20, 8, 8, RED_);
    t_note("source fully outside -> %d err %lu, dest changed %d", r, GetLastError(), md_diff(&d, WHITE_));
    r = TB(d.dc, 0, 0, 8, 8, s.dc, 4, 4, 8, 8, RED_);
    t_note("source partly outside -> %d err %lu, dest changed %d", r, GetLastError(), md_diff(&d, WHITE_));
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_tblt_same_dc_observed) {
    NEED_BLT(); REQUIRE(TB);
    MD m;
    if (!md_create(&m, 32, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_); md_rect(&m, 0, 0, 8, 8, GREEN_); md_rect(&m, 0, 0, 4, 8, RED_);
    BOOL r = TB(m.dc, 16, 0, 8, 8, m.dc, 0, 0, 8, 8, RED_);
    t_note("same-DC transparent blt -> %d", r);
    if (r) { EXPECT_EQ(md_px(&m, 17, 2), WHITE_); EXPECT_EQ(md_px(&m, 21, 2), GREEN_); }
    md_destroy(&m);
}
TEST(gap10_tblt_does_not_modify_source) {
    NEED_BLT(); REQUIRE(TB);
    MD d, s;
    if (!md_create(&d, 16, 16, 32) || !mk_split(&s, 8, 8, 32, RED_, GREEN_)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_);
    TB(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, RED_);
    EXPECT_EQ(md_count(&s, 0, 0, 4, 8, RED_), 32);
    EXPECT_EQ(md_count(&s, 4, 0, 8, 8, GREEN_), 32);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_tblt_key_rounding_16bpp_observed) {
    NEED_BLT(); REQUIRE(TB);
    MD d, s;
    if (!md_create(&d, 8, 8, 16) || !md_create(&s, 8, 8, 16)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, RGB(255, 0, 0));
    BOOL r = TB(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, RGB(250, 3, 2));
    t_note("16bpp near-key (250,3,2) vs source red -> %d, dest changed %d px", r, md_diff(&d, WHITE_));
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_tblt_halftone_flags_dont_affect_keying) {
    NEED_BLT(); REQUIRE(TB); REQUIRE(U(SetStretchBltMode));
    int modes[] = { BLACKONWHITE, WHITEONBLACK, COLORONCOLOR, HALFTONE }; unsigned k;
    for (k = 0; k < 4; k++) {
        MD d, s;
        if (!md_create(&d, 16, 16, 32) || !mk_split(&s, 8, 8, 32, RED_, GREEN_)) continue;
        md_clear(&d, WHITE_);
        U(SetStretchBltMode)(d.dc, modes[k]);
        CHECK(TB(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, RED_));
        CHECKF(md_count(&d, 4, 0, 8, 8, GREEN_) == 32, "mode %d", modes[k]);
        CHECKF(md_count(&d, 0, 0, 4, 8, WHITE_) == 32, "mode %d", modes[k]);
        md_destroy(&d); md_destroy(&s);
    }
}
TEST(gap10_tblt_repeated_no_leak) {
    NEED_BLT(); REQUIRE(TB);
    MD d, s; int i;
    if (!md_create(&d, 16, 16, 32) || !md_create(&s, 16, 16, 32)) { t_skip("no DIB"); return; }
    DWORD before = g10_gdicount();
    for (i = 0; i < 300; i++) TB(d.dc, 0, 0, 16, 16, s.dc, 0, 0, 16, 16, RED_);
    EXPECT_EQ(g10_gdicount(), before);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_tblt_gdi32_and_gdi32full_agree) {
    NEED_BLT();
    REQUIRE(UG(GdiTransparentBlt));
    MD d1, d2, s;
    if (!md_create(&d1, 16, 16, 32) || !md_create(&d2, 16, 16, 32) || !mk_split(&s, 8, 8, 32, RED_, GREEN_)) { t_skip("no DIB"); return; }
    md_clear(&d1, WHITE_); md_clear(&d2, WHITE_);
    CHECK(UG(GdiTransparentBlt)(d1.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, RED_));
    if (UF(GdiTransparentBlt)) {
        CHECK(UF(GdiTransparentBlt)(d2.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, RED_));
        EXPECT_EQ(memcmp(d1.bits, d2.bits, 16 * 16 * 4), 0);
    }
    md_destroy(&d1); md_destroy(&d2); md_destroy(&s);
}
TEST(gap10_tblt_metafile_dest_observed) {
    NEED_BLT(); REQUIRE(TB); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    SKIP_WINE("EMF recording of GdiTransparentBlt");
    MD s; HDC e; HENHMETAFILE h;
    if (!md_create(&s, 8, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&s, GREEN_);
    e = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    CHECK(e != NULL);
    if (e) {
        BOOL r = TB(e, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, RED_);
        t_note("TransparentBlt into EMF DC -> %d", r);
        h = U(CloseEnhMetaFile)(e);
        CHECK(h != NULL);
        if (h) U(DeleteEnhMetaFile)(h);
    }
    md_destroy(&s);
}

/* ============================ GdiAlphaBlend ============================ */
#define AB U(GdiAlphaBlend)
static BLENDFUNCTION bf(BYTE op, BYTE flags, BYTE sca, BYTE fmt) {
    BLENDFUNCTION b; b.BlendOp = op; b.BlendFlags = flags; b.SourceConstantAlpha = sca; b.AlphaFormat = fmt; return b;
}
TEST(gap10_ablend_null_source_fails_silently) {
    NEED_BLT(); REQUIRE(AB);
    MD d;
    if (!md_create(&d, 16, 16, 32)) { t_skip("no DIB"); return; }
    CLR();
    CHECK(!AB(d.dc, 0, 0, 8, 8, NULL, 0, 0, 8, 8, bf(0, 0, 255, 0)));
    EXPECT_EQ(GetLastError(), ERRS);
    md_destroy(&d);
}
TEST(gap10_ablend_invalid_dest_sets_6) {
    NEED_BLT(); REQUIRE(AB);
    MD s; HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef }; unsigned i;
    if (!md_create(&s, 8, 8, 32)) { t_skip("no DIB"); return; }
    for (i = 0; i < 3; i++) {
        CLR();
        CHECKF(!AB(bad[i], 0, 0, 8, 8, s.dc, 0, 0, 8, 8, bf(0, 0, 255, 0)), "dest %p", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "dest %p err %lu", bad[i], GetLastError());
    }
    md_destroy(&s);
}
TEST(gap10_ablend_dest_wrong_type) {
    NEED_BLT(); REQUIRE(AB);
    MD s; HBRUSH b = U(CreateSolidBrush)(0);
    if (!md_create(&s, 8, 8, 32)) { t_skip("no DIB"); return; }
    CLR();
    CHECK(!AB((HDC)b, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, bf(0, 0, 255, 0)));
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    U(DeleteObject)(b); md_destroy(&s);
}
TEST(gap10_ablend_constant_alpha_255_copies) {
    NEED_BLT(); REQUIRE(AB);
    int bpps[] = { 32, 24, 16 }; unsigned k;
    for (k = 0; k < 3; k++) {
        MD d, s;
        if (!md_create(&d, 16, 16, bpps[k]) || !mk_split(&s, 8, 8, bpps[k], RED_, GREEN_)) continue;
        md_clear(&d, WHITE_);
        CHECK(AB(d.dc, 4, 4, 8, 8, s.dc, 0, 0, 8, 8, bf(AC_SRC_OVER, 0, 255, 0)));
        CHECKF(md_count(&d, 4, 4, 8, 12, RED_) == 32 && md_count(&d, 8, 4, 12, 12, GREEN_) == 32, "bpp %d", bpps[k]);
        CHECKF(md_diff(&d, WHITE_) == 64, "bpp %d changed %d", bpps[k], md_diff(&d, WHITE_));
        md_destroy(&d); md_destroy(&s);
    }
}
TEST(gap10_ablend_constant_alpha_0_leaves_dest) {
    NEED_BLT(); REQUIRE(AB);
    MD d, s;
    if (!md_create(&d, 16, 16, 32) || !md_create(&s, 8, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, 0x00445566); md_clear(&s, RED_);
    BOOL r = AB(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, bf(AC_SRC_OVER, 0, 0, 0));
    CHECK(r);
    EXPECT_EQ(md_diff(&d, 0x00445566), 0);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_ablend_constant_alpha_half_black_over_white) {
    NEED_BLT(); REQUIRE(AB);
    int bpps[] = { 32, 24 }; unsigned k;
    for (k = 0; k < 2; k++) {
        MD d, s;
        if (!md_create(&d, 16, 16, bpps[k]) || !md_create(&s, 8, 8, bpps[k])) continue;
        md_clear(&d, WHITE_); md_clear(&s, BLACK_);
        CHECK(AB(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, bf(AC_SRC_OVER, 0, 128, 0)));
        COLORREF c = md_px(&d, 3, 3);
        CHECKF(near_c(c, RGB(127, 127, 127), 2), "bpp %d got %06lx", bpps[k], c);
        CHECKF(md_px(&d, 9, 9) == WHITE_, "bpp %d outside", bpps[k]);
        md_destroy(&d); md_destroy(&s);
    }
}
TEST(gap10_ablend_constant_alpha_table) {
    NEED_BLT(); REQUIRE(AB);
    int al[] = { 1, 32, 64, 100, 127, 128, 200, 254 }; unsigned k;
    for (k = 0; k < sizeof al / sizeof *al; k++) {
        MD d, s;
        if (!md_create(&d, 8, 8, 32) || !md_create(&s, 8, 8, 32)) continue;
        md_clear(&d, RGB(200, 100, 0)); md_clear(&s, RGB(0, 100, 250));
        CHECK(AB(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, bf(AC_SRC_OVER, 0, (BYTE)al[k], 0)));
        COLORREF want = RGB((200 * (255 - al[k])) / 255, 100, (250 * al[k]) / 255), got = md_px(&d, 4, 4);
        CHECKF(near_c(got, want, 2), "alpha %d got %06lx want %06lx", al[k], got, want);
        md_destroy(&d); md_destroy(&s);
    }
}
TEST(gap10_ablend_per_pixel_alpha_premultiplied) {
    NEED_BLT(); REQUIRE(AB);
    MD d, s; DWORD *px;
    if (!md_create(&d, 8, 8, 32) || !md_create(&s, 4, 4, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_);
    px = (DWORD *)s.bits;
    px[0] = 0xFF112233;                 /* opaque */
    px[1] = 0x00000000;                 /* transparent */
    px[2] = 0x80000080;                 /* 50% blue, premultiplied */
    px[3] = 0x40404000 | 0x40000000;    /* a=0x40, premult (0x40,0x40,0x00) */
    px[3] = 0x40404000;
    px[3] = (0x40u << 24) | (0x40u << 16) | (0x40u << 8) | 0x00u;
    { int i; for (i = 4; i < 16; i++) px[i] = 0; }
    CHECK(AB(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, bf(AC_SRC_OVER, 0, 255, AC_SRC_ALPHA)));
    EXPECT_EQ(md_px(&d, 0, 0), RGB(0x11, 0x22, 0x33));
    EXPECT_EQ(md_px(&d, 1, 0), WHITE_);
    COLORREF c2 = md_px(&d, 2, 0);
    CHECKF(near_c(c2, RGB(127, 127, 255), 2), "50%% blue over white %06lx", c2);
    COLORREF c3 = md_px(&d, 3, 0);
    CHECKF(near_c(c3, RGB(0x40 + (255 * 0xBF) / 255, 0x40 + (255 * 0xBF) / 255, (255 * 0xBF) / 255), 2) || near_c(c3, RGB(255, 255, 191), 3), "px3 %06lx", c3);
    EXPECT_EQ(md_px(&d, 5, 5), WHITE_);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_ablend_per_pixel_alpha_with_constant) {
    NEED_BLT(); REQUIRE(AB);
    MD d, s; DWORD *px; int i;
    if (!md_create(&d, 8, 8, 32) || !md_create(&s, 4, 4, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, BLACK_);
    px = (DWORD *)s.bits;
    for (i = 0; i < 16; i++) px[i] = 0xFFFFFFFF;    /* opaque white */
    CHECK(AB(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 4, 4, bf(AC_SRC_OVER, 0, 128, AC_SRC_ALPHA)));
    COLORREF c = md_px(&d, 1, 1);
    CHECKF(near_c(c, RGB(128, 128, 128), 2), "opaque white at SCA 128 over black: %06lx", c);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_ablend_src_alpha_needs_32bpp) {
    NEED_BLT(); REQUIRE(AB);
    MD d, s;
    if (!md_create(&d, 8, 8, 32) || !md_create(&s, 8, 8, 24)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, RED_);
    CLR();
    BOOL r = AB(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, bf(AC_SRC_OVER, 0, 255, AC_SRC_ALPHA));
    CHECK(!r);
    t_note("AC_SRC_ALPHA with 24bpp source: err %lu", GetLastError());
    EXPECT_EQ(md_diff(&d, WHITE_), 0);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_ablend_invalid_blend_op) {
    NEED_BLT(); REQUIRE(AB);
    MD d, s; int ops[] = { 1, 2, 0x7F, 0xFF }; unsigned i;
    if (!md_create(&d, 8, 8, 32) || !md_create(&s, 8, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, RED_);
    for (i = 0; i < 4; i++) {
        CLR();
        CHECKF(!AB(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, bf((BYTE)ops[i], 0, 255, 0)), "op %d", ops[i]);
    }
    EXPECT_EQ(md_diff(&d, WHITE_), 0);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_ablend_invalid_blend_flags) {
    NEED_BLT(); REQUIRE(AB);
    MD d, s; int fl[] = { 1, 2, 0x80, 0xFF }; unsigned i;
    if (!md_create(&d, 8, 8, 32) || !md_create(&s, 8, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, RED_);
    for (i = 0; i < 4; i++) {
        CLR();
        CHECKF(!AB(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, bf(0, (BYTE)fl[i], 255, 0)), "flags %d", fl[i]);
    }
    EXPECT_EQ(md_diff(&d, WHITE_), 0);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_ablend_invalid_alpha_format_observed) {
    NEED_BLT(); REQUIRE(AB);
    MD d, s; int af[] = { 2, 3, 0x80, 0xFF }; unsigned i;
    if (!md_create(&d, 8, 8, 32) || !md_create(&s, 8, 8, 32)) { t_skip("no DIB"); return; }
    for (i = 0; i < 4; i++) {
        md_clear(&d, WHITE_); md_clear(&s, RED_);
        CLR();
        BOOL r = AB(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, bf(0, 0, 255, (BYTE)af[i]));
        t_note("AlphaFormat %d -> %d err %lu", af[i], r, GetLastError());
    }
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_ablend_nonpositive_sizes_observed) {
    NEED_BLT(); REQUIRE(AB);
    MD d, s; int bad[][4] = { {0,8,8,8}, {8,0,8,8}, {8,8,0,8}, {8,8,8,0}, {-8,8,8,8}, {8,-8,8,8}, {8,8,-8,8}, {8,8,8,-8} }; unsigned i;
    if (!md_create(&d, 16, 16, 32) || !md_create(&s, 8, 8, 32)) { t_skip("no DIB"); return; }
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        md_clear(&d, WHITE_); md_clear(&s, RED_);
        CLR();
        BOOL r = AB(d.dc, 0, 0, bad[i][0], bad[i][1], s.dc, 0, 0, bad[i][2], bad[i][3], bf(0, 0, 255, 0));
        t_note("sizes %d,%d,%d,%d -> %d err %lu changed %d", bad[i][0], bad[i][1], bad[i][2], bad[i][3], r, GetLastError(), md_diff(&d, WHITE_));
    }
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_ablend_stretch_solid_fills_dest_rect) {
    NEED_BLT(); REQUIRE(AB);
    MD d, s;
    if (!md_create(&d, 32, 32, 32) || !md_create(&s, 4, 4, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, GREEN_);
    CHECK(AB(d.dc, 4, 4, 20, 12, s.dc, 0, 0, 4, 4, bf(AC_SRC_OVER, 0, 255, 0)));
    EXPECT_EQ(md_count(&d, 4, 4, 24, 16, GREEN_), 20 * 12);
    EXPECT_EQ(md_diff(&d, WHITE_), 20 * 12);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_ablend_source_offset_and_clip) {
    NEED_BLT(); REQUIRE(AB); REQUIRE(U(IntersectClipRect));
    MD d, s;
    if (!md_create(&d, 16, 16, 32) || !mk_split(&s, 16, 8, 32, RED_, GREEN_)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_);
    CHECK(AB(d.dc, 0, 0, 8, 8, s.dc, 8, 0, 8, 8, bf(0, 0, 255, 0)));
    EXPECT_EQ(md_count(&d, 0, 0, 8, 8, GREEN_), 64);
    md_clear(&d, WHITE_);
    U(IntersectClipRect)(d.dc, 0, 0, 4, 4);
    CHECK(AB(d.dc, 0, 0, 8, 8, s.dc, 8, 0, 8, 8, bf(0, 0, 255, 0)));
    EXPECT_EQ(md_diff(&d, WHITE_), 16);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_ablend_does_not_modify_source) {
    NEED_BLT(); REQUIRE(AB);
    MD d, s;
    if (!md_create(&d, 8, 8, 32) || !md_create(&s, 8, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, RED_);
    AB(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, bf(0, 0, 100, 0));
    EXPECT_EQ(md_diff(&s, RED_), 0);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_ablend_mixed_depths_observed) {
    NEED_BLT(); REQUIRE(AB);
    int sb[] = { 32, 24, 16 }, db[] = { 16, 32, 24 }; unsigned k;
    for (k = 0; k < 3; k++) {
        MD d, s;
        if (!md_create(&d, 8, 8, db[k]) || !md_create(&s, 8, 8, sb[k])) continue;
        md_clear(&d, WHITE_); md_clear(&s, BLACK_);
        BOOL r = AB(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, bf(0, 0, 128, 0));
        COLORREF c = md_px(&d, 2, 2);
        t_note("src %dbpp -> dst %dbpp: ret %d px %06lx", sb[k], db[k], r, c);
        if (r) CHECKF(near_c(c, RGB(127, 127, 127), 8), "src %d dst %d px %06lx", sb[k], db[k], c);
        md_destroy(&d); md_destroy(&s);
    }
}
TEST(gap10_ablend_repeated_blend_converges) {
    NEED_BLT(); REQUIRE(AB);
    MD d, s; int i;
    if (!md_create(&d, 8, 8, 32) || !md_create(&s, 8, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, BLACK_);
    for (i = 0; i < 10; i++) AB(d.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, bf(0, 0, 128, 0));
    COLORREF c = md_px(&d, 1, 1);
    CHECKF(GetRValue(c) <= 2, "10x 50%% black over white -> %06lx", c);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_ablend_gdi32_and_full_agree) {
    NEED_BLT(); REQUIRE(UG(GdiAlphaBlend));
    MD d1, d2, s;
    if (!md_create(&d1, 8, 8, 32) || !md_create(&d2, 8, 8, 32) || !md_create(&s, 8, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&d1, WHITE_); md_clear(&d2, WHITE_); md_clear(&s, BLUE_);
    CHECK(UG(GdiAlphaBlend)(d1.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, bf(0, 0, 77, 0)));
    if (UF(GdiAlphaBlend)) {
        CHECK(UF(GdiAlphaBlend)(d2.dc, 0, 0, 8, 8, s.dc, 0, 0, 8, 8, bf(0, 0, 77, 0)));
        EXPECT_EQ(memcmp(d1.bits, d2.bits, 8 * 8 * 4), 0);
    }
    md_destroy(&d1); md_destroy(&d2); md_destroy(&s);
}

/* ============================ GdiGradientFill ============================ */
#define GF U(GdiGradientFill)
static TRIVERTEX tv(int x, int y, int r, int g, int b) {
    TRIVERTEX v; v.x = x; v.y = y; v.Red = (COLOR16)(r << 8); v.Green = (COLOR16)(g << 8); v.Blue = (COLOR16)(b << 8); v.Alpha = 0; return v;
}
TEST(gap10_grad_invalid_dc_sets_87) {
    NEED_BLT(); REQUIRE(GF);
    TRIVERTEX v[2] = { tv(0, 0, 0, 0, 0), tv(4, 4, 255, 255, 255) }; GRADIENT_RECT gr = { 0, 1 };
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef, (HDC)(ULONG_PTR)0xFFFFFFFF }; unsigned i;
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        CLR();
        CHECKF(!GF(bad[i], v, 2, &gr, 1, GRADIENT_FILL_RECT_H), "dc %p", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "dc %p err %lu", bad[i], GetLastError());
    }
}
TEST(gap10_grad_wrong_object_type) {
    NEED_BLT(); REQUIRE(GF);
    TRIVERTEX v[2] = { tv(0, 0, 0, 0, 0), tv(4, 4, 255, 255, 255) }; GRADIENT_RECT gr = { 0, 1 }; HBRUSH b = U(CreateSolidBrush)(0);
    CLR();
    CHECK(!GF((HDC)b, v, 2, &gr, 1, GRADIENT_FILL_RECT_H));
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    U(DeleteObject)(b);
}
TEST(gap10_grad_deleted_dc) {
    NEED_BLT(); REQUIRE(GF);
    TRIVERTEX v[2] = { tv(0, 0, 0, 0, 0), tv(4, 4, 255, 255, 255) }; GRADIENT_RECT gr = { 0, 1 };
    HDC dc = U(CreateCompatibleDC)(NULL);
    U(DeleteDC)(dc);
    CLR();
    CHECK(!GF(dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H));
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
}
TEST(gap10_grad_invalid_mode) {
    NEED_BLT(); REQUIRE(GF);
    MD m; TRIVERTEX v[3] = { tv(0, 0, 255, 0, 0), tv(8, 8, 0, 255, 0), tv(0, 8, 0, 0, 255) }; GRADIENT_RECT gr = { 0, 1 };
    ULONG modes[] = { 3, 4, 0xFF, 0x100, 0xFFFFFFFF }; unsigned i;
    if (!md_create(&m, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    for (i = 0; i < sizeof modes / sizeof *modes; i++) {
        CLR();
        CHECKF(!GF(m.dc, v, 3, &gr, 1, modes[i]), "mode %lu", modes[i]);
    }
    EXPECT_EQ(md_diff(&m, WHITE_), 0);
    md_destroy(&m);
}
TEST(gap10_grad_null_pointers) {
    NEED_BLT(); REQUIRE(GF);
    MD m; TRIVERTEX v[2] = { tv(0, 0, 0, 0, 0), tv(8, 8, 255, 255, 255) }; GRADIENT_RECT gr = { 0, 1 };
    if (!md_create(&m, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    CLR(); CHECK(!GF(m.dc, NULL, 2, &gr, 1, GRADIENT_FILL_RECT_H));
    CLR(); CHECK(!GF(m.dc, v, 2, NULL, 1, GRADIENT_FILL_RECT_H));
    CLR(); CHECK(!GF(m.dc, NULL, 0, NULL, 0, GRADIENT_FILL_RECT_H));
    EXPECT_EQ(md_diff(&m, WHITE_), 0);
    md_destroy(&m);
}
TEST(gap10_grad_counts_and_indices_validated) {
    NEED_BLT(); REQUIRE(GF);
    MD m; TRIVERTEX v[3] = { tv(0, 0, 0, 0, 0), tv(8, 8, 255, 255, 255), tv(0, 8, 9, 9, 9) };
    GRADIENT_RECT bad1 = { 0, 2 }, bad2 = { 5, 1 }, bad3 = { 0xFFFFFFFF, 1 }, ok = { 0, 1 };
    GRADIENT_TRIANGLE badt = { 0, 1, 3 };
    if (!md_create(&m, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    CLR(); CHECK(!GF(m.dc, v, 2, &bad1, 1, GRADIENT_FILL_RECT_H));
    CLR(); CHECK(!GF(m.dc, v, 3, &bad2, 1, GRADIENT_FILL_RECT_H));
    CLR(); CHECK(!GF(m.dc, v, 3, &bad3, 1, GRADIENT_FILL_RECT_H));
    CLR(); CHECK(!GF(m.dc, v, 3, &badt, 1, GRADIENT_FILL_TRIANGLE));
    CLR(); CHECK(!GF(m.dc, v, 0, &ok, 1, GRADIENT_FILL_RECT_H));
    EXPECT_EQ(md_diff(&m, WHITE_), 0);
    md_destroy(&m);
}
TEST(gap10_grad_zero_count_observed) {
    NEED_BLT(); REQUIRE(GF);
    MD m; TRIVERTEX v[2] = { tv(0, 0, 0, 0, 0), tv(8, 8, 255, 255, 255) }; GRADIENT_RECT gr = { 0, 1 };
    if (!md_create(&m, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    CLR();
    BOOL r = GF(m.dc, v, 2, &gr, 0, GRADIENT_FILL_RECT_H);
    t_note("nCount 0 -> %d err %lu", r, GetLastError());
    EXPECT_EQ(md_diff(&m, WHITE_), 0);
    md_destroy(&m);
}
TEST(gap10_grad_rect_solid_all_depths) {
    NEED_BLT(); REQUIRE(GF);
    int bpps[] = { 32, 24, 16 }; unsigned k;
    for (k = 0; k < 3; k++) {
        MD m; TRIVERTEX v[2] = { tv(4, 4, 255, 0, 0), tv(20, 12, 255, 0, 0) }; GRADIENT_RECT gr = { 0, 1 };
        if (!md_create(&m, 32, 16, bpps[k])) continue;
        md_clear(&m, WHITE_);
        CHECK(GF(m.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H));
        CHECKF(md_count(&m, 4, 4, 20, 12, RED_) == 16 * 8, "bpp %d filled %d", bpps[k], md_count(&m, 4, 4, 20, 12, RED_));
        CHECKF(md_diff(&m, WHITE_) == 16 * 8, "bpp %d changed %d", bpps[k], md_diff(&m, WHITE_));
        md_destroy(&m);
    }
}
TEST(gap10_grad_rect_vertical_solid) {
    NEED_BLT(); REQUIRE(GF);
    MD m; TRIVERTEX v[2] = { tv(2, 2, 0, 255, 0), tv(10, 14, 0, 255, 0) }; GRADIENT_RECT gr = { 0, 1 };
    if (!md_create(&m, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    CHECK(GF(m.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_V));
    EXPECT_EQ(md_count(&m, 2, 2, 10, 14, GREEN_), 8 * 12);
    EXPECT_EQ(md_diff(&m, WHITE_), 8 * 12);
    md_destroy(&m);
}
TEST(gap10_grad_rect_h_monotonic) {
    NEED_BLT(); REQUIRE(GF);
    MD m; TRIVERTEX v[2] = { tv(0, 0, 0, 0, 0), tv(32, 4, 255, 255, 255) }; GRADIENT_RECT gr = { 0, 1 };
    int x, y, prev = -1;
    if (!md_create(&m, 32, 4, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, BLUE_);
    CHECK(GF(m.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H));
    for (x = 0; x < 32; x++) {
        COLORREF c = md_px(&m, x, 0);
        CHECKF(GetRValue(c) == GetGValue(c) && GetGValue(c) == GetBValue(c), "x=%d not grey %06lx", x, c);
        CHECKF((int)GetRValue(c) >= prev, "x=%d not monotonic (%d < %d)", x, GetRValue(c), prev);
        prev = GetRValue(c);
        for (y = 1; y < 4; y++) CHECKF(md_px(&m, x, y) == c, "x=%d row %d differs", x, y);
    }
    CHECKF(GetRValue(md_px(&m, 0, 0)) <= 16, "left end %d", GetRValue(md_px(&m, 0, 0)));
    CHECKF(GetRValue(md_px(&m, 31, 0)) >= 239, "right end %d", GetRValue(md_px(&m, 31, 0)));
    CHECKF(GetRValue(md_px(&m, 16, 0)) > 100 && GetRValue(md_px(&m, 16, 0)) < 156, "middle %d", GetRValue(md_px(&m, 16, 0)));
    md_destroy(&m);
}
TEST(gap10_grad_rect_v_monotonic) {
    NEED_BLT(); REQUIRE(GF);
    MD m; TRIVERTEX v[2] = { tv(0, 0, 255, 0, 0), tv(4, 32, 0, 0, 255) }; GRADIENT_RECT gr = { 0, 1 };
    int x, y, prev = 256;
    if (!md_create(&m, 4, 32, 32)) { t_skip("no DIB"); return; }
    CHECK(GF(m.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_V));
    for (y = 0; y < 32; y++) {
        COLORREF c = md_px(&m, 0, y);
        CHECKF((int)GetRValue(c) <= prev, "y=%d red not decreasing", y);
        prev = GetRValue(c);
        CHECKF(GetGValue(c) == 0, "y=%d green %d", y, GetGValue(c));
        for (x = 1; x < 4; x++) CHECKF(md_px(&m, x, y) == c, "y=%d col %d differs", y, x);
    }
    CHECKF(GetRValue(md_px(&m, 0, 0)) >= 239 && GetBValue(md_px(&m, 0, 0)) <= 16, "top %06lx", md_px(&m, 0, 0));
    CHECKF(GetBValue(md_px(&m, 0, 31)) >= 239 && GetRValue(md_px(&m, 0, 31)) <= 16, "bottom %06lx", md_px(&m, 0, 31));
    md_destroy(&m);
}
TEST(gap10_grad_rect_lowerright_exclusive_observed) {
    NEED_BLT(); REQUIRE(GF);
    MD m; TRIVERTEX v[2] = { tv(4, 4, 255, 0, 0), tv(12, 12, 255, 0, 0) }; GRADIENT_RECT gr = { 0, 1 };
    if (!md_create(&m, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    GF(m.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H);
    EXPECT_EQ(md_px(&m, 4, 4), RED_);
    EXPECT_EQ(md_px(&m, 11, 11), RED_);
    t_note("pixel at lower-right vertex (12,12): %06lx; (12,11): %06lx", md_px(&m, 12, 12), md_px(&m, 12, 11));
    EXPECT_EQ(md_px(&m, 3, 4), WHITE_);
    EXPECT_EQ(md_px(&m, 4, 3), WHITE_);
    md_destroy(&m);
}
TEST(gap10_grad_rect_vertex_order_irrelevant) {
    NEED_BLT(); REQUIRE(GF);
    MD a, b; TRIVERTEX v1[2] = { tv(2, 2, 0, 0, 255), tv(14, 10, 0, 0, 255) }, v2[2] = { tv(14, 10, 0, 0, 255), tv(2, 2, 0, 0, 255) };
    GRADIENT_RECT gr = { 0, 1 };
    if (!md_create(&a, 16, 16, 32) || !md_create(&b, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&a, WHITE_); md_clear(&b, WHITE_);
    CHECK(GF(a.dc, v1, 2, &gr, 1, GRADIENT_FILL_RECT_H));
    BOOL r = GF(b.dc, v2, 2, &gr, 1, GRADIENT_FILL_RECT_H);
    t_note("swapped corners -> %d, equal output %d", r, !memcmp(a.bits, b.bits, 16 * 16 * 4));
    md_destroy(&a); md_destroy(&b);
}
TEST(gap10_grad_two_rects_in_one_call) {
    NEED_BLT(); REQUIRE(GF);
    MD m; TRIVERTEX v[4] = { tv(0, 0, 255, 0, 0), tv(8, 8, 255, 0, 0), tv(16, 0, 0, 255, 0), tv(24, 8, 0, 255, 0) };
    GRADIENT_RECT gr[2] = { { 0, 1 }, { 2, 3 } };
    if (!md_create(&m, 32, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    CHECK(GF(m.dc, v, 4, gr, 2, GRADIENT_FILL_RECT_H));
    EXPECT_EQ(md_count(&m, 0, 0, 8, 8, RED_), 64);
    EXPECT_EQ(md_count(&m, 16, 0, 24, 8, GREEN_), 64);
    EXPECT_EQ(md_diff(&m, WHITE_), 128);
    md_destroy(&m);
}
TEST(gap10_grad_triangle_solid) {
    NEED_BLT(); REQUIRE(GF);
    MD m; TRIVERTEX v[3] = { tv(2, 2, 0, 255, 0), tv(30, 2, 0, 255, 0), tv(2, 30, 0, 255, 0) }; GRADIENT_TRIANGLE gt = { 0, 1, 2 };
    if (!md_create(&m, 32, 32, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    CHECK(GF(m.dc, v, 3, &gt, 1, GRADIENT_FILL_TRIANGLE));
    EXPECT_EQ(md_px(&m, 6, 6), GREEN_);
    EXPECT_EQ(md_px(&m, 14, 10), GREEN_);
    EXPECT_EQ(md_px(&m, 4, 24), GREEN_);
    EXPECT_EQ(md_px(&m, 26, 26), WHITE_);
    EXPECT_EQ(md_px(&m, 28, 20), WHITE_);
    EXPECT_EQ(md_px(&m, 0, 0), WHITE_);
    int n = md_count(&m, 0, 0, 32, 32, GREEN_);
    CHECKF(n > 340 && n < 420, "triangle area %d (ideal 392)", n);
    md_destroy(&m);
}
TEST(gap10_grad_triangle_vertex_dominance) {
    NEED_BLT(); REQUIRE(GF);
    MD m; TRIVERTEX v[3] = { tv(2, 2, 255, 0, 0), tv(60, 2, 0, 255, 0), tv(2, 60, 0, 0, 255) }; GRADIENT_TRIANGLE gt = { 0, 1, 2 };
    if (!md_create(&m, 64, 64, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    CHECK(GF(m.dc, v, 3, &gt, 1, GRADIENT_FILL_TRIANGLE));
    COLORREF a = md_px(&m, 5, 4), b = md_px(&m, 54, 4), c = md_px(&m, 4, 54);
    CHECKF(GetRValue(a) > 200 && GetGValue(a) < 60 && GetBValue(a) < 60, "near red vertex %06lx", a);
    CHECKF(GetGValue(b) > 200 && GetRValue(b) < 60 && GetBValue(b) < 60, "near green vertex %06lx", b);
    CHECKF(GetBValue(c) > 200 && GetRValue(c) < 60 && GetGValue(c) < 60, "near blue vertex %06lx", c);
    md_destroy(&m);
}
TEST(gap10_grad_triangle_winding_independent) {
    NEED_BLT(); REQUIRE(GF);
    MD a, b; TRIVERTEX v[3] = { tv(2, 2, 255, 0, 0), tv(28, 2, 255, 0, 0), tv(2, 28, 255, 0, 0) };
    GRADIENT_TRIANGLE t1 = { 0, 1, 2 }, t2 = { 0, 2, 1 };
    if (!md_create(&a, 32, 32, 32) || !md_create(&b, 32, 32, 32)) { t_skip("no DIB"); return; }
    md_clear(&a, WHITE_); md_clear(&b, WHITE_);
    CHECK(GF(a.dc, v, 3, &t1, 1, GRADIENT_FILL_TRIANGLE));
    CHECK(GF(b.dc, v, 3, &t2, 1, GRADIENT_FILL_TRIANGLE));
    EXPECT_EQ(memcmp(a.bits, b.bits, 32 * 32 * 4), 0);
    md_destroy(&a); md_destroy(&b);
}
TEST(gap10_grad_triangle_degenerate_observed) {
    NEED_BLT(); REQUIRE(GF);
    MD m; TRIVERTEX v[3] = { tv(2, 2, 255, 0, 0), tv(10, 10, 255, 0, 0), tv(18, 18, 255, 0, 0) }; GRADIENT_TRIANGLE gt = { 0, 1, 2 };
    if (!md_create(&m, 32, 32, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    CLR();
    BOOL r = GF(m.dc, v, 3, &gt, 1, GRADIENT_FILL_TRIANGLE);
    t_note("collinear triangle -> %d, changed %d px", r, md_diff(&m, WHITE_));
    md_destroy(&m);
}
TEST(gap10_grad_alpha_field_ignored) {
    NEED_BLT(); REQUIRE(GF);
    MD m; TRIVERTEX v[2] = { tv(0, 0, 255, 0, 0), tv(8, 8, 255, 0, 0) }; GRADIENT_RECT gr = { 0, 1 };
    if (!md_create(&m, 8, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    v[0].Alpha = 0; v[1].Alpha = 0;
    CHECK(GF(m.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H));
    EXPECT_EQ(md_count(&m, 0, 0, 8, 8, RED_), 64);
    md_destroy(&m);
}
TEST(gap10_grad_color16_low_byte_ignored) {
    NEED_BLT(); REQUIRE(GF);
    MD m; TRIVERTEX v[2] = { tv(0, 0, 0, 0, 0), tv(8, 8, 0, 0, 0) }; GRADIENT_RECT gr = { 0, 1 };
    if (!md_create(&m, 8, 8, 32)) { t_skip("no DIB"); return; }
    v[0].Red = v[1].Red = 0x80FF;
    v[0].Green = v[1].Green = 0xFFFF;
    v[0].Blue = v[1].Blue = 0x7F00;
    CHECK(GF(m.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H));
    COLORREF c = md_px(&m, 3, 3);
    t_note("COLOR16 0x80FF/0xFFFF/0x7F00 -> %06lx", c);
    CHECK(near_c(c, RGB(0x80, 0xFF, 0x7F), 1));
    md_destroy(&m);
}
TEST(gap10_grad_respects_clip) {
    NEED_BLT(); REQUIRE(GF); REQUIRE(U(IntersectClipRect));
    MD m; TRIVERTEX v[2] = { tv(0, 0, 255, 0, 0), tv(16, 16, 255, 0, 0) }; GRADIENT_RECT gr = { 0, 1 };
    if (!md_create(&m, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    U(IntersectClipRect)(m.dc, 4, 4, 8, 8);
    CHECK(GF(m.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H));
    EXPECT_EQ(md_count(&m, 0, 0, 16, 16, RED_), 16);
    EXPECT_EQ(md_px(&m, 4, 4), RED_); EXPECT_EQ(md_px(&m, 7, 7), RED_); EXPECT_EQ(md_px(&m, 8, 8), WHITE_);
    md_destroy(&m);
}
TEST(gap10_grad_logical_coordinates_use_transform) {
    NEED_BLT(); REQUIRE(GF); REQUIRE(U(SetViewportOrgEx));
    MD m; TRIVERTEX v[2] = { tv(0, 0, 255, 0, 0), tv(8, 8, 255, 0, 0) }; GRADIENT_RECT gr = { 0, 1 };
    if (!md_create(&m, 32, 32, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    U(SetViewportOrgEx)(m.dc, 10, 10, NULL);
    CHECK(GF(m.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H));
    t_note("viewport-org 10,10: red at origin-shifted rect %d, red at unshifted rect %d",
           md_count(&m, 10, 10, 18, 18, RED_), md_count(&m, 0, 0, 8, 8, RED_));
    CHECK(md_count(&m, 10, 10, 18, 18, RED_) == 64 || md_count(&m, 0, 0, 8, 8, RED_) == 64);
    md_destroy(&m);
}
TEST(gap10_grad_negative_and_offscreen_rect) {
    NEED_BLT(); REQUIRE(GF);
    MD m; TRIVERTEX v[2] = { tv(-8, -8, 255, 0, 0), tv(4, 4, 255, 0, 0) }; GRADIENT_RECT gr = { 0, 1 };
    if (!md_create(&m, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    CLR();
    BOOL r = GF(m.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H);
    t_note("rect partly outside -> %d (red px %d)", r, md_count(&m, 0, 0, 16, 16, RED_));
    if (r) EXPECT_EQ(md_count(&m, 0, 0, 16, 16, RED_), 16);
    v[0] = tv(100, 100, 255, 0, 0); v[1] = tv(120, 120, 255, 0, 0);
    r = GF(m.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H);
    t_note("rect fully outside -> %d", r);
    md_destroy(&m);
}
TEST(gap10_grad_gdi32_and_full_agree) {
    NEED_BLT(); REQUIRE(UG(GdiGradientFill));
    MD a, b; TRIVERTEX v[2] = { tv(0, 0, 10, 20, 30), tv(16, 8, 200, 100, 50) }; GRADIENT_RECT gr = { 0, 1 };
    if (!md_create(&a, 16, 8, 32) || !md_create(&b, 16, 8, 32)) { t_skip("no DIB"); return; }
    CHECK(UG(GdiGradientFill)(a.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H));
    if (UF(GdiGradientFill)) {
        CHECK(UF(GdiGradientFill)(b.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H));
        EXPECT_EQ(memcmp(a.bits, b.bits, 16 * 8 * 4), 0);
    }
    md_destroy(&a); md_destroy(&b);
}
TEST(gap10_grad_deterministic_repeat) {
    NEED_BLT(); REQUIRE(GF);
    MD a, b; TRIVERTEX v[2] = { tv(0, 0, 0, 128, 255), tv(32, 16, 255, 64, 0) }; GRADIENT_RECT gr = { 0, 1 };
    if (!md_create(&a, 32, 16, 32) || !md_create(&b, 32, 16, 32)) { t_skip("no DIB"); return; }
    CHECK(GF(a.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H));
    CHECK(GF(b.dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H));
    EXPECT_EQ(memcmp(a.bits, b.bits, 32 * 16 * 4), 0);
    md_destroy(&a); md_destroy(&b);
}

/* ============================ MaskBlt ============================ */
#define MB U(MaskBlt)
static HBITMAP mask_cols(int w, int h, int ones) {   /* mono DDB: first `ones` columns set (w <= 16) */
    BYTE bits[64]; int y; HBITMAP bm;
    memset(bits, 0, sizeof bits);
    for (y = 0; y < h; y++) {
        WORD row = 0; int x;
        for (x = 0; x < ones; x++) row |= (WORD)(0x8000 >> x);
        bits[y * 2] = (BYTE)(row >> 8); bits[y * 2 + 1] = (BYTE)(row & 0xFF);
    }
    bm = U(CreateBitmap)(w, h, 1, 1, bits);
    return bm;
}
#define NEED_MB() do { NEED_BLT(); REQUIRE(MB); REQUIRE(U(CreateBitmap)); } while (0)
TEST(gap10_maskblt_null_source_fails_silently) {
    NEED_MB();
    MD d;
    if (!md_create(&d, 16, 16, 32)) { t_skip("no DIB"); return; }
    CLR();
    CHECK(!MB(d.dc, 0, 0, 8, 8, NULL, 0, 0, NULL, 0, 0, SRCCOPY));
    EXPECT_EQ(GetLastError(), ERRS);
    md_destroy(&d);
}
TEST(gap10_maskblt_invalid_dest_sets_6) {
    NEED_MB();
    MD s; HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef }; unsigned i;
    if (!md_create(&s, 16, 16, 32)) { t_skip("no DIB"); return; }
    for (i = 0; i < 3; i++) {
        CLR();
        CHECKF(!MB(bad[i], 0, 0, 8, 8, s.dc, 0, 0, NULL, 0, 0, SRCCOPY), "dest %p", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "dest %p err %lu", bad[i], GetLastError());
    }
    md_destroy(&s);
}
TEST(gap10_maskblt_dest_wrong_type) {
    NEED_MB();
    MD s; HBRUSH b = U(CreateSolidBrush)(0);
    if (!md_create(&s, 16, 16, 32)) { t_skip("no DIB"); return; }
    CLR();
    CHECK(!MB((HDC)b, 0, 0, 8, 8, s.dc, 0, 0, NULL, 0, 0, SRCCOPY));
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    U(DeleteObject)(b); md_destroy(&s);
}
TEST(gap10_maskblt_no_mask_is_bitblt) {
    NEED_MB();
    int bpps[] = { 32, 24, 16 }; unsigned k;
    for (k = 0; k < 3; k++) {
        MD d, s;
        if (!md_create(&d, 16, 16, bpps[k]) || !mk_split(&s, 8, 8, bpps[k], RED_, GREEN_)) continue;
        md_clear(&d, WHITE_);
        CHECK(MB(d.dc, 4, 4, 8, 8, s.dc, 0, 0, NULL, 0, 0, SRCCOPY));
        CHECKF(md_count(&d, 4, 4, 8, 12, RED_) == 32 && md_count(&d, 8, 4, 12, 12, GREEN_) == 32, "bpp %d", bpps[k]);
        md_destroy(&d); md_destroy(&s);
    }
}
TEST(gap10_maskblt_no_mask_uses_foreground_rop_only) {
    NEED_MB();
    MD d, s;
    if (!md_create(&d, 16, 16, 32) || !md_create(&s, 8, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, RED_);
    CHECK(MB(d.dc, 0, 0, 8, 8, s.dc, 0, 0, NULL, 0, 0, MAKEROP4(SRCCOPY, DSTINVERT)));
    EXPECT_EQ(md_count(&d, 0, 0, 8, 8, RED_), 64);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_maskblt_mask_selects_foreground_columns) {
    NEED_MB();
    MD d, s; HBITMAP mk;
    if (!md_create(&d, 16, 8, 32) || !md_create(&s, 16, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, BLUE_);
    mk = mask_cols(16, 8, 8);
    CHECK(mk != NULL);
    CHECK(MB(d.dc, 0, 0, 16, 8, s.dc, 0, 0, mk, 0, 0, MAKEROP4(SRCCOPY, 0x00AA0029)));   /* back = leave destination */
    EXPECT_EQ(md_count(&d, 0, 0, 8, 8, BLUE_), 64);
    EXPECT_EQ(md_count(&d, 8, 0, 16, 8, WHITE_), 64);
    U(DeleteObject)(mk);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_maskblt_background_rop_applied) {
    NEED_MB();
    MD d, s; HBITMAP mk;
    if (!md_create(&d, 16, 8, 32) || !md_create(&s, 16, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, BLUE_);
    mk = mask_cols(16, 8, 4);
    CHECK(MB(d.dc, 0, 0, 16, 8, s.dc, 0, 0, mk, 0, 0, MAKEROP4(SRCCOPY, DSTINVERT)));
    EXPECT_EQ(md_count(&d, 0, 0, 4, 8, BLUE_), 32);
    EXPECT_EQ(md_count(&d, 4, 0, 16, 8, BLACK_), 12 * 8);
    U(DeleteObject)(mk);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_maskblt_background_blackness_whiteness) {
    NEED_MB();
    DWORD backs[] = { BLACKNESS, WHITENESS }; COLORREF want[] = { BLACK_, WHITE_ }; unsigned k;
    for (k = 0; k < 2; k++) {
        MD d, s; HBITMAP mk;
        if (!md_create(&d, 16, 8, 32) || !md_create(&s, 16, 8, 32)) continue;
        md_clear(&d, GREEN_); md_clear(&s, BLUE_);
        mk = mask_cols(16, 8, 8);
        CHECK(MB(d.dc, 0, 0, 16, 8, s.dc, 0, 0, mk, 0, 0, MAKEROP4(SRCCOPY, backs[k])));
        CHECKF(md_count(&d, 0, 0, 8, 8, BLUE_) == 64, "k=%u fg", k);
        CHECKF(md_count(&d, 8, 0, 16, 8, want[k]) == 64, "k=%u bg", k);
        U(DeleteObject)(mk);
        md_destroy(&d); md_destroy(&s);
    }
}
TEST(gap10_maskblt_all_zero_mask) {
    NEED_MB();
    MD d, s; HBITMAP mk;
    if (!md_create(&d, 16, 8, 32) || !md_create(&s, 16, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, BLUE_);
    mk = mask_cols(16, 8, 0);
    CHECK(MB(d.dc, 0, 0, 16, 8, s.dc, 0, 0, mk, 0, 0, MAKEROP4(SRCCOPY, 0x00AA0029)));
    EXPECT_EQ(md_diff(&d, WHITE_), 0);
    U(DeleteObject)(mk);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_maskblt_all_one_mask) {
    NEED_MB();
    MD d, s; HBITMAP mk;
    if (!md_create(&d, 16, 8, 32) || !md_create(&s, 16, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, BLUE_);
    mk = mask_cols(16, 8, 16);
    CHECK(MB(d.dc, 0, 0, 16, 8, s.dc, 0, 0, mk, 0, 0, MAKEROP4(SRCCOPY, 0x00AA0029)));
    EXPECT_EQ(md_count(&d, 0, 0, 16, 8, BLUE_), 128);
    U(DeleteObject)(mk);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_maskblt_mask_origin_offsets) {
    NEED_MB();
    MD d, s; HBITMAP mk; int xm;
    if (!md_create(&d, 16, 8, 32) || !md_create(&s, 16, 8, 32)) { t_skip("no DIB"); return; }
    mk = mask_cols(16, 8, 8);
    for (xm = 0; xm <= 4; xm += 2) {
        md_clear(&d, WHITE_); md_clear(&s, BLUE_);
        CHECK(MB(d.dc, 0, 0, 12, 8, s.dc, 0, 0, mk, xm, 0, MAKEROP4(SRCCOPY, 0x00AA0029)));
        /* destination column x uses mask column x + xm: set for x + xm < 8 */
        CHECKF(md_count(&d, 0, 0, 8 - xm, 8, BLUE_) == (8 - xm) * 8, "xm=%d fg", xm);
        CHECKF(md_count(&d, 8 - xm, 0, 12, 8, WHITE_) == (4 + xm) * 8, "xm=%d bg", xm);
    }
    U(DeleteObject)(mk);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_maskblt_source_origin_offset) {
    NEED_MB();
    MD d, s; HBITMAP mk;
    if (!md_create(&d, 16, 8, 32) || !mk_split(&s, 16, 8, 32, RED_, GREEN_)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_);
    mk = mask_cols(16, 8, 16);
    CHECK(MB(d.dc, 0, 0, 8, 8, s.dc, 8, 0, mk, 0, 0, MAKEROP4(SRCCOPY, 0x00AA0029)));
    EXPECT_EQ(md_count(&d, 0, 0, 8, 8, GREEN_), 64);
    U(DeleteObject)(mk);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_maskblt_color_mask_rejected) {
    NEED_MB();
    MD d, s; HBITMAP mk; HDC ref;
    if (!md_create(&d, 16, 8, 32) || !md_create(&s, 16, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, BLUE_);
    ref = U(CreateDCW)(L"DISPLAY", NULL, NULL, NULL);
    mk = ref ? U(CreateCompatibleBitmap)(ref, 16, 8) : NULL;
    if (!mk) { if (ref) U(DeleteDC)(ref); md_destroy(&d); md_destroy(&s); t_skip("no colour DDB"); return; }
    CLR();
    BOOL r = MB(d.dc, 0, 0, 16, 8, s.dc, 0, 0, mk, 0, 0, MAKEROP4(SRCCOPY, 0x00AA0029));
    CHECK(!r);
    t_note("colour mask: err %lu", GetLastError());
    EXPECT_EQ(md_diff(&d, WHITE_), 0);
    U(DeleteObject)(mk); U(DeleteDC)(ref);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_maskblt_dib_mask_rejected) {
    NEED_MB();
    MD d, s, mm;
    if (!md_create(&d, 16, 8, 32) || !md_create(&s, 16, 8, 32) || !md_create(&mm, 16, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, BLUE_);
    CLR();
    BOOL r = MB(d.dc, 0, 0, 16, 8, s.dc, 0, 0, mm.bmp, 0, 0, MAKEROP4(SRCCOPY, 0x00AA0029));
    CHECK(!r);
    t_note("32bpp DIB section as mask: err %lu", GetLastError());
    md_destroy(&d); md_destroy(&s); md_destroy(&mm);
}
TEST(gap10_maskblt_bogus_mask_handle) {
    NEED_MB();
    MD d, s; HBITMAP bad[] = { (HBITMAP)(ULONG_PTR)1, (HBITMAP)(ULONG_PTR)0xdeadbeef }; unsigned i;
    if (!md_create(&d, 16, 8, 32) || !md_create(&s, 16, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, BLUE_);
    for (i = 0; i < 2; i++) {
        CLR();
        BOOL r = MB(d.dc, 0, 0, 16, 8, s.dc, 0, 0, bad[i], 0, 0, MAKEROP4(SRCCOPY, 0x00AA0029));
        CHECKF(!r, "mask %p", bad[i]);
    }
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_maskblt_brush_as_mask_rejected) {
    NEED_MB();
    MD d, s; HBRUSH b = U(CreateSolidBrush)(0);
    if (!md_create(&d, 16, 8, 32) || !md_create(&s, 16, 8, 32)) { t_skip("no DIB"); return; }
    CLR();
    CHECK(!MB(d.dc, 0, 0, 8, 8, s.dc, 0, 0, (HBITMAP)b, 0, 0, SRCCOPY));
    U(DeleteObject)(b);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_maskblt_zero_and_negative_size_observed) {
    NEED_MB();
    MD d, s; int sz[][2] = { {0, 8}, {8, 0}, {0, 0}, {-8, 8}, {8, -8} }; unsigned i;
    if (!md_create(&d, 16, 16, 32) || !md_create(&s, 16, 16, 32)) { t_skip("no DIB"); return; }
    for (i = 0; i < sizeof sz / sizeof *sz; i++) {
        md_clear(&d, WHITE_); md_clear(&s, RED_);
        CLR();
        BOOL r = MB(d.dc, 4, 4, sz[i][0], sz[i][1], s.dc, 4, 4, NULL, 0, 0, SRCCOPY);
        t_note("size %d x %d -> %d, changed %d", sz[i][0], sz[i][1], r, md_diff(&d, WHITE_));
    }
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_maskblt_clip_applies) {
    NEED_MB(); REQUIRE(U(IntersectClipRect));
    MD d, s;
    if (!md_create(&d, 16, 8, 32) || !md_create(&s, 16, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, BLUE_);
    U(IntersectClipRect)(d.dc, 2, 2, 6, 6);
    CHECK(MB(d.dc, 0, 0, 16, 8, s.dc, 0, 0, NULL, 0, 0, SRCCOPY));
    EXPECT_EQ(md_count(&d, 0, 0, 16, 8, BLUE_), 16);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_maskblt_pattern_rop_with_brush) {
    NEED_MB();
    MD d, s; HBRUSH b; HGDIOBJ ob; HBITMAP mk;
    if (!md_create(&d, 16, 8, 32) || !md_create(&s, 16, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, BLUE_);
    b = U(CreateSolidBrush)(GREEN_); ob = U(SelectObject)(d.dc, b);
    mk = mask_cols(16, 8, 8);
    /* foreground: copy source; background: PATCOPY (0x00F00021) */
    CHECK(MB(d.dc, 0, 0, 16, 8, s.dc, 0, 0, mk, 0, 0, MAKEROP4(SRCCOPY, PATCOPY)));
    EXPECT_EQ(md_count(&d, 0, 0, 8, 8, BLUE_), 64);
    EXPECT_EQ(md_count(&d, 8, 0, 16, 8, GREEN_), 64);
    U(SelectObject)(d.dc, ob); U(DeleteObject)(b); U(DeleteObject)(mk);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_maskblt_mono_source_colors) {
    NEED_MB(); REQUIRE(U(SetTextColor)); REQUIRE(U(SetBkColor));
    MD d; HDC sdc; HBITMAP sb; HGDIOBJ os; HBITMAP mk;
    if (!md_create(&d, 16, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_);
    sdc = U(CreateCompatibleDC)(NULL);
    sb = mask_cols(16, 8, 8);     /* mono source, left half 1s */
    os = U(SelectObject)(sdc, sb);
    U(SetTextColor)(d.dc, RED_); U(SetBkColor)(d.dc, GREEN_);
    mk = mask_cols(16, 8, 16);
    CHECK(MB(d.dc, 0, 0, 16, 8, sdc, 0, 0, mk, 0, 0, MAKEROP4(SRCCOPY, 0x00AA0029)));
    t_note("mono->colour via MaskBlt: left %06lx right %06lx", md_px(&d, 2, 2), md_px(&d, 12, 2));
    CHECK(md_px(&d, 2, 2) != md_px(&d, 12, 2));
    U(SelectObject)(sdc, os); U(DeleteObject)(sb); U(DeleteObject)(mk); U(DeleteDC)(sdc);
    md_destroy(&d);
}
TEST(gap10_maskblt_does_not_modify_source_or_mask) {
    NEED_MB(); REQUIRE(U(GetBitmapBits));
    MD d, s; HBITMAP mk; BYTE b1[16], b2[16];
    if (!md_create(&d, 16, 8, 32) || !md_create(&s, 16, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, BLUE_);
    mk = mask_cols(16, 8, 5);
    U(GetBitmapBits)(mk, 16, b1);
    MB(d.dc, 0, 0, 16, 8, s.dc, 0, 0, mk, 0, 0, MAKEROP4(SRCCOPY, DSTINVERT));
    U(GetBitmapBits)(mk, 16, b2);
    CHECK(!memcmp(b1, b2, 16));
    EXPECT_EQ(md_diff(&s, BLUE_), 0);
    U(DeleteObject)(mk);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_maskblt_mask_selected_in_dc_observed) {
    NEED_MB();
    MD d, s; HBITMAP mk; HDC mdc; HGDIOBJ om;
    if (!md_create(&d, 16, 8, 32) || !md_create(&s, 16, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, BLUE_);
    mk = mask_cols(16, 8, 8);
    mdc = U(CreateCompatibleDC)(NULL); om = U(SelectObject)(mdc, mk);
    BOOL r = MB(d.dc, 0, 0, 16, 8, s.dc, 0, 0, mk, 0, 0, MAKEROP4(SRCCOPY, 0x00AA0029));
    t_note("mask selected into another DC -> %d (blue px %d)", r, md_count(&d, 0, 0, 16, 8, BLUE_));
    U(SelectObject)(mdc, om); U(DeleteDC)(mdc); U(DeleteObject)(mk);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_maskblt_gdi32_full_agree) {
    NEED_MB(); REQUIRE(UG(MaskBlt));
    MD d1, d2, s; HBITMAP mk;
    if (!md_create(&d1, 16, 8, 32) || !md_create(&d2, 16, 8, 32) || !md_create(&s, 16, 8, 32)) { t_skip("no DIB"); return; }
    md_clear(&d1, WHITE_); md_clear(&d2, WHITE_); md_clear(&s, BLUE_);
    mk = mask_cols(16, 8, 7);
    CHECK(UG(MaskBlt)(d1.dc, 0, 0, 16, 8, s.dc, 0, 0, mk, 0, 0, MAKEROP4(SRCCOPY, DSTINVERT)));
    if (UF(MaskBlt)) {
        CHECK(UF(MaskBlt)(d2.dc, 0, 0, 16, 8, s.dc, 0, 0, mk, 0, 0, MAKEROP4(SRCCOPY, DSTINVERT)));
        EXPECT_EQ(memcmp(d1.bits, d2.bits, 16 * 8 * 4), 0);
    }
    U(DeleteObject)(mk);
    md_destroy(&d1); md_destroy(&d2); md_destroy(&s);
}
TEST(gap10_maskblt_repeated_no_leak) {
    NEED_MB();
    MD d, s; HBITMAP mk; int i;
    if (!md_create(&d, 16, 8, 32) || !md_create(&s, 16, 8, 32)) { t_skip("no DIB"); return; }
    mk = mask_cols(16, 8, 8);
    DWORD before = g10_gdicount();
    for (i = 0; i < 200; i++) MB(d.dc, 0, 0, 16, 8, s.dc, 0, 0, mk, 0, 0, MAKEROP4(SRCCOPY, DSTINVERT));
    EXPECT_EQ(g10_gdicount(), before);
    U(DeleteObject)(mk);
    md_destroy(&d); md_destroy(&s);
}

/* ============================ PlgBlt ============================ */
#define PB U(PlgBlt)
#define NEED_PB() do { NEED_BLT(); REQUIRE(PB); } while (0)
static void pts3(POINT *p, int x0, int y0, int x1, int y1, int x2, int y2) {
    p[0].x = x0; p[0].y = y0; p[1].x = x1; p[1].y = y1; p[2].x = x2; p[2].y = y2;
}
TEST(gap10_plgblt_null_source_fails_silently) {
    NEED_PB();
    MD d; POINT p[3];
    if (!md_create(&d, 32, 32, 32)) { t_skip("no DIB"); return; }
    pts3(p, 0, 0, 8, 0, 0, 8);
    CLR();
    CHECK(!PB(d.dc, p, NULL, 0, 0, 8, 8, NULL, 0, 0));
    EXPECT_EQ(GetLastError(), ERRS);
    md_destroy(&d);
}
TEST(gap10_plgblt_invalid_dest_sets_6) {
    NEED_PB();
    MD s; POINT p[3]; HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef }; unsigned i;
    if (!md_create(&s, 16, 16, 32)) { t_skip("no DIB"); return; }
    pts3(p, 0, 0, 8, 0, 0, 8);
    for (i = 0; i < 3; i++) {
        CLR();
        CHECKF(!PB(bad[i], p, s.dc, 0, 0, 8, 8, NULL, 0, 0), "dest %p", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "dest %p err %lu", bad[i], GetLastError());
    }
    md_destroy(&s);
}
TEST(gap10_plgblt_dest_wrong_type) {
    NEED_PB();
    MD s; POINT p[3]; HBRUSH b = U(CreateSolidBrush)(0);
    if (!md_create(&s, 16, 16, 32)) { t_skip("no DIB"); return; }
    pts3(p, 0, 0, 8, 0, 0, 8);
    CLR();
    CHECK(!PB((HDC)b, p, s.dc, 0, 0, 8, 8, NULL, 0, 0));
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    U(DeleteObject)(b); md_destroy(&s);
}
TEST(gap10_plgblt_null_points_fail) {
    NEED_PB();
    MD d, s;
    if (!md_create(&d, 32, 32, 32) || !md_create(&s, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, RED_);
    CLR();
    CHECK(!PB(d.dc, NULL, s.dc, 0, 0, 8, 8, NULL, 0, 0));
    EXPECT_EQ(md_diff(&d, WHITE_), 0);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_plgblt_axis_aligned_split_source) {
    NEED_PB();
    int bpps[] = { 32, 24, 16 }; unsigned k;
    for (k = 0; k < 3; k++) {
        MD d, s; POINT p[3];
        if (!md_create(&d, 40, 40, bpps[k]) || !mk_split(&s, 16, 16, bpps[k], RED_, GREEN_)) continue;
        md_clear(&d, WHITE_);
        pts3(p, 10, 10, 26, 10, 10, 26);
        CHECK(PB(d.dc, p, s.dc, 0, 0, 16, 16, NULL, 0, 0));
        CHECKF(md_px(&d, 12, 18) == RED_ && md_px(&d, 17, 12) == RED_, "bpp %d left half", bpps[k]);
        CHECKF(md_px(&d, 23, 18) == GREEN_ && md_px(&d, 19, 24) == GREEN_, "bpp %d right half", bpps[k]);
        CHECKF(md_px(&d, 5, 18) == WHITE_ && md_px(&d, 30, 18) == WHITE_ && md_px(&d, 18, 5) == WHITE_ && md_px(&d, 18, 30) == WHITE_, "bpp %d outside", bpps[k]);
        int n = md_diff(&d, WHITE_);
        CHECKF(n >= 14 * 14 && n <= 18 * 18, "bpp %d painted %d", bpps[k], n);
        md_destroy(&d); md_destroy(&s);
    }
}
TEST(gap10_plgblt_identity_exact_observed) {
    NEED_PB();
    MD d, s; POINT p[3]; int x, y, bad = 0;
    if (!md_create(&d, 24, 24, 32) || !mk_split(&s, 8, 8, 32, RED_, GREEN_)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_);
    pts3(p, 4, 4, 12, 4, 4, 12);
    CHECK(PB(d.dc, p, s.dc, 0, 0, 8, 8, NULL, 0, 0));
    for (y = 0; y < 24; y++) for (x = 0; x < 24; x++) {
        COLORREF want = (x >= 4 && x < 12 && y >= 4 && y < 12) ? (x < 8 ? RED_ : GREEN_) : WHITE_;
        if (md_px(&d, x, y) != want) bad++;
    }
    t_note("identity PlgBlt: %d mismatching pixels", bad);
    CHECK(bad <= 16);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_plgblt_mirror_horizontal) {
    NEED_PB();
    MD d, s; POINT p[3];
    if (!md_create(&d, 40, 40, 32) || !mk_split(&s, 16, 16, 32, RED_, GREEN_)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_);
    pts3(p, 26, 10, 10, 10, 26, 26);     /* upper-left maps to the right edge */
    CHECK(PB(d.dc, p, s.dc, 0, 0, 16, 16, NULL, 0, 0));
    EXPECT_EQ(md_px(&d, 13, 18), GREEN_);
    EXPECT_EQ(md_px(&d, 22, 18), RED_);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_plgblt_mirror_vertical) {
    NEED_PB();
    MD d, s; POINT p[3]; int x, y;
    if (!md_create(&d, 40, 40, 32) || !md_create(&s, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_rect(&s, 0, 0, 16, 8, RED_); md_rect(&s, 0, 8, 16, 8, GREEN_);
    md_clear(&d, WHITE_);
    pts3(p, 10, 26, 26, 26, 10, 10);     /* flipped top/bottom */
    CHECK(PB(d.dc, p, s.dc, 0, 0, 16, 16, NULL, 0, 0));
    EXPECT_EQ(md_px(&d, 18, 13), GREEN_);
    EXPECT_EQ(md_px(&d, 18, 22), RED_);
    (void)x; (void)y;
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_plgblt_rotate_90) {
    NEED_PB();
    MD d, s; POINT p[3];
    if (!md_create(&d, 40, 40, 32) || !md_create(&s, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_rect(&s, 0, 0, 8, 16, RED_); md_rect(&s, 8, 0, 8, 16, GREEN_);   /* left red, right green */
    md_clear(&d, WHITE_);
    /* upper-left -> upper-right, upper-right -> lower-right, lower-left -> upper-left: rotate 90 deg clockwise */
    pts3(p, 26, 10, 26, 26, 10, 10);
    CHECK(PB(d.dc, p, s.dc, 0, 0, 16, 16, NULL, 0, 0));
    /* source left (red) goes to the top, source right (green) to the bottom */
    EXPECT_EQ(md_px(&d, 18, 13), RED_);
    EXPECT_EQ(md_px(&d, 18, 22), GREEN_);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_plgblt_shear_parallelogram_bounds) {
    NEED_PB();
    MD d, s; POINT p[3];
    if (!md_create(&d, 48, 32, 32) || !md_create(&s, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, BLUE_);
    pts3(p, 8, 4, 24, 4, 16, 20);        /* fourth point = (32,20) */
    CHECK(PB(d.dc, p, s.dc, 0, 0, 16, 16, NULL, 0, 0));
    EXPECT_EQ(md_px(&d, 16, 12), BLUE_);
    EXPECT_EQ(md_px(&d, 12, 8), BLUE_);
    EXPECT_EQ(md_px(&d, 28, 16), BLUE_);
    EXPECT_EQ(md_px(&d, 8, 18), WHITE_);        /* left of the lower-left corner */
    EXPECT_EQ(md_px(&d, 30, 6), WHITE_);        /* right of the upper-right edge */
    EXPECT_EQ(md_px(&d, 2, 2), WHITE_);
    int n = md_diff(&d, WHITE_);
    CHECKF(n > 230 && n < 290, "area %d (ideal 256)", n);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_plgblt_scale_up) {
    NEED_PB();
    MD d, s; POINT p[3];
    if (!md_create(&d, 40, 40, 32) || !mk_split(&s, 8, 8, 32, RED_, GREEN_)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_);
    pts3(p, 4, 4, 36, 4, 4, 36);
    CHECK(PB(d.dc, p, s.dc, 0, 0, 8, 8, NULL, 0, 0));
    EXPECT_EQ(md_px(&d, 10, 20), RED_);
    EXPECT_EQ(md_px(&d, 30, 20), GREEN_);
    int n = md_diff(&d, WHITE_);
    CHECKF(n > 980 && n < 1070, "painted %d (ideal 1024)", n);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_plgblt_source_subrect) {
    NEED_PB();
    MD d, s; POINT p[3];
    if (!md_create(&d, 40, 40, 32) || !mk_split(&s, 32, 16, 32, RED_, GREEN_)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_);
    pts3(p, 4, 4, 20, 4, 4, 20);
    CHECK(PB(d.dc, p, s.dc, 16, 0, 16, 16, NULL, 0, 0));   /* the green half only */
    EXPECT_EQ(md_px(&d, 12, 12), GREEN_);
    EXPECT_EQ(md_count(&d, 4, 4, 20, 20, RED_), 0);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_plgblt_mask_selects_pixels) {
    NEED_PB(); REQUIRE(U(CreateBitmap));
    MD d, s; POINT p[3]; HBITMAP mk;
    if (!md_create(&d, 40, 40, 32) || !md_create(&s, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, BLUE_);
    mk = mask_cols(16, 16, 8);
    pts3(p, 10, 10, 26, 10, 10, 26);
    CHECK(PB(d.dc, p, s.dc, 0, 0, 16, 16, mk, 0, 0));
    EXPECT_EQ(md_px(&d, 13, 18), BLUE_);
    EXPECT_EQ(md_px(&d, 22, 18), WHITE_);
    U(DeleteObject)(mk);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_plgblt_color_mask_rejected) {
    NEED_PB();
    MD d, s; POINT p[3]; HDC ref; HBITMAP mk;
    if (!md_create(&d, 40, 40, 32) || !md_create(&s, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, BLUE_);
    ref = U(CreateDCW)(L"DISPLAY", NULL, NULL, NULL);
    mk = ref ? U(CreateCompatibleBitmap)(ref, 16, 16) : NULL;
    if (!mk) { if (ref) U(DeleteDC)(ref); md_destroy(&d); md_destroy(&s); t_skip("no colour DDB"); return; }
    pts3(p, 10, 10, 26, 10, 10, 26);
    CLR();
    CHECK(!PB(d.dc, p, s.dc, 0, 0, 16, 16, mk, 0, 0));
    EXPECT_EQ(md_diff(&d, WHITE_), 0);
    U(DeleteObject)(mk); U(DeleteDC)(ref);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_plgblt_zero_negative_size_observed) {
    NEED_PB();
    MD d, s; POINT p[3]; int sz[][2] = { {0, 8}, {8, 0}, {-8, 8}, {8, -8}, {0, 0} }; unsigned i;
    if (!md_create(&d, 40, 40, 32) || !md_create(&s, 16, 16, 32)) { t_skip("no DIB"); return; }
    pts3(p, 10, 10, 26, 10, 10, 26);
    for (i = 0; i < sizeof sz / sizeof *sz; i++) {
        md_clear(&d, WHITE_); md_clear(&s, BLUE_);
        CLR();
        BOOL r = PB(d.dc, p, s.dc, 0, 0, sz[i][0], sz[i][1], NULL, 0, 0);
        t_note("size %d x %d -> %d err %lu changed %d", sz[i][0], sz[i][1], r, GetLastError(), md_diff(&d, WHITE_));
    }
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_plgblt_degenerate_parallelogram_observed) {
    NEED_PB();
    MD d, s; POINT p[3];
    if (!md_create(&d, 40, 40, 32) || !md_create(&s, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, BLUE_);
    pts3(p, 10, 10, 20, 10, 30, 10);     /* collinear */
    CLR();
    BOOL r = PB(d.dc, p, s.dc, 0, 0, 16, 16, NULL, 0, 0);
    t_note("collinear points -> %d err %lu changed %d", r, GetLastError(), md_diff(&d, WHITE_));
    pts3(p, 10, 10, 10, 10, 10, 10);
    r = PB(d.dc, p, s.dc, 0, 0, 16, 16, NULL, 0, 0);
    t_note("identical points -> %d changed %d", r, md_diff(&d, WHITE_));
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_plgblt_clip_applies) {
    NEED_PB(); REQUIRE(U(IntersectClipRect));
    MD d, s; POINT p[3];
    if (!md_create(&d, 40, 40, 32) || !md_create(&s, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, BLUE_);
    U(IntersectClipRect)(d.dc, 0, 0, 18, 40);
    pts3(p, 10, 10, 26, 10, 10, 26);
    CHECK(PB(d.dc, p, s.dc, 0, 0, 16, 16, NULL, 0, 0));
    EXPECT_EQ(md_px(&d, 14, 18), BLUE_);
    EXPECT_EQ(md_px(&d, 22, 18), WHITE_);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_plgblt_points_not_modified) {
    NEED_PB();
    MD d, s; POINT p[3], q[3];
    if (!md_create(&d, 40, 40, 32) || !md_create(&s, 16, 16, 32)) { t_skip("no DIB"); return; }
    pts3(p, 8, 8, 24, 12, 12, 24); memcpy(q, p, sizeof p);
    PB(d.dc, p, s.dc, 0, 0, 16, 16, NULL, 0, 0);
    CHECK(!memcmp(p, q, sizeof p));
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_plgblt_dest_offscreen_partially) {
    NEED_PB();
    MD d, s; POINT p[3];
    if (!md_create(&d, 32, 32, 32) || !md_create(&s, 16, 16, 32)) { t_skip("no DIB"); return; }
    md_clear(&d, WHITE_); md_clear(&s, BLUE_);
    pts3(p, 24, 24, 40, 24, 24, 40);
    CLR();
    BOOL r = PB(d.dc, p, s.dc, 0, 0, 16, 16, NULL, 0, 0);
    t_note("parallelogram partly outside -> %d", r);
    if (r) EXPECT_EQ(md_px(&d, 28, 28), BLUE_);
    pts3(p, -8, -8, 8, -8, -8, 8);
    r = PB(d.dc, p, s.dc, 0, 0, 16, 16, NULL, 0, 0);
    if (r) EXPECT_EQ(md_px(&d, 2, 2), BLUE_);
    md_destroy(&d); md_destroy(&s);
}
TEST(gap10_plgblt_gdi32_full_agree) {
    NEED_PB(); REQUIRE(UG(PlgBlt));
    MD d1, d2, s; POINT p[3];
    if (!md_create(&d1, 40, 40, 32) || !md_create(&d2, 40, 40, 32) || !mk_split(&s, 16, 16, 32, RED_, GREEN_)) { t_skip("no DIB"); return; }
    md_clear(&d1, WHITE_); md_clear(&d2, WHITE_);
    pts3(p, 6, 8, 28, 14, 12, 30);
    CHECK(UG(PlgBlt)(d1.dc, p, s.dc, 0, 0, 16, 16, NULL, 0, 0));
    if (UF(PlgBlt)) {
        CHECK(UF(PlgBlt)(d2.dc, p, s.dc, 0, 0, 16, 16, NULL, 0, 0));
        EXPECT_EQ(memcmp(d1.bits, d2.bits, 40 * 40 * 4), 0);
    }
    md_destroy(&d1); md_destroy(&d2); md_destroy(&s);
}
TEST(gap10_plgblt_repeated_no_leak) {
    NEED_PB();
    MD d, s; POINT p[3]; int i;
    if (!md_create(&d, 40, 40, 32) || !md_create(&s, 16, 16, 32)) { t_skip("no DIB"); return; }
    pts3(p, 4, 6, 26, 10, 8, 28);
    DWORD before = g10_gdicount();
    for (i = 0; i < 200; i++) PB(d.dc, p, s.dc, 0, 0, 16, 16, NULL, 0, 0);
    EXPECT_EQ(g10_gdicount(), before);
    md_destroy(&d); md_destroy(&s);
}
