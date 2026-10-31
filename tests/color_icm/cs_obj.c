#include "cih.h"

/* ---- GetLogColorSpaceW / A argument handling ---- */
TEST(color_icm_getlogcs_w_null_buffer) {
    CI_NOWINE();
    REQUIRE(U(GetLogColorSpaceW)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    HCOLORSPACE h = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    if (!h) { t_skip("no color space"); return; }
    SetLastError(0xdeadbeef);
    BOOL r = U(GetLogColorSpaceW)(h, NULL, CI_LCSW_SIZE);
    EXPECT_EQ(r, 0);
    EXPECT_ERR(r, ERROR_INSUFFICIENT_BUFFER);
    SetLastError(0xdeadbeef);
    r = U(GetLogColorSpaceW)(h, NULL, 0);
    EXPECT_EQ(r, 0);
    EXPECT_ERR(r, ERROR_INSUFFICIENT_BUFFER);
    U(DeleteColorSpace)(h);
}
TEST(color_icm_getlogcs_a_null_buffer) {
    CI_NOWINE();
    REQUIRE(U(GetLogColorSpaceA)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    HCOLORSPACE h = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    if (!h) { t_skip("no color space"); return; }
    SetLastError(0xdeadbeef);
    BOOL r = U(GetLogColorSpaceA)(h, NULL, CI_LCSA_SIZE);
    EXPECT_EQ(r, 0);
    EXPECT_ERR(r, ERROR_INSUFFICIENT_BUFFER);
    U(DeleteColorSpace)(h);
}
TEST(color_icm_getlogcs_w_size_table) {
    CI_NOWINE();
    REQUIRE(U(GetLogColorSpaceW)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    HCOLORSPACE h = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    if (!h) { t_skip("no color space"); return; }
    static const struct { DWORD n; int ok; } t[] = {
        {0, 0}, {1, 0}, {4, 0}, {0x148, 0}, {0x24b, 0}, {0x24c, 1}, {0x24d, 1}, {0x1000, 1},
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        LOGCOLORSPACEW *b = calloc(1, 4096);
        DWORD n = t[i].n;
        SetLastError(0xdeadbeef);
        BOOL r = U(GetLogColorSpaceW)(h, b, n);
        CHECKF(!!r == t[i].ok, "n=0x%lx r=%d", n, r);
        if (!t[i].ok) {
            CHECKF(GetLastError() == ERROR_INSUFFICIENT_BUFFER, "n=0x%lx err=%lu", n, GetLastError());
            EXPECT_EQ(b->lcsSignature, 0);
        } else {
            EXPECT_EQ(b->lcsSignature, 0x50534F43);
        }
        free(b);
    }
    U(DeleteColorSpace)(h);
}
TEST(color_icm_getlogcs_a_size_table) {
    CI_NOWINE();
    REQUIRE(U(GetLogColorSpaceA)); REQUIRE(U(CreateColorSpaceA)); REQUIRE(U(DeleteColorSpace));
    HCOLORSPACE h = ci_mka(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    if (!h) { t_skip("no color space"); return; }
    static const struct { DWORD n; int ok; } t[] = {
        {0, 0}, {1, 0}, {0x100, 0}, {0x147, 0}, {0x148, 1}, {0x149, 1}, {0x24c, 1}, {0x1000, 1},
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        LOGCOLORSPACEA *b = calloc(1, 4096);
        SetLastError(0xdeadbeef);
        BOOL r = U(GetLogColorSpaceA)(h, b, t[i].n);
        CHECKF(!!r == t[i].ok, "n=0x%lx r=%d", t[i].n, r);
        if (!t[i].ok) {
            CHECKF(GetLastError() == ERROR_INSUFFICIENT_BUFFER, "n=0x%lx err=%lu", t[i].n, GetLastError());
        } else {
            EXPECT_EQ(b->lcsSize, CI_LCSA_SIZE);
            EXPECT_EQ(b->lcsSignature, 0x50534F43);
        }
        free(b);
    }
    U(DeleteColorSpace)(h);
}
TEST(color_icm_getlogcs_w_bad_handles) {
    CI_NOWINE();
    REQUIRE(U(GetLogColorSpaceW)); REQUIRE(U(CreatePen)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(CreateCompatibleBitmap));
    LOGCOLORSPACEW o;
    HGDIOBJ pen = U(CreatePen)(PS_SOLID, 1, 0);
    HGDIOBJ br = U(CreateSolidBrush)(0x123456);
    HDC mem = U(CreateCompatibleDC)(NULL);
    HGDIOBJ bad[] = { NULL, (HGDIOBJ)(ULONG_PTR)0xdeadbeef, (HGDIOBJ)(ULONG_PTR)1, pen, br, U(GetStockObject)(BLACK_PEN),
                      U(GetStockObject)(SYSTEM_FONT), (HGDIOBJ)mem, (HGDIOBJ)(ULONG_PTR)0x00090000 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0xdeadbeef);
        BOOL r = U(GetLogColorSpaceW)((HCOLORSPACE)bad[i], &o, sizeof o);
        CHECKF(r == 0, "i=%u", i);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "i=%u err=%lu", i, GetLastError());
    }
    U(DeleteObject)(pen); U(DeleteObject)(br); U(DeleteDC)(mem);
}
TEST(color_icm_getlogcs_a_bad_handles) {
    CI_NOWINE();
    REQUIRE(U(GetLogColorSpaceA)); REQUIRE(U(CreatePen));
    LOGCOLORSPACEA o;
    HGDIOBJ pen = U(CreatePen)(PS_SOLID, 1, 0);
    HGDIOBJ bad[] = { NULL, (HGDIOBJ)(ULONG_PTR)0xdeadbeef, pen, U(GetStockObject)(WHITE_BRUSH) };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0xdeadbeef);
        BOOL r = U(GetLogColorSpaceA)((HCOLORSPACE)bad[i], &o, sizeof o);
        CHECKF(r == 0, "i=%u", i);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "i=%u err=%lu", i, GetLastError());
    }
    U(DeleteObject)(pen);
}
TEST(color_icm_getlogcs_deleted_handle) {
    CI_NOWINE();
    REQUIRE(U(GetLogColorSpaceW)); REQUIRE(U(GetLogColorSpaceA)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    HCOLORSPACE h = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    if (!h) { t_skip("no color space"); return; }
    CHECK(U(DeleteColorSpace)(h));
    LOGCOLORSPACEW w; LOGCOLORSPACEA a;
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetLogColorSpaceW)(h, &w, sizeof w), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetLogColorSpaceA)(h, &a, sizeof a), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
}
TEST(color_icm_getlogcs_size_check_precedes_handle) {
    CI_NOWINE();
    REQUIRE(U(GetLogColorSpaceW)); REQUIRE(U(GetLogColorSpaceA));
    LOGCOLORSPACEW w; LOGCOLORSPACEA a;
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetLogColorSpaceW)(NULL, &w, 4), 0);
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetLogColorSpaceA)(NULL, &a, 4), 0);
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetLogColorSpaceW)(NULL, NULL, 0), 0);
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
}
TEST(color_icm_getlogcs_stock_default) {
    CI_NOWINE();
    REQUIRE(U(GetLogColorSpaceW)); REQUIRE(U(GetLogColorSpaceA)); REQUIRE(U(GetStockObject));
    HCOLORSPACE s = ci_default_cs();
    if (!s) { t_skip("no stock color space"); return; }
    LOGCOLORSPACEW w; memset(&w, 0, sizeof w);
    BOOL r = U(GetLogColorSpaceW)(s, &w, sizeof w);
    CHECKF(r, "err=%lu", GetLastError());
    if (r) {
        EXPECT_EQ(w.lcsSignature, 0x50534F43);
        EXPECT_EQ(w.lcsVersion, 0x400);
        t_note("default cs: type=0x%lx intent=%lu size=%lu file=%ls", w.lcsCSType, w.lcsIntent, w.lcsSize, w.lcsFilename);
    }
    LOGCOLORSPACEA a; memset(&a, 0, sizeof a);
    r = U(GetLogColorSpaceA)(s, &a, sizeof a);
    CHECKF(r, "err=%lu", GetLastError());
    if (r) EXPECT_EQ(a.lcsSignature, 0x50534F43);
}
TEST(color_icm_getlogcs_a_w_filename_parity) {
    CI_NOWINE();
    REQUIRE(U(GetLogColorSpaceW)); REQUIRE(U(GetLogColorSpaceA));
    HCOLORSPACE s = ci_default_cs();
    if (!s) { t_skip("no stock color space"); return; }
    LOGCOLORSPACEW w; LOGCOLORSPACEA a;
    memset(&w, 0, sizeof w); memset(&a, 0, sizeof a);
    if (!U(GetLogColorSpaceW)(s, &w, sizeof w) || !U(GetLogColorSpaceA)(s, &a, sizeof a)) { t_note("stock cs query failed"); return; }
    char conv[MAX_PATH] = {0};
    WideCharToMultiByte(CP_ACP, 0, w.lcsFilename, -1, conv, MAX_PATH, NULL, NULL);
    EXPECT_STR(a.lcsFilename, conv);
    EXPECT_EQ(a.lcsCSType, w.lcsCSType);
    EXPECT_EQ(a.lcsIntent, w.lcsIntent);
}

/* ---- GetObject / GetObjectType on color spaces ---- */
TEST(color_icm_getobject_w_size) {
    CI_NOWINE();
    REQUIRE(U(GetObjectW)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    HCOLORSPACE h = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    if (!h) { t_skip("no color space"); return; }
    LOGCOLORSPACEW o; memset(&o, 0, sizeof o);
    int r = U(GetObjectW)(h, sizeof o, &o);
    EXPECT_EQ(r, CI_LCSW_SIZE);
    EXPECT_EQ(o.lcsSignature, 0x50534F43);
    U(DeleteColorSpace)(h);
}
TEST(color_icm_getobject_size_variants) {
    CI_NOWINE();
    REQUIRE(U(GetObjectW)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    HCOLORSPACE h = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    if (!h) { t_skip("no color space"); return; }
    BYTE *buf = calloc(1, 2048);
    int q = U(GetObjectW)(h, 0, NULL);
    t_note("GetObjectW(cs,0,NULL)=%d", q);
    static const int sizes[] = { 1, 4, 20, 0x148, 0x24b, 0x24d, 1024 };
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        int r = U(GetObjectW)(h, sizes[i], buf);
        t_note("GetObjectW(cs,%d)=%d", sizes[i], r);
        CHECKF(r == 0 || r == sizes[i] || r == (int)CI_LCSW_SIZE, "size=%d r=%d", sizes[i], r);
    }
    if (U(GetObjectA)) {
        int r = U(GetObjectA)(h, 2048, buf);
        t_note("GetObjectA(cs,2048)=%d", r);
    }
    free(buf);
    U(DeleteColorSpace)(h);
}
TEST(color_icm_getobjecttype_cs) {
    CI_NOWINE();
    REQUIRE(U(GetObjectType)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    HCOLORSPACE h = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    if (!h) { t_skip("no color space"); return; }
    EXPECT_EQ(U(GetObjectType)(h), OBJ_COLORSPACE);
    CHECK(U(DeleteColorSpace)(h));
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetObjectType)(h), 0);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}
TEST(color_icm_getobjecttype_stock_cs) {
    CI_NOWINE();
    REQUIRE(U(GetObjectType)); REQUIRE(U(GetStockObject));
    HGDIOBJ s = U(GetStockObject)(CI_STOCK_COLORSPACE);
    if (!s) { t_note("no stock color space object 20"); return; }
    EXPECT_EQ(U(GetObjectType)(s), OBJ_COLORSPACE);
}
TEST(color_icm_stock_cs_stable) {
    CI_NOWINE();
    REQUIRE(U(GetStockObject));
    HGDIOBJ a = U(GetStockObject)(CI_STOCK_COLORSPACE), b = U(GetStockObject)(CI_STOCK_COLORSPACE);
    EXPECT_EQ(a, b);
}
TEST(color_icm_stock_cs_not_deletable) {
    CI_NOWINE();
    REQUIRE(U(GetStockObject)); REQUIRE(U(DeleteColorSpace)); REQUIRE(U(GetObjectType));
    HGDIOBJ s = U(GetStockObject)(CI_STOCK_COLORSPACE);
    if (!s) { t_skip("no stock color space"); return; }
    BOOL r = U(DeleteColorSpace)((HCOLORSPACE)s);
    t_note("DeleteColorSpace(stock)=%d", r);
    EXPECT_EQ(U(GetObjectType)(s), OBJ_COLORSPACE);
}

/* ---- DeleteColorSpace ---- */
TEST(color_icm_delete_null) {
    CI_NOWINE();
    REQUIRE(U(DeleteColorSpace));
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(DeleteColorSpace)(NULL), 0);
}
TEST(color_icm_delete_bogus_handles) {
    CI_NOWINE();
    REQUIRE(U(DeleteColorSpace));
    static const ULONG_PTR v[] = { 1, 0x12345678, 0xdeadbeef, 0x00090001, 0x00090fff, 0xffffffff, 0x7fff0000 };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++)
        CHECKF(U(DeleteColorSpace)((HCOLORSPACE)v[i]) == 0, "handle=%p", (void *)v[i]);
}
TEST(color_icm_delete_twice) {
    CI_NOWINE();
    REQUIRE(U(DeleteColorSpace)); REQUIRE(U(CreateColorSpaceW));
    HCOLORSPACE h = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    if (!h) { t_skip("no color space"); return; }
    CHECK(U(DeleteColorSpace)(h));
    EXPECT_EQ(U(DeleteColorSpace)(h), 0);
}
TEST(color_icm_delete_via_deleteobject) {
    CI_NOWINE();
    REQUIRE(U(DeleteColorSpace)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectType));
    HCOLORSPACE h = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    if (!h) { t_skip("no color space"); return; }
    BOOL r = U(DeleteObject)(h);
    t_note("DeleteObject(colorspace)=%d", r);
    if (r) EXPECT_EQ(U(GetObjectType)(h), 0);
    else CHECK(U(DeleteColorSpace)(h));
}
TEST(color_icm_delete_wrong_type) {
    CI_NOWINE();
    REQUIRE(U(DeleteColorSpace)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(GetObjectType)); REQUIRE(U(DeleteObject));
    HBRUSH b = U(CreateSolidBrush)(0x00ff00);
    BOOL r = U(DeleteColorSpace)((HCOLORSPACE)b);
    t_note("DeleteColorSpace(brush)=%d", r);
    if (!r) { EXPECT_EQ(U(GetObjectType)(b), OBJ_BRUSH); CHECK(U(DeleteObject)(b)); }
}
TEST(color_icm_delete_selected_in_dc) {
    CI_NOWINE();
    REQUIRE(U(DeleteColorSpace)); REQUIRE(U(SetColorSpace)); REQUIRE(U(GetColorSpace)); REQUIRE(U(CreateColorSpaceW));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    HCOLORSPACE cs = ci_mkw(CI_LCS_sRGB, LCS_GM_IMAGES);
    if (!cs) { ci_mem_close(&m); t_skip("no color space"); return; }
    HCOLORSPACE prev = U(SetColorSpace)(m.dc, cs);
    if (prev) {
        BOOL r = U(DeleteColorSpace)(cs);
        t_note("DeleteColorSpace(selected)=%d", r);
        if (!r) {
            EXPECT_EQ(U(GetColorSpace)(m.dc), cs);
            U(SetColorSpace)(m.dc, prev);
            CHECK(U(DeleteColorSpace)(cs));
        } else {
            HCOLORSPACE now = U(GetColorSpace)(m.dc);
            t_note("after delete GetColorSpace=%p cs=%p prev=%p", now, cs, prev);
        }
    } else {
        t_note("SetColorSpace failed err=%lu", GetLastError());
        U(DeleteColorSpace)(cs);
    }
    ci_mem_close(&m);
}
TEST(color_icm_delete_dc_with_cs_selected) {
    CI_NOWINE();
    REQUIRE(U(DeleteColorSpace)); REQUIRE(U(SetColorSpace)); REQUIRE(U(CreateColorSpaceW));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    HCOLORSPACE cs = ci_mkw(CI_LCS_sRGB, LCS_GM_IMAGES);
    if (!cs) { ci_mem_close(&m); t_skip("no color space"); return; }
    HCOLORSPACE prev = U(SetColorSpace)(m.dc, cs);
    if (prev) U(SetColorSpace)(m.dc, prev);
    ci_mem_close(&m);
    CHECK(U(DeleteColorSpace)(cs));
}
TEST(color_icm_select_object_cs_rejected) {
    CI_NOWINE();
    REQUIRE(U(SelectObject)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace)); REQUIRE(U(GetColorSpace));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    HCOLORSPACE cs = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    if (!cs) { ci_mem_close(&m); t_skip("no color space"); return; }
    HCOLORSPACE before = U(GetColorSpace)(m.dc);
    SetLastError(0xdeadbeef);
    HGDIOBJ r = U(SelectObject)(m.dc, cs);
    t_note("SelectObject(colorspace) = %p err=%lu", r, GetLastError());
    CHECK(r == NULL);
    EXPECT_EQ(U(GetColorSpace)(m.dc), before);
    ci_mem_close(&m);
    U(DeleteColorSpace)(cs);
}

/* ---- GetColorSpace ---- */
TEST(color_icm_getcs_null_dc) {
    CI_NOWINE();
    REQUIRE(U(GetColorSpace));
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetColorSpace)(NULL), NULL);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
}
TEST(color_icm_getcs_bogus_dc) {
    CI_NOWINE();
    REQUIRE(U(GetColorSpace));
    static const ULONG_PTR v[] = { 1, 0xdeadbeef, 0x00010001, 0x00010fff, 0xffffffff };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        SetLastError(0xdeadbeef);
        CHECKF(U(GetColorSpace)((HDC)v[i]) == NULL, "dc=%p", (void *)v[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "dc=%p err=%lu", (void *)v[i], GetLastError());
    }
}
TEST(color_icm_getcs_deleted_dc) {
    CI_NOWINE();
    REQUIRE(U(GetColorSpace)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC d = ci_bogus_dc();
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetColorSpace)(d), NULL);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
}
TEST(color_icm_getcs_wrong_object_type) {
    CI_NOWINE();
    REQUIRE(U(GetColorSpace)); REQUIRE(U(CreatePen)); REQUIRE(U(DeleteObject));
    HGDIOBJ pen = U(CreatePen)(PS_SOLID, 1, 0);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GetColorSpace)((HDC)pen), NULL);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    U(DeleteObject)(pen);
}
TEST(color_icm_getcs_fresh_memdc) {
    CI_NOWINE();
    REQUIRE(U(GetColorSpace)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetObjectType));
    HDC d = U(CreateCompatibleDC)(NULL);
    HCOLORSPACE cs = U(GetColorSpace)(d);
    CHECK(cs != NULL);
    if (cs) {
        EXPECT_EQ(U(GetObjectType)(cs), OBJ_COLORSPACE);
        HCOLORSPACE st = ci_default_cs();
        if (st) EXPECT_EQ(cs, st);
    }
    U(DeleteDC)(d);
}
TEST(color_icm_getcs_two_dcs_same_default) {
    CI_NOWINE();
    REQUIRE(U(GetColorSpace)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC a = U(CreateCompatibleDC)(NULL), b = U(CreateCompatibleDC)(NULL);
    EXPECT_EQ(U(GetColorSpace)(a), U(GetColorSpace)(b));
    U(DeleteDC)(a); U(DeleteDC)(b);
}
TEST(color_icm_getcs_screen_dc) {
    CI_NOWINE();
    REQUIRE(U(GetColorSpace)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    if (!d) { t_skip("no screen dc"); return; }
    HCOLORSPACE cs = U(GetColorSpace)(d);
    CHECK(cs != NULL);
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_getcs_matches_getcurrentobject) {
    CI_NOWINE();
    REQUIRE(U(GetColorSpace)); REQUIRE(U(GetCurrentObject));
    ci_mem m; if (!ci_mem_open(&m, 2, 2)) { t_skip("no memdc"); return; }
    EXPECT_EQ(U(GetCurrentObject)(m.dc, OBJ_COLORSPACE), U(GetColorSpace)(m.dc));
    ci_mem_close(&m);
}
TEST(color_icm_getcs_dib_memdc) {
    CI_NOWINE();
    REQUIRE(U(GetColorSpace));
    ci_mem m; if (!ci_mem_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    CHECK(U(GetColorSpace)(m.dc) != NULL);
    ci_mem_close(&m);
}

/* ---- SetColorSpace ---- */
TEST(color_icm_setcs_null_dc) {
    CI_NOWINE();
    REQUIRE(U(SetColorSpace)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    HCOLORSPACE cs = ci_mkw(CI_LCS_sRGB, LCS_GM_IMAGES);
    if (!cs) { t_skip("no color space"); return; }
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(SetColorSpace)(NULL, cs), NULL);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    U(DeleteColorSpace)(cs);
}
TEST(color_icm_setcs_deleted_dc) {
    CI_NOWINE();
    REQUIRE(U(SetColorSpace)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    HCOLORSPACE cs = ci_mkw(CI_LCS_sRGB, LCS_GM_IMAGES);
    if (!cs) { t_skip("no color space"); return; }
    HDC d = ci_bogus_dc();
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(SetColorSpace)(d, cs), NULL);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    U(DeleteColorSpace)(cs);
}
TEST(color_icm_setcs_roundtrip_srgb) {
    CI_NOWINE();
    REQUIRE(U(SetColorSpace)); REQUIRE(U(GetColorSpace)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    HCOLORSPACE cs = ci_mkw(CI_LCS_sRGB, LCS_GM_IMAGES);
    if (!cs) { ci_mem_close(&m); t_skip("no color space"); return; }
    HCOLORSPACE def = U(GetColorSpace)(m.dc);
    SetLastError(0xdeadbeef);
    HCOLORSPACE prev = U(SetColorSpace)(m.dc, cs);
    if (!prev) {
        t_note("SetColorSpace(sRGB) failed err=%lu", GetLastError());
    } else {
        EXPECT_EQ(prev, def);
        EXPECT_EQ(U(GetColorSpace)(m.dc), cs);
        EXPECT_EQ(U(GetCurrentObject)(m.dc, OBJ_COLORSPACE), cs);
        /* setting the same space returns it */
        EXPECT_EQ(U(SetColorSpace)(m.dc, cs), cs);
        EXPECT_EQ(U(SetColorSpace)(m.dc, def), cs);
        EXPECT_EQ(U(GetColorSpace)(m.dc), def);
    }
    ci_mem_close(&m);
    CHECK(U(DeleteColorSpace)(cs));
}
TEST(color_icm_setcs_roundtrip_calibrated) {
    CI_NOWINE();
    REQUIRE(U(SetColorSpace)); REQUIRE(U(GetColorSpace)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    HCOLORSPACE cs = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    if (!cs) { ci_mem_close(&m); t_skip("no color space"); return; }
    HCOLORSPACE def = U(GetColorSpace)(m.dc);
    HCOLORSPACE prev = U(SetColorSpace)(m.dc, cs);
    if (!prev) t_note("SetColorSpace(calibrated) failed err=%lu", GetLastError());
    else { EXPECT_EQ(U(GetColorSpace)(m.dc), cs); U(SetColorSpace)(m.dc, def); }
    ci_mem_close(&m);
    CHECK(U(DeleteColorSpace)(cs));
}
TEST(color_icm_setcs_bogus_handles) {
    CI_NOWINE();
    REQUIRE(U(SetColorSpace)); REQUIRE(U(GetColorSpace)); REQUIRE(U(CreatePen)); REQUIRE(U(DeleteObject));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    HCOLORSPACE def = U(GetColorSpace)(m.dc);
    HGDIOBJ pen = U(CreatePen)(PS_SOLID, 1, 0);
    HGDIOBJ bad[] = { pen, (HGDIOBJ)(ULONG_PTR)0xdeadbeef, (HGDIOBJ)(ULONG_PTR)0x00090123, U(GetStockObject)(WHITE_BRUSH) };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0xdeadbeef);
        HCOLORSPACE r = U(SetColorSpace)(m.dc, (HCOLORSPACE)bad[i]);
        CHECKF(r == NULL, "i=%u r=%p", i, r);
        EXPECT_EQ(U(GetColorSpace)(m.dc), def);
    }
    U(DeleteObject)(pen);
    ci_mem_close(&m);
}
TEST(color_icm_setcs_null_cs) {
    CI_NOWINE();
    REQUIRE(U(SetColorSpace)); REQUIRE(U(GetColorSpace));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    HCOLORSPACE def = U(GetColorSpace)(m.dc);
    SetLastError(0xdeadbeef);
    HCOLORSPACE r = U(SetColorSpace)(m.dc, NULL);
    t_note("SetColorSpace(dc,NULL)=%p err=%lu", r, GetLastError());
    HCOLORSPACE now = U(GetColorSpace)(m.dc);
    if (r) U(SetColorSpace)(m.dc, def);
    else EXPECT_EQ(now, def);
    ci_mem_close(&m);
}
TEST(color_icm_setcs_deleted_cs) {
    CI_NOWINE();
    REQUIRE(U(SetColorSpace)); REQUIRE(U(GetColorSpace)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    ci_mem m; if (!ci_mem_open(&m, 4, 4)) { t_skip("no memdc"); return; }
    HCOLORSPACE cs = ci_mkw(CI_LCS_sRGB, LCS_GM_IMAGES);
    if (!cs) { ci_mem_close(&m); t_skip("no color space"); return; }
    HCOLORSPACE def = U(GetColorSpace)(m.dc);
    CHECK(U(DeleteColorSpace)(cs));
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(SetColorSpace)(m.dc, cs), NULL);
    EXPECT_EQ(U(GetColorSpace)(m.dc), def);
    ci_mem_close(&m);
}
TEST(color_icm_setcs_independent_per_dc) {
    CI_NOWINE();
    REQUIRE(U(SetColorSpace)); REQUIRE(U(GetColorSpace)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    ci_mem a, b;
    if (!ci_mem_open(&a, 2, 2)) { t_skip("no memdc"); return; }
    if (!ci_mem_open(&b, 2, 2)) { ci_mem_close(&a); t_skip("no memdc"); return; }
    HCOLORSPACE cs = ci_mkw(CI_LCS_sRGB, LCS_GM_IMAGES);
    HCOLORSPACE def = U(GetColorSpace)(a.dc);
    if (cs && U(SetColorSpace)(a.dc, cs)) {
        EXPECT_EQ(U(GetColorSpace)(a.dc), cs);
        EXPECT_EQ(U(GetColorSpace)(b.dc), def);
        U(SetColorSpace)(a.dc, def);
    }
    ci_mem_close(&a); ci_mem_close(&b);
    if (cs) U(DeleteColorSpace)(cs);
}
TEST(color_icm_setcs_savedc_restore) {
    CI_NOWINE();
    REQUIRE(U(SetColorSpace)); REQUIRE(U(GetColorSpace)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    ci_mem m; if (!ci_mem_open(&m, 2, 2)) { t_skip("no memdc"); return; }
    HCOLORSPACE cs = ci_mkw(CI_LCS_sRGB, LCS_GM_IMAGES);
    HCOLORSPACE def = U(GetColorSpace)(m.dc);
    if (cs) {
        int id = U(SaveDC)(m.dc);
        CHECK(id > 0);
        if (U(SetColorSpace)(m.dc, cs)) {
            CHECK(U(RestoreDC)(m.dc, id));
            HCOLORSPACE now = U(GetColorSpace)(m.dc);
            t_note("after RestoreDC color space %s", now == def ? "restored to default" : "unchanged (not part of saved state)");
            if (now != def) U(SetColorSpace)(m.dc, def);
        } else {
            U(RestoreDC)(m.dc, id);
        }
    }
    ci_mem_close(&m);
    if (cs) U(DeleteColorSpace)(cs);
}
TEST(color_icm_setcs_same_cs_two_dcs) {
    CI_NOWINE();
    REQUIRE(U(SetColorSpace)); REQUIRE(U(GetColorSpace)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    ci_mem a, b;
    if (!ci_mem_open(&a, 2, 2)) { t_skip("no memdc"); return; }
    if (!ci_mem_open(&b, 2, 2)) { ci_mem_close(&a); t_skip("no memdc"); return; }
    HCOLORSPACE cs = ci_mkw(CI_LCS_sRGB, LCS_GM_IMAGES);
    HCOLORSPACE da = U(GetColorSpace)(a.dc), db = U(GetColorSpace)(b.dc);
    if (cs && U(SetColorSpace)(a.dc, cs) && U(SetColorSpace)(b.dc, cs)) {
        EXPECT_EQ(U(GetColorSpace)(a.dc), cs);
        EXPECT_EQ(U(GetColorSpace)(b.dc), cs);
        U(SetColorSpace)(a.dc, da); U(SetColorSpace)(b.dc, db);
    }
    ci_mem_close(&a); ci_mem_close(&b);
    if (cs) U(DeleteColorSpace)(cs);
}
TEST(color_icm_setcs_screen_dc_restore) {
    CI_NOWINE();
    REQUIRE(U(SetColorSpace)); REQUIRE(U(GetColorSpace)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    HDC d = U(GetDC)(NULL);
    if (!d) { t_skip("no screen dc"); return; }
    HCOLORSPACE cs = ci_mkw(CI_LCS_sRGB, LCS_GM_IMAGES);
    HCOLORSPACE def = U(GetColorSpace)(d);
    if (cs && U(SetColorSpace)(d, cs)) {
        EXPECT_EQ(U(GetColorSpace)(d), cs);
        U(SetColorSpace)(d, def);
        EXPECT_EQ(U(GetColorSpace)(d), def);
    }
    U(ReleaseDC)(NULL, d);
    if (cs) U(DeleteColorSpace)(cs);
}
TEST(color_icm_setcs_gdi_count_balanced) {
    CI_NOWINE();
    REQUIRE(U(SetColorSpace)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    int before = ci_gdi_count();
    if (before < 0) { t_skip("GetGuiResources unusable (wine)"); return; }
    ci_mem m; if (!ci_mem_open(&m, 2, 2)) { t_skip("no memdc"); return; }
    HCOLORSPACE def = U(GetColorSpace)(m.dc);
    for (int i = 0; i < 20; i++) {
        HCOLORSPACE cs = ci_mkw(CI_LCS_sRGB, LCS_GM_IMAGES);
        if (cs && U(SetColorSpace)(m.dc, cs)) U(SetColorSpace)(m.dc, def);
        if (cs) U(DeleteColorSpace)(cs);
    }
    ci_mem_close(&m);
    EXPECT_EQ(ci_gdi_count(), before);
}

/* ---- cross-thread ---- */
static DWORD WINAPI ci_thr_query(LPVOID p) {
    HCOLORSPACE h = (HCOLORSPACE)p;
    LOGCOLORSPACEW o; memset(&o, 0, sizeof o);
    BOOL ok = U(GetLogColorSpaceW)(h, &o, sizeof o);
    return ok && o.lcsSignature == 0x50534F43 && U(GetObjectType)(h) == OBJ_COLORSPACE;
}
TEST(color_icm_thread_query_cs) {
    CI_NOWINE();
    REQUIRE(U(GetLogColorSpaceW)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace)); REQUIRE(U(GetObjectType));
    HCOLORSPACE h = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    if (!h) { t_skip("no color space"); return; }
    HANDLE t = CreateThread(NULL, 0, ci_thr_query, h, 0, NULL);
    WaitForSingleObject(t, 10000);
    DWORD rc = 0; GetExitCodeThread(t, &rc); CloseHandle(t);
    EXPECT_EQ(rc, 1);
    U(DeleteColorSpace)(h);
}
static DWORD WINAPI ci_thr_create(LPVOID p) {
    *(HCOLORSPACE *)p = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_BUSINESS);
    return 0;
}
TEST(color_icm_thread_created_cs_survives_thread) {
    CI_NOWINE();
    REQUIRE(U(GetLogColorSpaceW)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    HCOLORSPACE h = NULL;
    HANDLE t = CreateThread(NULL, 0, ci_thr_create, &h, 0, NULL);
    WaitForSingleObject(t, 10000); CloseHandle(t);
    CHECK(h != NULL);
    if (!h) return;
    LOGCOLORSPACEW o;
    CHECK(U(GetLogColorSpaceW)(h, &o, sizeof o));
    EXPECT_EQ(o.lcsIntent, LCS_GM_BUSINESS);
    CHECK(U(DeleteColorSpace)(h));
}
static DWORD WINAPI ci_thr_delete(LPVOID p) { return U(DeleteColorSpace)((HCOLORSPACE)p); }
TEST(color_icm_thread_delete_from_other_thread) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace)); REQUIRE(U(GetObjectType));
    HCOLORSPACE h = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    if (!h) { t_skip("no color space"); return; }
    HANDLE t = CreateThread(NULL, 0, ci_thr_delete, h, 0, NULL);
    WaitForSingleObject(t, 10000);
    DWORD rc = 0; GetExitCodeThread(t, &rc); CloseHandle(t);
    EXPECT_EQ(rc, 1);
    EXPECT_EQ(U(GetObjectType)(h), 0);
}
static DWORD WINAPI ci_thr_getcs(LPVOID p) { return (DWORD)(ULONG_PTR)U(GetColorSpace)((HDC)p); }
TEST(color_icm_thread_getcs_on_foreign_dc) {
    CI_NOWINE();
    REQUIRE(U(GetColorSpace));
    ci_mem m; if (!ci_mem_open(&m, 2, 2)) { t_skip("no memdc"); return; }
    HANDLE t = CreateThread(NULL, 0, ci_thr_getcs, m.dc, 0, NULL);
    WaitForSingleObject(t, 10000);
    DWORD rc = 0; GetExitCodeThread(t, &rc); CloseHandle(t);
    EXPECT_EQ(rc, (DWORD)(ULONG_PTR)U(GetColorSpace)(m.dc));
    ci_mem_close(&m);
}
