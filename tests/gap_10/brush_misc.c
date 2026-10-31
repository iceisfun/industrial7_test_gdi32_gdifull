/* CreateDIBPatternBrush, FixBrushOrgEx, GetRelAbs, GetCodePage, GetEUDCTimeStamp*, Get/SetSystemPaletteUse. */
#include "gap10.h"

/* ---------------- CreateDIBPatternBrush ---------------- */
#define NEED_DPB() do { REQUIRE(U(CreateDIBPatternBrush)); REQUIRE(U(GetObjectW)); REQUIRE(U(DeleteObject)); } while (0)

/* Packed 24bpp DIB w x h, colour depends on the column only (so bottom-up/top-down is irrelevant). */
static COLORREF colcol(int x) { return RGB(20 + x * 29, 250 - x * 31, (x * 53) & 0xFF); }
static HGLOBAL make_dib24(int w, int h, UINT flags) {
    int pitch = (w * 3 + 3) & ~3, x, y;
    HGLOBAL g = GlobalAlloc(flags, sizeof(BITMAPINFOHEADER) + pitch * h);
    BYTE *p; BITMAPINFOHEADER *bh;
    if (!g) return NULL;
    p = (BYTE *)GlobalLock(g);
    if (!p) return g;   /* fixed memory: handle is the pointer */
    memset(p, 0, sizeof(BITMAPINFOHEADER) + pitch * h);
    bh = (BITMAPINFOHEADER *)p;
    bh->biSize = sizeof *bh; bh->biWidth = w; bh->biHeight = h; bh->biPlanes = 1; bh->biBitCount = 24;
    bh->biCompression = BI_RGB; bh->biSizeImage = pitch * h;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        COLORREF c = colcol(x); BYTE *q = p + sizeof *bh + y * pitch + x * 3;
        q[0] = GetBValue(c); q[1] = GetGValue(c); q[2] = GetRValue(c);
    }
    GlobalUnlock(g);
    return g;
}
static HGLOBAL make_dib1(int w, int h, UINT flags) {   /* 1bpp, black/white, left half white */
    int pitch = ((w + 31) / 32) * 4, x, y;
    int sz = sizeof(BITMAPINFOHEADER) + 8 + pitch * h;
    HGLOBAL g = GlobalAlloc(flags, sz); BYTE *p; BITMAPINFOHEADER *bh;
    if (!g) return NULL;
    p = (BYTE *)GlobalLock(g);
    memset(p, 0, sz);
    bh = (BITMAPINFOHEADER *)p;
    bh->biSize = sizeof *bh; bh->biWidth = w; bh->biHeight = h; bh->biPlanes = 1; bh->biBitCount = 1;
    bh->biCompression = BI_RGB; bh->biClrUsed = 2;
    p[sizeof *bh + 4] = p[sizeof *bh + 5] = p[sizeof *bh + 6] = 0xFF;   /* entry1 = white, entry0 = black */
    for (y = 0; y < h; y++) for (x = 0; x < w / 2; x++) p[sizeof *bh + 8 + y * pitch + x / 8] |= (BYTE)(0x80 >> (x % 8));
    GlobalUnlock(g);
    return g;
}
TEST(gap10_cdpb_null_handle_fails) {
    NEED_DPB();
    HBRUSH b;
    CLR();
    b = U(CreateDIBPatternBrush)(NULL, DIB_RGB_COLORS);
    CHECK(b == NULL);
    t_note("NULL hglobal: err %lu", GetLastError());
    if (b) U(DeleteObject)(b);
    b = U(CreateDIBPatternBrush)(NULL, DIB_PAL_COLORS);
    CHECK(b == NULL);
    if (b) U(DeleteObject)(b);
}
TEST(gap10_cdpb_invalid_handle_fails) {
    NEED_DPB();
    HGLOBAL bad[] = { (HGLOBAL)(ULONG_PTR)0x1, (HGLOBAL)(ULONG_PTR)0xdeadbeef, (HGLOBAL)(ULONG_PTR)0xFFFFFFFF }; unsigned i;
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        HBRUSH b;
        CLR();
        b = U(CreateDIBPatternBrush)(bad[i], DIB_RGB_COLORS);
        CHECKF(b == NULL, "handle %p", bad[i]);
        if (b) U(DeleteObject)(b);
    }
}
TEST(gap10_cdpb_zeroed_header_fails) {
    NEED_DPB();
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, 256); HBRUSH b;
    CHECK(g != NULL);
    if (!g) return;
    CLR();
    b = U(CreateDIBPatternBrush)(g, DIB_RGB_COLORS);
    CHECK(b == NULL);
    t_note("zeroed header: err %lu", GetLastError());
    if (b) U(DeleteObject)(b);
    GlobalFree(g);
}
TEST(gap10_cdpb_basic_24bpp) {
    NEED_DPB();
    HGLOBAL g = make_dib24(8, 8, GMEM_MOVEABLE); HBRUSH b; LOGBRUSH lb;
    CHECK(g != NULL);
    if (!g) return;
    b = U(CreateDIBPatternBrush)(g, DIB_RGB_COLORS);
    CHECK(b != NULL);
    if (b) {
        memset(&lb, 0xCC, sizeof lb);
        EXPECT_EQ(U(GetObjectW)(b, sizeof lb, &lb), sizeof(LOGBRUSH));
        t_note("LOGBRUSH style %u color %08lx hatch %p", lb.lbStyle, lb.lbColor, (void *)lb.lbHatch);
        CHECK(lb.lbStyle == BS_DIBPATTERN || lb.lbStyle == BS_DIBPATTERNPT || lb.lbStyle == BS_PATTERN);
        U(DeleteObject)(b);
    }
    GlobalFree(g);
}
TEST(gap10_cdpb_fixed_memory_handle) {
    NEED_DPB();
    HGLOBAL g = make_dib24(8, 8, GMEM_FIXED); HBRUSH b;
    CHECK(g != NULL);
    if (!g) return;
    b = U(CreateDIBPatternBrush)(g, DIB_RGB_COLORS);
    CHECK(b != NULL);
    if (b) U(DeleteObject)(b);
    GlobalFree(g);
}
TEST(gap10_cdpb_object_type_and_select) {
    NEED_DPB(); NEED_MD(); REQUIRE(U(GetObjectType)); REQUIRE(U(GetCurrentObject));
    HGLOBAL g = make_dib24(8, 8, GMEM_MOVEABLE); HBRUSH b; HDC dc; HGDIOBJ old;
    if (!g) return;
    b = U(CreateDIBPatternBrush)(g, DIB_RGB_COLORS);
    CHECK(b != NULL);
    if (b) {
        EXPECT_EQ(U(GetObjectType)(b), OBJ_BRUSH);
        dc = U(CreateCompatibleDC)(NULL);
        old = U(SelectObject)(dc, b);
        CHECK(old != NULL);
        EXPECT_EQ(U(GetCurrentObject)(dc, OBJ_BRUSH), b);
        U(SelectObject)(dc, old);
        U(DeleteDC)(dc);
        CHECK(U(DeleteObject)(b));
        CHECK(!U(DeleteObject)(b));
    }
    GlobalFree(g);
}
TEST(gap10_cdpb_patblt_pixels_columns) {
    NEED_DPB(); NEED_MD();
    MD m; HGLOBAL g = make_dib24(8, 8, GMEM_MOVEABLE); HBRUSH b; HGDIOBJ ob; int x, y;
    if (!g) return;
    if (!md_create(&m, 24, 20, 32)) { GlobalFree(g); t_skip("no DIB"); return; }
    md_clear(&m, WHITE_);
    b = U(CreateDIBPatternBrush)(g, DIB_RGB_COLORS);
    CHECK(b != NULL);
    if (b) {
        ob = U(SelectObject)(m.dc, b);
        CHECK(U(PatBlt)(m.dc, 0, 0, 24, 20, PATCOPY));
        for (y = 0; y < 20; y += 3) for (x = 0; x < 24; x++)
            CHECKF(md_px(&m, x, y) == colcol(x % 8), "x=%d y=%d got %06lx want %06lx", x, y, md_px(&m, x, y), colcol(x % 8));
        U(SelectObject)(m.dc, ob);
        U(DeleteObject)(b);
    }
    GlobalFree(g);
    md_destroy(&m);
}
TEST(gap10_cdpb_brush_origin_shifts_pattern) {
    NEED_DPB(); NEED_MD(); REQUIRE(U(SetBrushOrgEx));
    MD m; HGLOBAL g = make_dib24(8, 8, GMEM_MOVEABLE); HBRUSH b; HGDIOBJ ob; int x;
    if (!g) return;
    if (!md_create(&m, 24, 8, 32)) { GlobalFree(g); t_skip("no DIB"); return; }
    b = U(CreateDIBPatternBrush)(g, DIB_RGB_COLORS);
    CHECK(b != NULL);
    if (b) {
        int org[] = { 0, 1, 3, 7, 8, 11, -2 }; unsigned k;
        ob = U(SelectObject)(m.dc, b);
        for (k = 0; k < sizeof org / sizeof *org; k++) {
            POINT old;
            U(SetBrushOrgEx)(m.dc, org[k], 0, &old);
            U(PatBlt)(m.dc, 0, 0, 24, 8, PATCOPY);
            for (x = 0; x < 24; x++) {
                int col = (((x - org[k]) % 8) + 8) % 8;
                CHECKF(md_px(&m, x, 4) == colcol(col), "org %d x=%d", org[k], x);
            }
        }
        U(SelectObject)(m.dc, ob);
        U(DeleteObject)(b);
    }
    GlobalFree(g);
    md_destroy(&m);
}
TEST(gap10_cdpb_source_freed_after_creation_observed) {
    NEED_DPB(); NEED_MD();
    MD m; HGLOBAL g = make_dib24(8, 8, GMEM_MOVEABLE); HBRUSH b; HGDIOBJ ob;
    if (!g) return;
    if (!md_create(&m, 8, 8, 32)) { GlobalFree(g); t_skip("no DIB"); return; }
    b = U(CreateDIBPatternBrush)(g, DIB_RGB_COLORS);
    CHECK(b != NULL);
    GlobalFree(g);
    if (b) {
        ob = U(SelectObject)(m.dc, b);
        U(PatBlt)(m.dc, 0, 0, 8, 8, PATCOPY);
        if (md_px(&m, 3, 3) != colcol(3)) t_note("brush depends on freed DIB memory");
        else CHECK(md_px(&m, 7, 0) == colcol(7));
        U(SelectObject)(m.dc, ob);
        U(DeleteObject)(b);
    }
    md_destroy(&m);
}
TEST(gap10_cdpb_mono_dib_two_colors) {
    NEED_DPB(); NEED_MD();
    MD m; HGLOBAL g = make_dib1(8, 8, GMEM_MOVEABLE); HBRUSH b; HGDIOBJ ob;
    if (!g) return;
    if (!md_create(&m, 8, 8, 32)) { GlobalFree(g); t_skip("no DIB"); return; }
    b = U(CreateDIBPatternBrush)(g, DIB_RGB_COLORS);
    CHECK(b != NULL);
    if (b) {
        ob = U(SelectObject)(m.dc, b);
        U(PatBlt)(m.dc, 0, 0, 8, 8, PATCOPY);
        EXPECT_EQ(md_px(&m, 0, 0), WHITE_); EXPECT_EQ(md_px(&m, 3, 5), WHITE_);
        EXPECT_EQ(md_px(&m, 4, 0), BLACK_); EXPECT_EQ(md_px(&m, 7, 7), BLACK_);
        U(SelectObject)(m.dc, ob);
        U(DeleteObject)(b);
    }
    GlobalFree(g);
    md_destroy(&m);
}
TEST(gap10_cdpb_small_pattern_observed) {
    NEED_DPB(); NEED_MD();
    int sizes[] = { 1, 2, 4, 8, 16, 32 }; unsigned k;
    for (k = 0; k < sizeof sizes / sizeof *sizes; k++) {
        MD m; HGLOBAL g = make_dib24(sizes[k], sizes[k], GMEM_MOVEABLE); HBRUSH b; HGDIOBJ ob;
        if (!g) continue;
        if (!md_create(&m, 32, 4, 32)) { GlobalFree(g); continue; }
        b = U(CreateDIBPatternBrush)(g, DIB_RGB_COLORS);
        if (!b) { t_note("pattern %d: NULL", sizes[k]); GlobalFree(g); md_destroy(&m); continue; }
        ob = U(SelectObject)(m.dc, b);
        U(PatBlt)(m.dc, 0, 0, 32, 4, PATCOPY);
        t_note("pattern %d: px0 %06lx px1 %06lx px%d %06lx", sizes[k], md_px(&m, 0, 0), md_px(&m, 1, 0), sizes[k], md_px(&m, sizes[k] < 32 ? sizes[k] : 31, 0));
        if (sizes[k] == 8 || sizes[k] == 16 || sizes[k] == 32)
            CHECKF(md_px(&m, 5, 1) == colcol(5), "size %d", sizes[k]);
        U(SelectObject)(m.dc, ob);
        U(DeleteObject)(b);
        GlobalFree(g);
        md_destroy(&m);
    }
}
TEST(gap10_cdpb_bad_usage_observed) {
    NEED_DPB();
    UINT us[] = { DIB_PAL_COLORS, 2, 3, 7, 0xFFFF, 0xFFFFFFFF }; unsigned i;
    HGLOBAL g = make_dib24(8, 8, GMEM_MOVEABLE);
    if (!g) return;
    for (i = 0; i < sizeof us / sizeof *us; i++) {
        HBRUSH b; CLR();
        b = U(CreateDIBPatternBrush)(g, us[i]);
        t_note("usage %u: %s err %lu", us[i], b ? "brush" : "NULL", GetLastError());
        if (b) U(DeleteObject)(b);
    }
    GlobalFree(g);
}
TEST(gap10_cdpb_compare_pt_variant) {
    NEED_DPB(); NEED_MD(); REQUIRE(U(CreateDIBPatternBrushPt));
    MD m1, m2; HGLOBAL g = make_dib24(8, 8, GMEM_MOVEABLE); HBRUSH b1, b2; int x, same = 1;
    if (!g) return;
    if (!md_create(&m1, 8, 2, 32) || !md_create(&m2, 8, 2, 32)) { GlobalFree(g); t_skip("no DIB"); return; }
    b1 = U(CreateDIBPatternBrush)(g, DIB_RGB_COLORS);
    b2 = U(CreateDIBPatternBrushPt)(GlobalLock(g), DIB_RGB_COLORS);
    GlobalUnlock(g);
    CHECK(b1 && b2);
    if (b1 && b2) {
        HGDIOBJ o1 = U(SelectObject)(m1.dc, b1), o2 = U(SelectObject)(m2.dc, b2);
        U(PatBlt)(m1.dc, 0, 0, 8, 2, PATCOPY); U(PatBlt)(m2.dc, 0, 0, 8, 2, PATCOPY);
        for (x = 0; x < 8; x++) if (md_px(&m1, x, 0) != md_px(&m2, x, 0)) same = 0;
        CHECK(same);
        U(SelectObject)(m1.dc, o1); U(SelectObject)(m2.dc, o2);
    }
    if (b1) U(DeleteObject)(b1);
    if (b2) U(DeleteObject)(b2);
    GlobalFree(g);
    md_destroy(&m1); md_destroy(&m2);
}
TEST(gap10_cdpb_no_gdi_leak) {
    NEED_DPB();
    if (!g10_gdicount()) { t_skip("no GetGuiResources"); return; }
    HGLOBAL g = make_dib24(8, 8, GMEM_MOVEABLE); DWORD before; HBRUSH h[100]; int i;
    if (!g) return;
    before = g10_gdicount();
    for (i = 0; i < 100; i++) h[i] = U(CreateDIBPatternBrush)(g, DIB_RGB_COLORS);
    for (i = 0; i < 100; i++) if (h[i]) U(DeleteObject)(h[i]);
    EXPECT_EQ(g10_gdicount(), before);
    GlobalFree(g);
}
TEST(gap10_cdpb_hglobal_unlocked_after_call) {
    NEED_DPB();
    HGLOBAL g = make_dib24(8, 8, GMEM_MOVEABLE); HBRUSH b;
    if (!g) return;
    b = U(CreateDIBPatternBrush)(g, DIB_RGB_COLORS);
    CHECK(b != NULL);
    /* the function locks and unlocks: lock count must be back to zero */
    EXPECT_EQ(GlobalFlags(g) & GMEM_LOCKCOUNT, 0);
    if (b) U(DeleteObject)(b);
    GlobalFree(g);
}

/* ---------------- FixBrushOrgEx ---------------- */
/* In Windows 10 this export is an empty stub that returns FALSE. */
TEST(gap10_fbo_always_false) {
    REQUIRE(UG(FixBrushOrgEx)); NEED_MD();
    HDC dc = U(CreateCompatibleDC)(NULL); POINT pt = { 111, 222 };
    CLR();
    EXPECT_EQ(UG(FixBrushOrgEx)(dc, 5, 6, &pt), 0);
    EXPECT_EQ(pt.x, 111); EXPECT_EQ(pt.y, 222);
    EXPECT_EQ(GetLastError(), ERRS);
    EXPECT_EQ(UG(FixBrushOrgEx)(dc, 0, 0, NULL), 0);
    EXPECT_EQ(UG(FixBrushOrgEx)(dc, -1, INT_MAX, NULL), 0);
    U(DeleteDC)(dc);
}
TEST(gap10_fbo_invalid_dc_same_result) {
    REQUIRE(UG(FixBrushOrgEx));
    POINT pt = { 7, 8 };
    CLR();
    EXPECT_EQ(UG(FixBrushOrgEx)(NULL, 1, 2, &pt), 0);
    EXPECT_EQ(UG(FixBrushOrgEx)((HDC)(ULONG_PTR)0xdeadbeef, 1, 2, &pt), 0);
    EXPECT_EQ(UG(FixBrushOrgEx)((HDC)(ULONG_PTR)1, 1, 2, NULL), 0);
    EXPECT_EQ(pt.x, 7); EXPECT_EQ(pt.y, 8);
    EXPECT_EQ(GetLastError(), ERRS);
}
TEST(gap10_fbo_does_not_change_brush_org) {
    REQUIRE(UG(FixBrushOrgEx)); REQUIRE(U(SetBrushOrgEx)); REQUIRE(U(GetBrushOrgEx)); NEED_MD();
    HDC dc = U(CreateCompatibleDC)(NULL); POINT p, o;
    U(SetBrushOrgEx)(dc, 13, 17, &o);
    UG(FixBrushOrgEx)(dc, 99, 98, &p);
    memset(&p, 0, sizeof p);
    CHECK(U(GetBrushOrgEx)(dc, &p));
    EXPECT_EQ(p.x, 13); EXPECT_EQ(p.y, 17);
    U(DeleteDC)(dc);
}
TEST(gap10_fbo_resolves_in_both_modules) {
    CHECK(UG(FixBrushOrgEx) != NULL);
    t_note("gdi32full FixBrushOrgEx %s", UF(FixBrushOrgEx) ? "present" : "absent");
    if (UF(FixBrushOrgEx)) {
        POINT pt = { 1, 2 };
        EXPECT_EQ(UF(FixBrushOrgEx)(NULL, 0, 0, &pt), 0);
    }
}

/* ---------------- GetRelAbs / SetRelAbs ---------------- */
typedef int (WINAPI *GetRelAbs_pfn)(HDC, DWORD);
typedef int (WINAPI *SetRelAbs_pfn)(HDC, int);
static GetRelAbs_pfn pGetRelAbs(void) { return (GetRelAbs_pfn)t_proc("GetRelAbs"); }
static SetRelAbs_pfn pSetRelAbs(void) { return (SetRelAbs_pfn)t_proc("SetRelAbs"); }

TEST(gap10_relabs_invalid_dc_returns_zero) {
    GetRelAbs_pfn f = pGetRelAbs(); REQUIRE(f);
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef, (HDC)(ULONG_PTR)0xFFFFFFFF }; unsigned i;
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        CLR();
        CHECKF(f(bad[i], 0) == 0, "dc %p", bad[i]);
    }
}
TEST(gap10_relabs_wrong_object_type) {
    GetRelAbs_pfn f = pGetRelAbs(); REQUIRE(f); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    HBRUSH b = U(CreateSolidBrush)(0);
    EXPECT_EQ(f((HDC)b, 0), 0);
    U(DeleteObject)(b);
}
TEST(gap10_relabs_fresh_dc_observed) {
    GetRelAbs_pfn f = pGetRelAbs(); REQUIRE(f); NEED_MD();
    HDC dc = U(CreateCompatibleDC)(NULL);
    int v = f(dc, 0);
    t_note("default RelAbs of a new DC: %d", v);
    CHECK(v >= 0 && v <= 2);
    EXPECT_EQ(f(dc, 0), v);
    EXPECT_EQ(f(dc, 0xFFFFFFFF), v);   /* second argument is ignored */
    U(DeleteDC)(dc);
}
TEST(gap10_relabs_set_get_roundtrip) {
    GetRelAbs_pfn g = pGetRelAbs(); SetRelAbs_pfn s = pSetRelAbs(); REQUIRE(g); REQUIRE(s); NEED_MD();
    HDC dc = U(CreateCompatibleDC)(NULL);
    int modes[] = { ABSOLUTE, RELATIVE, ABSOLUTE }; unsigned i;
    for (i = 0; i < 3; i++) {
        s(dc, modes[i]);
        int v = g(dc, 0);
        if (v != modes[i]) t_note("SetRelAbs(%d) -> GetRelAbs %d", modes[i], v);
        else CHECK(v == modes[i]);
    }
    U(DeleteDC)(dc);
}
TEST(gap10_relabs_per_dc_state) {
    GetRelAbs_pfn g = pGetRelAbs(); SetRelAbs_pfn s = pSetRelAbs(); REQUIRE(g); REQUIRE(s); NEED_MD();
    HDC a = U(CreateCompatibleDC)(NULL), b = U(CreateCompatibleDC)(NULL);
    int base = g(b, 0);
    s(a, RELATIVE);
    int va = g(a, 0), vb = g(b, 0);
    t_note("after SetRelAbs(a,RELATIVE): a=%d b=%d (base %d)", va, vb, base);
    CHECK(vb == base);
    U(DeleteDC)(a); U(DeleteDC)(b);
}
TEST(gap10_relabs_set_invalid_dc) {
    SetRelAbs_pfn s = pSetRelAbs(); REQUIRE(s);
    CLR();
    s(NULL, ABSOLUTE);
    s((HDC)(ULONG_PTR)0xdeadbeef, RELATIVE);
    CHECK(1);   /* must simply not fault */
}
TEST(gap10_relabs_same_both_modules) {
    GetRelAbs_pfn a = (GetRelAbs_pfn)t_proc_gdi32("GetRelAbs"), b = (GetRelAbs_pfn)t_proc_gdi32full("GetRelAbs");
    REQUIRE(a); NEED_MD();
    HDC dc = U(CreateCompatibleDC)(NULL);
    if (b) EXPECT_EQ(a(dc, 0), b(dc, 0));
    U(DeleteDC)(dc);
}

/* ---------------- GetCodePage (gdi32: GdiGetCodePage) ---------------- */
typedef UINT (WINAPI *GetCodePage_pfn)(HDC);
TEST(gap10_codepage_invalid_dc_returns_acp_and_87) {
    GetCodePage_pfn f = (GetCodePage_pfn)t_proc_gdi32full("GetCodePage"); REQUIRE(f);
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)0xdeadbeef, (HDC)(ULONG_PTR)0x7FFFFFFF }; unsigned i;
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        UINT r; CLR();
        r = f(bad[i]);
        CHECKF(r == GetACP(), "dc %p ret %u acp %u", bad[i], r, GetACP());
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "dc %p err %lu", bad[i], GetLastError());
    }
}
TEST(gap10_codepage_wrong_object_type) {
    GetCodePage_pfn f = (GetCodePage_pfn)t_proc_gdi32full("GetCodePage"); REQUIRE(f); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    HBRUSH b = U(CreateSolidBrush)(0); UINT r;
    CLR();
    r = f((HDC)b);
    EXPECT_EQ(r, GetACP());
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    U(DeleteObject)(b);
}
TEST(gap10_codepage_deleted_dc) {
    GetCodePage_pfn f = (GetCodePage_pfn)t_proc_gdi32full("GetCodePage"); REQUIRE(f); NEED_MD();
    HDC dc = U(CreateCompatibleDC)(NULL); UINT r;
    U(DeleteDC)(dc);
    CLR();
    r = f(dc);
    EXPECT_EQ(r, GetACP());
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
}
TEST(gap10_codepage_valid_dc_observed) {
    GetCodePage_pfn f = (GetCodePage_pfn)t_proc_gdi32full("GetCodePage"); REQUIRE(f); NEED_MD();
    HDC dc = U(CreateCompatibleDC)(NULL); UINT r;
    CLR();
    r = f(dc);
    t_note("GetCodePage(new DC) = %u (ACP %u)", r, GetACP());
    CHECK(r <= 0xFFFF);
    EXPECT_EQ(GetLastError(), ERRS);
    EXPECT_EQ(f(dc), r);
    U(DeleteDC)(dc);
}
TEST(gap10_codepage_gdi32_forwarder_matches) {
    GetCodePage_pfn a = (GetCodePage_pfn)t_proc_gdi32("GdiGetCodePage"), b = (GetCodePage_pfn)t_proc_gdi32full("GetCodePage");
    REQUIRE(a); REQUIRE(b); NEED_MD();
    HDC dc = U(CreateCompatibleDC)(NULL);
    EXPECT_EQ(a(dc), b(dc));
    CLR();
    EXPECT_EQ(a(NULL), b(NULL));
    EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    U(DeleteDC)(dc);
}
TEST(gap10_codepage_follows_selected_font_charset_observed) {
    GetCodePage_pfn f = (GetCodePage_pfn)t_proc_gdi32full("GetCodePage"); REQUIRE(f); NEED_MD();
    REQUIRE(U(CreateFontW)); REQUIRE(U(DeleteObject));
    HDC dc = U(CreateCompatibleDC)(NULL); struct { BYTE cs; const WCHAR *face; } t[] = {
        { ANSI_CHARSET, L"Arial" }, { SYMBOL_CHARSET, L"Symbol" }, { GREEK_CHARSET, L"Arial" }, { RUSSIAN_CHARSET, L"Arial" }, { EASTEUROPE_CHARSET, L"Arial" }
    }; unsigned i;
    for (i = 0; i < sizeof t / sizeof *t; i++) {
        HFONT hf = U(CreateFontW)(16, 0, 0, 0, 400, 0, 0, 0, t[i].cs, 0, 0, 0, 0, t[i].face);
        HGDIOBJ old = U(SelectObject)(dc, hf);
        t_note("charset %d face %ls -> GetCodePage %u", t[i].cs, t[i].face, f(dc));
        U(SelectObject)(dc, old);
        U(DeleteObject)(hf);
    }
    U(DeleteDC)(dc);
}

/* ---------------- GetEUDCTimeStamp / GetEUDCTimeStampExW ---------------- */
typedef DWORD (WINAPI *GetEUDCTimeStamp_pfn)(void);
typedef DWORD (WINAPI *GetEUDCTimeStampExW_pfn)(LPCWSTR);
TEST(gap10_eudc_timestamp_stable) {
    GetEUDCTimeStamp_pfn f = (GetEUDCTimeStamp_pfn)t_proc("GetEUDCTimeStamp"); REQUIRE(f);
    DWORD a = f(), b = f(), c = f();
    t_note("GetEUDCTimeStamp = 0x%lx", a);
    EXPECT_EQ(a, b); EXPECT_EQ(b, c);
}
TEST(gap10_eudc_timestamp_modules_agree) {
    GetEUDCTimeStamp_pfn a = (GetEUDCTimeStamp_pfn)t_proc_gdi32("GetEUDCTimeStamp"), b = (GetEUDCTimeStamp_pfn)t_proc_gdi32full("GetEUDCTimeStamp");
    REQUIRE(a);
    if (b) EXPECT_EQ(a(), b());
}
TEST(gap10_eudc_timestamp_does_not_touch_lasterror) {
    GetEUDCTimeStamp_pfn f = (GetEUDCTimeStamp_pfn)t_proc("GetEUDCTimeStamp"); REQUIRE(f);
    CLR();
    f();
    t_note("last error after GetEUDCTimeStamp: %lu", GetLastError());
    CHECK(1);
}
TEST(gap10_eudc_ex_null_name_matches_basic_observed) {
    GetEUDCTimeStamp_pfn f = (GetEUDCTimeStamp_pfn)t_proc("GetEUDCTimeStamp");
    GetEUDCTimeStampExW_pfn g = (GetEUDCTimeStampExW_pfn)t_proc("GetEUDCTimeStampExW"); REQUIRE(f); REQUIRE(g);
    DWORD a = g(NULL), b = f();
    t_note("Ex(NULL)=0x%lx basic=0x%lx", a, b);
    EXPECT_EQ(g(NULL), a);
}
TEST(gap10_eudc_ex_face_names_stable) {
    GetEUDCTimeStampExW_pfn g = (GetEUDCTimeStampExW_pfn)t_proc("GetEUDCTimeStampExW"); REQUIRE(g);
    const WCHAR *names[] = { L"", L"Arial", L"MS Gothic", L"NoSuchFace_xyz", L"Times New Roman", L"Segoe UI", L"Courier New" };
    unsigned i;
    for (i = 0; i < sizeof names / sizeof *names; i++) {
        DWORD a = g(names[i]), b = g(names[i]);
        CHECKF(a == b, "face %ls: 0x%lx vs 0x%lx", names[i], a, b);
        t_note("EUDC stamp [%ls] = 0x%lx", names[i], a);
    }
}
TEST(gap10_eudc_ex_long_name_truncation_safe) {
    GetEUDCTimeStampExW_pfn g = (GetEUDCTimeStampExW_pfn)t_proc("GetEUDCTimeStampExW"); REQUIRE(g);
    WCHAR big[260]; int i;
    for (i = 0; i < 259; i++) big[i] = L'A';
    big[259] = 0;
    DWORD a = g(big), b = g(big);
    EXPECT_EQ(a, b);
}
TEST(gap10_eudc_ex_case_and_unknown_same) {
    GetEUDCTimeStampExW_pfn g = (GetEUDCTimeStampExW_pfn)t_proc("GetEUDCTimeStampExW"); REQUIRE(g);
    DWORD u1 = g(L"NoSuchFace_xyz"), u2 = g(L"AnotherMissingFace_123");
    t_note("unknown faces: 0x%lx 0x%lx", u1, u2);
    EXPECT_EQ(u1, u2);
}
TEST(gap10_eudc_ex_gdi32_forwarder) {
    GetEUDCTimeStampExW_pfn a = (GetEUDCTimeStampExW_pfn)t_proc_gdi32("GetEUDCTimeStampExW"), b = (GetEUDCTimeStampExW_pfn)t_proc_gdi32full("GetEUDCTimeStampExW");
    REQUIRE(a);
    if (b) EXPECT_EQ(a(L"Arial"), b(L"Arial"));
}

/* ---------------- Get/SetSystemPaletteUse (error paths only; never changes state) ---------------- */
TEST(gap10_syspal_get_invalid_dc) {
    REQUIRE(U(GetSystemPaletteUse));
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef }; unsigned i;
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        CLR();
        CHECKF(U(GetSystemPaletteUse)(bad[i]) == SYSPAL_ERROR, "dc %p", bad[i]);
    }
}
TEST(gap10_syspal_set_invalid_dc) {
    REQUIRE(U(SetSystemPaletteUse));
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef }; unsigned i;
    UINT uses[] = { SYSPAL_STATIC, SYSPAL_NOSTATIC, SYSPAL_NOSTATIC256 }; unsigned k;
    for (i = 0; i < sizeof bad / sizeof *bad; i++) for (k = 0; k < 3; k++) {
        CLR();
        CHECKF(U(SetSystemPaletteUse)(bad[i], uses[k]) == SYSPAL_ERROR, "dc %p use %u", bad[i], uses[k]);
    }
}
TEST(gap10_syspal_set_invalid_dc_invalid_use) {
    REQUIRE(U(SetSystemPaletteUse));
    UINT uses[] = { 0, 4, 100, 0xFFFF, 0xFFFFFFFF }; unsigned k;
    for (k = 0; k < 5; k++) {
        CLR();
        CHECKF(U(SetSystemPaletteUse)(NULL, uses[k]) == SYSPAL_ERROR, "use %u", uses[k]);
    }
}
TEST(gap10_syspal_wrong_object_type) {
    REQUIRE(U(SetSystemPaletteUse)); REQUIRE(U(GetSystemPaletteUse)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DeleteObject));
    HBRUSH b = U(CreateSolidBrush)(0);
    EXPECT_EQ(U(GetSystemPaletteUse)((HDC)b), SYSPAL_ERROR);
    EXPECT_EQ(U(SetSystemPaletteUse)((HDC)b, SYSPAL_STATIC), SYSPAL_ERROR);
    U(DeleteObject)(b);
}
TEST(gap10_syspal_get_memory_dc_observed) {
    REQUIRE(U(GetSystemPaletteUse)); NEED_MD();
    HDC dc = U(CreateCompatibleDC)(NULL); UINT r = U(GetSystemPaletteUse)(dc);
    t_note("GetSystemPaletteUse(memory DC) = %u", r);
    CHECK(r <= SYSPAL_NOSTATIC256);
    EXPECT_EQ(U(GetSystemPaletteUse)(dc), r);
    U(DeleteDC)(dc);
}
TEST(gap10_syspal_get_is_read_only) {
    REQUIRE(U(GetSystemPaletteUse)); NEED_MD();
    HDC dc = U(CreateCompatibleDC)(NULL); UINT a = U(GetSystemPaletteUse)(dc), b, i;
    for (i = 0; i < 20; i++) b = U(GetSystemPaletteUse)(dc);
    EXPECT_EQ(a, b);
    U(DeleteDC)(dc);
}
TEST(gap10_syspal_deleted_dc) {
    REQUIRE(U(GetSystemPaletteUse)); NEED_MD();
    HDC dc = U(CreateCompatibleDC)(NULL);
    U(DeleteDC)(dc);
    CLR();
    EXPECT_EQ(U(GetSystemPaletteUse)(dc), SYSPAL_ERROR);
}
