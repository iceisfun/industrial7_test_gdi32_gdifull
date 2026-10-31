#include "dibh.h"

#define W 6
#define H 5
static BYTE pr(int x, int y) { return (BYTE)(x * 40 + 10); }
static BYTE pg(int x, int y) { return (BYTE)(y * 50 + 5); }
static BYTE pb(int x, int y) { return (BYTE)((x + y) * 20 + 1); }

/* top-down 32bpp source filled by direct memory writes; row y == scanline y */
static HBITMAP src32(void) {
    void *bits; HBITMAP hb = mkdib(W, -H, 32, &bits);
    if (!hb) return NULL;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
        ((DWORD *)bits)[y * W + x] = (pr(x, y) << 16) | (pg(x, y) << 8) | pb(x, y);
    return hb;
}
/* read-back request of bpp from hb with positive (bottom-up) or negative height */
static int getbits(HDC dc, HBITMAP hb, int bpp, int topdown, BYTE *buf, BMI *out, UINT start, UINT lines) {
    BMI b; bmi_init(&b, 0, 0, 0, BI_RGB);
    b.h.biBitCount = 0;
    if (!U(GetDIBits)(dc, hb, 0, 0, NULL, PB(&b), DIB_RGB_COLORS)) return -1;
    b.h.biBitCount = (WORD)bpp; b.h.biCompression = BI_RGB; b.h.biSizeImage = 0;
    if (topdown) b.h.biHeight = -abs(b.h.biHeight);
    else b.h.biHeight = abs(b.h.biHeight);
    int r = U(GetDIBits)(dc, hb, start, lines, buf, PB(&b), DIB_RGB_COLORS);
    if (out) *out = b;
    return r;
}
#define NEED_BITS() do { NEED_GDI(); REQUIRE(U(GetDIBits)); REQUIRE(U(SetDIBits)); } while (0)

/* ---------- GetDIBits: header query ---------- */
static void q_hdr(int bpp) {
    NEED_BITS();
    void *bits; HBITMAP hb = mkdib(W, H, bpp, &bits); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL); REQUIRE(dc);
    BMI b; bmi_init(&b, 0, 0, 0, 0);
    int r = U(GetDIBits)(dc, hb, 0, 0, NULL, PB(&b), DIB_RGB_COLORS);
    CHECK(r != 0);
    EXPECT_EQ(b.h.biSize, sizeof(BITMAPINFOHEADER));
    EXPECT_EQ(b.h.biWidth, W);
    EXPECT_EQ(abs(b.h.biHeight), H);
    EXPECT_EQ(b.h.biPlanes, 1);
    EXPECT_EQ(b.h.biBitCount, bpp);
    EXPECT_EQ(b.h.biSizeImage, dib_stride(W, bpp) * H);
    t_note("bpp %d query: compression=%lu height=%ld clrused=%lu", bpp, b.h.biCompression, (long)b.h.biHeight, b.h.biClrUsed);
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}
TEST(dib_getdibits_query_1) { q_hdr(1); }
TEST(dib_getdibits_query_4) { q_hdr(4); }
TEST(dib_getdibits_query_8) { q_hdr(8); }
TEST(dib_getdibits_query_16) { q_hdr(16); }
TEST(dib_getdibits_query_24) { q_hdr(24); }
TEST(dib_getdibits_query_32) { q_hdr(32); }

TEST(dib_getdibits_query_topdown_source) {
    NEED_BITS();
    void *bits; HBITMAP hb = mkdib(W, -H, 24, &bits); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    BMI b; bmi_init(&b, 0, 0, 0, 0);
    CHECK(U(GetDIBits)(dc, hb, 0, 0, NULL, PB(&b), DIB_RGB_COLORS));
    EXPECT_EQ(b.h.biWidth, W); EXPECT_EQ(abs(b.h.biHeight), H);
    t_note("top-down source query biHeight=%ld", (long)b.h.biHeight);
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}
/* request depth with NULL bits: sizeimage + table */
static void q_req(int bpp) {
    NEED_BITS();
    HBITMAP hb = src32(); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    BMI b; bmi_init(&b, W, H, bpp, BI_RGB);
    int r = U(GetDIBits)(dc, hb, 0, H, NULL, PB(&b), DIB_RGB_COLORS);
    CHECKF(r != 0, "r=%d", r);
    EXPECT_EQ(b.h.biBitCount, bpp);
    EXPECT_EQ(b.h.biWidth, W);
    EXPECT_EQ(b.h.biSizeImage, dib_stride(W, bpp) * H);
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}
TEST(dib_getdibits_query_request_1) { q_req(1); }
TEST(dib_getdibits_query_request_4) { q_req(4); }
TEST(dib_getdibits_query_request_8) { q_req(8); }
TEST(dib_getdibits_query_request_16) { q_req(16); }
TEST(dib_getdibits_query_request_24) { q_req(24); }
TEST(dib_getdibits_query_request_32) { q_req(32); }

TEST(dib_getdibits_query_8bpp_colors_from_dib) {
    NEED_BITS();
    void *bits; HBITMAP hb = mkdib(4, 4, 8, &bits); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    BMI b; bmi_init(&b, 0, 0, 0, 0); b.h.biBitCount = 0;
    CHECK(U(GetDIBits)(dc, hb, 0, 0, NULL, PB(&b), DIB_RGB_COLORS));
    EXPECT_EQ(b.h.biBitCount, 8);
    int bad = 0;
    for (int i = 0; i < 256; i++)
        if (b.q[i].rgbBlue != i || b.q[i].rgbGreen != 255 - i || b.q[i].rgbRed != (i ^ 0x55)) bad++;
    if (bad) t_note("query on DIB section: %d of 256 colour entries differ from the section table", bad);
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}

/* ---------- GetDIBits: pixels ---------- */
static void px_conv(int bpp, int topdown) {
    NEED_BITS();
    HBITMAP hb = src32(); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    BYTE buf[W * 4 * H + 64]; memset(buf, 0xEE, sizeof buf);
    BMI out;
    int r = getbits(dc, hb, bpp, topdown, buf, &out, 0, H);
    EXPECT_EQ(r, H);
    int st = dib_stride(W, bpp), bad = 0;
    for (int y = 0; y < H; y++) {
        int row = topdown ? y : (H - 1 - y);
        BYTE *p = buf + row * st;
        for (int x = 0; x < W; x++) {
            BYTE R = pr(x, y), G = pg(x, y), B = pb(x, y);
            if (bpp == 24) { if (p[x*3] != B || p[x*3+1] != G || p[x*3+2] != R) bad++; }
            else if (bpp == 32) { if (p[x*4] != B || p[x*4+1] != G || p[x*4+2] != R) bad++; }
            else if (bpp == 16) { WORD v = ((WORD *)p)[x]; WORD e = ((R >> 3) << 10) | ((G >> 3) << 5) | (B >> 3); if (v != e) bad++; }
        }
    }
    CHECKF(bad == 0, "bad pixels %d", bad);
    EXPECT_EQ(buf[sizeof buf - 1], 0xEE); /* no overrun */
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}
TEST(dib_getdibits_pixels_16_bottomup) { px_conv(16, 0); }
TEST(dib_getdibits_pixels_24_bottomup) { px_conv(24, 0); }
TEST(dib_getdibits_pixels_32_bottomup) { px_conv(32, 0); }
TEST(dib_getdibits_pixels_16_topdown) { px_conv(16, 1); }
TEST(dib_getdibits_pixels_24_topdown) { px_conv(24, 1); }
TEST(dib_getdibits_pixels_32_topdown) { px_conv(32, 1); }

/* same depth round-trip with a palette */
static void px_pal(int bpp) {
    NEED_BITS();
    void *bits; int w = 9, h = 4; HBITMAP hb = mkdib(w, -h, bpp, &bits); CHECK(hb); if (!hb) return;
    int st = dib_stride(w, bpp), n = 1 << bpp;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        int idx = (x * 7 + y * 3 + 1) % n; BYTE *row = (BYTE *)bits + y * st;
        if (bpp == 8) row[x] = idx;
        else if (bpp == 4) row[x / 2] = (x & 1) ? ((row[x / 2] & 0xF0) | idx) : ((row[x / 2] & 0x0F) | (idx << 4));
        else if (idx) row[x / 8] |= 0x80 >> (x & 7);
    }
    HDC dc = U(CreateCompatibleDC)(NULL);
    BYTE buf[256]; memset(buf, 0, sizeof buf); BMI out;
    EXPECT_EQ(getbits(dc, hb, bpp, 1, buf, &out, 0, h), h);
    EXPECT_EQ(memcmp(buf, bits, (size_t)st * h), 0);
    int bad = 0;
    for (int i = 0; i < n; i++)
        if (out.q[i].rgbBlue != i || out.q[i].rgbGreen != 255 - i || out.q[i].rgbRed != (i ^ 0x55)) bad++;
    CHECKF(bad == 0, "table mismatches %d", bad);
    /* bottom-up request: rows reversed */
    memset(buf, 0, sizeof buf);
    EXPECT_EQ(getbits(dc, hb, bpp, 0, buf, &out, 0, h), h);
    int rb = 0;
    for (int y = 0; y < h; y++) if (memcmp(buf + (h - 1 - y) * st, (BYTE *)bits + y * st, st)) rb++;
    CHECKF(rb == 0, "row mismatches %d", rb);
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}
TEST(dib_getdibits_roundtrip_1) { px_pal(1); }
TEST(dib_getdibits_roundtrip_4) { px_pal(4); }
TEST(dib_getdibits_roundtrip_8) { px_pal(8); }

/* scanline subsets */
TEST(dib_getdibits_scan_subset) {
    NEED_BITS();
    HBITMAP hb = src32(); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    for (UINT start = 0; start < H; start++) for (UINT lines = 1; start + lines <= H; lines++) {
        BYTE buf[W * 4 * H]; memset(buf, 0xCD, sizeof buf); BMI out;
        int r = getbits(dc, hb, 32, 0, buf, &out, start, lines);
        CHECKF(r == (int)lines, "start %u lines %u -> %d", start, lines, r);
        int st = W * 4;
        for (UINT i = 0; i < lines; i++) {
            int y = H - 1 - (start + i);
            for (int x = 0; x < W; x++) {
                BYTE *p = buf + i * st + x * 4;
                if (p[0] != pb(x, y) || p[1] != pg(x, y) || p[2] != pr(x, y)) { CHECKF(0, "start %u lines %u row %u x %d", start, lines, i, x); goto next; }
            }
        }
        for (UINT i = lines * st; i < sizeof buf; i++) if (buf[i] != 0xCD) { CHECKF(0, "wrote past lines (start %u lines %u)", start, lines); break; }
    next:;
    }
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}
TEST(dib_getdibits_scan_out_of_range) {
    NEED_BITS();
    HBITMAP hb = src32(); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    BYTE buf[W * 4 * H * 2]; BMI out;
    SetLastError(0);
    int r = getbits(dc, hb, 32, 0, buf, &out, H, 1);
    t_note("start==h -> %d le=%lu", r, GetLastError());
    r = getbits(dc, hb, 32, 0, buf, &out, H + 100, 1);
    t_note("start==h+100 -> %d", r);
    r = getbits(dc, hb, 32, 0, buf, &out, 0xFFFFFFFF, 1);
    t_note("start==-1 -> %d", r);
    r = getbits(dc, hb, 32, 0, buf, &out, 0, H + 3);
    t_note("cLines>h -> %d", r);
    r = getbits(dc, hb, 32, 0, buf, &out, 3, 100);
    t_note("start 3, lines 100 -> %d", r);
    r = getbits(dc, hb, 32, 0, buf, &out, 0, 0);
    t_note("cLines=0 with buffer -> %d", r);
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}
TEST(dib_getdibits_single_line_each) {
    NEED_BITS();
    HBITMAP hb = src32(); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    for (int s = 0; s < H; s++) {
        BYTE buf[W * 3 + 8]; memset(buf, 0, sizeof buf); BMI out;
        EXPECT_EQ(getbits(dc, hb, 24, 1, buf, &out, s, 1), 1);
        /* top-down request: scanline s == y s */
        int y1 = s, y2 = H - 1 - s;
        int a = buf[2] == pr(0, y1) && buf[1] == pg(0, y1) && buf[0] == pb(0, y1);
        int b2 = buf[2] == pr(0, y2) && buf[1] == pg(0, y2) && buf[0] == pb(0, y2);
        CHECKF(a || b2, "scan %d", s);
        t_note("top-down request, start=%d gives source y=%d", s, a ? y1 : y2);
    }
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}

/* BI_BITFIELDS requests */
static void bf_req(int bpp, DWORD mr, DWORD mg, DWORD mb, WORD (*enc)(BYTE, BYTE, BYTE)) {
    NEED_BITS();
    if (dib_is_wine() && bpp == 16) { t_skip("wine: clobbers caller bitfield masks"); return; }
    HBITMAP hb = src32(); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    BMI b; bmi_init(&b, W, -H, bpp, BI_BITFIELDS); b.m[0] = mr; b.m[1] = mg; b.m[2] = mb;
    BYTE buf[W * 4 * H];
    int r = U(GetDIBits)(dc, hb, 0, H, buf, PB(&b), DIB_RGB_COLORS);
    EXPECT_EQ(r, H);
    t_note("bitfields req: out compression=%lu masks %lx %lx %lx", b.h.biCompression, b.m[0], b.m[1], b.m[2]);
    if (r == H && enc) {
        int bad = 0, st = dib_stride(W, bpp);
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
            if (((WORD *)(buf + y * st))[x] != enc(pr(x, y), pg(x, y), pb(x, y))) bad++;
        CHECKF(bad == 0, "bad %d", bad);
    }
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}
static WORD enc565(BYTE r, BYTE g, BYTE b) { return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3); }
static WORD enc555(BYTE r, BYTE g, BYTE b) { return ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3); }
TEST(dib_getdibits_bitfields_565) { bf_req(16, 0xF800, 0x07E0, 0x001F, enc565); }
TEST(dib_getdibits_bitfields_555) { bf_req(16, 0x7C00, 0x03E0, 0x001F, enc555); }
TEST(dib_getdibits_bitfields_32) { bf_req(32, 0xFF0000, 0xFF00, 0xFF, NULL); }

TEST(dib_getdibits_bitfields_query_masks) {
    NEED_BITS();
    static const int bpps[] = { 16, 32 };
    for (int i = 0; i < 2; i++) {
        void *bits; HBITMAP hb = mkdib(4, 4, bpps[i], &bits); CHECK(hb); if (!hb) continue;
        HDC dc = U(CreateCompatibleDC)(NULL);
        BMI b; bmi_init(&b, 0, 0, 0, 0);
        CHECK(U(GetDIBits)(dc, hb, 0, 0, NULL, PB(&b), DIB_RGB_COLORS));
        t_note("bpp %d: compression=%lu masks %08lx %08lx %08lx", bpps[i], b.h.biCompression, b.m[0], b.m[1], b.m[2]);
        /* second call asking for the colour table / masks of the same format */
        BMI c = b;
        CHECK(U(GetDIBits)(dc, hb, 0, 0, NULL, PB(&c), DIB_RGB_COLORS));
        U(DeleteDC)(dc); U(DeleteObject)(hb);
    }
}

/* core header requests */
TEST(dib_getdibits_core_header) {
    NEED_BITS();
    HBITMAP hb = src32(); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    BYTE bb[sizeof(BITMAPCOREHEADER) + 8]; memset(bb, 0, sizeof bb);
    BITMAPCOREINFO *c = (BITMAPCOREINFO *)bb;
    c->bmciHeader.bcSize = sizeof(BITMAPCOREHEADER);
    c->bmciHeader.bcWidth = W; c->bmciHeader.bcHeight = H; c->bmciHeader.bcPlanes = 1; c->bmciHeader.bcBitCount = 24;
    BYTE buf[(W * 3 + 3) / 4 * 4 * H]; memset(buf, 0, sizeof buf);
    int r = U(GetDIBits)(dc, hb, 0, H, buf, (BITMAPINFO *)c, DIB_RGB_COLORS);
    EXPECT_EQ(r, H);
    int st = (W * 3 + 3) / 4 * 4, bad = 0;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        BYTE *p = buf + (H - 1 - y) * st + x * 3;
        if (p[0] != pb(x, y) || p[1] != pg(x, y) || p[2] != pr(x, y)) bad++;
    }
    CHECKF(bad == 0, "bad %d", bad);
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}
TEST(dib_getdibits_core_header_query) {
    NEED_BITS();
    HBITMAP hb = src32(); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    BYTE bb[sizeof(BITMAPCOREHEADER) + 3 * 256]; memset(bb, 0, sizeof bb);
    BITMAPCOREINFO *c = (BITMAPCOREINFO *)bb;
    c->bmciHeader.bcSize = sizeof(BITMAPCOREHEADER);
    int r = U(GetDIBits)(dc, hb, 0, 0, NULL, (BITMAPINFO *)c, DIB_RGB_COLORS);
    t_note("core query r=%d w=%u h=%u bc=%u", r, c->bmciHeader.bcWidth, c->bmciHeader.bcHeight, c->bmciHeader.bcBitCount);
    if (r) { EXPECT_EQ(c->bmciHeader.bcWidth, W); EXPECT_EQ(c->bmciHeader.bcHeight, H); EXPECT_EQ(c->bmciHeader.bcPlanes, 1); }
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}

/* V4 / V5 header requests */
static void v45(DWORD size) {
    NEED_BITS();
    HBITMAP hb = src32(); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    BYTE bb[sizeof(BITMAPV5HEADER) + 1100]; memset(bb, 0, sizeof bb);
    BITMAPV5HEADER *h = (BITMAPV5HEADER *)bb;
    h->bV5Size = size; h->bV5Width = W; h->bV5Height = H; h->bV5Planes = 1; h->bV5BitCount = 24; h->bV5Compression = BI_RGB;
    BYTE buf[(W * 3 + 3) / 4 * 4 * H];
    int r = U(GetDIBits)(dc, hb, 0, H, buf, (BITMAPINFO *)bb, DIB_RGB_COLORS);
    EXPECT_EQ(r, H);
    if (r == H) { int st = (W * 3 + 3) / 4 * 4; CHECK(buf[(H - 1) * st + 2] == pr(0, 0) && buf[(H - 1) * st] == pb(0, 0)); }
    EXPECT_EQ(h->bV5Size, size);
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}
TEST(dib_getdibits_v4_header) { v45(sizeof(BITMAPV4HEADER)); }
TEST(dib_getdibits_v5_header) { v45(sizeof(BITMAPV5HEADER)); }

/* mono DDB conversions are deterministic */
static HBITMAP mono8(void) {
    BYTE d[8] = { 0xAA, 0x55, 0xF0, 0x0F, 0x80, 0x01, 0xFF, 0x00 }; /* each row one byte but DDB rows are WORD aligned */
    WORD w[8]; for (int i = 0; i < 8; i++) w[i] = d[i]; /* little endian: low byte first */
    return U(CreateBitmap)(8, 8, 1, 1, w);
}
TEST(dib_getdibits_mono_ddb_24) {
    NEED_BITS(); REQUIRE(U(CreateBitmap));
    HBITMAP hb = mono8(); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    BYTE buf[24 * 8]; BMI out; memset(buf, 0x77, sizeof buf);
    EXPECT_EQ(getbits(dc, hb, 24, 1, buf, &out, 0, 8), 8);
    static const BYTE rows[8] = { 0xAA, 0x55, 0xF0, 0x0F, 0x80, 0x01, 0xFF, 0x00 };
    int bad = 0;
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
        int bit = (rows[y] >> (7 - x)) & 1; BYTE e = bit ? 0xFF : 0;
        BYTE *p = buf + y * 24 + x * 3;
        if (p[0] != e || p[1] != e || p[2] != e) bad++;
    }
    CHECKF(bad == 0, "bad %d", bad);
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}
TEST(dib_getdibits_mono_ddb_1) {
    NEED_BITS(); REQUIRE(U(CreateBitmap));
    HBITMAP hb = mono8(); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    BYTE buf[4 * 8]; BMI out; memset(buf, 0, sizeof buf);
    EXPECT_EQ(getbits(dc, hb, 1, 1, buf, &out, 0, 8), 8);
    EXPECT_EQ(buf[0], 0xAA); EXPECT_EQ(buf[8], 0xF0); EXPECT_EQ(buf[24], 0xFF); EXPECT_EQ(buf[28], 0x00);
    DWORD c0 = *(DWORD *)&out.q[0] & 0xFFFFFF, c1 = *(DWORD *)&out.q[1] & 0xFFFFFF;
    EXPECT_EQ(c0, 0x000000); EXPECT_EQ(c1, 0xFFFFFF);
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}
TEST(dib_getdibits_mono_ddb_query) {
    NEED_BITS(); REQUIRE(U(CreateBitmap));
    HBITMAP hb = mono8(); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    BMI b; bmi_init(&b, 0, 0, 0, 0);
    CHECK(U(GetDIBits)(dc, hb, 0, 0, NULL, PB(&b), DIB_RGB_COLORS));
    EXPECT_EQ(b.h.biWidth, 8); EXPECT_EQ(abs(b.h.biHeight), 8); EXPECT_EQ(b.h.biBitCount, 1); EXPECT_EQ(b.h.biPlanes, 1);
    EXPECT_EQ(b.h.biSizeImage, 32);
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}
TEST(dib_getdibits_mono_ddb_8) {
    NEED_BITS(); REQUIRE(U(CreateBitmap));
    HBITMAP hb = mono8(); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    BYTE buf[8 * 8]; BMI out;
    EXPECT_EQ(getbits(dc, hb, 8, 1, buf, &out, 0, 8), 8);
    /* row 0 = 0xAA => 1,0,1,0.. ; resolve each through the returned table */
    int bad = 0;
    for (int x = 0; x < 8; x++) {
        int bit = (0xAA >> (7 - x)) & 1; RGBQUAD q = out.q[buf[x]];
        BYTE e = bit ? 0xFF : 0; if (q.rgbRed != e || q.rgbGreen != e || q.rgbBlue != e) bad++;
    }
    CHECKF(bad == 0, "bad %d", bad);
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}
TEST(dib_getdibits_mono_ddb_32_alpha) {
    NEED_BITS(); REQUIRE(U(CreateBitmap));
    HBITMAP hb = mono8(); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    DWORD buf[8 * 8]; BMI out; memset(buf, 0xEE, sizeof buf);
    EXPECT_EQ(getbits(dc, hb, 32, 1, (BYTE *)buf, &out, 0, 8), 8);
    EXPECT_EQ(buf[0] & 0xFFFFFF, 0xFFFFFF); EXPECT_EQ(buf[1] & 0xFFFFFF, 0);
    t_note("alpha byte of GetDIBits(32) = 0x%02lx", buf[0] >> 24);
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}

/* ---------- GetDIBits: invalid ---------- */
TEST(dib_getdibits_invalid_hdc) {
    NEED_BITS();
    HBITMAP hb = src32(); CHECK(hb); if (!hb) return;
    BMI b; bmi_init(&b, W, H, 24, BI_RGB); BYTE buf[W * 3 * H + 16];
    SetLastError(0);
    EXPECT_EQ(U(GetDIBits)(NULL, hb, 0, H, buf, PB(&b), DIB_RGB_COLORS), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0);
    EXPECT_EQ(U(GetDIBits)((HDC)(ULONG_PTR)0xDEAD, hb, 0, H, buf, PB(&b), DIB_RGB_COLORS), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0);
    HBRUSH br = U(CreateSolidBrush)(0);
    EXPECT_EQ(U(GetDIBits)((HDC)br, hb, 0, H, buf, PB(&b), DIB_RGB_COLORS), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    U(DeleteObject)(br);
    U(DeleteObject)(hb);
}
TEST(dib_getdibits_invalid_hdc_query) {
    NEED_BITS();
    HBITMAP hb = src32(); CHECK(hb); if (!hb) return;
    BMI b; bmi_init(&b, 0, 0, 0, 0);
    SetLastError(0);
    EXPECT_EQ(U(GetDIBits)(NULL, hb, 0, 0, NULL, PB(&b), DIB_RGB_COLORS), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    U(DeleteObject)(hb);
}
TEST(dib_getdibits_invalid_bitmap) {
    NEED_BITS();
    HDC dc = U(CreateCompatibleDC)(NULL); REQUIRE(dc);
    BMI b; bmi_init(&b, 4, 4, 24, BI_RGB); BYTE buf[4 * 12 + 16];
    EXPECT_EQ(U(GetDIBits)(dc, NULL, 0, 4, buf, PB(&b), DIB_RGB_COLORS), 0);
    EXPECT_EQ(U(GetDIBits)(dc, (HBITMAP)(ULONG_PTR)0xBEEF, 0, 4, buf, PB(&b), DIB_RGB_COLORS), 0);
    HBRUSH br = U(CreateSolidBrush)(0);
    EXPECT_EQ(U(GetDIBits)(dc, (HBITMAP)br, 0, 4, buf, PB(&b), DIB_RGB_COLORS), 0);
    EXPECT_EQ(U(GetDIBits)(dc, (HBITMAP)dc, 0, 4, buf, PB(&b), DIB_RGB_COLORS), 0);
    U(DeleteObject)(br); U(DeleteDC)(dc);
}
TEST(dib_getdibits_invalid_bitcount) {
    NEED_BITS();
    HBITMAP hb = src32(); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    static const int bad[] = { 2, 3, 5, 7, 12, 15, 33, 48, 64 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        BMI b; bmi_init(&b, W, H, bad[i], BI_RGB); BYTE buf[W * 8 * H];
        int r = U(GetDIBits)(dc, hb, 0, H, buf, PB(&b), DIB_RGB_COLORS);
        if (r != 0) t_note("bitcount %d -> %d", bad[i], r);
    }
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}
TEST(dib_getdibits_jpeg_png_with_bits) {
    NEED_BITS();
    HBITMAP hb = src32(); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    static const DWORD comp[] = { BI_JPEG, BI_PNG };
    static const DWORD sizes[] = { sizeof(BITMAPINFOHEADER), sizeof(BITMAPV4HEADER), sizeof(BITMAPV5HEADER) };
    for (int c = 0; c < 2; c++) for (int s = 0; s < 3; s++) {
        BYTE bb[sizeof(BITMAPV5HEADER) + 64]; memset(bb, 0, sizeof bb);
        BITMAPINFOHEADER *h = (BITMAPINFOHEADER *)bb;
        h->biSize = sizes[s]; h->biWidth = W; h->biHeight = H; h->biPlanes = 1; h->biBitCount = 24; h->biCompression = comp[c]; h->biSizeImage = 1024;
        BYTE buf[1024]; SetLastError(0);
        int r = U(GetDIBits)(dc, hb, 0, H, buf, (BITMAPINFO *)bb, DIB_RGB_COLORS);
        CHECKF(r == 0, "comp %lu size %lu -> %d", comp[c], sizes[s], r);
        if (!dib_is_wine()) EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    }
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}
TEST(dib_getdibits_rle_request) {
    NEED_BITS();
    HBITMAP hb = src32(); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    BMI b; bmi_init(&b, W, H, 8, BI_RLE8); b.h.biSizeImage = 1024; BYTE buf[1024];
    int r = U(GetDIBits)(dc, hb, 0, H, buf, PB(&b), DIB_RGB_COLORS);
    t_note("GetDIBits BI_RLE8 request -> %d", r);
    bmi_init(&b, W, H, 4, BI_RLE4); b.h.biSizeImage = 1024;
    r = U(GetDIBits)(dc, hb, 0, H, buf, PB(&b), DIB_RGB_COLORS);
    t_note("GetDIBits BI_RLE4 request -> %d", r);
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}
TEST(dib_getdibits_bad_usage) {
    NEED_BITS();
    HBITMAP hb = src32(); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    BMI b; bmi_init(&b, W, H, 24, BI_RGB); BYTE buf[W * 3 * H + 16];
    static const UINT u[] = { 2, 3, 0x100, 0xFFFFFFFF };
    for (int i = 0; i < 4; i++) {
        int r = U(GetDIBits)(dc, hb, 0, H, buf, PB(&b), u[i]);
        t_note("usage 0x%x -> %d", u[i], r);
    }
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}
TEST(dib_getdibits_selected_bitmap) {
    NEED_BITS();
    HBITMAP hb = src32(); CHECK(hb); if (!hb) return;
    HGDIOBJ old; HDC dc = mkdc(hb, &old);
    BMI b; bmi_init(&b, W, H, 24, BI_RGB); BYTE buf[W * 3 * H + 16];
    int r = U(GetDIBits)(dc, hb, 0, H, buf, PB(&b), DIB_RGB_COLORS);
    t_note("GetDIBits on selected bitmap -> %d", r);
    freedc(dc, old); U(DeleteObject)(hb);
}
TEST(dib_getdibits_other_dc_selected) {
    NEED_BITS();
    HBITMAP hb = src32(); CHECK(hb); if (!hb) return;
    HGDIOBJ old; HDC d1 = mkdc(hb, &old), d2 = U(CreateCompatibleDC)(NULL);
    BMI b; bmi_init(&b, W, H, 32, BI_RGB); DWORD buf[W * H];
    int r = U(GetDIBits)(d2, hb, 0, H, buf, PB(&b), DIB_RGB_COLORS);
    t_note("GetDIBits via other DC -> %d", r);
    if (r == H) EXPECT_EQ(buf[(H - 1) * W] & 0xFFFFFF, (pr(0, 0) << 16) | (pg(0, 0) << 8) | pb(0, 0));
    U(DeleteDC)(d2); freedc(d1, old); U(DeleteObject)(hb);
}
TEST(dib_getdibits_null_bits_zero_lines) {
    NEED_BITS();
    HBITMAP hb = src32(); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    BMI b; bmi_init(&b, 0, 0, 0, 0);
    EXPECT_NE(U(GetDIBits)(dc, hb, 0, 0, NULL, PB(&b), DIB_RGB_COLORS), 0);
    bmi_init(&b, 0, 0, 0, 0);
    int r = U(GetDIBits)(dc, hb, 0, 5, NULL, PB(&b), DIB_RGB_COLORS);
    t_note("bits NULL cLines 5 -> %d", r);
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}
TEST(dib_getdibits_repeated_stable) {
    NEED_BITS();
    HBITMAP hb = src32(); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    BYTE first[W * 4 * H + 8], cur[W * 4 * H + 8]; BMI o;
    EXPECT_EQ(getbits(dc, hb, 24, 0, first, &o, 0, H), H);
    for (int i = 0; i < 50; i++) {
        memset(cur, 0, sizeof cur);
        EXPECT_EQ(getbits(dc, hb, 24, 0, cur, &o, 0, H), H);
        if (memcmp(first, cur, ((W * 3 + 3) / 4 * 4) * H)) { CHECKF(0, "iteration %d differs", i); break; }
    }
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}
TEST(dib_getdibits_unaligned_buffer) {
    NEED_BITS();
    HBITMAP hb = src32(); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    BYTE raw[W * 3 * H + 16]; BMI o;
    for (int off = 0; off < 4; off++) {
        memset(raw, 0, sizeof raw);
        EXPECT_EQ(getbits(dc, hb, 24, 0, raw + off, &o, 0, H), H);
        int st = (W * 3 + 3) / 4 * 4;
        CHECKF(raw[off + (H - 1) * st + 2] == pr(0, 0) && raw[off + (H - 1) * st] == pb(0, 0), "offset %d", off);
    }
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}
TEST(dib_getdibits_from_memdc_dib_after_gdi_draw) {
    NEED_BITS();
    void *bits; HBITMAP hb = mkdib(8, 8, 32, &bits); CHECK(hb); if (!hb) return;
    HGDIOBJ old; HDC dc = mkdc(hb, &old);
    U(SetPixelV)(dc, 3, 2, RGB(10, 20, 30));
    U(GdiFlush)();
    /* detach so the bitmap is not selected */
    U(SelectObject)(dc, old);
    BMI b; bmi_init(&b, 8, -8, 24, BI_RGB); BYTE buf[8 * 3 * 8];
    EXPECT_EQ(U(GetDIBits)(dc, hb, 0, 8, buf, PB(&b), DIB_RGB_COLORS), 8);
    EXPECT_EQ(buf[2 * 24 + 9], 30); EXPECT_EQ(buf[2 * 24 + 10], 20); EXPECT_EQ(buf[2 * 24 + 11], 10);
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}

/* ---------- SetDIBits ---------- */
static void setbits_depth(int bpp) {
    NEED_BITS();
    int w = 7, h = 4;
    void *tb; HBITMAP tgt = mkdib(w, -h, 32, &tb); CHECK(tgt); if (!tgt) return;
    BMI b; bmi_init(&b, w, -h, bpp, BI_RGB);
    int n = pal_n(bpp); if (n) bmi_pal(&b, n);
    int st = dib_stride(w, bpp); BYTE src[256]; memset(src, 0, sizeof src);
    DWORD expect[7 * 4];
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        BYTE r = pr(x, y), g = pg(x, y), bl = pb(x, y); BYTE *row = src + y * st; int idx = (x * 5 + y) % (n ? n : 1);
        if (bpp == 1) { if (idx) row[x / 8] |= 0x80 >> (x & 7); }
        else if (bpp == 4) row[x / 2] |= (x & 1) ? idx : idx << 4;
        else if (bpp == 8) row[x] = idx;
        else if (bpp == 16) ((WORD *)row)[x] = ((r >> 3) << 10) | ((g >> 3) << 5) | (bl >> 3);
        else if (bpp == 24) { row[x*3] = bl; row[x*3+1] = g; row[x*3+2] = r; }
        else { ((DWORD *)row)[x] = (r << 16) | (g << 8) | bl; }
        if (n) expect[y * w + x] = (b.q[idx].rgbRed << 16) | (b.q[idx].rgbGreen << 8) | b.q[idx].rgbBlue;
        else if (bpp == 16) expect[y * w + x] = ((((r>>3)<<3)|((r>>3)>>2)) << 16) | ((((g>>3)<<3)|((g>>3)>>2)) << 8) | (((bl>>3)<<3)|((bl>>3)>>2));
        else expect[y * w + x] = (r << 16) | (g << 8) | bl;
    }
    int r = U(SetDIBits)(NULL, tgt, 0, h, src, PB(&b), DIB_RGB_COLORS);
    EXPECT_EQ(r, h);
    int bad = 0, bad16 = 0;
    for (int i = 0; i < w * h; i++) {
        DWORD got = ((DWORD *)tb)[i] & 0xFFFFFF;
        if (got != expect[i]) {
            if (bpp == 16) { /* expansion style may be replicate or zero-fill */ bad16++; } else bad++;
        }
    }
    CHECKF(bad == 0, "bad pixels %d", bad);
    if (bad16) t_note("16bpp expansion differs from bit-replication in %d pixels", bad16);
    U(DeleteObject)(tgt);
}
TEST(dib_setdibits_depth_1) { setbits_depth(1); }
TEST(dib_setdibits_depth_4) { setbits_depth(4); }
TEST(dib_setdibits_depth_8) { setbits_depth(8); }
TEST(dib_setdibits_depth_16) { setbits_depth(16); }
TEST(dib_setdibits_depth_24) { setbits_depth(24); }
TEST(dib_setdibits_depth_32) { setbits_depth(32); }

TEST(dib_setdibits_bottomup_source) {
    NEED_BITS();
    void *tb; HBITMAP tgt = mkdib(4, -4, 24, &tb); CHECK(tgt); if (!tgt) return;
    BMI b; bmi_init(&b, 4, 4, 24, BI_RGB); BYTE src[48];
    for (int r = 0; r < 4; r++) for (int i = 0; i < 12; i++) src[r * 12 + i] = 0x10 * (r + 1);
    EXPECT_EQ(U(SetDIBits)(NULL, tgt, 0, 4, src, PB(&b), DIB_RGB_COLORS), 4);
    /* data row 0 == bottom row: target memory (top-down) row 3 */
    EXPECT_EQ(((BYTE *)tb)[3 * 12], 0x10); EXPECT_EQ(((BYTE *)tb)[0], 0x40);
    U(DeleteObject)(tgt);
}
TEST(dib_setdibits_topdown_source) {
    NEED_BITS();
    void *tb; HBITMAP tgt = mkdib(4, -4, 24, &tb); CHECK(tgt); if (!tgt) return;
    BMI b; bmi_init(&b, 4, -4, 24, BI_RGB); BYTE src[48];
    for (int r = 0; r < 4; r++) for (int i = 0; i < 12; i++) src[r * 12 + i] = 0x10 * (r + 1);
    EXPECT_EQ(U(SetDIBits)(NULL, tgt, 0, 4, src, PB(&b), DIB_RGB_COLORS), 4);
    EXPECT_EQ(((BYTE *)tb)[0], 0x10); EXPECT_EQ(((BYTE *)tb)[3 * 12], 0x40);
    U(DeleteObject)(tgt);
}
TEST(dib_setdibits_partial_ranges) {
    NEED_BITS();
    for (int start = 0; start < 6; start++) for (int lines = 1; start + lines <= 6; lines++) {
        void *tb; HBITMAP tgt = mkdib(4, 6, 24, &tb); CHECK(tgt); if (!tgt) return;
        BMI b; bmi_init(&b, 4, 6, 24, BI_RGB); BYTE src[12 * 6];
        for (int r = 0; r < 6; r++) memset(src + r * 12, 0x20 + r, 12);
        int rv = U(SetDIBits)(NULL, tgt, start, lines, src, PB(&b), DIB_RGB_COLORS);
        CHECKF(rv == lines, "start %d lines %d -> %d", start, lines, rv);
        /* buffer holds `lines` scanlines starting at scanline `start` */
        int bad = 0;
        for (int y = 0; y < 6; y++) {
            BYTE e = (y >= start && y < start + lines) ? 0x20 + (y - start) : 0;
            for (int i = 0; i < 12; i++) if (((BYTE *)tb)[y * 12 + i] != e) bad++;
        }
        CHECKF(bad == 0, "start %d lines %d bad %d", start, lines, bad);
        U(DeleteObject)(tgt);
    }
}
TEST(dib_setdibits_out_of_range) {
    NEED_BITS();
    void *tb; HBITMAP tgt = mkdib(4, -4, 24, &tb); CHECK(tgt); if (!tgt) return;
    BMI b; bmi_init(&b, 4, -4, 24, BI_RGB); BYTE src[12 * 16]; memset(src, 0x55, sizeof src);
    int r = U(SetDIBits)(NULL, tgt, 4, 1, src, PB(&b), DIB_RGB_COLORS);
    t_note("start==h -> %d", r);
    r = U(SetDIBits)(NULL, tgt, 100, 1, src, PB(&b), DIB_RGB_COLORS);
    t_note("start==100 -> %d", r);
    r = U(SetDIBits)(NULL, tgt, 0, 0, src, PB(&b), DIB_RGB_COLORS);
    EXPECT_EQ(r, 0);
    r = U(SetDIBits)(NULL, tgt, 0, 8, src, PB(&b), DIB_RGB_COLORS);
    t_note("cLines 8 on 4 rows -> %d", r);
    r = U(SetDIBits)(NULL, tgt, 2, 8, src, PB(&b), DIB_RGB_COLORS);
    t_note("start 2 cLines 8 on 4 rows -> %d", r);
    U(DeleteObject)(tgt);
}
TEST(dib_setdibits_invalid_args) {
    NEED_BITS();
    void *tb; HBITMAP tgt = mkdib(4, 4, 24, &tb); CHECK(tgt); if (!tgt) return;
    BMI b; bmi_init(&b, 4, 4, 24, BI_RGB); BYTE src[48] = { 0 };
    if (!dib_is_wine()) EXPECT_EQ(U(SetDIBits)(NULL, tgt, 0, 4, NULL, PB(&b), DIB_RGB_COLORS), 0);
    EXPECT_EQ(U(SetDIBits)(NULL, NULL, 0, 4, src, PB(&b), DIB_RGB_COLORS), 0);
    EXPECT_EQ(U(SetDIBits)(NULL, (HBITMAP)(ULONG_PTR)0x4321, 0, 4, src, PB(&b), DIB_RGB_COLORS), 0);
    HBRUSH br = U(CreateSolidBrush)(0);
    EXPECT_EQ(U(SetDIBits)(NULL, (HBITMAP)br, 0, 4, src, PB(&b), DIB_RGB_COLORS), 0);
    U(DeleteObject)(br);
    HDC dc = U(CreateCompatibleDC)(NULL);
    EXPECT_EQ(U(SetDIBits)(NULL, (HBITMAP)dc, 0, 4, src, PB(&b), DIB_RGB_COLORS), 0);
    U(DeleteDC)(dc);
    U(DeleteObject)(tgt);
}
TEST(dib_setdibits_jpeg_png) {
    NEED_BITS();
    void *tb; HBITMAP tgt = mkdib(4, 4, 24, &tb); CHECK(tgt); if (!tgt) return;
    static const DWORD comp[] = { BI_JPEG, BI_PNG };
    static const DWORD sizes[] = { sizeof(BITMAPINFOHEADER), sizeof(BITMAPV4HEADER), sizeof(BITMAPV5HEADER) };
    for (int c = 0; c < 2; c++) for (int s = 0; s < 3; s++) {
        BYTE bb[sizeof(BITMAPV5HEADER) + 64]; memset(bb, 0, sizeof bb);
        BITMAPINFOHEADER *h = (BITMAPINFOHEADER *)bb;
        h->biSize = sizes[s]; h->biWidth = 4; h->biHeight = 4; h->biPlanes = 1; h->biBitCount = 24; h->biCompression = comp[c]; h->biSizeImage = 48;
        BYTE src[64] = { 0 }; SetLastError(0);
        int r = U(SetDIBits)(NULL, tgt, 0, 4, src, (BITMAPINFO *)bb, DIB_RGB_COLORS);
        CHECKF(r == 0, "comp %lu size %lu -> %d", comp[c], sizes[s], r);
        if (!dib_is_wine()) EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    }
    U(DeleteObject)(tgt);
}
TEST(dib_setdibits_selected_target) {
    NEED_BITS();
    void *tb; HBITMAP tgt = mkdib(4, -4, 32, &tb); CHECK(tgt); if (!tgt) return;
    HGDIOBJ old; HDC dc = mkdc(tgt, &old);
    BMI b; bmi_init(&b, 4, -4, 32, BI_RGB); DWORD src[16]; for (int i = 0; i < 16; i++) src[i] = 0x00112233 + i;
    EXPECT_EQ(U(SetDIBits)(dc, tgt, 0, 4, src, PB(&b), DIB_RGB_COLORS), 4);
    U(GdiFlush)();
    EXPECT_EQ(((DWORD *)tb)[5] & 0xFFFFFF, 0x112238);
    EXPECT_EQ(U(GetPixel)(dc, 1, 1), RGB(0x11, 0x22, 0x38));
    freedc(dc, old); U(DeleteObject)(tgt);
}
TEST(dib_setdibits_to_ddb_then_getdibits) {
    NEED_BITS(); REQUIRE(U(CreateBitmap));
    HBITMAP ddb = U(CreateBitmap)(8, 8, 1, 1, NULL); CHECK(ddb); if (!ddb) return;
    BMI b; bmi_init(&b, 8, -8, 1, BI_RGB);
    b.q[0].rgbBlue = b.q[0].rgbGreen = b.q[0].rgbRed = 0; b.q[1].rgbBlue = b.q[1].rgbGreen = b.q[1].rgbRed = 255;
    BYTE src[32]; for (int y = 0; y < 8; y++) { memset(src + y * 4, 0, 4); src[y * 4] = (BYTE)(0x80 >> y); }
    HDC dc = U(CreateCompatibleDC)(NULL);
    EXPECT_EQ(U(SetDIBits)(dc, ddb, 0, 8, src, PB(&b), DIB_RGB_COLORS), 8);
    BYTE out[32]; BMI ob; memset(out, 0, sizeof out);
    EXPECT_EQ(getbits(dc, ddb, 1, 1, out, &ob, 0, 8), 8);
    int bad = 0; for (int y = 0; y < 8; y++) if (out[y * 4] != (BYTE)(0x80 >> y)) bad++;
    CHECKF(bad == 0, "bad rows %d", bad);
    U(DeleteDC)(dc); U(DeleteObject)(ddb);
}
TEST(dib_setdibits_rle8_to_dib) {
    NEED_BITS();
    void *tb; HBITMAP tgt = mkdib(4, -2, 8, &tb); CHECK(tgt); if (!tgt) return;
    BMI b; bmi_init(&b, 4, -2, 8, BI_RLE8); bmi_pal(&b, 256);
    /* row0: 4 x idx 7 ; EOL ; row1: 4 x idx 9 ; EOB */
    BYTE rle[] = { 4, 7, 0, 0, 4, 9, 0, 1 }; b.h.biSizeImage = sizeof rle;
    int r = U(SetDIBits)(NULL, tgt, 0, 2, rle, PB(&b), DIB_RGB_COLORS);
    t_note("SetDIBits RLE8 -> %d", r);
    if (r == 2) { EXPECT_EQ(((BYTE *)tb)[0], 7); EXPECT_EQ(((BYTE *)tb)[4], 9); }
    U(DeleteObject)(tgt);
}
TEST(dib_setdibits_pal_colors) {
    NEED_BITS(); REQUIRE(U(CreatePalette)); REQUIRE(U(SelectPalette)); REQUIRE(U(RealizePalette));
    if (dib_is_wine()) { t_skip("wine: palette semantics"); return; }
    void *tb; HBITMAP tgt = mkdib(4, -1, 32, &tb); CHECK(tgt); if (!tgt) return;
    BYTE lp[sizeof(LOGPALETTE) + 3 * sizeof(PALETTEENTRY)] = { 0 };
    LOGPALETTE *l = (LOGPALETTE *)lp; l->palVersion = 0x300; l->palNumEntries = 4;
    l->palPalEntry[0] = (PALETTEENTRY){ 10, 20, 30, 0 }; l->palPalEntry[1] = (PALETTEENTRY){ 40, 50, 60, 0 };
    l->palPalEntry[2] = (PALETTEENTRY){ 70, 80, 90, 0 }; l->palPalEntry[3] = (PALETTEENTRY){ 100, 110, 120, 0 };
    HPALETTE pal = U(CreatePalette)(l); CHECK(pal);
    HDC dc = U(CreateCompatibleDC)(NULL);
    HPALETTE op = U(SelectPalette)(dc, pal, FALSE);
    BMI b; bmi_init(&b, 4, -1, 8, BI_RGB); for (int i = 0; i < 4; i++) b.idx[i] = (WORD)(3 - i);
    BYTE idx[4] = { 0, 1, 2, 3 };
    int r = U(SetDIBits)(dc, tgt, 0, 1, idx, PB(&b), DIB_PAL_COLORS);
    t_note("SetDIBits PAL_COLORS -> %d pix0=%06lx", r, ((DWORD *)tb)[0] & 0xFFFFFF);
    U(SelectPalette)(dc, op, FALSE); U(DeleteDC)(dc); U(DeleteObject)(pal); U(DeleteObject)(tgt);
}
TEST(dib_setdibits_repeat_overwrites) {
    NEED_BITS();
    void *tb; HBITMAP tgt = mkdib(2, -2, 24, &tb); CHECK(tgt); if (!tgt) return;
    BMI b; bmi_init(&b, 2, -2, 24, BI_RGB); BYTE src[16];
    for (int k = 1; k <= 20; k++) {
        memset(src, k, sizeof src);
        EXPECT_EQ(U(SetDIBits)(NULL, tgt, 0, 2, src, PB(&b), DIB_RGB_COLORS), 2);
        if (((BYTE *)tb)[0] != k || ((BYTE *)tb)[8 + 5] != k) { CHECKF(0, "iteration %d", k); break; }
    }
    U(DeleteObject)(tgt);
}
TEST(dib_setdibits_header_larger_than_bitmap) {
    NEED_BITS();
    void *tb; HBITMAP tgt = mkdib(4, -4, 24, &tb); CHECK(tgt); if (!tgt) return;
    BMI b; bmi_init(&b, 8, -8, 24, BI_RGB); BYTE src[24 * 8]; memset(src, 0x66, sizeof src);
    int r = U(SetDIBits)(NULL, tgt, 0, 8, src, PB(&b), DIB_RGB_COLORS);
    t_note("src 8x8 into 4x4 bitmap -> %d, first byte 0x%02x", r, ((BYTE *)tb)[0]);
    U(DeleteObject)(tgt);
}

/* ---------- CreateDIBitmap ---------- */
#define NEED_CDB() do { NEED_BITS(); REQUIRE(U(CreateDIBitmap)); } while (0)
TEST(dib_createdibitmap_init_24) {
    NEED_CDB();
    void *tb; HBITMAP t = mkdib(2, 2, 32, &tb); HGDIOBJ old; HDC dc = mkdc(t, &old); CHECK(dc);
    BMI b; bmi_init(&b, W, -H, 24, BI_RGB); int st = (W * 3 + 3) / 4 * 4; BYTE src[(W * 3 + 3) / 4 * 4 * H];
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { BYTE *p = src + y * st + x * 3; p[0] = pb(x, y); p[1] = pg(x, y); p[2] = pr(x, y); }
    HBITMAP hb = U(CreateDIBitmap)(dc, &b.h, CBM_INIT, src, PB(&b), DIB_RGB_COLORS);
    CHECK(hb != NULL);
    if (hb) {
        BITMAP bm; U(GetObjectW)(hb, sizeof bm, &bm);
        EXPECT_EQ(bm.bmWidth, W); EXPECT_EQ(bm.bmHeight, H);
    t_note("DDB from DIB-selected DC bmBits=%p", bm.bmBits);
        t_note("DDB from 32bpp-DIB-selected DC: bpp=%d planes=%d", bm.bmBitsPixel, bm.bmPlanes);
        U(SelectObject)(dc, old); old = NULL;
        BYTE out[W * 3 * H + 32]; BMI ob; memset(out, 0, sizeof out);
        EXPECT_EQ(getbits(dc, hb, 24, 1, out, &ob, 0, H), H);
        int bad = 0;
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { BYTE *p = out + y * st + x * 3; if (p[0] != pb(x, y) || p[1] != pg(x, y) || p[2] != pr(x, y)) bad++; }
        CHECKF(bad == 0, "bad %d", bad);
        U(DeleteObject)(hb);
    }
    freedc(dc, old); U(DeleteObject)(t);
}
TEST(dib_createdibitmap_noinit) {
    NEED_CDB();
    void *tb; HBITMAP t = mkdib(2, 2, 32, &tb); HGDIOBJ old; HDC dc = mkdc(t, &old);
    BMI b; bmi_init(&b, 13, 7, 24, BI_RGB);
    HBITMAP hb = U(CreateDIBitmap)(dc, &b.h, 0, NULL, NULL, DIB_RGB_COLORS);
    CHECK(hb != NULL);
    if (hb) { BITMAP bm; U(GetObjectW)(hb, sizeof bm, &bm); EXPECT_EQ(bm.bmWidth, 13); EXPECT_EQ(bm.bmHeight, 7); U(DeleteObject)(hb); }
    b.h.biHeight = -7;
    hb = U(CreateDIBitmap)(dc, &b.h, 0, NULL, NULL, DIB_RGB_COLORS);
    CHECK(hb != NULL);
    if (hb) { BITMAP bm; U(GetObjectW)(hb, sizeof bm, &bm); EXPECT_EQ(bm.bmWidth, 13); EXPECT_EQ(abs(bm.bmHeight), 7); U(DeleteObject)(hb); }
    freedc(dc, old); U(DeleteObject)(t);
}
TEST(dib_createdibitmap_mono_dc) {
    NEED_CDB();
    HDC dc = U(CreateCompatibleDC)(NULL); REQUIRE(dc);
    BMI b; bmi_init(&b, 8, 8, 24, BI_RGB);
    HBITMAP hb = U(CreateDIBitmap)(dc, &b.h, 0, NULL, NULL, DIB_RGB_COLORS);
    CHECK(hb != NULL);
    if (hb) { BITMAP bm; U(GetObjectW)(hb, sizeof bm, &bm); EXPECT_EQ(bm.bmBitsPixel, 1); EXPECT_EQ(bm.bmPlanes, 1); U(DeleteObject)(hb); }
    U(DeleteDC)(dc);
}
TEST(dib_createdibitmap_null_header) {
    NEED_CDB();
    HDC dc = U(CreateCompatibleDC)(NULL); REQUIRE(dc);
    SetLastError(0);
    HBITMAP hb = U(CreateDIBitmap)(dc, NULL, 0, NULL, NULL, DIB_RGB_COLORS);
    t_note("NULL header -> %p", hb);
    if (hb) { BITMAP bm; U(GetObjectW)(hb, sizeof bm, &bm); t_note(" %ldx%ld bpp=%d", (long)bm.bmWidth, (long)bm.bmHeight, bm.bmBitsPixel); U(DeleteObject)(hb); }
    U(DeleteDC)(dc);
}
TEST(dib_createdibitmap_zero_size) {
    NEED_CDB();
    HDC dc = U(CreateCompatibleDC)(NULL); REQUIRE(dc);
    BMI b; bmi_init(&b, 0, 0, 24, BI_RGB);
    HBITMAP hb = U(CreateDIBitmap)(dc, &b.h, 0, NULL, NULL, DIB_RGB_COLORS);
    t_note("0x0 header -> %p", hb);
    if (hb) { BITMAP bm; U(GetObjectW)(hb, sizeof bm, &bm); t_note(" %ldx%ld bpp=%d", (long)bm.bmWidth, (long)bm.bmHeight, bm.bmBitsPixel); U(DeleteObject)(hb); }
    U(DeleteDC)(dc);
}
TEST(dib_createdibitmap_invalid_dc) {
    NEED_CDB();
    BMI b; bmi_init(&b, 4, 4, 24, BI_RGB);
    HBITMAP hb = U(CreateDIBitmap)(NULL, &b.h, 0, NULL, NULL, DIB_RGB_COLORS);
    t_note("NULL hdc -> %p", hb); if (hb) U(DeleteObject)(hb);
    SetLastError(0);
    hb = U(CreateDIBitmap)((HDC)(ULONG_PTR)0x777, &b.h, 0, NULL, NULL, DIB_RGB_COLORS);
    t_note("bogus hdc -> %p le=%lu", hb, GetLastError());
    CHECK(hb == NULL); if (hb) U(DeleteObject)(hb);
}
TEST(dib_createdibitmap_init_without_bits) {
    NEED_CDB();
    void *tb; HBITMAP t = mkdib(2, 2, 32, &tb); HGDIOBJ old; HDC dc = mkdc(t, &old);
    BMI b; bmi_init(&b, 4, 4, 24, BI_RGB);
    HBITMAP hb = U(CreateDIBitmap)(dc, &b.h, CBM_INIT, NULL, PB(&b), DIB_RGB_COLORS);
    t_note("CBM_INIT without bits -> %p", hb); if (hb) U(DeleteObject)(hb);
    hb = U(CreateDIBitmap)(dc, &b.h, CBM_INIT, (void *)tb, NULL, DIB_RGB_COLORS);
    t_note("CBM_INIT without bmi -> %p", hb); if (hb) U(DeleteObject)(hb);
    freedc(dc, old); U(DeleteObject)(t);
}
TEST(dib_createdibitmap_huge) {
    NEED_CDB();
    void *tb; HBITMAP t = mkdib(2, 2, 32, &tb); HGDIOBJ old; HDC dc = mkdc(t, &old);
    BMI b; bmi_init(&b, 100000, 100000, 32, BI_RGB);
    HBITMAP hb = U(CreateDIBitmap)(dc, &b.h, 0, NULL, NULL, DIB_RGB_COLORS);
    CHECK(hb == NULL); if (hb) U(DeleteObject)(hb);
    freedc(dc, old); U(DeleteObject)(t);
}
TEST(dib_createdibitmap_8bpp_init_with_table) {
    NEED_CDB();
    void *tb; HBITMAP t = mkdib(2, 2, 32, &tb); HGDIOBJ old; HDC dc = mkdc(t, &old);
    BMI b; bmi_init(&b, 4, -2, 8, BI_RGB); bmi_pal(&b, 256);
    BYTE src[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    HBITMAP hb = U(CreateDIBitmap)(dc, &b.h, CBM_INIT, src, PB(&b), DIB_RGB_COLORS);
    CHECK(hb != NULL);
    if (hb) {
        U(SelectObject)(dc, old); old = NULL;
        BYTE out[4 * 3 * 2]; BMI ob; EXPECT_EQ(getbits(dc, hb, 24, 1, out, &ob, 0, 2), 2);
        for (int i = 0; i < 4; i++) { /* pixel i of row0 has index i+1 */
            int idx = i + 1;
            CHECKF(out[i * 3] == idx && out[i * 3 + 1] == 255 - idx && out[i * 3 + 2] == (idx ^ 0x55), "px %d", i);
        }
        U(DeleteObject)(hb);
    }
    freedc(dc, old); U(DeleteObject)(t);
}
TEST(dib_createdibitmap_usage_pal) {
    NEED_CDB();
    void *tb; HBITMAP t = mkdib(2, 2, 32, &tb); HGDIOBJ old; HDC dc = mkdc(t, &old);
    BMI b; bmi_init(&b, 4, 2, 8, BI_RGB); for (int i = 0; i < 256; i++) b.idx[i] = (WORD)i;
    BYTE src[8] = { 0 };
    HBITMAP hb = U(CreateDIBitmap)(dc, &b.h, CBM_INIT, src, PB(&b), DIB_PAL_COLORS);
    t_note("CBM_INIT + DIB_PAL_COLORS -> %p", hb); if (hb) U(DeleteObject)(hb);
    hb = U(CreateDIBitmap)(dc, &b.h, CBM_INIT, src, PB(&b), 7);
    t_note("CBM_INIT + usage 7 -> %p", hb); if (hb) U(DeleteObject)(hb);
    freedc(dc, old); U(DeleteObject)(t);
}
TEST(dib_createdibitmap_cbm_createdib_flag) {
    NEED_CDB();
    void *tb; HBITMAP t = mkdib(2, 2, 32, &tb); HGDIOBJ old; HDC dc = mkdc(t, &old);
    BMI b; bmi_init(&b, 4, 4, 24, BI_RGB);
    HBITMAP hb = U(CreateDIBitmap)(dc, &b.h, 0x2 /* CBM_CREATEDIB */, NULL, PB(&b), DIB_RGB_COLORS);
    t_note("CBM_CREATEDIB -> %p", hb);
    if (hb) { DIBSECTION ds; int n = U(GetObjectW)(hb, sizeof ds, &ds); t_note(" GetObject=%d", n); U(DeleteObject)(hb); }
    freedc(dc, old); U(DeleteObject)(t);
}
TEST(dib_createdibitmap_gdi_balance) {
    NEED_CDB(); REQUIRE(U(GetGuiResources));
    if (dib_is_wine()) { t_skip("wine: no GDI count"); return; }
    void *tb; HBITMAP t = mkdib(2, 2, 32, &tb); HGDIOBJ old; HDC dc = mkdc(t, &old);
    BMI b; bmi_init(&b, 16, 16, 24, BI_RGB);
    int before = gdicount();
    for (int i = 0; i < 200; i++) { HBITMAP hb = U(CreateDIBitmap)(dc, &b.h, 0, NULL, NULL, DIB_RGB_COLORS); if (hb) U(DeleteObject)(hb); }
    EXPECT_EQ(gdicount(), before);
    freedc(dc, old); U(DeleteObject)(t);
}
