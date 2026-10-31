#include "pal.h"

/* ---- SelectPalette ---- */
TEST(palette_select_returns_default_first) {
    REQUIRE(U(SelectPalette)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(GetStockObject)); REQUIRE(U(DeleteObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HPALETTE p = pal_make(16);
    HPALETTE old = U(SelectPalette)(dc, p, FALSE);
    CHECK(old != NULL);
    EXPECT_EQ(old, U(GetStockObject)(DEFAULT_PALETTE));
    HPALETTE back = U(SelectPalette)(dc, old, FALSE);
    EXPECT_EQ(back, p);
    U(DeleteObject)(p);
    U(DeleteDC)(dc);
}

TEST(palette_select_chain) {
    REQUIRE(U(SelectPalette)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HPALETTE a = pal_make(4), b = pal_make(8), c = pal_make(12);
    HPALETTE def = U(SelectPalette)(dc, a, FALSE);
    EXPECT_EQ(U(SelectPalette)(dc, b, FALSE), a);
    EXPECT_EQ(U(SelectPalette)(dc, c, TRUE), b);
    EXPECT_EQ(U(SelectPalette)(dc, c, FALSE), c);     /* re-selecting the current one */
    EXPECT_EQ(U(SelectPalette)(dc, def, FALSE), c);
    U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(c);
    U(DeleteDC)(dc);
}

TEST(palette_select_bforcebkgd_values) {
    REQUIRE(U(SelectPalette)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HPALETTE p = pal_make(4);
    static const BOOL vals[] = {FALSE, TRUE, 2, -1, 0x100};
    HPALETTE def = NULL;
    for (unsigned i = 0; i < sizeof vals / sizeof *vals; i++) {
        HPALETTE o = U(SelectPalette)(dc, p, vals[i]);
        CHECKF(o != NULL, "bForce=%d", vals[i]);
        if (!def) def = o;
        HPALETTE b = U(SelectPalette)(dc, def, FALSE);
        EXPECT_EQ(b, p);
    }
    U(DeleteObject)(p);
    U(DeleteDC)(dc);
}

TEST(palette_select_null_dc) {
    REQUIRE(U(SelectPalette));
    HPALETTE p = pal_make(4);
    SetLastError(0);
    EXPECT_EQ(U(SelectPalette)(NULL, p, FALSE), NULL);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    U(DeleteObject)(p);
}

TEST(palette_select_bogus_dc) {
    REQUIRE(U(SelectPalette));
    HPALETTE p = pal_make(4);
    EXPECT_EQ(U(SelectPalette)((HDC)PAL_BOGUS_HANDLE, p, FALSE), NULL);
    EXPECT_EQ(U(SelectPalette)((HDC)p, p, FALSE), NULL);       /* a palette is not a DC */
    U(DeleteObject)(p);
}

TEST(palette_select_deleted_dc) {
    REQUIRE(U(SelectPalette)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    U(DeleteDC)(dc);
    HPALETTE p = pal_make(4);
    SetLastError(0);
    EXPECT_EQ(U(SelectPalette)(dc, p, FALSE), NULL);
    U(DeleteObject)(p);
}

TEST(palette_select_invalid_palette) {
    REQUIRE(U(SelectPalette)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(GetStockObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HPALETTE def = (HPALETTE)U(GetStockObject)(DEFAULT_PALETTE);
    SetLastError(0);
    HPALETTE r = U(SelectPalette)(dc, NULL, FALSE);
    t_note("SelectPalette(dc,NULL)=%p err=%lu", (void *)r, GetLastError());
    r = U(SelectPalette)(dc, (HPALETTE)PAL_BOGUS_HANDLE, FALSE);
    EXPECT_EQ(r, NULL);
    /* palette state must be unchanged: selecting default returns default */
    EXPECT_EQ(U(SelectPalette)(dc, def, FALSE), def);
    U(DeleteDC)(dc);
}

TEST(palette_select_wrong_object_types) {
    REQUIRE(U(SelectPalette)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(CreateSolidBrush));
    REQUIRE(U(CreatePen)); REQUIRE(U(CreateBitmap)); REQUIRE(U(CreateRectRgn)); REQUIRE(U(DeleteObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ objs[4] = {U(CreateSolidBrush)(0), U(CreatePen)(PS_SOLID, 1, 0), U(CreateBitmap)(4, 4, 1, 1, NULL), U(CreateRectRgn)(0, 0, 3, 3)};
    for (int i = 0; i < 4; i++) {
        HPALETTE r = U(SelectPalette)(dc, (HPALETTE)objs[i], FALSE);
        CHECKF(r == NULL, "type %d", i);
    }
    HPALETTE def = (HPALETTE)U(GetStockObject)(DEFAULT_PALETTE);
    EXPECT_EQ(U(SelectPalette)(dc, def, FALSE), def);
    for (int i = 0; i < 4; i++) U(DeleteObject)(objs[i]);
    U(DeleteDC)(dc);
}

TEST(palette_select_deleted_palette) {
    REQUIRE(U(SelectPalette)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetStockObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HPALETTE p = pal_make(4);
    U(DeleteObject)(p);
    EXPECT_EQ(U(SelectPalette)(dc, p, FALSE), NULL);
    U(DeleteDC)(dc);
}

TEST(palette_select_stock_default) {
    REQUIRE(U(SelectPalette)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(GetStockObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HPALETTE def = (HPALETTE)U(GetStockObject)(DEFAULT_PALETTE);
    EXPECT_EQ(U(SelectPalette)(dc, def, FALSE), def);
    EXPECT_EQ(U(SelectPalette)(dc, def, TRUE), def);
    U(DeleteDC)(dc);
}

TEST(palette_select_two_dcs) {
    REQUIRE(U(SelectPalette)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteObject));
    HDC d1 = U(CreateCompatibleDC)(NULL), d2 = U(CreateCompatibleDC)(NULL);
    HPALETTE p = pal_make(8);
    HPALETTE o1 = U(SelectPalette)(d1, p, FALSE);
    HPALETTE o2 = U(SelectPalette)(d2, p, FALSE);
    CHECK(o1 != NULL);
    t_note("palette selected in a second DC: prev=%p (default=%p)", (void *)o2, (void *)o1);
    if (o2) {
        U(SelectPalette)(d2, o2, FALSE);
        EXPECT_EQ(U(SelectPalette)(d1, o1, FALSE), p);
    } else {
        U(SelectPalette)(d1, o1, FALSE);
    }
    U(DeleteObject)(p);
    U(DeleteDC)(d1); U(DeleteDC)(d2);
}

TEST(palette_select_palette_survives_other_dc) {
    REQUIRE(U(SelectPalette)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteObject));
    HDC d1 = U(CreateCompatibleDC)(NULL), d2 = U(CreateCompatibleDC)(NULL);
    HPALETTE p = pal_make(8), q = pal_make(8);
    HPALETTE def = U(SelectPalette)(d1, p, FALSE);
    U(SelectPalette)(d2, q, FALSE);
    EXPECT_EQ(U(SelectPalette)(d1, def, FALSE), p);       /* selection on d2 does not disturb d1 */
    EXPECT_EQ(U(SelectPalette)(d2, def, FALSE), q);
    U(DeleteObject)(p); U(DeleteObject)(q);
    U(DeleteDC)(d1); U(DeleteDC)(d2);
}

TEST(palette_select_then_deletedc_leaves_palette) {
    REQUIRE(U(SelectPalette)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectType));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HPALETTE p = pal_make(8);
    U(SelectPalette)(dc, p, FALSE);
    U(DeleteDC)(dc);
    EXPECT_EQ(U(GetObjectType)(p), OBJ_PAL);
    EXPECT_EQ(pal_count(p), 8);
    CHECK(U(DeleteObject)(p));
}

TEST(palette_select_delete_while_selected) {
    REQUIRE(U(SelectPalette)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectType));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HPALETTE p = pal_make(8);
    HPALETTE def = U(SelectPalette)(dc, p, FALSE);
    SetLastError(0);
    BOOL r = U(DeleteObject)(p);
    t_note("DeleteObject(palette selected in DC)=%d err=%lu", r, GetLastError());
    if (r) {
        EXPECT_EQ(U(GetObjectType)(p), 0);
    } else {
        U(SelectPalette)(dc, def, FALSE);
        CHECK(U(DeleteObject)(p));
    }
    U(DeleteDC)(dc);
}

TEST(palette_select_gdi_leak) {
    REQUIRE(U(SelectPalette)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetGuiResources));
    DWORD before = gdi_count();
    if (!before) { t_skip("GetGuiResources unsupported"); return; }
    for (int i = 0; i < 100; i++) {
        HDC dc = U(CreateCompatibleDC)(NULL);
        HPALETTE p = pal_make(4);
        HPALETTE o = U(SelectPalette)(dc, p, i & 1);
        U(SelectPalette)(dc, o, FALSE);
        U(DeleteObject)(p);
        U(DeleteDC)(dc);
    }
    EXPECT_EQ(gdi_count(), before);
}

TEST(palette_select_into_dib_dcs) {
    REQUIRE(U(SelectPalette)); REQUIRE(U(DeleteObject));
    static const int bpps[] = {1, 4, 8, 16, 24, 32};
    for (unsigned i = 0; i < sizeof bpps / sizeof *bpps; i++) {
        struct dibdc d;
        if (!dibdc_create(&d, bpps[i], 8, 8)) continue;
        HPALETTE p = pal_make(16);
        HPALETTE o = U(SelectPalette)(d.dc, p, FALSE);
        CHECKF(o != NULL, "bpp %d", bpps[i]);
        if (o) EXPECT_EQ(U(SelectPalette)(d.dc, o, FALSE), p);
        U(DeleteObject)(p);
        dibdc_free(&d);
    }
}

TEST(palette_select_screen_dc) {
    REQUIRE(U(SelectPalette)); REQUIRE(U(DeleteObject));
    HDC dc = pal_screen_dc();
    if (!dc) { t_skip("no screen dc"); return; }
    HPALETTE p = pal_make(16);
    HPALETTE o = U(SelectPalette)(dc, p, TRUE);
    CHECK(o != NULL);
    if (o) EXPECT_EQ(U(SelectPalette)(dc, o, TRUE), p);
    U(DeleteObject)(p);
    pal_release_screen_dc(dc);
}

TEST(palette_select_per_dc_isolated_after_restore) {
    REQUIRE(U(SelectPalette)); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HPALETTE p = pal_make(4);
    int id = U(SaveDC)(dc);
    HPALETTE def = U(SelectPalette)(dc, p, FALSE);
    CHECK(U(RestoreDC)(dc, id));
    HPALETTE cur = U(SelectPalette)(dc, def, FALSE);
    t_note("palette after RestoreDC: %s", cur == p ? "still selected" : "restored");
    U(DeleteObject)(p);
    U(DeleteDC)(dc);
}

/* ---- RealizePalette ---- */
TEST(palette_realize_null_dc) {
    REQUIRE(U(RealizePalette));
    SetLastError(0);
    EXPECT_EQ(U(RealizePalette)(NULL), GDI_ERROR);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}

TEST(palette_realize_bogus_dc) {
    REQUIRE(U(RealizePalette));
    EXPECT_EQ(U(RealizePalette)((HDC)PAL_BOGUS_HANDLE), GDI_ERROR);
    HPALETTE p = pal_make(2);
    EXPECT_EQ(U(RealizePalette)((HDC)p), GDI_ERROR);
    U(DeleteObject)(p);
}

TEST(palette_realize_deleted_dc) {
    REQUIRE(U(RealizePalette)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    U(DeleteDC)(dc);
    EXPECT_EQ(U(RealizePalette)(dc), GDI_ERROR);
}

TEST(palette_realize_default_palette_memdc) {
    REQUIRE(U(RealizePalette)); REQUIRE(U(CreateCompatibleDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    UINT r = U(RealizePalette)(dc);
    t_note("RealizePalette(memdc, default palette)=%u", r);
    CHECK(r != GDI_ERROR);
    U(DeleteDC)(dc);
}

TEST(palette_realize_non_palette_dib_dcs) {
    REQUIRE(U(RealizePalette)); REQUIRE(U(SelectPalette)); REQUIRE(U(DeleteObject));
    static const int bpps[] = {16, 24, 32};
    for (unsigned i = 0; i < sizeof bpps / sizeof *bpps; i++) {
        struct dibdc d;
        if (!dibdc_create(&d, bpps[i], 8, 8)) continue;
        HPALETTE p = pal_make(32);
        HPALETTE o = U(SelectPalette)(d.dc, p, FALSE);
        UINT r = U(RealizePalette)(d.dc);
        EXPECT_NE(r, GDI_ERROR);
        if (r) t_note("%dbpp realize returned %u", bpps[i], r);
        U(SelectPalette)(d.dc, o, FALSE);
        U(DeleteObject)(p);
        dibdc_free(&d);
    }
}

TEST(palette_realize_8bpp_dib) {
    REQUIRE(U(RealizePalette)); REQUIRE(U(SelectPalette)); REQUIRE(U(DeleteObject));
    struct dibdc d;
    if (!dibdc_create(&d, 8, 8, 8)) { t_skip("no 8bpp dib"); return; }
    HPALETTE p = pal_make(64);
    HPALETTE o = U(SelectPalette)(d.dc, p, FALSE);
    UINT r = U(RealizePalette)(d.dc);
    EXPECT_NE(r, GDI_ERROR);
    t_note("RealizePalette(8bpp dib, 64 entries)=%u", r);
    UINT r2 = U(RealizePalette)(d.dc);
    EXPECT_NE(r2, GDI_ERROR);
    t_note("second RealizePalette=%u", r2);
    U(SelectPalette)(d.dc, o, FALSE);
    U(DeleteObject)(p);
    dibdc_free(&d);
}

TEST(palette_realize_8bpp_dib_table_unchanged) {
    REQUIRE(U(RealizePalette)); REQUIRE(U(SelectPalette)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetDIBColorTable));
    struct dibdc d;
    if (!dibdc_create(&d, 8, 8, 8)) { t_skip("no 8bpp dib"); return; }
    RGBQUAD before[256], after[256];
    EXPECT_EQ(U(GetDIBColorTable)(d.dc, 0, 256, before), 256);
    HPALETTE p = pal_make(64);
    HPALETTE o = U(SelectPalette)(d.dc, p, FALSE);
    U(RealizePalette)(d.dc);
    EXPECT_EQ(U(GetDIBColorTable)(d.dc, 0, 256, after), 256);
    int same = !memcmp(before, after, sizeof before);
    t_note("DIB color table %s after RealizePalette", same ? "unchanged" : "changed");
    U(SelectPalette)(d.dc, o, FALSE);
    U(DeleteObject)(p);
    dibdc_free(&d);
}

TEST(palette_realize_screen_dc) {
    REQUIRE(U(RealizePalette)); REQUIRE(U(SelectPalette)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetDeviceCaps));
    HDC dc = pal_screen_dc();
    if (!dc) { t_skip("no screen dc"); return; }
    int rc = U(GetDeviceCaps)(dc, RASTERCAPS);
    HPALETTE p = pal_make(16);
    HPALETTE o = U(SelectPalette)(dc, p, TRUE);
    UINT r = U(RealizePalette)(dc);
    if (!(rc & RC_PALETTE)) {
        /* non-palette display: nothing to map */
        EXPECT_NE(r, GDI_ERROR);
        t_note("RealizePalette on non-palette screen dc=%u", r);
    } else {
        t_note("palette display: RealizePalette=%u", r);
    }
    U(SelectPalette)(dc, o, TRUE);
    U(DeleteObject)(p);
    pal_release_screen_dc(dc);
}

TEST(palette_realize_repeated) {
    REQUIRE(U(RealizePalette)); REQUIRE(U(SelectPalette)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HPALETTE p = pal_make(8);
    HPALETTE o = U(SelectPalette)(dc, p, FALSE);
    for (int i = 0; i < 50; i++) EXPECT_NE(U(RealizePalette)(dc), GDI_ERROR);
    U(SelectPalette)(dc, o, FALSE);
    U(DeleteObject)(p);
    U(DeleteDC)(dc);
}

/* ---- UnrealizeObject ---- */
TEST(palette_unrealize_palette) {
    REQUIRE(U(UnrealizeObject)); REQUIRE(U(DeleteObject));
    HPALETTE p = pal_make(8);
    BOOL r = U(UnrealizeObject)(p);
    t_note("UnrealizeObject(palette)=%d", r);
    CHECK(r);
    EXPECT_EQ(pal_count(p), 8);
    U(DeleteObject)(p);
}

TEST(palette_unrealize_stock_default) {
    REQUIRE(U(UnrealizeObject)); REQUIRE(U(GetStockObject));
    BOOL r = U(UnrealizeObject)(U(GetStockObject)(DEFAULT_PALETTE));
    t_note("UnrealizeObject(default palette)=%d", r);
}

TEST(palette_unrealize_selected) {
    REQUIRE(U(UnrealizeObject)); REQUIRE(U(SelectPalette)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HPALETTE p = pal_make(8);
    HPALETTE o = U(SelectPalette)(dc, p, FALSE);
    BOOL r = U(UnrealizeObject)(p);
    t_note("UnrealizeObject(selected palette)=%d", r);
    EXPECT_EQ(U(SelectPalette)(dc, o, FALSE), p);
    U(DeleteObject)(p);
    U(DeleteDC)(dc);
}

TEST(palette_unrealize_invalid) {
    REQUIRE(U(UnrealizeObject));
    CHECK(!U(UnrealizeObject)(NULL));
    CHECK(!U(UnrealizeObject)((HGDIOBJ)PAL_BOGUS_HANDLE));
    HPALETTE p = pal_make(2);
    U(DeleteObject)(p);
    CHECK(!U(UnrealizeObject)(p));
}

/* ---- UpdateColors ---- */
TEST(palette_updatecolors_invalid_dc) {
    REQUIRE(U(UpdateColors));
    CHECK(!U(UpdateColors)(NULL));
    CHECK(!U(UpdateColors)((HDC)PAL_BOGUS_HANDLE));
    HPALETTE p = pal_make(2);
    CHECK(!U(UpdateColors)((HDC)p));
    U(DeleteObject)(p);
}

TEST(palette_updatecolors_memdc) {
    REQUIRE(U(UpdateColors)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    BOOL r = U(UpdateColors)(dc);
    t_note("UpdateColors(memdc)=%d", r);
    U(DeleteDC)(dc);
    CHECK(!U(UpdateColors)(dc));            /* deleted */
}

TEST(palette_updatecolors_dibs) {
    REQUIRE(U(UpdateColors));
    static const int bpps[] = {1, 8, 32};
    for (unsigned i = 0; i < 3; i++) {
        struct dibdc d;
        if (!dibdc_create(&d, bpps[i], 8, 8)) continue;
        BOOL r = U(UpdateColors)(d.dc);
        t_note("UpdateColors(%dbpp dib)=%d", bpps[i], r);
        dibdc_free(&d);
    }
}

TEST(palette_updatecolors_screen) {
    REQUIRE(U(UpdateColors));
    HDC dc = pal_screen_dc();
    if (!dc) { t_skip("no screen dc"); return; }
    BOOL r = U(UpdateColors)(dc);
    t_note("UpdateColors(screen dc)=%d", r);
    pal_release_screen_dc(dc);
}

/* ---- GetNearestPaletteIndex ---- */
TEST(palette_nearest_index_exact) {
    REQUIRE(U(GetNearestPaletteIndex)); REQUIRE(U(DeleteObject));
    COLORREF c[] = {RGB(0, 0, 0), RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255), RGB(255, 255, 255), RGB(128, 128, 128), RGB(10, 200, 90)};
    HPALETTE p = pal_make_rgb(c, 7);
    for (UINT i = 0; i < 7; i++) {
        UINT r = U(GetNearestPaletteIndex)(p, c[i]);
        CHECKF(r == i, "color %u -> %u", i, r);
    }
    U(DeleteObject)(p);
}

TEST(palette_nearest_index_closest) {
    REQUIRE(U(GetNearestPaletteIndex)); REQUIRE(U(DeleteObject));
    COLORREF c[] = {RGB(0, 0, 0), RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255), RGB(255, 255, 255)};
    HPALETTE p = pal_make_rgb(c, 5);
    struct { COLORREF q; UINT want; } t[] = {
        {RGB(5, 5, 5), 0}, {RGB(250, 10, 10), 1}, {RGB(10, 240, 10), 2}, {RGB(10, 10, 230), 3},
        {RGB(240, 250, 245), 4}, {RGB(200, 20, 0), 1}, {RGB(0, 0, 40), 0}, {RGB(0, 0, 200), 3},
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        UINT r = U(GetNearestPaletteIndex)(p, t[i].q);
        CHECKF(r == t[i].want, "q=%06lx got %u want %u", (unsigned long)t[i].q, r, t[i].want);
    }
    U(DeleteObject)(p);
}

TEST(palette_nearest_index_single_entry) {
    REQUIRE(U(GetNearestPaletteIndex)); REQUIRE(U(DeleteObject));
    COLORREF c = RGB(100, 150, 200);
    HPALETTE p = pal_make_rgb(&c, 1);
    EXPECT_EQ(U(GetNearestPaletteIndex)(p, RGB(0, 0, 0)), 0);
    EXPECT_EQ(U(GetNearestPaletteIndex)(p, RGB(255, 255, 255)), 0);
    EXPECT_EQ(U(GetNearestPaletteIndex)(p, c), 0);
    U(DeleteObject)(p);
}

TEST(palette_nearest_index_duplicates_first) {
    REQUIRE(U(GetNearestPaletteIndex)); REQUIRE(U(DeleteObject));
    COLORREF c[] = {RGB(9, 9, 9), RGB(1, 2, 3), RGB(1, 2, 3), RGB(1, 2, 3)};
    HPALETTE p = pal_make_rgb(c, 4);
    UINT r = U(GetNearestPaletteIndex)(p, RGB(1, 2, 3));
    CHECK(r >= 1 && r <= 3);
    t_note("duplicate exact match index: %u", r);
    U(DeleteObject)(p);
}

TEST(palette_nearest_index_invalid_handle) {
    REQUIRE(U(GetNearestPaletteIndex)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    EXPECT_EQ(U(GetNearestPaletteIndex)(NULL, 0), CLR_INVALID);
    EXPECT_EQ(U(GetNearestPaletteIndex)((HPALETTE)PAL_BOGUS_HANDLE, 0), CLR_INVALID);
    HBRUSH br = U(CreateSolidBrush)(0);
    EXPECT_EQ(U(GetNearestPaletteIndex)((HPALETTE)br, 0), CLR_INVALID);
    U(DeleteObject)(br);
    HPALETTE p = pal_make(2);
    U(DeleteObject)(p);
    EXPECT_EQ(U(GetNearestPaletteIndex)(p, 0), CLR_INVALID);
}

TEST(palette_nearest_index_color_forms) {
    REQUIRE(U(GetNearestPaletteIndex)); REQUIRE(U(DeleteObject));
    COLORREF c[] = {RGB(0, 0, 0), RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255)};
    HPALETTE p = pal_make_rgb(c, 4);
    COLORREF forms[] = {
        0x00ff0000 /* blue */, PALETTERGB(0, 255, 0), 0x01000002 /* PALETTEINDEX(2) */, 0x02ff0000,
        0x10000001, 0x03000000, 0xff0000ff,
    };
    for (unsigned i = 0; i < sizeof forms / sizeof *forms; i++) {
        UINT r = U(GetNearestPaletteIndex)(p, forms[i]);
        t_note("GetNearestPaletteIndex(0x%08lx)=%u", (unsigned long)forms[i], r);
    }
    EXPECT_EQ(U(GetNearestPaletteIndex)(p, 0x00ff0000), 3);
    EXPECT_EQ(U(GetNearestPaletteIndex)(p, PALETTERGB(0, 255, 0)), 2);
    U(DeleteObject)(p);
}

TEST(palette_nearest_index_after_set_entries) {
    REQUIRE(U(GetNearestPaletteIndex)); REQUIRE(U(SetPaletteEntries)); REQUIRE(U(DeleteObject));
    COLORREF c[] = {RGB(0, 0, 0), RGB(255, 255, 255)};
    HPALETTE p = pal_make_rgb(c, 2);
    EXPECT_EQ(U(GetNearestPaletteIndex)(p, RGB(250, 0, 0)), 0);
    PALETTEENTRY red = {255, 0, 0, 0};
    EXPECT_EQ(U(SetPaletteEntries)(p, 1, 1, &red), 1);
    EXPECT_EQ(U(GetNearestPaletteIndex)(p, RGB(250, 0, 0)), 1);
    U(DeleteObject)(p);
}

TEST(palette_nearest_index_after_resize) {
    REQUIRE(U(GetNearestPaletteIndex)); REQUIRE(U(ResizePalette)); REQUIRE(U(DeleteObject));
    COLORREF c[] = {RGB(200, 200, 200), RGB(255, 255, 255)};
    HPALETTE p = pal_make_rgb(c, 2);
    EXPECT_EQ(U(GetNearestPaletteIndex)(p, RGB(0, 0, 0)), 0);
    CHECK(U(ResizePalette)(p, 3));                    /* entry 2 = black */
    EXPECT_EQ(U(GetNearestPaletteIndex)(p, RGB(0, 0, 0)), 2);
    CHECK(U(ResizePalette)(p, 2));
    EXPECT_EQ(U(GetNearestPaletteIndex)(p, RGB(0, 0, 0)), 0);
    U(DeleteObject)(p);
}

TEST(palette_nearest_index_256_greys) {
    REQUIRE(U(GetNearestPaletteIndex)); REQUIRE(U(DeleteObject));
    COLORREF c[256];
    for (int i = 0; i < 256; i++) c[i] = RGB(i, i, i);
    HPALETTE p = pal_make_rgb(c, 256);
    for (int i = 0; i < 256; i += 5) EXPECT_EQ(U(GetNearestPaletteIndex)(p, RGB(i, i, i)), (UINT)i);
    U(DeleteObject)(p);
}

TEST(palette_nearest_index_stock_default) {
    REQUIRE(U(GetNearestPaletteIndex)); REQUIRE(U(GetStockObject));
    HPALETTE d = (HPALETTE)U(GetStockObject)(DEFAULT_PALETTE);
    UINT b = U(GetNearestPaletteIndex)(d, RGB(0, 0, 0));
    UINT w = U(GetNearestPaletteIndex)(d, RGB(255, 255, 255));
    CHECK(b != CLR_INVALID && w != CLR_INVALID);
    EXPECT_NE(b, w);
}

/* ---- GetNearestColor ---- */
TEST(palette_nearestcolor_invalid_dc) {
    REQUIRE(U(GetNearestColor));
    EXPECT_EQ(U(GetNearestColor)(NULL, RGB(1, 2, 3)), CLR_INVALID);
    EXPECT_EQ(U(GetNearestColor)((HDC)PAL_BOGUS_HANDLE, RGB(1, 2, 3)), CLR_INVALID);
    HPALETTE p = pal_make(2);
    EXPECT_EQ(U(GetNearestColor)((HDC)p, RGB(1, 2, 3)), CLR_INVALID);
    U(DeleteObject)(p);
}

TEST(palette_nearestcolor_deleted_dc) {
    REQUIRE(U(GetNearestColor)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    U(DeleteDC)(dc);
    EXPECT_EQ(U(GetNearestColor)(dc, RGB(1, 2, 3)), CLR_INVALID);
}

TEST(palette_nearestcolor_32bpp_identity) {
    REQUIRE(U(GetNearestColor));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 4, 4)) { t_skip("no 32bpp dib"); return; }
    COLORREF cs[] = {0, RGB(1, 2, 3), RGB(255, 255, 255), RGB(18, 52, 86), RGB(255, 0, 128), RGB(127, 128, 129), RGB(0, 0, 1), RGB(1, 0, 0)};
    for (unsigned i = 0; i < sizeof cs / sizeof *cs; i++) EXPECT_EQ(U(GetNearestColor)(d.dc, cs[i]), cs[i]);
    dibdc_free(&d);
}

TEST(palette_nearestcolor_24bpp_identity) {
    REQUIRE(U(GetNearestColor));
    struct dibdc d;
    if (!dibdc_create(&d, 24, 4, 4)) { t_skip("no 24bpp dib"); return; }
    COLORREF cs[] = {0, RGB(1, 2, 3), RGB(255, 255, 255), RGB(18, 52, 86), RGB(250, 3, 99)};
    for (unsigned i = 0; i < sizeof cs / sizeof *cs; i++) EXPECT_EQ(U(GetNearestColor)(d.dc, cs[i]), cs[i]);
    dibdc_free(&d);
}

TEST(palette_nearestcolor_16bpp_rounding) {
    REQUIRE(U(GetNearestColor));
    struct dibdc d;
    if (!dibdc_create(&d, 16, 4, 4)) { t_skip("no 16bpp dib"); return; }
    EXPECT_EQ(U(GetNearestColor)(d.dc, RGB(0, 0, 0)), RGB(0, 0, 0));
    EXPECT_EQ(U(GetNearestColor)(d.dc, RGB(255, 255, 255)), RGB(255, 255, 255));
    COLORREF r = U(GetNearestColor)(d.dc, RGB(1, 2, 3));
    t_note("16bpp nearest of (1,2,3) = %06lx", (unsigned long)r);
    COLORREF r2 = U(GetNearestColor)(d.dc, r);
    EXPECT_EQ(r2, r);                         /* idempotent */
    dibdc_free(&d);
}

TEST(palette_nearestcolor_mono_dc) {
    REQUIRE(U(GetNearestColor)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    EXPECT_EQ(U(GetNearestColor)(dc, RGB(0, 0, 0)), RGB(0, 0, 0));
    EXPECT_EQ(U(GetNearestColor)(dc, RGB(255, 255, 255)), RGB(255, 255, 255));
    EXPECT_EQ(U(GetNearestColor)(dc, RGB(10, 10, 10)), RGB(0, 0, 0));
    EXPECT_EQ(U(GetNearestColor)(dc, RGB(245, 245, 245)), RGB(255, 255, 255));
    COLORREF mid = U(GetNearestColor)(dc, RGB(128, 128, 128));
    CHECK(mid == 0 || mid == RGB(255, 255, 255));
    U(DeleteDC)(dc);
}

TEST(palette_nearestcolor_1bpp_dib) {
    REQUIRE(U(GetNearestColor));
    struct dibdc d;
    if (!dibdc_create(&d, 1, 8, 8)) { t_skip("no 1bpp dib"); return; }
    for (int v = 0; v < 256; v += 17) {
        COLORREF r = U(GetNearestColor)(d.dc, RGB(v, v, v));
        CHECKF(r == 0 || r == RGB(255, 255, 255), "v=%d -> %06lx", v, (unsigned long)r);
    }
    dibdc_free(&d);
}

TEST(palette_nearestcolor_8bpp_exact_table) {
    REQUIRE(U(GetNearestColor));
    struct dibdc d;
    if (!dibdc_create(&d, 8, 8, 8)) { t_skip("no 8bpp dib"); return; }
    COLORREF tab[] = {RGB(0, 0, 0), RGB(200, 10, 10), RGB(10, 200, 10), RGB(10, 10, 200), RGB(255, 255, 255)};
    EXPECT_EQ(dib_set_table(d.dc, tab, 5, 0), 5);
    for (unsigned i = 0; i < 5; i++) {
        COLORREF r = U(GetNearestColor)(d.dc, tab[i]);
        t_note("8bpp nearest(%06lx)=%06lx", (unsigned long)tab[i], (unsigned long)r);
    }
    dibdc_free(&d);
}

TEST(palette_nearestcolor_8bpp_with_palette) {
    REQUIRE(U(GetNearestColor)); REQUIRE(U(SelectPalette)); REQUIRE(U(RealizePalette)); REQUIRE(U(DeleteObject));
    struct dibdc d;
    if (!dibdc_create(&d, 8, 8, 8)) { t_skip("no 8bpp dib"); return; }
    COLORREF c[] = {RGB(0, 0, 0), RGB(128, 0, 0), RGB(0, 128, 0), RGB(255, 255, 255)};
    HPALETTE p = pal_make_rgb(c, 4);
    HPALETTE o = U(SelectPalette)(d.dc, p, FALSE);
    U(RealizePalette)(d.dc);
    COLORREF r = U(GetNearestColor)(d.dc, RGB(120, 5, 5));
    t_note("8bpp+logical palette nearest(120,5,5)=%06lx", (unsigned long)r);
    U(SelectPalette)(d.dc, o, FALSE);
    U(DeleteObject)(p);
    dibdc_free(&d);
}

TEST(palette_nearestcolor_screen) {
    REQUIRE(U(GetNearestColor));
    HDC dc = pal_screen_dc();
    if (!dc) { t_skip("no screen dc"); return; }
    COLORREF r = U(GetNearestColor)(dc, RGB(12, 34, 56));
    CHECK(r != CLR_INVALID);
    t_note("screen nearest(12,34,56)=%06lx", (unsigned long)r);
    pal_release_screen_dc(dc);
}

TEST(palette_nearestcolor_high_byte_forms) {
    REQUIRE(U(GetNearestColor));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 4, 4)) { t_skip("no 32bpp dib"); return; }
    COLORREF forms[] = {0x01000001, 0x02112233, 0x10000005, 0xff112233, 0x00112233};
    for (unsigned i = 0; i < sizeof forms / sizeof *forms; i++)
        t_note("GetNearestColor(32bpp, 0x%08lx)=0x%08lx", (unsigned long)forms[i], (unsigned long)U(GetNearestColor)(d.dc, forms[i]));
    EXPECT_EQ(U(GetNearestColor)(d.dc, 0x00112233), 0x00112233);
    dibdc_free(&d);
}

/* ---- System palette ---- */
TEST(palette_syspal_entries_nonpalette_dc) {
    REQUIRE(U(GetSystemPaletteEntries)); REQUIRE(U(GetDeviceCaps));
    struct dibdc d;
    if (!dibdc_create(&d, 32, 4, 4)) { t_skip("no 32bpp dib"); return; }
    PALETTEENTRY pe[256];
    memset(pe, 0, sizeof pe);
    EXPECT_EQ(U(GetSystemPaletteEntries)(d.dc, 0, 256, pe), 0);
    EXPECT_EQ(U(GetSystemPaletteEntries)(d.dc, 0, 0, NULL), 0);
    EXPECT_EQ(U(GetSystemPaletteEntries)(d.dc, 0, 256, NULL), 0);
    EXPECT_EQ(U(GetSystemPaletteEntries)(d.dc, 300, 4, pe), 0);
    EXPECT_EQ(U(GetSystemPaletteEntries)(d.dc, 0, 0x80000000u, pe), 0);
    dibdc_free(&d);
}

TEST(palette_syspal_entries_invalid_dc) {
    REQUIRE(U(GetSystemPaletteEntries));
    PALETTEENTRY pe[4];
    EXPECT_EQ(U(GetSystemPaletteEntries)(NULL, 0, 4, pe), 0);
    EXPECT_EQ(U(GetSystemPaletteEntries)((HDC)PAL_BOGUS_HANDLE, 0, 4, pe), 0);
    EXPECT_EQ(U(GetSystemPaletteEntries)(NULL, 0, 0, NULL), 0);
}

TEST(palette_syspal_entries_8bpp_dib) {
    REQUIRE(U(GetSystemPaletteEntries)); REQUIRE(U(GetDeviceCaps));
    struct dibdc d;
    if (!dibdc_create(&d, 8, 4, 4)) { t_skip("no 8bpp dib"); return; }
    int caps = U(GetDeviceCaps)(d.dc, RASTERCAPS);
    PALETTEENTRY pe[256];
    UINT r = U(GetSystemPaletteEntries)(d.dc, 0, 256, pe);
    t_note("GetSystemPaletteEntries(8bpp dib) RC_PALETTE=%d -> %u", !!(caps & RC_PALETTE), r);
    if (!(caps & RC_PALETTE)) EXPECT_EQ(r, 0);
    dibdc_free(&d);
}

TEST(palette_syspal_entries_screen) {
    REQUIRE(U(GetSystemPaletteEntries)); REQUIRE(U(GetDeviceCaps));
    HDC dc = pal_screen_dc();
    if (!dc) { t_skip("no screen dc"); return; }
    int caps = U(GetDeviceCaps)(dc, RASTERCAPS);
    PALETTEENTRY pe[256];
    UINT r = U(GetSystemPaletteEntries)(dc, 0, 256, pe);
    if (!(caps & RC_PALETTE)) {
        EXPECT_EQ(r, 0);
        EXPECT_EQ(U(GetSystemPaletteEntries)(dc, 0, 0, NULL), 0);
    } else {
        t_note("palette display: %u entries", r);
        EXPECT_EQ(U(GetSystemPaletteEntries)(dc, 0, 0, NULL), U(GetDeviceCaps)(dc, SIZEPALETTE));
    }
    pal_release_screen_dc(dc);
}

TEST(palette_syspal_use_query) {
    REQUIRE(U(GetSystemPaletteUse));
    HDC dc = pal_screen_dc();
    if (!dc) { t_skip("no screen dc"); return; }
    UINT u = U(GetSystemPaletteUse)(dc);
    t_note("GetSystemPaletteUse(screen)=%u", u);
    CHECK(u <= SYSPAL_NOSTATIC256 || u == SYSPAL_ERROR);
    UINT u2 = U(GetSystemPaletteUse)(dc);
    EXPECT_EQ(u, u2);
    pal_release_screen_dc(dc);
}

TEST(palette_syspal_use_invalid_dc) {
    REQUIRE(U(GetSystemPaletteUse));
    t_note("GetSystemPaletteUse(NULL)=%u bogus=%u", U(GetSystemPaletteUse)(NULL), U(GetSystemPaletteUse)((HDC)PAL_BOGUS_HANDLE));
}

TEST(palette_syspal_use_memdc) {
    REQUIRE(U(GetSystemPaletteUse)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    UINT u = U(GetSystemPaletteUse)(dc);
    t_note("GetSystemPaletteUse(memdc)=%u", u);
    U(DeleteDC)(dc);
}

TEST(palette_syspal_setuse_invalid_dc_only) {
    REQUIRE(U(SetSystemPaletteUse));
    /* never against a real device: these must fail without changing anything */
    EXPECT_EQ(U(SetSystemPaletteUse)(NULL, SYSPAL_STATIC), SYSPAL_ERROR);
    EXPECT_EQ(U(SetSystemPaletteUse)((HDC)PAL_BOGUS_HANDLE, SYSPAL_STATIC), SYSPAL_ERROR);
    EXPECT_EQ(U(SetSystemPaletteUse)(NULL, 77), SYSPAL_ERROR);
}

TEST(palette_syspal_query_unchanged) {
    REQUIRE(U(GetSystemPaletteUse)); REQUIRE(U(SetSystemPaletteUse));
    HDC dc = pal_screen_dc();
    if (!dc) { t_skip("no screen dc"); return; }
    UINT before = U(GetSystemPaletteUse)(dc);
    U(SetSystemPaletteUse)(NULL, SYSPAL_NOSTATIC);
    EXPECT_EQ(U(GetSystemPaletteUse)(dc), before);
    pal_release_screen_dc(dc);
}

/* ---- DIB color table / palette interplay ---- */
TEST(palette_dibtable_set_get_roundtrip) {
    REQUIRE(U(SetDIBColorTable)); REQUIRE(U(GetDIBColorTable));
    struct dibdc d;
    if (!dibdc_create(&d, 8, 8, 8)) { t_skip("no 8bpp dib"); return; }
    RGBQUAD in[256], out[256];
    for (int i = 0; i < 256; i++) { in[i].rgbRed = i; in[i].rgbGreen = 255 - i; in[i].rgbBlue = i ^ 0x33; in[i].rgbReserved = 0; }
    EXPECT_EQ(U(SetDIBColorTable)(d.dc, 0, 256, in), 256);
    memset(out, 0, sizeof out);
    EXPECT_EQ(U(GetDIBColorTable)(d.dc, 0, 256, out), 256);
    CHECK(!memcmp(in, out, sizeof in));
    dibdc_free(&d);
}

TEST(palette_dibtable_ranges_table) {
    REQUIRE(U(SetDIBColorTable)); REQUIRE(U(GetDIBColorTable));
    struct dibdc d;
    if (!dibdc_create(&d, 8, 8, 8)) { t_skip("no 8bpp dib"); return; }
    RGBQUAD q[300];
    memset(q, 0x40, sizeof q);
    struct { UINT s, c, want; } t[] = {{0, 1, 1}, {255, 1, 1}, {255, 2, 1}, {250, 100, 6}, {256, 1, 0}, {1000, 1, 0}, {0, 0, 0}, {0, 256, 256}, {0, 300, 256}};
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        UINT r = U(SetDIBColorTable)(d.dc, t[i].s, t[i].c, q);
        int clamped = (t[i].s + t[i].c > 256);
        if (clamped) { if (r != t[i].want) t_note("set clamp s=%u c=%u got %u want %u", t[i].s, t[i].c, r, t[i].want); }
        else CHECKF(r == t[i].want, "set s=%u c=%u got %u want %u", t[i].s, t[i].c, r, t[i].want);
        r = U(GetDIBColorTable)(d.dc, t[i].s, t[i].c, q);
        if (clamped) { if (r != t[i].want) t_note("get clamp s=%u c=%u got %u want %u", t[i].s, t[i].c, r, t[i].want); }
        else CHECKF(r == t[i].want, "get s=%u c=%u got %u want %u", t[i].s, t[i].c, r, t[i].want);
    }
    dibdc_free(&d);
}

TEST(palette_dibtable_non_palettized) {
    REQUIRE(U(SetDIBColorTable)); REQUIRE(U(GetDIBColorTable));
    static const int bpps[] = {16, 24, 32};
    for (unsigned i = 0; i < 3; i++) {
        struct dibdc d;
        if (!dibdc_create(&d, bpps[i], 4, 4)) continue;
        RGBQUAD q[4] = {{1, 2, 3, 0}};
        EXPECT_EQ(U(SetDIBColorTable)(d.dc, 0, 4, q), 0);
        EXPECT_EQ(U(GetDIBColorTable)(d.dc, 0, 4, q), 0);
        dibdc_free(&d);
    }
}

TEST(palette_dibtable_4bpp_and_1bpp) {
    REQUIRE(U(SetDIBColorTable)); REQUIRE(U(GetDIBColorTable));
    struct dibdc d;
    RGBQUAD q[256];
    memset(q, 0, sizeof q);
    if (dibdc_create(&d, 4, 4, 4)) {
        EXPECT_EQ(U(SetDIBColorTable)(d.dc, 0, 256, q), 16);
        EXPECT_EQ(U(GetDIBColorTable)(d.dc, 0, 256, q), 16);
        EXPECT_EQ(U(GetDIBColorTable)(d.dc, 15, 5, q), 1);
        EXPECT_EQ(U(GetDIBColorTable)(d.dc, 16, 1, q), 0);
        dibdc_free(&d);
    }
    if (dibdc_create(&d, 1, 8, 8)) {
        EXPECT_EQ(U(GetDIBColorTable)(d.dc, 0, 256, q), 2);
        EXPECT_EQ(U(GetDIBColorTable)(d.dc, 1, 5, q), 1);
        dibdc_free(&d);
    }
}

TEST(palette_dibtable_invalid_dc) {
    REQUIRE(U(SetDIBColorTable)); REQUIRE(U(GetDIBColorTable));
    RGBQUAD q[4] = {{0}};
    EXPECT_EQ(U(SetDIBColorTable)(NULL, 0, 4, q), 0);
    EXPECT_EQ(U(GetDIBColorTable)(NULL, 0, 4, q), 0);
    EXPECT_EQ(U(SetDIBColorTable)((HDC)PAL_BOGUS_HANDLE, 0, 4, q), 0);
    EXPECT_EQ(U(GetDIBColorTable)((HDC)PAL_BOGUS_HANDLE, 0, 4, q), 0);
}

TEST(palette_dibtable_no_dib_selected) {
    REQUIRE(U(SetDIBColorTable)); REQUIRE(U(GetDIBColorTable)); REQUIRE(U(CreateCompatibleDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    RGBQUAD q[4] = {{0}};
    EXPECT_EQ(U(SetDIBColorTable)(dc, 0, 2, q), 0);
    EXPECT_EQ(U(GetDIBColorTable)(dc, 0, 2, q), 0);
    U(DeleteDC)(dc);
}

TEST(palette_dibtable_null_buffer) {
    REQUIRE(U(GetDIBColorTable));
    struct dibdc d;
    if (!dibdc_create(&d, 8, 4, 4)) { t_skip("no 8bpp dib"); return; }
    UINT r = U(GetDIBColorTable)(d.dc, 0, 256, NULL);
    t_note("GetDIBColorTable(NULL buffer)=%u", r);
    dibdc_free(&d);
}

TEST(palette_dibtable_default_grey) {
    REQUIRE(U(GetDIBColorTable));
    struct dibdc d;
    if (!dibdc_create(&d, 8, 4, 4)) { t_skip("no 8bpp dib"); return; }
    RGBQUAD q[256];
    EXPECT_EQ(U(GetDIBColorTable)(d.dc, 0, 256, q), 256);
    for (int i = 0; i < 256; i++) {
        if (q[i].rgbRed != i || q[i].rgbGreen != i || q[i].rgbBlue != i) { CHECKF(0, "entry %d", i); break; }
    }
    dibdc_free(&d);
}

/* Exact pixel output on an 8bpp DIB section. */
TEST(palette_pixel_8bpp_exact_color_maps_to_index) {
    REQUIRE(U(SetPixel)); REQUIRE(U(GetPixel));
    struct dibdc d;
    if (!dibdc_create(&d, 8, 8, 8)) { t_skip("no 8bpp dib"); return; }
    COLORREF tab[] = {RGB(0, 0, 0), RGB(200, 10, 10), RGB(10, 200, 10), RGB(10, 10, 200), RGB(255, 255, 255), RGB(77, 88, 99)};
    EXPECT_EQ(dib_set_table(d.dc, tab, 6, 0), 6);
    for (int i = 0; i < 6; i++) {
        U(SetPixelV)(d.dc, i, 0, tab[i]);
        EXPECT_EQ(d.bits[i], i);
        EXPECT_EQ(U(GetPixel)(d.dc, i, 0), tab[i]);
    }
    /* unwritten pixels stay 0 */
    EXPECT_EQ(d.bits[d.stride], 0);
    dibdc_free(&d);
}

TEST(palette_pixel_8bpp_nearest_index) {
    REQUIRE(U(SetPixelV));
    struct dibdc d;
    if (!dibdc_create(&d, 8, 8, 8)) { t_skip("no 8bpp dib"); return; }
    dib_clear_table(d.dc);
    COLORREF tab[] = {RGB(0, 0, 0), RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255), RGB(255, 255, 255)};
    EXPECT_EQ(dib_set_table(d.dc, tab, 5, 0), 5);
    /* entries 5..255 are black too: restrict to non-black queries */
    U(SetPixelV)(d.dc, 0, 0, RGB(250, 10, 10));
    U(SetPixelV)(d.dc, 1, 0, RGB(10, 250, 10));
    U(SetPixelV)(d.dc, 2, 0, RGB(10, 10, 250));
    U(SetPixelV)(d.dc, 3, 0, RGB(250, 250, 250));
    EXPECT_EQ(d.bits[0], 1);
    EXPECT_EQ(d.bits[1], 2);
    EXPECT_EQ(d.bits[2], 3);
    EXPECT_EQ(d.bits[3], 4);
    dibdc_free(&d);
}

TEST(palette_pixel_8bpp_getpixel_returns_table_color) {
    REQUIRE(U(GetPixel));
    struct dibdc d;
    if (!dibdc_create(&d, 8, 8, 8)) { t_skip("no 8bpp dib"); return; }
    COLORREF tab[] = {RGB(10, 20, 30), RGB(40, 50, 60), RGB(70, 80, 90)};
    dib_set_table(d.dc, tab, 3, 0);
    for (int i = 0; i < 3; i++) d.bits[i] = (BYTE)i;
    for (int i = 0; i < 3; i++) EXPECT_EQ(U(GetPixel)(d.dc, i, 0), tab[i]);
    d.bits[0] = 2;
    EXPECT_EQ(U(GetPixel)(d.dc, 0, 0), tab[2]);
    dibdc_free(&d);
}

TEST(palette_pixel_8bpp_setpixel_returns_actual) {
    REQUIRE(U(SetPixel)); REQUIRE(U(GetPixel));
    struct dibdc d;
    if (!dibdc_create(&d, 8, 8, 8)) { t_skip("no 8bpp dib"); return; }
    dib_clear_table(d.dc);
    COLORREF tab[] = {RGB(0, 0, 0), RGB(255, 0, 0), RGB(0, 255, 0)};
    dib_set_table(d.dc, tab, 3, 0);
    COLORREF r = U(SetPixel)(d.dc, 1, 1, RGB(250, 5, 5));
    EXPECT_EQ(r, RGB(255, 0, 0));                   /* closest color actually painted */
    EXPECT_EQ(d.bits[d.stride + 1], 1);
    r = U(SetPixel)(d.dc, 2, 2, RGB(1, 1, 1));
    EXPECT_EQ(r, RGB(0, 0, 0));
    dibdc_free(&d);
}

TEST(palette_pixel_8bpp_dibindex_form) {
    REQUIRE(U(SetPixelV)); REQUIRE(U(GetPixel));
    struct dibdc d;
    if (!dibdc_create(&d, 8, 8, 8)) { t_skip("no 8bpp dib"); return; }
    COLORREF tab[] = {RGB(0, 0, 0), RGB(11, 22, 33), RGB(44, 55, 66), RGB(77, 88, 99)};
    dib_set_table(d.dc, tab, 4, 0);
    U(SetPixelV)(d.dc, 0, 0, 0x10000003);          /* DIBINDEX(3) */
    t_note("DIBINDEX(3) wrote index %u", d.bits[0]);
    U(SetPixelV)(d.dc, 1, 0, 0x10000002);
    t_note("DIBINDEX(2) wrote index %u", d.bits[1]);
    dibdc_free(&d);
}

TEST(palette_pixel_8bpp_paletteindex_form) {
    REQUIRE(U(SetPixelV));
    struct dibdc d;
    if (!dibdc_create(&d, 8, 8, 8)) { t_skip("no 8bpp dib"); return; }
    for (int i = 0; i < 4; i++) {
        U(SetPixelV)(d.dc, i, 0, 0x01000000 | i);
        t_note("PALETTEINDEX(%d) on 8bpp dib wrote index %u", i, d.bits[i]);
    }
    dibdc_free(&d);
}

TEST(palette_pixel_8bpp_paletterbg_form) {
    REQUIRE(U(SetPixelV));
    struct dibdc d;
    if (!dibdc_create(&d, 8, 8, 8)) { t_skip("no 8bpp dib"); return; }
    dib_clear_table(d.dc);
    COLORREF tab[] = {RGB(0, 0, 0), RGB(255, 0, 0), RGB(0, 255, 0)};
    dib_set_table(d.dc, tab, 3, 0);
    U(SetPixelV)(d.dc, 0, 0, PALETTERGB(250, 3, 3));
    U(SetPixelV)(d.dc, 1, 0, PALETTERGB(3, 250, 3));
    EXPECT_EQ(d.bits[0], 1);
    EXPECT_EQ(d.bits[1], 2);
    dibdc_free(&d);
}

TEST(palette_pixel_8bpp_logical_palette_selected) {
    REQUIRE(U(SetPixelV)); REQUIRE(U(SelectPalette)); REQUIRE(U(RealizePalette)); REQUIRE(U(DeleteObject));
    struct dibdc d;
    if (!dibdc_create(&d, 8, 8, 8)) { t_skip("no 8bpp dib"); return; }
    COLORREF c[] = {RGB(0, 0, 0), RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255)};
    HPALETTE p = pal_make_rgb(c, 4);
    HPALETTE o = U(SelectPalette)(d.dc, p, FALSE);
    U(RealizePalette)(d.dc);
    for (int i = 0; i < 4; i++) {
        U(SetPixelV)(d.dc, i, 0, 0x01000000 | i);       /* PALETTEINDEX(i) */
        t_note("PALETTEINDEX(%d) with logical palette -> dib index %u", i, d.bits[i]);
    }
    U(SetPixelV)(d.dc, 4, 0, RGB(255, 0, 0));
    t_note("RGB red with logical palette -> dib index %u", d.bits[4]);
    U(SelectPalette)(d.dc, o, FALSE);
    U(DeleteObject)(p);
    dibdc_free(&d);
}

TEST(palette_dibtable_vs_halftone_realize) {
    REQUIRE(U(CreateHalftonePalette)); REQUIRE(U(SelectPalette)); REQUIRE(U(RealizePalette)); REQUIRE(U(DeleteObject));
    struct dibdc d;
    if (!dibdc_create(&d, 8, 8, 8)) { t_skip("no 8bpp dib"); return; }
    HPALETTE h = U(CreateHalftonePalette)(d.dc);
    CHECK(h != NULL);
    if (h) {
        HPALETTE o = U(SelectPalette)(d.dc, h, FALSE);
        UINT r = U(RealizePalette)(d.dc);
        EXPECT_NE(r, GDI_ERROR);
        U(SelectPalette)(d.dc, o, FALSE);
        U(DeleteObject)(h);
    }
    dibdc_free(&d);
}
