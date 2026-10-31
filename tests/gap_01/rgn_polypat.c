#include "gap.h"

/* ================= MirrorRgn ================= */
/* MirrorRgn(hwnd, hrgn) mirrors a region about the vertical axis of the window: a pixel column x
 * becomes (W - 1 - x) where W is the window rectangle width. The region handle is modified in place. */

static HWND mkwin(int w, int h) {
    return t_make_window(NULL, WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, w, h, NULL);
}
static int winwidth(HWND w) { RECT r; U(GetWindowRect)(w, &r); return r.right - r.left; }
static void killwin(HWND w) { U(DestroyWindow)(w); t_pump(); }

TEST(gap1_mirrorrgn_single_rect_box) {
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(GetRgnBox)); REQUIRE(U(GetWindowRect)); REQUIRE(U(DestroyWindow)); REQUIRE(U(DeleteObject));
    MirrorRgn_pfn f = UN(MirrorRgn);
    REQUIRE(f);
    HWND w = mkwin(200, 100); if (!w) { t_skip("cannot create window"); return; }
    int W = winwidth(w);
    HRGN r = U(CreateRectRgn)(10, 5, 30, 20);
    f(w, r);
    RECT b;
    CHECK(U(GetRgnBox)(r, &b) == SIMPLEREGION);
    CHECKF(b.left == W - 30 && b.right == W - 10 && b.top == 5 && b.bottom == 20, "box %ld,%ld,%ld,%ld W=%d", b.left, b.top, b.right, b.bottom, W);
    U(DeleteObject)(r); killwin(w);
}

TEST(gap1_mirrorrgn_table_of_rects) {
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(GetRgnBox)); REQUIRE(U(GetWindowRect)); REQUIRE(U(DestroyWindow)); REQUIRE(U(DeleteObject));
    MirrorRgn_pfn f = UN(MirrorRgn);
    REQUIRE(f);
    static const RECT rs[] = { {0,0,1,1}, {0,0,200,100}, {199,0,200,1}, {50,50,150,60}, {-10,-10,10,10}, {190,90,260,200}, {100,0,101,100}, {3,7,4,8}, {-50,0,-20,5} };
    static const int widths[] = { 200, 64, 321 };
    for (unsigned wi = 0; wi < 3; wi++) {
        HWND w = mkwin(widths[wi], 80); if (!w) { t_skip("cannot create window"); return; }
        int W = winwidth(w);
        for (unsigned i = 0; i < sizeof rs / sizeof *rs; i++) {
            HRGN r = U(CreateRectRgn)(rs[i].left, rs[i].top, rs[i].right, rs[i].bottom);
            f(w, r);
            RECT b; U(GetRgnBox)(r, &b);
            CHECKF(b.left == W - rs[i].right && b.right == W - rs[i].left && b.top == rs[i].top && b.bottom == rs[i].bottom,
                   "W=%d rect %u got %ld,%ld,%ld,%ld", W, i, b.left, b.top, b.right, b.bottom);
            U(DeleteObject)(r);
        }
        killwin(w);
    }
}

TEST(gap1_mirrorrgn_twice_is_identity) {
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(CombineRgn)); REQUIRE(U(EqualRgn)); REQUIRE(U(CreateEllipticRgn)); REQUIRE(U(DestroyWindow)); REQUIRE(U(DeleteObject));
    MirrorRgn_pfn f = UN(MirrorRgn);
    REQUIRE(f);
    HWND w = mkwin(180, 90); if (!w) { t_skip("cannot create window"); return; }
    HRGN a = U(CreateRectRgn)(5, 5, 40, 30), b = U(CreateEllipticRgn)(30, 10, 90, 70);
    HRGN c = U(CreateRectRgn)(0, 0, 1, 1), orig = U(CreateRectRgn)(0, 0, 1, 1);
    U(CombineRgn)(c, a, b, RGN_OR);
    U(CombineRgn)(orig, c, NULL, RGN_COPY);
    f(w, c);
    CHECK(!U(EqualRgn)(c, orig));
    f(w, c);
    CHECK(U(EqualRgn)(c, orig));
    U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(c); U(DeleteObject)(orig); killwin(w);
}

TEST(gap1_mirrorrgn_pixel_mapping) {
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(CombineRgn)); REQUIRE(U(PtInRegion)); REQUIRE(U(CreateEllipticRgn)); REQUIRE(U(DestroyWindow)); REQUIRE(U(DeleteObject));
    MirrorRgn_pfn f = UN(MirrorRgn);
    REQUIRE(f);
    HWND w = mkwin(120, 70); if (!w) { t_skip("cannot create window"); return; }
    int W = winwidth(w);
    HRGN o = U(CreateEllipticRgn)(8, 6, 70, 44), t = U(CreateRectRgn)(60, 30, 100, 60);
    U(CombineRgn)(o, o, t, RGN_OR);
    HRGN m = U(CreateRectRgn)(0, 0, 1, 1);
    U(CombineRgn)(m, o, NULL, RGN_COPY);
    f(w, m);
    int bad = 0;
    for (int y = 0; y < 70; y++) for (int x = 0; x < W; x++)
        if (!!U(PtInRegion)(o, x, y) != !!U(PtInRegion)(m, W - 1 - x, y)) bad++;
    CHECKF(bad == 0, "%d mismatching pixels", bad);
    U(DeleteObject)(o); U(DeleteObject)(t); U(DeleteObject)(m); killwin(w);
}

TEST(gap1_mirrorrgn_keeps_handle_and_type) {
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(GetObjectType)); REQUIRE(U(DestroyWindow)); REQUIRE(U(DeleteObject));
    MirrorRgn_pfn f = UN(MirrorRgn);
    REQUIRE(f);
    HWND w = mkwin(100, 50); if (!w) { t_skip("cannot create window"); return; }
    HRGN r = U(CreateRectRgn)(1, 1, 9, 9);
    f(w, r);
    CHECK(U(GetObjectType)(r) == OBJ_REGION);
    CHECK(U(DeleteObject)(r));
    killwin(w);
}

TEST(gap1_mirrorrgn_symmetric_region_unchanged) {
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(EqualRgn)); REQUIRE(U(GetWindowRect)); REQUIRE(U(DestroyWindow)); REQUIRE(U(DeleteObject));
    MirrorRgn_pfn f = UN(MirrorRgn);
    REQUIRE(f);
    HWND w = mkwin(100, 50); if (!w) { t_skip("cannot create window"); return; }
    int W = winwidth(w);
    HRGN r = U(CreateRectRgn)(20, 3, W - 20, 30), o = U(CreateRectRgn)(20, 3, W - 20, 30);
    f(w, r);
    CHECK(U(EqualRgn)(r, o));
    U(DeleteObject)(r); U(DeleteObject)(o); killwin(w);
}

TEST(gap1_mirrorrgn_multi_rect_region_rows) {
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(CombineRgn)); REQUIRE(U(GetRegionData)); REQUIRE(U(DestroyWindow)); REQUIRE(U(DeleteObject));
    MirrorRgn_pfn f = UN(MirrorRgn);
    REQUIRE(f);
    HWND w = mkwin(100, 60); if (!w) { t_skip("cannot create window"); return; }
    int W = winwidth(w);
    HRGN a = U(CreateRectRgn)(0, 0, 10, 5), b = U(CreateRectRgn)(30, 0, 40, 5), c = U(CreateRectRgn)(0, 10, 15, 20), r = U(CreateRectRgn)(0, 0, 1, 1);
    U(CombineRgn)(r, a, b, RGN_OR); U(CombineRgn)(r, r, c, RGN_OR);
    f(w, r);
    BYTE buf[sizeof(RGNDATAHEADER) + 16 * 8];
    DWORD n = U(GetRegionData)(r, sizeof buf, (RGNDATA *)buf);
    CHECK(n >= sizeof(RGNDATAHEADER) + 3 * 16);
    RGNDATA *d = (RGNDATA *)buf;
    CHECK(d->rdh.nCount == 3);
    if (d->rdh.nCount == 3) {
        const RECT *rc = (const RECT *)d->Buffer;
        /* rects within a band are ordered left to right after mirroring */
        CHECKF(rc[0].left == W - 40 && rc[0].right == W - 30 && rc[0].top == 0 && rc[0].bottom == 5, "r0 %ld,%ld,%ld,%ld", rc[0].left, rc[0].top, rc[0].right, rc[0].bottom);
        CHECKF(rc[1].left == W - 10 && rc[1].right == W && rc[1].top == 0 && rc[1].bottom == 5, "r1 %ld,%ld,%ld,%ld", rc[1].left, rc[1].top, rc[1].right, rc[1].bottom);
        CHECKF(rc[2].left == W - 15 && rc[2].right == W && rc[2].top == 10 && rc[2].bottom == 20, "r2 %ld,%ld,%ld,%ld", rc[2].left, rc[2].top, rc[2].right, rc[2].bottom);
    }
    U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteObject)(c); U(DeleteObject)(r); killwin(w);
}

TEST(gap1_mirrorrgn_null_region_no_crash) {
    REQUIRE(U(DestroyWindow));
    MirrorRgn_pfn f = UN(MirrorRgn);
    REQUIRE(f);
    HWND w = mkwin(100, 50); if (!w) { t_skip("cannot create window"); return; }
    SetLastError(0);
    BOOL r = f(w, NULL);
    t_note("MirrorRgn(hwnd, NULL) -> %d err %lu", r, GetLastError());
    killwin(w);
}

TEST(gap1_mirrorrgn_wrong_object_type_no_crash) {
    REQUIRE(U(CreateSolidBrush)); REQUIRE(U(DestroyWindow)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectType));
    MirrorRgn_pfn f = UN(MirrorRgn);
    REQUIRE(f);
    HWND w = mkwin(100, 50); if (!w) { t_skip("cannot create window"); return; }
    HBRUSH b = U(CreateSolidBrush)(0x123456);
    f(w, (HRGN)b);
    CHECK(U(GetObjectType)(b) == OBJ_BRUSH);
    f(w, (HRGN)(ULONG_PTR)0x1234);
    U(DeleteObject)(b); killwin(w);
}

TEST(gap1_mirrorrgn_stale_region_no_crash) {
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(DestroyWindow)); REQUIRE(U(DeleteObject));
    MirrorRgn_pfn f = UN(MirrorRgn);
    REQUIRE(f);
    HWND w = mkwin(100, 50); if (!w) { t_skip("cannot create window"); return; }
    HRGN r = U(CreateRectRgn)(0, 0, 5, 5);
    U(DeleteObject)(r);
    f(w, r);
    killwin(w);
    CHECK(1);
}

TEST(gap1_mirrorrgn_empty_region) {
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(GetRgnBox)); REQUIRE(U(DestroyWindow)); REQUIRE(U(DeleteObject));
    MirrorRgn_pfn f = UN(MirrorRgn);
    REQUIRE(f);
    HWND w = mkwin(100, 50); if (!w) { t_skip("cannot create window"); return; }
    HRGN r = U(CreateRectRgn)(0, 0, 0, 0);
    f(w, r);
    RECT b;
    int t = U(GetRgnBox)(r, &b);
    t_note("empty region after MirrorRgn: type %d box %ld,%ld,%ld,%ld", t, b.left, b.top, b.right, b.bottom);
    CHECK(t == NULLREGION);
    U(DeleteObject)(r); killwin(w);
}

TEST(gap1_mirrorrgn_gdi32_vs_full_same_result) {
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(EqualRgn)); REQUIRE(U(DestroyWindow)); REQUIRE(U(DeleteObject));
    MirrorRgn_pfn a = (MirrorRgn_pfn)t_proc_gdi32("MirrorRgn"), b = (MirrorRgn_pfn)t_proc_gdi32full("MirrorRgn");
    REQUIRE(a); REQUIRE(b);
    HWND w = mkwin(150, 50); if (!w) { t_skip("cannot create window"); return; }
    HRGN r1 = U(CreateRectRgn)(7, 1, 44, 33), r2 = U(CreateRectRgn)(7, 1, 44, 33);
    a(w, r1); b(w, r2);
    CHECK(U(EqualRgn)(r1, r2));
    U(DeleteObject)(r1); U(DeleteObject)(r2); killwin(w);
}

TEST(gap1_mirrorrgn_window_with_frame_uses_outer_width) {
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(GetRgnBox)); REQUIRE(U(GetWindowRect)); REQUIRE(U(DestroyWindow)); REQUIRE(U(DeleteObject));
    MirrorRgn_pfn f = UN(MirrorRgn);
    REQUIRE(f);
    HWND w = t_make_window(NULL, WS_OVERLAPPEDWINDOW, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 250, 120, NULL);
    if (!w) { t_skip("cannot create window"); return; }
    RECT wr; U(GetWindowRect)(w, &wr);
    int W = wr.right - wr.left;
    HRGN r = U(CreateRectRgn)(10, 10, 20, 20);
    f(w, r);
    RECT b; U(GetRgnBox)(r, &b);
    CHECKF(b.left == W - 20 && b.right == W - 10, "W=%d box %ld..%ld", W, b.left, b.right);
    U(DeleteObject)(r); killwin(w);
}

TEST(gap1_mirrorrgn_repeat_does_not_leak_gdi_objects) {
    REQUIRE(U(CreateRectRgn)); REQUIRE(U(DestroyWindow)); REQUIRE(U(DeleteObject));
    MirrorRgn_pfn f = UN(MirrorRgn);
    REQUIRE(f);
    HWND w = mkwin(100, 50); if (!w) { t_skip("cannot create window"); return; }
    HRGN r = U(CreateRectRgn)(1, 1, 30, 30);
    DWORD before = gap_gdi_count();
    for (int i = 0; i < 200; i++) f(w, r);
    DWORD after = gap_gdi_count();
    CHECKF(after <= before + 1, "gdi objects %lu -> %lu", before, after);
    U(DeleteObject)(r); killwin(w);
}

/* ================= PolyPatBlt ================= */
#ifndef EMR_PATBLT
#define EMR_PATBLT 43
#endif
typedef struct { int x, y, w, h; HBRUSH br; } PATRECT_;

#define PPB_PATCOPY  0x00F00021
#define PPB_PATINV   0x005A0049
#define PPB_DSTINV   0x00550009
#define PPB_BLACK    0x00000042
#define PPB_WHITE    0x00FF0062

static int ppb_ok(void) { return U(CreateSolidBrush) && U(DeleteObject) && U(CreateCompatibleDC) && U(CreateDIBSection) && U(SelectObject); }

TEST(gap1_polypatblt_single_rect_exact) {
    REQUIRE(ppb_ok());
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    GapDib d; if (!gap_dib_make(&d, 64, 64)) { CHECK(0); return; }
    gap_dib_fill(&d, 0);
    HBRUSH b = U(CreateSolidBrush)(RGB(200, 100, 50));
    PATRECT_ pr = { 10, 12, 20, 15, b };
    CHECK(f(d.dc, PPB_PATCOPY, &pr, 1, 0) == TRUE);
    U(GdiFlush)();
    int bad = 0;
    for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) {
        int in = x >= 10 && x < 30 && y >= 12 && y < 27;
        DWORD want = in ? 0x00C86432 : 0;
        if ((d.bits[y * 64 + x] & 0xffffff) != want) bad++;
    }
    CHECKF(bad == 0, "%d bad pixels", bad);
    U(DeleteObject)(b); gap_dib_free(&d);
}

TEST(gap1_polypatblt_multiple_brushes_exact) {
    REQUIRE(ppb_ok());
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    GapDib d; if (!gap_dib_make(&d, 64, 64)) { CHECK(0); return; }
    gap_dib_fill(&d, 0x00FFFFFF);
    COLORREF cs[4] = { RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255), RGB(17, 34, 51) };
    HBRUSH bs[4]; PATRECT_ pr[4];
    for (int i = 0; i < 4; i++) {
        bs[i] = U(CreateSolidBrush)(cs[i]);
        pr[i].x = (i & 1) * 32; pr[i].y = (i >> 1) * 32; pr[i].w = 32; pr[i].h = 32; pr[i].br = bs[i];
    }
    CHECK(f(d.dc, PPB_PATCOPY, pr, 4, 0) == TRUE);
    U(GdiFlush)();
    int bad = 0;
    for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) {
        int q = (y >> 5) * 2 + (x >> 5);
        if ((d.bits[y * 64 + x] & 0xffffff) != (DWORD)GAP_RGB32(cs[q])) bad++;
    }
    CHECKF(bad == 0, "%d bad pixels", bad);
    for (int i = 0; i < 4; i++) U(DeleteObject)(bs[i]);
    gap_dib_free(&d);
}

TEST(gap1_polypatblt_patinvert_over_background) {
    REQUIRE(ppb_ok());
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    GapDib d; if (!gap_dib_make(&d, 32, 32)) { CHECK(0); return; }
    gap_dib_fill(&d, 0x00336699);
    HBRUSH b = U(CreateSolidBrush)(RGB(0xF0, 0x0F, 0xAA));
    PATRECT_ pr = { 4, 4, 8, 8, b };
    CHECK(f(d.dc, PPB_PATINV, &pr, 1, 0) == TRUE);
    U(GdiFlush)();
    DWORD want = 0x00336699 ^ GAP_RGB32(RGB(0xF0, 0x0F, 0xAA));
    CHECKF((d.bits[5 * 32 + 5] & 0xffffff) == want, "got %06lx want %06lx", d.bits[5 * 32 + 5] & 0xffffff, want);
    CHECK((d.bits[0] & 0xffffff) == 0x336699);
    CHECK((d.bits[12 * 32 + 12] & 0xffffff) == 0x336699);   /* just outside */
    CHECK((d.bits[11 * 32 + 11] & 0xffffff) == want);       /* last inside pixel */
    U(DeleteObject)(b); gap_dib_free(&d);
}

TEST(gap1_polypatblt_blackness_whiteness_dstinvert) {
    REQUIRE(ppb_ok());
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    GapDib d; if (!gap_dib_make(&d, 32, 32)) { CHECK(0); return; }
    HBRUSH b = U(GetStockObject)(BLACK_BRUSH);
    gap_dib_fill(&d, 0x00123456);
    PATRECT_ pr = { 0, 0, 16, 16, b };
    CHECK(f(d.dc, PPB_BLACK, &pr, 1, 0) == TRUE); U(GdiFlush)();
    CHECK((d.bits[0] & 0xffffff) == 0 && (d.bits[15 * 32 + 15] & 0xffffff) == 0 && (d.bits[16] & 0xffffff) == 0x123456);
    CHECK(f(d.dc, PPB_WHITE, &pr, 1, 0) == TRUE); U(GdiFlush)();
    CHECK((d.bits[0] & 0xffffff) == 0xffffff && (d.bits[15 * 32 + 15] & 0xffffff) == 0xffffff);
    CHECK(f(d.dc, PPB_DSTINV, &pr, 1, 0) == TRUE); U(GdiFlush)();
    CHECK((d.bits[0] & 0xffffff) == 0 && (d.bits[16 * 32] & 0xffffff) == 0x123456);
    gap_dib_free(&d);
}

TEST(gap1_polypatblt_zero_count_false) {
    REQUIRE(ppb_ok());
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    GapDib d; if (!gap_dib_make(&d, 8, 8)) { CHECK(0); return; }
    HBRUSH b = U(GetStockObject)(BLACK_BRUSH);
    PATRECT_ pr = { 0, 0, 8, 8, b };
    gap_dib_fill(&d, 0x00ABCDEF);
    CHECK(f(d.dc, PPB_PATCOPY, &pr, 0, 0) == FALSE);
    U(GdiFlush)();
    CHECK((d.bits[0] & 0xffffff) == 0xABCDEF);
    gap_dib_free(&d);
}

TEST(gap1_polypatblt_null_array_false) {
    REQUIRE(ppb_ok());
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    GapDib d; if (!gap_dib_make(&d, 8, 8)) { CHECK(0); return; }
    CHECK(f(d.dc, PPB_PATCOPY, NULL, 1, 0) == FALSE);
    CHECK(f(d.dc, PPB_PATCOPY, NULL, 100, 0) == FALSE);
    gap_dib_free(&d);
}

TEST(gap1_polypatblt_nonzero_mode_false) {
    REQUIRE(ppb_ok());
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    GapDib d; if (!gap_dib_make(&d, 8, 8)) { CHECK(0); return; }
    HBRUSH b = U(GetStockObject)(BLACK_BRUSH);
    PATRECT_ pr = { 0, 0, 8, 8, b };
    static const ULONG modes[] = { 1, 2, 3, 0x10, 0x80000000, 0xffffffff };
    for (unsigned i = 0; i < sizeof modes / sizeof *modes; i++) {
        gap_dib_fill(&d, 0x00FFFFFF);
        CHECKF(f(d.dc, PPB_PATCOPY, &pr, 1, modes[i]) == FALSE, "mode %#lx", modes[i]);
        U(GdiFlush)();
        CHECKF((d.bits[0] & 0xffffff) == 0xffffff, "mode %#lx painted", modes[i]);
    }
    gap_dib_free(&d);
}

TEST(gap1_polypatblt_null_dc_false) {
    REQUIRE(ppb_ok());
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    HBRUSH b = U(GetStockObject)(BLACK_BRUSH);
    PATRECT_ pr = { 0, 0, 8, 8, b };
    CHECK(f(NULL, PPB_PATCOPY, &pr, 1, 0) == FALSE);
}

TEST(gap1_polypatblt_stale_dc_false) {
    REQUIRE(ppb_ok());
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    HDC dc = U(CreateCompatibleDC)(NULL);
    U(DeleteDC)(dc);
    HBRUSH b = U(GetStockObject)(BLACK_BRUSH);
    PATRECT_ pr = { 0, 0, 8, 8, b };
    CHECK(f(dc, PPB_PATCOPY, &pr, 1, 0) == FALSE);
}

TEST(gap1_polypatblt_non_dc_handle_false) {
    REQUIRE(ppb_ok()); REQUIRE(U(CreateBitmap));
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    HBITMAP bm = U(CreateBitmap)(4, 4, 1, 1, NULL);
    HBRUSH b = U(GetStockObject)(BLACK_BRUSH);
    PATRECT_ pr = { 0, 0, 8, 8, b };
    CHECK(f((HDC)bm, PPB_PATCOPY, &pr, 1, 0) == FALSE);
    CHECK(f((HDC)b, PPB_PATCOPY, &pr, 1, 0) == FALSE);
    CHECK(f((HDC)(ULONG_PTR)0x1234, PPB_PATCOPY, &pr, 1, 0) == FALSE);
    U(DeleteObject)(bm);
}

TEST(gap1_polypatblt_does_not_change_selected_brush) {
    REQUIRE(ppb_ok()); REQUIRE(U(GetCurrentObject));
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    GapDib d; if (!gap_dib_make(&d, 16, 16)) { CHECK(0); return; }
    HBRUSH sel = U(CreateSolidBrush)(RGB(1, 2, 3)), use = U(CreateSolidBrush)(RGB(250, 250, 0));
    HGDIOBJ oldb = U(SelectObject)(d.dc, sel);
    PATRECT_ pr = { 0, 0, 16, 16, use };
    CHECK(f(d.dc, PPB_PATCOPY, &pr, 1, 0) == TRUE);
    U(GdiFlush)();
    CHECK(U(GetCurrentObject)(d.dc, OBJ_BRUSH) == (HGDIOBJ)sel);
    CHECK((d.bits[0] & 0xffffff) == (DWORD)GAP_RGB32(RGB(250, 250, 0)));
    U(SelectObject)(d.dc, oldb);
    U(DeleteObject)(sel); U(DeleteObject)(use);
    gap_dib_free(&d);
}

TEST(gap1_polypatblt_zero_size_rects_paint_nothing) {
    REQUIRE(ppb_ok());
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    GapDib d; if (!gap_dib_make(&d, 16, 16)) { CHECK(0); return; }
    gap_dib_fill(&d, 0x00202020);
    HBRUSH b = U(CreateSolidBrush)(RGB(255, 255, 255));
    PATRECT_ pr[3] = { { 2, 2, 0, 5, b }, { 4, 4, 5, 0, b }, { 6, 6, 0, 0, b } };
    BOOL r = f(d.dc, PPB_PATCOPY, pr, 3, 0);
    t_note("PolyPatBlt zero-sized rects -> %d", r);
    U(GdiFlush)();
    int bad = 0;
    for (int i = 0; i < 256; i++) if ((d.bits[i] & 0xffffff) != 0x202020) bad++;
    CHECKF(bad == 0, "%d pixels changed", bad);
    U(DeleteObject)(b); gap_dib_free(&d);
}

TEST(gap1_polypatblt_clipped_to_bitmap) {
    REQUIRE(ppb_ok());
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    GapDib d; if (!gap_dib_make(&d, 16, 16)) { CHECK(0); return; }
    gap_dib_fill(&d, 0);
    HBRUSH b = U(CreateSolidBrush)(RGB(255, 255, 255));
    PATRECT_ pr[3] = { { -5, -5, 10, 10, b }, { 12, 12, 100, 100, b }, { 1000, 1000, 10, 10, b } };
    CHECK(f(d.dc, PPB_PATCOPY, pr, 3, 0) == TRUE);
    U(GdiFlush)();
    int bad = 0;
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
        int in = (x < 5 && y < 5) || (x >= 12 && y >= 12);
        if (((d.bits[y * 16 + x] & 0xffffff) == 0xffffff) != in) bad++;
    }
    CHECKF(bad == 0, "%d bad pixels", bad);
    U(DeleteObject)(b); gap_dib_free(&d);
}

TEST(gap1_polypatblt_overlapping_rects_last_wins) {
    REQUIRE(ppb_ok());
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    GapDib d; if (!gap_dib_make(&d, 16, 16)) { CHECK(0); return; }
    gap_dib_fill(&d, 0);
    HBRUSH a = U(CreateSolidBrush)(RGB(255, 0, 0)), b = U(CreateSolidBrush)(RGB(0, 0, 255));
    PATRECT_ pr[2] = { { 0, 0, 10, 10, a }, { 5, 5, 10, 10, b } };
    CHECK(f(d.dc, PPB_PATCOPY, pr, 2, 0) == TRUE);
    U(GdiFlush)();
    CHECK((d.bits[2 * 16 + 2] & 0xffffff) == (DWORD)GAP_RGB32(RGB(255, 0, 0)));
    CHECK((d.bits[7 * 16 + 7] & 0xffffff) == (DWORD)GAP_RGB32(RGB(0, 0, 255)));
    CHECK((d.bits[12 * 16 + 12] & 0xffffff) == (DWORD)GAP_RGB32(RGB(0, 0, 255)));
    U(DeleteObject)(a); U(DeleteObject)(b); gap_dib_free(&d);
}

static void fill_rects(PATRECT_ *pr, int n, HBRUSH b, int cols) {
    for (int i = 0; i < n; i++) { pr[i].x = i % cols; pr[i].y = i / cols; pr[i].w = 1; pr[i].h = 1; pr[i].br = b; }
}

static void count_test(int n, int expect_true) {
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    GapDib d; if (!gap_dib_make(&d, 128, 128)) { CHECK(0); return; }
    gap_dib_fill(&d, 0);
    HBRUSH b = U(CreateSolidBrush)(RGB(255, 255, 255));
    PATRECT_ *pr = (PATRECT_ *)calloc(n, sizeof *pr);
    fill_rects(pr, n, b, 128);
    BOOL r = f(d.dc, PPB_PATCOPY, pr, n, 0);
    if (expect_true) CHECKF(r == TRUE, "count %d", n); else t_note("PolyPatBlt count %d -> %d", n, r);
    U(GdiFlush)();
    int painted = 0;
    for (int i = 0; i < 128 * 128; i++) if ((d.bits[i] & 0xffffff) == 0xffffff) painted++;
    if (r) CHECKF(painted == n, "count %d painted %d", n, painted);
    free(pr); U(DeleteObject)(b); gap_dib_free(&d);
}
TEST(gap1_polypatblt_count_100) { REQUIRE(ppb_ok()); REQUIRE(UN(PolyPatBlt)); count_test(100, 1); }
TEST(gap1_polypatblt_count_batch_limit_minus1) { REQUIRE(ppb_ok()); REQUIRE(UN(PolyPatBlt)); count_test(0xaa9, 1); }
TEST(gap1_polypatblt_count_batch_limit) { REQUIRE(ppb_ok()); REQUIRE(UN(PolyPatBlt)); count_test(0xaaa, 1); }
TEST(gap1_polypatblt_count_batch_limit_plus1) { REQUIRE(ppb_ok()); REQUIRE(UN(PolyPatBlt)); count_test(0xaab, 0); }
TEST(gap1_polypatblt_count_5000) { REQUIRE(ppb_ok()); REQUIRE(UN(PolyPatBlt)); count_test(5000, 0); }

TEST(gap1_polypatblt_repeated_batches_flush_correctly) {
    REQUIRE(ppb_ok());
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    GapDib d; if (!gap_dib_make(&d, 32, 32)) { CHECK(0); return; }
    gap_dib_fill(&d, 0);
    HBRUSH b = U(CreateSolidBrush)(RGB(255, 255, 255));
    int ok = 1;
    for (int i = 0; i < 32; i++) {
        PATRECT_ pr = { i, i, 1, 1, b };
        if (f(d.dc, PPB_PATCOPY, &pr, 1, 0) != TRUE) ok = 0;
    }
    CHECK(ok);
    U(GdiFlush)();
    int bad = 0;
    for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++)
        if (((d.bits[y * 32 + x] & 0xffffff) == 0xffffff) != (x == y)) bad++;
    CHECKF(bad == 0, "%d bad", bad);
    U(DeleteObject)(b); gap_dib_free(&d);
}

TEST(gap1_polypatblt_interleaved_with_gdi_calls) {
    REQUIRE(ppb_ok()); REQUIRE(U(PatBlt));
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    GapDib d; if (!gap_dib_make(&d, 16, 16)) { CHECK(0); return; }
    gap_dib_fill(&d, 0);
    HBRUSH r = U(CreateSolidBrush)(RGB(255, 0, 0)), g = U(CreateSolidBrush)(RGB(0, 255, 0));
    PATRECT_ p1 = { 0, 0, 16, 16, r }, p2 = { 4, 4, 8, 8, g };
    f(d.dc, PPB_PATCOPY, &p1, 1, 0);
    HGDIOBJ old = U(SelectObject)(d.dc, g);
    U(PatBlt)(d.dc, 6, 6, 2, 2, PPB_PATINV);   /* green over red = red ^ green = yellow */
    U(SelectObject)(d.dc, old);
    f(d.dc, PPB_PATCOPY, &p2, 1, 0);
    U(GdiFlush)();
    CHECK((d.bits[1 * 16 + 1] & 0xffffff) == (DWORD)GAP_RGB32(RGB(255, 0, 0)));
    CHECK((d.bits[6 * 16 + 6] & 0xffffff) == (DWORD)GAP_RGB32(RGB(0, 255, 0)));   /* later PolyPatBlt overwrote */
    U(DeleteObject)(r); U(DeleteObject)(g); gap_dib_free(&d);
}

TEST(gap1_polypatblt_invalid_brush_in_rect_note) {
    REQUIRE(ppb_ok());
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    GapDib d; if (!gap_dib_make(&d, 16, 16)) { CHECK(0); return; }
    gap_dib_fill(&d, 0x00445566);
    PATRECT_ pr = { 0, 0, 8, 8, (HBRUSH)(ULONG_PTR)0x1234 };
    BOOL r = f(d.dc, PPB_PATCOPY, &pr, 1, 0);
    U(GdiFlush)();
    t_note("PolyPatBlt with bogus brush handle -> %d, pixel %06lx", r, d.bits[0] & 0xffffff);
    gap_dib_free(&d);
}

TEST(gap1_polypatblt_null_brush_in_rect_note) {
    REQUIRE(ppb_ok());
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    GapDib d; if (!gap_dib_make(&d, 16, 16)) { CHECK(0); return; }
    gap_dib_fill(&d, 0x00445566);
    PATRECT_ pr = { 0, 0, 8, 8, NULL };
    BOOL r = f(d.dc, PPB_PATCOPY, &pr, 1, 0);
    U(GdiFlush)();
    t_note("PolyPatBlt with NULL brush -> %d, pixel %06lx", r, d.bits[0] & 0xffffff);
    gap_dib_free(&d);
}

TEST(gap1_polypatblt_null_brush_stock_leaves_dest) {
    REQUIRE(ppb_ok());
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    GapDib d; if (!gap_dib_make(&d, 16, 16)) { CHECK(0); return; }
    gap_dib_fill(&d, 0x00445566);
    PATRECT_ pr = { 0, 0, 8, 8, U(GetStockObject)(NULL_BRUSH) };
    BOOL r = f(d.dc, PPB_PATCOPY, &pr, 1, 0);
    U(GdiFlush)();
    t_note("PolyPatBlt with NULL_BRUSH -> %d pixel %06lx", r, d.bits[0] & 0xffffff);
    CHECK((d.bits[0] & 0xffffff) == 0x445566);
    gap_dib_free(&d);
}

TEST(gap1_polypatblt_dc_brush_color_note) {
    REQUIRE(ppb_ok()); REQUIRE(U(SetDCBrushColor));
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    GapDib d; if (!gap_dib_make(&d, 16, 16)) { CHECK(0); return; }
    gap_dib_fill(&d, 0);
    U(SetDCBrushColor)(d.dc, RGB(9, 99, 199));
    PATRECT_ pr = { 0, 0, 8, 8, U(GetStockObject)(DC_BRUSH) };
    BOOL r = f(d.dc, PPB_PATCOPY, &pr, 1, 0);
    U(GdiFlush)();
    t_note("DC_BRUSH polypatblt -> %d pixel %06lx (want %06x)", r, d.bits[0] & 0xffffff, GAP_RGB32(RGB(9, 99, 199)));
    gap_dib_free(&d);
}

TEST(gap1_polypatblt_gdi32_and_gdi32full_agree) {
    REQUIRE(ppb_ok());
    PolyPatBlt_pfn a = (PolyPatBlt_pfn)t_proc_gdi32("PolyPatBlt"), c = (PolyPatBlt_pfn)t_proc_gdi32full("PolyPatBlt");
    REQUIRE(a); REQUIRE(c);
    GapDib d1, d2; if (!gap_dib_make(&d1, 16, 16)) { CHECK(0); return; }
    if (!gap_dib_make(&d2, 16, 16)) { CHECK(0); gap_dib_free(&d1); return; }
    gap_dib_fill(&d1, 0); gap_dib_fill(&d2, 0);
    HBRUSH b = U(CreateSolidBrush)(RGB(31, 41, 59));
    PATRECT_ pr = { 2, 3, 9, 7, b };
    CHECK(a(d1.dc, PPB_PATCOPY, &pr, 1, 0) == TRUE);
    CHECK(c(d2.dc, PPB_PATCOPY, &pr, 1, 0) == TRUE);
    U(GdiFlush)();
    CHECK(!memcmp(d1.bits, d2.bits, 16 * 16 * 4));
    U(DeleteObject)(b); gap_dib_free(&d1); gap_dib_free(&d2);
}

/* ---- parity against PatBlt on several bit depths ---- */
typedef struct { HDC dc; HBITMAP bmp, old; BYTE *bits; int w, h, stride; } BppDib;
static int bpp_make(BppDib *d, int w, int h, int bpp) {
    memset(d, 0, sizeof *d);
    union { BITMAPINFO bi; BYTE raw[sizeof(BITMAPINFOHEADER) + 12 + 256 * 4]; } u; memset(&u, 0, sizeof u);
    u.bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); u.bi.bmiHeader.biWidth = w; u.bi.bmiHeader.biHeight = -h;
    u.bi.bmiHeader.biPlanes = 1; u.bi.bmiHeader.biBitCount = bpp; u.bi.bmiHeader.biCompression = BI_RGB;
    if (bpp <= 8) {
        int n = 1 << bpp; u.bi.bmiHeader.biClrUsed = n;
        for (int i = 0; i < n; i++) {
            RGBQUAD *q = &u.bi.bmiColors[i];
            if (bpp == 1) { q->rgbRed = q->rgbGreen = q->rgbBlue = i ? 255 : 0; }
            else if (bpp == 4) { q->rgbRed = (i & 1) ? 255 : 0; q->rgbGreen = (i & 2) ? 255 : 0; q->rgbBlue = (i & 4) ? 255 : 0; if (i == 8) q->rgbRed = q->rgbGreen = q->rgbBlue = 128; }
            else { q->rgbRed = (BYTE)i; q->rgbGreen = (BYTE)(i * 3); q->rgbBlue = (BYTE)(255 - i); }
        }
    }
    d->dc = U(CreateCompatibleDC)(NULL);
    void *bits = NULL;
    d->bmp = U(CreateDIBSection)(d->dc, &u.bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!d->bmp || !bits) { U(DeleteDC)(d->dc); d->dc = NULL; return 0; }
    d->bits = bits; d->w = w; d->h = h; d->stride = ((w * bpp + 31) / 32) * 4;
    d->old = U(SelectObject)(d->dc, d->bmp);
    return 1;
}
static void bpp_free(BppDib *d) { if (!d->dc) return; U(SelectObject)(d->dc, d->old); U(DeleteObject)(d->bmp); U(DeleteDC)(d->dc); d->dc = NULL; }

static void parity_for_bpp(int bpp) {
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    static const DWORD rops[] = { PPB_PATCOPY, PPB_PATINV, PPB_DSTINV, PPB_BLACK, PPB_WHITE };
    static const COLORREF cols[] = { RGB(255, 255, 255), RGB(0, 0, 0), RGB(255, 0, 0), RGB(0, 128, 255), RGB(77, 77, 77) };
    for (unsigned ri = 0; ri < 5; ri++) {
        BppDib a, b;
        if (!bpp_make(&a, 37, 19, bpp)) { CHECKF(0, "make bpp %d", bpp); return; }
        if (!bpp_make(&b, 37, 19, bpp)) { bpp_free(&a); CHECK(0); return; }
        for (int i = 0; i < a.stride * a.h; i++) a.bits[i] = b.bits[i] = (BYTE)(i * 7 + 3);
        HBRUSH br[5]; PATRECT_ pr[5];
        for (int i = 0; i < 5; i++) {
            br[i] = U(CreateSolidBrush)(cols[(i + ri) % 5]);
            pr[i].x = i * 6 - 2; pr[i].y = i * 3; pr[i].w = 11; pr[i].h = 9; pr[i].br = br[i];
        }
        BOOL r = f(a.dc, rops[ri], pr, 5, 0);
        CHECKF(r == TRUE, "bpp %d rop %#lx", bpp, rops[ri]);
        HGDIOBJ old = NULL;
        for (int i = 0; i < 5; i++) {
            HGDIOBJ o = U(SelectObject)(b.dc, br[i]); if (!old) old = o;
            U(PatBlt)(b.dc, pr[i].x, pr[i].y, pr[i].w, pr[i].h, rops[ri]);
        }
        U(SelectObject)(b.dc, old);
        U(GdiFlush)();
        CHECKF(!memcmp(a.bits, b.bits, a.stride * a.h), "bpp %d rop %#lx differs from PatBlt sequence", bpp, rops[ri]);
        for (int i = 0; i < 5; i++) U(DeleteObject)(br[i]);
        bpp_free(&a); bpp_free(&b);
    }
}
TEST(gap1_polypatblt_parity_patblt_1bpp)  { REQUIRE(ppb_ok()); REQUIRE(U(PatBlt)); REQUIRE(UN(PolyPatBlt)); parity_for_bpp(1); }
TEST(gap1_polypatblt_parity_patblt_4bpp)  { REQUIRE(ppb_ok()); REQUIRE(U(PatBlt)); REQUIRE(UN(PolyPatBlt)); parity_for_bpp(4); }
TEST(gap1_polypatblt_parity_patblt_8bpp)  { REQUIRE(ppb_ok()); REQUIRE(U(PatBlt)); REQUIRE(UN(PolyPatBlt)); parity_for_bpp(8); }
TEST(gap1_polypatblt_parity_patblt_16bpp) { REQUIRE(ppb_ok()); REQUIRE(U(PatBlt)); REQUIRE(UN(PolyPatBlt)); parity_for_bpp(16); }
TEST(gap1_polypatblt_parity_patblt_24bpp) { REQUIRE(ppb_ok()); REQUIRE(U(PatBlt)); REQUIRE(UN(PolyPatBlt)); parity_for_bpp(24); }
TEST(gap1_polypatblt_parity_patblt_32bpp) { REQUIRE(ppb_ok()); REQUIRE(U(PatBlt)); REQUIRE(UN(PolyPatBlt)); parity_for_bpp(32); }

TEST(gap1_polypatblt_pattern_brush_parity) {
    REQUIRE(ppb_ok()); REQUIRE(U(PatBlt)); REQUIRE(U(CreatePatternBrush)); REQUIRE(U(CreateBitmap));
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    static const WORD pat[8] = { 0xAA, 0x55, 0xAA, 0x55, 0xF0, 0xF0, 0x0F, 0x0F };
    HBITMAP pb = U(CreateBitmap)(8, 8, 1, 1, pat);
    HBRUSH br = U(CreatePatternBrush)(pb);
    GapDib a, b; if (!gap_dib_make(&a, 40, 40)) { CHECK(0); return; }
    if (!gap_dib_make(&b, 40, 40)) { CHECK(0); gap_dib_free(&a); return; }
    gap_dib_fill(&a, 0x00808080); gap_dib_fill(&b, 0x00808080);
    PATRECT_ pr = { 3, 5, 30, 20, br };
    CHECK(f(a.dc, PPB_PATCOPY, &pr, 1, 0) == TRUE);
    HGDIOBJ o = U(SelectObject)(b.dc, br);
    U(PatBlt)(b.dc, 3, 5, 30, 20, PPB_PATCOPY);
    U(SelectObject)(b.dc, o);
    U(GdiFlush)();
    CHECK(!memcmp(a.bits, b.bits, 40 * 40 * 4));
    U(DeleteObject)(br); U(DeleteObject)(pb); gap_dib_free(&a); gap_dib_free(&b);
}

TEST(gap1_polypatblt_hatch_brush_parity) {
    REQUIRE(ppb_ok()); REQUIRE(U(PatBlt)); REQUIRE(U(CreateHatchBrush));
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    for (int hs = HS_HORIZONTAL; hs <= HS_DIAGCROSS; hs++) {
        HBRUSH br = U(CreateHatchBrush)(hs, RGB(255, 0, 0));
        GapDib a, b; if (!gap_dib_make(&a, 24, 24)) { CHECK(0); return; }
        if (!gap_dib_make(&b, 24, 24)) { CHECK(0); gap_dib_free(&a); return; }
        gap_dib_fill(&a, 0x00FFFFFF); gap_dib_fill(&b, 0x00FFFFFF);
        U(SetBkColor)(a.dc, RGB(0, 0, 255)); U(SetBkColor)(b.dc, RGB(0, 0, 255));
        PATRECT_ pr = { 1, 2, 20, 18, br };
        CHECK(f(a.dc, PPB_PATCOPY, &pr, 1, 0) == TRUE);
        HGDIOBJ o = U(SelectObject)(b.dc, br);
        U(PatBlt)(b.dc, 1, 2, 20, 18, PPB_PATCOPY);
        U(SelectObject)(b.dc, o);
        U(GdiFlush)();
        CHECKF(!memcmp(a.bits, b.bits, 24 * 24 * 4), "hatch %d", hs);
        U(DeleteObject)(br); gap_dib_free(&a); gap_dib_free(&b);
    }
}

TEST(gap1_polypatblt_brush_origin_honoured) {
    REQUIRE(ppb_ok()); REQUIRE(U(PatBlt)); REQUIRE(U(CreateHatchBrush)); REQUIRE(U(SetBrushOrgEx));
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    HBRUSH br = U(CreateHatchBrush)(HS_VERTICAL, RGB(255, 255, 255));
    GapDib a, b; if (!gap_dib_make(&a, 32, 32)) { CHECK(0); return; }
    if (!gap_dib_make(&b, 32, 32)) { CHECK(0); gap_dib_free(&a); return; }
    gap_dib_fill(&a, 0); gap_dib_fill(&b, 0);
    U(SetBkMode)(a.dc, OPAQUE); U(SetBkMode)(b.dc, OPAQUE);
    U(SetBrushOrgEx)(a.dc, 3, 2, NULL); U(SetBrushOrgEx)(b.dc, 3, 2, NULL);
    PATRECT_ pr = { 0, 0, 32, 32, br };
    CHECK(f(a.dc, PPB_PATCOPY, &pr, 1, 0) == TRUE);
    HGDIOBJ o = U(SelectObject)(b.dc, br);
    U(PatBlt)(b.dc, 0, 0, 32, 32, PPB_PATCOPY);
    U(SelectObject)(b.dc, o);
    U(GdiFlush)();
    CHECK(!memcmp(a.bits, b.bits, 32 * 32 * 4));
    U(DeleteObject)(br); gap_dib_free(&a); gap_dib_free(&b);
}

TEST(gap1_polypatblt_respects_clip_region) {
    REQUIRE(ppb_ok()); REQUIRE(U(IntersectClipRect));
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    GapDib d; if (!gap_dib_make(&d, 20, 20)) { CHECK(0); return; }
    gap_dib_fill(&d, 0);
    U(IntersectClipRect)(d.dc, 5, 5, 15, 15);
    HBRUSH b = U(CreateSolidBrush)(RGB(255, 255, 255));
    PATRECT_ pr = { 0, 0, 20, 20, b };
    CHECK(f(d.dc, PPB_PATCOPY, &pr, 1, 0) == TRUE);
    U(GdiFlush)();
    int bad = 0;
    for (int y = 0; y < 20; y++) for (int x = 0; x < 20; x++) {
        int in = x >= 5 && x < 15 && y >= 5 && y < 15;
        if (((d.bits[y * 20 + x] & 0xffffff) == 0xffffff) != in) bad++;
    }
    CHECKF(bad == 0, "%d bad", bad);
    U(DeleteObject)(b); gap_dib_free(&d);
}

TEST(gap1_polypatblt_honours_mapping_mode) {
    REQUIRE(ppb_ok()); REQUIRE(U(SetViewportOrgEx));
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    GapDib d; if (!gap_dib_make(&d, 20, 20)) { CHECK(0); return; }
    gap_dib_fill(&d, 0);
    U(SetViewportOrgEx)(d.dc, 6, 4, NULL);
    HBRUSH b = U(CreateSolidBrush)(RGB(255, 255, 255));
    PATRECT_ pr = { 0, 0, 4, 3, b };
    CHECK(f(d.dc, PPB_PATCOPY, &pr, 1, 0) == TRUE);
    U(GdiFlush)();
    int bad = 0;
    for (int y = 0; y < 20; y++) for (int x = 0; x < 20; x++) {
        int in = x >= 6 && x < 10 && y >= 4 && y < 7;
        if (((d.bits[y * 20 + x] & 0xffffff) == 0xffffff) != in) bad++;
    }
    CHECKF(bad == 0, "%d bad", bad);
    U(DeleteObject)(b); gap_dib_free(&d);
}

/* EMF DC: PolyPatBlt takes the per-rect SelectObject + PatBlt route, so it records */
static int CALLBACK cnt_cb(HDC dc, HANDLETABLE *ht, const ENHMETARECORD *r, int n, LPARAM lp) {
    int *c = (int *)lp;
    if (r->iType < 256) c[r->iType]++;
    return 1;
}
TEST(gap1_polypatblt_on_emf_dc_records_patblts) {
    REQUIRE(ppb_ok()); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(EnumEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    CHECK(m != NULL); if (!m) return;
    HBRUSH r = U(CreateSolidBrush)(RGB(255, 0, 0)), g = U(CreateSolidBrush)(RGB(0, 255, 0));
    PATRECT_ pr[3] = { { 0, 0, 5, 5, r }, { 5, 5, 5, 5, g }, { 10, 10, 5, 5, r } };
    BOOL ret = f(m, PPB_PATCOPY, pr, 3, 0);
    CHECK(ret == TRUE);
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    CHECK(h != NULL);
    int cnt[256]; memset(cnt, 0, sizeof cnt);
    RECT rc = { 0, 0, 100, 100 };
    U(EnumEnhMetaFile)(NULL, h, cnt_cb, cnt, &rc);
    /* a PatBlt is recorded as a source-less EMR_BITBLT (or EMR_PATBLT) */
    CHECKF(cnt[EMR_PATBLT] + cnt[76] == 3, "PATBLT/BITBLT records %d + %d", cnt[EMR_PATBLT], cnt[76]);
    t_note("select records %d, createbrush %d", cnt[EMR_SELECTOBJECT], cnt[EMR_CREATEBRUSHINDIRECT]);
    U(DeleteEnhMetaFile)(h);
    U(DeleteObject)(r); U(DeleteObject)(g);
}
TEST(gap1_polypatblt_emf_playback_pixels) {
    REQUIRE(ppb_ok()); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(PlayEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    PolyPatBlt_pfn f = UN(PolyPatBlt);
    REQUIRE(f);
    RECT frame = { 0, 0, 3200, 3200 };
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, &frame, NULL);
    if (!m) { CHECK(0); return; }
    HBRUSH r = U(CreateSolidBrush)(RGB(255, 0, 0));
    PATRECT_ pr = { 8, 8, 16, 16, r };
    f(m, PPB_PATCOPY, &pr, 1, 0);
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    GapDib d; if (!gap_dib_make(&d, 64, 64)) { CHECK(0); return; }
    gap_dib_fill(&d, 0x00FFFFFF);
    RECT dst = { 0, 0, 64, 64 };
    t_note("PlayEnhMetaFile -> %d", U(PlayEnhMetaFile)(d.dc, h, &dst));
    U(GdiFlush)();
    t_note("center pixel after playback %06lx", d.bits[16 * 64 + 16] & 0xffffff);
    U(DeleteEnhMetaFile)(h); U(DeleteObject)(r); gap_dib_free(&d);
}
