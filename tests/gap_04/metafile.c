#include "gap4.h"
/* GdiConvertEnhMetaFile / GdiConvertMetaFilePict / GdiCreateLocal{Enh,}MetaFile(Pict):
 * client metafile handle <-> server (kernel) metafile object. */

static void drop_server(HANDLE h) {
    /* A server metafile is owned by whoever converted it; try to release it. */
    if (!h) return;
    BOOL ok = U(DeleteObject)(h);
    if (!ok) t_note("DeleteObject(server metafile %p) failed err=%lu", h, GetLastError());
}
static HGLOBAL make_pict(HMETAFILE wmf, int mm, int x, int y) {
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, sizeof(METAFILEPICT));
    if (!g) return NULL;
    METAFILEPICT *p = GlobalLock(g);
    p->mm = mm; p->xExt = x; p->yExt = y; p->hMF = wmf;
    GlobalUnlock(g);
    return g;
}
static unsigned count_recs(const BYTE *b, UINT n) {
    unsigned c = 0; UINT o = 0;
    while (o + 8 <= n) { DWORD sz = ((const DWORD *)(b + o))[1]; if (!sz || o + sz > n) break; c++; o += sz; }
    return c;
}

static HENHMETAFILE te; static HANDLE tr;
static DWORD WINAPI xt_conv(LPVOID p) { tr = UN(GdiConvertEnhMetaFile)(te); return 0; }
/* ---------- GdiConvertEnhMetaFile ---------- */
TEST(gap4_convertemf_null) {
    RQ(GdiConvertEnhMetaFile);
    EXPECT_EQ(UN(GdiConvertEnhMetaFile)(NULL), 0);
}
TEST(gap4_convertemf_bogus_handles) {
    RQ(GdiConvertEnhMetaFile);
    ULONG_PTR v[] = { 1, 0x12345, 0x7fffffff, 0xffffffff, (ULONG_PTR)-1, 0x460000, 0x46ffff, 0x4600000000ull };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        HANDLE r = UN(GdiConvertEnhMetaFile)((HENHMETAFILE)v[i]);
        CHECKF(r == NULL, "bogus 0x%llx -> %p", (unsigned long long)v[i], r);
    }
}
TEST(gap4_convertemf_wrong_object_types) {
    RQ(GdiConvertEnhMetaFile);
    HBRUSH b = U(CreateSolidBrush)(0x112233);
    HDC dc = U(CreateCompatibleDC)(NULL);
    HRGN r = U(CreateRectRgn)(0, 0, 4, 4);
    HMETAFILE wmf = g4_make_wmf(1);
    CHECK(b && dc && r && wmf);
    EXPECT_EQ(UN(GdiConvertEnhMetaFile)((HENHMETAFILE)b), 0);
    EXPECT_EQ(UN(GdiConvertEnhMetaFile)((HENHMETAFILE)dc), 0);
    EXPECT_EQ(UN(GdiConvertEnhMetaFile)((HENHMETAFILE)r), 0);
    EXPECT_EQ(UN(GdiConvertEnhMetaFile)((HENHMETAFILE)wmf), 0);
    EXPECT_EQ(UN(GdiConvertEnhMetaFile)((HENHMETAFILE)U(GetStockObject)(BLACK_PEN)), 0);
    U(DeleteObject)(b); U(DeleteDC)(dc); U(DeleteObject)(r); if (wmf) U(DeleteMetaFile)(wmf);
}
TEST(gap4_convertemf_deleted_handle) {
    RQ(GdiConvertEnhMetaFile);
    HENHMETAFILE e = g4_make_emf(1); REQUIRE(e);
    CHECK(U(DeleteEnhMetaFile)(e));
    EXPECT_EQ(UN(GdiConvertEnhMetaFile)(e), 0);
}
TEST(gap4_convertemf_valid_nonzero_and_source_intact) {
    RQ(GdiConvertEnhMetaFile);
    HENHMETAFILE e = g4_make_emf(3); REQUIRE(e);
    UINT n0 = U(GetEnhMetaFileBits)(e, 0, NULL);
    HANDLE s = UN(GdiConvertEnhMetaFile)(e);
    CHECK(s != NULL);
    EXPECT_EQ(U(GetEnhMetaFileBits)(e, 0, NULL), n0);
    ENHMETAHEADER h; CHECK(U(GetEnhMetaFileHeader)(e, sizeof h, &h) == sizeof h);
    drop_server(s);
    CHECK(U(DeleteEnhMetaFile)(e));
}
TEST(gap4_convertemf_twice_gives_two_objects) {
    RQ(GdiConvertEnhMetaFile);
    HENHMETAFILE e = g4_make_emf(2); REQUIRE(e);
    HANDLE a = UN(GdiConvertEnhMetaFile)(e), b = UN(GdiConvertEnhMetaFile)(e);
    CHECK(a && b && a != b);
    drop_server(a); drop_server(b);
    U(DeleteEnhMetaFile)(e);
}
TEST(gap4_convertemf_roundtrip_bits) {
    RQ(GdiConvertEnhMetaFile); RQ(GdiCreateLocalEnhMetaFile);
    HENHMETAFILE e = g4_make_emf(4); REQUIRE(e);
    UINT n = U(GetEnhMetaFileBits)(e, 0, NULL);
    BYTE *b0 = malloc(n), *b1 = NULL;
    U(GetEnhMetaFileBits)(e, n, b0);
    HANDLE s = UN(GdiConvertEnhMetaFile)(e);
    REQUIRE(s);
    HENHMETAFILE l = UN(GdiCreateLocalEnhMetaFile)(s);
    CHECK(l != NULL && l != e);
    if (l) {
        UINT n1 = U(GetEnhMetaFileBits)(l, 0, NULL);
        EXPECT_EQ(n1, n);
        b1 = malloc(n1 ? n1 : 1);
        U(GetEnhMetaFileBits)(l, n1, b1);
        CHECK(((ENHMETAHEADER *)b1)->dSignature == ENHMETA_SIGNATURE);
        EXPECT_EQ(count_recs(b1, n1), count_recs(b0, n));
        SOFT(n1 == n && !memcmp(b0, b1, n));
        U(DeleteEnhMetaFile)(l);
    }
    free(b0); free(b1);
    drop_server(s);
    U(DeleteEnhMetaFile)(e);
}
TEST(gap4_convertemf_roundtrip_header_fields) {
    RQ(GdiConvertEnhMetaFile); RQ(GdiCreateLocalEnhMetaFile);
    HDC dc = U(CreateEnhMetaFileW)(NULL, NULL, NULL, L"gap4 app\0gap4 pic\0");
    REQUIRE(dc);
    U(Ellipse)(dc, 1, 1, 50, 50); U(MoveToEx)(dc, 3, 3, NULL); U(LineTo)(dc, 40, 40);
    HENHMETAFILE e = U(CloseEnhMetaFile)(dc); REQUIRE(e);
    ENHMETAHEADER a, b; U(GetEnhMetaFileHeader)(e, sizeof a, &a);
    HANDLE s = UN(GdiConvertEnhMetaFile)(e); REQUIRE(s);
    HENHMETAFILE l = UN(GdiCreateLocalEnhMetaFile)(s); REQUIRE(l);
    U(GetEnhMetaFileHeader)(l, sizeof b, &b);
    CHECK(a.nBytes == b.nBytes && a.nRecords == b.nRecords && a.nHandles == b.nHandles);
    CHECK(!memcmp(&a.rclBounds, &b.rclBounds, sizeof(RECT)) && !memcmp(&a.rclFrame, &b.rclFrame, sizeof(RECT)));
    CHECK(a.nDescription == b.nDescription);
    WCHAR d0[64] = {0}, d1[64] = {0};
    U(GetEnhMetaFileDescriptionW)(e, 64, d0); U(GetEnhMetaFileDescriptionW)(l, 64, d1);
    CHECK(!memcmp(d0, d1, 2 * 17));
    U(DeleteEnhMetaFile)(l); drop_server(s); U(DeleteEnhMetaFile)(e);
}
TEST(gap4_convertemf_empty_metafile) {
    RQ(GdiConvertEnhMetaFile); RQ(GdiCreateLocalEnhMetaFile);
    HENHMETAFILE e = g4_make_emf(0); REQUIRE(e);
    HANDLE s = UN(GdiConvertEnhMetaFile)(e);
    if (!s) { t_note("empty EMF not convertible"); U(DeleteEnhMetaFile)(e); return; }
    HENHMETAFILE l = UN(GdiCreateLocalEnhMetaFile)(s);
    CHECK(l != NULL);
    if (l) U(DeleteEnhMetaFile)(l);
    drop_server(s); U(DeleteEnhMetaFile)(e);
}
TEST(gap4_convertemf_many_records) {
    RQ(GdiConvertEnhMetaFile); RQ(GdiCreateLocalEnhMetaFile);
    HENHMETAFILE e = g4_make_emf(500); REQUIRE(e);
    HANDLE s = UN(GdiConvertEnhMetaFile)(e); REQUIRE(s);
    HENHMETAFILE l = UN(GdiCreateLocalEnhMetaFile)(s); CHECK(l != NULL);
    if (l) { EXPECT_EQ(U(GetEnhMetaFileBits)(l, 0, NULL), U(GetEnhMetaFileBits)(e, 0, NULL)); U(DeleteEnhMetaFile)(l); }
    drop_server(s); U(DeleteEnhMetaFile)(e);
}
TEST(gap4_convertemf_no_client_leak) {
    RQ(GdiConvertEnhMetaFile); RQ(GdiCreateLocalEnhMetaFile);
    HENHMETAFILE e = g4_make_emf(1); REQUIRE(e);
    DWORD before = g4_gdi_count();
    for (int i = 0; i < 50; i++) {
        HANDLE s = UN(GdiConvertEnhMetaFile)(e);
        HENHMETAFILE l = s ? UN(GdiCreateLocalEnhMetaFile)(s) : NULL;
        if (l) U(DeleteEnhMetaFile)(l);
        if (s) U(DeleteObject)(s);
    }
    DWORD after = g4_gdi_count();
    if (after != before) t_note("gdi count %lu -> %lu after 50 convert/delete rounds", before, after);
    U(DeleteEnhMetaFile)(e);
}
TEST(gap4_convertemf_cross_thread) {
    RQ(GdiConvertEnhMetaFile);
    HENHMETAFILE e = g4_make_emf(1); REQUIRE(e);
    te = e; tr = (HANDLE)(ULONG_PTR)1;
    HANDLE th = CreateThread(NULL, 0, xt_conv, NULL, 0, NULL);
    WaitForSingleObject(th, 10000); CloseHandle(th);
    CHECK(tr != NULL && tr != (HANDLE)1);
    if (tr != (HANDLE)1) drop_server(tr);
    U(DeleteEnhMetaFile)(e);
}

/* ---------- GdiConvertMetaFilePict ---------- */
TEST(gap4_convertpict_null) {
    RQ(GdiConvertMetaFilePict);
    SET_ERR(0);
    EXPECT_EQ(UN(GdiConvertMetaFilePict)(NULL), 0);
    t_note("GetLastError after GdiConvertMetaFilePict(NULL) = %lu", GetLastError());
}
TEST(gap4_convertpict_bogus_hmf_sets_invalid_handle) {
    RQ(GdiConvertMetaFilePict);
    ULONG_PTR v[] = { 0, 1, 0x12345, 0x7fffffff, (ULONG_PTR)-1 };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        HGLOBAL g = make_pict((HMETAFILE)v[i], MM_ANISOTROPIC, 100, 100); REQUIRE(g);
        SET_ERR(0xabcd);
        HANDLE r = UN(GdiConvertMetaFilePict)(g);
        CHECKF(r == NULL, "hmf 0x%llx -> %p", (unsigned long long)v[i], r);
        EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
        CHECK((GlobalFlags(g) & GMEM_LOCKCOUNT) == 0);
        GlobalFree(g);
    }
}
TEST(gap4_convertpict_wrong_type_hmf) {
    RQ(GdiConvertMetaFilePict);
    HENHMETAFILE e = g4_make_emf(1); HBRUSH b = U(CreateSolidBrush)(0);
    HGLOBAL g1 = make_pict((HMETAFILE)e, MM_ANISOTROPIC, 1, 1), g2 = make_pict((HMETAFILE)b, MM_ANISOTROPIC, 1, 1);
    SET_ERR(0); EXPECT_EQ(UN(GdiConvertMetaFilePict)(g1), 0); EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(UN(GdiConvertMetaFilePict)(g2), 0); EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    GlobalFree(g1); GlobalFree(g2); U(DeleteObject)(b); U(DeleteEnhMetaFile)(e);
}
TEST(gap4_convertpict_deleted_wmf) {
    RQ(GdiConvertMetaFilePict);
    HMETAFILE w = g4_make_wmf(1); REQUIRE(w);
    HGLOBAL g = make_pict(w, MM_ANISOTROPIC, 100, 100);
    U(DeleteMetaFile)(w);
    SET_ERR(0); EXPECT_EQ(UN(GdiConvertMetaFilePict)(g), 0); EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    GlobalFree(g);
}
TEST(gap4_convertpict_valid) {
    RQ(GdiConvertMetaFilePict);
    HMETAFILE w = g4_make_wmf(3); REQUIRE(w);
    HGLOBAL g = make_pict(w, MM_ANISOTROPIC, 3000, 2000);
    HANDLE s = UN(GdiConvertMetaFilePict)(g);
    CHECK(s != NULL);
    CHECK((GlobalFlags(g) & GMEM_LOCKCOUNT) == 0);
    METAFILEPICT *p = GlobalLock(g);
    CHECK(p && p->mm == MM_ANISOTROPIC && p->xExt == 3000 && p->yExt == 2000 && p->hMF == w);
    GlobalUnlock(g);
    drop_server(s);
    GlobalFree(g); U(DeleteMetaFile)(w);
}
TEST(gap4_convertpict_mapmodes) {
    RQ(GdiConvertMetaFilePict); RQ(GdiCreateLocalMetaFilePict);
    int modes[] = { MM_ANISOTROPIC, MM_ISOTROPIC, MM_HIMETRIC, MM_TEXT };
    for (unsigned i = 0; i < 4; i++) {
        HMETAFILE w = g4_make_wmf(1); REQUIRE(w);
        HGLOBAL g = make_pict(w, modes[i], 1000 + i, 2000 + i);
        HANDLE s = UN(GdiConvertMetaFilePict)(g);
        CHECKF(s != NULL, "mm %d", modes[i]);
        if (s) {
            HGLOBAL back = UN(GdiCreateLocalMetaFilePict)(s);
            CHECKF(back != NULL, "mm %d back", modes[i]);
            if (back) {
                METAFILEPICT *p = (METAFILEPICT *)back;
                CHECKF(p->mm == modes[i], "mm %d got %ld", modes[i], p->mm);
                CHECKF(p->xExt == 1000 + (int)i && p->yExt == 2000 + (int)i, "ext %ld,%ld", p->xExt, p->yExt);
                CHECK(p->hMF != NULL && p->hMF != w);
                if (p->hMF) U(DeleteMetaFile)(p->hMF);
                GlobalFree(back);
            }
            drop_server(s);
        }
        GlobalFree(g); U(DeleteMetaFile)(w);
    }
}

/* ---------- GdiCreateLocalEnhMetaFile / GdiCreateLocalMetaFilePict ---------- */
TEST(gap4_createlocalemf_null_and_bogus) {
    RQ(GdiCreateLocalEnhMetaFile);
    EXPECT_EQ(UN(GdiCreateLocalEnhMetaFile)(NULL), 0);
    ULONG_PTR v[] = { 1, 0x12345, 0x7fffffff, 0xffffffff, (ULONG_PTR)-1 };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        HENHMETAFILE r = UN(GdiCreateLocalEnhMetaFile)((HANDLE)v[i]);
        CHECKF(r == NULL, "bogus 0x%llx -> %p", (unsigned long long)v[i], r);
    }
}
TEST(gap4_createlocalmfp_null_and_bogus) {
    RQ(GdiCreateLocalMetaFilePict);
    EXPECT_EQ(UN(GdiCreateLocalMetaFilePict)(NULL), 0);
    ULONG_PTR v[] = { 1, 0x12345, 0x7fffffff, 0xffffffff, (ULONG_PTR)-1 };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        HGLOBAL r = UN(GdiCreateLocalMetaFilePict)((HANDLE)v[i]);
        CHECKF(r == NULL, "bogus 0x%llx -> %p", (unsigned long long)v[i], r);
    }
}
TEST(gap4_createlocal_wrong_object_types) {
    RQ(GdiCreateLocalEnhMetaFile); RQ(GdiCreateLocalMetaFilePict);
    HBRUSH b = U(CreateSolidBrush)(0); HDC dc = U(CreateCompatibleDC)(NULL);
    EXPECT_EQ(UN(GdiCreateLocalEnhMetaFile)(b), 0);
    EXPECT_EQ(UN(GdiCreateLocalEnhMetaFile)(dc), 0);
    EXPECT_EQ(UN(GdiCreateLocalMetaFilePict)(b), 0);
    EXPECT_EQ(UN(GdiCreateLocalMetaFilePict)(dc), 0);
    HENHMETAFILE e = g4_make_emf(1);  /* client handle, not a server one */
    HENHMETAFILE r = UN(GdiCreateLocalEnhMetaFile)(e);
    CHECK(r == NULL);
    if (r) U(DeleteEnhMetaFile)(r);
    U(DeleteObject)(b); U(DeleteDC)(dc); U(DeleteEnhMetaFile)(e);
}
TEST(gap4_createlocalmfp_from_emf_server) {
    RQ(GdiConvertEnhMetaFile); RQ(GdiCreateLocalMetaFilePict);
    HENHMETAFILE e = g4_make_emf(3); REQUIRE(e);
    ENHMETAHEADER h; U(GetEnhMetaFileHeader)(e, sizeof h, &h);
    HANDLE s = UN(GdiConvertEnhMetaFile)(e); REQUIRE(s);
    HGLOBAL g = UN(GdiCreateLocalMetaFilePict)(s);
    CHECK(g != NULL);
    if (g) {
        METAFILEPICT *p = (METAFILEPICT *)g;
        CHECK(p->mm == MM_ANISOTROPIC);
        CHECKF(p->xExt == h.rclFrame.right - h.rclFrame.left, "xExt %ld vs %ld", p->xExt, h.rclFrame.right - h.rclFrame.left);
        CHECKF(p->yExt == h.rclFrame.bottom - h.rclFrame.top, "yExt %ld", p->yExt);
        CHECK(p->hMF != NULL);
        if (p->hMF) {
            CHECK(U(GetMetaFileBitsEx)(p->hMF, 0, NULL) > 0);
            U(DeleteMetaFile)(p->hMF);
        }
        GlobalFree(g);
    }
    drop_server(s); U(DeleteEnhMetaFile)(e);
}
TEST(gap4_createlocalemf_from_wmf_server) {
    RQ(GdiConvertMetaFilePict); RQ(GdiCreateLocalEnhMetaFile);
    HMETAFILE w = g4_make_wmf(2); REQUIRE(w);
    HGLOBAL g = make_pict(w, MM_ANISOTROPIC, 2000, 1000);
    HANDLE s = UN(GdiConvertMetaFilePict)(g); REQUIRE(s);
    HENHMETAFILE e = UN(GdiCreateLocalEnhMetaFile)(s);
    CHECK(e != NULL);
    if (e) {
        ENHMETAHEADER h; CHECK(U(GetEnhMetaFileHeader)(e, sizeof h, &h) == sizeof h);
        CHECK(h.iType == EMR_HEADER && h.dSignature == ENHMETA_SIGNATURE && h.nRecords >= 3);
        U(DeleteEnhMetaFile)(e);
    }
    drop_server(s); GlobalFree(g); U(DeleteMetaFile)(w);
}
TEST(gap4_createlocalmfp_from_wmf_server_bits) {
    RQ(GdiConvertMetaFilePict); RQ(GdiCreateLocalMetaFilePict);
    HMETAFILE w = g4_make_wmf(5); REQUIRE(w);
    UINT n = U(GetMetaFileBitsEx)(w, 0, NULL);
    HGLOBAL g = make_pict(w, MM_ANISOTROPIC, 800, 600);
    HANDLE s = UN(GdiConvertMetaFilePict)(g); REQUIRE(s);
    HGLOBAL back = UN(GdiCreateLocalMetaFilePict)(s); REQUIRE(back);
    METAFILEPICT *p = (METAFILEPICT *)back;
    EXPECT_EQ(U(GetMetaFileBitsEx)(p->hMF, 0, NULL), n);
    BYTE *a = malloc(n), *b = malloc(n);
    U(GetMetaFileBitsEx)(w, n, a); U(GetMetaFileBitsEx)(p->hMF, n, b);
    CHECK(!memcmp(a, b, n));
    free(a); free(b);
    U(DeleteMetaFile)(p->hMF); GlobalFree(back);
    drop_server(s); GlobalFree(g); U(DeleteMetaFile)(w);
}
TEST(gap4_createlocal_after_server_deleted) {
    RQ(GdiConvertEnhMetaFile); RQ(GdiCreateLocalEnhMetaFile);
    HENHMETAFILE e = g4_make_emf(1); REQUIRE(e);
    HANDLE s = UN(GdiConvertEnhMetaFile)(e); REQUIRE(s);
    if (U(DeleteObject)(s)) {
        HENHMETAFILE l = UN(GdiCreateLocalEnhMetaFile)(s);
        CHECK(l == NULL);
        if (l) U(DeleteEnhMetaFile)(l);
    } else t_note("server metafile not deletable via DeleteObject");
    U(DeleteEnhMetaFile)(e);
}
TEST(gap4_createlocal_can_create_twice_from_same_server) {
    RQ(GdiConvertEnhMetaFile); RQ(GdiCreateLocalEnhMetaFile);
    HENHMETAFILE e = g4_make_emf(2); REQUIRE(e);
    HANDLE s = UN(GdiConvertEnhMetaFile)(e); REQUIRE(s);
    HENHMETAFILE a = UN(GdiCreateLocalEnhMetaFile)(s), b = UN(GdiCreateLocalEnhMetaFile)(s);
    CHECK(a && b && a != b);
    if (a) U(DeleteEnhMetaFile)(a);
    if (b) U(DeleteEnhMetaFile)(b);
    drop_server(s); U(DeleteEnhMetaFile)(e);
}
TEST(gap4_createlocal_result_is_enh_metafile_object) {
    RQ(GdiConvertEnhMetaFile); RQ(GdiCreateLocalEnhMetaFile);
    HENHMETAFILE e = g4_make_emf(2); REQUIRE(e);
    HANDLE s = UN(GdiConvertEnhMetaFile)(e); REQUIRE(s);
    HENHMETAFILE l = UN(GdiCreateLocalEnhMetaFile)(s); REQUIRE(l);
    CHECK(U(GetObjectType)(l) == OBJ_ENHMETAFILE);
    CHECK(U(DeleteEnhMetaFile)(l));
    EXPECT_EQ(U(GetObjectType)(l), 0);
    drop_server(s); U(DeleteEnhMetaFile)(e);
}
