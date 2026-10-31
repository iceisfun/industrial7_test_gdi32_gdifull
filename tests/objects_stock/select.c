/* SelectObject / SelectPalette / GetCurrentObject / DeleteObject rules / UnrealizeObject / EnumObjects. */
#include "os.h"

static HDC bad_dcs[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0x10000, (HDC)(ULONG_PTR)0x12345678, (HDC)(ULONG_PTR)0xdeadbeef, (HDC)(LONG_PTR)-1 };
#define NBADDC 6

/* ---- invalid arguments ---- */
TEST(select_invalid_dc) {
    REQUIRE(SOBJ); REQUIRE(GSO);
    HGDIOBJ objs[] = { GSO(BLACK_PEN), GSO(WHITE_BRUSH), GSO(SYSTEM_FONT) };
    for (int i = 0; i < NBADDC; i++)
        for (int j = 0; j < 3; j++) {
            SET_ERR(0);
            CHECKF(SOBJ(bad_dcs[i], objs[j]) == NULL, "dc %p obj %p", bad_dcs[i], objs[j]);
        }
}
TEST(select_invalid_dc_lasterror_note) {
    REQUIRE(SOBJ); REQUIRE(GSO);
    SET_ERR(0);
    SOBJ((HDC)(ULONG_PTR)0x12345678, GSO(BLACK_PEN));
    t_note("SelectObject(bad dc) err=%lu", GetLastError());
}
TEST(select_null_object) {
    REQUIRE(SOBJ);
    HDC dc = os_memdc(); REQUIRE(dc);
    HGDIOBJ before = U(GetCurrentObject)(dc, OBJ_PEN);
    SET_ERR(0);
    CHECK(SOBJ(dc, NULL) == NULL);
    t_note("SelectObject(dc, NULL) err=%lu", GetLastError());
    CHECK(U(GetCurrentObject)(dc, OBJ_PEN) == before);
    U(DeleteDC)(dc);
}
TEST(select_bogus_object) {
    REQUIRE(SOBJ);
    HDC dc = os_memdc(); REQUIRE(dc);
    ULONG_PTR bad[] = { 1, 0x10, 0xffff, 0x12345678, 0xdeadbeef, 0x100000000ull, 0xffffffffffffffffull };
    for (int i = 0; i < 7; i++) {
        SET_ERR(0);
        CHECKF(SOBJ(dc, (HGDIOBJ)bad[i]) == NULL, "obj %p", (void *)bad[i]);
    }
    CHECK(U(GetCurrentObject)(dc, OBJ_PEN) == GSO(BLACK_PEN));
    U(DeleteDC)(dc);
}
TEST(select_both_invalid) {
    REQUIRE(SOBJ);
    SET_ERR(0);
    CHECK(SOBJ(NULL, NULL) == NULL);
    CHECK(SOBJ((HDC)(ULONG_PTR)7, (HGDIOBJ)(ULONG_PTR)9) == NULL);
}
TEST(select_deleted_object) {
    REQUIRE(SOBJ);
    HDC dc = os_memdc(); REQUIRE(dc);
    int kinds[] = { OS_K_PEN, OS_K_SOLID, OS_K_FONT, OS_K_DDB, OS_K_RGN };
    for (int i = 0; i < 5; i++) {
        HGDIOBJ h = os_make(kinds[i]); REQUIRE(h);
        os_free(kinds[i], h);
        SET_ERR(0);
        HGDIOBJ r = SOBJ(dc, h);
        if (kinds[i] == OS_K_RGN) CHECKF(r == NULL || (ULONG_PTR)r <= 3 || r == (HGDIOBJ)(LONG_PTR)-1, "kind %s ret %p", os_kname[kinds[i]], r);
        else CHECKF(r == NULL, "kind %s ret %p", os_kname[kinds[i]], r);
    }
    U(DeleteDC)(dc);
}
TEST(select_dc_handle_as_object) {
    REQUIRE(SOBJ);
    HDC a = os_memdc(), b = os_memdc(); REQUIRE(a && b);
    SET_ERR(0);
    CHECK(SOBJ(a, b) == NULL);
    CHECK(SOBJ(a, a) == NULL);
    U(DeleteDC)(a); U(DeleteDC)(b);
}
TEST(select_metafile_as_object) {
    REQUIRE(SOBJ); REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    HDC dc = os_memdc(); REQUIRE(dc);
    HDC mdc = U(CreateMetaFileW)(NULL); REQUIRE(mdc);
    HMETAFILE mf = U(CloseMetaFile)(mdc); REQUIRE(mf);
    CHECK(SOBJ(dc, mf) == NULL);
    U(DeleteMetaFile)(mf);
    U(DeleteDC)(dc);
}
TEST(select_enhmetafile_as_object) {
    REQUIRE(SOBJ); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC dc = os_memdc(); REQUIRE(dc);
    HDC mdc = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL); REQUIRE(mdc);
    HENHMETAFILE mf = U(CloseEnhMetaFile)(mdc); REQUIRE(mf);
    CHECK(SOBJ(dc, mf) == NULL);
    U(DeleteEnhMetaFile)(mf);
    U(DeleteDC)(dc);
}
TEST(select_palette_via_selectobject_note) {
    REQUIRE(SOBJ);
    HDC dc = os_memdc(); REQUIRE(dc);
    HPALETTE p = os_pal(4); REQUIRE(p);
    SET_ERR(0);
    HGDIOBJ r = SOBJ(dc, p);
    t_note("SelectObject(palette) -> %p err=%lu (default pal %p)", r, GetLastError(), GSO(DEFAULT_PALETTE));
    CHECKF(U(GetCurrentObject)(dc, OBJ_PAL) != p || r != NULL, "palette selected yet NULL returned");
    if (U(GetCurrentObject)(dc, OBJ_PAL) == p) U(SelectPalette)(dc, GSO(DEFAULT_PALETTE), FALSE);
    DOBJ(p);
    U(DeleteDC)(dc);
}
TEST(select_colorspace_via_selectobject_note) {
    REQUIRE(SOBJ); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    LOGCOLORSPACEW l; memset(&l, 0, sizeof l);
    l.lcsSignature = 0x50534F43; l.lcsVersion = 0x400; l.lcsSize = sizeof l; l.lcsCSType = 0x73524742;
    HCOLORSPACE cs = U(CreateColorSpaceW)(&l);
    if (!cs) return;
    HDC dc = os_memdc(); REQUIRE(dc);
    SET_ERR(0);
    HGDIOBJ r = SOBJ(dc, cs);
    t_note("SelectObject(colorspace) -> %p err=%lu", r, GetLastError());
    if (r) SOBJ(dc, r);
    U(DeleteDC)(dc);
    U(DeleteColorSpace)(cs);
}

/* ---- return of previous object ---- */
#define PREV_TEST(nm, K, CUR, DEFEXPR) \
TEST(select_prev_##nm) { \
    REQUIRE(SOBJ); HDC dc = os_memdc(); REQUIRE(dc); \
    HGDIOBJ o = os_make(K); REQUIRE(o); \
    HGDIOBJ def = U(GetCurrentObject)(dc, CUR); \
    HGDIOBJ prev = SOBJ(dc, o); \
    CHECKF(prev == def, "prev %p default %p", prev, def); \
    DEFEXPR; \
    CHECK(U(GetCurrentObject)(dc, CUR) == o); \
    CHECK(SOBJ(dc, o) == o); \
    CHECK(SOBJ(dc, def) == o); \
    CHECK(U(GetCurrentObject)(dc, CUR) == def); \
    os_free(K, o); U(DeleteDC)(dc); }
PREV_TEST(pen, OS_K_PEN, OBJ_PEN, CHECK(prev == GSO(BLACK_PEN)))
PREV_TEST(solidbrush, OS_K_SOLID, OBJ_BRUSH, CHECK(prev == GSO(WHITE_BRUSH)))
PREV_TEST(hatchbrush, OS_K_HATCH, OBJ_BRUSH, CHECK(prev == GSO(WHITE_BRUSH)))
PREV_TEST(patternbrush, OS_K_PATTERN, OBJ_BRUSH, CHECK(prev == GSO(WHITE_BRUSH)))
PREV_TEST(font, OS_K_FONT, OBJ_FONT, (void)0)
PREV_TEST(ddb, OS_K_DDB, OBJ_BITMAP, CHECK(GOT(prev) == OBJ_BITMAP))
PREV_TEST(dib, OS_K_DIB, OBJ_BITMAP, CHECK(GOT(prev) == OBJ_BITMAP))

TEST(select_prev_extpen) {
    REQUIRE(SOBJ);
    HDC dc = os_memdc(); REQUIRE(dc);
    HGDIOBJ p = os_make(OS_K_EXTPEN); REQUIRE(p);
    HGDIOBJ prev = SOBJ(dc, p);
    CHECK(prev == GSO(BLACK_PEN));
    HGDIOBJ c1 = U(GetCurrentObject)(dc, OBJ_PEN), c2 = U(GetCurrentObject)(dc, OBJ_EXTPEN);
    t_note("extpen selected: OBJ_PEN->%p OBJ_EXTPEN->%p (pen %p)", c1, c2, p);
    CHECK(c1 == p || c2 == p);
    CHECK(SOBJ(dc, prev) == p);
    os_free(OS_K_EXTPEN, p);
    U(DeleteDC)(dc);
}
TEST(select_every_stock_pen_brush_font) {
    REQUIRE(SOBJ); REQUIRE(GSO);
    HDC dc = os_memdc(); REQUIRE(dc);
    int idx[] = { WHITE_BRUSH, LTGRAY_BRUSH, GRAY_BRUSH, DKGRAY_BRUSH, BLACK_BRUSH, NULL_BRUSH, DC_BRUSH };
    HGDIOBJ prev = GSO(WHITE_BRUSH);
    for (int i = 0; i < 7; i++) {
        HGDIOBJ r = SOBJ(dc, GSO(idx[i]));
        CHECKF(r == prev, "brush %d prev %p want %p", idx[i], r, prev);
        prev = GSO(idx[i]);
    }
    int pi[] = { WHITE_PEN, BLACK_PEN, NULL_PEN, DC_PEN };
    prev = GSO(BLACK_PEN);
    for (int i = 0; i < 4; i++) {
        HGDIOBJ r = SOBJ(dc, GSO(pi[i]));
        CHECKF(r == prev, "pen %d", pi[i]);
        prev = GSO(pi[i]);
    }
    U(DeleteDC)(dc);
}
TEST(select_sequence_chain) {
    REQUIRE(SOBJ);
    HDC dc = os_memdc(); REQUIRE(dc);
    HGDIOBJ p[5]; HGDIOBJ orig = GSO(BLACK_PEN);
    for (int i = 0; i < 5; i++) p[i] = U(CreatePen)(PS_SOLID, i, i);
    HGDIOBJ prev = orig;
    for (int i = 0; i < 5; i++) { CHECK(SOBJ(dc, p[i]) == prev); prev = p[i]; }
    CHECK(SOBJ(dc, orig) == p[4]);
    for (int i = 0; i < 5; i++) CHECK(DOBJ(p[i]));
    U(DeleteDC)(dc);
}
TEST(select_type_checks_cross_slot) {
    REQUIRE(SOBJ);
    HDC dc = os_memdc(); REQUIRE(dc);
    HGDIOBJ pen = os_make(OS_K_PEN), br = os_make(OS_K_SOLID);
    SOBJ(dc, pen); SOBJ(dc, br);
    CHECK(U(GetCurrentObject)(dc, OBJ_PEN) == pen);
    CHECK(U(GetCurrentObject)(dc, OBJ_BRUSH) == br);
    SOBJ(dc, GSO(BLACK_PEN)); SOBJ(dc, GSO(WHITE_BRUSH));
    DOBJ(pen); DOBJ(br);
    U(DeleteDC)(dc);
}

/* ---- bitmap rules ---- */
TEST(select_bitmap_into_two_dcs) {
    REQUIRE(SOBJ);
    HDC a = os_memdc(), b = os_memdc(); REQUIRE(a && b);
    HBITMAP bm = U(CreateBitmap)(4, 4, 1, 1, NULL); REQUIRE(bm);
    HGDIOBJ oa = SOBJ(a, bm);
    CHECK(oa != NULL);
    SET_ERR(0);
    CHECK(SOBJ(b, bm) == NULL);
    t_note("second select err=%lu", GetLastError());
    CHECK(U(GetCurrentObject)(b, OBJ_BITMAP) != bm);
    CHECK(U(GetCurrentObject)(a, OBJ_BITMAP) == bm);
    SOBJ(a, oa);
    HGDIOBJ ob = SOBJ(b, bm);
    CHECK(ob != NULL);
    SOBJ(b, ob);
    DOBJ(bm);
    U(DeleteDC)(a); U(DeleteDC)(b);
}
TEST(select_dib_into_two_dcs) {
    REQUIRE(SOBJ);
    HDC a = os_memdc(), b = os_memdc(); REQUIRE(a && b);
    void *bits; HBITMAP bm = os_dib(NULL, 4, 4, 32, &bits); REQUIRE(bm);
    HGDIOBJ oa = SOBJ(a, bm);
    CHECK(oa != NULL);
    CHECK(SOBJ(b, bm) == NULL);
    SOBJ(a, oa);
    DOBJ(bm);
    U(DeleteDC)(a); U(DeleteDC)(b);
}
TEST(select_bitmap_same_dc_twice) {
    REQUIRE(SOBJ);
    HDC a = os_memdc(); REQUIRE(a);
    HBITMAP bm = U(CreateBitmap)(4, 4, 1, 1, NULL); REQUIRE(bm);
    HGDIOBJ o = SOBJ(a, bm);
    CHECK(SOBJ(a, bm) == bm);
    SOBJ(a, o);
    DOBJ(bm);
    U(DeleteDC)(a);
}
TEST(select_bitmap_into_screen_dc) {
    REQUIRE(SOBJ); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC s = U(GetDC)(NULL); REQUIRE(s);
    HBITMAP bm = U(CreateBitmap)(4, 4, 1, 1, NULL); REQUIRE(bm);
    SET_ERR(0);
    CHECK(SOBJ(s, bm) == NULL);
    DOBJ(bm);
    U(ReleaseDC)(NULL, s);
}
TEST(select_bitmap_into_createdc_display) {
    REQUIRE(SOBJ); REQUIRE(U(CreateDCW));
    HDC s = U(CreateDCW)(L"DISPLAY", NULL, NULL, NULL); REQUIRE(s);
    HBITMAP bm = U(CreateBitmap)(4, 4, 1, 1, NULL); REQUIRE(bm);
    CHECK(SOBJ(s, bm) == NULL);
    DOBJ(bm);
    U(DeleteDC)(s);
}
TEST(select_bitmap_into_info_dc) {
    REQUIRE(SOBJ); REQUIRE(U(CreateICW));
    HDC s = U(CreateICW)(L"DISPLAY", NULL, NULL, NULL); REQUIRE(s);
    HBITMAP bm = U(CreateBitmap)(4, 4, 1, 1, NULL); REQUIRE(bm);
    CHECK(SOBJ(s, bm) == NULL);
    DOBJ(bm);
    U(DeleteDC)(s);
}
TEST(select_bitmap_into_metafile_dc) {
    REQUIRE(SOBJ); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL); REQUIRE(m);
    HBITMAP bm = U(CreateBitmap)(4, 4, 1, 1, NULL); REQUIRE(bm);
    SET_ERR(0);
    HGDIOBJ r = SOBJ(m, bm);
    t_note("SelectObject(enhmetadc, bitmap) -> %p err=%lu", r, GetLastError());
    if (r) SOBJ(m, r);
    DOBJ(bm);
    U(DeleteEnhMetaFile)(U(CloseEnhMetaFile)(m));
}
TEST(select_bitmap_from_window_dc) {
    REQUIRE(SOBJ); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HWND w = t_make_window(NULL, WS_POPUP, 0, 0, 0, 10, 10, NULL);
    if (!w) { t_skip("no window"); return; }
    HDC s = U(GetDC)(w);
    HBITMAP bm = U(CreateBitmap)(4, 4, 1, 1, NULL);
    CHECK(SOBJ(s, bm) == NULL);
    DOBJ(bm);
    U(ReleaseDC)(w, s);
    U(DestroyWindow)(w);
}
TEST(select_bitmap_depths_into_memdc) {
    REQUIRE(SOBJ);
    int bpps[] = { 1, 8, 16, 32 };
    for (int i = 0; i < 4; i++) {
        HDC dc = os_memdc(); REQUIRE(dc);
        HBITMAP bm = U(CreateBitmap)(7, 5, 1, bpps[i], NULL);
        HGDIOBJ o = SOBJ(dc, bm);
        if (bpps[i] == 1 || bpps[i] == 32) CHECKF(o != NULL, "bpp %d", bpps[i]);
        else t_note("select %d bpp DDB into memdc -> %p", bpps[i], o);
        if (o) SOBJ(dc, o);
        DOBJ(bm);
        U(DeleteDC)(dc);
    }
}
TEST(select_bitmap_caps_follow_bitmap) {
    REQUIRE(SOBJ); REQUIRE(U(GetDeviceCaps));
    HDC dc = os_memdc(); REQUIRE(dc);
    void *bits; HBITMAP b24 = os_dib(NULL, 10, 6, 24, &bits); REQUIRE(b24);
    HGDIOBJ o = SOBJ(dc, b24);
    CHECK(U(GetDeviceCaps)(dc, BITSPIXEL) == 24 || os_wine());
    CHECK(U(GetDeviceCaps)(dc, PLANES) == 1);
    t_note("memdc with 10x6 DIB: HORZRES=%d VERTRES=%d", U(GetDeviceCaps)(dc, HORZRES), U(GetDeviceCaps)(dc, VERTRES));
    SOBJ(dc, o);
    DOBJ(b24);
    U(DeleteDC)(dc);
}

/* ---- regions ---- */
TEST(select_region_results) {
    REQUIRE(SOBJ); REQUIRE(U(CreateRectRgn));
    HDC dc = os_memdc(); REQUIRE(dc);
    HRGN rect = U(CreateRectRgn)(0, 0, 1, 1);
    HRGN empty = U(CreateRectRgn)(0, 0, 0, 0);
    HRGN a = U(CreateRectRgn)(0, 0, 1, 1), b = U(CreateRectRgn)(0, 0, 1, 1), u = U(CreateRectRgn)(0, 0, 0, 0);
    U(SetRectRgn)(b, 5, 5, 8, 8);
    U(CombineRgn)(u, a, b, RGN_OR);
    HGDIOBJ r;
    r = SOBJ(dc, rect);    CHECKF((ULONG_PTR)r == SIMPLEREGION, "rect -> %p", r);
    r = SOBJ(dc, empty);   CHECKF((ULONG_PTR)r == NULLREGION, "empty -> %p", r);
    r = SOBJ(dc, u);       CHECKF((ULONG_PTR)r == COMPLEXREGION, "complex -> %p", r);
    r = SOBJ(dc, a);       CHECKF((ULONG_PTR)r == SIMPLEREGION, "rect2 -> %p", r);
    U(SelectClipRgn)(dc, NULL);
    CHECK(DOBJ(rect) && DOBJ(empty) && DOBJ(a) && DOBJ(b) && DOBJ(u));
    U(DeleteDC)(dc);
}
TEST(select_region_not_retained) {
    REQUIRE(SOBJ);
    HDC dc = os_memdc(); REQUIRE(dc);
    void *bits; HBITMAP big = os_dib(NULL, 64, 64, 32, &bits); REQUIRE(big);
    HGDIOBJ obm = SOBJ(dc, big);
    HRGN r = os_rgn(); REQUIRE(r);
    SOBJ(dc, r);
    CHECK(DOBJ(r));
    CHECK(GOT(r) == 0);
    RECT rc; memset(&rc, 0, sizeof rc);
    int t = U(GetClipBox)(dc, &rc);
    CHECK(t == SIMPLEREGION);
    CHECK(rc.left == 1 && rc.top == 2 && rc.right == 11 && rc.bottom == 12);
    SOBJ(dc, obm); DOBJ(big);
    U(DeleteDC)(dc);
}
TEST(select_region_on_screen_dc) {
    REQUIRE(SOBJ); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC s = U(GetDC)(NULL); REQUIRE(s);
    HRGN r = os_rgn();
    HGDIOBJ x = SOBJ(s, r);
    t_note("select region into screen DC -> %p", x);
    CHECK(x != NULL);
    U(SelectClipRgn)(s, NULL);
    DOBJ(r);
    U(ReleaseDC)(NULL, s);
}
TEST(select_region_invalid_dc) {
    REQUIRE(SOBJ);
    HRGN r = os_rgn(); REQUIRE(r);
    for (int i = 0; i < NBADDC; i++) {
        HGDIOBJ x = SOBJ(bad_dcs[i], r);
        CHECKF(x == NULL || x == (HGDIOBJ)(LONG_PTR)-1 || 0, "dc %p -> %p", bad_dcs[i], x);
    }
    DOBJ(r);
}

/* ---- fonts ---- */
TEST(select_font_current_matches) {
    REQUIRE(SOBJ);
    HDC dc = os_memdc(); REQUIRE(dc);
    HFONT f = os_font("Arial", -20); REQUIRE(f);
    HGDIOBJ old = SOBJ(dc, f);
    CHECK(U(GetCurrentObject)(dc, OBJ_FONT) == f);
    TEXTMETRICA tm; memset(&tm, 0, sizeof tm);
    CHECK(U(GetTextMetricsA)(dc, &tm));
    CHECK(tm.tmHeight > 0);
    SOBJ(dc, old);
    DOBJ(f);
    U(DeleteDC)(dc);
}
TEST(select_font_two_dcs_allowed) {
    REQUIRE(SOBJ);
    HDC a = os_memdc(), b = os_memdc(); REQUIRE(a && b);
    HFONT f = os_font("Arial", -12); REQUIRE(f);
    HGDIOBJ oa = SOBJ(a, f), ob = SOBJ(b, f);
    CHECK(oa != NULL && ob != NULL);
    CHECK(U(GetCurrentObject)(a, OBJ_FONT) == f && U(GetCurrentObject)(b, OBJ_FONT) == f);
    SOBJ(a, oa); SOBJ(b, ob);
    DOBJ(f);
    U(DeleteDC)(a); U(DeleteDC)(b);
}
TEST(select_pen_brush_two_dcs_allowed) {
    REQUIRE(SOBJ);
    HDC a = os_memdc(), b = os_memdc(); REQUIRE(a && b);
    HGDIOBJ p = os_make(OS_K_PEN), br = os_make(OS_K_SOLID);
    HGDIOBJ pa = SOBJ(a, p), pb = SOBJ(b, p), ba = SOBJ(a, br), bb = SOBJ(b, br);
    CHECK(pa && pb && ba && bb);
    SOBJ(a, pa); SOBJ(b, pb); SOBJ(a, ba); SOBJ(b, bb);
    DOBJ(p); DOBJ(br);
    U(DeleteDC)(a); U(DeleteDC)(b);
}

/* ---- SelectPalette ---- */
TEST(selectpal_basic) {
    REQUIRE(U(SelectPalette)); REQUIRE(GSO);
    HDC dc = os_memdc(); REQUIRE(dc);
    HPALETTE p = os_pal(8); REQUIRE(p);
    HPALETTE prev = U(SelectPalette)(dc, p, FALSE);
    CHECK(prev == GSO(DEFAULT_PALETTE));
    CHECK(U(GetCurrentObject)(dc, OBJ_PAL) == p);
    CHECK(U(SelectPalette)(dc, prev, FALSE) == p);
    DOBJ(p);
    U(DeleteDC)(dc);
}
TEST(selectpal_invalid) {
    REQUIRE(U(SelectPalette));
    HDC dc = os_memdc(); REQUIRE(dc);
    HPALETTE p = os_pal(4);
    for (int i = 0; i < NBADDC; i++) CHECKF(U(SelectPalette)(bad_dcs[i], p, FALSE) == NULL, "dc %p", bad_dcs[i]);
    SET_ERR(0);
    CHECK(U(SelectPalette)(dc, NULL, FALSE) == NULL);
    CHECK(U(SelectPalette)(dc, (HPALETTE)(ULONG_PTR)0x1234, FALSE) == NULL);
    HPEN pen = os_make(OS_K_PEN);
    CHECK(U(SelectPalette)(dc, (HPALETTE)pen, FALSE) == NULL);
    DOBJ(pen);
    DOBJ(p);
    U(DeleteDC)(dc);
}
TEST(selectpal_two_dcs) {
    REQUIRE(U(SelectPalette));
    HDC a = os_memdc(), b = os_memdc(); REQUIRE(a && b);
    HPALETTE p = os_pal(4);
    HPALETTE pa = U(SelectPalette)(a, p, FALSE), pb = U(SelectPalette)(b, p, FALSE);
    t_note("palette in two DCs: %p %p", pa, pb);
    U(SelectPalette)(a, pa, FALSE);
    if (pb) U(SelectPalette)(b, pb, FALSE);
    DOBJ(p);
    U(DeleteDC)(a); U(DeleteDC)(b);
}

/* ---- GetCurrentObject ---- */
TEST(getcurrent_valid_types) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(GSO);
    HDC dc = os_memdc(); REQUIRE(dc);
    CHECK(U(GetCurrentObject)(dc, OBJ_PEN) == GSO(BLACK_PEN));
    CHECK(U(GetCurrentObject)(dc, OBJ_BRUSH) == GSO(WHITE_BRUSH));
    CHECK(U(GetCurrentObject)(dc, OBJ_PAL) == GSO(DEFAULT_PALETTE));
    CHECK(GOT(U(GetCurrentObject)(dc, OBJ_FONT)) == OBJ_FONT);
    CHECK(GOT(U(GetCurrentObject)(dc, OBJ_BITMAP)) == OBJ_BITMAP);
    t_note("OBJ_COLORSPACE -> %p OBJ_EXTPEN -> %p", U(GetCurrentObject)(dc, OBJ_COLORSPACE), U(GetCurrentObject)(dc, OBJ_EXTPEN));
    U(DeleteDC)(dc);
}
TEST(getcurrent_invalid_types) {
    REQUIRE(U(GetCurrentObject));
    HDC dc = os_memdc(); REQUIRE(dc);
    UINT bad[] = { 0, OBJ_DC, OBJ_METADC, OBJ_REGION, OBJ_METAFILE, OBJ_MEMDC, OBJ_ENHMETADC, OBJ_ENHMETAFILE, 15, 16, 100, 0xffff, 0xffffffff };
    for (int i = 0; i < 13; i++) {
        SET_ERR(0);
        HGDIOBJ r = U(GetCurrentObject)(dc, bad[i]);
        DWORD e_ = GetLastError();
        CHECKF(r == NULL, "type %u -> %p", bad[i], r);
        CHECKF(e_ == ERROR_INVALID_PARAMETER || os_wine(), "type %u err %lu", bad[i], e_);
    }
    U(DeleteDC)(dc);
}
TEST(getcurrent_invalid_dc) {
    REQUIRE(U(GetCurrentObject));
    UINT t[] = { OBJ_PEN, OBJ_BRUSH, OBJ_PAL, OBJ_FONT, OBJ_BITMAP };
    for (int i = 0; i < NBADDC; i++) for (int j = 0; j < 5; j++) {
        SET_ERR(0);
        CHECKF(U(GetCurrentObject)(bad_dcs[i], t[j]) == NULL, "dc %p type %u", bad_dcs[i], t[j]);
    }
}
TEST(getcurrent_screen_dc) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC s = U(GetDC)(NULL); REQUIRE(s);
    CHECK(U(GetCurrentObject)(s, OBJ_PEN) == GSO(BLACK_PEN));
    CHECK(U(GetCurrentObject)(s, OBJ_BRUSH) == GSO(WHITE_BRUSH));
    HGDIOBJ b = U(GetCurrentObject)(s, OBJ_BITMAP);
    t_note("screen DC bitmap %p type %lu", b, (unsigned long)GOT(b));
    U(ReleaseDC)(NULL, s);
}

/* ---- DeleteObject rules ---- */
TEST(delete_invalid) {
    REQUIRE(DOBJ);
    ULONG_PTR bad[] = { 0, 1, 0x10, 0xffff, 0x12345678, 0xdeadbeef, 0x100000000ull, 0xffffffffffffffffull };
    for (int i = 0; i < 8; i++) {
        SET_ERR(0);
        CHECKF(!DOBJ((HGDIOBJ)bad[i]), "bogus %p", (void *)bad[i]);
        t_note("DeleteObject(%p) err=%lu", (void *)bad[i], GetLastError());
    }
}
TEST(delete_returns_true_then_false) {
    REQUIRE(DOBJ);
    for (int k = OS_K_PEN; k <= OS_K_DIB; k++) {
        if (k == OS_K_NULLBR) continue;
        HGDIOBJ h = os_make(k); if (!h) continue;
        CHECKF(DOBJ(h), "%s first", os_kname[k]);
        SET_ERR(0);
        CHECKF(!DOBJ(h), "%s second", os_kname[k]);
    }
}
TEST(delete_second_lasterror_note) {
    REQUIRE(DOBJ);
    HGDIOBJ h = os_make(OS_K_PEN); DOBJ(h);
    SET_ERR(0); DOBJ(h);
    t_note("second DeleteObject(pen) err=%lu", GetLastError());
}
TEST(delete_selected_bitmap_fails) {
    REQUIRE(DOBJ);
    HDC dc = os_memdc(); REQUIRE(dc);
    HBITMAP bm = U(CreateBitmap)(4, 4, 1, 1, NULL); REQUIRE(bm);
    HGDIOBJ old = SOBJ(dc, bm);
    SET_ERR(0);
    BOOL r = DOBJ(bm);
    CHECK(!r || os_wine());
    t_note("delete selected bitmap -> %d err=%lu", r, GetLastError());
    if (r) { SOBJ(dc, old); U(DeleteDC)(dc); return; }
    CHECK(GOT(bm) == OBJ_BITMAP);
    BITMAP b2; CHECK(GOA(bm, sizeof b2, &b2) == (int)sizeof b2);
    SOBJ(dc, old);
    CHECK(DOBJ(bm));
    CHECK(GOT(bm) == 0);
    U(DeleteDC)(dc);
}
TEST(delete_selected_dib_fails) {
    REQUIRE(DOBJ);
    HDC dc = os_memdc(); REQUIRE(dc);
    void *bits; HBITMAP bm = os_dib(NULL, 4, 4, 32, &bits); REQUIRE(bm);
    HGDIOBJ old = SOBJ(dc, bm);
    BOOL r = DOBJ(bm);
    CHECK(!r || os_wine());
    if (!r) { CHECK(GOT(bm) == OBJ_BITMAP); }
    SOBJ(dc, old);
    if (!r) CHECK(DOBJ(bm));
    U(DeleteDC)(dc);
}
#define SEL_DEL_TEST(nm, K, CUR, STOCKIDX) \
TEST(delete_selected_##nm) { \
    REQUIRE(DOBJ); HDC dc = os_memdc(); REQUIRE(dc); \
    HGDIOBJ o = os_make(K); REQUIRE(o); \
    HGDIOBJ old = SOBJ(dc, o); \
    SET_ERR(0); BOOL r1 = DOBJ(o); \
    t_note(#nm " delete-while-selected -> %d err=%lu type-after=%lu", r1, GetLastError(), (unsigned long)GOT(o)); \
    SOBJ(dc, old); \
    if (GOT(o)) { CHECK(DOBJ(o)); } \
    else CHECK(r1); \
    CHECK(GOT(o) == 0); \
    CHECK(U(GetCurrentObject)(dc, CUR) == old); \
    U(DeleteDC)(dc); }
SEL_DEL_TEST(pen, OS_K_PEN, OBJ_PEN, BLACK_PEN)
SEL_DEL_TEST(extpen, OS_K_EXTPEN, OBJ_PEN, BLACK_PEN)
SEL_DEL_TEST(solidbrush, OS_K_SOLID, OBJ_BRUSH, WHITE_BRUSH)
SEL_DEL_TEST(hatchbrush, OS_K_HATCH, OBJ_BRUSH, WHITE_BRUSH)
SEL_DEL_TEST(patternbrush, OS_K_PATTERN, OBJ_BRUSH, WHITE_BRUSH)
SEL_DEL_TEST(font, OS_K_FONT, OBJ_FONT, SYSTEM_FONT)

TEST(delete_selected_pen_dc_falls_back_note) {
    REQUIRE(DOBJ);
    HDC dc = os_memdc(); REQUIRE(dc);
    HGDIOBJ p = os_make(OS_K_PEN);
    HGDIOBJ old = SOBJ(dc, p);
    BOOL r = DOBJ(p);
    HGDIOBJ cur = U(GetCurrentObject)(dc, OBJ_PEN);
    t_note("after DeleteObject(selected pen)=%d, current pen=%p (deleted=%p, old=%p)", r, cur, p, old);
    SOBJ(dc, old);
    if (GOT(p)) DOBJ(p);
    U(DeleteDC)(dc);
}
TEST(delete_object_survives_dc_deletion) {
    REQUIRE(DOBJ);
    int kinds[] = { OS_K_PEN, OS_K_SOLID, OS_K_FONT, OS_K_DDB, OS_K_DIB };
    for (int i = 0; i < 5; i++) {
        HDC dc = os_memdc(); REQUIRE(dc);
        HGDIOBJ o = os_make(kinds[i]); REQUIRE(o);
        SOBJ(dc, o);
        CHECK(U(DeleteDC)(dc));
        CHECKF(GOT(o) == os_ktype[kinds[i]], "%s after DeleteDC type %lu", os_kname[kinds[i]], (unsigned long)GOT(o));
        CHECKF(DOBJ(o), "%s deletable after DeleteDC", os_kname[kinds[i]]);
    }
}
TEST(delete_bitmap_deselected_by_deletedc) {
    REQUIRE(DOBJ);
    HDC dc = os_memdc(); REQUIRE(dc);
    HBITMAP bm = U(CreateBitmap)(4, 4, 1, 1, NULL);
    SOBJ(dc, bm);
    U(DeleteDC)(dc);
    HDC dc2 = os_memdc();
    HGDIOBJ o = SOBJ(dc2, bm);
    CHECK(o != NULL);
    SOBJ(dc2, o);
    CHECK(DOBJ(bm));
    U(DeleteDC)(dc2);
}
TEST(delete_stock_font_pen_brush_in_dc) {
    REQUIRE(DOBJ); REQUIRE(GSO);
    HDC dc = os_memdc(); REQUIRE(dc);
    int idx[] = { BLACK_PEN, WHITE_BRUSH, SYSTEM_FONT };
    for (int i = 0; i < 3; i++) {
        HGDIOBJ old = SOBJ(dc, GSO(idx[i]));
        t_note("DeleteObject(selected stock %d)=%d", idx[i], DOBJ(GSO(idx[i])));
        CHECK(GOT(GSO(idx[i])) != 0);
        SOBJ(dc, old);
    }
    U(DeleteDC)(dc);
}
TEST(delete_region_selected_clip) {
    REQUIRE(DOBJ);
    HDC dc = os_memdc(); REQUIRE(dc);
    void *bits; HBITMAP big = os_dib(NULL, 64, 64, 32, &bits); REQUIRE(big);
    HGDIOBJ obm = SOBJ(dc, big);
    HRGN r = os_rgn();
    U(SelectClipRgn)(dc, r);
    CHECK(DOBJ(r));
    RECT rc; CHECK(U(GetClipBox)(dc, &rc) == SIMPLEREGION);
    SOBJ(dc, obm); DOBJ(big);
    U(DeleteDC)(dc);
}
TEST(delete_palette_selected_note) {
    REQUIRE(DOBJ);
    HDC dc = os_memdc(); REQUIRE(dc);
    HPALETTE p = os_pal(8);
    HPALETTE old = U(SelectPalette)(dc, p, FALSE);
    SET_ERR(0);
    BOOL r = DOBJ(p);
    t_note("delete selected palette -> %d err=%lu type-after=%lu", r, GetLastError(), (unsigned long)GOT(p));
    U(SelectPalette)(dc, old, FALSE);
    if (GOT(p)) DOBJ(p);
    U(DeleteDC)(dc);
}
TEST(delete_dc_via_deleteobject_wrong_kind) {
    REQUIRE(DOBJ);
    HDC dc = os_memdc(); REQUIRE(dc);
    BOOL r = DOBJ(dc);
    t_note("DeleteObject(memdc)=%d err=%lu", r, GetLastError());
    if (GOT(dc)) U(DeleteDC)(dc);
}
TEST(deletedc_on_non_dc_objects) {
    REQUIRE(U(DeleteDC));
    for (int k = OS_K_PEN; k <= OS_K_DIB; k++) {
        HGDIOBJ h = os_make(k); if (!h) continue;
        SET_ERR(0);
        CHECKF(!U(DeleteDC)((HDC)h), "%s", os_kname[k]);
        CHECKF(GOT(h) != 0 || k == OS_K_NULLBR, "%s destroyed by DeleteDC", os_kname[k]);
        os_free(k, h);
    }
}
TEST(deletedc_invalid) {
    REQUIRE(U(DeleteDC));
    for (int i = 0; i < NBADDC; i++) { SET_ERR(0); CHECKF(!U(DeleteDC)(bad_dcs[i]), "%p", bad_dcs[i]); }
}
TEST(deletedc_twice) {
    REQUIRE(U(DeleteDC));
    HDC dc = os_memdc(); REQUIRE(dc);
    CHECK(U(DeleteDC)(dc));
    SET_ERR(0);
    CHECK(!U(DeleteDC)(dc));
}

/* ---- UnrealizeObject ---- */
TEST(unrealize_brush) {
    REQUIRE(U(UnrealizeObject));
    HGDIOBJ b = os_make(OS_K_SOLID); REQUIRE(b);
    CHECK(U(UnrealizeObject)(b));
    HGDIOBJ h = os_make(OS_K_HATCH);
    CHECK(U(UnrealizeObject)(h));
    HGDIOBJ p = os_make(OS_K_PATTERN);
    CHECK(U(UnrealizeObject)(p));
    DOBJ(b); DOBJ(h); DOBJ(p);
}
TEST(unrealize_palette) {
    REQUIRE(U(UnrealizeObject));
    HPALETTE p = os_pal(8); REQUIRE(p);
    CHECK(U(UnrealizeObject)(p));
    CHECK(U(UnrealizeObject)(GSO(DEFAULT_PALETTE)) || 1);
    DOBJ(p);
}
TEST(unrealize_other_types_note) {
    REQUIRE(U(UnrealizeObject));
    for (int k = OS_K_PEN; k <= OS_K_MEMDC; k++) {
        if (k == OS_K_SOLID || k == OS_K_HATCH || k == OS_K_PATTERN || k == OS_K_PAL) continue;
        HGDIOBJ h = os_make(k); if (!h) continue;
        SET_ERR(0);
        BOOL r = U(UnrealizeObject)(h);
        t_note("UnrealizeObject(%s)=%d err=%lu", os_kname[k], r, GetLastError());
        CHECKF(GOT(h) != 0, "%s destroyed", os_kname[k]);
        os_free(k, h);
    }
}
TEST(unrealize_invalid) {
    REQUIRE(U(UnrealizeObject));
    ULONG_PTR bad[] = { 0, 1, 0x1234, 0xdeadbeef, 0xffffffffffffffffull };
    for (int i = 0; i < 5; i++) { SET_ERR(0); CHECKF(!U(UnrealizeObject)((HGDIOBJ)bad[i]), "%p", (void *)bad[i]); }
}
TEST(unrealize_selected_brush) {
    REQUIRE(U(UnrealizeObject));
    HDC dc = os_memdc(); REQUIRE(dc);
    HGDIOBJ b = os_make(OS_K_SOLID);
    HGDIOBJ old = SOBJ(dc, b);
    t_note("UnrealizeObject(selected brush)=%d", U(UnrealizeObject)(b));
    CHECK(U(GetCurrentObject)(dc, OBJ_BRUSH) == b);
    SOBJ(dc, old);
    DOBJ(b);
    U(DeleteDC)(dc);
}

/* ---- EnumObjects ---- */
struct enum_ctx { int count; int stop_at; int kind; UINT styles[64]; COLORREF cols[64]; LPARAM lp; };
static int CALLBACK enum_cb(LPVOID obj, LPARAM lp) {
    struct enum_ctx *c = (struct enum_ctx *)lp;
    if (c->count < 64) {
        if (c->kind == OBJ_PEN) { LOGPEN *p = obj; c->styles[c->count] = p->lopnStyle; c->cols[c->count] = p->lopnColor; }
        else { LOGBRUSH *b = obj; c->styles[c->count] = b->lbStyle; c->cols[c->count] = b->lbColor; }
    }
    c->count++;
    return (c->stop_at && c->count >= c->stop_at) ? 0 : 1;
}
TEST(enumobj_pens) {
    REQUIRE(U(EnumObjects)); HDC dc = os_memdc(); REQUIRE(dc);
    struct enum_ctx c; memset(&c, 0, sizeof c); c.kind = OBJ_PEN;
    int r = U(EnumObjects)(dc, OBJ_PEN, (GOBJENUMPROC)enum_cb, (LPARAM)&c);
    t_note("EnumObjects(OBJ_PEN) ret=%d count=%d", r, c.count);
    CHECK(c.count >= 1);
    for (int i = 0; i < c.count && i < 64; i++) {
        CHECKF(c.styles[i] <= PS_INSIDEFRAME || c.styles[i] == PS_NULL, "pen %d style %u", i, c.styles[i]);
        t_note("  pen %d style=%u color=%06lx", i, c.styles[i], c.cols[i]);
    }
    U(DeleteDC)(dc);
}
TEST(enumobj_brushes) {
    REQUIRE(U(EnumObjects)); HDC dc = os_memdc(); REQUIRE(dc);
    struct enum_ctx c; memset(&c, 0, sizeof c); c.kind = OBJ_BRUSH;
    int r = U(EnumObjects)(dc, OBJ_BRUSH, (GOBJENUMPROC)enum_cb, (LPARAM)&c);
    t_note("EnumObjects(OBJ_BRUSH) ret=%d count=%d", r, c.count);
    CHECK(c.count >= 1);
    int solid = 0, nul = 0;
    for (int i = 0; i < c.count && i < 64; i++) { if (c.styles[i] == BS_SOLID) solid++; if (c.styles[i] == BS_NULL) nul++; t_note("  brush %d style=%u color=%06lx", i, c.styles[i], c.cols[i]); }
    CHECK(solid >= 1);
    if (nul < 1) t_note("no BS_NULL brush enumerated");
    U(DeleteDC)(dc);
}
TEST(enumobj_early_stop_pens) {
    REQUIRE(U(EnumObjects)); HDC dc = os_memdc(); REQUIRE(dc);
    struct enum_ctx c; memset(&c, 0, sizeof c); c.kind = OBJ_PEN; c.stop_at = 1;
    int r = U(EnumObjects)(dc, OBJ_PEN, (GOBJENUMPROC)enum_cb, (LPARAM)&c);
    CHECK(c.count == 1);
    CHECK(r == 0);
    U(DeleteDC)(dc);
}
TEST(enumobj_early_stop_brushes) {
    REQUIRE(U(EnumObjects)); HDC dc = os_memdc(); REQUIRE(dc);
    for (int stop = 1; stop <= 4; stop++) {
        struct enum_ctx c; memset(&c, 0, sizeof c); c.kind = OBJ_BRUSH; c.stop_at = stop;
        int r = U(EnumObjects)(dc, OBJ_BRUSH, (GOBJENUMPROC)enum_cb, (LPARAM)&c);
        CHECKF(c.count == stop, "stop %d count %d", stop, c.count);
        CHECKF(r == 0, "stop %d ret %d", stop, r);
    }
    U(DeleteDC)(dc);
}
TEST(enumobj_complete_return_value_note) {
    REQUIRE(U(EnumObjects)); HDC dc = os_memdc(); REQUIRE(dc);
    struct enum_ctx c; memset(&c, 0, sizeof c); c.kind = OBJ_PEN;
    int r = U(EnumObjects)(dc, OBJ_PEN, (GOBJENUMPROC)enum_cb, (LPARAM)&c);
    t_note("full enumeration returned %d (last callback returned 1)", r);
    CHECK(c.count >= 1);
    CHECK(r == 1);
    U(DeleteDC)(dc);
}
TEST(enumobj_invalid_type) {
    REQUIRE(U(EnumObjects)); HDC dc = os_memdc(); REQUIRE(dc);
    int types[] = { 0, OBJ_DC, OBJ_PAL, OBJ_FONT, OBJ_BITMAP, OBJ_REGION, OBJ_EXTPEN, OBJ_MEMDC, 99, -1 };
    for (int i = 0; i < 10; i++) {
        struct enum_ctx c; memset(&c, 0, sizeof c); c.kind = OBJ_PEN;
        SET_ERR(0);
        int r = U(EnumObjects)(dc, types[i], (GOBJENUMPROC)enum_cb, (LPARAM)&c);
        DWORD e_ = GetLastError();
        t_note("EnumObjects(type %d) -> %d calls=%d err=%lu", types[i], r, c.count, GetLastError());
        CHECKF(c.count == 0, "type %d invoked callback", types[i]);
        CHECKF((r == 0 && e_ == ERROR_INVALID_PARAMETER) || (os_wine() && r == 0), "type %d ret %d err %lu", types[i], r, e_);
    }
    U(DeleteDC)(dc);
}
TEST(enumobj_invalid_dc) {
    REQUIRE(U(EnumObjects));
    for (int i = 0; i < NBADDC; i++) {
        struct enum_ctx c; memset(&c, 0, sizeof c); c.kind = OBJ_PEN;
        SET_ERR(0);
        int r = U(EnumObjects)(bad_dcs[i], OBJ_PEN, (GOBJENUMPROC)enum_cb, (LPARAM)&c);
        t_note("EnumObjects(dc %p) -> %d calls=%d", bad_dcs[i], r, c.count);
        CHECKF((r == 0 && c.count == 0) || os_wine(), "dc %p ret %d calls %d", bad_dcs[i], r, c.count);
    }
}
TEST(enumobj_screen_dc_matches_memdc) {
    REQUIRE(U(EnumObjects)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC s = U(GetDC)(NULL), m = os_memdc(); REQUIRE(s && m);
    struct enum_ctx a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b); a.kind = b.kind = OBJ_BRUSH;
    U(EnumObjects)(s, OBJ_BRUSH, (GOBJENUMPROC)enum_cb, (LPARAM)&a);
    U(EnumObjects)(m, OBJ_BRUSH, (GOBJENUMPROC)enum_cb, (LPARAM)&b);
    CHECK(a.count == b.count);
    U(ReleaseDC)(NULL, s); U(DeleteDC)(m);
}
TEST(enumobj_repeatable) {
    REQUIRE(U(EnumObjects)); HDC dc = os_memdc(); REQUIRE(dc);
    int first = -1;
    for (int i = 0; i < 10; i++) {
        struct enum_ctx c; memset(&c, 0, sizeof c); c.kind = OBJ_PEN;
        U(EnumObjects)(dc, OBJ_PEN, (GOBJENUMPROC)enum_cb, (LPARAM)&c);
        if (first < 0) first = c.count;
        CHECK(c.count == first);
    }
    U(DeleteDC)(dc);
}
TEST(enumobj_no_gdi_leak) {
    REQUIRE(U(EnumObjects)); HDC dc = os_memdc(); REQUIRE(dc);
    DWORD a = os_gdicount();
    for (int i = 0; i < 20; i++) {
        struct enum_ctx c; memset(&c, 0, sizeof c); c.kind = OBJ_BRUSH;
        U(EnumObjects)(dc, OBJ_BRUSH, (GOBJENUMPROC)enum_cb, (LPARAM)&c);
        c.kind = OBJ_PEN; U(EnumObjects)(dc, OBJ_PEN, (GOBJENUMPROC)enum_cb, (LPARAM)&c);
    }
    CHECK(os_gdicount() == a);
    U(DeleteDC)(dc);
}
static int CALLBACK enum_delete_cb(LPVOID obj, LPARAM lp) { (*(int *)lp)++; return 1; }
TEST(enumobj_a_wrapper) {
    REQUIRE(U(EnumObjects)); HDC dc = os_memdc(); REQUIRE(dc);
    int n = 0;
    U(EnumObjects)(dc, OBJ_PEN, (GOBJENUMPROC)enum_delete_cb, (LPARAM)&n);
    CHECK(n >= 1);
    U(DeleteDC)(dc);
}
