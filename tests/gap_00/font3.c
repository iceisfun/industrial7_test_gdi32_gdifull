/* GetFontResourceInfoW, GetStringBitmapA/W, font association / EUDC / linking, font file helpers */
#include "gap.h"

UDECL(BOOL, GetFontResourceInfoW, (LPCWSTR, LPDWORD, LPVOID, DWORD));
UDECL(DWORD, GetStringBitmapA, (HDC, LPCSTR, UINT, UINT, BYTE *));
UDECL(DWORD, GetStringBitmapW, (HDC, LPCWSTR, UINT, UINT, BYTE *));
UDECL(DWORD, GetFontAssocStatus, (HDC));
UDECL(DWORD, QueryFontAssocStatus, (void));
UDECL(BOOL, FontIsLinked, (HDC));
UDECL(BOOL, AnyLinkedFonts, (void));
UDECL(BOOL, EudcLoadLinkW, (LPCWSTR, LPCWSTR, int, int));
UDECL(BOOL, EudcUnloadLinkW, (LPCWSTR, LPCWSTR));
UDECL(DWORD, SetFontEnumeration, (DWORD));
UDECL(BOOL, GetFontFileData, (UINT, UINT, ULONGLONG, PVOID, SIZE_T));
UDECL(BOOL, GetFontFileInfo, (UINT, UINT, PVOID, SIZE_T, SIZE_T *));
UDECL(DWORD, GetFontDataImpl, (HDC, DWORD, DWORD, PVOID, DWORD));
UDECL(BOOL, GetProcessSessionFonts, (HANDLE, PVOID, UINT *, PVOID, UINT *));

static int arial_path(WCHAR *p, size_t n) {
    WCHAR w[MAX_PATH];
    if (!GetWindowsDirectoryW(w, MAX_PATH)) return 0;
    wsprintfW(p, L"%s\\Fonts\\arial.ttf", w);
    return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES;
}

/* ================= GetFontResourceInfoW ================= */
TEST(gap0_fri_w_resolve) {
    CHECK(t_proc_gdi32("GetFontResourceInfoW") != NULL);
    CHECK(t_proc_gdi32full("GetFontResourceInfoW") != NULL);
}
TEST(gap0_frinfo_null_path) {
    GetFontResourceInfoW_pfn f = UN(GetFontResourceInfoW); REQUIRE(f);
    DWORD cb = 0; BYTE buf[16];
    SET_ERR(0x6001);
    CHECK(!f(NULL, &cb, NULL, 1));
    CHECK(!f(NULL, &cb, buf, 1));
    CHECK(!f(NULL, &cb, NULL, 2));
    CHECK(!f(NULL, &cb, NULL, 4));
    EXPECT_EQ(GetLastError(), 0x6001);
}
TEST(gap0_frinfo_nonzero_cb_null_buffer) {
    GetFontResourceInfoW_pfn f = UN(GetFontResourceInfoW); REQUIRE(f);
    DWORD cb = 64;
    CHECK(!f(L"C:\\Windows\\Fonts\\arial.ttf", &cb, NULL, 1));
    EXPECT_EQ(cb, 64);
    cb = 1;
    CHECK(!f(L"C:\\Windows\\Fonts\\arial.ttf", &cb, NULL, 2));
    EXPECT_EQ(cb, 1);
}
TEST(gap0_frinfo_nonexistent_file) {
    GetFontResourceInfoW_pfn f = UN(GetFontResourceInfoW); REQUIRE(f);
    DWORD cb = 0;
    for (DWORD q = 1; q <= 3; q++) {
        cb = 0;
        CHECKF(!f(L"C:\\gap0_does_not_exist\\nofont.ttf", &cb, NULL, q), "query %lu", q);
    }
    cb = 0;
    CHECK(!f(L"gap0_nofont_here.ttf", &cb, NULL, 1));
    cb = 0;
    CHECK(!f(L"", &cb, NULL, 1));
}
TEST(gap0_frinfo_directory_not_font) {
    GetFontResourceInfoW_pfn f = UN(GetFontResourceInfoW); REQUIRE(f);
    WCHAR w[MAX_PATH]; GetWindowsDirectoryW(w, MAX_PATH);
    DWORD cb = 0;
    CHECK(!f(w, &cb, NULL, 1));
}
TEST(gap0_frinfo_not_a_font_file) {
    GetFontResourceInfoW_pfn f = UN(GetFontResourceInfoW); REQUIRE(f);
    WCHAR tmp[MAX_PATH], path[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp); wsprintfW(path, L"%sgap0_notfont_%lu.ttf", tmp, GetCurrentProcessId());
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot write temp file"); return; }
    char junk[512]; memset(junk, 0x41, sizeof junk); DWORD wr; WriteFile(h, junk, sizeof junk, &wr, NULL); CloseHandle(h);
    DWORD cb = 0;
    for (DWORD q = 1; q <= 3; q++) { cb = 0; CHECKF(!f(path, &cb, NULL, q), "query %lu on garbage", q); }
    DeleteFileW(path);
}
TEST(gap0_frinfo_empty_file) {
    GetFontResourceInfoW_pfn f = UN(GetFontResourceInfoW); REQUIRE(f);
    WCHAR tmp[MAX_PATH], path[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp); wsprintfW(path, L"%sgap0_empty_%lu.ttf", tmp, GetCurrentProcessId());
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot write temp file"); return; }
    CloseHandle(h);
    DWORD cb = 0;
    CHECK(!f(path, &cb, NULL, 1));
    DeleteFileW(path);
}
TEST(gap0_frinfo_arial_description_size) {
    GetFontResourceInfoW_pfn f = UN(GetFontResourceInfoW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    WCHAR p[MAX_PATH]; if (!arial_path(p, MAX_PATH)) { t_skip("no arial.ttf"); return; }
    DWORD cb = 0;
    BOOL r = f(p, &cb, NULL, 1);
    t_note("description size query: ret %d cb %lu", r, cb);
    CHECK(r);
    CHECK(cb >= 2 && (cb % 2) == 0);
}
TEST(gap0_frinfo_arial_description_content) {
    GetFontResourceInfoW_pfn f = UN(GetFontResourceInfoW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    WCHAR p[MAX_PATH]; if (!arial_path(p, MAX_PATH)) { t_skip("no arial.ttf"); return; }
    DWORD cb = 0;
    if (!f(p, &cb, NULL, 1) || cb == 0 || cb > 4096) { t_skip("no description"); return; }
    WCHAR *buf = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cb + 16);
    DWORD cb2 = cb;
    BOOL r = f(p, &cb2, buf, 1);
    CHECK(r);
    EXPECT_EQ(cb2, cb);
    SOFT(wcsstr(buf, L"Arial") != NULL, "description mentions Arial");
    t_note("description: %ls", buf);
    HeapFree(GetProcessHeap(), 0, buf);
}
TEST(gap0_frinfo_arial_buffer_too_small) {
    GetFontResourceInfoW_pfn f = UN(GetFontResourceInfoW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    WCHAR p[MAX_PATH]; if (!arial_path(p, MAX_PATH)) { t_skip("no arial.ttf"); return; }
    DWORD cb = 0;
    if (!f(p, &cb, NULL, 1) || cb < 4 || cb > 4096) { t_skip("no description"); return; }
    BYTE buf[4096];
    DWORD small = cb - 2;
    BOOL r = f(p, &small, buf, 1);
    t_note("too-small buffer: ret %d, cb now %lu (needed %lu)", r, small, cb);
    SOFT(!r, "buffer one WCHAR short fails");
    DWORD zero = 0; BYTE b1[1];
    r = f(p, &zero, b1, 1);
    SOFT(r, "cb==0 is a size query even with a buffer");
}
TEST(gap0_frinfo_arial_logfont) {
    GetFontResourceInfoW_pfn f = UN(GetFontResourceInfoW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    WCHAR p[MAX_PATH]; if (!arial_path(p, MAX_PATH)) { t_skip("no arial.ttf"); return; }
    DWORD cb = 0;
    BOOL r = f(p, &cb, NULL, 2);
    t_note("logfont size query: ret %d cb %lu", r, cb);
    SOFT(r, "LOGFONT query succeeds for a TrueType file");
    if (r && cb) {
        SOFT(cb % sizeof(LOGFONTW) == 0, "multiple of LOGFONTW");
        LOGFONTW *lf = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cb + 8);
        DWORD c2 = cb;
        BOOL r2 = f(p, &c2, lf, 2);
        SOFT(r2, "LOGFONT fetch");
        if (r2) { SOFT(!lstrcmpiW(lf[0].lfFaceName, L"Arial"), "first face is Arial"); t_note("logfont[0]: %ls w=%ld", lf[0].lfFaceName, lf[0].lfWeight); }
        HeapFree(GetProcessHeap(), 0, lf);
    }
}
TEST(gap0_frinfo_arial_type3) {
    GetFontResourceInfoW_pfn f = UN(GetFontResourceInfoW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    WCHAR p[MAX_PATH]; if (!arial_path(p, MAX_PATH)) { t_skip("no arial.ttf"); return; }
    DWORD cb = 0;
    BOOL r = f(p, &cb, NULL, 3);
    t_note("query 3: ret %d cb %lu", r, cb);
}
TEST(gap0_frinfo_unknown_query_types) {
    GetFontResourceInfoW_pfn f = UN(GetFontResourceInfoW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    WCHAR p[MAX_PATH]; if (!arial_path(p, MAX_PATH)) { t_skip("no arial.ttf"); return; }
    DWORD q[] = { 0, 5, 6, 99, 0xffffffff };
    for (unsigned i = 0; i < sizeof q / sizeof *q; i++) {
        DWORD cb = 0;
        BOOL r = f(p, &cb, NULL, q[i]);
        t_note("query type 0x%lx -> %d cb=%lu", q[i], r, cb);
    }
}
TEST(gap0_frinfo_fot_query_on_ttf) {
    GetFontResourceInfoW_pfn f = UN(GetFontResourceInfoW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    WCHAR p[MAX_PATH]; if (!arial_path(p, MAX_PATH)) { t_skip("no arial.ttf"); return; }
    DWORD cb = 0;
    BOOL r = f(p, &cb, NULL, 4);
    t_note("FOT query on a plain TTF: ret %d cb %lu", r, cb);
    DWORD cb2 = 0;
    CHECK(!f(L"C:\\gap0_no_such.fot", &cb2, NULL, 4));
}
TEST(gap0_frinfo_repeat_stable) {
    GetFontResourceInfoW_pfn f = UN(GetFontResourceInfoW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    WCHAR p[MAX_PATH]; if (!arial_path(p, MAX_PATH)) { t_skip("no arial.ttf"); return; }
    DWORD first = 0; BOOL r0 = f(p, &first, NULL, 1);
    int bad = 0;
    for (int i = 0; i < 50; i++) { DWORD c = 0; BOOL r = f(p, &c, NULL, 1); if (r != r0 || c != first) bad++; }
    EXPECT_EQ(bad, 0);
}
TEST(gap0_frinfo_no_gdi_leak) {
    GetFontResourceInfoW_pfn f = UN(GetFontResourceInfoW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    WCHAR p[MAX_PATH]; if (!arial_path(p, MAX_PATH)) { t_skip("no arial.ttf"); return; }
    DWORD before = gap_gdi_count();
    for (int i = 0; i < 30; i++) { DWORD c = 0; f(p, &c, NULL, 1); }
    EXPECT_EQ(gap_gdi_count(), before);
}
TEST(gap0_frinfo_long_path) {
    GetFontResourceInfoW_pfn f = UN(GetFontResourceInfoW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    WCHAR *p = HeapAlloc(GetProcessHeap(), 0, 40000 * sizeof(WCHAR));
    lstrcpyW(p, L"C:\\");
    for (int i = 3; i < 30000; i++) p[i] = L'a';
    p[30000] = 0;
    DWORD cb = 0;
    CHECK(!f(p, &cb, NULL, 1));
    HeapFree(GetProcessHeap(), 0, p);
}

/* ================= GetStringBitmapA / W ================= */
TEST(gap0_sbm_resolve) {
    CHECK(t_proc_gdi32("GetStringBitmapA") != NULL);
    CHECK(t_proc_gdi32("GetStringBitmapW") != NULL);
    CHECK(t_proc_gdi32full("GetStringBitmapA") != NULL);
}
TEST(gap0_sbm_w_bad_count) {
    GetStringBitmapW_pfn f = UN(GetStringBitmapW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    BYTE buf[256];
    UINT counts[] = { 0, 2, 3, 100, 0xffffffff };
    for (unsigned i = 0; i < sizeof counts / sizeof *counts; i++)
        CHECKF(f(m.dc, L"AB", counts[i], sizeof buf, buf) == 0, "cwc %u", counts[i]);
    gap_mdc_close(&m);
}
TEST(gap0_sbm_w_invalid_dc) {
    GetStringBitmapW_pfn f = UN(GetStringBitmapW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    BYTE buf[64];
    SET_ERR(0);
    CHECK(f(NULL, L"A", 1, sizeof buf, buf) == 0);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(f((HDC)0x1234, L"A", 1, 0, NULL) == 0);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(f((HDC)0xdeadbeef, L"A", 1, 0, buf) == 0);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
}
TEST(gap0_sbm_w_deleted_dc) {
    GetStringBitmapW_pfn f = UN(GetStringBitmapW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    HDC dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    BYTE buf[64];
    SET_ERR(0);
    CHECK(f(dc, L"A", 1, sizeof buf, buf) == 0);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
}
TEST(gap0_sbm_w_size_zero_buffer) {
    GetStringBitmapW_pfn f = UN(GetStringBitmapW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -24);
    CHECK(f(fx.m.dc, L"A", 1, 0, NULL) == 0);
    gap_fx_close(&fx);
}
TEST(gap0_sbm_w_valid_layout) {
    GetStringBitmapW_pfn f = UN(GetStringBitmapW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    const WCHAR *faces[] = { L"Arial", L"Courier New", L"Tahoma" };
    for (int i = 0; i < 3; i++) {
        gap_fx fx; if (!gap_fx_open(&fx, faces[i], -24) || !fx.ok) { gap_fx_close(&fx); continue; }
        BYTE *buf = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 8192);
        DWORD r = f(fx.m.dc, L"A", 1, 8192, buf);
        t_note("%ls: GetStringBitmapW -> %lu", faces[i], r);
        if (r) {
            int w = *(int *)buf, h = *(int *)(buf + 4);
            CHECK(w > 0 && h > 0);
            CHECK(r == 8 + (DWORD)(((w + 7) >> 3) * h));
        }
        HeapFree(GetProcessHeap(), 0, buf);
        gap_fx_close(&fx);
    }
}
TEST(gap0_sbm_w_buffer_too_small) {
    GetStringBitmapW_pfn f = UN(GetStringBitmapW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -24);
    BYTE *buf = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 8192);
    DWORD need = f(fx.m.dc, L"W", 1, 8192, buf);
    if (!need) { HeapFree(GetProcessHeap(), 0, buf); gap_fx_close(&fx); t_skip("not supported for this font"); return; }
    CHECK(f(fx.m.dc, L"W", 1, need - 1, buf) == 0);
    CHECK(f(fx.m.dc, L"W", 1, 8, buf) == 0);
    CHECK(f(fx.m.dc, L"W", 1, 1, buf) == 0);
    EXPECT_EQ(f(fx.m.dc, L"W", 1, need, buf), need);
    HeapFree(GetProcessHeap(), 0, buf);
    gap_fx_close(&fx);
}
TEST(gap0_sbm_w_buffer_not_overrun) {
    GetStringBitmapW_pfn f = UN(GetStringBitmapW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -24);
    BYTE *buf = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 8192);
    DWORD need = f(fx.m.dc, L"M", 1, 8192, buf);
    if (!need) { HeapFree(GetProcessHeap(), 0, buf); gap_fx_close(&fx); t_skip("not supported"); return; }
    memset(buf, 0xEE, 8192);
    DWORD r = f(fx.m.dc, L"M", 1, need, buf);
    EXPECT_EQ(r, need);
    for (DWORD i = need; i < need + 64; i++) CHECK(buf[i] == 0xEE);
    HeapFree(GetProcessHeap(), 0, buf);
    gap_fx_close(&fx);
}
TEST(gap0_sbm_w_deterministic) {
    GetStringBitmapW_pfn f = UN(GetStringBitmapW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -24);
    BYTE *a = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 4096), *b = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 4096);
    DWORD ra = f(fx.m.dc, L"g", 1, 4096, a), rb = f(fx.m.dc, L"g", 1, 4096, b);
    EXPECT_EQ(ra, rb);
    if (ra) CHECK(!memcmp(a, b, ra));
    HeapFree(GetProcessHeap(), 0, a); HeapFree(GetProcessHeap(), 0, b);
    gap_fx_close(&fx);
}
TEST(gap0_sbm_a_bad_lengths) {
    GetStringBitmapA_pfn f = UN(GetStringBitmapA); REQUIRE(f);
    SKIP_WINE("unimplemented");
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    BYTE buf[256];
    CHECK(f(m.dc, "A", 0, sizeof buf, buf) == 0);
    CHECK(f(m.dc, "ABC", 3, sizeof buf, buf) == 0);
    CHECK(f(m.dc, "AB", 2, sizeof buf, buf) == 0);
    CHECK(f(m.dc, "A\0", 2, sizeof buf, buf) == 0);
    CHECK(f(m.dc, "A", 0xffffffff, sizeof buf, buf) == 0);
    CHECK(f(m.dc, "A", 100, sizeof buf, buf) == 0);
    gap_mdc_close(&m);
}
TEST(gap0_sbm_a_invalid_dc) {
    GetStringBitmapA_pfn f = UN(GetStringBitmapA); REQUIRE(f);
    SKIP_WINE("unimplemented");
    BYTE buf[64];
    CHECK(f(NULL, "A", 1, sizeof buf, buf) == 0);
    CHECK(f((HDC)0x1234, "A", 1, sizeof buf, buf) == 0);
    SET_ERR(0);
    f(NULL, "A", 1, sizeof buf, buf);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
}
TEST(gap0_sbm_a_bad_length_no_error) {
    GetStringBitmapA_pfn f = UN(GetStringBitmapA); REQUIRE(f);
    SKIP_WINE("unimplemented");
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    BYTE buf[64]; SET_ERR(0x7777);
    CHECK(f(m.dc, "ABC", 3, sizeof buf, buf) == 0);
    EXPECT_EQ(GetLastError(), 0x7777);
    gap_mdc_close(&m);
}
TEST(gap0_sbm_ab_parity) {
    GetStringBitmapA_pfn fa = UN(GetStringBitmapA); GetStringBitmapW_pfn fw = UN(GetStringBitmapW); REQUIRE(fa); REQUIRE(fw);
    SKIP_WINE("unimplemented");
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -24);
    BYTE *a = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 4096), *b = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 4096);
    const char *cs = "AaWgi.";
    for (const char *c = cs; *c; c++) {
        WCHAR wc = (WCHAR)(unsigned char)*c;
        DWORD ra = fa(fx.m.dc, c, 1, 4096, a), rb = fw(fx.m.dc, &wc, 1, 4096, b);
        CHECKF(ra == rb, "char %c: A=%lu W=%lu", *c, ra, rb);
        if (ra == rb && ra) CHECK(!memcmp(a, b, ra));
    }
    HeapFree(GetProcessHeap(), 0, a); HeapFree(GetProcessHeap(), 0, b);
    gap_fx_close(&fx);
}
TEST(gap0_sbm_non_identity_transform) {
    GetStringBitmapW_pfn f = UN(GetStringBitmapW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -24);
    U(SetMapMode)(fx.m.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(fx.m.dc, 1, 1, NULL); U(SetViewportExtEx)(fx.m.dc, 2, 2, NULL);
    BYTE buf[4096];
    DWORD r = f(fx.m.dc, L"A", 1, sizeof buf, buf);
    t_note("scaled mapping: %lu (0 expected since the page->device transform must be a pure translation)", r);
    gap_fx_close(&fx);
}
TEST(gap0_sbm_stock_fonts) {
    GetStringBitmapW_pfn f = UN(GetStringBitmapW); REQUIRE(f); REQUIRE(U(GetStockObject));
    SKIP_WINE("unimplemented");
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    int ids[] = { SYSTEM_FONT, SYSTEM_FIXED_FONT, OEM_FIXED_FONT, ANSI_VAR_FONT };
    for (int i = 0; i < 4; i++) {
        HGDIOBJ old = U(SelectObject)(m.dc, U(GetStockObject)(ids[i]));
        BYTE buf[4096];
        DWORD r = f(m.dc, L"A", 1, sizeof buf, buf);
        t_note("stock font %d: %lu", ids[i], r);
        if (r) { int w = *(int *)buf, h = *(int *)(buf + 4); CHECK(r == 8 + (DWORD)(((w + 7) >> 3) * h)); }
        U(SelectObject)(m.dc, old);
    }
    gap_mdc_close(&m);
}

/* ================= font association / linking ================= */
TEST(gap0_fassoc_resolve) {
    CHECK(t_proc_gdi32("GetFontAssocStatus") != NULL);
    CHECK(t_proc_gdi32("QueryFontAssocStatus") != NULL);
    CHECK(t_proc_gdi32("FontIsLinked") != NULL);
    CHECK(t_proc_gdi32("AnyLinkedFonts") != NULL);
    CHECK(t_proc_gdi32full("GetFontAssocStatus") != NULL);
    CHECK(t_proc_gdi32full("QueryFontAssocStatus") != NULL);
    CHECK(t_proc_gdi32full("FontIsLinked") != NULL);
    CHECK(t_proc_gdi32full("AnyLinkedFonts") != NULL);
}
TEST(gap0_fassoc_null_dc) {
    GetFontAssocStatus_pfn f = UN(GetFontAssocStatus); REQUIRE(f);
    SET_ERR(0x5151);
    CHECK(f(NULL) == 0);
    EXPECT_EQ(GetLastError(), 0x5151);
}
TEST(gap0_fassoc_bogus_dc) {
    GetFontAssocStatus_pfn f = UN(GetFontAssocStatus); REQUIRE(f);
    ULONG_PTR h[] = { 0x1234, 0xdeadbeef, 0x10000, 0x7fff0000 };
    for (unsigned i = 0; i < sizeof h / sizeof *h; i++) {
        SET_ERR(0);
        CHECKF(f((HDC)h[i]) == 0, "h %llx", (unsigned long long)h[i]);
        EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    }
}
TEST(gap0_fassoc_deleted_dc) {
    GetFontAssocStatus_pfn f = UN(GetFontAssocStatus); REQUIRE(f);
    HDC dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    SET_ERR(0);
    CHECK(f(dc) == 0);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
}
TEST(gap0_fassoc_non_dc_object) {
    GetFontAssocStatus_pfn f = UN(GetFontAssocStatus); REQUIRE(f);
    HBRUSH b = U(CreateSolidBrush)(0);
    SET_ERR(0);
    CHECK(f((HDC)b) == 0);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    U(DeleteObject)(b);
}
TEST(gap0_fassoc_valid_dc_range) {
    GetFontAssocStatus_pfn f = UN(GetFontAssocStatus); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -20);
    DWORD r = f(fx.m.dc);
    t_note("GetFontAssocStatus(Arial) = %lu", r);
    CHECK(r <= 2);
    EXPECT_EQ(f(fx.m.dc), r);
    gap_fx_close(&fx);
}
TEST(gap0_fassoc_default_font_dc) {
    GetFontAssocStatus_pfn f = UN(GetFontAssocStatus); REQUIRE(f);
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    DWORD r = f(m.dc);
    CHECK(r <= 2);
    t_note("default memdc font: %lu", r);
    gap_mdc_close(&m);
}
TEST(gap0_fassoc_symbol_font) {
    GetFontAssocStatus_pfn f = UN(GetFontAssocStatus); REQUIRE(f);
    gap_fx fx; if (!gap_fx_open(&fx, L"Marlett", -20) || !fx.ok) { gap_fx_close(&fx); t_skip("Marlett"); return; }
    DWORD r = f(fx.m.dc);
    CHECK(r <= 2);
    t_note("Marlett: %lu", r);
    gap_fx_close(&fx);
}
TEST(gap0_qfas_stable) {
    QueryFontAssocStatus_pfn f = UN(QueryFontAssocStatus); REQUIRE(f);
    DWORD a = f(), b = f(), c = f();
    EXPECT_EQ(a, b); EXPECT_EQ(b, c);
    t_note("QueryFontAssocStatus = 0x%lx", a);
    SOFT((a & ~7u) == 0, "status is a small bitmask");
}
TEST(gap0_qfas_no_error) {
    QueryFontAssocStatus_pfn f = UN(QueryFontAssocStatus); REQUIRE(f);
    SET_ERR(0x3131);
    f();
    EXPECT_EQ(GetLastError(), 0x3131);
}
TEST(gap0_fontislinked_invalid) {
    FontIsLinked_pfn f = UN(FontIsLinked); REQUIRE(f);
    CHECK(f(NULL) == 0);
    CHECK(f((HDC)0x1234) == 0);
    CHECK(f((HDC)0xdeadbeef) == 0);
    HDC dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    CHECK(f(dc) == 0);
}
TEST(gap0_fontislinked_valid) {
    FontIsLinked_pfn f = UN(FontIsLinked); REQUIRE(f);
    const WCHAR *faces[] = { L"Arial", L"Courier New", L"Times New Roman", L"Tahoma", L"Segoe UI", L"Marlett", L"Symbol" };
    for (unsigned i = 0; i < sizeof faces / sizeof *faces; i++) {
        gap_fx fx; if (!gap_fx_open(&fx, faces[i], -20) || !fx.ok) { gap_fx_close(&fx); continue; }
        BOOL r = f(fx.m.dc);
        CHECKF(r == 0 || r == 1, "%ls returned %d", faces[i], r);
        EXPECT_EQ(f(fx.m.dc), r);
        t_note("%ls linked=%d", faces[i], r);
        gap_fx_close(&fx);
    }
}
TEST(gap0_fontislinked_default_dc) {
    FontIsLinked_pfn f = UN(FontIsLinked); REQUIRE(f);
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    BOOL r = f(m.dc);
    CHECK(r == 0 || r == 1);
    gap_mdc_close(&m);
}
TEST(gap0_fontislinked_no_leak) {
    FontIsLinked_pfn f = UN(FontIsLinked); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -20);
    DWORD before = gap_gdi_count();
    for (int i = 0; i < 300; i++) f(fx.m.dc);
    EXPECT_EQ(gap_gdi_count(), before);
    gap_fx_close(&fx);
}
TEST(gap0_anylinked_stable) {
    AnyLinkedFonts_pfn f = UN(AnyLinkedFonts); REQUIRE(f);
    BOOL a = f(), b = f();
    CHECK(a == 0 || a == 1);
    EXPECT_EQ(a, b);
    t_note("AnyLinkedFonts = %d", a);
}
TEST(gap0_anylinked_implied_by_fontislinked) {
    AnyLinkedFonts_pfn any = UN(AnyLinkedFonts); FontIsLinked_pfn lk = UN(FontIsLinked); REQUIRE(any); REQUIRE(lk);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -20);
    BOOL l = lk(fx.m.dc), a = any();
    if (l) SOFT(a, "a linked font implies a link exists");
    gap_fx_close(&fx);
}
TEST(gap0_anylinked_no_error) {
    AnyLinkedFonts_pfn f = UN(AnyLinkedFonts); REQUIRE(f);
    SET_ERR(0x2323);
    f();
    EXPECT_EQ(GetLastError(), 0x2323);
}

/* ---- EUDC ---- */
TEST(gap0_eudc_resolve) {
    /* EnableEUDC toggles session-wide state: resolved only, never called. */
    CHECK(t_proc_gdi32("EnableEUDC") != NULL);
    CHECK(t_proc_gdi32("EudcLoadLinkW") != NULL);
    CHECK(t_proc_gdi32("EudcUnloadLinkW") != NULL);
    CHECK(t_proc_gdi32full("EudcLoadLinkW") != NULL);
    CHECK(t_proc_gdi32full("EudcUnloadLinkW") != NULL);
    CHECK(t_proc_gdi32full("EnableEUDC") == NULL);
}
TEST(gap0_eudc_load_empty_path) {
    EudcLoadLinkW_pfn f = UN(EudcLoadLinkW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    SET_ERR(0);
    CHECK(f(NULL, L"", 0, 0) == 0);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(f(L"Arial", L"", 0, 0) == 0);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
}
TEST(gap0_eudc_load_path_too_long) {
    EudcLoadLinkW_pfn f = UN(EudcLoadLinkW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    int lens[] = { 292, 293, 400, 1000, 5000 };
    for (unsigned i = 0; i < sizeof lens / sizeof *lens; i++) {
        WCHAR *p = HeapAlloc(GetProcessHeap(), 0, (lens[i] + 1) * sizeof(WCHAR));
        for (int k = 0; k < lens[i]; k++) p[k] = L'a';
        p[lens[i]] = 0;
        SET_ERR(0);
        BOOL r = f(NULL, p, 0, 0);
        DWORD e = GetLastError();
        CHECKF(r == 0, "len %d", lens[i]);
        CHECKF(e == ERROR_INVALID_PARAMETER, "len %d err %lu", lens[i], e);
        HeapFree(GetProcessHeap(), 0, p);
    }
}
TEST(gap0_eudc_load_face_too_long) {
    EudcLoadLinkW_pfn f = UN(EudcLoadLinkW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    WCHAR face[80];
    int lens[] = { 33, 34, 50, 79 };
    for (unsigned i = 0; i < sizeof lens / sizeof *lens; i++) {
        for (int k = 0; k < lens[i]; k++) face[k] = L'F';
        face[lens[i]] = 0;
        SET_ERR(0);
        BOOL r = f(face, L"x.tte", 0, 0);
        DWORD e = GetLastError();
        CHECKF(r == 0, "face len %d", lens[i]);
        CHECKF(e == ERROR_INVALID_PARAMETER, "face len %d err %lu", lens[i], e);
    }
}
TEST(gap0_eudc_unload_empty_path) {
    EudcUnloadLinkW_pfn f = UN(EudcUnloadLinkW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    SET_ERR(0);
    CHECK(f(NULL, L"") == 0);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(f(L"Arial", L"") == 0);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
}
TEST(gap0_eudc_unload_path_too_long) {
    EudcUnloadLinkW_pfn f = UN(EudcUnloadLinkW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    WCHAR *p = HeapAlloc(GetProcessHeap(), 0, 3001 * sizeof(WCHAR));
    for (int k = 0; k < 3000; k++) p[k] = L'b';
    p[3000] = 0;
    SET_ERR(0);
    CHECK(f(NULL, p) == 0);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    p[292] = 0;
    SET_ERR(0);
    CHECK(f(NULL, p) == 0);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    HeapFree(GetProcessHeap(), 0, p);
}
TEST(gap0_eudc_unload_face_too_long) {
    EudcUnloadLinkW_pfn f = UN(EudcUnloadLinkW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    WCHAR face[64];
    for (int k = 0; k < 40; k++) face[k] = L'G';
    face[40] = 0;
    SET_ERR(0);
    CHECK(f(face, L"y.tte") == 0);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
}
TEST(gap0_eudc_unload_nonexistent_link) {
    EudcUnloadLinkW_pfn f = UN(EudcUnloadLinkW); REQUIRE(f);
    SKIP_WINE("unimplemented");
    /* nothing was ever linked with this path: the kernel has nothing to remove */
    BOOL r = f(NULL, L"C:\\gap0_never_linked_eudc.tte");
    t_note("unload of a link that does not exist: %d err %lu", r, GetLastError());
    SOFT(!r, "unloading an unknown link fails");
}
TEST(gap0_eudc_does_not_change_link_state) {
    AnyLinkedFonts_pfn any = UN(AnyLinkedFonts); EudcLoadLinkW_pfn ld = UN(EudcLoadLinkW); REQUIRE(any); REQUIRE(ld);
    SKIP_WINE("unimplemented");
    BOOL before = any();
    ld(NULL, L"", 0, 0);
    EXPECT_EQ(any(), before);
}

/* ---- SetFontEnumeration ---- */
TEST(gap0_sfe_resolve) {
    CHECK(t_proc_gdi32("SetFontEnumeration") != NULL);
    CHECK(t_proc_gdi32full("SetFontEnumeration") != NULL);
}
TEST(gap0_sfe_not_supported_error) {
    SetFontEnumeration_pfn f = UN(SetFontEnumeration); REQUIRE(f);
    SKIP_WINE("unimplemented");
    SET_ERR(0);
    DWORD r = f(0);
    t_note("SetFontEnumeration(0) = 0x%lx err %lu", r, GetLastError());
    EXPECT_EQ(GetLastError(), ERROR_NOT_SUPPORTED);
}
TEST(gap0_sfe_argument_ignored) {
    SetFontEnumeration_pfn f = UN(SetFontEnumeration); REQUIRE(f);
    SKIP_WINE("unimplemented");
    SET_ERR(0);
    DWORD base = f(0);
    DWORD args[] = { 1, 2, 0x10, 0xFF, 0x80000000, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof args / sizeof *args; i++) {
        SET_ERR(0);
        DWORD r = f(args[i]);
        CHECKF(r == base, "arg 0x%lx returned 0x%lx, base 0x%lx", args[i], r, base);
        CHECKF(GetLastError() == ERROR_NOT_SUPPORTED, "arg 0x%lx err %lu", args[i], GetLastError());
    }
}
TEST(gap0_sfe_repeat_stable) {
    SetFontEnumeration_pfn f = UN(SetFontEnumeration); REQUIRE(f);
    SKIP_WINE("unimplemented");
    DWORD a = f(0); int bad = 0;
    for (int i = 0; i < 200; i++) if (f(i) != a) bad++;
    EXPECT_EQ(bad, 0);
}
TEST(gap0_sfe_does_not_change_font_enum) {
    SetFontEnumeration_pfn f = UN(SetFontEnumeration); REQUIRE(f); REQUIRE(U(EnumFontFamiliesExW)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    SKIP_WINE("unimplemented");
    HDC dc = U(GetDC)(NULL); if (!dc) { t_skip("no screen dc"); return; }
    struct cnt { int n; } c1 = { 0 }, c2 = { 0 };
    int (CALLBACK *cb)(const LOGFONTW *, const TEXTMETRICW *, DWORD, LPARAM) = NULL;
    (void)cb;
    /* count families before and after; the call must be a no-op */
    LOGFONTW lf; memset(&lf, 0, sizeof lf); lf.lfCharSet = DEFAULT_CHARSET;
    extern int CALLBACK gap_count_cb(const LOGFONTW *, const TEXTMETRICW *, DWORD, LPARAM);
    U(EnumFontFamiliesExW)(dc, &lf, gap_count_cb, (LPARAM)&c1, 0);
    f(0xFFFFFFFF);
    U(EnumFontFamiliesExW)(dc, &lf, gap_count_cb, (LPARAM)&c2, 0);
    EXPECT_EQ(c1.n, c2.n);
    CHECK(c1.n > 0);
    U(ReleaseDC)(NULL, dc);
}
int CALLBACK gap_count_cb(const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD ft, LPARAM p) { (*(int *)p)++; return 1; }

/* ---- font file helpers ---- */
TEST(gap0_ffile_resolve) {
    CHECK(t_proc_gdi32("GetFontFileData") != NULL);
    CHECK(t_proc_gdi32("GetFontFileInfo") != NULL);
    CHECK(t_proc_gdi32full("GetFontFileData") != NULL);
    CHECK(t_proc_gdi32full("GetFontFileInfo") != NULL);
    CHECK(t_proc_gdi32full("GetFontDataImpl") != NULL);
    CHECK(t_proc_gdi32("GetFontDataImpl") == NULL);
}
TEST(gap0_ffd_bogus_id) {
    GetFontFileData_pfn f = UN(GetFontFileData); REQUIRE(f);
    SKIP_WINE("unimplemented");
    BYTE buf[64];
    UINT ids[] = { 0, 1, 0x7fffffff, 0xFFFFFF00, 0xdeadbeef, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof ids / sizeof *ids; i++) {
        memset(buf, 0x5c, sizeof buf);
        SET_ERR(0);
        BOOL r = f(ids[i], 0, 0, buf, sizeof buf);
        DWORD e = GetLastError();
        if (r) { t_note("id 0x%x unexpectedly valid", ids[i]); continue; }
        CHECKF(e == ERROR_INVALID_PARAMETER, "id 0x%x err %lu", ids[i], e);
        CHECK(buf[0] == 0x5c);
    }
}
TEST(gap0_ffd_bogus_index_and_offset) {
    GetFontFileData_pfn f = UN(GetFontFileData); REQUIRE(f);
    SKIP_WINE("unimplemented");
    BYTE buf[16];
    SET_ERR(0);
    CHECK(!f(0xFFFFFF01, 0xFFFFFFFF, 0, buf, sizeof buf));
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!f(0xFFFFFF01, 0, 0xFFFFFFFFFFFFFFF0ull, buf, sizeof buf));
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!f(0xFFFFFF01, 0, 0, buf, 0));
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
}
TEST(gap0_ffd_error_cleared_on_nothing) {
    GetFontFileData_pfn f = UN(GetFontFileData); REQUIRE(f);
    SKIP_WINE("unimplemented");
    BYTE b[4];
    for (int i = 0; i < 50; i++) CHECK(!f(0xFFFFF000 + i, 0, 0, b, 4));
}
TEST(gap0_ffi_bogus_id) {
    GetFontFileInfo_pfn f = UN(GetFontFileInfo); REQUIRE(f);
    SKIP_WINE("unimplemented");
    BYTE buf[0x200]; SIZE_T need = 0x1234;
    UINT ids[] = { 0, 0x7fffffff, 0xFFFFFF00, 0xdeadbeef };
    for (unsigned i = 0; i < sizeof ids / sizeof *ids; i++) {
        memset(buf, 0x5c, sizeof buf);
        SET_ERR(0);
        BOOL r = f(ids[i], 0, buf, sizeof buf, &need);
        DWORD e = GetLastError();
        if (r) { t_note("id 0x%x unexpectedly valid", ids[i]); continue; }
        CHECKF(e != 0, "id 0x%x error code set", ids[i]);
        t_note("GetFontFileInfo bogus id 0x%x: err %lu", ids[i], e);
    }
}
TEST(gap0_ffi_bogus_index) {
    GetFontFileInfo_pfn f = UN(GetFontFileInfo); REQUIRE(f);
    SKIP_WINE("unimplemented");
    BYTE buf[0x200]; SIZE_T need = 0;
    SET_ERR(0);
    CHECK(!f(0xFFFFFF01, 0xFFFFFFFF, buf, sizeof buf, &need));
    CHECK(GetLastError() != 0);
}
TEST(gap0_fdi_all_bits_size_error) {
    GetFontDataImpl_pfn f = UN(GetFontDataImpl); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -20);
    BYTE b[8];
    CHECK(f(fx.m.dc, TAG_HEAD, 0, b, 0xFFFFFFFF) == GDI_ERROR);
    CHECK(f(fx.m.dc, 0, 0, NULL, 0xFFFFFFFF) == GDI_ERROR);
    gap_fx_close(&fx);
}
TEST(gap0_fdi_offset_overflow) {
    GetFontDataImpl_pfn f = UN(GetFontDataImpl); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -20);
    BYTE b[16];
    CHECK(f(fx.m.dc, TAG_HEAD, 0xFFFFFFF8, b, 16) == GDI_ERROR);
    CHECK(f(fx.m.dc, TAG_HEAD, 0xFFFFFFFF, b, 2) == GDI_ERROR);
    gap_fx_close(&fx);
}
TEST(gap0_fdi_invalid_dc) {
    GetFontDataImpl_pfn f = UN(GetFontDataImpl); REQUIRE(f);
    BYTE b[8];
    CHECK(f(NULL, TAG_HEAD, 0, b, 8) == GDI_ERROR);
    CHECK(f((HDC)0x1234, TAG_HEAD, 0, NULL, 0) == GDI_ERROR);
    CHECK(f((HDC)0xdeadbeef, 0, 0, NULL, 0) == GDI_ERROR);
}
TEST(gap0_fdi_head_table) {
    GetFontDataImpl_pfn f = UN(GetFontDataImpl); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -20);
    DWORD sz = f(fx.m.dc, TAG_HEAD, 0, NULL, 0);
    EXPECT_EQ(sz, 54);
    BYTE head[54]; memset(head, 0, sizeof head);
    EXPECT_EQ(f(fx.m.dc, TAG_HEAD, 0, head, sizeof head), 54);
    CHECK(head[12] == 0x5F && head[13] == 0x0F && head[14] == 0x3C && head[15] == 0xF5);   /* magicNumber */
    unsigned upem = (head[18] << 8) | head[19];
    SOFT(upem == 2048, "Arial unitsPerEm");
    gap_fx_close(&fx);
}
TEST(gap0_fdi_matches_getfontdata) {
    GetFontDataImpl_pfn f = UN(GetFontDataImpl); REQUIRE(f); REQUIRE(U(GetFontData));
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -20);
    DWORD tags[] = { TAG_HEAD, TAG_CMAP, TAG_NAME, 0 };
    for (int i = 0; i < 4; i++) {
        DWORD a = f(fx.m.dc, tags[i], 0, NULL, 0), b = U(GetFontData)(fx.m.dc, tags[i], 0, NULL, 0);
        CHECKF(a == b, "tag %lx: impl %lu public %lu", tags[i], a, b);
    }
    BYTE x[64], y[64];
    EXPECT_EQ(f(fx.m.dc, TAG_HEAD, 4, x, 32), 32);
    EXPECT_EQ(U(GetFontData)(fx.m.dc, TAG_HEAD, 4, y, 32), 32);
    CHECK(!memcmp(x, y, 32));
    gap_fx_close(&fx);
}
TEST(gap0_fdi_missing_table) {
    GetFontDataImpl_pfn f = UN(GetFontDataImpl); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -20);
    CHECK(f(fx.m.dc, 0x41424344, 0, NULL, 0) == GDI_ERROR);
    gap_fx_close(&fx);
}
TEST(gap0_fdi_offset_beyond_table) {
    GetFontDataImpl_pfn f = UN(GetFontDataImpl); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -20);
    BYTE b[8];
    CHECK(f(fx.m.dc, TAG_HEAD, 54, b, 4) == GDI_ERROR);
    CHECK(f(fx.m.dc, TAG_HEAD, 1000, b, 4) == GDI_ERROR);
    CHECK(f(fx.m.dc, TAG_HEAD, 50, b, 8) == GDI_ERROR);
    EXPECT_EQ(f(fx.m.dc, TAG_HEAD, 50, b, 4), 4);
    gap_fx_close(&fx);
}
TEST(gap0_fdi_whole_file_size) {
    GetFontDataImpl_pfn f = UN(GetFontDataImpl); REQUIRE(f);
    gap_fx fx; FX_OPEN_OR_SKIP(fx, L"Arial", -20);
    DWORD sz = f(fx.m.dc, 0, 0, NULL, 0);
    CHECK(sz != GDI_ERROR && sz > 1000);
    BYTE hdr[4];
    CHECK(f(fx.m.dc, 0, 0, hdr, 4) == 4);
    SOFT((hdr[0] == 0 && hdr[1] == 1 && hdr[2] == 0 && hdr[3] == 0) || !memcmp(hdr, "true", 4) || !memcmp(hdr, "ttcf", 4) || !memcmp(hdr, "OTTO", 4), "sfnt signature");
    WCHAR p[MAX_PATH];
    if (arial_path(p, MAX_PATH)) {
        WIN32_FILE_ATTRIBUTE_DATA fa;
        if (GetFileAttributesExW(p, GetFileExInfoStandard, &fa) && fa.nFileSizeHigh == 0) SOFT(fa.nFileSizeLow == sz, "whole-file size equals arial.ttf");
    }
    gap_fx_close(&fx);
}
TEST(gap0_fdi_bitmap_font) {
    GetFontDataImpl_pfn f = UN(GetFontDataImpl); REQUIRE(f); REQUIRE(U(GetStockObject));
    gap_mdc m; if (!gap_mdc_open(&m, 8, 8)) { t_skip("no memdc"); return; }
    HGDIOBJ old = U(SelectObject)(m.dc, U(GetStockObject)(SYSTEM_FIXED_FONT));
    DWORD r = f(m.dc, TAG_HEAD, 0, NULL, 0);
    SOFT(r == GDI_ERROR, "raster fonts have no sfnt tables");
    U(SelectObject)(m.dc, old);
    gap_mdc_close(&m);
}

/* ---- session fonts ---- */
TEST(gap0_psf_resolve) {
    CHECK(t_proc_gdi32("GetProcessSessionFonts") != NULL);
    CHECK(t_proc_gdi32full("GetProcessSessionFonts") != NULL);
    CHECK(t_proc_gdi32("UnloadNetworkFonts") != NULL);
    CHECK(t_proc_gdi32full("UnloadNetworkFonts") != NULL);
}
TEST(gap0_psf_count_query) {
    GetProcessSessionFonts_pfn f = UN(GetProcessSessionFonts); REQUIRE(f);
    SKIP_WINE("unimplemented");
    UINT nh = 0, nc = 0;
    BOOL r = f(GetCurrentProcess(), NULL, &nh, NULL, &nc);
    t_note("GetProcessSessionFonts: ret %d handles %u chars %u err %lu", r, nh, nc, GetLastError());
    CHECK(r == 0 || r == 1);
}
TEST(gap0_psf_repeat_stable) {
    GetProcessSessionFonts_pfn f = UN(GetProcessSessionFonts); REQUIRE(f);
    SKIP_WINE("unimplemented");
    UINT h0 = 0, c0 = 0;
    BOOL r0 = f(GetCurrentProcess(), NULL, &h0, NULL, &c0);
    for (int i = 0; i < 20; i++) {
        UINT h = 0, c = 0;
        BOOL r = f(GetCurrentProcess(), NULL, &h, NULL, &c);
        CHECK(r == r0); CHECK(h == h0); CHECK(c == c0);
    }
}
TEST(gap0_psf_no_gdi_leak) {
    GetProcessSessionFonts_pfn f = UN(GetProcessSessionFonts); REQUIRE(f);
    SKIP_WINE("unimplemented");
    DWORD before = gap_gdi_count();
    for (int i = 0; i < 50; i++) { UINT h = 0, c = 0; f(GetCurrentProcess(), NULL, &h, NULL, &c); }
    EXPECT_EQ(gap_gdi_count(), before);
}
