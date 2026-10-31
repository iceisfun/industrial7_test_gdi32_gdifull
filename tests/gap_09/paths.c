/* bMakePathNameW, bInitSystemAndFontsDirectoriesW, cGetTTFFromFOT (internal font path helpers).
 *
 *  BOOL bMakePathNameW(WCHAR out[MAX_PATH], const WCHAR *name, WCHAR **filePart, UINT *flags)
 *     flags: 1 found in <windir>\fonts, 2 absolute file directly in <windir>\system, 4 name was not rooted
 *  LONG bInitSystemAndFontsDirectoriesW(WCHAR **sysdir, WCHAR **fontsdir): 1 ok / 0 failed; no-op when *sysdir set
 *  UINT cGetTTFFromFOT(path, cwc, out, UINT *flags, UINT *, DWORD *, BOOL verifyFot)
 */
#include "gap9.h"

typedef BOOL (WINAPI *bmp_fn)(WCHAR *, const WCHAR *, WCHAR **, UINT *);
typedef LONG (WINAPI *bis_fn)(WCHAR **, WCHAR **);
typedef UINT (WINAPI *gtf_fn)(const WCHAR *, UINT, WCHAR *, UINT *, UINT *, DWORD *, int);

static bmp_fn BMP(void) { return (bmp_fn)t_proc_gdi32("bMakePathNameW"); }
static bis_fn BIS(void) { return (bis_fn)t_proc_gdi32("bInitSystemAndFontsDirectoriesW"); }
static gtf_fn GTF(void) { return (gtf_fn)t_proc_gdi32("cGetTTFFromFOT"); }

static void windir(WCHAR *o)
{
    UINT (WINAPI *gw)(WCHAR *, UINT) = (void *)t_proc("GetSystemWindowsDirectoryW");
    UINT n = gw(o, MAX_PATH);
    if (n && o[n - 1] == L'\\') o[n - 1] = 0;
}

static int file_exists(const WCHAR *p)
{
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

/* ---- bInitSystemAndFontsDirectoriesW ---- */
TEST(gap9_initdirs_noop_when_sysdir_preset)
{
    bis_fn f = BIS(); REQUIRE(f);
    WCHAR preset[] = L"preset";
    WCHAR *sys = preset, *fonts = NULL;
    EXPECT_EQ(f(&sys, &fonts), 1);
    EXPECT_EQ(sys, preset);
    EXPECT_EQ(fonts, NULL);
    EXPECT_WSTR(preset, L"preset");
}

TEST(gap9_initdirs_noop_leaves_fonts_pointer)
{
    bis_fn f = BIS(); REQUIRE(f);
    WCHAR preset[] = L"p", other[] = L"q";
    WCHAR *sys = preset, *fonts = other;
    EXPECT_EQ(f(&sys, &fonts), 1);
    EXPECT_EQ(sys, preset);
    EXPECT_EQ(fonts, other);
}

TEST(gap9_initdirs_builds_system_and_fonts_paths)
{
    bis_fn f = BIS(); REQUIRE(f);
    WCHAR wd[MAX_PATH]; windir(wd);
    WCHAR *sys = NULL, *fonts = NULL;
    LONG r = f(&sys, &fonts);
    EXPECT_EQ(r, 1);
    REQUIRE(sys); REQUIRE(fonts);
    WCHAR w1[MAX_PATH + 16], w2[MAX_PATH + 16];
    lstrcpyW(w1, wd); lstrcatW(w1, L"\\system");
    lstrcpyW(w2, wd); lstrcatW(w2, L"\\fonts");
    CHECK(!lstrcmpiW(sys, w1));
    CHECK(!lstrcmpiW(fonts, w2));
    /* one allocation: the fonts string lives inside the system string's block */
    CHECK((BYTE *)fonts > (BYTE *)sys);
    SIZE_T sz = LocalSize((HLOCAL)sys);
    CHECK(sz >= (SIZE_T)((BYTE *)fonts - (BYTE *)sys) + (lstrlenW(fonts) + 1) * sizeof(WCHAR));
    LocalFree((HLOCAL)sys);
}

TEST(gap9_initdirs_independent_calls_allocate_separately)
{
    bis_fn f = BIS(); REQUIRE(f);
    WCHAR *s1 = NULL, *f1 = NULL, *s2 = NULL, *f2 = NULL;
    EXPECT_EQ(f(&s1, &f1), 1);
    EXPECT_EQ(f(&s2, &f2), 1);
    REQUIRE(s1); REQUIRE(s2);
    CHECK(s1 != s2);
    CHECK(!lstrcmpW(s1, s2));
    CHECK(!lstrcmpW(f1, f2));
    LocalFree((HLOCAL)s1); LocalFree((HLOCAL)s2);
}

TEST(gap9_initdirs_no_trailing_backslash)
{
    bis_fn f = BIS(); REQUIRE(f);
    WCHAR *s = NULL, *fo = NULL;
    EXPECT_EQ(f(&s, &fo), 1);
    REQUIRE(s);
    CHECK(s[lstrlenW(s) - 1] != L'\\');
    CHECK(fo[lstrlenW(fo) - 1] != L'\\');
    CHECK(lstrlenW(s) < MAX_PATH && lstrlenW(fo) < MAX_PATH);
    LocalFree((HLOCAL)s);
}

TEST(gap9_initdirs_fonts_dir_exists)
{
    bis_fn f = BIS(); REQUIRE(f);
    WCHAR *s = NULL, *fo = NULL;
    EXPECT_EQ(f(&s, &fo), 1);
    REQUIRE(s);
    DWORD a = GetFileAttributesW(fo);
    CHECK(a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY));
    LocalFree((HLOCAL)s);
}

TEST(gap9_initdirs_gdi32_and_full_same_code)
{
    FARPROC a = t_proc_gdi32("bInitSystemAndFontsDirectoriesW"), b = t_proc_gdi32full("bInitSystemAndFontsDirectoriesW");
    REQUIRE(a); REQUIRE(b);
    if (!g9_wine()) CHECK(g9_follow(a) == (const BYTE *)b);
}

/* ---- bMakePathNameW ---- */
TEST(gap9_makepath_relative_font_found_in_fonts_dir)
{
    bmp_fn f = BMP(); REQUIRE(f);
    WCHAR wd[MAX_PATH], want[MAX_PATH + 32], out[MAX_PATH + 8]; windir(wd);
    lstrcpyW(want, wd); lstrcatW(want, L"\\fonts\\arial.ttf");
    if (!file_exists(want)) { t_skip("arial.ttf not installed"); return; }
    memset(out, 0, sizeof out);
    WCHAR *part = (WCHAR *)0x1; UINT fl = 0xFFFF;
    BOOL r = f(out, L"arial.ttf", &part, &fl);
    EXPECT_EQ(r, 1);
    CHECK(!lstrcmpiW(out, want));
    EXPECT_EQ(fl, 5);
    CHECK(part == out + lstrlenW(out) - 9);
    CHECK(!lstrcmpiW(part, L"arial.ttf"));
}

TEST(gap9_makepath_relative_is_case_insensitive)
{
    bmp_fn f = BMP(); REQUIRE(f);
    WCHAR wd[MAX_PATH], want[MAX_PATH + 32], out[MAX_PATH + 8]; windir(wd);
    lstrcpyW(want, wd); lstrcatW(want, L"\\fonts\\arial.ttf");
    if (!file_exists(want)) { t_skip("arial.ttf not installed"); return; }
    UINT fl = 0;
    EXPECT_EQ(f(out, L"ARIAL.TTF", NULL, &fl), 1);
    CHECK(!lstrcmpiW(out, want));
    EXPECT_EQ(fl, 5);
}

TEST(gap9_makepath_null_filepart_and_flags_allowed)
{
    bmp_fn f = BMP(); REQUIRE(f);
    WCHAR wd[MAX_PATH], want[MAX_PATH + 32], out[MAX_PATH + 8]; windir(wd);
    lstrcpyW(want, wd); lstrcatW(want, L"\\fonts\\arial.ttf");
    if (!file_exists(want)) { t_skip("arial.ttf not installed"); return; }
    EXPECT_EQ(f(out, L"arial.ttf", NULL, NULL), 1);
    CHECK(!lstrcmpiW(out, want));
}

TEST(gap9_makepath_relative_not_in_fonts_uses_default_search)
{
    bmp_fn f = BMP(); REQUIRE(f);
    WCHAR out[MAX_PATH + 8]; UINT fl = 0xFF;
    BOOL r = f(out, L"kernel32.dll", NULL, &fl);
    EXPECT_EQ(r, 1);
    EXPECT_EQ(fl, 4);          /* relative, not found in fonts */
    WCHAR sys[MAX_PATH], want[MAX_PATH + 32];
    UINT (WINAPI *gs)(WCHAR *, UINT) = (void *)t_proc("GetSystemDirectoryW");
    if (gs && gs(sys, MAX_PATH)) { lstrcpyW(want, sys); lstrcatW(want, L"\\kernel32.dll"); CHECK(!lstrcmpiW(out, want)); }
}

TEST(gap9_makepath_relative_missing_file)
{
    bmp_fn f = BMP(); REQUIRE(f);
    WCHAR out[MAX_PATH + 8]; UINT fl = 0xFF;
    WCHAR *part = NULL;
    BOOL r = f(out, L"gap9_no_such_font_file.ttf", &part, &fl);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(fl, 4);
}

TEST(gap9_makepath_absolute_existing_file)
{
    bmp_fn f = BMP(); REQUIRE(f);
    WCHAR sys[MAX_PATH], in[MAX_PATH + 32], out[MAX_PATH + 8];
    UINT (WINAPI *gs)(WCHAR *, UINT) = (void *)t_proc("GetSystemDirectoryW");
    REQUIRE(gs);
    gs(sys, MAX_PATH);
    lstrcpyW(in, sys); lstrcatW(in, L"\\kernel32.dll");
    UINT fl = 0xFF; WCHAR *part = NULL;
    EXPECT_EQ(f(out, in, &part, &fl), 1);
    CHECK(!lstrcmpiW(out, in));
    EXPECT_EQ(fl, 0);
    CHECK(part && !lstrcmpiW(part, L"kernel32.dll"));
}

TEST(gap9_makepath_absolute_missing_file)
{
    bmp_fn f = BMP(); REQUIRE(f);
    WCHAR out[MAX_PATH + 8]; UINT fl = 0xFF;
    EXPECT_EQ(f(out, L"C:\\gap9_no_such_dir_xyz\\no_such.ttf", NULL, &fl), 0);
    EXPECT_EQ(fl, 0);
}

TEST(gap9_makepath_rooted_without_drive)
{
    /* a name starting with a backslash is rooted: flag 4 never set */
    bmp_fn f = BMP(); REQUIRE(f);
    WCHAR out[MAX_PATH + 8]; UINT fl = 0xFF;
    BOOL r = f(out, L"\\gap9_no_such_file_xyz.ttf", NULL, &fl);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(fl & 4, 0);
}

TEST(gap9_makepath_absolute_font_path_resolves_itself)
{
    bmp_fn f = BMP(); REQUIRE(f);
    WCHAR wd[MAX_PATH], in[MAX_PATH + 32], out[MAX_PATH + 8]; windir(wd);
    lstrcpyW(in, wd); lstrcatW(in, L"\\fonts\\arial.ttf");
    if (!file_exists(in)) { t_skip("arial.ttf not installed"); return; }
    UINT fl = 0xFF;
    EXPECT_EQ(f(out, in, NULL, &fl), 1);
    CHECK(!lstrcmpiW(out, in));
    EXPECT_EQ(fl, 0);
}

TEST(gap9_makepath_system_dir_leaf_missing_tries_fonts)
{
    /* <windir>\system\<leaf> that does not exist is looked up in <windir>\fonts by leaf name */
    bmp_fn f = BMP(); REQUIRE(f);
    WCHAR wd[MAX_PATH], in[MAX_PATH + 32], out[MAX_PATH + 8], font[MAX_PATH + 32]; windir(wd);
    lstrcpyW(font, wd); lstrcatW(font, L"\\fonts\\arial.ttf");
    if (!file_exists(font)) { t_skip("arial.ttf not installed"); return; }
    lstrcpyW(in, wd); lstrcatW(in, L"\\system\\arial.ttf");
    if (file_exists(in)) { t_skip("legacy system dir has arial.ttf"); return; }
    UINT fl = 0;
    BOOL r = f(out, in, NULL, &fl);
    EXPECT_EQ(r, 1);
    EXPECT_EQ(fl & 1, 1);
    CHECK(!lstrcmpiW(out, font));
}

TEST(gap9_makepath_system_dir_leaf_missing_everywhere)
{
    bmp_fn f = BMP(); REQUIRE(f);
    WCHAR wd[MAX_PATH], in[MAX_PATH + 48], out[MAX_PATH + 8]; windir(wd);
    lstrcpyW(in, wd); lstrcatW(in, L"\\system\\gap9_no_such_font.ttf");
    UINT fl = 0xFF;
    EXPECT_EQ(f(out, in, NULL, &fl), 0);
    EXPECT_EQ(fl & 1, 0);
}

TEST(gap9_makepath_system_dir_subdir_not_searched_in_fonts)
{
    bmp_fn f = BMP(); REQUIRE(f);
    WCHAR wd[MAX_PATH], in[MAX_PATH + 48], out[MAX_PATH + 8]; windir(wd);
    lstrcpyW(in, wd); lstrcatW(in, L"\\system\\sub\\arial.ttf");
    UINT fl = 0xFF;
    EXPECT_EQ(f(out, in, NULL, &fl), 0);
    EXPECT_EQ(fl & 1, 0);
}

TEST(gap9_makepath_flags_cleared_even_on_failure)
{
    bmp_fn f = BMP(); REQUIRE(f);
    WCHAR out[MAX_PATH + 8]; UINT fl = 0xDEADBEEF;
    f(out, L"C:\\gap9_zz\\q.ttf", NULL, &fl);
    CHECK(fl != 0xDEADBEEF);
}

TEST(gap9_makepath_temp_file_roundtrip)
{
    bmp_fn f = BMP(); REQUIRE(f);
    WCHAR dir[MAX_PATH], path[MAX_PATH + 32], out[MAX_PATH + 8];
    DWORD n = GetTempPathW(MAX_PATH, dir);
    REQUIRE(n && n < MAX_PATH - 20);
    wsprintfW(path, L"%sgap9_mp_%lu.tmp", dir, GetCurrentProcessId());
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot create temp file"); return; }
    CloseHandle(h);
    UINT fl = 0xFF; WCHAR *part = NULL;
    BOOL r = f(out, path, &part, &fl);
    DeleteFileW(path);
    EXPECT_EQ(r, 1);
    EXPECT_EQ(fl, 0);
    /* SearchPath returns the long form of the temp dir; compare the leaf */
    CHECK(part && !lstrcmpiW(part, wcsrchr(path, L'\\') + 1));
}

TEST(gap9_makepath_utf16_unicode_name)
{
    bmp_fn f = BMP(); REQUIRE(f);
    WCHAR out[MAX_PATH + 8]; UINT fl = 0;
    static const WCHAR nm[] = { 'g', 'a', 'p', '9', 0x00E9, 0x0416, 0x4E2D, '.', 't', 't', 'f', 0 };
    EXPECT_EQ(f(out, nm, NULL, &fl), 0);
    EXPECT_EQ(fl, 4);
}

TEST(gap9_makepath_too_long_result_fails)
{
    /* an existing file whose full path needs >= MAX_PATH chars cannot be returned in the fixed buffer */
    bmp_fn f = BMP(); REQUIRE(f);
    WCHAR dir[MAX_PATH], longdir[600], path[700];
    DWORD n = GetTempPathW(MAX_PATH, dir);
    REQUIRE(n && n < 100);
    wsprintfW(longdir, L"\\\\?\\%sgap9_lp_%lu", dir, GetCurrentProcessId());
    if (!CreateDirectoryW(longdir, NULL)) { t_skip("cannot create dir"); return; }
    WCHAR cur[600]; lstrcpyW(cur, longdir);
    int depth = 0;
    while (lstrlenW(cur) < 280 && depth < 12) {
        WCHAR save[600]; lstrcpyW(save, cur);
        lstrcatW(cur, L"\\aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        if (!CreateDirectoryW(cur, NULL)) { lstrcpyW(cur, save); break; }
        depth++;
    }
    wsprintfW(path, L"%s\\f.ttf", cur);
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    int made = h != INVALID_HANDLE_VALUE;
    if (made) CloseHandle(h);
    BOOL r = 2; UINT fl = 0;
    if (made) {
        WCHAR out[MAX_PATH + 8];
        /* drop the \\?\ prefix: a plain drive path longer than MAX_PATH */
        r = f(out, path + 4, NULL, &fl);
        DeleteFileW(path);
    }
    /* unwind */
    for (int i = depth; i > 0; i--) {
        RemoveDirectoryW(cur);
        WCHAR *s = wcsrchr(cur, L'\\'); if (s) *s = 0;
    }
    RemoveDirectoryW(longdir);
    if (!made) { t_skip("long path file not creatable"); return; }
    t_note("bMakePathNameW(long path) = %d flags %u", (int)r, fl);
    EXPECT_EQ(r, 0);
}

/* ---- cGetTTFFromFOT ---- */
TEST(gap9_fot_plain_name_returns_length_plus_one)
{
    gtf_fn f = GTF(); REQUIRE(f);
    UINT fl = 0xFF;
    WCHAR out[32];
    UINT r = f(L"arial.ttf", 32, out, &fl, NULL, NULL, 0);
    EXPECT_EQ(r, 10);
    EXPECT_EQ(fl, 8);
    EXPECT_WSTR(out, L"arial.ttf");
}

TEST(gap9_fot_query_length_with_null_output)
{
    gtf_fn f = GTF(); REQUIRE(f);
    UINT fl = 0;
    EXPECT_EQ(f(L"arial.ttf", 0, NULL, &fl, NULL, NULL, 0), 10);
    EXPECT_EQ(f(L"verylongfontfilename.ttf", 0, NULL, NULL, NULL, NULL, 0), 25);
    EXPECT_EQ(f(L"a", 100, NULL, NULL, NULL, NULL, 0), 2);
}

TEST(gap9_fot_exact_and_short_output_buffer)
{
    gtf_fn f = GTF(); REQUIRE(f);
    WCHAR out[32];
    for (int i = 0; i < 32; i++) out[i] = 0x7777;
    EXPECT_EQ(f(L"arial.ttf", 10, out, NULL, NULL, NULL, 0), 10);
    EXPECT_WSTR(out, L"arial.ttf");
    for (int i = 0; i < 32; i++) out[i] = 0x7777;
    EXPECT_EQ(f(L"arial.ttf", 9, out, NULL, NULL, NULL, 0), 0);
    EXPECT_EQ(out[0], 0x7777);
    EXPECT_EQ(f(L"arial.ttf", 0, out, NULL, NULL, NULL, 0), 0);
    EXPECT_EQ(out[0], 0x7777);
    EXPECT_EQ(f(L"arial.ttf", 1, out, NULL, NULL, NULL, 0), 0);
}

TEST(gap9_fot_empty_name)
{
    gtf_fn f = GTF(); REQUIRE(f);
    WCHAR out[4] = { 0x7777, 0x7777 };
    UINT fl = 0xFF;
    EXPECT_EQ(f(L"", 4, out, &fl, NULL, NULL, 0), 1);
    EXPECT_EQ(out[0], 0);
    EXPECT_EQ(fl, 8);
    EXPECT_EQ(f(L"", 0, NULL, NULL, NULL, NULL, 0), 1);
}

TEST(gap9_fot_flags_optional)
{
    gtf_fn f = GTF(); REQUIRE(f);
    WCHAR out[16];
    EXPECT_EQ(f(L"x.pfb", 16, out, NULL, NULL, NULL, 0), 6);
    EXPECT_WSTR(out, L"x.pfb");
}

TEST(gap9_fot_short_names_never_treated_as_fot)
{
    /* the ".fot" check needs more than 4 characters */
    gtf_fn f = GTF(); REQUIRE(f);
    WCHAR out[16]; UINT fl = 0;
    EXPECT_EQ(f(L".fot", 16, out, &fl, NULL, NULL, 0), 5);
    EXPECT_EQ(fl, 8);
    EXPECT_WSTR(out, L".fot");
    EXPECT_EQ(f(L"ab", 16, out, &fl, NULL, NULL, 0), 3);
    EXPECT_EQ(f(L"abcd", 16, out, &fl, NULL, NULL, 0), 5);
}

TEST(gap9_fot_various_extensions_table)
{
    gtf_fn f = GTF(); REQUIRE(f);
    static const WCHAR *const names[] = { L"a.ttf", L"B.TTC", L"c.otf", L"d.fon", L"e.fnt", L"f.pfm", L"g.pfb",
        L"verdana.TTF", L"C:\\Windows\\Fonts\\arial.ttf", L"relative\\dir\\x.ttf", L"noext", L"foo.fo", L"foo.fots" };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        WCHAR out[MAX_PATH]; UINT fl = 0;
        UINT r = f(names[i], MAX_PATH, out, &fl, NULL, NULL, 0);
        EXPECT_EQ(r, (UINT)lstrlenW(names[i]) + 1);
        EXPECT_EQ(fl, 8);
        CHECKF(!lstrcmpW(out, names[i]), "name %zu", i);
    }
}

TEST(gap9_fot_verify_missing_file_returns_zero)
{
    gtf_fn f = GTF(); REQUIRE(f);
    WCHAR out[MAX_PATH]; UINT fl = 0xFF;
    for (int i = 0; i < MAX_PATH; i++) out[i] = 0x7777;
    UINT a = 0, b = 0; DWORD c = 0;
    UINT r = f(L"C:\\gap9_no_such_dir\\missing.fot", MAX_PATH, out, &fl, &a, &c, 1);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(fl, 0);
    EXPECT_EQ(out[0], 0x7777);
}

TEST(gap9_fot_verify_garbage_file_not_a_fot)
{
    gtf_fn f = GTF(); REQUIRE(f);
    WCHAR dir[MAX_PATH], path[MAX_PATH + 40];
    DWORD n = GetTempPathW(MAX_PATH, dir);
    REQUIRE(n && n < MAX_PATH - 30);
    wsprintfW(path, L"%sgap9_fot_%lu.fot", dir, GetCurrentProcessId());
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot create temp file"); return; }
    BYTE zero[256]; memset(zero, 0, sizeof zero);
    DWORD w; WriteFile(h, zero, sizeof zero, &w, NULL);
    CloseHandle(h);
    WCHAR out[MAX_PATH]; UINT fl = 0xFF, a = 0; DWORD c = 0;
    UINT r = f(path, MAX_PATH, out, &fl, &a, &c, 1);
    DeleteFileW(path);
    t_note("cGetTTFFromFOT(zeroed .fot, verify) = %u flags 0x%x", r, fl);
    /* not a valid FOT: either rejected or returned as a plain path */
    CHECK(r == 0 || r == (UINT)lstrlenW(path) + 1);
}

TEST(gap9_fot_gdi32_export_is_forwarder_to_full)
{
    FARPROC a = t_proc_gdi32("cGetTTFFromFOT"), b = t_proc_gdi32full("cGetTTFFromFOT");
    REQUIRE(a); REQUIRE(b);
    if (!g9_wine()) CHECK(g9_follow(a) == (const BYTE *)b);
}

TEST(gap9_makepath_gdi32_export_is_forwarder_to_full)
{
    FARPROC a = t_proc_gdi32("bMakePathNameW"), b = t_proc_gdi32full("bMakePathNameW");
    REQUIRE(a); REQUIRE(b);
    if (!g9_wine()) CHECK(g9_follow(a) == (const BYTE *)b);
}
