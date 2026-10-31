/* SetTextAlign, SetTextColor, SetMiterLimit, SetICMMode, MoveToEx (+ Impl exports) */
#include "g3.h"

#define IMPLS(var, pub) for (int i_ = 0; i_ < 2; i_++) { var = (__typeof__(&pub))g3_fn(#pub, i_); \
    if (!var) { t_note(#pub " impl %d missing", i_); continue; }

/* ---------- SetTextAlign ---------- */
TEST(gap3_textalign_default) {
    REQUIRE(U(GetTextAlign));
    HDC d = g3_mem();
    EQ(U(GetTextAlign)(d), TA_TOP | TA_LEFT | TA_NOUPDATECP);
    EQ(U(GetTextAlign)(d), 0);
    U(DeleteDC)(d);
}
TEST(gap3_textalign_roundtrip) {
    REQUIRE(U(GetTextAlign));
    static const UINT v[] = { TA_LEFT, TA_RIGHT, TA_CENTER, TA_TOP, TA_BOTTOM, TA_BASELINE, TA_UPDATECP,
        TA_RIGHT | TA_BOTTOM, TA_CENTER | TA_BASELINE | TA_UPDATECP, TA_RTLREADING, TA_RTLREADING | TA_RIGHT,
        TA_LEFT | TA_TOP, TA_BASELINE | TA_CENTER };
    __typeof__(&SetTextAlign) S;
    IMPLS(S, SetTextAlign)
        HDC d = g3_mem(); unsigned k; UINT prev = 0;
        for (k = 0; k < sizeof v / sizeof *v; k++) {
            EQ(S(d, v[k]), prev);
            EQ(U(GetTextAlign)(d), v[k]);
            prev = v[k];
        }
        U(DeleteDC)(d);
    }
}
TEST(gap3_textalign_values) {
    EQ(TA_NOUPDATECP, 0); EQ(TA_UPDATECP, 1); EQ(TA_LEFT, 0); EQ(TA_RIGHT, 2); EQ(TA_CENTER, 6);
    EQ(TA_TOP, 0); EQ(TA_BOTTOM, 8); EQ(TA_BASELINE, 24); EQ(TA_RTLREADING, 256);
}
TEST(gap3_textalign_invalid_dc) {
    __typeof__(&SetTextAlign) S;
    IMPLS(S, SetTextAlign)
        SET_ERR(0);
        EQ(S(NULL, TA_CENTER), GDI_ERROR);
        WERR(G3_ERR_INVALID_HANDLE);
        HDC st = g3_stale_dc();
        SET_ERR(0);
        EQ(S(st, TA_CENTER), GDI_ERROR);
        WERR(G3_ERR_INVALID_PARAM);
        SET_ERR(0);
        SOFT(U(GetTextAlign)(NULL) == GDI_ERROR);
    }
}
TEST(gap3_textalign_wrong_type) {
    HGDIOBJ o = U(GetStockObject)(BLACK_PEN);
    EQ(U(SetTextAlign)((HDC)o, TA_CENTER), GDI_ERROR);
    o = U(GetStockObject)(WHITE_BRUSH);
    EQ(U(SetTextAlign)((HDC)o, TA_CENTER), GDI_ERROR);
}
TEST(gap3_textalign_savedc) {
    HDC d = g3_mem();
    U(SetTextAlign)(d, TA_RIGHT);
    CHECK(U(SaveDC)(d) >= 1);
    U(SetTextAlign)(d, TA_CENTER | TA_BOTTOM);
    CHECK(U(RestoreDC)(d, -1));
    EQ(U(GetTextAlign)(d), TA_RIGHT);
    U(DeleteDC)(d);
}
TEST(gap3_textalign_high_bits) {
    /* the whole value is retained for GetTextAlign; only low 5 bits matter to rendering */
    HDC d = g3_mem();
    UINT r = U(SetTextAlign)(d, 0xfffff000u | TA_RIGHT);
    EQ(r, 0);
    t_note("GetTextAlign after high bits = 0x%x", U(GetTextAlign)(d));
    U(SetTextAlign)(d, 0);
    U(DeleteDC)(d);
}
TEST(gap3_textalign_isolated) {
    HDC a = g3_mem(), b = g3_mem();
    U(SetTextAlign)(a, TA_RIGHT);
    EQ(U(GetTextAlign)(b), 0);
    U(DeleteDC)(a); U(DeleteDC)(b);
}
TEST(gap3_textalign_updatecp_moves_pos) {
    /* with TA_UPDATECP, TextOut advances the current position by the text width */
    REQUIRE(U(TextOutW)); REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    G3DC m; if (!g3dc_make(&m, 64, 32)) { CHECK(0); return; }
    HGDIOBJ of = U(SelectObject)(m.dc, U(GetStockObject)(SYSTEM_FIXED_FONT));
    U(SetTextAlign)(m.dc, TA_UPDATECP);
    U(MoveToEx)(m.dc, 5, 5, NULL);
    CHECK(U(TextOutW)(m.dc, 0, 0, L"AB", 2));
    POINT p; CHECK(U(GetCurrentPositionEx)(m.dc, &p));
    CHECK(p.x > 5); EQ(p.y, 5);
    U(SetTextAlign)(m.dc, 0);
    U(MoveToEx)(m.dc, 5, 5, NULL);
    U(TextOutW)(m.dc, 0, 0, L"AB", 2);
    U(GetCurrentPositionEx)(m.dc, &p);
    EQ(p.x, 5);
    U(SelectObject)(m.dc, of);
    g3dc_free(&m);
}
TEST(gap3_textalign_right_vs_left_pixels) {
    REQUIRE(U(TextOutW));
    G3DC m; if (!g3dc_make(&m, 100, 24)) { CHECK(0); return; }
    HGDIOBJ of = U(SelectObject)(m.dc, U(GetStockObject)(SYSTEM_FIXED_FONT));
    U(SetBkMode)(m.dc, TRANSPARENT);
    U(SetTextColor)(m.dc, RGB(255, 255, 255));
    g3dc_clear(&m, RGB(0, 0, 0));
    U(SetTextAlign)(m.dc, TA_LEFT | TA_TOP);
    U(TextOutW)(m.dc, 50, 2, L"MM", 2);
    int left_of = g3dc_count_rect(&m, 0, 0, 50, 24, RGB(255, 255, 255));
    int right_of = g3dc_count_rect(&m, 50, 0, 100, 24, RGB(255, 255, 255));
    EQ(left_of, 0);
    CHECK(right_of > 0);
    g3dc_clear(&m, RGB(0, 0, 0));
    U(SetTextAlign)(m.dc, TA_RIGHT | TA_TOP);
    U(TextOutW)(m.dc, 50, 2, L"MM", 2);
    left_of = g3dc_count_rect(&m, 0, 0, 50, 24, RGB(255, 255, 255));
    right_of = g3dc_count_rect(&m, 50, 0, 100, 24, RGB(255, 255, 255));
    CHECK(left_of > 0);
    EQ(right_of, 0);
    U(SelectObject)(m.dc, of);
    g3dc_free(&m);
}

/* ---------- SetTextColor ---------- */
TEST(gap3_textcolor_default) {
    HDC d = g3_mem();
    EQ(U(GetTextColor)(d), RGB(0, 0, 0));
    U(DeleteDC)(d);
}
TEST(gap3_textcolor_roundtrip) {
    static const COLORREF v[] = { RGB(255,0,0), RGB(0,255,0), RGB(0,0,255), RGB(255,255,255), RGB(1,2,3), RGB(0,0,0),
        RGB(0x80,0x80,0x80), RGB(254,253,252) };
    __typeof__(&SetTextColor) S;
    IMPLS(S, SetTextColor)
        HDC d = g3_mem(); unsigned k; COLORREF prev = 0;
        for (k = 0; k < sizeof v / sizeof *v; k++) {
            EQ(S(d, v[k]), prev);
            EQ(U(GetTextColor)(d), v[k]);
            prev = v[k];
        }
        U(DeleteDC)(d);
    }
}
TEST(gap3_textcolor_invalid_dc) {
    __typeof__(&SetTextColor) S;
    IMPLS(S, SetTextColor)
        SET_ERR(0);
        EQ(S(NULL, RGB(1,2,3)), CLR_INVALID);
        WERR(G3_ERR_INVALID_HANDLE);
        HDC st = g3_stale_dc();
        SET_ERR(0);
        EQ(S(st, RGB(1,2,3)), CLR_INVALID);
        WERR(G3_ERR_INVALID_PARAM);
        SOFT(U(GetTextColor)(NULL) == CLR_INVALID);
    }
}
TEST(gap3_textcolor_wrong_type) {
    EQ(U(SetTextColor)((HDC)U(GetStockObject)(BLACK_PEN), RGB(1,1,1)), CLR_INVALID);
    EQ(U(SetTextColor)((HDC)U(GetStockObject)(SYSTEM_FONT), RGB(1,1,1)), CLR_INVALID);
}
TEST(gap3_textcolor_high_byte) {
    /* the upper byte is stored but only palette flags in it have meaning */
    HDC d = g3_mem();
    U(SetTextColor)(d, 0x00123456);
    EQ(U(GetTextColor)(d), 0x00123456);
    COLORREF r = U(SetTextColor)(d, 0x02123456);   /* PALETTEINDEX-style */
    EQ(r, 0x00123456);
    t_note("GetTextColor palette-style = 0x%08x", U(GetTextColor)(d));
    U(SetTextColor)(d, 0x01000005);
    t_note("GetTextColor PALETTEINDEX(5) = 0x%08x", U(GetTextColor)(d));
    U(SetTextColor)(d, 0x00FFFFFF);
    EQ(U(GetTextColor)(d), 0x00FFFFFF);
    U(DeleteDC)(d);
}
TEST(gap3_textcolor_savedc) {
    HDC d = g3_mem();
    U(SetTextColor)(d, RGB(10,20,30));
    CHECK(U(SaveDC)(d) >= 1);
    U(SetTextColor)(d, RGB(200,100,50));
    EQ(U(GetTextColor)(d), RGB(200,100,50));
    CHECK(U(RestoreDC)(d, -1));
    EQ(U(GetTextColor)(d), RGB(10,20,30));
    U(DeleteDC)(d);
}
TEST(gap3_textcolor_isolated) {
    HDC a = g3_mem(), b = g3_mem();
    U(SetTextColor)(a, RGB(9,9,9));
    EQ(U(GetTextColor)(b), 0);
    U(DeleteDC)(a); U(DeleteDC)(b);
}
TEST(gap3_textcolor_pixels_16bit_dib) {
    /* text colour is used by glyph interior (opaque, aliased font => exact colour present) */
    REQUIRE(U(TextOutW));
    G3DC m; if (!g3dc_make(&m, 64, 24)) { CHECK(0); return; }
    HGDIOBJ of = U(SelectObject)(m.dc, U(GetStockObject)(SYSTEM_FIXED_FONT));
    g3dc_clear(&m, RGB(0, 0, 0));
    U(SetBkMode)(m.dc, TRANSPARENT);
    U(SetTextColor)(m.dc, RGB(12, 200, 99));
    U(TextOutW)(m.dc, 2, 2, L"HI", 2);
    CHECK(g3dc_count(&m, RGB(12, 200, 99)) > 0);
    EQ(g3dc_count(&m, RGB(255, 255, 255)), 0);
    U(SelectObject)(m.dc, of);
    g3dc_free(&m);
}
TEST(gap3_textcolor_screen_restore) {
    HDC d = g3_screen(); if (!d) { t_skip("no dc"); return; }
    COLORREF o = U(GetTextColor)(d);
    COLORREF r = U(SetTextColor)(d, RGB(1,2,3));
    EQ(r, o);
    U(SetTextColor)(d, o);
    g3_unscreen(d);
}
static void tc_thread(void *p) { HDC d = p; U(SetTextColor)(d, RGB(7,8,9)); }
TEST(gap3_textcolor_cross_thread) {
    HDC d = g3_mem();
    g3_run_thread(tc_thread, d);
    EQ(U(GetTextColor)(d), RGB(7,8,9));
    U(DeleteDC)(d);
}

/* ---------- SetMiterLimit ---------- */
TEST(gap3_miter_default) {
    REQUIRE(U(SetMiterLimit)); REQUIRE(U(GetMiterLimit));
    HDC d = g3_mem(); FLOAT f = 0;
    CHECK(U(GetMiterLimit)(d, &f));
    CHECK(f == 10.0f);
    U(DeleteDC)(d);
}
TEST(gap3_miter_roundtrip) {
    REQUIRE(U(SetMiterLimit)); REQUIRE(U(GetMiterLimit));
    static const FLOAT v[] = { 1.0f, 2.5f, 5.0f, 10.0f, 100.0f, 1000.5f };
    __typeof__(&SetMiterLimit) S;
    IMPLS(S, SetMiterLimit)
        HDC d = g3_mem(); FLOAT old, cur, prev = 10.0f; unsigned k;
        for (k = 0; k < sizeof v / sizeof *v; k++) {
            old = -1;
            CHECK(S(d, v[k], &old));
            CHECK(old == prev);
            cur = 0; CHECK(U(GetMiterLimit)(d, &cur));
            CHECK(cur == v[k]);
            prev = v[k];
        }
        U(DeleteDC)(d);
    }
}
TEST(gap3_miter_null_old) {
    REQUIRE(U(SetMiterLimit));
    __typeof__(&SetMiterLimit) S;
    IMPLS(S, SetMiterLimit)
        HDC d = g3_mem(); FLOAT f = 0;
        CHECK(S(d, 3.0f, NULL));
        CHECK(U(GetMiterLimit)(d, &f)); CHECK(f == 3.0f);
        U(DeleteDC)(d);
    }
}
TEST(gap3_miter_invalid_dc) {
    REQUIRE(U(SetMiterLimit));
    __typeof__(&SetMiterLimit) S;
    IMPLS(S, SetMiterLimit)
        FLOAT old = 123.0f;
        SET_ERR(0);
        EQ(S(NULL, 5.0f, &old), 0);
        WERR(G3_ERR_INVALID_HANDLE);
        CHECK(old == 123.0f);
        HDC st = g3_stale_dc();
        EQ(S(st, 5.0f, &old), 0);
        EQ(S((HDC)U(GetStockObject)(BLACK_PEN), 5.0f, &old), 0);
        FLOAT g = 77;
        SET_ERR(0);
        EQ(U(GetMiterLimit)(NULL, &g), 0);
    }
}
TEST(gap3_miter_below_one) {
    REQUIRE(U(SetMiterLimit)); REQUIRE(U(GetMiterLimit));
    HDC d = g3_mem(); FLOAT old = 0, cur = 0;
    BOOL r = U(SetMiterLimit)(d, 0.5f, &old);
    U(GetMiterLimit)(d, &cur);
    t_note("SetMiterLimit(0.5) -> %d, stored %f", r, cur);
    r = U(SetMiterLimit)(d, 0.0f, &old);
    U(GetMiterLimit)(d, &cur);
    t_note("SetMiterLimit(0) -> %d, stored %f", r, cur);
    r = U(SetMiterLimit)(d, -3.0f, &old);
    U(GetMiterLimit)(d, &cur);
    t_note("SetMiterLimit(-3) -> %d, stored %f", r, cur);
    U(DeleteDC)(d);
}
TEST(gap3_miter_savedc) {
    REQUIRE(U(SetMiterLimit)); REQUIRE(U(GetMiterLimit));
    HDC d = g3_mem(); FLOAT f;
    U(SetMiterLimit)(d, 4.0f, NULL);
    CHECK(U(SaveDC)(d) >= 1);
    U(SetMiterLimit)(d, 8.0f, NULL);
    CHECK(U(RestoreDC)(d, -1));
    CHECK(U(GetMiterLimit)(d, &f)); WCHECK(f == 4.0f);
    U(DeleteDC)(d);
}
TEST(gap3_miter_effect_geometric_join) {
    /* a sharp miter joint is drawn with a large limit and bevelled with a limit of 1 */
    REQUIRE(U(ExtCreatePen)); REQUIRE(U(SetMiterLimit)); REQUIRE(U(Polyline));
    G3DC m; if (!g3dc_make(&m, 64, 64)) { CHECK(0); return; }
    LOGBRUSH lb = { BS_SOLID, RGB(255, 255, 255), 0 };
    HPEN pen = U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID | PS_JOIN_MITER | PS_ENDCAP_FLAT, 6, &lb, 0, NULL);
    CHECK(pen != NULL);
    HGDIOBJ op = U(SelectObject)(m.dc, pen);
    POINT p[3] = { {8, 50}, {32, 20}, {56, 50} };
    g3dc_clear(&m, RGB(0, 0, 0));
    U(SetMiterLimit)(m.dc, 10.0f, NULL);
    U(Polyline)(m.dc, p, 3);
    int wide = g3dc_count(&m, RGB(255, 255, 255));
    g3dc_clear(&m, RGB(0, 0, 0));
    U(SetMiterLimit)(m.dc, 1.0f, NULL);
    U(Polyline)(m.dc, p, 3);
    int bevel = g3dc_count(&m, RGB(255, 255, 255));
    CHECK(wide > bevel);
    U(SelectObject)(m.dc, op); U(DeleteObject)(pen);
    g3dc_free(&m);
}

/* ---------- SetICMMode ---------- */
TEST(gap3_icmmode_constants) {
    EQ(ICM_OFF, 1); EQ(ICM_ON, 2); EQ(ICM_QUERY, 3); EQ(ICM_DONE_OUTSIDEDC, 4);
}
TEST(gap3_icmmode_query_default) {
    REQUIRE(U(SetICMMode));
    __typeof__(&SetICMMode) S;
    IMPLS(S, SetICMMode)
        HDC d = g3_mem();
        int r = S(d, ICM_QUERY);
        CHECK(r == ICM_OFF);
        U(DeleteDC)(d);
    }
}
TEST(gap3_icmmode_invalid_dc) {
    REQUIRE(U(SetICMMode));
    __typeof__(&SetICMMode) S;
    IMPLS(S, SetICMMode)
        SET_ERR(0);
        WEQ(S(NULL, ICM_QUERY), 0);
        WERR(G3_ERR_INVALID_HANDLE);
        SET_ERR(0);
        WEQ(S(NULL, ICM_OFF), 0);
        WEQ(S(NULL, ICM_ON), 0);
        HDC st = g3_stale_dc();
        SOFT(S(st, ICM_QUERY) == 0);
    }
}
TEST(gap3_icmmode_off_idempotent) {
    REQUIRE(U(SetICMMode));
    __typeof__(&SetICMMode) S;
    IMPLS(S, SetICMMode)
        HDC d = g3_mem();
        int r = S(d, ICM_OFF);
        t_note("SetICMMode(OFF) on default mem dc = %d", r);
        CHECK(S(d, ICM_QUERY) == ICM_OFF);
        U(DeleteDC)(d);
    }
}
TEST(gap3_icmmode_bad_modes) {
    REQUIRE(U(SetICMMode));
    HDC d = g3_mem(); int ex[] = { 0, -1, 5, 100, INT_MAX, INT_MIN }; unsigned k;
    for (k = 0; k < 6; k++) {
        SET_ERR(0);
        int r = U(SetICMMode)(d, ex[k]);
        t_note("SetICMMode(%d) = %d, err=%lu", ex[k], r, GetLastError());
    }
    CHECK(U(SetICMMode)(d, ICM_QUERY) == ICM_OFF);
    U(DeleteDC)(d);
}
TEST(gap3_icmmode_dibdc) {
    REQUIRE(U(SetICMMode));
    G3DC m; if (!g3dc_make(&m, 4, 4)) { CHECK(0); return; }
    int r = U(SetICMMode)(m.dc, ICM_QUERY);
    CHECK(r == ICM_OFF);
    g3dc_free(&m);
}
TEST(gap3_icmmode_wrong_type) {
    REQUIRE(U(SetICMMode));
    SOFT(U(SetICMMode)((HDC)U(GetStockObject)(BLACK_PEN), ICM_QUERY) == 0);
}

/* ---------- MoveToEx ---------- */
TEST(gap3_moveto_basic) {
    REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    __typeof__(&MoveToEx) S;
    IMPLS(S, MoveToEx)
        HDC d = g3_mem(); POINT p = { 99, 99 };
        CHECK(S(d, 10, 20, &p));
        EQ(p.x, 0); EQ(p.y, 0);
        CHECK(S(d, -5, 7, &p));
        EQ(p.x, 10); EQ(p.y, 20);
        CHECK(U(GetCurrentPositionEx)(d, &p));
        EQ(p.x, -5); EQ(p.y, 7);
        CHECK(S(d, 1, 1, NULL));
        CHECK(U(GetCurrentPositionEx)(d, &p));
        EQ(p.x, 1); EQ(p.y, 1);
        U(DeleteDC)(d);
    }
}
TEST(gap3_moveto_extremes) {
    REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    __typeof__(&MoveToEx) S;
    IMPLS(S, MoveToEx)
        HDC d = g3_mem(); POINT p;
        static const int v[] = { INT_MAX, INT_MIN, 0, -1, 1, 65535, 65536, -65536, 0x7fff, 0x8000 };
        unsigned k;
        for (k = 0; k < sizeof v / sizeof *v; k++) {
            CHECK(S(d, v[k], v[k], NULL));
            CHECK(U(GetCurrentPositionEx)(d, &p));
            t_note("MoveToEx(%d) read back (%ld,%ld)", v[k], p.x, p.y);
        }
        CHECK(S(d, 3, 4, NULL));
        U(GetCurrentPositionEx)(d, &p);
        EQ(p.x, 3); EQ(p.y, 4);
        U(DeleteDC)(d);
    }
}
TEST(gap3_moveto_invalid_dc) {
    REQUIRE(U(MoveToEx));
    __typeof__(&MoveToEx) S;
    IMPLS(S, MoveToEx)
        POINT p = { 55, 66 };
        SET_ERR(0);
        EQ(S(NULL, 1, 2, &p), 0);
        WERR(G3_ERR_INVALID_HANDLE);
        EQ(p.x, 55); EQ(p.y, 66);
        HDC st = g3_stale_dc();
        EQ(S(st, 1, 2, &p), 0);
        EQ(S(st, 1, 2, NULL), 0);
        EQ(S((HDC)U(GetStockObject)(BLACK_PEN), 1, 2, &p), 0);
        SET_ERR(0);
        EQ(U(GetCurrentPositionEx)(NULL, &p), 0);
    }
}
TEST(gap3_moveto_isolated_and_save) {
    REQUIRE(U(MoveToEx));
    HDC a = g3_mem(), b = g3_mem(); POINT p;
    U(MoveToEx)(a, 11, 12, NULL);
    U(GetCurrentPositionEx)(b, &p);
    EQ(p.x, 0); EQ(p.y, 0);
    /* current position is not part of SaveDC state */
    CHECK(U(SaveDC)(a) >= 1);
    U(MoveToEx)(a, 40, 41, NULL);
    CHECK(U(RestoreDC)(a, -1));
    U(GetCurrentPositionEx)(a, &p);
    t_note("pos after RestoreDC = (%ld,%ld)", p.x, p.y);
    U(DeleteDC)(a); U(DeleteDC)(b);
}
TEST(gap3_moveto_logical_units) {
    /* old position is reported in the DC's current logical units */
    REQUIRE(U(MoveToEx)); REQUIRE(U(SetMapMode)); REQUIRE(U(SetWindowExtEx)); REQUIRE(U(SetViewportExtEx));
    HDC d = g3_mem(); POINT p;
    U(SetMapMode)(d, MM_ANISOTROPIC);
    U(SetWindowExtEx)(d, 100, 100, NULL);
    U(SetViewportExtEx)(d, 200, 200, NULL);
    U(MoveToEx)(d, 10, 20, NULL);
    U(MoveToEx)(d, 30, 40, &p);
    EQ(p.x, 10); EQ(p.y, 20);
    U(GetCurrentPositionEx)(d, &p);
    EQ(p.x, 30); EQ(p.y, 40);
    U(DeleteDC)(d);
}
TEST(gap3_moveto_lineto_start) {
    REQUIRE(U(MoveToEx)); REQUIRE(U(LineTo));
    G3DC m; if (!g3dc_make(&m, 32, 8)) { CHECK(0); return; }
    HPEN pen = U(CreatePen)(PS_SOLID, 1, RGB(255, 255, 255));
    HGDIOBJ op = U(SelectObject)(m.dc, pen);
    g3dc_clear(&m, RGB(0, 0, 0));
    U(MoveToEx)(m.dc, 4, 3, NULL);
    CHECK(U(LineTo)(m.dc, 12, 3));
    PXC(m, 4, 3, RGB(255,255,255));
    PXC(m, 11, 3, RGB(255,255,255));
    PXC(m, 12, 3, RGB(0,0,0));
    PXC(m, 3, 3, RGB(0,0,0));
    PXC(m, 8, 2, RGB(0,0,0));
    PXC(m, 8, 4, RGB(0,0,0));
    POINT p; U(GetCurrentPositionEx)(m.dc, &p);
    EQ(p.x, 12); EQ(p.y, 3);
    U(SelectObject)(m.dc, op); U(DeleteObject)(pen);
    g3dc_free(&m);
}
TEST(gap3_moveto_gdicount) {
    REQUIRE(U(MoveToEx));
    DWORD b = g3_gui(); int i;
    HDC d = g3_mem();
    for (i = 0; i < 1000; i++) U(MoveToEx)(d, i, -i, NULL);
    U(DeleteDC)(d);
    EQ(g3_gui(), b);
}
