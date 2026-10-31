#include "emf.h"

static HENHMETAFILE mk_scene(void) {
    HDC dc = U(CreateEnhMetaFileW)(NULL, NULL, NULL, L"bits\0test\0");
    if (!dc) return NULL;
    HBRUSH b = U(CreateSolidBrush)(RGB(10, 200, 30));
    HGDIOBJ o = U(SelectObject)(dc, b);
    U(Rectangle)(dc, 5, 5, 60, 40);
    U(Ellipse)(dc, 10, 10, 50, 35);
    U(SelectObject)(dc, o);
    U(DeleteObject)(b);
    return emf_end(dc);
}

/* ---------- GetEnhMetaFileBits ---------- */

TEST(emf_getbits_size_query) {
    REQUIRE(U(GetEnhMetaFileBits));
    HENHMETAFILE h = mk_scene();
    if (!h) { CHECK(h != NULL); return; }
    ENHMETAHEADER hd; U(GetEnhMetaFileHeader)(h, sizeof hd, &hd);
    UINT n = U(GetEnhMetaFileBits)(h, 0, NULL);
    EXPECT_EQ(n, hd.nBytes);
    EXPECT_EQ(U(GetEnhMetaFileBits)(h, 12345, NULL), hd.nBytes);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_getbits_exact_buffer) {
    REQUIRE(U(GetEnhMetaFileBits));
    HENHMETAFILE h = mk_scene();
    if (!h) { CHECK(h != NULL); return; }
    UINT n = U(GetEnhMetaFileBits)(h, 0, NULL);
    BYTE *b = malloc(n + 16);
    memset(b, 0xAA, n + 16);
    EXPECT_EQ(U(GetEnhMetaFileBits)(h, n, b), n);
    CHECK(b[n] == 0xAA && b[n + 15] == 0xAA);
    EXPECT_EQ(((ENHMETAHEADER *)b)->dSignature, ENHMETA_SIGNATURE);
    EXPECT_EQ(emf_count_recs(b, n), ((ENHMETAHEADER *)b)->nRecords);
    free(b);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_getbits_larger_buffer) {
    REQUIRE(U(GetEnhMetaFileBits));
    HENHMETAFILE h = mk_scene();
    if (!h) { CHECK(h != NULL); return; }
    UINT n = U(GetEnhMetaFileBits)(h, 0, NULL);
    BYTE *b = malloc(n * 2); memset(b, 0xAA, n * 2);
    EXPECT_EQ(U(GetEnhMetaFileBits)(h, n * 2, b), n);
    CHECK(b[n] == 0xAA);
    free(b);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_getbits_buffer_too_small) {
    REQUIRE(U(GetEnhMetaFileBits));
    HENHMETAFILE h = mk_scene();
    if (!h) { CHECK(h != NULL); return; }
    UINT n = U(GetEnhMetaFileBits)(h, 0, NULL);
    BYTE *b = malloc(n); memset(b, 0xAA, n);
    UINT sizes[] = { 0, 1, 4, 87, 88, n / 2, n - 1 };
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        SetLastError(0);
        UINT r = U(GetEnhMetaFileBits)(h, sizes[i], b);
        CHECKF(r == 0, "size %u -> %u", sizes[i], r);
        EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    }
    CHECK(b[0] == 0xAA && b[n - 1] == 0xAA);
    free(b);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_getbits_repeated_identical) {
    REQUIRE(U(GetEnhMetaFileBits));
    HENHMETAFILE h = mk_scene();
    if (!h) { CHECK(h != NULL); return; }
    UINT n1, n2;
    BYTE *a = emf_bits(h, &n1);
    for (int i = 0; i < 20; i++) {
        BYTE *b = emf_bits(h, &n2);
        CHECK(a && b && n1 == n2 && !memcmp(a, b, n1));
        free(b);
    }
    free(a);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_getbits_invalid_handles) {
    REQUIRE(U(GetEnhMetaFileBits)); REQUIRE(U(CreateSolidBrush));
    BYTE b[16];
    HBRUSH br = U(CreateSolidBrush)(0);
    HENHMETAFILE bad[] = { NULL, (HENHMETAFILE)1, (HENHMETAFILE)0xdeadbeef, (HENHMETAFILE)br };
    for (unsigned i = 0; i < 4; i++) {
        CHECKF(U(GetEnhMetaFileBits)(bad[i], 0, NULL) == 0, "q %u", i);
        CHECKF(U(GetEnhMetaFileBits)(bad[i], 16, b) == 0, "b %u", i);
    }
    U(DeleteObject)(br);
}

TEST(emf_getbits_deleted_handle) {
    REQUIRE(U(GetEnhMetaFileBits));
    HENHMETAFILE h = mk_scene();
    if (!h) { CHECK(h != NULL); return; }
    U(DeleteEnhMetaFile)(h);
    CHECK(U(GetEnhMetaFileBits)(h, 0, NULL) == 0);
}

TEST(emf_getbits_first_and_last_records) {
    REQUIRE(U(GetEnhMetaFileBits));
    HENHMETAFILE h = mk_scene();
    if (!h) { CHECK(h != NULL); return; }
    UINT n; BYTE *b = emf_bits(h, &n);
    CHECK(b != NULL);
    if (b) {
        const ENHMETARECORD *first = (const ENHMETARECORD *)b;
        EXPECT_EQ(first->iType, EMR_HEADER);
        const DWORD *eof = (const DWORD *)(b + n - 20);
        EXPECT_EQ(eof[0], EMR_EOF);
        EXPECT_EQ(eof[1], 20);
        EXPECT_EQ(eof[2], 0);       /* nPalEntries */
        EXPECT_EQ(eof[3], 16);      /* offPalEntries */
        EXPECT_EQ(eof[4], 20);      /* nSizeLast */
        free(b);
    }
    U(DeleteEnhMetaFile)(h);
}

/* ---------- SetEnhMetaFileBits ---------- */

TEST(emf_setbits_roundtrip) {
    REQUIRE(U(SetEnhMetaFileBits)); REQUIRE(U(GetEnhMetaFileBits));
    HENHMETAFILE h = mk_scene();
    if (!h) { CHECK(h != NULL); return; }
    UINT n; BYTE *b = emf_bits(h, &n);
    HENHMETAFILE h2 = U(SetEnhMetaFileBits)(n, b);
    CHECK(h2 != NULL && h2 != h);
    if (h2) {
        UINT n2; BYTE *b2 = emf_bits(h2, &n2);
        CHECK(n2 == n && b2 && !memcmp(b, b2, n));
        free(b2);
        EXPECT_EQ(U(GetObjectType)(h2), OBJ_ENHMETAFILE);
        U(DeleteEnhMetaFile)(h2);
    }
    free(b);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_setbits_copies_buffer) {
    REQUIRE(U(SetEnhMetaFileBits));
    HENHMETAFILE h = mk_scene();
    if (!h) { CHECK(h != NULL); return; }
    UINT n; BYTE *b = emf_bits(h, &n);
    HENHMETAFILE h2 = U(SetEnhMetaFileBits)(n, b);
    memset(b, 0, n);        /* the metafile must not alias the caller's buffer */
    if (h2) {
        ENHMETAHEADER hd;
        CHECK(U(GetEnhMetaFileHeader)(h2, sizeof hd, &hd) == 108);
        EXPECT_EQ(hd.dSignature, ENHMETA_SIGNATURE);
        U(DeleteEnhMetaFile)(h2);
    } else CHECK(h2 != NULL);
    free(b);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_setbits_larger_nsize_ok) {
    REQUIRE(U(SetEnhMetaFileBits));
    BYTE buf[256]; UINT n = emf_build_min(buf, 108);
    memset(buf + n, 0x77, 100);
    HENHMETAFILE h = U(SetEnhMetaFileBits)(n + 100, buf);
    CHECK(h != NULL);
    if (h) {
        SOFT(U(GetEnhMetaFileBits)(h, 0, NULL) == n);
        U(DeleteEnhMetaFile)(h);
    }
}

TEST(emf_setbits_minimal_hand_built) {
    REQUIRE(U(SetEnhMetaFileBits)); REQUIRE(U(GetEnhMetaFileHeader));
    static const UINT hs[] = { 88, 100, 108 };
    for (unsigned i = 0; i < 3; i++) {
        BYTE buf[256]; UINT n = emf_build_min(buf, hs[i]);
        HENHMETAFILE h = U(SetEnhMetaFileBits)(n, buf);
        CHECKF(h != NULL, "header %u", hs[i]);
        if (!h) continue;
        EXPECT_EQ(U(GetEnhMetaFileBits)(h, 0, NULL), n);
        EXPECT_EQ(U(GetEnhMetaFileHeader)(h, 0, NULL), hs[i]);
        ENHMETAHEADER hd; memset(&hd, 0xCC, sizeof hd);
        UINT r = U(GetEnhMetaFileHeader)(h, 108, &hd);
        EXPECT_EQ(r, 108);   /* old headers are widened */
        EXPECT_EQ(hd.nSize, 108);
        EXPECT_EQ(hd.nRecords, 2);
        EXPECT_EQ(hd.szlDevice.cx, 1024);
        if (hs[i] < 100) { EXPECT_EQ(hd.cbPixelFormat, 0); EXPECT_EQ(hd.offPixelFormat, 0); EXPECT_EQ(hd.bOpenGL, 0); }
        if (hs[i] < 108) { EXPECT_EQ(hd.szlMicrometers.cx, 0); EXPECT_EQ(hd.szlMicrometers.cy, 0); }
        else { EXPECT_EQ(hd.szlMicrometers.cx, 320000); }
        U(DeleteEnhMetaFile)(h);
    }
}

TEST(emf_setbits_old_header_get_88_100) {
    REQUIRE(U(SetEnhMetaFileBits)); REQUIRE(U(GetEnhMetaFileHeader));
    BYTE buf[256]; UINT n = emf_build_min(buf, 88);
    HENHMETAFILE h = U(SetEnhMetaFileBits)(n, buf);
    if (!h) { CHECK(h != NULL); return; }
    ENHMETAHEADER hd;
    EXPECT_EQ(U(GetEnhMetaFileHeader)(h, 88, &hd), 88);
    EXPECT_EQ(hd.nSize, 88);
    EXPECT_EQ(U(GetEnhMetaFileHeader)(h, 100, &hd), 100);
    EXPECT_EQ(hd.nSize, 100);
    EXPECT_EQ(U(GetEnhMetaFileHeader)(h, 99, &hd), 88);
    /* an 88-byte header is only widened for requests of exactly 100 or 108 bytes */
    EXPECT_EQ(U(GetEnhMetaFileHeader)(h, 107, &hd), 88);
    EXPECT_EQ(U(GetEnhMetaFileHeader)(h, 108, &hd), 108);
    EXPECT_EQ(U(GetEnhMetaFileHeader)(h, 300, &hd), 88);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_setbits_nsize_smaller_than_nbytes) {
    REQUIRE(U(SetEnhMetaFileBits));
    BYTE buf[256]; UINT n = emf_build_min(buf, 108);
    static const UINT sizes[] = { 88, 100, 108, 120, 127 };
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        SetLastError(0);
        HENHMETAFILE h = U(SetEnhMetaFileBits)(sizes[i], buf);
        CHECKF(h == NULL, "size %u of %u", sizes[i], n);
        if (!h) EXPECT_ERR(0, ERROR_INVALID_DATA); else U(DeleteEnhMetaFile)(h);
    }
}

TEST(emf_setbits_tiny_sizes) {
    REQUIRE(U(SetEnhMetaFileBits));
    BYTE buf[256]; emf_build_min(buf, 108);
    static const UINT sizes[] = { 0, 1, 4, 8, 50, 87 };
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        SetLastError(0);
        HENHMETAFILE h = U(SetEnhMetaFileBits)(sizes[i], buf);
        CHECKF(h == NULL, "size %u", sizes[i]);
        if (!h) EXPECT_ERR(0, ERROR_INVALID_DATA); else U(DeleteEnhMetaFile)(h);
    }
}

TEST(emf_setbits_bad_nbytes_field) {
    REQUIRE(U(SetEnhMetaFileBits));
    BYTE buf[256]; emf_build_min(buf, 108);
    static const DWORD nb[] = { 0, 1, 87 };
    for (unsigned i = 0; i < 3; i++) {
        emf_build_min(buf, 108);
        ((ENHMETAHEADER *)buf)->nBytes = nb[i];
        SetLastError(0);
        HENHMETAFILE h = U(SetEnhMetaFileBits)(128, buf);
        CHECKF(h == NULL, "nBytes %lu", nb[i]);
        if (!h) EXPECT_ERR(0, ERROR_INVALID_DATA); else U(DeleteEnhMetaFile)(h);
    }
}

TEST(emf_setbits_bad_signature) {
    REQUIRE(U(SetEnhMetaFileBits));
    BYTE buf[256]; UINT n = emf_build_min(buf, 108);
    ((ENHMETAHEADER *)buf)->dSignature = 0x12345678;
    SetLastError(0);
    HENHMETAFILE h = U(SetEnhMetaFileBits)(n, buf);
    t_note("bad signature accepted=%d err=%lu", h != NULL, GetLastError());
    if (h) {
        /* if accepted at creation it must not be usable as a valid metafile */
        ENHMETAHEADER hd;
        UINT r = U(GetEnhMetaFileHeader)(h, sizeof hd, &hd);
        t_note("header on bad-signature emf: %u", r);
        U(DeleteEnhMetaFile)(h);
    }
}

TEST(emf_setbits_truncated_real_emf) {
    REQUIRE(U(SetEnhMetaFileBits));
    HENHMETAFILE h = mk_scene();
    if (!h) { CHECK(h != NULL); return; }
    UINT n; BYTE *b = emf_bits(h, &n);
    UINT cuts[] = { n - 1, n - 4, n - 20, n / 2, 108 };
    for (unsigned i = 0; i < sizeof cuts / sizeof cuts[0]; i++) {
        SetLastError(0);
        HENHMETAFILE t = U(SetEnhMetaFileBits)(cuts[i], b);
        CHECKF(t == NULL, "cut to %u of %u", cuts[i], n);
        if (!t) EXPECT_ERR(0, ERROR_INVALID_DATA); else U(DeleteEnhMetaFile)(t);
    }
    free(b);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_setbits_missing_eof) {
    REQUIRE(U(SetEnhMetaFileBits));
    BYTE buf[256]; emf_build_min(buf, 108);
    /* header claims 2 records, bytes are exactly the header: no EOF */
    ((ENHMETAHEADER *)buf)->nBytes = 108;
    HENHMETAFILE h = U(SetEnhMetaFileBits)(108, buf);
    t_note("emf without EOF accepted=%d", h != NULL);
    if (h) U(DeleteEnhMetaFile)(h);
}

TEST(emf_setbits_no_leak) {
    REQUIRE(U(SetEnhMetaFileBits));
    BYTE buf[256]; UINT n = emf_build_min(buf, 108);
    DWORD g0 = emf_gdi_count();
    for (int i = 0; i < 500; i++) {
        HENHMETAFILE h = U(SetEnhMetaFileBits)(n, buf);
        if (!h) { CHECK(h != NULL); break; }
        U(DeleteEnhMetaFile)(h);
    }
    for (int i = 0; i < 100; i++) CHECK(U(SetEnhMetaFileBits)(40, buf) == NULL);
    EXPECT_EQ(emf_gdi_count(), g0);
}

/* ---------- GetEnhMetaFile A/W ---------- */

static void write_scene_file(const WCHAR *p, BYTE **keep, UINT *nkeep) {
    HENHMETAFILE h = mk_scene();
    UINT n; BYTE *b = emf_bits(h, &n);
    emf_writefile(p, b, n);
    U(DeleteEnhMetaFile)(h);
    *keep = b; *nkeep = n;
}

TEST(emf_getfile_w_reads_bits) {
    REQUIRE(U(GetEnhMetaFileW));
    WCHAR p[MAX_PATH]; emf_tmpname(p, L"gw");
    BYTE *orig; UINT n; write_scene_file(p, &orig, &n);
    HENHMETAFILE h = U(GetEnhMetaFileW)(p);
    CHECK(h != NULL);
    if (h) {
        UINT n2; BYTE *b = emf_bits(h, &n2);
        CHECK(n2 == n && b && !memcmp(b, orig, n));
        free(b);
        EXPECT_EQ(U(GetObjectType)(h), OBJ_ENHMETAFILE);
        U(DeleteEnhMetaFile)(h);
    }
    free(orig);
    CHECK(DeleteFileW(p));
}

TEST(emf_getfile_a_equals_w) {
    REQUIRE(U(GetEnhMetaFileA)); REQUIRE(U(GetEnhMetaFileW));
    WCHAR p[MAX_PATH]; emf_tmpname(p, L"ga");
    BYTE *orig; UINT n; write_scene_file(p, &orig, &n);
    char pa[MAX_PATH];
    WideCharToMultiByte(CP_ACP, 0, p, -1, pa, sizeof pa, NULL, NULL);
    HENHMETAFILE ha = U(GetEnhMetaFileA)(pa), hw = U(GetEnhMetaFileW)(p);
    CHECK(ha && hw && ha != hw);
    if (ha && hw) {
        UINT na, nw; BYTE *ba = emf_bits(ha, &na), *bw = emf_bits(hw, &nw);
        CHECK(na == nw && ba && bw && !memcmp(ba, bw, na));
        free(ba); free(bw);
    }
    if (ha) U(DeleteEnhMetaFile)(ha);
    if (hw) U(DeleteEnhMetaFile)(hw);
    free(orig); DeleteFileW(p);
}

TEST(emf_getfile_same_file_twice) {
    REQUIRE(U(GetEnhMetaFileW));
    WCHAR p[MAX_PATH]; emf_tmpname(p, L"g2");
    BYTE *orig; UINT n; write_scene_file(p, &orig, &n);
    HENHMETAFILE a = U(GetEnhMetaFileW)(p), b = U(GetEnhMetaFileW)(p);
    CHECK(a && b && a != b);
    if (a) U(DeleteEnhMetaFile)(a);
    if (b) { ENHMETAHEADER hd; CHECK(U(GetEnhMetaFileHeader)(b, sizeof hd, &hd) == 108); U(DeleteEnhMetaFile)(b); }
    free(orig); DeleteFileW(p);
}

TEST(emf_getfile_missing_file) {
    REQUIRE(U(GetEnhMetaFileW)); REQUIRE(U(GetEnhMetaFileA));
    SetLastError(0);
    CHECK(U(GetEnhMetaFileW)(L"Z:\\g32emf_missing_dir\\none.emf") == NULL);
    CHECK(U(GetEnhMetaFileA)("Z:\\g32emf_missing_dir\\none.emf") == NULL);
    CHECK(U(GetEnhMetaFileW)(L"") == NULL);
    CHECK(U(GetEnhMetaFileA)("") == NULL);
    WCHAR p[MAX_PATH]; emf_tmpname(p, L"nx");
    CHECK(U(GetEnhMetaFileW)(p) == NULL);
}

TEST(emf_getfile_a_path_length_limit) {
    REQUIRE(U(GetEnhMetaFileA));
    char longp[400];
    memset(longp, 'a', sizeof longp); longp[260] = 0;
    SetLastError(0);
    CHECK(U(GetEnhMetaFileA)(longp) == NULL);
    EXPECT_ERR(0, ERROR_FILENAME_EXCED_RANGE);
    memset(longp, 'b', sizeof longp); longp[300] = 0;
    SetLastError(0);
    CHECK(U(GetEnhMetaFileA)(longp) == NULL);
    EXPECT_ERR(0, ERROR_FILENAME_EXCED_RANGE);
    memset(longp, 'c', sizeof longp); longp[259] = 0;
    SetLastError(0);
    CHECK(U(GetEnhMetaFileA)(longp) == NULL);
    CHECK(GetLastError() != ERROR_FILENAME_EXCED_RANGE || 1);
}

TEST(emf_getfile_empty_file) {
    REQUIRE(U(GetEnhMetaFileW));
    WCHAR p[MAX_PATH]; emf_tmpname(p, L"em");
    emf_writefile(p, "", 0);
    CHECK(U(GetEnhMetaFileW)(p) == NULL);
    DeleteFileW(p);
}

TEST(emf_getfile_garbage_file) {
    REQUIRE(U(GetEnhMetaFileW));
    WCHAR p[MAX_PATH]; emf_tmpname(p, L"gb");
    BYTE junk[400]; for (int i = 0; i < 400; i++) junk[i] = (BYTE)(i * 7 + 3);
    emf_writefile(p, junk, sizeof junk);
    HENHMETAFILE h = U(GetEnhMetaFileW)(p);
    CHECK(h == NULL);
    if (h) U(DeleteEnhMetaFile)(h);
    DeleteFileW(p);
}

TEST(emf_getfile_short_files) {
    REQUIRE(U(GetEnhMetaFileW));
    static const DWORD lens[] = { 1, 4, 8, 40, 87 };
    for (unsigned i = 0; i < sizeof lens / sizeof lens[0]; i++) {
        WCHAR p[MAX_PATH]; emf_tmpname(p, L"sh");
        BYTE buf[128]; memset(buf, 0, sizeof buf);
        emf_build_min(buf, 108);
        emf_writefile(p, buf, lens[i]);
        HENHMETAFILE h = U(GetEnhMetaFileW)(p);
        CHECKF(h == NULL, "len %lu", lens[i]);
        if (h) U(DeleteEnhMetaFile)(h);
        DeleteFileW(p);
    }
}

TEST(emf_getfile_truncated_valid) {
    REQUIRE(U(GetEnhMetaFileW));
    WCHAR p[MAX_PATH]; emf_tmpname(p, L"tr");
    BYTE *orig; UINT n; write_scene_file(p, &orig, &n);
    emf_writefile(p, orig, n - 8);
    HENHMETAFILE h = U(GetEnhMetaFileW)(p);
    CHECK(h == NULL);
    if (h) U(DeleteEnhMetaFile)(h);
    free(orig); DeleteFileW(p);
}

TEST(emf_getfile_trailing_garbage) {
    REQUIRE(U(GetEnhMetaFileW));
    WCHAR p[MAX_PATH]; emf_tmpname(p, L"tg");
    BYTE *orig; UINT n; write_scene_file(p, &orig, &n);
    BYTE *big = malloc(n + 64); memcpy(big, orig, n); memset(big + n, 0x42, 64);
    emf_writefile(p, big, n + 64);
    HENHMETAFILE h = U(GetEnhMetaFileW)(p);
    t_note("trailing garbage accepted=%d", h != NULL);
    if (h) { SOFT(U(GetEnhMetaFileBits)(h, 0, NULL) == n); U(DeleteEnhMetaFile)(h); }
    free(big); free(orig); DeleteFileW(p);
}

TEST(emf_getfile_bad_signature) {
    REQUIRE(U(GetEnhMetaFileW));
    WCHAR p[MAX_PATH]; emf_tmpname(p, L"bs");
    BYTE buf[128]; UINT n = emf_build_min(buf, 108);
    ((ENHMETAHEADER *)buf)->dSignature = 0xCAFEBABE;
    emf_writefile(p, buf, n);
    HENHMETAFILE h = U(GetEnhMetaFileW)(p);
    CHECK(h == NULL);
    if (h) U(DeleteEnhMetaFile)(h);
    DeleteFileW(p);
}

TEST(emf_getfile_wrong_itype) {
    REQUIRE(U(GetEnhMetaFileW));
    WCHAR p[MAX_PATH]; emf_tmpname(p, L"wt");
    BYTE buf[128]; UINT n = emf_build_min(buf, 108);
    ((ENHMETAHEADER *)buf)->iType = 2;
    emf_writefile(p, buf, n);
    HENHMETAFILE h = U(GetEnhMetaFileW)(p);
    t_note("iType=2 header accepted=%d", h != NULL);
    if (h) U(DeleteEnhMetaFile)(h);
    DeleteFileW(p);
}

TEST(emf_getfile_hand_built_valid) {
    REQUIRE(U(GetEnhMetaFileW));
    WCHAR p[MAX_PATH]; emf_tmpname(p, L"hb");
    BYTE buf[128]; UINT n = emf_build_min(buf, 108);
    emf_writefile(p, buf, n);
    HENHMETAFILE h = U(GetEnhMetaFileW)(p);
    CHECK(h != NULL);
    if (h) {
        ENHMETAHEADER hd; CHECK(U(GetEnhMetaFileHeader)(h, sizeof hd, &hd) == 108);
        EXPECT_EQ(hd.szlDevice.cx, 1024);
        U(DeleteEnhMetaFile)(h);
    }
    DeleteFileW(p);
}

TEST(emf_getfile_locks_until_deleted) {
    REQUIRE(U(GetEnhMetaFileW));
    WCHAR p[MAX_PATH]; emf_tmpname(p, L"lk");
    BYTE *orig; UINT n; write_scene_file(p, &orig, &n);
    HENHMETAFILE h = U(GetEnhMetaFileW)(p);
    if (h) {
        BOOL del = DeleteFileW(p);
        t_note("delete file while emf handle open: %d (err %lu)", del, del ? 0 : GetLastError());
        U(DeleteEnhMetaFile)(h);
        if (!del) CHECK(DeleteFileW(p));
    } else CHECK(h != NULL);
    free(orig);
}

TEST(emf_getfile_no_leak) {
    REQUIRE(U(GetEnhMetaFileW));
    WCHAR p[MAX_PATH]; emf_tmpname(p, L"nl");
    BYTE *orig; UINT n; write_scene_file(p, &orig, &n);
    DWORD g0 = emf_gdi_count();
    for (int i = 0; i < 200; i++) {
        HENHMETAFILE h = U(GetEnhMetaFileW)(p);
        if (!h) { CHECK(h != NULL); break; }
        U(DeleteEnhMetaFile)(h);
    }
    EXPECT_EQ(emf_gdi_count(), g0);
    free(orig); DeleteFileW(p);
}

/* ---------- CopyEnhMetaFile ---------- */

TEST(emf_copy_to_memory_w) {
    REQUIRE(U(CopyEnhMetaFileW));
    HENHMETAFILE h = mk_scene();
    if (!h) { CHECK(h != NULL); return; }
    HENHMETAFILE c = U(CopyEnhMetaFileW)(h, NULL);
    CHECK(c != NULL && c != h);
    if (c) {
        UINT n1, n2; BYTE *a = emf_bits(h, &n1), *b = emf_bits(c, &n2);
        CHECK(n1 == n2 && a && b && !memcmp(a, b, n1));
        free(a); free(b);
        /* independent lifetime */
        CHECK(U(DeleteEnhMetaFile)(h)); h = NULL;
        CHECK(U(GetEnhMetaFileBits)(c, 0, NULL) == n1);
        U(DeleteEnhMetaFile)(c);
    }
    if (h) U(DeleteEnhMetaFile)(h);
}

TEST(emf_copy_to_memory_a) {
    REQUIRE(U(CopyEnhMetaFileA));
    HENHMETAFILE h = mk_scene();
    if (!h) { CHECK(h != NULL); return; }
    HENHMETAFILE c = U(CopyEnhMetaFileA)(h, NULL);
    CHECK(c != NULL);
    if (c) U(DeleteEnhMetaFile)(c);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_copy_to_file_w) {
    REQUIRE(U(CopyEnhMetaFileW));
    HENHMETAFILE h = mk_scene();
    if (!h) { CHECK(h != NULL); return; }
    WCHAR p[MAX_PATH]; emf_tmpname(p, L"cf");
    HENHMETAFILE c = U(CopyEnhMetaFileW)(h, p);
    CHECK(c != NULL);
    UINT n; BYTE *b = emf_bits(h, &n);
    DWORD fn = 0; BYTE *fb = emf_readfile(p, &fn);
    CHECK(fb && fn == n && b && !memcmp(fb, b, n));
    free(b); free(fb);
    if (c) {
        UINT n2 = U(GetEnhMetaFileBits)(c, 0, NULL);
        EXPECT_EQ(n2, n);
        U(DeleteEnhMetaFile)(c);
    }
    U(DeleteEnhMetaFile)(h);
    CHECK(DeleteFileW(p));
}

TEST(emf_copy_to_file_a) {
    REQUIRE(U(CopyEnhMetaFileA));
    HENHMETAFILE h = mk_scene();
    if (!h) { CHECK(h != NULL); return; }
    char tmp[MAX_PATH], p[MAX_PATH + 64];
    GetTempPathA(MAX_PATH, tmp);
    wsprintfA(p, "%sg32emf_copya_%lu.emf", tmp, GetCurrentProcessId());
    HENHMETAFILE c = U(CopyEnhMetaFileA)(h, p);
    CHECK(c != NULL);
    CHECK(GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES);
    if (c) U(DeleteEnhMetaFile)(c);
    U(DeleteEnhMetaFile)(h);
    DeleteFileA(p);
}

TEST(emf_copy_to_existing_file_overwrites) {
    REQUIRE(U(CopyEnhMetaFileW));
    HENHMETAFILE h = mk_scene();
    if (!h) { CHECK(h != NULL); return; }
    WCHAR p[MAX_PATH]; emf_tmpname(p, L"cx");
    char junk[5000]; memset(junk, 1, sizeof junk);
    emf_writefile(p, junk, sizeof junk);
    HENHMETAFILE c = U(CopyEnhMetaFileW)(h, p);
    CHECK(c != NULL);
    DWORD fn = 0; BYTE *fb = emf_readfile(p, &fn);
    CHECK(fb && fn == U(GetEnhMetaFileBits)(h, 0, NULL));
    free(fb);
    if (c) U(DeleteEnhMetaFile)(c);
    U(DeleteEnhMetaFile)(h);
    DeleteFileW(p);
}

TEST(emf_copy_to_bad_path) {
    REQUIRE(U(CopyEnhMetaFileW)); REQUIRE(U(CopyEnhMetaFileA));
    HENHMETAFILE h = mk_scene();
    if (!h) { CHECK(h != NULL); return; }
    CHECK(U(CopyEnhMetaFileW)(h, L"Z:\\g32emf_missing_dir\\x.emf") == NULL);
    CHECK(U(CopyEnhMetaFileA)(h, "Z:\\g32emf_missing_dir\\x.emf") == NULL);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_copy_invalid_handles) {
    REQUIRE(U(CopyEnhMetaFileW)); REQUIRE(U(CopyEnhMetaFileA));
    HENHMETAFILE bad[] = { NULL, (HENHMETAFILE)1, (HENHMETAFILE)0xdeadbeef };
    for (unsigned i = 0; i < 3; i++) {
        CHECK(U(CopyEnhMetaFileW)(bad[i], NULL) == NULL);
        CHECK(U(CopyEnhMetaFileA)(bad[i], NULL) == NULL);
    }
}

TEST(emf_copy_of_deleted) {
    REQUIRE(U(CopyEnhMetaFileW));
    HENHMETAFILE h = mk_scene();
    if (!h) { CHECK(h != NULL); return; }
    U(DeleteEnhMetaFile)(h);
    CHECK(U(CopyEnhMetaFileW)(h, NULL) == NULL);
}

TEST(emf_copy_chain) {
    REQUIRE(U(CopyEnhMetaFileW));
    HENHMETAFILE h = mk_scene(), prev = h;
    if (!h) { CHECK(h != NULL); return; }
    UINT n = U(GetEnhMetaFileBits)(h, 0, NULL);
    for (int i = 0; i < 20; i++) {
        HENHMETAFILE c = U(CopyEnhMetaFileW)(prev, NULL);
        if (!c) { CHECK(c != NULL); break; }
        EXPECT_EQ(U(GetEnhMetaFileBits)(c, 0, NULL), n);
        U(DeleteEnhMetaFile)(prev);
        prev = c;
    }
    U(DeleteEnhMetaFile)(prev);
}

TEST(emf_copy_preserves_description) {
    REQUIRE(U(CopyEnhMetaFileW));
    HENHMETAFILE h = mk_scene();
    if (!h) { CHECK(h != NULL); return; }
    HENHMETAFILE c = U(CopyEnhMetaFileW)(h, NULL);
    WCHAR a[64] = {0}, b[64] = {0};
    UINT na = U(GetEnhMetaFileDescriptionW)(h, 64, a), nb = c ? U(GetEnhMetaFileDescriptionW)(c, 64, b) : 0;
    CHECK(na == nb && na > 0 && !memcmp(a, b, na * 2));
    if (c) U(DeleteEnhMetaFile)(c);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_copy_no_leak) {
    REQUIRE(U(CopyEnhMetaFileW));
    HENHMETAFILE h = mk_scene();
    if (!h) { CHECK(h != NULL); return; }
    DWORD g0 = emf_gdi_count();
    for (int i = 0; i < 300; i++) {
        HENHMETAFILE c = U(CopyEnhMetaFileW)(h, NULL);
        if (!c) { CHECK(c != NULL); break; }
        U(DeleteEnhMetaFile)(c);
    }
    EXPECT_EQ(emf_gdi_count(), g0);
    U(DeleteEnhMetaFile)(h);
}

/* ---------- DeleteEnhMetaFile ---------- */

TEST(emf_delete_valid_then_invalid) {
    REQUIRE(U(DeleteEnhMetaFile));
    HENHMETAFILE h = mk_scene();
    if (!h) { CHECK(h != NULL); return; }
    CHECK(U(DeleteEnhMetaFile)(h));
    CHECK(U(DeleteEnhMetaFile)(h) == FALSE);
}

TEST(emf_delete_bogus) {
    REQUIRE(U(DeleteEnhMetaFile));
    CHECK(U(DeleteEnhMetaFile)(NULL) == FALSE);
    CHECK(U(DeleteEnhMetaFile)((HENHMETAFILE)1) == FALSE);
    CHECK(U(DeleteEnhMetaFile)((HENHMETAFILE)0xdeadbeef) == FALSE);
}

TEST(emf_delete_wrong_type) {
    REQUIRE(U(DeleteEnhMetaFile)); REQUIRE(U(CreateSolidBrush));
    HBRUSH b = U(CreateSolidBrush)(5);
    HDC m = U(CreateCompatibleDC)(NULL);
    CHECK(U(DeleteEnhMetaFile)((HENHMETAFILE)b) == FALSE);
    CHECK(U(DeleteEnhMetaFile)((HENHMETAFILE)m) == FALSE);
    CHECK(U(GetObjectType)(b) == OBJ_BRUSH);
    CHECK(U(GetObjectType)(m) == OBJ_MEMDC);
    U(DeleteDC)(m); U(DeleteObject)(b);
}

TEST(emf_delete_object_on_hemf) {
    if (emf_wine()) { t_skip("wine"); return; }
    REQUIRE(U(DeleteObject));
    HENHMETAFILE h = mk_scene();
    if (!h) { CHECK(h != NULL); return; }
    BOOL r = U(DeleteObject)((HGDIOBJ)h);
    t_note("DeleteObject(hemf) = %d", r);
    if (r) { SOFT(U(GetObjectType)(h) == 0); U(DeleteEnhMetaFile)(h); }
    else { CHECK(U(DeleteEnhMetaFile)(h)); }
}

TEST(emf_handle_not_reused_while_alive) {
    HENHMETAFILE hs[20];
    for (int i = 0; i < 20; i++) hs[i] = mk_scene();
    for (int i = 0; i < 20; i++) for (int j = i + 1; j < 20; j++) CHECK(hs[i] != hs[j]);
    for (int i = 0; i < 20; i++) if (hs[i]) U(DeleteEnhMetaFile)(hs[i]);
}

TEST(emf_hemf_gdi_count_accounting) {
    REQUIRE(U(GetEnhMetaFileBits));
    DWORD g0 = emf_gdi_count();
    HENHMETAFILE hs[10];
    for (int i = 0; i < 10; i++) hs[i] = mk_scene();
    DWORD g1 = emf_gdi_count();
    t_note("10 emfs: gdi count %lu -> %lu", g0, g1);
    CHECK(g1 >= g0);
    for (int i = 0; i < 10; i++) U(DeleteEnhMetaFile)(hs[i]);
    EXPECT_EQ(emf_gdi_count(), g0);
}
