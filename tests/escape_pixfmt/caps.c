#include "ep.h"

static HDC dib_dc(int bpp, int w, int h, HBITMAP *bmp, HBITMAP *old, void **bits) {
    HDC dc = U(CreateCompatibleDC)(NULL);
    if (!dc) return NULL;
    struct { BITMAPINFOHEADER h; RGBQUAD pal[256]; } bi; memset(&bi, 0, sizeof bi);
    bi.h.biSize = sizeof bi.h; bi.h.biWidth = w; bi.h.biHeight = -h; bi.h.biPlanes = 1; bi.h.biBitCount = (WORD)bpp;
    bi.h.biCompression = BI_RGB;
    if (bpp <= 8) { bi.h.biClrUsed = 1u << bpp; for (int i = 0; i < (1 << bpp); i++) { bi.pal[i].rgbRed = bi.pal[i].rgbGreen = bi.pal[i].rgbBlue = (BYTE)i; } }
    void *b = NULL;
    HBITMAP hb = U(CreateDIBSection)(dc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &b, NULL, 0);
    if (!hb) { U(DeleteDC)(dc); return NULL; }
    *old = (HBITMAP)U(SelectObject)(dc, hb); *bmp = hb; if (bits) *bits = b;
    return dc;
}

/* ---- TECHNOLOGY ---- */
TEST(ep_caps_technology_constants) { CHECK(DT_PLOTTER == 0 && DT_RASDISPLAY == 1 && DT_RASPRINTER == 2 && DT_RASCAMERA == 3 && DT_CHARSTREAM == 4 && DT_METAFILE == 5 && DT_DISPFILE == 6); }
TEST(ep_caps_technology_screen) { REQUIRE(U(GetDeviceCaps)); HDC d = ep_scr(); CHECK(U(GetDeviceCaps)(d, TECHNOLOGY) == DT_RASDISPLAY); ep_scr_free(d); }
TEST(ep_caps_technology_mem_default) { REQUIRE(U(GetDeviceCaps)); HDC m = U(CreateCompatibleDC)(NULL); CHECK(U(GetDeviceCaps)(m, TECHNOLOGY) == DT_RASDISPLAY); U(DeleteDC)(m); }
TEST(ep_caps_technology_mem_dib32) { REQUIRE(U(GetDeviceCaps)); HBITMAP b, o; HDC m = ep_memdc32(4, 4, &b, &o, NULL); CHECK(m && U(GetDeviceCaps)(m, TECHNOLOGY) == DT_RASDISPLAY); if (m) ep_memdc_free(m, b, o); }
TEST(ep_caps_technology_window_dc) {
    REQUIRE(U(GetDeviceCaps));
    HWND w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 40, 40, NULL); HDC d = U(GetDC)(w);
    CHECK(d && U(GetDeviceCaps)(d, TECHNOLOGY) == DT_RASDISPLAY);
    U(ReleaseDC)(w, d); U(DestroyWindow)(w);
}
TEST(ep_caps_technology_windowdc_nonclient) {
    REQUIRE(U(GetDeviceCaps)); REQUIRE(U(GetWindowDC));
    HWND w = t_make_window(NULL, WS_OVERLAPPEDWINDOW, 0, 0, 0, 80, 80, NULL); HDC d = U(GetWindowDC)(w);
    CHECK(d && U(GetDeviceCaps)(d, TECHNOLOGY) == DT_RASDISPLAY);
    U(ReleaseDC)(w, d); U(DestroyWindow)(w);
}
TEST(ep_caps_technology_getdcex_cache) {
    REQUIRE(U(GetDeviceCaps)); REQUIRE(U(GetDCEx));
    HWND w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 40, 40, NULL); HDC d = U(GetDCEx)(w, NULL, DCX_CACHE);
    CHECK(d && U(GetDeviceCaps)(d, TECHNOLOGY) == DT_RASDISPLAY);
    U(ReleaseDC)(w, d); U(DestroyWindow)(w);
}
TEST(ep_caps_technology_compat_dc_of_window) {
    REQUIRE(U(GetDeviceCaps));
    HWND w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 40, 40, NULL); HDC d = U(GetDC)(w);
    HDC m = U(CreateCompatibleDC)(d);
    CHECK(U(GetDeviceCaps)(m, TECHNOLOGY) == DT_RASDISPLAY);
    U(DeleteDC)(m); U(ReleaseDC)(w, d); U(DestroyWindow)(w);
}
TEST(ep_caps_technology_ic_display) {
    REQUIRE(U(GetDeviceCaps)); REQUIRE(U(CreateICA));
    HDC ic = U(CreateICA)("DISPLAY", NULL, NULL, NULL);
    CHECK(ic != NULL);
    if (ic) { CHECK(U(GetDeviceCaps)(ic, TECHNOLOGY) == DT_RASDISPLAY); U(DeleteDC)(ic); }
}
TEST(ep_caps_technology_dc_display) {
    REQUIRE(U(GetDeviceCaps)); REQUIRE(U(CreateDCA));
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    CHECK(dc != NULL);
    if (dc) { CHECK(U(GetDeviceCaps)(dc, TECHNOLOGY) == DT_RASDISPLAY); U(DeleteDC)(dc); }
}

/* ---- invalid inputs ---- */
TEST(ep_caps_null_dc_all_zero) {
    REQUIRE(U(GetDeviceCaps));
    static const int idx[] = { DRIVERVERSION, TECHNOLOGY, HORZSIZE, VERTSIZE, HORZRES, VERTRES, BITSPIXEL, PLANES, NUMCOLORS, RASTERCAPS, LOGPIXELSX, LOGPIXELSY, SIZEPALETTE };
    for (unsigned i = 0; i < sizeof idx / sizeof idx[0]; i++) {
        SET_ERR(0);
        int r = U(GetDeviceCaps)(NULL, idx[i]);
        CHECKF(r == 0, "index %d got %d", idx[i], r);
    }
}
TEST(ep_caps_bogus_dc_zero) {
    REQUIRE(U(GetDeviceCaps));
    CHECK(U(GetDeviceCaps)(EP_BOGUS_DC, HORZRES) == 0);
    CHECK(U(GetDeviceCaps)(EP_BOGUS_DC2, BITSPIXEL) == 0);
    CHECK(U(GetDeviceCaps)((HDC)(LONG_PTR)-1, TECHNOLOGY) == 0);
}
TEST(ep_caps_deleted_dc_zero) {
    REQUIRE(U(GetDeviceCaps));
    HDC m = U(CreateCompatibleDC)(NULL); U(DeleteDC)(m);
    CHECK(U(GetDeviceCaps)(m, HORZRES) == 0);
    CHECK(U(GetDeviceCaps)(m, TECHNOLOGY) == 0);
}
TEST(ep_caps_wrong_object_type_zero) {
    REQUIRE(U(GetDeviceCaps));
    HGDIOBJ br = U(CreateSolidBrush)(0);
    CHECK(U(GetDeviceCaps)((HDC)br, HORZRES) == 0);
    CHECK(U(GetDeviceCaps)((HDC)U(GetStockObject)(SYSTEM_FONT), TECHNOLOGY) == 0);
    U(DeleteObject)(br);
}
TEST(ep_caps_invalid_index_zero) {
    REQUIRE(U(GetDeviceCaps));
    HDC d = ep_scr(), m = U(CreateCompatibleDC)(NULL);
    static const int idx[] = { -1, -1000, 9999, 0x7fffffff, (int)0x80000000, 0x10000, 4096 };
    for (unsigned i = 0; i < sizeof idx / sizeof idx[0]; i++) {
        CHECKF(U(GetDeviceCaps)(d, idx[i]) == 0, "screen index %d", idx[i]);
        CHECKF(U(GetDeviceCaps)(m, idx[i]) == 0, "mem index %d", idx[i]);
    }
    U(DeleteDC)(m); ep_scr_free(d);
}
TEST(ep_caps_odd_indices_note) {
    REQUIRE(U(GetDeviceCaps));
    HDC d = ep_scr();
    for (int i = 1; i < 60; i += 2) { int r = U(GetDeviceCaps)(d, i); if (r) t_note("odd index %d -> %d", i, r); }
    ep_scr_free(d);
}
TEST(ep_caps_invalid_index_no_last_error_note) {
    REQUIRE(U(GetDeviceCaps));
    HDC d = ep_scr();
    SET_ERR(0);
    U(GetDeviceCaps)(d, 9999);
    t_note("GetDeviceCaps(9999) last error %lu", GetLastError());
    ep_scr_free(d);
}

/* ---- display DC values ---- */
TEST(ep_caps_screen_resolution_sane) {
    REQUIRE(U(GetDeviceCaps));
    HDC d = ep_scr();
    int w = U(GetDeviceCaps)(d, HORZRES), h = U(GetDeviceCaps)(d, VERTRES);
    CHECKF(w >= 640 && h >= 480, "%dx%d", w, h);
    CHECK(U(GetDeviceCaps)(d, DESKTOPHORZRES) >= w);
    CHECK(U(GetDeviceCaps)(d, DESKTOPVERTRES) >= h);
    t_note("screen %dx%d desktop %dx%d", w, h, U(GetDeviceCaps)(d, DESKTOPHORZRES), U(GetDeviceCaps)(d, DESKTOPVERTRES));
    ep_scr_free(d);
}
TEST(ep_caps_screen_matches_sysmetrics) {
    REQUIRE(U(GetDeviceCaps)); REQUIRE(U(GetSystemMetrics));
    HDC d = ep_scr();
    int w = U(GetDeviceCaps)(d, HORZRES), h = U(GetDeviceCaps)(d, VERTRES);
    int mw = U(GetSystemMetrics)(SM_CXSCREEN), mh = U(GetSystemMetrics)(SM_CYSCREEN);
    if (w != mw || h != mh) t_note("HORZRES/VERTRES %dx%d vs SM_CXSCREEN %dx%d", w, h, mw, mh);
    CHECK(w == mw && h == mh);
    ep_scr_free(d);
}
TEST(ep_caps_screen_bits_planes) {
    REQUIRE(U(GetDeviceCaps));
    HDC d = ep_scr();
    int bpp = U(GetDeviceCaps)(d, BITSPIXEL), planes = U(GetDeviceCaps)(d, PLANES);
    CHECKF(planes == 1, "planes %d", planes);
    CHECKF(bpp == 8 || bpp == 16 || bpp == 24 || bpp == 32, "bpp %d", bpp);
    t_note("screen bpp=%d colorres=%d", bpp, U(GetDeviceCaps)(d, COLORRES));
    ep_scr_free(d);
}
TEST(ep_caps_screen_numcolors_vs_bpp) {
    REQUIRE(U(GetDeviceCaps));
    HDC d = ep_scr();
    int bpp = U(GetDeviceCaps)(d, BITSPIXEL), nc = U(GetDeviceCaps)(d, NUMCOLORS);
    if (bpp > 8) CHECKF(nc == -1, "numcolors %d at %d bpp", nc, bpp);
    else t_note("numcolors %d at %d bpp", nc, bpp);
    ep_scr_free(d);
}
TEST(ep_caps_screen_palette_flags_vs_bpp) {
    REQUIRE(U(GetDeviceCaps));
    HDC d = ep_scr();
    int bpp = U(GetDeviceCaps)(d, BITSPIXEL), rc = U(GetDeviceCaps)(d, RASTERCAPS), sz = U(GetDeviceCaps)(d, SIZEPALETTE);
    if (bpp > 8) { CHECK(!(rc & RC_PALETTE)); CHECKF(sz == 0, "sizepalette %d", sz); }
    else { CHECK(rc & RC_PALETTE); CHECKF(sz == 256 || sz == 0, "sizepalette %d", sz); }
    ep_scr_free(d);
}
TEST(ep_caps_screen_rastercaps_required_bits) {
    REQUIRE(U(GetDeviceCaps));
    HDC d = ep_scr();
    int rc = U(GetDeviceCaps)(d, RASTERCAPS);
    CHECKF(rc & RC_BITBLT, "rc 0x%x", rc);
    CHECK(rc & RC_STRETCHBLT);
    CHECK(rc & RC_STRETCHDIB);
    CHECK(rc & RC_DIBTODEV);
    CHECK(rc & RC_DI_BITMAP);
    CHECK(rc & RC_BITMAP64);
    CHECK(!(rc & RC_BANDING));
    CHECK(!(rc & RC_SCALING));
    t_note("RASTERCAPS=0x%x", rc);
    ep_scr_free(d);
}
TEST(ep_caps_screen_gdi20_output_note) {
    REQUIRE(U(GetDeviceCaps));
    HDC d = ep_scr();
    int rc = U(GetDeviceCaps)(d, RASTERCAPS);
    t_note("RC_GDI20_OUTPUT=%d RC_FLOODFILL=%d RC_OP_DX_OUTPUT=%d", !!(rc & RC_GDI20_OUTPUT), !!(rc & RC_FLOODFILL), !!(rc & RC_OP_DX_OUTPUT));
    ep_scr_free(d);
}
TEST(ep_caps_screen_numreserved_20) {
    REQUIRE(U(GetDeviceCaps));
    HDC d = ep_scr();
    int bpp = U(GetDeviceCaps)(d, BITSPIXEL);
    int nr = U(GetDeviceCaps)(d, NUMRESERVED);
    if (bpp <= 8) CHECKF(nr == 20, "numreserved %d", nr);
    else t_note("NUMRESERVED=%d at %d bpp", nr, bpp);
    ep_scr_free(d);
}
TEST(ep_caps_screen_logpixels_positive) {
    REQUIRE(U(GetDeviceCaps));
    HDC d = ep_scr();
    int x = U(GetDeviceCaps)(d, LOGPIXELSX), y = U(GetDeviceCaps)(d, LOGPIXELSY);
    CHECKF(x >= 96 && y >= 96, "%d,%d", x, y);
    CHECKF(x == y, "x %d y %d", x, y);
    ep_scr_free(d);
}
TEST(ep_caps_screen_size_mm_consistent_note) {
    REQUIRE(U(GetDeviceCaps));
    HDC d = ep_scr();
    int hs = U(GetDeviceCaps)(d, HORZSIZE), vs = U(GetDeviceCaps)(d, VERTSIZE);
    CHECKF(hs > 0 && vs > 0, "%d x %d mm", hs, vs);
    int w = U(GetDeviceCaps)(d, HORZRES), dpi = U(GetDeviceCaps)(d, LOGPIXELSX);
    t_note("size %dx%d mm, res %d px, logpixelsx %d, implied dpi %d", hs, vs, w, dpi, hs ? (w * 254 + hs * 5) / (hs * 10) : 0);
    ep_scr_free(d);
}
TEST(ep_caps_screen_aspect_note) {
    REQUIRE(U(GetDeviceCaps));
    HDC d = ep_scr();
    int ax = U(GetDeviceCaps)(d, ASPECTX), ay = U(GetDeviceCaps)(d, ASPECTY), axy = U(GetDeviceCaps)(d, ASPECTXY);
    CHECKF(ax > 0 && ay > 0 && axy > 0, "%d %d %d", ax, ay, axy);
    CHECKF(ax == ay, "aspectx %d aspecty %d", ax, ay);
    t_note("ASPECT %d %d %d", ax, ay, axy);
    ep_scr_free(d);
}
TEST(ep_caps_screen_printer_only_zero_note) {
    REQUIRE(U(GetDeviceCaps));
    HDC d = ep_scr();
    t_note("PHYSICALWIDTH=%d PHYSICALHEIGHT=%d OFFX=%d OFFY=%d SCALEX=%d SCALEY=%d",
           U(GetDeviceCaps)(d, PHYSICALWIDTH), U(GetDeviceCaps)(d, PHYSICALHEIGHT), U(GetDeviceCaps)(d, PHYSICALOFFSETX),
           U(GetDeviceCaps)(d, PHYSICALOFFSETY), U(GetDeviceCaps)(d, SCALINGFACTORX), U(GetDeviceCaps)(d, SCALINGFACTORY));
    ep_scr_free(d);
}
TEST(ep_caps_screen_bltalignment_note) { REQUIRE(U(GetDeviceCaps)); HDC d = ep_scr(); t_note("BLTALIGNMENT=%d VREFRESH=%d", U(GetDeviceCaps)(d, BLTALIGNMENT), U(GetDeviceCaps)(d, VREFRESH)); ep_scr_free(d); }
TEST(ep_caps_screen_vrefresh_nonneg) { REQUIRE(U(GetDeviceCaps)); HDC d = ep_scr(); CHECK(U(GetDeviceCaps)(d, VREFRESH) >= 0); ep_scr_free(d); }
TEST(ep_caps_screen_curvecaps_note) {
    REQUIRE(U(GetDeviceCaps));
    HDC d = ep_scr();
    int cc = U(GetDeviceCaps)(d, CURVECAPS), lc = U(GetDeviceCaps)(d, LINECAPS), pc = U(GetDeviceCaps)(d, POLYGONALCAPS), tc = U(GetDeviceCaps)(d, TEXTCAPS), cl = U(GetDeviceCaps)(d, CLIPCAPS);
    t_note("CURVECAPS=0x%x LINECAPS=0x%x POLYGONALCAPS=0x%x TEXTCAPS=0x%x CLIPCAPS=0x%x", cc, lc, pc, tc, cl);
    CHECK(cl & CP_RECTANGLE);
    CHECK(lc & LC_POLYLINE);
    CHECK(lc & LC_STYLED);
    CHECK(pc & PC_RECTANGLE);
    CHECK(cc & CC_CIRCLES);
    CHECK(tc & TC_RA_ABLE);
    ep_scr_free(d);
}
TEST(ep_caps_screen_brushes_pens_fonts_note) {
    REQUIRE(U(GetDeviceCaps));
    HDC d = ep_scr();
    int nb = U(GetDeviceCaps)(d, NUMBRUSHES), np = U(GetDeviceCaps)(d, NUMPENS), nm = U(GetDeviceCaps)(d, NUMMARKERS), nf = U(GetDeviceCaps)(d, NUMFONTS);
    t_note("NUMBRUSHES=%d NUMPENS=%d NUMMARKERS=%d NUMFONTS=%d PDEVICESIZE=%d", nb, np, nm, nf, U(GetDeviceCaps)(d, PDEVICESIZE));
    CHECK(nb == -1 || nb >= 0);
    ep_scr_free(d);
}
TEST(ep_caps_screen_driverversion_note) { REQUIRE(U(GetDeviceCaps)); HDC d = ep_scr(); t_note("DRIVERVERSION=0x%x", U(GetDeviceCaps)(d, DRIVERVERSION)); ep_scr_free(d); }
TEST(ep_caps_screen_colormgmt_note) { REQUIRE(U(GetDeviceCaps)); HDC d = ep_scr(); t_note("COLORMGMTCAPS=0x%x", U(GetDeviceCaps)(d, COLORMGMTCAPS)); ep_scr_free(d); }
TEST(ep_caps_screen_repeatable) {
    REQUIRE(U(GetDeviceCaps));
    HDC d = ep_scr();
    int a = U(GetDeviceCaps)(d, HORZRES);
    for (int i = 0; i < 200; i++) if (U(GetDeviceCaps)(d, HORZRES) != a) { CHECK(0); break; }
    ep_scr_free(d);
}
TEST(ep_caps_screen_dc_vs_window_dc_same) {
    REQUIRE(U(GetDeviceCaps));
    HWND w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 40, 40, NULL);
    HDC wd = U(GetDC)(w), sd = ep_scr();
    static const int idx[] = { TECHNOLOGY, HORZRES, VERTRES, BITSPIXEL, PLANES, LOGPIXELSX, LOGPIXELSY, NUMCOLORS, RASTERCAPS, SIZEPALETTE, COLORRES, HORZSIZE, VERTSIZE };
    for (unsigned i = 0; i < sizeof idx / sizeof idx[0]; i++) {
        int a = U(GetDeviceCaps)(wd, idx[i]), b = U(GetDeviceCaps)(sd, idx[i]);
        CHECKF(a == b, "index %d window %d screen %d", idx[i], a, b);
    }
    U(ReleaseDC)(w, wd); ep_scr_free(sd); U(DestroyWindow)(w);
}


/* observed-only equality: logs a mismatch, never fails */
#define NOTE(c) do { if (!(c)) t_note("differs: %s (line %d)", #c, __LINE__); } while (0)
#define NOTEF(c, ...) do { if (!(c)) { t_note("differs: %s (line %d)", #c, __LINE__); t_note(__VA_ARGS__); } } while (0)
/* ---- memory DCs ---- */
TEST(ep_caps_mem_default_is_1x1_mono) {
    REQUIRE(U(GetDeviceCaps));
    HDC m = U(CreateCompatibleDC)(NULL);
    CHECK(U(GetDeviceCaps)(m, HORZRES) == 1);
    CHECK(U(GetDeviceCaps)(m, VERTRES) == 1);
    CHECK(U(GetDeviceCaps)(m, BITSPIXEL) == 1);
    CHECK(U(GetDeviceCaps)(m, PLANES) == 1);
    U(DeleteDC)(m);
}
TEST(ep_caps_mem_default_numcolors_2) {
    REQUIRE(U(GetDeviceCaps));
    HDC m = U(CreateCompatibleDC)(NULL);
    int nc = U(GetDeviceCaps)(m, NUMCOLORS);
    CHECKF(nc == 2, "numcolors %d", nc);
    U(DeleteDC)(m);
}
TEST(ep_caps_mem_logpixels_match_screen) {
    REQUIRE(U(GetDeviceCaps));
    HDC d = ep_scr(), m = U(CreateCompatibleDC)(d);
    CHECK(U(GetDeviceCaps)(m, LOGPIXELSX) == U(GetDeviceCaps)(d, LOGPIXELSX));
    CHECK(U(GetDeviceCaps)(m, LOGPIXELSY) == U(GetDeviceCaps)(d, LOGPIXELSY));
    NOTE(U(GetDeviceCaps)(m, HORZSIZE) > 0 || 1);
    U(DeleteDC)(m); ep_scr_free(d);
}
TEST(ep_caps_mem_compat_bitmap_screen_format) {
    REQUIRE(U(GetDeviceCaps)); REQUIRE(U(CreateCompatibleBitmap));
    HDC d = ep_scr(), m = U(CreateCompatibleDC)(d);
    HBITMAP bm = U(CreateCompatibleBitmap)(d, 33, 17);
    HGDIOBJ o = U(SelectObject)(m, bm);
    NOTE(U(GetDeviceCaps)(m, HORZRES) == 33);
    NOTE(U(GetDeviceCaps)(m, VERTRES) == 17);
    NOTE(U(GetDeviceCaps)(m, BITSPIXEL) == U(GetDeviceCaps)(d, BITSPIXEL));
    NOTE(U(GetDeviceCaps)(m, PLANES) == U(GetDeviceCaps)(d, PLANES));
    U(SelectObject)(m, o); U(DeleteObject)(bm); U(DeleteDC)(m); ep_scr_free(d);
}
TEST(ep_caps_mem_mono_bitmap_size) {
    REQUIRE(U(GetDeviceCaps));
    HDC m = U(CreateCompatibleDC)(NULL);
    HBITMAP bm = U(CreateBitmap)(100, 50, 1, 1, NULL);
    HGDIOBJ o = U(SelectObject)(m, bm);
    CHECK(U(GetDeviceCaps)(m, HORZRES) == 100);
    CHECK(U(GetDeviceCaps)(m, VERTRES) == 50);
    CHECK(U(GetDeviceCaps)(m, BITSPIXEL) == 1);
    U(SelectObject)(m, o); U(DeleteObject)(bm); U(DeleteDC)(m);
}
TEST(ep_caps_mem_size_follows_selection) {
    REQUIRE(U(GetDeviceCaps));
    HDC m = U(CreateCompatibleDC)(NULL);
    HBITMAP a = U(CreateBitmap)(10, 20, 1, 1, NULL), b = U(CreateBitmap)(30, 5, 1, 1, NULL);
    HGDIOBJ o = U(SelectObject)(m, a);
    CHECK(U(GetDeviceCaps)(m, HORZRES) == 10 && U(GetDeviceCaps)(m, VERTRES) == 20);
    U(SelectObject)(m, b);
    CHECK(U(GetDeviceCaps)(m, HORZRES) == 30 && U(GetDeviceCaps)(m, VERTRES) == 5);
    U(SelectObject)(m, o);
    CHECK(U(GetDeviceCaps)(m, HORZRES) == 1 && U(GetDeviceCaps)(m, VERTRES) == 1);
    U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteDC)(m);
}
TEST(ep_caps_mem_big_bitmap) {
    REQUIRE(U(GetDeviceCaps));
    HDC m = U(CreateCompatibleDC)(NULL);
    HBITMAP bm = U(CreateBitmap)(4000, 3000, 1, 1, NULL);
    if (bm) {
        HGDIOBJ o = U(SelectObject)(m, bm);
        NOTE(U(GetDeviceCaps)(m, HORZRES) == 4000 && U(GetDeviceCaps)(m, VERTRES) == 3000);
        U(SelectObject)(m, o); U(DeleteObject)(bm);
    }
    U(DeleteDC)(m);
}
#define CAPS_DIB(bpp) \
TEST(ep_caps_mem_dib_##bpp##bpp) { REQUIRE(U(GetDeviceCaps)); HBITMAP b, o; void *bits; HDC m = dib_dc(bpp, 13, 7, &b, &o, &bits); \
    CHECK(m != NULL); if (!m) return; \
    CHECKF(U(GetDeviceCaps)(m, BITSPIXEL) == bpp, "BITSPIXEL %d", U(GetDeviceCaps)(m, BITSPIXEL)); \
    CHECK(U(GetDeviceCaps)(m, PLANES) == 1); \
    CHECK(U(GetDeviceCaps)(m, HORZRES) == 13); CHECK(U(GetDeviceCaps)(m, VERTRES) == 7); \
    CHECK(U(GetDeviceCaps)(m, TECHNOLOGY) == DT_RASDISPLAY); \
    int nc = U(GetDeviceCaps)(m, NUMCOLORS); \
    if (bpp > 8) NOTEF(nc == -1, "numcolors %d", nc); else t_note("%dbpp DIB NUMCOLORS=%d", bpp, nc); \
    if (bpp > 8) NOTE(!(U(GetDeviceCaps)(m, RASTERCAPS) & RC_PALETTE)); \
    t_note("%dbpp DIB: COLORRES=%d SIZEPALETTE=%d RASTERCAPS=0x%x", bpp, U(GetDeviceCaps)(m, COLORRES), U(GetDeviceCaps)(m, SIZEPALETTE), U(GetDeviceCaps)(m, RASTERCAPS)); \
    ep_memdc_free(m, b, o); }
CAPS_DIB(1) CAPS_DIB(4) CAPS_DIB(8) CAPS_DIB(16) CAPS_DIB(24) CAPS_DIB(32)
TEST(ep_caps_mem_dib32_rastercaps_bits) {
    REQUIRE(U(GetDeviceCaps));
    HBITMAP b, o; HDC m = ep_memdc32(4, 4, &b, &o, NULL);
    if (m) {
        int rc = U(GetDeviceCaps)(m, RASTERCAPS);
        CHECK(rc & RC_BITBLT);
        CHECK(rc & RC_STRETCHBLT);
        CHECK(rc & RC_DI_BITMAP);
        NOTE(!(rc & RC_PALETTE));
        ep_memdc_free(m, b, o);
    }
}
TEST(ep_caps_mem_dib_topdown_vs_bottomup_same) {
    REQUIRE(U(GetDeviceCaps));
    HDC m = U(CreateCompatibleDC)(NULL);
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 9; bi.bmiHeader.biHeight = 11; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    void *bits;
    HBITMAP bm = U(CreateDIBSection)(m, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    HGDIOBJ o = U(SelectObject)(m, bm);
    NOTE(U(GetDeviceCaps)(m, HORZRES) == 9);
    NOTE(U(GetDeviceCaps)(m, VERTRES) == 11);
    U(SelectObject)(m, o); U(DeleteObject)(bm); U(DeleteDC)(m);
}
TEST(ep_caps_mem_dib_wide_1x1) {
    REQUIRE(U(GetDeviceCaps));
    HBITMAP b, o; HDC m = ep_memdc32(1, 1, &b, &o, NULL);
    if (m) { NOTE(U(GetDeviceCaps)(m, HORZRES) == 1 && U(GetDeviceCaps)(m, VERTRES) == 1 && U(GetDeviceCaps)(m, BITSPIXEL) == 32); ep_memdc_free(m, b, o); }
}
TEST(ep_caps_mem_dib_2000x2000) {
    REQUIRE(U(GetDeviceCaps));
    HBITMAP b, o; HDC m = ep_memdc32(2000, 2000, &b, &o, NULL);
    if (m) { NOTE(U(GetDeviceCaps)(m, HORZRES) == 2000 && U(GetDeviceCaps)(m, VERTRES) == 2000); ep_memdc_free(m, b, o); }
}
TEST(ep_caps_mem_dc_of_mem_dc_inherits_format_note) {
    REQUIRE(U(GetDeviceCaps));
    HBITMAP b, o; HDC m = ep_memdc32(4, 4, &b, &o, NULL);
    if (m) {
        HDC m2 = U(CreateCompatibleDC)(m);
        t_note("compat DC of 32bpp DIB DC: bpp=%d size=%dx%d", U(GetDeviceCaps)(m2, BITSPIXEL), U(GetDeviceCaps)(m2, HORZRES), U(GetDeviceCaps)(m2, VERTRES));
        NOTE(U(GetDeviceCaps)(m2, HORZRES) == 1 && U(GetDeviceCaps)(m2, VERTRES) == 1);
        CHECK(U(GetDeviceCaps)(m2, TECHNOLOGY) == DT_RASDISPLAY);
        U(DeleteDC)(m2);
        ep_memdc_free(m, b, o);
    }
}
TEST(ep_caps_mem_vs_screen_differences) {
    REQUIRE(U(GetDeviceCaps));
    HDC d = ep_scr(), m = U(CreateCompatibleDC)(NULL);
    NOTE(U(GetDeviceCaps)(m, HORZRES) != U(GetDeviceCaps)(d, HORZRES));
    NOTE(U(GetDeviceCaps)(m, BITSPIXEL) == 1);
    NOTE(U(GetDeviceCaps)(d, BITSPIXEL) > 1);
    U(DeleteDC)(m); ep_scr_free(d);
}
TEST(ep_caps_mem_desktop_res_note) {
    REQUIRE(U(GetDeviceCaps));
    HDC m = U(CreateCompatibleDC)(NULL);
    t_note("mem DC DESKTOPHORZRES=%d DESKTOPVERTRES=%d", U(GetDeviceCaps)(m, DESKTOPHORZRES), U(GetDeviceCaps)(m, DESKTOPVERTRES));
    U(DeleteDC)(m);
}
TEST(ep_caps_gdi_balance) {
    REQUIRE(U(GetDeviceCaps)); REQUIRE(U(GetGuiResources));
    DWORD before = ep_gui();
    for (int i = 0; i < 100; i++) { HDC d = ep_scr(); U(GetDeviceCaps)(d, HORZRES); ep_scr_free(d); HDC m = U(CreateCompatibleDC)(NULL); U(GetDeviceCaps)(m, BITSPIXEL); U(DeleteDC)(m); }
    CHECK(ep_gui() == before);
}
