/* SaveDC / RestoreDC interplay with attributes and other DC state. */
#include "dca.h"

#define NEEDSAVE() do { REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); } while (0)

TEST(dca_savedc_levels_increment) {
    NEEDSAVE();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    int a = U(SaveDC)(m.dc), b = U(SaveDC)(m.dc), c = U(SaveDC)(m.dc);
    CHECKF(a == 1 && b == 2 && c == 3, "levels %d %d %d", a, b, c);
    CHECK(U(RestoreDC)(m.dc, -1));
    int d = U(SaveDC)(m.dc);
    CHECKF(d == 3, "after one pop level is %d", d);
    CHECK(U(RestoreDC)(m.dc, 1));
    int e = U(SaveDC)(m.dc);
    CHECKF(e == 1, "after popping to 1 level is %d", e);
    mdc_free(&m);
}
TEST(dca_savedc_restore_without_save_fails) {
    NEEDSAVE();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    SetLastError(0);
    CHECK(!U(RestoreDC)(m.dc, -1));
    t_note("RestoreDC(-1) with empty stack err=%lu", GetLastError());
    CHECK(!U(RestoreDC)(m.dc, 1));
    CHECK(!U(RestoreDC)(m.dc, 0));
    CHECK(!U(RestoreDC)(m.dc, 2));
    CHECK(!U(RestoreDC)(m.dc, -2));
    t_note("RestoreDC(INT_MIN)=%d", U(RestoreDC)(m.dc, INT_MIN));
    CHECK(!U(RestoreDC)(m.dc, INT_MAX));
    mdc_free(&m);
}
TEST(dca_savedc_restore_bad_levels) {
    NEEDSAVE();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    int s1 = U(SaveDC)(m.dc), s2 = U(SaveDC)(m.dc);
    CHECK(s1 == 1 && s2 == 2);
    CHECK(!U(RestoreDC)(m.dc, 3));
    CHECK(!U(RestoreDC)(m.dc, 0));
    CHECK(!U(RestoreDC)(m.dc, -3));
    t_note("RestoreDC(INT_MIN)=%d", U(RestoreDC)(m.dc, INT_MIN));
    CHECK(!U(RestoreDC)(m.dc, INT_MAX));
    CHECK(U(RestoreDC)(m.dc, -2));               /* pops both */
    CHECK(!U(RestoreDC)(m.dc, -1));
    mdc_free(&m);
}
TEST(dca_savedc_restored_level_is_consumed) {
    NEEDSAVE();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    int s1 = U(SaveDC)(m.dc), s2 = U(SaveDC)(m.dc), s3 = U(SaveDC)(m.dc);
    (void)s3;
    CHECK(U(RestoreDC)(m.dc, s2));
    CHECK(!U(RestoreDC)(m.dc, s2));            /* level 2 and above are gone */
    CHECK(U(RestoreDC)(m.dc, s1));
    CHECK(!U(RestoreDC)(m.dc, s1));
    mdc_free(&m);
}
TEST(dca_savedc_bad_dc) {
    NEEDSAVE();
    SetLastError(0);
    CHECK(U(SaveDC)(NULL) == 0);
    t_note("SaveDC(NULL) err=%lu", GetLastError());
    SetLastError(0);
    CHECK(!U(RestoreDC)(NULL, -1));
    t_note("RestoreDC(NULL) err=%lu", GetLastError());
    for (int i = 0; i < DCA_NGARBAGE; i++) {
        CHECKF(U(SaveDC)((HDC)dca_garbage[i]) == 0, "SaveDC garbage %d", i);
        CHECKF(!U(RestoreDC)((HDC)dca_garbage[i], -1), "RestoreDC garbage %d", i);
    }
    WrongSet w; dca_wrong_make(&w);
    for (int i = 0; i < w.n; i++) if (w.h[i]) {
        CHECKF(U(SaveDC)((HDC)w.h[i]) == 0, "SaveDC %s", w.nm[i]);
        CHECKF(!U(RestoreDC)((HDC)w.h[i], -1), "RestoreDC %s", w.nm[i]);
    }
    dca_wrong_free(&w);
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    HDC d = m.dc; mdc_free(&m);
    CHECK(U(SaveDC)(d) == 0);
    CHECK(!U(RestoreDC)(d, -1));
}
TEST(dca_savedc_all_attributes_at_once) {
    NEEDSAVE();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    DWORD base[32], mid[32];
    int n = dca_nattrs, usable = 0;
    for (int i = 0; i < n; i++) if (dca_attrs[i].avail()) {
        const DcaAttr *a = &dca_attrs[i];
        base[i] = dca_nondef(a);
        a->set(m.dc, base[i]);
    }
    int s = U(SaveDC)(m.dc);
    CHECK(s > 0);
    for (int i = 0; i < n; i++) if (dca_attrs[i].avail()) {
        const DcaAttr *a = &dca_attrs[i];
        mid[i] = dca_nondef2(a, base[i]);
        a->set(m.dc, mid[i]);
    }
    CHECK(U(RestoreDC)(m.dc, -1));
    for (int i = 0; i < n; i++) if (dca_attrs[i].avail()) {
        const DcaAttr *a = &dca_attrs[i];
        DWORD g = a->get(m.dc);
        usable++;
        if (a->rt_savedc) CHECKF(g == base[i], "%s: 0x%lx want 0x%lx", a->name, g, base[i]);
        else t_note("%s after RestoreDC: 0x%lx (saved 0x%lx, changed to 0x%lx)", a->name, g, base[i], mid[i]);
    }
    CHECK(usable > 0);
    mdc_free(&m);
}
TEST(dca_savedc_deep_nesting_bkcolor) {
    NEEDSAVE(); REQUIRE(U(SetBkColor)); REQUIRE(U(GetBkColor));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    const int N = 100;
    for (int i = 0; i < N; i++) {
        U(SetBkColor)(m.dc, 0x010101 * (i % 200));
        CHECKF(U(SaveDC)(m.dc) == i + 1, "level %d", i);
    }
    int bad = 0;
    for (int i = N - 1; i >= 0; i--) {
        if (!U(RestoreDC)(m.dc, -1)) bad++;
        if (U(GetBkColor)(m.dc) != (COLORREF)(0x010101 * (i % 200))) bad++;
    }
    CHECKF(bad == 0, "%d mismatches", bad);
    CHECK(!U(RestoreDC)(m.dc, -1));
    mdc_free(&m);
}
TEST(dca_savedc_many_levels) {
    NEEDSAVE();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    int last = 0, ok = 1;
    for (int i = 1; i <= 2000; i++) { last = U(SaveDC)(m.dc); if (last != i) { ok = 0; break; } }
    CHECKF(ok, "SaveDC stopped counting at %d", last);
    CHECK(U(RestoreDC)(m.dc, 1));
    CHECK(U(SaveDC)(m.dc) == 1);
    CHECK(U(RestoreDC)(m.dc, -1));
    mdc_free(&m);
}
TEST(dca_savedc_no_object_leak) {
    NEEDSAVE();
    if (!U(GetGuiResources)) { t_skip("no GetGuiResources"); return; }
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    DWORD before = dca_gui();
    for (int i = 0; i < 200; i++) { int s = U(SaveDC)(m.dc); U(RestoreDC)(m.dc, s); }
    for (int i = 0; i < 50; i++) U(SaveDC)(m.dc);
    CHECK(U(RestoreDC)(m.dc, 1));
    DWORD after = dca_gui();
    CHECKF(after == before, "GDI objects %lu -> %lu", before, after);
    mdc_free(&m);
}
TEST(dca_savedc_restores_pen_brush_font) {
    NEEDSAVE();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    HPEN p1 = U(CreatePen)(PS_SOLID, 1, 0xFF), p2 = U(CreatePen)(PS_SOLID, 2, 0xFF00);
    HBRUSH b1 = U(CreateSolidBrush)(0xFF), b2 = U(CreateSolidBrush)(0xFF00);
    HFONT f1 = U(CreateFontA)(12, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, "Arial"), f2 = U(CreateFontA)(20, 0, 0, 0, 700, 0, 0, 0, 0, 0, 0, 0, 0, "Arial");
    HGDIOBJ op = U(SelectObject)(m.dc, p1), ob = U(SelectObject)(m.dc, b1), of = U(SelectObject)(m.dc, f1);
    int s = U(SaveDC)(m.dc);
    U(SelectObject)(m.dc, p2); U(SelectObject)(m.dc, b2); U(SelectObject)(m.dc, f2);
    CHECK(U(GetCurrentObject)(m.dc, OBJ_PEN) == p2 && U(GetCurrentObject)(m.dc, OBJ_BRUSH) == b2 && U(GetCurrentObject)(m.dc, OBJ_FONT) == f2);
    CHECK(U(RestoreDC)(m.dc, s));
    CHECK(U(GetCurrentObject)(m.dc, OBJ_PEN) == p1);
    CHECK(U(GetCurrentObject)(m.dc, OBJ_BRUSH) == b1);
    CHECK(U(GetCurrentObject)(m.dc, OBJ_FONT) == f1);
    U(SelectObject)(m.dc, op); U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, of);
    U(DeleteObject)(p1); U(DeleteObject)(p2); U(DeleteObject)(b1); U(DeleteObject)(b2); U(DeleteObject)(f1); U(DeleteObject)(f2);
    mdc_free(&m);
}
TEST(dca_savedc_restores_mapping_state) {
    NEEDSAVE(); REQUIRE(U(SetMapMode)); REQUIRE(U(GetMapMode)); REQUIRE(U(SetViewportOrgEx)); REQUIRE(U(GetViewportOrgEx)); REQUIRE(U(SetWindowOrgEx)); REQUIRE(U(GetWindowOrgEx));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    U(SetMapMode)(m.dc, MM_LOMETRIC);
    U(SetViewportOrgEx)(m.dc, 3, 4, NULL); U(SetWindowOrgEx)(m.dc, 5, 6, NULL);
    int s = U(SaveDC)(m.dc);
    U(SetMapMode)(m.dc, MM_TEXT); U(SetViewportOrgEx)(m.dc, 30, 40, NULL); U(SetWindowOrgEx)(m.dc, 50, 60, NULL);
    CHECK(U(RestoreDC)(m.dc, s));
    POINT v, w;
    U(GetViewportOrgEx)(m.dc, &v); U(GetWindowOrgEx)(m.dc, &w);
    CHECKF(U(GetMapMode)(m.dc) == MM_LOMETRIC, "map mode %d", U(GetMapMode)(m.dc));
    CHECKF(v.x == 3 && v.y == 4 && w.x == 5 && w.y == 6, "orgs (%ld,%ld) (%ld,%ld)", v.x, v.y, w.x, w.y);
    mdc_free(&m);
}
TEST(dca_savedc_restores_clip_region) {
    NEEDSAVE(); REQUIRE(U(IntersectClipRect)); REQUIRE(U(GetClipBox));
    MDC m; if (!mdc_make(&m, 40, 40)) { t_skip("mdc"); return; }
    int s = U(SaveDC)(m.dc);
    U(IntersectClipRect)(m.dc, 5, 5, 15, 15);
    RECT r; U(GetClipBox)(m.dc, &r);
    CHECKF(r.left == 5 && r.top == 5 && r.right == 15 && r.bottom == 15, "clip (%ld,%ld,%ld,%ld)", r.left, r.top, r.right, r.bottom);
    CHECK(U(RestoreDC)(m.dc, s));
    U(GetClipBox)(m.dc, &r);
    CHECKF(r.left == 0 && r.top == 0 && r.right == 40 && r.bottom == 40, "restored clip (%ld,%ld,%ld,%ld)", r.left, r.top, r.right, r.bottom);
    mdc_free(&m);
}
TEST(dca_savedc_restore_does_not_touch_other_dc) {
    NEEDSAVE(); REQUIRE(U(SetBkColor)); REQUIRE(U(GetBkColor));
    MDC a, b; if (!mdc_make(&a, 8, 8)) { t_skip("mdc"); return; }
    if (!mdc_make(&b, 8, 8)) { mdc_free(&a); t_skip("mdc"); return; }
    int s = U(SaveDC)(a.dc);
    U(SetBkColor)(a.dc, 0x112233); U(SetBkColor)(b.dc, 0x445566);
    CHECK(U(RestoreDC)(a.dc, s));
    CHECK(U(GetBkColor)(a.dc) == 0xFFFFFF);
    CHECK(U(GetBkColor)(b.dc) == 0x445566);
    CHECK(!U(RestoreDC)(b.dc, s));              /* b never saved */
    mdc_free(&a); mdc_free(&b);
}
TEST(dca_savedc_restore_after_attribute_set_in_saved_level_only) {
    NEEDSAVE(); REQUIRE(U(SetTextColor)); REQUIRE(U(GetTextColor));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    int s1 = U(SaveDC)(m.dc);
    U(SetTextColor)(m.dc, 0x010101);
    int s2 = U(SaveDC)(m.dc);
    U(SetTextColor)(m.dc, 0x020202);
    CHECK(U(RestoreDC)(m.dc, s1));              /* skip s2 */
    CHECK(U(GetTextColor)(m.dc) == 0);
    CHECK(!U(RestoreDC)(m.dc, s2));
    mdc_free(&m);
}
TEST(dca_savedc_current_object_handles_survive) {
    NEEDSAVE();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    HGDIOBJ b0 = U(GetCurrentObject)(m.dc, OBJ_BITMAP);
    CHECK(b0 == m.bmp);
    int s = U(SaveDC)(m.dc);
    CHECK(U(GetCurrentObject)(m.dc, OBJ_BITMAP) == m.bmp);
    CHECK(U(RestoreDC)(m.dc, s));
    CHECK(U(GetCurrentObject)(m.dc, OBJ_BITMAP) == m.bmp);
    mdc_free(&m);
}
TEST(dca_savedc_screen_dc) {
    NEEDSAVE(); REQUIRE(U(SetBkColor)); REQUIRE(U(GetBkColor));
    HDC s = dca_screen(); if (!s) { t_skip("no screen"); return; }
    int lvl = U(SaveDC)(s);
    CHECK(lvl > 0);
    U(SetBkColor)(s, 0x123456);
    CHECK(U(RestoreDC)(s, lvl));
    CHECK(U(GetBkColor)(s) == 0xFFFFFF);
    dca_unscreen(s);
}
TEST(dca_savedc_emf_dc) {
    NEEDSAVE(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC d = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!d) { t_skip("no EMF DC"); return; }
    int s = U(SaveDC)(d);
    t_note("SaveDC on EMF DC = %d", s);
    U(SetBkColor)(d, 0x123456);
    BOOL r = U(RestoreDC)(d, -1);
    t_note("RestoreDC on EMF DC = %d, GetBkColor=0x%06lx", r, U(GetBkColor)(d));
    U(DeleteEnhMetaFile)(U(CloseEnhMetaFile)(d));
}
TEST(dca_savedc_other_thread_restore_observed) {
    NEEDSAVE();
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    int s = U(SaveDC)(m.dc);
    U(SetBkColor)(m.dc, 0x112233);
    struct A { HDC dc; int s; BOOL ok; } a = { m.dc, s, 0 };
    DWORD WINAPI th(LPVOID p) { struct A *x = p; x->ok = U(RestoreDC)(x->dc, x->s); return 0; }
    HANDLE t = CreateThread(NULL, 0, th, &a, 0, NULL);
    if (t) { WaitForSingleObject(t, 10000); CloseHandle(t); }
    CHECK(a.ok);
    CHECK(U(GetBkColor)(m.dc) == 0xFFFFFF);
    mdc_free(&m);
}
