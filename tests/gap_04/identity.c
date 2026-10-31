#include "gap4.h"
/* GdiConvert{Font,Palette,Region,...} and GdiGetLocal{Brush,DC,Font} share one
 * body on Win10: they return their argument unchanged (no validation). */

static const ULONG_PTR k_vals[] = {
    0, 1, 2, 0x7f, 0xffff, 0x10000, 0x12345678, 0x7fffffff, 0x80000000ull,
    0xffffffffull, 0x100000000ull, 0x123456789abcdef0ull, 0x8000000000000000ull,
    (ULONG_PTR)-1, (ULONG_PTR)-2,
};
#define NVALS (sizeof k_vals / sizeof k_vals[0])

#define ID_TABLE_TEST(fn, tag) \
TEST(gap4_##tag##_identity_values) { \
    RQ(fn); \
    for (unsigned i = 0; i < NVALS; i++) { \
        HANDLE r = UN(fn)((HANDLE)k_vals[i]); \
        CHECKF((ULONG_PTR)r == k_vals[i], #fn "(0x%llx) -> 0x%llx", (unsigned long long)k_vals[i], (unsigned long long)(ULONG_PTR)r); \
    } \
} \
TEST(gap4_##tag##_identity_no_lasterror) { \
    RQ(fn); \
    SET_ERR(0x1234); \
    UN(fn)((HANDLE)0x55555); \
    EXPECT_EQ(GetLastError(), 0x1234); \
    SET_ERR(0x1234); \
    UN(fn)(NULL); \
    EXPECT_EQ(GetLastError(), 0x1234); \
} \
TEST(gap4_##tag##_identity_repeat) { \
    RQ(fn); \
    HANDLE acc = (HANDLE)0x4242; \
    for (int i = 0; i < 2000; i++) acc = UN(fn)(acc); \
    EXPECT_EQ(acc, 0x4242); \
}

ID_TABLE_TEST(GdiConvertFont, convertfont)
ID_TABLE_TEST(GdiConvertPalette, convertpalette)
ID_TABLE_TEST(GdiConvertRegion, convertregion)
ID_TABLE_TEST(GdiGetLocalBrush, getlocalbrush)
ID_TABLE_TEST(GdiGetLocalDC, getlocaldc)
ID_TABLE_TEST(GdiGetLocalFont, getlocalfont)

TEST(gap4_convertfont_realfont) {
    RQ(GdiConvertFont); REQUIRE(U(CreateFontW)); REQUIRE(U(DeleteObject));
    HFONT f = U(CreateFontW)(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, L"Arial");
    REQUIRE(f);
    EXPECT_EQ(UN(GdiConvertFont)(f), f);
    EXPECT_EQ(UN(GdiGetLocalFont)(f), f);
    /* object stays valid and untouched */
    CHECK(U(GetObjectType)(f) == OBJ_FONT);
    CHECK(U(DeleteObject)(f));
    /* no validation: deleted handle is still echoed back */
    EXPECT_EQ(UN(GdiConvertFont)(f), f);
}

TEST(gap4_convertfont_stock_font) {
    RQ(GdiConvertFont); REQUIRE(U(GetStockObject));
    int ids[] = { OEM_FIXED_FONT, ANSI_FIXED_FONT, ANSI_VAR_FONT, SYSTEM_FONT, DEVICE_DEFAULT_FONT, SYSTEM_FIXED_FONT, DEFAULT_GUI_FONT };
    for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        HGDIOBJ o = U(GetStockObject)(ids[i]);
        CHECKF(o != NULL, "stock %d", ids[i]);
        EXPECT_EQ(UN(GdiConvertFont)(o), o);
        EXPECT_EQ(UN(GdiGetLocalFont)(o), o);
    }
}

TEST(gap4_convertpalette_realpalette) {
    RQ(GdiConvertPalette); REQUIRE(U(CreatePalette)); REQUIRE(U(DeleteObject));
    LOGPALETTE lp = { 0x300, 1, { { 1, 2, 3, 0 } } };
    HPALETTE p = U(CreatePalette)(&lp);
    REQUIRE(p);
    EXPECT_EQ(UN(GdiConvertPalette)(p), p);
    CHECK(U(GetObjectType)(p) == OBJ_PAL);
    CHECK(U(DeleteObject)(p));
    EXPECT_EQ(UN(GdiConvertPalette)(p), p);
    HGDIOBJ sp = U(GetStockObject)(DEFAULT_PALETTE);
    EXPECT_EQ(UN(GdiConvertPalette)(sp), sp);
}

TEST(gap4_convertregion_realregion) {
    RQ(GdiConvertRegion); REQUIRE(U(CreateRectRgn)); REQUIRE(U(DeleteObject));
    HRGN r = U(CreateRectRgn)(1, 2, 30, 40);
    REQUIRE(r);
    EXPECT_EQ(UN(GdiConvertRegion)(r), r);
    RECT bb; CHECK(U(GetRgnBox)(r, &bb) == SIMPLEREGION);
    CHECK(bb.left == 1 && bb.top == 2 && bb.right == 30 && bb.bottom == 40);
    CHECK(U(DeleteObject)(r));
    EXPECT_EQ(UN(GdiConvertRegion)(r), r);
}

TEST(gap4_getlocalbrush_realbrush) {
    RQ(GdiGetLocalBrush); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    HBRUSH b = U(CreateSolidBrush)(RGB(1, 2, 3));
    REQUIRE(b);
    EXPECT_EQ(UN(GdiGetLocalBrush)(b), b);
    LOGBRUSH lb; CHECK(U(GetObjectW)(b, sizeof lb, &lb) == sizeof lb);
    CHECK(lb.lbColor == RGB(1, 2, 3));
    CHECK(U(DeleteObject)(b));
    int ids[] = { WHITE_BRUSH, LTGRAY_BRUSH, GRAY_BRUSH, DKGRAY_BRUSH, BLACK_BRUSH, NULL_BRUSH, DC_BRUSH };
    for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        HGDIOBJ o = U(GetStockObject)(ids[i]);
        EXPECT_EQ(UN(GdiGetLocalBrush)(o), o);
    }
}

TEST(gap4_getlocaldc_realdc) {
    RQ(GdiGetLocalDC); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    REQUIRE(dc);
    EXPECT_EQ(UN(GdiGetLocalDC)(dc), dc);
    CHECK(U(GetObjectType)(dc) == OBJ_MEMDC);
    CHECK(U(DeleteDC)(dc));
    EXPECT_EQ(UN(GdiGetLocalDC)(dc), dc);
}

TEST(gap4_convert_bitmap_brush_dc_same_body) {
    /* Not in this area's export list but share the body: sanity only. */
    if (g4_ok("GdiConvertBitmap")) EXPECT_EQ(UN(GdiConvertBitmap)((HANDLE)0x77), 0x77);
    if (g4_ok("GdiConvertBrush")) EXPECT_EQ(UN(GdiConvertBrush)((HANDLE)0x78), 0x78);
    if (g4_ok("GdiConvertDC")) EXPECT_EQ(UN(GdiConvertDC)((HANDLE)0x79), 0x79);
}

TEST(gap4_convert_no_gdi_leak) {
    RQ(GdiConvertFont); REQUIRE(U(GetGuiResources));
    DWORD before = g4_gdi_count();
    for (int i = 0; i < 1000; i++) {
        UN(GdiConvertFont)((HANDLE)(ULONG_PTR)i);
        UN(GdiConvertPalette)((HANDLE)(ULONG_PTR)i);
        UN(GdiConvertRegion)((HANDLE)(ULONG_PTR)i);
        UN(GdiGetLocalDC)((HANDLE)(ULONG_PTR)i);
    }
    EXPECT_EQ(g4_gdi_count(), before);
}

TEST(gap4_gdi_identity_exports_resolve_both_modules) {
    G4_WINE_SKIP("no gdi32full");
    const char *names[] = { "GdiConvertFont", "GdiConvertPalette", "GdiConvertRegion", "GdiGetLocalBrush", "GdiGetLocalDC", "GdiGetLocalFont" };
    for (unsigned i = 0; i < 6; i++) {
        CHECKF(t_proc_gdi32(names[i]) != NULL, "gdi32 export %s", names[i]);
        CHECKF(t_proc_gdi32full(names[i]) != NULL, "gdi32full export %s", names[i]);
    }
}

TEST(gap4_deletelocaldc_returns_one) {
    RQ(GdiDeleteLocalDC);
    for (unsigned i = 0; i < NVALS; i++) {
        int r = UN(GdiDeleteLocalDC)((HANDLE)k_vals[i]);
        CHECKF(r == 1, "GdiDeleteLocalDC(0x%llx)=%d", (unsigned long long)k_vals[i], r);
    }
}

TEST(gap4_deletelocaldc_does_not_delete) {
    RQ(GdiDeleteLocalDC); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetDeviceCaps));
    HDC dc = U(CreateCompatibleDC)(NULL);
    REQUIRE(dc);
    DWORD before = g4_gdi_count();
    EXPECT_EQ(UN(GdiDeleteLocalDC)(dc), 1);
    CHECK(U(GetObjectType)(dc) == OBJ_MEMDC);
    CHECK(U(GetDeviceCaps)(dc, BITSPIXEL) > 0);
    EXPECT_EQ(g4_gdi_count(), before);
    CHECK(U(DeleteDC)(dc));
    CHECK(g4_gdi_count() == before - 1);
    EXPECT_EQ(UN(GdiDeleteLocalDC)(dc), 1);
}

TEST(gap4_deletelocaldc_no_lasterror) {
    RQ(GdiDeleteLocalDC);
    SET_ERR(0x4321);
    UN(GdiDeleteLocalDC)(NULL);
    EXPECT_EQ(GetLastError(), 0x4321);
}
