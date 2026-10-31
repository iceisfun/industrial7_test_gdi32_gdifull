/* GetGlyphIndices* and glyph-index based width queries. */
#include "gc.h"

#define NEED_GI() do { REQUIRE(U(GetGlyphIndicesW)); REQUIRE(U(GetGlyphIndicesA)); GC_REQ_BASE(); REQUIRE(U(GetFontData)); } while (0)

static void gi_fill(WCHAR *s, unsigned first, unsigned n) { unsigned i; for (i = 0; i < n; i++) s[i] = (WCHAR)(first + i); }

/* Compare GetGlyphIndicesW against the font's own cmap for [first, first+n). */
static void gi_vs_cmap(GC *g, unsigned first, unsigned n, int mark, const char *what)
{
    static WCHAR s[0x1000]; static WORD out[0x1000]; DWORD cl = 0, r; BYTE *cm; unsigned i, bad = 0;
    if (n > 0x1000) n = 0x1000;
    cm = gc_table(g->dc, "cmap", &cl);
    if (!cm) { t_note("%s: no cmap table", g->face); return; }
    gi_fill(s, first, n);
    memset(out, 0x77, sizeof out);
    r = U(GetGlyphIndicesW)(g->dc, s, (int)n, out, mark ? GGI_MARK_NONEXISTING_GLYPHS : 0);
    CHECKF(r == n, "%s %s: returned %lu want %u", g->face, what, r, n);
    if (r == n) for (i = 0; i < n; i++) {
        unsigned want = gc_cmap(cm, cl, first + i);
        if (want) { if (out[i] != want) { bad++; if (bad < 4) t_note("%s %s U+%04x: got %u cmap %u", g->face, what, first + i, out[i], want); } }
        else if (mark) { if (out[i] != 0xFFFF) { bad++; if (bad < 4) t_note("%s %s U+%04x missing: got %04x", g->face, what, first + i, out[i]); } }
    }
    CHECKF(bad == 0, "%s %s: %u glyph index mismatches", g->face, what, bad);
    free(cm);
}

TEST(gi_ascii_matches_cmap_all_text_faces)
{
    static const char *const fs[] = { "Arial", "Times New Roman", "Courier New", "Segoe UI", "Tahoma", "Consolas" };
    GC g; unsigned f, mark; NEED_GI();
    for (f = 0; f < 6; f++) for (mark = 0; mark < 2; mark++) {
        if (!gc_open(&g, fs[f], -16)) break;
        gi_vs_cmap(&g, 0x20, 0x5F, mark, "ascii");
        gc_close(&g);
    }
}

TEST(gi_latin1_and_punctuation_matches_cmap)
{
    GC g; unsigned f, mark; static const char *const fs[] = { "Arial", "Times New Roman", "Segoe UI" }; NEED_GI();
    for (f = 0; f < 3; f++) for (mark = 0; mark < 2; mark++) {
        if (!gc_open(&g, fs[f], -24)) break;
        gi_vs_cmap(&g, 0xA0, 0x60, mark, "latin1");
        gi_vs_cmap(&g, 0x2010, 0x40, mark, "punct");
        gi_vs_cmap(&g, 0x391, 0x40, mark, "greek");
        gi_vs_cmap(&g, 0x410, 0x40, mark, "cyrillic");
        gc_close(&g);
    }
}

TEST(gi_whole_bmp_with_mark_matches_cmap)
{
    static const char *const fs[] = { "Arial", "Times New Roman", "Segoe UI", "Courier New" }; GC g; unsigned f, base; NEED_GI();
    for (f = 0; f < 4; f++) {
        if (!gc_open(&g, fs[f], -12)) continue;
        for (base = 0x20; base < 0x10000; base += 0x1000) {
            unsigned n = base + 0x1000 > 0x10000 ? 0x10000 - base : 0x1000;
            gi_vs_cmap(&g, base, n, 1, "bmp");
        }
        gc_close(&g);
    }
}

TEST(gi_missing_glyphs_marked_ffff)
{
    static const WCHAR cand[] = { 0xE000, 0xE001, 0x0378, 0x4E00, 0xFFFE, 0xD7FF, 0x10FF, 0x2FFF };
    GC g; DWORD cl; BYTE *cm; unsigned i, tested = 0; WORD out[8]; WCHAR s[1]; NEED_GI();
    if (!gc_open(&g, "Arial", -16)) return;
    cm = gc_table(g.dc, "cmap", &cl);
    for (i = 0; i < sizeof cand / sizeof cand[0]; i++) {
        s[0] = cand[i];
        if (cm && gc_cmap(cm, cl, cand[i])) continue;
        out[0] = 0x1234;
        CHECKF(U(GetGlyphIndicesW)(g.dc, s, 1, out, GGI_MARK_NONEXISTING_GLYPHS) == 1, "U+%04x", cand[i]);
        CHECKF(out[0] == 0xFFFF, "U+%04x -> %04x", cand[i], out[0]);
        tested++;
    }
    CHECK(tested > 0);
    free(cm);
    gc_close(&g);
}

TEST(gi_missing_glyph_without_flag_observed)
{
    GC g; WCHAR s[2] = { 0xE000, 'A' }; WORD out[2] = { 0x1234, 0x1234 }; TEXTMETRICW tm; NEED_GI();
    if (!gc_open(&g, "Arial", -16)) return;
    CHECK(U(GetGlyphIndicesW)(g.dc, s, 2, out, 0) == 2);
    CHECK(out[1] != 0xFFFF && out[1] != 0);
    U(GetTextMetricsW)(g.dc, &tm);
    t_note("Arial U+E000 flag0 -> %04x (default char %04x)", out[0], tm.tmDefaultChar);
    CHECK(out[0] != 0x1234);
    gc_close(&g);
}

TEST(gi_space_glyph_is_valid)
{
    GC g; int fi; static const char *const fs[] = { "Arial", "Times New Roman", "Courier New", "Segoe UI", "Tahoma" }; NEED_GI();
    for (fi = 0; fi < 5; fi++) {
        WCHAR s = ' '; WORD o = 0; DWORD cl; BYTE *cm;
        if (!gc_open(&g, fs[fi], -20)) continue;
        CHECK(U(GetGlyphIndicesW)(g.dc, &s, 1, &o, GGI_MARK_NONEXISTING_GLYPHS) == 1);
        CHECKF(o != 0xFFFF && o != 0, "%s space -> %04x", g.face, o);
        cm = gc_table(g.dc, "cmap", &cl);
        if (cm) { CHECKF(o == gc_cmap(cm, cl, ' '), "%s space %u cmap %u", g.face, o, gc_cmap(cm, cl, ' ')); free(cm); }
        gc_close(&g);
    }
}

TEST(gi_arial_known_glyph_order)
{
    GC g; WCHAR s[4] = { ' ', '!', 'A', 'a' }; WORD o[4]; NEED_GI();
    if (!gc_open(&g, "Arial", -16)) return;
    CHECK(U(GetGlyphIndicesW)(g.dc, s, 4, o, 0) == 4);
    /* consecutive ASCII characters have consecutive glyph ids in the stock Arial */
    CHECKF(o[1] == o[0] + 1, "space %u '!' %u", o[0], o[1]);
    CHECKF(o[3] > o[2], "A %u a %u", o[2], o[3]);
    gc_close(&g);
}

TEST(gi_A_matches_W_ascii)
{
    GC g; char a[96]; WCHAR w[96]; WORD ga[96], gw[96]; int i, mark; NEED_GI();
    if (!gc_open(&g, "Arial", -16)) return;
    for (i = 0; i < 95; i++) { a[i] = (char)(0x20 + i); w[i] = (WCHAR)(0x20 + i); }
    for (mark = 0; mark < 2; mark++) {
        DWORD fl = mark ? GGI_MARK_NONEXISTING_GLYPHS : 0;
        CHECK(U(GetGlyphIndicesA)(g.dc, a, 95, ga, fl) == 95);
        CHECK(U(GetGlyphIndicesW)(g.dc, w, 95, gw, fl) == 95);
        CHECK(!memcmp(ga, gw, sizeof(WORD) * 95));
    }
    gc_close(&g);
}

TEST(gi_A_matches_W_cp1252_high)
{
    GC g; char a[128]; WCHAR w[128]; WORD ga[128], gw[128]; int i; NEED_GI();
    if (!gc_open(&g, "Arial", -16)) return;
    for (i = 0; i < 96; i++) { a[i] = (char)(0xA0 + i); w[i] = (WCHAR)(0xA0 + i); }
    CHECK(U(GetGlyphIndicesA)(g.dc, a, 96, ga, 0) == 96);
    CHECK(U(GetGlyphIndicesW)(g.dc, w, 96, gw, 0) == 96);
    CHECK(!memcmp(ga, gw, sizeof(WORD) * 96));
    {
        static const unsigned char ea[] = { 0x80, 0x85, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x99 };
        static const WCHAR ew[] = { 0x20AC, 0x2026, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x2122 };
        WORD a2[10], w2[10];
        CHECK(U(GetGlyphIndicesA)(g.dc, (const char *)ea, 10, a2, GGI_MARK_NONEXISTING_GLYPHS) == 10);
        CHECK(U(GetGlyphIndicesW)(g.dc, ew, 10, w2, GGI_MARK_NONEXISTING_GLYPHS) == 10);
        CHECK(!memcmp(a2, w2, sizeof a2));
    }
    gc_close(&g);
}

TEST(gi_A_zero_and_negative_count)
{
    GC g; WORD o[2] = { 0x55, 0x55 }; DWORD r, e; NEED_GI();
    if (!gc_open(&g, "Arial", -16)) return;
    SetLastError(GC_ERRSENT);
    r = U(GetGlyphIndicesA)(g.dc, "A", 0, o, 0); e = GetLastError();
    EXPECT_EQ(r, 0); EXPECT_EQ(e, GC_ERRSENT); EXPECT_EQ(o[0], 0x55);
    SetLastError(GC_ERRSENT);
    r = U(GetGlyphIndicesA)(g.dc, "A", -1, o, 0); e = GetLastError();
    EXPECT_EQ(r, GDI_ERROR); EXPECT_EQ(e, ERROR_INVALID_PARAMETER);
    SetLastError(GC_ERRSENT);
    r = U(GetGlyphIndicesA)(g.dc, "A", 0x80000000, o, 0); e = GetLastError();
    EXPECT_EQ(r, GDI_ERROR); EXPECT_EQ(e, ERROR_INVALID_PARAMETER);
    EXPECT_EQ(o[0], 0x55);
    gc_close(&g);
}

TEST(gi_A_zero_count_ignores_dc)
{
    WORD o[1]; REQUIRE(U(GetGlyphIndicesA));
    /* decompiled: c == 0 returns 0 before the DC is looked at */
    EXPECT_EQ(U(GetGlyphIndicesA)(NULL, "A", 0, o, 0), 0);
    EXPECT_EQ(U(GetGlyphIndicesA)((HDC)(ULONG_PTR)0x1234, NULL, 0, NULL, 0), 0);
}

TEST(gi_W_zero_and_negative_count_observed)
{
    GC g; WORD o[2] = { 0x55, 0x55 }; WCHAR s[2] = { 'A', 'B' }; DWORD r; NEED_GI();
    if (!gc_open(&g, "Arial", -16)) return;
    SetLastError(GC_ERRSENT);
    r = U(GetGlyphIndicesW)(g.dc, s, 0, o, 0);
    t_note("GetGlyphIndicesW count 0 -> %lu err %lu out %x", r, GetLastError(), o[0]);
    SetLastError(GC_ERRSENT);
    r = U(GetGlyphIndicesW)(g.dc, s, -1, o, 0);
    t_note("GetGlyphIndicesW count -1 -> %lx err %lu", r, GetLastError());
    CHECKF(o[0] == 0x55 || r != GDI_ERROR, "buffer written despite error");
    gc_close(&g);
}

TEST(gi_bad_dc_returns_gdi_error)
{
    WCHAR s[1] = { 'A' }; WORD o[1]; DWORD r, e; HDC dc; NEED_GI();
    SetLastError(GC_ERRSENT);
    r = U(GetGlyphIndicesA)(NULL, "A", 1, o, 0); e = GetLastError();
    EXPECT_EQ(r, GDI_ERROR); EXPECT_EQ(e, ERROR_INVALID_PARAMETER);
    SetLastError(GC_ERRSENT);
    r = U(GetGlyphIndicesA)((HDC)(ULONG_PTR)0x12345, "A", 1, o, 0); e = GetLastError();
    EXPECT_EQ(r, GDI_ERROR); EXPECT_EQ(e, ERROR_INVALID_PARAMETER);
    EXPECT_EQ(U(GetGlyphIndicesW)(NULL, s, 1, o, 0), GDI_ERROR);
    EXPECT_EQ(U(GetGlyphIndicesW)((HDC)(ULONG_PTR)0x12345, s, 1, o, 0), GDI_ERROR);
    dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    SetLastError(GC_ERRSENT);
    r = U(GetGlyphIndicesA)(dc, "A", 1, o, 0); e = GetLastError();
    EXPECT_EQ(r, GDI_ERROR); EXPECT_EQ(e, ERROR_INVALID_PARAMETER);
    EXPECT_EQ(U(GetGlyphIndicesW)(dc, s, 1, o, 0), GDI_ERROR);
}

TEST(gi_wrong_object_as_dc)
{
    WCHAR s[1] = { 'A' }; WORD o[1]; HGDIOBJ p; NEED_GI(); REQUIRE(U(CreatePen));
    p = U(CreatePen)(PS_SOLID, 1, 0);
    EXPECT_EQ(U(GetGlyphIndicesA)((HDC)p, "A", 1, o, 0), GDI_ERROR);
    EXPECT_EQ(U(GetGlyphIndicesW)((HDC)p, s, 1, o, 0), GDI_ERROR);
    U(DeleteObject)(p);
}

TEST(gi_A_null_string_fails)
{
    GC g; WORD o[2]; DWORD r, e; NEED_GI();
    if (!gc_open(&g, "Arial", -16)) return;
    SetLastError(GC_ERRSENT);
    r = U(GetGlyphIndicesA)(g.dc, NULL, 1, o, 0); e = GetLastError();
    EXPECT_EQ(r, GDI_ERROR); EXPECT_EQ(e, ERROR_INVALID_PARAMETER);
    gc_close(&g);
}

TEST(gi_W_null_string_observed)
{
    GC g; WORD o[2]; DWORD r; NEED_GI();
    if (gc_wine()) { t_skip("wine hangs on NULL string"); return; }
    if (!gc_open(&g, "Arial", -16)) return;
    r = U(GetGlyphIndicesW)(g.dc, NULL, 1, o, 0);
    CHECKF(r == GDI_ERROR, "NULL string returned %lu", r);
    gc_close(&g);
}

TEST(gi_null_output_buffer_observed)
{
    GC g; WCHAR s[3] = { 'A', 'B', 'C' }; DWORD r; NEED_GI();
    if (gc_wine()) { t_skip("wine faults on NULL output buffer"); return; }
    if (!gc_open(&g, "Arial", -16)) return;
    SetLastError(GC_ERRSENT);
    r = U(GetGlyphIndicesW)(g.dc, s, 3, NULL, 0);
    t_note("GetGlyphIndicesW pgi=NULL -> %lx err %lu", r, GetLastError());
    SetLastError(GC_ERRSENT);
    r = U(GetGlyphIndicesA)(g.dc, "ABC", 3, NULL, 0);
    t_note("GetGlyphIndicesA pgi=NULL -> %lx err %lu", r, GetLastError());
    gc_close(&g);
}

TEST(gi_flag_values_observed)
{
    static const DWORD fl[] = { 0, 1, 2, 3, 4, 0x8000, 0x80000000, 0xFFFFFFFF }; GC g; unsigned i; WCHAR s[2] = { 'A', 0xE000 }; NEED_GI();
    if (!gc_open(&g, "Arial", -16)) return;
    for (i = 0; i < sizeof fl / sizeof fl[0]; i++) {
        WORD o[2] = { 0, 0 }; DWORD r = U(GetGlyphIndicesW)(g.dc, s, 2, o, fl[i]);
        if (i < 2) EXPECT_EQ(r, 2);
        else t_note("flags %08lx -> %lx (%04x %04x)", fl[i], r, o[0], o[1]);
    }
    gc_close(&g);
}

TEST(gi_output_not_written_past_count)
{
    GC g; WCHAR s[5] = { 'a', 'b', 'c', 'd', 'e' }; WORD o[10]; int i; NEED_GI();
    if (!gc_open(&g, "Arial", -16)) return;
    for (i = 0; i < 10; i++) o[i] = 0x7777;
    CHECK(U(GetGlyphIndicesW)(g.dc, s, 5, o, 0) == 5);
    for (i = 0; i < 5; i++) CHECK(o[i] != 0x7777);
    for (i = 5; i < 10; i++) CHECK(o[i] == 0x7777);
    gc_close(&g);
}

TEST(gi_duplicates_and_order)
{
    GC g; WCHAR s[6] = { 'x', 'y', 'x', 'y', 'x', 'z' }; WORD o[6]; NEED_GI();
    if (!gc_open(&g, "Arial", -16)) return;
    CHECK(U(GetGlyphIndicesW)(g.dc, s, 6, o, 0) == 6);
    CHECK(o[0] == o[2] && o[2] == o[4] && o[1] == o[3] && o[0] != o[1] && o[0] != o[5]);
    gc_close(&g);
}

TEST(gi_independent_of_font_size)
{
    GC a, b; WCHAR s[26]; WORD oa[26], ob[26]; int i; NEED_GI();
    for (i = 0; i < 26; i++) s[i] = (WCHAR)('A' + i);
    if (!gc_open(&a, "Arial", -8)) return;
    if (!gc_open(&b, "Arial", -100)) { gc_close(&a); return; }
    CHECK(U(GetGlyphIndicesW)(a.dc, s, 26, oa, 0) == 26);
    CHECK(U(GetGlyphIndicesW)(b.dc, s, 26, ob, 0) == 26);
    CHECK(!memcmp(oa, ob, sizeof oa));
    gc_close(&a); gc_close(&b);
}

TEST(gi_differs_between_faces)
{
    GC a, b; WCHAR s[1] = { 'g' }; WORD oa, ob; NEED_GI();
    if (!gc_open(&a, "Arial", -16)) return;
    if (!gc_open(&b, "Segoe UI", -16)) { gc_close(&a); return; }
    CHECK(U(GetGlyphIndicesW)(a.dc, s, 1, &oa, 0) == 1); CHECK(U(GetGlyphIndicesW)(b.dc, s, 1, &ob, 0) == 1);
    CHECKF(oa != ob || gc_wine(), "both %u", oa);
    gc_close(&a); gc_close(&b);
}

TEST(gi_surrogate_pair_observed)
{
    GC g; WCHAR s[2] = { 0xD83D, 0xDE00 }; WORD o[2] = { 0, 0 }; DWORD r; NEED_GI();
    if (!gc_open(&g, "Segoe UI", -16)) return;
    r = U(GetGlyphIndicesW)(g.dc, s, 2, o, GGI_MARK_NONEXISTING_GLYPHS);
    CHECKF(r == 2 || r == GDI_ERROR, "ret %lx", r);
    t_note("Segoe UI surrogate pair U+1F600 -> %lu: %04x %04x", r, o[0], o[1]);
    gc_close(&g);
    if (!gc_open(&g, "Arial", -16)) return;
    r = U(GetGlyphIndicesW)(g.dc, s, 2, o, GGI_MARK_NONEXISTING_GLYPHS);
    CHECKF(r == 2 || r == GDI_ERROR, "ret %lx", r);
    t_note("Arial surrogate pair -> %lu: %04x %04x", r, o[0], o[1]);
    gc_close(&g);
}

TEST(gi_lone_surrogates_do_not_crash)
{
    GC g; WCHAR s[4] = { 0xD800, 'A', 0xDC00, 'B' }; WORD o[4]; DWORD r; NEED_GI();
    if (!gc_open(&g, "Arial", -16)) return;
    r = U(GetGlyphIndicesW)(g.dc, s, 4, o, GGI_MARK_NONEXISTING_GLYPHS);
    CHECKF(r == 4 || r == GDI_ERROR, "ret %lx", r);
    if (r == 4) { CHECK(o[1] != 0xFFFF); CHECK(o[3] != 0xFFFF); }
    gc_close(&g);
}

TEST(gi_large_count_matches_single_queries)
{
    static WCHAR s[4096]; static WORD all[4096]; GC g; unsigned i; NEED_GI();
    if (!gc_open(&g, "Arial", -16)) return;
    gi_fill(s, 0x20, 4096);
    CHECK(U(GetGlyphIndicesW)(g.dc, s, 4096, all, GGI_MARK_NONEXISTING_GLYPHS) == 4096);
    for (i = 0; i < 4096; i += 97) {
        WORD one = 1;
        CHECK(U(GetGlyphIndicesW)(g.dc, &s[i], 1, &one, GGI_MARK_NONEXISTING_GLYPHS) == 1);
        CHECKF(one == all[i], "U+%04x single %04x bulk %04x", s[i], one, all[i]);
    }
    gc_close(&g);
}

TEST(gi_A_large_count_over_stack_buffer)
{
    static char a[600]; static WCHAR w[600]; static WORD ga[600], gw[600]; GC g; int i; NEED_GI();
    if (!gc_open(&g, "Arial", -16)) return;
    for (i = 0; i < 600; i++) { a[i] = (char)(0x20 + (i % 95)); w[i] = (WCHAR)a[i]; }
    /* A path switches to a heap buffer above 0x82 characters */
    CHECK(U(GetGlyphIndicesA)(g.dc, a, 600, ga, 0) == 600);
    CHECK(U(GetGlyphIndicesW)(g.dc, w, 600, gw, 0) == 600);
    CHECK(!memcmp(ga, gw, sizeof ga));
    for (i = 0x81; i <= 0x85; i++) {
        CHECKF(U(GetGlyphIndicesA)(g.dc, a, i, ga, 0) == (DWORD)i, "n=%d", i);
    }
    gc_close(&g);
}

TEST(gi_symbol_font_observed)
{
    GC g; WCHAR s[3] = { 'a', 0x61, 0xF061 }; WORD o[3]; DWORD r; DWORD cl; BYTE *cm; NEED_GI();
    if (!gc_open(&g, "Symbol", -24)) return;
    r = U(GetGlyphIndicesW)(g.dc, s, 3, o, GGI_MARK_NONEXISTING_GLYPHS);
    CHECK(r == 3);
    cm = gc_table(g.dc, "cmap", &cl);
    if (cm) { t_note("Symbol: 'a' -> %04x (cmap %u), U+F061 -> %04x (cmap %u)", o[0], gc_cmap(cm, cl, 0xF061), o[2], gc_cmap(cm, cl, 0xF061)); free(cm); }
    CHECK(o[0] == o[1]);
    gc_close(&g);
}

TEST(gi_raster_font_observed)
{
    GC g; WCHAR s[3] = { 'A', 'B', 0x3042 }; WORD o[3] = { 0, 0, 0 }; DWORD r; NEED_GI();
    if (!gc_open_ex(&g, "Terminal", -12, 400, OEM_CHARSET, 0)) return;
    r = U(GetGlyphIndicesW)(g.dc, s, 3, o, GGI_MARK_NONEXISTING_GLYPHS);
    t_note("raster Terminal: ret %lx -> %04x %04x %04x", r, o[0], o[1], o[2]);
    gc_close(&g);
}

TEST(gi_widthI_equals_char_width)
{
    static const char *const fs[] = { "Arial", "Times New Roman", "Segoe UI" }; static const int hs[] = { -12, -16, -32 };
    GC g; int f, h; UINT c; NEED_GI(); REQUIRE(U(GetCharWidthI)); REQUIRE(U(GetCharWidth32W));
    for (f = 0; f < 3; f++) for (h = 0; h < 3; h++) {
        if (!gc_open(&g, fs[f], hs[h])) continue;
        for (c = 0x20; c <= 0x7E; c++) {
            WCHAR s = (WCHAR)c; WORD gi; INT wi = -1, wc = -2;
            U(GetGlyphIndicesW)(g.dc, &s, 1, &gi, 0);
            CHECK(U(GetCharWidthI)(g.dc, 0, 1, &gi, &wi)); CHECK(U(GetCharWidth32W)(g.dc, c, c, &wc));
            CHECKF(wi == wc, "%s %d ch=%c glyph %u: I=%d char=%d", g.face, g.h, c, gi, wi, wc);
        }
        gc_close(&g);
    }
}

TEST(gi_abcI_equals_char_abc)
{
    GC g; UINT c; NEED_GI(); REQUIRE(U(GetCharABCWidthsI)); REQUIRE(U(GetCharABCWidthsW));
    if (!gc_open(&g, "Arial", -24)) return;
    for (c = 0x20; c <= 0x7E; c++) {
        WCHAR s = (WCHAR)c; WORD gi; ABC ai, ac;
        U(GetGlyphIndicesW)(g.dc, &s, 1, &gi, 0);
        CHECK(U(GetCharABCWidthsI)(g.dc, 0, 1, &gi, &ai)); CHECK(U(GetCharABCWidthsW)(g.dc, c, c, &ac));
        CHECKF(!memcmp(&ai, &ac, sizeof ai), "ch=%c glyph %u: I=%d,%u,%d char=%d,%u,%d", c, gi, ai.abcA, ai.abcB, ai.abcC, ac.abcA, ac.abcB, ac.abcC);
    }
    gc_close(&g);
}

TEST(gi_widthI_multiple_glyphs_array)
{
    GC g; WCHAR s[26]; WORD gi[26]; INT wi[26]; int i; NEED_GI(); REQUIRE(U(GetCharWidthI));
    if (!gc_open(&g, "Arial", -20)) return;
    for (i = 0; i < 26; i++) s[i] = (WCHAR)('a' + i);
    CHECK(U(GetGlyphIndicesW)(g.dc, s, 26, gi, 0) == 26);
    CHECK(U(GetCharWidthI)(g.dc, 0, 26, gi, wi));
    for (i = 0; i < 26; i++) { INT w; U(GetCharWidth32W)(g.dc, s[i], s[i], &w); CHECKF(wi[i] == w, "%c: %d vs %d", s[i], wi[i], w); }
    gc_close(&g);
}

TEST(gi_widthI_special_glyph_values_observed)
{
    GC g; WORD gi[4] = { 0, 0xFFFF, 0xFFFE, 0x7FFF }; INT w[4]; int i; NEED_GI(); REQUIRE(U(GetCharWidthI));
    if (!gc_open(&g, "Arial", -20)) return;
    for (i = 0; i < 4; i++) {
        BOOL r = U(GetCharWidthI)(g.dc, 0, 1, &gi[i], &w[i]);
        t_note("GetCharWidthI glyph %04x -> %d width %d", gi[i], r, r ? w[0 + i] : -1);
    }
    gc_close(&g);
}

TEST(gi_widthI_notdef_glyph_width_positive)
{
    GC g; WORD gi = 0; INT w = 0; ABC a; NEED_GI(); REQUIRE(U(GetCharWidthI)); REQUIRE(U(GetCharABCWidthsI));
    if (!gc_open(&g, "Arial", -32)) return;
    CHECK(U(GetCharWidthI)(g.dc, 0, 1, &gi, &w)); CHECK(w > 0);
    CHECK(U(GetCharABCWidthsI)(g.dc, 0, 1, &gi, &a)); CHECK(a.abcB > 0);
    gc_close(&g);
}

TEST(gi_glyph_ranges_up_to_numglyphs)
{
    GC g; DWORD l; BYTE *maxp; static INT w[2048]; unsigned n, gcnt; NEED_GI(); REQUIRE(U(GetCharWidthI));
    if (!gc_open(&g, "Arial", -16)) return;
    maxp = gc_table(g.dc, "maxp", &l);
    if (!maxp) { gc_close(&g); return; }
    gcnt = gc_be16(maxp + 4); free(maxp);
    n = gcnt < 2048 ? gcnt : 2048;
    CHECK(gcnt > 100);
    CHECK(U(GetCharWidthI)(g.dc, 0, n, NULL, w));
    gc_close(&g);
}

TEST(gi_stable_after_font_switch)
{
    GC g; WCHAR s = 'Q'; WORD a1, a2, b; HFONT f; NEED_GI();
    if (!gc_open(&g, "Arial", -16)) return;
    U(GetGlyphIndicesW)(g.dc, &s, 1, &a1, 0);
    f = gc_mkfont("Times New Roman", -16, 400, ANSI_CHARSET, 0);
    if (f && U(SelectObject)(g.dc, f)) {
        U(GetGlyphIndicesW)(g.dc, &s, 1, &b, 0);
        U(SelectObject)(g.dc, g.font);
        U(DeleteObject)(f);
    }
    U(GetGlyphIndicesW)(g.dc, &s, 1, &a2, 0);
    CHECK(a1 == a2);
    gc_close(&g);
}

TEST(gi_default_dc_font_works)
{
    HDC dc; WCHAR s[2] = { 'A', ' ' }; WORD o[2]; DWORD r; NEED_GI();
    dc = U(CreateCompatibleDC)(NULL);
    r = U(GetGlyphIndicesW)(dc, s, 2, o, 0);
    t_note("default DC font GetGlyphIndicesW -> %lx (%04x %04x)", r, o[0], o[1]);
    CHECKF(r == 2 || r == GDI_ERROR, "ret %lx", r);
    U(DeleteDC)(dc);
}

TEST(gi_no_gdi_leak)
{
    GC g; WCHAR s[16]; WORD o[16]; int before, after, i; NEED_GI();
    if (!gc_open(&g, "Arial", -16)) return;
    gi_fill(s, 'a', 16);
    before = gc_gdicount();
    for (i = 0; i < 500; i++) { U(GetGlyphIndicesW)(g.dc, s, 16, o, 1); U(GetGlyphIndicesA)(g.dc, "abcdefgh", 8, o, 0); }
    after = gc_gdicount();
    if (before >= 0) CHECKF(before == after, "%d -> %d", before, after);
    gc_close(&g);
}
