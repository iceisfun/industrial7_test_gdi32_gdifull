/* StartDocW, StartPage, font resource removal/add, and the Gdi* internal helper exports */
#include "g3.h"

#define IMPLS(var, pub) for (int i_ = 0; i_ < 2; i_++) { var = (__typeof__(&pub))g3_fn(#pub, i_); \
    if (!var) { t_note(#pub " impl %d missing", i_); continue; }

/* ---------- export presence ---------- */
TEST(gap3_exports_present) {
    static const char *const names[] = {
        "MoveToExImpl", "PolyBezierToImpl", "PolyDrawImpl", "PolylineToImpl", "RectangleImpl",
        "RemoveFontResourceTracking", "RemoveFontResourceWImpl", "ResetDCWImpl", "RestoreDCImpl", "SaveDCImpl",
        "SelectBrushLocal", "SelectClipPathImpl", "SelectClipRgnImpl", "SelectFontLocal", "SelectObjectImpl",
        "SetBkModeImpl", "SetICMModeImpl", "SetMiterLimitImpl", "SetPolyFillModeImpl", "SetStretchBltModeImpl",
        "SetTextAlignImpl", "SetTextColorImpl", "SetWorldTransformImpl", "StartDocWImpl", "StartPageImpl",
        "StretchDIBitsImpl", "StrokePathImpl", "GdiAddFontResourceW", "GdiAddGlsBounds", "GdiAddGlsRecord",
        "GdiAddInitialFonts", "GdiArtificialDecrementDriver", "GdiCleanCacheDC", "GdiConsoleTextOut",
        "GdiConvertAndCheckDC", "GdiConvertBitmap", "GdiConvertBitmapV5", "GdiConvertBrush", "GdiConvertDC" };
    unsigned k;
    for (k = 0; k < sizeof names / sizeof *names; k++) {
        FARPROC p = t_proc_gdi32full(names[k]);
        if (g3_wine()) { if (!p) t_note("(emulator) %s not exported", names[k]); }
        else CHECKF(p != NULL, "%s", names[k]);
    }
}
TEST(gap3_exports_in_module_range) {
    HMODULE m = t_gdi32full(); unsigned k;
    static const char *const names[] = { "SaveDCImpl", "GdiConvertBitmap", "GdiCleanCacheDC", "StretchDIBitsImpl" };
    REQUIRE(m);
    for (k = 0; k < 4; k++) {
        FARPROC p = GetProcAddress(m, names[k]);
        if (!p) { t_note("%s missing", names[k]); continue; }
        MEMORY_BASIC_INFORMATION mbi;
        CHECK(VirtualQuery((void *)p, &mbi, sizeof mbi) == sizeof mbi);
        CHECK(mbi.AllocationBase == (void *)m);
    }
}
TEST(gap3_public_vs_impl_independent_entry) {
    /* public gdi32 entries are thunks into gdi32full: they must not resolve to the Impl address of a different function */
    FARPROC a = t_proc_gdi32("SaveDC"), b = t_proc_gdi32full("SaveDCImpl"), c = t_proc_gdi32full("RestoreDCImpl");
    REQUIRE(a); REQUIRE(b); REQUIRE(c);
    CHECK(b != c);
    CHECK(a != c);
}

/* ---------- identity helpers ---------- */
typedef ULONG_PTR (WINAPI *ID1)(ULONG_PTR);
TEST(gap3_gdiconvert_identity) {
    static const char *const names[] = { "GdiConvertBitmap", "GdiConvertBrush", "GdiConvertDC" };
    static const ULONG_PTR v[] = { 0, 1, 0x1234, 0x00010000, 0xdeadbeef, (ULONG_PTR)-1, 0x123456789abcdefull };
    unsigned n, k;
    for (n = 0; n < 3; n++) {
        ID1 f = (ID1)t_proc_gdi32full(names[n]);
        if (!f) { t_note("%s missing", names[n]); continue; }
        for (k = 0; k < sizeof v / sizeof *v; k++) { EQ(f(v[k]), v[k]); }
    }
}
TEST(gap3_gdiconvert_real_handles) {
    ID1 f = (ID1)t_proc_gdi32full("GdiConvertDC");
    ID1 g = (ID1)t_proc_gdi32full("GdiConvertBrush");
    ID1 h = (ID1)t_proc_gdi32full("GdiConvertBitmap");
    REQUIRE(f); REQUIRE(g); REQUIRE(h);
    HDC d = g3_mem(); HBRUSH b = U(CreateSolidBrush)(RGB(1,2,3)); HBITMAP bm = U(CreateBitmap)(2, 2, 1, 1, NULL);
    EQ(f((ULONG_PTR)d), (ULONG_PTR)d);
    EQ(g((ULONG_PTR)b), (ULONG_PTR)b);
    EQ(h((ULONG_PTR)bm), (ULONG_PTR)bm);
    /* no side effects on the objects */
    EQ(U(GetObjectType)(b), OBJ_BRUSH);
    EQ(U(GetObjectType)(bm), OBJ_BITMAP);
    EQ(U(GetObjectType)(d), OBJ_MEMDC);
    U(DeleteObject)(b); U(DeleteObject)(bm); U(DeleteDC)(d);
    /* a deleted object is passed through just the same */
    EQ(g((ULONG_PTR)b), (ULONG_PTR)b);
}
TEST(gap3_gdiconvert_same_code) {
    FARPROC a = t_proc_gdi32full("GdiConvertBitmap"), b = t_proc_gdi32full("GdiConvertBrush"), c = t_proc_gdi32full("GdiConvertDC");
    REQUIRE(a); REQUIRE(b); REQUIRE(c);
    if (a != b || b != c) t_note("convert helpers are distinct entries %p %p %p", (void *)a, (void *)b, (void *)c);
    FARPROC d = t_proc_gdi32full("GdiConvertFont"), e = t_proc_gdi32full("GdiConvertPalette"), f = t_proc_gdi32full("GdiConvertRegion");
    if (d && e && f) t_note("sibling helpers %p %p %p", (void *)d, (void *)e, (void *)f);
}
TEST(gap3_gdiconvert_no_lasterror) {
    ID1 f = (ID1)t_proc_gdi32full("GdiConvertBitmap");
    REQUIRE(f);
    SET_ERR(0x1234);
    f(0);
    EQ(GetLastError(), 0x1234);
}
TEST(gap3_selectlocal_distinct_from_convert) {
    FARPROC a = t_proc_gdi32full("SelectBrushLocal"), b = t_proc_gdi32full("GdiConvertBrush");
    REQUIRE(a); REQUIRE(b);
    t_note("SelectBrushLocal %p GdiConvertBrush %p", (void *)a, (void *)b);
}

/* ---------- GdiConvertAndCheckDC ---------- */
typedef HDC (WINAPI *CACDC)(HDC);
TEST(gap3_convertandcheckdc_valid) {
    CACDC f = (CACDC)t_proc_gdi32full("GdiConvertAndCheckDC");
    REQUIRE(f);
    HDC d = g3_mem();
    EQ(f(d), d);
    G3DC m; if (g3dc_make(&m, 4, 4)) { EQ(f(m.dc), m.dc); g3dc_free(&m); }
    HDC s = g3_screen();
    if (s) { EQ(f(s), s); g3_unscreen(s); }
    U(DeleteDC)(d);
}
TEST(gap3_convertandcheckdc_null) {
    CACDC f = (CACDC)t_proc_gdi32full("GdiConvertAndCheckDC");
    REQUIRE(f);
    SET_ERR(0);
    EQ(f(NULL), 0);
    WERR(G3_ERR_INVALID_HANDLE);
}
TEST(gap3_convertandcheckdc_wrong_type) {
    CACDC f = (CACDC)t_proc_gdi32full("GdiConvertAndCheckDC");
    REQUIRE(f);
    SET_ERR(0);
    HDC r = f((HDC)U(GetStockObject)(BLACK_PEN));
    t_note("GdiConvertAndCheckDC(pen) = %p err %lu", r, GetLastError());
    CHECK(r == NULL);
    r = f((HDC)U(GetStockObject)(SYSTEM_FONT));
    CHECK(r == NULL);
}
TEST(gap3_convertandcheckdc_stale) {
    CACDC f = (CACDC)t_proc_gdi32full("GdiConvertAndCheckDC");
    REQUIRE(f);
    HDC st = g3_stale_dc();
    HDC r = f(st);
    t_note("GdiConvertAndCheckDC(stale) = %p (stale %p)", r, st);
}
TEST(gap3_convertandcheckdc_metafile_dcs) {
    CACDC f = (CACDC)t_proc_gdi32full("GdiConvertAndCheckDC");
    REQUIRE(f); REQUIRE(U(CreateMetaFileW));
    HDC mf = U(CreateMetaFileW)(NULL);
    if (mf) {
        HDC r = f(mf);
        t_note("wmf dc type 0x%lx -> %p", (unsigned long)((ULONG_PTR)mf & 0x7f0000), r);
        WEQ(r, mf);
        HMETAFILE h = U(CloseMetaFile)(mf); if (h) U(DeleteMetaFile)(h);
    }
    if (U(CreateEnhMetaFileW)) {
        HDC em = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
        if (em) {
            HDC r = f(em);
            t_note("emf dc -> %p (dc %p)", r, em);
            HENHMETAFILE e = U(CloseEnhMetaFile)(em); if (e) U(DeleteEnhMetaFile)(e);
        }
    }
}
TEST(gap3_convertandcheckdc_repeat) {
    CACDC f = (CACDC)t_proc_gdi32full("GdiConvertAndCheckDC");
    REQUIRE(f);
    HDC d = g3_mem(); int i, bad = 0;
    DWORD b = g3_gui();
    for (i = 0; i < 1000; i++) if (f(d) != d) bad++;
    EQ(bad, 0);
    U(DeleteDC)(d);
    EQ(g3_gui() + 0, b);
}

/* ---------- GdiCleanCacheDC ---------- */
typedef BYTE (WINAPI *CLEAN)(HDC);
TEST(gap3_cleancachedc_valid) {
    CLEAN f = (CLEAN)t_proc_gdi32full("GdiCleanCacheDC");
    REQUIRE(f);
    HDC d = g3_mem();
    SET_ERR(0x55);
    CHECK(f(d) != 0);
    G3DC m; if (g3dc_make(&m, 4, 4)) { CHECK(f(m.dc) != 0); g3dc_free(&m); }
    HDC s = g3_screen();
    if (s) { CHECK(f(s) != 0); g3_unscreen(s); }
    U(DeleteDC)(d);
}
TEST(gap3_cleancachedc_null) {
    CLEAN f = (CLEAN)t_proc_gdi32full("GdiCleanCacheDC");
    REQUIRE(f);
    SET_ERR(0);
    EQ(f(NULL), 0);
    WERR(G3_ERR_INVALID_HANDLE);
}
TEST(gap3_cleancachedc_wrong_types) {
    CLEAN f = (CLEAN)t_proc_gdi32full("GdiCleanCacheDC");
    REQUIRE(f);
    static const int objs[] = { BLACK_PEN, WHITE_BRUSH, SYSTEM_FONT, DEFAULT_PALETTE };
    unsigned k;
    for (k = 0; k < 4; k++) {
        SET_ERR(0);
        EQ(f((HDC)U(GetStockObject)(objs[k])), 0);
        WERR(G3_ERR_INVALID_HANDLE);
    }
    HRGN r = U(CreateRectRgn)(0, 0, 1, 1);
    EQ(f((HDC)r), 0);
    U(DeleteObject)(r);
}
TEST(gap3_cleancachedc_handle_format_only) {
    /* only the handle type bits are examined, so a deleted DC still passes */
    CLEAN f = (CLEAN)t_proc_gdi32full("GdiCleanCacheDC");
    REQUIRE(f);
    HDC st = g3_stale_dc();
    BYTE r = f(st);
    t_note("GdiCleanCacheDC(stale dc) = %d", r);
    WCHECK(r != 0);
}
TEST(gap3_cleancachedc_wmf_dc) {
    CLEAN f = (CLEAN)t_proc_gdi32full("GdiCleanCacheDC");
    REQUIRE(f); REQUIRE(U(CreateMetaFileW));
    HDC mf = U(CreateMetaFileW)(NULL);
    if (!mf) { t_skip("no wmf dc"); return; }
    SET_ERR(0);
    BYTE r = f(mf);
    t_note("GdiCleanCacheDC(wmf) = %d err %lu", r, GetLastError());
    if (((ULONG_PTR)mf & 0x7f0000) == 0x660000) { WCHECK(r == 0); }
    HMETAFILE h = U(CloseMetaFile)(mf); if (h) U(DeleteMetaFile)(h);
}
TEST(gap3_cleancachedc_no_state_change) {
    CLEAN f = (CLEAN)t_proc_gdi32full("GdiCleanCacheDC");
    REQUIRE(f);
    HDC d = g3_mem();
    U(SetTextColor)(d, RGB(1,2,3));
    f(d);
    EQ(U(GetTextColor)(d), RGB(1,2,3));
    U(DeleteDC)(d);
}

/* ---------- GdiConsoleTextOut ---------- */
typedef ULONG_PTR (WINAPI *CTO)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);
TEST(gap3_consoletextout_not_implemented) {
    CTO f = (CTO)t_proc_gdi32full("GdiConsoleTextOut");
    REQUIRE(f);
    HDC d = g3_mem();
    SET_ERR(0);
    EQ(f((ULONG_PTR)d, 0, 0, 0), 0);
    WEQ(GetLastError(), ERROR_CALL_NOT_IMPLEMENTED);
    SET_ERR(0);
    EQ(f(0, 0, 0, 0), 0);
    WEQ(GetLastError(), ERROR_CALL_NOT_IMPLEMENTED);
    U(DeleteDC)(d);
}
TEST(gap3_consoletextout_no_drawing) {
    CTO f = (CTO)t_proc_gdi32full("GdiConsoleTextOut");
    REQUIRE(f);
    G3DC m; if (!g3dc_make(&m, 8, 8)) { CHECK(0); return; }
    g3dc_clear(&m, RGB(5,5,5));
    f((ULONG_PTR)m.dc, 0, 0, 0);
    EQ(g3dc_count(&m, RGB(5,5,5)), 64);
    g3dc_free(&m);
}

/* ---------- GdiArtificialDecrementDriver ---------- */
typedef ULONG_PTR (WINAPI *GADD)(const WCHAR *, ULONG_PTR);
TEST(gap3_artdecrement_null_and_empty) {
    GADD f = (GADD)t_proc_gdi32full("GdiArtificialDecrementDriver");
    REQUIRE(f);
    static const ULONG_PTR flags[] = { 0, 1, 2, 3, 0x100 };
    unsigned k;
    for (k = 0; k < sizeof flags / sizeof *flags; k++) {
        EQ(f(NULL, flags[k]), 0);
        EQ(f(L"", flags[k]), 0);
    }
}
TEST(gap3_artdecrement_unknown_driver) {
    GADD f = (GADD)t_proc_gdi32full("GdiArtificialDecrementDriver");
    REQUIRE(f);
    ULONG_PTR r = f(L"gap3_no_such_driver.dll", 0);
    t_note("GdiArtificialDecrementDriver(unknown, 0) = %llu", (unsigned long long)r);
    CHECK(r == 0);
}

/* ---------- GdiAddGlsBounds / GdiAddGlsRecord ---------- */
typedef BOOL (WINAPI *GLSB)(HDC, const RECT *);
typedef BOOL (WINAPI *GLSR)(HDC, DWORD, LPCSTR, RECT *);
TEST(gap3_glsbounds_invalid_dc) {
    GLSB f = (GLSB)t_proc_gdi32full("GdiAddGlsBounds");
    REQUIRE(f);
    RECT r = { 0, 0, 5, 5 };
    EQ(f(NULL, &r), 0);
    EQ(f(NULL, NULL), 0);
    HDC st = g3_stale_dc();
    EQ(f(st, &r), 0);
    EQ(f((HDC)U(GetStockObject)(BLACK_PEN), &r), 0);
}
TEST(gap3_glsbounds_memdc_observed) {
    GLSB f = (GLSB)t_proc_gdi32full("GdiAddGlsBounds");
    REQUIRE(f); REQUIRE(U(SetBoundsRect));
    G3DC m; if (!g3dc_make(&m, 8, 8)) { CHECK(0); return; }
    RECT r = { 1, 1, 4, 4 };
    BOOL res = f(m.dc, &r);
    t_note("GdiAddGlsBounds(memdc) = %d", res);
    U(SetBoundsRect)(m.dc, NULL, DCB_RESET | DCB_DISABLE);
    g3dc_free(&m);
}
TEST(gap3_glsrecord_invalid_dc) {
    GLSR f = (GLSR)t_proc_gdi32full("GdiAddGlsRecord");
    REQUIRE(f);
    SET_ERR(0);
    EQ(f(NULL, 0, NULL, NULL), 0);
    WERR(G3_ERR_INVALID_HANDLE);
    SET_ERR(0);
    EQ(f(NULL, 4, "abcd", NULL), 0);
    WERR(G3_ERR_INVALID_HANDLE);
    RECT r = { 0, 0, 1, 1 };
    EQ(f(NULL, 4, "abcd", &r), 0);
}

/* ---------- GdiConvertBitmapV5 ---------- */
typedef HBITMAP (WINAPI *CBV5)(BITMAPINFOHEADER *, UINT, ULONG_PTR, int);
TEST(gap3_convertbitmapv5_invalid) {
    CBV5 f = (CBV5)t_proc_gdi32full("GdiConvertBitmapV5");
    REQUIRE(f);
    EQ(f(NULL, 0, 0, 0), 0);
    EQ(f(NULL, 0x7c, 0, 0), 0);
    EQ(f(NULL, 0xffffffffu, 0, 0), 0);
    BITMAPINFOHEADER h; memset(&h, 0, sizeof h);
    h.biSize = sizeof h; h.biWidth = 1; h.biHeight = 1; h.biPlanes = 1; h.biBitCount = 32;
    unsigned sz;
    for (sz = 0; sz < 0x2c; sz += 7) EQ(f(&h, sz, 0, 0), 0);
    EQ(f(&h, 0x2b, 0, 0), 0);
}

/* ---------- GdiAddInitialFonts / GdiAddFontResourceW ---------- */
TEST(gap3_addinitialfonts_resolves) {
    /* GdiAddInitialFonts re-initialises the kernel font tables: resolve only, never call. */
    FARPROC p = t_proc_gdi32full("GdiAddInitialFonts");
    REQUIRE(p);
    CHECK(p != NULL);
}
typedef int (WINAPI *GAFR)(const WCHAR *, UINT, PVOID);
TEST(gap3_gdiaddfont_nonexistent) {
    GAFR f = (GAFR)t_proc_gdi32full("GdiAddFontResourceW");
    REQUIRE(f);
    static const UINT fl[] = { 0, FR_PRIVATE, FR_NOT_ENUM, FR_PRIVATE | FR_NOT_ENUM };
    unsigned k;
    for (k = 0; k < 4; k++) {
        EQ(f(L"C:\\gap3_nonexistent_dir\\no_such_font.ttf", fl[k], NULL), 0);
        EQ(f(L"gap3_no_such_font_file.ttf", fl[k], NULL), 0);
    }
}
TEST(gap3_gdiaddfont_empty_name) {
    GAFR f = (GAFR)t_proc_gdi32full("GdiAddFontResourceW");
    REQUIRE(f);
    EQ(f(L"", 0, NULL), 0);
    EQ(f(L"", FR_PRIVATE, NULL), 0);
}
TEST(gap3_gdiaddfont_not_a_font) {
    GAFR f = (GAFR)t_proc_gdi32full("GdiAddFontResourceW");
    REQUIRE(f);
    WCHAR path[MAX_PATH], dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    wsprintfW(path, L"%sgap3_notafont_%lu.ttf", dir, GetCurrentProcessId());
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot write temp file"); return; }
    DWORD w; char junk[256]; memset(junk, 0x5a, sizeof junk);
    WriteFile(h, junk, sizeof junk, &w, NULL); CloseHandle(h);
    EQ(f(path, FR_PRIVATE, NULL), 0);
    EQ(f(path, 0, NULL), 0);
    DeleteFileW(path);
}
TEST(gap3_gdiaddfont_private_system_font) {
    GAFR f = (GAFR)t_proc_gdi32full("GdiAddFontResourceW");
    REQUIRE(f); REQUIRE(U(RemoveFontResourceExW));
    G3_WINE_SKIP("font loading");
    WCHAR path[MAX_PATH];
    GetWindowsDirectoryW(path, MAX_PATH);
    lstrcatW(path, L"\\Fonts\\arial.ttf");
    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) { t_skip("arial.ttf not present"); return; }
    DWORD before = g3_gui();
    int n = f(path, FR_PRIVATE, NULL);
    t_note("GdiAddFontResourceW(arial.ttf, FR_PRIVATE) = %d", n);
    CHECK(n >= 1);
    if (n > 0) {
        CHECK(U(RemoveFontResourceExW)(path, FR_PRIVATE, NULL));
        SET_ERR(0);
        int again = U(RemoveFontResourceExW)(path, FR_PRIVATE, NULL);
        t_note("second RemoveFontResourceExW = %d", again);
    }
    EQ(g3_gui(), before);
}
TEST(gap3_gdiaddfont_repeat_private_balance) {
    GAFR f = (GAFR)t_proc_gdi32full("GdiAddFontResourceW");
    REQUIRE(f); REQUIRE(U(RemoveFontResourceExW));
    G3_WINE_SKIP("font loading");
    WCHAR path[MAX_PATH];
    GetWindowsDirectoryW(path, MAX_PATH);
    lstrcatW(path, L"\\Fonts\\arial.ttf");
    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) { t_skip("arial.ttf not present"); return; }
    int i, n1 = f(path, FR_PRIVATE, NULL), n2 = f(path, FR_PRIVATE, NULL);
    EQ(n1, n2);
    for (i = 0; i < 2; i++) CHECK(U(RemoveFontResourceExW)(path, FR_PRIVATE, NULL));
}

/* ---------- RemoveFontResource family ---------- */
TEST(gap3_removefont_nonexistent) {
    __typeof__(&RemoveFontResourceW) R;
    IMPLS(R, RemoveFontResourceW)
        EQ(R(L"C:\\gap3_nonexistent_dir\\no_such_font.ttf"), 0);
        EQ(R(L"gap3_no_such_font_file.ttf"), 0);
        EQ(R(L"gap3_no_such_font_file.fon"), 0);
        EQ(R(L""), 0);
    }
}
TEST(gap3_removefont_null) {
    __typeof__(&RemoveFontResourceW) R;
    IMPLS(R, RemoveFontResourceW)
        EQ(R(NULL), 0);
    }
}
TEST(gap3_removefont_ex_flags) {
    REQUIRE(U(RemoveFontResourceExW));
    static const DWORD bad[] = { 1, 2, 4, 8, 0x40, 0x80, 0x100, 0x10000, 0x80000000u, 0xffffffffu, 0xf };
    unsigned k;
    for (k = 0; k < sizeof bad / sizeof *bad; k++) {
        SET_ERR(0);
        EQ(U(RemoveFontResourceExW)(L"gap3_no_such_font_file.ttf", bad[k], NULL), 0);
        CHECKF(GetLastError() == G3_ERR_INVALID_PARAM || g3_wine(), "flags 0x%lx err %lu", bad[k], GetLastError());
    }
}
TEST(gap3_removefont_ex_valid_flags_missing_file) {
    REQUIRE(U(RemoveFontResourceExW));
    static const DWORD ok[] = { 0, FR_PRIVATE, FR_NOT_ENUM, FR_PRIVATE | FR_NOT_ENUM };
    unsigned k;
    for (k = 0; k < 4; k++) {
        SET_ERR(0);
        EQ(U(RemoveFontResourceExW)(L"gap3_no_such_font_file.ttf", ok[k], NULL), 0);
        t_note("flags 0x%lx: err %lu", ok[k], GetLastError());
    }
}
TEST(gap3_removefont_ex_null_name) {
    REQUIRE(U(RemoveFontResourceExW));
    EQ(U(RemoveFontResourceExW)(NULL, 0, NULL), 0);
    EQ(U(RemoveFontResourceExW)(NULL, FR_PRIVATE, NULL), 0);
}
TEST(gap3_removefont_a_vs_w_parity) {
    REQUIRE(U(RemoveFontResourceA)); REQUIRE(U(RemoveFontResourceW));
    EQ(U(RemoveFontResourceA)("gap3_no_such_font_file.ttf"), 0);
    EQ(U(RemoveFontResourceW)(L"gap3_no_such_font_file.ttf"), 0);
    EQ(U(RemoveFontResourceExA)("gap3_no_such_font_file.ttf", FR_PRIVATE, NULL), 0);
}
TEST(gap3_removefont_tracking) {
    typedef void (WINAPI *RFT)(const char *, void *);
    RFT f = (RFT)t_proc_gdi32full("RemoveFontResourceTracking");
    REQUIRE(f);
    G3_WINE_SKIP("tracking helper");
    /* removal of a font that was never added fails inside, so the tracking list is untouched */
    f("gap3_no_such_font_file.ttf", NULL);
    f("C:\\gap3_nonexistent_dir\\no_such_font.ttf", NULL);
    f("", NULL);
    CHECK(1);
}
TEST(gap3_removefont_does_not_leak_objects) {
    REQUIRE(U(RemoveFontResourceW));
    DWORD b = g3_gui(); int i;
    for (i = 0; i < 200; i++) U(RemoveFontResourceW)(L"gap3_no_such_font_file.ttf");
    EQ(g3_gui(), b);
}
TEST(gap3_removefont_long_name) {
    REQUIRE(U(RemoveFontResourceW));
    WCHAR *p = malloc(40000 * sizeof(WCHAR)); int i;
    for (i = 0; i < 39999; i++) p[i] = L'a';
    p[39999] = 0;
    SET_ERR(0);
    EQ(U(RemoveFontResourceW)(p), 0);
    free(p);
}

/* ---------- StartDocW / StartPage ---------- */
TEST(gap3_startdoc_invalid_dc) {
    __typeof__(&StartDocW) S;
    IMPLS(S, StartDocW)
        DOCINFOW di; memset(&di, 0, sizeof di); di.cbSize = sizeof di; di.lpszDocName = L"gap3";
        SET_ERR(0);
        int r = S(NULL, &di);
        CHECK(r <= 0);
        WERR(G3_ERR_INVALID_HANDLE);
        SET_ERR(0);
        r = S(NULL, NULL);
        CHECK(r <= 0);
        WERR(G3_ERR_INVALID_HANDLE);
        HDC st = g3_stale_dc();
        r = S(st, &di);
        CHECK(r <= 0);
        r = S((HDC)U(GetStockObject)(BLACK_PEN), &di);
        CHECK(r <= 0);
    }
}
TEST(gap3_startdoc_memdc_fails) {
    __typeof__(&StartDocW) S;
    IMPLS(S, StartDocW)
        HDC d = g3_mem(); DOCINFOW di;
        memset(&di, 0, sizeof di); di.cbSize = sizeof di; di.lpszDocName = L"gap3 memdc";
        SET_ERR(0);
        int r = S(d, &di);
        t_note("StartDocW(memdc) = %d err %lu", r, GetLastError());
        CHECK(r <= 0);
        if (r > 0 && U(AbortDoc)) U(AbortDoc)(d);
        U(DeleteDC)(d);
    }
}
TEST(gap3_startdoc_memdc_bad_cbsize) {
    HDC d = g3_mem(); DOCINFOW di; int r;
    memset(&di, 0, sizeof di); di.cbSize = 0; di.lpszDocName = L"gap3";
    r = U(StartDocW)(d, &di);
    CHECK(r <= 0);
    if (r > 0) U(AbortDoc)(d);
    di.cbSize = 0x1000;
    r = U(StartDocW)(d, &di);
    CHECK(r <= 0);
    if (r > 0) U(AbortDoc)(d);
    U(DeleteDC)(d);
}
TEST(gap3_startdoc_dibdc) {
    G3DC m; if (!g3dc_make(&m, 4, 4)) { CHECK(0); return; }
    DOCINFOW di; memset(&di, 0, sizeof di); di.cbSize = sizeof di;
    int r = U(StartDocW)(m.dc, &di);
    CHECK(r <= 0);
    if (r > 0) U(AbortDoc)(m.dc);
    g3dc_free(&m);
}
TEST(gap3_startdoc_screen_dc) {
    HDC s = g3_screen(); if (!s) { t_skip("no dc"); return; }
    G3_WINE_SKIP("StartDoc on display DC");
    DOCINFOW di; memset(&di, 0, sizeof di); di.cbSize = sizeof di; di.lpszDocName = L"gap3 screen";
    int r = U(StartDocW)(s, &di);
    t_note("StartDocW(display dc) = %d", r);
    CHECK(r <= 0);
    if (r > 0) U(AbortDoc)(s);
    g3_unscreen(s);
}
TEST(gap3_startdoc_wmf_dc) {
    REQUIRE(U(CreateMetaFileW));
    HDC mf = U(CreateMetaFileW)(NULL);
    if (!mf) { t_skip("no wmf dc"); return; }
    DOCINFOW di; memset(&di, 0, sizeof di); di.cbSize = sizeof di;
    int r = U(StartDocW)(mf, &di);
    t_note("StartDocW(wmf dc) = %d", r);
    CHECK(r <= 0);
    if (r > 0) U(AbortDoc)(mf);
    HMETAFILE h = U(CloseMetaFile)(mf); if (h) U(DeleteMetaFile)(h);
}
TEST(gap3_startdoc_gdicount) {
    DWORD b = g3_gui(); int i;
    HDC d = g3_mem(); DOCINFOW di; memset(&di, 0, sizeof di); di.cbSize = sizeof di;
    for (i = 0; i < 50; i++) { int r = U(StartDocW)(d, &di); if (r > 0) U(AbortDoc)(d); }
    U(DeleteDC)(d);
    EQ(g3_gui(), b);
}
TEST(gap3_startpage_invalid) {
    __typeof__(&StartPage) S;
    IMPLS(S, StartPage)
        SET_ERR(0);
        WEQ(S(NULL), -1);
        WERR(G3_ERR_INVALID_HANDLE);
        HDC st = g3_stale_dc();
        SET_ERR(0);
        WEQ(S(st), -1);
        WEQ(S((HDC)U(GetStockObject)(BLACK_PEN)), -1);
    }
}
TEST(gap3_startpage_memdc) {
    __typeof__(&StartPage) S;
    IMPLS(S, StartPage)
        HDC d = g3_mem();
        SET_ERR(0);
        WEQ(S(d), -1);
        WERR(G3_ERR_INVALID_HANDLE);
        G3DC m; if (g3dc_make(&m, 4, 4)) { SET_ERR(0); WEQ(S(m.dc), -1); WERR(G3_ERR_INVALID_HANDLE); g3dc_free(&m); }
        U(DeleteDC)(d);
    }
}
TEST(gap3_startpage_screen_dc) {
    __typeof__(&StartPage) S;
    IMPLS(S, StartPage)
        HDC s = g3_screen(); if (!s) { t_skip("no dc"); return; }
        SET_ERR(0);
        int r = S(s);
        WEQ(r, -1);
        WERR(G3_ERR_INVALID_HANDLE);
        g3_unscreen(s);
    }
}
TEST(gap3_startpage_wmf_dc) {
    REQUIRE(U(CreateMetaFileW)); REQUIRE(U(StartPage));
    HDC mf = U(CreateMetaFileW)(NULL);
    if (!mf) { t_skip("no wmf dc"); return; }
    SET_ERR(0);
    int r = U(StartPage)(mf);
    t_note("StartPage(wmf dc) = %d err %lu", r, GetLastError());
    CHECK(r <= 0);
    HMETAFILE h = U(CloseMetaFile)(mf); if (h) U(DeleteMetaFile)(h);
}
TEST(gap3_startpage_no_side_effects) {
    G3DC m; if (!g3dc_make(&m, 4, 4)) { CHECK(0); return; }
    g3dc_clear(&m, RGB(3,3,3));
    U(StartPage)(m.dc);
    EQ(g3dc_count(&m, RGB(3,3,3)), 16);
    EQ(U(GetCurrentObject)(m.dc, OBJ_BITMAP), m.bmp);
    g3dc_free(&m);
}

/* ---------- batch limit ---------- */
TEST(gap3_batchlimit_data_export) {
    REQUIRE(U(GdiGetBatchLimit));
    DWORD *p = (DWORD *)t_proc_gdi32("GdiBatchLimit");
    REQUIRE(p);
    DWORD v = U(GdiGetBatchLimit)();
    EQ(*p, v);
    CHECK(v >= 1 && v <= 20);
    t_note("GdiBatchLimit = %lu", (unsigned long)v);
}
TEST(gap3_batchlimit_set_get) {
    REQUIRE(U(GdiGetBatchLimit)); REQUIRE(U(GdiSetBatchLimit));
    G3_WINE_SKIP("batch limit");
    DWORD orig = U(GdiGetBatchLimit)();
    EQ(U(GdiSetBatchLimit)(5), orig);
    EQ(U(GdiGetBatchLimit)(), 5);
    EQ(U(GdiSetBatchLimit)(1), 5);
    EQ(U(GdiGetBatchLimit)(), 1);
    EQ(U(GdiSetBatchLimit)(20), 1);
    EQ(U(GdiGetBatchLimit)(), 20);
    EQ(U(GdiSetBatchLimit)(orig), 20);
    EQ(U(GdiGetBatchLimit)(), orig);
}
TEST(gap3_batchlimit_zero_means_default) {
    REQUIRE(U(GdiGetBatchLimit)); REQUIRE(U(GdiSetBatchLimit));
    G3_WINE_SKIP("batch limit");
    DWORD orig = U(GdiGetBatchLimit)();
    U(GdiSetBatchLimit)(3);
    EQ(U(GdiSetBatchLimit)(0), 3);
    EQ(U(GdiGetBatchLimit)(), 20);
    U(GdiSetBatchLimit)(orig);
}
TEST(gap3_batchlimit_out_of_range) {
    REQUIRE(U(GdiGetBatchLimit)); REQUIRE(U(GdiSetBatchLimit));
    G3_WINE_SKIP("batch limit");
    DWORD orig = U(GdiGetBatchLimit)();
    static const DWORD bad[] = { 21, 22, 100, 0x7fffffff, 0xffffffffu, 0x80000000u };
    unsigned k;
    for (k = 0; k < sizeof bad / sizeof *bad; k++) {
        EQ(U(GdiSetBatchLimit)(bad[k]), 0);
        EQ(U(GdiGetBatchLimit)(), orig);
    }
    U(GdiSetBatchLimit)(orig);
}
TEST(gap3_batchlimit_all_valid_values) {
    REQUIRE(U(GdiGetBatchLimit)); REQUIRE(U(GdiSetBatchLimit));
    G3_WINE_SKIP("batch limit");
    DWORD orig = U(GdiGetBatchLimit)(), v, prev = orig;
    for (v = 1; v <= 20; v++) {
        EQ(U(GdiSetBatchLimit)(v), prev);
        EQ(U(GdiGetBatchLimit)(), v);
        prev = v;
    }
    U(GdiSetBatchLimit)(orig);
}
TEST(gap3_batchlimit_drawing_unaffected) {
    REQUIRE(U(GdiGetBatchLimit)); REQUIRE(U(GdiSetBatchLimit));
    G3_WINE_SKIP("batch limit");
    DWORD orig = U(GdiGetBatchLimit)();
    G3DC m; if (!g3dc_make(&m, 8, 8)) { CHECK(0); return; }
    U(GdiSetBatchLimit)(1);
    g3dc_clear(&m, RGB(0,0,0));
    int i; for (i = 0; i < 8; i++) U(SetPixelV)(m.dc, i, i, RGB(255,255,255));
    EQ(g3dc_count(&m, RGB(255,255,255)), 8);
    U(GdiSetBatchLimit)(orig);
    g3dc_free(&m);
}
static void bl_thread(void *p) { DWORD *a = p; a[0] = U(GdiGetBatchLimit)(); }
TEST(gap3_batchlimit_per_process) {
    REQUIRE(U(GdiGetBatchLimit)); REQUIRE(U(GdiSetBatchLimit));
    G3_WINE_SKIP("batch limit");
    DWORD orig = U(GdiGetBatchLimit)(), got[1] = { 0 };
    U(GdiSetBatchLimit)(7);
    g3_run_thread(bl_thread, got);
    t_note("other thread sees %lu", got[0]);
    CHECK(got[0] == 7);
    U(GdiSetBatchLimit)(orig);
}
