#include "bm.h"

#define FLOOD_REQ() do { BM_REQ_PIX(); REQUIRE(U(ExtFloodFill)); REQUIRE(U(FloodFill)); REQUIRE(U(CreateSolidBrush)); } while (0)

enum { W = 10, H = 10 };
#define WHITE 0xFFFFFF
#define BLACK 0x000000
#define RED   0x0000FF
#define GREEN 0x00FF00
#define BLUE  0xFF0000

static void rect_border(dib_t *d, int x0, int y0, int x1, int y1, COLORREF c) {  /* inclusive */
    int x, y;
    for (x = x0; x <= x1; x++) { U(SetPixelV)(d->dc, x, y0, c); U(SetPixelV)(d->dc, x, y1, c); }
    for (y = y0; y <= y1; y++) { U(SetPixelV)(d->dc, x0, y, c); U(SetPixelV)(d->dc, x1, y, c); }
}
static void clear(dib_t *d, COLORREF c) {
    int x, y;
    for (y = 0; y < d->h; y++) for (x = 0; x < d->w; x++) U(SetPixelV)(d->dc, x, y, c);
}
static int count_color(dib_t *d, COLORREF c) {
    int x, y, n = 0;
    for (y = 0; y < d->h; y++) for (x = 0; x < d->w; x++) if (U(GetPixel)(d->dc, x, y) == c) n++;
    return n;
}
/* white canvas with a black box x 2..7, y 2..7 */
static int setup_box(dib_t *d) {
    if (!dib_open(d, W, H, 32)) return 0;
    clear(d, WHITE);
    rect_border(d, 2, 2, 7, 7, BLACK);
    return 1;
}
static HGDIOBJ use_brush(dib_t *d, COLORREF c, HBRUSH *out) {
    *out = U(CreateSolidBrush)(c);
    return U(SelectObject)(d->dc, *out);
}
static void drop_brush(dib_t *d, HGDIOBJ old, HBRUSH br) { U(SelectObject)(d->dc, old); U(DeleteObject)(br); }

TEST(bitmaps_flood_border_fills_interior) {
    FLOOD_REQ();
    dib_t d; REQUIRE(setup_box(&d));
    HBRUSH br; HGDIOBJ old = use_brush(&d, RED, &br);
    CHECK(U(ExtFloodFill)(d.dc, 4, 4, BLACK, FLOODFILLBORDER));
    drop_brush(&d, old, br);
    EXPECT_EQ(count_color(&d, RED), 16);          /* 4x4 interior */
    EXPECT_EQ(count_color(&d, BLACK), 20);        /* box untouched */
    EXPECT_EQ(count_color(&d, WHITE), 100 - 16 - 20);
    EXPECT_EQ(U(GetPixel)(d.dc, 3, 3), RED);
    EXPECT_EQ(U(GetPixel)(d.dc, 0, 0), WHITE);
    dib_close(&d);
}
TEST(bitmaps_flood_border_outside_fills_exterior) {
    FLOOD_REQ();
    dib_t d; REQUIRE(setup_box(&d));
    HBRUSH br; HGDIOBJ old = use_brush(&d, GREEN, &br);
    CHECK(U(ExtFloodFill)(d.dc, 0, 0, BLACK, FLOODFILLBORDER));
    drop_brush(&d, old, br);
    EXPECT_EQ(count_color(&d, GREEN), 100 - 36);  /* everything outside the box outline and inside */
    EXPECT_EQ(count_color(&d, WHITE), 16);        /* interior still white */
    EXPECT_EQ(count_color(&d, BLACK), 20);
    dib_close(&d);
}
TEST(bitmaps_flood_surface_fills_matching_region) {
    FLOOD_REQ();
    dib_t d; REQUIRE(setup_box(&d));
    HBRUSH br; HGDIOBJ old = use_brush(&d, BLUE, &br);
    CHECK(U(ExtFloodFill)(d.dc, 4, 4, WHITE, FLOODFILLSURFACE));
    drop_brush(&d, old, br);
    EXPECT_EQ(count_color(&d, BLUE), 16);
    EXPECT_EQ(count_color(&d, WHITE), 100 - 36);
    dib_close(&d);
}
TEST(bitmaps_flood_surface_start_not_surface_color_fails) {
    FLOOD_REQ();
    dib_t d; REQUIRE(setup_box(&d));
    HBRUSH br; HGDIOBJ old = use_brush(&d, RED, &br);
    CHECK(!U(ExtFloodFill)(d.dc, 4, 4, BLACK, FLOODFILLSURFACE));
    CHECK(!U(ExtFloodFill)(d.dc, 4, 4, 0x123456, FLOODFILLSURFACE));
    drop_brush(&d, old, br);
    EXPECT_EQ(count_color(&d, RED), 0);
    dib_close(&d);
}
TEST(bitmaps_flood_border_start_on_border_fails) {
    FLOOD_REQ();
    dib_t d; REQUIRE(setup_box(&d));
    HBRUSH br; HGDIOBJ old = use_brush(&d, RED, &br);
    CHECK(!U(ExtFloodFill)(d.dc, 2, 2, BLACK, FLOODFILLBORDER));
    CHECK(!U(ExtFloodFill)(d.dc, 5, 2, BLACK, FLOODFILLBORDER));
    drop_brush(&d, old, br);
    EXPECT_EQ(count_color(&d, RED), 0);
    dib_close(&d);
}
TEST(bitmaps_flood_start_out_of_bounds_fails) {
    FLOOD_REQ();
    dib_t d; REQUIRE(setup_box(&d));
    HBRUSH br; HGDIOBJ old = use_brush(&d, RED, &br);
    static const int p[][2] = { {-1,0},{0,-1},{W,0},{0,H},{1000,1000},{-1000,5} }; unsigned i;
    for (i = 0; i < 6; i++) {
        CHECKF(!U(ExtFloodFill)(d.dc, p[i][0], p[i][1], BLACK, FLOODFILLBORDER), "border %d,%d", p[i][0], p[i][1]);
        CHECKF(!U(ExtFloodFill)(d.dc, p[i][0], p[i][1], WHITE, FLOODFILLSURFACE), "surface %d,%d", p[i][0], p[i][1]);
    }
    drop_brush(&d, old, br);
    EXPECT_EQ(count_color(&d, RED), 0);
    dib_close(&d);
}
TEST(bitmaps_flood_old_api_is_border_mode) {
    FLOOD_REQ();
    dib_t d; REQUIRE(setup_box(&d));
    HBRUSH br; HGDIOBJ old = use_brush(&d, RED, &br);
    CHECK(U(FloodFill)(d.dc, 4, 4, BLACK));
    EXPECT_EQ(count_color(&d, RED), 16);
    CHECK(!U(FloodFill)(d.dc, 2, 2, BLACK));
    drop_brush(&d, old, br);
    dib_close(&d);
}
TEST(bitmaps_flood_leaks_through_gap) {
    FLOOD_REQ();
    dib_t d; REQUIRE(setup_box(&d));
    U(SetPixelV)(d.dc, 5, 2, WHITE);   /* hole in the top edge */
    HBRUSH br; HGDIOBJ old = use_brush(&d, RED, &br);
    CHECK(U(ExtFloodFill)(d.dc, 4, 4, BLACK, FLOODFILLBORDER));
    drop_brush(&d, old, br);
    EXPECT_EQ(count_color(&d, RED), 100 - 19);
    EXPECT_EQ(count_color(&d, BLACK), 19);
    dib_close(&d);
}
TEST(bitmaps_flood_full_surface_no_border) {
    FLOOD_REQ();
    dib_t d; REQUIRE(dib_open(&d, W, H, 32));
    clear(&d, WHITE);
    HBRUSH br; HGDIOBJ old = use_brush(&d, GREEN, &br);
    CHECK(U(ExtFloodFill)(d.dc, 5, 5, BLACK, FLOODFILLBORDER));
    drop_brush(&d, old, br);
    EXPECT_EQ(count_color(&d, GREEN), 100);
    dib_close(&d);
}
TEST(bitmaps_flood_surface_same_color_as_brush) {
    FLOOD_REQ();
    dib_t d; REQUIRE(setup_box(&d));
    HBRUSH br; HGDIOBJ old = use_brush(&d, WHITE, &br);
    BOOL r = U(ExtFloodFill)(d.dc, 4, 4, WHITE, FLOODFILLSURFACE);
    t_note("surface fill with brush == surface colour -> %d", r);
    drop_brush(&d, old, br);
    EXPECT_EQ(count_color(&d, WHITE), 100 - 20);
    dib_close(&d);
}
TEST(bitmaps_flood_border_color_equals_brush) {
    FLOOD_REQ();
    dib_t d; REQUIRE(setup_box(&d));
    HBRUSH br; HGDIOBJ old = use_brush(&d, BLACK, &br);
    CHECK(U(ExtFloodFill)(d.dc, 4, 4, BLACK, FLOODFILLBORDER));
    drop_brush(&d, old, br);
    EXPECT_EQ(count_color(&d, BLACK), 36);
    dib_close(&d);
}
TEST(bitmaps_flood_two_regions_only_one_filled) {
    FLOOD_REQ();
    dib_t d; REQUIRE(dib_open(&d, W, H, 32));
    clear(&d, WHITE);
    rect_border(&d, 0, 0, 3, 3, BLACK);
    rect_border(&d, 5, 5, 9, 9, BLACK);
    HBRUSH br; HGDIOBJ old = use_brush(&d, RED, &br);
    CHECK(U(ExtFloodFill)(d.dc, 1, 1, BLACK, FLOODFILLBORDER));
    drop_brush(&d, old, br);
    EXPECT_EQ(U(GetPixel)(d.dc, 1, 1), RED);
    EXPECT_EQ(U(GetPixel)(d.dc, 2, 2), RED);
    EXPECT_EQ(U(GetPixel)(d.dc, 7, 7), WHITE);
    EXPECT_EQ(count_color(&d, RED), 4);
    dib_close(&d);
}
TEST(bitmaps_flood_one_pixel_region) {
    FLOOD_REQ();
    dib_t d; REQUIRE(dib_open(&d, 5, 5, 32));
    clear(&d, WHITE);
    rect_border(&d, 1, 1, 3, 3, BLACK);
    HBRUSH br; HGDIOBJ old = use_brush(&d, RED, &br);
    CHECK(U(ExtFloodFill)(d.dc, 2, 2, BLACK, FLOODFILLBORDER));
    drop_brush(&d, old, br);
    EXPECT_EQ(count_color(&d, RED), 1);
    dib_close(&d);
}
TEST(bitmaps_flood_respects_clip_rect) {
    FLOOD_REQ(); REQUIRE(U(IntersectClipRect));
    dib_t d; REQUIRE(dib_open(&d, W, H, 32));
    clear(&d, WHITE);
    U(IntersectClipRect)(d.dc, 0, 0, 5, 10);
    HBRUSH br; HGDIOBJ old = use_brush(&d, RED, &br);
    CHECK(U(ExtFloodFill)(d.dc, 1, 1, BLACK, FLOODFILLBORDER));
    drop_brush(&d, old, br);
    EXPECT_EQ(count_color(&d, RED), 50);
    EXPECT_EQ(U(GetPixel)(d.dc, 4, 9), RED);
    dib_close(&d);
}
TEST(bitmaps_flood_start_outside_clip_fails) {
    FLOOD_REQ(); REQUIRE(U(IntersectClipRect));
    dib_t d; REQUIRE(dib_open(&d, W, H, 32));
    clear(&d, WHITE);
    U(IntersectClipRect)(d.dc, 0, 0, 5, 10);
    HBRUSH br; HGDIOBJ old = use_brush(&d, RED, &br);
    CHECK(!U(ExtFloodFill)(d.dc, 8, 8, BLACK, FLOODFILLBORDER));
    drop_brush(&d, old, br);
    dib_close(&d);
}
TEST(bitmaps_flood_does_not_move_current_position) {
    FLOOD_REQ(); REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    dib_t d; REQUIRE(setup_box(&d));
    U(MoveToEx)(d.dc, 3, 4, NULL);
    HBRUSH br; HGDIOBJ old = use_brush(&d, RED, &br);
    U(ExtFloodFill)(d.dc, 4, 4, BLACK, FLOODFILLBORDER);
    drop_brush(&d, old, br);
    POINT p; U(GetCurrentPositionEx)(d.dc, &p);
    EXPECT_EQ(p.x, 3); EXPECT_EQ(p.y, 4);
    dib_close(&d);
}
TEST(bitmaps_flood_invalid_filltype) {
    FLOOD_REQ();
    dib_t d; REQUIRE(setup_box(&d));
    HBRUSH br; HGDIOBJ old = use_brush(&d, RED, &br);
    static const UINT t[] = { 2, 3, 100, 0xFFFFFFFF }; unsigned i;
    for (i = 0; i < 4; i++) {
        BOOL r = U(ExtFloodFill)(d.dc, 4, 4, BLACK, t[i]);
        t_note("ExtFloodFill type %u -> %d", t[i], r);
    }
    drop_brush(&d, old, br);
    dib_close(&d);
}
TEST(bitmaps_flood_invalid_dc) {
    FLOOD_REQ();
    CHECK(!U(ExtFloodFill)(NULL, 0, 0, 0, FLOODFILLBORDER));
    CHECK(!U(ExtFloodFill)((HDC)0xdeadbeef, 0, 0, 0, FLOODFILLSURFACE));
    CHECK(!U(FloodFill)(NULL, 0, 0, 0));
    HDC dc = U(CreateCompatibleDC)(NULL);
    U(DeleteDC)(dc);
    CHECK(!U(ExtFloodFill)(dc, 0, 0, 0, FLOODFILLBORDER));
}
TEST(bitmaps_flood_8bpp_dib) {
    FLOOD_REQ();
    RGBQUAD t[256]; int i;
    for (i = 0; i < 256; i++) { t[i].rgbRed = t[i].rgbGreen = t[i].rgbBlue = 255; t[i].rgbReserved = 0; }
    t[0].rgbRed = t[0].rgbGreen = t[0].rgbBlue = 0;
    t[1].rgbRed = 255; t[1].rgbGreen = 0; t[1].rgbBlue = 0;
    t[2].rgbRed = 0; t[2].rgbGreen = 0; t[2].rgbBlue = 255;
    dib_t d; REQUIRE(dib_open_ex(&d, W, H, 8, 0, t, 256));
    clear(&d, WHITE);
    rect_border(&d, 2, 2, 7, 7, BLACK);
    HBRUSH br; HGDIOBJ old = use_brush(&d, BLUE, &br);
    BOOL r = U(ExtFloodFill)(d.dc, 4, 4, BLACK, FLOODFILLBORDER);
    CHECK(r);
    drop_brush(&d, old, br);
    EXPECT_EQ(count_color(&d, BLACK), 20);
    EXPECT_EQ(U(GetPixel)(d.dc, 4, 4), 0xFF0000 /* RGB(0,0,255) */);
    dib_close(&d);
}
TEST(bitmaps_flood_24bpp_dib) {
    FLOOD_REQ();
    dib_t d; REQUIRE(dib_open(&d, W, H, 24));
    clear(&d, WHITE);
    rect_border(&d, 2, 2, 7, 7, 0x123456);
    HBRUSH br; HGDIOBJ old = use_brush(&d, 0x654321, &br);
    CHECK(U(ExtFloodFill)(d.dc, 4, 4, 0x123456, FLOODFILLBORDER));
    drop_brush(&d, old, br);
    EXPECT_EQ(count_color(&d, 0x654321), 16);
    dib_close(&d);
}
TEST(bitmaps_flood_16bpp_565_dib) {
    FLOOD_REQ();
    dib_t d; REQUIRE(dib_open_ex(&d, W, H, 16, 1, NULL, 0));
    clear(&d, WHITE);
    rect_border(&d, 2, 2, 7, 7, BLACK);
    HBRUSH br; HGDIOBJ old = use_brush(&d, RED, &br);
    CHECK(U(ExtFloodFill)(d.dc, 4, 4, BLACK, FLOODFILLBORDER));
    drop_brush(&d, old, br);
    EXPECT_EQ(count_color(&d, RED), 16);
    dib_close(&d);
}
TEST(bitmaps_flood_mono_ddb) {
    FLOOD_REQ(); REQUIRE(U(GetBitmapBits));
    ddb_t d; REQUIRE(ddb_open(&d, 16, 8, 1, 1, NULL));
    U(PatBlt)(d.dc, 0, 0, 16, 8, WHITENESS);
    int x, y;
    for (x = 2; x <= 9; x++) { U(SetPixelV)(d.dc, x, 1, 0); U(SetPixelV)(d.dc, x, 6, 0); }
    for (y = 1; y <= 6; y++) { U(SetPixelV)(d.dc, 2, y, 0); U(SetPixelV)(d.dc, 9, y, 0); }
    HBRUSH br = U(CreateSolidBrush)(0);
    HGDIOBJ old = U(SelectObject)(d.dc, br);
    CHECK(U(ExtFloodFill)(d.dc, 5, 3, 0, FLOODFILLBORDER));
    U(SelectObject)(d.dc, old); U(DeleteObject)(br);
    unsigned char out[16]; U(GetBitmapBits)(d.bm, 16, out);
    EXPECT_EQ(out[0], 0xFF);          /* row 0 untouched */
    EXPECT_EQ(out[4], 0xC0); EXPECT_EQ(out[5], 0x3F);   /* row 2: x2..x9 black */
    EXPECT_EQ(out[2 * 6], 0xC0); EXPECT_EQ(out[2 * 7], 0xFF);
    ddb_close(&d);
}
TEST(bitmaps_flood_idempotent_second_call) {
    FLOOD_REQ();
    dib_t d; REQUIRE(setup_box(&d));
    HBRUSH br; HGDIOBJ old = use_brush(&d, RED, &br);
    CHECK(U(ExtFloodFill)(d.dc, 4, 4, BLACK, FLOODFILLBORDER));
    BOOL r = U(ExtFloodFill)(d.dc, 4, 4, BLACK, FLOODFILLBORDER);
    t_note("second border fill of an already filled region -> %d", r);
    drop_brush(&d, old, br);
    EXPECT_EQ(count_color(&d, RED), 16);
    dib_close(&d);
}
TEST(bitmaps_flood_diagonal_gap_note) {
    FLOOD_REQ();
    dib_t d; REQUIRE(dib_open(&d, 6, 6, 32));
    clear(&d, WHITE);
    int i;
    for (i = 0; i < 6; i++) U(SetPixelV)(d.dc, i, 5 - i, BLACK);   /* anti-diagonal */
    HBRUSH br; HGDIOBJ old = use_brush(&d, RED, &br);
    U(ExtFloodFill)(d.dc, 0, 0, BLACK, FLOODFILLBORDER);
    drop_brush(&d, old, br);
    int red = count_color(&d, RED);
    t_note("diagonal line: filled %d of 15 upper-left pixels (15 => 4-connected)", red);
    CHECK(red == 15 || red == 30);
    dib_close(&d);
}
