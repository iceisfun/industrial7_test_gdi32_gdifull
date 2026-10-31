#include "cih.h"

#define CI_EMR_HEADER 1
#define CI_EMR_EOF 14
#define CI_EMR_SETWORLDTRANSFORM 35
#define CI_EMR_MODIFYWORLDTRANSFORM 36
#define CI_EMR_RECTANGLE 43
#define CI_EMR_GDICOMMENT 70
#define CI_EMR_SETICMMODE 98
#define CI_EMR_CREATECOLORSPACE 99
#define CI_EMR_SETCOLORSPACE 100
#define CI_EMR_DELETECOLORSPACE 101
#define CI_EMR_COLORCORRECTPALETTE 111
#define CI_EMR_SETICMPROFILEA 112
#define CI_EMR_SETICMPROFILEW 113
#define CI_EMR_CREATECOLORSPACEW 122

typedef struct { BYTE *b; UINT n; HENHMETAFILE h; } ci_emf;
static int ci_emf_open(ci_emf *e, HENHMETAFILE h) {
    memset(e, 0, sizeof *e);
    if (!h || !U(GetEnhMetaFileBits)) return 0;
    e->h = h;
    e->n = U(GetEnhMetaFileBits)(h, 0, NULL);
    if (!e->n) return 0;
    e->b = malloc(e->n);
    if (U(GetEnhMetaFileBits)(h, e->n, e->b) != e->n) { free(e->b); e->b = NULL; return 0; }
    return 1;
}
static void ci_emf_close(ci_emf *e) {
    free(e->b);
    if (e->h && U(DeleteEnhMetaFile)) U(DeleteEnhMetaFile)(e->h);
    memset(e, 0, sizeof *e);
}
/* nth (0-based) record of a type; NULL when absent. Walks and validates sizes. */
static const EMR *ci_emf_rec(const ci_emf *e, DWORD type, int nth) {
    UINT off = 0;
    while (off + 8 <= e->n) {
        const EMR *r = (const EMR *)(e->b + off);
        if (r->nSize < 8 || (r->nSize & 3) || off + r->nSize > e->n) return NULL;
        if (r->iType == type && nth-- == 0) return r;
        off += r->nSize;
    }
    return NULL;
}
static int ci_emf_count(const ci_emf *e, DWORD type) {
    int n = 0;
    while (ci_emf_rec(e, type, n)) n++;
    return n;
}
static int ci_emf_total(const ci_emf *e) {
    UINT off = 0; int n = 0;
    while (off + 8 <= e->n) {
        const EMR *r = (const EMR *)(e->b + off);
        if (r->nSize < 8 || (r->nSize & 3) || off + r->nSize > e->n) return -1;
        off += r->nSize; n++;
    }
    return off == e->n ? n : -1;
}
static HDC ci_new_emf(void) {
    return U(CreateEnhMetaFileW) ? U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL) : NULL;
}

/* ---------------- GdiComment ---------------- */
TEST(color_icm_comment_memdc_true) {
    CI_NOWINE();
    REQUIRE(U(GdiComment));
    ci_mem m; if (!ci_mem_open(&m, 2, 2)) { t_skip("no memdc"); return; }
    BYTE d[4] = { 1, 2, 3, 4 };
    SetLastError(0xdeadbeef);
    BOOL rr = U(GdiComment)(m.dc, sizeof d, d);
    EXPECT_NE(rr, 0);
    EXPECT_EQ(GetLastError(), 0xdeadbeef);
    ci_mem_close(&m);
}
TEST(color_icm_comment_screen_dc_true) {
    CI_NOWINE();
    REQUIRE(U(GdiComment)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC d = U(GetDC)(NULL);
    BYTE b[4] = { 9, 9, 9, 9 };
    EXPECT_NE(U(GdiComment)(d, sizeof b, b), 0);
    U(ReleaseDC)(NULL, d);
}
TEST(color_icm_comment_null_dc_true) {
    CI_NOWINE();
    REQUIRE(U(GdiComment));
    BYTE b[4] = { 0 };
    SetLastError(0xdeadbeef);
    EXPECT_NE(U(GdiComment)(NULL, sizeof b, b), 0);
    EXPECT_EQ(GetLastError(), 0xdeadbeef);
}
TEST(color_icm_comment_bogus_dc_true) {
    CI_NOWINE();
    REQUIRE(U(GdiComment));
    BYTE b[4] = { 0 };
    static const ULONG_PTR v[] = { 1, 0xdeadbeef, 0x00010001, 0xffffffff };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++)
        CHECKF(U(GdiComment)((HDC)v[i], sizeof b, b) != 0, "dc=%p", (void *)v[i]);
}
TEST(color_icm_comment_deleted_memdc_true) {
    CI_NOWINE();
    REQUIRE(U(GdiComment));
    HDC d = ci_bogus_dc();
    BYTE b[4] = { 0 };
    EXPECT_NE(U(GdiComment)(d, sizeof b, b), 0);
}
TEST(color_icm_comment_non_emf_ignores_args) {
    CI_NOWINE();
    REQUIRE(U(GdiComment));
    ci_mem m; if (!ci_mem_open(&m, 2, 2)) { t_skip("no memdc"); return; }
    /* Non-EMF DCs never touch the buffer, so size/pointer are irrelevant. */
    EXPECT_NE(U(GdiComment)(m.dc, 0, NULL), 0);
    EXPECT_NE(U(GdiComment)(m.dc, 0xffffffffu, NULL), 0);
    EXPECT_NE(U(GdiComment)(m.dc, 100, NULL), 0);
    ci_mem_close(&m);
}
TEST(color_icm_comment_wmf_dc_true) {
    CI_NOWINE();
    REQUIRE(U(GdiComment)); REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    HDC d = U(CreateMetaFileW)(NULL);
    if (!d) { t_skip("no wmf dc"); return; }
    BYTE b[8] = { 1 };
    EXPECT_NE(U(GdiComment)(d, sizeof b, b), 0);
    HMETAFILE mf = U(CloseMetaFile)(d);
    if (mf) U(DeleteMetaFile)(mf);
}
TEST(color_icm_comment_emf_basic_record) {
    REQUIRE(U(GdiComment)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits)); REQUIRE(U(CreateEnhMetaFileW));
    HDC d = ci_new_emf();
    if (!d) { t_skip("no emf dc"); return; }
    BYTE data[4] = { 'A', 'B', 'C', 'D' };
    SetLastError(0xdeadbeef);
    CHECKF(U(GdiComment)(d, sizeof data, data), "err=%lu", GetLastError());
    ci_emf e;
    if (!ci_emf_open(&e, U(CloseEnhMetaFile)(d))) { t_skip("no emf bits"); return; }
    const EMR *r = ci_emf_rec(&e, CI_EMR_GDICOMMENT, 0);
    CHECK(r != NULL);
    if (r) {
        EXPECT_EQ(r->nSize, 16);
        const EMRGDICOMMENT *c = (const EMRGDICOMMENT *)r;
        EXPECT_EQ(c->cbData, 4);
        CHECK(memcmp(c->Data, data, 4) == 0);
    }
    EXPECT_EQ(ci_emf_count(&e, CI_EMR_GDICOMMENT), 1);
    ci_emf_close(&e);
}
TEST(color_icm_comment_emf_size_table) {
    REQUIRE(U(GdiComment)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits)); REQUIRE(U(CreateEnhMetaFileW));
    static const UINT sizes[] = { 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 31, 32, 33, 100, 255, 256, 257, 1000, 4096, 4097, 65535 };
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        UINT n = sizes[i];
        BYTE *data = malloc(n);
        for (UINT k = 0; k < n; k++) data[k] = (BYTE)(k * 7 + i);
        HDC d = ci_new_emf();
        if (!d) { free(data); t_skip("no emf dc"); return; }
        BOOL ok = U(GdiComment)(d, n, data);
        CHECKF(ok, "n=%u", n);
        ci_emf e;
        if (ci_emf_open(&e, U(CloseEnhMetaFile)(d))) {
            const EMR *r = ci_emf_rec(&e, CI_EMR_GDICOMMENT, 0);
            CHECKF(r != NULL, "n=%u no record", n);
            if (r) {
                const EMRGDICOMMENT *c = (const EMRGDICOMMENT *)r;
                CHECKF(c->cbData == n, "n=%u cbData=%lu", n, c->cbData);
                CHECKF(r->nSize == 12 + ((n + 3) & ~3u), "n=%u nSize=%lu", n, r->nSize);
                CHECKF(memcmp(c->Data, data, n) == 0, "n=%u data differs", n);
            }
            ci_emf_close(&e);
        }
        free(data);
    }
}
TEST(color_icm_comment_emf_large) {
    REQUIRE(U(GdiComment)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits)); REQUIRE(U(CreateEnhMetaFileW));
    UINT n = 200000;
    BYTE *data = malloc(n);
    for (UINT k = 0; k < n; k++) data[k] = (BYTE)(k ^ (k >> 8));
    HDC d = ci_new_emf();
    if (!d) { free(data); t_skip("no emf dc"); return; }
    CHECK(U(GdiComment)(d, n, data));
    ci_emf e;
    if (ci_emf_open(&e, U(CloseEnhMetaFile)(d))) {
        const EMR *r = ci_emf_rec(&e, CI_EMR_GDICOMMENT, 0);
        CHECK(r != NULL);
        if (r) {
            const EMRGDICOMMENT *c = (const EMRGDICOMMENT *)r;
            EXPECT_EQ(c->cbData, n);
            CHECK(memcmp(c->Data, data, n) == 0);
        }
        ci_emf_close(&e);
    }
    free(data);
}
TEST(color_icm_comment_emf_multiple_in_order) {
    REQUIRE(U(GdiComment)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits)); REQUIRE(U(CreateEnhMetaFileW));
    HDC d = ci_new_emf();
    if (!d) { t_skip("no emf dc"); return; }
    for (int i = 0; i < 10; i++) {
        BYTE b[8]; memset(b, 0x10 + i, sizeof b);
        CHECK(U(GdiComment)(d, sizeof b, b));
    }
    ci_emf e;
    if (!ci_emf_open(&e, U(CloseEnhMetaFile)(d))) { t_skip("no emf bits"); return; }
    EXPECT_EQ(ci_emf_count(&e, CI_EMR_GDICOMMENT), 10);
    for (int i = 0; i < 10; i++) {
        const EMR *r = ci_emf_rec(&e, CI_EMR_GDICOMMENT, i);
        CHECKF(r && ((const EMRGDICOMMENT *)r)->Data[0] == 0x10 + i, "i=%d", i);
    }
    ci_emf_close(&e);
}
TEST(color_icm_comment_emf_header_consistency) {
    REQUIRE(U(GdiComment)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits)); REQUIRE(U(CreateEnhMetaFileW));
    HDC d = ci_new_emf();
    if (!d) { t_skip("no emf dc"); return; }
    BYTE b[5] = { 1, 2, 3, 4, 5 };
    U(GdiComment)(d, sizeof b, b);
    U(GdiComment)(d, sizeof b, b);
    ci_emf e;
    if (!ci_emf_open(&e, U(CloseEnhMetaFile)(d))) { t_skip("no emf bits"); return; }
    const ENHMETAHEADER *h = (const ENHMETAHEADER *)e.b;
    EXPECT_EQ(h->iType, CI_EMR_HEADER);
    EXPECT_EQ(h->dSignature, ENHMETA_SIGNATURE);
    EXPECT_EQ(h->nBytes, e.n);
    EXPECT_EQ(ci_emf_total(&e), (int)h->nRecords);
    EXPECT_EQ(h->nRecords, 4);   /* header + 2 comments + EOF */
    const EMR *last = NULL; UINT off = 0;
    while (off < e.n) { last = (const EMR *)(e.b + off); off += last->nSize; }
    CHECK(last && last->iType == CI_EMR_EOF);
    ci_emf_close(&e);
}
TEST(color_icm_comment_emf_interleaved_with_drawing) {
    REQUIRE(U(GdiComment)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits)); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(Rectangle));
    HDC d = ci_new_emf();
    if (!d) { t_skip("no emf dc"); return; }
    BYTE b[4] = { 'T', 'E', 'S', 'T' };
    U(GdiComment)(d, sizeof b, b);
    U(Rectangle)(d, 1, 1, 10, 10);
    U(GdiComment)(d, sizeof b, b);
    ci_emf e;
    if (!ci_emf_open(&e, U(CloseEnhMetaFile)(d))) { t_skip("no emf bits"); return; }
    /* order: header, comment, ..., rectangle, comment, EOF */
    UINT off = 0; int idx_c1 = -1, idx_r = -1, idx_c2 = -1, i = 0;
    while (off < e.n) {
        const EMR *r = (const EMR *)(e.b + off);
        if (r->iType == CI_EMR_GDICOMMENT) { if (idx_c1 < 0) idx_c1 = i; else idx_c2 = i; }
        if (r->iType == CI_EMR_RECTANGLE) idx_r = i;
        off += r->nSize; i++;
    }
    CHECK(idx_c1 >= 0 && idx_r > idx_c1 && idx_c2 > idx_r);
    ci_emf_close(&e);
}
TEST(color_icm_comment_emf_after_close_fails) {
    REQUIRE(U(GdiComment)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile)); REQUIRE(U(CreateEnhMetaFileW));
    HDC d = ci_new_emf();
    if (!d) { t_skip("no emf dc"); return; }
    HENHMETAFILE h = U(CloseEnhMetaFile)(d);
    BYTE b[4] = { 0 };
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(GdiComment)(d, sizeof b, b), 0);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    if (h) U(DeleteEnhMetaFile)(h);
}
TEST(color_icm_comment_emf_public_gdic_signature) {
    REQUIRE(U(GdiComment)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits)); REQUIRE(U(CreateEnhMetaFileW));
    HDC d = ci_new_emf();
    if (!d) { t_skip("no emf dc"); return; }
    BYTE b[12] = { 'G', 'D', 'I', 'C', 1, 0, 0, 0, 9, 9, 9, 9 };
    CHECK(U(GdiComment)(d, sizeof b, b));
    ci_emf e;
    if (!ci_emf_open(&e, U(CloseEnhMetaFile)(d))) { t_skip("no emf bits"); return; }
    const EMR *r = ci_emf_rec(&e, CI_EMR_GDICOMMENT, 0);
    CHECK(r != NULL);
    if (r) CHECK(memcmp(((const EMRGDICOMMENT *)r)->Data, b, 12) == 0);
    ci_emf_close(&e);
}
static int CALLBACK ci_count_cb(HDC dc, HANDLETABLE *ht, const ENHMETARECORD *r, int n, LPARAM lp) {
    int *c = (int *)lp;
    if (r->iType == CI_EMR_GDICOMMENT) c[0]++;
    c[1]++;
    return 1;
}
TEST(color_icm_comment_emf_enumerate) {
    REQUIRE(U(GdiComment)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(EnumEnhMetaFile)); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(DeleteEnhMetaFile));
    HDC d = ci_new_emf();
    if (!d) { t_skip("no emf dc"); return; }
    BYTE b[4] = { 1, 2, 3, 4 };
    U(GdiComment)(d, sizeof b, b);
    U(GdiComment)(d, sizeof b, b);
    U(GdiComment)(d, sizeof b, b);
    HENHMETAFILE h = U(CloseEnhMetaFile)(d);
    ci_mem m; if (!h || !ci_mem_open(&m, 4, 4)) { t_skip("setup"); return; }
    int c[2] = { 0, 0 };
    RECT rc = { 0, 0, 4, 4 };
    BOOL r = U(EnumEnhMetaFile)(m.dc, h, ci_count_cb, c, &rc);
    CHECK(r);
    EXPECT_EQ(c[0], 3);
    EXPECT_EQ(c[1], 5);
    ci_mem_close(&m);
    U(DeleteEnhMetaFile)(h);
}
TEST(color_icm_comment_emf_play_returns_true) {
    REQUIRE(U(GdiComment)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(PlayEnhMetaFile)); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(DeleteEnhMetaFile));
    HDC d = ci_new_emf();
    if (!d) { t_skip("no emf dc"); return; }
    BYTE b[4] = { 1, 2, 3, 4 };
    U(GdiComment)(d, sizeof b, b);
    HENHMETAFILE h = U(CloseEnhMetaFile)(d);
    ci_mem m; if (!h || !ci_mem_open(&m, 4, 4)) { t_skip("setup"); return; }
    RECT rc = { 0, 0, 4, 4 };
    CHECK(U(PlayEnhMetaFile)(m.dc, h, &rc));
    ci_mem_close(&m);
    U(DeleteEnhMetaFile)(h);
}
TEST(color_icm_comment_emf_zero_size) {
    REQUIRE(U(GdiComment)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits)); REQUIRE(U(CreateEnhMetaFileW));
    HDC d = ci_new_emf();
    if (!d) { t_skip("no emf dc"); return; }
    BYTE b[4] = { 1, 2, 3, 4 };
    SetLastError(0xdeadbeef);
    BOOL r = U(GdiComment)(d, 0, b);
    t_note("GdiComment(emf, 0 bytes) = %d err=%lu", r, GetLastError());
    ci_emf e;
    if (ci_emf_open(&e, U(CloseEnhMetaFile)(d))) {
        const EMR *rec = ci_emf_rec(&e, CI_EMR_GDICOMMENT, 0);
        t_note("zero-size comment record %s (nSize=%lu)", rec ? "present" : "absent", rec ? rec->nSize : 0);
        EXPECT_EQ(ci_emf_total(&e) > 0, 1);
        ci_emf_close(&e);
    }
}

/* ---------------- color spaces / ICM in an EMF ---------------- */
TEST(color_icm_emf_setcolorspace_records) {
    CI_NOWINE();
    REQUIRE(U(SetColorSpace)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace)); REQUIRE(U(CloseEnhMetaFile));
    HDC d = ci_new_emf();
    if (!d) { t_skip("no emf dc"); return; }
    HCOLORSPACE cs = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    if (!cs) { U(DeleteEnhMetaFile)(U(CloseEnhMetaFile)(d)); t_skip("no color space"); return; }
    SetLastError(0xdeadbeef);
    HCOLORSPACE prev = U(SetColorSpace)(d, cs);
    t_note("SetColorSpace(emf) = %p err=%lu", prev, GetLastError());
    ci_emf e;
    if (ci_emf_open(&e, U(CloseEnhMetaFile)(d))) {
        int mk = ci_emf_count(&e, CI_EMR_CREATECOLORSPACE) + ci_emf_count(&e, CI_EMR_CREATECOLORSPACEW);
        int set = ci_emf_count(&e, CI_EMR_SETCOLORSPACE);
        t_note("records: create=%d set=%d total=%d", mk, set, ci_emf_total(&e));
        if (prev) { EXPECT_EQ(set, 1); CHECK(mk >= 1); }
        ci_emf_close(&e);
    }
    CHECK(U(DeleteColorSpace)(cs));
}
TEST(color_icm_emf_setcolorspace_record_order) {
    CI_NOWINE();
    REQUIRE(U(SetColorSpace)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace)); REQUIRE(U(CloseEnhMetaFile));
    HDC d = ci_new_emf();
    if (!d) { t_skip("no emf dc"); return; }
    HCOLORSPACE cs = ci_mkw(CI_LCS_sRGB, LCS_GM_IMAGES);
    if (!cs) { U(DeleteEnhMetaFile)(U(CloseEnhMetaFile)(d)); t_skip("no color space"); return; }
    HCOLORSPACE prev = U(SetColorSpace)(d, cs);
    ci_emf e;
    if (ci_emf_open(&e, U(CloseEnhMetaFile)(d)) && prev) {
        int ic = -1, is = -1, i = 0; UINT off = 0;
        while (off < e.n) {
            const EMR *r = (const EMR *)(e.b + off);
            if (r->iType == CI_EMR_CREATECOLORSPACE || r->iType == CI_EMR_CREATECOLORSPACEW) { if (ic < 0) ic = i; }
            if (r->iType == CI_EMR_SETCOLORSPACE && is < 0) is = i;
            off += r->nSize; i++;
        }
        CHECKF(ic >= 0 && is > ic, "create@%d set@%d", ic, is);
    }
    if (e.b) ci_emf_close(&e);
    U(DeleteColorSpace)(cs);
}
TEST(color_icm_emf_seticmmode_records) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode)); REQUIRE(U(CloseEnhMetaFile));
    HDC d = ci_new_emf();
    if (!d) { t_skip("no emf dc"); return; }
    int q1 = U(SetICMMode)(d, ICM_QUERY);
    int on = U(SetICMMode)(d, ICM_ON);
    int q2 = U(SetICMMode)(d, ICM_QUERY);
    int off = U(SetICMMode)(d, ICM_OFF);
    int dn = U(SetICMMode)(d, ICM_DONE_OUTSIDEDC);
    EXPECT_EQ(q1, ICM_OFF); EXPECT_NE(on, 0); EXPECT_EQ(q2, ICM_ON); EXPECT_NE(off, 0); EXPECT_NE(dn, 0);
    ci_emf e;
    if (ci_emf_open(&e, U(CloseEnhMetaFile)(d))) {
        EXPECT_EQ(ci_emf_count(&e, CI_EMR_SETICMMODE), 3);
        static const DWORD want[] = { 2, 1, 4 };
        for (int i = 0; i < 3; i++) {
            const EMR *r = ci_emf_rec(&e, CI_EMR_SETICMMODE, i);
            CHECKF(r != NULL, "rec %d", i);
            if (r) {
                EXPECT_EQ(r->nSize, 12);
                CHECKF(((const EMRSETICMMODE *)r)->iMode == want[i], "rec %d mode=%lu", i, ((const EMRSETICMMODE *)r)->iMode);
            }
        }
        ci_emf_close(&e);
    }
}
TEST(color_icm_emf_seticmmode_invalid) {
    CI_NOWINE();
    REQUIRE(U(SetICMMode)); REQUIRE(U(CloseEnhMetaFile));
    HDC d = ci_new_emf();
    if (!d) { t_skip("no emf dc"); return; }
    static const int modes[] = { 0, 5, 6, -1, 0x7fffffff };
    for (unsigned i = 0; i < sizeof modes / sizeof *modes; i++)
        CHECKF(U(SetICMMode)(d, modes[i]) == 0, "mode=%d", modes[i]);
    ci_emf e;
    if (ci_emf_open(&e, U(CloseEnhMetaFile)(d))) {
        EXPECT_EQ(ci_emf_count(&e, CI_EMR_SETICMMODE), 0);
        ci_emf_close(&e);
    }
}
TEST(color_icm_emf_seticmprofile_w) {
    CI_NOWINE();
    REQUIRE(U(SetICMProfileW)); REQUIRE(U(CloseEnhMetaFile));
    HDC d = ci_new_emf();
    if (!d) { t_skip("no emf dc"); return; }
    WCHAR n[] = L"sRGB Color Space Profile.icm";
    SetLastError(0xdeadbeef);
    BOOL r = U(SetICMProfileW)(d, n);
    t_note("SetICMProfileW(emf) = %d err=%lu", r, GetLastError());
    ci_emf e;
    if (ci_emf_open(&e, U(CloseEnhMetaFile)(d))) {
        t_note("SETICMPROFILEW records=%d A records=%d", ci_emf_count(&e, CI_EMR_SETICMPROFILEW), ci_emf_count(&e, CI_EMR_SETICMPROFILEA));
        ci_emf_close(&e);
    }
}
TEST(color_icm_emf_seticmprofile_null_name) {
    CI_NOWINE();
    REQUIRE(U(SetICMProfileW)); REQUIRE(U(SetICMProfileA)); REQUIRE(U(CloseEnhMetaFile));
    HDC d = ci_new_emf();
    if (!d) { t_skip("no emf dc"); return; }
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(SetICMProfileW)(d, NULL), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef);
    EXPECT_EQ(U(SetICMProfileA)(d, NULL), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    ci_emf e;
    if (ci_emf_open(&e, U(CloseEnhMetaFile)(d))) {
        EXPECT_EQ(ci_emf_count(&e, CI_EMR_SETICMPROFILEW) + ci_emf_count(&e, CI_EMR_SETICMPROFILEA), 0);
        ci_emf_close(&e);
    }
}
TEST(color_icm_emf_deletecolorspace_not_recorded) {
    CI_NOWINE();
    REQUIRE(U(DeleteColorSpace)); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(CloseEnhMetaFile));
    HDC d = ci_new_emf();
    if (!d) { t_skip("no emf dc"); return; }
    HCOLORSPACE cs = ci_mkw(CI_LCS_CALIBRATED_RGB, LCS_GM_IMAGES);
    CHECK(cs && U(DeleteColorSpace)(cs));
    ci_emf e;
    if (ci_emf_open(&e, U(CloseEnhMetaFile)(d))) {
        EXPECT_EQ(ci_emf_count(&e, CI_EMR_DELETECOLORSPACE), 0);
        EXPECT_EQ(ci_emf_total(&e), 2);   /* header + EOF only */
        ci_emf_close(&e);
    }
}
TEST(color_icm_emf_getcolorspace_on_emf_dc) {
    CI_NOWINE();
    REQUIRE(U(GetColorSpace)); REQUIRE(U(CloseEnhMetaFile));
    HDC d = ci_new_emf();
    if (!d) { t_skip("no emf dc"); return; }
    SetLastError(0xdeadbeef);
    HCOLORSPACE cs = U(GetColorSpace)(d);
    t_note("GetColorSpace(emf dc) = %p err=%lu", cs, GetLastError());
    U(DeleteEnhMetaFile)(U(CloseEnhMetaFile)(d));
}
TEST(color_icm_emf_empty_has_two_records) {
    REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits)); REQUIRE(U(CreateEnhMetaFileW));
    HDC d = ci_new_emf();
    if (!d) { t_skip("no emf dc"); return; }
    ci_emf e;
    if (ci_emf_open(&e, U(CloseEnhMetaFile)(d))) {
        EXPECT_EQ(ci_emf_total(&e), 2);
        EXPECT_EQ(ci_emf_count(&e, CI_EMR_EOF), 1);
        ci_emf_close(&e);
    }
}

/* ---------------- world transform records in an EMF ---------------- */
TEST(color_icm_emf_setworldtransform_record) {
    REQUIRE(U(SetWorldTransform)); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(CloseEnhMetaFile));
    HDC d = ci_new_emf();
    if (!d) { t_skip("no emf dc"); return; }
    int oldgm = U(SetGraphicsMode)(d, GM_ADVANCED);
    EXPECT_EQ(oldgm, GM_COMPATIBLE);
    XFORM xf = { 2.0f, 0.5f, -1.0f, 3.0f, 10.25f, -20.5f };
    SetLastError(0xdeadbeef);
    BOOL r = U(SetWorldTransform)(d, &xf);
    CHECKF(r, "err=%lu", GetLastError());
    ci_emf e;
    if (ci_emf_open(&e, U(CloseEnhMetaFile)(d))) {
        const EMR *r35 = ci_emf_rec(&e, CI_EMR_SETWORLDTRANSFORM, 0);
        const EMR *r36 = ci_emf_rec(&e, CI_EMR_MODIFYWORLDTRANSFORM, 0);
        t_note("SETWORLDTRANSFORM=%d MODIFYWORLDTRANSFORM=%d", ci_emf_count(&e, CI_EMR_SETWORLDTRANSFORM), ci_emf_count(&e, CI_EMR_MODIFYWORLDTRANSFORM));
        CHECK(r35 != NULL || r36 != NULL);
        const XFORM *got = r35 ? &((const EMRSETWORLDTRANSFORM *)r35)->xform : r36 ? &((const EMRMODIFYWORLDTRANSFORM *)r36)->xform : NULL;
        if (got) CHECK(memcmp(got, &xf, sizeof xf) == 0);
        ci_emf_close(&e);
    }
}
TEST(color_icm_emf_modifyworldtransform_records) {
    REQUIRE(U(ModifyWorldTransform)); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(CloseEnhMetaFile));
    HDC d = ci_new_emf();
    if (!d) { t_skip("no emf dc"); return; }
    U(SetGraphicsMode)(d, GM_ADVANCED);
    XFORM xf = { 1.5f, 0.0f, 0.0f, 2.0f, 3.0f, 4.0f };
    static const DWORD modes[] = { MWT_LEFTMULTIPLY, MWT_RIGHTMULTIPLY, MWT_IDENTITY, MWT_LEFTMULTIPLY };
    for (int i = 0; i < 4; i++) CHECKF(U(ModifyWorldTransform)(d, &xf, modes[i]), "mode=%lu err=%lu", modes[i], GetLastError());
    ci_emf e;
    if (ci_emf_open(&e, U(CloseEnhMetaFile)(d))) {
        EXPECT_EQ(ci_emf_count(&e, CI_EMR_MODIFYWORLDTRANSFORM), 4);
        for (int i = 0; i < 4; i++) {
            const EMR *r = ci_emf_rec(&e, CI_EMR_MODIFYWORLDTRANSFORM, i);
            CHECKF(r != NULL, "rec %d", i);
            if (r) {
                EXPECT_EQ(r->nSize, 36);
                CHECKF(((const EMRMODIFYWORLDTRANSFORM *)r)->iMode == modes[i], "rec %d mode=%lu", i, ((const EMRMODIFYWORLDTRANSFORM *)r)->iMode);
            }
        }
        ci_emf_close(&e);
    }
}
TEST(color_icm_emf_setworldtransform_compat_mode_fails) {
    REQUIRE(U(SetWorldTransform)); REQUIRE(U(CloseEnhMetaFile));
    HDC d = ci_new_emf();
    if (!d) { t_skip("no emf dc"); return; }
    XFORM xf = { 1, 0, 0, 1, 5, 5 };
    EXPECT_EQ(U(SetWorldTransform)(d, &xf), 0);
    U(DeleteEnhMetaFile)(U(CloseEnhMetaFile)(d));
}
