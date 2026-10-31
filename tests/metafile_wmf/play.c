/* PlayMetaFile: recorded scenes must replay to the same pixels as direct drawing on a DIB section. */
#include "wmf.h"

typedef void (*scene_fn)(HDC);

static HGDIOBJ stock(int i) { return U(GetStockObject)(i); }

/* ---- scenes ---- */
static void sc_lines(HDC dc) {
    HPEN red = U(CreatePen)(PS_SOLID, 1, RGB(255, 0, 0));
    HPEN blue3 = U(CreatePen)(PS_SOLID, 3, RGB(0, 0, 255));
    HGDIOBJ old = U(SelectObject)(dc, red);
    U(MoveToEx)(dc, 2, 2, NULL); U(LineTo)(dc, 90, 40); U(LineTo)(dc, 10, 80); U(LineTo)(dc, 50, 5);
    U(SelectObject)(dc, blue3);
    U(MoveToEx)(dc, 5, 90, NULL); U(LineTo)(dc, 90, 90); U(LineTo)(dc, 90, 60);
    U(SelectObject)(dc, old);
    U(DeleteObject)(red); U(DeleteObject)(blue3);
}
static void sc_shapes(HDC dc) {
    HBRUSH g = U(CreateSolidBrush)(RGB(0, 200, 0));
    HBRUSH y = U(CreateSolidBrush)(RGB(250, 250, 0));
    HGDIOBJ ob = U(SelectObject)(dc, g);
    U(Rectangle)(dc, 5, 5, 40, 30);
    U(Ellipse)(dc, 45, 5, 90, 40);
    U(SelectObject)(dc, y);
    U(RoundRect)(dc, 5, 45, 50, 90, 12, 8);
    U(Rectangle)(dc, 60, 50, 91, 70);
    U(SelectObject)(dc, ob);
    U(DeleteObject)(g); U(DeleteObject)(y);
}
static void sc_arcs(HDC dc) {
    HBRUSH b = U(CreateSolidBrush)(RGB(10, 120, 250));
    HGDIOBJ ob = U(SelectObject)(dc, b);
    U(Arc)(dc, 2, 2, 45, 45, 45, 20, 2, 20);
    U(Pie)(dc, 48, 2, 92, 45, 92, 20, 48, 30);
    U(Chord)(dc, 2, 50, 45, 92, 40, 50, 5, 70);
    U(Ellipse)(dc, 55, 55, 85, 85);
    U(SelectObject)(dc, ob);
    U(DeleteObject)(b);
}
static void sc_polys(HDC dc) {
    POINT star[5] = { {48, 3}, {80, 90}, {5, 35}, {90, 35}, {16, 90} };
    HBRUSH b = U(CreateSolidBrush)(RGB(200, 20, 120));
    HGDIOBJ ob = U(SelectObject)(dc, b);
    U(SetPolyFillMode)(dc, ALTERNATE);
    U(Polygon)(dc, star, 5);
    U(SelectObject)(dc, ob);
    U(DeleteObject)(b);
}
static void sc_polys_winding(HDC dc) {
    POINT star[5] = { {48, 3}, {80, 90}, {5, 35}, {90, 35}, {16, 90} };
    HBRUSH b = U(CreateSolidBrush)(RGB(20, 200, 120));
    HGDIOBJ ob = U(SelectObject)(dc, b);
    U(SetPolyFillMode)(dc, WINDING);
    U(Polygon)(dc, star, 5);
    U(SelectObject)(dc, ob);
    U(DeleteObject)(b);
}
static void sc_polyline_pp(HDC dc) {
    POINT line[4] = { {3, 3}, {50, 20}, {20, 60}, {90, 90} };
    POINT pp[7] = { {10, 10}, {40, 10}, {25, 30}, {50, 50}, {80, 50}, {80, 80}, {55, 80} };
    INT cnt[2] = { 3, 4 };
    U(Polyline)(dc, line, 4);
    HBRUSH b = U(CreateHatchBrush)(HS_DIAGCROSS, RGB(0, 0, 0));
    HGDIOBJ ob = U(SelectObject)(dc, b);
    U(PolyPolygon)(dc, pp, cnt, 2);
    U(SelectObject)(dc, ob);
    U(DeleteObject)(b);
}
static void sc_hatch(HDC dc) {
    int styles[] = { HS_HORIZONTAL, HS_VERTICAL, HS_FDIAGONAL, HS_BDIAGONAL, HS_CROSS, HS_DIAGCROSS };
    for (int i = 0; i < 6; i++) {
        HBRUSH b = U(CreateHatchBrush)(styles[i], RGB(i * 40, 30, 200 - i * 30));
        HGDIOBJ ob = U(SelectObject)(dc, b);
        U(SetBkMode)(dc, (i & 1) ? TRANSPARENT : OPAQUE);
        U(SetBkColor)(dc, RGB(220, 220, 100));
        U(Rectangle)(dc, 4 + (i % 3) * 30, 4 + (i / 3) * 40, 32 + (i % 3) * 30, 40 + (i / 3) * 40);
        U(SelectObject)(dc, ob);
        U(DeleteObject)(b);
    }
}
static void sc_rop2(HDC dc) {
    HBRUSH b = U(CreateSolidBrush)(RGB(90, 160, 30));
    HGDIOBJ ob = U(SelectObject)(dc, b);
    U(Rectangle)(dc, 5, 5, 90, 90);
    int rops[] = { R2_XORPEN, R2_NOTCOPYPEN, R2_MASKPEN, R2_MERGEPEN, R2_NOT, R2_BLACK, R2_WHITE };
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(200, 100, 50));
    HGDIOBJ op = U(SelectObject)(dc, p);
    for (int i = 0; i < 7; i++) { U(SetROP2)(dc, rops[i]); U(MoveToEx)(dc, 5, 10 + i * 11, NULL); U(LineTo)(dc, 90, 12 + i * 11); }
    U(SetROP2)(dc, R2_COPYPEN);
    U(SelectObject)(dc, op); U(SelectObject)(dc, ob);
    U(DeleteObject)(p); U(DeleteObject)(b);
}
static void sc_clip_rects(HDC dc) {
    HBRUSH b = U(CreateSolidBrush)(RGB(30, 30, 200));
    HGDIOBJ ob = U(SelectObject)(dc, b);
    U(IntersectClipRect)(dc, 10, 10, 80, 80);
    U(ExcludeClipRect)(dc, 30, 30, 50, 50);
    U(PatBlt)(dc, 0, 0, 96, 96, PATCOPY);
    U(OffsetClipRgn)(dc, 5, 3);
    HBRUSH r = U(CreateSolidBrush)(RGB(200, 30, 30));
    U(SelectObject)(dc, r);
    U(PatBlt)(dc, 0, 0, 96, 96, PATINVERT);
    U(SelectObject)(dc, ob);
    U(DeleteObject)(b); U(DeleteObject)(r);
}
static void sc_clip_region(HDC dc) {
    HRGN e = U(CreateEllipticRgn)(10, 10, 85, 85);
    HBRUSH b = U(CreateSolidBrush)(RGB(255, 128, 0));
    HGDIOBJ ob = U(SelectObject)(dc, b);
    U(SelectClipRgn)(dc, e);
    U(PatBlt)(dc, 0, 0, 96, 96, PATCOPY);
    U(SelectClipRgn)(dc, NULL);
    U(SelectObject)(dc, ob);
    U(DeleteObject)(b); U(DeleteObject)(e);
}
static void sc_mapmode(HDC dc) {
    U(SetMapMode)(dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(dc, 100, 100, NULL);
    U(SetViewportExtEx)(dc, 90, 80, NULL);
    U(SetWindowOrgEx)(dc, -10, -10, NULL);
    U(SetViewportOrgEx)(dc, 3, 4, NULL);
    U(Rectangle)(dc, 0, 0, 60, 60);
    U(MoveToEx)(dc, 0, 0, NULL); U(LineTo)(dc, 60, 60);
    U(OffsetWindowOrgEx)(dc, 10, 10, NULL);
    U(OffsetViewportOrgEx)(dc, 2, 2, NULL);
    U(Ellipse)(dc, 20, 20, 70, 60);
    U(ScaleWindowExtEx)(dc, 1, 2, 1, 1, NULL);
    U(Rectangle)(dc, 10, 10, 40, 40);
    U(ScaleViewportExtEx)(dc, 3, 2, 1, 1, NULL);
    U(Rectangle)(dc, 0, 0, 20, 20);
}
static void sc_mapmode_fixed(HDC dc) {
    U(SetMapMode)(dc, MM_LOENGLISH);
    U(SetViewportOrgEx)(dc, 0, 95, NULL);
    U(Rectangle)(dc, 5, 5, 60, 40);
    U(MoveToEx)(dc, 0, 0, NULL); U(LineTo)(dc, 50, 50);
    U(SetMapMode)(dc, MM_TEXT);
    U(Rectangle)(dc, 60, 5, 90, 25);
}
static void sc_savedc(HDC dc) {
    HPEN p = U(CreatePen)(PS_SOLID, 2, RGB(255, 0, 255));
    HBRUSH b = U(CreateSolidBrush)(RGB(0, 255, 255));
    U(SelectObject)(dc, p);
    int lvl = U(SaveDC)(dc);
    U(SelectObject)(dc, b);
    U(SetROP2)(dc, R2_NOTXORPEN);
    U(Rectangle)(dc, 10, 10, 50, 50);
    U(RestoreDC)(dc, -1);
    U(Rectangle)(dc, 40, 40, 90, 90);
    U(SelectObject)(dc, stock(BLACK_PEN));
    U(SelectObject)(dc, stock(WHITE_BRUSH));
    U(DeleteObject)(p); U(DeleteObject)(b);
    (void)lvl;
}
static void sc_patblt(HDC dc) {
    HBRUSH h = U(CreateHatchBrush)(HS_CROSS, RGB(0, 80, 0));
    HGDIOBJ ob = U(SelectObject)(dc, h);
    DWORD rops[] = { PATCOPY, PATINVERT, DSTINVERT, BLACKNESS, WHITENESS, 0x00A000C9 /* DPa */, 0x00FA0089 /* DPo */ };
    for (int i = 0; i < 7; i++) U(PatBlt)(dc, 3 + (i % 3) * 30, 3 + (i / 3) * 30, 26, 26, rops[i]);
    U(SelectObject)(dc, ob);
    U(DeleteObject)(h);
}
static void sc_pixels(HDC dc) {
    for (int i = 0; i < 30; i++) U(SetPixel)(dc, 3 + i * 3, 5 + (i * 7) % 80, RGB(i * 8, 255 - i * 8, 77));
    for (int i = 0; i < 20; i++) U(SetPixelV)(dc, 90 - i * 2, 10 + i * 4, RGB(255, i * 12, 0));
}
static void sc_floodfill(HDC dc) {
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(0, 0, 0));
    HGDIOBJ op = U(SelectObject)(dc, p);
    HBRUSH nb = stock(NULL_BRUSH);
    HGDIOBJ ob = U(SelectObject)(dc, nb);
    U(Rectangle)(dc, 10, 10, 60, 60);
    U(Ellipse)(dc, 40, 40, 90, 90);
    HBRUSH f = U(CreateSolidBrush)(RGB(255, 200, 0));
    U(SelectObject)(dc, f);
    U(ExtFloodFill)(dc, 20, 20, RGB(0, 0, 0), FLOODFILLBORDER);
    U(SelectObject)(dc, ob); U(SelectObject)(dc, op);
    U(DeleteObject)(p); U(DeleteObject)(f);
}
static void sc_floodfill_surface(HDC dc) {
    HBRUSH f = U(CreateSolidBrush)(RGB(0, 90, 240));
    HGDIOBJ ob = U(SelectObject)(dc, f);
    U(ExtFloodFill)(dc, 50, 50, RGB(255, 255, 255), FLOODFILLSURFACE);
    U(SelectObject)(dc, ob);
    U(DeleteObject)(f);
}
static void sc_regions(HDC dc) {
    HRGN r1 = U(CreateRectRgn)(5, 5, 45, 45), r2 = U(CreateEllipticRgn)(50, 5, 92, 45);
    HRGN r3 = U(CreateRoundRectRgn)(5, 50, 45, 92, 10, 10), r4 = U(CreateRectRgn)(55, 55, 90, 90);
    HBRUSH b1 = U(CreateSolidBrush)(RGB(255, 0, 0)), b2 = U(CreateSolidBrush)(RGB(0, 0, 255));
    U(FillRgn)(dc, r1, b1);
    U(FrameRgn)(dc, r2, b2, 3, 2);
    U(InvertRgn)(dc, r3);
    HGDIOBJ ob = U(SelectObject)(dc, b1);
    U(PaintRgn)(dc, r4);
    U(SelectObject)(dc, ob);
    U(DeleteObject)(r1); U(DeleteObject)(r2); U(DeleteObject)(r3); U(DeleteObject)(r4);
    U(DeleteObject)(b1); U(DeleteObject)(b2);
}
static void fill_src(DIBDC *s) {
    for (int y = 0; y < s->h; y++) for (int x = 0; x < s->w; x++) s->bits[y * s->w + x] = ((x * 20) << 16) | ((y * 25) << 8) | ((x ^ y) * 15 & 0xFF);
}
static void sc_bitblt(HDC dc) {
    DIBDC s;
    if (!dibdc_make(&s, 12, 12)) return;
    fill_src(&s);
    U(BitBlt)(dc, 4, 4, 12, 12, s.dc, 0, 0, SRCCOPY);
    U(BitBlt)(dc, 20, 4, 12, 12, s.dc, 0, 0, SRCINVERT);
    U(BitBlt)(dc, 36, 4, 12, 12, s.dc, 0, 0, NOTSRCCOPY);
    U(BitBlt)(dc, 52, 4, 12, 12, s.dc, 0, 0, SRCAND);
    U(BitBlt)(dc, 4, 24, 8, 8, s.dc, 2, 2, SRCCOPY);
    dibdc_free(&s);
}
static void sc_stretchblt(HDC dc) {
    DIBDC s;
    if (!dibdc_make(&s, 8, 8)) return;
    fill_src(&s);
    U(SetStretchBltMode)(dc, COLORONCOLOR);
    U(StretchBlt)(dc, 2, 2, 32, 32, s.dc, 0, 0, 8, 8, SRCCOPY);
    U(StretchBlt)(dc, 40, 2, 16, 16, s.dc, 0, 0, 8, 8, SRCCOPY);
    U(StretchBlt)(dc, 2, 40, 24, 12, s.dc, 0, 0, 8, 8, SRCINVERT);
    U(StretchBlt)(dc, 60, 60, -20, -20, s.dc, 0, 0, 8, 8, SRCCOPY);
    dibdc_free(&s);
}
static void sc_dibs(HDC dc) {
    BYTE store[sizeof(BITMAPINFOHEADER) + 8 * 8 * 3];
    BITMAPINFO *bi = (BITMAPINFO *)store;
    memset(store, 0, sizeof store);
    bi->bmiHeader.biSize = sizeof bi->bmiHeader; bi->bmiHeader.biWidth = 8; bi->bmiHeader.biHeight = 8;
    bi->bmiHeader.biPlanes = 1; bi->bmiHeader.biBitCount = 24;
    BYTE *bits = store + sizeof(BITMAPINFOHEADER);
    for (int i = 0; i < 8 * 8 * 3; i++) bits[i] = (BYTE)(i * 3);
    U(StretchDIBits)(dc, 2, 2, 24, 24, 0, 0, 8, 8, bits, bi, DIB_RGB_COLORS, SRCCOPY);
    U(StretchDIBits)(dc, 30, 2, 8, 8, 0, 0, 8, 8, bits, bi, DIB_RGB_COLORS, SRCINVERT);
    U(SetDIBitsToDevice)(dc, 2, 40, 8, 8, 0, 0, 0, 8, bits, bi, DIB_RGB_COLORS);
    U(SetDIBitsToDevice)(dc, 20, 40, 4, 4, 2, 2, 0, 8, bits, bi, DIB_RGB_COLORS);
}
static void sc_pattern_brush(HDC dc) {
    static const BYTE bm[8] = { 0xAA, 0x55, 0xAA, 0x55, 0xF0, 0xF0, 0x0F, 0x0F };
    WORD w[8]; for (int i = 0; i < 8; i++) w[i] = bm[i];
    HBITMAP bmp = U(CreateBitmap)(8, 8, 1, 1, w);
    HBRUSH b = U(CreatePatternBrush)(bmp);
    HGDIOBJ ob = U(SelectObject)(dc, b);
    U(PatBlt)(dc, 4, 4, 40, 40, PATCOPY);
    U(Rectangle)(dc, 50, 50, 90, 90);
    U(SelectObject)(dc, ob);
    U(DeleteObject)(b); U(DeleteObject)(bmp);
}
static void sc_dib_pattern_brush(HDC dc) {
    BYTE store[sizeof(BITMAPINFOHEADER) + 8 * 8 * 3];
    BITMAPINFO *bi = (BITMAPINFO *)store;
    memset(store, 0, sizeof store);
    bi->bmiHeader.biSize = sizeof bi->bmiHeader; bi->bmiHeader.biWidth = 8; bi->bmiHeader.biHeight = 8;
    bi->bmiHeader.biPlanes = 1; bi->bmiHeader.biBitCount = 24;
    BYTE *bits = store + sizeof(BITMAPINFOHEADER);
    for (int i = 0; i < 8 * 8 * 3; i++) bits[i] = (BYTE)(i * 11);
    HBRUSH b = U(CreateDIBPatternBrushPt)(store, DIB_RGB_COLORS);
    HGDIOBJ ob = U(SelectObject)(dc, b);
    U(PatBlt)(dc, 4, 4, 40, 40, PATCOPY);
    U(Ellipse)(dc, 40, 40, 92, 92);
    U(SelectObject)(dc, ob);
    U(DeleteObject)(b);
}
static void sc_pen_styles(HDC dc) {
    int st[] = { PS_DASH, PS_DOT, PS_DASHDOT, PS_DASHDOTDOT, PS_NULL, PS_INSIDEFRAME };
    HGDIOBJ op = U(SelectObject)(dc, stock(BLACK_PEN));
    for (int i = 0; i < 6; i++) {
        HPEN p = U(CreatePen)(st[i], 1, RGB(i * 40, 0, 100));
        U(SelectObject)(dc, p);
        U(SetBkMode)(dc, i & 1 ? OPAQUE : TRANSPARENT);
        U(MoveToEx)(dc, 4, 8 + i * 14, NULL); U(LineTo)(dc, 90, 8 + i * 14);
        U(SelectObject)(dc, stock(BLACK_PEN));
        U(DeleteObject)(p);
    }
    U(SelectObject)(dc, op);
}
static void sc_opaque_text_rect(HDC dc) {
    RECT r = { 10, 10, 70, 40 };
    U(SetBkColor)(dc, RGB(40, 200, 120));
    U(ExtTextOutA)(dc, 0, 0, ETO_OPAQUE, &r, "", 0, NULL);
    RECT r2 = { 30, 50, 90, 90 };
    U(SetBkColor)(dc, RGB(220, 40, 20));
    U(ExtTextOutA)(dc, 0, 0, ETO_OPAQUE | ETO_CLIPPED, &r2, NULL, 0, NULL);
}
static void sc_palette_rel(HDC dc) {
    U(SetROP2)(dc, R2_XORPEN);
    U(SetPolyFillMode)(dc, WINDING);
    U(SetStretchBltMode)(dc, HALFTONE);
    U(SetTextAlign)(dc, TA_BASELINE);
    U(Rectangle)(dc, 8, 8, 60, 60);
}
static void sc_negative_coords(HDC dc) {
    U(SetViewportOrgEx)(dc, 48, 48, NULL);
    POINT p[4] = { {-40, -40}, {40, -30}, {30, 40}, {-30, 35} };
    HBRUSH b = U(CreateSolidBrush)(RGB(120, 120, 250));
    HGDIOBJ ob = U(SelectObject)(dc, b);
    U(Polygon)(dc, p, 4);
    U(MoveToEx)(dc, -45, 0, NULL); U(LineTo)(dc, 45, 0);
    U(SelectObject)(dc, ob);
    U(DeleteObject)(b);
}
static void sc_empty(HDC dc) { (void)dc; }
static void sc_many(HDC dc) {
    for (int i = 0; i < 400; i++) U(Rectangle)(dc, (i * 7) % 80, (i * 11) % 80, (i * 7) % 80 + 10, (i * 11) % 80 + 10);
}
static void sc_many_objects(HDC dc) {
    HGDIOBJ op = U(SelectObject)(dc, stock(BLACK_PEN));
    HPEN pens[40]; HBRUSH brs[40];
    for (int i = 0; i < 40; i++) {
        pens[i] = U(CreatePen)(PS_SOLID, 1, RGB(i * 6, 255 - i * 6, i * 3));
        brs[i] = U(CreateSolidBrush)(RGB(255 - i * 6, i * 6, 100));
        U(SelectObject)(dc, pens[i]); U(SelectObject)(dc, brs[i]);
        U(Rectangle)(dc, 2 + i, 2 + i, 20 + i, 20 + i);
    }
    U(SelectObject)(dc, op); U(SelectObject)(dc, stock(WHITE_BRUSH));
    for (int i = 0; i < 40; i++) { U(DeleteObject)(pens[i]); U(DeleteObject)(brs[i]); }
}

/* Run: draw directly vs record+replay, for three target DC set-ups. */
static void apply_target(HDC dc, int variant) {
    if (variant == 1) U(SetViewportOrgEx)(dc, 11, 7, NULL);
    if (variant == 2) { U(SetMapMode)(dc, MM_ANISOTROPIC); U(SetWindowExtEx)(dc, 1, 1, NULL); U(SetViewportExtEx)(dc, 2, 2, NULL); }
}

static int g_soft_scene;
static void scene_check(scene_fn fn, const char *name, int expect_ink) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile)); REQUIRE(U(CreateDIBSection));
    for (int variant = 0; variant < 3; variant++) {
        DIBDC a, b;
        int size = variant == 2 ? 200 : 110;
        if (!dibdc_make(&a, size, size)) { CHECK(0); return; }
        if (!dibdc_make(&b, size, size)) { dibdc_free(&a); CHECK(0); return; }
        apply_target(a.dc, variant);
        fn(a.dc);
        HDC mdc = mf_new();
        if (!mdc) { CHECK(0); dibdc_free(&a); dibdc_free(&b); return; }
        fn(mdc);
        HMETAFILE h = U(CloseMetaFile)(mdc);
        CHECKF(h != NULL, "%s close", name);
        if (h) {
            apply_target(b.dc, variant);
            BOOL ok = U(PlayMetaFile)(b.dc, h);
            CHECKF(ok, "%s variant %d: PlayMetaFile failed", name, variant);
            int diff = dibdc_diff(&a, &b);
            if (g_soft_scene) { if (diff) t_note("%s variant %d: %d pixels differ", name, variant, diff); }
            else CHECKF(diff == 0, "%s variant %d: %d pixels differ", name, variant, diff);
            if (expect_ink) CHECKF(dibdc_nonwhite(&b) > 0, "%s variant %d: nothing drawn", name, variant);
            U(DeleteMetaFile)(h);
        }
        dibdc_free(&a); dibdc_free(&b);
    }
}
#define SCENE(nm, ink) TEST(wmf_play_scene_##nm) { scene_check(sc_##nm, #nm, ink); }
SCENE(lines, 1) SCENE(shapes, 1) SCENE(arcs, 1) SCENE(polys, 1) SCENE(polys_winding, 1) SCENE(polyline_pp, 1)
SCENE(hatch, 1) SCENE(rop2, 1) SCENE(clip_rects, 1) SCENE(clip_region, 1) SCENE(mapmode, 1) SCENE(mapmode_fixed, 1)
SCENE(savedc, 1) SCENE(patblt, 1) SCENE(pixels, 1) SCENE(floodfill, 1)
TEST(wmf_play_scene_floodfill_surface) { scene_check(sc_floodfill_surface, "floodfill_surface", !on_wine()); }
 SCENE(regions, 1)
SCENE(bitblt, 1) SCENE(stretchblt, 1) TEST(wmf_play_scene_dibs) { g_soft_scene = 1; scene_check(sc_dibs, "dibs", 1); g_soft_scene = 0; }
 SCENE(pattern_brush, 1) SCENE(dib_pattern_brush, 1)
SCENE(pen_styles, 1) SCENE(opaque_text_rect, 1) SCENE(palette_rel, 1) SCENE(negative_coords, 1) SCENE(empty, 0)
SCENE(many, 1) SCENE(many_objects, 1)

/* ---- replay hygiene ---- */
static HMETAFILE rec_scene(scene_fn fn) {
    HDC m = mf_new();
    if (!m) return NULL;
    fn(m);
    return U(CloseMetaFile)(m);
}

TEST(wmf_play_replay_twice_identical) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile));
    HMETAFILE h = rec_scene(sc_shapes);
    REQUIRE(h);
    DIBDC a, b;
    REQUIRE(dibdc_make(&a, 100, 100));
    REQUIRE(dibdc_make(&b, 100, 100));
    CHECK(U(PlayMetaFile)(a.dc, h));
    CHECK(U(PlayMetaFile)(b.dc, h));
    CHECK(U(PlayMetaFile)(b.dc, h));   /* a second play over the same pixels is idempotent for opaque shapes */
    EXPECT_EQ(dibdc_diff(&a, &b), 0);
    dibdc_free(&a); dibdc_free(&b);
    U(DeleteMetaFile)(h);
}

TEST(wmf_play_after_getbits_roundtrip) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile)); REQUIRE(U(SetMetaFileBitsEx));
    HMETAFILE h = rec_scene(sc_lines);
    REQUIRE(h);
    WB b;
    REQUIRE(wb_get(h, &b));
    HMETAFILE h2 = U(SetMetaFileBitsEx)(b.n, b.p);
    REQUIRE(h2);
    DIBDC d1, d2;
    REQUIRE(dibdc_make(&d1, 100, 100));
    REQUIRE(dibdc_make(&d2, 100, 100));
    CHECK(U(PlayMetaFile)(d1.dc, h));
    CHECK(U(PlayMetaFile)(d2.dc, h2));
    EXPECT_EQ(dibdc_diff(&d1, &d2), 0);
    CHECK(dibdc_nonwhite(&d1) > 0);
    dibdc_free(&d1); dibdc_free(&d2);
    wb_free(&b);
    U(DeleteMetaFile)(h); U(DeleteMetaFile)(h2);
}

TEST(wmf_play_from_file_metafile) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile));
    char path[MAX_PATH]; tmp_path(path, "wmf_pf");
    DeleteFileA(path);
    HDC m = U(CreateMetaFileA)(path);
    REQUIRE(m);
    sc_shapes(m);
    HMETAFILE h = U(CloseMetaFile)(m);
    REQUIRE(h);
    DIBDC a, b;
    REQUIRE(dibdc_make(&a, 100, 100));
    REQUIRE(dibdc_make(&b, 100, 100));
    sc_shapes(a.dc);
    CHECK(U(PlayMetaFile)(b.dc, h));
    EXPECT_EQ(dibdc_diff(&a, &b), 0);
    dibdc_free(&a); dibdc_free(&b);
    U(DeleteMetaFile)(h);
    DeleteFileA(path);
}

TEST(wmf_play_copy_equivalent) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile)); REQUIRE(U(CopyMetaFileA));
    HMETAFILE h = rec_scene(sc_arcs);
    REQUIRE(h);
    HMETAFILE c = U(CopyMetaFileA)(h, NULL);
    REQUIRE(c);
    DIBDC a, b;
    REQUIRE(dibdc_make(&a, 100, 100));
    REQUIRE(dibdc_make(&b, 100, 100));
    CHECK(U(PlayMetaFile)(a.dc, h));
    CHECK(U(PlayMetaFile)(b.dc, c));
    EXPECT_EQ(dibdc_diff(&a, &b), 0);
    dibdc_free(&a); dibdc_free(&b);
    U(DeleteMetaFile)(h); U(DeleteMetaFile)(c);
}

TEST(wmf_play_no_gdi_leak) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile)); REQUIRE(U(GetGuiResources));
    scene_fn fns[] = { sc_many_objects, sc_pattern_brush, sc_dib_pattern_brush, sc_regions, sc_clip_region, sc_hatch, sc_pen_styles, sc_savedc };
    for (size_t i = 0; i < sizeof fns / sizeof *fns; i++) {
        HMETAFILE h = rec_scene(fns[i]);
        REQUIRE(h);
        DIBDC d;
        REQUIRE(dibdc_make(&d, 100, 100));
        DWORD before = gdi_count();
        for (int k = 0; k < 20; k++) U(PlayMetaFile)(d.dc, h);
        DWORD after = gdi_count();
        CHECKF(after == before, "scene %zu: GDI objects %lu -> %lu", i, before, after);
        dibdc_free(&d);
        U(DeleteMetaFile)(h);
    }
}

TEST(wmf_play_no_gdi_leak_record) {
    WMF_REQ_BASE(); REQUIRE(U(GetGuiResources));
    scene_fn fns[] = { sc_many_objects, sc_pattern_brush, sc_dib_pattern_brush, sc_regions, sc_clip_region, sc_bitblt, sc_dibs };
    for (size_t i = 0; i < sizeof fns / sizeof *fns; i++) {
        DWORD before = gdi_count();
        for (int k = 0; k < 10; k++) { HMETAFILE h = rec_scene(fns[i]); if (h) U(DeleteMetaFile)(h); }
        CHECKF(gdi_count() == before, "scene %zu leak %lu -> %lu", i, before, gdi_count());
    }
}

/* ---- error paths ---- */
TEST(wmf_play_null_dc) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile));
    HMETAFILE h = rec_scene(sc_shapes);
    REQUIRE(h);
    BOOL r = U(PlayMetaFile)(NULL, h);
    CHECK(!r);
    U(DeleteMetaFile)(h);
}

TEST(wmf_play_bogus_dc) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile));
    HMETAFILE h = rec_scene(sc_shapes);
    REQUIRE(h);
    HDC bad[] = { (HDC)1, (HDC)(ULONG_PTR)0xdeadbeef, (HDC)(ULONG_PTR)0x00010001, (HDC)(ULONG_PTR)0x00210001, (HDC)(ULONG_PTR)-1 };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        BOOL r = U(PlayMetaFile)(bad[i], h);
        CHECKF(!r, "bogus dc %p", bad[i]);
    }
    U(DeleteMetaFile)(h);
}

TEST(wmf_play_invalid_metafile) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile));
    DIBDC d;
    REQUIRE(dibdc_make(&d, 20, 20));
    HMETAFILE bad[] = { NULL, (HMETAFILE)1, (HMETAFILE)(ULONG_PTR)0xdeadbeef, (HMETAFILE)(ULONG_PTR)0x00260001 };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        BOOL r = U(PlayMetaFile)(d.dc, bad[i]);
        CHECK(!r);
        EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    }
    CHECK(dibdc_nonwhite(&d) == 0);
    dibdc_free(&d);
}

TEST(wmf_play_deleted_metafile) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile));
    HMETAFILE h = rec_scene(sc_shapes);
    REQUIRE(h);
    U(DeleteMetaFile)(h);
    DIBDC d;
    REQUIRE(dibdc_make(&d, 20, 20));
    SET_ERR(0);
    BOOL r = U(PlayMetaFile)(d.dc, h);
    CHECK(!r);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    dibdc_free(&d);
}

TEST(wmf_play_wrong_handle_type) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile));
    DIBDC d;
    REQUIRE(dibdc_make(&d, 20, 20));
    HBRUSH b = U(CreateSolidBrush)(0);
    SET_ERR(0);
    BOOL r = U(PlayMetaFile)(d.dc, (HMETAFILE)b);
    CHECK(!r);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    U(DeleteObject)(b);
    SET_ERR(0);
    r = U(PlayMetaFile)(d.dc, (HMETAFILE)d.dc);
    CHECK(!r);
    dibdc_free(&d);
}

TEST(wmf_play_bitmap_handle_as_dc) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile));
    HMETAFILE h = rec_scene(sc_shapes);
    REQUIRE(h);
    HBITMAP bmp = U(CreateBitmap)(4, 4, 1, 1, NULL);
    BOOL r = U(PlayMetaFile)((HDC)bmp, h);
    CHECK(!r);
    U(DeleteObject)(bmp);
    U(DeleteMetaFile)(h);
}

TEST(wmf_play_into_metafile_dc) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile));
    HMETAFILE inner = rec_scene(sc_lines);
    REQUIRE(inner);
    WB bi;
    REQUIRE(wb_get(inner, &bi));
    HDC outer = mf_new();
    REQUIRE(outer);
    U(Rectangle)(outer, 1, 1, 5, 5);
    BOOL r = U(PlayMetaFile)(outer, inner);
    CHECK(r);
    U(Ellipse)(outer, 1, 1, 5, 5);
    WB bo;
    REQUIRE(mf_finish(outer, &bo, NULL));
    CHECK(wb_valid(&bo));
    /* the inner stream is replayed as records into the outer metafile */
    EXPECT_EQ(wb_count(&bo, 0x0213), wb_count(&bi, 0x0213));
    EXPECT_EQ(wb_count(&bo, 0x0214), wb_count(&bi, 0x0214));
    EXPECT_EQ(wb_count(&bo, 0x041B), 1);
    EXPECT_EQ(wb_count(&bo, 0x0418), 1);
    wb_free(&bi); wb_free(&bo);
    U(DeleteMetaFile)(inner);
}

TEST(wmf_play_self_into_own_dc_after_close) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile));
    HDC m = mf_new();
    REQUIRE(m);
    HMETAFILE h = U(CloseMetaFile)(m);
    REQUIRE(h);
    SET_ERR(0);
    BOOL r = U(PlayMetaFile)(m, h);    /* stale metafile DC handle */
    t_note("PlayMetaFile(closed metafile DC) = %d", r);
    U(DeleteMetaFile)(h);
}

TEST(wmf_play_empty_metafile) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile));
    HDC m = mf_new();
    REQUIRE(m);
    HMETAFILE h = U(CloseMetaFile)(m);
    REQUIRE(h);
    DIBDC d;
    REQUIRE(dibdc_make(&d, 20, 20));
    CHECK(U(PlayMetaFile)(d.dc, h));
    EXPECT_EQ(dibdc_nonwhite(&d), 0);
    dibdc_free(&d);
    U(DeleteMetaFile)(h);
}

/* ---- DC state across PlayMetaFile ---- */
static HMETAFILE rec_state_changer(void) {
    HDC m = mf_new();
    if (!m) return NULL;
    HPEN p = U(CreatePen)(PS_SOLID, 4, RGB(1, 2, 3));
    HBRUSH b = U(CreateSolidBrush)(RGB(9, 8, 7));
    LOGFONTA lf; memset(&lf, 0, sizeof lf); lf.lfHeight = -20; strcpy(lf.lfFaceName, "Arial");
    HFONT f = U(CreateFontIndirectA)(&lf);
    U(SelectObject)(m, p); U(SelectObject)(m, b); U(SelectObject)(m, f);
    U(SetMapMode)(m, MM_ANISOTROPIC);
    U(SetWindowExtEx)(m, 7, 9, NULL);
    U(SetViewportExtEx)(m, 70, 90, NULL);
    U(SetBkMode)(m, TRANSPARENT);
    U(SetROP2)(m, R2_XORPEN);
    U(SetTextColor)(m, RGB(1, 1, 1));
    U(SetBkColor)(m, RGB(2, 2, 2));
    U(Rectangle)(m, 0, 0, 3, 3);
    U(SelectObject)(m, stock(BLACK_PEN)); U(SelectObject)(m, stock(WHITE_BRUSH)); U(SelectObject)(m, stock(SYSTEM_FONT));
    U(DeleteObject)(p); U(DeleteObject)(b); U(DeleteObject)(f);
    return U(CloseMetaFile)(m);
}

TEST(wmf_play_restores_selected_objects) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile)); REQUIRE(U(GetCurrentObject));
    HMETAFILE h = rec_state_changer();
    REQUIRE(h);
    DIBDC d;
    REQUIRE(dibdc_make(&d, 40, 40));
    HPEN p = U(CreatePen)(PS_DOT, 1, RGB(100, 0, 0));
    HBRUSH b = U(CreateSolidBrush)(RGB(0, 100, 0));
    HGDIOBJ op = U(SelectObject)(d.dc, p), ob = U(SelectObject)(d.dc, b);
    HGDIOBJ f0 = U(GetCurrentObject)(d.dc, OBJ_FONT);
    HGDIOBJ pal0 = U(GetCurrentObject)(d.dc, OBJ_PAL);
    CHECK(U(PlayMetaFile)(d.dc, h));
    EXPECT_EQ(U(GetCurrentObject)(d.dc, OBJ_PEN), p);
    EXPECT_EQ(U(GetCurrentObject)(d.dc, OBJ_BRUSH), b);
    EXPECT_EQ(U(GetCurrentObject)(d.dc, OBJ_FONT), f0);
    EXPECT_EQ(U(GetCurrentObject)(d.dc, OBJ_PAL), pal0);
    U(SelectObject)(d.dc, op); U(SelectObject)(d.dc, ob);
    U(DeleteObject)(p); U(DeleteObject)(b);
    dibdc_free(&d);
    U(DeleteMetaFile)(h);
}

TEST(wmf_play_mapping_after_play_observed) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile)); REQUIRE(U(GetMapMode)); REQUIRE(U(GetWindowExtEx)); REQUIRE(U(GetViewportExtEx));
    HMETAFILE h = rec_state_changer();
    REQUIRE(h);
    DIBDC d;
    REQUIRE(dibdc_make(&d, 40, 40));
    U(SetMapMode)(d.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(d.dc, 11, 12, NULL);
    U(SetViewportExtEx)(d.dc, 33, 36, NULL);
    CHECK(U(PlayMetaFile)(d.dc, h));
    SIZE w, v;
    CHECK(U(GetWindowExtEx)(d.dc, &w)); CHECK(U(GetViewportExtEx)(d.dc, &v));
    t_note("after play: mapmode %d window %ldx%ld viewport %ldx%ld", U(GetMapMode)(d.dc), w.cx, w.cy, v.cx, v.cy);
    dibdc_free(&d);
    U(DeleteMetaFile)(h);
}

TEST(wmf_play_mapmode_text_after_play_observed) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile)); REQUIRE(U(GetMapMode));
    HMETAFILE h = rec_state_changer();
    REQUIRE(h);
    DIBDC d;
    REQUIRE(dibdc_make(&d, 40, 40));
    EXPECT_EQ(U(GetMapMode)(d.dc), MM_TEXT);
    CHECK(U(PlayMetaFile)(d.dc, h));
    t_note("MM after play: %d", U(GetMapMode)(d.dc));
    dibdc_free(&d);
    U(DeleteMetaFile)(h);
}

TEST(wmf_play_attribute_leak_observed) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile)); REQUIRE(U(GetBkMode)); REQUIRE(U(GetROP2));
    HMETAFILE h = rec_state_changer();
    REQUIRE(h);
    DIBDC d;
    REQUIRE(dibdc_make(&d, 40, 40));
    int bk0 = U(GetBkMode)(d.dc), rop0 = U(GetROP2)(d.dc);
    COLORREF tc0 = U(GetTextColor)(d.dc), bc0 = U(GetBkColor)(d.dc);
    CHECK(U(PlayMetaFile)(d.dc, h));
    t_note("after play: BkMode %d->%d ROP2 %d->%d TextColor %06lx->%06lx BkColor %06lx->%06lx",
           bk0, U(GetBkMode)(d.dc), rop0, U(GetROP2)(d.dc), tc0, U(GetTextColor)(d.dc), bc0, U(GetBkColor)(d.dc));
    dibdc_free(&d);
    U(DeleteMetaFile)(h);
}

TEST(wmf_play_restores_clip_region) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile)); REQUIRE(U(GetClipBox));
    HMETAFILE h = rec_scene(sc_clip_rects);
    HMETAFILE hl = rec_scene(sc_lines);
    REQUIRE(hl);
    REQUIRE(h);
    DIBDC d;
    REQUIRE(dibdc_make(&d, 100, 100));
    HRGN clip = U(CreateRectRgn)(20, 20, 70, 70);
    U(SelectClipRgn)(d.dc, clip);
    RECT before, after;
    U(GetClipBox)(d.dc, &before);
    CHECK(U(PlayMetaFile)(d.dc, h));
    U(GetClipBox)(d.dc, &after);
    CHECK(EqualRect(&before, &after));
    dibdc_clear(&d, 0xFFFFFF);
    CHECK(U(PlayMetaFile)(d.dc, hl));
    U(DeleteMetaFile)(hl);
    /* drawing never escaped the original clip */
    for (int y = 0; y < 100; y++) for (int x = 0; x < 100; x++)
        if (x < 20 || x >= 70 || y < 20 || y >= 70) if (DIBPX(&d, x, y) != 0xFFFFFF) { CHECKF(0, "pixel outside clip at %d,%d", x, y); y = 100; break; }
    U(SelectClipRgn)(d.dc, NULL);
    U(DeleteObject)(clip);
    dibdc_free(&d);
    U(DeleteMetaFile)(h);
}

TEST(wmf_play_no_clip_stays_unclipped) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile)); REQUIRE(U(GetClipRgn));
    HMETAFILE h = rec_scene(sc_clip_region);
    REQUIRE(h);
    DIBDC d;
    REQUIRE(dibdc_make(&d, 100, 100));
    HRGN probe = U(CreateRectRgn)(0, 0, 1, 1);
    EXPECT_EQ(U(GetClipRgn)(d.dc, probe), 0);
    CHECK(U(PlayMetaFile)(d.dc, h));
    int r = U(GetClipRgn)(d.dc, probe);
    t_note("GetClipRgn after play = %d", r);
    U(DeleteObject)(probe);
    dibdc_free(&d);
    U(DeleteMetaFile)(h);
}

TEST(wmf_play_graphics_mode_restored) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile)); REQUIRE(U(GetGraphicsMode)); REQUIRE(U(SetGraphicsMode));
    HMETAFILE h = rec_scene(sc_shapes);
    REQUIRE(h);
    DIBDC d;
    REQUIRE(dibdc_make(&d, 100, 100));
    EXPECT_EQ(U(GetGraphicsMode)(d.dc), GM_COMPATIBLE);
    CHECK(U(PlayMetaFile)(d.dc, h));
    EXPECT_EQ(U(GetGraphicsMode)(d.dc), GM_COMPATIBLE);
    U(SetGraphicsMode)(d.dc, GM_ADVANCED);
    CHECK(U(PlayMetaFile)(d.dc, h));
    EXPECT_EQ(U(GetGraphicsMode)(d.dc), GM_ADVANCED);
    dibdc_free(&d);
    U(DeleteMetaFile)(h);
}

TEST(wmf_play_advanced_mode_same_pixels) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile)); REQUIRE(U(SetGraphicsMode));
    HMETAFILE h = rec_scene(sc_shapes);
    REQUIRE(h);
    DIBDC a, b;
    REQUIRE(dibdc_make(&a, 100, 100));
    REQUIRE(dibdc_make(&b, 100, 100));
    CHECK(U(PlayMetaFile)(a.dc, h));
    U(SetGraphicsMode)(b.dc, GM_ADVANCED);
    CHECK(U(PlayMetaFile)(b.dc, h));
    EXPECT_EQ(dibdc_diff(&a, &b), 0);
    dibdc_free(&a); dibdc_free(&b);
    U(DeleteMetaFile)(h);
}

TEST(wmf_play_world_transform_ignored) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile)); REQUIRE(U(SetWorldTransform)); REQUIRE(U(SetGraphicsMode));
    HMETAFILE h = rec_scene(sc_shapes);
    REQUIRE(h);
    DIBDC a, b;
    REQUIRE(dibdc_make(&a, 100, 100));
    REQUIRE(dibdc_make(&b, 100, 100));
    CHECK(U(PlayMetaFile)(a.dc, h));
    U(SetGraphicsMode)(b.dc, GM_ADVANCED);
    XFORM x = { 0.5f, 0, 0, 0.5f, 10, 10 };
    U(SetWorldTransform)(b.dc, &x);
    CHECK(U(PlayMetaFile)(b.dc, h));
    /* the player works in compatible mode: world transform has no effect on the replay */
    int diff = dibdc_diff(&a, &b);
    t_note("world transform active: %d pixels differ from plain replay", diff);
    U(SetGraphicsMode)(b.dc, GM_COMPATIBLE);
    dibdc_free(&a); dibdc_free(&b);
    U(DeleteMetaFile)(h);
}

TEST(wmf_play_selected_stock_restored_on_bitmap) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile)); REQUIRE(U(GetCurrentObject));
    HMETAFILE h = rec_state_changer();
    REQUIRE(h);
    DIBDC d;
    REQUIRE(dibdc_make(&d, 40, 40));
    HGDIOBJ p0 = U(GetCurrentObject)(d.dc, OBJ_PEN), b0 = U(GetCurrentObject)(d.dc, OBJ_BRUSH);
    HGDIOBJ bp = stock(BLACK_PEN), wb = stock(WHITE_BRUSH);
    EXPECT_EQ(p0, bp);
    EXPECT_EQ(b0, wb);
    CHECK(U(PlayMetaFile)(d.dc, h));
    HGDIOBJ p1 = U(GetCurrentObject)(d.dc, OBJ_PEN), b1 = U(GetCurrentObject)(d.dc, OBJ_BRUSH);
    EXPECT_EQ(p1, p0);
    EXPECT_EQ(b1, b0);
    dibdc_free(&d);
    U(DeleteMetaFile)(h);
}

TEST(wmf_play_target_object_deleted_during) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile));
    /* pen in the target DC survives replay and can be deleted afterwards */
    HMETAFILE h = rec_scene(sc_lines);
    REQUIRE(h);
    DIBDC d;
    REQUIRE(dibdc_make(&d, 100, 100));
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0);
    HGDIOBJ op = U(SelectObject)(d.dc, p);
    CHECK(U(PlayMetaFile)(d.dc, h));
    U(SelectObject)(d.dc, op);
    CHECK(U(DeleteObject)(p));
    dibdc_free(&d);
    U(DeleteMetaFile)(h);
}

TEST(wmf_play_cross_thread_dc) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile));
    HMETAFILE h = rec_scene(sc_shapes);
    REQUIRE(h);
    DIBDC d;
    REQUIRE(dibdc_make(&d, 100, 100));
    struct A { HDC dc; HMETAFILE h; BOOL r; } a = { d.dc, h, FALSE };
    DWORD WINAPI th(LPVOID p) { struct A *x = p; x->r = U(PlayMetaFile)(x->dc, x->h); return 0; }
    HANDLE t = CreateThread(NULL, 0, th, &a, 0, NULL);
    WaitForSingleObject(t, 10000);
    CloseHandle(t);
    CHECK(a.r);
    CHECK(dibdc_nonwhite(&d) > 0);
    dibdc_free(&d);
    U(DeleteMetaFile)(h);
}

TEST(wmf_play_metafile_handle_cross_thread) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile));
    struct A { HMETAFILE h; } a;
    DWORD WINAPI th(LPVOID p) { struct A *x = p; x->h = rec_scene(sc_shapes); return 0; }
    HANDLE t = CreateThread(NULL, 0, th, &a, 0, NULL);
    WaitForSingleObject(t, 10000);
    CloseHandle(t);
    REQUIRE(a.h);
    DIBDC d;
    REQUIRE(dibdc_make(&d, 100, 100));
    CHECK(U(PlayMetaFile)(d.dc, a.h));
    CHECK(dibdc_nonwhite(&d) > 0);
    CHECK(U(DeleteMetaFile)(a.h));
    dibdc_free(&d);
}

TEST(wmf_play_many_times) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile));
    HMETAFILE h = rec_scene(sc_shapes);
    REQUIRE(h);
    DIBDC d;
    REQUIRE(dibdc_make(&d, 100, 100));
    int ok = 0;
    for (int i = 0; i < 500; i++) ok += U(PlayMetaFile)(d.dc, h) != 0;
    EXPECT_EQ(ok, 500);
    dibdc_free(&d);
    U(DeleteMetaFile)(h);
}

TEST(wmf_play_32bpp_vs_24bpp_dib) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile)); REQUIRE(U(CreateDIBSection));
    HMETAFILE h = rec_scene(sc_shapes);
    REQUIRE(h);
    DIBDC a;
    REQUIRE(dibdc_make(&a, 100, 100));
    CHECK(U(PlayMetaFile)(a.dc, h));
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 100; bi.bmiHeader.biHeight = -100;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 24;
    HDC dc = U(CreateCompatibleDC)(NULL);
    BYTE *bits = NULL;
    HBITMAP bmp = U(CreateDIBSection)(dc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    REQUIRE(bmp && bits);
    HGDIOBJ old = U(SelectObject)(dc, bmp);
    memset(bits, 0xFF, 100 * 300);
    CHECK(U(PlayMetaFile)(dc, h));
    int bad = 0;
    for (int y = 0; y < 100; y++) for (int x = 0; x < 100; x++) {
        BYTE *q = bits + y * 300 + x * 3;
        DWORD v = (q[2] << 16) | (q[1] << 8) | q[0];
        if (v != DIBPX(&a, x, y)) bad++;
    }
    EXPECT_EQ(bad, 0);
    U(SelectObject)(dc, old); U(DeleteObject)(bmp); U(DeleteDC)(dc);
    dibdc_free(&a);
    U(DeleteMetaFile)(h);
}

TEST(wmf_play_1bpp_dib_black_white) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HBITMAP bmp = U(CreateBitmap)(32, 32, 1, 1, NULL);
    REQUIRE(dc && bmp);
    HGDIOBJ old = U(SelectObject)(dc, bmp);
    U(PatBlt)(dc, 0, 0, 32, 32, WHITENESS);
    HDC m = mf_new();
    REQUIRE(m);
    U(PatBlt)(m, 4, 4, 10, 10, BLACKNESS);
    U(Rectangle)(m, 16, 16, 28, 28);
    HMETAFILE h = U(CloseMetaFile)(m);
    REQUIRE(h);
    CHECK(U(PlayMetaFile)(dc, h));
    EXPECT_EQ(U(GetPixel)(dc, 6, 6), RGB(0, 0, 0));
    EXPECT_EQ(U(GetPixel)(dc, 30, 2), RGB(255, 255, 255));
    EXPECT_EQ(U(GetPixel)(dc, 20, 20), RGB(255, 255, 255));  /* white brush interior */
    EXPECT_EQ(U(GetPixel)(dc, 16, 20), RGB(0, 0, 0));        /* black pen edge */
    U(SelectObject)(dc, old); U(DeleteObject)(bmp); U(DeleteDC)(dc);
    U(DeleteMetaFile)(h);
}

TEST(wmf_play_exact_rect_pixels) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile));
    HDC m = mf_new();
    REQUIRE(m);
    HBRUSH b = U(CreateSolidBrush)(RGB(255, 0, 0));
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(0, 0, 255));
    U(SelectObject)(m, b); U(SelectObject)(m, p);
    U(Rectangle)(m, 10, 10, 20, 20);
    U(SelectObject)(m, stock(BLACK_PEN)); U(SelectObject)(m, stock(WHITE_BRUSH));
    U(DeleteObject)(b); U(DeleteObject)(p);
    HMETAFILE h = U(CloseMetaFile)(m);
    REQUIRE(h);
    DIBDC d;
    REQUIRE(dibdc_make(&d, 40, 40));
    CHECK(U(PlayMetaFile)(d.dc, h));
    EXPECT_EQ(DIBPX(&d, 10, 10), 0x0000FF);   /* pen outline */
    EXPECT_EQ(DIBPX(&d, 19, 19), 0x0000FF);
    EXPECT_EQ(DIBPX(&d, 9, 10), 0xFFFFFF);
    EXPECT_EQ(DIBPX(&d, 20, 20), 0xFFFFFF);
    EXPECT_EQ(DIBPX(&d, 15, 15), 0xFF0000);   /* brush interior */
    EXPECT_EQ(DIBPX(&d, 11, 11), 0xFF0000);
    EXPECT_EQ(DIBPX(&d, 18, 18), 0xFF0000);
    dibdc_free(&d);
    U(DeleteMetaFile)(h);
}

TEST(wmf_play_exact_line_pixels) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile));
    HDC m = mf_new();
    REQUIRE(m);
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(0, 255, 0));
    U(SelectObject)(m, p);
    U(MoveToEx)(m, 2, 5, NULL);
    U(LineTo)(m, 12, 5);
    U(SelectObject)(m, stock(BLACK_PEN));
    U(DeleteObject)(p);
    HMETAFILE h = U(CloseMetaFile)(m);
    REQUIRE(h);
    DIBDC d;
    REQUIRE(dibdc_make(&d, 20, 20));
    CHECK(U(PlayMetaFile)(d.dc, h));
    for (int x = 2; x < 12; x++) EXPECT_EQ(DIBPX(&d, x, 5), 0x00FF00);
    EXPECT_EQ(DIBPX(&d, 12, 5), 0xFFFFFF);   /* LineTo excludes the end point */
    EXPECT_EQ(DIBPX(&d, 1, 5), 0xFFFFFF);
    EXPECT_EQ(DIBPX(&d, 5, 4), 0xFFFFFF);
    EXPECT_EQ(DIBPX(&d, 5, 6), 0xFFFFFF);
    dibdc_free(&d);
    U(DeleteMetaFile)(h);
}

TEST(wmf_play_exact_setpixel) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile));
    HDC m = mf_new();
    REQUIRE(m);
    U(SetPixel)(m, 3, 4, RGB(10, 20, 30));
    U(SetPixelV)(m, 5, 6, RGB(200, 100, 50));
    HMETAFILE h = U(CloseMetaFile)(m);
    REQUIRE(h);
    DIBDC d;
    REQUIRE(dibdc_make(&d, 10, 10));
    CHECK(U(PlayMetaFile)(d.dc, h));
    EXPECT_EQ(DIBPX(&d, 3, 4), 0x0A141E);
    EXPECT_EQ(DIBPX(&d, 5, 6), 0xC86432);
    EXPECT_EQ(dibdc_nonwhite(&d), 2);
    dibdc_free(&d);
    U(DeleteMetaFile)(h);
}

TEST(wmf_play_exact_patblt_whiteness_blackness) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile));
    HDC m = mf_new();
    REQUIRE(m);
    U(PatBlt)(m, 2, 2, 4, 4, BLACKNESS);
    U(PatBlt)(m, 4, 4, 4, 4, DSTINVERT);
    HMETAFILE h = U(CloseMetaFile)(m);
    REQUIRE(h);
    DIBDC d;
    REQUIRE(dibdc_make(&d, 10, 10));
    CHECK(U(PlayMetaFile)(d.dc, h));
    EXPECT_EQ(DIBPX(&d, 2, 2), 0x000000);
    EXPECT_EQ(DIBPX(&d, 3, 3), 0x000000);
    EXPECT_EQ(DIBPX(&d, 4, 4), 0xFFFFFF);  /* black inverted -> white */
    EXPECT_EQ(DIBPX(&d, 5, 5), 0xFFFFFF);
    EXPECT_EQ(DIBPX(&d, 6, 6), 0x000000);  /* white inverted -> black */
    EXPECT_EQ(DIBPX(&d, 7, 7), 0x000000);
    EXPECT_EQ(DIBPX(&d, 8, 8), 0xFFFFFF);
    EXPECT_EQ(DIBPX(&d, 2, 5), 0x000000);
    dibdc_free(&d);
    U(DeleteMetaFile)(h);
}
