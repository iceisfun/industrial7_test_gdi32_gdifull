#include "cih.h"

/* ---- argument validation ---- */
TEST(color_icm_create_w_null) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW));
    SetLastError(0xdeadbeef);
    HCOLORSPACE h = U(CreateColorSpaceW)(NULL);
    EXPECT_EQ(h, NULL);
    EXPECT_ERR(h, ERROR_INVALID_PARAMETER);
}
TEST(color_icm_create_a_null) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceA));
    SetLastError(0xdeadbeef);
    HCOLORSPACE h = U(CreateColorSpaceA)(NULL);
    EXPECT_EQ(h, NULL);
    EXPECT_ERR(h, ERROR_INVALID_PARAMETER);
}
TEST(color_icm_create_w_valid_calibrated) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    HCOLORSPACE h = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    CHECKF(h != NULL, "err=%lu", GetLastError());
    if (h) CHECK(U(DeleteColorSpace)(h));
}
TEST(color_icm_create_a_valid_calibrated) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceA)); REQUIRE(U(DeleteColorSpace));
    HCOLORSPACE h = ci_mka(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    CHECKF(h != NULL, "err=%lu", GetLastError());
    if (h) CHECK(U(DeleteColorSpace)(h));
}
TEST(color_icm_create_w_handle_type_bits) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace)); REQUIRE(U(GetObjectType));
    HCOLORSPACE h = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_GRAPHICS);
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(((ULONG_PTR)h >> 16) & 0x7f, 9);
    EXPECT_EQ(U(GetObjectType)(h), OBJ_COLORSPACE);
    CHECK(U(DeleteColorSpace)(h));
}
TEST(color_icm_create_w_srgb) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    HCOLORSPACE h = ci_mkw(CI_LCS_sRGB, LCS_GM_IMAGES);
    CHECKF(h != NULL, "err=%lu", GetLastError());
    if (h) CHECK(U(DeleteColorSpace)(h));
}
TEST(color_icm_create_a_srgb) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceA)); REQUIRE(U(DeleteColorSpace));
    HCOLORSPACE h = ci_mka(CI_LCS_sRGB, LCS_GM_IMAGES);
    CHECKF(h != NULL, "err=%lu", GetLastError());
    if (h) CHECK(U(DeleteColorSpace)(h));
}
TEST(color_icm_create_w_windows_cs) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    HCOLORSPACE h = ci_mkw(CI_LCS_WINDOWS, LCS_GM_BUSINESS);
    CHECKF(h != NULL, "err=%lu", GetLastError());
    if (h) CHECK(U(DeleteColorSpace)(h));
}
TEST(color_icm_create_a_windows_cs) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceA)); REQUIRE(U(DeleteColorSpace));
    HCOLORSPACE h = ci_mka(CI_LCS_WINDOWS, LCS_GM_BUSINESS);
    CHECKF(h != NULL, "err=%lu", GetLastError());
    if (h) CHECK(U(DeleteColorSpace)(h));
}
TEST(color_icm_create_w_link_empty_name) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    /* LCS 'LINK' with an empty filename takes the same path as a calibrated space. */
    HCOLORSPACE h = ci_mkw(CI_LCS_LINK, LCS_GM_IMAGES);
    if (!h) t_note("LINK/empty name rejected, err=%lu", GetLastError());
    if (h) CHECK(U(DeleteColorSpace)(h));
}

/* ---- intent table ---- */
static const struct { DWORD intent; int ok; } ci_intents[] = {
    {1, 1}, {2, 1}, {4, 1}, {8, 1},
    {0, 0}, {3, 0}, {5, 0}, {6, 0}, {7, 0}, {9, 0}, {10, 0}, {16, 0}, {0x100, 0}, {0x80000000u, 0}, {0xffffffffu, 0},
};
TEST(color_icm_create_w_intent_table) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    for (unsigned i = 0; i < sizeof ci_intents / sizeof *ci_intents; i++) {
        SetLastError(0xdeadbeef);
        HCOLORSPACE h = ci_mkw(CI_LCS_CALIBRATED_RGB, ci_intents[i].intent);
        CHECKF((h != NULL) == ci_intents[i].ok, "intent=0x%lx h=%p", ci_intents[i].intent, h);
        if (!h && !ci_intents[i].ok) CHECKF(GetLastError() == CI_ERR_INVALID_COLORSPACE, "intent=0x%lx err=%lu", ci_intents[i].intent, GetLastError());
        if (h) CHECK(U(DeleteColorSpace)(h));
    }
}
TEST(color_icm_create_a_intent_table) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceA)); REQUIRE(U(DeleteColorSpace));
    /* The A entry point validates only sig/version/size itself; intent is checked by the shared W path. */
    for (unsigned i = 0; i < sizeof ci_intents / sizeof *ci_intents; i++) {
        SetLastError(0xdeadbeef);
        HCOLORSPACE h = ci_mka(CI_LCS_CALIBRATED_RGB, ci_intents[i].intent);
        CHECKF((h != NULL) == ci_intents[i].ok, "intent=0x%lx h=%p", ci_intents[i].intent, h);
        if (!h && !ci_intents[i].ok) CHECKF(GetLastError() == CI_ERR_INVALID_COLORSPACE, "intent=0x%lx err=%lu", ci_intents[i].intent, GetLastError());
        if (h) CHECK(U(DeleteColorSpace)(h));
    }
}

/* ---- signature / version / size tables ---- */
static const DWORD ci_bad_sigs[] = { 0, 1, 0x50534F42, 0x50534F44, 0x434F5350 /* byte-swapped */, 0xffffffffu, 0x80000000u };
static const DWORD ci_bad_vers[] = { 0, 1, 0x300, 0x3ff, 0x401, 0x4000, 0x40000, 0xffffffffu };
static const DWORD ci_bad_sizes[] = { 0, 1, 4, 0x147, 0x149, 0x24b, 0x24d, 0x400, 0xffffffffu, 0x80000000u };

TEST(color_icm_create_w_bad_signature) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW));
    for (unsigned i = 0; i < sizeof ci_bad_sigs / sizeof *ci_bad_sigs; i++) {
        LOGCOLORSPACEW w; ci_initw(&w, CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
        w.lcsSignature = ci_bad_sigs[i];
        SetLastError(0xdeadbeef);
        HCOLORSPACE h = U(CreateColorSpaceW)(&w);
        CHECKF(h == NULL, "sig=0x%lx", ci_bad_sigs[i]);
        CHECKF(GetLastError() == CI_ERR_INVALID_COLORSPACE, "sig=0x%lx err=%lu", ci_bad_sigs[i], GetLastError());
        if (h) U(DeleteColorSpace)(h);
    }
}
TEST(color_icm_create_a_bad_signature) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceA));
    for (unsigned i = 0; i < sizeof ci_bad_sigs / sizeof *ci_bad_sigs; i++) {
        LOGCOLORSPACEA a; ci_inita(&a, CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
        a.lcsSignature = ci_bad_sigs[i];
        SetLastError(0xdeadbeef);
        HCOLORSPACE h = U(CreateColorSpaceA)(&a);
        CHECKF(h == NULL, "sig=0x%lx", ci_bad_sigs[i]);
        CHECKF(GetLastError() == CI_ERR_INVALID_COLORSPACE, "sig=0x%lx err=%lu", ci_bad_sigs[i], GetLastError());
        if (h) U(DeleteColorSpace)(h);
    }
}
TEST(color_icm_create_w_bad_version) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW));
    for (unsigned i = 0; i < sizeof ci_bad_vers / sizeof *ci_bad_vers; i++) {
        LOGCOLORSPACEW w; ci_initw(&w, CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
        w.lcsVersion = ci_bad_vers[i];
        SetLastError(0xdeadbeef);
        HCOLORSPACE h = U(CreateColorSpaceW)(&w);
        CHECKF(h == NULL, "ver=0x%lx", ci_bad_vers[i]);
        CHECKF(GetLastError() == CI_ERR_INVALID_COLORSPACE, "ver=0x%lx err=%lu", ci_bad_vers[i], GetLastError());
        if (h) U(DeleteColorSpace)(h);
    }
}
TEST(color_icm_create_a_bad_version) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceA));
    for (unsigned i = 0; i < sizeof ci_bad_vers / sizeof *ci_bad_vers; i++) {
        LOGCOLORSPACEA a; ci_inita(&a, CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
        a.lcsVersion = ci_bad_vers[i];
        SetLastError(0xdeadbeef);
        HCOLORSPACE h = U(CreateColorSpaceA)(&a);
        CHECKF(h == NULL, "ver=0x%lx", ci_bad_vers[i]);
        CHECKF(GetLastError() == CI_ERR_INVALID_COLORSPACE, "ver=0x%lx err=%lu", ci_bad_vers[i], GetLastError());
        if (h) U(DeleteColorSpace)(h);
    }
}
TEST(color_icm_create_w_bad_size) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW));
    for (unsigned i = 0; i < sizeof ci_bad_sizes / sizeof *ci_bad_sizes; i++) {
        LOGCOLORSPACEW w; ci_initw(&w, CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
        w.lcsSize = ci_bad_sizes[i];
        SetLastError(0xdeadbeef);
        HCOLORSPACE h = U(CreateColorSpaceW)(&w);
        CHECKF(h == NULL, "size=0x%lx", ci_bad_sizes[i]);
        CHECKF(GetLastError() == CI_ERR_INVALID_COLORSPACE, "size=0x%lx err=%lu", ci_bad_sizes[i], GetLastError());
        if (h) U(DeleteColorSpace)(h);
    }
}
TEST(color_icm_create_a_bad_size) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceA));
    for (unsigned i = 0; i < sizeof ci_bad_sizes / sizeof *ci_bad_sizes; i++) {
        LOGCOLORSPACEA a; ci_inita(&a, CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
        a.lcsSize = ci_bad_sizes[i];
        SetLastError(0xdeadbeef);
        HCOLORSPACE h = U(CreateColorSpaceA)(&a);
        CHECKF(h == NULL, "size=0x%lx", ci_bad_sizes[i]);
        CHECKF(GetLastError() == CI_ERR_INVALID_COLORSPACE, "size=0x%lx err=%lu", ci_bad_sizes[i], GetLastError());
        if (h) U(DeleteColorSpace)(h);
    }
}
TEST(color_icm_create_a_with_w_size) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceA));
    LOGCOLORSPACEA a; ci_inita(&a, CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    a.lcsSize = CI_LCSW_SIZE;
    SetLastError(0xdeadbeef);
    HCOLORSPACE h = U(CreateColorSpaceA)(&a);
    EXPECT_EQ(h, NULL);
    EXPECT_ERR(h, CI_ERR_INVALID_COLORSPACE);
}
TEST(color_icm_create_w_with_a_size) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW));
    LOGCOLORSPACEW w; ci_initw(&w, CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    w.lcsSize = CI_LCSA_SIZE;
    SetLastError(0xdeadbeef);
    HCOLORSPACE h = U(CreateColorSpaceW)(&w);
    EXPECT_EQ(h, NULL);
    EXPECT_ERR(h, CI_ERR_INVALID_COLORSPACE);
}
TEST(color_icm_create_w_all_zero_struct) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW));
    LOGCOLORSPACEW w; memset(&w, 0, sizeof w);
    SetLastError(0xdeadbeef);
    HCOLORSPACE h = U(CreateColorSpaceW)(&w);
    EXPECT_EQ(h, NULL);
    EXPECT_ERR(h, CI_ERR_INVALID_COLORSPACE);
}
TEST(color_icm_create_a_all_zero_struct) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceA));
    LOGCOLORSPACEA a; memset(&a, 0, sizeof a);
    SetLastError(0xdeadbeef);
    HCOLORSPACE h = U(CreateColorSpaceA)(&a);
    EXPECT_EQ(h, NULL);
    EXPECT_ERR(h, CI_ERR_INVALID_COLORSPACE);
}

/* ---- colorspace type / filename ---- */
static const DWORD ci_bad_types[] = { 1, 2, 0x12345678, 0x73524743 /* 'sRGC' */, 0x57696E21, 0xffffffffu };
TEST(color_icm_create_w_bad_cstype) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW));
    for (unsigned i = 0; i < sizeof ci_bad_types / sizeof *ci_bad_types; i++) {
        SetLastError(0xdeadbeef);
        HCOLORSPACE h = ci_mkw(ci_bad_types[i], LCS_GM_IMAGES);
        CHECKF(h == NULL, "type=0x%lx", ci_bad_types[i]);
        CHECKF(GetLastError() == CI_ERR_INVALID_COLORSPACE, "type=0x%lx err=%lu", ci_bad_types[i], GetLastError());
        if (h) U(DeleteColorSpace)(h);
    }
}
TEST(color_icm_create_a_bad_cstype) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceA));
    for (unsigned i = 0; i < sizeof ci_bad_types / sizeof *ci_bad_types; i++) {
        SetLastError(0xdeadbeef);
        HCOLORSPACE h = ci_mka(ci_bad_types[i], LCS_GM_IMAGES);
        CHECKF(h == NULL, "type=0x%lx", ci_bad_types[i]);
        CHECKF(GetLastError() == CI_ERR_INVALID_COLORSPACE, "type=0x%lx err=%lu", ci_bad_types[i], GetLastError());
        if (h) U(DeleteColorSpace)(h);
    }
}
TEST(color_icm_create_w_missing_profile) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW));
    LOGCOLORSPACEW w; ci_initw(&w, CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    lstrcpyW(w.lcsFilename, L"no_such_profile_t32.icm");
    SetLastError(0xdeadbeef);
    HCOLORSPACE h = U(CreateColorSpaceW)(&w);
    EXPECT_EQ(h, NULL);
    DWORD e = GetLastError();
    CHECKF(e == CI_ERR_PROFILE_NOT_FOUND || e == ERROR_INVALID_PARAMETER, "err=%lu", e);
    if (e != CI_ERR_PROFILE_NOT_FOUND) t_note("missing profile err=%lu", e);
    if (h) U(DeleteColorSpace)(h);
}
TEST(color_icm_create_a_missing_profile) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceA));
    LOGCOLORSPACEA a; ci_inita(&a, CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    lstrcpyA(a.lcsFilename, "no_such_profile_t32.icm");
    SetLastError(0xdeadbeef);
    HCOLORSPACE h = U(CreateColorSpaceA)(&a);
    EXPECT_EQ(h, NULL);
    DWORD e = GetLastError();
    CHECKF(e == CI_ERR_PROFILE_NOT_FOUND || e == ERROR_INVALID_PARAMETER, "err=%lu", e);
    if (h) U(DeleteColorSpace)(h);
}
TEST(color_icm_create_w_missing_profile_abs) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW));
    LOGCOLORSPACEW w; ci_initw(&w, CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    lstrcpyW(w.lcsFilename, L"C:\\no_such_dir_t32\\no_such_profile_t32.icm");
    SetLastError(0xdeadbeef);
    HCOLORSPACE h = U(CreateColorSpaceW)(&w);
    EXPECT_EQ(h, NULL);
    if (h) U(DeleteColorSpace)(h);
}
TEST(color_icm_create_w_srgb_ignores_filename) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    LOGCOLORSPACEW w; ci_initw(&w, CI_LCS_sRGB, LCS_GM_IMAGES);
    lstrcpyW(w.lcsFilename, L"no_such_profile_t32.icm");
    HCOLORSPACE h = U(CreateColorSpaceW)(&w);
    CHECKF(h != NULL, "err=%lu", GetLastError());
    if (h) CHECK(U(DeleteColorSpace)(h));
}
TEST(color_icm_create_w_system_srgb_profile_file) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    LOGCOLORSPACEW w; ci_initw(&w, CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    lstrcpyW(w.lcsFilename, L"sRGB Color Space Profile.icm");
    HCOLORSPACE h = U(CreateColorSpaceW)(&w);
    if (!h) t_note("profile by short name not accepted, err=%lu", GetLastError());
    if (h) CHECK(U(DeleteColorSpace)(h));
}

/* ---- lifetime ---- */
TEST(color_icm_create_distinct_handles) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    HCOLORSPACE a = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    HCOLORSPACE b = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    CHECK(a && b);
    CHECK(a != b);
    if (a) CHECK(U(DeleteColorSpace)(a));
    if (b) CHECK(U(DeleteColorSpace)(b));
}
TEST(color_icm_create_gdi_count_balanced) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    int before = ci_gdi_count();
    if (before < 0) { t_skip("GetGuiResources unusable (wine)"); return; }
    HCOLORSPACE h[16];
    for (int i = 0; i < 16; i++) h[i] = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    int mid = ci_gdi_count();
    CHECKF(mid == before + 16, "before=%d mid=%d", before, mid);
    for (int i = 0; i < 16; i++) if (h[i]) CHECK(U(DeleteColorSpace)(h[i]));
    EXPECT_EQ(ci_gdi_count(), before);
}
TEST(color_icm_create_stress_loop) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    int before = ci_gdi_count();
    int fails = 0;
    for (int i = 0; i < 2000; i++) {
        HCOLORSPACE h = ci_mkw(CI_LCS_CALIBRATED_RGB, (DWORD)(1 << (i & 3)));
        if (!h || !U(DeleteColorSpace)(h)) fails++;
    }
    EXPECT_EQ(fails, 0);
    if (before >= 0) EXPECT_EQ(ci_gdi_count(), before);
}
TEST(color_icm_create_stress_a_loop) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceA)); REQUIRE(U(DeleteColorSpace));
    int fails = 0;
    for (int i = 0; i < 1000; i++) {
        HCOLORSPACE h = ci_mka(CI_LCS_CALIBRATED_RGB, (DWORD)(1 << (i & 3)));
        if (!h || !U(DeleteColorSpace)(h)) fails++;
    }
    EXPECT_EQ(fails, 0);
}
TEST(color_icm_create_many_simultaneous) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    enum { N = 200 };
    HCOLORSPACE h[N];
    int made = 0, dup = 0;
    for (int i = 0; i < N; i++) { h[i] = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_GRAPHICS); if (h[i]) made++; }
    EXPECT_EQ(made, N);
    for (int i = 0; i < N; i++)
        for (int j = i + 1; j < N; j++)
            if (h[i] && h[i] == h[j]) dup++;
    EXPECT_EQ(dup, 0);
    for (int i = 0; i < N; i++) if (h[i]) U(DeleteColorSpace)(h[i]);
}

/* ---- contents round trip through GetLogColorSpace ---- */
TEST(color_icm_create_w_roundtrip_fields) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(GetLogColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    LOGCOLORSPACEW w; ci_initw(&w, CI_LCS_CALIBRATED_RGB, LCS_GM_GRAPHICS);
    ci_endpoints(&w.lcsEndpoints);
    w.lcsGammaRed = 0x00020000; w.lcsGammaGreen = 0x00023333; w.lcsGammaBlue = 0x00010000;
    HCOLORSPACE h = U(CreateColorSpaceW)(&w);
    CHECK(h != NULL);
    if (!h) return;
    LOGCOLORSPACEW o; memset(&o, 0xcc, sizeof o);
    CHECK(U(GetLogColorSpaceW)(h, &o, sizeof o));
    EXPECT_EQ(o.lcsSignature, 0x50534F43);
    EXPECT_EQ(o.lcsVersion, 0x400);
    EXPECT_EQ(o.lcsCSType, CI_LCS_CALIBRATED_RGB);
    EXPECT_EQ(o.lcsIntent, LCS_GM_GRAPHICS);
    EXPECT_EQ(o.lcsGammaRed, 0x00020000);
    EXPECT_EQ(o.lcsGammaGreen, 0x00023333);
    EXPECT_EQ(o.lcsGammaBlue, 0x00010000);
    CHECK(memcmp(&o.lcsEndpoints, &w.lcsEndpoints, sizeof w.lcsEndpoints) == 0);
    EXPECT_EQ(o.lcsFilename[0], 0);
    if (o.lcsSize != CI_LCSW_SIZE) t_note("W lcsSize reads back as %lu", o.lcsSize);
    CHECK(U(DeleteColorSpace)(h));
}
TEST(color_icm_create_a_roundtrip_fields) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceA)); REQUIRE(U(GetLogColorSpaceA)); REQUIRE(U(DeleteColorSpace));
    LOGCOLORSPACEA a; ci_inita(&a, CI_LCS_CALIBRATED_RGB, LCS_GM_BUSINESS);
    ci_endpoints(&a.lcsEndpoints);
    a.lcsGammaRed = 0x00018000; a.lcsGammaGreen = 0x00018000; a.lcsGammaBlue = 0x00018000;
    HCOLORSPACE h = U(CreateColorSpaceA)(&a);
    CHECK(h != NULL);
    if (!h) return;
    LOGCOLORSPACEA o; memset(&o, 0xcc, sizeof o);
    CHECK(U(GetLogColorSpaceA)(h, &o, sizeof o));
    EXPECT_EQ(o.lcsSignature, 0x50534F43);
    EXPECT_EQ(o.lcsVersion, 0x400);
    EXPECT_EQ(o.lcsSize, CI_LCSA_SIZE);
    EXPECT_EQ(o.lcsCSType, CI_LCS_CALIBRATED_RGB);
    EXPECT_EQ(o.lcsIntent, LCS_GM_BUSINESS);
    EXPECT_EQ(o.lcsGammaRed, 0x00018000);
    CHECK(memcmp(&o.lcsEndpoints, &a.lcsEndpoints, sizeof a.lcsEndpoints) == 0);
    EXPECT_EQ(o.lcsFilename[0], 0);
    CHECK(U(DeleteColorSpace)(h));
}
TEST(color_icm_create_intent_roundtrip_table) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(GetLogColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    static const DWORD in[] = { LCS_GM_BUSINESS, LCS_GM_GRAPHICS, LCS_GM_IMAGES, LCS_GM_ABS_COLORIMETRIC };
    for (unsigned i = 0; i < 4; i++) {
        HCOLORSPACE h = ci_mkw(CI_LCS_CALIBRATED_RGB, in[i]);
        CHECK(h != NULL);
        if (!h) continue;
        LOGCOLORSPACEW o;
        CHECK(U(GetLogColorSpaceW)(h, &o, sizeof o));
        CHECKF(o.lcsIntent == in[i], "want %lu got %lu", in[i], o.lcsIntent);
        U(DeleteColorSpace)(h);
    }
}
TEST(color_icm_create_gamma_extremes_roundtrip) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(GetLogColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    static const DWORD g[] = { 0, 1, 0x10000, 0x7fffffff, 0x80000000u, 0xffffffffu };
    for (unsigned i = 0; i < sizeof g / sizeof *g; i++) {
        LOGCOLORSPACEW w; ci_initw(&w, CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
        w.lcsGammaRed = g[i]; w.lcsGammaGreen = g[(i + 1) % 6]; w.lcsGammaBlue = g[(i + 2) % 6];
        HCOLORSPACE h = U(CreateColorSpaceW)(&w);
        CHECKF(h != NULL, "gamma 0x%lx", g[i]);
        if (!h) continue;
        LOGCOLORSPACEW o;
        CHECK(U(GetLogColorSpaceW)(h, &o, sizeof o));
        EXPECT_EQ(o.lcsGammaRed, w.lcsGammaRed);
        EXPECT_EQ(o.lcsGammaGreen, w.lcsGammaGreen);
        EXPECT_EQ(o.lcsGammaBlue, w.lcsGammaBlue);
        U(DeleteColorSpace)(h);
    }
}
TEST(color_icm_create_endpoint_extremes_roundtrip) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(GetLogColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    LOGCOLORSPACEW w; ci_initw(&w, CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    FXPT2DOT30 *p = (FXPT2DOT30 *)&w.lcsEndpoints;
    static const LONG ext[] = { 0, 1, -1, 0x7fffffff, (LONG)0x80000000, 0x40000000, -0x40000000 };
    for (int i = 0; i < 9; i++) p[i] = ext[i % 7];
    HCOLORSPACE h = U(CreateColorSpaceW)(&w);
    CHECK(h != NULL);
    if (!h) return;
    LOGCOLORSPACEW o;
    CHECK(U(GetLogColorSpaceW)(h, &o, sizeof o));
    CHECK(memcmp(&o.lcsEndpoints, &w.lcsEndpoints, sizeof w.lcsEndpoints) == 0);
    U(DeleteColorSpace)(h);
}
TEST(color_icm_create_ap_parity) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceA)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(GetLogColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    HCOLORSPACE ha = ci_mka(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    HCOLORSPACE hw = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    CHECK(ha && hw);
    if (ha && hw) {
        LOGCOLORSPACEW oa, ow;
        CHECK(U(GetLogColorSpaceW)(ha, &oa, sizeof oa));
        CHECK(U(GetLogColorSpaceW)(hw, &ow, sizeof ow));
        CHECK(memcmp(&oa, &ow, sizeof oa) == 0);
    }
    if (ha) U(DeleteColorSpace)(ha);
    if (hw) U(DeleteColorSpace)(hw);
}
TEST(color_icm_create_srgb_logcolorspace_type) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(GetLogColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    HCOLORSPACE h = ci_mkw(CI_LCS_sRGB, LCS_GM_IMAGES);
    CHECK(h != NULL);
    if (!h) return;
    LOGCOLORSPACEW o; memset(&o, 0, sizeof o);
    CHECK(U(GetLogColorSpaceW)(h, &o, sizeof o));
    EXPECT_EQ(o.lcsCSType, CI_LCS_sRGB);
    EXPECT_EQ(o.lcsIntent, LCS_GM_IMAGES);
    U(DeleteColorSpace)(h);
}
TEST(color_icm_create_windows_cs_logcolorspace_type) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(GetLogColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    HCOLORSPACE h = ci_mkw(CI_LCS_WINDOWS, LCS_GM_BUSINESS);
    CHECK(h != NULL);
    if (!h) return;
    LOGCOLORSPACEW o; memset(&o, 0, sizeof o);
    CHECK(U(GetLogColorSpaceW)(h, &o, sizeof o));
    EXPECT_EQ(o.lcsCSType, CI_LCS_WINDOWS);
    EXPECT_EQ(o.lcsIntent, LCS_GM_BUSINESS);
    U(DeleteColorSpace)(h);
}
TEST(color_icm_create_input_not_modified) {
    CI_NOWINE();
    REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    LOGCOLORSPACEW w, c; ci_initw(&w, CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    ci_endpoints(&w.lcsEndpoints);
    c = w;
    HCOLORSPACE h = U(CreateColorSpaceW)(&w);
    CHECK(memcmp(&w, &c, sizeof w) == 0);
    if (h) U(DeleteColorSpace)(h);
}
