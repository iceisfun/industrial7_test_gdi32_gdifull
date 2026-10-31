/* SetBkMode / SetPolyFillMode / SetStretchBltMode (+ Impl exports) */
#include "g3.h"

typedef int (WINAPI *MSET)(HDC, int);
typedef int (WINAPI *MGET)(HDC);
typedef struct { const char *set, *get; int def, a, b, c; } ModeSpec;
static const ModeSpec SPEC_BK = { "SetBkMode", "GetBkMode", OPAQUE, TRANSPARENT, OPAQUE, TRANSPARENT };
static const ModeSpec SPEC_PF = { "SetPolyFillMode", "GetPolyFillMode", ALTERNATE, WINDING, ALTERNATE, WINDING };
static const ModeSpec SPEC_SB = { "SetStretchBltMode", "GetStretchBltMode", BLACKONWHITE, HALFTONE, COLORONCOLOR, WHITEONBLACK };

#define FOR_IMPL(sp, S, G) for (int impl_ = 0; impl_ < 2; impl_++) { \
    MSET S = (MSET)g3_fn((sp)->set, impl_); MGET G = (MGET)t_proc((sp)->get); \
    if (!S || !G) { t_note("%s%s missing", (sp)->set, impl_ ? "Impl" : ""); continue; }

static void m_default(const ModeSpec *sp) {
    FOR_IMPL(sp, S, G)
        HDC d = g3_mem();
        CHECK(d != NULL);
        EQ(G(d), sp->def);
        U(DeleteDC)(d);
    }
}
static void m_roundtrip(const ModeSpec *sp) {
    FOR_IMPL(sp, S, G)
        HDC d = g3_mem();
        int vals[3]; int i;
        vals[0] = sp->a; vals[1] = sp->b; vals[2] = sp->c;
        for (i = 0; i < 3; i++) {
            S(d, vals[i]);
            EQ(G(d), vals[i]);
        }
        U(DeleteDC)(d);
    }
}
static void m_prev(const ModeSpec *sp) {
    FOR_IMPL(sp, S, G)
        HDC d = g3_mem();
        EQ(S(d, sp->a), sp->def);
        EQ(S(d, sp->b), sp->a);
        EQ(S(d, sp->b), sp->b);
        EQ(S(d, sp->c), sp->b);
        EQ(S(d, sp->def), sp->c);
        U(DeleteDC)(d);
    }
}
static void m_nulldc(const ModeSpec *sp) {
    FOR_IMPL(sp, S, G)
        int r;
        SET_ERR(0);
        r = S(NULL, sp->a);
        EQ(r, 0);
        WERR(G3_ERR_INVALID_HANDLE);
        SET_ERR(0);
        r = G(NULL);
        EQ(r, 0);
    }
}
static void m_stale(const ModeSpec *sp) {
    FOR_IMPL(sp, S, G)
        HDC d = g3_stale_dc();
        int r;
        SET_ERR(0);
        r = S(d, sp->a);
        EQ(r, 0);
        WERR(G3_ERR_INVALID_PARAM);
    }
}
static void m_wrongtype(const ModeSpec *sp) {
    FOR_IMPL(sp, S, G)
        HGDIOBJ objs[4]; int i;
        objs[0] = U(GetStockObject)(BLACK_PEN);
        objs[1] = U(GetStockObject)(WHITE_BRUSH);
        objs[2] = U(GetStockObject)(SYSTEM_FONT);
        objs[3] = U(GetStockObject)(DEFAULT_PALETTE);
        for (i = 0; i < 4; i++) {
            SET_ERR(0);
            EQ(S((HDC)objs[i], sp->a), 0);
        }
    }
}
static void m_isolated(const ModeSpec *sp) {
    FOR_IMPL(sp, S, G)
        HDC d1 = g3_mem(), d2 = g3_mem();
        S(d1, sp->a);
        EQ(G(d1), sp->a);
        EQ(G(d2), sp->def);
        S(d2, sp->c);
        EQ(G(d1), sp->a);
        EQ(G(d2), sp->c);
        U(DeleteDC)(d1); U(DeleteDC)(d2);
    }
}
static void m_savedc(const ModeSpec *sp) {
    FOR_IMPL(sp, S, G)
        HDC d = g3_mem();
        int lvl;
        S(d, sp->a);
        lvl = U(SaveDC)(d);
        CHECK(lvl >= 1);
        S(d, sp->b);
        EQ(G(d), sp->b);
        CHECK(U(RestoreDC)(d, -1));
        EQ(G(d), sp->a);
        U(DeleteDC)(d);
    }
}
static void m_repeat(const ModeSpec *sp) {
    FOR_IMPL(sp, S, G)
        HDC d = g3_mem();
        int i, bad = 0;
        for (i = 0; i < 2000; i++) {
            int v = (i & 1) ? sp->a : sp->b;
            int prev = (i == 0) ? sp->def : ((i & 1) ? sp->b : sp->a);
            if (S(d, v) != prev) bad++;
        }
        EQ(bad, 0);
        U(DeleteDC)(d);
    }
}
static void m_extreme(const ModeSpec *sp) {
    FOR_IMPL(sp, S, G)
        HDC d = g3_mem();
        static const int ex[] = { 0, -1, 5, 100, 0x7fffffff, (int)0x80000000 };
        int i;
        for (i = 0; i < 6; i++) {
            int prev = G(d);
            int r = S(d, ex[i]);
            SOFT(r == prev);
            t_note("%s(%d) stored as %d", sp->set, ex[i], G(d));
            S(d, sp->a);
        }
        U(DeleteDC)(d);
    }
}
static void m_screen(const ModeSpec *sp) {
    FOR_IMPL(sp, S, G)
        HDC d = g3_screen();
        int orig;
        if (!d) { t_skip("no screen dc"); return; }
        orig = G(d);
        S(d, sp->a == orig ? sp->b : sp->a);
        CHECK(G(d) != orig);
        S(d, orig);
        EQ(G(d), orig);
        g3_unscreen(d);
    }
}
static void m_dib(const ModeSpec *sp) {
    FOR_IMPL(sp, S, G)
        G3DC m; CHECK(g3dc_make(&m, 8, 8));
        EQ(G(m.dc), sp->def);
        EQ(S(m.dc, sp->a), sp->def);
        EQ(G(m.dc), sp->a);
        g3dc_free(&m);
    }
}
static void m_gdicount(const ModeSpec *sp) {
    FOR_IMPL(sp, S, G)
        DWORD before = g3_gui(); int i;
        HDC d = g3_mem();
        for (i = 0; i < 100; i++) S(d, (i & 1) ? sp->a : sp->b);
        U(DeleteDC)(d);
        EQ(g3_gui(), before);
    }
}
static void m_thread_arg(void *p) {
    struct { const ModeSpec *sp; HDC dc; MSET S; MGET G; int ok; } *a = p;
    a->S(a->dc, a->sp->c);
    a->ok = a->G(a->dc) == a->sp->c;
}
static void m_thread(const ModeSpec *sp) {
    FOR_IMPL(sp, S, G)
        struct { const ModeSpec *sp; HDC dc; MSET S; MGET G; int ok; } a;
        a.sp = sp; a.dc = g3_mem(); a.S = S; a.G = G; a.ok = 0;
        g3_run_thread(m_thread_arg, &a);
        CHECK(a.ok);
        EQ(G(a.dc), sp->c);
        U(DeleteDC)(a.dc);
    }
}

#define MODE_TESTS(tag, SPEC) \
TEST(gap3_##tag##_default)   { REQUIRE(U(CreateCompatibleDC)); m_default(&SPEC); } \
TEST(gap3_##tag##_roundtrip) { REQUIRE(U(CreateCompatibleDC)); m_roundtrip(&SPEC); } \
TEST(gap3_##tag##_prev)      { REQUIRE(U(CreateCompatibleDC)); m_prev(&SPEC); } \
TEST(gap3_##tag##_nulldc)    { REQUIRE(U(CreateCompatibleDC)); m_nulldc(&SPEC); } \
TEST(gap3_##tag##_stale)     { REQUIRE(U(CreateCompatibleDC)); m_stale(&SPEC); } \
TEST(gap3_##tag##_wrongtype) { REQUIRE(U(GetStockObject)); m_wrongtype(&SPEC); } \
TEST(gap3_##tag##_isolated)  { REQUIRE(U(CreateCompatibleDC)); m_isolated(&SPEC); } \
TEST(gap3_##tag##_savedc)    { REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); m_savedc(&SPEC); } \
TEST(gap3_##tag##_repeat)    { REQUIRE(U(CreateCompatibleDC)); m_repeat(&SPEC); } \
TEST(gap3_##tag##_extreme)   { REQUIRE(U(CreateCompatibleDC)); m_extreme(&SPEC); } \
TEST(gap3_##tag##_screen)    { REQUIRE(U(GetDC)); m_screen(&SPEC); } \
TEST(gap3_##tag##_dibdc)     { REQUIRE(U(CreateDIBSection)); m_dib(&SPEC); } \
TEST(gap3_##tag##_gdicount)  { REQUIRE(U(CreateCompatibleDC)); m_gdicount(&SPEC); } \
TEST(gap3_##tag##_thread)    { REQUIRE(U(CreateCompatibleDC)); m_thread(&SPEC); }

MODE_TESTS(bkmode, SPEC_BK)
MODE_TESTS(polyfill, SPEC_PF)
MODE_TESTS(stretchmode, SPEC_SB)

/* ---- specific semantics ---- */
TEST(gap3_bkmode_constants) {
    EQ(TRANSPARENT, 1); EQ(OPAQUE, 2);
    EQ(ALTERNATE, 1); EQ(WINDING, 2);
    EQ(BLACKONWHITE, 1); EQ(WHITEONBLACK, 2); EQ(COLORONCOLOR, 3); EQ(HALFTONE, 4);
    EQ(STRETCH_ANDSCANS, 1); EQ(STRETCH_ORSCANS, 2); EQ(STRETCH_DELETESCANS, 3); EQ(STRETCH_HALFTONE, 4);
}
TEST(gap3_bkmode_gdi_struct_view) {
    /* GetBkMode and SetBkMode agree for both public and Impl entry */
    REQUIRE(U(SetBkMode)); REQUIRE(g3_fn("SetBkMode", 1)); REQUIRE(U(GetBkMode));
    HDC d = g3_mem();
    ((MSET)g3_fn("SetBkMode", 1))(d, TRANSPARENT);
    EQ(U(GetBkMode)(d), TRANSPARENT);
    U(SetBkMode)(d, OPAQUE);
    EQ(U(GetBkMode)(d), OPAQUE);
    U(DeleteDC)(d);
}
TEST(gap3_bkmode_text_effect) {
    /* opaque text paints the background cell; transparent text does not */
    REQUIRE(U(CreateDIBSection)); REQUIRE(U(TextOutW)); REQUIRE(U(SetBkColor)); REQUIRE(U(SetTextColor));
    G3DC m; if (!g3dc_make(&m, 64, 32)) { CHECK(0); return; }
    g3dc_clear(&m, RGB(0, 0, 0));
    U(SetBkColor)(m.dc, RGB(0, 255, 0));
    U(SetTextColor)(m.dc, RGB(255, 0, 0));
    U(SetBkMode)(m.dc, OPAQUE);
    U(TextOutW)(m.dc, 2, 2, L"M", 1);
    CHECK(g3dc_count(&m, RGB(0, 255, 0)) > 0);
    g3dc_clear(&m, RGB(0, 0, 0));
    U(SetBkMode)(m.dc, TRANSPARENT);
    U(TextOutW)(m.dc, 2, 2, L"M", 1);
    EQ(g3dc_count(&m, RGB(0, 255, 0)), 0);
    g3dc_free(&m);
}
TEST(gap3_bkmode_dash_pen_gaps) {
    /* a dashed pen leaves gaps filled with the background colour only when OPAQUE */
    REQUIRE(U(CreatePen)); REQUIRE(U(LineTo)); REQUIRE(U(MoveToEx));
    G3DC m; if (!g3dc_make(&m, 64, 8)) { CHECK(0); return; }
    HPEN pen = U(CreatePen)(PS_DASH, 1, RGB(255, 255, 255));
    HGDIOBJ op = U(SelectObject)(m.dc, pen);
    U(SetBkColor)(m.dc, RGB(0, 0, 255));
    U(SetBkMode)(m.dc, OPAQUE);
    g3dc_clear(&m, RGB(0, 0, 0));
    U(MoveToEx)(m.dc, 0, 3, NULL); U(LineTo)(m.dc, 60, 3);
    int opaque_bg = g3dc_count(&m, RGB(0, 0, 255));
    U(SetBkMode)(m.dc, TRANSPARENT);
    g3dc_clear(&m, RGB(0, 0, 0));
    U(MoveToEx)(m.dc, 0, 3, NULL); U(LineTo)(m.dc, 60, 3);
    int trans_bg = g3dc_count(&m, RGB(0, 0, 255));
    CHECK(opaque_bg > 0);
    EQ(trans_bg, 0);
    CHECK(g3dc_count(&m, RGB(255, 255, 255)) > 0);
    U(SelectObject)(m.dc, op); U(DeleteObject)(pen);
    g3dc_free(&m);
}
TEST(gap3_polyfill_effect_star) {
    /* five-point star: centre filled under WINDING, hollow under ALTERNATE */
    REQUIRE(U(Polygon)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(SetPolyFillMode));
    G3DC m; if (!g3dc_make(&m, 64, 64)) { CHECK(0); return; }
    POINT s[5] = { {32, 4}, {50, 58}, {4, 24}, {60, 24}, {14, 58} };
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 255, 255));
    HGDIOBJ ob = U(SelectObject)(m.dc, br);
    HGDIOBJ op = U(SelectObject)(m.dc, U(GetStockObject)(NULL_PEN));
    g3dc_clear(&m, RGB(0, 0, 0));
    U(SetPolyFillMode)(m.dc, ALTERNATE);
    U(Polygon)(m.dc, s, 5);
    DWORD alt = g3dc_get(&m, 32, 32);
    g3dc_clear(&m, RGB(0, 0, 0));
    U(SetPolyFillMode)(m.dc, WINDING);
    U(Polygon)(m.dc, s, 5);
    DWORD wnd = g3dc_get(&m, 32, 32);
    EQ(alt, 0);
    EQ(wnd, 0xffffff);
    U(SelectObject)(m.dc, op); U(SelectObject)(m.dc, ob); U(DeleteObject)(br);
    g3dc_free(&m);
}
TEST(gap3_polyfill_effect_nested_rects) {
    /* two same-direction nested rectangles: WINDING fills both, ALTERNATE leaves a hole */
    REQUIRE(U(PolyPolygon)); REQUIRE(U(SetPolyFillMode));
    G3DC m; if (!g3dc_make(&m, 32, 32)) { CHECK(0); return; }
    POINT p[8] = { {2,2},{30,2},{30,30},{2,30}, {10,10},{22,10},{22,22},{10,22} };
    INT cnt[2] = { 4, 4 };
    HBRUSH br = U(CreateSolidBrush)(RGB(255, 0, 0));
    HGDIOBJ ob = U(SelectObject)(m.dc, br);
    HGDIOBJ op = U(SelectObject)(m.dc, U(GetStockObject)(NULL_PEN));
    g3dc_clear(&m, RGB(0, 0, 0));
    U(SetPolyFillMode)(m.dc, ALTERNATE);
    CHECK(U(PolyPolygon)(m.dc, p, cnt, 2));
    PXC(m, 5, 5, RGB(255, 0, 0));
    PXC(m, 16, 16, RGB(0, 0, 0));
    g3dc_clear(&m, RGB(0, 0, 0));
    U(SetPolyFillMode)(m.dc, WINDING);
    CHECK(U(PolyPolygon)(m.dc, p, cnt, 2));
    PXC(m, 5, 5, RGB(255, 0, 0));
    PXC(m, 16, 16, RGB(255, 0, 0));
    U(SelectObject)(m.dc, op); U(SelectObject)(m.dc, ob); U(DeleteObject)(br);
    g3dc_free(&m);
}
TEST(gap3_stretchmode_effect_shrink_colors) {
    /* shrinking a 2x2 checker 4:1 under COLORONCOLOR yields exactly one of the source colours */
    REQUIRE(U(StretchBlt)); REQUIRE(U(SetStretchBltMode));
    G3DC src, dst;
    if (!g3dc_make(&src, 4, 4) || !g3dc_make(&dst, 4, 4)) { CHECK(0); return; }
    int x, y;
    for (y = 0; y < 4; y++) for (x = 0; x < 4; x++)
        src.bits[y * 4 + x] = ((x ^ y) & 1) ? 0xffffff : 0x000000;
    g3dc_clear(&dst, RGB(1, 2, 3));
    U(SetStretchBltMode)(dst.dc, COLORONCOLOR);
    CHECK(U(StretchBlt)(dst.dc, 0, 0, 1, 1, src.dc, 0, 0, 4, 4, SRCCOPY));
    DWORD v = g3dc_get(&dst, 0, 0);
    CHECK(v == 0x000000 || v == 0xffffff);
    PXC(dst, 1, 1, RGB(1, 2, 3));
    g3dc_free(&src); g3dc_free(&dst);
}
TEST(gap3_stretchmode_effect_andscans_orscans) {
    /* 2:1 horizontal shrink of a black/white pair: AND gives black, OR gives white */
    REQUIRE(U(StretchBlt)); REQUIRE(U(SetStretchBltMode));
    G3DC src, dst;
    if (!g3dc_make(&src, 2, 1) || !g3dc_make(&dst, 2, 1)) { CHECK(0); return; }
    src.bits[0] = 0x000000; src.bits[1] = 0xffffff;
    g3dc_clear(&dst, RGB(9, 9, 9));
    U(SetStretchBltMode)(dst.dc, BLACKONWHITE);
    U(StretchBlt)(dst.dc, 0, 0, 1, 1, src.dc, 0, 0, 2, 1, SRCCOPY);
    t_note("BLACKONWHITE shrink px=%06lx", (unsigned long)g3dc_get(&dst, 0, 0));
    g3dc_clear(&dst, RGB(9, 9, 9));
    U(SetStretchBltMode)(dst.dc, WHITEONBLACK);
    U(StretchBlt)(dst.dc, 0, 0, 1, 1, src.dc, 0, 0, 2, 1, SRCCOPY);
    t_note("WHITEONBLACK shrink px=%06lx", (unsigned long)g3dc_get(&dst, 0, 0));
    g3dc_free(&src); g3dc_free(&dst);
}
TEST(gap3_stretchmode_halftone_origin) {
    /* HALFTONE requires SetBrushOrgEx afterwards; the call itself must succeed */
    REQUIRE(U(SetStretchBltMode)); REQUIRE(U(SetBrushOrgEx));
    HDC d = g3_mem();
    EQ(U(SetStretchBltMode)(d, HALFTONE), BLACKONWHITE);
    POINT old = { 77, 77 };
    CHECK(U(SetBrushOrgEx)(d, 3, 4, &old));
    EQ(old.x, 0); EQ(old.y, 0);
    EQ(U(GetStretchBltMode)(d), HALFTONE);
    U(DeleteDC)(d);
}
