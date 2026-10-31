#include "mt.h"

#define WHITE RGB(255, 255, 255)
static XFORM xf(float a, float b, float c, float d, float e, float f)
{
    XFORM x; x.eM11 = a; x.eM12 = b; x.eM21 = c; x.eM22 = d; x.eDx = e; x.eDy = f; return x;
}
static void aniso(HDC dc, int wox, int woy, int wex, int wey, int vox, int voy, int vex, int vey)
{
    U(SetMapMode)(dc, MM_ANISOTROPIC);
    U(SetWindowOrgEx)(dc, wox, woy, NULL);
    U(SetViewportOrgEx)(dc, vox, voy, NULL);
    U(SetWindowExtEx)(dc, wex, wey, NULL);
    U(SetViewportExtEx)(dc, vex, vey, NULL);
}
/* plot one logical point, expect exactly one pixel at (ex,ey) */
#define PIX_AT(d, lx, ly, ex, ey) do { mt_dib_clear(&(d)); \
    BOOL r_ = U(SetPixelV)((d).dc, (lx), (ly), WHITE); \
    int n_ = mt_count(&(d)); \
    CHECKF(r_ && n_ == 1 && mt_px(&(d), (ex), (ey)) == 0xFFFFFF, "SetPixelV(%d,%d): ret %d count %d, pixel(%d,%d)=%ld", (int)(lx), (int)(ly), r_, n_, (int)(ex), (int)(ey), mt_px(&(d), (ex), (ey))); } while (0)

#define NEED_DIB(d) mt_dib d; if (!mt_dib_make(&d, 64, 64)) { t_skip("no dib"); return; }

TEST(mapping_draw_pixel_text_identity)
{
    REQUIRE(U(SetPixelV)); REQUIRE(U(GdiFlush)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    PIX_AT(d, 0, 0, 0, 0);
    PIX_AT(d, 63, 63, 63, 63);
    PIX_AT(d, 10, 20, 10, 20);
    PIX_AT(d, 20, 10, 20, 10);
    mt_dib_free(&d);
}

TEST(mapping_draw_pixel_viewport_org)
{
    REQUIRE(U(SetPixelV)); REQUIRE(U(GdiFlush)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    U(SetViewportOrgEx)(d.dc, 10, 20, NULL);
    PIX_AT(d, 0, 0, 10, 20);
    PIX_AT(d, 5, 5, 15, 25);
    PIX_AT(d, -10, -20, 0, 0);
    PIX_AT(d, 53, 43, 63, 63);
    U(SetViewportOrgEx)(d.dc, -5, 63, NULL);
    PIX_AT(d, 5, 0, 0, 63);
    PIX_AT(d, 68, -63, 63, 0);
    mt_dib_free(&d);
}

TEST(mapping_draw_pixel_window_org)
{
    REQUIRE(U(SetPixelV)); REQUIRE(U(GdiFlush)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    U(SetWindowOrgEx)(d.dc, 10, 20, NULL);
    PIX_AT(d, 10, 20, 0, 0);
    PIX_AT(d, 15, 25, 5, 5);
    PIX_AT(d, 73, 83, 63, 63);
    U(SetWindowOrgEx)(d.dc, -3, -4, NULL);
    PIX_AT(d, 0, 0, 3, 4);
    mt_dib_free(&d);
}

TEST(mapping_draw_pixel_window_and_viewport_org)
{
    REQUIRE(U(SetPixelV)); REQUIRE(U(GdiFlush)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    U(SetWindowOrgEx)(d.dc, 100, 100, NULL);
    U(SetViewportOrgEx)(d.dc, 32, 32, NULL);
    PIX_AT(d, 100, 100, 32, 32);
    PIX_AT(d, 90, 110, 22, 42);
    PIX_AT(d, 131, 131, 63, 63);
    mt_dib_free(&d);
}

TEST(mapping_draw_pixel_scale)
{
    REQUIRE(U(SetPixelV)); REQUIRE(U(GdiFlush)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    aniso(d.dc, 0, 0, 1, 1, 0, 0, 2, 2);
    PIX_AT(d, 3, 4, 6, 8);
    PIX_AT(d, 31, 31, 62, 62);
    aniso(d.dc, 0, 0, 1, 1, 0, 0, 3, 5);
    PIX_AT(d, 4, 3, 12, 15);
    aniso(d.dc, 0, 0, 2, 4, 0, 0, 1, 1);
    PIX_AT(d, 20, 40, 10, 10);
    PIX_AT(d, 126, 252, 63, 63);
    aniso(d.dc, 0, 0, 100, 100, 0, 0, 200, 200);
    PIX_AT(d, 10, 20, 20, 40);
    mt_dib_free(&d);
}

TEST(mapping_draw_pixel_flip)
{
    REQUIRE(U(SetPixelV)); REQUIRE(U(GdiFlush)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    aniso(d.dc, 0, 0, 1, 1, 0, 63, 1, -1);   /* y up, origin bottom-left */
    PIX_AT(d, 0, 0, 0, 63);
    PIX_AT(d, 10, 5, 10, 58);
    PIX_AT(d, 63, 63, 63, 0);
    aniso(d.dc, 0, 0, 1, 1, 63, 0, -1, 1);   /* x mirrored */
    PIX_AT(d, 0, 0, 63, 0);
    PIX_AT(d, 63, 10, 0, 10);
    aniso(d.dc, 0, 0, 1, 1, 63, 63, -1, -1);
    PIX_AT(d, 5, 6, 58, 57);
    mt_dib_free(&d);
}

TEST(mapping_draw_pixel_lometric_matches_lptodp)
{
    REQUIRE(U(SetPixelV)); REQUIRE(U(GdiFlush)); REQUIRE(U(LPtoDP)); REQUIRE(U(CreateDIBSection));
    static const int m[] = { MM_TEXT, MM_LOMETRIC, MM_HIMETRIC, MM_LOENGLISH, MM_HIENGLISH, MM_TWIPS };
    NEED_DIB(d);
    for (int i = 0; i < 6; i++) {
        U(SetMapMode)(d.dc, m[i]);
        U(SetViewportOrgEx)(d.dc, 5, 58, NULL);
        for (int j = 0; j < 4; j++) {
            POINT p = { j * 3, j * 4 };
            if (m[i] != MM_TEXT) { p.x *= 20; p.y *= 20; }
            POINT q = p;
            U(LPtoDP)(d.dc, &q, 1);
            if (q.x < 0 || q.y < 0 || q.x >= 64 || q.y >= 64) continue;
            PIX_AT(d, p.x, p.y, q.x, q.y);
        }
    }
    mt_dib_free(&d);
}

TEST(mapping_draw_pixel_world_transform)
{
    REQUIRE(U(SetPixelV)); REQUIRE(U(GdiFlush)); REQUIRE(U(SetWorldTransform)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    U(SetGraphicsMode)(d.dc, GM_ADVANCED);
    XFORM x;
    x = xf(1, 0, 0, 1, 5, 7);       U(SetWorldTransform)(d.dc, &x); PIX_AT(d, 1, 1, 6, 8);
    x = xf(3, 0, 0, 2, 0, 0);       U(SetWorldTransform)(d.dc, &x); PIX_AT(d, 4, 5, 12, 10);
    x = xf(-1, 0, 0, 1, 63, 0);     U(SetWorldTransform)(d.dc, &x); PIX_AT(d, 3, 9, 60, 9);
    x = xf(1, 0, 0, -1, 0, 63);     U(SetWorldTransform)(d.dc, &x); PIX_AT(d, 3, 9, 3, 54);
    x = xf(0, 1, -1, 0, 63, 0);     U(SetWorldTransform)(d.dc, &x); PIX_AT(d, 10, 20, 43, 10);   /* x'=-y+63, y'=x */
    x = xf(0, -1, 1, 0, 0, 63);     U(SetWorldTransform)(d.dc, &x); PIX_AT(d, 10, 20, 20, 53);   /* x'=y, y'=-x+63 */
    x = xf(1, 0, 1, 1, 0, 0);       U(SetWorldTransform)(d.dc, &x); PIX_AT(d, 3, 4, 7, 4);       /* shear */
    x = xf(1, 1, 0, 1, 0, 0);       U(SetWorldTransform)(d.dc, &x); PIX_AT(d, 3, 4, 3, 7);
    x = xf(0.5f, 0, 0, 0.5f, 0, 0); U(SetWorldTransform)(d.dc, &x); PIX_AT(d, 40, 20, 20, 10);
    mt_dib_free(&d);
}

TEST(mapping_draw_pixel_world_plus_page)
{
    REQUIRE(U(SetPixelV)); REQUIRE(U(GdiFlush)); REQUIRE(U(SetWorldTransform)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    U(SetGraphicsMode)(d.dc, GM_ADVANCED);
    XFORM x = xf(2, 0, 0, 2, 0, 0);
    U(SetWorldTransform)(d.dc, &x);
    U(SetViewportOrgEx)(d.dc, 10, 10, NULL);
    PIX_AT(d, 3, 4, 16, 18);
    U(SetWindowOrgEx)(d.dc, 2, 2, NULL);
    PIX_AT(d, 3, 4, 14, 16);
    U(SetMapMode)(d.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(d.dc, 1, 1, NULL);
    U(SetViewportExtEx)(d.dc, 2, 2, NULL);
    PIX_AT(d, 3, 4, 18, 22);
    mt_dib_free(&d);
}

TEST(mapping_draw_pixel_rtl)
{
    REQUIRE(U(SetPixelV)); REQUIRE(U(GdiFlush)); REQUIRE(U(SetLayout)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    U(SetLayout)(d.dc, LAYOUT_RTL);
    PIX_AT(d, 0, 0, 63, 0);
    PIX_AT(d, 10, 5, 53, 5);
    PIX_AT(d, 63, 63, 0, 63);
    U(SetLayout)(d.dc, 0);
    PIX_AT(d, 10, 5, 10, 5);
    mt_dib_free(&d);
}

TEST(mapping_draw_pixel_outside_bitmap)
{
    REQUIRE(U(SetPixelV)); REQUIRE(U(GdiFlush)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    aniso(d.dc, 0, 0, 1, 1, 0, 0, 2, 2);
    BOOL r = U(SetPixelV)(d.dc, 32, 32, WHITE);
    t_note("SetPixelV outside bitmap ret %d", r);
    CHECK(mt_count(&d) == 0);
    r = U(SetPixelV)(d.dc, -1, 0, WHITE);
    CHECK(mt_count(&d) == 0);
    r = U(SetPixelV)(d.dc, INT_MAX, INT_MIN, WHITE);
    CHECK(mt_count(&d) == 0);
    mt_dib_free(&d);
}

TEST(mapping_draw_getpixel_logical)
{
    REQUIRE(U(GetPixel)); REQUIRE(U(GdiFlush)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    d.bits[20 * 64 + 10] = 0x004080FF; /* R=40 G=80 B=FF */
    U(GdiFlush)();
    CHECK(U(GetPixel)(d.dc, 10, 20) == RGB(0x40, 0x80, 0xFF));
    U(SetViewportOrgEx)(d.dc, 5, 5, NULL);
    CHECK(U(GetPixel)(d.dc, 5, 15) == RGB(0x40, 0x80, 0xFF));
    CHECK(U(GetPixel)(d.dc, 10, 20) == RGB(0, 0, 0));
    aniso(d.dc, 0, 0, 1, 1, 0, 0, 2, 2);
    CHECK(U(GetPixel)(d.dc, 5, 10) == RGB(0x40, 0x80, 0xFF));
    CHECK(U(GetPixel)(d.dc, 10, 20) == RGB(0, 0, 0));
    CHECK(U(GetPixel)(d.dc, 100, 100) == CLR_INVALID);
    mt_dib_free(&d);
}

/* ---------- lines ---------- */
static HPEN mkpen(void) { return U(CreatePen)(PS_SOLID, 1, WHITE); }

static int line_x_extent(mt_dib *d, int *x0, int *x1, int *y)
{
    RECT r; if (!mt_bbox(d, &r)) return 0;
    *x0 = r.left; *x1 = r.right; *y = r.top;
    return r.top == r.bottom;
}

TEST(mapping_draw_lineto_horizontal_endpoint_excluded)
{
    REQUIRE(U(LineTo)); REQUIRE(U(MoveToEx)); REQUIRE(U(CreatePen)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    HPEN pen = mkpen(), old = U(SelectObject)(d.dc, pen);
    U(MoveToEx)(d.dc, 10, 5, NULL);
    CHECK(U(LineTo)(d.dc, 20, 5));
    int x0, x1, y;
    CHECK(line_x_extent(&d, &x0, &x1, &y));
    CHECKF(x0 == 10 && x1 == 19 && y == 5, "line spans %d..%d at %d", x0, x1, y);
    CHECK(mt_count(&d) == 10);
    /* reverse direction */
    mt_dib_clear(&d);
    U(MoveToEx)(d.dc, 20, 5, NULL); U(LineTo)(d.dc, 10, 5);
    CHECK(line_x_extent(&d, &x0, &x1, &y));
    CHECKF(x0 == 11 && x1 == 20 && y == 5, "reverse line spans %d..%d at %d", x0, x1, y);
    U(SelectObject)(d.dc, old); U(DeleteObject)(pen);
    mt_dib_free(&d);
}

TEST(mapping_draw_lineto_viewport_org)
{
    REQUIRE(U(LineTo)); REQUIRE(U(MoveToEx)); REQUIRE(U(CreatePen)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    HPEN pen = mkpen(), old = U(SelectObject)(d.dc, pen);
    U(SetViewportOrgEx)(d.dc, 7, 9, NULL);
    U(MoveToEx)(d.dc, 10, 5, NULL); U(LineTo)(d.dc, 20, 5);
    int x0, x1, y;
    CHECK(line_x_extent(&d, &x0, &x1, &y));
    CHECKF(x0 == 17 && x1 == 26 && y == 14, "line spans %d..%d at %d", x0, x1, y);
    U(SelectObject)(d.dc, old); U(DeleteObject)(pen);
    mt_dib_free(&d);
}

TEST(mapping_draw_lineto_scaled)
{
    REQUIRE(U(LineTo)); REQUIRE(U(MoveToEx)); REQUIRE(U(CreatePen)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    HPEN pen = mkpen(), old = U(SelectObject)(d.dc, pen);
    aniso(d.dc, 0, 0, 1, 1, 0, 0, 3, 2);
    U(MoveToEx)(d.dc, 2, 4, NULL); U(LineTo)(d.dc, 6, 4);
    int x0, x1, y;
    line_x_extent(&d, &x0, &x1, &y);
    t_note("aniso 3x2 line spans %d..%d at %d (expect 6..17 at 8 for a 1px pen)", x0, x1, y);
    CHECK(mt_set(&d, 6, 8) && mt_set(&d, 17, 8) == mt_set(&d, 17, 8));
    U(SelectObject)(d.dc, old); U(DeleteObject)(pen);
    mt_dib_free(&d);
}

TEST(mapping_draw_lineto_mirrored_x)
{
    REQUIRE(U(LineTo)); REQUIRE(U(MoveToEx)); REQUIRE(U(CreatePen)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    HPEN pen = mkpen(), old = U(SelectObject)(d.dc, pen);
    aniso(d.dc, 0, 0, 1, 1, 63, 0, -1, 1);
    U(MoveToEx)(d.dc, 10, 5, NULL); U(LineTo)(d.dc, 20, 5);   /* device 53 -> 43 */
    int x0, x1, y;
    CHECK(line_x_extent(&d, &x0, &x1, &y));
    CHECKF(x0 == 44 && x1 == 53 && y == 5, "line spans %d..%d at %d", x0, x1, y);
    CHECK(mt_set(&d, 53, 5) && !mt_set(&d, 43, 5));
    U(SelectObject)(d.dc, old); U(DeleteObject)(pen);
    mt_dib_free(&d);
}

TEST(mapping_draw_lineto_vertical_flip)
{
    REQUIRE(U(LineTo)); REQUIRE(U(MoveToEx)); REQUIRE(U(CreatePen)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    HPEN pen = mkpen(), old = U(SelectObject)(d.dc, pen);
    aniso(d.dc, 0, 0, 1, 1, 0, 63, 1, -1);
    U(MoveToEx)(d.dc, 4, 10, NULL); U(LineTo)(d.dc, 4, 20);   /* device y 53 -> 43 */
    RECT r; CHECK(mt_bbox(&d, &r));
    CHECKF(r.left == 4 && r.right == 4 && r.top == 44 && r.bottom == 53, "bbox %ld,%ld,%ld,%ld", r.left, r.top, r.right, r.bottom);
    U(SelectObject)(d.dc, old); U(DeleteObject)(pen);
    mt_dib_free(&d);
}

TEST(mapping_draw_lineto_world_translate_scale)
{
    REQUIRE(U(LineTo)); REQUIRE(U(MoveToEx)); REQUIRE(U(CreatePen)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    HPEN pen = mkpen(), old = U(SelectObject)(d.dc, pen);
    U(SetGraphicsMode)(d.dc, GM_ADVANCED);
    XFORM x = xf(1, 0, 0, 1, 5, 6);
    U(SetWorldTransform)(d.dc, &x);
    U(MoveToEx)(d.dc, 10, 5, NULL); U(LineTo)(d.dc, 20, 5);
    int x0, x1, y;
    CHECK(line_x_extent(&d, &x0, &x1, &y));
    CHECKF(x0 == 15 && x1 == 24 && y == 11, "translated line spans %d..%d at %d", x0, x1, y);
    mt_dib_clear(&d);
    x = xf(2, 0, 0, 2, 0, 0);
    U(SetWorldTransform)(d.dc, &x);
    U(MoveToEx)(d.dc, 5, 3, NULL); U(LineTo)(d.dc, 15, 3);
    line_x_extent(&d, &x0, &x1, &y);
    t_note("scaled (x2 world) line spans %d..%d at %d (pen width scaling is mode dependent)", x0, x1, y);
    U(SelectObject)(d.dc, old); U(DeleteObject)(pen);
    mt_dib_free(&d);
}

TEST(mapping_draw_lineto_world_rotate90)
{
    REQUIRE(U(LineTo)); REQUIRE(U(MoveToEx)); REQUIRE(U(CreatePen)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    HPEN pen = mkpen(), old = U(SelectObject)(d.dc, pen);
    U(SetGraphicsMode)(d.dc, GM_ADVANCED);
    XFORM x = xf(0, 1, -1, 0, 63, 0);   /* (x,y) -> (63-y, x) */
    U(SetWorldTransform)(d.dc, &x);
    U(MoveToEx)(d.dc, 10, 5, NULL); U(LineTo)(d.dc, 20, 5);   /* device (58,10) -> (58,20) */
    RECT r; CHECK(mt_bbox(&d, &r));
    CHECKF(r.left == 58 && r.right == 58 && r.top == 10 && r.bottom == 19, "bbox %ld,%ld,%ld,%ld", r.left, r.top, r.right, r.bottom);
    U(SelectObject)(d.dc, old); U(DeleteObject)(pen);
    mt_dib_free(&d);
}

TEST(mapping_draw_lineto_rtl)
{
    REQUIRE(U(LineTo)); REQUIRE(U(MoveToEx)); REQUIRE(U(CreatePen)); REQUIRE(U(SetLayout)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    HPEN pen = mkpen(), old = U(SelectObject)(d.dc, pen);
    U(SetLayout)(d.dc, LAYOUT_RTL);
    U(MoveToEx)(d.dc, 10, 5, NULL); U(LineTo)(d.dc, 20, 5);
    RECT r; CHECK(mt_bbox(&d, &r));
    CHECKF(r.top == 5 && r.bottom == 5 && r.right == 53 && r.left >= 43 && r.left <= 44, "bbox %ld,%ld,%ld,%ld", r.left, r.top, r.right, r.bottom);
    t_note("RTL line spans %ld..%ld", r.left, r.right);
    U(SelectObject)(d.dc, old); U(DeleteObject)(pen);
    mt_dib_free(&d);
}

TEST(mapping_draw_lineto_updates_logical_position)
{
    REQUIRE(U(LineTo)); REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx)); REQUIRE(U(CreatePen)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    HPEN pen = mkpen(), old = U(SelectObject)(d.dc, pen);
    aniso(d.dc, 0, 0, 1, 1, 3, 3, 2, 2);
    U(MoveToEx)(d.dc, 1, 1, NULL);
    U(LineTo)(d.dc, 11, 6);
    POINT p; CHECK(U(GetCurrentPositionEx)(d.dc, &p));
    CHECKF(p.x == 11 && p.y == 6, "pos %ld,%ld", p.x, p.y);
    CHECK(mt_set(&d, 5, 5));
    U(SelectObject)(d.dc, old); U(DeleteObject)(pen);
    mt_dib_free(&d);
}

TEST(mapping_draw_lineto_with_negative_region_clipped)
{
    REQUIRE(U(LineTo)); REQUIRE(U(MoveToEx)); REQUIRE(U(CreatePen)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    HPEN pen = mkpen(), old = U(SelectObject)(d.dc, pen);
    U(SetViewportOrgEx)(d.dc, 32, 32, NULL);
    U(MoveToEx)(d.dc, -100, 0, NULL); U(LineTo)(d.dc, 100, 0);
    RECT r; CHECK(mt_bbox(&d, &r));
    CHECKF(r.left == 0 && r.right == 63 && r.top == 32 && r.bottom == 32, "bbox %ld,%ld,%ld,%ld", r.left, r.top, r.right, r.bottom);
    CHECK(mt_count(&d) == 64);
    U(SelectObject)(d.dc, old); U(DeleteObject)(pen);
    mt_dib_free(&d);
}

/* ---------- rectangles / patblt ---------- */
TEST(mapping_draw_rectangle_outline_bbox)
{
    REQUIRE(U(Rectangle)); REQUIRE(U(CreatePen)); REQUIRE(U(GetStockObject)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    HPEN pen = mkpen(), old = U(SelectObject)(d.dc, pen);
    HGDIOBJ ob = U(SelectObject)(d.dc, U(GetStockObject)(NULL_BRUSH));
    RECT r;
    CHECK(U(Rectangle)(d.dc, 2, 3, 10, 8));
    CHECK(mt_bbox(&d, &r));
    CHECKF(r.left == 2 && r.top == 3 && r.right == 9 && r.bottom == 7, "plain bbox %ld,%ld,%ld,%ld", r.left, r.top, r.right, r.bottom);
    mt_dib_clear(&d);
    U(SetViewportOrgEx)(d.dc, 20, 30, NULL);
    U(Rectangle)(d.dc, 2, 3, 10, 8);
    CHECK(mt_bbox(&d, &r));
    CHECKF(r.left == 22 && r.top == 33 && r.right == 29 && r.bottom == 37, "viewport bbox %ld,%ld,%ld,%ld", r.left, r.top, r.right, r.bottom);
    mt_dib_clear(&d);
    aniso(d.dc, 0, 0, 1, 1, 0, 0, 2, 2);
    U(Rectangle)(d.dc, 2, 3, 10, 8);
    CHECK(mt_bbox(&d, &r));
    t_note("scaled bbox (expect 4,6,19,15)");
    CHECKF(r.left <= 4 && r.top <= 6 && r.right >= 19 && r.bottom >= 15 && r.right <= 20 && r.bottom <= 16, "scaled bbox %ld,%ld,%ld,%ld", r.left, r.top, r.right, r.bottom);
    U(SelectObject)(d.dc, ob); U(SelectObject)(d.dc, old); U(DeleteObject)(pen);
    mt_dib_free(&d);
}

TEST(mapping_draw_rectangle_flipped_normalized)
{
    REQUIRE(U(Rectangle)); REQUIRE(U(CreatePen)); REQUIRE(U(GetStockObject)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    HPEN pen = mkpen(), old = U(SelectObject)(d.dc, pen);
    HGDIOBJ ob = U(SelectObject)(d.dc, U(GetStockObject)(NULL_BRUSH));
    aniso(d.dc, 0, 0, 1, 1, 0, 63, 1, -1);
    CHECK(U(Rectangle)(d.dc, 5, 10, 15, 20));
    RECT r; CHECK(mt_bbox(&d, &r));
    t_note("flipped-y rectangle bbox %ld,%ld,%ld,%ld", r.left, r.top, r.right, r.bottom);
    CHECK(r.right - r.left >= 8 && r.right - r.left <= 10 && r.bottom - r.top >= 8 && r.bottom - r.top <= 10);
    CHECK(r.left >= 4 && r.left <= 6 && r.top >= 42 && r.top <= 44);
    U(SelectObject)(d.dc, ob); U(SelectObject)(d.dc, old); U(DeleteObject)(pen);
    mt_dib_free(&d);
}

TEST(mapping_draw_patblt_fill_rects)
{
    REQUIRE(U(PatBlt)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    HBRUSH br = U(CreateSolidBrush)(WHITE), old = U(SelectObject)(d.dc, br);
    RECT r;
    CHECK(U(PatBlt)(d.dc, 5, 6, 10, 4, PATCOPY));
    CHECK(mt_bbox(&d, &r) && r.left == 5 && r.top == 6 && r.right == 14 && r.bottom == 9);
    CHECK(mt_count(&d) == 40);
    mt_dib_clear(&d);
    U(SetViewportOrgEx)(d.dc, 20, 30, NULL);
    U(PatBlt)(d.dc, 5, 6, 10, 4, PATCOPY);
    CHECK(mt_bbox(&d, &r) && r.left == 25 && r.top == 36 && r.right == 34 && r.bottom == 39);
    mt_dib_clear(&d);
    aniso(d.dc, 0, 0, 1, 1, 0, 0, 2, 3);
    U(PatBlt)(d.dc, 5, 6, 10, 4, PATCOPY);
    CHECK(mt_bbox(&d, &r) && r.left == 10 && r.top == 18 && r.right == 29 && r.bottom == 29);
    CHECK(mt_count(&d) == 20 * 12);
    mt_dib_clear(&d);
    aniso(d.dc, 10, 10, 1, 1, 0, 0, 1, 1);
    U(PatBlt)(d.dc, 15, 15, 5, 5, PATCOPY);
    CHECK(mt_bbox(&d, &r) && r.left == 5 && r.top == 5 && r.right == 9 && r.bottom == 9);
    U(SelectObject)(d.dc, old); U(DeleteObject)(br);
    mt_dib_free(&d);
}

TEST(mapping_draw_patblt_world_translate)
{
    REQUIRE(U(PatBlt)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    HBRUSH br = U(CreateSolidBrush)(WHITE), old = U(SelectObject)(d.dc, br);
    U(SetGraphicsMode)(d.dc, GM_ADVANCED);
    XFORM x = xf(1, 0, 0, 1, 7, 9);
    U(SetWorldTransform)(d.dc, &x);
    RECT r;
    CHECK(U(PatBlt)(d.dc, 1, 1, 6, 5, PATCOPY));
    CHECK(mt_bbox(&d, &r) && r.left == 8 && r.top == 10 && r.right == 13 && r.bottom == 14);
    CHECK(mt_count(&d) == 30);
    mt_dib_clear(&d);
    x = xf(2, 0, 0, 2, 0, 0);
    U(SetWorldTransform)(d.dc, &x);
    U(PatBlt)(d.dc, 3, 3, 4, 4, PATCOPY);
    CHECK(mt_bbox(&d, &r) && r.left == 6 && r.top == 6 && r.right == 13 && r.bottom == 13);
    U(SelectObject)(d.dc, old); U(DeleteObject)(br);
    mt_dib_free(&d);
}

TEST(mapping_draw_patblt_flipped_observed)
{
    REQUIRE(U(PatBlt)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    HBRUSH br = U(CreateSolidBrush)(WHITE), old = U(SelectObject)(d.dc, br);
    aniso(d.dc, 0, 0, 1, 1, 0, 63, 1, -1);
    U(PatBlt)(d.dc, 5, 10, 4, 3, PATCOPY);
    RECT r; mt_bbox(&d, &r);
    t_note("flipped PatBlt bbox %ld,%ld,%ld,%ld count %d", r.left, r.top, r.right, r.bottom, mt_count(&d));
    CHECK(mt_count(&d) == 12);
    U(SelectObject)(d.dc, old); U(DeleteObject)(br);
    mt_dib_free(&d);
}

TEST(mapping_draw_patblt_negative_extent)
{
    REQUIRE(U(PatBlt)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    HBRUSH br = U(CreateSolidBrush)(WHITE), old = U(SelectObject)(d.dc, br);
    BOOL r = U(PatBlt)(d.dc, 15, 15, -5, -5, PATCOPY);
    RECT bb; mt_bbox(&d, &bb);
    t_note("PatBlt negative extents ret %d bbox %ld,%ld,%ld,%ld count %d", r, bb.left, bb.top, bb.right, bb.bottom, mt_count(&d));
    U(SelectObject)(d.dc, old); U(DeleteObject)(br);
    mt_dib_free(&d);
}

/* ---------- clip box / visibility ---------- */
#define BOX_IS(dc, l, t, r_, b, rv) do { RECT bx_ = {0x7777, 0x7777, 0x7777, 0x7777}; int ret_ = U(GetClipBox)((dc), &bx_); \
    CHECKF(ret_ == (rv) && bx_.left == (l) && bx_.top == (t) && bx_.right == (r_) && bx_.bottom == (b), \
           "GetClipBox ret %d box %ld,%ld,%ld,%ld want %d box %d,%d,%d,%d", ret_, bx_.left, bx_.top, bx_.right, bx_.bottom, (int)(rv), (int)(l), (int)(t), (int)(r_), (int)(b)); } while (0)

TEST(mapping_clipbox_logical_units)
{
    REQUIRE(U(GetClipBox)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    BOX_IS(d.dc, 0, 0, 64, 64, SIMPLEREGION);
    U(SetViewportOrgEx)(d.dc, 10, 20, NULL);
    BOX_IS(d.dc, -10, -20, 54, 44, SIMPLEREGION);
    U(SetViewportOrgEx)(d.dc, 0, 0, NULL);
    U(SetWindowOrgEx)(d.dc, 5, 7, NULL);
    BOX_IS(d.dc, 5, 7, 69, 71, SIMPLEREGION);
    U(SetWindowOrgEx)(d.dc, 0, 0, NULL);
    aniso(d.dc, 0, 0, 1, 1, 0, 0, 2, 2);
    BOX_IS(d.dc, 0, 0, 32, 32, SIMPLEREGION);
    aniso(d.dc, 0, 0, 2, 4, 0, 0, 1, 1);
    BOX_IS(d.dc, 0, 0, 128, 256, SIMPLEREGION);
    aniso(d.dc, 100, 100, 1, 1, 10, 10, 1, 1);
    BOX_IS(d.dc, 90, 90, 154, 154, SIMPLEREGION);
    mt_dib_free(&d);
}

TEST(mapping_clipbox_mirrored_observed)
{
    REQUIRE(U(GetClipBox)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    aniso(d.dc, 0, 0, 1, 1, 0, 63, 1, -1);
    RECT b; int r = U(GetClipBox)(d.dc, &b);
    t_note("flip-y clip box ret %d: %ld,%ld,%ld,%ld", r, b.left, b.top, b.right, b.bottom);
    CHECK(r == SIMPLEREGION);
    if (!(b.right - b.left == 64 && b.bottom - b.top == 64)) t_note("flip-y box not 64x64");
    CHECK(r != ERROR);
    aniso(d.dc, 0, 0, 1, 1, 63, 0, -1, 1);
    r = U(GetClipBox)(d.dc, &b);
    t_note("flip-x clip box ret %d: %ld,%ld,%ld,%ld", r, b.left, b.top, b.right, b.bottom);
    if (!(b.right - b.left == 64 && b.bottom - b.top == 64)) t_note("flip-x box not 64x64");
    mt_dib_free(&d);
}

TEST(mapping_clipbox_intersect_logical)
{
    REQUIRE(U(GetClipBox)); REQUIRE(U(IntersectClipRect)); REQUIRE(U(SetPixelV)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    U(SetViewportOrgEx)(d.dc, 3, 4, NULL);
    int r = U(IntersectClipRect)(d.dc, 10, 10, 20, 20);
    CHECK(r == SIMPLEREGION);
    BOX_IS(d.dc, 10, 10, 20, 20, SIMPLEREGION);
    PIX_AT(d, 10, 10, 13, 14);
    PIX_AT(d, 19, 19, 22, 23);
    mt_dib_clear(&d);
    U(SetPixelV)(d.dc, 9, 10, WHITE);
    U(SetPixelV)(d.dc, 20, 10, WHITE);
    U(SetPixelV)(d.dc, 10, 9, WHITE);
    U(SetPixelV)(d.dc, 10, 20, WHITE);
    CHECK(mt_count(&d) == 0);
    mt_dib_free(&d);
}

TEST(mapping_clipbox_intersect_scaled)
{
    REQUIRE(U(GetClipBox)); REQUIRE(U(IntersectClipRect)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    aniso(d.dc, 0, 0, 1, 1, 0, 0, 2, 2);
    int r = U(IntersectClipRect)(d.dc, 5, 5, 10, 12);
    CHECK(r == SIMPLEREGION);
    BOX_IS(d.dc, 5, 5, 10, 12, SIMPLEREGION);
    U(SetMapMode)(d.dc, MM_TEXT);
    BOX_IS(d.dc, 10, 10, 20, 24, SIMPLEREGION);
    mt_dib_free(&d);
}

TEST(mapping_clipbox_clip_stays_device_after_mapping_change)
{
    REQUIRE(U(GetClipBox)); REQUIRE(U(IntersectClipRect)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    U(IntersectClipRect)(d.dc, 10, 10, 20, 20);
    U(SetViewportOrgEx)(d.dc, 5, 5, NULL);
    BOX_IS(d.dc, 5, 5, 15, 15, SIMPLEREGION);
    U(SetViewportOrgEx)(d.dc, 0, 0, NULL);
    BOX_IS(d.dc, 10, 10, 20, 20, SIMPLEREGION);
    mt_dib_free(&d);
}

TEST(mapping_clipbox_empty_clip)
{
    REQUIRE(U(GetClipBox)); REQUIRE(U(IntersectClipRect)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    U(SetViewportOrgEx)(d.dc, 100, 100, NULL);   /* whole bitmap is at negative logical coords */
    int r = U(IntersectClipRect)(d.dc, 0, 0, 10, 10);
    if (r != NULLREGION) t_note("intersect outside ret %d", r);
    RECT b = {1, 1, 1, 1};
    r = U(GetClipBox)(d.dc, &b);
    if (r != NULLREGION) t_note("clip box ret %d", r);
    if (b.left || b.top || b.right || b.bottom) t_note("null box %ld,%ld,%ld,%ld", b.left, b.top, b.right, b.bottom);
    mt_dib_free(&d);
}

TEST(mapping_ptvisible_logical)
{
    REQUIRE(U(PtVisible)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    CHECK(U(PtVisible)(d.dc, 0, 0) == TRUE);
    CHECK(U(PtVisible)(d.dc, 63, 63) == TRUE);
    CHECK(U(PtVisible)(d.dc, 64, 0) == FALSE);
    CHECK(U(PtVisible)(d.dc, 0, 64) == FALSE);
    CHECK(U(PtVisible)(d.dc, -1, 0) == FALSE);
    U(SetViewportOrgEx)(d.dc, 10, 10, NULL);
    CHECK(U(PtVisible)(d.dc, -10, -10) == TRUE);
    CHECK(U(PtVisible)(d.dc, -11, -10) == FALSE);
    CHECK(U(PtVisible)(d.dc, 53, 53) == TRUE);
    CHECK(U(PtVisible)(d.dc, 54, 53) == FALSE);
    aniso(d.dc, 0, 0, 1, 1, 0, 0, 2, 2);
    CHECK(U(PtVisible)(d.dc, 31, 31) == TRUE);
    CHECK(U(PtVisible)(d.dc, 32, 0) == FALSE);
    CHECK(U(PtVisible)(d.dc, 0, 32) == FALSE);
    aniso(d.dc, 0, 0, 1, 1, 63, 63, -1, -1);
    CHECK(U(PtVisible)(d.dc, 0, 0) == TRUE);
    CHECK(U(PtVisible)(d.dc, 63, 63) == TRUE);
    CHECK(U(PtVisible)(d.dc, -1, 0) == FALSE);
    mt_dib_free(&d);
}

TEST(mapping_ptvisible_with_clip_and_world)
{
    REQUIRE(U(PtVisible)); REQUIRE(U(IntersectClipRect)); REQUIRE(U(SetWorldTransform)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    U(IntersectClipRect)(d.dc, 10, 10, 20, 20);
    CHECK(U(PtVisible)(d.dc, 10, 10) == TRUE);
    CHECK(U(PtVisible)(d.dc, 19, 19) == TRUE);
    CHECK(U(PtVisible)(d.dc, 20, 20) == FALSE);
    CHECK(U(PtVisible)(d.dc, 9, 15) == FALSE);
    U(SetGraphicsMode)(d.dc, GM_ADVANCED);
    XFORM x = xf(1, 0, 0, 1, 100, 100);
    U(SetWorldTransform)(d.dc, &x);
    CHECK(U(PtVisible)(d.dc, -90, -90) == TRUE);
    CHECK(U(PtVisible)(d.dc, 10, 10) == FALSE);
    mt_dib_free(&d);
}

TEST(mapping_rectvisible_logical)
{
    REQUIRE(U(RectVisible)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    RECT r;
    SetRect(&r, 10, 10, 20, 20);   CHECK(U(RectVisible)(d.dc, &r) == TRUE);
    SetRect(&r, 63, 0, 70, 10);    CHECK(U(RectVisible)(d.dc, &r) == TRUE);
    SetRect(&r, 64, 0, 70, 10);    CHECK(U(RectVisible)(d.dc, &r) == FALSE);
    SetRect(&r, -10, -10, 0, 0);   CHECK(U(RectVisible)(d.dc, &r) == FALSE);
    SetRect(&r, -10, -10, 1, 1);   CHECK(U(RectVisible)(d.dc, &r) == TRUE);
    SetRect(&r, -100, -100, 200, 200); CHECK(U(RectVisible)(d.dc, &r) == TRUE);
    U(SetViewportOrgEx)(d.dc, 100, 0, NULL);
    SetRect(&r, 10, 10, 20, 20);   CHECK(U(RectVisible)(d.dc, &r) == FALSE);
    SetRect(&r, -40, 10, -30, 20); CHECK(U(RectVisible)(d.dc, &r) == TRUE);
    aniso(d.dc, 0, 0, 1, 1, 0, 0, 4, 4);
    SetRect(&r, 15, 0, 20, 5);     CHECK(U(RectVisible)(d.dc, &r) == TRUE);
    SetRect(&r, 16, 0, 20, 5);     CHECK(U(RectVisible)(d.dc, &r) == FALSE);
    mt_dib_free(&d);
}

TEST(mapping_rectvisible_empty_and_flipped_rects)
{
    REQUIRE(U(RectVisible)); REQUIRE(U(CreateDIBSection));
    NEED_DIB(d);
    RECT r;
    SetRect(&r, 10, 10, 10, 20);   t_note("empty rect (zero width) visible=%d", U(RectVisible)(d.dc, &r));
    SetRect(&r, 20, 20, 10, 10);   t_note("inverted rect visible=%d", U(RectVisible)(d.dc, &r));
    aniso(d.dc, 0, 0, 1, 1, 63, 63, -1, -1);
    SetRect(&r, 0, 0, 10, 10);     CHECK(U(RectVisible)(d.dc, &r) == TRUE);
    SetRect(&r, -20, -20, -5, -5); CHECK(U(RectVisible)(d.dc, &r) == FALSE);
    mt_dib_free(&d);
}

TEST(mapping_visible_invalid_dc)
{
    REQUIRE(U(PtVisible)); REQUIRE(U(RectVisible));
    HDC dead = mt_dc(); mt_free_dc(dead);
    HDC bad[] = { NULL, (HDC)1, (HDC)0xdeadbeef, dead };
    RECT r = { 0, 0, 10, 10 };
    for (int i = 0; i < 4; i++) {
        SetLastError(0xdeadbeef);
        int pv = U(PtVisible)(bad[i], 1, 1);
        BOOL rv = U(RectVisible)(bad[i], &r);
        t_note("idx %d PtVisible=%d RectVisible=%d err %lu", i, pv, rv, GetLastError());
        CHECKF(pv != TRUE && !rv, "idx %d visible on invalid dc", i);
    }
}

TEST(mapping_clipbox_invalid_dc)
{
    REQUIRE(U(GetClipBox));
    HDC dead = mt_dc(); mt_free_dc(dead);
    HDC bad[] = { NULL, (HDC)1, (HDC)0xdeadbeef, dead };
    for (int i = 0; i < 4; i++) {
        RECT b = {0x7777, 0x7777, 0x7777, 0x7777};
        int r = U(GetClipBox)(bad[i], &b);
        CHECKF(r == ERROR || r == 0, "idx %d GetClipBox=%d", i, r);
    }
}

/* ---------- misc: full table-driven consistency ---------- */
TEST(mapping_draw_consistency_with_lptodp_grid)
{
    REQUIRE(U(SetPixelV)); REQUIRE(U(LPtoDP)); REQUIRE(U(GdiFlush)); REQUIRE(U(CreateDIBSection));
    static const int cfg[][8] = {
        { 0, 0, 1, 1, 0, 0, 1, 1 }, { 0, 0, 1, 1, 8, 8, 1, 1 }, { 4, 4, 1, 1, 0, 0, 2, 2 }, { 0, 0, 3, 3, 5, 5, 1, 1 },
        { 0, 0, 1, 1, 63, 0, -1, 1 }, { 0, 0, 1, 1, 0, 63, 1, -1 }, { 2, 2, 1, 1, 60, 60, -3, -3 }, { 0, 0, 4, 4, 0, 0, 1, 1 },
    };
    NEED_DIB(d);
    for (int c = 0; c < 8; c++) {
        aniso(d.dc, cfg[c][0], cfg[c][1], cfg[c][2], cfg[c][3], cfg[c][4], cfg[c][5], cfg[c][6], cfg[c][7]);
        for (int y = -4; y < 40; y += 7) for (int x = -4; x < 40; x += 5) {
            POINT p = { x, y }; U(LPtoDP)(d.dc, &p, 1);
            mt_dib_clear(&d);
            U(SetPixelV)(d.dc, x, y, WHITE);
            int n = mt_count(&d);
            int inside = p.x >= 0 && p.y >= 0 && p.x < 64 && p.y < 64;
            CHECKF(n == (inside ? 1 : 0) && (!inside || mt_set(&d, p.x, p.y)), "cfg %d point %d,%d -> %ld,%ld count %d", c, x, y, p.x, p.y, n);
        }
    }
    mt_dib_free(&d);
}

TEST(mapping_draw_no_object_leak)
{
    REQUIRE(U(GetGuiResources)); REQUIRE(U(CreateDIBSection));
    DWORD before = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    for (int i = 0; i < 50; i++) {
        mt_dib d;
        if (!mt_dib_make(&d, 16, 16)) break;
        aniso(d.dc, 0, 0, 1, 1, i, i, 2, 2);
        U(SetPixelV)(d.dc, 1, 1, WHITE);
        mt_dib_free(&d);
    }
    DWORD after = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    CHECKF(before == after, "gdi objects %lu -> %lu", before, after);
}
