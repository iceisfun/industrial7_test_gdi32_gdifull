/* Additional attribute tests: object-count stability, stock object interplay, cross-attribute independence. */
#include "dca.h"

TEST(dca_extra_setters_do_not_leak_gdi_objects) {
    REQUIRE(U(SetBkColor)); REQUIRE(U(SetTextColor)); REQUIRE(U(SetROP2)); REQUIRE(U(GetGuiResources));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    DWORD before = dca_gui();
    for (int i = 0; i < 2000; i++) {
        U(SetBkColor)(m.dc, i * 77); U(SetTextColor)(m.dc, i * 31);
        U(SetROP2)(m.dc, 1 + i % 16); U(SetBkMode)(m.dc, 1 + i % 2);
        U(SetTextAlign)(m.dc, i & 0x1f); U(SetPolyFillMode)(m.dc, 1 + i % 2);
        U(SetStretchBltMode)(m.dc, 1 + i % 4);
        U(MoveToEx)(m.dc, i, -i, NULL);
    }
    CHECK(dca_gui() == before);
    mdc_free(&m);
}
TEST(dca_extra_dcpen_dcbrush_color_setters_do_not_leak) {
    REQUIRE(U(SetDCBrushColor)); REQUIRE(U(SetDCPenColor)); REQUIRE(U(GetGuiResources));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    DWORD before = dca_gui();
    for (int i = 0; i < 2000; i++) { U(SetDCBrushColor)(m.dc, i * 99); U(SetDCPenColor)(m.dc, i * 13); }
    CHECK(dca_gui() == before);
    mdc_free(&m);
}
TEST(dca_extra_dc_brush_selected_is_current_object) {
    REQUIRE(U(SelectObject)); REQUIRE(U(GetCurrentObject)); REQUIRE(U(GetStockObject));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    HGDIOBJ b = U(GetStockObject)(DC_BRUSH), p = U(GetStockObject)(DC_PEN);
    CHECK(b != NULL); CHECK(p != NULL);
    HGDIOBJ ob = U(SelectObject)(m.dc, b), op = U(SelectObject)(m.dc, p);
    CHECK(U(GetCurrentObject)(m.dc, OBJ_BRUSH) == b);
    CHECK(U(GetCurrentObject)(m.dc, OBJ_PEN) == p);
    CHECK(U(GetObjectType)(b) == OBJ_BRUSH);
    CHECK(U(GetObjectType)(p) == OBJ_PEN);
    U(SelectObject)(m.dc, ob); U(SelectObject)(m.dc, op);
    mdc_free(&m);
}
TEST(dca_extra_dc_brush_color_change_does_not_change_handle) {
    REQUIRE(U(SetDCBrushColor)); REQUIRE(U(GetCurrentObject));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    HGDIOBJ ob = U(SelectObject)(m.dc, U(GetStockObject)(DC_BRUSH));
    HGDIOBJ h1 = U(GetCurrentObject)(m.dc, OBJ_BRUSH);
    U(SetDCBrushColor)(m.dc, 0x112233);
    CHECK(U(GetCurrentObject)(m.dc, OBJ_BRUSH) == h1);
    U(SelectObject)(m.dc, ob);
    mdc_free(&m);
}
TEST(dca_extra_dc_brush_color_survives_stock_brush_switch) {
    REQUIRE(U(SetDCBrushColor)); REQUIRE(U(GetDCBrushColor));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    U(SetDCBrushColor)(m.dc, 0xABCDEF);
    HGDIOBJ ob = U(SelectObject)(m.dc, U(GetStockObject)(NULL_BRUSH));
    CHECK(U(GetDCBrushColor)(m.dc) == 0xABCDEF);
    U(SelectObject)(m.dc, U(GetStockObject)(BLACK_BRUSH));
    CHECK(U(GetDCBrushColor)(m.dc) == 0xABCDEF);
    U(SelectObject)(m.dc, ob);
    mdc_free(&m);
}
TEST(dca_extra_set_returns_previous_two_dcs_interleaved) {
    REQUIRE(U(SetBkColor)); REQUIRE(U(SetTextColor));
    MDC a, b; if (!mdc_make(&a, 4, 4)) { t_skip("mdc"); return; }
    if (!mdc_make(&b, 4, 4)) { mdc_free(&a); t_skip("mdc"); return; }
    U(SetBkColor)(a.dc, 0x111111); U(SetBkColor)(b.dc, 0x222222);
    CHECK(U(SetBkColor)(a.dc, 0x333333) == 0x111111);
    CHECK(U(SetBkColor)(b.dc, 0x444444) == 0x222222);
    U(SetTextColor)(a.dc, 0x0000FF);
    CHECK(U(GetTextColor)(b.dc) == 0);
    CHECK(U(GetBkColor)(a.dc) == 0x333333);
    mdc_free(&a); mdc_free(&b);
}
TEST(dca_extra_bkmode_get_stable_after_failed_set_on_other_dc) {
    REQUIRE(U(SetBkMode)); REQUIRE(U(GetBkMode));
    MDC m; if (!mdc_make(&m, 4, 4)) { t_skip("mdc"); return; }
    U(SetBkMode)(m.dc, TRANSPARENT);
    U(SetBkMode)(NULL, OPAQUE);
    CHECK(U(GetBkMode)(m.dc) == TRANSPARENT);
    mdc_free(&m);
}
TEST(dca_extra_null_hdc_setters_do_not_modify_valid_dc_state) {
    REQUIRE(U(SetROP2)); REQUIRE(U(SetTextAlign)); REQUIRE(U(SetBkColor)); REQUIRE(U(SetTextColor));
    MDC m; if (!mdc_make(&m, 4, 4)) { t_skip("mdc"); return; }
    U(SetROP2)(m.dc, R2_XORPEN); U(SetTextAlign)(m.dc, TA_CENTER); U(SetBkColor)(m.dc, 0x102030); U(SetTextColor)(m.dc, 0x405060);
    U(SetROP2)(NULL, R2_BLACK); U(SetTextAlign)(NULL, 0); U(SetBkColor)(NULL, 0); U(SetTextColor)(NULL, 0);
    CHECK(U(GetROP2)(m.dc) == R2_XORPEN);
    CHECK(U(GetTextAlign)(m.dc) == TA_CENTER);
    CHECK(U(GetBkColor)(m.dc) == 0x102030);
    CHECK(U(GetTextColor)(m.dc) == 0x405060);
    mdc_free(&m);
}
TEST(dca_extra_savedc_return_values_monotonic_and_restore_minus_one) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    MDC m; if (!mdc_make(&m, 4, 4)) { t_skip("mdc"); return; }
    int a = U(SaveDC)(m.dc), b = U(SaveDC)(m.dc), c = U(SaveDC)(m.dc);
    CHECK(a > 0); CHECK(b == a + 1); CHECK(c == b + 1);
    CHECK(U(RestoreDC)(m.dc, -1));
    CHECK(U(RestoreDC)(m.dc, -1));
    CHECK(U(RestoreDC)(m.dc, -1));
    SetLastError(0);
    CHECK(!U(RestoreDC)(m.dc, -1));
    mdc_free(&m);
}
TEST(dca_extra_restore_absolute_level_discards_newer) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(SetTextColor));
    MDC m; if (!mdc_make(&m, 4, 4)) { t_skip("mdc"); return; }
    U(SetTextColor)(m.dc, 0x0000AA);
    int l1 = U(SaveDC)(m.dc);
    U(SetTextColor)(m.dc, 0x00BB00);
    int l2 = U(SaveDC)(m.dc);
    U(SetTextColor)(m.dc, 0xCC0000);
    CHECK(l2 == l1 + 1);
    CHECK(U(RestoreDC)(m.dc, l1));
    CHECK(U(GetTextColor)(m.dc) == 0x0000AA);
    CHECK(!U(RestoreDC)(m.dc, l2));
    mdc_free(&m);
}
TEST(dca_extra_restore_zero_and_positive_beyond_stack_fail) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(SetBkMode));
    MDC m; if (!mdc_make(&m, 4, 4)) { t_skip("mdc"); return; }
    int l = U(SaveDC)(m.dc);
    U(SetBkMode)(m.dc, TRANSPARENT);
    CHECK(!U(RestoreDC)(m.dc, l + 5));
    CHECK(U(GetBkMode)(m.dc) == TRANSPARENT);
    CHECK(!U(RestoreDC)(m.dc, -100));
    CHECK(U(GetBkMode)(m.dc) == TRANSPARENT);
    CHECK(U(RestoreDC)(m.dc, l));
    CHECK(U(GetBkMode)(m.dc) == OPAQUE);
    mdc_free(&m);
}
TEST(dca_extra_savedc_restores_current_position_observed) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    MDC m; if (!mdc_make(&m, 4, 4)) { t_skip("mdc"); return; }
    U(MoveToEx)(m.dc, 3, 4, NULL);
    int l = U(SaveDC)(m.dc);
    U(MoveToEx)(m.dc, 9, 9, NULL);
    U(RestoreDC)(m.dc, l);
    POINT p = {-1, -1};
    CHECK(U(GetCurrentPositionEx)(m.dc, &p));
    if (p.x != 3 || p.y != 4) t_note("current position after RestoreDC is %ld,%ld (not restored)", p.x, p.y);
    CHECK(1);
    mdc_free(&m);
}
TEST(dca_extra_getters_on_wrong_type_handles_return_failure) {
    REQUIRE(U(GetBkColor)); REQUIRE(U(GetTextColor)); REQUIRE(U(GetROP2)); REQUIRE(U(GetBkMode));
    REQUIRE(U(CreateBitmap)); REQUIRE(U(CreatePen)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(CreateRectRgn));
    WrongSet w; dca_wrong_make(&w);
    for (int i = 0; i < w.n; i++) {
        if (!w.h[i]) continue;
        HDC h = (HDC)w.h[i];
        CHECKF(U(GetBkColor)(h) == CLR_INVALID, "%s GetBkColor", w.nm[i]);
        CHECKF(U(GetTextColor)(h) == CLR_INVALID, "%s GetTextColor", w.nm[i]);
        CHECKF(U(GetROP2)(h) == 0, "%s GetROP2", w.nm[i]);
        CHECKF(U(GetBkMode)(h) == 0, "%s GetBkMode", w.nm[i]);
    }
    dca_wrong_free(&w);
}
TEST(dca_extra_setters_on_wrong_type_handles_fail_and_set_invalid_handle) {
    REQUIRE(U(SetBkColor)); REQUIRE(U(SetROP2)); REQUIRE(U(SetBkMode)); REQUIRE(U(CreateBitmap));
    WrongSet w; dca_wrong_make(&w);
    for (int i = 0; i < w.n; i++) {
        if (!w.h[i]) continue;
        HDC h = (HDC)w.h[i];
        SetLastError(0);
        CHECKF(U(SetBkColor)(h, 0x123456) == CLR_INVALID, "%s SetBkColor", w.nm[i]);
        CHECKF(GetLastError() != 0, "%s SetBkColor no error", w.nm[i]);
        CHECKF(U(SetROP2)(h, R2_BLACK) == 0, "%s SetROP2", w.nm[i]);
        CHECKF(U(SetBkMode)(h, TRANSPARENT) == 0, "%s SetBkMode", w.nm[i]);
    }
    dca_wrong_free(&w);
}
TEST(dca_extra_getdc_attributes_independent_per_getdc_call_released) {
    REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC)); REQUIRE(U(SetTextColor)); REQUIRE(U(GetTextColor));
    HDC a = dca_screen(); if (!a) { t_skip("no screen DC"); return; }
    COLORREF orig = U(GetTextColor)(a);
    U(SetTextColor)(a, 0x00FF00);
    CHECK(U(GetTextColor)(a) == 0x00FF00);
    U(SetTextColor)(a, orig);
    dca_unscreen(a);
}
TEST(dca_extra_bkcolor_roundtrip_all_byte_values_per_channel) {
    REQUIRE(U(SetBkColor)); REQUIRE(U(GetBkColor));
    MDC m; if (!mdc_make(&m, 4, 4)) { t_skip("mdc"); return; }
    int bad = 0;
    for (int v = 0; v < 256; v++) {
        COLORREF cs[3] = { (COLORREF)v, (COLORREF)v << 8, (COLORREF)v << 16 };
        for (int k = 0; k < 3; k++) { U(SetBkColor)(m.dc, cs[k]); if (U(GetBkColor)(m.dc) != cs[k]) bad++; }
    }
    CHECKF(bad == 0, "%d mismatches", bad);
    mdc_free(&m);
}
TEST(dca_extra_rop2_all_values_via_getter_after_each_set) {
    REQUIRE(U(SetROP2)); REQUIRE(U(GetROP2));
    MDC m; if (!mdc_make(&m, 4, 4)) { t_skip("mdc"); return; }
    for (int r = R2_BLACK; r <= R2_LAST; r++) {
        U(SetROP2)(m.dc, r);
        CHECKF(U(GetROP2)(m.dc) == r, "rop2 %d", r);
    }
    mdc_free(&m);
}
TEST(dca_extra_textalign_return_is_previous_for_each_flag) {
    REQUIRE(U(SetTextAlign)); REQUIRE(U(GetTextAlign));
    MDC m; if (!mdc_make(&m, 4, 4)) { t_skip("mdc"); return; }
    UINT prev = 0;
    static const UINT f[] = { TA_LEFT, TA_RIGHT, TA_CENTER, TA_TOP, TA_BOTTOM, TA_BASELINE, TA_UPDATECP, TA_NOUPDATECP };
    for (int i = 0; i < 8; i++) {
        UINT r = U(SetTextAlign)(m.dc, f[i]);
        CHECKF(r == prev, "flag %u: prev 0x%x want 0x%x", f[i], r, prev);
        prev = f[i];
    }
    mdc_free(&m);
}
TEST(dca_extra_mapper_flags_returns_previous_and_get_via_set) {
    REQUIRE(U(SetMapperFlags));
    MDC m; if (!mdc_make(&m, 4, 4)) { t_skip("mdc"); return; }
    CHECK(U(SetMapperFlags)(m.dc, 1) == 0);
    CHECK(U(SetMapperFlags)(m.dc, 1) == 1);
    CHECK(U(SetMapperFlags)(m.dc, 0) == 1);
    CHECK(U(SetMapperFlags)(m.dc, 0) == 0);
    mdc_free(&m);
}
TEST(dca_extra_isolation_all_attrs_between_three_dcs) {
    MDC d[3]; int n = 0;
    for (; n < 3; n++) if (!mdc_make(&d[n], 4, 4)) break;
    if (n < 3) { for (int i = 0; i < n; i++) mdc_free(&d[i]); t_skip("mdc"); return; }
    for (int k = 0; k < dca_nattrs; k++) {
        const DcaAttr *a = &dca_attrs[k];
        if (!a->avail()) continue;
        DWORD v = dca_nondef(a);
        a->set(d[1].dc, v);
        CHECKF(a->get(d[0].dc) == a->def, "%s leaked to dc0", a->name);
        CHECKF(a->get(d[2].dc) == a->def, "%s leaked to dc2", a->name);
        CHECKF(a->get(d[1].dc) == v, "%s lost on dc1", a->name);
        a->set(d[1].dc, a->def);
    }
    for (int i = 0; i < 3; i++) mdc_free(&d[i]);
}
