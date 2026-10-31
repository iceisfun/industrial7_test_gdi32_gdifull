#include "gap.h"

/* GetBrushAttributes(h) is, in gdi32full, a pure function of the handle value: bit 23
 * (the "stock object" bit of the handle) is returned as 0/1. SetBrushAttributes and
 * ClearBrushAttributes reject any flag word other than 0 or 1 before entering the kernel. */

static const ULONG_PTR k_handles[] = {
    0, 1, 2, 0x7fff, 0x8000, 0xffff, 0x10000, 0x00800000, 0x00800001, 0x00ffffff, 0x01000000,
    0x01900010, 0x01800010, 0x01900011, 0x018a0048, 0x01850000, 0x00100000, 0x00400000,
    0x007fffff, 0x01234567, 0x7fffffff, 0x80000000, 0xffffffff, 0x0000000100800000ULL,
    0xffffffff00800000ULL, 0xffffffff00000000ULL, 0xffffffffffffffffULL, 0x0123456789abcdefULL,
};

TEST(gap1_getbrushattr_pure_bit23_table) {
    
    GetBrushAttributes_pfn f = UN(GetBrushAttributes);
    REQUIRE(f);
    for (unsigned i = 0; i < sizeof k_handles / sizeof *k_handles; i++) {
        DWORD want = (DWORD)((k_handles[i] >> 23) & 1);
        DWORD got = f((HBRUSH)k_handles[i]);
        CHECKF(got == want, "handle %#llx got %lu want %lu", (unsigned long long)k_handles[i], got, want);
    }
}

TEST(gap1_getbrushattr_single_bits) {
    GetBrushAttributes_pfn f = UN(GetBrushAttributes);
    REQUIRE(f);
    for (int b = 0; b < 64; b++) {
        ULONG_PTR h = (ULONG_PTR)1 << b;
        DWORD got = f((HBRUSH)h);
        CHECKF(got == (b == 23), "bit %d got %lu", b, got);
    }
}

TEST(gap1_getbrushattr_all_ones_except_bit23) {
    GetBrushAttributes_pfn f = UN(GetBrushAttributes);
    REQUIRE(f);
    ULONG_PTR all = ~(ULONG_PTR)0;
    CHECK(f((HBRUSH)all) == 1);
    CHECK(f((HBRUSH)(all & ~((ULONG_PTR)1 << 23))) == 0);
}

TEST(gap1_getbrushattr_null_is_zero) {
    GetBrushAttributes_pfn f = UN(GetBrushAttributes);
    REQUIRE(f);
    SetLastError(0x5a5a);
    CHECK(f(NULL) == 0);
    EXPECT_ERR(0, 0x5a5a);   /* the function never touches last-error */
}

TEST(gap1_getbrushattr_no_lasterror_for_garbage) {
    GetBrushAttributes_pfn f = UN(GetBrushAttributes);
    REQUIRE(f);
    SetLastError(0x1234);
    f((HBRUSH)(ULONG_PTR)0xdeadbeef);
    f((HBRUSH)~(ULONG_PTR)0);
    EXPECT_ERR(0, 0x1234);
}

TEST(gap1_getbrushattr_stock_objects_have_bit) {
    REQUIRE(U(GetStockObject));
    GetBrushAttributes_pfn f = UN(GetBrushAttributes);
    REQUIRE(f);
    int seen = 0;
    for (int i = 0; i <= 21; i++) {
        HGDIOBJ h = U(GetStockObject)(i);
        if (!h) continue;
        seen++;
        CHECKF(f((HBRUSH)h) == 1, "stock %d handle %p", i, h);
    }
    CHECK(seen >= 17);
}

TEST(gap1_getbrushattr_stock_brush_handles) {
    REQUIRE(U(GetStockObject));
    GetBrushAttributes_pfn f = UN(GetBrushAttributes);
    REQUIRE(f);
    static const int brushes[] = { WHITE_BRUSH, LTGRAY_BRUSH, GRAY_BRUSH, DKGRAY_BRUSH, BLACK_BRUSH, NULL_BRUSH, DC_BRUSH };
    for (unsigned i = 0; i < sizeof brushes / sizeof *brushes; i++) {
        HGDIOBJ h = U(GetStockObject)(brushes[i]);
        CHECKF(h != NULL, "stock brush %d", brushes[i]);
        CHECKF(f((HBRUSH)h) == 1, "stock brush %d", brushes[i]);
    }
}

TEST(gap1_getbrushattr_created_brush_clear) {
    REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    GetBrushAttributes_pfn f = UN(GetBrushAttributes);
    REQUIRE(f);
    HBRUSH b = U(CreateSolidBrush)(RGB(1, 2, 3));
    CHECK(b != NULL);
    CHECK(f(b) == 0);
    U(DeleteObject)(b);
}

TEST(gap1_getbrushattr_other_object_kinds) {
    REQUIRE(U(CreatePen)); REQUIRE(U(CreateRectRgn)); REQUIRE(U(CreateBitmap));
    REQUIRE(U(CreateHatchBrush));
    GetBrushAttributes_pfn f = UN(GetBrushAttributes);
    REQUIRE(f);
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0);
    HRGN r = U(CreateRectRgn)(0, 0, 4, 4);
    HBITMAP bm = U(CreateBitmap)(4, 4, 1, 1, NULL);
    HBRUSH hb = U(CreateHatchBrush)(HS_CROSS, 0);
    CHECK(f((HBRUSH)p) == 0);
    CHECK(f((HBRUSH)r) == 0);
    CHECK(f((HBRUSH)bm) == 0);
    CHECK(f(hb) == 0);
    U(DeleteObject)(p); U(DeleteObject)(r); U(DeleteObject)(bm); U(DeleteObject)(hb);
}

TEST(gap1_getbrushattr_dc_handle) {
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    GetBrushAttributes_pfn f = UN(GetBrushAttributes);
    REQUIRE(f);
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(dc != NULL);
    CHECK(f((HBRUSH)dc) == 0);
    U(DeleteDC)(dc);
}

TEST(gap1_getbrushattr_deleted_brush_keeps_bit_clear) {
    REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    GetBrushAttributes_pfn f = UN(GetBrushAttributes);
    REQUIRE(f);
    HBRUSH b = U(CreateSolidBrush)(RGB(9, 9, 9));
    U(DeleteObject)(b);
    CHECK(f(b) == 0);   /* value-only function: stale handle still has bit 23 clear */
}

TEST(gap1_getbrushattr_gdi32_vs_gdi32full_parity) {
    GetBrushAttributes_pfn a = (GetBrushAttributes_pfn)t_proc_gdi32("GetBrushAttributes");
    GetBrushAttributes_pfn b = (GetBrushAttributes_pfn)t_proc_gdi32full("GetBrushAttributes");
    REQUIRE(a); REQUIRE(b);
    for (unsigned i = 0; i < sizeof k_handles / sizeof *k_handles; i++)
        CHECKF(a((HBRUSH)k_handles[i]) == b((HBRUSH)k_handles[i]), "handle %#llx", (unsigned long long)k_handles[i]);
}

TEST(gap1_getbrushattr_equals_getbitmapattributes) {
    GetBrushAttributes_pfn f = UN(GetBrushAttributes);
    GetBitmapAttributes_pfn g = UN(GetBitmapAttributes);
    REQUIRE(f); REQUIRE(g);
    for (unsigned i = 0; i < sizeof k_handles / sizeof *k_handles; i++)
        CHECKF(f((HBRUSH)k_handles[i]) == g((HBITMAP)k_handles[i]), "handle %#llx", (unsigned long long)k_handles[i]);
    t_note("same code address in gdi32full: %d",
           t_proc_gdi32full("GetBrushAttributes") == t_proc_gdi32full("GetBitmapAttributes"));
}

TEST(gap1_getbrushattr_repeatable) {
    GetBrushAttributes_pfn f = UN(GetBrushAttributes);
    REQUIRE(f);
    for (int i = 0; i < 1000; i++) {
        if (f((HBRUSH)(ULONG_PTR)0x00800000) != 1 || f((HBRUSH)(ULONG_PTR)0x00400000) != 0) { CHECK(0); return; }
    }
    CHECK(1);
}

/* ---- flag-word validation (no kernel involvement) ---- */
static const DWORD k_badflags[] = { 2, 3, 4, 5, 8, 0x10, 0x80, 0x100, 0xffff, 0x10000, 0x7ffffffe, 0x80000000, 0x80000001, 0xfffffffe, 0xffffffff };

TEST(gap1_setbrushattr_bad_flags_stock_brush) {
    REQUIRE(U(GetStockObject));
    SetBrushAttributes_pfn f = UN(SetBrushAttributes);
    REQUIRE(f);
    HBRUSH b = U(GetStockObject)(WHITE_BRUSH);
    for (unsigned i = 0; i < sizeof k_badflags / sizeof *k_badflags; i++)
        CHECKF(f(b, k_badflags[i]) == NULL, "flags %#lx", k_badflags[i]);
}

TEST(gap1_setbrushattr_bad_flags_null_brush) {
    SetBrushAttributes_pfn f = UN(SetBrushAttributes);
    REQUIRE(f);
    for (unsigned i = 0; i < sizeof k_badflags / sizeof *k_badflags; i++)
        CHECKF(f(NULL, k_badflags[i]) == NULL, "flags %#lx", k_badflags[i]);
}

TEST(gap1_setbrushattr_bad_flags_created_brush_unchanged) {
    REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectType));
    SetBrushAttributes_pfn f = UN(SetBrushAttributes);
    GetBrushAttributes_pfn g = UN(GetBrushAttributes);
    REQUIRE(f); REQUIRE(g);
    HBRUSH b = U(CreateSolidBrush)(RGB(10, 20, 30));
    for (unsigned i = 0; i < sizeof k_badflags / sizeof *k_badflags; i++) {
        CHECKF(f(b, k_badflags[i]) == NULL, "flags %#lx", k_badflags[i]);
        CHECKF(g(b) == 0, "flags %#lx", k_badflags[i]);
    }
    CHECK(U(GetObjectType)(b) == OBJ_BRUSH);
    CHECK(U(DeleteObject)(b));
}

TEST(gap1_setbrushattr_bad_flags_gdi32full_direct) {
    SetBrushAttributes_pfn f = (SetBrushAttributes_pfn)t_proc_gdi32full("SetBrushAttributes");
    REQUIRE(f);
    for (unsigned i = 0; i < sizeof k_badflags / sizeof *k_badflags; i++)
        CHECKF(f((HBRUSH)(ULONG_PTR)0x1234, k_badflags[i]) == NULL, "flags %#lx", k_badflags[i]);
}

TEST(gap1_setbrushattr_bad_flags_gdi32_forwarder) {
    SetBrushAttributes_pfn f = (SetBrushAttributes_pfn)t_proc_gdi32("SetBrushAttributes");
    REQUIRE(f);
    for (unsigned i = 0; i < sizeof k_badflags / sizeof *k_badflags; i++)
        CHECKF(f((HBRUSH)(ULONG_PTR)0x1234, k_badflags[i]) == NULL, "flags %#lx", k_badflags[i]);
}

TEST(gap1_setbrushattr_invalid_handle_valid_flag) {
    SKIP_WINE("kernel stub");
    SetBrushAttributes_pfn f = UN(SetBrushAttributes);
    REQUIRE(f);
    static const ULONG_PTR hs[] = { 0, 1, 0x1234, 0xffff, 0x7fffffff, 0xffffffff, ~(ULONG_PTR)0 };
    for (unsigned i = 0; i < sizeof hs / sizeof *hs; i++) {
        HBRUSH r = f((HBRUSH)hs[i], 1);
        CHECKF(r == NULL, "handle %#llx -> %p", (unsigned long long)hs[i], r);
    }
}

TEST(gap1_setbrushattr_invalid_handle_zero_flag) {
    SKIP_WINE("kernel stub");
    SetBrushAttributes_pfn f = UN(SetBrushAttributes);
    REQUIRE(f);
    static const ULONG_PTR hs[] = { 0, 1, 0x1234, 0xffff, 0x7fffffff };
    for (unsigned i = 0; i < sizeof hs / sizeof *hs; i++) {
        HBRUSH r = f((HBRUSH)hs[i], 0);
        CHECKF(r == NULL, "handle %#llx -> %p", (unsigned long long)hs[i], r);
    }
}

TEST(gap1_clearbrushattr_bad_flags_stock_brush) {
    REQUIRE(U(GetStockObject));
    ClearBrushAttributes_pfn f = UN(ClearBrushAttributes);
    REQUIRE(f);
    HBRUSH b = U(GetStockObject)(BLACK_BRUSH);
    for (unsigned i = 0; i < sizeof k_badflags / sizeof *k_badflags; i++)
        CHECKF(f(b, k_badflags[i]) == NULL, "flags %#lx", k_badflags[i]);
}

TEST(gap1_clearbrushattr_bad_flags_null_brush) {
    ClearBrushAttributes_pfn f = UN(ClearBrushAttributes);
    REQUIRE(f);
    for (unsigned i = 0; i < sizeof k_badflags / sizeof *k_badflags; i++)
        CHECKF(f(NULL, k_badflags[i]) == NULL, "flags %#lx", k_badflags[i]);
}

TEST(gap1_clearbrushattr_bad_flags_created_brush) {
    REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    ClearBrushAttributes_pfn f = UN(ClearBrushAttributes);
    REQUIRE(f);
    HBRUSH b = U(CreateSolidBrush)(RGB(5, 6, 7));
    for (unsigned i = 0; i < sizeof k_badflags / sizeof *k_badflags; i++)
        CHECKF(f(b, k_badflags[i]) == NULL, "flags %#lx", k_badflags[i]);
    CHECK(U(DeleteObject)(b));
}

TEST(gap1_clearbrushattr_bad_flags_both_modules) {
    ClearBrushAttributes_pfn a = (ClearBrushAttributes_pfn)t_proc_gdi32("ClearBrushAttributes");
    ClearBrushAttributes_pfn b = (ClearBrushAttributes_pfn)t_proc_gdi32full("ClearBrushAttributes");
    REQUIRE(a); REQUIRE(b);
    for (unsigned i = 0; i < sizeof k_badflags / sizeof *k_badflags; i++) {
        CHECKF(a((HBRUSH)(ULONG_PTR)0x4321, k_badflags[i]) == NULL, "gdi32 flags %#lx", k_badflags[i]);
        CHECKF(b((HBRUSH)(ULONG_PTR)0x4321, k_badflags[i]) == NULL, "gdi32full flags %#lx", k_badflags[i]);
    }
}

TEST(gap1_clearbrushattr_invalid_handle_valid_flag) {
    SKIP_WINE("kernel stub");
    ClearBrushAttributes_pfn f = UN(ClearBrushAttributes);
    REQUIRE(f);
    static const ULONG_PTR hs[] = { 0, 1, 0x1234, 0xffff, 0x7fffffff, ~(ULONG_PTR)0 };
    for (unsigned i = 0; i < sizeof hs / sizeof *hs; i++) {
        HBRUSH r = f((HBRUSH)hs[i], 1);
        CHECKF(r == NULL, "handle %#llx -> %p", (unsigned long long)hs[i], r);
    }
}

TEST(gap1_clearbrushattr_created_brush_flag_roundtrip) {
    SKIP_WINE("kernel stub");
    REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectType));
    ClearBrushAttributes_pfn f = UN(ClearBrushAttributes);
    GetBrushAttributes_pfn g = UN(GetBrushAttributes);
    REQUIRE(f); REQUIRE(g);
    HBRUSH b = U(CreateSolidBrush)(RGB(100, 100, 100));
    HBRUSH r = f(b, 1);   /* clearing a stock bit that is not set */
    t_note("ClearBrushAttributes(non-stock, 1) -> %p (orig %p)", r, b);
    HBRUSH use = r ? r : b;
    CHECK(g(use) == 0);
    CHECK(U(GetObjectType)(use) == OBJ_BRUSH);
    CHECK(U(DeleteObject)(use));
}

TEST(gap1_setbrushattr_stock_flag_roundtrip_cleanup) {
    SKIP_WINE("kernel stub");
    REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectType));
    SetBrushAttributes_pfn s = UN(SetBrushAttributes);
    ClearBrushAttributes_pfn c = UN(ClearBrushAttributes);
    GetBrushAttributes_pfn g = UN(GetBrushAttributes);
    REQUIRE(s); REQUIRE(c); REQUIRE(g);
    DWORD before = gap_gdi_count();
    HBRUSH b = U(CreateSolidBrush)(RGB(11, 22, 33));
    HBRUSH r = s(b, 1);
    t_note("SetBrushAttributes(brush, 1) -> %p (orig %p) bit=%lu", r, b, r ? g(r) : 0);
    HBRUSH cur = r ? r : b;
    /* restore to an ordinary deletable brush whatever happened */
    HBRUSH r2 = c(cur, 1);
    t_note("ClearBrushAttributes(.., 1) -> %p", r2);
    if (r2) cur = r2;
    CHECK(g(cur) == 0);
    BOOL del = U(DeleteObject)(cur);
    t_note("DeleteObject after restore -> %d", del);
    if (!del && cur != b) U(DeleteObject)(b);
    t_note("gdi objects before=%lu after=%lu", before, gap_gdi_count());
}

TEST(gap1_brushattr_flags_zero_and_one_accepted_by_validator) {
    /* bad flag words are rejected up front; 0 and 1 are not: with a bogus handle the
     * kernel decides, still NULL, but the result must not depend on the high bits being set */
    SKIP_WINE("kernel stub");
    SetBrushAttributes_pfn s = UN(SetBrushAttributes);
    ClearBrushAttributes_pfn c = UN(ClearBrushAttributes);
    REQUIRE(s); REQUIRE(c);
    CHECK(s((HBRUSH)(ULONG_PTR)0x3333, 0) == NULL);
    CHECK(s((HBRUSH)(ULONG_PTR)0x3333, 1) == NULL);
    CHECK(c((HBRUSH)(ULONG_PTR)0x3333, 0) == NULL);
    CHECK(c((HBRUSH)(ULONG_PTR)0x3333, 1) == NULL);
}
