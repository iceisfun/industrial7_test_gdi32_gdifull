#include "gap4.h"
/* Handle-table helpers: GdiFixUpHandle, GdiGetEntry, GdiGetCodePage, GdiIs*DC,
 * GdiHandleBeingTracked, GdiIsTrackingEnabled. */

static ULONG_PTR sext(HANDLE h) { return (ULONG_PTR)(LONG_PTR)(int)(ULONG_PTR)h; }

static ULONG_PTR res; static HBRUSH hb;
static DWORD WINAPI fix_thread(LPVOID x) { res = UN(GdiFixUpHandle)((DWORD)((ULONG_PTR)hb & 0xffff)); return 0; }
/* ---------- GdiFixUpHandle ---------- */
TEST(gap4_fixup_restores_real_handles) {
    RQ(GdiFixUpHandle);
    HBRUSH b = U(CreateSolidBrush)(1); HPEN p = U(CreatePen)(PS_SOLID, 1, 2);
    HRGN r = U(CreateRectRgn)(0, 0, 3, 3); HDC dc = U(CreateCompatibleDC)(NULL);
    HBITMAP bm = U(CreateBitmap)(4, 4, 1, 1, NULL);
    HFONT f = U(CreateFontW)(-10, 0, 0, 0, 400, 0, 0, 0, 1, 0, 0, 0, 0, L"Arial");
    HGDIOBJ all[] = { b, p, r, dc, bm, f };
    for (unsigned i = 0; i < 6; i++) {
        CHECK(all[i] != NULL);
        ULONG_PTR h = (ULONG_PTR)all[i];
        ULONG_PTR fx = UN(GdiFixUpHandle)((DWORD)(h & 0xffff));
        CHECKF(fx == sext(all[i]), "obj %u h=%p fix=0x%llx", i, all[i], (unsigned long long)fx);
        /* idempotent */
        EXPECT_EQ(UN(GdiFixUpHandle)((DWORD)(h & 0xffff)), fx);
    }
    U(DeleteObject)(b); U(DeleteObject)(p); U(DeleteObject)(r); U(DeleteDC)(dc); U(DeleteObject)(bm); U(DeleteObject)(f);
}
TEST(gap4_fixup_full_handle_rejected) {
    RQ(GdiFixUpHandle);
    HBRUSH b = U(CreateSolidBrush)(1);
    REQUIRE(b);
    DWORD full = (DWORD)(ULONG_PTR)b;
    CHECK((full & 0xffff0000) != 0);
    EXPECT_EQ(UN(GdiFixUpHandle)(full), 0);
    EXPECT_EQ(UN(GdiFixUpHandle)(0x10000), 0);
    EXPECT_EQ(UN(GdiFixUpHandle)(0xffffffff), 0);
    EXPECT_EQ(UN(GdiFixUpHandle)(0x80000000), 0);
    U(DeleteObject)(b);
}
TEST(gap4_fixup_stock_objects) {
    RQ(GdiFixUpHandle);
    for (int i = 0; i <= 19; i++) {
        HGDIOBJ o = U(GetStockObject)(i);
        if (!o) continue;
        ULONG_PTR fx = UN(GdiFixUpHandle)((DWORD)((ULONG_PTR)o & 0xffff));
        CHECKF(fx == sext(o), "stock %d %p -> 0x%llx", i, o, (unsigned long long)fx);
    }
}
TEST(gap4_fixup_result_is_sign_extended_32bit) {
    RQ(GdiFixUpHandle);
    HBRUSH b = U(CreateSolidBrush)(1); REQUIRE(b);
    ULONG_PTR fx = UN(GdiFixUpHandle)((DWORD)((ULONG_PTR)b & 0xffff));
    CHECK(fx == sext((HANDLE)fx));
    U(DeleteObject)(b);
}
TEST(gap4_fixup_after_delete_note) {
    RQ(GdiFixUpHandle);
    HBRUSH b = U(CreateSolidBrush)(1); REQUIRE(b);
    ULONG_PTR live = UN(GdiFixUpHandle)((DWORD)((ULONG_PTR)b & 0xffff));
    U(DeleteObject)(b);
    ULONG_PTR dead = UN(GdiFixUpHandle)((DWORD)((ULONG_PTR)b & 0xffff));
    t_note("fixup live=0x%llx after delete=0x%llx", (unsigned long long)live, (unsigned long long)dead);
}
TEST(gap4_fixup_distinct_per_object) {
    RQ(GdiFixUpHandle);
    HBRUSH a = U(CreateSolidBrush)(1), b = U(CreateSolidBrush)(2); REQUIRE(a && b);
    ULONG_PTR fa = UN(GdiFixUpHandle)((DWORD)((ULONG_PTR)a & 0xffff)), fb = UN(GdiFixUpHandle)((DWORD)((ULONG_PTR)b & 0xffff));
    CHECK(fa != fb);
    U(DeleteObject)(a); U(DeleteObject)(b);
}
TEST(gap4_fixup_cross_thread) {
    RQ(GdiFixUpHandle);
    HBRUSH b = U(CreateSolidBrush)(1); REQUIRE(b);
    static ULONG_PTR res;     hb = b; res = 0;
    HANDLE th = CreateThread(NULL, 0, fix_thread, NULL, 0, NULL);
    WaitForSingleObject(th, 10000); CloseHandle(th);
    EXPECT_EQ(res, sext(b));
    U(DeleteObject)(b);
}
/* ---------- GdiGetEntry ---------- */
TEST(gap4_getentry_dc_fields) {
    RQ(GdiGetEntry);
    HDC dc = U(CreateCompatibleDC)(NULL); REQUIRE(dc);
    G4ENTRY e; memset(&e, 0xcc, sizeof e);
    LONG r = UN(GdiGetEntry)((ULONG_PTR)dc & 0xffff, &e);
    EXPECT_EQ(r, 0);
    CHECKF((e.pid & ~1u) == (GetCurrentProcessId() & ~1u), "pid field 0x%lx", e.pid);
    CHECKF(e.upper == (WORD)(((ULONG_PTR)dc >> 16) & 0xffff), "upper 0x%x", e.upper);
    CHECKF(e.objt == 1, "objt %u", e.objt);
    CHECK(e.pUser != NULL);
    U(DeleteDC)(dc);
}
TEST(gap4_getentry_fills_24_bytes_only) {
    RQ(GdiGetEntry);
    HDC dc = U(CreateCompatibleDC)(NULL); REQUIRE(dc);
    BYTE buf[64]; memset(buf, 0xab, sizeof buf);
    EXPECT_EQ(UN(GdiGetEntry)((ULONG_PTR)dc & 0xffff, buf), 0);
    int ok = 1; for (int i = 24; i < 64; i++) if (buf[i] != 0xab) ok = 0;
    CHECK(ok);
    U(DeleteDC)(dc);
}
TEST(gap4_getentry_repeatable) {
    RQ(GdiGetEntry);
    HDC dc = U(CreateCompatibleDC)(NULL); REQUIRE(dc);
    G4ENTRY a, b;
    UN(GdiGetEntry)((ULONG_PTR)dc & 0xffff, &a); UN(GdiGetEntry)((ULONG_PTR)dc & 0xffff, &b);
    CHECK(a.pid == b.pid && a.upper == b.upper && a.objt == b.objt && a.pUser == b.pUser);
    U(DeleteDC)(dc);
}
TEST(gap4_getentry_brush_not_dc_type) {
    RQ(GdiGetEntry);
    HBRUSH b = U(CreateSolidBrush)(3); REQUIRE(b);
    G4ENTRY e; memset(&e, 0, sizeof e);
    EXPECT_EQ(UN(GdiGetEntry)((ULONG_PTR)b & 0xffff, &e), 0);
    CHECK((e.pid & ~1u) == (GetCurrentProcessId() & ~1u));
    CHECK(e.upper == (WORD)(((ULONG_PTR)b >> 16) & 0xffff));
    t_note("brush entry objt=%u flags=0x%x pUser=%p", e.objt, e.flags, e.pUser);
    U(DeleteObject)(b);
}
TEST(gap4_getentry_deleted_dc_changes_entry) {
    RQ(GdiGetEntry);
    HDC dc = U(CreateCompatibleDC)(NULL); REQUIRE(dc);
    G4ENTRY e; UN(GdiGetEntry)((ULONG_PTR)dc & 0xffff, &e);
    U(DeleteDC)(dc);
    G4ENTRY d; memset(&d, 0, sizeof d);
    UN(GdiGetEntry)((ULONG_PTR)dc & 0xffff, &d);
    SOFT(d.objt != 1 || d.upper != e.upper);
}
TEST(gap4_getentry_full_handle_note) {
    RQ(GdiGetEntry);
    HDC dc = U(CreateCompatibleDC)(NULL); REQUIRE(dc);
    G4ENTRY e; memset(&e, 0, sizeof e);
    LONG r = UN(GdiGetEntry)((ULONG_PTR)dc, &e);
    t_note("GdiGetEntry(full handle %p)=0x%lx objt=%u upper=0x%x", dc, r, e.objt, e.upper);
    U(DeleteDC)(dc);
}

/* ---------- GdiGetCodePage ---------- */
TEST(gap4_codepage_invalid_handles_return_acp) {
    RQ(GdiGetCodePage);
    UINT acp = GetACP();
    HBRUSH b = U(CreateSolidBrush)(1);
    HDC dead = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dead);
    HANDLE bad[] = { NULL, b, dead };
    for (unsigned i = 0; i < 3; i++) {
        SET_ERR(0);
        UINT r = UN(GdiGetCodePage)(bad[i]);
        CHECKF(r == acp, "case %u: got %u want ACP %u", i, r, acp);
        EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    }
    U(DeleteObject)(b);
}
TEST(gap4_codepage_valid_memdc_no_error) {
    RQ(GdiGetCodePage);
    HDC dc = U(CreateCompatibleDC)(NULL); REQUIRE(dc);
    SET_ERR(0x5555);
    UINT r = UN(GdiGetCodePage)(dc);
    EXPECT_EQ(GetLastError(), 0x5555);
    t_note("GdiGetCodePage(default memdc)=%u acp=%u", r, GetACP());
    U(DeleteDC)(dc);
}
TEST(gap4_codepage_follows_font_charset) {
    RQ(GdiGetCodePage);
    HDC dc = U(CreateCompatibleDC)(NULL); REQUIRE(dc);
    struct { BYTE cs; const WCHAR *face; } c[] = { {ANSI_CHARSET, L"Arial"}, {SYMBOL_CHARSET, L"Symbol"}, {SHIFTJIS_CHARSET, L"Arial"}, {GREEK_CHARSET, L"Arial"}, {RUSSIAN_CHARSET, L"Arial"} };
    UINT vals[5];
    for (int i = 0; i < 5; i++) {
        HFONT f = U(CreateFontW)(-12, 0, 0, 0, 400, 0, 0, 0, c[i].cs, 0, 0, 0, 0, c[i].face);
        HGDIOBJ old = U(SelectObject)(dc, f);
        SET_ERR(0x7777);
        vals[i] = UN(GdiGetCodePage)(dc);
        CHECKF(GetLastError() == 0x7777, "charset %u error", c[i].cs);
        t_note("charset %u -> GdiGetCodePage=%u (GetTextCharset=%d)", c[i].cs, vals[i], U(GetTextCharset)(dc));
        U(SelectObject)(dc, old); U(DeleteObject)(f);
    }
    U(DeleteDC)(dc);
}
TEST(gap4_codepage_window_dc) {
    RQ(GdiGetCodePage);
    HDC dc = U(GetDC)(NULL); REQUIRE(dc);
    SET_ERR(0x5555);
    UN(GdiGetCodePage)(dc);
    EXPECT_EQ(GetLastError(), 0x5555);
    U(ReleaseDC)(NULL, dc);
}
TEST(gap4_codepage_repeat_stable) {
    RQ(GdiGetCodePage);
    HDC dc = U(CreateCompatibleDC)(NULL); REQUIRE(dc);
    UINT a = UN(GdiGetCodePage)(dc);
    for (int i = 0; i < 1000; i++) if (UN(GdiGetCodePage)(dc) != a) { CHECK(0); break; }
    U(DeleteDC)(dc);
}

/* ---------- GdiIsScreenDC ---------- */
TEST(gap4_isscreendc_screen_dc) {
    RQ(GdiIsScreenDC);
    HDC dc = U(GetDC)(NULL); REQUIRE(dc);
    EXPECT_EQ(UN(GdiIsScreenDC)(dc), 1);
    U(ReleaseDC)(NULL, dc);
}
TEST(gap4_isscreendc_createdc_display) {
    RQ(GdiIsScreenDC);
    HDC dc = U(CreateDCW)(L"DISPLAY", NULL, NULL, NULL); REQUIRE(dc);
    EXPECT_EQ(UN(GdiIsScreenDC)(dc), 1);
    U(DeleteDC)(dc);
}
TEST(gap4_isscreendc_window_dc) {
    RQ(GdiIsScreenDC);
    HWND w = t_make_window(NULL, WS_OVERLAPPEDWINDOW, 0, 0, 0, 100, 100, NULL);
    if (!w) { t_skip("no window"); return; }
    HDC dc = U(GetDC)(w);
    if (dc) { SOFT(UN(GdiIsScreenDC)(dc) == 1); U(ReleaseDC)(w, dc); }
    DestroyWindow(w);
}
TEST(gap4_isscreendc_memory_dcs_are_not) {
    RQ(GdiIsScreenDC);
    HDC m = U(CreateCompatibleDC)(NULL); REQUIRE(m);
    EXPECT_EQ(UN(GdiIsScreenDC)(m), 0);
    HDC scr = U(GetDC)(NULL);
    HDC m2 = U(CreateCompatibleDC)(scr);
    EXPECT_EQ(UN(GdiIsScreenDC)(m2), 0);
    HBITMAP bm = U(CreateCompatibleBitmap)(scr, 8, 8);
    HGDIOBJ old = U(SelectObject)(m2, bm);
    EXPECT_EQ(UN(GdiIsScreenDC)(m2), 0);
    U(SelectObject)(m2, old); U(DeleteObject)(bm);
    U(DeleteDC)(m2); U(ReleaseDC)(NULL, scr); U(DeleteDC)(m);
}
TEST(gap4_isscreendc_dibsection_dc) {
    RQ(GdiIsScreenDC);
    HDC m = U(CreateCompatibleDC)(NULL); REQUIRE(m);
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = 40; bi.bmiHeader.biWidth = 4; bi.bmiHeader.biHeight = -4; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    void *bits; HBITMAP b = U(CreateDIBSection)(m, &bi, DIB_RGB_COLORS, &bits, NULL, 0); REQUIRE(b);
    HGDIOBJ old = U(SelectObject)(m, b);
    EXPECT_EQ(UN(GdiIsScreenDC)(m), 0);
    U(SelectObject)(m, old); U(DeleteObject)(b); U(DeleteDC)(m);
}
TEST(gap4_isscreendc_invalid_handles) {
    RQ(GdiIsScreenDC);
    EXPECT_EQ(UN(GdiIsScreenDC)(NULL), 0);
    EXPECT_EQ(UN(GdiIsScreenDC)((HDC)1), 0);
    EXPECT_EQ(UN(GdiIsScreenDC)((HDC)0x12345), 0);
    EXPECT_EQ(UN(GdiIsScreenDC)((HDC)(ULONG_PTR)-1), 0);
    HBRUSH b = U(CreateSolidBrush)(0);
    EXPECT_EQ(UN(GdiIsScreenDC)((HDC)b), 0);
    U(DeleteObject)(b);
    HDC dead = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dead);
    EXPECT_EQ(UN(GdiIsScreenDC)(dead), 0);
}
TEST(gap4_isscreendc_metafile_dc) {
    RQ(GdiIsScreenDC);
    HDC e = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL); REQUIRE(e);
    EXPECT_EQ(UN(GdiIsScreenDC)(e), 0);
    HENHMETAFILE h = U(CloseEnhMetaFile)(e); if (h) U(DeleteEnhMetaFile)(h);
}
TEST(gap4_isscreendc_no_leak_repeat) {
    RQ(GdiIsScreenDC);
    HDC m = U(CreateCompatibleDC)(NULL); REQUIRE(m);
    DWORD g = g4_gdi_count();
    for (int i = 0; i < 2000; i++) UN(GdiIsScreenDC)(m);
    EXPECT_EQ(g4_gdi_count(), g);
    U(DeleteDC)(m);
}
static HDC xt_dc; static int xt_res = -1;
static DWORD WINAPI th_screen(LPVOID p) { xt_res = UN(GdiIsScreenDC)(xt_dc); return 0; }
TEST(gap4_isscreendc_cross_thread) {
    RQ(GdiIsScreenDC);
    xt_dc = U(CreateDCW)(L"DISPLAY", NULL, NULL, NULL); REQUIRE(xt_dc);
    HANDLE th = CreateThread(NULL, 0, th_screen, NULL, 0, NULL);
    WaitForSingleObject(th, 10000); CloseHandle(th);
    EXPECT_EQ(xt_res, 1);
    U(DeleteDC)(xt_dc);
}

/* ---------- GdiIsMetaFileDC / GdiIsMetaPrintDC / GdiIsPlayMetafileDC ---------- */
TEST(gap4_ismetafiledc_plain_dcs_false) {
    RQ(GdiIsMetaFileDC);
    HDC m = U(CreateCompatibleDC)(NULL); HDC s = U(GetDC)(NULL);
    EXPECT_EQ(UN(GdiIsMetaFileDC)(m), 0);
    EXPECT_EQ(UN(GdiIsMetaFileDC)(s), 0);
    U(ReleaseDC)(NULL, s); U(DeleteDC)(m);
}
TEST(gap4_ismetafiledc_enh_true) {
    RQ(GdiIsMetaFileDC);
    HDC e = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL); REQUIRE(e);
    EXPECT_EQ(UN(GdiIsMetaFileDC)(e), 1);
    EXPECT_EQ(UN(GdiIsMetaFileDC)(e), 1);
    HENHMETAFILE h = U(CloseEnhMetaFile)(e); if (h) U(DeleteEnhMetaFile)(h);
}
TEST(gap4_ismetafiledc_wmf_true) {
    RQ(GdiIsMetaFileDC);
    HDC w = U(CreateMetaFileW)(NULL); REQUIRE(w);
    EXPECT_EQ(UN(GdiIsMetaFileDC)(w), 1);
    HMETAFILE h = U(CloseMetaFile)(w); if (h) U(DeleteMetaFile)(h);
}
TEST(gap4_ismetafiledc_with_ref_dc) {
    RQ(GdiIsMetaFileDC);
    HDC ref = U(GetDC)(NULL);
    RECT r = { 0, 0, 1000, 1000 };
    HDC e = U(CreateEnhMetaFileW)(ref, NULL, &r, L"a\0b\0"); REQUIRE(e);
    EXPECT_EQ(UN(GdiIsMetaFileDC)(e), 1);
    HENHMETAFILE h = U(CloseEnhMetaFile)(e); if (h) U(DeleteEnhMetaFile)(h);
    U(ReleaseDC)(NULL, ref);
}
TEST(gap4_ismetafiledc_after_close_false) {
    RQ(GdiIsMetaFileDC);
    HDC e = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL); REQUIRE(e);
    HENHMETAFILE h = U(CloseEnhMetaFile)(e);
    SET_ERR(0);
    EXPECT_EQ(UN(GdiIsMetaFileDC)(e), 0);
    t_note("closed metafile DC: lasterror=%lu", GetLastError());
    if (h) U(DeleteEnhMetaFile)(h);
}
TEST(gap4_ismetafiledc_invalid_null) {
    RQ(GdiIsMetaFileDC);
    SET_ERR(0);
    EXPECT_EQ(UN(GdiIsMetaFileDC)(NULL), 0);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
}
TEST(gap4_ismetafiledc_invalid_handles) {
    RQ(GdiIsMetaFileDC);
    ULONG_PTR v[] = { 1, 0x12345, 0x7fffffff, 0xffffffff, (ULONG_PTR)-1 };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        BOOL r = UN(GdiIsMetaFileDC)((HDC)v[i]);
        CHECKF(r == 0, "0x%llx -> %d", (unsigned long long)v[i], r);
    }
}
TEST(gap4_ismetafiledc_non_dc_objects) {
    RQ(GdiIsMetaFileDC);
    HBRUSH b = U(CreateSolidBrush)(0); HRGN r = U(CreateRectRgn)(0, 0, 1, 1); HBITMAP bm = U(CreateBitmap)(1, 1, 1, 1, NULL);
    EXPECT_EQ(UN(GdiIsMetaFileDC)((HDC)b), 0);
    EXPECT_EQ(UN(GdiIsMetaFileDC)((HDC)r), 0);
    EXPECT_EQ(UN(GdiIsMetaFileDC)((HDC)bm), 0);
    EXPECT_EQ(UN(GdiIsMetaFileDC)((HDC)U(GetStockObject)(WHITE_BRUSH)), 0);
    U(DeleteObject)(b); U(DeleteObject)(r); U(DeleteObject)(bm);
}
TEST(gap4_ismetafiledc_deleted_memdc) {
    RQ(GdiIsMetaFileDC);
    HDC m = U(CreateCompatibleDC)(NULL); U(DeleteDC)(m);
    EXPECT_EQ(UN(GdiIsMetaFileDC)(m), 0);
}
static HDC xt_mdc; static int xt_mres = -1;
static DWORD WINAPI th_meta(LPVOID p) { xt_mres = UN(GdiIsMetaFileDC)(xt_mdc); return 0; }
TEST(gap4_ismetafiledc_cross_thread) {
    RQ(GdiIsMetaFileDC);
    xt_mdc = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL); REQUIRE(xt_mdc);
    HANDLE th = CreateThread(NULL, 0, th_meta, NULL, 0, NULL);
    WaitForSingleObject(th, 10000); CloseHandle(th);
    t_note("IsMetaFileDC from other thread=%d", xt_mres);
    HENHMETAFILE h = U(CloseEnhMetaFile)(xt_mdc); if (h) U(DeleteEnhMetaFile)(h);
}
TEST(gap4_ismetaprintdc_plain_false) {
    RQ(GdiIsMetaPrintDC);
    HDC m = U(CreateCompatibleDC)(NULL); HDC s = U(GetDC)(NULL);
    EXPECT_EQ(UN(GdiIsMetaPrintDC)(m), 0);
    EXPECT_EQ(UN(GdiIsMetaPrintDC)(s), 0);
    U(ReleaseDC)(NULL, s); U(DeleteDC)(m);
}
TEST(gap4_ismetaprintdc_metafile_non_printer_false) {
    RQ(GdiIsMetaPrintDC);
    HDC e = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL); REQUIRE(e);
    EXPECT_EQ(UN(GdiIsMetaPrintDC)(e), 0);
    HENHMETAFILE h = U(CloseEnhMetaFile)(e); if (h) U(DeleteEnhMetaFile)(h);
    HDC w = U(CreateMetaFileW)(NULL); REQUIRE(w);
    EXPECT_EQ(UN(GdiIsMetaPrintDC)(w), 0);
    HMETAFILE hw = U(CloseMetaFile)(w); if (hw) U(DeleteMetaFile)(hw);
}
TEST(gap4_ismetaprintdc_null_sets_invalid_handle) {
    RQ(GdiIsMetaPrintDC);
    SET_ERR(0);
    EXPECT_EQ(UN(GdiIsMetaPrintDC)(NULL), 0);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
}
TEST(gap4_ismetaprintdc_invalid_handles) {
    RQ(GdiIsMetaPrintDC);
    ULONG_PTR v[] = { 1, 0x12345, 0x7fffffff, 0xffffffff, (ULONG_PTR)-1 };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        DWORD r = UN(GdiIsMetaPrintDC)((HDC)v[i]);
        CHECKF(r == 0, "0x%llx -> %lu", (unsigned long long)v[i], r);
    }
    HBRUSH b = U(CreateSolidBrush)(0);
    EXPECT_EQ(UN(GdiIsMetaPrintDC)((HDC)b), 0);
    U(DeleteObject)(b);
}
TEST(gap4_isplaymetafiledc_plain_false) {
    RQ(GdiIsPlayMetafileDC);
    HDC m = U(CreateCompatibleDC)(NULL); HDC s = U(GetDC)(NULL);
    EXPECT_EQ(UN(GdiIsPlayMetafileDC)(m), 0);
    EXPECT_EQ(UN(GdiIsPlayMetafileDC)(s), 0);
    U(ReleaseDC)(NULL, s); U(DeleteDC)(m);
}
TEST(gap4_isplaymetafiledc_invalid_handles) {
    RQ(GdiIsPlayMetafileDC);
    ULONG_PTR v[] = { 0, 1, 2, 0xffff, 0x10000, 0x12345, 0x7fffffff, 0xffffffff, (ULONG_PTR)-1 };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        DWORD r = UN(GdiIsPlayMetafileDC)((HDC)v[i]);
        CHECKF(r == 0, "0x%llx -> %lu", (unsigned long long)v[i], r);
    }
}
TEST(gap4_isplaymetafiledc_non_dc_and_deleted) {
    RQ(GdiIsPlayMetafileDC);
    HBRUSH b = U(CreateSolidBrush)(0);
    EXPECT_EQ(UN(GdiIsPlayMetafileDC)((HDC)b), 0);
    U(DeleteObject)(b);
    HDC dead = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dead);
    EXPECT_EQ(UN(GdiIsPlayMetafileDC)(dead), 0);
}
TEST(gap4_isplaymetafiledc_metafile_recording_dc_false) {
    RQ(GdiIsPlayMetafileDC);
    HDC e = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL); REQUIRE(e);
    EXPECT_EQ(UN(GdiIsPlayMetafileDC)(e), 0);
    HENHMETAFILE h = U(CloseEnhMetaFile)(e); if (h) U(DeleteEnhMetaFile)(h);
}
static int g_play_seen = -1, g_play_calls;
static int CALLBACK enum_cb(HDC dc, HANDLETABLE *ht, const ENHMETARECORD *r, int n, LPARAM lp) {
    if (g_play_seen < 0) g_play_seen = UN(GdiIsPlayMetafileDC)(dc) ? 1 : 0;
    g_play_calls++;
    return 1;
}
TEST(gap4_isplaymetafiledc_during_enum_note) {
    RQ(GdiIsPlayMetafileDC);
    HENHMETAFILE e = g4_make_emf(2); REQUIRE(e);
    HDC m = U(CreateCompatibleDC)(NULL);
    g_play_seen = -1; g_play_calls = 0;
    RECT rc = { 0, 0, 100, 100 };
    U(EnumEnhMetaFile)(m, e, enum_cb, NULL, &rc);
    CHECK(g_play_calls > 0);
    t_note("GdiIsPlayMetafileDC during EnumEnhMetaFile = %d", g_play_seen);
    EXPECT_EQ(UN(GdiIsPlayMetafileDC)(m), 0);
    U(DeleteDC)(m); U(DeleteEnhMetaFile)(e);
}
TEST(gap4_isplaymetafiledc_after_playenh_false) {
    RQ(GdiIsPlayMetafileDC);
    HENHMETAFILE e = g4_make_emf(2); REQUIRE(e);
    HDC m = U(CreateCompatibleDC)(NULL);
    HBITMAP bm = U(CreateBitmap)(64, 64, 1, 32, NULL);
    HGDIOBJ old = U(SelectObject)(m, bm);
    RECT rc = { 0, 0, 64, 64 };
    U(PlayEnhMetaFile)(m, e, &rc);
    EXPECT_EQ(UN(GdiIsPlayMetafileDC)(m), 0);
    U(SelectObject)(m, old); U(DeleteObject)(bm); U(DeleteDC)(m); U(DeleteEnhMetaFile)(e);
}

/* ---------- tracking ---------- */
TEST(gap4_istracking_returns_zero) {
    RQ(GdiIsTrackingEnabled);
    EXPECT_EQ(UN(GdiIsTrackingEnabled)(), 0);
    for (int i = 0; i < 100; i++) if (UN(GdiIsTrackingEnabled)() != 0) { CHECK(0); break; }
}
TEST(gap4_istracking_no_side_effects) {
    RQ(GdiIsTrackingEnabled);
    SET_ERR(0x2222); DWORD g = g4_gdi_count();
    UN(GdiIsTrackingEnabled)();
    EXPECT_EQ(GetLastError(), 0x2222); EXPECT_EQ(g4_gdi_count(), g);
}
TEST(gap4_handlebeingtracked_null) {
    RQ(GdiHandleBeingTracked);
    EXPECT_EQ(UN(GdiHandleBeingTracked)(NULL), 0);
}
TEST(gap4_handlebeingtracked_objects_untracked) {
    RQ(GdiHandleBeingTracked);
    HBRUSH b = U(CreateSolidBrush)(0); HDC m = U(CreateCompatibleDC)(NULL);
    int rb = UN(GdiHandleBeingTracked)(b), rm = UN(GdiHandleBeingTracked)(m);
    CHECKF(rb == 0, "brush tracked=%d", rb);
    CHECKF(rm == 0, "dc tracked=%d", rm);
    U(DeleteObject)(b); U(DeleteDC)(m);
}
TEST(gap4_handlebeingtracked_bogus) {
    RQ(GdiHandleBeingTracked);
    ULONG_PTR v[] = { 1, 0x12345, 0x7fffffff, (ULONG_PTR)-1 };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        int r = UN(GdiHandleBeingTracked)((void *)v[i]);
        CHECKF(r == 0, "0x%llx -> %d", (unsigned long long)v[i], r);
    }
}
TEST(gap4_handlebeingtracked_no_side_effects) {
    RQ(GdiHandleBeingTracked);
    SET_ERR(0x2323); UN(GdiHandleBeingTracked)((void *)0x1234);
    EXPECT_EQ(GetLastError(), 0x2323);
}
