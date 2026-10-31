#include "emf.h"

/* ---------- CreateEnhMetaFile / CloseEnhMetaFile basics ---------- */

TEST(emf_create_null_everything) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    DWORD g0 = emf_gdi_count();
    HDC dc = emf_begin();
    CHECK(dc != NULL);
    if (!dc) return;
    HENHMETAFILE h = emf_end(dc);
    CHECK(h != NULL);
    CHECK(U(DeleteEnhMetaFile)(h));
    EXPECT_EQ(emf_gdi_count(), g0);
}

TEST(emf_create_a_variant) {
    REQUIRE(U(CreateEnhMetaFileA)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC dc = U(CreateEnhMetaFileA)(NULL, NULL, NULL, NULL);
    CHECK(dc != NULL);
    if (!dc) return;
    HENHMETAFILE h = emf_end(dc);
    CHECK(h != NULL);
    CHECK(U(DeleteEnhMetaFile)(h));
}

TEST(emf_create_object_types) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(GetObjectType));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    EXPECT_EQ(U(GetObjectType)(dc), OBJ_ENHMETADC);
    HENHMETAFILE h = emf_end(dc);
    EXPECT_EQ(U(GetObjectType)(h), OBJ_ENHMETAFILE);
    EXPECT_EQ(U(GetObjectType)(dc), 0);
    U(DeleteEnhMetaFile)(h);
    EXPECT_EQ(U(GetObjectType)(h), 0);
}

TEST(emf_create_dc_handle_distinct) {
    REQUIRE(U(CreateEnhMetaFileW));
    HDC a = emf_begin(), b = emf_begin();
    CHECK(a && b && a != b);
    if (a) U(DeleteEnhMetaFile)(emf_end(a));
    if (b) U(DeleteEnhMetaFile)(emf_end(b));
}

TEST(emf_create_ref_screen_dc) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC scr = U(GetDC)(NULL);
    HDC dc = U(CreateEnhMetaFileW)(scr, NULL, NULL, NULL);
    CHECK(dc != NULL);
    if (dc) U(DeleteEnhMetaFile)(emf_end(dc));
    U(ReleaseDC)(NULL, scr);
}

TEST(emf_create_ref_memory_dc) {
    REQUIRE(U(CreateEnhMetaFileW));
    emf_dib d;
    if (!emf_dib_make(&d, 64, 48, 0)) { t_skip("no dib"); return; }
    HDC dc = U(CreateEnhMetaFileW)(d.dc, NULL, NULL, NULL);
    CHECK(dc != NULL);
    if (dc) U(DeleteEnhMetaFile)(emf_end(dc));
    emf_dib_free(&d);
}

TEST(emf_create_ref_memory_dc_default_bitmap) {
    REQUIRE(U(CreateEnhMetaFileW));
    HDC m = U(CreateCompatibleDC)(NULL);
    HDC dc = U(CreateEnhMetaFileW)(m, NULL, NULL, NULL);
    CHECK(dc != NULL);
    if (dc) U(DeleteEnhMetaFile)(emf_end(dc));
    U(DeleteDC)(m);
}

TEST(emf_create_ref_bogus_handles) {
    REQUIRE(U(CreateEnhMetaFileW));
    static const ULONG_PTR bogus[] = { 1, 0x1234, 0xdeadbeef, 0x00010001, 0xffffffffull };
    for (unsigned i = 0; i < sizeof bogus / sizeof bogus[0]; i++) {
        HDC dc = U(CreateEnhMetaFileW)((HDC)bogus[i], NULL, NULL, NULL);
        CHECKF(dc == NULL, "bogus ref %llx", (unsigned long long)bogus[i]);
        if (dc) U(DeleteEnhMetaFile)(emf_end(dc));
    }
}

TEST(emf_create_ref_wrong_object_type) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CreatePen)); REQUIRE(U(CreateSolidBrush));
    HGDIOBJ objs[3];
    objs[0] = U(CreatePen)(PS_SOLID, 1, 0);
    objs[1] = U(CreateSolidBrush)(0x123456);
    objs[2] = U(GetStockObject)(BLACK_BRUSH);
    for (int i = 0; i < 3; i++) {
        HDC dc = U(CreateEnhMetaFileW)((HDC)objs[i], NULL, NULL, NULL);
        CHECKF(dc == NULL, "obj %d", i);
        if (dc) U(DeleteEnhMetaFile)(emf_end(dc));
    }
    U(DeleteObject)(objs[0]); U(DeleteObject)(objs[1]);
}

TEST(emf_create_empty_frame_rects) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(GetDC));
    static const RECT bad[] = {
        {10, 10, 10, 20}, {10, 10, 20, 10}, {50, 0, 10, 10}, {0, 50, 10, 10},
        {0, 0, 0, 0}, {5, 5, 5, 5}, {100, 100, 0, 0}, {-5, -5, -5, 5}, {-5, -5, -10, -10},
    };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0xabcd);
        HDC dc = U(CreateEnhMetaFileW)(NULL, NULL, &bad[i], NULL);
        CHECKF(dc == NULL, "rect %u", i);
        if (!dc) EXPECT_ERR(0, ERROR_INVALID_PARAMETER); else U(DeleteEnhMetaFile)(emf_end(dc));
    }
}

TEST(emf_create_empty_frame_rects_a) {
    REQUIRE(U(CreateEnhMetaFileA));
    RECT r = {7, 7, 7, 70};
    SetLastError(0);
    HDC dc = U(CreateEnhMetaFileA)(NULL, NULL, &r, NULL);
    CHECK(dc == NULL);
    if (!dc) EXPECT_ERR(0, ERROR_INVALID_PARAMETER); else U(DeleteEnhMetaFile)(emf_end(dc));
}

TEST(emf_create_valid_frame_rects) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(GetEnhMetaFileHeader));
    static const RECT ok[] = {
        {0, 0, 1, 1}, {0, 0, 100, 100}, {-100, -100, 100, 100}, {-100000, -50000, -1, -1},
        {0, 0, 2000000, 1500000}, {1000, 2000, 3000, 4000}, {-1, -1, 0, 0},
    };
    for (unsigned i = 0; i < sizeof ok / sizeof ok[0]; i++) {
        HDC dc = U(CreateEnhMetaFileW)(NULL, NULL, &ok[i], NULL);
        CHECKF(dc != NULL, "rect %u", i);
        if (!dc) continue;
        HENHMETAFILE h = emf_end(dc);
        ENHMETAHEADER hd; memset(&hd, 0, sizeof hd);
        CHECK(U(GetEnhMetaFileHeader)(h, sizeof hd, &hd) != 0);
        CHECKF(!memcmp(&hd.rclFrame, &ok[i], sizeof(RECT)), "frame %u [%ld %ld %ld %ld]", i,
               hd.rclFrame.left, hd.rclFrame.top, hd.rclFrame.right, hd.rclFrame.bottom);
        U(DeleteEnhMetaFile)(h);
    }
}

TEST(emf_create_bad_ref_empty_frame_no_leak) {
    REQUIRE(U(CreateEnhMetaFileW));
    DWORD g0 = emf_gdi_count();
    RECT r = {1, 1, 1, 1};
    for (int i = 0; i < 100; i++) {
        HDC dc = U(CreateEnhMetaFileW)(NULL, NULL, &r, NULL);
        if (dc) { U(DeleteEnhMetaFile)(emf_end(dc)); CHECK(0); }
    }
    EXPECT_EQ(emf_gdi_count(), g0);
}

TEST(emf_create_bad_path) {
    REQUIRE(U(CreateEnhMetaFileW));
    static const WCHAR *paths[] = {
        L"Z:\\g32emf_no_such_dir_\\sub\\x.emf", L"C:\\g32emf_no_such_dir_\\x.emf",
        L"\\\\?\\Q:\\nonexistent\\x.emf", L"C:\\Windows\\System32\\g32emf_bad|name?.emf",
    };
    for (unsigned i = 0; i < sizeof paths / sizeof paths[0]; i++) {
        SetLastError(0);
        HDC dc = U(CreateEnhMetaFileW)(NULL, paths[i], NULL, NULL);
        CHECKF(dc == NULL, "path %u", i);
        if (dc) U(DeleteEnhMetaFile)(emf_end(dc));
    }
}

TEST(emf_create_bad_path_a) {
    REQUIRE(U(CreateEnhMetaFileA));
    HDC dc = U(CreateEnhMetaFileA)(NULL, "Z:\\g32emf_no_such_dir_\\x.emf", NULL, NULL);
    CHECK(dc == NULL);
    if (dc) U(DeleteEnhMetaFile)(emf_end(dc));
}

TEST(emf_create_bad_path_no_leak) {
    REQUIRE(U(CreateEnhMetaFileW));
    DWORD g0 = emf_gdi_count();
    for (int i = 0; i < 50; i++) {
        HDC dc = U(CreateEnhMetaFileW)(NULL, L"Z:\\g32emf_no_such_dir_\\x.emf", NULL, NULL);
        if (dc) U(DeleteEnhMetaFile)(emf_end(dc));
    }
    EXPECT_EQ(emf_gdi_count(), g0);
}

/* ---------- file based ---------- */

TEST(emf_file_create_and_close) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(GetEnhMetaFileBits)); REQUIRE(U(Rectangle));
    WCHAR p[MAX_PATH]; emf_tmpname(p, L"fc");
    HDC dc = U(CreateEnhMetaFileW)(NULL, p, NULL, L"file\0test\0");
    if (!dc) { CHECK(dc != NULL); return; }
    U(Rectangle)(dc, 5, 5, 30, 30);
    HENHMETAFILE h = emf_end(dc);
    CHECK(h != NULL);
    UINT n = 0; BYTE *b = h ? emf_bits(h, &n) : NULL;
    DWORD fn = 0; BYTE *fb = emf_readfile(p, &fn);
    CHECK(b && fb);
    if (b && fb) {
        EXPECT_EQ(fn, n);
        CHECK(fn == n && !memcmp(b, fb, n));
    }
    free(b); free(fb);
    if (h) U(DeleteEnhMetaFile)(h);
    if (!DeleteFileW(p)) t_note("could not delete temp emf: %lu", GetLastError());
}

TEST(emf_file_header_signature_on_disk) {
    REQUIRE(U(CreateEnhMetaFileW));
    WCHAR p[MAX_PATH]; emf_tmpname(p, L"sig");
    HDC dc = U(CreateEnhMetaFileW)(NULL, p, NULL, NULL);
    if (!dc) { CHECK(dc != NULL); return; }
    U(PatBlt)(dc, 0, 0, 10, 10, BLACKNESS);
    HENHMETAFILE h = emf_end(dc);
    DWORD fn = 0; BYTE *fb = emf_readfile(p, &fn);
    CHECK(fb != NULL);
    if (fb && fn >= 88) {
        const ENHMETAHEADER *hd = (const ENHMETAHEADER *)fb;
        EXPECT_EQ(hd->iType, 1);
        EXPECT_EQ(hd->dSignature, 0x464D4520);
        EXPECT_EQ(hd->nVersion, 0x10000);
        EXPECT_EQ(hd->nBytes, fn);
        CHECK(emf_count_recs(fb, fn) == (int)hd->nRecords);
    }
    free(fb);
    if (h) U(DeleteEnhMetaFile)(h);
    DeleteFileW(p);
}

TEST(emf_file_overwrite_existing) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(Rectangle));
    WCHAR p[MAX_PATH]; emf_tmpname(p, L"ow");
    char junk[3000]; memset(junk, 0x5a, sizeof junk);
    CHECK(emf_writefile(p, junk, sizeof junk));
    HDC dc = U(CreateEnhMetaFileW)(NULL, p, NULL, NULL);
    CHECK(dc != NULL);
    if (dc) {
        U(Rectangle)(dc, 1, 1, 9, 9);
        HENHMETAFILE h = emf_end(dc);
        DWORD fn = 0; BYTE *fb = emf_readfile(p, &fn);
        CHECK(fb && fn >= 88 && fn < sizeof junk);
        if (fb && fn >= 88) EXPECT_EQ(((ENHMETAHEADER *)fb)->dSignature, 0x464D4520);
        free(fb);
        if (h) U(DeleteEnhMetaFile)(h);
    }
    DeleteFileW(p);
}

TEST(emf_file_second_write_shrinks) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(Polyline));
    WCHAR p[MAX_PATH]; emf_tmpname(p, L"shr");
    POINT pts[400];
    for (int i = 0; i < 400; i++) { pts[i].x = i; pts[i].y = i & 31; }
    DWORD sz[2] = {0, 0};
    for (int k = 0; k < 2; k++) {
        HDC dc = U(CreateEnhMetaFileW)(NULL, p, NULL, NULL);
        if (!dc) { CHECK(dc != NULL); break; }
        U(Polyline)(dc, pts, k == 0 ? 400 : 4);
        HENHMETAFILE h = emf_end(dc);
        DWORD fn = 0; BYTE *fb = emf_readfile(p, &fn);
        sz[k] = fn; free(fb);
        if (h) U(DeleteEnhMetaFile)(h);
    }
    CHECK(sz[0] > 1000 && sz[1] > 0 && sz[1] < sz[0]);
    DeleteFileW(p);
}

TEST(emf_file_delete_hemf_keeps_file) {
    REQUIRE(U(CreateEnhMetaFileW));
    WCHAR p[MAX_PATH]; emf_tmpname(p, L"keep");
    HDC dc = U(CreateEnhMetaFileW)(NULL, p, NULL, NULL);
    if (!dc) { CHECK(dc != NULL); return; }
    HENHMETAFILE h = emf_end(dc);
    CHECK(U(DeleteEnhMetaFile)(h));
    CHECK(GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES);
    CHECK(DeleteFileW(p));
}

TEST(emf_file_a_variant) {
    REQUIRE(U(CreateEnhMetaFileA)); REQUIRE(U(GetEnhMetaFileA));
    char tmp[MAX_PATH]; char p[MAX_PATH + 64];
    GetTempPathA(MAX_PATH, tmp);
    wsprintfA(p, "%sg32emf_a_%lu.emf", tmp, GetCurrentProcessId());
    HDC dc = U(CreateEnhMetaFileA)(NULL, p, NULL, NULL);
    if (!dc) { CHECK(dc != NULL); return; }
    U(PatBlt)(dc, 0, 0, 5, 5, BLACKNESS);
    HENHMETAFILE h = emf_end(dc);
    CHECK(h != NULL);
    HENHMETAFILE h2 = U(GetEnhMetaFileA)(p);
    CHECK(h2 != NULL);
    if (h2) {
        UINT n1 = U(GetEnhMetaFileBits)(h, 0, NULL), n2 = U(GetEnhMetaFileBits)(h2, 0, NULL);
        EXPECT_EQ(n1, n2);
        U(DeleteEnhMetaFile)(h2);
    }
    if (h) U(DeleteEnhMetaFile)(h);
    DeleteFileA(p);
}

TEST(emf_file_abandoned_dc_cleanup) {
    /* Closing and deleting immediately must not leave the file locked. */
    REQUIRE(U(CreateEnhMetaFileW));
    WCHAR p[MAX_PATH]; emf_tmpname(p, L"lock");
    HDC dc = U(CreateEnhMetaFileW)(NULL, p, NULL, NULL);
    if (!dc) { CHECK(dc != NULL); return; }
    U(DeleteEnhMetaFile)(emf_end(dc));
    CHECK(DeleteFileW(p));
}

/* ---------- close ---------- */

TEST(emf_close_null) {
    REQUIRE(U(CloseEnhMetaFile));
    SetLastError(0);
    CHECK(U(CloseEnhMetaFile)(NULL) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}

TEST(emf_close_bogus_handles) {
    REQUIRE(U(CloseEnhMetaFile));
    static const ULONG_PTR bogus[] = { 1, 0x1234, 0xdeadbeef, 0x00210000, 0x00460000 };
    for (unsigned i = 0; i < sizeof bogus / sizeof bogus[0]; i++) {
        SetLastError(0);
        CHECKF(U(CloseEnhMetaFile)((HDC)bogus[i]) == NULL, "bogus %u", i);
    }
}

TEST(emf_close_memory_dc_is_not_emf) {
    REQUIRE(U(CloseEnhMetaFile));
    HDC m = U(CreateCompatibleDC)(NULL);
    CHECK(U(CloseEnhMetaFile)(m) == NULL);
    /* the DC must still be usable and deletable */
    CHECK(U(GetObjectType)(m) == OBJ_MEMDC);
    CHECK(U(DeleteDC)(m));
}

TEST(emf_close_screen_dc_is_not_emf) {
    REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(GetDC));
    HDC s = U(GetDC)(NULL);
    CHECK(U(CloseEnhMetaFile)(s) == NULL);
    CHECK(U(GetObjectType)(s) == OBJ_DC);
    U(ReleaseDC)(NULL, s);
}

TEST(emf_close_wmf_dc_fails) {
    REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile));
    HDC w = U(CreateMetaFileW)(NULL);
    if (!w) { t_skip("no wmf dc"); return; }
    SetLastError(0);
    CHECK(U(CloseEnhMetaFile)(w) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    HMETAFILE mf = U(CloseMetaFile)(w);
    CHECK(mf != NULL);
    if (mf) U(DeleteMetaFile)(mf);
}

TEST(emf_close_twice) {
    REQUIRE(U(CloseEnhMetaFile));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    HENHMETAFILE h = emf_end(dc);
    CHECK(h != NULL);
    SetLastError(0);
    CHECK(U(CloseEnhMetaFile)(dc) == NULL);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_close_invalidates_dc_for_drawing) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(Rectangle));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    HENHMETAFILE h = emf_end(dc);
    CHECK(U(Rectangle)(dc, 0, 0, 10, 10) == FALSE);
    CHECK(U(GetMapMode)(dc) == 0);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_close_restores_saved_dcs) {
    /* Unbalanced SaveDC levels do not prevent closing. */
    REQUIRE(U(SaveDC));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    for (int i = 0; i < 5; i++) CHECK(U(SaveDC)(dc) > 0);
    HENHMETAFILE h = emf_end(dc);
    CHECK(h != NULL);
    if (h) U(DeleteEnhMetaFile)(h);
}

TEST(emf_close_with_selected_objects) {
    REQUIRE(U(CreatePen)); REQUIRE(U(CreateSolidBrush));
    DWORD g0 = emf_gdi_count();
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    HPEN p = U(CreatePen)(PS_SOLID, 2, RGB(1, 2, 3));
    HBRUSH b = U(CreateSolidBrush)(RGB(9, 8, 7));
    U(SelectObject)(dc, p); U(SelectObject)(dc, b);
    HENHMETAFILE h = emf_end(dc);
    CHECK(h != NULL);
    /* objects are ordinary client-side objects and survive the DC */
    CHECK(U(DeleteObject)(p)); CHECK(U(DeleteObject)(b));
    if (h) U(DeleteEnhMetaFile)(h);
    EXPECT_EQ(emf_gdi_count(), g0);
}

TEST(emf_close_pen_survives) {
    REQUIRE(U(CreatePen));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0);
    U(SelectObject)(dc, p);
    HENHMETAFILE h = emf_end(dc);
    LOGPEN lp;
    CHECK(U(GetObjectW)(p, sizeof lp, &lp) == sizeof lp);
    CHECK(U(DeleteObject)(p));
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_close_many_concurrent_dcs) {
    REQUIRE(U(CreateEnhMetaFileW));
    enum { N = 150 };
    HDC dcs[N]; int made = 0;
    DWORD g0 = emf_gdi_count();
    for (int i = 0; i < N; i++) { dcs[i] = emf_begin(); if (!dcs[i]) break; made++; }
    CHECK(made == N);
    for (int i = 0; i < made; i++) {
        U(Rectangle)(dcs[i], i, i, i + 10, i + 10);
    }
    for (int i = made - 1; i >= 0; i--) {
        HENHMETAFILE h = emf_end(dcs[i]);
        CHECK(h != NULL);
        if (h) U(DeleteEnhMetaFile)(h);
    }
    EXPECT_EQ(emf_gdi_count(), g0);
}

TEST(emf_close_loop_no_leak) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CreatePen));
    DWORD g0 = emf_gdi_count();
    for (int i = 0; i < 1000; i++) {
        HDC dc = emf_begin();
        if (!dc) { CHECK(dc != NULL); break; }
        HPEN p = U(CreatePen)(PS_SOLID, 1, i);
        HGDIOBJ old = U(SelectObject)(dc, p);
        U(Rectangle)(dc, 0, 0, 20, 20);
        U(SelectObject)(dc, old);
        U(DeleteObject)(p);
        U(DeleteEnhMetaFile)(emf_end(dc));
    }
    EXPECT_EQ(emf_gdi_count(), g0);
}

TEST(emf_delete_leaves_dc_unreachable) {
    REQUIRE(U(DeleteEnhMetaFile));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    U(DeleteEnhMetaFile)(emf_end(dc));
    EXPECT_EQ(U(GetObjectType)(dc), 0);
}

/* ---------- device caps on EMF DC ---------- */

TEST(emf_dc_caps_follow_reference_screen) {
    REQUIRE(U(GetDeviceCaps)); REQUIRE(U(GetDC));
    HDC scr = U(GetDC)(NULL);
    HDC dc = U(CreateEnhMetaFileW)(scr, NULL, NULL, NULL);
    if (!dc) { CHECK(dc != NULL); U(ReleaseDC)(NULL, scr); return; }
    static const int caps[] = { HORZRES, VERTRES, HORZSIZE, VERTSIZE, LOGPIXELSX, LOGPIXELSY, BITSPIXEL, PLANES, TECHNOLOGY };
    for (unsigned i = 0; i < sizeof caps / sizeof caps[0]; i++) {
        int a = U(GetDeviceCaps)(dc, caps[i]), b = U(GetDeviceCaps)(scr, caps[i]);
        CHECKF(a == b, "cap %d: emf %d ref %d", caps[i], a, b);
    }
    U(DeleteEnhMetaFile)(emf_end(dc));
    U(ReleaseDC)(NULL, scr);
}

TEST(emf_dc_caps_technology_is_reference) {
    REQUIRE(U(GetDeviceCaps));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    int t = U(GetDeviceCaps)(dc, TECHNOLOGY);
    CHECKF(t == DT_RASDISPLAY || t == DT_METAFILE, "technology %d", t);
    U(DeleteEnhMetaFile)(emf_end(dc));
}

TEST(emf_dc_initial_state) {
    REQUIRE(U(GetMapMode)); REQUIRE(U(GetBkMode)); REQUIRE(U(GetROP2));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    EXPECT_EQ(U(GetMapMode)(dc), MM_TEXT);
    EXPECT_EQ(U(GetBkMode)(dc), OPAQUE);
    EXPECT_EQ(U(GetROP2)(dc), R2_COPYPEN);
    EXPECT_EQ(U(GetPolyFillMode)(dc), ALTERNATE);
    EXPECT_EQ(U(GetGraphicsMode)(dc), GM_COMPATIBLE);
    EXPECT_EQ(U(GetBkColor)(dc), 0xFFFFFF);
    EXPECT_EQ(U(GetTextColor)(dc), 0);
    EXPECT_EQ(U(GetCurrentObject)(dc, OBJ_PEN), U(GetStockObject)(BLACK_PEN));
    EXPECT_EQ(U(GetCurrentObject)(dc, OBJ_BRUSH), U(GetStockObject)(WHITE_BRUSH));
    U(DeleteEnhMetaFile)(emf_end(dc));
}

TEST(emf_dc_state_changes_are_tracked) {
    REQUIRE(U(SetMapMode)); REQUIRE(U(SetBkMode));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    EXPECT_EQ(U(SetMapMode)(dc, MM_HIMETRIC), MM_TEXT);
    EXPECT_EQ(U(GetMapMode)(dc), MM_HIMETRIC);
    EXPECT_EQ(U(SetBkMode)(dc, TRANSPARENT), OPAQUE);
    EXPECT_EQ(U(GetBkMode)(dc), TRANSPARENT);
    EXPECT_EQ(U(SetBkColor)(dc, RGB(1, 2, 3)), 0xFFFFFF);
    EXPECT_EQ(U(GetBkColor)(dc), RGB(1, 2, 3));
    EXPECT_EQ(U(SetROP2)(dc, R2_XORPEN), R2_COPYPEN);
    U(DeleteEnhMetaFile)(emf_end(dc));
}

TEST(emf_dc_savedc_levels) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    int a = U(SaveDC)(dc), b = U(SaveDC)(dc);
    CHECK(a >= 1 && b == a + 1);
    CHECK(U(RestoreDC)(dc, -1));
    CHECK(U(RestoreDC)(dc, a));
    U(DeleteEnhMetaFile)(emf_end(dc));
}

TEST(emf_dc_selectobject_returns_previous) {
    REQUIRE(U(CreatePen));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    HPEN p = U(CreatePen)(PS_SOLID, 3, RGB(255, 0, 0));
    HGDIOBJ old = U(SelectObject)(dc, p);
    EXPECT_EQ(old, U(GetStockObject)(BLACK_PEN));
    EXPECT_EQ(U(GetCurrentObject)(dc, OBJ_PEN), p);
    HGDIOBJ prev = U(SelectObject)(dc, old);
    EXPECT_EQ(prev, p);
    U(DeleteObject)(p);
    U(DeleteEnhMetaFile)(emf_end(dc));
}

TEST(emf_dc_selectobject_stock) {
    REQUIRE(U(GetStockObject));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    HGDIOBJ gray = U(GetStockObject)(GRAY_BRUSH);
    HGDIOBJ old = U(SelectObject)(dc, gray);
    EXPECT_EQ(old, U(GetStockObject)(WHITE_BRUSH));
    EXPECT_EQ(U(GetCurrentObject)(dc, OBJ_BRUSH), gray);
    U(SelectObject)(dc, old);
    U(DeleteEnhMetaFile)(emf_end(dc));
}
