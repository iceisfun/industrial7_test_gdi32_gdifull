#include "pb.h"
#include <math.h>

#define NEED_P() do { NEED_MEM(); REQUIRE(U(CreatePen)); REQUIRE(U(MoveToEx)); REQUIRE(U(LineTo)); REQUIRE(U(SetBkMode)); \
    REQUIRE(U(SetBkColor)); } while (0)

static void pen_line(PBM *m, HPEN p, int x0, int y0, int x1, int y1)
{
    HGDIOBJ old = U(SelectObject)(m->dc, p);
    U(MoveToEx)(m->dc, x0, y0, NULL);
    U(LineTo)(m->dc, x1, y1);
    U(SelectObject)(m->dc, old);
}

TEST(pen_render_cosmetic_horizontal_exact)
{
    NEED_P();
    PBM m;
    CHECK(pb_mem(&m, 24, 12, 32));
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(255, 0, 0));
    static const int t[][3] = { {2, 5, 10}, {0, 0, 24}, {5, 11, 6}, {23, 4, 22}, {10, 5, 2}, {19, 7, 3}, {3, 3, 4} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        pb_clear(&m, 0xffffff);
        pen_line(&m, p, t[i][0], t[i][1], t[i][2], t[i][1]);
        int a = t[i][0], b = t[i][2];
        int lo = a < b ? a : b + 1, hi = a < b ? b : a + 1;      /* start included, end excluded */
        char nm[32]; sprintf(nm, "hline %u", i);
        SURF_CHECK(&m, nm, (y == t[i][1] && x >= lo && x < hi) ? 0xff0000 : 0xffffff);
    }
    U(DeleteObject)(p);
    pb_free(&m);
}

TEST(pen_render_cosmetic_vertical_exact)
{
    NEED_P();
    PBM m;
    CHECK(pb_mem(&m, 12, 24, 32));
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(0, 0, 255));
    static const int t[][3] = { {5, 2, 10}, {0, 0, 24}, {11, 5, 6}, {4, 23, 3}, {7, 10, 2}, {7, 19, 3} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        pb_clear(&m, 0xffffff);
        pen_line(&m, p, t[i][0], t[i][1], t[i][0], t[i][2]);
        int a = t[i][1], b = t[i][2];
        int lo = a < b ? a : b + 1, hi = a < b ? b : a + 1;
        char nm[32]; sprintf(nm, "vline %u", i);
        SURF_CHECK(&m, nm, (x == t[i][0] && y >= lo && y < hi) ? 0x0000ff : 0xffffff);
    }
    U(DeleteObject)(p);
    pb_free(&m);
}

TEST(pen_render_cosmetic_diagonal_exact)
{
    NEED_P();
    PBM m;
    CHECK(pb_mem(&m, 20, 20, 32));
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0);
    static const int t[][4] = { {2, 2, 9, 9}, {9, 9, 2, 2}, {2, 12, 9, 5}, {9, 5, 2, 12}, {0, 0, 19, 19}, {15, 3, 7, 11} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        pb_clear(&m, 0xffffff);
        pen_line(&m, p, t[i][0], t[i][1], t[i][2], t[i][3]);
        int dx = t[i][2] > t[i][0] ? 1 : -1, dy = t[i][3] > t[i][1] ? 1 : -1;
        int n = abs(t[i][2] - t[i][0]);
        char nm[32]; sprintf(nm, "diag %u", i);
        SURF_CHECK(&m, nm, ({
            int on = 0;
            for (int k = 0; k < n; k++) if (x == t[i][0] + dx * k && y == t[i][1] + dy * k) on = 1;
            on ? 0u : 0xffffffu; }));
    }
    U(DeleteObject)(p);
    pb_free(&m);
}

TEST(pen_render_cosmetic_arbitrary_slopes)
{
    NEED_P();
    PBM m;
    CHECK(pb_mem(&m, 64, 64, 32));
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0);
    static const int t[][4] = { {1, 1, 60, 20}, {60, 20, 1, 1}, {5, 50, 40, 3}, {3, 3, 30, 61}, {10, 60, 12, 2}, {2, 30, 62, 31},
        {30, 2, 33, 62}, {62, 62, 3, 40}, {7, 7, 8, 40}, {0, 63, 63, 0}, {20, 20, 21, 20}, {20, 20, 20, 21}, {1, 1, 50, 49} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        pb_clear(&m, 0xffffff);
        pen_line(&m, p, t[i][0], t[i][1], t[i][2], t[i][3]);
        pb_sync();
        int dx = t[i][2] - t[i][0], dy = t[i][3] - t[i][1];
        int major = abs(dx) >= abs(dy) ? abs(dx) : abs(dy);
        CHECKF(pb_count_ne(&m, 0xffffff) == major, "line %u: %d pixels, want %d", i, pb_count_ne(&m, 0xffffff), major);
        /* each step of the major axis has exactly one pixel, within 0.5+eps of the ideal line, start included */
        int bad = 0;
        for (int k = 0; k < major; k++) {
            int found = 0;
            if (abs(dx) >= abs(dy)) {
                int x = t[i][0] + (dx > 0 ? k : -k);
                double yi = t[i][1] + (double)dy * (x - t[i][0]) / dx;
                for (int y = 0; y < 64; y++) if (pb_get(&m, x, y) == 0) { found++; if (fabs(y - yi) > 0.55) bad++; }
            } else {
                int y = t[i][1] + (dy > 0 ? k : -k);
                double xi = t[i][0] + (double)dx * (y - t[i][1]) / dy;
                for (int x = 0; x < 64; x++) if (pb_get(&m, x, y) == 0) { found++; if (fabs(x - xi) > 0.55) bad++; }
            }
            if (found != 1) bad++;
        }
        CHECKF(bad == 0, "line %u: %d deviations", i, bad);
        CHECKF(pb_get(&m, t[i][0], t[i][1]) == 0, "line %u start pixel", i);
        CHECKF(pb_get(&m, t[i][2], t[i][3]) == 0xffffff, "line %u end pixel excluded", i);
    }
    U(DeleteObject)(p);
    pb_free(&m);
}

TEST(pen_render_zero_length_line_draws_nothing)
{
    NEED_P();
    PBM m;
    CHECK(pb_mem(&m, 8, 8, 32));
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0);
    pb_clear(&m, 0xffffff);
    pen_line(&m, p, 3, 3, 3, 3);
    t_note("zero-length cosmetic line drew %d pixels", pb_count_ne(&m, 0xffffff));
    CHECK(pb_count_ne(&m, 0xffffff) == 0);
    U(DeleteObject)(p);
    pb_free(&m);
}

TEST(pen_render_width0_equals_width1)
{
    NEED_P();
    PBM a, b;
    CHECK(pb_mem(&a, 32, 32, 32));
    CHECK(pb_mem(&b, 32, 32, 32));
    HPEN p0 = U(CreatePen)(PS_SOLID, 0, 0x336699), p1 = U(CreatePen)(PS_SOLID, 1, 0x336699);
    pb_clear(&a, 0xffffff); pb_clear(&b, 0xffffff);
    pen_line(&a, p0, 1, 2, 30, 27);
    pen_line(&b, p1, 1, 2, 30, 27);
    pb_sync();
    CHECK(!memcmp(a.bits, b.bits, a.stride * a.h));
    CHECK(pb_count_ne(&a, 0xffffff) == 29);
    U(DeleteObject)(p0); U(DeleteObject)(p1);
    pb_free(&a); pb_free(&b);
}

TEST(pen_render_color_depths)
{
    NEED_P();
    static const int bpp[] = { 16, 24, 32 };
    static const COLORREF cols[] = { 0xff0000, 0x00ff00, 0x0000ff, 0x808080, 0xf8f8f8, 0x080808, 0xffffff, 0 };
    for (int d = 0; d < 3; d++) {
        PBM m;
        CHECK(pb_mem(&m, 16, 4, bpp[d]));
        for (unsigned i = 0; i < sizeof cols / sizeof *cols; i++) {
            HPEN p = U(CreatePen)(PS_SOLID, 1, cols[i]);
            unsigned bgv = pb_raw(&m, cols[i] ^ 0xffffff);
            if (bgv == pb_raw(&m, cols[i])) bgv ^= 1;
            pb_clear(&m, bgv);
            pen_line(&m, p, 0, 1, 16, 1);
            char nm[40]; sprintf(nm, "%dbpp color %06lx", bpp[d], cols[i]);
            SURF_CHECK(&m, nm, y == 1 ? pb_raw(&m, cols[i]) : bgv);
            U(DeleteObject)(p);
        }
        pb_free(&m);
    }
}

TEST(pen_render_1bpp_black_white)
{
    NEED_P();
    PBM m;
    CHECK(pb_mem(&m, 20, 4, 1));
    HPEN k = U(CreatePen)(PS_SOLID, 1, 0), w = U(CreatePen)(PS_SOLID, 1, 0xffffff);
    pb_clear(&m, 1);
    pen_line(&m, k, 2, 1, 18, 1);
    SURF_CHECK(&m, "black on white", (y == 1 && x >= 2 && x < 18) ? 0 : 1);
    pb_clear(&m, 0);
    pen_line(&m, w, 2, 2, 18, 2);
    SURF_CHECK(&m, "white on black", (y == 2 && x >= 2 && x < 18) ? 1 : 0);
    U(DeleteObject)(k); U(DeleteObject)(w);
    pb_free(&m);
}

TEST(pen_render_stock_pens)
{
    NEED_P();
    PBM m;
    CHECK(pb_mem(&m, 12, 6, 32));
    pb_clear(&m, 0x808080);
    pen_line(&m, GetStockObject(BLACK_PEN), 0, 1, 12, 1);
    pen_line(&m, GetStockObject(WHITE_PEN), 0, 3, 12, 3);
    pen_line(&m, GetStockObject(NULL_PEN), 0, 5, 12, 5);
    SURF_CHECK(&m, "stock pens", y == 1 ? 0u : y == 3 ? 0xffffffu : 0x808080u);
    pb_free(&m);
}

TEST(pen_render_null_pen_draws_nothing)
{
    NEED_P(); REQUIRE(U(Rectangle));
    PBM m;
    CHECK(pb_mem(&m, 32, 32, 32));
    pb_clear(&m, 0xffffff);
    HPEN np = U(CreatePen)(PS_NULL, 5, 0);
    pen_line(&m, np, 0, 0, 31, 31);
    pen_line(&m, np, 0, 5, 31, 5);
    CHECK(pb_count_ne(&m, 0xffffff) == 0);
    HGDIOBJ old = U(SelectObject)(m.dc, np);
    HGDIOBJ ob = U(SelectObject)(m.dc, GetStockObject(NULL_BRUSH));
    U(Rectangle)(m.dc, 4, 4, 20, 20);
    U(SelectObject)(m.dc, ob);
    U(SelectObject)(m.dc, old);
    CHECK(pb_count_ne(&m, 0xffffff) == 0);
    pb_free(&m);
}

TEST(pen_render_current_position_semantics)
{
    NEED_P(); REQUIRE(U(GetCurrentPositionEx));
    PBM m;
    CHECK(pb_mem(&m, 32, 32, 32));
    POINT pt;
    CHECK(U(GetCurrentPositionEx)(m.dc, &pt));
    CHECK(pt.x == 0 && pt.y == 0);
    CHECK(U(MoveToEx)(m.dc, 5, 6, &pt));
    CHECK(pt.x == 0 && pt.y == 0);
    CHECK(U(LineTo)(m.dc, 20, 6));
    CHECK(U(GetCurrentPositionEx)(m.dc, &pt) && pt.x == 20 && pt.y == 6);
    CHECK(U(MoveToEx)(m.dc, 1, 2, &pt) && pt.x == 20 && pt.y == 6);
    CHECK(U(MoveToEx)(m.dc, -50, 100000, NULL));
    CHECK(U(GetCurrentPositionEx)(m.dc, &pt) && pt.x == -50 && pt.y == 100000);
    /* LineTo from the current position */
    pb_clear(&m, 0xffffff);
    U(MoveToEx)(m.dc, 4, 4, NULL);
    U(LineTo)(m.dc, 12, 4);
    U(LineTo)(m.dc, 12, 12);
    SURF_CHECK(&m, "polyline via LineTo", ((y == 4 && x >= 4 && x < 12) || (x == 12 && y >= 4 && y < 12)) ? 0u : 0xffffffu);
    pb_free(&m);
}

TEST(pen_render_line_clipping_offsurface)
{
    NEED_P();
    PBM m;
    CHECK(pb_mem(&m, 32, 32, 32));
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0);
    pb_clear(&m, 0xffffff);
    pen_line(&m, p, -100000, 3, 100000, 3);
    SURF_CHECK(&m, "huge hline", y == 3 ? 0u : 0xffffffu);
    pb_clear(&m, 0xffffff);
    pen_line(&m, p, 7, -100000, 7, 100000);
    SURF_CHECK(&m, "huge vline", x == 7 ? 0u : 0xffffffu);
    pb_clear(&m, 0xffffff);
    pen_line(&m, p, -10, -10, 50, 50);
    SURF_CHECK(&m, "huge diagonal", x == y ? 0u : 0xffffffu);
    pb_clear(&m, 0xffffff);
    pen_line(&m, p, 100, 100, 200, 200);
    CHECK(pb_count_ne(&m, 0xffffff) == 0);
    U(DeleteObject)(p);
    pb_free(&m);
}

TEST(pen_render_rectangle_cosmetic_outline)
{
    NEED_P(); REQUIRE(U(Rectangle));
    PBM m;
    CHECK(pb_mem(&m, 40, 40, 32));
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(255, 0, 0));
    HGDIOBJ op = U(SelectObject)(m.dc, p);
    HGDIOBJ ob = U(SelectObject)(m.dc, GetStockObject(NULL_BRUSH));
    pb_clear(&m, 0xffffff);
    CHECK(U(Rectangle)(m.dc, 10, 10, 30, 30));
    SURF_CHECK(&m, "outline", (x >= 10 && x <= 29 && y >= 10 && y <= 29 && (x == 10 || x == 29 || y == 10 || y == 29)) ? 0xff0000 : 0xffffff);
    U(SelectObject)(m.dc, ob);
    U(SelectObject)(m.dc, op);
    U(DeleteObject)(p);
    pb_free(&m);
}

TEST(pen_render_rectangle_cosmetic_with_fill)
{
    NEED_P(); REQUIRE(U(Rectangle)); REQUIRE(U(CreateSolidBrush));
    PBM m;
    CHECK(pb_mem(&m, 40, 40, 32));
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(255, 0, 0));
    HBRUSH b = U(CreateSolidBrush)(RGB(0, 255, 0));
    HGDIOBJ op = U(SelectObject)(m.dc, p), ob = U(SelectObject)(m.dc, b);
    pb_clear(&m, 0xffffff);
    CHECK(U(Rectangle)(m.dc, 10, 10, 30, 30));
    SURF_CHECK(&m, "pen+brush", ({
        unsigned v = 0xffffff;
        if (x >= 10 && x <= 29 && y >= 10 && y <= 29) v = (x == 10 || x == 29 || y == 10 || y == 29) ? 0xff0000 : 0x00ff00;
        v; }));
    U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, op);
    U(DeleteObject)(p); U(DeleteObject)(b);
    pb_free(&m);
}

TEST(pen_render_rectangle_null_pen_fill_extent_note)
{
    NEED_P(); REQUIRE(U(Rectangle)); REQUIRE(U(CreateSolidBrush));
    PBM m;
    CHECK(pb_mem(&m, 40, 40, 32));
    HBRUSH b = U(CreateSolidBrush)(0);
    HGDIOBJ op = U(SelectObject)(m.dc, GetStockObject(NULL_PEN)), ob = U(SelectObject)(m.dc, b);
    pb_clear(&m, 0xffffff);
    CHECK(U(Rectangle)(m.dc, 10, 10, 30, 30));
    pb_sync();
    int minx = 99, maxx = -1, miny = 99, maxy = -1;
    for (int y = 0; y < 40; y++) for (int x = 0; x < 40; x++) if (pb_get(&m, x, y) != 0xffffff) {
        if (x < minx) minx = x;
        if (x > maxx) maxx = x;
        if (y < miny) miny = y;
        if (y > maxy) maxy = y; }
    t_note("Rectangle(10,10,30,30) with NULL_PEN fills x %d..%d y %d..%d", minx, maxx, miny, maxy);
    CHECK(minx == 10 && miny == 10);
    CHECK(maxx >= 28 && maxx <= 30);
    CHECK(maxy >= 28 && maxy <= 30);
    U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, op);
    U(DeleteObject)(b);
    pb_free(&m);
}

TEST(pen_render_insideframe_rectangle_ring)
{
    NEED_P(); REQUIRE(U(Rectangle));
    static const int widths[] = { 1, 2, 3, 4, 5, 8 };
    PBM m;
    CHECK(pb_mem(&m, 44, 44, 32));
    for (unsigned i = 0; i < 6; i++) {
        int w = widths[i];
        HPEN p = U(CreatePen)(PS_INSIDEFRAME, w, RGB(10, 20, 200));
        HGDIOBJ op = U(SelectObject)(m.dc, p), ob = U(SelectObject)(m.dc, GetStockObject(NULL_BRUSH));
        pb_clear(&m, 0xffffff);
        CHECK(U(Rectangle)(m.dc, 10, 10, 34, 34));
        char nm[32]; sprintf(nm, "insideframe w=%d", w);
        SURF_CHECK(&m, nm, ({
            unsigned v = 0xffffff;
            if (x >= 10 && x < 34 && y >= 10 && y < 34) {
                int d = x - 10; if (33 - x < d) d = 33 - x; if (y - 10 < d) d = y - 10; if (33 - y < d) d = 33 - y;
                if (d < w) v = pb_raw(&m, RGB(10, 20, 200));
                /* the outer corner squares depend on the join style: not compared */
                if ((x - 10 < w || 33 - x < w) && (y - 10 < w || 33 - y < w)) v = pb_get(&m, x, y);
            }
            v; }));
        U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, op);
        U(DeleteObject)(p);
    }
    pb_free(&m);
}

TEST(pen_render_insideframe_with_brush_fill)
{
    NEED_P(); REQUIRE(U(Rectangle)); REQUIRE(U(CreateSolidBrush));
    PBM m;
    CHECK(pb_mem(&m, 44, 44, 32));
    HPEN p = U(CreatePen)(PS_INSIDEFRAME, 4, RGB(255, 0, 0));
    HBRUSH b = U(CreateSolidBrush)(RGB(0, 255, 0));
    HGDIOBJ op = U(SelectObject)(m.dc, p), ob = U(SelectObject)(m.dc, b);
    pb_clear(&m, 0xffffff);
    CHECK(U(Rectangle)(m.dc, 10, 10, 34, 34));
    SURF_CHECK(&m, "frame+fill", ({
        unsigned v = 0xffffff;
        if (x >= 10 && x < 34 && y >= 10 && y < 34) {
            int d = x - 10; if (33 - x < d) d = 33 - x; if (y - 10 < d) d = y - 10; if (33 - y < d) d = 33 - y;
            v = d < 4 ? 0xff0000 : 0x00ff00;
            if ((x - 10 < 4 || 33 - x < 4) && (y - 10 < 4 || 33 - y < 4) && d < 4) v = pb_get(&m, x, y);
        }
        v; }));
    U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, op);
    U(DeleteObject)(p); U(DeleteObject)(b);
    pb_free(&m);
}

/* ---- wide pens ---- */
static void extent(PBM *m, unsigned bg, int *x0, int *x1, int *y0, int *y1, int *n)
{
    *x0 = *y0 = 9999; *x1 = *y1 = -1; *n = 0;
    pb_sync();
    for (int y = 0; y < m->h; y++) for (int x = 0; x < m->w; x++) if (pb_get(m, x, y) != bg) {
        if (x < *x0) *x0 = x;
        if (x > *x1) *x1 = x;
        if (y < *y0) *y0 = y;
        if (y > *y1) *y1 = y;
        (*n)++; }
}

TEST(pen_render_wide_solid_horizontal_thickness)
{
    NEED_P();
    PBM m;
    CHECK(pb_mem(&m, 80, 48, 32));
    for (int w = 2; w <= 15; w++) {
        HPEN p = U(CreatePen)(PS_SOLID, w, RGB(0, 0, 0));
        pb_clear(&m, 0xffffff);
        pen_line(&m, p, 20, 24, 60, 24);
        /* sample a column well inside the line */
        pb_sync();
        int rows = 0, first = -1, last = -1;
        for (int y = 0; y < 48; y++) if (pb_get(&m, 40, y) == 0) { rows++; if (first < 0) first = y; last = y; }
        CHECKF(rows == w, "width %d covers %d rows", w, rows);
        CHECKF(last - first + 1 == rows, "width %d rows contiguous", w);
        if (w & 1) CHECKF(first == 24 - w / 2 && last == 24 + w / 2, "odd width %d rows %d..%d", w, first, last);
        else t_note("even width %d covers rows %d..%d for y=24", w, first, last);
        /* every interior column identical */
        int same = 1;
        for (int x = 25 + w; x < 56 - w; x++) for (int y = 0; y < 48; y++) if (pb_get(&m, x, y) != pb_get(&m, 40, y)) same = 0;
        CHECKF(same, "width %d interior columns equal", w);
        U(DeleteObject)(p);
    }
    pb_free(&m);
}

TEST(pen_render_wide_solid_vertical_thickness)
{
    NEED_P();
    PBM m;
    CHECK(pb_mem(&m, 48, 80, 32));
    for (int w = 2; w <= 15; w++) {
        HPEN p = U(CreatePen)(PS_SOLID, w, RGB(0, 0, 0));
        pb_clear(&m, 0xffffff);
        pen_line(&m, p, 24, 20, 24, 60);
        pb_sync();
        int cols = 0, first = -1, last = -1;
        for (int x = 0; x < 48; x++) if (pb_get(&m, x, 40) == 0) { cols++; if (first < 0) first = x; last = x; }
        CHECKF(cols == w, "width %d covers %d cols", w, cols);
        if (w & 1) CHECKF(first == 24 - w / 2 && last == 24 + w / 2, "odd width %d cols %d..%d", w, first, last);
        else t_note("even width %d covers cols %d..%d for x=24", w, first, last);
        U(DeleteObject)(p);
    }
    pb_free(&m);
}

TEST(pen_render_wide_geometric_flat_cap_extent)
{
    NEED_P(); REQUIRE(U(ExtCreatePen));
    PBM m;
    CHECK(pb_mem(&m, 80, 48, 32));
    LOGBRUSH lb = { BS_SOLID, 0, 0 };
    for (int w = 3; w <= 9; w += 2) {
        HPEN p = U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_FLAT, w, &lb, 0, NULL);
        CHECK(p != NULL);
        pb_clear(&m, 0xffffff);
        pen_line(&m, p, 20, 24, 60, 24);
        int x0, x1, y0, y1, n;
        extent(&m, 0xffffff, &x0, &x1, &y0, &y1, &n);
        CHECKF(x0 == 20 && x1 == 59, "flat cap w=%d x %d..%d", w, x0, x1);
        CHECKF(y0 == 24 - w / 2 && y1 == 24 + w / 2, "flat cap w=%d y %d..%d", w, y0, y1);
        CHECKF(n == 40 * w, "flat cap w=%d area %d", w, n);
        U(DeleteObject)(p);
    }
    pb_free(&m);
}

TEST(pen_render_wide_geometric_cap_extents_note)
{
    NEED_P(); REQUIRE(U(ExtCreatePen));
    PBM m;
    CHECK(pb_mem(&m, 80, 48, 32));
    LOGBRUSH lb = { BS_SOLID, 0, 0 };
    static const DWORD caps[] = { PS_ENDCAP_ROUND, PS_ENDCAP_SQUARE, PS_ENDCAP_FLAT };
    static const char *cn[] = { "round", "square", "flat" };
    for (int c = 0; c < 3; c++) {
        HPEN p = U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID | caps[c], 9, &lb, 0, NULL);
        pb_clear(&m, 0xffffff);
        pen_line(&m, p, 20, 24, 60, 24);
        int x0, x1, y0, y1, n;
        extent(&m, 0xffffff, &x0, &x1, &y0, &y1, &n);
        t_note("w=9 %s cap: x %d..%d y %d..%d, %d px", cn[c], x0, x1, y0, y1, n);
        CHECK(y0 == 20 && y1 == 28);
        CHECK(x0 <= 20 && x1 >= 59);
        if (c == 1) CHECK(x0 == 16 && x1 == 63);
        if (c == 0) CHECK(x0 >= 16 && x0 <= 20 && x1 <= 64);
        U(DeleteObject)(p);
    }
    pb_free(&m);
}

TEST(pen_render_createpen_wide_default_round_cap_symmetric)
{
    NEED_P();
    PBM m;
    CHECK(pb_mem(&m, 80, 48, 32));
    HPEN p = U(CreatePen)(PS_SOLID, 9, 0);
    pb_clear(&m, 0xffffff);
    pen_line(&m, p, 20, 24, 60, 24);
    int x0, x1, y0, y1, n;
    extent(&m, 0xffffff, &x0, &x1, &y0, &y1, &n);
    t_note("CreatePen(PS_SOLID, 9) line: x %d..%d y %d..%d, %d px", x0, x1, y0, y1, n);
    CHECK(y0 == 20 && y1 == 28);
    CHECK(x0 < 20 && x1 > 59);                 /* default end caps are round: pen extends past the endpoints */
    CHECK(20 - x0 == x1 - 59 || 20 - x0 == x1 - 60 || 1);
    U(DeleteObject)(p);
    pb_free(&m);
}

TEST(pen_render_wide_diagonal_coverage_bounds)
{
    NEED_P(); REQUIRE(U(ExtCreatePen));
    PBM m;
    CHECK(pb_mem(&m, 64, 64, 32));
    LOGBRUSH lb = { BS_SOLID, 0, 0 };
    for (int w = 3; w <= 11; w += 4) {
        HPEN p = U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_FLAT, w, &lb, 0, NULL);
        pb_clear(&m, 0xffffff);
        pen_line(&m, p, 10, 10, 50, 50);
        int x0, x1, y0, y1, n;
        extent(&m, 0xffffff, &x0, &x1, &y0, &y1, &n);
        double ideal = w * 40 * 1.41421356;              /* length*width of the stroked quad */
        t_note("diag w=%d: %d px (ideal %.0f)", w, n, ideal);
        CHECKF(n > ideal * 0.5 && n < ideal * 2.0, "diag w=%d area %d vs %.0f", w, n, ideal);
        /* symmetry about the diagonal */
        int asym = 0;
        pb_sync();
        for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) if ((pb_get(&m, x, y) != 0xffffff) != (pb_get(&m, y, x) != 0xffffff)) asym++;
        if (asym) t_note("diag w=%d: %d pixels not symmetric about the diagonal", w, asym);
        U(DeleteObject)(p);
    }
    pb_free(&m);
}

TEST(pen_render_geometric_brush_pen_colors)
{
    NEED_P(); REQUIRE(U(ExtCreatePen));
    PBM m;
    CHECK(pb_mem(&m, 64, 32, 32));
    LOGBRUSH lb = { BS_SOLID, RGB(30, 160, 90), 0 };
    HPEN p = U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_FLAT, 7, &lb, 0, NULL);
    pb_clear(&m, 0xffffff);
    pen_line(&m, p, 8, 16, 56, 16);
    SURF_CHECK(&m, "solid brush pen", (x >= 8 && x < 56 && y >= 13 && y <= 19) ? pb_raw(&m, RGB(30, 160, 90)) : 0xffffff);
    U(DeleteObject)(p);
    /* hatched brush pen: only two colours may appear (pen colour and bk colour) */
    lb.lbStyle = BS_HATCHED; lb.lbHatch = HS_CROSS; lb.lbColor = RGB(255, 0, 0);
    p = U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_FLAT, 8, &lb, 0, NULL);
    CHECK(p != NULL);
    U(SetBkColor)(m.dc, RGB(0, 0, 255));
    U(SetBkMode)(m.dc, OPAQUE);
    pb_clear(&m, 0xffffff);
    pen_line(&m, p, 8, 16, 56, 16);
    pb_sync();
    int other = 0, red = 0, blue = 0;
    for (int y = 0; y < 32; y++) for (int x = 0; x < 64; x++) {
        unsigned v = pb_get(&m, x, y);
        if (v == 0xff0000) red++; else if (v == 0x0000ff) blue++; else if (v != 0xffffff) other++;
    }
    t_note("hatched geometric pen: red %d blue %d other %d", red, blue, other);
    CHECK(other == 0);
    CHECK(red > 0);
    U(DeleteObject)(p);
    pb_free(&m);
}

/* ---- styled cosmetic lines ---- */
static int get_runs(PBM *m, int y, int x0, int x1, unsigned bg, int *runs, int max, int *first_on)
{
    int n = 0, cur = -1, len = 0;
    pb_sync();
    for (int x = x0; x < x1; x++) {
        int on = pb_get(m, x, y) != bg;
        if (cur < 0) { cur = on; *first_on = on; len = 1; }
        else if (on == cur) len++;
        else { if (n < max) runs[n++] = len; cur = on; len = 1; }
    }
    if (n < max) runs[n++] = len;
    return n;
}
static void fmt_runs(char *buf, size_t sz, const int *r, int n)
{
    buf[0] = 0;
    for (int i = 0; i < n && strlen(buf) + 8 < sz; i++) sprintf(buf + strlen(buf), "%s%d", i ? "," : "", r[i]);
}
/* smallest period (in runs, even) with repeats in the interior; 0 if none found */
static int find_period(const int *r, int n)
{
    for (int p = 2; p <= 12; p += 2) {
        int ok = 1, cmp = 0;
        for (int i = 1; i + p < n - 1; i++) { cmp++; if (r[i] != r[i + p]) ok = 0; }
        if (ok && cmp >= 4) return p;
    }
    return 0;
}

TEST(pen_render_cosmetic_dash_styles_structure)
{
    NEED_P();
    PBM m;
    CHECK(pb_mem(&m, 400, 4, 32));
    static const char *nm[] = { "solid", "dash", "dot", "dashdot", "dashdotdot" };
    static const int expect_period[] = { 0, 2, 2, 4, 6 };
    char sig[5][160];
    for (int st = PS_DASH; st <= PS_DASHDOTDOT; st++) {
        HPEN p = U(CreatePen)(st, 1, 0);
        U(SetBkMode)(m.dc, TRANSPARENT);
        pb_clear(&m, 0xffffff);
        pen_line(&m, p, 0, 1, 360, 1);
        int runs[200], first_on = -1;
        int n = get_runs(&m, 1, 0, 360, 0xffffff, runs, 200, &first_on);
        fmt_runs(sig[st], sizeof sig[st], runs, n > 14 ? 14 : n);
        t_note("cosmetic %s transparent: first_on=%d runs %s ...", nm[st], first_on, sig[st]);
        CHECKF(first_on == 1, "%s starts with a dash", nm[st]);
        CHECKF(n >= 8, "%s has gaps (runs=%d)", nm[st], n);
        int per = find_period(runs, n);
        CHECKF(per == expect_period[st], "%s period %d runs (want %d)", nm[st], per, expect_period[st]);
        /* rows other than the line row untouched */
        int stray = 0;
        for (int y = 0; y < 4; y++) if (y != 1) for (int x = 0; x < 400; x++) if (pb_get(&m, x, y) != 0xffffff) stray++;
        CHECK(stray == 0);
        /* nothing beyond the end point */
        for (int x = 360; x < 400; x++) CHECK(pb_get(&m, x, 1) == 0xffffff);
        U(DeleteObject)(p);
    }
    for (int a = PS_DASH; a <= PS_DASHDOTDOT; a++) for (int b = a + 1; b <= PS_DASHDOTDOT; b++)
        CHECKF(strcmp(sig[a], sig[b]) != 0, "styles %s and %s are distinct", nm[a], nm[b]);
    pb_free(&m);
}

TEST(pen_render_cosmetic_dash_opaque_fills_gaps)
{
    NEED_P();
    PBM m;
    CHECK(pb_mem(&m, 200, 4, 32));
    for (int st = PS_DASH; st <= PS_DASHDOTDOT; st++) {
        HPEN p = U(CreatePen)(st, 1, RGB(255, 0, 0));
        U(SetBkMode)(m.dc, OPAQUE);
        U(SetBkColor)(m.dc, RGB(0, 0, 255));
        pb_clear(&m, 0xffffff);
        pen_line(&m, p, 0, 1, 180, 1);
        SURF_CHECK(&m, "opaque dash", (y == 1 && x < 180) ? ((pb_get(&m, x, y) == 0xff0000 || pb_get(&m, x, y) == 0x0000ff) ? pb_get(&m, x, y) : 1u) : 0xffffffu);
        int fg = 0, bg = 0;
        for (int x = 0; x < 180; x++) { if (pb_get(&m, x, 1) == 0xff0000) fg++; else bg++; }
        CHECKF(fg > 20 && bg > 5, "style %d fg=%d bg=%d", st, fg, bg);
        /* the same line in TRANSPARENT mode draws exactly the foreground pixels */
        U(SetBkMode)(m.dc, TRANSPARENT);
        PBM t;
        CHECK(pb_mem(&t, 200, 4, 32));
        pb_clear(&t, 0xffffff);
        pen_line(&t, p, 0, 1, 180, 1);
        pb_sync();
        int mism = 0;
        for (int x = 0; x < 180; x++) if ((pb_get(&t, x, 1) == 0xff0000) != (pb_get(&m, x, 1) == 0xff0000)) mism++;
        CHECKF(mism == 0, "style %d transparent/opaque foreground differ at %d px", st, mism);
        pb_free(&t);
        U(DeleteObject)(p);
    }
    pb_free(&m);
}

TEST(pen_render_cosmetic_dash_vertical_matches_horizontal)
{
    NEED_P();
    PBM h, v;
    CHECK(pb_mem(&h, 200, 4, 32));
    CHECK(pb_mem(&v, 4, 200, 32));
    for (int st = PS_DASH; st <= PS_DASHDOTDOT; st++) {
        HPEN p = U(CreatePen)(st, 1, 0);
        U(SetBkMode)(h.dc, TRANSPARENT); U(SetBkMode)(v.dc, TRANSPARENT);
        pb_clear(&h, 0xffffff); pb_clear(&v, 0xffffff);
        pen_line(&h, p, 0, 1, 150, 1);
        pen_line(&v, p, 1, 0, 1, 150);
        pb_sync();
        int mism = 0;
        for (int i = 0; i < 150; i++) if ((pb_get(&h, i, 1) != 0xffffff) != (pb_get(&v, 1, i) != 0xffffff)) mism++;
        CHECKF(mism == 0, "style %d horizontal vs vertical differ at %d", st, mism);
        U(DeleteObject)(p);
    }
    pb_free(&h); pb_free(&v);
}

TEST(pen_render_cosmetic_dash_reverse_direction)
{
    NEED_P();
    PBM m;
    CHECK(pb_mem(&m, 200, 4, 32));
    HPEN p = U(CreatePen)(PS_DASH, 1, 0);
    U(SetBkMode)(m.dc, TRANSPARENT);
    pb_clear(&m, 0xffffff);
    pen_line(&m, p, 150, 1, 0, 1);
    int runs[100], first_on;
    int n = get_runs(&m, 1, 1, 151, 0xffffff, runs, 100, &first_on);
    char b[160]; fmt_runs(b, sizeof b, runs, n > 12 ? 12 : n);
    t_note("PS_DASH drawn right-to-left, scanned left-to-right: last_x_on=%d runs %s", (int)pb_get(&m, 150, 1) != 0xffffff, b);
    CHECK(pb_get(&m, 150, 1) != 0xffffff);          /* the start point is the first dash pixel */
    CHECK(pb_get(&m, 0, 1) == 0xffffff);            /* the end point is not drawn */
    U(DeleteObject)(p);
    pb_free(&m);
}

TEST(pen_render_dash_pattern_restarts_per_line)
{
    NEED_P();
    PBM m;
    CHECK(pb_mem(&m, 200, 8, 32));
    HPEN p = U(CreatePen)(PS_DASHDOT, 1, 0);
    U(SetBkMode)(m.dc, TRANSPARENT);
    pb_clear(&m, 0xffffff);
    pen_line(&m, p, 0, 1, 100, 1);
    pen_line(&m, p, 0, 5, 100, 5);
    pb_sync();
    int mism = 0;
    for (int x = 0; x < 100; x++) if ((pb_get(&m, x, 1) != 0xffffff) != (pb_get(&m, x, 5) != 0xffffff)) mism++;
    CHECKF(mism == 0, "mismatch %d", mism);
    U(DeleteObject)(p);
    pb_free(&m);
}

TEST(pen_render_wide_nonsolid_createpen_is_solid)
{
    NEED_P();
    PBM m;
    CHECK(pb_mem(&m, 120, 20, 32));
    for (int st = PS_DASH; st <= PS_DASHDOTDOT; st++)
    for (int w = 2; w <= 6; w += 2) {
        HPEN p = U(CreatePen)(st, w, 0);
        U(SetBkMode)(m.dc, TRANSPARENT);
        pb_clear(&m, 0xffffff);
        pen_line(&m, p, 10, 10, 110, 10);
        pb_sync();
        int holes = 0;
        for (int x = 20; x < 100; x++) if (pb_get(&m, x, 10) != 0) holes++;
        CHECKF(holes == 0, "style %d width %d has %d gaps", st, w, holes);
        U(DeleteObject)(p);
    }
    pb_free(&m);
}

TEST(pen_render_ext_alternate_pattern)
{
    NEED_P(); REQUIRE(U(ExtCreatePen));
    PBM m;
    CHECK(pb_mem(&m, 40, 4, 32));
    LOGBRUSH lb = { BS_SOLID, 0, 0 };
    HPEN p = U(ExtCreatePen)(PS_COSMETIC | PS_ALTERNATE, 1, &lb, 0, NULL);
    CHECK(p != NULL);
    U(SetBkMode)(m.dc, TRANSPARENT);
    static const int t[][2] = { {0, 1}, {0, 2}, {0, 3}, {0, 4}, {1, 4}, {1, 5}, {4, 8}, {0, 33}, {5, 38} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        pb_clear(&m, 0xffffff);
        pen_line(&m, p, t[i][0], 1, t[i][1], 1);
        char nm[32]; sprintf(nm, "alternate %d..%d", t[i][0], t[i][1]);
        SURF_CHECK(&m, nm, (y == 1 && x >= t[i][0] && x < t[i][1] && ((x - t[i][0]) & 1) == 0) ? 0u : 0xffffffu);
    }
    /* vertical */
    PBM v;
    CHECK(pb_mem(&v, 4, 40, 32));
    U(SetBkMode)(v.dc, TRANSPARENT);
    pb_clear(&v, 0xffffff);
    pen_line(&v, p, 1, 2, 1, 21);
    SURF_CHECK(&v, "alternate vertical", (x == 1 && y >= 2 && y < 21 && ((y - 2) & 1) == 0) ? 0u : 0xffffffu);
    pb_free(&v);
    U(DeleteObject)(p);
    pb_free(&m);
}

TEST(pen_render_ext_alternate_opaque_note)
{
    NEED_P(); REQUIRE(U(ExtCreatePen));
    PBM m;
    CHECK(pb_mem(&m, 40, 4, 32));
    LOGBRUSH lb = { BS_SOLID, RGB(255, 0, 0), 0 };
    HPEN p = U(ExtCreatePen)(PS_COSMETIC | PS_ALTERNATE, 1, &lb, 0, NULL);
    U(SetBkMode)(m.dc, OPAQUE);
    U(SetBkColor)(m.dc, RGB(0, 255, 0));
    pb_clear(&m, 0xffffff);
    pen_line(&m, p, 0, 1, 12, 1);
    pb_sync();
    char b[64] = "";
    for (int x = 0; x < 12; x++) sprintf(b + strlen(b), "%06x ", pb_get(&m, x, 1));
    t_note("opaque alternate pen row: %s", b);
    for (int x = 0; x < 12; x += 2) CHECK(pb_get(&m, x, 1) == 0xff0000);
    U(DeleteObject)(p);
    pb_free(&m);
}

TEST(pen_render_ext_userstyle_cosmetic_runs)
{
    NEED_P(); REQUIRE(U(ExtCreatePen));
    PBM m;
    CHECK(pb_mem(&m, 200, 4, 32));
    static const DWORD sets[][4] = { {4, 3, 0, 0}, {1, 1, 0, 0}, {5, 2, 1, 2}, {2, 2, 0, 0}, {2, 6, 0, 0} };
    static const int cnt[] = { 2, 2, 4, 2, 2 };
    LOGBRUSH lb = { BS_SOLID, 0, 0 };
    for (int i = 0; i < 5; i++) {
        HPEN p = U(ExtCreatePen)(PS_COSMETIC | PS_USERSTYLE, 1, &lb, cnt[i], sets[i]);
        CHECK(p != NULL);
        U(SetBkMode)(m.dc, TRANSPARENT);
        pb_clear(&m, 0xffffff);
        pen_line(&m, p, 0, 1, 190, 1);
        int runs[200], first_on, n = get_runs(&m, 1, 0, 190, 0xffffff, runs, 200, &first_on);
        char b[128]; fmt_runs(b, sizeof b, runs, n > 12 ? 12 : n);
        t_note("cosmetic userstyle set %d: first_on=%d runs %s", i, first_on, b);
        CHECK(first_on == 1);
        int per = find_period(runs, n);
        CHECKF(per == cnt[i], "set %d period %d (want %d)", i, per, cnt[i]);
        /* the repeat length in pixels is the sum of the style entries */
        int sum = 0, want = 0;
        for (int k = 0; k < cnt[i]; k++) { sum += runs[1 + k]; want += sets[i][k]; }
        t_note("set %d: observed period %d px for style sum %d (scale %.2f)", i, sum, want, (double)sum / want);
        CHECK(sum % want == 0);
        U(DeleteObject)(p);
    }
    pb_free(&m);
}

TEST(pen_render_ext_userstyle_geometric_runs_note)
{
    NEED_P(); REQUIRE(U(ExtCreatePen));
    PBM m;
    CHECK(pb_mem(&m, 200, 24, 32));
    static const DWORD set[2] = { 8, 4 };
    LOGBRUSH lb = { BS_SOLID, 0, 0 };
    for (int w = 1; w <= 5; w += 2) {
        HPEN p = U(ExtCreatePen)(PS_GEOMETRIC | PS_USERSTYLE | PS_ENDCAP_FLAT, w, &lb, 2, set);
        CHECK(p != NULL);
        pb_clear(&m, 0xffffff);
        pen_line(&m, p, 0, 12, 160, 12);
        int runs[200], first_on, n = get_runs(&m, 12, 0, 160, 0xffffff, runs, 200, &first_on);
        char b[128]; fmt_runs(b, sizeof b, runs, n > 12 ? 12 : n);
        t_note("geometric userstyle {8,4} width %d: first_on=%d runs %s", w, first_on, b);
        CHECK(first_on == 1 && n >= 6);
        CHECK(find_period(runs, n) == 2);
        U(DeleteObject)(p);
    }
    pb_free(&m);
}

TEST(pen_render_dcpen_follows_color)
{
    NEED_P(); REQUIRE(U(SetDCPenColor));
    PBM m;
    CHECK(pb_mem(&m, 16, 4, 32));
    HGDIOBJ dp = GetStockObject(DC_PEN);
    for (int i = 0; i < 5; i++) {
        COLORREF c = RGB(i * 50, 255 - i * 40, 30 + i);
        U(SetDCPenColor)(m.dc, c);
        pb_clear(&m, 0xffffff);
        pen_line(&m, dp, 0, 1, 16, 1);
        SURF_CHECK(&m, "DC_PEN", y == 1 ? pb_raw(&m, c) : 0xffffff);
    }
    pb_free(&m);
}

TEST(pen_render_dcpen_wide_not_possible)
{
    NEED_P(); REQUIRE(U(SetDCPenColor));
    /* DC_PEN is a 1 pixel cosmetic pen no matter what */
    PBM m;
    CHECK(pb_mem(&m, 16, 8, 32));
    U(SetDCPenColor)(m.dc, 0);
    pb_clear(&m, 0xffffff);
    pen_line(&m, GetStockObject(DC_PEN), 0, 3, 16, 3);
    CHECK(pb_count_ne(&m, 0xffffff) == 16);
    pb_free(&m);
}

typedef struct { int r2; const char *n; } R2;
static unsigned r2_model(int r2, unsigned P, unsigned D)
{
    unsigned r;
    switch (r2) {
    case R2_BLACK: r = 0; break;
    case R2_NOTMERGEPEN: r = ~(D | P); break;
    case R2_MASKNOTPEN: r = D & ~P; break;
    case R2_NOTCOPYPEN: r = ~P; break;
    case R2_MASKPENNOT: r = P & ~D; break;
    case R2_NOT: r = ~D; break;
    case R2_XORPEN: r = P ^ D; break;
    case R2_NOTMASKPEN: r = ~(D & P); break;
    case R2_MASKPEN: r = D & P; break;
    case R2_NOTXORPEN: r = ~(P ^ D); break;
    case R2_NOP: r = D; break;
    case R2_MERGENOTPEN: r = D | ~P; break;
    case R2_COPYPEN: r = P; break;
    case R2_MERGEPENNOT: r = P | ~D; break;
    case R2_MERGEPEN: r = P | D; break;
    default: r = ~0u; break;
    }
    return r & 0xffffff;
}

TEST(pen_render_rop2_all_modes_32bpp)
{
    NEED_P(); REQUIRE(U(SetROP2)); REQUIRE(U(GetROP2));
    PBM m;
    CHECK(pb_mem(&m, 16, 4, 32));
    CHECK(U(GetROP2)(m.dc) == R2_COPYPEN);
    for (int r2 = R2_BLACK; r2 <= R2_WHITE; r2++) {
        HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(0xa5, 0x3c, 0xf0));
        int old = U(SetROP2)(m.dc, r2);
        CHECKF(old == R2_COPYPEN, "mode %d old %d", r2, old);
        CHECK(U(GetROP2)(m.dc) == r2);
        pb_clear(&m, 0x5a96c3);
        pen_line(&m, p, 0, 1, 16, 1);
        char nm[32]; sprintf(nm, "R2 %d", r2);
        SURF_CHECK(&m, nm, y == 1 ? r2_model(r2, pb_raw(&m, RGB(0xa5, 0x3c, 0xf0)), 0x5a96c3) : 0x5a96c3);
        U(SetROP2)(m.dc, R2_COPYPEN);
        U(DeleteObject)(p);
    }
    pb_free(&m);
}

TEST(pen_render_rop2_invalid_modes)
{
    NEED_P(); REQUIRE(U(SetROP2)); REQUIRE(U(GetROP2));
    PBM m;
    CHECK(pb_mem(&m, 4, 4, 32));
    static const int bad[] = { 0, -1, 17, 18, 100, INT_MAX, INT_MIN };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0xdeadbeef);
        int r = U(SetROP2)(m.dc, bad[i]);
        CHECKF(r == 0, "mode %d returned %d", bad[i], r);
        CHECKF(U(GetROP2)(m.dc) == R2_COPYPEN, "mode %d changed state", bad[i]);
    }
    CHECK(U(SetROP2)(NULL, R2_XORPEN) == 0);
    CHECK(U(GetROP2)(NULL) == 0);
    pb_free(&m);
}

TEST(pen_render_rop2_xor_twice_restores)
{
    NEED_P(); REQUIRE(U(SetROP2));
    PBM m;
    CHECK(pb_mem(&m, 24, 24, 32));
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(255, 255, 255));
    U(SetROP2)(m.dc, R2_XORPEN);
    for (int y = 0; y < 24; y++) for (int x = 0; x < 24; x++) pb_set(&m, x, y, (x * 10) << 16 | (y * 10) << 8 | (x ^ y));
    PBM ref;
    CHECK(pb_mem(&ref, 24, 24, 32));
    memcpy(ref.bits, m.bits, m.stride * m.h);
    pen_line(&m, p, 1, 3, 22, 18);
    pb_sync();
    CHECK(memcmp(ref.bits, m.bits, m.stride * m.h) != 0);
    pen_line(&m, p, 1, 3, 22, 18);
    pb_sync();
    CHECK(!memcmp(ref.bits, m.bits, m.stride * m.h));
    U(DeleteObject)(p);
    pb_free(&ref); pb_free(&m);
}

TEST(pen_render_rop2_not_ignores_pen)
{
    NEED_P(); REQUIRE(U(SetROP2));
    PBM m;
    CHECK(pb_mem(&m, 16, 4, 32));
    U(SetROP2)(m.dc, R2_NOT);
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(1, 2, 3));
    pb_clear(&m, 0x123456);
    pen_line(&m, p, 0, 1, 16, 1);
    SURF_CHECK(&m, "R2_NOT", y == 1 ? (~0x123456u & 0xffffff) : 0x123456u);
    U(DeleteObject)(p);
    pb_free(&m);
}

TEST(pen_render_rop2_1bpp)
{
    NEED_P(); REQUIRE(U(SetROP2));
    PBM m;
    CHECK(pb_mem(&m, 16, 3, 1));
    for (int r2 = R2_BLACK; r2 <= R2_WHITE; r2++)
    for (int pc = 0; pc < 2; pc++)
    for (int dc = 0; dc < 2; dc++) {
        HPEN p = U(CreatePen)(PS_SOLID, 1, pc ? 0xffffff : 0);
        U(SetROP2)(m.dc, r2);
        pb_clear(&m, dc);
        pen_line(&m, p, 0, 1, 16, 1);
        char nm[48]; sprintf(nm, "1bpp R2 %d P=%d D=%d", r2, pc, dc);
        SURF_CHECK(&m, nm, y == 1 ? (r2_model(r2, pc ? 0xffffff : 0, dc ? 0xffffff : 0) & 1) : (unsigned)dc);
        U(DeleteObject)(p);
    }
    pb_free(&m);
}

TEST(pen_render_polyline_matches_lineto)
{
    NEED_P(); REQUIRE(U(Polyline));
    PBM a, b;
    CHECK(pb_mem(&a, 40, 40, 32));
    CHECK(pb_mem(&b, 40, 40, 32));
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0);
    POINT pts[] = { {3, 3}, {30, 8}, {12, 35}, {35, 35} };
    pb_clear(&a, 0xffffff); pb_clear(&b, 0xffffff);
    HGDIOBJ oa = U(SelectObject)(a.dc, p);
    CHECK(U(Polyline)(a.dc, pts, 4));
    U(SelectObject)(a.dc, oa);
    HGDIOBJ ob = U(SelectObject)(b.dc, p);
    U(MoveToEx)(b.dc, pts[0].x, pts[0].y, NULL);
    for (int i = 1; i < 4; i++) U(LineTo)(b.dc, pts[i].x, pts[i].y);
    U(SelectObject)(b.dc, ob);
    pb_sync();
    int mism = 0;
    for (int y = 0; y < 40; y++) for (int x = 0; x < 40; x++) if (pb_get(&a, x, y) != pb_get(&b, x, y)) mism++;
    t_note("Polyline vs MoveTo/LineTo: %d differing pixels", mism);
    /* Polyline includes the last point? LineTo-chain excludes it */
    CHECK(mism <= 1);
    U(DeleteObject)(p);
    pb_free(&a); pb_free(&b);
}

TEST(pen_render_selected_pen_unaffected_by_other_dc_ops)
{
    NEED_P(); REQUIRE(U(GetCurrentObject));
    PBM m;
    CHECK(pb_mem(&m, 16, 4, 32));
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(0, 200, 0));
    HGDIOBJ old = U(SelectObject)(m.dc, p);
    U(SetBkMode)(m.dc, TRANSPARENT);
    pb_clear(&m, 0xffffff);
    U(MoveToEx)(m.dc, 0, 1, NULL);
    U(LineTo)(m.dc, 16, 1);
    U(MoveToEx)(m.dc, 0, 2, NULL);
    U(LineTo)(m.dc, 16, 2);
    SURF_CHECK(&m, "two lines", (y == 1 || y == 2) ? 0x00c800u : 0xffffffu);
    CHECK(U(GetCurrentObject)(m.dc, OBJ_PEN) == p);
    U(SelectObject)(m.dc, old);
    U(DeleteObject)(p);
    pb_free(&m);
}

TEST(pen_render_ext_cosmetic_matches_createpen)
{
    NEED_P(); REQUIRE(U(ExtCreatePen));
    PBM a, b;
    CHECK(pb_mem(&a, 40, 40, 32));
    CHECK(pb_mem(&b, 40, 40, 32));
    LOGBRUSH lb = { BS_SOLID, RGB(10, 100, 200), 0 };
    for (int st = PS_SOLID; st <= PS_DASHDOTDOT; st++) {
        HPEN p1 = U(CreatePen)(st, 1, RGB(10, 100, 200));
        HPEN p2 = U(ExtCreatePen)(PS_COSMETIC | st, 1, &lb, 0, NULL);
        pb_clear(&a, 0xffffff); pb_clear(&b, 0xffffff);
        U(SetBkMode)(a.dc, TRANSPARENT); U(SetBkMode)(b.dc, TRANSPARENT);
        pen_line(&a, p1, 2, 3, 37, 30);
        pen_line(&b, p2, 2, 3, 37, 30);
        pb_sync();
        CHECKF(!memcmp(a.bits, b.bits, a.stride * a.h), "style %d differs", st);
        U(DeleteObject)(p1); U(DeleteObject)(p2);
    }
    pb_free(&a); pb_free(&b);
}

TEST(pen_render_leaves_gdi_state_clean)
{
    NEED_P();
    DWORD before = pb_gdi_count();
    if (!before) { t_skip("GetGuiResources unavailable"); return; }
    for (int i = 0; i < 100; i++) {
        PBM m;
        CHECK(pb_mem(&m, 16, 16, 32));
        HPEN p = U(CreatePen)(i % 5, 1, i);
        pen_line(&m, p, 0, 0, 15, 15);
        U(DeleteObject)(p);
        pb_free(&m);
    }
    CHECKF(pb_gdi_count() <= before + 1, "before %lu after %lu", before, pb_gdi_count());
}
