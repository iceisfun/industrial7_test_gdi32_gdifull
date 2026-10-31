/* CreateBitmapIndirect: validation of BITMAP fields, data copy, pitch rules, lifetime. */
#include "gap10.h"

static void bm_init(BITMAP *b, int w, int h, int planes, int bpp, int wb, const void *bits) {
    memset(b, 0, sizeof *b);
    b->bmType = 0; b->bmWidth = w; b->bmHeight = h; b->bmPlanes = (WORD)planes;
    b->bmBitsPixel = (WORD)bpp; b->bmWidthBytes = wb; b->bmBits = (LPVOID)bits;
}
static int pitch_of(int w, int bpp) { return ((bpp * w + 15) >> 3) & ~1; }

#define NEED_BI() do { REQUIRE(U(CreateBitmapIndirect)); REQUIRE(U(GetObjectW)); REQUIRE(U(DeleteObject)); } while (0)

TEST(gap10_cbi_mono_basic) {
    NEED_BI();
    BITMAP in, out; HBITMAP h;
    bm_init(&in, 10, 5, 1, 1, 2, NULL);
    h = U(CreateBitmapIndirect)(&in);
    CHECK(h != NULL);
    if (!h) return;
    memset(&out, 0xCC, sizeof out);
    EXPECT_EQ(U(GetObjectW)(h, sizeof out, &out), sizeof(BITMAP));
    EXPECT_EQ(out.bmType, 0);
    EXPECT_EQ(out.bmWidth, 10); EXPECT_EQ(out.bmHeight, 5);
    EXPECT_EQ(out.bmPlanes, 1); EXPECT_EQ(out.bmBitsPixel, 1);
    EXPECT_EQ(out.bmWidthBytes, 2);
    CHECK(U(DeleteObject)(h));
}
TEST(gap10_cbi_bmtype_is_ignored) {
    NEED_BI();
    BITMAP in, out; HBITMAP h;
    int types[] = { 0, 1, 0x4D42, 0x7FFF, -1 };
    unsigned i;
    for (i = 0; i < sizeof types / sizeof *types; i++) {
        bm_init(&in, 8, 8, 1, 1, 2, NULL);
        in.bmType = types[i];
        h = U(CreateBitmapIndirect)(&in);
        CHECKF(h != NULL, "bmType %d", types[i]);
        if (!h) continue;
        memset(&out, 0xCC, sizeof out);
        U(GetObjectW)(h, sizeof out, &out);
        CHECKF(out.bmType == 0, "bmType out %d", out.bmType);
        U(DeleteObject)(h);
    }
}
TEST(gap10_cbi_32bpp_pitch) {
    NEED_BI();
    int ws[] = { 1, 2, 3, 7, 16, 31, 64 }; unsigned i;
    for (i = 0; i < sizeof ws / sizeof *ws; i++) {
        BITMAP in, out; HBITMAP h;
        bm_init(&in, ws[i], 3, 1, 32, pitch_of(ws[i], 32), NULL);
        h = U(CreateBitmapIndirect)(&in);
        CHECKF(h != NULL, "w=%d", ws[i]);
        if (!h) continue;
        memset(&out, 0, sizeof out);
        U(GetObjectW)(h, sizeof out, &out);
        CHECKF(out.bmBitsPixel == 32, "bpp %d", out.bmBitsPixel);
        CHECKF(out.bmWidthBytes == ws[i] * 4, "w=%d wb=%d", ws[i], out.bmWidthBytes);
        CHECKF(out.bmWidth == ws[i] && out.bmHeight == 3, "dims");
        U(DeleteObject)(h);
    }
}
TEST(gap10_cbi_mono_pitch_table) {
    NEED_BI();
    int ws[] = { 1, 8, 15, 16, 17, 31, 32, 33, 100 }; unsigned i;
    for (i = 0; i < sizeof ws / sizeof *ws; i++) {
        BITMAP in, out; HBITMAP h;
        int p = pitch_of(ws[i], 1);
        bm_init(&in, ws[i], 2, 1, 1, p, NULL);
        h = U(CreateBitmapIndirect)(&in);
        CHECKF(h != NULL, "w=%d", ws[i]);
        if (!h) continue;
        memset(&out, 0, sizeof out);
        U(GetObjectW)(h, sizeof out, &out);
        CHECKF(out.bmWidthBytes == p, "w=%d wb=%d want %d", ws[i], out.bmWidthBytes, p);
        U(DeleteObject)(h);
    }
}
TEST(gap10_cbi_other_depths_observed) {
    NEED_BI();
    int bpps[] = { 4, 8, 16, 24 }; unsigned i;
    for (i = 0; i < sizeof bpps / sizeof *bpps; i++) {
        BITMAP in, out; HBITMAP h;
        bm_init(&in, 6, 4, 1, bpps[i], pitch_of(6, bpps[i]), NULL);
        CLR();
        h = U(CreateBitmapIndirect)(&in);
        if (!h) { t_note("bpp %d: failed, err %lu", bpps[i], GetLastError()); continue; }
        memset(&out, 0, sizeof out);
        U(GetObjectW)(h, sizeof out, &out);
        t_note("bpp %d -> bitsPixel %d widthBytes %d", bpps[i], out.bmBitsPixel, out.bmWidthBytes);
        CHECK(out.bmWidth == 6 && out.bmHeight == 4);
        U(DeleteObject)(h);
    }
}
/* ---- validation (decomp-verified: pitch must be even, nonzero, >= natural, planes == 1) ---- */
TEST(gap10_cbi_zero_widthbytes_fails) {
    NEED_BI();
    BITMAP in; HBITMAP h;
    bm_init(&in, 8, 8, 1, 1, 0, NULL);
    CLR();
    h = U(CreateBitmapIndirect)(&in);
    CHECK(h == NULL);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    if (h) U(DeleteObject)(h);
}
TEST(gap10_cbi_odd_widthbytes_fails) {
    NEED_BI();
    int wbs[] = { 1, 3, 5, 7, 9, 33 }; unsigned i;
    for (i = 0; i < sizeof wbs / sizeof *wbs; i++) {
        BITMAP in; HBITMAP h;
        bm_init(&in, 8, 8, 1, 1, wbs[i], NULL);
        CLR();
        h = U(CreateBitmapIndirect)(&in);
        CHECKF(h == NULL, "wb=%d", wbs[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "wb=%d err %lu", wbs[i], GetLastError());
        if (h) U(DeleteObject)(h);
    }
}
TEST(gap10_cbi_widthbytes_too_small_fails) {
    NEED_BI();
    BITMAP in; HBITMAP h;
    bm_init(&in, 32, 4, 1, 1, 2, NULL);   /* needs 4 */
    CLR();
    h = U(CreateBitmapIndirect)(&in);
    CHECK(h == NULL);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    if (h) U(DeleteObject)(h);
    bm_init(&in, 4, 4, 1, 32, 12, NULL);  /* needs 16 */
    CLR();
    h = U(CreateBitmapIndirect)(&in);
    CHECK(h == NULL);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    if (h) U(DeleteObject)(h);
}
TEST(gap10_cbi_negative_widthbytes_fails) {
    NEED_BI();
    BITMAP in; HBITMAP h;
    bm_init(&in, 8, 8, 1, 1, -2, NULL);
    CLR();
    h = U(CreateBitmapIndirect)(&in);
    CHECK(h == NULL);
    if (h) U(DeleteObject)(h);
}
TEST(gap10_cbi_planes_must_be_one) {
    NEED_BI();
    int planes[] = { 0, 2, 3, 4, 255, 65535 }; unsigned i;
    for (i = 0; i < sizeof planes / sizeof *planes; i++) {
        BITMAP in; HBITMAP h;
        bm_init(&in, 8, 8, planes[i], 1, 2, NULL);
        CLR();
        h = U(CreateBitmapIndirect)(&in);
        CHECKF(h == NULL, "planes=%d", planes[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "planes=%d err %lu", planes[i], GetLastError());
        if (h) U(DeleteObject)(h);
    }
}
TEST(gap10_cbi_huge_width_fails) {
    NEED_BI();
    BITMAP in; HBITMAP h;
    /* width > 0x7ffffff0 / bpp is rejected before any allocation */
    bm_init(&in, 0x4000000, 1, 1, 32, 0x7ffffffe, NULL);
    CLR();
    h = U(CreateBitmapIndirect)(&in);
    CHECK(h == NULL);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    if (h) U(DeleteObject)(h);
    bm_init(&in, INT_MAX, 1, 1, 1, 2, NULL);
    CLR();
    h = U(CreateBitmapIndirect)(&in);
    CHECK(h == NULL);
    if (h) U(DeleteObject)(h);
    bm_init(&in, INT_MAX, INT_MAX, 1, 16, 2, NULL);
    h = U(CreateBitmapIndirect)(&in);
    CHECK(h == NULL);
    if (h) U(DeleteObject)(h);
}
TEST(gap10_cbi_huge_height_fails_cleanly) {
    NEED_BI();
    BITMAP in; HBITMAP h;
    bm_init(&in, 1, INT_MAX, 1, 1, 2, NULL);
    CLR();
    h = U(CreateBitmapIndirect)(&in);
    CHECK(h == NULL);
    t_note("height INT_MAX: err %lu", GetLastError());
    if (h) U(DeleteObject)(h);
    bm_init(&in, 30000, 30000, 1, 32, 120000, NULL);   /* ~3.6 GB */
    h = U(CreateBitmapIndirect)(&in);
    CHECK(h == NULL);
    if (h) U(DeleteObject)(h);
}
TEST(gap10_cbi_zero_and_negative_dims_observed) {
    NEED_BI();
    int dims[][2] = { {0,0}, {0,5}, {5,0}, {-1,5}, {5,-1}, {-3,-3} }; unsigned i;
    for (i = 0; i < sizeof dims / sizeof *dims; i++) {
        BITMAP in, out; HBITMAP h;
        bm_init(&in, dims[i][0], dims[i][1], 1, 1, 2, NULL);
        CLR();
        h = U(CreateBitmapIndirect)(&in);
        if (!h) { t_note("%dx%d: NULL err %lu", dims[i][0], dims[i][1], GetLastError()); continue; }
        memset(&out, 0, sizeof out);
        U(GetObjectW)(h, sizeof out, &out);
        t_note("%dx%d: got %dx%d bpp %d", dims[i][0], dims[i][1], out.bmWidth, out.bmHeight, out.bmBitsPixel);
        U(DeleteObject)(h);
    }
}
/* ---- bits copying ---- */
TEST(gap10_cbi_bits_copied_exact) {
    NEED_BI(); REQUIRE(U(GetBitmapBits));
    BYTE src[8] = { 0xA5, 0x5A, 0xFF, 0x00, 0x0F, 0xF0, 0x81, 0x18 }, got[8];
    BITMAP in; HBITMAP h;
    bm_init(&in, 16, 4, 1, 1, 2, src);
    h = U(CreateBitmapIndirect)(&in);
    CHECK(h != NULL);
    if (!h) return;
    memset(got, 0xEE, sizeof got);
    EXPECT_EQ(U(GetBitmapBits)(h, 8, got), 8);
    CHECK(!memcmp(src, got, 8));
    U(DeleteObject)(h);
}
TEST(gap10_cbi_bits_source_stride_larger) {
    NEED_BI(); REQUIRE(U(GetBitmapBits));
    /* source rows are 8 bytes apart, bitmap rows are 2 bytes */
    BYTE src[32], got[8]; BITMAP in; HBITMAP h; int i;
    memset(src, 0x77, sizeof src);
    for (i = 0; i < 4; i++) { src[i * 8] = (BYTE)(0x10 + i); src[i * 8 + 1] = (BYTE)(0x20 + i); }
    bm_init(&in, 16, 4, 1, 1, 8, src);
    h = U(CreateBitmapIndirect)(&in);
    CHECK(h != NULL);
    if (!h) return;
    memset(got, 0, sizeof got);
    EXPECT_EQ(U(GetBitmapBits)(h, 8, got), 8);
    for (i = 0; i < 4; i++) {
        CHECKF(got[i * 2] == 0x10 + i, "row %d byte0 %02x", i, got[i * 2]);
        CHECKF(got[i * 2 + 1] == 0x20 + i, "row %d byte1 %02x", i, got[i * 2 + 1]);
    }
    U(DeleteObject)(h);
}
TEST(gap10_cbi_bits_snapshot_not_alias) {
    NEED_BI(); REQUIRE(U(GetBitmapBits));
    BYTE src[4] = { 0xAA, 0xAA, 0x55, 0x55 }, got[4];
    BITMAP in; HBITMAP h;
    bm_init(&in, 16, 2, 1, 1, 2, src);
    h = U(CreateBitmapIndirect)(&in);
    CHECK(h != NULL);
    if (!h) return;
    memset(src, 0, sizeof src);
    memset(got, 0, sizeof got);
    U(GetBitmapBits)(h, 4, got);
    EXPECT_EQ(got[0], 0xAA); EXPECT_EQ(got[2], 0x55);
    U(DeleteObject)(h);
}
TEST(gap10_cbi_bits_32bpp_pixels) {
    NEED_BI(); NEED_MD();
    DWORD src[6] = { 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00FFFFFF, 0x00000000, 0x00123456 };
    BITMAP in; HBITMAP h; HDC dc; HGDIOBJ old; int i;
    bm_init(&in, 3, 2, 1, 32, 12, src);
    h = U(CreateBitmapIndirect)(&in);
    CHECK(h != NULL);
    if (!h) return;
    dc = U(CreateCompatibleDC)(NULL);
    old = U(SelectObject)(dc, h);
    for (i = 0; i < 6; i++) {
        COLORREF want = ((src[i] & 0xFF) << 16) | (src[i] & 0xFF00) | ((src[i] >> 16) & 0xFF);
        COLORREF got = U(GetPixel)(dc, i % 3, i / 3);
        CHECKF(got == want, "px %d got %06lx want %06lx", i, got, want);
    }
    U(SelectObject)(dc, old);
    U(DeleteDC)(dc);
    U(DeleteObject)(h);
}
TEST(gap10_cbi_mono_pixels_via_dc) {
    NEED_BI(); NEED_MD();
    BYTE src[4] = { 0xA0, 0x00, 0x5F, 0xFF };   /* row0: 1010 0000..., row1: 0101 1111... */
    BITMAP in; HBITMAP h; HDC dc; HGDIOBJ old;
    bm_init(&in, 8, 2, 1, 1, 2, src);
    h = U(CreateBitmapIndirect)(&in);
    CHECK(h != NULL);
    if (!h) return;
    dc = U(CreateCompatibleDC)(NULL);
    old = U(SelectObject)(dc, h);
    /* default DC colours: bit 0 -> black, bit 1 -> white */
    EXPECT_EQ(U(GetPixel)(dc, 0, 0), WHITE_);
    EXPECT_EQ(U(GetPixel)(dc, 1, 0), BLACK_);
    EXPECT_EQ(U(GetPixel)(dc, 2, 0), WHITE_);
    EXPECT_EQ(U(GetPixel)(dc, 3, 0), BLACK_);
    EXPECT_EQ(U(GetPixel)(dc, 7, 0), BLACK_);
    EXPECT_EQ(U(GetPixel)(dc, 0, 1), BLACK_);
    EXPECT_EQ(U(GetPixel)(dc, 1, 1), WHITE_);
    EXPECT_EQ(U(GetPixel)(dc, 7, 1), WHITE_);
    U(SelectObject)(dc, old);
    U(DeleteDC)(dc);
    U(DeleteObject)(h);
}
/* ---- parity with CreateBitmap ---- */
TEST(gap10_cbi_matches_createbitmap) {
    NEED_BI(); REQUIRE(U(CreateBitmap)); REQUIRE(U(GetBitmapBits));
    BYTE src[8] = { 1, 2, 3, 4, 5, 6, 7, 8 }, a[8], b[8];
    BITMAP in, oa, ob; HBITMAP h1, h2;
    bm_init(&in, 16, 4, 1, 1, 2, src);
    h1 = U(CreateBitmapIndirect)(&in);
    h2 = U(CreateBitmap)(16, 4, 1, 1, src);
    CHECK(h1 && h2);
    if (h1 && h2) {
        memset(&oa, 1, sizeof oa); memset(&ob, 2, sizeof ob);
        U(GetObjectW)(h1, sizeof oa, &oa); U(GetObjectW)(h2, sizeof ob, &ob);
        CHECK(!memcmp(&oa, &ob, offsetof(BITMAP, bmBits)));
        memset(a, 0, 8); memset(b, 0xFF, 8);
        U(GetBitmapBits)(h1, 8, a); U(GetBitmapBits)(h2, 8, b);
        CHECK(!memcmp(a, b, 8));
        CHECK(h1 != h2);
    }
    if (h1) U(DeleteObject)(h1);
    if (h2) U(DeleteObject)(h2);
}
TEST(gap10_cbi_distinct_handles) {
    NEED_BI();
    BITMAP in; HBITMAP h[16]; int i, j, ok = 1;
    bm_init(&in, 4, 4, 1, 1, 2, NULL);
    for (i = 0; i < 16; i++) h[i] = U(CreateBitmapIndirect)(&in);
    for (i = 0; i < 16; i++) { if (!h[i]) ok = 0; for (j = 0; j < i; j++) if (h[i] == h[j]) ok = 0; }
    CHECK(ok);
    for (i = 0; i < 16; i++) if (h[i]) U(DeleteObject)(h[i]);
}
TEST(gap10_cbi_object_type_is_bitmap) {
    NEED_BI(); REQUIRE(U(GetObjectType));
    BITMAP in; HBITMAP h;
    bm_init(&in, 4, 4, 1, 1, 2, NULL);
    h = U(CreateBitmapIndirect)(&in);
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(U(GetObjectType)(h), OBJ_BITMAP);
    U(DeleteObject)(h);
}
TEST(gap10_cbi_delete_lifetime) {
    NEED_BI(); REQUIRE(U(GetObjectType));
    BITMAP in, out; HBITMAP h;
    bm_init(&in, 4, 4, 1, 1, 2, NULL);
    h = U(CreateBitmapIndirect)(&in);
    CHECK(h != NULL);
    if (!h) return;
    CHECK(U(DeleteObject)(h));
    CLR();
    CHECK(!U(DeleteObject)(h));
    EXPECT_EQ(U(GetObjectW)(h, sizeof out, &out), 0);
    EXPECT_EQ(U(GetObjectType)(h), 0);
}
TEST(gap10_cbi_selected_delete_observed) {
    NEED_BI(); NEED_MD();
    BITMAP in; HBITMAP h; HDC dc; HGDIOBJ old; BOOL r;
    bm_init(&in, 4, 4, 1, 1, 2, NULL);
    h = U(CreateBitmapIndirect)(&in);
    CHECK(h != NULL);
    if (!h) return;
    dc = U(CreateCompatibleDC)(NULL);
    old = U(SelectObject)(dc, h);
    CHECK(old != NULL);
    r = U(DeleteObject)(h);
    t_note("DeleteObject on selected bitmap -> %d", r);
    U(SelectObject)(dc, old);
    if (!r) CHECK(U(DeleteObject)(h));
    U(DeleteDC)(dc);
}
TEST(gap10_cbi_select_into_two_dcs_fails) {
    NEED_BI(); NEED_MD();
    BITMAP in; HBITMAP h; HDC d1, d2; HGDIOBJ o1, o2;
    bm_init(&in, 4, 4, 1, 1, 2, NULL);
    h = U(CreateBitmapIndirect)(&in);
    CHECK(h != NULL);
    if (!h) return;
    d1 = U(CreateCompatibleDC)(NULL); d2 = U(CreateCompatibleDC)(NULL);
    o1 = U(SelectObject)(d1, h);
    CHECK(o1 != NULL);
    o2 = U(SelectObject)(d2, h);
    CHECK(o2 == NULL);   /* a bitmap may be selected into one DC at a time */
    U(SelectObject)(d1, o1);
    o2 = U(SelectObject)(d2, h);
    CHECK(o2 != NULL);
    U(SelectObject)(d2, o2);
    U(DeleteDC)(d1); U(DeleteDC)(d2);
    U(DeleteObject)(h);
}
TEST(gap10_cbi_no_gdi_leak) {
    NEED_BI();
    if (!g10_gdicount()) { t_skip("no GetGuiResources"); return; }
    DWORD before = g10_gdicount(), mid; int i; BITMAP in; HBITMAP h[200];
    bm_init(&in, 8, 8, 1, 1, 2, NULL);
    for (i = 0; i < 200; i++) h[i] = U(CreateBitmapIndirect)(&in);
    mid = g10_gdicount();
    CHECKF(mid >= before + 190, "count %lu -> %lu", before, mid);
    for (i = 0; i < 200; i++) if (h[i]) U(DeleteObject)(h[i]);
    EXPECT_EQ(g10_gdicount(), before);
}
TEST(gap10_cbi_failure_no_gdi_leak) {
    NEED_BI();
    if (!g10_gdicount()) { t_skip("no GetGuiResources"); return; }
    DWORD before = g10_gdicount(); int i; BITMAP in;
    bm_init(&in, 8, 8, 2, 1, 2, NULL);
    for (i = 0; i < 100; i++) U(CreateBitmapIndirect)(&in);
    EXPECT_EQ(g10_gdicount(), before);
}
static void cbi_thread(void *p) {
    BITMAP in; bm_init(&in, 5, 6, 1, 1, 2, NULL);
    *(HBITMAP *)p = U(CreateBitmapIndirect)(&in);
}
TEST(gap10_cbi_cross_thread_use) {
    NEED_BI();
    HBITMAP h = NULL; BITMAP out;
    g10_run_thread(cbi_thread, &h);
    CHECK(h != NULL);
    if (!h) return;
    memset(&out, 0, sizeof out);
    EXPECT_EQ(U(GetObjectW)(h, sizeof out, &out), sizeof(BITMAP));
    EXPECT_EQ(out.bmWidth, 5); EXPECT_EQ(out.bmHeight, 6);
    CHECK(U(DeleteObject)(h));
}
TEST(gap10_cbi_dib_section_not_created) {
    NEED_BI();
    BITMAP in, out; HBITMAP h;
    bm_init(&in, 4, 4, 1, 32, 16, NULL);
    h = U(CreateBitmapIndirect)(&in);
    CHECK(h != NULL);
    if (!h) return;
    memset(&out, 0, sizeof out);
    U(GetObjectW)(h, sizeof out, &out);
    CHECK(out.bmBits == NULL);      /* DDB, not a DIB section */
    DIBSECTION ds;
    EXPECT_EQ(U(GetObjectW)(h, sizeof ds, &ds), sizeof(BITMAP));
    U(DeleteObject)(h);
}
TEST(gap10_cbi_getobject_buffer_sizes) {
    NEED_BI();
    BITMAP in, out; HBITMAP h; BYTE big[128];
    bm_init(&in, 4, 4, 1, 1, 2, NULL);
    h = U(CreateBitmapIndirect)(&in);
    CHECK(h != NULL);
    if (!h) return;
    t_note("GetObject(h,0,NULL) -> %d", U(GetObjectW)(h, 0, NULL));
    EXPECT_EQ(U(GetObjectW)(h, sizeof out, NULL), sizeof(BITMAP));
    t_note("GetObject(h,4) -> %d, (h,1) -> %d", U(GetObjectW)(h, 4, &out), U(GetObjectW)(h, 1, &out));
    EXPECT_EQ(U(GetObjectW)(h, sizeof big, big), sizeof(BITMAP));
    U(DeleteObject)(h);
}
