#include "bm.h"

/* ---- CreateBitmap: valid depths -------------------------------------- */
static void create_depth(int bpp) {
    BM_REQ();
    static const int sizes[][2] = { {1,1},{2,3},{7,5},{8,8},{9,2},{16,1},{17,17},{33,4},{100,60} };
    unsigned i;
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        int w = sizes[i][0], h = sizes[i][1];
        HBITMAP b = U(CreateBitmap)(w, h, 1, bpp, NULL);
        CHECKF(b != NULL, "bpp %d %dx%d", bpp, w, h);
        if (!b) continue;
        BITMAP bm;
        EXPECT_EQ(bm_getobj(b, &bm), sizeof(BITMAP));
        EXPECT_EQ(bm.bmType, 0);
        EXPECT_EQ(bm.bmWidth, w);
        EXPECT_EQ(bm.bmHeight, h);
        EXPECT_EQ(bm.bmPlanes, 1);
        EXPECT_EQ(bm.bmBitsPixel, bpp);
        EXPECT_EQ(bm.bmWidthBytes, bm_row_bytes(w, bpp));
        CHECK(bm.bmBits == NULL);
        EXPECT_EQ(U(GetObjectType)(b), OBJ_BITMAP);
        CHECK(U(DeleteObject)(b));
    }
}
TEST(bitmaps_create_depth_1)  { create_depth(1); }
TEST(bitmaps_create_depth_4)  { create_depth(4); }
TEST(bitmaps_create_depth_8)  { create_depth(8); }
TEST(bitmaps_create_depth_16) { create_depth(16); }
TEST(bitmaps_create_depth_24) { create_depth(24); }
TEST(bitmaps_create_depth_32) { create_depth(32); }

/* ---- zero-sized: stock 1x1 mono -------------------------------------- */
TEST(bitmaps_create_zero_size_is_stock) {
    BM_REQ();
    static const int z[][2] = { {0,0},{0,1},{1,0},{0,100},{100,0},{0,-1},{-1,0} };
    unsigned i;
    HBITMAP stock = U(GetStockObject)(BM_DEFAULT_BITMAP);
    CHECK(stock != NULL);
    for (i = 0; i < sizeof z / sizeof z[0]; i++) {
        HBITMAP b = U(CreateBitmap)(z[i][0], z[i][1], 1, 32, NULL);
        CHECKF(b != NULL, "%d,%d", z[i][0], z[i][1]);
        CHECKF(b == stock, "%d,%d returns the stock default bitmap", z[i][0], z[i][1]);
        BITMAP bm;
        if (b) {
            EXPECT_EQ(bm_getobj(b, &bm), sizeof(BITMAP));
            EXPECT_EQ(bm.bmWidth, 1); EXPECT_EQ(bm.bmHeight, 1);
            EXPECT_EQ(bm.bmBitsPixel, 1); EXPECT_EQ(bm.bmPlanes, 1);
        }
    }
}
TEST(bitmaps_create_zero_size_ignores_bits_and_depth) {
    BM_REQ();
    unsigned char junk[8] = {0};
    HBITMAP stock = U(GetStockObject)(BM_DEFAULT_BITMAP);
    CHECK(U(CreateBitmap)(0, 0, 1, 8, junk) == stock);
    CHECK(U(CreateBitmap)(0, 0, 1, 0, NULL) == stock);
    CHECK(U(CreateBitmap)(0, 0, 0, 0, NULL) == stock);
    CHECK(U(CreateBitmap)(0, 5, 1, 1000, NULL) == stock);
}
TEST(bitmaps_create_stock_properties) {
    BM_REQ();
    HBITMAP stock = U(GetStockObject)(BM_DEFAULT_BITMAP);
    REQUIRE(stock);
    EXPECT_EQ(U(GetObjectType)(stock), OBJ_BITMAP);
    BITMAP bm;
    EXPECT_EQ(bm_getobj(stock, &bm), sizeof(BITMAP));
    EXPECT_EQ(bm.bmWidthBytes, 2);
    CHECK(bm.bmBits == NULL);
    /* repeated calls are stable */
    CHECK(U(GetStockObject)(BM_DEFAULT_BITMAP) == stock);
}

/* ---- CreateBitmap invalid / unusual arguments ------------------------- */
TEST(bitmaps_create_negative_dims) {
    BM_REQ();
    static const int d[][2] = { {-1,1},{1,-1},{-1,-1},{-100,50},{50,-100},{0x80000000,1},{1,(int)0x80000000} };
    unsigned i;
    for (i = 0; i < sizeof d / sizeof d[0]; i++) {
        SET_ERR(0);
        HBITMAP b = U(CreateBitmap)(d[i][0], d[i][1], 1, 1, NULL);
        if (b && b != U(GetStockObject)(BM_DEFAULT_BITMAP)) {
            t_note("CreateBitmap(%d,%d) succeeded", d[i][0], d[i][1]);
            U(DeleteObject)(b);
        }
        if (!b) t_note("CreateBitmap(%d,%d) failed, gle=%lu", d[i][0], d[i][1], GetLastError());
    }
}
TEST(bitmaps_create_odd_bpp) {
    BM_REQ();
    static const int bpps[] = { 2, 3, 5, 7, 9, 12, 15, 20, 31, 33, 48, 64, 128, 255, 256 };
    unsigned i;
    for (i = 0; i < sizeof bpps / sizeof bpps[0]; i++) {
        SET_ERR(0);
        HBITMAP b = U(CreateBitmap)(4, 4, 1, bpps[i], NULL);
        if (b) {
            BITMAP bm; bm_getobj(b, &bm);
            t_note("bpp %d accepted -> bmBitsPixel %d", bpps[i], bm.bmBitsPixel);
            U(DeleteObject)(b);
        } else t_note("bpp %d rejected gle=%lu", bpps[i], GetLastError());
    }
}
TEST(bitmaps_create_bpp_zero_planes_zero) {
    BM_REQ();
    HBITMAP b = U(CreateBitmap)(4, 4, 1, 0, NULL);
    if (b) { BITMAP bm; bm_getobj(b, &bm); t_note("bpp 0 -> %d", bm.bmBitsPixel); U(DeleteObject)(b); }
    else t_note("bpp 0 rejected");
    b = U(CreateBitmap)(4, 4, 0, 8, NULL);
    if (b) { BITMAP bm; bm_getobj(b, &bm); t_note("planes 0 -> bpp %d planes %d", bm.bmBitsPixel, bm.bmPlanes); U(DeleteObject)(b); }
    else t_note("planes 0 rejected");
}
TEST(bitmaps_create_multi_plane) {
    BM_REQ();
    /* planes * bpp is used as the effective depth */
    static const int pl[][2] = { {1,1},{2,1},{4,1},{8,1},{3,8},{1,24} };
    unsigned i;
    for (i = 0; i < sizeof pl / sizeof pl[0]; i++) {
        HBITMAP b = U(CreateBitmap)(8, 8, pl[i][0], pl[i][1], NULL);
        if (b) {
            BITMAP bm; bm_getobj(b, &bm);
            t_note("planes %d bpp %d -> planes %d bpp %d wb %d", pl[i][0], pl[i][1], bm.bmPlanes, bm.bmBitsPixel, bm.bmWidthBytes);
            EXPECT_EQ(bm.bmPlanes, 1);
            U(DeleteObject)(b);
        } else t_note("planes %d bpp %d rejected", pl[i][0], pl[i][1]);
    }
}
TEST(bitmaps_create_huge_rejected) {
    BM_REQ();
    static const int d[][3] = { {0x7fffffff,0x7fffffff,32},{100000,100000,32},{0x7fffffff,1,32},{1,0x7fffffff,32},
                                {0x40000000,0x40000000,1} };
    unsigned i;
    for (i = 0; i < sizeof d / sizeof d[0]; i++) {
        HBITMAP b = U(CreateBitmap)(d[i][0], d[i][1], 1, d[i][2], NULL);
        CHECKF(b == NULL, "huge %d x %d x %d", d[i][0], d[i][1], d[i][2]);
        if (b) U(DeleteObject)(b);
    }
}
TEST(bitmaps_create_bits_size_overflow_invalid_param) {
    BM_REQ();
    unsigned char buf[16] = {0};
    /* Row size 256 KiB * 8192 rows = 2^31: the user-mode size check rejects it before touching the bits. */
    SET_ERR(0);
    HBITMAP b = U(CreateBitmap)(65536, 8192, 1, 32, buf);
    CHECK(b == NULL);
    EXPECT_ERR(b, ERROR_INVALID_PARAMETER);
    if (b) U(DeleteObject)(b);
    SET_ERR(0);
    b = U(CreateBitmap)(0x7fffffff, 2, 1, 32, buf);
    CHECK(b == NULL);
    if (b) U(DeleteObject)(b);
    SET_ERR(0);
    b = U(CreateBitmap)(0x10000000, 0x10000000, 1, 1, buf);
    CHECK(b == NULL);
    if (b) U(DeleteObject)(b);
}

/* ---- CreateBitmap with initial bits ------------------------------------ */
TEST(bitmaps_create_bits_mono_word_aligned) {
    BM_REQ(); REQUIRE(U(GetBitmapBits));
    /* 9x3 mono: each scanline is 2 bytes */
    unsigned char in[6] = { 0xFF, 0x80, 0x00, 0x00, 0xAA, 0x80 };
    HBITMAP b = U(CreateBitmap)(9, 3, 1, 1, in);
    REQUIRE(b);
    unsigned char out[8]; memset(out, 0xEE, sizeof out);
    LONG n = U(GetBitmapBits)(b, sizeof out, out);
    EXPECT_EQ(n, 6);
    CHECK(!memcmp(in, out, 6));
    EXPECT_EQ(out[6], 0xEE);
    U(DeleteObject)(b);
}
TEST(bitmaps_create_bits_8bpp_word_aligned) {
    BM_REQ(); REQUIRE(U(GetBitmapBits));
    /* 3x2 8bpp: row 4 bytes (3 + 1 pad) */
    unsigned char in[8] = { 1,2,3,0, 4,5,6,0 };
    HBITMAP b = U(CreateBitmap)(3, 2, 1, 8, in);
    REQUIRE(b);
    unsigned char out[8]; memset(out, 0xEE, sizeof out);
    EXPECT_EQ(U(GetBitmapBits)(b, 8, out), 8);
    EXPECT_EQ(out[0], 1); EXPECT_EQ(out[1], 2); EXPECT_EQ(out[2], 3);
    EXPECT_EQ(out[4], 4); EXPECT_EQ(out[5], 5); EXPECT_EQ(out[6], 6);
    U(DeleteObject)(b);
}
TEST(bitmaps_create_bits_24bpp_word_aligned) {
    BM_REQ(); REQUIRE(U(GetBitmapBits));
    /* 1x2 24bpp: row 4 bytes (3 + 1 pad) -- WORD alignment, not DWORD */
    unsigned char in[8] = { 0x11,0x22,0x33,0, 0x44,0x55,0x66,0 };
    HBITMAP b = U(CreateBitmap)(1, 2, 1, 24, in);
    REQUIRE(b);
    BITMAP bm; bm_getobj(b, &bm);
    EXPECT_EQ(bm.bmWidthBytes, 4);
    unsigned char out[8] = {0};
    EXPECT_EQ(U(GetBitmapBits)(b, 8, out), 8);
    EXPECT_EQ(out[0], 0x11); EXPECT_EQ(out[1], 0x22); EXPECT_EQ(out[2], 0x33);
    EXPECT_EQ(out[4], 0x44); EXPECT_EQ(out[5], 0x55); EXPECT_EQ(out[6], 0x66);
    U(DeleteObject)(b);
}
TEST(bitmaps_create_bits_24bpp_odd_width_stride) {
    BM_REQ(); REQUIRE(U(GetBitmapBits));
    /* 3x1 24bpp: 9 bytes -> padded to 10 */
    unsigned char in[10] = { 1,2,3,4,5,6,7,8,9,0 };
    HBITMAP b = U(CreateBitmap)(3, 1, 1, 24, in);
    REQUIRE(b);
    BITMAP bm; bm_getobj(b, &bm);
    EXPECT_EQ(bm.bmWidthBytes, 10);
    unsigned char out[10] = {0};
    EXPECT_EQ(U(GetBitmapBits)(b, 10, out), 10);
    CHECK(!memcmp(in, out, 9));
    U(DeleteObject)(b);
}
TEST(bitmaps_create_bits_16bpp_32bpp_roundtrip) {
    BM_REQ(); REQUIRE(U(GetBitmapBits));
    unsigned char in[16], out[16]; int i;
    for (i = 0; i < 16; i++) in[i] = (unsigned char)(i * 17 + 3);
    HBITMAP b = U(CreateBitmap)(2, 2, 1, 32, in);
    REQUIRE(b);
    EXPECT_EQ(U(GetBitmapBits)(b, 16, out), 16);
    CHECK(!memcmp(in, out, 16) || bm_is_wine());
    U(DeleteObject)(b);
    b = U(CreateBitmap)(3, 2, 1, 16, in);
    REQUIRE(b);
    memset(out, 0, sizeof out);
    EXPECT_EQ(U(GetBitmapBits)(b, 16, out), 12);
    CHECK(!memcmp(in, out, 12));
    U(DeleteObject)(b);
}
TEST(bitmaps_create_bits_4bpp_roundtrip) {
    BM_REQ(); REQUIRE(U(GetBitmapBits));
    /* 5x2 4bpp: row = 3 bytes -> 4 */
    unsigned char in[8] = { 0x12,0x34,0x50,0, 0x9A,0xBC,0xD0,0 };
    HBITMAP b = U(CreateBitmap)(5, 2, 1, 4, in);
    REQUIRE(b);
    BITMAP bm; bm_getobj(b, &bm);
    EXPECT_EQ(bm.bmWidthBytes, 4);
    unsigned char out[8] = {0};
    EXPECT_EQ(U(GetBitmapBits)(b, 8, out), 8);
    CHECK(out[0] == 0x12 && out[1] == 0x34 && out[2] == 0x50);
    CHECK(out[4] == 0x9A && out[5] == 0xBC && out[6] == 0xD0);
    U(DeleteObject)(b);
}
TEST(bitmaps_create_null_bits_zero_filled_or_defined) {
    BM_REQ(); REQUIRE(U(GetBitmapBits));
    HBITMAP b = U(CreateBitmap)(16, 4, 1, 1, NULL);
    REQUIRE(b);
    unsigned char out[8];
    EXPECT_EQ(U(GetBitmapBits)(b, 8, out), 8);
    int allzero = 1, i; for (i = 0; i < 8; i++) if (out[i]) allzero = 0;
    if (!allzero) t_note("uninitialised mono DDB is not zero-filled");
    U(DeleteObject)(b);
}

/* ---- CreateBitmapIndirect ---------------------------------------------- */
static HBITMAP indirect(int w, int h, int wb, int planes, int bpp, void *bits, int type) {
    BITMAP bm; memset(&bm, 0, sizeof bm);
    bm.bmType = type; bm.bmWidth = w; bm.bmHeight = h; bm.bmWidthBytes = wb;
    bm.bmPlanes = planes; bm.bmBitsPixel = bpp; bm.bmBits = bits;
    return U(CreateBitmapIndirect)(&bm);
}
TEST(bitmaps_indirect_basic) {
    BM_REQ();
    static const int bpps[] = {1,4,8,16,24,32}; unsigned i;
    for (i = 0; i < 6; i++) {
        int wb = bm_row_bytes(10, bpps[i]);
        HBITMAP b = indirect(10, 7, wb, 1, bpps[i], NULL, 0);
        CHECKF(b != NULL, "bpp %d", bpps[i]);
        if (!b) continue;
        BITMAP bm; bm_getobj(b, &bm);
        EXPECT_EQ(bm.bmWidth, 10); EXPECT_EQ(bm.bmHeight, 7);
        EXPECT_EQ(bm.bmBitsPixel, bpps[i]); EXPECT_EQ(bm.bmWidthBytes, wb);
        U(DeleteObject)(b);
    }
}
TEST(bitmaps_indirect_bmtype_ignored) {
    BM_REQ();
    static const int types[] = { 0, 1, 0x4D42, -1, 0x7fff };
    unsigned i;
    for (i = 0; i < 5; i++) {
        HBITMAP b = indirect(8, 8, 2, 1, 1, NULL, types[i]);
        CHECKF(b != NULL, "type %d", types[i]);
        if (b) { BITMAP bm; bm_getobj(b, &bm); EXPECT_EQ(bm.bmType, 0); U(DeleteObject)(b); }
    }
}
TEST(bitmaps_indirect_widthbytes_zero_invalid) {
    BM_REQ();
    SET_ERR(0);
    HBITMAP b = indirect(8, 8, 0, 1, 1, NULL, 0);
    CHECK(b == NULL); EXPECT_ERR(b, ERROR_INVALID_PARAMETER);
    if (b) U(DeleteObject)(b);
}
TEST(bitmaps_indirect_widthbytes_odd_invalid) {
    BM_REQ();
    static const int wbs[] = { 1, 3, 5, 7, 9, 101 };
    unsigned i;
    for (i = 0; i < 6; i++) {
        SET_ERR(0);
        HBITMAP b = indirect(8, 8, wbs[i], 1, 8, NULL, 0);
        CHECKF(b == NULL, "wb %d", wbs[i]);
        EXPECT_ERR(b, ERROR_INVALID_PARAMETER);
        if (b) U(DeleteObject)(b);
    }
}
TEST(bitmaps_indirect_widthbytes_too_small_invalid) {
    BM_REQ();
    static const int c[][3] = { {17,1,2},{9,1,0},{8,8,6},{3,24,8},{4,32,12},{9,4,4} };
    /* width, bpp, widthbytes smaller than the required stride (checked only when computed > given) */
    unsigned i;
    for (i = 0; i < 6; i++) {
        int w = c[i][0], bpp = c[i][1], wb = c[i][2];
        if (wb >= bm_row_bytes(w, bpp)) continue;
        SET_ERR(0);
        HBITMAP b = indirect(w, 4, wb, 1, bpp, NULL, 0);
        CHECKF(b == NULL, "w %d bpp %d wb %d", w, bpp, wb);
        if (wb) EXPECT_ERR(b, ERROR_INVALID_PARAMETER);
        if (b) U(DeleteObject)(b);
    }
}
TEST(bitmaps_indirect_widthbytes_larger_ok) {
    BM_REQ();
    HBITMAP b = indirect(8, 4, 64, 1, 1, NULL, 0);
    CHECK(b != NULL);
    if (b) { BITMAP bm; bm_getobj(b, &bm); EXPECT_EQ(bm.bmWidthBytes, 2); U(DeleteObject)(b); }
}
TEST(bitmaps_indirect_planes_must_be_one) {
    BM_REQ();
    static const int pl[] = { 0, 2, 3, 4, 8, -1, 256 };
    unsigned i;
    for (i = 0; i < 7; i++) {
        SET_ERR(0);
        HBITMAP b = indirect(8, 8, 8, pl[i], 8, NULL, 0);
        CHECKF(b == NULL, "planes %d", pl[i]);
        EXPECT_ERR(b, ERROR_INVALID_PARAMETER);
        if (b) U(DeleteObject)(b);
    }
}
TEST(bitmaps_indirect_width_overflow_invalid) {
    BM_REQ();
    static const int w[] = { 0x7fffffff, 0x7ffffff0, 0x10000000 };
    unsigned i;
    for (i = 0; i < 3; i++) {
        SET_ERR(0);
        HBITMAP b = indirect(w[i], 1, 0x1000, 1, 32, NULL, 0);
        CHECKF(b == NULL, "w %x", w[i]);
        EXPECT_ERR(b, ERROR_INVALID_PARAMETER);
        if (b) U(DeleteObject)(b);
    }
}
TEST(bitmaps_indirect_zero_size_is_stock) {
    BM_REQ();
    HBITMAP stock = U(GetStockObject)(BM_DEFAULT_BITMAP);
    CHECK(indirect(0, 0, 2, 1, 1, NULL, 0) == stock);
    CHECK(indirect(0, 5, 2, 1, 8, NULL, 0) == stock);
    CHECK(indirect(5, 0, 8, 1, 8, NULL, 0) == stock);
}
TEST(bitmaps_indirect_bits_stride_repacked) {
    BM_REQ(); REQUIRE(U(GetBitmapBits));
    /* source rows have stride 6 although the bitmap rows are 2 bytes */
    unsigned char src[18] = { 0xF0,0x80,0xEE,0xEE,0xEE,0xEE, 0x0F,0x00,0xEE,0xEE,0xEE,0xEE, 0xAA,0x80,0xEE,0xEE,0xEE,0xEE };
    HBITMAP b = indirect(9, 3, 6, 1, 1, src, 0);
    REQUIRE(b);
    BITMAP bm; bm_getobj(b, &bm);
    EXPECT_EQ(bm.bmWidthBytes, 2);
    unsigned char out[6] = {0};
    EXPECT_EQ(U(GetBitmapBits)(b, 6, out), 6);
    CHECK(out[0] == 0xF0 && out[1] == 0x80 && out[2] == 0x0F && out[3] == 0x00 && out[4] == 0xAA && out[5] == 0x80);
    U(DeleteObject)(b);
}
TEST(bitmaps_indirect_bits_exact_stride) {
    BM_REQ(); REQUIRE(U(GetBitmapBits));
    unsigned char src[8] = { 1,2,3,4, 5,6,7,8 };
    HBITMAP b = indirect(4, 2, 4, 1, 8, src, 0);
    REQUIRE(b);
    unsigned char out[8] = {0};
    EXPECT_EQ(U(GetBitmapBits)(b, 8, out), 8);
    CHECK(!memcmp(src, out, 8));
    U(DeleteObject)(b);
}
TEST(bitmaps_indirect_bpp_zero) {
    BM_REQ();
    HBITMAP b = indirect(8, 8, 2, 1, 0, NULL, 0);
    if (b) { BITMAP bm; bm_getobj(b, &bm); t_note("indirect bpp 0 -> %d", bm.bmBitsPixel); U(DeleteObject)(b); }
    else t_note("indirect bpp 0 rejected gle=%lu", GetLastError());
}

/* ---- CreateCompatibleBitmap -------------------------------------------- */
TEST(bitmaps_ccb_screen_dc_matches_device) {
    BM_REQ(); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC)); REQUIRE(U(GetDeviceCaps));
    HDC sc = U(GetDC)(NULL);
    REQUIRE(sc);
    HBITMAP b = U(CreateCompatibleBitmap)(sc, 13, 9);
    CHECK(b != NULL);
    if (b) {
        BITMAP bm; EXPECT_EQ(bm_getobj(b, &bm), sizeof bm);
        EXPECT_EQ(bm.bmWidth, 13); EXPECT_EQ(bm.bmHeight, 9);
        EXPECT_EQ(bm.bmPlanes, U(GetDeviceCaps)(sc, PLANES));
        EXPECT_EQ(bm.bmBitsPixel, U(GetDeviceCaps)(sc, BITSPIXEL));
        EXPECT_EQ(bm.bmWidthBytes, bm_row_bytes(13, bm.bmBitsPixel));
        CHECK(bm.bmBits == NULL);
        EXPECT_EQ(U(GetObjectType)(b), OBJ_BITMAP);
        U(DeleteObject)(b);
    }
    U(ReleaseDC)(NULL, sc);
}
TEST(bitmaps_ccb_memdc_default_is_mono) {
    BM_REQ();
    HDC dc = U(CreateCompatibleDC)(NULL);
    REQUIRE(dc);
    HBITMAP b = U(CreateCompatibleBitmap)(dc, 10, 10);
    CHECK(b != NULL);
    if (b) {
        BITMAP bm; bm_getobj(b, &bm);
        EXPECT_EQ(bm.bmBitsPixel, 1); EXPECT_EQ(bm.bmPlanes, 1);
        EXPECT_EQ(bm.bmWidthBytes, 2);
        U(DeleteObject)(b);
    }
    U(DeleteDC)(dc);
}
TEST(bitmaps_ccb_memdc_follows_selected_ddb) {
    BM_REQ();
    static const int bpps[] = { 1, 4, 8, 16, 24, 32 }; unsigned i;
    for (i = 0; i < 6; i++) {
        ddb_t d;
        if (!ddb_open(&d, 4, 4, 1, bpps[i], NULL)) { CHECK(0); continue; }
        HBITMAP b = U(CreateCompatibleBitmap)(d.dc, 5, 6);
        CHECKF(b != NULL, "bpp %d", bpps[i]);
        if (b) {
            BITMAP bm; bm_getobj(b, &bm);
            t_note("compat of %dbpp DDB DC -> %d bpp", bpps[i], bm.bmBitsPixel);
            if (bpps[i] == 1) EXPECT_EQ(bm.bmBitsPixel, 1);
            EXPECT_EQ(bm.bmWidth, 5); EXPECT_EQ(bm.bmHeight, 6);
            U(DeleteObject)(b);
        }
        ddb_close(&d);
    }
}
TEST(bitmaps_ccb_dibsection_dc) {
    BM_REQ_DIB();
    static const int bpps[] = { 1, 4, 8, 16, 24, 32 }; unsigned i;
    for (i = 0; i < 6; i++) {
        dib_t d;
        if (!dib_open(&d, 8, 8, bpps[i])) { CHECKF(0, "dib %d", bpps[i]); continue; }
        HBITMAP b = U(CreateCompatibleBitmap)(d.dc, 5, 3);
        CHECKF(b != NULL, "bpp %d", bpps[i]);
        if (b) {
            DIBSECTION ds; memset(&ds, 0, sizeof ds);
            int n = U(GetObjectA)(b, sizeof ds, &ds);
            EXPECT_EQ(ds.dsBm.bmWidth, 5); EXPECT_EQ(ds.dsBm.bmHeight, 3);
            EXPECT_EQ(ds.dsBm.bmBitsPixel, bpps[i]);
            if (n == sizeof ds) CHECK(ds.dsBm.bmBits != NULL);
            else t_note("compat of DIB DC bpp %d: GetObject size %d (DDB)", bpps[i], n);
            U(DeleteObject)(b);
        }
        dib_close(&d);
    }
}
TEST(bitmaps_ccb_zero_size_is_stock) {
    BM_REQ();
    HDC dc = U(CreateCompatibleDC)(NULL);
    REQUIRE(dc);
    HBITMAP stock = U(GetStockObject)(BM_DEFAULT_BITMAP);
    CHECK(U(CreateCompatibleBitmap)(dc, 0, 0) == stock);
    CHECK(U(CreateCompatibleBitmap)(dc, 0, 5) == stock);
    CHECK(U(CreateCompatibleBitmap)(dc, 5, 0) == stock);
    U(DeleteDC)(dc);
}
TEST(bitmaps_ccb_null_dc_fails) {
    BM_REQ();
    SET_ERR(0);
    HBITMAP b = U(CreateCompatibleBitmap)(NULL, 10, 10);
    CHECK(b == NULL);
    if (b) U(DeleteObject)(b);
}
TEST(bitmaps_ccb_bogus_dc_fails) {
    BM_REQ();
    static const ULONG_PTR h[] = { 1, 0x1234, 0xdeadbeef, 0x00010001, (ULONG_PTR)-1 };
    unsigned i;
    for (i = 0; i < 5; i++) {
        HBITMAP b = U(CreateCompatibleBitmap)((HDC)h[i], 4, 4);
        CHECKF(b == NULL, "handle %llx", (unsigned long long)h[i]);
        if (b) U(DeleteObject)(b);
    }
}
TEST(bitmaps_ccb_wrong_object_type_fails) {
    BM_REQ(); REQUIRE(U(CreatePen));
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0);
    HBITMAP b = U(CreateCompatibleBitmap)((HDC)p, 4, 4);
    CHECK(b == NULL);
    if (b) U(DeleteObject)(b);
    HBITMAP bb = U(CreateBitmap)(2, 2, 1, 1, NULL);
    b = U(CreateCompatibleBitmap)((HDC)bb, 4, 4);
    CHECK(b == NULL);
    if (b) U(DeleteObject)(b);
    U(DeleteObject)(bb); U(DeleteObject)(p);
}
TEST(bitmaps_ccb_deleted_dc_fails) {
    BM_REQ();
    HDC dc = U(CreateCompatibleDC)(NULL);
    REQUIRE(dc);
    CHECK(U(DeleteDC)(dc));
    HBITMAP b = U(CreateCompatibleBitmap)(dc, 4, 4);
    CHECK(b == NULL);
    if (b) U(DeleteObject)(b);
}
TEST(bitmaps_ccb_negative_and_huge) {
    BM_REQ();
    HDC dc = U(CreateCompatibleDC)(NULL);
    REQUIRE(dc);
    static const int d[][2] = { {-1,4},{4,-1},{-5,-5},{0x7fffffff,0x7fffffff},{0x40000000,0x40000000} };
    unsigned i;
    for (i = 0; i < 5; i++) {
        HBITMAP b = U(CreateCompatibleBitmap)(dc, d[i][0], d[i][1]);
        if (d[i][0] > 4096 || d[i][1] > 4096) CHECKF(b == NULL, "%d x %d", d[i][0], d[i][1]);
        else if (b) t_note("ccb(%d,%d) accepted", d[i][0], d[i][1]);
        else t_note("ccb(%d,%d) rejected", d[i][0], d[i][1]);
        if (b) U(DeleteObject)(b);
    }
    U(DeleteDC)(dc);
}
TEST(bitmaps_ccb_info_dc) {
    BM_REQ(); REQUIRE(U(CreateICA)); REQUIRE(U(DeleteDC));
    HDC ic = U(CreateICA)("DISPLAY", NULL, NULL, NULL);
    REQUIRE(ic);
    HBITMAP b = U(CreateCompatibleBitmap)(ic, 8, 8);
    if (b) { BITMAP bm; bm_getobj(b, &bm); t_note("ccb on IC bpp %d", bm.bmBitsPixel); U(DeleteObject)(b); }
    else t_note("ccb on IC fails");
    U(DeleteDC)(ic);
}

/* ---- CreateDiscardableBitmap -------------------------------------------- */
TEST(bitmaps_discardable_basic) {
    BM_REQ(); REQUIRE(U(CreateDiscardableBitmap)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC)); REQUIRE(U(GetDeviceCaps));
    HDC sc = U(GetDC)(NULL);
    REQUIRE(sc);
    HBITMAP b = U(CreateDiscardableBitmap)(sc, 11, 7);
    CHECK(b != NULL);
    if (b) {
        BITMAP bm; EXPECT_EQ(bm_getobj(b, &bm), sizeof bm);
        EXPECT_EQ(bm.bmWidth, 11); EXPECT_EQ(bm.bmHeight, 7);
        EXPECT_EQ(bm.bmBitsPixel, U(GetDeviceCaps)(sc, BITSPIXEL));
        EXPECT_EQ(U(GetObjectType)(b), OBJ_BITMAP);
        U(DeleteObject)(b);
    }
    U(ReleaseDC)(NULL, sc);
}
TEST(bitmaps_discardable_memdc_mono) {
    BM_REQ(); REQUIRE(U(CreateDiscardableBitmap));
    HDC dc = U(CreateCompatibleDC)(NULL);
    REQUIRE(dc);
    HBITMAP b = U(CreateDiscardableBitmap)(dc, 8, 8);
    CHECK(b != NULL);
    if (b) { BITMAP bm; bm_getobj(b, &bm); EXPECT_EQ(bm.bmBitsPixel, 1); U(DeleteObject)(b); }
    U(DeleteDC)(dc);
}
TEST(bitmaps_discardable_invalid) {
    BM_REQ(); REQUIRE(U(CreateDiscardableBitmap));
    HBITMAP b = U(CreateDiscardableBitmap)(NULL, 4, 4);
    CHECK(b == NULL);
    if (b) U(DeleteObject)(b);
    HDC dc = U(CreateCompatibleDC)(NULL);
    REQUIRE(dc);
    b = U(CreateDiscardableBitmap)(dc, 0, 0);
    if (b) { t_note("discardable 0x0 -> %p (stock=%d)", b, b == U(GetStockObject)(BM_DEFAULT_BITMAP)); }
    else t_note("discardable 0x0 fails");
    U(DeleteDC)(dc);
}
TEST(bitmaps_discardable_is_forwarder_or_ccb) {
    BM_REQ(); REQUIRE(U(CreateDiscardableBitmap));
    CHECK(UG(CreateDiscardableBitmap) != NULL);
    t_note("gdi32full has CreateDiscardableBitmap: %d", UF(CreateDiscardableBitmap) != NULL);
}
