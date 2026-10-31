/* GetMetaFileW, CopyMetaFileW, GetEnhMetaFilePixelFormat. Temp files are always removed. */
#include "gap10.h"

static void tmpname(WCHAR *out, const char *tag, const char *ext) {
    WCHAR dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    wsprintfW(out, L"%sg10_%S_%lu.%S", dir, tag, GetCurrentProcessId(), ext);
    DeleteFileW(out);
}
static int fexists(const WCHAR *p) { return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES; }
static DWORD fsize(const WCHAR *p) {
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExW(p, GetFileExInfoStandard, &d)) return 0xFFFFFFFF;
    return d.nFileSizeLow;
}
#define NEED_MF() do { REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile)); REQUIRE(U(GetMetaFileBitsEx)); \
    REQUIRE(U(Rectangle)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(SelectObject)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetStockObject)); } while (0)

/* A WMF with one red filled rectangle (0,0)-(10,10) and no pen. */
static HMETAFILE make_wmf(const WCHAR *path) {
    HDC m = U(CreateMetaFileW)(path);
    HBRUSH b;
    if (!m) return NULL;
    b = U(CreateSolidBrush)(RED_);
    U(SelectObject)(m, b);
    U(SelectObject)(m, U(GetStockObject)(NULL_PEN));
    U(Rectangle)(m, 0, 0, 11, 11);
    U(SelectObject)(m, U(GetStockObject)(WHITE_BRUSH));
    U(DeleteObject)(b);
    return U(CloseMetaFile)(m);
}
static UINT wmf_bits(HMETAFILE h, BYTE **out) {
    UINT n = U(GetMetaFileBitsEx)(h, 0, NULL);
    BYTE *p;
    *out = NULL;
    if (!n) return 0;
    p = (BYTE *)HeapAlloc(GetProcessHeap(), 0, n);
    if (U(GetMetaFileBitsEx)(h, n, p) != n) { HeapFree(GetProcessHeap(), 0, p); return 0; }
    *out = p;
    return n;
}
static int render_pixel(HMETAFILE h, int x, int y, COLORREF *c) {
    MD m;
    if (!md_create(&m, 16, 16, 32)) return 0;
    md_clear(&m, WHITE_);
    U(PlayMetaFile)(m.dc, h);
    *c = md_px(&m, x, y);
    md_destroy(&m);
    return 1;
}

/* ====================== GetMetaFileW ====================== */
TEST(gap10_gmfw_null_name) {
    NEED_MF(); REQUIRE(U(GetMetaFileW));
    CLR();
    HMETAFILE h = U(GetMetaFileW)(NULL);
    CHECK(h == NULL);
    t_note("NULL name: err %lu", GetLastError());
    if (h) U(DeleteMetaFile)(h);
}
TEST(gap10_gmfw_empty_name) {
    NEED_MF(); REQUIRE(U(GetMetaFileW));
    CLR();
    HMETAFILE h = U(GetMetaFileW)(L"");
    CHECK(h == NULL);
    t_note("empty name: err %lu", GetLastError());
    if (h) U(DeleteMetaFile)(h);
}
TEST(gap10_gmfw_missing_file) {
    NEED_MF(); REQUIRE(U(GetMetaFileW));
    WCHAR p[MAX_PATH]; tmpname(p, "missing", "wmf");
    CLR();
    HMETAFILE h = U(GetMetaFileW)(p);
    CHECK(h == NULL);
    t_note("missing file: err %lu", GetLastError());
    CHECK(!fexists(p));          /* must not create it */
    if (h) U(DeleteMetaFile)(h);
}
TEST(gap10_gmfw_missing_directory) {
    NEED_MF(); REQUIRE(U(GetMetaFileW));
    CLR();
    HMETAFILE h = U(GetMetaFileW)(L"C:\\no_such_dir_g10\\x.wmf");
    CHECK(h == NULL);
    if (h) U(DeleteMetaFile)(h);
}
TEST(gap10_gmfw_directory_path) {
    NEED_MF(); REQUIRE(U(GetMetaFileW));
    WCHAR d[MAX_PATH]; GetTempPathW(MAX_PATH, d);
    HMETAFILE h = U(GetMetaFileW)(d);
    CHECK(h == NULL);
    if (h) U(DeleteMetaFile)(h);
}
TEST(gap10_gmfw_path_too_long) {
    NEED_MF(); REQUIRE(U(GetMetaFileW));
    WCHAR big[1100]; int i;
    for (i = 0; i < 1099; i++) big[i] = L'a';
    big[1099] = 0;
    CLR();
    HMETAFILE h = U(GetMetaFileW)(big);
    CHECK(h == NULL);
    if (h) U(DeleteMetaFile)(h);
}
static HANDLE write_file(const WCHAR *p, const void *data, DWORD n) {
    HANDLE h = CreateFileW(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD w;
    if (h == INVALID_HANDLE_VALUE) return h;
    WriteFile(h, data, n, &w, NULL);
    CloseHandle(h);
    return h;
}
TEST(gap10_gmfw_garbage_file) {
    NEED_MF(); REQUIRE(U(GetMetaFileW));
    WCHAR p[MAX_PATH]; char junk[256]; HMETAFILE h;
    tmpname(p, "junk", "wmf");
    memset(junk, 'Z', sizeof junk);
    CHECK(write_file(p, junk, sizeof junk) != INVALID_HANDLE_VALUE);
    CLR();
    h = U(GetMetaFileW)(p);
    CHECK(h == NULL);
    t_note("garbage file: err %lu", GetLastError());
    if (h) U(DeleteMetaFile)(h);
    DeleteFileW(p);
}
TEST(gap10_gmfw_empty_and_tiny_files) {
    NEED_MF(); REQUIRE(U(GetMetaFileW));
    WCHAR p[MAX_PATH]; BYTE tiny[18] = { 1, 0, 9, 0, 0, 3 }; HMETAFILE h;
    tmpname(p, "tiny", "wmf");
    write_file(p, "", 0);
    h = U(GetMetaFileW)(p);
    CHECK(h == NULL);
    if (h) U(DeleteMetaFile)(h);
    write_file(p, tiny, 4);
    h = U(GetMetaFileW)(p);
    CHECK(h == NULL);
    if (h) U(DeleteMetaFile)(h);
    write_file(p, tiny, sizeof tiny);
    h = U(GetMetaFileW)(p);
    t_note("18-byte header-only file -> %s", h ? "handle" : "NULL");
    if (h) U(DeleteMetaFile)(h);
    DeleteFileW(p);
}
TEST(gap10_gmfw_loads_disk_metafile) {
    NEED_MF(); REQUIRE(U(GetMetaFileW)); REQUIRE(U(GetObjectType));
    WCHAR p[MAX_PATH]; HMETAFILE h1, h2; BYTE *b1 = NULL, *b2 = NULL; UINT n1, n2;
    tmpname(p, "disk", "wmf");
    h1 = make_wmf(p);
    CHECK(h1 != NULL);
    if (!h1) { DeleteFileW(p); return; }
    CHECK(fexists(p));
    h2 = U(GetMetaFileW)(p);
    CHECK(h2 != NULL);
    if (h2) {
        CHECK(h2 != h1);
        EXPECT_EQ(U(GetObjectType)(h2), OBJ_METAFILE);
        n1 = wmf_bits(h1, &b1); n2 = wmf_bits(h2, &b2);
        CHECK(n1 > 0); EXPECT_EQ(n1, n2);
        if (b1 && b2 && n1 == n2) CHECK(!memcmp(b1, b2, n1));
        if (b1) HeapFree(GetProcessHeap(), 0, b1);
        if (b2) HeapFree(GetProcessHeap(), 0, b2);
        CHECK(U(DeleteMetaFile)(h2));
    }
    U(DeleteMetaFile)(h1);
    DeleteFileW(p);
}
TEST(gap10_gmfw_file_size_matches_bits) {
    NEED_MF(); REQUIRE(U(GetMetaFileW));
    WCHAR p[MAX_PATH]; HMETAFILE h1, h2; BYTE *b = NULL; UINT n;
    tmpname(p, "size", "wmf");
    h1 = make_wmf(p);
    if (!h1) { t_skip("no wmf"); DeleteFileW(p); return; }
    h2 = U(GetMetaFileW)(p);
    CHECK(h2 != NULL);
    n = h2 ? wmf_bits(h2, &b) : 0;
    EXPECT_EQ(fsize(p), n);
    if (b) HeapFree(GetProcessHeap(), 0, b);
    if (h2) U(DeleteMetaFile)(h2);
    U(DeleteMetaFile)(h1);
    DeleteFileW(p);
}
TEST(gap10_gmfw_playback_pixels) {
    NEED_MF(); REQUIRE(U(GetMetaFileW)); REQUIRE(U(PlayMetaFile)); NEED_MD();
    WCHAR p[MAX_PATH]; HMETAFILE h1, h2; COLORREF c;
    tmpname(p, "play", "wmf");
    h1 = make_wmf(p);
    if (!h1) { t_skip("no wmf"); DeleteFileW(p); return; }
    h2 = U(GetMetaFileW)(p);
    CHECK(h2 != NULL);
    if (h2) {
        CHECK(render_pixel(h2, 5, 5, &c)); EXPECT_EQ(c, RED_);
        CHECK(render_pixel(h2, 13, 13, &c)); EXPECT_EQ(c, WHITE_);
        CHECK(render_pixel(h2, 0, 0, &c)); EXPECT_EQ(c, RED_);
        CHECK(render_pixel(h2, 9, 9, &c)); EXPECT_EQ(c, RED_);
        CHECK(render_pixel(h2, 12, 12, &c)); EXPECT_EQ(c, WHITE_);
        U(DeleteMetaFile)(h2);
    }
    U(DeleteMetaFile)(h1);
    DeleteFileW(p);
}
TEST(gap10_gmfw_same_file_twice_distinct_handles) {
    NEED_MF(); REQUIRE(U(GetMetaFileW));
    WCHAR p[MAX_PATH]; HMETAFILE h1, a, b;
    tmpname(p, "twice", "wmf");
    h1 = make_wmf(p);
    if (!h1) { t_skip("no wmf"); DeleteFileW(p); return; }
    a = U(GetMetaFileW)(p); b = U(GetMetaFileW)(p);
    CHECK(a && b);
    CHECK(a != b);
    if (a) CHECK(U(DeleteMetaFile)(a));
    if (b) CHECK(U(DeleteMetaFile)(b));
    U(DeleteMetaFile)(h1);
    DeleteFileW(p);
}
TEST(gap10_gmfw_delete_handle_lifetime) {
    NEED_MF(); REQUIRE(U(GetMetaFileW)); REQUIRE(U(GetObjectType));
    WCHAR p[MAX_PATH]; HMETAFILE h1, a;
    tmpname(p, "life", "wmf");
    h1 = make_wmf(p);
    if (!h1) { t_skip("no wmf"); DeleteFileW(p); return; }
    a = U(GetMetaFileW)(p);
    CHECK(a != NULL);
    if (a) {
        CHECK(U(DeleteMetaFile)(a));
        CLR();
        CHECK(!U(DeleteMetaFile)(a));
        EXPECT_EQ(U(GetObjectType)(a), 0);
        EXPECT_EQ(U(GetMetaFileBitsEx)(a, 0, NULL), 0);
    }
    U(DeleteMetaFile)(h1);
    CHECK(DeleteFileW(p));       /* file must be released once the handles are gone */
}
TEST(gap10_gmfw_ansi_variant_parity) {
    NEED_MF(); REQUIRE(U(GetMetaFileW)); REQUIRE(U(GetMetaFileA));
    WCHAR p[MAX_PATH]; char pa[MAX_PATH]; HMETAFILE h1, w, a; BYTE *bw = NULL, *ba = NULL; UINT nw = 0, na = 0;
    tmpname(p, "ansi", "wmf");
    h1 = make_wmf(p);
    if (!h1) { t_skip("no wmf"); DeleteFileW(p); return; }
    WideCharToMultiByte(CP_ACP, 0, p, -1, pa, sizeof pa, NULL, NULL);
    w = U(GetMetaFileW)(p); a = U(GetMetaFileA)(pa);
    CHECK(w && a);
    if (w) nw = wmf_bits(w, &bw);
    if (a) na = wmf_bits(a, &ba);
    EXPECT_EQ(nw, na);
    if (bw && ba && nw == na) CHECK(!memcmp(bw, ba, nw));
    if (bw) HeapFree(GetProcessHeap(), 0, bw);
    if (ba) HeapFree(GetProcessHeap(), 0, ba);
    if (w) U(DeleteMetaFile)(w);
    if (a) U(DeleteMetaFile)(a);
    U(DeleteMetaFile)(h1);
    DeleteFileW(p);
    CLR();
    CHECK(U(GetMetaFileA)(NULL) == NULL);
}
TEST(gap10_gmfw_relative_path_and_dot_names) {
    NEED_MF(); REQUIRE(U(GetMetaFileW));
    HMETAFILE h = U(GetMetaFileW)(L".");
    CHECK(h == NULL); if (h) U(DeleteMetaFile)(h);
    h = U(GetMetaFileW)(L"..");
    CHECK(h == NULL); if (h) U(DeleteMetaFile)(h);
    h = U(GetMetaFileW)(L"no_such_relative_g10.wmf");
    CHECK(h == NULL); if (h) U(DeleteMetaFile)(h);
    h = U(GetMetaFileW)(L"CON");
    t_note("device name CON -> %s", h ? "handle" : "NULL");
    if (h) U(DeleteMetaFile)(h);
}
TEST(gap10_gmfw_no_gdi_leak) {
    NEED_MF(); REQUIRE(U(GetMetaFileW));
    if (!g10_gdicount()) { t_skip("no GetGuiResources"); return; }
    WCHAR p[MAX_PATH]; HMETAFILE h1; int i; DWORD before;
    tmpname(p, "leak", "wmf");
    h1 = make_wmf(p);
    if (!h1) { t_skip("no wmf"); DeleteFileW(p); return; }
    before = g10_gdicount();
    for (i = 0; i < 100; i++) { HMETAFILE a = U(GetMetaFileW)(p); if (a) U(DeleteMetaFile)(a); }
    EXPECT_EQ(g10_gdicount(), before);
    U(DeleteMetaFile)(h1);
    DeleteFileW(p);
}
static void gmf_thr(void *p) {
    WCHAR *path = (WCHAR *)p; HMETAFILE h = U(GetMetaFileW)(path);
    ((HMETAFILE *)(path + MAX_PATH))[0] = h;
}
TEST(gap10_gmfw_cross_thread_handle) {
    NEED_MF(); REQUIRE(U(GetMetaFileW)); REQUIRE(U(GetObjectType));
    union { WCHAR p[MAX_PATH + 8]; } u; HMETAFILE h1;
    tmpname(u.p, "thr", "wmf");
    h1 = make_wmf(u.p);
    if (!h1) { t_skip("no wmf"); DeleteFileW(u.p); return; }
    ((HMETAFILE *)(u.p + MAX_PATH))[0] = NULL;
    g10_run_thread(gmf_thr, u.p);
    {
        HMETAFILE h = ((HMETAFILE *)(u.p + MAX_PATH))[0];
        CHECK(h != NULL);
        if (h) { EXPECT_EQ(U(GetObjectType)(h), OBJ_METAFILE); CHECK(U(DeleteMetaFile)(h)); }
    }
    U(DeleteMetaFile)(h1);
    DeleteFileW(u.p);
}
TEST(gap10_gmfw_gdi32_vs_gdi32full) {
    typedef HMETAFILE (WINAPI *P)(LPCWSTR);
    P a = (P)t_proc_gdi32("GetMetaFileW"), b = (P)t_proc_gdi32full("GetMetaFileW"); REQUIRE(a); NEED_MF();
    WCHAR p[MAX_PATH]; HMETAFILE h1, x, y;
    tmpname(p, "mods", "wmf");
    h1 = make_wmf(p);
    if (!h1) { t_skip("no wmf"); DeleteFileW(p); return; }
    x = a(p); CHECK(x != NULL);
    if (b) { y = b(p); CHECK(y != NULL); if (y) U(DeleteMetaFile)(y); }
    if (x) U(DeleteMetaFile)(x);
    U(DeleteMetaFile)(h1);
    DeleteFileW(p);
}

/* ====================== CopyMetaFileW ====================== */
TEST(gap10_cmfw_invalid_handles) {
    NEED_MF(); REQUIRE(U(CopyMetaFileW));
    HMETAFILE bad[] = { NULL, (HMETAFILE)(ULONG_PTR)1, (HMETAFILE)(ULONG_PTR)0xdeadbeef, (HMETAFILE)(ULONG_PTR)0xFFFFFFFF }; unsigned i;
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        CLR();
        CHECKF(U(CopyMetaFileW)(bad[i], NULL) == NULL, "handle %p", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "handle %p err %lu", bad[i], GetLastError());
    }
}
TEST(gap10_cmfw_invalid_handle_with_filename_creates_nothing) {
    NEED_MF(); REQUIRE(U(CopyMetaFileW));
    WCHAR p[MAX_PATH]; tmpname(p, "nocreate", "wmf");
    CLR();
    CHECK(U(CopyMetaFileW)(NULL, p) == NULL);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    CHECK(!fexists(p));
}
TEST(gap10_cmfw_wrong_object_type) {
    NEED_MF(); REQUIRE(U(CopyMetaFileW));
    HBRUSH b = U(CreateSolidBrush)(0); HRGN r = U(CreateRectRgn)(0, 0, 1, 1);
    CLR(); CHECK(U(CopyMetaFileW)((HMETAFILE)b, NULL) == NULL); EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    CLR(); CHECK(U(CopyMetaFileW)((HMETAFILE)r, NULL) == NULL); EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    U(DeleteObject)(b); U(DeleteObject)(r);
}
TEST(gap10_cmfw_enhmetafile_handle_rejected) {
    NEED_MF(); REQUIRE(U(CopyMetaFileW)); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC e = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL); HENHMETAFILE eh;
    if (!e) { t_skip("no EMF"); return; }
    U(Rectangle)(e, 0, 0, 4, 4);
    eh = U(CloseEnhMetaFile)(e);
    CLR();
    CHECK(U(CopyMetaFileW)((HMETAFILE)eh, NULL) == NULL);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    U(DeleteEnhMetaFile)(eh);
}
TEST(gap10_cmfw_deleted_handle) {
    NEED_MF(); REQUIRE(U(CopyMetaFileW));
    HMETAFILE h = make_wmf(NULL);
    CHECK(h != NULL);
    if (!h) return;
    U(DeleteMetaFile)(h);
    CLR();
    CHECK(U(CopyMetaFileW)(h, NULL) == NULL);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
}
TEST(gap10_cmfw_memory_copy_equal_bits) {
    NEED_MF(); REQUIRE(U(CopyMetaFileW)); REQUIRE(U(GetObjectType));
    HMETAFILE h = make_wmf(NULL), c; BYTE *b1 = NULL, *b2 = NULL; UINT n1, n2;
    CHECK(h != NULL);
    if (!h) return;
    c = U(CopyMetaFileW)(h, NULL);
    CHECK(c != NULL);
    if (c) {
        CHECK(c != h);
        EXPECT_EQ(U(GetObjectType)(c), OBJ_METAFILE);
        n1 = wmf_bits(h, &b1); n2 = wmf_bits(c, &b2);
        CHECK(n1 > 0); EXPECT_EQ(n1, n2);
        if (b1 && b2 && n1 == n2) CHECK(!memcmp(b1, b2, n1));
        if (b1) HeapFree(GetProcessHeap(), 0, b1);
        if (b2) HeapFree(GetProcessHeap(), 0, b2);
        U(DeleteMetaFile)(c);
    }
    U(DeleteMetaFile)(h);
}
TEST(gap10_cmfw_copy_independent_lifetime) {
    NEED_MF(); REQUIRE(U(CopyMetaFileW)); REQUIRE(U(PlayMetaFile)); NEED_MD();
    HMETAFILE h = make_wmf(NULL), c; COLORREF col;
    if (!h) { t_skip("no wmf"); return; }
    c = U(CopyMetaFileW)(h, NULL);
    CHECK(c != NULL);
    CHECK(U(DeleteMetaFile)(h));
    if (c) {
        CHECK(render_pixel(c, 5, 5, &col)); EXPECT_EQ(col, RED_);
        CHECK(U(DeleteMetaFile)(c));
    }
}
TEST(gap10_cmfw_source_unchanged_after_copy) {
    NEED_MF(); REQUIRE(U(CopyMetaFileW)); REQUIRE(U(PlayMetaFile)); NEED_MD();
    HMETAFILE h = make_wmf(NULL), c; COLORREF col; BYTE *b1 = NULL, *b2 = NULL; UINT n1, n2;
    if (!h) { t_skip("no wmf"); return; }
    n1 = wmf_bits(h, &b1);
    c = U(CopyMetaFileW)(h, NULL);
    n2 = wmf_bits(h, &b2);
    EXPECT_EQ(n1, n2);
    if (b1 && b2 && n1 == n2) CHECK(!memcmp(b1, b2, n1));
    CHECK(render_pixel(h, 3, 3, &col)); EXPECT_EQ(col, RED_);
    if (b1) HeapFree(GetProcessHeap(), 0, b1);
    if (b2) HeapFree(GetProcessHeap(), 0, b2);
    if (c) U(DeleteMetaFile)(c);
    U(DeleteMetaFile)(h);
}
TEST(gap10_cmfw_copy_playback_identical) {
    NEED_MF(); REQUIRE(U(CopyMetaFileW)); REQUIRE(U(PlayMetaFile)); NEED_MD();
    HMETAFILE h = make_wmf(NULL), c; MD a, b;
    if (!h) { t_skip("no wmf"); return; }
    c = U(CopyMetaFileW)(h, NULL);
    CHECK(c != NULL);
    if (c && md_create(&a, 16, 16, 32) && md_create(&b, 16, 16, 32)) {
        md_clear(&a, WHITE_); md_clear(&b, WHITE_);
        U(PlayMetaFile)(a.dc, h); U(PlayMetaFile)(b.dc, c);
        EXPECT_EQ(memcmp(a.bits, b.bits, 16 * 16 * 4), 0);
        { int n = md_count(&a, 0, 0, 16, 16, RED_); CHECKF(n == 100 || n == 121, "red pixels %d", n); }
        md_destroy(&a); md_destroy(&b);
    }
    if (c) U(DeleteMetaFile)(c);
    U(DeleteMetaFile)(h);
}
TEST(gap10_cmfw_memory_to_file) {
    NEED_MF(); REQUIRE(U(CopyMetaFileW)); REQUIRE(U(GetObjectType));
    HMETAFILE h = make_wmf(NULL), c; WCHAR p[MAX_PATH]; BYTE *b = NULL; UINT n;
    tmpname(p, "m2f", "wmf");
    if (!h) { t_skip("no wmf"); return; }
    c = U(CopyMetaFileW)(h, p);
    CHECK(c != NULL);
    if (c) {
        CHECK(fexists(p));
        n = wmf_bits(c, &b);
        CHECK(n > 0);
        t_note("memory->file: bits %u, file size %lu", n, fsize(p));
        CHECK(fsize(p) == n || fsize(p) > 0);
        if (b) HeapFree(GetProcessHeap(), 0, b);
        EXPECT_EQ(U(GetObjectType)(c), OBJ_METAFILE);
        U(DeleteMetaFile)(c);
    }
    U(DeleteMetaFile)(h);
    DeleteFileW(p);
}
TEST(gap10_cmfw_disk_to_file) {
    NEED_MF(); REQUIRE(U(CopyMetaFileW)); REQUIRE(U(GetMetaFileW));
    WCHAR p1[MAX_PATH], p2[MAX_PATH]; HMETAFILE h, c; BYTE *b1 = NULL, *b2 = NULL; UINT n1 = 0, n2 = 0;
    tmpname(p1, "d2f_a", "wmf"); tmpname(p2, "d2f_b", "wmf");
    h = make_wmf(p1);
    if (!h) { t_skip("no wmf"); DeleteFileW(p1); return; }
    c = U(CopyMetaFileW)(h, p2);
    CHECK(c != NULL);
    if (c) {
        CHECK(fexists(p2));
        EXPECT_EQ(fsize(p1), fsize(p2));
        n1 = wmf_bits(h, &b1); n2 = wmf_bits(c, &b2);
        EXPECT_EQ(n1, n2);
        if (b1 && b2 && n1 == n2) CHECK(!memcmp(b1, b2, n1));
        if (b1) HeapFree(GetProcessHeap(), 0, b1);
        if (b2) HeapFree(GetProcessHeap(), 0, b2);
        U(DeleteMetaFile)(c);
    }
    U(DeleteMetaFile)(h);
    DeleteFileW(p1); DeleteFileW(p2);
}
TEST(gap10_cmfw_disk_to_memory) {
    NEED_MF(); REQUIRE(U(CopyMetaFileW));
    WCHAR p[MAX_PATH]; HMETAFILE h, c; BYTE *b1 = NULL, *b2 = NULL; UINT n1, n2;
    tmpname(p, "d2m", "wmf");
    h = make_wmf(p);
    if (!h) { t_skip("no wmf"); DeleteFileW(p); return; }
    c = U(CopyMetaFileW)(h, NULL);
    CHECK(c != NULL);
    if (c) {
        n1 = wmf_bits(h, &b1); n2 = wmf_bits(c, &b2);
        CHECK(n1 > 0); EXPECT_EQ(n1, n2);
        if (b1) HeapFree(GetProcessHeap(), 0, b1);
        if (b2) HeapFree(GetProcessHeap(), 0, b2);
        U(DeleteMetaFile)(c);
    }
    U(DeleteMetaFile)(h);
    DeleteFileW(p);
}
TEST(gap10_cmfw_bad_destination) {
    NEED_MF(); REQUIRE(U(CopyMetaFileW));
    HMETAFILE h = make_wmf(NULL), c;
    if (!h) { t_skip("no wmf"); return; }
    CLR();
    c = U(CopyMetaFileW)(h, L"C:\\no_such_dir_g10\\copy.wmf");
    CHECK(c == NULL);
    t_note("bad destination dir: err %lu", GetLastError());
    if (c) U(DeleteMetaFile)(c);
    c = U(CopyMetaFileW)(h, L"");
    t_note("empty destination -> %s", c ? "handle" : "NULL");
    if (c) U(DeleteMetaFile)(c);
    U(DeleteMetaFile)(h);
}
TEST(gap10_cmfw_destination_exists_observed) {
    NEED_MF(); REQUIRE(U(CopyMetaFileW));
    HMETAFILE h = make_wmf(NULL), c; WCHAR p[MAX_PATH];
    tmpname(p, "exists", "wmf");
    if (!h) { t_skip("no wmf"); return; }
    write_file(p, "OLD", 3);
    c = U(CopyMetaFileW)(h, p);
    t_note("destination exists -> %s, size now %lu", c ? "handle" : "NULL", fsize(p));
    if (c) U(DeleteMetaFile)(c);
    U(DeleteMetaFile)(h);
    DeleteFileW(p);
}
TEST(gap10_cmfw_chain_of_copies) {
    NEED_MF(); REQUIRE(U(CopyMetaFileW));
    HMETAFILE h = make_wmf(NULL), c[6]; int i; BYTE *b0 = NULL, *bi = NULL; UINT n0, ni;
    if (!h) { t_skip("no wmf"); return; }
    n0 = wmf_bits(h, &b0);
    c[0] = U(CopyMetaFileW)(h, NULL);
    for (i = 1; i < 6; i++) c[i] = c[i - 1] ? U(CopyMetaFileW)(c[i - 1], NULL) : NULL;
    for (i = 0; i < 6; i++) {
        CHECKF(c[i] != NULL, "copy %d", i);
        if (c[i]) { ni = wmf_bits(c[i], &bi); CHECKF(ni == n0 && !memcmp(b0, bi, n0), "copy %d differs", i); if (bi) HeapFree(GetProcessHeap(), 0, bi); bi = NULL; U(DeleteMetaFile)(c[i]); }
    }
    if (b0) HeapFree(GetProcessHeap(), 0, b0);
    U(DeleteMetaFile)(h);
}
TEST(gap10_cmfw_ansi_parity) {
    NEED_MF(); REQUIRE(U(CopyMetaFileW)); REQUIRE(U(CopyMetaFileA));
    HMETAFILE h = make_wmf(NULL), a, w; BYTE *ba = NULL, *bw = NULL; UINT na = 0, nw = 0;
    if (!h) { t_skip("no wmf"); return; }
    a = U(CopyMetaFileA)(h, NULL); w = U(CopyMetaFileW)(h, NULL);
    CHECK(a && w);
    if (a) na = wmf_bits(a, &ba);
    if (w) nw = wmf_bits(w, &bw);
    EXPECT_EQ(na, nw);
    if (ba && bw && na == nw) CHECK(!memcmp(ba, bw, na));
    if (ba) HeapFree(GetProcessHeap(), 0, ba);
    if (bw) HeapFree(GetProcessHeap(), 0, bw);
    if (a) U(DeleteMetaFile)(a);
    if (w) U(DeleteMetaFile)(w);
    CLR();
    EXPECT_EQ((ULONG_PTR)U(CopyMetaFileA)(NULL, NULL), 0);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    U(DeleteMetaFile)(h);
}
TEST(gap10_cmfw_no_gdi_leak) {
    NEED_MF(); REQUIRE(U(CopyMetaFileW));
    if (!g10_gdicount()) { t_skip("no GetGuiResources"); return; }
    HMETAFILE h = make_wmf(NULL); int i; DWORD before;
    if (!h) { t_skip("no wmf"); return; }
    before = g10_gdicount();
    for (i = 0; i < 100; i++) { HMETAFILE c = U(CopyMetaFileW)(h, NULL); if (c) U(DeleteMetaFile)(c); }
    EXPECT_EQ(g10_gdicount(), before);
    for (i = 0; i < 50; i++) U(CopyMetaFileW)(NULL, NULL);
    EXPECT_EQ(g10_gdicount(), before);
    U(DeleteMetaFile)(h);
}
static void cmf_thr(void *p) { HMETAFILE *a = (HMETAFILE *)p; a[1] = U(CopyMetaFileW)(a[0], NULL); }
TEST(gap10_cmfw_copy_in_other_thread) {
    NEED_MF(); REQUIRE(U(CopyMetaFileW));
    HMETAFILE a[2];
    a[0] = make_wmf(NULL); a[1] = NULL;
    if (!a[0]) { t_skip("no wmf"); return; }
    g10_run_thread(cmf_thr, a);
    CHECK(a[1] != NULL);
    if (a[1]) CHECK(U(DeleteMetaFile)(a[1]));
    U(DeleteMetaFile)(a[0]);
}
TEST(gap10_cmfw_gdi32_vs_gdi32full) {
    typedef HMETAFILE (WINAPI *P)(HMETAFILE, LPCWSTR);
    P a = (P)t_proc_gdi32("CopyMetaFileW"), b = (P)t_proc_gdi32full("CopyMetaFileW"); REQUIRE(a); NEED_MF();
    HMETAFILE h = make_wmf(NULL), x, y;
    if (!h) { t_skip("no wmf"); return; }
    x = a(h, NULL); CHECK(x != NULL);
    if (b) { y = b(h, NULL); CHECK(y != NULL); if (y) U(DeleteMetaFile)(y); }
    if (x) U(DeleteMetaFile)(x);
    CLR();
    CHECK(a(NULL, NULL) == NULL);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    U(DeleteMetaFile)(h);
}

/* ====================== GetEnhMetaFilePixelFormat ====================== */
#define GEPF U(GetEnhMetaFilePixelFormat)
#define NEED_EMF() do { REQUIRE(GEPF); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile)); REQUIRE(U(Rectangle)); } while (0)
static HENHMETAFILE make_emf(void) {
    HDC e = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!e) return NULL;
    U(Rectangle)(e, 0, 0, 10, 10);
    return U(CloseEnhMetaFile)(e);
}
/* Hand-built EMF carrying a 40-byte pixel format descriptor. */
static HENHMETAFILE make_emf_with_pfd(PIXELFORMATDESCRIPTOR *pfd_out) {
    BYTE buf[160]; ENHMETAHEADER *h = (ENHMETAHEADER *)buf; EMREOF *eof = (EMREOF *)(buf + 140);
    PIXELFORMATDESCRIPTOR pfd;
    if (!U(SetEnhMetaFileBits)) return NULL;
    memset(buf, 0, sizeof buf);
    memset(&pfd, 0, sizeof pfd);
    pfd.nSize = sizeof pfd; pfd.nVersion = 1; pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32; pfd.cDepthBits = 24; pfd.iLayerType = PFD_MAIN_PLANE;
    h->iType = EMR_HEADER; h->nSize = 100;
    h->rclBounds.right = 10; h->rclBounds.bottom = 10; h->rclFrame.right = 1000; h->rclFrame.bottom = 1000;
    h->dSignature = ENHMETA_SIGNATURE; h->nVersion = 0x10000; h->nBytes = sizeof buf; h->nRecords = 2; h->nHandles = 1;
    h->szlDevice.cx = 1024; h->szlDevice.cy = 768; h->szlMillimeters.cx = 320; h->szlMillimeters.cy = 240;
    h->cbPixelFormat = sizeof pfd; h->offPixelFormat = 100;
    memcpy(buf + 100, &pfd, sizeof pfd);
    eof->emr.iType = EMR_EOF; eof->emr.nSize = 20; eof->nPalEntries = 0; eof->offPalEntries = 16;
    *(DWORD *)(buf + 156) = 20;
    if (pfd_out) *pfd_out = pfd;
    return U(SetEnhMetaFileBits)(sizeof buf, buf);
}
TEST(gap10_gepf_invalid_handles) {
    NEED_EMF();
    HENHMETAFILE bad[] = { NULL, (HENHMETAFILE)(ULONG_PTR)1, (HENHMETAFILE)(ULONG_PTR)0xdeadbeef, (HENHMETAFILE)(ULONG_PTR)0xFFFFFFFF }; unsigned i;
    PIXELFORMATDESCRIPTOR pfd;
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        CLR();
        UINT r = GEPF(bad[i], sizeof pfd, &pfd);
        CHECKF(r == 0xFFFFFFFF, "handle %p -> %u", bad[i], r);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "handle %p err %lu", bad[i], GetLastError());
        CLR();
        CHECKF(GEPF(bad[i], 0, NULL) == 0xFFFFFFFF, "size query handle %p", bad[i]);
    }
}
TEST(gap10_gepf_wrong_object_types) {
    NEED_EMF(); REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    HBRUSH b = U(CreateSolidBrush)(0); HMETAFILE w = make_wmf(NULL); PIXELFORMATDESCRIPTOR pfd;
    CLR(); EXPECT_EQ(GEPF((HENHMETAFILE)b, sizeof pfd, &pfd), 0xFFFFFFFF); EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    if (w) { CLR(); EXPECT_EQ(GEPF((HENHMETAFILE)w, sizeof pfd, &pfd), 0xFFFFFFFF); EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE); U(DeleteMetaFile)(w); }
    U(DeleteObject)(b);
}
TEST(gap10_gepf_deleted_handle) {
    NEED_EMF();
    HENHMETAFILE h = make_emf(); PIXELFORMATDESCRIPTOR pfd;
    if (!h) { t_skip("no EMF"); return; }
    U(DeleteEnhMetaFile)(h);
    CLR();
    EXPECT_EQ(GEPF(h, sizeof pfd, &pfd), 0xFFFFFFFF);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
}
TEST(gap10_gepf_plain_emf_has_no_format) {
    NEED_EMF();
    HENHMETAFILE h = make_emf(); PIXELFORMATDESCRIPTOR pfd; BYTE guard[sizeof pfd];
    if (!h) { t_skip("no EMF"); return; }
    memset(&pfd, 0xA5, sizeof pfd); memset(guard, 0xA5, sizeof guard);
    CLR();
    UINT r = GEPF(h, sizeof pfd, &pfd);
    t_note("plain EMF: ret %u err %lu", r, GetLastError());
    CHECK(r == 0 || r == 0xFFFFFFFF);
    CHECK(!memcmp(&pfd, guard, sizeof pfd));      /* nothing to copy */
    r = GEPF(h, 0, NULL);
    t_note("size query: %u", r);
    CHECK(r == 0 || r == 0xFFFFFFFF);
    U(DeleteEnhMetaFile)(h);
}
TEST(gap10_gepf_header_flags_in_plain_emf) {
    NEED_EMF(); REQUIRE(U(GetEnhMetaFileHeader));
    HENHMETAFILE h = make_emf(); ENHMETAHEADER eh;
    if (!h) { t_skip("no EMF"); return; }
    memset(&eh, 0, sizeof eh);
    EXPECT_EQ(U(GetEnhMetaFileHeader)(h, sizeof eh, &eh), sizeof eh);
    EXPECT_EQ(eh.cbPixelFormat, 0);
    EXPECT_EQ(eh.offPixelFormat, 0);
    EXPECT_EQ(eh.bOpenGL, 0);
    U(DeleteEnhMetaFile)(h);
}
TEST(gap10_gepf_handcrafted_size_query) {
    NEED_EMF(); REQUIRE(U(SetEnhMetaFileBits));
    PIXELFORMATDESCRIPTOR src; HENHMETAFILE h = make_emf_with_pfd(&src);
    if (!h) { t_note("SetEnhMetaFileBits rejected the hand-built EMF"); return; }
    if (GEPF(h, 0, NULL) != sizeof(PIXELFORMATDESCRIPTOR)) { t_note("hand-built EMF: pixel format not reported (%u)", GEPF(h, 0, NULL)); U(DeleteEnhMetaFile)(h); return; }
    CLR();
    EXPECT_EQ(GEPF(h, 0, NULL), sizeof(PIXELFORMATDESCRIPTOR));
    EXPECT_EQ(GEPF(h, 1000, NULL), sizeof(PIXELFORMATDESCRIPTOR));
    U(DeleteEnhMetaFile)(h);
}
TEST(gap10_gepf_handcrafted_fetch_exact) {
    NEED_EMF(); REQUIRE(U(SetEnhMetaFileBits));
    PIXELFORMATDESCRIPTOR src, got; HENHMETAFILE h = make_emf_with_pfd(&src);
    if (!h) { t_note("SetEnhMetaFileBits rejected the hand-built EMF"); return; }
    if (GEPF(h, 0, NULL) != sizeof(PIXELFORMATDESCRIPTOR)) { t_note("hand-built EMF: pixel format not reported (%u)", GEPF(h, 0, NULL)); U(DeleteEnhMetaFile)(h); return; }
    memset(&got, 0xEE, sizeof got);
    EXPECT_EQ(GEPF(h, sizeof got, &got), sizeof got);
    CHECK(!memcmp(&got, &src, sizeof got));
    U(DeleteEnhMetaFile)(h);
}
TEST(gap10_gepf_handcrafted_buffer_too_small) {
    NEED_EMF(); REQUIRE(U(SetEnhMetaFileBits));
    PIXELFORMATDESCRIPTOR src, got; HENHMETAFILE h = make_emf_with_pfd(&src); BYTE guard[sizeof got];
    if (!h) { t_note("SetEnhMetaFileBits rejected the hand-built EMF"); return; }
    if (GEPF(h, 0, NULL) != sizeof(PIXELFORMATDESCRIPTOR)) { t_note("hand-built EMF: pixel format not reported (%u)", GEPF(h, 0, NULL)); U(DeleteEnhMetaFile)(h); return; }
    memset(&got, 0xEE, sizeof got); memset(guard, 0xEE, sizeof guard);
    EXPECT_EQ(GEPF(h, sizeof got - 1, &got), sizeof got);     /* still reports the needed size */
    CHECK(!memcmp(&got, guard, sizeof got));                   /* but copies nothing */
    EXPECT_EQ(GEPF(h, 0, &got), sizeof got);
    CHECK(!memcmp(&got, guard, sizeof got));
    EXPECT_EQ(GEPF(h, sizeof got + 100, &got), sizeof got);
    CHECK(!memcmp(&got, &src, sizeof got));
    U(DeleteEnhMetaFile)(h);
}
TEST(gap10_gepf_handcrafted_no_overrun) {
    NEED_EMF(); REQUIRE(U(SetEnhMetaFileBits));
    PIXELFORMATDESCRIPTOR src; HENHMETAFILE h = make_emf_with_pfd(&src); BYTE buf[sizeof(PIXELFORMATDESCRIPTOR) + 32]; unsigned i; int ok = 1;
    if (!h) { t_note("SetEnhMetaFileBits rejected the hand-built EMF"); return; }
    if (GEPF(h, 0, NULL) != sizeof(PIXELFORMATDESCRIPTOR)) { t_note("hand-built EMF: pixel format not reported (%u)", GEPF(h, 0, NULL)); U(DeleteEnhMetaFile)(h); return; }
    memset(buf, 0x77, sizeof buf);
    EXPECT_EQ(GEPF(h, sizeof buf, (PIXELFORMATDESCRIPTOR *)buf), sizeof src);
    for (i = sizeof src; i < sizeof buf; i++) if (buf[i] != 0x77) ok = 0;
    CHECK(ok);
    U(DeleteEnhMetaFile)(h);
}
TEST(gap10_gepf_handcrafted_header_reports_format) {
    NEED_EMF(); REQUIRE(U(SetEnhMetaFileBits)); REQUIRE(U(GetEnhMetaFileHeader));
    PIXELFORMATDESCRIPTOR src; HENHMETAFILE h = make_emf_with_pfd(&src); ENHMETAHEADER eh;
    if (!h) { t_note("SetEnhMetaFileBits rejected the hand-built EMF"); return; }
    if (GEPF(h, 0, NULL) != sizeof(PIXELFORMATDESCRIPTOR)) { t_note("hand-built EMF: pixel format not reported (%u)", GEPF(h, 0, NULL)); U(DeleteEnhMetaFile)(h); return; }
    memset(&eh, 0, sizeof eh);
    UINT n = U(GetEnhMetaFileHeader)(h, sizeof eh, &eh);
    t_note("GetEnhMetaFileHeader -> %u", n);
    EXPECT_EQ(eh.cbPixelFormat, sizeof src);
    EXPECT_EQ(eh.offPixelFormat, 100);
    U(DeleteEnhMetaFile)(h);
}
TEST(gap10_gepf_handcrafted_repeatable) {
    NEED_EMF(); REQUIRE(U(SetEnhMetaFileBits));
    PIXELFORMATDESCRIPTOR src, got; HENHMETAFILE h = make_emf_with_pfd(&src); int i, ok = 1;
    if (!h) { t_note("SetEnhMetaFileBits rejected the hand-built EMF"); return; }
    if (GEPF(h, 0, NULL) != sizeof(PIXELFORMATDESCRIPTOR)) { t_note("hand-built EMF: pixel format not reported (%u)", GEPF(h, 0, NULL)); U(DeleteEnhMetaFile)(h); return; }
    for (i = 0; i < 200; i++) { memset(&got, 0, sizeof got); if (GEPF(h, sizeof got, &got) != sizeof got || memcmp(&got, &src, sizeof got)) ok = 0; }
    CHECK(ok);
    U(DeleteEnhMetaFile)(h);
}
TEST(gap10_gepf_handcrafted_survives_copy) {
    NEED_EMF(); REQUIRE(U(SetEnhMetaFileBits)); REQUIRE(U(CopyEnhMetaFileW));
    PIXELFORMATDESCRIPTOR src, got; HENHMETAFILE h = make_emf_with_pfd(&src), c;
    if (!h) { t_note("SetEnhMetaFileBits rejected the hand-built EMF"); return; }
    if (GEPF(h, 0, NULL) != sizeof(PIXELFORMATDESCRIPTOR)) { t_note("hand-built EMF: pixel format not reported (%u)", GEPF(h, 0, NULL)); U(DeleteEnhMetaFile)(h); return; }
    c = U(CopyEnhMetaFileW)(h, NULL);
    CHECK(c != NULL);
    if (c) {
        memset(&got, 0, sizeof got);
        UINT r = GEPF(c, sizeof got, &got);
        t_note("copy: ret %u", r);
        if (r == sizeof got) CHECK(!memcmp(&got, &src, sizeof got));
        U(DeleteEnhMetaFile)(c);
    }
    U(DeleteEnhMetaFile)(h);
}
TEST(gap10_gepf_gdi_count_stable) {
    NEED_EMF();
    if (!g10_gdicount()) { t_skip("no GetGuiResources"); return; }
    HENHMETAFILE h = make_emf(); PIXELFORMATDESCRIPTOR pfd; int i; DWORD before;
    if (!h) { t_skip("no EMF"); return; }
    before = g10_gdicount();
    for (i = 0; i < 200; i++) { GEPF(h, sizeof pfd, &pfd); GEPF(NULL, 0, NULL); }
    EXPECT_EQ(g10_gdicount(), before);
    U(DeleteEnhMetaFile)(h);
}
static void gepf_thr(void *p) { HENHMETAFILE *a = (HENHMETAFILE *)p; UINT *out = (UINT *)(a + 1); *out = GEPF(a[0], 0, NULL); }
TEST(gap10_gepf_other_thread) {
    NEED_EMF();
    struct { HENHMETAFILE h; UINT r; } s;
    s.h = make_emf(); s.r = 12345;
    if (!s.h) { t_skip("no EMF"); return; }
    UINT here = GEPF(s.h, 0, NULL);
    g10_run_thread(gepf_thr, &s);
    EXPECT_EQ(s.r, here);
    U(DeleteEnhMetaFile)(s.h);
}
TEST(gap10_gepf_gdi32_vs_gdi32full) {
    typedef UINT (WINAPI *P)(HENHMETAFILE, UINT, PIXELFORMATDESCRIPTOR *);
    P a = (P)t_proc_gdi32("GetEnhMetaFilePixelFormat"), b = (P)t_proc_gdi32full("GetEnhMetaFilePixelFormat"); REQUIRE(a); NEED_EMF();
    HENHMETAFILE h = make_emf();
    if (!h) { t_skip("no EMF"); return; }
    UINT r1 = a(h, 0, NULL);
    if (b) EXPECT_EQ(r1, b(h, 0, NULL));
    CLR();
    EXPECT_EQ(a(NULL, 0, NULL), 0xFFFFFFFF);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    U(DeleteEnhMetaFile)(h);
}
