#include "cih.h"

/* ---- additional edge cases ---- */
TEST(color_icm_extra_getprofile_w_zero_size_nonnull_buf) {
    CI_NOWINE();
    REQUIRE(U(GetICMProfileW)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC d = U(CreateCompatibleDC)(NULL);
    REQUIRE(d);
    DWORD n = 0;
    WCHAR buf[8] = { L'x' };
    SetLastError(0xdeadbeef);
    BOOL r = U(GetICMProfileW)(d, &n, buf);
    CHECK(!r);
    /* required size reported back, buffer untouched */
    CHECKF(n > 1, "needed=%lu", n);
    CHECK(buf[0] == L'x');
    U(DeleteDC)(d);
}
TEST(color_icm_extra_getprofile_a_zero_size_nonnull_buf) {
    CI_NOWINE();
    REQUIRE(U(GetICMProfileA)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC d = U(CreateCompatibleDC)(NULL);
    REQUIRE(d);
    DWORD n = 0;
    char buf[8] = { 'x' };
    BOOL r = U(GetICMProfileA)(d, &n, buf);
    CHECK(!r);
    CHECKF(n > 1, "needed=%lu", n);
    CHECK(buf[0] == 'x');
    U(DeleteDC)(d);
}
TEST(color_icm_extra_setprofile_overlong_path) {
    CI_NOWINE();
    REQUIRE(U(SetICMProfileW)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC d = U(CreateCompatibleDC)(NULL);
    REQUIRE(d);
    static WCHAR p[1200];
    for (int i = 0; i < 1199; i++) p[i] = L'a';
    p[1199] = 0;
    SetLastError(0xdeadbeef);
    BOOL r = U(SetICMProfileW)(d, p);
    if (r) t_note("overlong profile path accepted");
    U(DeleteDC)(d);
}
TEST(color_icm_extra_seticmmode_repeat_off_idempotent) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC d = U(CreateCompatibleDC)(NULL);
    REQUIRE(d);
    for (int i = 0; i < 5; i++) {
        int q = U(SetICMMode)(d, ICM_QUERY);
        CHECKF(q == ICM_OFF || q == 0, "q=%d", q);
    }
    U(DeleteDC)(d);
}
TEST(color_icm_extra_enum_null_proc_bad_dc) {
    CI_NOWINE();
    REQUIRE(U(EnumICMProfilesW));
    HDC d = ci_bogus_dc();
    SetLastError(0xdeadbeef);
    int r = U(EnumICMProfilesW)(d, NULL, 0);
    CHECK(r <= 0);
    r = U(EnumICMProfilesW)(NULL, NULL, 0);
    CHECK(r <= 0);
}
TEST(color_icm_extra_colorspace_delete_leaves_no_leak_after_getlog) {
    CI_NOWINE();
    REQUIRE(U(GetLogColorSpaceW)); REQUIRE(U(DeleteColorSpace)); REQUIRE(U(CreateColorSpaceW));
    int before = ci_gdi_count();
    for (int i = 0; i < 200; i++) {
        HCOLORSPACE h = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
        if (!h) { t_skip("no color space"); return; }
        LOGCOLORSPACEW w;
        CHECK(U(GetLogColorSpaceW)(h, &w, sizeof w));
        CHECK(U(DeleteColorSpace)(h));
    }
    int after = ci_gdi_count();
    if (before >= 0) CHECKF(after == before, "before=%d after=%d", before, after);
}
TEST(color_icm_extra_getlogcs_result_not_touched_on_error) {
    CI_NOWINE();
    REQUIRE(U(GetLogColorSpaceW));
    LOGCOLORSPACEW w;
    memset(&w, 0x5a, sizeof w);
    HCOLORSPACE bad = (HCOLORSPACE)(ULONG_PTR)0xdeadbeef;
    SetLastError(0xdeadbeef);
    BOOL r = U(GetLogColorSpaceW)(bad, &w, sizeof w);
    CHECK(!r);
    CHECK(((BYTE *)&w)[0] == 0x5a);
    CHECK(((BYTE *)&w)[sizeof w - 1] == 0x5a);
}
TEST(color_icm_extra_getlogcs_stock_object_roundtrip) {
    CI_NOWINE();
    REQUIRE(U(GetLogColorSpaceW)); REQUIRE(U(GetStockObject));
    HCOLORSPACE h = ci_default_cs();
    REQUIRE(h);
    LOGCOLORSPACEW w;
    memset(&w, 0, sizeof w);
    BOOL r = U(GetLogColorSpaceW)(h, &w, sizeof w);
    CHECK(r);
    CHECK(w.lcsSignature == 0x50534F43);
    CHECKF(w.lcsSize == CI_LCSW_SIZE, "size=%lu", w.lcsSize);
}
TEST(color_icm_extra_xform_identity_set_ok) {
    CI_NOWINE_ABORT();
    REQUIRE(U(SetWorldTransform)); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC d = U(CreateCompatibleDC)(NULL);
    REQUIRE(d);
    int gm = U(SetGraphicsMode)(d, GM_ADVANCED);
    if (gm == 0) { t_skip("no advanced mode"); U(DeleteDC)(d); return; }
    XFORM x = { 1, 0, 0, 1, 0, 0 };
    BOOL r = U(SetWorldTransform)(d, &x);
    CHECK(r);
    t_note("identity SetWorldTransform ok");
    U(DeleteDC)(d);
}
