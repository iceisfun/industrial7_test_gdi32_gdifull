/* Additional edge cases: boundaries, consistency and handle hygiene. */
#include "gc.h"
#define NEED_X() do { REQUIRE(U(GetCharWidth32W)); REQUIRE(U(GetCharABCWidthsW)); REQUIRE(U(GetGlyphOutlineW)); REQUIRE(U(GetGlyphIndicesW)); GC_REQ_BASE(); } while (0)

TEST(extra_width32_single_vs_range_each_face)
{
    GC g; int fi; UINT c; NEED_X();
    for (fi = 0; fi < GC_NFACES; fi++) {
        INT all[96];
        if (!gc_open(&g, gc_faces[fi], -16)) continue;
        CHECK(U(GetCharWidth32W)(g.dc, 0x20, 0x7F, all));
        for (c = 0x20; c <= 0x7F; c++) { INT w = -1; CHECK(U(GetCharWidth32W)(g.dc, c, c, &w)); CHECKF(w == all[c - 0x20], "%s c=%x %d vs %d", g.face, c, w, all[c - 0x20]); }
        gc_close(&g);
    }
}

TEST(extra_abc_single_vs_range_each_face)
{
    GC g; int fi; UINT c; NEED_X();
    for (fi = 0; fi < GC_NFACES; fi++) {
        ABC all[96];
        if (!gc_open(&g, gc_faces[fi], -24)) continue;
        CHECK(U(GetCharABCWidthsW)(g.dc, 0x20, 0x7F, all));
        for (c = 0x20; c <= 0x7F; c++) { ABC a; CHECK(U(GetCharABCWidthsW)(g.dc, c, c, &a));
            CHECKF(a.abcA == all[c - 0x20].abcA && a.abcB == all[c - 0x20].abcB && a.abcC == all[c - 0x20].abcC, "%s c=%x", g.face, c); }
        gc_close(&g);
    }
}

TEST(extra_abc_B_nonnegative_printable_arial)
{
    GC g; UINT c; ABC a[95]; NEED_X();
    if (!gc_open(&g, "Arial", -32)) return;
    CHECK(U(GetCharABCWidthsW)(g.dc, 0x21, 0x7F - 1, a));
    for (c = 0; c < 94; c++) CHECKF((int)a[c].abcB > 0, "c=%c B=%u", 0x21 + c, a[c].abcB);
    gc_close(&g);
}

TEST(extra_cellinc_equals_abc_sum_arial_sizes)
{
    static const int hs[] = { -12, -16, -32, -48 }; GC g; unsigned i; UINT c; NEED_X();
    for (i = 0; i < 4; i++) {
        if (!gc_open(&g, "Arial", hs[i])) continue;
        for (c = 'A'; c <= 'z'; c += 5) {
            GLYPHMETRICS gm; MAT2 m = gc_ident(); ABC a; INT w; DWORD r;
            r = U(GetGlyphOutlineW)(g.dc, c, GGO_METRICS, &gm, 0, NULL, &m);
            if (r == GDI_ERROR) { CHECK(0); continue; }
            CHECK(U(GetCharABCWidthsW)(g.dc, c, c, &a)); CHECK(U(GetCharWidth32W)(g.dc, c, c, &w));
            CHECKF((int)gm.gmCellIncX == w, "h=%d c=%c cellinc %d w %d", hs[i], c, gm.gmCellIncX, w);
            CHECKF(a.abcA + (int)a.abcB + a.abcC == w, "h=%d c=%c abc sum", hs[i], c);
            CHECKF(gm.gmCellIncY == 0, "cellincY %d", gm.gmCellIncY);
        }
        gc_close(&g);
    }
}

TEST(extra_glyph_index_outline_equals_char_outline)
{
    GC g; UINT c; NEED_X();
    if (!gc_open(&g, "Arial", -32)) return;
    for (c = 'A'; c <= 'Z'; c += 3) {
        WCHAR ch = (WCHAR)c; WORD gi = 0xFFFF; GLYPHMETRICS a, b; MAT2 m = gc_ident();
        CHECK(U(GetGlyphIndicesW)(g.dc, &ch, 1, &gi, 0) == 1);
        CHECK(U(GetGlyphOutlineW)(g.dc, c, GGO_METRICS, &a, 0, NULL, &m) != GDI_ERROR);
        CHECK(U(GetGlyphOutlineW)(g.dc, gi, GGO_METRICS | GGO_GLYPH_INDEX, &b, 0, NULL, &m) != GDI_ERROR);
        CHECKF(!memcmp(&a, &b, sizeof a), "c=%c", c);
    }
    gc_close(&g);
}

TEST(extra_widths_unchanged_by_dc_bitmap_depth)
{
    static const int bpp[] = { 1, 8, 24, 32 }; unsigned i; NEED_X();
    for (i = 0; i < 4; i++) {
        struct { BITMAPINFOHEADER h; RGBQUAD pal[256]; } bi; void *bits; HDC dc; HBITMAP b, ob; HFONT f, of; INT w = 0, w0;
        dc = U(CreateCompatibleDC)(NULL); if (!dc) { CHECK(0); continue; }
        memset(&bi, 0, sizeof bi); bi.h.biSize = sizeof bi.h; bi.h.biWidth = 8; bi.h.biHeight = 8; bi.h.biPlanes = 1; bi.h.biBitCount = bpp[i];
        b = U(CreateDIBSection)(dc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
        if (!b) { U(DeleteDC)(dc); t_note("no %d bpp dib", bpp[i]); continue; }
        ob = (HBITMAP)U(SelectObject)(dc, b);
        f = gc_mkfont("Arial", -24, FW_NORMAL, ANSI_CHARSET, 0); of = (HFONT)U(SelectObject)(dc, f);
        CHECK(U(GetCharWidth32W)(dc, 'W', 'W', &w));
        w0 = w;
        CHECKF(w0 > 0, "bpp %d w %d", bpp[i], w0);
        U(SelectObject)(dc, of); U(DeleteObject)(f); U(SelectObject)(dc, ob); U(DeleteObject)(b); U(DeleteDC)(dc);
    }
}

TEST(extra_char_range_boundary_ffff)
{
    GC g; INT w[2]; NEED_X();
    if (!gc_open(&g, "Arial", -16)) return;
    CHECK(U(GetCharWidth32W)(g.dc, 0xFFFF, 0xFFFF, w));
    CHECK(U(GetCharWidth32W)(g.dc, 0xFFFE, 0xFFFF, w));
    CHECK(U(GetCharWidth32W)(g.dc, 0, 0, w));
    gc_close(&g);
}

TEST(extra_glyph_indices_ascii_distinct_arial)
{
    GC g; WCHAR s[95]; WORD o[95]; unsigned i, j; NEED_X();
    if (!gc_open(&g, "Arial", -16)) return;
    for (i = 0; i < 95; i++) s[i] = (WCHAR)(0x20 + i);
    CHECK(U(GetGlyphIndicesW)(g.dc, s, 95, o, GGI_MARK_NONEXISTING_GLYPHS) == 95);
    for (i = 0; i < 95; i++) for (j = i + 1; j < 95; j++) CHECKF(o[i] != o[j], "%x and %x share glyph %u", s[i], s[j], o[i]);
    gc_close(&g);
}

TEST(extra_space_advance_equals_textextent_arial)
{
    GC g; SIZE sz; INT w; WCHAR sp = ' '; NEED_X(); REQUIRE(U(GetTextExtentPoint32W));
    if (!gc_open(&g, "Arial", -20)) return;
    CHECK(U(GetCharWidth32W)(g.dc, ' ', ' ', &w));
    CHECK(U(GetTextExtentPoint32W)(g.dc, &sp, 1, &sz));
    CHECKF(sz.cx == w, "%ld vs %d", sz.cx, w);
    gc_close(&g);
}
