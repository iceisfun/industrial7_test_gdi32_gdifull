#include "bm.h"

/* ---- GetObject on DDBs ---------------------------------------------------- */
TEST(bitmaps_getobject_null_buffer_size) {
    BM_REQ();
    HBITMAP b = U(CreateBitmap)(8, 8, 1, 1, NULL);
    REQUIRE(b);
    EXPECT_EQ(U(GetObjectA)(b, 0, NULL), sizeof(BITMAP));
    EXPECT_EQ(U(GetObjectA)(b, 100, NULL), sizeof(BITMAP));
    U(DeleteObject)(b);
}
TEST(bitmaps_getobject_oversized_buffer) {
    BM_REQ();
    HBITMAP b = U(CreateBitmap)(5, 6, 1, 8, NULL);
    REQUIRE(b);
    unsigned char buf[128]; memset(buf, 0xAB, sizeof buf);
    EXPECT_EQ(U(GetObjectA)(b, sizeof buf, buf), sizeof(BITMAP));
    EXPECT_EQ(buf[sizeof(BITMAP)], 0xAB);
    EXPECT_EQ(buf[127], 0xAB);
    BITMAP *bm = (BITMAP *)buf;
    EXPECT_EQ(bm->bmWidth, 5); EXPECT_EQ(bm->bmHeight, 6); EXPECT_EQ(bm->bmBitsPixel, 8);
    U(DeleteObject)(b);
}
TEST(bitmaps_getobject_small_buffer) {
    BM_REQ();
    HBITMAP b = U(CreateBitmap)(5, 6, 1, 8, NULL);
    REQUIRE(b);
    static const int sz[] = { 0, 1, 4, 8, 16, 24, 31 }; unsigned i;
    for (i = 0; i < 7; i++) {
        unsigned char buf[64]; memset(buf, 0xAB, sizeof buf);
        int n = U(GetObjectA)(b, sz[i], buf);
        t_note("GetObject(bitmap, cb=%d) -> %d", sz[i], n);
        CHECKF(n <= sz[i], "no overrun cb=%d n=%d", sz[i], n);
        CHECKF(buf[sz[i] > 0 && sz[i] < 64 ? sz[i] : 40] == 0xAB, "byte past cb untouched (%d)", sz[i]);
    }
    U(DeleteObject)(b);
}
TEST(bitmaps_getobject_w_parity) {
    BM_REQ(); REQUIRE(U(GetObjectW));
    HBITMAP b = U(CreateBitmap)(7, 3, 1, 16, NULL);
    REQUIRE(b);
    BITMAP a, w; memset(&a, 1, sizeof a); memset(&w, 2, sizeof w);
    EXPECT_EQ(U(GetObjectA)(b, sizeof a, &a), sizeof a);
    EXPECT_EQ(U(GetObjectW)(b, sizeof w, &w), sizeof w);
    CHECK(a.bmWidth == w.bmWidth && a.bmHeight == w.bmHeight && a.bmBitsPixel == w.bmBitsPixel && a.bmWidthBytes == w.bmWidthBytes && a.bmBits == w.bmBits);
    EXPECT_EQ(U(GetObjectW)(b, 0, NULL), sizeof(BITMAP));
    U(DeleteObject)(b);
}
TEST(bitmaps_getobject_null_handle) {
    BM_REQ();
    BITMAP bm;
    SET_ERR(0);
    EXPECT_EQ(U(GetObjectA)(NULL, sizeof bm, &bm), 0);
    EXPECT_EQ(U(GetObjectA)(NULL, 0, NULL), 0);
}
TEST(bitmaps_getobject_bogus_handles) {
    BM_REQ();
    static const ULONG_PTR h[] = { 1, 2, 0x100, 0xdeadbeef, 0x00800001, 0x10000, (ULONG_PTR)-1 };
    unsigned i; BITMAP bm;
    for (i = 0; i < 7; i++) {
        memset(&bm, 0, sizeof bm);
        EXPECT_EQ(U(GetObjectA)((HANDLE)h[i], sizeof bm, &bm), 0);
        EXPECT_EQ(U(GetObjectType)((HGDIOBJ)h[i]), 0);
    }
}
TEST(bitmaps_getobject_after_delete) {
    BM_REQ();
    HBITMAP b = U(CreateBitmap)(4, 4, 1, 1, NULL);
    REQUIRE(b);
    CHECK(U(DeleteObject)(b));
    BITMAP bm;
    EXPECT_EQ(U(GetObjectA)(b, sizeof bm, &bm), 0);
    EXPECT_EQ(U(GetObjectType)(b), 0);
    CHECK(!U(DeleteObject)(b));
}
TEST(bitmaps_getobject_dc_handle_is_not_bitmap) {
    BM_REQ();
    HDC dc = U(CreateCompatibleDC)(NULL);
    REQUIRE(dc);
    BITMAP bm;
    EXPECT_EQ(U(GetObjectType)(dc), OBJ_MEMDC);
    EXPECT_EQ(U(GetObjectA)(dc, sizeof bm, &bm), 0);
    U(DeleteDC)(dc);
}
TEST(bitmaps_getobject_other_gdi_types_not_bitmap) {
    BM_REQ(); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(CreatePen));
    HBRUSH br = U(CreateSolidBrush)(0x123456);
    HPEN pen = U(CreatePen)(PS_SOLID, 1, 0);
    BITMAP bm;
    EXPECT_EQ(U(GetObjectType)(br), OBJ_BRUSH);
    EXPECT_EQ(U(GetObjectType)(pen), OBJ_PEN);
    EXPECT_EQ(U(GetObjectA)(br, sizeof bm, &bm) == sizeof(BITMAP), 0);
    EXPECT_EQ(U(GetObjectA)(pen, sizeof bm, &bm) == sizeof(BITMAP), 0);
    U(DeleteObject)(br); U(DeleteObject)(pen);
}
TEST(bitmaps_getobject_widthbytes_table) {
    BM_REQ();
    static const int bpps[] = {1,4,8,16,24,32}; unsigned i; int w;
    for (i = 0; i < 6; i++)
        for (w = 1; w <= 40; w++) {
            HBITMAP b = U(CreateBitmap)(w, 2, 1, bpps[i], NULL);
            if (!b) { CHECKF(0, "create %d bpp %d", w, bpps[i]); continue; }
            BITMAP bm; bm_getobj(b, &bm);
            CHECKF(bm.bmWidthBytes == bm_row_bytes(w, bpps[i]), "w %d bpp %d wb %d", w, bpps[i], bm.bmWidthBytes);
            CHECK((bm.bmWidthBytes & 1) == 0);
            U(DeleteObject)(b);
        }
}
TEST(bitmaps_getobject_large_dims) {
    BM_REQ();
    HBITMAP b = U(CreateBitmap)(4096, 2048, 1, 1, NULL);
    REQUIRE(b);
    BITMAP bm; bm_getobj(b, &bm);
    EXPECT_EQ(bm.bmWidth, 4096); EXPECT_EQ(bm.bmHeight, 2048); EXPECT_EQ(bm.bmWidthBytes, 512);
    U(DeleteObject)(b);
    b = U(CreateBitmap)(1, 4096, 1, 32, NULL);
    REQUIRE(b);
    bm_getobj(b, &bm);
    EXPECT_EQ(bm.bmWidth, 1); EXPECT_EQ(bm.bmHeight, 4096); EXPECT_EQ(bm.bmWidthBytes, 4);
    U(DeleteObject)(b);
}
TEST(bitmaps_getobject_selected_bitmap_same) {
    BM_REQ();
    ddb_t d;
    REQUIRE(ddb_open(&d, 12, 5, 1, 1, NULL));
    BITMAP bm; bm_getobj(d.bm, &bm);
    EXPECT_EQ(bm.bmWidth, 12); EXPECT_EQ(bm.bmHeight, 5);
    HGDIOBJ cur = U(GetCurrentObject)(d.dc, OBJ_BITMAP);
    CHECK(cur == (HGDIOBJ)d.bm);
    ddb_close(&d);
}

/* ---- GetObject on DIBSections --------------------------------------------- */
TEST(bitmaps_getobject_dib_full) {
    BM_REQ_DIB();
    static const int bpps[] = {1,4,8,16,24,32}; unsigned i;
    for (i = 0; i < 6; i++) {
        dib_t d;
        if (!dib_open(&d, 7, 5, bpps[i])) { CHECKF(0, "open %d", bpps[i]); continue; }
        DIBSECTION ds; memset(&ds, 0xCC, sizeof ds);
        EXPECT_EQ(U(GetObjectA)(d.bm, sizeof ds, &ds), sizeof(DIBSECTION));
        EXPECT_EQ(ds.dsBm.bmType, 0);
        EXPECT_EQ(ds.dsBm.bmWidth, 7); EXPECT_EQ(ds.dsBm.bmHeight, 5);
        EXPECT_EQ(ds.dsBm.bmPlanes, 1); EXPECT_EQ(ds.dsBm.bmBitsPixel, bpps[i]);
        int wb = ((7 * bpps[i] + 31) >> 5) * 4;
        EXPECT_EQ(ds.dsBm.bmWidthBytes, wb);
        CHECK(ds.dsBm.bmBits == d.bits);
        EXPECT_EQ(ds.dsBmih.biSize, sizeof(BITMAPINFOHEADER));
        EXPECT_EQ(ds.dsBmih.biWidth, 7); EXPECT_EQ(ds.dsBmih.biHeight, 5);
        EXPECT_EQ(ds.dsBmih.biPlanes, 1); EXPECT_EQ(ds.dsBmih.biBitCount, bpps[i]);
        if (bpps[i] == 16 || bpps[i] == 32) t_note("bpp %d BI_RGB dib reports compression %lu", bpps[i], ds.dsBmih.biCompression);
        else EXPECT_EQ(ds.dsBmih.biCompression, BI_RGB);
        EXPECT_EQ(ds.dsBmih.biSizeImage, wb * 5);
        EXPECT_EQ(ds.dshSection, 0);
        EXPECT_EQ(ds.dsOffset, 0);
        EXPECT_EQ(U(GetObjectType)(d.bm), OBJ_BITMAP);
        dib_close(&d);
    }
}
TEST(bitmaps_getobject_dib_topdown_height_sign) {
    BM_REQ_DIB();
    dib_t d;
    REQUIRE(dib_open_ex(&d, 6, 4, 32, 0x100, NULL, 0));
    DIBSECTION ds; memset(&ds, 0, sizeof ds);
    EXPECT_EQ(U(GetObjectA)(d.bm, sizeof ds, &ds), sizeof ds);
    EXPECT_EQ(ds.dsBm.bmHeight, 4);
    t_note("top-down dib dsBmih.biHeight = %ld", ds.dsBmih.biHeight);
    dib_close(&d);
}
TEST(bitmaps_getobject_dib_cb_variants) {
    BM_REQ_DIB();
    dib_t d;
    REQUIRE(dib_open(&d, 4, 4, 24));
    EXPECT_EQ(U(GetObjectA)(d.bm, 0, NULL) > 0, 1);
    DIBSECTION ds;
    EXPECT_EQ(U(GetObjectA)(d.bm, sizeof(BITMAP), &ds), sizeof(BITMAP));
    EXPECT_EQ(U(GetObjectA)(d.bm, sizeof ds + 64, &ds), sizeof ds);
    int n = U(GetObjectA)(d.bm, sizeof ds - 1, &ds);
    t_note("GetObject(dib, sizeof(DIBSECTION)-1) -> %d", n);
    n = U(GetObjectA)(d.bm, sizeof(BITMAP) - 1, &ds);
    t_note("GetObject(dib, sizeof(BITMAP)-1) -> %d", n);
    t_note("GetObject(dib, 0, NULL) -> %d", U(GetObjectA)(d.bm, 0, NULL));
    dib_close(&d);
}
TEST(bitmaps_getobject_dib_565_bitfields) {
    BM_REQ_DIB();
    dib_t d;
    REQUIRE(dib_open_ex(&d, 4, 4, 16, 1, NULL, 0));
    DIBSECTION ds; memset(&ds, 0, sizeof ds);
    EXPECT_EQ(U(GetObjectA)(d.bm, sizeof ds, &ds), sizeof ds);
    EXPECT_EQ(ds.dsBmih.biCompression, BI_BITFIELDS);
    EXPECT_EQ(ds.dsBitfields[0], 0xF800);
    EXPECT_EQ(ds.dsBitfields[1], 0x07E0);
    EXPECT_EQ(ds.dsBitfields[2], 0x001F);
    EXPECT_EQ(ds.dsBm.bmBitsPixel, 16);
    EXPECT_EQ(ds.dsBm.bmWidthBytes, 8);
    dib_close(&d);
}
TEST(bitmaps_getobject_dib_555_default_masks) {
    BM_REQ_DIB();
    dib_t d;
    REQUIRE(dib_open(&d, 4, 4, 16));
    DIBSECTION ds; memset(&ds, 0, sizeof ds);
    EXPECT_EQ(U(GetObjectA)(d.bm, sizeof ds, &ds), sizeof ds);
    t_note("16bpp BI_RGB compression %lu dsBitfields %lx %lx %lx", ds.dsBmih.biCompression, ds.dsBitfields[0], ds.dsBitfields[1], ds.dsBitfields[2]);
    dib_close(&d);
}
TEST(bitmaps_getobject_dib_32_bitfields) {
    BM_REQ_DIB();
    dib_t d;
    REQUIRE(dib_open_ex(&d, 4, 4, 32, 2, NULL, 0));
    DIBSECTION ds; memset(&ds, 0, sizeof ds);
    EXPECT_EQ(U(GetObjectA)(d.bm, sizeof ds, &ds), sizeof ds);
    EXPECT_EQ(ds.dsBmih.biCompression, BI_BITFIELDS);
    EXPECT_EQ(ds.dsBitfields[0], 0xFF0000);
    EXPECT_EQ(ds.dsBitfields[1], 0x00FF00);
    EXPECT_EQ(ds.dsBitfields[2], 0x0000FF);
    dib_close(&d);
}
TEST(bitmaps_getobject_dib_w_parity) {
    BM_REQ_DIB(); REQUIRE(U(GetObjectW));
    dib_t d;
    REQUIRE(dib_open(&d, 3, 3, 8));
    DIBSECTION a, w; memset(&a, 1, sizeof a); memset(&w, 2, sizeof w);
    EXPECT_EQ(U(GetObjectA)(d.bm, sizeof a, &a), sizeof a);
    EXPECT_EQ(U(GetObjectW)(d.bm, sizeof w, &w), sizeof w);
    CHECK(!memcmp(&a, &w, sizeof a));
    dib_close(&d);
}
TEST(bitmaps_getobject_dib_clrused) {
    BM_REQ_DIB();
    dib_t d;
    REQUIRE(dib_open(&d, 4, 4, 8));
    DIBSECTION ds; memset(&ds, 0, sizeof ds);
    EXPECT_EQ(U(GetObjectA)(d.bm, sizeof ds, &ds), sizeof ds);
    EXPECT_EQ(ds.dsBmih.biClrUsed, 256);
    dib_close(&d);
    REQUIRE(dib_open(&d, 4, 4, 4));
    memset(&ds, 0, sizeof ds);
    EXPECT_EQ(U(GetObjectA)(d.bm, sizeof ds, &ds), sizeof ds);
    EXPECT_EQ(ds.dsBmih.biClrUsed, 16);
    dib_close(&d);
}

/* ---- GetBitmapBits ------------------------------------------------------------ */
TEST(bitmaps_getbits_null_buffer_returns_size) {
    BM_REQ(); REQUIRE(U(GetBitmapBits));
    static const int c[][3] = { {8,8,1},{9,3,1},{5,5,8},{3,7,16},{10,2,24},{4,4,32},{17,1,4} }; unsigned i;
    for (i = 0; i < 7; i++) {
        HBITMAP b = U(CreateBitmap)(c[i][0], c[i][1], 1, c[i][2], NULL);
        REQUIRE(b);
        LONG want = bm_row_bytes(c[i][0], c[i][2]) * c[i][1];
        EXPECT_EQ(U(GetBitmapBits)(b, 0, NULL), want);
        EXPECT_EQ(U(GetBitmapBits)(b, 1, NULL), want);
        U(DeleteObject)(b);
    }
}
TEST(bitmaps_getbits_count_variants) {
    BM_REQ(); REQUIRE(U(GetBitmapBits));
    unsigned char in[16], out[32]; int i;
    for (i = 0; i < 16; i++) in[i] = (unsigned char)(0x10 + i);
    HBITMAP b = U(CreateBitmap)(16, 8, 1, 1, NULL);   /* 16 bytes */
    REQUIRE(b);
    EXPECT_EQ(U(SetBitmapBits)(b, 16, in), 16);
    static const int cnt[] = { 1, 2, 3, 5, 7, 15, 16 };
    for (i = 0; i < 7; i++) {
        memset(out, 0xEE, sizeof out);
        LONG n = U(GetBitmapBits)(b, cnt[i], out);
        CHECKF(n == cnt[i], "count %d got %ld", cnt[i], n);
        CHECK(!memcmp(in, out, cnt[i]));
        EXPECT_EQ(out[cnt[i]], 0xEE);
    }
    memset(out, 0xEE, sizeof out);
    EXPECT_EQ(U(GetBitmapBits)(b, 32, out), 16);
    CHECK(!memcmp(in, out, 16));
    EXPECT_EQ(out[16], 0xEE);
    EXPECT_EQ(U(GetBitmapBits)(b, 0, out), 0);
    U(DeleteObject)(b);
}
TEST(bitmaps_getbits_invalid_handles) {
    BM_REQ(); REQUIRE(U(GetBitmapBits)); REQUIRE(U(CreatePen));
    unsigned char out[8];
    EXPECT_EQ(U(GetBitmapBits)(NULL, 8, out), 0);
    EXPECT_EQ(U(GetBitmapBits)((HBITMAP)0xdeadbeef, 8, out), 0);
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0);
    EXPECT_EQ(U(GetBitmapBits)((HBITMAP)p, 8, out), 0);
    U(DeleteObject)(p);
    HBITMAP b = U(CreateBitmap)(8, 8, 1, 1, NULL);
    U(DeleteObject)(b);
    EXPECT_EQ(U(GetBitmapBits)(b, 8, out), 0);
    HDC dc = U(CreateCompatibleDC)(NULL);
    EXPECT_EQ(U(GetBitmapBits)((HBITMAP)dc, 8, out), 0);
    U(DeleteDC)(dc);
}
TEST(bitmaps_getbits_negative_count) {
    BM_REQ(); REQUIRE(U(GetBitmapBits));
    HBITMAP b = U(CreateBitmap)(8, 8, 1, 1, NULL);
    REQUIRE(b);
    unsigned char out[16]; memset(out, 0xEE, sizeof out);
    LONG n = U(GetBitmapBits)(b, -1, out);
    t_note("GetBitmapBits(count=-1) -> %ld", n);
    CHECK(out[0] == 0xEE || n > 0);
    U(DeleteObject)(b);
}
TEST(bitmaps_getbits_mono_padding_bytes) {
    BM_REQ(); REQUIRE(U(GetBitmapBits)); REQUIRE(U(SetBitmapBits));
    /* 3x2 mono: 2 bytes per row, only the top 3 bits are pixels */
    HBITMAP b = U(CreateBitmap)(3, 2, 1, 1, NULL);
    REQUIRE(b);
    unsigned char in[4] = { 0xA0, 0x00, 0x40, 0x00 }, out[4] = {0xEE,0xEE,0xEE,0xEE};
    EXPECT_EQ(U(SetBitmapBits)(b, 4, in), 4);
    EXPECT_EQ(U(GetBitmapBits)(b, 4, out), 4);
    EXPECT_EQ(out[0] & 0xE0, 0xA0);
    EXPECT_EQ(out[2] & 0xE0, 0x40);
    EXPECT_EQ(out[1], 0x00); EXPECT_EQ(out[3], 0x00);
    U(DeleteObject)(b);
}
TEST(bitmaps_getbits_roundtrip_all_depths) {
    BM_REQ(); REQUIRE(U(GetBitmapBits)); REQUIRE(U(SetBitmapBits));
    static const int bpps[] = {1,4,8,16,24,32}; unsigned i;
    for (i = 0; i < 6; i++) {
        int w = 7, h = 5, wb = bm_row_bytes(w, bpps[i]), n = wb * h, k;
        unsigned char *in = malloc(n), *out = malloc(n);
        HBITMAP b = U(CreateBitmap)(w, h, 1, bpps[i], NULL);
        for (k = 0; k < n; k++) in[k] = (unsigned char)(k * 37 + 11);
        /* clear the padding bits/bytes so only pixel data is compared */
        if (b) {
            EXPECT_EQ(U(SetBitmapBits)(b, n, in), n);
            memset(out, 0, n);
            EXPECT_EQ(U(GetBitmapBits)(b, n, out), n);
            int row, bad = 0;
            for (row = 0; row < h; row++) {
                int pixbytes = (w * bpps[i]) / 8;
                if (memcmp(in + row * wb, out + row * wb, pixbytes)) bad = 1;
            }
            if (bpps[i] == 32 && bad) t_note("32bpp DDB roundtrip is not byte exact");
            else CHECKF(!bad, "roundtrip bpp %d", bpps[i]);
            U(DeleteObject)(b);
        } else CHECKF(0, "create bpp %d", bpps[i]);
        free(in); free(out);
    }
}
TEST(bitmaps_getbits_dibsection_32bpp) {
    BM_REQ_DIB(); REQUIRE(U(GetBitmapBits));
    dib_t d;
    REQUIRE(dib_open_ex(&d, 3, 3, 32, 0x100, NULL, 0));
    int i; DWORD *p = (DWORD *)d.bits;
    for (i = 0; i < 9; i++) p[i] = 0x01010101u * (i + 1) & 0x00FFFFFF;
    U(GdiFlush)();
    DWORD out[9]; memset(out, 0xEE, sizeof out);
    LONG n = U(GetBitmapBits)(d.bm, sizeof out, out);
    EXPECT_EQ(n, 36);
    CHECK(!memcmp(out, p, 36));
    EXPECT_EQ(U(GetBitmapBits)(d.bm, 0, NULL), 36);
    dib_close(&d);
}
TEST(bitmaps_getbits_dibsection_sizes) {
    BM_REQ_DIB(); REQUIRE(U(GetBitmapBits));
    static const int bpps[] = {1,4,8,16,24,32}; unsigned i;
    for (i = 0; i < 6; i++) {
        dib_t d;
        if (!dib_open(&d, 5, 3, bpps[i])) { CHECK(0); continue; }
        LONG want = ((5 * bpps[i] + 31) >> 5) * 4 * 3;
        LONG n = U(GetBitmapBits)(d.bm, 0, NULL);
        if (bpps[i] == 32) EXPECT_EQ(n, want);
        else t_note("GetBitmapBits(dib bpp %d,0,NULL) = %ld (dword-aligned %ld)", bpps[i], n, want);
        dib_close(&d);
    }
}
TEST(bitmaps_getbits_reflects_drawing_mono) {
    BM_REQ(); REQUIRE(U(GetBitmapBits)); REQUIRE(U(PatBlt));
    ddb_t d;
    REQUIRE(ddb_open(&d, 16, 4, 1, 1, NULL));
    unsigned char out[8];
    CHECK(U(PatBlt)(d.dc, 0, 0, 16, 4, WHITENESS));
    EXPECT_EQ(U(GetBitmapBits)(d.bm, 8, out), 8);
    int i, ok = 1; for (i = 0; i < 8; i++) if (out[i] != 0xFF) ok = 0;
    CHECK(ok);
    CHECK(U(PatBlt)(d.dc, 0, 0, 16, 4, BLACKNESS));
    EXPECT_EQ(U(GetBitmapBits)(d.bm, 8, out), 8);
    ok = 1; for (i = 0; i < 8; i++) if (out[i] != 0x00) ok = 0;
    CHECK(ok);
    CHECK(U(PatBlt)(d.dc, 8, 1, 4, 2, WHITENESS));
    EXPECT_EQ(U(GetBitmapBits)(d.bm, 8, out), 8);
    EXPECT_EQ(out[2], 0x00); EXPECT_EQ(out[3], 0xF0); EXPECT_EQ(out[4], 0x00); EXPECT_EQ(out[5], 0xF0);
    EXPECT_EQ(out[0], 0); EXPECT_EQ(out[6], 0);
    ddb_close(&d);
}

/* ---- SetBitmapBits -------------------------------------------------------------- */
TEST(bitmaps_setbits_basic_counts) {
    BM_REQ(); REQUIRE(U(SetBitmapBits)); REQUIRE(U(GetBitmapBits));
    HBITMAP b = U(CreateBitmap)(16, 4, 1, 1, NULL);   /* 8 bytes */
    REQUIRE(b);
    unsigned char z[8] = {0}, in[8] = {1,2,3,4,5,6,7,8}, out[8];
    EXPECT_EQ(U(SetBitmapBits)(b, 8, z), 8);
    EXPECT_EQ(U(SetBitmapBits)(b, 4, in), 4);
    EXPECT_EQ(U(GetBitmapBits)(b, 8, out), 8);
    CHECK(!memcmp(out, in, 4));
    CHECK(out[4] == 0 && out[5] == 0 && out[6] == 0 && out[7] == 0);
    EXPECT_EQ(U(SetBitmapBits)(b, 0, in), 0);
    U(DeleteObject)(b);
}
TEST(bitmaps_setbits_odd_count) {
    BM_REQ(); REQUIRE(U(SetBitmapBits)); REQUIRE(U(GetBitmapBits));
    HBITMAP b = U(CreateBitmap)(16, 4, 1, 1, NULL);
    REQUIRE(b);
    unsigned char z[8] = {0}, in[8] = {0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88}, out[8];
    U(SetBitmapBits)(b, 8, z);
    LONG n = U(SetBitmapBits)(b, 3, in);
    t_note("SetBitmapBits(odd count 3) -> %ld", n);
    U(GetBitmapBits)(b, 8, out);
    EXPECT_EQ(out[0], 0x81); EXPECT_EQ(out[1], 0x82);
    t_note("third byte after odd count: %02x", out[2]);
    EXPECT_EQ(out[3], 0); EXPECT_EQ(out[4], 0);
    U(DeleteObject)(b);
}
TEST(bitmaps_setbits_count_exceeds_size) {
    BM_REQ(); REQUIRE(U(SetBitmapBits)); REQUIRE(U(GetBitmapBits));
    HBITMAP b = U(CreateBitmap)(16, 2, 1, 1, NULL);   /* 4 bytes */
    REQUIRE(b);
    unsigned char in[16]; memset(in, 0x5A, sizeof in);
    LONG n = U(SetBitmapBits)(b, 16, in);
    t_note("SetBitmapBits(count 16 on 4-byte bitmap) -> %ld", n);
    CHECK(n == 4 || n == 16);
    unsigned char out[4];
    EXPECT_EQ(U(GetBitmapBits)(b, 4, out), 4);
    CHECK(out[0] == 0x5A && out[3] == 0x5A);
    U(DeleteObject)(b);
}
TEST(bitmaps_setbits_invalid_handles) {
    BM_REQ(); REQUIRE(U(SetBitmapBits)); REQUIRE(U(CreateSolidBrush));
    unsigned char in[8] = {0};
    EXPECT_EQ(U(SetBitmapBits)(NULL, 8, in), 0);
    EXPECT_EQ(U(SetBitmapBits)((HBITMAP)0x12345, 8, in), 0);
    HBRUSH br = U(CreateSolidBrush)(0);
    EXPECT_EQ(U(SetBitmapBits)((HBITMAP)br, 8, in), 0);
    U(DeleteObject)(br);
    HBITMAP b = U(CreateBitmap)(8, 8, 1, 1, NULL);
    U(DeleteObject)(b);
    EXPECT_EQ(U(SetBitmapBits)(b, 8, in), 0);
}
TEST(bitmaps_setbits_visible_via_getpixel_mono) {
    BM_REQ(); REQUIRE(U(SetBitmapBits)); REQUIRE(U(GetPixel));
    ddb_t d;
    REQUIRE(ddb_open(&d, 8, 2, 1, 1, NULL));
    unsigned char in[4] = { 0xA0, 0, 0x01, 0 };
    EXPECT_EQ(U(SetBitmapBits)(d.bm, 4, in), 4);
    EXPECT_EQ(U(GetPixel)(d.dc, 0, 0), 0xFFFFFF);
    EXPECT_EQ(U(GetPixel)(d.dc, 1, 0), 0);
    EXPECT_EQ(U(GetPixel)(d.dc, 2, 0), 0xFFFFFF);
    EXPECT_EQ(U(GetPixel)(d.dc, 3, 0), 0);
    EXPECT_EQ(U(GetPixel)(d.dc, 7, 1), 0xFFFFFF);
    EXPECT_EQ(U(GetPixel)(d.dc, 6, 1), 0);
    ddb_close(&d);
}
TEST(bitmaps_setbits_on_selected_bitmap) {
    BM_REQ(); REQUIRE(U(SetBitmapBits)); REQUIRE(U(GetBitmapBits));
    ddb_t d;
    REQUIRE(ddb_open(&d, 16, 2, 1, 1, NULL));
    unsigned char in[4] = { 0x12, 0x34, 0x56, 0x78 }, out[4];
    EXPECT_EQ(U(SetBitmapBits)(d.bm, 4, in), 4);
    EXPECT_EQ(U(GetBitmapBits)(d.bm, 4, out), 4);
    CHECK(!memcmp(in, out, 4));
    ddb_close(&d);
}

/* ---- Bitmap dimension ------------------------------------------------------------ */
TEST(bitmaps_dimension_default_zero) {
    BM_REQ(); REQUIRE(U(GetBitmapDimensionEx));
    HBITMAP b = U(CreateBitmap)(8, 8, 1, 1, NULL);
    REQUIRE(b);
    SIZE s = { 77, 88 };
    CHECK(U(GetBitmapDimensionEx)(b, &s));
    EXPECT_EQ(s.cx, 0); EXPECT_EQ(s.cy, 0);
    U(DeleteObject)(b);
}
TEST(bitmaps_dimension_set_get) {
    BM_REQ(); REQUIRE(U(GetBitmapDimensionEx)); REQUIRE(U(SetBitmapDimensionEx));
    HBITMAP b = U(CreateBitmap)(8, 8, 1, 1, NULL);
    REQUIRE(b);
    SIZE prev = { 1, 1 }, s;
    CHECK(U(SetBitmapDimensionEx)(b, 123, 456, &prev));
    EXPECT_EQ(prev.cx, 0); EXPECT_EQ(prev.cy, 0);
    CHECK(U(GetBitmapDimensionEx)(b, &s));
    EXPECT_EQ(s.cx, 123); EXPECT_EQ(s.cy, 456);
    CHECK(U(SetBitmapDimensionEx)(b, 7, 9, &prev));
    EXPECT_EQ(prev.cx, 123); EXPECT_EQ(prev.cy, 456);
    CHECK(U(SetBitmapDimensionEx)(b, 5, 6, NULL));
    CHECK(U(GetBitmapDimensionEx)(b, &s));
    EXPECT_EQ(s.cx, 5); EXPECT_EQ(s.cy, 6);
    U(DeleteObject)(b);
}
TEST(bitmaps_dimension_values_unrestricted) {
    BM_REQ(); REQUIRE(U(GetBitmapDimensionEx)); REQUIRE(U(SetBitmapDimensionEx));
    HBITMAP b = U(CreateBitmap)(2, 2, 1, 1, NULL);
    REQUIRE(b);
    static const int v[][2] = { {-1,-1},{0,0},{0x7fffffff,0x7fffffff},{(int)0x80000000,5},{1,-1000000} }; unsigned i;
    for (i = 0; i < 5; i++) {
        SIZE s = {0,0};
        BOOL r = U(SetBitmapDimensionEx)(b, v[i][0], v[i][1], NULL);
        U(GetBitmapDimensionEx)(b, &s);
        if (!r) t_note("SetBitmapDimensionEx(%d,%d) rejected", v[i][0], v[i][1]);
        else CHECKF(s.cx == v[i][0] && s.cy == v[i][1], "%d,%d got %ld,%ld", v[i][0], v[i][1], s.cx, s.cy);
    }
    U(DeleteObject)(b);
}
TEST(bitmaps_dimension_independent_of_pixels) {
    BM_REQ(); REQUIRE(U(GetBitmapDimensionEx)); REQUIRE(U(SetBitmapDimensionEx));
    HBITMAP b = U(CreateBitmap)(30, 20, 1, 1, NULL);
    REQUIRE(b);
    U(SetBitmapDimensionEx)(b, 1000, 2000, NULL);
    BITMAP bm; bm_getobj(b, &bm);
    EXPECT_EQ(bm.bmWidth, 30); EXPECT_EQ(bm.bmHeight, 20);
    U(DeleteObject)(b);
}
TEST(bitmaps_dimension_persists_across_select) {
    BM_REQ(); REQUIRE(U(GetBitmapDimensionEx)); REQUIRE(U(SetBitmapDimensionEx));
    ddb_t d;
    REQUIRE(ddb_open(&d, 4, 4, 1, 1, NULL));
    U(SetBitmapDimensionEx)(d.bm, 40, 50, NULL);
    SIZE s;
    CHECK(U(GetBitmapDimensionEx)(d.bm, &s));
    EXPECT_EQ(s.cx, 40); EXPECT_EQ(s.cy, 50);
    U(SelectObject)(d.dc, d.old); d.old = NULL;
    CHECK(U(GetBitmapDimensionEx)(d.bm, &s));
    EXPECT_EQ(s.cx, 40); EXPECT_EQ(s.cy, 50);
    ddb_close(&d);
}
TEST(bitmaps_dimension_invalid_handles) {
    BM_REQ(); REQUIRE(U(GetBitmapDimensionEx)); REQUIRE(U(SetBitmapDimensionEx)); REQUIRE(U(CreateSolidBrush));
    SIZE s = {5,5};
    CHECK(!U(GetBitmapDimensionEx)(NULL, &s));
    CHECK(!U(SetBitmapDimensionEx)(NULL, 1, 1, &s));
    CHECK(!U(GetBitmapDimensionEx)((HBITMAP)0xbeef, &s));
    HBRUSH br = U(CreateSolidBrush)(0);
    CHECK(!U(GetBitmapDimensionEx)((HBITMAP)br, &s));
    CHECK(!U(SetBitmapDimensionEx)((HBITMAP)br, 1, 1, &s));
    U(DeleteObject)(br);
    HBITMAP b = U(CreateBitmap)(4, 4, 1, 1, NULL);
    U(DeleteObject)(b);
    CHECK(!U(GetBitmapDimensionEx)(b, &s));
    CHECK(!U(SetBitmapDimensionEx)(b, 1, 1, &s));
}
TEST(bitmaps_dimension_dibsection) {
    BM_REQ_DIB(); REQUIRE(U(GetBitmapDimensionEx)); REQUIRE(U(SetBitmapDimensionEx));
    dib_t d;
    REQUIRE(dib_open(&d, 4, 4, 24));
    SIZE s = {9,9}, p = {9,9};
    BOOL r = U(GetBitmapDimensionEx)(d.bm, &s);
    t_note("GetBitmapDimensionEx(dib) -> %d (%ld,%ld)", r, s.cx, s.cy);
    r = U(SetBitmapDimensionEx)(d.bm, 11, 12, &p);
    t_note("SetBitmapDimensionEx(dib) -> %d (prev %ld,%ld)", r, p.cx, p.cy);
    if (r) { U(GetBitmapDimensionEx)(d.bm, &s); EXPECT_EQ(s.cx, 11); EXPECT_EQ(s.cy, 12); }
    dib_close(&d);
}
TEST(bitmaps_dimension_stock_default_bitmap) {
    BM_REQ(); REQUIRE(U(GetBitmapDimensionEx));
    SIZE s = {1,1};
    BOOL r = U(GetBitmapDimensionEx)(U(GetStockObject)(BM_DEFAULT_BITMAP), &s);
    t_note("dimension of stock bitmap -> %d (%ld,%ld)", r, s.cx, s.cy);
}
