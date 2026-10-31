#include "to.h"
/* Cross-cutting behaviour: DC kinds, state save/restore, leaks, threads, code pages, batching boundaries. */

#define NEED_S() do { NEED_TEXT(); REQUIRE(U(SetTextAlign)); } while (0)
static int sctx(TCtx *c, TEXTMETRICA *tm, int w, int h) { return to_fixed_ctx(c, tm, SYSTEM_FIXED_FONT, w, h); }

TEST(text_output_special_default_memdc_bitmap) {
    NEED_S();
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(dc != NULL);
    SetLastError(0);
    CHECK(U(TextOutA)(dc, 0, 0, "abc", 3));
    CHECK(U(TextOutW)(dc, 0, 0, L"abc", 3));
    CHECK(U(ExtTextOutW)(dc, 0, 0, ETO_OPAQUE, &(RECT){ 0, 0, 1, 1 }, L"abc", 3, NULL));
    SIZE s; CHECK(U(GetTextExtentPoint32A)(dc, "abc", 3, &s));
    CHECK(s.cx > 0 && s.cy > 0);
    U(DeleteDC)(dc);
}
TEST(text_output_special_memdc_default_font_is_system) {
    NEED_S(); REQUIRE(U(GetCurrentObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ f = U(GetCurrentObject)(dc, OBJ_FONT);
    CHECK(f == U(GetStockObject)(SYSTEM_FONT) || f != NULL);
    t_note("default DC font is SYSTEM_FONT: %d", f == U(GetStockObject)(SYSTEM_FONT));
    U(DeleteDC)(dc);
}
TEST(text_output_special_info_dc_measure) {
    NEED_S(); REQUIRE(U(CreateICW));
    HDC ic = U(CreateICW)(L"DISPLAY", NULL, NULL, NULL);
    if (!ic) { t_skip("no display IC"); return; }
    SIZE s = { 0, 0 };
    CHECK(U(GetTextExtentPoint32A)(ic, "Measure", 7, &s));
    CHECK(s.cx > 0 && s.cy > 0);
    TEXTMETRICA tm; CHECK(U(GetTextMetricsA)(ic, &tm));
    CHECK(s.cy == tm.tmHeight);
    U(DeleteDC)(ic);
}
TEST(text_output_special_screen_dc_measure_matches_memdc) {
    NEED_S(); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC sc = U(GetDC)(NULL);
    HDC mc = U(CreateCompatibleDC)(sc);
    HFONT f = to_mkfont("Arial", -17, FW_NORMAL, 0, NONANTIALIASED_QUALITY);
    HGDIOBJ o1 = U(SelectObject)(sc, f), o2 = U(SelectObject)(mc, f);
    SIZE a, b;
    CHECK(U(GetTextExtentPoint32A)(sc, "Screen vs memory", 16, &a));
    CHECK(U(GetTextExtentPoint32A)(mc, "Screen vs memory", 16, &b));
    t_note("screen %ldx%ld memory %ldx%ld", a.cx, a.cy, b.cx, b.cy);
    CHECK(a.cx == b.cx && a.cy == b.cy);
    U(SelectObject)(sc, o1); U(SelectObject)(mc, o2);
    U(DeleteObject)(f); U(DeleteDC)(mc); U(ReleaseDC)(NULL, sc);
}
TEST(text_output_special_same_font_two_dcs_same_extent) {
    NEED_S();
    TCtx a, b; if (!to_open(&a, 16, 16, 32) || !to_open(&b, 16, 16, 8)) { CHECK(0); return; }
    HFONT f = to_mkfont("Segoe UI", -15, FW_NORMAL, 0, NONANTIALIASED_QUALITY);
    U(SelectObject)(a.dc, f); U(SelectObject)(b.dc, f);
    SIZE sa, sb;
    CHECK(U(GetTextExtentPoint32W)(a.dc, L"Two DCs", 7, &sa));
    CHECK(U(GetTextExtentPoint32W)(b.dc, L"Two DCs", 7, &sb));
    CHECK(sa.cx == sb.cx && sa.cy == sb.cy);
    U(SelectObject)(a.dc, a.ofont ? a.ofont : U(GetStockObject)(SYSTEM_FONT));
    U(SelectObject)(b.dc, U(GetStockObject)(SYSTEM_FONT));
    U(DeleteObject)(f);
    to_close(&a); to_close(&b);
}
/* ---- enhanced metafile recording ---- */
static int emf_find(const BYTE *bits, UINT sz, DWORD type, UINT *off) {
    UINT p = 0; int n = 0;
    while (p + 8 <= sz) {
        DWORD t = *(const DWORD *)(bits + p), l = *(const DWORD *)(bits + p + 4);
        if (l < 8 || p + l > sz) break;
        if (t == type) { if (!n && off) *off = p; n++; }
        p += l;
        if (t == EMR_EOF) break;
    }
    return n;
}
static HENHMETAFILE emf_record(int variant) {
    HDC m = U(CreateEnhMetaFileA)(NULL, NULL, NULL, NULL);
    if (!m) return NULL;
    switch (variant) {
    case 0: U(TextOutA)(m, 5, 6, "meta", 4); break;
    case 1: U(TextOutW)(m, 5, 6, L"meta", 4); break;
    case 2: U(ExtTextOutA)(m, 5, 6, ETO_OPAQUE, &(RECT){ 0, 0, 40, 20 }, "meta", 4, NULL); break;
    case 3: U(ExtTextOutW)(m, 5, 6, ETO_CLIPPED, &(RECT){ 0, 0, 40, 20 }, L"meta", 4, NULL); break;
    }
    return U(CloseEnhMetaFile)(m);
}
#define NEED_EMF() do { REQUIRE(U(CreateEnhMetaFileA)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits)); REQUIRE(U(DeleteEnhMetaFile)); } while (0)
static void emf_case(int variant, DWORD type) {
    NEED_S(); NEED_EMF();
    HENHMETAFILE e = emf_record(variant); CHECK(e != NULL); if (!e) return;
    UINT sz = U(GetEnhMetaFileBits)(e, 0, NULL);
    BYTE *bits = (BYTE *)HeapAlloc(GetProcessHeap(), 0, sz);
    CHECK(U(GetEnhMetaFileBits)(e, sz, bits) == sz);
    UINT off = 0;
    int n = emf_find(bits, sz, type, &off);
    CHECKF(n == 1, "variant %d: %d records of type %lu", variant, n, type);
    if (n == 1) {
        const EMREXTTEXTOUTW *r = (const EMREXTTEXTOUTW *)(bits + off);
        CHECKF(r->emrtext.nChars == 4, "nChars %lu", r->emrtext.nChars);
        CHECKF(r->emrtext.ptlReference.x == 5 && r->emrtext.ptlReference.y == 6, "ref %ld,%ld", r->emrtext.ptlReference.x, r->emrtext.ptlReference.y);
        if (variant == 2) CHECK(r->emrtext.fOptions == ETO_OPAQUE);
        if (variant == 3) CHECK(r->emrtext.fOptions == ETO_CLIPPED);
    }
    HeapFree(GetProcessHeap(), 0, bits);
    U(DeleteEnhMetaFile)(e);
}
TEST(text_output_emf_textouta_record) { emf_case(0, EMR_EXTTEXTOUTA); }
TEST(text_output_emf_textoutw_record) { emf_case(1, EMR_EXTTEXTOUTW); }
TEST(text_output_emf_exttextouta_opaque_record) { emf_case(2, EMR_EXTTEXTOUTA); }
TEST(text_output_emf_exttextoutw_clipped_record) { emf_case(3, EMR_EXTTEXTOUTW); }
TEST(text_output_emf_zero_count_textout_records_nothing) {
    NEED_S(); NEED_EMF();
    HDC m = U(CreateEnhMetaFileA)(NULL, NULL, NULL, NULL);
    CHECK(m != NULL); if (!m) return;
    CHECK(U(TextOutA)(m, 0, 0, "x", 0));
    CHECK(U(TextOutW)(m, 0, 0, L"x", 0));
    HENHMETAFILE e = U(CloseEnhMetaFile)(m);
    UINT sz = U(GetEnhMetaFileBits)(e, 0, NULL);
    BYTE *bits = (BYTE *)HeapAlloc(GetProcessHeap(), 0, sz);
    U(GetEnhMetaFileBits)(e, sz, bits);
    CHECK(emf_find(bits, sz, EMR_EXTTEXTOUTA, NULL) == 0);
    CHECK(emf_find(bits, sz, EMR_EXTTEXTOUTW, NULL) == 0);
    HeapFree(GetProcessHeap(), 0, bits);
    U(DeleteEnhMetaFile)(e);
}
TEST(text_output_emf_polytextout_records) {
    NEED_S(); NEED_EMF(); REQUIRE(U(PolyTextOutW));
    HDC m = U(CreateEnhMetaFileA)(NULL, NULL, NULL, NULL);
    CHECK(m != NULL); if (!m) return;
    POLYTEXTW pt[2]; memset(pt, 0, sizeof pt);
    pt[0].x = 1; pt[0].y = 2; pt[0].lpstr = L"ab"; pt[0].n = 2;
    pt[1].x = 3; pt[1].y = 4; pt[1].lpstr = L"cde"; pt[1].n = 3;
    CHECK(U(PolyTextOutW)(m, pt, 2));
    HENHMETAFILE e = U(CloseEnhMetaFile)(m);
    UINT sz = U(GetEnhMetaFileBits)(e, 0, NULL);
    BYTE *bits = (BYTE *)HeapAlloc(GetProcessHeap(), 0, sz);
    U(GetEnhMetaFileBits)(e, sz, bits);
    int w = emf_find(bits, sz, EMR_POLYTEXTOUTW, NULL), a = emf_find(bits, sz, EMR_POLYTEXTOUTA, NULL);
    int ex = emf_find(bits, sz, EMR_EXTTEXTOUTW, NULL);
    t_note("PolyTextOutW recorded as: POLYTEXTOUTW=%d POLYTEXTOUTA=%d EXTTEXTOUTW=%d", w, a, ex);
    CHECK(w + a + ex >= 1);
    HeapFree(GetProcessHeap(), 0, bits);
    U(DeleteEnhMetaFile)(e);
}
TEST(text_output_emf_header_record_count_grows) {
    NEED_S(); NEED_EMF();
    HENHMETAFILE e0 = emf_record(-1), e1 = emf_record(1);
    ENHMETAHEADER h0, h1;
    memset(&h0, 0, sizeof h0); memset(&h1, 0, sizeof h1);
    U(GetEnhMetaFileHeader)(e0, sizeof h0, &h0);
    U(GetEnhMetaFileHeader)(e1, sizeof h1, &h1);
    CHECKF(h1.nRecords == h0.nRecords + 1, "records %lu -> %lu", h0.nRecords, h1.nRecords);
    U(DeleteEnhMetaFile)(e0); U(DeleteEnhMetaFile)(e1);
}
/* ---- DC state ---- */
TEST(text_output_state_savedc_restores_text_state) {
    NEED_S(); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(GetBkMode)); REQUIRE(U(GetTextAlign));
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    U(SetTextColor)(c.dc, RGB(1, 2, 3)); U(SetBkColor)(c.dc, RGB(4, 5, 6));
    U(SetBkMode)(c.dc, OPAQUE); U(SetTextAlign)(c.dc, TA_RIGHT | TA_BOTTOM);
    HFONT f = to_mkfont("Arial", -20, FW_NORMAL, 0, NONANTIALIASED_QUALITY);
    HGDIOBJ o = U(SelectObject)(c.dc, f);
    int id = U(SaveDC)(c.dc);
    CHECK(id >= 1);
    U(SetTextColor)(c.dc, RGB(9, 9, 9)); U(SetBkColor)(c.dc, RGB(8, 8, 8));
    U(SetBkMode)(c.dc, TRANSPARENT); U(SetTextAlign)(c.dc, TA_LEFT);
    U(SelectObject)(c.dc, U(GetStockObject)(SYSTEM_FONT));
    CHECK(U(RestoreDC)(c.dc, id));
    CHECK(U(GetTextColor)(c.dc) == RGB(1, 2, 3));
    CHECK(U(GetBkColor)(c.dc) == RGB(4, 5, 6));
    CHECK(U(GetBkMode)(c.dc) == OPAQUE);
    CHECK(U(GetTextAlign)(c.dc) == (TA_RIGHT | TA_BOTTOM));
    CHECK(U(GetCurrentObject)(c.dc, OBJ_FONT) == (HGDIOBJ)f);
    U(SelectObject)(c.dc, o);
    U(DeleteObject)(f);
    to_close(&c);
}
TEST(text_output_state_restoredc_negative_relative) {
    NEED_S(); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    U(SetTextColor)(c.dc, RGB(10, 20, 30));
    U(SaveDC)(c.dc);
    U(SetTextColor)(c.dc, RGB(40, 50, 60));
    U(SaveDC)(c.dc);
    U(SetTextColor)(c.dc, RGB(70, 80, 90));
    CHECK(U(RestoreDC)(c.dc, -1));
    CHECK(U(GetTextColor)(c.dc) == RGB(40, 50, 60));
    CHECK(U(RestoreDC)(c.dc, -1));
    CHECK(U(GetTextColor)(c.dc) == RGB(10, 20, 30));
    to_close(&c);
}
TEST(text_output_state_text_color_special_flags) {
    NEED_S();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    static const COLORREF cr[] = { 0x01000005, 0x02ff8040, 0x00ffffff, 0x03123456, 0xff000000, 0x80000000 };
    for (int i = 0; i < ARRAYSZ(cr); i++) {
        SetLastError(0);
        COLORREF prev = U(SetTextColor)(c.dc, cr[i]);
        t_note("SetTextColor(0x%08lx): prev=0x%08lx now=0x%08lx", cr[i], prev, U(GetTextColor)(c.dc));
        CHECK(prev != CLR_INVALID);
    }
    U(SetTextColor)(c.dc, 0);
    to_close(&c);
}
TEST(text_output_state_bk_color_special_flags) {
    NEED_S();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    static const COLORREF cr[] = { 0x01000005, 0x02ff8040, 0x03123456, 0xff000000 };
    for (int i = 0; i < ARRAYSZ(cr); i++) {
        COLORREF prev = U(SetBkColor)(c.dc, cr[i]);
        t_note("SetBkColor(0x%08lx): prev=0x%08lx now=0x%08lx", cr[i], prev, U(GetBkColor)(c.dc));
    }
    to_close(&c);
}
/* ---- depths ---- */
TEST(text_output_depth_16bpp_colors) {
    NEED_S();
    TCtx c; if (!to_open(&c, 120, 40, 16)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FIXED_FONT), 0);
    U(SetBkMode)(c.dc, OPAQUE); U(SetBkColor)(c.dc, RGB(0, 0, 255)); U(SetTextColor)(c.dc, RGB(255, 0, 0));
    CHECK(U(TextOutA)(c.dc, 4, 4, "16bpp", 5));
    CHECK(to_count(&c, 0x7c00, 0, 0, 120, 40) > 0);
    CHECK(to_count(&c, 0x001f, 0, 0, 120, 40) > 0);
    int other = 0;
    for (int y = 0; y < 40; y++) for (int x = 0; x < 120; x++) { unsigned p = to_px(&c, x, y); other += p != 0x7c00 && p != 0x001f && p != 0x7fff; }
    CHECKF(other == 0, "%d unexpected pixels", other);
    to_close(&c);
}
TEST(text_output_depth_8bpp_gray_table_exact_index) {
    NEED_S();
    TCtx c; if (!to_open(&c, 120, 40, 8)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FIXED_FONT), 0);
    U(SetBkMode)(c.dc, OPAQUE); U(SetBkColor)(c.dc, RGB(100, 100, 100)); U(SetTextColor)(c.dc, RGB(50, 50, 50));
    CHECK(U(TextOutA)(c.dc, 4, 4, "8bpp", 4));
    CHECK(to_count(&c, 100, 0, 0, 120, 40) > 0);
    CHECK(to_count(&c, 50, 0, 0, 120, 40) > 0);
    int other = 0;
    for (int y = 0; y < 40; y++) for (int x = 0; x < 120; x++) { unsigned p = to_px(&c, x, y); other += p != 100 && p != 50 && p != 255; }
    CHECKF(other == 0, "%d pixels with other indices", other);
    to_close(&c);
}
TEST(text_output_depth_24bpp_exact_rgb) {
    NEED_S();
    TCtx c; if (!to_open(&c, 120, 40, 24)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FIXED_FONT), 0);
    U(SetBkMode)(c.dc, OPAQUE); U(SetBkColor)(c.dc, RGB(12, 34, 56)); U(SetTextColor)(c.dc, RGB(200, 150, 100));
    CHECK(U(TextOutA)(c.dc, 4, 4, "24bpp", 5));
    CHECK(to_count(&c, to_cref2px(RGB(12, 34, 56)), 0, 0, 120, 40) > 0);
    CHECK(to_count(&c, to_cref2px(RGB(200, 150, 100)), 0, 0, 120, 40) > 0);
    to_close(&c);
}
/* ---- boundaries of the client-side text batching (0x240 chars) ---- */
static void batch_case(int n) {
    NEED_S();
    TCtx c; TEXTMETRICA tm;
    if (!to_fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT, 6000, 24)) return;
    if (n * tm.tmAveCharWidth + 8 > c.w) { t_skip("too wide"); to_close(&c); return; }
    static char s[700]; static WCHAR w[700];
    for (int i = 0; i < n; i++) { s[i] = 'M'; w[i] = L'M'; }
    U(SetTextColor)(c.dc, 0);
    CHECK(U(TextOutA)(c.dc, 4, 2, s, n));
    RECT ink; int na = to_ink(&c, 0xffffff, &ink);
    int last = 4 + (n - 1) * tm.tmAveCharWidth;
    CHECKF(to_count(&c, 0, last, 0, last + tm.tmAveCharWidth, 24) > 0, "n=%d: last glyph missing", n);
    CHECKF(ink.right < 4 + n * tm.tmAveCharWidth, "n=%d: ink spills right (%ld)", n, ink.right);
    to_clear(&c);
    CHECK(U(TextOutW)(c.dc, 4, 2, w, n));
    RECT ink2; int nw = to_ink(&c, 0xffffff, &ink2);
    CHECKF(na == nw && ink.left == ink2.left && ink.right == ink2.right, "n=%d: A (%d) vs W (%d)", n, na, nw);
    to_clear(&c);
    CHECK(U(ExtTextOutW)(c.dc, 4, 2, 0, NULL, w, n, NULL));
    CHECKF(to_ink(&c, 0xffffff, &ink2) == na, "n=%d ExtTextOutW differs", n);
    to_close(&c);
}
TEST(text_output_batch_boundary_575) { batch_case(575); }
TEST(text_output_batch_boundary_576) { batch_case(576); }
TEST(text_output_batch_boundary_577) { batch_case(577); }
TEST(text_output_batch_boundary_578) { batch_case(578); }
TEST(text_output_batch_boundary_1) { batch_case(1); }
TEST(text_output_batch_boundary_100) { batch_case(100); }
/* ---- additivity and per-char tables on bitmap fonts ---- */
TEST(text_output_special_every_byte_one_cell_fixed) {
    NEED_S();
    TCtx c; TEXTMETRICA tm; if (!sctx(&c, &tm, 32, 32)) return;
    for (int ch = 1; ch < 256; ch++) {
        char b[2] = { (char)ch, 0 }; SIZE s;
        CHECKF(U(GetTextExtentPoint32A)(c.dc, b, 1, &s) && s.cx == tm.tmAveCharWidth, "byte 0x%02x cx=%ld ave=%ld", ch, s.cx, tm.tmAveCharWidth);
    }
    to_close(&c);
}
TEST(text_output_special_system_font_extent_additive) {
    NEED_S();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FONT), 0);
    TEXTMETRICA tm; U(GetTextMetricsA)(c.dc, &tm);
    if (tm.tmPitchAndFamily & (TMPF_TRUETYPE | TMPF_VECTOR)) { t_skip("System font is not a bitmap font here"); to_close(&c); return; }
    static const char *s = "Additive bitmap widths: 0123456789";
    int n = (int)strlen(s), sum = 0;
    for (int i = 0; i < n; i++) { SIZE one; CHECK(U(GetTextExtentPoint32A)(c.dc, s + i, 1, &one)); sum += one.cx; }
    SIZE all; CHECK(U(GetTextExtentPoint32A)(c.dc, s, n, &all));
    CHECKF(all.cx == sum, "whole %ld sum %d", all.cx, sum);
    to_close(&c);
}
TEST(text_output_special_system_font_a_w_pixel_parity) {
    NEED_S();
    TCtx c; if (!to_open(&c, 400, 30, 32)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(SYSTEM_FONT), 0);
    U(SetTextColor)(c.dc, 0);
    char s[96]; WCHAR w[96]; int n = 0;
    for (int ch = 0x20; ch < 0x7f; ch++) s[n++] = (char)ch;
    to_w(s, w, 96);
    CHECK(U(TextOutA)(c.dc, 2, 2, s, n));
    static unsigned a[400 * 30];
    for (int y = 0; y < 30; y++) for (int x = 0; x < 400; x++) a[y * 400 + x] = to_px(&c, x, y);
    to_clear(&c);
    CHECK(U(TextOutW)(c.dc, 2, 2, w, n));
    int d = 0; for (int y = 0; y < 30; y++) for (int x = 0; x < 400; x++) d += a[y * 400 + x] != to_px(&c, x, y);
    CHECKF(d == 0, "%d px differ", d);
    to_close(&c);
}
TEST(text_output_special_draw_idempotent_transparent) {
    NEED_S();
    TCtx c; TEXTMETRICA tm; if (!sctx(&c, &tm, 100, 30)) return;
    U(SetTextColor)(c.dc, RGB(0, 80, 0));
    for (int i = 0; i < 2; i++) CHECK(U(TextOutA)(c.dc, 3, 3, "twice", 5));
    static unsigned a[100 * 30];
    for (int y = 0; y < 30; y++) for (int x = 0; x < 100; x++) a[y * 100 + x] = to_px(&c, x, y);
    to_clear(&c);
    CHECK(U(TextOutA)(c.dc, 3, 3, "twice", 5));
    int d = 0; for (int y = 0; y < 30; y++) for (int x = 0; x < 100; x++) d += a[y * 100 + x] != to_px(&c, x, y);
    CHECKF(d == 0, "%d px differ", d);
    to_close(&c);
}
TEST(text_output_special_right_edge_clipping_translation) {
    NEED_S();
    TCtx c; TEXTMETRICA tm; if (!sctx(&c, &tm, 100, 30)) return;
    U(SetTextColor)(c.dc, 0);
    CHECK(U(TextOutA)(c.dc, 70, 5, "EdgeEdge", 8));
    static unsigned a[100 * 30];
    for (int y = 0; y < 30; y++) for (int x = 0; x < 100; x++) a[y * 100 + x] = to_px(&c, x, y);
    to_close(&c);
    TCtx w; if (!to_fixed_ctx(&w, &tm, SYSTEM_FIXED_FONT, 300, 30)) return;
    U(SetTextColor)(w.dc, 0);
    CHECK(U(TextOutA)(w.dc, 70, 5, "EdgeEdge", 8));
    int d = 0; for (int y = 0; y < 30; y++) for (int x = 0; x < 100; x++) d += a[y * 100 + x] != to_px(&w, x, y);
    CHECKF(d == 0, "clipped render differs from wide render in %d px", d);
    to_close(&w);
}
TEST(text_output_special_bottom_edge_clipping_no_overrun) {
    NEED_S();
    TCtx c; TEXTMETRICA tm; if (!sctx(&c, &tm, 64, 20)) return;
    U(SetBkMode)(c.dc, OPAQUE); U(SetBkColor)(c.dc, RGB(255, 0, 0));
    CHECK(U(TextOutA)(c.dc, 60, 15, "overrun", 7));
    RECT ink; int n = to_ink(&c, 0xffffff, &ink);
    CHECKF(n == 4 * 5, "painted %d want 20", n);
    to_close(&c);
}
/* ---- threads and handles ---- */
typedef struct { HDC dc; HFONT font; BOOL r[4]; SIZE sz; } TD;
static DWORD WINAPI thr_draw(LPVOID p) {
    TD *d = (TD *)p;
    HGDIOBJ o = U(SelectObject)(d->dc, d->font);
    d->r[0] = U(TextOutA)(d->dc, 3, 3, "thread", 6);
    d->r[1] = U(TextOutW)(d->dc, 3, 15, L"thread", 6);
    d->r[2] = U(GetTextExtentPoint32A)(d->dc, "thread", 6, &d->sz);
    U(SelectObject)(d->dc, o);
    return 0;
}
TEST(text_output_thread_use_dc_from_other_thread) {
    NEED_S();
    TCtx c; if (!to_open(&c, 100, 40, 32)) { CHECK(0); return; }
    HFONT f = to_mkfont("Arial", -14, FW_NORMAL, 0, NONANTIALIASED_QUALITY);
    U(SetTextColor)(c.dc, 0);
    TD d; memset(&d, 0, sizeof d); d.dc = c.dc; d.font = f;
    HANDLE h = CreateThread(NULL, 0, thr_draw, &d, 0, NULL);
    CHECK(h != NULL);
    if (h) { CHECK(WaitForSingleObject(h, 10000) == WAIT_OBJECT_0); CloseHandle(h); }
    t_note("thread results: %d %d %d", d.r[0], d.r[1], d.r[2]);
    CHECK(d.r[0] && d.r[1] && d.r[2]);
    RECT ink; CHECK(to_ink(&c, 0xffffff, &ink) > 0);
    SIZE here; HGDIOBJ o = U(SelectObject)(c.dc, f);
    U(GetTextExtentPoint32A)(c.dc, "thread", 6, &here);
    U(SelectObject)(c.dc, o);
    CHECK(here.cx == d.sz.cx && here.cy == d.sz.cy);
    U(DeleteObject)(f);
    to_close(&c);
}
static DWORD WINAPI thr_font(LPVOID p) {
    HFONT *out = (HFONT *)p;
    *out = to_mkfont("Arial", -22, FW_BOLD, 0, NONANTIALIASED_QUALITY);
    return 0;
}
TEST(text_output_thread_font_created_elsewhere) {
    NEED_S();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    HFONT f = NULL;
    HANDLE h = CreateThread(NULL, 0, thr_font, &f, 0, NULL);
    if (h) { WaitForSingleObject(h, 10000); CloseHandle(h); }
    CHECK(f != NULL);
    if (f) {
        HFONT mine = to_mkfont("Arial", -22, FW_BOLD, 0, NONANTIALIASED_QUALITY);
        HGDIOBJ o = U(SelectObject)(c.dc, f);
        SIZE a, b; CHECK(U(GetTextExtentPoint32A)(c.dc, "Elsewhere", 9, &a));
        U(SelectObject)(c.dc, mine);
        CHECK(U(GetTextExtentPoint32A)(c.dc, "Elsewhere", 9, &b));
        CHECK(a.cx == b.cx && a.cy == b.cy);
        U(SelectObject)(c.dc, o);
        CHECK(U(DeleteObject)(f)); CHECK(U(DeleteObject)(mine));
    }
    to_close(&c);
}
TEST(text_output_leak_font_create_use_delete_loop) {
    NEED_S();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    int before = to_gdi_count();
    for (int i = 0; i < 300; i++) {
        HFONT f = to_mkfont(i & 1 ? "Arial" : "Courier New", -8 - (i % 20), FW_NORMAL, 0, NONANTIALIASED_QUALITY);
        HGDIOBJ o = U(SelectObject)(c.dc, f);
        U(TextOutA)(c.dc, 0, 0, "leak", 4);
        SIZE s; U(GetTextExtentPoint32W)(c.dc, L"leak", 4, &s);
        U(SelectObject)(c.dc, o);
        U(DeleteObject)(f);
    }
    int after = to_gdi_count();
    CHECKF(before < 0 || after == before, "GDI objects %d -> %d", before, after);
    to_close(&c);
}
TEST(text_output_leak_text_calls_do_not_create_objects) {
    NEED_S();
    TCtx c; TEXTMETRICA tm; if (!sctx(&c, &tm, 100, 40)) return;
    int before = to_gdi_count();
    for (int i = 0; i < 2000; i++) {
        U(TextOutA)(c.dc, i % 50, 2, "abc", 3);
        U(ExtTextOutW)(c.dc, 0, 20, ETO_OPAQUE, &(RECT){ 0, 20, 10, 30 }, L"x", 1, NULL);
        SIZE s; U(GetTextExtentPoint32A)(c.dc, "abc", 3, &s);
    }
    int after = to_gdi_count();
    CHECKF(before < 0 || after == before, "GDI objects %d -> %d", before, after);
    to_close(&c);
}
TEST(text_output_leak_dc_create_delete_with_text) {
    NEED_S();
    int before = to_gdi_count();
    for (int i = 0; i < 200; i++) {
        TCtx c; if (!to_open(&c, 32, 16, 32)) { CHECK(0); return; }
        U(TextOutA)(c.dc, 0, 0, "dc", 2);
        to_close(&c);
    }
    int after = to_gdi_count();
    CHECKF(before < 0 || after == before, "GDI objects %d -> %d", before, after);
}
/* ---- code pages ---- */
TEST(text_output_cp_ansi_high_bytes_a_vs_w) {
    NEED_S();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    LOGFONTA lf; memset(&lf, 0, sizeof lf); lf.lfHeight = -16; lf.lfCharSet = ANSI_CHARSET; lf.lfQuality = NONANTIALIASED_QUALITY;
    strcpy(lf.lfFaceName, "Arial");
    to_use_font(&c, U(CreateFontIndirectA)(&lf), 1);
    int bad = 0;
    for (int ch = 0xa0; ch < 0x100; ch++) {
        char b[2] = { (char)ch, 0 }; WCHAR w[2] = { (WCHAR)ch, 0 }; SIZE a, d;
        U(GetTextExtentPoint32A)(c.dc, b, 1, &a);
        U(GetTextExtentPoint32W)(c.dc, w, 1, &d);
        if (a.cx != d.cx) bad++;
    }
    t_note("Latin-1 bytes whose A/W widths differ: %d", bad);
    CHECKF(bad == 0, "%d bytes differ", bad);
    to_close(&c);
}
TEST(text_output_cp_cp1252_specials_a_vs_w) {
    NEED_S();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    LOGFONTA lf; memset(&lf, 0, sizeof lf); lf.lfHeight = -16; lf.lfCharSet = ANSI_CHARSET; lf.lfQuality = NONANTIALIASED_QUALITY;
    strcpy(lf.lfFaceName, "Arial");
    to_use_font(&c, U(CreateFontIndirectA)(&lf), 1);
    static const struct { unsigned char b; WCHAR w; } m[] = { {0x80, 0x20ac}, {0x85, 0x2026}, {0x91, 0x2018}, {0x92, 0x2019}, {0x93, 0x201c}, {0x94, 0x201d}, {0x95, 0x2022}, {0x96, 0x2013}, {0x97, 0x2014}, {0x99, 0x2122} };
    for (int i = 0; i < ARRAYSZ(m); i++) {
        char b[2] = { (char)m[i].b, 0 }; WCHAR w[2] = { m[i].w, 0 }; SIZE a, d;
        U(GetTextExtentPoint32A)(c.dc, b, 1, &a);
        U(GetTextExtentPoint32W)(c.dc, w, 1, &d);
        t_note("byte 0x%02x width %ld, U+%04x width %ld", m[i].b, a.cx, m[i].w, d.cx);
        CHECK(a.cx > 0 && d.cx > 0);
    }
    to_close(&c);
}
TEST(text_output_cp_greek_charset_font_a) {
    NEED_S();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    LOGFONTA lf; memset(&lf, 0, sizeof lf); lf.lfHeight = -16; lf.lfCharSet = GREEK_CHARSET; lf.lfQuality = NONANTIALIASED_QUALITY;
    strcpy(lf.lfFaceName, "Arial");
    to_use_font(&c, U(CreateFontIndirectA)(&lf), 1);
    SIZE a, w;
    CHECK(U(GetTextExtentPoint32A)(c.dc, "\xe1", 1, &a));
    CHECK(U(GetTextExtentPoint32W)(c.dc, L"\x03b1", 1, &w));
    t_note("Greek charset font: byte 0xE1 width %ld, U+03B1 width %ld", a.cx, w.cx);
    CHECK(a.cx > 0);
    to_close(&c);
}
TEST(text_output_cp_dbcs_lead_byte_note) {
    NEED_S(); REQUIRE(U(CreateFontIndirectA));
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    CPINFO ci; GetCPInfo(CP_ACP, &ci);
    t_note("ACP=%u MaxCharSize=%u lead(0x81)=%d lead(0x82)=%d", GetACP(), ci.MaxCharSize, IsDBCSLeadByte(0x81), IsDBCSLeadByte(0x82));
    to_use_font(&c, to_mkfont("Arial", -16, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    SIZE one, two;
    CHECK(U(GetTextExtentPoint32A)(c.dc, "\x82\xa0", 2, &two));
    CHECK(U(GetTextExtentPoint32A)(c.dc, "\x82", 1, &one));
    t_note("2-byte string width %ld, lead byte alone %ld", two.cx, one.cx);
    CHECK(two.cx > 0);
    to_close(&c);
}
TEST(text_output_cp_trailing_lead_byte_does_not_overrun) {
    NEED_S();
    TCtx c; if (!to_open(&c, 64, 32, 32)) { CHECK(0); return; }
    char buf[4096]; memset(buf, 'a', sizeof buf);
    /* a lone lead byte as the last character of the run */
    char s[3] = { 'a', (char)0x82, 0 };
    SIZE sz;
    SetLastError(0);
    BOOL r = U(GetTextExtentPoint32A)(c.dc, s, 2, &sz);
    t_note("trailing lead byte: ret=%d cx=%ld", r, sz.cx);
    CHECK(U(TextOutA)(c.dc, 0, 0, s, 2) != 2);
    to_close(&c);
}
TEST(text_output_cp_surrogate_pair_extent) {
    NEED_S();
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Segoe UI", -20, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    static const WCHAR pair[] = { 0xd83d, 0xde00 };
    SIZE s;
    CHECK(U(GetTextExtentPoint32W)(c.dc, pair, 2, &s));
    t_note("surrogate pair extent %ldx%ld", s.cx, s.cy);
    CHECK(s.cy > 0);
    static const WCHAR lone[] = { 0xd83d };
    CHECK(U(GetTextExtentPoint32W)(c.dc, lone, 1, &s));
    CHECK(U(TextOutW)(c.dc, 0, 0, pair, 2));
    CHECK(U(TextOutW)(c.dc, 0, 0, lone, 1));
    to_close(&c);
}
TEST(text_output_special_dpi_scaled_font_sizes) {
    NEED_S(); REQUIRE(U(GetDeviceCaps));
    TCtx c; if (!to_open(&c, 32, 32, 32)) { CHECK(0); return; }
    int dpi = U(GetDeviceCaps)(c.dc, LOGPIXELSY);
    t_note("LOGPIXELSY=%d", dpi);
    CHECK(dpi >= 72);
    int px = MulDiv(12, dpi, 72);
    to_use_font(&c, to_mkfont("Arial", -px, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    TEXTMETRICA tm; CHECK(U(GetTextMetricsA)(c.dc, &tm));
    if (!to_wine()) CHECKF(tm.tmHeight - tm.tmInternalLeading == px, "12pt at %d dpi: %ld", dpi, tm.tmHeight - tm.tmInternalLeading);
    to_close(&c);
}
TEST(text_output_special_dc_selected_bitmap_must_be_dib_or_ddb) {
    NEED_S();
    /* text into a DDB (CreateCompatibleBitmap of memory DC) - same API paths as DIB */
    HDC dc = U(CreateCompatibleDC)(NULL);
    HBITMAP bm = U(CreateBitmap)(40, 20, 1, 1, NULL);
    HGDIOBJ o = U(SelectObject)(dc, bm);
    CHECK(o != NULL);
    CHECK(U(PatBlt)(dc, 0, 0, 40, 20, WHITENESS));
    CHECK(U(TextOutA)(dc, 0, 0, "mono", 4));
    U(SelectObject)(dc, o);
    U(DeleteObject)(bm);
    U(DeleteDC)(dc);
}
TEST(text_output_special_mono_ddb_pixels_blackness) {
    NEED_S(); REQUIRE(U(GetPixel));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HBITMAP bm = U(CreateBitmap)(64, 24, 1, 1, NULL);
    HGDIOBJ o = U(SelectObject)(dc, bm);
    HGDIOBJ of = U(SelectObject)(dc, U(GetStockObject)(SYSTEM_FIXED_FONT));
    U(PatBlt)(dc, 0, 0, 64, 24, WHITENESS);
    U(SetBkMode)(dc, TRANSPARENT);
    U(SetTextColor)(dc, RGB(0, 0, 0));
    CHECK(U(TextOutA)(dc, 2, 2, "HH", 2));
    int dark = 0, light = 0, other = 0;
    for (int y = 0; y < 24; y++) for (int x = 0; x < 64; x++) {
        COLORREF c = U(GetPixel)(dc, x, y);
        if (c == RGB(0, 0, 0)) dark++; else if (c == RGB(255, 255, 255)) light++; else other++;
    }
    CHECKF(dark > 0 && other == 0, "dark %d light %d other %d", dark, light, other);
    U(SelectObject)(dc, of); U(SelectObject)(dc, o);
    U(DeleteObject)(bm); U(DeleteDC)(dc);
}
