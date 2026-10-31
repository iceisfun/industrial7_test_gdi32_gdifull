/* GetCharWidthInfo, GdiRealizationInfo, GdiGetCharDimensions, object-count and DPI behaviour. */
#include "gc.h"

typedef struct { INT lsb, rsb, unk; } CWI;
UDECL(BOOL, GetCharWidthInfo, (HDC, void *));
UDECL(BOOL, GdiRealizationInfo, (HDC, void *));
UDECL(LONG, GdiGetCharDimensions, (HDC, TEXTMETRICW *, LONG *));

/* ---- GetCharWidthInfo ---- */
TEST(cwi_invalid_dc_fails)
{
    CWI i; HDC dc;
    REQUIRE(UN(GetCharWidthInfo)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    memset(&i, 0xAA, sizeof i);
    CHECK(!UN(GetCharWidthInfo)(NULL, &i));
    CHECK(!UN(GetCharWidthInfo)((HDC)(ULONG_PTR)0x12345, &i));
    dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    CHECK(!UN(GetCharWidthInfo)(dc, &i));
    CHECKF(i.lsb == 0xAAAAAAAA && i.rsb == 0xAAAAAAAA, "output written on failure");
}

TEST(cwi_null_output_observed)
{
    GC g; BOOL r; GC_REQ_BASE(); REQUIRE(UN(GetCharWidthInfo));
    if (gc_wine()) { t_skip("wine faults on NULL output"); return; }
    if (!gc_open(&g, "Arial", -16)) return;
    r = UN(GetCharWidthInfo)(g.dc, NULL);
    t_note("GetCharWidthInfo(dc, NULL) -> %d err %lu", r, GetLastError());
    CHECK(!r);
    gc_close(&g);
}

TEST(cwi_matches_hhea_at_em_height)
{
    static const char *const fs[] = { "Arial", "Times New Roman", "Tahoma", "Segoe UI", "Courier New" }; GC g; unsigned f; GC_REQ_BASE(); REQUIRE(UN(GetCharWidthInfo)); REQUIRE(U(GetFontData));
    for (f = 0; f < 5; f++) {
        int upem; DWORD hl; BYTE *hhea; CWI i;
        if (!gc_open(&g, fs[f], -16)) continue;
        upem = gc_upem(g.dc); gc_close(&g);
        if (upem <= 0 || !gc_open(&g, fs[f], -upem)) continue;
        hhea = gc_table(g.dc, "hhea", &hl);
        memset(&i, 0xAA, sizeof i);
        if (hhea && hl >= 36) {
            BOOL r = UN(GetCharWidthInfo)(g.dc, &i);
            CHECKF(r, "%s: GetCharWidthInfo failed err %lu", fs[f], GetLastError());
            if (r) {
                CHECKF(abs(i.lsb - gc_be16s(hhea + 12)) <= 1, "%s lsb %d minLSB %d", fs[f], i.lsb, gc_be16s(hhea + 12));
                CHECKF(abs(i.rsb - gc_be16s(hhea + 14)) <= 1, "%s rsb %d minRSB %d", fs[f], i.rsb, gc_be16s(hhea + 14));
                CHECKF(i.unk == 0, "%s unk %d", fs[f], i.unk);
            }
        }
        free(hhea); gc_close(&g);
    }
}

TEST(cwi_scales_with_font_size)
{
    GC g; int upem; CWI big, s3, s4; GC_REQ_BASE(); REQUIRE(UN(GetCharWidthInfo)); REQUIRE(U(GetFontData));
    if (!gc_open(&g, "Tahoma", -16)) { if (!gc_open(&g, "Arial", -16)) return; }
    upem = gc_upem(g.dc); { char face[64]; lstrcpyA(face, g.face); gc_close(&g);
    if (upem <= 0 || !gc_open(&g, face, -upem)) return; }
    CHECK(UN(GetCharWidthInfo)(g.dc, &big));
    gc_close(&g);
    {
        char dummy = 0; (void)dummy;
    }
    if (!gc_open(&g, "Tahoma", -upem / 3)) { if (!gc_open(&g, "Arial", -upem / 3)) return; }
    CHECK(UN(GetCharWidthInfo)(g.dc, &s3));
    CHECKF(abs(s3.lsb - big.lsb / 3) <= 1 && abs(s3.rsb - big.rsb / 3) <= 1, "em (%d,%d) em/3 (%d,%d)", big.lsb, big.rsb, s3.lsb, s3.rsb);
    gc_close(&g);
    if (!gc_open(&g, "Tahoma", -upem / 4)) { if (!gc_open(&g, "Arial", -upem / 4)) return; }
    CHECK(UN(GetCharWidthInfo)(g.dc, &s4));
    CHECKF(abs(s4.lsb - big.lsb / 4) <= 1 && abs(s4.rsb - big.rsb / 4) <= 1, "em (%d,%d) em/4 (%d,%d)", big.lsb, big.rsb, s4.lsb, s4.rsb);
    gc_close(&g);
}

TEST(cwi_bounds_actual_bearings)
{
    GC g; int upem; CWI i; UINT c; int minA = 1 << 30, minC = 1 << 30; GC_REQ_BASE(); REQUIRE(UN(GetCharWidthInfo)); REQUIRE(U(GetFontData)); REQUIRE(U(GetCharABCWidthsW));
    if (!gc_open(&g, "Arial", -16)) return;
    upem = gc_upem(g.dc); gc_close(&g);
    if (upem <= 0 || !gc_open(&g, "Arial", -upem)) return;
    CHECK(UN(GetCharWidthInfo)(g.dc, &i));
    for (c = 0x21; c <= 0x7E; c++) { ABC a; if (U(GetCharABCWidthsW)(g.dc, c, c, &a)) { if (a.abcA < minA) minA = a.abcA; if (a.abcC < minC) minC = a.abcC; } }
    CHECKF(i.lsb <= minA + 1, "lsb %d > min abcA %d", i.lsb, minA);
    CHECKF(i.rsb <= minC + 1, "rsb %d > min abcC %d", i.rsb, minC);
    gc_close(&g);
}

TEST(cwi_repeat_stable_and_font_dependent)
{
    GC a, b; CWI i1, i2, i3; GC_REQ_BASE(); REQUIRE(UN(GetCharWidthInfo));
    if (!gc_open(&a, "Arial", -64)) return;
    if (!gc_open(&b, "Times New Roman", -64)) { gc_close(&a); return; }
    CHECK(UN(GetCharWidthInfo)(a.dc, &i1)); CHECK(UN(GetCharWidthInfo)(a.dc, &i2)); CHECK(UN(GetCharWidthInfo)(b.dc, &i3));
    CHECK(!memcmp(&i1, &i2, sizeof i1));
    t_note("Arial -64 lsb/rsb %d/%d, Times New Roman %d/%d", i1.lsb, i1.rsb, i3.lsb, i3.rsb);
    gc_close(&a); gc_close(&b);
}

TEST(cwi_default_and_raster_fonts_observed)
{
    HDC dc; CWI i; BOOL r; GC_REQ_BASE(); REQUIRE(UN(GetCharWidthInfo));
    dc = U(CreateCompatibleDC)(NULL);
    memset(&i, 0xAA, sizeof i);
    r = UN(GetCharWidthInfo)(dc, &i);
    t_note("default font GetCharWidthInfo -> %d (%d,%d,%d) err %lu", r, i.lsb, i.rsb, i.unk, GetLastError());
    U(DeleteDC)(dc);
}

TEST(cwi_symbol_font_observed)
{
    GC g; CWI i; BOOL r; GC_REQ_BASE(); REQUIRE(UN(GetCharWidthInfo));
    if (!gc_open(&g, "Symbol", -48)) return;
    r = UN(GetCharWidthInfo)(g.dc, &i);
    t_note("Symbol GetCharWidthInfo -> %d (%d,%d,%d)", r, i.lsb, i.rsb, i.unk);
    gc_close(&g);
}

/* ---- GdiRealizationInfo ---- */
TEST(realization_invalid_dc_fails)
{
    BYTE b[16]; HDC dc; GC_REQ_BASE(); REQUIRE(UN(GdiRealizationInfo));
    memset(b, 0xAA, sizeof b);
    CHECK(!UN(GdiRealizationInfo)(NULL, b));
    CHECK(!UN(GdiRealizationInfo)((HDC)(ULONG_PTR)0x12345, b));
    dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    CHECK(!UN(GdiRealizationInfo)(dc, b));
    { int i, same = 1; for (i = 0; i < 16; i++) if (b[i] != 0xAA) same = 0; CHECKF(same, "output touched on failure"); }
}

TEST(realization_writes_documented_prefix_only)
{
    GC g; BYTE b[20]; BOOL r; GC_REQ_BASE(); REQUIRE(UN(GdiRealizationInfo));
    if (!gc_open(&g, "Arial", -16)) return;
    memset(b, 0xAA, sizeof b);
    r = UN(GdiRealizationInfo)(g.dc, b);
    t_note("GdiRealizationInfo(Arial) -> %d: %02x %02x %02x %02x | %02x%02x%02x%02x | %02x%02x%02x%02x", r, b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11]);
    if (r) {
        CHECKF(b[2] == 0 && b[3] == 0, "reserved bytes %02x %02x", b[2], b[3]);
        { int i; for (i = 12; i < 20; i++) CHECKF(b[i] == 0xAA, "byte %d written", i); }
    } else {
        int i; for (i = 0; i < 20; i++) CHECKF(b[i] == 0xAA, "byte %d written on failure", i);
    }
    gc_close(&g);
}

TEST(realization_stable_for_same_font_and_varies_with_font)
{
    GC a, b; BYTE x[16], y[16], z[16]; BOOL r1, r2, r3; GC_REQ_BASE(); REQUIRE(UN(GdiRealizationInfo));
    if (!gc_open(&a, "Arial", -16)) return;
    if (!gc_open(&b, "Courier New", -24)) { gc_close(&a); return; }
    memset(x, 0, 16); memset(y, 0, 16); memset(z, 0, 16);
    r1 = UN(GdiRealizationInfo)(a.dc, x); r2 = UN(GdiRealizationInfo)(a.dc, y); r3 = UN(GdiRealizationInfo)(b.dc, z);
    CHECK(r1 == r2);
    if (r1 && r2) CHECK(!memcmp(x, y, 12));
    t_note("realization: arial %d/%d courier %d, flags arial %02x%02x courier %02x%02x", r1, r2, r3, x[0], x[1], z[0], z[1]);
    gc_close(&a); gc_close(&b);
}

TEST(realization_raster_vs_truetype_flags_observed)
{
    HDC dc; BYTE x[16], y[16]; HGDIOBJ old; BOOL r1, r2; GC g; GC_REQ_BASE(); REQUIRE(UN(GdiRealizationInfo)); REQUIRE(U(GetStockObject));
    if (!gc_open(&g, "Arial", -16)) return;
    memset(x, 0, 16); memset(y, 0, 16);
    r1 = UN(GdiRealizationInfo)(g.dc, x);
    old = U(SelectObject)(g.dc, U(GetStockObject)(SYSTEM_FIXED_FONT));
    r2 = UN(GdiRealizationInfo)(g.dc, y);
    U(SelectObject)(g.dc, old);
    t_note("TrueType flags %02x%02x (%d); raster flags %02x%02x (%d)", x[0], x[1], r1, y[0], y[1], r2);
    gc_close(&g);
}

/* ---- GdiGetCharDimensions ---- */
static LONG expect_dim(HDC dc, TEXTMETRICW *tm)
{
    static const WCHAR az[] = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"; SIZE sz = { 0, 0 };
    U(GetTextMetricsW)(dc, tm);
    if (!(tm->tmPitchAndFamily & TMPF_FIXED_PITCH)) return tm->tmAveCharWidth;
    U(GetTextExtentPoint32W)(dc, az, 52, &sz);
    return (sz.cx / 26 + 1) / 2;
}

TEST(chardim_invalid_dc_returns_zero)
{
    TEXTMETRICW tm; LONG h = 77; HDC dc; HGDIOBJ pen; REQUIRE(UN(GdiGetCharDimensions)); GC_REQ_BASE(); REQUIRE(U(CreatePen));
    EXPECT_EQ(UN(GdiGetCharDimensions)(NULL, NULL, NULL), 0);
    EXPECT_EQ(UN(GdiGetCharDimensions)(NULL, &tm, &h), 0);
    EXPECT_EQ(h, 77);
    EXPECT_EQ(UN(GdiGetCharDimensions)((HDC)(ULONG_PTR)0x12345, NULL, NULL), 0);
    dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    EXPECT_EQ(UN(GdiGetCharDimensions)(dc, NULL, NULL), 0);
    pen = U(CreatePen)(PS_SOLID, 1, 0);
    EXPECT_EQ(UN(GdiGetCharDimensions)((HDC)pen, NULL, NULL), 0);
    U(DeleteObject)(pen);
}

TEST(chardim_matches_formula_variable_pitch)
{
    static const char *const fs[] = { "Arial", "Times New Roman", "Tahoma", "Segoe UI" }; GC g; unsigned f; int hi; REQUIRE(UN(GdiGetCharDimensions)); GC_REQ_BASE(); REQUIRE(U(GetTextMetricsW)); REQUIRE(U(GetTextExtentPoint32W));
    for (f = 0; f < 4; f++) for (hi = 0; hi < GC_NHEIGHTS; hi++) {
        TEXTMETRICW tm, got; LONG h = -1, r, want;
        if (!gc_open(&g, fs[f], gc_heights[hi])) continue;
        want = expect_dim(g.dc, &tm);
        memset(&got, 0, sizeof got);
        r = UN(GdiGetCharDimensions)(g.dc, &got, &h);
        CHECKF(r == want, "%s %d: got %ld want %ld", fs[f], gc_heights[hi], r, want);
        CHECKF(h == tm.tmHeight, "%s %d: height %ld tmHeight %ld", fs[f], gc_heights[hi], h, tm.tmHeight);
        CHECKF(got.tmHeight == tm.tmHeight && got.tmAscent == tm.tmAscent && got.tmDescent == tm.tmDescent && got.tmAveCharWidth == tm.tmAveCharWidth &&
               got.tmMaxCharWidth == tm.tmMaxCharWidth && got.tmWeight == tm.tmWeight && got.tmPitchAndFamily == tm.tmPitchAndFamily && got.tmCharSet == tm.tmCharSet,
               "%s %d: returned TEXTMETRIC differs", fs[f], gc_heights[hi]);
        gc_close(&g);
    }
}

TEST(chardim_fixed_pitch_is_average_width)
{
    static const char *const fs[] = { "Courier New", "Consolas" }; GC g; unsigned f; int hi; REQUIRE(UN(GdiGetCharDimensions)); GC_REQ_BASE(); REQUIRE(U(GetTextMetricsW));
    for (f = 0; f < 2; f++) for (hi = 0; hi < GC_NHEIGHTS; hi++) {
        TEXTMETRICW tm; LONG r;
        if (!gc_open(&g, fs[f], gc_heights[hi])) continue;
        U(GetTextMetricsW)(g.dc, &tm);
        r = UN(GdiGetCharDimensions)(g.dc, NULL, NULL);
        CHECKF(!(tm.tmPitchAndFamily & TMPF_FIXED_PITCH), "%s should be fixed pitch", fs[f]);
        CHECKF(r == tm.tmAveCharWidth, "%s %d: got %ld ave %ld", fs[f], gc_heights[hi], r, tm.tmAveCharWidth);
        gc_close(&g);
    }
}

TEST(chardim_null_optional_outputs)
{
    GC g; LONG r1, r2, h = -1; TEXTMETRICW tm; REQUIRE(UN(GdiGetCharDimensions)); GC_REQ_BASE();
    if (!gc_open(&g, "Tahoma", -13)) { if (!gc_open(&g, "Arial", -13)) return; }
    r1 = UN(GdiGetCharDimensions)(g.dc, NULL, NULL);
    r2 = UN(GdiGetCharDimensions)(g.dc, &tm, NULL);
    CHECK(r1 > 0 && r1 == r2);
    r2 = UN(GdiGetCharDimensions)(g.dc, NULL, &h);
    CHECK(r1 == r2 && h > 0);
    gc_close(&g);
}

TEST(chardim_repeat_and_font_switch)
{
    GC g; LONG a1, a2, a3, b; HFONT f; REQUIRE(UN(GdiGetCharDimensions)); GC_REQ_BASE();
    if (!gc_open(&g, "Arial", -16)) return;
    a1 = UN(GdiGetCharDimensions)(g.dc, NULL, NULL); a2 = UN(GdiGetCharDimensions)(g.dc, NULL, NULL);
    f = gc_mkfont("Arial", -48, 400, ANSI_CHARSET, 0);
    U(SelectObject)(g.dc, f);
    b = UN(GdiGetCharDimensions)(g.dc, NULL, NULL);
    U(SelectObject)(g.dc, g.font); U(DeleteObject)(f);
    a3 = UN(GdiGetCharDimensions)(g.dc, NULL, NULL);
    CHECKF(a1 == a2 && a2 == a3 && a1 > 0, "%ld %ld %ld", a1, a2, a3);
    CHECKF(b > 2 * a1, "48px %ld vs 16px %ld", b, a1);
    gc_close(&g);
}

TEST(chardim_stock_fonts_formula)
{
    static const int st[] = { SYSTEM_FONT, OEM_FIXED_FONT, ANSI_FIXED_FONT, ANSI_VAR_FONT, SYSTEM_FIXED_FONT, DEFAULT_GUI_FONT }; HDC dc; unsigned i;
    REQUIRE(UN(GdiGetCharDimensions)); GC_REQ_BASE(); REQUIRE(U(GetStockObject)); REQUIRE(U(GetTextMetricsW)); REQUIRE(U(GetTextExtentPoint32W));
    dc = U(CreateCompatibleDC)(NULL);
    for (i = 0; i < 6; i++) {
        TEXTMETRICW tm; LONG want, r; HGDIOBJ old = U(SelectObject)(dc, U(GetStockObject)(st[i]));
        want = expect_dim(dc, &tm); r = UN(GdiGetCharDimensions)(dc, NULL, NULL);
        CHECKF(r == want, "stock %d: got %ld want %ld", st[i], r, want);
        U(SelectObject)(dc, old);
    }
    U(DeleteDC)(dc);
}

TEST(chardim_vs_dialog_base_units_observed)
{
    HDC dc; LONG r, bu; HGDIOBJ old; REQUIRE(UN(GdiGetCharDimensions)); GC_REQ_BASE(); REQUIRE(U(GetStockObject)); REQUIRE(U(GetDialogBaseUnits));
    dc = U(CreateCompatibleDC)(NULL);
    bu = U(GetDialogBaseUnits)();
    old = U(SelectObject)(dc, U(GetStockObject)(SYSTEM_FONT));
    r = UN(GdiGetCharDimensions)(dc, NULL, NULL);
    t_note("GetDialogBaseUnits %d,%d; GdiGetCharDimensions(SYSTEM_FONT) %ld", LOWORD(bu), HIWORD(bu), r);
    U(SelectObject)(dc, old);
    old = U(SelectObject)(dc, U(GetStockObject)(DEFAULT_GUI_FONT));
    r = UN(GdiGetCharDimensions)(dc, NULL, NULL);
    t_note("GdiGetCharDimensions(DEFAULT_GUI_FONT) %ld", r);
    U(SelectObject)(dc, old);
    CHECK(LOWORD(bu) > 0 && HIWORD(bu) > 0);
    U(DeleteDC)(dc);
}

TEST(chardim_no_gdi_leak)
{
    GC g; int before, after, i; REQUIRE(UN(GdiGetCharDimensions)); GC_REQ_BASE();
    if (!gc_open(&g, "Arial", -16)) return;
    before = gc_gdicount();
    for (i = 0; i < 500; i++) UN(GdiGetCharDimensions)(g.dc, NULL, NULL);
    after = gc_gdicount();
    if (before >= 0) CHECKF(before == after, "%d -> %d", before, after);
    gc_close(&g);
}

/* ---- object counts, handle lifetime, DPI ---- */
TEST(misc_font_handles_counted_by_getguiresources)
{
    enum { N = 400 }; static HFONT fonts[N]; int before, mid, after, i; GC_REQ_BASE(); REQUIRE(U(GetGuiResources));
    before = gc_gdicount();
    if (before < 0) { t_skip("GetGuiResources unusable"); return; }
    for (i = 0; i < N; i++) fonts[i] = gc_mkfont("Arial", -8 - (i % 40), 400 + (i % 3) * 100, ANSI_CHARSET, 0);
    mid = gc_gdicount();
    CHECKF(mid >= before + N, "created %d fonts, count %d -> %d", N, before, mid);
    if (mid != before + N) t_note("count delta %d for %d fonts", mid - before, N);
    for (i = 0; i < N; i++) CHECK(U(DeleteObject)(fonts[i]));
    after = gc_gdicount();
    CHECKF(after == before, "after delete %d vs before %d", after, before);
}

TEST(misc_deleted_font_handle_is_not_selectable)
{
    GC g; HFONT f; HGDIOBJ r; GC_REQ_BASE();
    if (!gc_open(&g, "Arial", -16)) return;
    f = gc_mkfont("Arial", -20, 400, ANSI_CHARSET, 0);
    CHECK(U(DeleteObject)(f));
    SetLastError(GC_ERRSENT);
    r = U(SelectObject)(g.dc, f);
    CHECKF(r == NULL, "selecting deleted font returned %p", r);
    CHECK(!U(DeleteObject)(f));
    { INT w; CHECK(U(GetCharWidth32W)(g.dc, 'A', 'A', &w) && w > 0); }
    gc_close(&g);
}

TEST(misc_width_queries_survive_stock_font_selection)
{
    GC g; INT w1, w2; HGDIOBJ old; GC_REQ_BASE(); REQUIRE(U(GetStockObject)); REQUIRE(U(GetCharWidth32W));
    if (!gc_open(&g, "Arial", -24)) return;
    CHECK(U(GetCharWidth32W)(g.dc, 'M', 'M', &w1));
    old = U(SelectObject)(g.dc, U(GetStockObject)(ANSI_VAR_FONT));
    CHECK(old == (HGDIOBJ)g.font);
    CHECK(U(GetCharWidth32W)(g.dc, 'M', 'M', &w2));
    U(SelectObject)(g.dc, old);
    CHECK(U(GetCharWidth32W)(g.dc, 'M', 'M', &w2) && w1 == w2);
    gc_close(&g);
}

TEST(misc_point_size_widths_follow_logpixels)
{
    GC g; int dpiy, h12, h24; INT w12, w24; HFONT f; GC_REQ_BASE(); REQUIRE(U(GetDeviceCaps)); REQUIRE(U(GetCharWidth32W));
    if (!gc_open(&g, "Arial", -16)) return;
    dpiy = U(GetDeviceCaps)(g.dc, LOGPIXELSY);
    CHECK(dpiy >= 96);
    t_note("LOGPIXELSY=%d LOGPIXELSX=%d", dpiy, U(GetDeviceCaps)(g.dc, LOGPIXELSX));
    h12 = -MulDiv(12, dpiy, 72); h24 = -MulDiv(24, dpiy, 72);
    f = gc_mkfont("Arial", h12, 400, ANSI_CHARSET, 0); CHECK(gc_setfont(&g, f));
    CHECK(U(GetCharWidth32W)(g.dc, 'M', 'M', &w12));
    f = gc_mkfont("Arial", h24, 400, ANSI_CHARSET, 0); CHECK(gc_setfont(&g, f));
    CHECK(U(GetCharWidth32W)(g.dc, 'M', 'M', &w24));
    CHECKF(abs(w24 - 2 * w12) <= 3 && w12 > 0, "12pt %d, 24pt %d at %d dpi", w12, w24, dpiy);
    gc_close(&g);
}

TEST(misc_em_width_of_M_arial_matches_hmtx)
{
    GC g; int upem; DWORD hl, ml, hh; BYTE *hhea, *hmtx; WCHAR c = 'M'; WORD gid; GC_REQ_BASE(); REQUIRE(U(GetGlyphIndicesW)); REQUIRE(U(GetFontData));
    if (!gc_open(&g, "Arial", -16)) return;
    upem = gc_upem(g.dc); gc_close(&g);
    if (upem <= 0 || !gc_open(&g, "Arial", -upem)) return;
    hhea = gc_table(g.dc, "hhea", &hh); hmtx = gc_table(g.dc, "hmtx", &ml);
    U(GetGlyphIndicesW)(g.dc, &c, 1, &gid, 0);
    if (hhea && hmtx) {
        int nh = gc_be16(hhea + 34), adv = gc_be16(hmtx + (gid < nh ? gid : nh - 1) * 4); INT w; ABC a;
        CHECK(U(GetCharWidth32W)(g.dc, 'M', 'M', &w));
        CHECKF(abs(w - adv) <= 1, "width %d hmtx advance %d", w, adv);
        CHECK(U(GetCharABCWidthsW)(g.dc, 'M', 'M', &a));
        CHECKF(abs((a.abcA + (int)a.abcB + a.abcC) - adv) <= 1, "abc sum %d hmtx advance %d", a.abcA + (int)a.abcB + a.abcC, adv);
        {
            float f; CHECK(U(GetCharWidthFloatW)(g.dc, 'M', 'M', &f)); CHECKF(fabsf(f - (float)adv) <= 1.0f, "float width %f advance %d", f, adv);
        }
    }
    free(hhea); free(hmtx);
    gc_close(&g);
}
