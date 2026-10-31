/* StartPage / EndPage / EndDoc / AbortDoc (error paths on non-printer DCs only) and CreateScalableFontResourceW. */
#include "gap10.h"

/* Documented return of these four is "<= 0 on failure"; the Win10 implementation returns -1 and sets ERROR_INVALID_HANDLE
 * for every handle that is not a printer/spool DC (display/memory DCs, bogus values, other object types). */
typedef int (WINAPI *DCFN)(HDC);

static void run_invalid_set(DCFN f, const char *nm) {
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef, (HDC)(ULONG_PTR)0xFFFFFFFFFFFFFFFF, (HDC)(ULONG_PTR)0x7FFFFFFF };
    unsigned i;
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        int r; CLR();
        r = f(bad[i]);
        CHECKF(r == -1, "%s(%p) = %d", nm, bad[i], r);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "%s(%p) err %lu", nm, bad[i], GetLastError());
    }
}
static void run_memdc(DCFN f, const char *nm) {
    HDC dc = U(CreateCompatibleDC)(NULL); int r;
    CHECK(dc != NULL);
    if (!dc) return;
    CLR();
    r = f(dc);
    CHECKF(r == -1, "%s(memdc) = %d", nm, r);
    CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "%s(memdc) err %lu", nm, GetLastError());
    /* DC must stay fully usable afterwards */
    CHECK(U(GetDeviceCaps)(dc, BITSPIXEL) > 0);
    CHECK(U(DeleteDC)(dc));
}
static void run_displaydc(DCFN f, const char *nm) {
    HDC dc = U(CreateDCW)(L"DISPLAY", NULL, NULL, NULL); int r;
    if (!dc) { t_skip("no display DC"); return; }
    CLR();
    r = f(dc);
    CHECKF(r == -1, "%s(display DC) = %d", nm, r);
    CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "%s(display DC) err %lu", nm, GetLastError());
    U(DeleteDC)(dc);
}
static void run_wrongtype(DCFN f, const char *nm) {
    HBRUSH b = U(CreateSolidBrush)(0); HPEN p = U(CreatePen)(PS_SOLID, 1, 0); HRGN r = U(CreateRectRgn)(0, 0, 1, 1);
    HGDIOBJ objs[3]; unsigned i; objs[0] = b; objs[1] = p; objs[2] = r;
    for (i = 0; i < 3; i++) {
        int rc; CLR();
        rc = f((HDC)objs[i]);
        CHECKF(rc == -1, "%s(obj %u) = %d", nm, i, rc);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "%s(obj %u) err %lu", nm, i, GetLastError());
    }
    U(DeleteObject)(b); U(DeleteObject)(p); U(DeleteObject)(r);
}
static void run_deleted(DCFN f, const char *nm) {
    HDC dc = U(CreateCompatibleDC)(NULL); int r;
    U(DeleteDC)(dc);
    CLR();
    r = f(dc);
    CHECKF(r == -1, "%s(deleted) = %d", nm, r);
    t_note("%s(deleted DC) err %lu", nm, GetLastError());
}
static void run_metadc(DCFN f, const char *nm) {
    HDC dc = U(CreateMetaFileW)(NULL); int r;
    if (!dc) { t_skip("no metafile DC"); return; }
    CLR();
    r = f(dc);
    CHECKF(r == -1, "%s(metafile DC) = %d", nm, r);
    CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "%s(metafile DC) err %lu", nm, GetLastError());
    { HMETAFILE h = U(CloseMetaFile)(dc); if (h) U(DeleteMetaFile)(h); }
}
static void run_enhdc(DCFN f, const char *nm) {
    HDC dc = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL); int r;
    if (!dc) { t_skip("no EMF DC"); return; }
    CLR();
    r = f(dc);
    t_note("%s(enhmetafile DC) = %d err %lu", nm, r, GetLastError());
    CHECK(r <= 0 || r == 1);
    { HENHMETAFILE h = U(CloseEnhMetaFile)(dc); if (h) U(DeleteEnhMetaFile)(h); }
}

#define PAGEAPI_TESTS(NAME, FN) \
TEST(gap10_##NAME##_invalid_handles) { REQUIRE(U(FN)); run_invalid_set(U(FN), #FN); } \
TEST(gap10_##NAME##_memory_dc) { REQUIRE(U(FN)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(GetDeviceCaps)); REQUIRE(U(DeleteDC)); run_memdc(U(FN), #FN); } \
TEST(gap10_##NAME##_display_dc) { REQUIRE(U(FN)); REQUIRE(U(CreateDCW)); REQUIRE(U(DeleteDC)); run_displaydc(U(FN), #FN); } \
TEST(gap10_##NAME##_wrong_object_types) { REQUIRE(U(FN)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(CreatePen)); REQUIRE(U(CreateRectRgn)); REQUIRE(U(DeleteObject)); run_wrongtype(U(FN), #FN); } \
TEST(gap10_##NAME##_deleted_dc) { REQUIRE(U(FN)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); run_deleted(U(FN), #FN); } \
TEST(gap10_##NAME##_metafile_dc) { REQUIRE(U(FN)); REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile)); SKIP_WINE("metafile DC page APIs"); run_metadc(U(FN), #FN); } \
TEST(gap10_##NAME##_enhmetafile_dc) { REQUIRE(U(FN)); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile)); SKIP_WINE("EMF DC page APIs"); run_enhdc(U(FN), #FN); } \
TEST(gap10_##NAME##_repeated_calls_stable) { \
    REQUIRE(U(FN)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); \
    HDC dc = U(CreateCompatibleDC)(NULL); int i, ok = 1; \
    for (i = 0; i < 200; i++) { CLR(); if (U(FN)(dc) != -1 || GetLastError() != ERROR_INVALID_HANDLE) ok = 0; } \
    CHECK(ok); U(DeleteDC)(dc); } \
TEST(gap10_##NAME##_does_not_disturb_dc_state) { \
    REQUIRE(U(FN)); NEED_MD(); REQUIRE(U(SetTextColor)); REQUIRE(U(GetTextColor)); REQUIRE(U(SetBkMode)); REQUIRE(U(GetBkMode)); \
    MD m; if (!md_create(&m, 8, 8, 32)) { t_skip("no DIB"); return; } \
    U(SetTextColor)(m.dc, 0x123456); U(SetBkMode)(m.dc, TRANSPARENT); md_clear(&m, 0x00808080); \
    U(FN)(m.dc); \
    EXPECT_EQ(U(GetTextColor)(m.dc), 0x123456); EXPECT_EQ(U(GetBkMode)(m.dc), TRANSPARENT); \
    EXPECT_EQ(md_px(&m, 3, 3), 0x00808080); md_rect(&m, 0, 0, 2, 2, RED_); EXPECT_EQ(md_px(&m, 1, 1), RED_); \
    md_destroy(&m); } \
TEST(gap10_##NAME##_gdi32_vs_gdi32full) { \
    DCFN a = (DCFN)t_proc_gdi32(#FN), b = (DCFN)t_proc_gdi32full(#FN); REQUIRE(a); \
    CLR(); EXPECT_EQ(a(NULL), -1); EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE); \
    if (b) { CLR(); EXPECT_EQ(b(NULL), -1); EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE); } } \
static void NAME##_thr(void *p) { HDC dc = U(CreateCompatibleDC)(NULL); CLR(); *(int *)p = U(FN)(dc); *((int *)p + 1) = (int)GetLastError(); U(DeleteDC)(dc); } \
TEST(gap10_##NAME##_other_thread) { \
    REQUIRE(U(FN)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); \
    int r[2] = { 0, 0 }; g10_run_thread(NAME##_thr, r); \
    EXPECT_EQ(r[0], -1); EXPECT_EQ(r[1], ERROR_INVALID_HANDLE); }

PAGEAPI_TESTS(startpage, StartPage)
PAGEAPI_TESTS(endpage, EndPage)
PAGEAPI_TESTS(enddoc, EndDoc)
PAGEAPI_TESTS(abortdoc, AbortDoc)

TEST(gap10_docapis_sequence_on_memdc) {
    REQUIRE(U(StartPage)); REQUIRE(U(EndPage)); REQUIRE(U(EndDoc)); REQUIRE(U(AbortDoc)); NEED_MD();
    HDC dc = U(CreateCompatibleDC)(NULL);
    CLR(); EXPECT_EQ(U(StartPage)(dc), -1); EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    CLR(); EXPECT_EQ(U(EndPage)(dc), -1); EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    CLR(); EXPECT_EQ(U(EndDoc)(dc), -1); EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    CLR(); EXPECT_EQ(U(AbortDoc)(dc), -1); EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    U(DeleteDC)(dc);
}
TEST(gap10_docapis_no_gdi_leak) {
    REQUIRE(U(StartPage)); REQUIRE(U(EndPage)); REQUIRE(U(EndDoc)); REQUIRE(U(AbortDoc)); NEED_MD();
    if (!g10_gdicount()) { t_skip("no GetGuiResources"); return; }
    HDC dc = U(CreateCompatibleDC)(NULL); DWORD before = g10_gdicount(); int i;
    for (i = 0; i < 100; i++) { U(StartPage)(dc); U(EndPage)(dc); U(EndDoc)(dc); U(AbortDoc)(dc); }
    EXPECT_EQ(g10_gdicount(), before);
    U(DeleteDC)(dc);
}
TEST(gap10_docapis_escape_startdoc_not_started) {
    /* a DC that never had StartDoc: Escape(ENDDOC/ABORTDOC/NEWFRAME) style calls are also rejected, not crashing */
    REQUIRE(U(Escape)); NEED_MD();
    HDC dc = U(CreateCompatibleDC)(NULL); int r;
    CLR(); r = U(Escape)(dc, ENDDOC, 0, NULL, NULL);
    t_note("Escape(ENDDOC) on memdc -> %d err %lu", r, GetLastError());
    CLR(); r = U(Escape)(dc, ABORTDOC, 0, NULL, NULL);
    t_note("Escape(ABORTDOC) on memdc -> %d err %lu", r, GetLastError());
    CHECK(U(GetDeviceCaps)(dc, HORZRES) > 0);
    U(DeleteDC)(dc);
}

/* ---------------- CreateScalableFontResourceW ---------------- */
typedef BOOL (WINAPI *CSFR_W)(DWORD, LPCWSTR, LPCWSTR, LPCWSTR);
typedef BOOL (WINAPI *CSFR_A)(DWORD, LPCSTR, LPCSTR, LPCSTR);

static void tmp_fot(WCHAR *out, const char *tag) {
    WCHAR dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    wsprintfW(out, L"%sg10_%S_%lu.fot", dir, tag, GetCurrentProcessId());
    DeleteFileW(out);
}
static int exists(const WCHAR *p) { return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES; }

TEST(gap10_csfr_null_args) {
    CSFR_W f = (CSFR_W)t_proc("CreateScalableFontResourceW"); REQUIRE(f);
    WCHAR fot[MAX_PATH]; tmp_fot(fot, "null");
    CLR(); EXPECT_EQ(f(0, NULL, NULL, NULL), 0); EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    CLR(); EXPECT_EQ(f(0, fot, NULL, NULL), 0); EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    CLR(); EXPECT_EQ(f(0, NULL, L"arial.ttf", NULL), 0); EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    CLR(); EXPECT_EQ(f(1, NULL, NULL, L"C:\\"), 0); EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    CHECK(!exists(fot));
}
TEST(gap10_csfr_missing_font_file) {
    CSFR_W f = (CSFR_W)t_proc("CreateScalableFontResourceW"); REQUIRE(f);
    WCHAR fot[MAX_PATH]; tmp_fot(fot, "missing");
    CLR();
    BOOL r = f(0, fot, L"C:\\no_such_dir_g10\\nosuchfont_g10.ttf", NULL);
    CHECK(!r);
    t_note("missing ttf: err %lu", GetLastError());
    CHECK(GetLastError() != ERRS);
    if (exists(fot)) { t_note("fot created despite failure"); DeleteFileW(fot); }
}
TEST(gap10_csfr_not_a_font_file) {
    CSFR_W f = (CSFR_W)t_proc("CreateScalableFontResourceW"); REQUIRE(f);
    WCHAR fot[MAX_PATH], txt[MAX_PATH], dir[MAX_PATH]; HANDLE h; DWORD wr;
    tmp_fot(fot, "notfont");
    GetTempPathW(MAX_PATH, dir);
    wsprintfW(txt, L"%sg10_notfont_%lu.ttf", dir, GetCurrentProcessId());
    h = CreateFileW(txt, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    WriteFile(h, "this is not a font", 18, &wr, NULL); CloseHandle(h);
    CLR();
    BOOL r = f(0, fot, txt, NULL);
    CHECK(!r);
    t_note("garbage ttf: err %lu", GetLastError());
    if (exists(fot)) DeleteFileW(fot);
    DeleteFileW(txt);
}
TEST(gap10_csfr_empty_strings) {
    CSFR_W f = (CSFR_W)t_proc("CreateScalableFontResourceW"); REQUIRE(f);
    CLR(); BOOL r = f(0, L"", L"", NULL);
    CHECK(!r);
    t_note("empty names: err %lu", GetLastError());
    CLR(); r = f(0, L"", L"arial.ttf", L"");
    CHECK(!r);
    CLR(); r = f(0, L"g10_x.fot", L"", NULL);
    CHECK(!r);
    DeleteFileW(L"g10_x.fot");
}
TEST(gap10_csfr_path_too_long) {
    CSFR_W f = (CSFR_W)t_proc("CreateScalableFontResourceW"); REQUIRE(f);
    WCHAR big[1100]; int i;
    for (i = 0; i < 1099; i++) big[i] = L'a';
    big[1099] = 0;
    CLR(); BOOL r = f(0, big, L"arial.ttf", NULL);
    CHECK(!r);
    CLR(); r = f(0, L"x.fot", big, NULL);
    CHECK(!r);
    CLR(); r = f(0, L"x.fot", L"arial.ttf", big);
    CHECK(!r);
    DeleteFileW(L"x.fot");
}
TEST(gap10_csfr_existing_fot_fails) {
    CSFR_W f = (CSFR_W)t_proc("CreateScalableFontResourceW"); REQUIRE(f);
    WCHAR fot[MAX_PATH], ttf[MAX_PATH]; HANDLE h; DWORD wr; char buf[8] = { 0 };
    tmp_fot(fot, "exists");
    h = CreateFileW(fot, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    WriteFile(h, "KEEP", 4, &wr, NULL); CloseHandle(h);
    GetWindowsDirectoryW(ttf, MAX_PATH); lstrcatW(ttf, L"\\Fonts\\arial.ttf");
    CLR();
    BOOL r = f(0, fot, ttf, NULL);
    CHECK(!r);    /* documented: fails if the resource file already exists */
    t_note("existing fot: err %lu", GetLastError());
    h = CreateFileW(fot, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) { ReadFile(h, buf, 4, &wr, NULL); CloseHandle(h); }
    EXPECT_STR(buf, "KEEP");
    DeleteFileW(fot);
}
TEST(gap10_csfr_create_from_arial_observed) {
    CSFR_W f = (CSFR_W)t_proc("CreateScalableFontResourceW"); REQUIRE(f); SKIP_WINE("CreateScalableFontResource");
    WCHAR fot[MAX_PATH], ttf[MAX_PATH]; BOOL r;
    tmp_fot(fot, "arial");
    GetWindowsDirectoryW(ttf, MAX_PATH); lstrcatW(ttf, L"\\Fonts\\arial.ttf");
    if (!exists(ttf)) { t_skip("arial.ttf absent"); return; }
    CLR();
    r = f(0, fot, ttf, NULL);
    t_note("create FOT from %ls: %d err %lu", ttf, r, GetLastError());
    if (r) {
        WIN32_FILE_ATTRIBUTE_DATA d;
        CHECK(exists(fot));
        if (GetFileAttributesExW(fot, GetFileExInfoStandard, &d)) { CHECK(d.nFileSizeLow > 0 && d.nFileSizeHigh == 0); t_note("fot size %lu", d.nFileSizeLow); }
    } else CHECK(!exists(fot));
    DeleteFileW(fot);
}
TEST(gap10_csfr_hidden_flag_observed) {
    CSFR_W f = (CSFR_W)t_proc("CreateScalableFontResourceW"); REQUIRE(f); SKIP_WINE("CreateScalableFontResource");
    WCHAR fot[MAX_PATH], ttf[MAX_PATH]; BOOL r;
    tmp_fot(fot, "hidden");
    GetWindowsDirectoryW(ttf, MAX_PATH); lstrcatW(ttf, L"\\Fonts\\arial.ttf");
    if (!exists(ttf)) { t_skip("arial.ttf absent"); return; }
    r = f(1, fot, ttf, NULL);
    t_note("hidden=1: %d", r);
    if (exists(fot)) { CHECK(r); DeleteFileW(fot); }
    DeleteFileW(fot);
}
TEST(gap10_csfr_relative_path_with_current_path_observed) {
    CSFR_W f = (CSFR_W)t_proc("CreateScalableFontResourceW"); REQUIRE(f); SKIP_WINE("CreateScalableFontResource");
    WCHAR fot[MAX_PATH], dir[MAX_PATH]; BOOL r;
    tmp_fot(fot, "rel");
    GetWindowsDirectoryW(dir, MAX_PATH); lstrcatW(dir, L"\\Fonts");
    if (GetFileAttributesW(L"C:\\Windows\\Fonts\\arial.ttf") == INVALID_FILE_ATTRIBUTES) { t_skip("no arial.ttf"); return; }
    CLR();
    r = f(0, fot, L"arial.ttf", dir);
    t_note("relative ttf + current path: %d err %lu", r, GetLastError());
    if (r) CHECK(exists(fot));
    DeleteFileW(fot);
}
TEST(gap10_csfr_missing_output_dir) {
    CSFR_W f = (CSFR_W)t_proc("CreateScalableFontResourceW"); REQUIRE(f); SKIP_WINE("CreateScalableFontResource");
    WCHAR ttf[MAX_PATH];
    GetWindowsDirectoryW(ttf, MAX_PATH); lstrcatW(ttf, L"\\Fonts\\arial.ttf");
    if (!exists(ttf)) { t_skip("no arial.ttf"); return; }
    CLR();
    BOOL r = f(0, L"C:\\no_such_dir_g10\\out.fot", ttf, NULL);
    CHECK(!r);
    t_note("missing output dir: err %lu", GetLastError());
}
TEST(gap10_csfr_ansi_matches_wide_argument_validation) {
    CSFR_A fa = (CSFR_A)t_proc("CreateScalableFontResourceA"); CSFR_W fw = (CSFR_W)t_proc("CreateScalableFontResourceW");
    REQUIRE(fa); REQUIRE(fw);
    CLR(); EXPECT_EQ(fa(0, NULL, NULL, NULL), 0); DWORD ea = GetLastError();
    CLR(); EXPECT_EQ(fw(0, NULL, NULL, NULL), 0); DWORD ew = GetLastError();
    EXPECT_EQ(ea, ew);
    CLR(); EXPECT_EQ(fa(0, "x.fot", NULL, NULL), 0);
    CLR(); EXPECT_EQ(fa(0, NULL, "arial.ttf", NULL), 0);
}
TEST(gap10_csfr_never_installs_font) {
    CSFR_W f = (CSFR_W)t_proc("CreateScalableFontResourceW"); REQUIRE(f); SKIP_WINE("CreateScalableFontResource");
    REQUIRE(U(EnumFontFamiliesExW)); REQUIRE(U(CreateDCW));
    WCHAR fot[MAX_PATH], ttf[MAX_PATH];
    tmp_fot(fot, "noinst");
    GetWindowsDirectoryW(ttf, MAX_PATH); lstrcatW(ttf, L"\\Fonts\\arial.ttf");
    if (!exists(ttf)) { t_skip("no arial.ttf"); return; }
    f(0, fot, ttf, NULL);
    DeleteFileW(fot);
    CHECK(!exists(fot));
}
