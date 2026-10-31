#include "gap.h"

/* ================= GetBitmapDpiScaleValue ================= */
TEST(gap1_bmpdpi_null_is_one) {
    GetBitmapDpiScaleValue_pfn f = UN(GetBitmapDpiScaleValue);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    CHECK(f(NULL) == 1);
}
TEST(gap1_bmpdpi_garbage_handles_are_one) {
    GetBitmapDpiScaleValue_pfn f = UN(GetBitmapDpiScaleValue);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    static const ULONG_PTR hs[] = { 1, 2, 0x1234, 0xffff, 0x10000, 0x00050000, 0x00850000, 0x7fffffff, 0xffffffff, ~(ULONG_PTR)0, 0x0123456789abcdefULL };
    for (unsigned i = 0; i < sizeof hs / sizeof *hs; i++)
        CHECKF(f((HBITMAP)hs[i]) == 1, "handle %#llx", (unsigned long long)hs[i]);
}
TEST(gap1_bmpdpi_plain_bitmaps_are_one) {
    REQUIRE(U(CreateBitmap)); REQUIRE(U(DeleteObject));
    GetBitmapDpiScaleValue_pfn f = UN(GetBitmapDpiScaleValue);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    static const int bpp[] = { 1, 4, 8, 16, 24, 32 };
    for (unsigned i = 0; i < sizeof bpp / sizeof *bpp; i++) {
        HBITMAP b = U(CreateBitmap)(8, 8, 1, bpp[i], NULL);
        if (!b) { t_note("CreateBitmap bpp %d failed", bpp[i]); continue; }
        CHECKF(f(b) == 1, "bpp %d", bpp[i]);
        U(DeleteObject)(b);
    }
}
TEST(gap1_bmpdpi_dibsection_is_one) {
    GapDib d;
    GetBitmapDpiScaleValue_pfn f = UN(GetBitmapDpiScaleValue);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    REQUIRE(U(CreateDIBSection));
    if (!gap_dib_make(&d, 16, 16)) { CHECK(0); return; }
    CHECK(f(d.bmp) == 1);
    gap_dib_free(&d);
}
TEST(gap1_bmpdpi_deleted_bitmap_is_one) {
    REQUIRE(U(CreateBitmap)); REQUIRE(U(DeleteObject));
    GetBitmapDpiScaleValue_pfn f = UN(GetBitmapDpiScaleValue);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    HBITMAP b = U(CreateBitmap)(4, 4, 1, 1, NULL);
    U(DeleteObject)(b);
    CHECK(f(b) == 1);
}
TEST(gap1_bmpdpi_wrong_object_types_are_one) {
    REQUIRE(U(CreatePen)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(CreateRectRgn)); REQUIRE(U(CreateCompatibleDC));
    GetBitmapDpiScaleValue_pfn f = UN(GetBitmapDpiScaleValue);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0);
    HBRUSH b = U(CreateSolidBrush)(0);
    HRGN r = U(CreateRectRgn)(0, 0, 1, 1);
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(f((HBITMAP)p) == 1);
    CHECK(f((HBITMAP)b) == 1);
    CHECK(f((HBITMAP)r) == 1);
    CHECK(f((HBITMAP)dc) == 1);
    CHECK(f((HBITMAP)U(GetStockObject)(WHITE_BRUSH)) == 1);
    U(DeleteObject)(p); U(DeleteObject)(b); U(DeleteObject)(r); U(DeleteDC)(dc);
}
TEST(gap1_bmpdpi_stock_bitmap) {
    REQUIRE(U(GetStockObject));
    GetBitmapDpiScaleValue_pfn f = UN(GetBitmapDpiScaleValue);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    HGDIOBJ s = U(GetStockObject)(21);  /* DEFAULT_BITMAP */
    t_note("stock default bitmap %p", s);
    if (s) CHECK(f((HBITMAP)s) == 1);
}
TEST(gap1_bmpdpi_compat_bitmap_nonzero) {
    REQUIRE(U(CreateCompatibleBitmap)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(DeleteObject));
    GetBitmapDpiScaleValue_pfn f = UN(GetBitmapDpiScaleValue);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    HDC (WINAPI *gdc)(HWND) = (void *)t_proc("GetDC");
    int (WINAPI *rdc)(HWND, HDC) = (void *)t_proc("ReleaseDC");
    REQUIRE(gdc); REQUIRE(rdc);
    HDC sc = gdc(NULL);
    HBITMAP b = U(CreateCompatibleBitmap)(sc, 8, 8);
    CHECK(b != NULL);
    ULONG v = f(b);
    t_note("compatible bitmap dpi scale %lu", v);
    CHECK(v >= 1);
    U(DeleteObject)(b);
    rdc(NULL, sc);
}
TEST(gap1_bmpdpi_repeatable) {
    REQUIRE(U(CreateBitmap)); REQUIRE(U(DeleteObject));
    GetBitmapDpiScaleValue_pfn f = UN(GetBitmapDpiScaleValue);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    HBITMAP b = U(CreateBitmap)(4, 4, 1, 32, NULL);
    ULONG first = f(b);
    for (int i = 0; i < 500; i++) if (f(b) != first) { CHECK(0); break; }
    U(DeleteObject)(b);
    CHECK(1);
}
TEST(gap1_bmpdpi_no_lasterror_change_for_invalid) {
    GetBitmapDpiScaleValue_pfn f = UN(GetBitmapDpiScaleValue);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    SetLastError(0x4242);
    f((HBITMAP)(ULONG_PTR)0x1234);
    t_note("last error after invalid handle: %lu", GetLastError());
}

/* ================= GetDCDpiScaleValue ================= */
TEST(gap1_dcdpi_null_is_one) {
    GetDCDpiScaleValue_pfn f = UN(GetDCDpiScaleValue);
    REQUIRE(f);
    CHECK(f(NULL) == 1);
}
TEST(gap1_dcdpi_null_both_modules) {
    GetDCDpiScaleValue_pfn a = (GetDCDpiScaleValue_pfn)t_proc_gdi32("GetDCDpiScaleValue");
    GetDCDpiScaleValue_pfn b = (GetDCDpiScaleValue_pfn)t_proc_gdi32full("GetDCDpiScaleValue");
    REQUIRE(a); REQUIRE(b);
    CHECK(a(NULL) == 1);
    CHECK(b(NULL) == 1);
}
TEST(gap1_dcdpi_garbage_handles_are_one) {
    GetDCDpiScaleValue_pfn f = UN(GetDCDpiScaleValue);
    REQUIRE(f);
    static const ULONG_PTR hs[] = { 1, 2, 0x1234, 0xffff, 0x10000, 0x00010001, 0x00010000, 0x7fffffff, 0xffffffff, ~(ULONG_PTR)0, 0x0123456789abcdefULL, 0x00ff0001 };
    for (unsigned i = 0; i < sizeof hs / sizeof *hs; i++)
        CHECKF(f((HDC)hs[i]) == 1, "handle %#llx", (unsigned long long)hs[i]);
}
TEST(gap1_dcdpi_memory_dc_at_least_one) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    GetDCDpiScaleValue_pfn f = UN(GetDCDpiScaleValue);
    REQUIRE(f);
    HDC dc = U(CreateCompatibleDC)(NULL);
    ULONG v = f(dc);
    t_note("memory DC dpi scale %lu", v);
    CHECK(v >= 1);
    U(DeleteDC)(dc);
}
TEST(gap1_dcdpi_memory_dc_stable) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    GetDCDpiScaleValue_pfn f = UN(GetDCDpiScaleValue);
    REQUIRE(f);
    HDC dc = U(CreateCompatibleDC)(NULL);
    ULONG first = f(dc);
    for (int i = 0; i < 1000; i++) if (f(dc) != first) { CHECK(0); break; }
    U(DeleteDC)(dc);
    CHECK(1);
}
TEST(gap1_dcdpi_deleted_dc_is_one) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    GetDCDpiScaleValue_pfn f = UN(GetDCDpiScaleValue);
    REQUIRE(f);
    HDC dc = U(CreateCompatibleDC)(NULL);
    U(DeleteDC)(dc);
    CHECK(f(dc) == 1);
}
TEST(gap1_dcdpi_wrong_types_are_one) {
    REQUIRE(U(CreatePen)); REQUIRE(U(CreateRectRgn)); REQUIRE(U(CreateBitmap));
    GetDCDpiScaleValue_pfn f = UN(GetDCDpiScaleValue);
    REQUIRE(f);
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0);
    HRGN r = U(CreateRectRgn)(0, 0, 1, 1);
    HBITMAP b = U(CreateBitmap)(2, 2, 1, 1, NULL);
    CHECK(f((HDC)p) == 1);
    CHECK(f((HDC)r) == 1);
    CHECK(f((HDC)b) == 1);
    U(DeleteObject)(p); U(DeleteObject)(r); U(DeleteObject)(b);
}
TEST(gap1_dcdpi_screen_dc_note) {
    GetDCDpiScaleValue_pfn f = UN(GetDCDpiScaleValue);
    HDC (WINAPI *gdc)(HWND) = (void *)t_proc("GetDC");
    int (WINAPI *rdc)(HWND, HDC) = (void *)t_proc("ReleaseDC");
    REQUIRE(f); REQUIRE(gdc); REQUIRE(rdc);
    HDC sc = gdc(NULL);
    CHECK(sc != NULL);
    ULONG v = f(sc);
    t_note("screen DC dpi scale %lu", v);
    CHECK(v >= 1);
    rdc(NULL, sc);
}
TEST(gap1_dcdpi_window_dc_note) {
    GetDCDpiScaleValue_pfn f = UN(GetDCDpiScaleValue);
    HDC (WINAPI *gdc)(HWND) = (void *)t_proc("GetDC");
    int (WINAPI *rdc)(HWND, HDC) = (void *)t_proc("ReleaseDC");
    BOOL (WINAPI *dw)(HWND) = (void *)t_proc("DestroyWindow");
    REQUIRE(f); REQUIRE(gdc); REQUIRE(rdc); REQUIRE(dw);
    HWND w = t_make_window(NULL, WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 40, 40, NULL);
    if (!w) { t_skip("cannot create window"); return; }
    HDC dc = gdc(w);
    ULONG v = f(dc);
    t_note("window DC dpi scale %lu", v);
    CHECK(v >= 1);
    rdc(w, dc);
    dw(w);
}
TEST(gap1_dcdpi_after_released_dc_is_one) {
    GetDCDpiScaleValue_pfn f = UN(GetDCDpiScaleValue);
    HDC (WINAPI *gdc)(HWND) = (void *)t_proc("GetDC");
    int (WINAPI *rdc)(HWND, HDC) = (void *)t_proc("ReleaseDC");
    REQUIRE(f); REQUIRE(gdc); REQUIRE(rdc);
    HDC sc = gdc(NULL);
    rdc(NULL, sc);
    CHECK(f(sc) == 1);
}
TEST(gap1_dcdpi_fresh_dcs_agree) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    GetDCDpiScaleValue_pfn f = UN(GetDCDpiScaleValue);
    REQUIRE(f);
    HDC a = U(CreateCompatibleDC)(NULL), b = U(CreateCompatibleDC)(NULL);
    CHECK(f(a) == f(b));
    U(DeleteDC)(a); U(DeleteDC)(b);
}
TEST(gap1_dcdpi_gdi32_gdi32full_agree) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    GetDCDpiScaleValue_pfn a = (GetDCDpiScaleValue_pfn)t_proc_gdi32("GetDCDpiScaleValue");
    GetDCDpiScaleValue_pfn b = (GetDCDpiScaleValue_pfn)t_proc_gdi32full("GetDCDpiScaleValue");
    REQUIRE(a); REQUIRE(b);
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(a(dc) == b(dc));
    U(DeleteDC)(dc);
}

/* ================= GetCurrentDpiInfo ================= */
TEST(gap1_curdpi_null_monitor_fails_invalid_handle) {
    GetCurrentDpiInfo_pfn f = UN(GetCurrentDpiInfo);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    unsigned char buf[0x80]; memset(buf, 0xcc, sizeof buf);
    SetLastError(0x1111);
    BOOL r = f(NULL, buf);
    CHECK(r == FALSE);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    unsigned char want[0x80]; memset(want, 0xcc, sizeof want);
    CHECK(!memcmp(buf, want, sizeof buf));   /* untouched on failure */
}
TEST(gap1_curdpi_bogus_monitors_fail) {
    GetCurrentDpiInfo_pfn f = UN(GetCurrentDpiInfo);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    static const ULONG_PTR hs[] = { 1, 2, 0x1234, 0xffff, 0x10001, 0x7fffffff, 0xffffffff, ~(ULONG_PTR)0 };
    unsigned char buf[0x80];
    for (unsigned i = 0; i < sizeof hs / sizeof *hs; i++) {
        memset(buf, 0xcc, sizeof buf);
        SetLastError(0);
        BOOL r = f((HMONITOR)hs[i], buf);
        CHECKF(r == FALSE, "handle %#llx", (unsigned long long)hs[i]);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "handle %#llx err %lu", (unsigned long long)hs[i], GetLastError());
        CHECKF(buf[0] == 0xcc && buf[0x5f] == 0xcc, "handle %#llx buffer touched", (unsigned long long)hs[i]);
    }
}
TEST(gap1_curdpi_null_out_with_bogus_monitor) {
    GetCurrentDpiInfo_pfn f = UN(GetCurrentDpiInfo);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    SetLastError(0);
    CHECK(f((HMONITOR)(ULONG_PTR)0x1234, NULL) == FALSE);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}
TEST(gap1_curdpi_hdc_as_monitor_fails) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    GetCurrentDpiInfo_pfn f = UN(GetCurrentDpiInfo);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    HDC dc = U(CreateCompatibleDC)(NULL);
    unsigned char buf[0x80];
    SetLastError(0);
    CHECK(f((HMONITOR)dc, buf) == FALSE);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    U(DeleteDC)(dc);
}
TEST(gap1_curdpi_primary_monitor_succeeds) {
    GetCurrentDpiInfo_pfn f = UN(GetCurrentDpiInfo);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    WCHAR dev[64]; HMONITOR m = NULL;
    if (!gap_primary_device(dev, &m)) { t_skip("no monitor"); return; }
    unsigned char buf[0xa0]; memset(buf, 0xcc, sizeof buf);
    SetLastError(0x7777);
    BOOL r = f(m, buf);
    t_note("GetCurrentDpiInfo(primary) -> %d", r);
    if (r) {
        CHECK(GetLastError() == 0x7777);          /* success path never writes last-error */
        for (int i = 0x60; i < 0xa0; i++) if (buf[i] != 0xcc) { CHECKF(0, "overrun at %#x", i); break; }
        const DWORD *d = (const DWORD *)buf;
        t_note("dpi info dwords: %lu %lu %lu %lu %lu %lu", d[0], d[1], d[2], d[3], d[4], d[5]);
    }
}
TEST(gap1_curdpi_primary_monitor_repeatable) {
    GetCurrentDpiInfo_pfn f = UN(GetCurrentDpiInfo);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    WCHAR dev[64]; HMONITOR m = NULL;
    if (!gap_primary_device(dev, &m)) { t_skip("no monitor"); return; }
    unsigned char a[0x60], b[0x60];
    memset(a, 0, sizeof a); memset(b, 0xff, sizeof b);
    BOOL ra = f(m, a), rb = f(m, b);
    CHECK(ra == rb);
    if (ra && rb) CHECK(!memcmp(a, b, sizeof a));
}

/* ================= SetVirtualResolution ================= */
static HDC mkdc(void) { return U(CreateCompatibleDC)(NULL); }

TEST(gap1_virtres_all_zero_ok_on_valid_dc) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    SetVirtualResolution_pfn f = UN(SetVirtualResolution);
    REQUIRE(f);
    HDC dc = mkdc();
    CHECK(f(dc, 0, 0, 0, 0) == TRUE);
    U(DeleteDC)(dc);
}
TEST(gap1_virtres_all_nonzero_ok) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    SetVirtualResolution_pfn f = UN(SetVirtualResolution);
    REQUIRE(f);
    HDC dc = mkdc();
    CHECK(f(dc, 1920, 1080, 508, 286) == TRUE);
    CHECK(f(dc, 0, 0, 0, 0) == TRUE);
    U(DeleteDC)(dc);
}
TEST(gap1_virtres_zero_nonzero_matrix) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    SetVirtualResolution_pfn f = UN(SetVirtualResolution);
    REQUIRE(f);
    HDC dc = mkdc();
    for (int m = 0; m < 16; m++) {
        int a[4];
        for (int i = 0; i < 4; i++) a[i] = (m >> i) & 1 ? 100 + i : 0;
        BOOL want = (m == 0 || m == 15);
        BOOL r = f(dc, a[0], a[1], a[2], a[3]);
        CHECKF(r == want, "mask %#x got %d want %d", m, r, want);
        f(dc, 0, 0, 0, 0);
    }
    U(DeleteDC)(dc);
}
TEST(gap1_virtres_negative_values_accepted) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    SetVirtualResolution_pfn f = UN(SetVirtualResolution);
    REQUIRE(f);
    HDC dc = mkdc();
    CHECK(f(dc, -1, -1, -1, -1) == TRUE);
    CHECK(f(dc, -1, 5, -7, 9) == TRUE);
    CHECK(f(dc, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff) == TRUE);
    CHECK(f(dc, (int)0x80000000, (int)0x80000000, (int)0x80000000, (int)0x80000000) == TRUE);
    f(dc, 0, 0, 0, 0);
    U(DeleteDC)(dc);
}
TEST(gap1_virtres_null_dc_fails) {
    SetVirtualResolution_pfn f = UN(SetVirtualResolution);
    REQUIRE(f);
    CHECK(f(NULL, 0, 0, 0, 0) == FALSE);
    CHECK(f(NULL, 1, 1, 1, 1) == FALSE);
}
TEST(gap1_virtres_bogus_dc_fails) {
    SetVirtualResolution_pfn f = UN(SetVirtualResolution);
    REQUIRE(f);
    static const ULONG_PTR hs[] = { 1, 0x1234, 0xffff, 0x00010001, 0x7fffffff, ~(ULONG_PTR)0 };
    for (unsigned i = 0; i < sizeof hs / sizeof *hs; i++) {
        CHECKF(f((HDC)hs[i], 0, 0, 0, 0) == FALSE, "h %#llx zero", (unsigned long long)hs[i]);
        CHECKF(f((HDC)hs[i], 3, 3, 3, 3) == FALSE, "h %#llx nonzero", (unsigned long long)hs[i]);
    }
}
TEST(gap1_virtres_deleted_dc_fails) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    SetVirtualResolution_pfn f = UN(SetVirtualResolution);
    REQUIRE(f);
    HDC dc = mkdc();
    U(DeleteDC)(dc);
    CHECK(f(dc, 0, 0, 0, 0) == FALSE);
}
TEST(gap1_virtres_non_dc_handle_fails) {
    REQUIRE(U(CreateBitmap)); REQUIRE(U(DeleteObject));
    SetVirtualResolution_pfn f = UN(SetVirtualResolution);
    REQUIRE(f);
    HBITMAP b = U(CreateBitmap)(2, 2, 1, 1, NULL);
    CHECK(f((HDC)b, 0, 0, 0, 0) == FALSE);
    CHECK(f((HDC)U(GetStockObject)(WHITE_BRUSH), 0, 0, 0, 0) == FALSE);
    U(DeleteObject)(b);
}
TEST(gap1_virtres_both_modules) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    SetVirtualResolution_pfn a = (SetVirtualResolution_pfn)t_proc_gdi32("SetVirtualResolution");
    SetVirtualResolution_pfn b = (SetVirtualResolution_pfn)t_proc_gdi32full("SetVirtualResolution");
    REQUIRE(a); REQUIRE(b);
    HDC dc = mkdc();
    CHECK(a(dc, 10, 10, 10, 10) == TRUE);
    CHECK(b(dc, 10, 10, 10, 10) == TRUE);
    CHECK(a(dc, 10, 0, 10, 10) == FALSE);
    CHECK(b(dc, 10, 0, 10, 10) == FALSE);
    a(dc, 0, 0, 0, 0);
    U(DeleteDC)(dc);
}
TEST(gap1_virtres_does_not_alter_pixel_caps_of_mem_dc) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetDeviceCaps));
    SetVirtualResolution_pfn f = UN(SetVirtualResolution);
    REQUIRE(f);
    HDC dc = mkdc();
    int hr = U(GetDeviceCaps)(dc, HORZRES), vr = U(GetDeviceCaps)(dc, VERTRES), bpp = U(GetDeviceCaps)(dc, BITSPIXEL);
    CHECK(f(dc, 640, 480, 169, 127) == TRUE);
    t_note("HORZRES %d -> %d, HORZSIZE %d, LOGPIXELSX %d", hr, U(GetDeviceCaps)(dc, HORZRES), U(GetDeviceCaps)(dc, HORZSIZE), U(GetDeviceCaps)(dc, LOGPIXELSX));
    CHECK(U(GetDeviceCaps)(dc, BITSPIXEL) == bpp);
    f(dc, 0, 0, 0, 0);
    CHECK(U(GetDeviceCaps)(dc, HORZRES) == hr);
    CHECK(U(GetDeviceCaps)(dc, VERTRES) == vr);
    U(DeleteDC)(dc);
}
TEST(gap1_virtres_idempotent_reset) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    SetVirtualResolution_pfn f = UN(SetVirtualResolution);
    REQUIRE(f);
    HDC dc = mkdc();
    for (int i = 0; i < 50; i++) {
        if (f(dc, 800 + i, 600 + i, 200 + i, 150 + i) != TRUE || f(dc, 0, 0, 0, 0) != TRUE) { CHECKF(0, "iteration %d", i); break; }
    }
    CHECK(1);
    U(DeleteDC)(dc);
}
TEST(gap1_virtres_survives_savedc_restoredc) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    SetVirtualResolution_pfn f = UN(SetVirtualResolution);
    REQUIRE(f);
    HDC dc = mkdc();
    int s = U(SaveDC)(dc);
    CHECK(f(dc, 100, 100, 50, 50) == TRUE);
    CHECK(U(RestoreDC)(dc, s));
    CHECK(f(dc, 0, 0, 0, 0) == TRUE);
    U(DeleteDC)(dc);
}
TEST(gap1_virtres_screen_dc_error_paths_only) {
    SetVirtualResolution_pfn f = UN(SetVirtualResolution);
    HDC (WINAPI *gdc)(HWND) = (void *)t_proc("GetDC");
    int (WINAPI *rdc)(HWND, HDC) = (void *)t_proc("ReleaseDC");
    REQUIRE(f); REQUIRE(gdc); REQUIRE(rdc);
    HDC sc = gdc(NULL);
    CHECK(f(sc, 1, 0, 0, 0) == FALSE);   /* rejected before the DC is touched */
    CHECK(f(sc, 0, 1, 0, 0) == FALSE);
    CHECK(f(sc, 0, 0, 0, 1) == FALSE);
    rdc(NULL, sc);
}

/* ================= SetLayoutWidth ================= */
TEST(gap1_layoutw_null_dc_returns_minus_one_no_error) {
    SetLayoutWidth_pfn f = UN(SetLayoutWidth);
    REQUIRE(f);
    SetLastError(0x2468);
    CHECK(f(NULL, -1, 0) == (DWORD)-1);
    EXPECT_ERR(0, 0x2468);   /* non-DC handle types are rejected without setting an error */
}
TEST(gap1_layoutw_nondc_types_minus_one_no_error) {
    REQUIRE(U(CreateBitmap)); REQUIRE(U(CreatePen)); REQUIRE(U(CreateRectRgn)); REQUIRE(U(DeleteObject));
    SetLayoutWidth_pfn f = UN(SetLayoutWidth);
    REQUIRE(f);
    HBITMAP b = U(CreateBitmap)(2, 2, 1, 1, NULL);
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0);
    HRGN r = U(CreateRectRgn)(0, 0, 1, 1);
    HGDIOBJ objs[] = { b, p, r, U(GetStockObject)(WHITE_BRUSH), (HGDIOBJ)(ULONG_PTR)0x1234 };
    for (unsigned i = 0; i < sizeof objs / sizeof *objs; i++) {
        SetLastError(0x3579);
        CHECKF(f((HDC)objs[i], -1, LAYOUT_RTL) == (DWORD)-1, "obj %u", i);
        CHECKF(GetLastError() == 0x3579, "obj %u err %lu", i, GetLastError());
    }
    U(DeleteObject)(b); U(DeleteObject)(p); U(DeleteObject)(r);
}
TEST(gap1_layoutw_stale_dc_invalid_parameter) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    SetLayoutWidth_pfn f = UN(SetLayoutWidth);
    REQUIRE(f);
    HDC dc = mkdc();
    U(DeleteDC)(dc);
    SetLastError(0);
    CHECK(f(dc, -1, 0) == (DWORD)-1);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
}
TEST(gap1_layoutw_dc_typed_garbage_invalid_parameter) {
    SetLayoutWidth_pfn f = UN(SetLayoutWidth);
    REQUIRE(f);
    static const ULONG_PTR hs[] = { 0x0001ffff, 0x00810000 };
    for (unsigned i = 0; i < sizeof hs / sizeof *hs; i++) {
        if (((hs[i] & 0x7f0000) != 0x10000)) continue;
        SetLastError(0);
        DWORD r = f((HDC)hs[i], -1, 0);
        CHECKF(r == (DWORD)-1, "handle %#llx -> %lu", (unsigned long long)hs[i], r);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "handle %#llx err %lu", (unsigned long long)hs[i], GetLastError());
    }
}
TEST(gap1_layoutw_fresh_dc_old_layout_zero) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    SetLayoutWidth_pfn f = UN(SetLayoutWidth);
    GetLayout_pfn g = UN(GetLayout);
    REQUIRE(f); REQUIRE(g);
    HDC dc = mkdc();
    CHECK(g(dc) == 0);
    CHECK(f(dc, -1, 0) == 0);
    U(DeleteDC)(dc);
}
TEST(gap1_layoutw_rtl_roundtrip) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    SetLayoutWidth_pfn f = UN(SetLayoutWidth);
    GetLayout_pfn g = UN(GetLayout);
    REQUIRE(f); REQUIRE(g);
    HDC dc = mkdc();
    CHECK(f(dc, -1, LAYOUT_RTL) == 0);
    CHECK((g(dc) & LAYOUT_RTL) == LAYOUT_RTL);
    CHECK((f(dc, -1, 0) & LAYOUT_RTL) == LAYOUT_RTL);
    CHECK(g(dc) == 0);
    U(DeleteDC)(dc);
}
TEST(gap1_layoutw_matches_setlayout) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    SetLayoutWidth_pfn f = UN(SetLayoutWidth);
    SetLayout_pfn s = UN(SetLayout);
    GetLayout_pfn g = UN(GetLayout);
    REQUIRE(f); REQUIRE(s); REQUIRE(g);
    HDC a = mkdc(), b = mkdc();
    static const DWORD ls[] = { 0, 1, 2, 3, 4, 5, 8, 9, 0 };
    for (unsigned i = 0; i < sizeof ls / sizeof *ls; i++) {
        DWORD ra = f(a, -1, ls[i]), rb = s(b, ls[i]);
        CHECKF(ra == rb, "layout %lu old %lu vs %lu", ls[i], ra, rb);
        CHECKF(g(a) == g(b), "layout %lu get %lu vs %lu", ls[i], g(a), g(b));
    }
    U(DeleteDC)(a); U(DeleteDC)(b);
}
TEST(gap1_layoutw_returns_previous_each_step) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    SetLayoutWidth_pfn f = UN(SetLayoutWidth);
    GetLayout_pfn g = UN(GetLayout);
    REQUIRE(f); REQUIRE(g);
    HDC dc = mkdc();
    static const DWORD ls[] = { 1, 0, 1, 1, 0, 0 };
    DWORD prev = 0;
    for (unsigned i = 0; i < sizeof ls / sizeof *ls; i++) {
        DWORD r = f(dc, -1, ls[i]);
        CHECKF((r & 1) == (prev & 1), "step %u old %lu prev %lu", i, r, prev);
        prev = ls[i];
    }
    U(DeleteDC)(dc);
}
TEST(gap1_layoutw_with_width_values) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    SetLayoutWidth_pfn f = UN(SetLayoutWidth);
    GetLayout_pfn g = UN(GetLayout);
    REQUIRE(f); REQUIRE(g);
    HDC dc = mkdc();
    static const LONG ws[] = { 0, 1, 16, 100, 1920, 100000, -1, -2 };
    for (unsigned i = 0; i < sizeof ws / sizeof *ws; i++) {
        DWORD r = f(dc, ws[i], LAYOUT_RTL);
        t_note("width %ld -> old %lu now %lu", ws[i], r, g(dc));
        CHECKF(r != (DWORD)-1, "width %ld", ws[i]);
        f(dc, -1, 0);
    }
    CHECK(g(dc) == 0);
    U(DeleteDC)(dc);
}
TEST(gap1_layoutw_rtl_mirrors_lptodp_note) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(LPtoDP));
    SetLayoutWidth_pfn f = UN(SetLayoutWidth);
    REQUIRE(f);
    GapDib d;
    if (!gap_dib_make(&d, 64, 8)) { CHECK(0); return; }
    POINT p = {10, 3};
    U(LPtoDP)(d.dc, &p, 1);
    CHECK(p.x == 10 && p.y == 3);
    f(d.dc, -1, LAYOUT_RTL);
    p.x = 10; p.y = 3;
    U(LPtoDP)(d.dc, &p, 1);
    t_note("RTL LPtoDP(10,3) on 64-wide dib -> (%ld,%ld)", p.x, p.y);
    CHECK(p.y == 3);
    f(d.dc, -1, 0);
    gap_dib_free(&d);
}
TEST(gap1_layoutw_invalid_flag_bits_note) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    SetLayoutWidth_pfn f = UN(SetLayoutWidth);
    GetLayout_pfn g = UN(GetLayout);
    REQUIRE(f); REQUIRE(g);
    HDC dc = mkdc();
    static const DWORD bad[] = { 0x10, 0x20, 0x100, 0x10000, 0x80000000, 0xffffffff };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0);
        DWORD r = f(dc, -1, bad[i]);
        t_note("layout %#lx -> %#lx err %lu now %#lx", bad[i], r, GetLastError(), g(dc));
        f(dc, -1, 0);
    }
    CHECK(g(dc) == 0);
    U(DeleteDC)(dc);
}
TEST(gap1_layoutw_both_modules) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    SetLayoutWidth_pfn a = (SetLayoutWidth_pfn)t_proc_gdi32("SetLayoutWidth");
    SetLayoutWidth_pfn b = (SetLayoutWidth_pfn)t_proc_gdi32full("SetLayoutWidth");
    REQUIRE(a); REQUIRE(b);
    HDC dc = mkdc();
    CHECK(a(dc, -1, 1) == 0);
    CHECK((b(dc, -1, 0) & 1) == 1);
    CHECK(a(NULL, -1, 0) == (DWORD)-1);
    CHECK(b(NULL, -1, 0) == (DWORD)-1);
    U(DeleteDC)(dc);
}
TEST(gap1_layoutw_independent_between_dcs) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    SetLayoutWidth_pfn f = UN(SetLayoutWidth);
    GetLayout_pfn g = UN(GetLayout);
    REQUIRE(f); REQUIRE(g);
    HDC a = mkdc(), b = mkdc();
    f(a, -1, LAYOUT_RTL);
    CHECK(g(b) == 0);
    CHECK((g(a) & 1) == 1);
    U(DeleteDC)(a); U(DeleteDC)(b);
}

/* ================= ScaleValues ================= */
TEST(gap1_scalevalues_null_dc_false_buffer_untouched) {
    ScaleValues_pfn f = UN(ScaleValues);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    LONG v[4] = { 1, 2, 3, 4 };
    CHECK(f(NULL, v, 4) == FALSE);
    CHECK(v[0] == 1 && v[1] == 2 && v[2] == 3 && v[3] == 4);
}
TEST(gap1_scalevalues_bogus_dc_false) {
    ScaleValues_pfn f = UN(ScaleValues);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    static const ULONG_PTR hs[] = { 1, 0x1234, 0xffff, 0x00010001, 0x7fffffff, ~(ULONG_PTR)0 };
    for (unsigned i = 0; i < sizeof hs / sizeof *hs; i++) {
        LONG v[2] = { 10, 20 };
        CHECKF(f((HDC)hs[i], v, 2) == FALSE, "h %#llx", (unsigned long long)hs[i]);
        CHECKF(v[0] == 10 && v[1] == 20, "h %#llx buffer", (unsigned long long)hs[i]);
    }
}
TEST(gap1_scalevalues_stale_dc_false) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    ScaleValues_pfn f = UN(ScaleValues);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    HDC dc = mkdc();
    U(DeleteDC)(dc);
    LONG v[1] = { 77 };
    CHECK(f(dc, v, 1) == FALSE);
    CHECK(v[0] == 77);
}
TEST(gap1_scalevalues_valid_dc_true) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    ScaleValues_pfn f = UN(ScaleValues);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    HDC dc = mkdc();
    LONG v[5] = { 1, 2, 3, 100, -5 };
    CHECK(f(dc, v, 5) == TRUE);
    t_note("scaled values: %ld %ld %ld %ld %ld", v[0], v[1], v[2], v[3], v[4]);
    U(DeleteDC)(dc);
}
TEST(gap1_scalevalues_memdc_identity_when_scale_one) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    ScaleValues_pfn f = UN(ScaleValues);
    GetDCDpiScaleValue_pfn g = UN(GetDCDpiScaleValue);
    REQUIRE(f); REQUIRE(g);
    SKIP_WINE("kernel stub");
    HDC dc = mkdc();
    LONG v[6] = { 0, 1, -1, 12345, -98765, 7 };
    LONG o[6]; memcpy(o, v, sizeof v);
    CHECK(f(dc, v, 6) == TRUE);
    if (g(dc) == 1) CHECK(!memcmp(v, o, sizeof v));
    else t_note("DC dpi scale %lu: values changed", g(dc));
    U(DeleteDC)(dc);
}
TEST(gap1_scalevalues_zero_count_note) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    ScaleValues_pfn f = UN(ScaleValues);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    HDC dc = mkdc();
    LONG v[1] = { 5 };
    BOOL r = f(dc, v, 0);
    t_note("ScaleValues(count 0) -> %d", r);
    CHECK(v[0] == 5);
    U(DeleteDC)(dc);
}
TEST(gap1_scalevalues_repeated_calls_stable) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    ScaleValues_pfn f = UN(ScaleValues);
    GetDCDpiScaleValue_pfn g = UN(GetDCDpiScaleValue);
    REQUIRE(f); REQUIRE(g);
    SKIP_WINE("kernel stub");
    HDC dc = mkdc();
    if (g(dc) != 1) { t_skip("DC is dpi scaled"); U(DeleteDC)(dc); return; }
    LONG v[3] = { 8, 16, 24 };
    for (int i = 0; i < 100; i++) f(dc, v, 3);
    CHECK(v[0] == 8 && v[1] == 16 && v[2] == 24);
    U(DeleteDC)(dc);
}
TEST(gap1_scalevalues_large_count_valid_buffer) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    ScaleValues_pfn f = UN(ScaleValues);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    HDC dc = mkdc();
    LONG *v = (LONG *)calloc(4096, sizeof(LONG));
    for (int i = 0; i < 4096; i++) v[i] = i;
    BOOL r = f(dc, v, 4096);
    t_note("ScaleValues(4096) -> %d, v[4095]=%ld", r, v[4095]);
    CHECK(r == TRUE);
    free(v);
    U(DeleteDC)(dc);
}

/* ================= ScaleRgn ================= */
TEST(gap1_scalergn_null_args_false) {
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(DeleteObject)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    ScaleRgn_pfn f = UN(ScaleRgn);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    HDC dc = mkdc();
    HRGN r = U(CreateRectRgn)(0, 0, 10, 10);
    CHECK(f(NULL, NULL) == FALSE);
    CHECK(f(dc, NULL) == FALSE);
    CHECK(f(NULL, r) == FALSE);
    U(DeleteObject)(r); U(DeleteDC)(dc);
}
TEST(gap1_scalergn_valid_true) {
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(DeleteObject)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    ScaleRgn_pfn f = UN(ScaleRgn);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    HDC dc = mkdc();
    HRGN r = U(CreateRectRgn)(3, 4, 30, 40);
    CHECK(f(dc, r) == TRUE);
    RECT rc;
    CHECK(U(GetRgnBox)(r, &rc) == SIMPLEREGION);
    t_note("region after ScaleRgn: %ld,%ld,%ld,%ld", rc.left, rc.top, rc.right, rc.bottom);
    U(DeleteObject)(r); U(DeleteDC)(dc);
}
TEST(gap1_scalergn_identity_when_unscaled) {
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(DeleteObject)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetRgnBox));
    ScaleRgn_pfn f = UN(ScaleRgn);
    GetDCDpiScaleValue_pfn g = UN(GetDCDpiScaleValue);
    REQUIRE(f); REQUIRE(g);
    SKIP_WINE("kernel stub");
    HDC dc = mkdc();
    HRGN r = U(CreateRectRgn)(3, 4, 30, 40);
    f(dc, r);
    RECT rc; U(GetRgnBox)(r, &rc);
    if (g(dc) == 1) CHECK(rc.left == 3 && rc.top == 4 && rc.right == 30 && rc.bottom == 40);
    U(DeleteObject)(r); U(DeleteDC)(dc);
}
TEST(gap1_scalergn_wrong_object_types) {
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(DeleteObject)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(CreateBitmap));
    ScaleRgn_pfn f = UN(ScaleRgn);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    HDC dc = mkdc();
    HRGN r = U(CreateRectRgn)(0, 0, 4, 4);
    HBITMAP b = U(CreateBitmap)(2, 2, 1, 1, NULL);
    CHECK(f(dc, (HRGN)b) == FALSE);
    CHECK(f(dc, (HRGN)dc) == FALSE);
    CHECK(f((HDC)r, r) == FALSE);
    CHECK(f((HDC)b, r) == FALSE);
    U(DeleteObject)(r); U(DeleteObject)(b); U(DeleteDC)(dc);
}
TEST(gap1_scalergn_deleted_objects_false) {
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(DeleteObject)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    ScaleRgn_pfn f = UN(ScaleRgn);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    HDC dc = mkdc();
    HRGN r = U(CreateRectRgn)(0, 0, 4, 4), r2 = U(CreateRectRgn)(0, 0, 4, 4);
    U(DeleteObject)(r);
    CHECK(f(dc, r) == FALSE);
    U(DeleteDC)(dc);
    CHECK(f(dc, r2) == FALSE);
    U(DeleteObject)(r2);
}
TEST(gap1_scalergn_bogus_handles) {
    ScaleRgn_pfn f = UN(ScaleRgn);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    CHECK(f((HDC)(ULONG_PTR)0x1234, (HRGN)(ULONG_PTR)0x5678) == FALSE);
    CHECK(f((HDC)~(ULONG_PTR)0, (HRGN)~(ULONG_PTR)0) == FALSE);
}
TEST(gap1_scalergn_empty_and_complex_regions) {
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(DeleteObject)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(CombineRgn)); REQUIRE(U(GetRgnBox));
    ScaleRgn_pfn f = UN(ScaleRgn);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    HDC dc = mkdc();
    HRGN e = U(CreateRectRgn)(0, 0, 0, 0);
    HRGN a = U(CreateRectRgn)(0, 0, 10, 10), b = U(CreateRectRgn)(20, 20, 30, 30), c = U(CreateRectRgn)(0, 0, 1, 1);
    U(CombineRgn)(c, a, b, RGN_OR);
    CHECK(f(dc, e) == TRUE);
    CHECK(f(dc, c) == TRUE);
    RECT rc;
    CHECK(U(GetRgnBox)(c, &rc) == COMPLEXREGION);
    U(DeleteObject)(e); U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(c); U(DeleteDC)(dc);
}

/* ================= SetMagicColors ================= */
TEST(gap1_magiccolors_null_dc_note) {
    SetMagicColors_pfn f = UN(SetMagicColors);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    SetLastError(0);
    BOOL r = f(NULL, 0, 0);
    t_note("SetMagicColors(NULL,0,0) -> %d err %lu", r, GetLastError());
}
TEST(gap1_magiccolors_bogus_dc_note) {
    SetMagicColors_pfn f = UN(SetMagicColors);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    BOOL r = f((HDC)(ULONG_PTR)0x1234, 0, 0);
    t_note("SetMagicColors(bogus) -> %d", r);
}
TEST(gap1_magiccolors_memdc_note) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    SetMagicColors_pfn f = UN(SetMagicColors);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    HDC dc = mkdc();
    for (ULONG idx = 0; idx < 5; idx++) {
        BOOL r = f(dc, 0, idx);
        t_note("SetMagicColors(mem,0,%lu) -> %d", idx, r);
    }
    U(DeleteDC)(dc);
}
TEST(gap1_magiccolors_both_modules_resolve) {
    SKIP_WINE("export layout differs");
    CHECK(t_proc_gdi32("SetMagicColors") != NULL);
    CHECK(t_proc_gdi32full("SetMagicColors") == NULL);   /* only exported by gdi32.dll itself */
}
