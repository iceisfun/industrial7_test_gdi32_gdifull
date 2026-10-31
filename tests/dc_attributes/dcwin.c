/* Attributes on window/screen/IC/EMF DCs, DC lifetime, handle recycling, object counts. */
#include "dca.h"

static LRESULT CALLBACK dca_defproc(HWND h, UINT m, WPARAM w, LPARAM l) { return U(DefWindowProcA)(h, m, w, l); }
static ATOM reg_class(const char *name, UINT style) {
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.style = style; wc.lpfnWndProc = dca_defproc;
    wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = name;
    return U(RegisterClassExA)(&wc);
}
static HWND mk_win(const char *cls, DWORD ex) {
    HWND w = U(CreateWindowExA)(ex, cls, "dca", WS_POPUP, 0, 0, 60, 40, NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (!w) t_note("CreateWindowEx(%s) failed, error %lu", cls, GetLastError());
    return w;
}
#define NEEDWIN() do { REQUIRE(U(RegisterClassExA)); REQUIRE(U(CreateWindowExA)); REQUIRE(U(DestroyWindow)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC)); REQUIRE(U(DefWindowProcA)); REQUIRE(U(UnregisterClassA)); } while (0)

TEST(dca_win_common_dc_defaults) {
    NEEDWIN();
    ATOM a = reg_class("DcaCommonCls", 0); if (!a) { t_skip("class"); return; }
    HWND w = mk_win("DcaCommonCls", 0);
    if (!w) { U(UnregisterClassA)("DcaCommonCls", GetModuleHandleA(NULL)); t_skip("window"); return; }
    HDC d = U(GetDC)(w);
    CHECK(d != NULL);
    if (d) {
        for (int i = 0; i < dca_nattrs; i++) {
            const DcaAttr *at = &dca_attrs[i];
            if (!at->avail()) continue;
            CHECKF(at->get(d) == at->def, "%s on window DC = 0x%lx want 0x%lx", at->name, at->get(d), at->def);
        }
        U(ReleaseDC)(w, d);
    }
    U(DestroyWindow)(w); U(UnregisterClassA)("DcaCommonCls", GetModuleHandleA(NULL));
}
TEST(dca_win_common_dc_reset_after_release) {
    NEEDWIN(); REQUIRE(U(SetBkColor)); REQUIRE(U(SetTextColor)); REQUIRE(U(SetBkMode)); REQUIRE(U(SetTextAlign)); REQUIRE(U(SetROP2));
    ATOM a = reg_class("DcaCommonCls2", 0); if (!a) { t_skip("class"); return; }
    HWND w = mk_win("DcaCommonCls2", 0);
    if (!w) { U(UnregisterClassA)("DcaCommonCls2", GetModuleHandleA(NULL)); t_skip("window"); return; }
    HDC d = U(GetDC)(w);
    U(SetBkColor)(d, 0x112233); U(SetTextColor)(d, 0x445566); U(SetBkMode)(d, TRANSPARENT); U(SetTextAlign)(d, TA_RIGHT); U(SetROP2)(d, R2_XORPEN);
    U(ReleaseDC)(w, d);
    d = U(GetDC)(w);
    CHECK(U(GetBkColor)(d) == 0xFFFFFF);
    CHECK(U(GetTextColor)(d) == 0);
    CHECK(U(GetBkMode)(d) == OPAQUE);
    CHECK(U(GetTextAlign)(d) == 0);
    CHECK(U(GetROP2)(d) == R2_COPYPEN);
    U(ReleaseDC)(w, d);
    U(DestroyWindow)(w); U(UnregisterClassA)("DcaCommonCls2", GetModuleHandleA(NULL));
}
TEST(dca_win_owndc_keeps_attributes) {
    NEEDWIN(); REQUIRE(U(SetBkColor)); REQUIRE(U(SetTextColor)); REQUIRE(U(SetBkMode)); REQUIRE(U(SetTextAlign)); REQUIRE(U(SetROP2));
    ATOM a = reg_class("DcaOwnDcCls", CS_OWNDC); if (!a) { t_skip("class"); return; }
    HWND w = mk_win("DcaOwnDcCls", 0);
    if (!w) { U(UnregisterClassA)("DcaOwnDcCls", GetModuleHandleA(NULL)); t_skip("window"); return; }
    HDC d = U(GetDC)(w);
    U(SetBkColor)(d, 0x112233); U(SetTextColor)(d, 0x445566); U(SetBkMode)(d, TRANSPARENT); U(SetTextAlign)(d, TA_RIGHT); U(SetROP2)(d, R2_XORPEN);
    U(ReleaseDC)(w, d);
    HDC d2 = U(GetDC)(w);
    CHECKF(d2 == d, "CS_OWNDC returned a different DC handle");
    CHECK(U(GetBkColor)(d2) == 0x112233);
    CHECK(U(GetTextColor)(d2) == 0x445566);
    CHECK(U(GetBkMode)(d2) == TRANSPARENT);
    CHECK(U(GetTextAlign)(d2) == TA_RIGHT);
    CHECK(U(GetROP2)(d2) == R2_XORPEN);
    U(ReleaseDC)(w, d2);
    U(DestroyWindow)(w); U(UnregisterClassA)("DcaOwnDcCls", GetModuleHandleA(NULL));
}
TEST(dca_win_owndc_isolated_between_windows) {
    NEEDWIN(); REQUIRE(U(SetBkColor)); REQUIRE(U(GetBkColor));
    ATOM a = reg_class("DcaOwnDcCls2", CS_OWNDC); if (!a) { t_skip("class"); return; }
    HWND w1 = mk_win("DcaOwnDcCls2", 0), w2 = mk_win("DcaOwnDcCls2", 0);
    if (!w1 || !w2) { if (w1) U(DestroyWindow)(w1); if (w2) U(DestroyWindow)(w2); U(UnregisterClassA)("DcaOwnDcCls2", GetModuleHandleA(NULL)); t_skip("windows"); return; }
    HDC d1 = U(GetDC)(w1), d2 = U(GetDC)(w2);
    CHECK(d1 != d2);
    U(SetBkColor)(d1, 0x010203);
    CHECK(U(GetBkColor)(d2) == 0xFFFFFF);
    U(ReleaseDC)(w1, d1); U(ReleaseDC)(w2, d2);
    U(DestroyWindow)(w1); U(DestroyWindow)(w2); U(UnregisterClassA)("DcaOwnDcCls2", GetModuleHandleA(NULL));
}
TEST(dca_win_classdc_observed) {
    NEEDWIN(); REQUIRE(U(SetBkColor)); REQUIRE(U(GetBkColor));
    ATOM a = reg_class("DcaClassDcCls", CS_CLASSDC); if (!a) { t_skip("class"); return; }
    HWND w1 = mk_win("DcaClassDcCls", 0), w2 = mk_win("DcaClassDcCls", 0);
    if (!w1 || !w2) { if (w1) U(DestroyWindow)(w1); if (w2) U(DestroyWindow)(w2); U(UnregisterClassA)("DcaClassDcCls", GetModuleHandleA(NULL)); t_skip("windows"); return; }
    HDC d1 = U(GetDC)(w1);
    U(SetBkColor)(d1, 0x010203);
    U(ReleaseDC)(w1, d1);
    HDC d2 = U(GetDC)(w2);
    t_note("CS_CLASSDC: same handle %d, bk colour via other window = 0x%06lx", d1 == d2, U(GetBkColor)(d2));
    U(ReleaseDC)(w2, d2);
    U(DestroyWindow)(w1); U(DestroyWindow)(w2); U(UnregisterClassA)("DcaClassDcCls", GetModuleHandleA(NULL));
}
TEST(dca_win_window_dc_defaults) {
    NEEDWIN(); REQUIRE(U(GetWindowDC));
    ATOM a = reg_class("DcaWinDcCls", 0); if (!a) { t_skip("class"); return; }
    HWND w = mk_win("DcaWinDcCls", 0);
    if (!w) { U(UnregisterClassA)("DcaWinDcCls", GetModuleHandleA(NULL)); t_skip("window"); return; }
    HDC d = U(GetWindowDC)(w);
    CHECK(d != NULL);
    if (d) {
        for (int i = 0; i < dca_nattrs; i++) {
            const DcaAttr *at = &dca_attrs[i];
            if (!at->avail()) continue;
            CHECKF(at->get(d) == at->def, "%s on GetWindowDC = 0x%lx want 0x%lx", at->name, at->get(d), at->def);
        }
        U(ReleaseDC)(w, d);
    }
    U(DestroyWindow)(w); U(UnregisterClassA)("DcaWinDcCls", GetModuleHandleA(NULL));
}
TEST(dca_win_rtl_window_dc_layout_observed) {
    NEEDWIN(); REQUIRE(U(GetLayout));
    ATOM a = reg_class("DcaRtlCls", 0); if (!a) { t_skip("class"); return; }
    HWND w = mk_win("DcaRtlCls", WS_EX_LAYOUTRTL);
    if (!w) { U(UnregisterClassA)("DcaRtlCls", GetModuleHandleA(NULL)); t_skip("window"); return; }
    HDC d = U(GetDC)(w);
    t_note("GetLayout of WS_EX_LAYOUTRTL window DC = 0x%lx", U(GetLayout)(d));
    U(ReleaseDC)(w, d);
    d = U(GetWindowDC)(w);
    t_note("GetLayout of WS_EX_LAYOUTRTL GetWindowDC = 0x%lx", U(GetLayout)(d));
    U(ReleaseDC)(w, d);
    U(DestroyWindow)(w); U(UnregisterClassA)("DcaRtlCls", GetModuleHandleA(NULL));
}
TEST(dca_win_screen_dc_released_is_reset) {
    NEEDWIN(); REQUIRE(U(SetBkColor)); REQUIRE(U(GetBkColor)); REQUIRE(U(SetTextColor)); REQUIRE(U(GetTextColor));
    HDC s = dca_screen(); if (!s) { t_skip("no screen"); return; }
    U(SetBkColor)(s, 0x112233); U(SetTextColor)(s, 0x445566);
    dca_unscreen(s);
    s = dca_screen();
    CHECK(U(GetBkColor)(s) == 0xFFFFFF);
    CHECK(U(GetTextColor)(s) == 0);
    dca_unscreen(s);
}
TEST(dca_dcs_compatible_dc_does_not_inherit_attributes) {
    REQUIRE(U(CreateCompatibleDC));
    HDC s = dca_screen(); if (!s) { t_skip("no screen"); return; }
    for (int i = 0; i < dca_nattrs; i++) { const DcaAttr *a = &dca_attrs[i]; if (a->avail() && a->rt_savedc) a->set(s, dca_nondef(a)); }
    HDC d = U(CreateCompatibleDC)(s);
    CHECK(d != NULL);
    if (d) {
        for (int i = 0; i < dca_nattrs; i++) {
            const DcaAttr *a = &dca_attrs[i];
            if (!a->avail() || !a->rt_savedc) continue;
            CHECKF(a->get(d) == a->def, "%s inherited 0x%lx", a->name, a->get(d));
        }
        U(DeleteDC)(d);
    }
    dca_unscreen(s);
}
TEST(dca_dcs_memdc_from_memdc_does_not_inherit) {
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    for (int i = 0; i < dca_nattrs; i++) { const DcaAttr *a = &dca_attrs[i]; if (a->avail()) a->set(m.dc, dca_nondef(a)); }
    HDC d = U(CreateCompatibleDC)(m.dc);
    CHECK(d != NULL);
    if (d) {
        for (int i = 0; i < dca_nattrs; i++) {
            const DcaAttr *a = &dca_attrs[i];
            if (!a->avail()) continue;
            CHECKF(a->get(d) == a->def, "%s inherited 0x%lx", a->name, a->get(d));
        }
        U(DeleteDC)(d);
    }
    mdc_free(&m);
}
TEST(dca_dcs_recycled_handle_has_defaults) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    for (int round = 0; round < 20; round++) {
        HDC d = U(CreateCompatibleDC)(NULL);
        if (!d) { CHECK(0); return; }
        for (int i = 0; i < dca_nattrs; i++) { const DcaAttr *a = &dca_attrs[i]; if (a->avail()) a->set(d, dca_nondef(a)); }
        U(DeleteDC)(d);
        HDC e = U(CreateCompatibleDC)(NULL);
        for (int i = 0; i < dca_nattrs; i++) {
            const DcaAttr *a = &dca_attrs[i];
            if (!a->avail()) continue;
            if (a->get(e) != a->def) { CHECKF(0, "round %d %s = 0x%lx on new DC (same handle %d)", round, a->name, a->get(e), d == e); round = 99; break; }
        }
        U(DeleteDC)(e);
    }
    CHECK(1);
}
TEST(dca_dcs_gdi_object_count_per_dc) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(GetGuiResources));
    DWORD base = dca_gui();
    HDC d[10];
    for (int i = 0; i < 10; i++) d[i] = U(CreateCompatibleDC)(NULL);
    DWORD mid = dca_gui();
    if (base == 0 && mid == 0) { for (int i = 0; i < 10; i++) if (d[i]) U(DeleteDC)(d[i]); t_skip("GetGuiResources not functional"); return; }
    CHECKF(mid == base + 10, "count %lu -> %lu after 10 DCs", base, mid);
    for (int i = 0; i < 10; i++) if (d[i]) U(DeleteDC)(d[i]);
    CHECKF(dca_gui() == base, "count after delete %lu vs %lu", dca_gui(), base);
}
TEST(dca_dcs_many_dcs_hold_independent_state) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(SetBkColor)); REQUIRE(U(GetBkColor)); REQUIRE(U(SetTextColor)); REQUIRE(U(GetTextColor));
    enum { N = 100 };
    HDC d[N]; int n = 0;
    DWORD base = dca_gui();
    for (; n < N; n++) { d[n] = U(CreateCompatibleDC)(NULL); if (!d[n]) break; }
    CHECKF(n == N, "created %d DCs", n);
    for (int i = 0; i < n; i++) { U(SetBkColor)(d[i], i * 0x010101 & 0xFFFFFF); U(SetTextColor)(d[i], (i * 77) & 0xFFFFFF); U(SetBkMode)(d[i], (i & 1) ? TRANSPARENT : OPAQUE); }
    int bad = 0;
    for (int i = 0; i < n; i++) {
        if (U(GetBkColor)(d[i]) != (COLORREF)(i * 0x010101 & 0xFFFFFF)) bad++;
        if (U(GetTextColor)(d[i]) != (COLORREF)((i * 77) & 0xFFFFFF)) bad++;
        if (U(GetBkMode)(d[i]) != ((i & 1) ? TRANSPARENT : OPAQUE)) bad++;
    }
    CHECKF(bad == 0, "%d mismatches", bad);
    for (int i = 0; i < n; i++) U(DeleteDC)(d[i]);
    CHECK(dca_gui() == base);
}
TEST(dca_dcs_object_types) {
    REQUIRE(U(GetObjectType)); REQUIRE(U(CreateEnhMetaFileW));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    CHECK(U(GetObjectType)(m.dc) == OBJ_MEMDC);
    HDC s = dca_screen();
    if (s) { CHECK(U(GetObjectType)(s) == OBJ_DC); dca_unscreen(s); }
    HDC e = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (e) { CHECK(U(GetObjectType)(e) == OBJ_ENHMETADC); U(DeleteEnhMetaFile)(U(CloseEnhMetaFile)(e)); }
    mdc_free(&m);
}
TEST(dca_dcs_ic_attributes_observed) {
    REQUIRE(U(CreateICW));
    HDC ic = U(CreateICW)(L"DISPLAY", NULL, NULL, NULL);
    if (!ic) { t_skip("no IC"); return; }
    for (int i = 0; i < dca_nattrs; i++) {
        const DcaAttr *a = &dca_attrs[i];
        if (!a->avail()) continue;
        if (a->get(ic) != a->def) t_note("IC %s default 0x%lx", a->name, a->get(ic));
        DWORD v = dca_nondef(a);
        a->set(ic, v);
        if (a->get(ic) != v) t_note("IC %s roundtrip 0x%lx want 0x%lx", a->name, a->get(ic), v);
    }
    CHECK(U(DeleteDC)(ic));
}
TEST(dca_dcs_createdc_display_set_and_delete) {
    REQUIRE(U(CreateDCW)); REQUIRE(U(DeleteDC));
    HDC d = U(CreateDCW)(L"DISPLAY", NULL, NULL, NULL);
    if (!d) { t_skip("no display DC"); return; }
    for (int i = 0; i < dca_nattrs; i++) {
        const DcaAttr *a = &dca_attrs[i];
        if (!a->avail()) continue;
        DWORD v = dca_nondef(a);
        a->set(d, v);
        CHECKF(a->get(d) == v, "%s roundtrip 0x%lx", a->name, a->get(d));
        a->set(d, a->def);
    }
    CHECK(U(DeleteDC)(d));
}
/* EMF: attributes recorded in a metafile do not leak into the DC it is played on */
TEST(dca_emf_playback_restores_target_attributes) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(PlayEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    RECT fr = { 0, 0, 1000, 1000 };
    HDC e = U(CreateEnhMetaFileW)(NULL, NULL, &fr, NULL);
    if (!e) { t_skip("no EMF DC"); return; }
    U(SetBkColor)(e, RGB(255, 0, 0)); U(SetTextColor)(e, RGB(0, 255, 0)); U(SetBkMode)(e, TRANSPARENT); U(SetROP2)(e, R2_XORPEN); U(SetTextAlign)(e, TA_RIGHT);
    U(Rectangle)(e, 10, 10, 200, 200);
    HENHMETAFILE h = U(CloseEnhMetaFile)(e);
    CHECK(h != NULL);
    if (!h) return;
    MDC m; if (!mdc_make(&m, 64, 64)) { U(DeleteEnhMetaFile)(h); t_skip("mdc"); return; }
    U(SetBkColor)(m.dc, 0x111111); U(SetTextColor)(m.dc, 0x222222);
    RECT r = { 0, 0, 64, 64 };
    BOOL ok = U(PlayEnhMetaFile)(m.dc, h, &r);
    CHECK(ok);
    CHECK(U(GetBkColor)(m.dc) == 0x111111);
    CHECK(U(GetTextColor)(m.dc) == 0x222222);
    if (U(GetBkMode)(m.dc) != OPAQUE) t_note("bk mode leaked from metafile playback: %d", U(GetBkMode)(m.dc));
    if (U(GetROP2)(m.dc) != R2_COPYPEN) t_note("ROP2 leaked from metafile playback: %d", U(GetROP2)(m.dc));
    if (U(GetTextAlign)(m.dc) != 0) t_note("text align leaked from metafile playback: 0x%x", U(GetTextAlign)(m.dc));
    U(DeleteEnhMetaFile)(h);
    mdc_free(&m);
}
TEST(dca_emf_dc_start_state_matches_defaults) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC e = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!e) { t_skip("no EMF DC"); return; }
    CHECK(U(GetBkColor)(e) == 0xFFFFFF);
    CHECK(U(GetTextColor)(e) == 0);
    CHECK(U(GetBkMode)(e) == OPAQUE);
    CHECK(U(GetROP2)(e) == R2_COPYPEN);
    CHECK(U(GetPolyFillMode)(e) == ALTERNATE);
    CHECK(U(GetStretchBltMode)(e) == BLACKONWHITE);
    CHECK(U(GetTextAlign)(e) == 0);
    U(DeleteEnhMetaFile)(U(CloseEnhMetaFile)(e));
}
TEST(dca_emf_dc_roundtrip_hard) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC e = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!e) { t_skip("no EMF DC"); return; }
    U(SetBkColor)(e, 0x123456); U(SetTextColor)(e, 0x654321); U(SetBkMode)(e, TRANSPARENT); U(SetROP2)(e, R2_NOT);
    U(SetPolyFillMode)(e, WINDING); U(SetStretchBltMode)(e, HALFTONE); U(SetTextAlign)(e, TA_CENTER | TA_BASELINE);
    CHECK(U(GetBkColor)(e) == 0x123456);
    CHECK(U(GetTextColor)(e) == 0x654321);
    CHECK(U(GetBkMode)(e) == TRANSPARENT);
    CHECK(U(GetROP2)(e) == R2_NOT);
    CHECK(U(GetPolyFillMode)(e) == WINDING);
    CHECK(U(GetStretchBltMode)(e) == HALFTONE);
    CHECK(U(GetTextAlign)(e) == (TA_CENTER | TA_BASELINE));
    U(DeleteEnhMetaFile)(U(CloseEnhMetaFile)(e));
}
TEST(dca_wmf_dc_attribute_calls_observed) {
    REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    HDC w = U(CreateMetaFileW)(NULL);
    if (!w) { t_skip("no WMF DC"); return; }
    DWORD r = U(SetBkColor)(w, 0x123456);
    t_note("WMF DC SetBkColor ret=0x%lx Get=0x%lx", r, U(GetBkColor)(w));
    int m = U(SetBkMode)(w, TRANSPARENT);
    t_note("WMF DC SetBkMode ret=%d Get=%d", m, U(GetBkMode)(w));
    int rop = U(SetROP2)(w, R2_XORPEN);
    t_note("WMF DC SetROP2 ret=%d Get=%d", rop, U(GetROP2)(w));
    UINT ta = U(SetTextAlign)(w, TA_RIGHT);
    t_note("WMF DC SetTextAlign ret=0x%x Get=0x%x", ta, U(GetTextAlign)(w));
    U(DeleteMetaFile)(U(CloseMetaFile)(w));
}
