#include "emf.h"

static HENHMETAFILE mk_patblt(int x, int y, int w, int h) {
    HDC dc = emf_begin();
    if (!dc) return NULL;
    U(PatBlt)(dc, x, y, w, h, BLACKNESS);
    return emf_end(dc);
}
static int get_hdr(HENHMETAFILE h, ENHMETAHEADER *hd) {
    memset(hd, 0xCC, sizeof *hd);
    return U(GetEnhMetaFileHeader)(h, sizeof *hd, hd);
}

/* ---------- header field basics ---------- */

TEST(emf_hdr_fixed_fields) {
    REQUIRE(U(GetEnhMetaFileHeader));
    HENHMETAFILE h = mk_patblt(0, 0, 10, 10);
    if (!h) { CHECK(h != NULL); return; }
    ENHMETAHEADER hd;
    UINT r = get_hdr(h, &hd);
    EXPECT_EQ(r, 108);
    EXPECT_EQ(hd.iType, EMR_HEADER);
    EXPECT_EQ(hd.nSize, 108);
    EXPECT_EQ(hd.dSignature, 0x464D4520);
    EXPECT_EQ(hd.nVersion, 0x10000);
    EXPECT_EQ(hd.sReserved, 0);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_hdr_size_query_null_buffer) {
    REQUIRE(U(GetEnhMetaFileHeader));
    HENHMETAFILE h = mk_patblt(0, 0, 10, 10);
    if (!h) { CHECK(h != NULL); return; }
    UINT n = U(GetEnhMetaFileHeader)(h, 0, NULL);
    CHECK(n >= 88);
    /* no description -> the record is the header alone */
    CHECK(n == 88 || n == 100 || n == 108);
    UINT n2 = U(GetEnhMetaFileHeader)(h, 1000, NULL);
    EXPECT_EQ(n2, n);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_hdr_size_query_with_description) {
    REQUIRE(U(GetEnhMetaFileHeader));
    HDC dc = U(CreateEnhMetaFileW)(NULL, NULL, NULL, L"Appname\0Picture\0");
    if (!dc) { CHECK(dc != NULL); return; }
    U(PatBlt)(dc, 0, 0, 4, 4, BLACKNESS);
    HENHMETAFILE h = emf_end(dc);
    UINT n = U(GetEnhMetaFileHeader)(h, 0, NULL);
    ENHMETAHEADER hd;
    UINT r = get_hdr(h, &hd);
    /* the size query returns the whole header record (including description),
     * the buffer form reports only the struct size */
    CHECK(n > 108);
    EXPECT_EQ(r, 108);
    EXPECT_EQ(hd.nSize, 108);
    CHECK(hd.nDescription > 0);
    CHECK(hd.offDescription >= 100);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_hdr_buffer_too_small) {
    REQUIRE(U(GetEnhMetaFileHeader));
    HENHMETAFILE h = mk_patblt(0, 0, 10, 10);
    if (!h) { CHECK(h != NULL); return; }
    ENHMETAHEADER hd;
    static const UINT small[] = { 1, 4, 8, 50, 87 };
    for (unsigned i = 0; i < sizeof small / sizeof small[0]; i++) {
        SetLastError(0);
        memset(&hd, 0xCC, sizeof hd);
        UINT r = U(GetEnhMetaFileHeader)(h, small[i], &hd);
        CHECKF(r == 0, "size %u -> %u", small[i], r);
        EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
        CHECKF(((BYTE *)&hd)[0] == 0xCC, "buffer written on failure (size %u)", small[i]);
    }
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_hdr_buffer_sizes_88_100_108) {
    REQUIRE(U(GetEnhMetaFileHeader));
    HENHMETAFILE h = mk_patblt(0, 0, 10, 10);
    if (!h) { CHECK(h != NULL); return; }
    BYTE buf[256];
    struct { UINT in; UINT want; } t[] = { {88, 88}, {89, 88}, {99, 88}, {100, 100}, {107, 100}, {108, 108}, {109, 108}, {200, 108}, {256, 108} };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        memset(buf, 0xAA, sizeof buf);
        UINT r = U(GetEnhMetaFileHeader)(h, t[i].in, (ENHMETAHEADER *)buf);
        CHECKF(r == t[i].want, "in %u got %u want %u", t[i].in, r, t[i].want);
        if (r == t[i].want) {
            CHECKF(((ENHMETAHEADER *)buf)->nSize == t[i].want, "nSize field %lu", ((ENHMETAHEADER *)buf)->nSize);
            CHECKF(buf[t[i].want] == 0xAA, "overrun at in=%u", t[i].in);
            EXPECT_EQ(((ENHMETAHEADER *)buf)->dSignature, ENHMETA_SIGNATURE);
        }
    }
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_hdr_prefix_stable_across_sizes) {
    REQUIRE(U(GetEnhMetaFileHeader));
    HENHMETAFILE h = mk_patblt(3, 4, 20, 30);
    if (!h) { CHECK(h != NULL); return; }
    BYTE a[108], b[108];
    CHECK(U(GetEnhMetaFileHeader)(h, 108, (ENHMETAHEADER *)a) == 108);
    CHECK(U(GetEnhMetaFileHeader)(h, 88, (ENHMETAHEADER *)b) == 88);
    CHECK(!memcmp(a + 8, b + 8, 80));
    CHECK(U(GetEnhMetaFileHeader)(h, 100, (ENHMETAHEADER *)b) == 100);
    CHECK(!memcmp(a + 8, b + 8, 92));
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_hdr_repeated_calls_identical) {
    REQUIRE(U(GetEnhMetaFileHeader));
    HENHMETAFILE h = mk_patblt(3, 4, 20, 30);
    if (!h) { CHECK(h != NULL); return; }
    ENHMETAHEADER a, b;
    get_hdr(h, &a);
    for (int i = 0; i < 50; i++) { get_hdr(h, &b); CHECK(!memcmp(&a, &b, sizeof a)); }
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_hdr_invalid_handles) {
    REQUIRE(U(GetEnhMetaFileHeader)); REQUIRE(U(CreatePen));
    HPEN pen = U(CreatePen)(PS_SOLID, 1, 0);
    HBITMAP bmp = U(CreateBitmap)(4, 4, 1, 1, NULL);
    HDC mdc = U(CreateCompatibleDC)(NULL);
    HENHMETAFILE bad[] = { NULL, (HENHMETAFILE)1, (HENHMETAFILE)0xdeadbeef, (HENHMETAFILE)pen, (HENHMETAFILE)bmp,
                           (HENHMETAFILE)mdc, (HENHMETAFILE)U(GetStockObject)(BLACK_BRUSH) };
    ENHMETAHEADER hd;
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        CHECKF(U(GetEnhMetaFileHeader)(bad[i], sizeof hd, &hd) == 0, "case %u", i);
        CHECKF(U(GetEnhMetaFileHeader)(bad[i], 0, NULL) == 0, "null %u", i);
    }
    U(DeleteDC)(mdc); U(DeleteObject)(bmp); U(DeleteObject)(pen);
}

TEST(emf_hdr_deleted_handle) {
    REQUIRE(U(GetEnhMetaFileHeader));
    HENHMETAFILE h = mk_patblt(0, 0, 4, 4);
    if (!h) { CHECK(h != NULL); return; }
    U(DeleteEnhMetaFile)(h);
    ENHMETAHEADER hd;
    CHECK(U(GetEnhMetaFileHeader)(h, sizeof hd, &hd) == 0);
    CHECK(U(DeleteEnhMetaFile)(h) == FALSE);
}

TEST(emf_hdr_nbytes_matches_bits) {
    REQUIRE(U(GetEnhMetaFileHeader)); REQUIRE(U(GetEnhMetaFileBits));
    HENHMETAFILE h = mk_patblt(0, 0, 16, 16);
    if (!h) { CHECK(h != NULL); return; }
    ENHMETAHEADER hd; get_hdr(h, &hd);
    EXPECT_EQ(U(GetEnhMetaFileBits)(h, 0, NULL), hd.nBytes);
    CHECK(hd.nBytes % 4 == 0);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_hdr_nrecords_matches_enum) {
    REQUIRE(U(EnumEnhMetaFile));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    U(Rectangle)(dc, 1, 1, 10, 10); U(Ellipse)(dc, 2, 2, 20, 20); U(MoveToEx)(dc, 1, 1, NULL); U(LineTo)(dc, 8, 8);
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; get_hdr(h, &hd);
    emf_log l; emf_enum(h, &l);
    EXPECT_EQ(l.n, hd.nRecords);
    CHECK(hd.nRecords >= 6);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_hdr_empty_metafile) {
    REQUIRE(U(EnumEnhMetaFile));
    HENHMETAFILE h;
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    h = emf_end(dc);
    ENHMETAHEADER hd; CHECK(get_hdr(h, &hd) != 0);
    if (hd.nRecords != 2) t_note("empty emf nRecords=%lu", hd.nRecords);
    CHECK(hd.nRecords >= 2);
    CHECK(hd.nHandles >= 1);
    emf_log l; emf_enum(h, &l);
    EXPECT_EQ(l.n, hd.nRecords);
    CHECK(l.n >= 2 && l.type[0] == EMR_HEADER && l.type[l.n - 1] == EMR_EOF);
    t_note("empty emf bounds [%ld %ld %ld %ld] frame [%ld %ld %ld %ld]", hd.rclBounds.left, hd.rclBounds.top, hd.rclBounds.right,
           hd.rclBounds.bottom, hd.rclFrame.left, hd.rclFrame.top, hd.rclFrame.right, hd.rclFrame.bottom);
    SOFT(hd.rclBounds.left == 0 && hd.rclBounds.top == 0 && hd.rclBounds.right == -1 && hd.rclBounds.bottom == -1);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_hdr_empty_metafile_with_frame) {
    RECT fr = {0, 0, 5000, 4000};
    HDC dc = U(CreateEnhMetaFileW)(NULL, NULL, &fr, NULL);
    if (!dc) { CHECK(dc != NULL); return; }
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; get_hdr(h, &hd);
    CHECK(!memcmp(&hd.rclFrame, &fr, sizeof fr));
    U(DeleteEnhMetaFile)(h);
}

/* ---------- bounds ---------- */

struct bcase { int x, y, w, h; };
static const struct bcase bcases[] = {
    {0, 0, 1, 1}, {0, 0, 10, 10}, {10, 20, 30, 40}, {5, 5, 100, 1}, {5, 5, 1, 100},
    {100, 100, 50, 50}, {-20, -30, 40, 60}, {0, 0, 640, 480}, {200, 300, 7, 9}, {1000, 900, 20, 20},
};

#define BOUNDS_TEST(i) \
TEST(emf_bounds_patblt_##i) { \
    REQUIRE(U(GetEnhMetaFileHeader)); \
    const struct bcase *c = &bcases[i]; \
    HENHMETAFILE h = mk_patblt(c->x, c->y, c->w, c->h); \
    if (!h) { CHECK(h != NULL); return; } \
    ENHMETAHEADER hd; get_hdr(h, &hd); \
    CHECKF(hd.rclBounds.left == c->x && hd.rclBounds.top == c->y && hd.rclBounds.right == c->x + c->w - 1 && \
           hd.rclBounds.bottom == c->y + c->h - 1, "bounds [%ld %ld %ld %ld]", hd.rclBounds.left, hd.rclBounds.top, \
           hd.rclBounds.right, hd.rclBounds.bottom); \
    U(DeleteEnhMetaFile)(h); \
}
BOUNDS_TEST(0) BOUNDS_TEST(1) BOUNDS_TEST(2) BOUNDS_TEST(3) BOUNDS_TEST(4)
BOUNDS_TEST(5) BOUNDS_TEST(6) BOUNDS_TEST(7) BOUNDS_TEST(8) BOUNDS_TEST(9)

TEST(emf_bounds_union_of_two) {
    REQUIRE(U(GetEnhMetaFileHeader));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    U(PatBlt)(dc, 10, 10, 10, 10, BLACKNESS);
    U(PatBlt)(dc, 100, 50, 20, 20, BLACKNESS);
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; get_hdr(h, &hd);
    EXPECT_EQ(hd.rclBounds.left, 10); EXPECT_EQ(hd.rclBounds.top, 10);
    EXPECT_EQ(hd.rclBounds.right, 119); EXPECT_EQ(hd.rclBounds.bottom, 69);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_bounds_not_shrunk_by_later_small_draw) {
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    U(PatBlt)(dc, 0, 0, 200, 200, BLACKNESS);
    U(PatBlt)(dc, 50, 50, 2, 2, BLACKNESS);
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; get_hdr(h, &hd);
    EXPECT_EQ(hd.rclBounds.right, 199); EXPECT_EQ(hd.rclBounds.bottom, 199);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_bounds_state_changes_do_not_grow) {
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    U(PatBlt)(dc, 10, 10, 10, 10, BLACKNESS);
    U(SetBkMode)(dc, TRANSPARENT); U(SetTextColor)(dc, 0x123456); U(SetROP2)(dc, R2_XORPEN);
    U(SetMapMode)(dc, MM_ANISOTROPIC); U(SetWindowOrgEx)(dc, 500, 500, NULL);
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; get_hdr(h, &hd);
    EXPECT_EQ(hd.rclBounds.left, 10); EXPECT_EQ(hd.rclBounds.right, 19);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_bounds_lineto_contains_endpoints) {
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    U(MoveToEx)(dc, 20, 30, NULL); U(LineTo)(dc, 120, 90);
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; get_hdr(h, &hd);
    CHECK(hd.rclBounds.left <= 20 && hd.rclBounds.top <= 30);
    CHECK(hd.rclBounds.right >= 119 && hd.rclBounds.bottom >= 89);
    CHECK(hd.rclBounds.left >= 18 && hd.rclBounds.top >= 28 && hd.rclBounds.right <= 122 && hd.rclBounds.bottom <= 92);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_bounds_rectangle_contains_shape) {
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    U(Rectangle)(dc, 10, 20, 60, 80);
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; get_hdr(h, &hd);
    CHECKF(hd.rclBounds.left <= 10 && hd.rclBounds.top <= 20 && hd.rclBounds.right >= 59 && hd.rclBounds.bottom >= 79,
           "bounds [%ld %ld %ld %ld]", hd.rclBounds.left, hd.rclBounds.top, hd.rclBounds.right, hd.rclBounds.bottom);
    CHECK(hd.rclBounds.left >= 8 && hd.rclBounds.top >= 18 && hd.rclBounds.right <= 62 && hd.rclBounds.bottom <= 82);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_bounds_wide_pen_expands) {
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    HPEN p = U(CreatePen)(PS_SOLID, 20, 0);
    HGDIOBJ o = U(SelectObject)(dc, p);
    U(MoveToEx)(dc, 100, 100, NULL); U(LineTo)(dc, 200, 100);
    U(SelectObject)(dc, o);
    HENHMETAFILE h = emf_end(dc);
    U(DeleteObject)(p);
    ENHMETAHEADER hd; get_hdr(h, &hd);
    t_note("wide pen line bounds [%ld %ld %ld %ld]", hd.rclBounds.left, hd.rclBounds.top, hd.rclBounds.right, hd.rclBounds.bottom);
    CHECK(hd.rclBounds.top <= 100 && hd.rclBounds.bottom >= 100);
    SOFT(hd.rclBounds.top <= 90 && hd.rclBounds.bottom >= 109);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_bounds_scaled_by_map_mode) {
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    U(SetMapMode)(dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(dc, 1, 1, NULL); U(SetViewportExtEx)(dc, 2, 2, NULL);
    U(PatBlt)(dc, 10, 10, 20, 20, BLACKNESS);
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; get_hdr(h, &hd);
    t_note("2x scaled bounds [%ld %ld %ld %ld]", hd.rclBounds.left, hd.rclBounds.top, hd.rclBounds.right, hd.rclBounds.bottom);
    /* bounds are device units: logical 10..29 at 2x covers 20..59 */
    SOFT(hd.rclBounds.left == 20 && hd.rclBounds.top == 20 && hd.rclBounds.right == 59 && hd.rclBounds.bottom == 59);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_bounds_world_transform_translation) {
    REQUIRE(U(SetWorldTransform)); REQUIRE(U(SetGraphicsMode));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    CHECK(U(SetGraphicsMode)(dc, GM_ADVANCED) == GM_COMPATIBLE);
    XFORM x = {1, 0, 0, 1, 100, 50};
    CHECK(U(SetWorldTransform)(dc, &x));
    U(PatBlt)(dc, 0, 0, 10, 10, BLACKNESS);
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; get_hdr(h, &hd);
    t_note("xform bounds [%ld %ld %ld %ld]", hd.rclBounds.left, hd.rclBounds.top, hd.rclBounds.right, hd.rclBounds.bottom);
    SOFT(hd.rclBounds.left == 100 && hd.rclBounds.top == 50 && hd.rclBounds.right == 109 && hd.rclBounds.bottom == 59);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_bounds_world_transform_scale_grows) {
    REQUIRE(U(SetWorldTransform)); REQUIRE(U(SetGraphicsMode));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    U(SetGraphicsMode)(dc, GM_ADVANCED);
    XFORM x = {3, 0, 0, 2, 0, 0};
    U(SetWorldTransform)(dc, &x);
    U(PatBlt)(dc, 0, 0, 10, 10, BLACKNESS);
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; get_hdr(h, &hd);
    SOFT(hd.rclBounds.right >= 29 && hd.rclBounds.bottom >= 19);
    t_note("xform scale bounds [%ld %ld %ld %ld]", hd.rclBounds.left, hd.rclBounds.top, hd.rclBounds.right, hd.rclBounds.bottom);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_worldtransform_requires_advanced_mode) {
    REQUIRE(U(SetWorldTransform));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    XFORM x = {2, 0, 0, 2, 0, 0};
    CHECK(U(SetWorldTransform)(dc, &x) == FALSE);
    SOFT(U(ModifyWorldTransform)(dc, &x, MWT_LEFTMULTIPLY) == FALSE);
    CHECK(U(GetGraphicsMode)(dc) == GM_COMPATIBLE);
    U(DeleteEnhMetaFile)(emf_end(dc));
}

TEST(emf_worldtransform_roundtrip_on_emf_dc) {
    REQUIRE(U(SetWorldTransform)); REQUIRE(U(GetWorldTransform));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    U(SetGraphicsMode)(dc, GM_ADVANCED);
    XFORM x = {1.5f, 0.25f, -0.5f, 2.0f, 12.0f, -7.0f}, y;
    CHECK(U(SetWorldTransform)(dc, &x));
    memset(&y, 0, sizeof y);
    CHECK(U(GetWorldTransform)(dc, &y));
    CHECK(y.eM11 == x.eM11 && y.eM12 == x.eM12 && y.eM21 == x.eM21 && y.eM22 == x.eM22 && y.eDx == x.eDx && y.eDy == x.eDy);
    U(DeleteEnhMetaFile)(emf_end(dc));
}

/* ---------- frame ---------- */

static void frame_formula_case(int x, int y, int w, int h) {
    HENHMETAFILE m = mk_patblt(x, y, w, h);
    if (!m) { CHECK(m != NULL); return; }
    ENHMETAHEADER hd; get_hdr(m, &hd);
    LONG l = MulDiv(hd.rclBounds.left * 100, hd.szlMillimeters.cx, hd.szlDevice.cx);
    LONG t = MulDiv(hd.rclBounds.top * 100, hd.szlMillimeters.cy, hd.szlDevice.cy);
    LONG r = MulDiv(hd.rclBounds.right * 100, hd.szlMillimeters.cx, hd.szlDevice.cx);
    LONG b = MulDiv(hd.rclBounds.bottom * 100, hd.szlMillimeters.cy, hd.szlDevice.cy);
    LONG tx = hd.szlMillimeters.cx * 100 / hd.szlDevice.cx + 2, ty = hd.szlMillimeters.cy * 100 / hd.szlDevice.cy + 2;
    CHECKF(labs(hd.rclFrame.left - l) <= tx && labs(hd.rclFrame.right - r) <= tx && labs(hd.rclFrame.top - t) <= ty &&
           labs(hd.rclFrame.bottom - b) <= ty,
           "frame [%ld %ld %ld %ld] want [%ld %ld %ld %ld]", hd.rclFrame.left, hd.rclFrame.top, hd.rclFrame.right,
           hd.rclFrame.bottom, l, t, r, b);
    SOFT(hd.rclFrame.left == l && hd.rclFrame.top == t && hd.rclFrame.right == r && hd.rclFrame.bottom == b);
    U(DeleteEnhMetaFile)(m);
}
TEST(emf_frame_from_bounds_a) { REQUIRE(U(GetEnhMetaFileHeader)); frame_formula_case(0, 0, 100, 100); }
TEST(emf_frame_from_bounds_b) { REQUIRE(U(GetEnhMetaFileHeader)); frame_formula_case(10, 20, 300, 200); }
TEST(emf_frame_from_bounds_c) { REQUIRE(U(GetEnhMetaFileHeader)); frame_formula_case(-50, -40, 100, 100); }
TEST(emf_frame_from_bounds_d) { REQUIRE(U(GetEnhMetaFileHeader)); frame_formula_case(1000, 700, 1, 1); }
TEST(emf_frame_from_bounds_e) { REQUIRE(U(GetEnhMetaFileHeader)); frame_formula_case(0, 0, 1920, 1080); }

TEST(emf_frame_explicit_wins_over_bounds) {
    REQUIRE(U(GetEnhMetaFileHeader));
    RECT fr = {100, 200, 3000, 4000};
    HDC dc = U(CreateEnhMetaFileW)(NULL, NULL, &fr, NULL);
    if (!dc) { CHECK(dc != NULL); return; }
    U(PatBlt)(dc, 0, 0, 5000, 5000, BLACKNESS);
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; get_hdr(h, &hd);
    CHECK(!memcmp(&hd.rclFrame, &fr, sizeof fr));
    EXPECT_EQ(hd.rclBounds.right, 4999);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_frame_a_variant_same_as_w) {
    REQUIRE(U(CreateEnhMetaFileA));
    RECT fr = {10, 20, 3000, 4000};
    HDC dc = U(CreateEnhMetaFileA)(NULL, NULL, &fr, NULL);
    if (!dc) { CHECK(dc != NULL); return; }
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; get_hdr(h, &hd);
    CHECK(!memcmp(&hd.rclFrame, &fr, sizeof fr));
    U(DeleteEnhMetaFile)(h);
}

/* ---------- reference device ---------- */

TEST(emf_refdev_screen_header_device) {
    REQUIRE(U(GetDC)); REQUIRE(U(GetDeviceCaps));
    HDC scr = U(GetDC)(NULL);
    HDC dc = U(CreateEnhMetaFileW)(scr, NULL, NULL, NULL);
    if (!dc) { CHECK(dc != NULL); U(ReleaseDC)(NULL, scr); return; }
    U(PatBlt)(dc, 0, 0, 4, 4, BLACKNESS);
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; get_hdr(h, &hd);
    EXPECT_EQ(hd.szlDevice.cx, U(GetDeviceCaps)(scr, HORZRES));
    EXPECT_EQ(hd.szlDevice.cy, U(GetDeviceCaps)(scr, VERTRES));
    EXPECT_EQ(hd.szlMillimeters.cx, U(GetDeviceCaps)(scr, HORZSIZE));
    EXPECT_EQ(hd.szlMillimeters.cy, U(GetDeviceCaps)(scr, VERTSIZE));
    t_note("micrometers %ld x %ld", hd.szlMicrometers.cx, hd.szlMicrometers.cy);
    SOFT(hd.szlMicrometers.cx / 1000 == hd.szlMillimeters.cx);
    SOFT(hd.szlMicrometers.cy / 1000 == hd.szlMillimeters.cy);
    U(DeleteEnhMetaFile)(h);
    U(ReleaseDC)(NULL, scr);
}

TEST(emf_refdev_null_matches_screen) {
    REQUIRE(U(GetDC)); REQUIRE(U(GetDeviceCaps));
    HDC scr = U(GetDC)(NULL);
    HENHMETAFILE h = mk_patblt(0, 0, 4, 4);
    if (!h) { CHECK(h != NULL); U(ReleaseDC)(NULL, scr); return; }
    ENHMETAHEADER hd; get_hdr(h, &hd);
    SOFT(hd.szlDevice.cx == U(GetDeviceCaps)(scr, HORZRES) && hd.szlDevice.cy == U(GetDeviceCaps)(scr, VERTRES));
    SOFT(hd.szlMillimeters.cx == U(GetDeviceCaps)(scr, HORZSIZE) && hd.szlMillimeters.cy == U(GetDeviceCaps)(scr, VERTSIZE));
    CHECK(hd.szlDevice.cx > 0 && hd.szlDevice.cy > 0 && hd.szlMillimeters.cx > 0 && hd.szlMillimeters.cy > 0);
    U(DeleteEnhMetaFile)(h);
    U(ReleaseDC)(NULL, scr);
}

TEST(emf_refdev_memory_dib_header_device) {
    REQUIRE(U(GetDeviceCaps));
    emf_dib d;
    if (!emf_dib_make(&d, 200, 100, 0)) { t_skip("no dib"); return; }
    HDC dc = U(CreateEnhMetaFileW)(d.dc, NULL, NULL, NULL);
    if (!dc) { CHECK(dc != NULL); emf_dib_free(&d); return; }
    U(PatBlt)(dc, 0, 0, 20, 20, BLACKNESS);
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; get_hdr(h, &hd);
    EXPECT_EQ(hd.szlDevice.cx, U(GetDeviceCaps)(d.dc, HORZRES));
    EXPECT_EQ(hd.szlDevice.cy, U(GetDeviceCaps)(d.dc, VERTRES));
    t_note("memory DIB ref: szlDevice %ldx%ld (bitmap 200x100)", hd.szlDevice.cx, hd.szlDevice.cy);
    SOFT(hd.szlMillimeters.cx == U(GetDeviceCaps)(d.dc, HORZSIZE));
    U(DeleteEnhMetaFile)(h);
    emf_dib_free(&d);
}

TEST(emf_refdev_frame_ratio_memory_dc) {
    emf_dib d;
    if (!emf_dib_make(&d, 400, 300, 0)) { t_skip("no dib"); return; }
    HDC dc = U(CreateEnhMetaFileW)(d.dc, NULL, NULL, NULL);
    if (!dc) { CHECK(dc != NULL); emf_dib_free(&d); return; }
    U(PatBlt)(dc, 0, 0, 400, 300, BLACKNESS);
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; get_hdr(h, &hd);
    /* frame is bounds converted via mm/device: right = 399 * 100 * mmx / 400 */
    LONG r = MulDiv(399 * 100, hd.szlMillimeters.cx, hd.szlDevice.cx);
    CHECK(labs(hd.rclFrame.right - r) <= hd.szlMillimeters.cx * 100 / hd.szlDevice.cx + 2);
    EXPECT_EQ(hd.rclFrame.left, 0);
    U(DeleteEnhMetaFile)(h);
    emf_dib_free(&d);
}

TEST(emf_refdev_different_devices_different_frames) {
    emf_dib a, b;
    if (!emf_dib_make(&a, 100, 100, 0) || !emf_dib_make(&b, 400, 400, 0)) { t_skip("no dib"); return; }
    HENHMETAFILE hs[2]; HDC refs[2] = { a.dc, b.dc };
    for (int i = 0; i < 2; i++) {
        HDC dc = U(CreateEnhMetaFileW)(refs[i], NULL, NULL, NULL);
        if (!dc) { CHECK(dc != NULL); hs[i] = NULL; continue; }
        U(PatBlt)(dc, 0, 0, 50, 50, BLACKNESS);
        hs[i] = emf_end(dc);
    }
    if (hs[0] && hs[1]) {
        ENHMETAHEADER h0, h1; get_hdr(hs[0], &h0); get_hdr(hs[1], &h1);
        if (h0.szlDevice.cx != h1.szlDevice.cx) {
            /* same pixel bounds on a smaller device = bigger physical frame */
            CHECK(h0.rclFrame.right > h1.rclFrame.right);
        } else t_note("memory DCs report identical device sizes (%ld)", h0.szlDevice.cx);
    }
    for (int i = 0; i < 2; i++) if (hs[i]) U(DeleteEnhMetaFile)(hs[i]);
    emf_dib_free(&a); emf_dib_free(&b);
}

/* ---------- description ---------- */

static HENHMETAFILE mk_desc_w(const WCHAR *d) {
    HDC dc = U(CreateEnhMetaFileW)(NULL, NULL, NULL, d);
    if (!dc) return NULL;
    U(PatBlt)(dc, 0, 0, 3, 3, BLACKNESS);
    return emf_end(dc);
}

TEST(emf_desc_none) {
    REQUIRE(U(GetEnhMetaFileDescriptionW));
    HENHMETAFILE h = mk_desc_w(NULL);
    if (!h) { CHECK(h != NULL); return; }
    ENHMETAHEADER hd; get_hdr(h, &hd);
    EXPECT_EQ(hd.nDescription, 0);
    WCHAR buf[8] = {1};
    UINT r = U(GetEnhMetaFileDescriptionW)(h, 8, buf);
    CHECKF(r == 0 || r == 0xFFFFFFFF, "got %u", r);
    UINT q = U(GetEnhMetaFileDescriptionW)(h, 0, NULL);
    CHECKF(q == 0 || q == 0xFFFFFFFF, "query got %u", q);
    UINT qa = U(GetEnhMetaFileDescriptionA)(h, 0, NULL);
    CHECKF(qa == 0 || qa == 0xFFFFFFFF, "query A got %u", qa);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_desc_two_strings) {
    REQUIRE(U(GetEnhMetaFileDescriptionW));
    static const WCHAR d[] = L"App\0Pic\0";
    HENHMETAFILE h = mk_desc_w(d);
    if (!h) { CHECK(h != NULL); return; }
    ENHMETAHEADER hd; get_hdr(h, &hd);
    t_note("nDescription=%lu offDescription=%lu", hd.nDescription, hd.offDescription);
    CHECK(hd.nDescription >= 8);
    SOFT(hd.nDescription == 9);
    CHECK(hd.offDescription >= 100);
    UINT q = U(GetEnhMetaFileDescriptionW)(h, 0, NULL);
    EXPECT_EQ(q, hd.nDescription);
    WCHAR buf[64]; memset(buf, 0xAA, sizeof buf);
    UINT r = U(GetEnhMetaFileDescriptionW)(h, 64, buf);
    EXPECT_EQ(r, hd.nDescription);
    CHECK(!memcmp(buf, d, 8 * sizeof(WCHAR)));
    EXPECT_EQ(buf[r], 0xAAAA);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_desc_buffer_boundaries_w) {
    REQUIRE(U(GetEnhMetaFileDescriptionW));
    static const WCHAR d[] = L"App\0Pic\0";
    HENHMETAFILE h = mk_desc_w(d);
    if (!h) { CHECK(h != NULL); return; }
    ENHMETAHEADER hd; get_hdr(h, &hd);
    UINT n = hd.nDescription;
    WCHAR buf[64];
    static const int sizes[] = { 0, 1, 2, 3, 4, 5, 8 };
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        memset(buf, 0xAA, sizeof buf);
        UINT r = U(GetEnhMetaFileDescriptionW)(h, sizes[i], buf);
        UINT want = (UINT)sizes[i] < n ? (UINT)sizes[i] : n;
        CHECKF(r == want, "cch=%d got %u want %u", sizes[i], r, want);
        CHECKF(buf[want] == 0xAAAA, "wrote past count (cch=%d)", sizes[i]);
        CHECKF(!memcmp(buf, d, want * sizeof(WCHAR)), "content cch=%d", sizes[i]);
    }
    /* exactly n and n-1 */
    memset(buf, 0xAA, sizeof buf);
    EXPECT_EQ(U(GetEnhMetaFileDescriptionW)(h, n, buf), n);
    memset(buf, 0xAA, sizeof buf);
    EXPECT_EQ(U(GetEnhMetaFileDescriptionW)(h, n - 1, buf), n - 1);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_desc_a_variant) {
    REQUIRE(U(GetEnhMetaFileDescriptionA));
    static const WCHAR d[] = L"App\0Pic\0";
    HENHMETAFILE h = mk_desc_w(d);
    if (!h) { CHECK(h != NULL); return; }
    ENHMETAHEADER hd; get_hdr(h, &hd);
    UINT q = U(GetEnhMetaFileDescriptionA)(h, 0, NULL);
    EXPECT_EQ(q, hd.nDescription);
    char buf[64]; memset(buf, 0xAA, sizeof buf);
    UINT r = U(GetEnhMetaFileDescriptionA)(h, 64, buf);
    EXPECT_EQ(r, hd.nDescription);
    CHECK(!memcmp(buf, "App\0Pic\0", 8));
    EXPECT_EQ((unsigned char)buf[r], 0xAA);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_desc_a_buffer_too_small) {
    REQUIRE(U(GetEnhMetaFileDescriptionA));
    static const WCHAR d[] = L"App\0Pic\0";
    HENHMETAFILE h = mk_desc_w(d);
    if (!h) { CHECK(h != NULL); return; }
    ENHMETAHEADER hd; get_hdr(h, &hd);
    char buf[64];
    /* a too-small ANSI buffer fails outright rather than truncating */
    memset(buf, 0xAA, sizeof buf);
    UINT r = U(GetEnhMetaFileDescriptionA)(h, 4, buf);
    CHECKF(r == 0xFFFFFFFF, "got %u", r);
    memset(buf, 0xAA, sizeof buf);
    r = U(GetEnhMetaFileDescriptionA)(h, hd.nDescription, buf);
    EXPECT_EQ(r, hd.nDescription);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_desc_created_via_a) {
    REQUIRE(U(CreateEnhMetaFileA));
    HDC dc = U(CreateEnhMetaFileA)(NULL, NULL, NULL, "Alpha\0Beta\0");
    if (!dc) { CHECK(dc != NULL); return; }
    HENHMETAFILE h = emf_end(dc);
    WCHAR buf[64]; memset(buf, 0, sizeof buf);
    UINT r = U(GetEnhMetaFileDescriptionW)(h, 64, buf);
    CHECK(r >= 11);
    CHECK(!lstrcmpW(buf, L"Alpha"));
    CHECK(!lstrcmpW(buf + 6, L"Beta"));
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_desc_long_string) {
    REQUIRE(U(GetEnhMetaFileDescriptionW));
    WCHAR d[1000]; int n = 0;
    for (int i = 0; i < 400; i++) d[n++] = L'a' + (i % 26);
    d[n++] = 0;
    for (int i = 0; i < 400; i++) d[n++] = L'A' + (i % 26);
    d[n++] = 0; d[n++] = 0;
    HENHMETAFILE h = mk_desc_w(d);
    if (!h) { CHECK(h != NULL); return; }
    ENHMETAHEADER hd; get_hdr(h, &hd);
    CHECK(hd.nDescription >= 801);
    WCHAR out[1000]; memset(out, 0, sizeof out);
    UINT r = U(GetEnhMetaFileDescriptionW)(h, 1000, out);
    EXPECT_EQ(r, hd.nDescription);
    CHECK(!memcmp(out, d, 801 * sizeof(WCHAR)));
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_desc_invalid_handles) {
    REQUIRE(U(GetEnhMetaFileDescriptionW)); REQUIRE(U(GetEnhMetaFileDescriptionA));
    WCHAR w[4]; char a[4];
    HENHMETAFILE bad[] = { NULL, (HENHMETAFILE)1, (HENHMETAFILE)0xdeadbeef };
    for (unsigned i = 0; i < 3; i++) {
        CHECK(U(GetEnhMetaFileDescriptionW)(bad[i], 4, w) == 0xFFFFFFFF);
        CHECK(U(GetEnhMetaFileDescriptionW)(bad[i], 0, NULL) == 0xFFFFFFFF);
        CHECK(U(GetEnhMetaFileDescriptionA)(bad[i], 4, a) == 0xFFFFFFFF);
        CHECK(U(GetEnhMetaFileDescriptionA)(bad[i], 0, NULL) == 0xFFFFFFFF);
    }
}

TEST(emf_desc_deleted_handle) {
    REQUIRE(U(GetEnhMetaFileDescriptionW));
    HENHMETAFILE h = mk_desc_w(L"x\0y\0");
    if (!h) { CHECK(h != NULL); return; }
    U(DeleteEnhMetaFile)(h);
    CHECK(U(GetEnhMetaFileDescriptionW)(h, 0, NULL) == 0xFFFFFFFF);
}

TEST(emf_desc_single_string_with_double_nul) {
    REQUIRE(U(GetEnhMetaFileDescriptionW));
    static const WCHAR d[] = L"Solo\0";   /* literal contributes a second NUL */
    HENHMETAFILE h = mk_desc_w(d);
    if (!h) { CHECK(h != NULL); return; }
    ENHMETAHEADER hd; get_hdr(h, &hd);
    t_note("single string nDescription=%lu", hd.nDescription);
    CHECK(hd.nDescription >= 5);
    WCHAR buf[16] = {0};
    UINT r = U(GetEnhMetaFileDescriptionW)(h, 16, buf);
    EXPECT_EQ(r, hd.nDescription);
    CHECK(!lstrcmpW(buf, L"Solo"));
    U(DeleteEnhMetaFile)(h);
}

/* ---------- palette entries / pixel format ---------- */

TEST(emf_pal_none) {
    REQUIRE(U(GetEnhMetaFilePaletteEntries));
    HENHMETAFILE h = mk_patblt(0, 0, 4, 4);
    if (!h) { CHECK(h != NULL); return; }
    ENHMETAHEADER hd; get_hdr(h, &hd);
    EXPECT_EQ(hd.nPalEntries, 0);
    EXPECT_EQ(U(GetEnhMetaFilePaletteEntries)(h, 0, NULL), 0);
    PALETTEENTRY pe[4];
    EXPECT_EQ(U(GetEnhMetaFilePaletteEntries)(h, 4, pe), 0);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_pal_invalid_handles) {
    REQUIRE(U(GetEnhMetaFilePaletteEntries));
    PALETTEENTRY pe[2];
    HENHMETAFILE bad[] = { NULL, (HENHMETAFILE)1, (HENHMETAFILE)0xdeadbeef };
    for (unsigned i = 0; i < 3; i++) {
        CHECK(U(GetEnhMetaFilePaletteEntries)(bad[i], 0, NULL) == 0xFFFFFFFF);
        CHECK(U(GetEnhMetaFilePaletteEntries)(bad[i], 2, pe) == 0xFFFFFFFF);
    }
}

TEST(emf_pal_selected_palette) {
    REQUIRE(U(GetEnhMetaFilePaletteEntries)); REQUIRE(U(CreatePalette)); REQUIRE(U(SelectPalette)); REQUIRE(U(RealizePalette));
    struct { WORD ver, n; PALETTEENTRY e[4]; } lp = { 0x300, 4, { {10, 20, 30, 0}, {40, 50, 60, 0}, {70, 80, 90, 0}, {100, 110, 120, 0} } };
    HPALETTE pal = U(CreatePalette)((LOGPALETTE *)&lp);
    HDC dc = emf_begin();
    if (!pal || !dc) { CHECK(pal && dc); if (pal) U(DeleteObject)(pal); if (dc) U(DeleteEnhMetaFile)(emf_end(dc)); return; }
    HPALETTE old = U(SelectPalette)(dc, pal, FALSE);
    U(RealizePalette)(dc);
    U(PatBlt)(dc, 0, 0, 5, 5, BLACKNESS);
    U(SelectPalette)(dc, old, FALSE);
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; get_hdr(h, &hd);
    UINT cnt = U(GetEnhMetaFilePaletteEntries)(h, 0, NULL);
    EXPECT_EQ(cnt, hd.nPalEntries);
    t_note("palette entries in emf: %u", cnt);
    if (cnt) {
        PALETTEENTRY pe[8]; memset(pe, 0, sizeof pe);
        UINT got = U(GetEnhMetaFilePaletteEntries)(h, 8, pe);
        EXPECT_EQ(got, cnt);
        UINT part = U(GetEnhMetaFilePaletteEntries)(h, 2, pe);
        EXPECT_EQ(part, 2);
    }
    U(DeleteEnhMetaFile)(h);
    U(DeleteObject)(pal);
}

TEST(emf_pixfmt_none) {
    REQUIRE(U(GetEnhMetaFilePixelFormat));
    HENHMETAFILE h = mk_patblt(0, 0, 4, 4);
    if (!h) { CHECK(h != NULL); return; }
    PIXELFORMATDESCRIPTOR pfd; memset(&pfd, 0xCC, sizeof pfd);
    EXPECT_EQ(U(GetEnhMetaFilePixelFormat)(h, sizeof pfd, &pfd), 0);
    EXPECT_EQ(U(GetEnhMetaFilePixelFormat)(h, 0, NULL), 0);
    EXPECT_EQ(((BYTE *)&pfd)[0], 0xCC);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_pixfmt_invalid_handle) {
    REQUIRE(U(GetEnhMetaFilePixelFormat));
    PIXELFORMATDESCRIPTOR pfd;
    HENHMETAFILE bad[] = { NULL, (HENHMETAFILE)1, (HENHMETAFILE)0xdeadbeef };
    for (unsigned i = 0; i < 3; i++) {
        SetLastError(0);
        CHECK(U(GetEnhMetaFilePixelFormat)(bad[i], sizeof pfd, &pfd) == 0xFFFFFFFF);
        EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    }
}

TEST(emf_pixfmt_old_88_byte_header) {
    REQUIRE(U(GetEnhMetaFilePixelFormat)); REQUIRE(U(SetEnhMetaFileBits));
    BYTE buf[128]; UINT n = emf_build_min(buf, 88);
    HENHMETAFILE h = U(SetEnhMetaFileBits)(n, buf);
    if (!h) { CHECK(h != NULL); return; }
    SetLastError(0);
    UINT r = U(GetEnhMetaFilePixelFormat)(h, 0, NULL);
    CHECK(r == 0xFFFFFFFF);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    U(DeleteEnhMetaFile)(h);
}
