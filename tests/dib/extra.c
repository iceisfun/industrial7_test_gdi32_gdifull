#include "dibh.h"

#define NEED_X() do { NEED_GDI(); REQUIRE(U(GetDIBits)); REQUIRE(U(SetDIBits)); REQUIRE(U(BitBlt)); \
    REQUIRE(U(SetDIBColorTable)); REQUIRE(U(CreateBitmap)); } while (0)

static unsigned lcg(unsigned *s) { *s = *s * 1664525u + 1013904223u; return *s >> 8; }

/* SetDIBits / GetDIBits roundtrip through a DIB section: every width 1..33 */
static void rt(int bpp) {
    NEED_X();
    HDC dc = U(CreateCompatibleDC)(NULL); REQUIRE(dc);
    unsigned seed = 77 + bpp;
    for (int w = 1; w <= 33; w++) {
        int h = 3, st = dib_stride(w, bpp), n = st * h;
        BYTE *src = malloc(n), *dst = malloc(n);
        if (!src || !dst) { free(src); free(dst); break; }
        for (int i = 0; i < n; i++) src[i] = (BYTE)lcg(&seed);
        /* keep indices in range and padding bits out of the comparison */
        int bits_row = w * bpp;
        for (int y = 0; y < h; y++) {
            BYTE *r = src + y * st;
            if (bpp <= 8) for (int x = 0; x < w; x++) {
                if (bpp == 8) r[x] = (BYTE)lcg(&seed);
                else if (bpp == 4) { BYTE v = (BYTE)(lcg(&seed) & 0xFF); if (x & 1) r[x / 2] = (r[x / 2] & 0xF0) | (v & 0xF); else r[x / 2] = (v & 0xF0) | (r[x / 2] & 0xF); }
                else { if (lcg(&seed) & 1) r[x / 8] |= 0x80 >> (x & 7); else r[x / 8] &= ~(0x80 >> (x & 7)); }
            }
            if (bits_row & 7) r[bits_row / 8] &= (BYTE)(0xFF << (8 - (bits_row & 7)));
            for (int i = (bits_row + 7) / 8; i < st; i++) r[i] = 0;
        }
        void *bits; HBITMAP hb = mkdib(w, h, bpp, &bits);
        if (!hb) { CHECKF(0, "mkdib w=%d", w); free(src); free(dst); continue; }
        BMI b; bmi_init(&b, w, h, bpp, BI_RGB); if (bpp <= 8) bmi_pal(&b, 1 << bpp);
        int r1 = U(SetDIBits)(dc, hb, 0, h, src, PB(&b), DIB_RGB_COLORS);
        CHECKF(r1 == h, "SetDIBits w=%d -> %d", w, r1);
        memset(dst, 0xEE, n);
        bmi_init(&b, w, h, bpp, BI_RGB); if (bpp <= 8) bmi_pal(&b, 1 << bpp);
        int r2 = U(GetDIBits)(dc, hb, 0, h, dst, PB(&b), DIB_RGB_COLORS);
        CHECKF(r2 == h, "GetDIBits w=%d -> %d", w, r2);
        int bad = 0;
        for (int y = 0; y < h; y++) {
            int full = bits_row / 8;
            if (memcmp(src + y * st, dst + y * st, full)) bad++;
            if ((bits_row & 7) && ((src[y * st + full] ^ dst[y * st + full]) & (0xFF << (8 - (bits_row & 7))) & 0xFF)) bad++;
        }
        CHECKF(bad == 0, "bpp %d w %d mismatching rows %d", bpp, w, bad);
        U(DeleteObject)(hb); free(src); free(dst);
    }
    U(DeleteDC)(dc);
}
TEST(dib_x_roundtrip_1) { rt(1); }
TEST(dib_x_roundtrip_4) { rt(4); }
TEST(dib_x_roundtrip_8) { rt(8); }
TEST(dib_x_roundtrip_16) { rt(16); }
TEST(dib_x_roundtrip_24) { rt(24); }
TEST(dib_x_roundtrip_32) { rt(32); }

/* tiny and degenerate sizes */
static void tiny(int bpp) {
    NEED_GDI();
    void *p; HBITMAP hb = mkdib(1, 1, bpp, &p); CHECK(hb); if (!hb) return;
    BITMAP bm; EXPECT_EQ(U(GetObjectW)(hb, sizeof bm, &bm), sizeof bm);
    EXPECT_EQ(bm.bmWidth, 1); EXPECT_EQ(bm.bmHeight, 1); EXPECT_EQ(bm.bmWidthBytes, 4);
    CHECK(bm.bmBits == p);
    DIBSECTION ds; EXPECT_EQ(U(GetObjectW)(hb, sizeof ds, &ds), sizeof ds);
    EXPECT_EQ(ds.dsBmih.biWidth, 1); EXPECT_EQ(ds.dsBmih.biHeight, 1);
    EXPECT_EQ(ds.dsBmih.biPlanes, 1); EXPECT_EQ(ds.dsBmih.biBitCount, bpp);
    U(DeleteObject)(hb);
}
TEST(dib_x_tiny_1) { tiny(1); }
TEST(dib_x_tiny_4) { tiny(4); }
TEST(dib_x_tiny_8) { tiny(8); }
TEST(dib_x_tiny_16) { tiny(16); }
TEST(dib_x_tiny_24) { tiny(24); }
TEST(dib_x_tiny_32) { tiny(32); }

TEST(dib_x_thin_shapes) {
    NEED_GDI();
    static const int d[][2] = { { 4096, 1 }, { 1, 4096 }, { 4095, 2 }, { 2, 4093 } };
    for (int i = 0; i < 4; i++) {
        void *p; HBITMAP hb = mkdib(d[i][0], d[i][1], 8, &p);
        CHECKF(hb && p, "%dx%d", d[i][0], d[i][1]);
        if (!hb) continue;
        BITMAP bm; U(GetObjectW)(hb, sizeof bm, &bm);
        EXPECT_EQ(bm.bmWidth, d[i][0]); EXPECT_EQ(bm.bmHeight, d[i][1]);
        EXPECT_EQ(bm.bmWidthBytes, dib_stride(d[i][0], 8));
        ((BYTE *)p)[dib_stride(d[i][0], 8) * d[i][1] - 1] = 0x42; /* last byte writable */
        U(DeleteObject)(hb);
    }
}
TEST(dib_x_large_2048_corner) {
    NEED_GDI();
    void *p; HBITMAP hb = mkdib(2048, 2048, 32, &p); CHECK(hb); if (!hb) return;
    HGDIOBJ old; HDC dc = mkdc(hb, &old);
    ((DWORD *)p)[2047 * 2048 + 2047] = 0x00123456;           /* top-down y=0 is the LAST row for bottom-up */
    U(GdiFlush)();
    /* bottom-up: last row in memory is the top of the image */
    EXPECT_EQ(U(GetPixel)(dc, 2047, 0), RGB(0x12, 0x34, 0x56));
    ((DWORD *)p)[0] = 0x00ABCDEF;
    U(GdiFlush)();
    EXPECT_EQ(U(GetPixel)(dc, 0, 2047), RGB(0xAB, 0xCD, 0xEF));
    freedc(dc, old); U(DeleteObject)(hb);
}

/* many live DIB sections at once */
TEST(dib_x_many_live) {
    NEED_GDI(); REQUIRE(U(GetGuiResources));
    int before = gdicount();
    enum { N = 150 };
    HBITMAP h[N]; BYTE *p[N]; int ok = 0;
    for (int i = 0; i < N; i++) { void *b; h[i] = mkdib(8 + i % 5, 8, 32, &b); p[i] = b; if (h[i]) ok++; }
    CHECK(ok == N);
    for (int i = 0; i < N; i++) if (h[i]) memset(p[i], i, 8 * 4);
    int bad = 0;
    for (int i = 0; i < N; i++) if (h[i] && (p[i][0] != (BYTE)i || p[i][31] != (BYTE)i)) bad++;
    CHECKF(bad == 0, "bad %d", bad);
    /* distinct, non-overlapping buffers */
    int ov = 0;
    for (int i = 0; i < N; i++) for (int j = i + 1; j < N; j++) if (h[i] && h[j] && p[i] == p[j]) ov++;
    CHECK(ov == 0);
    for (int i = 0; i < N; i++) if (h[i]) U(DeleteObject)(h[i]);
    EXPECT_EQ(gdicount(), before);
}

/* DDB GetObject semantics next to DIB sections */
TEST(dib_x_getobject_ddb_dibsection_size) {
    NEED_X();
    HBITMAP ddb = U(CreateBitmap)(5, 3, 1, 1, NULL); CHECK(ddb); if (!ddb) return;
    DIBSECTION ds; memset(&ds, 0xCC, sizeof ds);
    int r = U(GetObjectW)(ddb, sizeof ds, &ds);
    EXPECT_EQ(r, sizeof(BITMAP));
    EXPECT_EQ(ds.dsBm.bmWidth, 5); EXPECT_EQ(ds.dsBm.bmHeight, 3);
    U(DeleteObject)(ddb);
    void *p; HBITMAP dib = mkdib(5, 3, 24, &p);
    BITMAP bm; r = U(GetObjectW)(dib, sizeof bm, &bm);
    EXPECT_EQ(r, sizeof bm);
    U(DeleteObject)(dib);
}

/* BitBlt from a palettised DIB to a true-colour DIB uses the colour table */
static void blt_pal_to_true(int bpp, int dbpp) {
    NEED_X();
    int n = 1 << bpp, w = 16;
    void *sp, *dp;
    HBITMAP s = mkdib(w, 1, bpp, &sp), d = mkdib(w, 1, dbpp, &dp); CHECK(s && d);
    if (!s || !d) { if (s) U(DeleteObject)(s); if (d) U(DeleteObject)(d); return; }
    HGDIOBJ so, dob; HDC sdc = mkdc(s, &so), ddc = mkdc(d, &dob);
    RGBQUAD q[256];
    for (int i = 0; i < n; i++) { q[i].rgbBlue = (BYTE)(i * 11 + 3); q[i].rgbGreen = (BYTE)(200 - i * 9); q[i].rgbRed = (BYTE)(i * 17 + 5); q[i].rgbReserved = 0; }
    EXPECT_EQ(U(SetDIBColorTable)(sdc, 0, n, q), n);
    /* write indices directly */
    BYTE *b = sp;
    for (int x = 0; x < w; x++) {
        int idx = x % n;
        if (bpp == 8) b[x] = (BYTE)idx;
        else if (bpp == 4) b[x / 2] = (x & 1) ? (b[x / 2] & 0xF0) | idx : (BYTE)((idx << 4) | (b[x / 2] & 0xF));
        else { if (idx) b[x / 8] |= 0x80 >> (x & 7); else b[x / 8] &= ~(0x80 >> (x & 7)); }
    }
    U(GdiFlush)();
    CHECK(U(BitBlt)(ddc, 0, 0, w, 1, sdc, 0, 0, SRCCOPY));
    U(GdiFlush)();
    int bad = 0;
    for (int x = 0; x < w; x++) {
        int idx = x % n;
        COLORREF want = RGB(q[idx].rgbRed, q[idx].rgbGreen, q[idx].rgbBlue);
        if (U(GetPixel)(ddc, x, 0) != want) bad++;
        if (dbpp == 32) { DWORD v = ((DWORD *)dp)[x]; if ((v & 0xFFFFFF) != (DWORD)((q[idx].rgbRed << 16) | (q[idx].rgbGreen << 8) | q[idx].rgbBlue)) bad++; }
        if (dbpp == 24) { BYTE *t = (BYTE *)dp + x * 3; if (t[0] != q[idx].rgbBlue || t[1] != q[idx].rgbGreen || t[2] != q[idx].rgbRed) bad++; }
    }
    CHECKF(bad == 0, "bpp %d->%d bad %d", bpp, dbpp, bad);
    freedc(sdc, so); freedc(ddc, dob); U(DeleteObject)(s); U(DeleteObject)(d);
}
TEST(dib_x_blt_1_to_24) { blt_pal_to_true(1, 24); }
TEST(dib_x_blt_4_to_24) { blt_pal_to_true(4, 24); }
TEST(dib_x_blt_8_to_24) { blt_pal_to_true(8, 24); }
TEST(dib_x_blt_1_to_32) { blt_pal_to_true(1, 32); }
TEST(dib_x_blt_4_to_32) { blt_pal_to_true(4, 32); }
TEST(dib_x_blt_8_to_32) { blt_pal_to_true(8, 32); }

/* BitBlt from true colour to an 8bpp DIB with exact-match table entries */
TEST(dib_x_blt_32_to_8_exact) {
    NEED_X();
    void *sp, *dp;
    HBITMAP s = mkdib(4, 1, 32, &sp), d = mkdib(4, 1, 8, &dp); CHECK(s && d);
    if (!s || !d) return;
    HGDIOBJ so, dob; HDC sdc = mkdc(s, &so), ddc = mkdc(d, &dob);
    RGBQUAD q[4] = { { 0, 0, 0, 0 }, { 0, 0, 255, 0 }, { 0, 255, 0, 0 }, { 255, 0, 0, 0 } };
    EXPECT_EQ(U(SetDIBColorTable)(ddc, 0, 4, q), 4);
    ((DWORD *)sp)[0] = 0x00FF0000; ((DWORD *)sp)[1] = 0x0000FF00; ((DWORD *)sp)[2] = 0x000000FF; ((DWORD *)sp)[3] = 0;
    U(GdiFlush)();
    CHECK(U(BitBlt)(ddc, 0, 0, 4, 1, sdc, 0, 0, SRCCOPY));
    U(GdiFlush)();
    BYTE *b = dp; EXPECT_EQ(b[0], 1); EXPECT_EQ(b[1], 2); EXPECT_EQ(b[2], 3); EXPECT_EQ(b[3], 0);
    freedc(sdc, so); freedc(ddc, dob); U(DeleteObject)(s); U(DeleteObject)(d);
}

/* 16bpp -> 32bpp expansion keeps the significant bits */
TEST(dib_x_getdibits_16_to_32_top_bits) {
    NEED_X();
    void *p; HBITMAP hb = mkdib(4, 1, 16, &p); CHECK(hb); if (!hb) return;
    WORD v[4] = { 0x7C00, 0x03E0, 0x001F, 0x4210 };
    memcpy(p, v, sizeof v);
    HDC dc = U(CreateCompatibleDC)(NULL);
    BMI b; bmi_init(&b, 4, 1, 32, BI_RGB); DWORD out[4] = { 0 };
    EXPECT_EQ(U(GetDIBits)(dc, hb, 0, 1, out, PB(&b), DIB_RGB_COLORS), 1);
    for (int i = 0; i < 4; i++) {
        BYTE R = (BYTE)(out[i] >> 16), G = (BYTE)(out[i] >> 8), B = (BYTE)out[i];
        CHECKF((R >> 3) == ((v[i] >> 10) & 31) && (G >> 3) == ((v[i] >> 5) & 31) && (B >> 3) == (v[i] & 31), "px %d -> %08lx", i, out[i]);
    }
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}

/* CreateDIBitmap: dimensions come from the header, for many sizes */
TEST(dib_x_createdibitmap_dims) {
    NEED_X(); REQUIRE(U(CreateDIBitmap));
    HDC dc = U(CreateCompatibleDC)(NULL); REQUIRE(dc);
    static const int d[][2] = { { 1, 1 }, { 2, 7 }, { 31, 3 }, { 32, 32 }, { 33, 1 }, { 100, 50 } };
    for (int i = 0; i < 6; i++) {
        BMI b; bmi_init(&b, d[i][0], d[i][1], 24, BI_RGB);
        HBITMAP hb = U(CreateDIBitmap)(dc, &b.h, 0, NULL, NULL, DIB_RGB_COLORS);
        CHECKF(hb != NULL, "%dx%d", d[i][0], d[i][1]);
        if (!hb) continue;
        BITMAP bm; EXPECT_EQ(U(GetObjectW)(hb, sizeof bm, &bm), sizeof bm);
        EXPECT_EQ(bm.bmWidth, d[i][0]); EXPECT_EQ(bm.bmHeight, d[i][1]);
        EXPECT_EQ(U(GetObjectType)(hb), OBJ_BITMAP);
        U(DeleteObject)(hb);
    }
    U(DeleteDC)(dc);
}
TEST(dib_x_createdibitmap_topdown_dims) {
    NEED_X(); REQUIRE(U(CreateDIBitmap));
    HDC dc = U(CreateCompatibleDC)(NULL); REQUIRE(dc);
    BMI b; bmi_init(&b, 6, -4, 24, BI_RGB);
    HBITMAP hb = U(CreateDIBitmap)(dc, &b.h, 0, NULL, NULL, DIB_RGB_COLORS);
    CHECK(hb);
    if (hb) { BITMAP bm; U(GetObjectW)(hb, sizeof bm, &bm); EXPECT_EQ(bm.bmWidth, 6); EXPECT_EQ(bm.bmHeight, 4); U(DeleteObject)(hb); }
    U(DeleteDC)(dc);
}

/* CBM_INIT: pixels land in the right rows */
TEST(dib_x_createdibitmap_init_rows) {
    NEED_X(); REQUIRE(U(CreateDIBitmap));
    void *tp; HBITMAP target = mkdib(2, 2, 32, &tp); CHECK(target); if (!target) return;
    HGDIOBJ old; HDC dc = mkdc(target, &old);
    BMI b; bmi_init(&b, 2, 2, 24, BI_RGB);
    BYTE bits[16] = { 0 }; /* bottom row first: stride 8 */
    bits[0] = 1; bits[1] = 2; bits[2] = 3;   /* bottom-left  R3 G2 B1 */
    bits[3] = 4; bits[4] = 5; bits[5] = 6;   /* bottom-right R6 G5 B4 */
    bits[8] = 7; bits[9] = 8; bits[10] = 9;  /* top-left     R9 G8 B7 */
    bits[11] = 10; bits[12] = 11; bits[13] = 12; /* top-right R12 */
    HBITMAP hb = U(CreateDIBitmap)(dc, &b.h, CBM_INIT, bits, PB(&b), DIB_RGB_COLORS);
    CHECK(hb);
    if (hb) {
        HDC m = U(CreateCompatibleDC)(dc); HGDIOBJ mo = U(SelectObject)(m, hb);
        CHECK(U(BitBlt)(dc, 0, 0, 2, 2, m, 0, 0, SRCCOPY));
        U(GdiFlush)();
        int bad = 0;
        COLORREF want[4] = { RGB(9, 8, 7), RGB(12, 11, 10), RGB(3, 2, 1), RGB(6, 5, 4) };
        for (int i = 0; i < 4; i++) if (U(GetPixel)(dc, i & 1, i >> 1) != want[i]) bad++;
        t_note("CBM_INIT rows mismatches (depth-dependent) = %d", bad);
        U(SelectObject)(m, mo); U(DeleteDC)(m); U(DeleteObject)(hb);
    }
    freedc(dc, old); U(DeleteObject)(target);
}

/* Cross thread: bits pointer and handle remain usable after the creating thread exits */
static DWORD WINAPI mk_thread(LPVOID a) {
    void **o = a; void *bits; HBITMAP hb = mkdib(8, 8, 32, &bits);
    if (hb) memset(bits, 0x5A, 8 * 8 * 4);
    o[0] = hb; o[1] = bits; return 0;
}
TEST(dib_x_survives_creating_thread) {
    NEED_GDI();
    void *o[2] = { 0, 0 };
    HANDLE t = CreateThread(NULL, 0, mk_thread, o, 0, NULL); REQUIRE(t);
    WaitForSingleObject(t, 10000); CloseHandle(t);
    HBITMAP hb = o[0]; CHECK(hb != NULL); if (!hb) return;
    EXPECT_EQ(((BYTE *)o[1])[100], 0x5A);
    BITMAP bm; EXPECT_EQ(U(GetObjectW)(hb, sizeof bm, &bm), sizeof bm);
    HGDIOBJ old; HDC dc = mkdc(hb, &old);
    EXPECT_EQ(U(GetPixel)(dc, 1, 1) & 0xFFFFFF, 0x5A5A5Au);
    freedc(dc, old);
    CHECK(U(DeleteObject)(hb));
}

/* invalid-argument sweep: wrong object type as bitmap/hdc handles */
TEST(dib_x_setdibits_wrong_handle_types) {
    NEED_X();
    HDC dc = U(CreateCompatibleDC)(NULL); REQUIRE(dc);
    HBRUSH br = U(CreateSolidBrush)(0); HPEN pen = U(CreatePen)(PS_SOLID, 1, 0);
    BMI b; bmi_init(&b, 2, 2, 24, BI_RGB); BYTE bits[16] = { 0 };
    HGDIOBJ o[] = { br, pen, dc, NULL, (HGDIOBJ)(ULONG_PTR)0x1234 };
    for (int i = 0; i < 5; i++)
        CHECKF(U(SetDIBits)(dc, (HBITMAP)o[i], 0, 2, bits, PB(&b), DIB_RGB_COLORS) == 0, "handle %d", i);
    U(DeleteObject)(br); U(DeleteObject)(pen); U(DeleteDC)(dc);
}

/* biSizeImage and biClrUsed in GetDIBits query output for DDBs */
TEST(dib_x_getdibits_query_ddb_sizeimage) {
    NEED_X();
    HBITMAP ddb = U(CreateBitmap)(13, 3, 1, 1, NULL); CHECK(ddb); if (!ddb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    BMI b; bmi_init(&b, 0, 0, 24, BI_RGB);
    b.h.biBitCount = 0;
    CHECK(U(GetDIBits)(dc, ddb, 0, 0, NULL, PB(&b), DIB_RGB_COLORS));
    EXPECT_EQ(b.h.biSize, sizeof(BITMAPINFOHEADER));
    EXPECT_EQ(b.h.biWidth, 13); EXPECT_EQ(abs(b.h.biHeight), 3);
    EXPECT_EQ(b.h.biPlanes, 1);
    EXPECT_EQ(b.h.biSizeImage, (DWORD)(dib_stride(13, b.h.biBitCount) * 3));
    U(DeleteDC)(dc); U(DeleteObject)(ddb);
}

/* GetDIBits for requested sizeimage: caller-supplied biSizeImage is replaced */
TEST(dib_x_getdibits_fills_sizeimage) {
    NEED_X();
    void *p; HBITMAP hb = mkdib(5, 3, 24, &p); CHECK(hb); if (!hb) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    BMI b; bmi_init(&b, 5, 3, 24, BI_RGB); b.h.biSizeImage = 0;
    BYTE buf[64];
    EXPECT_EQ(U(GetDIBits)(dc, hb, 0, 3, buf, PB(&b), DIB_RGB_COLORS), 3);
    EXPECT_EQ(b.h.biSizeImage, 48);
    EXPECT_EQ(b.h.biWidth, 5);
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}

/* GetDIBits does not modify the source bits */
TEST(dib_x_getdibits_source_unchanged) {
    NEED_X();
    void *p; HBITMAP hb = mkdib(7, 5, 32, &p); CHECK(hb); if (!hb) return;
    unsigned s = 5; for (int i = 0; i < 7 * 5 * 4; i++) ((BYTE *)p)[i] = (BYTE)lcg(&s);
    BYTE copy[7 * 5 * 4]; memcpy(copy, p, sizeof copy);
    HDC dc = U(CreateCompatibleDC)(NULL);
    BMI b; bmi_init(&b, 7, 5, 24, BI_RGB); BYTE buf[24 * 5];
    EXPECT_EQ(U(GetDIBits)(dc, hb, 0, 5, buf, PB(&b), DIB_RGB_COLORS), 5);
    EXPECT_EQ(memcmp(copy, p, sizeof copy), 0);
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}

/* SetDIBits must not touch rows outside the requested range */
TEST(dib_x_setdibits_leaves_other_rows) {
    NEED_X();
    void *p; HBITMAP hb = mkdib(4, 6, 32, &p); CHECK(hb); if (!hb) return;
    memset(p, 0x11, 4 * 6 * 4);
    HDC dc = U(CreateCompatibleDC)(NULL);
    BMI b; bmi_init(&b, 4, 6, 32, BI_RGB);
    DWORD src[4 * 6]; for (int i = 0; i < 24; i++) src[i] = 0x00FFFFFF;
    int r = U(SetDIBits)(dc, hb, 2, 2, src, PB(&b), DIB_RGB_COLORS);
    CHECK(r == 2);
    int n_ff = 0, n_11 = 0;
    for (int i = 0; i < 24; i++) { DWORD v = ((DWORD *)p)[i]; if ((v & 0xFFFFFF) == 0xFFFFFF) n_ff++; else if (v == 0x11111111) n_11++; }
    CHECK(n_ff == 8); CHECK(n_11 == 16);
    U(DeleteDC)(dc); U(DeleteObject)(hb);
}

/* SetDIBitsToDevice with a zero-sized dest/src rect and with cLines>0 but empty width */
TEST(dib_x_sdtd_zero_width) {
    NEED_X(); REQUIRE(U(SetDIBitsToDevice));
    void *p; HBITMAP hb = mkdib(4, 4, 32, &p); CHECK(hb); if (!hb) return;
    HGDIOBJ old; HDC dc = mkdc(hb, &old);
    BMI b; bmi_init(&b, 4, 4, 32, BI_RGB); DWORD src[16]; for (int i = 0; i < 16; i++) src[i] = 0x00FFFFFF;
    int r = U(SetDIBitsToDevice)(dc, 0, 0, 0, 4, 0, 0, 0, 4, src, PB(&b), DIB_RGB_COLORS);
    t_note("zero width -> %d", r);
    int touched = 0; for (int i = 0; i < 16; i++) if (((DWORD *)p)[i]) touched++;
    CHECKF(touched == 0, "touched %d", touched);
    freedc(dc, old); U(DeleteObject)(hb);
}
