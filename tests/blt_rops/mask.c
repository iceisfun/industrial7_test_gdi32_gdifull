/* MaskBlt (ROP4) and PlgBlt */
#include "blt.h"

typedef int (*maskfn)(int x, int y);
static int m_checker(int x, int y) { return (x + y) & 1; }
static int m_cols(int x, int y) { return (x / 3) & 1; }
static int m_rows(int x, int y) { return (y / 2) & 1; }
static int m_ones(int x, int y) { return 1; }
static int m_zeros(int x, int y) { return 0; }
static int m_diag(int x, int y) { return x > y; }
static int m_rnd(int x, int y) { return (x * 31 + y * 17 + (x * y)) % 5 < 2; }

static HBITMAP make_mask(int w, int h, maskfn f) {
    int stride = ((w + 15) / 16) * 2, x, y;
    BYTE *bits = calloc(stride * h, 1);
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) if (f(x, y)) bits[y * stride + x / 8] |= 0x80 >> (x & 7);
    HBITMAP bm = pCreateBitmap(w, h, 1, 1, bits);
    free(bits);
    return bm;
}
#define BACK(code) ((DWORD)(code) << 16)
static DWORD R4(unsigned fore, unsigned back) { return MAKEROP4(ROP(fore), ROP(back)); }

static void maskblt_case(unsigned fore, unsigned back, maskfn f, int xm, int ym, int msize, int check_unused) {
    BD s, d; HBITMAP mk; int x, y, bad = 0; const int N = 12;
    NEED();
    s = bd_new(32, N + 4, N + 4); d = bd_new(32, N + 4, N + 4);
    DWORD P = 0x40A0E0, sv[16 * 16], dv[16 * 16];
    for (y = 0; y < N + 4; y++) for (x = 0; x < N + 4; x++) { bd_put(&s, x, y, sv[y * 16 + x] = rnd() & 0xFFFFFF); bd_put(&d, x, y, dv[y * 16 + x] = rnd() & 0xFFFFFF); }
    mk = make_mask(msize, msize, f);
    HBRUSH br = pCreateSolidBrush(cr(P)); HGDIOBJ o = pSelectObject(d.dc, br);
    BOOL r = pMaskBlt(d.dc, 2, 3, N, N, s.dc, 1, 2, mk, xm, ym, R4(fore, back));
    CHECKF(r, "MaskBlt fore %02X back %02X ret %d err %lu", fore, back, r, GetLastError());
    for (y = 0; y < N + 4; y++) for (x = 0; x < N + 4; x++) {
        DWORD e = dv[y * 16 + x];
        if (x >= 2 && x < 2 + N && y >= 3 && y < 3 + N) {
            int m = f(x - 2 + xm, y - 3 + ym);
            DWORD S = sv[(y - 3 + 2) * 16 + (x - 2 + 1)];
            e = rop3(m ? fore : back, P, S, dv[y * 16 + x]) & 0xFFFFFF;
        }
        if ((bd_get(&d, x, y) & 0xFFFFFF) != (e & 0xFFFFFF)) bad++;
    }
    CHECKF(!bad, "fore %02X back %02X mask-off (%d,%d): %d pixels differ", fore, back, xm, ym, bad);
    pSelectObject(d.dc, o); pDeleteObject(br); pDeleteObject(mk);
    bd_free(&s); bd_free(&d);
}
TEST(maskblt_copy_where_set_keep_elsewhere) { maskblt_case(0xCC, 0xAA, m_checker, 0, 0, 16, 0); }
TEST(maskblt_inverse_roles) { maskblt_case(0xAA, 0xCC, m_checker, 0, 0, 16, 0); }
TEST(maskblt_copy_or_black) { maskblt_case(0xCC, 0x00, m_cols, 0, 0, 16, 0); }
TEST(maskblt_copy_or_white) { maskblt_case(0xCC, 0xFF, m_rows, 0, 0, 16, 0); }
TEST(maskblt_src_or_pattern) { maskblt_case(0xCC, 0xF0, m_checker, 0, 0, 16, 0); }
TEST(maskblt_pattern_or_src) { maskblt_case(0xF0, 0xCC, m_diag, 0, 0, 16, 0); }
TEST(maskblt_xor_or_not) { maskblt_case(0x66, 0x33, m_rnd, 0, 0, 16, 0); }
TEST(maskblt_all_ones_is_fore_only) { maskblt_case(0xCC, 0x00, m_ones, 0, 0, 16, 0); }
TEST(maskblt_all_zeros_is_back_only) { maskblt_case(0x00, 0xCC, m_zeros, 0, 0, 16, 0); }
TEST(maskblt_all_zeros_back_nop) { maskblt_case(0xCC, 0xAA, m_zeros, 0, 0, 16, 0); }
TEST(maskblt_mask_offset_x) { maskblt_case(0xCC, 0xAA, m_cols, 2, 0, 20, 0); }
TEST(maskblt_mask_offset_y) { maskblt_case(0xCC, 0xAA, m_rows, 0, 3, 20, 0); }
TEST(maskblt_mask_offset_xy) { maskblt_case(0xCC, 0xAA, m_rnd, 5, 7, 24, 0); }
TEST(maskblt_mask_exact_size) { maskblt_case(0xCC, 0xAA, m_diag, 0, 0, 12, 0); }
TEST(maskblt_rop4_random_pairs) {
    int i; NEED();
    for (i = 0; i < 24; i++) {
        unsigned f = rnd() & 255, b = rnd() & 255;
        maskblt_case(f, b, (i & 1) ? m_rnd : m_checker, i % 3, i % 4, 20, 0);
    }
}
TEST(maskblt_rop4_all_fore_codes_vs_nop_back) {
    int f; NEED();
    for (f = 0; f < 256; f += 5) maskblt_case(f, 0xAA, m_rnd, 0, 0, 16, 0);
}
TEST(maskblt_rop4_all_back_codes_vs_copy_fore) {
    int b; NEED();
    for (b = 0; b < 256; b += 5) maskblt_case(0xCC, b, m_checker, 0, 0, 16, 0);
}
TEST(maskblt_null_mask_acts_as_bitblt) {
    BD s, d; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0x00FF00);
    BOOL r = pMaskBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, NULL, 0, 0, R4(0xCC, 0xAA));
    CHECK(r);
    int x, y, bad = 0;
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != enc(x, y)) bad++;
    CHECKF(!bad, "bad %d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(maskblt_null_mask_plain_srccopy) {
    BD s, d; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0);
    CHECK(pMaskBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, NULL, 0, 0, SRCCOPY));
    CHECK_PX(d, 4, 5, enc(4, 5), 0xFFFFFF);
    bd_free(&s); bd_free(&d);
}
TEST(maskblt_null_src_fails) {
    BD d; NEED(); d = bd_new(32, 8, 8);
    HBITMAP mk = make_mask(8, 8, m_checker);
    bd_clear(&d, 0x123456);
    SetLastError(0xDEAD);
    CHECK(!pMaskBlt(d.dc, 0, 0, 8, 8, NULL, 0, 0, mk, 0, 0, R4(0xCC, 0xAA)));
    t_note("MaskBlt NULL src err=%lu", GetLastError());
    CHECK(!pMaskBlt(d.dc, 0, 0, 8, 8, NULL, 0, 0, mk, 0, 0, R4(0xF0, 0xAA)));
    CHECK_RECT_VAL(d, 0, 0, 8, 8, 0x123456, 0xFFFFFF);
    pDeleteObject(mk); bd_free(&d);
}
TEST(maskblt_color_mask_rejected) {
    BD s, d; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_clear(&d, 0x112233); bd_fill_enc(&s);
    HBITMAP cm = pCreateBitmap(8, 8, 1, 32, NULL);
    SetLastError(0xDEAD);
    BOOL r = pMaskBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, cm, 0, 0, R4(0xCC, 0xAA));
    t_note("MaskBlt with 32bpp mask -> %d err=%lu", r, GetLastError());
    CHECK(!r);
    if (!r) CHECK_RECT_VAL(d, 0, 0, 8, 8, 0x112233, 0xFFFFFF);
    pDeleteObject(cm);
    bd_free(&s); bd_free(&d);
}
TEST(maskblt_color_mask_dib_rejected) {
    BD s, d, m; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8); m = bd_new(8, 8, 8);
    bd_clear(&d, 0x112233); bd_fill_enc(&s);
    SetLastError(0xDEAD);
    BOOL r = pMaskBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, m.bm, 0, 0, R4(0xCC, 0xAA));
    t_note("MaskBlt with 8bpp DIB mask -> %d err=%lu", r, GetLastError());
    bd_free(&s); bd_free(&d); bd_free(&m);
}
TEST(maskblt_mono_dibsection_mask) {
    BD s, d, m; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8); m = bd_new(1, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0x00FF00);
    int x, y; for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) bd_put(&m, x, y, (x + y) & 1);
    BOOL r = pMaskBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, m.bm, 0, 0, R4(0xCC, 0xAA));
    t_note("MaskBlt with 1bpp DIBSection mask -> %d", r);
    if (r) {
        int bad = 0;
        for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != (((x + y) & 1) ? enc(x, y) : 0x00FF00)) bad++;
        CHECKF(!bad, "bad %d", bad);
    }
    bd_free(&s); bd_free(&d); bd_free(&m);
}
TEST(maskblt_mask_too_small) {
    BD s, d; NEED();
    s = bd_new(32, 16, 16); d = bd_new(32, 16, 16);
    bd_fill_enc(&s); bd_clear(&d, 0x00FF00);
    HBITMAP mk = make_mask(4, 4, m_checker);
    SetLastError(0xDEAD);
    BOOL r = pMaskBlt(d.dc, 0, 0, 16, 16, s.dc, 0, 0, mk, 0, 0, R4(0xCC, 0xAA));
    t_note("MaskBlt mask smaller than blit rect -> %d err=%lu d(0,0)=%06lx d(10,10)=%06lx", r, GetLastError(), (unsigned long)(bd_get(&d, 0, 0) & 0xFFFFFF), (unsigned long)(bd_get(&d, 10, 10) & 0xFFFFFF));
    pDeleteObject(mk); bd_free(&s); bd_free(&d);
}
TEST(maskblt_bad_dest) {
    BD s; NEED(); s = bd_new(32, 4, 4);
    HBITMAP mk = make_mask(4, 4, m_ones);
    CHECK(!pMaskBlt(NULL, 0, 0, 4, 4, s.dc, 0, 0, mk, 0, 0, R4(0xCC, 0xAA)));
    CHECK(!pMaskBlt((HDC)(ULONG_PTR)0x3333, 0, 0, 4, 4, s.dc, 0, 0, mk, 0, 0, R4(0xCC, 0xAA)));
    CHECK(!pMaskBlt(s.dc, 0, 0, 4, 4, (HDC)(ULONG_PTR)0x3333, 0, 0, mk, 0, 0, R4(0xCC, 0xAA)));
    pDeleteObject(mk); bd_free(&s);
}
TEST(maskblt_bad_mask_handle) {
    BD s, d; NEED(); s = bd_new(32, 4, 4); d = bd_new(32, 4, 4);
    HBRUSH br = pCreateSolidBrush(0);
    SetLastError(0xDEAD);
    BOOL r = pMaskBlt(d.dc, 0, 0, 4, 4, s.dc, 0, 0, (HBITMAP)br, 0, 0, R4(0xCC, 0xAA));
    CHECK(!r);
    t_note("MaskBlt brush as mask err=%lu", GetLastError());
    r = pMaskBlt(d.dc, 0, 0, 4, 4, s.dc, 0, 0, (HBITMAP)(ULONG_PTR)0x7777, 0, 0, R4(0xCC, 0xAA));
    CHECK(!r);
    pDeleteObject(br); bd_free(&s); bd_free(&d);
}
TEST(maskblt_zero_size) {
    BD s, d; NEED(); s = bd_new(32, 4, 4); d = bd_new(32, 4, 4);
    HBITMAP mk = make_mask(4, 4, m_ones);
    bd_clear(&d, 0x99);
    BOOL r = pMaskBlt(d.dc, 0, 0, 0, 0, s.dc, 0, 0, mk, 0, 0, R4(0xCC, 0xAA));
    t_note("MaskBlt zero size -> %d", r);
    CHECK_RECT_VAL(d, 0, 0, 4, 4, 0x99, 0xFFFFFF);
    pDeleteObject(mk); bd_free(&s); bd_free(&d);
}
TEST(maskblt_mask_selected_in_dc_rejected_or_ok) {
    BD s, d; NEED(); s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    HBITMAP mk = make_mask(8, 8, m_checker);
    HDC mdc = pCreateCompatibleDC(NULL); HGDIOBJ old = pSelectObject(mdc, mk);
    bd_fill_enc(&s); bd_clear(&d, 0);
    BOOL r = pMaskBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, mk, 0, 0, R4(0xCC, 0xAA));
    t_note("MaskBlt with mask selected into another DC -> %d", r);
    pSelectObject(mdc, old); pDeleteDC(mdc); pDeleteObject(mk);
    bd_free(&s); bd_free(&d);
}
TEST(maskblt_24bpp_dest) {
    BD s, d; HBITMAP mk; int x, y, bad = 0; NEED();
    s = bd_new(24, 9, 9); d = bd_new(24, 9, 9);
    mk = make_mask(9, 9, m_checker);
    for (y = 0; y < 9; y++) for (x = 0; x < 9; x++) { bd_put(&s, x, y, enc(x, y)); bd_put(&d, x, y, 0x00FF00); }
    CHECK(pMaskBlt(d.dc, 0, 0, 9, 9, s.dc, 0, 0, mk, 0, 0, R4(0xCC, 0xAA)));
    for (y = 0; y < 9; y++) for (x = 0; x < 9; x++) if (bd_get(&d, x, y) != (m_checker(x, y) ? enc(x, y) : 0x00FF00)) bad++;
    CHECKF(!bad, "bad %d", bad);
    pDeleteObject(mk); bd_free(&s); bd_free(&d);
}
TEST(maskblt_1bpp_dest) {
    BD s, d; HBITMAP mk; int x, y, bad = 0; NEED();
    s = bd_new(1, 16, 4); d = bd_new(1, 16, 4);
    mk = make_mask(16, 4, m_cols);
    for (y = 0; y < 4; y++) for (x = 0; x < 16; x++) { bd_put(&s, x, y, (x ^ y) & 1); bd_put(&d, x, y, 1); }
    CHECK(pMaskBlt(d.dc, 0, 0, 16, 4, s.dc, 0, 0, mk, 0, 0, R4(0xCC, 0xAA)));
    for (y = 0; y < 4; y++) for (x = 0; x < 16; x++) if (bd_get(&d, x, y) != (DWORD)(m_cols(x, y) ? ((x ^ y) & 1) : 1)) bad++;
    CHECKF(!bad, "bad %d", bad);
    pDeleteObject(mk); bd_free(&s); bd_free(&d);
}
TEST(maskblt_clipped_by_region) {
    BD s, d; HBITMAP mk; int x, y, bad = 0; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    mk = make_mask(8, 8, m_ones);
    bd_fill_enc(&s); bd_clear(&d, 0);
    HRGN rg = pCreateRectRgn(2, 2, 6, 5); pSelectClipRgn(d.dc, rg);
    CHECK(pMaskBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, mk, 0, 0, R4(0xCC, 0xAA)));
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) {
        DWORD e = (x >= 2 && x < 6 && y >= 2 && y < 5) ? enc(x, y) : 0;
        if ((bd_get(&d, x, y) & 0xFFFFFF) != e) bad++;
    }
    CHECKF(!bad, "bad %d", bad);
    pSelectClipRgn(d.dc, NULL); pDeleteObject(rg); pDeleteObject(mk); bd_free(&s); bd_free(&d);
}
TEST(maskblt_ddb_source) {
    BD d; HBITMAP mk; int x, y, bad = 0; NEED();
    d = bd_new(32, 8, 8); mk = make_mask(8, 8, m_checker);
    HDC sdc = pCreateCompatibleDC(NULL);
    HBITMAP sb = pCreateBitmap(8, 8, 1, 32, NULL);
    HGDIOBJ so = pSelectObject(sdc, sb);
    HBRUSH br = pCreateSolidBrush(RGB(0x12, 0x34, 0x56)); HGDIOBJ bo = pSelectObject(sdc, br);
    CHECK(pPatBlt(sdc, 0, 0, 8, 8, PATCOPY));
    pSelectObject(sdc, bo); pDeleteObject(br);
    bd_clear(&d, 0xABCDEF);
    BOOL r = pMaskBlt(d.dc, 0, 0, 8, 8, sdc, 0, 0, mk, 0, 0, R4(0xCC, 0xAA));
    CHECK(r);
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != (m_checker(x, y) ? 0x123456 : 0xABCDEF)) bad++;
    CHECKF(!bad, "bad %d", bad);
    pSelectObject(sdc, so); pDeleteObject(sb); pDeleteDC(sdc); pDeleteObject(mk); bd_free(&d);
}

/* ---- PlgBlt ---- */
static void plg_setup(BD *s, BD *d, int sw, int sh, int dw, int dh) {
    *s = bd_new(32, sw, sh); *d = bd_new(32, dw, dh);
    bd_fill_enc(s); bd_clear(d, 0x00FF00);
}
TEST(plgblt_identity_rect) {
    BD s, d; int x, y, bad = 0; NEED(); plg_setup(&s, &d, 12, 10, 40, 40);
    POINT p[3] = { { 5, 7 }, { 17, 7 }, { 5, 17 } };
    CHECK(pPlgBlt(d.dc, p, s.dc, 0, 0, 12, 10, NULL, 0, 0));
    for (y = 0; y < 10; y++) for (x = 0; x < 12; x++) if ((bd_get(&d, 5 + x, 7 + y) & 0xFFFFFF) != enc(x, y)) bad++;
    CHECKF(!bad, "bad %d", bad);
    CHECK_PX(d, 4, 7, 0x00FF00, 0xFFFFFF); CHECK_PX(d, 17, 7, 0x00FF00, 0xFFFFFF); CHECK_PX(d, 5, 17, 0x00FF00, 0xFFFFFF); CHECK_PX(d, 5, 6, 0x00FF00, 0xFFFFFF);
    bd_free(&s); bd_free(&d);
}
TEST(plgblt_identity_with_source_offset) {
    BD s, d; int x, y, bad = 0; NEED(); plg_setup(&s, &d, 20, 20, 30, 30);
    POINT p[3] = { { 2, 3 }, { 10, 3 }, { 2, 9 } };
    CHECK(pPlgBlt(d.dc, p, s.dc, 6, 5, 8, 6, NULL, 0, 0));
    for (y = 0; y < 6; y++) for (x = 0; x < 8; x++) if ((bd_get(&d, 2 + x, 3 + y) & 0xFFFFFF) != enc(6 + x, 5 + y)) bad++;
    CHECKF(!bad, "bad %d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(plgblt_identity_does_not_touch_outside) {
    BD s, d; int x, y, n = 0; NEED(); plg_setup(&s, &d, 8, 8, 24, 24);
    POINT p[3] = { { 8, 8 }, { 16, 8 }, { 8, 16 } };
    CHECK(pPlgBlt(d.dc, p, s.dc, 0, 0, 8, 8, NULL, 0, 0));
    for (y = 0; y < 24; y++) for (x = 0; x < 24; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != 0x00FF00) n++;
    CHECKF(n == 64, "changed %d pixels", n);
    bd_free(&s); bd_free(&d);
}
TEST(plgblt_scale_2x_replicates) {
    BD s, d; int x, y, bad = 0; NEED(); plg_setup(&s, &d, 6, 6, 30, 30);
    POINT p[3] = { { 4, 4 }, { 16, 4 }, { 4, 16 } };
    CHECK(pPlgBlt(d.dc, p, s.dc, 0, 0, 6, 6, NULL, 0, 0));
    for (y = 0; y < 12; y++) for (x = 0; x < 12; x++) if ((bd_get(&d, 4 + x, 4 + y) & 0xFFFFFF) != enc(x / 2, y / 2)) bad++;
    if (bad) t_note("PlgBlt 2x scale: %d pixels differ from replication", bad);
    int n = 0; for (y = 0; y < 30; y++) for (x = 0; x < 30; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != 0x00FF00) n++;
    CHECKF(n == 144, "covered %d", n);
    bd_free(&s); bd_free(&d);
}
TEST(plgblt_hflip) {
    BD s, d; int pos[100][2], sx, sy, bad = 0; NEED(); plg_setup(&s, &d, 8, 8, 30, 30);
    POINT p[3] = { { 18, 4 }, { 10, 4 }, { 18, 12 } };
    CHECK(pPlgBlt(d.dc, p, s.dc, 0, 0, 8, 8, NULL, 0, 0));
    int cnt = 0, x, y;
    for (y = 0; y < 30; y++) for (x = 0; x < 30; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != 0x00FF00) cnt++;
    t_note("PlgBlt hflip covered %d", cnt);
    CHECK(cnt == 64);
    for (sy = 0; sy < 8; sy++) for (sx = 0; sx < 8; sx++) pos[sy * 8 + sx][0] = -1;
    for (y = 0; y < 30; y++) for (x = 0; x < 30; x++) for (sy = 0; sy < 8; sy++) for (sx = 0; sx < 8; sx++) if ((bd_get(&d, x, y) & 0xFFFFFF) == enc(sx, sy)) { pos[sy * 8 + sx][0] = x; pos[sy * 8 + sx][1] = y; }
    int k = -100;
    for (sy = 0; sy < 8; sy++) for (sx = 0; sx < 8; sx++) {
        if (pos[sy * 8 + sx][0] < 0) { bad++; continue; }
        if (pos[sy * 8 + sx][1] != 4 + sy) bad++;
        int kk = pos[sy * 8 + sx][0] + sx; if (k == -100) k = kk; else if (kk != k) bad++;
    }
    CHECKF(!bad, "hflip inconsistent %d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(plgblt_vflip) {
    BD s, d; int sx, sy, bad = 0, pos[100][2]; NEED(); plg_setup(&s, &d, 8, 8, 30, 30);
    POINT p[3] = { { 10, 12 }, { 18, 12 }, { 10, 4 } };
    CHECK(pPlgBlt(d.dc, p, s.dc, 0, 0, 8, 8, NULL, 0, 0));
    int x, y, k = -100;
    for (sy = 0; sy < 8; sy++) for (sx = 0; sx < 8; sx++) pos[sy * 8 + sx][0] = -1;
    for (y = 0; y < 30; y++) for (x = 0; x < 30; x++) for (sy = 0; sy < 8; sy++) for (sx = 0; sx < 8; sx++) if ((bd_get(&d, x, y) & 0xFFFFFF) == enc(sx, sy)) { pos[sy * 8 + sx][0] = x; pos[sy * 8 + sx][1] = y; }
    for (sy = 0; sy < 8; sy++) for (sx = 0; sx < 8; sx++) {
        if (pos[sy * 8 + sx][0] < 0) { bad++; continue; }
        if (pos[sy * 8 + sx][0] != 10 + sx) bad++;
        int kk = pos[sy * 8 + sx][1] + sy; if (k == -100) k = kk; else if (kk != k) bad++;
    }
    CHECKF(!bad, "vflip inconsistent %d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(plgblt_rotate90_covers_square) {
    BD s, d; int x, y, n = 0; NEED(); plg_setup(&s, &d, 8, 8, 30, 30);
    POINT p[3] = { { 20, 4 }, { 20, 12 }, { 12, 4 } };   /* src UL->(20,4), UR->(20,12), LL->(12,4) */
    BOOL r = pPlgBlt(d.dc, p, s.dc, 0, 0, 8, 8, NULL, 0, 0);
    CHECK(r);
    for (y = 0; y < 30; y++) for (x = 0; x < 30; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != 0x00FF00) n++;
    t_note("PlgBlt rot90 covered %d pixels; (19,4)=%06lx (12,11)=%06lx", n, (unsigned long)(bd_get(&d, 19, 4) & 0xFFFFFF), (unsigned long)(bd_get(&d, 12, 11) & 0xFFFFFF));
    CHECKF(n >= 56 && n <= 80, "covered %d", n);
    /* corner mapping: the source UL pixel lands at the dest corner nearest pt[0] */
    int found = 0;
    for (y = 3; y < 6; y++) for (x = 18; x < 21; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) == enc(0, 0)) found = 1;
    CHECK(found);
    bd_free(&s); bd_free(&d);
}
TEST(plgblt_mask_keeps_dest_where_zero) {
    BD s, d; HBITMAP mk; int x, y, bad = 0; NEED(); plg_setup(&s, &d, 8, 8, 24, 24);
    mk = make_mask(8, 8, m_checker);
    POINT p[3] = { { 4, 4 }, { 12, 4 }, { 4, 12 } };
    CHECK(pPlgBlt(d.dc, p, s.dc, 0, 0, 8, 8, mk, 0, 0));
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) if ((bd_get(&d, 4 + x, 4 + y) & 0xFFFFFF) != (m_checker(x, y) ? enc(x, y) : 0x00FF00)) bad++;
    CHECKF(!bad, "bad %d", bad);
    pDeleteObject(mk); bd_free(&s); bd_free(&d);
}
TEST(plgblt_mask_offset) {
    BD s, d; HBITMAP mk; int x, y, bad = 0; NEED(); plg_setup(&s, &d, 8, 8, 24, 24);
    mk = make_mask(16, 16, m_rnd);
    POINT p[3] = { { 4, 4 }, { 12, 4 }, { 4, 12 } };
    CHECK(pPlgBlt(d.dc, p, s.dc, 0, 0, 8, 8, mk, 5, 3));
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) if ((bd_get(&d, 4 + x, 4 + y) & 0xFFFFFF) != (m_rnd(x + 5, y + 3) ? enc(x, y) : 0x00FF00)) bad++;
    CHECKF(!bad, "bad %d", bad);
    pDeleteObject(mk); bd_free(&s); bd_free(&d);
}
TEST(plgblt_color_mask_rejected) {
    BD s, d; NEED(); plg_setup(&s, &d, 8, 8, 24, 24);
    HBITMAP cm = pCreateBitmap(8, 8, 1, 32, NULL);
    POINT p[3] = { { 4, 4 }, { 12, 4 }, { 4, 12 } };
    SetLastError(0xDEAD);
    BOOL r = pPlgBlt(d.dc, p, s.dc, 0, 0, 8, 8, cm, 0, 0);
    t_note("PlgBlt color mask -> %d err=%lu", r, GetLastError());
    CHECK(!r);
    pDeleteObject(cm); bd_free(&s); bd_free(&d);
}
TEST(plgblt_degenerate_parallelogram) {
    BD s, d; NEED(); plg_setup(&s, &d, 8, 8, 24, 24);
    POINT p[3] = { { 4, 4 }, { 12, 12 }, { 8, 8 } };
    SetLastError(0xDEAD);
    BOOL r = pPlgBlt(d.dc, p, s.dc, 0, 0, 8, 8, NULL, 0, 0);
    t_note("PlgBlt collinear points -> %d err=%lu", r, GetLastError());
    POINT q[3] = { { 4, 4 }, { 4, 4 }, { 4, 4 } };
    r = pPlgBlt(d.dc, q, s.dc, 0, 0, 8, 8, NULL, 0, 0);
    t_note("PlgBlt identical points -> %d", r);
    bd_free(&s); bd_free(&d);
}
TEST(plgblt_zero_and_negative_source_size) {
    BD s, d; NEED(); plg_setup(&s, &d, 8, 8, 24, 24);
    POINT p[3] = { { 4, 4 }, { 12, 4 }, { 4, 12 } };
    int w[][2] = { { 0, 8 }, { 8, 0 }, { -8, 8 }, { 8, -8 } }, i;
    for (i = 0; i < 4; i++) {
        bd_clear(&d, 0x00FF00);
        SetLastError(0xDEAD);
        BOOL r = pPlgBlt(d.dc, p, s.dc, 0, 0, w[i][0], w[i][1], NULL, 0, 0);
        t_note("PlgBlt src size %dx%d -> %d err=%lu", w[i][0], w[i][1], r, GetLastError());
    }
    bd_free(&s); bd_free(&d);
}
TEST(plgblt_bad_dcs) {
    BD s, d; NEED(); plg_setup(&s, &d, 8, 8, 24, 24);
    POINT p[3] = { { 4, 4 }, { 12, 4 }, { 4, 12 } };
    CHECK(!pPlgBlt(NULL, p, s.dc, 0, 0, 8, 8, NULL, 0, 0));
    CHECK(!pPlgBlt(d.dc, p, (HDC)(ULONG_PTR)0x5555, 0, 0, 8, 8, NULL, 0, 0));
    SetLastError(0xDEAD);
    BOOL r = pPlgBlt(d.dc, p, NULL, 0, 0, 8, 8, NULL, 0, 0);
    t_note("PlgBlt NULL src -> %d err=%lu", r, GetLastError());
    CHECK(!r);
    bd_free(&s); bd_free(&d);
}
TEST(plgblt_clipped_by_region) {
    BD s, d; int x, y, bad = 0; NEED(); plg_setup(&s, &d, 8, 8, 24, 24);
    POINT p[3] = { { 4, 4 }, { 12, 4 }, { 4, 12 } };
    HRGN rg = pCreateRectRgn(6, 5, 10, 9); pSelectClipRgn(d.dc, rg);
    CHECK(pPlgBlt(d.dc, p, s.dc, 0, 0, 8, 8, NULL, 0, 0));
    for (y = 0; y < 24; y++) for (x = 0; x < 24; x++) {
        int in = x >= 4 && x < 12 && y >= 4 && y < 12 && x >= 6 && x < 10 && y >= 5 && y < 9;
        DWORD e = in ? enc(x - 4, y - 4) : 0x00FF00;
        if ((bd_get(&d, x, y) & 0xFFFFFF) != e) bad++;
    }
    CHECKF(!bad, "bad %d", bad);
    pSelectClipRgn(d.dc, NULL); pDeleteObject(rg); bd_free(&s); bd_free(&d);
}
