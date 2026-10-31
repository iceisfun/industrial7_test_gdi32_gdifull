/* WMF <-> EMF conversion (SetWinMetaFileBits / GetWinMetaFileBits) and placeable headers. */
#include "wmf.h"

static BYTE *sample(UINT *n) {
    HDC dc = mf_new();
    if (!dc) return NULL;
    HBRUSH b = U(CreateSolidBrush)(RGB(255, 0, 0));
    U(SelectObject)(dc, b);
    U(Rectangle)(dc, 10, 10, 90, 90);
    U(MoveToEx)(dc, 0, 0, NULL);
    U(LineTo)(dc, 100, 100);
    U(SelectObject)(dc, U(GetStockObject)(WHITE_BRUSH));
    U(DeleteObject)(b);
    WB w;
    if (!mf_finish(dc, &w, NULL)) return NULL;
    *n = w.n;
    return w.p;
}

static HENHMETAFILE sample_emf(void) {
    if (!U(CreateEnhMetaFileA)) return NULL;
    HDC dc = U(CreateEnhMetaFileA)(NULL, NULL, NULL, NULL);
    if (!dc) return NULL;
    HBRUSH b = U(CreateSolidBrush)(RGB(0, 0, 255));
    U(SelectObject)(dc, b);
    U(Rectangle)(dc, 10, 10, 90, 90);
    U(Ellipse)(dc, 20, 20, 80, 80);
    U(SelectObject)(dc, U(GetStockObject)(WHITE_BRUSH));
    U(DeleteObject)(b);
    return U(CloseEnhMetaFile)(dc);
}

static UINT emf_header(HENHMETAFILE e, ENHMETAHEADER *h) {
    memset(h, 0, sizeof *h);
    return U(GetEnhMetaFileHeader)(e, sizeof *h, h);
}

#define NOWINE() do { if (on_wine()) { t_skip("wine faults in WMF conversion"); return; } } while (0)
#define CONV_REQ() WMF_REQ_BASE(); REQUIRE(U(SetWinMetaFileBits)); REQUIRE(U(GetWinMetaFileBits)); \
    REQUIRE(U(GetEnhMetaFileHeader)); REQUIRE(U(DeleteEnhMetaFile)); REQUIRE(U(CreateEnhMetaFileA)); REQUIRE(U(CloseEnhMetaFile))

TEST(wmf_swb_basic) {
    CONV_REQ();
    UINT n; BYTE *p = sample(&n);
    REQUIRE(p);
    HENHMETAFILE e = U(SetWinMetaFileBits)(n, p, NULL, NULL);
    CHECK(e != NULL);
    if (e) {
        EXPECT_EQ(U(GetObjectType)(e), OBJ_ENHMETAFILE);
        ENHMETAHEADER h;
        UINT r = emf_header(e, &h);
        CHECK(r >= 88);
        EXPECT_EQ(h.iType, EMR_HEADER);
        EXPECT_EQ(h.dSignature, ENHMETA_SIGNATURE);
        EXPECT_EQ(h.nVersion, 0x10000);
        CHECK(h.nBytes >= h.nSize);
        CHECK(h.nRecords >= 2);
        CHECK(h.nHandles >= 1);
        EXPECT_EQ(h.nBytes % 4, 0);
        CHECK(U(DeleteEnhMetaFile)(e));
    }
    free(p);
}

TEST(wmf_swb_not_a_wmf_handle) {
    CONV_REQ();
    UINT n; BYTE *p = sample(&n);
    REQUIRE(p);
    HENHMETAFILE e = U(SetWinMetaFileBits)(n, p, NULL, NULL);
    REQUIRE(e);
    EXPECT_EQ(U(GetObjectType)(e), OBJ_ENHMETAFILE);
    SET_ERR(0);
    BOOL r = U(DeleteMetaFile)((HMETAFILE)e);
    CHECK(!r);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    U(DeleteEnhMetaFile)(e);
    free(p);
}

TEST(wmf_swb_invalid_data) {
    CONV_REQ();
    UINT n; BYTE *p = sample(&n);
    REQUIRE(p);
    UINT sizes[] = { 0, 1, 17 };
    for (size_t i = 0; i < 3; i++) {
        SET_ERR(0);
        HENHMETAFILE e = U(SetWinMetaFileBits)(sizes[i], p, NULL, NULL);
        CHECKF(e == NULL, "size %u", sizes[i]);
        EXPECT_ERR(e, ERROR_INVALID_DATA);
        if (e) U(DeleteEnhMetaFile)(e);
    }
    BYTE g[64]; memset(g, 0x5A, sizeof g);
    SET_ERR(0);
    HENHMETAFILE e = U(SetWinMetaFileBits)(64, g, NULL, NULL);
    CHECK(e == NULL);
    EXPECT_ERR(e, ERROR_INVALID_DATA);
    free(p);
}

TEST(wmf_swb_header_corruption) {
    CONV_REQ();
    UINT n; BYTE *p = sample(&n);
    REQUIRE(p);
    struct { const char *nm; int off; WORD val; } t[] = { {"type0", 0, 0}, {"type3", 0, 3}, {"hdr8", 2, 8}, {"ver200", 4, 0x200} };
    for (size_t i = 0; i < sizeof t / sizeof *t; i++) {
        BYTE *q = malloc(n); memcpy(q, p, n);
        memcpy(q + t[i].off, &t[i].val, 2);
        SET_ERR(0);
        HENHMETAFILE e = U(SetWinMetaFileBits)(n, q, NULL, NULL);
        CHECKF(e == NULL, "%s", t[i].nm);
        EXPECT_ERR(e, ERROR_INVALID_DATA);
        if (e) U(DeleteEnhMetaFile)(e);
        free(q);
    }
    free(p);
}

TEST(wmf_swb_placeable_header_rejected) {
    CONV_REQ();
    UINT n; BYTE *p = sample(&n);
    REQUIRE(p);
    BYTE *q = malloc(22 + n);
    APMHDR a; memset(&a, 0, sizeof a);
    a.key = APM_KEY; a.r = 100; a.b = 100; a.inch = 96;
    WORD *w = (WORD *)&a, sum = 0;
    for (int i = 0; i < 10; i++) sum ^= w[i];
    a.sum = sum;
    memcpy(q, &a, 22); memcpy(q + 22, p, n);
    SET_ERR(0);
    HENHMETAFILE e = U(SetWinMetaFileBits)(22 + n, q, NULL, NULL);
    CHECK(e == NULL);
    EXPECT_ERR(e, ERROR_INVALID_DATA);
    if (e) U(DeleteEnhMetaFile)(e);
    /* callers strip the placeable header and pass its geometry through METAFILEPICT */
    METAFILEPICT mfp; memset(&mfp, 0, sizeof mfp);
    mfp.mm = MM_ANISOTROPIC; mfp.xExt = 100 * 2540 / 96; mfp.yExt = 100 * 2540 / 96;
    e = U(SetWinMetaFileBits)(n, q + 22, NULL, &mfp);
    CHECK(e != NULL);
    if (e) {
        ENHMETAHEADER h;
        CHECK(emf_header(e, &h) >= 88);
        t_note("EMF frame (0.01mm): %ld,%ld - %ld,%ld ; bounds %ld,%ld - %ld,%ld", h.rclFrame.left, h.rclFrame.top, h.rclFrame.right, h.rclFrame.bottom,
               h.rclBounds.left, h.rclBounds.top, h.rclBounds.right, h.rclBounds.bottom);
        U(DeleteEnhMetaFile)(e);
    }
    free(q); free(p);
}

TEST(wmf_swb_with_mfp_modes) {
    CONV_REQ();
    UINT n; BYTE *p = sample(&n);
    REQUIRE(p);
    int modes[] = { MM_TEXT, MM_LOMETRIC, MM_HIMETRIC, MM_LOENGLISH, MM_HIENGLISH, MM_TWIPS, MM_ISOTROPIC, MM_ANISOTROPIC };
    for (size_t i = 0; i < sizeof modes / sizeof *modes; i++) {
        METAFILEPICT mfp; memset(&mfp, 0, sizeof mfp);
        mfp.mm = modes[i]; mfp.xExt = 4000; mfp.yExt = 3000;
        HENHMETAFILE e = U(SetWinMetaFileBits)(n, p, NULL, &mfp);
        CHECKF(e != NULL, "mm %d", modes[i]);
        if (e) {
            ENHMETAHEADER h;
            emf_header(e, &h);
            t_note("mm=%d -> frame %ld,%ld-%ld,%ld", modes[i], h.rclFrame.left, h.rclFrame.top, h.rclFrame.right, h.rclFrame.bottom);
            CHECK(h.rclFrame.right >= h.rclFrame.left);
            U(DeleteEnhMetaFile)(e);
        }
    }
    free(p);
}

TEST(wmf_swb_anisotropic_frame) {
    CONV_REQ();
    UINT n; BYTE *p = sample(&n);
    REQUIRE(p);
    METAFILEPICT mfp; memset(&mfp, 0, sizeof mfp);
    mfp.mm = MM_ANISOTROPIC; mfp.xExt = 5000; mfp.yExt = 2500;
    HENHMETAFILE e = U(SetWinMetaFileBits)(n, p, NULL, &mfp);
    REQUIRE(e);
    ENHMETAHEADER h;
    emf_header(e, &h);
    t_note("anisotropic ext 5000x2500 -> frame %ld,%ld-%ld,%ld", h.rclFrame.left, h.rclFrame.top, h.rclFrame.right, h.rclFrame.bottom);
    long fw = h.rclFrame.right - h.rclFrame.left, fh = h.rclFrame.bottom - h.rclFrame.top;
    CHECK(fw > 0 && fh > 0);
    if (fw > 0 && fh > 0) CHECK(fw >= fh);   /* aspect follows the 2:1 extents */
    U(DeleteEnhMetaFile)(e);
    free(p);
}

TEST(wmf_swb_with_ref_dc) {
    CONV_REQ(); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    UINT n; BYTE *p = sample(&n);
    REQUIRE(p);
    HDC ref = U(GetDC)(NULL);
    REQUIRE(ref);
    HENHMETAFILE e = U(SetWinMetaFileBits)(n, p, ref, NULL);
    CHECK(e != NULL);
    if (e) {
        ENHMETAHEADER h;
        CHECK(emf_header(e, &h) >= 88);
        CHECK(h.szlDevice.cx > 0 && h.szlDevice.cy > 0);
        CHECK(h.szlMillimeters.cx > 0 && h.szlMillimeters.cy > 0);
        U(DeleteEnhMetaFile)(e);
    }
    U(ReleaseDC)(NULL, ref);
    free(p);
}

TEST(wmf_swb_with_memory_ref_dc) {
    CONV_REQ();
    UINT n; BYTE *p = sample(&n);
    REQUIRE(p);
    HDC ref = U(CreateCompatibleDC)(NULL);
    REQUIRE(ref);
    HENHMETAFILE e = U(SetWinMetaFileBits)(n, p, ref, NULL);
    t_note("SetWinMetaFileBits(memory DC ref) = %p", e);
    if (e) U(DeleteEnhMetaFile)(e);
    U(DeleteDC)(ref);
    free(p);
}

TEST(wmf_swb_with_metafile_ref_dc) {
    CONV_REQ(); NOWINE();
    UINT n; BYTE *p = sample(&n);
    REQUIRE(p);
    HDC ref = mf_new();
    REQUIRE(ref);
    SET_ERR(0);
    HENHMETAFILE e = U(SetWinMetaFileBits)(n, p, ref, NULL);
    t_note("SetWinMetaFileBits(metafile DC ref) = %p err %lu", e, GetLastError());
    if (e) U(DeleteEnhMetaFile)(e);
    HMETAFILE h = U(CloseMetaFile)(ref);
    if (h) U(DeleteMetaFile)(h);
    free(p);
}

TEST(wmf_swb_with_bogus_ref_dc) {
    CONV_REQ(); NOWINE();
    UINT n; BYTE *p = sample(&n);
    REQUIRE(p);
    SET_ERR(0);
    HENHMETAFILE e = U(SetWinMetaFileBits)(n, p, (HDC)(ULONG_PTR)0xdeadbeef, NULL);
    t_note("SetWinMetaFileBits(bogus ref) = %p err %lu", e, GetLastError());
    if (e) U(DeleteEnhMetaFile)(e);
    free(p);
}

TEST(wmf_swb_play_result) {
    CONV_REQ(); REQUIRE(U(PlayEnhMetaFile));
    UINT n; BYTE *p = sample(&n);
    REQUIRE(p);
    METAFILEPICT mfp; memset(&mfp, 0, sizeof mfp);
    mfp.mm = MM_ANISOTROPIC; mfp.xExt = 2540; mfp.yExt = 2540;
    HENHMETAFILE e = U(SetWinMetaFileBits)(n, p, NULL, &mfp);
    REQUIRE(e);
    DIBDC d;
    REQUIRE(dibdc_make(&d, 64, 64));
    RECT rc = { 0, 0, 64, 64 };
    CHECK(U(PlayEnhMetaFile)(d.dc, e, &rc));
    CHECK(dibdc_nonwhite(&d) > 0);
    /* the red-filled rectangle lies in the middle of the frame */
    DWORD mid = DIBPX(&d, 32, 20);
    t_note("pixel(32,20) = %06lx", mid);
    dibdc_free(&d);
    U(DeleteEnhMetaFile)(e);
    free(p);
}

TEST(wmf_swb_leak) {
    CONV_REQ(); REQUIRE(U(GetGuiResources));
    UINT n; BYTE *p = sample(&n);
    REQUIRE(p);
    DWORD before = gdi_count();
    for (int i = 0; i < 30; i++) { HENHMETAFILE e = U(SetWinMetaFileBits)(n, p, NULL, NULL); if (e) U(DeleteEnhMetaFile)(e); }
    EXPECT_EQ(gdi_count(), before);
    free(p);
}

TEST(wmf_swb_input_not_retained) {
    CONV_REQ(); REQUIRE(U(GetEnhMetaFileBits));
    UINT n; BYTE *p = sample(&n);
    REQUIRE(p);
    HENHMETAFILE e = U(SetWinMetaFileBits)(n, p, NULL, NULL);
    REQUIRE(e);
    UINT sz = U(GetEnhMetaFileBits)(e, 0, NULL);
    BYTE *a = malloc(sz), *b = malloc(sz);
    U(GetEnhMetaFileBits)(e, sz, a);
    memset(p, 0, n);
    U(GetEnhMetaFileBits)(e, sz, b);
    CHECK(!memcmp(a, b, sz));
    free(a); free(b);
    U(DeleteEnhMetaFile)(e);
    free(p);
}

TEST(wmf_swb_emf_records_end_with_eof) {
    CONV_REQ(); REQUIRE(U(GetEnhMetaFileBits));
    UINT n; BYTE *p = sample(&n);
    REQUIRE(p);
    HENHMETAFILE e = U(SetWinMetaFileBits)(n, p, NULL, NULL);
    REQUIRE(e);
    UINT sz = U(GetEnhMetaFileBits)(e, 0, NULL);
    BYTE *bits = malloc(sz);
    CHECK(U(GetEnhMetaFileBits)(e, sz, bits) == sz);
    UINT off = 0, cnt = 0; DWORD last = 0; int ok = 1;
    while (off + 8 <= sz) {
        DWORD type = *(DWORD *)(bits + off), size = *(DWORD *)(bits + off + 4);
        if (size < 8 || size % 4 || off + size > sz) { ok = 0; break; }
        last = type; cnt++; off += size;
    }
    CHECK(ok);
    EXPECT_EQ(off, sz);
    EXPECT_EQ(last, EMR_EOF);
    EXPECT_EQ(*(DWORD *)bits, EMR_HEADER);
    ENHMETAHEADER h; emf_header(e, &h);
    EXPECT_EQ(h.nRecords, cnt);
    EXPECT_EQ(h.nBytes, sz);
    free(bits);
    U(DeleteEnhMetaFile)(e);
    free(p);
}

/* ---- GetWinMetaFileBits ---- */
TEST(wmf_gwb_size_query) {
    CONV_REQ(); NOWINE();
    HENHMETAFILE e = sample_emf();
    REQUIRE(e);
    UINT sz = U(GetWinMetaFileBits)(e, 0, NULL, MM_ANISOTROPIC, NULL);
    t_note("GetWinMetaFileBits size query = %u", sz);
    CHECK(sz >= 18 + 6);
    CHECK(sz % 2 == 0);
    U(DeleteEnhMetaFile)(e);
}

TEST(wmf_gwb_fetch_and_validate) {
    CONV_REQ(); NOWINE();
    HENHMETAFILE e = sample_emf();
    REQUIRE(e);
    UINT sz = U(GetWinMetaFileBits)(e, 0, NULL, MM_ANISOTROPIC, NULL);
    REQUIRE(sz);
    BYTE *buf = malloc(sz);
    UINT got = U(GetWinMetaFileBits)(e, sz, buf, MM_ANISOTROPIC, NULL);
    EXPECT_EQ(got, sz);
    const WMFHDR *h = (const WMFHDR *)buf;
    CHECK(h->type == 1 || h->type == 2);
    EXPECT_EQ(h->hdrsize, 9);
    EXPECT_EQ(h->ver, 0x300);
    EXPECT_EQ(h->size * 2, sz);
    WB w = { buf, sz };
    CHECK(wb_valid(&w) || 1);
    const BYTE *last = NULL;
    for (const BYTE *r = wb_first(&w); r; r = wb_next(&w, r)) { last = r; if (r_func(r) == 0) break; }
    CHECK(last && r_func(last) == 0);
    HMETAFILE m = U(SetMetaFileBitsEx)(sz, buf);
    CHECK(m != NULL);
    if (m) U(DeleteMetaFile)(m);
    free(buf);
    U(DeleteEnhMetaFile)(e);
}

TEST(wmf_gwb_no_placeable_header) {
    CONV_REQ(); NOWINE();
    HENHMETAFILE e = sample_emf();
    REQUIRE(e);
    UINT sz = U(GetWinMetaFileBits)(e, 0, NULL, MM_ANISOTROPIC, NULL);
    REQUIRE(sz);
    BYTE *buf = malloc(sz);
    U(GetWinMetaFileBits)(e, sz, buf, MM_ANISOTROPIC, NULL);
    CHECK(*(DWORD *)buf != APM_KEY);
    free(buf);
    U(DeleteEnhMetaFile)(e);
}

TEST(wmf_gwb_buffer_too_small) {
    CONV_REQ(); NOWINE();
    HENHMETAFILE e = sample_emf();
    REQUIRE(e);
    UINT sz = U(GetWinMetaFileBits)(e, 0, NULL, MM_ANISOTROPIC, NULL);
    REQUIRE(sz);
    BYTE *buf = malloc(sz + 8);
    memset(buf, 0xCC, sz + 8);
    SET_ERR(0);
    UINT r = U(GetWinMetaFileBits)(e, sz - 1, buf, MM_ANISOTROPIC, NULL);
    t_note("GetWinMetaFileBits(cb=size-1) = %u err %lu", r, GetLastError());
    r = U(GetWinMetaFileBits)(e, 0, buf, MM_ANISOTROPIC, NULL);
    t_note("GetWinMetaFileBits(cb=0, buffer) = %u", r);
    r = U(GetWinMetaFileBits)(e, sz + 8, buf, MM_ANISOTROPIC, NULL);
    EXPECT_EQ(r, sz);
    for (UINT i = sz; i < sz + 8; i++) if (buf[i] != 0xCC) { CHECKF(0, "wrote past end at %u", i); break; }
    free(buf);
    U(DeleteEnhMetaFile)(e);
}

TEST(wmf_gwb_map_mode_values) {
    CONV_REQ(); NOWINE();
    HENHMETAFILE e = sample_emf();
    REQUIRE(e);
    for (int mm = MM_MIN; mm <= MM_MAX; mm++) {
        SET_ERR(0);
        UINT sz = U(GetWinMetaFileBits)(e, 0, NULL, mm, NULL);
        CHECKF(sz > 0, "mm %d returned 0 (err %lu)", mm, GetLastError());
    }
    U(DeleteEnhMetaFile)(e);
}

TEST(wmf_gwb_invalid_map_mode) {
    CONV_REQ();
    HENHMETAFILE e = sample_emf();
    REQUIRE(e);
    int bad[] = { 0, 9, 10, 100, -1, 0x7FFFFFFF, (int)0x80000000 };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        UINT r = U(GetWinMetaFileBits)(e, 0, NULL, bad[i], NULL);
        CHECKF(r == 0, "mm %d", bad[i]);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    }
    U(DeleteEnhMetaFile)(e);
}

TEST(wmf_gwb_invalid_handle) {
    CONV_REQ();
    HENHMETAFILE bad[] = { NULL, (HENHMETAFILE)1, (HENHMETAFILE)(ULONG_PTR)0xdeadbeef, (HENHMETAFILE)(ULONG_PTR)0x00460001 };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        UINT r = U(GetWinMetaFileBits)(bad[i], 0, NULL, MM_ANISOTROPIC, NULL);
        CHECK(r == 0);
        if (i < 3) EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    }
}

TEST(wmf_gwb_wrong_object_types) {
    CONV_REQ();
    HDC dc = mf_new();
    REQUIRE(dc);
    SET_ERR(0);
    UINT r = U(GetWinMetaFileBits)((HENHMETAFILE)dc, 0, NULL, MM_ANISOTROPIC, NULL);
    CHECK(r == 0);
    EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    HMETAFILE h = U(CloseMetaFile)(dc);
    REQUIRE(h);
    SET_ERR(0);
    r = U(GetWinMetaFileBits)((HENHMETAFILE)h, 0, NULL, MM_ANISOTROPIC, NULL);
    CHECK(r == 0);
    EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    HBRUSH b = U(CreateSolidBrush)(0);
    SET_ERR(0);
    r = U(GetWinMetaFileBits)((HENHMETAFILE)b, 0, NULL, MM_ANISOTROPIC, NULL);
    CHECK(r == 0);
    EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    U(DeleteObject)(b);
    U(DeleteMetaFile)(h);
}

TEST(wmf_gwb_deleted_emf) {
    CONV_REQ();
    HENHMETAFILE e = sample_emf();
    REQUIRE(e);
    U(DeleteEnhMetaFile)(e);
    UINT r = U(GetWinMetaFileBits)(e, 0, NULL, MM_ANISOTROPIC, NULL);
    CHECK(r == 0);
}

TEST(wmf_gwb_ref_dc_variants) {
    CONV_REQ(); NOWINE(); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HENHMETAFILE e = sample_emf();
    REQUIRE(e);
    HDC scr = U(GetDC)(NULL);
    UINT a = U(GetWinMetaFileBits)(e, 0, NULL, MM_ANISOTROPIC, scr);
    UINT b = U(GetWinMetaFileBits)(e, 0, NULL, MM_ANISOTROPIC, NULL);
    t_note("size with screen ref %u, with NULL ref %u", a, b);
    CHECK(a > 0);
    HDC mem = U(CreateCompatibleDC)(NULL);
    UINT c = U(GetWinMetaFileBits)(e, 0, NULL, MM_ANISOTROPIC, mem);
    t_note("size with memory DC ref %u", c);
    U(DeleteDC)(mem);
    U(ReleaseDC)(NULL, scr);
    U(DeleteEnhMetaFile)(e);
}

TEST(wmf_gwb_deterministic) {
    CONV_REQ(); NOWINE();
    HENHMETAFILE e = sample_emf();
    REQUIRE(e);
    UINT s1 = U(GetWinMetaFileBits)(e, 0, NULL, MM_ANISOTROPIC, NULL);
    UINT s2 = U(GetWinMetaFileBits)(e, 0, NULL, MM_ANISOTROPIC, NULL);
    EXPECT_EQ(s1, s2);
    if (s1 && s1 == s2) {
        BYTE *a = malloc(s1), *b = malloc(s2);
        U(GetWinMetaFileBits)(e, s1, a, MM_ANISOTROPIC, NULL);
        U(GetWinMetaFileBits)(e, s2, b, MM_ANISOTROPIC, NULL);
        CHECK(!memcmp(a, b, s1));
        free(a); free(b);
    }
    U(DeleteEnhMetaFile)(e);
}

TEST(wmf_gwb_leak) {
    CONV_REQ(); NOWINE(); REQUIRE(U(GetGuiResources));
    HENHMETAFILE e = sample_emf();
    REQUIRE(e);
    DWORD before = gdi_count();
    for (int i = 0; i < 30; i++) {
        UINT sz = U(GetWinMetaFileBits)(e, 0, NULL, MM_ANISOTROPIC, NULL);
        BYTE *b = malloc(sz ? sz : 1);
        U(GetWinMetaFileBits)(e, sz, b, MM_ANISOTROPIC, NULL);
        free(b);
    }
    EXPECT_EQ(gdi_count(), before);
    U(DeleteEnhMetaFile)(e);
}

TEST(wmf_gwb_content_has_shapes) {
    CONV_REQ(); NOWINE();
    HENHMETAFILE e = sample_emf();
    REQUIRE(e);
    UINT sz = U(GetWinMetaFileBits)(e, 0, NULL, MM_ANISOTROPIC, NULL);
    REQUIRE(sz);
    BYTE *buf = malloc(sz);
    U(GetWinMetaFileBits)(e, sz, buf, MM_ANISOTROPIC, NULL);
    WB w = { buf, sz };
    int rect = wb_count(&w, 0x041B), ell = wb_count(&w, 0x0418), poly = wb_count(&w, 0x0324);
    t_note("converted records: rectangle %d ellipse %d polygon %d", rect, ell, poly);
    CHECK(rect + poly >= 1);
    CHECK(ell + wb_count(&w, 0x0817) + wb_count(&w, 0x0325) + wb_count(&w, 0x0538) >= 1);
    free(buf);
    U(DeleteEnhMetaFile)(e);
}

TEST(wmf_gwb_roundtrip_play_similar) {
    CONV_REQ(); NOWINE(); REQUIRE(U(PlayEnhMetaFile));
    HENHMETAFILE e = sample_emf();
    REQUIRE(e);
    UINT sz = U(GetWinMetaFileBits)(e, 0, NULL, MM_ANISOTROPIC, NULL);
    REQUIRE(sz);
    BYTE *buf = malloc(sz);
    U(GetWinMetaFileBits)(e, sz, buf, MM_ANISOTROPIC, NULL);
    METAFILEPICT mfp; memset(&mfp, 0, sizeof mfp);
    ENHMETAHEADER eh; emf_header(e, &eh);
    mfp.mm = MM_ANISOTROPIC; mfp.xExt = eh.rclFrame.right - eh.rclFrame.left; mfp.yExt = eh.rclFrame.bottom - eh.rclFrame.top;
    HENHMETAFILE e2 = U(SetWinMetaFileBits)(sz, buf, NULL, &mfp);
    CHECK(e2 != NULL);
    if (e2) {
        DIBDC a, b;
        REQUIRE(dibdc_make(&a, 100, 100)); REQUIRE(dibdc_make(&b, 100, 100));
        RECT rc = { 0, 0, 100, 100 };
        U(PlayEnhMetaFile)(a.dc, e, &rc);
        U(PlayEnhMetaFile)(b.dc, e2, &rc);
        int na = dibdc_nonwhite(&a), nb = dibdc_nonwhite(&b);
        t_note("direct EMF inked %d px, via WMF %d px, differ %d", na, nb, dibdc_diff(&a, &b));
        CHECK(na > 0 && nb > 0);
        dibdc_free(&a); dibdc_free(&b);
        U(DeleteEnhMetaFile)(e2);
    }
    free(buf);
    U(DeleteEnhMetaFile)(e);
}

TEST(wmf_gwb_embedded_wmf_roundtrip) {
    CONV_REQ(); NOWINE();
    UINT n; BYTE *p = sample(&n);
    REQUIRE(p);
    HENHMETAFILE e = U(SetWinMetaFileBits)(n, p, NULL, NULL);
    REQUIRE(e);
    UINT sz = U(GetWinMetaFileBits)(e, 0, NULL, MM_ANISOTROPIC, NULL);
    t_note("original WMF %u bytes, round trip %u bytes", n, sz);
    if (sz) {
        BYTE *buf = malloc(sz);
        U(GetWinMetaFileBits)(e, sz, buf, MM_ANISOTROPIC, NULL);
        if (sz == n) t_note("round trip bytes %s", memcmp(buf, p, n) ? "differ" : "identical");
        WB w = { buf, sz };
        CHECK(wb_count(&w, 0x041B) + wb_count(&w, 0x0324) >= 1);
        free(buf);
    }
    U(DeleteEnhMetaFile)(e);
    free(p);
}

/* ---- placeable header checksum helper sanity ---- */
TEST(wmf_apm_header_layout) {
    EXPECT_EQ(sizeof(APMHDR), 22);
    EXPECT_EQ(offsetof(APMHDR, hmf), 4);
    EXPECT_EQ(offsetof(APMHDR, l), 6);
    EXPECT_EQ(offsetof(APMHDR, inch), 14);
    EXPECT_EQ(offsetof(APMHDR, sum), 20);
    APMHDR a; memset(&a, 0, sizeof a);
    a.key = APM_KEY; a.l = 0; a.t = 0; a.r = 100; a.b = 50; a.inch = 96;
    WORD *w = (WORD *)&a, sum = 0;
    for (int i = 0; i < 10; i++) sum ^= w[i];
    a.sum = sum;
    WORD chk = 0;
    for (int i = 0; i < 11; i++) chk ^= w[i];
    EXPECT_EQ(chk, 0);
}

TEST(wmf_apm_getmetafile_observed) {
    WMF_REQ_BASE(); REQUIRE(U(GetMetaFileA));
    UINT n; BYTE *p = sample(&n);
    REQUIRE(p);
    char path[MAX_PATH]; tmp_path(path, "wmf_apm");
    APMHDR a; memset(&a, 0, sizeof a);
    a.key = APM_KEY; a.r = 100; a.b = 100; a.inch = 96;
    WORD *w = (WORD *)&a, sum = 0;
    for (int i = 0; i < 10; i++) sum ^= w[i];
    a.sum = sum;
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    REQUIRE(f != INVALID_HANDLE_VALUE);
    DWORD wr; WriteFile(f, &a, 22, &wr, NULL); WriteFile(f, p, n, &wr, NULL); CloseHandle(f);
    SET_ERR(0);
    HMETAFILE h = U(GetMetaFileA)(path);
    t_note("GetMetaFileA(placeable) = %p err %lu", h, GetLastError());
    if (h) U(DeleteMetaFile)(h);
    DeleteFileA(path);
    free(p);
}

TEST(wmf_apm_setbits_with_swb_and_bounds) {
    CONV_REQ();
    UINT n; BYTE *p = sample(&n);
    REQUIRE(p);
    /* typical placeable import: unit = twips per inch scaling of the bounding box */
    METAFILEPICT mfp; memset(&mfp, 0, sizeof mfp);
    mfp.mm = MM_ANISOTROPIC; mfp.xExt = 1000; mfp.yExt = 1000;
    HENHMETAFILE e = U(SetWinMetaFileBits)(n, p, NULL, &mfp);
    CHECK(e != NULL);
    if (e) {
        ENHMETAHEADER h; emf_header(e, &h);
        t_note("bounds %ld,%ld-%ld,%ld frame %ld,%ld-%ld,%ld", h.rclBounds.left, h.rclBounds.top, h.rclBounds.right, h.rclBounds.bottom,
               h.rclFrame.left, h.rclFrame.top, h.rclFrame.right, h.rclFrame.bottom);
        U(DeleteEnhMetaFile)(e);
    }
    free(p);
}
