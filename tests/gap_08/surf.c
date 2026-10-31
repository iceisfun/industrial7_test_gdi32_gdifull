/* Surface / palette creation and deletion through the DDI entry points. */
#include "g8.h"
#include <limits.h>

#define NEED_SURF() do { G8_NOWINE(); REQUIRE(UN(EngCreateBitmap)); REQUIRE(UN(EngDeleteSurface)); } while (0)

static void del_if(HBITMAP h) { if (h && UN(EngDeleteSurface)) UN(EngDeleteSurface)(h); }

TEST(gap8_engcreatebitmap_formats) {
    NEED_SURF();
    static const SIZE sz[] = { {1,1}, {8,8}, {33,7}, {64,2}, {3,100} };
    for (ULONG f = G8_1BPP; f <= G8_32BPP; f++)
        for (unsigned i = 0; i < sizeof sz / sizeof *sz; i++) {
            UINT before = g8_gdicount();
            HBITMAP h = UN(EngCreateBitmap)(sz[i], 0, f, 0, NULL);
            CHECKF(h != NULL, "fmt %lu %ldx%ld", f, sz[i].cx, sz[i].cy);
            if (h) {
                CHECKF(UN(EngDeleteSurface)(h), "delete fmt %lu", f);
                EXPECT_EQ(g8_gdicount(), before);
            }
        }
}
TEST(gap8_engcreatebitmap_distinct_handles) {
    NEED_SURF();
    HBITMAP h[16];
    for (int i = 0; i < 16; i++) h[i] = UN(EngCreateBitmap)(SZ(4, 4), 0, G8_32BPP, 0, NULL);
    for (int i = 0; i < 16; i++) { CHECK(h[i] != NULL); for (int j = 0; j < i; j++) CHECK(h[i] != h[j]); }
    for (int i = 0; i < 16; i++) CHECK(UN(EngDeleteSurface)(h[i]));
}
TEST(gap8_engcreatebitmap_invalid_sizes) {
    NEED_SURF();
    static const SIZE bad[] = { {0,8}, {8,0}, {0,0}, {-1,8}, {8,-1}, {-1,-1}, {INT_MIN,8}, {8,INT_MIN}, {INT_MIN,INT_MIN},
                                {65536,65536}, {INT_MAX,INT_MAX} };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        HBITMAP h = UN(EngCreateBitmap)(bad[i], 0, G8_32BPP, 0, NULL);
        CHECKF(h == NULL, "size %ldx%ld", bad[i].cx, bad[i].cy);
        del_if(h);
    }
}
TEST(gap8_engcreatebitmap_invalid_sizes_all_formats) {
    NEED_SURF();
    for (ULONG f = G8_1BPP; f <= G8_32BPP; f++) {
        HBITMAP a = UN(EngCreateBitmap)(SZ(0, 4), 0, f, 0, NULL);
        HBITMAP b = UN(EngCreateBitmap)(SZ(4, -4), 0, f, 0, NULL);
        CHECKF(a == NULL && b == NULL, "fmt %lu", f);
        del_if(a); del_if(b);
    }
}
TEST(gap8_engcreatebitmap_odd_formats_nobits) {
    NEED_SURF();
    /* No bits supplied: formats 0 and > 6 (compressed) are rejected unless the driver
     * security level is permissive; observed only. */
    static const ULONG f[] = { 0, 7, 8, 9, 0x10, 0xFF, 0xFFFF, 0x8000, 0x80000006, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof f / sizeof *f; i++) {
        HBITMAP h = UN(EngCreateBitmap)(SZ(4, 4), 0, f[i], 0, NULL);
        t_note("EngCreateBitmap(4x4, fmt 0x%lx) -> %p", f[i], (void *)h);
        del_if(h);
    }
}
TEST(gap8_engcreatebitmap_flags) {
    NEED_SURF();
    /* BMF_TOPDOWN 1, BMF_NOZEROINIT 2, BMF_NOTSYSMEM 8, BMF_DONTCACHE 0x10: all valid with no user bits. */
    static const ULONG fl[] = { 0, 1, 2, 3, 8, 0x10, 0x18 };
    for (unsigned i = 0; i < sizeof fl / sizeof *fl; i++) {
        HBITMAP h = UN(EngCreateBitmap)(SZ(8, 8), 0, G8_32BPP, fl[i], NULL);
        if (!h) t_note("flags 0x%lx rejected", fl[i]);
        else CHECK(UN(EngDeleteSurface)(h));
    }
}
TEST(gap8_engcreatebitmap_user_buffer) {
    NEED_SURF();
    static DWORD bits[8 * 8];
    memset(bits, 0xA5, sizeof bits);
    HBITMAP h = UN(EngCreateBitmap)(SZ(8, 8), 32, G8_32BPP, 4 /*BMF_USERMEM*/, bits);
    CHECK(h != NULL);
    if (h) CHECK(UN(EngDeleteSurface)(h));
    /* the application buffer must stay intact and usable after the surface is gone */
    bits[0] = 1; EXPECT_EQ(bits[0], 1);
    HBITMAP h2 = UN(EngCreateBitmap)(SZ(8, 8), 32, G8_32BPP, 0, bits);
    if (h2) CHECK(UN(EngDeleteSurface)(h2));
}
TEST(gap8_engcreatebitmap_explicit_stride) {
    NEED_SURF();
    HBITMAP h = UN(EngCreateBitmap)(SZ(8, 8), 64, G8_32BPP, 0, NULL);
    t_note("explicit stride 64 -> %p", (void *)h);
    del_if(h);
    h = UN(EngCreateBitmap)(SZ(8, 8), 4, G8_32BPP, 0, NULL);
    t_note("stride too small (4) -> %p", (void *)h);
    del_if(h);
    h = UN(EngCreateBitmap)(SZ(8, 8), -32, G8_32BPP, 0, NULL);
    t_note("negative stride -> %p", (void *)h);
    del_if(h);
}
TEST(gap8_engcreatebitmap_repeat_balanced) {
    NEED_SURF();
    UINT before = g8_gdicount();
    for (int i = 0; i < 300; i++) {
        HBITMAP h = UN(EngCreateBitmap)(SZ(16, 16), 0, (i % 6) + 1, 0, NULL);
        if (!h) { CHECKF(0, "iteration %d", i); break; }
        if (!UN(EngDeleteSurface)(h)) { CHECKF(0, "delete %d", i); break; }
    }
    EXPECT_EQ(g8_gdicount(), before);
}
TEST(gap8_engcreatebitmap_large) {
    NEED_SURF();
    HBITMAP h = UN(EngCreateBitmap)(SZ(2048, 2048), 0, G8_32BPP, 0, NULL);
    t_note("2048x2048x32 -> %p", (void *)h);
    del_if(h);
}
TEST(gap8_engcreatebitmap_lasterror_note) {
    NEED_SURF();
    SET_ERR(0x1234);
    HBITMAP h = UN(EngCreateBitmap)(SZ(4, 4), 0, G8_32BPP, 0, NULL);
    t_note("last error after success: %lu", GetLastError());
    del_if(h);
    SET_ERR(0x1234);
    h = UN(EngCreateBitmap)(SZ(0, 4), 0, G8_32BPP, 0, NULL);
    t_note("last error after bad size: %lu", GetLastError());
}
TEST(gap8_engcreatebitmap_gettype_note) {
    NEED_SURF(); REQUIRE(U(GetObjectType));
    HBITMAP h = UN(EngCreateBitmap)(SZ(4, 4), 0, G8_32BPP, 0, NULL);
    CHECK(h != NULL);
    if (h) {
        t_note("GetObjectType(eng surface) = %lu", U(GetObjectType)(h));
        UN(EngDeleteSurface)(h);
        t_note("after delete = %lu", U(GetObjectType)(h));
    }
}
static HBITMAP volatile hh;
static DWORD WINAPI mk_thread(LPVOID p) { hh = UN(EngCreateBitmap)(SZ(4, 4), 0, G8_32BPP, 0, NULL); return 0; }

TEST(gap8_engcreatebitmap_thread_created_deleted_elsewhere) {
    NEED_SURF();
    hh = NULL;
    HANDLE t = CreateThread(NULL, 0, mk_thread, NULL, 0, NULL);
    CHECK(t != NULL);
    if (!t) return;
    WaitForSingleObject(t, 20000); CloseHandle(t);
    CHECK(hh != NULL);
    if (hh) CHECK(UN(EngDeleteSurface)(hh));
}

/* ---- EngDeleteSurface: deleting things that are not driver surfaces ---- */
TEST(gap8_engdeletesurface_null_and_bogus) {
    NEED_SURF();
    static const ULONG_PTR v[] = { 0, 1, 2, 4, 0x1000, 0xDEADBEEF, 0x00050001, 0xFFFFFFFF, (ULONG_PTR)-1 };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++)
        CHECKF(!UN(EngDeleteSurface)((HBITMAP)v[i]), "handle %llx", (unsigned long long)v[i]);
}
TEST(gap8_engdeletesurface_regular_bitmap_survives) {
    NEED_SURF(); REQUIRE(U(CreateBitmap)); REQUIRE(U(GetObjectType)); REQUIRE(U(DeleteObject));
    HBITMAP bm = U(CreateBitmap)(5, 5, 1, 32, NULL);
    CHECK(bm != NULL);
    CHECK(!UN(EngDeleteSurface)(bm));
    EXPECT_EQ(U(GetObjectType)(bm), OBJ_BITMAP);
    CHECK(!UN(EngDeleteSurface)(bm));
    EXPECT_EQ(U(GetObjectType)(bm), OBJ_BITMAP);
    CHECK(U(DeleteObject)(bm));
    EXPECT_EQ(U(GetObjectType)(bm), 0);
}
TEST(gap8_engdeletesurface_dibsection_survives) {
    NEED_SURF(); REQUIRE(U(CreateDIBSection)); REQUIRE(U(GetObjectType)); REQUIRE(U(DeleteObject));
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 4; bi.bmiHeader.biHeight = 4;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    void *bits = NULL;
    HBITMAP bm = U(CreateDIBSection)(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    CHECK(bm != NULL && bits != NULL);
    if (!bm) return;
    CHECK(!UN(EngDeleteSurface)(bm));
    EXPECT_EQ(U(GetObjectType)(bm), OBJ_BITMAP);
    ((DWORD *)bits)[0] = 0x00112233; EXPECT_EQ(((DWORD *)bits)[0], 0x00112233);
    CHECK(U(DeleteObject)(bm));
}
TEST(gap8_engdeletesurface_wrong_types_survive) {
    NEED_SURF(); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(CreatePen)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(GetObjectType)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetStockObject));
    HBRUSH br = U(CreateSolidBrush)(RGB(1, 2, 3));
    HPEN pn = U(CreatePen)(PS_SOLID, 1, 0);
    HDC dc = U(CreateCompatibleDC)(NULL);
    HRGN rg = U(CreateRectRgn)(0, 0, 4, 4);
    HGDIOBJ stock = U(GetStockObject)(WHITE_BRUSH);
    CHECK(!UN(EngDeleteSurface)((HBITMAP)br)); EXPECT_EQ(U(GetObjectType)(br), OBJ_BRUSH);
    CHECK(!UN(EngDeleteSurface)((HBITMAP)pn)); EXPECT_EQ(U(GetObjectType)(pn), OBJ_PEN);
    CHECK(!UN(EngDeleteSurface)((HBITMAP)dc)); EXPECT_EQ(U(GetObjectType)(dc), OBJ_MEMDC);
    CHECK(!UN(EngDeleteSurface)((HBITMAP)rg)); EXPECT_EQ(U(GetObjectType)(rg), OBJ_REGION);
    CHECK(!UN(EngDeleteSurface)((HBITMAP)stock)); EXPECT_EQ(U(GetObjectType)(stock), OBJ_BRUSH);
    U(DeleteObject)(br); U(DeleteObject)(pn); U(DeleteDC)(dc); U(DeleteObject)(rg);
}
TEST(gap8_engdeletesurface_selected_bitmap_survives) {
    NEED_SURF(); REQUIRE(U(CreateBitmap)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(SelectObject));
    REQUIRE(U(DeleteDC)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectType));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 32, NULL);
    HGDIOBJ old = U(SelectObject)(dc, bm);
    CHECK(old != NULL);
    CHECK(!UN(EngDeleteSurface)(bm));
    EXPECT_EQ(U(GetObjectType)(bm), OBJ_BITMAP);
    EXPECT_EQ(U(SelectObject)(dc, old), bm);
    CHECK(U(DeleteObject)(bm));
    CHECK(U(DeleteDC)(dc));
}
TEST(gap8_engdeletesurface_already_deleted_bitmap) {
    NEED_SURF(); REQUIRE(U(CreateBitmap)); REQUIRE(U(DeleteObject));
    HBITMAP bm = U(CreateBitmap)(5, 5, 1, 32, NULL);
    CHECK(U(DeleteObject)(bm));
    CHECK(!UN(EngDeleteSurface)(bm));
}
TEST(gap8_engdeletesurface_eng_surface_double_delete) {
    NEED_SURF();
    HBITMAP h = UN(EngCreateBitmap)(SZ(4, 4), 0, G8_32BPP, 0, NULL);
    CHECK(h != NULL);
    if (!h) return;
    CHECK(UN(EngDeleteSurface)(h));
    CHECK(!UN(EngDeleteSurface)(h));
}
TEST(gap8_engdeletesurface_eng_surface_not_deleteobject_able) {
    NEED_SURF(); REQUIRE(U(DeleteObject));
    HBITMAP h = UN(EngCreateBitmap)(SZ(4, 4), 0, G8_32BPP, 0, NULL);
    CHECK(h != NULL);
    if (!h) return;
    BOOL r = U(DeleteObject)(h);
    t_note("DeleteObject(eng surface) = %d", r);
    if (!r) CHECK(UN(EngDeleteSurface)(h));
}
TEST(gap8_engdeletesurface_stock_bitmap_object) {
    NEED_SURF(); REQUIRE(U(GetStockObject)); REQUIRE(U(GetObjectType));
    HGDIOBJ s = U(GetStockObject)(DEFAULT_PALETTE);
    CHECK(!UN(EngDeleteSurface)((HBITMAP)s));
    EXPECT_EQ(U(GetObjectType)(s), OBJ_PAL);
}

/* ---- device bitmap / device surface ---- */
TEST(gap8_engcreatedevicebitmap_bad_format) {
    G8_NOWINE(); REQUIRE(UN(EngCreateDeviceBitmap)); REQUIRE(UN(EngDeleteSurface));
    static const ULONG bad[] = { 0, 9, 10, 0x100, 0xFFFF, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        HBITMAP h = UN(EngCreateDeviceBitmap)(0x1234, SZ(8, 8), bad[i]);
        CHECKF(h == NULL, "fmt 0x%lx", bad[i]);
        del_if(h);
    }
}
TEST(gap8_engcreatedevicebitmap_bad_size) {
    G8_NOWINE(); REQUIRE(UN(EngCreateDeviceBitmap)); REQUIRE(UN(EngDeleteSurface));
    static const SIZE bad[] = { {0,8}, {8,0}, {-1,8}, {8,-1}, {0,0}, {INT_MIN,INT_MIN}, {65536,65536}, {INT_MAX,INT_MAX} };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        HBITMAP h = UN(EngCreateDeviceBitmap)(0x1234, bad[i], G8_32BPP);
        CHECKF(h == NULL, "size %ldx%ld", bad[i].cx, bad[i].cy);
        del_if(h);
    }
}
TEST(gap8_engcreatedevicebitmap_valid_formats_observed) {
    G8_NOWINE(); REQUIRE(UN(EngCreateDeviceBitmap)); REQUIRE(UN(EngDeleteSurface));
    for (ULONG f = 1; f <= 8; f++) {
        UINT before = g8_gdicount();
        HBITMAP h = UN(EngCreateDeviceBitmap)(0x1234, SZ(8, 8), f);
        t_note("EngCreateDeviceBitmap fmt %lu -> %p", f, (void *)h);
        if (h) { UN(EngDeleteSurface)(h); EXPECT_EQ(g8_gdicount(), before); }
    }
}
TEST(gap8_engcreatedevicesurface_bad_format) {
    G8_NOWINE(); REQUIRE(UN(EngCreateDeviceSurface)); REQUIRE(UN(EngDeleteSurface));
    static const ULONG bad[] = { 0, 9, 10, 0x100, 0xFFFF, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        HBITMAP h = UN(EngCreateDeviceSurface)(0x1234, SZ(8, 8), bad[i]);
        CHECKF(h == NULL, "fmt 0x%lx", bad[i]);
        del_if(h);
    }
}
TEST(gap8_engcreatedevicesurface_bad_size_observed) {
    G8_NOWINE(); REQUIRE(UN(EngCreateDeviceSurface)); REQUIRE(UN(EngDeleteSurface));
    static const SIZE bad[] = { {0,8}, {8,0}, {-1,8}, {8,-1}, {0,0}, {INT_MIN,INT_MIN} };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        HBITMAP h = UN(EngCreateDeviceSurface)(0x1234, bad[i], G8_32BPP);
        t_note("EngCreateDeviceSurface %ldx%ld -> %p", bad[i].cx, bad[i].cy, (void *)h);
        del_if(h);
    }
}
TEST(gap8_engcreatedevicesurface_valid_observed) {
    G8_NOWINE(); REQUIRE(UN(EngCreateDeviceSurface)); REQUIRE(UN(EngDeleteSurface));
    for (ULONG f = 1; f <= 8; f++) {
        UINT before = g8_gdicount();
        HBITMAP h = UN(EngCreateDeviceSurface)(0x1234, SZ(8, 8), f);
        t_note("EngCreateDeviceSurface fmt %lu -> %p", f, (void *)h);
        if (h) { UN(EngDeleteSurface)(h); EXPECT_EQ(g8_gdicount(), before); }
    }
}

/* ---- association / banding ---- */
TEST(gap8_engassociatesurface_null_and_bogus) {
    G8_NOWINE(); REQUIRE(UN(EngAssociateSurface));
    static const ULONG_PTR v[] = { 0, 1, 0x1000, 0xDEADBEEF, (ULONG_PTR)-1 };
    static const ULONG fl[] = { 0, 1, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++)
        for (unsigned j = 0; j < sizeof fl / sizeof *fl; j++) {
            CHECK(!UN(EngAssociateSurface)((HBITMAP)v[i], NULL, fl[j]));
            CHECK(!UN(EngAssociateSurface)((HBITMAP)v[i], (HANDLE)v[i], fl[j]));
        }
}
TEST(gap8_engassociatesurface_regular_bitmap) {
    G8_NOWINE(); REQUIRE(UN(EngAssociateSurface)); REQUIRE(U(CreateBitmap)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectType));
    HBITMAP bm = U(CreateBitmap)(4, 4, 1, 32, NULL);
    CHECK(!UN(EngAssociateSurface)(bm, NULL, 0));
    CHECK(!UN(EngAssociateSurface)(bm, (HANDLE)(ULONG_PTR)0x1234, 0));
    CHECK(!UN(EngAssociateSurface)(bm, (HANDLE)(ULONG_PTR)0x1234, 0xFFFFFFFF));
    EXPECT_EQ(U(GetObjectType)(bm), OBJ_BITMAP);
    CHECK(U(DeleteObject)(bm));
}
TEST(gap8_engassociatesurface_eng_surface_bad_hdev) {
    NEED_SURF(); REQUIRE(UN(EngAssociateSurface));
    HBITMAP h = UN(EngCreateBitmap)(SZ(4, 4), 0, G8_32BPP, 0, NULL);
    CHECK(h != NULL);
    if (!h) return;
    CHECK(!UN(EngAssociateSurface)(h, NULL, 0));
    CHECK(!UN(EngAssociateSurface)(h, (HANDLE)(ULONG_PTR)0x1234, 0));
    CHECK(!UN(EngAssociateSurface)(h, (HANDLE)(ULONG_PTR)0xDEADBEEF, 0x1FF));
    CHECK(!UN(EngAssociateSurface)(h, (HANDLE)(ULONG_PTR)-1, 0xFFFFFFFF));
    CHECK(UN(EngDeleteSurface)(h));
}
TEST(gap8_engmarkbandingsurface_null_and_bogus) {
    G8_NOWINE(); REQUIRE(UN(EngMarkBandingSurface));
    static const ULONG_PTR v[] = { 0, 1, 0x1000, 0xDEADBEEF, 0x00050001, (ULONG_PTR)-1 };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++)
        CHECKF(!UN(EngMarkBandingSurface)((HBITMAP)v[i]), "handle %llx", (unsigned long long)v[i]);
}
TEST(gap8_engmarkbandingsurface_regular_bitmap) {
    G8_NOWINE(); REQUIRE(UN(EngMarkBandingSurface)); REQUIRE(U(CreateBitmap)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectType));
    HBITMAP bm = U(CreateBitmap)(4, 4, 1, 32, NULL);
    CHECK(!UN(EngMarkBandingSurface)(bm));
    CHECK(!UN(EngMarkBandingSurface)(bm));
    EXPECT_EQ(U(GetObjectType)(bm), OBJ_BITMAP);
    CHECK(U(DeleteObject)(bm));
}
TEST(gap8_engmarkbandingsurface_wrong_type) {
    G8_NOWINE(); REQUIRE(UN(EngMarkBandingSurface)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    HBRUSH b = U(CreateSolidBrush)(0);
    CHECK(!UN(EngMarkBandingSurface)((HBITMAP)b));
    CHECK(U(DeleteObject)(b));
}
TEST(gap8_engmarkbandingsurface_eng_surface_observed) {
    NEED_SURF(); REQUIRE(UN(EngMarkBandingSurface));
    HBITMAP h = UN(EngCreateBitmap)(SZ(4, 4), 0, G8_32BPP, 0, NULL);
    CHECK(h != NULL);
    if (!h) return;
    t_note("EngMarkBandingSurface(plain eng surface) = %d", UN(EngMarkBandingSurface)(h));
    CHECK(UN(EngDeleteSurface)(h));
}

/* ---- palettes ---- */
#define NEED_PAL() do { G8_NOWINE(); REQUIRE(UN(EngCreatePalette)); REQUIRE(UN(EngDeletePalette)); } while (0)

TEST(gap8_engcreatepalette_indexed) {
    NEED_PAL(); REQUIRE(U(GetObjectType));
    ULONG col[4] = { 0x000000, 0x0000FF, 0x00FF00, 0xFF0000 };
    UINT before = g8_gdicount();
    HPALETTE p = UN(EngCreatePalette)(1 /*PAL_INDEXED*/, 4, col, 0, 0, 0);
    CHECK(p != NULL);
    if (!p) return;
    t_note("GetObjectType(eng palette) = %lu", U(GetObjectType)(p));
    UN(EngDeletePalette)(p);
    EXPECT_EQ(g8_gdicount(), before);
}
TEST(gap8_engcreatepalette_indexed_sizes) {
    NEED_PAL();
    static ULONG col[300];
    for (int i = 0; i < 300; i++) col[i] = i * 0x010203;
    static const ULONG n[] = { 1, 2, 16, 255, 256, 257, 300 };
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++) {
        HPALETTE p = UN(EngCreatePalette)(1, n[i], col, 0, 0, 0);
        CHECKF(p != NULL, "n=%lu", n[i]);
        if (p) UN(EngDeletePalette)(p);
    }
}
TEST(gap8_engcreatepalette_rgb_bgr_bitfields) {
    NEED_PAL();
    HPALETTE a = UN(EngCreatePalette)(4 /*PAL_RGB*/, 0, NULL, 0, 0, 0);
    HPALETTE b = UN(EngCreatePalette)(8 /*PAL_BGR*/, 0, NULL, 0, 0, 0);
    HPALETTE c = UN(EngCreatePalette)(2 /*PAL_BITFIELDS*/, 0, NULL, 0xF800, 0x07E0, 0x001F);
    HPALETTE d = UN(EngCreatePalette)(2, 0, NULL, 0xFF, 0xFF00, 0xFF0000);
    HPALETTE e = UN(EngCreatePalette)(2, 0, NULL, 0xFF0000, 0xFF00, 0xFF);
    t_note("rgb=%p bgr=%p bitfields565=%p bitfieldsBGR=%p bitfieldsRGB=%p", (void*)a, (void*)b, (void*)c, (void*)d, (void*)e);
    CHECK(a != NULL); CHECK(b != NULL); CHECK(c != NULL);
    if (a) UN(EngDeletePalette)(a);
    if (b) UN(EngDeletePalette)(b);
    if (c) UN(EngDeletePalette)(c);
    if (d) UN(EngDeletePalette)(d);
    if (e) UN(EngDeletePalette)(e);
}
TEST(gap8_engcreatepalette_too_many_colors) {
    NEED_PAL();
    static ULONG col[4];
    static const ULONG n[] = { 0x10001, 0x20000, 0x7FFFFFFF, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++) {
        HPALETTE p = UN(EngCreatePalette)(1, n[i], col, 0, 0, 0);
        CHECKF(p == NULL, "n=0x%lx", n[i]);
        if (p) UN(EngDeletePalette)(p);
    }
}
TEST(gap8_engcreatepalette_indexed_unreadable_colors) {
    NEED_PAL();
    HPALETTE p = UN(EngCreatePalette)(1, 4, NULL, 0, 0, 0);
    CHECK(p == NULL);
    if (p) UN(EngDeletePalette)(p);
}
TEST(gap8_engcreatepalette_odd_modes_observed) {
    NEED_PAL();
    static const ULONG m[] = { 0, 3, 5, 6, 7, 9, 16, 0x100, 0xFFFF };
    ULONG col[2] = { 1, 2 };
    for (unsigned i = 0; i < sizeof m / sizeof *m; i++) {
        HPALETTE p = UN(EngCreatePalette)(m[i], 0, col, 0, 0, 0);
        t_note("EngCreatePalette(mode 0x%lx, 0 colors) -> %p", m[i], (void *)p);
        if (p) UN(EngDeletePalette)(p);
    }
}
TEST(gap8_engcreatepalette_zero_colors_indexed_observed) {
    NEED_PAL();
    HPALETTE p = UN(EngCreatePalette)(1, 0, NULL, 0, 0, 0);
    t_note("indexed, 0 colors -> %p", (void *)p);
    if (p) UN(EngDeletePalette)(p);
}
TEST(gap8_engcreatepalette_repeat_balanced) {
    NEED_PAL();
    UINT before = g8_gdicount();
    ULONG col[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    for (int i = 0; i < 200; i++) {
        HPALETTE p = UN(EngCreatePalette)(1, 8, col, 0, 0, 0);
        if (!p) { CHECKF(0, "iteration %d", i); break; }
        UN(EngDeletePalette)(p);
    }
    EXPECT_EQ(g8_gdicount(), before);
}
TEST(gap8_engdeletepalette_deletes_effectively) {
    NEED_PAL(); REQUIRE(U(GetObjectType));
    ULONG col[2] = { 0x112233, 0x445566 };
    HPALETTE p = UN(EngCreatePalette)(1, 2, col, 0, 0, 0);
    CHECK(p != NULL);
    if (!p) return;
    DWORD t = U(GetObjectType)(p);
    UN(EngDeletePalette)(p);
    if (t) EXPECT_EQ(U(GetObjectType)(p), 0);
}
TEST(gap8_engdeletepalette_null_and_bogus) {
    NEED_PAL();
    static const ULONG_PTR v[] = { 0, 1, 0x1000, 0xDEADBEEF, 0x00050001, (ULONG_PTR)-1 };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) UN(EngDeletePalette)((HPALETTE)v[i]);
    CHECK(1);
}
TEST(gap8_engdeletepalette_stock_palette_survives) {
    NEED_PAL(); REQUIRE(U(GetStockObject)); REQUIRE(U(GetObjectType));
    HGDIOBJ s = U(GetStockObject)(DEFAULT_PALETTE);
    CHECK(s != NULL);
    UN(EngDeletePalette)((HPALETTE)s);
    EXPECT_EQ(U(GetObjectType)(s), OBJ_PAL);
    EXPECT_EQ(U(GetStockObject)(DEFAULT_PALETTE), s);
}
TEST(gap8_engdeletepalette_regular_palette_survives) {
    NEED_PAL(); REQUIRE(U(CreatePalette)); REQUIRE(U(GetObjectType)); REQUIRE(U(DeleteObject));
    struct { WORD v, n; PALETTEENTRY e[2]; } lp = { 0x300, 2, { {1,2,3,0}, {4,5,6,0} } };
    HPALETTE p = U(CreatePalette)((LOGPALETTE *)&lp);
    CHECK(p != NULL);
    if (!p) return;
    UN(EngDeletePalette)(p);
    EXPECT_EQ(U(GetObjectType)(p), OBJ_PAL);
    CHECK(U(DeleteObject)(p));
}
TEST(gap8_engdeletepalette_wrong_type_survives) {
    NEED_PAL(); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(GetObjectType)); REQUIRE(U(DeleteObject));
    HBRUSH b = U(CreateSolidBrush)(0x123456);
    UN(EngDeletePalette)((HPALETTE)b);
    EXPECT_EQ(U(GetObjectType)(b), OBJ_BRUSH);
    CHECK(U(DeleteObject)(b));
}
