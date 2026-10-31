#include "gap4.h"
/* GdiGetBitmapBitsSize(BITMAPINFOHEADER / BITMAPCOREHEADER): bytes of pixel data. */

typedef struct { BYTE b[64]; } HDRBUF;
static void mkhdr(HDRBUF *h, LONG w, LONG ht, WORD pl, WORD bc, DWORD comp, DWORD simg) {
    memset(h, 0, sizeof *h);
    BITMAPINFOHEADER *b = (BITMAPINFOHEADER *)h;
    b->biSize = 40; b->biWidth = w; b->biHeight = ht; b->biPlanes = pl; b->biBitCount = bc;
    b->biCompression = comp; b->biSizeImage = simg;
}
static void mkcore(HDRBUF *h, WORD w, WORD ht, WORD pl, WORD bc) {
    memset(h, 0, sizeof *h);
    BITMAPCOREHEADER *c = (BITMAPCOREHEADER *)h;
    c->bcSize = 12; c->bcWidth = w; c->bcHeight = ht; c->bcPlanes = pl; c->bcBitCount = bc;
}
/* Reference model of the scan-line computation. */
static int cjscan(unsigned w, unsigned long long pl, unsigned bc, unsigned *out) {
    unsigned long long t = (unsigned long long)w * pl;
    if (t >= 0x100000000ull) return 0;
    t = (t & 0xffffffffull) * bc;
    if (t >= 0x100000000ull) return 0;
    unsigned u = (unsigned)t, v = u + 0x1f;
    if (u > v) return 0;
    *out = (v >> 3) & 0x1ffffffc;
    return 1;
}
static DWORD model_bi(LONG w, LONG ht, WORD pl, WORD bc, DWORD comp, DWORD simg) {
    unsigned s = 0;
    unsigned long long h = (unsigned)(ht < 0 ? -ht : ht);
    if (ht == (LONG)0x80000000) h = 0x80000000u;
    if (comp > 10 || !((0x409u >> comp) & 1)) return simg;
    if (!cjscan((unsigned)w, pl, bc, &s)) return 0xffffffffu;
    unsigned long long r = h * s;
    if (r >= 0x100000000ull) return 0xffffffffu;
    return (DWORD)r;
}
static DWORD model_core(WORD w, WORD ht, WORD pl, WORD bc) {
    unsigned s;
    if (!cjscan(w, pl, bc, &s)) return 0xffffffffu;
    unsigned long long r = (unsigned long long)ht * s;
    return r >= 0x100000000ull ? 0xffffffffu : (DWORD)r;
}
#define BB(h) UN(GdiGetBitmapBitsSize)(&(h))

TEST(gap4_bitssize_known_24bpp) {
    RQ(GdiGetBitmapBitsSize); HDRBUF h;
    mkhdr(&h, 3, 2, 1, 24, BI_RGB, 0); EXPECT_EQ(BB(h), 24);
    mkhdr(&h, 1, 1, 1, 24, BI_RGB, 0); EXPECT_EQ(BB(h), 4);
    mkhdr(&h, 4, 1, 1, 24, BI_RGB, 0); EXPECT_EQ(BB(h), 12);
    mkhdr(&h, 5, 7, 1, 24, BI_RGB, 0); EXPECT_EQ(BB(h), 16 * 7);
}
TEST(gap4_bitssize_known_32bpp) {
    RQ(GdiGetBitmapBitsSize); HDRBUF h;
    mkhdr(&h, 10, 10, 1, 32, BI_RGB, 0); EXPECT_EQ(BB(h), 400);
    mkhdr(&h, 1, 1, 1, 32, BI_RGB, 0); EXPECT_EQ(BB(h), 4);
    mkhdr(&h, 640, 480, 1, 32, BI_RGB, 0); EXPECT_EQ(BB(h), 640 * 480 * 4);
}
TEST(gap4_bitssize_known_1bpp) {
    RQ(GdiGetBitmapBitsSize); HDRBUF h;
    mkhdr(&h, 1, 1, 1, 1, BI_RGB, 0); EXPECT_EQ(BB(h), 4);
    mkhdr(&h, 32, 1, 1, 1, BI_RGB, 0); EXPECT_EQ(BB(h), 4);
    mkhdr(&h, 33, 1, 1, 1, BI_RGB, 0); EXPECT_EQ(BB(h), 8);
    mkhdr(&h, 64, 3, 1, 1, BI_RGB, 0); EXPECT_EQ(BB(h), 24);
}
TEST(gap4_bitssize_known_4_8_16bpp) {
    RQ(GdiGetBitmapBitsSize); HDRBUF h;
    mkhdr(&h, 9, 2, 1, 4, BI_RGB, 0); EXPECT_EQ(BB(h), 16);
    mkhdr(&h, 5, 2, 1, 8, BI_RGB, 0); EXPECT_EQ(BB(h), 16);
    mkhdr(&h, 4, 2, 1, 8, BI_RGB, 0); EXPECT_EQ(BB(h), 8);
    mkhdr(&h, 3, 2, 1, 16, BI_RGB, 0); EXPECT_EQ(BB(h), 16);
    mkhdr(&h, 2, 2, 1, 16, BI_RGB, 0); EXPECT_EQ(BB(h), 8);
}
TEST(gap4_bitssize_topdown_uses_abs_height) {
    RQ(GdiGetBitmapBitsSize); HDRBUF h;
    mkhdr(&h, 10, -10, 1, 32, BI_RGB, 0); EXPECT_EQ(BB(h), 400);
    mkhdr(&h, 7, -3, 1, 8, BI_RGB, 0); EXPECT_EQ(BB(h), 24);
    mkhdr(&h, 7, 3, 1, 8, BI_RGB, 0); EXPECT_EQ(BB(h), 24);
}
TEST(gap4_bitssize_zero_dims) {
    RQ(GdiGetBitmapBitsSize); HDRBUF h;
    mkhdr(&h, 10, 0, 1, 32, BI_RGB, 0); EXPECT_EQ(BB(h), 0);
    mkhdr(&h, 0, 10, 1, 32, BI_RGB, 0); EXPECT_EQ(BB(h), 0 /* stride (0+31)>>3 &~3 = 0 */ * 10 + 0);
    mkhdr(&h, 0, 0, 1, 32, BI_RGB, 0); EXPECT_EQ(BB(h), 0);
}
TEST(gap4_bitssize_bitfields) {
    RQ(GdiGetBitmapBitsSize); HDRBUF h;
    mkhdr(&h, 10, 10, 1, 32, BI_BITFIELDS, 0); EXPECT_EQ(BB(h), 400);
    mkhdr(&h, 5, 4, 1, 16, BI_BITFIELDS, 0); EXPECT_EQ(BB(h), 48);
}
TEST(gap4_bitssize_compressed_returns_sizeimage) {
    RQ(GdiGetBitmapBitsSize); HDRBUF h;
    DWORD comps[] = { BI_RLE8, BI_RLE4, BI_JPEG, BI_PNG, 6, 7, 8, 9, 11, 12, 0x12345678 };
    for (unsigned i = 0; i < sizeof comps / sizeof comps[0]; i++) {
        mkhdr(&h, 10, 10, 1, 8, comps[i], 777);
        CHECKF(BB(h) == 777, "comp %lu got %lu", comps[i], BB(h));
        mkhdr(&h, 10, 10, 1, 8, comps[i], 0);
        CHECKF(BB(h) == 0, "comp %lu sizeimage 0", comps[i]);
        mkhdr(&h, 0, 0, 1, 0, comps[i], 0xdeadbeef);
        CHECKF(BB(h) == 0xdeadbeef, "comp %lu big", comps[i]);
    }
}
TEST(gap4_bitssize_sizeimage_ignored_for_rgb) {
    RQ(GdiGetBitmapBitsSize); HDRBUF h;
    mkhdr(&h, 10, 10, 1, 32, BI_RGB, 5); EXPECT_EQ(BB(h), 400);
    mkhdr(&h, 10, 10, 1, 32, BI_RGB, 0xffffffff); EXPECT_EQ(BB(h), 400);
}
TEST(gap4_bitssize_planes_multiplier) {
    RQ(GdiGetBitmapBitsSize); HDRBUF h;
    mkhdr(&h, 8, 2, 4, 1, BI_RGB, 0); EXPECT_EQ(BB(h), model_bi(8, 2, 4, 1, 0, 0));
    mkhdr(&h, 8, 1, 2, 8, BI_RGB, 0); EXPECT_EQ(BB(h), 16);
    mkhdr(&h, 8, 1, 0, 8, BI_RGB, 0); EXPECT_EQ(BB(h), 0);
}
TEST(gap4_bitssize_bitcount_zero_and_odd) {
    RQ(GdiGetBitmapBitsSize); HDRBUF h;
    WORD bcs[] = { 0, 1, 2, 3, 5, 7, 9, 12, 15, 17, 24, 31, 33, 48, 64, 128, 0xffff };
    for (unsigned i = 0; i < sizeof bcs / sizeof bcs[0]; i++) {
        mkhdr(&h, 13, 5, 1, bcs[i], BI_RGB, 0);
        DWORD m = model_bi(13, 5, 1, bcs[i], 0, 0);
        CHECKF(BB(h) == m, "bc %u got %lu want %lu", bcs[i], BB(h), m);
    }
}
TEST(gap4_bitssize_overflow_width) {
    RQ(GdiGetBitmapBitsSize); HDRBUF h;
    mkhdr(&h, 0x7fffffff, 1, 1, 32, BI_RGB, 0); EXPECT_EQ(BB(h), 0xffffffffu);
    mkhdr(&h, 0x40000000, 1, 1, 32, BI_RGB, 0); EXPECT_EQ(BB(h), 0xffffffffu);
    mkhdr(&h, 0x10000000, 1, 1, 8, BI_RGB, 0); EXPECT_EQ(BB(h), model_bi(0x10000000, 1, 1, 8, 0, 0));
    mkhdr(&h, -1, 1, 1, 8, BI_RGB, 0); EXPECT_EQ(BB(h), 0xffffffffu);
}
TEST(gap4_bitssize_overflow_total) {
    RQ(GdiGetBitmapBitsSize); HDRBUF h;
    mkhdr(&h, 0x10000, 0x10000, 1, 32, BI_RGB, 0); EXPECT_EQ(BB(h), 0xffffffffu);
    mkhdr(&h, 0x8000, 0x8000, 1, 32, BI_RGB, 0); EXPECT_EQ(BB(h), 0xffffffffu);
    mkhdr(&h, 0x8000, 0x7fff, 1, 32, BI_RGB, 0); EXPECT_EQ(BB(h), model_bi(0x8000, 0x7fff, 1, 32, 0, 0));
    mkhdr(&h, 4096, 4096, 1, 32, BI_RGB, 0); EXPECT_EQ(BB(h), 64u * 1024 * 1024);
    mkhdr(&h, 100, (LONG)0x80000000, 1, 32, BI_RGB, 0); EXPECT_EQ(BB(h), model_bi(100, (LONG)0x80000000, 1, 32, 0, 0));
    mkhdr(&h, 100, 0x7fffffff, 1, 32, BI_RGB, 0); EXPECT_EQ(BB(h), model_bi(100, 0x7fffffff, 1, 32, 0, 0));
}
TEST(gap4_bitssize_bruteforce_matches_model) {
    RQ(GdiGetBitmapBitsSize); HDRBUF h;
    WORD bcs[] = { 1, 4, 8, 16, 24, 32 }; int bad = 0;
    for (int w = 0; w < 130; w++) for (int b = 0; b < 6; b++) for (int ht = -2; ht < 3; ht++) {
        mkhdr(&h, w, ht, 1, bcs[b], BI_RGB, 0);
        DWORD g = BB(h), m = model_bi(w, ht, 1, bcs[b], 0, 0);
        if (g != m && bad++ < 10) CHECKF(0, "w=%d bc=%u h=%d got %lu want %lu", w, bcs[b], ht, g, m);
    }
    CHECK(bad == 0);
}
TEST(gap4_bitssize_matches_stride_formula) {
    RQ(GdiGetBitmapBitsSize); HDRBUF h; int bad = 0;
    WORD bcs[] = { 1, 4, 8, 16, 24, 32 };
    for (int w = 1; w < 200; w += 3) for (int b = 0; b < 6; b++) {
        mkhdr(&h, w, 5, 1, bcs[b], BI_RGB, 0);
        DWORD want = (((w * bcs[b] + 31) / 32) * 4) * 5;
        if (BB(h) != want) bad++;
    }
    CHECK(bad == 0);
}
TEST(gap4_bitssize_core_header) {
    RQ(GdiGetBitmapBitsSize); HDRBUF h;
    mkcore(&h, 3, 2, 1, 24); EXPECT_EQ(BB(h), 24);
    mkcore(&h, 10, 10, 1, 32); EXPECT_EQ(BB(h), 400);
    mkcore(&h, 33, 2, 1, 1); EXPECT_EQ(BB(h), 16);
    mkcore(&h, 5, 0, 1, 8); EXPECT_EQ(BB(h), 0);
    mkcore(&h, 0, 5, 1, 8); EXPECT_EQ(BB(h), 0);
}
TEST(gap4_bitssize_core_matches_model) {
    RQ(GdiGetBitmapBitsSize); HDRBUF h; int bad = 0;
    WORD bcs[] = { 1, 4, 8, 24, 32, 0, 3, 100 };
    for (int w = 0; w < 80; w += 2) for (int b = 0; b < 8; b++) for (int ht = 0; ht < 4; ht++) {
        mkcore(&h, w, ht, 1, bcs[b]);
        if (BB(h) != model_core(w, ht, 1, bcs[b])) bad++;
    }
    CHECK(bad == 0);
}
TEST(gap4_bitssize_core_big) {
    RQ(GdiGetBitmapBitsSize); HDRBUF h;
    mkcore(&h, 0xffff, 0xffff, 1, 32); EXPECT_EQ(BB(h), model_core(0xffff, 0xffff, 1, 32));
    mkcore(&h, 0xffff, 0xffff, 0xffff, 0xffff); EXPECT_EQ(BB(h), model_core(0xffff, 0xffff, 0xffff, 0xffff));
    mkcore(&h, 0xffff, 1, 1, 32);  EXPECT_EQ(BB(h), 0x3fffc);
}
TEST(gap4_bitssize_dibsection_header_roundtrip) {
    RQ(GdiGetBitmapBitsSize); REQUIRE(U(CreateDIBSection)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectW));
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = 40; bi.bmiHeader.biWidth = 37; bi.bmiHeader.biHeight = -11;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 24; bi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL;
    HBITMAP b = U(CreateDIBSection)(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    REQUIRE(b);
    DIBSECTION ds; CHECK(U(GetObjectW)(b, sizeof ds, &ds) == sizeof ds);
    EXPECT_EQ(BB(ds.dsBmih), ds.dsBm.bmWidthBytes * 11);
    EXPECT_EQ(BB(bi), 112 * 11);
    U(DeleteObject)(b);
}
TEST(gap4_bitssize_v4_v5_headers) {
    RQ(GdiGetBitmapBitsSize);
    BITMAPV5HEADER v5; memset(&v5, 0, sizeof v5);
    v5.bV5Size = sizeof v5; v5.bV5Width = 9; v5.bV5Height = 4; v5.bV5Planes = 1; v5.bV5BitCount = 32; v5.bV5Compression = BI_BITFIELDS;
    EXPECT_EQ(UN(GdiGetBitmapBitsSize)(&v5), 9 * 4 * 4);
    BITMAPV4HEADER v4; memset(&v4, 0, sizeof v4);
    v4.bV4Size = sizeof v4; v4.bV4Width = 9; v4.bV4Height = -4; v4.bV4Planes = 1; v4.bV4BitCount = 8; v4.bV4V4Compression = BI_RGB;
    EXPECT_EQ(UN(GdiGetBitmapBitsSize)(&v4), 12 * 4);
}
TEST(gap4_bitssize_repeat_stable) {
    RQ(GdiGetBitmapBitsSize); HDRBUF h; mkhdr(&h, 17, 9, 1, 24, BI_RGB, 0);
    DWORD first = BB(h);
    for (int i = 0; i < 5000; i++) if (BB(h) != first) { CHECK(0); break; }
    EXPECT_EQ(first, 52 * 9);
}
TEST(gap4_bitssize_no_lasterror_no_leak) {
    RQ(GdiGetBitmapBitsSize); HDRBUF h; mkhdr(&h, 17, 9, 1, 24, BI_RGB, 0);
    SET_ERR(0x777); DWORD g = g4_gdi_count(); BB(h);
    EXPECT_EQ(GetLastError(), 0x777); EXPECT_EQ(g4_gdi_count(), g);
}
TEST(gap4_bitssize_both_modules) {
    G4_WINE_SKIP("no export");
    CHECK(t_proc_gdi32("GdiGetBitmapBitsSize") != NULL);
    CHECK(t_proc_gdi32full("GdiGetBitmapBitsSize") != NULL);
}
