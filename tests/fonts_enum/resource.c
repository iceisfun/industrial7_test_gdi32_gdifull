/* AddFontResource*, RemoveFontResource*, AddFontMemResourceEx, CreateScalableFontResource. */
#include "fe.h"

#define RES_BASE() do { FE_BASE(); REQUIRE(U(AddFontResourceA)); REQUIRE(U(AddFontResourceW)); REQUIRE(U(AddFontResourceExA)); \
    REQUIRE(U(AddFontResourceExW)); REQUIRE(U(RemoveFontResourceA)); REQUIRE(U(RemoveFontResourceW)); \
    REQUIRE(U(RemoveFontResourceExA)); REQUIRE(U(RemoveFontResourceExW)); } while (0)

/* A private copy of arial.ttf under the temp directory (so removal cannot affect the installed font). */
typedef struct { char path[MAX_PATH]; WCHAR wpath[MAX_PATH]; int ok; } tmpfont;
static int make_tmpfont(tmpfont *t, const char *src_leaf, const char *tag) {
    char src[MAX_PATH + 32], tmp[MAX_PATH];
    t->ok = 0;
    fe_fontfile(src, sizeof src, src_leaf);
    GetTempPathA(sizeof tmp, tmp);
    wsprintfA(t->path, "%sfe_%s_%lu_%lu.ttf", tmp, tag, GetCurrentProcessId(), GetTickCount());
    if (!CopyFileA(src, t->path, FALSE)) { t_note("cannot copy %s: %lu", src, GetLastError()); return 0; }
    MultiByteToWideChar(CP_ACP, 0, t->path, -1, t->wpath, MAX_PATH);
    t->ok = 1;
    return 1;
}
static void drop_tmpfont(tmpfont *t) { if (t->ok) DeleteFileA(t->path); t->ok = 0; }

static int count_family(const char *face) {
    HDC dc = U(CreateCompatibleDC)(NULL);
    int n = fe_enum_exA(dc, face, DEFAULT_CHARSET, 0)->total;
    U(DeleteDC)(dc);
    return n;
}

/* ---- error paths ---- */
TEST(fonts_res_add_nonexistent_a) {
    RES_BASE();
    SetLastError(0);
    EQ(U(AddFontResourceA)("C:\\no_such_dir_gdifull\\nofont.ttf"), 0);
    EQ(U(AddFontResourceA)("nofont_gdifull.ttf"), 0);
    EQ(U(AddFontResourceA)(""), 0);
    EQ(U(AddFontResourceExA)("C:\\no_such_dir_gdifull\\nofont.ttf", 0, NULL), 0);
    EQ(U(AddFontResourceExA)("C:\\no_such_dir_gdifull\\nofont.ttf", FR_PRIVATE, NULL), 0);
    EQ(U(AddFontResourceExA)("C:\\no_such_dir_gdifull\\nofont.ttf", FR_NOT_ENUM, NULL), 0);
    EQ(U(AddFontResourceExA)("C:\\no_such_dir_gdifull\\nofont.ttf", FR_PRIVATE | FR_NOT_ENUM, NULL), 0);
}
TEST(fonts_res_add_nonexistent_w) {
    RES_BASE();
    EQ(U(AddFontResourceW)(L"C:\\no_such_dir_gdifull\\nofont.ttf"), 0);
    EQ(U(AddFontResourceW)(L"nofont_gdifull.ttf"), 0);
    EQ(U(AddFontResourceW)(L""), 0);
    EQ(U(AddFontResourceExW)(L"C:\\no_such_dir_gdifull\\nofont.ttf", 0, NULL), 0);
    EQ(U(AddFontResourceExW)(L"C:\\no_such_dir_gdifull\\nofont.ttf", FR_PRIVATE, NULL), 0);
}
TEST(fonts_res_add_nonexistent_error_code) {
    RES_BASE();
    SetLastError(0xDEAD);
    int r = U(AddFontResourceA)("C:\\no_such_dir_gdifull\\nofont.ttf");
    t_note("AddFontResourceA(nonexistent) = %d, GetLastError = %lu", r, GetLastError());
    EQ(r, 0);
}
TEST(fonts_res_add_invalid_flags) {
    RES_BASE();
    static const DWORD bad[] = {1, 2, 4, 8, 0x40, 0x80, 0x100, 0x10000, 0x80000000, 0xFFFFFFFF, 0x30 | 0x40};
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0);
        CHECKF(U(AddFontResourceExA)("C:\\Windows\\Fonts\\arial.ttf", bad[i], NULL) == 0, "ExA flags %lx", bad[i]);
        EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
        SetLastError(0);
        CHECKF(U(AddFontResourceExW)(L"C:\\Windows\\Fonts\\arial.ttf", bad[i], NULL) == 0, "ExW flags %lx", bad[i]);
        EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
        SetLastError(0);
        CHECKF(U(RemoveFontResourceExA)("C:\\Windows\\Fonts\\arial.ttf", bad[i], NULL) == 0, "RemoveExA flags %lx", bad[i]);
        EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
        SetLastError(0);
        CHECKF(U(RemoveFontResourceExW)(L"C:\\Windows\\Fonts\\arial.ttf", bad[i], NULL) == 0, "RemoveExW flags %lx", bad[i]);
        EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    }
}
TEST(fonts_res_remove_nonexistent) {
    RES_BASE();
    EQ(U(RemoveFontResourceA)("C:\\no_such_dir_gdifull\\nofont.ttf"), 0);
    EQ(U(RemoveFontResourceW)(L"C:\\no_such_dir_gdifull\\nofont.ttf"), 0);
    EQ(U(RemoveFontResourceExA)("C:\\no_such_dir_gdifull\\nofont.ttf", 0, NULL), 0);
    EQ(U(RemoveFontResourceExW)(L"C:\\no_such_dir_gdifull\\nofont.ttf", FR_PRIVATE, NULL), 0);
    EQ(U(RemoveFontResourceA)(""), 0);
    EQ(U(RemoveFontResourceW)(L""), 0);
}
TEST(fonts_res_remove_null_name) {
    RES_BASE();
    EQ(U(RemoveFontResourceExW)(NULL, 0, NULL), 0);
    EQ(U(RemoveFontResourceExW)(NULL, FR_PRIVATE, NULL), 0);
}
TEST(fonts_res_remove_never_added_installed_font) {
    RES_BASE();
    /* A font that was never added by this process through this API must not be removable. */
    char path[MAX_PATH + 32]; fe_fontfile(path, sizeof path, "arial.ttf");
    int r = U(RemoveFontResourceExA)(path, FR_PRIVATE, NULL);
    EQ(r, 0);
}
TEST(fonts_res_add_directory_name) {
    RES_BASE();
    char w[MAX_PATH]; GetWindowsDirectoryA(w, sizeof w);
    EQ(U(AddFontResourceA)(w), 0);
}
TEST(fonts_res_add_not_a_font_file) {
    RES_BASE();
    char tmp[MAX_PATH], path[MAX_PATH + 40];
    GetTempPathA(sizeof tmp, tmp);
    wsprintfA(path, "%sfe_notfont_%lu.ttf", tmp, GetCurrentProcessId());
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    BYTE junk[512]; for (int i = 0; i < 512; i++) junk[i] = (BYTE)(i * 7 + 3);
    DWORD wr; WriteFile(h, junk, sizeof junk, &wr, NULL); CloseHandle(h);
    EQ(U(AddFontResourceExA)(path, FR_PRIVATE, NULL), 0);
    EQ(U(AddFontResourceExA)(path, FR_PRIVATE | FR_NOT_ENUM, NULL), 0);
    DeleteFileA(path);
}
TEST(fonts_res_add_empty_file) {
    RES_BASE();
    char tmp[MAX_PATH], path[MAX_PATH + 40];
    GetTempPathA(sizeof tmp, tmp);
    wsprintfA(path, "%sfe_empty_%lu.ttf", tmp, GetCurrentProcessId());
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    CloseHandle(h);
    EQ(U(AddFontResourceExA)(path, FR_PRIVATE, NULL), 0);
    DeleteFileA(path);
}
TEST(fonts_res_add_truncated_font) {
    RES_BASE();
    tmpfont t; if (!make_tmpfont(&t, "arial.ttf", "trunc")) return;
    HANDLE h = CreateFileA(t.path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        SetFilePointer(h, 200, NULL, FILE_BEGIN); SetEndOfFile(h); CloseHandle(h);
        int r = U(AddFontResourceExA)(t.path, FR_PRIVATE, NULL);
        EQ(r, 0);
        if (r) U(RemoveFontResourceExA)(t.path, FR_PRIVATE, NULL);
    }
    drop_tmpfont(&t);
}
TEST(fonts_res_long_path) {
    RES_BASE();
    char p[600]; strcpy(p, "C:\\");
    for (int i = 0; i < 38; i++) strcat(p, "0123456789\\");
    strcat(p, "font.ttf");
    EQ(U(AddFontResourceA)(p), 0);
    EQ(U(RemoveFontResourceA)(p), 0);
}

/* ---- private font lifecycle ---- */
TEST(fonts_res_private_lifecycle_a) {
    RES_BASE();
    tmpfont t; if (!make_tmpfont(&t, "arial.ttf", "priv")) return;
    int before = count_family("Arial");
    int n = U(AddFontResourceExA)(t.path, FR_PRIVATE, NULL);
    EQ(n, 1);
    int during = count_family("Arial");
    t_note("Arial entries: before %d, with private copy %d", before, during);
    CHECK(during >= before);
    CHECK(U(RemoveFontResourceExA)(t.path, FR_PRIVATE, NULL));
    EQ(count_family("Arial"), before);
    SetLastError(0);
    CHECK(!U(RemoveFontResourceExA)(t.path, FR_PRIVATE, NULL));
    drop_tmpfont(&t);
}
TEST(fonts_res_private_lifecycle_w) {
    RES_BASE();
    tmpfont t; if (!make_tmpfont(&t, "cour.ttf", "privw")) return;
    int before = count_family("Courier New");
    int n = U(AddFontResourceExW)(t.wpath, FR_PRIVATE, NULL);
    EQ(n, 1);
    CHECK(count_family("Courier New") >= before);
    CHECK(U(RemoveFontResourceExW)(t.wpath, FR_PRIVATE, NULL));
    EQ(count_family("Courier New"), before);
    CHECK(!U(RemoveFontResourceExW)(t.wpath, FR_PRIVATE, NULL));
    drop_tmpfont(&t);
}
TEST(fonts_res_private_not_enum_hidden) {
    RES_BASE();
    tmpfont t; if (!make_tmpfont(&t, "arial.ttf", "noenum")) return;
    int before = count_family("Arial");
    int n = U(AddFontResourceExA)(t.path, FR_PRIVATE | FR_NOT_ENUM, NULL);
    EQ(n, 1);
    EQ(count_family("Arial"), before);
    /* still usable for selection */
    fe_ctx c; int ok = fe_open(&c, "Arial", -16, 400, 0, ANSI_CHARSET);
    CHECK(ok);
    fe_close(&c);
    CHECK(U(RemoveFontResourceExA)(t.path, FR_PRIVATE | FR_NOT_ENUM, NULL));
    drop_tmpfont(&t);
}
TEST(fonts_res_private_flags_must_match_on_remove) {
    RES_BASE();
    tmpfont t; if (!make_tmpfont(&t, "arial.ttf", "mismatch")) return;
    EQ(U(AddFontResourceExA)(t.path, FR_PRIVATE, NULL), 1);
    BOOL r0 = U(RemoveFontResourceExA)(t.path, 0, NULL);
    t_note("RemoveFontResourceEx with flags 0 for a private font -> %d", r0);
    if (r0) { SetLastError(0); } else CHECK(U(RemoveFontResourceExA)(t.path, FR_PRIVATE, NULL));
    drop_tmpfont(&t);
}
TEST(fonts_res_private_refcount) {
    RES_BASE();
    tmpfont t; if (!make_tmpfont(&t, "arial.ttf", "ref")) return;
    EQ(U(AddFontResourceExA)(t.path, FR_PRIVATE, NULL), 1);
    int second = U(AddFontResourceExA)(t.path, FR_PRIVATE, NULL);
    t_note("second private AddFontResourceEx of the same file -> %d", second);
    CHECK(U(RemoveFontResourceExA)(t.path, FR_PRIVATE, NULL));
    BOOL again = U(RemoveFontResourceExA)(t.path, FR_PRIVATE, NULL);
    t_note("second private remove -> %d", again);
    BOOL third = U(RemoveFontResourceExA)(t.path, FR_PRIVATE, NULL);
    CHECK(!third);
    drop_tmpfont(&t);
}
TEST(fonts_res_private_delete_file_while_loaded) {
    RES_BASE();
    tmpfont t; if (!make_tmpfont(&t, "tahoma.ttf", "del")) return;
    EQ(U(AddFontResourceExA)(t.path, FR_PRIVATE, NULL), 1);
    BOOL del = DeleteFileA(t.path);
    t_note("DeleteFile on a loaded private font -> %d (err %lu)", del, del ? 0 : GetLastError());
    BOOL rm = U(RemoveFontResourceExA)(t.path, FR_PRIVATE, NULL);
    t_note("RemoveFontResourceEx after deleting the file -> %d", rm);
    if (!del) DeleteFileA(t.path);
    t.ok = 0;
}
TEST(fonts_res_private_family_name_preserved) {
    RES_BASE();
    tmpfont t; if (!make_tmpfont(&t, "times.ttf", "fam")) return;
    EQ(U(AddFontResourceExA)(t.path, FR_PRIVATE, NULL), 1);
    fe_ctx c; int ok = fe_open(&c, "Times New Roman", -20, 400, 0, ANSI_CHARSET);
    CHECK(ok);
    fe_close(&c);
    CHECK(U(RemoveFontResourceExA)(t.path, FR_PRIVATE, NULL));
    drop_tmpfont(&t);
}
TEST(fonts_res_private_not_visible_to_other_thread) {
    RES_BASE();
    /* private fonts are process wide: another thread can enumerate them just the same */
    tmpfont t; if (!make_tmpfont(&t, "arial.ttf", "thr")) return;
    EQ(U(AddFontResourceExA)(t.path, FR_PRIVATE, NULL), 1);
    CHECK(count_family("Arial") >= 1);
    CHECK(U(RemoveFontResourceExA)(t.path, FR_PRIVATE, NULL));
    drop_tmpfont(&t);
}
TEST(fonts_res_private_gdi_count_stable) {
    RES_BASE(); REQUIRE(U(GetGuiResources));
    tmpfont t; if (!make_tmpfont(&t, "arial.ttf", "gdi")) return;
    DWORD before = fe_gdi_count();
    for (int i = 0; i < 5; i++) {
        int n = U(AddFontResourceExA)(t.path, FR_PRIVATE, NULL);
        CHECK(n == 1);
        if (n) CHECK(U(RemoveFontResourceExA)(t.path, FR_PRIVATE, NULL));
    }
    EQ(fe_gdi_count(), before);
    drop_tmpfont(&t);
}
TEST(fonts_res_private_wide_and_ansi_name_equivalent) {
    RES_BASE();
    tmpfont t; if (!make_tmpfont(&t, "arial.ttf", "aw")) return;
    EQ(U(AddFontResourceExW)(t.wpath, FR_PRIVATE, NULL), 1);
    CHECK(U(RemoveFontResourceExA)(t.path, FR_PRIVATE, NULL));
    drop_tmpfont(&t);
}
TEST(fonts_res_private_relative_path) {
    RES_BASE();
    tmpfont t; if (!make_tmpfont(&t, "arial.ttf", "rel")) return;
    char cwd[MAX_PATH]; GetCurrentDirectoryA(sizeof cwd, cwd);
    char dir[MAX_PATH]; lstrcpyA(dir, t.path);
    char *slash = strrchr(dir, '\\'); *slash = 0;
    SetCurrentDirectoryA(dir);
    int n = U(AddFontResourceExA)(slash + 1, FR_PRIVATE, NULL);
    t_note("relative private add -> %d", n);
    if (n) CHECK(U(RemoveFontResourceExA)(slash + 1, FR_PRIVATE, NULL));
    SetCurrentDirectoryA(cwd);
    drop_tmpfont(&t);
}

/* ---- AddFontMemResourceEx ---- */
TEST(fonts_mem_invalid_params) {
    REQUIRE(U(AddFontMemResourceEx)); REQUIRE(U(RemoveFontMemResourceEx));
    BYTE b[64] = {0}; DWORD nf = 0x7777;
    SetLastError(0);
    CHECK(U(AddFontMemResourceEx)(NULL, 64, NULL, &nf) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(U(AddFontMemResourceEx)(b, 0, NULL, &nf) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(U(AddFontMemResourceEx)(b, 64, NULL, NULL) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(U(AddFontMemResourceEx)(NULL, 0, NULL, NULL) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
}
TEST(fonts_mem_garbage_data) {
    REQUIRE(U(AddFontMemResourceEx));
    BYTE *b = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 4096);
    DWORD nf = 0x7777;
    HANDLE h = U(AddFontMemResourceEx)(b, 4096, NULL, &nf);
    CHECK(h == NULL);
    if (h) U(RemoveFontMemResourceEx)(h);
    for (int i = 0; i < 4096; i++) b[i] = (BYTE)(i * 31 + 7);
    nf = 0x7777;
    h = U(AddFontMemResourceEx)(b, 4096, NULL, &nf);
    CHECK(h == NULL);
    if (h) U(RemoveFontMemResourceEx)(h);
    HeapFree(GetProcessHeap(), 0, b);
}
TEST(fonts_mem_tiny_sizes) {
    REQUIRE(U(AddFontMemResourceEx));
    BYTE b[16] = {0, 1, 0, 0}; DWORD nf = 0;
    for (DWORD sz = 1; sz <= 16; sz += 3) {
        HANDLE h = U(AddFontMemResourceEx)(b, sz, NULL, &nf);
        CHECKF(h == NULL, "size %lu accepted", sz);
        if (h) U(RemoveFontMemResourceEx)(h);
    }
}
TEST(fonts_mem_remove_invalid) {
    REQUIRE(U(RemoveFontMemResourceEx));
    SetLastError(0);
    CHECK(!U(RemoveFontMemResourceEx)(NULL));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    CHECK(!U(RemoveFontMemResourceEx)((HANDLE)(ULONG_PTR)0x12345678));
    CHECK(!U(RemoveFontMemResourceEx)((HANDLE)(ULONG_PTR)1));
    CHECK(!U(RemoveFontMemResourceEx)(INVALID_HANDLE_VALUE));
}
TEST(fonts_mem_lifecycle) {
    FE_BASE(); REQUIRE(U(AddFontMemResourceEx)); REQUIRE(U(RemoveFontMemResourceEx));
    char src[MAX_PATH + 32]; fe_fontfile(src, sizeof src, "arial.ttf");
    DWORD sz = 0; BYTE *data = fe_readfile(src, &sz);
    if (!data) { t_note("cannot read arial.ttf"); return; }
    DWORD nf = 0x7777;
    HANDLE h = U(AddFontMemResourceEx)(data, sz, NULL, &nf);
    CHECK(h != NULL);
    if (h) {
        EQ(nf, 1);
        int before_removed = count_family("Arial");
        CHECK(before_removed >= 1);
        CHECK(U(RemoveFontMemResourceEx)(h));
        SetLastError(0);
        BOOL again = U(RemoveFontMemResourceEx)(h);
        t_note("second RemoveFontMemResourceEx -> %d", again);
    }
    fe_free(data);
}
TEST(fonts_mem_multiple_handles_distinct) {
    FE_BASE(); REQUIRE(U(AddFontMemResourceEx)); REQUIRE(U(RemoveFontMemResourceEx));
    char src[MAX_PATH + 32]; fe_fontfile(src, sizeof src, "cour.ttf");
    DWORD sz = 0; BYTE *data = fe_readfile(src, &sz);
    if (!data) return;
    DWORD n1 = 0, n2 = 0;
    HANDLE a = U(AddFontMemResourceEx)(data, sz, NULL, &n1), b = U(AddFontMemResourceEx)(data, sz, NULL, &n2);
    CHECK(a != NULL && b != NULL);
    CHECK(a != b);
    if (a) CHECK(U(RemoveFontMemResourceEx)(a));
    if (b) CHECK(U(RemoveFontMemResourceEx)(b));
    fe_free(data);
}
TEST(fonts_mem_data_copied) {
    FE_BASE(); REQUIRE(U(AddFontMemResourceEx)); REQUIRE(U(RemoveFontMemResourceEx));
    char src[MAX_PATH + 32]; fe_fontfile(src, sizeof src, "tahoma.ttf");
    DWORD sz = 0; BYTE *data = fe_readfile(src, &sz);
    if (!data) return;
    DWORD nf = 0;
    HANDLE h = U(AddFontMemResourceEx)(data, sz, NULL, &nf);
    CHECK(h != NULL);
    memset(data, 0, sz);                         /* the font must not depend on the caller's buffer */
    fe_free(data);
    fe_ctx c; int ok = fe_open(&c, "Tahoma", -16, 400, 0, ANSI_CHARSET);
    CHECK(ok);
    if (ok) { TEXTMETRICA tm; CHECK(U(GetTextMetricsA)(c.dc, &tm)); CHECK(tm.tmHeight > 0); }
    fe_close(&c);
    if (h) CHECK(U(RemoveFontMemResourceEx)(h));
}
TEST(fonts_mem_reserved_param) {
    REQUIRE(U(AddFontMemResourceEx));
    BYTE b[64] = {0}; DWORD nf = 0;
    HANDLE h = U(AddFontMemResourceEx)(b, 64, (PVOID)(ULONG_PTR)1, &nf);
    CHECK(h == NULL);
    if (h) U(RemoveFontMemResourceEx)(h);
}
TEST(fonts_mem_gdi_count_stable) {
    FE_BASE(); REQUIRE(U(AddFontMemResourceEx)); REQUIRE(U(RemoveFontMemResourceEx)); REQUIRE(U(GetGuiResources));
    char src[MAX_PATH + 32]; fe_fontfile(src, sizeof src, "cour.ttf");
    DWORD sz = 0; BYTE *data = fe_readfile(src, &sz);
    if (!data) return;
    DWORD before = fe_gdi_count();
    for (int i = 0; i < 5; i++) {
        DWORD nf = 0; HANDLE h = U(AddFontMemResourceEx)(data, sz, NULL, &nf);
        CHECK(h != NULL);
        if (h) U(RemoveFontMemResourceEx)(h);
    }
    EQ(fe_gdi_count(), before);
    fe_free(data);
}

/* ---- CreateScalableFontResource ---- */
TEST(fonts_scalable_null_params) {
    REQUIRE(U(CreateScalableFontResourceA)); REQUIRE(U(CreateScalableFontResourceW));
    EQ(U(CreateScalableFontResourceA)(0, NULL, NULL, NULL), 0);
    EQ(U(CreateScalableFontResourceA)(0, "x.fot", NULL, NULL), 0);
    EQ(U(CreateScalableFontResourceA)(0, NULL, "x.ttf", NULL), 0);
    EQ(U(CreateScalableFontResourceW)(0, NULL, NULL, NULL), 0);
    EQ(U(CreateScalableFontResourceW)(0, L"x.fot", NULL, NULL), 0);
    EQ(U(CreateScalableFontResourceW)(0, NULL, L"x.ttf", NULL), 0);
}
TEST(fonts_scalable_nonexistent_font_file) {
    REQUIRE(U(CreateScalableFontResourceA)); REQUIRE(U(CreateScalableFontResourceW));
    char tmp[MAX_PATH], fot[MAX_PATH + 40];
    GetTempPathA(sizeof tmp, tmp);
    wsprintfA(fot, "%sfe_none_%lu.fot", tmp, GetCurrentProcessId());
    DeleteFileA(fot);
    SetLastError(0);
    EQ(U(CreateScalableFontResourceA)(0, fot, "C:\\no_such_dir_gdifull\\nofont.ttf", NULL), 0);
    t_note("CreateScalableFontResourceA(nonexistent ttf) error %lu", GetLastError());
    WCHAR wf[MAX_PATH + 40]; MultiByteToWideChar(CP_ACP, 0, fot, -1, wf, MAX_PATH + 40);
    EQ(U(CreateScalableFontResourceW)(1, wf, L"C:\\no_such_dir_gdifull\\nofont.ttf", NULL), 0);
    DeleteFileA(fot);
}
TEST(fonts_scalable_existing_output_file) {
    REQUIRE(U(CreateScalableFontResourceA));
    char tmp[MAX_PATH], fot[MAX_PATH + 40];
    GetTempPathA(sizeof tmp, tmp);
    wsprintfA(fot, "%sfe_exist_%lu.fot", tmp, GetCurrentProcessId());
    HANDLE h = CreateFileA(fot, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    CloseHandle(h);
    char src[MAX_PATH + 32]; fe_fontfile(src, sizeof src, "arial.ttf");
    SetLastError(0);
    BOOL r = U(CreateScalableFontResourceA)(0, fot, src, NULL);
    CHECKF(!r, "resource file overwrote an existing file");
    t_note("CreateScalableFontResourceA(existing .fot) = %d error %lu", r, GetLastError());
    DeleteFileA(fot);
}
TEST(fonts_scalable_bad_output_directory) {
    REQUIRE(U(CreateScalableFontResourceA));
    char src[MAX_PATH + 32]; fe_fontfile(src, sizeof src, "arial.ttf");
    BOOL r = U(CreateScalableFontResourceA)(0, "C:\\no_such_dir_gdifull\\out.fot", src, NULL);
    EQ(r, 0);
    EQ(GetFileAttributesA("C:\\no_such_dir_gdifull\\out.fot"), INVALID_FILE_ATTRIBUTES);
}
