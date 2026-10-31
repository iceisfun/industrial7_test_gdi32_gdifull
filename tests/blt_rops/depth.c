/* Cross-depth conversion, mono<->colour, DDBs, mapping modes, layout, errors, lifetimes */
#include "blt.h"

static int ch(DWORD v, int sh) { return (v >> sh) & 255; }
static int count_v(BD *b, int x0, int y0, int w, int h, DWORD v) {
    int n = 0, x, y;
    for (y = y0; y < y0 + h; y++) for (x = x0; x < x0 + w; x++) if ((bd_get(b, x, y) & 0xFFFFFF) == (v & 0xFFFFFF)) n++;
    return n;
}

/* ---- 32 -> 16 ---- */
TEST(depth_32_to_565_representable_values) {
    BD s, d; int i, bad = 0; NEED();
    s = bd_new(32, 8, 1); d = bd_new16(8, 1, 1);
    for (i = 0; i < 64; i++) {
        int r5 = rnd() & 31, g6 = rnd() & 63, b5 = rnd() & 31;
        DWORD c = (r5 << 19) | (g6 << 10) | (b5 << 3);
        bd_put(&s, 0, 0, c);
        CHECK(pBitBlt(d.dc, 0, 0, 1, 1, s.dc, 0, 0, SRCCOPY));
        DWORD e = (r5 << 11) | (g6 << 5) | b5;
        if (bd_get(&d, 0, 0) != e) bad++;
    }
    CHECKF(!bad, "%d mismatches", bad);
    bd_free(&s); bd_free(&d);
}
TEST(depth_32_to_565_white_black) {
    BD s, d; NEED();
    s = bd_new(32, 2, 1); d = bd_new16(2, 1, 1);
    bd_put(&s, 0, 0, 0xFFFFFF); bd_put(&s, 1, 0, 0);
    CHECK(pBitBlt(d.dc, 0, 0, 2, 1, s.dc, 0, 0, SRCCOPY));
    CHECK(bd_get(&d, 0, 0) == 0xFFFF && bd_get(&d, 1, 0) == 0);
    bd_free(&s); bd_free(&d);
}
TEST(depth_32_to_565_rounding_observed) {
    BD s, d; int v, trunc = 0, round_ = 0, other = 0; NEED();
    s = bd_new(32, 1, 1); d = bd_new16(1, 1, 1);
    for (v = 0; v < 256; v++) {
        bd_put(&s, 0, 0, (DWORD)v << 16);   /* red ramp */
        pBitBlt(d.dc, 0, 0, 1, 1, s.dc, 0, 0, SRCCOPY);
        int got = (bd_get(&d, 0, 0) >> 11) & 31, t = v >> 3, r = (v * 31 + 127) / 255;
        if (got == t) trunc++; else if (got == r) round_++; else other++;
    }
    t_note("32->565 red ramp: truncation matches %d, rounding matches %d, other %d", trunc, round_, other);
    CHECKF(other == 0, "%d values map to neither truncation nor rounding", other);
    bd_free(&s); bd_free(&d);
}
TEST(depth_32_to_565_monotonic_all_channels) {
    BD s, d; int v, sh, bad = 0; NEED();
    s = bd_new(32, 1, 1); d = bd_new16(1, 1, 1);
    for (sh = 0; sh < 3; sh++) {
        int prev = -1;
        for (v = 0; v < 256; v++) {
            bd_put(&s, 0, 0, (DWORD)v << (sh * 8));
            pBitBlt(d.dc, 0, 0, 1, 1, s.dc, 0, 0, SRCCOPY);
            DWORD g = bd_get(&d, 0, 0);
            int c = sh == 2 ? (g >> 11) & 31 : sh == 1 ? (g >> 5) & 63 : g & 31;
            if (c < prev) bad++;
            prev = c;
        }
    }
    CHECKF(!bad, "%d non-monotonic steps", bad);
    bd_free(&s); bd_free(&d);
}
TEST(depth_32_to_555_representable_values) {
    BD s, d; int i, bad = 0; NEED();
    s = bd_new(32, 1, 1); d = bd_new16(1, 1, 0);
    for (i = 0; i < 64; i++) {
        int r5 = rnd() & 31, g5 = rnd() & 31, b5 = rnd() & 31;
        bd_put(&s, 0, 0, (r5 << 19) | (g5 << 11) | (b5 << 3));
        CHECK(pBitBlt(d.dc, 0, 0, 1, 1, s.dc, 0, 0, SRCCOPY));
        if ((bd_get(&d, 0, 0) & 0x7FFF) != (DWORD)((r5 << 10) | (g5 << 5) | b5)) bad++;
    }
    CHECKF(!bad, "%d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(depth_16_555_to_32_endpoints) {
    BD s, d; NEED();
    s = bd_new16(4, 1, 0); d = bd_new(32, 4, 1);
    bd_put(&s, 0, 0, 0); bd_put(&s, 1, 0, 0x7FFF); bd_put(&s, 2, 0, 0x7C00); bd_put(&s, 3, 0, 0x001F);
    CHECK(pBitBlt(d.dc, 0, 0, 4, 1, s.dc, 0, 0, SRCCOPY));
    CHECK_PX(d, 0, 0, 0, 0xFFFFFF); CHECK_PX(d, 1, 0, 0xFFFFFF, 0xFFFFFF);
    CHECK_PX(d, 2, 0, 0xFF0000, 0xFFFFFF); CHECK_PX(d, 3, 0, 0x0000FF, 0xFFFFFF);
    bd_free(&s); bd_free(&d);
}
TEST(depth_16_565_to_32_endpoints) {
    BD s, d; NEED();
    s = bd_new16(4, 1, 1); d = bd_new(32, 4, 1);
    bd_put(&s, 0, 0, 0); bd_put(&s, 1, 0, 0xFFFF); bd_put(&s, 2, 0, 0xF800); bd_put(&s, 3, 0, 0x07E0);
    CHECK(pBitBlt(d.dc, 0, 0, 4, 1, s.dc, 0, 0, SRCCOPY));
    CHECK_PX(d, 0, 0, 0, 0xFFFFFF); CHECK_PX(d, 1, 0, 0xFFFFFF, 0xFFFFFF);
    CHECK_PX(d, 2, 0, 0xFF0000, 0xFFFFFF); CHECK_PX(d, 3, 0, 0x00FF00, 0xFFFFFF);
    bd_free(&s); bd_free(&d);
}
TEST(depth_16_565_roundtrip_stable) {
    BD a, m, b; int i, bad = 0; NEED();
    a = bd_new16(1, 1, 1); m = bd_new(32, 1, 1); b = bd_new16(1, 1, 1);
    for (i = 0; i < 200; i++) {
        DWORD v = rnd() & 0xFFFF;
        bd_put(&a, 0, 0, v);
        pBitBlt(m.dc, 0, 0, 1, 1, a.dc, 0, 0, SRCCOPY);
        pBitBlt(b.dc, 0, 0, 1, 1, m.dc, 0, 0, SRCCOPY);
        if (bd_get(&b, 0, 0) != v) bad++;
    }
    CHECKF(!bad, "%d values changed by 565->32->565", bad);
    bd_free(&a); bd_free(&m); bd_free(&b);
}
TEST(depth_16_555_roundtrip_stable) {
    BD a, m, b; int i, bad = 0; NEED();
    a = bd_new16(1, 1, 0); m = bd_new(32, 1, 1); b = bd_new16(1, 1, 0);
    for (i = 0; i < 200; i++) {
        DWORD v = rnd() & 0x7FFF;
        bd_put(&a, 0, 0, v);
        pBitBlt(m.dc, 0, 0, 1, 1, a.dc, 0, 0, SRCCOPY);
        pBitBlt(b.dc, 0, 0, 1, 1, m.dc, 0, 0, SRCCOPY);
        if ((bd_get(&b, 0, 0) & 0x7FFF) != v) bad++;
    }
    CHECKF(!bad, "%d values changed", bad);
    bd_free(&a); bd_free(&m); bd_free(&b);
}
TEST(depth_555_to_565_direct) {
    BD s, d; int i, bad = 0; NEED();
    s = bd_new16(1, 1, 0); d = bd_new16(1, 1, 1);
    for (i = 0; i < 100; i++) {
        int r = rnd() & 31, g = rnd() & 31, b = rnd() & 31;
        bd_put(&s, 0, 0, (r << 10) | (g << 5) | b);
        pBitBlt(d.dc, 0, 0, 1, 1, s.dc, 0, 0, SRCCOPY);
        DWORD g565 = bd_get(&d, 0, 0);
        if (((g565 >> 11) & 31) != (DWORD)r || (g565 & 31) != (DWORD)b) bad++;
    }
    CHECKF(!bad, "%d red/blue mismatches", bad);
    bd_free(&s); bd_free(&d);
}

/* ---- 24 / 32 ---- */
TEST(depth_32_to_24_and_back_exact) {
    BD a, b, c; int x, y, bad = 0; NEED();
    a = bd_new(32, 9, 7); b = bd_new(24, 9, 7); c = bd_new(32, 9, 7);
    for (y = 0; y < 7; y++) for (x = 0; x < 9; x++) bd_put(&a, x, y, rnd() & 0xFFFFFF);
    CHECK(pBitBlt(b.dc, 0, 0, 9, 7, a.dc, 0, 0, SRCCOPY));
    CHECK(pBitBlt(c.dc, 0, 0, 9, 7, b.dc, 0, 0, SRCCOPY));
    for (y = 0; y < 7; y++) for (x = 0; x < 9; x++) {
        if (bd_get(&b, x, y) != (bd_get(&a, x, y) & 0xFFFFFF)) bad++;
        if ((bd_get(&c, x, y) & 0xFFFFFF) != (bd_get(&a, x, y) & 0xFFFFFF)) bad++;
    }
    CHECKF(!bad, "%d", bad);
    bd_free(&a); bd_free(&b); bd_free(&c);
}
TEST(depth_24_row_padding_untouched) {
    /* 5px * 3 = 15 bytes, padded to 16: the pad byte must not matter for neighbours */
    BD a, b; int x, y, bad = 0; NEED();
    a = bd_new(24, 5, 4); b = bd_new(24, 5, 4);
    for (y = 0; y < 4; y++) for (x = 0; x < 5; x++) bd_put(&a, x, y, enc(x, y));
    memset(b.raw, 0xEE, (size_t)b.stride * 4);
    CHECK(pBitBlt(b.dc, 0, 0, 5, 4, a.dc, 0, 0, SRCCOPY));
    for (y = 0; y < 4; y++) for (x = 0; x < 5; x++) if (bd_get(&b, x, y) != enc(x, y)) bad++;
    CHECKF(!bad, "bad %d", bad);
    bd_free(&a); bd_free(&b);
}
TEST(depth_32_alpha_byte_note) {
    BD s, d; NEED();
    s = bd_new(32, 2, 2); d = bd_new(32, 2, 2);
    bd_clear(&s, 0xAB123456); bd_clear(&d, 0x55000000);
    CHECK(pBitBlt(d.dc, 0, 0, 2, 2, s.dc, 0, 0, SRCCOPY));
    t_note("32bpp SRCCOPY: dest alpha byte becomes %02lx (source 0xAB, previous 0x55)", (unsigned long)(bd_get(&d, 0, 0) >> 24));
    CHECK_PX(d, 0, 0, 0x123456, 0xFFFFFF);
    bd_free(&s); bd_free(&d);
}

/* ---- palettised ---- */
TEST(depth_32_to_8_exact_palette_entries) {
    BD s, d; int i, bad = 0; NEED();
    s = bd_new(32, 256, 1); d = bd_new(8, 256, 1);
    for (i = 0; i < 256; i++) bd_put(&s, i, 0, px(blt_palcol(8, i)));
    CHECK(pBitBlt(d.dc, 0, 0, 256, 1, s.dc, 0, 0, SRCCOPY));
    for (i = 0; i < 256; i++) if (bd_get(&d, i, 0) != (DWORD)i) bad++;
    CHECKF(!bad, "%d entries mapped to a different index", bad);
    bd_free(&s); bd_free(&d);
}
TEST(depth_32_to_8_black_white) {
    BD s, d; NEED();
    s = bd_new(32, 2, 1); d = bd_new(8, 2, 1);
    bd_put(&s, 0, 0, 0); bd_put(&s, 1, 0, 0xFFFFFF);
    CHECK(pBitBlt(d.dc, 0, 0, 2, 1, s.dc, 0, 0, SRCCOPY));
    CHECK(bd_get(&d, 0, 0) == 0 && bd_get(&d, 1, 0) == 255);
    bd_free(&s); bd_free(&d);
}
TEST(depth_32_to_8_nearest_observed) {
    BD s, d; int i, sumd = 0; NEED();
    s = bd_new(32, 1, 1); d = bd_new(8, 1, 1);
    for (i = 0; i < 40; i++) {
        DWORD c = rnd() & 0xFFFFFF;
        bd_put(&s, 0, 0, c);
        CHECK(pBitBlt(d.dc, 0, 0, 1, 1, s.dc, 0, 0, SRCCOPY));
        COLORREF pc = blt_palcol(8, bd_get(&d, 0, 0));
        int dr = GetRValue(pc) - ch(c, 16), dg = GetGValue(pc) - ch(c, 8), db = GetBValue(pc) - ch(c, 0);
        sumd += dr * dr + dg * dg + db * db;
    }
    t_note("32->8 nearest: total squared error over 40 colours = %d", sumd);
    bd_free(&s); bd_free(&d);
}
TEST(depth_8_to_32_uses_color_table) {
    BD s, d; int i, bad = 0; NEED();
    s = bd_new(8, 256, 1); d = bd_new(32, 256, 1);
    RGBQUAD tbl[256];
    for (i = 0; i < 256; i++) { tbl[i].rgbRed = (i * 5 + 3) & 255; tbl[i].rgbGreen = (i * 11 + 7) & 255; tbl[i].rgbBlue = (i * 17 + 1) & 255; tbl[i].rgbReserved = 0; }
    CHECK(pSetDIBColorTable(s.dc, 0, 256, tbl) == 256);
    for (i = 0; i < 256; i++) bd_put(&s, i, 0, i);
    CHECK(pBitBlt(d.dc, 0, 0, 256, 1, s.dc, 0, 0, SRCCOPY));
    for (i = 0; i < 256; i++) if ((bd_get(&d, i, 0) & 0xFFFFFF) != (DWORD)((tbl[i].rgbRed << 16) | (tbl[i].rgbGreen << 8) | tbl[i].rgbBlue)) bad++;
    CHECKF(!bad, "%d entries wrong", bad);
    bd_free(&s); bd_free(&d);
}
TEST(depth_4_to_32_uses_color_table) {
    BD s, d; int i, bad = 0; NEED();
    s = bd_new(4, 16, 1); d = bd_new(32, 16, 1);
    RGBQUAD tbl[16];
    for (i = 0; i < 16; i++) { tbl[i].rgbRed = 255 - i * 13; tbl[i].rgbGreen = i * 9 + 40; tbl[i].rgbBlue = (i * 29) & 255; tbl[i].rgbReserved = 0; }
    CHECK(pSetDIBColorTable(s.dc, 0, 16, tbl) == 16);
    for (i = 0; i < 16; i++) bd_put(&s, i, 0, i);
    CHECK(pBitBlt(d.dc, 0, 0, 16, 1, s.dc, 0, 0, SRCCOPY));
    for (i = 0; i < 16; i++) if ((bd_get(&d, i, 0) & 0xFFFFFF) != (DWORD)((tbl[i].rgbRed << 16) | (tbl[i].rgbGreen << 8) | tbl[i].rgbBlue)) bad++;
    CHECKF(!bad, "%d entries wrong", bad);
    bd_free(&s); bd_free(&d);
}
TEST(depth_1_to_32_dib_color_table) {
    BD s, d; int x, bad = 0; NEED();
    s = bd_new(1, 16, 2); d = bd_new(32, 16, 2);
    RGBQUAD tbl[2] = { { 0x30, 0x20, 0x10, 0 }, { 0x50, 0x60, 0x70, 0 } };   /* B,G,R */
    CHECK(pSetDIBColorTable(s.dc, 0, 2, tbl) == 2);
    for (x = 0; x < 16; x++) { bd_put(&s, x, 0, x & 1); bd_put(&s, x, 1, (x >> 1) & 1); }
    CHECK(pBitBlt(d.dc, 0, 0, 16, 2, s.dc, 0, 0, SRCCOPY));
    for (x = 0; x < 16; x++) {
        if ((bd_get(&d, x, 0) & 0xFFFFFF) != ((x & 1) ? 0x706050 : 0x102030)) bad++;
        if ((bd_get(&d, x, 1) & 0xFFFFFF) != (((x >> 1) & 1) ? 0x706050 : 0x102030)) bad++;
    }
    CHECKF(!bad, "%d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(depth_1_to_32_default_table_note) {
    BD s, d; NEED();
    s = bd_new(1, 8, 1); d = bd_new(32, 8, 1);
    bd_put(&s, 0, 0, 0); bd_put(&s, 1, 0, 1);
    pSetTextColor(d.dc, RGB(255, 0, 0)); pSetBkColor(d.dc, RGB(0, 0, 255));
    CHECK(pBitBlt(d.dc, 0, 0, 2, 1, s.dc, 0, 0, SRCCOPY));
    t_note("1bpp DIB (black/white table) -> 32: bit0 -> %06lx, bit1 -> %06lx (text=ff0000 bk=0000ff)", (unsigned long)(bd_get(&d, 0, 0) & 0xFFFFFF), (unsigned long)(bd_get(&d, 1, 0) & 0xFFFFFF));
    bd_free(&s); bd_free(&d);
}
TEST(depth_8_to_8_different_palettes_remap) {
    BD s, d; int i, bad = 0; NEED();
    s = bd_new(8, 256, 1); d = bd_new(8, 256, 1);
    RGBQUAD rev[256];
    for (i = 0; i < 256; i++) { COLORREF c = blt_palcol(8, 255 - i); rev[i].rgbRed = GetRValue(c); rev[i].rgbGreen = GetGValue(c); rev[i].rgbBlue = GetBValue(c); rev[i].rgbReserved = 0; }
    CHECK(pSetDIBColorTable(d.dc, 0, 256, rev) == 256);
    for (i = 0; i < 256; i++) bd_put(&s, i, 0, i);
    CHECK(pBitBlt(d.dc, 0, 0, 256, 1, s.dc, 0, 0, SRCCOPY));
    for (i = 0; i < 256; i++) if (bd_get(&d, i, 0) != (DWORD)(255 - i)) bad++;
    CHECKF(!bad, "%d indices not remapped by colour", bad);
    bd_free(&s); bd_free(&d);
}
TEST(depth_8_to_8_same_palette_identity) {
    BD s, d; int i, bad = 0; NEED();
    s = bd_new(8, 256, 1); d = bd_new(8, 256, 1);
    for (i = 0; i < 256; i++) bd_put(&s, i, 0, i);
    CHECK(pBitBlt(d.dc, 0, 0, 256, 1, s.dc, 0, 0, SRCCOPY));
    for (i = 0; i < 256; i++) if (bd_get(&d, i, 0) != (DWORD)i) bad++;
    CHECKF(!bad, "%d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(depth_4_to_8_by_color) {
    BD s, d; int i, bad = 0; NEED();
    s = bd_new(4, 16, 1); d = bd_new(8, 16, 1);
    for (i = 0; i < 16; i++) bd_put(&s, i, 0, i);
    CHECK(pBitBlt(d.dc, 0, 0, 16, 1, s.dc, 0, 0, SRCCOPY));
    for (i = 0; i < 16; i++) {
        COLORREF want = blt_palcol(4, i), got = blt_palcol(8, bd_get(&d, i, 0));
        if (want != got) bad++;
    }
    t_note("4bpp->8bpp: %d of 16 colours not exactly matched (palettes unrelated)", bad);
    bd_free(&s); bd_free(&d);
}
TEST(depth_32_to_4_exact_entries) {
    BD s, d; int i, bad = 0; NEED();
    s = bd_new(32, 16, 1); d = bd_new(4, 16, 1);
    for (i = 0; i < 16; i++) bd_put(&s, i, 0, px(blt_palcol(4, i)));
    CHECK(pBitBlt(d.dc, 0, 0, 16, 1, s.dc, 0, 0, SRCCOPY));
    for (i = 0; i < 16; i++) if (bd_get(&d, i, 0) != (DWORD)i) bad++;
    CHECKF(!bad, "%d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(depth_32_to_1bpp_black_white) {
    BD s, d; int x, bad = 0; NEED();
    s = bd_new(32, 16, 1); d = bd_new(1, 16, 1);
    for (x = 0; x < 16; x++) bd_put(&s, x, 0, (x & 1) ? 0xFFFFFF : 0);
    CHECK(pBitBlt(d.dc, 0, 0, 16, 1, s.dc, 0, 0, SRCCOPY));
    for (x = 0; x < 16; x++) if (bd_get(&d, x, 0) != (DWORD)(x & 1)) bad++;
    CHECKF(!bad, "%d", bad);
    bd_free(&s); bd_free(&d);
}

/* ---- monochrome DDBs ---- */
static HBITMAP mono_ddb(int w, int h, int (*f)(int, int)) {
    int stride = ((w + 15) / 16) * 2, x, y;
    BYTE *bits = calloc(stride * h, 1);
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) if (f(x, y)) bits[y * stride + x / 8] |= 0x80 >> (x & 7);
    HBITMAP bm = pCreateBitmap(w, h, 1, 1, bits);
    free(bits);
    return bm;
}
static int f_chk(int x, int y) { return (x + y) & 1; }
static int f_col(int x, int y) { return x & 1; }

TEST(mono_ddb_to_color_text_and_bk) {
    BD d; int x, y, bad = 0; NEED();
    d = bd_new(32, 16, 8);
    HBITMAP mb = mono_ddb(16, 8, f_chk);
    HDC sdc = pCreateCompatibleDC(NULL); HGDIOBJ so = pSelectObject(sdc, mb);
    pSetTextColor(d.dc, RGB(0xFF, 0x00, 0x00)); pSetBkColor(d.dc, RGB(0x00, 0x00, 0xFF));
    CHECK(pBitBlt(d.dc, 0, 0, 16, 8, sdc, 0, 0, SRCCOPY));
    for (y = 0; y < 8; y++) for (x = 0; x < 16; x++) {
        DWORD e = f_chk(x, y) ? 0x0000FF : 0xFF0000;   /* 1 -> background colour, 0 -> text colour */
        if ((bd_get(&d, x, y) & 0xFFFFFF) != e) bad++;
    }
    CHECKF(!bad, "%d pixels (1 bits should take the bk colour, 0 bits the text colour)", bad);
    pSelectObject(sdc, so); pDeleteDC(sdc); pDeleteObject(mb); bd_free(&d);
}
TEST(mono_ddb_to_color_default_colors) {
    BD d; NEED();
    d = bd_new(32, 4, 2);
    HBITMAP mb = mono_ddb(4, 2, f_col);
    HDC sdc = pCreateCompatibleDC(NULL); HGDIOBJ so = pSelectObject(sdc, mb);
    CHECK(pBitBlt(d.dc, 0, 0, 4, 2, sdc, 0, 0, SRCCOPY));
    CHECK_PX(d, 0, 0, 0x000000, 0xFFFFFF);
    CHECK_PX(d, 1, 0, 0xFFFFFF, 0xFFFFFF);
    pSelectObject(sdc, so); pDeleteDC(sdc); pDeleteObject(mb); bd_free(&d);
}
TEST(mono_ddb_to_color_colors_vary_per_blit) {
    BD d; int i; NEED();
    d = bd_new(32, 2, 1);
    HBITMAP mb = mono_ddb(2, 1, f_col);
    HDC sdc = pCreateCompatibleDC(NULL); HGDIOBJ so = pSelectObject(sdc, mb);
    for (i = 0; i < 20; i++) {
        DWORD t = rnd() & 0xFFFFFF, b = rnd() & 0xFFFFFF;
        pSetTextColor(d.dc, cr(t)); pSetBkColor(d.dc, cr(b));
        CHECK(pBitBlt(d.dc, 0, 0, 2, 1, sdc, 0, 0, SRCCOPY));
        CHECKF((bd_get(&d, 0, 0) & 0xFFFFFF) == t && (bd_get(&d, 1, 0) & 0xFFFFFF) == b, "t=%06lx b=%06lx got %06lx %06lx", (unsigned long)t, (unsigned long)b, (unsigned long)(bd_get(&d, 0, 0) & 0xFFFFFF), (unsigned long)(bd_get(&d, 1, 0) & 0xFFFFFF));
    }
    pSelectObject(sdc, so); pDeleteDC(sdc); pDeleteObject(mb); bd_free(&d);
}
TEST(mono_ddb_source_dc_colors_ignored) {
    /* the destination DC's colours are used, not the source's */
    BD d; NEED();
    d = bd_new(32, 2, 1);
    HBITMAP mb = mono_ddb(2, 1, f_col);
    HDC sdc = pCreateCompatibleDC(NULL); HGDIOBJ so = pSelectObject(sdc, mb);
    pSetTextColor(sdc, RGB(1, 2, 3)); pSetBkColor(sdc, RGB(4, 5, 6));
    pSetTextColor(d.dc, RGB(0x10, 0x20, 0x30)); pSetBkColor(d.dc, RGB(0x40, 0x50, 0x60));
    CHECK(pBitBlt(d.dc, 0, 0, 2, 1, sdc, 0, 0, SRCCOPY));
    CHECK_PX(d, 0, 0, 0x102030, 0xFFFFFF); CHECK_PX(d, 1, 0, 0x405060, 0xFFFFFF);
    pSelectObject(sdc, so); pDeleteDC(sdc); pDeleteObject(mb); bd_free(&d);
}
TEST(color_to_mono_ddb_matches_bk_color) {
    BD s; int x, y, bad = 0; NEED();
    s = bd_new(32, 16, 4);
    for (y = 0; y < 4; y++) for (x = 0; x < 16; x++) bd_put(&s, x, y, ((x + y) & 1) ? 0x123456 : 0xFEDCBA);
    pSetBkColor(s.dc, cr(0x123456));
    HDC ddc = pCreateCompatibleDC(NULL);
    HBITMAP mb = pCreateBitmap(16, 4, 1, 1, NULL); HGDIOBJ o = pSelectObject(ddc, mb);
    CHECK(pBitBlt(ddc, 0, 0, 16, 4, s.dc, 0, 0, SRCCOPY));
    BYTE bits[8]; memset(bits, 0, sizeof bits);
    pGetBitmapBits(mb, 8, bits);
    for (y = 0; y < 4; y++) for (x = 0; x < 16; x++) {
        int bit = (bits[y * 2 + x / 8] >> (7 - (x & 7))) & 1;
        int want = ((x + y) & 1) ? 1 : 0;      /* pixels matching the source bk colour become white */
        if (bit != want) bad++;
    }
    CHECKF(!bad, "%d pixels", bad);
    pSelectObject(ddc, o); pDeleteDC(ddc); pDeleteObject(mb); bd_free(&s);
}
TEST(color_to_mono_ddb_no_match_all_black) {
    BD s; NEED();
    s = bd_new(32, 16, 2);
    bd_clear(&s, 0x778899);
    pSetBkColor(s.dc, RGB(1, 2, 3));
    HDC ddc = pCreateCompatibleDC(NULL);
    HBITMAP mb = pCreateBitmap(16, 2, 1, 1, NULL); HGDIOBJ o = pSelectObject(ddc, mb);
    CHECK(pBitBlt(ddc, 0, 0, 16, 2, s.dc, 0, 0, SRCCOPY));
    BYTE bits[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    pGetBitmapBits(mb, 4, bits);
    CHECK(bits[0] == 0 && bits[1] == 0 && bits[2] == 0 && bits[3] == 0);
    pSelectObject(ddc, o); pDeleteDC(ddc); pDeleteObject(mb); bd_free(&s);
}
TEST(color_to_mono_ddb_all_match_all_white) {
    BD s; NEED();
    s = bd_new(32, 16, 2);
    bd_clear(&s, 0x778899);
    pSetBkColor(s.dc, cr(0x778899));
    HDC ddc = pCreateCompatibleDC(NULL);
    HBITMAP mb = pCreateBitmap(16, 2, 1, 1, NULL); HGDIOBJ o = pSelectObject(ddc, mb);
    CHECK(pBitBlt(ddc, 0, 0, 16, 2, s.dc, 0, 0, SRCCOPY));
    BYTE bits[4] = { 0 };
    pGetBitmapBits(mb, 4, bits);
    CHECK(bits[0] == 0xFF && bits[1] == 0xFF && bits[2] == 0xFF && bits[3] == 0xFF);
    pSelectObject(ddc, o); pDeleteDC(ddc); pDeleteObject(mb); bd_free(&s);
}
TEST(mono_to_mono_copy) {
    int x, y, bad = 0; NEED();
    HBITMAP a = mono_ddb(16, 4, f_chk), b = pCreateBitmap(16, 4, 1, 1, NULL);
    HDC da = pCreateCompatibleDC(NULL), db = pCreateCompatibleDC(NULL);
    HGDIOBJ oa = pSelectObject(da, a), ob = pSelectObject(db, b);
    CHECK(pBitBlt(db, 0, 0, 16, 4, da, 0, 0, SRCCOPY));
    BYTE bits[8] = { 0 };
    pGetBitmapBits(b, 8, bits);
    for (y = 0; y < 4; y++) for (x = 0; x < 16; x++) if (((bits[y * 2 + x / 8] >> (7 - (x & 7))) & 1) != f_chk(x, y)) bad++;
    CHECKF(!bad, "%d", bad);
    CHECK(pBitBlt(db, 0, 0, 16, 4, da, 0, 0, NOTSRCCOPY));
    pGetBitmapBits(b, 8, bits);
    bad = 0;
    for (y = 0; y < 4; y++) for (x = 0; x < 16; x++) if (((bits[y * 2 + x / 8] >> (7 - (x & 7))) & 1) == f_chk(x, y)) bad++;
    CHECKF(!bad, "NOTSRCCOPY %d", bad);
    pSelectObject(da, oa); pSelectObject(db, ob); pDeleteDC(da); pDeleteDC(db); pDeleteObject(a); pDeleteObject(b);
}
TEST(mono_ddb_rop_and_with_colors) {
    /* mono source with SRCAND onto a colour DC: converted with text/bk colours, then ANDed */
    BD d; NEED();
    d = bd_new(32, 2, 1);
    HBITMAP mb = mono_ddb(2, 1, f_col);
    HDC sdc = pCreateCompatibleDC(NULL); HGDIOBJ so = pSelectObject(sdc, mb);
    pSetTextColor(d.dc, RGB(0, 0, 0)); pSetBkColor(d.dc, RGB(255, 255, 255));
    bd_clear(&d, 0xA5C3E1);
    CHECK(pBitBlt(d.dc, 0, 0, 2, 1, sdc, 0, 0, SRCAND));
    CHECK_PX(d, 0, 0, 0x000000, 0xFFFFFF);
    CHECK_PX(d, 1, 0, 0xA5C3E1, 0xFFFFFF);
    bd_clear(&d, 0xA5C3E1);
    CHECK(pBitBlt(d.dc, 0, 0, 2, 1, sdc, 0, 0, SRCPAINT));
    CHECK_PX(d, 0, 0, 0xA5C3E1, 0xFFFFFF);
    CHECK_PX(d, 1, 0, 0xFFFFFF, 0xFFFFFF);
    pSelectObject(sdc, so); pDeleteDC(sdc); pDeleteObject(mb); bd_free(&d);
}
TEST(mono_ddb_to_24bpp_and_16bpp) {
    NEED();
    HBITMAP mb = mono_ddb(2, 1, f_col);
    HDC sdc = pCreateCompatibleDC(NULL); HGDIOBJ so = pSelectObject(sdc, mb);
    BD d24 = bd_new(24, 2, 1), d16 = bd_new16(2, 1, 1);
    pSetTextColor(d24.dc, RGB(255, 0, 0)); pSetBkColor(d24.dc, RGB(0, 255, 0));
    pSetTextColor(d16.dc, RGB(255, 0, 0)); pSetBkColor(d16.dc, RGB(0, 255, 0));
    CHECK(pBitBlt(d24.dc, 0, 0, 2, 1, sdc, 0, 0, SRCCOPY));
    CHECK(pBitBlt(d16.dc, 0, 0, 2, 1, sdc, 0, 0, SRCCOPY));
    CHECK(bd_get(&d24, 0, 0) == 0xFF0000 && bd_get(&d24, 1, 0) == 0x00FF00);
    CHECK(bd_get(&d16, 0, 0) == 0xF800 && bd_get(&d16, 1, 0) == 0x07E0);
    pSelectObject(sdc, so); pDeleteDC(sdc); pDeleteObject(mb); bd_free(&d24); bd_free(&d16);
}

/* ---- compatible bitmaps from the display ---- */
TEST(ddb_compatible_roundtrip_via_display) {
    NEED(); REQUIRE(pGetDC); REQUIRE(pReleaseDC);
    HDC scr = pGetDC(NULL);
    if (!scr) { t_skip("no screen DC"); return; }
    int bpp = pGetDeviceCaps(scr, BITSPIXEL) * pGetDeviceCaps(scr, PLANES);
    HBITMAP cb = pCreateCompatibleBitmap(scr, 16, 8);
    CHECK(cb != NULL);
    HDC mdc = pCreateCompatibleDC(scr); HGDIOBJ o = pSelectObject(mdc, cb);
    BD d = bd_new(32, 16, 8);
    static const DWORD cols[] = { 0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0x00FFFF, 0xFF00FF };
    int i, bad = 0;
    for (i = 0; i < 8; i++) {
        HBRUSH br = pCreateSolidBrush(cr(cols[i])); HGDIOBJ ob = pSelectObject(mdc, br);
        pPatBlt(mdc, i * 2, 0, 2, 8, PATCOPY);
        pSelectObject(mdc, ob); pDeleteObject(br);
    }
    CHECK(pBitBlt(d.dc, 0, 0, 16, 8, mdc, 0, 0, SRCCOPY));
    for (i = 0; i < 8; i++) if ((bd_get(&d, i * 2, 3) & 0xFFFFFF) != cols[i] || (bd_get(&d, i * 2 + 1, 7) & 0xFFFFFF) != cols[i]) bad++;
    if (bpp >= 24) CHECKF(!bad, "display depth %d: %d stripes wrong", bpp, bad);
    else t_note("display depth %d: %d stripes differ", bpp, bad);
    pSelectObject(mdc, o); pDeleteDC(mdc); pDeleteObject(cb); bd_free(&d);
    pReleaseDC(NULL, scr);
}
TEST(ddb_createbitmap_32bpp_roundtrip) {
    BD d, e; int x, y, bad = 0; NEED();
    d = bd_new(32, 8, 8); e = bd_new(32, 8, 8);
    bd_fill_enc(&d);
    HBITMAP bm = pCreateBitmap(8, 8, 1, 32, NULL);
    CHECK(bm != NULL);
    HDC mdc = pCreateCompatibleDC(NULL); HGDIOBJ o = pSelectObject(mdc, bm);
    CHECK(pBitBlt(mdc, 0, 0, 8, 8, d.dc, 0, 0, SRCCOPY));
    CHECK(pBitBlt(e.dc, 0, 0, 8, 8, mdc, 0, 0, SRCCOPY));
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) if ((bd_get(&e, x, y) & 0xFFFFFF) != enc(x, y)) bad++;
    CHECKF(!bad, "bad %d", bad);
    pSelectObject(mdc, o); pDeleteDC(mdc); pDeleteObject(bm); bd_free(&d); bd_free(&e);
}
TEST(ddb_createbitmap_24bpp_not_forced) {
    NEED();
    HBITMAP bm = pCreateBitmap(8, 8, 1, 24, NULL);
    t_note("CreateBitmap(8,8,1,24) -> %p", bm);
    if (bm) pDeleteObject(bm);
}
TEST(ddb_default_dc_bitmap_blit) {
    BD d; NEED();
    d = bd_new(32, 4, 4); bd_clear(&d, 0x123456);
    HDC mdc = pCreateCompatibleDC(NULL);
    BOOL r = pBitBlt(d.dc, 0, 0, 4, 4, mdc, 0, 0, SRCCOPY);
    CHECK(r);
    t_note("blit from fresh DC's 1x1 mono bitmap: (0,0)=%06lx (1,1)=%06lx", (unsigned long)(bd_get(&d, 0, 0) & 0xFFFFFF), (unsigned long)(bd_get(&d, 1, 1) & 0xFFFFFF));
    pDeleteDC(mdc); bd_free(&d);
}

/* ---- mapping modes ---- */
TEST(mm_dest_scale_2x_stretches_source) {
    BD s, d; int x, y, bad = 0; NEED();
    s = bd_new(32, 4, 4); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0);
    pSetMapMode(d.dc, MM_ANISOTROPIC);
    pSetWindowExtEx(d.dc, 1, 1, NULL); pSetViewportExtEx(d.dc, 2, 2, NULL);
    pSetStretchBltMode(d.dc, COLORONCOLOR);
    BOOL r = pBitBlt(d.dc, 0, 0, 4, 4, s.dc, 0, 0, SRCCOPY);
    CHECK(r);
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != enc(x / 2, y / 2)) bad++;
    CHECKF(!bad, "bad %d", bad);
    pSetMapMode(d.dc, MM_TEXT);
    bd_free(&s); bd_free(&d);
}
TEST(mm_src_scale_only_compresses_or_clips) {
    BD s, d; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0);
    pSetMapMode(s.dc, MM_ANISOTROPIC);
    pSetWindowExtEx(s.dc, 1, 1, NULL); pSetViewportExtEx(s.dc, 2, 2, NULL);
    BOOL r = pBitBlt(d.dc, 0, 0, 4, 4, s.dc, 0, 0, SRCCOPY);
    t_note("BitBlt with scaled source DC -> %d; d(0,0)=%06lx d(3,3)=%06lx d(5,5)=%06lx", r, (unsigned long)(bd_get(&d, 0, 0) & 0xFFFFFF), (unsigned long)(bd_get(&d, 3, 3) & 0xFFFFFF), (unsigned long)(bd_get(&d, 5, 5) & 0xFFFFFF));
    pSetMapMode(s.dc, MM_TEXT);
    bd_free(&s); bd_free(&d);
}
TEST(mm_both_same_scale_is_plain_copy) {
    BD s, d; int x, y, bad = 0; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0);
    HDC dcs[2] = { s.dc, d.dc }; int i;
    for (i = 0; i < 2; i++) { pSetMapMode(dcs[i], MM_ANISOTROPIC); pSetWindowExtEx(dcs[i], 1, 1, NULL); pSetViewportExtEx(dcs[i], 2, 2, NULL); }
    CHECK(pBitBlt(d.dc, 0, 0, 4, 4, s.dc, 0, 0, SRCCOPY));
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != enc(x, y)) bad++;
    CHECKF(!bad, "bad %d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(mm_origin_offsets_apply_to_both_dcs) {
    BD s, d; int x, y, bad = 0; NEED();
    s = bd_new(32, 16, 16); d = bd_new(32, 16, 16);
    bd_fill_enc(&s); bd_clear(&d, 0);
    pSetViewportOrgEx(s.dc, 3, 2, NULL);
    pSetViewportOrgEx(d.dc, 5, 6, NULL);
    CHECK(pBitBlt(d.dc, 1, 1, 4, 3, s.dc, 2, 2, SRCCOPY));
    for (y = 0; y < 16; y++) for (x = 0; x < 16; x++) {
        DWORD e = 0;
        if (x >= 6 && x < 10 && y >= 7 && y < 10) e = enc(x - 6 + 2 + 3, y - 7 + 2 + 2);
        if ((bd_get(&d, x, y) & 0xFFFFFF) != e) bad++;
    }
    CHECKF(!bad, "bad %d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(mm_dest_flipped_y_vs_text_source) {
    BD s, d; int x, y, covered = 0; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0);
    pSetMapMode(d.dc, MM_ANISOTROPIC);
    pSetWindowExtEx(d.dc, 1, 1, NULL); pSetViewportExtEx(d.dc, 1, -1, NULL); pSetViewportOrgEx(d.dc, 0, 8, NULL);
    BOOL r = pBitBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, SRCCOPY);
    CHECK(r);
    int pos[64][2], sx, sy; memset(pos, -1, sizeof pos);
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) for (sy = 0; sy < 8; sy++) for (sx = 0; sx < 8; sx++) if ((bd_get(&d, x, y) & 0xFFFFFF) == enc(sx, sy)) { pos[sy * 8 + sx][0] = x; pos[sy * 8 + sx][1] = y; covered++; }
    CHECKF(covered >= 56, "covered %d", covered);
    int vflip = pos[0][1] > pos[8][1];
    t_note("dest y-flipped, source MM_TEXT: image is %s", vflip ? "vertically mirrored" : "not mirrored");
    pSetMapMode(d.dc, MM_TEXT); pSetViewportOrgEx(d.dc, 0, 0, NULL);
    bd_free(&s); bd_free(&d);
}
TEST(mm_lometric_blit_runs) {
    BD s, d; NEED();
    s = bd_new(32, 16, 16); d = bd_new(32, 16, 16);
    bd_fill_enc(&s); bd_clear(&d, 0);
    pSetMapMode(d.dc, MM_LOMETRIC);
    pSetViewportOrgEx(d.dc, 0, 16, NULL);
    BOOL r = pBitBlt(d.dc, 0, 0, 100, 100, s.dc, 0, 0, SRCCOPY);
    t_note("MM_LOMETRIC dest BitBlt(100x100 units) -> %d; nonzero pixels %d", r, 256 - count_v(&d, 0, 0, 16, 16, 0));
    CHECK(r);
    pSetMapMode(d.dc, MM_TEXT); pSetViewportOrgEx(d.dc, 0, 0, NULL);
    bd_free(&s); bd_free(&d);
}

/* ---- RTL layout ---- */
TEST(layout_setlayout_returns_previous) {
    NEED();
    HDC dc = pCreateCompatibleDC(NULL);
    CHECK(pGetLayout(dc) == 0);
    DWORD p = pSetLayout(dc, LAYOUT_RTL);
    CHECKF(p == 0, "prev %lu", p);
    CHECKF(pGetLayout(dc) == LAYOUT_RTL, "layout %lu", pGetLayout(dc));
    p = pSetLayout(dc, 0);
    CHECKF(p == LAYOUT_RTL, "prev %lu", p);
    CHECK(pGetLayout(dc) == 0);
    pDeleteDC(dc);
    SetLastError(0xDEAD);
    CHECK(pGetLayout(NULL) == GDI_ERROR);
    CHECK(pSetLayout(NULL, LAYOUT_RTL) == GDI_ERROR);
}
TEST(layout_rtl_dest_mirrors_bitblt) {
    BD s, d; int x, y, bad = 0; NEED();
    s = bd_new(32, 16, 4); d = bd_new(32, 16, 4);
    bd_fill_enc(&s); bd_clear(&d, 0);
    pSetLayout(d.dc, LAYOUT_RTL);
    CHECK(pBitBlt(d.dc, 0, 0, 16, 4, s.dc, 0, 0, SRCCOPY));
    for (y = 0; y < 4; y++) for (x = 0; x < 16; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != enc(15 - x, y)) bad++;
    CHECKF(!bad, "%d pixels not mirrored", bad);
    pSetLayout(d.dc, 0);
    bd_free(&s); bd_free(&d);
}
TEST(layout_rtl_nomirrorbitmap_keeps_content) {
    BD s, d; int x, y, bad = 0; NEED();
    s = bd_new(32, 16, 4); d = bd_new(32, 16, 4);
    bd_fill_enc(&s); bd_clear(&d, 0);
    pSetLayout(d.dc, LAYOUT_RTL);
    CHECK(pBitBlt(d.dc, 0, 0, 16, 4, s.dc, 0, 0, SRCCOPY | NOMIRRORBITMAP));
    for (y = 0; y < 4; y++) for (x = 0; x < 16; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != enc(x, y)) bad++;
    CHECKF(!bad, "%d pixels", bad);
    pSetLayout(d.dc, 0);
    bd_free(&s); bd_free(&d);
}
TEST(layout_rtl_both_dcs_plain_copy) {
    BD s, d; int x, y, bad = 0; NEED();
    s = bd_new(32, 16, 4); d = bd_new(32, 16, 4);
    bd_fill_enc(&s); bd_clear(&d, 0);
    pSetLayout(d.dc, LAYOUT_RTL); pSetLayout(s.dc, LAYOUT_RTL);
    CHECK(pBitBlt(d.dc, 0, 0, 16, 4, s.dc, 0, 0, SRCCOPY));
    for (y = 0; y < 4; y++) for (x = 0; x < 16; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != enc(x, y)) bad++;
    CHECKF(!bad, "%d pixels", bad);
    bd_free(&s); bd_free(&d);
}
TEST(layout_rtl_bitmap_orientation_preserved) {
    BD s, d; int x, y, bad = 0; NEED();
    s = bd_new(32, 16, 4); d = bd_new(32, 16, 4);
    bd_fill_enc(&s); bd_clear(&d, 0);
    pSetLayout(d.dc, LAYOUT_RTL | LAYOUT_BITMAPORIENTATIONPRESERVED);
    CHECK(pBitBlt(d.dc, 0, 0, 16, 4, s.dc, 0, 0, SRCCOPY));
    for (y = 0; y < 4; y++) for (x = 0; x < 16; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != enc(x, y)) bad++;
    CHECKF(!bad, "%d pixels (no mirroring expected)", bad);
    pSetLayout(d.dc, 0);
    bd_free(&s); bd_free(&d);
}
TEST(layout_rtl_partial_rect_position_mirrors) {
    BD s, d; int x, y; NEED();
    s = bd_new(32, 4, 2); d = bd_new(32, 16, 2);
    bd_fill_enc(&s); bd_clear(&d, 0);
    pSetLayout(d.dc, LAYOUT_RTL);
    CHECK(pBitBlt(d.dc, 2, 0, 4, 2, s.dc, 0, 0, SRCCOPY | NOMIRRORBITMAP));
    int first = -1, last = -1;
    for (x = 0; x < 16; x++) if (bd_get(&d, x, 0) & 0xFFFFFF) { if (first < 0) first = x; last = x; }
    t_note("RTL dest x=2,w=4 occupies device columns %d..%d", first, last);
    CHECKF(last - first == 3, "span %d..%d", first, last);
    (void)y;
    pSetLayout(d.dc, 0);
    bd_free(&s); bd_free(&d);
}
TEST(layout_rtl_patblt_side) {
    BD d; NEED();
    d = bd_new(32, 16, 2); bd_clear(&d, 0);
    HGDIOBJ o = pSelectObject(d.dc, pGetStockObject(WHITE_BRUSH));
    pSetLayout(d.dc, LAYOUT_RTL);
    CHECK(pPatBlt(d.dc, 0, 0, 4, 2, PATCOPY));
    CHECK_RECT_VAL(d, 12, 0, 4, 2, 0xFFFFFF, 0xFFFFFF);
    CHECK_RECT_VAL(d, 0, 0, 12, 2, 0, 0xFFFFFF);
    pSetLayout(d.dc, 0);
    pSelectObject(d.dc, o); bd_free(&d);
}

/* ---- clipping interplay ---- */
TEST(clip_dest_region_limits_bitblt) {
    BD s, d; int x, y, bad = 0; NEED();
    s = bd_new(32, 16, 16); d = bd_new(32, 16, 16);
    bd_fill_enc(&s); bd_clear(&d, 0);
    HRGN rg = pCreateRectRgn(3, 4, 11, 9); pSelectClipRgn(d.dc, rg);
    CHECK(pBitBlt(d.dc, 0, 0, 16, 16, s.dc, 0, 0, SRCCOPY));
    for (y = 0; y < 16; y++) for (x = 0; x < 16; x++) {
        DWORD e = (x >= 3 && x < 11 && y >= 4 && y < 9) ? enc(x, y) : 0;
        if ((bd_get(&d, x, y) & 0xFFFFFF) != e) bad++;
    }
    CHECKF(!bad, "bad %d", bad);
    pSelectClipRgn(d.dc, NULL); pDeleteObject(rg); bd_free(&s); bd_free(&d);
}
TEST(clip_region_is_in_device_units) {
    BD s, d; int x, y, bad = 0; NEED();
    s = bd_new(32, 20, 8); d = bd_new(32, 20, 8);
    bd_fill_enc(&s); bd_clear(&d, 0);
    pSetViewportOrgEx(d.dc, 10, 0, NULL);
    HRGN rg = pCreateRectRgn(0, 0, 5, 8); pSelectClipRgn(d.dc, rg);
    pSetViewportOrgEx(d.dc, 0, 0, NULL);
    CHECK(pBitBlt(d.dc, 0, 0, 20, 8, s.dc, 0, 0, SRCCOPY));
    for (y = 0; y < 8; y++) for (x = 0; x < 20; x++) {
        DWORD e = (x >= 0 && x < 5) ? enc(x, y) : 0;   /* region coordinates are device units */
        if ((bd_get(&d, x, y) & 0xFFFFFF) != e) bad++;
    }
    CHECKF(!bad, "bad %d", bad);
    pSelectClipRgn(d.dc, NULL); pDeleteObject(rg); bd_free(&s); bd_free(&d);
}
TEST(clip_source_dc_region_note) {
    BD s, d; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0);
    HRGN rg = pCreateRectRgn(0, 0, 2, 2); pSelectClipRgn(s.dc, rg);
    CHECK(pBitBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, SRCCOPY));
    t_note("clip region on source DC: dest pixels copied %d of 64", 64 - count_v(&d, 0, 0, 8, 8, 0));
    pSelectClipRgn(s.dc, NULL); pDeleteObject(rg); bd_free(&s); bd_free(&d);
}
TEST(clip_empty_region_blocks_all) {
    BD s, d; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0x77);
    HRGN rg = pCreateRectRgn(0, 0, 0, 0); pSelectClipRgn(d.dc, rg);
    BOOL r = pBitBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, SRCCOPY);
    t_note("BitBlt with empty clip -> %d", r);
    CHECK_RECT_VAL(d, 0, 0, 8, 8, 0x77, 0xFFFFFF);
    pSelectClipRgn(d.dc, NULL); pDeleteObject(rg); bd_free(&s); bd_free(&d);
}
TEST(clip_region_deleted_after_select_is_safe) {
    BD s, d; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0);
    HRGN rg = pCreateRectRgn(0, 0, 4, 4); pSelectClipRgn(d.dc, rg);
    pDeleteObject(rg);   /* DC keeps its own copy */
    CHECK(pBitBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, SRCCOPY));
    CHECK_PX(d, 3, 3, enc(3, 3), 0xFFFFFF); CHECK_PX(d, 4, 4, 0, 0xFFFFFF);
    pSelectClipRgn(d.dc, NULL); bd_free(&s); bd_free(&d);
}

/* ---- errors and lifetime ---- */
TEST(bitblt_bad_handles) {
    BD s, d; NEED();
    s = bd_new(32, 4, 4); d = bd_new(32, 4, 4);
    HBRUSH br = pCreateSolidBrush(0);
    HDC bads[] = { NULL, (HDC)(ULONG_PTR)0x1234, (HDC)(ULONG_PTR)-1, (HDC)s.bm, (HDC)br };
    int i;
    for (i = 0; i < 5; i++) {
        SetLastError(0xDEAD);
        BOOL r = pBitBlt(bads[i], 0, 0, 4, 4, s.dc, 0, 0, SRCCOPY);
        CHECKF(!r, "bad dest %d returned %d", i, r);
        t_note("BitBlt bad dest case %d err=%lu", i, GetLastError());
        SetLastError(0xDEAD);
        r = pBitBlt(d.dc, 0, 0, 4, 4, bads[i], 0, 0, SRCCOPY);
        CHECKF(!r, "bad src %d returned %d", i, r);
        t_note("BitBlt bad src case %d err=%lu", i, GetLastError());
    }
    pDeleteObject(br); bd_free(&s); bd_free(&d);
}
TEST(bitblt_null_dest_sets_invalid_handle_note) {
    NEED();
    BD s = bd_new(32, 4, 4);
    SetLastError(0);
    CHECK(!pBitBlt(NULL, 0, 0, 4, 4, s.dc, 0, 0, SRCCOPY));
    DWORD e = GetLastError();
    t_note("BitBlt(NULL,...) last error %lu", e);
    bd_free(&s);
}
TEST(bitblt_after_dc_deleted) {
    NEED();
    BD s = bd_new(32, 4, 4);
    HDC dc = pCreateCompatibleDC(NULL);
    pDeleteDC(dc);
    CHECK(!pBitBlt(dc, 0, 0, 4, 4, s.dc, 0, 0, SRCCOPY));
    CHECK(!pBitBlt(s.dc, 0, 0, 4, 4, dc, 0, 0, SRCCOPY));
    bd_free(&s);
}
TEST(bitblt_zero_extents_noop) {
    BD s, d; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0x77);
    static const int e[][2] = { { 0, 4 }, { 4, 0 }, { 0, 0 } };
    int i;
    for (i = 0; i < 3; i++) {
        SetLastError(0xDEAD);
        BOOL r = pBitBlt(d.dc, 1, 1, e[i][0], e[i][1], s.dc, 0, 0, SRCCOPY);
        t_note("BitBlt cx=%d cy=%d -> %d err=%lu", e[i][0], e[i][1], r, GetLastError());
        CHECK_RECT_VAL(d, 0, 0, 8, 8, 0x77, 0xFFFFFF);
    }
    bd_free(&s); bd_free(&d);
}
TEST(bitblt_negative_extents_observed) {
    BD s, d; NEED();
    s = bd_new(32, 16, 16); d = bd_new(32, 16, 16);
    bd_fill_enc(&s); bd_clear(&d, 0);
    SetLastError(0xDEAD);
    BOOL r = pBitBlt(d.dc, 10, 10, -4, -4, s.dc, 10, 10, SRCCOPY);
    int n = 256 - count_v(&d, 0, 0, 16, 16, 0);
    t_note("BitBlt negative extents -> %d err=%lu, %d pixels written", r, GetLastError(), n);
    r = pBitBlt(d.dc, 10, 10, -4, 4, s.dc, 10, 10, SRCCOPY);
    t_note("BitBlt cx<0 cy>0 -> %d", r);
    bd_free(&s); bd_free(&d);
}
TEST(bitblt_source_rect_beyond_bitmap) {
    BD s, d; NEED();
    s = bd_new(32, 4, 4); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0x77);
    SetLastError(0xDEAD);
    BOOL r = pBitBlt(d.dc, 0, 0, 8, 8, s.dc, 0, 0, SRCCOPY);
    t_note("BitBlt source smaller than rect -> %d; d(3,3)=%06lx d(4,4)=%06lx d(7,7)=%06lx", r, (unsigned long)(bd_get(&d, 3, 3) & 0xFFFFFF), (unsigned long)(bd_get(&d, 4, 4) & 0xFFFFFF), (unsigned long)(bd_get(&d, 7, 7) & 0xFFFFFF));
    CHECK_PX(d, 3, 3, enc(3, 3), 0xFFFFFF);
    bd_free(&s); bd_free(&d);
}
TEST(bitblt_source_offset_negative) {
    BD s, d; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0x77);
    BOOL r = pBitBlt(d.dc, 0, 0, 4, 4, s.dc, -2, -2, SRCCOPY);
    t_note("BitBlt negative source origin -> %d; d(0,0)=%06lx d(2,2)=%06lx", r, (unsigned long)(bd_get(&d, 0, 0) & 0xFFFFFF), (unsigned long)(bd_get(&d, 2, 2) & 0xFFFFFF));
    if (r) CHECK_PX(d, 2, 2, enc(0, 0), 0xFFFFFF);
    bd_free(&s); bd_free(&d);
}
TEST(bitblt_huge_extent_no_hang) {
    BD s, d; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0);
    BOOL r = pBitBlt(d.dc, 0, 0, 0x3FFFFFFF, 0x3FFFFFFF, s.dc, 0, 0, SRCCOPY);
    t_note("BitBlt 0x3FFFFFFF extents -> %d", r);
    bd_free(&s); bd_free(&d);
}
TEST(bitblt_large_1024_exact) {
    BD s, d; int x, y, bad = 0; NEED();
    s = bd_new(32, 1024, 1024); d = bd_new(32, 1024, 1024);
    if (!bd_ok(&s) || !bd_ok(&d)) { CHECK(0); bd_free(&s); bd_free(&d); return; }
    for (y = 0; y < 1024; y++) for (x = 0; x < 1024; x++) bd_put(&s, x, y, (x * 2654435761u + y * 40503u) & 0xFFFFFF);
    CHECK(pBitBlt(d.dc, 0, 0, 1024, 1024, s.dc, 0, 0, SRCCOPY));
    for (y = 0; y < 1024; y++) for (x = 0; x < 1024; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != ((x * 2654435761u + y * 40503u) & 0xFFFFFF)) bad++;
    CHECKF(!bad, "%d pixels", bad);
    bd_free(&s); bd_free(&d);
}
TEST(bitblt_wide_row_widths_1_to_70) {
    int w; NEED();
    for (w = 1; w <= 70; w += (w < 40 ? 1 : 5)) {
        BD s = bd_new(32, w, 3), d = bd_new(32, w, 3);
        int x, y, bad = 0;
        for (y = 0; y < 3; y++) for (x = 0; x < w; x++) bd_put(&s, x, y, enc(x, y));
        CHECK(pBitBlt(d.dc, 0, 0, w, 3, s.dc, 0, 0, SRCCOPY));
        for (y = 0; y < 3; y++) for (x = 0; x < w; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != enc(x, y)) bad++;
        CHECKF(!bad, "width %d: %d", w, bad);
        bd_free(&s); bd_free(&d);
    }
}
TEST(bitblt_1bpp_unaligned_offsets) {
    int sx, dx, bad_total = 0; NEED();
    for (sx = 0; sx < 9; sx++) for (dx = 0; dx < 9; dx++) {
        BD s = bd_new(1, 40, 2), d = bd_new(1, 40, 2), ref = bd_new(1, 40, 2);
        int x, y, bad = 0;
        for (y = 0; y < 2; y++) for (x = 0; x < 40; x++) { DWORD v = rnd() & 1; bd_put(&s, x, y, v); bd_put(&d, x, y, 1); bd_put(&ref, x, y, 1); }
        for (y = 0; y < 2; y++) for (x = 0; x < 21; x++) bd_put(&ref, dx + x, y, bd_get(&s, sx + x, y));
        pBitBlt(d.dc, dx, 0, 21, 2, s.dc, sx, 0, SRCCOPY);
        for (y = 0; y < 2; y++) for (x = 0; x < 40; x++) if (bd_get(&d, x, y) != bd_get(&ref, x, y)) bad++;
        bad_total += bad;
        bd_free(&s); bd_free(&d); bd_free(&ref);
    }
    CHECKF(!bad_total, "%d pixel errors over all 81 offset pairs", bad_total);
}
TEST(bitblt_4bpp_unaligned_offsets) {
    int sx, dx, bad_total = 0; NEED();
    for (sx = 0; sx < 4; sx++) for (dx = 0; dx < 4; dx++) {
        BD s = bd_new(4, 24, 2), d = bd_new(4, 24, 2), ref = bd_new(4, 24, 2);
        int x, y, bad = 0;
        for (y = 0; y < 2; y++) for (x = 0; x < 24; x++) { DWORD v = rnd() & 15; bd_put(&s, x, y, v); bd_put(&d, x, y, 5); bd_put(&ref, x, y, 5); }
        for (y = 0; y < 2; y++) for (x = 0; x < 15; x++) bd_put(&ref, dx + x, y, bd_get(&s, sx + x, y));
        pBitBlt(d.dc, dx, 0, 15, 2, s.dc, sx, 0, SRCCOPY);
        for (y = 0; y < 2; y++) for (x = 0; x < 24; x++) if (bd_get(&d, x, y) != bd_get(&ref, x, y)) bad++;
        bad_total += bad;
        bd_free(&s); bd_free(&d); bd_free(&ref);
    }
    CHECKF(!bad_total, "%d pixel errors", bad_total);
}
TEST(bitblt_srcinvert_twice_restores) {
    BD s, d; int x, y, bad = 0; NEED();
    s = bd_new(32, 12, 12); d = bd_new(32, 12, 12);
    for (y = 0; y < 12; y++) for (x = 0; x < 12; x++) { bd_put(&s, x, y, rnd() & 0xFFFFFF); bd_put(&d, x, y, enc(x, y)); }
    pBitBlt(d.dc, 0, 0, 12, 12, s.dc, 0, 0, SRCINVERT);
    pBitBlt(d.dc, 0, 0, 12, 12, s.dc, 0, 0, SRCINVERT);
    for (y = 0; y < 12; y++) for (x = 0; x < 12; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != enc(x, y)) bad++;
    CHECKF(!bad, "bad %d", bad);
    bd_free(&s); bd_free(&d);
}
TEST(bitblt_bitmap_selected_out_then_blit_fails) {
    NEED();
    BD s = bd_new(32, 4, 4);
    HDC dc = pCreateCompatibleDC(NULL);
    HBITMAP bm = pCreateBitmap(4, 4, 1, 1, NULL);
    HGDIOBJ o = pSelectObject(dc, bm);
    pSelectObject(dc, o);   /* back to the stock 1x1 mono bitmap */
    pDeleteObject(bm);
    BOOL r = pBitBlt(s.dc, 0, 0, 4, 4, dc, 0, 0, SRCCOPY);
    t_note("blit from DC after its bitmap was deselected and deleted -> %d", r);
    pDeleteDC(dc); bd_free(&s);
}

/* ---- threads and leaks ---- */
typedef struct { BD *s, *d; BOOL ret; DWORD err; } thr_arg;
static DWORD WINAPI blt_thread(LPVOID p) {
    thr_arg *a = p;
    SetLastError(0);
    a->ret = pBitBlt(a->d->dc, 0, 0, 8, 8, a->s->dc, 0, 0, SRCCOPY);
    a->err = GetLastError();
    return 0;
}
TEST(thread_blit_with_dcs_from_other_thread) {
    BD s, d; NEED();
    s = bd_new(32, 8, 8); d = bd_new(32, 8, 8);
    bd_fill_enc(&s); bd_clear(&d, 0);
    thr_arg a = { &s, &d, 0, 0 };
    HANDLE h = CreateThread(NULL, 0, blt_thread, &a, 0, NULL);
    WaitForSingleObject(h, 10000); CloseHandle(h);
    CHECKF(a.ret, "BitBlt in other thread failed err=%lu", a.err);
    int x, y, bad = 0;
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) if ((bd_get(&d, x, y) & 0xFFFFFF) != enc(x, y)) bad++;
    CHECKF(!bad, "bad %d", bad);
    bd_free(&s); bd_free(&d);
}
typedef struct { HDC dc; HBITMAP bm; BYTE *bits; BOOL ret; } mk_arg;
static DWORD WINAPI mk_thread(LPVOID p) {
    mk_arg *a = p;
    BD b = bd_new(32, 8, 8);
    bd_fill_enc(&b);
    /* hand the DC to the creator: detach by selecting nothing back */
    a->dc = b.dc; a->bm = b.bm; a->bits = b.raw; a->ret = TRUE;
    return 0;
}
TEST(thread_dc_outlives_creating_thread) {
    BD d; NEED();
    mk_arg a; memset(&a, 0, sizeof a);
    HANDLE h = CreateThread(NULL, 0, mk_thread, &a, 0, NULL);
    WaitForSingleObject(h, 10000); CloseHandle(h);
    d = bd_new(32, 8, 8); bd_clear(&d, 0);
    SetLastError(0);
    BOOL r = pBitBlt(d.dc, 0, 0, 8, 8, a.dc, 0, 0, SRCCOPY);
    t_note("blit from a DC whose creating thread exited -> %d err=%lu", r, GetLastError());
    if (r) CHECK_PX(d, 3, 3, enc(3, 3), 0xFFFFFF);
    HGDIOBJ o = pGetCurrentObject(a.dc, OBJ_BITMAP); (void)o;
    pDeleteDC(a.dc); pDeleteObject(a.bm);
    bd_free(&d);
}
TEST(gdi_objects_do_not_leak_across_blits) {
    NEED(); REQUIRE(pGetGuiResources);
    DWORD before = pGetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS), mid, after;
    int i;
    for (i = 0; i < 200; i++) {
        BD s = bd_new(32, 16, 16), d = bd_new(32, 16, 16);
        HBRUSH br = pCreateSolidBrush(0x123456); HGDIOBJ o = pSelectObject(d.dc, br);
        pBitBlt(d.dc, 0, 0, 16, 16, s.dc, 0, 0, SRCCOPY);
        pBitBlt(d.dc, 0, 0, 16, 16, s.dc, 0, 0, MERGECOPY);
        pPatBlt(d.dc, 0, 0, 16, 16, PATINVERT);
        pStretchBlt(d.dc, 0, 0, 16, 16, s.dc, 0, 0, 8, 8, SRCCOPY);
        pSelectObject(d.dc, o); pDeleteObject(br);
        bd_free(&s); bd_free(&d);
    }
    after = pGetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    (void)mid;
    CHECKF(after == before, "GDI objects before %lu after %lu", before, after);
}
TEST(gdi_objects_stable_on_failed_blits) {
    NEED(); REQUIRE(pGetGuiResources);
    BD s = bd_new(32, 4, 4), d = bd_new(32, 4, 4);
    DWORD before = pGetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS), after;
    int i;
    for (i = 0; i < 300; i++) {
        pBitBlt(NULL, 0, 0, 4, 4, s.dc, 0, 0, SRCCOPY);
        pBitBlt(d.dc, 0, 0, 4, 4, NULL, 0, 0, SRCCOPY);
        pStretchBlt(d.dc, 0, 0, 0, 0, s.dc, 0, 0, 0, 0, SRCCOPY);
        pMaskBlt(d.dc, 0, 0, 4, 4, NULL, 0, 0, NULL, 0, 0, SRCCOPY);
        if (pGdiAlphaBlend) pGdiAlphaBlend(d.dc, 0, 0, 4, 4, NULL, 0, 0, 4, 4, (BLENDFUNCTION){ 0, 0, 255, 0 });
    }
    after = pGetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    CHECKF(after == before, "before %lu after %lu", before, after);
    bd_free(&s); bd_free(&d);
}
TEST(bitblt_dest_bitmap_deselected_dc_state) {
    /* after bd_free-like teardown the DC's original bitmap is back and usable as a destination */
    NEED();
    HDC dc = pCreateCompatibleDC(NULL);
    HBITMAP bm = pCreateBitmap(4, 4, 1, 1, NULL);
    HGDIOBJ o = pSelectObject(dc, bm);
    CHECK(o != NULL);
    CHECK(pGetObjectType(o) == OBJ_BITMAP);
    CHECK(pSelectObject(dc, o) == (HGDIOBJ)bm);
    CHECK(pPatBlt(dc, 0, 0, 4, 4, BLACKNESS));
    pDeleteDC(dc); pDeleteObject(bm);
}
