/* PatBlt: source-less raster operations on DIBSection memory DCs */
#include "blt.h"

static int count_val(BD *b, int x0, int y0, int w, int h, DWORD v) {
    int n = 0, x, y;
    for (y = y0; y < y0 + h; y++) for (x = x0; x < x0 + w; x++) if ((bd_get(b, x, y) & 0xFFFFFF) == (v & 0xFFFFFF)) n++;
    return n;
}
static int src_used(unsigned code) { return ((code << 2) ^ code) & 0xCC; }

static HBRUSH sel_solid(BD *b, DWORD P, HGDIOBJ *old) { HBRUSH br = pCreateSolidBrush(cr(P)); *old = pSelectObject(b->dc, br); return br; }
static void unsel(BD *b, HBRUSH br, HGDIOBJ old) { pSelectObject(b->dc, old); pDeleteObject(br); }

TEST(patblt_patcopy_rect_exact) {
    BD d; NEED(); d = bd_new(32, 16, 16);
    bd_clear(&d, 0x112233);
    HGDIOBJ o; HBRUSH br = sel_solid(&d, 0xAA5511, &o);
    CHECK(pPatBlt(d.dc, 3, 4, 7, 5, PATCOPY));
    CHECK_RECT_VAL(d, 3, 4, 7, 5, 0xAA5511, 0xFFFFFF);
    CHECKF(count_val(&d, 0, 0, 16, 16, 0x112233) == 256 - 35, "border untouched count %d", count_val(&d, 0, 0, 16, 16, 0x112233));
    CHECK_PX(d, 2, 4, 0x112233, 0xFFFFFF); CHECK_PX(d, 10, 4, 0x112233, 0xFFFFFF);
    CHECK_PX(d, 3, 3, 0x112233, 0xFFFFFF); CHECK_PX(d, 3, 9, 0x112233, 0xFFFFFF);
    unsel(&d, br, o); bd_free(&d);
}
TEST(patblt_1x1) {
    BD d; NEED(); d = bd_new(32, 8, 8); bd_clear(&d, 0);
    HGDIOBJ o; HBRUSH br = sel_solid(&d, 0xFFFFFF, &o);
    CHECK(pPatBlt(d.dc, 5, 6, 1, 1, PATCOPY));
    CHECKF(count_val(&d, 0, 0, 8, 8, 0xFFFFFF) == 1, "n=%d", count_val(&d, 0, 0, 8, 8, 0xFFFFFF));
    CHECK_PX(d, 5, 6, 0xFFFFFF, 0xFFFFFF);
    unsel(&d, br, o); bd_free(&d);
}
TEST(patblt_patinvert) {
    BD d; NEED(); d = bd_new(32, 8, 8); bd_clear(&d, 0x00FF00);
    HGDIOBJ o; HBRUSH br = sel_solid(&d, 0x0F0F0F, &o);
    CHECK(pPatBlt(d.dc, 0, 0, 8, 8, PATINVERT));
    CHECK_RECT_VAL(d, 0, 0, 8, 8, 0x0FF00F, 0xFFFFFF);
    CHECK(pPatBlt(d.dc, 0, 0, 8, 8, PATINVERT));
    CHECK_RECT_VAL(d, 0, 0, 8, 8, 0x00FF00, 0xFFFFFF);
    unsel(&d, br, o); bd_free(&d);
}
TEST(patblt_dstinvert) {
    BD d; NEED(); d = bd_new(32, 8, 8); bd_clear(&d, 0x123456);
    CHECK(pPatBlt(d.dc, 2, 2, 4, 4, DSTINVERT));
    CHECK_RECT_VAL(d, 2, 2, 4, 4, 0xEDCBA9, 0xFFFFFF);
    CHECK_PX(d, 1, 1, 0x123456, 0xFFFFFF); CHECK_PX(d, 6, 6, 0x123456, 0xFFFFFF);
    bd_free(&d);
}
TEST(patblt_blackness_whiteness) {
    BD d; NEED(); d = bd_new(32, 8, 8); bd_clear(&d, 0x123456);
    CHECK(pPatBlt(d.dc, 0, 0, 4, 8, BLACKNESS));
    CHECK(pPatBlt(d.dc, 4, 0, 4, 8, WHITENESS));
    CHECK_RECT_VAL(d, 0, 0, 4, 8, 0, 0xFFFFFF);
    CHECK_RECT_VAL(d, 4, 0, 4, 8, 0xFFFFFF, 0xFFFFFF);
    bd_free(&d);
}
TEST(patblt_all_sourceless_rops) {
    BD d; int code; NEED(); d = bd_new(32, 4, 4);
    for (code = 0; code < 256; code++) {
        if (src_used(code)) continue;
        DWORD P = 0x3C5A96, D = 0xA1B2C3;
        HGDIOBJ o; HBRUSH br = sel_solid(&d, P, &o);
        bd_clear(&d, D);
        BOOL r = pPatBlt(d.dc, 0, 0, 4, 4, ROP(code));
        DWORD e = rop3(code, P, 0, D) & 0xFFFFFF;
        CHECKF(r && count_val(&d, 0, 0, 4, 4, e) == 16, "rop %02X ret %d want 0x%lx got 0x%lx", code, r, (unsigned long)e, (unsigned long)(bd_get(&d, 1, 1) & 0xFFFFFF));
        unsel(&d, br, o);
    }
    bd_free(&d);
}
TEST(patblt_sourceless_rops_random_dest) {
    BD d; int code, x, y; NEED(); d = bd_new(32, 6, 6);
    for (code = 0; code < 256; code++) {
        if (src_used(code)) continue;
        DWORD P = rnd() & 0xFFFFFF, dv[36]; HGDIOBJ o; HBRUSH br = sel_solid(&d, P, &o);
        for (y = 0; y < 6; y++) for (x = 0; x < 6; x++) bd_put(&d, x, y, dv[y * 6 + x] = rnd() & 0xFFFFFF);
        BOOL r = pPatBlt(d.dc, 0, 0, 6, 6, ROP(code)); int bad = 0;
        for (y = 0; y < 6; y++) for (x = 0; x < 6; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != (rop3(code, P, 0, dv[y * 6 + x]) & 0xFFFFFF)) bad++;
        CHECKF(r && !bad, "rop %02X ret %d bad %d", code, r, bad);
        unsel(&d, br, o);
    }
    bd_free(&d);
}
TEST(patblt_source_rops_rejected) {
    BD d; NEED(); d = bd_new(32, 4, 4);
    static const DWORD rops[] = { SRCCOPY, SRCPAINT, SRCAND, SRCINVERT, SRCERASE, NOTSRCCOPY, NOTSRCERASE, MERGECOPY, MERGEPAINT, PATPAINT };
    int i;
    for (i = 0; i < 10; i++) {
        HGDIOBJ o; HBRUSH br = sel_solid(&d, 0x808080, &o);
        bd_clear(&d, 0x404040);
        SetLastError(0xDEAD);
        BOOL r = pPatBlt(d.dc, 0, 0, 4, 4, rops[i]);
        t_note("PatBlt rop %08lx -> %d err=%lu pixel=0x%06lx", (unsigned long)rops[i], r, GetLastError(), (unsigned long)(bd_get(&d, 0, 0) & 0xFFFFFF));
        unsel(&d, br, o);
    }
    bd_free(&d);
}
TEST(patblt_default_brush_is_white) {
    BD d; NEED(); d = bd_new(32, 4, 4); bd_clear(&d, 0x102030);
    CHECK(pPatBlt(d.dc, 0, 0, 4, 4, PATCOPY));
    CHECK_RECT_VAL(d, 0, 0, 4, 4, 0xFFFFFF, 0xFFFFFF);
    bd_free(&d);
}
TEST(patblt_stock_brushes) {
    BD d; NEED(); d = bd_new(32, 4, 4);
    static const struct { int id; DWORD v; } t[] = { { WHITE_BRUSH, 0xFFFFFF }, { BLACK_BRUSH, 0 } };
    int i;
    for (i = 0; i < 2; i++) {
        HGDIOBJ o = pSelectObject(d.dc, pGetStockObject(t[i].id));
        bd_clear(&d, 0x808080);
        CHECK(pPatBlt(d.dc, 0, 0, 4, 4, PATCOPY));
        CHECK_RECT_VAL(d, 0, 0, 4, 4, t[i].v, 0xFFFFFF);
        pSelectObject(d.dc, o);
    }
    bd_free(&d);
}
TEST(patblt_null_brush_draws_nothing) {
    BD d; NEED(); d = bd_new(32, 4, 4); bd_clear(&d, 0x808080);
    HGDIOBJ o = pSelectObject(d.dc, pGetStockObject(NULL_BRUSH));
    BOOL r = pPatBlt(d.dc, 0, 0, 4, 4, PATCOPY);
    t_note("PatBlt with NULL_BRUSH -> %d", r);
    CHECK_RECT_VAL(d, 0, 0, 4, 4, 0x808080, 0xFFFFFF);
    pSelectObject(d.dc, o); bd_free(&d);
}
TEST(patblt_zero_extents) {
    BD d; NEED(); d = bd_new(32, 8, 8);
    HGDIOBJ o; HBRUSH br = sel_solid(&d, 0xFFFFFF, &o);
    bd_clear(&d, 0x222222);
    BOOL a = pPatBlt(d.dc, 2, 2, 0, 4, PATCOPY), b = pPatBlt(d.dc, 2, 2, 4, 0, PATCOPY), c = pPatBlt(d.dc, 2, 2, 0, 0, PATCOPY);
    t_note("PatBlt zero w/h/both -> %d %d %d", a, b, c);
    CHECK_RECT_VAL(d, 0, 0, 8, 8, 0x222222, 0xFFFFFF);
    unsel(&d, br, o); bd_free(&d);
}
TEST(patblt_negative_extents_cover_same_area) {
    BD d; NEED(); d = bd_new(32, 16, 16);
    HGDIOBJ o; HBRUSH br = sel_solid(&d, 0xFFFFFF, &o);
    static const int c[][4] = { { 8, 8, -4, 3 }, { 8, 8, 4, -3 }, { 8, 8, -4, -3 } };
    int i;
    for (i = 0; i < 3; i++) {
        bd_clear(&d, 0);
        BOOL r = pPatBlt(d.dc, c[i][0], c[i][1], c[i][2], c[i][3], PATCOPY);
        int n = count_val(&d, 0, 0, 16, 16, 0xFFFFFF);
        CHECKF(r && n == 12, "case %d ret %d count %d", i, r, n);
        int minx = 99, maxx = -1, miny = 99, maxy = -1, x, y;
        for (y = 0; y < 16; y++) for (x = 0; x < 16; x++) if (bd_get(&d, x, y) & 0xFFFFFF) { if (x < minx) minx = x; if (x > maxx) maxx = x; if (y < miny) miny = y; if (y > maxy) maxy = y; }
        t_note("PatBlt(8,8,%d,%d) bbox x %d..%d y %d..%d", c[i][2], c[i][3], minx, maxx, miny, maxy);
    }
    unsel(&d, br, o); bd_free(&d);
}
TEST(patblt_clipped_to_bitmap) {
    BD d; NEED(); d = bd_new(32, 16, 16);
    HGDIOBJ o; HBRUSH br = sel_solid(&d, 0xCAFE01, &o);
    bd_clear(&d, 0);
    BOOL r = pPatBlt(d.dc, -5, -5, 10, 10, PATCOPY);
    CHECK(r);
    CHECK_RECT_VAL(d, 0, 0, 5, 5, 0xCAFE01, 0xFFFFFF);
    CHECKF(count_val(&d, 0, 0, 16, 16, 0xCAFE01) == 25, "n=%d", count_val(&d, 0, 0, 16, 16, 0xCAFE01));
    bd_clear(&d, 0);
    r = pPatBlt(d.dc, 12, 12, 100, 100, PATCOPY);
    CHECK(r);
    CHECKF(count_val(&d, 0, 0, 16, 16, 0xCAFE01) == 16, "n=%d", count_val(&d, 0, 0, 16, 16, 0xCAFE01));
    bd_clear(&d, 0);
    r = pPatBlt(d.dc, 100, 100, 10, 10, PATCOPY);
    t_note("fully outside -> %d", r);
    CHECKF(count_val(&d, 0, 0, 16, 16, 0) == 256, "outside draws nothing");
    r = pPatBlt(d.dc, -100000, -100000, 200000, 200000, PATCOPY);
    t_note("huge covering -> %d", r);
    if (r) CHECKF(count_val(&d, 0, 0, 16, 16, 0xCAFE01) == 256, "huge covering fills all");
    unsel(&d, br, o); bd_free(&d);
}
TEST(patblt_huge_extents_no_hang) {
    BD d; NEED(); d = bd_new(32, 8, 8);
    HGDIOBJ o; HBRUSH br = sel_solid(&d, 0xFFFFFF, &o);
    bd_clear(&d, 0);
    BOOL r = pPatBlt(d.dc, 0, 0, 0x7FFFFFF, 0x7FFFFFF, PATCOPY);
    t_note("PatBlt 0x7FFFFFF -> %d", r);
    if (r) CHECK_RECT_VAL(d, 0, 0, 8, 8, 0xFFFFFF, 0xFFFFFF);
    unsel(&d, br, o); bd_free(&d);
}
TEST(patblt_bad_dc) {
    NEED();
    HBITMAP bm = pCreateBitmap(4, 4, 1, 1, NULL);
    HBRUSH br = pCreateSolidBrush(0);
    HDC cases[] = { NULL, (HDC)(ULONG_PTR)0x12345, (HDC)bm, (HDC)br, (HDC)(ULONG_PTR)-1, (HDC)pGetStockObject(WHITE_BRUSH) };
    int i;
    for (i = 0; i < 6; i++) {
        SetLastError(0xDEAD);
        BOOL r = pPatBlt(cases[i], 0, 0, 4, 4, PATCOPY);
        CHECKF(!r, "case %d returned %d", i, r);
        t_note("PatBlt bad dc case %d err=%lu", i, GetLastError());
    }
    pDeleteObject(bm); pDeleteObject(br);
}
TEST(patblt_deleted_dc) {
    NEED();
    HDC dc = pCreateCompatibleDC(NULL);
    CHECK(pDeleteDC(dc));
    CHECK(!pPatBlt(dc, 0, 0, 4, 4, PATCOPY));
    SetLastError(0);
    t_note("PatBlt deleted DC err=%lu", GetLastError());
}
TEST(patblt_dc_without_bitmap) {
    /* a fresh compatible DC owns a 1x1 mono bitmap */
    NEED();
    HDC dc = pCreateCompatibleDC(NULL);
    BOOL r = pPatBlt(dc, 0, 0, 4, 4, BLACKNESS);
    CHECK(r);
    HGDIOBJ cur = pGetCurrentObject(dc, OBJ_BITMAP);
    BITMAP bm; memset(&bm, 0, sizeof bm);
    pGetObjectA(cur, sizeof bm, &bm);
    CHECKF(bm.bmWidth == 1 && bm.bmHeight == 1 && bm.bmBitsPixel == 1, "default bitmap %ldx%ld bpp %d", bm.bmWidth, bm.bmHeight, bm.bmBitsPixel);
    pDeleteDC(dc);
}
TEST(patblt_clip_region) {
    BD d; NEED(); d = bd_new(32, 16, 16);
    HGDIOBJ o; HBRUSH br = sel_solid(&d, 0xFFFFFF, &o);
    bd_clear(&d, 0);
    HRGN rg = pCreateRectRgn(4, 4, 10, 8);
    CHECK(pSelectClipRgn(d.dc, rg) != ERROR);
    CHECK(pPatBlt(d.dc, 0, 0, 16, 16, PATCOPY));
    CHECK_RECT_VAL(d, 4, 4, 6, 4, 0xFFFFFF, 0xFFFFFF);
    CHECKF(count_val(&d, 0, 0, 16, 16, 0xFFFFFF) == 24, "n=%d", count_val(&d, 0, 0, 16, 16, 0xFFFFFF));
    pSelectClipRgn(d.dc, NULL);
    bd_clear(&d, 0);
    CHECK(pPatBlt(d.dc, 0, 0, 16, 16, PATCOPY));
    CHECK(count_val(&d, 0, 0, 16, 16, 0xFFFFFF) == 256);
    pDeleteObject(rg); unsel(&d, br, o); bd_free(&d);
}
TEST(patblt_empty_clip_region) {
    BD d; NEED(); d = bd_new(32, 8, 8);
    HGDIOBJ o; HBRUSH br = sel_solid(&d, 0xFFFFFF, &o);
    bd_clear(&d, 0x10);
    HRGN rg = pCreateRectRgn(0, 0, 0, 0);
    int rc = pSelectClipRgn(d.dc, rg);
    CHECKF(rc == NULLREGION, "SelectClipRgn empty -> %d", rc);
    BOOL r = pPatBlt(d.dc, 0, 0, 8, 8, PATCOPY);
    t_note("PatBlt with empty clip -> %d", r);
    CHECK_RECT_VAL(d, 0, 0, 8, 8, 0x10, 0xFFFFFF);
    pSelectClipRgn(d.dc, NULL); pDeleteObject(rg); unsel(&d, br, o); bd_free(&d);
}
TEST(patblt_clip_complex_region) {
    BD d; NEED(); d = bd_new(32, 16, 16);
    HGDIOBJ o; HBRUSH br = sel_solid(&d, 0xFFFFFF, &o);
    bd_clear(&d, 0);
    HRGN a = pCreateRectRgn(0, 0, 4, 16), b = pCreateRectRgn(8, 0, 12, 16);
    int (WINAPI *comb)(HRGN, HRGN, HRGN, int) = (void *)pCombineRgn;
    if (!comb) { t_skip("CombineRgn"); unsel(&d, br, o); bd_free(&d); pDeleteObject(a); pDeleteObject(b); return; }
    HRGN u = pCreateRectRgn(0, 0, 1, 1);
    CHECK(comb(u, a, b, RGN_OR) == COMPLEXREGION);
    pSelectClipRgn(d.dc, u);
    CHECK(pPatBlt(d.dc, 0, 0, 16, 16, PATCOPY));
    CHECK_RECT_VAL(d, 0, 0, 4, 16, 0xFFFFFF, 0xFFFFFF);
    CHECK_RECT_VAL(d, 4, 0, 4, 16, 0, 0xFFFFFF);
    CHECK_RECT_VAL(d, 8, 0, 4, 16, 0xFFFFFF, 0xFFFFFF);
    CHECK_RECT_VAL(d, 12, 0, 4, 16, 0, 0xFFFFFF);
    pSelectClipRgn(d.dc, NULL);
    pDeleteObject(a); pDeleteObject(b); pDeleteObject(u); unsel(&d, br, o); bd_free(&d);
}
TEST(patblt_mm_anisotropic_scale) {
    BD d; NEED(); d = bd_new(32, 32, 32);
    HGDIOBJ o; HBRUSH br = sel_solid(&d, 0xFFFFFF, &o);
    bd_clear(&d, 0);
    CHECK(pSetMapMode(d.dc, MM_ANISOTROPIC) == MM_TEXT);
    pSetWindowExtEx(d.dc, 1, 1, NULL); pSetViewportExtEx(d.dc, 2, 3, NULL);
    CHECK(pPatBlt(d.dc, 2, 1, 4, 5, PATCOPY));
    CHECK_RECT_VAL(d, 4, 3, 8, 15, 0xFFFFFF, 0xFFFFFF);
    CHECKF(count_val(&d, 0, 0, 32, 32, 0xFFFFFF) == 8 * 15, "n=%d", count_val(&d, 0, 0, 32, 32, 0xFFFFFF));
    pSetMapMode(d.dc, MM_TEXT);
    unsel(&d, br, o); bd_free(&d);
}
TEST(patblt_mm_origin_offset) {
    BD d; NEED(); d = bd_new(32, 32, 32);
    HGDIOBJ o; HBRUSH br = sel_solid(&d, 0xFFFFFF, &o);
    bd_clear(&d, 0);
    pSetViewportOrgEx(d.dc, 10, 7, NULL);
    CHECK(pPatBlt(d.dc, 1, 2, 3, 4, PATCOPY));
    CHECK_RECT_VAL(d, 11, 9, 3, 4, 0xFFFFFF, 0xFFFFFF);
    CHECKF(count_val(&d, 0, 0, 32, 32, 0xFFFFFF) == 12, "n");
    pSetViewportOrgEx(d.dc, 0, 0, NULL);
    pSetWindowOrgEx(d.dc, 5, 5, NULL);
    bd_clear(&d, 0);
    CHECK(pPatBlt(d.dc, 6, 6, 2, 2, PATCOPY));
    CHECK_RECT_VAL(d, 1, 1, 2, 2, 0xFFFFFF, 0xFFFFFF);
    pSetWindowOrgEx(d.dc, 0, 0, NULL);
    unsel(&d, br, o); bd_free(&d);
}
TEST(patblt_mm_flipped_y) {
    BD d; NEED(); d = bd_new(32, 32, 32);
    HGDIOBJ o; HBRUSH br = sel_solid(&d, 0xFFFFFF, &o);
    bd_clear(&d, 0);
    pSetMapMode(d.dc, MM_ANISOTROPIC);
    pSetWindowExtEx(d.dc, 1, 1, NULL); pSetViewportExtEx(d.dc, 1, -1, NULL); pSetViewportOrgEx(d.dc, 0, 31, NULL);
    BOOL r = pPatBlt(d.dc, 2, 3, 4, 5, PATCOPY);
    CHECK(r);
    int n = count_val(&d, 0, 0, 32, 32, 0xFFFFFF);
    CHECKF(n == 20, "count %d", n);
    int miny = 99, maxy = -1, x, y;
    for (y = 0; y < 32; y++) for (x = 0; x < 32; x++) if (bd_get(&d, x, y) & 0xFFFFFF) { if (y < miny) miny = y; if (y > maxy) maxy = y; }
    t_note("flipped-y PatBlt(2,3,4,5) rows %d..%d", miny, maxy);
    CHECKF(maxy <= 28 && miny >= 23, "rows %d..%d inside expected band", miny, maxy);
    pSetMapMode(d.dc, MM_TEXT); pSetViewportOrgEx(d.dc, 0, 0, NULL);
    unsel(&d, br, o); bd_free(&d);
}
TEST(patblt_hatch_period) {
    NEED();
    HBRUSH (WINAPI *chb)(int, COLORREF) = (void *)pCreateHatchBrush;
    REQUIRE(chb);
    static const int styles[] = { HS_HORIZONTAL, HS_VERTICAL, HS_FDIAGONAL, HS_BDIAGONAL, HS_CROSS, HS_DIAGCROSS };
    static const int expect8[] = { 8, 8, 8, 8, 15, 16 };
    int i;
    for (i = 0; i < 6; i++) {
        BD d = bd_new(32, 16, 16);
        HBRUSH br = chb(styles[i], RGB(255, 255, 255)); HGDIOBJ o = pSelectObject(d.dc, br);
        pSetBkColor(d.dc, RGB(0, 0, 0));
        pSetBkMode(d.dc, OPAQUE);
        bd_clear(&d, 0x404040);
        CHECK(pPatBlt(d.dc, 0, 0, 16, 16, PATCOPY));
        int nw = count_val(&d, 0, 0, 16, 16, 0xFFFFFF), nb = count_val(&d, 0, 0, 16, 16, 0);
        CHECKF(nw + nb == 256, "style %d: only fg/bk colors (%d+%d)", styles[i], nw, nb);
        CHECKF(nw == 4 * expect8[i], "style %d fg count %d want %d", styles[i], nw, 4 * expect8[i]);
        /* period 8 */
        int x, y, bad = 0;
        for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) { DWORD v = bd_get(&d, x, y); if (v != bd_get(&d, x + 8, y) || v != bd_get(&d, x, y + 8)) bad++; }
        CHECKF(!bad, "style %d not periodic", styles[i]);
        pSelectObject(d.dc, o); pDeleteObject(br); bd_free(&d);
    }
}
TEST(patblt_hatch_transparent_bkmode) {
    NEED();
    HBRUSH (WINAPI *chb)(int, COLORREF) = (void *)pCreateHatchBrush;
    REQUIRE(chb);
    BD d = bd_new(32, 16, 16);
    HBRUSH br = chb(HS_HORIZONTAL, RGB(255, 255, 255)); HGDIOBJ o = pSelectObject(d.dc, br);
    pSetBkColor(d.dc, RGB(255, 0, 0)); pSetBkMode(d.dc, TRANSPARENT);
    bd_clear(&d, 0x404040);
    CHECK(pPatBlt(d.dc, 0, 0, 16, 16, PATCOPY));
    int nw = count_val(&d, 0, 0, 16, 16, 0xFFFFFF), nd = count_val(&d, 0, 0, 16, 16, 0x404040);
    CHECKF(nw == 32 && nd == 224, "white %d dark %d", nw, nd);
    pSelectObject(d.dc, o); pDeleteObject(br); bd_free(&d);
}
TEST(patblt_hatch_opaque_bkcolor) {
    NEED();
    HBRUSH (WINAPI *chb)(int, COLORREF) = (void *)pCreateHatchBrush;
    REQUIRE(chb);
    BD d = bd_new(32, 16, 16);
    HBRUSH br = chb(HS_VERTICAL, RGB(255, 255, 255)); HGDIOBJ o = pSelectObject(d.dc, br);
    pSetBkColor(d.dc, RGB(0x10, 0x20, 0x30)); pSetBkMode(d.dc, OPAQUE);
    bd_clear(&d, 0x404040);
    CHECK(pPatBlt(d.dc, 0, 0, 16, 16, PATCOPY));
    CHECKF(count_val(&d, 0, 0, 16, 16, 0x102030) == 224 && count_val(&d, 0, 0, 16, 16, 0xFFFFFF) == 32, "bk %d fg %d", count_val(&d, 0, 0, 16, 16, 0x102030), count_val(&d, 0, 0, 16, 16, 0xFFFFFF));
    pSelectObject(d.dc, o); pDeleteObject(br); bd_free(&d);
}
TEST(patblt_pattern_brush_origin) {
    BD d, pt; int x, y; NEED();
    d = bd_new(32, 24, 24); pt = bd_new(32, 8, 8);
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) bd_put(&pt, x, y, 0x010000 * (x + 1) + 0x000100 * (y + 1));
    HBRUSH br = pCreatePatternBrush(pt.bm); HGDIOBJ o = pSelectObject(d.dc, br);
    static const int org[][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 3, 5 }, { 7, 7 }, { 8, 8 }, { -1, -1 }, { 100, 203 } };
    int k;
    for (k = 0; k < 8; k++) {
        int ox = org[k][0], oy = org[k][1];
        POINT old;
        CHECK(pSetBrushOrgEx(d.dc, ox, oy, &old));
        CHECK(pPatBlt(d.dc, 0, 0, 24, 24, PATCOPY));
        int bad = 0;
        for (y = 0; y < 24; y++) for (x = 0; x < 24; x++)
            if ((bd_get(&d, x, y) & 0xFFFFFF) != (bd_get(&pt, ((x - ox) % 8 + 8) % 8, ((y - oy) % 8 + 8) % 8) & 0xFFFFFF)) bad++;
        CHECKF(!bad, "origin (%d,%d): %d pixels differ", ox, oy, bad);
        POINT now; pGetBrushOrgEx(d.dc, &now);
        CHECKF(now.x == ox && now.y == oy, "GetBrushOrgEx (%ld,%ld)", now.x, now.y);
    }
    pSetBrushOrgEx(d.dc, 0, 0, NULL);
    pSelectObject(d.dc, o); pDeleteObject(br); bd_free(&d); bd_free(&pt);
}
TEST(patblt_brush_origin_returns_previous) {
    BD d; NEED(); d = bd_new(32, 4, 4);
    POINT p = { -7, -7 };
    CHECK(pSetBrushOrgEx(d.dc, 5, 6, &p));
    CHECKF(p.x == 0 && p.y == 0, "initial origin (%ld,%ld)", p.x, p.y);
    CHECK(pSetBrushOrgEx(d.dc, 9, 10, &p));
    CHECKF(p.x == 5 && p.y == 6, "prev (%ld,%ld)", p.x, p.y);
    CHECK(pSetBrushOrgEx(d.dc, 1, 1, NULL));
    SetLastError(0xDEAD);
    CHECK(!pSetBrushOrgEx(NULL, 1, 1, &p));
    CHECK(!pGetBrushOrgEx(NULL, &p));
    bd_free(&d);
}
TEST(patblt_dither_solid_brush_32bpp_exact) {
    BD d; int i; NEED(); d = bd_new(32, 8, 8);
    for (i = 0; i < 64; i++) {
        DWORD c = rnd() & 0xFFFFFF; HGDIOBJ o; HBRUSH br = sel_solid(&d, c, &o);
        CHECK(pPatBlt(d.dc, 0, 0, 8, 8, PATCOPY));
        CHECKF(count_val(&d, 0, 0, 8, 8, c) == 64, "color 0x%06lx not exact", (unsigned long)c);
        unsel(&d, br, o);
    }
    bd_free(&d);
}
TEST(patblt_24bpp_exact_colors) {
    BD d; int i; NEED(); d = bd_new(24, 8, 8);
    for (i = 0; i < 32; i++) {
        DWORD c = rnd() & 0xFFFFFF; HGDIOBJ o; HBRUSH br = sel_solid(&d, c, &o);
        CHECK(pPatBlt(d.dc, 1, 1, 6, 6, PATCOPY));
        CHECKF(count_val(&d, 1, 1, 6, 6, c) == 36, "color 0x%06lx", (unsigned long)c);
        unsel(&d, br, o);
    }
    bd_free(&d);
}
TEST(patblt_16bpp_555) {
    BD d; NEED(); d = bd_new16(8, 8, 0);
    HGDIOBJ o; HBRUSH br = sel_solid(&d, 0xFFFFFF, &o);
    CHECK(pPatBlt(d.dc, 0, 0, 8, 8, PATCOPY));
    CHECK_RECT_VAL(d, 0, 0, 8, 8, 0x7FFF, 0x7FFF);
    unsel(&d, br, o); br = sel_solid(&d, 0, &o);
    CHECK(pPatBlt(d.dc, 0, 0, 8, 8, PATCOPY));
    CHECK_RECT_VAL(d, 0, 0, 8, 8, 0, 0x7FFF);
    unsel(&d, br, o);
    br = sel_solid(&d, 0xFF0000, &o);
    CHECK(pPatBlt(d.dc, 0, 0, 8, 8, PATCOPY));
    CHECK_RECT_VAL(d, 0, 0, 8, 8, 0x7C00, 0x7FFF);
    unsel(&d, br, o); bd_free(&d);
}
TEST(patblt_16bpp_565_primaries) {
    BD d; NEED(); d = bd_new16(8, 8, 1);
    static const struct { DWORD c, v; } t[] = { { 0xFF0000, 0xF800 }, { 0x00FF00, 0x07E0 }, { 0x0000FF, 0x001F }, { 0xFFFFFF, 0xFFFF }, { 0, 0 } };
    int i;
    for (i = 0; i < 5; i++) {
        HGDIOBJ o; HBRUSH br = sel_solid(&d, t[i].c, &o);
        CHECK(pPatBlt(d.dc, 0, 0, 8, 8, PATCOPY));
        CHECK_RECT_VAL(d, 0, 0, 8, 8, t[i].v, 0xFFFF);
        unsel(&d, br, o);
    }
    bd_free(&d);
}
TEST(patblt_8bpp_exact_palette_match) {
    BD d; int i; NEED(); d = bd_new(8, 8, 8);
    for (i = 0; i < 256; i += 5) {
        COLORREF c = blt_palcol(8, i);
        HBRUSH br = pCreateSolidBrush(c); HGDIOBJ o = pSelectObject(d.dc, br);
        CHECK(pPatBlt(d.dc, 0, 0, 8, 8, PATCOPY));
        CHECKF(bd_get(&d, 3, 3) == (DWORD)i && bd_get(&d, 7, 7) == (DWORD)i, "index %d got %lu", i, (unsigned long)bd_get(&d, 3, 3));
        pSelectObject(d.dc, o); pDeleteObject(br);
    }
    bd_free(&d);
}
TEST(patblt_4bpp_exact_palette_match) {
    BD d; int i; NEED(); d = bd_new(4, 8, 8);
    for (i = 0; i < 16; i++) {
        COLORREF c = blt_palcol(4, i);
        HBRUSH br = pCreateSolidBrush(c); HGDIOBJ o = pSelectObject(d.dc, br);
        CHECK(pPatBlt(d.dc, 1, 1, 5, 5, PATCOPY));
        CHECKF(bd_get(&d, 1, 1) == (DWORD)i && bd_get(&d, 5, 5) == (DWORD)i, "index %d got %lu", i, (unsigned long)bd_get(&d, 3, 3));
        CHECK_PX(d, 0, 0, 0, 0xF); CHECK_PX(d, 6, 6, 0, 0xF);
        pSelectObject(d.dc, o); pDeleteObject(br);
    }
    bd_free(&d);
}
TEST(patblt_1bpp_black_white) {
    BD d; NEED(); d = bd_new(1, 16, 4);
    HGDIOBJ o; HBRUSH br = sel_solid(&d, 0xFFFFFF, &o);
    CHECK(pPatBlt(d.dc, 3, 0, 9, 4, PATCOPY));
    int x, y, bad = 0;
    for (y = 0; y < 4; y++) for (x = 0; x < 16; x++) if (bd_get(&d, x, y) != (DWORD)(x >= 3 && x < 12)) bad++;
    CHECKF(!bad, "bad %d", bad);
    unsel(&d, br, o);
    br = sel_solid(&d, 0, &o);
    CHECK(pPatBlt(d.dc, 4, 1, 3, 2, PATCOPY));
    CHECK_RECT_VAL(d, 4, 1, 3, 2, 0, 1);
    unsel(&d, br, o); bd_free(&d);
}
TEST(patblt_ddb_compatible_bitmap) {
    NEED();
    HDC dc = pCreateCompatibleDC(NULL);
    HBITMAP bm = pCreateBitmap(8, 8, 1, 1, NULL);
    HGDIOBJ old = pSelectObject(dc, bm);
    CHECK(pPatBlt(dc, 0, 0, 8, 8, WHITENESS));
    BYTE bits[16]; memset(bits, 0, sizeof bits);
    LONG n = pGetBitmapBits(bm, sizeof bits, bits);
    CHECKF(n == 16, "GetBitmapBits %ld", n);
    int i, bad = 0; for (i = 0; i < 16; i += 2) if (bits[i] != 0xFF) bad++;
    CHECK(bad == 0);
    CHECK(pPatBlt(dc, 0, 0, 8, 8, BLACKNESS));
    pGetBitmapBits(bm, sizeof bits, bits);
    bad = 0; for (i = 0; i < 16; i += 2) if (bits[i] != 0) bad++;
    CHECK(bad == 0);
    pSelectObject(dc, old); pDeleteObject(bm); pDeleteDC(dc);
}
TEST(patblt_bitblt_equivalence_for_pattern_rops) {
    BD a, b; NEED();
    a = bd_new(32, 12, 12); b = bd_new(32, 12, 12);
    static const DWORD rops[] = { PATCOPY, PATINVERT, DSTINVERT, BLACKNESS, WHITENESS };
    int i, x, y;
    for (i = 0; i < 5; i++) {
        HGDIOBJ o1, o2; HBRUSH b1 = sel_solid(&a, 0x7A3C1D, &o1), b2 = sel_solid(&b, 0x7A3C1D, &o2);
        for (y = 0; y < 12; y++) for (x = 0; x < 12; x++) { DWORD v = rnd() & 0xFFFFFF; bd_put(&a, x, y, v); bd_put(&b, x, y, v); }
        CHECK(pPatBlt(a.dc, 2, 3, 7, 6, rops[i]));
        CHECK(pBitBlt(b.dc, 2, 3, 7, 6, NULL, 0, 0, rops[i]));
        CHECKF(memcmp(a.raw, b.raw, (size_t)a.stride * 12) == 0, "rop %08lx differs", (unsigned long)rops[i]);
        unsel(&a, b1, o1); unsel(&b, b2, o2);
    }
    bd_free(&a); bd_free(&b);
}
TEST(patblt_repeated_calls_stable) {
    BD d; int i; NEED(); d = bd_new(32, 8, 8);
    HGDIOBJ o; HBRUSH br = sel_solid(&d, 0xFFFFFF, &o);
    bd_clear(&d, 0);
    for (i = 0; i < 1000; i++) if (!pPatBlt(d.dc, 0, 0, 8, 8, PATINVERT)) { CHECKF(0, "iteration %d failed", i); break; }
    CHECK_RECT_VAL(d, 0, 0, 8, 8, 0, 0xFFFFFF);
    unsel(&d, br, o); bd_free(&d);
}
TEST(patblt_rop2_does_not_affect) {
    /* SetROP2 governs lines, not PatBlt */
    BD d; NEED(); d = bd_new(32, 4, 4);
    HGDIOBJ o; HBRUSH br = sel_solid(&d, 0x123456, &o);
    pSetROP2(d.dc, R2_BLACK);
    bd_clear(&d, 0xFFFFFF);
    CHECK(pPatBlt(d.dc, 0, 0, 4, 4, PATCOPY));
    CHECK_RECT_VAL(d, 0, 0, 4, 4, 0x123456, 0xFFFFFF);
    pSetROP2(d.dc, R2_COPYPEN);
    unsel(&d, br, o); bd_free(&d);
}
TEST(patblt_savedc_restore_brush) {
    BD d; NEED(); d = bd_new(32, 4, 4);
    HGDIOBJ o; HBRUSH br = sel_solid(&d, 0x00AA00, &o);
    int id = pSaveDC(d.dc);
    HBRUSH br2 = pCreateSolidBrush(0x0000FF);
    pSelectObject(d.dc, br2);
    CHECK(pPatBlt(d.dc, 0, 0, 4, 4, PATCOPY));
    CHECK_PX(d, 1, 1, 0xFF0000, 0xFFFFFF);
    CHECK(pRestoreDC(d.dc, id));
    CHECK(pPatBlt(d.dc, 0, 0, 4, 4, PATCOPY));
    CHECK_PX(d, 1, 1, 0x00AA00, 0xFFFFFF);
    pDeleteObject(br2); unsel(&d, br, o); bd_free(&d);
}
