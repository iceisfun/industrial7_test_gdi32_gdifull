#include "ep.h"

/* DPI awareness helpers (all through user32 exports; absent on very old systems) */
typedef DPI_AWARENESS_CONTEXT (WINAPI *setdpi_pfn)(DPI_AWARENESS_CONTEXT);
typedef DPI_AWARENESS_CONTEXT (WINAPI *getdpi_pfn)(void);
typedef UINT (WINAPI *getdpisys_pfn)(void);
typedef UINT (WINAPI *getdpiwin_pfn)(HWND);
typedef BOOL (WINAPI *eqdpi_pfn)(DPI_AWARENESS_CONTEXT, DPI_AWARENESS_CONTEXT);
#define CTX_UNAWARE   ((DPI_AWARENESS_CONTEXT)-1)
#define CTX_SYSTEM    ((DPI_AWARENESS_CONTEXT)-2)
#define CTX_PMONITOR  ((DPI_AWARENESS_CONTEXT)-3)
#define CTX_PMONITOR2 ((DPI_AWARENESS_CONTEXT)-4)
static setdpi_pfn p_setdpi;
static DPI_AWARENESS_CONTEXT dpi_push(DPI_AWARENESS_CONTEXT c) {
    if (!p_setdpi) p_setdpi = (setdpi_pfn)t_proc("SetThreadDpiAwarenessContext");
    return p_setdpi ? p_setdpi(c) : NULL;
}
static void dpi_pop(DPI_AWARENESS_CONTEXT old) { if (old && p_setdpi) p_setdpi(old); }

/* ---- CreateDC / CreateIC ---- */
TEST(ep_dc_createdca_display) {
    REQUIRE(U(CreateDCA));
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    CHECK(dc != NULL);
    if (dc) { CHECK(U(DeleteDC)(dc)); }
}
TEST(ep_dc_createdcw_display) {
    REQUIRE(U(CreateDCW));
    HDC dc = U(CreateDCW)(L"DISPLAY", NULL, NULL, NULL);
    CHECK(dc != NULL);
    if (dc) { CHECK(U(DeleteDC)(dc)); }
}
TEST(ep_dc_createdca_display_lowercase) {
    REQUIRE(U(CreateDCA));
    HDC dc = U(CreateDCA)("display", NULL, NULL, NULL);
    CHECK(dc != NULL);
    if (dc) U(DeleteDC)(dc);
}
TEST(ep_dc_createdc_object_type) {
    REQUIRE(U(CreateDCA)); REQUIRE(U(GetObjectType));
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    CHECK(U(GetObjectType)(dc) == OBJ_DC);
    U(DeleteDC)(dc);
}
TEST(ep_dc_createic_object_type_note) {
    REQUIRE(U(CreateICA)); REQUIRE(U(GetObjectType));
    HDC dc = U(CreateICA)("DISPLAY", NULL, NULL, NULL);
    CHECK(dc != NULL);
    t_note("GetObjectType(IC)=%lu (OBJ_DC=%d)", U(GetObjectType)(dc), OBJ_DC);
    U(DeleteDC)(dc);
}
TEST(ep_dc_createica_display) { REQUIRE(U(CreateICA)); HDC dc = U(CreateICA)("DISPLAY", NULL, NULL, NULL); CHECK(dc != NULL); if (dc) CHECK(U(DeleteDC)(dc)); }
TEST(ep_dc_createicw_display) { REQUIRE(U(CreateICW)); HDC dc = U(CreateICW)(L"DISPLAY", NULL, NULL, NULL); CHECK(dc != NULL); if (dc) CHECK(U(DeleteDC)(dc)); }
TEST(ep_dc_createdc_distinct_handles) {
    REQUIRE(U(CreateDCA));
    HDC a = U(CreateDCA)("DISPLAY", NULL, NULL, NULL), b = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    CHECK(a && b && a != b);
    if (a) U(DeleteDC)(a);
    if (b) U(DeleteDC)(b);
}
TEST(ep_dc_createdc_all_null_note) {
    REQUIRE(U(CreateDCA));
    SET_ERR(0);
    HDC dc = U(CreateDCA)(NULL, NULL, NULL, NULL);
    t_note("CreateDCA(NULL x4) -> %p err %lu", dc, GetLastError());
    if (dc) U(DeleteDC)(dc);
}
TEST(ep_dc_createdc_bad_driver) {
    REQUIRE(U(CreateDCA));
    SET_ERR(0);
    HDC dc = U(CreateDCA)("NoSuchDriver_gdi32_corpus", NULL, NULL, NULL);
    CHECK(dc == NULL);
    t_note("bad driver err %lu", GetLastError());
    if (dc) U(DeleteDC)(dc);
}
TEST(ep_dc_createdcw_bad_driver) {
    REQUIRE(U(CreateDCW));
    HDC dc = U(CreateDCW)(L"NoSuchDriver_gdi32_corpus", NULL, NULL, NULL);
    CHECK(dc == NULL);
    if (dc) U(DeleteDC)(dc);
}
TEST(ep_dc_createdc_bad_driver_with_device) {
    REQUIRE(U(CreateDCA));
    HDC dc = U(CreateDCA)("NoSuchDriver_gdi32_corpus", "NoSuchDevice", NULL, NULL);
    CHECK(dc == NULL);
    if (dc) U(DeleteDC)(dc);
}
TEST(ep_dc_createic_bad_driver) {
    REQUIRE(U(CreateICA));
    HDC dc = U(CreateICA)("NoSuchDriver_gdi32_corpus", NULL, NULL, NULL);
    CHECK(dc == NULL);
    if (dc) U(DeleteDC)(dc);
}
TEST(ep_dc_createdc_printer_nonexistent_a) {
    REQUIRE(U(CreateDCA));
    SET_ERR(0);
    HDC dc = U(CreateDCA)("WINSPOOL", "No Such Printer gdi32 corpus", NULL, NULL);
    CHECK(dc == NULL);
    t_note("nonexistent printer err %lu", GetLastError());
    if (dc) U(DeleteDC)(dc);
}
TEST(ep_dc_createdc_printer_nonexistent_w) {
    REQUIRE(U(CreateDCW));
    HDC dc = U(CreateDCW)(L"WINSPOOL", L"No Such Printer gdi32 corpus", NULL, NULL);
    CHECK(dc == NULL);
    if (dc) U(DeleteDC)(dc);
}
TEST(ep_dc_createdc_printer_nonexistent_null_driver) {
    REQUIRE(U(CreateDCA));
    HDC dc = U(CreateDCA)(NULL, "No Such Printer gdi32 corpus", NULL, NULL);
    CHECK(dc == NULL);
    if (dc) U(DeleteDC)(dc);
}
TEST(ep_dc_createdc_printer_long_name) {
    REQUIRE(U(CreateDCA));
    char name[600]; memset(name, 'P', 599); name[599] = 0;
    HDC dc = U(CreateDCA)("WINSPOOL", name, NULL, NULL);
    CHECK(dc == NULL);
    if (dc) U(DeleteDC)(dc);
}
TEST(ep_dc_createdc_printer_empty_device) {
    REQUIRE(U(CreateDCA));
    SET_ERR(0);
    HDC dc = U(CreateDCA)("WINSPOOL", "", NULL, NULL);
    t_note("WINSPOOL empty device -> %p err %lu", dc, GetLastError());
    if (dc) U(DeleteDC)(dc);
}
TEST(ep_dc_createic_printer_nonexistent) {
    REQUIRE(U(CreateICA));
    HDC dc = U(CreateICA)("WINSPOOL", "No Such Printer gdi32 corpus", NULL, NULL);
    CHECK(dc == NULL);
    if (dc) U(DeleteDC)(dc);
}
TEST(ep_dc_createic_printer_nonexistent_w) {
    REQUIRE(U(CreateICW));
    HDC dc = U(CreateICW)(L"WINSPOOL", L"No Such Printer gdi32 corpus", NULL, NULL);
    CHECK(dc == NULL);
    if (dc) U(DeleteDC)(dc);
}
TEST(ep_dc_createdc_gdi_balance) {
    REQUIRE(U(CreateDCA)); REQUIRE(U(GetGuiResources));
    DWORD before = ep_gui();
    for (int i = 0; i < 50; i++) {
        HDC a = U(CreateDCA)("DISPLAY", NULL, NULL, NULL); HDC b = U(CreateICA)("DISPLAY", NULL, NULL, NULL);
        HDC c = U(CreateDCA)("WINSPOOL", "No Such Printer gdi32 corpus", NULL, NULL);
        if (a) U(DeleteDC)(a);
        if (b) U(DeleteDC)(b);
        if (c) U(DeleteDC)(c);
    }
    CHECK(ep_gui() == before);
}
TEST(ep_dc_ic_text_metrics) {
    REQUIRE(U(CreateICA)); REQUIRE(U(GetTextMetricsW));
    HDC ic = U(CreateICA)("DISPLAY", NULL, NULL, NULL);
    CHECK(ic != NULL);
    if (ic) {
        TEXTMETRICW tm; memset(&tm, 0, sizeof tm);
        HGDIOBJ old = U(SelectObject)(ic, U(GetStockObject)(SYSTEM_FONT));
        CHECK(U(GetTextMetricsW)(ic, &tm));
        CHECK(tm.tmHeight > 0);
        U(SelectObject)(ic, old);
        U(DeleteDC)(ic);
    }
}
TEST(ep_dc_ic_caps_match_dc) {
    REQUIRE(U(CreateICA)); REQUIRE(U(CreateDCA));
    HDC ic = U(CreateICA)("DISPLAY", NULL, NULL, NULL), dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    static const int idx[] = { HORZRES, VERTRES, BITSPIXEL, PLANES, LOGPIXELSX, LOGPIXELSY, RASTERCAPS, TECHNOLOGY, HORZSIZE, VERTSIZE, NUMCOLORS };
    for (unsigned i = 0; i < sizeof idx / sizeof idx[0]; i++) {
        int a = U(GetDeviceCaps)(ic, idx[i]), b = U(GetDeviceCaps)(dc, idx[i]);
        CHECKF(a == b, "index %d IC %d DC %d", idx[i], a, b);
    }
    if (ic) U(DeleteDC)(ic);
    if (dc) U(DeleteDC)(dc);
}
TEST(ep_dc_createdc_matches_screen_dc) {
    REQUIRE(U(CreateDCA));
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL), sd = ep_scr();
    static const int idx[] = { HORZRES, VERTRES, BITSPIXEL, PLANES, LOGPIXELSX, LOGPIXELSY, NUMCOLORS, SIZEPALETTE };
    for (unsigned i = 0; i < sizeof idx / sizeof idx[0]; i++) {
        int a = U(GetDeviceCaps)(dc, idx[i]), b = U(GetDeviceCaps)(sd, idx[i]);
        CHECKF(a == b, "index %d CreateDC %d GetDC %d", idx[i], a, b);
    }
    if (dc) U(DeleteDC)(dc);
    ep_scr_free(sd);
}
TEST(ep_dc_createdc_compatible_bitmap_roundtrip) {
    REQUIRE(U(CreateDCA)); REQUIRE(U(CreateCompatibleBitmap));
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    HDC m = U(CreateCompatibleDC)(dc);
    HBITMAP bm = U(CreateCompatibleBitmap)(dc, 20, 10);
    CHECK(bm != NULL);
    HGDIOBJ o = U(SelectObject)(m, bm);
    CHECK(o != NULL);
    BITMAP bmp; memset(&bmp, 0, sizeof bmp);
    U(GetObjectW)(bm, sizeof bmp, &bmp);
    CHECK(bmp.bmWidth == 20 && bmp.bmHeight == 10);
    CHECK(bmp.bmBitsPixel == U(GetDeviceCaps)(dc, BITSPIXEL));
    U(SelectObject)(m, o); U(DeleteObject)(bm); U(DeleteDC)(m); U(DeleteDC)(dc);
}
TEST(ep_dc_deletedc_twice_fails) {
    REQUIRE(U(CreateDCA));
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    CHECK(U(DeleteDC)(dc));
    SET_ERR(0);
    CHECK(!U(DeleteDC)(dc));
}
TEST(ep_dc_deletedc_on_getdc_dc_fails) {
    REQUIRE(U(GetDC));
    HDC d = ep_scr();
    SET_ERR(0);
    BOOL r = U(DeleteDC)(d);
    t_note("DeleteDC(GetDC(NULL)) -> %d", r);
    U(ReleaseDC)(NULL, d);
}

/* ---- DEVMODE round trip ---- */
static BOOL cur_devmode(DEVMODEW *dm) {
    memset(dm, 0, sizeof *dm);
    dm->dmSize = sizeof *dm;
    return U(EnumDisplaySettingsW)(NULL, ENUM_CURRENT_SETTINGS, dm);
}
TEST(ep_devmode_enum_current) {
    REQUIRE(U(EnumDisplaySettingsW));
    DEVMODEW dm;
    CHECK(cur_devmode(&dm));
    CHECK(dm.dmPelsWidth >= 640 && dm.dmPelsHeight >= 480);
    CHECK(dm.dmBitsPerPel == 8 || dm.dmBitsPerPel == 16 || dm.dmBitsPerPel == 24 || dm.dmBitsPerPel == 32);
    CHECK((dm.dmFields & (DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL)) == (DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL));
    CHECK(dm.dmSize >= 148);
}
TEST(ep_devmode_createdc_with_devmode) {
    REQUIRE(U(EnumDisplaySettingsW)); REQUIRE(U(CreateDCW));
    DEVMODEW dm; CHECK(cur_devmode(&dm));
    HDC dc = U(CreateDCW)(L"DISPLAY", NULL, NULL, &dm);
    CHECK(dc != NULL);
    if (dc) {
        t_note("devmode %lux%lu@%lu, DC desktop %dx%d bpp %d", dm.dmPelsWidth, dm.dmPelsHeight, dm.dmBitsPerPel,
               U(GetDeviceCaps)(dc, DESKTOPHORZRES), U(GetDeviceCaps)(dc, DESKTOPVERTRES), U(GetDeviceCaps)(dc, BITSPIXEL));
        CHECK(U(GetDeviceCaps)(dc, TECHNOLOGY) == DT_RASDISPLAY);
        U(DeleteDC)(dc);
    }
}
TEST(ep_devmode_createic_with_devmode) {
    REQUIRE(U(EnumDisplaySettingsW)); REQUIRE(U(CreateICW));
    DEVMODEW dm; CHECK(cur_devmode(&dm));
    HDC dc = U(CreateICW)(L"DISPLAY", NULL, NULL, &dm);
    CHECK(dc != NULL);
    if (dc) { CHECK(U(GetDeviceCaps)(dc, BITSPIXEL) == (int)dm.dmBitsPerPel || U(GetDeviceCaps)(dc, BITSPIXEL) > 0); U(DeleteDC)(dc); }
}
TEST(ep_devmode_createdc_current_bpp_matches) {
    REQUIRE(U(EnumDisplaySettingsW)); REQUIRE(U(CreateDCW));
    DEVMODEW dm; CHECK(cur_devmode(&dm));
    HDC dc = U(CreateDCW)(L"DISPLAY", NULL, NULL, &dm);
    if (dc) {
        int bpp = U(GetDeviceCaps)(dc, BITSPIXEL) * U(GetDeviceCaps)(dc, PLANES);
        if ((DWORD)bpp != dm.dmBitsPerPel) t_note("DC bpp*planes %d vs devmode %lu", bpp, dm.dmBitsPerPel);
        CHECK(bpp > 0);
        U(DeleteDC)(dc);
    }
}
TEST(ep_devmode_createdca_converted) {
    REQUIRE(U(EnumDisplaySettingsA)); REQUIRE(U(CreateDCA));
    DEVMODEA dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    CHECK(U(EnumDisplaySettingsA)(NULL, ENUM_CURRENT_SETTINGS, &dm));
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, &dm);
    CHECK(dc != NULL);
    if (dc) U(DeleteDC)(dc);
}
TEST(ep_devmode_createdc_zeroed_devmode_note) {
    REQUIRE(U(CreateDCW));
    DEVMODEW dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    HDC dc = U(CreateDCW)(L"DISPLAY", NULL, NULL, &dm);
    t_note("CreateDCW(DISPLAY, zeroed devmode) -> %p", dc);
    if (dc) { CHECK(U(GetDeviceCaps)(dc, TECHNOLOGY) == DT_RASDISPLAY); U(DeleteDC)(dc); }
}
TEST(ep_devmode_createdc_bad_size_note) {
    REQUIRE(U(CreateDCW));
    DEVMODEW dm; memset(&dm, 0, sizeof dm); dm.dmSize = 1;
    HDC dc = U(CreateDCW)(L"DISPLAY", NULL, NULL, &dm);
    t_note("CreateDCW(DISPLAY, dmSize=1) -> %p", dc);
    if (dc) U(DeleteDC)(dc);
}
TEST(ep_devmode_resetdcw_display_same_devmode) {
    REQUIRE(U(EnumDisplaySettingsW)); REQUIRE(U(ResetDCW)); REQUIRE(U(CreateDCW));
    DEVMODEW dm; CHECK(cur_devmode(&dm));
    HDC dc = U(CreateDCW)(L"DISPLAY", NULL, NULL, &dm);
    CHECK(dc != NULL);
    if (dc) {
        SET_ERR(0);
        HDC r = U(ResetDCW)(dc, &dm);
        t_note("ResetDCW(display, same devmode) -> %p (dc %p) err %lu", r, dc, GetLastError());
        if (r) CHECK(r == dc);
        U(DeleteDC)(dc);
    }
}
TEST(ep_devmode_resetdca_display_same_devmode) {
    REQUIRE(U(EnumDisplaySettingsA)); REQUIRE(U(ResetDCA)); REQUIRE(U(CreateDCA));
    DEVMODEA dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    CHECK(U(EnumDisplaySettingsA)(NULL, ENUM_CURRENT_SETTINGS, &dm));
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, &dm);
    if (dc) {
        HDC r = U(ResetDCA)(dc, &dm);
        t_note("ResetDCA(display) -> %p (dc %p)", r, dc);
        if (r) CHECK(r == dc);
        U(DeleteDC)(dc);
    }
}
TEST(ep_devmode_resetdc_null_dc) {
    REQUIRE(U(ResetDCW)); REQUIRE(U(ResetDCA));
    DEVMODEW dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    DEVMODEA da; memset(&da, 0, sizeof da); da.dmSize = sizeof da;
    SET_ERR(0);
    CHECK(U(ResetDCW)(NULL, &dm) == NULL);
    CHECK(U(ResetDCA)(NULL, &da) == NULL);
    CHECK(U(ResetDCW)(NULL, NULL) == NULL);
}
TEST(ep_devmode_resetdc_bogus_dc) {
    REQUIRE(U(ResetDCW));
    DEVMODEW dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    CHECK(U(ResetDCW)(EP_BOGUS_DC, &dm) == NULL);
    CHECK(U(ResetDCW)(EP_BOGUS_DC2, NULL) == NULL);
}
TEST(ep_devmode_resetdc_mem_dc_note) {
    REQUIRE(U(ResetDCW));
    HDC m = U(CreateCompatibleDC)(NULL);
    DEVMODEW dm; CHECK(cur_devmode(&dm));
    SET_ERR(0);
    HDC r = U(ResetDCW)(m, &dm);
    t_note("ResetDCW(mem DC) -> %p err %lu", r, GetLastError());
    U(DeleteDC)(m);
}
TEST(ep_devmode_resetdc_deleted_dc) {
    REQUIRE(U(ResetDCW));
    HDC m = U(CreateCompatibleDC)(NULL); U(DeleteDC)(m);
    DEVMODEW dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    CHECK(U(ResetDCW)(m, &dm) == NULL);
}
TEST(ep_devmode_resetdc_keeps_display_mode) {
    REQUIRE(U(EnumDisplaySettingsW)); REQUIRE(U(ResetDCW)); REQUIRE(U(CreateDCW));
    DEVMODEW dm, after; CHECK(cur_devmode(&dm));
    HDC dc = U(CreateDCW)(L"DISPLAY", NULL, NULL, &dm);
    if (dc) { U(ResetDCW)(dc, &dm); U(DeleteDC)(dc); }
    CHECK(cur_devmode(&after));
    CHECK(after.dmPelsWidth == dm.dmPelsWidth && after.dmPelsHeight == dm.dmPelsHeight && after.dmBitsPerPel == dm.dmBitsPerPel);
}

/* ---- monitors ---- */
#define MAXMON 16
typedef struct { HMONITOR h[MAXMON]; int n; } monlist;
static BOOL CALLBACK mon_cb(HMONITOR hm, HDC hdc, LPRECT rc, LPARAM lp) {
    monlist *l = (monlist *)lp;
    if (l->n < MAXMON) l->h[l->n++] = hm;
    return TRUE;
}
static int mon_enum(monlist *l) {
    l->n = 0;
    U(EnumDisplayMonitors)(NULL, NULL, mon_cb, (LPARAM)l);
    return l->n;
}
TEST(ep_monitor_enum_count) {
    REQUIRE(U(EnumDisplayMonitors));
    monlist l;
    CHECK(mon_enum(&l) >= 1);
    CHECKF(mon_enum(&l) == U(GetSystemMetrics)(SM_CMONITORS) || U(GetSystemMetrics)(SM_CMONITORS) == 0, "enum %d vs SM_CMONITORS %d", l.n, U(GetSystemMetrics)(SM_CMONITORS));
}
TEST(ep_monitor_info_primary_exactly_one) {
    REQUIRE(U(EnumDisplayMonitors)); REQUIRE(U(GetMonitorInfoW));
    monlist l; mon_enum(&l);
    int primaries = 0;
    for (int i = 0; i < l.n; i++) {
        MONITORINFOEXW mi; memset(&mi, 0, sizeof mi); mi.cbSize = sizeof mi;
        CHECK(U(GetMonitorInfoW)(l.h[i], (MONITORINFO *)&mi));
        if (mi.dwFlags & MONITORINFOF_PRIMARY) primaries++;
        CHECK(mi.szDevice[0] != 0);
        CHECK(mi.rcMonitor.right > mi.rcMonitor.left && mi.rcMonitor.bottom > mi.rcMonitor.top);
    }
    CHECK(primaries == 1);
}
TEST(ep_monitor_dc_per_monitor_caps) {
    REQUIRE(U(EnumDisplayMonitors)); REQUIRE(U(GetMonitorInfoW)); REQUIRE(U(CreateDCW));
    DPI_AWARENESS_CONTEXT old = dpi_push(CTX_PMONITOR2);
    monlist l; mon_enum(&l);
    for (int i = 0; i < l.n; i++) {
        MONITORINFOEXW mi; memset(&mi, 0, sizeof mi); mi.cbSize = sizeof mi;
        if (!U(GetMonitorInfoW)(l.h[i], (MONITORINFO *)&mi)) { CHECK(0); continue; }
        HDC dc = U(CreateDCW)(L"DISPLAY", mi.szDevice, NULL, NULL);
        CHECKF(dc != NULL, "CreateDC for %ls", mi.szDevice);
        if (!dc) continue;
        CHECK(U(GetDeviceCaps)(dc, TECHNOLOGY) == DT_RASDISPLAY);
        int w = U(GetDeviceCaps)(dc, HORZRES), h = U(GetDeviceCaps)(dc, VERTRES);
        CHECK(w > 0 && h > 0);
        if (w != mi.rcMonitor.right - mi.rcMonitor.left || h != mi.rcMonitor.bottom - mi.rcMonitor.top)
            t_note("monitor %ls: DC %dx%d vs rect %ldx%ld", mi.szDevice, w, h, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top);
        CHECK(U(GetDeviceCaps)(dc, BITSPIXEL) > 0);
        U(DeleteDC)(dc);
    }
    dpi_pop(old);
}
TEST(ep_monitor_dc_device_name_as_driver_note) {
    REQUIRE(U(EnumDisplayMonitors)); REQUIRE(U(GetMonitorInfoW)); REQUIRE(U(CreateDCW));
    monlist l; mon_enum(&l);
    for (int i = 0; i < l.n; i++) {
        MONITORINFOEXW mi; memset(&mi, 0, sizeof mi); mi.cbSize = sizeof mi;
        U(GetMonitorInfoW)(l.h[i], (MONITORINFO *)&mi);
        HDC a = U(CreateDCW)(mi.szDevice, NULL, NULL, NULL);
        HDC b = U(CreateDCW)(mi.szDevice, mi.szDevice, NULL, NULL);
        t_note("monitor %ls: CreateDC(dev,NULL)=%p CreateDC(dev,dev)=%p", mi.szDevice, a, b);
        if (a) U(DeleteDC)(a);
        if (b) U(DeleteDC)(b);
    }
}
TEST(ep_monitor_dc_org_matches_rect_note) {
    REQUIRE(U(EnumDisplayMonitors)); REQUIRE(U(GetMonitorInfoW)); REQUIRE(U(CreateDCW)); REQUIRE(U(GetDCOrgEx));
    DPI_AWARENESS_CONTEXT old = dpi_push(CTX_PMONITOR2);
    monlist l; mon_enum(&l);
    for (int i = 0; i < l.n; i++) {
        MONITORINFOEXW mi; memset(&mi, 0, sizeof mi); mi.cbSize = sizeof mi;
        U(GetMonitorInfoW)(l.h[i], (MONITORINFO *)&mi);
        HDC dc = U(CreateDCW)(L"DISPLAY", mi.szDevice, NULL, NULL);
        if (!dc) continue;
        POINT pt = { -77, -77 };
        BOOL ok = U(GetDCOrgEx)(dc, &pt);
        CHECK(ok);
        if (pt.x != mi.rcMonitor.left || pt.y != mi.rcMonitor.top)
            t_note("monitor %ls: DC origin (%ld,%ld) vs rect origin (%ld,%ld)", mi.szDevice, pt.x, pt.y, mi.rcMonitor.left, mi.rcMonitor.top);
        U(DeleteDC)(dc);
    }
    dpi_pop(old);
}
TEST(ep_monitor_dc_distinct_per_monitor_bpp_consistent) {
    REQUIRE(U(EnumDisplayMonitors)); REQUIRE(U(GetMonitorInfoW)); REQUIRE(U(CreateDCW));
    monlist l; mon_enum(&l);
    for (int i = 0; i < l.n; i++) {
        MONITORINFOEXW mi; memset(&mi, 0, sizeof mi); mi.cbSize = sizeof mi;
        U(GetMonitorInfoW)(l.h[i], (MONITORINFO *)&mi);
        HDC dc = U(CreateDCW)(L"DISPLAY", mi.szDevice, NULL, NULL);
        if (!dc) continue;
        int planes = U(GetDeviceCaps)(dc, PLANES), bpp = U(GetDeviceCaps)(dc, BITSPIXEL);
        CHECK(planes == 1);
        CHECK(bpp == 8 || bpp == 16 || bpp == 24 || bpp == 32);
        int ncol = U(GetDeviceCaps)(dc, NUMCOLORS);
        if (bpp > 8) CHECK(ncol == -1);
        U(DeleteDC)(dc);
    }
}
TEST(ep_monitor_dc_enum_with_hdc_callback) {
    REQUIRE(U(EnumDisplayMonitors));
    HDC sd = ep_scr();
    monlist l; l.n = 0;
    BOOL ok = U(EnumDisplayMonitors)(sd, NULL, mon_cb, (LPARAM)&l);
    CHECK(ok);
    CHECK(l.n >= 1);
    ep_scr_free(sd);
}
static BOOL CALLBACK mon_hdc_cb(HMONITOR hm, HDC hdc, LPRECT rc, LPARAM lp) {
    int *n = (int *)lp;
    if (hdc && U(GetDeviceCaps)(hdc, TECHNOLOGY) == DT_RASDISPLAY) (*n)++;
    return TRUE;
}
TEST(ep_monitor_enum_hdc_passed_to_callback) {
    REQUIRE(U(EnumDisplayMonitors));
    HDC sd = ep_scr(); int good = 0;
    U(EnumDisplayMonitors)(sd, NULL, mon_hdc_cb, (LPARAM)&good);
    CHECK(good >= 1);
    ep_scr_free(sd);
}
TEST(ep_monitor_info_invalid_cbsize) {
    REQUIRE(U(EnumDisplayMonitors)); REQUIRE(U(GetMonitorInfoW));
    monlist l; mon_enum(&l);
    MONITORINFO mi; memset(&mi, 0, sizeof mi);
    mi.cbSize = 7;
    SET_ERR(0);
    CHECK(!U(GetMonitorInfoW)(l.h[0], &mi));
    mi.cbSize = 0;
    CHECK(!U(GetMonitorInfoW)(l.h[0], &mi));
}
TEST(ep_monitor_dcs_gdi_balance) {
    REQUIRE(U(EnumDisplayMonitors)); REQUIRE(U(GetMonitorInfoW)); REQUIRE(U(CreateDCW)); REQUIRE(U(GetGuiResources));
    monlist l; mon_enum(&l);
    DWORD before = ep_gui();
    for (int k = 0; k < 10; k++) for (int i = 0; i < l.n; i++) {
        MONITORINFOEXW mi; memset(&mi, 0, sizeof mi); mi.cbSize = sizeof mi;
        U(GetMonitorInfoW)(l.h[i], (MONITORINFO *)&mi);
        HDC dc = U(CreateDCW)(L"DISPLAY", mi.szDevice, NULL, NULL);
        if (dc) U(DeleteDC)(dc);
    }
    CHECK(ep_gui() == before);
}

/* ---- GetDCOrgEx ---- */
TEST(ep_orgex_null_dc) { REQUIRE(U(GetDCOrgEx)); POINT p = { 5, 6 }; SET_ERR(0); CHECK(!U(GetDCOrgEx)(NULL, &p)); }
TEST(ep_orgex_bogus_dc) { REQUIRE(U(GetDCOrgEx)); POINT p = { 5, 6 }; CHECK(!U(GetDCOrgEx)(EP_BOGUS_DC, &p)); CHECK(!U(GetDCOrgEx)(EP_BOGUS_DC2, &p)); }
TEST(ep_orgex_deleted_dc) { REQUIRE(U(GetDCOrgEx)); HDC m = U(CreateCompatibleDC)(NULL); U(DeleteDC)(m); POINT p = { 5, 6 }; CHECK(!U(GetDCOrgEx)(m, &p)); }
TEST(ep_orgex_mem_dc_origin_zero) {
    REQUIRE(U(GetDCOrgEx));
    HDC m = U(CreateCompatibleDC)(NULL);
    POINT p = { 99, 99 };
    CHECK(U(GetDCOrgEx)(m, &p));
    CHECK(p.x == 0 && p.y == 0);
    U(DeleteDC)(m);
}
TEST(ep_orgex_dib_dc_origin_zero) {
    REQUIRE(U(GetDCOrgEx));
    HBITMAP b, o; HDC m = ep_memdc32(4, 4, &b, &o, NULL);
    POINT p = { 99, 99 };
    if (m) { CHECK(U(GetDCOrgEx)(m, &p)); CHECK(p.x == 0 && p.y == 0); ep_memdc_free(m, b, o); }
}
TEST(ep_orgex_mem_dc_after_setviewport_still_zero) {
    REQUIRE(U(GetDCOrgEx)); REQUIRE(U(SetViewportOrgEx));
    HDC m = U(CreateCompatibleDC)(NULL);
    U(SetViewportOrgEx)(m, 17, 23, NULL);
    POINT p = { 99, 99 };
    CHECK(U(GetDCOrgEx)(m, &p));
    CHECK(p.x == 0 && p.y == 0);
    U(DeleteDC)(m);
}
TEST(ep_orgex_window_dc_matches_client_origin) {
    REQUIRE(U(GetDCOrgEx)); REQUIRE(U(ClientToScreen));
    DPI_AWARENESS_CONTEXT old = dpi_push(CTX_PMONITOR2);
    HWND w = t_make_window(NULL, WS_POPUP, 0, 50, 60, 100, 80, NULL);
    CHECK(w != NULL);
    if (w) {
        HDC d = U(GetDC)(w);
        POINT p = { -1, -1 }, c = { 0, 0 };
        CHECK(U(GetDCOrgEx)(d, &p));
        U(ClientToScreen)(w, &c);
        CHECKF(p.x == c.x && p.y == c.y, "org (%ld,%ld) client (%ld,%ld)", p.x, p.y, c.x, c.y);
        U(ReleaseDC)(w, d);
        U(DestroyWindow)(w);
    }
    dpi_pop(old);
}
TEST(ep_orgex_windowdc_vs_clientdc_offset) {
    REQUIRE(U(GetDCOrgEx)); REQUIRE(U(GetWindowDC)); REQUIRE(U(GetWindowRect)); REQUIRE(U(ClientToScreen));
    DPI_AWARENESS_CONTEXT old = dpi_push(CTX_PMONITOR2);
    HWND w = t_make_window(NULL, WS_OVERLAPPEDWINDOW, 0, 120, 130, 200, 150, NULL);
    if (w) {
        HDC wd = U(GetWindowDC)(w), cd = U(GetDC)(w);
        POINT pw = {0}, pc = {0}, cs = {0};
        RECT wr;
        U(GetDCOrgEx)(wd, &pw); U(GetDCOrgEx)(cd, &pc);
        U(GetWindowRect)(w, &wr); U(ClientToScreen)(w, &cs);
        CHECKF(pw.x == wr.left && pw.y == wr.top, "window DC org (%ld,%ld) rect (%ld,%ld)", pw.x, pw.y, wr.left, wr.top);
        CHECKF(pc.x == cs.x && pc.y == cs.y, "client DC org (%ld,%ld) vs (%ld,%ld)", pc.x, pc.y, cs.x, cs.y);
        CHECK(pc.x > pw.x && pc.y > pw.y);
        U(ReleaseDC)(w, cd); U(ReleaseDC)(w, wd); U(DestroyWindow)(w);
    }
    dpi_pop(old);
}
TEST(ep_orgex_screen_dc_note) {
    REQUIRE(U(GetDCOrgEx));
    HDC d = ep_scr(); POINT p = { 99, 99 };
    CHECK(U(GetDCOrgEx)(d, &p));
    t_note("GetDC(NULL) origin (%ld,%ld)", p.x, p.y);
    ep_scr_free(d);
}
TEST(ep_orgex_createdc_display_note) {
    REQUIRE(U(GetDCOrgEx)); REQUIRE(U(CreateDCA));
    HDC d = U(CreateDCA)("DISPLAY", NULL, NULL, NULL); POINT p = { 99, 99 };
    CHECK(U(GetDCOrgEx)(d, &p));
    t_note("CreateDC(DISPLAY) origin (%ld,%ld)", p.x, p.y);
    U(DeleteDC)(d);
}

/* ---- window DC vs memory DC differences ---- */
TEST(ep_wdc_object_types) {
    REQUIRE(U(GetObjectType));
    HWND w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 40, 40, NULL);
    HDC d = U(GetDC)(w), m = U(CreateCompatibleDC)(d);
    CHECK(U(GetObjectType)(d) == OBJ_DC);
    CHECK(U(GetObjectType)(m) == OBJ_MEMDC);
    U(DeleteDC)(m); U(ReleaseDC)(w, d); U(DestroyWindow)(w);
}
TEST(ep_wdc_windowfromdc) {
    REQUIRE(U(WindowFromDC));
    HWND w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 40, 40, NULL);
    HDC d = U(GetDC)(w), m = U(CreateCompatibleDC)(d);
    CHECK(U(WindowFromDC)(d) == w);
    CHECK(U(WindowFromDC)(m) == NULL);
    U(DeleteDC)(m); U(ReleaseDC)(w, d); U(DestroyWindow)(w);
}
TEST(ep_wdc_clipbox_mem_default) {
    REQUIRE(U(GetClipBox));
    HDC m = U(CreateCompatibleDC)(NULL);
    RECT rc = { 9, 9, 9, 9 };
    int r = U(GetClipBox)(m, &rc);
    CHECK(r == SIMPLEREGION);
    CHECK(rc.left == 0 && rc.top == 0 && rc.right == 1 && rc.bottom == 1);
    U(DeleteDC)(m);
}
TEST(ep_wdc_clipbox_mem_dib) {
    REQUIRE(U(GetClipBox));
    HBITMAP b, o; HDC m = ep_memdc32(21, 13, &b, &o, NULL);
    if (m) {
        RECT rc;
        CHECK(U(GetClipBox)(m, &rc) == SIMPLEREGION);
        CHECK(rc.left == 0 && rc.top == 0 && rc.right == 21 && rc.bottom == 13);
        ep_memdc_free(m, b, o);
    }
}
TEST(ep_wdc_clipbox_window_client_size) {
    REQUIRE(U(GetClipBox)); REQUIRE(U(GetClientRect));
    HWND w = t_make_window(NULL, WS_POPUP, 0, 0, 0, 90, 70, NULL);
    if (w) {
        HDC d = U(GetDC)(w);
        RECT rc, cr;
        int r = U(GetClipBox)(d, &rc);
        U(GetClientRect)(w, &cr);
        CHECK(r != ERROR && r != NULLREGION);
        t_note("window DC clipbox (%ld,%ld,%ld,%ld) client (%ld,%ld,%ld,%ld)", rc.left, rc.top, rc.right, rc.bottom, cr.left, cr.top, cr.right, cr.bottom);
        U(ReleaseDC)(w, d); U(DestroyWindow)(w);
    }
}
TEST(ep_wdc_mem_dc_default_selected_objects) {
    REQUIRE(U(GetCurrentObject));
    HDC m = U(CreateCompatibleDC)(NULL);
    CHECK(U(GetCurrentObject)(m, OBJ_BITMAP) != NULL);
    CHECK(U(GetCurrentObject)(m, OBJ_PEN) == U(GetStockObject)(BLACK_PEN));
    CHECK(U(GetCurrentObject)(m, OBJ_BRUSH) == U(GetStockObject)(WHITE_BRUSH));
    U(DeleteDC)(m);
}
TEST(ep_wdc_screen_dc_has_no_bitmap_object) {
    REQUIRE(U(GetCurrentObject));
    HDC d = ep_scr();
    HGDIOBJ b = U(GetCurrentObject)(d, OBJ_BITMAP);
    t_note("GetCurrentObject(screen, OBJ_BITMAP) = %p", b);
    ep_scr_free(d);
}
TEST(ep_wdc_select_bitmap_into_screen_dc_fails) {
    REQUIRE(U(SelectObject)); REQUIRE(U(CreateBitmap));
    HDC d = ep_scr();
    HBITMAP bm = U(CreateBitmap)(4, 4, 1, 1, NULL);
    SET_ERR(0);
    HGDIOBJ r = U(SelectObject)(d, bm);
    CHECK(r == NULL);
    if (r) U(SelectObject)(d, r);
    U(DeleteObject)(bm); ep_scr_free(d);
}
TEST(ep_wdc_getdc_returns_same_cached_dc_note) {
    REQUIRE(U(GetDC));
    HWND w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 40, 40, NULL);
    HDC a = U(GetDC)(w), b = U(GetDC)(w);
    t_note("GetDC twice (no CS_OWNDC): %p %p", a, b);
    CHECK(a != NULL && b != NULL);
    U(ReleaseDC)(w, b); U(ReleaseDC)(w, a); U(DestroyWindow)(w);
}
TEST(ep_wdc_releasedc_return_values) {
    REQUIRE(U(ReleaseDC));
    HWND w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 40, 40, NULL);
    HDC a = U(GetDC)(w);
    CHECK(a != NULL);
    int r1 = U(ReleaseDC)(w, a);
    CHECKF(r1 == 1, "first ReleaseDC returned %d", r1);
    int r2 = U(ReleaseDC)(w, a);
    CHECKF(r2 == 0, "second ReleaseDC returned %d", r2);
    U(DestroyWindow)(w);
}
TEST(ep_wdc_dc_usable_after_window_hidden) {
    REQUIRE(U(GetDC)); REQUIRE(U(ShowWindow));
    HWND w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 40, 40, NULL);
    HDC a = U(GetDC)(w);
    U(ShowWindow)(w, SW_HIDE);
    CHECK(U(GetDeviceCaps)(a, TECHNOLOGY) == DT_RASDISPLAY);
    U(ReleaseDC)(w, a); U(DestroyWindow)(w);
}

/* ---- cross-thread ---- */
static DWORD WINAPI xt_caps(LPVOID p) {
    HDC dc = (HDC)p;
    return (DWORD)U(GetDeviceCaps)(dc, TECHNOLOGY);
}
static DWORD run_thread(LPTHREAD_START_ROUTINE f, LPVOID p) {
    HANDLE h = CreateThread(NULL, 0, f, p, 0, NULL);
    DWORD rc = 0xdeadbeef;
    if (h) { WaitForSingleObject(h, 20000); GetExitCodeThread(h, &rc); CloseHandle(h); }
    return rc;
}
TEST(ep_xthread_memdc_caps) {
    REQUIRE(U(GetDeviceCaps));
    HDC m = U(CreateCompatibleDC)(NULL);
    CHECK(run_thread(xt_caps, m) == DT_RASDISPLAY);
    U(DeleteDC)(m);
}
TEST(ep_xthread_screen_dc_caps) {
    REQUIRE(U(GetDeviceCaps));
    HDC d = ep_scr();
    CHECK(run_thread(xt_caps, d) == DT_RASDISPLAY);
    ep_scr_free(d);
}
TEST(ep_xthread_createdc_caps) {
    REQUIRE(U(CreateDCA));
    HDC d = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    CHECK(run_thread(xt_caps, d) == DT_RASDISPLAY);
    U(DeleteDC)(d);
}
static DWORD WINAPI xt_delete(LPVOID p) { return U(DeleteDC)((HDC)p) ? 1 : 0; }
TEST(ep_xthread_delete_memdc_from_other_thread) {
    REQUIRE(U(DeleteDC));
    HDC m = U(CreateCompatibleDC)(NULL);
    CHECK(run_thread(xt_delete, m) == 1);
    CHECK(U(GetDeviceCaps)(m, TECHNOLOGY) == 0);
}
static DWORD WINAPI xt_create_dc(LPVOID p) { *(HDC *)p = U(CreateCompatibleDC)(NULL); return 0; }
TEST(ep_xthread_dc_survives_thread_exit) {
    REQUIRE(U(CreateCompatibleDC));
    HDC m = NULL;
    run_thread(xt_create_dc, &m);
    CHECK(m != NULL);
    if (m) { CHECK(U(GetDeviceCaps)(m, TECHNOLOGY) == DT_RASDISPLAY); CHECK(U(DeleteDC)(m)); }
}
static DWORD WINAPI xt_escape(LPVOID p) { int q = 0x100; return (DWORD)U(Escape)((HDC)p, QUERYESCSUPPORT, sizeof q, (LPCSTR)&q, NULL); }
TEST(ep_xthread_escape_query) {
    REQUIRE(U(Escape));
    HDC d = ep_scr();
    DWORD a = run_thread(xt_escape, d);
    CHECKF(a == 1, "got %lu", a);
    ep_scr_free(d);
}

/* ---- DPI ---- */
TEST(ep_dpi_set_context_returns_previous) {
    DPI_AWARENESS_CONTEXT old = dpi_push(CTX_UNAWARE);
    REQUIRE(old);
    DPI_AWARENESS_CONTEXT prev = p_setdpi(CTX_SYSTEM);
    CHECK(prev != NULL);
    getdpi_pfn g = (getdpi_pfn)t_proc("GetThreadDpiAwarenessContext");
    eqdpi_pfn eq = (eqdpi_pfn)t_proc("AreDpiAwarenessContextsEqual");
    if (g && eq) { CHECK(eq(g(), CTX_SYSTEM)); }
    p_setdpi(old);
    if (g && eq) CHECK(eq(g(), old));
}
TEST(ep_dpi_set_context_invalid_fails) {
    DPI_AWARENESS_CONTEXT old = dpi_push(CTX_UNAWARE);
    REQUIRE(old);
    SET_ERR(0);
    CHECK(p_setdpi((DPI_AWARENESS_CONTEXT)0) == NULL);
    { DPI_AWARENESS_CONTEXT r = p_setdpi((DPI_AWARENESS_CONTEXT)(ULONG_PTR)0x1234); t_note("SetThreadDpiAwarenessContext(0x1234) -> %p", r); if (r) p_setdpi(r); }
    { DPI_AWARENESS_CONTEXT r = p_setdpi((DPI_AWARENESS_CONTEXT)(LONG_PTR)-100); t_note("SetThreadDpiAwarenessContext(-100) -> %p", r); if (r) p_setdpi(r); }
    p_setdpi(old);
}
TEST(ep_dpi_unaware_logpixels_96) {
    REQUIRE(U(GetDeviceCaps));
    DPI_AWARENESS_CONTEXT old = dpi_push(CTX_UNAWARE);
    REQUIRE(old);
    HDC d = ep_scr();
    CHECK(U(GetDeviceCaps)(d, LOGPIXELSX) == 96);
    CHECK(U(GetDeviceCaps)(d, LOGPIXELSY) == 96);
    ep_scr_free(d);
    HDC m = U(CreateCompatibleDC)(NULL);
    CHECK(U(GetDeviceCaps)(m, LOGPIXELSX) == 96);
    U(DeleteDC)(m);
    dpi_pop(old);
}
TEST(ep_dpi_unaware_window_dpi_96) {
    getdpiwin_pfn gw = (getdpiwin_pfn)t_proc("GetDpiForWindow");
    REQUIRE(gw);
    DPI_AWARENESS_CONTEXT old = dpi_push(CTX_UNAWARE);
    REQUIRE(old);
    HWND w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 40, 40, NULL);
    CHECK(gw(w) == 96);
    U(DestroyWindow)(w);
    dpi_pop(old);
}
TEST(ep_dpi_system_aware_logpixels_vs_system_dpi) {
    REQUIRE(U(GetDeviceCaps));
    getdpisys_pfn gs = (getdpisys_pfn)t_proc("GetDpiForSystem");
    REQUIRE(gs);
    DPI_AWARENESS_CONTEXT old = dpi_push(CTX_SYSTEM);
    REQUIRE(old);
    HDC d = ep_scr();
    int x = U(GetDeviceCaps)(d, LOGPIXELSX), y = U(GetDeviceCaps)(d, LOGPIXELSY);
    UINT sys = gs();
    if ((UINT)x != sys || (UINT)y != sys) t_note("system-aware LOGPIXELS %d,%d vs GetDpiForSystem %u", x, y, sys);
    CHECK(x >= 96 && y >= 96);
    CHECK(sys >= 96);
    ep_scr_free(d);
    dpi_pop(old);
}
TEST(ep_dpi_permonitor_logpixels_vs_system_dpi) {
    REQUIRE(U(GetDeviceCaps));
    getdpisys_pfn gs = (getdpisys_pfn)t_proc("GetDpiForSystem");
    REQUIRE(gs);
    DPI_AWARENESS_CONTEXT old = dpi_push(CTX_PMONITOR2);
    REQUIRE(old);
    HDC d = ep_scr();
    int x = U(GetDeviceCaps)(d, LOGPIXELSX);
    if ((UINT)x != gs()) t_note("per-monitor-v2 LOGPIXELSX %d vs GetDpiForSystem %u", x, gs());
    CHECK(x >= 96);
    ep_scr_free(d);
    dpi_pop(old);
}
TEST(ep_dpi_memdc_matches_screen_per_context) {
    REQUIRE(U(GetDeviceCaps));
    DPI_AWARENESS_CONTEXT ctxs[] = { CTX_UNAWARE, CTX_SYSTEM, CTX_PMONITOR, CTX_PMONITOR2 };
    DPI_AWARENESS_CONTEXT old = dpi_push(CTX_UNAWARE);
    REQUIRE(old);
    for (int i = 0; i < 4; i++) {
        if (!p_setdpi(ctxs[i])) continue;
        HDC d = ep_scr(), m = U(CreateCompatibleDC)(d);
        int a = U(GetDeviceCaps)(d, LOGPIXELSX), b = U(GetDeviceCaps)(m, LOGPIXELSX);
        CHECKF(a == b, "ctx %d: screen %d mem %d", i, a, b);
        U(DeleteDC)(m); ep_scr_free(d);
    }
    dpi_pop(old);
}
TEST(ep_dpi_horzres_unaware_vs_desktophorzres) {
    REQUIRE(U(GetDeviceCaps));
    DPI_AWARENESS_CONTEXT old = dpi_push(CTX_UNAWARE);
    REQUIRE(old);
    HDC d = ep_scr();
    int h = U(GetDeviceCaps)(d, HORZRES), dh = U(GetDeviceCaps)(d, DESKTOPHORZRES), lp = U(GetDeviceCaps)(d, LOGPIXELSX);
    CHECK(dh >= h);
    t_note("unaware HORZRES=%d DESKTOPHORZRES=%d LOGPIXELSX=%d", h, dh, lp);
    ep_scr_free(d);
    dpi_pop(old);
}
TEST(ep_dpi_scale_factor_consistency_note) {
    REQUIRE(U(GetDeviceCaps));
    DPI_AWARENESS_CONTEXT old = dpi_push(CTX_PMONITOR2);
    REQUIRE(old);
    HDC d = ep_scr();
    int h = U(GetDeviceCaps)(d, HORZRES), dh = U(GetDeviceCaps)(d, DESKTOPHORZRES);
    if (h != dh) t_note("per-monitor-v2 HORZRES %d != DESKTOPHORZRES %d", h, dh);
    CHECK(dh >= 640);
    ep_scr_free(d);
    dpi_pop(old);
}
TEST(ep_dpi_context_restored_after_tests) {
    getdpi_pfn g = (getdpi_pfn)t_proc("GetThreadDpiAwarenessContext");
    eqdpi_pfn eq = (eqdpi_pfn)t_proc("AreDpiAwarenessContextsEqual");
    REQUIRE(g); REQUIRE(eq);
    DPI_AWARENESS_CONTEXT cur = g();
    t_note("thread DPI context is %p", cur);
    DPI_AWARENESS_CONTEXT o = dpi_push(CTX_SYSTEM);
    dpi_pop(o);
    CHECK(eq(g(), cur));
}
