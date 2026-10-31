/* ScriptXtoCP / ScriptStringXtoCP: x-coordinate to character position.
 *
 * ScriptXtoCP(iX, cChars, cGlyphs, pwLogClust, psva, piAdvance, psa, *piCP, *piTrailing):
 *   - any of pwLogClust, psva, piAdvance, psa, piCP, piTrailing NULL -> E_INVALIDARG, nothing written
 *   - iX < 0 (left of the run, LTR)  -> *piCP = -1, *piTrailing = 1
 *   - iX >= total advance (LTR)       -> *piCP = cChars, *piTrailing = 0
 *   - inside a cluster of k chars and width w, at offset o: u = (o*k*2)/w ; *piCP = first + u/2 ; *piTrailing = u & 1
 * ScriptStringXtoCP(ssa, iX, *piCh, *piTrailing): same over a whole analysed string; ssa == NULL -> E_INVALIDARG.
 */
#include "g7.h"

#define E_INVALIDARG_DW G7_E_INVALIDARG

/* an Itemize'd Latin SCRIPT_ANALYSIS (eScript = SCRIPT_LATIN, LTR) without needing a DC */
static int g7_latin_psa(SCRIPT_ANALYSIS *psa) {
    HRESULT (WINAPI *it)(const WCHAR *, int, int, const SCRIPT_CONTROL *, const SCRIPT_STATE *, SCRIPT_ITEM *, int *) = (void *)g7_usp_proc("ScriptItemize");
    SCRIPT_ITEM items[8]; int n = 0;
    static const WCHAR s[] = L"abcdefgh";
    if (!it || FAILED(it(s, 8, 6, NULL, NULL, items, &n)) || n < 1) return 0;
    *psa = items[0].a;
    return 1;
}

typedef struct { int cchars, cglyphs; WORD clust[32]; SCRIPT_VISATTR va[32]; int adv[32]; } SYN;

static void syn_1to1(SYN *s, int n, const int *adv) {
    memset(s, 0, sizeof *s);
    s->cchars = s->cglyphs = n;
    for (int i = 0; i < n; i++) { s->clust[i] = (WORD)i; s->va[i].fClusterStart = 1; s->va[i].uJustification = 0; s->adv[i] = adv[i]; }
}
static int total(const SYN *s) { int t = 0; for (int i = 0; i < s->cglyphs; i++) t += s->adv[i]; return t; }

/* oracle for 1:1 LTR data */
static void oracle_1to1(const SYN *s, int x, int *cp, int *tr) {
    if (x < 0) { *cp = -1; *tr = 1; return; }
    int cum = 0;
    for (int i = 0; i < s->cglyphs; i++) {
        if (x < cum + s->adv[i]) { int off = x - cum; int u = off * 2 / s->adv[i]; *cp = i + u / 2; *tr = u & 1; return; }
        cum += s->adv[i];
    }
    *cp = s->cchars; *tr = 0;
}

static HRESULT call_xtocp(const SYN *s, int x, const SCRIPT_ANALYSIS *psa, int *cp, int *tr) {
    HRESULT (WINAPI *f)(int, int, int, const WORD *, const SCRIPT_VISATTR *, const int *, const SCRIPT_ANALYSIS *, int *, int *) = (void *)g7_usp_proc("ScriptXtoCP");
    return f(x, s->cchars, s->cglyphs, s->clust, s->va, s->adv, psa, cp, tr);
}

#define XREQ() \
    REQUIRE(g7_usp_proc("ScriptXtoCP")); \
    SCRIPT_ANALYSIS psa; if (!g7_latin_psa(&psa)) { t_skip("cannot itemize"); return; }

/* ---------------- invalid parameters ---------------- */
#define XNULLTEST(nm, ARGS) \
TEST(gap7_xtocp_null_##nm) { \
    XREQ(); \
    if (g7_is_wine()) { t_skip("wine: no NULL checks"); return; } \
    SYN s; static const int adv[4] = { 10, 10, 10, 10 }; syn_1to1(&s, 4, adv); \
    int cp = 0x1111, tr = 0x2222; \
    HRESULT (WINAPI *f)(int, int, int, const WORD *, const SCRIPT_VISATTR *, const int *, const SCRIPT_ANALYSIS *, int *, int *) = (void *)g7_usp_proc("ScriptXtoCP"); \
    HRESULT hr = f ARGS; \
    EXPECT_EQ((DWORD)hr, E_INVALIDARG_DW); \
    EXPECT_EQ(cp, 0x1111); EXPECT_EQ(tr, 0x2222); \
}
XNULLTEST(logclust, (5, 4, 4, NULL, s.va, s.adv, &psa, &cp, &tr))
XNULLTEST(visattr,  (5, 4, 4, s.clust, NULL, s.adv, &psa, &cp, &tr))
XNULLTEST(advance,  (5, 4, 4, s.clust, s.va, NULL, &psa, &cp, &tr))
XNULLTEST(analysis, (5, 4, 4, s.clust, s.va, s.adv, NULL, &cp, &tr))
XNULLTEST(outcp,    (5, 4, 4, s.clust, s.va, s.adv, &psa, NULL, &tr))
XNULLTEST(outtrail, (5, 4, 4, s.clust, s.va, s.adv, &psa, &cp, NULL))
TEST(gap7_xtocp_null_all) {
    XREQ();
    if (g7_is_wine()) { t_skip("wine: differs/crashes here"); return; }
    HRESULT (WINAPI *f)(int, int, int, const WORD *, const SCRIPT_VISATTR *, const int *, const SCRIPT_ANALYSIS *, int *, int *) = (void *)g7_usp_proc("ScriptXtoCP");
    EXPECT_EQ((DWORD)f(0, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL), E_INVALIDARG_DW);
    EXPECT_EQ((DWORD)f(-1, -1, -1, NULL, NULL, NULL, NULL, NULL, NULL), E_INVALIDARG_DW);
    EXPECT_EQ((DWORD)f(INT_MAX, INT_MAX, INT_MAX, NULL, NULL, NULL, NULL, NULL, NULL), E_INVALIDARG_DW);
}
TEST(gap7_xtocp_null_checked_before_values) {
    XREQ();
    if (g7_is_wine()) { t_skip("wine: differs/crashes here"); return; }
    /* with a NULL pointer the numeric arguments are irrelevant */
    SYN s; static const int adv[4] = { 10, 10, 10, 10 }; syn_1to1(&s, 4, adv);
    HRESULT (WINAPI *f)(int, int, int, const WORD *, const SCRIPT_VISATTR *, const int *, const SCRIPT_ANALYSIS *, int *, int *) = (void *)g7_usp_proc("ScriptXtoCP");
    int cp, tr;
    static const int xs[] = { INT_MIN, -1, 0, 1, 39, 40, INT_MAX };
    for (int i = 0; i < 7; i++) {
        EXPECT_EQ((DWORD)f(xs[i], -5, 99, NULL, s.va, s.adv, &psa, &cp, &tr), E_INVALIDARG_DW);
        EXPECT_EQ((DWORD)f(xs[i], 4, 4, s.clust, s.va, s.adv, &psa, NULL, &tr), E_INVALIDARG_DW);
    }
}

/* ---------------- LTR 1:1 sweeps ---------------- */
static void sweep(const int *adv, int n, const SCRIPT_ANALYSIS *psa) {
    SYN s; syn_1to1(&s, n, adv);
    int t = total(&s), bad = 0, firstbad = 0;
    for (int x = -4; x <= t + 4; x++) {
        int cp = 0x7777, tr = 0x7777, ecp, etr;
        HRESULT hr = call_xtocp(&s, x, psa, &cp, &tr);
        oracle_1to1(&s, x, &ecp, &etr);
        if (hr != 0 || cp != ecp || tr != etr) { if (!bad) firstbad = x; bad++; }
    }
    CHECKF(bad == 0, "%d mismatches, first at x=%d", bad, firstbad);
}
#define SWEEP(nm, ...) \
TEST(gap7_xtocp_ltr_##nm) { XREQ(); static const int adv[] = { __VA_ARGS__ }; sweep(adv, (int)(sizeof adv / sizeof *adv), &psa); }
SWEEP(uniform10, 10, 10, 10, 10)
SWEEP(uniform7, 7, 7, 7, 7, 7)
SWEEP(uniform1, 1, 1, 1, 1)
SWEEP(uniform2, 2, 2, 2)
SWEEP(single, 13)
SWEEP(single_wide, 200)
SWEEP(mixed, 5, 10, 15, 20)
SWEEP(mixed_odd, 3, 9, 4, 11, 6)
SWEEP(descending, 20, 15, 10, 5, 1)
SWEEP(ascending, 1, 2, 3, 4, 5, 6, 7, 8)
SWEEP(wide_narrow, 100, 1, 100, 1)
SWEEP(many, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8)
SWEEP(prime_widths, 17, 19, 23, 29, 31)

TEST(gap7_xtocp_ltr_extreme_x) {
    XREQ();
    SYN s; static const int adv[4] = { 10, 10, 10, 10 }; syn_1to1(&s, 4, adv);
    int cp = 0, tr = 0;
    EXPECT_EQ((DWORD)call_xtocp(&s, INT_MIN, &psa, &cp, &tr), 0); EXPECT_EQ(cp, -1); EXPECT_EQ(tr, 1);
    EXPECT_EQ((DWORD)call_xtocp(&s, INT_MAX, &psa, &cp, &tr), 0); EXPECT_EQ(cp, 4); EXPECT_EQ(tr, 0);
    EXPECT_EQ((DWORD)call_xtocp(&s, -1, &psa, &cp, &tr), 0); EXPECT_EQ(cp, -1); EXPECT_EQ(tr, 1);
    EXPECT_EQ((DWORD)call_xtocp(&s, 40, &psa, &cp, &tr), 0); EXPECT_EQ(cp, 4); EXPECT_EQ(tr, 0);
    EXPECT_EQ((DWORD)call_xtocp(&s, 39, &psa, &cp, &tr), 0); EXPECT_EQ(cp, 3); EXPECT_EQ(tr, 1);
    EXPECT_EQ((DWORD)call_xtocp(&s, 0, &psa, &cp, &tr), 0); EXPECT_EQ(cp, 0); EXPECT_EQ(tr, 0);
}
TEST(gap7_xtocp_ltr_half_boundary) {
    XREQ();
    /* offset exactly half the width counts as the trailing half */
    SYN s; static const int adv[3] = { 10, 10, 10 }; syn_1to1(&s, 3, adv);
    int cp, tr;
    call_xtocp(&s, 4, &psa, &cp, &tr); EXPECT_EQ(cp, 0); EXPECT_EQ(tr, 0);
    call_xtocp(&s, 5, &psa, &cp, &tr); EXPECT_EQ(cp, 0); EXPECT_EQ(tr, 1);
    call_xtocp(&s, 14, &psa, &cp, &tr); EXPECT_EQ(cp, 1); EXPECT_EQ(tr, 0);
    call_xtocp(&s, 15, &psa, &cp, &tr); EXPECT_EQ(cp, 1); EXPECT_EQ(tr, 1);
    call_xtocp(&s, 29, &psa, &cp, &tr); EXPECT_EQ(cp, 2); EXPECT_EQ(tr, 1);
    call_xtocp(&s, 30, &psa, &cp, &tr); EXPECT_EQ(cp, 3); EXPECT_EQ(tr, 0);
}
TEST(gap7_xtocp_single_char) {
    XREQ();
    SYN s; static const int adv[1] = { 20 }; syn_1to1(&s, 1, adv);
    int cp, tr;
    call_xtocp(&s, -1, &psa, &cp, &tr); EXPECT_EQ(cp, -1); EXPECT_EQ(tr, 1);
    call_xtocp(&s, 0, &psa, &cp, &tr);  EXPECT_EQ(cp, 0); EXPECT_EQ(tr, 0);
    call_xtocp(&s, 9, &psa, &cp, &tr);  EXPECT_EQ(cp, 0); EXPECT_EQ(tr, 0);
    call_xtocp(&s, 10, &psa, &cp, &tr); EXPECT_EQ(cp, 0); EXPECT_EQ(tr, 1);
    call_xtocp(&s, 19, &psa, &cp, &tr); EXPECT_EQ(cp, 0); EXPECT_EQ(tr, 1);
    call_xtocp(&s, 20, &psa, &cp, &tr); EXPECT_EQ(cp, 1); EXPECT_EQ(tr, 0);
}
TEST(gap7_xtocp_return_is_s_ok) {
    XREQ();
    SYN s; static const int adv[4] = { 10, 10, 10, 10 }; syn_1to1(&s, 4, adv);
    int cp, tr;
    static const int xs[] = { -100, 0, 15, 40, 100 };
    for (int i = 0; i < 5; i++) { HRESULT hr = call_xtocp(&s, xs[i], &psa, &cp, &tr); EXPECT_EQ((DWORD)hr, 0); }
}
TEST(gap7_xtocp_deterministic_repeat) {
    XREQ();
    SYN s; static const int adv[4] = { 7, 11, 5, 13 }; syn_1to1(&s, 4, adv);
    int cp0, tr0, bad = 0;
    call_xtocp(&s, 19, &psa, &cp0, &tr0);
    for (int i = 0; i < 1000; i++) { int cp, tr; call_xtocp(&s, 19, &psa, &cp, &tr); bad += cp != cp0 || tr != tr0; }
    CHECK(bad == 0);
}
TEST(gap7_xtocp_inputs_not_modified) {
    XREQ();
    SYN s; static const int adv[4] = { 7, 11, 5, 13 }; syn_1to1(&s, 4, adv);
    SYN c = s; SCRIPT_ANALYSIS pc = psa; int cp, tr;
    call_xtocp(&s, 12, &psa, &cp, &tr);
    CHECK(memcmp(&c, &s, sizeof s) == 0);
    CHECK(memcmp(&pc, &psa, sizeof psa) == 0);
}

/* ---------------- clusters ---------------- */
/* generic oracle: cluster list (first char, nchars, width) in visual order for LTR */
typedef struct { int first, k, w; } CL;
static void oracle_cl(const CL *c, int nc, int nchars, int x, int *cp, int *tr) {
    if (x < 0) { *cp = -1; *tr = 1; return; }
    int cum = 0;
    for (int i = 0; i < nc; i++) {
        if (x < cum + c[i].w) { int off = x - cum; int u = off * c[i].k * 2 / c[i].w; *cp = c[i].first + u / 2; *tr = u & 1; return; }
        cum += c[i].w;
    }
    *cp = nchars; *tr = 0;
}
static void cluster_sweep(const SYN *s, const CL *c, int nc, const SCRIPT_ANALYSIS *psa, int hard) {
    int t = total(s), bad = 0, fb = 0;
    for (int x = -3; x <= t + 3; x++) {
        int cp = 0x7777, tr = 0x7777, ecp, etr;
        HRESULT hr = call_xtocp(s, x, psa, &cp, &tr);
        oracle_cl(c, nc, s->cchars, x, &ecp, &etr);
        if (hr != 0 || cp != ecp || tr != etr) { if (!bad) fb = x; bad++; }
    }
    if (hard) CHECKF(bad == 0, "%d mismatches, first at x=%d", bad, fb);
    else SOFT(bad == 0, "%d mismatches, first at x=%d", bad, fb);
}
TEST(gap7_xtocp_cluster_two_chars_one_glyph) {
    XREQ();
    /* chars 0,1 -> glyph 0 (ligature), char 2 -> glyph 1 */
    SYN s; memset(&s, 0, sizeof s);
    s.cchars = 3; s.cglyphs = 2;
    s.clust[0] = 0; s.clust[1] = 0; s.clust[2] = 1;
    s.va[0].fClusterStart = 1; s.va[1].fClusterStart = 1;
    s.adv[0] = 20; s.adv[1] = 10;
    CL c[] = { {0, 2, 20}, {2, 1, 10} };
    cluster_sweep(&s, c, 2, &psa, 1);
}
TEST(gap7_xtocp_cluster_three_chars_one_glyph) {
    XREQ();
    SYN s; memset(&s, 0, sizeof s);
    s.cchars = 3; s.cglyphs = 1;
    s.clust[0] = s.clust[1] = s.clust[2] = 0; s.va[0].fClusterStart = 1; s.adv[0] = 30;
    CL c[] = { {0, 3, 30} };
    cluster_sweep(&s, c, 1, &psa, 1);
}
TEST(gap7_xtocp_cluster_ligature_in_middle) {
    XREQ();
    SYN s; memset(&s, 0, sizeof s);
    s.cchars = 5; s.cglyphs = 3;
    s.clust[0] = 0; s.clust[1] = 1; s.clust[2] = 1; s.clust[3] = 1; s.clust[4] = 2;
    s.va[0].fClusterStart = s.va[1].fClusterStart = s.va[2].fClusterStart = 1;
    s.adv[0] = 8; s.adv[1] = 24; s.adv[2] = 8;
    CL c[] = { {0, 1, 8}, {1, 3, 24}, {4, 1, 8} };
    cluster_sweep(&s, c, 3, &psa, 1);
}
TEST(gap7_xtocp_cluster_one_char_two_glyphs) {
    XREQ();
    if (g7_is_wine()) { t_skip("wine: differs/crashes here"); return; }
    /* base glyph + combining glyph for a single char: the cluster spans both glyph advances */
    SYN s; memset(&s, 0, sizeof s);
    s.cchars = 2; s.cglyphs = 3;
    s.clust[0] = 0; s.clust[1] = 2;
    s.va[0].fClusterStart = 1; s.va[1].fClusterStart = 0; s.va[1].fDiacritic = 1; s.va[2].fClusterStart = 1;
    s.adv[0] = 7; s.adv[1] = 0; s.adv[2] = 9;
    CL c[] = { {0, 1, 7}, {1, 1, 9} };
    cluster_sweep(&s, c, 2, &psa, 0);
}
TEST(gap7_xtocp_cluster_multiglyph_cluster_width) {
    XREQ();
    if (g7_is_wine()) { t_skip("wine: differs/crashes here"); return; }
    SYN s; memset(&s, 0, sizeof s);
    s.cchars = 2; s.cglyphs = 3;
    s.clust[0] = 0; s.clust[1] = 2;
    s.va[0].fClusterStart = 1; s.va[1].fClusterStart = 0; s.va[2].fClusterStart = 1;
    s.adv[0] = 6; s.adv[1] = 8; s.adv[2] = 10;
    CL c[] = { {0, 1, 14}, {1, 1, 10} };
    cluster_sweep(&s, c, 2, &psa, 0);
}
TEST(gap7_xtocp_cluster_all_one) {
    XREQ();
    /* 6 chars in 2 clusters of 3 */
    SYN s; memset(&s, 0, sizeof s);
    s.cchars = 6; s.cglyphs = 2;
    for (int i = 0; i < 6; i++) s.clust[i] = (WORD)(i / 3);
    s.va[0].fClusterStart = s.va[1].fClusterStart = 1;
    s.adv[0] = 12; s.adv[1] = 18;
    CL c[] = { {0, 3, 12}, {3, 3, 18} };
    cluster_sweep(&s, c, 2, &psa, 1);
}

/* ---------------- RTL (visual-order glyph array) ---------------- */
TEST(gap7_xtocp_rtl_observed) {
    XREQ();
    if (g7_is_wine()) { t_skip("wine: differs/crashes here"); return; }
    SCRIPT_ANALYSIS r = psa;
    r.fRTL = 1; r.fLogicalOrder = 0; r.s.uBidiLevel = 1;
    /* glyph array is visual: leftmost glyph is the last logical char */
    SYN s; memset(&s, 0, sizeof s);
    s.cchars = s.cglyphs = 4;
    for (int i = 0; i < 4; i++) { s.clust[i] = (WORD)(3 - i); s.va[i].fClusterStart = 1; s.adv[i] = 10; }
    int cp, tr;
    HRESULT hr;
    hr = call_xtocp(&s, -1, &r, &cp, &tr);  EXPECT_EQ((DWORD)hr, 0); t_note("rtl x=-1 -> cp=%d tr=%d", cp, tr);
    hr = call_xtocp(&s, 0, &r, &cp, &tr);   EXPECT_EQ((DWORD)hr, 0); t_note("rtl x=0 -> cp=%d tr=%d", cp, tr);
    hr = call_xtocp(&s, 12, &r, &cp, &tr);  EXPECT_EQ((DWORD)hr, 0); t_note("rtl x=12 -> cp=%d tr=%d", cp, tr);
    hr = call_xtocp(&s, 39, &r, &cp, &tr);  EXPECT_EQ((DWORD)hr, 0); t_note("rtl x=39 -> cp=%d tr=%d", cp, tr);
    hr = call_xtocp(&s, 40, &r, &cp, &tr);  EXPECT_EQ((DWORD)hr, 0); t_note("rtl x=40 -> cp=%d tr=%d", cp, tr);
    /* whatever the layout, results stay in range and are consistent */
    int bad = 0;
    for (int x = -2; x <= 42; x++) { hr = call_xtocp(&s, x, &r, &cp, &tr); bad += hr != 0 || cp < -1 || cp > 4 || (tr != 0 && tr != 1); }
    CHECK(bad == 0);
}
TEST(gap7_xtocp_rtl_mirror_symmetry) {
    XREQ();
    if (g7_is_wine()) { t_skip("wine: differs/crashes here"); return; }
    SCRIPT_ANALYSIS r = psa; r.fRTL = 1; r.fLogicalOrder = 0; r.s.uBidiLevel = 1;
    SYN s; memset(&s, 0, sizeof s);
    s.cchars = s.cglyphs = 4;
    for (int i = 0; i < 4; i++) { s.clust[i] = (WORD)(3 - i); s.va[i].fClusterStart = 1; s.adv[i] = 10; }
    /* the character under x (RTL) is the mirror of the character under 39-x in the LTR run */
    int bad = 0;
    for (int x = 0; x < 40; x++) {
        int cp, tr, cpL, trL;
        call_xtocp(&s, x, &r, &cp, &tr);
        SYN l; static const int adv[4] = { 10, 10, 10, 10 }; syn_1to1(&l, 4, adv);
        call_xtocp(&l, 39 - x, &psa, &cpL, &trL);
        if (cp != cpL && !(cp == cpL + 1 && tr == 0)) bad++;
    }
    SOFT(bad == 0, "mirror symmetry violated %d times", bad);
}

/* ---------------- real shaped text ---------------- */
static void real_run(const WCHAR *face, int h, const WCHAR *txt, int hard_roundtrip) {
    REQUIRE(g7_usp_proc("ScriptXtoCP")); REQUIRE(g7_usp_proc("ScriptCPtoX"));
    HRESULT (WINAPI *c2x)(int, BOOL, int, int, const WORD *, const SCRIPT_VISATTR *, const int *, const SCRIPT_ANALYSIS *, int *) = (void *)g7_usp_proc("ScriptCPtoX");
    G7DC d; G7RUN r;
    if (!g7_dc_create(&d, 64, 64, face, h)) { t_skip("dc"); return; }
    if (!g7_run_build(d.dc, &r, txt)) { g7_dc_destroy(&d); t_skip("shape"); return; }
    int n = r.n, bad = 0, badf = 0;
    for (int i = 0; i <= n; i++) {
        int x = 0, cp = -7, tr = -7;
        {
            HRESULT hr = (i < n) ? c2x(i, FALSE, r.n, r.ng, r.clust, r.va, r.adv, &r.psa, &x)
                                 : c2x(n - 1, TRUE, r.n, r.ng, r.clust, r.va, r.adv, &r.psa, &x);
            if (FAILED(hr)) { bad++; continue; }
        }
        HRESULT hr;
        HRESULT (WINAPI *x2c)(int, int, int, const WORD *, const SCRIPT_VISATTR *, const int *, const SCRIPT_ANALYSIS *, int *, int *) = (void *)g7_usp_proc("ScriptXtoCP");
        hr = x2c(x, r.n, r.ng, r.clust, r.va, r.adv, &r.psa, &cp, &tr);
        if (FAILED(hr) || cp != i || tr != 0) badf++;
    }
    if (hard_roundtrip) CHECKF(bad == 0 && badf == 0, "cptox failures %d, roundtrip mismatches %d", bad, badf);
    else SOFT(bad == 0 && badf == 0, "cptox failures %d, roundtrip mismatches %d", bad, badf);
    g7_run_free(&r); g7_dc_destroy(&d);
}
TEST(gap7_xtocp_real_arial_hello) { real_run(L"Arial", 16, L"Hello", 1); }
TEST(gap7_xtocp_real_arial_sentence) { real_run(L"Arial", 20, L"The quick brown dog jumps over", 1); }
TEST(gap7_xtocp_real_courier) { real_run(L"Courier New", 14, L"abcdefghij", 1); }
TEST(gap7_xtocp_real_tahoma_digits) { real_run(L"Tahoma", 12, L"0123456789", 1); }
TEST(gap7_xtocp_real_times_caps) { real_run(L"Times New Roman", 18, L"WXYZ MNOP", 1); }
TEST(gap7_xtocp_real_segoe) { real_run(L"Segoe UI", 15, L"Segoe text", 1); }
TEST(gap7_xtocp_real_consolas) { real_run(L"Consolas", 13, L"if (x == 1) {}", 1); }
TEST(gap7_xtocp_real_single_char) { real_run(L"Arial", 16, L"W", 1); }
TEST(gap7_xtocp_real_punct) { real_run(L"Arial", 16, L"a,b;c.d:e!f?", 1); }
TEST(gap7_xtocp_real_long) {
    real_run(L"Arial", 12, L"Lorem ipsum dolor sit amet consectetur adipiscing elit sed do eiusmod", 1);
}
TEST(gap7_xtocp_real_monospace_uniform) {
    REQUIRE(g7_usp_proc("ScriptXtoCP"));
    G7DC d; G7RUN r;
    if (!g7_dc_create(&d, 64, 64, L"Courier New", 16)) { t_skip("dc"); return; }
    if (!g7_run_build(d.dc, &r, L"iiiiWWWWmmmm")) { g7_dc_destroy(&d); t_skip("shape"); return; }
    int w0 = r.adv[0], bad = 0;
    for (int i = 0; i < r.ng; i++) bad += r.adv[i] != w0;
    CHECK(bad == 0); /* fixed-pitch font: equal advances */
    HRESULT (WINAPI *x2c)(int, int, int, const WORD *, const SCRIPT_VISATTR *, const int *, const SCRIPT_ANALYSIS *, int *, int *) = (void *)g7_usp_proc("ScriptXtoCP");
    int badm = 0;
    for (int x = 0; x < w0 * r.n; x++) {
        int cp, tr; x2c(x, r.n, r.ng, r.clust, r.va, r.adv, &r.psa, &cp, &tr);
        int ecp = x / w0, etr = ((x % w0) * 2) / w0;
        badm += cp != ecp || tr != etr;
    }
    CHECKF(badm == 0, "%d mismatches", badm);
    g7_run_free(&r); g7_dc_destroy(&d);
}

/* ================= ScriptStringXtoCP ================= */
typedef HRESULT (WINAPI *ssa_analyse_t)(HDC, const void *, int, int, int, DWORD, int, SCRIPT_CONTROL *, SCRIPT_STATE *, const int *, SCRIPT_TABDEF *, const BYTE *, SCRIPT_STRING_ANALYSIS *);
typedef HRESULT (WINAPI *ssa_free_t)(SCRIPT_STRING_ANALYSIS *);
typedef HRESULT (WINAPI *ssa_c2x_t)(SCRIPT_STRING_ANALYSIS, int, BOOL, int *);
typedef HRESULT (WINAPI *ssa_x2c_t)(SCRIPT_STRING_ANALYSIS, int, int *, int *);

#define SSREQ() \
    ssa_analyse_t an = (ssa_analyse_t)g7_usp_proc("ScriptStringAnalyse"); REQUIRE(an); \
    ssa_free_t fr = (ssa_free_t)g7_usp_proc("ScriptStringFree"); REQUIRE(fr); \
    ssa_c2x_t c2x = (ssa_c2x_t)g7_usp_proc("ScriptStringCPtoX"); REQUIRE(c2x); \
    ssa_x2c_t x2c = (ssa_x2c_t)g7_usp_proc("ScriptStringXtoCP"); REQUIRE(x2c)

static int ss_build(HDC dc, const WCHAR *s, SCRIPT_STRING_ANALYSIS *ssa, DWORD extra) {
    ssa_analyse_t an = (ssa_analyse_t)g7_usp_proc("ScriptStringAnalyse");
    int n = lstrlenW(s);
    *ssa = NULL;
    HRESULT hr = an(dc, s, n, n * 3 / 2 + 16, -1, SSA_GLYPHS | SSA_FALLBACK | extra, 0, NULL, NULL, NULL, NULL, NULL, ssa);
    return SUCCEEDED(hr) && *ssa;
}

TEST(gap7_ssxtocp_null_ssa) {
    SSREQ();
    if (g7_is_wine()) { t_skip("wine: returns S_FALSE for NULL ssa"); return; }
    int ch = 0x1111, tr = 0x2222;
    EXPECT_EQ((DWORD)x2c(NULL, 0, &ch, &tr), E_INVALIDARG_DW);
    EXPECT_EQ(ch, 0x1111); EXPECT_EQ(tr, 0x2222);
    static const int xs[] = { INT_MIN, -1, 0, 1, 100, INT_MAX };
    for (int i = 0; i < 6; i++) EXPECT_EQ((DWORD)x2c(NULL, xs[i], &ch, &tr), E_INVALIDARG_DW);
}
TEST(gap7_ssxtocp_null_ssa_null_outputs) {
    SSREQ();
    if (g7_is_wine()) { t_skip("wine: returns S_FALSE for NULL ssa"); return; }
    /* the ssa check comes first, so NULL output pointers are not dereferenced */
    EXPECT_EQ((DWORD)x2c(NULL, 0, NULL, NULL), E_INVALIDARG_DW);
    EXPECT_EQ((DWORD)x2c(NULL, 5, NULL, (int[1]){0}), E_INVALIDARG_DW);
}
/* NOTE: ScriptStringXtoCP with NULL piCh/piTrailing on a valid ssa frees the analysis (UspFreeAnalysisMem) before
 * returning E_INVALIDARG, so it is deliberately not exercised here. */

static void ss_roundtrip(const WCHAR *face, int h, const WCHAR *txt, int hard) {
    if (!hard && g7_is_wine()) { t_skip("wine: bidi/complex scripts crash here"); return; }
    ssa_analyse_t an = (ssa_analyse_t)g7_usp_proc("ScriptStringAnalyse"); ssa_free_t fr = (ssa_free_t)g7_usp_proc("ScriptStringFree");
    ssa_c2x_t c2x = (ssa_c2x_t)g7_usp_proc("ScriptStringCPtoX"); ssa_x2c_t x2c = (ssa_x2c_t)g7_usp_proc("ScriptStringXtoCP");
    if (!an || !fr || !c2x || !x2c) { t_skip("missing export"); return; }
    G7DC d;
    if (!g7_dc_create(&d, 32, 32, face, h)) { t_skip("dc"); return; }
    SCRIPT_STRING_ANALYSIS ssa;
    if (!ss_build(d.dc, txt, &ssa, 0)) { g7_dc_destroy(&d); t_skip("analyse failed"); return; }
    int n = lstrlenW(txt), bad = 0, badt = 0, fi = -1;
    int prevx = -1;
    for (int i = 0; i <= n; i++) {
        int x = 0, ch = -9, tr = -9;
        /* the end position is queried as the trailing edge of the last char: a failing ScriptStringCPtoX frees the analysis */
        if (i == n) { if (FAILED(c2x(ssa, n - 1, TRUE, &x))) { bad++; break; } }
        else if (FAILED(c2x(ssa, i, FALSE, &x))) { bad++; continue; }
        if (x < prevx) bad++;
        prevx = x;
        if (FAILED(x2c(ssa, x, &ch, &tr)) || ch != i || tr != 0) { bad++; if (fi < 0) fi = i; }
        /* trailing edge of char i is the leading edge of i+1 */
        if (i < n) {
            int xt = 0, ch2 = -9, tr2 = -9;
            if (FAILED(c2x(ssa, i, TRUE, &xt))) { badt++; continue; }
            if (FAILED(x2c(ssa, xt, &ch2, &tr2)) || !((ch2 == i && tr2 == 1) || (ch2 == i + 1 && tr2 == 0))) badt++;
        }
    }
    if (hard) CHECKF(bad == 0 && badt == 0, "roundtrip: %d leading mismatches (first at cp %d), %d trailing", bad, fi, badt);
    else SOFT(bad == 0 && badt == 0, "roundtrip: %d leading mismatches (first at cp %d), %d trailing", bad, fi, badt);
    fr(&ssa);
    if (!g7_is_wine()) CHECK(ssa == NULL); /* ScriptStringFree clears the caller's handle */
    g7_dc_destroy(&d);
}
#define SSRT(nm, face, h, txt, hard) TEST(gap7_ssxtocp_roundtrip_##nm) { SSREQ(); ss_roundtrip(face, h, txt, hard); }
SSRT(arial_hello, L"Arial", 16, L"Hello", 1)
SSRT(arial_one, L"Arial", 16, L"A", 1)
SSRT(arial_words, L"Arial", 14, L"alpha beta gamma", 1)
SSRT(arial_digits, L"Arial", 14, L"0123456789", 1)
SSRT(arial_big, L"Arial", 48, L"Big", 1)
SSRT(arial_tiny, L"Arial", 6, L"tiny text here", 1)
SSRT(courier, L"Courier New", 16, L"monospaced text", 1)
SSRT(times, L"Times New Roman", 20, L"Serif Sample", 1)
SSRT(tahoma, L"Tahoma", 13, L"Tahoma Sample 123", 1)
SSRT(segoe, L"Segoe UI", 15, L"Segoe UI sample", 1)
SSRT(consolas, L"Consolas", 14, L"int main(void)", 1)
SSRT(symbols, L"Arial", 16, L"#$%&*+=<>@[]{}", 1)
SSRT(spaces, L"Arial", 16, L"a  b   c", 1)
SSRT(latin1, L"Arial", 16, L"éèüñß", 1)
SSRT(greek, L"Arial", 16, L"αβγδ", 1)
SSRT(cyrillic, L"Arial", 16, L"привет", 1)
SSRT(long90, L"Arial", 11, L"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-0123456789abcdefghijklmnopqrst", 1)
SSRT(hebrew_observed, L"Arial", 16, L"אבגד", 0)
SSRT(mixed_bidi_observed, L"Arial", 16, L"abc אבג def", 0)
SSRT(arabic_observed, L"Arial", 16, L"مرحبا", 0)
SSRT(combining_observed, L"Arial", 16, L"éàô", 0)
SSRT(thai_observed, L"Tahoma", 16, L"สวัสดี", 0)

TEST(gap7_ssxtocp_out_of_range_x) {
    SSREQ();
    G7DC d; if (!g7_dc_create(&d, 32, 32, L"Arial", 16)) { t_skip("dc"); return; }
    SCRIPT_STRING_ANALYSIS ssa;
    if (!ss_build(d.dc, L"Range", &ssa, 0)) { g7_dc_destroy(&d); t_skip("analyse"); return; }
    int total = 0;
    EXPECT_EQ((DWORD)c2x(ssa, 4, TRUE, &total), 0);
    CHECK(total > 0);
    int ch, tr;
    EXPECT_EQ((DWORD)x2c(ssa, -1, &ch, &tr), 0); EXPECT_EQ(ch, -1); EXPECT_EQ(tr, 1);
    EXPECT_EQ((DWORD)x2c(ssa, INT_MIN, &ch, &tr), 0); EXPECT_EQ(ch, -1); EXPECT_EQ(tr, 1);
    EXPECT_EQ((DWORD)x2c(ssa, total, &ch, &tr), 0); EXPECT_EQ(ch, 5); EXPECT_EQ(tr, 0);
    EXPECT_EQ((DWORD)x2c(ssa, total + 1, &ch, &tr), 0); EXPECT_EQ(ch, 5); EXPECT_EQ(tr, 0);
    EXPECT_EQ((DWORD)x2c(ssa, total + 1000, &ch, &tr), 0); EXPECT_EQ(ch, 5); EXPECT_EQ(tr, 0);
    EXPECT_EQ((DWORD)x2c(ssa, INT_MAX, &ch, &tr), 0); EXPECT_EQ(ch, 5); EXPECT_EQ(tr, 0);
    EXPECT_EQ((DWORD)x2c(ssa, total - 1, &ch, &tr), 0); EXPECT_EQ(ch, 4);
    fr(&ssa); g7_dc_destroy(&d);
}
TEST(gap7_ssxtocp_monotonic_sweep) {
    SSREQ();
    G7DC d; if (!g7_dc_create(&d, 32, 32, L"Times New Roman", 17)) { t_skip("dc"); return; }
    SCRIPT_STRING_ANALYSIS ssa;
    if (!ss_build(d.dc, L"Monotonic sweep of a longer line", &ssa, 0)) { g7_dc_destroy(&d); t_skip("analyse"); return; }
    int n = 32, total = 0;
    c2x(ssa, n - 1, TRUE, &total);
    int prevch = -2, prevtr = 0, bad = 0;
    for (int x = -2; x <= total + 2; x++) {
        int ch = -9, tr = -9;
        if (FAILED(x2c(ssa, x, &ch, &tr))) { bad++; continue; }
        if (ch < -1 || ch > n || (tr != 0 && tr != 1)) bad++;
        if (ch < prevch || (ch == prevch && tr < prevtr && ch >= 0)) bad++;
        prevch = ch; prevtr = tr;
    }
    CHECKF(bad == 0, "%d violations", bad);
    fr(&ssa); g7_dc_destroy(&d);
}
TEST(gap7_ssxtocp_repeat_stable) {
    SSREQ();
    G7DC d; if (!g7_dc_create(&d, 32, 32, L"Arial", 16)) { t_skip("dc"); return; }
    SCRIPT_STRING_ANALYSIS ssa;
    if (!ss_build(d.dc, L"Stable", &ssa, 0)) { g7_dc_destroy(&d); t_skip("analyse"); return; }
    int ch0 = -9, tr0 = -9; x2c(ssa, 17, &ch0, &tr0);
    int bad = 0;
    for (int i = 0; i < 500; i++) { int ch, tr; if (FAILED(x2c(ssa, 17, &ch, &tr)) || ch != ch0 || tr != tr0) bad++; }
    CHECK(bad == 0);
    fr(&ssa); g7_dc_destroy(&d);
}
TEST(gap7_ssxtocp_no_gdi_leak) {
    SSREQ();
    G7DC d; if (!g7_dc_create(&d, 32, 32, L"Arial", 16)) { t_skip("dc"); return; }
    int before = g7_gui();
    for (int i = 0; i < 50; i++) {
        SCRIPT_STRING_ANALYSIS ssa;
        if (!ss_build(d.dc, L"leak check", &ssa, 0)) { t_skip("analyse"); break; }
        int ch, tr; x2c(ssa, 10, &ch, &tr);
        fr(&ssa);
    }
    int after = g7_gui();
    EXPECT_EQ(after, before);
    g7_dc_destroy(&d);
}
static void g7_ss_thr(void *a) {
    void **p = a; HDC dc = p[0]; int *res = p[1];
    SCRIPT_STRING_ANALYSIS ssa;
    ssa_x2c_t x2c = (ssa_x2c_t)g7_usp_proc("ScriptStringXtoCP"); ssa_free_t fr = (ssa_free_t)g7_usp_proc("ScriptStringFree");
    if (!ss_build(dc, L"thread", &ssa, 0)) { res[0] = -1; return; }
    int ch, tr; HRESULT hr = x2c(ssa, 0, &ch, &tr);
    res[0] = hr == 0 && ch == 0 && tr == 0; fr(&ssa);
}
TEST(gap7_ssxtocp_other_thread_with_own_dc) {
    SSREQ();
    int res[1] = { 0 };
    G7DC d; if (!g7_dc_create(&d, 32, 32, L"Arial", 16)) { t_skip("dc"); return; }
    void *args[2] = { d.dc, res };
    g7_run_thread(g7_ss_thr, args);
    CHECK(res[0] == 1);
    g7_dc_destroy(&d);
}
TEST(gap7_ssxtocp_different_ssa_independent) {
    SSREQ();
    G7DC d; if (!g7_dc_create(&d, 32, 32, L"Arial", 16)) { t_skip("dc"); return; }
    SCRIPT_STRING_ANALYSIS a, b;
    if (!ss_build(d.dc, L"short", &a, 0) || !ss_build(d.dc, L"a much longer string", &b, 0)) { t_skip("analyse"); g7_dc_destroy(&d); return; }
    int ch, tr;
    x2c(a, 100000, &ch, &tr); EXPECT_EQ(ch, 5);
    x2c(b, 100000, &ch, &tr); EXPECT_EQ(ch, 20);
    fr(&a); x2c(b, 100000, &ch, &tr); EXPECT_EQ(ch, 20);
    fr(&b); g7_dc_destroy(&d);
}
TEST(gap7_ssxtocp_clip_flag_same_geometry) {
    SSREQ();
    G7DC d; if (!g7_dc_create(&d, 32, 32, L"Arial", 16)) { t_skip("dc"); return; }
    SCRIPT_STRING_ANALYSIS a, b;
    if (!ss_build(d.dc, L"clip text", &a, 0) || !ss_build(d.dc, L"clip text", &b, SSA_CLIP)) { t_skip("analyse"); g7_dc_destroy(&d); return; }
    int xa, xb; c2x(a, 8, TRUE, &xa); c2x(b, 8, TRUE, &xb);
    SOFT(xa == xb, "widths %d vs %d", xa, xb);
    fr(&a); fr(&b); g7_dc_destroy(&d);
}

/* ScriptStringFree contract used by the tests above (handle cleared, NULL rejected) */
TEST(gap7_ssfree_null_and_cleared_handle) {
    ssa_free_t fr = (ssa_free_t)g7_usp_proc("ScriptStringFree"); REQUIRE(fr);
    if (g7_is_wine()) { t_skip("wine: differs"); return; }
    EXPECT_EQ((DWORD)fr(NULL), E_INVALIDARG_DW);
    SCRIPT_STRING_ANALYSIS none = NULL;
    EXPECT_EQ((DWORD)fr(&none), E_INVALIDARG_DW);
}

/* ---------------- more ScriptStringXtoCP shapes ---------------- */
TEST(gap7_ssxtocp_all_lengths_roundtrip) {
    SSREQ();
    G7DC d; if (!g7_dc_create(&d, 32, 32, L"Arial", 14)) { t_skip("dc"); return; }
    static const WCHAR base[] = L"abcdefghijklmnopqrstuvwxyz0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int bad = 0;
    for (int len = 1; len <= 40; len++) {
        WCHAR t[80]; memcpy(t, base, len * sizeof(WCHAR)); t[len] = 0;
        SCRIPT_STRING_ANALYSIS ssa;
        if (!ss_build(d.dc, t, &ssa, 0)) { bad++; continue; }
        for (int i = 0; i < len; i++) {
            int x, ch = -9, tr = -9;
            if (FAILED(c2x(ssa, i, FALSE, &x)) || FAILED(x2c(ssa, x, &ch, &tr)) || ch != i || tr != 0) { bad++; break; }
        }
        fr(&ssa);
    }
    CHECKF(bad == 0, "%d failing lengths", bad);
    g7_dc_destroy(&d);
}
TEST(gap7_ssxtocp_password_flag_uniform) {
    SSREQ();
    G7DC d; if (!g7_dc_create(&d, 32, 32, L"Arial", 16)) { t_skip("dc"); return; }
    SCRIPT_STRING_ANALYSIS a, b;
    if (!ss_build(d.dc, L"iii", &a, SSA_PASSWORD) || !ss_build(d.dc, L"WWW", &b, SSA_PASSWORD)) { t_skip("analyse"); g7_dc_destroy(&d); return; }
    int xa = 0, xb = 0; c2x(a, 2, TRUE, &xa); c2x(b, 2, TRUE, &xb);
    /* in password mode every character is drawn as the same mask glyph */
    SOFT(xa == xb, "password widths differ: %d vs %d", xa, xb);
    int ch, tr; x2c(a, xa / 2, &ch, &tr);
    SOFT(ch >= 0 && ch <= 3, "ch %d", ch);
    fr(&a); fr(&b); g7_dc_destroy(&d);
}
TEST(gap7_ssxtocp_monospace_exact_division) {
    SSREQ();
    G7DC d; if (!g7_dc_create(&d, 32, 32, L"Courier New", 16)) { t_skip("dc"); return; }
    SCRIPT_STRING_ANALYSIS ssa;
    if (!ss_build(d.dc, L"abcdefgh", &ssa, 0)) { t_skip("analyse"); g7_dc_destroy(&d); return; }
    int cw = 0; c2x(ssa, 0, TRUE, &cw);
    CHECK(cw > 0);
    int bad = 0;
    for (int x = 0; x < cw * 8; x++) {
        int ch, tr; x2c(ssa, x, &ch, &tr);
        if (ch != x / cw || tr != ((x % cw) * 2) / cw) bad++;
    }
    CHECKF(bad == 0, "%d mismatches (cw=%d)", bad, cw);
    fr(&ssa); g7_dc_destroy(&d);
}
TEST(gap7_ssxtocp_font_size_scales_widths) {
    SSREQ();
    G7DC s, b;
    if (!g7_dc_create(&s, 16, 16, L"Arial", 12) || !g7_dc_create(&b, 16, 16, L"Arial", 48)) { t_skip("dc"); return; }
    SCRIPT_STRING_ANALYSIS a, c;
    if (!ss_build(s.dc, L"scale", &a, 0) || !ss_build(b.dc, L"scale", &c, 0)) { t_skip("analyse"); return; }
    int xs = 0, xb = 0; c2x(a, 4, TRUE, &xs); c2x(c, 4, TRUE, &xb);
    CHECKF(xb > xs * 3 && xb < xs * 5, "12px width %d, 48px width %d", xs, xb);
    int ch, tr; x2c(a, xs - 1, &ch, &tr); EXPECT_EQ(ch, 4);
    x2c(c, xb - 1, &ch, &tr); EXPECT_EQ(ch, 4);
    fr(&a); fr(&c); g7_dc_destroy(&s); g7_dc_destroy(&b);
}
TEST(gap7_ssxtocp_trailing_only_values_0_1) {
    SSREQ();
    G7DC d; if (!g7_dc_create(&d, 32, 32, L"Tahoma", 14)) { t_skip("dc"); return; }
    SCRIPT_STRING_ANALYSIS ssa;
    if (!ss_build(d.dc, L"Trailing flag values", &ssa, 0)) { t_skip("analyse"); g7_dc_destroy(&d); return; }
    int total = 0; c2x(ssa, 19, TRUE, &total);
    int bad = 0;
    for (int x = -5; x < total + 5; x++) { int ch, tr; if (FAILED(x2c(ssa, x, &ch, &tr)) || (tr != 0 && tr != 1)) bad++; }
    CHECK(bad == 0);
    fr(&ssa); g7_dc_destroy(&d);
}
TEST(gap7_ssxtocp_each_char_center_hits_char) {
    SSREQ();
    G7DC d; if (!g7_dc_create(&d, 32, 32, L"Arial", 20)) { t_skip("dc"); return; }
    SCRIPT_STRING_ANALYSIS ssa;
    if (!ss_build(d.dc, L"Center hit", &ssa, 0)) { t_skip("analyse"); g7_dc_destroy(&d); return; }
    int bad = 0;
    for (int i = 0; i < 10; i++) {
        int x0, x1; c2x(ssa, i, FALSE, &x0); c2x(ssa, i, TRUE, &x1);
        if (x1 <= x0) { bad++; continue; }
        int ch, tr; x2c(ssa, (x0 + x1) / 2 - ((x1 - x0) % 2 == 0 ? 1 : 0), &ch, &tr);
        if (ch != i) bad++;
    }
    CHECKF(bad == 0, "%d chars whose centre maps elsewhere", bad);
    fr(&ssa); g7_dc_destroy(&d);
}
