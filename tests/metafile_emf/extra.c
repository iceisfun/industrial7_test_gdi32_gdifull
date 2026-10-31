#include "emf.h"

/* Additional record-level, header and pixel checks. */

static HENHMETAFILE x_make(void (*s)(HDC)) {
    HDC dc = emf_begin();
    if (!dc) return NULL;
    s(dc);
    return emf_end(dc);
}

/* ---- single-DWORD-payload state records: size 12, payload as passed ---- */
typedef struct { DWORD type; DWORD val; } x_dw;
static void x_sc_textalign(HDC dc) { U(SetTextAlign)(dc, TA_CENTER | TA_BOTTOM); }
static void x_sc_stretchmode(HDC dc) { U(SetStretchBltMode)(dc, HALFTONE); }
static void x_sc_polyfill(HDC dc) { U(SetPolyFillMode)(dc, WINDING); }
static void x_sc_bkmode(HDC dc) { U(SetBkMode)(dc, TRANSPARENT); }
static void x_sc_rop2(HDC dc) { U(SetROP2)(dc, R2_NOT); }
static void x_sc_mapmode(HDC dc) { U(SetMapMode)(dc, MM_HIMETRIC); }
static void x_sc_arcdir(HDC dc) { U(SetArcDirection)(dc, AD_CLOCKWISE); }
static void x_sc_mapper(HDC dc) { U(SetMapperFlags)(dc, 1); }

static void x_dw_run(void (*s)(HDC), DWORD type, DWORD val) {
    HENHMETAFILE h = x_make(s);
    CHECK(h != NULL);
    if (!h) return;
    UINT n; BYTE *b = emf_bits(h, &n);
    CHECK(b != NULL);
    if (b) {
        const ENHMETARECORD *r = emf_find(b, n, type, 0);
        CHECKF(r != NULL, "record %lu missing", (unsigned long)type);
        if (r) {
            EXPECT_EQ(r->nSize, 12);
            EXPECT_EQ(r->dParm[0], val);
        }
        free(b);
    }
    U(DeleteEnhMetaFile)(h);
}
TEST(emf_x_payload_textalign) { REQUIRE(U(CreateEnhMetaFileW)); x_dw_run(x_sc_textalign, EMR_SETTEXTALIGN, TA_CENTER | TA_BOTTOM); }
TEST(emf_x_payload_stretchmode) { REQUIRE(U(CreateEnhMetaFileW)); x_dw_run(x_sc_stretchmode, EMR_SETSTRETCHBLTMODE, HALFTONE); }
TEST(emf_x_payload_polyfill) { REQUIRE(U(CreateEnhMetaFileW)); x_dw_run(x_sc_polyfill, EMR_SETPOLYFILLMODE, WINDING); }
TEST(emf_x_payload_bkmode) { REQUIRE(U(CreateEnhMetaFileW)); x_dw_run(x_sc_bkmode, EMR_SETBKMODE, TRANSPARENT); }
TEST(emf_x_payload_rop2) { REQUIRE(U(CreateEnhMetaFileW)); x_dw_run(x_sc_rop2, EMR_SETROP2, R2_NOT); }
TEST(emf_x_payload_mapmode) { REQUIRE(U(CreateEnhMetaFileW)); x_dw_run(x_sc_mapmode, EMR_SETMAPMODE, MM_HIMETRIC); }
TEST(emf_x_payload_arcdirection) { REQUIRE(U(CreateEnhMetaFileW)); x_dw_run(x_sc_arcdir, EMR_SETARCDIRECTION, AD_CLOCKWISE); }
TEST(emf_x_payload_mapperflags) { REQUIRE(U(CreateEnhMetaFileW)); x_dw_run(x_sc_mapper, EMR_SETMAPPERFLAGS, 1); }

/* ---- world transform records ---- */
static void x_sc_wt(HDC dc) {
    XFORM x = { 2.0f, 0.0f, 0.0f, 3.0f, 10.0f, 20.0f };
    U(SetGraphicsMode)(dc, GM_ADVANCED);
    U(SetWorldTransform)(dc, &x);
}
TEST(emf_x_setworldtransform_payload) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(SetWorldTransform)); REQUIRE(U(SetGraphicsMode));
    HENHMETAFILE h = x_make(x_sc_wt);
    CHECK(h != NULL);
    if (!h) return;
    UINT n; BYTE *b = emf_bits(h, &n);
    if (b) {
        const ENHMETARECORD *r = emf_find(b, n, EMR_SETWORLDTRANSFORM, 0);
        CHECK(r != NULL);
        if (r) {
            EXPECT_EQ(r->nSize, 8 + sizeof(XFORM));
            const XFORM *x = (const XFORM *)r->dParm;
            CHECK(x->eM11 == 2.0f && x->eM12 == 0.0f && x->eM21 == 0.0f && x->eM22 == 3.0f);
            CHECK(x->eDx == 10.0f && x->eDy == 20.0f);
        }
        free(b);
    }
    U(DeleteEnhMetaFile)(h);
}

static void x_sc_mwt(HDC dc) {
    XFORM x = { 1.0f, 0.0f, 0.0f, 1.0f, 5.0f, 6.0f };
    U(SetGraphicsMode)(dc, GM_ADVANCED);
    U(ModifyWorldTransform)(dc, &x, MWT_LEFTMULTIPLY);
}
TEST(emf_x_modifyworldtransform_payload) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(ModifyWorldTransform)); REQUIRE(U(SetGraphicsMode));
    HENHMETAFILE h = x_make(x_sc_mwt);
    CHECK(h != NULL);
    if (!h) return;
    UINT n; BYTE *b = emf_bits(h, &n);
    if (b) {
        const ENHMETARECORD *r = emf_find(b, n, EMR_MODIFYWORLDTRANSFORM, 0);
        CHECK(r != NULL);
        if (r) {
            EXPECT_EQ(r->nSize, 8 + sizeof(XFORM) + 4);
            const XFORM *x = (const XFORM *)r->dParm;
            CHECK(x->eDx == 5.0f && x->eDy == 6.0f);
            EXPECT_EQ(r->dParm[6], MWT_LEFTMULTIPLY);
        }
        free(b);
    }
    U(DeleteEnhMetaFile)(h);
}

static void x_sc_mwt_ident(HDC dc) {
    U(SetGraphicsMode)(dc, GM_ADVANCED);
    U(ModifyWorldTransform)(dc, NULL, MWT_IDENTITY);
}
TEST(emf_x_modifyworldtransform_identity_recorded) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(ModifyWorldTransform)); REQUIRE(U(SetGraphicsMode));
    HENHMETAFILE h = x_make(x_sc_mwt_ident);
    CHECK(h != NULL);
    if (!h) return;
    UINT n; BYTE *b = emf_bits(h, &n);
    if (b) {
        const ENHMETARECORD *r = emf_find(b, n, EMR_MODIFYWORLDTRANSFORM, 0);
        CHECK(r != NULL);
        if (r && r->nSize >= 8 + sizeof(XFORM) + 4) EXPECT_EQ(r->dParm[6], MWT_IDENTITY);
        free(b);
    }
    U(DeleteEnhMetaFile)(h);
}

/* ---- PolyBezier16 and Arc ---- */
static void x_sc_bezier(HDC dc) {
    POINT p[4] = { {1, 2}, {10, 20}, {30, 40}, {50, 60} };
    U(PolyBezier)(dc, p, 4);
}
TEST(emf_x_polybezier16_layout) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(PolyBezier));
    HENHMETAFILE h = x_make(x_sc_bezier);
    CHECK(h != NULL);
    if (!h) return;
    UINT n; BYTE *b = emf_bits(h, &n);
    if (b) {
        const ENHMETARECORD *r = emf_find(b, n, EMR_POLYBEZIER16, 0);
        CHECK(r != NULL);
        if (r) {
            EXPECT_EQ(r->nSize, 8 + 16 + 4 + 4 * 4);
            EXPECT_EQ(r->dParm[4], 4);
            const SHORT *s = (const SHORT *)&r->dParm[5];
            CHECK(s[0] == 1 && s[1] == 2 && s[6] == 50 && s[7] == 60);
        }
        free(b);
    }
    U(DeleteEnhMetaFile)(h);
}

static void x_sc_arc(HDC dc) { U(Arc)(dc, 1, 2, 31, 42, 5, 6, 7, 8); }
TEST(emf_x_arc_layout) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(Arc));
    HENHMETAFILE h = x_make(x_sc_arc);
    CHECK(h != NULL);
    if (!h) return;
    UINT n; BYTE *b = emf_bits(h, &n);
    if (b) {
        const ENHMETARECORD *r = emf_find(b, n, EMR_ARC, 0);
        CHECK(r != NULL);
        if (r) {
            EXPECT_EQ(r->nSize, 8 + 16 + 8 + 8);
            const LONG *p = (const LONG *)r->dParm;
            CHECK(p[0] == 1 && p[1] == 2);
            SOFT(p[2] == 31 && p[3] == 42);
            CHECK(p[4] == 5 && p[5] == 6 && p[6] == 7 && p[7] == 8);
        }
        free(b);
    }
    U(DeleteEnhMetaFile)(h);
}

/* ---- header relationships ---- */
TEST(emf_x_hdr_description_offsets_consistent) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(GetEnhMetaFileBits)); REQUIRE(U(GetEnhMetaFileHeader));
    HDC dc = U(CreateEnhMetaFileW)(NULL, NULL, NULL, L"Offsets\0Check\0");
    if (!dc) { CHECK(dc != NULL); return; }
    U(PatBlt)(dc, 0, 0, 3, 3, BLACKNESS);
    HENHMETAFILE h = emf_end(dc);
    UINT n; BYTE *b = emf_bits(h, &n);
    CHECK(b != NULL);
    if (b) {
        const ENHMETAHEADER *hd = (const ENHMETAHEADER *)b;
        EXPECT_EQ(hd->iType, EMR_HEADER);
        EXPECT_EQ(hd->nBytes, n);
        CHECK((hd->nBytes & 3) == 0);
        CHECK(hd->nSize >= 100 && (hd->nSize & 3) == 0);
        CHECK(hd->offDescription >= 88);
        CHECK(hd->offDescription + hd->nDescription * 2 <= hd->nSize);
        CHECK(hd->nDescription >= 14);
        if (hd->offDescription + 14 * 2 <= n)
            CHECK(!memcmp(b + hd->offDescription, L"Offsets\0Check\0", 28));
        free(b);
    }
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_x_hdr_nhandles_counts_slots) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(GetEnhMetaFileHeader)); REQUIRE(U(CreatePen));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    HPEN p[3]; HGDIOBJ old[3];
    for (int i = 0; i < 3; i++) {
        p[i] = U(CreatePen)(PS_SOLID, 1, RGB(i * 40, 0, 0));
        old[i] = U(SelectObject)(dc, p[i]);
    }
    for (int i = 0; i < 3; i++) U(SelectObject)(dc, old[0]);
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; memset(&hd, 0, sizeof hd);
    CHECK(U(GetEnhMetaFileHeader)(h, sizeof hd, &hd) >= 88);
    EXPECT_EQ(hd.nHandles, 4);
    U(DeleteEnhMetaFile)(h);
    for (int i = 0; i < 3; i++) U(DeleteObject)(p[i]);
}

TEST(emf_x_hdr_header_is_first_record_bytes) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(GetEnhMetaFileHeader));
    HENHMETAFILE h = x_make(x_sc_bkmode);
    CHECK(h != NULL);
    if (!h) return;
    UINT n; BYTE *b = emf_bits(h, &n);
    if (b) {
        ENHMETAHEADER hd; memset(&hd, 0, sizeof hd);
        UINT r = U(GetEnhMetaFileHeader)(h, sizeof hd, &hd);
        CHECK(r >= 88 && r <= sizeof hd);
        if (r >= 88 && n >= r) {
            /* the clamped copy matches the stored header except the nSize field when it differs */
            CHECK(!memcmp(&hd.rclBounds, b + 8, 80 - 0 > r - 8 ? r - 8 : 80));
        }
        free(b);
    }
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_x_hdr_size_nonnull_zero_buffer) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(GetEnhMetaFileHeader));
    HENHMETAFILE h = x_make(x_sc_bkmode);
    CHECK(h != NULL);
    if (!h) return;
    ENHMETAHEADER hd; memset(&hd, 0xCC, sizeof hd);
    SetLastError(0);
    UINT r = U(GetEnhMetaFileHeader)(h, 0, &hd);
    CHECK(r == 0);
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    CHECK(((BYTE *)&hd)[0] == 0xCC);
    U(DeleteEnhMetaFile)(h);
}

/* ---- exact pixel playback into a DIB (interior pixels only) ---- */
typedef struct { DWORD rop; DWORD bg; DWORD want; } x_rop;
static HENHMETAFILE x_make_patblt_rop(DWORD rop) {
    HDC dc = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!dc) return NULL;
    U(PatBlt)(dc, 0, 0, 32, 32, rop);
    return emf_end(dc);
}
TEST(emf_x_play_patblt_rops_interior) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(PlayEnhMetaFile)); REQUIRE(U(PatBlt));
    static const x_rop t[] = {
        { BLACKNESS, 0xFFFFFF, 0x000000 }, { WHITENESS, 0x000000, 0xFFFFFF },
        { DSTINVERT, 0xFFFFFF, 0x000000 }, { DSTINVERT, 0x000000, 0xFFFFFF },
        { DSTINVERT, 0x123456, 0xEDCBA9 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        HENHMETAFILE h = x_make_patblt_rop(t[i].rop);
        CHECKF(h != NULL, "rop %u", i);
        if (!h) continue;
        emf_dib d;
        if (emf_dib_make(&d, 48, 48, t[i].bg)) {
            RECT rc = {0, 0, 48, 48};
            BOOL ok = U(PlayEnhMetaFile)(d.dc, h, &rc);
            CHECKF(ok, "play %u", i);
            /* the frame spans the whole 32x32 region: the center must carry the rop result */
            CHECKF(emf_px(&d, 24, 24) == t[i].want, "rop %u center %06lx want %06lx", i, (unsigned long)emf_px(&d, 24, 24), (unsigned long)t[i].want);
            emf_dib_free(&d);
        }
        U(DeleteEnhMetaFile)(h);
    }
}

TEST(emf_x_play_into_rect_far_corner_untouched) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(PlayEnhMetaFile));
    HENHMETAFILE h = x_make_patblt_rop(BLACKNESS);
    CHECK(h != NULL);
    if (!h) return;
    emf_dib d;
    if (emf_dib_make(&d, 64, 64, 0xFFFFFF)) {
        RECT rc = {8, 8, 24, 24};
        CHECK(U(PlayEnhMetaFile)(d.dc, h, &rc));
        CHECK(emf_px(&d, 16, 16) == 0x000000);
        CHECK(emf_px(&d, 0, 0) == 0xFFFFFF);
        CHECK(emf_px(&d, 63, 63) == 0xFFFFFF);
        CHECK(emf_px(&d, 40, 40) == 0xFFFFFF);
        CHECK(emf_px(&d, 4, 30) == 0xFFFFFF);
        emf_dib_free(&d);
    }
    U(DeleteEnhMetaFile)(h);
}

static void x_sc_solid_rect(HDC dc) {
    HBRUSH br = U(CreateSolidBrush)(RGB(10, 200, 30));
    HGDIOBJ ob = U(SelectObject)(dc, br);
    HGDIOBJ op = U(SelectObject)(dc, U(GetStockObject)(NULL_PEN));
    U(Rectangle)(dc, 0, 0, 100, 100);
    U(SelectObject)(dc, op);
    U(SelectObject)(dc, ob);
    U(DeleteObject)(br);
}
TEST(emf_x_play_solid_brush_color_exact) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(PlayEnhMetaFile)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(Rectangle));
    HDC dc = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!dc) { CHECK(dc != NULL); return; }
    x_sc_solid_rect(dc);
    HENHMETAFILE h = emf_end(dc);
    emf_dib d;
    if (emf_dib_make(&d, 100, 100, 0xFFFFFF)) {
        RECT rc = {0, 0, 100, 100};
        CHECK(U(PlayEnhMetaFile)(d.dc, h, &rc));
        EXPECT_EQ(emf_px(&d, 50, 50), BGR(10, 200, 30));
        EXPECT_EQ(emf_px(&d, 25, 75), BGR(10, 200, 30));
        emf_dib_free(&d);
    }
    U(DeleteEnhMetaFile)(h);
}

/* ---- GDI object accounting ---- */
TEST(emf_x_gdi_count_stable_over_record_play_cycle) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(PlayEnhMetaFile)); REQUIRE(U(CreateSolidBrush));
    DWORD g0 = emf_gdi_count();
    for (int i = 0; i < 50; i++) {
        HDC dc = emf_begin();
        if (!dc) { CHECK(dc != NULL); break; }
        x_sc_solid_rect(dc);
        HENHMETAFILE h = emf_end(dc);
        emf_dib d;
        if (emf_dib_make(&d, 16, 16, 0)) {
            RECT rc = {0, 0, 16, 16};
            U(PlayEnhMetaFile)(d.dc, h, &rc);
            emf_dib_free(&d);
        }
        U(DeleteEnhMetaFile)(h);
    }
    EXPECT_EQ(emf_gdi_count(), g0);
}

/* ---- bounds ---- */
TEST(emf_x_bounds_polygon_contain_all_points) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(Polygon)); REQUIRE(U(GetEnhMetaFileHeader));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    POINT p[4] = { {40, 50}, {200, 60}, {180, 300}, {20, 250} };
    U(Polygon)(dc, p, 4);
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; memset(&hd, 0, sizeof hd);
    CHECK(U(GetEnhMetaFileHeader)(h, sizeof hd, &hd) >= 88);
    CHECK(hd.rclBounds.left <= 20 && hd.rclBounds.top <= 50);
    CHECK(hd.rclBounds.right >= 199 && hd.rclBounds.bottom >= 299);
    CHECK(hd.rclBounds.left >= 0 && hd.rclBounds.right < 400 && hd.rclBounds.bottom < 400);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_x_bounds_moveto_alone_is_empty) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(MoveToEx)); REQUIRE(U(GetEnhMetaFileHeader));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    U(MoveToEx)(dc, 100, 100, NULL);
    U(SetBkColor)(dc, RGB(1, 2, 3));
    HENHMETAFILE h = emf_end(dc);
    ENHMETAHEADER hd; memset(&hd, 0, sizeof hd);
    CHECK(U(GetEnhMetaFileHeader)(h, sizeof hd, &hd) >= 88);
    /* nothing drawn: bounds stay inverted/empty */
    CHECK(hd.rclBounds.right <= hd.rclBounds.left || hd.rclBounds.bottom <= hd.rclBounds.top);
    U(DeleteEnhMetaFile)(h);
}

/* ---- API robustness ---- */
TEST(emf_x_getbits_after_play_unchanged) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(PlayEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits));
    HENHMETAFILE h = x_make(x_sc_solid_rect);
    CHECK(h != NULL);
    if (!h) return;
    UINT n1, n2; BYTE *b1 = emf_bits(h, &n1);
    emf_dib d;
    if (emf_dib_make(&d, 32, 32, 0)) {
        RECT rc = {0, 0, 32, 32};
        U(PlayEnhMetaFile)(d.dc, h, &rc);
        emf_dib_free(&d);
    }
    BYTE *b2 = emf_bits(h, &n2);
    EXPECT_EQ(n1, n2);
    CHECK(b1 && b2 && n1 == n2 && !memcmp(b1, b2, n1));
    free(b1); free(b2);
    U(DeleteEnhMetaFile)(h);
}

TEST(emf_x_setbits_zero_and_null) {
    REQUIRE(U(SetEnhMetaFileBits));
    SetLastError(0);
    CHECK(U(SetEnhMetaFileBits)(0, NULL) == NULL);
    BYTE one[4] = {0};
    CHECK(U(SetEnhMetaFileBits)(0, one) == NULL);
    CHECK(U(SetEnhMetaFileBits)(4, one) == NULL);
    CHECK(U(SetEnhMetaFileBits)(87, one) == NULL);
}

TEST(emf_x_hdr_signature_and_version_every_dc_kind) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(GetEnhMetaFileHeader)); REQUIRE(U(CreateCompatibleDC));
    HDC ref[2];
    ref[0] = NULL; ref[1] = U(CreateCompatibleDC)(NULL);
    for (int i = 0; i < 2; i++) {
        HDC dc = U(CreateEnhMetaFileW)(ref[i], NULL, NULL, NULL);
        CHECKF(dc != NULL, "ref %d", i);
        if (!dc) continue;
        U(PatBlt)(dc, 0, 0, 2, 2, BLACKNESS);
        HENHMETAFILE h = emf_end(dc);
        ENHMETAHEADER hd; memset(&hd, 0, sizeof hd);
        CHECK(U(GetEnhMetaFileHeader)(h, sizeof hd, &hd) >= 88);
        EXPECT_EQ(hd.iType, 1);
        EXPECT_EQ(hd.dSignature, 0x464D4520);
        EXPECT_EQ(hd.nVersion, 0x10000);
        U(DeleteEnhMetaFile)(h);
    }
    if (ref[1]) U(DeleteDC)(ref[1]);
}
