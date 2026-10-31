/* StretchDIBits (+ StretchDIBitsImpl) */
#include "g3.h"

typedef int (WINAPI *SDIB)(HDC, int, int, int, int, int, int, int, int, const VOID *, const BITMAPINFO *, UINT, DWORD);

static SDIB sdib(int impl) { return (SDIB)g3_fn("StretchDIBits", impl); }

typedef union { BITMAPINFO bi; char raw[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)]; } BMI;
static void bmi_init(BMI *b, int w, int h, int bpp, int ncolors) {
    memset(b, 0, sizeof *b);
    b->bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    b->bi.bmiHeader.biWidth = w; b->bi.bmiHeader.biHeight = h;
    b->bi.bmiHeader.biPlanes = 1; b->bi.bmiHeader.biBitCount = bpp;
    b->bi.bmiHeader.biCompression = BI_RGB;
    b->bi.bmiHeader.biClrUsed = ncolors;
}
static int stride(int w, int bpp) { return ((w * bpp + 31) / 32) * 4; }
static void set_pal(BMI *b, int i, int r, int g, int bl) {
    b->bi.bmiColors[i].rgbRed = r; b->bi.bmiColors[i].rgbGreen = g; b->bi.bmiColors[i].rgbBlue = bl;
}
static const COLORREF ROWC[8] = { RGB(255,0,0), RGB(0,255,0), RGB(0,0,255), RGB(255,255,0),
    RGB(0,255,255), RGB(255,0,255), RGB(128,128,128), RGB(255,255,255) };

/* build a 32bpp image of w x h whose memory row i has colour ROWC[i%8] */
static BYTE *make32(int w, int h) {
    BYTE *p = malloc(w * h * 4); int x, y;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        COLORREF c = ROWC[y % 8]; BYTE *q = p + (y * w + x) * 4;
        q[0] = GetBValue(c); q[1] = GetGValue(c); q[2] = GetRValue(c); q[3] = 0;
    }
    return p;
}

#define FOR_SD(S) for (int i_ = 0; i_ < 2; i_++) { SDIB S = sdib(i_); if (!S) { t_note("StretchDIBits impl %d missing", i_); continue; }

TEST(gap3_sdib_32bpp_bottomup) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 8, 8)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 4, 4, 32, 0);
        BYTE *px = make32(4, 4);
        g3dc_clear(&m, RGB(1,2,3));
        int r = S(m.dc, 1, 1, 4, 4, 0, 0, 4, 4, px, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        EQ(r, 4);
        int x, y;
        for (y = 0; y < 4; y++) for (x = 0; x < 4; x++)
            EQ(g3dc_get(&m, 1 + x, 1 + y), g3_px(ROWC[3 - y]));
        PXC(m, 0, 0, RGB(1,2,3)); PXC(m, 5, 5, RGB(1,2,3)); PXC(m, 0, 1, RGB(1,2,3)); PXC(m, 5, 1, RGB(1,2,3));
        free(px); g3dc_free(&m);
    }
}
TEST(gap3_sdib_32bpp_topdown) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 8, 8)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 4, -4, 32, 0);
        BYTE *px = make32(4, 4);
        g3dc_clear(&m, RGB(1,2,3));
        int r = S(m.dc, 2, 2, 4, 4, 0, 0, 4, 4, px, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        EQ(r, 4);
        int x, y;
        for (y = 0; y < 4; y++) for (x = 0; x < 4; x++)
            EQ(g3dc_get(&m, 2 + x, 2 + y), g3_px(ROWC[y]));
        free(px); g3dc_free(&m);
    }
}
TEST(gap3_sdib_source_rect_bottomup) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 8, 8)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 4, 4, 32, 0);
        BYTE *px = make32(4, 4);
        g3dc_clear(&m, RGB(1,2,3));
        /* source origin is the lower-left corner: ySrc=0,hSrc=2 selects memory rows 0 and 1 */
        int r = S(m.dc, 0, 0, 4, 2, 0, 0, 4, 2, px, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        t_note("partial copy returned %d", r);
        PXC(m, 0, 0, ROWC[1]);
        PXC(m, 3, 0, ROWC[1]);
        PXC(m, 0, 1, ROWC[0]);
        PXC(m, 0, 2, RGB(1,2,3));
        free(px); g3dc_free(&m);
    }
}
TEST(gap3_sdib_source_rect_x_offset) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 8, 8)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 4, -2, 32, 0);
        DWORD src[8] = { 0x010101, 0x020202, 0x030303, 0x040404, 0x050505, 0x060606, 0x070707, 0x080808 };
        g3dc_clear(&m, RGB(0,0,0));
        S(m.dc, 0, 0, 2, 2, 2, 0, 2, 2, src, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        EQ(g3dc_get(&m, 0, 0), 0x030303);
        EQ(g3dc_get(&m, 1, 0), 0x040404);
        EQ(g3dc_get(&m, 0, 1), 0x070707);
        EQ(g3dc_get(&m, 1, 1), 0x080808);
        EQ(g3dc_get(&m, 2, 0), 0);
        g3dc_free(&m);
    }
}
TEST(gap3_sdib_24bpp_padding) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 8, 8)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 3, -2, 24, 0);       /* stride 12 */
        BYTE px[24]; memset(px, 0xee, sizeof px);
        int x, y;
        for (y = 0; y < 2; y++) for (x = 0; x < 3; x++) {
            BYTE *q = px + y * 12 + x * 3; q[0] = 10 * (x + 1); q[1] = 20 * (y + 1); q[2] = 5;
        }
        g3dc_clear(&m, RGB(0,0,0));
        EQ(S(m.dc, 0, 0, 3, 2, 0, 0, 3, 2, px, &b.bi, DIB_RGB_COLORS, SRCCOPY), 2);
        for (y = 0; y < 2; y++) for (x = 0; x < 3; x++)
            EQ(g3dc_get(&m, x, y), g3_px(RGB(5, 20 * (y + 1), 10 * (x + 1))));
        PXC(m, 3, 0, RGB(0,0,0));
        g3dc_free(&m);
    }
}
TEST(gap3_sdib_8bpp_palette) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 8, 8)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 4, -1, 8, 4);
        set_pal(&b, 0, 255, 0, 0); set_pal(&b, 1, 0, 255, 0); set_pal(&b, 2, 0, 0, 255); set_pal(&b, 3, 9, 8, 7);
        BYTE px[4] = { 3, 2, 1, 0 };
        g3dc_clear(&m, RGB(0,0,0));
        EQ(S(m.dc, 0, 0, 4, 1, 0, 0, 4, 1, px, &b.bi, DIB_RGB_COLORS, SRCCOPY), 1);
        PXC(m, 0, 0, RGB(9,8,7)); PXC(m, 1, 0, RGB(0,0,255)); PXC(m, 2, 0, RGB(0,255,0)); PXC(m, 3, 0, RGB(255,0,0));
        g3dc_free(&m);
    }
}
TEST(gap3_sdib_4bpp) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 8, 8)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 4, -1, 4, 16);
        int i; for (i = 0; i < 16; i++) set_pal(&b, i, i * 16, 255 - i * 16, i);
        BYTE px[4] = { 0x01, 0x23, 0, 0 };
        g3dc_clear(&m, RGB(0,0,0));
        EQ(S(m.dc, 0, 0, 4, 1, 0, 0, 4, 1, px, &b.bi, DIB_RGB_COLORS, SRCCOPY), 1);
        for (i = 0; i < 4; i++) PXC(m, i, 0, RGB(i * 16, 255 - i * 16, i));
        g3dc_free(&m);
    }
}
TEST(gap3_sdib_1bpp) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 16, 8)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 8, -1, 1, 2);
        set_pal(&b, 0, 0, 0, 0); set_pal(&b, 1, 200, 100, 50);
        BYTE px[4] = { 0xA5, 0, 0, 0 };
        g3dc_clear(&m, RGB(1,1,1));
        EQ(S(m.dc, 0, 0, 8, 1, 0, 0, 8, 1, px, &b.bi, DIB_RGB_COLORS, SRCCOPY), 1);
        int i;
        for (i = 0; i < 8; i++) PXC(m, i, 0, ((0xA5 >> (7 - i)) & 1) ? RGB(200,100,50) : RGB(0,0,0));
        g3dc_free(&m);
    }
}
TEST(gap3_sdib_16bpp_555) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 8, 8)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 2, -1, 16, 0);
        WORD px[2] = { 0x7c00, 0x03e0 };  /* red, green in 5-5-5 */
        g3dc_clear(&m, RGB(0,0,0));
        EQ(S(m.dc, 0, 0, 2, 1, 0, 0, 2, 1, px, &b.bi, DIB_RGB_COLORS, SRCCOPY), 1);
        PXC(m, 0, 0, RGB(255,0,0)); PXC(m, 1, 0, RGB(0,255,0));
        g3dc_free(&m);
    }
}
TEST(gap3_sdib_rops) {
    FOR_SD(S)
        static const struct { DWORD rop; DWORD expect; } tc[] = {
            { SRCCOPY, 0x336699 }, { NOTSRCCOPY, 0xcc9966 }, { SRCINVERT, 0x336699 ^ 0x0f0f0f },
            { SRCAND, 0x336699 & 0x0f0f0f }, { SRCPAINT, 0x336699 | 0x0f0f0f }, { BLACKNESS, 0 },
            { WHITENESS, 0xffffff }, { DSTINVERT, 0xf0f0f0 }, { NOTSRCERASE, (~(0x336699 | 0x0f0f0f)) & 0xffffff },
            { SRCERASE, 0x336699 & ~0x0f0f0f & 0xffffff }, { MERGEPAINT, (~0x336699 | 0x0f0f0f) & 0xffffff },
            { 0x00AA0029 /* D */, 0x0f0f0f } };
        unsigned k;
        for (k = 0; k < sizeof tc / sizeof *tc; k++) {
            G3DC m; if (!g3dc_make(&m, 4, 4)) { CHECK(0); return; }
            BMI b; bmi_init(&b, 1, -1, 32, 0);
            DWORD src = 0x336699;     /* 0x00RRGGBB in a 32bpp DIB */
            g3dc_clear(&m, RGB(0x0f, 0x0f, 0x0f));
            int r = S(m.dc, 0, 0, 1, 1, 0, 0, 1, 1, &src, &b.bi, DIB_RGB_COLORS, tc[k].rop);
            if (g3_wine() && r != 1) t_note("(emulator differs) rop %08lx ret %d", tc[k].rop, r);
            else {
                CHECKF(r == 1, "rop %08lx ret %d", tc[k].rop, r);
                CHECKF(g3dc_get(&m, 0, 0) == tc[k].expect, "rop %08lx got %06lx want %06lx", tc[k].rop,
                       (unsigned long)g3dc_get(&m, 0, 0), (unsigned long)tc[k].expect);
            }
            g3dc_free(&m);
        }
    }
}
TEST(gap3_sdib_stretch_up_2x) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 8, 8)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 2, -2, 32, 0);
        DWORD src[4] = { 0x111111, 0x222222, 0x333333, 0x444444 };
        U(SetStretchBltMode)(m.dc, COLORONCOLOR);
        g3dc_clear(&m, RGB(0,0,0));
        EQ(S(m.dc, 0, 0, 4, 4, 0, 0, 2, 2, src, &b.bi, DIB_RGB_COLORS, SRCCOPY), 2);
        int x, y;
        for (y = 0; y < 4; y++) for (x = 0; x < 4; x++)
            EQ(g3dc_get(&m, x, y), src[(y / 2) * 2 + (x / 2)]);
        PXC(m, 4, 0, RGB(0,0,0)); PXC(m, 0, 4, RGB(0,0,0));
        g3dc_free(&m);
    }
}
TEST(gap3_sdib_stretch_up_3x_nonuniform) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 12, 4)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 2, -1, 32, 0);
        DWORD src[2] = { 0x0000ff, 0xff0000 };
        U(SetStretchBltMode)(m.dc, COLORONCOLOR);
        g3dc_clear(&m, RGB(0,0,0));
        S(m.dc, 0, 0, 6, 1, 0, 0, 2, 1, src, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        int x;
        for (x = 0; x < 3; x++) EQ(g3dc_get(&m, x, 0), 0x0000ff);
        for (x = 3; x < 6; x++) EQ(g3dc_get(&m, x, 0), 0xff0000);
        PXC(m, 6, 0, RGB(0,0,0));
        g3dc_free(&m);
    }
}
TEST(gap3_sdib_shrink) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 8, 8)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 4, -4, 32, 0);
        DWORD src[16]; int i;
        for (i = 0; i < 16; i++) src[i] = 0x808080;
        U(SetStretchBltMode)(m.dc, COLORONCOLOR);
        g3dc_clear(&m, RGB(0,0,0));
        EQ(S(m.dc, 0, 0, 2, 2, 0, 0, 4, 4, src, &b.bi, DIB_RGB_COLORS, SRCCOPY), 4);
        EQ(g3dc_count(&m, RGB(0x80,0x80,0x80)), 4);
        PXC(m, 2, 0, RGB(0,0,0)); PXC(m, 0, 2, RGB(0,0,0));
        g3dc_free(&m);
    }
}
TEST(gap3_sdib_dest_clipped) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 4, 4)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 4, -4, 32, 0);
        DWORD src[16]; int i;
        for (i = 0; i < 16; i++) src[i] = 0xf0f0f0;
        g3dc_clear(&m, RGB(0,0,0));
        int r = S(m.dc, 2, 2, 4, 4, 0, 0, 4, 4, src, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        t_note("clipped copy returned %d", r);
        EQ(g3dc_count(&m, RGB(0xf0,0xf0,0xf0)), 4);
        PXC(m, 2, 2, RGB(0xf0,0xf0,0xf0)); PXC(m, 3, 3, RGB(0xf0,0xf0,0xf0)); PXC(m, 1, 1, RGB(0,0,0));
        r = S(m.dc, -2, -2, 4, 4, 0, 0, 4, 4, src, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        EQ(g3dc_count(&m, RGB(0xf0,0xf0,0xf0)), 8);
        S(m.dc, 100, 100, 4, 4, 0, 0, 4, 4, src, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        EQ(g3dc_count(&m, RGB(0xf0,0xf0,0xf0)), 8);
        g3dc_free(&m);
    }
}
TEST(gap3_sdib_clip_region) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 8, 8)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 8, -8, 32, 0);
        DWORD src[64]; int i;
        for (i = 0; i < 64; i++) src[i] = 0xabcdef;
        HRGN rg = U(CreateRectRgn)(2, 2, 5, 4);
        U(SelectClipRgn)(m.dc, rg);
        g3dc_clear(&m, RGB(0,0,0));
        S(m.dc, 0, 0, 8, 8, 0, 0, 8, 8, src, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        EQ(g3dc_count(&m, RGB(0xab,0xcd,0xef)), 6);
        U(SelectClipRgn)(m.dc, NULL); U(DeleteObject)(rg);
        g3dc_free(&m);
    }
}
TEST(gap3_sdib_mirror_horizontal) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 8, 2)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 4, -1, 32, 0);
        DWORD src[4] = { 0x010000, 0x020000, 0x030000, 0x040000 };
        g3dc_clear(&m, RGB(0,0,0));
        int r = S(m.dc, 7, 0, -4, 1, 0, 0, 4, 1, src, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        t_note("mirrored copy returned %d: %06lx %06lx %06lx %06lx %06lx", r, (unsigned long)g3dc_get(&m, 3, 0),
               (unsigned long)g3dc_get(&m, 4, 0), (unsigned long)g3dc_get(&m, 5, 0), (unsigned long)g3dc_get(&m, 6, 0),
               (unsigned long)g3dc_get(&m, 7, 0));
        g3dc_free(&m);
    }
}
TEST(gap3_sdib_flip_vertical_dest) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 4, 8)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 1, -3, 32, 0);
        DWORD src[3] = { 0x000001, 0x000002, 0x000003 };
        g3dc_clear(&m, RGB(0,0,0));
        int r = S(m.dc, 0, 5, 1, -3, 0, 0, 1, 3, src, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        t_note("vflipped copy returned %d: %06lx %06lx %06lx", r, (unsigned long)g3dc_get(&m, 0, 3),
               (unsigned long)g3dc_get(&m, 0, 4), (unsigned long)g3dc_get(&m, 0, 5));
        g3dc_free(&m);
    }
}
TEST(gap3_sdib_invalid_dc) {
    FOR_SD(S)
        BMI b; bmi_init(&b, 1, -1, 32, 0); DWORD px = 0;
        SET_ERR(0);
        int r = S(NULL, 0, 0, 1, 1, 0, 0, 1, 1, &px, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        CHECK(r == 0 || r == (int)GDI_ERROR);
        WERR(G3_ERR_INVALID_HANDLE);
        HDC st = g3_stale_dc();
        r = S(st, 0, 0, 1, 1, 0, 0, 1, 1, &px, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        CHECK(r == 0 || r == (int)GDI_ERROR);
        r = S((HDC)U(GetStockObject)(BLACK_PEN), 0, 0, 1, 1, 0, 0, 1, 1, &px, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        CHECK(r == 0 || r == (int)GDI_ERROR);
    }
}
TEST(gap3_sdib_null_args) {
    G3_WINE_SKIP("NULL bits / bmi");
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 4, 4)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 1, -1, 32, 0); DWORD px = 0x112233;
        g3dc_clear(&m, RGB(7,7,7));
        SET_ERR(0);
        int r = S(m.dc, 0, 0, 1, 1, 0, 0, 1, 1, NULL, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        t_note("NULL bits: ret %d err %lu", r, GetLastError());
        CHECK(r != 1);
        SET_ERR(0);
        r = S(m.dc, 0, 0, 1, 1, 0, 0, 1, 1, &px, NULL, DIB_RGB_COLORS, SRCCOPY);
        t_note("NULL bmi: ret %d err %lu", r, GetLastError());
        CHECK(r != 1);
        PXC(m, 0, 0, RGB(7,7,7));
        g3dc_free(&m);
    }
}
TEST(gap3_sdib_bad_usage) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 4, 4)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 1, -1, 32, 0); DWORD px = 0x112233;
        static const UINT bad[] = { 3, 4, 0x10, 0xffff, 0xffffffffu };
        unsigned k;
        for (k = 0; k < sizeof bad / sizeof *bad; k++) {
            g3dc_clear(&m, RGB(7,7,7));
            SET_ERR(0);
            int r = S(m.dc, 0, 0, 1, 1, 0, 0, 1, 1, &px, &b.bi, bad[k], SRCCOPY);
            t_note("usage 0x%x: ret %d err %lu px %06lx", bad[k], r, GetLastError(), (unsigned long)g3dc_get(&m, 0, 0));
        }
        g3dc_free(&m);
    }
}
TEST(gap3_sdib_bad_header) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 4, 4)) { CHECK(0); return; }
        DWORD px[4] = { 1, 2, 3, 4 };
        BMI b;
        g3dc_clear(&m, RGB(7,7,7));
        bmi_init(&b, 2, 0, 32, 0);                     /* zero height */
        SET_ERR(0);
        int r = S(m.dc, 0, 0, 2, 1, 0, 0, 2, 1, px, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        t_note("height 0: ret %d err %lu", r, GetLastError());
        CHECK(r != 1);
        bmi_init(&b, 0, 1, 32, 0);                     /* zero width */
        r = S(m.dc, 0, 0, 2, 1, 0, 0, 2, 1, px, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        t_note("width 0: ret %d", r);
        bmi_init(&b, 2, 1, 32, 0); b.bi.bmiHeader.biPlanes = 7;
        r = S(m.dc, 0, 0, 2, 1, 0, 0, 2, 1, px, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        t_note("planes 7: ret %d", r);
        bmi_init(&b, 2, 1, 32, 0); b.bi.bmiHeader.biBitCount = 3;
        SET_ERR(0);
        r = S(m.dc, 0, 0, 2, 1, 0, 0, 2, 1, px, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        t_note("bitcount 3: ret %d err %lu", r, GetLastError());
        CHECK(r != 1);
        bmi_init(&b, 2, 1, 32, 0); b.bi.bmiHeader.biSize = 4;
        SET_ERR(0);
        r = S(m.dc, 0, 0, 2, 1, 0, 0, 2, 1, px, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        t_note("biSize 4: ret %d err %lu", r, GetLastError());
        CHECK(r != 1);
        g3dc_free(&m);
    }
}
TEST(gap3_sdib_zero_dest_size) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 4, 4)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 1, -1, 32, 0); DWORD px = 0x112233;
        g3dc_clear(&m, RGB(7,7,7));
        int r = S(m.dc, 0, 0, 0, 0, 0, 0, 1, 1, &px, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        t_note("zero dest: ret %d", r);
        PXC(m, 0, 0, RGB(7,7,7));
        r = S(m.dc, 0, 0, 1, 1, 0, 0, 0, 0, &px, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        t_note("zero src: ret %d", r);
        g3dc_free(&m);
    }
}
TEST(gap3_sdib_src_out_of_range) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 4, 4)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 2, -2, 32, 0); DWORD px[4] = { 1, 2, 3, 4 };
        g3dc_clear(&m, RGB(7,7,7));
        int r = S(m.dc, 0, 0, 2, 2, 100, 100, 2, 2, px, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        t_note("source rect outside: ret %d", r);
        PXC(m, 0, 0, RGB(7,7,7));
        r = S(m.dc, 0, 0, 2, 2, -5, -5, 2, 2, px, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        t_note("negative source origin: ret %d", r);
        g3dc_free(&m);
    }
}
TEST(gap3_sdib_pal_colors_no_palette) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 4, 4)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 2, -1, 8, 2);
        WORD *idx = (WORD *)b.bi.bmiColors; idx[0] = 0; idx[1] = 1;
        BYTE px[4] = { 0, 1, 0, 0 };
        g3dc_clear(&m, RGB(7,7,7));
        SET_ERR(0);
        int r = S(m.dc, 0, 0, 2, 1, 0, 0, 2, 1, px, &b.bi, DIB_PAL_COLORS, SRCCOPY);
        t_note("DIB_PAL_COLORS ret %d err %lu px %06lx %06lx", r, GetLastError(), (unsigned long)g3dc_get(&m, 0, 0), (unsigned long)g3dc_get(&m, 1, 0));
        g3dc_free(&m);
    }
}
TEST(gap3_sdib_pal_colors_stock_palette) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 4, 4)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 2, -1, 8, 2);
        WORD *idx = (WORD *)b.bi.bmiColors; idx[0] = 0; idx[1] = 1;
        BYTE px[4] = { 0, 1, 0, 0 };
        HPALETTE pal = U(GetStockObject)(DEFAULT_PALETTE);
        HGDIOBJ op = U(SelectPalette)(m.dc, pal, FALSE);
        g3dc_clear(&m, RGB(7,7,7));
        int r = S(m.dc, 0, 0, 2, 1, 0, 0, 2, 1, px, &b.bi, DIB_PAL_COLORS, SRCCOPY);
        t_note("PAL_COLORS stock palette ret %d px %06lx %06lx", r, (unsigned long)g3dc_get(&m, 0, 0), (unsigned long)g3dc_get(&m, 1, 0));
        U(SelectPalette)(m.dc, op, FALSE);
        g3dc_free(&m);
    }
}
TEST(gap3_sdib_bitfields_32) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 4, 4)) { CHECK(0); return; }
        union { BITMAPINFO bi; char raw[sizeof(BITMAPINFOHEADER) + 12]; } b;
        memset(&b, 0, sizeof b);
        b.bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        b.bi.bmiHeader.biWidth = 1; b.bi.bmiHeader.biHeight = -1; b.bi.bmiHeader.biPlanes = 1;
        b.bi.bmiHeader.biBitCount = 32; b.bi.bmiHeader.biCompression = BI_BITFIELDS;
        DWORD *mk = (DWORD *)b.bi.bmiColors; mk[0] = 0x0000ff; mk[1] = 0x00ff00; mk[2] = 0xff0000;  /* BGR-swapped masks */
        DWORD px = 0x0000ff;      /* with these masks this is full "red" */
        g3dc_clear(&m, RGB(0,0,0));
        int r = S(m.dc, 0, 0, 1, 1, 0, 0, 1, 1, &px, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        CHECK(r == 1);
        PXC(m, 0, 0, RGB(255,0,0));
        g3dc_free(&m);
    }
}
TEST(gap3_sdib_dest_dib_vs_screen_consistency) {
    /* the same call to two memory DCs of different depth gives matching colour for primaries */
    FOR_SD(S)
        BMI b; bmi_init(&b, 1, -1, 32, 0); DWORD px = 0x0000ff;
        HDC d = g3_mem(); HBITMAP bm = U(CreateBitmap)(2, 2, 1, 1, NULL);
        HGDIOBJ ob = U(SelectObject)(d, bm);
        U(PatBlt)(d, 0, 0, 2, 2, BLACKNESS);
        int r = S(d, 0, 0, 1, 1, 0, 0, 1, 1, &px, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        EQ(r, 1);
        t_note("mono dest pixel after red source = %06lx", (unsigned long)U(GetPixel)(d, 0, 0));
        U(SelectObject)(d, ob); U(DeleteObject)(bm); U(DeleteDC)(d);
    }
}
TEST(gap3_sdib_large_stretch) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 512, 512)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 16, -16, 32, 0);
        DWORD *px = malloc(16 * 16 * 4); int i;
        for (i = 0; i < 256; i++) px[i] = 0x123456;
        U(SetStretchBltMode)(m.dc, COLORONCOLOR);
        g3dc_clear(&m, RGB(0,0,0));
        EQ(S(m.dc, 0, 0, 512, 512, 0, 0, 16, 16, px, &b.bi, DIB_RGB_COLORS, SRCCOPY), 16);
        EQ(g3dc_count(&m, RGB(0x12, 0x34, 0x56)), 512 * 512);
        free(px); g3dc_free(&m);
    }
}
TEST(gap3_sdib_gdicount) {
    FOR_SD(S)
        DWORD before = g3_gui();
        G3DC m; if (!g3dc_make(&m, 8, 8)) { CHECK(0); return; }
        BMI b; bmi_init(&b, 1, -1, 32, 0); DWORD px = 1; int i;
        for (i = 0; i < 1000; i++) S(m.dc, 0, 0, 1, 1, 0, 0, 1, 1, &px, &b.bi, DIB_RGB_COLORS, SRCCOPY);
        g3dc_free(&m);
        EQ(g3_gui(), before);
    }
}
static void sd_thread(void *p) {
    void **a = p; BMI b; bmi_init(&b, 1, -1, 32, 0); DWORD px = 0x778899;
    a[1] = (void *)(ULONG_PTR)((SDIB)a[2])((HDC)a[0], 0, 0, 1, 1, 0, 0, 1, 1, &px, &b.bi, DIB_RGB_COLORS, SRCCOPY);
}
TEST(gap3_sdib_cross_thread) {
    FOR_SD(S)
        G3DC m; if (!g3dc_make(&m, 4, 4)) { CHECK(0); return; }
        void *a[3]; a[0] = m.dc; a[1] = NULL; a[2] = (void *)S;
        g3_run_thread(sd_thread, a);
        EQ((int)(ULONG_PTR)a[1], 1);
        PXC(m, 0, 0, RGB(0x77, 0x88, 0x99));
        g3dc_free(&m);
    }
}
TEST(gap3_sdib_in_path_fails) {
    G3DC m; if (!g3dc_make(&m, 4, 4)) { CHECK(0); return; }
    BMI b; bmi_init(&b, 1, -1, 32, 0); DWORD px = 0x112233;
    g3dc_clear(&m, RGB(7,7,7));
    U(BeginPath)(m.dc);
    int r = U(StretchDIBits)(m.dc, 0, 0, 1, 1, 0, 0, 1, 1, &px, &b.bi, DIB_RGB_COLORS, SRCCOPY);
    t_note("StretchDIBits in path: %d", r);
    U(AbortPath)(m.dc);
    g3dc_free(&m);
}
TEST(gap3_sdib_wmf_dc) {
    REQUIRE(U(CreateMetaFileW));
    HDC mf = U(CreateMetaFileW)(NULL);
    if (!mf) { t_skip("no wmf dc"); return; }
    BMI b; bmi_init(&b, 1, -1, 24, 0); BYTE px[4] = { 1, 2, 3, 0 };
    int r = U(StretchDIBits)(mf, 0, 0, 1, 1, 0, 0, 1, 1, px, &b.bi, DIB_RGB_COLORS, SRCCOPY);
    t_note("StretchDIBits on wmf dc = %d", r);
    HMETAFILE h = U(CloseMetaFile)(mf);
    if (h) U(DeleteMetaFile)(h);
}
