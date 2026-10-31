/* DeviceCapabilitiesEx*, CreateDCExW, NamedEscape, GetAspectRatioFilterEx, GetTransform */
#include "gap.h"

UDECL(int, DeviceCapabilitiesExA, (LPCSTR, LPCSTR, WORD, LPSTR, const void *));
UDECL(int, DeviceCapabilitiesExW, (LPCWSTR, LPCWSTR, WORD, LPWSTR, const void *));
UDECL(HDC, CreateDCExW, (LPCWSTR, LPCWSTR, LPCWSTR, const DEVMODEW *, DWORD));
UDECL(int, NamedEscape, (HDC, LPCWSTR, int, int, LPCSTR, int, LPSTR));
UDECL(int, GetTransform, (HDC, DWORD, XFORM *));
UDECL(BOOL, GetAspectRatioFilterEx2, (HDC, SIZE *));

/* ---------------- DeviceCapabilitiesExA / W ---------------- */
TEST(gap0_dcex_resolve) {
    HMODULE g = t_gdi32();
    CHECK(GetProcAddress(g, "DeviceCapabilitiesExA") != NULL);
    CHECK(GetProcAddress(g, "DeviceCapabilitiesExW") != NULL);
    CHECK(GetProcAddress(g, "DeviceCapabilitiesExA") == GetProcAddress(g, "DeviceCapabilitiesExA"));
    /* both are forwarded to the same implementation; resolving through gdi32 must give identical code */
    FARPROC a = GetProcAddress(g, "DeviceCapabilitiesExA"), w = GetProcAddress(g, "DeviceCapabilitiesExW");
    CHECK(a != NULL && w != NULL);
    SOFT(a == w, "ExA and ExW resolve to the same address");
    CHECK(GetProcAddress(g, "DeviceCapabilitiesExA") != GetProcAddress(g, "DeviceCapabilitiesA"));
}
TEST(gap0_dcex_a_null_args) {
    DeviceCapabilitiesExA_pfn f = UN(DeviceCapabilitiesExA); REQUIRE(f); SKIP_WINE("unimplemented stub");
    EXPECT_EQ(f(NULL, NULL, 0, NULL, NULL), -1);
}
TEST(gap0_dcex_a_every_capability) {
    DeviceCapabilitiesExA_pfn f = UN(DeviceCapabilitiesExA); REQUIRE(f); SKIP_WINE("unimplemented stub");
    for (int cap = 0; cap <= 0x40; cap++) {
        int r = f("x", "y", (WORD)cap, NULL, NULL);
        CHECKF(r == -1, "cap %d returned %d", cap, r);
    }
}
TEST(gap0_dcex_a_high_caps) {
    DeviceCapabilitiesExA_pfn f = UN(DeviceCapabilitiesExA); REQUIRE(f); SKIP_WINE("unimplemented stub");
    WORD caps[] = { 0x7fff, 0x8000, 0xfffe, 0xffff, 100, 255, 256, 1000 };
    for (unsigned i = 0; i < sizeof caps / sizeof *caps; i++)
        CHECKF(f("x", "y", caps[i], NULL, NULL) == -1, "cap 0x%x", caps[i]);
}
TEST(gap0_dcex_a_output_untouched) {
    DeviceCapabilitiesExA_pfn f = UN(DeviceCapabilitiesExA); REQUIRE(f); SKIP_WINE("unimplemented stub");
    char buf[64]; memset(buf, 0xCC, sizeof buf);
    CHECK(f("Microsoft Print to PDF", "PORTPROMPT:", 2 /*DC_PAPERS*/, buf, NULL) == -1);
    int ok = 1; for (int i = 0; i < 64; i++) if ((unsigned char)buf[i] != 0xCC) ok = 0;
    CHECK(ok);
}
TEST(gap0_dcex_a_devmode_ignored) {
    DeviceCapabilitiesExA_pfn f = UN(DeviceCapabilitiesExA); REQUIRE(f); SKIP_WINE("unimplemented stub");
    DEVMODEA dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    CHECK(f("a", "b", 18, NULL, &dm) == -1);
    CHECK(f(NULL, NULL, 18, NULL, &dm) == -1);
}
TEST(gap0_dcex_a_lasterror_untouched) {
    DeviceCapabilitiesExA_pfn f = UN(DeviceCapabilitiesExA); REQUIRE(f); SKIP_WINE("unimplemented stub");
    SetLastError(0x1234);
    f(NULL, NULL, 1, NULL, NULL);
    EXPECT_EQ(GetLastError(), 0x1234);
}
TEST(gap0_dcex_a_repeat) {
    DeviceCapabilitiesExA_pfn f = UN(DeviceCapabilitiesExA); REQUIRE(f); SKIP_WINE("unimplemented stub");
    int bad = 0;
    for (int i = 0; i < 2000; i++) if (f("p", "q", (WORD)(i & 31), NULL, NULL) != -1) bad++;
    EXPECT_EQ(bad, 0);
}
TEST(gap0_dcex_a_no_gdi_leak) {
    DeviceCapabilitiesExA_pfn f = UN(DeviceCapabilitiesExA); REQUIRE(f); SKIP_WINE("unimplemented stub");
    DWORD before = gap_gdi_count();
    for (int i = 0; i < 100; i++) f("p", "q", 1, NULL, NULL);
    EXPECT_EQ(gap_gdi_count(), before);
}
TEST(gap0_dcex_w_null_args) {
    DeviceCapabilitiesExW_pfn f = UN(DeviceCapabilitiesExW); REQUIRE(f); SKIP_WINE("unimplemented stub");
    EXPECT_EQ(f(NULL, NULL, 0, NULL, NULL), -1);
}
TEST(gap0_dcex_w_every_capability) {
    DeviceCapabilitiesExW_pfn f = UN(DeviceCapabilitiesExW); REQUIRE(f); SKIP_WINE("unimplemented stub");
    for (int cap = 0; cap <= 0x40; cap++) {
        int r = f(L"x", L"y", (WORD)cap, NULL, NULL);
        CHECKF(r == -1, "cap %d returned %d", cap, r);
    }
}
TEST(gap0_dcex_w_output_untouched) {
    DeviceCapabilitiesExW_pfn f = UN(DeviceCapabilitiesExW); REQUIRE(f); SKIP_WINE("unimplemented stub");
    WCHAR buf[32]; memset(buf, 0xCC, sizeof buf);
    CHECK(f(L"Microsoft Print to PDF", L"PORTPROMPT:", 16 /*DC_DRIVER*/, buf, NULL) == -1);
    int ok = 1; for (int i = 0; i < 32; i++) if (buf[i] != 0xCCCC) ok = 0;
    CHECK(ok);
}
TEST(gap0_dcex_ab_parity) {
    DeviceCapabilitiesExA_pfn a = UN(DeviceCapabilitiesExA);
    DeviceCapabilitiesExW_pfn w = UN(DeviceCapabilitiesExW); REQUIRE(a); REQUIRE(w); SKIP_WINE("unimplemented stub");
    for (int cap = 0; cap < 40; cap++)
        CHECKF(a("d", "p", (WORD)cap, NULL, NULL) == w(L"d", L"p", (WORD)cap, NULL, NULL), "cap %d", cap);
}
TEST(gap0_dcex_w_lasterror_untouched) {
    DeviceCapabilitiesExW_pfn f = UN(DeviceCapabilitiesExW); REQUIRE(f); SKIP_WINE("unimplemented stub");
    SetLastError(0x4321);
    f(L"d", L"p", 3, NULL, NULL);
    EXPECT_EQ(GetLastError(), 0x4321);
}
TEST(gap0_dcex_in_gdi32full) {
    SKIP_WINE("unimplemented stub");
    FARPROC p = t_proc_gdi32full("DeviceCapabilitiesExA");
    CHECK(p != NULL);
    if (p) CHECK(((int (WINAPI *)(LPCSTR, LPCSTR, WORD, LPSTR, const void *))p)(NULL, NULL, 9, NULL, NULL) == -1);
}

/* ---------------- CreateDCExW ---------------- */
TEST(gap0_cdcex_resolve) {
    CHECK(t_proc_gdi32("CreateDCExW") != NULL);
    CHECK(t_proc_gdi32full("CreateDCExW") != NULL);
}
TEST(gap0_cdcex_type_even_returns_null) {
    CreateDCExW_pfn f = UN(CreateDCExW); REQUIRE(f);
    DWORD types[] = { 0, 2, 4, 6, 8, 0x10, 0x100, 0x1000, 0x80000000, 0xFFFFFFFE };
    for (unsigned i = 0; i < sizeof types / sizeof *types; i++) {
        DWORD before = gap_gdi_count();
        HDC dc = f(L"DISPLAY", NULL, NULL, NULL, types[i]);
        CHECKF(dc == NULL, "type 0x%lx gave %p", types[i], dc);
        if (dc) U(DeleteDC)(dc);
        EXPECT_EQ(gap_gdi_count(), before);
    }
}
TEST(gap0_cdcex_type_even_null_all) {
    CreateDCExW_pfn f = UN(CreateDCExW); REQUIRE(f);
    CHECK(f(NULL, NULL, NULL, NULL, 0) == NULL);
}
TEST(gap0_cdcex_type_even_no_error_set) {
    CreateDCExW_pfn f = UN(CreateDCExW); REQUIRE(f);
    SetLastError(0xBEEF);
    HDC dc = f(L"DISPLAY", NULL, NULL, NULL, 0);
    CHECK(dc == NULL);
    EXPECT_EQ(GetLastError(), 0xBEEF);
}
TEST(gap0_cdcex_display_basic) {
    CreateDCExW_pfn f = UN(CreateDCExW); REQUIRE(f);
    REQUIRE(U(DeleteDC)); REQUIRE(U(GetDeviceCaps)); REQUIRE(U(GetObjectType));
    HDC dc = f(L"DISPLAY", NULL, NULL, NULL, 1);
    CHECK(dc != NULL);
    if (!dc) return;
    EXPECT_EQ(U(GetObjectType)(dc), OBJ_DC);
    CHECK(U(GetDeviceCaps)(dc, HORZRES) > 0);
    CHECK(U(GetDeviceCaps)(dc, VERTRES) > 0);
    CHECK(U(GetDeviceCaps)(dc, BITSPIXEL) > 0);
    EXPECT_EQ(U(GetDeviceCaps)(dc, TECHNOLOGY), DT_RASDISPLAY);
    CHECK(U(DeleteDC)(dc));
}
TEST(gap0_cdcex_display_matches_createdcw) {
    CreateDCExW_pfn f = UN(CreateDCExW); REQUIRE(f);
    REQUIRE(U(CreateDCW)); REQUIRE(U(GetDeviceCaps));
    HDC a = f(L"DISPLAY", NULL, NULL, NULL, 1), b = U(CreateDCW)(L"DISPLAY", NULL, NULL, NULL);
    CHECK(a && b);
    if (a && b) {
        int idx[] = { HORZRES, VERTRES, BITSPIXEL, PLANES, LOGPIXELSX, LOGPIXELSY, TECHNOLOGY, NUMCOLORS, SIZEPALETTE, RASTERCAPS, CURVECAPS, LINECAPS, POLYGONALCAPS, TEXTCAPS };
        for (unsigned i = 0; i < sizeof idx / sizeof *idx; i++)
            CHECKF(U(GetDeviceCaps)(a, idx[i]) == U(GetDeviceCaps)(b, idx[i]), "cap %d", idx[i]);
    }
    if (a) U(DeleteDC)(a);
    if (b) U(DeleteDC)(b);
}
TEST(gap0_cdcex_odd_types) {
    CreateDCExW_pfn f = UN(CreateDCExW); REQUIRE(f); REQUIRE(U(DeleteDC));
    DWORD types[] = { 1, 3, 5, 0x11, 0x101, 0x80000001, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof types / sizeof *types; i++) {
        HDC dc = f(L"DISPLAY", NULL, NULL, NULL, types[i]);
        SOFT(dc != NULL, "odd type creates a DC");
        if (dc) CHECK(U(DeleteDC)(dc));
    }
}
TEST(gap0_cdcex_default_objects) {
    CreateDCExW_pfn f = UN(CreateDCExW); REQUIRE(f);
    REQUIRE(U(GetCurrentObject)); REQUIRE(U(GetStockObject));
    HDC dc = f(L"DISPLAY", NULL, NULL, NULL, 1);
    if (!dc) { t_skip("no display dc"); return; }
    EXPECT_EQ(U(GetCurrentObject)(dc, OBJ_PEN), U(GetStockObject)(BLACK_PEN));
    EXPECT_EQ(U(GetCurrentObject)(dc, OBJ_BRUSH), U(GetStockObject)(WHITE_BRUSH));
    EXPECT_EQ(U(GetCurrentObject)(dc, OBJ_PAL), U(GetStockObject)(DEFAULT_PALETTE));
    EXPECT_EQ(U(GetBkMode)(dc), OPAQUE);
    EXPECT_EQ(U(GetMapMode)(dc), MM_TEXT);
    EXPECT_EQ(U(GetROP2)(dc), R2_COPYPEN);
    U(DeleteDC)(dc);
}
TEST(gap0_cdcex_bad_driver) {
    CreateDCExW_pfn f = UN(CreateDCExW); REQUIRE(f);
    DWORD before = gap_gdi_count();
    SetLastError(0);
    HDC dc = f(L"NO_SUCH_DRIVER_GAP0", NULL, NULL, NULL, 1);
    DWORD e = GetLastError();
    CHECK(dc == NULL);
    t_note("bad driver last error %lu", e);
    if (dc) U(DeleteDC)(dc);
    EXPECT_EQ(gap_gdi_count(), before);
}
TEST(gap0_cdcex_all_null) {
    CreateDCExW_pfn f = UN(CreateDCExW); REQUIRE(f);
    HDC dc = f(NULL, NULL, NULL, NULL, 1);
    SOFT(dc == NULL, "all-NULL CreateDCExW fails like CreateDCW");
    if (dc) U(DeleteDC)(dc);
}
TEST(gap0_cdcex_devmode_zero_fields) {
    CreateDCExW_pfn f = UN(CreateDCExW); REQUIRE(f);
    DEVMODEW dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm; dm.dmSpecVersion = DM_SPECVERSION;
    HDC dc = f(L"DISPLAY", NULL, NULL, &dm, 1);
    SOFT(dc != NULL, "DISPLAY with empty devmode");
    if (dc) U(DeleteDC)(dc);
}
TEST(gap0_cdcex_repeat_no_leak) {
    CreateDCExW_pfn f = UN(CreateDCExW); REQUIRE(f);
    HDC probe = f(L"DISPLAY", NULL, NULL, NULL, 1);
    if (!probe) { t_skip("no display dc"); return; }
    U(DeleteDC)(probe);
    DWORD before = gap_gdi_count();
    for (int i = 0; i < 300; i++) { HDC dc = f(L"DISPLAY", NULL, NULL, NULL, 1); if (dc) U(DeleteDC)(dc); }
    EXPECT_EQ(gap_gdi_count(), before);
}
TEST(gap0_cdcex_dc_independent) {
    CreateDCExW_pfn f = UN(CreateDCExW); REQUIRE(f);
    HDC a = f(L"DISPLAY", NULL, NULL, NULL, 1), b = f(L"DISPLAY", NULL, NULL, NULL, 1);
    if (!a || !b) { if (a) U(DeleteDC)(a); if (b) U(DeleteDC)(b); t_skip("no display dc"); return; }
    CHECK(a != b);
    U(SetBkMode)(a, TRANSPARENT);
    EXPECT_EQ(U(GetBkMode)(b), OPAQUE);
    U(SetROP2)(a, R2_XORPEN);
    EXPECT_EQ(U(GetROP2)(b), R2_COPYPEN);
    CHECK(U(DeleteDC)(a));
    CHECK(U(DeleteDC)(b));
    SET_ERR(0);
    CHECK(!U(DeleteDC)(a));
}
TEST(gap0_cdcex_delete_twice_fails) {
    CreateDCExW_pfn f = UN(CreateDCExW); REQUIRE(f);
    HDC a = f(L"DISPLAY", NULL, NULL, NULL, 1);
    if (!a) { t_skip("no display dc"); return; }
    CHECK(U(DeleteDC)(a));
    CHECK(!U(DeleteDC)(a));
}
TEST(gap0_cdcex_not_memdc) {
    CreateDCExW_pfn f = UN(CreateDCExW); REQUIRE(f);
    HDC a = f(L"DISPLAY", NULL, NULL, NULL, 1);
    if (!a) { t_skip("no display dc"); return; }
    /* a direct display DC has no bitmap selected: GetCurrentObject(OBJ_BITMAP) yields the stock 1x1 or NULL, never a DIB */
    HGDIOBJ b = U(GetCurrentObject)(a, OBJ_BITMAP);
    if (b) { DIBSECTION ds; EXPECT_EQ(U(GetObjectW)(b, sizeof ds, &ds) <= (int)sizeof(BITMAP), 1); }
    U(DeleteDC)(a);
}

/* ---------------- NamedEscape ---------------- */
TEST(gap0_nesc_resolve) {
    CHECK(t_proc_gdi32("NamedEscape") != NULL);
    CHECK(t_proc_gdi32full("NamedEscape") != NULL);
}
TEST(gap0_nesc_bogus_hdc_error6) {
    NamedEscape_pfn f = UN(NamedEscape); REQUIRE(f);
    ULONG_PTR bogus[] = { 0x1234, 0x7f0000, 0x40000, 0x100000, 0xdeadbeef, 0x00020000, 0x0b0000 };
    for (unsigned i = 0; i < sizeof bogus / sizeof *bogus; i++) {
        SET_ERR(0);
        int r = f((HDC)bogus[i], L"gap0_nodriver", 1, 0, NULL, 0, NULL);
        DWORD e = GetLastError();
        CHECKF(r == 0, "hdc %llx ret %d", (unsigned long long)bogus[i], r);
        CHECKF(e == ERROR_INVALID_HANDLE, "hdc %llx err %lu", (unsigned long long)bogus[i], e);
    }
}
TEST(gap0_nesc_deleted_dc) {
    NamedEscape_pfn f = UN(NamedEscape); REQUIRE(f); REQUIRE(U(CreateCompatibleDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(dc != NULL);
    U(DeleteDC)(dc);
    SET_ERR(0);
    int r = f(dc, L"gap0_nodriver", 1, 0, NULL, 0, NULL);
    t_note("deleted dc: ret %d err %lu", r, GetLastError());
    SOFT(r <= 0, "no success on deleted dc");
}
TEST(gap0_nesc_gdi_object_not_dc) {
    NamedEscape_pfn f = UN(NamedEscape); REQUIRE(f);
    HBRUSH br = U(CreateSolidBrush)(0x112233);
    SET_ERR(0);
    int r = f((HDC)br, L"gap0_nodriver", 1, 0, NULL, 0, NULL);
    CHECK(r == 0);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    U(DeleteObject)(br);
}
TEST(gap0_nesc_null_hdc_unknown_driver) {
    NamedEscape_pfn f = UN(NamedEscape); REQUIRE(f);
    SET_ERR(0);
    int r = f(NULL, L"gap0_nodriver", 1, 0, NULL, 0, NULL);
    CHECK(r <= 0);
    t_note("NULL hdc: ret %d err %lu", r, GetLastError());
}
TEST(gap0_nesc_memdc_unknown_driver) {
    NamedEscape_pfn f = UN(NamedEscape); REQUIRE(f);
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    char in[8] = { 1, 2, 3, 4 }, out[8]; memset(out, 0x5A, sizeof out);
    SET_ERR(0);
    int r = f(m.dc, L"gap0_nodriver", 0x1001, sizeof in, in, sizeof out, out);
    CHECK(r <= 0);
    t_note("memdc: ret %d err %lu", r, GetLastError());
    gap_mdc_close(&m);
}
TEST(gap0_nesc_memdc_empty_name) {
    NamedEscape_pfn f = UN(NamedEscape); REQUIRE(f);
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    int r = f(m.dc, L"", 1, 0, NULL, 0, NULL);
    CHECK(r <= 0);
    gap_mdc_close(&m);
}
TEST(gap0_nesc_memdc_long_name) {
    NamedEscape_pfn f = UN(NamedEscape); REQUIRE(f);
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    WCHAR name[300]; for (int i = 0; i < 299; i++) name[i] = L'a' + (i % 26); name[299] = 0;
    int r = f(m.dc, name, 1, 0, NULL, 0, NULL);
    CHECK(r <= 0);
    gap_mdc_close(&m);
}
TEST(gap0_nesc_no_leak) {
    NamedEscape_pfn f = UN(NamedEscape); REQUIRE(f);
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    DWORD before = gap_gdi_count();
    for (int i = 0; i < 200; i++) f(m.dc, L"gap0_nodriver", i, 0, NULL, 0, NULL);
    EXPECT_EQ(gap_gdi_count(), before);
    gap_mdc_close(&m);
}
TEST(gap0_nesc_does_not_alter_dc) {
    NamedEscape_pfn f = UN(NamedEscape); REQUIRE(f);
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    U(SetBkColor)(m.dc, RGB(1, 2, 3)); U(SetTextColor)(m.dc, RGB(4, 5, 6));
    f(m.dc, L"gap0_nodriver", 5, 0, NULL, 0, NULL);
    EXPECT_EQ(U(GetBkColor)(m.dc), RGB(1, 2, 3));
    EXPECT_EQ(U(GetTextColor)(m.dc), RGB(4, 5, 6));
    gap_mdc_close(&m);
}
TEST(gap0_nesc_escape_codes) {
    NamedEscape_pfn f = UN(NamedEscape); REQUIRE(f);
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    int codes[] = { 0, 1, -1, 0x7fffffff, (int)0x80000000, 0x10000, 0xffff };
    for (unsigned i = 0; i < sizeof codes / sizeof *codes; i++) {
        int r = f(m.dc, L"gap0_nodriver", codes[i], 0, NULL, 0, NULL);
        CHECKF(r <= 0, "code %d returned %d", codes[i], r);
    }
    gap_mdc_close(&m);
}

/* ---------------- GetAspectRatioFilterEx ---------------- */
TEST(gap0_arf_resolve) {
    CHECK(t_proc_gdi32("GetAspectRatioFilterEx") != NULL);
    CHECK(t_proc_gdi32full("GetAspectRatioFilterEx") != NULL);
}
TEST(gap0_arf_null_dc) {
    REQUIRE(U(GetAspectRatioFilterEx));
    SIZE sz = { 111, 222 };
    CHECK(!U(GetAspectRatioFilterEx)(NULL, &sz));
    EXPECT_EQ(sz.cx, 111); EXPECT_EQ(sz.cy, 222);
}
TEST(gap0_arf_bogus_dc) {
    REQUIRE(U(GetAspectRatioFilterEx));
    ULONG_PTR h[] = { 0x1234, 0xdeadbeef, 0x00010000, 0x7fffffff };
    for (unsigned i = 0; i < sizeof h / sizeof *h; i++) {
        SIZE sz = { 7, 8 };
        CHECKF(!U(GetAspectRatioFilterEx)((HDC)h[i], &sz), "h %llx", (unsigned long long)h[i]);
        EXPECT_EQ(sz.cx, 7); EXPECT_EQ(sz.cy, 8);
    }
}
TEST(gap0_arf_deleted_dc) {
    REQUIRE(U(GetAspectRatioFilterEx)); REQUIRE(U(CreateCompatibleDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    U(DeleteDC)(dc);
    SIZE sz = { 3, 4 };
    CHECK(!U(GetAspectRatioFilterEx)(dc, &sz));
    EXPECT_EQ(sz.cx, 3); EXPECT_EQ(sz.cy, 4);
}
TEST(gap0_arf_non_dc_object) {
    REQUIRE(U(GetAspectRatioFilterEx));
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0);
    SIZE sz = { 5, 6 };
    CHECK(!U(GetAspectRatioFilterEx)((HDC)p, &sz));
    EXPECT_EQ(sz.cx, 5);
    U(DeleteObject)(p);
}
TEST(gap0_arf_memdc) {
    REQUIRE(U(GetAspectRatioFilterEx));
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    SIZE sz = { 9, 9 };
    BOOL r = U(GetAspectRatioFilterEx)(m.dc, &sz);
    t_note("memdc: ret %d size %ldx%ld", r, sz.cx, sz.cy);
    if (r) { CHECK(sz.cx >= 0 && sz.cy >= 0); }
    gap_mdc_close(&m);
}
TEST(gap0_arf_screen_dc) {
    REQUIRE(U(GetAspectRatioFilterEx)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC dc = U(GetDC)(NULL);
    if (!dc) { t_skip("no screen dc"); return; }
    SIZE sz = { 9, 9 };
    BOOL r = U(GetAspectRatioFilterEx)(dc, &sz);
    t_note("screen: ret %d size %ldx%ld", r, sz.cx, sz.cy);
    if (r) CHECK(sz.cx >= 0 && sz.cy >= 0);
    U(ReleaseDC)(NULL, dc);
}
TEST(gap0_arf_unaffected_by_mapmode) {
    REQUIRE(U(GetAspectRatioFilterEx));
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    SIZE a = { 0, 0 }, b = { 0, 0 };
    BOOL ra = U(GetAspectRatioFilterEx)(m.dc, &a);
    U(SetMapMode)(m.dc, MM_HIMETRIC);
    BOOL rb = U(GetAspectRatioFilterEx)(m.dc, &b);
    EXPECT_EQ(ra, rb);
    EXPECT_EQ(a.cx, b.cx); EXPECT_EQ(a.cy, b.cy);
    gap_mdc_close(&m);
}
TEST(gap0_arf_repeat) {
    REQUIRE(U(GetAspectRatioFilterEx));
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    SIZE first = { 0, 0 }; BOOL r0 = U(GetAspectRatioFilterEx)(m.dc, &first);
    int bad = 0;
    for (int i = 0; i < 500; i++) { SIZE s = { 0, 0 }; BOOL r = U(GetAspectRatioFilterEx)(m.dc, &s); if (r != r0 || s.cx != first.cx || s.cy != first.cy) bad++; }
    EXPECT_EQ(bad, 0);
    gap_mdc_close(&m);
}

/* ---------------- GetTransform ---------------- */
#define XF_W2P 0x0203
#define XF_P2D 0x0204
static void xf_fill(XFORM *x) { memset(x, 0xCC, sizeof *x); }
static int xf_untouched(const XFORM *x) { const unsigned char *p = (const unsigned char *)x; for (unsigned i = 0; i < sizeof *x; i++) if (p[i] != 0xCC) return 0; return 1; }

TEST(gap0_gettransform_resolve) {
    CHECK(t_proc_gdi32("GetTransform") != NULL);
    CHECK(t_proc_gdi32full("GetTransform") != NULL);
}
TEST(gap0_gettransform_null_dc_untouched) {
    GetTransform_pfn f = UN(GetTransform); REQUIRE(f);
    XFORM x; xf_fill(&x);
    f(NULL, XF_W2P, &x);
    CHECK(xf_untouched(&x));
}
TEST(gap0_gettransform_bogus_dc_untouched) {
    GetTransform_pfn f = UN(GetTransform); REQUIRE(f);
    ULONG_PTR h[] = { 0x1234, 0xdeadbeef, 0x10000, 0x00050001 };
    for (unsigned i = 0; i < sizeof h / sizeof *h; i++) {
        XFORM x; xf_fill(&x);
        f((HDC)h[i], XF_W2P, &x);
        CHECKF(xf_untouched(&x), "h %llx", (unsigned long long)h[i]);
    }
}
TEST(gap0_gettransform_deleted_dc_untouched) {
    GetTransform_pfn f = UN(GetTransform); REQUIRE(f); REQUIRE(U(CreateCompatibleDC));
    HDC dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    XFORM x; xf_fill(&x);
    f(dc, XF_W2P, &x);
    CHECK(xf_untouched(&x));
}
TEST(gap0_gettransform_bad_selector_untouched) {
    GetTransform_pfn f = UN(GetTransform); REQUIRE(f);
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    DWORD sel[] = { 0, 1, 0xffffffff, 0x0100, 0x0999, 0x80000000 };
    for (unsigned i = 0; i < sizeof sel / sizeof *sel; i++) {
        XFORM x; xf_fill(&x);
        f(m.dc, sel[i], &x);
        if (!xf_untouched(&x)) t_note("selector 0x%lx wrote output", sel[i]);
    }
    gap_mdc_close(&m);
}
TEST(gap0_gettransform_identity_default) {
    GetTransform_pfn f = UN(GetTransform); REQUIRE(f);
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    XFORM x; xf_fill(&x);
    f(m.dc, XF_W2P, &x);
    if (xf_untouched(&x)) { t_note("selector 0x203 produced no output"); }
    else {
        SOFT(x.eM11 == 1.0f && x.eM22 == 1.0f && x.eM12 == 0 && x.eM21 == 0 && x.eDx == 0 && x.eDy == 0, "default world->page is identity");
    }
    gap_mdc_close(&m);
}
TEST(gap0_gettransform_follows_setworldtransform) {
    GetTransform_pfn f = UN(GetTransform); REQUIRE(f);
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform)); REQUIRE(U(GetWorldTransform));
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    CHECK(U(SetGraphicsMode)(m.dc, GM_ADVANCED) != 0);
    XFORM set = { 2.0f, 0.5f, -0.5f, 3.0f, 10.0f, -7.0f }, got; xf_fill(&got);
    CHECK(U(SetWorldTransform)(m.dc, &set));
    f(m.dc, XF_W2P, &got);
    if (!xf_untouched(&got)) {
        XFORM wt; CHECK(U(GetWorldTransform)(m.dc, &wt));
        SOFT(!memcmp(&got, &wt, sizeof got), "GetTransform(0x203) equals GetWorldTransform");
    } else t_note("no output for 0x203");
    gap_mdc_close(&m);
}
TEST(gap0_gettransform_page_to_device_mm_text) {
    GetTransform_pfn f = UN(GetTransform); REQUIRE(f);
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    XFORM x; xf_fill(&x);
    f(m.dc, XF_P2D, &x);
    if (!xf_untouched(&x)) SOFT(x.eM11 == 1.0f && x.eM22 == 1.0f && x.eDx == 0 && x.eDy == 0, "MM_TEXT page->device identity");
    gap_mdc_close(&m);
}
TEST(gap0_gettransform_page_to_device_follows_extents) {
    GetTransform_pfn f = UN(GetTransform); REQUIRE(f);
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    U(SetMapMode)(m.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(m.dc, 4, 4, NULL);
    U(SetViewportExtEx)(m.dc, 8, 12, NULL);
    U(SetViewportOrgEx)(m.dc, 5, 6, NULL);
    XFORM x; xf_fill(&x);
    f(m.dc, XF_P2D, &x);
    if (!xf_untouched(&x)) {
        SOFT(x.eM11 == 2.0f && x.eM22 == 3.0f, "scale follows window/viewport extents");
        SOFT(x.eDx == 5.0f && x.eDy == 6.0f, "offset follows viewport origin");
    }
    gap_mdc_close(&m);
}
TEST(gap0_gettransform_does_not_modify_dc) {
    GetTransform_pfn f = UN(GetTransform); REQUIRE(f);
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    U(SetMapMode)(m.dc, MM_HIMETRIC);
    POINT before, after; U(GetViewportOrgEx)(m.dc, &before);
    SIZE e1, e2; U(GetWindowExtEx)(m.dc, &e1);
    XFORM x;
    for (int i = 0; i < 20; i++) { f(m.dc, XF_W2P, &x); f(m.dc, XF_P2D, &x); }
    U(GetViewportOrgEx)(m.dc, &after); U(GetWindowExtEx)(m.dc, &e2);
    EXPECT_EQ(before.x, after.x); EXPECT_EQ(before.y, after.y);
    EXPECT_EQ(e1.cx, e2.cx); EXPECT_EQ(U(GetMapMode)(m.dc), MM_HIMETRIC);
    gap_mdc_close(&m);
}
TEST(gap0_gettransform_screen_dc) {
    GetTransform_pfn f = UN(GetTransform); REQUIRE(f); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC dc = U(GetDC)(NULL); if (!dc) { t_skip("no screen dc"); return; }
    XFORM x; xf_fill(&x);
    f(dc, XF_W2P, &x);
    if (!xf_untouched(&x)) SOFT(x.eM11 == 1.0f && x.eM22 == 1.0f, "screen DC world transform identity");
    U(ReleaseDC)(NULL, dc);
}
TEST(gap0_gettransform_after_restore) {
    GetTransform_pfn f = UN(GetTransform); REQUIRE(f);
    REQUIRE(U(SetGraphicsMode)); REQUIRE(U(ModifyWorldTransform)); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    int s = U(SaveDC)(m.dc);
    XFORM sc = { 4, 0, 0, 4, 0, 0 };
    U(ModifyWorldTransform)(m.dc, &sc, MWT_LEFTMULTIPLY);
    XFORM x; xf_fill(&x); f(m.dc, XF_W2P, &x);
    if (!xf_untouched(&x)) SOFT(x.eM11 == 4.0f, "scaled");
    U(RestoreDC)(m.dc, s);
    xf_fill(&x); f(m.dc, XF_W2P, &x);
    if (!xf_untouched(&x)) SOFT(x.eM11 == 1.0f, "restored to identity");
    gap_mdc_close(&m);
}
