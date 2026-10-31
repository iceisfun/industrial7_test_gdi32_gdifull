/* EnumMetaFile and PlayMetaFileRecord. */
#include "wmf.h"

/* ---- record construction ---- */
typedef struct { DWORD size; WORD func; WORD parm[40]; } REC;
static REC *mkrec(REC *r, WORD fn, int np, ...) {
    va_list ap; va_start(ap, np);
    memset(r, 0, sizeof *r);
    r->size = 3 + np; r->func = fn;
    for (int i = 0; i < np; i++) r->parm[i] = (WORD)va_arg(ap, int);
    va_end(ap);
    return r;
}
static BOOL play1(HDC dc, HANDLETABLE *ht, UINT n, REC *r) { return U(PlayMetaFileRecord)(dc, ht, (METARECORD *)r, n); }

typedef struct { HANDLETABLE *ht; } HT;
static HANDLETABLE *mkht(UINT n) { return calloc(n ? n : 1, sizeof(HGDIOBJ)); }

TEST(wmf_pmr_rectangle_pixels) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFileRecord));
    DIBDC a, b;
    REQUIRE(dibdc_make(&a, 40, 40)); REQUIRE(dibdc_make(&b, 40, 40));
    HANDLETABLE *ht = mkht(4);
    REC r; mkrec(&r, 0x041B, 4, 30, 25, 5, 3);   /* bottom, right, top, left */
    CHECK(play1(a.dc, ht, 4, &r));
    U(Rectangle)(b.dc, 3, 5, 25, 30);
    EXPECT_EQ(dibdc_diff(&a, &b), 0);
    CHECK(dibdc_nonwhite(&a) > 0);
    free(ht); dibdc_free(&a); dibdc_free(&b);
}

TEST(wmf_pmr_short_record_rejected) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFileRecord));
    DIBDC d; REQUIRE(dibdc_make(&d, 40, 40));
    HANDLETABLE *ht = mkht(4);
    REC r;
    for (int np = 0; np < 4; np++) {
        mkrec(&r, 0x041B, np, 30, 25, 5, 3);
        BOOL ok = play1(d.dc, ht, 4, &r);
        CHECKF(!ok, "rectangle with %d params accepted", np);
    }
    mkrec(&r, 0x041B, 4, 30, 25, 5, 3);
    CHECK(play1(d.dc, ht, 4, &r));
    free(ht); dibdc_free(&d);
}

typedef struct { const char *name; WORD fn; int minp; } MINP;
TEST(wmf_pmr_short_record_table) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFileRecord));
    static const MINP t[] = {
        { "SetMapMode", 0x0103, 1 }, { "SetBkMode", 0x0102, 1 }, { "SetROP2", 0x0104, 1 },
        { "SetPolyFillMode", 0x0106, 1 }, { "SetStretchBltMode", 0x0107, 1 },
        { "SetTextColor", 0x0209, 2 }, { "SetBkColor", 0x0201, 2 }, { "MoveTo", 0x0214, 2 }, { "LineTo", 0x0213, 2 },
        { "SetWindowOrg", 0x020B, 2 }, { "SetWindowExt", 0x020C, 2 }, { "SetViewportOrg", 0x020D, 2 }, { "SetViewportExt", 0x020E, 2 },
        { "Ellipse", 0x0418, 4 }, { "Rectangle", 0x041B, 4 }, { "RoundRect", 0x061C, 6 }, { "Arc", 0x0817, 8 }, { "Pie", 0x081A, 8 },
        { "Chord", 0x0830, 8 }, { "PatBlt", 0x061D, 6 }, { "SetPixel", 0x041F, 4 },
        { "IntersectClipRect", 0x0416, 4 }, { "ExcludeClipRect", 0x0415, 4 }, { "OffsetClipRgn", 0x0220, 2 },
        { "Polygon", 0x0324, 1 }, { "Polyline", 0x0325, 1 }, { "SelectObject", 0x012D, 1 }, { "DeleteObject", 0x01F0, 1 },
    };
    DIBDC d; REQUIRE(dibdc_make(&d, 40, 40));
    HANDLETABLE *ht = mkht(4);
    for (size_t i = 0; i < sizeof t / sizeof *t; i++) {
        REC r; mkrec(&r, t[i].fn, t[i].minp - 1, 5, 6, 7, 8, 9, 10, 11, 12);
        BOOL ok;
        if (t[i].minp >= 4) { ok = play1(d.dc, ht, 4, &r); CHECKF(!ok, "%s with %d params accepted", t[i].name, t[i].minp - 1); }
        mkrec(&r, t[i].fn, 0);
        ok = play1(d.dc, ht, 4, &r);
        CHECKF(!ok, "%s with zero params accepted", t[i].name);
    }
    free(ht); dibdc_free(&d);
}

TEST(wmf_pmr_unknown_and_eof) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFileRecord));
    DIBDC d; REQUIRE(dibdc_make(&d, 20, 20));
    HANDLETABLE *ht = mkht(2);
    REC r;
    mkrec(&r, 0x0000, 0);
    CHECK(play1(d.dc, ht, 2, &r));
    mkrec(&r, 0x0005, 0);
    CHECK(play1(d.dc, ht, 2, &r));
    mkrec(&r, 0x0105, 1, 1);
    CHECK(play1(d.dc, ht, 2, &r));
    CHECK(dibdc_nonwhite(&d) == 0);
    free(ht); dibdc_free(&d);
}

TEST(wmf_pmr_null_dc) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFileRecord));
    HANDLETABLE *ht = mkht(2);
    REC r; mkrec(&r, 0x041B, 4, 30, 25, 5, 3);
    CHECK(!play1(NULL, ht, 2, &r));
    mkrec(&r, 0x0103, 1, 1);
    CHECK(!play1(NULL, ht, 2, &r));
    free(ht);
}

TEST(wmf_pmr_bogus_dc) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFileRecord));
    HANDLETABLE *ht = mkht(2);
    REC r; mkrec(&r, 0x041B, 4, 30, 25, 5, 3);
    CHECK(!play1((HDC)(ULONG_PTR)0xdeadbeef, ht, 2, &r));
    CHECK(!play1((HDC)(ULONG_PTR)1, ht, 2, &r));
    free(ht);
}

typedef struct { const char *name; WORD fn; int np; int p[4]; int (*get)(HDC); int want; } STATE_CASE;
static int g_bk(HDC d) { return U(GetBkMode)(d); }
static int g_rop(HDC d) { return U(GetROP2)(d); }
static int g_poly(HDC d) { return U(GetPolyFillMode)(d); }
static int g_stretch(HDC d) { return U(GetStretchBltMode)(d); }
static int g_map(HDC d) { return U(GetMapMode)(d); }
static int g_align(HDC d) { return (int)U(GetTextAlign)(d); }
static int g_tc(HDC d) { return (int)U(GetTextColor)(d); }
static int g_bc(HDC d) { return (int)U(GetBkColor)(d); }
static int g_cx(HDC d) { return U(GetTextCharacterExtra)(d); }
static int g_wox(HDC d) { POINT p; U(GetWindowOrgEx)(d, &p); return p.x; }
static int g_woy(HDC d) { POINT p; U(GetWindowOrgEx)(d, &p); return p.y; }
static int g_vox(HDC d) { POINT p; U(GetViewportOrgEx)(d, &p); return p.x; }
static int g_voy(HDC d) { POINT p; U(GetViewportOrgEx)(d, &p); return p.y; }
static int g_wex(HDC d) { SIZE s; U(GetWindowExtEx)(d, &s); return s.cx; }
static int g_wey(HDC d) { SIZE s; U(GetWindowExtEx)(d, &s); return s.cy; }
static int g_vex(HDC d) { SIZE s; U(GetViewportExtEx)(d, &s); return s.cx; }
static int g_vey(HDC d) { SIZE s; U(GetViewportExtEx)(d, &s); return s.cy; }
static int g_cpx(HDC d) { POINT p; U(GetCurrentPositionEx)(d, &p); return p.x; }
static int g_cpy(HDC d) { POINT p; U(GetCurrentPositionEx)(d, &p); return p.y; }

TEST(wmf_pmr_state_records) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFileRecord));
    static const STATE_CASE t[] = {
        { "bkmode", 0x0102, 1, { 1 }, g_bk, TRANSPARENT },
        { "bkmode2", 0x0102, 2, { 2, 0 }, g_bk, OPAQUE },
        { "rop2", 0x0104, 1, { R2_XORPEN }, g_rop, R2_XORPEN },
        { "rop2b", 0x0104, 2, { R2_NOT, 0 }, g_rop, R2_NOT },
        { "polyfill", 0x0106, 1, { WINDING }, g_poly, WINDING },
        { "stretch", 0x0107, 1, { HALFTONE }, g_stretch, HALFTONE },
        { "stretch2", 0x0107, 1, { COLORONCOLOR }, g_stretch, COLORONCOLOR },
        { "mapmode", 0x0103, 1, { MM_HIMETRIC }, g_map, MM_HIMETRIC },
        { "mapmode2", 0x0103, 1, { MM_TWIPS }, g_map, MM_TWIPS },
        { "mapmode_aniso", 0x0103, 1, { MM_ANISOTROPIC }, g_map, MM_ANISOTROPIC },
        { "textalign", 0x012E, 2, { TA_BASELINE | TA_CENTER, 0 }, g_align, TA_BASELINE | TA_CENTER },
        { "textcolor", 0x0209, 2, { 0x3412, 0x0078 }, g_tc, 0x783412 },
        { "bkcolor", 0x0201, 2, { 0x6655, 0x0044 }, g_bc, 0x446655 },
        { "textcharextra", 0x0108, 1, { 3 }, g_cx, 3 },
        { "textcharextra_neg", 0x0108, 1, { 0xFFFE }, g_cx, -2 },
        { "windoworg_x", 0x020B, 2, { 20, 10 }, g_wox, 10 },
        { "windoworg_y", 0x020B, 2, { 20, 10 }, g_woy, 20 },
        { "windoworg_negx", 0x020B, 2, { 0xFFF6, 0xFFFB }, g_wox, -5 },
        { "windoworg_negy", 0x020B, 2, { 0xFFF6, 0xFFFB }, g_woy, -10 },
        { "viewportorg_x", 0x020D, 2, { 40, 30 }, g_vox, 30 },
        { "viewportorg_y", 0x020D, 2, { 40, 30 }, g_voy, 40 },
        { "moveto_x", 0x0214, 2, { 20, 10 }, g_cpx, 10 },
        { "moveto_y", 0x0214, 2, { 20, 10 }, g_cpy, 20 },
        { "lineto_x", 0x0213, 2, { 20, 10 }, g_cpx, 10 },
        { "lineto_y", 0x0213, 2, { 20, 10 }, g_cpy, 20 },
        { "moveto_neg", 0x0214, 2, { 0xFFFF, 0xFFFE }, g_cpx, -2 },
    };
    for (size_t i = 0; i < sizeof t / sizeof *t; i++) {
        DIBDC d; REQUIRE(dibdc_make(&d, 40, 40));
        HANDLETABLE *ht = mkht(2);
        REC r;
        mkrec(&r, t[i].fn, t[i].np, t[i].p[0], t[i].p[1], t[i].p[2], t[i].p[3]);
        BOOL ok = play1(d.dc, ht, 2, &r);
        CHECKF(ok, "%s: play failed", t[i].name);
        int got = t[i].get(d.dc);
        CHECKF(got == t[i].want, "%s: state %d want %d", t[i].name, got, t[i].want);
        free(ht); dibdc_free(&d);
    }
}

TEST(wmf_pmr_extents) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFileRecord));
    DIBDC d; REQUIRE(dibdc_make(&d, 40, 40));
    HANDLETABLE *ht = mkht(2);
    REC r;
    mkrec(&r, 0x0103, 1, MM_ANISOTROPIC); CHECK(play1(d.dc, ht, 2, &r));
    mkrec(&r, 0x020C, 2, 200, 100); CHECK(play1(d.dc, ht, 2, &r));
    mkrec(&r, 0x020E, 2, 400, 300); CHECK(play1(d.dc, ht, 2, &r));
    EXPECT_EQ(g_wex(d.dc), 100); EXPECT_EQ(g_wey(d.dc), 200);
    EXPECT_EQ(g_vex(d.dc), 300); EXPECT_EQ(g_vey(d.dc), 400);
    free(ht); dibdc_free(&d);
}

TEST(wmf_pmr_object_create_select_delete) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFileRecord));
    DIBDC d; REQUIRE(dibdc_make(&d, 40, 40));
    HANDLETABLE *ht = mkht(4);
    REC r;
    mkrec(&r, 0x02FA, 5, PS_SOLID, 1, 0, 0x00FF, 0x0000);   /* red pen */
    CHECK(play1(d.dc, ht, 4, &r));
    CHECK(ht->objectHandle[0] != NULL);
    if (ht->objectHandle[0]) EXPECT_EQ(U(GetObjectType)(ht->objectHandle[0]), OBJ_PEN);
    mkrec(&r, 0x02FC, 4, BS_SOLID, 0xFF00, 0x0000, 0);        /* green brush */
    CHECK(play1(d.dc, ht, 4, &r));
    CHECK(ht->objectHandle[1] != NULL);
    if (ht->objectHandle[1]) EXPECT_EQ(U(GetObjectType)(ht->objectHandle[1]), OBJ_BRUSH);
    CHECK(ht->objectHandle[0] != ht->objectHandle[1]);
    mkrec(&r, 0x012D, 1, 0); CHECK(play1(d.dc, ht, 4, &r));
    mkrec(&r, 0x012D, 1, 1); CHECK(play1(d.dc, ht, 4, &r));
    EXPECT_EQ(U(GetCurrentObject)(d.dc, OBJ_PEN), ht->objectHandle[0]);
    EXPECT_EQ(U(GetCurrentObject)(d.dc, OBJ_BRUSH), ht->objectHandle[1]);
    mkrec(&r, 0x041B, 4, 20, 20, 5, 5); CHECK(play1(d.dc, ht, 4, &r));
    EXPECT_EQ(DIBPX(&d, 5, 5), 0xFF0000);
    EXPECT_EQ(DIBPX(&d, 10, 10), 0x00FF00);
    /* deleting a selected object fails; deselect first */
    HGDIOBJ p0 = ht->objectHandle[0];
    U(SelectObject)(d.dc, U(GetStockObject)(BLACK_PEN));
    U(SelectObject)(d.dc, U(GetStockObject)(WHITE_BRUSH));
    mkrec(&r, 0x01F0, 1, 0); CHECK(play1(d.dc, ht, 4, &r));
    CHECK(ht->objectHandle[0] == NULL);
    EXPECT_EQ(U(GetObjectType)(p0), 0);
    mkrec(&r, 0x01F0, 1, 1); CHECK(play1(d.dc, ht, 4, &r));
    CHECK(ht->objectHandle[1] == NULL);
    free(ht); dibdc_free(&d);
}

TEST(wmf_pmr_delete_invalid_slot) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFileRecord));
    if (on_wine()) { t_skip("wine faults on out-of-range slot"); return; }
    DIBDC d; REQUIRE(dibdc_make(&d, 10, 10));
    HANDLETABLE *ht = mkht(4);
    REC r;
    mkrec(&r, 0x01F0, 1, 0); CHECK(!play1(d.dc, ht, 4, &r));     /* empty slot */
    mkrec(&r, 0x01F0, 1, 3); CHECK(!play1(d.dc, ht, 4, &r));
    mkrec(&r, 0x01F0, 1, 4); CHECK(!play1(d.dc, ht, 4, &r));     /* index == noObjs */
    mkrec(&r, 0x01F0, 1, 0xFFFF); CHECK(!play1(d.dc, ht, 4, &r));
    mkrec(&r, 0x01F0, 1, 0); CHECK(!play1(d.dc, ht, 0, &r));     /* noObjs == 0 */
    free(ht); dibdc_free(&d);
}

TEST(wmf_pmr_create_fills_first_free_slot) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFileRecord));
    DIBDC d; REQUIRE(dibdc_make(&d, 10, 10));
    HANDLETABLE *ht = mkht(4);
    REC r;
    for (int i = 0; i < 3; i++) { mkrec(&r, 0x02FA, 5, PS_SOLID, 1, 0, i, 0); CHECK(play1(d.dc, ht, 4, &r)); }
    CHECK(ht->objectHandle[0] && ht->objectHandle[1] && ht->objectHandle[2] && !ht->objectHandle[3]);
    mkrec(&r, 0x01F0, 1, 1); CHECK(play1(d.dc, ht, 4, &r));
    CHECK(ht->objectHandle[1] == NULL);
    mkrec(&r, 0x02FC, 4, BS_SOLID, 1, 0, 0); CHECK(play1(d.dc, ht, 4, &r));
    CHECK(ht->objectHandle[1] != NULL);       /* freed slot reused */
    CHECK(ht->objectHandle[3] == NULL);
    for (int i = 0; i < 4; i++) if (ht->objectHandle[i]) U(DeleteObject)(ht->objectHandle[i]);
    free(ht); dibdc_free(&d);
}

TEST(wmf_pmr_create_table_full) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFileRecord)); REQUIRE(U(GetGuiResources));
    DIBDC d; REQUIRE(dibdc_make(&d, 10, 10));
    HANDLETABLE *ht = mkht(2);
    REC r;
    DWORD base = gdi_count();
    mkrec(&r, 0x02FA, 5, PS_SOLID, 1, 0, 1, 0); CHECK(play1(d.dc, ht, 2, &r));
    mkrec(&r, 0x02FA, 5, PS_SOLID, 1, 0, 2, 0); CHECK(play1(d.dc, ht, 2, &r));
    mkrec(&r, 0x02FA, 5, PS_SOLID, 1, 0, 3, 0);
    BOOL ok = play1(d.dc, ht, 2, &r);
    t_note("create into full table = %d, objects created: %lu", ok, gdi_count() - base);
    for (int i = 0; i < 2; i++) if (ht->objectHandle[i]) U(DeleteObject)(ht->objectHandle[i]);
    EXPECT_EQ(gdi_count(), base);
    free(ht); dibdc_free(&d);
}

TEST(wmf_pmr_select_bad_index) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFileRecord));
    DIBDC d; REQUIRE(dibdc_make(&d, 10, 10));
    HANDLETABLE *ht = mkht(4);
    REC r;
    mkrec(&r, 0x012D, 1, 0);
    BOOL ok = play1(d.dc, ht, 4, &r);
    t_note("select empty slot = %d", ok);
    mkrec(&r, 0x012D, 1, 100);
    ok = play1(d.dc, ht, 4, &r);
    t_note("select index beyond table = %d", ok);
    free(ht); dibdc_free(&d);
}

TEST(wmf_pmr_into_metafile_dc) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFileRecord));
    HDC m = mf_new();
    REQUIRE(m);
    HANDLETABLE *ht = mkht(2);
    REC r;
    mkrec(&r, 0x041B, 4, 30, 25, 5, 3);
    CHECK(play1(m, ht, 2, &r));
    mkrec(&r, 0x0214, 2, 7, 8);
    CHECK(play1(m, ht, 2, &r));
    WB b;
    REQUIRE(mf_finish(m, &b, NULL));
    EXPECT_EQ(wb_count(&b, 0x041B), 1);
    EXPECT_EQ(wb_count(&b, 0x0214), 1);
    const BYTE *q = wb_find(&b, 0x041B, 0);
    if (q) { EXPECT_EQ(r_parm(q, 0), 30); EXPECT_EQ(r_parm(q, 3), 3); }
    wb_free(&b);
    free(ht);
}

TEST(wmf_pmr_savedc_restoredc) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFileRecord));
    DIBDC d; REQUIRE(dibdc_make(&d, 10, 10));
    HANDLETABLE *ht = mkht(2);
    REC r;
    mkrec(&r, 0x0102, 1, TRANSPARENT); CHECK(play1(d.dc, ht, 2, &r));
    mkrec(&r, 0x001E, 0); CHECK(play1(d.dc, ht, 2, &r));
    mkrec(&r, 0x0102, 1, OPAQUE); CHECK(play1(d.dc, ht, 2, &r));
    EXPECT_EQ(U(GetBkMode)(d.dc), OPAQUE);
    mkrec(&r, 0x0127, 1, 0xFFFF); CHECK(play1(d.dc, ht, 2, &r));
    EXPECT_EQ(U(GetBkMode)(d.dc), TRANSPARENT);
    free(ht); dibdc_free(&d);
}

TEST(wmf_pmr_polygon_records) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFileRecord));
    DIBDC a, b; REQUIRE(dibdc_make(&a, 50, 50)); REQUIRE(dibdc_make(&b, 50, 50));
    HANDLETABLE *ht = mkht(2);
    REC r; mkrec(&r, 0x0324, 7, 3, 5, 5, 40, 10, 20, 45);
    CHECK(play1(a.dc, ht, 2, &r));
    POINT p[3] = { {5, 5}, {40, 10}, {20, 45} };
    U(Polygon)(b.dc, p, 3);
    EXPECT_EQ(dibdc_diff(&a, &b), 0);
    CHECK(dibdc_nonwhite(&a) > 0);
    mkrec(&r, 0x0325, 7, 3, 1, 1, 30, 2, 40, 49);
    dibdc_clear(&a, 0xFFFFFF); dibdc_clear(&b, 0xFFFFFF);
    CHECK(play1(a.dc, ht, 2, &r));
    POINT q[3] = { {1, 1}, {30, 2}, {40, 49} };
    U(Polyline)(b.dc, q, 3);
    EXPECT_EQ(dibdc_diff(&a, &b), 0);
    free(ht); dibdc_free(&a); dibdc_free(&b);
}

TEST(wmf_pmr_polygon_count_exceeds_record) {
    WMF_REQ_BASE(); REQUIRE(U(PlayMetaFileRecord));
    DIBDC d; REQUIRE(dibdc_make(&d, 50, 50));
    HANDLETABLE *ht = mkht(2);
    REC r; mkrec(&r, 0x0324, 7, 30, 5, 5, 40, 10, 20, 45);   /* claims 30 points, supplies 3 */
    BOOL ok = play1(d.dc, ht, 2, &r);
    t_note("polygon with oversized count = %d", ok);
    free(ht); dibdc_free(&d);
}

/* ---- EnumMetaFile ---- */
typedef struct {
    int n; WORD funcs[512]; DWORD sizes[512]; UINT nobj; LPARAM lp; int stop_at; int retval;
    HANDLETABLE *ht_first; int saw_ht; HDC dc; int play; int eof_seen;
} ENUMCTX;

static int CALLBACK enum_cb(HDC dc, HANDLETABLE *ht, METARECORD *mr, int nobj, LPARAM lp) {
    ENUMCTX *c = (ENUMCTX *)lp;
    if (c->n < 512) { c->funcs[c->n] = mr->rdFunction; c->sizes[c->n] = mr->rdSize; }
    if (mr->rdFunction == 0) c->eof_seen = 1;
    c->n++;
    c->nobj = nobj;
    if (ht) { c->saw_ht++; if (!c->ht_first) c->ht_first = ht; }
    if (c->play && c->dc) U(PlayMetaFileRecord)(c->dc, ht, mr, nobj);
    if (c->stop_at && c->n >= c->stop_at) return 0;
    return c->retval ? c->retval : 1;
}

static HMETAFILE mk_enum_sample(WB *b) {
    HDC dc = mf_new();
    if (!dc) return NULL;
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(255, 0, 0));
    U(SelectObject)(dc, p);
    U(MoveToEx)(dc, 1, 1, NULL);
    U(LineTo)(dc, 20, 20);
    U(Rectangle)(dc, 3, 3, 30, 30);
    U(SelectObject)(dc, U(GetStockObject)(BLACK_PEN));
    U(DeleteObject)(p);
    U(Ellipse)(dc, 5, 5, 25, 25);
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (h && b) wb_get(h, b);
    return h;
}

TEST(wmf_enum_null_proc) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile));
    if (on_wine()) { t_skip("wine calls a NULL callback"); return; }
    HMETAFILE h = mk_enum_sample(NULL);
    REQUIRE(h);
    DIBDC d; REQUIRE(dibdc_make(&d, 10, 10));
    SET_ERR(0);
    BOOL r = U(EnumMetaFile)(d.dc, h, NULL, 0);
    CHECK(!r);
    EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    r = U(EnumMetaFile)(NULL, h, NULL, 0);
    CHECK(!r);
    EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    r = U(EnumMetaFile)(d.dc, NULL, NULL, 0);
    CHECK(!r);
    EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    dibdc_free(&d);
    U(DeleteMetaFile)(h);
}

TEST(wmf_enum_invalid_metafile) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile));
    DIBDC d; REQUIRE(dibdc_make(&d, 10, 10));
    ENUMCTX c; memset(&c, 0, sizeof c);
    HMETAFILE bad[] = { NULL, (HMETAFILE)1, (HMETAFILE)(ULONG_PTR)0xdeadbeef };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        BOOL r = U(EnumMetaFile)(d.dc, bad[i], enum_cb, (LPARAM)&c);
        CHECK(!r);
        EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    }
    EXPECT_EQ(c.n, 0);
    dibdc_free(&d);
}

TEST(wmf_enum_invalid_dc) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile));
    HMETAFILE h = mk_enum_sample(NULL);
    REQUIRE(h);
    ENUMCTX c; memset(&c, 0, sizeof c);
    HDC bad[] = { (HDC)1, (HDC)(ULONG_PTR)0xdeadbeef, (HDC)(ULONG_PTR)0x00010001 };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        BOOL r = U(EnumMetaFile)(bad[i], h, enum_cb, (LPARAM)&c);
        CHECKF(!r, "bad dc %p", bad[i]);
    }
    EXPECT_EQ(c.n, 0);
    U(DeleteMetaFile)(h);
}

TEST(wmf_enum_null_dc_allowed) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile));
    WB b;
    HMETAFILE h = mk_enum_sample(&b);
    REQUIRE(h);
    ENUMCTX c; memset(&c, 0, sizeof c);
    BOOL r = U(EnumMetaFile)(NULL, h, enum_cb, (LPARAM)&c);
    CHECK(r);
    CHECK(c.n > 0);
    wb_free(&b);
    U(DeleteMetaFile)(h);
}

TEST(wmf_enum_sequence_matches_records) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile));
    WB b;
    HMETAFILE h = mk_enum_sample(&b);
    REQUIRE(h);
    DIBDC d; REQUIRE(dibdc_make(&d, 40, 40));
    ENUMCTX c; memset(&c, 0, sizeof c);
    BOOL r = U(EnumMetaFile)(d.dc, h, enum_cb, (LPARAM)&c);
    CHECK(r);
    int total = wb_nrecs(&b);
    EXPECT_EQ(c.n, total - 1);          /* EOF is not delivered */
    CHECK(!c.eof_seen);
    const BYTE *q = wb_first(&b);
    for (int i = 0; i < c.n && q; i++, q = wb_next(&b, q)) {
        CHECKF(c.funcs[i] == r_func(q), "record %d func %04x want %04x", i, c.funcs[i], r_func(q));
        CHECKF(c.sizes[i] == r_size(q), "record %d size", i);
    }
    EXPECT_EQ(c.nobj, wb_hdr(&b)->nobj);
    CHECK(c.saw_ht == c.n);
    wb_free(&b); dibdc_free(&d);
    U(DeleteMetaFile)(h);
}

TEST(wmf_enum_record_content) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile));
    WB b;
    HMETAFILE h = mk_enum_sample(&b);
    REQUIRE(h);
    typedef struct { int n; int bad; WB *b; } CC;
    CC cc = { 0, 0, &b };
    int CALLBACK cb(HDC dc, HANDLETABLE *ht, METARECORD *mr, int nobj, LPARAM lp) {
        CC *c = (CC *)lp;
        const BYTE *q = wb_first(c->b);
        for (int i = 0; i < c->n && q; i++) q = wb_next(c->b, q);
        if (!q || memcmp(q, mr, mr->rdSize * 2)) c->bad++;
        c->n++;
        return 1;
    }
    CHECK(U(EnumMetaFile)(NULL, h, cb, (LPARAM)&cc));
    EXPECT_EQ(cc.bad, 0);
    CHECK(cc.n > 0);
    wb_free(&b);
    U(DeleteMetaFile)(h);
}

TEST(wmf_enum_lparam_passthrough) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile));
    HMETAFILE h = mk_enum_sample(NULL);
    REQUIRE(h);
    LPARAM vals[] = { 0, 1, (LPARAM)-1, (LPARAM)0x123456789ABCDEF0ull };
    for (size_t i = 0; i < 4; i++) {
        int bad = 0, n = 0;
        static LPARAM want; static int *pb, *pn;
        want = vals[i]; pb = &bad; pn = &n;
        int CALLBACK cb(HDC dc, HANDLETABLE *ht, METARECORD *mr, int nobj, LPARAM lp) { (*pn)++; if (lp != want) (*pb)++; return 1; }
        CHECK(U(EnumMetaFile)(NULL, h, cb, vals[i]));
        EXPECT_EQ(bad, 0);
        CHECK(n > 0);
    }
    U(DeleteMetaFile)(h);
}

TEST(wmf_enum_dc_passthrough) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile));
    HMETAFILE h = mk_enum_sample(NULL);
    REQUIRE(h);
    DIBDC d; REQUIRE(dibdc_make(&d, 10, 10));
    static HDC want; static int bad;
    want = d.dc; bad = 0;
    int CALLBACK cb(HDC dc, HANDLETABLE *ht, METARECORD *mr, int nobj, LPARAM lp) { if (dc != want) bad++; return 1; }
    CHECK(U(EnumMetaFile)(d.dc, h, cb, 0));
    EXPECT_EQ(bad, 0);
    bad = 0; want = NULL;
    CHECK(U(EnumMetaFile)(NULL, h, cb, 0));
    EXPECT_EQ(bad, 0);
    dibdc_free(&d);
    U(DeleteMetaFile)(h);
}

TEST(wmf_enum_early_stop) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile));
    HMETAFILE h = mk_enum_sample(NULL);
    REQUIRE(h);
    DIBDC d; REQUIRE(dibdc_make(&d, 40, 40));
    for (int stop = 1; stop <= 4; stop++) {
        ENUMCTX c; memset(&c, 0, sizeof c);
        c.stop_at = stop;
        BOOL r = U(EnumMetaFile)(d.dc, h, enum_cb, (LPARAM)&c);
        CHECKF(!r, "stop %d returned %d", stop, r);
        EXPECT_EQ(c.n, stop);
    }
    dibdc_free(&d);
    U(DeleteMetaFile)(h);
}

TEST(wmf_enum_restart_after_stop) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile));
    HMETAFILE h = mk_enum_sample(NULL);
    REQUIRE(h);
    ENUMCTX a, b, c;
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b); memset(&c, 0, sizeof c);
    b.stop_at = 2;
    U(EnumMetaFile)(NULL, h, enum_cb, (LPARAM)&a);
    U(EnumMetaFile)(NULL, h, enum_cb, (LPARAM)&b);
    U(EnumMetaFile)(NULL, h, enum_cb, (LPARAM)&c);
    EXPECT_EQ(a.n, c.n);
    CHECK(a.n > 2);
    if (a.n == c.n) CHECK(!memcmp(a.funcs, c.funcs, a.n * sizeof(WORD)));
    U(DeleteMetaFile)(h);
}

TEST(wmf_enum_return_value) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile));
    HMETAFILE h = mk_enum_sample(NULL);
    REQUIRE(h);
    ENUMCTX c; memset(&c, 0, sizeof c);
    c.retval = 7;
    BOOL r = U(EnumMetaFile)(NULL, h, enum_cb, (LPARAM)&c);
    CHECK(r != 0);
    t_note("EnumMetaFile with callback returning 7 = %d", r);
    U(DeleteMetaFile)(h);
}

TEST(wmf_enum_empty_metafile) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile));
    HDC m = mf_new();
    REQUIRE(m);
    HMETAFILE h = U(CloseMetaFile)(m);
    REQUIRE(h);
    ENUMCTX c; memset(&c, 0, sizeof c);
    BOOL r = U(EnumMetaFile)(NULL, h, enum_cb, (LPARAM)&c);
    CHECK(r);
    EXPECT_EQ(c.n, 0);
    U(DeleteMetaFile)(h);
}

TEST(wmf_enum_header_only_no_eof) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile)); REQUIRE(U(SetMetaFileBitsEx));
    WMFHDR hd = { 1, 9, 0x300, 9, 0, 0, 0 };
    HMETAFILE h = U(SetMetaFileBitsEx)(18, (BYTE *)&hd);
    if (!h) { t_note("header-only metafile rejected"); return; }
    ENUMCTX c; memset(&c, 0, sizeof c);
    BOOL r = U(EnumMetaFile)(NULL, h, enum_cb, (LPARAM)&c);
    CHECK(!r);
    EXPECT_EQ(c.n, 0);
    U(DeleteMetaFile)(h);
}

TEST(wmf_enum_missing_eof_record) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile)); REQUIRE(U(SetMetaFileBitsEx));
    WB b;
    HMETAFILE h = mk_enum_sample(&b);
    REQUIRE(h);
    U(DeleteMetaFile)(h);
    HMETAFILE h2 = U(SetMetaFileBitsEx)(b.n - 6, b.p);    /* drop EOF */
    if (!h2) { t_note("metafile without EOF rejected"); wb_free(&b); return; }
    ENUMCTX c; memset(&c, 0, sizeof c);
    BOOL r = U(EnumMetaFile)(NULL, h2, enum_cb, (LPARAM)&c);
    t_note("no-EOF enumeration: result %d, %d callbacks", r, c.n);
    CHECK(!r);
    U(DeleteMetaFile)(h2);
    wb_free(&b);
}

TEST(wmf_enum_truncated_last_record) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile)); REQUIRE(U(SetMetaFileBitsEx));
    WB b;
    HMETAFILE h = mk_enum_sample(&b);
    REQUIRE(h);
    U(DeleteMetaFile)(h);
    HMETAFILE h2 = U(SetMetaFileBitsEx)(b.n - 8, b.p);    /* cuts into the last record before EOF */
    if (!h2) { wb_free(&b); return; }
    ENUMCTX c; memset(&c, 0, sizeof c);
    BOOL r = U(EnumMetaFile)(NULL, h2, enum_cb, (LPARAM)&c);
    CHECK(!r);
    U(DeleteMetaFile)(h2);
    wb_free(&b);
}

TEST(wmf_enum_zero_size_record) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile)); REQUIRE(U(SetMetaFileBitsEx));
    if (on_wine()) { t_skip("wine faults on zero-size record"); return; }
    WB b;
    HMETAFILE h = mk_enum_sample(&b);
    REQUIRE(h);
    U(DeleteMetaFile)(h);
    memset(b.p + 18, 0, 4);   /* first record size = 0 */
    HMETAFILE h2 = U(SetMetaFileBitsEx)(b.n, b.p);
    if (!h2) { wb_free(&b); return; }
    ENUMCTX c; memset(&c, 0, sizeof c);
    BOOL r = U(EnumMetaFile)(NULL, h2, enum_cb, (LPARAM)&c);
    CHECK(!r);
    EXPECT_EQ(c.n, 0);
    U(DeleteMetaFile)(h2);
    wb_free(&b);
}

TEST(wmf_enum_replay_via_pmr_equals_play) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile)); REQUIRE(U(PlayMetaFile));
    HMETAFILE h = mk_enum_sample(NULL);
    REQUIRE(h);
    DIBDC a, b; REQUIRE(dibdc_make(&a, 40, 40)); REQUIRE(dibdc_make(&b, 40, 40));
    CHECK(U(PlayMetaFile)(a.dc, h));
    ENUMCTX c; memset(&c, 0, sizeof c);
    c.play = 1; c.dc = b.dc;
    CHECK(U(EnumMetaFile)(b.dc, h, enum_cb, (LPARAM)&c));
    EXPECT_EQ(dibdc_diff(&a, &b), 0);
    CHECK(dibdc_nonwhite(&a) > 0);
    dibdc_free(&a); dibdc_free(&b);
    U(DeleteMetaFile)(h);
}

TEST(wmf_enum_cleans_handle_table) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile)); REQUIRE(U(GetGuiResources));
    /* objects created through PlayMetaFileRecord and never deleted by a record are freed at the end */
    HDC m = mf_new();
    REQUIRE(m);
    HPEN pens[5];
    for (int i = 0; i < 5; i++) { pens[i] = U(CreatePen)(PS_SOLID, 1, i); U(SelectObject)(m, pens[i]); }
    HMETAFILE h = U(CloseMetaFile)(m);
    for (int i = 0; i < 5; i++) U(DeleteObject)(pens[i]);
    REQUIRE(h);
    DIBDC d; REQUIRE(dibdc_make(&d, 10, 10));
    DWORD before = gdi_count();
    ENUMCTX c; memset(&c, 0, sizeof c);
    c.play = 1; c.dc = d.dc;
    CHECK(U(EnumMetaFile)(d.dc, h, enum_cb, (LPARAM)&c));
    EXPECT_EQ(gdi_count(), before);
    dibdc_free(&d);
    U(DeleteMetaFile)(h);
}

TEST(wmf_enum_handle_table_contents) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile));
    HMETAFILE h = mk_enum_sample(NULL);
    REQUIRE(h);
    DIBDC d; REQUIRE(dibdc_make(&d, 10, 10));
    typedef struct { int pens_seen; int nonnull_after_create; } T;
    static T t; memset(&t, 0, sizeof t);
    int CALLBACK cb(HDC dc, HANDLETABLE *ht, METARECORD *mr, int nobj, LPARAM lp) {
        U(PlayMetaFileRecord)(dc, ht, mr, nobj);
        if (mr->rdFunction == 0x02FA) { t.pens_seen++; if (ht->objectHandle[0]) t.nonnull_after_create++; }
        return 1;
    }
    CHECK(U(EnumMetaFile)(d.dc, h, cb, 0));
    CHECK(t.pens_seen >= 1);
    CHECK(t.nonnull_after_create >= 1);
    dibdc_free(&d);
    U(DeleteMetaFile)(h);
}

TEST(wmf_enum_restores_dc_objects) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile)); REQUIRE(U(GetCurrentObject));
    HMETAFILE h = mk_enum_sample(NULL);
    REQUIRE(h);
    DIBDC d; REQUIRE(dibdc_make(&d, 10, 10));
    HPEN p = U(CreatePen)(PS_DOT, 1, 0);
    HGDIOBJ op = U(SelectObject)(d.dc, p);
    ENUMCTX c; memset(&c, 0, sizeof c);
    c.play = 1; c.dc = d.dc;
    CHECK(U(EnumMetaFile)(d.dc, h, enum_cb, (LPARAM)&c));
    EXPECT_EQ(U(GetCurrentObject)(d.dc, OBJ_PEN), p);
    U(SelectObject)(d.dc, op); U(DeleteObject)(p);
    dibdc_free(&d);
    U(DeleteMetaFile)(h);
}

TEST(wmf_enum_many_records) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile));
    HDC m = mf_new();
    REQUIRE(m);
    for (int i = 0; i < 3000; i++) U(LineTo)(m, i & 255, i & 127);
    HMETAFILE h = U(CloseMetaFile)(m);
    REQUIRE(h);
    static int n;
    n = 0;
    int CALLBACK cb(HDC dc, HANDLETABLE *ht, METARECORD *mr, int nobj, LPARAM lp) { n++; return 1; }
    CHECK(U(EnumMetaFile)(NULL, h, cb, 0));
    EXPECT_EQ(n, 3000);
    U(DeleteMetaFile)(h);
}

TEST(wmf_enum_on_file_metafile) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile));
    char path[MAX_PATH]; tmp_path(path, "wmf_en");
    DeleteFileA(path);
    HDC m = U(CreateMetaFileA)(path);
    REQUIRE(m);
    U(Rectangle)(m, 1, 2, 3, 4); U(Ellipse)(m, 1, 2, 3, 4);
    HMETAFILE h = U(CloseMetaFile)(m);
    REQUIRE(h);
    ENUMCTX c; memset(&c, 0, sizeof c);
    CHECK(U(EnumMetaFile)(NULL, h, enum_cb, (LPARAM)&c));
    EXPECT_EQ(c.n, 2);
    if (c.n == 2) { EXPECT_EQ(c.funcs[0], 0x041B); EXPECT_EQ(c.funcs[1], 0x0418); }
    U(DeleteMetaFile)(h);
    DeleteFileA(path);
}

TEST(wmf_enum_metafile_dc_target) {
    WMF_REQ_BASE(); REQUIRE(U(EnumMetaFile));
    HMETAFILE h = mk_enum_sample(NULL);
    REQUIRE(h);
    HDC m = mf_new();
    REQUIRE(m);
    ENUMCTX c; memset(&c, 0, sizeof c);
    c.play = 1; c.dc = m;
    BOOL r = U(EnumMetaFile)(m, h, enum_cb, (LPARAM)&c);
    CHECK(r);
    WB b;
    REQUIRE(mf_finish(m, &b, NULL));
    CHECK(wb_valid(&b));
    EXPECT_EQ(wb_count(&b, 0x041B), 1);
    EXPECT_EQ(wb_count(&b, 0x0418), 1);
    wb_free(&b);
    U(DeleteMetaFile)(h);
}
