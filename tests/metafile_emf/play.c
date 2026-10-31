#include "emf.h"

#define RED   0xFF0000
#define GREEN 0x00FF00
#define BLUE  0x0000FF
#define WHITE 0xFFFFFF

/* ---- helpers ---- */
static void fill_rect_brush(HDC dc, int x, int y, int w, int h, COLORREF c) {
    HBRUSH b = U(CreateSolidBrush)(c);
    HGDIOBJ o = U(SelectObject)(dc, b);
    U(PatBlt)(dc, x, y, w, h, PATCOPY);
    U(SelectObject)(dc, o);
    U(DeleteObject)(b);
}

/* Record a single filled rect on a memory-DC reference of w x h. */
static HENHMETAFILE rec_rect(int rx, int ry, int rw, int rh, COLORREF c) {
    HDC dc = emf_begin();
    if (!dc) return NULL;
    fill_rect_brush(dc, rx, ry, rw, rh, c);
    return emf_end(dc);
}

/* ---- scaling into a destination rect ---- */

TEST(emf_play_fills_destination_rect) {
    REQUIRE(U(PlayEnhMetaFile));
    HENHMETAFILE h = rec_rect(20, 20, 40, 30, RGB(255, 0, 0));
    emf_dib d;
    if (!h || !emf_dib_make(&d, 200, 200, WHITE)) { t_skip("setup"); return; }
    RECT rc = {100, 100, 180, 160};
    CHECK(U(PlayEnhMetaFile)(d.dc, h, &rc));
    /* the picture frame equals the drawn bounds, so the rect fills the target */
    EXPECT_EQ(emf_px(&d, 110, 110), RED);
    EXPECT_EQ(emf_px(&d, 140, 130), RED);
    EXPECT_EQ(emf_px(&d, 170, 150), RED);
    EXPECT_EQ(emf_px(&d, 96, 130), WHITE);
    EXPECT_EQ(emf_px(&d, 184, 130), WHITE);
    EXPECT_EQ(emf_px(&d, 140, 96), WHITE);
    EXPECT_EQ(emf_px(&d, 140, 164), WHITE);
    EXPECT_EQ(emf_px(&d, 10, 10), WHITE);
    EXPECT_EQ(emf_px(&d, 199, 199), WHITE);
    emf_dib_free(&d); U(DeleteEnhMetaFile)(h);
}

TEST(emf_play_scale_up) {
    REQUIRE(U(PlayEnhMetaFile));
    HENHMETAFILE h = rec_rect(5, 5, 10, 10, RGB(0, 255, 0));
    emf_dib d;
    if (!h || !emf_dib_make(&d, 200, 200, WHITE)) { t_skip("setup"); return; }
    RECT rc = {10, 10, 190, 190};
    CHECK(U(PlayEnhMetaFile)(d.dc, h, &rc));
    EXPECT_EQ(emf_px(&d, 100, 100), GREEN);
    EXPECT_EQ(emf_px(&d, 15, 15), GREEN);
    EXPECT_EQ(emf_px(&d, 185, 185), GREEN);
    EXPECT_EQ(emf_px(&d, 5, 5), WHITE);
    EXPECT_EQ(emf_px(&d, 195, 195), WHITE);
    emf_dib_free(&d); U(DeleteEnhMetaFile)(h);
}

TEST(emf_play_scale_down) {
    REQUIRE(U(PlayEnhMetaFile));
    HENHMETAFILE h = rec_rect(0, 0, 300, 300, RGB(0, 0, 255));
    emf_dib d;
    if (!h || !emf_dib_make(&d, 100, 100, WHITE)) { t_skip("setup"); return; }
    RECT rc = {20, 20, 60, 60};
    CHECK(U(PlayEnhMetaFile)(d.dc, h, &rc));
    EXPECT_EQ(emf_px(&d, 40, 40), BLUE);
    EXPECT_EQ(emf_px(&d, 24, 24), BLUE);
    EXPECT_EQ(emf_px(&d, 56, 56), BLUE);
    EXPECT_EQ(emf_px(&d, 10, 40), WHITE);
    EXPECT_EQ(emf_px(&d, 70, 40), WHITE);
    emf_dib_free(&d); U(DeleteEnhMetaFile)(h);
}

TEST(emf_play_anisotropic_stretch) {
    REQUIRE(U(PlayEnhMetaFile));
    HENHMETAFILE h = rec_rect(0, 0, 10, 10, RGB(255, 0, 0));
    emf_dib d;
    if (!h || !emf_dib_make(&d, 200, 100, WHITE)) { t_skip("setup"); return; }
    RECT rc = {0, 0, 180, 30};
    CHECK(U(PlayEnhMetaFile)(d.dc, h, &rc));
    EXPECT_EQ(emf_px(&d, 170, 15), RED);
    EXPECT_EQ(emf_px(&d, 90, 25), RED);
    EXPECT_EQ(emf_px(&d, 90, 40), WHITE);
    EXPECT_EQ(emf_px(&d, 190, 15), WHITE);
    emf_dib_free(&d); U(DeleteEnhMetaFile)(h);
}

TEST(emf_play_negative_origin_rect) {
    REQUIRE(U(PlayEnhMetaFile));
    HENHMETAFILE h = rec_rect(0, 0, 20, 20, RGB(255, 0, 0));
    emf_dib d;
    if (!h || !emf_dib_make(&d, 100, 100, WHITE)) { t_skip("setup"); return; }
    RECT rc = {-30, -30, 50, 50};
    CHECK(U(PlayEnhMetaFile)(d.dc, h, &rc));
    EXPECT_EQ(emf_px(&d, 10, 10), RED);
    EXPECT_EQ(emf_px(&d, 45, 45), RED);
    EXPECT_EQ(emf_px(&d, 60, 60), WHITE);
    emf_dib_free(&d); U(DeleteEnhMetaFile)(h);
}

TEST(emf_play_offscreen_rect_draws_nothing) {
    REQUIRE(U(PlayEnhMetaFile));
    HENHMETAFILE h = rec_rect(0, 0, 20, 20, RGB(255, 0, 0));
    emf_dib d;
    if (!h || !emf_dib_make(&d, 50, 50, WHITE)) { t_skip("setup"); return; }
    RECT rc = {500, 500, 600, 600};
    U(PlayEnhMetaFile)(d.dc, h, &rc);
    int changed = 0;
    for (int i = 0; i < 2500; i++) if ((d.bits[i] & 0xFFFFFF) != WHITE) changed++;
    EXPECT_EQ(changed, 0);
    emf_dib_free(&d); U(DeleteEnhMetaFile)(h);
}

TEST(emf_play_repeat_same_result) {
    REQUIRE(U(PlayEnhMetaFile));
    HENHMETAFILE h = rec_rect(3, 3, 14, 14, RGB(255, 0, 0));
    emf_dib a, b;
    if (!h || !emf_dib_make(&a, 80, 80, WHITE) || !emf_dib_make(&b, 80, 80, WHITE)) { t_skip("setup"); return; }
    RECT rc = {10, 10, 70, 70};
    U(PlayEnhMetaFile)(a.dc, h, &rc);
    U(PlayEnhMetaFile)(b.dc, h, &rc);
    CHECK(!memcmp(a.bits, b.bits, 80 * 80 * 4));
    emf_dib_free(&a); emf_dib_free(&b); U(DeleteEnhMetaFile)(h);
}

TEST(emf_play_dest_dc_map_mode_applies) {
    REQUIRE(U(PlayEnhMetaFile));
    HENHMETAFILE h = rec_rect(0, 0, 20, 20, RGB(255, 0, 0));
    emf_dib d;
    if (!h || !emf_dib_make(&d, 100, 100, WHITE)) { t_skip("setup"); return; }
    /* the rect is in logical units of the destination DC */
    U(SetMapMode)(d.dc, MM_ANISOTROPIC);
    U(SetWindowExtEx)(d.dc, 1, 1, NULL); U(SetViewportExtEx)(d.dc, 2, 2, NULL);
    RECT rc = {10, 10, 30, 30};   /* logical -> device 20..60 */
    CHECK(U(PlayEnhMetaFile)(d.dc, h, &rc));
    EXPECT_EQ(emf_px(&d, 40, 40), RED);
    EXPECT_EQ(emf_px(&d, 24, 24), RED);
    EXPECT_EQ(emf_px(&d, 56, 56), RED);
    EXPECT_EQ(emf_px(&d, 15, 15), WHITE);
    EXPECT_EQ(emf_px(&d, 70, 70), WHITE);
    emf_dib_free(&d); U(DeleteEnhMetaFile)(h);
}

/* ---- playback equals direct drawing ---- */

typedef void (*draw_fn)(HDC);

/* Record `fn` on a reference DIB DC of size w x h with a frame covering the entire device (so play is 1:1),
 * play into a fresh DIB, draw `fn` directly into another DIB and count differing pixels. */
static int compare_play_direct(draw_fn fn, int w, int h, int *total_diff_out) {
    emf_dib ref, a, b;
    int diff = -1;
    if (!emf_dib_make(&ref, w, h, WHITE)) return -1;
    int mmx = U(GetDeviceCaps)(ref.dc, HORZSIZE), mmy = U(GetDeviceCaps)(ref.dc, VERTSIZE);
    RECT frame = { 0, 0, mmx * 100, mmy * 100 };
    HDC dc = U(CreateEnhMetaFileW)(ref.dc, NULL, &frame, NULL);
    if (!dc) { emf_dib_free(&ref); return -1; }
    fn(dc);
    HENHMETAFILE m = emf_end(dc);
    if (m && emf_dib_make(&a, w, h, WHITE)) {
        if (emf_dib_make(&b, w, h, WHITE)) {
            RECT rc = {0, 0, w, h};
            U(PlayEnhMetaFile)(a.dc, m, &rc);
            fn(b.dc);
            diff = 0;
            for (int i = 0; i < w * h; i++) if (((a.bits[i] ^ b.bits[i]) & 0xFFFFFF) != 0) diff++;
            emf_dib_free(&b);
        }
        emf_dib_free(&a);
    }
    if (m) U(DeleteEnhMetaFile)(m);
    emf_dib_free(&ref);
    if (total_diff_out) *total_diff_out = diff;
    return diff;
}

static void d_rect(HDC dc) { fill_rect_brush(dc, 20, 20, 40, 30, RGB(255, 0, 0)); }
static void d_two_rects(HDC dc) { fill_rect_brush(dc, 10, 10, 50, 50, RGB(255, 0, 0)); fill_rect_brush(dc, 30, 30, 50, 50, RGB(0, 0, 255)); }
static void d_blackness(HDC dc) { U(PatBlt)(dc, 5, 5, 30, 30, BLACKNESS); }
static void d_invert(HDC dc) { U(PatBlt)(dc, 0, 0, 100, 100, DSTINVERT); U(PatBlt)(dc, 20, 20, 20, 20, DSTINVERT); }
static void d_wt(HDC dc) {
    XFORM x = {1, 0, 0, 1, 25, 15};
    U(SetGraphicsMode)(dc, GM_ADVANCED); U(SetWorldTransform)(dc, &x);
    fill_rect_brush(dc, 0, 0, 20, 20, RGB(0, 255, 0));
}
static void d_clip(HDC dc) { U(IntersectClipRect)(dc, 20, 20, 50, 50); fill_rect_brush(dc, 0, 0, 100, 100, RGB(255, 0, 0)); }
static void d_savedc(HDC dc) {
    U(SaveDC)(dc); U(IntersectClipRect)(dc, 0, 0, 40, 40); fill_rect_brush(dc, 0, 0, 100, 100, RGB(255, 0, 0));
    U(RestoreDC)(dc, -1); fill_rect_brush(dc, 60, 60, 20, 20, RGB(0, 255, 0));
}
static void d_mapmode(HDC dc) {
    U(SetMapMode)(dc, MM_ANISOTROPIC); U(SetWindowExtEx)(dc, 10, 10, NULL); U(SetViewportExtEx)(dc, 20, 20, NULL);
    fill_rect_brush(dc, 5, 5, 10, 10, RGB(255, 0, 0));
}
static void d_orgs(HDC dc) {
    U(SetViewportOrgEx)(dc, 30, 40, NULL); fill_rect_brush(dc, 0, 0, 20, 20, RGB(255, 0, 0));
    U(SetWindowOrgEx)(dc, -10, -10, NULL); fill_rect_brush(dc, 0, 0, 10, 10, RGB(0, 0, 255));
}
static void d_pixels(HDC dc) { for (int i = 0; i < 40; i++) U(SetPixelV)(dc, i * 2, i, RGB(255, 0, 0)); }
static void d_region(HDC dc) {
    HRGN r = U(CreateRectRgn)(10, 10, 70, 40); HBRUSH b = U(CreateSolidBrush)(RGB(0, 0, 255));
    U(FillRgn)(dc, r, b); U(DeleteObject)(r); U(DeleteObject)(b);
}
static void d_stretchdib(HDC dc) {
    BITMAPINFO bi; DWORD px[16]; for (int i = 0; i < 16; i++) px[i] = (i & 1) ? 0xFF0000 : 0x0000FF;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 4; bi.bmiHeader.biHeight = 4;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    U(StretchDIBits)(dc, 10, 10, 40, 40, 0, 0, 4, 4, px, &bi, DIB_RGB_COLORS, SRCCOPY);
}
static void d_bitblt_mem(HDC dc) {
    HDC m = U(CreateCompatibleDC)(NULL); HBITMAP b = U(CreateCompatibleBitmap)(dc, 1, 1);
    HBITMAP b2 = U(CreateBitmap)(20, 20, 1, 32, NULL);
    HGDIOBJ o = U(SelectObject)(m, b2);
    fill_rect_brush(m, 0, 0, 20, 20, RGB(255, 128, 0));
    U(BitBlt)(dc, 30, 30, 20, 20, m, 0, 0, SRCCOPY);
    U(SelectObject)(m, o); U(DeleteObject)(b2); U(DeleteObject)(b); U(DeleteDC)(m);
}

#define PD_TEST(name, fn, tol) \
TEST(emf_play_direct_##name) { \
    REQUIRE(U(PlayEnhMetaFile)); \
    int diff = 0; int r = compare_play_direct(fn, 100, 100, &diff); \
    if (r < 0) { t_skip("setup"); return; } \
    CHECKF(diff <= (tol), "%d pixels differ (tolerance %d)", diff, (tol)); \
    if (diff) t_note(#name ": %d differing pixels", diff); \
}
PD_TEST(rect, d_rect, 250)
PD_TEST(two_rects, d_two_rects, 250)
PD_TEST(blackness, d_blackness, 250)
PD_TEST(invert, d_invert, 250)
PD_TEST(world_transform, d_wt, 250)
PD_TEST(clip, d_clip, 250)
PD_TEST(savedc, d_savedc, 250)
PD_TEST(mapmode, d_mapmode, 250)
PD_TEST(orgs, d_orgs, 250)
PD_TEST(pixels, d_pixels, 250)
PD_TEST(region, d_region, 250)
PD_TEST(stretchdib, d_stretchdib, 250)
PD_TEST(bitblt_mem, d_bitblt_mem, 250)

TEST(emf_play_direct_reference_sanity) {
    /* the direct drawing really draws */
    emf_dib b;
    if (!emf_dib_make(&b, 100, 100, WHITE)) { t_skip("no dib"); return; }
    d_two_rects(b.dc);
    EXPECT_EQ(emf_px(&b, 20, 20), RED);
    EXPECT_EQ(emf_px(&b, 70, 70), BLUE);
    EXPECT_EQ(emf_px(&b, 40, 40), BLUE);
    EXPECT_EQ(emf_px(&b, 5, 5), WHITE);
    emf_dib_free(&b);
}

/* ---- enum + PlayEnhMetaFileRecord equals PlayEnhMetaFile ---- */

static int CALLBACK replay_cb(HDC dc, HANDLETABLE *ht, const ENHMETARECORD *r, int n, LPARAM lp) {
    U(PlayEnhMetaFileRecord)(dc, ht, r, n);
    return 1;
}
TEST(emf_play_enum_replay_equals_play) {
    REQUIRE(U(EnumEnhMetaFile)); REQUIRE(U(PlayEnhMetaFileRecord));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    d_two_rects(dc); d_region(dc);
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0); HGDIOBJ o = U(SelectObject)(dc, p);
    U(MoveToEx)(dc, 0, 0, NULL); U(LineTo)(dc, 90, 90);
    U(SelectObject)(dc, o); U(DeleteObject)(p);
    HENHMETAFILE h = emf_end(dc);
    emf_dib a, b;
    if (!h || !emf_dib_make(&a, 100, 100, WHITE) || !emf_dib_make(&b, 100, 100, WHITE)) { t_skip("setup"); return; }
    RECT rc = {0, 0, 100, 100};
    CHECK(U(PlayEnhMetaFile)(a.dc, h, &rc));
    CHECK(U(EnumEnhMetaFile)(b.dc, h, replay_cb, NULL, &rc));
    CHECK(!memcmp(a.bits, b.bits, 100 * 100 * 4));
    emf_dib_free(&a); emf_dib_free(&b); U(DeleteEnhMetaFile)(h);
}

/* ---- persistence: bits roundtrip plays identically ---- */

TEST(emf_play_after_bits_roundtrip) {
    REQUIRE(U(PlayEnhMetaFile)); REQUIRE(U(SetEnhMetaFileBits));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    d_two_rects(dc);
    HENHMETAFILE h = emf_end(dc);
    UINT n; BYTE *bits = emf_bits(h, &n);
    HENHMETAFILE h2 = bits ? U(SetEnhMetaFileBits)(n, bits) : NULL;
    emf_dib a, b;
    if (!h2 || !emf_dib_make(&a, 100, 100, WHITE) || !emf_dib_make(&b, 100, 100, WHITE)) { t_skip("setup"); return; }
    RECT rc = {0, 0, 100, 100};
    U(PlayEnhMetaFile)(a.dc, h, &rc); U(PlayEnhMetaFile)(b.dc, h2, &rc);
    CHECK(!memcmp(a.bits, b.bits, 100 * 100 * 4));
    emf_dib_free(&a); emf_dib_free(&b); free(bits);
    U(DeleteEnhMetaFile)(h); U(DeleteEnhMetaFile)(h2);
}

TEST(emf_play_after_file_roundtrip) {
    REQUIRE(U(PlayEnhMetaFile));
    WCHAR p[MAX_PATH]; emf_tmpname(p, L"pf");
    HDC dc = U(CreateEnhMetaFileW)(NULL, p, NULL, NULL);
    if (!dc) { CHECK(dc != NULL); return; }
    d_two_rects(dc);
    HENHMETAFILE h = emf_end(dc);
    HENHMETAFILE h2 = U(GetEnhMetaFileW)(p);
    emf_dib a, b;
    if (!h2 || !emf_dib_make(&a, 100, 100, WHITE) || !emf_dib_make(&b, 100, 100, WHITE)) { t_skip("setup"); return; }
    RECT rc = {10, 10, 90, 90};
    U(PlayEnhMetaFile)(a.dc, h, &rc); U(PlayEnhMetaFile)(b.dc, h2, &rc);
    CHECK(!memcmp(a.bits, b.bits, 100 * 100 * 4));
    emf_dib_free(&a); emf_dib_free(&b);
    U(DeleteEnhMetaFile)(h); U(DeleteEnhMetaFile)(h2); DeleteFileW(p);
}

/* ---- rendering into various bit depths ---- */

static int dib_depth_play(int bpp, DWORD *out_px) {
    HENHMETAFILE h = rec_rect(0, 0, 10, 10, RGB(255, 0, 0));
    if (!h) return 0;
    struct { BITMAPINFOHEADER h; RGBQUAD pal[256]; } bi; void *bits = NULL;
    memset(&bi, 0, sizeof bi);
    bi.h.biSize = sizeof bi.h; bi.h.biWidth = 32; bi.h.biHeight = -32; bi.h.biPlanes = 1; bi.h.biBitCount = bpp;
    if (bpp <= 8) { bi.h.biClrUsed = 2; bi.pal[0].rgbBlue = bi.pal[0].rgbGreen = bi.pal[0].rgbRed = 255; bi.pal[1].rgbRed = 255; }
    HDC m = U(CreateCompatibleDC)(NULL);
    HBITMAP bm = U(CreateDIBSection)(m, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    int ok = 0;
    if (bm) {
        HGDIOBJ o = U(SelectObject)(m, bm);
        memset(bits, 0, 32 * 32 * 4);
        RECT rc = {0, 0, 32, 32};
        ok = U(PlayEnhMetaFile)(m, h, &rc);
        *out_px = U(GetPixel)(m, 16, 16);
        U(SelectObject)(m, o); U(DeleteObject)(bm);
    }
    U(DeleteDC)(m); U(DeleteEnhMetaFile)(h);
    return ok;
}
TEST(emf_play_into_32bpp) { REQUIRE(U(PlayEnhMetaFile)); DWORD px = 0; CHECK(dib_depth_play(32, &px)); EXPECT_EQ(px, RGB(255, 0, 0)); }
TEST(emf_play_into_24bpp) { REQUIRE(U(PlayEnhMetaFile)); DWORD px = 0; CHECK(dib_depth_play(24, &px)); EXPECT_EQ(px, RGB(255, 0, 0)); }
TEST(emf_play_into_16bpp) { REQUIRE(U(PlayEnhMetaFile)); DWORD px = 0; CHECK(dib_depth_play(16, &px)); EXPECT_EQ(px, RGB(255, 0, 0)); }
TEST(emf_play_into_8bpp) { REQUIRE(U(PlayEnhMetaFile)); DWORD px = 0; CHECK(dib_depth_play(8, &px)); EXPECT_EQ(px, RGB(255, 0, 0)); }
TEST(emf_play_into_1bpp) {
    REQUIRE(U(PlayEnhMetaFile));
    DWORD px = 0; CHECK(dib_depth_play(1, &px));
    /* palette is {white, red}: a red fill must map to the red entry */
    EXPECT_EQ(px, RGB(255, 0, 0));
}

/* ---- DC kinds as play target ---- */

TEST(emf_play_into_memory_dc_default_bitmap) {
    REQUIRE(U(PlayEnhMetaFile));
    HENHMETAFILE h = rec_rect(0, 0, 10, 10, RGB(255, 0, 0));
    HDC m = U(CreateCompatibleDC)(NULL);
    RECT rc = {0, 0, 1, 1};
    CHECK(U(PlayEnhMetaFile)(m, h, &rc));
    U(DeleteDC)(m); U(DeleteEnhMetaFile)(h);
}

TEST(emf_play_into_screen_dc_degenerate_rect_is_safe) {
    REQUIRE(U(PlayEnhMetaFile)); REQUIRE(U(CreateEnhMetaFileW));
    HENHMETAFILE h = rec_rect(0, 0, 10, 10, RGB(255, 0, 0));
    emf_dib d;
    if (!h || !emf_dib_make(&d, 50, 50, WHITE)) { t_skip("setup"); return; }
    RECT rcs[] = { {10, 10, 10, 10}, {10, 10, 10, 20}, {20, 20, 10, 10} };
    for (unsigned i = 0; i < 3; i++) {
        BOOL r = U(PlayEnhMetaFile)(d.dc, h, &rcs[i]);
        t_note("degenerate rect %u -> %d", i, r);
    }
    emf_dib_free(&d); U(DeleteEnhMetaFile)(h);
}

TEST(emf_play_null_rect_pointer) {
    REQUIRE(U(PlayEnhMetaFile));
    HENHMETAFILE h = rec_rect(0, 0, 10, 10, RGB(255, 0, 0));
    emf_dib d;
    if (!h || !emf_dib_make(&d, 50, 50, WHITE)) { t_skip("setup"); return; }
    if (emf_wine()) { t_skip("wine"); emf_dib_free(&d); U(DeleteEnhMetaFile)(h); return; }
    BOOL r = U(PlayEnhMetaFile)(d.dc, h, NULL);
    t_note("PlayEnhMetaFile(rect NULL) = %d", r);
    emf_dib_free(&d); U(DeleteEnhMetaFile)(h);
}

TEST(emf_play_clip_region_on_target_limits_output) {
    REQUIRE(U(PlayEnhMetaFile));
    HENHMETAFILE h = rec_rect(0, 0, 50, 50, RGB(255, 0, 0));
    emf_dib d;
    if (!h || !emf_dib_make(&d, 100, 100, WHITE)) { t_skip("setup"); return; }
    U(IntersectClipRect)(d.dc, 0, 0, 30, 100);
    RECT rc = {0, 0, 100, 100};
    CHECK(U(PlayEnhMetaFile)(d.dc, h, &rc));
    EXPECT_EQ(emf_px(&d, 15, 50), RED);
    EXPECT_EQ(emf_px(&d, 60, 50), WHITE);
    emf_dib_free(&d); U(DeleteEnhMetaFile)(h);
}

TEST(emf_play_bkcolor_state_persists_in_metafile_only) {
    REQUIRE(U(PlayEnhMetaFile));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    U(SetBkColor)(dc, RGB(1, 2, 3));
    HENHMETAFILE h = emf_end(dc);
    emf_dib d; if (!emf_dib_make(&d, 8, 8, 0)) { t_skip("no dib"); return; }
    COLORREF before = U(GetBkColor)(d.dc);
    RECT rc = {0, 0, 8, 8};
    U(PlayEnhMetaFile)(d.dc, h, &rc);
    EXPECT_EQ(U(GetBkColor)(d.dc), before);
    emf_dib_free(&d); U(DeleteEnhMetaFile)(h);
}

TEST(emf_play_text_does_not_crash_and_draws_something) {
    REQUIRE(U(PlayEnhMetaFile));
    HDC dc = emf_begin();
    if (!dc) { CHECK(dc != NULL); return; }
    LOGFONTW lf; memset(&lf, 0, sizeof lf); lf.lfHeight = -24; lstrcpyW(lf.lfFaceName, L"Arial");
    HFONT f = U(CreateFontIndirectW)(&lf); HGDIOBJ o = U(SelectObject)(dc, f);
    U(SetBkMode)(dc, TRANSPARENT); U(SetTextColor)(dc, 0);
    U(TextOutW)(dc, 2, 2, L"WWWW", 4);
    U(SelectObject)(dc, o); U(DeleteObject)(f);
    HENHMETAFILE h = emf_end(dc);
    emf_dib d; if (!h || !emf_dib_make(&d, 120, 50, WHITE)) { t_skip("setup"); return; }
    RECT rc = {0, 0, 120, 50};
    CHECK(U(PlayEnhMetaFile)(d.dc, h, &rc));
    int dark = 0;
    for (int i = 0; i < 120 * 50; i++) if ((d.bits[i] & 0xFFFFFF) != WHITE) dark++;
    CHECKF(dark > 20, "text pixels %d", dark);   /* font rendering is not pixel-checked */
    emf_dib_free(&d); U(DeleteEnhMetaFile)(h);
}
