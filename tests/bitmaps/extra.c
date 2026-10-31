/* Additional bitmap edge cases: partial transfers, sentinels, DIB-to-DIB blits, ownership hand-off. */
#include "bm.h"

#define XREQ() do { BM_REQ(); REQUIRE(U(GetBitmapBits)); REQUIRE(U(SetBitmapBits)); } while (0)

TEST(bitmaps_extra_setbits_partial_leaves_tail) {
    XREQ();
    HBITMAP b = U(CreateBitmap)(16, 4, 1, 1, NULL);   /* 2 bytes/row, 8 bytes total */
    REQUIRE(b);
    unsigned char ff[8], zero[2] = {0, 0}, out[8];
    memset(ff, 0xFF, 8);
    EXPECT_EQ(U(SetBitmapBits)(b, 8, ff), 8);
    EXPECT_EQ(U(SetBitmapBits)(b, 2, zero), 2);
    memset(out, 0x55, 8);
    EXPECT_EQ(U(GetBitmapBits)(b, 8, out), 8);
    EXPECT_EQ(out[0], 0); EXPECT_EQ(out[1], 0);
    int i;
    for (i = 2; i < 8; i++) CHECKF(out[i] == 0xFF, "byte %d = %02x", i, out[i]);
    U(DeleteObject)(b);
}
TEST(bitmaps_extra_getbits_does_not_write_past_count) {
    XREQ();
    unsigned char in[8] = {1,2,3,4,5,6,7,8}, out[16];
    HBITMAP b = U(CreateBitmap)(16, 4, 1, 1, in);
    REQUIRE(b);
    static const int counts[] = { 1, 2, 3, 5, 7 }; unsigned i; int k;
    for (i = 0; i < 5; i++) {
        memset(out, 0xA5, sizeof out);
        LONG n = U(GetBitmapBits)(b, counts[i], out);
        EXPECT_EQ(n, counts[i]);
        for (k = 0; k < counts[i]; k++) CHECKF(out[k] == in[k], "count %d byte %d", counts[i], k);
        for (k = counts[i]; k < 16; k++) CHECKF(out[k] == 0xA5, "count %d overwrote byte %d", counts[i], k);
    }
    U(DeleteObject)(b);
}
TEST(bitmaps_extra_getbits_stock_bitmap_size) {
    XREQ();
    HGDIOBJ st = U(GetStockObject)(BM_DEFAULT_BITMAP);
    LONG n = U(GetBitmapBits)(st, 0, NULL);
    t_note("GetBitmapBits(stock, 0, NULL) = %ld", (long)n);
    HBITMAP one = U(CreateBitmap)(1, 1, 1, 1, NULL);
    REQUIRE(one);
    EXPECT_EQ(U(GetBitmapBits)(one, 0, NULL), 2);
    if (n != 2) t_note("stock bitmap size query returned %ld, not 2", (long)n);
    U(DeleteObject)(one);
}
TEST(bitmaps_extra_bitmap_content_moves_between_dcs) {
    BM_REQ_PIX();
    HBITMAP b = U(CreateBitmap)(8, 8, 1, 1, NULL);
    REQUIRE(b);
    HDC d1 = U(CreateCompatibleDC)(NULL), d2 = U(CreateCompatibleDC)(NULL);
    REQUIRE(d1); REQUIRE(d2);
    HGDIOBJ o1 = U(SelectObject)(d1, b);
    U(SetPixelV)(d1, 0, 0, RGB(255,255,255));
    U(SetPixelV)(d1, 7, 7, RGB(255,255,255));
    U(SetPixelV)(d1, 3, 3, RGB(0,0,0));
    U(SelectObject)(d1, o1);
    HGDIOBJ o2 = U(SelectObject)(d2, b);
    CHECK(o2 != NULL);
    EXPECT_EQ(U(GetPixel)(d2, 0, 0), RGB(255,255,255));
    EXPECT_EQ(U(GetPixel)(d2, 7, 7), RGB(255,255,255));
    EXPECT_EQ(U(GetPixel)(d2, 3, 3), RGB(0,0,0));
    U(SelectObject)(d2, o2);
    U(DeleteDC)(d1); U(DeleteDC)(d2); U(DeleteObject)(b);
}
TEST(bitmaps_extra_select_returns_default_then_previous) {
    BM_REQ();
    HDC dc = U(CreateCompatibleDC)(NULL);
    REQUIRE(dc);
    HBITMAP a = U(CreateBitmap)(2, 2, 1, 1, NULL), b = U(CreateBitmap)(3, 3, 1, 1, NULL);
    HGDIOBJ def = U(GetCurrentObject)(dc, OBJ_BITMAP);
    CHECK(def == U(GetStockObject)(BM_DEFAULT_BITMAP));
    CHECK(U(SelectObject)(dc, a) == def);
    CHECK(U(SelectObject)(dc, b) == (HGDIOBJ)a);
    CHECK(U(SelectObject)(dc, def) == (HGDIOBJ)b);
    CHECK(U(GetCurrentObject)(dc, OBJ_BITMAP) == def);
    U(DeleteDC)(dc); U(DeleteObject)(a); U(DeleteObject)(b);
}
TEST(bitmaps_extra_dib_to_dib_srccopy_exact) {
    BM_REQ_DIB(); REQUIRE(U(BitBlt));
    dib_t s, d;
    REQUIRE(dib_open_ex(&s, 6, 6, 32, 0x100, NULL, 0));
    if (!dib_open_ex(&d, 6, 6, 32, 0x100, NULL, 0)) { dib_close(&s); CHECK(0); return; }
    DWORD *sp = (DWORD *)s.bits; int i, x, y;
    for (i = 0; i < 36; i++) sp[i] = 0x00010203u * (i + 1) ^ 0x00F0F0F0u;
    dib_fill32(&d, 0x00AAAAAA);
    CHECK(U(BitBlt)(d.dc, 1, 2, 4, 3, s.dc, 1, 1, SRCCOPY));
    U(GdiFlush)();
    DWORD *dp = (DWORD *)d.bits;
    for (y = 0; y < 6; y++) for (x = 0; x < 6; x++) {
        DWORD want = 0x00AAAAAA;
        if (x >= 1 && x < 5 && y >= 2 && y < 5) want = sp[(y - 1) * 6 + (x)] ;
        CHECKF((dp[y * 6 + x] & 0xFFFFFF) == (want & 0xFFFFFF), "(%d,%d) %06x want %06x", x, y, dp[y*6+x] & 0xFFFFFF, want & 0xFFFFFF);
    }
    dib_close(&s); dib_close(&d);
}
TEST(bitmaps_extra_dib_srcinvert_twice_restores) {
    BM_REQ_DIB(); REQUIRE(U(BitBlt));
    dib_t s, d;
    REQUIRE(dib_open_ex(&s, 5, 5, 32, 0x100, NULL, 0));
    if (!dib_open_ex(&d, 5, 5, 32, 0x100, NULL, 0)) { dib_close(&s); CHECK(0); return; }
    int i;
    for (i = 0; i < 25; i++) { ((DWORD *)s.bits)[i] = 0x00123456u + i * 0x010101; ((DWORD *)d.bits)[i] = 0x00FEDCBAu - i * 0x020101; }
    DWORD orig[25];
    U(GdiFlush)();
    memcpy(orig, d.bits, sizeof orig);
    CHECK(U(BitBlt)(d.dc, 0, 0, 5, 5, s.dc, 0, 0, SRCINVERT));
    U(GdiFlush)();
    for (i = 0; i < 25; i++) CHECKF((((DWORD *)d.bits)[i] & 0xFFFFFF) == ((orig[i] ^ ((DWORD *)s.bits)[i]) & 0xFFFFFF), "xor %d", i);
    CHECK(U(BitBlt)(d.dc, 0, 0, 5, 5, s.dc, 0, 0, SRCINVERT));
    U(GdiFlush)();
    for (i = 0; i < 25; i++) CHECKF((((DWORD *)d.bits)[i] & 0xFFFFFF) == (orig[i] & 0xFFFFFF), "restore %d", i);
    dib_close(&s); dib_close(&d);
}
TEST(bitmaps_extra_dib_blit_clipped_to_dest) {
    BM_REQ_DIB(); REQUIRE(U(BitBlt));
    dib_t s, d;
    REQUIRE(dib_open_ex(&s, 4, 4, 32, 0x100, NULL, 0));
    if (!dib_open_ex(&d, 4, 4, 32, 0x100, NULL, 0)) { dib_close(&s); CHECK(0); return; }
    dib_fill32(&s, 0x00112233); dib_fill32(&d, 0x00445566);
    U(BitBlt)(d.dc, -2, -2, 4, 4, s.dc, 0, 0, SRCCOPY);   /* only dest (0..1,0..1) is covered */
    U(GdiFlush)();
    int x, y;
    for (y = 0; y < 4; y++) for (x = 0; x < 4; x++) {
        DWORD want = (x < 2 && y < 2) ? 0x112233 : 0x445566;
        CHECKF((((DWORD *)d.bits)[y * 4 + x] & 0xFFFFFF) == want, "(%d,%d)", x, y);
    }
    dib_close(&s); dib_close(&d);
}
TEST(bitmaps_extra_dib_stretch_2x_nearest) {
    BM_REQ_DIB(); REQUIRE(U(StretchBlt)); REQUIRE(U(SetStretchBltMode));
    dib_t s, d;
    REQUIRE(dib_open_ex(&s, 2, 2, 32, 0x100, NULL, 0));
    if (!dib_open_ex(&d, 4, 4, 32, 0x100, NULL, 0)) { dib_close(&s); CHECK(0); return; }
    DWORD v[4] = { 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00FFFFFF };
    memcpy(s.bits, v, sizeof v);
    U(SetStretchBltMode)(d.dc, COLORONCOLOR);
    CHECK(U(StretchBlt)(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 2, 2, SRCCOPY));
    U(GdiFlush)();
    int x, y;
    for (y = 0; y < 4; y++) for (x = 0; x < 4; x++)
        CHECKF((((DWORD *)d.bits)[y * 4 + x] & 0xFFFFFF) == v[(y / 2) * 2 + x / 2], "(%d,%d)", x, y);
    dib_close(&s); dib_close(&d);
}
TEST(bitmaps_extra_patblt_on_dib_exact) {
    BM_REQ_DIB(); REQUIRE(U(PatBlt)); REQUIRE(U(CreateSolidBrush));
    dib_t d;
    REQUIRE(dib_open_ex(&d, 4, 4, 32, 0x100, NULL, 0));
    dib_fill32(&d, 0x00102030);
    HBRUSH br = U(CreateSolidBrush)(RGB(0x11, 0x22, 0x33));
    HGDIOBJ ob = U(SelectObject)(d.dc, br);
    CHECK(U(PatBlt)(d.dc, 1, 1, 2, 2, PATCOPY));
    U(GdiFlush)();
    int x, y;
    for (y = 0; y < 4; y++) for (x = 0; x < 4; x++) {
        DWORD want = (x >= 1 && x < 3 && y >= 1 && y < 3) ? 0x00112233 : 0x00102030;   /* DIB memory is 0x00RRGGBB */
        CHECKF((((DWORD *)d.bits)[y * 4 + x] & 0xFFFFFF) == want, "(%d,%d)", x, y);
    }
    CHECK(U(PatBlt)(d.dc, 0, 0, 4, 4, DSTINVERT));
    U(GdiFlush)();
    CHECK((((DWORD *)d.bits)[0] & 0xFFFFFF) == (0x00102030 ^ 0xFFFFFF));
    U(SelectObject)(d.dc, ob);
    U(DeleteObject)(br);
    dib_close(&d);
}
TEST(bitmaps_extra_dib_24bpp_blit_odd_stride) {
    BM_REQ_DIB(); REQUIRE(U(BitBlt));
    dib_t s, d;
    REQUIRE(dib_open(&s, 5, 3, 24));
    if (!dib_open(&d, 5, 3, 24)) { dib_close(&s); CHECK(0); return; }
    int i, x, y, stride = 16;   /* 5*3 = 15 -> DWORD aligned 16 */
    for (i = 0; i < stride * 3; i++) s.bits[i] = (unsigned char)(i * 7 + 3);
    memset(d.bits, 0, stride * 3);
    CHECK(U(BitBlt)(d.dc, 0, 0, 5, 3, s.dc, 0, 0, SRCCOPY));
    U(GdiFlush)();
    for (y = 0; y < 3; y++) for (x = 0; x < 15; x++)
        CHECKF(d.bits[y * stride + x] == s.bits[y * stride + x], "(%d,%d)", x, y);
    dib_close(&s); dib_close(&d);
}
TEST(bitmaps_extra_createbitmap_bits_not_retained) {
    XREQ();
    unsigned char in[4] = {0xF0, 0, 0xF0, 0}, out[4];
    HBITMAP b = U(CreateBitmap)(16, 2, 1, 1, in);
    REQUIRE(b);
    in[0] = 0; in[2] = 0;   /* changing the source must not change the bitmap */
    memset(out, 0, 4);
    EXPECT_EQ(U(GetBitmapBits)(b, 4, out), 4);
    EXPECT_EQ(out[0], 0xF0); EXPECT_EQ(out[2], 0xF0);
    U(DeleteObject)(b);
}
TEST(bitmaps_extra_createbitmap_dims_reported_exactly) {
    BM_REQ();
    static const int w[] = { 1, 2, 15, 16, 17, 31, 32, 33, 63, 65, 255 }; unsigned i;
    for (i = 0; i < sizeof w / sizeof w[0]; i++) {
        HBITMAP b = U(CreateBitmap)(w[i], 3, 1, 1, NULL);
        CHECKF(b != NULL, "w %d", w[i]);
        if (!b) continue;
        BITMAP bm; EXPECT_EQ(bm_getobj(b, &bm), sizeof bm);
        EXPECT_EQ(bm.bmWidth, w[i]); EXPECT_EQ(bm.bmHeight, 3);
        EXPECT_EQ(bm.bmWidthBytes, bm_row_bytes(w[i], 1));
        EXPECT_EQ(bm.bmType, 0);
        U(DeleteObject)(b);
    }
}
