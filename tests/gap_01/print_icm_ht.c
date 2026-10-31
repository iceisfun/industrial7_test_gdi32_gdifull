#include "gap.h"

/* ================= StartFormPage / EndFormPage ================= */
/* Neither is ever pointed at a real printer DC: only memory / bogus / metafile DCs. */

static HDC memdc(void) { return U(CreateCompatibleDC)(NULL); }

TEST(gap1_startformpage_memory_dc_fails) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    StartFormPage_pfn f = UN(StartFormPage);
    REQUIRE(f);
    HDC dc = memdc();
    SetLastError(0);
    int r = f(dc);
    CHECKF(r <= 0, "ret %d", r);
    t_note("StartFormPage(memdc) -> %d err %lu", r, GetLastError());
    U(DeleteDC)(dc);
}
TEST(gap1_startformpage_null_dc_fails) {
    StartFormPage_pfn f = UN(StartFormPage);
    REQUIRE(f);
    SetLastError(0);
    int r = f(NULL);
    CHECKF(r <= 0, "ret %d", r);
    t_note("StartFormPage(NULL) -> %d err %lu", r, GetLastError());
}
TEST(gap1_startformpage_bogus_dc_fails) {
    StartFormPage_pfn f = UN(StartFormPage);
    REQUIRE(f);
    static const ULONG_PTR hs[] = { 1, 0x1234, 0x00010001, 0x7fffffff, ~(ULONG_PTR)0 };
    for (unsigned i = 0; i < sizeof hs / sizeof *hs; i++) {
        int r = f((HDC)hs[i]);
        CHECKF(r <= 0, "h %#llx ret %d", (unsigned long long)hs[i], r);
    }
}
TEST(gap1_startformpage_deleted_dc_fails) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    StartFormPage_pfn f = UN(StartFormPage);
    REQUIRE(f);
    HDC dc = memdc();
    U(DeleteDC)(dc);
    CHECK(f(dc) <= 0);
}
TEST(gap1_startformpage_matches_startpage_on_memdc) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(StartPage));
    StartFormPage_pfn f = UN(StartFormPage);
    REQUIRE(f);
    HDC dc = memdc();
    SetLastError(0); int a = f(dc); DWORD ea = GetLastError();
    SetLastError(0); int b = U(StartPage)(dc); DWORD eb = GetLastError();
    CHECKF(a == b, "StartFormPage %d StartPage %d", a, b);
    CHECKF(ea == eb, "err %lu vs %lu", ea, eb);
    U(DeleteDC)(dc);
}
TEST(gap1_startformpage_both_modules) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    StartFormPage_pfn a = (StartFormPage_pfn)t_proc_gdi32("StartFormPage"), b = (StartFormPage_pfn)t_proc_gdi32full("StartFormPage");
    REQUIRE(a); REQUIRE(b);
    HDC dc = memdc();
    CHECK(a(dc) <= 0);
    CHECK(b(dc) <= 0);
    U(DeleteDC)(dc);
}
TEST(gap1_startformpage_metafile_dc_note) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    StartFormPage_pfn f = UN(StartFormPage);
    REQUIRE(f);
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    CHECK(m != NULL); if (!m) return;
    SetLastError(0);
    int r = f(m);
    t_note("StartFormPage(emf dc) -> %d err %lu", r, GetLastError());
    U(DeleteEnhMetaFile)(U(CloseEnhMetaFile)(m));
}

TEST(gap1_endformpage_memory_dc_invalid_handle) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    EndFormPage_pfn f = UN(EndFormPage);
    REQUIRE(f);
    HDC dc = memdc();
    SetLastError(0);
    int r = f(dc);
    CHECKF(r <= 0, "ret %d", r);
    t_note("EndFormPage(memdc) -> %d", r);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    U(DeleteDC)(dc);
}
TEST(gap1_endformpage_null_dc_invalid_handle) {
    EndFormPage_pfn f = UN(EndFormPage);
    REQUIRE(f);
    SetLastError(0);
    int r = f(NULL);
    CHECKF(r <= 0, "ret %d", r);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}
TEST(gap1_endformpage_bogus_dc_invalid_handle) {
    EndFormPage_pfn f = UN(EndFormPage);
    REQUIRE(f);
    static const ULONG_PTR hs[] = { 1, 0x1234, 0x7fffffff, ~(ULONG_PTR)0 };
    for (unsigned i = 0; i < sizeof hs / sizeof *hs; i++) {
        SetLastError(0);
        int r = f((HDC)hs[i]);
        CHECKF(r <= 0, "h %#llx ret %d", (unsigned long long)hs[i], r);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "h %#llx err %lu", (unsigned long long)hs[i], GetLastError());
    }
}
TEST(gap1_endformpage_deleted_dc_invalid_handle) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    EndFormPage_pfn f = UN(EndFormPage);
    REQUIRE(f);
    HDC dc = memdc();
    U(DeleteDC)(dc);
    SetLastError(0);
    CHECK(f(dc) <= 0);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}
TEST(gap1_endformpage_non_dc_object_handle) {
    REQUIRE(U(CreateBitmap)); REQUIRE(U(DeleteObject));
    EndFormPage_pfn f = UN(EndFormPage);
    REQUIRE(f);
    HBITMAP b = U(CreateBitmap)(2, 2, 1, 1, NULL);
    SetLastError(0);
    CHECK(f((HDC)b) <= 0);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    U(DeleteObject)(b);
}
TEST(gap1_endformpage_matches_endpage_on_memdc) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(EndPage));
    EndFormPage_pfn f = UN(EndFormPage);
    REQUIRE(f);
    HDC dc = memdc();
    SetLastError(0); int a = f(dc); DWORD ea = GetLastError();
    SetLastError(0); int b = U(EndPage)(dc); DWORD eb = GetLastError();
    CHECKF(a <= 0 && b <= 0, "EndFormPage %d EndPage %d", a, b);
    t_note("errors %lu vs %lu", ea, eb);
    U(DeleteDC)(dc);
}
TEST(gap1_endformpage_both_modules) {
    EndFormPage_pfn a = (EndFormPage_pfn)t_proc_gdi32("EndFormPage"), b = (EndFormPage_pfn)t_proc_gdi32full("EndFormPage");
    REQUIRE(a); REQUIRE(b);
    SetLastError(0); CHECK(a(NULL) <= 0); EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SetLastError(0); CHECK(b(NULL) <= 0); EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}
TEST(gap1_endformpage_metafile_dc_note) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    EndFormPage_pfn f = UN(EndFormPage);
    REQUIRE(f);
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    CHECK(m != NULL); if (!m) return;
    SetLastError(0);
    int r = f(m);
    t_note("EndFormPage(emf dc) -> %d err %lu", r, GetLastError());
    U(DeleteEnhMetaFile)(U(CloseEnhMetaFile)(m));
}
TEST(gap1_endformpage_does_not_disturb_dc) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(SetTextColor)); REQUIRE(U(GetTextColor));
    EndFormPage_pfn f = UN(EndFormPage);
    StartFormPage_pfn s = UN(StartFormPage);
    REQUIRE(f); REQUIRE(s);
    HDC dc = memdc();
    U(SetTextColor)(dc, RGB(1, 2, 3));
    f(dc); s(dc);
    CHECK(U(GetTextColor)(dc) == RGB(1, 2, 3));
    CHECK(U(DeleteDC)(dc));
}

/* ================= DocumentEventEx ================= */
/* DocumentEventEx dispatches through the winspool DocumentEvent pointer set up during init; calling it
 * outside a print job could jump through an uninitialised pointer, so only the export is verified. */
TEST(gap1_documenteventex_exported_by_gdi32full_only) {
    SKIP_WINE("export layout / stub behaviour differs");
    CHECK(t_proc_gdi32full("DocumentEventEx") != NULL);
    CHECK(t_proc_gdi32("DocumentEventEx") == NULL);
}
TEST(gap1_documenteventex_ordinal_matches) {
    SKIP_WINE("export layout / stub behaviour differs");
    HMODULE m = t_gdi32full();
    REQUIRE(m);
    FARPROC byname = GetProcAddress(m, "DocumentEventEx");
    REQUIRE(byname);
    FARPROC byord = GetProcAddress(m, MAKEINTRESOURCEA(1098));
    t_note("ordinal 1098 -> %p, name -> %p", byord, byname);
    CHECK(byord == byname || byord == NULL);
}

/* ================= DeleteEMFSpoolData ================= */
/* Takes an LDC pointer and releases the EMF spool data hanging off offset 0x70, if any. A
 * zeroed block therefore has nothing to release. */
TEST(gap1_deleteemfspooldata_empty_ldc_is_noop) {
    DeleteEMFSpoolData_pfn f = (DeleteEMFSpoolData_pfn)t_proc_gdi32full("DeleteEMFSpoolData");
    REQUIRE(f);
    BYTE blk[0x400]; memset(blk, 0, sizeof blk);
    f(blk);
    BYTE z[0x400]; memset(z, 0, sizeof z);
    CHECK(!memcmp(blk, z, sizeof blk));
}
TEST(gap1_deleteemfspooldata_repeat_noop) {
    DeleteEMFSpoolData_pfn f = (DeleteEMFSpoolData_pfn)t_proc_gdi32full("DeleteEMFSpoolData");
    REQUIRE(f);
    BYTE blk[0x200]; memset(blk, 0, sizeof blk);
    for (int i = 0; i < 100; i++) f(blk);
    int nz = 0; for (int i = 0; i < 0x200; i++) nz += blk[i] != 0;
    CHECK(nz == 0);
}
TEST(gap1_deleteemfspooldata_other_fields_untouched) {
    DeleteEMFSpoolData_pfn f = (DeleteEMFSpoolData_pfn)t_proc_gdi32full("DeleteEMFSpoolData");
    REQUIRE(f);
    BYTE blk[0x200]; memset(blk, 0x5a, sizeof blk);
    memset(blk + 0x70, 0, 8);   /* only the spool data pointer is zero */
    BYTE ref[0x200]; memcpy(ref, blk, sizeof ref);
    f(blk);
    CHECK(!memcmp(blk, ref, sizeof blk));
}
TEST(gap1_deleteemfspooldata_not_in_gdi32) {
    SKIP_WINE("export layout / stub behaviour differs");
    CHECK(t_proc_gdi32("DeleteEMFSpoolData") == NULL);
    CHECK(t_proc_gdi32full("DeleteEMFSpoolData") != NULL);
}

/* ================= IsValidEnhMetaRecord / OffExt ================= */
typedef struct { DWORD iType, nSize; DWORD pad[6]; } FAKEREC;
static const DWORD k_badtypes[] = { 0, 0x7b, 0x7c, 0x80, 0xff, 0x100, 0x1000, 0xffff, 0x10000, 0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff };

TEST(gap1_isvalidemr_out_of_range_types_false) {
    IsValidEnhMetaRecord_pfn f = UN(IsValidEnhMetaRecord);
    REQUIRE(f);
    HGDIOBJ ht[4] = { 0 };
    for (unsigned i = 0; i < sizeof k_badtypes / sizeof *k_badtypes; i++) {
        FAKEREC r; memset(&r, 0, sizeof r); r.iType = k_badtypes[i]; r.nSize = 32;
        CHECKF(f(ht, &r) == FALSE, "type %#lx", k_badtypes[i]);
    }
}
TEST(gap1_isvalidemr_both_modules_out_of_range) {
    IsValidEnhMetaRecord_pfn a = (IsValidEnhMetaRecord_pfn)t_proc_gdi32("IsValidEnhMetaRecord"), b = (IsValidEnhMetaRecord_pfn)t_proc_gdi32full("IsValidEnhMetaRecord");
    REQUIRE(a); REQUIRE(b);
    HGDIOBJ ht[2] = { 0 };
    FAKEREC r; memset(&r, 0, sizeof r); r.iType = 0; r.nSize = 8;
    CHECK(a(ht, &r) == FALSE);
    CHECK(b(ht, &r) == FALSE);
    r.iType = 0x7b;
    CHECK(a(ht, &r) == FALSE);
    CHECK(b(ht, &r) == FALSE);
}
TEST(gap1_isvalidemr_nsize_irrelevant_for_bad_type) {
    IsValidEnhMetaRecord_pfn f = UN(IsValidEnhMetaRecord);
    REQUIRE(f);
    HGDIOBJ ht[2] = { 0 };
    static const DWORD sizes[] = { 0, 1, 7, 8, 12, 0xffffffff };
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        FAKEREC r; memset(&r, 0, sizeof r); r.iType = 0x200; r.nSize = sizes[i];
        CHECKF(f(ht, &r) == FALSE, "nSize %#lx", sizes[i]);
    }
}
TEST(gap1_isvalidemr_null_handletable_bad_type) {
    IsValidEnhMetaRecord_pfn f = UN(IsValidEnhMetaRecord);
    REQUIRE(f);
    FAKEREC r; memset(&r, 0, sizeof r); r.iType = 0; r.nSize = 8;
    CHECK(f(NULL, &r) == FALSE);
}
TEST(gap1_isvalidemr_offext_bad_type_false) {
    IsValidEnhMetaRecordOffExt_pfn f = UN(IsValidEnhMetaRecordOffExt);
    REQUIRE(f);
    HGDIOBJ ht[2] = { 0 };
    static const struct { DWORD off, ext; } oe[] = { {0, 0}, {0, 4}, {4, 4}, {0xffffffff, 1}, {8, 0xffffffff} };
    for (unsigned i = 0; i < sizeof k_badtypes / sizeof *k_badtypes; i++)
        for (unsigned j = 0; j < sizeof oe / sizeof *oe; j++) {
            FAKEREC r; memset(&r, 0, sizeof r); r.iType = k_badtypes[i]; r.nSize = 32;
            CHECKF(f(ht, &r, oe[j].off, oe[j].ext) == FALSE, "type %#lx off %lu ext %lu", k_badtypes[i], oe[j].off, oe[j].ext);
        }
}
TEST(gap1_isvalidemr_offext_both_modules) {
    IsValidEnhMetaRecordOffExt_pfn a = (IsValidEnhMetaRecordOffExt_pfn)t_proc_gdi32("IsValidEnhMetaRecordOffExt"), b = (IsValidEnhMetaRecordOffExt_pfn)t_proc_gdi32full("IsValidEnhMetaRecordOffExt");
    REQUIRE(a); REQUIRE(b);
    HGDIOBJ ht[2] = { 0 };
    FAKEREC r; memset(&r, 0, sizeof r); r.iType = 0; r.nSize = 32;
    CHECK(a(ht, &r, 0, 0) == FALSE);
    CHECK(b(ht, &r, 0, 0) == FALSE);
}

typedef struct { int n, valid, validoe, bad_oe, past_end; } ISV_CTX;
static IsValidEnhMetaRecord_pfn g_isv; static IsValidEnhMetaRecordOffExt_pfn g_isvoe;
static int CALLBACK isv_cb(HDC dc, HANDLETABLE *ht, const ENHMETARECORD *r, int nobj, LPARAM lp) {
    ISV_CTX *c = (ISV_CTX *)lp;
    c->n++;
    if (g_isv(ht, r)) c->valid++;
    if (g_isvoe(ht, r, 0, 0)) c->validoe++;
    if (g_isvoe(ht, r, r->nSize, 0)) c->past_end++;       /* offset == nSize is out of the record */
    if (g_isvoe(ht, r, 0, r->nSize + 1)) c->bad_oe++;     /* extent longer than the record (but inside the file for non-last records) */
    return 1;
}
TEST(gap1_isvalidemr_real_records_in_callback_note) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(EnumEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    REQUIRE(U(Rectangle)); REQUIRE(U(SetMapMode)); REQUIRE(U(CreateSolidBrush));
    g_isv = UN(IsValidEnhMetaRecord); g_isvoe = UN(IsValidEnhMetaRecordOffExt);
    REQUIRE(g_isv); REQUIRE(g_isvoe);
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    CHECK(m != NULL); if (!m) return;
    HBRUSH b = U(CreateSolidBrush)(RGB(1, 2, 3));
    U(SetMapMode)(m, MM_TEXT);
    HGDIOBJ o = U(SelectObject)(m, b);
    U(Rectangle)(m, 1, 2, 30, 40);
    U(SelectObject)(m, o);
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    ISV_CTX c; memset(&c, 0, sizeof c);
    RECT rc = { 0, 0, 100, 100 };
    U(EnumEnhMetaFile)(NULL, h, isv_cb, &c, &rc);
    t_note("records %d valid %d validOffExt(0,0) %d offset==nSize ok %d overlong ext ok %d", c.n, c.valid, c.validoe, c.past_end, c.bad_oe);
    CHECK(c.n >= 4);
    CHECK(c.past_end == 0);   /* offset equal to the record size is never inside the record */
    U(DeleteEnhMetaFile)(h); U(DeleteObject)(b);
}

/* ================= UpdateICMRegKeyA / W ================= */
static const UINT k_icmcmds[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 0x10, 0xff, 0x100, 0xffff, 0x7fffffff, 0x80000000, 0xffffffff };

TEST(gap1_updateicmregkeyw_always_not_supported) {
    SKIP_WINE("export layout / stub behaviour differs");
    UpdateICMRegKeyW_pfn f = UN(UpdateICMRegKeyW);
    REQUIRE(f);
    for (unsigned i = 0; i < sizeof k_icmcmds / sizeof *k_icmcmds; i++) {
        SetLastError(0);
        BOOL r = f(0, L"cmid", L"profile.icm", k_icmcmds[i]);
        CHECKF(r == FALSE, "cmd %#x", k_icmcmds[i]);
        CHECKF(GetLastError() == ERROR_NOT_SUPPORTED, "cmd %#x err %lu", k_icmcmds[i], GetLastError());
    }
}
TEST(gap1_updateicmregkeya_always_not_supported) {
    SKIP_WINE("export layout / stub behaviour differs");
    UpdateICMRegKeyA_pfn f = UN(UpdateICMRegKeyA);
    REQUIRE(f);
    for (unsigned i = 0; i < sizeof k_icmcmds / sizeof *k_icmcmds; i++) {
        SetLastError(0);
        BOOL r = f(0, "cmid", "profile.icm", k_icmcmds[i]);
        CHECKF(r == FALSE, "cmd %#x", k_icmcmds[i]);
        CHECKF(GetLastError() == ERROR_NOT_SUPPORTED, "cmd %#x err %lu", k_icmcmds[i], GetLastError());
    }
}
TEST(gap1_updateicmregkey_null_pointers) {
    SKIP_WINE("export layout / stub behaviour differs");
    UpdateICMRegKeyA_pfn a = UN(UpdateICMRegKeyA); UpdateICMRegKeyW_pfn w = UN(UpdateICMRegKeyW);
    REQUIRE(a); REQUIRE(w);
    SetLastError(0); CHECK(a(0, NULL, NULL, 1) == FALSE); EXPECT_ERR(0, ERROR_NOT_SUPPORTED);
    SetLastError(0); CHECK(w(0, NULL, NULL, 1) == FALSE); EXPECT_ERR(0, ERROR_NOT_SUPPORTED);
    SetLastError(0); CHECK(a(0, NULL, "x", 2) == FALSE); EXPECT_ERR(0, ERROR_NOT_SUPPORTED);
    SetLastError(0); CHECK(w(0, L"x", NULL, 3) == FALSE); EXPECT_ERR(0, ERROR_NOT_SUPPORTED);
}
TEST(gap1_updateicmregkey_reserved_value_ignored) {
    SKIP_WINE("export layout / stub behaviour differs");
    UpdateICMRegKeyA_pfn a = UN(UpdateICMRegKeyA); UpdateICMRegKeyW_pfn w = UN(UpdateICMRegKeyW);
    REQUIRE(a); REQUIRE(w);
    static const DWORD rs[] = { 0, 1, 0x1234, 0xffffffff };
    for (unsigned i = 0; i < 4; i++) {
        SetLastError(0); CHECK(a(rs[i], "a", "b", 1) == FALSE); EXPECT_ERR(0, ERROR_NOT_SUPPORTED);
        SetLastError(0); CHECK(w(rs[i], L"a", L"b", 1) == FALSE); EXPECT_ERR(0, ERROR_NOT_SUPPORTED);
    }
}
TEST(gap1_updateicmregkey_does_not_touch_strings) {
    SKIP_WINE("export layout / stub behaviour differs");
    UpdateICMRegKeyA_pfn a = UN(UpdateICMRegKeyA);
    REQUIRE(a);
    char id[] = "id-string", prof[] = "p.icm";
    a(0, id, prof, 1);
    EXPECT_STR(id, "id-string");
    EXPECT_STR(prof, "p.icm");
}
TEST(gap1_updateicmregkey_garbage_pointers_unused) {
    SKIP_WINE("export layout / stub behaviour differs");
    UpdateICMRegKeyA_pfn a = UN(UpdateICMRegKeyA); UpdateICMRegKeyW_pfn w = UN(UpdateICMRegKeyW);
    REQUIRE(a); REQUIRE(w);
    /* the stub never dereferences its arguments */
    SetLastError(0); CHECK(a(0, (LPSTR)(ULONG_PTR)1, (LPSTR)(ULONG_PTR)2, 1) == FALSE); EXPECT_ERR(0, ERROR_NOT_SUPPORTED);
    SetLastError(0); CHECK(w(0, (LPWSTR)(ULONG_PTR)1, (LPWSTR)(ULONG_PTR)2, 1) == FALSE); EXPECT_ERR(0, ERROR_NOT_SUPPORTED);
}
TEST(gap1_updateicmregkey_both_modules) {
    SKIP_WINE("export layout / stub behaviour differs");
    UpdateICMRegKeyA_pfn a1 = (UpdateICMRegKeyA_pfn)t_proc_gdi32("UpdateICMRegKeyA"), a2 = (UpdateICMRegKeyA_pfn)t_proc_gdi32full("UpdateICMRegKeyA");
    UpdateICMRegKeyW_pfn w1 = (UpdateICMRegKeyW_pfn)t_proc_gdi32("UpdateICMRegKeyW"), w2 = (UpdateICMRegKeyW_pfn)t_proc_gdi32full("UpdateICMRegKeyW");
    REQUIRE(a1); REQUIRE(a2); REQUIRE(w1); REQUIRE(w2);
    SetLastError(0); CHECK(a1(0, "", "", 0) == FALSE); EXPECT_ERR(0, ERROR_NOT_SUPPORTED);
    SetLastError(0); CHECK(a2(0, "", "", 0) == FALSE); EXPECT_ERR(0, ERROR_NOT_SUPPORTED);
    SetLastError(0); CHECK(w1(0, L"", L"", 0) == FALSE); EXPECT_ERR(0, ERROR_NOT_SUPPORTED);
    SetLastError(0); CHECK(w2(0, L"", L"", 0) == FALSE); EXPECT_ERR(0, ERROR_NOT_SUPPORTED);
}
TEST(gap1_updateicmregkey_a_w_share_code) {
    SKIP_WINE("export layout / stub behaviour differs");
    FARPROC a = t_proc_gdi32full("UpdateICMRegKeyA"), w = t_proc_gdi32full("UpdateICMRegKeyW");
    REQUIRE(a); REQUIRE(w);
    t_note("A and W are the same stub in gdi32full: %d", a == w);
}
TEST(gap1_updateicmregkey_last_error_replaces_previous) {
    SKIP_WINE("export layout / stub behaviour differs");
    UpdateICMRegKeyW_pfn w = UN(UpdateICMRegKeyW);
    REQUIRE(w);
    SetLastError(ERROR_ACCESS_DENIED);
    w(0, L"a", L"b", 1);
    EXPECT_ERR(0, ERROR_NOT_SUPPORTED);
}

/* ================= IcmReleaseCachedColorSpace / IcmDeleteLocalDC ================= */
TEST(gap1_icmreleasecached_unknown_key_returns_true) {
    IcmReleaseCachedColorSpace_pfn f = (IcmReleaseCachedColorSpace_pfn)t_proc_gdi32full("IcmReleaseCachedColorSpace");
    REQUIRE(f);
    static const ULONG_PTR ks[] = { 0x13572468, 0x7e57ab1e, 0xdeadbeef00000001ULL, 0x1 };
    for (unsigned i = 0; i < sizeof ks / sizeof *ks; i++)
        CHECKF(f((void *)ks[i]) == TRUE, "key %#llx", (unsigned long long)ks[i]);
}
TEST(gap1_icmreleasecached_null_key) {
    IcmReleaseCachedColorSpace_pfn f = (IcmReleaseCachedColorSpace_pfn)t_proc_gdi32full("IcmReleaseCachedColorSpace");
    REQUIRE(f);
    CHECK(f(NULL) == TRUE);
}
TEST(gap1_icmreleasecached_repeated) {
    IcmReleaseCachedColorSpace_pfn f = (IcmReleaseCachedColorSpace_pfn)t_proc_gdi32full("IcmReleaseCachedColorSpace");
    REQUIRE(f);
    int ok = 1;
    for (int i = 0; i < 1000; i++) if (f((void *)(ULONG_PTR)(0x40000000 + i * 8)) != TRUE) ok = 0;
    CHECK(ok);
}
TEST(gap1_icmreleasecached_not_in_gdi32) {
    SKIP_WINE("export layout / stub behaviour differs");
    CHECK(t_proc_gdi32("IcmReleaseCachedColorSpace") == NULL);
    CHECK(t_proc_gdi32full("IcmReleaseCachedColorSpace") != NULL);
}
TEST(gap1_icmreleasecached_no_lasterror_touch) {
    IcmReleaseCachedColorSpace_pfn f = (IcmReleaseCachedColorSpace_pfn)t_proc_gdi32full("IcmReleaseCachedColorSpace");
    REQUIRE(f);
    SetLastError(0x2222);
    f((void *)(ULONG_PTR)0x3333);
    EXPECT_ERR(0, 0x2222);
}
TEST(gap1_icmdeletelocaldc_zeroed_ldc_returns_true) {
    IcmDeleteLocalDC_pfn f = (IcmDeleteLocalDC_pfn)t_proc_gdi32full("IcmDeleteLocalDC");
    REQUIRE(f);
    /* a zeroed LDC has no ICM info attached (offset 0x108) and no ICM flags, so only the
     * "deselect transform" step with a NULL DC runs */
    BYTE ldc[0x400]; memset(ldc, 0, sizeof ldc);
    CHECK(f(NULL, ldc, NULL) == TRUE);
    t_note("ldc +0xf8 after: %llx", *(unsigned long long *)(ldc + 0xf8));
}
TEST(gap1_icmdeletelocaldc_not_in_gdi32) {
    SKIP_WINE("export layout / stub behaviour differs");
    CHECK(t_proc_gdi32("IcmDeleteLocalDC") == NULL);
    CHECK(t_proc_gdi32full("IcmDeleteLocalDC") != NULL);
}

/* ================= HT_Get8BPPFormatPalette ================= */
TEST(gap1_ht8fmt_null_buffer_returns_216) {
    HT_Get8BPPFormatPalette_pfn f = UN(HT_Get8BPPFormatPalette);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    CHECK(f(NULL, 0, 0, 0) == 216);
    CHECK(f(NULL, 10000, 10000, 10000) == 216);
    CHECK(f(NULL, 0xffff, 0xffff, 0xffff) == 216);
    CHECK(f(NULL, 1, 2, 3) == 216);
}
TEST(gap1_ht8fmt_null_buffer_both_calls_same) {
    HT_Get8BPPFormatPalette_pfn f = UN(HT_Get8BPPFormatPalette);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    ULONG a = f(NULL, 100, 200, 300), b = f(NULL, 100, 200, 300);
    CHECK(a == b);
}
TEST(gap1_ht8fmt_fills_864_bytes_no_overrun) {
    HT_Get8BPPFormatPalette_pfn f = UN(HT_Get8BPPFormatPalette);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    BYTE buf[0x360 + 64]; memset(buf, 0xcd, sizeof buf);
    ULONG r = f(buf, 10000, 10000, 10000);
    t_note("HT_Get8BPPFormatPalette -> %lu, first entries %02x%02x%02x%02x %02x%02x%02x%02x", r, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
    for (int i = 0x360; i < 0x360 + 64; i++) if (buf[i] != 0xcd) { CHECKF(0, "overrun at %#x", i); break; }
}
TEST(gap1_ht8fmt_deterministic) {
    HT_Get8BPPFormatPalette_pfn f = UN(HT_Get8BPPFormatPalette);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    BYTE a[0x360], b[0x360]; memset(a, 0x11, sizeof a); memset(b, 0xee, sizeof b);
    ULONG ra = f(a, 10000, 10000, 10000), rb = f(b, 10000, 10000, 10000);
    CHECK(ra == rb);
    if (ra) CHECK(!memcmp(a, b, sizeof a));
}
TEST(gap1_ht8fmt_gamma_table_note) {
    HT_Get8BPPFormatPalette_pfn f = UN(HT_Get8BPPFormatPalette);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    static const USHORT gs[] = { 0, 5000, 10000, 20000, 22000, 0xffff };
    BYTE ref[0x360]; ULONG rr = f(ref, 10000, 10000, 10000);
    for (unsigned i = 0; i < sizeof gs / sizeof *gs; i++) {
        BYTE b[0x360]; memset(b, 0, sizeof b);
        ULONG r = f(b, gs[i], gs[i], gs[i]);
        t_note("gamma %u -> ret %lu, same-as-10000 %d", gs[i], r, r == rr && !memcmp(b, ref, sizeof b));
    }
}
TEST(gap1_ht8fmt_palette_entries_are_distinct_colors_note) {
    HT_Get8BPPFormatPalette_pfn f = UN(HT_Get8BPPFormatPalette);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    DWORD pal[216]; memset(pal, 0, sizeof pal);
    ULONG r = f(pal, 10000, 10000, 10000);
    int distinct = 0;
    for (int i = 0; i < 216; i++) {
        int dup = 0;
        for (int j = 0; j < i; j++) if ((pal[i] & 0xffffff) == (pal[j] & 0xffffff)) { dup = 1; break; }
        distinct += !dup;
    }
    t_note("ret %lu, %d distinct of 216; pal[0]=%08lx pal[215]=%08lx", r, distinct, pal[0], pal[215]);
}
TEST(gap1_ht8fmt_not_in_gdi32full) {
    SKIP_WINE("export layout / stub behaviour differs");
    CHECK(t_proc_gdi32("HT_Get8BPPFormatPalette") != NULL);
    CHECK(t_proc_gdi32full("HT_Get8BPPFormatPalette") == NULL);
}

/* ================= HT_Get8BPPMaskPalette ================= */
TEST(gap1_ht8mask_null_buffer_count_constant) {
    HT_Get8BPPMaskPalette_pfn f = UN(HT_Get8BPPMaskPalette);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    ULONG c = f(NULL, FALSE, 0, 0, 0, 0);
    t_note("mask palette entry count %lu", c);
    CHECK(f(NULL, TRUE, 0, 0, 0, 0) == c);
    CHECK(f(NULL, TRUE, 0xff, 10000, 10000, 10000) == c);
    CHECK(f(NULL, FALSE, 7, 0xffff, 1, 2) == c);
}
TEST(gap1_ht8mask_null_buffer_sane_count) {
    HT_Get8BPPMaskPalette_pfn f = UN(HT_Get8BPPMaskPalette);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    ULONG c = f(NULL, FALSE, 0, 0, 0, 0);
    CHECKF(c > 0 && c <= 0x9c4000, "count %lu", c);
}
TEST(gap1_ht8mask_fills_without_overrun) {
    HT_Get8BPPMaskPalette_pfn f = UN(HT_Get8BPPMaskPalette);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    ULONG c = f(NULL, FALSE, 0, 0, 0, 0);
    if (c == 0 || c > 4096) { t_skip("unexpected count %lu", c); return; }
    BYTE *buf = (BYTE *)malloc(c * 4 + 64);
    memset(buf, 0, c * 4); memset(buf + c * 4, 0xcd, 64);
    ULONG r = f(buf, TRUE, 0, 10000, 10000, 10000);
    t_note("HT_Get8BPPMaskPalette(buf) -> %lu", r);
    for (int i = 0; i < 64; i++) if (buf[c * 4 + i] != 0xcd) { CHECKF(0, "overrun at +%d", i); break; }
    free(buf);
}
TEST(gap1_ht8mask_deterministic) {
    HT_Get8BPPMaskPalette_pfn f = UN(HT_Get8BPPMaskPalette);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    ULONG c = f(NULL, FALSE, 0, 0, 0, 0);
    if (c == 0 || c > 4096) { t_skip("unexpected count %lu", c); return; }
    BYTE *a = (BYTE *)calloc(c, 4), *b = (BYTE *)calloc(c, 4);
    ULONG ra = f(a, TRUE, 0, 10000, 10000, 10000), rb = f(b, TRUE, 0, 10000, 10000, 10000);
    CHECK(ra == rb);
    if (ra) CHECK(!memcmp(a, b, c * 4));
    free(a); free(b);
}
TEST(gap1_ht8mask_param_variants_note) {
    HT_Get8BPPMaskPalette_pfn f = UN(HT_Get8BPPMaskPalette);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    ULONG c = f(NULL, FALSE, 0, 0, 0, 0);
    if (c == 0 || c > 4096) { t_skip("unexpected count %lu", c); return; }
    BYTE *a = (BYTE *)calloc(c + 16, 4);
    static const struct { BOOL m; BYTE cmy; USHORT g; } v[] = { {0,0,0}, {1,0,10000}, {1,1,10000}, {1,7,10000}, {0,0,20000}, {1,0xff,0xffff} };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        memset(a, 0, c * 4); memset(a + c * 4, 0xcd, 64);
        ULONG r = f(a, v[i].m, v[i].cmy, v[i].g, v[i].g, v[i].g);
        int over = 0; for (int k = 0; k < 64; k++) over |= a[c * 4 + k] != 0xcd;
        CHECKF(!over, "variant %u overrun", i);
        t_note("variant %u (mask %d cmy %u gamma %u) -> %lu", i, v[i].m, v[i].cmy, v[i].g, r);
    }
    free(a);
}
TEST(gap1_ht8mask_not_in_gdi32full) {
    SKIP_WINE("export layout / stub behaviour differs");
    CHECK(t_proc_gdi32("HT_Get8BPPMaskPalette") != NULL);
    CHECK(t_proc_gdi32full("HT_Get8BPPMaskPalette") == NULL);
}
