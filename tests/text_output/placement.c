#include "to.h"
/* GetCharacterPlacementA/W and the ETO_* option flags. */

#ifndef GCP_DBCS
#define GCP_DBCS 0x1
#endif
#ifndef GCP_REORDER
#define GCP_REORDER 0x2
#endif
#ifndef GCP_USEKERNING
#define GCP_USEKERNING 0x8
#endif
#ifndef GCP_GLYPHSHAPE
#define GCP_GLYPHSHAPE 0x10
#endif
#ifndef GCP_LIGATE
#define GCP_LIGATE 0x20
#endif
#ifndef GCP_DIACRITIC
#define GCP_DIACRITIC 0x100
#endif
#ifndef GCP_KASHIDA
#define GCP_KASHIDA 0x400
#endif
#ifndef GCP_JUSTIFY
#define GCP_JUSTIFY 0x10000
#endif
#ifndef GCP_CLASSIN
#define GCP_CLASSIN 0x80000
#endif
#ifndef GCP_MAXEXTENT
#define GCP_MAXEXTENT 0x100000
#endif
#ifndef GCP_JUSTIFYIN
#define GCP_JUSTIFYIN 0x200000
#endif
#ifndef GCP_NUMERICSLATIN
#define GCP_NUMERICSLATIN 0x4000000
#endif
#ifndef GCP_NUMERICSLOCAL
#define GCP_NUMERICSLOCAL 0x8000000
#endif

#define NEED_P() do { NEED_TEXT(); REQUIRE(U(GetCharacterPlacementA)); REQUIRE(U(GetCharacterPlacementW)); } while (0)
#define MAXN 64
typedef struct {
    GCP_RESULTSW r; WCHAR out[MAXN]; UINT order[MAXN]; int dx[MAXN]; int caret[MAXN]; char cls[MAXN]; WCHAR glyphs[MAXN];
} GW;
typedef struct {
    GCP_RESULTSA r; char out[MAXN]; UINT order[MAXN]; int dx[MAXN]; int caret[MAXN]; char cls[MAXN]; WCHAR glyphs[MAXN];
} GA;
static void gw_init(GW *g, UINT n, int use_out, int use_order, int use_dx, int use_caret, int use_cls, int use_glyphs) {
    memset(g, 0xcc, sizeof *g);
    memset(&g->r, 0, sizeof g->r);
    g->r.lStructSize = sizeof g->r; g->r.nGlyphs = n; g->r.nMaxFit = -7;
    g->r.lpOutString = use_out ? g->out : NULL;
    g->r.lpOrder = use_order ? g->order : NULL;
    g->r.lpDx = use_dx ? g->dx : NULL;
    g->r.lpCaretPos = use_caret ? g->caret : NULL;
    g->r.lpClass = use_cls ? g->cls : NULL;
    g->r.lpGlyphs = use_glyphs ? g->glyphs : NULL;
}
static void ga_init(GA *g, UINT n, int use_out, int use_order, int use_dx, int use_caret, int use_cls, int use_glyphs) {
    memset(g, 0xcc, sizeof *g);
    memset(&g->r, 0, sizeof g->r);
    g->r.lStructSize = sizeof g->r; g->r.nGlyphs = n; g->r.nMaxFit = -7;
    g->r.lpOutString = use_out ? g->out : NULL;
    g->r.lpOrder = use_order ? g->order : NULL;
    g->r.lpDx = use_dx ? g->dx : NULL;
    g->r.lpCaretPos = use_caret ? g->caret : NULL;
    g->r.lpClass = use_cls ? g->cls : NULL;
    g->r.lpGlyphs = use_glyphs ? g->glyphs : NULL;
}
static int fixed_p(TCtx *c, TEXTMETRICA *tm) { return to_fixed_ctx(c, tm, SYSTEM_FIXED_FONT, 64, 64); }
static int wine_guard(void) { if (to_wine()) { t_skip("wine faults or diverges here"); return 1; } return 0; }

TEST(text_output_gcp_null_results_returns_extent_a) {
    NEED_P();
    static const char *face[] = { "Arial", "Courier New", "Times New Roman", "Tahoma" };
    for (int f = 0; f < 4; f++) {
        TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
        to_use_font(&c, to_mkfont(face[f], -17, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
        SIZE s; CHECK(U(GetTextExtentPoint32A)(c.dc, "Placement", 9, &s));
        DWORD r = U(GetCharacterPlacementA)(c.dc, "Placement", 9, 0, NULL, 0);
        CHECKF(LOWORD(r) == s.cx && HIWORD(r) == s.cy, "%s: %lx vs %ldx%ld", face[f], r, s.cx, s.cy);
        to_close(&c);
    }
}
TEST(text_output_gcp_null_results_returns_extent_w) {
    NEED_P();
    static const char *face[] = { "Arial", "Courier New", "Times New Roman", "Tahoma" };
    for (int f = 0; f < 4; f++) {
        TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
        to_use_font(&c, to_mkfont(face[f], -17, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
        SIZE s; CHECK(U(GetTextExtentPoint32W)(c.dc, L"Placement", 9, &s));
        DWORD r = U(GetCharacterPlacementW)(c.dc, L"Placement", 9, 0, NULL, 0);
        CHECKF(LOWORD(r) == s.cx && HIWORD(r) == s.cy, "%s: %lx vs %ldx%ld", face[f], r, s.cx, s.cy);
        to_close(&c);
    }
}
TEST(text_output_gcp_fixed_font_exact_dims) {
    NEED_P();
    TCtx c; TEXTMETRICA tm; if (!fixed_p(&c, &tm)) return;
    for (int n = 1; n <= 12; n++) {
        DWORD r = U(GetCharacterPlacementA)(c.dc, "abcdefghijkl", n, 0, NULL, 0);
        CHECKF(LOWORD(r) == n * tm.tmAveCharWidth && HIWORD(r) == tm.tmHeight, "n=%d -> %lx", n, r);
    }
    to_close(&c);
}
TEST(text_output_gcp_invalid_params_a) {
    NEED_P();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    GA g; ga_init(&g, 8, 0, 0, 1, 0, 0, 0);
    SetLastError(0);
    CHECK(U(GetCharacterPlacementA)(c.dc, NULL, 3, 0, NULL, 0) == 0);
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(U(GetCharacterPlacementA)(c.dc, "abc", 0, 0, NULL, 0) == 0);
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(U(GetCharacterPlacementA)(c.dc, "abc", -1, 0, &g.r, 0) == 0);
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(U(GetCharacterPlacementA)(c.dc, "abc", 3, -2, NULL, 0) == 0);
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(U(GetCharacterPlacementA)(c.dc, "abc", 3, (int)0x80000000, &g.r, 0) == 0);
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    to_close(&c);
}
TEST(text_output_gcp_invalid_params_w) {
    NEED_P();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    GW g; gw_init(&g, 8, 0, 0, 1, 0, 0, 0);
    if (wine_guard()) { to_close(&c); return; }
    SetLastError(0);
    CHECK(U(GetCharacterPlacementW)(c.dc, NULL, 3, 0, NULL, 0) == 0);
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(U(GetCharacterPlacementW)(c.dc, L"abc", 0, 0, NULL, 0) == 0);
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(U(GetCharacterPlacementW)(c.dc, L"abc", -1, 0, &g.r, 0) == 0);
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(U(GetCharacterPlacementW)(c.dc, L"abc", 3, -2, NULL, 0) == 0);
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    to_close(&c);
}
TEST(text_output_gcp_max_extent_minus1_allowed) {
    NEED_P();
    TCtx c; TEXTMETRICA tm; if (!fixed_p(&c, &tm)) return;
    DWORD r = U(GetCharacterPlacementA)(c.dc, "abc", 3, -1, NULL, 0);
    CHECKF(LOWORD(r) == 3 * tm.tmAveCharWidth, "r=%lx", r);
    to_close(&c);
}
TEST(text_output_gcp_bogus_dc) {
    NEED_P();
    SetLastError(0);
    CHECK(U(GetCharacterPlacementA)(NULL, "abc", 3, 0, NULL, 0) == 0);
    CHECK(U(GetCharacterPlacementW)(NULL, L"abc", 3, 0, NULL, 0) == 0);
    CHECK(U(GetCharacterPlacementA)((HDC)(ULONG_PTR)0x1234, "abc", 3, 0, NULL, 0) == 0);
    CHECK(U(GetCharacterPlacementW)((HDC)(ULONG_PTR)0x1234, L"abc", 3, 0, NULL, 0) == 0);
    GW g; gw_init(&g, 4, 0, 0, 1, 0, 0, 0);
    if (!wine_guard()) CHECK(U(GetCharacterPlacementW)(NULL, L"abc", 3, 0, &g.r, 0) == 0);
}
TEST(text_output_gcp_dx_fixed_font_w) {
    NEED_P();
    TCtx c; TEXTMETRICA tm; if (!fixed_p(&c, &tm)) return;
    GW g; gw_init(&g, 8, 0, 0, 1, 0, 0, 0);
    DWORD r = U(GetCharacterPlacementW)(c.dc, L"abcdef", 6, 0, &g.r, 0);
    CHECKF(r != 0, "failed");
    CHECKF(LOWORD(r) == 6 * tm.tmAveCharWidth && HIWORD(r) == tm.tmHeight, "dims %lx", r);
    for (int i = 0; i < 6; i++) CHECKF(g.dx[i] == tm.tmAveCharWidth, "dx[%d]=%d", i, g.dx[i]);
    CHECK(g.r.nGlyphs == 6);
    to_close(&c);
}
TEST(text_output_gcp_dx_fixed_font_a) {
    NEED_P();
    TCtx c; TEXTMETRICA tm; if (!fixed_p(&c, &tm)) return;
    GA g; ga_init(&g, 8, 0, 0, 1, 0, 0, 0);
    DWORD r = U(GetCharacterPlacementA)(c.dc, "abcdef", 6, 0, &g.r, 0);
    CHECKF(r != 0, "failed");
    CHECKF(LOWORD(r) == 6 * tm.tmAveCharWidth, "dims %lx", r);
    for (int i = 0; i < 6; i++) CHECKF(g.dx[i] == tm.tmAveCharWidth, "dx[%d]=%d", i, g.dx[i]);
    to_close(&c);
}
TEST(text_output_gcp_dx_sum_equals_width_proportional) {
    NEED_P();
    static const char *face[] = { "Arial", "Times New Roman", "Tahoma", "Segoe UI" };
    for (int f = 0; f < 4; f++) {
        TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
        to_use_font(&c, to_mkfont(face[f], -15, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
        GW g; gw_init(&g, 16, 0, 0, 1, 0, 0, 0);
        DWORD r = U(GetCharacterPlacementW)(c.dc, L"Sum of widths", 13, 0, &g.r, 0);
        int sum = 0; for (int i = 0; i < 13; i++) sum += g.dx[i];
        CHECKF(r != 0 && LOWORD(r) == sum, "%s: width %u sum %d", face[f], LOWORD(r), sum);
        to_close(&c);
    }
}
TEST(text_output_gcp_caret_pos_cumulative) {
    NEED_P();
    TCtx c; TEXTMETRICA tm; if (!fixed_p(&c, &tm)) return;
    GW g; gw_init(&g, 8, 0, 0, 0, 1, 0, 0);
    DWORD r = U(GetCharacterPlacementW)(c.dc, L"abcdef", 6, 0, &g.r, 0);
    CHECK(r != 0);
    for (int i = 0; i < 6; i++) CHECKF(g.caret[i] == i * tm.tmAveCharWidth, "caret[%d]=%d", i, g.caret[i]);
    to_close(&c);
}
TEST(text_output_gcp_out_string_copies_latin) {
    NEED_P();
    TCtx c; TEXTMETRICA tm; if (!fixed_p(&c, &tm)) return;
    GW g; gw_init(&g, 16, 1, 0, 0, 0, 0, 0);
    DWORD r = U(GetCharacterPlacementW)(c.dc, L"Hello", 5, 0, &g.r, 0);
    CHECK(r != 0);
    CHECK(!memcmp(g.out, L"Hello", 5 * sizeof(WCHAR)));
    GA a; ga_init(&a, 16, 1, 0, 0, 0, 0, 0);
    r = U(GetCharacterPlacementA)(c.dc, "Hello", 5, 0, &a.r, 0);
    CHECK(r != 0);
    CHECK(!memcmp(a.out, "Hello", 5));
    to_close(&c);
}
TEST(text_output_gcp_nglyphs_clamps_count) {
    NEED_P();
    TCtx c; TEXTMETRICA tm; if (!fixed_p(&c, &tm)) return;
    GW g; gw_init(&g, 3, 1, 0, 1, 0, 0, 0);
    DWORD r = U(GetCharacterPlacementW)(c.dc, L"abcdefgh", 8, 0, &g.r, 0);
    CHECK(r != 0);
    t_note("nGlyphs clamps count: width %u (3 cells = %ld)", LOWORD(r), 3 * tm.tmAveCharWidth);
    CHECKF(g.r.nGlyphs <= 3, "nGlyphs %u", g.r.nGlyphs);
    CHECKF(g.dx[3] == (int)0xcccccccc, "dx beyond nGlyphs written");
    to_close(&c);
}
TEST(text_output_gcp_nglyphs_zero) {
    NEED_P();
    TCtx c; TEXTMETRICA tm; if (!fixed_p(&c, &tm)) return;
    if (wine_guard()) { to_close(&c); return; }
    GW g; gw_init(&g, 0, 1, 0, 1, 0, 0, 0);
    SetLastError(0);
    DWORD r = U(GetCharacterPlacementW)(c.dc, L"abc", 3, 0, &g.r, 0);
    t_note("nGlyphs=0 -> %lx err=%lu", r, GetLastError());
    to_close(&c);
}
TEST(text_output_gcp_reorder_latin_identity) {
    NEED_P();
    TCtx c; TEXTMETRICA tm; if (!fixed_p(&c, &tm)) return;
    GW g; gw_init(&g, 16, 0, 1, 1, 0, 0, 0);
    DWORD r = U(GetCharacterPlacementW)(c.dc, L"abcdef", 6, 0, &g.r, GCP_REORDER);
    CHECK(r != 0);
    for (int i = 0; i < 6; i++) CHECKF(g.order[i] == (UINT)i, "order[%d]=%u", i, g.order[i]);
    to_close(&c);
}
static int is_perm(const UINT *o, int n) {
    unsigned char seen[MAXN] = { 0 };
    for (int i = 0; i < n; i++) { if (o[i] >= (UINT)n || seen[o[i]]) return 0; seen[o[i]] = 1; }
    return 1;
}
TEST(text_output_gcp_reorder_hebrew_is_permutation) {
    NEED_P();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Arial", -18, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    if (wine_guard()) { to_close(&c); return; }
    static const WCHAR he[] = { 0x05d0, 0x05d1, 0x05d2, 0x05d3 };
    GW g; gw_init(&g, 8, 1, 1, 1, 0, 0, 0);
    DWORD r = U(GetCharacterPlacementW)(c.dc, he, 4, 0, &g.r, GCP_REORDER);
    CHECK(r != 0);
    CHECKF(is_perm(g.order, 4), "order %u %u %u %u", g.order[0], g.order[1], g.order[2], g.order[3]);
    t_note("Hebrew order: %u %u %u %u", g.order[0], g.order[1], g.order[2], g.order[3]);
    to_close(&c);
}
TEST(text_output_gcp_reorder_mixed_is_permutation) {
    NEED_P();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Arial", -18, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    if (wine_guard()) { to_close(&c); return; }
    static const WCHAR mix[] = { 'a', 'b', 0x05d0, 0x05d1, 0x05d2, 'c', 'd' };
    GW g; gw_init(&g, 8, 1, 1, 1, 0, 0, 0);
    DWORD r = U(GetCharacterPlacementW)(c.dc, mix, 7, 0, &g.r, GCP_REORDER);
    CHECK(r != 0);
    CHECK(is_perm(g.order, 7));
    t_note("mixed order: %u %u %u %u %u %u %u", g.order[0], g.order[1], g.order[2], g.order[3], g.order[4], g.order[5], g.order[6]);
    to_close(&c);
}
TEST(text_output_gcp_reorder_arabic_is_permutation) {
    NEED_P();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Arial", -18, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    if (wine_guard()) { to_close(&c); return; }
    static const WCHAR ar[] = { 0x0627, 0x0644, 0x0639, 0x0631, 0x0628, 0x064a, 0x0629 };
    GW g; gw_init(&g, 8, 1, 1, 1, 0, 1, 1);
    DWORD r = U(GetCharacterPlacementW)(c.dc, ar, 7, 0, &g.r, GCP_REORDER | GCP_GLYPHSHAPE | GCP_LIGATE);
    CHECK(r != 0);
    CHECK(is_perm(g.order, 7));
    t_note("Arabic shaped: nGlyphs=%u first glyph %04x", g.r.nGlyphs, g.glyphs[0]);
    to_close(&c);
}
TEST(text_output_gcp_classin_latin_class) {
    NEED_P();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Arial", -18, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    if (wine_guard()) { to_close(&c); return; }
    GW g; gw_init(&g, 8, 0, 1, 1, 0, 1, 0);
    memset(g.cls, 0, sizeof g.cls);
    DWORD r = U(GetCharacterPlacementW)(c.dc, L"abc12", 5, 0, &g.r, GCP_REORDER);
    CHECK(r != 0);
    t_note("classes after REORDER: %d %d %d %d %d", g.cls[0], g.cls[1], g.cls[2], g.cls[3], g.cls[4]);
    for (int i = 0; i < 3; i++) CHECKF(g.cls[i] == GCPCLASS_LATIN, "cls[%d]=%d", i, g.cls[i]);
    to_close(&c);
}
TEST(text_output_gcp_max_extent_limits_fit) {
    NEED_P();
    TCtx c; TEXTMETRICA tm; if (!fixed_p(&c, &tm)) return;
    int aw = tm.tmAveCharWidth;
    static const int mul[] = { 1, 2, 3, 5 };
    for (int i = 0; i < 4; i++) {
        GW g; gw_init(&g, 16, 0, 0, 1, 0, 0, 0);
        DWORD r = U(GetCharacterPlacementW)(c.dc, L"abcdefgh", 8, mul[i] * aw + aw / 2, &g.r, GCP_MAXEXTENT);
        CHECK(r != 0);
        CHECKF(g.r.nMaxFit == mul[i], "max %d cells: nMaxFit=%d", mul[i], g.r.nMaxFit);
        t_note("GCP_MAXEXTENT %d cells: returned width %u", mul[i], LOWORD(r));
    }
    to_close(&c);
}
TEST(text_output_gcp_max_extent_ignored_without_flag) {
    NEED_P();
    TCtx c; TEXTMETRICA tm; if (!fixed_p(&c, &tm)) return;
    GW g; gw_init(&g, 16, 0, 0, 1, 0, 0, 0);
    DWORD r = U(GetCharacterPlacementW)(c.dc, L"abcdefgh", 8, tm.tmAveCharWidth * 2, &g.r, 0);
    CHECK(r != 0);
    CHECKF(LOWORD(r) == 8 * tm.tmAveCharWidth, "width %u", LOWORD(r));
    t_note("nMaxFit without GCP_MAXEXTENT = %d", g.r.nMaxFit);
    to_close(&c);
}
TEST(text_output_gcp_justify_stretches_to_max_extent) {
    NEED_P();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Arial", -16, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    if (wine_guard()) { to_close(&c); return; }
    GW g; gw_init(&g, 16, 0, 0, 1, 0, 0, 0);
    DWORD r = U(GetCharacterPlacementW)(c.dc, L"a b c d", 7, 300, &g.r, GCP_MAXEXTENT | GCP_JUSTIFY);
    t_note("GCP_JUSTIFY to 300: ret=%lx width=%u", r, LOWORD(r));
    int sum = 0; for (int i = 0; i < 7; i++) sum += g.dx[i];
    t_note("sum dx = %d nMaxFit=%d", sum, g.r.nMaxFit);
    to_close(&c);
}
TEST(text_output_gcp_usekerning_flag_accepted) {
    NEED_P();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Times New Roman", -24, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    if (wine_guard()) { to_close(&c); return; }
    GW g; gw_init(&g, 16, 0, 0, 1, 0, 0, 0);
    DWORD r0 = U(GetCharacterPlacementW)(c.dc, L"AVATAR To", 9, 0, &g.r, 0);
    gw_init(&g, 16, 0, 0, 1, 0, 0, 0);
    DWORD r1 = U(GetCharacterPlacementW)(c.dc, L"AVATAR To", 9, 0, &g.r, GCP_USEKERNING);
    CHECK(r0 != 0 && r1 != 0);
    CHECKF(LOWORD(r1) <= LOWORD(r0), "kerned %u wider than plain %u", LOWORD(r1), LOWORD(r0));
    t_note("kerning: plain %u kerned %u", LOWORD(r0), LOWORD(r1));
    to_close(&c);
}
TEST(text_output_gcp_diacritic_flag_accepted) {
    NEED_P();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Arial", -18, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    if (wine_guard()) { to_close(&c); return; }
    static const WCHAR s[] = { 'e', 0x0301, 'a', 0x0300 };
    GW g; gw_init(&g, 8, 0, 0, 1, 0, 0, 0);
    DWORD r = U(GetCharacterPlacementW)(c.dc, s, 4, 0, &g.r, GCP_DIACRITIC);
    t_note("GCP_DIACRITIC combining marks: ret=%lx nGlyphs=%u dx=%d,%d,%d,%d", r, g.r.nGlyphs, g.dx[0], g.dx[1], g.dx[2], g.dx[3]);
    CHECK(r != 0);
    to_close(&c);
}
TEST(text_output_gcp_glyphshape_returns_glyph_indices) {
    NEED_P(); REQUIRE(U(GetGlyphIndicesW));
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Arial", -18, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    if (wine_guard()) { to_close(&c); return; }
    GW g; gw_init(&g, 16, 0, 0, 1, 0, 0, 1);
    DWORD r = U(GetCharacterPlacementW)(c.dc, L"Glyph", 5, 0, &g.r, GCP_GLYPHSHAPE);
    CHECK(r != 0);
    WORD gi[8]; CHECK(U(GetGlyphIndicesW)(c.dc, L"Glyph", 5, gi, 0) == 5);
    int same = 1; for (int i = 0; i < 5; i++) same &= g.glyphs[i] == gi[i];
    CHECKF(same, "glyphs %04x %04x vs %04x %04x", g.glyphs[0], g.glyphs[1], gi[0], gi[1]);
    to_close(&c);
}
TEST(text_output_gcp_numerics_flags_accepted) {
    NEED_P();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Arial", -18, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    if (wine_guard()) { to_close(&c); return; }
    GW g; gw_init(&g, 8, 1, 0, 1, 0, 0, 0);
    DWORD r = U(GetCharacterPlacementW)(c.dc, L"2024", 4, 0, &g.r, GCP_NUMERICSLATIN);
    CHECK(r != 0);
    CHECK(!memcmp(g.out, L"2024", 8));
    gw_init(&g, 8, 1, 0, 1, 0, 0, 0);
    r = U(GetCharacterPlacementW)(c.dc, L"2024", 4, 0, &g.r, GCP_NUMERICSLOCAL);
    t_note("GCP_NUMERICSLOCAL ret=%lx out=%04x%04x", r, g.out[0], g.out[1]);
    to_close(&c);
}
TEST(text_output_gcp_a_matches_w_dims) {
    NEED_P();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Arial", -18, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    if (wine_guard()) { to_close(&c); return; }
    GA a; GW w; ga_init(&a, 16, 1, 0, 1, 1, 0, 0); gw_init(&w, 16, 1, 0, 1, 1, 0, 0);
    DWORD ra = U(GetCharacterPlacementA)(c.dc, "A and W agree", 13, 0, &a.r, 0);
    DWORD rw = U(GetCharacterPlacementW)(c.dc, L"A and W agree", 13, 0, &w.r, 0);
    CHECK(ra == rw && ra != 0);
    CHECK(!memcmp(a.dx, w.dx, 13 * sizeof(int)));
    CHECK(!memcmp(a.caret, w.caret, 13 * sizeof(int)));
    to_close(&c);
}
TEST(text_output_gcp_does_not_move_cp_or_draw) {
    NEED_P(); REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    TCtx c; TEXTMETRICA tm; if (!fixed_p(&c, &tm)) return;
    U(MoveToEx)(c.dc, 5, 6, NULL);
    GW g; gw_init(&g, 8, 1, 0, 1, 0, 0, 0);
    CHECK(U(GetCharacterPlacementW)(c.dc, L"abc", 3, 0, &g.r, 0) != 0);
    POINT p; U(GetCurrentPositionEx)(c.dc, &p);
    CHECK(p.x == 5 && p.y == 6);
    RECT ink; CHECK(to_ink(&c, 0xffffff, &ink) == 0);
    to_close(&c);
}
TEST(text_output_gcp_repeatable) {
    NEED_P();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Tahoma", -14, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    DWORD first = 0;
    for (int i = 0; i < 100; i++) {
        DWORD r = U(GetCharacterPlacementW)(c.dc, L"Repeat", 6, 0, NULL, 0);
        if (i == 0) first = r; else if (r != first) { CHECKF(0, "iteration %d: %lx vs %lx", i, r, first); break; }
    }
    to_close(&c);
}
/* ---------- ETO flags ---------- */
static unsigned render_eto(TCtx *c, UINT flags, const WCHAR *s, int n, const int *dx) {
    to_clear(c);
    U(SetTextColor)(c->dc, 0);
    BOOL b = U(ExtTextOutW)(c->dc, 8, 10, flags, NULL, s, n, (INT *)dx);
    return b;
}
static int eto_same(UINT f1, UINT f2, const WCHAR *s, int n, const char *face, int h, int *ret1, int *ret2) {
    TCtx c; if (!to_open(&c, 160, 60, 32)) { CHECK(0); return -1; }
    to_use_font(&c, to_mkfont(face, h, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    *ret1 = render_eto(&c, f1, s, n, NULL);
    static unsigned a[160 * 60];
    for (int y = 0; y < 60; y++) for (int x = 0; x < 160; x++) a[y * 160 + x] = to_px(&c, x, y);
    *ret2 = render_eto(&c, f2, s, n, NULL);
    int d = 0; for (int y = 0; y < 60; y++) for (int x = 0; x < 160; x++) d += a[y * 160 + x] != to_px(&c, x, y);
    to_close(&c);
    return d;
}
TEST(text_output_eto_numericslatin_same_for_latin) {
    NEED_P();
    int r1, r2; int d = eto_same(0, ETO_NUMERICSLATIN, L"Latin 0123456789", 16, "Arial", -16, &r1, &r2);
    CHECK(r1 && r2);
    CHECKF(d == 0, "%d px differ", d);
}
TEST(text_output_eto_numericslocal_accepted) {
    NEED_P();
    int r1, r2; int d = eto_same(0, ETO_NUMERICSLOCAL, L"Latin 0123456789", 16, "Arial", -16, &r1, &r2);
    CHECK(r1 && r2);
    t_note("ETO_NUMERICSLOCAL vs none: %d px differ", d);
}
TEST(text_output_eto_ignorelanguage_same_for_latin) {
    NEED_P();
    int r1, r2; int d = eto_same(0, ETO_IGNORELANGUAGE, L"Ignore language", 15, "Arial", -16, &r1, &r2);
    CHECK(r1 && r2);
    t_note("ETO_IGNORELANGUAGE vs none: %d px differ", d);
    CHECK(d == 0);
}
TEST(text_output_eto_rtlreading_latin_accepted) {
    NEED_P();
    int r1, r2; int d = eto_same(0, ETO_RTLREADING, L"Latin text", 10, "Arial", -16, &r1, &r2);
    CHECK(r1 && r2);
    t_note("ETO_RTLREADING on Latin: %d px differ", d);
}
TEST(text_output_eto_rtlreading_hebrew_draws) {
    NEED_P();
    TCtx c; if (!to_open(&c, 160, 60, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Arial", -20, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    static const WCHAR he[] = { 0x05e9, 0x05dc, 0x05d5, 0x05dd };
    U(SetTextColor)(c.dc, 0);
    CHECK(U(ExtTextOutW)(c.dc, 8, 10, ETO_RTLREADING, NULL, he, 4, NULL));
    RECT ink; CHECK(to_ink(&c, 0xffffff, &ink) > 0);
    to_close(&c);
}
TEST(text_output_eto_glyph_index_matches_text) {
    NEED_P(); REQUIRE(U(GetGlyphIndicesW));
    TCtx c; if (!to_open(&c, 160, 60, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Arial", -18, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    U(SetTextColor)(c.dc, 0);
    WORD gi[16];
    CHECK(U(GetGlyphIndicesW)(c.dc, L"Glyph idx", 9, gi, GGI_MARK_NONEXISTING_GLYPHS) == 9);
    CHECK(U(ExtTextOutW)(c.dc, 8, 10, 0, NULL, L"Glyph idx", 9, NULL));
    static unsigned a[160 * 60];
    for (int y = 0; y < 60; y++) for (int x = 0; x < 160; x++) a[y * 160 + x] = to_px(&c, x, y);
    to_clear(&c);
    CHECK(U(ExtTextOutW)(c.dc, 8, 10, ETO_GLYPH_INDEX, NULL, (LPCWSTR)gi, 9, NULL));
    int d = 0; for (int y = 0; y < 60; y++) for (int x = 0; x < 160; x++) d += a[y * 160 + x] != to_px(&c, x, y);
    CHECKF(d == 0, "%d px differ", d);
    to_close(&c);
}
TEST(text_output_eto_glyph_index_a_variant) {
    NEED_P(); REQUIRE(U(GetGlyphIndicesW));
    TCtx c; if (!to_open(&c, 160, 60, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Arial", -18, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    U(SetTextColor)(c.dc, 0);
    WORD gi[16];
    CHECK(U(GetGlyphIndicesW)(c.dc, L"Gl", 2, gi, 0) == 2);
    SetLastError(0);
    BOOL b = U(ExtTextOutA)(c.dc, 8, 10, ETO_GLYPH_INDEX, NULL, (LPCSTR)gi, 2, NULL);
    t_note("ExtTextOutA ETO_GLYPH_INDEX -> %d err=%lu", b, GetLastError());
    RECT ink; t_note("ink=%d", to_ink(&c, 0xffffff, &ink));
    to_close(&c);
}
TEST(text_output_eto_glyph_index_width_uses_glyph_advance) {
    NEED_P(); REQUIRE(U(GetGlyphIndicesW)); REQUIRE(U(GetTextExtentPointI)); REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    TCtx c; if (!to_open(&c, 160, 60, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Arial", -18, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    WORD gi[8]; CHECK(U(GetGlyphIndicesW)(c.dc, L"idx", 3, gi, 0) == 3);
    SIZE s; CHECK(U(GetTextExtentPointI)(c.dc, gi, 3, &s));
    U(SetTextAlign)(c.dc, TA_UPDATECP);
    U(MoveToEx)(c.dc, 10, 10, NULL);
    CHECK(U(ExtTextOutW)(c.dc, 0, 0, ETO_GLYPH_INDEX, NULL, (LPCWSTR)gi, 3, NULL));
    POINT p; U(GetCurrentPositionEx)(c.dc, &p);
    CHECKF(p.x == 10 + s.cx, "cp %ld want %ld", p.x, 10 + s.cx);
    to_close(&c);
}
TEST(text_output_eto_reverse_index_map_accepted) {
    NEED_P(); REQUIRE(U(GetGlyphIndicesW));
    TCtx c; if (!to_open(&c, 160, 60, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Arial", -18, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    WORD gi[4]; U(GetGlyphIndicesW)(c.dc, L"ab", 2, gi, 0);
    SetLastError(0);
    BOOL b = U(ExtTextOutW)(c.dc, 8, 10, ETO_GLYPH_INDEX | 0x10000 /* ETO_REVERSE_INDEX_MAP */, NULL, (LPCWSTR)gi, 2, NULL);
    t_note("ETO_REVERSE_INDEX_MAP -> %d err=%lu", b, GetLastError());
    to_close(&c);
}
TEST(text_output_eto_pdy_vertical_offsets) {
    NEED_P();
    TCtx c; TEXTMETRICA tm; if (!fixed_p(&c, &tm)) return;
    U(SetTextColor)(c.dc, 0);
    int aw = tm.tmAveCharWidth;
    int dxy[6] = { aw, 0, aw, 6, aw, -3 };
    CHECK(U(ExtTextOutW)(c.dc, 4, 20, ETO_PDY, NULL, L"abc", 3, dxy));
    static unsigned a[64 * 64];
    for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) a[y * 64 + x] = to_px(&c, x, y);
    to_clear(&c);
    U(TextOutW)(c.dc, 4, 20, L"a", 1);
    U(TextOutW)(c.dc, 4 + aw, 20, L"b", 1);
    U(TextOutW)(c.dc, 4 + 2 * aw, 26, L"c", 1);
    int d = 0; for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) d += a[y * 64 + x] != to_px(&c, x, y);
    t_note("PDY placement: %d px differ from manual composition", d);
    to_close(&c);
}
TEST(text_output_eto_dx_array_exact_count) {
    /* lpDx is read for exactly 'count' entries: a short, guarded array is enough */
    NEED_P();
    TCtx c; TEXTMETRICA tm; if (!fixed_p(&c, &tm)) return;
    int *p = (int *)VirtualAlloc(NULL, 4096 * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    DWORD old; VirtualProtect(p + 1024, 4096, PAGE_NOACCESS, &old);
    int *dx = p + 1024 - 4;
    for (int i = 0; i < 4; i++) dx[i] = tm.tmAveCharWidth;
    CHECK(U(ExtTextOutW)(c.dc, 0, 0, 0, NULL, L"abcd", 4, dx));
    CHECK(U(ExtTextOutA)(c.dc, 0, 0, 0, NULL, "abcd", 4, dx));
    VirtualFree(p, 0, MEM_RELEASE);
    to_close(&c);
}
