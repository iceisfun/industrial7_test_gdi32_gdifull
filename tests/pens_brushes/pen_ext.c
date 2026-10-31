#include "pb.h"

#define NEED_EXT() do { REQUIRE(U(ExtCreatePen)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectW)); \
    REQUIRE(U(GetObjectType)); } while (0)

#define ELP_BASE ((int)offsetof(EXTLOGPEN, elpStyleEntry))
typedef union { EXTLOGPEN e; BYTE raw[sizeof(EXTLOGPEN) + 16 * 4]; LOGPEN lp; } ELPBUF;

static HPEN ext(DWORD style, DWORD w, UINT bs, COLORREF c, ULONG_PTR h, DWORD cnt, const DWORD *arr)
{
    LOGBRUSH lb = { bs, c, h };
    return U(ExtCreatePen)(style, w, &lb, cnt, arr);
}
static HPEN ext_solid(DWORD style, DWORD w)
{
    return ext(style, w, BS_SOLID, RGB(0x12, 0x34, 0x56), HS_CROSS, 0, NULL);
}
static void expect_fail(HPEN p, const char *what, DWORD line)
{
    t_check(p == NULL, __FILE__, line, what, "pen created");
    if (p) U(DeleteObject)(p);
}
#define EXPECT_PEN_FAIL(call) do { SetLastError(0xdeadbeef); HPEN p_ = (call); \
    t_check(p_ == NULL, __FILE__, __LINE__, #call, "unexpectedly succeeded"); \
    if (p_) U(DeleteObject)(p_); else EXPECT_ERR(0, ERROR_INVALID_PARAMETER); } while (0)
#define EXPECT_PEN_OK(call, var) HPEN var; do { SetLastError(0xdeadbeef); var = (call); \
    t_check(var != NULL, __FILE__, __LINE__, #call, "failed gle=%lu", GetLastError()); } while (0)

TEST(pens_ext_cosmetic_basic_styles)
{
    NEED_EXT();
    for (DWORD st = PS_SOLID; st <= PS_DASHDOTDOT; st++) {
        EXPECT_PEN_OK(ext_solid(PS_COSMETIC | st, 1), p);
        if (!p) continue;
        ELPBUF b; memset(&b, 0xcc, sizeof b);
        CHECK(U(GetObjectType)(p) == OBJ_EXTPEN);
        CHECKF(U(GetObjectW)(p, 0, NULL) == ELP_BASE, "style %lu", st);
        int n = U(GetObjectW)(p, sizeof b, &b);
        CHECKF(n == ELP_BASE, "style %lu n=%d", st, n);
        CHECK(b.e.elpPenStyle == (PS_COSMETIC | st));
        CHECK(b.e.elpWidth == 1);
        CHECK(b.e.elpColor == RGB(0x12, 0x34, 0x56));
        CHECK(b.e.elpBrushStyle == BS_SOLID);
        CHECK(b.e.elpHatch == HS_CROSS);
        CHECK(b.e.elpNumEntries == 0);
        U(DeleteObject)(p);
    }
}

TEST(pens_ext_geometric_basic_styles)
{
    NEED_EXT();
    static const DWORD widths[] = { 0, 1, 2, 3, 10, 100 };
    for (DWORD st = PS_SOLID; st <= PS_DASHDOTDOT; st++)
    for (unsigned wi = 0; wi < 6; wi++) {
        EXPECT_PEN_OK(ext_solid(PS_GEOMETRIC | st, widths[wi]), p);
        if (!p) continue;
        ELPBUF b; memset(&b, 0xcc, sizeof b);
        CHECK(U(GetObjectType)(p) == OBJ_EXTPEN);
        int n = U(GetObjectW)(p, sizeof b, &b);
        CHECKF(n == ELP_BASE, "st %lu w %lu n=%d", st, widths[wi], n);
        CHECK(b.e.elpPenStyle == (PS_GEOMETRIC | st));
        CHECKF(b.e.elpWidth == widths[wi], "st %lu w %lu got %lu", st, widths[wi], b.e.elpWidth);
        CHECK(b.e.elpColor == RGB(0x12, 0x34, 0x56));
        CHECK(b.e.elpBrushStyle == BS_SOLID);
        CHECK(b.e.elpNumEntries == 0);
        U(DeleteObject)(p);
    }
}

TEST(pens_ext_cosmetic_width_must_be_one)
{
    NEED_EXT();
    static const DWORD widths[] = { 0, 2, 3, 5, 10, 100, 0x10000, 0x7fffffff };
    for (DWORD st = PS_SOLID; st <= PS_DASHDOTDOT; st++)
    for (unsigned wi = 0; wi < 8; wi++) {
        SetLastError(0xdeadbeef);
        HPEN p = ext_solid(PS_COSMETIC | st, widths[wi]);
        CHECKF(p == NULL, "style %lu width %lu", st, widths[wi]);
        if (p) U(DeleteObject)(p); else EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    }
}

TEST(pens_ext_cosmetic_width_negative_one)
{
    NEED_EXT();
    HPEN p = ext_solid(PS_COSMETIC | PS_SOLID, (DWORD)-1);
    t_note("ExtCreatePen cosmetic width -1 -> %p", (void *)p);
    if (p) {
        ELPBUF b;
        int n = U(GetObjectW)(p, sizeof b, &b);
        t_note("  n=%d width=%lu", n, b.e.elpWidth);
        U(DeleteObject)(p);
    }
}

TEST(pens_ext_geometric_negative_width_note)
{
    NEED_EXT();
    HPEN p = ext_solid(PS_GEOMETRIC | PS_SOLID, (DWORD)-5);
    t_note("ExtCreatePen geometric width -5 -> %p", (void *)p);
    if (p) { ELPBUF b; int n = U(GetObjectW)(p, sizeof b, &b); t_note("  n=%d width=%lu", n, b.e.elpWidth); U(DeleteObject)(p); }
    p = ext_solid(PS_GEOMETRIC | PS_SOLID, 0x80000000u);
    t_note("ExtCreatePen geometric width 0x80000000 -> %p", (void *)p);
    if (p) U(DeleteObject)(p);
}

TEST(pens_ext_invalid_style_low_nibble)
{
    NEED_EXT();
    for (DWORD st = 9; st <= 15; st++) {
        EXPECT_PEN_FAIL(ext_solid(PS_COSMETIC | st, 1));
        EXPECT_PEN_FAIL(ext_solid(PS_GEOMETRIC | st, 3));
    }
}

TEST(pens_ext_invalid_style_flag_bits)
{
    NEED_EXT();
    /* bits 0x10..0x80 (unused), type bits other than 0/0x10000, caps/joins other than defined */
    static const DWORD bad[] = { 0x10, 0x20, 0x40, 0x80, 0x30, 0xf0, 0x20000, 0x30000, 0x40000, 0x80000, 0x100000,
        0x1000000, 0x10000000, 0x80000000u, 0x300, 0x400, 0x800, 0x3000, 0x4000, 0x8000 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0xdeadbeef);
        HPEN p = ext_solid(PS_GEOMETRIC | bad[i], 3);
        CHECKF(p == NULL, "geometric|%lx", bad[i]);
        if (p) U(DeleteObject)(p); else EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
        SetLastError(0xdeadbeef);
        p = ext_solid(PS_COSMETIC | bad[i], 1);
        CHECKF(p == NULL, "cosmetic|%lx", bad[i]);
        if (p) U(DeleteObject)(p); else EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    }
}

TEST(pens_ext_endcap_join_matrix_geometric)
{
    NEED_EXT();
    static const DWORD caps[] = { PS_ENDCAP_ROUND, PS_ENDCAP_SQUARE, PS_ENDCAP_FLAT };
    static const DWORD joins[] = { PS_JOIN_ROUND, PS_JOIN_BEVEL, PS_JOIN_MITER };
    for (int c = 0; c < 3; c++) for (int j = 0; j < 3; j++) for (DWORD st = 0; st <= 4; st++) {
        DWORD full = PS_GEOMETRIC | st | caps[c] | joins[j];
        EXPECT_PEN_OK(ext_solid(full, 6), p);
        if (!p) continue;
        ELPBUF b;
        CHECK(U(GetObjectW)(p, sizeof b, &b) == ELP_BASE);
        CHECKF(b.e.elpPenStyle == full, "want %lx got %lx", full, b.e.elpPenStyle);
        CHECK(b.e.elpWidth == 6);
        U(DeleteObject)(p);
    }
}

TEST(pens_ext_endcap_join_on_cosmetic)
{
    NEED_EXT();
    for (DWORD cap = 0; cap <= 0x200; cap += 0x100) {
        HPEN p = ext_solid(PS_COSMETIC | PS_SOLID | cap, 1);
        t_note("cosmetic with cap %lx -> %s", cap, p ? "created" : "failed");
        if (p) {
            ELPBUF b;
            if (U(GetObjectW)(p, sizeof b, &b) > 0) t_note("  style readback %lx", b.e.elpPenStyle);
            U(DeleteObject)(p);
        }
    }
}

TEST(pens_ext_insideframe)
{
    NEED_EXT();
    EXPECT_PEN_FAIL(ext_solid(PS_COSMETIC | PS_INSIDEFRAME, 1));
    EXPECT_PEN_OK(ext_solid(PS_GEOMETRIC | PS_INSIDEFRAME, 5), p);
    if (p) {
        ELPBUF b;
        CHECK(U(GetObjectW)(p, sizeof b, &b) == ELP_BASE);
        CHECK(b.e.elpPenStyle == (PS_GEOMETRIC | PS_INSIDEFRAME));
        CHECK(b.e.elpWidth == 5);
        U(DeleteObject)(p);
    }
}

TEST(pens_ext_alternate_only_cosmetic)
{
    NEED_EXT();
    EXPECT_PEN_OK(ext_solid(PS_COSMETIC | PS_ALTERNATE, 1), p);
    if (p) {
        ELPBUF b;
        CHECK(U(GetObjectW)(p, sizeof b, &b) == ELP_BASE);
        CHECK(b.e.elpPenStyle == (PS_COSMETIC | PS_ALTERNATE));
        CHECK(b.e.elpWidth == 1);
        U(DeleteObject)(p);
    }
    EXPECT_PEN_FAIL(ext_solid(PS_GEOMETRIC | PS_ALTERNATE, 1));
    EXPECT_PEN_FAIL(ext_solid(PS_GEOMETRIC | PS_ALTERNATE, 5));
    EXPECT_PEN_FAIL(ext_solid(PS_COSMETIC | PS_ALTERNATE, 2));
    EXPECT_PEN_FAIL(ext_solid(PS_COSMETIC | PS_ALTERNATE, 0));
}

TEST(pens_ext_null_pen_is_stock)
{
    NEED_EXT();
    for (DWORD ty = 0; ty <= PS_GEOMETRIC; ty += PS_GEOMETRIC) {
        DWORD w = ty ? 5 : 1;
        EXPECT_PEN_OK(ext_solid(ty | PS_NULL, w), p);
        if (!p) continue;
        CHECK(U(GetObjectType)(p) == OBJ_PEN);
        CHECK(p == GetStockObject(NULL_PEN));
        CHECK(U(GetObjectW)(p, 0, NULL) == sizeof(LOGPEN));
        LOGPEN lp;
        CHECK(U(GetObjectW)(p, sizeof lp, &lp) == sizeof lp);
        CHECK(lp.lopnStyle == PS_NULL);
        CHECK(lp.lopnColor == 0);
        U(DeleteObject)(p);
    }
}

TEST(pens_ext_null_pen_getobject_extlogpen)
{
    NEED_EXT();
    HPEN p = ext_solid(PS_GEOMETRIC | PS_NULL, 3);
    CHECK(p != NULL);
    ELPBUF b; memset(&b, 0xb0, sizeof b);
    /* the NULL pen answers to an EXTLOGPEN sized request with up to elpStyleEntry[1] bytes */
    int n = U(GetObjectW)(p, ELP_BASE + 4, &b);
    t_note("GetObject(null extpen, %d) = %d", ELP_BASE + 4, n);
    CHECK(n == ELP_BASE + 4 || n == sizeof(LOGPEN));
    n = U(GetObjectW)(p, sizeof b, &b);
    CHECK(n == sizeof(LOGPEN));
    CHECK(b.lp.lopnStyle == PS_NULL);
    U(DeleteObject)(p);
}

TEST(pens_ext_count_array_mismatch)
{
    NEED_EXT();
    static const DWORD arr[4] = { 5, 5, 5, 5 };
    for (DWORD st = PS_SOLID; st <= PS_DASHDOTDOT; st++) {
        DWORD ty[2] = { PS_COSMETIC, PS_GEOMETRIC };
        for (int t = 0; t < 2; t++) {
            DWORD w = t ? 4 : 1;
            EXPECT_PEN_FAIL(ext(ty[t] | st, w, BS_SOLID, 0, 0, 2, arr));
            EXPECT_PEN_FAIL(ext(ty[t] | st, w, BS_SOLID, 0, 0, 0, arr));
            EXPECT_PEN_FAIL(ext(ty[t] | st, w, BS_SOLID, 0, 0, 1, NULL));
        }
    }
    EXPECT_PEN_FAIL(ext(PS_COSMETIC | PS_ALTERNATE, 1, BS_SOLID, 0, 0, 2, arr));
    EXPECT_PEN_FAIL(ext(PS_COSMETIC | PS_ALTERNATE, 1, BS_SOLID, 0, 0, 0, arr));
}

TEST(pens_ext_userstyle_requires_array)
{
    NEED_EXT();
    static const DWORD arr[17] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17 };
    for (int t = 0; t < 2; t++) {
        DWORD ty = t ? PS_GEOMETRIC : PS_COSMETIC, w = t ? 20 : 1;
        EXPECT_PEN_FAIL(ext(ty | PS_USERSTYLE, w, BS_SOLID, 0, 0, 3, NULL));
        EXPECT_PEN_FAIL(ext(ty | PS_USERSTYLE, w, BS_SOLID, 0, 0, 0, arr));
        EXPECT_PEN_FAIL(ext(ty | PS_USERSTYLE, w, BS_SOLID, 0, 0, 0, NULL));
        EXPECT_PEN_FAIL(ext(ty | PS_USERSTYLE, w, BS_SOLID, 0, 0, 17, arr));
        EXPECT_PEN_FAIL(ext(ty | PS_USERSTYLE, w, BS_SOLID, 0, 0, 18, arr));
        EXPECT_PEN_FAIL(ext(ty | PS_USERSTYLE, w, BS_SOLID, 0, 0, 100, arr));
        SetLastError(0xdeadbeef);
        HPEN p = ext(ty | PS_USERSTYLE, w, BS_SOLID, 0, 0, (DWORD)-1, arr);
        CHECK(p == NULL);
        if (p) U(DeleteObject)(p);
    }
}

TEST(pens_ext_userstyle_counts_2_to_16)
{
    NEED_EXT();
    DWORD arr[16];
    for (int i = 0; i < 16; i++) arr[i] = 3 + i;
    for (int t = 0; t < 2; t++)
    for (DWORD cnt = 2; cnt <= 16; cnt++) {
        DWORD ty = t ? PS_GEOMETRIC : PS_COSMETIC, w = t ? 9 : 1;
        EXPECT_PEN_OK(ext(ty | PS_USERSTYLE, w, BS_SOLID, RGB(1, 2, 3), 0, cnt, arr), p);
        if (!p) continue;
        ELPBUF b; memset(&b, 0xcc, sizeof b);
        int want = ELP_BASE + 4 * cnt;
        CHECKF(U(GetObjectW)(p, 0, NULL) == want, "cnt %lu", cnt);
        int n = U(GetObjectW)(p, sizeof b, &b);
        CHECKF(n == want, "cnt %lu n=%d want %d", cnt, n, want);
        CHECK(b.e.elpPenStyle == (ty | PS_USERSTYLE));
        CHECK(b.e.elpNumEntries == cnt);
        for (DWORD i = 0; i < cnt; i++) CHECKF(b.e.elpStyleEntry[i] == arr[i], "cnt %lu entry %lu", cnt, i);
        /* too small buffer fails, exactly sized works */
        memset(&b, 0xcc, sizeof b);
        SetLastError(0xdeadbeef);
        CHECKF(U(GetObjectW)(p, want - 1, &b) == 0, "cnt %lu short", cnt);
        CHECK(U(GetObjectW)(p, want, &b) == want);
        CHECK(b.raw[want] == 0xcc);
        U(DeleteObject)(p);
    }
}

TEST(pens_ext_userstyle_count_one)
{
    NEED_EXT();
    static const DWORD arr[1] = { 4 };
    HPEN p = ext(PS_GEOMETRIC | PS_USERSTYLE, 4, BS_SOLID, 0, 0, 1, arr);
    t_note("USERSTYLE with 1 entry -> %s", p ? "created" : "failed");
    if (p) {
        ELPBUF b;
        t_note("  size %d", U(GetObjectW)(p, sizeof b, &b));
        U(DeleteObject)(p);
    }
}

TEST(pens_ext_userstyle_zero_entries)
{
    NEED_EXT();
    static const DWORD allzero[5] = { 0, 0, 0, 0, 0 };
    static const DWORD mixed[16] = { 0, 2, 0, 4, 5, 0, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0 };
    static const DWORD neg1[5] = { 4, 7, 8, 3, (DWORD)-1 };
    EXPECT_PEN_FAIL(ext(PS_GEOMETRIC | PS_USERSTYLE, 50, BS_SOLID, 0, 0, 5, allzero));
    HPEN cz = ext(PS_COSMETIC | PS_USERSTYLE, 1, BS_SOLID, 0, 0, 5, allzero);
    t_note("cosmetic all-zero userstyle -> %s", cz ? "created" : "failed");
    if (cz) U(DeleteObject)(cz);
    EXPECT_PEN_FAIL(ext(PS_GEOMETRIC | PS_USERSTYLE, 50, BS_SOLID, 0, 0, 5, neg1));
    EXPECT_PEN_OK(ext(PS_GEOMETRIC | PS_USERSTYLE, 50, BS_SOLID, 0, 0, 16, mixed), p);
    if (p) {
        ELPBUF b;
        CHECK(U(GetObjectW)(p, sizeof b, &b) == ELP_BASE + 64);
        for (int i = 0; i < 16; i++) CHECK(b.e.elpStyleEntry[i] == mixed[i]);
        U(DeleteObject)(p);
    }
}

TEST(pens_ext_userstyle_large_values)
{
    NEED_EXT();
    static const DWORD big[2] = { 0x7fffffff, 0x7fffffff };
    static const DWORD huge[2] = { 0xffffffff, 0xffffffff };
    static const DWORD one[2] = { 1, 1 };
    HPEN p = ext(PS_GEOMETRIC | PS_USERSTYLE, 3, BS_SOLID, 0, 0, 2, big);
    t_note("userstyle 0x7fffffff x2 -> %s", p ? "created" : "failed");
    if (p) U(DeleteObject)(p);
    p = ext(PS_GEOMETRIC | PS_USERSTYLE, 3, BS_SOLID, 0, 0, 2, huge);
    t_note("userstyle 0xffffffff x2 -> %s", p ? "created" : "failed");
    if (p) U(DeleteObject)(p);
    EXPECT_PEN_OK(ext(PS_GEOMETRIC | PS_USERSTYLE, 3, BS_SOLID, 0, 0, 2, one), q);
    if (q) U(DeleteObject)(q);
    static const DWORD v[2] = { 0xabc, 0xdef };
    EXPECT_PEN_OK(ext(PS_COSMETIC | PS_USERSTYLE, 1, BS_SOLID, 0, 0, 2, v), r);
    if (r) {
        ELPBUF b;
        CHECK(U(GetObjectW)(r, sizeof b, &b) == ELP_BASE + 8);
        CHECK(b.e.elpStyleEntry[0] == 0xabc && b.e.elpStyleEntry[1] == 0xdef);
        U(DeleteObject)(r);
    }
}

TEST(pens_ext_userstyle_array_is_copied)
{
    NEED_EXT();
    DWORD arr[3] = { 7, 8, 9 };
    EXPECT_PEN_OK(ext(PS_GEOMETRIC | PS_USERSTYLE, 3, BS_SOLID, 0, 0, 3, arr), p);
    arr[0] = arr[1] = arr[2] = 1000;
    if (p) {
        ELPBUF b;
        CHECK(U(GetObjectW)(p, sizeof b, &b) == ELP_BASE + 12);
        CHECK(b.e.elpStyleEntry[0] == 7 && b.e.elpStyleEntry[1] == 8 && b.e.elpStyleEntry[2] == 9);
        U(DeleteObject)(p);
    }
}

TEST(pens_ext_getobject_small_buffers)
{
    NEED_EXT();
    HPEN p = ext_solid(PS_GEOMETRIC | PS_DASH, 4);
    ELPBUF b;
    for (int sz = 1; sz < ELP_BASE; sz++) {
        memset(&b, 0xb0, sizeof b);
        SetLastError(0xdeadbeef);
        int n = U(GetObjectW)(p, sz, &b);
        CHECKF(n == 0, "size %d n=%d", sz, n);
    }
    memset(&b, 0xb0, sizeof b);
    CHECK(U(GetObjectW)(p, ELP_BASE, &b) == ELP_BASE);
    CHECK(b.e.elpNumEntries == 0);
    CHECK(U(GetObjectW)(p, sizeof(LOGPEN), &b) == 0);
    CHECK(U(GetObjectW)(p, ELP_BASE + 1, &b) == ELP_BASE);
    CHECK(U(GetObjectW)(p, 1000, &b) == ELP_BASE);
    U(DeleteObject)(p);
}

TEST(pens_ext_getobject_sizeof_extlogpen)
{
    NEED_EXT();
    HPEN p = ext_solid(PS_COSMETIC | PS_SOLID, 1);
    EXTLOGPEN e;
    memset(&e, 0xb0, sizeof e);
    int n = U(GetObjectW)(p, sizeof e, &e);
    CHECK(n == ELP_BASE);
    CHECK(sizeof(EXTLOGPEN) == 32 && ELP_BASE == 28);
    U(DeleteObject)(p);
}

TEST(pens_ext_color_roundtrip)
{
    NEED_EXT();
    for (int v = 0; v < 256; v += 5) {
        COLORREF c = RGB(v, 255 - v, v ^ 0x55);
        HPEN p = ext(PS_GEOMETRIC | PS_SOLID, 3, BS_SOLID, c, 0, 0, NULL);
        CHECK(p != NULL);
        ELPBUF b;
        CHECK(U(GetObjectW)(p, sizeof b, &b) == ELP_BASE);
        CHECKF(b.e.elpColor == c, "v=%d", v);
        U(DeleteObject)(p);
    }
}

TEST(pens_ext_solid_hatch_field_preserved)
{
    NEED_EXT();
    static const ULONG_PTR h[] = { 0, 1, 4, 5, 6, 100 };
    for (unsigned i = 0; i < 6; i++) {
        HPEN p = ext(PS_GEOMETRIC | PS_SOLID, 3, BS_SOLID, RGB(1, 2, 3), h[i], 0, NULL);
        CHECK(p != NULL);
        ELPBUF b;
        CHECK(U(GetObjectW)(p, sizeof b, &b) == ELP_BASE);
        if (b.e.elpHatch != h[i]) t_note("BS_SOLID pen hatch %Iu reads back %Iu", h[i], b.e.elpHatch);
        CHECK(b.e.elpBrushStyle == BS_SOLID);
        U(DeleteObject)(p);
    }
}

TEST(pens_ext_hatched_brush_pen)
{
    NEED_EXT();
    for (ULONG_PTR h = HS_HORIZONTAL; h <= HS_DIAGCROSS; h++) {
        HPEN p = ext(PS_GEOMETRIC | PS_SOLID, 4, BS_HATCHED, RGB(10, 20, 30), h, 0, NULL);
        CHECKF(p != NULL, "hatch %Iu", h);
        if (!p) continue;
        ELPBUF b;
        CHECK(U(GetObjectW)(p, sizeof b, &b) == ELP_BASE);
        CHECK(b.e.elpBrushStyle == BS_HATCHED);
        CHECK(b.e.elpHatch == h);
        CHECK(b.e.elpColor == RGB(10, 20, 30));
        U(DeleteObject)(p);
    }
    /* cosmetic pens only take BS_SOLID */
    EXPECT_PEN_FAIL(ext(PS_COSMETIC | PS_SOLID, 1, BS_HATCHED, 0, HS_CROSS, 0, NULL));
}

TEST(pens_ext_hollow_brush_pen)
{
    NEED_EXT();
    for (DWORD ty = PS_GEOMETRIC; ty; ty = 0) {
        HPEN p = ext(ty | PS_SOLID, 3, BS_NULL, 0, 0, 0, NULL);
        if (!ty) {
            CHECK(p == NULL);
            if (p) U(DeleteObject)(p);
            break;
        }
        CHECK(p != NULL);
        LOGPEN lp;
        CHECK(U(GetObjectW)(p, sizeof lp, &lp) == sizeof lp);
        CHECK(lp.lopnStyle == PS_NULL);
        U(DeleteObject)(p);
    }
    EXPECT_PEN_FAIL(ext(PS_COSMETIC | PS_SOLID, 1, BS_NULL, 0, 0, 0, NULL));
}

TEST(pens_ext_pattern_brush_pen)
{
    NEED_EXT(); REQUIRE(U(CreateBitmap));
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, NULL);
    CHECK(bm != NULL);
    HPEN p = ext(PS_GEOMETRIC | PS_SOLID, 8, BS_PATTERN, RGB(1, 2, 3), (ULONG_PTR)bm, 0, NULL);
    CHECK(p != NULL);
    if (p) {
        ELPBUF b;
        CHECK(U(GetObjectW)(p, sizeof b, &b) == ELP_BASE);
        CHECK(b.e.elpBrushStyle == BS_PATTERN);
        CHECK(b.e.elpHatch == (ULONG_PTR)bm);
        CHECK(b.e.elpColor == 0);
        U(DeleteObject)(p);
    }
    EXPECT_PEN_FAIL(ext(PS_GEOMETRIC | PS_SOLID, 8, BS_PATTERN, 0, 0, 0, NULL));
    EXPECT_PEN_FAIL(ext(PS_COSMETIC | PS_SOLID, 1, BS_PATTERN, 0, (ULONG_PTR)bm, 0, NULL));
    U(DeleteObject)(bm);
}

TEST(pens_ext_pattern_pen_survives_bitmap_delete)
{
    NEED_EXT(); REQUIRE(U(CreateBitmap));
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, NULL);
    HPEN p = ext(PS_GEOMETRIC | PS_SOLID, 8, BS_PATTERN, 0, (ULONG_PTR)bm, 0, NULL);
    CHECK(p != NULL);
    U(DeleteObject)(bm);
    if (p) {
        ELPBUF b;
        CHECK(U(GetObjectW)(p, sizeof b, &b) == ELP_BASE);
        CHECK(b.e.elpBrushStyle == BS_PATTERN);
        CHECK(U(DeleteObject)(p));
    }
}

static BITMAPINFO *mk_dib(int w, int h, int bpp, void **mem_out)
{
    size_t sz = sizeof(BITMAPINFOHEADER) + 256 * 4 + (size_t)w * h * 4;
    BITMAPINFO *bi = calloc(1, sz);
    bi->bmiHeader.biSize = sizeof bi->bmiHeader;
    bi->bmiHeader.biWidth = w; bi->bmiHeader.biHeight = h;
    bi->bmiHeader.biPlanes = 1; bi->bmiHeader.biBitCount = bpp;
    bi->bmiHeader.biCompression = BI_RGB;
    return bi;
}

TEST(pens_ext_dibpattern_pens)
{
    NEED_EXT();
    BITMAPINFO *bi = mk_dib(8, 8, 32, NULL);
    HGLOBAL hm = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(BITMAPINFOHEADER) + 8 * 8 * 4);
    BITMAPINFO *pk = GlobalLock(hm);
    memcpy(pk, bi, sizeof(BITMAPINFOHEADER));
    GlobalUnlock(hm);
    LOGBRUSH lb;
    for (int k = 0; k < 2; k++) {
        lb.lbStyle = k ? BS_DIBPATTERN : BS_DIBPATTERNPT;
        lb.lbColor = DIB_RGB_COLORS;
        lb.lbHatch = k ? (ULONG_PTR)hm : (ULONG_PTR)bi;
        SetLastError(0xdeadbeef);
        HPEN p = U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID, 8, &lb, 0, NULL);
        CHECKF(p != NULL, "k=%d gle=%lu", k, GetLastError());
        if (p) {
            ELPBUF b;
            CHECK(U(GetObjectW)(p, sizeof b, &b) == ELP_BASE);
            CHECK(b.e.elpBrushStyle == BS_DIBPATTERNPT);
            CHECK(b.e.elpHatch == lb.lbHatch);
            CHECK(b.e.elpColor == 0);
            U(DeleteObject)(p);
        }
        /* cosmetic pens must be solid */
        EXPECT_PEN_FAIL(U(ExtCreatePen)(PS_COSMETIC | PS_SOLID, 1, &lb, 0, NULL));
    }
    GlobalFree(hm);
    free(bi);
}

TEST(pens_ext_dibpattern_bad_header)
{
    NEED_EXT();
    BITMAPINFO *bi = mk_dib(8, 8, 32, NULL);
    LOGBRUSH lb = { BS_DIBPATTERNPT, DIB_RGB_COLORS, (ULONG_PTR)bi };
    bi->bmiHeader.biSize = 8;
    HPEN p = U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID, 8, &lb, 0, NULL);
    t_note("DIB pattern pen with biSize=8 -> %s", p ? "created" : "failed");
    if (p) U(DeleteObject)(p);
    bi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi->bmiHeader.biBitCount = 7;
    p = U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID, 8, &lb, 0, NULL);
    t_note("DIB pattern pen with 7bpp -> %s", p ? "created" : "failed");
    if (p) U(DeleteObject)(p);
    free(bi);
}

TEST(pens_ext_invalid_brush_styles)
{
    NEED_EXT();
    for (UINT bs = 0; bs <= 12; bs++) {
        int valid = bs == BS_SOLID || bs == BS_NULL || bs == BS_HATCHED || bs == BS_PATTERN || bs == BS_DIBPATTERN || bs == BS_DIBPATTERNPT;
        if (valid) continue;
        SetLastError(0xdeadbeef);
        HPEN p = ext(PS_GEOMETRIC | PS_DOT, 3, bs, RGB(1, 2, 3), (ULONG_PTR)HS_CROSS, 0, NULL);
        CHECKF(p == NULL, "brush style %u", bs);
        if (p) U(DeleteObject)(p); else EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    }
    for (UINT bs = 0; bs <= 12; bs++) {
        if (bs == BS_SOLID) continue;
        HPEN p = ext(PS_COSMETIC | PS_DOT, 1, bs, RGB(1, 2, 3), HS_CROSS, 0, NULL);
        CHECKF(p == NULL, "cosmetic brush style %u", bs);
        if (p) U(DeleteObject)(p);
    }
}

TEST(pens_ext_invalid_brush_style_huge)
{
    NEED_EXT();
    static const UINT bs[] = { 13, 100, 0x100, 0xffff, 0x10000, 0x7fffffff, 0x80000000u, 0xffffffffu };
    for (unsigned i = 0; i < sizeof bs / sizeof *bs; i++) {
        HPEN p = ext(PS_GEOMETRIC | PS_SOLID, 3, bs[i], 0, 0, 0, NULL);
        CHECKF(p == NULL, "bs %x", bs[i]);
        if (p) U(DeleteObject)(p);
    }
}

TEST(pens_ext_userstyle_with_other_brushes)
{
    NEED_EXT();
    static const DWORD v[2] = { 6, 6 };
    HPEN p = ext(PS_GEOMETRIC | PS_USERSTYLE, 6, BS_HATCHED, RGB(5, 5, 5), HS_VERTICAL, 2, v);
    CHECK(p != NULL);
    if (p) {
        ELPBUF b;
        CHECK(U(GetObjectW)(p, sizeof b, &b) == ELP_BASE + 8);
        CHECK(b.e.elpBrushStyle == BS_HATCHED && b.e.elpHatch == HS_VERTICAL);
        CHECK(b.e.elpNumEntries == 2);
        U(DeleteObject)(p);
    }
}

TEST(pens_ext_handle_lifecycle)
{
    NEED_EXT();
    HPEN a = ext_solid(PS_GEOMETRIC | PS_SOLID, 3);
    HPEN b = ext_solid(PS_GEOMETRIC | PS_SOLID, 3);
    CHECK(a && b && a != b);
    CHECK(U(DeleteObject)(a));
    ELPBUF e;
    CHECK(U(GetObjectW)(a, sizeof e, &e) == 0);
    CHECK(U(GetObjectType)(a) == 0);
    CHECK(!U(DeleteObject)(a));
    CHECK(U(GetObjectW)(b, sizeof e, &e) == ELP_BASE);
    CHECK(U(DeleteObject)(b));
}

TEST(pens_ext_select_into_dc)
{
    NEED_EXT(); NEED_MEM(); REQUIRE(U(GetCurrentObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ def = U(GetCurrentObject)(dc, OBJ_PEN);
    HPEN p = ext_solid(PS_GEOMETRIC | PS_SOLID, 4);
    CHECK(U(SelectObject)(dc, p) == def);
    HGDIOBJ cur = U(GetCurrentObject)(dc, OBJ_PEN);
    CHECK(cur == p);
    cur = U(GetCurrentObject)(dc, OBJ_EXTPEN);
    t_note("GetCurrentObject(OBJ_EXTPEN) %s the selected pen", cur == p ? "returns" : "does not return");
    CHECK(U(SelectObject)(dc, def) == p);
    CHECK(U(DeleteObject)(p));
    CHECK(U(DeleteDC)(dc));
}

TEST(pens_ext_leak_loop)
{
    NEED_EXT();
    DWORD before = pb_gdi_count();
    if (!before) { t_skip("GetGuiResources unavailable"); return; }
    static const DWORD v[2] = { 2, 3 };
    for (int i = 0; i < 2000; i++) {
        HPEN p = (i & 1) ? ext(PS_GEOMETRIC | PS_USERSTYLE, 4, BS_SOLID, i, 0, 2, v) : ext_solid(PS_COSMETIC | PS_DOT, 1);
        if (!p) { CHECK(p != NULL); break; }
        U(DeleteObject)(p);
    }
    DWORD after = pb_gdi_count();
    CHECKF(after <= before + 2, "before %lu after %lu", before, after);
}

TEST(pens_ext_failed_creates_do_not_leak)
{
    NEED_EXT();
    DWORD before = pb_gdi_count();
    if (!before) { t_skip("GetGuiResources unavailable"); return; }
    static const DWORD v[2] = { 0, 0 };
    for (int i = 0; i < 500; i++) {
        HPEN p = ext_solid(PS_COSMETIC | PS_DOT, 5);
        if (p) U(DeleteObject)(p);
        p = ext(PS_GEOMETRIC | PS_USERSTYLE, 4, BS_SOLID, 0, 0, 2, v);
        if (p) U(DeleteObject)(p);
        p = ext(PS_GEOMETRIC | PS_SOLID, 4, 77, 0, 0, 0, NULL);
        if (p) U(DeleteObject)(p);
    }
    DWORD after = pb_gdi_count();
    CHECKF(after <= before + 1, "before %lu after %lu", before, after);
}

typedef struct { HPEN pen; int n; ELPBUF b; } XE;
static DWORD WINAPI xe_proc(LPVOID a)
{
    XE *x = a;
    x->n = U(GetObjectW)(x->pen, sizeof x->b, &x->b);
    return 0;
}
TEST(pens_ext_cross_thread_getobject)
{
    NEED_EXT();
    static const DWORD v[3] = { 4, 5, 6 };
    XE x; memset(&x, 0, sizeof x);
    x.pen = ext(PS_GEOMETRIC | PS_USERSTYLE | PS_ENDCAP_FLAT, 7, BS_SOLID, RGB(9, 9, 9), 0, 3, v);
    CHECK(x.pen != NULL);
    HANDLE th = CreateThread(NULL, 0, xe_proc, &x, 0, NULL);
    WaitForSingleObject(th, 10000);
    CloseHandle(th);
    CHECK(x.n == ELP_BASE + 12);
    CHECK(x.b.e.elpNumEntries == 3 && x.b.e.elpStyleEntry[2] == 6);
    CHECK(x.b.e.elpPenStyle == (PS_GEOMETRIC | PS_USERSTYLE | PS_ENDCAP_FLAT));
    U(DeleteObject)(x.pen);
}

TEST(pens_ext_null_logbrush_style_fields_only)
{
    NEED_EXT();
    /* BS_SOLID ignores lbHatch for validation */
    static const ULONG_PTR h[] = { 0, 0xdeadbeef, ~(ULONG_PTR)0 };
    for (unsigned i = 0; i < 3; i++) {
        HPEN p = ext(PS_GEOMETRIC | PS_SOLID, 2, BS_SOLID, 0x112233, h[i], 0, NULL);
        CHECKF(p != NULL, "hatch %Ix", h[i]);
        if (p) U(DeleteObject)(p);
    }
}

TEST(pens_ext_style_type_mask_values)
{
    NEED_EXT();
    CHECK(PS_TYPE_MASK == 0x000F0000 && PS_STYLE_MASK == 0xF && PS_ENDCAP_MASK == 0xF00 && PS_JOIN_MASK == 0xF000);
    /* cosmetic = type 0 explicitly */
    EXPECT_PEN_OK(ext_solid(0 | PS_DASH, 1), p);
    if (p) {
        ELPBUF b;
        CHECK(U(GetObjectW)(p, sizeof b, &b) == ELP_BASE);
        CHECK(b.e.elpPenStyle == PS_DASH);
        U(DeleteObject)(p);
    }
}

TEST(pens_ext_userstyle_value_limits)
{
    NEED_EXT();
    static const DWORD lim[] = { 0x3ffe, 0x3fff, 0x4000, 0x4001, 0x8000, 0xffff, 0x10000 };
    for (unsigned i = 0; i < sizeof lim / sizeof *lim; i++) {
        DWORD arr[2] = { lim[i], 1 };
        HPEN p = ext(PS_GEOMETRIC | PS_USERSTYLE, 3, BS_SOLID, 0, 0, 2, arr);
        t_note("userstyle entry %lu -> %s", lim[i], p ? "created" : "failed");
        if (p) U(DeleteObject)(p);
    }
    for (DWORD tot = 0x3ff0; tot <= 0x4010; tot += 8) {
        DWORD arr[2] = { tot / 2, tot - tot / 2 };
        HPEN p = ext(PS_GEOMETRIC | PS_USERSTYLE, 3, BS_SOLID, 0, 0, 2, arr);
        t_note("userstyle total %lu -> %s", tot, p ? "created" : "failed");
        if (p) U(DeleteObject)(p);
    }
}
