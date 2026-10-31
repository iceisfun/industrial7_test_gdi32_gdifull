/* CreateScaledCompatibleBitmap, CreateDPIScaledDIBSection, CreateSessionMappedDIBSection,
 * CreateBitmapFromDxSurface(2), Get/Set/ClearBitmapAttributes */
#include "gap.h"

UDECL(HBITMAP, CreateScaledCompatibleBitmap, (HDC, int, int, WORD));
UDECL(HBITMAP, CreateDPIScaledDIBSection, (HDC, const BITMAPINFO *, UINT, void **, HANDLE, DWORD));
UDECL(HBITMAP, CreateSessionMappedDIBSection, (HDC, const BITMAPINFO *, UINT, HANDLE, DWORD));
UDECL(HBITMAP, CreateBitmapFromDxSurface, (HDC, UINT, UINT, UINT, HANDLE));
UDECL(HBITMAP, CreateBitmapFromDxSurface2, (HDC, UINT, UINT, UINT, HANDLE, UINT));
UDECL(DWORD, GetBitmapAttributes, (HBITMAP));
UDECL(DWORD, GetBrushAttributes, (HBRUSH));
UDECL(HBITMAP, SetBitmapAttributes, (HBITMAP, DWORD));
UDECL(HBITMAP, ClearBitmapAttributes, (HBITMAP, DWORD));

static void mkbmi(BITMAPINFO *bi, int w, int h, int bpp) {
    memset(bi, 0, sizeof *bi);
    bi->bmiHeader.biSize = sizeof bi->bmiHeader; bi->bmiHeader.biWidth = w; bi->bmiHeader.biHeight = h;
    bi->bmiHeader.biPlanes = 1; bi->bmiHeader.biBitCount = (WORD)bpp; bi->bmiHeader.biCompression = BI_RGB;
}
static void del(HGDIOBJ o) { if (o) U(DeleteObject)(o); }

/* ================= CreateDPIScaledDIBSection ================= */
TEST(gap0_dpidib_resolve) {
    CHECK(t_proc_gdi32("CreateDPIScaledDIBSection") != NULL);
    CHECK(t_proc_gdi32full("CreateDPIScaledDIBSection") != NULL);
}
TEST(gap0_dpidib_null_bmi) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f); REQUIRE(U(CreateDIBSection));
    void *bits = (void *)1;
    HBITMAP a = f(NULL, NULL, DIB_RGB_COLORS, &bits, NULL, 0);
    HBITMAP b = U(CreateDIBSection)(NULL, NULL, DIB_RGB_COLORS, &bits, NULL, 0);
    CHECK(a == NULL); CHECK(b == NULL);
    del(a); del(b);
}
TEST(gap0_dpidib_basic_32) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f);
    BITMAPINFO bi; mkbmi(&bi, 16, -16, 32);
    void *bits = NULL;
    HBITMAP h = f(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    CHECK(h != NULL); CHECK(bits != NULL);
    if (!h) return;
    EXPECT_EQ(U(GetObjectType)(h), OBJ_BITMAP);
    DIBSECTION ds; memset(&ds, 0, sizeof ds);
    EXPECT_EQ(U(GetObjectW)(h, sizeof ds, &ds), sizeof ds);
    EXPECT_EQ(ds.dsBm.bmWidth, 16); EXPECT_EQ(ds.dsBm.bmHeight, 16);
    EXPECT_EQ(ds.dsBm.bmBitsPixel, 32); EXPECT_EQ(ds.dsBm.bmPlanes, 1);
    EXPECT_EQ(ds.dsBm.bmWidthBytes, 64);
    EXPECT_EQ((ULONG_PTR)ds.dsBm.bmBits, (ULONG_PTR)bits);
    EXPECT_EQ(ds.dsBmih.biSizeImage, 16 * 16 * 4);
    EXPECT_EQ(ds.dsBmih.biCompression, BI_RGB);
    EXPECT_EQ(ds.dsBmih.biHeight, -16);
    del(h);
}
TEST(gap0_dpidib_bits_zeroed_and_writable) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f);
    BITMAPINFO bi; mkbmi(&bi, 8, -8, 32);
    DWORD *bits = NULL;
    HBITMAP h = f(NULL, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    CHECK(h && bits);
    if (!h || !bits) { del(h); return; }
    int nz = 0; for (int i = 0; i < 64; i++) if (bits[i]) nz++;
    EXPECT_EQ(nz, 0);
    for (int i = 0; i < 64; i++) bits[i] = 0x00112233 + i;
    gap_mdc m; memset(&m, 0, sizeof m);
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ old = U(SelectObject)(dc, h);
    CHECK(old != NULL);
    EXPECT_EQ(U(GetPixel)(dc, 0, 0), RGB(0x33, 0x22, 0x11));
    EXPECT_EQ(U(GetPixel)(dc, 7, 7), RGB((0x33 + 63) & 0xff, 0x22, 0x11));
    U(SetPixel)(dc, 3, 2, RGB(10, 20, 30));
    EXPECT_EQ(bits[2 * 8 + 3], 0x001E140A);
    U(SelectObject)(dc, old);
    U(DeleteDC)(dc);
    del(h);
}
TEST(gap0_dpidib_matches_createdibsection_depths) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f); REQUIRE(U(CreateDIBSection));
    int depths[] = { 1, 4, 8, 16, 24, 32 };
    for (unsigned i = 0; i < sizeof depths / sizeof *depths; i++) {
        struct { BITMAPINFOHEADER h; RGBQUAD pal[256]; } bi; memset(&bi, 0, sizeof bi);
        bi.h.biSize = sizeof bi.h; bi.h.biWidth = 13; bi.h.biHeight = 7; bi.h.biPlanes = 1; bi.h.biBitCount = (WORD)depths[i];
        void *b1 = NULL, *b2 = NULL;
        HBITMAP a = f(NULL, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &b1, NULL, 0);
        HBITMAP b = U(CreateDIBSection)(NULL, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &b2, NULL, 0);
        CHECKF((a != NULL) == (b != NULL), "depth %d", depths[i]);
        if (a && b) {
            DIBSECTION da, db; U(GetObjectW)(a, sizeof da, &da); U(GetObjectW)(b, sizeof db, &db);
            EXPECT_EQ(da.dsBm.bmWidthBytes, db.dsBm.bmWidthBytes);
            EXPECT_EQ(da.dsBm.bmBitsPixel, db.dsBm.bmBitsPixel);
            EXPECT_EQ(da.dsBmih.biSizeImage, db.dsBmih.biSizeImage);
            EXPECT_EQ(da.dsBmih.biClrUsed, db.dsBmih.biClrUsed);
            EXPECT_EQ(da.dsBmih.biHeight, db.dsBmih.biHeight);
            CHECKF(da.dsBm.bmWidthBytes == ((13 * depths[i] + 31) / 32) * 4, "stride depth %d", depths[i]);
        }
        del(a); del(b);
    }
}
TEST(gap0_dpidib_top_down_vs_bottom_up) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f);
    BITMAPINFO up, down; mkbmi(&up, 4, 4, 32); mkbmi(&down, 4, -4, 32);
    DWORD *bu = NULL, *bd = NULL;
    HBITMAP hu = f(NULL, &up, DIB_RGB_COLORS, (void **)&bu, NULL, 0);
    HBITMAP hd = f(NULL, &down, DIB_RGB_COLORS, (void **)&bd, NULL, 0);
    CHECK(hu && hd && bu && bd);
    if (hu && hd) {
        bu[0] = 0xAA; bd[0] = 0xAA;
        HDC dc = U(CreateCompatibleDC)(NULL);
        HGDIOBJ o = U(SelectObject)(dc, hu);
        COLORREF cu = U(GetPixel)(dc, 0, 3);   /* first dword of a bottom-up DIB is the bottom row */
        U(SelectObject)(dc, hd);
        COLORREF cd = U(GetPixel)(dc, 0, 0);   /* first dword of a top-down DIB is the top row */
        EXPECT_EQ(cu, RGB(0xAA, 0, 0)); EXPECT_EQ(cd, RGB(0xAA, 0, 0));
        U(SelectObject)(dc, o); U(DeleteDC)(dc);
    }
    del(hu); del(hd);
}
TEST(gap0_dpidib_rle_rejected) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f); REQUIRE(U(CreateDIBSection));
    BITMAPINFO bi; mkbmi(&bi, 8, 8, 8); bi.bmiHeader.biCompression = BI_RLE8;
    void *b = NULL;
    HBITMAP a = f(NULL, &bi, DIB_RGB_COLORS, &b, NULL, 0);
    HBITMAP c = U(CreateDIBSection)(NULL, &bi, DIB_RGB_COLORS, &b, NULL, 0);
    EXPECT_EQ((a != NULL), (c != NULL));
    del(a); del(c);
}
TEST(gap0_dpidib_bad_bpp) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f);
    int bpps[] = { 0, 2, 3, 5, 7, 9, 12, 15, 33, 64, 255 };
    for (unsigned i = 0; i < sizeof bpps / sizeof *bpps; i++) {
        BITMAPINFO bi; mkbmi(&bi, 8, 8, bpps[i]); void *b = NULL;
        HBITMAP h = f(NULL, &bi, DIB_RGB_COLORS, &b, NULL, 0);
        CHECKF(h == NULL, "bpp %d accepted", bpps[i]);
        del(h);
    }
}
TEST(gap0_dpidib_planes_not_one) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f); REQUIRE(U(CreateDIBSection));
    BITMAPINFO bi; mkbmi(&bi, 8, 8, 32); bi.bmiHeader.biPlanes = 2; void *b = NULL;
    HBITMAP a = f(NULL, &bi, DIB_RGB_COLORS, &b, NULL, 0), c = U(CreateDIBSection)(NULL, &bi, DIB_RGB_COLORS, &b, NULL, 0);
    EXPECT_EQ(a != NULL, c != NULL);
    del(a); del(c);
}
TEST(gap0_dpidib_zero_dimensions) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f); REQUIRE(U(CreateDIBSection));
    int dims[][2] = { { 0, 8 }, { 8, 0 }, { 0, 0 } };
    for (int i = 0; i < 3; i++) {
        BITMAPINFO bi; mkbmi(&bi, dims[i][0], dims[i][1], 32); void *b = NULL;
        HBITMAP a = f(NULL, &bi, DIB_RGB_COLORS, &b, NULL, 0), c = U(CreateDIBSection)(NULL, &bi, DIB_RGB_COLORS, &b, NULL, 0);
        CHECKF((a != NULL) == (c != NULL), "dims %dx%d", dims[i][0], dims[i][1]);
        t_note("%dx%d -> %p", dims[i][0], dims[i][1], a);
        del(a); del(c);
    }
}
TEST(gap0_dpidib_huge_fails) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f);
    int dims[][2] = { { 65536, 65536 }, { 0x7fffffff, 2 }, { 100000, 100000 }, { 1 << 24, 1 << 10 } };
    for (int i = 0; i < 4; i++) {
        BITMAPINFO bi; mkbmi(&bi, dims[i][0], dims[i][1], 32); void *b = (void *)8;
        HBITMAP h = f(NULL, &bi, DIB_RGB_COLORS, &b, NULL, 0);
        CHECKF(h == NULL, "%dx%d accepted", dims[i][0], dims[i][1]);
        del(h);
    }
}
TEST(gap0_dpidib_min_int_height) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f);
    BITMAPINFO bi; mkbmi(&bi, 8, (int)0x80000000, 32); void *b = NULL;
    HBITMAP h = f(NULL, &bi, DIB_RGB_COLORS, &b, NULL, 0);
    CHECK(h == NULL);
    del(h);
}
TEST(gap0_dpidib_bad_usage) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f); REQUIRE(U(CreateDIBSection));
    BITMAPINFO bi; mkbmi(&bi, 8, 8, 32);
    UINT us[] = { 3, 4, 99, 0xffffffff };
    for (unsigned i = 0; i < sizeof us / sizeof *us; i++) {
        void *b = NULL;
        HBITMAP a = f(NULL, &bi, us[i], &b, NULL, 0), c = U(CreateDIBSection)(NULL, &bi, us[i], &b, NULL, 0);
        CHECKF((a != NULL) == (c != NULL), "usage %u", us[i]);
        t_note("usage %u -> %p", us[i], a);
        del(a); del(c);
    }
}
TEST(gap0_dpidib_pal_colors_null_dc) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f); REQUIRE(U(CreateDIBSection));
    BITMAPINFO bi; mkbmi(&bi, 8, 8, 8); void *b = NULL;
    HBITMAP a = f(NULL, &bi, DIB_PAL_COLORS, &b, NULL, 0), c = U(CreateDIBSection)(NULL, &bi, DIB_PAL_COLORS, &b, NULL, 0);
    EXPECT_EQ(a != NULL, c != NULL);
    del(a); del(c);
}
TEST(gap0_dpidib_null_ppvbits) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f); REQUIRE(U(CreateDIBSection));
    BITMAPINFO bi; mkbmi(&bi, 8, 8, 32);
    HBITMAP a = f(NULL, &bi, DIB_RGB_COLORS, NULL, NULL, 0), c = U(CreateDIBSection)(NULL, &bi, DIB_RGB_COLORS, NULL, NULL, 0);
    EXPECT_EQ(a != NULL, c != NULL);
    t_note("NULL ppvBits: %p (CreateDIBSection %p)", a, c);
    del(a); del(c);
}
TEST(gap0_dpidib_bogus_dc) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f); REQUIRE(U(CreateDIBSection));
    BITMAPINFO bi; mkbmi(&bi, 8, 8, 32); void *b = NULL;
    HBITMAP a = f((HDC)0x1234, &bi, DIB_RGB_COLORS, &b, NULL, 0), c = U(CreateDIBSection)((HDC)0x1234, &bi, DIB_RGB_COLORS, &b, NULL, 0);
    EXPECT_EQ(a != NULL, c != NULL);
    del(a); del(c);
}
TEST(gap0_dpidib_with_memdc) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f);
    HDC dc = U(CreateCompatibleDC)(NULL);
    BITMAPINFO bi; mkbmi(&bi, 8, 8, 24); void *b = NULL;
    HBITMAP h = f(dc, &bi, DIB_RGB_COLORS, &b, NULL, 0);
    CHECK(h != NULL);
    if (h) { DIBSECTION ds; U(GetObjectW)(h, sizeof ds, &ds); EXPECT_EQ(ds.dsBm.bmBitsPixel, 24); EXPECT_EQ(ds.dsBm.bmWidthBytes, 24); }
    del(h); U(DeleteDC)(dc);
}
TEST(gap0_dpidib_file_mapping_section) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f);
    HANDLE sec = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 64 * 64 * 4 + 4096, NULL);
    CHECK(sec != NULL); if (!sec) return;
    BITMAPINFO bi; mkbmi(&bi, 64, -64, 32); DWORD *b = NULL;
    HBITMAP h = f(NULL, &bi, DIB_RGB_COLORS, (void **)&b, sec, 0);
    CHECK(h && b);
    if (h && b) {
        b[0] = 0xCAFEBABE;
        DWORD *view = MapViewOfFile(sec, FILE_MAP_READ, 0, 0, 4096);
        if (view) { EXPECT_EQ(view[0], 0xCAFEBABE); UnmapViewOfFile(view); }
        DIBSECTION ds; U(GetObjectW)(h, sizeof ds, &ds);
        EXPECT_EQ((ULONG_PTR)ds.dshSection, (ULONG_PTR)sec);
        EXPECT_EQ(ds.dsOffset, 0);
    }
    del(h);
    CloseHandle(sec);
}
TEST(gap0_dpidib_section_offset_unaligned) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f); REQUIRE(U(CreateDIBSection));
    HANDLE sec = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 64 * 1024, NULL);
    if (!sec) { t_skip("mapping"); return; }
    BITMAPINFO bi; mkbmi(&bi, 4, -4, 32); void *b = NULL;
    HBITMAP a = f(NULL, &bi, DIB_RGB_COLORS, &b, sec, 3);
    HBITMAP c = U(CreateDIBSection)(NULL, &bi, DIB_RGB_COLORS, &b, sec, 3);
    CHECK(a == NULL); CHECK(c == NULL);
    del(a); del(c);
    HBITMAP d = f(NULL, &bi, DIB_RGB_COLORS, &b, sec, 4);
    CHECK(d != NULL);
    del(d); CloseHandle(sec);
}
TEST(gap0_dpidib_section_too_small) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f);
    HANDLE sec = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, NULL);
    if (!sec) { t_skip("mapping"); return; }
    BITMAPINFO bi; mkbmi(&bi, 256, -256, 32); void *b = NULL;
    HBITMAP h = f(NULL, &bi, DIB_RGB_COLORS, &b, sec, 0);
    CHECK(h == NULL);
    del(h); CloseHandle(sec);
}
TEST(gap0_dpidib_bogus_section) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f);
    BITMAPINFO bi; mkbmi(&bi, 8, 8, 32); void *b = NULL;
    HBITMAP h = f(NULL, &bi, DIB_RGB_COLORS, &b, (HANDLE)0x7654, 0);
    CHECK(h == NULL);
    del(h);
}
TEST(gap0_dpidib_core_header) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f);
    BITMAPCOREINFO ci; memset(&ci, 0, sizeof ci);
    ci.bmciHeader.bcSize = sizeof ci.bmciHeader; ci.bmciHeader.bcWidth = 9; ci.bmciHeader.bcHeight = 5;
    ci.bmciHeader.bcPlanes = 1; ci.bmciHeader.bcBitCount = 24;
    void *b = NULL;
    HBITMAP h = f(NULL, (const BITMAPINFO *)&ci, DIB_RGB_COLORS, &b, NULL, 0);
    CHECK(h != NULL);
    if (h) { BITMAP bm; U(GetObjectW)(h, sizeof bm, &bm); EXPECT_EQ(bm.bmWidth, 9); EXPECT_EQ(bm.bmHeight, 5); EXPECT_EQ(bm.bmWidthBytes, 28); }
    del(h);
}
TEST(gap0_dpidib_bad_header_size) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f);
    DWORD sizes[] = { 0, 4, 8, 11, 13, 39, 41, 0x100, 0x7fffffff };
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        BITMAPINFO bi; mkbmi(&bi, 8, 8, 32); bi.bmiHeader.biSize = sizes[i]; void *b = NULL;
        HBITMAP h = f(NULL, &bi, DIB_RGB_COLORS, &b, NULL, 0);
        CHECKF(h == NULL, "biSize %lu accepted", sizes[i]);
        del(h);
    }
}
TEST(gap0_dpidib_v4_v5_headers) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f);
    BITMAPV5HEADER v5; memset(&v5, 0, sizeof v5);
    v5.bV5Size = sizeof v5; v5.bV5Width = 5; v5.bV5Height = -5; v5.bV5Planes = 1; v5.bV5BitCount = 32;
    v5.bV5Compression = BI_BITFIELDS; v5.bV5RedMask = 0xff0000; v5.bV5GreenMask = 0xff00; v5.bV5BlueMask = 0xff; v5.bV5AlphaMask = 0xff000000;
    v5.bV5CSType = 0x73524742;
    void *b = NULL;
    HBITMAP h = f(NULL, (BITMAPINFO *)&v5, DIB_RGB_COLORS, &b, NULL, 0);
    CHECK(h != NULL);
    if (h) { DIBSECTION ds; U(GetObjectW)(h, sizeof ds, &ds); EXPECT_EQ(ds.dsBmih.biCompression, BI_BITFIELDS); EXPECT_EQ(ds.dsBitfields[0], 0xff0000); }
    del(h);
}
TEST(gap0_dpidib_bitfields_16) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f);
    struct { BITMAPINFOHEADER h; DWORD m[3]; } bi; memset(&bi, 0, sizeof bi);
    bi.h.biSize = sizeof bi.h; bi.h.biWidth = 4; bi.h.biHeight = -4; bi.h.biPlanes = 1; bi.h.biBitCount = 16; bi.h.biCompression = BI_BITFIELDS;
    bi.m[0] = 0xF800; bi.m[1] = 0x07E0; bi.m[2] = 0x001F;
    WORD *b = NULL;
    HBITMAP h = f(NULL, (BITMAPINFO *)&bi, DIB_RGB_COLORS, (void **)&b, NULL, 0);
    CHECK(h && b);
    if (h && b) {
        b[0] = 0xF800;
        HDC dc = U(CreateCompatibleDC)(NULL); HGDIOBJ o = U(SelectObject)(dc, h);
        EXPECT_EQ(U(GetPixel)(dc, 0, 0), RGB(255, 0, 0));
        b[1] = 0x07E0; EXPECT_EQ(U(GetPixel)(dc, 1, 0), RGB(0, 255, 0));
        b[2] = 0x001F; EXPECT_EQ(U(GetPixel)(dc, 2, 0), RGB(0, 0, 255));
        U(SelectObject)(dc, o); U(DeleteDC)(dc);
    }
    del(h);
}
TEST(gap0_dpidib_8bpp_palette) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f);
    struct { BITMAPINFOHEADER h; RGBQUAD pal[256]; } bi; memset(&bi, 0, sizeof bi);
    bi.h.biSize = sizeof bi.h; bi.h.biWidth = 4; bi.h.biHeight = -4; bi.h.biPlanes = 1; bi.h.biBitCount = 8;
    bi.pal[1].rgbRed = 200; bi.pal[1].rgbGreen = 100; bi.pal[1].rgbBlue = 50;
    BYTE *b = NULL;
    HBITMAP h = f(NULL, (BITMAPINFO *)&bi, DIB_RGB_COLORS, (void **)&b, NULL, 0);
    CHECK(h && b);
    if (h && b) {
        b[0] = 1;
        HDC dc = U(CreateCompatibleDC)(NULL); HGDIOBJ o = U(SelectObject)(dc, h);
        RGBQUAD got[2]; memset(got, 0, sizeof got);
        EXPECT_EQ(U(GetDIBColorTable)(dc, 0, 2, got), 2);
        EXPECT_EQ(got[1].rgbRed, 200); EXPECT_EQ(got[1].rgbGreen, 100); EXPECT_EQ(got[1].rgbBlue, 50);
        EXPECT_EQ(U(GetPixel)(dc, 0, 0), RGB(200, 100, 50));
        U(SelectObject)(dc, o); U(DeleteDC)(dc);
    }
    del(h);
}
TEST(gap0_dpidib_no_leak_loop) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f);
    DWORD before = gap_gdi_count();
    BITMAPINFO bi; mkbmi(&bi, 32, -32, 32);
    for (int i = 0; i < 200; i++) { void *b; HBITMAP h = f(NULL, &bi, DIB_RGB_COLORS, &b, NULL, 0); del(h); }
    EXPECT_EQ(gap_gdi_count(), before);
}
TEST(gap0_dpidib_object_count_bounded) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f);
    HBITMAP hs[100]; BITMAPINFO bi; mkbmi(&bi, 8, -8, 32);
    DWORD before = gap_gdi_count();
    for (int i = 0; i < 100; i++) { void *b; hs[i] = f(NULL, &bi, DIB_RGB_COLORS, &b, NULL, 0); }
    DWORD mid = gap_gdi_count();
    for (int i = 0; i < 100; i++) del(hs[i]);
    EXPECT_EQ(mid - before, 100);
    EXPECT_EQ(gap_gdi_count(), before);
}
TEST(gap0_dpidib_delete_selected_fails) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f);
    BITMAPINFO bi; mkbmi(&bi, 8, -8, 32); void *b;
    HBITMAP h = f(NULL, &bi, DIB_RGB_COLORS, &b, NULL, 0);
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ o = U(SelectObject)(dc, h);
    CHECK(!U(DeleteObject)(h));
    CHECK(U(GetObjectType)(h) == OBJ_BITMAP);
    U(SelectObject)(dc, o);
    CHECK(U(DeleteObject)(h));
    EXPECT_EQ(U(GetObjectType)(h), 0);
    U(DeleteDC)(dc);
}
TEST(gap0_dpidib_not_stock_bit) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f);
    BITMAPINFO bi; mkbmi(&bi, 8, -8, 32); void *b;
    HBITMAP h = f(NULL, &bi, DIB_RGB_COLORS, &b, NULL, 0);
    CHECK(h != NULL);
    CHECK(((ULONG_PTR)h & 0x800000) == 0);
    del(h);
}
typedef struct { HBITMAP h; DWORD type; DWORD ok; } dpi_thr;
static DWORD WINAPI dpi_thread(LPVOID p) {
    dpi_thr *t = p;
    t->type = ((int (WINAPI *)(HGDIOBJ))t_proc("GetObjectType"))(t->h);
    BITMAP bm; t->ok = ((int (WINAPI *)(HANDLE, int, LPVOID))t_proc("GetObjectW"))(t->h, sizeof bm, &bm) == sizeof bm && bm.bmWidth == 8;
    return 0;
}
TEST(gap0_dpidib_cross_thread_query) {
    CreateDPIScaledDIBSection_pfn f = UN(CreateDPIScaledDIBSection); REQUIRE(f);
    BITMAPINFO bi; mkbmi(&bi, 8, -8, 32); void *b;
    HBITMAP h = f(NULL, &bi, DIB_RGB_COLORS, &b, NULL, 0);
    dpi_thr t = { h, 0, 0 };
    HANDLE th = CreateThread(NULL, 0, dpi_thread, &t, 0, NULL);
    WaitForSingleObject(th, 10000); CloseHandle(th);
    EXPECT_EQ(t.type, OBJ_BITMAP); EXPECT_EQ(t.ok, 1);
    del(h);
}

/* ================= CreateSessionMappedDIBSection ================= */
TEST(gap0_smdib_resolve) {
    CHECK(t_proc_gdi32("CreateSessionMappedDIBSection") != NULL);
    CHECK(t_proc_gdi32full("CreateSessionMappedDIBSection") != NULL);
}
TEST(gap0_smdib_null_bmi) {
    CreateSessionMappedDIBSection_pfn f = UN(CreateSessionMappedDIBSection); REQUIRE(f);
    HBITMAP h = f(NULL, NULL, DIB_RGB_COLORS, NULL, 0);
    CHECK(h == NULL);
    del(h);
}
TEST(gap0_smdib_null_bmi_with_section) {
    CreateSessionMappedDIBSection_pfn f = UN(CreateSessionMappedDIBSection); REQUIRE(f);
    HANDLE sec = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 8192, NULL);
    if (!sec) { t_skip("mapping"); return; }
    HBITMAP h = f(NULL, NULL, DIB_RGB_COLORS, sec, 0);
    CHECK(h == NULL);
    del(h); CloseHandle(sec);
}
TEST(gap0_smdib_bad_bpp) {
    CreateSessionMappedDIBSection_pfn f = UN(CreateSessionMappedDIBSection); REQUIRE(f);
    int bpps[] = { 0, 3, 7, 12, 33, 255 };
    for (unsigned i = 0; i < sizeof bpps / sizeof *bpps; i++) {
        BITMAPINFO bi; mkbmi(&bi, 8, 8, bpps[i]);
        HBITMAP h = f(NULL, &bi, DIB_RGB_COLORS, NULL, 0);
        CHECKF(h == NULL, "bpp %d", bpps[i]);
        del(h);
    }
}
TEST(gap0_smdib_bad_header_size) {
    CreateSessionMappedDIBSection_pfn f = UN(CreateSessionMappedDIBSection); REQUIRE(f);
    DWORD sizes[] = { 0, 4, 13, 39, 0x100 };
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        BITMAPINFO bi; mkbmi(&bi, 8, 8, 32); bi.bmiHeader.biSize = sizes[i];
        HBITMAP h = f(NULL, &bi, DIB_RGB_COLORS, NULL, 0);
        CHECKF(h == NULL, "biSize %lu", sizes[i]);
        del(h);
    }
}
TEST(gap0_smdib_huge) {
    CreateSessionMappedDIBSection_pfn f = UN(CreateSessionMappedDIBSection); REQUIRE(f);
    BITMAPINFO bi; mkbmi(&bi, 65536, 65536, 32);
    HBITMAP h = f(NULL, &bi, DIB_RGB_COLORS, NULL, 0);
    CHECK(h == NULL);
    del(h);
}
TEST(gap0_smdib_valid_observed) {
    CreateSessionMappedDIBSection_pfn f = UN(CreateSessionMappedDIBSection); REQUIRE(f);
    BITMAPINFO bi; mkbmi(&bi, 8, -8, 32);
    DWORD before = gap_gdi_count();
    SET_ERR(0);
    HBITMAP h = f(NULL, &bi, DIB_RGB_COLORS, NULL, 0);
    DWORD e = GetLastError();
    t_note("session-mapped DIB (no section): %p err %lu", h, e);
    if (h) {
        EXPECT_EQ(U(GetObjectType)(h), OBJ_BITMAP);
        BITMAP bm; EXPECT_EQ(U(GetObjectW)(h, sizeof bm, &bm), sizeof bm);
        EXPECT_EQ(bm.bmWidth, 8); EXPECT_EQ(bm.bmHeight, 8); EXPECT_EQ(bm.bmBitsPixel, 32);
        del(h);
    }
    EXPECT_EQ(gap_gdi_count(), before);
}
TEST(gap0_smdib_valid_with_section_observed) {
    CreateSessionMappedDIBSection_pfn f = UN(CreateSessionMappedDIBSection); REQUIRE(f);
    HANDLE sec = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 8192, NULL);
    if (!sec) { t_skip("mapping"); return; }
    BITMAPINFO bi; mkbmi(&bi, 16, -16, 32);
    HBITMAP h = f(NULL, &bi, DIB_RGB_COLORS, sec, 0);
    t_note("session-mapped DIB (section): %p", h);
    if (h) { EXPECT_EQ(U(GetObjectType)(h), OBJ_BITMAP); del(h); }
    CloseHandle(sec);
}
TEST(gap0_smdib_bogus_section) {
    CreateSessionMappedDIBSection_pfn f = UN(CreateSessionMappedDIBSection); REQUIRE(f);
    BITMAPINFO bi; mkbmi(&bi, 8, 8, 32);
    HBITMAP h = f(NULL, &bi, DIB_RGB_COLORS, (HANDLE)0x7654, 0);
    CHECK(h == NULL);
    del(h);
}
TEST(gap0_smdib_no_leak) {
    CreateSessionMappedDIBSection_pfn f = UN(CreateSessionMappedDIBSection); REQUIRE(f);
    BITMAPINFO bi; mkbmi(&bi, 8, -8, 32);
    DWORD before = gap_gdi_count();
    for (int i = 0; i < 50; i++) del(f(NULL, &bi, DIB_RGB_COLORS, NULL, 0));
    EXPECT_EQ(gap_gdi_count(), before);
}

/* ================= CreateScaledCompatibleBitmap ================= */
TEST(gap0_scb_resolve) {
    CHECK(t_proc_gdi32("CreateScaledCompatibleBitmap") != NULL);
    CHECK(t_proc_gdi32full("CreateScaledCompatibleBitmap") != NULL);
}
/* The wrapper issues a named driver escape ("GSESC") and hands back a field of its own request block,
 * so only error-ish inputs are exercised and results are classified rather than asserted. */
static void scb_cleanup(HGDIOBJ r) {
    if (r && U(GetObjectType)(r) == OBJ_BITMAP) U(DeleteObject)(r);
}
TEST(gap0_scb_null_dc) {
    CreateScaledCompatibleBitmap_pfn f = UN(CreateScaledCompatibleBitmap); REQUIRE(f);
    SKIP_WINE("unimplemented");
    DWORD before = gap_gdi_count();
    HBITMAP r = f(NULL, 16, 16, 0);
    t_note("NULL dc: %p type %d", r, r ? U(GetObjectType)(r) : -1);
    SOFT(r == NULL, "no bitmap for a NULL DC");
    scb_cleanup(r);
    EXPECT_EQ(gap_gdi_count(), before);
}
TEST(gap0_scb_bogus_dc) {
    CreateScaledCompatibleBitmap_pfn f = UN(CreateScaledCompatibleBitmap); REQUIRE(f);
    SKIP_WINE("unimplemented");
    HBITMAP r = f((HDC)0x1234, 16, 16, 0);
    t_note("bogus dc: %p", r);
    HBITMAP r2 = f((HDC)0xdeadbeef, 0, 0, 0);
    t_note("bogus dc 0x0: %p", r2);
    scb_cleanup(r); scb_cleanup(r2);
}
TEST(gap0_scb_memdc_observed) {
    CreateScaledCompatibleBitmap_pfn f = UN(CreateScaledCompatibleBitmap); REQUIRE(f);
    SKIP_WINE("unimplemented");
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    DWORD before = gap_gdi_count();
    HBITMAP r = f(m.dc, 16, 8, 0);
    t_note("memdc: %p (is dc %d)", r, r == (HBITMAP)m.dc);
    if (r && r != (HBITMAP)m.dc && U(GetObjectType)(r) == OBJ_BITMAP) {
        BITMAP bm; U(GetObjectW)(r, sizeof bm, &bm);
        t_note("scaled bitmap %ldx%ld bpp %u", bm.bmWidth, bm.bmHeight, bm.bmBitsPixel);
        U(DeleteObject)(r);
    }
    SOFT(gap_gdi_count() == before, "no objects leaked");
    gap_mdc_close(&m);
}
TEST(gap0_scb_negative_sizes) {
    CreateScaledCompatibleBitmap_pfn f = UN(CreateScaledCompatibleBitmap); REQUIRE(f);
    SKIP_WINE("unimplemented");
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    int d[][2] = { { -1, 8 }, { 8, -1 }, { 0, 0 }, { 0x7fffffff, 0x7fffffff } };
    for (int i = 0; i < 4; i++) {
        HBITMAP r = f(m.dc, d[i][0], d[i][1], 0);
        t_note("%d x %d -> %p", d[i][0], d[i][1], r);
        scb_cleanup(r);
    }
    gap_mdc_close(&m);
}

/* ================= CreateBitmapFromDxSurface / 2 ================= */
TEST(gap0_dxs_resolve) {
    CHECK(t_proc_gdi32("CreateBitmapFromDxSurface") != NULL);
    CHECK(t_proc_gdi32("CreateBitmapFromDxSurface2") != NULL);
    CHECK(t_proc_gdi32full("CreateBitmapFromDxSurface") == NULL);
    CHECK(t_proc_gdi32full("CreateBitmapFromDxSurface2") == NULL);
}
TEST(gap0_dxs_zero_dims) {
    CreateBitmapFromDxSurface_pfn f = UN(CreateBitmapFromDxSurface); REQUIRE(f);
    SKIP_WINE("unimplemented");
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    UINT d[][2] = { { 0, 16 }, { 16, 0 }, { 0, 0 } };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        HBITMAP h = f(m.dc, d[i][0], d[i][1], 0, NULL);
        DWORD e = GetLastError();
        CHECKF(h == NULL, "%ux%u", d[i][0], d[i][1]);
        CHECKF(e == ERROR_INVALID_PARAMETER, "%ux%u err %lu", d[i][0], d[i][1], e);
        del(h);
    }
    gap_mdc_close(&m);
}
TEST(gap0_dxs_null_dc) {
    CreateBitmapFromDxSurface_pfn f = UN(CreateBitmapFromDxSurface); REQUIRE(f);
    SKIP_WINE("unimplemented");
    SET_ERR(0);
    HBITMAP h = f(NULL, 16, 16, 0, NULL);
    CHECK(h == NULL);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    h = f(NULL, 16, 16, 87, (HANDLE)0x1234);
    CHECK(h == NULL);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
}
TEST(gap0_dxs_dims_overflow) {
    CreateBitmapFromDxSurface_pfn f = UN(CreateBitmapFromDxSurface); REQUIRE(f);
    SKIP_WINE("unimplemented");
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    UINT d[][2] = { { 0x10000, 0x10000 }, { 0xffffffff, 2 }, { 0xffffffff, 0xffffffff }, { 0x80000000, 2 } };
    for (int i = 0; i < 4; i++) {
        SET_ERR(0);
        HBITMAP h = f(m.dc, d[i][0], d[i][1], 0, NULL);
        CHECKF(h == NULL, "%ux%u", d[i][0], d[i][1]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "%ux%u err %lu", d[i][0], d[i][1], GetLastError());
        del(h);
    }
    gap_mdc_close(&m);
}
TEST(gap0_dxs_bogus_dc_dims_checked_first) {
    CreateBitmapFromDxSurface_pfn f = UN(CreateBitmapFromDxSurface); REQUIRE(f);
    SKIP_WINE("unimplemented");
    SET_ERR(0);
    CHECK(f((HDC)0x1234, 0, 0, 0, NULL) == NULL);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
}
TEST(gap0_dxs_bogus_dc_valid_dims) {
    CreateBitmapFromDxSurface_pfn f = UN(CreateBitmapFromDxSurface); REQUIRE(f);
    SKIP_WINE("unimplemented");
    HBITMAP h = f((HDC)0x1234, 16, 16, 0, NULL);
    CHECK(h == NULL);
    h = f((HDC)0xdeadbeef, 16, 16, 0, (HANDLE)0x1234);
    CHECK(h == NULL);
}
TEST(gap0_dxs_deleted_dc) {
    CreateBitmapFromDxSurface_pfn f = UN(CreateBitmapFromDxSurface); REQUIRE(f);
    SKIP_WINE("unimplemented");
    HDC dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    CHECK(f(dc, 16, 16, 0, NULL) == NULL);
}
TEST(gap0_dxs_bogus_surface_handle) {
    CreateBitmapFromDxSurface_pfn f = UN(CreateBitmapFromDxSurface); REQUIRE(f);
    SKIP_WINE("unimplemented");
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    DWORD before = gap_gdi_count();
    HBITMAP h = f(m.dc, 16, 16, 0, (HANDLE)0x7654);
    t_note("memdc + bogus shared surface handle: %p err %lu", h, GetLastError());
    SOFT(h == NULL, "a bogus shared surface handle does not yield a bitmap");
    if (h) del(h);
    EXPECT_EQ(gap_gdi_count(), before);
    gap_mdc_close(&m);
}
TEST(gap0_dxs_non_dc_object) {
    CreateBitmapFromDxSurface_pfn f = UN(CreateBitmapFromDxSurface); REQUIRE(f);
    SKIP_WINE("unimplemented");
    HBRUSH br = U(CreateSolidBrush)(0);
    CHECK(f((HDC)br, 16, 16, 0, NULL) == NULL);
    del(br);
}
TEST(gap0_dxs2_zero_dims) {
    CreateBitmapFromDxSurface2_pfn f = UN(CreateBitmapFromDxSurface2); REQUIRE(f);
    SKIP_WINE("unimplemented");
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    UINT d[][2] = { { 0, 16 }, { 16, 0 }, { 0, 0 } };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        HBITMAP h = f(m.dc, d[i][0], d[i][1], 0, NULL, 0);
        CHECKF(h == NULL, "%ux%u", d[i][0], d[i][1]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "%ux%u err %lu", d[i][0], d[i][1], GetLastError());
        del(h);
    }
    gap_mdc_close(&m);
}
TEST(gap0_dxs2_null_dc) {
    CreateBitmapFromDxSurface2_pfn f = UN(CreateBitmapFromDxSurface2); REQUIRE(f);
    SKIP_WINE("unimplemented");
    SET_ERR(0);
    CHECK(f(NULL, 16, 16, 0, NULL, 0) == NULL);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
}
TEST(gap0_dxs2_dims_overflow) {
    CreateBitmapFromDxSurface2_pfn f = UN(CreateBitmapFromDxSurface2); REQUIRE(f);
    SKIP_WINE("unimplemented");
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    SET_ERR(0);
    CHECK(f(m.dc, 0x10000, 0x10000, 0, NULL, 0) == NULL);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(f(m.dc, 0xffffffff, 0xffffffff, 0, NULL, 0) == NULL);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    gap_mdc_close(&m);
}
TEST(gap0_dxs2_bogus_dc) {
    CreateBitmapFromDxSurface2_pfn f = UN(CreateBitmapFromDxSurface2); REQUIRE(f);
    SKIP_WINE("unimplemented");
    CHECK(f((HDC)0x1234, 16, 16, 0, NULL, 0) == NULL);
    CHECK(f((HDC)0xdeadbeef, 16, 16, 0, (HANDLE)0x1234, 0) == NULL);
}
TEST(gap0_dxs_a_b_parity_errors) {
    CreateBitmapFromDxSurface_pfn f1 = UN(CreateBitmapFromDxSurface); CreateBitmapFromDxSurface2_pfn f2 = UN(CreateBitmapFromDxSurface2); REQUIRE(f1); REQUIRE(f2);
    SKIP_WINE("unimplemented");
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    UINT d[][2] = { { 0, 1 }, { 1, 0 }, { 0x20000, 0x20000 } };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0); HBITMAP a = f1(m.dc, d[i][0], d[i][1], 0, NULL); DWORD ea = GetLastError();
        SET_ERR(0); HBITMAP b = f2(m.dc, d[i][0], d[i][1], 0, NULL, 0); DWORD eb = GetLastError();
        CHECK(a == NULL && b == NULL);
        EXPECT_EQ(ea, eb);
    }
    gap_mdc_close(&m);
}

/* ================= Get/Set/ClearBitmapAttributes ================= */
TEST(gap0_battr_resolve) {
    CHECK(t_proc_gdi32("GetBitmapAttributes") != NULL);
    CHECK(t_proc_gdi32("SetBitmapAttributes") != NULL);
    CHECK(t_proc_gdi32("ClearBitmapAttributes") != NULL);
    CHECK(t_proc_gdi32full("GetBitmapAttributes") != NULL);
    CHECK(t_proc_gdi32full("SetBitmapAttributes") != NULL);
    CHECK(t_proc_gdi32full("ClearBitmapAttributes") != NULL);
    CHECK(t_proc_gdi32full("GetBrushAttributes") != NULL);
}
TEST(gap0_battr_get_is_bit23) {
    GetBitmapAttributes_pfn f = UN(GetBitmapAttributes); REQUIRE(f);
    for (int bit = 0; bit < 64; bit++) {
        ULONG_PTR v = (ULONG_PTR)1 << bit;
        DWORD r = f((HBITMAP)v);
        CHECKF(r == (bit == 23 ? 1u : 0u), "bit %d -> %lu", bit, r);
    }
}
TEST(gap0_battr_get_values) {
    GetBitmapAttributes_pfn f = UN(GetBitmapAttributes); REQUIRE(f);
    EXPECT_EQ(f(NULL), 0);
    EXPECT_EQ(f((HBITMAP)0x800000), 1);
    EXPECT_EQ(f((HBITMAP)0x7FFFFF), 0);
    EXPECT_EQ(f((HBITMAP)0x1000000), 0);
    EXPECT_EQ(f((HBITMAP)0xFFFFFFFF), 1);
    EXPECT_EQ(f((HBITMAP)(ULONG_PTR)-1), 1);
    EXPECT_EQ(f((HBITMAP)0xFF7FFFFF), 0);
    EXPECT_EQ(f((HBITMAP)0x01900010), 1);
    EXPECT_EQ(f((HBITMAP)0x0100FFFF), 0);
    EXPECT_EQ(f((HBITMAP)0xFFFFFFFF7FFFFFFFull), 1);
}
TEST(gap0_battr_get_upper_bits_ignored) {
    GetBitmapAttributes_pfn f = UN(GetBitmapAttributes); REQUIRE(f);
    for (int i = 32; i < 64; i++) {
        ULONG_PTR v = ((ULONG_PTR)1 << i);
        EXPECT_EQ(f((HBITMAP)(v | 0x800000)), 1);
        EXPECT_EQ(f((HBITMAP)v), 0);
    }
}
TEST(gap0_battr_get_returns_only_0_or_1) {
    GetBitmapAttributes_pfn f = UN(GetBitmapAttributes); REQUIRE(f);
    ULONG_PTR s = 0x9E3779B97F4A7C15ull; int bad = 0;
    for (int i = 0; i < 5000; i++) {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        DWORD r = f((HBITMAP)s);
        if (r > 1 || r != (DWORD)((s >> 23) & 1)) bad++;
    }
    EXPECT_EQ(bad, 0);
}
TEST(gap0_battr_get_no_lasterror) {
    GetBitmapAttributes_pfn f = UN(GetBitmapAttributes); REQUIRE(f);
    SET_ERR(0x4242);
    f(NULL); f((HBITMAP)0xdeadbeef);
    EXPECT_EQ(GetLastError(), 0x4242);
}
TEST(gap0_battr_get_normal_bitmaps_not_stock) {
    GetBitmapAttributes_pfn f = UN(GetBitmapAttributes); REQUIRE(f);
    HBITMAP a = U(CreateBitmap)(8, 8, 1, 1, NULL);
    HBITMAP b = U(CreateBitmap)(8, 8, 1, 32, NULL);
    HDC dc = U(CreateCompatibleDC)(NULL);
    HBITMAP c = U(CreateCompatibleBitmap)(dc, 4, 4);
    BITMAPINFO bi; mkbmi(&bi, 4, -4, 32); void *bits;
    HBITMAP d = U(CreateDIBSection)(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    HBITMAP e = U(CreateDIBitmap)(dc, &bi.bmiHeader, 0, NULL, NULL, 0);
    HBITMAP all[] = { a, b, c, d, e };
    for (int i = 0; i < 5; i++) { CHECKF(all[i] != NULL, "bitmap %d", i); if (all[i]) EXPECT_EQ(f(all[i]), 0); }
    for (int i = 0; i < 5; i++) del(all[i]);
    U(DeleteDC)(dc);
}
TEST(gap0_battr_get_default_bitmap_is_stock) {
    GetBitmapAttributes_pfn f = UN(GetBitmapAttributes); REQUIRE(f);
    HDC dc = U(CreateCompatibleDC)(NULL);
    HBITMAP cur = U(GetCurrentObject)(dc, OBJ_BITMAP);
    CHECK(cur != NULL);
    EXPECT_EQ(U(GetObjectType)(cur), OBJ_BITMAP);
    EXPECT_EQ(f(cur), 1);
    HBITMAP mine = U(CreateBitmap)(4, 4, 1, 1, NULL);
    HGDIOBJ old = U(SelectObject)(dc, mine);
    EXPECT_EQ(old, cur);
    EXPECT_EQ(f(old), 1);
    EXPECT_EQ(f(mine), 0);
    U(SelectObject)(dc, old);
    del(mine);
    U(DeleteDC)(dc);
}
TEST(gap0_battr_get_stock_objects_all_have_bit) {
    GetBitmapAttributes_pfn f = UN(GetBitmapAttributes); REQUIRE(f);
    int stock[] = { WHITE_BRUSH, LTGRAY_BRUSH, GRAY_BRUSH, DKGRAY_BRUSH, BLACK_BRUSH, NULL_BRUSH, WHITE_PEN, BLACK_PEN, NULL_PEN,
                    OEM_FIXED_FONT, ANSI_FIXED_FONT, ANSI_VAR_FONT, SYSTEM_FONT, DEVICE_DEFAULT_FONT, DEFAULT_PALETTE, SYSTEM_FIXED_FONT, DEFAULT_GUI_FONT, DC_BRUSH, DC_PEN };
    for (unsigned i = 0; i < sizeof stock / sizeof *stock; i++) {
        HGDIOBJ o = U(GetStockObject)(stock[i]);
        CHECKF(o != NULL, "stock %d", stock[i]);
        if (o) CHECKF(f((HBITMAP)o) == 1, "stock %d handle %p", stock[i], o);
    }
}
TEST(gap0_battr_get_created_objects_no_stock_bit) {
    GetBitmapAttributes_pfn f = UN(GetBitmapAttributes); REQUIRE(f);
    HGDIOBJ o[] = { U(CreatePen)(PS_SOLID, 1, 0), U(CreateSolidBrush)(0x123456), U(CreateRectRgn)(0, 0, 4, 4),
                    U(CreateFontW)(-12, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, L"Arial"), U(CreateHatchBrush)(HS_CROSS, 0) };
    for (unsigned i = 0; i < 5; i++) { CHECK(o[i] != NULL); if (o[i]) EXPECT_EQ(f((HBITMAP)o[i]), 0); }
    for (unsigned i = 0; i < 5; i++) del(o[i]);
}
TEST(gap0_brushattr_alias) {
    GetBrushAttributes_pfn g = UN(GetBrushAttributes); GetBitmapAttributes_pfn f = UN(GetBitmapAttributes); REQUIRE(g); REQUIRE(f);
    ULONG_PTR v[] = { 0, 0x800000, 0x1234, 0x01900010, 0xFFFFFFFF, 0x7FFFFF };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) EXPECT_EQ(g((HBRUSH)v[i]), f((HBITMAP)v[i]));
    EXPECT_EQ(g((HBRUSH)U(GetStockObject)(WHITE_BRUSH)), 1);
    HBRUSH b = U(CreateSolidBrush)(0xFF00FF);
    EXPECT_EQ(g(b), 0);
    del(b);
}
TEST(gap0_battr_set_invalid_flags_return0) {
    SetBitmapAttributes_pfn s = UN(SetBitmapAttributes); REQUIRE(s);
    HBITMAP h = U(CreateBitmap)(8, 8, 1, 1, NULL);
    DWORD bad[] = { 2, 3, 4, 0x80, 0x100, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0x6677);
        HBITMAP r = s(h, bad[i]);
        CHECKF(r == NULL, "flags 0x%lx", bad[i]);
        CHECKF(GetLastError() == 0x6677, "flags 0x%lx touched last error", bad[i]);
    }
    EXPECT_EQ(U(GetObjectType)(h), OBJ_BITMAP);
    del(h);
}
TEST(gap0_battr_set_zero_flags) {
    SetBitmapAttributes_pfn s = UN(SetBitmapAttributes); REQUIRE(s);
    HBITMAP h = U(CreateBitmap)(8, 8, 1, 1, NULL);
    CHECK(s(h, 0) == NULL);
    EXPECT_EQ(UN(GetBitmapAttributes)(h), 0);
    del(h);
}
TEST(gap0_battr_set_invalid_handles) {
    SetBitmapAttributes_pfn s = UN(SetBitmapAttributes); REQUIRE(s);
    ULONG_PTR v[] = { 0, 0x1234, 0xdeadbeef, 0x800000, 0x01900010, 0xFFFFFFFF, 0x10000 };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) CHECKF(s((HBITMAP)v[i], 1) == NULL, "handle %llx", (unsigned long long)v[i]);
}
TEST(gap0_battr_set_wrong_object_type) {
    SetBitmapAttributes_pfn s = UN(SetBitmapAttributes); REQUIRE(s);
    HBRUSH b = U(CreateSolidBrush)(0); HPEN p = U(CreatePen)(PS_SOLID, 1, 0); HRGN r = U(CreateRectRgn)(0, 0, 2, 2);
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(s((HBITMAP)b, 1) == NULL);
    CHECK(s((HBITMAP)p, 1) == NULL);
    CHECK(s((HBITMAP)r, 1) == NULL);
    CHECK(s((HBITMAP)dc, 1) == NULL);
    CHECK(s((HBITMAP)U(GetStockObject)(WHITE_BRUSH), 1) == NULL);
    EXPECT_EQ(U(GetObjectType)(b), OBJ_BRUSH);
    del(b); del(p); del(r); U(DeleteDC)(dc);
}
TEST(gap0_battr_set_default_bitmap_rejected) {
    SetBitmapAttributes_pfn s = UN(SetBitmapAttributes); REQUIRE(s);
    HDC dc = U(CreateCompatibleDC)(NULL);
    HBITMAP def = U(GetCurrentObject)(dc, OBJ_BITMAP);
    CHECK(s(def, 1) == NULL);   /* already a stock handle */
    U(DeleteDC)(dc);
}
TEST(gap0_battr_clear_invalid_flags) {
    ClearBitmapAttributes_pfn c = UN(ClearBitmapAttributes); REQUIRE(c);
    HBITMAP h = U(CreateBitmap)(8, 8, 1, 1, NULL);
    DWORD bad[] = { 2, 3, 0x80, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0x6678);
        CHECKF(c(h, bad[i]) == NULL, "flags 0x%lx", bad[i]);
        CHECKF(GetLastError() == 0x6678, "flags 0x%lx touched last error", bad[i]);
    }
    del(h);
}
TEST(gap0_battr_clear_zero_flags) {
    ClearBitmapAttributes_pfn c = UN(ClearBitmapAttributes); REQUIRE(c);
    HBITMAP h = U(CreateBitmap)(8, 8, 1, 1, NULL);
    CHECK(c(h, 0) == NULL);
    del(h);
}
TEST(gap0_battr_clear_on_non_stock) {
    ClearBitmapAttributes_pfn c = UN(ClearBitmapAttributes); REQUIRE(c);
    HBITMAP h = U(CreateBitmap)(8, 8, 1, 1, NULL);
    CHECK(c(h, 1) == NULL);
    EXPECT_EQ(U(GetObjectType)(h), OBJ_BITMAP);
    del(h);
}
TEST(gap0_battr_clear_invalid_handles) {
    ClearBitmapAttributes_pfn c = UN(ClearBitmapAttributes); REQUIRE(c);
    ULONG_PTR v[] = { 0, 0x1234, 0xdeadbeef, 0x800000, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) CHECKF(c((HBITMAP)v[i], 1) == NULL, "handle %llx", (unsigned long long)v[i]);
}
TEST(gap0_battr_clear_default_bitmap_rejected) {
    ClearBitmapAttributes_pfn c = UN(ClearBitmapAttributes); REQUIRE(c);
    HDC dc = U(CreateCompatibleDC)(NULL);
    HBITMAP def = U(GetCurrentObject)(dc, OBJ_BITMAP);
    CHECK(c(def, 1) == NULL);
    EXPECT_EQ(U(GetObjectType)(def), OBJ_BITMAP);
    EXPECT_EQ(UN(GetBitmapAttributes)(def), 1);
    U(DeleteDC)(dc);
}
TEST(gap0_battr_stock_roundtrip) {
    SetBitmapAttributes_pfn s = UN(SetBitmapAttributes); ClearBitmapAttributes_pfn c = UN(ClearBitmapAttributes); GetBitmapAttributes_pfn g = UN(GetBitmapAttributes);
    REQUIRE(s); REQUIRE(c); REQUIRE(g);
    SKIP_WINE("unimplemented");
    HBITMAP h = U(CreateBitmap)(16, 8, 1, 1, NULL);
    CHECK(h != NULL); if (!h) return;
    HBITMAP st = s(h, 1);
    t_note("SetBitmapAttributes(1): %p -> %p", h, st);
    if (st) {
        CHECK(((ULONG_PTR)st & 0x800000) != 0);
        EXPECT_EQ((ULONG_PTR)st & ~(ULONG_PTR)0x800000, (ULONG_PTR)h);
        EXPECT_EQ(g(st), 1);
        EXPECT_EQ(U(GetObjectType)(st), OBJ_BITMAP);
        BITMAP bm; memset(&bm, 0, sizeof bm);
        EXPECT_EQ(U(GetObjectW)(st, sizeof bm, &bm), sizeof bm);
        EXPECT_EQ(bm.bmWidth, 16); EXPECT_EQ(bm.bmHeight, 8); EXPECT_EQ(bm.bmBitsPixel, 1);
        /* setting it again is a no-op failure: already stock */
        CHECK(s(st, 1) == NULL);
        HBITMAP back = c(st, 1);
        CHECK(back != NULL);
        EXPECT_EQ((ULONG_PTR)back, (ULONG_PTR)h);
        EXPECT_EQ(g(back), 0);
        EXPECT_EQ(U(GetObjectType)(back), OBJ_BITMAP);
        /* now it is an ordinary bitmap again and can be deleted */
        CHECK(U(DeleteObject)(back));
        EXPECT_EQ(U(GetObjectType)(back), 0);
    } else {
        del(h);
    }
}
TEST(gap0_battr_stock_pixels_preserved) {
    SetBitmapAttributes_pfn s = UN(SetBitmapAttributes); ClearBitmapAttributes_pfn c = UN(ClearBitmapAttributes);
    REQUIRE(s); REQUIRE(c);
    SKIP_WINE("unimplemented");
    BYTE bits[8 * 2] = { 0xAA, 0x00, 0x55, 0x00, 0xF0, 0x00, 0x0F, 0x00, 0xFF, 0, 0, 0, 0xFF, 0, 0, 0 };
    HBITMAP h = U(CreateBitmap)(8, 8, 1, 1, bits);
    HBITMAP st = s(h, 1);
    if (!st) { del(h); t_skip("bitmap could not be made stock"); return; }
    BYTE out[16]; memset(out, 0, sizeof out);
    LONG n = U(GetBitmapBits)(st, sizeof out, out);
    EXPECT_EQ(n, 16);
    EXPECT_EQ(out[0], 0xAA); EXPECT_EQ(out[2], 0x55); EXPECT_EQ(out[4], 0xF0); EXPECT_EQ(out[6], 0x0F);
    HBITMAP back = c(st, 1);
    CHECK(back != NULL);
    del(back ? back : st);
}
TEST(gap0_battr_stock_select_into_dc) {
    SetBitmapAttributes_pfn s = UN(SetBitmapAttributes); ClearBitmapAttributes_pfn c = UN(ClearBitmapAttributes);
    REQUIRE(s); REQUIRE(c);
    SKIP_WINE("unimplemented");
    HBITMAP h = U(CreateBitmap)(8, 8, 1, 1, NULL);
    HBITMAP st = s(h, 1);
    if (!st) { del(h); t_skip("bitmap could not be made stock"); return; }
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ old = U(SelectObject)(dc, st);
    t_note("selecting a stock bitmap into a DC: %p", old);
    if (old) {
        EXPECT_EQ(U(GetCurrentObject)(dc, OBJ_BITMAP), st);
        U(PatBlt)(dc, 0, 0, 8, 8, BLACKNESS);
        EXPECT_EQ(U(GetPixel)(dc, 1, 1), RGB(0, 0, 0));
        U(PatBlt)(dc, 0, 0, 8, 8, WHITENESS);
        EXPECT_EQ(U(GetPixel)(dc, 1, 1), RGB(255, 255, 255));
        U(SelectObject)(dc, old);
    }
    HBITMAP back = c(st, 1);
    CHECK(back != NULL);
    del(back ? back : st);
    U(DeleteDC)(dc);
}
TEST(gap0_battr_stock_selected_clear_refused) {
    SetBitmapAttributes_pfn s = UN(SetBitmapAttributes); ClearBitmapAttributes_pfn c = UN(ClearBitmapAttributes);
    REQUIRE(s); REQUIRE(c);
    SKIP_WINE("unimplemented");
    HBITMAP h = U(CreateBitmap)(8, 8, 1, 1, NULL);
    HBITMAP st = s(h, 1);
    if (!st) { del(h); t_skip("bitmap could not be made stock"); return; }
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ old = U(SelectObject)(dc, st);
    HBITMAP r = c(st, 1);
    t_note("clear while selected: %p", r);
    U(SelectObject)(dc, old);
    U(DeleteDC)(dc);
    HBITMAP back = r ? r : c(st, 1);
    CHECK(back != NULL);
    del(back ? back : st);
}
TEST(gap0_battr_stock_delete_is_noop) {
    SetBitmapAttributes_pfn s = UN(SetBitmapAttributes); ClearBitmapAttributes_pfn c = UN(ClearBitmapAttributes);
    REQUIRE(s); REQUIRE(c);
    SKIP_WINE("unimplemented");
    HBITMAP h = U(CreateBitmap)(8, 8, 1, 1, NULL);
    HBITMAP st = s(h, 1);
    if (!st) { del(h); t_skip("bitmap could not be made stock"); return; }
    BOOL d = U(DeleteObject)(st);
    t_note("DeleteObject on stock bitmap: %d", d);
    EXPECT_EQ(U(GetObjectType)(st), OBJ_BITMAP);   /* stock objects are not deleted by DeleteObject */
    HBITMAP back = c(st, 1);
    CHECK(back != NULL);
    del(back ? back : st);
}
TEST(gap0_battr_stock_dib_section) {
    SetBitmapAttributes_pfn s = UN(SetBitmapAttributes); ClearBitmapAttributes_pfn c = UN(ClearBitmapAttributes);
    REQUIRE(s); REQUIRE(c);
    SKIP_WINE("unimplemented");
    BITMAPINFO bi; mkbmi(&bi, 8, -8, 32); void *bits;
    HBITMAP h = U(CreateDIBSection)(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    HBITMAP st = s(h, 1);
    t_note("DIB section made stock: %p", st);
    if (st) { HBITMAP back = c(st, 1); del(back ? back : st); }
    else del(h);
}
TEST(gap0_battr_stock_slots_recycled) {
    SetBitmapAttributes_pfn s = UN(SetBitmapAttributes); ClearBitmapAttributes_pfn c = UN(ClearBitmapAttributes);
    REQUIRE(s); REQUIRE(c);
    SKIP_WINE("unimplemented");
    int ok = 0;
    for (int i = 0; i < 40; i++) {
        HBITMAP h = U(CreateBitmap)(4, 4, 1, 1, NULL);
        HBITMAP st = s(h, 1);
        if (!st) { del(h); continue; }
        HBITMAP back = c(st, 1);
        if (back) { ok++; del(back); } else del(st);
    }
    t_note("%d/40 make-stock/clear cycles succeeded", ok);
    SOFT(ok == 40 || ok == 0, "all or none");
}
TEST(gap0_battr_stock_no_leak) {
    SetBitmapAttributes_pfn s = UN(SetBitmapAttributes); ClearBitmapAttributes_pfn c = UN(ClearBitmapAttributes);
    REQUIRE(s); REQUIRE(c);
    SKIP_WINE("unimplemented");
    DWORD before = gap_gdi_count();
    for (int i = 0; i < 20; i++) {
        HBITMAP h = U(CreateBitmap)(4, 4, 1, 1, NULL);
        HBITMAP st = s(h, 1);
        if (st) { HBITMAP b = c(st, 1); del(b ? b : st); } else del(h);
    }
    EXPECT_EQ(gap_gdi_count(), before);
}
typedef struct { HBITMAP h; DWORD attr; DWORD type; } battr_thr;
static DWORD WINAPI battr_thread(LPVOID p) {
    battr_thr *t = p;
    t->attr = ((DWORD (WINAPI *)(HBITMAP))t_proc("GetBitmapAttributes"))(t->h);
    t->type = ((int (WINAPI *)(HGDIOBJ))t_proc("GetObjectType"))(t->h);
    return 0;
}
TEST(gap0_battr_cross_thread) {
    SetBitmapAttributes_pfn s = UN(SetBitmapAttributes); ClearBitmapAttributes_pfn c = UN(ClearBitmapAttributes);
    REQUIRE(s); REQUIRE(c);
    SKIP_WINE("unimplemented");
    HBITMAP h = U(CreateBitmap)(8, 8, 1, 1, NULL);
    HBITMAP st = s(h, 1);
    battr_thr t = { st ? st : h, 99, 99 };
    HANDLE th = CreateThread(NULL, 0, battr_thread, &t, 0, NULL);
    WaitForSingleObject(th, 10000); CloseHandle(th);
    EXPECT_EQ(t.attr, st ? 1u : 0u);
    EXPECT_EQ(t.type, OBJ_BITMAP);
    if (st) { HBITMAP b = c(st, 1); del(b ? b : st); } else del(h);
}
TEST(gap0_battr_set_selected_bitmap_refused) {
    SetBitmapAttributes_pfn s = UN(SetBitmapAttributes); ClearBitmapAttributes_pfn c = UN(ClearBitmapAttributes);
    REQUIRE(s); REQUIRE(c);
    SKIP_WINE("unimplemented");
    HBITMAP h = U(CreateBitmap)(8, 8, 1, 1, NULL);
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ old = U(SelectObject)(dc, h);
    HBITMAP st = s(h, 1);
    t_note("make stock while selected into a DC: %p", st);
    U(SelectObject)(dc, old);
    if (st) { HBITMAP b = c(st, 1); del(b ? b : st); } else del(h);
    U(DeleteDC)(dc);
}
