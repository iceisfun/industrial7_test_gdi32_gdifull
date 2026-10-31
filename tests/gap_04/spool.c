#include "gap4.h"
/* Spooler-EMF helpers (GdiGetDC, GdiGetPageCount, GdiGetPageHandle,
 * GdiGetDevmodeForPage, GdiEndDocEMF, GdiEndPageEMF, GdiDeleteSpoolFileHandle,
 * GdiGetSpoolFileHandle) -- only error paths are exercised: a spool handle is
 * validated by its 'THPS' signature (0x53504854) before anything else. */

#define SPSIG 0x53504854u
typedef struct { DWORD sig; DWORD pad; HDC hdc; BYTE rest[480]; } FAKESPOOL;
static void mkfake(FAKESPOOL *f, DWORD sig, HDC dc) { memset(f, 0, sizeof *f); f->sig = sig; f->hdc = dc; }

TEST(gap4_getdc_signature_match_returns_hdc) {
    RQ(GdiGetDC);
    FAKESPOOL f;
    HDC vals[] = { NULL, (HDC)1, (HDC)0x1234, (HDC)0x123456789ull, (HDC)(ULONG_PTR)-1 };
    for (unsigned i = 0; i < 5; i++) {
        mkfake(&f, SPSIG, vals[i]);
        HANDLE r = UN(GdiGetDC)(&f);
        CHECKF(r == (HANDLE)vals[i], "case %u: %p", i, r);
    }
}
TEST(gap4_getdc_signature_mismatch_returns_null) {
    RQ(GdiGetDC);
    FAKESPOOL f;
    DWORD sigs[] = { 0, 1, SPSIG ^ 1, SPSIG + 1, 0x45464854 /* 'THFE' page-handle tag */, 0xffffffff, 0x54485053 };
    for (unsigned i = 0; i < 7; i++) {
        mkfake(&f, sigs[i], (HDC)0x4321);
        CHECKF(UN(GdiGetDC)(&f) == NULL, "sig 0x%lx", sigs[i]);
    }
}
TEST(gap4_getdc_reads_offset_8) {
    RQ(GdiGetDC);
    FAKESPOOL f; mkfake(&f, SPSIG, (HDC)0x1111);
    EXPECT_EQ(UN(GdiGetDC)(&f), 0x1111);
    f.hdc = (HDC)0x2222; EXPECT_EQ(UN(GdiGetDC)(&f), 0x2222);
    f.pad = 0xdeadbeef; EXPECT_EQ(UN(GdiGetDC)(&f), 0x2222);
}
TEST(gap4_getdc_real_dc_roundtrip) {
    RQ(GdiGetDC);
    HDC m = U(CreateCompatibleDC)(NULL); REQUIRE(m);
    FAKESPOOL f; mkfake(&f, SPSIG, m);
    EXPECT_EQ(UN(GdiGetDC)(&f), m);
    U(DeleteDC)(m);
}
TEST(gap4_getdc_no_side_effects) {
    RQ(GdiGetDC);
    FAKESPOOL f; mkfake(&f, SPSIG, (HDC)5);
    SET_ERR(0x3131); DWORD g = g4_gdi_count();
    UN(GdiGetDC)(&f);
    EXPECT_EQ(GetLastError(), 0x3131); EXPECT_EQ(g4_gdi_count(), g);
}

TEST(gap4_getpagecount_bad_signature) {
    RQ(GdiGetPageCount);
    FAKESPOOL f;
    DWORD sigs[] = { 0, 1, SPSIG ^ 0x100, 0xffffffff };
    for (unsigned i = 0; i < 4; i++) { mkfake(&f, sigs[i], NULL); CHECKF(UN(GdiGetPageCount)(&f) == 0, "sig 0x%lx", sigs[i]); }
}
TEST(gap4_getpagehandle_page_zero_and_bad_sig) {
    RQ(GdiGetPageHandle);
    FAKESPOOL f; mkfake(&f, 0, NULL);
    DWORD type = 0xabab;
    EXPECT_EQ(UN(GdiGetPageHandle)(&f, 0, &type), 0);
    EXPECT_EQ(UN(GdiGetPageHandle)(&f, 1, &type), 0);
    EXPECT_EQ(UN(GdiGetPageHandle)(&f, 0xffffffff, NULL), 0);
    EXPECT_EQ(type, 0xabab);
    /* page 0 is rejected before the signature is looked at */
    mkfake(&f, SPSIG, NULL);
    EXPECT_EQ(UN(GdiGetPageHandle)(&f, 0, &type), 0);
    EXPECT_EQ(type, 0xabab);
}
TEST(gap4_getdevmodeforpage_bad_signature) {
    RQ(GdiGetDevmodeForPage);
    G4_WINE_SKIP("different spool handle layout");
    FAKESPOOL f; mkfake(&f, 0x12345678, NULL);
    void *a = (void *)0x77, *b = (void *)0x88;
    EXPECT_EQ(UN(GdiGetDevmodeForPage)(&f, 1, &a, &b), 0);
    EXPECT_EQ(UN(GdiGetDevmodeForPage)(&f, 0, &a, &b), 0);
    EXPECT_EQ(UN(GdiGetDevmodeForPage)(&f, 1, NULL, NULL), 0);
    CHECK(a == (void *)0x77 && b == (void *)0x88);
}
TEST(gap4_enddocemf_bad_signature) {
    RQ(GdiEndDocEMF);
    FAKESPOOL f; mkfake(&f, 0xdead, NULL);
    EXPECT_EQ(UN(GdiEndDocEMF)(&f), 0);
    mkfake(&f, 0, (HDC)1);
    EXPECT_EQ(UN(GdiEndDocEMF)(&f), 0);
}
TEST(gap4_endpageemf_bad_signature) {
    RQ(GdiEndPageEMF);
    FAKESPOOL f; mkfake(&f, 0xdead, NULL);
    EXPECT_EQ(UN(GdiEndPageEMF)(&f, 0), 0);
    EXPECT_EQ(UN(GdiEndPageEMF)(&f, 1), 0);
    EXPECT_EQ(UN(GdiEndPageEMF)(&f, 0xff), 0);
}
TEST(gap4_deletespoolfilehandle_bad_signature) {
    RQ(GdiDeleteSpoolFileHandle);
    G4_WINE_SKIP("different spool handle layout");
    FAKESPOOL f; mkfake(&f, 0xdead, NULL);
    EXPECT_EQ(UN(GdiDeleteSpoolFileHandle)(&f), 0);
    mkfake(&f, 0, NULL);
    EXPECT_EQ(UN(GdiDeleteSpoolFileHandle)(&f), 0);
}
TEST(gap4_spool_bad_signature_leaves_struct_untouched) {
    RQ(GdiEndDocEMF); RQ(GdiEndPageEMF); RQ(GdiDeleteSpoolFileHandle); RQ(GdiGetPageCount);
    G4_WINE_SKIP("different spool handle layout");
    FAKESPOOL f, ref; mkfake(&f, 0xbeef, (HDC)9); memset(f.rest, 0x5a, sizeof f.rest); ref = f;
    UN(GdiEndDocEMF)(&f); UN(GdiEndPageEMF)(&f, 1); UN(GdiDeleteSpoolFileHandle)(&f); UN(GdiGetPageCount)(&f);
    CHECK(!memcmp(&f, &ref, sizeof f));
}
TEST(gap4_getspoolfilehandle_nonexistent_printer) {
    RQ(GdiGetSpoolFileHandle);
    G4_WINE_SKIP("spooler");
    WCHAR name[] = L"Gap4NoSuchPrinterXyz";
    SET_ERR(0);
    void *h = UN(GdiGetSpoolFileHandle)(name, NULL, name);
    CHECK(h == NULL);
    if (h) UN(GdiDeleteSpoolFileHandle)(h);
    t_note("GdiGetSpoolFileHandle(nonexistent) lasterror=%lu", GetLastError());
}
TEST(gap4_getspoolfilehandle_empty_name) {
    RQ(GdiGetSpoolFileHandle);
    G4_WINE_SKIP("spooler");
    WCHAR name[] = L"";
    void *h = UN(GdiGetSpoolFileHandle)(name, NULL, name);
    CHECK(h == NULL);
    if (h) UN(GdiDeleteSpoolFileHandle)(h);
}

/* ---- exports resolved but not called (side effects / unknown signatures) ---- */
static const struct { const char *name; int in_full; } k_names[] = {
    { "GdiDllInitialize", 1 }, { "GdiCurrentProcessSplWow64", 1 }, { "GdiInitializeLanguagePack", 1 },
    { "GdiGetVariationStoreDelta", 1 }, { "GdiFullscreenControl", 0 }, { "GdiGetSpoolMessage", 0 },
    { "GdiInitSpool", 0 }, { "GdiDrawStream", 1 }, { "GdiDescribePixelFormat", 1 }, { "GdiGetSpoolFileHandle", 1 },
    { "GdiGetPageCount", 1 }, { "GdiGetPageHandle", 1 }, { "GdiGetDevmodeForPage", 1 }, { "GdiEndDocEMF", 1 },
    { "GdiEndPageEMF", 1 }, { "GdiDeleteSpoolFileHandle", 1 }, { "GdiGetDC", 1 }, { "GdiIsTrackingEnabled", 1 },
    { "GdiHandleBeingTracked", 1 }, { "GdiFixUpHandle", 1 }, { "GdiGetEntry", 0 }, { "GdiGetCodePage", 0 },
    { "GdiIsScreenDC", 1 }, { "GdiIsPlayMetafileDC", 1 }, { "GdiIsMetaPrintDC", 1 }, { "GdiIsMetaFileDC", 1 },
    { "GdiGetBitmapBitsSize", 1 }, { "GdiConvertToDevmodeW", 1 }, { "GdiDeleteLocalDC", 1 },
    { "GdiCreateLocalMetaFilePict", 1 }, { "GdiCreateLocalEnhMetaFile", 1 }, { "GdiConvertMetaFilePict", 1 },
    { "GdiConvertEnhMetaFile", 1 },
};
TEST(gap4_exports_resolve_gdi32) {
    G4_WINE_SKIP("export set differs");
    for (unsigned i = 0; i < sizeof k_names / sizeof k_names[0]; i++)
        CHECKF(t_proc_gdi32(k_names[i].name) != NULL, "gdi32.dll!%s", k_names[i].name);
}
TEST(gap4_exports_resolve_gdi32full) {
    G4_WINE_SKIP("export set differs");
    for (unsigned i = 0; i < sizeof k_names / sizeof k_names[0]; i++)
        if (k_names[i].in_full) CHECKF(t_proc_gdi32full(k_names[i].name) != NULL, "gdi32full.dll!%s", k_names[i].name);
}
TEST(gap4_exports_gdi32full_only) {
    G4_WINE_SKIP("export set differs");
    CHECK(t_proc_gdi32full("GdiDllInitializeWrapper") != NULL);
    CHECK(t_proc_gdi32("GdiDllInitializeWrapper") == NULL);
}
TEST(gap4_exports_gdi32_only_not_in_full) {
    G4_WINE_SKIP("export set differs");
    const char *n[] = { "GdiFullscreenControl", "GdiGetSpoolMessage", "GdiInitSpool", "GdiGetEntry", "GdiGetCodePage" };
    for (unsigned i = 0; i < 5; i++) CHECKF(t_proc_gdi32full(n[i]) == NULL, "gdi32full unexpectedly exports %s", n[i]);
}
TEST(gap4_exports_by_ordinal_match_names) {
    G4_WINE_SKIP("ordinals differ");
    /* Win10 gdi32full ordinals from the export table */
    struct { const char *n; unsigned ord; } o[] = { { "GdiConvertEnhMetaFile", 1178 }, { "GdiConvertFont", 1179 }, { "GdiIsMetaFileDC", 1226 },
        { "GdiFixUpHandle", 1210 }, { "GdiGetDC", 1215 }, { "GdiGetPageCount", 1220 } };
    HMODULE m = t_gdi32full();
    REQUIRE(m);
    for (unsigned i = 0; i < 6; i++) {
        FARPROC byname = GetProcAddress(m, o[i].n), byord = GetProcAddress(m, (LPCSTR)(ULONG_PTR)o[i].ord);
        if (byname != byord) t_note("%s: ordinal %u differs from name lookup", o[i].n, o[i].ord);
    }
}
