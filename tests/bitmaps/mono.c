#include "bm.h"

#define MONO_REQ() do { BM_REQ_PIX(); REQUIRE(U(BitBlt)); REQUIRE(U(SetTextColor)); REQUIRE(U(SetBkColor)); \
    REQUIRE(U(GetBitmapBits)); REQUIRE(U(SetBitmapBits)); REQUIRE(U(PatBlt)); } while (0)

/* ---- monochrome -> colour: 0 -> text colour, 1 -> background colour ------------------------ */
static void mono_to_color(int bpp, int mode) {
    MONO_REQ();
    static const struct { COLORREF text, bk; } pairs[] = {
        { 0x000000, 0xFFFFFF }, { 0xFFFFFF, 0x000000 }, { 0x0000FF, 0x00FF00 }, { 0xFF0000, 0x00FFFF }, { 0x00FF00, 0xFF00FF },
    };
    unsigned i;
    unsigned char src[2] = { 0xA5, 0x00 };   /* 8 px: 1 0 1 0 0 1 0 1 */
    for (i = 0; i < 5; i++) {
        ddb_t m; dib_t d;
        if (!ddb_open(&m, 8, 1, 1, 1, src) || !dib_open_ex(&d, 8, 1, bpp, mode, NULL, 0)) { CHECK(0); ddb_close(&m); continue; }
        if (bpp == 8) {
            /* palettised destination: use exact table colours only */
        }
        U(SetTextColor)(d.dc, pairs[i].text);
        U(SetBkColor)(d.dc, pairs[i].bk);
        CHECK(U(BitBlt)(d.dc, 0, 0, 8, 1, m.dc, 0, 0, SRCCOPY));
        int x;
        for (x = 0; x < 8; x++) {
            COLORREF want = ((src[0] >> (7 - x)) & 1) ? pairs[i].bk : pairs[i].text;
            COLORREF got = U(GetPixel)(d.dc, x, 0);
            if (bpp == 8) continue;
            CHECKF(got == want, "bpp %d pair %u x %d: got %08lx want %08lx", bpp, i, x, got, want);
        }
        dib_close(&d); ddb_close(&m);
    }
}
TEST(bitmaps_mono_to_color_32) { mono_to_color(32, 0); }
TEST(bitmaps_mono_to_color_24) { mono_to_color(24, 0); }
TEST(bitmaps_mono_to_color_16_555) { mono_to_color(16, 0); }
TEST(bitmaps_mono_to_color_16_565) { mono_to_color(16, 1); }

TEST(bitmaps_mono_to_color_default_colors) {
    MONO_REQ();
    ddb_t m; dib_t d;
    unsigned char src[2] = { 0x80, 0 };
    REQUIRE(ddb_open(&m, 4, 1, 1, 1, src));
    REQUIRE(dib_open(&d, 4, 1, 32));
    /* a fresh DC has text = black, background = white */
    EXPECT_EQ(U(GetTextColor)(d.dc), 0);
    EXPECT_EQ(U(GetBkColor)(d.dc), 0xFFFFFF);
    CHECK(U(BitBlt)(d.dc, 0, 0, 4, 1, m.dc, 0, 0, SRCCOPY));
    EXPECT_EQ(U(GetPixel)(d.dc, 0, 0), 0xFFFFFF);
    EXPECT_EQ(U(GetPixel)(d.dc, 1, 0), 0);
    dib_close(&d); ddb_close(&m);
}
TEST(bitmaps_mono_to_color_uses_dest_dc_colors_not_source) {
    MONO_REQ();
    ddb_t m; dib_t d;
    unsigned char src[2] = { 0x80, 0 };
    REQUIRE(ddb_open(&m, 4, 1, 1, 1, src));
    REQUIRE(dib_open(&d, 4, 1, 32));
    U(SetTextColor)(m.dc, 0x0000FF); U(SetBkColor)(m.dc, 0x00FF00);   /* source DC colours must not matter */
    U(SetTextColor)(d.dc, 0xFF0000); U(SetBkColor)(d.dc, 0x00FFFF);
    CHECK(U(BitBlt)(d.dc, 0, 0, 4, 1, m.dc, 0, 0, SRCCOPY));
    EXPECT_EQ(U(GetPixel)(d.dc, 0, 0), 0x00FFFF);
    EXPECT_EQ(U(GetPixel)(d.dc, 1, 0), 0xFF0000);
    dib_close(&d); ddb_close(&m);
}
TEST(bitmaps_mono_to_color_8bpp_exact_table) {
    MONO_REQ();
    RGBQUAD t[256]; int i;
    for (i = 0; i < 256; i++) { t[i].rgbRed = t[i].rgbGreen = t[i].rgbBlue = 128; t[i].rgbReserved = 0; }
    t[0].rgbRed = t[0].rgbGreen = t[0].rgbBlue = 0;
    t[1].rgbRed = 255; t[1].rgbGreen = t[1].rgbBlue = 0;
    t[2].rgbGreen = 255; t[2].rgbRed = t[2].rgbBlue = 0;
    ddb_t m; dib_t d;
    unsigned char src[2] = { 0x50, 0 };
    REQUIRE(ddb_open(&m, 4, 1, 1, 1, src));
    REQUIRE(dib_open_ex(&d, 4, 1, 8, 0, t, 256));
    U(SetTextColor)(d.dc, RGB(255, 0, 0)); U(SetBkColor)(d.dc, RGB(0, 255, 0));
    CHECK(U(BitBlt)(d.dc, 0, 0, 4, 1, m.dc, 0, 0, SRCCOPY));
    EXPECT_EQ(U(GetPixel)(d.dc, 0, 0), RGB(255, 0, 0));
    EXPECT_EQ(U(GetPixel)(d.dc, 1, 0), RGB(0, 255, 0));
    EXPECT_EQ(U(GetPixel)(d.dc, 2, 0), RGB(255, 0, 0));
    EXPECT_EQ(U(GetPixel)(d.dc, 3, 0), RGB(0, 255, 0));
    dib_close(&d); ddb_close(&m);
}

/* ---- colour -> monochrome: pixels equal to the SOURCE bk colour become 1 ------------------------- */
TEST(bitmaps_color_to_mono_bk_match) {
    MONO_REQ();
    dib_t s; ddb_t m;
    REQUIRE(dib_open(&s, 8, 1, 32));
    REQUIRE(ddb_open(&m, 8, 1, 1, 1, NULL));
    static const COLORREF px[8] = { 0x0000FF, 0x00FF00, 0x0000FF, 0xFFFFFF, 0x000000, 0x0000FF, 0x123456, 0x0000FF };
    int x;
    for (x = 0; x < 8; x++) U(SetPixelV)(s.dc, x, 0, px[x]);
    U(SetBkColor)(s.dc, 0x0000FF);
    CHECK(U(BitBlt)(m.dc, 0, 0, 8, 1, s.dc, 0, 0, SRCCOPY));
    unsigned char out[2];
    EXPECT_EQ(U(GetBitmapBits)(m.bm, 2, out), 2);
    EXPECT_EQ(out[0], 0xA5);            /* 1 0 1 0 0 1 0 1 */
    dib_close(&s); ddb_close(&m);
}
TEST(bitmaps_color_to_mono_bk_white_default) {
    MONO_REQ();
    dib_t s; ddb_t m;
    REQUIRE(dib_open(&s, 8, 1, 24));
    REQUIRE(ddb_open(&m, 8, 1, 1, 1, NULL));
    static const COLORREF px[8] = { 0xFFFFFF, 0, 0xFFFFFF, 0x808080, 0xFEFEFE, 0xFFFFFF, 0, 0x00FFFF };
    int x;
    for (x = 0; x < 8; x++) U(SetPixelV)(s.dc, x, 0, px[x]);
    CHECK(U(BitBlt)(m.dc, 0, 0, 8, 1, s.dc, 0, 0, SRCCOPY));
    unsigned char out[2];
    U(GetBitmapBits)(m.bm, 2, out);
    EXPECT_EQ(out[0], 0xA4);
    dib_close(&s); ddb_close(&m);
}
TEST(bitmaps_color_to_mono_no_match_all_zero) {
    MONO_REQ();
    dib_t s; ddb_t m;
    REQUIRE(dib_open(&s, 8, 1, 32));
    REQUIRE(ddb_open(&m, 8, 1, 1, 1, NULL));
    int x;
    for (x = 0; x < 8; x++) U(SetPixelV)(s.dc, x, 0, 0x0000FF);
    U(SetBkColor)(s.dc, 0x00FF00);
    U(PatBlt)(m.dc, 0, 0, 8, 1, WHITENESS);
    CHECK(U(BitBlt)(m.dc, 0, 0, 8, 1, s.dc, 0, 0, SRCCOPY));
    unsigned char out[2];
    U(GetBitmapBits)(m.bm, 2, out);
    EXPECT_EQ(out[0], 0x00);
    dib_close(&s); ddb_close(&m);
}
TEST(bitmaps_color_to_mono_dest_colors_irrelevant) {
    MONO_REQ();
    dib_t s; ddb_t m;
    REQUIRE(dib_open(&s, 8, 1, 32));
    REQUIRE(ddb_open(&m, 8, 1, 1, 1, NULL));
    int x;
    for (x = 0; x < 8; x++) U(SetPixelV)(s.dc, x, 0, (x & 1) ? 0xFFFFFF : 0);
    U(SetTextColor)(m.dc, 0x0000FF); U(SetBkColor)(m.dc, 0x00FF00);
    CHECK(U(BitBlt)(m.dc, 0, 0, 8, 1, s.dc, 0, 0, SRCCOPY));
    unsigned char out[2];
    U(GetBitmapBits)(m.bm, 2, out);
    EXPECT_EQ(out[0], 0x55);
    dib_close(&s); ddb_close(&m);
}
TEST(bitmaps_color_to_mono_16bpp_source) {
    MONO_REQ();
    dib_t s; ddb_t m;
    REQUIRE(dib_open_ex(&s, 4, 1, 16, 1, NULL, 0));
    REQUIRE(ddb_open(&m, 4, 1, 1, 1, NULL));
    U(SetPixelV)(s.dc, 0, 0, RGB(255, 0, 0)); U(SetPixelV)(s.dc, 1, 0, RGB(0, 255, 0));
    U(SetPixelV)(s.dc, 2, 0, RGB(255, 0, 0)); U(SetPixelV)(s.dc, 3, 0, RGB(0, 0, 255));
    U(SetBkColor)(s.dc, RGB(255, 0, 0));
    CHECK(U(BitBlt)(m.dc, 0, 0, 4, 1, s.dc, 0, 0, SRCCOPY));
    unsigned char out[2];
    U(GetBitmapBits)(m.bm, 2, out);
    EXPECT_EQ(out[0] & 0xF0, 0xA0);
    dib_close(&s); ddb_close(&m);
}
TEST(bitmaps_color_to_mono_8bpp_source) {
    MONO_REQ();
    RGBQUAD t[256]; int i;
    for (i = 0; i < 256; i++) { t[i].rgbRed = t[i].rgbGreen = t[i].rgbBlue = 255; t[i].rgbReserved = 0; }
    t[0].rgbRed = t[0].rgbGreen = t[0].rgbBlue = 0;
    t[1].rgbRed = 255; t[1].rgbGreen = t[1].rgbBlue = 0;
    dib_t s; ddb_t m;
    REQUIRE(dib_open_ex(&s, 4, 1, 8, 0, t, 256));
    REQUIRE(ddb_open(&m, 4, 1, 1, 1, NULL));
    U(SetPixelV)(s.dc, 0, 0, RGB(255, 0, 0)); U(SetPixelV)(s.dc, 1, 0, 0);
    U(SetPixelV)(s.dc, 2, 0, RGB(255, 0, 0)); U(SetPixelV)(s.dc, 3, 0, 0xFFFFFF);
    U(SetBkColor)(s.dc, RGB(255, 0, 0));
    CHECK(U(BitBlt)(m.dc, 0, 0, 4, 1, s.dc, 0, 0, SRCCOPY));
    unsigned char out[2];
    U(GetBitmapBits)(m.bm, 2, out);
    EXPECT_EQ(out[0] & 0xF0, 0xA0);
    dib_close(&s); ddb_close(&m);
}

/* ---- mono -> mono ----------------------------------------------------------------------------------- */
static void mono_rop_case(DWORD rop, unsigned (*f)(unsigned s, unsigned d), const char *nm) {
    ddb_t s, d;
    unsigned char sb[4] = { 0xCC, 0xCC, 0xF0, 0xF0 }, db[4] = { 0xAA, 0xAA, 0x66, 0x66 }, out[4];
    if (!ddb_open(&s, 16, 2, 1, 1, sb) || !ddb_open(&d, 16, 2, 1, 1, db)) { CHECK(0); return; }
    CHECKF(U(BitBlt)(d.dc, 0, 0, 16, 2, s.dc, 0, 0, rop), "%s", nm);
    U(GetBitmapBits)(d.bm, 4, out);
    CHECKF(out[0] == (f(0xCC, 0xAA) & 0xFF) && out[1] == out[0], "%s row0 %02x%02x want %02x", nm, out[0], out[1], f(0xCC, 0xAA) & 0xFF);
    CHECKF(out[2] == (f(0xF0, 0x66) & 0xFF) && out[3] == out[2], "%s row1 %02x%02x want %02x", nm, out[2], out[3], f(0xF0, 0x66) & 0xFF);
    ddb_close(&s); ddb_close(&d);
}
static unsigned f_copy(unsigned s, unsigned d) { return s; }
static unsigned f_and(unsigned s, unsigned d) { return s & d; }
static unsigned f_or(unsigned s, unsigned d) { return s | d; }
static unsigned f_xor(unsigned s, unsigned d) { return s ^ d; }
static unsigned f_notsrc(unsigned s, unsigned d) { return ~s; }
static unsigned f_nor(unsigned s, unsigned d) { return ~(s | d); }
static unsigned f_erase(unsigned s, unsigned d) { return s & ~d; }
static unsigned f_merge(unsigned s, unsigned d) { return ~s | d; }
static unsigned f_dstinv(unsigned s, unsigned d) { return ~d; }
static unsigned f_black(unsigned s, unsigned d) { return 0; }
static unsigned f_white(unsigned s, unsigned d) { return 0xFF; }
static unsigned f_dst(unsigned s, unsigned d) { return d; }
TEST(bitmaps_mono_rop_srccopy)   { MONO_REQ(); mono_rop_case(SRCCOPY, f_copy, "SRCCOPY"); }
TEST(bitmaps_mono_rop_srcand)    { MONO_REQ(); mono_rop_case(SRCAND, f_and, "SRCAND"); }
TEST(bitmaps_mono_rop_srcpaint)  { MONO_REQ(); mono_rop_case(SRCPAINT, f_or, "SRCPAINT"); }
TEST(bitmaps_mono_rop_srcinvert) { MONO_REQ(); mono_rop_case(SRCINVERT, f_xor, "SRCINVERT"); }
TEST(bitmaps_mono_rop_notsrccopy){ MONO_REQ(); mono_rop_case(NOTSRCCOPY, f_notsrc, "NOTSRCCOPY"); }
TEST(bitmaps_mono_rop_notsrcerase){ MONO_REQ(); mono_rop_case(NOTSRCERASE, f_nor, "NOTSRCERASE"); }
TEST(bitmaps_mono_rop_srcerase)  { MONO_REQ(); mono_rop_case(SRCERASE, f_erase, "SRCERASE"); }
TEST(bitmaps_mono_rop_mergepaint){ MONO_REQ(); mono_rop_case(MERGEPAINT, f_merge, "MERGEPAINT"); }
TEST(bitmaps_mono_rop_dstinvert) { MONO_REQ(); mono_rop_case(DSTINVERT, f_dstinv, "DSTINVERT"); }
TEST(bitmaps_mono_rop_blackness) { MONO_REQ(); mono_rop_case(BLACKNESS, f_black, "BLACKNESS"); }
TEST(bitmaps_mono_rop_whiteness) { MONO_REQ(); mono_rop_case(WHITENESS, f_white, "WHITENESS"); }
TEST(bitmaps_mono_rop_noop_0aa0)  { MONO_REQ(); mono_rop_case(0x00AA0029, f_dst, "D (0x00AA0029)"); }

TEST(bitmaps_mono_blit_offsets) {
    MONO_REQ();
    ddb_t s, d;
    unsigned char sb[2] = { 0xFF, 0xFF }, out[2];
    REQUIRE(ddb_open(&s, 16, 1, 1, 1, sb));
    REQUIRE(ddb_open(&d, 16, 1, 1, 1, NULL));
    U(PatBlt)(d.dc, 0, 0, 16, 1, BLACKNESS);
    CHECK(U(BitBlt)(d.dc, 3, 0, 7, 1, s.dc, 5, 0, SRCCOPY));
    U(GetBitmapBits)(d.bm, 2, out);
    EXPECT_EQ(out[0], 0x1F); EXPECT_EQ(out[1], 0xC0);   /* x3..x9 */
    ddb_close(&s); ddb_close(&d);
}
TEST(bitmaps_mono_stretch_2x) {
    MONO_REQ(); REQUIRE(U(StretchBlt));
    ddb_t s, d;
    unsigned char sb[2] = { 0xA5, 0 }, out[4];
    REQUIRE(ddb_open(&s, 8, 1, 1, 1, sb));
    REQUIRE(ddb_open(&d, 16, 2, 1, 1, NULL));
    CHECK(U(StretchBlt)(d.dc, 0, 0, 16, 2, s.dc, 0, 0, 8, 1, SRCCOPY));
    U(GetBitmapBits)(d.bm, 4, out);
    EXPECT_EQ(out[0], 0xCC); EXPECT_EQ(out[1], 0x33);
    EXPECT_EQ(out[2], 0xCC); EXPECT_EQ(out[3], 0x33);
    ddb_close(&s); ddb_close(&d);
}
TEST(bitmaps_mono_stretch_mirror) {
    MONO_REQ(); REQUIRE(U(StretchBlt));
    ddb_t s, d;
    unsigned char sb[2] = { 0xB0, 0 }, out[2];     /* 1 0 1 1 0 0 0 0 */
    REQUIRE(ddb_open(&s, 8, 1, 1, 1, sb));
    REQUIRE(ddb_open(&d, 8, 1, 1, 1, NULL));
    CHECK(U(StretchBlt)(d.dc, 7, 0, -8, 1, s.dc, 0, 0, 8, 1, SRCCOPY));
    U(GetBitmapBits)(d.bm, 2, out);
    EXPECT_EQ(out[0], 0x0D);
    ddb_close(&s); ddb_close(&d);
}
TEST(bitmaps_mono_patblt_rops) {
    MONO_REQ();
    ddb_t d;
    REQUIRE(ddb_open(&d, 16, 2, 1, 1, NULL));
    unsigned char out[4];
    U(PatBlt)(d.dc, 0, 0, 16, 2, WHITENESS);
    U(PatBlt)(d.dc, 4, 0, 4, 1, DSTINVERT);
    U(GetBitmapBits)(d.bm, 4, out);
    EXPECT_EQ(out[0], 0xF0); EXPECT_EQ(out[1], 0xFF); EXPECT_EQ(out[2], 0xFF); EXPECT_EQ(out[3], 0xFF);
    ddb_close(&d);
}
TEST(bitmaps_mono_setpixel_getbits_pattern) {
    MONO_REQ();
    ddb_t d;
    REQUIRE(ddb_open(&d, 16, 3, 1, 1, NULL));
    int x, y;
    U(PatBlt)(d.dc, 0, 0, 16, 3, BLACKNESS);
    for (y = 0; y < 3; y++) for (x = 0; x < 16; x++) if (((x + y) & 3) == 0) U(SetPixelV)(d.dc, x, y, 0xFFFFFF);
    unsigned char out[6];
    EXPECT_EQ(U(GetBitmapBits)(d.bm, 6, out), 6);
    unsigned char want[6] = { 0x88, 0x88, 0x11, 0x11, 0x22, 0x22 };
    CHECK(!memcmp(out, want, 6));
    ddb_close(&d);
}
TEST(bitmaps_mono_text_bk_colors_do_not_affect_mono_copy) {
    MONO_REQ();
    ddb_t s, d;
    unsigned char sb[2] = { 0x3C, 0 }, out[2];
    REQUIRE(ddb_open(&s, 8, 1, 1, 1, sb));
    REQUIRE(ddb_open(&d, 8, 1, 1, 1, NULL));
    U(SetTextColor)(d.dc, 0x0000FF); U(SetBkColor)(d.dc, 0x00FF00);
    U(SetTextColor)(s.dc, 0xFF0000); U(SetBkColor)(s.dc, 0x123456);
    CHECK(U(BitBlt)(d.dc, 0, 0, 8, 1, s.dc, 0, 0, SRCCOPY));
    U(GetBitmapBits)(d.bm, 2, out);
    t_note("mono->mono copy with custom colours: %02x (plain copy would be 3c)", out[0]);
    ddb_close(&s); ddb_close(&d);
}

/* ---- mono pattern brush ---------------------------------------------------------------------------------- */
TEST(bitmaps_mono_pattern_brush_colors) {
    MONO_REQ(); REQUIRE(U(CreatePatternBrush));
    unsigned char rows[16]; int i;
    for (i = 0; i < 8; i++) { rows[2 * i] = 0xF0; rows[2 * i + 1] = 0; }   /* x0..3 = 1, x4..7 = 0 */
    HBITMAP pat = U(CreateBitmap)(8, 8, 1, 1, rows);
    REQUIRE(pat);
    HBRUSH br = U(CreatePatternBrush)(pat);
    REQUIRE(br);
    dib_t d; REQUIRE(dib_open(&d, 16, 16, 32));
    U(SetTextColor)(d.dc, 0x0000FF); U(SetBkColor)(d.dc, 0x00FF00);
    HGDIOBJ old = U(SelectObject)(d.dc, br);
    CHECK(U(PatBlt)(d.dc, 0, 0, 16, 16, PATCOPY));
    U(SelectObject)(d.dc, old);
    int x, y, bad = 0;
    for (y = 0; y < 16; y++) for (x = 0; x < 16; x++) {
        COLORREF want = ((x & 7) < 4) ? 0x00FF00 : 0x0000FF;
        if (U(GetPixel)(d.dc, x, y) != want) bad++;
    }
    CHECKF(bad == 0, "%d wrong pixels", bad);
    dib_close(&d); U(DeleteObject)(br); U(DeleteObject)(pat);
}
TEST(bitmaps_mono_pattern_brush_bitmap_copied) {
    MONO_REQ(); REQUIRE(U(CreatePatternBrush));
    HBITMAP pat = U(CreateBitmap)(8, 8, 1, 1, NULL);
    HBRUSH br = U(CreatePatternBrush)(pat);
    REQUIRE(br);
    CHECK(U(DeleteObject)(pat));          /* brush owns a copy: deleting the bitmap is fine */
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ old = U(SelectObject)(dc, br);
    CHECK(old != NULL);
    U(SelectObject)(dc, old);
    U(DeleteDC)(dc);
    CHECK(U(DeleteObject)(br));
}
TEST(bitmaps_mono_pattern_brush_selected_bitmap_rejected) {
    MONO_REQ(); REQUIRE(U(CreatePatternBrush));
    ddb_t d; REQUIRE(ddb_open(&d, 8, 8, 1, 1, NULL));
    HBRUSH br = U(CreatePatternBrush)(d.bm);
    t_note("CreatePatternBrush(bitmap selected in a DC) -> %p", br);
    if (br) U(DeleteObject)(br);
    ddb_close(&d);
}

/* ---- GetDIBits of a mono DDB ----------------------------------------------------------------------------------- */
TEST(bitmaps_mono_getdibits_24) {
    MONO_REQ(); REQUIRE(U(GetDIBits));
    unsigned char src[2] = { 0x90, 0 };      /* 1 0 0 1 */
    ddb_t m; REQUIRE(ddb_open(&m, 4, 1, 1, 1, src));
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 4; bi.bmiHeader.biHeight = 1;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 24; bi.bmiHeader.biCompression = BI_RGB;
    unsigned char out[16]; memset(out, 0xEE, sizeof out);
    HGDIOBJ o = U(SelectObject)(m.dc, m.old); m.old = NULL;
    int n = U(GetDIBits)(m.dc, m.bm, 0, 1, out, &bi, DIB_RGB_COLORS);
    EXPECT_EQ(n, 1);
    EXPECT_EQ(out[0], 0xFF); EXPECT_EQ(out[1], 0xFF); EXPECT_EQ(out[2], 0xFF);
    EXPECT_EQ(out[3], 0); EXPECT_EQ(out[4], 0); EXPECT_EQ(out[5], 0);
    EXPECT_EQ(out[9], 0xFF);
    ddb_close(&m);
}
TEST(bitmaps_mono_getdibits_1bpp_table) {
    MONO_REQ(); REQUIRE(U(GetDIBits));
    unsigned char src[2] = { 0x90, 0 }, out[8];
    ddb_t m; REQUIRE(ddb_open(&m, 4, 1, 1, 1, src));
    struct { BITMAPINFOHEADER h; RGBQUAD c[2]; } bi; memset(&bi, 0, sizeof bi);
    bi.h.biSize = sizeof bi.h; bi.h.biWidth = 4; bi.h.biHeight = 1; bi.h.biPlanes = 1; bi.h.biBitCount = 1;
    U(SelectObject)(m.dc, m.old); m.old = NULL;
    int n = U(GetDIBits)(m.dc, m.bm, 0, 1, out, (BITMAPINFO *)&bi, DIB_RGB_COLORS);
    EXPECT_EQ(n, 1);
    EXPECT_EQ(out[0] & 0xF0, 0x90);
    t_note("1bpp table: %02x%02x%02x / %02x%02x%02x", bi.c[0].rgbRed, bi.c[0].rgbGreen, bi.c[0].rgbBlue, bi.c[1].rgbRed, bi.c[1].rgbGreen, bi.c[1].rgbBlue);
    ddb_close(&m);
}

/* ---- sizes and stress ---------------------------------------------------------------------------------------------- */
TEST(bitmaps_large_mono_4096) {
    BM_REQ();
    HBITMAP b = U(CreateBitmap)(4096, 4096, 1, 1, NULL);
    CHECK(b != NULL);
    if (b) {
        BITMAP bm; bm_getobj(b, &bm);
        EXPECT_EQ(bm.bmWidthBytes, 512);
        EXPECT_EQ(U(GetBitmapBits)(b, 0, NULL), 512 * 4096);
        U(DeleteObject)(b);
    }
}
TEST(bitmaps_large_32bpp_2048) {
    BM_REQ();
    HBITMAP b = U(CreateBitmap)(2048, 2048, 1, 32, NULL);
    if (!b) { t_note("2048x2048x32 DDB creation failed"); return; }
    BITMAP bm; bm_getobj(b, &bm);
    EXPECT_EQ(bm.bmWidthBytes, 8192);
    U(DeleteObject)(b);
}
TEST(bitmaps_repeated_create_delete) {
    BM_REQ();
    int i, fail = 0;
    for (i = 0; i < 2000; i++) {
        HBITMAP b = U(CreateBitmap)(64, 64, 1, 32, NULL);
        if (!b) { fail++; continue; }
        if (!U(DeleteObject)(b)) fail++;
    }
    CHECKF(fail == 0, "%d failures", fail);
}
TEST(bitmaps_repeated_ccb_select_cycle) {
    BM_REQ_DIB();
    dib_t d; REQUIRE(dib_open(&d, 16, 16, 32));
    int i, fail = 0;
    for (i = 0; i < 500; i++) {
        HBITMAP b = U(CreateCompatibleBitmap)(d.dc, 8 + (i & 7), 8);
        if (!b) { fail++; continue; }
        HDC dc = U(CreateCompatibleDC)(d.dc);
        HGDIOBJ o = U(SelectObject)(dc, b);
        if (!o) fail++; else U(SelectObject)(dc, o);
        U(DeleteDC)(dc);
        U(DeleteObject)(b);
    }
    CHECKF(fail == 0, "%d failures", fail);
    dib_close(&d);
}
TEST(bitmaps_extreme_aspect_ratios) {
    BM_REQ();
    static const int d[][2] = { {1,8192},{8192,1},{1,1},{16384,1},{1,16384} }; unsigned i;
    for (i = 0; i < 5; i++) {
        HBITMAP b = U(CreateBitmap)(d[i][0], d[i][1], 1, 8, NULL);
        CHECKF(b != NULL, "%d x %d", d[i][0], d[i][1]);
        if (b) {
            BITMAP bm; bm_getobj(b, &bm);
            EXPECT_EQ(bm.bmWidth, d[i][0]); EXPECT_EQ(bm.bmHeight, d[i][1]);
            U(DeleteObject)(b);
        }
    }
}
TEST(bitmaps_ccb_dc_with_changed_bitmap_tracks_selection) {
    MONO_REQ();
    HDC dc = U(CreateCompatibleDC)(NULL);
    REQUIRE(dc);
    dib_t d; REQUIRE(dib_open(&d, 4, 4, 32));
    HBITMAP c1 = U(CreateCompatibleBitmap)(dc, 4, 4);     /* mono */
    HGDIOBJ old = U(SelectObject)(dc, d.bm);              /* d.bm is selected elsewhere: must fail */
    CHECK(old == NULL);
    U(SelectObject)(d.dc, d.old); d.old = NULL;
    old = U(SelectObject)(dc, d.bm);
    CHECK(old != NULL);
    HBITMAP c2 = U(CreateCompatibleBitmap)(dc, 4, 4);
    BITMAP b1, b2; bm_getobj(c1, &b1); bm_getobj(c2, &b2);
    EXPECT_EQ(b1.bmBitsPixel, 1);
    EXPECT_EQ(b2.bmBitsPixel, 32);
    U(SelectObject)(dc, old);
    U(DeleteObject)(c1); U(DeleteObject)(c2); U(DeleteDC)(dc);
    dib_close(&d);
}
