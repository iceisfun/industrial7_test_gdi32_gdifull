#include "emf.h"

static HENHMETAFILE mk_enum_scene(void) {
    HDC dc = emf_begin();
    if (!dc) return NULL;
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0); HGDIOBJ o = U(SelectObject)(dc, p);
    U(Rectangle)(dc, 1, 1, 20, 20);
    U(Ellipse)(dc, 5, 5, 30, 30);
    U(SelectObject)(dc, o); U(DeleteObject)(p);
    return emf_end(dc);
}

/* ---------- EnumEnhMetaFile ---------- */

TEST(emf_enum_sequence_header_to_eof) {
    REQUIRE(U(EnumEnhMetaFile));
    HENHMETAFILE h = mk_enum_scene();
    if (!h) { CHECK(h != NULL); return; }
    emf_log l; BOOL r = emf_enum(h, &l);
    CHECK(r);
    CHECK(l.n >= 8);
    EXPECT_EQ(l.type[0], EMR_HEADER);
    EXPECT_EQ(l.type[l.n - 1], EMR_EOF);
    for (int i = 1; i < l.n - 1; i++) CHECKF(l.type[i] != EMR_HEADER && l.type[i] != EMR_EOF, "stray at %d", i);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_enum_param_passthrough) {
    REQUIRE(U(EnumEnhMetaFile));
    HENHMETAFILE h = mk_enum_scene();
    if (!h) { CHECK(h != NULL); return; }
    emf_log l; emf_enum(h, &l);
    EXPECT_EQ(l.seen_param, (LPARAM)&l);
    EXPECT_EQ(l.bad_dc, 0);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_enum_handle_table_size) {
    REQUIRE(U(EnumEnhMetaFile));
    HENHMETAFILE h = mk_enum_scene();
    if (!h) { CHECK(h != NULL); return; }
    emf_log l; emf_enum(h, &l);
    ENHMETAHEADER hd; U(GetEnhMetaFileHeader)(h, sizeof hd, &hd);
    EXPECT_EQ(l.nobj, hd.nHandles);
    CHECK(l.nobj >= 2);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_enum_early_stop) {
    REQUIRE(U(EnumEnhMetaFile));
    HENHMETAFILE h = mk_enum_scene();
    if (!h) { CHECK(h != NULL); return; }
    for (int stop = 1; stop <= 4; stop++) {
        HDC dc = U(CreateCompatibleDC)(NULL); RECT rc = {0, 0, 50, 50};
        emf_log l; memset(&l, 0, sizeof l); l.stop_at = stop;
        BOOL r = U(EnumEnhMetaFile)(dc, h, emf_log_cb, &l, &rc);
        EXPECT_EQ(l.n, stop);
        SOFT(r == FALSE);
        t_note("stop at %d: result %d", stop, r);
        U(DeleteDC)(dc);
    }
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_enum_null_callback) {
    REQUIRE(U(EnumEnhMetaFile));
    if (emf_wine()) { t_skip("wine faults on NULL callback"); return; }
    HENHMETAFILE h = mk_enum_scene();
    if (!h) { CHECK(h != NULL); return; }
    HDC dc = U(CreateCompatibleDC)(NULL); RECT rc = {0, 0, 50, 50};
    SetLastError(0);
    CHECK(U(EnumEnhMetaFile)(dc, h, NULL, NULL, &rc) == FALSE);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    U(DeleteDC)(dc);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_enum_invalid_handle) {
    REQUIRE(U(EnumEnhMetaFile));
    HDC dc = U(CreateCompatibleDC)(NULL); RECT rc = {0, 0, 50, 50};
    emf_log l; memset(&l, 0, sizeof l);
    HENHMETAFILE bad[] = { NULL, (HENHMETAFILE)1, (HENHMETAFILE)0xdeadbeef };
    for (unsigned i = 0; i < 3; i++) {
        CHECKF(U(EnumEnhMetaFile)(dc, bad[i], emf_log_cb, &l, &rc) == FALSE, "case %u", i);
    }
    EXPECT_EQ(l.n, 0);
    U(DeleteDC)(dc);
}

TEST(emf_enum_deleted_handle) {
    REQUIRE(U(EnumEnhMetaFile));
    HENHMETAFILE h = mk_enum_scene();
    if (!h) { CHECK(h != NULL); return; }
    U(DeleteEnhMetaFile)(h);
    emf_log l; BOOL r = emf_enum(h, &l);
    CHECK(r == FALSE);
    EXPECT_EQ(l.n, 0);
}

TEST(emf_enum_repeatable) {
    REQUIRE(U(EnumEnhMetaFile));
    HENHMETAFILE h = mk_enum_scene();
    if (!h) { CHECK(h != NULL); return; }
    emf_log a, b; emf_enum(h, &a); emf_enum(h, &b);
    CHECK(a.n == b.n && !memcmp(a.type, b.type, a.n * sizeof(DWORD)) && !memcmp(a.size, b.size, a.n * sizeof(DWORD)));
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_enum_matches_raw_parse) {
    REQUIRE(U(EnumEnhMetaFile));
    HENHMETAFILE h = mk_enum_scene();
    if (!h) { CHECK(h != NULL); return; }
    emf_log l; emf_enum(h, &l);
    UINT n; BYTE *b = emf_bits(h, &n);
    CHECK(b != NULL);
    if (b) {
        UINT off = 0; int i = 0;
        while (off < n && i < l.n) {
            const ENHMETARECORD *r = (const ENHMETARECORD *)(b + off);
            CHECKF(r->iType == l.type[i] && r->nSize == l.size[i], "rec %d", i);
            off += r->nSize; i++;
        }
        CHECK(i == l.n && off == n);
        free(b);
    }
    U(DeleteEnhMetaFile)(h);
}

static int CALLBACK ptr_cb(HDC dc, HANDLETABLE *ht, const ENHMETARECORD *r, int nobj, LPARAM lp) {
    struct { int n; const ENHMETARECORD *last; int bad; int htbad; } *s = (void *)lp;
    if (!r || ((ULONG_PTR)r & 3)) s->bad++;
    if (s->last && (const BYTE *)r != (const BYTE *)s->last + s->last->nSize) s->bad++;
    if (!ht) s->htbad++;
    s->last = r; s->n++;
    return 1;
}
TEST(emf_enum_record_pointers_contiguous) {
    REQUIRE(U(EnumEnhMetaFile));
    HENHMETAFILE h = mk_enum_scene();
    if (!h) { CHECK(h != NULL); return; }
    HDC dc = U(CreateCompatibleDC)(NULL); RECT rc = {0, 0, 50, 50};
    struct { int n; const ENHMETARECORD *last; int bad; int htbad; } s = {0};
    U(EnumEnhMetaFile)(dc, h, ptr_cb, &s, &rc);
    CHECK(s.n >= 8);
    CHECK(s.bad == 0);
    CHECK(s.htbad == 0);
    U(DeleteDC)(dc); U(DeleteEnhMetaFile)(h);
}

static int CALLBACK ht0_cb(HDC dc, HANDLETABLE *ht, const ENHMETARECORD *r, int nobj, LPARAM lp) {
    HGDIOBJ *first = (HGDIOBJ *)lp;
    if (r->iType == EMR_HEADER || *first == NULL) *first = ht->objectHandle[0];
    return 1;
}
TEST(emf_enum_handle_zero_is_metafile) {
    REQUIRE(U(EnumEnhMetaFile));
    HENHMETAFILE h = mk_enum_scene();
    if (!h) { CHECK(h != NULL); return; }
    HDC dc = U(CreateCompatibleDC)(NULL); RECT rc = {0, 0, 50, 50};
    HGDIOBJ first = NULL;
    U(EnumEnhMetaFile)(dc, h, ht0_cb, &first, &rc);
    t_note("handle table [0] = %p (hemf %p)", first, h);
    SOFT(first == (HGDIOBJ)h);
    U(DeleteDC)(dc); U(DeleteEnhMetaFile)(h);
}

static int CALLBACK objs_cb(HDC dc, HANDLETABLE *ht, const ENHMETARECORD *r, int nobj, LPARAM lp) {
    int *info = (int *)lp;   /* [0]: pen seen non-null after playing CREATEPEN, [1]: type, [2]: record index */
    if (r->iType == EMR_HEADER) return 1;
    BOOL ok = U(PlayEnhMetaFileRecord)(dc, ht, r, nobj);
    if (r->iType == EMR_CREATEPEN) {
        info[0] = ht->objectHandle[1] != NULL;
        info[1] = U(GetObjectType)(ht->objectHandle[1]);
    }
    if (r->iType == EMR_DELETEOBJECT) info[2] = ht->objectHandle[1] == NULL;
    (void)ok;
    return 1;
}
TEST(emf_enum_handle_table_objects_created_and_deleted) {
    REQUIRE(U(EnumEnhMetaFile)); REQUIRE(U(PlayEnhMetaFileRecord));
    HENHMETAFILE h = mk_enum_scene();
    if (!h) { CHECK(h != NULL); return; }
    emf_dib d;
    if (!emf_dib_make(&d, 40, 40, 0xFFFFFF)) { U(DeleteEnhMetaFile)(h); t_skip("no dib"); return; }
    RECT rc = {0, 0, 40, 40}; int info[3] = {0, 0, 0};
    DWORD g0 = emf_gdi_count();
    U(EnumEnhMetaFile)(d.dc, h, objs_cb, info, &rc);
    CHECK(info[0] == 1);
    EXPECT_EQ(info[1], OBJ_PEN);
    CHECK(info[2] == 1);
    EXPECT_EQ(emf_gdi_count(), g0);
    emf_dib_free(&d); U(DeleteEnhMetaFile)(h);
}

TEST(emf_enum_no_leak) {
    REQUIRE(U(EnumEnhMetaFile));
    HENHMETAFILE h = mk_enum_scene();
    if (!h) { CHECK(h != NULL); return; }
    DWORD g0 = emf_gdi_count();
    for (int i = 0; i < 300; i++) { emf_log l; emf_enum(h, &l); }
    EXPECT_EQ(emf_gdi_count(), g0);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_enum_rect_variants) {
    REQUIRE(U(EnumEnhMetaFile));
    HENHMETAFILE h = mk_enum_scene();
    if (!h) { CHECK(h != NULL); return; }
    HDC dc = U(CreateCompatibleDC)(NULL);
    RECT rcs[] = { {0, 0, 100, 100}, {10, 10, 11, 11}, {-50, -50, 50, 50}, {0, 0, 100000, 100000} };
    for (unsigned i = 0; i < 4; i++) {
        emf_log l; memset(&l, 0, sizeof l);
        BOOL r = U(EnumEnhMetaFile)(dc, h, emf_log_cb, &l, &rcs[i]);
        CHECKF(r, "rect %u", i);
        CHECKF(l.n >= 8, "rect %u n=%d", i, l.n);
    }
    U(DeleteDC)(dc); U(DeleteEnhMetaFile)(h);
}

/* ---------- PlayEnhMetaFileRecord ---------- */

/* Slot 0 of a handle table holds the metafile itself (as EnumEnhMetaFile supplies it). */
static HENHMETAFILE ht_mf(void) {
    static HENHMETAFILE m;
    if (!m) m = mk_enum_scene();
    return m;
}

TEST(emf_playrec_setbkcolor_on_dib) {
    REQUIRE(U(PlayEnhMetaFileRecord));
    emf_dib d; if (!emf_dib_make(&d, 8, 8, 0)) { t_skip("no dib"); return; }
    EMRSETBKCOLOR r = { { EMR_SETBKCOLOR, sizeof r }, RGB(11, 22, 33) };
    HANDLETABLE ht[4]; memset(ht, 0, sizeof ht); ht[0].objectHandle[0] = (HGDIOBJ)ht_mf();
    CHECK(U(PlayEnhMetaFileRecord)(d.dc, ht, (ENHMETARECORD *)&r, 4));
    EXPECT_EQ(U(GetBkColor)(d.dc), RGB(11, 22, 33));
    emf_dib_free(&d);
}

TEST(emf_playrec_setmapmode_and_back) {
    REQUIRE(U(PlayEnhMetaFileRecord));
    emf_dib d; if (!emf_dib_make(&d, 8, 8, 0)) { t_skip("no dib"); return; }
    EMRSETMAPMODE r = { { EMR_SETMAPMODE, sizeof r }, MM_LOMETRIC };
    HANDLETABLE ht[2]; memset(ht, 0, sizeof ht); ht[0].objectHandle[0] = (HGDIOBJ)ht_mf();
    CHECK(U(PlayEnhMetaFileRecord)(d.dc, ht, (ENHMETARECORD *)&r, 2));
    EXPECT_EQ(U(GetMapMode)(d.dc), MM_LOMETRIC);
    r.iMode = MM_TEXT;
    CHECK(U(PlayEnhMetaFileRecord)(d.dc, ht, (ENHMETARECORD *)&r, 2));
    EXPECT_EQ(U(GetMapMode)(d.dc), MM_TEXT);
    emf_dib_free(&d);
}

TEST(emf_playrec_rectangle_draws) {
    REQUIRE(U(PlayEnhMetaFileRecord));
    emf_dib d; if (!emf_dib_make(&d, 32, 32, 0xFFFFFF)) { t_skip("no dib"); return; }
    HANDLETABLE ht[2]; memset(ht, 0, sizeof ht); ht[0].objectHandle[0] = (HGDIOBJ)ht_mf();
    EMRSELECTOBJECT sp = { { EMR_SELECTOBJECT, sizeof sp }, 0x80000000u | NULL_PEN };
    EMRSELECTOBJECT sb = { { EMR_SELECTOBJECT, sizeof sb }, 0x80000000u | BLACK_BRUSH };
    EMRRECTANGLE rr; memset(&rr, 0, sizeof rr);
    rr.emr.iType = EMR_RECTANGLE; rr.emr.nSize = sizeof rr; rr.rclBox.left = 4; rr.rclBox.top = 5; rr.rclBox.right = 20; rr.rclBox.bottom = 15;
    CHECK(U(PlayEnhMetaFileRecord)(d.dc, ht, (ENHMETARECORD *)&sp, 2));
    CHECK(U(PlayEnhMetaFileRecord)(d.dc, ht, (ENHMETARECORD *)&sb, 2));
    CHECK(U(PlayEnhMetaFileRecord)(d.dc, ht, (ENHMETARECORD *)&rr, 2));
    EXPECT_EQ(emf_px(&d, 10, 10), 0);
    EXPECT_EQ(emf_px(&d, 4, 5), 0);
    EXPECT_EQ(emf_px(&d, 2, 2), 0xFFFFFF);
    EXPECT_EQ(emf_px(&d, 25, 25), 0xFFFFFF);
    emf_dib_free(&d);
}

TEST(emf_playrec_select_stock_object) {
    REQUIRE(U(PlayEnhMetaFileRecord));
    emf_dib d; if (!emf_dib_make(&d, 8, 8, 0)) { t_skip("no dib"); return; }
    HANDLETABLE ht[2]; memset(ht, 0, sizeof ht); ht[0].objectHandle[0] = (HGDIOBJ)ht_mf();
    EMRSELECTOBJECT sp = { { EMR_SELECTOBJECT, sizeof sp }, 0x80000000u | WHITE_PEN };
    CHECK(U(PlayEnhMetaFileRecord)(d.dc, ht, (ENHMETARECORD *)&sp, 2));
    EXPECT_EQ(U(GetCurrentObject)(d.dc, OBJ_PEN), U(GetStockObject)(WHITE_PEN));
    emf_dib_free(&d);
}

TEST(emf_playrec_createpen_fills_table) {
    REQUIRE(U(PlayEnhMetaFileRecord));
    emf_dib d; if (!emf_dib_make(&d, 8, 8, 0)) { t_skip("no dib"); return; }
    ht_mf();
    HANDLETABLE ht[4]; memset(ht, 0, sizeof ht); ht[0].objectHandle[0] = (HGDIOBJ)ht_mf();
    EMRCREATEPEN cp; memset(&cp, 0, sizeof cp);
    cp.emr.iType = EMR_CREATEPEN; cp.emr.nSize = sizeof cp; cp.ihPen = 2;
    cp.lopn.lopnStyle = PS_SOLID; cp.lopn.lopnWidth.x = 3; cp.lopn.lopnColor = RGB(1, 2, 3);
    DWORD g0 = emf_gdi_count();
    CHECK(U(PlayEnhMetaFileRecord)(d.dc, ht, (ENHMETARECORD *)&cp, 4));
    CHECK(ht[2].objectHandle[0] != NULL);
    if (ht[2].objectHandle[0]) {
        LOGPEN lp; memset(&lp, 0, sizeof lp);
        CHECK(U(GetObjectW)(ht[2].objectHandle[0], sizeof lp, &lp) == sizeof lp);
        EXPECT_EQ(lp.lopnWidth.x, 3); EXPECT_EQ(lp.lopnColor, RGB(1, 2, 3));
        EMRSELECTOBJECT so = { { EMR_SELECTOBJECT, sizeof so }, 2 };
        CHECK(U(PlayEnhMetaFileRecord)(d.dc, ht, (ENHMETARECORD *)&so, 4));
        EXPECT_EQ(U(GetCurrentObject)(d.dc, OBJ_PEN), ht[2].objectHandle[0]);
        EMRSELECTOBJECT sp = { { EMR_SELECTOBJECT, sizeof sp }, 0x80000000u | BLACK_PEN };
        U(PlayEnhMetaFileRecord)(d.dc, ht, (ENHMETARECORD *)&sp, 4);
        EMRDELETEOBJECT del = { { EMR_DELETEOBJECT, sizeof del }, 2 };
        CHECK(U(PlayEnhMetaFileRecord)(d.dc, ht, (ENHMETARECORD *)&del, 4));
        CHECK(ht[2].objectHandle[0] == NULL);
    }
    EXPECT_EQ(emf_gdi_count(), g0);
    emf_dib_free(&d);
}

TEST(emf_playrec_createpen_index_out_of_range) {
    REQUIRE(U(PlayEnhMetaFileRecord));
    emf_dib d; if (!emf_dib_make(&d, 8, 8, 0)) { t_skip("no dib"); return; }
    HANDLETABLE ht[4]; memset(ht, 0, sizeof ht); ht[0].objectHandle[0] = (HGDIOBJ)ht_mf();
    EMRCREATEPEN cp; memset(&cp, 0, sizeof cp);
    cp.emr.iType = EMR_CREATEPEN; cp.emr.nSize = sizeof cp;
    cp.lopn.lopnStyle = PS_SOLID; cp.lopn.lopnWidth.x = 1;
    DWORD g0 = emf_gdi_count();
    static const DWORD idx[] = { 4, 5, 100, 0x7fffffff };
    for (unsigned i = 0; i < 4; i++) {
        cp.ihPen = idx[i];
        BOOL r = U(PlayEnhMetaFileRecord)(d.dc, ht, (ENHMETARECORD *)&cp, 4);
        SOFT(r == FALSE);
        t_note("ihPen=%lu cht=4 -> %d", idx[i], r);
    }
    for (int i = 1; i < 4; i++) CHECK(ht[i].objectHandle[0] == NULL);
    EXPECT_EQ(emf_gdi_count(), g0);
    emf_dib_free(&d);
}

TEST(emf_playrec_selectobject_index_unset) {
    REQUIRE(U(PlayEnhMetaFileRecord));
    emf_dib d; if (!emf_dib_make(&d, 8, 8, 0)) { t_skip("no dib"); return; }
    HANDLETABLE ht[4]; memset(ht, 0, sizeof ht); ht[0].objectHandle[0] = (HGDIOBJ)ht_mf();
    HGDIOBJ before = U(GetCurrentObject)(d.dc, OBJ_PEN);
    EMRSELECTOBJECT so = { { EMR_SELECTOBJECT, sizeof so }, 3 };
    BOOL r = U(PlayEnhMetaFileRecord)(d.dc, ht, (ENHMETARECORD *)&so, 4);
    t_note("select unset table slot -> %d", r);
    EXPECT_EQ(U(GetCurrentObject)(d.dc, OBJ_PEN), before);
    so.ihObject = 99;
    r = U(PlayEnhMetaFileRecord)(d.dc, ht, (ENHMETARECORD *)&so, 4);
    t_note("select out-of-range slot -> %d", r);
    EXPECT_EQ(U(GetCurrentObject)(d.dc, OBJ_PEN), before);
    emf_dib_free(&d);
}

TEST(emf_playrec_unknown_record_type) {
    REQUIRE(U(PlayEnhMetaFileRecord));
    emf_dib d; if (!emf_dib_make(&d, 8, 8, 0)) { t_skip("no dib"); return; }
    HANDLETABLE ht[2]; memset(ht, 0, sizeof ht); ht[0].objectHandle[0] = (HGDIOBJ)ht_mf();
    DWORD rec[4] = { 0x7b, 16, 0, 0 };
    BOOL r = U(PlayEnhMetaFileRecord)(d.dc, ht, (ENHMETARECORD *)rec, 2);
    t_note("unknown type 0x7b -> %d", r);
    rec[0] = 0xFFFFFFF0u;
    r = U(PlayEnhMetaFileRecord)(d.dc, ht, (ENHMETARECORD *)rec, 2);
    t_note("negative type -> %d", r);
    SOFT(r == TRUE);
    emf_dib_free(&d);
}

TEST(emf_playrec_on_invalid_dc) {
    REQUIRE(U(PlayEnhMetaFileRecord));
    HANDLETABLE ht[2]; memset(ht, 0, sizeof ht); ht[0].objectHandle[0] = (HGDIOBJ)ht_mf();
    EMRSETBKCOLOR r = { { EMR_SETBKCOLOR, sizeof r }, 5 };
    BOOL ok = U(PlayEnhMetaFileRecord)((HDC)0x1234, ht, (ENHMETARECORD *)&r, 2);
    t_note("PlayEnhMetaFileRecord on bogus DC = %d", ok);
    SOFT(ok == FALSE);
}

TEST(emf_playrec_into_emf_dc_rerecords) {
    REQUIRE(U(PlayEnhMetaFileRecord));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    HANDLETABLE ht[2]; memset(ht, 0, sizeof ht); ht[0].objectHandle[0] = (HGDIOBJ)ht_mf();
    EMRSETBKCOLOR r = { { EMR_SETBKCOLOR, sizeof r }, 0x00123456 };
    CHECK(U(PlayEnhMetaFileRecord)(dc, ht, (ENHMETARECORD *)&r, 2));
    HENHMETAFILE h = emf_end(dc);
    UINT n; BYTE *b = emf_bits(h, &n);
    const ENHMETARECORD *rec = b ? emf_find(b, n, EMR_SETBKCOLOR, 0) : NULL;
    CHECK(rec != NULL);
    if (rec) EXPECT_EQ(rec->dParm[0], 0x00123456);
    free(b); U(DeleteEnhMetaFile)(h);
}

TEST(emf_playrec_unknown_record_into_emf_dc_copied) {
    REQUIRE(U(PlayEnhMetaFileRecord));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    HANDLETABLE ht[2]; memset(ht, 0, sizeof ht); ht[0].objectHandle[0] = (HGDIOBJ)ht_mf();
    DWORD rec[4] = { 0x7b, 16, 0x11223344, 0x55667788 };
    BOOL ok = U(PlayEnhMetaFileRecord)(dc, ht, (ENHMETARECORD *)rec, 2);
    t_note("unknown record into emf dc -> %d", ok);
    HENHMETAFILE h = emf_end(dc);
    UINT n; BYTE *b = emf_bits(h, &n);
    const ENHMETARECORD *r2 = b ? emf_find(b, n, 0x7b, 0) : NULL;
    t_note("unknown record preserved=%d", r2 != NULL);
    if (r2) { EXPECT_EQ(r2->nSize, 16); EXPECT_EQ(r2->dParm[0], 0x11223344); }
    free(b); U(DeleteEnhMetaFile)(h);
}

/* ---------- PlayEnhMetaFile argument errors ---------- */

TEST(emf_play_null_dc) {
    REQUIRE(U(PlayEnhMetaFile));
    HENHMETAFILE h = mk_enum_scene();
    if (!h) { CHECK(h != NULL); return; }
    RECT rc = {0, 0, 10, 10};
    SetLastError(0);
    CHECK(U(PlayEnhMetaFile)(NULL, h, &rc) == FALSE);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_play_invalid_handles) {
    REQUIRE(U(PlayEnhMetaFile));
    emf_dib d; if (!emf_dib_make(&d, 8, 8, 0)) { t_skip("no dib"); return; }
    RECT rc = {0, 0, 8, 8};
    HENHMETAFILE bad[] = { NULL, (HENHMETAFILE)1, (HENHMETAFILE)0xdeadbeef };
    for (unsigned i = 0; i < 3; i++) CHECKF(U(PlayEnhMetaFile)(d.dc, bad[i], &rc) == FALSE, "case %u", i);
    emf_dib_free(&d);
}

TEST(emf_play_invalid_dc) {
    REQUIRE(U(PlayEnhMetaFile));
    HENHMETAFILE h = mk_enum_scene();
    if (!h) { CHECK(h != NULL); return; }
    RECT rc = {0, 0, 10, 10};
    CHECK(U(PlayEnhMetaFile)((HDC)0x1234, h, &rc) == FALSE);
    CHECK(U(PlayEnhMetaFile)((HDC)(ULONG_PTR)0xdeadbeef, h, &rc) == FALSE);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_play_deleted_handle) {
    REQUIRE(U(PlayEnhMetaFile));
    emf_dib d; if (!emf_dib_make(&d, 8, 8, 0)) { t_skip("no dib"); return; }
    HENHMETAFILE h = mk_enum_scene();
    U(DeleteEnhMetaFile)(h);
    RECT rc = {0, 0, 8, 8};
    CHECK(U(PlayEnhMetaFile)(d.dc, h, &rc) == FALSE);
    emf_dib_free(&d);
}

TEST(emf_play_into_emf_dc_embeds_records) {
    REQUIRE(U(PlayEnhMetaFile));
    HENHMETAFILE inner = mk_enum_scene();
    if (!inner) { CHECK(inner != NULL); return; }
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); U(DeleteEnhMetaFile)(inner); return; }
    RECT rc = {0, 0, 100, 100};
    CHECK(U(PlayEnhMetaFile)(dc, inner, &rc));
    HENHMETAFILE outer = emf_end(dc);
    CHECK(outer != NULL);
    if (outer) {
        emf_log l; emf_enum(outer, &l);
        t_note("outer records: %d", l.n);
        DWORD exp[] = { EMR_RECTANGLE, EMR_ELLIPSE };
        SOFT(emf_has_seq(&l, exp, 2));
        CHECK(l.n >= 2);
        U(DeleteEnhMetaFile)(outer);
    }
    U(DeleteEnhMetaFile)(inner);
}

TEST(emf_play_nested_two_levels) {
    REQUIRE(U(PlayEnhMetaFile));
    HENHMETAFILE a = mk_enum_scene();
    HDC dc = emf_begin();
    if (!a || !dc) { CHECK(a && dc); return; }
    RECT rc = {0, 0, 50, 50};
    U(PlayEnhMetaFile)(dc, a, &rc);
    HENHMETAFILE b = emf_end(dc);
    HDC dc2 = emf_begin();
    CHECK(dc2 != NULL);
    if (dc2 && b) {
        CHECK(U(PlayEnhMetaFile)(dc2, b, &rc));
        HENHMETAFILE c = emf_end(dc2);
        CHECK(c != NULL);
        emf_dib d;
        if (c && emf_dib_make(&d, 64, 64, 0xFFFFFF)) { CHECK(U(PlayEnhMetaFile)(d.dc, c, &rc)); emf_dib_free(&d); }
        if (c) U(DeleteEnhMetaFile)(c);
    }
    if (b) U(DeleteEnhMetaFile)(b);
    U(DeleteEnhMetaFile)(a);
}

TEST(emf_play_self_into_itself_impossible) {
    /* An in-progress DC has no HENHMETAFILE: playing a closed copy into a new DC and closing both is fine. */
    REQUIRE(U(PlayEnhMetaFile));
    HENHMETAFILE a = mk_enum_scene();
    if (!a) { CHECK(a != NULL); return; }
    for (int i = 0; i < 5; i++) {
        HDC dc = emf_begin(); RECT rc = {0, 0, 20, 20};
        U(PlayEnhMetaFile)(dc, a, &rc);
        HENHMETAFILE b = emf_end(dc);
        if (b) U(DeleteEnhMetaFile)(b);
    }
    U(DeleteEnhMetaFile)(a);
}

TEST(emf_play_leaves_dc_state_restored) {
    REQUIRE(U(PlayEnhMetaFile));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    HBRUSH b = U(CreateSolidBrush)(RGB(1, 2, 3)); HGDIOBJ o = U(SelectObject)(dc, b);
    U(SetBkMode)(dc, TRANSPARENT); U(SetROP2)(dc, R2_XORPEN); U(SetMapMode)(dc, MM_ANISOTROPIC);
    U(SelectObject)(dc, o); U(DeleteObject)(b);
    HENHMETAFILE h = emf_end(dc);
    emf_dib d;
    if (!emf_dib_make(&d, 16, 16, 0xFFFFFF)) { U(DeleteEnhMetaFile)(h); t_skip("no dib"); return; }
    RECT rc = {0, 0, 16, 16};
    HGDIOBJ br0 = U(GetCurrentObject)(d.dc, OBJ_BRUSH), pen0 = U(GetCurrentObject)(d.dc, OBJ_PEN);
    U(PlayEnhMetaFile)(d.dc, h, &rc);
    EXPECT_EQ(U(GetBkMode)(d.dc), OPAQUE);
    EXPECT_EQ(U(GetROP2)(d.dc), R2_COPYPEN);
    EXPECT_EQ(U(GetMapMode)(d.dc), MM_TEXT);
    EXPECT_EQ(U(GetCurrentObject)(d.dc, OBJ_BRUSH), br0);
    EXPECT_EQ(U(GetCurrentObject)(d.dc, OBJ_PEN), pen0);
    emf_dib_free(&d); U(DeleteEnhMetaFile)(h);
}

TEST(emf_play_no_leak) {
    REQUIRE(U(PlayEnhMetaFile));
    HENHMETAFILE h = mk_enum_scene();
    emf_dib d;
    if (!h || !emf_dib_make(&d, 32, 32, 0xFFFFFF)) { t_skip("setup"); return; }
    RECT rc = {0, 0, 32, 32};
    DWORD g0 = emf_gdi_count();
    for (int i = 0; i < 500; i++) U(PlayEnhMetaFile)(d.dc, h, &rc);
    EXPECT_EQ(emf_gdi_count(), g0);
    emf_dib_free(&d); U(DeleteEnhMetaFile)(h);
}
