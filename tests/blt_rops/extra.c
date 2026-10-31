/* additional BitBlt/PatBlt/StretchBlt edge cases: clipping at bitmap edges, state interplay */
#include "blt.h"


TEST(extra_bitblt_random_rects_vs_reference) {
    BD s, d; int it; NEED();
    s = bd_new(32, 24, 24); d = bd_new(32, 24, 24);
    if (!bd_ok(&s) || !bd_ok(&d)) { CHECK(0); bd_free(&s); bd_free(&d); return; }
    for (it = 0; it < 120; it++) {
        static const unsigned codes[] = { 0xCC, 0x66, 0x88, 0xEE, 0x33, 0x11, 0x44, 0xBB, 0xC0, 0xB8, 0xE2, 0x5A, 0xF0, 0x55 };
        unsigned code = codes[rnd() % 14];
        int dx = (int)(rnd() % 40) - 12, dy = (int)(rnd() % 40) - 12, sx = (int)(rnd() % 12), sy = (int)(rnd() % 12);
        int w = 1 + rnd() % 14, h = 1 + rnd() % 14, x, y, bad = 0;
        DWORD P = rnd() & 0xFFFFFF, sv[24 * 24], dv[24 * 24];
        if (sx + w > 24) w = 24 - sx;
        if (sy + h > 24) h = 24 - sy;
        for (y = 0; y < 24; y++) for (x = 0; x < 24; x++) { bd_put(&s, x, y, rnd() & 0xFFFFFF); bd_put(&d, x, y, rnd() & 0xFFFFFF); sv[y * 24 + x] = bd_get(&s, x, y) & 0xFFFFFF; dv[y * 24 + x] = bd_get(&d, x, y) & 0xFFFFFF; }
        HBRUSH br = pCreateSolidBrush(cr(P)); HGDIOBJ ob = pSelectObject(d.dc, br);
        BOOL r = pBitBlt(d.dc, dx, dy, w, h, s.dc, sx, sy, ROP(code));
        pSelectObject(d.dc, ob); pDeleteObject(br);
        for (y = 0; y < 24 && !bad; y++) for (x = 0; x < 24; x++) {
            DWORD e = dv[y * 24 + x];
            if (x >= dx && x < dx + w && y >= dy && y < dy + h) e = rop3(code, P, sv[(y - dy + sy) * 24 + x - dx + sx], e) & 0xFFFFFF;
            if ((bd_get(&d, x, y) & 0xFFFFFF) != e) { bad = 1; CHECKF(0, "it %d rop %02X d(%d,%d) %dx%d s(%d,%d) diff at (%d,%d) got %06lx want %06lx", it, code, dx, dy, w, h, sx, sy, x, y, (unsigned long)(bd_get(&d, x, y) & 0xFFFFFF), (unsigned long)e); break; }
        }
        CHECKF(r, "it %d ret %d", it, r);
    }
    bd_free(&s); bd_free(&d);
}

TEST(extra_patblt_negative_origin_clips_to_corner) {
    BD d; NEED(); d = bd_new(32, 12, 12);
    bd_clear(&d, 0x102030);
    HBRUSH br = pCreateSolidBrush(RGB(1, 2, 3)); HGDIOBJ ob = pSelectObject(d.dc, br);
    CHECK(pPatBlt(d.dc, -3, -2, 8, 8, PATCOPY));
    CHECK_RECT_VAL(d, 0, 0, 5, 6, 0x010203, 0xFFFFFF);
    CHECK_PX(d, 5, 0, 0x102030, 0xFFFFFF); CHECK_PX(d, 0, 6, 0x102030, 0xFFFFFF); CHECK_PX(d, 11, 11, 0x102030, 0xFFFFFF);
    CHECK(pPatBlt(d.dc, 10, 10, 50, 50, PATCOPY));
    CHECK_RECT_VAL(d, 10, 10, 2, 2, 0x010203, 0xFFFFFF);
    CHECK_PX(d, 9, 10, 0x102030, 0xFFFFFF);
    pSelectObject(d.dc, ob); pDeleteObject(br); bd_free(&d);
}

TEST(extra_bitblt_ignores_text_and_bk_colors_for_color_source) {
    BD s, d; int x, y, bad = 0; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_fill_enc(&s);
    pSetBkColor(d.dc, RGB(255, 0, 0)); pSetTextColor(d.dc, RGB(0, 255, 0));
    pSetBkColor(s.dc, RGB(0, 0, 255)); pSetTextColor(s.dc, RGB(9, 9, 9));
    CHECK(pBitBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, SRCCOPY));
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != enc(x, y)) bad++;
    CHECKF(!bad, "%d bad", bad);
    bd_free(&s); bd_free(&d);
}

TEST(extra_bottom_up_dib_source_same_logical_image) {
    BD t, d1, d2; BI256 bi; void *bits = NULL; int x, y, bad = 0; HDC dc; HBITMAP bm, old; NEED();
    t = bd_new(32, 9, 7); d1 = bd_new(32, 9, 7); d2 = bd_new(32, 9, 7);
    bd_fill_enc(&t);
    memset(&bi, 0, sizeof bi);
    bi.h.biSize = sizeof(BITMAPINFOHEADER); bi.h.biWidth = 9; bi.h.biHeight = 7; bi.h.biPlanes = 1; bi.h.biBitCount = 32;
    dc = pCreateCompatibleDC(NULL);
    bm = pCreateDIBSection(dc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    REQUIRE(bm && bits);
    for (y = 0; y < 7; y++) for (x = 0; x < 9; x++) ((DWORD *)bits)[(6 - y) * 9 + x] = enc(x, y);
    old = pSelectObject(dc, bm);
    CHECK(pBitBlt(d1.dc, 0, 0, 9, 7, t.dc, 0, 0, SRCCOPY));
    CHECK(pBitBlt(d2.dc, 0, 0, 9, 7, dc, 0, 0, SRCCOPY));
    for (y = 0; y < 7; y++) for (x = 0; x < 9; x++) if ((bd_get(&d1, x, y) & 0xFFFFFF) != (bd_get(&d2, x, y) & 0xFFFFFF)) bad++;
    CHECKF(!bad, "%d bad", bad);
    pSelectObject(dc, old); pDeleteObject(bm); pDeleteDC(dc);
    bd_free(&t); bd_free(&d1); bd_free(&d2);
}

TEST(extra_savedc_restore_undoes_clip_for_blit) {
    BD s, d; int id; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_clear(&s, 0xABCDEF); bd_clear(&d, 0);
    id = pSaveDC(d.dc); CHECK(id > 0);
    HRGN rg = pCreateRectRgn(2, 2, 4, 4);
    CHECK(pSelectClipRgn(d.dc, rg) != 0);
    CHECK(pBitBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, SRCCOPY));
    CHECK_RECT_VAL(d, 2, 2, 2, 2, 0xABCDEF, 0xFFFFFF);
    CHECK_PX(d, 0, 0, 0, 0xFFFFFF); CHECK_PX(d, 4, 4, 0, 0xFFFFFF);
    CHECK(pRestoreDC(d.dc, id));
    CHECK(pBitBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, SRCCOPY));
    CHECK_RECT_VAL(d, 0, 0, 8, 8, 0xABCDEF, 0xFFFFFF);
    pDeleteObject(rg);
    bd_free(&s); bd_free(&d);
}

TEST(extra_exclude_clip_rect_blit) {
    BD s, d; typeof(&ExtSelectClipRgn) pExt; int x, y, bad = 0; NEED();
    pExt = U(ExtSelectClipRgn); REQUIRE(pExt);
    s = bd_new(32, 10, 10); d = bd_new(32, 10, 10);
    bd_clear(&s, 0x00FFFF); bd_clear(&d, 0);
    HRGN rg = pCreateRectRgn(3, 3, 7, 7);
    CHECK(pExt(d.dc, rg, RGN_DIFF) != ERROR);
    CHECK(pBitBlt(d.dc, 0, 0, 10, 10, s.dc, 0, 0, SRCCOPY));
    for (y = 0; y < 10; y++) for (x = 0; x < 10; x++) {
        DWORD e = (x >= 3 && x < 7 && y >= 3 && y < 7) ? 0 : 0x00FFFF;
        if ((bd_get(&d, x, y) & 0xFFFFFF) != e) bad++;
    }
    CHECKF(!bad, "%d bad", bad);
    CHECK(pSelectClipRgn(d.dc, NULL) != ERROR);
    pDeleteObject(rg);
    bd_free(&s); bd_free(&d);
}

TEST(extra_stretch_1to1_matches_bitblt_all_depths) {
    static const int dep[] = { 32, 24, 16, 8 };
    int i; NEED();
    for (i = 0; i < 4; i++) {
        BD s, a, b; int x, y, bad = 0;
        s = bd_new(dep[i], 17, 11); a = bd_new(dep[i], 17, 11); b = bd_new(dep[i], 17, 11);
        if (!bd_ok(&s) || !bd_ok(&a) || !bd_ok(&b)) { CHECK(0); bd_free(&s); bd_free(&a); bd_free(&b); continue; }
        for (y = 0; y < 11; y++) for (x = 0; x < 17; x++) bd_put(&s, x, y, rnd() & bd_mask(dep[i]));
        CHECK(pBitBlt(a.dc, 0, 0, 17, 11, s.dc, 0, 0, SRCCOPY));
        CHECK(pStretchBlt(b.dc, 0, 0, 17, 11, s.dc, 0, 0, 17, 11, SRCCOPY));
        for (y = 0; y < 11; y++) for (x = 0; x < 17; x++) {
            if (bd_get(&a, x, y) != bd_get(&b, x, y)) bad++;
            if (dep[i] >= 24 || dep[i] == 8) if (bd_get(&a, x, y) != (bd_get(&s, x, y) & bd_mask(dep[i]))) bad++;
        }
        CHECKF(!bad, "depth %d: %d bad", dep[i], bad);
        bd_free(&s); bd_free(&a); bd_free(&b);
    }
}

TEST(extra_getpixel_after_patblt) {
    BD d; NEED(); d = bd_new(32, 8, 8);
    HBRUSH br = pCreateSolidBrush(RGB(12, 34, 56)); HGDIOBJ ob = pSelectObject(d.dc, br);
    CHECK(pPatBlt(d.dc, 0, 0, 8, 8, PATCOPY));
    pGdiFlush();
    CHECKF(pGetPixel(d.dc, 3, 3) == RGB(12, 34, 56), "got %06lx", (unsigned long)pGetPixel(d.dc, 3, 3));
    CHECK(pGetPixel(d.dc, 8, 8) == CLR_INVALID);
    CHECK(pGetPixel(d.dc, -1, 0) == CLR_INVALID);
    pSelectObject(d.dc, ob); pDeleteObject(br); bd_free(&d);
}

TEST(extra_blit_does_not_change_selected_objects) {
    BD s, d; NEED();
    s = bd_new(32, 4, 4); d = bd_new(32, 4, 4);
    HBRUSH br = pCreateSolidBrush(RGB(5, 6, 7)); HGDIOBJ ob = pSelectObject(d.dc, br);
    HGDIOBJ b0 = pGetCurrentObject(d.dc, OBJ_BITMAP), br0 = pGetCurrentObject(d.dc, OBJ_BRUSH);
    CHECK(pBitBlt(d.dc, 0, 0, 4, 4, s.dc, 0, 0, MERGECOPY));
    CHECK(pStretchBlt(d.dc, 0, 0, 4, 4, s.dc, 0, 0, 2, 2, SRCCOPY));
    CHECK(pPatBlt(d.dc, 0, 0, 4, 4, PATINVERT));
    CHECK(pGetCurrentObject(d.dc, OBJ_BITMAP) == b0);
    CHECK(pGetCurrentObject(d.dc, OBJ_BRUSH) == br0);
    CHECK(br0 == (HGDIOBJ)br);
    pSelectObject(d.dc, ob); pDeleteObject(br);
    bd_free(&s); bd_free(&d);
}

TEST(extra_bitblt_invalid_rop_bits_do_not_hang) {
    BD s, d; DWORD codes[] = { 0x00CC0020, 0x00CC0000, 0x00CC00FF, 0x01CC0020, 0xFFFFFFFF, 0x00000000 };
    int i; NEED();
    s = bd_new(32, 4, 4); d = bd_new(32, 4, 4);
    bd_clear(&s, 1); bd_clear(&d, 2);
    for (i = 0; i < 6; i++) {
        SetLastError(0xDEAD);
        BOOL r = pBitBlt(d.dc, 0, 0, 4, 4, s.dc, 0, 0, codes[i]);
        t_note("BitBlt rop %08lx -> %d err=%lu d=%06lx", (unsigned long)codes[i], r, GetLastError(), (unsigned long)(bd_get(&d, 0, 0) & 0xFFFFFF));
        bd_clear(&d, 2);
    }
    CHECK(1);
    bd_free(&s); bd_free(&d);
}

TEST(extra_stretch_up2x_24bpp_replication) {
    BD s, d; int x, y, bad = 0; NEED();
    s = bd_new(24, 6, 5); d = bd_new(24, 12, 10);
    for (y = 0; y < 5; y++) for (x = 0; x < 6; x++) bd_put(&s, x, y, enc(x, y));
    pSetStretchBltMode(d.dc, COLORONCOLOR);
    CHECK(pStretchBlt(d.dc, 0, 0, 12, 10, s.dc, 0, 0, 6, 5, SRCCOPY));
    for (y = 0; y < 10; y++) for (x = 0; x < 12; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != enc(x / 2, y / 2)) bad++;
    CHECKF(!bad, "%d bad", bad);
    bd_free(&s); bd_free(&d);
}

TEST(extra_repeated_blits_no_handle_growth) {
    BD s, d; int i; DWORD a, b; NEED(); REQUIRE(pGetGuiResources);
    s = bd_new(32, 16, 16); d = bd_new(32, 16, 16);
    bd_fill_enc(&s);
    a = pGetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    for (i = 0; i < 2000; i++) {
        pBitBlt(d.dc, 0, 0, 16, 16, s.dc, 0, 0, SRCCOPY);
        pStretchBlt(d.dc, 0, 0, 16, 16, s.dc, 0, 0, 8, 8, SRCINVERT);
        pPatBlt(d.dc, 1, 1, 3, 3, DSTINVERT);
    }
    b = pGetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    CHECKF(a == b, "gdi objects %lu -> %lu", (unsigned long)a, (unsigned long)b);
    bd_free(&s); bd_free(&d);
}
