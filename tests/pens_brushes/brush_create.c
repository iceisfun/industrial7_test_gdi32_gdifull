#include "pb.h"

#define NEED_BR() do { REQUIRE(U(CreateSolidBrush)); REQUIRE(U(CreateHatchBrush)); REQUIRE(U(CreateBrushIndirect)); \
    REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectW)); REQUIRE(U(GetObjectType)); } while (0)

static int get_lb(HGDIOBJ h, LOGBRUSH *lb)
{
    memset(lb, 0xb0, sizeof *lb);
    return U(GetObjectW)(h, sizeof *lb, lb);
}

/* heap-built packed DIB: header + colour table + zeroed bits. */
static BITMAPINFO *mkdib(int w, int h, int bpp, size_t *total)
{
    int ncol = bpp <= 8 ? 1 << bpp : 0;
    size_t stride = (((size_t)w * bpp + 31) / 32) * 4;
    size_t sz = sizeof(BITMAPINFOHEADER) + ncol * 4 + stride * (h < 0 ? -h : h);
    BITMAPINFO *bi = calloc(1, sz);
    bi->bmiHeader.biSize = sizeof bi->bmiHeader;
    bi->bmiHeader.biWidth = w; bi->bmiHeader.biHeight = h;
    bi->bmiHeader.biPlanes = 1; bi->bmiHeader.biBitCount = bpp;
    bi->bmiHeader.biCompression = BI_RGB;
    for (int i = 0; i < ncol; i++) {
        int v = ncol > 1 ? i * 255 / (ncol - 1) : 0;
        bi->bmiColors[i].rgbRed = bi->bmiColors[i].rgbGreen = bi->bmiColors[i].rgbBlue = v;
    }
    if (total) *total = sz;
    return bi;
}

TEST(brushes_solid_roundtrip)
{
    NEED_BR();
    static const COLORREF c[] = { 0, 1, 0xff, 0xff00, 0xff0000, 0xffffff, 0x123456, 0xabcdef, 0x808080, 0xc0c0c0, 0x010203 };
    for (unsigned i = 0; i < sizeof c / sizeof *c; i++) {
        HBRUSH b = U(CreateSolidBrush)(c[i]);
        CHECK(b != NULL);
        LOGBRUSH lb;
        CHECK(get_lb(b, &lb) == sizeof lb);
        CHECKF(lb.lbStyle == BS_SOLID, "c=%08lx style %u", c[i], lb.lbStyle);
        CHECKF(lb.lbColor == c[i], "c=%08lx got %08lx", c[i], lb.lbColor);
        CHECKF(lb.lbHatch == 0, "hatch %Ix", lb.lbHatch);
        CHECK(U(GetObjectType)(b) == OBJ_BRUSH);
        CHECK(U(DeleteObject)(b));
    }
}

TEST(brushes_solid_every_channel_value)
{
    NEED_BR();
    for (int v = 0; v < 256; v++) {
        COLORREF c = RGB(255 - v, v, (v * 3) & 255);
        HBRUSH b = U(CreateSolidBrush)(c);
        LOGBRUSH lb;
        CHECK(b && get_lb(b, &lb) == sizeof lb);
        CHECKF(lb.lbColor == c && lb.lbStyle == BS_SOLID, "v=%d", v);
        U(DeleteObject)(b);
    }
}

TEST(brushes_solid_color_high_byte)
{
    NEED_BR();
    static const COLORREF c[] = { 0x01123456, 0x02000003, 0x03123456, 0x80000000, 0xff123456, 0xffffffff };
    for (unsigned i = 0; i < sizeof c / sizeof *c; i++) {
        HBRUSH b = U(CreateSolidBrush)(c[i]);
        CHECKF(b != NULL, "c=%08lx", c[i]);
        LOGBRUSH lb;
        if (b && get_lb(b, &lb) == sizeof lb && lb.lbColor != c[i]) t_note("solid %08lx reads back %08lx", c[i], lb.lbColor);
        if (b) U(DeleteObject)(b);
    }
}

TEST(brushes_solid_not_stock)
{
    NEED_BR();
    static const struct { int id; COLORREF c; } s[] = { {WHITE_BRUSH, 0xffffff}, {LTGRAY_BRUSH, 0xc0c0c0},
        {GRAY_BRUSH, 0x808080}, {BLACK_BRUSH, 0} };
    for (int i = 0; i < 4; i++) {
        HBRUSH b = U(CreateSolidBrush)(s[i].c);
        CHECK(b != NULL && b != GetStockObject(s[i].id));
        U(DeleteObject)(b);
    }
}

TEST(brushes_solid_handle_lifecycle)
{
    NEED_BR();
    HBRUSH a = U(CreateSolidBrush)(0x112233), b = U(CreateSolidBrush)(0x112233);
    CHECK(a && b && a != b);
    CHECK(U(DeleteObject)(a));
    LOGBRUSH lb;
    SetLastError(0xdeadbeef);
    CHECK(get_lb(a, &lb) == 0);
    CHECK(U(GetObjectType)(a) == 0);
    CHECK(!U(DeleteObject)(a));
    CHECK(get_lb(b, &lb) == sizeof lb && lb.lbColor == 0x112233);
    CHECK(U(DeleteObject)(b));
}

TEST(brushes_solid_cache_cycles)
{
    NEED_BR();
    for (int i = 0; i < 100; i++) {
        COLORREF c = RGB(i * 2, 255 - i, i * 5);
        HBRUSH b = U(CreateSolidBrush)(c);
        LOGBRUSH lb;
        CHECK(b && get_lb(b, &lb) == sizeof lb);
        CHECKF(lb.lbColor == c && lb.lbStyle == BS_SOLID, "i=%d", i);
        U(DeleteObject)(b);
    }
    HBRUSH bs[40];
    for (int i = 0; i < 40; i++) bs[i] = U(CreateSolidBrush)(RGB(i, 0, 0));
    for (int i = 0; i < 40; i++) {
        LOGBRUSH lb;
        for (int j = 0; j < i; j++) CHECK(bs[i] != bs[j]);
        CHECK(get_lb(bs[i], &lb) == sizeof lb && lb.lbColor == RGB(i, 0, 0));
    }
    for (int i = 0; i < 40; i++) U(DeleteObject)(bs[i]);
}

TEST(brushes_stock_logbrush)
{
    NEED_BR();
    static const struct { int id; UINT st; COLORREF c; } s[] = {
        {WHITE_BRUSH, BS_SOLID, 0xffffff}, {LTGRAY_BRUSH, BS_SOLID, 0xc0c0c0}, {GRAY_BRUSH, BS_SOLID, 0x808080},
        {DKGRAY_BRUSH, BS_SOLID, 0x404040}, {BLACK_BRUSH, BS_SOLID, 0}, {NULL_BRUSH, BS_NULL, 0} };
    for (int i = 0; i < 6; i++) {
        HGDIOBJ h = GetStockObject(s[i].id);
        CHECK(h != NULL);
        LOGBRUSH lb;
        CHECK(get_lb(h, &lb) == sizeof lb);
        CHECKF(lb.lbStyle == s[i].st, "id %d style %u", s[i].id, lb.lbStyle);
        CHECKF(lb.lbColor == s[i].c, "id %d color %08lx", s[i].id, lb.lbColor);
        CHECK(U(GetObjectType)(h) == OBJ_BRUSH);
        CHECK(GetStockObject(s[i].id) == h);
    }
    CHECK(GetStockObject(HOLLOW_BRUSH) == GetStockObject(NULL_BRUSH));
    HGDIOBJ dcb = GetStockObject(DC_BRUSH);
    LOGBRUSH lb;
    CHECK(dcb != NULL && get_lb(dcb, &lb) == sizeof lb);
    CHECK(lb.lbStyle == BS_SOLID);
}

TEST(brushes_stock_delete_harmless)
{
    NEED_BR();
    for (int id = WHITE_BRUSH; id <= NULL_BRUSH; id++) {
        HGDIOBJ h = GetStockObject(id);
        BOOL r = U(DeleteObject)(h);
        t_note("DeleteObject(stock brush %d) = %d", id, r);
        LOGBRUSH lb;
        CHECK(get_lb(GetStockObject(id), &lb) == sizeof lb);
        CHECK(GetStockObject(id) == h);
    }
}

TEST(brushes_hatch_all_indices)
{
    NEED_BR();
    for (int i = -3; i < 20; i++) {
        SetLastError(0xdeadbeef);
        HBRUSH b = U(CreateHatchBrush)(i, RGB(12, 34, 56));
        if (i >= 0 && i < HS_API_MAX) {
            CHECKF(b != NULL, "hatch %d", i);
            if (!b) continue;
            LOGBRUSH lb;
            CHECK(get_lb(b, &lb) == sizeof lb);
            CHECKF(lb.lbColor == RGB(12, 34, 56), "hatch %d color %08lx", i, lb.lbColor);
            if (i <= HS_DIAGCROSS) {
                CHECKF(lb.lbStyle == BS_HATCHED, "hatch %d style %u", i, lb.lbStyle);
                CHECKF(lb.lbHatch == (ULONG_PTR)i, "hatch %d got %Iu", i, lb.lbHatch);
            } else {
                CHECKF(lb.lbStyle == BS_SOLID, "hatch %d style %u", i, lb.lbStyle);
                CHECKF(lb.lbHatch == 0, "hatch %d got %Iu", i, lb.lbHatch);
            }
            CHECK(U(GetObjectType)(b) == OBJ_BRUSH);
            CHECK(U(DeleteObject)(b));
        } else {
            CHECKF(b == NULL, "hatch %d", i);
            if (b) U(DeleteObject)(b);
        }
    }
}

TEST(brushes_hatch_invalid_extreme)
{
    NEED_BR();
    static const int h[] = { HS_API_MAX, HS_API_MAX + 1, 100, 255, 256, 0x10000, INT_MAX, INT_MIN, -1 };
    for (unsigned i = 0; i < sizeof h / sizeof *h; i++) {
        SetLastError(0xdeadbeef);
        HBRUSH b = U(CreateHatchBrush)(h[i], 0);
        CHECKF(b == NULL, "hatch %d", h[i]);
        if (b) U(DeleteObject)(b);
        else t_note("CreateHatchBrush(%d) last error %lx", h[i], GetLastError());
    }
}

TEST(brushes_hatch_color_variety)
{
    NEED_BR();
    for (int v = 0; v < 256; v += 17)
    for (int h = HS_HORIZONTAL; h <= HS_DIAGCROSS; h++) {
        COLORREF c = RGB(v, 255 - v, v / 2);
        HBRUSH b = U(CreateHatchBrush)(h, c);
        LOGBRUSH lb;
        CHECK(b && get_lb(b, &lb) == sizeof lb);
        CHECK(lb.lbColor == c && lb.lbHatch == (ULONG_PTR)h && lb.lbStyle == BS_HATCHED);
        U(DeleteObject)(b);
    }
}

TEST(brushes_hatch_handle_lifecycle)
{
    NEED_BR();
    HBRUSH a = U(CreateHatchBrush)(HS_CROSS, 1);
    LOGBRUSH lb;
    CHECK(a != NULL && get_lb(a, &lb) == sizeof lb);
    CHECK(U(DeleteObject)(a));
    CHECK(get_lb(a, &lb) == 0);
    CHECK(!U(DeleteObject)(a));
}

TEST(brushes_indirect_style_matrix)
{
    NEED_BR(); REQUIRE(U(CreateBitmap));
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, NULL);
    BITMAPINFO *bi = mkdib(8, 8, 24, NULL);
    for (UINT st = 0; st <= 12; st++) {
        LOGBRUSH in = { st, RGB(1, 2, 3), 0 };
        switch (st) {
        case BS_HATCHED: in.lbHatch = HS_DIAGCROSS; break;
        case BS_PATTERN: case BS_PATTERN8X8: in.lbHatch = (ULONG_PTR)bm; break;
        case BS_DIBPATTERNPT: case BS_DIBPATTERN8X8: in.lbColor = DIB_RGB_COLORS; in.lbHatch = (ULONG_PTR)bi; break;
        case BS_DIBPATTERN: continue;
        }
        SetLastError(0xdeadbeef);
        HBRUSH b = U(CreateBrushIndirect)(&in);
        int valid = st <= BS_PATTERN || st == BS_DIBPATTERNPT || st == BS_PATTERN8X8 || st == BS_DIBPATTERN8X8;
        if (st == BS_DIBPATTERN8X8) { if (b) U(DeleteObject)(b); t_note("BS_DIBPATTERN8X8 -> %p", (void *)b); continue; }
        if (st == BS_INDEXED || st == BS_MONOPATTERN || st > BS_MONOPATTERN) valid = 0;
        CHECKF((b != NULL) == valid, "style %u -> %p", st, (void *)b);
        if (!b) {
            CHECKF(GetLastError() == 0xdeadbeef || !valid, "style %u gle %lx", st, GetLastError());
            continue;
        }
        LOGBRUSH out;
        CHECK(get_lb(b, &out) == sizeof out);
        switch (st) {
        case BS_SOLID: CHECK(out.lbStyle == BS_SOLID && out.lbColor == in.lbColor); break;
        case BS_NULL: CHECK(b == GetStockObject(NULL_BRUSH)); CHECK(out.lbStyle == BS_NULL); break;
        case BS_HATCHED: CHECK(out.lbStyle == BS_HATCHED && out.lbHatch == HS_DIAGCROSS && out.lbColor == in.lbColor); break;
        case BS_PATTERN: case BS_PATTERN8X8:
            CHECK(out.lbStyle == BS_PATTERN); CHECK(out.lbColor == 0); CHECK(out.lbHatch == (ULONG_PTR)bm); break;
        case BS_DIBPATTERNPT: case BS_DIBPATTERN8X8:
            t_note("style %u reads back style %u color %08lx", st, out.lbStyle, out.lbColor); break;
        }
        if (b != GetStockObject(NULL_BRUSH)) U(DeleteObject)(b);
    }
    U(DeleteObject)(bm);
    free(bi);
}

TEST(brushes_indirect_invalid_styles_large)
{
    NEED_BR();
    static const UINT st[] = { 10, 11, 16, 100, 0x100, 0xffff, 0x10000, 0x7fffffff, 0x80000000u, 0xffffffffu };
    for (unsigned i = 0; i < sizeof st / sizeof *st; i++) {
        LOGBRUSH in = { st[i], 0, 0 };
        SetLastError(0xdeadbeef);
        HBRUSH b = U(CreateBrushIndirect)(&in);
        CHECKF(b == NULL, "style %x", st[i]);
        if (b) U(DeleteObject)(b);
        else CHECKF(GetLastError() == 0xdeadbeef, "style %x gle %lx", st[i], GetLastError());
    }
}

TEST(brushes_indirect_indexed_and_mono_fail)
{
    NEED_BR(); REQUIRE(U(CreateBitmap));
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, NULL);
    UINT st[] = { BS_INDEXED, BS_MONOPATTERN };
    for (int i = 0; i < 2; i++) {
        LOGBRUSH in = { st[i], 0, (ULONG_PTR)bm };
        HBRUSH b = U(CreateBrushIndirect)(&in);
        CHECKF(b == NULL, "style %u", st[i]);
        if (b) U(DeleteObject)(b);
    }
    U(DeleteObject)(bm);
}

TEST(brushes_indirect_null_hollow_stock)
{
    NEED_BR();
    LOGBRUSH lb = { BS_NULL, 0, 0 };
    HBRUSH b = U(CreateBrushIndirect)(&lb);
    CHECK(b == GetStockObject(NULL_BRUSH));
    lb.lbStyle = BS_HOLLOW; lb.lbColor = 0x123456; lb.lbHatch = 77;
    CHECK(U(CreateBrushIndirect)(&lb) == GetStockObject(HOLLOW_BRUSH));
    CHECK(BS_NULL == BS_HOLLOW);
    LOGBRUSH out;
    CHECK(get_lb(b, &out) == sizeof out && out.lbStyle == BS_HOLLOW);
}

TEST(brushes_indirect_solid_ignores_hatch)
{
    NEED_BR();
    static const ULONG_PTR h[] = { 0, 1, 99, 0xdeadbeef, ~(ULONG_PTR)0 };
    for (unsigned i = 0; i < 5; i++) {
        LOGBRUSH in = { BS_SOLID, 0x00cc8844, h[i] }, out;
        HBRUSH b = U(CreateBrushIndirect)(&in);
        CHECKF(b != NULL, "hatch %Ix", h[i]);
        if (!b) continue;
        CHECK(get_lb(b, &out) == sizeof out);
        CHECK(out.lbStyle == BS_SOLID && out.lbColor == 0x00cc8844);
        CHECK(out.lbHatch == 0);
        U(DeleteObject)(b);
    }
}

TEST(brushes_indirect_hatched_values)
{
    NEED_BR();
    for (int h = -2; h < 16; h++) {
        LOGBRUSH in = { BS_HATCHED, 0x445566, (ULONG_PTR)(LONG_PTR)h }, out;
        HBRUSH b = U(CreateBrushIndirect)(&in);
        int valid = h >= 0 && h < HS_API_MAX;
        CHECKF((b != NULL) == valid, "hatch %d -> %p", h, (void *)b);
        if (!b) continue;
        CHECK(get_lb(b, &out) == sizeof out);
        if (h <= HS_DIAGCROSS) CHECK(out.lbStyle == BS_HATCHED && out.lbHatch == (ULONG_PTR)h);
        else CHECK(out.lbStyle == BS_SOLID);
        U(DeleteObject)(b);
    }
}

TEST(brushes_indirect_hatch_huge_value)
{
    NEED_BR();
    LOGBRUSH in = { BS_HATCHED, 0, (ULONG_PTR)1 << 32 };
    HBRUSH b = U(CreateBrushIndirect)(&in);
    t_note("BS_HATCHED with lbHatch 1<<32 -> %p", (void *)b);
    if (b) U(DeleteObject)(b);
    in.lbHatch = (ULONG_PTR)0x100000002ull;
    b = U(CreateBrushIndirect)(&in);
    t_note("BS_HATCHED with lbHatch 0x100000002 -> %p", (void *)b);
    if (b) U(DeleteObject)(b);
}

TEST(brushes_pattern_sizes)
{
    NEED_BR(); REQUIRE(U(CreatePatternBrush)); REQUIRE(U(CreateBitmap));
    static const int sz[][2] = { {1,1}, {1,2}, {2,1}, {2,2}, {3,5}, {7,7}, {8,8}, {9,9}, {8,16}, {16,8}, {16,16}, {20,20},
        {32,32}, {33,17}, {64,1}, {1,64}, {100,3}, {256,256}, {1000,2} };
    for (unsigned i = 0; i < sizeof sz / sizeof *sz; i++) {
        HBITMAP bm = U(CreateBitmap)(sz[i][0], sz[i][1], 1, 1, NULL);
        CHECK(bm != NULL);
        HBRUSH b = U(CreatePatternBrush)(bm);
        CHECKF(b != NULL, "%dx%d", sz[i][0], sz[i][1]);
        if (b) {
            LOGBRUSH lb;
            CHECK(get_lb(b, &lb) == sizeof lb);
            CHECK(lb.lbStyle == BS_PATTERN && lb.lbColor == 0 && lb.lbHatch == (ULONG_PTR)bm);
            CHECK(U(GetObjectType)(b) == OBJ_BRUSH);
            CHECK(U(DeleteObject)(b));
        }
        U(DeleteObject)(bm);
    }
}

TEST(brushes_pattern_color_depths)
{
    NEED_BR(); REQUIRE(U(CreatePatternBrush)); REQUIRE(U(CreateBitmap));
    static const int bpp[] = { 1, 4, 8, 16, 24, 32 };
    for (int i = 0; i < 6; i++) {
        HBITMAP bm = U(CreateBitmap)(8, 8, 1, bpp[i], NULL);
        if (!bm) { t_note("CreateBitmap 8x8x%d failed", bpp[i]); continue; }
        HBRUSH b = U(CreatePatternBrush)(bm);
        t_note("pattern brush from %dbpp bitmap -> %s", bpp[i], b ? "ok" : "failed");
        if (b) {
            LOGBRUSH lb;
            CHECK(get_lb(b, &lb) == sizeof lb && lb.lbStyle == BS_PATTERN && lb.lbHatch == (ULONG_PTR)bm);
            U(DeleteObject)(b);
        }
        U(DeleteObject)(bm);
    }
}

TEST(brushes_pattern_dibsection)
{
    NEED_MEM(); NEED_BR(); REQUIRE(U(CreatePatternBrush));
    static const int bpp[] = { 1, 16, 24, 32 };
    for (int i = 0; i < 4; i++) {
        PBM m;
        CHECK(pb_mem(&m, 16, 16, bpp[i]));
        /* the DIB section is selected into a DC while the brush is created */
        HBRUSH b = U(CreatePatternBrush)(m.bmp);
        CHECKF(b != NULL, "%dbpp", bpp[i]);
        if (b) {
            LOGBRUSH lb;
            CHECK(get_lb(b, &lb) == sizeof lb);
            CHECK(lb.lbStyle == BS_PATTERN && lb.lbHatch == (ULONG_PTR)m.bmp);
            U(DeleteObject)(b);
        }
        pb_free(&m);
    }
}

TEST(brushes_pattern_null_and_bogus)
{
    NEED_BR(); NEED_MEM(); REQUIRE(U(CreatePatternBrush)); REQUIRE(U(CreateBitmap));
    CHECK(U(CreatePatternBrush)(NULL) == NULL);
    HANDLE bad[] = { (HANDLE)1, (HANDLE)0xdeadbeef, (HANDLE)(LONG_PTR)-1, (HANDLE)0x7fff0000 };
    for (unsigned i = 0; i < 4; i++) {
        HBRUSH b = U(CreatePatternBrush)(bad[i]);
        CHECKF(b == NULL, "h=%p", bad[i]);
        if (b) U(DeleteObject)(b);
    }
    HPEN pen = U(CreatePen)(PS_SOLID, 1, 0);
    HBRUSH b = U(CreatePatternBrush)((HBITMAP)pen);
    CHECK(b == NULL);
    if (b) U(DeleteObject)(b);
    U(DeleteObject)(pen);
    HBRUSH sb = U(CreateSolidBrush)(5);
    b = U(CreatePatternBrush)((HBITMAP)sb);
    CHECK(b == NULL);
    if (b) U(DeleteObject)(b);
    U(DeleteObject)(sb);
    HDC dc = U(CreateCompatibleDC)(NULL);
    b = U(CreatePatternBrush)((HBITMAP)dc);
    CHECK(b == NULL);
    if (b) U(DeleteObject)(b);
    U(DeleteDC)(dc);
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, NULL);
    U(DeleteObject)(bm);
    b = U(CreatePatternBrush)(bm);
    CHECK(b == NULL);
    if (b) U(DeleteObject)(b);
}

TEST(brushes_pattern_bitmap_deleted_after_create)
{
    NEED_BR(); REQUIRE(U(CreatePatternBrush)); REQUIRE(U(CreateBitmap));
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, NULL);
    HBRUSH b = U(CreatePatternBrush)(bm);
    CHECK(U(DeleteObject)(bm));
    BITMAP bmp;
    CHECK(U(GetObjectW)(bm, sizeof bmp, &bmp) == 0);
    LOGBRUSH lb;
    CHECK(get_lb(b, &lb) == sizeof lb);
    CHECK(lb.lbStyle == BS_PATTERN && lb.lbColor == 0 && lb.lbHatch == (ULONG_PTR)bm);
    CHECK(U(DeleteObject)(b));
}

TEST(brushes_pattern_bitmap_survives_brush_delete)
{
    NEED_BR(); REQUIRE(U(CreatePatternBrush)); REQUIRE(U(CreateBitmap));
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, NULL);
    HBRUSH b = U(CreatePatternBrush)(bm);
    CHECK(U(DeleteObject)(b));
    BITMAP bmp;
    CHECK(U(GetObjectW)(bm, sizeof bmp, &bmp) == sizeof bmp);
    CHECK(bmp.bmWidth == 8 && bmp.bmHeight == 8);
    CHECK(U(DeleteObject)(bm));
}

TEST(brushes_pattern_same_bitmap_two_brushes)
{
    NEED_BR(); REQUIRE(U(CreatePatternBrush)); REQUIRE(U(CreateBitmap));
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, NULL);
    HBRUSH a = U(CreatePatternBrush)(bm), b = U(CreatePatternBrush)(bm);
    CHECK(a && b && a != b);
    CHECK(U(DeleteObject)(a));
    LOGBRUSH lb;
    CHECK(get_lb(b, &lb) == sizeof lb && lb.lbStyle == BS_PATTERN);
    CHECK(U(DeleteObject)(b));
    CHECK(U(DeleteObject)(bm));
}

TEST(brushes_pattern_huge_bitmap)
{
    NEED_BR(); REQUIRE(U(CreatePatternBrush)); REQUIRE(U(CreateBitmap));
    HBITMAP bm = U(CreateBitmap)(4096, 4096, 1, 1, NULL);
    if (!bm) { t_note("CreateBitmap 4096x4096 mono failed"); return; }
    HBRUSH b = U(CreatePatternBrush)(bm);
    t_note("pattern brush from 4096x4096 mono bitmap -> %s", b ? "ok" : "failed");
    if (b) U(DeleteObject)(b);
    U(DeleteObject)(bm);
}

TEST(brushes_pattern_indirect_matches)
{
    NEED_BR(); REQUIRE(U(CreatePatternBrush)); REQUIRE(U(CreateBitmap));
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, NULL);
    LOGBRUSH in = { BS_PATTERN, 0x12345, (ULONG_PTR)bm }, out;
    HBRUSH b = U(CreateBrushIndirect)(&in);
    CHECK(b != NULL);
    CHECK(get_lb(b, &out) == sizeof out);
    CHECK(out.lbStyle == BS_PATTERN && out.lbColor == 0 && out.lbHatch == (ULONG_PTR)bm);
    U(DeleteObject)(b);
    in.lbHatch = 0;
    SetLastError(0xdeadbeef);
    b = U(CreateBrushIndirect)(&in);
    CHECK(b == NULL);
    if (b) U(DeleteObject)(b);
    U(DeleteObject)(bm);
}

TEST(brushes_pattern8x8_style)
{
    NEED_BR(); REQUIRE(U(CreateBitmap));
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, NULL);
    LOGBRUSH in = { BS_PATTERN8X8, 0, (ULONG_PTR)bm }, out;
    HBRUSH b = U(CreateBrushIndirect)(&in);
    CHECK(b != NULL);
    if (b) {
        CHECK(get_lb(b, &out) == sizeof out);
        CHECK(out.lbStyle == BS_PATTERN);
        CHECK(out.lbHatch == (ULONG_PTR)bm);
        U(DeleteObject)(b);
    }
    HBITMAP big = U(CreateBitmap)(20, 20, 1, 1, NULL);
    in.lbHatch = (ULONG_PTR)big;
    b = U(CreateBrushIndirect)(&in);
    t_note("BS_PATTERN8X8 with 20x20 bitmap -> %s", b ? "ok" : "failed");
    if (b) U(DeleteObject)(b);
    U(DeleteObject)(big);
    U(DeleteObject)(bm);
}

TEST(brushes_dibpatternpt_depths)
{
    NEED_BR(); REQUIRE(U(CreateDIBPatternBrushPt));
    static const int bpp[] = { 1, 4, 8, 16, 24, 32 };
    for (int i = 0; i < 6; i++)
    for (int usage = DIB_RGB_COLORS; usage <= DIB_PAL_COLORS; usage++) {
        BITMAPINFO *bi = mkdib(8, 8, bpp[i], NULL);
        HBRUSH b = U(CreateDIBPatternBrushPt)(bi, usage);
        CHECKF(b != NULL, "%dbpp usage %d", bpp[i], usage);
        if (b) {
            LOGBRUSH lb;
            CHECK(get_lb(b, &lb) == sizeof lb);
            CHECK(lb.lbStyle == BS_DIBPATTERN);
            CHECK(lb.lbColor == 0);
            if (lb.lbHatch != (ULONG_PTR)bi) t_note("DIB brush lbHatch %p vs source %p", (void *)lb.lbHatch, (void *)bi);
            CHECK(U(GetObjectType)(b) == OBJ_BRUSH);
            CHECK(U(DeleteObject)(b));
        }
        free(bi);
    }
}

TEST(brushes_dibpatternpt_hatch_is_source_pointer)
{
    NEED_BR(); REQUIRE(U(CreateDIBPatternBrushPt));
    BITMAPINFO *bi = mkdib(16, 16, 1, NULL);
    HBRUSH b = U(CreateDIBPatternBrushPt)(bi, DIB_RGB_COLORS);
    LOGBRUSH lb;
    CHECK(b && get_lb(b, &lb) == sizeof lb);
    CHECK(lb.lbHatch == (ULONG_PTR)bi);
    U(DeleteObject)(b);
    free(bi);
}

TEST(brushes_dibpatternpt_null)
{
    NEED_BR(); REQUIRE(U(CreateDIBPatternBrushPt));
    SetLastError(0xdeadbeef);
    CHECK(U(CreateDIBPatternBrushPt)(NULL, DIB_RGB_COLORS) == NULL);
    CHECK(U(CreateDIBPatternBrushPt)(NULL, DIB_PAL_COLORS) == NULL);
    CHECK(U(CreateDIBPatternBrushPt)(NULL, 77) == NULL);
}

TEST(brushes_dibpatternpt_usage_values)
{
    NEED_BR(); REQUIRE(U(CreateDIBPatternBrushPt));
    BITMAPINFO *bi = mkdib(8, 8, 1, NULL);
    for (UINT u = 0; u <= 6; u++) {
        HBRUSH b = U(CreateDIBPatternBrushPt)(bi, u);
        if (u <= DIB_PAL_COLORS + 1) CHECKF(b != NULL, "usage %u", u);
        else if (u == DIB_PAL_COLORS + 2) CHECKF(b == NULL, "usage %u", u);
        else t_note("usage %u -> %s", u, b ? "ok" : "failed");
        if (b) U(DeleteObject)(b);
    }
    static const UINT big[] = { 0x100, 0xffff, 0x7fffffff, 0xffffffffu };
    for (int i = 0; i < 4; i++) {
        HBRUSH b = U(CreateDIBPatternBrushPt)(bi, big[i]);
        t_note("usage %x -> %s", big[i], b ? "ok" : "failed");
        if (b) U(DeleteObject)(b);
    }
    free(bi);
}

TEST(brushes_dibpatternpt_rle_rejected)
{
    NEED_BR(); REQUIRE(U(CreateDIBPatternBrushPt));
    BITMAPINFO *bi = mkdib(8, 8, 8, NULL);
    bi->bmiHeader.biCompression = BI_RLE8;
    CHECK(U(CreateDIBPatternBrushPt)(bi, DIB_RGB_COLORS) == NULL);
    free(bi);
    bi = mkdib(8, 8, 4, NULL);
    bi->bmiHeader.biCompression = BI_RLE4;
    CHECK(U(CreateDIBPatternBrushPt)(bi, DIB_RGB_COLORS) == NULL);
    free(bi);
}

TEST(brushes_dibpatternpt_other_compressions)
{
    NEED_BR(); REQUIRE(U(CreateDIBPatternBrushPt));
    static const DWORD comp[] = { BI_BITFIELDS, BI_JPEG, BI_PNG, 6, 100 };
    for (int i = 0; i < 5; i++) {
        BITMAPINFO *bi = mkdib(8, 8, 32, NULL);
        bi->bmiHeader.biCompression = comp[i];
        HBRUSH b = U(CreateDIBPatternBrushPt)(bi, DIB_RGB_COLORS);
        t_note("compression %lu -> %s", comp[i], b ? "ok" : "failed");
        if (b) U(DeleteObject)(b);
        free(bi);
    }
}

TEST(brushes_dibpatternpt_core_header)
{
    NEED_BR(); REQUIRE(U(CreateDIBPatternBrushPt));
    BYTE buf[sizeof(BITMAPCOREHEADER) + 8 * 4 * 3 + 16];
    memset(buf, 0, sizeof buf);
    BITMAPCOREHEADER *c = (BITMAPCOREHEADER *)buf;
    c->bcSize = sizeof *c; c->bcWidth = 8; c->bcHeight = 8; c->bcPlanes = 1; c->bcBitCount = 24;
    HBRUSH b = U(CreateDIBPatternBrushPt)(c, DIB_RGB_COLORS);
    t_note("BITMAPCOREHEADER 24bpp -> %s", b ? "ok" : "failed");
    CHECK(b != NULL);
    if (b) U(DeleteObject)(b);
}

TEST(brushes_dibpatternpt_v4_v5_headers)
{
    NEED_BR(); REQUIRE(U(CreateDIBPatternBrushPt));
    BYTE buf[sizeof(BITMAPV5HEADER) + 8 * 8 * 4];
    for (int v = 4; v <= 5; v++) {
        memset(buf, 0, sizeof buf);
        BITMAPV5HEADER *h = (BITMAPV5HEADER *)buf;
        h->bV5Size = v == 4 ? sizeof(BITMAPV4HEADER) : sizeof(BITMAPV5HEADER);
        h->bV5Width = 8; h->bV5Height = 8; h->bV5Planes = 1; h->bV5BitCount = 32; h->bV5Compression = BI_RGB;
        HBRUSH b = U(CreateDIBPatternBrushPt)(h, DIB_RGB_COLORS);
        t_note("V%d header -> %s", v, b ? "ok" : "failed");
        CHECK(b != NULL);
        if (b) U(DeleteObject)(b);
    }
}

TEST(brushes_dibpatternpt_bad_headers_notes)
{
    NEED_BR(); REQUIRE(U(CreateDIBPatternBrushPt));
    struct { const char *n; DWORD size; LONG w, h; WORD planes, bpp; } t[] = {
        {"size 0", 0, 8, 8, 1, 24}, {"size 1", 1, 8, 8, 1, 24}, {"size 39", 39, 8, 8, 1, 24}, {"size 41", 41, 8, 8, 1, 24},
        {"width 0", 40, 0, 8, 1, 24}, {"height 0", 40, 8, 0, 1, 24}, {"planes 0", 40, 8, 8, 0, 24}, {"planes 2", 40, 8, 8, 2, 24},
        {"bpp 0", 40, 8, 8, 1, 0}, {"bpp 3", 40, 8, 8, 1, 3}, {"bpp 64", 40, 8, 8, 1, 64}, {"neg width", 40, -8, 8, 1, 24},
        {"top-down", 40, 8, -8, 1, 24}, {"huge", 40, 0x7fffff, 0x7fffff, 1, 32},
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        BYTE *buf = calloc(1, 4096);
        BITMAPINFOHEADER *h = (BITMAPINFOHEADER *)buf;
        h->biSize = t[i].size; h->biWidth = t[i].w; h->biHeight = t[i].h; h->biPlanes = t[i].planes; h->biBitCount = t[i].bpp;
        h->biCompression = BI_RGB;
        if (!strcmp(t[i].n, "huge")) { free(buf); continue; }
        HBRUSH b = U(CreateDIBPatternBrushPt)(buf, DIB_RGB_COLORS);
        t_note("bad header '%s' -> %s", t[i].n, b ? "ok" : "failed");
        if (b) U(DeleteObject)(b);
        free(buf);
    }
}

TEST(brushes_dibpattern_hglobal)
{
    NEED_BR(); REQUIRE(U(CreateDIBPatternBrush));
    size_t sz;
    BITMAPINFO *bi = mkdib(8, 8, 24, &sz);
    static const UINT flags[] = { GMEM_MOVEABLE, GMEM_FIXED };
    for (int k = 0; k < 2; k++) {
        HGLOBAL g = GlobalAlloc(flags[k] | GMEM_ZEROINIT, sz);
        void *p = GlobalLock(g);
        memcpy(p, bi, sz);
        GlobalUnlock(g);
        UINT before = GlobalFlags(g);
        HBRUSH b = U(CreateDIBPatternBrush)(g, DIB_RGB_COLORS);
        CHECKF(b != NULL, "flags %x", flags[k]);
        CHECKF(GlobalFlags(g) == before, "lock count changed %x -> %x", before, GlobalFlags(g));
        if (b) {
            LOGBRUSH lb;
            CHECK(get_lb(b, &lb) == sizeof lb);
            CHECK(lb.lbStyle == BS_DIBPATTERN && lb.lbColor == 0);
            void *lp = GlobalLock(g);
            CHECK(lb.lbHatch == (ULONG_PTR)lp);
            GlobalUnlock(g);
            CHECK(U(DeleteObject)(b));
        }
        CHECKF(GlobalFlags(g) == before, "lock count after delete");
        GlobalFree(g);
    }
    free(bi);
}

TEST(brushes_dibpattern_null_handle)
{
    NEED_BR(); REQUIRE(U(CreateDIBPatternBrush));
    HBRUSH b = U(CreateDIBPatternBrush)(NULL, DIB_RGB_COLORS);
    CHECK(b == NULL);
    if (b) U(DeleteObject)(b);
}

TEST(brushes_dibpattern_usage_values)
{
    NEED_BR(); REQUIRE(U(CreateDIBPatternBrush));
    size_t sz;
    BITMAPINFO *bi = mkdib(8, 8, 1, &sz);
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sz);
    memcpy(GlobalLock(g), bi, sz);
    GlobalUnlock(g);
    for (UINT u = 0; u <= 5; u++) {
        HBRUSH b = U(CreateDIBPatternBrush)(g, u);
        if (u <= DIB_PAL_COLORS + 1) CHECKF(b != NULL, "usage %u", u);
        else if (u == DIB_PAL_COLORS + 2) CHECKF(b == NULL, "usage %u", u);
        else t_note("usage %u -> %s", u, b ? "ok" : "failed");
        if (b) U(DeleteObject)(b);
    }
    GlobalFree(g);
    free(bi);
}

TEST(brushes_indirect_dibpattern_variants)
{
    NEED_BR();
    size_t sz;
    BITMAPINFO *bi = mkdib(8, 8, 8, &sz);
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sz);
    memcpy(GlobalLock(g), bi, sz);
    GlobalUnlock(g);
    LOGBRUSH in, out;
    in.lbStyle = BS_DIBPATTERNPT; in.lbColor = DIB_RGB_COLORS; in.lbHatch = (ULONG_PTR)bi;
    HBRUSH b = U(CreateBrushIndirect)(&in);
    CHECK(b != NULL);
    CHECK(b && get_lb(b, &out) == sizeof out);
    CHECK(out.lbStyle == BS_DIBPATTERN && out.lbColor == 0 && out.lbHatch == (ULONG_PTR)bi);
    U(DeleteObject)(b);
    in.lbStyle = BS_DIBPATTERN; in.lbHatch = (ULONG_PTR)g;
    b = U(CreateBrushIndirect)(&in);
    CHECK(b != NULL);
    CHECK(b && get_lb(b, &out) == sizeof out);
    CHECK(out.lbStyle == BS_DIBPATTERN && out.lbColor == 0);
    CHECK(out.lbHatch != (ULONG_PTR)g || 1);
    U(DeleteObject)(b);
    in.lbColor = DIB_PAL_COLORS;
    b = U(CreateBrushIndirect)(&in);
    CHECK(b != NULL);
    if (b) U(DeleteObject)(b);
    in.lbColor = 3;
    b = U(CreateBrushIndirect)(&in);
    CHECK(b == NULL);
    if (b) U(DeleteObject)(b);
    in.lbStyle = BS_DIBPATTERN; in.lbColor = DIB_RGB_COLORS; in.lbHatch = 0;
    b = U(CreateBrushIndirect)(&in);
    CHECK(b == NULL);
    if (b) U(DeleteObject)(b);
    in.lbStyle = BS_DIBPATTERNPT; in.lbHatch = 0;
    b = U(CreateBrushIndirect)(&in);
    CHECK(b == NULL);
    if (b) U(DeleteObject)(b);
    GlobalFree(g);
    free(bi);
}

TEST(brushes_dibpattern8x8_style)
{
    NEED_BR();
    BITMAPINFO *bi = mkdib(8, 8, 24, NULL);
    LOGBRUSH in = { BS_DIBPATTERN8X8, DIB_RGB_COLORS, (ULONG_PTR)bi }, out;
    HBRUSH b = U(CreateBrushIndirect)(&in);
    t_note("BS_DIBPATTERN8X8 -> %s", b ? "ok" : "failed");
    if (b) {
        CHECK(get_lb(b, &out) == sizeof out);
        t_note("  reads back style %u color %lx", out.lbStyle, out.lbColor);
        U(DeleteObject)(b);
    }
    free(bi);
}

TEST(brushes_getobject_buffer_matrix)
{
    NEED_BR(); REQUIRE(U(CreatePatternBrush)); REQUIRE(U(CreateBitmap));
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, NULL);
    BITMAPINFO *bi = mkdib(8, 8, 1, NULL);
    HBRUSH br[] = { U(CreateSolidBrush)(0x123456), U(CreateHatchBrush)(HS_CROSS, 0x654321), U(CreatePatternBrush)(bm),
        U(CreateDIBPatternBrushPt)(bi, DIB_RGB_COLORS) };
    for (int i = 0; i < 4; i++) {
        CHECKF(br[i] != NULL, "kind %d", i);
        if (!br[i]) continue;
        CHECKF(U(GetObjectW)(br[i], 0, NULL) == sizeof(LOGBRUSH), "kind %d size query", i);
        BYTE buf[64];
        for (int sz = (int)sizeof(LOGBRUSH); sz <= 64; sz += 8) {
            memset(buf, 0xcd, sizeof buf);
            int n = U(GetObjectW)(br[i], sz, buf);
            CHECKF(n == sizeof(LOGBRUSH), "kind %d size %d n=%d", i, sz, n);
            int clean = 1;
            for (int k = sizeof(LOGBRUSH); k < sz; k++) if (buf[k] != 0xcd) clean = 0;
            CHECKF(clean, "kind %d size %d overflow", i, sz);
        }
        for (int sz = 1; sz < (int)sizeof(LOGBRUSH); sz++) {
            memset(buf, 0xcd, sizeof buf);
            int n = U(GetObjectW)(br[i], sz, buf);
            if (n != 0 && n != sz) t_note("kind %d GetObject(size %d) = %d", i, sz, n);
        }
        U(DeleteObject)(br[i]);
    }
    U(DeleteObject)(bm);
    free(bi);
}

TEST(brushes_getobject_invalid_handles)
{
    NEED_BR();
    LOGBRUSH lb;
    HANDLE bad[] = { NULL, (HANDLE)1, (HANDLE)0xdeadbeef, (HANDLE)(LONG_PTR)-1, (HANDLE)0x300000, (HANDLE)0x7fffffff };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        CHECKF(U(GetObjectW)(bad[i], sizeof lb, &lb) == 0, "h=%p", bad[i]);
        CHECKF(U(GetObjectType)(bad[i]) == 0, "h=%p", bad[i]);
        CHECKF(!U(DeleteObject)(bad[i]), "h=%p", bad[i]);
    }
}

TEST(brushes_select_into_dc)
{
    NEED_BR(); NEED_MEM(); REQUIRE(U(GetCurrentObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ def = U(GetCurrentObject)(dc, OBJ_BRUSH);
    CHECK(def == GetStockObject(WHITE_BRUSH));
    HBRUSH a = U(CreateSolidBrush)(1), b = U(CreateHatchBrush)(HS_VERTICAL, 2);
    CHECK(U(SelectObject)(dc, a) == def);
    CHECK(U(GetCurrentObject)(dc, OBJ_BRUSH) == a);
    CHECK(U(SelectObject)(dc, b) == a);
    CHECK(U(SelectObject)(dc, b) == b);
    CHECK(U(SelectObject)(dc, def) == b);
    CHECK(U(DeleteObject)(a));
    CHECK(U(DeleteObject)(b));
    CHECK(U(SelectObject)(dc, a) == NULL);
    CHECK(U(GetCurrentObject)(dc, OBJ_BRUSH) == def);
    CHECK(U(DeleteDC)(dc));
}

TEST(brushes_delete_selected)
{
    NEED_BR(); NEED_MEM(); REQUIRE(U(GetCurrentObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ def = U(GetCurrentObject)(dc, OBJ_BRUSH);
    HBRUSH a = U(CreateSolidBrush)(0x7f7f7f);
    U(SelectObject)(dc, a);
    BOOL r1 = U(DeleteObject)(a);
    t_note("DeleteObject(selected brush) = %d", r1);
    U(SelectObject)(dc, def);
    BOOL r2 = U(DeleteObject)(a);
    CHECK(r1 || r2);
    LOGBRUSH lb;
    CHECK(get_lb(a, &lb) == 0);
    U(DeleteDC)(dc);
}

typedef struct { HBRUSH b; int n; LOGBRUSH lb; BOOL del; } XB;
static DWORD WINAPI xb_proc(LPVOID a)
{
    XB *x = a;
    x->n = U(GetObjectW)(x->b, sizeof x->lb, &x->lb);
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ old = U(SelectObject)(dc, x->b);
    U(SelectObject)(dc, old);
    U(DeleteDC)(dc);
    x->del = U(DeleteObject)(x->b);
    return 0;
}
TEST(brushes_cross_thread_use_delete)
{
    NEED_BR(); NEED_MEM();
    XB x; memset(&x, 0, sizeof x);
    x.b = U(CreateHatchBrush)(HS_BDIAGONAL, 0x102030);
    HANDLE th = CreateThread(NULL, 0, xb_proc, &x, 0, NULL);
    WaitForSingleObject(th, 10000);
    CloseHandle(th);
    CHECK(x.n == sizeof(LOGBRUSH));
    CHECK(x.lb.lbStyle == BS_HATCHED && x.lb.lbHatch == HS_BDIAGONAL && x.lb.lbColor == 0x102030);
    CHECK(x.del);
    LOGBRUSH lb;
    CHECK(get_lb(x.b, &lb) == 0);
}

TEST(brushes_leak_loop_all_kinds)
{
    NEED_BR(); REQUIRE(U(CreatePatternBrush)); REQUIRE(U(CreateBitmap)); REQUIRE(U(CreateDIBPatternBrushPt));
    DWORD before = pb_gdi_count();
    if (!before) { t_skip("GetGuiResources unavailable"); return; }
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, NULL);
    BITMAPINFO *bi = mkdib(8, 8, 8, NULL);
    for (int i = 0; i < 1500; i++) {
        HBRUSH b;
        switch (i & 3) {
        case 0: b = U(CreateSolidBrush)(i); break;
        case 1: b = U(CreateHatchBrush)(i % 6, i); break;
        case 2: b = U(CreatePatternBrush)(bm); break;
        default: b = U(CreateDIBPatternBrushPt)(bi, DIB_RGB_COLORS); break;
        }
        if (!b) { CHECK(b != NULL); break; }
        U(DeleteObject)(b);
    }
    U(DeleteObject)(bm);
    free(bi);
    DWORD after = pb_gdi_count();
    CHECKF(after <= before + 2, "before %lu after %lu", before, after);
}

TEST(brushes_leak_failed_creates)
{
    NEED_BR(); REQUIRE(U(CreatePatternBrush)); REQUIRE(U(CreateDIBPatternBrushPt));
    DWORD before = pb_gdi_count();
    if (!before) { t_skip("GetGuiResources unavailable"); return; }
    BITMAPINFO *bi = mkdib(8, 8, 8, NULL);
    bi->bmiHeader.biCompression = BI_RLE8;
    for (int i = 0; i < 500; i++) {
        HBRUSH b = U(CreateHatchBrush)(99, 0);
        if (b) U(DeleteObject)(b);
        b = U(CreatePatternBrush)(NULL);
        if (b) U(DeleteObject)(b);
        b = U(CreateDIBPatternBrushPt)(bi, DIB_RGB_COLORS);
        if (b) U(DeleteObject)(b);
        LOGBRUSH in = { 4, 0, 0 };
        b = U(CreateBrushIndirect)(&in);
        if (b) U(DeleteObject)(b);
    }
    free(bi);
    DWORD after = pb_gdi_count();
    CHECKF(after <= before + 1, "before %lu after %lu", before, after);
}

TEST(brushes_leak_alive_count_tracks)
{
    NEED_BR();
    DWORD before = pb_gdi_count();
    if (!before) { t_skip("GetGuiResources unavailable"); return; }
    enum { N = 300 };
    static HBRUSH bs[N];
    for (int i = 0; i < N; i++) bs[i] = U(CreateSolidBrush)(RGB(i & 255, i >> 8, 1));
    DWORD mid = pb_gdi_count();
    CHECKF(mid >= before + N - 2 && mid <= before + N + 2, "before %lu mid %lu", before, mid);
    for (int i = 0; i < N; i++) U(DeleteObject)(bs[i]);
    CHECKF(pb_gdi_count() <= before + 2, "after %lu", pb_gdi_count());
}

TEST(brushes_dcbrush_color_api)
{
    NEED_BR(); NEED_MEM(); REQUIRE(U(SetDCBrushColor)); REQUIRE(U(GetDCBrushColor));
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(U(GetDCBrushColor)(dc) == RGB(255, 255, 255));
    CHECK(U(SetDCBrushColor)(dc, RGB(1, 2, 3)) == RGB(255, 255, 255));
    CHECK(U(GetDCBrushColor)(dc) == RGB(1, 2, 3));
    CHECK(U(SetDCBrushColor)(dc, RGB(9, 8, 7)) == RGB(1, 2, 3));
    CHECK(U(GetDCBrushColor)(dc) == RGB(9, 8, 7));
    U(DeleteDC)(dc);
}

TEST(brushes_dcbrush_invalid_dc)
{
    NEED_BR(); REQUIRE(U(SetDCBrushColor)); REQUIRE(U(GetDCBrushColor));
    CHECK(U(SetDCBrushColor)(NULL, 0) == CLR_INVALID);
    CHECK(U(GetDCBrushColor)(NULL) == CLR_INVALID);
    CHECK(U(SetDCBrushColor)((HDC)(LONG_PTR)-1, 0) == CLR_INVALID);
    CHECK(U(GetDCBrushColor)((HDC)0xdeadbeef) == CLR_INVALID);
    HBRUSH b = U(CreateSolidBrush)(0);
    CHECK(U(GetDCBrushColor)((HDC)b) == CLR_INVALID);
    U(DeleteObject)(b);
}

TEST(brushes_dcbrush_per_dc_and_saverestore)
{
    NEED_BR(); NEED_MEM(); REQUIRE(U(SetDCBrushColor)); REQUIRE(U(GetDCBrushColor)); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC a = U(CreateCompatibleDC)(NULL), b = U(CreateCompatibleDC)(NULL);
    U(SetDCBrushColor)(a, 0x111111);
    U(SetDCBrushColor)(b, 0x222222);
    CHECK(U(GetDCBrushColor)(a) == 0x111111 && U(GetDCBrushColor)(b) == 0x222222);
    int s = U(SaveDC)(a);
    U(SetDCBrushColor)(a, 0x333333);
    CHECK(U(RestoreDC)(a, s));
    CHECK(U(GetDCBrushColor)(a) == 0x111111);
    U(DeleteDC)(a); U(DeleteDC)(b);
}

TEST(brushes_object_type_codes)
{
    NEED_BR(); REQUIRE(U(CreatePatternBrush)); REQUIRE(U(CreateBitmap));
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, NULL);
    HBRUSH b[3] = { U(CreateSolidBrush)(0), U(CreateHatchBrush)(0, 0), U(CreatePatternBrush)(bm) };
    for (int i = 0; i < 3; i++) { CHECK(U(GetObjectType)(b[i]) == OBJ_BRUSH); U(DeleteObject)(b[i]); }
    CHECK(U(GetObjectType)(GetStockObject(NULL_BRUSH)) == OBJ_BRUSH);
    CHECK(U(GetObjectType)(bm) == OBJ_BITMAP);
    U(DeleteObject)(bm);
}

TEST(brushes_orgex_api)
{
    NEED_BR(); NEED_MEM(); REQUIRE(U(SetBrushOrgEx)); REQUIRE(U(GetBrushOrgEx));
    HDC dc = U(CreateCompatibleDC)(NULL);
    POINT pt = { 55, 66 };
    CHECK(U(GetBrushOrgEx)(dc, &pt));
    CHECK(pt.x == 0 && pt.y == 0);
    CHECK(U(SetBrushOrgEx)(dc, 3, 4, &pt));
    CHECK(pt.x == 0 && pt.y == 0);
    CHECK(U(SetBrushOrgEx)(dc, 0x10000, -1, &pt));
    CHECK(pt.x == 3 && pt.y == 4);
    CHECK(U(GetBrushOrgEx)(dc, &pt));
    CHECK(pt.x == 0x10000 && pt.y == -1);
    CHECK(U(SetBrushOrgEx)(dc, 7, 7, NULL));
    CHECK(U(GetBrushOrgEx)(dc, &pt) && pt.x == 7 && pt.y == 7);
    CHECK(U(SetBrushOrgEx)(dc, 0, 0, &pt) && pt.x == 7);
    U(DeleteDC)(dc);
}

TEST(brushes_orgex_extremes)
{
    NEED_BR(); NEED_MEM(); REQUIRE(U(SetBrushOrgEx)); REQUIRE(U(GetBrushOrgEx));
    HDC dc = U(CreateCompatibleDC)(NULL);
    static const int v[] = { 8, 9, 100, 255, 256, 1000, -8, -9, -1000, 0x7fff, 0x8000, 0xffff, INT_MAX, INT_MIN };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        POINT pt;
        BOOL r = U(SetBrushOrgEx)(dc, v[i], -v[i], NULL);
        CHECKF(r, "v=%d", v[i]);
        CHECK(U(GetBrushOrgEx)(dc, &pt));
        if (pt.x != v[i] || pt.y != -v[i]) t_note("brush org (%d,%d) reads back (%ld,%ld)", v[i], -v[i], pt.x, pt.y);
    }
    U(DeleteDC)(dc);
}

TEST(brushes_orgex_invalid_dc)
{
    NEED_BR(); REQUIRE(U(SetBrushOrgEx)); REQUIRE(U(GetBrushOrgEx));
    POINT pt = { 5, 6 };
    CHECK(!U(SetBrushOrgEx)(NULL, 1, 1, &pt));
    CHECK(!U(GetBrushOrgEx)(NULL, &pt));
    CHECK(!U(SetBrushOrgEx)((HDC)(LONG_PTR)-1, 1, 1, &pt));
    CHECK(!U(GetBrushOrgEx)((HDC)0xdeadbeef, &pt));
    HBRUSH b = U(CreateSolidBrush)(0);
    CHECK(!U(GetBrushOrgEx)((HDC)b, &pt));
    U(DeleteObject)(b);
}

TEST(brushes_fixbrushorgex_is_noop)
{
    NEED_BR(); NEED_MEM(); REQUIRE(U(FixBrushOrgEx)); REQUIRE(U(GetBrushOrgEx)); REQUIRE(U(SetBrushOrgEx));
    HDC dc = U(CreateCompatibleDC)(NULL);
    U(SetBrushOrgEx)(dc, 5, 6, NULL);
    POINT pt = { 10, 20 };
    BOOL r = U(FixBrushOrgEx)(dc, 2, 3, &pt);
    t_note("FixBrushOrgEx = %d, pt=(%ld,%ld)", r, pt.x, pt.y);
    POINT o;
    CHECK(U(GetBrushOrgEx)(dc, &o) && o.x == 5 && o.y == 6);
    U(DeleteDC)(dc);
}

TEST(brushes_unrealizeobject_variants)
{
    NEED_BR(); NEED_MEM(); REQUIRE(U(UnrealizeObject));
    HBRUSH b = U(CreateHatchBrush)(HS_CROSS, 0x10);
    CHECK(U(UnrealizeObject)(b));
    HBRUSH s = U(CreateSolidBrush)(5);
    CHECK(U(UnrealizeObject)(s));
    LOGBRUSH lb;
    CHECK(get_lb(b, &lb) == sizeof lb && lb.lbHatch == HS_CROSS);
    t_note("UnrealizeObject(stock) = %d", U(UnrealizeObject)(GetStockObject(BLACK_BRUSH)));
    t_note("UnrealizeObject(NULL) = %d", U(UnrealizeObject)(NULL));
    U(DeleteObject)(b); U(DeleteObject)(s);
}
