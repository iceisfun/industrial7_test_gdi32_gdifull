#include "bm.h"

#define RGBQ(r,g,b) { (b), (g), (r), 0 }

/* ---- 32 / 24 bpp: exact ---------------------------------------------------- */
static const COLORREF g_cols[] = {
    0x000000, 0xFFFFFF, 0x0000FF, 0x00FF00, 0xFF0000, 0x123456, 0xFEDCBA, 0x010203, 0x808080, 0xC0C0C0, 0x7F7F7F, 0x00FFFF
};
#define NCOLS (sizeof g_cols / sizeof g_cols[0])

static void exact_depth(int bpp) {
    BM_REQ_PIX();
    dib_t d;
    REQUIRE(dib_open(&d, 6, 6, bpp));
    unsigned i;
    for (i = 0; i < NCOLS; i++) {
        int x = i % 6, y = i / 6;
        COLORREF r = U(SetPixel)(d.dc, x, y, g_cols[i]);
        CHECKF(r == g_cols[i], "SetPixel ret %08lx want %08lx", r, g_cols[i]);
        COLORREF g = U(GetPixel)(d.dc, x, y);
        CHECKF(g == g_cols[i], "GetPixel %08lx want %08lx", g, g_cols[i]);
    }
    /* SetPixelV same effect */
    for (i = 0; i < NCOLS; i++) {
        int x = i % 6, y = 3 + i / 6;
        CHECK(U(SetPixelV)(d.dc, x, y, g_cols[i]));
        COLORREF g = U(GetPixel)(d.dc, x, y);
        CHECKF(g == g_cols[i], "SetPixelV GetPixel %08lx want %08lx", g, g_cols[i]);
    }
    dib_close(&d);
}
TEST(bitmaps_pixel_exact_32) { exact_depth(32); }
TEST(bitmaps_pixel_exact_24) { exact_depth(24); }

TEST(bitmaps_pixel_raw_32bpp_layout) {
    BM_REQ_PIX();
    dib_t d;
    REQUIRE(dib_open_ex(&d, 4, 2, 32, 0x100, NULL, 0));   /* top-down */
    dib_fill32(&d, 0xFFFFFFFF);
    U(SetPixelV)(d.dc, 1, 0, RGB(0x11, 0x22, 0x33));
    U(SetPixelV)(d.dc, 3, 1, RGB(0xFF, 0x00, 0x80));
    DWORD p = dib_px32(&d, 1, 0);
    EXPECT_EQ(p & 0x00FFFFFF, 0x00112233);
    p = dib_px32(&d, 3, 1);
    EXPECT_EQ(p & 0x00FFFFFF, 0x00FF0080);
    EXPECT_EQ(dib_px32(&d, 0, 0), 0xFFFFFFFF);
    EXPECT_EQ(dib_px32(&d, 2, 1), 0xFFFFFFFF);
    dib_close(&d);
}
TEST(bitmaps_pixel_raw_bottom_up_orientation) {
    BM_REQ_PIX();
    dib_t d;
    REQUIRE(dib_open(&d, 3, 3, 32));   /* bottom-up: y=0 is the LAST row of memory */
    dib_fill32(&d, 0);
    U(SetPixelV)(d.dc, 0, 0, RGB(1, 2, 3));
    EXPECT_EQ(dib_px32(&d, 0, 2) & 0xFFFFFF, 0x010203);
    EXPECT_EQ(dib_px32(&d, 0, 0) & 0xFFFFFF, 0);
    U(SetPixelV)(d.dc, 2, 2, RGB(4, 5, 6));
    EXPECT_EQ(dib_px32(&d, 2, 0) & 0xFFFFFF, 0x040506);
    dib_close(&d);
}
TEST(bitmaps_pixel_raw_24bpp_bgr) {
    BM_REQ_PIX();
    dib_t d;
    REQUIRE(dib_open_ex(&d, 2, 1, 24, 0x100, NULL, 0));
    memset(d.bits, 0, 8);
    U(SetPixelV)(d.dc, 1, 0, RGB(0xAA, 0xBB, 0xCC));
    U(GdiFlush)();
    EXPECT_EQ(d.bits[3], 0xCC); EXPECT_EQ(d.bits[4], 0xBB); EXPECT_EQ(d.bits[5], 0xAA);
    EXPECT_EQ(d.bits[0], 0);
    dib_close(&d);
}
TEST(bitmaps_pixel_get_reads_memory_written_directly) {
    BM_REQ_PIX();
    dib_t d;
    REQUIRE(dib_open_ex(&d, 3, 1, 32, 0x100, NULL, 0));
    ((DWORD *)d.bits)[0] = 0x00112233; ((DWORD *)d.bits)[1] = 0x00FFEEDD; ((DWORD *)d.bits)[2] = 0;
    U(GdiFlush)();
    EXPECT_EQ(U(GetPixel)(d.dc, 0, 0), RGB(0x11, 0x22, 0x33));
    EXPECT_EQ(U(GetPixel)(d.dc, 1, 0), RGB(0xFF, 0xEE, 0xDD));
    EXPECT_EQ(U(GetPixel)(d.dc, 2, 0), 0);
    dib_close(&d);
}
TEST(bitmaps_pixel_32bpp_alpha_ignored_by_getpixel) {
    BM_REQ_PIX();
    dib_t d;
    REQUIRE(dib_open_ex(&d, 1, 1, 32, 0x100, NULL, 0));
    ((DWORD *)d.bits)[0] = 0x80102030;
    U(GdiFlush)();
    EXPECT_EQ(U(GetPixel)(d.dc, 0, 0), RGB(0x10, 0x20, 0x30));
    dib_close(&d);
}

/* ---- 16 bpp encodings --------------------------------------------------------- */
static const struct { COLORREF c; WORD v555, v565; } k16[] = {
    { RGB(0,0,0),       0x0000, 0x0000 },
    { RGB(255,255,255), 0x7FFF, 0xFFFF },
    { RGB(255,0,0),     0x7C00, 0xF800 },
    { RGB(0,255,0),     0x03E0, 0x07E0 },
    { RGB(0,0,255),     0x001F, 0x001F },
    { RGB(255,255,0),   0x7FE0, 0xFFE0 },
    { RGB(0,255,255),   0x03FF, 0x07FF },
    { RGB(255,0,255),   0x7C1F, 0xF81F },
};
TEST(bitmaps_pixel_16bpp_555_raw) {
    BM_REQ_PIX();
    dib_t d;
    REQUIRE(dib_open_ex(&d, 8, 1, 16, 0x100, NULL, 0));
    unsigned i;
    for (i = 0; i < 8; i++) U(SetPixelV)(d.dc, i, 0, k16[i].c);
    U(GdiFlush)();
    WORD *p = (WORD *)d.bits;
    for (i = 0; i < 8; i++) CHECKF(p[i] == k16[i].v555, "px %u: %04x want %04x", i, p[i], k16[i].v555);
    for (i = 0; i < 8; i++) {
        COLORREF g = U(GetPixel)(d.dc, i, 0);
        CHECKF(g == k16[i].c, "GetPixel %u: %08lx want %08lx", i, g, k16[i].c);
    }
    dib_close(&d);
}
TEST(bitmaps_pixel_16bpp_565_raw) {
    BM_REQ_PIX();
    dib_t d;
    REQUIRE(dib_open_ex(&d, 8, 1, 16, 0x101, NULL, 0));
    unsigned i;
    for (i = 0; i < 8; i++) U(SetPixelV)(d.dc, i, 0, k16[i].c);
    U(GdiFlush)();
    WORD *p = (WORD *)d.bits;
    for (i = 0; i < 8; i++) CHECKF(p[i] == k16[i].v565, "px %u: %04x want %04x", i, p[i], k16[i].v565);
    for (i = 0; i < 8; i++) {
        COLORREF g = U(GetPixel)(d.dc, i, 0);
        CHECKF(g == k16[i].c, "GetPixel %u: %08lx want %08lx", i, g, k16[i].c);
    }
    dib_close(&d);
}
TEST(bitmaps_pixel_16bpp_decode_raw_555) {
    BM_REQ_PIX();
    dib_t d;
    REQUIRE(dib_open_ex(&d, 4, 1, 16, 0x100, NULL, 0));
    WORD *p = (WORD *)d.bits;
    p[0] = 0x7FFF; p[1] = 0x0000; p[2] = 0x7C00; p[3] = 0x001F;
    U(GdiFlush)();
    EXPECT_EQ(U(GetPixel)(d.dc, 0, 0), 0xFFFFFF);
    EXPECT_EQ(U(GetPixel)(d.dc, 1, 0), 0);
    EXPECT_EQ(U(GetPixel)(d.dc, 2, 0), RGB(255, 0, 0));
    EXPECT_EQ(U(GetPixel)(d.dc, 3, 0), RGB(0, 0, 255));
    dib_close(&d);
}
TEST(bitmaps_pixel_16bpp_decode_raw_565) {
    BM_REQ_PIX();
    dib_t d;
    REQUIRE(dib_open_ex(&d, 4, 1, 16, 0x101, NULL, 0));
    WORD *p = (WORD *)d.bits;
    p[0] = 0xFFFF; p[1] = 0x0000; p[2] = 0xF800; p[3] = 0x07E0;
    U(GdiFlush)();
    EXPECT_EQ(U(GetPixel)(d.dc, 0, 0), 0xFFFFFF);
    EXPECT_EQ(U(GetPixel)(d.dc, 1, 0), 0);
    EXPECT_EQ(U(GetPixel)(d.dc, 2, 0), RGB(255, 0, 0));
    EXPECT_EQ(U(GetPixel)(d.dc, 3, 0), RGB(0, 255, 0));
    dib_close(&d);
}
TEST(bitmaps_pixel_16bpp_quantisation_roundtrip) {
    BM_REQ_PIX();
    dib_t d;
    int v;
    REQUIRE(dib_open_ex(&d, 1, 1, 16, 1, NULL, 0));
    /* the colour returned by SetPixel is what GetPixel reads back, and is idempotent */
    for (v = 0; v < 256; v += 5) {
        COLORREF in = RGB(v, 255 - v, (v * 7) & 255);
        COLORREF r = U(SetPixel)(d.dc, 0, 0, in);
        COLORREF g = U(GetPixel)(d.dc, 0, 0);
        CHECKF(r == g, "v=%d set %08lx get %08lx", v, r, g);
        COLORREF r2 = U(SetPixel)(d.dc, 0, 0, g);
        CHECKF(r2 == g, "idempotent v=%d", v);
        /* quantisation error is bounded by the lost bits */
        int dr = abs((int)GetRValue(r) - v), dg = abs((int)GetGValue(r) - (255 - v)), db = abs((int)GetBValue(r) - ((v * 7) & 255));
        CHECKF(dr <= 8 && dg <= 4 && db <= 8, "v=%d err %d %d %d", v, dr, dg, db);
    }
    dib_close(&d);
    REQUIRE(dib_open_ex(&d, 1, 1, 16, 0, NULL, 0));
    for (v = 0; v < 256; v += 5) {
        COLORREF in = RGB(v, 255 - v, (v * 7) & 255);
        COLORREF r = U(SetPixel)(d.dc, 0, 0, in);
        COLORREF g = U(GetPixel)(d.dc, 0, 0);
        CHECKF(r == g, "555 v=%d set %08lx get %08lx", v, r, g);
        int dr = abs((int)GetRValue(r) - v), dg = abs((int)GetGValue(r) - (255 - v)), db = abs((int)GetBValue(r) - ((v * 7) & 255));
        CHECKF(dr <= 8 && dg <= 8 && db <= 8, "555 v=%d err %d %d %d", v, dr, dg, db);
    }
    dib_close(&d);
}
TEST(bitmaps_pixel_16bpp_mid_gray_note) {
    BM_REQ_PIX();
    dib_t d;
    REQUIRE(dib_open_ex(&d, 1, 1, 16, 0, NULL, 0));
    COLORREF r = U(SetPixel)(d.dc, 0, 0, RGB(128, 128, 128));
    U(GdiFlush)();
    t_note("555: gray128 -> %08lx raw %04x", r, ((WORD *)d.bits)[0]);
    dib_close(&d);
    REQUIRE(dib_open_ex(&d, 1, 1, 16, 1, NULL, 0));
    r = U(SetPixel)(d.dc, 0, 0, RGB(128, 128, 128));
    U(GdiFlush)();
    t_note("565: gray128 -> %08lx raw %04x", r, ((WORD *)d.bits)[0]);
    dib_close(&d);
}

/* ---- palettised DIBs: nearest colour ---------------------------------------------- */
static const RGBQUAD pal8[] = { RGBQ(0,0,0), RGBQ(255,0,0), RGBQ(0,255,0), RGBQ(0,0,255), RGBQ(255,255,255) };
static void fill_pal(RGBQUAD *t, int n) {   /* table of n entries: the five above then white */
    int i; for (i = 0; i < n; i++) t[i] = i < 5 ? pal8[i] : pal8[4];
}
TEST(bitmaps_pixel_8bpp_exact_entries) {
    BM_REQ_PIX();
    RGBQUAD t[256]; fill_pal(t, 256);
    dib_t d;
    REQUIRE(dib_open_ex(&d, 4, 4, 8, 0, t, 256));
    static const COLORREF c[] = { RGB(0,0,0), RGB(255,0,0), RGB(0,255,0), RGB(0,0,255) };
    unsigned i;
    for (i = 0; i < 4; i++) {
        COLORREF r = U(SetPixel)(d.dc, i, 0, c[i]);
        CHECKF(r == c[i], "ret %08lx want %08lx", r, c[i]);
        EXPECT_EQ(U(GetPixel)(d.dc, i, 0), c[i]);
    }
    U(GdiFlush)();
    /* bottom-up: y=0 is the last memory row, stride 4 */
    unsigned char *row = d.bits + 3 * 4;
    EXPECT_EQ(row[0], 0); EXPECT_EQ(row[1], 1); EXPECT_EQ(row[2], 2); EXPECT_EQ(row[3], 3);
    dib_close(&d);
}
TEST(bitmaps_pixel_8bpp_nearest) {
    BM_REQ_PIX();
    RGBQUAD t[256]; fill_pal(t, 256);
    dib_t d;
    REQUIRE(dib_open_ex(&d, 4, 4, 8, 0, t, 256));
    struct { COLORREF in, want; } k[] = {
        { RGB(250, 5, 5), RGB(255,0,0) }, { RGB(4, 240, 3), RGB(0,255,0) }, { RGB(3, 3, 250), RGB(0,0,255) },
        { RGB(2, 2, 2), RGB(0,0,0) }, { RGB(240, 250, 245), RGB(255,255,255) }, { RGB(20, 10, 0), RGB(0,0,0) },
    };
    unsigned i;
    for (i = 0; i < 6; i++) {
        COLORREF r = U(SetPixel)(d.dc, 0, 0, k[i].in);
        CHECKF(r == k[i].want, "in %08lx ret %08lx want %08lx", k[i].in, r, k[i].want);
        EXPECT_EQ(U(GetPixel)(d.dc, 0, 0), k[i].want);
    }
    dib_close(&d);
}
TEST(bitmaps_pixel_8bpp_setpixel_ret_equals_getpixel) {
    BM_REQ_PIX();
    RGBQUAD t[256]; int i;
    for (i = 0; i < 256; i++) { t[i].rgbRed = (i * 5) & 255; t[i].rgbGreen = (i * 11) & 255; t[i].rgbBlue = (i * 23) & 255; t[i].rgbReserved = 0; }
    dib_t d;
    REQUIRE(dib_open_ex(&d, 2, 2, 8, 0, t, 256));
    for (i = 0; i < 300; i += 7) {
        COLORREF in = RGB((i * 3) & 255, (i * 9) & 255, (i * 13) & 255);
        COLORREF r = U(SetPixel)(d.dc, 1, 1, in);
        COLORREF g = U(GetPixel)(d.dc, 1, 1);
        CHECKF(r == g, "i=%d ret %08lx get %08lx", i, r, g);
        int found = 0, k;
        for (k = 0; k < 256; k++) if (RGB(t[k].rgbRed, t[k].rgbGreen, t[k].rgbBlue) == r) found = 1;
        CHECKF(found, "returned colour %08lx is in the table", r);
    }
    dib_close(&d);
}
TEST(bitmaps_pixel_4bpp) {
    BM_REQ_PIX();
    RGBQUAD t[16]; fill_pal(t, 16);
    dib_t d;
    REQUIRE(dib_open_ex(&d, 5, 3, 4, 0x100, t, 16));
    static const COLORREF c[] = { RGB(0,0,0), RGB(255,0,0), RGB(0,255,0), RGB(0,0,255) };
    unsigned i;
    for (i = 0; i < 4; i++) {
        EXPECT_EQ(U(SetPixel)(d.dc, i, 1, c[i]), c[i]);
        EXPECT_EQ(U(GetPixel)(d.dc, i, 1), c[i]);
    }
    U(GdiFlush)();
    unsigned char *row = d.bits + 4;   /* stride 4 (5*4=20 bits -> 3 bytes -> dword 4), top-down row 1 */
    EXPECT_EQ(row[0], 0x01); EXPECT_EQ(row[1], 0x23);
    EXPECT_EQ(U(SetPixel)(d.dc, 4, 1, RGB(250, 4, 4)), RGB(255,0,0));
    U(GdiFlush)();
    EXPECT_EQ(row[2] >> 4, 1);
    dib_close(&d);
}
TEST(bitmaps_pixel_1bpp_dib_bw_table) {
    BM_REQ_PIX();
    dib_t d;
    REQUIRE(dib_open_ex(&d, 16, 2, 1, 0x100, NULL, 0));   /* default table: black, white */
    memset(d.bits, 0, 8);
    EXPECT_EQ(U(SetPixel)(d.dc, 0, 0, 0xFFFFFF), 0xFFFFFF);
    EXPECT_EQ(U(SetPixel)(d.dc, 9, 0, 0xFFFFFF), 0xFFFFFF);
    EXPECT_EQ(U(SetPixel)(d.dc, 1, 0, 0x000000), 0);
    U(GdiFlush)();
    EXPECT_EQ(d.bits[0], 0x80); EXPECT_EQ(d.bits[1], 0x40);
    EXPECT_EQ(U(GetPixel)(d.dc, 0, 0), 0xFFFFFF);
    EXPECT_EQ(U(GetPixel)(d.dc, 1, 0), 0);
    EXPECT_EQ(U(SetPixel)(d.dc, 2, 0, RGB(250, 250, 250)), 0xFFFFFF);
    EXPECT_EQ(U(SetPixel)(d.dc, 3, 0, RGB(5, 5, 5)), 0);
    dib_close(&d);
}
TEST(bitmaps_pixel_1bpp_dib_custom_table) {
    BM_REQ_PIX();
    RGBQUAD t[2] = { RGBQ(255,0,0), RGBQ(0,0,255) };
    dib_t d;
    REQUIRE(dib_open_ex(&d, 8, 1, 1, 0x100, t, 2));
    memset(d.bits, 0, 4);
    EXPECT_EQ(U(SetPixel)(d.dc, 0, 0, RGB(0, 0, 255)), RGB(0, 0, 255));
    EXPECT_EQ(U(SetPixel)(d.dc, 1, 0, RGB(255, 0, 0)), RGB(255, 0, 0));
    U(GdiFlush)();
    EXPECT_EQ(d.bits[0] & 0xC0, 0x80);
    EXPECT_EQ(U(GetPixel)(d.dc, 7, 0), RGB(255, 0, 0));
    dib_close(&d);
}

/* ---- bounds, clipping, invalid DC ------------------------------------------------------ */
TEST(bitmaps_pixel_out_of_bounds) {
    BM_REQ_PIX();
    dib_t d;
    REQUIRE(dib_open(&d, 4, 3, 32));
    static const int pts[][2] = { {-1,0},{0,-1},{4,0},{0,3},{4,3},{-1000,-1000},{100000,0},{0,100000},{0x7fffffff,0},{(int)0x80000000,0},{0,0x7fffffff} };
    unsigned i;
    for (i = 0; i < sizeof pts / sizeof pts[0]; i++) {
        int x = pts[i][0], y = pts[i][1];
        CHECKF(U(GetPixel)(d.dc, x, y) == BM_CLR_INVALID, "GetPixel(%d,%d)", x, y);
        CHECKF(U(SetPixel)(d.dc, x, y, 0x123456) == BM_CLR_INVALID, "SetPixel(%d,%d)", x, y);
        CHECKF(!U(SetPixelV)(d.dc, x, y, 0x123456), "SetPixelV(%d,%d)", x, y);
    }
    /* corners inside */
    CHECK(U(GetPixel)(d.dc, 0, 0) != BM_CLR_INVALID);
    CHECK(U(GetPixel)(d.dc, 3, 2) != BM_CLR_INVALID);
    CHECK(U(SetPixelV)(d.dc, 3, 2, 0x010101));
    dib_close(&d);
}
TEST(bitmaps_pixel_out_of_bounds_leaves_memory_alone) {
    BM_REQ_PIX();
    dib_t d;
    REQUIRE(dib_open(&d, 4, 4, 32));
    dib_fill32(&d, 0x00AABBCC);
    U(SetPixelV)(d.dc, -1, 0, 0); U(SetPixelV)(d.dc, 4, 0, 0); U(SetPixelV)(d.dc, 0, 4, 0); U(SetPixelV)(d.dc, 0, -1, 0);
    int i, bad = 0; U(GdiFlush)();
    for (i = 0; i < 16; i++) if (((DWORD *)d.bits)[i] != 0x00AABBCC) bad = 1;
    CHECK(!bad);
    dib_close(&d);
}
TEST(bitmaps_pixel_clip_rect) {
    BM_REQ_PIX(); REQUIRE(U(IntersectClipRect));
    dib_t d;
    REQUIRE(dib_open(&d, 8, 8, 32));
    dib_fill32(&d, 0);
    U(IntersectClipRect)(d.dc, 2, 2, 5, 5);   /* x 2..4, y 2..4 */
    EXPECT_EQ(U(SetPixel)(d.dc, 1, 3, 0x0000FF), BM_CLR_INVALID);
    EXPECT_EQ(U(SetPixel)(d.dc, 5, 3, 0x0000FF), BM_CLR_INVALID);
    EXPECT_EQ(U(SetPixel)(d.dc, 3, 5, 0x0000FF), BM_CLR_INVALID);
    EXPECT_EQ(U(SetPixel)(d.dc, 3, 1, 0x0000FF), BM_CLR_INVALID);
    CHECK(!U(SetPixelV)(d.dc, 0, 0, 0x0000FF));
    EXPECT_EQ(U(SetPixel)(d.dc, 2, 2, 0x0000FF), 0x0000FF);
    EXPECT_EQ(U(SetPixel)(d.dc, 4, 4, 0x00FF00), 0x00FF00);
    CHECK(U(SetPixelV)(d.dc, 3, 3, 0xFF0000));
    EXPECT_EQ(U(GetPixel)(d.dc, 1, 1), BM_CLR_INVALID);
    EXPECT_EQ(U(GetPixel)(d.dc, 5, 5), BM_CLR_INVALID);
    EXPECT_EQ(U(GetPixel)(d.dc, 2, 2), 0x0000FF);
    EXPECT_EQ(U(GetPixel)(d.dc, 3, 3), 0xFF0000);
    /* memory outside the clip untouched */
    int x, y, bad = 0; U(GdiFlush)();
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) {
        int inside = x >= 2 && x < 5 && y >= 2 && y < 5;
        DWORD v = ((DWORD *)d.bits)[(7 - y) * 8 + x];
        if (!inside && v != 0) bad = 1;
    }
    CHECK(!bad);
    dib_close(&d);
}
TEST(bitmaps_pixel_clip_region_empty) {
    BM_REQ_PIX(); REQUIRE(U(IntersectClipRect));
    dib_t d;
    REQUIRE(dib_open(&d, 4, 4, 32));
    U(IntersectClipRect)(d.dc, 0, 0, 0, 0);
    EXPECT_EQ(U(GetPixel)(d.dc, 1, 1), BM_CLR_INVALID);
    EXPECT_EQ(U(SetPixel)(d.dc, 1, 1, 0x0000FF), BM_CLR_INVALID);
    CHECK(!U(SetPixelV)(d.dc, 1, 1, 0x0000FF));
    dib_close(&d);
}
TEST(bitmaps_pixel_clip_region_object) {
    BM_REQ_PIX(); REQUIRE(U(CreateRectRgn)); REQUIRE(U(SelectClipRgn));
    dib_t d;
    REQUIRE(dib_open(&d, 6, 6, 32));
    HRGN rg = U(CreateRectRgn)(1, 1, 3, 3);
    U(SelectClipRgn)(d.dc, rg);
    EXPECT_EQ(U(GetPixel)(d.dc, 0, 0), BM_CLR_INVALID);
    CHECK(U(GetPixel)(d.dc, 1, 1) != BM_CLR_INVALID);
    CHECK(U(GetPixel)(d.dc, 2, 2) != BM_CLR_INVALID);
    EXPECT_EQ(U(GetPixel)(d.dc, 3, 3), BM_CLR_INVALID);
    U(SelectClipRgn)(d.dc, NULL);
    U(DeleteObject)(rg);
    CHECK(U(GetPixel)(d.dc, 3, 3) != BM_CLR_INVALID);
    dib_close(&d);
}
TEST(bitmaps_pixel_origin_offset) {
    BM_REQ_PIX(); REQUIRE(U(SetViewportOrgEx));
    dib_t d;
    REQUIRE(dib_open(&d, 6, 6, 32));
    dib_fill32(&d, 0);
    U(SetViewportOrgEx)(d.dc, 2, 1, NULL);
    EXPECT_EQ(U(SetPixel)(d.dc, 0, 0, 0x0000FF), 0x0000FF);
    EXPECT_EQ(U(GetPixel)(d.dc, 0, 0), 0x0000FF);
    U(SetViewportOrgEx)(d.dc, 0, 0, NULL);
    EXPECT_EQ(U(GetPixel)(d.dc, 2, 1), 0x0000FF);
    EXPECT_EQ(U(GetPixel)(d.dc, 0, 0), 0);
    U(SetViewportOrgEx)(d.dc, 5, 5, NULL);
    EXPECT_EQ(U(GetPixel)(d.dc, 1, 1), BM_CLR_INVALID);
    dib_close(&d);
}
TEST(bitmaps_pixel_invalid_dc) {
    BM_REQ_PIX();
    EXPECT_EQ(U(GetPixel)(NULL, 0, 0), BM_CLR_INVALID);
    EXPECT_EQ(U(SetPixel)(NULL, 0, 0, 0), BM_CLR_INVALID);
    CHECK(!U(SetPixelV)(NULL, 0, 0, 0));
    EXPECT_EQ(U(GetPixel)((HDC)0xdeadbeef, 0, 0), BM_CLR_INVALID);
    EXPECT_EQ(U(SetPixel)((HDC)0xdeadbeef, 0, 0, 0), BM_CLR_INVALID);
    HDC dc = U(CreateCompatibleDC)(NULL);
    U(DeleteDC)(dc);
    EXPECT_EQ(U(GetPixel)(dc, 0, 0), BM_CLR_INVALID);
    EXPECT_EQ(U(SetPixel)(dc, 0, 0, 0), BM_CLR_INVALID);
    CHECK(!U(SetPixelV)(dc, 0, 0, 0));
    HBITMAP b = U(CreateBitmap)(2, 2, 1, 1, NULL);
    EXPECT_EQ(U(GetPixel)((HDC)b, 0, 0), BM_CLR_INVALID);
    U(DeleteObject)(b);
}
TEST(bitmaps_pixel_invalid_dc_last_error) {
    BM_REQ_PIX();
    SET_ERR(0);
    COLORREF c = U(GetPixel)(NULL, 0, 0);
    t_note("GetPixel(NULL) = %08lx gle=%lu", c, GetLastError());
    SET_ERR(0);
    c = U(SetPixel)(NULL, 0, 0, 0);
    t_note("SetPixel(NULL) = %08lx gle=%lu", c, GetLastError());
}
TEST(bitmaps_pixel_default_1x1_bitmap) {
    BM_REQ_PIX();
    HDC dc = U(CreateCompatibleDC)(NULL);
    REQUIRE(dc);
    COLORREF c = U(GetPixel)(dc, 0, 0);
    CHECK(c == 0 || c == 0xFFFFFF);
    EXPECT_EQ(U(GetPixel)(dc, 1, 0), BM_CLR_INVALID);
    EXPECT_EQ(U(GetPixel)(dc, 0, 1), BM_CLR_INVALID);
    EXPECT_EQ(U(GetPixel)(dc, -1, -1), BM_CLR_INVALID);
    U(DeleteDC)(dc);
}
TEST(bitmaps_pixel_stress_roundtrip_32) {
    BM_REQ_PIX();
    dib_t d;
    REQUIRE(dib_open(&d, 32, 32, 32));
    int x, y, bad = 0;
    for (y = 0; y < 32; y++) for (x = 0; x < 32; x++) U(SetPixelV)(d.dc, x, y, RGB(x * 8, y * 8, (x ^ y) * 8));
    for (y = 0; y < 32; y++) for (x = 0; x < 32; x++)
        if (U(GetPixel)(d.dc, x, y) != RGB(x * 8, y * 8, (x ^ y) * 8)) bad++;
    CHECKF(bad == 0, "%d mismatches", bad);
    dib_close(&d);
}
TEST(bitmaps_pixel_colorref_high_byte) {
    BM_REQ_PIX();
    dib_t d;
    REQUIRE(dib_open(&d, 2, 1, 32));
    /* high byte of a COLORREF selects palette-relative / palette-index modes; plain RGB uses 0 */
    COLORREF r = U(SetPixel)(d.dc, 0, 0, 0x00102030);
    EXPECT_EQ(r, 0x00102030);
    r = U(SetPixel)(d.dc, 1, 0, 0xFF102030);
    t_note("SetPixel with high byte 0xFF -> %08lx", r);
    dib_close(&d);
}

/* ---- DDB (device-dependent) pixels --------------------------------------------------------- */
TEST(bitmaps_pixel_mono_ddb) {
    BM_REQ_PIX(); REQUIRE(U(GetBitmapBits));
    ddb_t d;
    REQUIRE(ddb_open(&d, 16, 2, 1, 1, NULL));
    EXPECT_EQ(U(SetPixel)(d.dc, 0, 0, 0xFFFFFF), 0xFFFFFF);
    EXPECT_EQ(U(SetPixel)(d.dc, 9, 1, 0xFFFFFF), 0xFFFFFF);
    EXPECT_EQ(U(SetPixel)(d.dc, 1, 0, 0), 0);
    unsigned char out[4] = {0};
    EXPECT_EQ(U(GetBitmapBits)(d.bm, 4, out), 4);
    EXPECT_EQ(out[0] & 0xC0, 0x80);
    EXPECT_EQ(out[3] & 0x40, 0x40);
    EXPECT_EQ(U(GetPixel)(d.dc, 0, 0), 0xFFFFFF);
    EXPECT_EQ(U(GetPixel)(d.dc, 1, 0), 0);
    EXPECT_EQ(U(GetPixel)(d.dc, 16, 0), BM_CLR_INVALID);
    EXPECT_EQ(U(GetPixel)(d.dc, 0, 2), BM_CLR_INVALID);
    ddb_close(&d);
}
TEST(bitmaps_pixel_mono_ddb_nonbw_colors) {
    BM_REQ_PIX();
    ddb_t d;
    REQUIRE(ddb_open(&d, 8, 1, 1, 1, NULL));
    static const COLORREF c[] = { RGB(250,250,250), RGB(5,5,5), RGB(128,128,128), RGB(255,0,0), RGB(0,255,0), RGB(0,0,255), RGB(255,255,0) };
    unsigned i;
    for (i = 0; i < 7; i++) {
        COLORREF r = U(SetPixel)(d.dc, 0, 0, c[i]);
        t_note("mono ddb SetPixel(%08lx) -> %08lx", c[i], r);
        CHECK(r == 0 || r == 0xFFFFFF);
    }
    ddb_close(&d);
}
TEST(bitmaps_pixel_color_ddb_32_roundtrip) {
    BM_REQ_PIX();
    ddb_t d;
    REQUIRE(ddb_open(&d, 4, 4, 1, 32, NULL));
    if (!d.old) { t_skip("32bpp DDB not selectable"); ddb_close(&d); return; }
    COLORREF r = U(SetPixel)(d.dc, 1, 1, RGB(10, 20, 30));
    COLORREF g = U(GetPixel)(d.dc, 1, 1);
    t_note("32bpp DDB set %08lx get %08lx", r, g);
    CHECK(r == g);
    ddb_close(&d);
}
TEST(bitmaps_pixel_1x1_dib) {
    BM_REQ_PIX();
    dib_t d;
    REQUIRE(dib_open(&d, 1, 1, 32));
    EXPECT_EQ(U(SetPixel)(d.dc, 0, 0, 0x00ABCDEF), 0x00ABCDEF);
    EXPECT_EQ(U(GetPixel)(d.dc, 0, 0), 0x00ABCDEF);
    EXPECT_EQ(U(GetPixel)(d.dc, 1, 0), BM_CLR_INVALID);
    EXPECT_EQ(U(GetPixel)(d.dc, 0, 1), BM_CLR_INVALID);
    dib_close(&d);
}
TEST(bitmaps_pixel_wide_dib_far_edge) {
    BM_REQ_PIX();
    dib_t d;
    REQUIRE(dib_open(&d, 4096, 2, 32));
    EXPECT_EQ(U(SetPixel)(d.dc, 4095, 1, 0x00112233), 0x00112233);
    EXPECT_EQ(U(GetPixel)(d.dc, 4095, 1), 0x00112233);
    EXPECT_EQ(U(GetPixel)(d.dc, 4096, 1), BM_CLR_INVALID);
    dib_close(&d);
}
TEST(bitmaps_pixel_tall_dib_far_edge) {
    BM_REQ_PIX();
    dib_t d;
    REQUIRE(dib_open(&d, 2, 4096, 24));
    EXPECT_EQ(U(SetPixel)(d.dc, 1, 4095, 0x00112233), 0x00112233);
    EXPECT_EQ(U(GetPixel)(d.dc, 1, 4095), 0x00112233);
    EXPECT_EQ(U(GetPixel)(d.dc, 1, 4096), BM_CLR_INVALID);
    dib_close(&d);
}
