/* CreateDC / CreateIC / CreateCompatibleDC */
#include "dcl.h"

static void del(HDC dc) { if (dc) U(DeleteDC)(dc); }

/* ---------------------------------------------------------------- CreateDC */

TEST(dcl_createdca_display) {
    REQUIRE(U(CreateDCA)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetObjectType)); REQUIRE(U(GetDeviceCaps));
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    CHECK(dc != NULL);
    if (!dc) return;
    EQ(U(GetObjectType)(dc), OBJ_DC);
    EQ(U(GetDeviceCaps)(dc, TECHNOLOGY), DT_RASDISPLAY);
    CHECK(U(DeleteDC)(dc));
}

TEST(dcl_createdcw_display) {
    REQUIRE(U(CreateDCW)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetObjectType)); REQUIRE(U(GetDeviceCaps));
    HDC dc = U(CreateDCW)(L"DISPLAY", NULL, NULL, NULL);
    CHECK(dc != NULL);
    if (!dc) return;
    EQ(U(GetObjectType)(dc), OBJ_DC);
    EQ(U(GetDeviceCaps)(dc, TECHNOLOGY), DT_RASDISPLAY);
    CHECK(U(DeleteDC)(dc));
}

TEST(dcl_createdc_driver_name_case) {
    REQUIRE(U(CreateDCA)); REQUIRE(U(DeleteDC));
    static const char *names[] = { "DISPLAY", "display", "Display", "dIsPlAy" };
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++) {
        HDC dc = U(CreateDCA)(names[i], NULL, NULL, NULL);
        CHECKF(dc != NULL, "driver '%s'", names[i]);
        del(dc);
    }
}

TEST(dcl_createdc_null_args) {
    REQUIRE(U(CreateDCA)); REQUIRE(U(CreateDCW)); REQUIRE(U(DeleteDC));
    HDC dc = U(CreateDCW)(NULL, NULL, NULL, NULL);
    CHECK(dc == NULL); del(dc);
    dc = U(CreateDCA)(NULL, NULL, NULL, NULL);
    CHECK(dc == NULL); del(dc);
}

TEST(dcl_createdc_null_driver_display_device) {
    REQUIRE(U(CreateDCW)); REQUIRE(U(DeleteDC));
    /* The device name "display" is not an adapter name; with no driver this fails on Windows. */
    HDC dc = U(CreateDCW)(NULL, L"display", NULL, NULL);
    DCL_WCHECK(dc == NULL);
    del(dc);
}

TEST(dcl_createdc_empty_driver) {
    REQUIRE(U(CreateDCW)); REQUIRE(U(DeleteDC));
    HDC dc = U(CreateDCW)(L"", NULL, NULL, NULL);
    t_note("CreateDCW(\"\") -> %p", (void *)dc);
    del(dc);
}

TEST(dcl_createdc_bogus_driver) {
    REQUIRE(U(CreateDCA)); REQUIRE(U(CreateDCW)); REQUIRE(U(DeleteDC));
    HDC dc = U(CreateDCA)("NoSuchDriver_dcl_xyz", NULL, NULL, NULL);
    DCL_WCHECK(dc == NULL); del(dc);
    dc = U(CreateDCW)(L"NoSuchDriver_dcl_xyz", L"nodev", NULL, NULL);
    DCL_WCHECK(dc == NULL); del(dc);
    dc = U(CreateDCW)(L"NoSuchDriver_dcl_xyz", L"DISPLAY", NULL, NULL);
    t_note("CreateDCW(bogus driver, \"DISPLAY\") -> %p", (void *)dc); del(dc);
}

TEST(dcl_createdc_display_device_arg_ignored) {
    REQUIRE(U(CreateDCW)); REQUIRE(U(DeleteDC));
    HDC dc = U(CreateDCW)(L"DISPLAY", L"deadbeef", NULL, NULL);
    CHECK(dc != NULL); del(dc);
    dc = U(CreateDCW)(L"display", L"", NULL, NULL);
    CHECK(dc != NULL); del(dc);
}

TEST(dcl_createdc_display_port_arg) {
    REQUIRE(U(CreateDCW)); REQUIRE(U(DeleteDC));
    HDC dc = U(CreateDCW)(L"DISPLAY", NULL, L"LPT1:", NULL);
    t_note("CreateDCW(DISPLAY, port) -> %p", (void *)dc);
    del(dc);
}

TEST(dcl_createdc_adapter_names) {
    REQUIRE(U(CreateDCW)); REQUIRE(U(DeleteDC)); REQUIRE(U(EnumDisplayDevicesW));
    DISPLAY_DEVICEW dd;
    int attached = 0;
    for (DWORD i = 0; i < 16; i++) {
        memset(&dd, 0, sizeof dd); dd.cb = sizeof dd;
        if (!U(EnumDisplayDevicesW)(NULL, i, &dd, 0)) break;
        if (!(dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) continue;
        attached++;
        WCHAR name[32]; lstrcpyW(name, dd.DeviceName);
        HDC dc = U(CreateDCW)(name, NULL, NULL, NULL);
        CHECKF(dc != NULL, "adapter %ls", name); del(dc);
        dc = U(CreateDCW)(NULL, name, NULL, NULL);
        CHECKF(dc != NULL, "device %ls", name); del(dc);
        dc = U(CreateDCW)(name, name, NULL, NULL);
        CHECK(dc != NULL); del(dc);
        dc = U(CreateDCW)(name, L"deadbeef", NULL, NULL);
        CHECK(dc != NULL); del(dc);
        /* monitor-level device names are not valid DC targets */
        for (DWORD j = 0; j < 4; j++) {
            memset(&dd, 0, sizeof dd); dd.cb = sizeof dd;
            if (!U(EnumDisplayDevicesW)(name, j, &dd, 0)) break;
            dc = U(CreateDCW)(dd.DeviceName, NULL, NULL, NULL);
            DCL_WCHECK(dc == NULL); del(dc);
        }
    }
    if (!attached) t_note("no attached display adapter reported");
}

TEST(dcl_createdc_primary_adapter_geometry) {
    REQUIRE(U(CreateDCW)); REQUIRE(U(DeleteDC)); REQUIRE(U(EnumDisplayDevicesW)); REQUIRE(U(GetDeviceCaps));
    REQUIRE(U(GetSystemMetrics));
    DISPLAY_DEVICEW dd;
    for (DWORD i = 0; i < 16; i++) {
        memset(&dd, 0, sizeof dd); dd.cb = sizeof dd;
        if (!U(EnumDisplayDevicesW)(NULL, i, &dd, 0)) break;
        if (!(dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)) continue;
        HDC dc = U(CreateDCW)(dd.DeviceName, NULL, NULL, NULL);
        CHECK(dc != NULL);
        if (!dc) return;
        int hr = U(GetDeviceCaps)(dc, HORZRES), vr = U(GetDeviceCaps)(dc, VERTRES);
        CHECK(hr > 0 && vr > 0);
        if (hr != U(GetSystemMetrics)(SM_CXSCREEN) || vr != U(GetSystemMetrics)(SM_CYSCREEN))
            t_note("HORZRES/VERTRES %dx%d vs SM_C*SCREEN %dx%d (DPI virtualisation?)", hr, vr,
                   U(GetSystemMetrics)(SM_CXSCREEN), U(GetSystemMetrics)(SM_CYSCREEN));
        del(dc);
        return;
    }
    t_note("no primary adapter reported");
}

TEST(dcl_createdc_devmode_current) {
    REQUIRE(U(CreateDCA)); REQUIRE(U(DeleteDC)); REQUIRE(U(EnumDisplaySettingsA));
    DEVMODEA dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    if (!U(EnumDisplaySettingsA)(NULL, ENUM_CURRENT_SETTINGS, &dm)) { t_skip("no current settings"); return; }
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, &dm);
    CHECK(dc != NULL);
    if (dc) {
        EQ(U(GetDeviceCaps)(dc, TECHNOLOGY), DT_RASDISPLAY);
        del(dc);
    }
    dc = U(CreateDCA)("DISPLAY", (const char *)dm.dmDeviceName, NULL, &dm);
    CHECK(dc != NULL); del(dc);
}

TEST(dcl_createdc_devmode_w_current) {
    REQUIRE(U(CreateDCW)); REQUIRE(U(DeleteDC)); REQUIRE(U(EnumDisplaySettingsW));
    DEVMODEW dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    if (!U(EnumDisplaySettingsW)(NULL, ENUM_CURRENT_SETTINGS, &dm)) { t_skip("no current settings"); return; }
    HDC dc = U(CreateDCW)(L"DISPLAY", NULL, NULL, &dm);
    CHECK(dc != NULL); del(dc);
}

TEST(dcl_createdc_devmode_no_fields) {
    REQUIRE(U(CreateDCA)); REQUIRE(U(DeleteDC));
    DEVMODEA dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm; dm.dmFields = 0;
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, &dm);
    t_note("CreateDCA(DISPLAY, dmFields=0) -> %p", (void *)dc);
    del(dc);
}

TEST(dcl_createdc_devmode_bad_size) {
    REQUIRE(U(CreateDCA)); REQUIRE(U(DeleteDC)); REQUIRE(U(EnumDisplaySettingsA));
    union { DEVMODEA dm; char pad[1024]; } u;
    static const WORD sizes[] = { 0, 1, 2, 31, 32, 40, 100, (WORD)(sizeof(DEVMODEA) - 1), (WORD)(sizeof(DEVMODEA) + 8), 0x7fff, 0xffff };
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        memset(&u, 0, sizeof u);
        u.dm.dmSize = sizeof(DEVMODEA);
        U(EnumDisplaySettingsA)(NULL, ENUM_CURRENT_SETTINGS, &u.dm);
        u.dm.dmSize = sizes[i];
        u.dm.dmDriverExtra = 0;
        HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, &u.dm);
        t_note("dmSize=%u -> %p", sizes[i], (void *)dc);
        del(dc);
    }
}

TEST(dcl_createdc_devmode_bad_fields) {
    REQUIRE(U(CreateDCA)); REQUIRE(U(DeleteDC)); REQUIRE(U(EnumDisplaySettingsA));
    static const DWORD fields[] = { DM_ORIENTATION, DM_PAPERSIZE, DM_COPIES, DM_COLOR, DM_DUPLEX, 0xffffffff, 0x80000000 };
    DEVMODEA dm;
    for (unsigned i = 0; i < sizeof fields / sizeof *fields; i++) {
        memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
        U(EnumDisplaySettingsA)(NULL, ENUM_CURRENT_SETTINGS, &dm);
        dm.dmFields = fields[i];
        dm.dmOrientation = 77; dm.dmPaperSize = 999; dm.dmCopies = 30000;
        HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, &dm);
        t_note("dmFields=0x%lx -> %p", fields[i], (void *)dc);
        if (dc) {
            CHECK(U(GetObjectType)(dc) == OBJ_DC);
            CHECK(U(GetDeviceCaps)(dc, TECHNOLOGY) == DT_RASDISPLAY);
        }
        del(dc);
    }
}

TEST(dcl_createdc_devmode_dmscale) {
    REQUIRE(U(CreateDCA)); REQUIRE(U(DeleteDC)); REQUIRE(U(EnumDisplaySettingsA)); REQUIRE(U(GetDeviceCaps));
    DEVMODEA dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    if (!U(EnumDisplaySettingsA)(NULL, ENUM_CURRENT_SETTINGS, &dm)) { t_skip("no settings"); return; }
    dm.dmScale = 200; dm.dmFields |= DM_SCALE;
    HDC a = U(CreateDCA)("DISPLAY", NULL, NULL, &dm);
    HDC b = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    CHECK(a && b);
    if (a && b) {
        /* A display DC ignores dmScale: all caps match the plain DC. */
        static const int idx[] = { HORZRES, VERTRES, BITSPIXEL, PLANES, LOGPIXELSX, LOGPIXELSY, HORZSIZE, VERTSIZE, RASTERCAPS, NUMCOLORS };
        for (unsigned i = 0; i < sizeof idx / sizeof *idx; i++)
            CHECKF(U(GetDeviceCaps)(a, idx[i]) == U(GetDeviceCaps)(b, idx[i]), "cap %d", idx[i]);
    }
    del(a); del(b);
}

TEST(dcl_createdc_handles_unique) {
    REQUIRE(U(CreateDCA)); REQUIRE(U(DeleteDC));
    HDC dcs[40]; int n = 0;
    for (int i = 0; i < 40; i++) { dcs[i] = U(CreateDCA)("DISPLAY", NULL, NULL, NULL); if (dcs[i]) n++; }
    CHECK(n == 40);
    for (int i = 0; i < 40; i++)
        for (int j = i + 1; j < 40; j++)
            if (dcs[i] && dcs[j]) CHECK(dcs[i] != dcs[j]);
    for (int i = 0; i < 40; i++) del(dcs[i]);
}

TEST(dcl_createdc_gdi_object_count) {
    REQUIRE(U(CreateDCA)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetGuiResources));
    DWORD before = dcl_gdicount();
    HDC dcs[20];
    for (int i = 0; i < 20; i++) dcs[i] = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    DWORD mid = dcl_gdicount();
    CHECKF(!before || mid >= before + 20, "before=%lu mid=%lu", before, mid);
    for (int i = 0; i < 20; i++) del(dcs[i]);
    DWORD after = dcl_gdicount();
    CHECKF(after == before, "before=%lu after=%lu", before, after);
}

TEST(dcl_createdc_handle_layout) {
    REQUIRE(U(CreateDCA)); REQUIRE(U(DeleteDC)); REQUIRE(U(CreateCompatibleDC));
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    HDC mem = U(CreateCompatibleDC)(NULL);
    HDC list[2] = { dc, mem };
    for (int i = 0; i < 2; i++) {
        ULONG_PTR h = (ULONG_PTR)list[i];
        if (!h) { CHECK(h != 0); continue; }
        ULONG_PTR hi = h >> 32;
        CHECKF(hi == 0 || hi == 0xffffffff, "handle %p not a sign-extended 32-bit value", list[i]);
        DCL_WCHECK((h & 0x7f0000) == 0x10000);
        CHECKF((h & 0xffff) != 0, "index of %p", list[i]);
    }
    del(dc); del(mem);
}

TEST(dcl_createdc_default_attributes) {
    REQUIRE(U(CreateDCA)); REQUIRE(U(DeleteDC));
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    if (!dc) { CHECK(dc != NULL); return; }
    EQ(U(GetBkColor)(dc), RGB(255, 255, 255));
    EQ(U(GetTextColor)(dc), RGB(0, 0, 0));
    EQ(U(GetBkMode)(dc), OPAQUE);
    EQ(U(GetROP2)(dc), R2_COPYPEN);
    EQ(U(GetMapMode)(dc), MM_TEXT);
    EQ(U(GetPolyFillMode)(dc), ALTERNATE);
    EQ(U(GetStretchBltMode)(dc), BLACKONWHITE);
    EQ(U(GetArcDirection)(dc), AD_COUNTERCLOCKWISE);
    EQ(U(GetTextAlign)(dc), TA_LEFT | TA_TOP | TA_NOUPDATECP);
    EQ(U(GetGraphicsMode)(dc), GM_COMPATIBLE);
    EQ(U(GetTextCharacterExtra)(dc), 0);
    EQ(U(GetCurrentObject)(dc, OBJ_PEN), U(GetStockObject)(BLACK_PEN));
    EQ(U(GetCurrentObject)(dc, OBJ_BRUSH), U(GetStockObject)(WHITE_BRUSH));
    POINT p = { 77, 77 };
    CHECK(U(GetCurrentPositionEx)(dc, &p));
    CHECK(p.x == 0 && p.y == 0);
    p.x = p.y = 77;
    CHECK(U(GetBrushOrgEx)(dc, &p));
    CHECK(p.x == 0 && p.y == 0);
    p.x = p.y = 77;
    CHECK(U(GetViewportOrgEx)(dc, &p));
    CHECK(p.x == 0 && p.y == 0);
    SIZE s = { 77, 77 };
    CHECK(U(GetViewportExtEx)(dc, &s));
    CHECK(s.cx == 1 && s.cy == 1);
    s.cx = s.cy = 77;
    CHECK(U(GetWindowExtEx)(dc, &s));
    CHECK(s.cx == 1 && s.cy == 1);
    del(dc);
}

TEST(dcl_createdc_screen_vs_getdc_caps) {
    REQUIRE(U(CreateDCA)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC)); REQUIRE(U(GetDeviceCaps));
    HDC a = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    HDC b = dcl_screen();
    CHECK(a && b);
    if (a && b) {
        for (int i = 0; i <= 121; i++) {
            if (i == VREFRESH) continue;
            CHECKF(U(GetDeviceCaps)(a, i) == U(GetDeviceCaps)(b, i), "index %d: %d vs %d", i, U(GetDeviceCaps)(a, i), U(GetDeviceCaps)(b, i));
        }
    }
    del(a);
    if (b) dcl_release_screen(b);
}

/* ---------------------------------------------------------------- CreateIC */

TEST(dcl_createica_display) {
    REQUIRE(U(CreateICA)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetObjectType)); REQUIRE(U(GetDeviceCaps));
    HDC ic = U(CreateICA)("DISPLAY", NULL, NULL, NULL);
    CHECK(ic != NULL);
    if (!ic) return;
    EQ(U(GetObjectType)(ic), OBJ_DC);
    EQ(U(GetDeviceCaps)(ic, TECHNOLOGY), DT_RASDISPLAY);
    CHECK(U(DeleteDC)(ic));
}

TEST(dcl_createicw_display) {
    REQUIRE(U(CreateICW)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetDeviceCaps));
    HDC ic = U(CreateICW)(L"DISPLAY", NULL, NULL, NULL);
    CHECK(ic != NULL);
    if (!ic) return;
    EQ(U(GetDeviceCaps)(ic, TECHNOLOGY), DT_RASDISPLAY);
    CHECK(U(DeleteDC)(ic));
}

TEST(dcl_createic_null_args) {
    REQUIRE(U(CreateICA)); REQUIRE(U(CreateICW)); REQUIRE(U(DeleteDC));
    HDC ic = U(CreateICW)(NULL, NULL, NULL, NULL);
    CHECK(ic == NULL); del(ic);
    ic = U(CreateICA)(NULL, NULL, NULL, NULL);
    CHECK(ic == NULL); del(ic);
}

TEST(dcl_createic_bogus_driver) {
    REQUIRE(U(CreateICA)); REQUIRE(U(DeleteDC));
    HDC ic = U(CreateICA)("NoSuchDriver_dcl_xyz", NULL, NULL, NULL);
    CHECK(ic == NULL); del(ic);
}

TEST(dcl_createic_matches_dc_caps) {
    REQUIRE(U(CreateICA)); REQUIRE(U(CreateDCA)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetDeviceCaps));
    HDC ic = U(CreateICA)("DISPLAY", NULL, NULL, NULL);
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    CHECK(ic && dc);
    if (ic && dc)
        for (int i = 0; i <= 121; i++) {
            if (i == VREFRESH) continue;
            CHECKF(U(GetDeviceCaps)(ic, i) == U(GetDeviceCaps)(dc, i), "index %d", i);
        }
    del(ic); del(dc);
}

TEST(dcl_createic_devmode) {
    REQUIRE(U(CreateICA)); REQUIRE(U(DeleteDC)); REQUIRE(U(EnumDisplaySettingsA));
    DEVMODEA dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    if (!U(EnumDisplaySettingsA)(NULL, ENUM_CURRENT_SETTINGS, &dm)) { t_skip("no settings"); return; }
    HDC ic = U(CreateICA)("DISPLAY", NULL, NULL, &dm);
    CHECK(ic != NULL); del(ic);
}

TEST(dcl_createic_default_attributes) {
    REQUIRE(U(CreateICA)); REQUIRE(U(DeleteDC));
    HDC ic = U(CreateICA)("DISPLAY", NULL, NULL, NULL);
    if (!ic) { CHECK(ic != NULL); return; }
    EQ(U(GetBkMode)(ic), OPAQUE);
    EQ(U(GetMapMode)(ic), MM_TEXT);
    EQ(U(GetROP2)(ic), R2_COPYPEN);
    EQ(U(SetBkMode)(ic, TRANSPARENT), OPAQUE);
    EQ(U(GetBkMode)(ic), TRANSPARENT);
    del(ic);
}

TEST(dcl_createic_gdi_count) {
    REQUIRE(U(CreateICA)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetGuiResources));
    DWORD before = dcl_gdicount();
    HDC ics[10];
    for (int i = 0; i < 10; i++) ics[i] = U(CreateICA)("DISPLAY", NULL, NULL, NULL);
    for (int i = 0; i < 10; i++) del(ics[i]);
    EQ(dcl_gdicount(), before);
}

/* ------------------------------------------------------- CreateCompatibleDC */

TEST(dcl_compat_null) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetObjectType)); REQUIRE(U(GetDeviceCaps));
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(dc != NULL);
    if (!dc) return;
    EQ(U(GetObjectType)(dc), OBJ_MEMDC);
    EQ(U(GetDeviceCaps)(dc, TECHNOLOGY), DT_RASDISPLAY);
    CHECK(U(DeleteDC)(dc));
}

TEST(dcl_compat_from_screen_dc) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC scr = dcl_screen();
    CHECK(scr != NULL);
    HDC dc = U(CreateCompatibleDC)(scr);
    CHECK(dc != NULL);
    if (dc) EQ(U(GetObjectType)(dc), OBJ_MEMDC);
    del(dc);
    if (scr) dcl_release_screen(scr);
}

TEST(dcl_compat_from_createdc) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(CreateDCA)); REQUIRE(U(DeleteDC));
    HDC d = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    HDC dc = U(CreateCompatibleDC)(d);
    CHECK(dc != NULL);
    if (dc) EQ(U(GetObjectType)(dc), OBJ_MEMDC);
    del(dc); del(d);
}

TEST(dcl_compat_from_ic) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(CreateICA)); REQUIRE(U(DeleteDC));
    HDC d = U(CreateICA)("DISPLAY", NULL, NULL, NULL);
    HDC dc = U(CreateCompatibleDC)(d);
    CHECK(dc != NULL);
    if (dc) EQ(U(GetObjectType)(dc), OBJ_MEMDC);
    del(dc); del(d);
}

TEST(dcl_compat_from_window_dc) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    REQUIRE(U(GetWindowDC)); REQUIRE(U(DestroyWindow)); REQUIRE(U(CreateWindowExA));
    HWND hw = dcl_window(10, 10, 100, 80);
    if (!hw) { t_skip("no window"); return; }
    HDC wdc = U(GetDC)(hw);
    HDC dc = U(CreateCompatibleDC)(wdc);
    CHECK(dc != NULL);
    if (dc) EQ(U(GetObjectType)(dc), OBJ_MEMDC);
    del(dc);
    U(ReleaseDC)(hw, wdc);
    wdc = U(GetWindowDC)(hw);
    dc = U(CreateCompatibleDC)(wdc);
    CHECK(dc != NULL); del(dc);
    U(ReleaseDC)(hw, wdc);
    U(DestroyWindow)(hw);
}

TEST(dcl_compat_from_memdc) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetObjectType));
    HDC a = U(CreateCompatibleDC)(NULL);
    HDC b = U(CreateCompatibleDC)(a);
    HDC c = U(CreateCompatibleDC)(b);
    CHECK(a && b && c);
    CHECK(a != b && b != c && a != c);
    if (b) EQ(U(GetObjectType)(b), OBJ_MEMDC);
    if (c) EQ(U(GetObjectType)(c), OBJ_MEMDC);
    /* deleting the source does not affect the derived DC */
    del(a);
    if (b) EQ(U(GetObjectType)(b), OBJ_MEMDC);
    del(b);
    if (c) EQ(U(GetObjectType)(c), OBJ_MEMDC);
    del(c);
}

TEST(dcl_compat_from_deleted_dc) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC a = U(CreateCompatibleDC)(NULL);
    CHECK(U(DeleteDC)(a));
    HDC b = U(CreateCompatibleDC)(a);
    CHECK(b == NULL); del(b);
}

TEST(dcl_compat_from_bogus_handle) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    static const ULONG_PTR h[] = { 0xdeadbeef, 0x1, 0xffffffff, 0x7fffffff, 0x10001 };
    for (unsigned i = 0; i < sizeof h / sizeof *h; i++) {
        HDC b = U(CreateCompatibleDC)((HDC)h[i]);
        CHECKF(b == NULL, "handle 0x%llx", (unsigned long long)h[i]);
        del(b);
    }
}

TEST(dcl_compat_from_non_dc_objects) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(CreatePen));
    REQUIRE(U(CreateBitmap)); REQUIRE(U(DeleteObject));
    HGDIOBJ objs[3];
    objs[0] = U(CreateSolidBrush)(RGB(1, 2, 3));
    objs[1] = U(CreatePen)(PS_SOLID, 1, 0);
    objs[2] = U(CreateBitmap)(4, 4, 1, 1, NULL);
    for (int i = 0; i < 3; i++) {
        HDC b = U(CreateCompatibleDC)((HDC)objs[i]);
        t_note("CreateCompatibleDC(non-DC object %d) -> %p", i, (void *)b);
        del(b);
        U(DeleteObject)(objs[i]);
    }
}

TEST(dcl_compat_default_bitmap) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetCurrentObject)); REQUIRE(U(GetObjectW));
    HDC dc = dcl_mem();
    if (!dc) { CHECK(dc != NULL); return; }
    HGDIOBJ bm = U(GetCurrentObject)(dc, OBJ_BITMAP);
    CHECK(bm != NULL);
    if (bm) {
        EQ(U(GetObjectType)(bm), OBJ_BITMAP);
        BITMAP b; memset(&b, 0xcc, sizeof b);
        EQ(U(GetObjectW)(bm, sizeof b, &b), sizeof(BITMAP));
        CHECK(b.bmWidth == 1 && b.bmHeight == 1);
        CHECK(b.bmPlanes == 1 && b.bmBitsPixel == 1);
        EQ(b.bmType, 0);
    }
    del(dc);
}

TEST(dcl_compat_default_bitmap_is_shared_stock) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetCurrentObject));
    HDC a = dcl_mem(), b = dcl_mem();
    if (a && b) {
        HGDIOBJ ba = U(GetCurrentObject)(a, OBJ_BITMAP), bb = U(GetCurrentObject)(b, OBJ_BITMAP);
        CHECK(ba != NULL && bb != NULL);
        /* the 1x1 default bitmap is the stock bitmap: identical handle for every memory DC */
        if (ba != bb) t_note("default bitmaps differ between memory DCs (%p / %p)", ba, bb);
    }
    del(a); del(b);
}

TEST(dcl_compat_default_attributes) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC dc = dcl_mem();
    if (!dc) { CHECK(dc != NULL); return; }
    EQ(U(GetBkColor)(dc), RGB(255, 255, 255));
    EQ(U(GetTextColor)(dc), RGB(0, 0, 0));
    EQ(U(GetBkMode)(dc), OPAQUE);
    EQ(U(GetROP2)(dc), R2_COPYPEN);
    EQ(U(GetMapMode)(dc), MM_TEXT);
    EQ(U(GetPolyFillMode)(dc), ALTERNATE);
    EQ(U(GetStretchBltMode)(dc), BLACKONWHITE);
    EQ(U(GetArcDirection)(dc), AD_COUNTERCLOCKWISE);
    EQ(U(GetTextAlign)(dc), TA_LEFT | TA_TOP | TA_NOUPDATECP);
    EQ(U(GetGraphicsMode)(dc), GM_COMPATIBLE);
    del(dc);
}

TEST(dcl_compat_default_objects) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetCurrentObject)); REQUIRE(U(GetStockObject));
    HDC dc = dcl_mem();
    if (!dc) { CHECK(dc != NULL); return; }
    EQ(U(GetCurrentObject)(dc, OBJ_PEN), U(GetStockObject)(BLACK_PEN));
    EQ(U(GetCurrentObject)(dc, OBJ_BRUSH), U(GetStockObject)(WHITE_BRUSH));
    HGDIOBJ f = U(GetCurrentObject)(dc, OBJ_FONT);
    CHECK(f != NULL);
    if (f != U(GetStockObject)(SYSTEM_FONT)) t_note("default font %p is not stock SYSTEM_FONT %p", f, U(GetStockObject)(SYSTEM_FONT));
    HGDIOBJ p = U(GetCurrentObject)(dc, OBJ_PAL);
    CHECK(p != NULL);
    if (p != U(GetStockObject)(DEFAULT_PALETTE)) t_note("default palette %p is not DEFAULT_PALETTE", p);
    del(dc);
}

TEST(dcl_compat_does_not_inherit_state) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC a = dcl_mem();
    if (!a) { CHECK(a != NULL); return; }
    U(SetBkColor)(a, RGB(1, 2, 3));
    U(SetTextColor)(a, RGB(4, 5, 6));
    U(SetBkMode)(a, TRANSPARENT);
    U(SetROP2)(a, R2_XORPEN);
    U(SetMapMode)(a, MM_LOENGLISH);
    U(SetPolyFillMode)(a, WINDING);
    HDC b = U(CreateCompatibleDC)(a);
    CHECK(b != NULL);
    if (b) {
        EQ(U(GetBkColor)(b), RGB(255, 255, 255));
        EQ(U(GetTextColor)(b), RGB(0, 0, 0));
        EQ(U(GetBkMode)(b), OPAQUE);
        EQ(U(GetROP2)(b), R2_COPYPEN);
        EQ(U(GetMapMode)(b), MM_TEXT);
        EQ(U(GetPolyFillMode)(b), ALTERNATE);
    }
    del(b); del(a);
}

TEST(dcl_compat_state_independent) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC a = dcl_mem(), b = dcl_mem();
    if (!a || !b) { CHECK(a && b); del(a); del(b); return; }
    U(SetBkColor)(a, RGB(10, 20, 30));
    EQ(U(GetBkColor)(b), RGB(255, 255, 255));
    U(SetBkColor)(b, RGB(40, 50, 60));
    EQ(U(GetBkColor)(a), RGB(10, 20, 30));
    del(a); del(b);
}

TEST(dcl_compat_many) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetGuiResources));
    DWORD before = dcl_gdicount();
    HDC dcs[100];
    int n = 0;
    for (int i = 0; i < 100; i++) { dcs[i] = U(CreateCompatibleDC)(NULL); if (dcs[i]) n++; }
    CHECK(n == 100);
    DWORD mid = dcl_gdicount();
    CHECKF(!before || mid >= before + 100, "before=%lu mid=%lu", before, mid);
    for (int i = 0; i < 100; i++) del(dcs[i]);
    EQ(dcl_gdicount(), before);
}

TEST(dcl_compat_create_delete_loop) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetGuiResources));
    DWORD before = dcl_gdicount();
    int ok = 0;
    for (int i = 0; i < 2000; i++) {
        HDC dc = U(CreateCompatibleDC)(NULL);
        if (dc && U(DeleteDC)(dc)) ok++;
    }
    CHECK(ok == 2000);
    EQ(dcl_gdicount(), before);
}

TEST(dcl_compat_select_dib_depths) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(CreateDIBSection)); REQUIRE(U(SelectObject));
    REQUIRE(U(GetClipBox)); REQUIRE(U(GetObjectW));
    static const int depths[] = { 1, 4, 8, 16, 24, 32 };
    for (unsigned i = 0; i < sizeof depths / sizeof *depths; i++) {
        HDC dc = dcl_mem();
        HBITMAP dib = dcl_dib(dc, 24, 12, depths[i], NULL);
        CHECKF(dib != NULL, "depth %d", depths[i]);
        if (!dib) { del(dc); continue; }
        HGDIOBJ old = U(SelectObject)(dc, dib);
        CHECKF(old != NULL, "depth %d", depths[i]);
        EQ(U(GetCurrentObject)(dc, OBJ_BITMAP), dib);
        RECT r = { -1, -1, -1, -1 };
        int t = U(GetClipBox)(dc, &r);
        CHECKF(t == SIMPLEREGION, "depth %d clipbox type %d", depths[i], t);
        CHECKF(r.left == 0 && r.top == 0 && r.right == 24 && r.bottom == 12, "depth %d clipbox", depths[i]);
        BITMAP bm; memset(&bm, 0, sizeof bm);
        CHECK(U(GetObjectW)(dib, sizeof bm, &bm) == sizeof bm);
        CHECKF(bm.bmBitsPixel == depths[i], "depth %d bmBitsPixel %d", depths[i], bm.bmBitsPixel);
        CHECKF(bm.bmWidth == 24 && bm.bmHeight == 12, "depth %d dims", depths[i]);
        U(SelectObject)(dc, old);
        del(dc);
        /* DeleteDC with a DIB selected must leave the DIB valid */
        EQ(U(GetObjectType)(dib), OBJ_BITMAP);
        CHECK(U(DeleteObject)(dib));
    }
}

TEST(dcl_compat_select_mono_ddb_caps) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(CreateBitmap)); REQUIRE(U(SelectObject));
    HDC dc = dcl_mem();
    HBITMAP bm = U(CreateBitmap)(32, 32, 1, 1, NULL);
    HGDIOBJ old = U(SelectObject)(dc, bm);
    CHECK(old != NULL);
    DCL_WCHECK(U(GetDeviceCaps)(dc, BITSPIXEL) == 1);
    DCL_WCHECK(U(GetDeviceCaps)(dc, PLANES) == 1);
    EQ(U(GetDeviceCaps)(dc, TECHNOLOGY), DT_RASDISPLAY);
    U(SelectObject)(dc, old);
    del(dc);
    U(DeleteObject)(bm);
}

TEST(dcl_compat_select_compatible_bitmap_caps) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(CreateCompatibleBitmap)); REQUIRE(U(SelectObject));
    HDC scr = dcl_screen();
    HDC dc = U(CreateCompatibleDC)(scr);
    HBITMAP bm = U(CreateCompatibleBitmap)(scr, 40, 30);
    CHECK(bm != NULL);
    HGDIOBJ old = U(SelectObject)(dc, bm);
    CHECK(old != NULL);
    EQ(U(GetDeviceCaps)(dc, BITSPIXEL), U(GetDeviceCaps)(scr, BITSPIXEL));
    EQ(U(GetDeviceCaps)(dc, PLANES), U(GetDeviceCaps)(scr, PLANES));
    RECT r; CHECK(U(GetClipBox)(dc, &r) == SIMPLEREGION);
    CHECK(r.left == 0 && r.top == 0 && r.right == 40 && r.bottom == 30);
    U(SelectObject)(dc, old);
    del(dc);
    if (bm) U(DeleteObject)(bm);
    dcl_release_screen(scr);
}

TEST(dcl_compat_ddb_other_depths) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(CreateBitmap)); REQUIRE(U(SelectObject));
    HDC scr = dcl_screen();
    int bpp = U(GetDeviceCaps)(scr, BITSPIXEL);
    dcl_release_screen(scr);
    HDC dc = dcl_mem();
    static const int depths[] = { 2, 4, 8, 16, 24, 32 };
    for (unsigned i = 0; i < sizeof depths / sizeof *depths; i++) {
        HBITMAP bm = U(CreateBitmap)(8, 8, 1, depths[i], NULL);
        CHECKF(bm != NULL, "depth %d", depths[i]);
        if (!bm) continue;
        HGDIOBJ old = U(SelectObject)(dc, bm);
        if (depths[i] == bpp) CHECKF(old != NULL, "selecting a display-depth DDB (%d)", bpp);
        else t_note("DDB depth %d on %d bpp display: SelectObject -> %p", depths[i], bpp, old);
        if (old) U(SelectObject)(dc, old);
        U(DeleteObject)(bm);
    }
    del(dc);
}
