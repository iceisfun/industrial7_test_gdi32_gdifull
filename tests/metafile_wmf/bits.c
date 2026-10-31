/* SetMetaFileBitsEx, CopyMetaFile, GetMetaFile, cross-handle-type behaviour. */
#include "wmf.h"

/* Build a small valid metafile and return its bits (malloc'd). */
static BYTE *sample_bits(UINT *n) {
    HDC dc = mf_new();
    if (!dc) return NULL;
    U(MoveToEx)(dc, 1, 1, NULL);
    U(LineTo)(dc, 20, 30);
    U(Rectangle)(dc, 2, 3, 40, 50);
    U(Ellipse)(dc, 4, 5, 60, 70);
    WB b;
    if (!mf_finish(dc, &b, NULL)) return NULL;
    *n = b.n;
    return b.p;
}

TEST(wmf_setbits_roundtrip) {
    WMF_REQ_BASE(); REQUIRE(U(SetMetaFileBitsEx));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    HMETAFILE h = U(SetMetaFileBitsEx)(n, p);
    CHECK(h != NULL);
    if (h) {
        WB b;
        CHECK(wb_get(h, &b) != NULL);
        EXPECT_EQ(b.n, n);
        if (b.n == n) CHECK(!memcmp(b.p, p, n));
        wb_free(&b);
        EXPECT_EQ(U(GetObjectType)(h), OBJ_METAFILE);
        CHECK(U(DeleteMetaFile)(h));
    }
    free(p);
}

TEST(wmf_setbits_copies_data) {
    WMF_REQ_BASE(); REQUIRE(U(SetMetaFileBitsEx));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    HMETAFILE h = U(SetMetaFileBitsEx)(n, p);
    REQUIRE(h);
    BYTE *orig = malloc(n); memcpy(orig, p, n);
    memset(p, 0xEE, n);            /* caller's buffer is no longer referenced */
    WB b;
    CHECK(wb_get(h, &b) != NULL);
    if (b.p && b.n == n) CHECK(!memcmp(b.p, orig, n));
    wb_free(&b);
    U(DeleteMetaFile)(h);
    free(orig); free(p);
}

TEST(wmf_setbits_too_small) {
    WMF_REQ_BASE(); REQUIRE(U(SetMetaFileBitsEx));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    UINT sizes[] = { 0, 1, 2, 9, 17 };
    for (size_t i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        SET_ERR(0);
        HMETAFILE h = U(SetMetaFileBitsEx)(sizes[i], p);
        CHECKF(h == NULL, "size %u", sizes[i]);
        EXPECT_ERR(h, ERROR_INVALID_DATA);
        if (h) U(DeleteMetaFile)(h);
    }
    free(p);
}

TEST(wmf_setbits_header_only) {
    WMF_REQ_BASE(); REQUIRE(U(SetMetaFileBitsEx));
    WMFHDR hd = { 1, 9, 0x300, 9, 0, 0, 0 };
    SET_ERR(0);
    HMETAFILE h = U(SetMetaFileBitsEx)(18, (BYTE *)&hd);
    t_note("SetMetaFileBitsEx(header only) = %p err %lu", h, GetLastError());
    if (h) {
        WB b;
        wb_get(h, &b);
        t_note("  bits size %u", b.n);
        wb_free(&b);
        U(DeleteMetaFile)(h);
    }
}

typedef struct { const char *name; WORD type, hdrsize, ver; int ok; } HDRCASE;
TEST(wmf_setbits_header_validation) {
    WMF_REQ_BASE(); REQUIRE(U(SetMetaFileBitsEx));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    static const HDRCASE cases[] = {
        { "type1", 1, 9, 0x300, 1 }, { "type2", 2, 9, 0x300, 1 }, { "type0", 0, 9, 0x300, 0 },
        { "type3", 3, 9, 0x300, 0 }, { "typeFFFF", 0xFFFF, 9, 0x300, 0 },
        { "hdr8", 1, 8, 0x300, 0 }, { "hdr10", 1, 10, 0x300, 0 }, { "hdr0", 1, 0, 0x300, 0 },
        { "ver100", 1, 9, 0x100, 1 }, { "ver200", 1, 9, 0x200, 0 }, { "ver301", 1, 9, 0x301, 0 },
        { "ver0", 1, 9, 0, 0 }, { "ver101", 1, 9, 0x101, 0 }, { "ver3FF", 1, 9, 0x3FF, 0 },
        { "ver400", 1, 9, 0x400, 0 }, { "verFFFF", 1, 9, 0xFFFF, 0 }, { "ver700", 1, 9, 0x700, 0 },
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++) {
        BYTE *q = malloc(n); memcpy(q, p, n);
        WMFHDR *h = (WMFHDR *)q;
        h->type = cases[i].type; h->hdrsize = cases[i].hdrsize; h->ver = cases[i].ver;
        SET_ERR(0);
        HMETAFILE m = U(SetMetaFileBitsEx)(n, q);
        CHECKF((m != NULL) == cases[i].ok, "%s: result %p", cases[i].name, m);
        if (!cases[i].ok) EXPECT_ERR(m, ERROR_INVALID_DATA);
        if (m) U(DeleteMetaFile)(m);
        free(q);
    }
    free(p);
}

TEST(wmf_setbits_type2_normalised_to_1) {
    WMF_REQ_BASE(); REQUIRE(U(SetMetaFileBitsEx));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    ((WMFHDR *)p)->type = 2;
    HMETAFILE h = U(SetMetaFileBitsEx)(n, p);
    REQUIRE(h);
    WB b;
    CHECK(wb_get(h, &b) != NULL);
    if (b.p) EXPECT_EQ(wb_hdr(&b)->type, 1);
    wb_free(&b);
    U(DeleteMetaFile)(h);
    free(p);
}

TEST(wmf_setbits_nparams_ignored) {
    WMF_REQ_BASE(); REQUIRE(U(SetMetaFileBitsEx));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    ((WMFHDR *)p)->nparm = 0x1234;
    HMETAFILE h = U(SetMetaFileBitsEx)(n, p);
    CHECK(h != NULL);
    if (h) U(DeleteMetaFile)(h);
    free(p);
}

TEST(wmf_setbits_corrupt_size_field) {
    WMF_REQ_BASE(); REQUIRE(U(SetMetaFileBitsEx));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    DWORD vals[] = { 0, 1, 12, 1000, 0x7FFFFFFF };
    for (size_t i = 0; i < sizeof vals / sizeof *vals; i++) {
        BYTE *q = malloc(n); memcpy(q, p, n);
        ((WMFHDR *)q)->size = vals[i];
        HMETAFILE h = U(SetMetaFileBitsEx)(n, q);
        t_note("mtSize=%lu -> %p", vals[i], h);
        if (h) {
            WB b;
            wb_get(h, &b);
            t_note("  bits size %u (orig %u)", b.n, n);
            wb_free(&b);
            U(DeleteMetaFile)(h);
        }
        free(q);
    }
    free(p);
}

TEST(wmf_setbits_odd_size) {
    WMF_REQ_BASE(); REQUIRE(U(SetMetaFileBitsEx));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    SET_ERR(0);
    HMETAFILE h = U(SetMetaFileBitsEx)(n - 1, p);
    t_note("SetMetaFileBitsEx(odd size) = %p err %lu", h, GetLastError());
    if (h) U(DeleteMetaFile)(h);
    free(p);
}

TEST(wmf_setbits_placeable_rejected) {
    WMF_REQ_BASE(); REQUIRE(U(SetMetaFileBitsEx));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    BYTE *q = malloc(22 + n);
    APMHDR a; memset(&a, 0, sizeof a);
    a.key = APM_KEY; a.l = 0; a.t = 0; a.r = 100; a.b = 100; a.inch = 96;
    WORD *w = (WORD *)&a, sum = 0;
    for (int i = 0; i < 10; i++) sum ^= w[i];
    a.sum = sum;
    memcpy(q, &a, 22); memcpy(q + 22, p, n);
    SET_ERR(0);
    HMETAFILE h = U(SetMetaFileBitsEx)(22 + n, q);
    CHECK(h == NULL);
    EXPECT_ERR(h, ERROR_INVALID_DATA);
    if (h) U(DeleteMetaFile)(h);
    /* stripped of the 22-byte header it is valid */
    h = U(SetMetaFileBitsEx)(n, q + 22);
    CHECK(h != NULL);
    if (h) U(DeleteMetaFile)(h);
    free(q); free(p);
}

TEST(wmf_setbits_garbage) {
    WMF_REQ_BASE(); REQUIRE(U(SetMetaFileBitsEx));
    BYTE g[256];
    for (int round = 0; round < 8; round++) {
        for (int i = 0; i < 256; i++) g[i] = (BYTE)(i * 31 + round * 17 + 5);
        g[0] = 0x55;   /* never a legal mtType */
        SET_ERR(0);
        HMETAFILE h = U(SetMetaFileBitsEx)(256, g);
        CHECK(h == NULL);
        EXPECT_ERR(h, ERROR_INVALID_DATA);
        if (h) U(DeleteMetaFile)(h);
    }
}

TEST(wmf_setbits_zeros) {
    WMF_REQ_BASE(); REQUIRE(U(SetMetaFileBitsEx));
    BYTE z[64]; memset(z, 0, sizeof z);
    SET_ERR(0);
    HMETAFILE h = U(SetMetaFileBitsEx)(64, z);
    CHECK(h == NULL);
    EXPECT_ERR(h, ERROR_INVALID_DATA);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_setbits_independent_handles) {
    WMF_REQ_BASE(); REQUIRE(U(SetMetaFileBitsEx));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    HMETAFILE a = U(SetMetaFileBitsEx)(n, p), b = U(SetMetaFileBitsEx)(n, p);
    REQUIRE(a && b);
    CHECK(a != b);
    CHECK(U(DeleteMetaFile)(a));
    WB w;
    CHECK(wb_get(b, &w) != NULL);
    EXPECT_EQ(w.n, n);
    wb_free(&w);
    CHECK(U(DeleteMetaFile)(b));
    free(p);
}

TEST(wmf_setbits_leak) {
    WMF_REQ_BASE(); REQUIRE(U(SetMetaFileBitsEx)); REQUIRE(U(GetGuiResources));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    DWORD before = gdi_count();
    for (int i = 0; i < 300; i++) { HMETAFILE h = U(SetMetaFileBitsEx)(n, p); if (h) U(DeleteMetaFile)(h); }
    EXPECT_EQ(gdi_count(), before);
    free(p);
}

TEST(wmf_setbits_many_live_handles) {
    WMF_REQ_BASE(); REQUIRE(U(SetMetaFileBitsEx));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    HMETAFILE hs[200];
    int ok = 0;
    for (int i = 0; i < 200; i++) { hs[i] = U(SetMetaFileBitsEx)(n, p); ok += hs[i] != NULL; }
    EXPECT_EQ(ok, 200);
    for (int i = 0; i < 200; i++) if (hs[i]) U(DeleteMetaFile)(hs[i]);
    free(p);
}

/* ---- CopyMetaFile ---- */
TEST(wmf_copy_memory) {
    WMF_REQ_BASE(); REQUIRE(U(CopyMetaFileA));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    HMETAFILE h = U(SetMetaFileBitsEx)(n, p);
    REQUIRE(h);
    HMETAFILE c = U(CopyMetaFileA)(h, NULL);
    CHECK(c != NULL);
    CHECK(c != h);
    if (c) {
        WB b;
        CHECK(wb_get(c, &b) != NULL);
        EXPECT_EQ(b.n, n);
        if (b.n == n) CHECK(!memcmp(b.p, p, n));
        wb_free(&b);
        EXPECT_EQ(U(GetObjectType)(c), OBJ_METAFILE);
        U(DeleteMetaFile)(c);
    }
    U(DeleteMetaFile)(h);
    free(p);
}

TEST(wmf_copy_memory_w) {
    WMF_REQ_BASE(); REQUIRE(U(CopyMetaFileW));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    HMETAFILE h = U(SetMetaFileBitsEx)(n, p);
    REQUIRE(h);
    HMETAFILE c = U(CopyMetaFileW)(h, NULL);
    CHECK(c != NULL);
    if (c) U(DeleteMetaFile)(c);
    U(DeleteMetaFile)(h);
    free(p);
}

TEST(wmf_copy_survives_original_delete) {
    WMF_REQ_BASE(); REQUIRE(U(CopyMetaFileA));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    HMETAFILE h = U(SetMetaFileBitsEx)(n, p);
    HMETAFILE c = U(CopyMetaFileA)(h, NULL);
    REQUIRE(h && c);
    CHECK(U(DeleteMetaFile)(h));
    WB b;
    CHECK(wb_get(c, &b) != NULL);
    EXPECT_EQ(b.n, n);
    wb_free(&b);
    CHECK(U(DeleteMetaFile)(c));
    free(p);
}

TEST(wmf_copy_invalid_handle) {
    WMF_REQ_BASE(); REQUIRE(U(CopyMetaFileA)); REQUIRE(U(CopyMetaFileW));
    HMETAFILE bad[] = { NULL, (HMETAFILE)1, (HMETAFILE)(ULONG_PTR)0xdeadbeef };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        HMETAFILE c = U(CopyMetaFileA)(bad[i], NULL);
        CHECK(c == NULL);
        EXPECT_ERR(c, ERROR_INVALID_HANDLE);
        SET_ERR(0);
        c = U(CopyMetaFileW)(bad[i], NULL);
        CHECK(c == NULL);
        EXPECT_ERR(c, ERROR_INVALID_HANDLE);
    }
}

TEST(wmf_copy_dc_handle) {
    WMF_REQ_BASE(); REQUIRE(U(CopyMetaFileA));
    HDC dc = mf_new();
    REQUIRE(dc);
    SET_ERR(0);
    HMETAFILE c = U(CopyMetaFileA)((HMETAFILE)dc, NULL);
    CHECK(c == NULL);
    EXPECT_ERR(c, ERROR_INVALID_HANDLE);
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_copy_to_file) {
    WMF_REQ_BASE(); REQUIRE(U(CopyMetaFileA));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    HMETAFILE h = U(SetMetaFileBitsEx)(n, p);
    REQUIRE(h);
    char path[MAX_PATH]; tmp_path(path, "wmf_cpf");
    DeleteFileA(path);
    HMETAFILE c = U(CopyMetaFileA)(h, path);
    CHECK(c != NULL);
    if (c) {
        WB b;
        CHECK(wb_get(c, &b) != NULL);
        CHECK(wb_valid(&b));
        EXPECT_EQ(wb_count(&b, 0x0213), 1);
        EXPECT_EQ(wb_count(&b, 0x041B), 1);
        EXPECT_EQ(wb_count(&b, 0x0418), 1);
        EXPECT_EQ(b.n, n);
        wb_free(&b);
        U(DeleteMetaFile)(c);
    }
    UINT fn; BYTE *f = file_read(path, &fn);
    CHECK(f != NULL);
    if (f) { EXPECT_EQ(fn, n); if (fn == n) CHECK(!memcmp(f + 2, p + 2, n - 2)); free(f); }
    DeleteFileA(path);
    U(DeleteMetaFile)(h);
    free(p);
}

TEST(wmf_copy_to_file_w) {
    WMF_REQ_BASE(); REQUIRE(U(CopyMetaFileW));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    HMETAFILE h = U(SetMetaFileBitsEx)(n, p);
    REQUIRE(h);
    char path[MAX_PATH]; tmp_path(path, "wmf_cpfw");
    WCHAR wp[MAX_PATH]; MultiByteToWideChar(CP_ACP, 0, path, -1, wp, MAX_PATH);
    DeleteFileA(path);
    HMETAFILE c = U(CopyMetaFileW)(h, wp);
    CHECK(c != NULL);
    if (c) U(DeleteMetaFile)(c);
    CHECK(GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES);
    DeleteFileA(path);
    U(DeleteMetaFile)(h);
    free(p);
}

TEST(wmf_copy_to_bad_path) {
    WMF_REQ_BASE(); REQUIRE(U(CopyMetaFileA));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    HMETAFILE h = U(SetMetaFileBitsEx)(n, p);
    REQUIRE(h);
    char path[MAX_PATH], d[MAX_PATH]; GetTempPathA(MAX_PATH, d);
    snprintf(path, sizeof path, "%swmf_cpnodir_%lu\\x.wmf", d, GetCurrentProcessId());
    SET_ERR(0);
    HMETAFILE c = U(CopyMetaFileA)(h, path);
    CHECK(c == NULL);
    EXPECT_ERR(c, ERROR_PATH_NOT_FOUND);
    if (c) U(DeleteMetaFile)(c);
    /* source still valid */
    EXPECT_NE(U(GetMetaFileBitsEx)(h, 0, NULL), 0);
    U(DeleteMetaFile)(h);
    free(p);
}

TEST(wmf_copy_file_to_file) {
    WMF_REQ_BASE(); REQUIRE(U(CopyMetaFileA));
    char p1[MAX_PATH], p2[MAX_PATH];
    tmp_path(p1, "wmf_cff1"); tmp_path(p2, "wmf_cff2");
    DeleteFileA(p1); DeleteFileA(p2);
    HDC dc = U(CreateMetaFileA)(p1);
    REQUIRE(dc);
    U(Rectangle)(dc, 1, 2, 3, 4);
    HMETAFILE h = U(CloseMetaFile)(dc);
    REQUIRE(h);
    HMETAFILE c = U(CopyMetaFileA)(h, p2);
    CHECK(c != NULL);
    if (c) U(DeleteMetaFile)(c);
    U(DeleteMetaFile)(h);
    UINT n1, n2; BYTE *f1 = file_read(p1, &n1), *f2 = file_read(p2, &n2);
    CHECK(f1 && f2);
    if (f1 && f2) { EXPECT_EQ(n1, n2); if (n1 == n2) CHECK(!memcmp(f1, f2, n1)); }
    free(f1); free(f2);
    DeleteFileA(p1); DeleteFileA(p2);
}

TEST(wmf_copy_file_to_memory) {
    WMF_REQ_BASE(); REQUIRE(U(CopyMetaFileA));
    char p1[MAX_PATH]; tmp_path(p1, "wmf_cfm");
    DeleteFileA(p1);
    HDC dc = U(CreateMetaFileA)(p1);
    REQUIRE(dc);
    U(Rectangle)(dc, 1, 2, 3, 4);
    HMETAFILE h = U(CloseMetaFile)(dc);
    REQUIRE(h);
    HMETAFILE c = U(CopyMetaFileA)(h, NULL);
    CHECK(c != NULL);
    if (c) {
        WB a, b;
        CHECK(wb_get(c, &a) != NULL); CHECK(wb_get(h, &b) != NULL);
        EXPECT_EQ(a.n, b.n);
        if (a.n == b.n && a.p) CHECK(wb_count(&a, 0x041B) == 1);
        wb_free(&a); wb_free(&b);
        U(DeleteMetaFile)(c);
    }
    U(DeleteMetaFile)(h);
    DeleteFileA(p1);
}

TEST(wmf_copy_leak) {
    WMF_REQ_BASE(); REQUIRE(U(CopyMetaFileA)); REQUIRE(U(GetGuiResources));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    HMETAFILE h = U(SetMetaFileBitsEx)(n, p);
    REQUIRE(h);
    DWORD before = gdi_count();
    for (int i = 0; i < 200; i++) { HMETAFILE c = U(CopyMetaFileA)(h, NULL); if (c) U(DeleteMetaFile)(c); }
    EXPECT_EQ(gdi_count(), before);
    U(DeleteMetaFile)(h);
    free(p);
}

/* ---- GetMetaFile ---- */
TEST(wmf_getmetafile_from_bits_file) {
    WMF_REQ_BASE(); REQUIRE(U(GetMetaFileA)); REQUIRE(U(GetMetaFileW));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    char path[MAX_PATH]; tmp_path(path, "wmf_gmf");
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    REQUIRE(f != INVALID_HANDLE_VALUE);
    DWORD w; WriteFile(f, p, n, &w, NULL); CloseHandle(f);
    HMETAFILE h = U(GetMetaFileA)(path);
    CHECK(h != NULL);
    if (h) {
        WB b;
        CHECK(wb_get(h, &b) != NULL);
        EXPECT_EQ(b.n, n);
        if (b.n == n) CHECK(!memcmp(b.p, p, n));
        EXPECT_EQ(U(GetObjectType)(h), OBJ_METAFILE);
        wb_free(&b);
        U(DeleteMetaFile)(h);
    }
    WCHAR wp[MAX_PATH]; MultiByteToWideChar(CP_ACP, 0, path, -1, wp, MAX_PATH);
    h = U(GetMetaFileW)(wp);
    CHECK(h != NULL);
    if (h) U(DeleteMetaFile)(h);
    DeleteFileA(path);
    free(p);
}

TEST(wmf_getmetafile_type2_file) {
    WMF_REQ_BASE(); REQUIRE(U(GetMetaFileA));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    ((WMFHDR *)p)->type = 2;
    char path[MAX_PATH]; tmp_path(path, "wmf_gm2");
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    REQUIRE(f != INVALID_HANDLE_VALUE);
    DWORD w; WriteFile(f, p, n, &w, NULL); CloseHandle(f);
    HMETAFILE h = U(GetMetaFileA)(path);
    t_note("GetMetaFileA(type 2 file) = %p", h);
    if (h) U(DeleteMetaFile)(h);
    DeleteFileA(path);
    free(p);
}

TEST(wmf_getmetafile_bad_header_file) {
    WMF_REQ_BASE(); REQUIRE(U(GetMetaFileA));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    ((WMFHDR *)p)->ver = 0x555;
    char path[MAX_PATH]; tmp_path(path, "wmf_gmbh");
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    REQUIRE(f != INVALID_HANDLE_VALUE);
    DWORD w; WriteFile(f, p, n, &w, NULL); CloseHandle(f);
    HMETAFILE h = U(GetMetaFileA)(path);
    CHECK(h == NULL);
    if (h) U(DeleteMetaFile)(h);
    DeleteFileA(path);
    free(p);
}

TEST(wmf_getmetafile_placeable_file) {
    WMF_REQ_BASE(); REQUIRE(U(GetMetaFileA));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    APMHDR a; memset(&a, 0, sizeof a);
    a.key = APM_KEY; a.r = 100; a.b = 100; a.inch = 96;
    char path[MAX_PATH]; tmp_path(path, "wmf_gmpl");
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    REQUIRE(f != INVALID_HANDLE_VALUE);
    DWORD w; WriteFile(f, &a, 22, &w, NULL); WriteFile(f, p, n, &w, NULL); CloseHandle(f);
    HMETAFILE h = U(GetMetaFileA)(path);
    t_note("GetMetaFileA(placeable file) = %p", h);
    if (h) U(DeleteMetaFile)(h);
    DeleteFileA(path);
    free(p);
}

TEST(wmf_getmetafile_truncated_records) {
    WMF_REQ_BASE(); REQUIRE(U(GetMetaFileA));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    char path[MAX_PATH]; tmp_path(path, "wmf_gmtr");
    UINT cuts[] = { 18, 20, 24, 30, n - 6, n - 2 };
    for (size_t i = 0; i < sizeof cuts / sizeof *cuts; i++) {
        HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        REQUIRE(f != INVALID_HANDLE_VALUE);
        DWORD w; WriteFile(f, p, cuts[i], &w, NULL); CloseHandle(f);
        HMETAFILE h = U(GetMetaFileA)(path);
        t_note("truncated to %u bytes: GetMetaFileA = %p", cuts[i], h);
        if (h) U(DeleteMetaFile)(h);
    }
    DeleteFileA(path);
    free(p);
}

TEST(wmf_getmetafile_directory) {
    WMF_REQ_BASE(); REQUIRE(U(GetMetaFileA));
    char d[MAX_PATH]; GetTempPathA(MAX_PATH, d);
    HMETAFILE h = U(GetMetaFileA)(d);
    CHECK(h == NULL);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_getmetafile_leak) {
    WMF_REQ_BASE(); REQUIRE(U(GetMetaFileA)); REQUIRE(U(GetGuiResources));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    char path[MAX_PATH]; tmp_path(path, "wmf_gml");
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    REQUIRE(f != INVALID_HANDLE_VALUE);
    DWORD w; WriteFile(f, p, n, &w, NULL); CloseHandle(f);
    DWORD before = gdi_count();
    for (int i = 0; i < 100; i++) { HMETAFILE h = U(GetMetaFileA)(path); if (h) U(DeleteMetaFile)(h); }
    EXPECT_EQ(gdi_count(), before);
    CHECK(DeleteFileA(path));
    free(p);
}

TEST(wmf_getmetafile_file_stays_after_delete) {
    WMF_REQ_BASE(); REQUIRE(U(GetMetaFileA));
    UINT n; BYTE *p = sample_bits(&n);
    REQUIRE(p);
    char path[MAX_PATH]; tmp_path(path, "wmf_gms");
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    REQUIRE(f != INVALID_HANDLE_VALUE);
    DWORD w; WriteFile(f, p, n, &w, NULL); CloseHandle(f);
    HMETAFILE h = U(GetMetaFileA)(path);
    REQUIRE(h);
    CHECK(U(DeleteMetaFile)(h));
    CHECK(GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES);
    CHECK(DeleteFileA(path));
    free(p);
}

/* ---- cross handle kinds: enhanced metafiles are a different object type ---- */
static HENHMETAFILE mk_emf(void) {
    if (!U(CreateEnhMetaFileA)) return NULL;
    HDC dc = U(CreateEnhMetaFileA)(NULL, NULL, NULL, NULL);
    if (!dc) return NULL;
    U(Rectangle)(dc, 0, 0, 10, 10);
    return U(CloseEnhMetaFile)(dc);
}

TEST(wmf_cross_getbits_on_emf) {
    WMF_REQ_BASE(); REQUIRE(U(CreateEnhMetaFileA)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HENHMETAFILE e = mk_emf();
    REQUIRE(e);
    BYTE buf[256];
    SET_ERR(0);
    UINT r = U(GetMetaFileBitsEx)((HMETAFILE)e, 0, NULL);
    CHECK(r == 0);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    SET_ERR(0);
    r = U(GetMetaFileBitsEx)((HMETAFILE)e, sizeof buf, buf);
    CHECK(r == 0);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    EXPECT_EQ(U(GetObjectType)(e), OBJ_ENHMETAFILE);
    CHECK(U(DeleteEnhMetaFile)(e));
}

TEST(wmf_cross_deletemetafile_on_emf) {
    WMF_REQ_BASE(); REQUIRE(U(CreateEnhMetaFileA)); REQUIRE(U(DeleteEnhMetaFile));
    HENHMETAFILE e = mk_emf();
    REQUIRE(e);
    SET_ERR(0);
    BOOL r = U(DeleteMetaFile)((HMETAFILE)e);
    CHECK(!r);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    CHECK(U(DeleteEnhMetaFile)(e));
}

TEST(wmf_cross_deleteenhmetafile_on_wmf) {
    WMF_REQ_BASE(); REQUIRE(U(DeleteEnhMetaFile));
    HDC dc = mf_new();
    REQUIRE(dc);
    HMETAFILE h = U(CloseMetaFile)(dc);
    REQUIRE(h);
    SET_ERR(0);
    BOOL r = U(DeleteEnhMetaFile)((HENHMETAFILE)h);
    CHECK(!r);
    t_note("DeleteEnhMetaFile(WMF handle) err %lu", GetLastError());
    CHECK(U(DeleteMetaFile)(h));
}

TEST(wmf_cross_getenhbits_on_wmf) {
    WMF_REQ_BASE(); REQUIRE(U(GetEnhMetaFileBits));
    HDC dc = mf_new();
    REQUIRE(dc);
    HMETAFILE h = U(CloseMetaFile)(dc);
    REQUIRE(h);
    SET_ERR(0);
    UINT r = U(GetEnhMetaFileBits)((HENHMETAFILE)h, 0, NULL);
    CHECK(r == 0);
    U(DeleteMetaFile)(h);
}

TEST(wmf_cross_copymetafile_on_emf) {
    WMF_REQ_BASE(); REQUIRE(U(CopyMetaFileA)); REQUIRE(U(DeleteEnhMetaFile));
    HENHMETAFILE e = mk_emf();
    REQUIRE(e);
    SET_ERR(0);
    HMETAFILE c = U(CopyMetaFileA)((HMETAFILE)e, NULL);
    CHECK(c == NULL);
    EXPECT_ERR(c, ERROR_INVALID_HANDLE);
    U(DeleteEnhMetaFile)(e);
}

TEST(wmf_cross_closemetafile_on_enh_dc) {
    WMF_REQ_BASE(); REQUIRE(U(CreateEnhMetaFileA)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC dc = U(CreateEnhMetaFileA)(NULL, NULL, NULL, NULL);
    REQUIRE(dc);
    EXPECT_EQ(U(GetObjectType)(dc), OBJ_ENHMETADC);
    SET_ERR(0);
    HMETAFILE h = U(CloseMetaFile)(dc);
    CHECK(h == NULL);
    EXPECT_ERR(h, ERROR_INVALID_HANDLE);
    HENHMETAFILE e = U(CloseEnhMetaFile)(dc);
    CHECK(e != NULL);
    if (e) U(DeleteEnhMetaFile)(e);
}

TEST(wmf_cross_closeenh_on_wmf_dc) {
    WMF_REQ_BASE(); REQUIRE(U(CloseEnhMetaFile));
    HDC dc = mf_new();
    REQUIRE(dc);
    SET_ERR(0);
    HENHMETAFILE e = U(CloseEnhMetaFile)(dc);
    CHECK(e == NULL);
    HMETAFILE h = U(CloseMetaFile)(dc);
    CHECK(h != NULL);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_cross_playmetafile_with_emf) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HENHMETAFILE e = mk_emf();
    REQUIRE(e);
    DIBDC d;
    REQUIRE(dibdc_make(&d, 16, 16));
    SET_ERR(0);
    BOOL r = U(PlayMetaFile)(d.dc, (HMETAFILE)e);
    CHECK(!r);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    CHECK(dibdc_nonwhite(&d) == 0);
    dibdc_free(&d);
    U(DeleteEnhMetaFile)(e);
}
