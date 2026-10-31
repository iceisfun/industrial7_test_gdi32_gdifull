/* Parameter validation of the GetCharWidth* / GetCharABCWidths* families. */
#include "gc.h"

enum { K_W, K_32W, K_FW, K_A, K_32A, K_FA, K_ABCA, K_ABCW, K_ABCFA, K_ABCFW, K_N };
static const char *const kname[K_N] = { "GetCharWidthW", "GetCharWidth32W", "GetCharWidthFloatW", "GetCharWidthA", "GetCharWidth32A",
    "GetCharWidthFloatA", "GetCharABCWidthsA", "GetCharABCWidthsW", "GetCharABCWidthsFloatA", "GetCharABCWidthsFloatW" };
static int k_abc(int k) { return k >= K_ABCA; }
static int k_ansi(int k) { return k == K_A || k == K_32A || k == K_FA || k == K_ABCA || k == K_ABCFA; }
static int k_elem(int k) { return k_abc(k) ? 12 : 4; }

static BOOL kcall(int k, HDC dc, UINT f, UINT l, void *b)
{
    switch (k) {
    case K_W: return U(GetCharWidthW)(dc, f, l, b);
    case K_32W: return U(GetCharWidth32W)(dc, f, l, b);
    case K_FW: return U(GetCharWidthFloatW)(dc, f, l, b);
    case K_A: return U(GetCharWidthA)(dc, f, l, b);
    case K_32A: return U(GetCharWidth32A)(dc, f, l, b);
    case K_FA: return U(GetCharWidthFloatA)(dc, f, l, b);
    case K_ABCA: return U(GetCharABCWidthsA)(dc, f, l, b);
    case K_ABCW: return U(GetCharABCWidthsW)(dc, f, l, b);
    case K_ABCFA: return U(GetCharABCWidthsFloatA)(dc, f, l, b);
    default: return U(GetCharABCWidthsFloatW)(dc, f, l, b);
    }
}
#define KREQ() do { REQUIRE(U(GetCharWidthW)); REQUIRE(U(GetCharWidth32W)); REQUIRE(U(GetCharWidthFloatW)); REQUIRE(U(GetCharWidthA)); \
    REQUIRE(U(GetCharWidth32A)); REQUIRE(U(GetCharWidthFloatA)); REQUIRE(U(GetCharABCWidthsA)); REQUIRE(U(GetCharABCWidthsW)); \
    REQUIRE(U(GetCharABCWidthsFloatA)); REQUIRE(U(GetCharABCWidthsFloatW)); GC_REQ_BASE(); } while (0)

static unsigned char gbuf[0x10000 * 12 + 64] __attribute__((aligned(16)));

TEST(args_null_buffer)
{
    GC g; int k; KREQ();
    if (!gc_open(&g, "Arial", -16)) return;
    for (k = 0; k < K_N; k++) {
        BOOL r; DWORD e;
        if (gc_wine() && !k_abc(k)) continue; /* wine hangs on a NULL buffer */
        SetLastError(GC_ERRSENT);
        r = kcall(k, g.dc, 'a', 'a', NULL); e = GetLastError();
        CHECKF(!r, "%s", kname[k]);
        CHECKF(e == ERROR_INVALID_PARAMETER, "%s err %lu", kname[k], e);
    }
    gc_close(&g);
}

TEST(args_first_greater_than_last)
{
    GC g; int k; static const UINT pr[][2] = { {'b','a'}, {1,0}, {0xFF,0}, {0x41,0x40}, {0x80,0x7F} };
    unsigned i; KREQ();
    if (!gc_open(&g, "Arial", -16)) return;
    for (k = 0; k < K_N; k++)
        for (i = 0; i < sizeof pr / sizeof pr[0]; i++) {
            BOOL r; DWORD e;
            if (gc_wine() && pr[i][0] - pr[i][1] > 1) continue; /* wine overruns the buffer */
            SetLastError(GC_ERRSENT);
            r = kcall(k, g.dc, pr[i][0], pr[i][1], gbuf); e = GetLastError();
            CHECKF(!r, "%s %x>%x", kname[k], pr[i][0], pr[i][1]);
            CHECKF(e == ERROR_INVALID_PARAMETER, "%s %x>%x err %lu", kname[k], pr[i][0], pr[i][1], e);
        }
    gc_close(&g);
}

TEST(args_null_dc)
{
    int k; KREQ();
    for (k = 0; k < K_N; k++) {
        BOOL r; DWORD e;
        SetLastError(GC_ERRSENT);
        r = kcall(k, NULL, 'a', 'a', gbuf); e = GetLastError();
        CHECKF(!r, "%s", kname[k]);
        if (!k_abc(k)) CHECKF(e == ERROR_INVALID_PARAMETER, "%s err %lu", kname[k], e);
        else if (e != ERROR_INVALID_PARAMETER) t_note("%s NULL dc err %lu", kname[k], e);
    }
}

TEST(args_bogus_dc_handles)
{
    static const ULONG_PTR hs[] = { 1, 0x12345, 0x01010101, 0x7FFFFFFF, 0xFFFFFFFF, 0x00050001 };
    int k; unsigned i; KREQ();
    for (k = 0; k < K_N; k++)
        for (i = 0; i < sizeof hs / sizeof hs[0]; i++) {
            BOOL r; DWORD e;
            SetLastError(GC_ERRSENT);
            r = kcall(k, (HDC)hs[i], 'a', 'a', gbuf); e = GetLastError();
            CHECKF(!r, "%s h=%llx", kname[k], (unsigned long long)hs[i]);
            if (!k_abc(k)) CHECKF(e == ERROR_INVALID_PARAMETER, "%s h=%llx err %lu", kname[k], (unsigned long long)hs[i], e);
        }
}

TEST(args_wrong_object_type_as_dc)
{
    HGDIOBJ objs[6]; const char *nm[6] = { "pen", "brush", "font", "bitmap", "stock pen", "palette-less stock font" };
    int k, i; KREQ();
    REQUIRE(U(CreatePen)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(CreateBitmap)); REQUIRE(U(GetStockObject));
    objs[0] = U(CreatePen)(PS_SOLID, 1, 0); objs[1] = U(CreateSolidBrush)(0xFF);
    objs[2] = U(CreateFontIndirectA) ? gc_mkfont("Arial", -12, 400, ANSI_CHARSET, 0) : NULL;
    objs[3] = U(CreateBitmap)(4, 4, 1, 1, NULL); objs[4] = U(GetStockObject)(BLACK_PEN); objs[5] = U(GetStockObject)(SYSTEM_FONT);
    for (i = 0; i < 6; i++) {
        if (!objs[i]) continue;
        for (k = 0; k < K_N; k++) {
            BOOL r; DWORD e;
            SetLastError(GC_ERRSENT);
            r = kcall(k, (HDC)objs[i], 'a', 'a', gbuf); e = GetLastError();
            CHECKF(!r, "%s with %s", kname[k], nm[i]);
            if (!k_abc(k)) CHECKF(e == ERROR_INVALID_PARAMETER, "%s with %s err %lu", kname[k], nm[i], e);
        }
    }
    U(DeleteObject)(objs[0]); U(DeleteObject)(objs[1]); if (objs[2]) U(DeleteObject)(objs[2]); U(DeleteObject)(objs[3]);
}

TEST(args_deleted_dc)
{
    int k; KREQ();
    for (k = 0; k < K_N; k++) {
        HDC dc = U(CreateCompatibleDC)(NULL); BOOL r; DWORD e;
        CHECK(dc != NULL);
        CHECK(U(DeleteDC)(dc));
        SetLastError(GC_ERRSENT);
        r = kcall(k, dc, 'a', 'a', gbuf); e = GetLastError();
        CHECKF(!r, "%s", kname[k]);
        if (!k_abc(k)) CHECKF(e == ERROR_INVALID_PARAMETER, "%s err %lu", kname[k], e);
    }
}

TEST(args_ansi_last_over_ff)
{
    GC g; int k; static const UINT pr[][2] = { {0,0x100}, {0x100,0x100}, {0xFF,0x100}, {0,0xFFFF}, {0x1FF,0xFF00}, {0xFFFF,0xFFFF}, {0x10000,0x10000} };
    unsigned i; KREQ();
    if (!gc_open(&g, "Arial", -16)) return;
    for (k = 0; k < K_N; k++) {
        if (!k_ansi(k)) continue;
        for (i = 0; i < sizeof pr / sizeof pr[0]; i++) {
            BOOL r; DWORD e;
            SetLastError(GC_ERRSENT);
            r = kcall(k, g.dc, pr[i][0], pr[i][1], gbuf); e = GetLastError();
            CHECKF(!r, "%s %x-%x", kname[k], pr[i][0], pr[i][1]);
            CHECKF(e == ERROR_INVALID_PARAMETER, "%s %x-%x err %lu", kname[k], pr[i][0], pr[i][1], e);
        }
        CHECKF(kcall(k, g.dc, 0xFF, 0xFF, gbuf), "%s ff-ff", kname[k]);
        CHECKF(kcall(k, g.dc, 0, 0xFF, gbuf), "%s 0-ff", kname[k]);
    }
    gc_close(&g);
}

TEST(args_wide_width_range_limits)
{
    GC g; int k; static const UINT bad[][2] = { {0x10000,0x10000}, {0xFFFF,0x10000}, {0xFFFFFF,0xFFFFFF}, {0x1000000,0x1000000},
        {0xFFFFFFFF,0xFFFFFFFF}, {0,0x10000}, {0,0xFFFFFFFF} };
    unsigned i; KREQ();
    if (!gc_open(&g, "Arial", -16)) return;
    for (k = 0; k < K_N; k++) {
        if (k_ansi(k) || k_abc(k)) continue;
        for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            BOOL r; DWORD e;
            SetLastError(GC_ERRSENT);
            r = kcall(k, g.dc, bad[i][0], bad[i][1], gbuf); e = GetLastError();
            CHECKF(!r, "%s %x-%x", kname[k], bad[i][0], bad[i][1]);
            CHECKF(e == ERROR_INVALID_PARAMETER, "%s %x-%x err %lu", kname[k], bad[i][0], bad[i][1], e);
        }
        CHECKF(kcall(k, g.dc, 0xFFFF, 0xFFFF, gbuf), "%s ffff", kname[k]);
        CHECKF(kcall(k, g.dc, 0x100, 0x100, gbuf), "%s 100", kname[k]);
    }
    gc_close(&g);
}

TEST(args_abc_wide_range_limits_observed)
{
    GC g; static const UINT bad[][2] = { {0x10000,0x10000}, {0xFFFF,0x10000}, {0xFFFFFF,0xFFFFFF}, {0xFFFFFFFF,0xFFFFFFFF} };
    unsigned i; int k; KREQ();
    if (!gc_open(&g, "Arial", -16)) return;
    for (k = K_ABCW; k <= K_ABCFW; k++) {
        if (k_ansi(k)) continue;
        for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            BOOL r = kcall(k, g.dc, bad[i][0], bad[i][1], gbuf);
            if (r) t_note("%s %x-%x unexpectedly succeeded", kname[k], bad[i][0], bad[i][1]);
        }
        CHECKF(kcall(k, g.dc, 0xFFFF, 0xFFFF, gbuf), "%s ffff", kname[k]);
    }
    gc_close(&g);
}

TEST(args_failure_leaves_buffer_untouched)
{
    GC g; int k, i; KREQ();
    if (!gc_open(&g, "Arial", -16)) return;
    for (k = 0; k < K_N; k++) {
        memset(gbuf, 0xCC, 64);
        kcall(k, g.dc, 'b', 'a', gbuf);
        for (i = 0; i < 64; i++) if (gbuf[i] != 0xCC) { CHECKF(0, "%s byte %d changed", kname[k], i); break; }
    }
    gc_close(&g);
}

TEST(args_single_char_succeeds_all_kinds)
{
    GC g; int k; KREQ();
    if (!gc_open(&g, "Arial", -16)) return;
    for (k = 0; k < K_N; k++) {
        memset(gbuf, 0xCC, 32);
        CHECKF(kcall(k, g.dc, 'A', 'A', gbuf), "%s", kname[k]);
        CHECKF(gbuf[k_elem(k)] == 0xCC, "%s wrote past one element", kname[k]);
    }
    gc_close(&g);
}

TEST(args_exact_extent_written)
{
    GC g; int k; KREQ();
    if (!gc_open(&g, "Arial", -16)) return;
    for (k = 0; k < K_N; k++) {
        int n = 10, es = k_elem(k);
        memset(gbuf, 0x5A, 200);
        CHECKF(kcall(k, g.dc, 'A', 'A' + n - 1, gbuf), "%s", kname[k]);
        CHECKF(gbuf[n * es] == 0x5A && gbuf[n * es + 1] == 0x5A, "%s overran %d elements", kname[k], n);
    }
    gc_close(&g);
}

TEST(args_default_dc_font_works)
{
    HDC dc; int k; KREQ();
    dc = U(CreateCompatibleDC)(NULL);
    CHECK(dc != NULL);
    for (k = 0; k < K_N; k++) CHECKF(kcall(k, dc, 'a', 'z', gbuf), "%s on default font", kname[k]);
    U(DeleteDC)(dc);
}

TEST(args_stock_bitmap_fonts_width_ok)
{
    static const int st[] = { SYSTEM_FONT, OEM_FIXED_FONT, ANSI_FIXED_FONT, ANSI_VAR_FONT, SYSTEM_FIXED_FONT, DEFAULT_GUI_FONT, DEVICE_DEFAULT_FONT };
    HDC dc; unsigned i; int k; KREQ(); REQUIRE(U(GetStockObject));
    dc = U(CreateCompatibleDC)(NULL);
    for (i = 0; i < sizeof st / sizeof st[0]; i++) {
        HGDIOBJ o = U(GetStockObject)(st[i]), old;
        if (!o) continue;
        old = U(SelectObject)(dc, o);
        for (k = 0; k < K_N; k++) {
            if (k_abc(k)) continue;
            CHECKF(kcall(k, dc, 'a', 'z', gbuf), "%s stock font %d", kname[k], st[i]);
        }
        U(SelectObject)(dc, old);
    }
    U(DeleteDC)(dc);
}

TEST(args_abc_on_raster_font_observed)
{
    GC g; int k; KREQ();
    if (!gc_open_ex(&g, "Terminal", -12, 400, OEM_CHARSET, 0)) return;
    for (k = 0; k < K_N; k++) {
        if (!k_abc(k)) continue;
        if (!kcall(k, g.dc, 'a', 'a', gbuf)) t_note("%s fails on raster font (err %lu)", kname[k], GetLastError());
    }
    gc_close(&g);
}

TEST(args_no_gdi_leak_when_failing)
{
    GC g; int k, i, before, after; KREQ();
    if (!gc_open(&g, "Arial", -16)) return;
    before = gc_gdicount();
    if (before < 0) { gc_close(&g); t_skip("GetGuiResources unusable (wine)"); return; }
    for (i = 0; i < 20; i++) for (k = 0; k < K_N; k++) { kcall(k, g.dc, 'z', 'a', gbuf); kcall(k, NULL, 'a', 'a', gbuf); }
    after = gc_gdicount();
    if (before >= 0) CHECKF(before == after, "before %d after %d", before, after);
    gc_close(&g);
}

/* ---- GetCharWidthI / GetCharABCWidthsI ---- */
TEST(args_widthI_null_widths)
{
    GC g; WORD gi[2] = { 3, 4 }; BOOL r; DWORD e;
    REQUIRE(U(GetCharWidthI)); GC_REQ_BASE();
    if (!gc_open(&g, "Arial", -16)) return;
    SetLastError(GC_ERRSENT);
    r = U(GetCharWidthI)(g.dc, 0, 2, gi, NULL); e = GetLastError();
    CHECK(!r); EXPECT_EQ(e, ERROR_INVALID_PARAMETER);
    SetLastError(GC_ERRSENT);
    r = U(GetCharWidthI)(g.dc, 0, 2, NULL, NULL); e = GetLastError();
    CHECK(!r); EXPECT_EQ(e, ERROR_INVALID_PARAMETER);
    gc_close(&g);
}

TEST(args_widthI_null_pgi_with_large_first)
{
    GC g; int w[4]; BOOL r; DWORD e;
    REQUIRE(U(GetCharWidthI)); GC_REQ_BASE();
    if (!gc_open(&g, "Arial", -16)) return;
    SetLastError(GC_ERRSENT);
    r = U(GetCharWidthI)(g.dc, 0x10000, 1, NULL, w); e = GetLastError();
    CHECK(!r); EXPECT_EQ(e, ERROR_INVALID_PARAMETER);
    SetLastError(GC_ERRSENT);
    r = U(GetCharWidthI)(g.dc, 0xFFFF0000u, 1, NULL, w); e = GetLastError();
    CHECK(!r); EXPECT_EQ(e, ERROR_INVALID_PARAMETER);
    SetLastError(GC_ERRSENT);
    r = U(GetCharWidthI)(g.dc, 0x1FFFF, 0, NULL, w); e = GetLastError();
    CHECK(!r); EXPECT_EQ(e, ERROR_INVALID_PARAMETER);
    gc_close(&g);
}

TEST(args_widthI_zero_count_is_success_without_dc_check)
{
    int w[2] = { 7, 7 }; WORD gi[1] = { 0 };
    REQUIRE(U(GetCharWidthI));
    /* decompiled: cgi == 0 returns TRUE before any handle validation */
    CHECK(U(GetCharWidthI)(NULL, 0, 0, gi, w));
    CHECK(U(GetCharWidthI)((HDC)(ULONG_PTR)0x12345, 5, 0, NULL, w));
    CHECK(U(GetCharWidthI)(NULL, 0, 0, NULL, w));
    EXPECT_EQ(w[0], 7); EXPECT_EQ(w[1], 7);
}

TEST(args_widthI_null_dc_with_count)
{
    WORD gi[1] = { 3 }; int w[1];
    REQUIRE(U(GetCharWidthI));
    CHECK(!U(GetCharWidthI)(NULL, 0, 1, gi, w));
    CHECK(!U(GetCharWidthI)((HDC)(ULONG_PTR)0x77777, 0, 1, gi, w));
}

TEST(args_widthI_contiguous_range_matches_array)
{
    GC g; int wa[8], wb[8]; WORD gi[8]; int i; BOOL r1, r2;
    REQUIRE(U(GetCharWidthI)); GC_REQ_BASE();
    if (!gc_open(&g, "Arial", -16)) return;
    for (i = 0; i < 8; i++) gi[i] = (WORD)(36 + i);
    r1 = U(GetCharWidthI)(g.dc, 36, 8, NULL, wa);
    r2 = U(GetCharWidthI)(g.dc, 0, 8, gi, wb);
    CHECK(r1); CHECK(r2);
    for (i = 0; i < 8; i++) CHECKF(wa[i] == wb[i] && wa[i] > 0, "glyph %d: %d vs %d", 36 + i, wa[i], wb[i]);
    gc_close(&g);
}

TEST(args_abcI_null_args)
{
    GC g; ABC abc[2]; WORD gi[2] = { 3, 4 }; BOOL r;
    REQUIRE(U(GetCharABCWidthsI)); GC_REQ_BASE();
    CHECK(!U(GetCharABCWidthsI)(NULL, 0, 1, gi, abc));
    CHECK(!U(GetCharABCWidthsI)((HDC)(ULONG_PTR)0x99999, 0, 1, gi, abc));
    if (!gc_open(&g, "Arial", -16)) return;
    CHECK(!U(GetCharABCWidthsI)(g.dc, 0, 1, gi, NULL));
    r = U(GetCharABCWidthsI)(g.dc, 0, 2, gi, abc);
    CHECK(r);
    gc_close(&g);
}

TEST(args_abcI_zero_count_observed)
{
    GC g; ABC abc[2]; WORD gi[2] = { 3, 4 }; BOOL r;
    REQUIRE(U(GetCharABCWidthsI)); GC_REQ_BASE();
    if (!gc_open(&g, "Arial", -16)) return;
    SetLastError(GC_ERRSENT);
    r = U(GetCharABCWidthsI)(g.dc, 0, 0, gi, abc);
    t_note("GetCharABCWidthsI cgi=0 -> %d err %lu", r, GetLastError());
    gc_close(&g);
}

TEST(args_abcI_contiguous_matches_array)
{
    GC g; ABC a[6], b[6]; WORD gi[6]; int i;
    REQUIRE(U(GetCharABCWidthsI)); GC_REQ_BASE();
    if (!gc_open(&g, "Arial", -24)) return;
    for (i = 0; i < 6; i++) gi[i] = (WORD)(40 + i);
    CHECK(U(GetCharABCWidthsI)(g.dc, 40, 6, NULL, a));
    CHECK(U(GetCharABCWidthsI)(g.dc, 0, 6, gi, b));
    CHECK(!memcmp(a, b, sizeof a));
    gc_close(&g);
}
