#include "pb.h"

#define NEED_R() do { NEED_MEM(); REQUIRE(U(PatBlt)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(CreateHatchBrush)); \
    REQUIRE(U(SetBkMode)); REQUIRE(U(SetBkColor)); REQUIRE(U(SetTextColor)); REQUIRE(U(SetBrushOrgEx)); } while (0)

TEST(brush_render_solid_32bpp_colors)
{
    NEED_R();
    static const COLORREF c[] = { 0, 0xffffff, 0x0000ff, 0x00ff00, 0xff0000, 0x123456, 0xfedcba, 0x010101, 0x808080, 0x7f7f7f };
    PBM m;
    CHECK(pb_mem(&m, 16, 16, 32));
    for (unsigned i = 0; i < sizeof c / sizeof *c; i++) {
        pb_clear(&m, 0xabcdef);
        HBRUSH b = U(CreateSolidBrush)(c[i]);
        CHECK(pb_fill(&m, b, 0, 0, 16, 16, PATCOPY));
        SURF_CHECK(&m, "solid fill", pb_raw(&m, c[i]));
        U(DeleteObject)(b);
    }
    pb_free(&m);
}

TEST(brush_render_solid_24bpp_colors)
{
    NEED_R();
    PBM m;
    CHECK(pb_mem(&m, 11, 7, 24));
    for (int v = 0; v < 256; v += 15) {
        COLORREF c = RGB(v, 255 - v, (v * 5) & 255);
        HBRUSH b = U(CreateSolidBrush)(c);
        pb_clear(&m, 0x010203);
        CHECK(pb_fill(&m, b, 0, 0, 11, 7, PATCOPY));
        SURF_CHECK(&m, "solid 24bpp", pb_raw(&m, c));
        U(DeleteObject)(b);
    }
    pb_free(&m);
}

TEST(brush_render_solid_16bpp_primaries)
{
    NEED_R();
    PBM m;
    CHECK(pb_mem(&m, 9, 9, 16));
    static const COLORREF c[] = { 0, 0xffffff, 0x0000ff, 0x00ff00, 0xff0000, 0x080808, 0xf8f8f8, 0x804020 };
    for (unsigned i = 0; i < sizeof c / sizeof *c; i++) {
        HBRUSH b = U(CreateSolidBrush)(c[i]);
        pb_clear(&m, 0x1234);
        CHECK(pb_fill(&m, b, 0, 0, 9, 9, PATCOPY));
        SURF_CHECK(&m, "solid 16bpp", pb_raw(&m, c[i]));
        U(DeleteObject)(b);
    }
    pb_free(&m);
}

TEST(brush_render_solid_1bpp_black_white)
{
    NEED_R();
    PBM m;
    CHECK(pb_mem(&m, 33, 5, 1));
    HBRUSH w = U(CreateSolidBrush)(0xffffff), k = U(CreateSolidBrush)(0);
    pb_clear(&m, 1);
    CHECK(pb_fill(&m, k, 0, 0, 33, 5, PATCOPY));
    SURF_CHECK(&m, "black", 0);
    CHECK(pb_fill(&m, w, 0, 0, 33, 5, PATCOPY));
    SURF_CHECK(&m, "white", 1);
    CHECK(pb_fill(&m, k, 3, 1, 29, 3, PATCOPY));
    SURF_CHECK(&m, "partial", (x >= 3 && x < 32 && y >= 1 && y < 4) ? 0 : 1);
    U(DeleteObject)(w); U(DeleteObject)(k);
    pb_free(&m);
}

TEST(brush_render_solid_1bpp_midtones_note)
{
    NEED_R();
    PBM m;
    CHECK(pb_mem(&m, 16, 16, 1));
    static const COLORREF c[] = { 0x303030, 0x7f7f7f, 0x808080, 0xc0c0c0, 0xff0000, 0x00ff00, 0x0000ff };
    for (unsigned i = 0; i < sizeof c / sizeof *c; i++) {
        HBRUSH b = U(CreateSolidBrush)(c[i]);
        pb_clear(&m, 0);
        pb_fill(&m, b, 0, 0, 16, 16, PATCOPY);
        pb_sync();
        t_note("1bpp solid %06lx -> %d set pixels of 256", c[i], pb_count_ne(&m, 0));
        U(DeleteObject)(b);
    }
    pb_free(&m);
}

TEST(brush_render_patblt_rect_bounds)
{
    NEED_R();
    static const int r[][4] = { {0,0,1,1}, {5,5,1,1}, {2,3,10,4}, {0,0,32,32}, {31,31,1,1}, {10,0,1,32}, {0,10,32,1},
        {7,9,13,2}, {-5,-5,10,10}, {28,28,20,20}, {-10,-10,100,100}, {16,16,0,5}, {16,16,5,0}, {40,40,5,5}, {-20,3,10,10} };
    PBM m;
    CHECK(pb_mem(&m, 32, 32, 32));
    HBRUSH b = U(CreateSolidBrush)(0x3366cc);
    unsigned fg = pb_raw(&m, 0x3366cc);
    for (unsigned i = 0; i < sizeof r / sizeof *r; i++) {
        pb_clear(&m, 0xffffff);
        BOOL ok = pb_fill(&m, b, r[i][0], r[i][1], r[i][2], r[i][3], PATCOPY);
        CHECKF(ok, "rect %u", i);
        int x0 = r[i][0], y0 = r[i][1], x1 = x0 + r[i][2], y1 = y0 + r[i][3];
        char nm[32]; sprintf(nm, "rect %u", i);
        SURF_CHECK(&m, nm, (x >= x0 && x < x1 && y >= y0 && y < y1) ? fg : 0xffffff);
    }
    U(DeleteObject)(b);
    pb_free(&m);
}

TEST(brush_render_patblt_negative_extent_note)
{
    NEED_R();
    PBM m;
    CHECK(pb_mem(&m, 32, 32, 32));
    HBRUSH b = U(CreateSolidBrush)(0);
    pb_clear(&m, 0xffffff);
    BOOL r = pb_fill(&m, b, 20, 20, -5, -5, PATCOPY);
    t_note("PatBlt negative extents returned %d, %d black pixels", r, pb_count_ne(&m, 0xffffff));
    pb_clear(&m, 0xffffff);
    r = pb_fill(&m, b, 20, 20, -5, 5, PATCOPY);
    t_note("PatBlt negative width returned %d, %d black pixels", r, pb_count_ne(&m, 0xffffff));
    U(DeleteObject)(b);
    pb_free(&m);
}

TEST(brush_render_patblt_zero_extent)
{
    NEED_R();
    PBM m;
    CHECK(pb_mem(&m, 8, 8, 32));
    HBRUSH b = U(CreateSolidBrush)(0);
    pb_clear(&m, 0x55aa55);
    BOOL r1 = pb_fill(&m, b, 2, 2, 0, 0, PATCOPY);
    BOOL r2 = pb_fill(&m, b, 2, 2, 4, 0, PATCOPY);
    BOOL r3 = pb_fill(&m, b, 2, 2, 0, 4, PATCOPY);
    t_note("zero extent returns %d %d %d", r1, r2, r3);
    CHECK(pb_count_ne(&m, 0x55aa55) == 0);
    U(DeleteObject)(b);
    pb_free(&m);
}

static unsigned rop_model(DWORD rop, unsigned P, unsigned D)
{
    unsigned r;
    switch (rop) {
    case 0x00F00021: r = P; break;                 /* PATCOPY */
    case 0x005A0049: r = P ^ D; break;             /* PATINVERT */
    case 0x00550009: r = ~D; break;                /* DSTINVERT */
    case 0x00000042: r = 0; break;                 /* BLACKNESS */
    case 0x00FF0062: r = ~0u; break;               /* WHITENESS */
    case 0x00A000C9: r = D & P; break;             /* DPa */
    case 0x00FA0089: r = D | P; break;             /* DPo */
    case 0x000F0001: r = ~P; break;                /* Pn */
    case 0x00F50225: r = P | ~D; break;            /* PDno */
    case 0x00A50065: r = ~(P ^ D); break;          /* PDxn */
    case 0x00500325: r = P & ~D; break;            /* PDna */
    case 0x00AF0229: r = D | ~P; break;            /* DPno */
    case 0x000A0329: r = D & ~P; break;            /* DPna */
    case 0x00AA0029: r = D; break;                 /* D */
    case 0x00050225: r = ~(P | D); break;          /* DPon */
    case 0x005F0229: r = ~(P & D); break;          /* DPan */
    default: r = 0; break;
    }
    return r & 0xffffff;
}

TEST(brush_render_patblt_rop_table_32bpp)
{
    NEED_R();
    static const DWORD rops[] = { 0x00F00021, 0x005A0049, 0x00550009, 0x00000042, 0x00FF0062, 0x00A000C9, 0x00FA0089,
        0x000F0001, 0x00F50225, 0x00A50065, 0x00500325, 0x00AF0229, 0x000A0329, 0x00AA0029, 0x00050225, 0x005F0229 };
    PBM m;
    CHECK(pb_mem(&m, 8, 8, 32));
    static const COLORREF dcol[] = { 0x00ff00ff, 0x00123456, 0, 0xffffff };
    for (unsigned i = 0; i < sizeof rops / sizeof *rops; i++)
    for (unsigned d = 0; d < 4; d++) {
        HBRUSH b = U(CreateSolidBrush)(RGB(0x3c, 0xa5, 0x96));
        pb_clear(&m, pb_raw(&m, dcol[d]));
        BOOL ok = pb_fill(&m, b, 0, 0, 8, 8, rops[i]);
        CHECKF(ok, "rop %06lx", rops[i]);
        char nm[48]; sprintf(nm, "rop %06lx dst %06lx", rops[i], dcol[d]);
        SURF_CHECK(&m, nm, rop_model(rops[i], pb_raw(&m, RGB(0x3c, 0xa5, 0x96)), pb_raw(&m, dcol[d])));
        U(DeleteObject)(b);
    }
    pb_free(&m);
}

TEST(brush_render_patblt_rop_table_1bpp)
{
    NEED_R();
    static const DWORD rops[] = { 0x00F00021, 0x005A0049, 0x00550009, 0x00000042, 0x00FF0062, 0x00A000C9, 0x00FA0089,
        0x000F0001, 0x00F50225, 0x00A50065, 0x00500325 };
    PBM m;
    CHECK(pb_mem(&m, 16, 4, 1));
    for (unsigned i = 0; i < sizeof rops / sizeof *rops; i++)
    for (int pc = 0; pc < 2; pc++)
    for (int dc = 0; dc < 2; dc++) {
        HBRUSH b = U(CreateSolidBrush)(pc ? 0xffffff : 0);
        pb_clear(&m, dc);
        pb_fill(&m, b, 0, 0, 16, 4, rops[i]);
        char nm[48]; sprintf(nm, "rop %06lx P=%d D=%d", rops[i], pc, dc);
        SURF_CHECK(&m, nm, rop_model(rops[i], pc ? 0xffffff : 0, dc ? 0xffffff : 0) & 1);
        U(DeleteObject)(b);
    }
    pb_free(&m);
}

TEST(brush_render_patblt_source_rop_behaviour)
{
    NEED_R();
    PBM m;
    CHECK(pb_mem(&m, 8, 8, 32));
    HBRUSH b = U(CreateSolidBrush)(0x112233);
    static const DWORD rops[] = { SRCCOPY, SRCPAINT, SRCAND, SRCINVERT, NOTSRCCOPY, MERGECOPY, PATPAINT, 0x00CA0749 };
    for (unsigned i = 0; i < sizeof rops / sizeof *rops; i++) {
        pb_clear(&m, 0x445566);
        SetLastError(0xdeadbeef);
        BOOL r = pb_fill(&m, b, 0, 0, 8, 8, rops[i]);
        t_note("PatBlt rop %06lx -> %d gle=%lx changed=%d", rops[i], r, GetLastError(), pb_count_ne(&m, 0x445566));
    }
    U(DeleteObject)(b);
    pb_free(&m);
}

TEST(brush_render_hatch_opaque_all_32bpp)
{
    NEED_R();
    PBM m;
    CHECK(pb_mem(&m, 24, 24, 32));
    for (int h = HS_HORIZONTAL; h <= HS_DIAGCROSS; h++) {
        HBRUSH b = U(CreateHatchBrush)(h, RGB(200, 10, 20));
        U(SetBkMode)(m.dc, OPAQUE);
        U(SetBkColor)(m.dc, RGB(10, 220, 30));
        pb_clear(&m, 0x777777);
        CHECK(pb_fill(&m, b, 0, 0, 24, 24, PATCOPY));
        unsigned fg = pb_raw(&m, RGB(200, 10, 20)), bk = pb_raw(&m, RGB(10, 220, 30));
        char nm[32]; sprintf(nm, "hatch %d", h);
        SURF_CHECK(&m, nm, pb_hatch_bit(h, x, y) ? fg : bk);
        U(DeleteObject)(b);
    }
    pb_free(&m);
}

TEST(brush_render_hatch_transparent_all_32bpp)
{
    NEED_R();
    PBM m;
    CHECK(pb_mem(&m, 24, 24, 32));
    for (int h = HS_HORIZONTAL; h <= HS_DIAGCROSS; h++) {
        HBRUSH b = U(CreateHatchBrush)(h, RGB(0, 0, 255));
        U(SetBkMode)(m.dc, TRANSPARENT);
        U(SetBkColor)(m.dc, RGB(255, 0, 0));
        pb_clear(&m, 0x777777);
        CHECK(pb_fill(&m, b, 0, 0, 24, 24, PATCOPY));
        char nm[32]; sprintf(nm, "hatch %d", h);
        SURF_CHECK(&m, nm, pb_hatch_bit(h, x, y) ? pb_raw(&m, RGB(0, 0, 255)) : 0x777777);
        U(DeleteObject)(b);
        U(SetBkMode)(m.dc, OPAQUE);
    }
    pb_free(&m);
}

TEST(brush_render_hatch_default_bk_is_white)
{
    NEED_R();
    PBM m;
    CHECK(pb_mem(&m, 16, 16, 32));
    HBRUSH b = U(CreateHatchBrush)(HS_CROSS, 0);
    pb_clear(&m, 0x123456);
    CHECK(pb_fill(&m, b, 0, 0, 16, 16, PATCOPY));
    SURF_CHECK(&m, "default bk", pb_hatch_bit(HS_CROSS, x, y) ? 0 : 0xffffff);
    U(DeleteObject)(b);
    pb_free(&m);
}

TEST(brush_render_hatch_sub_rect_alignment)
{
    NEED_R();
    PBM m;
    CHECK(pb_mem(&m, 40, 40, 32));
    for (int h = HS_HORIZONTAL; h <= HS_DIAGCROSS; h++) {
        HBRUSH b = U(CreateHatchBrush)(h, 0xff);
        U(SetBkMode)(m.dc, TRANSPARENT);
        pb_clear(&m, 0xffffff);
        CHECK(pb_fill(&m, b, 5, 11, 23, 17, PATCOPY));
        char nm[32]; sprintf(nm, "hatch %d sub", h);
        SURF_CHECK(&m, nm, (x >= 5 && x < 28 && y >= 11 && y < 28 && pb_hatch_bit(h, x, y)) ? pb_raw(&m, 0xff) : 0xffffff);
        U(DeleteObject)(b);
        U(SetBkMode)(m.dc, OPAQUE);
    }
    pb_free(&m);
}

TEST(brush_render_hatch_bk_color_changes)
{
    NEED_R();
    PBM m;
    CHECK(pb_mem(&m, 16, 16, 32));
    HBRUSH b = U(CreateHatchBrush)(HS_VERTICAL, RGB(255, 255, 255));
    for (int i = 0; i < 6; i++) {
        COLORREF bk = RGB(i * 40, 255 - i * 40, i * 7);
        U(SetBkColor)(m.dc, bk);
        pb_fill(&m, b, 0, 0, 16, 16, PATCOPY);
        SURF_CHECK(&m, "bk", pb_hatch_bit(HS_VERTICAL, x, y) ? 0xffffff : pb_raw(&m, bk));
    }
    U(DeleteObject)(b);
    pb_free(&m);
}

TEST(brush_render_hatch_textcolor_ignored)
{
    NEED_R();
    PBM m;
    CHECK(pb_mem(&m, 16, 16, 32));
    HBRUSH b = U(CreateHatchBrush)(HS_HORIZONTAL, RGB(0, 128, 0));
    U(SetTextColor)(m.dc, RGB(255, 0, 255));
    pb_fill(&m, b, 0, 0, 16, 16, PATCOPY);
    SURF_CHECK(&m, "text color irrelevant", pb_hatch_bit(HS_HORIZONTAL, x, y) ? pb_raw(&m, RGB(0, 128, 0)) : 0xffffff);
    U(DeleteObject)(b);
    pb_free(&m);
}

TEST(brush_render_hatch_bkmode_values)
{
    NEED_R();
    PBM m;
    CHECK(pb_mem(&m, 16, 16, 32));
    HBRUSH b = U(CreateHatchBrush)(HS_BDIAGONAL, RGB(1, 2, 3));
    U(SetBkColor)(m.dc, RGB(9, 9, 9));
    int modes[] = { TRANSPARENT, OPAQUE };
    for (int i = 0; i < 2; i++) {
        U(SetBkMode)(m.dc, modes[i]);
        pb_clear(&m, 0x00aaaa);
        pb_fill(&m, b, 0, 0, 16, 16, PATCOPY);
        SURF_CHECK(&m, "bkmode", pb_hatch_bit(HS_BDIAGONAL, x, y) ? pb_raw(&m, RGB(1, 2, 3)) : (modes[i] == OPAQUE ? pb_raw(&m, RGB(9, 9, 9)) : 0x00aaaa));
    }
    U(DeleteObject)(b);
    pb_free(&m);
}

TEST(brush_render_hatch_patinvert)
{
    NEED_R();
    PBM m;
    CHECK(pb_mem(&m, 16, 16, 32));
    HBRUSH b = U(CreateHatchBrush)(HS_FDIAGONAL, RGB(255, 0, 0));
    pb_clear(&m, 0x00ff00);
    pb_fill(&m, b, 0, 0, 16, 16, PATINVERT);
    pb_sync();
    int bad = 0;
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
        unsigned g = pb_get(&m, x, y);
        /* line pixels must differ from the untouched background */
        if (pb_hatch_bit(HS_FDIAGONAL, x, y) && g == 0x00ff00) bad++;
    }
    t_note("PATINVERT hatch: %d unchanged line pixels", bad);
    U(DeleteObject)(b);
    pb_free(&m);
}

TEST(brush_render_hatch_1bpp)
{
    NEED_R();
    PBM m;
    CHECK(pb_mem(&m, 24, 24, 1));
    for (int h = HS_HORIZONTAL; h <= HS_DIAGCROSS; h++) {
        HBRUSH b = U(CreateHatchBrush)(h, 0);
        U(SetBkColor)(m.dc, 0xffffff);
        U(SetBkMode)(m.dc, OPAQUE);
        pb_clear(&m, 1);
        pb_fill(&m, b, 0, 0, 24, 24, PATCOPY);
        char nm[32]; sprintf(nm, "hatch %d 1bpp opaque", h);
        SURF_CHECK(&m, nm, pb_hatch_bit(h, x, y) ? 0 : 1);
        U(SetBkMode)(m.dc, TRANSPARENT);
        pb_clear(&m, 0);
        pb_fill(&m, b, 0, 0, 24, 24, PATCOPY);
        sprintf(nm, "hatch %d 1bpp transparent", h);
        SURF_CHECK(&m, nm, 0);
        pb_clear(&m, 1);
        pb_fill(&m, b, 0, 0, 24, 24, PATCOPY);
        sprintf(nm, "hatch %d 1bpp transparent on white", h);
        SURF_CHECK(&m, nm, pb_hatch_bit(h, x, y) ? 0 : 1);
        U(DeleteObject)(b);
        U(SetBkMode)(m.dc, OPAQUE);
    }
    pb_free(&m);
}

TEST(brush_render_hatch_origin_shift_note)
{
    NEED_R();
    PBM m;
    CHECK(pb_mem(&m, 24, 24, 32));
    HBRUSH b = U(CreateHatchBrush)(HS_FDIAGONAL, 0);
    U(SetBkMode)(m.dc, TRANSPARENT);
    static const int o[][2] = { {1,0}, {0,1}, {3,5}, {-2,-7}, {8,8}, {9,17} };
    for (unsigned i = 0; i < 6; i++) {
        U(SetBrushOrgEx)(m.dc, o[i][0], o[i][1], NULL);
        pb_clear(&m, 0xffffff);
        pb_fill(&m, b, 0, 0, 24, 24, PATCOPY);
        pb_sync();
        int bad_shift = 0, bad_fixed = 0;
        for (int y = 0; y < 24; y++) for (int x = 0; x < 24; x++) {
            unsigned g = pb_get(&m, x, y);
            if ((g == 0) != pb_hatch_bit(HS_FDIAGONAL, pb_mod8(x - o[i][0]), pb_mod8(y - o[i][1]))) bad_shift++;
            if ((g == 0) != pb_hatch_bit(HS_FDIAGONAL, x, y)) bad_fixed++;
        }
        t_note("hatch org (%d,%d): mismatches vs shifted model %d, vs fixed model %d", o[i][0], o[i][1], bad_shift, bad_fixed);
        /* exactly one of the two must describe the output */
        CHECK(bad_shift == 0 || bad_fixed == 0);
    }
    U(SetBrushOrgEx)(m.dc, 0, 0, NULL);
    U(DeleteObject)(b);
    pb_free(&m);
}

/* 8x8 24bpp bottom-up packed DIB with unique colour per pixel; colour(x,y) with y measured from the top. */
static COLORREF pat_col(int x, int y) { return RGB(20 + x * 25, 30 + y * 25, 40 + x * 3 + y * 3); }
static BITMAPINFO *mk_pat24(int w, int h)
{
    size_t stride = ((size_t)w * 3 + 3) & ~3u;
    BITMAPINFO *bi = calloc(1, sizeof(BITMAPINFOHEADER) + stride * h);
    bi->bmiHeader.biSize = sizeof bi->bmiHeader;
    bi->bmiHeader.biWidth = w; bi->bmiHeader.biHeight = h; bi->bmiHeader.biPlanes = 1;
    bi->bmiHeader.biBitCount = 24; bi->bmiHeader.biCompression = BI_RGB;
    BYTE *bits = (BYTE *)bi + sizeof(BITMAPINFOHEADER);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        BYTE *p = bits + (size_t)(h - 1 - y) * stride + x * 3;
        COLORREF c = pat_col(x, y);
        p[0] = GetBValue(c); p[1] = GetGValue(c); p[2] = GetRValue(c);
    }
    return bi;
}

TEST(brush_render_dibpattern_origin_matrix)
{
    NEED_R(); REQUIRE(U(CreateDIBPatternBrushPt));
    BITMAPINFO *bi = mk_pat24(8, 8);
    HBRUSH b = U(CreateDIBPatternBrushPt)(bi, DIB_RGB_COLORS);
    CHECK(b != NULL);
    PBM m;
    CHECK(pb_mem(&m, 40, 40, 32));
    static const int o[][2] = { {0,0}, {1,0}, {0,1}, {3,5}, {7,7}, {8,8}, {9,17}, {-1,-1}, {-8,-8}, {-13,-22}, {100,100},
        {1000,-1000}, {0x10000, 0x10001}, {-32769, 32768} };
    for (unsigned i = 0; i < sizeof o / sizeof *o; i++) {
        U(SetBrushOrgEx)(m.dc, o[i][0], o[i][1], NULL);
        pb_clear(&m, 0);
        CHECK(pb_fill(&m, b, 0, 0, 40, 40, PATCOPY));
        char nm[48]; sprintf(nm, "origin (%d,%d)", o[i][0], o[i][1]);
        SURF_CHECK(&m, nm, pb_raw(&m, pat_col(pb_mod8(x - o[i][0]), pb_mod8(y - o[i][1]))));
    }
    U(DeleteObject)(b);
    pb_free(&m);
    free(bi);
}

TEST(brush_render_dibpattern_default_origin_exact)
{
    NEED_R(); REQUIRE(U(CreateDIBPatternBrushPt));
    BITMAPINFO *bi = mk_pat24(8, 8);
    HBRUSH b = U(CreateDIBPatternBrushPt)(bi, DIB_RGB_COLORS);
    PBM m;
    CHECK(pb_mem(&m, 16, 16, 32));
    CHECK(pb_fill(&m, b, 0, 0, 16, 16, PATCOPY));
    pb_sync();
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++)
        CHECKF(pb_get(&m, x, y) == pb_raw(&m, pat_col(x, y)), "(%d,%d) got %x", x, y, pb_get(&m, x, y));
    U(DeleteObject)(b);
    pb_free(&m);
    free(bi);
}

TEST(brush_render_dibpattern_sub_rect)
{
    NEED_R(); REQUIRE(U(CreateDIBPatternBrushPt));
    BITMAPINFO *bi = mk_pat24(8, 8);
    HBRUSH b = U(CreateDIBPatternBrushPt)(bi, DIB_RGB_COLORS);
    PBM m;
    CHECK(pb_mem(&m, 40, 40, 32));
    pb_clear(&m, 0xffffff);
    CHECK(pb_fill(&m, b, 13, 6, 19, 22, PATCOPY));
    SURF_CHECK(&m, "sub rect", (x >= 13 && x < 32 && y >= 6 && y < 28) ? pb_raw(&m, pat_col(x & 7, y & 7)) : 0xffffff);
    U(DeleteObject)(b);
    pb_free(&m);
    free(bi);
}

TEST(brush_render_dibpattern_copied_at_creation)
{
    NEED_R(); REQUIRE(U(CreateDIBPatternBrushPt));
    BITMAPINFO *bi = mk_pat24(8, 8);
    HBRUSH b = U(CreateDIBPatternBrushPt)(bi, DIB_RGB_COLORS);
    memset((BYTE *)bi + sizeof(BITMAPINFOHEADER), 0xff, 8 * 24);
    PBM m;
    CHECK(pb_mem(&m, 8, 8, 32));
    CHECK(pb_fill(&m, b, 0, 0, 8, 8, PATCOPY));
    SURF_CHECK(&m, "copy semantics", pb_raw(&m, pat_col(x, y)));
    U(DeleteObject)(b);
    pb_free(&m);
    free(bi);
}

TEST(brush_render_dibpattern_on_24bpp_dest)
{
    NEED_R(); REQUIRE(U(CreateDIBPatternBrushPt));
    BITMAPINFO *bi = mk_pat24(8, 8);
    HBRUSH b = U(CreateDIBPatternBrushPt)(bi, DIB_RGB_COLORS);
    PBM m;
    CHECK(pb_mem(&m, 19, 13, 24));
    U(SetBrushOrgEx)(m.dc, 3, 2, NULL);
    CHECK(pb_fill(&m, b, 0, 0, 19, 13, PATCOPY));
    SURF_CHECK(&m, "24bpp dest", pb_raw(&m, pat_col(pb_mod8(x - 3), pb_mod8(y - 2))));
    U(DeleteObject)(b);
    pb_free(&m);
    free(bi);
}

TEST(brush_render_dibpattern_bottom_up_vs_top_down)
{
    NEED_R(); REQUIRE(U(CreateDIBPatternBrushPt));
    BITMAPINFO *bi = mk_pat24(8, 8);
    HBRUSH up = U(CreateDIBPatternBrushPt)(bi, DIB_RGB_COLORS);
    /* flip to top-down: same bits, negative height => rows reversed */
    bi->bmiHeader.biHeight = -8;
    HBRUSH down = U(CreateDIBPatternBrushPt)(bi, DIB_RGB_COLORS);
    PBM m;
    CHECK(pb_mem(&m, 8, 8, 32));
    CHECK(pb_fill(&m, up, 0, 0, 8, 8, PATCOPY));
    SURF_CHECK(&m, "bottom-up", pb_raw(&m, pat_col(x, y)));
    if (down) {
        pb_fill(&m, down, 0, 0, 8, 8, PATCOPY);
        pb_sync();
        int flipped = 1;
        for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++)
            if (pb_get(&m, x, y) != pb_raw(&m, pat_col(x, 7 - y))) flipped = 0;
        t_note("top-down DIB brush rows are %s", flipped ? "reversed relative to bottom-up (as expected)" : "not simply reversed");
        CHECK(flipped);
        U(DeleteObject)(down);
    } else t_note("top-down DIB brush creation failed");
    U(DeleteObject)(up);
    pb_free(&m);
    free(bi);
}

TEST(brush_render_dibpattern_1bpp_uses_own_palette)
{
    NEED_R(); REQUIRE(U(CreateDIBPatternBrushPt));
    BYTE buf[sizeof(BITMAPINFOHEADER) + 8 + 8 * 4];
    memset(buf, 0, sizeof buf);
    BITMAPINFO *bi = (BITMAPINFO *)buf;
    bi->bmiHeader.biSize = sizeof bi->bmiHeader; bi->bmiHeader.biWidth = 8; bi->bmiHeader.biHeight = 8;
    bi->bmiHeader.biPlanes = 1; bi->bmiHeader.biBitCount = 1; bi->bmiHeader.biCompression = BI_RGB;
    bi->bmiColors[0].rgbRed = 200;                       /* index 0 -> red */
    bi->bmiColors[1].rgbBlue = 180;                      /* index 1 -> blue */
    BYTE *bits = buf + sizeof(BITMAPINFOHEADER) + 8;
    for (int r = 0; r < 8; r++) bits[r * 4] = (BYTE)(0xA5 ^ (r * 0x11));   /* r = file row, bottom first */
    HBRUSH b = U(CreateDIBPatternBrushPt)(bi, DIB_RGB_COLORS);
    CHECK(b != NULL);
    PBM m;
    CHECK(pb_mem(&m, 16, 16, 32));
    U(SetTextColor)(m.dc, RGB(0, 255, 0));
    U(SetBkColor)(m.dc, RGB(255, 255, 0));
    CHECK(pb_fill(&m, b, 0, 0, 16, 16, PATCOPY));
    SURF_CHECK(&m, "1bpp dib brush", ({
        int row = 7 - (y & 7);
        int bit = (bits[row * 4] >> (7 - (x & 7))) & 1;
        bit ? pb_raw(&m, RGB(0, 0, 180)) : pb_raw(&m, RGB(200, 0, 0)); }));
    U(DeleteObject)(b);
    pb_free(&m);
}

TEST(brush_render_mono_ddb_pattern_text_bk_colors)
{
    NEED_R(); REQUIRE(U(CreatePatternBrush)); REQUIRE(U(CreateBitmap));
    static const BYTE rows[8] = { 0xAA, 0x55, 0xF0, 0x0F, 0x81, 0x7E, 0xFF, 0x00 };
    WORD bits[8];
    for (int i = 0; i < 8; i++) bits[i] = rows[i];             /* WORD aligned scanlines, byte 0 = low byte on LE */
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, bits);
    HBRUSH b = U(CreatePatternBrush)(bm);
    CHECK(b != NULL);
    PBM m;
    CHECK(pb_mem(&m, 24, 24, 32));
    static const COLORREF tc[] = { 0, RGB(255, 0, 0), RGB(10, 200, 30) };
    static const COLORREF bc[] = { 0xffffff, RGB(0, 0, 255), RGB(250, 250, 5) };
    for (int i = 0; i < 3; i++) {
        U(SetTextColor)(m.dc, tc[i]);
        U(SetBkColor)(m.dc, bc[i]);
        CHECK(pb_fill(&m, b, 0, 0, 24, 24, PATCOPY));
        char nm[32]; sprintf(nm, "mono pattern colours %d", i);
        SURF_CHECK(&m, nm, ((rows[y & 7] >> (7 - (x & 7))) & 1) ? pb_raw(&m, bc[i]) : pb_raw(&m, tc[i]));
    }
    U(DeleteObject)(b);
    U(DeleteObject)(bm);
    pb_free(&m);
}

TEST(brush_render_mono_ddb_pattern_origin)
{
    NEED_R(); REQUIRE(U(CreatePatternBrush)); REQUIRE(U(CreateBitmap));
    static const BYTE rows[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
    WORD bits[8];
    for (int i = 0; i < 8; i++) bits[i] = rows[i];
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, bits);
    HBRUSH b = U(CreatePatternBrush)(bm);
    PBM m;
    CHECK(pb_mem(&m, 32, 32, 32));
    static const int o[][2] = { {0,0}, {1,0}, {0,2}, {5,3}, {-3,-4}, {17,9} };
    for (unsigned i = 0; i < 6; i++) {
        U(SetBrushOrgEx)(m.dc, o[i][0], o[i][1], NULL);
        pb_fill(&m, b, 0, 0, 32, 32, PATCOPY);
        char nm[32]; sprintf(nm, "mono org %u", i);
        SURF_CHECK(&m, nm, ((rows[pb_mod8(y - o[i][1])] >> (7 - pb_mod8(x - o[i][0]))) & 1) ? 0xffffff : 0);
    }
    U(DeleteObject)(b); U(DeleteObject)(bm);
    pb_free(&m);
}

TEST(brush_render_mono_ddb_pattern_on_1bpp)
{
    NEED_R(); REQUIRE(U(CreatePatternBrush)); REQUIRE(U(CreateBitmap));
    static const BYTE rows[8] = { 0xCC, 0xCC, 0x33, 0x33, 0xCC, 0xCC, 0x33, 0x33 };
    WORD bits[8];
    for (int i = 0; i < 8; i++) bits[i] = rows[i];
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, bits);
    HBRUSH b = U(CreatePatternBrush)(bm);
    PBM m;
    CHECK(pb_mem(&m, 24, 16, 1));
    pb_fill(&m, b, 0, 0, 24, 16, PATCOPY);
    SURF_CHECK(&m, "1bpp dest", (rows[y & 7] >> (7 - (x & 7))) & 1);
    U(DeleteObject)(b); U(DeleteObject)(bm);
    pb_free(&m);
}

TEST(brush_render_large_pattern_tiles_at_bitmap_size)
{
    NEED_R(); REQUIRE(U(CreatePatternBrush));
    PBM src;
    CHECK(pb_mem(&src, 16, 16, 32));
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) pb_set(&src, x, y, (x * 16 + y) * 0x010101 & 0xffffff ? ((x * 13) << 16) | ((y * 13) << 8) | 0x40 : 0x40);
    HBRUSH b = U(CreatePatternBrush)(src.bmp);
    CHECK(b != NULL);
    PBM m;
    CHECK(pb_mem(&m, 48, 48, 32));
    pb_fill(&m, b, 0, 0, 48, 48, PATCOPY);
    pb_sync();
    int bad16 = 0, bad8 = 0;
    for (int y = 0; y < 48; y++) for (int x = 0; x < 48; x++) {
        if (pb_get(&m, x, y) != pb_get(&src, x & 15, y & 15)) bad16++;
        if (pb_get(&m, x, y) != pb_get(&src, x & 7, y & 7)) bad8++;
    }
    t_note("16x16 pattern: %d mismatches vs 16-periodic, %d vs 8-periodic", bad16, bad8);
    CHECK(bad16 == 0);
    U(DeleteObject)(b);
    pb_free(&m);
    pb_free(&src);
}

TEST(brush_render_small_pattern_tiles)
{
    NEED_R(); REQUIRE(U(CreatePatternBrush)); REQUIRE(U(CreateBitmap));
    /* 1x1 mono bitmap: all pixels take the text or bk colour */
    WORD one = 0x80;
    HBITMAP bm0 = U(CreateBitmap)(1, 1, 1, 1, &one);          /* bit set -> bk colour */
    HBITMAP bm1 = U(CreateBitmap)(1, 1, 1, 1, &(WORD){0});
    PBM m;
    CHECK(pb_mem(&m, 16, 16, 32));
    U(SetTextColor)(m.dc, RGB(1, 2, 3));
    U(SetBkColor)(m.dc, RGB(200, 100, 50));
    HBRUSH b0 = U(CreatePatternBrush)(bm0), b1 = U(CreatePatternBrush)(bm1);
    pb_fill(&m, b0, 0, 0, 16, 16, PATCOPY);
    SURF_CHECK(&m, "1x1 set", pb_raw(&m, RGB(200, 100, 50)));
    pb_fill(&m, b1, 0, 0, 16, 16, PATCOPY);
    SURF_CHECK(&m, "1x1 clear", pb_raw(&m, RGB(1, 2, 3)));
    U(DeleteObject)(b0); U(DeleteObject)(b1); U(DeleteObject)(bm0); U(DeleteObject)(bm1);
    pb_free(&m);
}

TEST(brush_render_pattern_bitmap_deleted_after_create)
{
    NEED_R(); REQUIRE(U(CreatePatternBrush));
    PBM src;
    CHECK(pb_mem(&src, 8, 8, 32));
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) pb_set(&src, x, y, (x << 20) | (y << 12) | 0x77);
    HBRUSH b = U(CreatePatternBrush)(src.bmp);
    pb_free(&src);                                     /* bitmap gone */
    PBM m;
    CHECK(pb_mem(&m, 8, 8, 32));
    CHECK(pb_fill(&m, b, 0, 0, 8, 8, PATCOPY));
    SURF_CHECK(&m, "deleted source", (x << 20) | (y << 12) | 0x77);
    U(DeleteObject)(b);
    pb_free(&m);
}

TEST(brush_render_pattern_bitmap_modified_after_create)
{
    NEED_R(); REQUIRE(U(CreatePatternBrush));
    PBM src;
    CHECK(pb_mem(&src, 8, 8, 32));
    pb_clear(&src, 0x112233);
    HBRUSH b = U(CreatePatternBrush)(src.bmp);
    pb_clear(&src, 0x445566);
    PBM m;
    CHECK(pb_mem(&m, 8, 8, 32));
    pb_fill(&m, b, 0, 0, 8, 8, PATCOPY);
    pb_sync();
    unsigned got = pb_get(&m, 3, 3);
    t_note("pattern brush after source DIBSection modified: %06x (old 112233, new 445566)", got);
    CHECK(got == 0x112233 || got == 0x445566);
    U(DeleteObject)(b);
    pb_free(&m);
    pb_free(&src);
}

TEST(brush_render_viewport_origin_does_not_move_brush)
{
    NEED_R(); REQUIRE(U(CreateDIBPatternBrushPt)); REQUIRE(U(SetViewportOrgEx)); REQUIRE(U(SetWindowOrgEx));
    BITMAPINFO *bi = mk_pat24(8, 8);
    HBRUSH b = U(CreateDIBPatternBrushPt)(bi, DIB_RGB_COLORS);
    PBM m;
    CHECK(pb_mem(&m, 40, 40, 32));
    U(SetViewportOrgEx)(m.dc, 3, 5, NULL);
    pb_clear(&m, 0xffffff);
    pb_fill(&m, b, 0, 0, 20, 20, PATCOPY);
    SURF_CHECK(&m, "viewport", (x >= 3 && x < 23 && y >= 5 && y < 25) ? pb_raw(&m, pat_col(x & 7, y & 7)) : 0xffffff);
    U(SetViewportOrgEx)(m.dc, 0, 0, NULL);
    U(SetWindowOrgEx)(m.dc, 2, 6, NULL);
    pb_clear(&m, 0xffffff);
    pb_fill(&m, b, 2, 6, 20, 20, PATCOPY);
    SURF_CHECK(&m, "window org", (x < 20 && y < 20) ? pb_raw(&m, pat_col(x & 7, y & 7)) : 0xffffff);
    U(SetWindowOrgEx)(m.dc, 0, 0, NULL);
    U(DeleteObject)(b);
    pb_free(&m);
    free(bi);
}

TEST(brush_render_dcbrush_follows_color)
{
    NEED_R(); REQUIRE(U(SetDCBrushColor));
    PBM m;
    CHECK(pb_mem(&m, 8, 8, 32));
    HGDIOBJ dcb = GetStockObject(DC_BRUSH);
    for (int i = 0; i < 5; i++) {
        COLORREF c = RGB(i * 60, 255 - i * 50, i * 9);
        U(SetDCBrushColor)(m.dc, c);
        pb_fill(&m, dcb, 0, 0, 8, 8, PATCOPY);
        SURF_CHECK(&m, "DC_BRUSH", pb_raw(&m, c));
    }
    /* colour captured at draw time, selected brush stays selected */
    HGDIOBJ old = U(SelectObject)(m.dc, dcb);
    U(SetDCBrushColor)(m.dc, 0x00ff00);
    U(PatBlt)(m.dc, 0, 0, 4, 8, PATCOPY);
    U(SetDCBrushColor)(m.dc, 0x0000ff);
    U(PatBlt)(m.dc, 4, 0, 4, 8, PATCOPY);
    U(SelectObject)(m.dc, old);
    SURF_CHECK(&m, "DC_BRUSH two draws", x < 4 ? pb_raw(&m, 0x00ff00) : pb_raw(&m, 0x0000ff));
    pb_free(&m);
}

TEST(brush_render_dcbrush_default_white)
{
    NEED_R();
    PBM m;
    CHECK(pb_mem(&m, 8, 8, 32));
    pb_clear(&m, 0);
    pb_fill(&m, GetStockObject(DC_BRUSH), 0, 0, 8, 8, PATCOPY);
    SURF_CHECK(&m, "default DC brush colour", 0xffffff);
    pb_free(&m);
}

TEST(brush_render_stock_brushes)
{
    NEED_R();
    static const struct { int id; COLORREF c; } s[] = { {WHITE_BRUSH, 0xffffff}, {LTGRAY_BRUSH, 0xc0c0c0}, {GRAY_BRUSH, 0x808080},
        {DKGRAY_BRUSH, 0x404040}, {BLACK_BRUSH, 0} };
    PBM m;
    CHECK(pb_mem(&m, 8, 8, 32));
    for (int i = 0; i < 5; i++) {
        pb_clear(&m, 0x123456);
        pb_fill(&m, GetStockObject(s[i].id), 0, 0, 8, 8, PATCOPY);
        SURF_CHECK(&m, "stock brush", pb_raw(&m, s[i].c));
    }
    pb_free(&m);
}

TEST(brush_render_null_brush_noop)
{
    NEED_R();
    PBM m;
    CHECK(pb_mem(&m, 8, 8, 32));
    pb_clear(&m, 0x123456);
    BOOL r = pb_fill(&m, GetStockObject(NULL_BRUSH), 0, 0, 8, 8, PATCOPY);
    t_note("PatBlt(PATCOPY) with NULL_BRUSH returned %d, changed %d pixels", r, pb_count_ne(&m, 0x123456));
    pb_clear(&m, 0x123456);
    r = pb_fill(&m, GetStockObject(NULL_BRUSH), 0, 0, 8, 8, PATINVERT);
    t_note("PatBlt(PATINVERT) with NULL_BRUSH returned %d, changed %d pixels", r, pb_count_ne(&m, 0x123456));
    pb_clear(&m, 0x123456);
    r = pb_fill(&m, GetStockObject(NULL_BRUSH), 0, 0, 8, 8, DSTINVERT);
    CHECK(r);
    SURF_CHECK(&m, "DSTINVERT ignores brush", (~0x123456u) & 0xffffff);
    pb_free(&m);
}

TEST(brush_render_repeatable_and_shared_between_dcs)
{
    NEED_R();
    PBM a, b;
    CHECK(pb_mem(&a, 16, 16, 32));
    CHECK(pb_mem(&b, 16, 16, 32));
    HBRUSH br = U(CreateHatchBrush)(HS_DIAGCROSS, RGB(0, 0, 0));
    U(SetBkMode)(a.dc, TRANSPARENT);
    U(SetBkMode)(b.dc, OPAQUE);
    U(SetBkColor)(b.dc, RGB(255, 255, 0));
    HGDIOBJ oa = U(SelectObject)(a.dc, br), ob = U(SelectObject)(b.dc, br);
    for (int i = 0; i < 3; i++) {
        pb_clear(&a, 0x00ff00);
        pb_clear(&b, 0);
        U(PatBlt)(a.dc, 0, 0, 16, 16, PATCOPY);
        U(PatBlt)(b.dc, 0, 0, 16, 16, PATCOPY);
        SURF_CHECK(&a, "dc a", pb_hatch_bit(HS_DIAGCROSS, x, y) ? 0 : 0x00ff00);
        SURF_CHECK(&b, "dc b", pb_hatch_bit(HS_DIAGCROSS, x, y) ? 0 : pb_raw(&b, RGB(255, 255, 0)));
    }
    U(SelectObject)(a.dc, oa); U(SelectObject)(b.dc, ob);
    U(DeleteObject)(br);
    pb_free(&a); pb_free(&b);
}

TEST(brush_render_fillrect_bounds_and_brush)
{
    NEED_R(); REQUIRE(U(FillRect));
    PBM m;
    CHECK(pb_mem(&m, 32, 32, 32));
    HBRUSH b = U(CreateSolidBrush)(0x2468ac);
    RECT rc = { 4, 6, 20, 25 };
    pb_clear(&m, 0xffffff);
    CHECK(U(FillRect)(m.dc, &rc, b) != 0);
    SURF_CHECK(&m, "FillRect", (x >= 4 && x < 20 && y >= 6 && y < 25) ? pb_raw(&m, 0x2468ac) : 0xffffff);
    RECT e = { 10, 10, 10, 20 };
    pb_clear(&m, 0xffffff);
    U(FillRect)(m.dc, &e, b);
    CHECK(pb_count_ne(&m, 0xffffff) == 0);
    RECT inv = { 20, 20, 5, 5 };
    pb_clear(&m, 0xffffff);
    int r = U(FillRect)(m.dc, &inv, b);
    t_note("FillRect inverted rect -> %d, %d pixels", r, pb_count_ne(&m, 0xffffff));
    U(DeleteObject)(b);
    pb_free(&m);
}

TEST(brush_render_fillrect_does_not_leave_brush_selected)
{
    NEED_R(); REQUIRE(U(FillRect)); REQUIRE(U(GetCurrentObject));
    PBM m;
    CHECK(pb_mem(&m, 8, 8, 32));
    HGDIOBJ before = U(GetCurrentObject)(m.dc, OBJ_BRUSH);
    HBRUSH b = U(CreateSolidBrush)(0x808000);
    RECT rc = { 0, 0, 8, 8 };
    U(FillRect)(m.dc, &rc, b);
    CHECK(U(GetCurrentObject)(m.dc, OBJ_BRUSH) == before);
    CHECK(U(DeleteObject)(b));
    pb_free(&m);
}

TEST(brush_render_fillrect_system_color_brush)
{
    NEED_R(); REQUIRE(U(FillRect)); REQUIRE(U(GetSysColor));
    PBM m;
    CHECK(pb_mem(&m, 8, 8, 32));
    static const int idx[] = { COLOR_WINDOW, COLOR_BTNFACE, COLOR_WINDOWTEXT, COLOR_HIGHLIGHT, COLOR_DESKTOP };
    RECT rc = { 0, 0, 8, 8 };
    for (int i = 0; i < 5; i++) {
        pb_clear(&m, 0x010101);
        int r = U(FillRect)(m.dc, &rc, (HBRUSH)(ULONG_PTR)(idx[i] + 1));
        CHECKF(r != 0, "index %d", idx[i]);
        SURF_CHECK(&m, "sys colour", pb_raw(&m, U(GetSysColor)(idx[i])));
    }
    pb_free(&m);
}

TEST(brush_render_palette_dibpattern)
{
    NEED_R(); REQUIRE(U(CreateDIBPatternBrushPt)); REQUIRE(U(CreatePalette)); REQUIRE(U(SelectPalette));
    BYTE buf[sizeof(BITMAPINFOHEADER) + 256 * 2 + 16 * 16];
    memset(buf, 0, sizeof buf);
    BITMAPINFO *bi = (BITMAPINFO *)buf;
    WORD *idx = (WORD *)bi->bmiColors;
    bi->bmiHeader.biSize = sizeof bi->bmiHeader; bi->bmiHeader.biWidth = 16; bi->bmiHeader.biHeight = 16;
    bi->bmiHeader.biPlanes = 1; bi->bmiHeader.biBitCount = 8; bi->bmiHeader.biCompression = BI_RGB;
    for (int i = 0; i < 256; i++) idx[i] = 255 - i;
    BYTE *bits = (BYTE *)(idx + 256);
    for (int i = 0; i < 256; i++) bits[i] = i;
    HBRUSH b = U(CreateDIBPatternBrushPt)(bi, DIB_PAL_COLORS);
    CHECK(b != NULL);
    char pb[sizeof(LOGPALETTE) + 256 * sizeof(PALETTEENTRY)];
    LOGPALETTE *lp = (LOGPALETTE *)pb;
    lp->palVersion = 0x300; lp->palNumEntries = 256;
    for (int i = 0; i < 256; i++) { lp->palPalEntry[i].peRed = i * 2; lp->palPalEntry[i].peGreen = 255 - i; lp->palPalEntry[i].peBlue = i; lp->palPalEntry[i].peFlags = 0; }
    HPALETTE pal = U(CreatePalette)(lp);
    PBM m;
    CHECK(pb_mem(&m, 16, 16, 32));
    HGDIOBJ oldp = U(SelectPalette)(m.dc, pal, FALSE);
    CHECK(oldp != NULL);
    pb_fill(&m, b, 0, 0, 16, 16, PATCOPY);
    pb_sync();
    int bad = 0;
    for (int i = 0; i < 256; i++) {
        int px = i % 16, py = 15 - i / 16;          /* bottom-up rows */
        int e = 255 - bits[i];
        unsigned want = ((e * 2 & 255) << 16) | ((255 - e) << 8) | e;
        /* the brush index of pixel i is idx[bits[i]] */
        if (pb_get(&m, px, py) != want) bad++;
    }
    t_note("palette DIB brush: %d of 256 pixels differ from the index model", bad);
    CHECK(bad == 0);
    U(SelectPalette)(m.dc, oldp, FALSE);
    U(DeleteObject)(pal);
    U(DeleteObject)(b);
    pb_free(&m);
}
