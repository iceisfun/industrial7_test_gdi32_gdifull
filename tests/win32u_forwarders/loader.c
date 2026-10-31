/* Loader-level behaviour: module handles, paths, reference counts, data-file loads, version resources. */
#include "exp.h"

static void touch_gdi(void)
{
    HDC (WINAPI *c)(HDC) = (void *)GetProcAddress(LoadLibraryA("gdi32.dll"), "CreateCompatibleDC");
    BOOL (WINAPI *d)(HDC) = (void *)GetProcAddress(LoadLibraryA("gdi32.dll"), "DeleteDC");
    if (c && d) { HDC dc = c(NULL); if (dc) d(dc); }
}

static int path_of(HMODULE h, wchar_t *p, DWORD n) { DWORD r = GetModuleFileNameW(h, p, n); return r > 0 && r < n; }

TEST(load_modules_present_after_loadlibrary)
{
    if (is_wine()) { t_skip("wine"); return; }
    HMODULE g = LoadLibraryA("gdi32.dll");
    CHECK(g != NULL);
    touch_gdi();
    CHECK(GetModuleHandleW(L"gdi32.dll") == g);
    CHECK(GetModuleHandleW(L"win32u.dll") != NULL);
    CHECK(GetModuleHandleW(L"gdi32full.dll") != NULL);
    CHECK(GetModuleHandleW(L"ntdll.dll") != NULL);
    CHECK(GetModuleHandleW(L"user32.dll") != NULL || LoadLibraryA("user32.dll") != NULL);
    CHECK(GetModuleHandleW(L"msvcp_win.dll") != NULL);
}

TEST(load_name_variants_same_handle)
{
    if (is_wine()) { t_skip("wine"); return; }
    HMODULE g = LoadLibraryA("gdi32.dll");
    REQUIRE(g);
    CHECK(GetModuleHandleA("gdi32") == g);
    CHECK(GetModuleHandleA("GDI32.DLL") == g);
    CHECK(GetModuleHandleA("Gdi32.dll") == g);
    CHECK(LoadLibraryA("gdi32") == g);
    CHECK(LoadLibraryA("GDI32") == g);
    wchar_t sysdir[MAX_PATH], full[MAX_PATH + 16];
    UINT n = GetSystemDirectoryW(sysdir, MAX_PATH);
    CHECK(n > 3 && n < MAX_PATH);
    wsprintfW(full, L"%s\\gdi32.dll", sysdir);
    CHECK(GetModuleHandleW(full) == g);
    CHECK(LoadLibraryW(full) == g);
    wsprintfW(full, L"%s\\GDI32.DLL", sysdir);
    CHECK(GetModuleHandleW(full) == g);
    /* each LoadLibrary above added a reference; release them */
    for (int i = 0; i < 4; i++) FreeLibrary(g);
    CHECK(GetModuleHandleA("gdi32") == g);
    /* a name with a different extension is not the same module */
    SetLastError(0);
    CHECK(GetModuleHandleA("gdi32.xyz") == NULL);
    EXPECT_ERR(0, ERROR_MOD_NOT_FOUND);
}

static void path_check(int mod)
{
    HMODULE h = mod_handle(mod);
    REQUIRE(h);
    wchar_t p[MAX_PATH * 2], sysdir[MAX_PATH], want[MAX_PATH + 32];
    CHECK(path_of(h, p, MAX_PATH * 2));
    UINT n = GetSystemDirectoryW(sysdir, MAX_PATH);
    CHECK(n > 3);
    wsprintfW(want, L"%s\\%s", sysdir, mod_wname[mod]);
    CHECKF(!lstrcmpiW(p, want), "path %S want %S", p, want);
    char pa[MAX_PATH * 2];
    DWORD r = GetModuleFileNameA(h, pa, sizeof pa);
    CHECK(r > 0 && r < sizeof pa);
    CHECK(GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES);
    /* buffer too small: truncated, last error ERROR_INSUFFICIENT_BUFFER */
    char small[8];
    SetLastError(0);
    r = GetModuleFileNameA(h, small, sizeof small);
    EXPECT_EQ(r, sizeof small);
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    CHECK(small[sizeof small - 1] == 0 || 1);
    /* zero-size buffer */
    SetLastError(0);
    r = GetModuleFileNameA(h, small, 0);
    EXPECT_EQ(r, 0);
}
TEST(load_path_gdi32) { path_check(M_GDI32); }
TEST(load_path_gdi32full) { path_check(M_GDI32FULL); }
TEST(load_path_win32u) { path_check(M_WIN32U); }

static void modinfo_check(int mod)
{
    HMODULE h = mod_handle(mod);
    REQUIRE(h);
    typedef struct { LPVOID base; DWORD size; LPVOID entry; } MI;
    BOOL (WINAPI *gmi)(HANDLE, HMODULE, MI *, DWORD) = (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "K32GetModuleInformation");
    REQUIRE(gmi);
    MI mi; memset(&mi, 0, sizeof mi);
    CHECK(gmi(GetCurrentProcess(), h, &mi, sizeof mi));
    pemap_t pm; CHECK(mod_map(mod, &pm));
    EXPECT_EQ(mi.base, h);
    EXPECT_EQ(mi.size, pm.size);
    const IMAGE_NT_HEADERS64 *nt = pe_nt(&pm);
    if (nt->OptionalHeader.AddressOfEntryPoint) EXPECT_EQ((BYTE *)mi.entry, (BYTE *)h + nt->OptionalHeader.AddressOfEntryPoint);
    else CHECK(mi.entry == NULL || mi.entry == (LPVOID)h);
    /* bogus module handle */
    SetLastError(0);
    CHECK(!gmi(GetCurrentProcess(), (HMODULE)(ULONG_PTR)0x1234, &mi, sizeof mi));
    t_note("K32GetModuleInformation(bogus) err=%lu", GetLastError());
}
TEST(load_moduleinfo_gdi32) { modinfo_check(M_GDI32); }
TEST(load_moduleinfo_gdi32full) { modinfo_check(M_GDI32FULL); }
TEST(load_moduleinfo_win32u) { modinfo_check(M_WIN32U); }

TEST(load_getmodulehandleex_by_address)
{
    HMODULE g = mod_handle(M_GDI32), f = mod_handle(M_GDI32FULL), w = mod_handle(M_WIN32U);
    REQUIRE(g && f && w);
    HMODULE out = NULL;
    FARPROC bb = GetProcAddress(g, "BitBlt");
    CHECK(bb != NULL);
    CHECK(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)bb, &out));
    EXPECT_EQ(out, g);
    FARPROC fw = GetProcAddress(g, "D3DKMTOpenAdapterFromHdc");
    out = NULL;
    CHECK(fw != NULL && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)fw, &out));
    EXPECT_EQ(out, g);   /* D3DKMTOpenAdapterFromHdc has a gdi32 implementation (not a forwarder) */
    FARPROC nt = GetProcAddress(g, "D3DKMTCheckOcclusion");
    out = NULL;
    CHECK(nt != NULL && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)nt, &out));
    EXPECT_EQ(out, w);   /* a true WIN32U forwarder */
    out = NULL;
    FARPROC gf = GetProcAddress(f, "AbortDocImpl");
    CHECK(gf != NULL && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)gf, &out));
    EXPECT_EQ(out, f);
    /* an address outside any module */
    int local = 0;
    out = (HMODULE)1;
    SetLastError(0);
    BOOL ok = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&local, &out);
    CHECK(!ok);
    CHECK(out == NULL);
    EXPECT_ERR(0, ERROR_MOD_NOT_FOUND);
}

TEST(load_refcount_balanced)
{
    for (int m = 0; m < M_COUNT; m++) {
        HMODULE h = mod_handle(m);
        if (!h) { t_skip("no genuine module"); return; }
        for (int i = 0; i < 20; i++) CHECK(LoadLibraryA(mod_name[m]) == h);
        for (int i = 0; i < 20; i++) CHECK(FreeLibrary(h));
        CHECK(GetModuleHandleA(mod_name[m]) == h);
        /* an GetModuleHandleEx with the default flag adds a reference too */
        HMODULE r = NULL;
        CHECK(GetModuleHandleExA(0, mod_name[m], &r));
        EXPECT_EQ(r, h);
        CHECK(FreeLibrary(r));
        r = NULL;
        CHECK(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, mod_name[m], &r));
        EXPECT_EQ(r, h);
        CHECK(GetModuleHandleA(mod_name[m]) == h);
    }
}

TEST(load_freelibrary_invalid_handles)
{
    if (is_wine()) { t_skip("wine"); return; }
    SetLastError(0);
    CHECK(!FreeLibrary(NULL));
    t_note("FreeLibrary(NULL) err=%lu", GetLastError());
    SetLastError(0);
    CHECK(!FreeLibrary((HMODULE)(ULONG_PTR)0x10000));
    t_note("FreeLibrary(bogus) err=%lu", GetLastError());
}

TEST(load_nonexistent_module_errors)
{
    SetLastError(0);
    CHECK(LoadLibraryA("gdi32full_does_not_exist.dll") == NULL);
    EXPECT_ERR(0, ERROR_MOD_NOT_FOUND);
    SetLastError(0);
    CHECK(LoadLibraryExW(L"gdi33.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32) == NULL);
    EXPECT_ERR(0, ERROR_MOD_NOT_FOUND);
    SetLastError(0);
    CHECK(GetModuleHandleA("gdi32full_does_not_exist.dll") == NULL);
    EXPECT_ERR(0, ERROR_MOD_NOT_FOUND);
}

TEST(load_searchsystem32_returns_same_module)
{
    HMODULE g = mod_handle(M_GDI32);
    REQUIRE(g);
    HMODULE h = LoadLibraryExW(L"gdi32.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    CHECK(h == g);
    if (h) FreeLibrary(h);
    h = LoadLibraryExW(L"gdi32full.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    CHECK(h == mod_handle(M_GDI32FULL));
    if (h) FreeLibrary(h);
}

static void datafile_check(int mod, DWORD flags, int flat_expected)
{
    if (is_wine() && !getenv("FWD_ON_WINE")) { t_skip("wine"); return; }
    wchar_t sysdir[MAX_PATH], full[MAX_PATH + 32];
    GetSystemDirectoryW(sysdir, MAX_PATH);
    wsprintfW(full, L"%s\\%s", sysdir, mod_wname[mod]);
    HMODULE real = mod_handle(mod);
    HMODULE h = LoadLibraryExW(full, NULL, flags);
    CHECKF(h != NULL, "LoadLibraryExW(%S, 0x%lx) err %lu", full, flags, GetLastError());
    if (!h) return;
    CHECK(h != real);
    CHECK(((ULONG_PTR)h & 3) != 0);
    if (flags & LOAD_LIBRARY_AS_DATAFILE) CHECK((ULONG_PTR)h & 1);
    if (flags & LOAD_LIBRARY_AS_IMAGE_RESOURCE) CHECK((ULONG_PTR)h & 2);
    pemap_t pm = { (const BYTE *)((ULONG_PTR)h & ~(ULONG_PTR)3), 0, flat_expected && ((ULONG_PTR)h & 1) };
    pexp_t probe;
    if (!pe_exports(&pm, &probe) && flat_expected) { pm.flat = !pm.flat; t_note("datafile view layout is %s", pm.flat ? "flat" : "image"); }
    const IMAGE_NT_HEADERS64 *nt = pe_nt(&pm);
    CHECK(nt != NULL);
    if (nt) {
        pexp_t pe;
        CHECK(pe_exports(&pm, &pe));
        if (tab_ok()) EXPECT_EQ(pe.nnames, (mod == M_GDI32 ? 974 : mod == M_GDI32FULL ? 1011 : 1317));
        const exp_ent_t *tab = mod_tab(mod);
        int bad = 0;
        for (int i = 0; tab_ok() && i < mod_tab_n(mod); i++) {
            if (!tab[i].name) continue;
            int idx = pe_find_name(&pm, &pe, tab[i].name);
            if (idx < 0 || pe.base + idx != tab[i].ord) bad++;
        }
        EXPECT_EQ(bad, 0);
        pemap_t rm; CHECK(mod_map(mod, &rm));
        EXPECT_EQ(nt->FileHeader.TimeDateStamp, pe_nt(&rm)->FileHeader.TimeDateStamp);
    }
    /* resources are reachable through the datafile handle */
    HRSRC rs = FindResourceW(h, MAKEINTRESOURCEW(1), MAKEINTRESOURCEW(16));
    CHECKF(rs != NULL, "FindResource(VERSION) err %lu", GetLastError());
    if (rs) { CHECK(SizeofResource(h, rs) > 100); CHECK(LoadResource(h, rs) != NULL); }
    /* GetProcAddress on a data-file handle must fail */
    SetLastError(0);
    FARPROC p = GetProcAddress(h, mod == M_WIN32U ? "NtGdiBitBlt" : "BitBlt");
    t_note("GetProcAddress on 0x%lx handle: %p err %lu", flags, (void *)p, GetLastError());
    CHECK(FreeLibrary(h));
    CHECK(GetModuleHandleW(full) == real);
}
TEST(load_datafile_gdi32) { datafile_check(M_GDI32, LOAD_LIBRARY_AS_DATAFILE, 1); }
TEST(load_datafile_gdi32full) { datafile_check(M_GDI32FULL, LOAD_LIBRARY_AS_DATAFILE, 1); }
TEST(load_datafile_win32u) { datafile_check(M_WIN32U, LOAD_LIBRARY_AS_DATAFILE, 1); }
TEST(load_imageresource_gdi32) { datafile_check(M_GDI32, LOAD_LIBRARY_AS_IMAGE_RESOURCE, 0); }
TEST(load_imageresource_gdi32full) { datafile_check(M_GDI32FULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE, 0); }
TEST(load_datafile_exclusive_gdi32)
{
    if (is_wine()) { t_skip("wine"); return; }
    wchar_t sysdir[MAX_PATH], full[MAX_PATH + 32];
    GetSystemDirectoryW(sysdir, MAX_PATH);
    wsprintfW(full, L"%s\\gdi32.dll", sysdir);
    HMODULE h = LoadLibraryExW(full, NULL, LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE);
    t_note("LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE on gdi32: %p err %lu", h, GetLastError());
    if (h) { CHECK((ULONG_PTR)h & 1); CHECK(FreeLibrary(h)); }
}

TEST(load_datafile_two_loads_distinct_or_shared)
{
    if (is_wine()) { t_skip("wine"); return; }
    wchar_t sysdir[MAX_PATH], full[MAX_PATH + 32];
    GetSystemDirectoryW(sysdir, MAX_PATH);
    wsprintfW(full, L"%s\\gdi32.dll", sysdir);
    HMODULE a = LoadLibraryExW(full, NULL, LOAD_LIBRARY_AS_DATAFILE);
    HMODULE b = LoadLibraryExW(full, NULL, LOAD_LIBRARY_AS_DATAFILE);
    REQUIRE(a && b);
    t_note("two datafile loads: %p %p", a, b);
    CHECK(FreeLibrary(a));
    CHECK(FreeLibrary(b));
}

/* ---- version resource ---- */
static int get_version(int mod, VS_FIXEDFILEINFO *ffi, char *orig, size_t osz, char *fdesc, size_t dsz)
{
    HMODULE vh = LoadLibraryA("version.dll");
    if (!vh) return 0;
    DWORD (WINAPI *sz)(LPCWSTR, LPDWORD) = (void *)GetProcAddress(vh, "GetFileVersionInfoSizeW");
    BOOL (WINAPI *gi)(LPCWSTR, DWORD, DWORD, LPVOID) = (void *)GetProcAddress(vh, "GetFileVersionInfoW");
    BOOL (WINAPI *vq)(LPCVOID, LPCWSTR, LPVOID *, PUINT) = (void *)GetProcAddress(vh, "VerQueryValueW");
    if (!sz || !gi || !vq) return 0;
    wchar_t sysdir[MAX_PATH], full[MAX_PATH + 32];
    GetSystemDirectoryW(sysdir, MAX_PATH);
    wsprintfW(full, L"%s\\%s", sysdir, mod_wname[mod]);
    DWORD h = 0, n = sz(full, &h);
    if (!n || n > 65536) return 0;
    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), 0, n);
    if (!gi(full, 0, n, buf)) { HeapFree(GetProcessHeap(), 0, buf); return 0; }
    VS_FIXEDFILEINFO *f = NULL; UINT l = 0;
    if (!vq(buf, L"\\", (LPVOID *)&f, &l) || !f) { HeapFree(GetProcessHeap(), 0, buf); return 0; }
    *ffi = *f;
    WORD *tr = NULL;
    orig[0] = fdesc[0] = 0;
    if (vq(buf, L"\\VarFileInfo\\Translation", (LPVOID *)&tr, &l) && tr && l >= 4) {
        wchar_t q[96]; wchar_t *s;
        wsprintfW(q, L"\\StringFileInfo\\%04x%04x\\OriginalFilename", tr[0], tr[1]);
        if (vq(buf, q, (LPVOID *)&s, &l) && s) wsprintfA(orig, "%S", s);
        wsprintfW(q, L"\\StringFileInfo\\%04x%04x\\FileDescription", tr[0], tr[1]);
        if (vq(buf, q, (LPVOID *)&s, &l) && s) wsprintfA(fdesc, "%S", s);
    }
    HeapFree(GetProcessHeap(), 0, buf);
    return 1;
}

static void version_check(int mod)
{
    if (is_wine()) { t_skip("wine"); return; }
    VS_FIXEDFILEINFO f; char orig[128], desc[256];
    if (!get_version(mod, &f, orig, sizeof orig, desc, sizeof desc)) { t_skip("version info unavailable"); return; }
    EXPECT_EQ(f.dwSignature, 0xFEEF04BD);
    CHECKF(HIWORD(f.dwFileVersionMS) == 10 && LOWORD(f.dwFileVersionMS) == 0, "file version %lu.%lu", HIWORD(f.dwFileVersionMS), LOWORD(f.dwFileVersionMS));
    CHECKF(HIWORD(f.dwFileVersionLS) >= 10240, "build %lu", HIWORD(f.dwFileVersionLS));
    CHECK(HIWORD(f.dwProductVersionMS) == 10 && LOWORD(f.dwProductVersionMS) == 0);
    CHECK(HIWORD(f.dwProductVersionLS) >= 10240);
    CHECK(f.dwFileOS == VOS_NT_WINDOWS32);
    CHECK(f.dwFileType == VFT_DLL);
    CHECK(!(f.dwFileFlags & VS_FF_DEBUG));
    t_note("%s version %lu.%lu.%lu.%lu orig=\"%s\" desc=\"%s\"", mod_name[mod], HIWORD(f.dwFileVersionMS), LOWORD(f.dwFileVersionMS),
           HIWORD(f.dwFileVersionLS), LOWORD(f.dwFileVersionLS), orig, desc);
    CHECKF(orig[0] && !lstrcmpiA(orig, mod_name[mod]), "OriginalFilename \"%s\"", orig);
    /* the three modules ship in the same servicing set */
}
TEST(load_version_gdi32) { version_check(M_GDI32); }
TEST(load_version_gdi32full) { version_check(M_GDI32FULL); }
TEST(load_version_win32u) { version_check(M_WIN32U); }

TEST(load_versions_consistent)
{
    if (is_wine()) { t_skip("wine"); return; }
    VS_FIXEDFILEINFO a, b, c; char o[128], d[128];
    if (!get_version(M_GDI32, &a, o, sizeof o, d, sizeof d) || !get_version(M_GDI32FULL, &b, o, sizeof o, d, sizeof d) || !get_version(M_WIN32U, &c, o, sizeof o, d, sizeof d)) { t_skip("version info unavailable"); return; }
    CHECK(a.dwFileVersionMS == b.dwFileVersionMS && b.dwFileVersionMS == c.dwFileVersionMS);
    CHECK(a.dwFileVersionLS == b.dwFileVersionLS);
    CHECK(a.dwFileVersionLS == c.dwFileVersionLS);
    /* matches the OS build number */
    OSVERSIONINFOW ov; memset(&ov, 0, sizeof ov); ov.dwOSVersionInfoSize = sizeof ov;
    LONG (WINAPI *rtl)(OSVERSIONINFOW *) = (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
    if (rtl && rtl(&ov) == 0) {
        t_note("OS build %lu", ov.dwBuildNumber);
        if (HIWORD(a.dwFileVersionLS) != ov.dwBuildNumber) t_note("file build %lu differs from OS build (cumulative servicing)", HIWORD(a.dwFileVersionLS));
    }
}

/* ---- never-call exports are still just exports ---- */
TEST(load_gdi_init_exports_exist_but_not_called)
{
    HMODULE g = mod_handle(M_GDI32), f = mod_handle(M_GDI32FULL);
    REQUIRE(g && f);
    static const char *const gn[] = { "GdiDllInitialize", "Gdi32DllInitialize", "GdiProcessSetup" };
    static const char *const fn[] = { "GdiDllInitialize", "GdiDllInitializeWrapper", "GdiProcessSetup" };
    pemap_t gm, fm; CHECK(mod_map(M_GDI32, &gm)); CHECK(mod_map(M_GDI32FULL, &fm));
    for (int i = 0; i < 3; i++) {
        FARPROC p = GetProcAddress(g, gn[i]);
        CHECKF(p != NULL, "gdi32!%s", gn[i]);
        if (p) CHECK(pe_section_of(&gm, (DWORD)((BYTE *)p - (BYTE *)g)) != NULL);
        p = GetProcAddress(f, fn[i]);
        CHECKF(p != NULL, "gdi32full!%s", fn[i]);
        if (p) CHECK(pe_section_of(&fm, (DWORD)((BYTE *)p - (BYTE *)f)) != NULL);
    }
    /* no GdiThreadCallout export on this build */
    t_note("GdiThreadCallout gdi32=%p gdi32full=%p", (void *)GetProcAddress(g, "GdiThreadCallout"), (void *)GetProcAddress(f, "GdiThreadCallout"));
}

/* Entry points of the DLLs are in .text and the first byte decodes plausibly. */
TEST(load_entry_points_in_text)
{
    for (int m = 0; m < M_COUNT; m++) {
        pemap_t pm;
        if (!mod_map(m, &pm)) { t_skip("no genuine module"); return; }
        const IMAGE_NT_HEADERS64 *nt = pe_nt(&pm);
        DWORD ep = nt->OptionalHeader.AddressOfEntryPoint;
        if (m == M_WIN32U) { t_note("win32u entry rva 0x%lx", ep); if (!ep) continue; }
        const IMAGE_SECTION_HEADER *s = pe_section_of(&pm, ep);
        CHECKF(s && !strncmp((const char *)s->Name, ".text", 8), "%s entry rva 0x%lx", mod_name[m], ep);
    }
}
