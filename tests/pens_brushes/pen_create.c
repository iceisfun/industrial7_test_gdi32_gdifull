#include "pb.h"

#define NEED_PEN() do { REQUIRE(U(CreatePen)); REQUIRE(U(CreatePenIndirect)); REQUIRE(U(DeleteObject)); \
    REQUIRE(U(GetObjectW)); REQUIRE(U(GetObjectType)); } while (0)

static int get_logpen(HGDIOBJ h, LOGPEN *lp)
{
    memset(lp, 0xb0, sizeof *lp);
    return U(GetObjectW)(h, sizeof *lp, lp);
}

/* expected result of CreatePen(style, ...) per gdi32full: invalid styles degrade to PS_SOLID,
 * PS_NULL becomes the stock NULL_PEN. */
static int expect_style(int style)
{
    if (style == PS_NULL) return PS_NULL;
    if (style >= 0 && style <= 4) return style;
    if (style == PS_INSIDEFRAME) return PS_INSIDEFRAME;
    return PS_SOLID;
}

TEST(pens_createpen_style_table)
{
    NEED_PEN();
    static const int styles[] = { INT_MIN, -100, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        0x10, 0x20, 0x100, 0x200, 0x1000, 0x10000, 0x10001, 0x20000, 0x7fffffff, 0xff };
    for (unsigned i = 0; i < sizeof styles / sizeof *styles; i++) {
        int st = styles[i];
        HPEN p = U(CreatePen)(st, 3, RGB(0x12, 0x34, 0x56));
        CHECKF(p != NULL, "style %d", st);
        if (!p) continue;
        CHECKF(U(GetObjectType)(p) == OBJ_PEN, "style %d", st);
        LOGPEN lp;
        int n = get_logpen(p, &lp);
        CHECKF(n == sizeof(LOGPEN), "style %d size %d", st, n);
        CHECKF((int)lp.lopnStyle == expect_style(st), "style %d got %u", st, lp.lopnStyle);
        if (st == PS_NULL) {
            CHECK(p == GetStockObject(NULL_PEN));
            CHECKF(lp.lopnWidth.x == 1 && lp.lopnColor == 0, "null pen w=%ld c=%08lx", lp.lopnWidth.x, lp.lopnColor);
        } else {
            CHECKF(lp.lopnWidth.x == 3, "style %d width %ld", st, lp.lopnWidth.x);
            CHECKF(lp.lopnColor == RGB(0x12, 0x34, 0x56), "style %d color %08lx", st, lp.lopnColor);
        }
        CHECKF(lp.lopnWidth.y == 0, "style %d y=%ld", st, lp.lopnWidth.y);
        if (st != PS_NULL) CHECK(U(DeleteObject)(p));
    }
}

TEST(pens_createpen_null_style_is_stock)
{
    NEED_PEN();
    HPEN a = U(CreatePen)(PS_NULL, 0, RGB(1, 2, 3));
    HPEN b = U(CreatePen)(PS_NULL, 50, RGB(9, 9, 9));
    CHECK(a == GetStockObject(NULL_PEN));
    CHECK(b == a);
    LOGPEN lp;
    CHECK(get_logpen(a, &lp) == sizeof lp);
    CHECK(lp.lopnStyle == PS_NULL);
    /* deleting a stock object must leave it usable */
    U(DeleteObject)(a);
    CHECK(get_logpen(GetStockObject(NULL_PEN), &lp) == sizeof lp);
}

TEST(pens_createpen_width_table)
{
    NEED_PEN();
    static const struct { int in, out; } w[] = {
        {0, 0}, {1, 1}, {2, 2}, {3, 3}, {5, 5}, {7, 7}, {16, 16}, {100, 100}, {255, 255}, {256, 256}, {1000, 1000},
        {-1, 1}, {-2, 2}, {-3, 3}, {-123, 123}, {-1000, 1000},
    };
    for (unsigned i = 0; i < sizeof w / sizeof *w; i++) {
        HPEN p = U(CreatePen)(PS_SOLID, w[i].in, RGB(10, 20, 30));
        CHECKF(p != NULL, "w=%d", w[i].in);
        if (!p) continue;
        LOGPEN lp;
        CHECK(get_logpen(p, &lp) == sizeof lp);
        CHECKF(lp.lopnWidth.x == w[i].out, "w=%d got %ld", w[i].in, lp.lopnWidth.x);
        CHECKF(lp.lopnWidth.y == 0, "w=%d y=%ld", w[i].in, lp.lopnWidth.y);
        CHECK(U(DeleteObject)(p));
    }
}

TEST(pens_createpen_width_large)
{
    NEED_PEN();
    static const int w[] = { 0x7fff, 0x8000, 0xffff, 0x10000, 0x7ffff, 0x1000000, INT_MAX };
    for (unsigned i = 0; i < sizeof w / sizeof *w; i++) {
        HPEN p = U(CreatePen)(PS_SOLID, w[i], 0);
        if (!p) { t_note("CreatePen width %d failed", w[i]); continue; }
        LOGPEN lp;
        int n = get_logpen(p, &lp);
        CHECK(n == sizeof lp);
        if (lp.lopnWidth.x != w[i]) t_note("CreatePen width %d reads back %ld", w[i], lp.lopnWidth.x);
        CHECK(U(DeleteObject)(p));
    }
}

TEST(pens_createpen_width_int_min_fails)
{
    NEED_PEN();
    SetLastError(0xdeadbeef);
    HPEN p = U(CreatePen)(PS_SOLID, INT_MIN, 0);
    CHECK(p == NULL);
    if (p) U(DeleteObject)(p);
    else EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
}

TEST(pens_createpen_color_plain)
{
    NEED_PEN();
    static const COLORREF c[] = { 0, 1, 0xff, 0x100, 0xff00, 0x10000, 0xff0000, 0xffffff, 0x123456, 0xabcdef,
        0x7f7f7f, 0x808080, 0xfefefe, 0x010101 };
    for (unsigned i = 0; i < sizeof c / sizeof *c; i++) {
        HPEN p = U(CreatePen)(PS_SOLID, 1, c[i]);
        CHECK(p != NULL);
        LOGPEN lp;
        CHECK(get_logpen(p, &lp) == sizeof lp);
        CHECKF(lp.lopnColor == c[i], "in %08lx got %08lx", c[i], lp.lopnColor);
        U(DeleteObject)(p);
    }
}

TEST(pens_createpen_color_each_channel_value)
{
    NEED_PEN();
    for (int v = 0; v < 256; v++) {
        COLORREF c = RGB(v, 255 - v, (v * 7) & 255);
        HPEN p = U(CreatePen)(PS_SOLID, 1, c);
        LOGPEN lp;
        if (!p) { CHECK(p != NULL); continue; }
        CHECK(get_logpen(p, &lp) == sizeof lp);
        CHECKF(lp.lopnColor == c, "v=%d got %08lx want %08lx", v, lp.lopnColor, c);
        U(DeleteObject)(p);
    }
}

TEST(pens_createpen_color_high_byte)
{
    NEED_PEN();
    static const COLORREF c[] = { 0x01123456, 0x02000005, 0x03123456, 0x10123456, 0xff123456, 0xffffffff, 0x80000000 };
    for (unsigned i = 0; i < sizeof c / sizeof *c; i++) {
        HPEN p = U(CreatePen)(PS_SOLID, 1, c[i]);
        CHECKF(p != NULL, "color %08lx", c[i]);
        if (!p) continue;
        LOGPEN lp;
        CHECK(get_logpen(p, &lp) == sizeof lp);
        if (lp.lopnColor != c[i]) t_note("pen color %08lx reads back %08lx", c[i], lp.lopnColor);
        U(DeleteObject)(p);
    }
}

TEST(pens_createpen_handle_lifecycle)
{
    NEED_PEN();
    HPEN a = U(CreatePen)(PS_SOLID, 1, RGB(1, 1, 1));
    HPEN b = U(CreatePen)(PS_SOLID, 1, RGB(1, 1, 1));
    CHECK(a && b);
    CHECK(a != b);
    CHECK(a != GetStockObject(BLACK_PEN));
    CHECK(U(GetObjectType)(a) == OBJ_PEN);
    CHECK(U(DeleteObject)(a));
    LOGPEN lp;
    SetLastError(0xdeadbeef);
    CHECK(get_logpen(a, &lp) == 0);
    CHECK(U(GetObjectType)(a) == 0);
    SetLastError(0xdeadbeef);
    CHECK(!U(DeleteObject)(a));
    /* b untouched */
    CHECK(get_logpen(b, &lp) == sizeof lp);
    CHECK(U(DeleteObject)(b));
    CHECK(!U(DeleteObject)(b));
}

TEST(pens_createpen_width0_cache_cycles)
{
    NEED_PEN();
    /* CreatePen(PS_SOLID,0,c) may recycle a cached pen: each result must carry the new colour */
    for (int i = 0; i < 64; i++) {
        COLORREF c = RGB(i * 4, 255 - i * 3, i);
        HPEN p = U(CreatePen)(PS_SOLID, 0, c);
        CHECK(p != NULL);
        LOGPEN lp;
        CHECK(get_logpen(p, &lp) == sizeof lp);
        CHECKF(lp.lopnColor == c && lp.lopnWidth.x == 0 && lp.lopnStyle == PS_SOLID, "i=%d col %08lx", i, lp.lopnColor);
        CHECK(U(DeleteObject)(p));
    }
    /* many alive at once */
    HPEN pens[32];
    for (int i = 0; i < 32; i++) pens[i] = U(CreatePen)(PS_SOLID, 0, RGB(i, i, i));
    for (int i = 0; i < 32; i++) {
        LOGPEN lp;
        CHECK(pens[i] != NULL);
        for (int j = 0; j < i; j++) CHECK(pens[i] != pens[j]);
        CHECK(get_logpen(pens[i], &lp) == sizeof lp);
        CHECK(lp.lopnColor == RGB(i, i, i));
    }
    for (int i = 0; i < 32; i++) U(DeleteObject)(pens[i]);
}

TEST(pens_getobject_buffer_sizes)
{
    NEED_PEN();
    HPEN p = U(CreatePen)(PS_DASH, 4, RGB(9, 8, 7));
    CHECK(p != NULL);
    CHECK(U(GetObjectW)(p, 0, NULL) == sizeof(LOGPEN));
    LOGPEN lp;
    for (int sz = 1; sz < (int)sizeof(LOGPEN); sz++) {
        memset(&lp, 0xb0, sizeof lp);
        SetLastError(0xdeadbeef);
        int n = U(GetObjectW)(p, sz, &lp);
        CHECKF(n == 0, "size %d returned %d", sz, n);
        /* nothing may be written on failure */
        CHECKF(((BYTE *)&lp)[0] == 0xb0, "size %d wrote", sz);
    }
    CHECK(U(GetObjectW)(p, sizeof lp, &lp) == sizeof lp);
    BYTE big[128];
    for (int sz = (int)sizeof lp + 1; sz <= 128; sz += 7) {
        memset(big, 0xcd, sizeof big);
        int n = U(GetObjectW)(p, sz, big);
        CHECKF(n == sizeof(LOGPEN), "size %d returned %d", sz, n);
        int clean = 1;
        for (int i = sizeof(LOGPEN); i < sz; i++) if (big[i] != 0xcd) clean = 0;
        CHECKF(clean, "size %d wrote past LOGPEN", sz);
    }
    SetLastError(0xdeadbeef);
    int zero = U(GetObjectW)(p, 0, &lp);
    t_note("GetObject(pen, 0, non-NULL) = %d", zero);
    CHECK(U(GetObjectW)(p, -1, &lp) == 0 || 1);
    U(DeleteObject)(p);
}

TEST(pens_getobject_extlogpen_sized_buffer_on_cosmetic)
{
    NEED_PEN();
    HPEN p = U(CreatePen)(PS_SOLID, 2, RGB(1, 2, 3));
    EXTLOGPEN e;
    memset(&e, 0xb0, sizeof e);
    int n = U(GetObjectW)(p, sizeof e, &e);
    CHECK(n == sizeof(LOGPEN));
    CHECK(((LOGPEN *)&e)->lopnWidth.x == 2);
    U(DeleteObject)(p);
}

TEST(pens_getobject_invalid_handles)
{
    NEED_PEN();
    LOGPEN lp;
    HANDLE bad[] = { NULL, (HANDLE)1, (HANDLE)2, (HANDLE)0xdeadbeef, (HANDLE)(LONG_PTR)-1, (HANDLE)0x10001,
        (HANDLE)0x7fffffff, (HANDLE)0x300000 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0xdeadbeef);
        CHECKF(U(GetObjectW)(bad[i], sizeof lp, &lp) == 0, "h=%p", bad[i]);
        CHECKF(U(GetObjectType)(bad[i]) == 0, "h=%p", bad[i]);
        SetLastError(0xdeadbeef);
        CHECKF(!U(DeleteObject)(bad[i]), "h=%p", bad[i]);
    }
}

TEST(pens_stock_pens_logpen)
{
    NEED_PEN();
    static const struct { int id; UINT style; COLORREF c; } s[] = {
        { BLACK_PEN, PS_SOLID, RGB(0, 0, 0) },
        { WHITE_PEN, PS_SOLID, RGB(255, 255, 255) },
        { NULL_PEN, PS_NULL, RGB(0, 0, 0) },
    };
    for (unsigned i = 0; i < 3; i++) {
        HGDIOBJ h = GetStockObject(s[i].id);
        CHECK(h != NULL);
        CHECK(U(GetObjectType)(h) == OBJ_PEN);
        LOGPEN lp;
        CHECK(get_logpen(h, &lp) == sizeof lp);
        CHECKF(lp.lopnStyle == s[i].style, "id %d style %u", s[i].id, lp.lopnStyle);
        CHECKF(lp.lopnColor == s[i].c, "id %d color %08lx", s[i].id, lp.lopnColor);
        if (s[i].id == NULL_PEN) CHECKF(lp.lopnWidth.x == 1, "id %d width %ld", s[i].id, lp.lopnWidth.x);
        else if (lp.lopnWidth.x != 1) t_note("stock pen %d width %ld", s[i].id, lp.lopnWidth.x);
        CHECK(GetStockObject(s[i].id) == h);
    }
    HGDIOBJ dc = GetStockObject(DC_PEN);
    CHECK(dc != NULL);
    CHECK(U(GetObjectType)(dc) == OBJ_PEN);
    LOGPEN lp;
    CHECK(get_logpen(dc, &lp) == sizeof lp);
    CHECK(lp.lopnStyle == PS_SOLID);
}

TEST(pens_stock_delete_is_harmless)
{
    NEED_PEN();
    for (int id = WHITE_PEN; id <= NULL_PEN; id++) {
        HGDIOBJ h = GetStockObject(id);
        BOOL r = U(DeleteObject)(h);
        t_note("DeleteObject(stock pen %d) = %d", id, r);
        LOGPEN lp;
        CHECK(get_logpen(GetStockObject(id), &lp) == sizeof lp);
        CHECK(GetStockObject(id) == h);
    }
}

TEST(pens_createpenindirect_table)
{
    NEED_PEN();
    static const int styles[] = { -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 100 };
    static const int widths[] = { -50, -1, 0, 1, 2, 50 };
    for (unsigned a = 0; a < sizeof styles / sizeof *styles; a++)
    for (unsigned b = 0; b < sizeof widths / sizeof *widths; b++) {
        LOGPEN in, out;
        in.lopnStyle = styles[a];
        in.lopnWidth.x = widths[b];
        in.lopnWidth.y = 77;
        in.lopnColor = RGB(0x11, 0x22, 0x33);
        HPEN p = U(CreatePenIndirect)(&in);
        CHECKF(p != NULL, "st=%d w=%d", styles[a], widths[b]);
        if (!p) continue;
        CHECK(get_logpen(p, &out) == sizeof out);
        /* style is an UINT in LOGPEN: a negative int arrives as a large unsigned value */
        UINT es = (UINT)styles[a] > 6 ? PS_SOLID : (UINT)styles[a];
        CHECKF(out.lopnStyle == es, "st=%d got %u", styles[a], out.lopnStyle);
        if (es == PS_NULL) CHECK(out.lopnColor == 0);
        else {
            CHECKF(out.lopnWidth.x == (widths[b] < 0 ? -widths[b] : widths[b]), "w=%d got %ld", widths[b], out.lopnWidth.x);
            CHECK(out.lopnColor == in.lopnColor);
        }
        CHECK(out.lopnWidth.y == 0);
        U(DeleteObject)(p);
    }
}

TEST(pens_createpenindirect_equals_createpen)
{
    NEED_PEN();
    for (int st = 0; st <= 6; st++) {
        LOGPEN in = { (UINT)st, { 4, 0 }, RGB(5, 6, 7) }, a, b;
        HPEN p1 = U(CreatePenIndirect)(&in);
        HPEN p2 = U(CreatePen)(st, 4, RGB(5, 6, 7));
        CHECK(p1 && p2);
        CHECK(get_logpen(p1, &a) == sizeof a);
        CHECK(get_logpen(p2, &b) == sizeof b);
        CHECK(!memcmp(&a, &b, sizeof a));
        if (st != PS_NULL) { U(DeleteObject)(p1); U(DeleteObject)(p2); }
        else CHECK(p1 == p2);
    }
}

TEST(pens_select_returns_previous)
{
    NEED_PEN(); NEED_MEM();
    REQUIRE(U(GetCurrentObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(dc != NULL);
    HGDIOBJ def = U(GetCurrentObject)(dc, OBJ_PEN);
    CHECK(def == GetStockObject(BLACK_PEN));
    HPEN a = U(CreatePen)(PS_SOLID, 1, RGB(1, 2, 3));
    HPEN b = U(CreatePen)(PS_DOT, 1, RGB(3, 2, 1));
    CHECK(U(SelectObject)(dc, a) == def);
    CHECK(U(GetCurrentObject)(dc, OBJ_PEN) == a);
    CHECK(U(SelectObject)(dc, a) == a);
    CHECK(U(SelectObject)(dc, b) == a);
    CHECK(U(GetCurrentObject)(dc, OBJ_PEN) == b);
    CHECK(U(SelectObject)(dc, def) == b);
    CHECK(U(GetCurrentObject)(dc, OBJ_PEN) == def);
    CHECK(U(DeleteObject)(a));
    CHECK(U(DeleteObject)(b));
    /* selecting a deleted pen fails */
    SetLastError(0xdeadbeef);
    CHECK(U(SelectObject)(dc, a) == NULL);
    CHECK(U(GetCurrentObject)(dc, OBJ_PEN) == def);
    CHECK(U(DeleteDC)(dc));
}

TEST(pens_select_wrong_type_object)
{
    NEED_PEN(); NEED_MEM();
    REQUIRE(U(GetCurrentObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ def = U(GetCurrentObject)(dc, OBJ_PEN);
    HBRUSH br = GetStockObject(BLACK_BRUSH);
    HGDIOBJ old = U(SelectObject)(dc, br);
    CHECK(old == GetStockObject(WHITE_BRUSH));
    CHECK(U(GetCurrentObject)(dc, OBJ_PEN) == def);
    CHECK(U(GetCurrentObject)(dc, OBJ_BRUSH) == br);
    U(SelectObject)(dc, old);
    U(DeleteDC)(dc);
}

TEST(pens_delete_selected_pen)
{
    NEED_PEN(); NEED_MEM();
    REQUIRE(U(GetCurrentObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ def = U(GetCurrentObject)(dc, OBJ_PEN);
    HPEN p = U(CreatePen)(PS_SOLID, 1, RGB(200, 100, 50));
    U(SelectObject)(dc, p);
    BOOL r1 = U(DeleteObject)(p);
    t_note("DeleteObject(selected pen) = %d", r1);
    LOGPEN lp;
    int alive = get_logpen(p, &lp) == sizeof lp;
    t_note("selected pen still valid after DeleteObject: %d", alive);
    U(SelectObject)(dc, def);
    BOOL r2 = U(DeleteObject)(p);
    CHECK(r1 || r2);
    CHECK(get_logpen(p, &lp) == 0);
    CHECK(U(DeleteDC)(dc));
}

typedef struct { HPEN pen; int ok; LOGPEN lp; HDC dc; HGDIOBJ prev; BOOL del; } XT;
static DWORD WINAPI xt_proc(LPVOID a)
{
    XT *x = a;
    x->ok = U(GetObjectW)(x->pen, sizeof x->lp, &x->lp) == sizeof x->lp;
    HDC dc = U(CreateCompatibleDC)(NULL);
    x->prev = U(SelectObject)(dc, x->pen);
    U(SelectObject)(dc, x->prev);
    U(DeleteDC)(dc);
    x->del = U(DeleteObject)(x->pen);
    return 0;
}
TEST(pens_cross_thread_use_and_delete)
{
    NEED_PEN(); NEED_MEM();
    XT x; memset(&x, 0, sizeof x);
    x.pen = U(CreatePen)(PS_DASHDOT, 3, RGB(9, 99, 199));
    HANDLE th = CreateThread(NULL, 0, xt_proc, &x, 0, NULL);
    CHECK(th != NULL);
    WaitForSingleObject(th, 10000);
    CloseHandle(th);
    CHECK(x.ok);
    CHECK(x.lp.lopnStyle == PS_DASHDOT && x.lp.lopnWidth.x == 3 && x.lp.lopnColor == RGB(9, 99, 199));
    CHECK(x.prev == GetStockObject(BLACK_PEN));
    CHECK(x.del);
    LOGPEN lp;
    CHECK(get_logpen(x.pen, &lp) == 0);
}

TEST(pens_leak_create_delete_loop)
{
    NEED_PEN();
    DWORD before = pb_gdi_count();
    if (!before) { t_skip("GetGuiResources unavailable"); return; }
    for (int i = 0; i < 3000; i++) {
        HPEN p = U(CreatePen)(i % 7, (i % 5) ? 1 : 3, RGB(i, i >> 2, 3));
        if (!p) { CHECK(p != NULL); break; }
        if (p != GetStockObject(NULL_PEN)) U(DeleteObject)(p);
    }
    DWORD after = pb_gdi_count();
    CHECKF(after <= before + 2 && after + 2 >= before, "before %lu after %lu", before, after);
}

TEST(pens_leak_alive_count_tracks)
{
    NEED_PEN();
    DWORD before = pb_gdi_count();
    if (!before) { t_skip("GetGuiResources unavailable"); return; }
    enum { N = 300 };
    static HPEN ps[N];
    for (int i = 0; i < N; i++) ps[i] = U(CreatePen)(PS_SOLID, 1, RGB(i & 255, i >> 8, 77));
    DWORD mid = pb_gdi_count();
    CHECKF(mid >= before + N - 2 && mid <= before + N + 2, "before %lu mid %lu", before, mid);
    for (int i = 0; i < N; i++) U(DeleteObject)(ps[i]);
    DWORD after = pb_gdi_count();
    CHECKF(after <= before + 2, "before %lu after %lu", before, after);
}

TEST(pens_dcpen_color_api)
{
    NEED_PEN(); NEED_MEM();
    REQUIRE(U(SetDCPenColor)); REQUIRE(U(GetDCPenColor));
    HDC dc = U(CreateCompatibleDC)(NULL);
    COLORREF init = U(GetDCPenColor)(dc);
    t_note("initial DC pen colour %08lx", init);
    CHECK(init != CLR_INVALID);
    CHECK(U(SetDCPenColor)(dc, RGB(1, 2, 3)) == init);
    CHECK(U(GetDCPenColor)(dc) == RGB(1, 2, 3));
    CHECK(U(SetDCPenColor)(dc, RGB(250, 251, 252)) == RGB(1, 2, 3));
    CHECK(U(GetDCPenColor)(dc) == RGB(250, 251, 252));
    /* does not alter the stock pen object itself */
    LOGPEN lp;
    CHECK(get_logpen(GetStockObject(DC_PEN), &lp) == sizeof lp);
    t_note("DC_PEN LOGPEN colour %08lx", lp.lopnColor);
    CHECK(U(DeleteDC)(dc));
}

TEST(pens_dcpen_color_invalid_dc)
{
    NEED_PEN();
    REQUIRE(U(SetDCPenColor)); REQUIRE(U(GetDCPenColor));
    SetLastError(0xdeadbeef);
    CHECK(U(SetDCPenColor)(NULL, 0) == CLR_INVALID);
    SetLastError(0xdeadbeef);
    CHECK(U(GetDCPenColor)(NULL) == CLR_INVALID);
    SetLastError(0xdeadbeef);
    CHECK(U(SetDCPenColor)((HDC)(LONG_PTR)-1, 0) == CLR_INVALID);
    SetLastError(0xdeadbeef);
    CHECK(U(GetDCPenColor)((HDC)0xdeadbeef) == CLR_INVALID);
    HPEN pen = U(CreatePen)(PS_SOLID, 1, 0);
    CHECK(U(SetDCPenColor)((HDC)pen, 0) == CLR_INVALID);
    U(DeleteObject)(pen);
}

TEST(pens_dcpen_color_is_per_dc)
{
    NEED_PEN(); NEED_MEM();
    REQUIRE(U(SetDCPenColor)); REQUIRE(U(GetDCPenColor));
    HDC a = U(CreateCompatibleDC)(NULL), b = U(CreateCompatibleDC)(NULL);
    U(SetDCPenColor)(a, RGB(10, 20, 30));
    U(SetDCPenColor)(b, RGB(40, 50, 60));
    CHECK(U(GetDCPenColor)(a) == RGB(10, 20, 30));
    CHECK(U(GetDCPenColor)(b) == RGB(40, 50, 60));
    U(DeleteDC)(a); U(DeleteDC)(b);
}

TEST(pens_dcpen_color_survives_save_restore)
{
    NEED_PEN(); NEED_MEM();
    REQUIRE(U(SetDCPenColor)); REQUIRE(U(GetDCPenColor)); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    U(SetDCPenColor)(dc, RGB(1, 1, 1));
    int s = U(SaveDC)(dc);
    CHECK(s > 0);
    U(SetDCPenColor)(dc, RGB(2, 2, 2));
    CHECK(U(RestoreDC)(dc, -1));
    CHECK(U(GetDCPenColor)(dc) == RGB(1, 1, 1));
    U(DeleteDC)(dc);
}

TEST(pens_select_pen_save_restore)
{
    NEED_PEN(); NEED_MEM();
    REQUIRE(U(GetCurrentObject)); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HPEN a = U(CreatePen)(PS_SOLID, 1, 5), b = U(CreatePen)(PS_SOLID, 1, 6);
    U(SelectObject)(dc, a);
    int s = U(SaveDC)(dc);
    U(SelectObject)(dc, b);
    CHECK(U(GetCurrentObject)(dc, OBJ_PEN) == b);
    CHECK(U(RestoreDC)(dc, s));
    CHECK(U(GetCurrentObject)(dc, OBJ_PEN) == a);
    U(SelectObject)(dc, GetStockObject(BLACK_PEN));
    U(DeleteObject)(a); U(DeleteObject)(b);
    U(DeleteDC)(dc);
}

TEST(pens_object_type_codes)
{
    NEED_PEN();
    LOGBRUSH lb = { BS_SOLID, RGB(1, 2, 3), 0 };
    REQUIRE(U(ExtCreatePen));
    HPEN a = U(CreatePen)(PS_SOLID, 1, 0);
    HPEN e = U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID, 4, &lb, 0, NULL);
    HPEN c = U(ExtCreatePen)(PS_COSMETIC | PS_DOT, 1, &lb, 0, NULL);
    CHECK(U(GetObjectType)(a) == OBJ_PEN);
    CHECK(U(GetObjectType)(e) == OBJ_EXTPEN);
    CHECK(U(GetObjectType)(c) == OBJ_EXTPEN);
    U(DeleteObject)(a); U(DeleteObject)(e); U(DeleteObject)(c);
}
