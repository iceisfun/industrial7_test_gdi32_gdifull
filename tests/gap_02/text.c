/* ExtTextOutAImpl / ExtTextOutWImpl. Glyph shapes are never compared against fixed bitmaps; checks use
 * rectangle fills (exact), non-antialiased text colour sets, A/W identity and metafile records. */
#include "g2.h"

typedef BOOL (WINAPI *ETOA_t)(HDC, int, int, UINT, const RECT *, LPCSTR, UINT, const INT *);
typedef BOOL (WINAPI *ETOW_t)(HDC, int, int, UINT, const RECT *, LPCWSTR, UINT, const INT *);
static ETOA_t etoa(void) {
    FARPROC p = t_proc_gdi32full("ExtTextOutAImpl");
    if (!p && g2_wine()) p = t_proc("ExtTextOutA");
    return (ETOA_t)p;
}
static ETOW_t etow(void) {
    FARPROC p = t_proc_gdi32full("ExtTextOutWImpl");
    if (!p && g2_wine()) p = t_proc("ExtTextOutW");
    return (ETOW_t)p;
}
#define NEED_ETO() do { REQUIRE(etoa()); REQUIRE(etow()); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(SetBkColor)); \
    REQUIRE(U(SetTextColor)); REQUIRE(U(SetBkMode)); REQUIRE(U(CreateFontIndirectW)); REQUIRE(U(SelectObject)); } while (0)

#define WHITE_PX 0xffffff
#define BLUE_PX  G2_PX(0, 0, 255)
#define RED_PX   G2_PX(255, 0, 0)

static HFONT mkfont(const WCHAR *face, int h) {
    LOGFONTW lf; memset(&lf, 0, sizeof lf);
    lf.lfHeight = h; lf.lfWeight = FW_NORMAL; lf.lfCharSet = ANSI_CHARSET; lf.lfQuality = NONANTIALIASED_QUALITY;
    lstrcpyW(lf.lfFaceName, face);
    return U(CreateFontIndirectW)(&lf);
}
typedef struct { G2DC d; HFONT f; HGDIOBJ of; } TCtx;
static int tctx(TCtx *c, int w, int h) {
    memset(c, 0, sizeof *c);
    if (!g2_mk(&c->d, w, h)) return 0;
    g2_fill(&c->d, WHITE_PX);
    c->f = mkfont(L"Arial", -16);
    c->of = U(SelectObject)(c->d.dc, c->f);
    U(SetBkMode)(c->d.dc, TRANSPARENT);
    U(SetTextColor)(c->d.dc, RGB(255, 0, 0));
    return 1;
}
static void tfree(TCtx *c) {
    if (c->of) U(SelectObject)(c->d.dc, c->of);
    if (c->f) U(DeleteObject)(c->f);
    g2_free(&c->d);
}
/* number of pixels differing from `bg` */
static int not_bg(G2DC *d, DWORD bg) { return d->w * d->h - g2_count(d, bg); }
/* true if every pixel is one of two colours */
static int only_two(G2DC *d, DWORD a, DWORD b) {
    U(GdiFlush)();
    for (int i = 0; i < d->w * d->h; i++) { DWORD v = d->bits[i] & 0xffffff; if (v != a && v != b) return 0; }
    return 1;
}
static int emf_count(HENHMETAFILE h, DWORD type) {
    UINT n = U(GetEnhMetaFileBits)(h, 0, NULL);
    if (!n) return -1;
    BYTE *b = malloc(n);
    if (!b) return -1;
    U(GetEnhMetaFileBits)(h, n, b);
    int cnt = 0; DWORD off = 0;
    while (off + 8 <= n) {
        DWORD t = *(DWORD *)(b + off), sz = *(DWORD *)(b + off + 4);
        if (sz < 8 || off + sz > n) break;
        if (t == type) cnt++;
        off += sz;
    }
    free(b);
    return cnt;
}

/* ============================== invalid arguments ============================== */
TEST(gap2_eto_invalid_dc_w) {
    NEED_ETO(); SKIP_ON_WINE("last error differs");
    for (int i = 0; i < G2_NJUNK; i++) {
        SetLastError(0);
        BOOL r = etow()((HDC)g2_junk[i], 0, 0, 0, NULL, L"abc", 3, NULL);
        CHECKF(!r, "handle %#llx", (unsigned long long)g2_junk[i]);
        EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    }
}
TEST(gap2_eto_invalid_dc_a) {
    NEED_ETO(); SKIP_ON_WINE("last error differs");
    for (int i = 0; i < G2_NJUNK; i++) {
        SetLastError(0);
        BOOL r = etoa()((HDC)g2_junk[i], 0, 0, 0, NULL, "abc", 3, NULL);
        CHECKF(!r, "handle %#llx", (unsigned long long)g2_junk[i]);
        EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    }
}
TEST(gap2_eto_invalid_dc_with_opaque_rect) {
    NEED_ETO(); SKIP_ON_WINE("last error differs");
    RECT rc = { 0, 0, 5, 5 };
    for (int i = 0; i < G2_NJUNK; i++) {
        SetLastError(0);
        CHECKF(!etow()((HDC)g2_junk[i], 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL), "W %d", i);
        EXPECT_ERR(0, ERROR_INVALID_HANDLE);
        SetLastError(0);
        CHECKF(!etoa()((HDC)g2_junk[i], 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL), "A %d", i);
        EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    }
}
TEST(gap2_eto_stale_dc) {
    NEED_ETO();
    HDC dc = g2_stale_dc();
    CHECK(!etow()(dc, 0, 0, 0, NULL, L"abc", 3, NULL));
    CHECK(!etoa()(dc, 0, 0, 0, NULL, "abc", 3, NULL));
}
TEST(gap2_eto_non_dc_objects) {
    NEED_ETO();
    HGDIOBJ o[3] = { U(CreateSolidBrush)(0), U(CreateRectRgn)(0, 0, 1, 1), U(CreateBitmap)(1, 1, 1, 1, NULL) };
    for (int i = 0; i < 3; i++) {
        CHECKF(!etow()((HDC)o[i], 0, 0, 0, NULL, L"abc", 3, NULL), "W %d", i);
        CHECKF(!etoa()((HDC)o[i], 0, 0, 0, NULL, "abc", 3, NULL), "A %d", i);
        CHECKF(U(GetObjectType)(o[i]) != 0, "obj %d damaged", i);
        U(DeleteObject)(o[i]);
    }
}
TEST(gap2_eto_pdy_requires_dx) {
    NEED_ETO(); SKIP_ON_WINE("ETO_PDY handling differs");
    TCtx c; if (!tctx(&c, 64, 32)) { CHECK(0); return; }
    CHECK(!etow()(c.d.dc, 2, 2, ETO_PDY, NULL, L"ab", 2, NULL));
    CHECK(!etoa()(c.d.dc, 2, 2, ETO_PDY, NULL, "ab", 2, NULL));
    EXPECT_EQ(not_bg(&c.d, WHITE_PX), 0);
    tfree(&c);
}
TEST(gap2_eto_pdy_with_dx_ok) {
    NEED_ETO();
    TCtx c; if (!tctx(&c, 64, 32)) { CHECK(0); return; }
    INT dx[4] = { 10, 0, 10, 0 };
    CHECK(etow()(c.d.dc, 2, 2, ETO_PDY, NULL, L"ab", 2, dx));
    CHECK(etoa()(c.d.dc, 2, 2, ETO_PDY, NULL, "ab", 2, dx));
    tfree(&c);
}
TEST(gap2_eto_pdy_check_precedes_count) {
    NEED_ETO(); SKIP_ON_WINE("ETO_PDY handling differs");
    TCtx c; if (!tctx(&c, 32, 32)) { CHECK(0); return; }
    CHECK(!etow()(c.d.dc, 0, 0, ETO_PDY, NULL, NULL, 0, NULL));
    CHECK(!etoa()(c.d.dc, 0, 0, ETO_PDY, NULL, NULL, 0, NULL));
    CHECK(!etow()(NULL, 0, 0, ETO_PDY, NULL, NULL, 0, NULL));
    tfree(&c);
}

/* ============================== opaque rectangle (exact pixels) ============================== */
TEST(gap2_eto_opaque_rect_no_text) {
    NEED_ETO();
    TCtx c; if (!tctx(&c, 32, 32)) { CHECK(0); return; }
    U(SetBkColor)(c.d.dc, RGB(0, 0, 255));
    RECT rc = { 4, 5, 20, 17 };
    CHECK(etow()(c.d.dc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL));
    G2_EQ(g2_count(&c.d, BLUE_PX), 16 * 12);
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++) {
            int in = x >= 4 && x < 20 && y >= 5 && y < 17;
            if (g2_px(&c.d, x, y) != (in ? BLUE_PX : WHITE_PX)) { CHECKF(0, "pixel (%d,%d)", x, y); x = y = 99; }
        }
    tfree(&c);
}
TEST(gap2_eto_opaque_rect_no_text_a) {
    NEED_ETO();
    TCtx c; if (!tctx(&c, 32, 32)) { CHECK(0); return; }
    U(SetBkColor)(c.d.dc, RGB(0, 0, 255));
    RECT rc = { 4, 5, 20, 17 };
    CHECK(etoa()(c.d.dc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL));
    G2_EQ(g2_count(&c.d, BLUE_PX), 16 * 12);
    tfree(&c);
}
TEST(gap2_eto_opaque_rect_empty_string) {
    NEED_ETO();
    TCtx c; if (!tctx(&c, 32, 32)) { CHECK(0); return; }
    U(SetBkColor)(c.d.dc, RGB(0, 0, 255));
    RECT rc = { 1, 1, 9, 9 };
    CHECK(etow()(c.d.dc, 0, 0, ETO_OPAQUE, &rc, L"", 0, NULL));
    G2_EQ(g2_count(&c.d, BLUE_PX), 64);
    CHECK(etoa()(c.d.dc, 0, 0, ETO_OPAQUE, &rc, "", 0, NULL));
    G2_EQ(g2_count(&c.d, BLUE_PX), 64);
    tfree(&c);
}
TEST(gap2_eto_opaque_ignores_bk_mode) {
    NEED_ETO();
    for (int mode = TRANSPARENT; mode <= OPAQUE; mode++) {
        TCtx c; if (!tctx(&c, 16, 16)) { CHECK(0); return; }
        U(SetBkMode)(c.d.dc, mode);
        U(SetBkColor)(c.d.dc, RGB(0, 0, 255));
        RECT rc = { 2, 2, 6, 6 };
        CHECK(etow()(c.d.dc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL));
        CHECKF(g2_count(&c.d, BLUE_PX) == 16, "bk mode %d", mode);
        tfree(&c);
    }
}
TEST(gap2_eto_rect_ignored_without_flags) {
    NEED_ETO();
    TCtx c; if (!tctx(&c, 16, 16)) { CHECK(0); return; }
    U(SetBkColor)(c.d.dc, RGB(0, 0, 255));
    RECT rc = { 2, 2, 12, 12 };
    CHECK(etow()(c.d.dc, 0, 0, 0, &rc, NULL, 0, NULL));
    G2_EQ(g2_count(&c.d, WHITE_PX), 256);
    CHECK(etow()(c.d.dc, 0, 0, 0, &rc, L"", 0, NULL));
    G2_EQ(g2_count(&c.d, WHITE_PX), 256);
    tfree(&c);
}
TEST(gap2_eto_opaque_without_rect) {
    NEED_ETO();
    TCtx c; if (!tctx(&c, 16, 16)) { CHECK(0); return; }
    U(SetBkColor)(c.d.dc, RGB(0, 0, 255));
    CHECK(etow()(c.d.dc, 0, 0, ETO_OPAQUE, NULL, NULL, 0, NULL));
    G2_EQ(g2_count(&c.d, WHITE_PX), 256);
    tfree(&c);
}
TEST(gap2_eto_opaque_rect_clipped_by_clip_region) {
    NEED_ETO(); REQUIRE(U(IntersectClipRect));
    TCtx c; if (!tctx(&c, 32, 32)) { CHECK(0); return; }
    U(SetBkColor)(c.d.dc, RGB(0, 0, 255));
    U(IntersectClipRect)(c.d.dc, 8, 8, 12, 12);
    RECT rc = { 0, 0, 32, 32 };
    CHECK(etow()(c.d.dc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL));
    G2_EQ(g2_count(&c.d, BLUE_PX), 16);
    tfree(&c);
}
TEST(gap2_eto_opaque_rect_off_bitmap) {
    NEED_ETO();
    TCtx c; if (!tctx(&c, 16, 16)) { CHECK(0); return; }
    U(SetBkColor)(c.d.dc, RGB(0, 0, 255));
    RECT rc = { -20, -20, 8, 8 };
    CHECK(etow()(c.d.dc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL));
    G2_EQ(g2_count(&c.d, BLUE_PX), 64);
    RECT farr = { 1000, 1000, 2000, 2000 };
    CHECK(etow()(c.d.dc, 0, 0, ETO_OPAQUE, &farr, NULL, 0, NULL));
    G2_EQ(g2_count(&c.d, BLUE_PX), 64);
    tfree(&c);
}
TEST(gap2_eto_opaque_rect_empty_and_inverted) {
    NEED_ETO();
    TCtx c; if (!tctx(&c, 16, 16)) { CHECK(0); return; }
    U(SetBkColor)(c.d.dc, RGB(0, 0, 255));
    RECT e1 = { 4, 4, 4, 9 }, e2 = { 4, 4, 9, 4 }, inv = { 9, 9, 4, 4 };
    BOOL r1 = etow()(c.d.dc, 0, 0, ETO_OPAQUE, &e1, NULL, 0, NULL);
    BOOL r2 = etow()(c.d.dc, 0, 0, ETO_OPAQUE, &e2, NULL, 0, NULL);
    BOOL r3 = etow()(c.d.dc, 0, 0, ETO_OPAQUE, &inv, NULL, 0, NULL);
    t_note("empty/inverted opaque rect returns %d %d %d, %d blue px", r1, r2, r3, g2_count(&c.d, BLUE_PX));
    CHECK(g2_count(&c.d, BLUE_PX) <= 25);
    tfree(&c);
}
TEST(gap2_eto_opaque_bk_colors_exact) {
    NEED_ETO();
    static const COLORREF cs[] = { RGB(0, 0, 0), RGB(255, 255, 255), RGB(1, 2, 3), RGB(250, 128, 7), RGB(0, 255, 0) };
    for (unsigned i = 0; i < sizeof cs / sizeof cs[0]; i++) {
        TCtx c; if (!tctx(&c, 16, 16)) { CHECK(0); return; }
        g2_fill(&c.d, G2_PX(77, 88, 99));
        U(SetBkColor)(c.d.dc, cs[i]);
        RECT rc = { 0, 0, 16, 16 };
        CHECK(etow()(c.d.dc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL));
        DWORD want = G2_PX(GetRValue(cs[i]), GetGValue(cs[i]), GetBValue(cs[i]));
        CHECKF(g2_count(&c.d, want) == 256, "color %u", i);
        tfree(&c);
    }
}
TEST(gap2_eto_opaque_mapping_scale) {
    NEED_ETO(); REQUIRE(U(SetMapMode)); REQUIRE(U(SetWindowExtEx)); REQUIRE(U(SetViewportExtEx));
    TCtx c; if (!tctx(&c, 32, 32)) { CHECK(0); return; }
    U(SetMapMode)(c.d.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(c.d.dc, 1, 1, NULL); U(SetViewportExtEx)(c.d.dc, 2, 2, NULL);
    U(SetBkColor)(c.d.dc, RGB(0, 0, 255));
    RECT rc = { 1, 1, 3, 3 };
    CHECK(etow()(c.d.dc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL));
    G2_EQ(g2_count(&c.d, BLUE_PX), 16);
    CHECK(g2_px(&c.d, 2, 2) == BLUE_PX && g2_px(&c.d, 5, 5) == BLUE_PX && g2_px(&c.d, 6, 6) == WHITE_PX && g2_px(&c.d, 1, 1) == WHITE_PX);
    tfree(&c);
}
TEST(gap2_eto_opaque_viewport_origin) {
    NEED_ETO(); REQUIRE(U(SetViewportOrgEx));
    TCtx c; if (!tctx(&c, 32, 32)) { CHECK(0); return; }
    U(SetViewportOrgEx)(c.d.dc, 10, 12, NULL);
    U(SetBkColor)(c.d.dc, RGB(0, 0, 255));
    RECT rc = { 0, 0, 5, 3 };
    CHECK(etow()(c.d.dc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL));
    G2_EQ(g2_count(&c.d, BLUE_PX), 15);
    CHECK(g2_px(&c.d, 10, 12) == BLUE_PX && g2_px(&c.d, 14, 14) == BLUE_PX && g2_px(&c.d, 15, 14) == WHITE_PX);
    tfree(&c);
}
TEST(gap2_eto_opaque_a_w_identical_bitmaps) {
    NEED_ETO();
    TCtx a, b; if (!tctx(&a, 64, 24) || !tctx(&b, 64, 24)) { CHECK(0); return; }
    U(SetBkColor)(a.d.dc, RGB(0, 0, 255)); U(SetBkColor)(b.d.dc, RGB(0, 0, 255));
    RECT rc = { 2, 2, 60, 20 };
    CHECK(etoa()(a.d.dc, 4, 3, ETO_OPAQUE | ETO_CLIPPED, &rc, "Hello 123", 9, NULL));
    CHECK(etow()(b.d.dc, 4, 3, ETO_OPAQUE | ETO_CLIPPED, &rc, L"Hello 123", 9, NULL));
    CHECK(!memcmp(a.d.bits, b.d.bits, 64 * 24 * 4));
    CHECK(not_bg(&a.d, WHITE_PX) >= 58 * 18);
    tfree(&a); tfree(&b);
}

/* ============================== text output (non antialiased) ============================== */
TEST(gap2_eto_text_draws_only_text_colour) {
    NEED_ETO();
    TCtx c; if (!tctx(&c, 96, 32)) { CHECK(0); return; }
    CHECK(etow()(c.d.dc, 2, 2, 0, NULL, L"Hello", 5, NULL));
    CHECK(not_bg(&c.d, WHITE_PX) > 0);
    CHECK(only_two(&c.d, WHITE_PX, RED_PX));
    tfree(&c);
}
TEST(gap2_eto_text_a_equals_w) {
    NEED_ETO();
    static const char *const s[] = { "Hello", "A", "The quick brown fox", "0123456789", "  x  ", "~!@#$%^&*()" };
    for (unsigned i = 0; i < sizeof s / sizeof s[0]; i++) {
        TCtx a, b; if (!tctx(&a, 200, 32) || !tctx(&b, 200, 32)) { CHECK(0); return; }
        WCHAR w[64]; MultiByteToWideChar(CP_ACP, 0, s[i], -1, w, 64);
        UINT n = (UINT)strlen(s[i]);
        CHECK(etoa()(a.d.dc, 3, 4, 0, NULL, s[i], n, NULL));
        CHECK(etow()(b.d.dc, 3, 4, 0, NULL, w, n, NULL));
        CHECKF(!memcmp(a.d.bits, b.d.bits, 200 * 32 * 4), "string %u", i);
        tfree(&a); tfree(&b);
    }
}
TEST(gap2_eto_text_deterministic_repeat) {
    NEED_ETO();
    TCtx a, b; if (!tctx(&a, 96, 32) || !tctx(&b, 96, 32)) { CHECK(0); return; }
    for (int k = 0; k < 3; k++) { etow()(a.d.dc, 5, 5, 0, NULL, L"Repeat", 6, NULL); }
    etow()(b.d.dc, 5, 5, 0, NULL, L"Repeat", 6, NULL);
    CHECK(!memcmp(a.d.bits, b.d.bits, 96 * 32 * 4));   /* opaque glyph pixels: drawing twice changes nothing */
    tfree(&a); tfree(&b);
}
TEST(gap2_eto_zero_count_draws_nothing) {
    NEED_ETO();
    TCtx c; if (!tctx(&c, 32, 32)) { CHECK(0); return; }
    CHECK(etow()(c.d.dc, 1, 1, 0, NULL, L"abc", 0, NULL));
    CHECK(etoa()(c.d.dc, 1, 1, 0, NULL, "abc", 0, NULL));
    CHECK(etow()(c.d.dc, 1, 1, 0, NULL, NULL, 0, NULL));
    CHECK(etoa()(c.d.dc, 1, 1, 0, NULL, NULL, 0, NULL));
    G2_EQ(not_bg(&c.d, WHITE_PX), 0);
    tfree(&c);
}
TEST(gap2_eto_count_limits_characters) {
    NEED_ETO();
    TCtx a, b; if (!tctx(&a, 160, 32) || !tctx(&b, 160, 32)) { CHECK(0); return; }
    etow()(a.d.dc, 2, 2, 0, NULL, L"ABCDEFGH", 4, NULL);
    etow()(b.d.dc, 2, 2, 0, NULL, L"ABCD", 4, NULL);
    CHECK(!memcmp(a.d.bits, b.d.bits, 160 * 32 * 4));
    tfree(&a); tfree(&b);
}
TEST(gap2_eto_single_char_counts) {
    NEED_ETO();
    for (UINT n = 1; n <= 6; n++) {
        TCtx a, b; if (!tctx(&a, 160, 32) || !tctx(&b, 160, 32)) { CHECK(0); return; }
        etow()(a.d.dc, 2, 2, 0, NULL, L"mnbvcx", n, NULL);
        WCHAR t[8] = L""; memcpy(t, L"mnbvcx", n * sizeof(WCHAR));
        etow()(b.d.dc, 2, 2, 0, NULL, t, n, NULL);
        CHECKF(!memcmp(a.d.bits, b.d.bits, 160 * 32 * 4), "count %u", n);
        tfree(&a); tfree(&b);
    }
}
TEST(gap2_eto_clipped_rect_confines_text) {
    NEED_ETO();
    TCtx c; if (!tctx(&c, 96, 32)) { CHECK(0); return; }
    RECT rc = { 10, 4, 30, 20 };
    CHECK(etow()(c.d.dc, 2, 2, ETO_CLIPPED, &rc, L"WWWWWWWWWW", 10, NULL));
    int out = 0;
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 96; x++)
            if (!(x >= 10 && x < 30 && y >= 4 && y < 20) && g2_px(&c.d, x, y) != WHITE_PX) out++;
    G2_EQ(out, 0);
    int in = 0;
    for (int y = 4; y < 20; y++) for (int x = 10; x < 30; x++) if (g2_px(&c.d, x, y) == RED_PX) in++;
    CHECK(in > 0);
    tfree(&c);
}
TEST(gap2_eto_unclipped_rect_does_not_confine_text) {
    NEED_ETO();
    TCtx c; if (!tctx(&c, 96, 32)) { CHECK(0); return; }
    RECT rc = { 10, 4, 30, 20 };
    CHECK(etow()(c.d.dc, 2, 2, ETO_OPAQUE, &rc, L"WWWWWWWWWW", 10, NULL));   /* opaque only: text runs over the rect edge */
    int out = 0;
    for (int y = 0; y < 32; y++) for (int x = 30; x < 96; x++) if (g2_px(&c.d, x, y) == RED_PX) out++;
    CHECK(out > 0);
    tfree(&c);
}
TEST(gap2_eto_clipped_a_equals_w) {
    NEED_ETO();
    TCtx a, b; if (!tctx(&a, 96, 32) || !tctx(&b, 96, 32)) { CHECK(0); return; }
    RECT rc = { 6, 3, 40, 25 };
    U(SetBkColor)(a.d.dc, RGB(0, 0, 255)); U(SetBkColor)(b.d.dc, RGB(0, 0, 255));
    etoa()(a.d.dc, 2, 2, ETO_CLIPPED | ETO_OPAQUE, &rc, "Clipping", 8, NULL);
    etow()(b.d.dc, 2, 2, ETO_CLIPPED | ETO_OPAQUE, &rc, L"Clipping", 8, NULL);
    CHECK(!memcmp(a.d.bits, b.d.bits, 96 * 32 * 4));
    tfree(&a); tfree(&b);
}
TEST(gap2_eto_clipped_empty_rect_draws_nothing) {
    NEED_ETO();
    TCtx c; if (!tctx(&c, 64, 32)) { CHECK(0); return; }
    RECT rc = { 10, 10, 10, 20 };
    BOOL r = etow()(c.d.dc, 2, 2, ETO_CLIPPED, &rc, L"WWWW", 4, NULL);
    t_note("ETO_CLIPPED with empty rect -> %d", r);
    G2_EQ(not_bg(&c.d, WHITE_PX), 0);
    tfree(&c);
}
TEST(gap2_eto_bk_mode_opaque_fills_cells) {
    NEED_ETO();
    TCtx t, o; if (!tctx(&t, 96, 32) || !tctx(&o, 96, 32)) { CHECK(0); return; }
    U(SetBkColor)(t.d.dc, RGB(0, 0, 255)); U(SetBkColor)(o.d.dc, RGB(0, 0, 255));
    U(SetBkMode)(o.d.dc, OPAQUE);
    etow()(t.d.dc, 4, 4, 0, NULL, L"Cell", 4, NULL);
    etow()(o.d.dc, 4, 4, 0, NULL, L"Cell", 4, NULL);
    G2_EQ(g2_count(&t.d, BLUE_PX), 0);
    CHECK(g2_count(&o.d, BLUE_PX) > 100);
    tfree(&t); tfree(&o);
}
TEST(gap2_eto_cell_extent_left_aligned) {
    NEED_ETO(); REQUIRE(U(GetTextExtentPoint32W)); REQUIRE(U(GetTextMetricsW)); REQUIRE(U(SetTextAlign));
    TCtx c; if (!tctx(&c, 160, 40)) { CHECK(0); return; }
    U(SetBkMode)(c.d.dc, OPAQUE); U(SetBkColor)(c.d.dc, RGB(0, 0, 255));
    U(SetTextAlign)(c.d.dc, TA_LEFT | TA_TOP);
    SIZE sz; U(GetTextExtentPoint32W)(c.d.dc, L"Extent", 6, &sz);
    TEXTMETRICW tm; U(GetTextMetricsW)(c.d.dc, &tm);
    CHECK(etow()(c.d.dc, 10, 6, 0, NULL, L"Extent", 6, NULL));
    int minx = 999, maxx = -1, miny = 999, maxy = -1;
    for (int y = 0; y < 40; y++) for (int x = 0; x < 160; x++) {
        DWORD p = g2_px(&c.d, x, y);
        if (p != WHITE_PX) { if (x < minx) minx = x; if (x > maxx) maxx = x; if (y < miny) miny = y; if (y > maxy) maxy = y; }
    }
    t_note("cell [%d..%d] x [%d..%d]; extent %ld x %ld, tmHeight %ld", minx, maxx, miny, maxy, sz.cx, sz.cy, tm.tmHeight);
    CHECK(minx <= 10 && minx >= 8);
    CHECKF(maxx - minx + 1 >= sz.cx - 1 && maxx - minx + 1 <= sz.cx + 3, "width %d vs %ld", maxx - minx + 1, sz.cx);
    CHECKF(miny == 6, "top %d", miny);
    CHECKF(maxy - miny + 1 >= tm.tmHeight - 1 && maxy - miny + 1 <= tm.tmHeight + 1, "height %d vs %ld", maxy - miny + 1, tm.tmHeight);
    tfree(&c);
}
TEST(gap2_eto_cell_extent_right_aligned) {
    NEED_ETO(); REQUIRE(U(GetTextExtentPoint32W)); REQUIRE(U(SetTextAlign));
    TCtx c; if (!tctx(&c, 160, 40)) { CHECK(0); return; }
    U(SetBkMode)(c.d.dc, OPAQUE); U(SetBkColor)(c.d.dc, RGB(0, 0, 255));
    U(SetTextAlign)(c.d.dc, TA_RIGHT | TA_TOP);
    SIZE sz; U(GetTextExtentPoint32W)(c.d.dc, L"Extent", 6, &sz);
    CHECK(etow()(c.d.dc, 120, 6, 0, NULL, L"Extent", 6, NULL));
    int minx = 999, maxx = -1;
    for (int y = 0; y < 40; y++) for (int x = 0; x < 160; x++) if (g2_px(&c.d, x, y) != WHITE_PX) { if (x < minx) minx = x; if (x > maxx) maxx = x; }
    t_note("right-aligned cell [%d..%d], extent %ld", minx, maxx, sz.cx);
    CHECK(maxx <= 121 && maxx >= 118);
    CHECK(minx >= 120 - sz.cx - 3 && minx <= 120 - sz.cx + 1);
    tfree(&c);
}
TEST(gap2_eto_cell_extent_centered) {
    NEED_ETO(); REQUIRE(U(GetTextExtentPoint32W)); REQUIRE(U(SetTextAlign));
    TCtx c; if (!tctx(&c, 160, 40)) { CHECK(0); return; }
    U(SetBkMode)(c.d.dc, OPAQUE); U(SetBkColor)(c.d.dc, RGB(0, 0, 255));
    U(SetTextAlign)(c.d.dc, TA_CENTER | TA_TOP);
    SIZE sz; U(GetTextExtentPoint32W)(c.d.dc, L"Centered", 8, &sz);
    CHECK(etow()(c.d.dc, 80, 6, 0, NULL, L"Centered", 8, NULL));
    int minx = 999, maxx = -1;
    for (int y = 0; y < 40; y++) for (int x = 0; x < 160; x++) if (g2_px(&c.d, x, y) != WHITE_PX) { if (x < minx) minx = x; if (x > maxx) maxx = x; }
    int mid2 = minx + maxx + 1;   /* twice the centre */
    t_note("centred cell [%d..%d], extent %ld", minx, maxx, sz.cx);
    CHECK(mid2 >= 158 && mid2 <= 162);
    tfree(&c);
}
TEST(gap2_eto_vertical_alignments) {
    NEED_ETO(); REQUIRE(U(GetTextMetricsW)); REQUIRE(U(SetTextAlign));
    static const UINT al[] = { TA_TOP, TA_BOTTOM, TA_BASELINE };
    int tops[3];
    for (int i = 0; i < 3; i++) {
        TCtx c; if (!tctx(&c, 96, 64)) { CHECK(0); return; }
        U(SetBkMode)(c.d.dc, OPAQUE); U(SetBkColor)(c.d.dc, RGB(0, 0, 255));
        U(SetTextAlign)(c.d.dc, TA_LEFT | al[i]);
        TEXTMETRICW tm; U(GetTextMetricsW)(c.d.dc, &tm);
        CHECK(etow()(c.d.dc, 10, 30, 0, NULL, L"Vy", 2, NULL));
        int miny = 999, maxy = -1;
        for (int y = 0; y < 64; y++) for (int x = 0; x < 96; x++) if (g2_px(&c.d, x, y) != WHITE_PX) { if (y < miny) miny = y; if (y > maxy) maxy = y; }
        tops[i] = miny;
        int want_top = al[i] == TA_TOP ? 30 : al[i] == TA_BOTTOM ? 30 - tm.tmHeight : 30 - tm.tmAscent;
        CHECKF(miny >= want_top - 1 && miny <= want_top + 1, "align %u: top %d want ~%d", al[i], miny, want_top);
        tfree(&c);
    }
    CHECK(tops[0] > tops[1] || tops[0] > tops[2]);
}

/* ============================== TA_UPDATECP / spacing ============================== */
TEST(gap2_eto_updatecp_advances_by_extent) {
    NEED_ETO(); REQUIRE(U(SetTextAlign)); REQUIRE(U(GetCurrentPositionEx)); REQUIRE(U(MoveToEx)); REQUIRE(U(GetTextExtentPoint32W));
    TCtx c; if (!tctx(&c, 160, 40)) { CHECK(0); return; }
    U(SetTextAlign)(c.d.dc, TA_LEFT | TA_TOP | TA_UPDATECP);
    U(MoveToEx)(c.d.dc, 5, 7, NULL);
    SIZE sz; U(GetTextExtentPoint32W)(c.d.dc, L"abcdef", 6, &sz);
    CHECK(etow()(c.d.dc, 99, 99, 0, NULL, L"abcdef", 6, NULL));   /* x,y are ignored with TA_UPDATECP */
    POINT p; U(GetCurrentPositionEx)(c.d.dc, &p);
    CHECKF(p.x == 5 + sz.cx && p.y == 7, "cp (%ld,%ld) extent %ld", p.x, p.y, sz.cx);
    tfree(&c);
}
TEST(gap2_eto_updatecp_a_equals_w) {
    NEED_ETO(); REQUIRE(U(SetTextAlign)); REQUIRE(U(GetCurrentPositionEx)); REQUIRE(U(MoveToEx));
    TCtx a, b; if (!tctx(&a, 160, 40) || !tctx(&b, 160, 40)) { CHECK(0); return; }
    U(SetTextAlign)(a.d.dc, TA_UPDATECP); U(SetTextAlign)(b.d.dc, TA_UPDATECP);
    U(MoveToEx)(a.d.dc, 3, 3, NULL); U(MoveToEx)(b.d.dc, 3, 3, NULL);
    etoa()(a.d.dc, 0, 0, 0, NULL, "abc", 3, NULL); etoa()(a.d.dc, 0, 0, 0, NULL, "def", 3, NULL);
    etow()(b.d.dc, 0, 0, 0, NULL, L"abc", 3, NULL); etow()(b.d.dc, 0, 0, 0, NULL, L"def", 3, NULL);
    POINT pa, pb; U(GetCurrentPositionEx)(a.d.dc, &pa); U(GetCurrentPositionEx)(b.d.dc, &pb);
    CHECK(pa.x == pb.x && pa.y == pb.y);
    CHECK(!memcmp(a.d.bits, b.d.bits, 160 * 40 * 4));
    tfree(&a); tfree(&b);
}
TEST(gap2_eto_updatecp_with_dx_sums) {
    NEED_ETO(); REQUIRE(U(SetTextAlign)); REQUIRE(U(GetCurrentPositionEx)); REQUIRE(U(MoveToEx));
    TCtx c; if (!tctx(&c, 160, 40)) { CHECK(0); return; }
    U(SetTextAlign)(c.d.dc, TA_UPDATECP);
    static const INT dx[] = { 10, 20, 7, 3 };
    U(MoveToEx)(c.d.dc, 4, 9, NULL);
    CHECK(etow()(c.d.dc, 0, 0, 0, NULL, L"wxyz", 4, dx));
    POINT p; U(GetCurrentPositionEx)(c.d.dc, &p);
    CHECKF(p.x == 4 + 40 && p.y == 9, "cp (%ld,%ld)", p.x, p.y);
    U(MoveToEx)(c.d.dc, 4, 9, NULL);
    CHECK(etoa()(c.d.dc, 0, 0, 0, NULL, "wxyz", 4, dx));
    U(GetCurrentPositionEx)(c.d.dc, &p);
    CHECKF(p.x == 4 + 40 && p.y == 9, "A cp (%ld,%ld)", p.x, p.y);
    tfree(&c);
}
TEST(gap2_eto_dx_spacing_moves_glyphs) {
    NEED_ETO();
    TCtx a, b; if (!tctx(&a, 160, 40) || !tctx(&b, 160, 40)) { CHECK(0); return; }
    static const INT wide[] = { 30, 30 };
    etow()(a.d.dc, 5, 5, 0, NULL, L"II", 2, wide);
    etow()(b.d.dc, 5, 5, 0, NULL, L"II", 2, NULL);
    CHECK(memcmp(a.d.bits, b.d.bits, 160 * 40 * 4) != 0);
    /* with 30px advance the second glyph begins well to the right of the natural position */
    int ra = 0, rb = 0;
    for (int y = 0; y < 40; y++) for (int x = 0; x < 160; x++) {
        if (g2_px(&a.d, x, y) != WHITE_PX && x > ra) ra = x;
        if (g2_px(&b.d, x, y) != WHITE_PX && x > rb) rb = x;
    }
    CHECKF(ra > rb + 15, "right edges %d vs %d", ra, rb);
    tfree(&a); tfree(&b);
}
TEST(gap2_eto_dx_zero_overstrike) {
    NEED_ETO();
    TCtx a, b; if (!tctx(&a, 96, 40) || !tctx(&b, 96, 40)) { CHECK(0); return; }
    static const INT zero[] = { 0, 0, 0 };
    etow()(a.d.dc, 5, 5, 0, NULL, L"III", 3, zero);   /* three glyphs on the same spot */
    etow()(b.d.dc, 5, 5, 0, NULL, L"I", 1, NULL);
    CHECK(!memcmp(a.d.bits, b.d.bits, 96 * 40 * 4));
    tfree(&a); tfree(&b);
}
TEST(gap2_eto_pdy_vertical_offsets) {
    NEED_ETO(); REQUIRE(U(SetTextAlign)); REQUIRE(U(GetCurrentPositionEx)); REQUIRE(U(MoveToEx));
    TCtx c; if (!tctx(&c, 160, 60)) { CHECK(0); return; }
    U(SetTextAlign)(c.d.dc, TA_UPDATECP);
    static const INT dx[] = { 10, 0, 10, 0 };
    U(MoveToEx)(c.d.dc, 4, 9, NULL);
    CHECK(etow()(c.d.dc, 0, 0, ETO_PDY, NULL, L"ab", 2, dx));
    POINT p; U(GetCurrentPositionEx)(c.d.dc, &p);
    t_note("ETO_PDY cp after (4,9): (%ld,%ld)", p.x, p.y);
    CHECK(p.x == 24);
    tfree(&c);
}

/* ============================== glyph indices ============================== */
TEST(gap2_eto_glyph_index_matches_text) {
    NEED_ETO(); REQUIRE(U(GetGlyphIndicesW));
    TCtx a, b; if (!tctx(&a, 160, 40) || !tctx(&b, 160, 40)) { CHECK(0); return; }
    static const WCHAR s[] = L"Glyphs";
    WORD gi[8];
    DWORD n = U(GetGlyphIndicesW)(a.d.dc, s, 6, gi, GGI_MARK_NONEXISTING_GLYPHS);
    if (n != 6) { t_skip("GetGlyphIndicesW unsupported (%lu)", n); tfree(&a); tfree(&b); return; }
    CHECK(etow()(a.d.dc, 5, 5, 0, NULL, s, 6, NULL));
    CHECK(etow()(b.d.dc, 5, 5, ETO_GLYPH_INDEX, NULL, (LPCWSTR)gi, 6, NULL));
    CHECK(!memcmp(a.d.bits, b.d.bits, 160 * 40 * 4));
    tfree(&a); tfree(&b);
}
TEST(gap2_eto_glyph_index_a_matches_w) {
    NEED_ETO(); REQUIRE(U(GetGlyphIndicesW));
    TCtx a, b; if (!tctx(&a, 160, 40) || !tctx(&b, 160, 40)) { CHECK(0); return; }
    static const WCHAR s[] = L"Idx";
    WORD gi[4];
    if (U(GetGlyphIndicesW)(a.d.dc, s, 3, gi, 0) != 3) { t_skip("GetGlyphIndicesW unsupported"); tfree(&a); tfree(&b); return; }
    CHECK(etow()(a.d.dc, 5, 5, ETO_GLYPH_INDEX, NULL, (LPCWSTR)gi, 3, NULL));
    CHECK(etoa()(b.d.dc, 5, 5, ETO_GLYPH_INDEX, NULL, (LPCSTR)gi, 3, NULL));
    CHECK(!memcmp(a.d.bits, b.d.bits, 160 * 40 * 4));
    tfree(&a); tfree(&b);
}
TEST(gap2_eto_glyph_index_zero_is_missing_glyph) {
    NEED_ETO();
    TCtx c; if (!tctx(&c, 96, 40)) { CHECK(0); return; }
    WORD gi[2] = { 0, 0 };
    BOOL r = etow()(c.d.dc, 5, 5, ETO_GLYPH_INDEX, NULL, (LPCWSTR)gi, 2, NULL);
    t_note("glyph index 0 -> %d, %d px", r, not_bg(&c.d, WHITE_PX));
    CHECK(r);
    tfree(&c);
}

/* ============================== counts / options / odd arguments ============================== */
TEST(gap2_eto_large_counts_do_not_crash) {
    NEED_ETO();
    TCtx c; if (!tctx(&c, 64, 32)) { CHECK(0); return; }
    static const UINT counts[] = { 1, 100, 575, 576, 577, 1000, 4096, 8191, 8192, 8193, 20000, 32767, 32768, 65535 };
    WCHAR *w = malloc(65536 * sizeof(WCHAR)); char *a = malloc(65536);
    if (!w || !a) { CHECK(0); free(w); free(a); tfree(&c); return; }
    for (int i = 0; i < 65536; i++) { w[i] = L' '; a[i] = ' '; }
    for (unsigned i = 0; i < sizeof counts / sizeof counts[0]; i++) {
        BOOL rw = etow()(c.d.dc, -50000, 3, 0, NULL, w, counts[i], NULL);
        BOOL ra = etoa()(c.d.dc, -50000, 3, 0, NULL, a, counts[i], NULL);
        t_note("count %u: W=%d A=%d", counts[i], rw, ra);
        if (counts[i] <= 576) CHECKF(rw && ra, "count %u", counts[i]);
    }
    G2_EQ(not_bg(&c.d, WHITE_PX), 0);   /* only blanks, far off-screen */
    free(w); free(a); tfree(&c);
}
TEST(gap2_eto_blank_string_draws_nothing) {
    NEED_ETO();
    TCtx c; if (!tctx(&c, 64, 32)) { CHECK(0); return; }
    CHECK(etow()(c.d.dc, 2, 2, 0, NULL, L"     ", 5, NULL));
    CHECK(etoa()(c.d.dc, 2, 2, 0, NULL, "     ", 5, NULL));
    G2_EQ(not_bg(&c.d, WHITE_PX), 0);
    tfree(&c);
}
TEST(gap2_eto_extreme_coordinates) {
    NEED_ETO();
    TCtx c; if (!tctx(&c, 64, 32)) { CHECK(0); return; }
    static const int xs[] = { 0, -1, 1000000, -1000000, 0x7fffff, -0x7fffff, INT_MAX, INT_MIN };
    for (unsigned i = 0; i < sizeof xs / sizeof xs[0]; i++) {
        BOOL r1 = etow()(c.d.dc, xs[i], 5, 0, NULL, L"abc", 3, NULL);
        BOOL r2 = etow()(c.d.dc, 5, xs[i], 0, NULL, L"abc", 3, NULL);
        t_note("coord %d -> %d %d", xs[i], r1, r2);
    }
    CHECK(U(GetObjectType)(c.d.dc) == OBJ_MEMDC);
    tfree(&c);
}
TEST(gap2_eto_negative_origin_partial_glyphs) {
    NEED_ETO();
    TCtx c; if (!tctx(&c, 64, 32)) { CHECK(0); return; }
    CHECK(etow()(c.d.dc, -6, 4, 0, NULL, L"WWWW", 4, NULL));
    CHECK(not_bg(&c.d, WHITE_PX) > 0);
    CHECK(only_two(&c.d, WHITE_PX, RED_PX));
    tfree(&c);
}
TEST(gap2_eto_unknown_option_bits_observed) {
    NEED_ETO();
    static const UINT bits[] = { ETO_RTLREADING, ETO_NUMERICSLOCAL, ETO_NUMERICSLATIN, ETO_IGNORELANGUAGE, 0x0100, 0x0200, 0x0400, 0x4000, 0x8000, 0xffff0000 };
    for (unsigned i = 0; i < sizeof bits / sizeof bits[0]; i++) {
        TCtx c; if (!tctx(&c, 64, 32)) { CHECK(0); return; }
        BOOL r = etow()(c.d.dc, 2, 2, bits[i], NULL, L"abc", 3, NULL);
        t_note("option %#x -> %d (%d px)", bits[i], r, not_bg(&c.d, WHITE_PX));
        CHECKF(U(GetObjectType)(c.d.dc) == OBJ_MEMDC, "dc destroyed by option %#x", bits[i]);
        tfree(&c);
    }
}
TEST(gap2_eto_options_rect_gating) {
    NEED_ETO();
    /* the rect is honoured only together with ETO_OPAQUE and/or ETO_CLIPPED; any other flag leaves it unused */
    static const UINT flags[] = { 0, ETO_GLYPH_INDEX, ETO_RTLREADING, ETO_NUMERICSLOCAL, ETO_OPAQUE, ETO_CLIPPED, ETO_OPAQUE | ETO_CLIPPED };
    for (unsigned i = 0; i < sizeof flags / sizeof flags[0]; i++) {
        TCtx c; if (!tctx(&c, 32, 32)) { CHECK(0); return; }
        U(SetBkColor)(c.d.dc, RGB(0, 0, 255));
        RECT rc = { 4, 4, 12, 12 };
        etow()(c.d.dc, 0, 0, flags[i], &rc, NULL, 0, NULL);
        int want = (flags[i] & ETO_OPAQUE) ? 64 : 0;
        CHECKF(g2_count(&c.d, BLUE_PX) == want, "flags %#x: %d", flags[i], g2_count(&c.d, BLUE_PX));
        tfree(&c);
    }
}
TEST(gap2_eto_text_through_clip_region) {
    NEED_ETO(); REQUIRE(U(IntersectClipRect));
    TCtx c; if (!tctx(&c, 96, 32)) { CHECK(0); return; }
    U(IntersectClipRect)(c.d.dc, 20, 0, 40, 32);
    CHECK(etow()(c.d.dc, 2, 2, 0, NULL, L"WWWWWWWWWWWWWW", 14, NULL));
    int out = 0;
    for (int y = 0; y < 32; y++) for (int x = 0; x < 96; x++) if ((x < 20 || x >= 40) && g2_px(&c.d, x, y) != WHITE_PX) out++;
    G2_EQ(out, 0);
    CHECK(not_bg(&c.d, WHITE_PX) > 0);
    tfree(&c);
}
TEST(gap2_eto_text_color_exact) {
    NEED_ETO();
    static const COLORREF cs[] = { RGB(255, 0, 0), RGB(0, 0, 0), RGB(10, 20, 30), RGB(0, 255, 255) };
    for (unsigned i = 0; i < sizeof cs / sizeof cs[0]; i++) {
        TCtx c; if (!tctx(&c, 64, 32)) { CHECK(0); return; }
        g2_fill(&c.d, G2_PX(200, 200, 200));
        U(SetTextColor)(c.d.dc, cs[i]);
        etow()(c.d.dc, 2, 2, 0, NULL, L"Color", 5, NULL);
        DWORD want = G2_PX(GetRValue(cs[i]), GetGValue(cs[i]), GetBValue(cs[i]));
        CHECKF(only_two(&c.d, G2_PX(200, 200, 200), want), "text colour %u", i);
        CHECKF(g2_count(&c.d, want) > 0, "text colour %u absent", i);
        tfree(&c);
    }
}
TEST(gap2_eto_font_height_scales_output) {
    NEED_ETO();
    int small = 0, big = 0;
    for (int k = 0; k < 2; k++) {
        TCtx c; if (!tctx(&c, 160, 80)) { CHECK(0); return; }
        HFONT f2 = mkfont(L"Arial", k ? -40 : -12);
        HGDIOBJ prev = U(SelectObject)(c.d.dc, f2);
        etow()(c.d.dc, 2, 2, 0, NULL, L"Size", 4, NULL);
        { int v = not_bg(&c.d, WHITE_PX); if (k) big = v; else small = v; }
        U(SelectObject)(c.d.dc, prev); U(DeleteObject)(f2);
        tfree(&c);
    }
    t_note("12px: %d px, 40px: %d px", small, big);
    CHECK(big > small * 3);
}
TEST(gap2_eto_does_not_leak_gdi_objects) {
    NEED_ETO();
    TCtx c; if (!tctx(&c, 64, 32)) { CHECK(0); return; }
    DWORD base = g2_gdi_count();
    for (int i = 0; i < 2000; i++) {
        etow()(c.d.dc, 2, 2, 0, NULL, L"leak", 4, NULL);
        etoa()(c.d.dc, 2, 2, ETO_OPAQUE, NULL, "leak", 4, NULL);
    }
    EXPECT_GDI(base);
    tfree(&c);
}
TEST(gap2_eto_other_bit_depths_opaque_rect) {
    NEED_ETO(); REQUIRE(U(CreateDIBSection)); REQUIRE(U(GetPixel));
    static const int bpps[] = { 8, 16, 24, 32 };
    for (unsigned i = 0; i < sizeof bpps / sizeof bpps[0]; i++) {
        HDC dc = U(CreateCompatibleDC)(NULL);
        struct { BITMAPINFOHEADER h; RGBQUAD pal[256]; } bi; memset(&bi, 0, sizeof bi);
        bi.h.biSize = sizeof bi.h; bi.h.biWidth = 16; bi.h.biHeight = -16; bi.h.biPlanes = 1; bi.h.biBitCount = bpps[i];
        if (bpps[i] == 8) { bi.h.biClrUsed = 256; for (int k = 0; k < 256; k++) { bi.pal[k].rgbRed = k; bi.pal[k].rgbGreen = k; bi.pal[k].rgbBlue = k; } }
        void *bits;
        HBITMAP bm = U(CreateDIBSection)(dc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
        if (!bm) { U(DeleteDC)(dc); CHECKF(0, "%d bpp", bpps[i]); continue; }
        HGDIOBJ old = U(SelectObject)(dc, bm);
        U(SetBkColor)(dc, RGB(255, 255, 255));
        RECT rc = { 2, 2, 10, 10 };
        CHECKF(etow()(dc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL), "%d bpp", bpps[i]);
        CHECKF(U(GetPixel)(dc, 5, 5) == RGB(255, 255, 255), "%d bpp inside", bpps[i]);
        U(SelectObject)(dc, old); U(DeleteObject)(bm); U(DeleteDC)(dc);
    }
}

/* ============================== metafiles ============================== */
TEST(gap2_eto_emf_w_record) {
    NEED_ETO(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits));
    SKIP_ON_WINE("wine records different EMF stream");
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    CHECK(etow()(m, 5, 5, 0, NULL, L"record", 6, NULL));
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    CHECK(h != NULL); if (!h) return;
    G2_EQ(emf_count(h, EMR_EXTTEXTOUTW), 1);
    G2_EQ(emf_count(h, EMR_EXTTEXTOUTA), 0);
    U(DeleteEnhMetaFile)(h);
}
TEST(gap2_eto_emf_a_record) {
    NEED_ETO(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits));
    SKIP_ON_WINE("wine records different EMF stream");
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    CHECK(etoa()(m, 5, 5, 0, NULL, "record", 6, NULL));
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    CHECK(h != NULL); if (!h) return;
    int a = emf_count(h, EMR_EXTTEXTOUTA), w = emf_count(h, EMR_EXTTEXTOUTW);
    t_note("ExtTextOutA recorded as %d EXTTEXTOUTA + %d EXTTEXTOUTW", a, w);
    CHECK(a + w == 1);
    U(DeleteEnhMetaFile)(h);
}
TEST(gap2_eto_emf_glyph_index_record_is_w) {
    NEED_ETO(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits));
    SKIP_ON_WINE("wine records different EMF stream");
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    WORD gi[3] = { 36, 37, 38 };
    CHECK(etoa()(m, 5, 5, ETO_GLYPH_INDEX, NULL, (LPCSTR)gi, 3, NULL));
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    CHECK(h != NULL); if (!h) return;
    G2_EQ(emf_count(h, EMR_EXTTEXTOUTW), 1);
    U(DeleteEnhMetaFile)(h);
}
TEST(gap2_eto_emf_multiple_calls) {
    NEED_ETO(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits));
    SKIP_ON_WINE("wine records different EMF stream");
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    for (int i = 0; i < 7; i++) CHECK(etow()(m, i, i, 0, NULL, L"x", 1, NULL));
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    CHECK(h != NULL); if (!h) return;
    G2_EQ(emf_count(h, EMR_EXTTEXTOUTW), 7);
    U(DeleteEnhMetaFile)(h);
}
TEST(gap2_eto_emf_opaque_rect_only) {
    NEED_ETO(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits));
    SKIP_ON_WINE("wine records different EMF stream");
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    RECT rc = { 0, 0, 10, 10 };
    BOOL r = etow()(m, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL);
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    CHECK(h != NULL); if (!h) return;
    t_note("opaque rect only in EMF: ret %d, %d EXTTEXTOUTW records", r, emf_count(h, EMR_EXTTEXTOUTW));
    U(DeleteEnhMetaFile)(h);
}
TEST(gap2_eto_emf_pdy_without_dx_fails) {
    NEED_ETO(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits));
    SKIP_ON_WINE("ETO_PDY handling differs");
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    CHECK(!etow()(m, 0, 0, ETO_PDY, NULL, L"ab", 2, NULL));
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    CHECK(h != NULL); if (!h) return;
    G2_EQ(emf_count(h, EMR_EXTTEXTOUTW), 0);   /* rejected before recording */
    U(DeleteEnhMetaFile)(h);
}
TEST(gap2_eto_wmf_record) {
    NEED_ETO(); REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile)); REQUIRE(U(GetMetaFileBitsEx));
    SKIP_ON_WINE("WMF text differs");
    HDC m = U(CreateMetaFileW)(NULL);
    if (!m) { t_skip("no WMF DC"); return; }
    CHECK(etoa()(m, 5, 5, 0, NULL, "wmf", 3, NULL));
    CHECK(etow()(m, 5, 15, 0, NULL, L"wmf", 3, NULL));
    HMETAFILE h = U(CloseMetaFile)(m);
    CHECK(h != NULL); if (!h) return;
    UINT n = U(GetMetaFileBitsEx)(h, 0, NULL);
    BYTE *b = malloc(n); U(GetMetaFileBitsEx)(h, n, b);
    int cnt = 0; DWORD off = 18;
    while (off + 6 <= n) { DWORD sz = *(DWORD *)(b + off) * 2; WORD f = *(WORD *)(b + off + 4); if (sz < 6 || off + sz > n) break; if (f == 0x0A32) cnt++; off += sz; }
    free(b);
    G2_EQ(cnt, 2);   /* META_EXTTEXTOUT */
    U(DeleteMetaFile)(h);
}
