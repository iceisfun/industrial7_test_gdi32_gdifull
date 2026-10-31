#include "cih.h"

/* Low-colour (<= 2 colours) memory DCs take a simplified ICM-mode path; high-colour DCs go through mscms. */
static HDC ci_mono_dc(void) { return U(CreateCompatibleDC) ? U(CreateCompatibleDC)(NULL) : NULL; }

/* ---------------- SetICMMode ---------------- */
TEST(color_icm_mode_null_dc) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode));
    static const int modes[] = { ICM_OFF, ICM_ON, ICM_QUERY, ICM_DONE_OUTSIDEDC, 0, 5 };
    for (unsigned i = 0; i < sizeof modes / sizeof *modes; i++) {
        SetLastError(0xdeadbeef);
        int r = U(SetICMMode)(NULL, modes[i]);
        CHECKF(r == 0, "mode=%d r=%d", modes[i], r);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "mode=%d err=%lu", modes[i], GetLastError());
    }
}
TEST(color_icm_mode_deleted_dc) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode));
    HDC d = ci_bogus_dc();
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(SetICMMode)(d, ICM_QUERY), 0);
    /* a stale DC handle still carries the DC type bits, so it is rejected by the entry lookup */
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
}
TEST(color_icm_mode_bogus_dc_values) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode));
    static const ULONG_PTR v[] = { 1, 0xdeadbeef, 0x00010001, 0xffffffff };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        SetLastError(0xdeadbeef);
        CHECKF(U(SetICMMode)((HDC)v[i], ICM_QUERY) == 0, "dc=%p", (void *)v[i]);
    }
}
TEST(color_icm_mode_wrong_object_type) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode)); REQUIRE(U(CreatePen)); REQUIRE(U(DeleteObject));
    HGDIOBJ pen = U(CreatePen)(PS_SOLID, 1, 0);
    EXPECT_EQ(U(SetICMMode)((HDC)pen, ICM_QUERY), 0);
    U(DeleteObject)(pen);
}
TEST(color_icm_mode_mono_dc_default_off) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode)); REQUIRE(U(DeleteDC));
    HDC d = ci_mono_dc();
    EXPECT_EQ(U(SetICMMode)(d, ICM_QUERY), ICM_OFF);
    U(DeleteDC)(d);
}
TEST(color_icm_mode_mono_dc_on_off_cycle) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode)); REQUIRE(U(DeleteDC));
    HDC d = ci_mono_dc();
    EXPECT_NE(U(SetICMMode)(d, ICM_ON), 0);
    EXPECT_EQ(U(SetICMMode)(d, ICM_QUERY), ICM_ON);
    EXPECT_NE(U(SetICMMode)(d, ICM_ON), 0);
    EXPECT_EQ(U(SetICMMode)(d, ICM_QUERY), ICM_ON);
    EXPECT_NE(U(SetICMMode)(d, ICM_OFF), 0);
    EXPECT_EQ(U(SetICMMode)(d, ICM_QUERY), ICM_OFF);
    EXPECT_NE(U(SetICMMode)(d, ICM_OFF), 0);
    EXPECT_EQ(U(SetICMMode)(d, ICM_QUERY), ICM_OFF);
    U(DeleteDC)(d);
}
TEST(color_icm_mode_mono_dc_done_outside) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode)); REQUIRE(U(DeleteDC));
    HDC d = ci_mono_dc();
    EXPECT_NE(U(SetICMMode)(d, ICM_DONE_OUTSIDEDC), 0);
    int q = U(SetICMMode)(d, ICM_QUERY);
    CHECKF(q == ICM_DONE_OUTSIDEDC || q == ICM_ON, "query=%d", q);
    EXPECT_NE(U(SetICMMode)(d, ICM_OFF), 0);
    EXPECT_EQ(U(SetICMMode)(d, ICM_QUERY), ICM_OFF);
    U(DeleteDC)(d);
}
TEST(color_icm_mode_mono_dc_invalid_modes) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode)); REQUIRE(U(DeleteDC));
    HDC d = ci_mono_dc();
    static const int modes[] = { 0, 5, 6, 7, 100, -1, -2, 0x7fffffff, (int)0x80000000 };
    for (unsigned i = 0; i < sizeof modes / sizeof *modes; i++) {
        int r = U(SetICMMode)(d, modes[i]);
        CHECKF(r == 0, "mode=%d r=%d", modes[i], r);
    }
    EXPECT_EQ(U(SetICMMode)(d, ICM_QUERY), ICM_OFF);
    U(DeleteDC)(d);
}
TEST(color_icm_mode_mono_dc_independent) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode)); REQUIRE(U(DeleteDC));
    HDC a = ci_mono_dc(), b = ci_mono_dc();
    U(SetICMMode)(a, ICM_ON);
    EXPECT_EQ(U(SetICMMode)(a, ICM_QUERY), ICM_ON);
    EXPECT_EQ(U(SetICMMode)(b, ICM_QUERY), ICM_OFF);
    U(SetICMMode)(a, ICM_OFF);
    U(DeleteDC)(a); U(DeleteDC)(b);
}
TEST(color_icm_mode_highcolor_default_off) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    EXPECT_EQ(U(SetICMMode)(m.dc, ICM_QUERY), ICM_OFF);
    ci_mem_close(&m);
}
TEST(color_icm_mode_highcolor_off_when_off) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    EXPECT_NE(U(SetICMMode)(m.dc, ICM_OFF), 0);
    EXPECT_EQ(U(SetICMMode)(m.dc, ICM_QUERY), ICM_OFF);
    ci_mem_close(&m);
}
TEST(color_icm_mode_highcolor_invalid_modes) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    static const int modes[] = { 0, 5, 6, 7, 100, -1, -2, 0x7fffffff, (int)0x80000000, 0x100, 0x500 };
    for (unsigned i = 0; i < sizeof modes / sizeof *modes; i++) {
        SetLastError(0xdeadbeef);
        int r = U(SetICMMode)(m.dc, modes[i]);
        CHECKF(r == 0, "mode=%d r=%d", modes[i], r);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "mode=%d err=%lu", modes[i], GetLastError());
    }
    EXPECT_EQ(U(SetICMMode)(m.dc, ICM_QUERY), ICM_OFF);
    ci_mem_close(&m);
}
TEST(color_icm_mode_highcolor_on_off) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    SetLastError(0xdeadbeef);
    int r = U(SetICMMode)(m.dc, ICM_ON);
    if (!r) {
        t_note("SetICMMode(ON) on 32bpp memdc failed err=%lu", GetLastError());
    } else {
        EXPECT_EQ(U(SetICMMode)(m.dc, ICM_QUERY), ICM_ON);
        EXPECT_NE(U(SetICMMode)(m.dc, ICM_ON), 0);
        EXPECT_EQ(U(SetICMMode)(m.dc, ICM_QUERY), ICM_ON);
        EXPECT_NE(U(SetICMMode)(m.dc, ICM_OFF), 0);
        EXPECT_EQ(U(SetICMMode)(m.dc, ICM_QUERY), ICM_OFF);
    }
    ci_mem_close(&m);
}
TEST(color_icm_mode_highcolor_done_outside) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    int r = U(SetICMMode)(m.dc, ICM_DONE_OUTSIDEDC);
    int q = U(SetICMMode)(m.dc, ICM_QUERY);
    t_note("DONE_OUTSIDEDC ret=%d query=%d", r, q);
    if (r && q == ICM_DONE_OUTSIDEDC) EXPECT_EQ(U(SetICMMode)(m.dc, ICM_DONE_OUTSIDEDC), 1);
    U(SetICMMode)(m.dc, ICM_OFF);
    ci_mem_close(&m);
}
TEST(color_icm_mode_highcolor_high_bits_aliasing) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    /* Only the low byte of the mode is examined on high-colour DCs. */
    int q1 = U(SetICMMode)(m.dc, 0x103);
    int q2 = U(SetICMMode)(m.dc, ICM_QUERY);
    t_note("mode 0x103 -> %d, query -> %d", q1, q2);
    CHECK(q1 == q2 || q1 == 0);
    ci_mem_close(&m);
}
TEST(color_icm_mode_1bpp_dib_dc) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode)); REQUIRE(U(CreateDIBSection)); REQUIRE(U(CreateCompatibleDC));
    struct { BITMAPINFOHEADER h; RGBQUAD c[2]; } bi;
    memset(&bi, 0, sizeof bi);
    bi.h.biSize = sizeof bi.h; bi.h.biWidth = 8; bi.h.biHeight = 8; bi.h.biPlanes = 1; bi.h.biBitCount = 1;
    bi.c[1].rgbRed = bi.c[1].rgbGreen = bi.c[1].rgbBlue = 255;
    HDC d = U(CreateCompatibleDC)(NULL);
    void *bits;
    HBITMAP bm = U(CreateDIBSection)(d, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    HGDIOBJ old = U(SelectObject)(d, bm);
    EXPECT_EQ(U(SetICMMode)(d, ICM_QUERY), ICM_OFF);
    EXPECT_NE(U(SetICMMode)(d, ICM_ON), 0);
    EXPECT_EQ(U(SetICMMode)(d, ICM_QUERY), ICM_ON);
    EXPECT_NE(U(SetICMMode)(d, ICM_OFF), 0);
    U(SelectObject)(d, old); U(DeleteObject)(bm); U(DeleteDC)(d);
}
TEST(color_icm_mode_8bpp_dib_dc) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode)); REQUIRE(U(CreateDIBSection)); REQUIRE(U(CreateCompatibleDC));
    struct { BITMAPINFOHEADER h; RGBQUAD c[256]; } bi;
    memset(&bi, 0, sizeof bi);
    bi.h.biSize = sizeof bi.h; bi.h.biWidth = 8; bi.h.biHeight = 8; bi.h.biPlanes = 1; bi.h.biBitCount = 8; bi.h.biClrUsed = 256;
    HDC d = U(CreateCompatibleDC)(NULL);
    void *bits;
    HBITMAP bm = U(CreateDIBSection)(d, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    HGDIOBJ old = U(SelectObject)(d, bm);
    EXPECT_EQ(U(SetICMMode)(d, ICM_QUERY), ICM_OFF);
    int r = U(SetICMMode)(d, 7);
    CHECK(r == 0);
    U(SelectObject)(d, old); U(DeleteObject)(bm); U(DeleteDC)(d);
}
TEST(color_icm_mode_screen_dc_query) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    if (!d) { t_skip("no screen dc"); return; }
    int q = U(SetICMMode)(d, ICM_QUERY);
    CHECKF(q == ICM_OFF || q == ICM_ON, "query=%d", q);
    EXPECT_EQ(U(SetICMMode)(d, ICM_QUERY), q);
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_mode_create_dc_display_off) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode)); REQUIRE(U(CreateDCW)); REQUIRE(U(DeleteDC));
    HDC d = U(CreateDCW)(L"DISPLAY", NULL, NULL, NULL);
    if (!d) { t_skip("no DISPLAY dc"); return; }
    EXPECT_EQ(U(SetICMMode)(d, ICM_QUERY), ICM_OFF);
    U(DeleteDC)(d);
}
TEST(color_icm_mode_query_idempotent) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode)); REQUIRE(U(DeleteDC));
    HDC d = ci_mono_dc();
    for (int i = 0; i < 100; i++) if (U(SetICMMode)(d, ICM_QUERY) != ICM_OFF) { CHECKF(0, "iter %d", i); break; }
    U(DeleteDC)(d);
}
TEST(color_icm_mode_savedc_restore) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode)); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(DeleteDC));
    HDC d = ci_mono_dc();
    int id = U(SaveDC)(d);
    U(SetICMMode)(d, ICM_ON);
    CHECK(U(RestoreDC)(d, id));
    t_note("ICM mode after RestoreDC = %d", U(SetICMMode)(d, ICM_QUERY));
    U(SetICMMode)(d, ICM_OFF);
    U(DeleteDC)(d);
}

/* ---------------- GetICMProfile ---------------- */
TEST(color_icm_getprofile_w_null_size) {
    CI_NOWINE();
    REQUIRE(U(GetICMProfileW)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    WCHAR buf[MAX_PATH];
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetICMProfileW)(d, NULL, buf), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetICMProfileW)(d, NULL, NULL), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetICMProfileW)(NULL, NULL, NULL), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_getprofile_a_null_size) {
    CI_NOWINE();
    REQUIRE(U(GetICMProfileA)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    char buf[MAX_PATH];
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetICMProfileA)(d, NULL, buf), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetICMProfileA)(d, NULL, NULL), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetICMProfileA)(NULL, NULL, NULL), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_getprofile_w_bad_dc) {
    CI_NOWINE();
    REQUIRE(U(GetICMProfileW));
    WCHAR buf[MAX_PATH];
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)0xdeadbeef, (HDC)(ULONG_PTR)1, ci_bogus_dc() };
    for (unsigned i = 0; i < 4; i++) {
        DWORD sz = MAX_PATH;
        SetLastError(0xdeadbeef);
        CHECKF(U(GetICMProfileW)(bad[i], &sz, buf) == 0, "i=%u", i);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "i=%u err=%lu", i, GetLastError());
    }
}
TEST(color_icm_getprofile_a_bad_dc) {
    CI_NOWINE();
    REQUIRE(U(GetICMProfileA));
    char buf[MAX_PATH];
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)0xdeadbeef, ci_bogus_dc() };
    for (unsigned i = 0; i < 3; i++) {
        DWORD sz = MAX_PATH;
        SetLastError(0xdeadbeef);
        CHECKF(U(GetICMProfileA)(bad[i], &sz, buf) == 0, "i=%u", i);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "i=%u err=%lu", i, GetLastError());
    }
}
TEST(color_icm_getprofile_wrong_object_type) {
    CI_NOWINE();
    REQUIRE(U(GetICMProfileW)); REQUIRE(U(CreatePen)); REQUIRE(U(DeleteObject));
    HGDIOBJ pen = U(CreatePen)(PS_SOLID, 1, 0);
    WCHAR buf[MAX_PATH]; DWORD sz = MAX_PATH;
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetICMProfileW)((HDC)pen, &sz, buf), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    U(DeleteObject)(pen);
}

/* Fetch the screen DC profile; returns 0 and notes when the system has none. */
static int ci_screen_profile_w(HDC d, WCHAR *out, DWORD *len) {
    *len = MAX_PATH;
    if (U(GetICMProfileW)(d, len, out)) return 1;
    t_note("GetICMProfileW failed err=%lu", GetLastError());
    return 0;
}
TEST(color_icm_getprofile_w_size_protocol) {
    CI_NOWINE();
    REQUIRE(U(GetICMProfileW)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    if (!d) { t_skip("no screen dc"); return; }
    WCHAR full[MAX_PATH]; DWORD need;
    if (!ci_screen_profile_w(d, full, &need)) { U(ReleaseDC)(NULL, d); return; }
    EXPECT_EQ(need, lstrlenW(full) + 1);
    /* size query: zero size, NULL buffer */
    DWORD sz = 0;
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetICMProfileW)(d, &sz, NULL), 0);
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(sz, need);
    /* exact - 1 */
    WCHAR *b = calloc(MAX_PATH + 8, sizeof(WCHAR));
    sz = need - 1;
    memset(b, 0x55, 16);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetICMProfileW)(d, &sz, b), 0);
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(sz, need);
    EXPECT_EQ(b[0], 0x5555);
    /* exact */
    sz = need;
    EXPECT_NE(U(GetICMProfileW)(d, &sz, b), 0);
    EXPECT_EQ(sz, need);
    EXPECT_WSTR(b, full);
    /* oversize: the in/out count is rewritten to the needed length */
    sz = MAX_PATH;
    EXPECT_NE(U(GetICMProfileW)(d, &sz, b), 0);
    EXPECT_EQ(sz, need);
    /* zero size with a buffer */
    sz = 0; b[0] = 0x7777;
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetICMProfileW)(d, &sz, b), 0);
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(b[0], 0x7777);
    EXPECT_EQ(sz, need);
    /* size 1 */
    sz = 1;
    EXPECT_EQ(U(GetICMProfileW)(d, &sz, b), 0);
    EXPECT_EQ(sz, need);
    free(b);
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_getprofile_a_size_protocol) {
    CI_NOWINE();
    REQUIRE(U(GetICMProfileA)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    if (!d) { t_skip("no screen dc"); return; }
    char full[MAX_PATH]; DWORD need = MAX_PATH;
    if (!U(GetICMProfileA)(d, &need, full)) { t_note("GetICMProfileA failed err=%lu", GetLastError()); U(ReleaseDC)(NULL, d); return; }
    EXPECT_EQ(need, lstrlenA(full) + 1);
    DWORD sz = 0;
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetICMProfileA)(d, &sz, NULL), 0);
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(sz, need);
    sz = MAX_PATH;
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetICMProfileA)(d, &sz, NULL), 0);
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(sz, need);
    char b[MAX_PATH + 8];
    sz = need - 1; memset(b, 0x55, sizeof b);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetICMProfileA)(d, &sz, b), 0);
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(sz, need);
    EXPECT_EQ((unsigned char)b[0], 0x55);
    sz = need;
    EXPECT_NE(U(GetICMProfileA)(d, &sz, b), 0);
    EXPECT_EQ(sz, need);
    EXPECT_STR(b, full);
    sz = 0;
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetICMProfileA)(d, &sz, b), 0);
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(sz, need);
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_getprofile_ap_parity) {
    CI_NOWINE();
    REQUIRE(U(GetICMProfileA)); REQUIRE(U(GetICMProfileW)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    if (!d) { t_skip("no screen dc"); return; }
    WCHAR w[MAX_PATH]; char a[MAX_PATH], conv[MAX_PATH]; DWORD wl = MAX_PATH, al = MAX_PATH;
    if (U(GetICMProfileW)(d, &wl, w) && U(GetICMProfileA)(d, &al, a)) {
        WideCharToMultiByte(CP_ACP, 0, w, -1, conv, MAX_PATH, NULL, NULL);
        EXPECT_STR(a, conv);
    } else t_note("profile unavailable");
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_getprofile_repeatable) {
    CI_NOWINE();
    REQUIRE(U(GetICMProfileW)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    if (!d) { t_skip("no screen dc"); return; }
    WCHAR f[MAX_PATH], g[MAX_PATH]; DWORD fl, gl;
    if (ci_screen_profile_w(d, f, &fl)) {
        for (int i = 0; i < 50; i++) {
            gl = MAX_PATH;
            CHECK(U(GetICMProfileW)(d, &gl, g));
            CHECK(!lstrcmpW(f, g));
        }
    }
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_getprofile_path_shape) {
    CI_NOWINE();
    REQUIRE(U(GetICMProfileW)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    if (!d) { t_skip("no screen dc"); return; }
    WCHAR f[MAX_PATH]; DWORD fl;
    if (ci_screen_profile_w(d, f, &fl)) {
        CHECK(wcschr(f, L'\\') != NULL);
        DWORD at = GetFileAttributesW(f);
        if (at == INVALID_FILE_ATTRIBUTES) t_note("profile file %ls not found (err=%lu)", f, GetLastError());
        t_note("screen profile: %ls", f);
    }
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_getprofile_memdc_highcolor) {
    CI_NOWINE();
    REQUIRE(U(GetICMProfileW));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    DWORD sz = 0;
    SetLastError(0xdeadbeef);
    BOOL r = U(GetICMProfileW)(m.dc, &sz, NULL);
    DWORD e = GetLastError();
    EXPECT_EQ(r, 0);
    CHECKF(e == ERROR_INSUFFICIENT_BUFFER || e == CI_ERR_PROFILE_NOT_FOUND, "err=%lu", e);
    t_note("memdc profile query err=%lu size=%lu", e, sz);
    ci_mem_close(&m);
}
TEST(color_icm_getprofile_mono_dc) {
    CI_NOWINE();
    REQUIRE(U(GetICMProfileW)); REQUIRE(U(DeleteDC));
    HDC d = ci_mono_dc();
    DWORD sz = MAX_PATH; WCHAR b[MAX_PATH];
    SetLastError(0xdeadbeef);
    BOOL r = U(GetICMProfileW)(d, &sz, b);
    t_note("mono memdc GetICMProfileW=%d err=%lu size=%lu", r, GetLastError(), sz);
    if (!r) EXPECT_EQ(GetLastError(), CI_ERR_PROFILE_NOT_FOUND);
    U(DeleteDC)(d);
}
TEST(color_icm_getprofile_after_deletedc) {
    CI_NOWINE();
    REQUIRE(U(GetICMProfileA)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC d = U(CreateCompatibleDC)(NULL);
    U(DeleteDC)(d);
    char b[MAX_PATH]; DWORD sz = MAX_PATH;
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetICMProfileA)(d, &sz, b), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
}

/* ---------------- SetICMProfile ---------------- */
TEST(color_icm_setprofile_w_null_name) {
    CI_NOWINE();
    REQUIRE(U(SetICMProfileW)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(SetICMProfileW)(d, NULL), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(SetICMProfileW)(NULL, NULL), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_setprofile_a_null_name) {
    CI_NOWINE();
    REQUIRE(U(SetICMProfileA)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(SetICMProfileA)(d, NULL), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(SetICMProfileA)(NULL, NULL), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_setprofile_w_null_dc) {
    CI_NOWINE();
    REQUIRE(U(SetICMProfileW));
    WCHAR n[] = L"sRGB Color Space Profile.icm";
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(SetICMProfileW)(NULL, n), 0);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}
TEST(color_icm_setprofile_a_null_dc) {
    CI_NOWINE();
    REQUIRE(U(SetICMProfileA));
    char n[] = "sRGB Color Space Profile.icm";
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(SetICMProfileA)(NULL, n), 0);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}
TEST(color_icm_setprofile_deleted_dc) {
    CI_NOWINE();
    REQUIRE(U(SetICMProfileW));
    HDC d = ci_bogus_dc();
    WCHAR n[] = L"sRGB Color Space Profile.icm";
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(SetICMProfileW)(d, n), 0);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_INVALID_HANDLE || e == ERROR_INVALID_PARAMETER, "err=%lu", e);
}
TEST(color_icm_setprofile_bogus_dc_values) {
    CI_NOWINE();
    REQUIRE(U(SetICMProfileA));
    static const ULONG_PTR v[] = { 1, 0xdeadbeef, 0xffffffff };
    char n[] = "x.icm";
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        SetLastError(0xdeadbeef);
        CHECKF(U(SetICMProfileA)((HDC)v[i], n) == 0, "dc=%p", (void *)v[i]);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "dc=%p err=%lu", (void *)v[i], GetLastError());
    }
}
TEST(color_icm_setprofile_screen_same_profile) {
    CI_NOWINE();
    REQUIRE(U(SetICMProfileW)); REQUIRE(U(GetICMProfileW)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    if (!d) { t_skip("no screen dc"); return; }
    WCHAR f[MAX_PATH]; DWORD fl;
    if (ci_screen_profile_w(d, f, &fl)) {
        SetLastError(0xdeadbeef);
        BOOL r = U(SetICMProfileW)(d, f);
        CHECKF(r, "err=%lu", GetLastError());
        WCHAR g[MAX_PATH]; DWORD gl = MAX_PATH;
        if (r && U(GetICMProfileW)(d, &gl, g)) CHECKF(!lstrcmpiW(f, g), "set %ls got %ls", f, g);
    }
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_setprofile_a_screen_same_profile) {
    CI_NOWINE();
    REQUIRE(U(SetICMProfileA)); REQUIRE(U(GetICMProfileA)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    if (!d) { t_skip("no screen dc"); return; }
    char f[MAX_PATH]; DWORD fl = MAX_PATH;
    if (U(GetICMProfileA)(d, &fl, f)) {
        BOOL r = U(SetICMProfileA)(d, f);
        CHECKF(r, "err=%lu", GetLastError());
    }
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_setprofile_missing_file) {
    CI_NOWINE();
    REQUIRE(U(SetICMProfileW)); REQUIRE(U(GetICMProfileW)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    if (!d) { t_skip("no screen dc"); return; }
    WCHAR before[MAX_PATH], after[MAX_PATH]; DWORD bl = MAX_PATH, al = MAX_PATH;
    int have = U(GetICMProfileW)(d, &bl, before);
    WCHAR n[] = L"C:\\no_such_dir_t32\\missing_profile_t32.icm";
    SetLastError(0xdeadbeef);
    BOOL r = U(SetICMProfileW)(d, n);
    t_note("SetICMProfileW(missing) = %d err=%lu", r, GetLastError());
    CHECK(r == 0);
    if (have && U(GetICMProfileW)(d, &al, after)) CHECKF(!lstrcmpW(before, after), "profile changed after failed set");
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_setprofile_empty_name) {
    CI_NOWINE();
    REQUIRE(U(SetICMProfileW)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    if (!d) { t_skip("no screen dc"); return; }
    WCHAR n[] = L"";
    SetLastError(0xdeadbeef);
    BOOL r = U(SetICMProfileW)(d, n);
    t_note("SetICMProfileW(\"\") = %d err=%lu", r, GetLastError());
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_setprofile_memdc_roundtrip) {
    CI_NOWINE();
    REQUIRE(U(SetICMProfileW)); REQUIRE(U(GetICMProfileW));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    WCHAR n[] = L"sRGB Color Space Profile.icm";
    SetLastError(0xdeadbeef);
    BOOL r = U(SetICMProfileW)(m.dc, n);
    t_note("SetICMProfileW(memdc, sRGB) = %d err=%lu", r, GetLastError());
    if (r) {
        WCHAR g[MAX_PATH]; DWORD gl = MAX_PATH;
        if (U(GetICMProfileW)(m.dc, &gl, g)) {
            t_note("read back: %ls", g);
            CHECK(wcsstr(g, L"sRGB") != NULL || wcsstr(g, L"srgb") != NULL);
        }
    }
    ci_mem_close(&m);
}

/* ---------------- EnumICMProfiles ---------------- */
typedef struct { int n; LPARAM seen_param; int stop_after; int first_len; } ci_enum;
static int CALLBACK ci_enum_a(LPSTR name, LPARAM lp) {
    ci_enum *e = (ci_enum *)lp;
    e->n++;
    e->seen_param = lp;
    if (e->n == 1) e->first_len = name ? lstrlenA(name) : -1;
    return e->stop_after && e->n >= e->stop_after ? 0 : 1;
}
static int CALLBACK ci_enum_w(LPWSTR name, LPARAM lp) {
    ci_enum *e = (ci_enum *)lp;
    e->n++;
    e->seen_param = lp;
    if (e->n == 1) e->first_len = name ? lstrlenW(name) : -1;
    return e->stop_after && e->n >= e->stop_after ? 0 : 1;
}
static int CALLBACK ci_enum_never(LPSTR name, LPARAM lp) { (*(int *)lp)++; return 1; }
static int CALLBACK ci_enum_neverw(LPWSTR name, LPARAM lp) { (*(int *)lp)++; return 1; }
TEST(color_icm_enum_w_null_args) {
    CI_NOWINE();
    REQUIRE(U(EnumICMProfilesW)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    int hits = 0;
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(EnumICMProfilesW)(NULL, NULL, 0), -1);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(EnumICMProfilesW)(NULL, ci_enum_neverw, (LPARAM)&hits), -1);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(EnumICMProfilesW)(d, NULL, 0), -1);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    EXPECT_EQ(hits, 0);
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_enum_a_null_args) {
    CI_NOWINE();
    REQUIRE(U(EnumICMProfilesA)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    int hits = 0;
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(EnumICMProfilesA)(NULL, NULL, 0), -1);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(EnumICMProfilesA)(NULL, ci_enum_never, (LPARAM)&hits), -1);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(EnumICMProfilesA)(d, NULL, 0), -1);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    EXPECT_EQ(hits, 0);
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_enum_bad_dcs) {
    CI_NOWINE();
    REQUIRE(U(EnumICMProfilesA)); REQUIRE(U(EnumICMProfilesW));
    HDC bad[] = { (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef, (HDC)(ULONG_PTR)0x00010001, (HDC)(ULONG_PTR)0xffffffff, ci_bogus_dc() };
    int hits = 0;
    for (unsigned i = 0; i < 5; i++) {
        SetLastError(0xdeadbeef);
        CHECKF(U(EnumICMProfilesA)(bad[i], ci_enum_never, (LPARAM)&hits) == -1, "A i=%u", i);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "A i=%u err=%lu", i, GetLastError());
        SetLastError(0xdeadbeef);
        CHECKF(U(EnumICMProfilesW)(bad[i], ci_enum_neverw, (LPARAM)&hits) == -1, "W i=%u", i);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "W i=%u err=%lu", i, GetLastError());
    }
    EXPECT_EQ(hits, 0);
}
TEST(color_icm_enum_wrong_object_type) {
    CI_NOWINE();
    REQUIRE(U(EnumICMProfilesW)); REQUIRE(U(CreatePen)); REQUIRE(U(DeleteObject));
    HGDIOBJ pen = U(CreatePen)(PS_SOLID, 1, 0);
    int hits = 0;
    EXPECT_EQ(U(EnumICMProfilesW)((HDC)pen, ci_enum_neverw, (LPARAM)&hits), -1);
    EXPECT_EQ(hits, 0);
    U(DeleteObject)(pen);
}
TEST(color_icm_enum_w_screen_dc) {
    CI_NOWINE();
    REQUIRE(U(EnumICMProfilesW)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    if (!d) { t_skip("no screen dc"); return; }
    ci_enum e; memset(&e, 0, sizeof e);
    int r = U(EnumICMProfilesW)(d, ci_enum_w, (LPARAM)&e);
    t_note("EnumICMProfilesW ret=%d count=%d", r, e.n);
    CHECKF(r == -1 || r >= 0, "r=%d", r);
    if (e.n) { EXPECT_EQ(e.seen_param, (LPARAM)&e); CHECK(e.first_len > 0); }
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_enum_a_screen_dc) {
    CI_NOWINE();
    REQUIRE(U(EnumICMProfilesA)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    if (!d) { t_skip("no screen dc"); return; }
    ci_enum e; memset(&e, 0, sizeof e);
    int r = U(EnumICMProfilesA)(d, ci_enum_a, (LPARAM)&e);
    t_note("EnumICMProfilesA ret=%d count=%d", r, e.n);
    if (e.n) { EXPECT_EQ(e.seen_param, (LPARAM)&e); CHECK(e.first_len > 0); }
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_enum_ap_count_parity) {
    CI_NOWINE();
    REQUIRE(U(EnumICMProfilesA)); REQUIRE(U(EnumICMProfilesW)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    if (!d) { t_skip("no screen dc"); return; }
    ci_enum ea, ew; memset(&ea, 0, sizeof ea); memset(&ew, 0, sizeof ew);
    U(EnumICMProfilesA)(d, ci_enum_a, (LPARAM)&ea);
    U(EnumICMProfilesW)(d, ci_enum_w, (LPARAM)&ew);
    EXPECT_EQ(ea.n, ew.n);
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_enum_stop_early) {
    CI_NOWINE();
    REQUIRE(U(EnumICMProfilesW)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    if (!d) { t_skip("no screen dc"); return; }
    ci_enum all; memset(&all, 0, sizeof all);
    U(EnumICMProfilesW)(d, ci_enum_w, (LPARAM)&all);
    ci_enum one; memset(&one, 0, sizeof one); one.stop_after = 1;
    int r = U(EnumICMProfilesW)(d, ci_enum_w, (LPARAM)&one);
    t_note("stop-after-1 ret=%d count=%d (full=%d)", r, one.n, all.n);
    if (all.n >= 1) EXPECT_EQ(one.n, 1);
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_enum_repeatable) {
    CI_NOWINE();
    REQUIRE(U(EnumICMProfilesW)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    if (!d) { t_skip("no screen dc"); return; }
    ci_enum a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    int ra = U(EnumICMProfilesW)(d, ci_enum_w, (LPARAM)&a);
    int rb = U(EnumICMProfilesW)(d, ci_enum_w, (LPARAM)&b);
    EXPECT_EQ(a.n, b.n);
    EXPECT_EQ(ra, rb);
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_enum_memdc) {
    CI_NOWINE();
    REQUIRE(U(EnumICMProfilesW));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    ci_enum e; memset(&e, 0, sizeof e);
    int r = U(EnumICMProfilesW)(m.dc, ci_enum_w, (LPARAM)&e);
    t_note("memdc EnumICMProfilesW ret=%d count=%d err=%lu", r, e.n, GetLastError());
    ci_mem_close(&m);
}

/* ---------------- ColorMatchToTarget / CheckColorsInGamut / ColorCorrectPalette ---------------- */
TEST(color_icm_matchtotarget_null_target) {
    CI_NOWINE_ABORT();
    REQUIRE(U(ColorMatchToTarget));
    ci_mem m; if (!ci_mem_open(&m, 2, 2)) { t_skip("no memdc"); return; }
    for (DWORD act = 0; act <= 3; act++) {
        SetLastError(0xdeadbeef);
        CHECKF(U(ColorMatchToTarget)(m.dc, NULL, act) == 0, "act=%lu", act);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "act=%lu err=%lu", act, GetLastError());
    }
    ci_mem_close(&m);
}
TEST(color_icm_matchtotarget_bad_target) {
    CI_NOWINE_ABORT();
    REQUIRE(U(ColorMatchToTarget));
    ci_mem m; if (!ci_mem_open(&m, 2, 2)) { t_skip("no memdc"); return; }
    HDC bad[] = { (HDC)(ULONG_PTR)0xdeadbeef, (HDC)(ULONG_PTR)1, ci_bogus_dc() };
    for (unsigned i = 0; i < 3; i++) {
        SetLastError(0xdeadbeef);
        CHECKF(U(ColorMatchToTarget)(m.dc, bad[i], CS_ENABLE) == 0, "i=%u", i);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "i=%u err=%lu", i, GetLastError());
    }
    ci_mem_close(&m);
}
TEST(color_icm_matchtotarget_icm_off) {
    CI_NOWINE_ABORT();
    REQUIRE(U(ColorMatchToTarget));
    ci_mem a, b;
    if (!ci_mem_open(&a, 2, 2)) { t_skip("no memdc"); return; }
    if (!ci_mem_open(&b, 2, 2)) { ci_mem_close(&a); t_skip("no memdc"); return; }
    static const DWORD acts[] = { CS_ENABLE, CS_DISABLE, CS_DELETE_TRANSFORM, 0, 4, 0xffffffffu };
    for (unsigned i = 0; i < sizeof acts / sizeof *acts; i++) {
        SetLastError(0xdeadbeef);
        CHECKF(U(ColorMatchToTarget)(a.dc, b.dc, acts[i]) == 0, "act=%lu", acts[i]);
        CHECKF(GetLastError() == CI_ERR_ICM_NOT_ENABLED, "act=%lu err=%lu", acts[i], GetLastError());
    }
    ci_mem_close(&a); ci_mem_close(&b);
}
TEST(color_icm_matchtotarget_self_icm_off) {
    CI_NOWINE_ABORT();
    REQUIRE(U(ColorMatchToTarget));
    ci_mem a; if (!ci_mem_open(&a, 2, 2)) { t_skip("no memdc"); return; }
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(ColorMatchToTarget)(a.dc, a.dc, CS_ENABLE), 0);
    EXPECT_ERR(0, CI_ERR_ICM_NOT_ENABLED);
    ci_mem_close(&a);
}
TEST(color_icm_gamut_null_args) {
    CI_NOWINE_ABORT();
    REQUIRE(U(CheckColorsInGamut));
    ci_mem m; if (!ci_mem_open(&m, 2, 2)) { t_skip("no memdc"); return; }
    RGBTRIPLE rgb[2]; BYTE out[2];
    memset(rgb, 0, sizeof rgb);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(CheckColorsInGamut)(m.dc, NULL, out, 2), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(CheckColorsInGamut)(m.dc, rgb, NULL, 2), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(CheckColorsInGamut)(m.dc, rgb, out, 0), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(CheckColorsInGamut)(m.dc, NULL, NULL, 0), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    ci_mem_close(&m);
}
TEST(color_icm_gamut_null_args_null_dc) {
    CI_NOWINE_ABORT();
    REQUIRE(U(CheckColorsInGamut));
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(CheckColorsInGamut)(NULL, NULL, NULL, 0), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
}
TEST(color_icm_gamut_bad_dc) {
    CI_NOWINE_ABORT();
    REQUIRE(U(CheckColorsInGamut));
    RGBTRIPLE rgb[2]; BYTE out[2];
    memset(rgb, 0, sizeof rgb);
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)0xdeadbeef, (HDC)(ULONG_PTR)1, ci_bogus_dc() };
    for (unsigned i = 0; i < 4; i++) {
        SetLastError(0xdeadbeef);
        CHECKF(U(CheckColorsInGamut)(bad[i], rgb, out, 2) == 0, "i=%u", i);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "i=%u err=%lu", i, GetLastError());
    }
}
TEST(color_icm_gamut_icm_off) {
    CI_NOWINE_ABORT();
    REQUIRE(U(CheckColorsInGamut));
    ci_mem m; if (!ci_mem_open(&m, 2, 2)) { t_skip("no memdc"); return; }
    RGBTRIPLE rgb[4]; BYTE out[4];
    memset(rgb, 0x40, sizeof rgb); memset(out, 0x7e, sizeof out);
    static const DWORD counts[] = { 1, 2, 3, 4 };
    for (unsigned i = 0; i < 4; i++) {
        SetLastError(0xdeadbeef);
        CHECKF(U(CheckColorsInGamut)(m.dc, rgb, out, counts[i]) == 0, "n=%lu", counts[i]);
        CHECKF(GetLastError() == CI_ERR_ICM_NOT_ENABLED, "n=%lu err=%lu", counts[i], GetLastError());
    }
    EXPECT_EQ(out[0], 0x7e);
    ci_mem_close(&m);
}
TEST(color_icm_gamut_mono_dc_icm_on) {
    CI_NOWINE_ABORT();
    REQUIRE(U(CheckColorsInGamut)); REQUIRE(U(SetICMMode)); REQUIRE(U(DeleteDC));
    HDC d = ci_mono_dc();
    U(SetICMMode)(d, ICM_ON);
    RGBTRIPLE rgb[2]; BYTE out[2];
    memset(rgb, 0x40, sizeof rgb); memset(out, 0x7e, sizeof out);
    SetLastError(0xdeadbeef);
    BOOL r = U(CheckColorsInGamut)(d, rgb, out, 2);
    t_note("gamut on mono DC with ICM flag: r=%d err=%lu out=%02x %02x", r, GetLastError(), out[0], out[1]);
    U(SetICMMode)(d, ICM_OFF);
    U(DeleteDC)(d);
}
TEST(color_icm_correctpalette_arg_table) {
    CI_NOWINE_ABORT();
    REQUIRE(U(ColorCorrectPalette)); REQUIRE(U(CreatePalette));
    ci_mem m; if (!ci_mem_open(&m, 2, 2)) { t_skip("no memdc"); return; }
    LOGPALETTE *lp = calloc(1, sizeof(LOGPALETTE) + 255 * sizeof(PALETTEENTRY));
    lp->palVersion = 0x300; lp->palNumEntries = 256;
    HPALETTE pal = U(CreatePalette)(lp);
    free(lp);
    static const struct { DWORD first, num; } bad[] = {
        {0, 0}, {0, 0x10001}, {0x10000, 1}, {0xffff, 2}, {0x8000, 0x8001}, {1, 0x10000}, {0xffffffffu, 1}, {0, 0xffffffffu}, {0x10001, 0x10000},
    };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0xdeadbeef);
        CHECKF(U(ColorCorrectPalette)(m.dc, pal, bad[i].first, bad[i].num) == 0, "i=%u", i);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "i=%u err=%lu", i, GetLastError());
    }
    U(DeleteObject)(pal);
    ci_mem_close(&m);
}
TEST(color_icm_correctpalette_null_handles) {
    CI_NOWINE_ABORT();
    REQUIRE(U(ColorCorrectPalette)); REQUIRE(U(CreatePalette));
    ci_mem m; if (!ci_mem_open(&m, 2, 2)) { t_skip("no memdc"); return; }
    LOGPALETTE lp; memset(&lp, 0, sizeof lp); lp.palVersion = 0x300; lp.palNumEntries = 1;
    HPALETTE pal = U(CreatePalette)(&lp);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(ColorCorrectPalette)(NULL, pal, 0, 1), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(ColorCorrectPalette)(m.dc, NULL, 0, 1), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(ColorCorrectPalette)(NULL, NULL, 0, 0), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    U(DeleteObject)(pal);
    ci_mem_close(&m);
}
TEST(color_icm_correctpalette_bad_dc) {
    CI_NOWINE_ABORT();
    REQUIRE(U(ColorCorrectPalette)); REQUIRE(U(CreatePalette));
    LOGPALETTE lp; memset(&lp, 0, sizeof lp); lp.palVersion = 0x300; lp.palNumEntries = 1;
    HPALETTE pal = U(CreatePalette)(&lp);
    HDC bad[] = { (HDC)(ULONG_PTR)0xdeadbeef, (HDC)(ULONG_PTR)1, ci_bogus_dc() };
    for (unsigned i = 0; i < 3; i++) {
        SetLastError(0xdeadbeef);
        CHECKF(U(ColorCorrectPalette)(bad[i], pal, 0, 1) == 0, "i=%u", i);
        /* non-DC handle types go through the local-DC lookup (6); a stale DC handle fails the entry lookup (87) */
        DWORD want = (i == 2) ? ERROR_INVALID_PARAMETER : ERROR_INVALID_HANDLE;
        CHECKF(GetLastError() == want, "i=%u err=%lu", i, GetLastError());
    }
    U(DeleteObject)(pal);
}
TEST(color_icm_correctpalette_stock_palette) {
    CI_NOWINE_ABORT();
    REQUIRE(U(ColorCorrectPalette)); REQUIRE(U(GetStockObject));
    ci_mem m; if (!ci_mem_open(&m, 2, 2)) { t_skip("no memdc"); return; }
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(ColorCorrectPalette)(m.dc, (HPALETTE)U(GetStockObject)(DEFAULT_PALETTE), 0, 1), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    ci_mem_close(&m);
}
TEST(color_icm_correctpalette_icm_off) {
    CI_NOWINE_ABORT();
    REQUIRE(U(ColorCorrectPalette)); REQUIRE(U(CreatePalette));
    ci_mem m; if (!ci_mem_open(&m, 2, 2)) { t_skip("no memdc"); return; }
    LOGPALETTE *lp = calloc(1, sizeof(LOGPALETTE) + 255 * sizeof(PALETTEENTRY));
    lp->palVersion = 0x300; lp->palNumEntries = 256;
    HPALETTE pal = U(CreatePalette)(lp);
    free(lp);
    static const struct { DWORD first, num; } ok[] = { {0, 1}, {0, 256}, {255, 1}, {10, 20}, {0, 0xffff}, {0xffff, 1}, {0x8000, 0x8000} };
    for (unsigned i = 0; i < sizeof ok / sizeof *ok; i++) {
        SetLastError(0xdeadbeef);
        CHECKF(U(ColorCorrectPalette)(m.dc, pal, ok[i].first, ok[i].num) == 0, "i=%u", i);
        CHECKF(GetLastError() == CI_ERR_ICM_NOT_ENABLED, "i=%u err=%lu", i, GetLastError());
    }
    U(DeleteObject)(pal);
    ci_mem_close(&m);
}
TEST(color_icm_correctpalette_wrong_palette_type) {
    CI_NOWINE_ABORT();
    REQUIRE(U(ColorCorrectPalette)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    ci_mem m; if (!ci_mem_open(&m, 2, 2)) { t_skip("no memdc"); return; }
    HBRUSH br = U(CreateSolidBrush)(0x112233);
    SetLastError(0xdeadbeef);
    BOOL r = U(ColorCorrectPalette)(m.dc, (HPALETTE)br, 0, 1);
    EXPECT_EQ(r, 0);
    t_note("ColorCorrectPalette(brush) err=%lu", GetLastError());
    U(DeleteObject)(br);
    ci_mem_close(&m);
}
TEST(color_icm_correctpalette_icm_on_highcolor) {
    CI_NOWINE_ABORT();
    REQUIRE(U(ColorCorrectPalette)); REQUIRE(U(CreatePalette)); REQUIRE(U(SetICMMode));
    ci_mem m; if (!ci_mem_open(&m, 2, 2)) { t_skip("no memdc"); return; }
    LOGPALETTE *lp = calloc(1, sizeof(LOGPALETTE) + 15 * sizeof(PALETTEENTRY));
    lp->palVersion = 0x300; lp->palNumEntries = 16;
    for (int i = 0; i < 16; i++) { lp->palPalEntry[i].peRed = i * 16; lp->palPalEntry[i].peGreen = 255 - i * 16; lp->palPalEntry[i].peBlue = i; }
    HPALETTE pal = U(CreatePalette)(lp);
    free(lp);
    if (U(SetICMMode)(m.dc, ICM_ON)) {
        SetLastError(0xdeadbeef);
        BOOL r = U(ColorCorrectPalette)(m.dc, pal, 0, 16);
        t_note("ColorCorrectPalette with ICM on: r=%d err=%lu", r, GetLastError());
        U(SetICMMode)(m.dc, ICM_OFF);
    }
    U(DeleteObject)(pal);
    ci_mem_close(&m);
}
