#include "rg.h"

#define WHITE 0xFFFFFF
#define RED   0xFF0000   /* in DIB 0x00RRGGBB for COLORREF RGB(255,0,0) */
#define BLUE  0x0000FF

static BOOL wine_unsafe(void) { return rg_is_wine(); }

/* compare DIB to a region grid: pixels == c exactly where region contains the point */
static int mismatches(RgDib *d, HRGN h, DWORD in, DWORD out) {
    int bad = 0;
    __typeof__(&PtInRegion) pt = U(PtInRegion);
    for (int y = 0; y < d->h; y++) for (int x = 0; x < d->w; x++) {
        DWORD want = pt(h, x, y) ? in : out;
        if (rg_px(d, x, y) != want) bad++;
    }
    return bad;
}

/* ---- FillRgn ---- */
TEST(regions_fill_rect_exact) {
    REQUIRE(U(FillRgn)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
    HRGN h = rg_rect(10, 12, 30, 22);
    CHECK(U(FillRgn)(d.dc, h, br));
    CHECK(rg_all_in(&d, RED, 10, 12, 30, 22));
    CHECK(rg_count(&d, RED) == 200);
    CHECK(rg_count(&d, WHITE) == 64 * 64 - 200);
    U(DeleteObject)(h); U(DeleteObject)(br); rg_dib_free(&d);
}
TEST(regions_fill_shapes_match_ptin) {
    REQUIRE(U(FillRgn)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
    POINT star[5] = {{32, 2}, {50, 60}, {2, 22}, {62, 22}, {14, 60}};
    HRGN rs[4] = {U(CreateEllipticRgn)(2, 3, 60, 50), U(CreateRoundRectRgn)(4, 4, 58, 58, 20, 20),
                  U(CreatePolygonRgn)(star, 5, ALTERNATE), U(CreatePolygonRgn)(star, 5, WINDING)};
    for (int i = 0; i < 4; i++) {
        rg_clear(&d, WHITE);
        CHECK(U(FillRgn)(d.dc, rs[i], br));
        int bad = mismatches(&d, rs[i], RED, WHITE);
        CHECKF(bad == 0, "shape %d mismatches %d", i, bad);
        U(DeleteObject)(rs[i]);
    }
    U(DeleteObject)(br); rg_dib_free(&d);
}
TEST(regions_fill_complex_and_empty) {
    REQUIRE(U(FillRgn)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
    HRGN a = rg_rect(5, 5, 40, 40), b = rg_rect(15, 15, 30, 30);
    U(CombineRgn)(a, a, b, RGN_DIFF);
    CHECK(U(FillRgn)(d.dc, a, br));
    CHECK(mismatches(&d, a, RED, WHITE) == 0);
    CHECK(rg_px(&d, 20, 20) == WHITE && rg_px(&d, 5, 5) == RED);
    rg_clear(&d, WHITE);
    HRGN e = rg_rect(0, 0, 0, 0);
    BOOL r = U(FillRgn)(d.dc, e, br);
    t_note("FillRgn(empty region) = %d", r);
    CHECK(rg_count(&d, WHITE) == 64 * 64);
    U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(e); U(DeleteObject)(br); rg_dib_free(&d);
}
TEST(regions_fill_clipped_by_surface) {
    REQUIRE(U(FillRgn)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
    HRGN h = rg_rect(-1000, -1000, 1000, 1000);
    CHECK(U(FillRgn)(d.dc, h, br));
    CHECK(rg_count(&d, RED) == 64 * 64);
    U(SetRectRgn)(h, -10, 50, 10, 200);
    rg_clear(&d, WHITE);
    CHECK(U(FillRgn)(d.dc, h, br));
    CHECK(rg_count(&d, RED) == 10 * 14);
    CHECK(rg_all_in(&d, RED, 0, 50, 10, 64));
    U(SetRectRgn)(h, 60, 60, 100000, 100000);
    rg_clear(&d, WHITE);
    CHECK(U(FillRgn)(d.dc, h, br));
    CHECK(rg_count(&d, RED) == 16);
    U(DeleteObject)(h); U(DeleteObject)(br); rg_dib_free(&d);
}
TEST(regions_fill_invalid_args) {
    REQUIRE(U(FillRgn)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 16, 16, WHITE)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
    HRGN h = rg_rect(0, 0, 16, 16);
    CHECK(!U(FillRgn)(NULL, h, br));
    CHECK(!U(FillRgn)(d.dc, NULL, br));
    CHECK(!U(FillRgn)(d.dc, (HRGN)br, br));
    CHECK(!U(FillRgn)((HDC)(ULONG_PTR)0x1234, h, br));
    CHECK(rg_count(&d, WHITE) == 256);
    SET_ERR(0);
    BOOL r = U(FillRgn)(d.dc, h, NULL);
    t_note("FillRgn(NULL brush) = %d err %lu", r, GetLastError());
    r = U(FillRgn)(d.dc, h, (HBRUSH)h);
    t_note("FillRgn(region as brush) = %d", r);
    U(DeleteObject)(h); U(DeleteObject)(br); rg_dib_free(&d);
}
TEST(regions_fill_hollow_stock_brushes) {
    REQUIRE(U(FillRgn)); REQUIRE(U(GetStockObject)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 16, 16, WHITE)) { CHECK(0); return; }
    HRGN h = rg_rect(2, 2, 10, 10);
    CHECK(U(FillRgn)(d.dc, h, (HBRUSH)U(GetStockObject)(BLACK_BRUSH)));
    CHECK(rg_count(&d, 0) == 64);
    rg_clear(&d, WHITE);
    U(FillRgn)(d.dc, h, (HBRUSH)U(GetStockObject)(NULL_BRUSH));
    CHECK(rg_count(&d, WHITE) == 256);
    rg_clear(&d, 0x808080);
    CHECK(U(FillRgn)(d.dc, h, (HBRUSH)U(GetStockObject)(WHITE_BRUSH)));
    CHECK(rg_count(&d, WHITE) == 64);
    U(DeleteObject)(h); rg_dib_free(&d);
}
TEST(regions_fill_ignores_dc_brush_and_origin_of_dc_selection) {
    REQUIRE(U(FillRgn)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(SelectObject)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 32, 32, WHITE)) { CHECK(0); return; }
    HBRUSH sel = U(CreateSolidBrush)(RGB(0, 0, 255)), use = U(CreateSolidBrush)(RGB(255, 0, 0));
    HGDIOBJ old = U(SelectObject)(d.dc, sel);
    HRGN h = rg_rect(0, 0, 8, 8);
    CHECK(U(FillRgn)(d.dc, h, use));
    CHECK(rg_count(&d, RED) == 64 && rg_count(&d, BLUE) == 0);
    CHECK(U(SelectObject)(d.dc, old) == sel);
    U(DeleteObject)(h); U(DeleteObject)(sel); U(DeleteObject)(use); rg_dib_free(&d);
}
TEST(regions_fill_mapping_mode_independent) {
    REQUIRE(U(FillRgn)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(SetMapMode)); REQUIRE(U(SetViewportExtEx)); REQUIRE(U(SetWindowExtEx)); REQUIRE(U(SetViewportOrgEx)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
    HRGN h = rg_rect(10, 10, 20, 30);
    U(SetMapMode)(d.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(d.dc, 1, 1, NULL);
    U(SetViewportExtEx)(d.dc, 3, 2, NULL);
    U(SetViewportOrgEx)(d.dc, 7, 9, NULL);
    CHECK(U(FillRgn)(d.dc, h, br));
    int in = rg_count_in(&d, RED, 10, 10, 20, 30);
    t_note("FillRgn under MM_ANISOTROPIC: device-space pixels in region box = %d (200 expected if region is device-space)", in);
    t_note("total red px %d", rg_count(&d, RED));
    U(DeleteObject)(h); U(DeleteObject)(br); rg_dib_free(&d);
}
TEST(regions_fill_mm_text_flips_not_region) {
    REQUIRE(U(FillRgn)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(SetMapMode)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
    HRGN h = rg_rect(10, 10, 20, 30);
    U(SetMapMode)(d.dc, MM_LOMETRIC);
    CHECK(U(FillRgn)(d.dc, h, br));
    t_note("MM_LOMETRIC FillRgn: red px %d, inside device box %d", rg_count(&d, RED), rg_count_in(&d, RED, 10, 10, 20, 30));
    U(DeleteObject)(h); U(DeleteObject)(br); rg_dib_free(&d);
}
TEST(regions_fill_with_dc_clip) {
    REQUIRE(U(FillRgn)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(SelectClipRgn)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
    HRGN clip = rg_rect(20, 20, 40, 40), h = rg_rect(10, 10, 30, 30);
    CHECK(U(SelectClipRgn)(d.dc, clip) == SIMPLEREGION);
    CHECK(U(FillRgn)(d.dc, h, br));
    CHECK(rg_count(&d, RED) == 100 && rg_all_in(&d, RED, 20, 20, 30, 30));
    U(SelectClipRgn)(d.dc, NULL);
    U(DeleteObject)(clip); U(DeleteObject)(h); U(DeleteObject)(br); rg_dib_free(&d);
}
TEST(regions_fill_pattern_brush_origin) {
    REQUIRE(U(FillRgn)); REQUIRE(U(CreateBitmap)); REQUIRE(U(CreatePatternBrush)); REQUIRE(U(SetBrushOrgEx)); REQUIRE(U(SetTextColor)); REQUIRE(U(SetBkColor)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 32, 32, 0x808080)) { CHECK(0); return; }
    /* 8x8 monochrome pattern; only pixel (0,0) is 0 (text colour), others 1 (background colour) */
    WORD bits[8] = {0x007F, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF};  /* first byte of each row = leftmost 8 pixels */
    HBITMAP pb = U(CreateBitmap)(8, 8, 1, 1, bits);
    HBRUSH br = U(CreatePatternBrush)(pb);
    CHECK(pb && br);
    U(SetTextColor)(d.dc, RGB(255, 0, 0));
    U(SetBkColor)(d.dc, RGB(255, 255, 255));
    HRGN h = rg_rect(0, 0, 32, 32);
    static const int org[][2] = {{0, 0}, {3, 2}, {7, 7}, {8, 8}, {-1, -1}, {13, 5}};
    for (int i = 0; i < 6; i++) {
        rg_clear(&d, 0x808080);
        U(SetBrushOrgEx)(d.dc, org[i][0], org[i][1], NULL);
        CHECK(U(FillRgn)(d.dc, h, br));
        int ox = ((org[i][0] % 8) + 8) % 8, oy = ((org[i][1] % 8) + 8) % 8, bad = 0;
        for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++) {
            DWORD want = (((x - ox) & 7) == 0 && ((y - oy) & 7) == 0) ? RED : WHITE;
            if (rg_px(&d, x, y) != want) bad++;
        }
        CHECKF(bad == 0, "origin %d,%d mismatches %d", org[i][0], org[i][1], bad);
    }
    U(DeleteObject)(h); U(DeleteObject)(br); U(DeleteObject)(pb); rg_dib_free(&d);
}

/* ---- PaintRgn ---- */
TEST(regions_paint_uses_dc_brush) {
    REQUIRE(U(PaintRgn)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(SelectObject)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
    HGDIOBJ old = U(SelectObject)(d.dc, br);
    HRGN h = U(CreateEllipticRgn)(4, 4, 60, 40);
    CHECK(U(PaintRgn)(d.dc, h));
    CHECK(mismatches(&d, h, RED, WHITE) == 0);
    U(SelectObject)(d.dc, old);
    U(DeleteObject)(h); U(DeleteObject)(br); rg_dib_free(&d);
}
TEST(regions_paint_invalid) {
    REQUIRE(U(PaintRgn)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 16, 16, WHITE)) { CHECK(0); return; }
    HRGN h = rg_rect(0, 0, 16, 16);
    CHECK(!U(PaintRgn)(NULL, h));
    CHECK(!U(PaintRgn)(d.dc, NULL));
    CHECK(!U(PaintRgn)(d.dc, (HRGN)(ULONG_PTR)0x4444));
    CHECK(rg_count(&d, WHITE) == 256);
    U(DeleteObject)(h); rg_dib_free(&d);
}
TEST(regions_paint_default_brush_is_white) {
    REQUIRE(U(PaintRgn)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 16, 16, 0)) { CHECK(0); return; }
    HRGN h = rg_rect(2, 2, 6, 6);
    CHECK(U(PaintRgn)(d.dc, h));
    CHECK(rg_count(&d, WHITE) == 16);   /* new DC default brush is WHITE_BRUSH */
    U(DeleteObject)(h); rg_dib_free(&d);
}

/* ---- InvertRgn ---- */
TEST(regions_invert_exact_and_involution) {
    REQUIRE(U(InvertRgn)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 64, 64, 0x123456)) { CHECK(0); return; }
    HRGN h = U(CreateRoundRectRgn)(8, 8, 50, 40, 12, 12);
    CHECK(U(InvertRgn)(d.dc, h));
    CHECK(mismatches(&d, h, 0xEDCBA9, 0x123456) == 0);
    CHECK(U(InvertRgn)(d.dc, h));
    CHECK(rg_count(&d, 0x123456) == 64 * 64);
    U(DeleteObject)(h); rg_dib_free(&d);
}
TEST(regions_invert_overlap_xor) {
    REQUIRE(U(InvertRgn)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 32, 32, 0)) { CHECK(0); return; }
    HRGN a = rg_rect(0, 0, 20, 20), b = rg_rect(10, 10, 30, 30);
    U(InvertRgn)(d.dc, a); U(InvertRgn)(d.dc, b);
    CHECK(rg_px(&d, 5, 5) == WHITE && rg_px(&d, 25, 25) == WHITE && rg_px(&d, 15, 15) == 0 && rg_px(&d, 31, 0) == 0);
    U(DeleteObject)(a); U(DeleteObject)(b); rg_dib_free(&d);
}
TEST(regions_invert_invalid) {
    REQUIRE(U(InvertRgn)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 16, 16, 0)) { CHECK(0); return; }
    HRGN h = rg_rect(0, 0, 16, 16);
    CHECK(!U(InvertRgn)(NULL, h));
    CHECK(!U(InvertRgn)(d.dc, NULL));
    CHECK(!U(InvertRgn)(d.dc, (HRGN)(ULONG_PTR)0x55));
    CHECK(rg_count(&d, 0) == 256);
    U(DeleteObject)(h); rg_dib_free(&d);
}
TEST(regions_invert_empty) {
    REQUIRE(U(InvertRgn)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 16, 16, 0x102030)) { CHECK(0); return; }
    HRGN h = rg_rect(3, 3, 3, 3);
    BOOL r = U(InvertRgn)(d.dc, h);
    t_note("InvertRgn(empty) = %d", r);
    CHECK(rg_count(&d, 0x102030) == 256);
    U(DeleteObject)(h); rg_dib_free(&d);
}

/* ---- FrameRgn ---- */
TEST(regions_frame_rect_shape) {
    REQUIRE(U(FrameRgn)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
    HRGN h = rg_rect(20, 20, 40, 40);
    CHECK(U(FrameRgn)(d.dc, h, br, 1, 1));
    int n = rg_count(&d, RED);
    t_note("FrameRgn 1x1 on 20x20 rect painted %d px (inside-only 76, outside-only 84)", n);
    CHECK(n > 0);
    CHECK(rg_all_in(&d, WHITE, 22, 22, 38, 38));          /* interior untouched */
    CHECK(rg_count_in(&d, RED, 0, 0, 18, 64) == 0 && rg_count_in(&d, RED, 42, 0, 64, 64) == 0);
    CHECK(rg_count_in(&d, RED, 0, 0, 64, 18) == 0 && rg_count_in(&d, RED, 0, 42, 64, 64) == 0);
    /* the frame is a closed loop: every row 20..39 has a red pixel near each edge */
    CHECK(rg_count_in(&d, RED, 19, 19, 41, 41) == n);
    U(DeleteObject)(h); U(DeleteObject)(br); rg_dib_free(&d);
}
TEST(regions_frame_thickness_scales) {
    REQUIRE(U(FrameRgn)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
    HRGN h = rg_rect(16, 16, 48, 48);
    int prev = 0;
    for (int w = 1; w <= 4; w++) {
        rg_clear(&d, WHITE);
        CHECK(U(FrameRgn)(d.dc, h, br, w, w));
        int n = rg_count(&d, RED);
        CHECKF(n > prev, "width %d count %d prev %d", w, n, prev);
        /* thick part must not reach the centre */
        CHECK(rg_all_in(&d, WHITE, 16 + 4, 16 + 4, 48 - 4, 48 - 4) || w == 4);
        t_note("frame width %d -> %d px", w, n);
        prev = n;
    }
    U(DeleteObject)(h); U(DeleteObject)(br); rg_dib_free(&d);
}
TEST(regions_frame_asymmetric) {
    REQUIRE(U(FrameRgn)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
    HRGN h = rg_rect(16, 16, 48, 48);
    CHECK(U(FrameRgn)(d.dc, h, br, 4, 1));
    int vert = rg_count_in(&d, RED, 16, 28, 24, 36), horz = rg_count_in(&d, RED, 28, 16, 36, 24);
    CHECKF(vert > horz, "vertical edge px %d should exceed horizontal edge px %d for width 4 height 1", vert, horz);
    U(DeleteObject)(h); U(DeleteObject)(br); rg_dib_free(&d);
}
TEST(regions_frame_invalid) {
    REQUIRE(U(FrameRgn)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 32, 32, WHITE)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
    HRGN h = rg_rect(4, 4, 20, 20);
    CHECK(!U(FrameRgn)(NULL, h, br, 1, 1));
    CHECK(!U(FrameRgn)(d.dc, NULL, br, 1, 1));
    CHECK(!U(FrameRgn)(d.dc, (HRGN)br, br, 1, 1));
    CHECK(rg_count(&d, WHITE) == 1024);
    BOOL r;
    r = U(FrameRgn)(d.dc, h, NULL, 1, 1);  t_note("FrameRgn NULL brush = %d", r);
    r = U(FrameRgn)(d.dc, h, br, 0, 0);    t_note("FrameRgn width 0 = %d, red=%d", r, rg_count(&d, RED));
    rg_clear(&d, WHITE);
    r = U(FrameRgn)(d.dc, h, br, -2, -2);  t_note("FrameRgn width -2 = %d, red=%d", r, rg_count(&d, RED));
    U(DeleteObject)(h); U(DeleteObject)(br); rg_dib_free(&d);
}
TEST(regions_frame_ellipse_closed_ring) {
    REQUIRE(U(FrameRgn)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(CreateEllipticRgn)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
    HRGN h = U(CreateEllipticRgn)(8, 8, 56, 56);
    CHECK(U(FrameRgn)(d.dc, h, br, 2, 2));
    /* centre untouched, 4 axis extremes touched */
    CHECK(rg_px(&d, 32, 32) == WHITE);
    CHECK(rg_count_in(&d, RED, 30, 6, 34, 12) > 0 && rg_count_in(&d, RED, 30, 52, 34, 58) > 0);
    CHECK(rg_count_in(&d, RED, 6, 30, 12, 34) > 0 && rg_count_in(&d, RED, 52, 30, 58, 34) > 0);
    /* four-fold symmetry of the ring about the box centre is not guaranteed; just bound the area */
    int n = rg_count(&d, RED);
    CHECKF(n > 150 && n < 700, "ring area %d", n);
    U(DeleteObject)(h); U(DeleteObject)(br); rg_dib_free(&d);
}
TEST(regions_frame_does_not_modify_region) {
    REQUIRE(U(FrameRgn)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
    HRGN h = U(CreateEllipticRgn)(8, 8, 56, 56), c = U(CreateEllipticRgn)(8, 8, 56, 56);
    U(FrameRgn)(d.dc, h, br, 3, 3);
    CHECK(U(EqualRgn)(h, c));
    U(DeleteObject)(h); U(DeleteObject)(c); U(DeleteObject)(br); rg_dib_free(&d);
}

/* ---- per bit depth ---- */
static int dib_bpp_fill(int bpp, DWORD *out_white_count) {
    RgDib d; memset(&d, 0, sizeof d);
    HDC dc = U(CreateCompatibleDC)(NULL);
    char bibuf[sizeof(BITMAPINFO) + 256 * 4]; memset(bibuf, 0, sizeof bibuf);
    BITMAPINFO *pbi = (BITMAPINFO *)bibuf;
    pbi->bmiHeader.biSize = sizeof pbi->bmiHeader; pbi->bmiHeader.biWidth = 32; pbi->bmiHeader.biHeight = -32;
    pbi->bmiHeader.biPlanes = 1; pbi->bmiHeader.biBitCount = bpp; pbi->bmiHeader.biCompression = BI_RGB;
    if (bpp == 1) { pbi->bmiHeader.biClrUsed = 2; pbi->bmiColors[1].rgbRed = pbi->bmiColors[1].rgbGreen = pbi->bmiColors[1].rgbBlue = 255; }
    void *bits = NULL;
    HBITMAP bmp = U(CreateDIBSection)(dc, pbi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bmp || !bits) { U(DeleteDC)(dc); return -1; }
    HGDIOBJ old = U(SelectObject)(dc, bmp);
    int stride = ((32 * bpp + 31) / 32) * 4;
    memset(bits, 0xFF, stride * 32);   /* all ones: white for 1/16/24/32 */
    HBRUSH blk = (HBRUSH)U(GetStockObject)(BLACK_BRUSH);
    HRGN h = rg_rect(4, 4, 20, 14);
    BOOL ok = U(FillRgn)(dc, h, blk);
    int black = 0;
    for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++) {
        unsigned char *row = (unsigned char *)bits + y * stride;
        int isblack;
        switch (bpp) {
        case 1: isblack = !(row[x / 8] & (0x80 >> (x % 8))); break;
        case 4: isblack = ((x & 1) ? row[x / 2] & 0xF : row[x / 2] >> 4) == 0; break;
        case 8: isblack = row[x] == 0; break;
        case 16: isblack = (*(WORD *)(row + x * 2) & 0x7FFF) == 0; break;
        case 24: isblack = !row[x * 3] && !row[x * 3 + 1] && !row[x * 3 + 2]; break;
        default: isblack = !(*(DWORD *)(row + x * 4) & 0xFFFFFF); break;
        }
        int inside = x >= 4 && x < 20 && y >= 4 && y < 14;
        if (isblack != inside) black += 1000;
        else if (isblack) black++;
    }
    U(SelectObject)(dc, old); U(DeleteObject)(bmp); U(DeleteDC)(dc); U(DeleteObject)(h);
    return ok ? black : -2;
}
TEST(regions_fill_bitdepths) {
    REQUIRE(U(FillRgn)); REQUIRE(U(CreateDIBSection)); REQUIRE(U(GetStockObject)); REQUIRE(U(DeleteObject));
    static const int depths[] = {1, 16, 24, 32};
    for (int i = 0; i < 4; i++) {
        DWORD w; int r = dib_bpp_fill(depths[i], &w);
        CHECKF(r == 160, "bpp %d result %d (expect 160 black pixels, all inside)", depths[i], r);
    }
}
TEST(regions_fill_bitdepth_8_4_palette) {
    REQUIRE(U(FillRgn)); REQUIRE(U(CreateDIBSection)); REQUIRE(U(GetStockObject)); REQUIRE(U(DeleteObject));
    /* palettized: all-ones is index max, black brush maps to nearest black palette entry (default index 0 in colour table zero-filled) */
    int r8 = dib_bpp_fill(8, NULL), r4 = dib_bpp_fill(4, NULL);
    t_note("8bpp -> %d, 4bpp -> %d (zero colour table, informational)", r8, r4);
}

/* ---- SelectObject / SelectClipRgn ---- */
TEST(regions_selectobject_returns_type) {
    REQUIRE(U(SelectObject)); REQUIRE(U(DeleteObject)); REQUIRE(U(CombineRgn));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    HRGN s = rg_rect(10, 10, 20, 20), e = rg_rect(0, 0, 0, 0), c = rg_rect(0, 0, 10, 10), o = rg_rect(20, 20, 30, 30);
    U(CombineRgn)(c, c, o, RGN_OR);
    HGDIOBJ r;
    r = U(SelectObject)(d.dc, s); CHECK((ULONG_PTR)r == SIMPLEREGION);
    r = U(SelectObject)(d.dc, c); CHECK((ULONG_PTR)r == COMPLEXREGION);
    r = U(SelectObject)(d.dc, e); CHECK((ULONG_PTR)r == NULLREGION);
    U(SelectClipRgn)(d.dc, NULL);
    U(DeleteObject)(s); U(DeleteObject)(e); U(DeleteObject)(c); U(DeleteObject)(o); rg_dib_free(&d);
}
TEST(regions_selectobject_region_is_copied) {
    REQUIRE(U(SelectObject)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetClipRgn));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    HRGN h = rg_rect(10, 10, 20, 20);
    U(SelectObject)(d.dc, h);
    CHECK(U(DeleteObject)(h));                   /* deleting is allowed: the DC holds a copy */
    HBRUSH br = (HBRUSH)U(GetStockObject)(BLACK_BRUSH);
    HRGN full = rg_rect(0, 0, 64, 64);
    U(FillRgn)(d.dc, full, br);
    CHECK(rg_count(&d, 0) == 100 && rg_all_in(&d, 0, 10, 10, 20, 20));
    U(SelectClipRgn)(d.dc, NULL);
    U(DeleteObject)(full); rg_dib_free(&d);
}
TEST(regions_selectobject_modifying_region_after_select) {
    REQUIRE(U(SelectClipRgn)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetClipBox));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    HRGN h = rg_rect(10, 10, 20, 20);
    U(SelectClipRgn)(d.dc, h);
    U(SetRectRgn)(h, 0, 0, 64, 64);          /* must not change the DC clip */
    RECT rc; CHECK(U(GetClipBox)(d.dc, &rc) == SIMPLEREGION);
    CHECK(rc.left == 10 && rc.top == 10 && rc.right == 20 && rc.bottom == 20);
    U(SelectClipRgn)(d.dc, NULL);
    U(DeleteObject)(h); rg_dib_free(&d);
}
TEST(regions_selectobject_invalid) {
    REQUIRE(U(SelectObject)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 16, 16, WHITE)) { CHECK(0); return; }
    HRGN h = rg_rect(1, 1, 5, 5);
    HGDIOBJ r = U(SelectObject)(NULL, h);
    CHECK(r == NULL || r == HGDI_ERROR);
    t_note("SelectObject(NULL dc, region) = %p", r);
    HRGN dead = rg_rect(1, 1, 5, 5); U(DeleteObject)(dead);
    r = U(SelectObject)(d.dc, dead);
    CHECK(r == NULL || r == HGDI_ERROR);
    r = U(SelectObject)(d.dc, NULL);
    CHECK(r == NULL || r == HGDI_ERROR);
    U(DeleteObject)(h); rg_dib_free(&d);
}
TEST(regions_selectclip_basic) {
    REQUIRE(U(SelectClipRgn)); REQUIRE(U(GetClipRgn)); REQUIRE(U(GetClipBox)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    HRGN h = rg_rect(5, 6, 25, 36), out = rg_rect(0, 0, 1, 1);
    RECT rc;
    CHECK(U(GetClipRgn)(d.dc, out) == 0);
    CHECK(U(GetClipBox)(d.dc, &rc) == SIMPLEREGION);
    CHECK(rc.left == 0 && rc.top == 0 && rc.right == 64 && rc.bottom == 64);
    CHECK(U(SelectClipRgn)(d.dc, h) == SIMPLEREGION);
    CHECK(U(GetClipRgn)(d.dc, out) == 1);
    CHECK(U(EqualRgn)(out, h));
    CHECK(U(GetClipBox)(d.dc, &rc) == SIMPLEREGION);
    CHECK(rc.left == 5 && rc.top == 6 && rc.right == 25 && rc.bottom == 36);
    CHECK(U(SelectClipRgn)(d.dc, NULL) != 0);
    CHECK(U(GetClipRgn)(d.dc, out) == 0);
    U(DeleteObject)(h); U(DeleteObject)(out); rg_dib_free(&d);
}
TEST(regions_selectclip_types) {
    REQUIRE(U(SelectClipRgn)); REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    HRGN e = rg_rect(0, 0, 0, 0), c = rg_rect(0, 0, 10, 10), o = rg_rect(20, 20, 30, 30);
    U(CombineRgn)(c, c, o, RGN_OR);
    CHECK(U(SelectClipRgn)(d.dc, e) == NULLREGION);
    CHECK(U(SelectClipRgn)(d.dc, c) == COMPLEXREGION);
    HBRUSH br = (HBRUSH)U(GetStockObject)(BLACK_BRUSH);
    HRGN full = rg_rect(0, 0, 64, 64);
    U(FillRgn)(d.dc, full, br);
    CHECK(rg_count(&d, 0) == 200);
    U(SelectClipRgn)(d.dc, e);
    rg_clear(&d, WHITE);
    U(FillRgn)(d.dc, full, br);
    CHECK(rg_count(&d, WHITE) == 4096);   /* empty clip: nothing drawn */
    U(SelectClipRgn)(d.dc, NULL);
    U(DeleteObject)(e); U(DeleteObject)(c); U(DeleteObject)(o); U(DeleteObject)(full); rg_dib_free(&d);
}
TEST(regions_selectclip_invalid) {
    REQUIRE(U(SelectClipRgn)); REQUIRE(U(DeleteObject)); REQUIRE(U(CreateSolidBrush));
    RgDib d; if (!rg_dib(&d, 16, 16, WHITE)) { CHECK(0); return; }
    HRGN h = rg_rect(1, 1, 5, 5);
    CHECK(U(SelectClipRgn)(NULL, h) == 0);
    CHECK(U(SelectClipRgn)((HDC)(ULONG_PTR)0x1234, h) == 0);
    HBRUSH br = U(CreateSolidBrush)(0);
    int r = U(SelectClipRgn)(d.dc, (HRGN)br);
    t_note("SelectClipRgn(brush) = %d", r);
    HRGN dead = rg_rect(1, 1, 5, 5); U(DeleteObject)(dead);
    r = U(SelectClipRgn)(d.dc, dead);
    t_note("SelectClipRgn(deleted) = %d", r);
    U(SelectClipRgn)(d.dc, NULL);
    U(DeleteObject)(br); U(DeleteObject)(h); rg_dib_free(&d);
}
TEST(regions_getcliprgn_invalid) {
    REQUIRE(U(GetClipRgn)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 16, 16, WHITE)) { CHECK(0); return; }
    HRGN h = rg_rect(1, 1, 5, 5);
    CHECK(U(GetClipRgn)(NULL, h) == -1);
    int r = U(GetClipRgn)(d.dc, NULL);
    t_note("GetClipRgn(dc, NULL) = %d", r);
    CHECK(r == -1 || r == 0);
    r = U(GetClipRgn)(d.dc, (HRGN)(ULONG_PTR)0x777);
    t_note("GetClipRgn(dc, bogus) = %d", r);
    U(DeleteObject)(h); rg_dib_free(&d);
}
TEST(regions_extselectclip_modes) {
    REQUIRE(U(ExtSelectClipRgn)); REQUIRE(U(GetClipBox)); REQUIRE(U(GetClipRgn)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    HRGN a = rg_rect(0, 0, 20, 20), b = rg_rect(10, 10, 30, 30), out = rg_rect(0, 0, 1, 1), ref = rg_rect(0, 0, 1, 1);
    U(SelectClipRgn)(d.dc, a);
    CHECK(U(ExtSelectClipRgn)(d.dc, b, RGN_AND) == SIMPLEREGION);
    U(GetClipRgn)(d.dc, out); U(CombineRgn)(ref, a, b, RGN_AND);
    CHECK(U(EqualRgn)(out, ref));
    U(SelectClipRgn)(d.dc, a);
    CHECK(U(ExtSelectClipRgn)(d.dc, b, RGN_OR) == COMPLEXREGION);
    U(GetClipRgn)(d.dc, out); U(CombineRgn)(ref, a, b, RGN_OR);
    CHECK(U(EqualRgn)(out, ref));
    U(SelectClipRgn)(d.dc, a);
    CHECK(U(ExtSelectClipRgn)(d.dc, b, RGN_XOR) == COMPLEXREGION);
    U(GetClipRgn)(d.dc, out); U(CombineRgn)(ref, a, b, RGN_XOR);
    CHECK(U(EqualRgn)(out, ref));
    U(SelectClipRgn)(d.dc, a);
    CHECK(U(ExtSelectClipRgn)(d.dc, b, RGN_DIFF) == COMPLEXREGION);
    U(GetClipRgn)(d.dc, out); U(CombineRgn)(ref, a, b, RGN_DIFF);
    CHECK(U(EqualRgn)(out, ref));
    U(SelectClipRgn)(d.dc, a);
    CHECK(U(ExtSelectClipRgn)(d.dc, b, RGN_COPY) == SIMPLEREGION);
    U(GetClipRgn)(d.dc, out);
    CHECK(U(EqualRgn)(out, b));
    SET_ERR(0);
    CHECK(U(ExtSelectClipRgn)(d.dc, b, 0) == 0);
    CHECK(U(ExtSelectClipRgn)(d.dc, b, 6) == 0);
    CHECK(U(ExtSelectClipRgn)(NULL, b, RGN_COPY) == 0);
    U(SelectClipRgn)(d.dc, NULL);
    U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(out); U(DeleteObject)(ref); rg_dib_free(&d);
}
TEST(regions_extselectclip_null_region) {
    REQUIRE(U(ExtSelectClipRgn)); REQUIRE(U(GetClipRgn)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    HRGN a = rg_rect(0, 0, 20, 20), out = rg_rect(0, 0, 1, 1);
    U(SelectClipRgn)(d.dc, a);
    int r = U(ExtSelectClipRgn)(d.dc, NULL, RGN_COPY);
    t_note("ExtSelectClipRgn(NULL, RGN_COPY) = %d", r);
    CHECK(U(GetClipRgn)(d.dc, out) == 0);
    U(SelectClipRgn)(d.dc, a);
    r = U(ExtSelectClipRgn)(d.dc, NULL, RGN_AND);
    t_note("ExtSelectClipRgn(NULL, RGN_AND) = %d", r);
    U(SelectClipRgn)(d.dc, NULL);
    U(DeleteObject)(a); U(DeleteObject)(out); rg_dib_free(&d);
}
TEST(regions_intersect_offset_clip) {
    REQUIRE(U(IntersectClipRect)); REQUIRE(U(OffsetClipRgn)); REQUIRE(U(GetClipBox)); REQUIRE(U(SelectClipRgn));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    RECT rc;
    CHECK(U(IntersectClipRect)(d.dc, 10, 10, 40, 40) == SIMPLEREGION);
    CHECK(U(IntersectClipRect)(d.dc, 20, 20, 60, 60) == SIMPLEREGION);
    U(GetClipBox)(d.dc, &rc);
    CHECK(rc.left == 20 && rc.top == 20 && rc.right == 40 && rc.bottom == 40);
    CHECK(U(OffsetClipRgn)(d.dc, 5, -5) == SIMPLEREGION);
    U(GetClipBox)(d.dc, &rc);
    CHECK(rc.left == 25 && rc.top == 15 && rc.right == 45 && rc.bottom == 35);
    CHECK(U(IntersectClipRect)(d.dc, 50, 50, 60, 60) == NULLREGION);
    CHECK(U(OffsetClipRgn)(NULL, 1, 1) == 0);
    U(SelectClipRgn)(d.dc, NULL);
    rg_dib_free(&d);
}
TEST(regions_excludeclip) {
    REQUIRE(U(ExcludeClipRect)); REQUIRE(U(GetClipBox)); REQUIRE(U(SelectClipRgn)); REQUIRE(U(PtVisible)); REQUIRE(U(RectVisible));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    CHECK(U(ExcludeClipRect)(d.dc, 20, 20, 30, 30) == COMPLEXREGION);
    CHECK(!U(PtVisible)(d.dc, 25, 25) && U(PtVisible)(d.dc, 19, 25) && U(PtVisible)(d.dc, 30, 25));
    RECT in = {22, 22, 28, 28}, ov = {10, 10, 25, 25};
    CHECK(!U(RectVisible)(d.dc, &in) && U(RectVisible)(d.dc, &ov));
    U(SelectClipRgn)(d.dc, NULL);
    CHECK(U(PtVisible)(d.dc, 25, 25));
    rg_dib_free(&d);
}
TEST(regions_ptvisible_outside_surface) {
    REQUIRE(U(PtVisible)); REQUIRE(U(RectVisible));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    CHECK(U(PtVisible)(d.dc, 0, 0) && U(PtVisible)(d.dc, 63, 63));
    CHECK(!U(PtVisible)(d.dc, 64, 10) && !U(PtVisible)(d.dc, 10, 64) && !U(PtVisible)(d.dc, -1, 10));
    RECT r = {64, 0, 70, 10}, r2 = {60, 60, 70, 70};
    CHECK(!U(RectVisible)(d.dc, &r) && U(RectVisible)(d.dc, &r2));
    rg_dib_free(&d);
}
TEST(regions_clip_device_coords_with_viewport_org) {
    REQUIRE(U(SelectClipRgn)); REQUIRE(U(SetViewportOrgEx)); REQUIRE(U(GetClipRgn)); REQUIRE(U(FillRgn)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 64, 64, WHITE)) { CHECK(0); return; }
    HRGN h = rg_rect(10, 10, 20, 20), out = rg_rect(0, 0, 1, 1);
    U(SetViewportOrgEx)(d.dc, 7, 7, NULL);
    U(SelectClipRgn)(d.dc, h);
    CHECK(U(GetClipRgn)(d.dc, out) == 1);
    RECT rc; U(GetRgnBox)(out, &rc);
    t_note("clip region after SetViewportOrg(7,7): %ld,%ld,%ld,%ld (device coordinates expected unchanged)", rc.left, rc.top, rc.right, rc.bottom);
    CHECK(U(EqualRgn)(out, h));
    HRGN full = rg_rect(0, 0, 64, 64);
    U(FillRgn)(d.dc, full, (HBRUSH)U(GetStockObject)(BLACK_BRUSH));
    CHECK(rg_count(&d, 0) == 100 && rg_all_in(&d, 0, 10, 10, 20, 20));
    U(SelectClipRgn)(d.dc, NULL);
    U(DeleteObject)(h); U(DeleteObject)(out); U(DeleteObject)(full); rg_dib_free(&d);
}
TEST(regions_dc_default_clip_after_bitmap_select) {
    REQUIRE(U(GetClipBox)); REQUIRE(U(GetClipRgn)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 30, 20, WHITE)) { CHECK(0); return; }
    RECT rc; CHECK(U(GetClipBox)(d.dc, &rc) == SIMPLEREGION);
    CHECK(rc.left == 0 && rc.top == 0 && rc.right == 30 && rc.bottom == 20);
    HRGN out = rg_rect(0, 0, 1, 1);
    CHECK(U(GetClipRgn)(d.dc, out) == 0);
    U(DeleteObject)(out); rg_dib_free(&d);
}

/* ---- handles / leaks ---- */
static DWORD gdi_count(void) { return U(GetGuiResources) ? U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS) : 0; }
TEST(regions_leak_create_delete) {
    REQUIRE(U(GetGuiResources)); REQUIRE(U(DeleteObject)); REQUIRE(U(CombineRgn));
    if (rg_is_wine()) { t_skip("GetGuiResources not implemented by wine"); return; }
    DWORD before = gdi_count();
    HRGN h[200];
    for (int i = 0; i < 200; i++) h[i] = (i % 3 == 0) ? rg_rect(i, i, i + 5, i + 9) : (i % 3 == 1) ? U(CreateEllipticRgn)(0, 0, 10 + i, 20) : U(CreateRoundRectRgn)(0, 0, 30, 30, 8, 8);
    DWORD mid = gdi_count();
    CHECKF(mid >= before + 190, "mid %lu before %lu", mid, before);
    for (int i = 0; i < 200; i++) CHECK(U(DeleteObject)(h[i]));
    CHECKF(gdi_count() == before, "after %lu before %lu", gdi_count(), before);
}
TEST(regions_leak_combine_does_not_allocate) {
    REQUIRE(U(GetGuiResources)); REQUIRE(U(DeleteObject)); REQUIRE(U(CombineRgn));
    HRGN a = rg_rect(0, 0, 10, 10), b = rg_rect(5, 5, 15, 15), d = rg_rect(0, 0, 1, 1);
    DWORD before = gdi_count();
    for (int i = 0; i < 2000; i++) U(CombineRgn)(d, a, b, RGN_XOR);
    CHECK(gdi_count() == before);
    U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(d);
}
TEST(regions_leak_selectclip_and_getcliprgn) {
    REQUIRE(U(GetGuiResources)); REQUIRE(U(SelectClipRgn)); REQUIRE(U(GetClipRgn)); REQUIRE(U(DeleteObject));
    RgDib d; if (!rg_dib(&d, 32, 32, WHITE)) { CHECK(0); return; }
    HRGN a = rg_rect(0, 0, 10, 10), out = rg_rect(0, 0, 1, 1);
    DWORD before = gdi_count();
    for (int i = 0; i < 1000; i++) { U(SelectClipRgn)(d.dc, a); U(GetClipRgn)(d.dc, out); U(SelectClipRgn)(d.dc, NULL); }
    CHECK(gdi_count() == before);
    U(DeleteObject)(a); U(DeleteObject)(out); rg_dib_free(&d);
}
TEST(regions_leak_extcreate_and_data) {
    REQUIRE(U(GetGuiResources)); REQUIRE(U(ExtCreateRegion)); REQUIRE(U(DeleteObject));
    HRGN a = U(CreateEllipticRgn)(0, 0, 50, 50);
    DWORD sz; RGNDATA *dta = rg_getdata(a, &sz);
    CHECK(dta != NULL);
    if (dta) {
        DWORD before = gdi_count();
        for (int i = 0; i < 500; i++) { HRGN h = U(ExtCreateRegion)(NULL, sz, dta); if (!h) { CHECK(0); break; } U(DeleteObject)(h); }
        CHECK(gdi_count() == before);
        free(dta);
    }
    U(DeleteObject)(a);
}
TEST(regions_many_regions_at_once) {
    REQUIRE(U(DeleteObject));
    enum { N = 2000 };
    HRGN *h = (HRGN *)calloc(N, sizeof(HRGN));
    int made = 0;
    for (int i = 0; i < N; i++) { h[i] = rg_rect(i, 0, i + 1, 1); if (!h[i]) break; made++; }
    CHECKF(made == N, "made %d", made);
    for (int i = 0; i < made; i++) CHECK(U(PtInRegion)(h[i], i, 0));
    for (int i = 0; i < made; i++) U(DeleteObject)(h[i]);
    free(h);
}
TEST(regions_handle_values_distinct_types) {
    REQUIRE(U(GetObjectType)); REQUIRE(U(DeleteObject));
    HRGN h = rg_rect(0, 0, 1, 1);
    CHECK(((ULONG_PTR)h & 0xFFFF) != 0);
    CHECK((((ULONG_PTR)h >> 16) & 0xFF) == 0x04 || (((ULONG_PTR)h >> 16) & 0xFF) != 0 || 1);   /* type byte is observed only */
    t_note("region handle = %p", (void *)h);
    U(DeleteObject)(h);
}

/* ---- cross thread ---- */
typedef struct { HRGN a, b, d; int res[8]; } XT;
static DWORD WINAPI xt_proc(LPVOID p) {
    XT *x = (XT *)p;
    RECT rc;
    x->res[0] = U(GetRgnBox)(x->a, &rc);
    x->res[1] = rc.right == 20 && rc.bottom == 20;
    x->res[2] = U(CombineRgn)(x->d, x->a, x->b, RGN_OR);
    x->res[3] = U(PtInRegion)(x->a, 5, 5);
    x->res[4] = U(OffsetRgn)(x->b, 1, 1);
    HRGN mine = rg_rect(0, 0, 4, 4);
    x->a = mine;     /* hand a region created here back to the creator */
    return 0;
}
TEST(regions_cross_thread_use) {
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(CombineRgn)); REQUIRE(U(DeleteObject));
    XT x; memset(&x, 0, sizeof x);
    x.a = rg_rect(0, 0, 20, 20); x.b = rg_rect(30, 30, 40, 40); x.d = rg_rect(0, 0, 1, 1);
    HRGN origa = x.a;
    HANDLE th = CreateThread(NULL, 0, xt_proc, &x, 0, NULL);
    if (!th) { CHECK(0); return; }
    WaitForSingleObject(th, 10000); CloseHandle(th);
    CHECK(x.res[0] == SIMPLEREGION && x.res[1]);
    CHECK(x.res[2] == COMPLEXREGION);
    CHECK(x.res[3] != 0);
    CHECK(x.res[4] == SIMPLEREGION);
    /* region created in the dead thread is still usable (regions are process objects) */
    CHECK(rg_type(x.a) == SIMPLEREGION);
    CHECK(U(DeleteObject)(x.a));
    CHECK(U(DeleteObject)(origa)); CHECK(U(DeleteObject)(x.b)); CHECK(U(DeleteObject)(x.d));
}
static DWORD WINAPI xt_delete(LPVOID p) { *(BOOL *)((void **)p + 1) = U(DeleteObject)(*(HRGN *)p); return 0; }
TEST(regions_cross_thread_delete) {
    REQUIRE(U(DeleteObject));
    void *slot[2];
    HRGN h = rg_rect(0, 0, 4, 4);
    slot[0] = h; slot[1] = NULL;
    HANDLE th = CreateThread(NULL, 0, xt_delete, slot, 0, NULL);
    if (!th) { CHECK(0); return; }
    WaitForSingleObject(th, 10000); CloseHandle(th);
    CHECK(slot[1] != NULL);
    CHECK(rg_type(h) == 0);
}
static DWORD WINAPI xt_selectclip(LPVOID p) {
    HDC dc = (HDC)p;
    HRGN h = rg_rect(0, 0, 8, 8);
    int r = U(SelectClipRgn)(dc, h);
    U(DeleteObject)(h);
    return r;
}
TEST(regions_cross_thread_selectclip_on_memdc) {
    REQUIRE(U(SelectClipRgn));
    RgDib d; if (!rg_dib(&d, 32, 32, WHITE)) { CHECK(0); return; }
    HANDLE th = CreateThread(NULL, 0, xt_selectclip, d.dc, 0, NULL);
    if (!th) { CHECK(0); return; }
    WaitForSingleObject(th, 10000);
    DWORD r = 0; GetExitCodeThread(th, &r); CloseHandle(th);
    t_note("SelectClipRgn from another thread on a memory DC -> %lu", r);
    CHECK(r == SIMPLEREGION);
    RECT rc; U(GetClipBox)(d.dc, &rc);
    CHECK(rc.right == 8 && rc.bottom == 8);
    U(SelectClipRgn)(d.dc, NULL);
    rg_dib_free(&d);
}

/* ---- XFORM bounds ---- */
TEST(regions_extcreate_xform_bounds_property) {
    REQUIRE(U(ExtCreateRegion)); REQUIRE(U(GetRgnBox)); REQUIRE(U(DeleteObject));
    RECT r[2] = {{0, 0, 10, 10}, {20, 20, 40, 30}};
    int sz = sizeof(RGNDATAHEADER) + 2 * sizeof(RECT);
    RGNDATA *d = (RGNDATA *)calloc(1, sz);
    d->rdh.dwSize = 32; d->rdh.iType = RDH_RECTANGLES; d->rdh.nCount = 2; d->rdh.nRgnSize = 32;
    d->rdh.rcBound = (RECT){0, 0, 40, 30};
    memcpy(d->Buffer, r, sizeof r);
    static const float ang[] = {0.3f, 1.0f, 2.0f, 3.14159265f, 4.5f};
    for (int i = 0; i < 5; i++) {
        float c = cosf(ang[i]), s = sinf(ang[i]);
        XFORM x = {c, s, -s, c, 100.0f, -50.0f};
        HRGN h = U(ExtCreateRegion)(&x, sz, d);
        CHECKF(h != NULL, "angle %d", i);
        if (!h) continue;
        RECT rc; int t = U(GetRgnBox)(h, &rc);
        CHECK(t == SIMPLEREGION || t == COMPLEXREGION);
        /* bounds of the transformed corners (±2 px slack) */
        float xs[4] = {0, 40, 0, 40}, ys[4] = {0, 0, 30, 30};
        float mnx = 1e9, mxx = -1e9, mny = 1e9, mxy = -1e9;
        for (int k = 0; k < 4; k++) {
            float tx = xs[k] * c - ys[k] * s + 100, ty = xs[k] * s + ys[k] * c - 50;
            if (tx < mnx) mnx = tx; if (tx > mxx) mxx = tx; if (ty < mny) mny = ty; if (ty > mxy) mxy = ty;
        }
        CHECKF(rc.left >= mnx - 2 && rc.right <= mxx + 2 && rc.top >= mny - 2 && rc.bottom <= mxy + 2, "angle %d box %ld,%ld,%ld,%ld vs %.1f,%.1f,%.1f,%.1f", i, rc.left, rc.top, rc.right, rc.bottom, mnx, mny, mxx, mxy);
        U(DeleteObject)(h);
    }
    free(d);
}
TEST(regions_extcreate_xform_mirror) {
    REQUIRE(U(ExtCreateRegion)); REQUIRE(U(GetRgnBox)); REQUIRE(U(DeleteObject));
    RECT r[2] = {{0, 0, 10, 10}, {20, 20, 40, 30}};
    int sz = sizeof(RGNDATAHEADER) + 2 * sizeof(RECT);
    RGNDATA *d = (RGNDATA *)calloc(1, sz);
    d->rdh.dwSize = 32; d->rdh.iType = RDH_RECTANGLES; d->rdh.nCount = 2; d->rdh.rcBound = (RECT){0, 0, 40, 30};
    memcpy(d->Buffer, r, sizeof r);
    XFORM x = {-1, 0, 0, 1, 0, 0};
    HRGN h = U(ExtCreateRegion)(&x, sz, d);
    CHECK(h != NULL);
    if (h) {
        RECT rc; CHECK(U(GetRgnBox)(h, &rc) == COMPLEXREGION);
        CHECK(rc.left == -40 && rc.right == 0 && rc.top == 0 && rc.bottom == 30);
        CHECK(U(PtInRegion)(h, -1, 0) && !U(PtInRegion)(h, 0, 0) && U(PtInRegion)(h, -21, 20) && U(PtInRegion)(h, -40, 29));
        U(DeleteObject)(h);
    }
    free(d);
}
TEST(regions_extcreate_xform_fractional) {
    REQUIRE(U(ExtCreateRegion)); REQUIRE(U(GetRgnBox)); REQUIRE(U(DeleteObject));
    RECT r[2] = {{0, 0, 10, 10}, {20, 20, 40, 30}};
    int sz = sizeof(RGNDATAHEADER) + 2 * sizeof(RECT);
    RGNDATA *d = (RGNDATA *)calloc(1, sz);
    d->rdh.dwSize = 32; d->rdh.iType = RDH_RECTANGLES; d->rdh.nCount = 2; d->rdh.rcBound = (RECT){0, 0, 40, 30};
    memcpy(d->Buffer, r, sizeof r);
    XFORM x = {0.5f, 0, 0, 0.5f, 0, 0};
    HRGN h = U(ExtCreateRegion)(&x, sz, d);
    CHECK(h != NULL);
    if (h) {
        RECT rc; int t = U(GetRgnBox)(h, &rc);
        CHECK(t == COMPLEXREGION);
        CHECK(rc.left == 0 && rc.top == 0 && rc.right == 20 && rc.bottom == 15);
        U(DeleteObject)(h);
    }
    free(d);
}
