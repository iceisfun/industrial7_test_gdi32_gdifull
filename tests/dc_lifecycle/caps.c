/* GetDeviceCaps */
#include "dcl.h"
#include <math.h>

static int cap(HDC dc, int i) { return U(GetDeviceCaps)(dc, i); }

/* Set of DC flavours exercised by the constant-value tests. */
typedef struct {
    HDC dc[6];
    const char *name[6];
    int n;
    HDC scr, win_dc;
    HWND win;
    HBITMAP bmp, old;
    HDC dibdc;
} kinds_t;

static void kinds_open(kinds_t *k) {
    memset(k, 0, sizeof *k);
    k->scr = dcl_screen();
    k->dc[k->n] = k->scr; k->name[k->n++] = "GetDC(NULL)";
    k->dc[k->n] = U(CreateDCA)("DISPLAY", NULL, NULL, NULL); k->name[k->n++] = "CreateDC";
    k->dc[k->n] = U(CreateICA)("DISPLAY", NULL, NULL, NULL); k->name[k->n++] = "CreateIC";
    k->dc[k->n] = dcl_mem(); k->name[k->n++] = "memdc";
    k->dibdc = dcl_mem32(16, 16, &k->bmp, &k->old, NULL);
    k->dc[k->n] = k->dibdc; k->name[k->n++] = "memdc+DIB32";
    k->win = dcl_window(20, 20, 64, 64);
    if (k->win) { k->win_dc = U(GetDC)(k->win); k->dc[k->n] = k->win_dc; k->name[k->n++] = "window DC"; }
}
static void kinds_close(kinds_t *k) {
    if (k->win_dc) U(ReleaseDC)(k->win, k->win_dc);
    if (k->win) U(DestroyWindow)(k->win);
    dcl_free_mem(k->dibdc, k->bmp, k->old);
    if (k->dc[1]) U(DeleteDC)(k->dc[1]);
    if (k->dc[2]) U(DeleteDC)(k->dc[2]);
    if (k->dc[3]) U(DeleteDC)(k->dc[3]);
    if (k->scr) dcl_release_screen(k->scr);
}

#define CONST_CAP(tname, idx, val) \
TEST(dcl_caps_const_##tname) { \
    REQUIRE(U(GetDeviceCaps)); REQUIRE(U(CreateDCA)); REQUIRE(U(CreateICA)); \
    kinds_t k; kinds_open(&k); \
    for (int i = 0; i < k.n; i++) if (k.dc[i]) { \
        int v = cap(k.dc[i], idx); \
        CHECKF(v == (val), "%s: index %d got %d want %d", k.name[i], idx, v, (int)(val)); } \
    kinds_close(&k); \
}

CONST_CAP(numbrushes, NUMBRUSHES, -1)
CONST_CAP(nummarkers, NUMMARKERS, 0)
CONST_CAP(pdevicesize, PDEVICESIZE, 0)
CONST_CAP(curvecaps, CURVECAPS, 0x1ff)
CONST_CAP(linecaps, LINECAPS, 0xfe)
CONST_CAP(polygonalcaps, POLYGONALCAPS, 0xff)
CONST_CAP(clipcaps, CLIPCAPS, 1)
CONST_CAP(numreserved, NUMRESERVED, 20)
CONST_CAP(technology, TECHNOLOGY, DT_RASDISPLAY)
CONST_CAP(scalingfactorx, SCALINGFACTORX, 0)
CONST_CAP(scalingfactory, SCALINGFACTORY, 0)

/* Geometry-style caps must agree between every DC flavour. */
#define SAME_CAP(tname, idx) \
TEST(dcl_caps_same_##tname) { \
    REQUIRE(U(GetDeviceCaps)); REQUIRE(U(CreateDCA)); REQUIRE(U(CreateICA)); \
    kinds_t k; kinds_open(&k); \
    int ref = cap(k.scr, idx); \
    for (int i = 1; i < k.n; i++) if (k.dc[i]) { \
        int v = cap(k.dc[i], idx); \
        CHECKF(v == ref, "%s: index %d got %d, screen %d", k.name[i], idx, v, ref); } \
    kinds_close(&k); \
}
SAME_CAP(horzres, HORZRES)
SAME_CAP(vertres, VERTRES)
SAME_CAP(horzsize, HORZSIZE)
SAME_CAP(vertsize, VERTSIZE)
SAME_CAP(logpixelsx, LOGPIXELSX)
SAME_CAP(logpixelsy, LOGPIXELSY)
SAME_CAP(aspectx, ASPECTX)
SAME_CAP(aspecty, ASPECTY)
SAME_CAP(aspectxy, ASPECTXY)
SAME_CAP(desktophorzres, DESKTOPHORZRES)
SAME_CAP(desktopvertres, DESKTOPVERTRES)
SAME_CAP(textcaps, TEXTCAPS)
SAME_CAP(driverversion, DRIVERVERSION)
SAME_CAP(physicalwidth, PHYSICALWIDTH)
SAME_CAP(physicalheight, PHYSICALHEIGHT)
SAME_CAP(physicaloffsetx, PHYSICALOFFSETX)
SAME_CAP(physicaloffsety, PHYSICALOFFSETY)
SAME_CAP(bltalignment, BLTALIGNMENT)
SAME_CAP(shadeblendcaps, SHADEBLENDCAPS)
SAME_CAP(colormgmtcaps, COLORMGMTCAPS)

/* ------------------------------------------------------------ invalid index */

TEST(dcl_caps_undefined_indices_zero_screen) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_screen();
    static const unsigned char valid[130] = {
        [0]=1,[2]=1,[4]=1,[6]=1,[8]=1,[10]=1,[12]=1,[14]=1,[16]=1,[18]=1,[20]=1,[22]=1,[24]=1,[26]=1,[28]=1,
        [30]=1,[32]=1,[34]=1,[36]=1,[38]=1,[40]=1,[42]=1,[44]=1,[88]=1,[90]=1,[104]=1,[106]=1,[108]=1,
        [110]=1,[111]=1,[112]=1,[113]=1,[114]=1,[115]=1,[116]=1,[117]=1,[118]=1,[119]=1,[120]=1,[121]=1 };
    for (int i = 0; i < 130; i++) {
        if (valid[i]) continue;
        int v = cap(dc, i);
        CHECKF(v == 0, "undefined index %d returned %d", i, v);
    }
    dcl_release_screen(dc);
}

TEST(dcl_caps_undefined_indices_zero_memdc) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_mem();
    static const unsigned char valid[130] = {
        [0]=1,[2]=1,[4]=1,[6]=1,[8]=1,[10]=1,[12]=1,[14]=1,[16]=1,[18]=1,[20]=1,[22]=1,[24]=1,[26]=1,[28]=1,
        [30]=1,[32]=1,[34]=1,[36]=1,[38]=1,[40]=1,[42]=1,[44]=1,[88]=1,[90]=1,[104]=1,[106]=1,[108]=1,
        [110]=1,[111]=1,[112]=1,[113]=1,[114]=1,[115]=1,[116]=1,[117]=1,[118]=1,[119]=1,[120]=1,[121]=1 };
    for (int i = 0; i < 130; i++) {
        if (valid[i]) continue;
        int v = cap(dc, i);
        CHECKF(v == 0, "undefined index %d returned %d", i, v);
    }
    U(DeleteDC)(dc);
}

TEST(dcl_caps_extreme_indices) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_screen();
    static const int idx[] = { -1, -2, -100, 122, 123, 127, 128, 129, 255, 256, 257, 1000, 0x7fff, 0x8000, 0xffff, 0x10000, 0x10002,
                               0x10058, 0x1000000, INT_MAX, INT_MIN, INT_MIN + 2 };
    for (unsigned i = 0; i < sizeof idx / sizeof *idx; i++) {
        int v = cap(dc, idx[i]);
        CHECKF(v == 0, "index %d returned %d", idx[i], v);
    }
    dcl_release_screen(dc);
}

TEST(dcl_caps_odd_indices_zero) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_screen();
    for (int i = 1; i < 120; i += 2) {
        if (i == 111 || i == 113 || i == 115 || i == 117 || i == 119 || i == 121) continue;
        CHECKF(cap(dc, i) == 0, "odd index %d", i);
    }
    dcl_release_screen(dc);
}

/* ---------------------------------------------------------- invalid handles */

TEST(dcl_caps_null_handle) {
    REQUIRE(U(GetDeviceCaps));
    for (int i = 0; i <= 121; i++) {
        SET_ERR(0xdeadbeef);
        int v = cap(NULL, i);
        CHECKF(v == 0, "index %d returned %d", i, v);
        if (i == TECHNOLOGY) EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    }
}

TEST(dcl_caps_bogus_handle) {
    REQUIRE(U(GetDeviceCaps));
    static const ULONG_PTR h[] = { 0xdeadbeef, 0x1, 0xffffffff };
    for (unsigned j = 0; j < sizeof h / sizeof *h; j++)
        for (int i = 0; i <= 121; i += 1) {
            int v = cap((HDC)h[j], i);
            CHECKF(v == 0, "handle 0x%llx index %d returned %d", (unsigned long long)h[j], i, v);
        }
    SET_ERR(0xdeadbeef);
    cap((HDC)(ULONG_PTR)0xdeadbeef, TECHNOLOGY);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}

TEST(dcl_caps_deleted_dc) {
    REQUIRE(U(GetDeviceCaps));
    HDC mem = dcl_mem(), dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    CHECK(mem && dc);
    CHECK(U(DeleteDC)(mem)); CHECK(U(DeleteDC)(dc));
    for (int i = 0; i <= 121; i++) {
        CHECKF(cap(mem, i) == 0, "deleted memdc index %d", i);
        CHECKF(cap(dc, i) == 0, "deleted dc index %d", i);
    }
    SET_ERR(0xdeadbeef);
    cap(dc, TECHNOLOGY);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_INVALID_HANDLE || e == ERROR_INVALID_PARAMETER, "error %lu", e);
}

TEST(dcl_caps_non_dc_objects) {
    REQUIRE(U(GetDeviceCaps));
    HGDIOBJ objs[4] = { U(CreateSolidBrush)(0), U(CreatePen)(PS_SOLID, 1, 0), U(CreateBitmap)(2, 2, 1, 1, NULL), U(CreateRectRgn)(0, 0, 4, 4) };
    for (int j = 0; j < 4; j++) {
        for (int i = 0; i <= 121; i += 1) CHECKF(cap((HDC)objs[j], i) == 0, "object %d index %d", j, i);
        U(DeleteObject)(objs[j]);
    }
    CHECK(cap((HDC)U(GetStockObject)(WHITE_BRUSH), HORZRES) == 0);
    CHECK(cap((HDC)U(GetStockObject)(SYSTEM_FONT), TECHNOLOGY) == 0);
}

/* ------------------------------------------------------------ relations */

TEST(dcl_caps_bpp_planes) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_screen();
    int bpp = cap(dc, BITSPIXEL), pl = cap(dc, PLANES);
    CHECKF(pl == 1, "planes %d", pl);
    CHECKF(bpp == 1 || bpp == 4 || bpp == 8 || bpp == 15 || bpp == 16 || bpp == 24 || bpp == 32, "bpp %d", bpp);
    CHECK(bpp * pl >= 1);
    t_note("BITSPIXEL=%d PLANES=%d COLORRES=%d NUMCOLORS=%d", bpp, pl, cap(dc, COLORRES), cap(dc, NUMCOLORS));
    dcl_release_screen(dc);
}

TEST(dcl_caps_numcolors_vs_depth) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_screen();
    int bpp = cap(dc, BITSPIXEL), nc = cap(dc, NUMCOLORS);
    if (bpp > 8) CHECKF(nc == -1, "NUMCOLORS %d at %d bpp", nc, bpp);
    else if (bpp == 8) CHECKF(nc == 20, "NUMCOLORS %d at 8bpp", nc);
    else t_note("NUMCOLORS %d at %d bpp", nc, bpp);
    dcl_release_screen(dc);
}

TEST(dcl_caps_palette_consistency) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_screen();
    int bpp = cap(dc, BITSPIXEL), sp = cap(dc, SIZEPALETTE), rc = cap(dc, RASTERCAPS);
    if (bpp <= 8) {
        CHECK(rc & RC_PALETTE);
        CHECKF(sp == (1 << bpp) || sp == 256, "SIZEPALETTE %d at %d bpp", sp, bpp);
    } else {
        CHECK(!(rc & RC_PALETTE));
        CHECKF(sp == 0, "SIZEPALETTE %d at %d bpp", sp, bpp);
    }
    dcl_release_screen(dc);
}

TEST(dcl_caps_colorres) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_screen();
    int bpp = cap(dc, BITSPIXEL), cr = cap(dc, COLORRES);
    if (bpp >= 24) CHECKF(cr == 24, "COLORRES %d at %d bpp", cr, bpp);
    else if (bpp == 8) CHECKF(cr == 18 || cr == 24, "COLORRES %d at 8 bpp", cr);
    else CHECK(cr > 0);
    dcl_release_screen(dc);
}

TEST(dcl_caps_rastercaps_required_bits) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_screen();
    int rc = cap(dc, RASTERCAPS);
    static const struct { int bit; const char *n; } req[] = {
        { RC_BITBLT, "RC_BITBLT" }, { RC_BITMAP64, "RC_BITMAP64" }, { RC_DI_BITMAP, "RC_DI_BITMAP" },
        { RC_DIBTODEV, "RC_DIBTODEV" }, { RC_STRETCHBLT, "RC_STRETCHBLT" }, { RC_STRETCHDIB, "RC_STRETCHDIB" },
        { RC_FLOODFILL, "RC_FLOODFILL" },
    };
    for (unsigned i = 0; i < sizeof req / sizeof *req; i++) CHECKF(rc & req[i].bit, "%s missing in 0x%x", req[i].n, rc);
    t_note("RASTERCAPS=0x%x", rc);
    dcl_release_screen(dc);
}

TEST(dcl_caps_rastercaps_same_memdc) {
    REQUIRE(U(GetDeviceCaps));
    HDC s = dcl_screen(), m = dcl_mem();
    int a = cap(s, RASTERCAPS), b = cap(m, RASTERCAPS);
    CHECKF((a & ~RC_PALETTE) == (b & ~RC_PALETTE), "screen 0x%x memdc 0x%x", a, b);
    U(DeleteDC)(m); dcl_release_screen(s);
}

TEST(dcl_caps_aspect_relations) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_screen();
    int ax = cap(dc, ASPECTX), ay = cap(dc, ASPECTY), axy = cap(dc, ASPECTXY);
    CHECK(ax > 0 && ay > 0 && axy > 0);
    double h = sqrt((double)ax * ax + (double)ay * ay);
    CHECKF(fabs(h - axy) <= 1.0, "ASPECTXY %d vs hypot(%d,%d)=%.2f", axy, ax, ay, h);
    if (ax != ay) t_note("non-square pixels ASPECTX=%d ASPECTY=%d", ax, ay);
    dcl_release_screen(dc);
}

TEST(dcl_caps_logpixels_positive_square) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_screen();
    int lx = cap(dc, LOGPIXELSX), ly = cap(dc, LOGPIXELSY);
    CHECK(lx >= 96 && ly >= 96);
    CHECKF(lx == ly, "LOGPIXELSX %d LOGPIXELSY %d", lx, ly);
    CHECK(lx <= 960);
    dcl_release_screen(dc);
}

TEST(dcl_caps_logpixels_vs_system_dpi) {
    REQUIRE(U(GetDeviceCaps));
    UDECL(UINT, GetDpiForSystem, (void));
    if (!UN(GetDpiForSystem)) { t_skip("no GetDpiForSystem"); return; }
    HDC dc = dcl_screen();
    UINT dpi = UN(GetDpiForSystem)();
    int lx = cap(dc, LOGPIXELSX);
    if ((UINT)lx != dpi) t_note("LOGPIXELSX %d vs GetDpiForSystem %u", lx, dpi);
    CHECK(dpi >= 96);
    dcl_release_screen(dc);
}

TEST(dcl_caps_logpixels_via_createdc_vs_getdc) {
    REQUIRE(U(GetDeviceCaps));
    HDC a = dcl_screen(), b = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    EQ(cap(a, LOGPIXELSX), cap(b, LOGPIXELSX));
    EQ(cap(a, LOGPIXELSY), cap(b, LOGPIXELSY));
    U(DeleteDC)(b); dcl_release_screen(a);
}

TEST(dcl_caps_size_vs_res_dpi) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_screen();
    int hs = cap(dc, HORZSIZE), vs = cap(dc, VERTSIZE), hr = cap(dc, HORZRES), vr = cap(dc, VERTRES);
    int lx = cap(dc, LOGPIXELSX), ly = cap(dc, LOGPIXELSY);
    CHECK(hs > 0 && vs > 0 && hr > 0 && vr > 0);
    /* HORZSIZE is in millimetres: size*dpi/25.4 should be within a few percent of the resolution */
    double ph = hs * lx / 25.4, pv = vs * ly / 25.4;
    t_note("HORZSIZE=%d VERTSIZE=%d -> %.1f x %.1f px at dpi; HORZRES=%d VERTRES=%d", hs, vs, ph, pv, hr, vr);
    dcl_release_screen(dc);
}

TEST(dcl_caps_desktop_res_vs_res) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_screen();
    int dh = cap(dc, DESKTOPHORZRES), dv = cap(dc, DESKTOPVERTRES), hr = cap(dc, HORZRES), vr = cap(dc, VERTRES);
    CHECK(dh >= hr && dv >= vr);
    CHECK(dh > 0 && dv > 0);
    if (dh != hr || dv != vr) t_note("DESKTOPRES %dx%d differs from RES %dx%d", dh, dv, hr, vr);
    dcl_release_screen(dc);
}

TEST(dcl_caps_desktop_res_vs_metrics) {
    REQUIRE(U(GetDeviceCaps)); REQUIRE(U(GetSystemMetrics));
    HDC dc = dcl_screen();
    int dh = cap(dc, DESKTOPHORZRES), dv = cap(dc, DESKTOPVERTRES);
    int cx = U(GetSystemMetrics)(SM_CXSCREEN), cy = U(GetSystemMetrics)(SM_CYSCREEN);
    if (dh != cx || dv != cy) t_note("DESKTOPRES %dx%d vs SM_C*SCREEN %dx%d", dh, dv, cx, cy);
    CHECK(dh >= cx / 4 && dv >= cy / 4);
    dcl_release_screen(dc);
}

TEST(dcl_caps_res_vs_metrics) {
    REQUIRE(U(GetDeviceCaps)); REQUIRE(U(GetSystemMetrics));
    HDC dc = dcl_screen();
    int hr = cap(dc, HORZRES), vr = cap(dc, VERTRES);
    int cx = U(GetSystemMetrics)(SM_CXSCREEN), cy = U(GetSystemMetrics)(SM_CYSCREEN);
    if (hr != cx || vr != cy) t_note("HORZRES/VERTRES %dx%d vs SM_C*SCREEN %dx%d", hr, vr, cx, cy);
    CHECK(hr > 0 && vr > 0 && cx > 0 && cy > 0);
    dcl_release_screen(dc);
}

TEST(dcl_caps_vrefresh) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_screen(), mem = dcl_mem();
    int v = cap(dc, VREFRESH);
    CHECKF(v >= 0, "VREFRESH %d", v);
    t_note("VREFRESH screen=%d memdc=%d", v, cap(mem, VREFRESH));
    U(DeleteDC)(mem); dcl_release_screen(dc);
}

TEST(dcl_caps_textcaps_bits) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_screen();
    int tc = cap(dc, TEXTCAPS);
    CHECK(tc != 0);
    CHECK(!(tc & 0xffff0000));
    t_note("TEXTCAPS=0x%x", tc);
    dcl_release_screen(dc);
}

TEST(dcl_caps_shadeblend_colormgmt_bits) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_screen();
    int sb = cap(dc, SHADEBLENDCAPS), cm = cap(dc, COLORMGMTCAPS);
    CHECK((sb & ~(SB_CONST_ALPHA | SB_GRAD_RECT | SB_GRAD_TRI | SB_NONE | SB_PIXEL_ALPHA | SB_PREMULT_ALPHA)) == 0);
    CHECK((cm & ~(CM_CMYK_COLOR | CM_DEVICE_ICM | CM_GAMMA_RAMP)) == 0);
    t_note("SHADEBLENDCAPS=0x%x COLORMGMTCAPS=0x%x", sb, cm);
    dcl_release_screen(dc);
}

TEST(dcl_caps_bltalignment) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_screen();
    int v = cap(dc, BLTALIGNMENT);
    CHECKF(v >= 0, "BLTALIGNMENT %d", v);
    t_note("BLTALIGNMENT=%d", v);
    dcl_release_screen(dc);
}

TEST(dcl_caps_physical_page) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_screen();
    CHECKF(cap(dc, PHYSICALOFFSETX) == 0, "offx %d", cap(dc, PHYSICALOFFSETX));
    CHECKF(cap(dc, PHYSICALOFFSETY) == 0, "offy %d", cap(dc, PHYSICALOFFSETY));
    CHECKF(cap(dc, PHYSICALWIDTH) >= 0 && cap(dc, PHYSICALHEIGHT) >= 0, "physical size");
    t_note("PHYSICALWIDTH=%d PHYSICALHEIGHT=%d", cap(dc, PHYSICALWIDTH), cap(dc, PHYSICALHEIGHT));
    dcl_release_screen(dc);
}

TEST(dcl_caps_numpens_numfonts_driverversion) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_screen();
    t_note("NUMPENS=%d NUMFONTS=%d DRIVERVERSION=0x%x NUMBRUSHES=%d", cap(dc, NUMPENS), cap(dc, NUMFONTS), cap(dc, DRIVERVERSION), cap(dc, NUMBRUSHES));
    dcl_release_screen(dc);
}

TEST(dcl_caps_sweep_note_screen) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_screen();
    char line[1024]; int n = 0;
    for (int i = 0; i <= 121; i++) {
        int v = cap(dc, i);
        if (v) n += snprintf(line + n, sizeof line - n, "%d=%d ", i, v);
        if (n > 900) break;
    }
    t_note("screen caps: %s", line);
    dcl_release_screen(dc);
}

TEST(dcl_caps_sweep_note_memdc) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_mem();
    char line[1024]; int n = 0;
    for (int i = 0; i <= 121; i++) {
        int v = cap(dc, i);
        if (v) n += snprintf(line + n, sizeof line - n, "%d=%d ", i, v);
        if (n > 900) break;
    }
    t_note("memdc caps: %s", line);
    U(DeleteDC)(dc);
}

TEST(dcl_caps_memdc_default_bitmap_depth) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_mem();
    int bpp = cap(dc, BITSPIXEL), pl = cap(dc, PLANES);
    t_note("fresh memdc: BITSPIXEL=%d PLANES=%d", bpp, pl);
    CHECK(bpp >= 1 && pl >= 1);
    U(DeleteDC)(dc);
}

TEST(dcl_caps_memdc_compat_bitmap_depth) {
    REQUIRE(U(GetDeviceCaps)); REQUIRE(U(CreateCompatibleBitmap));
    HDC s = dcl_screen(), m = dcl_mem();
    HBITMAP bm = U(CreateCompatibleBitmap)(s, 20, 20);
    HGDIOBJ old = U(SelectObject)(m, bm);
    EQ(cap(m, BITSPIXEL), cap(s, BITSPIXEL));
    EQ(cap(m, PLANES), cap(s, PLANES));
    U(SelectObject)(m, old);
    U(DeleteObject)(bm);
    U(DeleteDC)(m); dcl_release_screen(s);
}

TEST(dcl_caps_memdc_dib_depths_note) {
    REQUIRE(U(GetDeviceCaps));
    static const int depths[] = { 1, 4, 8, 16, 24, 32 };
    HDC s = dcl_screen();
    for (unsigned i = 0; i < sizeof depths / sizeof *depths; i++) {
        HDC m = dcl_mem();
        HBITMAP b = dcl_dib(m, 8, 8, depths[i], NULL);
        HGDIOBJ old = U(SelectObject)(m, b);
        t_note("DIB %d bpp selected: BITSPIXEL=%d PLANES=%d NUMCOLORS=%d SIZEPALETTE=%d RASTERCAPS=0x%x", depths[i], cap(m, BITSPIXEL),
               cap(m, PLANES), cap(m, NUMCOLORS), cap(m, SIZEPALETTE), cap(m, RASTERCAPS));
        EQ(cap(m, TECHNOLOGY), DT_RASDISPLAY);
        EQ(cap(m, HORZRES), cap(s, HORZRES));
        EQ(cap(m, VERTRES), cap(s, VERTRES));
        U(SelectObject)(m, old);
        U(DeleteObject)(b);
        U(DeleteDC)(m);
    }
    dcl_release_screen(s);
}

TEST(dcl_caps_independent_of_map_mode) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_screen();
    int hr = cap(dc, HORZRES), vs = cap(dc, VERTSIZE), lx = cap(dc, LOGPIXELSX);
    static const int modes[] = { MM_LOMETRIC, MM_HIMETRIC, MM_LOENGLISH, MM_HIENGLISH, MM_TWIPS, MM_ISOTROPIC, MM_ANISOTROPIC };
    int old = U(GetMapMode)(dc);
    for (unsigned i = 0; i < sizeof modes / sizeof *modes; i++) {
        U(SetMapMode)(dc, modes[i]);
        EQ(cap(dc, HORZRES), hr);
        EQ(cap(dc, VERTSIZE), vs);
        EQ(cap(dc, LOGPIXELSX), lx);
    }
    U(SetViewportExtEx)(dc, 7, 9, NULL);
    EQ(cap(dc, HORZRES), hr);
    U(SetMapMode)(dc, old);
    dcl_release_screen(dc);
}

TEST(dcl_caps_independent_of_world_transform) {
    REQUIRE(U(GetDeviceCaps)); REQUIRE(U(SetWorldTransform));
    HDC dc = dcl_mem();
    int hr = cap(dc, HORZRES);
    U(SetGraphicsMode)(dc, GM_ADVANCED);
    XFORM x = { 3, 0, 0, 3, 10, 10 };
    U(SetWorldTransform)(dc, &x);
    EQ(cap(dc, HORZRES), hr);
    U(DeleteDC)(dc);
}

TEST(dcl_caps_stable_repeated) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_screen();
    int ref[122];
    for (int i = 0; i <= 121; i++) ref[i] = cap(dc, i);
    int diff = 0;
    for (int r = 0; r < 200; r++)
        for (int i = 0; i <= 121; i++)
            if (i != VREFRESH && cap(dc, i) != ref[i]) diff++;
    CHECK(diff == 0);
    dcl_release_screen(dc);
}

TEST(dcl_caps_after_save_restore) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = dcl_mem();
    int hr = cap(dc, HORZRES);
    int l = U(SaveDC)(dc);
    U(RestoreDC)(dc, l);
    EQ(cap(dc, HORZRES), hr);
    U(DeleteDC)(dc);
}

TEST(dcl_caps_ic_vs_dc_all_indices) {
    REQUIRE(U(GetDeviceCaps));
    HDC ic = U(CreateICA)("DISPLAY", NULL, NULL, NULL), dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    for (int i = 0; i <= 121; i++) if (i != VREFRESH) CHECKF(cap(ic, i) == cap(dc, i), "index %d", i);
    U(DeleteDC)(ic); U(DeleteDC)(dc);
}

TEST(dcl_caps_window_dc_vs_screen) {
    REQUIRE(U(GetDeviceCaps)); REQUIRE(U(GetDC)); REQUIRE(U(GetWindowDC));
    HWND w = dcl_window(30, 30, 80, 60);
    if (!w) { t_skip("no window"); return; }
    HDC a = U(GetDC)(w), b = U(GetWindowDC)(w), s = dcl_screen();
    for (int i = 0; i <= 121; i++) {
        if (i == VREFRESH) continue;
        CHECKF(cap(a, i) == cap(s, i), "client DC index %d", i);
        CHECKF(cap(b, i) == cap(s, i), "window DC index %d", i);
    }
    U(ReleaseDC)(w, a); U(ReleaseDC)(w, b); dcl_release_screen(s);
    U(DestroyWindow)(w);
}

TEST(dcl_caps_desktop_window_dc) {
    REQUIRE(U(GetDeviceCaps)); REQUIRE(U(GetDC)); REQUIRE(U(GetDesktopWindow));
    HWND d = U(GetDesktopWindow)();
    HDC a = U(GetDC)(d), s = dcl_screen();
    EQ(cap(a, TECHNOLOGY), DT_RASDISPLAY);
    EQ(cap(a, HORZRES), cap(s, HORZRES));
    EQ(cap(a, BITSPIXEL), cap(s, BITSPIXEL));
    U(ReleaseDC)(d, a); dcl_release_screen(s);
}

TEST(dcl_caps_after_delete_does_not_poison_new_dc) {
    REQUIRE(U(GetDeviceCaps));
    for (int r = 0; r < 20; r++) {
        HDC a = dcl_mem();
        int hr = cap(a, HORZRES);
        U(DeleteDC)(a);
        HDC b = dcl_mem();
        EQ(cap(b, HORZRES), hr);
        U(DeleteDC)(b);
    }
}

TEST(dcl_caps_other_thread_value_same) {
    REQUIRE(U(GetDeviceCaps));
    /* A DC created on one thread can be queried from another */
    static HDC shared; static int result;
    shared = dcl_mem();
    int here = cap(shared, HORZRES);
    DWORD WINAPI th(LPVOID p) { result = cap(shared, HORZRES); return 0; }
    HANDLE h = CreateThread(NULL, 0, th, NULL, 0, NULL);
    WaitForSingleObject(h, 10000);
    CloseHandle(h);
    EQ(result, here);
    U(DeleteDC)(shared);
}
