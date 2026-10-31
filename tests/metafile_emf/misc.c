#include "emf.h"

/* ---------- GdiComment ---------- */

TEST(emf_comment_on_non_emf_dcs_succeeds) {
    REQUIRE(U(GdiComment));
    static const BYTE d[4] = "abc";
    HDC m = U(CreateCompatibleDC)(NULL);
    CHECK(U(GdiComment)(m, 4, d));
    HDC scr = U(GetDC)(NULL);
    CHECK(U(GdiComment)(scr, 4, d));
    U(ReleaseDC)(NULL, scr);
    U(DeleteDC)(m);
}

TEST(emf_comment_on_invalid_dc) {
    REQUIRE(U(GdiComment));
    static const BYTE d[4] = "abc";
    BOOL r = U(GdiComment)(NULL, 4, d);
    t_note("GdiComment(NULL) = %d", r);
    r = U(GdiComment)((HDC)0x1234, 4, d);
    t_note("GdiComment(bogus) = %d", r);
}

TEST(emf_comment_sizes_roundtrip) {
    REQUIRE(U(GdiComment));
    BYTE data[300]; for (int i = 0; i < 300; i++) data[i] = (BYTE)(i * 3 + 1);
    static const UINT sizes[] = { 1, 2, 3, 4, 5, 7, 8, 9, 63, 64, 65, 255, 256, 299 };
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) CHECKF(U(GdiComment)(dc, sizes[i], data), "size %u", sizes[i]);
    HENHMETAFILE h = emf_end(dc);
    UINT n; BYTE *b = emf_bits(h, &n);
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        const ENHMETARECORD *r = b ? emf_find(b, n, EMR_GDICOMMENT, i) : NULL;
        CHECKF(r != NULL, "comment %u", i);
        if (!r) continue;
        const EMRGDICOMMENT *e = (const EMRGDICOMMENT *)r;
        EXPECT_EQ(e->cbData, sizes[i]);
        EXPECT_EQ(r->nSize, 12 + ((sizes[i] + 3) & ~3u));
        CHECK(!memcmp(e->Data, data, sizes[i]));
    }
    free(b); U(DeleteEnhMetaFile)(h);
}

TEST(emf_comment_large_64k) {
    REQUIRE(U(GdiComment));
    UINT sz = 65536;
    BYTE *data = malloc(sz); for (UINT i = 0; i < sz; i++) data[i] = (BYTE)(i ^ (i >> 8));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); free(data); return; }
    BOOL ok = U(GdiComment)(dc, sz, data);
    CHECK(ok);
    HENHMETAFILE h = emf_end(dc);
    UINT n; BYTE *b = emf_bits(h, &n);
    const ENHMETARECORD *r = b ? emf_find(b, n, EMR_GDICOMMENT, 0) : NULL;
    if (ok) {
        CHECK(r != NULL);
        if (r) { CHECK(((const EMRGDICOMMENT *)r)->cbData == sz); CHECK(!memcmp(((const EMRGDICOMMENT *)r)->Data, data, sz)); }
    }
    free(b); free(data); U(DeleteEnhMetaFile)(h);
}

TEST(emf_comment_large_4mb) {
    REQUIRE(U(GdiComment));
    UINT sz = 4 * 1024 * 1024;
    BYTE *data = calloc(sz, 1);
    HDC dc = emf_begin();
    if (!dc || !data) { CHECK(dc != NULL); free(data); return; }
    BOOL ok = U(GdiComment)(dc, sz, data);
    t_note("4MB comment accepted=%d", ok);
    HENHMETAFILE h = emf_end(dc);
    CHECK(h != NULL);
    if (h) {
        UINT n = U(GetEnhMetaFileBits)(h, 0, NULL);
        if (ok) CHECK(n >= sz);
        U(DeleteEnhMetaFile)(h);
    }
    free(data);
}

TEST(emf_comment_zero_size) {
    REQUIRE(U(GdiComment));
    static const BYTE d[4] = "abc";
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    BOOL r = U(GdiComment)(dc, 0, d);
    t_note("GdiComment(size 0, buf) = %d", r);
    HENHMETAFILE h = emf_end(dc);
    if (h) {
        UINT n; BYTE *b = emf_bits(h, &n);
        const ENHMETARECORD *rec = b ? emf_find(b, n, EMR_GDICOMMENT, 0) : NULL;
        t_note("zero-size comment record present=%d", rec != NULL);
        free(b); U(DeleteEnhMetaFile)(h);
    }
}

TEST(emf_comment_after_close_fails) {
    REQUIRE(U(GdiComment));
    static const BYTE d[4] = "abc";
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    HENHMETAFILE h = emf_end(dc);
    BOOL r = U(GdiComment)(dc, 4, d);
    t_note("GdiComment on closed emf dc = %d", r);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_comment_does_not_change_bounds) {
    REQUIRE(U(GdiComment));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    U(PatBlt)(dc, 10, 10, 10, 10, BLACKNESS);
    U(GdiComment)(dc, 4, (const BYTE *)"GDIC");
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; U(GetEnhMetaFileHeader)(h, sizeof hd, &hd);
    EXPECT_EQ(hd.rclBounds.left, 10); EXPECT_EQ(hd.rclBounds.right, 19);
    U(DeleteEnhMetaFile)(h);
}

static int CALLBACK comment_cb(HDC dc, HANDLETABLE *ht, const ENHMETARECORD *r, int n, LPARAM lp) {
    if (r->iType == EMR_GDICOMMENT) (*(int *)lp)++;
    return 1;
}
TEST(emf_comment_enum_sees_comments) {
    REQUIRE(U(GdiComment)); REQUIRE(U(EnumEnhMetaFile));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    for (int i = 0; i < 7; i++) U(GdiComment)(dc, 8, (const BYTE *)"EMF+test");
    HENHMETAFILE h = emf_end(dc);
    HDC m = U(CreateCompatibleDC)(NULL); RECT rc = {0, 0, 10, 10}; int cnt = 0;
    U(EnumEnhMetaFile)(m, h, comment_cb, &cnt, &rc);
    EXPECT_EQ(cnt, 7);
    U(DeleteDC)(m); U(DeleteEnhMetaFile)(h);
}

/* ---------- operations not valid on a metafile DC ---------- */

TEST(emf_forbidden_getpixel) {
    REQUIRE(U(GetPixel));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    COLORREF c = U(GetPixel)(dc, 0, 0);
    SOFT(c == CLR_INVALID);
    t_note("GetPixel on EMF DC = 0x%08lx", c);
    U(DeleteEnhMetaFile)(emf_end(dc));
}

TEST(emf_forbidden_select_bitmap) {
    REQUIRE(U(CreateBitmap));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, NULL);
    HGDIOBJ r = U(SelectObject)(dc, bm);
    CHECK(r == NULL);
    HBITMAP bm2 = U(CreateCompatibleBitmap)(dc, 4, 4);
    t_note("CreateCompatibleBitmap(emf dc) = %p", bm2);
    if (bm2) {
        HGDIOBJ r2 = U(SelectObject)(dc, bm2);
        CHECK(r2 == NULL);
        U(DeleteObject)(bm2);
    }
    U(DeleteObject)(bm);
    U(DeleteEnhMetaFile)(emf_end(dc));
}

TEST(emf_forbidden_blt_from_emf_dc) {
    REQUIRE(U(BitBlt));
    HDC dc = emf_begin();
    emf_dib d;
    if (!dc || !emf_dib_make(&d, 16, 16, WHITE)) { t_skip("setup"); return; }
    BOOL r = U(BitBlt)(d.dc, 0, 0, 8, 8, dc, 0, 0, SRCCOPY);
    t_note("BitBlt with EMF DC as source = %d", r);
    SOFT(r == FALSE);
    emf_dib_free(&d);
    U(DeleteEnhMetaFile)(emf_end(dc));
}

TEST(emf_forbidden_bitblt_srccopy_from_memory_is_recorded) {
    REQUIRE(U(BitBlt));
    HDC dc = emf_begin();
    emf_dib s;
    if (!dc || !emf_dib_make(&s, 16, 16, 0x00FF00)) { t_skip("setup"); return; }
    CHECK(U(BitBlt)(dc, 0, 0, 16, 16, s.dc, 0, 0, SRCCOPY));
    HENHMETAFILE h = emf_end(dc);
    UINT n; BYTE *b = emf_bits(h, &n);
    const ENHMETARECORD *r = b ? emf_find(b, n, EMR_BITBLT, 0) : NULL;
    CHECK(r != NULL);
    if (r) {
        const EMRBITBLT *e = (const EMRBITBLT *)r;
        CHECK(e->cbBmiSrc > 0);
        CHECK(e->cbBitsSrc > 0);
        EXPECT_EQ(e->dwRop, SRCCOPY);
        EXPECT_EQ(e->cxDest, 16);
    }
    free(b); emf_dib_free(&s); U(DeleteEnhMetaFile)(h);
}

TEST(emf_text_metrics_on_emf_dc) {
    REQUIRE(U(GetTextExtentPoint32W)); REQUIRE(U(GetTextMetricsW));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    SIZE sz = {0, 0};
    BOOL r = U(GetTextExtentPoint32W)(dc, L"Hello", 5, &sz);
    CHECK(r);
    CHECK(sz.cx > 0 && sz.cy > 0);
    TEXTMETRICW tm; memset(&tm, 0, sizeof tm);
    CHECK(U(GetTextMetricsW)(dc, &tm));
    CHECK(tm.tmHeight > 0);
    U(DeleteEnhMetaFile)(emf_end(dc));
}

TEST(emf_getclipbox_on_emf_dc) {
    REQUIRE(U(GetClipBox));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    RECT rc;
    int r = U(GetClipBox)(dc, &rc);
    t_note("GetClipBox on EMF DC = %d [%ld %ld %ld %ld]", r, rc.left, rc.top, rc.right, rc.bottom);
    U(DeleteEnhMetaFile)(emf_end(dc));
}

TEST(emf_selectcliprgn_on_emf_dc) {
    REQUIRE(U(SelectClipRgn));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    HRGN r = U(CreateRectRgn)(0, 0, 10, 10);
    int rv = U(SelectClipRgn)(dc, r);
    CHECK(rv != ERROR);
    t_note("SelectClipRgn(emf) = %d", rv);
    U(DeleteObject)(r);
    U(DeleteEnhMetaFile)(emf_end(dc));
}

TEST(emf_setmetargn_effect_on_bounds) {
    REQUIRE(U(SetMetaRgn));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    U(IntersectClipRect)(dc, 0, 0, 50, 50);
    U(SetMetaRgn)(dc);
    U(PatBlt)(dc, 0, 0, 200, 200, BLACKNESS);
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; U(GetEnhMetaFileHeader)(h, sizeof hd, &hd);
    t_note("bounds after metargn [%ld %ld %ld %ld]", hd.rclBounds.left, hd.rclBounds.top, hd.rclBounds.right, hd.rclBounds.bottom);
    CHECK(hd.rclBounds.right >= 49);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_clip_limits_bounds) {
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    U(IntersectClipRect)(dc, 0, 0, 50, 50);
    U(PatBlt)(dc, 0, 0, 200, 200, BLACKNESS);
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; U(GetEnhMetaFileHeader)(h, sizeof hd, &hd);
    t_note("bounds with clip [%ld %ld %ld %ld]", hd.rclBounds.left, hd.rclBounds.top, hd.rclBounds.right, hd.rclBounds.bottom);
    SOFT(hd.rclBounds.right <= 50 && hd.rclBounds.bottom <= 50);
    U(DeleteEnhMetaFile)(h);
}

/* ---------- limits ---------- */

TEST(emf_big_ten_megabytes) {
    REQUIRE(U(Polyline));
    enum { NPTS = 5000, NREC = 500 };
    POINT *pts = malloc(NPTS * sizeof(POINT));
    for (int i = 0; i < NPTS; i++) { pts[i].x = i % 3000; pts[i].y = (i * 7) % 2000; }
    HDC dc = emf_begin();
    if (!dc || !pts) { CHECK(dc != NULL); free(pts); return; }
    for (int i = 0; i < NREC; i++) U(Polyline)(dc, pts, NPTS);
    HENHMETAFILE h = emf_end(dc);
    CHECK(h != NULL);
    if (h) {
        ENHMETAHEADER hd; U(GetEnhMetaFileHeader)(h, sizeof hd, &hd);
        CHECK(hd.nBytes > 9u * 1024 * 1024);
        EXPECT_EQ(hd.nRecords, NREC + 2);
        UINT n; BYTE *b = emf_bits(h, &n);
        CHECK(b != NULL && n == hd.nBytes);
        if (b) {
            EXPECT_EQ(emf_count_recs(b, n), NREC + 2);
            HENHMETAFILE h2 = U(SetEnhMetaFileBits)(n, b);
            CHECK(h2 != NULL);
            if (h2) U(DeleteEnhMetaFile)(h2);
            free(b);
        }
        U(DeleteEnhMetaFile)(h);
    }
    free(pts);
}

TEST(emf_big_many_small_records) {
    REQUIRE(U(SetPixelV));
    enum { N = 20000 };
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    for (int i = 0; i < N; i++) U(SetPixelV)(dc, i % 200, i / 200, RGB(i & 255, 0, 0));
    HENHMETAFILE h = emf_end(dc);
    CHECK(h != NULL);
    if (h) {
        ENHMETAHEADER hd; U(GetEnhMetaFileHeader)(h, sizeof hd, &hd);
        CHECK(hd.nRecords >= N);
        t_note("%d SetPixelV -> %lu records, %lu bytes", N, hd.nRecords, hd.nBytes);
        emf_log l; emf_enum(h, &l);
        EXPECT_EQ(l.n, hd.nRecords);
        U(DeleteEnhMetaFile)(h);
    }
}

TEST(emf_handle_table_slot_reuse) {
    REQUIRE(U(CreatePen));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    DWORD g0 = emf_gdi_count();
    for (int i = 0; i < 2000; i++) {
        HPEN p = U(CreatePen)(PS_SOLID, 1, i);
        HGDIOBJ o = U(SelectObject)(dc, p);
        U(MoveToEx)(dc, 0, 0, NULL); U(LineTo)(dc, i % 100, 5);
        U(SelectObject)(dc, o);
        U(DeleteObject)(p);
    }
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; U(GetEnhMetaFileHeader)(h, sizeof hd, &hd);
    t_note("2000 transient pens -> nHandles=%u", (unsigned)hd.nHandles);
    CHECK(hd.nHandles >= 2);
    U(DeleteEnhMetaFile)(h);
    EXPECT_EQ(emf_gdi_count(), g0);
}

TEST(emf_handle_table_many_live_objects) {
    REQUIRE(U(CreateSolidBrush));
    enum { N = 300 };
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    HBRUSH br[N]; HGDIOBJ o = NULL;
    for (int i = 0; i < N; i++) {
        br[i] = U(CreateSolidBrush)(RGB(i & 255, i >> 3, 0));
        HGDIOBJ prev = U(SelectObject)(dc, br[i]);
        if (!o) o = prev;
        U(PatBlt)(dc, i % 50, 0, 2, 2, PATCOPY);
    }
    U(SelectObject)(dc, o);
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; U(GetEnhMetaFileHeader)(h, sizeof hd, &hd);
    CHECK(hd.nHandles >= N + 1);
    t_note("%d live brushes -> nHandles=%u", N, (unsigned)hd.nHandles);
    emf_log l; emf_enum(h, &l);
    EXPECT_EQ(l.nobj, hd.nHandles);
    for (int i = 0; i < N; i++) U(DeleteObject)(br[i]);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_gdi_object_limit_with_dcs) {
    REQUIRE(U(CreateEnhMetaFileW));
    DWORD g0 = emf_gdi_count();
    enum { N = 400 };
    HDC dcs[N]; int made = 0;
    for (int i = 0; i < N; i++) { dcs[i] = emf_begin(); if (!dcs[i]) break; made++; }
    DWORD g1 = emf_gdi_count();
    t_note("%d EMF DCs open: gdi count %lu -> %lu", made, g0, g1);
    CHECK(made == N);
    for (int i = 0; i < made; i++) U(DeleteEnhMetaFile)(emf_end(dcs[i]));
    EXPECT_EQ(emf_gdi_count(), g0);
}

/* ---------- threads ---------- */

struct thr { HENHMETAFILE h; int ok; HENHMETAFILE out; emf_dib *d; };

static DWORD WINAPI thr_use(LPVOID p) {
    struct thr *t = p;
    ENHMETAHEADER hd;
    t->ok = 0;
    if (U(GetEnhMetaFileHeader)(t->h, sizeof hd, &hd) != 108) return 1;
    if (hd.dSignature != ENHMETA_SIGNATURE) return 2;
    UINT n = U(GetEnhMetaFileBits)(t->h, 0, NULL);
    if (n != hd.nBytes) return 3;
    emf_log l; emf_enum(t->h, &l);
    if (l.n != (int)hd.nRecords) return 4;
    RECT rc = {0, 0, 100, 100};
    if (!U(PlayEnhMetaFile)(t->d->dc, t->h, &rc)) return 5;
    HENHMETAFILE c = U(CopyEnhMetaFileW)(t->h, NULL);
    if (!c) return 6;
    U(DeleteEnhMetaFile)(c);
    t->ok = 1;
    return 0;
}
TEST(emf_thread_use_handle_from_other_thread) {
    REQUIRE(U(PlayEnhMetaFile)); REQUIRE(U(CopyEnhMetaFileW));
    HDC dc = emf_begin();
    emf_dib d;
    if (!dc || !emf_dib_make(&d, 100, 100, WHITE)) { t_skip("setup"); return; }
    U(PatBlt)(dc, 10, 10, 50, 50, BLACKNESS);
    HENHMETAFILE h = emf_end(dc);
    struct thr t = { h, 0, NULL, &d };
    HANDLE th = CreateThread(NULL, 0, thr_use, &t, 0, NULL);
    WaitForSingleObject(th, 30000);
    DWORD code = 99; GetExitCodeThread(th, &code); CloseHandle(th);
    EXPECT_EQ(code, 0);
    CHECK(t.ok);
    EXPECT_EQ(emf_px(&d, 30, 30), 0);
    emf_dib_free(&d); U(DeleteEnhMetaFile)(h);
}

static DWORD WINAPI thr_make(LPVOID p) {
    struct thr *t = p;
    HDC dc = emf_begin();
    if (!dc) return 1;
    U(PatBlt)(dc, 1, 2, 30, 40, BLACKNESS);
    t->out = emf_end(dc);
    return t->out ? 0 : 2;
}
TEST(emf_thread_handle_outlives_creating_thread) {
    REQUIRE(U(CreateEnhMetaFileW));
    struct thr t = {0};
    HANDLE th = CreateThread(NULL, 0, thr_make, &t, 0, NULL);
    WaitForSingleObject(th, 30000);
    DWORD code = 99; GetExitCodeThread(th, &code); CloseHandle(th);
    EXPECT_EQ(code, 0);
    if (t.out) {
        ENHMETAHEADER hd;
        CHECK(U(GetEnhMetaFileHeader)(t.out, sizeof hd, &hd) == 108);
        EXPECT_EQ(hd.rclBounds.right, 30);
        CHECK(U(DeleteEnhMetaFile)(t.out));
    }
}

static DWORD WINAPI thr_close(LPVOID p) {
    struct thr *t = p;
    HDC dc = (HDC)t->h;
    t->out = emf_end(dc);
    return 0;
}
TEST(emf_thread_close_dc_in_other_thread) {
    REQUIRE(U(CreateEnhMetaFileW));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    U(PatBlt)(dc, 0, 0, 5, 5, BLACKNESS);
    struct thr t = { (HENHMETAFILE)dc, 0, NULL, NULL };
    HANDLE th = CreateThread(NULL, 0, thr_close, &t, 0, NULL);
    WaitForSingleObject(th, 30000); CloseHandle(th);
    t_note("closing an EMF DC from another thread -> %p", t.out);
    if (t.out) U(DeleteEnhMetaFile)(t.out);
    else { HENHMETAFILE h = emf_end(dc); if (h) U(DeleteEnhMetaFile)(h); }
}

struct hammer { HENHMETAFILE h; volatile LONG fails; };
static DWORD WINAPI thr_hammer(LPVOID p) {
    struct hammer *hm = p;
    for (int i = 0; i < 300; i++) {
        ENHMETAHEADER hd;
        if (U(GetEnhMetaFileHeader)(hm->h, sizeof hd, &hd) != 108) InterlockedIncrement(&hm->fails);
        UINT n = U(GetEnhMetaFileBits)(hm->h, 0, NULL);
        if (n != hd.nBytes) InterlockedIncrement(&hm->fails);
        emf_log l; emf_enum(hm->h, &l);
        if (l.n < 3) InterlockedIncrement(&hm->fails);
    }
    return 0;
}
TEST(emf_thread_concurrent_readers) {
    REQUIRE(U(EnumEnhMetaFile));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    U(PatBlt)(dc, 0, 0, 5, 5, BLACKNESS);
    HENHMETAFILE h = emf_end(dc);
    struct hammer hm = { h, 0 };
    HANDLE th[4];
    for (int i = 0; i < 4; i++) th[i] = CreateThread(NULL, 0, thr_hammer, &hm, 0, NULL);
    WaitForMultipleObjects(4, th, TRUE, 60000);
    for (int i = 0; i < 4; i++) CloseHandle(th[i]);
    EXPECT_EQ(hm.fails, 0);
    U(DeleteEnhMetaFile)(h);
}

/* ---------- multiple EMF DCs recording at once ---------- */

TEST(emf_two_dcs_independent) {
    REQUIRE(U(CreateEnhMetaFileW));
    HDC a = emf_begin(), b = emf_begin();
    if (!a || !b) { CHECK(a && b); return; }
    U(PatBlt)(a, 0, 0, 10, 10, BLACKNESS);
    U(PatBlt)(b, 100, 100, 20, 20, BLACKNESS);
    U(SetBkColor)(a, 0x111111);
    EXPECT_EQ(U(GetBkColor)(b), 0xFFFFFF);
    HENHMETAFILE hb = emf_end(b), ha = emf_end(a);
    ENHMETAHEADER A, B; U(GetEnhMetaFileHeader)(ha, sizeof A, &A); U(GetEnhMetaFileHeader)(hb, sizeof B, &B);
    EXPECT_EQ(A.rclBounds.right, 9);
    EXPECT_EQ(B.rclBounds.left, 100);
    EXPECT_EQ(B.rclBounds.right, 119);
    U(DeleteEnhMetaFile)(ha); U(DeleteEnhMetaFile)(hb);
}

TEST(emf_shared_pen_in_two_dcs_has_independent_indexes) {
    REQUIRE(U(CreatePen));
    HDC a = emf_begin(), b = emf_begin();
    if (!a || !b) { CHECK(a && b); return; }
    HPEN p1 = U(CreatePen)(PS_SOLID, 1, 1), p2 = U(CreatePen)(PS_SOLID, 1, 2);
    HGDIOBJ oa = U(SelectObject)(a, p1);
    U(SelectObject)(b, p2); U(SelectObject)(b, p1);   /* in b: p2 -> 1, p1 -> 2 */
    HENHMETAFILE hb = emf_end(b), ha = emf_end(a);
    UINT n; BYTE *bb = emf_bits(hb, &n);
    const ENHMETARECORD *s0 = bb ? emf_find(bb, n, EMR_SELECTOBJECT, 0) : NULL, *s1 = bb ? emf_find(bb, n, EMR_SELECTOBJECT, 1) : NULL;
    CHECK(s0 && s1);
    if (s0 && s1) { EXPECT_EQ(s0->dParm[0], 1); EXPECT_EQ(s1->dParm[0], 2); }
    free(bb);
    U(DeleteObject)(p1); U(DeleteObject)(p2);
    U(DeleteEnhMetaFile)(ha); U(DeleteEnhMetaFile)(hb);
}

TEST(emf_object_deleted_while_selected_in_emf_dc) {
    REQUIRE(U(CreatePen));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    HPEN p = U(CreatePen)(PS_SOLID, 1, 5);
    HGDIOBJ o = U(SelectObject)(dc, p);
    BOOL del = U(DeleteObject)(p);
    t_note("DeleteObject on pen selected into EMF DC = %d", del);
    if (!del) { U(SelectObject)(dc, o); U(DeleteObject)(p); }
    else U(SelectObject)(dc, o);
    HENHMETAFILE h = emf_end(dc);
    CHECK(h != NULL);
    U(DeleteEnhMetaFile)(h);
}
