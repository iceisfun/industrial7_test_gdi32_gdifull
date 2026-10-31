/* CreateMetaFile / CloseMetaFile / DeleteMetaFile lifecycle, errors, handle types. */
#include "wmf.h"

TEST(wmf_create_mem_basic) {
    WMF_REQ_BASE();
    HDC dc = U(CreateMetaFileA)(NULL);
    CHECK(dc != NULL);
    if (!dc) return;
    HMETAFILE h = U(CloseMetaFile)(dc);
    CHECK(h != NULL);
    if (h) CHECK(U(DeleteMetaFile)(h));
}

TEST(wmf_create_mem_w) {
    WMF_REQ_BASE(); REQUIRE(U(CreateMetaFileW));
    HDC dc = U(CreateMetaFileW)(NULL);
    CHECK(dc != NULL);
    if (!dc) return;
    HMETAFILE h = U(CloseMetaFile)(dc);
    CHECK(h != NULL);
    if (h) CHECK(U(DeleteMetaFile)(h));
}

TEST(wmf_create_handle_type) {
    WMF_REQ_BASE(); REQUIRE(U(GetObjectType));
    HDC dc = mf_new();
    REQUIRE(dc);
    EXPECT_EQ(U(GetObjectType)(dc), OBJ_METADC);
    /* the DC handle type field of a metafile DC is 0x66 */
    EXPECT_EQ(((ULONG_PTR)dc >> 16) & 0x7f, 0x66);
    HMETAFILE h = U(CloseMetaFile)(dc);
    CHECK(h != NULL);
    if (h) {
        DWORD t = U(GetObjectType)(h);
        EXPECT_EQ(t, OBJ_METAFILE);
        EXPECT_EQ(((ULONG_PTR)h >> 16) & 0x7f, 0x26);
        U(DeleteMetaFile)(h);
    }
}

TEST(wmf_create_handles_distinct) {
    WMF_REQ_BASE();
    HDC a = mf_new(), b = mf_new(), c = mf_new();
    REQUIRE(a && b && c);
    CHECK(a != b); CHECK(b != c); CHECK(a != c);
    HMETAFILE ha = U(CloseMetaFile)(a), hb = U(CloseMetaFile)(b), hc = U(CloseMetaFile)(c);
    CHECK(ha && hb && hc);
    CHECK(ha != hb); CHECK(hb != hc); CHECK(ha != hc);
    U(DeleteMetaFile)(ha); U(DeleteMetaFile)(hb); U(DeleteMetaFile)(hc);
}

TEST(wmf_create_many_dcs) {
    WMF_REQ_BASE();
    enum { N = 64 };
    HDC dcs[N];
    int ok = 0;
    for (int i = 0; i < N; i++) { dcs[i] = mf_new(); ok += dcs[i] != NULL; }
    EXPECT_EQ(ok, N);
    for (int i = 0; i < N; i++) for (int j = i + 1; j < N; j++) if (dcs[i] && dcs[i] == dcs[j]) CHECK(0);
    for (int i = 0; i < N; i++) if (dcs[i]) { HMETAFILE h = U(CloseMetaFile)(dcs[i]); CHECK(h != NULL); if (h) U(DeleteMetaFile)(h); }
}

TEST(wmf_create_leak_cycle) {
    WMF_REQ_BASE(); REQUIRE(U(GetGuiResources));
    DWORD before = gdi_count();
    for (int i = 0; i < 200; i++) {
        HDC dc = mf_new();
        if (!dc) { CHECK(0); break; }
        U(Rectangle)(dc, 0, 0, 10, 10);
        HMETAFILE h = U(CloseMetaFile)(dc);
        if (h) U(DeleteMetaFile)(h);
    }
    DWORD after = gdi_count();
    EXPECT_EQ(after, before);
}

TEST(wmf_create_leak_objects_in_dc) {
    WMF_REQ_BASE(); REQUIRE(U(GetGuiResources)); REQUIRE(U(CreatePen)); REQUIRE(U(CreateSolidBrush));
    DWORD before = gdi_count();
    HDC dc = mf_new();
    REQUIRE(dc);
    HPEN pen = U(CreatePen)(PS_SOLID, 1, RGB(1, 2, 3));
    HBRUSH br = U(CreateSolidBrush)(RGB(4, 5, 6));
    HGDIOBJ op = U(SelectObject)(dc, pen);
    HGDIOBJ ob = U(SelectObject)(dc, br);
    CHECK(op != NULL); CHECK(ob != NULL);
    U(Rectangle)(dc, 0, 0, 4, 4);
    U(SelectObject)(dc, op); U(SelectObject)(dc, ob);
    U(DeleteObject)(pen); U(DeleteObject)(br);
    HMETAFILE h = U(CloseMetaFile)(dc);
    CHECK(h != NULL);
    if (h) U(DeleteMetaFile)(h);
    EXPECT_EQ(gdi_count(), before);
}

TEST(wmf_close_null) {
    WMF_REQ_BASE();
    SET_ERR(0);
    HMETAFILE h = U(CloseMetaFile)(NULL);
    CHECK(h == NULL);
    EXPECT_ERR(h, ERROR_INVALID_HANDLE);
}

TEST(wmf_close_bogus_handle) {
    WMF_REQ_BASE();
    HDC bogus[] = { (HDC)(ULONG_PTR)0x1, (HDC)(ULONG_PTR)0xdeadbeef, (HDC)(ULONG_PTR)0x00660001, (HDC)(ULONG_PTR)0x00660FFF, (HDC)(ULONG_PTR)-1 };
    for (size_t i = 0; i < sizeof bogus / sizeof *bogus; i++) {
        SET_ERR(0);
        HMETAFILE h = U(CloseMetaFile)(bogus[i]);
        CHECKF(h == NULL, "bogus %p", bogus[i]);
        EXPECT_ERR(h, ERROR_INVALID_HANDLE);
    }
}

TEST(wmf_close_memory_dc) {
    WMF_REQ_BASE(); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC m = U(CreateCompatibleDC)(NULL);
    REQUIRE(m);
    SET_ERR(0);
    HMETAFILE h = U(CloseMetaFile)(m);
    CHECK(h == NULL);
    EXPECT_ERR(h, ERROR_INVALID_HANDLE);
    CHECK(U(DeleteDC)(m));
}

TEST(wmf_close_twice) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    HMETAFILE h = U(CloseMetaFile)(dc);
    CHECK(h != NULL);
    SET_ERR(0);
    HMETAFILE h2 = U(CloseMetaFile)(dc);
    CHECK(h2 == NULL);
    EXPECT_ERR(h2, ERROR_INVALID_HANDLE);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_close_with_metafile_handle) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    HMETAFILE h = U(CloseMetaFile)(dc);
    REQUIRE(h);
    SET_ERR(0);
    HMETAFILE r = U(CloseMetaFile)((HDC)h);
    CHECK(r == NULL);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    CHECK(U(DeleteMetaFile)(h));
}

TEST(wmf_delete_null) {
    WMF_REQ_BASE();
    SET_ERR(0);
    BOOL r = U(DeleteMetaFile)(NULL);
    CHECK(!r);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
}

TEST(wmf_delete_bogus) {
    WMF_REQ_BASE();
    HMETAFILE bogus[] = { (HMETAFILE)(ULONG_PTR)1, (HMETAFILE)(ULONG_PTR)0x12345678, (HMETAFILE)(ULONG_PTR)0x00260001, (HMETAFILE)(ULONG_PTR)-1 };
    for (size_t i = 0; i < sizeof bogus / sizeof *bogus; i++) {
        SET_ERR(0);
        BOOL r = U(DeleteMetaFile)(bogus[i]);
        CHECK(!r);
        EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    }
}

TEST(wmf_delete_twice) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    HMETAFILE h = U(CloseMetaFile)(dc);
    REQUIRE(h);
    CHECK(U(DeleteMetaFile)(h));
    SET_ERR(0);
    BOOL r = U(DeleteMetaFile)(h);
    CHECK(!r);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
}

TEST(wmf_delete_dc_handle) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    SET_ERR(0);
    BOOL r = U(DeleteMetaFile)((HMETAFILE)dc);
    CHECK(!r);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    HMETAFILE h = U(CloseMetaFile)(dc);
    CHECK(h != NULL);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_delete_gdi_object_as_metafile) {
    WMF_REQ_BASE(); REQUIRE(U(CreateSolidBrush));
    HBRUSH b = U(CreateSolidBrush)(RGB(1, 1, 1));
    REQUIRE(b);
    SET_ERR(0);
    BOOL r = U(DeleteMetaFile)((HMETAFILE)b);
    CHECK(!r);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    CHECK(U(DeleteObject)(b));
}

TEST(wmf_deleteobject_on_metafile_handle) {
    WMF_REQ_BASE(); REQUIRE(U(DeleteObject));
    HDC dc = mf_new();
    REQUIRE(dc);
    HMETAFILE h = U(CloseMetaFile)(dc);
    REQUIRE(h);
    /* DeleteObject is not the correct way to free an HMETAFILE; observed only */
    BOOL r = U(DeleteObject)(h);
    t_note("DeleteObject(HMETAFILE) = %d", r);
    if (!r) U(DeleteMetaFile)(h);
}

TEST(wmf_deleteobject_on_metadc) {
    WMF_REQ_BASE(); REQUIRE(U(DeleteObject)); REQUIRE(U(DeleteDC));
    HDC dc = mf_new();
    REQUIRE(dc);
    BOOL r = U(DeleteDC)(dc);
    t_note("DeleteDC(metafile dc) = %d", r);
    if (!r) {
        HMETAFILE h = U(CloseMetaFile)(dc);
        if (h) U(DeleteMetaFile)(h);
    }
}

TEST(wmf_use_after_close_dc) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    HMETAFILE h = U(CloseMetaFile)(dc);
    REQUIRE(h);
    /* drawing on the stale DC handle must fail */
    SET_ERR(0);
    BOOL r = U(Rectangle)(dc, 0, 0, 5, 5);
    CHECK(!r);
    r = U(MoveToEx)(dc, 1, 1, NULL);
    CHECK(!r);
    CHECK(U(SetBkMode)(dc, TRANSPARENT) == 0);
    U(DeleteMetaFile)(h);
}

TEST(wmf_use_after_delete_handle) {
    WMF_REQ_BASE(); REQUIRE(U(CopyMetaFileA));
    HDC dc = mf_new();
    REQUIRE(dc);
    HMETAFILE h = U(CloseMetaFile)(dc);
    REQUIRE(h);
    CHECK(U(DeleteMetaFile)(h));
    WB b;
    CHECK(wb_get(h, &b) == NULL);
    SET_ERR(0);
    HMETAFILE c = U(CopyMetaFileA)(h, NULL);
    CHECK(c == NULL);
    EXPECT_ERR(c, ERROR_INVALID_HANDLE);
}

TEST(wmf_handle_reuse_after_delete) {
    WMF_REQ_BASE();
    /* A stale handle must stay invalid even after later metafiles are created. */
    HDC dc = mf_new();
    REQUIRE(dc);
    HMETAFILE h = U(CloseMetaFile)(dc);
    REQUIRE(h);
    U(DeleteMetaFile)(h);
    HMETAFILE keep[8]; int n = 0;
    for (int i = 0; i < 8; i++) { HDC d = mf_new(); if (d) keep[n++] = U(CloseMetaFile)(d); }
    for (int i = 0; i < n; i++) if (keep[i] == h) { /* index reuse is allowed only with a fresh uniqueness bits */ t_note("handle value reused"); }
    SET_ERR(0);
    UINT sz = U(GetMetaFileBitsEx)(h, 0, NULL);
    if (sz == 0) EXPECT_ERR(sz, ERROR_INVALID_HANDLE);
    for (int i = 0; i < n; i++) if (keep[i]) U(DeleteMetaFile)(keep[i]);
}

TEST(wmf_device_caps_technology) {
    WMF_REQ_BASE(); REQUIRE(U(GetDeviceCaps));
    HDC dc = mf_new();
    REQUIRE(dc);
    EXPECT_EQ(U(GetDeviceCaps)(dc, TECHNOLOGY), DT_METAFILE);
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_device_caps_misc) {
    WMF_REQ_BASE(); REQUIRE(U(GetDeviceCaps));
    HDC dc = mf_new();
    REQUIRE(dc);
    int idx[] = { HORZSIZE, VERTSIZE, HORZRES, VERTRES, BITSPIXEL, PLANES, NUMBRUSHES, NUMPENS, NUMFONTS, NUMCOLORS, ASPECTX, ASPECTY, ASPECTXY, LOGPIXELSX, LOGPIXELSY, SIZEPALETTE, NUMRESERVED, COLORRES, RASTERCAPS, CURVECAPS, LINECAPS, POLYGONALCAPS, TEXTCAPS };
    for (size_t i = 0; i < sizeof idx / sizeof *idx; i++) {
        int v = U(GetDeviceCaps)(dc, idx[i]);
        t_note("metafile DC cap %d = %d", idx[i], v);
    }
    CHECK(U(GetDeviceCaps)(dc, TECHNOLOGY) == DT_METAFILE);
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_device_caps_bad_index) {
    WMF_REQ_BASE(); REQUIRE(U(GetDeviceCaps));
    HDC dc = mf_new();
    REQUIRE(dc);
    EXPECT_EQ(U(GetDeviceCaps)(dc, -1), 0);
    EXPECT_EQ(U(GetDeviceCaps)(dc, 100000), 0);
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_unsupported_getpixel) {
    WMF_REQ_BASE(); REQUIRE(U(GetPixel));
    HDC dc = mf_new();
    REQUIRE(dc);
    COLORREF c = U(GetPixel)(dc, 0, 0);
    EXPECT_EQ(c, CLR_INVALID);
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_unsupported_getters) {
    WMF_REQ_BASE(); REQUIRE(U(GetCurrentObject)); REQUIRE(U(GetMapMode));
    HDC dc = mf_new();
    REQUIRE(dc);
    /* queries against a metafile DC: observed, not asserted beyond non-crash */
    int mm = U(GetMapMode)(dc);
    t_note("GetMapMode(metaDC) = %d", mm);
    HGDIOBJ p = U(GetCurrentObject)(dc, OBJ_PEN);
    t_note("GetCurrentObject(metaDC, PEN) = %p", p);
    POINT pt;
    BOOL r = U(GetCurrentPositionEx)(dc, &pt);
    t_note("GetCurrentPositionEx(metaDC) = %d", r);
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_unsupported_gettextmetrics) {
    WMF_REQ_BASE(); REQUIRE(U(GetTextMetricsA));
    HDC dc = mf_new();
    REQUIRE(dc);
    TEXTMETRICA tm; memset(&tm, 0, sizeof tm);
    BOOL r = U(GetTextMetricsA)(dc, &tm);
    t_note("GetTextMetrics(metaDC) = %d, height %ld", r, tm.tmHeight);
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (h) U(DeleteMetaFile)(h);
}

/* ---- file based ---- */
TEST(wmf_file_create_close_play) {
    WMF_REQ_BASE(); REQUIRE(U(GetMetaFileA));
    char path[MAX_PATH]; tmp_path(path, "wmf_fc");
    DeleteFileA(path);
    HDC dc = U(CreateMetaFileA)(path);
    REQUIRE(dc);
    CHECK(GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES);
    EXPECT_EQ(U(GetDeviceCaps) ? U(GetDeviceCaps)(dc, TECHNOLOGY) : DT_METAFILE, DT_METAFILE);
    U(Rectangle)(dc, 1, 2, 30, 40);
    HMETAFILE h = U(CloseMetaFile)(dc);
    CHECK(h != NULL);
    if (h) {
        WB b;
        CHECK(wb_get(h, &b) != NULL);
        CHECK(wb_valid(&b));
        CHECK(wb_count(&b, 0x041B) == 1);
        wb_free(&b);
        U(DeleteMetaFile)(h);
    }
    /* the file remains on disk after DeleteMetaFile */
    CHECK(GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES);
    DeleteFileA(path);
}

TEST(wmf_file_create_w) {
    WMF_REQ_BASE(); REQUIRE(U(CreateMetaFileW));
    char path[MAX_PATH]; tmp_path(path, "wmf_fcw");
    WCHAR wp[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, path, -1, wp, MAX_PATH);
    DeleteFileA(path);
    HDC dc = U(CreateMetaFileW)(wp);
    REQUIRE(dc);
    U(Ellipse)(dc, 0, 0, 9, 9);
    HMETAFILE h = U(CloseMetaFile)(dc);
    CHECK(h != NULL);
    if (h) U(DeleteMetaFile)(h);
    CHECK(GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES);
    DeleteFileA(path);
}

TEST(wmf_file_content_equals_bits) {
    WMF_REQ_BASE();
    char path[MAX_PATH]; tmp_path(path, "wmf_fce");
    DeleteFileA(path);
    HDC dc = U(CreateMetaFileA)(path);
    REQUIRE(dc);
    U(MoveToEx)(dc, 3, 4, NULL);
    U(LineTo)(dc, 50, 60);
    U(Rectangle)(dc, 5, 6, 70, 80);
    HMETAFILE h = U(CloseMetaFile)(dc);
    REQUIRE(h);
    WB b;
    CHECK(wb_get(h, &b) != NULL);
    UINT fn; BYTE *f = file_read(path, &fn);
    CHECK(f != NULL);
    if (f && b.p) {
        EXPECT_EQ(fn, b.n);
        if (fn == b.n) {
            /* type word may differ (1 vs 2) but everything else is identical */
            CHECK(memcmp(f + 2, b.p + 2, fn - 2) == 0);
            if (memcmp(f, b.p, 2)) t_note("file mtType %u, bits mtType %u", *(WORD *)f, *(WORD *)b.p);
        }
    }
    free(f); wb_free(&b);
    U(DeleteMetaFile)(h);
    DeleteFileA(path);
}

TEST(wmf_file_header_fields) {
    WMF_REQ_BASE();
    char path[MAX_PATH]; tmp_path(path, "wmf_fhf");
    DeleteFileA(path);
    HDC dc = U(CreateMetaFileA)(path);
    REQUIRE(dc);
    U(Rectangle)(dc, 0, 0, 3, 3);
    HMETAFILE h = U(CloseMetaFile)(dc);
    REQUIRE(h);
    UINT fn; BYTE *f = file_read(path, &fn);
    REQUIRE(f && fn >= 18);
    const WMFHDR *hd = (const WMFHDR *)f;
    EXPECT_EQ(hd->hdrsize, 9);
    EXPECT_EQ(hd->ver, 0x300);
    EXPECT_EQ(hd->nparm, 0);
    EXPECT_EQ(hd->size * 2, fn);
    CHECK(hd->type == 1 || hd->type == 2);
    t_note("on-disk mtType = %u", hd->type);
    free(f);
    U(DeleteMetaFile)(h);
    DeleteFileA(path);
}

TEST(wmf_file_locked_while_open) {
    WMF_REQ_BASE();
    char path[MAX_PATH]; tmp_path(path, "wmf_flk");
    DeleteFileA(path);
    HDC dc = U(CreateMetaFileA)(path);
    REQUIRE(dc);
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    DWORD e = GetLastError();
    CHECK(f == INVALID_HANDLE_VALUE);
    if (f == INVALID_HANDLE_VALUE) EXPECT_EQ(e, ERROR_SHARING_VIOLATION);
    else CloseHandle(f);
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (h) U(DeleteMetaFile)(h);
    /* readable once closed */
    f = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(f != INVALID_HANDLE_VALUE);
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    DeleteFileA(path);
}

TEST(wmf_file_overwrite_existing) {
    WMF_REQ_BASE();
    char path[MAX_PATH]; tmp_path(path, "wmf_fow");
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    REQUIRE(f != INVALID_HANDLE_VALUE);
    BYTE junk[4096]; memset(junk, 0xAB, sizeof junk); DWORD w;
    WriteFile(f, junk, sizeof junk, &w, NULL);
    CloseHandle(f);
    HDC dc = U(CreateMetaFileA)(path);
    REQUIRE(dc);
    HMETAFILE h = U(CloseMetaFile)(dc);
    CHECK(h != NULL);
    UINT fn; BYTE *p = file_read(path, &fn);
    CHECK(p != NULL);
    CHECK(fn < sizeof junk);
    if (p && fn >= 18) { EXPECT_EQ(((WMFHDR *)p)->hdrsize, 9); }
    free(p);
    if (h) U(DeleteMetaFile)(h);
    DeleteFileA(path);
}

TEST(wmf_file_overwrite_with_smaller) {
    WMF_REQ_BASE();
    char path[MAX_PATH]; tmp_path(path, "wmf_fos");
    DeleteFileA(path);
    HDC dc = U(CreateMetaFileA)(path);
    REQUIRE(dc);
    for (int i = 0; i < 100; i++) U(Rectangle)(dc, i, i, i + 10, i + 10);
    HMETAFILE h1 = U(CloseMetaFile)(dc);
    REQUIRE(h1);
    UINT n1; BYTE *p1 = file_read(path, &n1); free(p1);
    U(DeleteMetaFile)(h1);
    dc = U(CreateMetaFileA)(path);
    REQUIRE(dc);
    U(Rectangle)(dc, 0, 0, 1, 1);
    HMETAFILE h2 = U(CloseMetaFile)(dc);
    CHECK(h2 != NULL);
    UINT n2; BYTE *p2 = file_read(path, &n2); free(p2);
    CHECK(n2 < n1);
    EXPECT_EQ(n2, 18 + 14 + 6);
    if (h2) U(DeleteMetaFile)(h2);
    DeleteFileA(path);
}

TEST(wmf_file_relative_path) {
    WMF_REQ_BASE();
    char tmp[MAX_PATH], old[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    GetCurrentDirectoryA(MAX_PATH, old);
    REQUIRE(SetCurrentDirectoryA(tmp));
    char leaf[64]; snprintf(leaf, sizeof leaf, "wmf_rel_%lu.wmf", GetCurrentProcessId());
    DeleteFileA(leaf);
    HDC dc = U(CreateMetaFileA)(leaf);
    CHECK(dc != NULL);
    if (dc) { HMETAFILE h = U(CloseMetaFile)(dc); if (h) U(DeleteMetaFile)(h); }
    CHECK(GetFileAttributesA(leaf) != INVALID_FILE_ATTRIBUTES);
    DeleteFileA(leaf);
    SetCurrentDirectoryA(old);
}

TEST(wmf_file_path_not_found) {
    WMF_REQ_BASE();
    char path[MAX_PATH]; char d[MAX_PATH];
    GetTempPathA(MAX_PATH, d);
    snprintf(path, sizeof path, "%swmf_nodir_%lu\\x\\a.wmf", d, GetCurrentProcessId());
    SET_ERR(0);
    HDC dc = U(CreateMetaFileA)(path);
    CHECK(dc == NULL);
    EXPECT_ERR(dc, ERROR_PATH_NOT_FOUND);
    if (dc) { HMETAFILE h = U(CloseMetaFile)(dc); if (h) U(DeleteMetaFile)(h); }
}

TEST(wmf_file_path_not_found_w) {
    WMF_REQ_BASE(); REQUIRE(U(CreateMetaFileW));
    WCHAR path[MAX_PATH];
    WCHAR d[MAX_PATH]; GetTempPathW(MAX_PATH, d);
    wsprintfW(path, L"%swmf_nodirw_%lu\\a.wmf", d, GetCurrentProcessId());
    SET_ERR(0);
    HDC dc = U(CreateMetaFileW)(path);
    CHECK(dc == NULL);
    EXPECT_ERR(dc, ERROR_PATH_NOT_FOUND);
}

TEST(wmf_file_name_too_long_a) {
    WMF_REQ_BASE();
    static char path[600];
    int lens[] = { 260, 261, 300, 500 };
    for (size_t i = 0; i < sizeof lens / sizeof *lens; i++) {
        memset(path, 'a', lens[i]); path[lens[i]] = 0;
        SET_ERR(0);
        HDC dc = U(CreateMetaFileA)(path);
        CHECKF(dc == NULL, "len %d", lens[i]);
        EXPECT_ERR(dc, ERROR_FILENAME_EXCED_RANGE);
        if (dc) { HMETAFILE h = U(CloseMetaFile)(dc); if (h) U(DeleteMetaFile)(h); }
    }
}

TEST(wmf_file_name_too_long_w) {
    WMF_REQ_BASE(); REQUIRE(U(CreateMetaFileW));
    static WCHAR path[700];
    for (int i = 0; i < 400; i++) path[i] = L'b';
    path[400] = 0;
    SET_ERR(0);
    HDC dc = U(CreateMetaFileW)(path);
    CHECK(dc == NULL);
    DWORD e = GetLastError();
    CHECKF(e != 0, "error %lu", e);
    t_note("CreateMetaFileW(400 chars) error %lu", e);
    if (dc) { HMETAFILE h = U(CloseMetaFile)(dc); if (h) U(DeleteMetaFile)(h); }
}

TEST(wmf_file_empty_name) {
    WMF_REQ_BASE();
    SET_ERR(0);
    HDC dc = U(CreateMetaFileA)("");
    t_note("CreateMetaFileA(\"\") = %p err %lu", dc, GetLastError());
    if (dc) { HMETAFILE h = U(CloseMetaFile)(dc); if (h) U(DeleteMetaFile)(h); }
}

TEST(wmf_file_leak_cycle) {
    WMF_REQ_BASE(); REQUIRE(U(GetGuiResources));
    char path[MAX_PATH]; tmp_path(path, "wmf_flc");
    DWORD before = gdi_count();
    for (int i = 0; i < 30; i++) {
        HDC dc = U(CreateMetaFileA)(path);
        if (!dc) { CHECK(0); break; }
        U(Rectangle)(dc, 0, 0, 5, 5);
        HMETAFILE h = U(CloseMetaFile)(dc);
        if (h) U(DeleteMetaFile)(h);
    }
    EXPECT_EQ(gdi_count(), before);
    DeleteFileA(path);
}

TEST(wmf_file_get_nonexistent) {
    WMF_REQ_BASE(); REQUIRE(U(GetMetaFileA));
    char path[MAX_PATH]; tmp_path(path, "wmf_nonex");
    DeleteFileA(path);
    SET_ERR(0);
    HMETAFILE h = U(GetMetaFileA)(path);
    CHECK(h == NULL);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND, "error %lu", e);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_file_get_null_name) {
    WMF_REQ_BASE(); REQUIRE(U(GetMetaFileA));
    SET_ERR(0);
    HMETAFILE h = U(GetMetaFileA)(NULL);
    t_note("GetMetaFileA(NULL) = %p err %lu", h, GetLastError());
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_file_get_garbage_file) {
    WMF_REQ_BASE(); REQUIRE(U(GetMetaFileA));
    char path[MAX_PATH]; tmp_path(path, "wmf_garb");
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    REQUIRE(f != INVALID_HANDLE_VALUE);
    BYTE junk[64]; for (int i = 0; i < 64; i++) junk[i] = (BYTE)(i * 7 + 3); DWORD w;
    WriteFile(f, junk, sizeof junk, &w, NULL);
    CloseHandle(f);
    SET_ERR(0);
    HMETAFILE h = U(GetMetaFileA)(path);
    CHECK(h == NULL);
    if (h) U(DeleteMetaFile)(h);
    DeleteFileA(path);
}

TEST(wmf_file_get_empty_file) {
    WMF_REQ_BASE(); REQUIRE(U(GetMetaFileA));
    char path[MAX_PATH]; tmp_path(path, "wmf_empty");
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    REQUIRE(f != INVALID_HANDLE_VALUE);
    CloseHandle(f);
    HMETAFILE h = U(GetMetaFileA)(path);
    CHECK(h == NULL);
    if (h) U(DeleteMetaFile)(h);
    DeleteFileA(path);
}

TEST(wmf_file_get_short_file) {
    WMF_REQ_BASE(); REQUIRE(U(GetMetaFileA));
    char path[MAX_PATH]; tmp_path(path, "wmf_short");
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    REQUIRE(f != INVALID_HANDLE_VALUE);
    WMFHDR hd = { 1, 9, 0x300, 9, 0, 0, 0 }; DWORD w;
    WriteFile(f, &hd, 10, &w, NULL);   /* header truncated */
    CloseHandle(f);
    HMETAFILE h = U(GetMetaFileA)(path);
    CHECK(h == NULL);
    if (h) U(DeleteMetaFile)(h);
    DeleteFileA(path);
}
