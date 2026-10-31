/* Additional edge cases: bit-buffer trimming, record-size limits, call parity. */
#include "wmf.h"

static int CALLBACK xr_count_cb(HDC dc, HANDLETABLE *ht, METARECORD *mr, int n, LPARAM lp) {
    (*(int *)lp)++;
    return 1;
}

static HMETAFILE xr_sample(WB *b) {
    HDC dc = mf_new();
    if (!dc) return NULL;
    U(Rectangle)(dc, 1, 2, 30, 40);
    U(MoveToEx)(dc, 3, 4, NULL);
    U(LineTo)(dc, 20, 25);
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (h && b) wb_get(h, b);
    return h;
}

TEST(wmf_xtra_setbits_trailing_bytes_trimmed) {
    WMF_REQ_BASE(); REQUIRE(U(SetMetaFileBitsEx));
    WB b; HMETAFILE h = xr_sample(&b);
    REQUIRE(h && b.p);
    U(DeleteMetaFile)(h);
    BYTE *big = malloc(b.n + 64);
    REQUIRE(big);
    memcpy(big, b.p, b.n);
    memset(big + b.n, 0x5A, 64);
    HMETAFILE h2 = U(SetMetaFileBitsEx)(b.n + 64, big);
    if (!h2) { t_note("metafile with trailing bytes rejected"); free(big); wb_free(&b); return; }
    UINT got = U(GetMetaFileBitsEx)(h2, 0, NULL);
    /* mtSize says where the metafile ends; trailing bytes are not part of it. */
    CHECKF(got == b.n, "size %u want %u", got, b.n);
    WB c; wb_get(h2, &c);
    if (c.p && c.n == b.n) CHECK(memcmp(c.p, b.p, b.n) == 0);
    wb_free(&c);
    U(DeleteMetaFile)(h2);
    free(big); wb_free(&b);
}

TEST(wmf_xtra_setbits_exact_size_roundtrip_bytes) {
    WMF_REQ_BASE(); REQUIRE(U(SetMetaFileBitsEx));
    WB b; HMETAFILE h = xr_sample(&b);
    REQUIRE(h && b.p);
    U(DeleteMetaFile)(h);
    HMETAFILE h2 = U(SetMetaFileBitsEx)(b.n, b.p);
    REQUIRE(h2);
    WB c; wb_get(h2, &c);
    CHECK(c.n == b.n);
    if (c.p && c.n == b.n) CHECK(memcmp(c.p, b.p, b.n) == 0);
    CHECK(wb_valid(&c));
    wb_free(&c);
    U(DeleteMetaFile)(h2);
    wb_free(&b);
}

TEST(wmf_xtra_bits_null_buffer_ignores_size) {
    WMF_REQ_BASE();
    HMETAFILE h = xr_sample(NULL);
    REQUIRE(h);
    UINT n = U(GetMetaFileBitsEx)(h, 0, NULL);
    CHECK(n != 0);
    for (UINT cb = 1; cb < 4; cb++) EXPECT_EQ(U(GetMetaFileBitsEx)(h, cb, NULL), n);
    /* a too-small buffer is a failure, not a partial copy */
    BYTE tmp[8]; memset(tmp, 0xCC, sizeof tmp);
    EXPECT_EQ(U(GetMetaFileBitsEx)(h, 8, tmp), 0);
    for (int i = 0; i < 8; i++) CHECK(tmp[i] == 0xCC);
    U(DeleteMetaFile)(h);
}

TEST(wmf_xtra_pmr_oversized_record) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFileRecord));
    if (on_wine()) { t_skip("wine reads the huge record"); return; }
    DIBDC d; REQUIRE(dibdc_make(&d, 16, 16));
    HANDLETABLE ht; memset(&ht, 0, sizeof ht);
    struct { DWORD size; WORD func; WORD p[4]; } r = { 0x80000000u, 0x0000, { 0 } };
    /* size in bytes overflows 32 bits */
    CHECK(!U(PlayMetaFileRecord)(d.dc, &ht, (METARECORD *)&r, 1));
    r.size = 0xFFFFFFFFu;
    CHECK(!U(PlayMetaFileRecord)(d.dc, &ht, (METARECORD *)&r, 1));
    dibdc_free(&d);
}

TEST(wmf_xtra_pmr_delete_empty_slot_fails) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFileRecord));
    if (on_wine()) { t_skip("wine differs on empty slot deletion"); return; }
    DIBDC d; REQUIRE(dibdc_make(&d, 16, 16));
    HANDLETABLE ht[3]; memset(ht, 0, sizeof ht);
    struct { DWORD size; WORD func; WORD p[1]; } r = { 4, 0x01F0, { 1 } };
    CHECK(!U(PlayMetaFileRecord)(d.dc, ht, (METARECORD *)&r, 3));   /* empty slot */
    r.p[0] = 3;
    CHECK(!U(PlayMetaFileRecord)(d.dc, ht, (METARECORD *)&r, 3));   /* index == noObjs */
    dibdc_free(&d);
}

TEST(wmf_xtra_pmr_delete_clears_slot) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFileRecord)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(GetObjectType));
    DIBDC d; REQUIRE(dibdc_make(&d, 16, 16));
    HANDLETABLE ht[2]; memset(ht, 0, sizeof ht);
    HBRUSH b = U(CreateSolidBrush)(RGB(1, 2, 3));
    REQUIRE(b);
    ht[1].objectHandle[0] = b;
    struct { DWORD size; WORD func; WORD p[1]; } r = { 4, 0x01F0, { 1 } };
    CHECK(U(PlayMetaFileRecord)(d.dc, ht, (METARECORD *)&r, 2));
    CHECK(ht[1].objectHandle[0] == NULL);
    EXPECT_EQ(U(GetObjectType)(b), 0);   /* really deleted */
    dibdc_free(&d);
}

TEST(wmf_xtra_pmr_unused_opcodes_succeed) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFileRecord));
    DIBDC d; REQUIRE(dibdc_make(&d, 16, 16));
    HANDLETABLE ht[2]; memset(ht, 0, sizeof ht);
    /* opcode 0 (EOF) and 5 are accepted as no-ops */
    struct { DWORD size; WORD func; WORD p[2]; } r = { 3, 0, { 0, 0 } };
    CHECK(U(PlayMetaFileRecord)(d.dc, ht, (METARECORD *)&r, 2));
    r.func = 0x0005;
    CHECK(U(PlayMetaFileRecord)(d.dc, ht, (METARECORD *)&r, 2));
    CHECK(dibdc_nonwhite(&d) == 0);
    dibdc_free(&d);
}

TEST(wmf_xtra_enum_null_dc_with_proc_counts_records) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile));
    WB b; HMETAFILE h = xr_sample(&b);
    REQUIRE(h && b.p);
    int expect = wb_nrecs(&b) - 1;   /* the EOF record is not delivered */
    struct C { int n; } c = { 0 };
    BOOL r = U(EnumMetaFile)(NULL, h, xr_count_cb, (LPARAM)&c);
    CHECK(r);
    CHECKF(c.n == expect, "records %d want %d", c.n, expect);
    U(DeleteMetaFile)(h);
    wb_free(&b);
}
