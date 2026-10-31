/* ScriptTextOut, ScriptSubstituteSingleGlyph, ftsWordBreak.
 * Rendering is checked only through deterministic properties on a 32bpp DIB memory DC (ink present / confined to
 * a clip rect / opaque fill colour / translation invariance), never against exact anti-aliased pixel values. */
#include "g7.h"

typedef HRESULT (WINAPI *sto_t)(HDC, SCRIPT_CACHE *, int, int, UINT, const RECT *, const SCRIPT_ANALYSIS *, const WCHAR *, int, const WORD *, int, const int *, const int *, const GOFFSET *);
#define STO() ((sto_t)g7_usp_proc("ScriptTextOut"))

#define TREQ() \
    sto_t sto = STO(); REQUIRE(sto); \
    G7DC d; G7RUN r; \
    if (!g7_dc_create(&d, 240, 70, L"Arial", 22)) { t_skip("dc"); return; } \
    if (!g7_run_build(d.dc, &r, L"Hello")) { g7_dc_destroy(&d); t_skip("shape"); return; }
#define TDONE() do { g7_run_free(&r); g7_dc_destroy(&d); } while (0)

/* the fNoGlyphIndex path never touches the cache, so NULL is valid on Windows (other hosts insist on one) */
#define NOPSC (g7_is_wine() ? &r.sc : NULL)

static HRESULT draw(sto_t sto, G7DC *d, G7RUN *r, int x, int y, UINT opt, const RECT *rc) {
    return sto(d->dc, &r->sc, x, y, opt, rc, &r->psa, NULL, 0, r->glyphs, r->ng, r->adv, NULL, r->go);
}

/* ---------------- invalid parameters ---------------- */
#define BADCALL_X(nm, hard, ARGS) \
TEST(gap7_stextout_invalid_##nm) { \
    TREQ(); \
    DWORD before = 0; for (int i = 0; i < d.w * d.h; i++) before += d.bits[i]; \
    SCRIPT_CACHE *psc = &r.sc; HDC hdc = d.dc; (void)psc; (void)hdc; \
    HRESULT hr = sto ARGS; \
    if (hard) CHECKF(FAILED(hr), "hr=%lx", (unsigned long)hr); \
    else SOFT(FAILED(hr), "hr=%lx", (unsigned long)hr); \
    if (hr != (HRESULT)G7_E_INVALIDARG) t_note("invalid_" #nm ": hr=%lx (E_INVALIDARG expected)", (unsigned long)hr); \
    DWORD after = 0; for (int i = 0; i < d.w * d.h; i++) after += d.bits[i]; \
    CHECK(before == after); /* nothing drawn */ \
    TDONE(); \
}
#define BADCALL(nm, ARGS) BADCALL_X(nm, 1, ARGS)
#define BADCALL_SOFT(nm, ARGS) BADCALL_X(nm, 0, ARGS)
BADCALL(null_hdc,    (NULL, psc, 5, 5, 0, NULL, &r.psa, NULL, 0, r.glyphs, r.ng, r.adv, NULL, r.go))
BADCALL(null_psa,    (hdc, psc, 5, 5, 0, NULL, NULL, NULL, 0, r.glyphs, r.ng, r.adv, NULL, r.go))
BADCALL(null_glyphs, (hdc, psc, 5, 5, 0, NULL, &r.psa, NULL, 0, NULL, r.ng, r.adv, NULL, r.go))
BADCALL(null_advance,(hdc, psc, 5, 5, 0, NULL, &r.psa, NULL, 0, r.glyphs, r.ng, NULL, NULL, r.go))
BADCALL(null_cache_glyph_path, (hdc, NULL, 5, 5, 0, NULL, &r.psa, NULL, 0, r.glyphs, r.ng, r.adv, NULL, r.go))
BADCALL(all_null,    (NULL, NULL, 0, 0, 0, NULL, NULL, NULL, 0, NULL, 0, NULL, NULL, NULL))
BADCALL_SOFT(bogus_hdc,   ((HDC)(ULONG_PTR)0x0DEADBEE, psc, 5, 5, 0, NULL, &r.psa, NULL, 0, r.glyphs, r.ng, r.adv, NULL, r.go))
BADCALL_SOFT(hdc_minus1,  ((HDC)(ULONG_PTR)-1, psc, 5, 5, 0, NULL, &r.psa, NULL, 0, r.glyphs, r.ng, r.adv, NULL, r.go))

TEST(gap7_stextout_invalid_hdc_is_pen_handle) {
    TREQ();
    HPEN (WINAPI *cp)(int, int, COLORREF) = (void *)t_proc("CreatePen");
    REQUIRE(cp);
    HPEN pen = cp(PS_SOLID, 1, 0);
    HRESULT hr = sto((HDC)pen, &r.sc, 5, 5, 0, NULL, &r.psa, NULL, 0, r.glyphs, r.ng, r.adv, NULL, r.go);
    t_note("pen handle as hdc -> hr=%lx", (unsigned long)hr);
    U(DeleteObject)(pen);
    SOFT(FAILED(hr), "hr=%lx", (unsigned long)hr);
    CHECK(g7_dc_ink(&d, NULL) == 0);
    TDONE();
}
TEST(gap7_stextout_invalid_hdc_deleted) {
    TREQ();
    HDC (WINAPI *ccdc)(HDC) = (void *)t_proc("CreateCompatibleDC");
    REQUIRE(ccdc);
    HDC dc2 = ccdc(NULL); U(DeleteDC)(dc2);
    HRESULT hr = sto(dc2, &r.sc, 5, 5, 0, NULL, &r.psa, NULL, 0, r.glyphs, r.ng, r.adv, NULL, r.go);
    SOFT(FAILED(hr), "hr=%lx", (unsigned long)hr);
    t_note("deleted hdc -> hr=%lx", (unsigned long)hr);
    TDONE();
}

/* ---------------- plain drawing ---------------- */
TEST(gap7_stextout_draws_ink) {
    TREQ();
    HRESULT hr = draw(sto, &d, &r, 10, 10, 0, NULL);
    EXPECT_EQ((DWORD)hr, 0);
    RECT bb; int n = g7_dc_ink(&d, &bb);
    CHECK(n > 20);
    int wid = g7_run_width(&r);
    CHECKF(bb.left >= 10 - 4 && bb.right <= 10 + wid + 4, "bbox x %d..%d, run width %d", (int)bb.left, (int)bb.right, wid);
    CHECKF(bb.top >= 10 - 1 && bb.bottom <= 10 + 40, "bbox y %d..%d", (int)bb.top, (int)bb.bottom);
    CHECK(r.sc != NULL); /* the cache handle is populated */
    TDONE();
}
TEST(gap7_stextout_ink_in_text_box) {
    TREQ();
    draw(sto, &d, &r, 30, 12, 0, NULL);
    RECT bb; g7_dc_ink(&d, &bb);
    TEXTMETRICW tm; BOOL (WINAPI *gtm)(HDC, LPTEXTMETRICW) = (void *)t_proc("GetTextMetricsW");
    REQUIRE(gtm); gtm(d.dc, &tm);
    CHECKF(bb.top >= 12 - 1 && bb.bottom <= 12 + tm.tmHeight + 1, "bbox y %d..%d height %ld", (int)bb.top, (int)bb.bottom, tm.tmHeight);
    TDONE();
}
TEST(gap7_stextout_translation_invariant) {
    TREQ();
    draw(sto, &d, &r, 10, 10, 0, NULL);
    RECT a; int na = g7_dc_ink(&d, &a);
    g7_dc_clear(&d);
    draw(sto, &d, &r, 50, 20, 0, NULL);
    RECT b; int nb = g7_dc_ink(&d, &b);
    CHECKF(b.left - a.left == 40 && b.right - a.right == 40, "x shift %ld/%ld", b.left - a.left, b.right - a.right);
    CHECKF(b.top - a.top == 10 && b.bottom - a.bottom == 10, "y shift %ld/%ld", b.top - a.top, b.bottom - a.bottom);
    CHECKF(na == nb, "ink %d vs %d", na, nb);
    TDONE();
}
TEST(gap7_stextout_pixel_identical_when_repeated) {
    TREQ();
    draw(sto, &d, &r, 12, 8, 0, NULL);
    DWORD *c1 = malloc((size_t)d.w * d.h * 4); memcpy(c1, d.bits, (size_t)d.w * d.h * 4);
    g7_dc_clear(&d);
    draw(sto, &d, &r, 12, 8, 0, NULL);
    CHECK(memcmp(c1, d.bits, (size_t)d.w * d.h * 4) == 0);
    free(c1);
    TDONE();
}
TEST(gap7_stextout_offscreen_negative_coords) {
    TREQ();
    EXPECT_EQ((DWORD)draw(sto, &d, &r, -1000, -1000, 0, NULL), 0);
    CHECK(g7_dc_ink(&d, NULL) == 0);
    EXPECT_EQ((DWORD)draw(sto, &d, &r, 100000, 100000, 0, NULL), 0);
    CHECK(g7_dc_ink(&d, NULL) == 0);
    EXPECT_EQ((DWORD)draw(sto, &d, &r, -20, 5, 0, NULL), 0); /* partially off the left edge */
    CHECK(g7_dc_ink(&d, NULL) > 0);
    TDONE();
}
TEST(gap7_stextout_zero_glyphs) {
    TREQ();
    HRESULT hr = sto(d.dc, &r.sc, 10, 10, 0, NULL, &r.psa, NULL, 0, r.glyphs, 0, r.adv, NULL, r.go);
    t_note("cGlyphs=0 -> hr=%lx", (unsigned long)hr);
    CHECK(g7_dc_ink(&d, NULL) == 0);
    TDONE();
}
TEST(gap7_stextout_single_glyph_prefix) {
    TREQ();
    EXPECT_EQ((DWORD)sto(d.dc, &r.sc, 10, 10, 0, NULL, &r.psa, NULL, 0, r.glyphs, 1, r.adv, NULL, r.go), 0);
    RECT one; int n1 = g7_dc_ink(&d, &one);
    g7_dc_clear(&d);
    draw(sto, &d, &r, 10, 10, 0, NULL);
    RECT all; int n5 = g7_dc_ink(&d, &all);
    CHECK(n1 > 0 && n1 < n5);
    CHECK(one.right < all.right);
    CHECKF(one.left == all.left, "left %ld vs %ld", one.left, all.left);
    TDONE();
}
TEST(gap7_stextout_cache_reused_across_calls) {
    TREQ();
    draw(sto, &d, &r, 10, 10, 0, NULL);
    SCRIPT_CACHE c0 = r.sc;
    for (int i = 0; i < 20; i++) EXPECT_EQ((DWORD)draw(sto, &d, &r, 10, 10, 0, NULL), 0);
    CHECK(r.sc == c0); /* same cache object keeps being used */
    TDONE();
}
TEST(gap7_stextout_fresh_cache_each_time) {
    TREQ();
    HRESULT (WINAPI *fc)(SCRIPT_CACHE *) = (void *)g7_usp_proc("ScriptFreeCache");
    REQUIRE(fc);
    int before = g7_gui();
    for (int i = 0; i < 30; i++) {
        SCRIPT_CACHE sc = NULL;
        EXPECT_EQ((DWORD)sto(d.dc, &sc, 10, 10, 0, NULL, &r.psa, NULL, 0, r.glyphs, r.ng, r.adv, NULL, r.go), 0);
        EXPECT_EQ((DWORD)fc(&sc), 0);
        CHECK(sc == NULL);
    }
    EXPECT_EQ(g7_gui(), before);
    TDONE();
}

/* ---------------- options ---------------- */
TEST(gap7_stextout_clipped) {
    TREQ();
    RECT clip = { 20, 8, 45, 28 };
    EXPECT_EQ((DWORD)draw(sto, &d, &r, 10, 10, ETO_CLIPPED, &clip), 0);
    RECT bb; int n = g7_dc_ink(&d, &bb);
    CHECK(n > 0);
    CHECKF(bb.left >= clip.left && bb.right < clip.right && bb.top >= clip.top && bb.bottom < clip.bottom,
           "ink %ld,%ld-%ld,%ld outside clip", bb.left, bb.top, bb.right, bb.bottom);
    TDONE();
}
TEST(gap7_stextout_clipped_empty_rect) {
    TREQ();
    RECT clip = { 30, 30, 30, 30 };
    draw(sto, &d, &r, 10, 10, ETO_CLIPPED, &clip);
    CHECK(g7_dc_ink(&d, NULL) == 0);
    RECT inv = { 50, 50, 20, 20 };
    draw(sto, &d, &r, 10, 10, ETO_CLIPPED, &inv);
    SOFT(g7_dc_ink(&d, NULL) == 0, "inverted clip rect drew ink"); /* GDI may or may not normalise it */
    TDONE();
}
TEST(gap7_stextout_clipped_away_from_text) {
    TREQ();
    RECT clip = { 150, 40, 230, 65 };
    draw(sto, &d, &r, 10, 10, ETO_CLIPPED, &clip);
    CHECK(g7_dc_ink(&d, NULL) == 0);
    TDONE();
}
TEST(gap7_stextout_clipped_without_rect_ignored) {
    TREQ();
    /* no ETO_CLIPPED -> rect is ignored even if supplied */
    RECT clip = { 0, 0, 1, 1 };
    draw(sto, &d, &r, 10, 10, 0, &clip);
    RECT bb; g7_dc_ink(&d, &bb);
    CHECK(bb.right > 20);
    TDONE();
}
TEST(gap7_stextout_opaque_fills_rect) {
    TREQ();
    U(SetBkColor)(d.dc, RGB(255, 0, 0));
    RECT rc = { 5, 3, 200, 60 };
    EXPECT_EQ((DWORD)draw(sto, &d, &r, 10, 10, ETO_OPAQUE, &rc), 0);
    int bad = 0;
    for (int y = 0; y < d.h; y++) for (int x = 0; x < d.w; x++) {
        DWORD px = d.bits[y * d.w + x] & 0xFFFFFF;
        int in = x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom;
        if (!in && px != 0xFFFFFF) bad++;                     /* outside the rect only the (white) background */
    }
    CHECKF(bad == 0, "%d pixels outside the opaque rect changed", bad);
    CHECK((d.bits[3 * d.w + 5] & 0xFFFFFF) == 0xFF0000);          /* rect corners are pure background colour */
    CHECK((d.bits[59 * d.w + 199] & 0xFFFFFF) == 0xFF0000);
    CHECK((d.bits[3 * d.w + 199] & 0xFFFFFF) == 0xFF0000);
    CHECK((d.bits[59 * d.w + 5] & 0xFFFFFF) == 0xFF0000);
    CHECK((d.bits[2 * d.w + 5] & 0xFFFFFF) == 0xFFFFFF);          /* one pixel outside */
    CHECK((d.bits[3 * d.w + 4] & 0xFFFFFF) == 0xFFFFFF);
    CHECK((d.bits[60 * d.w + 100] & 0xFFFFFF) == 0xFFFFFF);
    CHECK((d.bits[30 * d.w + 200] & 0xFFFFFF) == 0xFFFFFF);
    TDONE();
}
TEST(gap7_stextout_opaque_and_clipped) {
    TREQ();
    U(SetBkColor)(d.dc, RGB(0, 200, 0));
    RECT rc = { 20, 5, 60, 35 };
    EXPECT_EQ((DWORD)draw(sto, &d, &r, 10, 10, ETO_OPAQUE | ETO_CLIPPED, &rc), 0);
    RECT bb; g7_dc_ink(&d, &bb);
    CHECKF(bb.left == rc.left && bb.top == rc.top && bb.right == rc.right - 1 && bb.bottom == rc.bottom - 1,
           "changed area %ld,%ld-%ld,%ld", bb.left, bb.top, bb.right, bb.bottom);
    TDONE();
}
TEST(gap7_stextout_opaque_without_rect) {
    TREQ();
    U(SetBkColor)(d.dc, RGB(255, 0, 0));
    HRESULT hr = draw(sto, &d, &r, 10, 10, ETO_OPAQUE, NULL);
    t_note("ETO_OPAQUE with NULL rect -> hr=%lx", (unsigned long)hr);
    TDONE();
}
TEST(gap7_stextout_transparent_bkmode) {
    TREQ();
    U(SetBkColor)(d.dc, RGB(255, 0, 0));
    U(SetBkMode)(d.dc, TRANSPARENT);
    draw(sto, &d, &r, 10, 10, 0, NULL);
    int red = 0; for (int i = 0; i < d.w * d.h; i++) red += (d.bits[i] & 0xFFFFFF) == 0xFF0000;
    CHECK(red == 0);
    CHECK(g7_dc_ink(&d, NULL) > 0);
    TDONE();
}
TEST(gap7_stextout_opaque_bkmode_fills_cells) {
    TREQ();
    U(SetBkColor)(d.dc, RGB(255, 0, 0));
    U(SetBkMode)(d.dc, OPAQUE);
    draw(sto, &d, &r, 10, 10, 0, NULL);
    int red = 0; for (int i = 0; i < d.w * d.h; i++) red += (d.bits[i] & 0xFFFFFF) == 0xFF0000;
    SOFT(red > 100, "red pixels %d", red);
    TDONE();
}
TEST(gap7_stextout_text_color) {
    TREQ();
    U(SetTextColor)(d.dc, RGB(0, 0, 255));
    draw(sto, &d, &r, 10, 10, 0, NULL);
    int blue = 0, other = 0;
    for (int i = 0; i < d.w * d.h; i++) {
        DWORD px = d.bits[i] & 0xFFFFFF;
        if (px == 0x0000FF) blue++;
        /* anti-aliased pixels lie between white and blue: R == G always for grey-scale AA, only B is full */
        if (px != 0xFFFFFF && (px & 0xFF) != 0xFF) other++;
    }
    SOFT(blue > 0, "no pure text-colour pixel");
    SOFT(other == 0, "%d pixels with blue < 255 (text should only darken R/G)", other);
    TDONE();
}
TEST(gap7_stextout_extra_option_bits_masked) {
    TREQ();
    /* only ETO_OPAQUE | ETO_CLIPPED are honoured; ETO_GLYPH_INDEX and friends are masked off */
    draw(sto, &d, &r, 10, 10, 0, NULL);
    DWORD *c1 = malloc((size_t)d.w * d.h * 4); memcpy(c1, d.bits, (size_t)d.w * d.h * 4);
    g7_dc_clear(&d);
    HRESULT hr = draw(sto, &d, &r, 10, 10, ETO_GLYPH_INDEX | ETO_RTLREADING | ETO_IGNORELANGUAGE | ETO_PDY, NULL);
    EXPECT_EQ((DWORD)hr, 0);
    SOFT(memcmp(c1, d.bits, (size_t)d.w * d.h * 4) == 0, "extra option bits changed the output");
    free(c1);
    TDONE();
}

/* ---------------- advance / justify / goffset ---------------- */
TEST(gap7_stextout_justify_spreads_glyphs) {
    TREQ();
    draw(sto, &d, &r, 10, 10, 0, NULL);
    RECT a; g7_dc_ink(&d, &a);
    g7_dc_clear(&d);
    int just[G7_MAXG]; for (int i = 0; i < r.ng; i++) just[i] = r.adv[i] + 6;
    EXPECT_EQ((DWORD)sto(d.dc, &r.sc, 10, 10, 0, NULL, &r.psa, NULL, 0, r.glyphs, r.ng, r.adv, just, r.go), 0);
    RECT b; g7_dc_ink(&d, &b);
    int delta = b.right - a.right;
    SOFT(delta >= 6 * (r.ng - 1) - 1 && delta <= 6 * (r.ng - 1) + 1, "right edge moved %d, wanted ~%d", delta, 6 * (r.ng - 1));
    CHECK(b.left == a.left);
    TDONE();
}
TEST(gap7_stextout_zero_advances_overprint) {
    TREQ();
    int zero[G7_MAXG] = { 0 };
    EXPECT_EQ((DWORD)sto(d.dc, &r.sc, 10, 10, 0, NULL, &r.psa, NULL, 0, r.glyphs, r.ng, zero, NULL, r.go), 0);
    RECT bb; g7_dc_ink(&d, &bb);
    CHECKF(bb.right - bb.left < g7_run_width(&r) / 2, "ink width %ld not collapsed", bb.right - bb.left);
    TDONE();
}
TEST(gap7_stextout_goffset_shifts_ink) {
    TREQ();
    draw(sto, &d, &r, 30, 20, 0, NULL);
    RECT a; g7_dc_ink(&d, &a);
    g7_dc_clear(&d);
    GOFFSET go[G7_MAXG]; for (int i = 0; i < r.ng; i++) { go[i].du = 7; go[i].dv = 4; }
    EXPECT_EQ((DWORD)sto(d.dc, &r.sc, 30, 20, 0, NULL, &r.psa, NULL, 0, r.glyphs, r.ng, r.adv, NULL, go), 0);
    RECT b; g7_dc_ink(&d, &b);
    int dx = b.left - a.left, dy = b.top - a.top;
    SOFT(dx == 7, "dx=%d", dx);
    SOFT(dy == -4 || dy == 4, "dy=%d", dy);
    t_note("goffset (7,4): ink moved (%d,%d)", dx, dy);
    TDONE();
}
TEST(gap7_stextout_null_goffset_ok) {
    TREQ();
    EXPECT_EQ((DWORD)sto(d.dc, &r.sc, 10, 10, 0, NULL, &r.psa, NULL, 0, r.glyphs, r.ng, r.adv, NULL, NULL), 0);
    RECT bb; g7_dc_ink(&d, &bb);
    CHECK(bb.right > bb.left);
    TDONE();
}
TEST(gap7_stextout_null_goffset_equals_zero_goffset) {
    TREQ();
    sto(d.dc, &r.sc, 10, 10, 0, NULL, &r.psa, NULL, 0, r.glyphs, r.ng, r.adv, NULL, NULL);
    DWORD *c1 = malloc((size_t)d.w * d.h * 4); memcpy(c1, d.bits, (size_t)d.w * d.h * 4);
    g7_dc_clear(&d);
    GOFFSET zero[G7_MAXG]; memset(zero, 0, sizeof zero);
    sto(d.dc, &r.sc, 10, 10, 0, NULL, &r.psa, NULL, 0, r.glyphs, r.ng, r.adv, NULL, zero);
    SOFT(memcmp(c1, d.bits, (size_t)d.w * d.h * 4) == 0, "NULL vs zero goffsets differ");
    free(c1);
    TDONE();
}

/* ---------------- fNoGlyphIndex: ScriptTextOut degrades to ExtTextOutW ---------------- */
TEST(gap7_stextout_noglyphindex_matches_exttextout) {
    TREQ();
    BOOL (WINAPI *eto)(HDC, int, int, UINT, const RECT *, LPCWSTR, UINT, const INT *) = (void *)t_proc("ExtTextOutW");
    REQUIRE(eto);
    static const WCHAR txt[] = L"Hello";
    int dx[5] = { 14, 11, 7, 7, 12 };
    SCRIPT_ANALYSIS psa = r.psa; psa.fNoGlyphIndex = 1;
    /* psc is not used on this path */
    EXPECT_EQ((DWORD)sto(d.dc, NOPSC, 12, 9, 0, NULL, &psa, NULL, 0, (const WORD *)txt, 5, dx, NULL, NULL), 0);
    DWORD *c1 = malloc((size_t)d.w * d.h * 4); memcpy(c1, d.bits, (size_t)d.w * d.h * 4);
    CHECK(g7_dc_ink(&d, NULL) > 0);
    g7_dc_clear(&d);
    eto(d.dc, 12, 9, 0x1000, NULL, txt, 5, dx);
    CHECK(memcmp(c1, d.bits, (size_t)d.w * d.h * 4) == 0);
    free(c1);
    TDONE();
}
TEST(gap7_stextout_noglyphindex_justify_overrides_advance) {
    TREQ();
    if (g7_is_wine()) { t_skip("wine: piJustify not preferred"); return; }
    BOOL (WINAPI *eto)(HDC, int, int, UINT, const RECT *, LPCWSTR, UINT, const INT *) = (void *)t_proc("ExtTextOutW");
    REQUIRE(eto);
    static const WCHAR txt[] = L"Hello";
    int adv[5] = { 10, 10, 10, 10, 10 }, jus[5] = { 20, 20, 20, 20, 20 };
    SCRIPT_ANALYSIS psa = r.psa; psa.fNoGlyphIndex = 1;
    sto(d.dc, NOPSC, 12, 9, 0, NULL, &psa, NULL, 0, (const WORD *)txt, 5, adv, jus, NULL);
    DWORD *c1 = malloc((size_t)d.w * d.h * 4); memcpy(c1, d.bits, (size_t)d.w * d.h * 4);
    g7_dc_clear(&d);
    eto(d.dc, 12, 9, 0x1000, NULL, txt, 5, jus);
    CHECK(memcmp(c1, d.bits, (size_t)d.w * d.h * 4) == 0);
    free(c1);
    TDONE();
}
TEST(gap7_stextout_noglyphindex_null_hdc) {
    TREQ();
    static const WCHAR txt[] = L"Hello";
    int dx[5] = { 10, 10, 10, 10, 10 };
    SCRIPT_ANALYSIS psa = r.psa; psa.fNoGlyphIndex = 1;
    HRESULT hr = sto(NULL, NULL, 12, 9, 0, NULL, &psa, NULL, 0, (const WORD *)txt, 5, dx, NULL, NULL);
    CHECKF(FAILED(hr), "hr=%lx", (unsigned long)hr);
    TDONE();
}
TEST(gap7_stextout_noglyphindex_opaque_clipped) {
    TREQ();
    static const WCHAR txt[] = L"Hello";
    int dx[5] = { 14, 14, 14, 14, 14 };
    SCRIPT_ANALYSIS psa = r.psa; psa.fNoGlyphIndex = 1;
    U(SetBkColor)(d.dc, RGB(255, 0, 0));
    RECT rc = { 20, 5, 70, 40 };
    EXPECT_EQ((DWORD)sto(d.dc, NOPSC, 12, 9, ETO_OPAQUE | ETO_CLIPPED, &rc, &psa, NULL, 0, (const WORD *)txt, 5, dx, NULL, NULL), 0);
    RECT bb; g7_dc_ink(&d, &bb);
    CHECK(bb.left == 20 && bb.top == 5 && bb.right == 69 && bb.bottom == 39);
    TDONE();
}
TEST(gap7_stextout_glyph_path_vs_exttextout_glyph_index) {
    TREQ();
    BOOL (WINAPI *eto)(HDC, int, int, UINT, const RECT *, LPCWSTR, UINT, const INT *) = (void *)t_proc("ExtTextOutW");
    REQUIRE(eto);
    sto(d.dc, &r.sc, 12, 9, 0, NULL, &r.psa, NULL, 0, r.glyphs, r.ng, r.adv, NULL, NULL);
    DWORD *c1 = malloc((size_t)d.w * d.h * 4); memcpy(c1, d.bits, (size_t)d.w * d.h * 4);
    g7_dc_clear(&d);
    eto(d.dc, 12, 9, ETO_GLYPH_INDEX | 0x1000, NULL, (LPCWSTR)r.glyphs, r.ng, r.adv);
    SOFT(memcmp(c1, d.bits, (size_t)d.w * d.h * 4) == 0, "glyph-index rendering differs from ExtTextOutW");
    free(c1);
    TDONE();
}

/* ---------------- other fonts / sizes / threads ---------------- */
static void font_case(const WCHAR *face, int h, const WCHAR *txt) {
    sto_t sto = STO(); if (!sto) { t_skip("missing ScriptTextOut"); return; }
    G7DC d; G7RUN r;
    if (!g7_dc_create(&d, 320, 100, face, h)) { t_skip("dc"); return; }
    if (!g7_run_build(d.dc, &r, txt)) { g7_dc_destroy(&d); t_skip("shape"); return; }
    HRESULT hr = draw(sto, &d, &r, 8, 8, 0, NULL);
    EXPECT_EQ((DWORD)hr, 0);
    RECT bb; int n = g7_dc_ink(&d, &bb);
    CHECKF(n > 0, "no ink for %ls", face);
    int w = g7_run_width(&r);
    CHECKF(bb.left >= 8 - 6 && bb.right <= 8 + w + 6, "ink x %ld..%ld run width %d", bb.left, bb.right, w);
    g7_run_free(&r); g7_dc_destroy(&d);
}
#define FONTCASE(nm, face, h, txt) TEST(gap7_stextout_font_##nm) { font_case(face, h, txt); }
FONTCASE(arial_12, L"Arial", 12, L"Sample text")
FONTCASE(arial_40, L"Arial", 40, L"Big")
FONTCASE(courier_16, L"Courier New", 16, L"Fixed pitch")
FONTCASE(times_18, L"Times New Roman", 18, L"Serif text")
FONTCASE(tahoma_11, L"Tahoma", 11, L"Tahoma text")
FONTCASE(segoe_14, L"Segoe UI", 14, L"Segoe UI text")
FONTCASE(consolas_14, L"Consolas", 14, L"Consolas text")
FONTCASE(digits, L"Arial", 20, L"0123456789")
FONTCASE(single_char, L"Arial", 30, L"W")
FONTCASE(latin1, L"Arial", 20, L"éèüß")
FONTCASE(greek, L"Arial", 20, L"αβγ")

static void g7_sto_thr(void *a) {
    int *res = a; sto_t sto = STO();
    G7DC d; G7RUN r; *res = 0;
    if (!g7_dc_create(&d, 160, 50, L"Arial", 18)) return;
    if (g7_run_build(d.dc, &r, L"thread")) {
        *res = sto(d.dc, &r.sc, 5, 5, 0, NULL, &r.psa, NULL, 0, r.glyphs, r.ng, r.adv, NULL, r.go) == 0 && g7_dc_ink(&d, NULL) > 0;
        g7_run_free(&r);
    }
    g7_dc_destroy(&d);
}
TEST(gap7_stextout_other_thread) {
    REQUIRE(STO());
    int res = 0; g7_run_thread(g7_sto_thr, &res);
    CHECK(res == 1);
}
TEST(gap7_stextout_no_gdi_leak) {
    TREQ();
    int before = g7_gui();
    for (int i = 0; i < 40; i++) { g7_dc_clear(&d); draw(sto, &d, &r, 10, 10, 0, NULL); }
    EXPECT_EQ(g7_gui(), before);
    TDONE();
}
TEST(gap7_stextout_dc_state_preserved) {
    TREQ();
    HGDIOBJ (WINAPI *go)(HDC, int) = (void *)t_proc("GetCurrentObject");
    REQUIRE(go);
    HGDIOBJ f0 = go(d.dc, OBJ_FONT), b0 = go(d.dc, OBJ_BRUSH), p0 = go(d.dc, OBJ_PEN);
    COLORREF tc = U(GetTextColor)(d.dc), bc = U(GetBkColor)(d.dc);
    int bm = U(GetBkMode)(d.dc);
    draw(sto, &d, &r, 10, 10, 0, NULL);
    EXPECT_EQ(go(d.dc, OBJ_FONT), f0);
    EXPECT_EQ(go(d.dc, OBJ_BRUSH), b0);
    EXPECT_EQ(go(d.dc, OBJ_PEN), p0);
    EXPECT_EQ(U(GetTextColor)(d.dc), tc);
    EXPECT_EQ(U(GetBkColor)(d.dc), bc);
    EXPECT_EQ(U(GetBkMode)(d.dc), bm);
    TDONE();
}
TEST(gap7_stextout_does_not_move_current_position) {
    TREQ();
    POINT p0, p1;
    U(MoveToEx)(d.dc, 17, 23, NULL);
    draw(sto, &d, &r, 10, 10, 0, NULL);
    U(GetCurrentPositionEx)(d.dc, &p1);
    EXPECT_EQ(p1.x, 17); EXPECT_EQ(p1.y, 23);
    TDONE();
}
TEST(gap7_stextout_text_align_bottom) {
    TREQ();
    U(SetTextAlign)(d.dc, TA_BOTTOM);
    draw(sto, &d, &r, 10, 60, 0, NULL);
    RECT bb; g7_dc_ink(&d, &bb);
    SOFT(bb.bottom <= 61 && bb.top < 60, "TA_BOTTOM ink y %ld..%ld", bb.top, bb.bottom);
    t_note("TA_BOTTOM: ink y %ld..%ld", bb.top, bb.bottom);
    TDONE();
}
TEST(gap7_stextout_text_align_baseline) {
    TREQ();
    U(SetTextAlign)(d.dc, TA_BASELINE);
    draw(sto, &d, &r, 10, 40, 0, NULL);
    RECT bb; g7_dc_ink(&d, &bb);
    CHECKF(bb.bottom >= 38 && bb.bottom <= 44, "baseline ink bottom %ld", bb.bottom);
    CHECK(bb.top < 40);
    TDONE();
}
TEST(gap7_stextout_map_mode_text_origin) {
    TREQ();
    /* logical coordinates are mapped through the DC transform */
    U(SetViewportOrgEx)(d.dc, 30, 0, NULL);
    draw(sto, &d, &r, 0, 10, 0, NULL);
    RECT bb; g7_dc_ink(&d, &bb);
    CHECKF(bb.left >= 30 - 4 && bb.left <= 30 + 12, "ink left %ld with viewport org 30", bb.left);
    TDONE();
}

/* ================= ScriptSubstituteSingleGlyph ================= */
typedef HRESULT (WINAPI *sssg_t)(HDC, SCRIPT_CACHE *, SCRIPT_ANALYSIS *, OPENTYPE_TAG, OPENTYPE_TAG, OPENTYPE_TAG, LONG, WORD, WORD *);
#define SSG() ((sssg_t)g7_usp_proc("ScriptSubstituteSingleGlyph"))
#define TAG(a,b,c,d) ((OPENTYPE_TAG)((a)|((b)<<8)|((c)<<16)|((DWORD)(d)<<24)))

#define SGREQ() \
    sssg_t sg = SSG(); REQUIRE(sg); \
    G7DC d; G7RUN r; \
    if (!g7_dc_create(&d, 64, 64, L"Arial", 20)) { t_skip("dc"); return; } \
    if (!g7_run_build(d.dc, &r, L"Aa")) { g7_dc_destroy(&d); t_skip("shape"); return; }
#define SGDONE() TDONE()

TEST(gap7_sssg_null_cache) {
    SGREQ();
    WORD out = 0x1234;
    HRESULT hr = sg(d.dc, NULL, &r.psa, TAG('l','a','t','n'), 0, TAG('s','m','c','p'), 0, r.glyphs[0], &out);
    EXPECT_EQ((DWORD)hr, G7_E_INVALIDARG);
    EXPECT_EQ(out, 0x1234);
    SGDONE();
}
TEST(gap7_sssg_null_output) {
    SGREQ();
    HRESULT hr = sg(d.dc, &r.sc, &r.psa, TAG('l','a','t','n'), 0, TAG('s','m','c','p'), 0, r.glyphs[0], NULL);
    EXPECT_EQ((DWORD)hr, G7_E_INVALIDARG);
    SGDONE();
}
TEST(gap7_sssg_null_cache_and_output) {
    SGREQ();
    EXPECT_EQ((DWORD)sg(d.dc, NULL, &r.psa, 0, 0, 0, 0, 0, NULL), G7_E_INVALIDARG);
    EXPECT_EQ((DWORD)sg(NULL, NULL, NULL, 0, 0, 0, 0, 0, NULL), G7_E_INVALIDARG);
    SGDONE();
}
TEST(gap7_sssg_args_checked_before_font_work) {
    SGREQ();
    /* NULL cache/output are rejected whatever the other (even absurd) arguments are */
    WORD out;
    static const OPENTYPE_TAG tags[] = { 0, 0xFFFFFFFFu, TAG('x','x','x','x'), TAG('l','i','g','a') };
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ((DWORD)sg(d.dc, NULL, &r.psa, tags[i], tags[i], tags[i], -1, 0xFFFF, &out), G7_E_INVALIDARG);
        EXPECT_EQ((DWORD)sg(d.dc, &r.sc, &r.psa, tags[i], tags[i], tags[i], -1, 0xFFFF, NULL), G7_E_INVALIDARG);
    }
    SGDONE();
}
TEST(gap7_sssg_no_dc_empty_cache_fails) {
    sssg_t sg = SSG(); REQUIRE(sg);
    HRESULT (WINAPI *fc)(SCRIPT_CACHE *) = (void *)g7_usp_proc("ScriptFreeCache");
    SCRIPT_ANALYSIS psa; memset(&psa, 0, sizeof psa);
    SCRIPT_CACHE sc = NULL; WORD out = 0x4321;
    HRESULT hr = sg(NULL, &sc, &psa, TAG('l','a','t','n'), 0, TAG('l','i','g','a'), 0, 5, &out);
    CHECKF(FAILED(hr), "hr=%lx", (unsigned long)hr);
    t_note("ScriptSubstituteSingleGlyph(NULL hdc, empty cache) -> %lx", (unsigned long)hr);
    if (fc) fc(&sc);
}
TEST(gap7_sssg_bogus_dc_fails) {
    sssg_t sg = SSG(); REQUIRE(sg);
    HRESULT (WINAPI *fc)(SCRIPT_CACHE *) = (void *)g7_usp_proc("ScriptFreeCache");
    SCRIPT_ANALYSIS psa; memset(&psa, 0, sizeof psa);
    SCRIPT_CACHE sc = NULL; WORD out = 0x4321;
    HRESULT hr = sg((HDC)(ULONG_PTR)0x0BADBAD0, &sc, &psa, TAG('l','a','t','n'), 0, TAG('l','i','g','a'), 0, 5, &out);
    CHECKF(FAILED(hr), "hr=%lx", (unsigned long)hr);
    if (fc) fc(&sc);
}
static void sg_probe(const char *what, DWORD feature) {
    sssg_t sg = SSG(); if (!sg) { t_skip("missing"); return; }
    G7DC d; G7RUN r;
    if (!g7_dc_create(&d, 64, 64, L"Arial", 20)) { t_skip("dc"); return; }
    if (!g7_run_build(d.dc, &r, L"Aa1")) { g7_dc_destroy(&d); t_skip("shape"); return; }
    for (int i = 0; i < r.ng; i++) {
        WORD out = 0xABCD;
        HRESULT hr = sg(d.dc, &r.sc, &r.psa, TAG('l','a','t','n'), 0, feature, 0, r.glyphs[i], &out);
        t_note("%s glyph %u -> hr=%lx out=%u", what, r.glyphs[i], (unsigned long)hr, out);
        if (SUCCEEDED(hr) && hr == 0) SOFT(out != 0xABCD, "S_OK but output not written");
        if (FAILED(hr)) CHECKF(out == 0xABCD || out == 0, "failure wrote %u", out);
    }
    g7_run_free(&r); g7_dc_destroy(&d);
}
TEST(gap7_sssg_probe_smcp) { sg_probe("smcp", TAG('s','m','c','p')); }
TEST(gap7_sssg_probe_liga) { sg_probe("liga", TAG('l','i','g','a')); }
TEST(gap7_sssg_probe_unknown_feature) { sg_probe("zzzz", TAG('z','z','z','z')); }
TEST(gap7_sssg_probe_zero_feature) { sg_probe("0", 0); }
TEST(gap7_sssg_unknown_feature_never_succeeds_with_change) {
    SGREQ();
    WORD out = 0xABCD;
    HRESULT hr = sg(d.dc, &r.sc, &r.psa, TAG('l','a','t','n'), 0, TAG('z','z','z','z'), 0, r.glyphs[0], &out);
    /* a feature the font cannot have must not report a substitution */
    if (hr == 0) SOFT(out == r.glyphs[0], "unknown feature 'zzzz' changed glyph %u -> %u", r.glyphs[0], out);
    t_note("feature zzzz: hr=%lx", (unsigned long)hr);
    SGDONE();
}
TEST(gap7_sssg_repeat_stable) {
    SGREQ();
    WORD o0 = 0xABCD; HRESULT h0 = sg(d.dc, &r.sc, &r.psa, TAG('l','a','t','n'), 0, TAG('l','i','g','a'), 0, r.glyphs[0], &o0);
    int bad = 0;
    for (int i = 0; i < 50; i++) { WORD o = 0xABCD; HRESULT h = sg(d.dc, &r.sc, &r.psa, TAG('l','a','t','n'), 0, TAG('l','i','g','a'), 0, r.glyphs[0], &o); bad += h != h0 || (SUCCEEDED(h) && o != o0); }
    CHECK(bad == 0);
    SGDONE();
}
TEST(gap7_sssg_does_not_free_cache) {
    SGREQ();
    sg(d.dc, &r.sc, &r.psa, TAG('l','a','t','n'), 0, TAG('l','i','g','a'), 0, r.glyphs[0], &(WORD){0});
    CHECK(r.sc != NULL);
    /* the cache is still usable for layout afterwards */
    draw(STO(), &d, &r, 5, 5, 0, NULL);
    CHECK(g7_dc_ink(&d, NULL) > 0);
    SGDONE();
}
TEST(gap7_sssg_no_gdi_leak) {
    SGREQ();
    int before = g7_gui();
    for (int i = 0; i < 30; i++) { WORD o; sg(d.dc, &r.sc, &r.psa, TAG('l','a','t','n'), 0, TAG('l','i','g','a'), 0, r.glyphs[0], &o); }
    EXPECT_EQ(g7_gui(), before);
    SGDONE();
}

/* ================= ftsWordBreak ================= */
/* ftsWordBreak(const WCHAR *text, int cch, char *runLengths, int cbRunLengths, int charset)
 * Always returns 1. On success of the internal SSA_BREAK analysis it stores, as bytes, the distance between
 * consecutive soft-break opportunities, then a terminating 0 (never writing more than cbRunLengths bytes). */
typedef int (WINAPI *fts_t)(const void *, int, char *, int, int);
#define FTS() ((fts_t)t_proc("ftsWordBreak"))

/* guard-byte harness: returns number of leading bytes written (to NUL), or -1 if untouched */
typedef struct { BYTE pre[8]; char buf[64]; BYTE post[8]; } FTSBUF;
static void fts_init(FTSBUF *b) { memset(b, 0x7F, sizeof *b); }
static int fts_guard_ok(const FTSBUF *b, int cb) {
    for (int i = 0; i < 8; i++) if (b->pre[i] != 0x7F || b->post[i] != 0x7F) return 0;
    for (int i = cb; i < 64; i++) if ((BYTE)b->buf[i] != 0x7F) return 0;
    return 1;
}
static int fts_written(const FTSBUF *b, int cb) {
    for (int i = 0; i < cb; i++) if ((BYTE)b->buf[i] != 0x7F) return 1;
    return 0;
}
static void fts_case(const WCHAR *s, int cch, int cb, const char *expect, int nexp) {
    fts_t f = FTS(); if (!f) { t_skip("missing ftsWordBreak"); return; }
    FTSBUF b; fts_init(&b);
    int rc = f(s, cch, b.buf, cb, -1);
    EXPECT_EQ(rc, 1);
    CHECK(fts_guard_ok(&b, cb > 0 ? cb : 0));
    if (cb > 0 && fts_written(&b, cb)) {
        int n = 0, sum = 0;
        while (n < cb && b.buf[n] != 0) { CHECKF((BYTE)b.buf[n] >= 1, "zero-length run"); sum += (BYTE)b.buf[n]; n++; }
        CHECKF(n < cb, "no terminator within %d bytes", cb);
        CHECKF(sum <= cch - 1 || cch <= 1, "runs sum %d for %d chars", sum, cch);
        if (nexp >= 0 && (n != nexp || memcmp(b.buf, expect, nexp))) {
            char tmp[200]; int o = 0; for (int i = 0; i < n && o < 180; i++) o += sprintf(tmp + o, "%d ", (BYTE)b.buf[i]);
            t_note("ftsWordBreak(%d chars, cb %d) -> runs: %s(expected %d runs)", cch, cb, tmp, nexp);
        }
    } else {
        t_note("ftsWordBreak(%d chars, cb %d): output untouched", cch, cb);
    }
}
TEST(gap7_fts_exported) {
    if (g7_is_wine()) { t_skip("wine: no such export"); return; }
    CHECK(t_proc_gdi32("ftsWordBreak") != NULL);
    CHECK(t_proc_gdi32full("ftsWordBreak") != NULL);
}
TEST(gap7_fts_zero_or_negative_size_returns_one_untouched) {
    fts_t f = FTS(); REQUIRE(f);
    static const int cbs[] = { 0, -1, -100, INT_MIN };
    for (int i = 0; i < 4; i++) {
        FTSBUF b; fts_init(&b);
        EXPECT_EQ(f(L"hello world", 11, b.buf, cbs[i], -1), 1);
        CHECK(fts_guard_ok(&b, 0));
    }
}
TEST(gap7_fts_zero_size_null_buffer) {
    fts_t f = FTS(); REQUIRE(f);
    /* the size guard comes first: a NULL buffer is never touched when cb <= 0 */
    EXPECT_EQ(f(L"hello world", 11, NULL, 0, -1), 1);
    EXPECT_EQ(f(NULL, 0, NULL, 0, -1), 1);
    EXPECT_EQ(f(NULL, 5, NULL, -3, -1), 1);
}
TEST(gap7_fts_two_words) { static const char e[] = { 6 }; fts_case(L"hello world", 11, 32, e, 1); }
TEST(gap7_fts_three_words) { static const char e[] = { 2, 2 }; fts_case(L"a b c", 5, 32, e, 2); }
TEST(gap7_fts_one_word) { fts_case(L"hello", 5, 32, NULL, 0); }
TEST(gap7_fts_single_char) { fts_case(L"x", 1, 32, NULL, 0); }
TEST(gap7_fts_empty) { fts_case(L"", 0, 32, NULL, -1); }
TEST(gap7_fts_buffer_size_1) { fts_case(L"one two three", 13, 1, NULL, -1); }
TEST(gap7_fts_buffer_size_2) { static const char e[] = { 4 }; fts_case(L"one two three", 13, 2, e, 1); }
TEST(gap7_fts_buffer_size_exact) { static const char e[] = { 4, 4 }; fts_case(L"one two three", 13, 3, e, 2); }
TEST(gap7_fts_buffer_size_exact_minus_1) { static const char e[] = { 4 }; fts_case(L"one two three", 13, 2, e, 1); }
TEST(gap7_fts_buffer_larger) { static const char e[] = { 4, 4 }; fts_case(L"one two three", 13, 60, e, 2); }
TEST(gap7_fts_many_words_truncated) { fts_case(L"a b c d e f g h i j k l m n o p", 31, 5, NULL, -1); }
TEST(gap7_fts_many_words_full) { fts_case(L"a b c d e f g h i j k l m n o p", 31, 60, NULL, -1); }
TEST(gap7_fts_punctuation) { fts_case(L"foo, bar; baz.", 14, 32, NULL, -1); }
TEST(gap7_fts_multiple_spaces) { fts_case(L"a   b", 5, 32, NULL, -1); }
TEST(gap7_fts_leading_trailing_space) { fts_case(L" ab ", 4, 32, NULL, -1); }
TEST(gap7_fts_hyphen) { fts_case(L"well-known fact", 15, 32, NULL, -1); }
TEST(gap7_fts_cjk) { fts_case(L"中文文本", 4, 32, NULL, -1); }
TEST(gap7_fts_cyrillic_words) { fts_case(L"привет мир", 10, 32, NULL, -1); }
TEST(gap7_fts_length_shorter_than_string) { static const char e[] = { 6 }; fts_case(L"hello world again", 11, 32, e, 1); }
TEST(gap7_fts_long_text) {
    static const WCHAR t[] = L"The quick brown fox jumps over the lazy dog and keeps running through the forest until night falls.";
    fts_case(t, lstrlenW(t), 64, NULL, -1);
}
TEST(gap7_fts_run_lengths_fit_in_byte_for_long_words) {
    WCHAR t[300]; for (int i = 0; i < 299; i++) t[i] = (i % 40 == 39) ? L' ' : L'a'; t[299] = 0;
    fts_case(t, 299, 64, NULL, -1);
}
TEST(gap7_fts_repeat_deterministic) {
    fts_t f = FTS(); REQUIRE(f);
    FTSBUF a, b; fts_init(&a);
    f(L"alpha beta gamma delta", 22, a.buf, 32, -1);
    int bad = 0;
    for (int i = 0; i < 100; i++) { fts_init(&b); f(L"alpha beta gamma delta", 22, b.buf, 32, -1); bad += memcmp(&a, &b, sizeof a) != 0; }
    CHECK(bad == 0);
}
TEST(gap7_fts_does_not_touch_gdi) {
    fts_t f = FTS(); REQUIRE(f);
    int before = g7_gui();
    FTSBUF b;
    for (int i = 0; i < 100; i++) { fts_init(&b); f(L"gdi leak check", 14, b.buf, 32, -1); }
    EXPECT_EQ(g7_gui(), before);
}
static void g7_fts_thr(void *a) { int *res = a; fts_t f = FTS(); FTSBUF b; fts_init(&b); *res = f(L"thread test", 11, b.buf, 16, -1) == 1 && fts_guard_ok(&b, 16); }
TEST(gap7_fts_other_thread) {
    REQUIRE(FTS());
    int res = 0; g7_run_thread(g7_fts_thr, &res);
    CHECK(res == 1);
}
TEST(gap7_fts_ansi_charset_observed) {
    fts_t f = FTS(); REQUIRE(f);
    FTSBUF b; fts_init(&b);
    int rc = f("hello big world", 15, b.buf, 32, ANSI_CHARSET);
    EXPECT_EQ(rc, 1);
    CHECK(fts_guard_ok(&b, 32));
    t_note("ANSI charset: written=%d", fts_written(&b, 32));
}
