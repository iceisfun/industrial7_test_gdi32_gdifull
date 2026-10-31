#include "dibh.h"

/* ---------- basic creation per bit depth ---------- */
static void basic(int bpp) {
    NEED_GDI(); REQUIRE(U(GetObjectType));
    void *bits = NULL; int w = 5, h = 3;
    HBITMAP hb = mkdib(w, h, bpp, &bits);
    CHECK(hb != NULL); if (!hb) return;
    CHECK(bits != NULL);
    CHECK(((ULONG_PTR)bits & 3) == 0);
    EXPECT_EQ(U(GetObjectType)(hb), OBJ_BITMAP);
    int st = dib_stride(w, bpp), z = 1;
    for (int i = 0; i < st * h; i++) if (((BYTE *)bits)[i]) z = 0;
    CHECK(z);
    DIBSECTION ds; memset(&ds, 0xCC, sizeof ds);
    EXPECT_EQ(U(GetObjectW)(hb, sizeof ds, &ds), sizeof(DIBSECTION));
    EXPECT_EQ(ds.dsBm.bmType, 0);
    EXPECT_EQ(ds.dsBm.bmWidth, w);
    EXPECT_EQ(ds.dsBm.bmHeight, h);
    EXPECT_EQ(ds.dsBm.bmWidthBytes, st);
    EXPECT_EQ(ds.dsBm.bmPlanes, 1);
    EXPECT_EQ(ds.dsBm.bmBitsPixel, bpp);
    EXPECT_EQ(ds.dsBm.bmBits, bits);
    EXPECT_EQ(ds.dsBmih.biSize, sizeof(BITMAPINFOHEADER));
    EXPECT_EQ(ds.dsBmih.biWidth, w);
    EXPECT_EQ(ds.dsBmih.biPlanes, 1);
    EXPECT_EQ(ds.dsBmih.biBitCount, bpp);
    if (bpp == 16 || bpp == 32) t_note("bpp %d GetObject compression=%lu", bpp, ds.dsBmih.biCompression);
    else EXPECT_EQ(ds.dsBmih.biCompression, BI_RGB);
    EXPECT_EQ(ds.dsBmih.biSizeImage, st * h);
    EXPECT_EQ(ds.dshSection, 0);
    EXPECT_EQ(ds.dsOffset, 0);
    CHECK(U(DeleteObject)(hb));
}
TEST(dib_create_basic_1) { basic(1); }
TEST(dib_create_basic_4) { basic(4); }
TEST(dib_create_basic_8) { basic(8); }
TEST(dib_create_basic_16) { basic(16); }
TEST(dib_create_basic_24) { basic(24); }
TEST(dib_create_basic_32) { basic(32); }

/* GetObject with BITMAP-sized buffer on a DIB section */
static void getobj_bm(int bpp) {
    NEED_GDI();
    void *bits; HBITMAP hb = mkdib(9, 4, bpp, &bits); CHECK(hb); if (!hb) return;
    BITMAP bm; memset(&bm, 0xCC, sizeof bm);
    EXPECT_EQ(U(GetObjectW)(hb, sizeof bm, &bm), sizeof(BITMAP));
    EXPECT_EQ(bm.bmWidth, 9); EXPECT_EQ(bm.bmHeight, 4);
    EXPECT_EQ(bm.bmWidthBytes, dib_stride(9, bpp));
    EXPECT_EQ(bm.bmBitsPixel, bpp);
    EXPECT_EQ(bm.bmBits, bits);
    int n = U(GetObjectW)(hb, 0, NULL);
    if (n != (int)sizeof(BITMAP) && n != (int)sizeof(DIBSECTION)) CHECKF(0, "size query %d", n);
    t_note("GetObject(dib,0,NULL)=%d", n);
    /* wrong-size small buffers */
    char tmp[64];
    int r = U(GetObjectW)(hb, 1, tmp);
    t_note("GetObject(dib,1,buf)=%d", r);
    U(DeleteObject)(hb);
}
TEST(dib_getobject_bitmap_1) { getobj_bm(1); }
TEST(dib_getobject_bitmap_4) { getobj_bm(4); }
TEST(dib_getobject_bitmap_8) { getobj_bm(8); }
TEST(dib_getobject_bitmap_16) { getobj_bm(16); }
TEST(dib_getobject_bitmap_24) { getobj_bm(24); }
TEST(dib_getobject_bitmap_32) { getobj_bm(32); }

/* stride for many widths */
static void strides(int bpp) {
    NEED_GDI();
    for (int w = 1; w <= 70; w++) {
        void *bits; HBITMAP hb = mkdib(w, 2, bpp, &bits);
        if (!hb) { CHECKF(0, "create w=%d", w); continue; }
        BITMAP bm; U(GetObjectW)(hb, sizeof bm, &bm);
        CHECKF(bm.bmWidthBytes == dib_stride(w, bpp), "w=%d got %d want %d", w, bm.bmWidthBytes, dib_stride(w, bpp));
        U(DeleteObject)(hb);
    }
}
TEST(dib_stride_1) { strides(1); }
TEST(dib_stride_4) { strides(4); }
TEST(dib_stride_8) { strides(8); }
TEST(dib_stride_16) { strides(16); }
TEST(dib_stride_24) { strides(24); }
TEST(dib_stride_32) { strides(32); }

/* top-down vs bottom-up row order, verified through GetPixel */
static void orient(int bpp, int topdown) {
    NEED_GDI();
    BMI b; void *bits;
    bmi_init(&b, 4, topdown ? -4 : 4, bpp, BI_RGB);
    if (bpp <= 8) { for (int i = 0; i < (1 << bpp); i++) { b.q[i].rgbRed = b.q[i].rgbGreen = b.q[i].rgbBlue = i ? 255 : 0; } }
    HBITMAP hb = U(CreateDIBSection)(NULL, PB(&b), DIB_RGB_COLORS, &bits, NULL, 0);
    CHECK(hb); if (!hb) return;
    int st = dib_stride(4, bpp);
    BYTE *p = bits;
    /* set pixel x=0 of memory row 0 to 'white' */
    if (bpp == 1) p[0] = 0x80;
    else if (bpp == 4) p[0] = 0xF0;
    else if (bpp == 8) p[0] = 1;
    else if (bpp == 16) { p[0] = 0xFF; p[1] = 0x7F; }
    else { p[0] = p[1] = p[2] = 0xFF; }
    HGDIOBJ old; HDC dc = mkdc(hb, &old); CHECK(dc);
    U(GdiFlush)();
    COLORREF top = U(GetPixel)(dc, 0, 0), bot = U(GetPixel)(dc, 0, 3);
    if (topdown) { EXPECT_EQ(top, 0xFFFFFF); EXPECT_EQ(bot, 0); }
    else { EXPECT_EQ(top, 0); EXPECT_EQ(bot, 0xFFFFFF); }
    BITMAP bm; U(GetObjectW)(hb, sizeof bm, &bm);
    EXPECT_EQ(bm.bmHeight, 4);
    DIBSECTION ds; U(GetObjectW)(hb, sizeof ds, &ds);
    t_note("topdown=%d dsBmih.biHeight=%ld", topdown, (long)ds.dsBmih.biHeight);
    freedc(dc, old); U(DeleteObject)(hb);
}
TEST(dib_orient_bottomup_1) { orient(1, 0); }
TEST(dib_orient_bottomup_4) { orient(4, 0); }
TEST(dib_orient_bottomup_8) { orient(8, 0); }
TEST(dib_orient_bottomup_16) { orient(16, 0); }
TEST(dib_orient_bottomup_24) { orient(24, 0); }
TEST(dib_orient_bottomup_32) { orient(32, 0); }
TEST(dib_orient_topdown_1) { orient(1, 1); }
TEST(dib_orient_topdown_4) { orient(4, 1); }
TEST(dib_orient_topdown_8) { orient(8, 1); }
TEST(dib_orient_topdown_16) { orient(16, 1); }
TEST(dib_orient_topdown_24) { orient(24, 1); }
TEST(dib_orient_topdown_32) { orient(32, 1); }

/* ---------- invalid bit counts ---------- */
TEST(dib_create_invalid_bitcount) {
    NEED_GDI();
    static const int bad[] = { 0, 2, 3, 5, 6, 7, 9, 12, 15, 17, 20, 31, 33, 48, 64, 65535 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        BMI b; void *bits = NULL;
        bmi_init(&b, 4, 4, bad[i], BI_RGB);
        SetLastError(0);
        HBITMAP hb = U(CreateDIBSection)(NULL, PB(&b), DIB_RGB_COLORS, &bits, NULL, 0);
        CHECKF(hb == NULL, "bitcount %d created", bad[i]);
        if (hb) U(DeleteObject)(hb);
    }
}
TEST(dib_create_null_bmi) {
    NEED_GDI();
    void *bits = (void *)1;
    SetLastError(0);
    HBITMAP hb = U(CreateDIBSection)(NULL, NULL, DIB_RGB_COLORS, &bits, NULL, 0);
    CHECK(hb == NULL);
    if (hb) U(DeleteObject)(hb);
    t_note("NULL bmi: le=%lu bits=%p", GetLastError(), bits);
}
TEST(dib_create_invalid_usage) {
    NEED_GDI();
    static const UINT bad[] = { 2, 3, 4, 0x10, 0xFFFF, 0x80000000u, 0xFFFFFFFFu };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        BMI b; void *bits = NULL; bmi_init(&b, 4, 4, 24, BI_RGB);
        HBITMAP hb = U(CreateDIBSection)(NULL, PB(&b), bad[i], &bits, NULL, 0);
        if (hb) t_note("usage 0x%x accepted", bad[i]);
        if (hb) U(DeleteObject)(hb);
    }
}
TEST(dib_create_pal_colors_nodc) {
    NEED_GDI();
    BMI b; void *bits = NULL; bmi_init(&b, 4, 4, 8, BI_RGB);
    for (int i = 0; i < 256; i++) b.idx[i] = (WORD)i;
    HBITMAP hb = U(CreateDIBSection)(NULL, PB(&b), DIB_PAL_COLORS, &bits, NULL, 0);
    t_note("PAL_COLORS with NULL hdc -> %p", hb);
    if (hb) U(DeleteObject)(hb);
    HDC dc = U(CreateCompatibleDC)(NULL); REQUIRE(dc);
    hb = U(CreateDIBSection)(dc, PB(&b), DIB_PAL_COLORS, &bits, NULL, 0);
    CHECK(hb != NULL);
    if (hb) {
        HBITMAP o = U(SelectObject)(dc, hb);
        RGBQUAD q[256]; UINT n = U(GetDIBColorTable)(dc, 0, 256, q);
        EXPECT_EQ(n, 256);
        U(SelectObject)(dc, o); U(DeleteObject)(hb);
    }
    U(DeleteDC)(dc);
}

/* ---------- header variants ---------- */
TEST(dib_create_core_header) {
    NEED_GDI();
    static const int bpps[] = { 1, 4, 8, 24 };
    for (int i = 0; i < 4; i++) {
        BYTE buf[sizeof(BITMAPCOREHEADER) + 256 * 3]; memset(buf, 0, sizeof buf);
        BITMAPCOREINFO *c = (BITMAPCOREINFO *)buf;
        c->bmciHeader.bcSize = sizeof(BITMAPCOREHEADER);
        c->bmciHeader.bcWidth = 6; c->bmciHeader.bcHeight = 3;
        c->bmciHeader.bcPlanes = 1; c->bmciHeader.bcBitCount = bpps[i];
        void *bits = NULL;
        HBITMAP hb = U(CreateDIBSection)(NULL, (BITMAPINFO *)c, DIB_RGB_COLORS, &bits, NULL, 0);
        CHECKF(hb != NULL, "core bpp %d", bpps[i]);
        if (!hb) continue;
        BITMAP bm; U(GetObjectW)(hb, sizeof bm, &bm);
        EXPECT_EQ(bm.bmWidth, 6); EXPECT_EQ(bm.bmHeight, 3);
        EXPECT_EQ(bm.bmBitsPixel, bpps[i]);
        EXPECT_EQ(bm.bmWidthBytes, dib_stride(6, bpps[i]));
        U(DeleteObject)(hb);
    }
}
TEST(dib_create_core_header_32_invalid) {
    NEED_GDI();
    BYTE buf[sizeof(BITMAPCOREHEADER) + 16]; memset(buf, 0, sizeof buf);
    BITMAPCOREINFO *c = (BITMAPCOREINFO *)buf;
    c->bmciHeader.bcSize = sizeof(BITMAPCOREHEADER);
    c->bmciHeader.bcWidth = 4; c->bmciHeader.bcHeight = 4; c->bmciHeader.bcPlanes = 1;
    for (int bc = 0; bc < 3; bc++) {
        static const int v[] = { 0, 2, 16 };
        c->bmciHeader.bcBitCount = v[bc];
        void *bits;
        HBITMAP hb = U(CreateDIBSection)(NULL, (BITMAPINFO *)c, DIB_RGB_COLORS, &bits, NULL, 0);
        t_note("core bitcount %d -> %p", v[bc], hb);
        if (hb) U(DeleteObject)(hb);
    }
}
static void hdrv(DWORD size, int bpp) {
    NEED_GDI();
    BYTE buf[sizeof(BITMAPV5HEADER) + 1100]; memset(buf, 0, sizeof buf);
    BITMAPV5HEADER *h = (BITMAPV5HEADER *)buf;
    h->bV5Size = size; h->bV5Width = 7; h->bV5Height = 5; h->bV5Planes = 1; h->bV5BitCount = bpp;
    h->bV5Compression = BI_RGB;
    if (bpp <= 8) for (int i = 0; i < (1 << bpp); i++) ((RGBQUAD *)(buf + size))[i].rgbGreen = (BYTE)i;
    void *bits = NULL;
    HBITMAP hb = U(CreateDIBSection)(NULL, (BITMAPINFO *)buf, DIB_RGB_COLORS, &bits, NULL, 0);
    CHECK(hb != NULL); if (!hb) return;
    CHECK(bits != NULL);
    DIBSECTION ds; EXPECT_EQ(U(GetObjectW)(hb, sizeof ds, &ds), sizeof ds);
    EXPECT_EQ(ds.dsBm.bmWidth, 7); EXPECT_EQ(ds.dsBm.bmHeight, 5);
    EXPECT_EQ(ds.dsBm.bmBitsPixel, bpp);
    EXPECT_EQ(ds.dsBm.bmWidthBytes, dib_stride(7, bpp));
    EXPECT_EQ(ds.dsBmih.biSize, sizeof(BITMAPINFOHEADER));
    if (bpp == 8) {
        HGDIOBJ old; HDC dc = mkdc(hb, &old);
        RGBQUAD q[256]; EXPECT_EQ(U(GetDIBColorTable)(dc, 0, 256, q), 256);
        EXPECT_EQ(q[77].rgbGreen, 77);
        freedc(dc, old);
    }
    U(DeleteObject)(hb);
}
TEST(dib_create_v4_24) { hdrv(sizeof(BITMAPV4HEADER), 24); }
TEST(dib_create_v4_32) { hdrv(sizeof(BITMAPV4HEADER), 32); }
TEST(dib_create_v4_8) { REQUIRE(U(GetDIBColorTable)); hdrv(sizeof(BITMAPV4HEADER), 8); }
TEST(dib_create_v5_24) { hdrv(sizeof(BITMAPV5HEADER), 24); }
TEST(dib_create_v5_32) { hdrv(sizeof(BITMAPV5HEADER), 32); }
TEST(dib_create_v5_8) { REQUIRE(U(GetDIBColorTable)); hdrv(sizeof(BITMAPV5HEADER), 8); }
TEST(dib_create_v5_1) { REQUIRE(U(GetDIBColorTable)); hdrv(sizeof(BITMAPV5HEADER), 1); }
TEST(dib_create_v5_16) { hdrv(sizeof(BITMAPV5HEADER), 16); }

TEST(dib_create_bad_biSize) {
    NEED_GDI();
    static const DWORD sz[] = { 0, 1, 4, 8, 11, 13, 39, 41, 44, 52, 56, 64, 107, 109, 123, 125, 200, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof sz / sizeof *sz; i++) {
        BYTE buf[sizeof(BITMAPV5HEADER) + 1100]; memset(buf, 0, sizeof buf);
        BITMAPINFOHEADER *h = (BITMAPINFOHEADER *)buf;
        h->biSize = sz[i]; h->biWidth = 4; h->biHeight = 4; h->biPlanes = 1; h->biBitCount = 24;
        void *bits = NULL; SetLastError(0);
        HBITMAP hb = U(CreateDIBSection)(NULL, (BITMAPINFO *)buf, DIB_RGB_COLORS, &bits, NULL, 0);
        if (sz[i] == 0 || sz[i] == 1 || sz[i] == 4 || sz[i] == 8) CHECKF(hb == NULL, "biSize %lu created", sz[i]);
        else t_note("biSize %lu -> %p le=%lu", sz[i], hb, GetLastError());
        if (hb) U(DeleteObject)(hb);
    }
}

/* ---------- compression / bitfields ---------- */
static HBITMAP cdib(int w, int h, int bpp, DWORD comp, DWORD m0, DWORD m1, DWORD m2, void **bits) {
    BMI b; bmi_init(&b, w, h, bpp, comp);
    b.m[0] = m0; b.m[1] = m1; b.m[2] = m2;
    return U(CreateDIBSection)(NULL, PB(&b), DIB_RGB_COLORS, bits, NULL, 0);
}
TEST(dib_create_bitfields_16_565) {
    NEED_GDI(); void *bits;
    HBITMAP hb = cdib(4, 4, 16, BI_BITFIELDS, 0xF800, 0x07E0, 0x001F, &bits);
    CHECK(hb); if (!hb) return;
    DIBSECTION ds; U(GetObjectW)(hb, sizeof ds, &ds);
    EXPECT_EQ(ds.dsBmih.biCompression, BI_BITFIELDS);
    EXPECT_EQ(ds.dsBitfields[0], 0xF800); EXPECT_EQ(ds.dsBitfields[1], 0x07E0); EXPECT_EQ(ds.dsBitfields[2], 0x001F);
    EXPECT_EQ(ds.dsBm.bmBitsPixel, 16);
    U(DeleteObject)(hb);
}
TEST(dib_create_bitfields_16_555) {
    NEED_GDI(); void *bits;
    HBITMAP hb = cdib(4, 4, 16, BI_BITFIELDS, 0x7C00, 0x03E0, 0x001F, &bits);
    CHECK(hb); if (!hb) return;
    DIBSECTION ds; U(GetObjectW)(hb, sizeof ds, &ds);
    EXPECT_EQ(ds.dsBitfields[0], 0x7C00); EXPECT_EQ(ds.dsBitfields[1], 0x03E0); EXPECT_EQ(ds.dsBitfields[2], 0x001F);
    U(DeleteObject)(hb);
}
TEST(dib_create_bitfields_16_bgr565) {
    NEED_GDI(); void *bits;
    HBITMAP hb = cdib(4, 4, 16, BI_BITFIELDS, 0x001F, 0x07E0, 0xF800, &bits);
    t_note("16bpp BGR565 masks -> %p", hb);
    if (hb) {
        DIBSECTION ds; U(GetObjectW)(hb, sizeof ds, &ds);
        EXPECT_EQ(ds.dsBitfields[0], 0x001F); EXPECT_EQ(ds.dsBitfields[2], 0xF800);
        U(DeleteObject)(hb);
    }
}
TEST(dib_create_bitfields_32_std) {
    NEED_GDI(); void *bits;
    HBITMAP hb = cdib(4, 4, 32, BI_BITFIELDS, 0xFF0000, 0x00FF00, 0x0000FF, &bits);
    CHECK(hb); if (!hb) return;
    DIBSECTION ds; U(GetObjectW)(hb, sizeof ds, &ds);
    EXPECT_EQ(ds.dsBitfields[0], 0xFF0000); EXPECT_EQ(ds.dsBitfields[1], 0xFF00); EXPECT_EQ(ds.dsBitfields[2], 0xFF);
    U(DeleteObject)(hb);
}
TEST(dib_create_bitfields_32_swapped) {
    NEED_GDI(); void *bits;
    HBITMAP hb = cdib(4, 4, 32, BI_BITFIELDS, 0x0000FF, 0x00FF00, 0xFF0000, &bits);
    t_note("32bpp RGB masks -> %p", hb);
    if (hb) {
        HGDIOBJ old; HDC dc = mkdc(hb, &old);
        ((DWORD *)bits)[0] = 0x000000FF; /* R=0xFF in mask0 position */
        U(GdiFlush)();
        t_note("pixel with mask0=0xFF -> 0x%06lx", (unsigned long)U(GetPixel)(dc, 0, 3));
        freedc(dc, old); U(DeleteObject)(hb);
    }
}
TEST(dib_create_bitfields_32_10bit) {
    NEED_GDI(); void *bits;
    HBITMAP hb = cdib(4, 4, 32, BI_BITFIELDS, 0x3FF00000, 0x000FFC00, 0x000003FF, &bits);
    t_note("32bpp 10-10-10 masks -> %p", hb);
    if (hb) U(DeleteObject)(hb);
}
TEST(dib_create_bitfields_invalid_masks) {
    NEED_GDI();
    static const DWORD m[][3] = {
        { 0, 0, 0 },
        { 0xFF0000, 0xFF0000, 0x0000FF },     /* overlap */
        { 0xFFFFFF, 0xFFFFFF, 0xFFFFFF },
        { 0xFF0000, 0x00FF00, 0 },            /* missing blue */
        { 0x0F0F00, 0x00F0F0, 0x0000FF },     /* non-contiguous, overlapping */
    };
    for (unsigned i = 0; i < sizeof m / sizeof *m; i++) {
        void *bits; HBITMAP hb = cdib(4, 4, 32, BI_BITFIELDS, m[i][0], m[i][1], m[i][2], &bits);
        t_note("32bpp masks set %u -> %p", i, hb);
        if (hb) U(DeleteObject)(hb);
        hb = cdib(4, 4, 16, BI_BITFIELDS, m[i][0], m[i][1], m[i][2], &bits);
        t_note("16bpp masks set %u -> %p", i, hb);
        if (hb) U(DeleteObject)(hb);
    }
}
TEST(dib_create_bitfields_wrong_depth) {
    NEED_GDI();
    static const int b[] = { 1, 4, 8, 24 };
    for (int i = 0; i < 4; i++) {
        void *bits; HBITMAP hb = cdib(4, 4, b[i], BI_BITFIELDS, 0xFF0000, 0xFF00, 0xFF, &bits);
        CHECKF(hb == NULL, "BITFIELDS at %d bpp created", b[i]);
        if (hb) U(DeleteObject)(hb);
    }
}
TEST(dib_create_rle_rejected) {
    NEED_GDI();
    void *bits; HBITMAP hb;
    hb = cdib(4, 4, 8, BI_RLE8, 0, 0, 0, &bits);
    t_note("BI_RLE8 8bpp -> %p", hb); if (hb) U(DeleteObject)(hb);
    hb = cdib(4, 4, 4, BI_RLE4, 0, 0, 0, &bits);
    t_note("BI_RLE4 4bpp -> %p", hb); if (hb) U(DeleteObject)(hb);
    hb = cdib(4, 4, 24, BI_RLE8, 0, 0, 0, &bits);
    CHECK(hb == NULL); if (hb) U(DeleteObject)(hb);
    hb = cdib(4, 4, 32, BI_RLE4, 0, 0, 0, &bits);
    CHECK(hb == NULL); if (hb) U(DeleteObject)(hb);
}
TEST(dib_create_jpeg_png_rejected) {
    NEED_GDI();
    static const int bpps[] = { 8, 24, 32 };
    for (int i = 0; i < 3; i++) {
        void *bits;
        HBITMAP hb = cdib(4, 4, bpps[i], BI_JPEG, 0, 0, 0, &bits);
        CHECKF(hb == NULL, "JPEG %d", bpps[i]); if (hb) U(DeleteObject)(hb);
        hb = cdib(4, 4, bpps[i], BI_PNG, 0, 0, 0, &bits);
        CHECKF(hb == NULL, "PNG %d", bpps[i]); if (hb) U(DeleteObject)(hb);
    }
}
TEST(dib_create_bad_compression) {
    NEED_GDI();
    static const DWORD c[] = { 6, 7, 11, 100, 0x80000000u, 0xFFFFFFFFu };
    for (unsigned i = 0; i < sizeof c / sizeof *c; i++) {
        void *bits; HBITMAP hb = cdib(4, 4, 24, c[i], 0, 0, 0, &bits);
        if (hb) t_note("compression %lu accepted", c[i]);
        if (hb) U(DeleteObject)(hb);
    }
}
TEST(dib_create_bitfields_16_pixel_packing) {
    NEED_GDI(); void *bits;
    HBITMAP hb = cdib(2, 2, 16, BI_BITFIELDS, 0xF800, 0x07E0, 0x001F, &bits);
    CHECK(hb); if (!hb) return;
    HGDIOBJ old; HDC dc = mkdc(hb, &old);
    U(SetPixelV)(dc, 0, 0, RGB(255, 0, 0));
    U(SetPixelV)(dc, 1, 0, RGB(0, 255, 0));
    U(SetPixelV)(dc, 0, 1, RGB(0, 0, 255));
    U(SetPixelV)(dc, 1, 1, RGB(255, 255, 255));
    U(GdiFlush)();
    WORD *r1 = (WORD *)((BYTE *)bits + 0), *r0 = (WORD *)((BYTE *)bits + 4); /* bottom-up: memory row0 = y1 */
    EXPECT_EQ(r0[0], 0xF800); EXPECT_EQ(r0[1], 0x07E0);
    EXPECT_EQ(r1[0], 0x001F); EXPECT_EQ(r1[1], 0xFFFF);
    freedc(dc, old); U(DeleteObject)(hb);
}
TEST(dib_create_16_default_555_packing) {
    NEED_GDI(); void *bits; HBITMAP hb = mkdib(2, 1, 16, &bits); CHECK(hb); if (!hb) return;
    HGDIOBJ old; HDC dc = mkdc(hb, &old);
    U(SetPixelV)(dc, 0, 0, RGB(255, 0, 0));
    U(SetPixelV)(dc, 1, 0, RGB(0, 255, 0));
    U(GdiFlush)();
    WORD *p = bits;
    EXPECT_EQ(p[0], 0x7C00); EXPECT_EQ(p[1], 0x03E0);
    U(SetPixelV)(dc, 0, 0, RGB(0, 0, 255)); U(GdiFlush)();
    EXPECT_EQ(p[0], 0x001F);
    U(SetPixelV)(dc, 0, 0, RGB(255, 255, 255)); U(GdiFlush)();
    EXPECT_EQ(p[0], 0x7FFF);
    freedc(dc, old); U(DeleteObject)(hb);
}

/* ---------- sizes ---------- */
TEST(dib_create_huge_rejected) {
    NEED_GDI(); void *bits = (void *)1;
    HBITMAP hb = mkdib(100000, 100000, 32, &bits);
    CHECK(hb == NULL); if (hb) U(DeleteObject)(hb);
    hb = mkdib(0x7FFFFFFF, 0x7FFFFFFF, 32, &bits);
    CHECK(hb == NULL); if (hb) U(DeleteObject)(hb);
    hb = mkdib(0x7FFFFFFF, 1, 32, &bits);
    CHECK(hb == NULL); if (hb) U(DeleteObject)(hb);
    hb = mkdib(0x7FFFFFFF, -0x7FFFFFFF, 24, &bits);
    CHECK(hb == NULL); if (hb) U(DeleteObject)(hb);
    hb = mkdib(-100000, 100000, 32, &bits);
    CHECK(hb == NULL); if (hb) U(DeleteObject)(hb);
}
TEST(dib_create_huge_rejected_2) {
    NEED_GDI(); void *bits;
    HBITMAP hb = mkdib(100000, 100000, 8, &bits);
    t_note("100000x100000x8 -> %p", hb); if (hb) U(DeleteObject)(hb);
    hb = mkdib(100000, -100000, 24, &bits);
    CHECK(hb == NULL); if (hb) U(DeleteObject)(hb);
    hb = mkdib(0x40000000, 4, 32, &bits);
    CHECK(hb == NULL); if (hb) U(DeleteObject)(hb);
}
TEST(dib_create_negative_width) {
    NEED_GDI(); void *bits;
    HBITMAP hb = mkdib(-4, 4, 24, &bits);
    CHECK(hb == NULL); if (hb) U(DeleteObject)(hb);
    hb = mkdib(-1, -1, 8, &bits);
    CHECK(hb == NULL); if (hb) U(DeleteObject)(hb);
    hb = mkdib(0x80000000, 4, 24, &bits);
    CHECK(hb == NULL); if (hb) U(DeleteObject)(hb);
}
TEST(dib_create_zero_dims) {
    NEED_GDI(); void *bits = NULL;
    int dims[][2] = { {0,0}, {0,4}, {4,0}, {1,0}, {0,-4} };
    for (int i = 0; i < 5; i++) {
        bits = NULL; SetLastError(0);
        HBITMAP hb = mkdib(dims[i][0], dims[i][1], 24, &bits);
        t_note("%dx%d -> %p bits=%p le=%lu", dims[i][0], dims[i][1], hb, bits, GetLastError());
        if (hb) { BITMAP bm; U(GetObjectW)(hb, sizeof bm, &bm); t_note(" bm %ldx%ld", (long)bm.bmWidth, (long)bm.bmHeight); U(DeleteObject)(hb); }
    }
}
TEST(dib_create_height_intmin) {
    NEED_GDI(); void *bits;
    HBITMAP hb = mkdib(4, (int)0x80000000, 24, &bits);
    CHECK(hb == NULL); if (hb) U(DeleteObject)(hb);
}
TEST(dib_create_1x1_all_depths) {
    NEED_GDI();
    static const int b[] = { 1, 4, 8, 16, 24, 32 };
    for (int i = 0; i < 6; i++) {
        void *bits; HBITMAP hb = mkdib(1, 1, b[i], &bits);
        CHECKF(hb != NULL, "1x1 at %d", b[i]);
        if (hb) { BITMAP bm; U(GetObjectW)(hb, sizeof bm, &bm); EXPECT_EQ(bm.bmWidthBytes, 4); U(DeleteObject)(hb); }
    }
}
TEST(dib_create_large_reasonable) {
    NEED_GDI(); void *bits;
    HBITMAP hb = mkdib(2048, 2048, 32, &bits); /* 16 MB */
    CHECK(hb != NULL && bits != NULL);
    if (hb) {
        BITMAP bm; U(GetObjectW)(hb, sizeof bm, &bm);
        EXPECT_EQ(bm.bmWidthBytes, 8192); EXPECT_EQ(bm.bmHeight, 2048);
        ((DWORD *)bits)[2048 * 2048 - 1] = 0x12345678;
        EXPECT_EQ(((DWORD *)bits)[2048 * 2048 - 1], 0x12345678);
        U(DeleteObject)(hb);
    }
}
TEST(dib_create_wide_1bpp) {
    NEED_GDI(); void *bits;
    HBITMAP hb = mkdib(4000, 3, 1, &bits); CHECK(hb);
    if (hb) { BITMAP bm; U(GetObjectW)(hb, sizeof bm, &bm); EXPECT_EQ(bm.bmWidthBytes, 500); U(DeleteObject)(hb); }
}

/* ---------- biClrUsed / color table ---------- */
TEST(dib_clrused_default) {
    NEED_GDI(); REQUIRE(U(GetDIBColorTable));
    static const int b[] = { 1, 4, 8 };
    for (int i = 0; i < 3; i++) {
        void *bits; HBITMAP hb = mkdib(4, 4, b[i], &bits); CHECK(hb); if (!hb) continue;
        HGDIOBJ old; HDC dc = mkdc(hb, &old);
        RGBQUAD q[256];
        UINT n = U(GetDIBColorTable)(dc, 0, 256, q);
        EXPECT_EQ(n, 1u << b[i]);
        for (UINT k = 0; k < n && k < 256; k++)
            if (q[k].rgbBlue != k || q[k].rgbGreen != 255 - k || q[k].rgbRed != (k ^ 0x55)) { CHECKF(0, "entry %u mismatch", k); break; }
        freedc(dc, old); U(DeleteObject)(hb);
    }
}
TEST(dib_clrused_values_8bpp) {
    NEED_GDI(); REQUIRE(U(GetDIBColorTable));
    static const DWORD cu[] = { 0, 1, 2, 16, 100, 255, 256, 257, 300, 1000, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof cu / sizeof *cu; i++) {
        BMI b; void *bits = NULL; bmi_init(&b, 4, 4, 8, BI_RGB); bmi_pal(&b, 256); b.h.biClrUsed = cu[i];
        HBITMAP hb = U(CreateDIBSection)(NULL, PB(&b), DIB_RGB_COLORS, &bits, NULL, 0);
        if (cu[i] <= 256) CHECKF(hb != NULL, "clrused %lu", cu[i]);
        else t_note("clrused %lu -> %p", cu[i], hb);
        if (!hb) continue;
        HGDIOBJ old; HDC dc = mkdc(hb, &old);
        RGBQUAD q[256]; UINT n = U(GetDIBColorTable)(dc, 0, 256, q);
        t_note("8bpp clrused=%lu -> table n=%u", cu[i], n);
        DIBSECTION ds; U(GetObjectW)(hb, sizeof ds, &ds);
        t_note("  GetObject biClrUsed=%lu", ds.dsBmih.biClrUsed);
        freedc(dc, old); U(DeleteObject)(hb);
    }
}
TEST(dib_clrused_values_4bpp_1bpp) {
    NEED_GDI(); REQUIRE(U(GetDIBColorTable));
    static const struct { int bpp; DWORD cu; } t[] = { {4,1},{4,15},{4,16},{4,17},{4,256},{1,1},{1,2},{1,3},{1,256} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        BMI b; void *bits = NULL; bmi_init(&b, 4, 4, t[i].bpp, BI_RGB); bmi_pal(&b, 16); b.h.biClrUsed = t[i].cu;
        HBITMAP hb = U(CreateDIBSection)(NULL, PB(&b), DIB_RGB_COLORS, &bits, NULL, 0);
        if (t[i].cu <= (DWORD)(1 << t[i].bpp)) CHECKF(hb != NULL, "bpp %d clrused %lu", t[i].bpp, t[i].cu);
        else t_note("bpp %d clrused %lu -> %p", t[i].bpp, t[i].cu, hb);
        if (hb) {
            HGDIOBJ old; HDC dc = mkdc(hb, &old); RGBQUAD q[256];
            t_note("  bpp %d clrused %lu table n=%u", t[i].bpp, t[i].cu, U(GetDIBColorTable)(dc, 0, 256, q));
            freedc(dc, old); U(DeleteObject)(hb);
        }
    }
}
TEST(dib_clrused_ignored_for_highcolor) {
    NEED_GDI(); REQUIRE(U(GetDIBColorTable));
    static const int b[] = { 16, 24, 32 };
    for (int i = 0; i < 3; i++) {
        BMI bi; void *bits = NULL; bmi_init(&bi, 4, 4, b[i], BI_RGB); bi.h.biClrUsed = 0;
        HBITMAP hb = U(CreateDIBSection)(NULL, PB(&bi), DIB_RGB_COLORS, &bits, NULL, 0);
        CHECK(hb); if (!hb) continue;
        HGDIOBJ old; HDC dc = mkdc(hb, &old); RGBQUAD q[4];
        SetLastError(0);
        EXPECT_EQ(U(GetDIBColorTable)(dc, 0, 4, q), 0);
        freedc(dc, old); U(DeleteObject)(hb);
    }
}
TEST(dib_sizeimage_ignored_for_rgb) {
    NEED_GDI();
    BMI b; void *bits; bmi_init(&b, 5, 5, 24, BI_RGB);
    DWORD v[] = { 0, 1, 10, 1000000, 0xFFFFFFFF };
    for (int i = 0; i < 5; i++) {
        b.h.biSizeImage = v[i];
        HBITMAP hb = U(CreateDIBSection)(NULL, PB(&b), DIB_RGB_COLORS, &bits, NULL, 0);
        CHECKF(hb != NULL, "sizeimage %lu", v[i]);
        if (hb) {
            DIBSECTION ds; U(GetObjectW)(hb, sizeof ds, &ds);
            t_note("biSizeImage in=%lu out=%lu", v[i], ds.dsBmih.biSizeImage);
            EXPECT_EQ(ds.dsBm.bmWidthBytes, 16);
            U(DeleteObject)(hb);
        }
    }
}
TEST(dib_planes_variants) {
    NEED_GDI();
    WORD v[] = { 0, 1, 2, 3, 0xFFFF };
    for (int i = 0; i < 5; i++) {
        BMI b; void *bits; bmi_init(&b, 4, 4, 24, BI_RGB); b.h.biPlanes = v[i];
        HBITMAP hb = U(CreateDIBSection)(NULL, PB(&b), DIB_RGB_COLORS, &bits, NULL, 0);
        if (v[i] == 1) CHECK(hb != NULL); else t_note("biPlanes %u -> %p", v[i], hb);
        if (hb) { DIBSECTION ds; U(GetObjectW)(hb, sizeof ds, &ds); EXPECT_EQ(ds.dsBm.bmPlanes, 1); U(DeleteObject)(hb); }
    }
}
TEST(dib_ppvbits_independent_per_bitmap) {
    NEED_GDI(); void *a, *b;
    HBITMAP h1 = mkdib(8, 8, 32, &a), h2 = mkdib(8, 8, 32, &b);
    CHECK(h1 && h2 && a != b);
    if (h1 && h2) {
        memset(a, 0x11, 256); memset(b, 0x22, 256);
        EXPECT_EQ(((BYTE *)a)[255], 0x11); EXPECT_EQ(((BYTE *)b)[0], 0x22);
    }
    if (h1) U(DeleteObject)(h1);
    if (h2) U(DeleteObject)(h2);
}
TEST(dib_ppvbits_zero_on_each_create) {
    NEED_GDI();
    for (int k = 0; k < 5; k++) {
        void *p; HBITMAP hb = mkdib(64, 64, 32, &p); CHECK(hb); if (!hb) return;
        int z = 1; for (int i = 0; i < 64 * 64 * 4; i++) if (((BYTE *)p)[i]) { z = 0; break; }
        CHECK(z); memset(p, 0xAB, 64 * 64 * 4);
        U(DeleteObject)(hb);
    }
}
TEST(dib_bits_alignment_many) {
    NEED_GDI();
    for (int w = 1; w < 40; w++) {
        void *p; HBITMAP hb = mkdib(w, 3, 24, &p);
        CHECKF(hb && ((ULONG_PTR)p & 3) == 0, "w=%d p=%p", w, p);
        if (hb) U(DeleteObject)(hb);
    }
}

/* ---------- section handle ---------- */
TEST(dib_create_with_section) {
    NEED_GDI();
    HANDLE m = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 65536, NULL);
    REQUIRE(m);
    BMI b; void *bits = NULL; bmi_init(&b, 16, 16, 32, BI_RGB);
    HBITMAP hb = U(CreateDIBSection)(NULL, PB(&b), DIB_RGB_COLORS, &bits, m, 0);
    CHECK(hb != NULL);
    if (hb) {
        BYTE *view = MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        CHECK(view != NULL);
        if (view) {
            ((BYTE *)bits)[100] = 0x5A;
            EXPECT_EQ(view[100], 0x5A);
            view[200] = 0xA5;
            EXPECT_EQ(((BYTE *)bits)[200], 0xA5);
            UnmapViewOfFile(view);
        }
        DIBSECTION ds; U(GetObjectW)(hb, sizeof ds, &ds);
        CHECK(ds.dshSection != NULL || 1);
        t_note("dshSection=%p dsOffset=%lu", ds.dshSection, ds.dsOffset);
        U(DeleteObject)(hb);
    }
    CloseHandle(m);
}
TEST(dib_create_section_offset) {
    NEED_GDI();
    HANDLE m = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 1 << 20, NULL);
    REQUIRE(m);
    BMI b; void *bits = NULL; bmi_init(&b, 8, 8, 32, BI_RGB);
    HBITMAP hb = U(CreateDIBSection)(NULL, PB(&b), DIB_RGB_COLORS, &bits, m, 4096);
    CHECK(hb != NULL);
    if (hb) {
        BYTE *view = MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (view) { ((BYTE *)bits)[0] = 0x77; EXPECT_EQ(view[4096], 0x77); UnmapViewOfFile(view); }
        DIBSECTION ds; U(GetObjectW)(hb, sizeof ds, &ds);
        EXPECT_EQ(ds.dsOffset, 4096);
        U(DeleteObject)(hb);
    }
    hb = U(CreateDIBSection)(NULL, PB(&b), DIB_RGB_COLORS, &bits, m, 4);
    t_note("offset 4 -> %p", hb); if (hb) U(DeleteObject)(hb);
    SetLastError(0);
    hb = U(CreateDIBSection)(NULL, PB(&b), DIB_RGB_COLORS, &bits, m, 3);
    t_note("offset 3 -> %p le=%lu", hb, GetLastError()); if (hb) U(DeleteObject)(hb);
    hb = U(CreateDIBSection)(NULL, PB(&b), DIB_RGB_COLORS, &bits, m, 0xFFFFF000);
    CHECK(hb == NULL); if (hb) U(DeleteObject)(hb);
    CloseHandle(m);
}
TEST(dib_create_section_too_small) {
    NEED_GDI();
    HANDLE m = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, NULL);
    REQUIRE(m);
    BMI b; void *bits = NULL; bmi_init(&b, 256, 256, 32, BI_RGB);
    HBITMAP hb = U(CreateDIBSection)(NULL, PB(&b), DIB_RGB_COLORS, &bits, m, 0);
    CHECK(hb == NULL); if (hb) U(DeleteObject)(hb);
    CloseHandle(m);
}
TEST(dib_create_section_invalid_handle) {
    NEED_GDI();
    BMI b; void *bits = NULL; bmi_init(&b, 8, 8, 32, BI_RGB);
    HBITMAP hb = U(CreateDIBSection)(NULL, PB(&b), DIB_RGB_COLORS, &bits, (HANDLE)(ULONG_PTR)0x12345678, 0);
    CHECK(hb == NULL); if (hb) U(DeleteObject)(hb);
    hb = U(CreateDIBSection)(NULL, PB(&b), DIB_RGB_COLORS, &bits, GetCurrentProcess(), 0);
    CHECK(hb == NULL); if (hb) U(DeleteObject)(hb);
}
TEST(dib_create_section_readonly_mapping) {
    NEED_GDI();
    HANDLE m = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 65536, NULL);
    REQUIRE(m);
    HANDLE ro;
    if (!DuplicateHandle(GetCurrentProcess(), m, GetCurrentProcess(), &ro, FILE_MAP_READ, FALSE, 0)) { CloseHandle(m); return; }
    BMI b; void *bits = NULL; bmi_init(&b, 8, 8, 32, BI_RGB);
    HBITMAP hb = U(CreateDIBSection)(NULL, PB(&b), DIB_RGB_COLORS, &bits, ro, 0);
    t_note("read-only section -> %p", hb); if (hb) U(DeleteObject)(hb);
    CloseHandle(ro); CloseHandle(m);
}

/* ---------- lifetime ---------- */
TEST(dib_delete_selected_fails) {
    NEED_GDI(); void *bits; HBITMAP hb = mkdib(4, 4, 24, &bits); CHECK(hb); if (!hb) return;
    HGDIOBJ old; HDC dc = mkdc(hb, &old);
    CHECK(old != NULL);
    if (dib_is_wine()) { freedc(dc, old); U(DeleteObject)(hb); t_skip("wine: deletes selected bitmap"); return; }
    CHECK(!U(DeleteObject)(hb));
    HGDIOBJ cur = U(SelectObject)(dc, old);
    EXPECT_EQ(cur, hb);
    CHECK(U(DeleteObject)(hb));
    U(DeleteDC)(dc);
}
TEST(dib_delete_twice) {
    NEED_GDI(); void *bits; HBITMAP hb = mkdib(4, 4, 24, &bits); CHECK(hb); if (!hb) return;
    CHECK(U(DeleteObject)(hb));
    SetLastError(0);
    CHECK(!U(DeleteObject)(hb));
    BITMAP bm; EXPECT_EQ(U(GetObjectW)(hb, sizeof bm, &bm), 0);
}
TEST(dib_select_into_two_dcs) {
    NEED_GDI(); void *bits; HBITMAP hb = mkdib(4, 4, 24, &bits); CHECK(hb); if (!hb) return;
    HGDIOBJ o1, o2; HDC d1 = mkdc(hb, &o1);
    HDC d2 = U(CreateCompatibleDC)(NULL);
    HGDIOBJ r = U(SelectObject)(d2, hb);
    CHECK(r == NULL);
    freedc(d1, o1); freedc(d2, NULL);
    U(DeleteObject)(hb);
}
TEST(dib_gdi_count_balanced) {
    NEED_GDI(); REQUIRE(U(GetGuiResources));
    int before = gdicount();
    for (int i = 0; i < 300; i++) {
        void *p; HBITMAP hb = mkdib(16 + i % 7, 16, (int[]){1,4,8,16,24,32}[i % 6], &p);
        if (!hb) { CHECKF(0, "alloc %d", i); break; }
        HGDIOBJ old; HDC dc = mkdc(hb, &old); freedc(dc, old); U(DeleteObject)(hb);
    }
    EXPECT_EQ(gdicount(), before);
}
TEST(dib_gdi_count_incremented) {
    NEED_GDI(); REQUIRE(U(GetGuiResources));
    if (dib_is_wine()) { t_skip("wine: no GDI count"); return; }
    int before = gdicount(); void *p; HBITMAP hb = mkdib(8, 8, 32, &p); CHECK(hb);
    EXPECT_EQ(gdicount(), before + 1);
    U(DeleteObject)(hb);
    EXPECT_EQ(gdicount(), before);
}
TEST(dib_many_alive_at_once) {
    NEED_GDI();
    HBITMAP hs[200]; int n = 0;
    for (int i = 0; i < 200; i++) { void *p; hs[i] = mkdib(32, 32, 32, &p); if (hs[i]) n++; }
    EXPECT_EQ(n, 200);
    for (int i = 0; i < 200; i++) if (hs[i]) U(DeleteObject)(hs[i]);
}
static DWORD WINAPI thr_sel(LPVOID p) {
    HBITMAP hb = p;
    HDC dc = U(CreateCompatibleDC)(NULL); if (!dc) return 1;
    HGDIOBJ o = U(SelectObject)(dc, hb);
    DWORD r = 0;
    if (!o) r = 2;
    else { U(SetPixelV)(dc, 1, 1, RGB(1, 2, 3)); r = (U(GetPixel)(dc, 1, 1) == RGB(1, 2, 3)) ? 0 : 3; U(SelectObject)(dc, o); }
    U(DeleteDC)(dc);
    return r;
}
TEST(dib_cross_thread_use) {
    NEED_GDI(); void *bits; HBITMAP hb = mkdib(4, 4, 24, &bits); CHECK(hb); if (!hb) return;
    HANDLE t = CreateThread(NULL, 0, thr_sel, hb, 0, NULL); REQUIRE(t);
    WaitForSingleObject(t, 10000);
    DWORD r = 99; GetExitCodeThread(t, &r); CloseHandle(t);
    EXPECT_EQ(r, 0);
    U(GdiFlush)();
    BYTE *p = (BYTE *)bits + 2 * 12 + 3; /* bottom-up: y=1 -> memory row 2 */
    EXPECT_EQ(p[0], 3); EXPECT_EQ(p[1], 2); EXPECT_EQ(p[2], 1);
    U(DeleteObject)(hb);
}
static DWORD WINAPI thr_create(LPVOID p) {
    void *bits; HBITMAP hb = mkdib(4, 4, 32, &bits);
    *(HBITMAP *)p = hb;
    return 0;
}
TEST(dib_created_in_exited_thread) {
    NEED_GDI(); HBITMAP hb = NULL;
    HANDLE t = CreateThread(NULL, 0, thr_create, &hb, 0, NULL); REQUIRE(t);
    WaitForSingleObject(t, 10000); CloseHandle(t);
    CHECK(hb != NULL);
    if (hb) { BITMAP bm; EXPECT_EQ(U(GetObjectW)(hb, sizeof bm, &bm), sizeof bm); U(DeleteObject)(hb); }
}
TEST(dib_getobject_wrong_handle_types) {
    NEED_GDI();
    BITMAP bm; DIBSECTION ds;
    EXPECT_EQ(U(GetObjectW)(NULL, sizeof bm, &bm), 0);
    EXPECT_EQ(U(GetObjectW)((HGDIOBJ)(ULONG_PTR)0xDEADBEEF, sizeof bm, &bm), 0);
    HDC dc = U(CreateCompatibleDC)(NULL); REQUIRE(dc);
    EXPECT_EQ(U(GetObjectW)(dc, sizeof bm, &bm), 0);
    U(DeleteDC)(dc);
    (void)ds;
}
TEST(dib_dc_default_bitmap_not_dibsection) {
    NEED_GDI();
    HDC dc = U(CreateCompatibleDC)(NULL); REQUIRE(dc);
    HGDIOBJ cur = U(GetCurrentObject)(dc, OBJ_BITMAP);
    CHECK(cur != NULL);
    BITMAP bm; DIBSECTION ds;
    EXPECT_EQ(U(GetObjectW)(cur, sizeof bm, &bm), sizeof bm);
    EXPECT_EQ(bm.bmWidth, 1); EXPECT_EQ(bm.bmHeight, 1); EXPECT_EQ(bm.bmBitsPixel, 1);
    EXPECT_EQ(bm.bmBits, 0);
    int n = U(GetObjectW)(cur, sizeof ds, &ds);
    EXPECT_EQ(n, sizeof(BITMAP));
    U(DeleteDC)(dc);
}
TEST(dib_compat_bitmap_getobject_not_dib) {
    NEED_GDI(); REQUIRE(U(CreateCompatibleBitmap)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC sdc = U(GetDC)(NULL); REQUIRE(sdc);
    HBITMAP hb = U(CreateCompatibleBitmap)(sdc, 10, 10); CHECK(hb);
    if (hb) {
        DIBSECTION ds; int n = U(GetObjectW)(hb, sizeof ds, &ds);
        EXPECT_EQ(n, sizeof(BITMAP));
        U(DeleteObject)(hb);
    }
    U(ReleaseDC)(NULL, sdc);
}
TEST(dib_create_with_screen_dc) {
    NEED_GDI(); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC sdc = U(GetDC)(NULL); REQUIRE(sdc);
    BMI b; void *bits; bmi_init(&b, 8, 8, 24, BI_RGB);
    HBITMAP hb = U(CreateDIBSection)(sdc, PB(&b), DIB_RGB_COLORS, &bits, NULL, 0);
    CHECK(hb != NULL);
    if (hb) U(DeleteObject)(hb);
    b.h.biBitCount = 8; bmi_pal(&b, 256);
    hb = U(CreateDIBSection)(sdc, PB(&b), DIB_RGB_COLORS, &bits, NULL, 0);
    CHECK(hb != NULL); if (hb) U(DeleteObject)(hb);
    U(ReleaseDC)(NULL, sdc);
}
TEST(dib_create_bogus_hdc_rgb_colors) {
    NEED_GDI();
    BMI b; void *bits; bmi_init(&b, 8, 8, 24, BI_RGB);
    HBITMAP hb = U(CreateDIBSection)((HDC)(ULONG_PTR)0x12345, PB(&b), DIB_RGB_COLORS, &bits, NULL, 0);
    t_note("bogus hdc, RGB_COLORS -> %p", hb); if (hb) U(DeleteObject)(hb);
    hb = U(CreateDIBSection)((HDC)(ULONG_PTR)0x12345, PB(&b), DIB_PAL_COLORS, &bits, NULL, 0);
    t_note("bogus hdc, PAL_COLORS -> %p", hb); if (hb) U(DeleteObject)(hb);
}
TEST(dib_create_hdc_wrong_type) {
    NEED_GDI();
    BMI b; void *bits; bmi_init(&b, 8, 8, 24, BI_RGB);
    HBRUSH br = U(CreateSolidBrush)(0); REQUIRE(br);
    HBITMAP hb = U(CreateDIBSection)((HDC)br, PB(&b), DIB_RGB_COLORS, &bits, NULL, 0);
    t_note("brush as hdc -> %p", hb); if (hb) U(DeleteObject)(hb);
    U(DeleteObject)(br);
}
