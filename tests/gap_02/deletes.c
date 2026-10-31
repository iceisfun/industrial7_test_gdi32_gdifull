/* InternalDeleteDC / InternalDeleteObject (the workers behind DeleteDC / DeleteObject),
 * MF_DeleteObject and MF16_DeleteObject (metafile link bookkeeping). */
#include "g2.h"

UDECL(BOOL, InternalDeleteDC, (HDC));
UDECL(BOOL, InternalDeleteObject, (HGDIOBJ));
UDECL(BOOL, MF_DeleteObject, (HGDIOBJ));
UDECL(BOOL, MF16_DeleteObject, (HGDIOBJ));

/* wine has no InternalDelete*: fall back to the public API there so the test logic is still exercised */
static BOOL idc(HDC h) { InternalDeleteDC_pfn f = G32(InternalDeleteDC); return f ? f(h) : U(DeleteDC)(h); }
static BOOL ido(HGDIOBJ h) { InternalDeleteObject_pfn f = G32(InternalDeleteObject); return f ? f(h) : U(DeleteObject)(h); }
#define IDC(h)  idc(h)
#define IDO(h)  ido(h)
#define HAVE_IDC() (G32(InternalDeleteDC) || g2_wine())
#define HAVE_IDO() (G32(InternalDeleteObject) || g2_wine())
#define NEED_IDC()  do { REQUIRE(HAVE_IDC()); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetObjectType)); } while (0)
#define NEED_IDO()  do { REQUIRE(HAVE_IDO()); REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectType)); } while (0)

/* ------------------------------ InternalDeleteDC ------------------------------ */
TEST(gap2_idc_memory_dc) {
    NEED_IDC();
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(dc != NULL);
    G2_EQ(U(GetObjectType)(dc), OBJ_MEMDC);
    CHECK(IDC(dc));
    G2_EQ(U(GetObjectType)(dc), 0);
}
TEST(gap2_idc_second_delete_fails) {
    NEED_IDC();
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(IDC(dc));
    SetLastError(0);
    CHECK(!IDC(dc));
    t_note("second InternalDeleteDC last error %lu", GetLastError());
    CHECK(!U(DeleteDC)(dc));
}
TEST(gap2_idc_junk_handles) {
    NEED_IDC();
    for (int i = 0; i < G2_NJUNK; i++) {
        SetLastError(0x5555);
        BOOL r = IDC((HDC)g2_junk[i]);
        CHECKF(!r, "handle %#llx accepted", (unsigned long long)g2_junk[i]);
        EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    }
}
TEST(gap2_idc_junk_matches_deletedc) {
    NEED_IDC();
    for (int i = 0; i < G2_NJUNK; i++) {
        SetLastError(0);
        BOOL a = U(DeleteDC)((HDC)g2_junk[i]); DWORD ea = GetLastError();
        SetLastError(0);
        BOOL b = IDC((HDC)g2_junk[i]); DWORD eb = GetLastError();
        CHECKF(a == b && ea == eb, "handle %#llx: DeleteDC %d/%lu internal %d/%lu", (unsigned long long)g2_junk[i], a, ea, b, eb);
    }
}
TEST(gap2_idc_null_handle) {
    NEED_IDC();
    SetLastError(0);
    BOOL r = IDC(NULL);
    CHECK(!r); EXPECT_ERR(r, ERROR_INVALID_HANDLE);
}
TEST(gap2_idc_stale_handle) {
    NEED_IDC();
    HDC dc = g2_stale_dc();
    SetLastError(0);
    CHECK(!IDC(dc));
    t_note("stale DC InternalDeleteDC last error %lu", GetLastError());
}
TEST(gap2_idc_with_selected_objects) {
    NEED_IDC(); REQUIRE(U(SelectObject)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(CreatePen));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 32, NULL);
    HBRUSH br = U(CreateSolidBrush)(RGB(10, 20, 30));
    HPEN pen = U(CreatePen)(PS_SOLID, 2, RGB(1, 2, 3));
    U(SelectObject)(dc, bm); U(SelectObject)(dc, br); U(SelectObject)(dc, pen);
    /* deleting the DC with objects still selected is legal and releases them */
    CHECK(IDC(dc));
    CHECK(U(DeleteObject)(bm));
    CHECK(U(DeleteObject)(br));
    CHECK(U(DeleteObject)(pen));
    G2_EQ(U(GetObjectType)(bm), 0);
}
TEST(gap2_idc_gdi_count_balanced) {
    NEED_IDC();
    DWORD base = g2_gdi_count();
    HDC dcs[64];
    for (int i = 0; i < 64; i++) { dcs[i] = U(CreateCompatibleDC)(NULL); CHECK(dcs[i] != NULL); }
    EXPECT_GDI(base + 64);
    for (int i = 0; i < 64; i++) CHECK(IDC(dcs[i]));
    EXPECT_GDI(base);
}
TEST(gap2_idc_repeat_cycles) {
    NEED_IDC();
    DWORD base = g2_gdi_count();
    for (int i = 0; i < 2000; i++) {
        HDC dc = U(CreateCompatibleDC)(NULL);
        if (!dc) { CHECKF(0, "create failed at %d", i); break; }
        if (!IDC(dc)) { CHECKF(0, "delete failed at %d", i); break; }
    }
    EXPECT_GDI(base);
}
TEST(gap2_idc_handle_reuse_not_aliasing) {
    NEED_IDC();
    HDC a = U(CreateCompatibleDC)(NULL);
    CHECK(IDC(a));
    HDC b = U(CreateCompatibleDC)(NULL);
    /* the table slot may be reused, but the reused handle must differ in its uniqueness bits */
    CHECK(b != NULL);
    CHECKF(a != b, "handle value reused exactly: %p", a);
    CHECK(!IDC(a) || a == b);
    G2_EQ(U(GetObjectType)(b), OBJ_MEMDC);
    CHECK(IDC(b));
}
TEST(gap2_idc_bitmap_survives_dc_delete) {
    NEED_IDC(); REQUIRE(U(SelectObject));
    G2DC d; if (!g2_mk(&d, 4, 4)) { CHECK(0); return; }
    d.bits[5] = 0xabcdef;
    U(SelectObject)(d.dc, d.old);
    CHECK(IDC(d.dc)); d.dc = NULL; d.old = NULL;
    G2_EQ(U(GetObjectType)(d.bmp), OBJ_BITMAP);
    G2_EQ(d.bits[5], 0xabcdef);
    g2_free(&d);
}
TEST(gap2_idc_display_dc) {
    NEED_IDC(); REQUIRE(U(CreateDCA));
    HDC dc = U(CreateDCA)("DISPLAY", NULL, NULL, NULL);
    if (!dc) { t_skip("no display DC"); return; }
    G2_EQ(U(GetObjectType)(dc), OBJ_DC);
    CHECK(IDC(dc));
    G2_EQ(U(GetObjectType)(dc), 0);
}
TEST(gap2_idc_info_dc) {
    NEED_IDC(); REQUIRE(U(CreateICA));
    HDC dc = U(CreateICA)("DISPLAY", NULL, NULL, NULL);
    if (!dc) { t_skip("no display IC"); return; }
    CHECK(IDC(dc));
    CHECK(!IDC(dc));
}
TEST(gap2_idc_wrong_object_types_refused) {
    NEED_IDC(); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(CreatePen)); REQUIRE(U(CreateRectRgn)); REQUIRE(U(CreateBitmap));
    HGDIOBJ o[4] = { U(CreateSolidBrush)(1), U(CreatePen)(PS_SOLID, 1, 0), U(CreateRectRgn)(0, 0, 3, 3), U(CreateBitmap)(2, 2, 1, 1, NULL) };
    for (int i = 0; i < 4; i++) {
        SetLastError(0);
        BOOL r = IDC((HDC)o[i]);
        CHECKF(!r, "object %d", i);
        t_note("InternalDeleteDC(non-DC object %d) err %lu", i, GetLastError());
        CHECKF(U(GetObjectType)(o[i]) != 0, "object %d was destroyed", i);
        U(DeleteObject)(o[i]);
    }
}
TEST(gap2_idc_stock_object_refused) {
    NEED_IDC(); REQUIRE(U(GetStockObject));
    HGDIOBJ st = U(GetStockObject)(WHITE_BRUSH);
    CHECK(!IDC((HDC)st));
    G2_EQ(U(GetObjectType)(st), OBJ_BRUSH);
}
TEST(gap2_idc_wmf_dc_refused) {
    NEED_IDC(); REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    SKIP_ON_WINE("wine allows deleting a WMF DC");
    HDC m = U(CreateMetaFileW)(NULL);
    if (!m) { t_skip("no WMF DC"); return; }
    SetLastError(0);
    BOOL r = IDC(m);
    CHECK(!r); EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    /* still a live metafile DC */
    HMETAFILE h = U(CloseMetaFile)(m);
    CHECK(h != NULL);
    if (h) U(DeleteMetaFile)(h);
}
TEST(gap2_idc_closed_emf_dc_is_stale) {
    NEED_IDC(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    CHECK(h != NULL);
    SetLastError(0);
    BOOL r = IDC(m);
    CHECK(!r); EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    if (h) U(DeleteEnhMetaFile)(h);
}
TEST(gap2_idc_cross_thread_delete) {
    NEED_IDC();
    struct S { HDC dc; } s; s.dc = U(CreateCompatibleDC)(NULL);
    CHECK(s.dc != NULL);
    BOOL r = IDC(s.dc);   /* same-process delete from a different thread than the creator is exercised below */
    CHECK(r);
}
static DWORD WINAPI idc_thread(LPVOID p) {
    HDC *out = p;
    *out = U(CreateCompatibleDC)(NULL);
    return 0;
}
TEST(gap2_idc_dc_from_other_thread) {
    NEED_IDC();
    HDC dc = NULL;
    HANDLE t = CreateThread(NULL, 0, idc_thread, &dc, 0, NULL);
    CHECK(t != NULL); if (!t) return;
    WaitForSingleObject(t, 5000); CloseHandle(t);
    CHECK(dc != NULL);
    /* GDI handles belong to the process, not the creating thread */
    G2_EQ(U(GetObjectType)(dc), OBJ_MEMDC);
    BOOL r = IDC(dc);
    t_note("cross-thread InternalDeleteDC -> %d", r);
    if (!r) U(DeleteDC)(dc);
}

/* ------------------------------ InternalDeleteObject ------------------------------ */
typedef HGDIOBJ (*mk_fn)(void);
static HGDIOBJ mk_pen(void) { return U(CreatePen)(PS_SOLID, 1, RGB(1, 2, 3)); }
static HGDIOBJ mk_extpen(void) { LOGBRUSH lb = { BS_SOLID, RGB(1, 2, 3), 0 }; return U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID, 4, &lb, 0, NULL); }
static HGDIOBJ mk_brush(void) { return U(CreateSolidBrush)(RGB(9, 8, 7)); }
static HGDIOBJ mk_hatch(void) { return U(CreateHatchBrush)(HS_CROSS, RGB(9, 8, 7)); }
static HGDIOBJ mk_pat(void) { HBITMAP b = U(CreateBitmap)(8, 8, 1, 1, NULL); HGDIOBJ r = U(CreatePatternBrush)(b); U(DeleteObject)(b); return r; }
static HGDIOBJ mk_bmp(void) { return U(CreateBitmap)(5, 5, 1, 32, NULL); }
static HGDIOBJ mk_cbmp(void) { HDC d = U(CreateCompatibleDC)(NULL); HBITMAP b = U(CreateCompatibleBitmap)(d, 5, 5); U(DeleteDC)(d); return b; }
static HGDIOBJ mk_rgn(void) { return U(CreateRectRgn)(0, 0, 10, 10); }
static HGDIOBJ mk_ellrgn(void) { return U(CreateEllipticRgn)(0, 0, 10, 10); }
static HGDIOBJ mk_font(void) { LOGFONTW lf; memset(&lf, 0, sizeof lf); lf.lfHeight = -12; lstrcpyW(lf.lfFaceName, L"Arial"); return U(CreateFontIndirectW)(&lf); }
static HGDIOBJ mk_pal(void) { LOGPALETTE lp = { 0x300, 1, { { 1, 2, 3, 0 } } }; return U(CreatePalette)(&lp); }

static void idobj_roundtrip(mk_fn mk, DWORD type) {
    HGDIOBJ o = mk();
    CHECK(o != NULL); if (!o) return;
    G2_EQ(U(GetObjectType)(o), type);
    DWORD base = g2_gdi_count();
    CHECK(IDO(o));
    G2_EQ(U(GetObjectType)(o), 0);
    EXPECT_GDI(base - 1);
    SetLastError(0);
    CHECK(!IDO(o));
    CHECK(!U(DeleteObject)(o));
}
#define IDO_TYPE_TEST(nm, mk, ty) TEST(gap2_ido_##nm) { NEED_IDO(); REQUIRE(U(GetStockObject)); idobj_roundtrip(mk, ty); }
IDO_TYPE_TEST(pen, mk_pen, OBJ_PEN)
IDO_TYPE_TEST(extpen, mk_extpen, OBJ_EXTPEN)
IDO_TYPE_TEST(solid_brush, mk_brush, OBJ_BRUSH)
IDO_TYPE_TEST(hatch_brush, mk_hatch, OBJ_BRUSH)
IDO_TYPE_TEST(pattern_brush, mk_pat, OBJ_BRUSH)
IDO_TYPE_TEST(bitmap, mk_bmp, OBJ_BITMAP)
IDO_TYPE_TEST(compat_bitmap, mk_cbmp, OBJ_BITMAP)
IDO_TYPE_TEST(rect_region, mk_rgn, OBJ_REGION)
IDO_TYPE_TEST(ellipse_region, mk_ellrgn, OBJ_REGION)
IDO_TYPE_TEST(font, mk_font, OBJ_FONT)
IDO_TYPE_TEST(palette, mk_pal, OBJ_PAL)

TEST(gap2_ido_dibsection) {
    NEED_IDO();
    G2DC d; if (!g2_mk(&d, 8, 8)) { CHECK(0); return; }
    U(SelectObject)(d.dc, d.old); d.old = NULL;
    HBITMAP b = d.bmp;
    CHECK(IDO(b));
    G2_EQ(U(GetObjectType)(b), 0);
    d.bmp = NULL; d.bits = NULL;
    g2_free(&d);
}
TEST(gap2_ido_junk_handles) {
    NEED_IDO();
    for (int i = 0; i < G2_NJUNK; i++) {
        SetLastError(0);
        CHECKF(!IDO((HGDIOBJ)g2_junk[i]), "handle %#llx accepted", (unsigned long long)g2_junk[i]);
    }
}
TEST(gap2_ido_junk_matches_deleteobject) {
    NEED_IDO();
    for (int i = 0; i < G2_NJUNK; i++) {
        BOOL a = U(DeleteObject)((HGDIOBJ)g2_junk[i]);
        BOOL b = IDO((HGDIOBJ)g2_junk[i]);
        CHECKF(a == b, "handle %#llx: DeleteObject %d internal %d", (unsigned long long)g2_junk[i], a, b);
    }
}
TEST(gap2_ido_stock_objects_survive) {
    NEED_IDO(); REQUIRE(U(GetStockObject));
    static const int ids[] = { WHITE_BRUSH, LTGRAY_BRUSH, GRAY_BRUSH, DKGRAY_BRUSH, BLACK_BRUSH, NULL_BRUSH,
                               WHITE_PEN, BLACK_PEN, NULL_PEN, OEM_FIXED_FONT, ANSI_FIXED_FONT, ANSI_VAR_FONT,
                               SYSTEM_FONT, DEVICE_DEFAULT_FONT, SYSTEM_FIXED_FONT, DEFAULT_GUI_FONT, DC_BRUSH, DC_PEN };
    for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        HGDIOBJ o = U(GetStockObject)(ids[i]);
        CHECKF(o != NULL, "stock %d", ids[i]);
        DWORD t = U(GetObjectType)(o);
        BOOL a = IDO(o);
        t_note("stock %d InternalDeleteObject -> %d", ids[i], a);
        G2_EQ(U(GetObjectType)(o), t);
        G2_EQ(U(GetStockObject)(ids[i]), o);
    }
}
TEST(gap2_ido_stock_default_palette) {
    NEED_IDO(); REQUIRE(U(GetStockObject));
    HGDIOBJ o = U(GetStockObject)(DEFAULT_PALETTE);
    CHECK(o != NULL);
    BOOL a = IDO(o);
    t_note("DEFAULT_PALETTE InternalDeleteObject -> %d", a);
    G2_EQ(U(GetObjectType)(o), OBJ_PAL);
}
TEST(gap2_ido_selected_bitmap_not_deleted) {
    NEED_IDO(); REQUIRE(U(SelectObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HBITMAP bm = U(CreateBitmap)(4, 4, 1, 32, NULL);
    HGDIOBJ old = U(SelectObject)(dc, bm);
    BOOL a = IDO(bm);
    t_note("InternalDeleteObject(selected bitmap) -> %d", a);
    if (!a) G2_EQ(U(GetObjectType)(bm), OBJ_BITMAP);
    U(SelectObject)(dc, old);
    if (U(GetObjectType)(bm)) CHECK(IDO(bm));
    G2_EQ(U(GetObjectType)(bm), 0);
    U(DeleteDC)(dc);
}
TEST(gap2_ido_selected_bitmap_matches_deleteobject) {
    NEED_IDO(); REQUIRE(U(SelectObject));
    BOOL res[2];
    for (int k = 0; k < 2; k++) {
        HDC dc = U(CreateCompatibleDC)(NULL);
        HBITMAP bm = U(CreateBitmap)(4, 4, 1, 32, NULL);
        HGDIOBJ old = U(SelectObject)(dc, bm);
        res[k] = k ? IDO(bm) : U(DeleteObject)(bm);
        U(SelectObject)(dc, old);
        if (U(GetObjectType)(bm)) U(DeleteObject)(bm);
        U(DeleteDC)(dc);
    }
    CHECKF(res[0] == res[1], "DeleteObject %d internal %d", res[0], res[1]);
}
TEST(gap2_ido_selected_pen_deferred) {
    NEED_IDO(); REQUIRE(U(SelectObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HPEN pen = U(CreatePen)(PS_SOLID, 1, RGB(255, 0, 0));
    HGDIOBJ old = U(SelectObject)(dc, pen);
    BOOL a = IDO(pen);
    t_note("InternalDeleteObject(selected pen) -> %d, type after %lu", a, U(GetObjectType)(pen));
    U(SelectObject)(dc, old);
    if (U(GetObjectType)(pen)) U(DeleteObject)(pen);
    G2_EQ(U(GetObjectType)(pen), 0);
    U(DeleteDC)(dc);
}
TEST(gap2_ido_selected_brush_deferred) {
    NEED_IDO(); REQUIRE(U(SelectObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HBRUSH br = U(CreateSolidBrush)(RGB(0, 255, 0));
    HGDIOBJ old = U(SelectObject)(dc, br);
    BOOL a = IDO(br);
    t_note("InternalDeleteObject(selected brush) -> %d", a);
    U(SelectObject)(dc, old);
    if (U(GetObjectType)(br)) U(DeleteObject)(br);
    G2_EQ(U(GetObjectType)(br), 0);
    U(DeleteDC)(dc);
}
TEST(gap2_ido_dc_handle_deletes_dc) {
    NEED_IDO(); REQUIRE(U(DeleteDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    BOOL r = IDO((HGDIOBJ)dc);
    t_note("InternalDeleteObject(memory DC) -> %d", r);
    if (U(GetObjectType)(dc) != 0) { U(DeleteDC)(dc); CHECKF(0, "DC survived InternalDeleteObject"); }
}
TEST(gap2_ido_enhmetafile_not_deleted) {
    NEED_IDO(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits));
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    U(Rectangle)(m, 0, 0, 5, 5);
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    CHECK(h != NULL); if (!h) return;
    BOOL r = IDO((HGDIOBJ)h);
    t_note("InternalDeleteObject(HENHMETAFILE) -> %d", r);
    CHECK(U(GetEnhMetaFileBits)(h, 0, NULL) > 0);   /* metafile handles are skipped by this path */
    CHECK(U(DeleteEnhMetaFile)(h));
    CHECK(U(GetEnhMetaFileBits)(h, 0, NULL) == 0);
}
TEST(gap2_ido_wmf_not_deleted) {
    NEED_IDO(); REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile)); REQUIRE(U(GetMetaFileBitsEx));
    HDC m = U(CreateMetaFileW)(NULL);
    if (!m) { t_skip("no WMF DC"); return; }
    U(Rectangle)(m, 0, 0, 5, 5);
    HMETAFILE h = U(CloseMetaFile)(m);
    CHECK(h != NULL); if (!h) return;
    BOOL r = IDO((HGDIOBJ)h);
    t_note("InternalDeleteObject(HMETAFILE) -> %d", r);
    CHECK(U(GetMetaFileBitsEx)(h, 0, NULL) > 0);
    CHECK(U(DeleteMetaFile)(h));
}
TEST(gap2_ido_colorspace) {
    NEED_IDO(); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(GetColorSpace));
    LOGCOLORSPACEW lcs; memset(&lcs, 0, sizeof lcs);
    lcs.lcsSignature = 0x50534F43; lcs.lcsVersion = 0x400; lcs.lcsSize = sizeof lcs;
    lcs.lcsCSType = 0x73524742; lcs.lcsIntent = LCS_GM_IMAGES;
    HCOLORSPACE cs = U(CreateColorSpaceW)(&lcs);
    if (!cs) { t_skip("CreateColorSpaceW failed"); return; }
    CHECK(IDO((HGDIOBJ)cs));
    SetLastError(0);
    CHECK(!IDO((HGDIOBJ)cs));
}
TEST(gap2_ido_gdi_count_many) {
    NEED_IDO();
    DWORD base = g2_gdi_count();
    HGDIOBJ o[300];
    for (int i = 0; i < 300; i++) o[i] = (i % 3 == 0) ? mk_pen() : (i % 3 == 1) ? mk_brush() : mk_rgn();
    EXPECT_GDI(base + 300);
    for (int i = 0; i < 300; i++) CHECK(IDO(o[i]));
    EXPECT_GDI(base);
}
TEST(gap2_ido_double_delete_each_type) {
    NEED_IDO();
    mk_fn fns[] = { mk_pen, mk_brush, mk_bmp, mk_rgn, mk_font, mk_pal };
    for (unsigned i = 0; i < sizeof fns / sizeof fns[0]; i++) {
        HGDIOBJ o = fns[i]();
        CHECK(IDO(o));
        CHECKF(!IDO(o), "type index %u deleted twice", i);
    }
}
TEST(gap2_ido_wrong_type_confusion_safe) {
    NEED_IDO();
    /* a region handle freed through the object path must not harm other live objects */
    HGDIOBJ keep = mk_brush();
    HGDIOBJ r = mk_rgn();
    CHECK(IDO(r));
    G2_EQ(U(GetObjectType)(keep), OBJ_BRUSH);
    CHECK(IDO(keep));
}

/* ------------------------------ MF_DeleteObject ------------------------------ */
#define NEED_MFD() do { REQUIRE(G32F(MF_DeleteObject)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectType)); } while (0)
TEST(gap2_mfdel_unlinked_objects_untouched) {
    NEED_MFD();
    mk_fn fns[] = { mk_pen, mk_brush, mk_bmp, mk_rgn, mk_font, mk_pal };
    DWORD ty[] = { OBJ_PEN, OBJ_BRUSH, OBJ_BITMAP, OBJ_REGION, OBJ_FONT, OBJ_PAL };
    for (unsigned i = 0; i < 6; i++) {
        HGDIOBJ o = fns[i]();
        CHECKF(G32F(MF_DeleteObject)(o) == 1, "type %u", i);
        G2_EQ(U(GetObjectType)(o), ty[i]);   /* only the metafile link is removed, never the object */
        CHECK(U(DeleteObject)(o));
    }
}
TEST(gap2_mfdel_null_and_junk) {
    NEED_MFD();
    for (int i = 0; i < G2_NJUNK; i++)
        CHECKF(G32F(MF_DeleteObject)((HGDIOBJ)g2_junk[i]) == 1, "handle %#llx", (unsigned long long)g2_junk[i]);
}
TEST(gap2_mfdel_stale_handle) {
    NEED_MFD();
    HGDIOBJ o = mk_brush(); U(DeleteObject)(o);
    CHECK(G32F(MF_DeleteObject)(o) == 1);
}
TEST(gap2_mfdel_repeatable) {
    NEED_MFD();
    HGDIOBJ o = mk_pen();
    for (int i = 0; i < 100; i++) if (G32F(MF_DeleteObject)(o) != 1) { CHECKF(0, "call %d", i); break; }
    G2_EQ(U(GetObjectType)(o), OBJ_PEN);
    U(DeleteObject)(o);
}
TEST(gap2_mfdel_stock_object) {
    NEED_MFD(); REQUIRE(U(GetStockObject));
    HGDIOBJ s = U(GetStockObject)(BLACK_PEN);
    CHECK(G32F(MF_DeleteObject)(s) == 1);
    G2_EQ(U(GetObjectType)(s), OBJ_PEN);
}
TEST(gap2_mfdel_no_gdi_leak) {
    NEED_MFD();
    DWORD base = g2_gdi_count();
    HGDIOBJ o = mk_brush();
    G32F(MF_DeleteObject)(o);
    EXPECT_GDI(base + 1);
    U(DeleteObject)(o);
    EXPECT_GDI(base);
}
static int count_emf_records(HENHMETAFILE h, DWORD type) {
    UINT n = U(GetEnhMetaFileBits)(h, 0, NULL);
    if (!n) return -1;
    BYTE *b = malloc(n);
    if (!b) return -1;
    U(GetEnhMetaFileBits)(h, n, b);
    int cnt = 0; DWORD off = 0;
    while (off + 8 <= n) {
        DWORD t = *(DWORD *)(b + off), sz = *(DWORD *)(b + off + 4);
        if (sz < 8 || off + sz > n) break;
        if (t == type) cnt++;
        off += sz;
    }
    free(b);
    return cnt;
}
TEST(gap2_mfdel_emf_linked_pen) {
    NEED_MFD(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits));
    SKIP_ON_WINE("MF_DeleteObject absent");
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    HPEN pen = U(CreatePen)(PS_SOLID, 3, RGB(200, 0, 0));
    HGDIOBJ old = U(SelectObject)(m, pen);
    U(MoveToEx)(m, 0, 0, NULL); U(LineTo)(m, 10, 10);
    U(SelectObject)(m, old);
    BOOL r = G32F(MF_DeleteObject)(pen);
    CHECK(r == 1);
    G2_EQ(U(GetObjectType)(pen), OBJ_PEN);
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    CHECK(h != NULL);
    if (h) {
        t_note("EMR_DELETEOBJECT records after MF_DeleteObject: %d", count_emf_records(h, EMR_DELETEOBJECT));
        U(DeleteEnhMetaFile)(h);
    }
    CHECK(U(DeleteObject)(pen));
}
TEST(gap2_mfdel_emf_deleteobject_records) {
    NEED_MFD(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile)); REQUIRE(U(GetEnhMetaFileBits));
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    HBRUSH br = U(CreateSolidBrush)(RGB(0, 0, 200));
    HGDIOBJ old = U(SelectObject)(m, br);
    U(Rectangle)(m, 0, 0, 10, 10);
    U(SelectObject)(m, old);
    CHECK(U(DeleteObject)(br));   /* goes through the metafile link cleanup */
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    CHECK(h != NULL);
    if (h) {
        int c = count_emf_records(h, EMR_CREATEBRUSHINDIRECT), d = count_emf_records(h, EMR_DELETEOBJECT);
        t_note("EMF: %d CREATEBRUSHINDIRECT, %d DELETEOBJECT", c, d);
        CHECK(c >= 1);
        CHECK(d >= 1);
        U(DeleteEnhMetaFile)(h);
    }
}

/* ------------------------------ MF16_DeleteObject ------------------------------ */
/* MF16_DeleteObject dereferences the 16-bit metafile link of its argument without a NULL check, so it is only
 * ever reached here through DeleteObject on an object that a WMF DC has used. */
TEST(gap2_mf16del_resolves) {
    REQUIRE(G32F(MF16_DeleteObject));
    CHECK(t_proc_gdi32("MF16_DeleteObject") == NULL);
}
static int wmf_has_record(HMETAFILE h, WORD fn) {
    UINT n = U(GetMetaFileBitsEx)(h, 0, NULL);
    if (n < 18) return -1;
    BYTE *b = malloc(n);
    if (!b) return -1;
    U(GetMetaFileBitsEx)(h, n, b);
    int cnt = 0; DWORD off = 18;   /* skip METAHEADER */
    while (off + 6 <= n) {
        DWORD sz = *(DWORD *)(b + off) * 2; WORD f = *(WORD *)(b + off + 4);
        if (sz < 6 || off + sz > n) break;
        if (f == fn) cnt++;
        off += sz;
    }
    free(b);
    return cnt;
}
TEST(gap2_mf16del_via_deleteobject_pen) {
    REQUIRE(G32F(MF16_DeleteObject)); REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile)); REQUIRE(U(GetMetaFileBitsEx));
    HDC m = U(CreateMetaFileW)(NULL);
    if (!m) { t_skip("no WMF DC"); return; }
    HPEN pen = U(CreatePen)(PS_SOLID, 1, RGB(0, 128, 0));
    HGDIOBJ old = U(SelectObject)(m, pen);
    U(MoveToEx)(m, 0, 0, NULL); U(LineTo)(m, 5, 5);
    U(SelectObject)(m, old);
    CHECK(U(DeleteObject)(pen));
    G2_EQ(U(GetObjectType)(pen), 0);
    HMETAFILE h = U(CloseMetaFile)(m);
    CHECK(h != NULL);
    if (h) {
        t_note("META_DELETEOBJECT records: %d, CREATEPENINDIRECT: %d", wmf_has_record(h, 0x01F0), wmf_has_record(h, 0x02FA));
        U(DeleteMetaFile)(h);
    }
}
TEST(gap2_mf16del_via_deleteobject_brush_font_region) {
    REQUIRE(G32F(MF16_DeleteObject)); REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    HDC m = U(CreateMetaFileW)(NULL);
    if (!m) { t_skip("no WMF DC"); return; }
    HBRUSH br = U(CreateSolidBrush)(RGB(0, 0, 128));
    HGDIOBJ f = mk_font();
    HGDIOBJ ob = U(SelectObject)(m, br), of = U(SelectObject)(m, f);
    U(Rectangle)(m, 0, 0, 5, 5);
    U(TextOutA)(m, 1, 1, "x", 1);
    U(SelectObject)(m, ob); U(SelectObject)(m, of);
    CHECK(U(DeleteObject)(br));
    CHECK(U(DeleteObject)(f));
    HMETAFILE h = U(CloseMetaFile)(m);
    CHECK(h != NULL);
    if (h) U(DeleteMetaFile)(h);
}
TEST(gap2_mf16del_selected_object_stays_until_deselected) {
    REQUIRE(G32F(MF16_DeleteObject)); REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    HDC m = U(CreateMetaFileW)(NULL);
    if (!m) { t_skip("no WMF DC"); return; }
    HPEN pen = U(CreatePen)(PS_SOLID, 1, 0);
    HGDIOBJ old = U(SelectObject)(m, pen);
    U(LineTo)(m, 3, 3);
    BOOL r = U(DeleteObject)(pen);   /* still selected into the WMF DC */
    t_note("DeleteObject(pen selected in WMF DC) -> %d", r);
    U(SelectObject)(m, old);
    if (U(GetObjectType)(pen)) CHECK(U(DeleteObject)(pen));
    HMETAFILE h = U(CloseMetaFile)(m);
    CHECK(h != NULL);
    if (h) U(DeleteMetaFile)(h);
}
TEST(gap2_mf16del_many_objects_gdi_balanced) {
    REQUIRE(G32F(MF16_DeleteObject)); REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    DWORD base = g2_gdi_count();
    HDC m = U(CreateMetaFileW)(NULL);
    if (!m) { t_skip("no WMF DC"); return; }
    HGDIOBJ old = NULL;
    for (int i = 0; i < 40; i++) {
        HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(i, i, i));
        HGDIOBJ o = U(SelectObject)(m, p);
        if (!old) old = o;
        U(LineTo)(m, i, i);
        U(SelectObject)(m, old);
        CHECK(U(DeleteObject)(p));
    }
    HMETAFILE h = U(CloseMetaFile)(m);
    CHECK(h != NULL);
    if (h) U(DeleteMetaFile)(h);
    EXPECT_GDI(base);
}
