/* SaveDC / RestoreDC */
#include "dcl.h"

static void del(HDC dc) { if (dc) U(DeleteDC)(dc); }

TEST(dcl_save_first_level) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = dcl_mem();
    EQ(U(SaveDC)(dc), 1);
    EQ(U(SaveDC)(dc), 2);
    EQ(U(SaveDC)(dc), 3);
    CHECK(U(RestoreDC)(dc, 1));
    del(dc);
}

TEST(dcl_save_screen_dc_levels) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    EQ(U(SaveDC)(dc), 1);
    EQ(U(SaveDC)(dc), 2);
    EQ(U(SaveDC)(dc), 3);
    CHECK(U(RestoreDC)(dc, -1));
    EQ(U(SaveDC)(dc), 3);
    CHECK(U(RestoreDC)(dc, 1));
    EQ(U(SaveDC)(dc), 1);
    del(dc);
}

TEST(dcl_save_invalid_handle) {
    REQUIRE(U(SaveDC));
    SET_ERR(0xdeadbeef);
    EQ(U(SaveDC)(NULL), 0);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SET_ERR(0xdeadbeef);
    EQ(U(SaveDC)((HDC)(ULONG_PTR)0xdeadbeef), 0);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SET_ERR(0xdeadbeef);
    EQ(U(SaveDC)((HDC)(ULONG_PTR)0xffffffff), 0);
    t_note("SaveDC(-1) error %lu", GetLastError());
}

TEST(dcl_save_wrong_object_type) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HGDIOBJ objs[3] = { U(CreateSolidBrush)(0), U(CreatePen)(PS_SOLID, 1, 0), U(CreateBitmap)(2, 2, 1, 1, NULL) };
    for (int i = 0; i < 3; i++) {
        EQ(U(SaveDC)((HDC)objs[i]), 0);
        CHECK(!U(RestoreDC)((HDC)objs[i], -1));
        U(DeleteObject)(objs[i]);
    }
    EQ(U(SaveDC)((HDC)U(GetStockObject)(BLACK_PEN)), 0);
}

TEST(dcl_save_deleted_dc) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = dcl_mem();
    CHECK(U(DeleteDC)(dc));
    EQ(U(SaveDC)(dc), 0);
    CHECK(!U(RestoreDC)(dc, -1));
    CHECK(!U(RestoreDC)(dc, 1));
}

TEST(dcl_restore_invalid_handle) {
    REQUIRE(U(RestoreDC));
    SET_ERR(0xdeadbeef);
    CHECK(!U(RestoreDC)(NULL, -1));
    t_note("RestoreDC(NULL) error %lu", GetLastError());
    SET_ERR(0xdeadbeef);
    CHECK(!U(RestoreDC)((HDC)(ULONG_PTR)0xdeadbeef, 1));
    t_note("RestoreDC(0xdeadbeef) error %lu", GetLastError());
}

TEST(dcl_restore_relative_sequence) {
    /* Level bookkeeping, mirrors documented SaveDC/RestoreDC semantics. */
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = dcl_mem();
    CHECK(U(SaveDC)(dc) == 1); CHECK(U(SaveDC)(dc) == 2); CHECK(U(SaveDC)(dc) == 3);
    CHECK(U(RestoreDC)(dc, -1));
    CHECK(U(SaveDC)(dc) == 3);
    CHECK(U(RestoreDC)(dc, 1));
    CHECK(U(SaveDC)(dc) == 1);
    CHECK(U(SaveDC)(dc) == 2);
    CHECK(U(SaveDC)(dc) == 3);
    CHECK(U(RestoreDC)(dc, -2));
    CHECK(U(SaveDC)(dc) == 2);
    CHECK(U(RestoreDC)(dc, -2));
    CHECK(U(SaveDC)(dc) == 1);
    CHECK(U(SaveDC)(dc) == 2);
    del(dc);
}

TEST(dcl_restore_out_of_range) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = dcl_mem();
    U(SaveDC)(dc); U(SaveDC)(dc); /* depth 2 */
    CHECK(!U(RestoreDC)(dc, -4));
    CHECK(!U(RestoreDC)(dc, -3));
    CHECK(!U(RestoreDC)(dc, 3));
    CHECK(!U(RestoreDC)(dc, 100));
    CHECK(!U(RestoreDC)(dc, 0));
    CHECK(!U(RestoreDC)(dc, INT_MAX));
    DCL_WCHECK(!U(RestoreDC)(dc, INT_MIN));
    /* failed restores must not have popped anything */
    EQ(U(SaveDC)(dc), 3);
    del(dc);
}

TEST(dcl_restore_empty_stack) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = dcl_mem();
    CHECK(!U(RestoreDC)(dc, -1));
    CHECK(!U(RestoreDC)(dc, 0));
    CHECK(!U(RestoreDC)(dc, 1));
    CHECK(!U(RestoreDC)(dc, -2));
    EQ(U(SaveDC)(dc), 1);
    del(dc);
}

TEST(dcl_restore_zero_level) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = dcl_mem();
    U(SaveDC)(dc);
    CHECK(!U(RestoreDC)(dc, 0));
    EQ(U(SaveDC)(dc), 2);
    del(dc);
}

TEST(dcl_restore_pops_all_above) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = dcl_mem();
    for (int i = 1; i <= 10; i++) EQ(U(SaveDC)(dc), i);
    CHECK(U(RestoreDC)(dc, 4));      /* depth becomes 3 */
    EQ(U(SaveDC)(dc), 4);
    CHECK(U(RestoreDC)(dc, -4));     /* level 1 -> depth 0 */
    EQ(U(SaveDC)(dc), 1);
    del(dc);
}

TEST(dcl_restore_same_level_twice) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = dcl_mem();
    U(SaveDC)(dc); U(SaveDC)(dc); U(SaveDC)(dc);
    CHECK(U(RestoreDC)(dc, 2));      /* depth 1 */
    CHECK(!U(RestoreDC)(dc, 2));     /* level 2 no longer exists */
    CHECK(U(RestoreDC)(dc, 1));
    del(dc);
}

TEST(dcl_save_deep_stack) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = dcl_mem();
    int bad = 0;
    for (int i = 1; i <= 500; i++) if (U(SaveDC)(dc) != i) bad++;
    CHECK(bad == 0);
    CHECK(U(RestoreDC)(dc, 250));
    EQ(U(SaveDC)(dc), 250);
    CHECK(U(RestoreDC)(dc, 1));
    EQ(U(SaveDC)(dc), 1);
    del(dc);
}

TEST(dcl_save_deep_stack_delete_with_levels) {
    /* DeleteDC with a non-empty save stack must not leak GDI objects */
    REQUIRE(U(SaveDC)); REQUIRE(U(GetGuiResources));
    DWORD before = dcl_gdicount();
    HDC dc = dcl_mem();
    for (int i = 0; i < 100; i++) U(SaveDC)(dc);
    del(dc);
    EQ(dcl_gdicount(), before);
}

TEST(dcl_save_stack_is_per_dc) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC a = dcl_mem(), b = dcl_mem();
    U(SaveDC)(a); U(SaveDC)(a); U(SaveDC)(a);
    EQ(U(SaveDC)(b), 1);
    CHECK(!U(RestoreDC)(b, 2));
    CHECK(U(RestoreDC)(a, 3));
    CHECK(U(RestoreDC)(b, -1));
    del(a); del(b);
}

/* ---- state restored ---- */

typedef struct { const char *name; } nm;

TEST(dcl_restore_colors_and_modes) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = dcl_mem();
    U(SetBkColor)(dc, RGB(10, 20, 30));
    U(SetTextColor)(dc, RGB(40, 50, 60));
    U(SetBkMode)(dc, TRANSPARENT);
    U(SetROP2)(dc, R2_NOT);
    U(SetPolyFillMode)(dc, WINDING);
    U(SetStretchBltMode)(dc, HALFTONE);
    U(SetTextAlign)(dc, TA_RIGHT | TA_BOTTOM);
    U(SetArcDirection)(dc, AD_CLOCKWISE);
    U(SetTextCharacterExtra)(dc, 3);
    int lvl = U(SaveDC)(dc);
    U(SetBkColor)(dc, RGB(1, 1, 1));
    U(SetTextColor)(dc, RGB(2, 2, 2));
    U(SetBkMode)(dc, OPAQUE);
    U(SetROP2)(dc, R2_XORPEN);
    U(SetPolyFillMode)(dc, ALTERNATE);
    U(SetStretchBltMode)(dc, COLORONCOLOR);
    U(SetTextAlign)(dc, TA_LEFT | TA_TOP);
    U(SetArcDirection)(dc, AD_COUNTERCLOCKWISE);
    U(SetTextCharacterExtra)(dc, 9);
    CHECK(U(RestoreDC)(dc, lvl));
    EQ(U(GetBkColor)(dc), RGB(10, 20, 30));
    EQ(U(GetTextColor)(dc), RGB(40, 50, 60));
    EQ(U(GetBkMode)(dc), TRANSPARENT);
    EQ(U(GetROP2)(dc), R2_NOT);
    EQ(U(GetPolyFillMode)(dc), WINDING);
    EQ(U(GetStretchBltMode)(dc), HALFTONE);
    EQ(U(GetTextAlign)(dc), TA_RIGHT | TA_BOTTOM);
    EQ(U(GetArcDirection)(dc), AD_CLOCKWISE);
    EQ(U(GetTextCharacterExtra)(dc), 3);
    del(dc);
}

TEST(dcl_restore_map_mode_and_transform) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = dcl_mem();
    U(SetMapMode)(dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(dc, 100, 200, NULL);
    U(SetViewportExtEx)(dc, 300, 400, NULL);
    U(SetWindowOrgEx)(dc, 5, 6, NULL);
    U(SetViewportOrgEx)(dc, 7, 8, NULL);
    int lvl = U(SaveDC)(dc);
    U(SetMapMode)(dc, MM_TEXT);
    U(SetViewportOrgEx)(dc, 99, 99, NULL);
    U(SetWindowOrgEx)(dc, 98, 98, NULL);
    CHECK(U(RestoreDC)(dc, lvl));
    EQ(U(GetMapMode)(dc), MM_ANISOTROPIC);
    SIZE s; POINT p;
    U(GetWindowExtEx)(dc, &s); CHECK(s.cx == 100 && s.cy == 200);
    U(GetViewportExtEx)(dc, &s); CHECK(s.cx == 300 && s.cy == 400);
    U(GetWindowOrgEx)(dc, &p); CHECK(p.x == 5 && p.y == 6);
    U(GetViewportOrgEx)(dc, &p); CHECK(p.x == 7 && p.y == 8);
    del(dc);
}

TEST(dcl_restore_world_transform) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(SetWorldTransform)); REQUIRE(U(GetWorldTransform));
    HDC dc = dcl_mem();
    CHECK(U(SetGraphicsMode)(dc, GM_ADVANCED) == GM_COMPATIBLE);
    XFORM x = { 2.0f, 0, 0, 3.0f, 10.0f, 20.0f };
    CHECK(U(SetWorldTransform)(dc, &x));
    int lvl = U(SaveDC)(dc);
    XFORM id = { 1, 0, 0, 1, 0, 0 };
    CHECK(U(SetWorldTransform)(dc, &id));
    CHECK(U(RestoreDC)(dc, lvl));
    XFORM g; memset(&g, 0, sizeof g);
    CHECK(U(GetWorldTransform)(dc, &g));
    CHECK(g.eM11 == 2.0f && g.eM22 == 3.0f && g.eDx == 10.0f && g.eDy == 20.0f && g.eM12 == 0 && g.eM21 == 0);
    EQ(U(GetGraphicsMode)(dc), GM_ADVANCED);
    del(dc);
}

TEST(dcl_restore_graphics_mode) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = dcl_mem();
    int lvl = U(SaveDC)(dc);
    U(SetGraphicsMode)(dc, GM_ADVANCED);
    CHECK(U(RestoreDC)(dc, lvl));
    int gm = U(GetGraphicsMode)(dc);
    if (gm != GM_COMPATIBLE) t_note("graphics mode after RestoreDC is %d", gm);
    del(dc);
}

TEST(dcl_restore_selected_objects) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(CreatePen)); REQUIRE(U(CreateSolidBrush));
    HDC dc = dcl_mem();
    HPEN p1 = U(CreatePen)(PS_SOLID, 1, RGB(1, 2, 3)), p2 = U(CreatePen)(PS_DASH, 1, RGB(4, 5, 6));
    HBRUSH b1 = U(CreateSolidBrush)(RGB(7, 8, 9)), b2 = U(CreateSolidBrush)(RGB(9, 8, 7));
    HGDIOBJ op = U(SelectObject)(dc, p1), ob = U(SelectObject)(dc, b1);
    int lvl = U(SaveDC)(dc);
    U(SelectObject)(dc, p2); U(SelectObject)(dc, b2);
    EQ(U(GetCurrentObject)(dc, OBJ_PEN), p2);
    EQ(U(GetCurrentObject)(dc, OBJ_BRUSH), b2);
    CHECK(U(RestoreDC)(dc, lvl));
    EQ(U(GetCurrentObject)(dc, OBJ_PEN), p1);
    EQ(U(GetCurrentObject)(dc, OBJ_BRUSH), b1);
    U(SelectObject)(dc, op); U(SelectObject)(dc, ob);
    del(dc);
    U(DeleteObject)(p1); U(DeleteObject)(p2); U(DeleteObject)(b1); U(DeleteObject)(b2);
}

TEST(dcl_restore_selected_font) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(CreateFontA));
    HDC dc = dcl_mem();
    HFONT f1 = U(CreateFontA)(14, 0, 0, 0, 400, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, "Arial");
    HFONT f2 = U(CreateFontA)(20, 0, 0, 0, 700, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, "Tahoma");
    HGDIOBJ of = U(SelectObject)(dc, f1);
    int lvl = U(SaveDC)(dc);
    U(SelectObject)(dc, f2);
    CHECK(U(RestoreDC)(dc, lvl));
    EQ(U(GetCurrentObject)(dc, OBJ_FONT), f1);
    U(SelectObject)(dc, of);
    del(dc);
    U(DeleteObject)(f1); U(DeleteObject)(f2);
}

TEST(dcl_restore_selected_bitmap) {
    /* The current bitmap is part of the saved state per the docs; observe, do not demand. */
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = dcl_mem();
    HBITMAP a = U(CreateBitmap)(4, 4, 1, 1, NULL), b = U(CreateBitmap)(8, 8, 1, 1, NULL);
    HGDIOBJ old = U(SelectObject)(dc, a);
    int lvl = U(SaveDC)(dc);
    U(SelectObject)(dc, b);
    CHECK(U(RestoreDC)(dc, lvl));
    HGDIOBJ cur = U(GetCurrentObject)(dc, OBJ_BITMAP);
    t_note("bitmap after RestoreDC: %s", cur == a ? "restored" : cur == b ? "not restored" : "other");
    U(SelectObject)(dc, old);
    del(dc);
    U(DeleteObject)(a); U(DeleteObject)(b);
}

TEST(dcl_restore_clip_region) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(IntersectClipRect));
    HBITMAP bmp, old; DWORD *bits;
    HDC dc = dcl_mem32(100, 100, &bmp, &old, &bits);
    RECT r;
    CHECK(U(GetClipBox)(dc, &r) == SIMPLEREGION);
    CHECK(r.left == 0 && r.top == 0 && r.right == 100 && r.bottom == 100);
    int lvl = U(SaveDC)(dc);
    EQ(U(IntersectClipRect)(dc, 10, 10, 50, 60), SIMPLEREGION);
    EQ(U(GetClipBox)(dc, &r), SIMPLEREGION);
    CHECK(r.left == 10 && r.top == 10 && r.right == 50 && r.bottom == 60);
    CHECK(U(RestoreDC)(dc, lvl));
    EQ(U(GetClipBox)(dc, &r), SIMPLEREGION);
    CHECK(r.left == 0 && r.top == 0 && r.right == 100 && r.bottom == 100);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_restore_clip_nested) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(IntersectClipRect));
    HBITMAP bmp, old; DWORD *bits;
    HDC dc = dcl_mem32(100, 100, &bmp, &old, &bits);
    RECT r;
    int l1 = U(SaveDC)(dc);
    U(IntersectClipRect)(dc, 0, 0, 80, 80);
    int l2 = U(SaveDC)(dc);
    U(IntersectClipRect)(dc, 20, 20, 90, 90);
    U(GetClipBox)(dc, &r);
    CHECK(r.left == 20 && r.top == 20 && r.right == 80 && r.bottom == 80);
    CHECK(U(RestoreDC)(dc, l2));
    U(GetClipBox)(dc, &r);
    CHECK(r.left == 0 && r.top == 0 && r.right == 80 && r.bottom == 80);
    CHECK(U(RestoreDC)(dc, l1));
    U(GetClipBox)(dc, &r);
    CHECK(r.left == 0 && r.top == 0 && r.right == 100 && r.bottom == 100);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_restore_clip_affects_drawing) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(IntersectClipRect)); REQUIRE(U(PatBlt));
    HBITMAP bmp, old; DWORD *bits;
    HDC dc = dcl_mem32(32, 32, &bmp, &old, &bits);
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
    HGDIOBJ ob = U(SelectObject)(dc, br);
    int lvl = U(SaveDC)(dc);
    U(IntersectClipRect)(dc, 8, 8, 16, 16);
    U(PatBlt)(dc, 0, 0, 32, 32, PATCOPY);
    U(RestoreDC)(dc, lvl);
    U(GdiFlush)();
    EQ(bits[0], 0);
    EQ(bits[8 * 32 + 8], DCL_RGB32(255, 0, 0));
    EQ(bits[15 * 32 + 15], DCL_RGB32(255, 0, 0));
    EQ(bits[16 * 32 + 16], 0);
    EQ(bits[7 * 32 + 8], 0);
    U(PatBlt)(dc, 0, 0, 32, 32, PATCOPY);
    U(GdiFlush)();
    EQ(bits[0], DCL_RGB32(255, 0, 0));
    EQ(bits[31 * 32 + 31], DCL_RGB32(255, 0, 0));
    U(SelectObject)(dc, ob);
    U(DeleteObject)(br);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_restore_dc_colors) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(SetDCBrushColor)); REQUIRE(U(SetDCPenColor));
    HDC dc = dcl_mem();
    U(SetDCBrushColor)(dc, RGB(11, 22, 33));
    U(SetDCPenColor)(dc, RGB(44, 55, 66));
    int lvl = U(SaveDC)(dc);
    U(SetDCBrushColor)(dc, RGB(1, 1, 1));
    U(SetDCPenColor)(dc, RGB(2, 2, 2));
    CHECK(U(RestoreDC)(dc, lvl));
    if (U(GetDCBrushColor)(dc) != RGB(11, 22, 33)) t_note("DC brush colour not restored by RestoreDC (got 0x%lx)", U(GetDCBrushColor)(dc));
    if (U(GetDCPenColor)(dc) != RGB(44, 55, 66)) t_note("DC pen colour not restored by RestoreDC (got 0x%lx)", U(GetDCPenColor)(dc));
    del(dc);
}

TEST(dcl_restore_current_position) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(MoveToEx));
    HDC dc = dcl_mem();
    U(MoveToEx)(dc, 5, 6, NULL);
    int lvl = U(SaveDC)(dc);
    U(MoveToEx)(dc, 50, 60, NULL);
    U(RestoreDC)(dc, lvl);
    POINT p; U(GetCurrentPositionEx)(dc, &p);
    t_note("current position after RestoreDC: (%ld,%ld)", p.x, p.y);
    del(dc);
}

TEST(dcl_restore_brush_org) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(SetBrushOrgEx));
    HDC dc = dcl_mem();
    U(SetBrushOrgEx)(dc, 3, 4, NULL);
    int lvl = U(SaveDC)(dc);
    U(SetBrushOrgEx)(dc, 30, 40, NULL);
    U(RestoreDC)(dc, lvl);
    POINT p; U(GetBrushOrgEx)(dc, &p);
    t_note("brush origin after RestoreDC: (%ld,%ld)", p.x, p.y);
    del(dc);
}

TEST(dcl_restore_relative_level_state) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = dcl_mem();
    for (int i = 0; i < 5; i++) {
        U(SetBkColor)(dc, RGB(i * 10, i * 10, i * 10));
        U(SaveDC)(dc);
    }
    U(SetBkColor)(dc, RGB(200, 200, 200));
    for (int i = 4; i >= 0; i--) {
        CHECK(U(RestoreDC)(dc, -1));
        EQ(U(GetBkColor)(dc), RGB(i * 10, i * 10, i * 10));
    }
    del(dc);
}

TEST(dcl_restore_absolute_level_state) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = dcl_mem();
    for (int i = 1; i <= 5; i++) {
        U(SetTextColor)(dc, RGB(i, i, i));
        EQ(U(SaveDC)(dc), i);
    }
    U(SetTextColor)(dc, RGB(99, 99, 99));
    CHECK(U(RestoreDC)(dc, 3));
    EQ(U(GetTextColor)(dc), RGB(3, 3, 3));
    CHECK(U(RestoreDC)(dc, 1));
    EQ(U(GetTextColor)(dc), RGB(1, 1, 1));
    del(dc);
}

TEST(dcl_restore_screen_dc_state) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    U(SetBkColor)(dc, RGB(9, 9, 9));
    int lvl = U(SaveDC)(dc);
    U(SetBkColor)(dc, RGB(1, 2, 3));
    U(SetMapMode)(dc, MM_HIMETRIC);
    CHECK(U(RestoreDC)(dc, lvl));
    EQ(U(GetBkColor)(dc), RGB(9, 9, 9));
    EQ(U(GetMapMode)(dc), MM_TEXT);
    del(dc);
}

TEST(dcl_restore_window_dc_state) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HWND hw = dcl_window(0, 0, 50, 50);
    if (!hw) { t_skip("no window"); return; }
    HDC dc = U(GetDC)(hw);
    int lvl = U(SaveDC)(dc);
    CHECK(lvl >= 1);
    U(SetBkMode)(dc, TRANSPARENT);
    CHECK(U(RestoreDC)(dc, lvl));
    EQ(U(GetBkMode)(dc), OPAQUE);
    U(ReleaseDC)(hw, dc);
    U(DestroyWindow)(hw);
}

TEST(dcl_restore_after_object_deleted) {
    /* an object deselected and deleted after SaveDC: restore must not crash or leave a dangling selection */
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = dcl_mem();
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(9, 9, 9));
    HGDIOBJ op = U(SelectObject)(dc, p);
    int lvl = U(SaveDC)(dc);
    U(SelectObject)(dc, op);
    U(DeleteObject)(p);
    BOOL r = U(RestoreDC)(dc, lvl);
    t_note("RestoreDC with deleted saved pen -> %d", r);
    HGDIOBJ cur = U(GetCurrentObject)(dc, OBJ_PEN);
    t_note("current pen after: %p (stock black %p)", cur, U(GetStockObject)(BLACK_PEN));
    U(SelectObject)(dc, U(GetStockObject)(BLACK_PEN));
    del(dc);
}

TEST(dcl_save_restore_object_counts) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(GetGuiResources));
    HDC dc = dcl_mem();
    DWORD before = dcl_gdicount();
    for (int i = 0; i < 200; i++) { U(SaveDC)(dc); U(RestoreDC)(dc, -1); }
    EQ(dcl_gdicount(), before);
    del(dc);
}

TEST(dcl_save_restore_loop_balanced) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = dcl_mem();
    int bad = 0;
    for (int i = 0; i < 5000; i++) {
        if (U(SaveDC)(dc) != 1) bad++;
        if (!U(RestoreDC)(dc, -1)) bad++;
    }
    CHECK(bad == 0);
    del(dc);
}

TEST(dcl_restore_negative_boundaries) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = dcl_mem();
    for (int i = 0; i < 3; i++) U(SaveDC)(dc);
    CHECK(!U(RestoreDC)(dc, -4));
    CHECK(U(RestoreDC)(dc, -3)); /* equals level 1 */
    EQ(U(SaveDC)(dc), 1);
    del(dc);
}

TEST(dcl_restore_cross_dc_independent_state) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC a = dcl_mem(), b = dcl_mem();
    U(SetBkColor)(a, RGB(1, 1, 1)); U(SetBkColor)(b, RGB(2, 2, 2));
    int la = U(SaveDC)(a), lb = U(SaveDC)(b);
    U(SetBkColor)(a, RGB(5, 5, 5)); U(SetBkColor)(b, RGB(6, 6, 6));
    U(RestoreDC)(a, la);
    EQ(U(GetBkColor)(a), RGB(1, 1, 1));
    EQ(U(GetBkColor)(b), RGB(6, 6, 6));
    U(RestoreDC)(b, lb);
    EQ(U(GetBkColor)(b), RGB(2, 2, 2));
    del(a); del(b);
}

TEST(dcl_restore_dibsection_still_selected) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HBITMAP bmp, old; DWORD *bits;
    HDC dc = dcl_mem32(8, 8, &bmp, &old, &bits);
    int lvl = U(SaveDC)(dc);
    U(RestoreDC)(dc, lvl);
    EQ(U(GetCurrentObject)(dc, OBJ_BITMAP), bmp);
    dcl_free_mem(dc, bmp, old);
}

TEST(dcl_save_restore_roundtrip_many_attrs) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(SetTextColor)); REQUIRE(U(SetBkMode)); REQUIRE(U(SetROP2));
    HDC dc = dcl_mem();
    COLORREF tc = U(GetTextColor)(dc); int bm = U(GetBkMode)(dc), rop = U(GetROP2)(dc);
    EQ(U(SaveDC)(dc), 1);
    U(SetTextColor)(dc, RGB(1, 2, 3)); U(SetBkMode)(dc, TRANSPARENT); U(SetROP2)(dc, R2_XORPEN);
    EQ(U(GetROP2)(dc), R2_XORPEN);
    CHECK(U(RestoreDC)(dc, 1));
    EQ(U(GetTextColor)(dc), tc); EQ(U(GetBkMode)(dc), bm); EQ(U(GetROP2)(dc), rop);
    /* level 1 is gone: restoring again fails */
    CHECK(!U(RestoreDC)(dc, 1));
    del(dc);
}

TEST(dcl_save_restore_zero_level) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = dcl_mem();
    U(SaveDC)(dc);
    t_note("RestoreDC(0) -> %d", U(RestoreDC)(dc, 0));
    t_note("RestoreDC(INT_MIN) -> %d", U(RestoreDC)(dc, INT_MIN));
    t_note("RestoreDC(INT_MAX) -> %d", U(RestoreDC)(dc, INT_MAX));
    del(dc);
}
