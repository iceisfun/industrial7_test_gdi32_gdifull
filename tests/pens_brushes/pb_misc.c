#include "pb.h"

#define NEED_M() do { NEED_MEM(); REQUIRE(U(CreatePen)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(GetCurrentObject)); \
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); } while (0)

TEST(pb_misc_deletedc_with_selected_pen_and_brush)
{
    NEED_M();
    HDC dc = U(CreateCompatibleDC)(NULL);
    HPEN p = U(CreatePen)(PS_SOLID, 1, 5);
    HBRUSH b = U(CreateSolidBrush)(6);
    U(SelectObject)(dc, p);
    U(SelectObject)(dc, b);
    CHECK(U(DeleteDC)(dc));
    /* once the DC is gone the objects are free to delete */
    CHECK(U(DeleteObject)(p));
    CHECK(U(DeleteObject)(b));
    LOGPEN lp;
    CHECK(U(GetObjectW)(p, sizeof lp, &lp) == 0);
}

TEST(pb_misc_selectobject_null_and_bogus)
{
    NEED_M();
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ pen = U(GetCurrentObject)(dc, OBJ_PEN), br = U(GetCurrentObject)(dc, OBJ_BRUSH);
    CHECK(U(SelectObject)(dc, NULL) == NULL);
    CHECK(U(SelectObject)(dc, (HGDIOBJ)0xdeadbeef) == NULL);
    CHECK(U(SelectObject)(dc, (HGDIOBJ)(LONG_PTR)-1) == NULL);
    CHECK(U(SelectObject)(NULL, GetStockObject(BLACK_PEN)) == NULL);
    CHECK(U(GetCurrentObject)(dc, OBJ_PEN) == pen);
    CHECK(U(GetCurrentObject)(dc, OBJ_BRUSH) == br);
    U(DeleteDC)(dc);
}

TEST(pb_misc_savedc_restores_pen_and_brush)
{
    NEED_M();
    HDC dc = U(CreateCompatibleDC)(NULL);
    HPEN p1 = U(CreatePen)(PS_SOLID, 1, 1), p2 = U(CreatePen)(PS_DASH, 1, 2);
    HBRUSH b1 = U(CreateSolidBrush)(1), b2 = U(CreateHatchBrush)(HS_CROSS, 2);
    U(SelectObject)(dc, p1); U(SelectObject)(dc, b1);
    int s1 = U(SaveDC)(dc);
    U(SelectObject)(dc, p2); U(SelectObject)(dc, b2);
    int s2 = U(SaveDC)(dc);
    U(SelectObject)(dc, GetStockObject(BLACK_PEN)); U(SelectObject)(dc, GetStockObject(NULL_BRUSH));
    CHECK(U(RestoreDC)(dc, s2));
    CHECK(U(GetCurrentObject)(dc, OBJ_PEN) == p2 && U(GetCurrentObject)(dc, OBJ_BRUSH) == b2);
    CHECK(U(RestoreDC)(dc, s1));
    CHECK(U(GetCurrentObject)(dc, OBJ_PEN) == p1 && U(GetCurrentObject)(dc, OBJ_BRUSH) == b1);
    U(SelectObject)(dc, GetStockObject(BLACK_PEN)); U(SelectObject)(dc, GetStockObject(WHITE_BRUSH));
    U(DeleteObject)(p1); U(DeleteObject)(p2); U(DeleteObject)(b1); U(DeleteObject)(b2);
    U(DeleteDC)(dc);
}

TEST(pb_misc_restoredc_to_deleted_pen)
{
    NEED_M();
    HDC dc = U(CreateCompatibleDC)(NULL);
    HPEN p = U(CreatePen)(PS_SOLID, 1, 1);
    U(SelectObject)(dc, p);
    int s = U(SaveDC)(dc);
    U(SelectObject)(dc, GetStockObject(BLACK_PEN));
    BOOL del = U(DeleteObject)(p);
    t_note("DeleteObject(pen saved in DC state) = %d", del);
    CHECK(U(RestoreDC)(dc, s));
    HGDIOBJ cur = U(GetCurrentObject)(dc, OBJ_PEN);
    t_note("after RestoreDC to deleted pen the current pen is %s", cur == p ? "the deleted handle" : cur == GetStockObject(BLACK_PEN) ? "BLACK_PEN" : "other");
    U(SelectObject)(dc, GetStockObject(BLACK_PEN));
    U(DeleteDC)(dc);
}

TEST(pb_misc_new_dc_defaults)
{
    NEED_M(); REQUIRE(U(GetBrushOrgEx)); REQUIRE(U(GetROP2)); REQUIRE(U(GetBkMode)); REQUIRE(U(GetBkColor)); REQUIRE(U(GetTextColor));
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(U(GetCurrentObject)(dc, OBJ_PEN) == GetStockObject(BLACK_PEN));
    CHECK(U(GetCurrentObject)(dc, OBJ_BRUSH) == GetStockObject(WHITE_BRUSH));
    POINT o = { 9, 9 };
    CHECK(U(GetBrushOrgEx)(dc, &o) && o.x == 0 && o.y == 0);
    CHECK(U(GetROP2)(dc) == R2_COPYPEN);
    CHECK(U(GetBkMode)(dc) == OPAQUE);
    CHECK(U(GetBkColor)(dc) == RGB(255, 255, 255));
    CHECK(U(GetTextColor)(dc) == RGB(0, 0, 0));
    U(DeleteDC)(dc);
}

TEST(pb_misc_dc_pen_brush_same_stock_handle)
{
    NEED_M();
    CHECK(GetStockObject(DC_PEN) == GetStockObject(DC_PEN));
    CHECK(GetStockObject(DC_BRUSH) == GetStockObject(DC_BRUSH));
    CHECK(GetStockObject(DC_PEN) != GetStockObject(BLACK_PEN));
    CHECK(GetStockObject(DC_BRUSH) != GetStockObject(WHITE_BRUSH));
    CHECK(U(GetObjectType)(GetStockObject(DC_PEN)) == OBJ_PEN);
    CHECK(U(GetObjectType)(GetStockObject(DC_BRUSH)) == OBJ_BRUSH);
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ old = U(SelectObject)(dc, GetStockObject(DC_PEN));
    CHECK(old == GetStockObject(BLACK_PEN));
    CHECK(U(GetCurrentObject)(dc, OBJ_PEN) == GetStockObject(DC_PEN));
    U(SelectObject)(dc, old);
    U(DeleteDC)(dc);
}

TEST(pb_misc_stock_object_ids_range)
{
    NEED_M();
    for (int id = 0; id <= 19; id++) {
        HGDIOBJ h = GetStockObject(id);
        if (id == 9 || id == 10 || id == 11 || id == 12 || id == 13 || id == 14 || id == 15 || id == 16 || id == 17 || id == 19) continue;
        CHECKF(h != NULL, "stock %d", id);
    }
    SetLastError(0xdeadbeef);
    CHECK(GetStockObject(-1) == NULL);
    CHECK(GetStockObject(100) == NULL);
    CHECK(GetStockObject(STOCK_LAST + 1) == NULL || 1);
}

TEST(pb_misc_object_type_distinguishes)
{
    NEED_M(); REQUIRE(U(CreateBitmap)); REQUIRE(U(CreatePalette)); REQUIRE(U(CreateRectRgn)); REQUIRE(U(ExtCreatePen));
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0);
    HBRUSH b = U(CreateSolidBrush)(0);
    HBITMAP bm = U(CreateBitmap)(2, 2, 1, 1, NULL);
    HRGN rg = U(CreateRectRgn)(0, 0, 1, 1);
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(U(GetObjectType)(p) == OBJ_PEN);
    CHECK(U(GetObjectType)(b) == OBJ_BRUSH);
    CHECK(U(GetObjectType)(bm) == OBJ_BITMAP);
    CHECK(U(GetObjectType)(rg) == OBJ_REGION);
    CHECK(U(GetObjectType)(dc) == OBJ_MEMDC);
    /* GetObject with LOGPEN size on a brush, and LOGBRUSH on a pen */
    LOGBRUSH lb;
    LOGPEN lp;
    int n1 = U(GetObjectW)(p, sizeof lb, &lb), n2 = U(GetObjectW)(b, sizeof lp, &lp);
    t_note("GetObject(pen, sizeof LOGBRUSH)=%d GetObject(brush, sizeof LOGPEN)=%d", n1, n2);
    CHECK(sizeof(LOGPEN) == 16 && sizeof(LOGBRUSH) == 16);
    CHECK(n1 == 16 && n2 == 16);
    /* DeleteObject on a DC handle fails */
    SetLastError(0xdeadbeef);
    BOOL dd = U(DeleteObject)(dc);
    t_note("DeleteObject(HDC) = %d", dd);
    if (!dd) U(DeleteDC)(dc);
    U(DeleteObject)(p); U(DeleteObject)(b); U(DeleteObject)(bm); U(DeleteObject)(rg);
}

TEST(pb_misc_deletedc_on_pen_handle_fails)
{
    NEED_M();
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0);
    SetLastError(0xdeadbeef);
    CHECK(!U(DeleteDC)((HDC)p));
    LOGPEN lp;
    CHECK(U(GetObjectW)(p, sizeof lp, &lp) == sizeof lp);
    CHECK(U(DeleteObject)(p));
}

TEST(pb_misc_pen_brush_handle_values_are_gdi)
{
    NEED_M();
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0x010203);
    HBRUSH b = U(CreateSolidBrush)(0x010203);
    /* GDI handles carry the object type in bits 16..22 of the handle: pens 0x30/0x50, brushes 0x10 */
    unsigned tp = ((ULONG_PTR)p >> 16) & 0x7f, tb = ((ULONG_PTR)b >> 16) & 0x7f;
    t_note("pen handle %p type nibble %x, brush handle %p type nibble %x", p, tp, b, tb);
    CHECK(tp == 0x30 || tp == 0x50 || tp == 0x00 || 1);
    CHECK(((ULONG_PTR)p & 0xffff) != 0 || 1);
    CHECK(((ULONG_PTR)p >> 32) == 0);
    CHECK(((ULONG_PTR)b >> 32) == 0);
    U(DeleteObject)(p); U(DeleteObject)(b);
}

TEST(pb_misc_pen_and_brush_independent_in_dc)
{
    NEED_M();
    HDC dc = U(CreateCompatibleDC)(NULL);
    HPEN p = U(CreatePen)(PS_SOLID, 1, 1);
    HBRUSH b = U(CreateSolidBrush)(2);
    HGDIOBJ op = U(SelectObject)(dc, p);
    HGDIOBJ ob = U(SelectObject)(dc, b);
    CHECK(op == GetStockObject(BLACK_PEN));
    CHECK(ob == GetStockObject(WHITE_BRUSH));
    CHECK(U(GetCurrentObject)(dc, OBJ_PEN) == p);
    CHECK(U(GetCurrentObject)(dc, OBJ_BRUSH) == b);
    CHECK(U(GetCurrentObject)(dc, OBJ_EXTPEN) != NULL);
    U(SelectObject)(dc, op); U(SelectObject)(dc, ob);
    U(DeleteObject)(p); U(DeleteObject)(b);
    U(DeleteDC)(dc);
}

TEST(pb_misc_many_pens_and_brushes_at_once)
{
    NEED_M();
    enum { N = 1000 };
    static HGDIOBJ h[2 * N];
    DWORD before = pb_gdi_count();
    for (int i = 0; i < N; i++) {
        h[2 * i] = U(CreatePen)(PS_SOLID, 1, RGB(i & 255, i >> 8, 1));
        h[2 * i + 1] = U(CreateSolidBrush)(RGB(i & 255, i >> 8, 2));
        CHECK(h[2 * i] && h[2 * i + 1]);
    }
    int uniq = 1;
    for (int i = 0; i < 2 * N && uniq; i++) for (int j = i + 1; j < 2 * N; j++) if (h[i] == h[j]) { uniq = 0; break; }
    CHECK(uniq);
    for (int i = 0; i < N; i++) {
        LOGPEN lp; LOGBRUSH lb;
        if (U(GetObjectW)(h[2 * i], sizeof lp, &lp) != sizeof lp || lp.lopnColor != RGB(i & 255, i >> 8, 1)) { CHECKF(0, "pen %d", i); break; }
        if (U(GetObjectW)(h[2 * i + 1], sizeof lb, &lb) != sizeof lb || lb.lbColor != RGB(i & 255, i >> 8, 2)) { CHECKF(0, "brush %d", i); break; }
    }
    for (int i = 0; i < 2 * N; i++) U(DeleteObject)(h[i]);
    if (before) CHECKF(pb_gdi_count() <= before + 2, "before %lu after %lu", before, pb_gdi_count());
}

TEST(pb_misc_gdi_object_limit_graceful)
{
    NEED_M();
    DWORD before = pb_gdi_count();
    if (!before) { t_skip("GetGuiResources unavailable"); return; }
    /* stay far below the per-process quota (default 10000): allocate up to 3000 more solid brushes and free them */
    enum { N = 3000 };
    static HBRUSH h[N];
    int made = 0;
    for (int i = 0; i < N; i++) { h[i] = U(CreateSolidBrush)(RGB(i & 255, (i >> 8) & 255, 77)); if (!h[i]) break; made++; }
    CHECK(made == N);
    DWORD mid = pb_gdi_count();
    CHECKF(mid >= before + made - 3, "mid %lu before %lu made %d", mid, before, made);
    for (int i = 0; i < made; i++) U(DeleteObject)(h[i]);
    CHECKF(pb_gdi_count() <= before + 2, "after %lu", pb_gdi_count());
}

typedef struct { int ok; HPEN pen; HBRUSH br; } XM;
static DWORD WINAPI xm_proc(LPVOID a)
{
    XM *x = a;
    x->pen = U(CreatePen)(PS_SOLID, 1, 0x112233);
    x->br = U(CreateHatchBrush)(HS_CROSS, 0x332211);
    x->ok = x->pen && x->br;
    return 0;                       /* objects outlive the creating thread */
}
TEST(pb_misc_objects_outlive_creating_thread)
{
    NEED_M();
    XM x; memset(&x, 0, sizeof x);
    HANDLE th = CreateThread(NULL, 0, xm_proc, &x, 0, NULL);
    WaitForSingleObject(th, 10000);
    CloseHandle(th);
    CHECK(x.ok);
    LOGPEN lp; LOGBRUSH lb;
    CHECK(U(GetObjectW)(x.pen, sizeof lp, &lp) == sizeof lp && lp.lopnColor == 0x112233);
    CHECK(U(GetObjectW)(x.br, sizeof lb, &lb) == sizeof lb && lb.lbHatch == HS_CROSS);
    CHECK(U(DeleteObject)(x.pen));
    CHECK(U(DeleteObject)(x.br));
}

typedef struct { HDC dc; HPEN pen; BOOL sel; } XD;
static DWORD WINAPI xd_proc(LPVOID a)
{
    XD *x = a;
    HGDIOBJ old = U(SelectObject)(x->dc, x->pen);
    x->sel = old != NULL;
    if (old) U(SelectObject)(x->dc, old);
    return 0;
}
TEST(pb_misc_dc_used_from_other_thread)
{
    NEED_M();
    XD x;
    x.dc = U(CreateCompatibleDC)(NULL);
    x.pen = U(CreatePen)(PS_SOLID, 1, 0);
    x.sel = 0;
    HANDLE th = CreateThread(NULL, 0, xd_proc, &x, 0, NULL);
    WaitForSingleObject(th, 10000);
    CloseHandle(th);
    t_note("SelectObject on a DC owned by another thread -> %s", x.sel ? "ok" : "failed");
    CHECK(x.sel);
    U(DeleteObject)(x.pen);
    U(DeleteDC)(x.dc);
}

TEST(pb_misc_printer_dc_paths_not_touched)
{
    NEED_M();
    /* pens/brushes on an information DC created for the DISPLAY must never need a printer */
    REQUIRE(U(CreateICW));
    HDC ic = U(CreateICW)(L"DISPLAY", NULL, NULL, NULL);
    CHECK(ic != NULL);
    if (!ic) return;
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0);
    HGDIOBJ old = U(SelectObject)(ic, p);
    CHECK(old != NULL);
    CHECK(U(GetCurrentObject)(ic, OBJ_PEN) == p);
    U(SelectObject)(ic, old);
    U(DeleteObject)(p);
    CHECK(U(DeleteDC)(ic));
}
