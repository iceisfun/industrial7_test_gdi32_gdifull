/* Spool-EMF helpers (GdiPlayEMF family), error paths only, plus process/DLL init exports that are resolve-only. */
#include "gap5.h"

#define SPOOL_MAGIC  0x53504854   /* 'THPS' */
#define PAGE_MAGIC   0x45464854   /* 'THFE' */

/* ---- GdiStartPageEMF: returns (hdr->magic == 'THPS') ---- */
TEST(gap5_startpageemf_magic_true) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiStartPageEMF));
    DWORD buf[64] = {SPOOL_MAGIC};
    EXPECT_EQ(G(GdiStartPageEMF)(buf), 1);
}
TEST(gap5_startpageemf_magic_false) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiStartPageEMF));
    DWORD vals[] = {0, 1, PAGE_MAGIC, 0x54485053, SPOOL_MAGIC ^ 1, SPOOL_MAGIC + 1, 0xFFFFFFFF, 0x53504854u << 1};
    for (unsigned i = 0; i < sizeof vals / sizeof *vals; i++) {
        DWORD buf[64] = {vals[i]};
        CHECKF(G(GdiStartPageEMF)(buf) == 0, "val 0x%lx", (unsigned long)vals[i]);
    }
}
TEST(gap5_startpageemf_only_first_dword_matters) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiStartPageEMF));
    DWORD buf[64]; memset(buf, 0xFF, sizeof buf);
    buf[0] = SPOOL_MAGIC;
    EXPECT_EQ(G(GdiStartPageEMF)(buf), 1);
    memset(buf, 0, sizeof buf); buf[1] = SPOOL_MAGIC;
    EXPECT_EQ(G(GdiStartPageEMF)(buf), 0);
}
TEST(gap5_startpageemf_no_side_effects) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiStartPageEMF));
    DWORD buf[64] = {SPOOL_MAGIC, 1, 2, 3}, copy[64];
    memcpy(copy, buf, sizeof buf);
    SetLastError(0x6666);
    G(GdiStartPageEMF)(buf);
    CHECK(!memcmp(copy, buf, sizeof buf));
    EXPECT_EQ(GetLastError(), 0x6666);
    SetLastError(0);
}

/* ---- GdiStartDocEMF: bad magic -> 0 without touching the structure ---- */
TEST(gap5_startdocemf_bad_magic) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiStartDocEMF));
    DWORD vals[] = {0, PAGE_MAGIC, 0xFFFFFFFF, 1};
    for (unsigned i = 0; i < sizeof vals / sizeof *vals; i++) {
        DWORD buf[96] = {vals[i]}, copy[96];
        memcpy(copy, buf, sizeof buf);
        CHECKF(G(GdiStartDocEMF)(buf, NULL) == 0, "val 0x%lx", (unsigned long)vals[i]);
        CHECK(!memcmp(copy, buf, sizeof buf));
    }
}
TEST(gap5_startdocemf_bad_magic_ignores_docinfo) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiStartDocEMF));
    DWORD buf[96] = {0};
    DWORD di[16] = {0x28};
    EXPECT_EQ(G(GdiStartDocEMF)(buf, di), 0);
}

/* ---- GdiResetDCEMF ---- */
TEST(gap5_resetdcemf_bad_magic) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiResetDCEMF));
    DWORD buf[96] = {0};
    EXPECT_EQ(G(GdiResetDCEMF)(buf, NULL), 0);
    buf[0] = PAGE_MAGIC;
    EXPECT_EQ(G(GdiResetDCEMF)(buf, NULL), 0);
    buf[0] = 0xFFFFFFFF;
    EXPECT_EQ(G(GdiResetDCEMF)(buf, NULL), 0);
}
TEST(gap5_resetdcemf_good_magic_null_devmode) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiResetDCEMF));
    DWORD buf[96] = {SPOOL_MAGIC}, copy[96];
    memcpy(copy, buf, sizeof buf);
    EXPECT_EQ(G(GdiResetDCEMF)(buf, NULL), 0);
    CHECK(!memcmp(copy, buf, sizeof buf));   /* nothing recorded for a NULL devmode */
}

/* ---- GdiPlayPageEMF: validates both headers before doing anything ---- */
TEST(gap5_playpageemf_bad_spool_magic) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiPlayPageEMF));
    DWORD sp[128] = {0}, pg[16] = {PAGE_MAGIC, 1};
    EXPECT_EQ(G(GdiPlayPageEMF)(sp, pg, NULL, NULL, NULL), 0);
}
TEST(gap5_playpageemf_bad_page_magic) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiPlayPageEMF));
    DWORD sp[128] = {SPOOL_MAGIC}, pg[16] = {0, 1};
    EXPECT_EQ(G(GdiPlayPageEMF)(sp, pg, NULL, NULL, NULL), 0);
    pg[0] = SPOOL_MAGIC;
    EXPECT_EQ(G(GdiPlayPageEMF)(sp, pg, NULL, NULL, NULL), 0);
}
TEST(gap5_playpageemf_zero_page_number) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiPlayPageEMF));
    DWORD sp[128] = {SPOOL_MAGIC}, pg[16] = {PAGE_MAGIC, 0};
    EXPECT_EQ(G(GdiPlayPageEMF)(sp, pg, NULL, NULL, NULL), 0);
}
TEST(gap5_playpageemf_both_zero) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiPlayPageEMF));
    DWORD sp[128] = {0}, pg[16] = {0};
    EXPECT_EQ(G(GdiPlayPageEMF)(sp, pg, NULL, NULL, NULL), 0);
}
TEST(gap5_playpageemf_no_mutation_on_reject) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiPlayPageEMF));
    DWORD sp[128] = {1, 2, 3}, pg[16] = {4, 5, 6}, a[128], b[16];
    memcpy(a, sp, sizeof sp); memcpy(b, pg, sizeof pg);
    G(GdiPlayPageEMF)(sp, pg, NULL, NULL, NULL);
    CHECK(!memcmp(a, sp, sizeof sp));
    CHECK(!memcmp(b, pg, sizeof pg));
}
TEST(gap5_playprivatepageemf_bad_spool_magic) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiPlayPrivatePageEMF));
    DWORD sp[128] = {0}, copy[128];
    memcpy(copy, sp, sizeof sp);
    G(GdiPlayPrivatePageEMF)(sp, NULL, NULL);
    CHECK(!memcmp(copy, sp, sizeof sp));
}
TEST(gap5_playprivatepageemf_bad_magic_values) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    REQUIRE(G(GdiPlayPrivatePageEMF));
    DWORD vals[] = {PAGE_MAGIC, 1, 0xFFFFFFFF};
    for (unsigned i = 0; i < 3; i++) {
        DWORD sp[128] = {vals[i]};
        G(GdiPlayPrivatePageEMF)(sp, (void *)1, NULL);
        CHECK(sp[0] == vals[i]);
    }
}

/* ---- GdiPlayEMF: open of a nonexistent printer fails cleanly ---- */
TEST(gap5_playemf_nonexistent_printer) {
    REQUIRE(G(GdiPlayEMF));
    if (g5_is_wine()) { t_skip("wine"); return; }
    static const WCHAR nm[] = L"gap5-no-such-printer-0xDEAD";
    DWORD c = g5_gdi_count();
    ULONG_PTR r = G(GdiPlayEMF)(nm, NULL, nm, NULL, NULL);
    EXPECT_EQ(r & 0xFFFFFFFF, 0);
    EXPECT_EQ(g5_gdi_count(), c);
}
TEST(gap5_playemf_nonexistent_printer_repeat) {
    REQUIRE(G(GdiPlayEMF));
    if (g5_is_wine()) { t_skip("wine"); return; }
    static const WCHAR nm[] = L"gap5-no-such-printer-0xBEEF";
    DWORD c = g5_gdi_count();
    for (int i = 0; i < 5; i++) EXPECT_EQ(G(GdiPlayEMF)(nm, NULL, nm, NULL, NULL) & 0xFFFFFFFF, 0);
    EXPECT_EQ(g5_gdi_count(), c);
}

/* ---- Exports that are resolved only: calling them would init the process, query the kernel with
 *      unknown layouts, or drive a printer driver. ---- */
TEST(gap5_resolve_internal_exports) {
    const char *n[] = {"GdiProcessSetup", "GdiPrinterThunk", "GdiQueryFonts", "Gdi32DllInitialize",
        "LpkInitialize", "LpkDrawTextEx", "LpkpEditControlSize", "LpkpInitializeEditControl", "LpkEditControl",
        "GdiGetSpoolFileHandle", "GdiStartDocEMF"};
    if (g5_is_wine()) { t_skip("wine export set differs"); return; }
    if (!t_proc_gdi32("GdiProcessSetup")) { t_skip("no gdi32 GdiProcessSetup"); return; }
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++)
        CHECKF(t_proc_gdi32(n[i]) != NULL, "gdi32 lacks %s", n[i]);
}
TEST(gap5_gdi32_only_exports_not_in_gdi32full) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    if (!t_proc_gdi32("GdiQueryFonts")) { t_skip("no GdiQueryFonts"); return; }
    const char *n[] = {"GdiQueryFonts", "Gdi32DllInitialize", "LpkEditControl", "LpkGetEditControl",
        "LpkpEditControlSize", "LpkpInitializeEditControl", "GdiSetLastError", "GdiReleaseDC"};
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++)
        CHECKF(t_proc_gdi32full(n[i]) == NULL, "gdi32full unexpectedly exports %s", n[i]);
}
TEST(gap5_gdi32full_exports_forwarded_by_gdi32) {
    if (g5_is_wine()) { t_skip("wine"); return; }
    if (!t_proc_gdi32full("GdiProcessSetup")) { t_skip("no gdi32full export"); return; }
    const char *n[] = {"GdiProcessSetup", "GdiPrinterThunk", "GdiValidateHandle", "GdiPlayEMF", "GdiIsUMPDSandboxingEnabled",
        "GdiSetPixelFormat", "GdiSwapBuffers", "GdiLoadType1Fonts", "GdiSetAttrs", "GdiSupportsFontChangeEvent"};
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++) {
        FARPROC a = t_proc_gdi32(n[i]), b = t_proc_gdi32full(n[i]);
        CHECKF(a != NULL && b != NULL, "%s: gdi32=%p full=%p", n[i], (void *)a, (void *)b);
        if (a && b && a != b) t_note("%s not a plain forwarder (gdi32 %p, gdi32full %p)", n[i], (void *)a, (void *)b);
    }
}
TEST(gap5_gdiprocesssetup_not_invoked_note) {
    /* GdiProcessSetup initialises process-wide critical sections and font tables: never called here. */
    CHECK(1);
}
TEST(gap5_gdi32dllinitialize_not_invoked_note) {
    /* Gdi32DllInitialize is the DllMain; reason 1 would re-disable thread calls and re-run init. */
    CHECK(1);
}
TEST(gap5_gdiprinterthunk_not_invoked_note) {
    /* GdiPrinterThunk dispatches on a UMTHDR type into printer-driver code. */
    CHECK(1);
}
