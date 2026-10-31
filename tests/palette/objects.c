#include "pal.h"

/* ---- CreatePalette ---- */
TEST(palette_create_null) {
    REQUIRE(U(CreatePalette));
    SetLastError(0xdeadbeef);
    HPALETTE h = U(CreatePalette)(NULL);
    EXPECT_EQ(h, NULL);
    EXPECT_ERR(h, ERROR_INVALID_PARAMETER);
}

TEST(palette_create_counts_table) {
    REQUIRE(U(CreatePalette)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectType));
    static const unsigned counts[] = {1, 2, 3, 15, 16, 17, 20, 127, 128, 255, 256};
    for (unsigned i = 0; i < sizeof counts / sizeof *counts; i++) {
        HPALETTE h = pal_make(counts[i]);
        CHECKF(h != NULL, "n=%u", counts[i]);
        if (!h) continue;
        EXPECT_EQ(U(GetObjectType)(h), OBJ_PAL);
        EXPECT_EQ(pal_count(h), counts[i]);
        CHECK(U(DeleteObject)(h));
    }
}

TEST(palette_create_257) {
    REQUIRE(U(CreatePalette)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(257);
    CHECK(h != NULL);
    if (h) {
        /* Logical palettes may exceed 256 entries. */
        int n = pal_count(h);
        CHECKF(n == 257, "n=%d", n);
        CHECK(U(DeleteObject)(h));
    }
}

TEST(palette_create_large_counts) {
    REQUIRE(U(CreatePalette)); REQUIRE(U(DeleteObject));
    static const unsigned counts[] = {1000, 4096, 32767, 65535};
    for (unsigned i = 0; i < sizeof counts / sizeof *counts; i++) {
        SetLastError(0);
        HPALETTE h = pal_make(counts[i]);
        if (!h) { t_note("CreatePalette(%u) failed, err=%lu", counts[i], GetLastError()); continue; }
        int n = pal_count(h);
        if (n != (int)counts[i]) t_note("CreatePalette(%u): GetPaletteEntries count=%d", counts[i], n);
        CHECK(U(DeleteObject)(h));
    }
}

TEST(palette_create_zero_entries) {
    REQUIRE(U(CreatePalette)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(0);
    t_note("CreatePalette(0 entries) -> %p err=%lu", (void *)h, GetLastError());
    if (h) {
        int n = pal_count(h);
        t_note("  entries=%d", n);
        CHECK(U(DeleteObject)(h));
    }
}

TEST(palette_create_version_values) {
    REQUIRE(U(CreatePalette)); REQUIRE(U(DeleteObject));
    static const WORD vers[] = {0x300, 0x0, 0x1, 0x299, 0x301, 0x400, 0xffff};
    for (unsigned i = 0; i < sizeof vers / sizeof *vers; i++) {
        LOGPALETTE lp;
        memset(&lp, 0, sizeof lp);
        lp.palVersion = vers[i];
        lp.palNumEntries = 1;
        lp.palPalEntry[0].peRed = 1;
        HPALETTE h = U(CreatePalette)(&lp);
        if (vers[i] == 0x300) CHECK(h != NULL);
        else t_note("version 0x%x -> %p", vers[i], (void *)h);
        if (h) U(DeleteObject)(h);
    }
}

TEST(palette_create_preserves_flags) {
    REQUIRE(U(CreatePalette)); REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    static const BYTE flags[] = {0, PC_RESERVED, PC_EXPLICIT, PC_NOCOLLAPSE, PC_RESERVED | PC_NOCOLLAPSE, 0x80, 0xff};
    for (unsigned i = 0; i < sizeof flags; i++) {
        HPALETTE h = pal_make_flags(4, flags[i]);
        if (!h) { t_note("flags 0x%x rejected", flags[i]); continue; }
        PALETTEENTRY pe[4];
        memset(pe, 0xcc, sizeof pe);
        EXPECT_EQ(U(GetPaletteEntries)(h, 0, 4, pe), 4);
        for (int k = 0; k < 4; k++) {
            EXPECT_EQ(pe[k].peRed, k);
            EXPECT_EQ(pe[k].peGreen, 3 * k);
            EXPECT_EQ(pe[k].peBlue, 7 * k);
            if (pe[k].peFlags != flags[i]) t_note("flags 0x%x read back as 0x%x", flags[i], pe[k].peFlags);
        }
        /* The common documented flags must round trip. */
        if (flags[i] <= (PC_RESERVED | PC_NOCOLLAPSE | PC_EXPLICIT)) EXPECT_EQ(pe[0].peFlags, flags[i]);
        U(DeleteObject)(h);
    }
}

TEST(palette_create_independent_copy) {
    REQUIRE(U(CreatePalette)); REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    LOGPALETTE *lp = (LOGPALETTE *)calloc(1, sizeof(LOGPALETTE) + 3 * sizeof(PALETTEENTRY));
    lp->palVersion = 0x300; lp->palNumEntries = 4;
    for (int i = 0; i < 4; i++) lp->palPalEntry[i].peRed = (BYTE)(10 + i);
    HPALETTE h = U(CreatePalette)(lp);
    CHECK(h != NULL);
    lp->palPalEntry[0].peRed = 99;   /* caller's copy changes after creation */
    PALETTEENTRY pe;
    EXPECT_EQ(U(GetPaletteEntries)(h, 0, 1, &pe), 1);
    EXPECT_EQ(pe.peRed, 10);
    if (h) U(DeleteObject)(h);
    free(lp);
}

TEST(palette_create_distinct_handles) {
    REQUIRE(U(CreatePalette)); REQUIRE(U(DeleteObject));
    HPALETTE a = pal_make(4), b = pal_make(4);
    CHECK(a && b);
    CHECK(a != b);
    if (a) U(DeleteObject)(a);
    if (b) U(DeleteObject)(b);
}

/* ---- GetObject / object type ---- */
TEST(palette_getobject_size) {
    REQUIRE(U(GetObjectW)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(37);
    CHECK(h != NULL);
    WORD w = 0xffff;
    EXPECT_EQ(U(GetObjectW)(h, sizeof w, &w), sizeof(WORD));
    EXPECT_EQ(w, 37);
    if (h) U(DeleteObject)(h);
}

TEST(palette_getobject_buffer_sizes) {
    REQUIRE(U(GetObjectW)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(9);
    BYTE buf[16];
    int r;
    memset(buf, 0xaa, sizeof buf);
    r = U(GetObjectW)(h, 0, NULL);
    t_note("GetObject(pal,0,NULL)=%d", r);
    if (r) EXPECT_EQ(r, 2);
    r = U(GetObjectW)(h, 16, buf);
    EXPECT_EQ(r, 2);
    EXPECT_EQ(*(WORD *)buf, 9);
    EXPECT_EQ(buf[2], 0xaa);               /* nothing written beyond a WORD */
    memset(buf, 0xaa, sizeof buf);
    r = U(GetObjectW)(h, 1, buf);
    t_note("GetObject(pal,1,buf)=%d buf0=%02x", r, buf[0]);
    r = U(GetObjectW)(h, 0, buf);
    t_note("GetObject(pal,0,buf)=%d", r);
    if (h) U(DeleteObject)(h);
}

TEST(palette_getobject_a_w_parity) {
    REQUIRE(U(GetObjectA)); REQUIRE(U(GetObjectW)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(11);
    WORD a = 0, w = 0;
    EXPECT_EQ(U(GetObjectA)(h, 2, &a), 2);
    EXPECT_EQ(U(GetObjectW)(h, 2, &w), 2);
    EXPECT_EQ(a, w);
    EXPECT_EQ(a, 11);
    if (h) U(DeleteObject)(h);
}

TEST(palette_getobject_after_resize) {
    REQUIRE(U(GetObjectW)); REQUIRE(U(ResizePalette)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(8);
    WORD w = 0;
    CHECK(U(ResizePalette)(h, 20));
    EXPECT_EQ(U(GetObjectW)(h, 2, &w), 2);
    EXPECT_EQ(w, 20);
    CHECK(U(ResizePalette)(h, 3));
    EXPECT_EQ(U(GetObjectW)(h, 2, &w), 2);
    EXPECT_EQ(w, 3);
    if (h) U(DeleteObject)(h);
}

TEST(palette_getobjecttype) {
    REQUIRE(U(GetObjectType)); REQUIRE(U(GetStockObject)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(2);
    EXPECT_EQ(U(GetObjectType)(h), OBJ_PAL);
    EXPECT_EQ(U(GetObjectType)(U(GetStockObject)(DEFAULT_PALETTE)), OBJ_PAL);
    if (h) U(DeleteObject)(h);
    SetLastError(0);
    EXPECT_EQ(U(GetObjectType)(h), 0);
}

TEST(palette_stock_default_palette) {
    REQUIRE(U(GetStockObject)); REQUIRE(U(GetObjectW));
    HGDIOBJ a = U(GetStockObject)(DEFAULT_PALETTE), b = U(GetStockObject)(DEFAULT_PALETTE);
    CHECK(a != NULL);
    EXPECT_EQ(a, b);
    WORD w = 0;
    EXPECT_EQ(U(GetObjectW)(a, 2, &w), 2);
    if (w != 20) t_note("default palette entries: %u (expected 20 on non-palette display)", w);
    CHECK(w >= 1);
}

TEST(palette_stock_default_entries) {
    REQUIRE(U(GetStockObject)); REQUIRE(U(GetPaletteEntries));
    HPALETTE p = (HPALETTE)U(GetStockObject)(DEFAULT_PALETTE);
    PALETTEENTRY pe[20];
    memset(pe, 0, sizeof pe);
    UINT n = U(GetPaletteEntries)(p, 0, 20, pe);
    t_note("default palette GetPaletteEntries -> %u", n);
    if (n == 20) {
        /* The first and last static colors are black and white. */
        if (pe[0].peRed || pe[0].peGreen || pe[0].peBlue) t_note("entry 0 not black");
        if (pe[19].peRed != 255 || pe[19].peGreen != 255 || pe[19].peBlue != 255) t_note("entry 19 not white");
    }
}

/* ---- DeleteObject ---- */
TEST(palette_delete_twice) {
    REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(4);
    SetLastError(0);
    CHECK(U(DeleteObject)(h));
    SetLastError(0);
    CHECK(!U(DeleteObject)(h));
}

TEST(palette_delete_bogus_handle) {
    REQUIRE(U(DeleteObject));
    SetLastError(0);
    CHECK(!U(DeleteObject)((HGDIOBJ)PAL_BOGUS_HANDLE));
    CHECK(!U(DeleteObject)(NULL));
}

TEST(palette_delete_stock) {
    REQUIRE(U(DeleteObject)); REQUIRE(U(GetStockObject)); REQUIRE(U(GetObjectType));
    HGDIOBJ s = U(GetStockObject)(DEFAULT_PALETTE);
    BOOL r = U(DeleteObject)(s);
    t_note("DeleteObject(stock default palette)=%d", r);
    EXPECT_EQ(U(GetObjectType)(s), OBJ_PAL);   /* must remain usable */
}

TEST(palette_handle_invalid_after_delete) {
    REQUIRE(U(GetPaletteEntries)); REQUIRE(U(SetPaletteEntries)); REQUIRE(U(ResizePalette));
    REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectW));
    HPALETTE h = pal_make(4);
    PALETTEENTRY pe = {1, 2, 3, 0};
    CHECK(U(DeleteObject)(h));
    EXPECT_EQ(U(GetPaletteEntries)(h, 0, 1, &pe), 0);
    EXPECT_EQ(U(GetPaletteEntries)(h, 0, 0, NULL), 0);
    EXPECT_EQ(U(SetPaletteEntries)(h, 0, 1, &pe), 0);
    CHECK(!U(ResizePalette)(h, 8));
    WORD w;
    EXPECT_EQ(U(GetObjectW)(h, 2, &w), 0);
}

TEST(palette_leak_create_delete) {
    REQUIRE(U(GetGuiResources)); REQUIRE(U(DeleteObject));
    HPALETTE hs[100];
    DWORD before = gdi_count();
    if (!before) { t_skip("GetGuiResources unsupported"); return; }
    for (int i = 0; i < 100; i++) hs[i] = pal_make(8);
    DWORD mid = gdi_count();
    EXPECT_EQ(mid - before, 100);
    for (int i = 0; i < 100; i++) CHECK(U(DeleteObject)(hs[i]));
    EXPECT_EQ(gdi_count(), before);
}

TEST(palette_leak_repeat_resize) {
    REQUIRE(U(GetGuiResources)); REQUIRE(U(ResizePalette)); REQUIRE(U(DeleteObject));
    DWORD before = gdi_count();
    if (!before) { t_skip("GetGuiResources unsupported"); return; }
    HPALETTE h = pal_make(4);
    for (int i = 0; i < 200; i++) U(ResizePalette)(h, 4 + (i % 50));
    EXPECT_EQ(gdi_count(), before + 1);
    U(DeleteObject)(h);
    EXPECT_EQ(gdi_count(), before);
}

TEST(palette_leak_halftone) {
    REQUIRE(U(GetGuiResources)); REQUIRE(U(CreateHalftonePalette)); REQUIRE(U(DeleteObject));
    HDC dc = pal_screen_dc();
    if (!dc) { t_skip("no screen dc"); return; }
    DWORD before = gdi_count();
    for (int i = 0; i < 20; i++) {
        HPALETTE h = U(CreateHalftonePalette)(dc);
        CHECK(h != NULL);
        if (h) U(DeleteObject)(h);
    }
    EXPECT_EQ(gdi_count(), before);
    pal_release_screen_dc(dc);
}

/* ---- GetPaletteEntries ---- */
TEST(palette_get_count_query) {
    REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    static const unsigned counts[] = {1, 5, 16, 100, 256, 300};
    for (unsigned i = 0; i < sizeof counts / sizeof *counts; i++) {
        HPALETTE h = pal_make(counts[i]);
        EXPECT_EQ(U(GetPaletteEntries)(h, 0, 0, NULL), counts[i]);
        EXPECT_EQ(U(GetPaletteEntries)(h, 0, 1000, NULL), counts[i]);
        if (h) U(DeleteObject)(h);
    }
}

TEST(palette_get_count_query_with_start) {
    REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(10);
    UINT r = U(GetPaletteEntries)(h, 4, 0, NULL);
    t_note("GetPaletteEntries(start=4,0,NULL)=%u", r);
    r = U(GetPaletteEntries)(h, 50, 0, NULL);
    t_note("GetPaletteEntries(start=50,0,NULL)=%u", r);
    U(DeleteObject)(h);
}

TEST(palette_get_ranges_table) {
    REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    struct { UINT start, cnt, want; } tab[] = {
        {0, 1, 1}, {0, 10, 10}, {0, 11, 10}, {0, 1000, 10}, {9, 1, 1}, {9, 2, 1}, {9, 100, 1},
        {5, 5, 5}, {5, 6, 5}, {10, 1, 0}, {11, 1, 0}, {100, 5, 0}, {0xffff, 5, 0},
    };
    HPALETTE h = pal_make(10);
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        PALETTEENTRY pe[1100];
        memset(pe, 0, sizeof pe);
        UINT r = U(GetPaletteEntries)(h, tab[i].start, tab[i].cnt > 1000 ? 1000 : tab[i].cnt, pe);
        CHECKF(r == tab[i].want, "start=%u cnt=%u got %u want %u", tab[i].start, tab[i].cnt, r, tab[i].want);
        for (UINT k = 0; k < r; k++) {
            EXPECT_EQ(pe[k].peRed, tab[i].start + k);
            EXPECT_EQ(pe[k].peGreen, (BYTE)(3 * (tab[i].start + k)));
        }
    }
    U(DeleteObject)(h);
}

TEST(palette_get_buffer_untouched_past_end) {
    REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(4);
    PALETTEENTRY pe[8];
    memset(pe, 0x5a, sizeof pe);
    EXPECT_EQ(U(GetPaletteEntries)(h, 2, 6, pe), 2);
    EXPECT_EQ(pe[0].peRed, 2);
    EXPECT_EQ(pe[1].peRed, 3);
    for (int i = 2; i < 8; i++) EXPECT_EQ(*(DWORD *)&pe[i], 0x5a5a5a5a);
    U(DeleteObject)(h);
}

TEST(palette_get_start_beyond_untouched) {
    REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(4);
    PALETTEENTRY pe[2];
    memset(pe, 0x77, sizeof pe);
    EXPECT_EQ(U(GetPaletteEntries)(h, 4, 2, pe), 0);
    EXPECT_EQ(*(DWORD *)&pe[0], 0x77777777);
    U(DeleteObject)(h);
}

TEST(palette_get_zero_count_with_buffer) {
    REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(4);
    PALETTEENTRY pe;
    memset(&pe, 0x33, sizeof pe);
    UINT r = U(GetPaletteEntries)(h, 0, 0, &pe);
    t_note("GetPaletteEntries(0,0,buf)=%u", r);
    EXPECT_EQ(*(DWORD *)&pe, 0x33333333);
    U(DeleteObject)(h);
}

TEST(palette_get_invalid_handles) {
    REQUIRE(U(GetPaletteEntries)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject)); REQUIRE(U(CreateCompatibleDC));
    PALETTEENTRY pe[2];
    EXPECT_EQ(U(GetPaletteEntries)(NULL, 0, 2, pe), 0);
    EXPECT_EQ(U(GetPaletteEntries)(NULL, 0, 0, NULL), 0);
    EXPECT_EQ(U(GetPaletteEntries)((HPALETTE)PAL_BOGUS_HANDLE, 0, 2, pe), 0);
    HBRUSH br = U(CreateSolidBrush)(RGB(1, 2, 3));
    EXPECT_EQ(U(GetPaletteEntries)((HPALETTE)br, 0, 2, pe), 0);
    EXPECT_EQ(U(GetPaletteEntries)((HPALETTE)br, 0, 0, NULL), 0);
    U(DeleteObject)(br);
    HDC dc = U(CreateCompatibleDC)(NULL);
    EXPECT_EQ(U(GetPaletteEntries)((HPALETTE)dc, 0, 2, pe), 0);
    U(DeleteDC)(dc);
}

TEST(palette_get_wrong_type_table) {
    REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    REQUIRE(U(CreatePen)); REQUIRE(U(CreateRectRgn)); REQUIRE(U(CreateBitmap)); REQUIRE(U(GetStockObject));
    HGDIOBJ objs[5];
    objs[0] = U(CreatePen)(PS_SOLID, 1, 0);
    objs[1] = U(CreateRectRgn)(0, 0, 2, 2);
    objs[2] = U(CreateBitmap)(2, 2, 1, 1, NULL);
    objs[3] = U(GetStockObject)(SYSTEM_FONT);
    objs[4] = U(GetStockObject)(WHITE_BRUSH);
    PALETTEENTRY pe[2];
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(U(GetPaletteEntries)((HPALETTE)objs[i], 0, 2, pe), 0);
        EXPECT_EQ(U(GetPaletteEntries)((HPALETTE)objs[i], 0, 0, NULL), 0);
    }
    for (int i = 0; i < 3; i++) U(DeleteObject)(objs[i]);
}

TEST(palette_get_null_buffer_nonzero_count) {
    REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(12);
    /* lppe NULL always means: return total entry count. */
    EXPECT_EQ(U(GetPaletteEntries)(h, 0, 5, NULL), 12);
    EXPECT_EQ(U(GetPaletteEntries)(h, 3, 5, NULL), 12);
    U(DeleteObject)(h);
}

/* ---- SetPaletteEntries ---- */
TEST(palette_set_basic) {
    REQUIRE(U(SetPaletteEntries)); REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(8);
    PALETTEENTRY in[3] = {{10, 20, 30, 0}, {40, 50, 60, 0}, {70, 80, 90, 0}}, out[3];
    EXPECT_EQ(U(SetPaletteEntries)(h, 2, 3, in), 3);
    memset(out, 0, sizeof out);
    EXPECT_EQ(U(GetPaletteEntries)(h, 2, 3, out), 3);
    CHECK(!memcmp(in, out, sizeof in));
    /* neighbours untouched */
    PALETTEENTRY n;
    EXPECT_EQ(U(GetPaletteEntries)(h, 1, 1, &n), 1);
    EXPECT_EQ(n.peRed, 1);
    EXPECT_EQ(U(GetPaletteEntries)(h, 5, 1, &n), 1);
    EXPECT_EQ(n.peRed, 5);
    U(DeleteObject)(h);
}

TEST(palette_set_ranges_table) {
    REQUIRE(U(SetPaletteEntries)); REQUIRE(U(DeleteObject));
    struct { UINT start, cnt, want; } tab[] = {
        {0, 1, 1}, {0, 10, 10}, {0, 11, 10}, {0, 100, 10}, {9, 1, 1}, {9, 3, 1}, {5, 5, 5}, {5, 9, 5},
        {10, 1, 0}, {10, 0, 0}, {20, 3, 0}, {0xfffe, 1, 0},
    };
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        HPALETTE h = pal_make(10);
        PALETTEENTRY in[128];
        memset(in, 0x11, sizeof in);
        UINT r = U(SetPaletteEntries)(h, tab[i].start, tab[i].cnt, in);
        CHECKF(r == tab[i].want, "start=%u cnt=%u got %u want %u", tab[i].start, tab[i].cnt, r, tab[i].want);
        U(DeleteObject)(h);
    }
}

TEST(palette_set_zero_count) {
    REQUIRE(U(SetPaletteEntries)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(4);
    PALETTEENTRY pe = {1, 2, 3, 0};
    EXPECT_EQ(U(SetPaletteEntries)(h, 0, 0, &pe), 0);
    U(DeleteObject)(h);
}

TEST(palette_set_preserves_flags) {
    REQUIRE(U(SetPaletteEntries)); REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(4);
    static const BYTE fl[4] = {PC_RESERVED, PC_EXPLICIT, PC_NOCOLLAPSE, 0};
    PALETTEENTRY in[4], out[4];
    for (int i = 0; i < 4; i++) { in[i].peRed = 100 + i; in[i].peGreen = 0; in[i].peBlue = 255 - i; in[i].peFlags = fl[i]; }
    EXPECT_EQ(U(SetPaletteEntries)(h, 0, 4, in), 4);
    EXPECT_EQ(U(GetPaletteEntries)(h, 0, 4, out), 4);
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(out[i].peRed, in[i].peRed);
        EXPECT_EQ(out[i].peBlue, in[i].peBlue);
        EXPECT_EQ(out[i].peFlags, fl[i]);
    }
    U(DeleteObject)(h);
}

TEST(palette_set_invalid_handles) {
    REQUIRE(U(SetPaletteEntries)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    PALETTEENTRY pe = {1, 2, 3, 0};
    EXPECT_EQ(U(SetPaletteEntries)(NULL, 0, 1, &pe), 0);
    EXPECT_EQ(U(SetPaletteEntries)((HPALETTE)PAL_BOGUS_HANDLE, 0, 1, &pe), 0);
    HBRUSH br = U(CreateSolidBrush)(0);
    EXPECT_EQ(U(SetPaletteEntries)((HPALETTE)br, 0, 1, &pe), 0);
    U(DeleteObject)(br);
}

TEST(palette_set_stock_default) {
    REQUIRE(U(SetPaletteEntries)); REQUIRE(U(GetStockObject)); REQUIRE(U(GetPaletteEntries));
    HPALETTE p = (HPALETTE)U(GetStockObject)(DEFAULT_PALETTE);
    PALETTEENTRY orig, mod = {1, 2, 3, 0}, now;
    CHECK(U(GetPaletteEntries)(p, 5, 1, &orig) == 1 || 1);
    UINT r = U(SetPaletteEntries)(p, 5, 1, &mod);
    t_note("SetPaletteEntries(default palette)=%u", r);
    if (r) U(SetPaletteEntries)(p, 5, 1, &orig);       /* restore */
    U(GetPaletteEntries)(p, 5, 1, &now);
}

TEST(palette_set_roundtrip_all_values) {
    REQUIRE(U(SetPaletteEntries)); REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(256);
    PALETTEENTRY in[256], out[256];
    for (int i = 0; i < 256; i++) { in[i].peRed = (BYTE)i; in[i].peGreen = (BYTE)(255 - i); in[i].peBlue = (BYTE)(i ^ 0x5a); in[i].peFlags = 0; }
    EXPECT_EQ(U(SetPaletteEntries)(h, 0, 256, in), 256);
    EXPECT_EQ(U(GetPaletteEntries)(h, 0, 256, out), 256);
    CHECK(!memcmp(in, out, sizeof in));
    U(DeleteObject)(h);
}

TEST(palette_set_one_by_one) {
    REQUIRE(U(SetPaletteEntries)); REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(32);
    for (UINT i = 0; i < 32; i++) {
        PALETTEENTRY pe = {(BYTE)(200 - i), (BYTE)i, (BYTE)(i * 5), 0};
        EXPECT_EQ(U(SetPaletteEntries)(h, i, 1, &pe), 1);
    }
    for (UINT i = 0; i < 32; i++) {
        PALETTEENTRY pe;
        EXPECT_EQ(U(GetPaletteEntries)(h, i, 1, &pe), 1);
        EXPECT_EQ(pe.peRed, (BYTE)(200 - i));
        EXPECT_EQ(pe.peBlue, (BYTE)(i * 5));
    }
    U(DeleteObject)(h);
}

/* ---- ResizePalette ---- */
TEST(palette_resize_grow_zero_fill) {
    REQUIRE(U(ResizePalette)); REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make_flags(4, PC_RESERVED);
    CHECK(U(ResizePalette)(h, 10));
    EXPECT_EQ(pal_count(h), 10);
    PALETTEENTRY pe[10];
    memset(pe, 0xcc, sizeof pe);
    EXPECT_EQ(U(GetPaletteEntries)(h, 0, 10, pe), 10);
    for (int i = 0; i < 4; i++) { EXPECT_EQ(pe[i].peRed, i); EXPECT_EQ(pe[i].peFlags, PC_RESERVED); }
    for (int i = 4; i < 10; i++) {
        EXPECT_EQ(*(DWORD *)&pe[i], 0);
    }
    U(DeleteObject)(h);
}

TEST(palette_resize_shrink_keeps_prefix) {
    REQUIRE(U(ResizePalette)); REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(20);
    CHECK(U(ResizePalette)(h, 6));
    EXPECT_EQ(pal_count(h), 6);
    PALETTEENTRY pe[6];
    EXPECT_EQ(U(GetPaletteEntries)(h, 0, 20, pe), 6);
    for (int i = 0; i < 6; i++) { EXPECT_EQ(pe[i].peRed, i); EXPECT_EQ(pe[i].peBlue, (BYTE)(7 * i)); }
    EXPECT_EQ(U(GetPaletteEntries)(h, 6, 1, pe), 0);
    U(DeleteObject)(h);
}

TEST(palette_resize_shrink_then_grow_zero) {
    REQUIRE(U(ResizePalette)); REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(10);
    CHECK(U(ResizePalette)(h, 3));
    CHECK(U(ResizePalette)(h, 10));
    PALETTEENTRY pe[10];
    EXPECT_EQ(U(GetPaletteEntries)(h, 0, 10, pe), 10);
    for (int i = 0; i < 3; i++) EXPECT_EQ(pe[i].peRed, i);
    for (int i = 3; i < 10; i++) EXPECT_EQ(*(DWORD *)&pe[i], 0);   /* grown entries are black, not the old data */
    U(DeleteObject)(h);
}

TEST(palette_resize_same_size) {
    REQUIRE(U(ResizePalette)); REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(10);
    CHECK(U(ResizePalette)(h, 10));
    EXPECT_EQ(pal_count(h), 10);
    PALETTEENTRY pe;
    EXPECT_EQ(U(GetPaletteEntries)(h, 9, 1, &pe), 1);
    EXPECT_EQ(pe.peRed, 9);
    U(DeleteObject)(h);
}

TEST(palette_resize_table) {
    REQUIRE(U(ResizePalette)); REQUIRE(U(DeleteObject));
    static const unsigned sizes[] = {1, 2, 255, 256, 257, 1000, 5000, 1, 100};
    HPALETTE h = pal_make(50);
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        BOOL r = U(ResizePalette)(h, sizes[i]);
        CHECKF(r, "ResizePalette(%u)", sizes[i]);
        if (r) EXPECT_EQ(pal_count(h), sizes[i]);
    }
    U(DeleteObject)(h);
}

TEST(palette_resize_zero) {
    REQUIRE(U(ResizePalette)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(5);
    SetLastError(0);
    BOOL r = U(ResizePalette)(h, 0);
    t_note("ResizePalette(h,0)=%d err=%lu count=%d", r, GetLastError(), pal_count(h));
    U(DeleteObject)(h);
}

TEST(palette_resize_huge) {
    REQUIRE(U(ResizePalette)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(5);
    static const UINT huge[] = {0x10000, 0x10001, 0x7fffffff, 0x80000000, 0xffffffff};
    for (unsigned i = 0; i < sizeof huge / sizeof *huge; i++) {
        SetLastError(0);
        BOOL r = U(ResizePalette)(h, huge[i]);
        t_note("ResizePalette(h,0x%x)=%d count=%d", huge[i], r, pal_count(h));
        if (r) break;
    }
    U(DeleteObject)(h);
}

TEST(palette_resize_invalid_handles) {
    REQUIRE(U(ResizePalette)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetStockObject));
    CHECK(!U(ResizePalette)(NULL, 4));
    CHECK(!U(ResizePalette)((HPALETTE)PAL_BOGUS_HANDLE, 4));
    HBRUSH br = U(CreateSolidBrush)(0);
    CHECK(!U(ResizePalette)((HPALETTE)br, 4));
    U(DeleteObject)(br);
    BOOL r = U(ResizePalette)((HPALETTE)U(GetStockObject)(DEFAULT_PALETTE), 30);
    t_note("ResizePalette(stock default)=%d", r);
    if (r) U(ResizePalette)((HPALETTE)U(GetStockObject)(DEFAULT_PALETTE), 20);
}

TEST(palette_resize_repeated_alternating) {
    REQUIRE(U(ResizePalette)); REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(8);
    for (int i = 0; i < 100; i++) {
        CHECK(U(ResizePalette)(h, (i & 1) ? 8 : 64));
    }
    EXPECT_EQ(pal_count(h), 8);
    PALETTEENTRY pe;
    EXPECT_EQ(U(GetPaletteEntries)(h, 7, 1, &pe), 1);
    t_note("entry 7 after alternating resize: r=%u", pe.peRed);
    U(DeleteObject)(h);
}

/* ---- AnimatePalette ---- */
TEST(palette_animate_non_reserved_unaffected) {
    REQUIRE(U(AnimatePalette)); REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make_flags(8, 0);
    PALETTEENTRY in[4], out[4];
    for (int i = 0; i < 4; i++) { in[i].peRed = 250; in[i].peGreen = 251; in[i].peBlue = 252; in[i].peFlags = 0; }
    BOOL r = U(AnimatePalette)(h, 2, 4, in);
    t_note("AnimatePalette(non-reserved)=%d", r);
    EXPECT_EQ(U(GetPaletteEntries)(h, 2, 4, out), 4);
    for (int i = 0; i < 4; i++) EXPECT_EQ(out[i].peRed, 2 + i);
    U(DeleteObject)(h);
}

TEST(palette_animate_reserved_changes) {
    REQUIRE(U(AnimatePalette)); REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make_flags(8, PC_RESERVED);
    PALETTEENTRY in[2], out[2];
    for (int i = 0; i < 2; i++) { in[i].peRed = 200 + i; in[i].peGreen = 1; in[i].peBlue = 2; in[i].peFlags = PC_RESERVED; }
    BOOL r = U(AnimatePalette)(h, 3, 2, in);
    CHECK(r);
    EXPECT_EQ(U(GetPaletteEntries)(h, 3, 2, out), 2);
    if (out[0].peRed != 200) t_note("AnimatePalette did not update PC_RESERVED entry (r=%u)", out[0].peRed);
    EXPECT_EQ(U(GetPaletteEntries)(h, 2, 1, out), 1);
    EXPECT_EQ(out[0].peRed, 2);                     /* outside range */
    U(DeleteObject)(h);
}

TEST(palette_animate_mixed_flags) {
    REQUIRE(U(AnimatePalette)); REQUIRE(U(SetPaletteEntries)); REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make(4);
    PALETTEENTRY base[4];
    for (int i = 0; i < 4; i++) { base[i].peRed = 10 * (i + 1); base[i].peGreen = 0; base[i].peBlue = 0; base[i].peFlags = (i & 1) ? PC_RESERVED : 0; }
    EXPECT_EQ(U(SetPaletteEntries)(h, 0, 4, base), 4);
    PALETTEENTRY an[4];
    for (int i = 0; i < 4; i++) { an[i].peRed = 99; an[i].peGreen = 99; an[i].peBlue = 99; an[i].peFlags = base[i].peFlags; }
    U(AnimatePalette)(h, 0, 4, an);
    PALETTEENTRY out[4];
    EXPECT_EQ(U(GetPaletteEntries)(h, 0, 4, out), 4);
    EXPECT_EQ(out[0].peRed, 10);       /* non-reserved: unchanged */
    EXPECT_EQ(out[2].peRed, 30);
    t_note("reserved entries after animate: %u %u", out[1].peRed, out[3].peRed);
    U(DeleteObject)(h);
}

TEST(palette_animate_ranges) {
    REQUIRE(U(AnimatePalette)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make_flags(8, PC_RESERVED);
    PALETTEENTRY pe[16];
    memset(pe, 0, sizeof pe);
    struct { UINT start, cnt; } tab[] = {{0, 8}, {0, 9}, {7, 1}, {7, 5}, {8, 1}, {100, 2}, {0, 0}};
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        SetLastError(0);
        BOOL r = U(AnimatePalette)(h, tab[i].start, tab[i].cnt, pe);
        t_note("AnimatePalette(start=%u cnt=%u)=%d", tab[i].start, tab[i].cnt, r);
    }
    U(DeleteObject)(h);
}

TEST(palette_animate_invalid) {
    REQUIRE(U(AnimatePalette)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    PALETTEENTRY pe = {1, 2, 3, PC_RESERVED};
    CHECK(!U(AnimatePalette)(NULL, 0, 1, &pe));
    CHECK(!U(AnimatePalette)((HPALETTE)PAL_BOGUS_HANDLE, 0, 1, &pe));
    HBRUSH br = U(CreateSolidBrush)(0);
    CHECK(!U(AnimatePalette)((HPALETTE)br, 0, 1, &pe));
    U(DeleteObject)(br);
}

TEST(palette_animate_deleted) {
    REQUIRE(U(AnimatePalette)); REQUIRE(U(DeleteObject));
    HPALETTE h = pal_make_flags(4, PC_RESERVED);
    PALETTEENTRY pe = {1, 2, 3, PC_RESERVED};
    U(DeleteObject)(h);
    CHECK(!U(AnimatePalette)(h, 0, 1, &pe));
}

/* ---- Halftone ---- */
TEST(palette_halftone_basic) {
    REQUIRE(U(CreateHalftonePalette)); REQUIRE(U(GetObjectW)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectType));
    HDC dc = pal_screen_dc();
    if (!dc) { t_skip("no screen dc"); return; }
    HPALETTE h = U(CreateHalftonePalette)(dc);
    CHECK(h != NULL);
    if (h) {
        EXPECT_EQ(U(GetObjectType)(h), OBJ_PAL);
        WORD w = 0;
        EXPECT_EQ(U(GetObjectW)(h, 2, &w), 2);
        if (w != 256) t_note("halftone palette entries: %u", w);
        CHECK(w >= 1);
        U(DeleteObject)(h);
    }
    pal_release_screen_dc(dc);
}

TEST(palette_halftone_entries) {
    REQUIRE(U(CreateHalftonePalette)); REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    HDC dc = pal_screen_dc();
    if (!dc) { t_skip("no screen dc"); return; }
    HPALETTE h = U(CreateHalftonePalette)(dc);
    int n = pal_count(h);
    CHECK(n > 0);
    PALETTEENTRY pe[256];
    memset(pe, 0xee, sizeof pe);
    UINT got = U(GetPaletteEntries)(h, 0, 256, pe);
    EXPECT_EQ(got, n > 256 ? 256 : n);
    if (got == 256) {
        if (pe[0].peRed || pe[0].peGreen || pe[0].peBlue) t_note("halftone entry 0 = %u,%u,%u", pe[0].peRed, pe[0].peGreen, pe[0].peBlue);
        if (pe[255].peRed != 255 || pe[255].peGreen != 255 || pe[255].peBlue != 255)
            t_note("halftone entry 255 = %u,%u,%u", pe[255].peRed, pe[255].peGreen, pe[255].peBlue);
        int flagged = 0;
        for (int i = 0; i < 256; i++) if (pe[i].peFlags) flagged++;
        t_note("halftone entries with flags: %d", flagged);
    }
    U(DeleteObject)(h);
    pal_release_screen_dc(dc);
}

TEST(palette_halftone_unique_entries) {
    REQUIRE(U(CreateHalftonePalette)); REQUIRE(U(GetPaletteEntries)); REQUIRE(U(DeleteObject));
    HDC dc = pal_screen_dc();
    if (!dc) { t_skip("no screen dc"); return; }
    HPALETTE a = U(CreateHalftonePalette)(dc), b = U(CreateHalftonePalette)(dc);
    CHECK(a && b && a != b);
    PALETTEENTRY pa[256], pb[256];
    UINT na = U(GetPaletteEntries)(a, 0, 256, pa), nb = U(GetPaletteEntries)(b, 0, 256, pb);
    EXPECT_EQ(na, nb);
    if (na == nb && na) CHECK(!memcmp(pa, pb, na * sizeof(PALETTEENTRY)));
    if (a) U(DeleteObject)(a);
    if (b) U(DeleteObject)(b);
    pal_release_screen_dc(dc);
}

TEST(palette_halftone_memory_dcs) {
    REQUIRE(U(CreateHalftonePalette)); REQUIRE(U(DeleteObject));
    struct dibdc d;
    static const int bpps[] = {1, 4, 8, 16, 24, 32};
    for (unsigned i = 0; i < sizeof bpps / sizeof *bpps; i++) {
        if (!dibdc_create(&d, bpps[i], 8, 8)) { t_note("no %dbpp dib", bpps[i]); continue; }
        HPALETTE h = U(CreateHalftonePalette)(d.dc);
        CHECKF(h != NULL, "bpp=%d", bpps[i]);
        t_note("halftone for %dbpp memdc: count=%d", bpps[i], h ? pal_count(h) : -1);
        if (h) U(DeleteObject)(h);
        dibdc_free(&d);
    }
}

TEST(palette_halftone_invalid_dc) {
    REQUIRE(U(CreateHalftonePalette)); REQUIRE(U(DeleteObject));
    if (pal_is_wine()) { t_skip("wine accepts invalid dc"); return; }
    SetLastError(0);
    HPALETTE h = U(CreateHalftonePalette)(NULL);
    EXPECT_EQ(h, NULL);
    h = U(CreateHalftonePalette)((HDC)PAL_BOGUS_HANDLE);
    EXPECT_EQ(h, NULL);
    HPALETTE p = pal_make(2);
    h = U(CreateHalftonePalette)((HDC)p);          /* a palette is not a DC */
    EXPECT_EQ(h, NULL);
    U(DeleteObject)(p);
}

TEST(palette_halftone_deleted_dc) {
    REQUIRE(U(CreateHalftonePalette)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    if (pal_is_wine()) { t_skip("wine accepts invalid dc"); return; }
    HDC dc = U(CreateCompatibleDC)(NULL);
    U(DeleteDC)(dc);
    EXPECT_EQ(U(CreateHalftonePalette)(dc), NULL);
}

/* ---- Threads ---- */
struct tctx { HPALETTE h; UINT count; UINT first_r; UINT set; };
static DWORD WINAPI thr_pal(LPVOID p) {
    struct tctx *c = (struct tctx *)p;
    PALETTEENTRY pe;
    c->count = U(GetPaletteEntries)(c->h, 0, 0, NULL);
    if (U(GetPaletteEntries)(c->h, 3, 1, &pe) == 1) c->first_r = pe.peRed;
    PALETTEENTRY n = {77, 78, 79, 0};
    c->set = U(SetPaletteEntries)(c->h, 0, 1, &n);
    return 0;
}
TEST(palette_cross_thread_use) {
    REQUIRE(U(GetPaletteEntries)); REQUIRE(U(SetPaletteEntries)); REQUIRE(U(DeleteObject));
    struct tctx c = {0};
    c.h = pal_make(10);
    HANDLE t = CreateThread(NULL, 0, thr_pal, &c, 0, NULL);
    WaitForSingleObject(t, 10000);
    CloseHandle(t);
    EXPECT_EQ(c.count, 10);
    EXPECT_EQ(c.first_r, 3);
    EXPECT_EQ(c.set, 1);
    PALETTEENTRY pe;
    EXPECT_EQ(U(GetPaletteEntries)(c.h, 0, 1, &pe), 1);
    EXPECT_EQ(pe.peRed, 77);
    U(DeleteObject)(c.h);
}

static DWORD WINAPI thr_create(LPVOID p) {
    *(HPALETTE *)p = pal_make(6);
    return 0;
}
TEST(palette_created_in_thread_survives) {
    REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectType));
    HPALETTE h = NULL;
    HANDLE t = CreateThread(NULL, 0, thr_create, &h, 0, NULL);
    WaitForSingleObject(t, 10000);
    CloseHandle(t);
    CHECK(h != NULL);
    if (h) {
        EXPECT_EQ(U(GetObjectType)(h), OBJ_PAL);
        EXPECT_EQ(pal_count(h), 6);
        CHECK(U(DeleteObject)(h));
    }
}
