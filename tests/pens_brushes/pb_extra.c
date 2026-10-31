#include "pb.h"

#define NEED_X() do { NEED_MEM(); REQUIRE(U(CreatePen)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(CreateHatchBrush)); \
    REQUIRE(U(CreateBrushIndirect)); REQUIRE(U(ExtCreatePen)); REQUIRE(U(GetObjectA)); REQUIRE(U(PatBlt)); \
    REQUIRE(U(MoveToEx)); REQUIRE(U(LineTo)); REQUIRE(U(Rectangle)); REQUIRE(U(SetBkMode)); REQUIRE(U(SetBkColor)); } while (0)

/* GetObjectA and GetObjectW are the same for pens and brushes */
TEST(pbx_getobject_a_w_parity)
{
    NEED_X();
    HPEN p = U(CreatePen)(PS_DASH, 1, RGB(1, 2, 3));
    HBRUSH b = U(CreateHatchBrush)(HS_VERTICAL, RGB(4, 5, 6));
    HPEN e = U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID, 5, &(LOGBRUSH){ BS_SOLID, RGB(7, 8, 9), 0 }, 0, NULL);
    CHECK(p && b && e);
    LOGPEN la, lw; LOGBRUSH ba, bw; EXTLOGPEN xa, xw;
    memset(&la, 1, sizeof la); memset(&lw, 2, sizeof lw);
    CHECK(U(GetObjectA)(p, sizeof la, &la) == sizeof la);
    CHECK(U(GetObjectW)(p, sizeof lw, &lw) == sizeof lw);
    CHECK(memcmp(&la, &lw, sizeof la) == 0);
    CHECK(U(GetObjectA)(b, sizeof ba, &ba) == sizeof ba);
    CHECK(U(GetObjectW)(b, sizeof bw, &bw) == sizeof bw);
    CHECK(memcmp(&ba, &bw, sizeof ba) == 0);
    memset(&xa, 1, sizeof xa); memset(&xw, 1, sizeof xw);
    int na = U(GetObjectA)(e, sizeof xa, &xa), nw = U(GetObjectW)(e, sizeof xw, &xw);
    CHECK(na == nw && na >= (int)sizeof(EXTLOGPEN) - 4);
    CHECK(xa.elpPenStyle == xw.elpPenStyle && xa.elpWidth == xw.elpWidth && xa.elpColor == xw.elpColor);
    /* NULL buffer: size query */
    CHECK(U(GetObjectA)(p, 0, NULL) == sizeof(LOGPEN));
    CHECK(U(GetObjectA)(b, 0, NULL) == sizeof(LOGBRUSH));
    CHECK(U(GetObjectA)(e, 0, NULL) == na);
    U(DeleteObject)(p); U(DeleteObject)(b); U(DeleteObject)(e);
}

/* CreatePenIndirect: lopnWidth.y is ignored */
TEST(pbx_penindirect_width_y_ignored)
{
    NEED_X(); REQUIRE(U(CreatePenIndirect));
    static const LONG ys[] = { 0, 1, 7, -3, 0x7fffffff };
    for (unsigned i = 0; i < sizeof ys / sizeof *ys; i++) {
        LOGPEN in = { PS_SOLID, { 3, ys[i] }, RGB(10, 20, 30) }, out;
        HPEN p = U(CreatePenIndirect)(&in);
        CHECKF(p != NULL, "y=%ld", ys[i]);
        if (!p) continue;
        memset(&out, 0xcc, sizeof out);
        CHECK(U(GetObjectW)(p, sizeof out, &out) == sizeof out);
        CHECKF(out.lopnStyle == PS_SOLID && out.lopnWidth.x == 3 && out.lopnColor == in.lopnColor, "y=%ld x=%ld", ys[i], out.lopnWidth.x);
        CHECKF(out.lopnWidth.y == 0, "y=%ld got %ld", ys[i], out.lopnWidth.y);
        U(DeleteObject)(p);
    }
}

/* cosmetic PS_SOLID ext pen reports width 1 and style flags in elpPenStyle */
TEST(pbx_extpen_cosmetic_report)
{
    NEED_X();
    LOGBRUSH lb = { BS_SOLID, RGB(9, 8, 7), 0 };
    HPEN p = U(ExtCreatePen)(PS_COSMETIC | PS_SOLID, 1, &lb, 0, NULL);
    CHECK(p != NULL);
    EXTLOGPEN x; memset(&x, 0, sizeof x);
    CHECK(U(GetObjectW)(p, sizeof x, &x) >= (int)sizeof x - 4);
    CHECK((x.elpPenStyle & 0xf) == PS_SOLID);
    CHECK((x.elpPenStyle & PS_TYPE_MASK) == PS_COSMETIC);
    CHECK(x.elpWidth == 1);
    CHECK(x.elpColor == lb.lbColor);
    CHECK(x.elpBrushStyle == BS_SOLID);
    CHECK(x.elpNumEntries == 0);
    CHECK(U(GetObjectType)(p) == OBJ_EXTPEN);
    U(DeleteObject)(p);
    /* an old-style CreatePen is OBJ_PEN, never OBJ_EXTPEN */
    p = U(CreatePen)(PS_SOLID, 1, 0);
    CHECK(U(GetObjectType)(p) == OBJ_PEN);
    U(DeleteObject)(p);
}

/* Pen colour is independent from text color and bk color */
TEST(pbx_pen_ignores_text_color)
{
    NEED_X(); REQUIRE(U(SetTextColor));
    PBM m;
    CHECK(pb_mem(&m, 16, 4, 32));
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(0x12, 0x34, 0x56));
    HGDIOBJ old = U(SelectObject)(m.dc, p);
    U(SetTextColor)(m.dc, RGB(255, 0, 0));
    U(SetBkColor)(m.dc, RGB(0, 255, 0));
    pb_clear(&m, 0);
    U(MoveToEx)(m.dc, 0, 1, NULL);
    U(LineTo)(m.dc, 10, 1);
    pb_sync();
    for (int x = 0; x < 10; x++) CHECKF(pb_get(&m, x, 1) == 0x123456, "x=%d got %x", x, pb_get(&m, x, 1));
    CHECK(pb_get(&m, 10, 1) == 0);          /* LineTo excludes its endpoint */
    CHECK(pb_count_ne(&m, 0) == 10);
    U(SelectObject)(m.dc, old);
    U(DeleteObject)(p);
    pb_free(&m);
}

/* solid brush PatBlt fills exactly w*h pixels, for odd sizes at odd offsets */
TEST(pbx_patblt_solid_area_counts)
{
    NEED_X();
    PBM m;
    CHECK(pb_mem(&m, 40, 30, 32));
    HBRUSH b = U(CreateSolidBrush)(RGB(0xaa, 0x55, 0x11));
    static const int r[][4] = { {0,0,1,1}, {39,29,1,1}, {3,5,7,11}, {0,0,40,30}, {10,10,29,19}, {1,1,38,1}, {5,0,1,30} };
    for (unsigned i = 0; i < sizeof r / sizeof *r; i++) {
        pb_clear(&m, 0);
        CHECK(pb_fill(&m, b, r[i][0], r[i][1], r[i][2], r[i][3], PATCOPY));
        CHECKF(pb_count_eq(&m, 0xaa5511) == r[i][2] * r[i][3], "rect %u", i);
        SURF_CHECK(&m, "solid rect", (x >= r[i][0] && x < r[i][0] + r[i][2] && y >= r[i][1] && y < r[i][1] + r[i][3]) ? 0xaa5511 : 0);
    }
    U(DeleteObject)(b);
    pb_free(&m);
}

/* BLACKNESS / WHITENESS ignore the brush colour */
TEST(pbx_patblt_blackness_whiteness)
{
    NEED_X();
    PBM m;
    CHECK(pb_mem(&m, 8, 8, 32));
    HBRUSH b = U(CreateSolidBrush)(RGB(10, 200, 30));
    pb_fill(&m, b, 0, 0, 8, 8, WHITENESS);
    CHECK(pb_count_eq(&m, 0xffffff) == 64);
    pb_fill(&m, b, 0, 0, 8, 8, BLACKNESS);
    CHECK(pb_count_eq(&m, 0) == 64);
    pb_fill(&m, b, 0, 0, 8, 8, DSTINVERT);
    CHECK(pb_count_eq(&m, 0xffffff) == 64);
    U(DeleteObject)(b);
    pb_free(&m);
}

/* PATINVERT twice restores and uses the brush colour */
TEST(pbx_patblt_patinvert_involution)
{
    NEED_X();
    PBM m;
    CHECK(pb_mem(&m, 8, 8, 32));
    HBRUSH b = U(CreateSolidBrush)(RGB(0xff, 0x0f, 0xf0));
    pb_clear(&m, 0x00ff00);
    pb_fill(&m, b, 0, 0, 8, 8, PATINVERT);
    CHECK(pb_count_eq(&m, 0x00ff00 ^ 0xff0ff0) == 64);
    pb_fill(&m, b, 0, 0, 8, 8, PATINVERT);
    CHECK(pb_count_eq(&m, 0x00ff00) == 64);
    U(DeleteObject)(b);
    pb_free(&m);
}

/* Rectangle with a hatched brush: interior pixels follow the hatch model, opaque bk fill */
TEST(pbx_rectangle_hatch_fill_null_pen)
{
    NEED_X();
    PBM m;
    CHECK(pb_mem(&m, 24, 24, 32));
    HBRUSH b = U(CreateHatchBrush)(HS_CROSS, RGB(255, 0, 0));
    HGDIOBJ ob = U(SelectObject)(m.dc, b);
    HGDIOBJ op = U(SelectObject)(m.dc, GetStockObject(NULL_PEN));
    U(SetBkColor)(m.dc, RGB(0, 0, 255));
    U(SetBkMode)(m.dc, OPAQUE);
    pb_clear(&m, 0x808080);
    CHECK(U(Rectangle)(m.dc, 0, 0, 17, 17));
    pb_sync();
    int bad = 0;
    for (int y = 1; y < 15; y++) for (int x = 1; x < 15; x++) {
        unsigned want = pb_hatch_bit(HS_CROSS, x, y) ? 0xff0000 : 0x0000ff;
        if (pb_get(&m, x, y) != want) bad++;
    }
    CHECKF(bad == 0, "%d interior mismatches", bad);
    CHECK(pb_get(&m, 20, 20) == 0x808080);
    U(SelectObject)(m.dc, op); U(SelectObject)(m.dc, ob);
    U(DeleteObject)(b);
    pb_free(&m);
}

/* a pen deleted while selected into a DC does not break the DC's other drawing after reselect */
TEST(pbx_delete_selected_then_reselect_stock)
{
    NEED_X();
    PBM m;
    CHECK(pb_mem(&m, 8, 4, 32));
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(1, 2, 3));
    HGDIOBJ old = U(SelectObject)(m.dc, p);
    CHECK(old != NULL);
    U(SelectObject)(m.dc, old);       /* deselect first */
    CHECK(U(DeleteObject)(p));
    CHECK(U(GetObjectW)(p, sizeof(LOGPEN), &(LOGPEN){0}) == 0);
    CHECK(U(SelectObject)(m.dc, p) == NULL);   /* deleted handle can't be selected */
    CHECK(U(GetCurrentObject)(m.dc, OBJ_PEN) == old);
    pb_clear(&m, 0xffffff);
    U(MoveToEx)(m.dc, 0, 0, NULL); U(LineTo)(m.dc, 5, 0);
    CHECK(pb_count_eq(&m, 0xffffff) == 32 - 5);
    pb_free(&m);
}

/* brush created with every style from LOGBRUSH reports a consistent object type and never NULL for valid styles */
TEST(pbx_logbrush_valid_styles_all_succeed)
{
    NEED_X();
    static const struct { UINT style; COLORREF c; } t[] = { { BS_SOLID, 0x112233 }, { BS_NULL, 0 }, { BS_HATCHED, 0x445566 } };
    for (unsigned i = 0; i < 3; i++) {
        LOGBRUSH in = { t[i].style, t[i].c, HS_BDIAGONAL }, out;
        HBRUSH b = U(CreateBrushIndirect)(&in);
        CHECK(b != NULL);
        if (!b) continue;
        CHECK(U(GetObjectType)(b) == OBJ_BRUSH);
        CHECK(U(GetObjectW)(b, sizeof out, &out) == sizeof out);
        CHECK(out.lbStyle == t[i].style);
        if (t[i].style == BS_HATCHED) CHECK(out.lbHatch == HS_BDIAGONAL && out.lbColor == t[i].c);
        if (t[i].style == BS_SOLID) CHECK(out.lbColor == t[i].c);
        U(DeleteObject)(b);
    }
}

/* many pens of varying widths selected one after another: SelectObject returns the previous every time */
TEST(pbx_select_chain_returns_previous)
{
    NEED_X();
    HDC dc = U(CreateCompatibleDC)(NULL);
    HPEN pens[8];
    HGDIOBJ prev = U(GetCurrentObject)(dc, OBJ_PEN);
    for (int i = 0; i < 8; i++) {
        pens[i] = U(CreatePen)(PS_SOLID, i + 1, RGB(i, i, i));
        HGDIOBJ r = U(SelectObject)(dc, pens[i]);
        CHECKF(r == prev, "i=%d", i);
        prev = pens[i];
    }
    for (int i = 7; i >= 0; i--) {
        HGDIOBJ r = U(SelectObject)(dc, i ? (HGDIOBJ)pens[i - 1] : GetStockObject(BLACK_PEN));
        CHECKF(r == pens[i], "back %d", i);
    }
    for (int i = 0; i < 8; i++) CHECK(U(DeleteObject)(pens[i]));
    U(DeleteDC)(dc);
}

/* cosmetic line with PS_NULL ext pen draws nothing and same for width>1 NULL pen */
TEST(pbx_null_pens_draw_nothing)
{
    NEED_X();
    PBM m;
    CHECK(pb_mem(&m, 16, 16, 32));
    HPEN np[3] = { GetStockObject(NULL_PEN), U(CreatePen)(PS_NULL, 5, RGB(255, 0, 0)),
        U(ExtCreatePen)(PS_GEOMETRIC | PS_NULL, 5, &(LOGBRUSH){ BS_SOLID, RGB(255, 0, 0), 0 }, 0, NULL) };
    for (int i = 0; i < 3; i++) {
        CHECKF(np[i] != NULL, "pen %d", i);
        HGDIOBJ old = U(SelectObject)(m.dc, np[i]);
        pb_clear(&m, 0x123456);
        U(MoveToEx)(m.dc, 0, 8, NULL); U(LineTo)(m.dc, 15, 8);
        U(MoveToEx)(m.dc, 8, 0, NULL); U(LineTo)(m.dc, 8, 15);
        CHECKF(pb_count_ne(&m, 0x123456) == 0, "pen %d drew", i);
        U(SelectObject)(m.dc, old);
    }
    if (np[1]) U(DeleteObject)(np[1]);
    if (np[2]) U(DeleteObject)(np[2]);
    pb_free(&m);
}

/* GetObject with buffer larger than the structure still returns the structure size and leaves the tail alone */
TEST(pbx_getobject_oversized_buffer_tail)
{
    NEED_X();
    HPEN p = U(CreatePen)(PS_SOLID, 2, 5);
    HBRUSH b = U(CreateSolidBrush)(5);
    BYTE buf[128];
    memset(buf, 0xee, sizeof buf);
    CHECK(U(GetObjectW)(p, sizeof buf, buf) == sizeof(LOGPEN));
    for (size_t i = sizeof(LOGPEN); i < sizeof buf; i++) if (buf[i] != 0xee) { CHECKF(0, "pen tail byte %zu changed", i); break; }
    memset(buf, 0xee, sizeof buf);
    CHECK(U(GetObjectW)(b, sizeof buf, buf) == sizeof(LOGBRUSH));
    for (size_t i = sizeof(LOGBRUSH); i < sizeof buf; i++) if (buf[i] != 0xee) { CHECKF(0, "brush tail byte %zu changed", i); break; }
    U(DeleteObject)(p); U(DeleteObject)(b);
}
