/* Value consistency of the character width functions. */
#include "gc.h"

#define NEED_W() do { REQUIRE(U(GetCharWidth32W)); REQUIRE(U(GetCharWidthW)); REQUIRE(U(GetCharABCWidthsW)); REQUIRE(U(GetCharWidthFloatW)); \
    REQUIRE(U(GetCharABCWidthsFloatW)); REQUIRE(U(GetTextExtentPoint32W)); GC_REQ_BASE(); } while (0)

static int w32(HDC dc, UINT ch) { INT w = -12345; return U(GetCharWidth32W)(dc, ch, ch, &w) ? w : -12345; }
static int abcsum(const ABC *a) { return a->abcA + (int)a->abcB + a->abcC; }
static int tx(HDC dc, UINT ch) { WCHAR c = (WCHAR)ch; SIZE s = { -1, -1 }; U(GetTextExtentPoint32W)(dc, &c, 1, &s); return s.cx; }

TEST(values_abc_sum_equals_width32_arial)
{
    static const int hs[] = { -12, -16, -32 }; GC g; int hi; UINT c; NEED_W();
    for (hi = 0; hi < 3; hi++) {
        if (!gc_open(&g, "Arial", hs[hi])) continue;
        for (c = 0x20; c <= 0x7E; c++) {
            ABC a; INT w;
            CHECK(U(GetCharABCWidthsW)(g.dc, c, c, &a));
            CHECK(U(GetCharWidth32W)(g.dc, c, c, &w));
            CHECKF(abcsum(&a) == w, "h=%d ch=%c abc=%d,%u,%d w=%d", hs[hi], c, a.abcA, a.abcB, a.abcC, w);
        }
        gc_close(&g);
    }
}

TEST(values_abc_sum_vs_width32_all_faces_tolerance)
{
    GC g; int fi, hi, bad = 0, tot = 0; UINT c; NEED_W();
    for (fi = 0; fi < GC_NFACES; fi++) for (hi = 0; hi < GC_NHEIGHTS; hi++) {
        if (!gc_open(&g, gc_faces[fi], gc_heights[hi])) continue;
        for (c = 0x20; c <= 0x7E; c++) {
            ABC a; INT w;
            if (!U(GetCharABCWidthsW)(g.dc, c, c, &a) || !U(GetCharWidth32W)(g.dc, c, c, &w)) { CHECK(0); continue; }
            tot++;
            if (abcsum(&a) != w) bad++;
            CHECKF(abs(abcsum(&a) - w) <= 1, "%s %d ch=%c abc=%d w=%d", g.face, g.h, c, abcsum(&a), w);
        }
        gc_close(&g);
    }
    if (bad) t_note("%d of %d (face,size,char) tuples have abc sum != width32 (by 1)", bad, tot);
}

TEST(values_width_vs_width32_arial)
{
    static const int hs[] = { -8, -12, -16, -24, -48, -100 }; GC g; int hi; UINT c; int diff = 0; NEED_W();
    for (hi = 0; hi < 6; hi++) {
        if (!gc_open(&g, "Arial", hs[hi])) continue;
        for (c = 0x20; c <= 0x7E; c++) {
            INT a, b;
            CHECK(U(GetCharWidthW)(g.dc, c, c, &a)); CHECK(U(GetCharWidth32W)(g.dc, c, c, &b));
            if (a != b) diff++;
            CHECKF(abs(a - b) <= 1, "h=%d ch=%c %d vs %d", hs[hi], c, a, b);
        }
        gc_close(&g);
    }
    if (diff) t_note("GetCharWidthW differs from GetCharWidth32W for %d chars", diff);
}

TEST(values_width32_equals_textextent_arial)
{
    static const int hs[] = { -12, -16, -32 }; GC g; int hi; UINT c; NEED_W();
    for (hi = 0; hi < 3; hi++) {
        if (!gc_open(&g, "Arial", hs[hi])) continue;
        for (c = 0x20; c <= 0x7E; c++) CHECKF(w32(g.dc, c) == tx(g.dc, c), "h=%d ch=%c w=%d tx=%d", hs[hi], c, w32(g.dc, c), tx(g.dc, c));
        gc_close(&g);
    }
}

TEST(values_abc_sum_equals_textextent_arial)
{
    static const int hs[] = { -12, -16, -32 }; GC g; int hi; UINT c; NEED_W();
    for (hi = 0; hi < 3; hi++) {
        if (!gc_open(&g, "Arial", hs[hi])) continue;
        for (c = 0x20; c <= 0x7E; c++) {
            ABC a; CHECK(U(GetCharABCWidthsW)(g.dc, c, c, &a));
            CHECKF(abcsum(&a) == tx(g.dc, c), "h=%d ch=%c abc=%d tx=%d", hs[hi], c, abcsum(&a), tx(g.dc, c));
        }
        gc_close(&g);
    }
}

TEST(values_textextent_string_is_sum_of_chars)
{
    static const WCHAR s[] = L"Hello, World! AVATAR To Wa";
    static const int hs[] = { -12, -16, -32 }; GC g; int hi; unsigned i; NEED_W();
    for (hi = 0; hi < 3; hi++) {
        SIZE sz = { 0, 0 }; int sum = 0;
        if (!gc_open(&g, "Arial", hs[hi])) continue;
        for (i = 0; s[i]; i++) sum += w32(g.dc, s[i]);
        CHECK(U(GetTextExtentPoint32W)(g.dc, s, lstrlenW(s), &sz));
        CHECKF(sz.cx == sum, "h=%d extent %ld sum %d", hs[hi], sz.cx, sum);
        gc_close(&g);
    }
}

TEST(values_A_equals_W_ascii_all_faces)
{
    GC g; int fi, i; UINT c; NEED_W(); REQUIRE(U(GetCharWidth32A)); REQUIRE(U(GetCharWidthA)); REQUIRE(U(GetCharABCWidthsA));
    REQUIRE(U(GetCharWidthFloatA)); REQUIRE(U(GetCharABCWidthsFloatA));
    for (fi = 0; fi < GC_NFACES; fi++) {
        int bad = 0;
        if (!gc_open(&g, gc_faces[fi], -16)) continue;
        for (c = 0x20; c <= 0x7E; c++) {
            INT a, w; ABC aa, aw; float fa, fw; ABCFLOAT ba, bw;
            CHECK(U(GetCharWidth32A)(g.dc, c, c, &a)); CHECK(U(GetCharWidth32W)(g.dc, c, c, &w)); if (a != w) bad++;
            CHECK(U(GetCharWidthA)(g.dc, c, c, &a)); CHECK(U(GetCharWidthW)(g.dc, c, c, &w)); if (a != w) bad++;
            CHECK(U(GetCharABCWidthsA)(g.dc, c, c, &aa)); CHECK(U(GetCharABCWidthsW)(g.dc, c, c, &aw)); if (memcmp(&aa, &aw, sizeof aa)) bad++;
            CHECK(U(GetCharWidthFloatA)(g.dc, c, c, &fa)); CHECK(U(GetCharWidthFloatW)(g.dc, c, c, &fw)); if (fa != fw) bad++;
            CHECK(U(GetCharABCWidthsFloatA)(g.dc, c, c, &ba)); CHECK(U(GetCharABCWidthsFloatW)(g.dc, c, c, &bw)); if (memcmp(&ba, &bw, sizeof ba)) bad++;
        }
        CHECKF(bad == 0, "%s: %d A/W mismatches", g.face, bad);
        gc_close(&g);
    }
}

TEST(values_A_equals_W_latin1_high_half)
{
    GC g; UINT c; int bad = 0; NEED_W(); REQUIRE(U(GetCharWidth32A)); REQUIRE(U(GetCharABCWidthsA));
    if (!gc_open(&g, "Arial", -16)) return;
    for (c = 0xA0; c <= 0xFF; c++) {
        INT a, w; ABC aa, aw;
        CHECK(U(GetCharWidth32A)(g.dc, c, c, &a)); CHECK(U(GetCharWidth32W)(g.dc, c, c, &w));
        CHECKF(a == w, "ch=%x A=%d W=%d", c, a, w);
        CHECK(U(GetCharABCWidthsA)(g.dc, c, c, &aa)); CHECK(U(GetCharABCWidthsW)(g.dc, c, c, &aw));
        CHECKF(!memcmp(&aa, &aw, sizeof aa), "ch=%x ABC differs", c);
    }
    gc_close(&g);
}

TEST(values_A_cp1252_extras_map_to_unicode)
{
    static const struct { UINT a, w; } m[] = { {0x80,0x20AC}, {0x85,0x2026}, {0x91,0x2018}, {0x92,0x2019}, {0x93,0x201C}, {0x94,0x201D}, {0x95,0x2022},
        {0x96,0x2013}, {0x97,0x2014}, {0x99,0x2122}, {0x8A,0x0160}, {0x9C,0x0153}, {0x9F,0x0178} };
    GC g; unsigned i; NEED_W(); REQUIRE(U(GetCharWidth32A));
    if (!gc_open(&g, "Arial", -24)) return;
    for (i = 0; i < sizeof m / sizeof m[0]; i++) {
        INT a, w;
        CHECK(U(GetCharWidth32A)(g.dc, m[i].a, m[i].a, &a)); CHECK(U(GetCharWidth32W)(g.dc, m[i].w, m[i].w, &w));
        CHECKF(a == w && a > 0, "0x%x -> U+%04x: %d vs %d", m[i].a, m[i].w, a, w);
    }
    gc_close(&g);
}

TEST(values_float_close_to_integer_widths)
{
    GC g; int fi, hi; UINT c; NEED_W();
    for (fi = 0; fi < GC_NFACES; fi++) for (hi = 0; hi < GC_NHEIGHTS; hi++) {
        if (!gc_open(&g, gc_faces[fi], gc_heights[hi])) continue;
        for (c = 0x21; c <= 0x7E; c += 3) {
            float f = -1; INT w;
            CHECK(U(GetCharWidthFloatW)(g.dc, c, c, &f)); CHECK(U(GetCharWidth32W)(g.dc, c, c, &w));
            CHECKF(fabsf(f - (float)w) <= 1.5f, "%s %d ch=%c float %f int %d", g.face, g.h, c, f, w);
        }
        gc_close(&g);
    }
}

TEST(values_abcfloat_sum_equals_float_width)
{
    GC g; int fi, hi; UINT c; NEED_W();
    for (fi = 0; fi < 4; fi++) for (hi = 1; hi < 5; hi++) {
        if (!gc_open(&g, gc_faces[fi], gc_heights[hi])) continue;
        for (c = 0x21; c <= 0x7E; c += 2) {
            ABCFLOAT a; float f;
            CHECK(U(GetCharABCWidthsFloatW)(g.dc, c, c, &a)); CHECK(U(GetCharWidthFloatW)(g.dc, c, c, &f));
            CHECKF(fabsf(a.abcfA + a.abcfB + a.abcfC - f) < 0.1f, "%s %d ch=%c sum %f f %f", g.face, g.h, c, a.abcfA + a.abcfB + a.abcfC, f);
            CHECKF(a.abcfB >= 0.0f, "%s ch=%c B=%f", g.face, c, a.abcfB);
        }
        gc_close(&g);
    }
}

TEST(values_abcfloat_close_to_integer_abc)
{
    GC g; UINT c; NEED_W();
    if (!gc_open(&g, "Arial", -48)) return;
    for (c = 0x21; c <= 0x7E; c++) {
        ABCFLOAT f; ABC a;
        CHECK(U(GetCharABCWidthsFloatW)(g.dc, c, c, &f)); CHECK(U(GetCharABCWidthsW)(g.dc, c, c, &a));
        CHECKF(fabsf(f.abcfA - a.abcA) <= 2.0f && fabsf(f.abcfB - (float)a.abcB) <= 2.0f && fabsf(f.abcfC - a.abcC) <= 2.0f,
               "ch=%c float %f,%f,%f int %d,%u,%d", c, f.abcfA, f.abcfB, f.abcfC, a.abcA, a.abcB, a.abcC);
    }
    gc_close(&g);
}

TEST(values_fixed_pitch_fonts_are_uniform)
{
    static const char *const fx[] = { "Courier New", "Consolas" }; static const int hs[] = { -8, -12, -16, -24, -48 };
    GC g; int fi, hi; UINT c; NEED_W();
    for (fi = 0; fi < 2; fi++) for (hi = 0; hi < 5; hi++) {
        TEXTMETRICW tm; int w0;
        if (!gc_open(&g, fx[fi], hs[hi])) continue;
        CHECK(U(GetTextMetricsW)(g.dc, &tm));
        w0 = w32(g.dc, 'A');
        for (c = 0x20; c <= 0x7E; c++) CHECKF(w32(g.dc, c) == w0, "%s %d ch=%c %d vs %d", fx[fi], hs[hi], c, w32(g.dc, c), w0);
        CHECKF(w0 == tm.tmAveCharWidth, "%s %d width %d ave %ld", fx[fi], hs[hi], w0, tm.tmAveCharWidth);
        gc_close(&g);
    }
}

TEST(values_proportional_fonts_vary)
{
    GC g; int fi; NEED_W();
    for (fi = 0; fi < 5; fi++) {
        if (fi == 2 || !gc_open(&g, gc_faces[fi], -24)) continue;
        if (!strcmp(g.face, "Symbol")) { gc_close(&g); continue; }
        CHECKF(w32(g.dc, 'i') < w32(g.dc, 'W'), "%s i=%d W=%d", g.face, w32(g.dc, 'i'), w32(g.dc, 'W'));
        CHECKF(w32(g.dc, 'l') < w32(g.dc, 'm'), "%s l=%d m=%d", g.face, w32(g.dc, 'l'), w32(g.dc, 'm'));
        gc_close(&g);
    }
}

TEST(values_width_scales_with_height)
{
    static const UINT cs[] = { 'M', 'a', 'W', 'i', '0' }; GC g1, g2; unsigned i; int fi, ok1, ok2; NEED_W();
    for (fi = 0; fi < 4; fi++) {
        ok1 = gc_open(&g1, gc_faces[fi], -24); ok2 = gc_open(&g2, gc_faces[fi], -48);
        if (ok1 && ok2) for (i = 0; i < sizeof cs / sizeof cs[0]; i++) {
            int a = w32(g1.dc, cs[i]), b = w32(g2.dc, cs[i]);
            CHECKF(abs(b - 2 * a) <= 3 && a > 0, "%s ch=%c 24px=%d 48px=%d", g1.face, cs[i], a, b);
        }
        if (ok1) gc_close(&g1);
        if (ok2) gc_close(&g2);
    }
}

TEST(values_width_monotonic_in_size_arial)
{
    static const int hs[] = { -8, -12, -16, -24, -48, -100 }; GC g; int hi, prev = 0; NEED_W();
    for (hi = 0; hi < 6; hi++) {
        int w;
        if (!gc_open(&g, "Arial", hs[hi])) continue;
        w = w32(g.dc, 'M');
        CHECKF(w > prev, "size %d width %d prev %d", hs[hi], w, prev);
        prev = w; gc_close(&g);
    }
}

TEST(values_bold_not_narrower)
{
    GC r, b; UINT c; int sr = 0, sb = 0; NEED_W();
    if (!gc_open_ex(&r, "Arial", -24, FW_NORMAL, ANSI_CHARSET, 0)) return;
    if (!gc_open_ex(&b, "Arial", -24, FW_BOLD, ANSI_CHARSET, 0)) { gc_close(&r); return; }
    for (c = 'a'; c <= 'z'; c++) { sr += w32(r.dc, c); sb += w32(b.dc, c); }
    CHECKF(sb > sr, "bold %d regular %d", sb, sr);
    gc_close(&r); gc_close(&b);
}

TEST(values_lfwidth_override_scales_width)
{
    GC n, w; TEXTMETRICW tm; int a, b; NEED_W();
    if (!gc_open(&n, "Arial", -32)) return;
    U(GetTextMetricsW)(n.dc, &tm);
    if (!gc_open_ex(&w, "Arial", -32, FW_NORMAL, ANSI_CHARSET, tm.tmAveCharWidth * 2)) { gc_close(&n); return; }
    a = w32(n.dc, 'M'); b = w32(w.dc, 'M');
    CHECKF(b >= a * 16 / 10 && b <= a * 24 / 10, "natural %d doubled %d", a, b);
    gc_close(&w);
    if (gc_open_ex(&w, "Arial", -32, FW_NORMAL, ANSI_CHARSET, tm.tmAveCharWidth / 2)) {
        b = w32(w.dc, 'M');
        CHECKF(b < a && b >= a * 3 / 10, "natural %d halved %d", a, b);
        gc_close(&w);
    }
    gc_close(&n);
}

TEST(values_escapement_does_not_change_widths)
{
    GC g; LOGFONTA lf; HFONT f; int base[26], i, diff = 0; NEED_W();
    if (!gc_open(&g, "Arial", -24)) return;
    for (i = 0; i < 26; i++) base[i] = w32(g.dc, 'a' + i);
    memset(&lf, 0, sizeof lf); lf.lfHeight = -24; lf.lfEscapement = 900; lf.lfOrientation = 900; lstrcpyA(lf.lfFaceName, "Arial");
    f = U(CreateFontIndirectA)(&lf);
    if (f && gc_setfont(&g, f)) { for (i = 0; i < 26; i++) if (w32(g.dc, 'a' + i) != base[i]) diff++; }
    if (diff) t_note("escapement 900 changed %d widths", diff);
    CHECKF(diff == 0 || gc_wine(), "%d widths differ with escapement", diff);
    gc_close(&g);
}

TEST(values_space_width_positive)
{
    GC g; int fi; NEED_W();
    for (fi = 0; fi < GC_NFACES; fi++) {
        ABC a;
        if (!gc_open(&g, gc_faces[fi], -24)) continue;
        CHECKF(w32(g.dc, ' ') > 0, "%s space width %d", g.face, w32(g.dc, ' '));
        CHECK(U(GetCharABCWidthsW)(g.dc, ' ', ' ', &a));
        CHECKF(abs(abcsum(&a) - w32(g.dc, ' ')) <= 1, "%s", g.face);
        gc_close(&g);
    }
}

TEST(values_missing_glyph_uses_default_char)
{
    GC g; ABC a, d; INT w; NEED_W();
    if (!gc_open(&g, "Arial", -32)) return;
    CHECK(U(GetCharABCWidthsW)(g.dc, 0xE000, 0xE000, &a));
    CHECK(U(GetCharWidth32W)(g.dc, 0xE000, 0xE000, &w));
    CHECKF(abcsum(&a) == w || abs(abcsum(&a) - w) <= 1, "abc %d w %d", abcsum(&a), w);
    CHECK(U(GetCharABCWidthsW)(g.dc, 0xE001, 0xE001, &d));
    CHECKF(!memcmp(&a, &d, sizeof a), "two missing chars have different ABC");
    CHECK(w >= 0);
    gc_close(&g);
}

TEST(values_whole_range_per_font_once)
{
    GC g; int fi; static unsigned char big[0x10000 * 12]; NEED_W();
    for (fi = 0; fi < GC_NFACES; fi++) {
        INT *iw = (INT *)big; ABC *ab = (ABC *)big; ABCFLOAT *af = (ABCFLOAT *)big; float *fw = (float *)big; unsigned c; int neg = 0;
        if (!gc_open(&g, gc_faces[fi], -16)) continue;
        CHECKF(U(GetCharWidth32W)(g.dc, 0, 0xFFFF, iw), "%s 32W", g.face);
        for (c = 0; c < 0x10000; c++) if (iw[c] < 0) neg++;
        CHECKF(neg == 0, "%s negative widths: %d", g.face, neg);
        CHECKF(U(GetCharWidthW)(g.dc, 0, 0xFFFF, iw), "%s W", g.face);
        CHECKF(U(GetCharWidthFloatW)(g.dc, 0, 0xFFFF, fw), "%s FloatW", g.face);
        CHECKF(U(GetCharABCWidthsW)(g.dc, 0, 0xFFFF, ab), "%s ABCW", g.face);
        CHECKF(U(GetCharABCWidthsFloatW)(g.dc, 0, 0xFFFF, af), "%s ABCFloatW", g.face);
        gc_close(&g);
    }
}

TEST(values_chunks_equal_whole_range)
{
    GC g; static INT whole[256]; static ABC wabc[256]; unsigned c, base; NEED_W();
    if (!gc_open(&g, "Times New Roman", -20)) return;
    CHECK(U(GetCharWidth32W)(g.dc, 0, 255, whole)); CHECK(U(GetCharABCWidthsW)(g.dc, 0, 255, wabc));
    for (base = 0; base < 256; base += 37) {
        INT part[64]; ABC pabc[64]; unsigned n = 256 - base < 40 ? 256 - base : 40;
        CHECK(U(GetCharWidth32W)(g.dc, base, base + n - 1, part));
        CHECK(U(GetCharABCWidthsW)(g.dc, base, base + n - 1, pabc));
        for (c = 0; c < n; c++) {
            CHECKF(part[c] == whole[base + c], "w chunk %u+%u", base, c);
            CHECKF(!memcmp(&pabc[c], &wabc[base + c], sizeof(ABC)), "abc chunk %u+%u", base, c);
        }
    }
    gc_close(&g);
}

TEST(values_repeat_calls_identical)
{
    GC g; INT a[256], b[256]; ABC x[256], y[256]; int i; NEED_W();
    if (!gc_open(&g, "Segoe UI", -18)) return;
    for (i = 0; i < 5; i++) {
        CHECK(U(GetCharWidth32W)(g.dc, 0, 255, i & 1 ? a : b)); CHECK(U(GetCharABCWidthsW)(g.dc, 0, 255, i & 1 ? x : y));
        if (i) { CHECK(!memcmp(a, b, sizeof a)); CHECK(!memcmp(x, y, sizeof x)); }
    }
    gc_close(&g);
}

TEST(values_font_switch_changes_and_restores_widths)
{
    GC g; HFONT f2; HGDIOBJ prev; int a1, b, a2; NEED_W();
    if (!gc_open(&g, "Arial", -24)) return;
    a1 = w32(g.dc, 'W');
    f2 = gc_mkfont("Courier New", -24, 400, ANSI_CHARSET, 0);
    CHECK(f2 != NULL);
    prev = U(SelectObject)(g.dc, f2);
    CHECK(prev == (HGDIOBJ)g.font);
    b = w32(g.dc, 'W');
    CHECK(U(SelectObject)(g.dc, prev) == (HGDIOBJ)f2);
    a2 = w32(g.dc, 'W');
    CHECKF(a1 == a2, "restored %d vs %d", a2, a1);
    CHECKF(a1 != b || gc_wine(), "arial %d courier %d", a1, b);
    U(DeleteObject)(f2);
    gc_close(&g);
}

TEST(values_stock_fixed_fonts_uniform_widths)
{
    static const int st[] = { OEM_FIXED_FONT, ANSI_FIXED_FONT, SYSTEM_FIXED_FONT }; HDC dc; unsigned i; UINT c; NEED_W(); REQUIRE(U(GetStockObject));
    dc = U(CreateCompatibleDC)(NULL);
    for (i = 0; i < 3; i++) {
        HGDIOBJ old = U(SelectObject)(dc, U(GetStockObject)(st[i])); TEXTMETRICW tm; int w0;
        CHECK(U(GetTextMetricsW)(dc, &tm));
        w0 = w32(dc, 'A');
        CHECKF(w0 == tm.tmAveCharWidth, "stock %d w %d ave %ld", st[i], w0, tm.tmAveCharWidth);
        for (c = 0x20; c <= 0x7E; c++) CHECKF(w32(dc, c) == w0, "stock %d ch=%c", st[i], c);
        U(SelectObject)(dc, old);
    }
    U(DeleteDC)(dc);
}

TEST(values_system_font_widths_within_tm_bounds)
{
    HDC dc; TEXTMETRICW tm; UINT c; NEED_W(); REQUIRE(U(GetStockObject));
    dc = U(CreateCompatibleDC)(NULL);
    CHECK(U(GetTextMetricsW)(dc, &tm));
    for (c = 0x20; c <= 0x7E; c++) { int w = w32(dc, c); CHECKF(w > 0 && w <= tm.tmMaxCharWidth, "ch=%c w=%d max=%ld", c, w, tm.tmMaxCharWidth); }
    U(DeleteDC)(dc);
}

TEST(values_ascii_widths_within_tm_max_width)
{
    GC g; int fi, hi; UINT c; NEED_W(); REQUIRE(U(GetTextMetricsW));
    for (fi = 0; fi < GC_NFACES; fi++) for (hi = 1; hi < 5; hi++) {
        TEXTMETRICW tm;
        if (!gc_open(&g, gc_faces[fi], gc_heights[hi])) continue;
        CHECK(U(GetTextMetricsW)(g.dc, &tm));
        for (c = 0x20; c <= 0x7E; c++) { int w = w32(g.dc, c); CHECKF(w <= tm.tmMaxCharWidth + 1, "%s %d ch=%c w=%d max=%ld", g.face, g.h, c, w, tm.tmMaxCharWidth); }
        gc_close(&g);
    }
}

TEST(values_mapmode_anisotropic_scales_logical_widths)
{
    GC g1, g2; UINT c; NEED_W(); REQUIRE(U(SetMapMode)); REQUIRE(U(SetWindowExtEx)); REQUIRE(U(SetViewportExtEx));
    if (!gc_open(&g1, "Arial", -32)) return;
    if (!gc_open(&g2, "Arial", -16)) { gc_close(&g1); return; }
    U(SetMapMode)(g2.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(g2.dc, 1, 1, NULL); U(SetViewportExtEx)(g2.dc, 2, 2, NULL);
    /* logical -16 maps to 32 device px; logical widths are device/2 */
    {
        HFONT f = gc_mkfont("Arial", -16, 400, ANSI_CHARSET, 0);
        CHECK(gc_setfont(&g2, f));
    }
    for (c = 'A'; c <= 'Z'; c += 5) {
        int dev = w32(g1.dc, c), lg = w32(g2.dc, c);
        CHECKF(abs(lg * 2 - dev) <= 3, "ch=%c device %d logical %d", c, dev, lg);
    }
    gc_close(&g2); gc_close(&g1);
}

TEST(values_mapmode_text_vs_loenglish_width_units)
{
    GC g; INT w1, w2; HFONT f; NEED_W(); REQUIRE(U(SetMapMode)); REQUIRE(U(GetDeviceCaps));
    if (!gc_open(&g, "Arial", -16)) return;
    w1 = w32(g.dc, 'M');
    U(SetMapMode)(g.dc, MM_LOENGLISH);
    f = gc_mkfont("Arial", -(16 * 100 / U(GetDeviceCaps)(g.dc, LOGPIXELSY)), 400, ANSI_CHARSET, 0);
    CHECK(gc_setfont(&g, f));
    w2 = w32(g.dc, 'M');
    /* logical units are 1/100 inch: convert back to pixels */
    {
        int px = (int)((w2 * U(GetDeviceCaps)(g.dc, LOGPIXELSX) + 50) / 100);
        CHECKF(abs(px - w1) <= 2, "MM_TEXT %d, LOENGLISH %d (=%d px)", w1, w2, px);
    }
    gc_close(&g);
}

TEST(values_italic_abc_bearings_observed)
{
    GC g; ABC a; UINT c; int neg = 0; NEED_W();
    if (!gc_open_ex(&g, "Times New Roman", -64, FW_NORMAL, ANSI_CHARSET, 0)) return;
    { LOGFONTA lf; HFONT f; memset(&lf, 0, sizeof lf); lf.lfHeight = -64; lf.lfItalic = 1; lstrcpyA(lf.lfFaceName, "Times New Roman");
      f = U(CreateFontIndirectA)(&lf); CHECK(gc_setfont(&g, f)); }
    for (c = 'a'; c <= 'z'; c++) { CHECK(U(GetCharABCWidthsW)(g.dc, c, c, &a)); if (a.abcA < 0 || a.abcC < 0) neg++; CHECK(abcsum(&a) == w32(g.dc, c) || abs(abcsum(&a) - w32(g.dc, c)) <= 1); }
    t_note("italic TNR: %d lowercase glyphs with negative A or C", neg);
    gc_close(&g);
}

TEST(values_symbol_font_ranges)
{
    GC g; static INT w[256]; static ABC a[256]; unsigned c; NEED_W();
    if (!gc_open(&g, "Symbol", -24)) return;
    CHECK(U(GetCharWidth32W)(g.dc, 0x20, 0xFF, w)); CHECK(U(GetCharABCWidthsW)(g.dc, 0x20, 0xFF, a));
    for (c = 0; c < 0xE0; c++) CHECKF(w[c] >= 0 && abs(abcsum(&a[c]) - w[c]) <= 1, "ch=%x w=%d abc=%d", c + 0x20, w[c], abcsum(&a[c]));
    CHECK(w['a' - 0x20] > 0);
    gc_close(&g);
}

TEST(values_marlett_glyphs_have_width)
{
    GC g; UINT c; int nz = 0; NEED_W();
    if (!gc_open(&g, "Marlett", -24)) return;
    for (c = 0x30; c <= 0x7A; c++) if (w32(g.dc, c) > 0) nz++;
    CHECKF(nz > 20, "only %d marlett chars with width", nz);
    gc_close(&g);
}

TEST(values_no_gdi_leak_over_font_loop)
{
    int before, after, i; NEED_W();
    before = gc_gdicount();
    for (i = 0; i < 300; i++) {
        GC g;
        if (!gc_open(&g, gc_faces[i % 4], -8 - (i % 20))) { CHECK(0); break; }
        w32(g.dc, 'a'); gc_close(&g);
    }
    after = gc_gdicount();
    if (before >= 0) CHECKF(before == after, "before %d after %d", before, after);
}

struct thr { HDC dc; int w; ABC abc; BOOL r1, r2; };
static DWORD WINAPI thr_proc(LPVOID p)
{
    struct thr *t = p; INT w = -1;
    t->r1 = U(GetCharWidth32W)(t->dc, 'W', 'W', &w); t->w = w;
    t->r2 = U(GetCharABCWidthsW)(t->dc, 'W', 'W', &t->abc);
    return 0;
}
TEST(values_cross_thread_dc_use)
{
    GC g; struct thr t; HANDLE h; ABC a; NEED_W();
    if (!gc_open(&g, "Arial", -24)) return;
    memset(&t, 0, sizeof t); t.dc = g.dc;
    h = CreateThread(NULL, 0, thr_proc, &t, 0, NULL);
    CHECK(h != NULL);
    if (h) { WaitForSingleObject(h, 10000); CloseHandle(h); }
    U(GetCharABCWidthsW)(g.dc, 'W', 'W', &a);
    CHECK(t.r1); CHECK(t.r2);
    CHECK(t.w == w32(g.dc, 'W')); CHECK(!memcmp(&t.abc, &a, sizeof a));
    gc_close(&g);
}

TEST(values_selected_font_delete_still_usable)
{
    GC g; int w1, w2; BOOL del; NEED_W();
    if (!gc_open(&g, "Arial", -24)) return;
    w1 = w32(g.dc, 'W');
    del = U(DeleteObject)(g.font);
    t_note("DeleteObject on selected font -> %d", del);
    w2 = w32(g.dc, 'W');
    CHECKF(w2 > 0, "width after deleting selected font: %d (was %d)", w2, w1);
    if (del) g.font = NULL;
    gc_close(&g);
}

TEST(values_gray_quality_flags_do_not_change_abc_sum)
{
    static const BYTE qs[] = { DEFAULT_QUALITY, DRAFT_QUALITY, PROOF_QUALITY, NONANTIALIASED_QUALITY, ANTIALIASED_QUALITY, CLEARTYPE_QUALITY };
    GC g; unsigned i; NEED_W();
    if (!gc_open(&g, "Arial", -16)) return;
    for (i = 0; i < sizeof qs; i++) {
        LOGFONTA lf; HFONT f; ABC a; INT w;
        memset(&lf, 0, sizeof lf); lf.lfHeight = -16; lf.lfQuality = qs[i]; lstrcpyA(lf.lfFaceName, "Arial");
        f = U(CreateFontIndirectA)(&lf);
        CHECK(gc_setfont(&g, f));
        CHECK(U(GetCharABCWidthsW)(g.dc, 'g', 'g', &a)); CHECK(U(GetCharWidth32W)(g.dc, 'g', 'g', &w));
        CHECKF(abs(abcsum(&a) - w) <= 1, "quality %u: abc %d w %d", qs[i], abcsum(&a), w);
    }
    gc_close(&g);
}
