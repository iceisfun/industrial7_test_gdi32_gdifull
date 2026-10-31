#include "dibh.h"

#define NEED_DRAW() do { NEED_GDI(); REQUIRE(U(SetDIBitsToDevice)); REQUIRE(U(StretchDIBits)); \
    REQUIRE(U(GetDIBColorTable)); REQUIRE(U(SetDIBColorTable)); } while (0)

static COLORREF src_col(int x, int y) { return RGB(x * 30 + 7, y * 25 + 3, (x ^ y) * 17 + 9); }
/* 24bpp bottom-up image w x h with src_col(x, y) at top-based (x, y) */
static void img24(BYTE *buf, int w, int h) {
    int st = dib_stride(w, 24);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        BYTE *p = buf + (h - 1 - y) * st + x * 3; COLORREF c = src_col(x, y);
        p[0] = GetBValue(c); p[1] = GetGValue(c); p[2] = GetRValue(c);
    }
}
/* dest: 32bpp memory DC w x h, bottom-up, cleared to 0x00FFFFFF-ish pattern */
typedef struct { HBITMAP bm; HDC dc; HGDIOBJ old; void *bits; int w, h; } DST;
static int dst_open(DST *d, int w, int h, int bpp, COLORREF fill) {
    memset(d, 0, sizeof *d); d->w = w; d->h = h;
    d->bm = mkdib(w, h, bpp, &d->bits); if (!d->bm) return 0;
    d->dc = mkdc(d->bm, &d->old); if (!d->dc) return 0;
    if (fill != 0 && bpp >= 16) {
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) U(SetPixelV)(d->dc, x, y, fill);
    }
    return 1;
}
static void dst_close(DST *d) { if (d->dc) freedc(d->dc, d->old); if (d->bm) U(DeleteObject)(d->bm); }
static COLORREF gp(DST *d, int x, int y) { U(GdiFlush)(); return U(GetPixel)(d->dc, x, y); }

/* ---------- SetDIBitsToDevice ---------- */
TEST(dib_sdtd_full_copy_24) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 8, 8, 32, 0));
    BMI b; bmi_init(&b, 8, 8, 24, BI_RGB); BYTE src[24 * 8]; img24(src, 8, 8);
    int r = U(SetDIBitsToDevice)(d.dc, 0, 0, 8, 8, 0, 0, 0, 8, src, PB(&b), DIB_RGB_COLORS);
    EXPECT_EQ(r, 8);
    int bad = 0;
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) if (gp(&d, x, y) != src_col(x, y)) bad++;
    CHECKF(bad == 0, "bad %d", bad);
    dst_close(&d);
}
TEST(dib_sdtd_full_copy_topdown_source) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 8, 8, 32, 0));
    BMI b; bmi_init(&b, 8, -8, 24, BI_RGB); BYTE tmp[24 * 8], src[24 * 8]; img24(tmp, 8, 8);
    for (int y = 0; y < 8; y++) memcpy(src + y * 24, tmp + (7 - y) * 24, 24);
    int r = U(SetDIBitsToDevice)(d.dc, 0, 0, 8, 8, 0, 0, 0, 8, src, PB(&b), DIB_RGB_COLORS);
    EXPECT_EQ(r, 8);
    int bad = 0;
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) if (gp(&d, x, y) != src_col(x, y)) bad++;
    CHECKF(bad == 0, "bad %d", bad);
    dst_close(&d);
}
TEST(dib_sdtd_dest_offset) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 12, 12, 32, RGB(1, 1, 1)));
    BMI b; bmi_init(&b, 4, 4, 24, BI_RGB); BYTE src[12 * 4]; img24(src, 4, 4);
    EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 5, 6, 4, 4, 0, 0, 0, 4, src, PB(&b), DIB_RGB_COLORS), 4);
    int bad = 0;
    for (int y = 0; y < 12; y++) for (int x = 0; x < 12; x++) {
        COLORREF e = (x >= 5 && x < 9 && y >= 6 && y < 10) ? src_col(x - 5, y - 6) : RGB(1, 1, 1);
        if (gp(&d, x, y) != e) bad++;
    }
    CHECKF(bad == 0, "bad %d", bad);
    dst_close(&d);
}
TEST(dib_sdtd_src_subrect) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 10, 10, 32, RGB(2, 2, 2)));
    BMI b; bmi_init(&b, 8, 8, 24, BI_RGB); BYTE src[24 * 8]; img24(src, 8, 8);
    /* ySrc counted from the bottom row of the bottom-up DIB */
    int xs = 2, ys = 1, w = 4, h = 3;
    int r = U(SetDIBitsToDevice)(d.dc, 1, 1, w, h, xs, ys, 0, 8, src, PB(&b), DIB_RGB_COLORS);
    t_note("subrect return=%d", r);
    int top = 8 - ys - h, bad = 0;
    for (int y = 0; y < 10; y++) for (int x = 0; x < 10; x++) {
        COLORREF e = (x >= 1 && x < 1 + w && y >= 1 && y < 1 + h) ? src_col(xs + x - 1, top + y - 1) : RGB(2, 2, 2);
        if (gp(&d, x, y) != e) bad++;
    }
    CHECKF(bad == 0, "bad %d", bad);
    dst_close(&d);
}
TEST(dib_sdtd_startscan_band) {
    NEED_DRAW();
    for (int start = 0; start < 8; start += 3) for (int lines = 1; start + lines <= 8; lines += 2) {
        DST d; CHECK(dst_open(&d, 8, 8, 32, RGB(3, 3, 3)));
        BMI b; bmi_init(&b, 8, 8, 24, BI_RGB); BYTE full[24 * 8], band[24 * 8]; img24(full, 8, 8);
        memcpy(band, full + start * 24, lines * 24);
        int r = U(SetDIBitsToDevice)(d.dc, 0, 0, 8, 8, 0, 0, start, lines, band, PB(&b), DIB_RGB_COLORS);
        CHECKF(r == lines, "start %d lines %d -> %d", start, lines, r);
        int bad = 0;
        for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
            int scan = 7 - y; /* scanline counted from bottom */
            COLORREF e = (scan >= start && scan < start + lines) ? src_col(x, y) : RGB(3, 3, 3);
            if (gp(&d, x, y) != e) bad++;
        }
        CHECKF(bad == 0, "start %d lines %d bad %d", start, lines, bad);
        dst_close(&d);
    }
}
TEST(dib_sdtd_cLines_clamped_return) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 8, 8, 32, 0));
    BMI b; bmi_init(&b, 8, 8, 24, BI_RGB); BYTE src[24 * 8 * 2]; memset(src, 0x40, sizeof src);
    /* cLines clipped to min(ySrc+h, biHeight) - StartScan */
    EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 0, 0, 8, 8, 0, 0, 0, 100, src, PB(&b), DIB_RGB_COLORS), 8);
    EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 0, 0, 8, 4, 0, 0, 0, 100, src, PB(&b), DIB_RGB_COLORS), 4);
    EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 0, 0, 8, 8, 0, 0, 5, 100, src, PB(&b), DIB_RGB_COLORS), 3);
    EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 0, 0, 8, 8, 0, 0, 0, 3, src, PB(&b), DIB_RGB_COLORS), 3);
    EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 0, 0, 8, 8, 0, 0, 0, 1, src, PB(&b), DIB_RGB_COLORS), 1);
    dst_close(&d);
}
TEST(dib_sdtd_invalid_params) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 8, 8, 32, 0));
    BMI b; bmi_init(&b, 8, 8, 24, BI_RGB); BYTE src[24 * 8]; memset(src, 0x40, sizeof src);
    EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 0, 0, 8, 8, 0, 0, 0, 0, src, PB(&b), DIB_RGB_COLORS), 0);      /* cLines 0 */
    EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 0, 0, 8, 8, 0, 0, 0, 8, NULL, PB(&b), DIB_RGB_COLORS), 0);     /* no bits */
    EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 0, 0, 8, 8, 0, 0, 0, 8, src, NULL, DIB_RGB_COLORS), 0);        /* no bmi */
    EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 0, 0, 8, 8, 0, 0, 0, 8, src, PB(&b), 3), 0);                   /* usage */
    EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 0, 0, 8, 8, 0, 0, 0, 8, src, PB(&b), 0xFFFF), 0);
    EXPECT_EQ(U(SetDIBitsToDevice)(NULL, 0, 0, 8, 8, 0, 0, 0, 8, src, PB(&b), DIB_RGB_COLORS), 0);
    EXPECT_EQ(U(SetDIBitsToDevice)((HDC)(ULONG_PTR)0x1234, 0, 0, 8, 8, 0, 0, 0, 8, src, PB(&b), DIB_RGB_COLORS), 0);
    HBRUSH br = U(CreateSolidBrush)(0);
    EXPECT_EQ(U(SetDIBitsToDevice)((HDC)br, 0, 0, 8, 8, 0, 0, 0, 8, src, PB(&b), DIB_RGB_COLORS), 0);
    U(DeleteObject)(br);
    int bad = 0; for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) if (gp(&d, x, y) != 0) bad++;
    CHECKF(bad == 0, "dest modified by failed calls (%d)", bad);
    dst_close(&d);
}
TEST(dib_sdtd_invalid_biSize) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 4, 4, 32, 0));
    static const DWORD bad[] = { 0, 1, 4, 8, 11, 13, 24, 39, 249, 256, 1000, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        BYTE buf[600]; memset(buf, 0, sizeof buf);
        BITMAPINFOHEADER *h = (BITMAPINFOHEADER *)buf; h->biSize = bad[i]; h->biWidth = 4; h->biHeight = 4; h->biPlanes = 1; h->biBitCount = 24;
        BYTE src[48]; memset(src, 0x80, sizeof src);
        int r = U(SetDIBitsToDevice)(d.dc, 0, 0, 4, 4, 0, 0, 0, 4, src, (BITMAPINFO *)buf, DIB_RGB_COLORS);
        if (dib_is_wine() && bad[i] > 248) continue;
        CHECKF(r == 0, "biSize %lu -> %d", bad[i], r);
    }
    dst_close(&d);
}
TEST(dib_sdtd_valid_biSize_range) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 4, 4, 32, 0));
    static const DWORD ok[] = { 40, 41, 44, 52, 56, 64, 108, 124, 200, 248 };
    for (unsigned i = 0; i < sizeof ok / sizeof *ok; i++) {
        BYTE buf[600]; memset(buf, 0, sizeof buf);
        BITMAPINFOHEADER *h = (BITMAPINFOHEADER *)buf; h->biSize = ok[i]; h->biWidth = 4; h->biHeight = 4; h->biPlanes = 1; h->biBitCount = 24;
        BYTE src[48]; memset(src, 0x80, sizeof src);
        int r = U(SetDIBitsToDevice)(d.dc, 0, 0, 4, 4, 0, 0, 0, 4, src, (BITMAPINFO *)buf, DIB_RGB_COLORS);
        t_note("biSize %lu -> %d", ok[i], r);
        if (ok[i] == 40 || ok[i] == 108 || ok[i] == 124) EXPECT_EQ(r, 4);
    }
    dst_close(&d);
}
TEST(dib_sdtd_invalid_bitcount) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 4, 4, 32, 0));
    static const int bad[] = { 0, 2, 3, 5, 7, 9, 12, 15, 17, 31, 33, 48, 64 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        BMI b; bmi_init(&b, 4, 4, bad[i], BI_RGB); BYTE src[64 * 4]; memset(src, 0x55, sizeof src);
        int r = U(SetDIBitsToDevice)(d.dc, 0, 0, 4, 4, 0, 0, 0, 4, src, PB(&b), DIB_RGB_COLORS);
        t_note("bitcount %d -> %d", bad[i], r);
    }
    dst_close(&d);
}
TEST(dib_sdtd_compression_mismatch) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 4, 4, 32, 0));
    BYTE src[256]; memset(src, 0, sizeof src);
    struct { int bpp; DWORD comp; } t[] = { {24, BI_RLE8}, {24, BI_RLE4}, {8, BI_RLE4}, {4, BI_RLE8}, {8, BI_BITFIELDS}, {24, BI_BITFIELDS}, {1, BI_BITFIELDS}, {4, BI_BITFIELDS} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        BMI b; bmi_init(&b, 4, 4, t[i].bpp, t[i].comp); b.h.biSizeImage = 16;
        int r = U(SetDIBitsToDevice)(d.dc, 0, 0, 4, 4, 0, 0, 0, 4, src, PB(&b), DIB_RGB_COLORS);
        CHECKF(r == 0, "bpp %d comp %lu -> %d", t[i].bpp, t[i].comp, r);
    }
    dst_close(&d);
}
TEST(dib_sdtd_clip_dest_outside) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 6, 6, 32, RGB(4, 4, 4)));
    BMI b; bmi_init(&b, 4, 4, 24, BI_RGB); BYTE src[12 * 4]; img24(src, 4, 4);
    int r = U(SetDIBitsToDevice)(d.dc, -2, -2, 4, 4, 0, 0, 0, 4, src, PB(&b), DIB_RGB_COLORS);
    t_note("partially off top-left -> %d", r);
    int bad = 0;
    for (int y = 0; y < 6; y++) for (int x = 0; x < 6; x++) {
        COLORREF e = (x < 2 && y < 2) ? src_col(x + 2, y + 2) : RGB(4, 4, 4);
        if (gp(&d, x, y) != e) bad++;
    }
    CHECKF(bad == 0, "bad %d", bad);
    r = U(SetDIBitsToDevice)(d.dc, 100, 100, 4, 4, 0, 0, 0, 4, src, PB(&b), DIB_RGB_COLORS);
    t_note("entirely outside -> %d", r);
    r = U(SetDIBitsToDevice)(d.dc, 4, 4, 4, 4, 0, 0, 0, 4, src, PB(&b), DIB_RGB_COLORS);
    t_note("partially off bottom-right -> %d", r);
    EXPECT_EQ(gp(&d, 5, 5), src_col(1, 1));
    dst_close(&d);
}
TEST(dib_sdtd_clip_region) {
    NEED_DRAW(); REQUIRE(U(CreateRectRgn)); REQUIRE(U(SelectClipRgn));
    DST d; CHECK(dst_open(&d, 8, 8, 32, RGB(5, 5, 5)));
    HRGN rg = U(CreateRectRgn)(2, 2, 5, 5); U(SelectClipRgn)(d.dc, rg);
    BMI b; bmi_init(&b, 8, 8, 24, BI_RGB); BYTE src[24 * 8]; img24(src, 8, 8);
    U(SetDIBitsToDevice)(d.dc, 0, 0, 8, 8, 0, 0, 0, 8, src, PB(&b), DIB_RGB_COLORS);
    int bad = 0;
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
        COLORREF e = (x >= 2 && x < 5 && y >= 2 && y < 5) ? src_col(x, y) : RGB(5, 5, 5);
        if (gp(&d, x, y) != e) bad++;
    }
    CHECKF(bad == 0, "bad %d", bad);
    U(SelectClipRgn)(d.dc, NULL); U(DeleteObject)(rg);
    dst_close(&d);
}
TEST(dib_sdtd_core_header) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 4, 4, 32, 0));
    BYTE bb[sizeof(BITMAPCOREHEADER)]; BITMAPCOREHEADER *c = (BITMAPCOREHEADER *)bb;
    memset(bb, 0, sizeof bb); c->bcSize = 12; c->bcWidth = 4; c->bcHeight = 4; c->bcPlanes = 1; c->bcBitCount = 24;
    BYTE src[12 * 4]; img24(src, 4, 4);
    EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 0, 0, 4, 4, 0, 0, 0, 4, src, (BITMAPINFO *)bb, DIB_RGB_COLORS), 4);
    EXPECT_EQ(gp(&d, 1, 2), src_col(1, 2));
    dst_close(&d);
}
TEST(dib_sdtd_core_header_8bpp) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 4, 2, 32, 0));
    BYTE bb[sizeof(BITMAPCOREHEADER) + 256 * 3]; memset(bb, 0, sizeof bb);
    BITMAPCOREINFO *c = (BITMAPCOREINFO *)bb; c->bmciHeader.bcSize = 12; c->bmciHeader.bcWidth = 4; c->bmciHeader.bcHeight = 2; c->bmciHeader.bcPlanes = 1; c->bmciHeader.bcBitCount = 8;
    c->bmciColors[0].rgbtRed = 200; c->bmciColors[1].rgbtGreen = 100; c->bmciColors[2].rgbtBlue = 50; c->bmciColors[3].rgbtRed = 1; c->bmciColors[3].rgbtGreen = 2; c->bmciColors[3].rgbtBlue = 3;
    BYTE src[8] = { 0, 1, 2, 3, 3, 2, 1, 0 };
    EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 0, 0, 4, 2, 0, 0, 0, 2, src, (BITMAPINFO *)c, DIB_RGB_COLORS), 2);
    EXPECT_EQ(gp(&d, 0, 1), RGB(200, 0, 0)); EXPECT_EQ(gp(&d, 3, 1), RGB(1, 2, 3));
    EXPECT_EQ(gp(&d, 0, 0), RGB(1, 2, 3)); EXPECT_EQ(gp(&d, 3, 0), RGB(200, 0, 0));
    dst_close(&d);
}

/* paletted sources of every depth */
static void sdtd_pal(int bpp) {
    NEED_DRAW();
    int w = 11, h = 3, n = 1 << bpp;
    DST d; CHECK(dst_open(&d, w, h, 32, 0));
    BMI b; bmi_init(&b, w, h, bpp, BI_RGB); bmi_pal(&b, n);
    int st = dib_stride(w, bpp); BYTE src[200]; memset(src, 0, sizeof src);
    int idxs[3][11];
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        int idx = (x * 3 + y * 5 + 1) % n; idxs[y][x] = idx; BYTE *row = src + (h - 1 - y) * st;
        if (bpp == 8) row[x] = idx;
        else if (bpp == 4) row[x / 2] |= (x & 1) ? idx : (idx << 4);
        else if (idx) row[x / 8] |= 0x80 >> (x & 7);
    }
    EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 0, 0, w, h, 0, 0, 0, h, src, PB(&b), DIB_RGB_COLORS), h);
    int bad = 0;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        int i = idxs[y][x]; COLORREF e = RGB(b.q[i].rgbRed, b.q[i].rgbGreen, b.q[i].rgbBlue);
        if (gp(&d, x, y) != e) bad++;
    }
    CHECKF(bad == 0, "bad %d", bad);
    dst_close(&d);
}
TEST(dib_sdtd_src_1bpp) { sdtd_pal(1); }
TEST(dib_sdtd_src_4bpp) { sdtd_pal(4); }
TEST(dib_sdtd_src_8bpp) { sdtd_pal(8); }

static void sdtd_hc(int bpp) {
    NEED_DRAW();
    int w = 5, h = 3; DST d; CHECK(dst_open(&d, w, h, 32, 0));
    BMI b; bmi_init(&b, w, h, bpp, BI_RGB); int st = dib_stride(w, bpp); BYTE src[128]; memset(src, 0, sizeof src);
    static const COLORREF cols[] = { RGB(255,0,0), RGB(0,255,0), RGB(0,0,255), RGB(255,255,255), RGB(0,0,0) };
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        COLORREF c = cols[(x + y) % 5]; BYTE *row = src + (h - 1 - y) * st; BYTE r = GetRValue(c), g = GetGValue(c), bl = GetBValue(c);
        if (bpp == 16) ((WORD *)row)[x] = ((r >> 3) << 10) | ((g >> 3) << 5) | (bl >> 3);
        else if (bpp == 24) { row[x*3] = bl; row[x*3+1] = g; row[x*3+2] = r; }
        else ((DWORD *)row)[x] = (r << 16) | (g << 8) | bl;
    }
    EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 0, 0, w, h, 0, 0, 0, h, src, PB(&b), DIB_RGB_COLORS), h);
    int bad = 0;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (gp(&d, x, y) != cols[(x + y) % 5]) bad++;
    CHECKF(bad == 0, "bad %d", bad);
    dst_close(&d);
}
TEST(dib_sdtd_src_16bpp) { sdtd_hc(16); }
TEST(dib_sdtd_src_24bpp) { sdtd_hc(24); }
TEST(dib_sdtd_src_32bpp) { sdtd_hc(32); }

TEST(dib_sdtd_src_16bpp_565_bitfields) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 4, 1, 32, 0));
    BMI b; bmi_init(&b, 4, 1, 16, BI_BITFIELDS); b.m[0] = 0xF800; b.m[1] = 0x07E0; b.m[2] = 0x001F;
    WORD src[4] = { 0xF800, 0x07E0, 0x001F, 0xFFFF };
    EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 0, 0, 4, 1, 0, 0, 0, 1, src, PB(&b), DIB_RGB_COLORS), 1);
    EXPECT_EQ(gp(&d, 0, 0), RGB(255, 0, 0)); EXPECT_EQ(gp(&d, 1, 0), RGB(0, 255, 0));
    EXPECT_EQ(gp(&d, 2, 0), RGB(0, 0, 255)); EXPECT_EQ(gp(&d, 3, 0), RGB(255, 255, 255));
    dst_close(&d);
}
TEST(dib_sdtd_src_16bpp_555_default) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 4, 1, 32, 0));
    BMI b; bmi_init(&b, 4, 1, 16, BI_RGB);
    WORD src[4] = { 0x7C00, 0x03E0, 0x001F, 0x7FFF };
    EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 0, 0, 4, 1, 0, 0, 0, 1, src, PB(&b), DIB_RGB_COLORS), 1);
    EXPECT_EQ(gp(&d, 0, 0), RGB(255, 0, 0)); EXPECT_EQ(gp(&d, 1, 0), RGB(0, 255, 0));
    EXPECT_EQ(gp(&d, 2, 0), RGB(0, 0, 255)); EXPECT_EQ(gp(&d, 3, 0), RGB(255, 255, 255));
    dst_close(&d);
}
TEST(dib_sdtd_src_32bpp_bitfields_swapped) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 2, 1, 32, 0));
    BMI b; bmi_init(&b, 2, 1, 32, BI_BITFIELDS); b.m[0] = 0x0000FF; b.m[1] = 0x00FF00; b.m[2] = 0xFF0000;
    DWORD src[2] = { 0x000000FF, 0x00FF0000 };
    EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 0, 0, 2, 1, 0, 0, 0, 1, src, PB(&b), DIB_RGB_COLORS), 1);
    EXPECT_EQ(gp(&d, 0, 0), RGB(255, 0, 0)); EXPECT_EQ(gp(&d, 1, 0), RGB(0, 0, 255));
    dst_close(&d);
}

/* destination depths receive the same colour set */
static void sdtd_dest(int bpp) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 4, 4, bpp, 0));
    BMI b; bmi_init(&b, 4, 4, 24, BI_RGB); BYTE src[12 * 4];
    static const COLORREF cs[4] = { RGB(255,255,255), RGB(0,0,0), RGB(255,0,0), RGB(0,0,255) };
    for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) { BYTE *p = src + (3 - y) * 12 + x * 3; COLORREF c = cs[(x + y) & 3]; p[0] = GetBValue(c); p[1] = GetGValue(c); p[2] = GetRValue(c); }
    EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 0, 0, 4, 4, 0, 0, 0, 4, src, PB(&b), DIB_RGB_COLORS), 4);
    int bad = 0;
    for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) {
        COLORREF e = cs[(x + y) & 3];
        COLORREF g = gp(&d, x, y);
        if (bpp == 1) { if (g != RGB(0x55, 255, 0) && g != RGB(0x54, 254, 1)) bad++; } /* mkdib's 2-entry table */
        else if (g != e) bad++;
    }
    CHECKF(bad == 0, "bad %d", bad);
    dst_close(&d);
}
TEST(dib_sdtd_dest_4bpp_note) {
    NEED_DRAW();
    DST d; if (!dst_open(&d, 4, 4, 4, 0)) { CHECK(0); return; }
    BMI b; bmi_init(&b, 4, 4, 24, BI_RGB); BYTE src[48]; memset(src, 0xFF, sizeof src);
    int r = U(SetDIBitsToDevice)(d.dc, 0, 0, 4, 4, 0, 0, 0, 4, src, PB(&b), DIB_RGB_COLORS);
    t_note("24->4bpp dest r=%d pixel=%06lx", r, (unsigned long)gp(&d, 0, 0));
    dst_close(&d);
}
TEST(dib_sdtd_dest_16) { sdtd_dest(16); }
TEST(dib_sdtd_dest_24) { sdtd_dest(24); }
TEST(dib_sdtd_dest_32) { sdtd_dest(32); }
TEST(dib_sdtd_dest_1) { sdtd_dest(1); }

TEST(dib_sdtd_dest_8bpp_exact_palette_match) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 4, 1, 8, 0));
    RGBQUAD q[256]; memset(q, 0, sizeof q);
    q[10].rgbRed = 255; q[20].rgbGreen = 255; q[30].rgbBlue = 255; q[40].rgbRed = q[40].rgbGreen = q[40].rgbBlue = 128;
    EXPECT_EQ(U(SetDIBColorTable)(d.dc, 0, 256, q), 256);
    BMI b; bmi_init(&b, 4, 1, 24, BI_RGB); BYTE src[12] = { 0,0,255, 0,255,0, 255,0,0, 128,128,128 };
    EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 0, 0, 4, 1, 0, 0, 0, 1, src, PB(&b), DIB_RGB_COLORS), 1);
    U(GdiFlush)();
    BYTE *p = d.bits;
    EXPECT_EQ(p[0], 10); EXPECT_EQ(p[1], 20); EXPECT_EQ(p[2], 30); EXPECT_EQ(p[3], 40);
    dst_close(&d);
}

/* PAL_COLORS / PAL_INDICES (usage 2) */
TEST(dib_sdtd_usage_pal_colors) {
    NEED_DRAW();
    if (dib_is_wine()) { t_skip("wine: palette usage"); return; }
    DST d; CHECK(dst_open(&d, 4, 1, 32, 0));
    BMI b; bmi_init(&b, 4, 1, 8, BI_RGB); for (int i = 0; i < 4; i++) b.idx[i] = (WORD)i;
    BYTE src[4] = { 0, 1, 2, 3 };
    int r = U(SetDIBitsToDevice)(d.dc, 0, 0, 4, 1, 0, 0, 0, 1, src, PB(&b), DIB_PAL_COLORS);
    t_note("PAL_COLORS r=%d px0=%06lx px1=%06lx px2=%06lx px3=%06lx", r, (unsigned long)gp(&d, 0, 0), (unsigned long)gp(&d, 1, 0), (unsigned long)gp(&d, 2, 0), (unsigned long)gp(&d, 3, 0));
    dst_close(&d);
}
TEST(dib_sdtd_usage_pal_indices) {
    NEED_DRAW();
    if (dib_is_wine()) { t_skip("wine: palette usage"); return; }
    DST d; CHECK(dst_open(&d, 4, 1, 32, 0));
    BMI b; bmi_init(&b, 4, 1, 8, BI_RGB);
    BYTE src[4] = { 0, 1, 2, 3 };
    int r = U(SetDIBitsToDevice)(d.dc, 0, 0, 4, 1, 0, 0, 0, 1, src, PB(&b), 2);
    t_note("usage 2 r=%d px1=%06lx", r, (unsigned long)gp(&d, 1, 0));
    dst_close(&d);
}

/* ---------- RLE ---------- */
TEST(dib_rle8_basic_runs) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 6, 3, 32, RGB(9, 9, 9)));
    BMI b; bmi_init(&b, 6, 3, 8, BI_RLE8);
    for (int i = 0; i < 256; i++) { b.q[i].rgbRed = i; b.q[i].rgbGreen = 0; b.q[i].rgbBlue = 255 - i; }
    /* bottom row (scanline 0): 6 x idx 10 ; EOL ; row 1: 3 x 20, 3 x 30 ; EOL ; row 2: absolute 4 pixels 1,2,3,4 + run 2 x 5 ; EOB */
    BYTE rle[] = { 6, 10, 0, 0,  3, 20, 3, 30, 0, 0,  0, 4, 1, 2, 3, 4, 2, 5, 0, 1 };
    b.h.biSizeImage = sizeof rle;
    int r = U(SetDIBitsToDevice)(d.dc, 0, 0, 6, 3, 0, 0, 0, 3, rle, PB(&b), DIB_RGB_COLORS);
    t_note("RLE8 SetDIBitsToDevice -> %d", r);
    EXPECT_EQ(r, 3);
    #define IC(i) RGB(i, 0, 255 - (i))
    for (int x = 0; x < 6; x++) EXPECT_EQ(gp(&d, x, 2), IC(10));
    for (int x = 0; x < 3; x++) EXPECT_EQ(gp(&d, x, 1), IC(20));
    for (int x = 3; x < 6; x++) EXPECT_EQ(gp(&d, x, 1), IC(30));
    EXPECT_EQ(gp(&d, 0, 0), IC(1)); EXPECT_EQ(gp(&d, 3, 0), IC(4)); EXPECT_EQ(gp(&d, 4, 0), IC(5)); EXPECT_EQ(gp(&d, 5, 0), IC(5));
    dst_close(&d);
}
TEST(dib_rle8_delta_and_skipped) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 6, 3, 32, RGB(9, 9, 9)));
    BMI b; bmi_init(&b, 6, 3, 8, BI_RLE8);
    for (int i = 0; i < 256; i++) { b.q[i].rgbRed = i; b.q[i].rgbBlue = 255 - i; }
    /* delta (+2,+1) then run of 2 x idx 7, EOB: leaves other pixels untouched/transparent-ish */
    BYTE rle[] = { 0, 2, 2, 1,  2, 7, 0, 1 };
    b.h.biSizeImage = sizeof rle;
    int r = U(SetDIBitsToDevice)(d.dc, 0, 0, 6, 3, 0, 0, 0, 3, rle, PB(&b), DIB_RGB_COLORS);
    t_note("RLE8 delta -> %d", r);
    /* the run lands at x=2..3 of scanline 1 (from bottom) = y 1 */
    EXPECT_EQ(gp(&d, 2, 1), IC(7)); EXPECT_EQ(gp(&d, 3, 1), IC(7));
    t_note("skipped pixel (0,2) = %06lx", (unsigned long)gp(&d, 0, 2));
    dst_close(&d);
}
TEST(dib_rle8_absolute_odd_padding) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 5, 1, 32, 0));
    BMI b; bmi_init(&b, 5, 1, 8, BI_RLE8);
    for (int i = 0; i < 256; i++) { b.q[i].rgbRed = i; b.q[i].rgbBlue = 255 - i; }
    BYTE rle[] = { 0, 5, 11, 12, 13, 14, 15, 0, 0, 1 }; /* 5 literal bytes + 1 pad */
    b.h.biSizeImage = sizeof rle;
    EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 0, 0, 5, 1, 0, 0, 0, 1, rle, PB(&b), DIB_RGB_COLORS), 1);
    for (int x = 0; x < 5; x++) EXPECT_EQ(gp(&d, x, 0), IC(11 + x));
    dst_close(&d);
}
TEST(dib_rle4_basic) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 6, 2, 32, 0));
    BMI b; bmi_init(&b, 6, 2, 4, BI_RLE4);
    for (int i = 0; i < 16; i++) { b.q[i].rgbRed = i * 16; b.q[i].rgbBlue = 255 - i * 16; }
    /* row0: 6 pixels alternating 3,5 ; EOL ; row1: absolute 4 nibbles 1 2 3 4, then run 2 of nibble pair 6,7 ; EOB */
    BYTE rle[] = { 6, 0x35, 0, 0,  0, 4, 0x12, 0x34, 2, 0x67, 0, 1 };
    b.h.biSizeImage = sizeof rle;
    int r = U(SetDIBitsToDevice)(d.dc, 0, 0, 6, 2, 0, 0, 0, 2, rle, PB(&b), DIB_RGB_COLORS);
    EXPECT_EQ(r, 2);
    #define NC(i) RGB((i) * 16, 0, 255 - (i) * 16)
    for (int x = 0; x < 6; x++) EXPECT_EQ(gp(&d, x, 1), NC((x & 1) ? 5 : 3));
    EXPECT_EQ(gp(&d, 0, 0), NC(1)); EXPECT_EQ(gp(&d, 1, 0), NC(2)); EXPECT_EQ(gp(&d, 2, 0), NC(3)); EXPECT_EQ(gp(&d, 3, 0), NC(4));
    EXPECT_EQ(gp(&d, 4, 0), NC(6)); EXPECT_EQ(gp(&d, 5, 0), NC(7));
    dst_close(&d);
}
TEST(dib_rle8_via_stretchdibits) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 4, 2, 32, 0));
    BMI b; bmi_init(&b, 4, 2, 8, BI_RLE8);
    for (int i = 0; i < 256; i++) { b.q[i].rgbRed = i; b.q[i].rgbBlue = 255 - i; }
    BYTE rle[] = { 4, 40, 0, 0, 4, 50, 0, 1 }; b.h.biSizeImage = sizeof rle;
    int r = U(StretchDIBits)(d.dc, 0, 0, 4, 2, 0, 0, 4, 2, rle, PB(&b), DIB_RGB_COLORS, SRCCOPY);
    t_note("StretchDIBits RLE8 -> %d", r);
    EXPECT_EQ(gp(&d, 0, 1), IC(40)); EXPECT_EQ(gp(&d, 3, 0), IC(50));
    dst_close(&d);
}
TEST(dib_rle_topdown_rejected) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 4, 2, 32, 0));
    BMI b; bmi_init(&b, 4, -2, 8, BI_RLE8); BYTE rle[] = { 4, 1, 0, 0, 4, 2, 0, 1 }; b.h.biSizeImage = sizeof rle;
    int r = U(SetDIBitsToDevice)(d.dc, 0, 0, 4, 2, 0, 0, 0, 2, rle, PB(&b), DIB_RGB_COLORS);
    t_note("top-down RLE8 -> %d", r);
    dst_close(&d);
}

/* ---------- StretchDIBits ---------- */
TEST(dib_stretchdibits_1to1) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 8, 8, 32, 0));
    BMI b; bmi_init(&b, 8, 8, 24, BI_RGB); BYTE src[24 * 8]; img24(src, 8, 8);
    int r = U(StretchDIBits)(d.dc, 0, 0, 8, 8, 0, 0, 8, 8, src, PB(&b), DIB_RGB_COLORS, SRCCOPY);
    EXPECT_EQ(r, 8);
    int bad = 0; for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) if (gp(&d, x, y) != src_col(x, y)) bad++;
    CHECKF(bad == 0, "bad %d", bad);
    dst_close(&d);
}
TEST(dib_stretchdibits_2x_enlarge) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 8, 8, 32, 0));
    BMI b; bmi_init(&b, 4, 4, 24, BI_RGB); BYTE src[12 * 4]; img24(src, 4, 4);
    int r = U(StretchDIBits)(d.dc, 0, 0, 8, 8, 0, 0, 4, 4, src, PB(&b), DIB_RGB_COLORS, SRCCOPY);
    EXPECT_EQ(r, 4);
    int bad = 0; for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) if (gp(&d, x, y) != src_col(x / 2, y / 2)) bad++;
    CHECKF(bad == 0, "bad %d", bad);
    dst_close(&d);
}
TEST(dib_stretchdibits_2x_shrink) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 4, 4, 32, 0));
    BMI b; bmi_init(&b, 8, 8, 24, BI_RGB); BYTE src[24 * 8]; img24(src, 8, 8);
    int r = U(StretchDIBits)(d.dc, 0, 0, 4, 4, 0, 0, 8, 8, src, PB(&b), DIB_RGB_COLORS, SRCCOPY);
    EXPECT_EQ(r, 8);
    int bad = 0; for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) {
        COLORREF g = gp(&d, x, y);
        COLORREF a = src_col(x * 2, y * 2), bb = src_col(x * 2 + 1, y * 2 + 1);
        COLORREF c1 = src_col(x * 2 + 1, y * 2), c2 = src_col(x * 2, y * 2 + 1);
        COLORREF an = a & bb & c1 & c2; /* default STRETCH_ANDSCANS combines covered pixels */
        if (g != a && g != bb && g != c1 && g != c2 && g != an) bad++;
    }
    CHECKF(bad == 0, "bad %d", bad);
    dst_close(&d);
}
TEST(dib_stretchdibits_mirror_x) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 4, 4, 32, 0));
    BMI b; bmi_init(&b, 4, 4, 24, BI_RGB); BYTE src[12 * 4]; img24(src, 4, 4);
    int r = U(StretchDIBits)(d.dc, 3, 0, -4, 4, 0, 0, 4, 4, src, PB(&b), DIB_RGB_COLORS, SRCCOPY);
    t_note("mirror x r=%d", r);
    int bad = 0; for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) if (gp(&d, x, y) != src_col(3 - x, y)) bad++;
    CHECKF(bad == 0, "bad %d", bad);
    dst_close(&d);
}
TEST(dib_stretchdibits_mirror_y) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 4, 4, 32, 0));
    BMI b; bmi_init(&b, 4, 4, 24, BI_RGB); BYTE src[12 * 4]; img24(src, 4, 4);
    int r = U(StretchDIBits)(d.dc, 0, 3, 4, -4, 0, 0, 4, 4, src, PB(&b), DIB_RGB_COLORS, SRCCOPY);
    t_note("mirror y r=%d", r);
    int bad = 0; for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) if (gp(&d, x, y) != src_col(x, 3 - y)) bad++;
    CHECKF(bad == 0, "bad %d", bad);
    dst_close(&d);
}
TEST(dib_stretchdibits_topdown_source) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 4, 4, 32, 0));
    BMI b; bmi_init(&b, 4, -4, 24, BI_RGB); BYTE tmp[12 * 4], src[12 * 4]; img24(tmp, 4, 4);
    for (int y = 0; y < 4; y++) memcpy(src + y * 12, tmp + (3 - y) * 12, 12);
    EXPECT_EQ(U(StretchDIBits)(d.dc, 0, 0, 4, 4, 0, 0, 4, 4, src, PB(&b), DIB_RGB_COLORS, SRCCOPY), 4);
    int bad = 0; for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) if (gp(&d, x, y) != src_col(x, y)) bad++;
    CHECKF(bad == 0, "bad %d", bad);
    dst_close(&d);
}
TEST(dib_stretchdibits_src_rect) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 8, 8, 32, 0));
    BMI b; bmi_init(&b, 8, 8, 24, BI_RGB); BYTE src[24 * 8]; img24(src, 8, 8);
    int r = U(StretchDIBits)(d.dc, 0, 0, 3, 2, 2, 1, 3, 2, src, PB(&b), DIB_RGB_COLORS, SRCCOPY);
    t_note("src rect r=%d", r);
    int bad = 0; /* ySrc=1,hSrc=2 counted from the bottom => top-based rows 5..6 */
    for (int y = 0; y < 2; y++) for (int x = 0; x < 3; x++) if (gp(&d, x, y) != src_col(2 + x, 5 + y)) bad++;
    CHECKF(bad == 0, "bad %d", bad);
    dst_close(&d);
}
TEST(dib_stretchdibits_rops) {
    NEED_DRAW();
    static const struct { DWORD rop; int kind; } t[] = {
        { SRCCOPY, 0 }, { NOTSRCCOPY, 1 }, { SRCINVERT, 2 }, { SRCAND, 3 }, { SRCPAINT, 4 },
        { BLACKNESS, 5 }, { WHITENESS, 6 }, { DSTINVERT, 7 }, { SRCERASE, 8 }, { NOTSRCERASE, 9 }, { MERGEPAINT, 10 },
    };
    for (unsigned k = 0; k < sizeof t / sizeof *t; k++) {
        DST d; CHECK(dst_open(&d, 4, 4, 32, RGB(0x33, 0x55, 0xF0)));
        BMI b; bmi_init(&b, 4, 4, 24, BI_RGB); BYTE src[12 * 4]; img24(src, 4, 4);
        U(StretchDIBits)(d.dc, 0, 0, 4, 4, 0, 0, 4, 4, src, PB(&b), DIB_RGB_COLORS, t[k].rop);
        int bad = 0;
        for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) {
            DWORD s = src_col(x, y) & 0xFFFFFF, dd = RGB(0x33, 0x55, 0xF0), e = 0;
            switch (t[k].kind) {
            case 0: e = s; break; case 1: e = ~s; break; case 2: e = s ^ dd; break; case 3: e = s & dd; break;
            case 4: e = s | dd; break; case 5: e = 0; break; case 6: e = ~0u; break; case 7: e = ~dd; break;
            case 8: e = s & ~dd; break; case 9: e = ~(s | dd); break; case 10: e = ~s | dd; break;
            }
            if ((gp(&d, x, y) & 0xFFFFFF) != (e & 0xFFFFFF)) bad++;
        }
        CHECKF(bad == 0, "rop 0x%08lx bad %d", t[k].rop, bad);
        dst_close(&d);
    }
}
TEST(dib_stretchdibits_pattern_rops_nopattern_brush) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 2, 2, 32, RGB(0x10, 0x20, 0x30)));
    BMI b; bmi_init(&b, 2, 2, 24, BI_RGB); BYTE src[8 * 2]; memset(src, 0xFF, sizeof src);
    HBRUSH br = U(CreateSolidBrush)(RGB(0xFF, 0, 0)), ob = U(SelectObject)(d.dc, br);
    int r = U(StretchDIBits)(d.dc, 0, 0, 2, 2, 0, 0, 2, 2, src, PB(&b), DIB_RGB_COLORS, PATCOPY);
    t_note("StretchDIBits PATCOPY r=%d px=%06lx", r, (unsigned long)gp(&d, 0, 0));
    r = U(StretchDIBits)(d.dc, 0, 0, 2, 2, 0, 0, 2, 2, src, PB(&b), DIB_RGB_COLORS, 0x00F00021 /* PATPAINT-like*/);
    t_note("rop 0xF00021 r=%d", r);
    U(SelectObject)(d.dc, ob); U(DeleteObject)(br);
    dst_close(&d);
}
TEST(dib_stretchdibits_invalid) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 4, 4, 32, 0));
    BMI b; bmi_init(&b, 4, 4, 24, BI_RGB); BYTE src[12 * 4]; memset(src, 0x77, sizeof src);
    int r;
    r = U(StretchDIBits)(NULL, 0, 0, 4, 4, 0, 0, 4, 4, src, PB(&b), DIB_RGB_COLORS, SRCCOPY);
    CHECKF(r == 0 || r == (int)GDI_ERROR, "NULL hdc -> %d", r);
    r = U(StretchDIBits)((HDC)(ULONG_PTR)0x4321, 0, 0, 4, 4, 0, 0, 4, 4, src, PB(&b), DIB_RGB_COLORS, SRCCOPY);
    CHECKF(r == 0 || r == (int)GDI_ERROR, "bogus hdc -> %d", r);
    r = U(StretchDIBits)(d.dc, 0, 0, 4, 4, 0, 0, 4, 4, src, PB(&b), 5, SRCCOPY);
    CHECKF(r == 0 || r == (int)GDI_ERROR, "bad usage -> %d", r);
    r = U(StretchDIBits)(d.dc, 0, 0, 4, 4, 0, 0, 4, 4, NULL, PB(&b), DIB_RGB_COLORS, SRCCOPY);
    t_note("NULL bits -> %d", r);
    r = U(StretchDIBits)(d.dc, 0, 0, 4, 4, 0, 0, 4, 4, src, NULL, DIB_RGB_COLORS, SRCCOPY);
    t_note("NULL bmi -> %d", r);
    r = U(StretchDIBits)(d.dc, 0, 0, 0, 0, 0, 0, 4, 4, src, PB(&b), DIB_RGB_COLORS, SRCCOPY);
    t_note("zero dest size -> %d", r);
    r = U(StretchDIBits)(d.dc, 0, 0, 4, 4, 0, 0, 0, 0, src, PB(&b), DIB_RGB_COLORS, SRCCOPY);
    t_note("zero src size -> %d", r);
    r = U(StretchDIBits)(d.dc, 0, 0, 4, 4, 0, 0, 4, 4, src, PB(&b), DIB_RGB_COLORS, 0xFFFFFFFF);
    t_note("junk rop -> %d", r);
    int bad = 0; for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) if (gp(&d, x, y) != 0) bad++;
    t_note("dest touched pixels after invalid calls: %d", bad);
    dst_close(&d);
}
TEST(dib_stretchdibits_return_values) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 16, 16, 32, 0));
    BMI b; bmi_init(&b, 8, 8, 24, BI_RGB); BYTE src[24 * 8]; img24(src, 8, 8);
    static const int hs[] = { 1, 2, 3, 5, 8 };
    for (int i = 0; i < 5; i++) {
        int r = U(StretchDIBits)(d.dc, 0, 0, 16, 16, 0, 0, 8, hs[i], src, PB(&b), DIB_RGB_COLORS, SRCCOPY);
        CHECKF(r > 0 && r != (int)GDI_ERROR, "hSrc %d -> %d", hs[i], r);
        t_note("StretchDIBits hSrc=%d returned %d", hs[i], r);
    }
    dst_close(&d);
}
TEST(dib_stretchdibits_pal_colors_default) {
    NEED_DRAW();
    if (dib_is_wine()) { t_skip("wine: palette usage"); return; }
    DST d; CHECK(dst_open(&d, 4, 1, 32, 0));
    BMI b; bmi_init(&b, 4, 1, 8, BI_RGB); for (int i = 0; i < 4; i++) b.idx[i] = (WORD)i;
    BYTE src[4] = { 0, 1, 2, 3 };
    int r = U(StretchDIBits)(d.dc, 0, 0, 4, 1, 0, 0, 4, 1, src, PB(&b), DIB_PAL_COLORS, SRCCOPY);
    t_note("StretchDIBits PAL_COLORS r=%d: %06lx %06lx %06lx %06lx", r, (unsigned long)gp(&d, 0, 0), (unsigned long)gp(&d, 1, 0), (unsigned long)gp(&d, 2, 0), (unsigned long)gp(&d, 3, 0));
    dst_close(&d);
}
TEST(dib_stretchdibits_8bpp_src_table) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 4, 2, 32, 0));
    BMI b; bmi_init(&b, 4, 2, 8, BI_RGB); bmi_pal(&b, 256);
    BYTE src[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    EXPECT_EQ(U(StretchDIBits)(d.dc, 0, 0, 4, 2, 0, 0, 4, 2, src, PB(&b), DIB_RGB_COLORS, SRCCOPY), 2);
    for (int x = 0; x < 4; x++) {
        int i = 1 + x;      /* bottom row in memory = y 1 */
        EXPECT_EQ(gp(&d, x, 1), RGB(i ^ 0x55, 255 - i, i));
        i = 5 + x;
        EXPECT_EQ(gp(&d, x, 0), RGB(i ^ 0x55, 255 - i, i));
    }
    dst_close(&d);
}
TEST(dib_stretchdibits_dest_clipping) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 4, 4, 32, RGB(1, 2, 3)));
    BMI b; bmi_init(&b, 4, 4, 24, BI_RGB); BYTE src[12 * 4]; img24(src, 4, 4);
    int r = U(StretchDIBits)(d.dc, 2, 2, 8, 8, 0, 0, 4, 4, src, PB(&b), DIB_RGB_COLORS, SRCCOPY);
    t_note("partially outside r=%d", r);
    EXPECT_EQ(gp(&d, 2, 2), src_col(0, 0)); EXPECT_EQ(gp(&d, 3, 3), src_col(0, 0)); EXPECT_EQ(gp(&d, 1, 1), RGB(1, 2, 3));
    dst_close(&d);
}
TEST(dib_stretchdibits_gdi_balance) {
    NEED_DRAW(); REQUIRE(U(GetGuiResources));
    if (dib_is_wine()) { t_skip("wine: no GDI count"); return; }
    DST d; CHECK(dst_open(&d, 8, 8, 32, 0));
    BMI b; bmi_init(&b, 8, 8, 24, BI_RGB); BYTE src[24 * 8]; img24(src, 8, 8);
    int before = gdicount();
    for (int i = 0; i < 500; i++) {
        U(StretchDIBits)(d.dc, 0, 0, 8, 8, 0, 0, 8, 8, src, PB(&b), DIB_RGB_COLORS, SRCCOPY);
        U(SetDIBitsToDevice)(d.dc, 0, 0, 8, 8, 0, 0, 0, 8, src, PB(&b), DIB_RGB_COLORS);
    }
    EXPECT_EQ(gdicount(), before);
    dst_close(&d);
}

/* ---------- colour table ---------- */
static void ct_roundtrip(int bpp) {
    NEED_DRAW();
    void *bits; HBITMAP hb = mkdib(4, 4, bpp, &bits); CHECK(hb); if (!hb) return;
    HGDIOBJ old; HDC dc = mkdc(hb, &old);
    int n = 1 << bpp; RGBQUAD in[256], out[256];
    for (int i = 0; i < n; i++) { in[i].rgbBlue = i * 3; in[i].rgbGreen = i * 5 + 1; in[i].rgbRed = i * 7 + 2; in[i].rgbReserved = 0; }
    EXPECT_EQ(U(SetDIBColorTable)(dc, 0, n, in), n);
    memset(out, 0xCC, sizeof out);
    EXPECT_EQ(U(GetDIBColorTable)(dc, 0, n, out), n);
    int bad = 0; for (int i = 0; i < n; i++) if (memcmp(&in[i], &out[i], 3)) bad++;
    CHECKF(bad == 0, "bad %d", bad);
    /* request more than exists */
    EXPECT_EQ(U(GetDIBColorTable)(dc, 0, 1000, out), n);
    EXPECT_EQ(U(SetDIBColorTable)(dc, 0, 1000, in), n);
    /* partial window */
    if (n > 2) {
        memset(out, 0, sizeof out);
        EXPECT_EQ(U(GetDIBColorTable)(dc, 1, 2, out), 2);
        EXPECT_EQ(memcmp(&in[1], &out[0], 3), 0); EXPECT_EQ(memcmp(&in[2], &out[1], 3), 0);
        EXPECT_EQ(U(GetDIBColorTable)(dc, n - 1, 5, out), 1);
        EXPECT_EQ(U(GetDIBColorTable)(dc, n, 1, out), 0);
        EXPECT_EQ(U(GetDIBColorTable)(dc, n + 1, 1, out), 0);
        EXPECT_EQ(U(GetDIBColorTable)(dc, 0xFFFFFFFF, 1, out), 0);
        EXPECT_EQ(U(SetDIBColorTable)(dc, n, 1, in), 0);
        EXPECT_EQ(U(SetDIBColorTable)(dc, n - 1, 5, in), 1);
        EXPECT_EQ(U(SetDIBColorTable)(dc, 0xFFFFFFFF, 1, in), 0);
    }
    EXPECT_EQ(U(GetDIBColorTable)(dc, 0, 0, out), 0);
    EXPECT_EQ(U(SetDIBColorTable)(dc, 0, 0, in), 0);
    freedc(dc, old); U(DeleteObject)(hb);
}
TEST(dib_colortable_roundtrip_1) { ct_roundtrip(1); }
TEST(dib_colortable_roundtrip_4) { ct_roundtrip(4); }
TEST(dib_colortable_roundtrip_8) { ct_roundtrip(8); }

TEST(dib_colortable_high_bpp_zero) {
    NEED_DRAW();
    static const int b[] = { 16, 24, 32 };
    for (int i = 0; i < 3; i++) {
        void *bits; HBITMAP hb = mkdib(4, 4, b[i], &bits); CHECK(hb); if (!hb) continue;
        HGDIOBJ old; HDC dc = mkdc(hb, &old); RGBQUAD q[4] = { 0 };
        EXPECT_EQ(U(GetDIBColorTable)(dc, 0, 4, q), 0);
        EXPECT_EQ(U(SetDIBColorTable)(dc, 0, 4, q), 0);
        freedc(dc, old); U(DeleteObject)(hb);
    }
}
TEST(dib_colortable_no_dib_selected) {
    NEED_DRAW();
    HDC dc = U(CreateCompatibleDC)(NULL); REQUIRE(dc);
    RGBQUAD q[4] = { 0 };
    EXPECT_EQ(U(GetDIBColorTable)(dc, 0, 4, q), 0);
    EXPECT_EQ(U(SetDIBColorTable)(dc, 0, 4, q), 0);
    HBITMAP ddb = U(CreateBitmap)(4, 4, 1, 1, NULL);
    if (ddb) {
        HGDIOBJ o = U(SelectObject)(dc, ddb);
        EXPECT_EQ(U(GetDIBColorTable)(dc, 0, 2, q), 0);
        U(SelectObject)(dc, o); U(DeleteObject)(ddb);
    }
    U(DeleteDC)(dc);
}
TEST(dib_colortable_invalid_dc) {
    NEED_DRAW();
    RGBQUAD q[4] = { 0 };
    EXPECT_EQ(U(GetDIBColorTable)(NULL, 0, 4, q), 0);
    EXPECT_EQ(U(SetDIBColorTable)(NULL, 0, 4, q), 0);
    EXPECT_EQ(U(GetDIBColorTable)((HDC)(ULONG_PTR)0x999, 0, 4, q), 0);
    EXPECT_EQ(U(SetDIBColorTable)((HDC)(ULONG_PTR)0x999, 0, 4, q), 0);
    HBRUSH br = U(CreateSolidBrush)(0);
    EXPECT_EQ(U(GetDIBColorTable)((HDC)br, 0, 4, q), 0);
    U(DeleteObject)(br);
}
TEST(dib_colortable_effect_on_getpixel) {
    NEED_DRAW();
    void *bits; HBITMAP hb = mkdib(2, 1, 8, &bits); CHECK(hb); if (!hb) return;
    HGDIOBJ old; HDC dc = mkdc(hb, &old);
    ((BYTE *)bits)[0] = 3; ((BYTE *)bits)[1] = 4;
    RGBQUAD q[2] = { { 10, 20, 30, 0 }, { 40, 50, 60, 0 } }; /* B,G,R */
    EXPECT_EQ(U(SetDIBColorTable)(dc, 3, 2, q), 2);
    EXPECT_EQ(gp(&(DST){ .dc = dc }, 0, 0), RGB(30, 20, 10));
    EXPECT_EQ(gp(&(DST){ .dc = dc }, 1, 0), RGB(60, 50, 40));
    RGBQUAD q2 = { 1, 2, 3, 0 };
    EXPECT_EQ(U(SetDIBColorTable)(dc, 3, 1, &q2), 1);
    EXPECT_EQ(gp(&(DST){ .dc = dc }, 0, 0), RGB(3, 2, 1));
    freedc(dc, old); U(DeleteObject)(hb);
}
TEST(dib_colortable_shared_between_dcs) {
    NEED_DRAW();
    void *bits; HBITMAP hb = mkdib(2, 1, 8, &bits); CHECK(hb); if (!hb) return;
    HGDIOBJ o1; HDC d1 = mkdc(hb, &o1);
    RGBQUAD q = { 5, 6, 7, 0 }; EXPECT_EQ(U(SetDIBColorTable)(d1, 9, 1, &q), 1);
    U(SelectObject)(d1, o1);
    HGDIOBJ o2; HDC d2 = mkdc(hb, &o2);
    RGBQUAD r = { 0 }; EXPECT_EQ(U(GetDIBColorTable)(d2, 9, 1, &r), 1);
    EXPECT_EQ(r.rgbBlue, 5); EXPECT_EQ(r.rgbGreen, 6); EXPECT_EQ(r.rgbRed, 7);
    freedc(d2, o2); U(DeleteDC)(d1); U(DeleteObject)(hb);
}
TEST(dib_colortable_reserved_byte) {
    NEED_DRAW();
    void *bits; HBITMAP hb = mkdib(2, 1, 8, &bits); CHECK(hb); if (!hb) return;
    HGDIOBJ old; HDC dc = mkdc(hb, &old);
    RGBQUAD q = { 1, 2, 3, 0x7F }, r = { 0xCC, 0xCC, 0xCC, 0xCC };
    U(SetDIBColorTable)(dc, 0, 1, &q); U(GetDIBColorTable)(dc, 0, 1, &r);
    t_note("rgbReserved set 0x7F, got 0x%02x", r.rgbReserved);
    EXPECT_EQ(r.rgbBlue, 1); EXPECT_EQ(r.rgbGreen, 2); EXPECT_EQ(r.rgbRed, 3);
    freedc(dc, old); U(DeleteObject)(hb);
}
TEST(dib_colortable_setpixel_nearest) {
    NEED_DRAW();
    void *bits; HBITMAP hb = mkdib(4, 1, 8, &bits); CHECK(hb); if (!hb) return;
    HGDIOBJ old; HDC dc = mkdc(hb, &old);
    RGBQUAD q[4] = { {0,0,0,0}, {0,0,255,0}, {0,255,0,0}, {255,0,0,0} }; /* black, red, green, blue */
    U(SetDIBColorTable)(dc, 0, 4, q);
    U(SetPixelV)(dc, 0, 0, RGB(255, 0, 0)); U(SetPixelV)(dc, 1, 0, RGB(0, 255, 0)); U(SetPixelV)(dc, 2, 0, RGB(0, 0, 255)); U(SetPixelV)(dc, 3, 0, RGB(0, 0, 0));
    U(GdiFlush)();
    BYTE *p = bits; EXPECT_EQ(p[0], 1); EXPECT_EQ(p[1], 2); EXPECT_EQ(p[2], 3); EXPECT_EQ(p[3], 0);
    U(SetPixelV)(dc, 0, 0, RGB(250, 10, 10)); U(GdiFlush)();
    EXPECT_EQ(p[0], 1);
    freedc(dc, old); U(DeleteObject)(hb);
}

/* ---------- packing / direct memory access ---------- */
TEST(dib_pack_1bpp_colors) {
    NEED_DRAW();
    BMI b; void *bits; bmi_init(&b, 8, -1, 1, BI_RGB);
    b.q[0] = (RGBQUAD){ 0, 0, 255, 0 }; b.q[1] = (RGBQUAD){ 255, 0, 0, 0 }; /* idx0 red, idx1 blue */
    HBITMAP hb = U(CreateDIBSection)(NULL, PB(&b), DIB_RGB_COLORS, &bits, NULL, 0); CHECK(hb); if (!hb) return;
    HGDIOBJ old; HDC dc = mkdc(hb, &old);
    *(BYTE *)bits = 0xA5; U(GdiFlush)();
    for (int x = 0; x < 8; x++) {
        int bit = (0xA5 >> (7 - x)) & 1;
        EXPECT_EQ(U(GetPixel)(dc, x, 0), bit ? RGB(0, 0, 255) : RGB(255, 0, 0));
    }
    U(SetPixelV)(dc, 0, 0, RGB(255, 0, 0)); U(SetPixelV)(dc, 7, 0, RGB(0, 0, 255)); U(GdiFlush)();
    EXPECT_EQ(*(BYTE *)bits, 0x25);
    freedc(dc, old); U(DeleteObject)(hb);
}
TEST(dib_pack_4bpp_nibble_order) {
    NEED_DRAW();
    void *bits; HBITMAP hb = mkdib(4, -1, 4, &bits); CHECK(hb); if (!hb) return;
    HGDIOBJ old; HDC dc = mkdc(hb, &old);
    RGBQUAD q[16]; memset(q, 0, sizeof q);
    for (int i = 0; i < 16; i++) { q[i].rgbRed = i * 16; q[i].rgbGreen = 255 - i * 16; q[i].rgbBlue = i; }
    U(SetDIBColorTable)(dc, 0, 16, q);
    BYTE *p = bits; p[0] = 0x3A; p[1] = 0xC5; U(GdiFlush)();
    EXPECT_EQ(U(GetPixel)(dc, 0, 0), RGB(3 * 16, 255 - 48, 3));
    EXPECT_EQ(U(GetPixel)(dc, 1, 0), RGB(10 * 16, 255 - 160, 10));
    EXPECT_EQ(U(GetPixel)(dc, 2, 0), RGB(12 * 16, 255 - 192, 12));
    EXPECT_EQ(U(GetPixel)(dc, 3, 0), RGB(5 * 16, 255 - 80, 5));
    U(SetPixelV)(dc, 0, 0, RGB(15 * 16, 255 - 240, 15)); U(SetPixelV)(dc, 3, 0, RGB(1 * 16, 255 - 16, 1)); U(GdiFlush)();
    EXPECT_EQ(p[0], 0xFA); EXPECT_EQ(p[1], 0xC1);
    freedc(dc, old); U(DeleteObject)(hb);
}
TEST(dib_pack_8bpp_index_write) {
    NEED_DRAW();
    void *bits; HBITMAP hb = mkdib(4, -1, 8, &bits); CHECK(hb); if (!hb) return;
    HGDIOBJ old; HDC dc = mkdc(hb, &old);
    for (int i = 0; i < 4; i++) ((BYTE *)bits)[i] = 100 + i * 50;
    U(GdiFlush)();
    for (int i = 0; i < 4; i++) { int k = 100 + i * 50; EXPECT_EQ(U(GetPixel)(dc, i, 0), RGB(k ^ 0x55, 255 - k, k)); }
    freedc(dc, old); U(DeleteObject)(hb);
}
TEST(dib_pack_16bpp_555_read) {
    NEED_DRAW();
    void *bits; HBITMAP hb = mkdib(4, -1, 16, &bits); CHECK(hb); if (!hb) return;
    HGDIOBJ old; HDC dc = mkdc(hb, &old);
    WORD *p = bits; p[0] = 0x7C00; p[1] = 0x03E0; p[2] = 0x001F; p[3] = 0x7FFF; U(GdiFlush)();
    EXPECT_EQ(U(GetPixel)(dc, 0, 0), RGB(255, 0, 0)); EXPECT_EQ(U(GetPixel)(dc, 1, 0), RGB(0, 255, 0));
    EXPECT_EQ(U(GetPixel)(dc, 2, 0), RGB(0, 0, 255)); EXPECT_EQ(U(GetPixel)(dc, 3, 0), RGB(255, 255, 255));
    p[0] = 0x4210; U(GdiFlush)(); /* r=16 g=16 b=16 */
    t_note("0x4210 in 555 -> %06lx", (unsigned long)U(GetPixel)(dc, 0, 0));
    p[0] = 0x8000; U(GdiFlush)();
    t_note("0x8000 (top bit) in 555 -> %06lx", (unsigned long)U(GetPixel)(dc, 0, 0));
    freedc(dc, old); U(DeleteObject)(hb);
}
TEST(dib_pack_16bpp_565_read) {
    NEED_DRAW();
    BMI b; void *bits; bmi_init(&b, 4, -1, 16, BI_BITFIELDS); b.m[0] = 0xF800; b.m[1] = 0x07E0; b.m[2] = 0x001F;
    HBITMAP hb = U(CreateDIBSection)(NULL, PB(&b), DIB_RGB_COLORS, &bits, NULL, 0); CHECK(hb); if (!hb) return;
    HGDIOBJ old; HDC dc = mkdc(hb, &old);
    WORD *p = bits; p[0] = 0xF800; p[1] = 0x07E0; p[2] = 0x001F; p[3] = 0xFFFF; U(GdiFlush)();
    EXPECT_EQ(U(GetPixel)(dc, 0, 0), RGB(255, 0, 0)); EXPECT_EQ(U(GetPixel)(dc, 1, 0), RGB(0, 255, 0));
    EXPECT_EQ(U(GetPixel)(dc, 2, 0), RGB(0, 0, 255)); EXPECT_EQ(U(GetPixel)(dc, 3, 0), RGB(255, 255, 255));
    freedc(dc, old); U(DeleteObject)(hb);
}
TEST(dib_pack_24bpp_bgr_order) {
    NEED_DRAW();
    void *bits; HBITMAP hb = mkdib(2, -1, 24, &bits); CHECK(hb); if (!hb) return;
    HGDIOBJ old; HDC dc = mkdc(hb, &old);
    U(SetPixelV)(dc, 0, 0, RGB(0x11, 0x22, 0x33)); U(SetPixelV)(dc, 1, 0, RGB(0xAA, 0xBB, 0xCC)); U(GdiFlush)();
    BYTE *p = bits; EXPECT_EQ(p[0], 0x33); EXPECT_EQ(p[1], 0x22); EXPECT_EQ(p[2], 0x11);
    EXPECT_EQ(p[3], 0xCC); EXPECT_EQ(p[4], 0xBB); EXPECT_EQ(p[5], 0xAA);
    p[0] = 1; p[1] = 2; p[2] = 3; U(GdiFlush)();
    EXPECT_EQ(U(GetPixel)(dc, 0, 0), RGB(3, 2, 1));
    freedc(dc, old); U(DeleteObject)(hb);
}
TEST(dib_pack_32bpp_order_and_alpha) {
    NEED_DRAW();
    void *bits; HBITMAP hb = mkdib(2, -1, 32, &bits); CHECK(hb); if (!hb) return;
    HGDIOBJ old; HDC dc = mkdc(hb, &old);
    ((DWORD *)bits)[0] = 0xFF010203; ((DWORD *)bits)[1] = 0x00112233; U(GdiFlush)();
    EXPECT_EQ(U(GetPixel)(dc, 0, 0), RGB(1, 2, 3)); EXPECT_EQ(U(GetPixel)(dc, 1, 0), RGB(0x11, 0x22, 0x33));
    ((DWORD *)bits)[0] = 0xFF000000;
    U(SetPixelV)(dc, 0, 0, RGB(0x44, 0x55, 0x66)); U(GdiFlush)();
    t_note("32bpp dword after SetPixelV over alpha FF: 0x%08lx", ((DWORD *)bits)[0]);
    EXPECT_EQ(((DWORD *)bits)[0] & 0xFFFFFF, 0x445566);
    HBRUSH br = U(CreateSolidBrush)(RGB(1, 2, 3)); RECT rc = { 0, 0, 2, 1 };
    BOOL (WINAPI *pat)(HDC, int, int, int, int, DWORD) = (void *)U(PatBlt);
    if (pat) {
        HGDIOBJ ob = U(SelectObject)(dc, br);
        pat(dc, 0, 0, 2, 1, PATCOPY); U(GdiFlush)();
        t_note("32bpp dword after PatBlt: 0x%08lx 0x%08lx", ((DWORD *)bits)[0], ((DWORD *)bits)[1]);
        EXPECT_EQ(((DWORD *)bits)[0] & 0xFFFFFF, 0x010203);
        U(SelectObject)(dc, ob);
    }
    (void)rc; U(DeleteObject)(br);
    freedc(dc, old); U(DeleteObject)(hb);
}
TEST(dib_poke_visible_to_getpixel_all_depths) {
    NEED_DRAW();
    static const int b[] = { 16, 24, 32 };
    for (int i = 0; i < 3; i++) {
        void *bits; HBITMAP hb = mkdib(8, 8, b[i], &bits); CHECK(hb); if (!hb) continue;
        HGDIOBJ old; HDC dc = mkdc(hb, &old);
        int st = dib_stride(8, b[i]);
        for (int k = 0; k < 20; k++) {
            int x = (k * 3) % 8, y = (k * 5) % 8; BYTE *row = (BYTE *)bits + (7 - y) * st;
            if (b[i] == 16) ((WORD *)row)[x] = (k & 1) ? 0x7FFF : 0;
            else if (b[i] == 24) { row[x * 3] = row[x * 3 + 1] = row[x * 3 + 2] = (k & 1) ? 255 : 0; }
            else ((DWORD *)row)[x] = (k & 1) ? 0xFFFFFF : 0;
            U(GdiFlush)();
            COLORREF g = U(GetPixel)(dc, x, y);
            if (g != ((k & 1) ? RGB(255, 255, 255) : 0)) { CHECKF(0, "bpp %d k=%d got %06lx", b[i], k, (unsigned long)g); break; }
        }
        freedc(dc, old); U(DeleteObject)(hb);
    }
}
TEST(dib_gdi_write_visible_in_memory_all_depths) {
    NEED_DRAW();
    static const int b[] = { 16, 24, 32 };
    for (int i = 0; i < 3; i++) {
        void *bits; HBITMAP hb = mkdib(8, 8, b[i], &bits); CHECK(hb); if (!hb) continue;
        HGDIOBJ old; HDC dc = mkdc(hb, &old); int st = dib_stride(8, b[i]);
        for (int k = 0; k < 20; k++) {
            int x = (k * 3) % 8, y = (k * 5) % 8; BYTE *row = (BYTE *)bits + (7 - y) * st;
            U(SetPixelV)(dc, x, y, (k & 1) ? RGB(255, 255, 255) : RGB(0, 0, 0));
            U(GdiFlush)();
            int v;
            if (b[i] == 16) v = ((WORD *)row)[x] == ((k & 1) ? 0x7FFF : 0);
            else if (b[i] == 24) v = row[x*3] == ((k & 1) ? 255 : 0) && row[x*3+1] == row[x*3] && row[x*3+2] == row[x*3];
            else v = (((DWORD *)row)[x] & 0xFFFFFF) == ((k & 1) ? 0xFFFFFF : 0);
            if (!v) { CHECKF(0, "bpp %d k=%d", b[i], k); break; }
        }
        freedc(dc, old); U(DeleteObject)(hb);
    }
}
TEST(dib_fillrect_32bpp_exact) {
    NEED_DRAW(); REQUIRE(U(FillRect));
    DST d; CHECK(dst_open(&d, 8, 8, 32, 0));
    HBRUSH br = U(CreateSolidBrush)(RGB(0x12, 0x34, 0x56)); RECT rc = { 2, 2, 6, 5 };
    U(FillRect)(d.dc, &rc, br); U(GdiFlush)();
    int bad = 0;
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
        int in = x >= 2 && x < 6 && y >= 2 && y < 5;
        DWORD v = ((DWORD *)d.bits)[(7 - y) * 8 + x] & 0xFFFFFF;
        if (v != (in ? 0x123456u : 0u)) bad++;
    }
    CHECKF(bad == 0, "bad %d", bad);
    U(DeleteObject)(br); dst_close(&d);
}
TEST(dib_bitblt_between_dibs_exact) {
    NEED_DRAW(); REQUIRE(U(BitBlt));
    DST a, b2; CHECK(dst_open(&a, 8, 8, 24, 0)); CHECK(dst_open(&b2, 8, 8, 32, 0));
    BMI bi; bmi_init(&bi, 8, 8, 24, BI_RGB); BYTE src[24 * 8]; img24(src, 8, 8);
    U(SetDIBitsToDevice)(a.dc, 0, 0, 8, 8, 0, 0, 0, 8, src, PB(&bi), DIB_RGB_COLORS);
    EXPECT_NE(U(BitBlt)(b2.dc, 0, 0, 8, 8, a.dc, 0, 0, SRCCOPY), 0);
    int bad = 0; for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) if (gp(&b2, x, y) != src_col(x, y)) bad++;
    CHECKF(bad == 0, "bad %d", bad);
    dst_close(&a); dst_close(&b2);
}
TEST(dib_gdiflush_returns_true) {
    NEED_DRAW();
    EXPECT_NE(U(GdiFlush)(), 0);
    for (int i = 0; i < 100; i++) U(GdiFlush)();
}
TEST(dib_stride_padding_untouched) {
    NEED_DRAW();
    void *bits; HBITMAP hb = mkdib(3, 2, 24, &bits); CHECK(hb); if (!hb) return;
    HGDIOBJ old; HDC dc = mkdc(hb, &old);
    memset(bits, 0xEE, 12 * 2);
    for (int x = 0; x < 3; x++) for (int y = 0; y < 2; y++) U(SetPixelV)(dc, x, y, RGB(1, 2, 3));
    U(GdiFlush)();
    BYTE *p = bits;
    EXPECT_EQ(p[9], 0xEE); EXPECT_EQ(p[10], 0xEE); EXPECT_EQ(p[11], 0xEE);
    EXPECT_EQ(p[21], 0xEE);
    freedc(dc, old); U(DeleteObject)(hb);
}
TEST(dib_truncated_color_table_unused_indices) {
    NEED_DRAW();
    BMI b; void *bits; bmi_init(&b, 4, -1, 8, BI_RGB); b.h.biClrUsed = 2;
    b.q[0] = (RGBQUAD){ 0, 0, 255, 0 }; b.q[1] = (RGBQUAD){ 255, 0, 0, 0 };
    HBITMAP hb = U(CreateDIBSection)(NULL, PB(&b), DIB_RGB_COLORS, &bits, NULL, 0); CHECK(hb); if (!hb) return;
    HGDIOBJ old; HDC dc = mkdc(hb, &old);
    ((BYTE *)bits)[0] = 0; ((BYTE *)bits)[1] = 1; ((BYTE *)bits)[2] = 2; ((BYTE *)bits)[3] = 200; U(GdiFlush)();
    EXPECT_EQ(U(GetPixel)(dc, 0, 0), RGB(255, 0, 0)); EXPECT_EQ(U(GetPixel)(dc, 1, 0), RGB(0, 0, 255));
    t_note("index beyond clrUsed -> %06lx / %06lx", (unsigned long)U(GetPixel)(dc, 2, 0), (unsigned long)U(GetPixel)(dc, 3, 0));
    RGBQUAD q[256]; t_note("table size with clrUsed=2: %u", U(GetDIBColorTable)(dc, 0, 256, q));
    freedc(dc, old); U(DeleteObject)(hb);
}
TEST(dib_sdtd_truncated_color_table_clrused) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 4, 1, 32, 0));
    BMI b; bmi_init(&b, 4, 1, 8, BI_RGB); b.h.biClrUsed = 2;
    b.q[0] = (RGBQUAD){ 0, 0, 255, 0 }; b.q[1] = (RGBQUAD){ 255, 0, 0, 0 };
    BYTE src[4] = { 0, 1, 0, 1 };
    EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 0, 0, 4, 1, 0, 0, 0, 1, src, PB(&b), DIB_RGB_COLORS), 1);
    EXPECT_EQ(gp(&d, 0, 0), RGB(255, 0, 0)); EXPECT_EQ(gp(&d, 1, 0), RGB(0, 0, 255));
    dst_close(&d);
}
TEST(dib_sdtd_oversized_header_color_table) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 4, 1, 32, 0));
    BYTE buf[sizeof(BITMAPV5HEADER) + 1024]; memset(buf, 0, sizeof buf);
    BITMAPV5HEADER *h = (BITMAPV5HEADER *)buf; h->bV5Size = sizeof *h; h->bV5Width = 4; h->bV5Height = 1; h->bV5Planes = 1; h->bV5BitCount = 8; h->bV5Compression = BI_RGB;
    RGBQUAD *q = (RGBQUAD *)(buf + sizeof *h); q[0] = (RGBQUAD){ 0, 0, 255, 0 }; q[1] = (RGBQUAD){ 255, 0, 0, 0 }; q[2] = (RGBQUAD){ 0, 255, 0, 0 };
    BYTE src[4] = { 0, 1, 2, 1 };
    EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 0, 0, 4, 1, 0, 0, 0, 1, src, (BITMAPINFO *)buf, DIB_RGB_COLORS), 1);
    EXPECT_EQ(gp(&d, 0, 0), RGB(255, 0, 0)); EXPECT_EQ(gp(&d, 1, 0), RGB(0, 0, 255)); EXPECT_EQ(gp(&d, 2, 0), RGB(0, 255, 0));
    dst_close(&d);
}
TEST(dib_sdtd_repeat_stable) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 8, 8, 32, 0));
    BMI b; bmi_init(&b, 8, 8, 24, BI_RGB); BYTE src[24 * 8]; img24(src, 8, 8);
    for (int i = 0; i < 200; i++) {
        int r = U(SetDIBitsToDevice)(d.dc, 0, 0, 8, 8, 0, 0, 0, 8, src, PB(&b), DIB_RGB_COLORS);
        if (r != 8) { CHECKF(0, "iteration %d -> %d", i, r); break; }
    }
    EXPECT_EQ(gp(&d, 7, 7), src_col(7, 7));
    dst_close(&d);
}
TEST(dib_sdtd_unaligned_bits) {
    NEED_DRAW();
    DST d; CHECK(dst_open(&d, 4, 4, 32, 0));
    BMI b; bmi_init(&b, 4, 4, 24, BI_RGB); BYTE raw[12 * 4 + 8]; BYTE tmp[12 * 4]; img24(tmp, 4, 4);
    for (int off = 0; off < 4; off++) {
        memset(raw, 0, sizeof raw); memcpy(raw + off, tmp, sizeof tmp);
        EXPECT_EQ(U(SetDIBitsToDevice)(d.dc, 0, 0, 4, 4, 0, 0, 0, 4, raw + off, PB(&b), DIB_RGB_COLORS), 4);
        EXPECT_EQ(gp(&d, 2, 1), src_col(2, 1));
        U(PatBlt)(d.dc, 0, 0, 4, 4, BLACKNESS);
    }
    dst_close(&d);
}
TEST(dib_sdtd_to_ddb_dc) {
    NEED_DRAW(); REQUIRE(U(CreateBitmap));
    HDC dc = U(CreateCompatibleDC)(NULL); REQUIRE(dc);
    HBITMAP ddb = U(CreateBitmap)(8, 8, 1, 1, NULL); HGDIOBJ o = U(SelectObject)(dc, ddb);
    BMI b; bmi_init(&b, 8, 8, 24, BI_RGB); BYTE src[24 * 8]; img24(src, 8, 8);
    int r = U(SetDIBitsToDevice)(dc, 0, 0, 8, 8, 0, 0, 0, 8, src, PB(&b), DIB_RGB_COLORS);
    EXPECT_EQ(r, 8);
    COLORREF c = U(GetPixel)(dc, 0, 0);
    CHECK(c == RGB(0, 0, 0) || c == RGB(255, 255, 255));
    U(SelectObject)(dc, o); U(DeleteObject)(ddb); U(DeleteDC)(dc);
}
