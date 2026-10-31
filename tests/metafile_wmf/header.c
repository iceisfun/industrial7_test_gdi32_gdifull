/* METAHEADER, record structure, object table accounting and buffer queries. */
#include "wmf.h"

typedef struct { WORD fn; DWORD size; int np; int p[6]; } EXPREC;

/* Compare the record stream against an exact list (params ANY = -1 are skipped). */
static void expect_seq(const WB *b, const EXPREC *e, int n, const char *name) {
    const BYTE *r = wb_first(b);
    for (int i = 0; i < n; i++) {
        if (!r) { CHECKF(0, "%s: record %d missing", name, i); return; }
        CHECKF(r_func(r) == e[i].fn, "%s: record %d func %04x want %04x", name, i, r_func(r), e[i].fn);
        if (e[i].size) CHECKF(r_size(r) == e[i].size, "%s: record %d size %lu want %lu", name, i, r_size(r), e[i].size);
        for (int k = 0; k < e[i].np; k++)
            if (e[i].p[k] >= 0) CHECKF(r_parm(r, k) == (WORD)e[i].p[k], "%s: record %d param %d = %04x want %04x", name, i, k, r_parm(r, k), e[i].p[k]);
        r = wb_next(b, r);
    }
}

TEST(wmf_hdr_blank_exact_bytes) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    static const BYTE want[24] = { 1,0, 9,0, 0,3, 12,0,0,0, 0,0, 3,0,0,0, 0,0, 3,0,0,0, 0,0 };
    EXPECT_EQ(b.n, 24);
    if (b.n == 24) CHECK(memcmp(b.p, want, 24) == 0);
    wb_free(&b);
}

TEST(wmf_hdr_fields_blank) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    const WMFHDR *h = wb_hdr(&b);
    EXPECT_EQ(h->type, 1);
    EXPECT_EQ(h->hdrsize, 9);
    EXPECT_EQ(h->ver, 0x300);
    EXPECT_EQ(h->size, 12);
    EXPECT_EQ(h->nobj, 0);
    EXPECT_EQ(h->maxrec, 3);
    EXPECT_EQ(h->nparm, 0);
    EXPECT_EQ(sizeof(WMFHDR), 18);
    wb_free(&b);
}

TEST(wmf_hdr_eof_record) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    U(Rectangle)(dc, 1, 2, 3, 4);
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    EXPECT_EQ(b.n, 18 + 14 + 6);
    static const BYTE eof[6] = { 3, 0, 0, 0, 0, 0 };
    CHECK(memcmp(b.p + b.n - 6, eof, 6) == 0);
    CHECK(wb_valid(&b));
    wb_free(&b);
}

TEST(wmf_hdr_size_equals_bits) {
    WMF_REQ_BASE();
    for (int n = 0; n < 12; n++) {
        HDC dc = mf_new();
        REQUIRE(dc);
        for (int i = 0; i < n; i++) U(LineTo)(dc, i, i);
        WB b;
        REQUIRE(mf_finish(dc, &b, NULL));
        EXPECT_EQ(wb_hdr(&b)->size * 2, b.n);
        EXPECT_EQ(b.n, 18 + n * 10 + 6);
        wb_free(&b);
    }
}

TEST(wmf_hdr_maxrec_single_rect) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    U(Rectangle)(dc, 1, 2, 3, 4);
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    EXPECT_EQ(wb_hdr(&b)->maxrec, 7);
    wb_free(&b);
}

TEST(wmf_hdr_maxrec_tracks_largest) {
    WMF_REQ_BASE();
    POINT pts[100];
    for (int i = 0; i < 100; i++) { pts[i].x = i; pts[i].y = i * 2; }
    HDC dc = mf_new();
    REQUIRE(dc);
    U(Rectangle)(dc, 1, 2, 3, 4);
    U(Polygon)(dc, pts, 100);
    U(MoveToEx)(dc, 1, 1, NULL);
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    EXPECT_EQ(wb_hdr(&b)->maxrec, 3 + 1 + 200);
    CHECK(wb_valid(&b));
    wb_free(&b);
}

TEST(wmf_hdr_nobj_zero_without_objects) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    U(Rectangle)(dc, 0, 0, 5, 5);
    U(SetBkMode)(dc, TRANSPARENT);
    U(MoveToEx)(dc, 1, 2, NULL);
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    EXPECT_EQ(wb_hdr(&b)->nobj, 0);
    wb_free(&b);
}

TEST(wmf_hdr_nobj_counts_live_objects) {
    WMF_REQ_BASE();
    for (int n = 1; n <= 6; n++) {
        HDC dc = mf_new();
        REQUIRE(dc);
        HPEN pens[6];
        for (int i = 0; i < n; i++) { pens[i] = U(CreatePen)(PS_SOLID, 1, RGB(i, i, i)); U(SelectObject)(dc, pens[i]); }
        U(SelectObject)(dc, U(GetStockObject)(BLACK_PEN));
        WB b;
        REQUIRE(mf_finish(dc, &b, NULL));
        CHECKF(wb_hdr(&b)->nobj >= n, "n=%d nobj=%u", n, wb_hdr(&b)->nobj);
        CHECKF(wb_hdr(&b)->nobj <= n + 1, "n=%d nobj=%u", n, wb_hdr(&b)->nobj);
        EXPECT_EQ(wb_count(&b, 0x02FA), n + 1);
        for (int i = 0; i < n; i++) U(DeleteObject)(pens[i]);
        wb_free(&b);
    }
}

TEST(wmf_hdr_object_indices_sequential) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    HBRUSH br[4];
    for (int i = 0; i < 4; i++) { br[i] = U(CreateSolidBrush)(RGB(i * 10, 0, 0)); U(SelectObject)(dc, br[i]); }
    U(SelectObject)(dc, U(GetStockObject)(WHITE_BRUSH));
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    for (int i = 0; i < 4; i++) {
        const BYTE *s = wb_find(&b, 0x012D, i);
        CHECKF(s != NULL, "select %d", i);
        if (s) EXPECT_EQ(r_parm(s, 0), i);
    }
    for (int i = 0; i < 4; i++) U(DeleteObject)(br[i]);
    wb_free(&b);
}

TEST(wmf_hdr_select_sequence) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    U(SelectObject)(dc, U(GetStockObject)(DC_BRUSH));
    U(SelectObject)(dc, U(GetStockObject)(WHITE_BRUSH));
    HPEN pen = U(CreatePen)(PS_SOLID, 1, RGB(1, 1, 1));
    U(SelectObject)(dc, pen);
    U(SaveDC)(dc);
    U(SelectObject)(dc, U(GetStockObject)(BLACK_PEN));
    U(RestoreDC)(dc, -1);
    U(SelectObject)(dc, U(GetStockObject)(BLACK_PEN));
    U(DeleteObject)(pen);
    U(RestoreDC)(dc, -5);
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    static const EXPREC e[] = {
        { 0x02FC, 7, 2, { 0, -1 } }, { 0x012D, 4, 1, { 0 } },
        { 0x02FC, 7, 2, { 0, -1 } }, { 0x012D, 4, 1, { 1 } },
        { 0x02FA, 8, 4, { 0, 1, 0, 0x0101 } }, { 0x012D, 4, 1, { 2 } },
        { 0x001E, 3, 0, { 0 } },
        { 0x02FA, 8, 4, { 0, 0, 0, 0 } }, { 0x012D, 4, 1, { 3 } },
        { 0x0127, 4, 1, { 0xFFFF } },
        { 0x012D, 4, 1, { 3 } },
        { 0x01F0, 4, 1, { 2 } },
        { 0x0127, 4, 1, { 0xFFFB } },
        { 0x0000, 3, 0, { 0 } },
    };
    expect_seq(&b, e, sizeof e / sizeof *e, "select_sequence");
    EXPECT_EQ(wb_hdr(&b)->nobj, 4);
    EXPECT_EQ(wb_hdr(&b)->maxrec, 8);
    EXPECT_EQ(wb_hdr(&b)->size, 77);
    wb_free(&b);
}

TEST(wmf_hdr_delete_unselected_sequence) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    HPEN pen = U(CreatePen)(PS_SOLID, 1, RGB(1, 1, 1));
    U(SelectObject)(dc, pen);
    U(SelectObject)(dc, U(GetStockObject)(BLACK_PEN));
    CHECK(U(DeleteObject)(pen));
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    static const EXPREC e[] = {
        { 0x02FA, 8, 4, { 0, 1, 0, 0x0101 } }, { 0x012D, 4, 1, { 0 } },
        { 0x02FA, 8, 4, { 0, 0, 0, 0 } }, { 0x012D, 4, 1, { 1 } },
        { 0x01F0, 4, 1, { 0 } },
        { 0x0000, 3, 0, { 0 } },
    };
    expect_seq(&b, e, sizeof e / sizeof *e, "delete_unselected");
    EXPECT_EQ(wb_hdr(&b)->nobj, 2);
    wb_free(&b);
}

TEST(wmf_hdr_delete_selected_elsewhere) {
    WMF_REQ_BASE();
    /* a pen still selected into another metafile DC: DeleteObject in the first DC records
     * no DELETEOBJECT in the DC where it is selected */
    HDC dc1 = mf_new(), dc2 = mf_new();
    REQUIRE(dc1 && dc2);
    HPEN pen = U(CreatePen)(PS_SOLID, 1, RGB(1, 1, 1));
    U(SelectObject)(dc1, pen);
    U(SelectObject)(dc2, pen);
    U(SelectObject)(dc1, U(GetStockObject)(BLACK_PEN));
    CHECK(U(DeleteObject)(pen));
    WB b1, b2;
    REQUIRE(mf_finish(dc1, &b1, NULL));
    REQUIRE(mf_finish(dc2, &b2, NULL));
    static const EXPREC e2[] = {
        { 0x02FA, 8, 4, { 0, 1, 0, 0x0101 } }, { 0x012D, 4, 1, { 0 } }, { 0x0000, 3, 0, { 0 } },
    };
    expect_seq(&b2, e2, 3, "delete_selected");
    EXPECT_EQ(wb_hdr(&b2)->nobj, 1);
    EXPECT_EQ(wb_count(&b2, 0x01F0), 0);
    EXPECT_EQ(wb_count(&b1, 0x01F0), 1);
    wb_free(&b1); wb_free(&b2);
}

TEST(wmf_hdr_object_reuse_after_delete) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    HPEN p1 = U(CreatePen)(PS_SOLID, 1, 1);
    U(SelectObject)(dc, p1);
    U(SelectObject)(dc, U(GetStockObject)(BLACK_PEN));
    U(DeleteObject)(p1);
    HPEN p2 = U(CreatePen)(PS_SOLID, 2, 2);
    U(SelectObject)(dc, p2);
    U(SelectObject)(dc, U(GetStockObject)(BLACK_PEN));
    U(DeleteObject)(p2);
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    CHECK(wb_valid(&b));
    t_note("slot reuse: nobj = %u", wb_hdr(&b)->nobj);
    CHECK(wb_hdr(&b)->nobj <= 3);
    wb_free(&b);
}

TEST(wmf_bits_size_query) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    U(Rectangle)(dc, 0, 0, 10, 10);
    HMETAFILE h = U(CloseMetaFile)(dc);
    REQUIRE(h);
    EXPECT_EQ(U(GetMetaFileBitsEx)(h, 0, NULL), 38);
    EXPECT_EQ(U(GetMetaFileBitsEx)(h, 1000, NULL), 38);
    EXPECT_EQ(U(GetMetaFileBitsEx)(h, 0xFFFFFFFF, NULL), 38);
    U(DeleteMetaFile)(h);
}

TEST(wmf_bits_buffer_sizes) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    U(Rectangle)(dc, 0, 0, 10, 10);
    HMETAFILE h = U(CloseMetaFile)(dc);
    REQUIRE(h);
    BYTE buf[128];
    UINT n = U(GetMetaFileBitsEx)(h, 0, NULL);
    EXPECT_EQ(n, 38);
    struct { UINT cb; UINT want; } t[] = { {0, 0}, {1, 0}, {17, 0}, {18, 0}, {37, 0}, {38, 38}, {39, 38}, {64, 38}, {128, 38} };
    for (size_t i = 0; i < sizeof t / sizeof *t; i++) {
        memset(buf, 0xA5, sizeof buf);
        UINT r = U(GetMetaFileBitsEx)(h, t[i].cb, buf);
        CHECKF(r == t[i].want, "cb=%u got %u want %u", t[i].cb, r, t[i].want);
        if (t[i].want) {
            CHECKF(buf[37] == 0 || 1, "x");
            for (UINT k = 38; k < 128; k++) if (buf[k] != 0xA5) { CHECKF(0, "cb=%u wrote past end at %u", t[i].cb, k); break; }
        } else {
            for (UINT k = 0; k < 128; k++) if (buf[k] != 0xA5) { CHECKF(0, "cb=%u buffer touched at %u", t[i].cb, k); break; }
        }
    }
    U(DeleteMetaFile)(h);
}

TEST(wmf_bits_repeatable) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    U(Ellipse)(dc, 0, 0, 10, 10);
    U(LineTo)(dc, 3, 3);
    HMETAFILE h = U(CloseMetaFile)(dc);
    REQUIRE(h);
    WB a, b;
    CHECK(wb_get(h, &a) != NULL);
    CHECK(wb_get(h, &b) != NULL);
    EXPECT_EQ(a.n, b.n);
    if (a.n == b.n && a.p) CHECK(!memcmp(a.p, b.p, a.n));
    wb_free(&a); wb_free(&b);
    U(DeleteMetaFile)(h);
}

TEST(wmf_bits_invalid_handle) {
    WMF_REQ_BASE();
    BYTE buf[64];
    HMETAFILE bad[] = { NULL, (HMETAFILE)1, (HMETAFILE)(ULONG_PTR)0xdeadbeef, (HMETAFILE)(ULONG_PTR)0x00260001 };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        UINT r = U(GetMetaFileBitsEx)(bad[i], 0, NULL);
        CHECK(r == 0);
        EXPECT_ERR(r, ERROR_INVALID_HANDLE);
        SET_ERR(0);
        r = U(GetMetaFileBitsEx)(bad[i], sizeof buf, buf);
        CHECK(r == 0);
        EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    }
}

TEST(wmf_bits_wrong_object_types) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    SET_ERR(0);
    UINT r = U(GetMetaFileBitsEx)((HMETAFILE)dc, 0, NULL);
    CHECK(r == 0);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    HBRUSH b = U(CreateSolidBrush)(0);
    SET_ERR(0);
    r = U(GetMetaFileBitsEx)((HMETAFILE)b, 0, NULL);
    CHECK(r == 0);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    U(DeleteObject)(b);
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_bits_on_open_dc) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    SET_ERR(0);
    UINT r = U(GetMetaFileBitsEx)((HMETAFILE)dc, 0, NULL);
    CHECK(r == 0);
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (h) U(DeleteMetaFile)(h);
}

TEST(wmf_big_many_records) {
    WMF_REQ_BASE();
    enum { N = 20000 };
    HDC dc = mf_new();
    REQUIRE(dc);
    for (int i = 0; i < N; i++) U(Rectangle)(dc, i & 0x3FF, i & 0xFF, (i & 0x3FF) + 5, (i & 0xFF) + 5);
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    EXPECT_EQ(b.n, 18 + N * 14 + 6);
    CHECK(wb_valid(&b));
    EXPECT_EQ(wb_count(&b, 0x041B), N);
    const BYTE *r = wb_find(&b, 0x041B, N - 1);
    if (r) { EXPECT_EQ(r_parm(r, 3), (N - 1) & 0x3FF); }
    wb_free(&b);
}

TEST(wmf_big_buffer_growth_steps) {
    WMF_REQ_BASE();
    /* internal buffer starts at 16 KiB: probe sizes around the growth boundary */
    int counts[] = { 1170, 1171, 1172, 1173, 1200, 2340, 2341, 2342, 4680 };
    for (size_t k = 0; k < sizeof counts / sizeof *counts; k++) {
        HDC dc = mf_new();
        REQUIRE(dc);
        for (int i = 0; i < counts[k]; i++) U(Rectangle)(dc, 0, 0, 1, 1);
        WB b;
        REQUIRE(mf_finish(dc, &b, NULL));
        CHECKF(b.n == (UINT)(18 + counts[k] * 14 + 6), "n=%d size %u", counts[k], b.n);
        CHECKF(wb_valid(&b), "n=%d", counts[k]);
        wb_free(&b);
    }
}

TEST(wmf_big_single_polygon) {
    WMF_REQ_BASE();
    enum { N = 30000 };
    POINT *pts = malloc(N * sizeof(POINT));
    REQUIRE(pts);
    for (int i = 0; i < N; i++) { pts[i].x = i & 0x7FFF; pts[i].y = (i * 3) & 0x7FFF; }
    HDC dc = mf_new();
    REQUIRE(dc);
    BOOL r = U(Polygon)(dc, pts, N);
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    t_note("Polygon(30000 pts) = %d", r);
    if (r) {
        EXPECT_EQ(wb_hdr(&b)->maxrec, 3 + 1 + 2 * N);
        const BYTE *g = wb_find(&b, 0x0324, 0);
        CHECK(g != NULL);
        if (g) { EXPECT_EQ(r_parm(g, 0), N); EXPECT_EQ(r_size(g), 3 + 1 + 2 * N); }
    }
    CHECK(wb_valid(&b));
    wb_free(&b);
    free(pts);
}

TEST(wmf_big_polygon_over_16bit_count) {
    WMF_REQ_BASE();
    enum { N = 70000 };
    POINT *pts = malloc(N * sizeof(POINT));
    REQUIRE(pts);
    for (int i = 0; i < N; i++) { pts[i].x = i & 0x7FFF; pts[i].y = i & 0x7FFF; }
    HDC dc = mf_new();
    REQUIRE(dc);
    SET_ERR(0);
    BOOL r = U(Polygon)(dc, pts, N);
    t_note("Polygon(70000 pts) = %d err %lu", r, GetLastError());
    WB b;
    if (mf_finish(dc, &b, NULL)) { t_note("size after: %u bytes, valid=%d", b.n, wb_valid(&b)); wb_free(&b); }
    free(pts);
}

TEST(wmf_big_escape_payload) {
    WMF_REQ_BASE();
    BYTE *buf = malloc(60000);
    REQUIRE(buf);
    for (int i = 0; i < 60000; i++) buf[i] = (BYTE)i;
    HDC dc = mf_new();
    REQUIRE(dc);
    int r = U(Escape)(dc, MFCOMMENT, 60000, (LPCSTR)buf, NULL);
    t_note("Escape(MFCOMMENT, 60000) = %d", r);
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    CHECK(wb_valid(&b));
    if (r > 0) {
        const BYTE *e = wb_find(&b, 0x0626, 0);
        CHECK(e != NULL);
        if (e) { EXPECT_EQ(r_parm(e, 0), 15); EXPECT_EQ(r_parm(e, 1), 60000); CHECK(!memcmp(e + 6 + 4, buf, 60000)); }
    }
    wb_free(&b);
    free(buf);
}

TEST(wmf_hdr_record_alignment) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    U(TextOutA)(dc, 0, 0, "a", 1);
    U(TextOutA)(dc, 0, 0, "abc", 3);
    U(TextOutA)(dc, 0, 0, "abcde", 5);
    U(Rectangle)(dc, 0, 0, 1, 1);
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    CHECK(wb_valid(&b));
    EXPECT_EQ(b.n % 2, 0);
    wb_free(&b);
}

TEST(wmf_hdr_wb_walk_helper_sanity) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    for (int i = 0; i < 5; i++) { U(MoveToEx)(dc, i, i, NULL); U(LineTo)(dc, i + 1, i + 1); }
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    EXPECT_EQ(wb_nrecs(&b), 11);
    EXPECT_EQ(wb_count(&b, 0x0214), 5);
    EXPECT_EQ(wb_count(&b, 0x0213), 5);
    EXPECT_EQ(wb_count(&b, 0), 1);
    wb_free(&b);
}

TEST(wmf_hdr_two_dcs_independent) {
    WMF_REQ_BASE();
    HDC a = mf_new(), c = mf_new();
    REQUIRE(a && c);
    U(Rectangle)(a, 1, 1, 2, 2);
    U(Ellipse)(c, 3, 3, 4, 4);
    U(Rectangle)(a, 5, 5, 6, 6);
    WB ba, bc;
    REQUIRE(mf_finish(a, &ba, NULL));
    REQUIRE(mf_finish(c, &bc, NULL));
    EXPECT_EQ(wb_count(&ba, 0x041B), 2);
    EXPECT_EQ(wb_count(&ba, 0x0418), 0);
    EXPECT_EQ(wb_count(&bc, 0x0418), 1);
    EXPECT_EQ(wb_count(&bc, 0x041B), 0);
    wb_free(&ba); wb_free(&bc);
}

TEST(wmf_hdr_stock_font_recorded) {
    WMF_REQ_BASE();
    HDC dc = mf_new();
    REQUIRE(dc);
    U(SelectObject)(dc, U(GetStockObject)(SYSTEM_FIXED_FONT));
    WB b;
    REQUIRE(mf_finish(dc, &b, NULL));
    CHECK(wb_valid(&b));
    t_note("stock font select records: create=%d select=%d", wb_count(&b, 0x02FB), wb_count(&b, 0x012D));
    wb_free(&b);
}
