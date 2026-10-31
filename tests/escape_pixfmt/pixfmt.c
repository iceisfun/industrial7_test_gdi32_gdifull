#include "ep.h"

static HWND pf_win(void) { return t_make_window(NULL, WS_OVERLAPPED | WS_CAPTION, 0, 10, 10, 64, 64, NULL); }
static void pf_win_free(HWND w) { if (w) U(DestroyWindow)(w); t_pump(); }
static void pf_fill(PIXELFORMATDESCRIPTOR *p, DWORD flags, BYTE color, BYTE depth) {
    memset(p, 0, sizeof *p);
    p->nSize = sizeof *p; p->nVersion = 1; p->dwFlags = flags; p->iPixelType = PFD_TYPE_RGBA;
    p->cColorBits = color; p->cDepthBits = depth; p->iLayerType = PFD_MAIN_PLANE;
}
static int pf_count(HDC dc) { return U(DescribePixelFormat)(dc, 1, sizeof(PIXELFORMATDESCRIPTOR), NULL); }

/* ---- constants and layout ---- */
TEST(pixfmt_struct_size_40) { CHECK(sizeof(PIXELFORMATDESCRIPTOR) == 40); }
TEST(pixfmt_struct_offsets) {
    CHECK(offsetof(PIXELFORMATDESCRIPTOR, nVersion) == 2);
    CHECK(offsetof(PIXELFORMATDESCRIPTOR, dwFlags) == 4);
    CHECK(offsetof(PIXELFORMATDESCRIPTOR, iPixelType) == 8);
    CHECK(offsetof(PIXELFORMATDESCRIPTOR, cColorBits) == 9);
    CHECK(offsetof(PIXELFORMATDESCRIPTOR, cAlphaBits) == 16);
    CHECK(offsetof(PIXELFORMATDESCRIPTOR, cDepthBits) == 23);
    CHECK(offsetof(PIXELFORMATDESCRIPTOR, iLayerType) == 26);
    CHECK(offsetof(PIXELFORMATDESCRIPTOR, dwLayerMask) == 28);
    CHECK(offsetof(PIXELFORMATDESCRIPTOR, dwDamageMask) == 40 - 4);
}
TEST(pixfmt_flag_values) {
    CHECK(PFD_DOUBLEBUFFER == 1 && PFD_STEREO == 2 && PFD_DRAW_TO_WINDOW == 4 && PFD_DRAW_TO_BITMAP == 8);
    CHECK(PFD_SUPPORT_GDI == 0x10 && PFD_SUPPORT_OPENGL == 0x20 && PFD_GENERIC_FORMAT == 0x40);
    CHECK(PFD_NEED_PALETTE == 0x80 && PFD_NEED_SYSTEM_PALETTE == 0x100);
    CHECK(PFD_SWAP_EXCHANGE == 0x200 && PFD_SWAP_COPY == 0x400 && PFD_SWAP_LAYER_BUFFERS == 0x800);
    CHECK(PFD_GENERIC_ACCELERATED == 0x1000 && PFD_SUPPORT_DIRECTDRAW == 0x2000);
    CHECK((DWORD)PFD_DEPTH_DONTCARE == 0x20000000 && PFD_DOUBLEBUFFER_DONTCARE == 0x40000000 && (DWORD)PFD_STEREO_DONTCARE == 0x80000000u);
    CHECK(PFD_TYPE_RGBA == 0 && PFD_TYPE_COLORINDEX == 1);
    CHECK(PFD_MAIN_PLANE == 0 && PFD_OVERLAY_PLANE == 1 && (BYTE)PFD_UNDERLAY_PLANE == 255);
}
TEST(pixfmt_exports_present) {
    CHECK(UG(ChoosePixelFormat) != NULL);
    CHECK(UG(SetPixelFormat) != NULL);
    CHECK(UG(GetPixelFormat) != NULL);
    CHECK(UG(DescribePixelFormat) != NULL);
    CHECK(UG(SwapBuffers) != NULL);
    CHECK(UF(ChoosePixelFormat) != NULL);
    CHECK(UF(SetPixelFormat) != NULL);
    CHECK(UF(GetPixelFormat) != NULL);
    CHECK(UF(DescribePixelFormat) != NULL);
    CHECK(UF(SwapBuffers) != NULL);
}

/* ---- invalid DC ---- */
#define PF_BADDC(tag, dcexpr) \
TEST(pixfmt_choose_##tag) { REQUIRE(U(ChoosePixelFormat)); PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL, 32, 24); \
    HDC dc = (dcexpr); SET_ERR(0); int r = U(ChoosePixelFormat)(dc, &p); CHECKF(r == 0, "got %d", r); t_note("ChoosePixelFormat err %lu", GetLastError()); } \
TEST(pixfmt_set_##tag) { REQUIRE(U(SetPixelFormat)); PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL, 32, 24); \
    HDC dc = (dcexpr); SET_ERR(0); BOOL r = U(SetPixelFormat)(dc, 1, &p); CHECKF(r == FALSE, "got %d", r); t_note("SetPixelFormat err %lu", GetLastError()); } \
TEST(pixfmt_get_##tag) { REQUIRE(U(GetPixelFormat)); HDC dc = (dcexpr); int r = U(GetPixelFormat)(dc); CHECKF(r == 0, "got %d", r); } \
TEST(pixfmt_describe_##tag) { REQUIRE(U(DescribePixelFormat)); PIXELFORMATDESCRIPTOR p; memset(&p, 0, sizeof p); HDC dc = (dcexpr); \
    int r = U(DescribePixelFormat)(dc, 1, sizeof p, &p); CHECKF(r == 0, "got %d", r); CHECK(p.nSize == 0); \
    r = U(DescribePixelFormat)(dc, 1, sizeof p, NULL); CHECKF(r == 0, "null buf got %d", r); } \
TEST(pixfmt_swap_##tag) { REQUIRE(U(SwapBuffers)); HDC dc = (dcexpr); SET_ERR(0); BOOL r = U(SwapBuffers)(dc); CHECKF(r == FALSE, "got %d", r); t_note("SwapBuffers err %lu", GetLastError()); }
PF_BADDC(nulldc, NULL)
PF_BADDC(bogusdc, EP_BOGUS_DC)
PF_BADDC(bogusdc2, EP_BOGUS_DC2)
PF_BADDC(minus1, (HDC)(LONG_PTR)-1)
PF_BADDC(highword_dc, (HDC)(ULONG_PTR)0x12340000)

TEST(pixfmt_deleted_dc_all) {
    REQUIRE(U(ChoosePixelFormat));
    HDC m = U(CreateCompatibleDC)(NULL); U(DeleteDC)(m);
    PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL, 32, 24);
    CHECK(U(ChoosePixelFormat)(m, &p) == 0);
    CHECK(U(SetPixelFormat)(m, 1, &p) == FALSE);
    CHECK(U(GetPixelFormat)(m) == 0);
    CHECK(U(DescribePixelFormat)(m, 1, sizeof p, &p) == 0);
    CHECK(U(SwapBuffers)(m) == FALSE);
}
TEST(pixfmt_wrong_object_type_handles) {
    REQUIRE(U(ChoosePixelFormat)); REQUIRE(U(CreateSolidBrush));
    HGDIOBJ objs[3];
    objs[0] = U(CreateSolidBrush)(RGB(1, 2, 3));
    objs[1] = U(GetStockObject)(BLACK_PEN);
    objs[2] = U(CreateBitmap)(4, 4, 1, 1, NULL);
    PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL, 32, 24);
    for (int i = 0; i < 3; i++) {
        HDC h = (HDC)objs[i];
        CHECKF(U(ChoosePixelFormat)(h, &p) == 0, "obj %d", i);
        CHECKF(U(SetPixelFormat)(h, 1, &p) == FALSE, "obj %d", i);
        CHECKF(U(GetPixelFormat)(h) == 0, "obj %d", i);
        CHECKF(U(DescribePixelFormat)(h, 1, sizeof p, &p) == 0, "obj %d", i);
        CHECKF(U(SwapBuffers)(h) == FALSE, "obj %d", i);
    }
    U(DeleteObject)(objs[0]); U(DeleteObject)(objs[2]);
}

/* ---- DescribePixelFormat on a window DC ---- */
TEST(pixfmt_describe_count_window_dc) {
    REQUIRE(U(DescribePixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    CHECK(dc != NULL);
    int n = pf_count(dc);
    CHECKF(n >= 1, "count %d", n);
    t_note("pixel formats on window DC: %d", n);
    int n2 = U(DescribePixelFormat)(dc, 1, 0, NULL);
    CHECKF(n2 == n, "nBytes=0 NULL buf count %d vs %d", n2, n);
    int n3 = U(DescribePixelFormat)(dc, n, sizeof(PIXELFORMATDESCRIPTOR), NULL);
    CHECKF(n3 == n, "idx=n count %d vs %d", n3, n);
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_describe_count_screen_dc) {
    REQUIRE(U(DescribePixelFormat));
    HDC dc = ep_scr();
    int n = pf_count(dc);
    CHECKF(n >= 1, "count %d", n);
    ep_scr_free(dc);
}
TEST(pixfmt_describe_count_stable) {
    REQUIRE(U(DescribePixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    int n = pf_count(dc);
    for (int i = 0; i < 20; i++) { int k = pf_count(dc); if (k != n) { CHECKF(0, "iter %d: %d vs %d", i, k, n); break; } }
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_describe_count_two_windows_equal) {
    REQUIRE(U(DescribePixelFormat));
    HWND w1 = pf_win(), w2 = pf_win(); HDC d1 = U(GetDC)(w1), d2 = U(GetDC)(w2);
    CHECK(pf_count(d1) == pf_count(d2));
    U(ReleaseDC)(w1, d1); U(ReleaseDC)(w2, d2); pf_win_free(w1); pf_win_free(w2);
}
TEST(pixfmt_describe_all_formats_wellformed) {
    REQUIRE(U(DescribePixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    int n = pf_count(dc), bad = 0, anywin = 0, anygl = 0, rgba32 = 0, generic = 0;
    if (n > 2000) n = 2000;
    for (int i = 1; i <= n; i++) {
        PIXELFORMATDESCRIPTOR p; memset(&p, 0xCC, sizeof p);
        int r = U(DescribePixelFormat)(dc, i, sizeof p, &p);
        if (r < n) { CHECKF(0, "idx %d returned %d (count %d)", i, r, n); bad++; }
        if (p.nSize != sizeof p || p.nVersion != 1) { CHECKF(0, "idx %d nSize %u nVersion %u", i, p.nSize, p.nVersion); bad++; }
        if (p.iPixelType != PFD_TYPE_RGBA && p.iPixelType != PFD_TYPE_COLORINDEX) { CHECKF(0, "idx %d pixel type %u", i, p.iPixelType); bad++; }
        if (p.iPixelType == PFD_TYPE_RGBA && p.cRedBits + p.cGreenBits + p.cBlueBits > p.cColorBits) { CHECKF(0, "idx %d bits %u+%u+%u > %u", i, p.cRedBits, p.cGreenBits, p.cBlueBits, p.cColorBits); bad++; }
        if (p.cColorBits > 32) { CHECKF(0, "idx %d cColorBits %u", i, p.cColorBits); bad++; }
        if (p.cAlphaBits > 8 || p.cRedBits > 16) { CHECKF(0, "idx %d alpha %u red %u", i, p.cAlphaBits, p.cRedBits); bad++; }
        if (p.dwFlags & PFD_DRAW_TO_WINDOW) anywin++;
        if (p.dwFlags & PFD_SUPPORT_OPENGL) anygl++;
        if (p.dwFlags & PFD_GENERIC_FORMAT) generic++;
        if (p.iPixelType == PFD_TYPE_RGBA && p.cColorBits >= 24 && (p.dwFlags & PFD_DRAW_TO_WINDOW) && (p.dwFlags & PFD_SUPPORT_OPENGL)) rgba32++;
        if (bad > 10) break;
    }
    CHECK(anywin > 0);
    CHECK(anygl > 0);
    CHECK(rgba32 > 0);
    t_note("formats=%d drawToWindow=%d openGL=%d generic=%d usable>=24bpp=%d", n, anywin, anygl, generic, rgba32);
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_describe_invalid_indices) {
    REQUIRE(U(DescribePixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    int n = pf_count(dc);
    PIXELFORMATDESCRIPTOR p;
    int idx[] = { 0, -1, n + 1, n + 100, 0x7fffffff, (int)0x80000000 };
    for (unsigned i = 0; i < sizeof idx / sizeof idx[0]; i++) {
        memset(&p, 0, sizeof p);
        SET_ERR(0);
        int r = U(DescribePixelFormat)(dc, idx[i], sizeof p, &p);
        CHECKF(r == 0, "index %d got %d", idx[i], r);
        CHECKF(p.nSize == 0, "index %d wrote nSize %u", idx[i], p.nSize);
    }
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_describe_nbytes_variants_note) {
    REQUIRE(U(DescribePixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    int n = pf_count(dc);
    static const UINT nb[] = { 0, 1, 4, 39, 40, 41, 44, 100 };
    for (unsigned i = 0; i < sizeof nb / sizeof nb[0]; i++) {
        PIXELFORMATDESCRIPTOR p; memset(&p, 0xCC, sizeof p);
        SET_ERR(0);
        int r = U(DescribePixelFormat)(dc, 1, nb[i], &p);
        t_note("Describe nBytes=%u -> %d (count %d) nSize=%u err=%lu", nb[i], r, n, p.nSize, GetLastError());
        if (nb[i] >= 40) CHECKF(r == n, "nBytes %u got %d", nb[i], r);
    }
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_describe_no_overrun) {
    REQUIRE(U(DescribePixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    struct { PIXELFORMATDESCRIPTOR p; unsigned char guard[16]; } s;
    memset(&s, 0xCC, sizeof s);
    int r = U(DescribePixelFormat)(dc, 1, sizeof s.p, &s.p);
    CHECK(r >= 1);
    int ok = 1; for (int i = 0; i < 16; i++) if (s.guard[i] != 0xCC) ok = 0;
    CHECK(ok);
    CHECK(s.p.nSize == 40);
    memset(&s, 0xCC, sizeof s);
    r = U(DescribePixelFormat)(dc, 1, sizeof s, &s.p);
    CHECK(r >= 1);
    ok = 1; for (int i = 0; i < 16; i++) if (s.guard[i] != 0xCC) ok = 0;
    CHECK(ok);
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_describe_deterministic) {
    REQUIRE(U(DescribePixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    PIXELFORMATDESCRIPTOR a, b;
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    U(DescribePixelFormat)(dc, 1, sizeof a, &a);
    U(DescribePixelFormat)(dc, 1, sizeof b, &b);
    CHECK(!memcmp(&a, &b, sizeof a));
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_describe_last_format_generic_note) {
    REQUIRE(U(DescribePixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    int n = pf_count(dc);
    PIXELFORMATDESCRIPTOR p; memset(&p, 0, sizeof p);
    U(DescribePixelFormat)(dc, n, sizeof p, &p);
    t_note("last format %d: flags=0x%lx color=%u depth=%u stencil=%u", n, p.dwFlags, p.cColorBits, p.cDepthBits, p.cStencilBits);
    CHECK(p.nSize == 40);
    U(ReleaseDC)(w, dc); pf_win_free(w);
}

/* ---- ChoosePixelFormat ---- */
static void choose_check(DWORD flags, BYTE color, BYTE depth, DWORD must_flags) {
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    PIXELFORMATDESCRIPTOR p; pf_fill(&p, flags, color, depth);
    int n = pf_count(dc);
    int f = U(ChoosePixelFormat)(dc, &p);
    CHECKF(f >= 1 && f <= n, "chose %d of %d (flags 0x%lx color %u depth %u)", f, n, flags, color, depth);
    if (f >= 1 && f <= n) {
        PIXELFORMATDESCRIPTOR q; memset(&q, 0, sizeof q);
        U(DescribePixelFormat)(dc, f, sizeof q, &q);
        CHECKF((q.dwFlags & must_flags) == must_flags, "chosen flags 0x%lx lacks 0x%lx", q.dwFlags, must_flags);
        CHECK(q.iPixelType == PFD_TYPE_RGBA);
        t_note("choose(0x%lx,%u,%u) -> %d flags=0x%lx color=%u depth=%u", flags, color, depth, f, q.dwFlags, q.cColorBits, q.cDepthBits);
    }
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_choose_window_gl_double_32) { REQUIRE(U(ChoosePixelFormat)); choose_check(PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, 32, 24, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL); }
TEST(pixfmt_choose_window_gl_double_24) { REQUIRE(U(ChoosePixelFormat)); choose_check(PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, 24, 16, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL); }
TEST(pixfmt_choose_window_gl_single_32) { REQUIRE(U(ChoosePixelFormat)); choose_check(PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL, 32, 24, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL); }
TEST(pixfmt_choose_window_gdi_only) { REQUIRE(U(ChoosePixelFormat)); choose_check(PFD_DRAW_TO_WINDOW | PFD_SUPPORT_GDI, 32, 0, PFD_DRAW_TO_WINDOW); }
TEST(pixfmt_choose_window_depth_dontcare) { REQUIRE(U(ChoosePixelFormat)); choose_check(PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DEPTH_DONTCARE, 32, 0, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL); }
TEST(pixfmt_choose_window_doublebuffer_dontcare) { REQUIRE(U(ChoosePixelFormat)); choose_check(PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER_DONTCARE, 32, 24, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL); }
TEST(pixfmt_choose_window_flags_zero) {
    REQUIRE(U(ChoosePixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    PIXELFORMATDESCRIPTOR p; pf_fill(&p, 0, 32, 24);
    int n = pf_count(dc);
    int f = U(ChoosePixelFormat)(dc, &p);
    CHECKF(f >= 1 && f <= n, "chose %d of %d", f, n);
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_choose_all_zero_pfd_note) {
    REQUIRE(U(ChoosePixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    PIXELFORMATDESCRIPTOR p; memset(&p, 0, sizeof p);
    SET_ERR(0);
    int f = U(ChoosePixelFormat)(dc, &p);
    t_note("ChoosePixelFormat(all-zero pfd) -> %d err %lu (count %d)", f, GetLastError(), pf_count(dc));
    CHECK(f >= 0);
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_choose_huge_color_bits) {
    REQUIRE(U(ChoosePixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL, 255, 255);
    int n = pf_count(dc);
    int f = U(ChoosePixelFormat)(dc, &p);
    /* closest match still returns a valid index */
    CHECKF(f >= 1 && f <= n, "chose %d of %d", f, n);
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_choose_stereo_request_note) {
    REQUIRE(U(ChoosePixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_STEREO, 32, 24);
    int f = U(ChoosePixelFormat)(dc, &p);
    t_note("stereo request -> %d", f);
    CHECK(f >= 0);
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_choose_idempotent) {
    REQUIRE(U(ChoosePixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, 32, 24);
    int a = U(ChoosePixelFormat)(dc, &p);
    for (int i = 0; i < 10; i++) { int b = U(ChoosePixelFormat)(dc, &p); if (b != a) { CHECKF(0, "iter %d: %d vs %d", i, b, a); break; } }
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_choose_does_not_set_format) {
    REQUIRE(U(ChoosePixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL, 32, 24);
    U(ChoosePixelFormat)(dc, &p);
    CHECK(U(GetPixelFormat)(dc) == 0);
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_choose_leaves_pfd_unmodified) {
    REQUIRE(U(ChoosePixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    PIXELFORMATDESCRIPTOR p, c; pf_fill(&p, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, 32, 24);
    c = p;
    U(ChoosePixelFormat)(dc, &p);
    CHECK(!memcmp(&p, &c, sizeof p));
    U(ReleaseDC)(w, dc); pf_win_free(w);
}

/* ---- SetPixelFormat / GetPixelFormat on windows ---- */
TEST(pixfmt_get_unset_window_zero) {
    REQUIRE(U(GetPixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    CHECK(U(GetPixelFormat)(dc) == 0);
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_get_unset_screen_zero) {
    REQUIRE(U(GetPixelFormat));
    HDC dc = ep_scr();
    int r = U(GetPixelFormat)(dc);
    t_note("desktop DC pixel format %d", r);
    CHECK(r >= 0);
    ep_scr_free(dc);
}
TEST(pixfmt_set_get_roundtrip_window) {
    REQUIRE(U(SetPixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, 32, 24);
    int f = U(ChoosePixelFormat)(dc, &p);
    CHECK(f >= 1);
    SET_ERR(0);
    BOOL ok = U(SetPixelFormat)(dc, f, &p);
    CHECKF(ok, "SetPixelFormat(%d) failed err %lu", f, GetLastError());
    CHECK(U(GetPixelFormat)(dc) == f);
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_set_once_per_window_different_format) {
    REQUIRE(U(SetPixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL, 32, 24);
    int f = U(ChoosePixelFormat)(dc, &p);
    int n = pf_count(dc);
    CHECK(U(SetPixelFormat)(dc, f, &p));
    int g = (f == n) ? 1 : n;
    PIXELFORMATDESCRIPTOR q; memset(&q, 0, sizeof q); U(DescribePixelFormat)(dc, g, sizeof q, &q);
    SET_ERR(0);
    BOOL ok = U(SetPixelFormat)(dc, g, &q);
    if (f != g) CHECKF(ok == FALSE, "second SetPixelFormat(%d) succeeded", g);
    t_note("second SetPixelFormat err %lu", GetLastError());
    CHECK(U(GetPixelFormat)(dc) == f);
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_set_twice_same_format_note) {
    REQUIRE(U(SetPixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL, 32, 24);
    int f = U(ChoosePixelFormat)(dc, &p);
    CHECK(U(SetPixelFormat)(dc, f, &p));
    SET_ERR(0);
    BOOL ok = U(SetPixelFormat)(dc, f, &p);
    t_note("second SetPixelFormat(same %d) -> %d err %lu", f, ok, GetLastError());
    CHECK(U(GetPixelFormat)(dc) == f);
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_format_is_per_window_not_per_dc) {
    REQUIRE(U(SetPixelFormat));
    HWND w = pf_win(); HDC d1 = U(GetDC)(w);
    PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL, 32, 24);
    int f = U(ChoosePixelFormat)(d1, &p);
    CHECK(U(SetPixelFormat)(d1, f, &p));
    U(ReleaseDC)(w, d1);
    HDC d2 = U(GetDC)(w);
    int g = U(GetPixelFormat)(d2);
    CHECKF(g == f, "after re-GetDC got %d want %d", g, f);
    U(ReleaseDC)(w, d2); pf_win_free(w);
}
TEST(pixfmt_two_windows_independent) {
    REQUIRE(U(SetPixelFormat));
    HWND w1 = pf_win(), w2 = pf_win(); HDC d1 = U(GetDC)(w1), d2 = U(GetDC)(w2);
    PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL, 32, 24);
    int f = U(ChoosePixelFormat)(d1, &p);
    CHECK(U(SetPixelFormat)(d1, f, &p));
    CHECK(U(GetPixelFormat)(d2) == 0);
    CHECK(U(SetPixelFormat)(d2, f, &p));
    CHECK(U(GetPixelFormat)(d2) == f);
    U(ReleaseDC)(w1, d1); U(ReleaseDC)(w2, d2); pf_win_free(w1); pf_win_free(w2);
}
TEST(pixfmt_set_invalid_index_then_valid) {
    REQUIRE(U(SetPixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL, 32, 24);
    int n = pf_count(dc);
    int bad[] = { 0, -1, n + 1, 0x7fffffff };
    for (unsigned i = 0; i < 4; i++) {
        SET_ERR(0);
        BOOL ok = U(SetPixelFormat)(dc, bad[i], &p);
        CHECKF(ok == FALSE, "index %d succeeded", bad[i]);
        CHECKF(U(GetPixelFormat)(dc) == 0, "format set after failing index %d", bad[i]);
    }
    int f = U(ChoosePixelFormat)(dc, &p);
    CHECK(U(SetPixelFormat)(dc, f, &p));
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_set_null_pfd_note) {
    REQUIRE(U(SetPixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    SET_ERR(0);
    BOOL ok = U(SetPixelFormat)(dc, 1, NULL);
    t_note("SetPixelFormat(idx 1, NULL pfd) -> %d err %lu", ok, GetLastError());
    CHECK(ok == TRUE || ok == FALSE);
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_set_ignores_pfd_contents_note) {
    REQUIRE(U(SetPixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    PIXELFORMATDESCRIPTOR p; memset(&p, 0, sizeof p);
    BOOL ok = U(SetPixelFormat)(dc, 1, &p);
    t_note("SetPixelFormat(1, zeroed pfd) -> %d, GetPixelFormat %d", ok, U(GetPixelFormat)(dc));
    if (ok) CHECK(U(GetPixelFormat)(dc) == 1);
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_set_after_window_destroyed_dc) {
    REQUIRE(U(SetPixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    U(ReleaseDC)(w, dc);
    PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL, 32, 24);
    SET_ERR(0);
    BOOL ok = U(SetPixelFormat)(dc, 1, &p);
    CHECKF(ok == FALSE, "released DC accepted");
    pf_win_free(w);
}
TEST(pixfmt_getdcex_window_dc_set_format) {
    REQUIRE(U(SetPixelFormat)); REQUIRE(U(GetWindowDC));
    HWND w = pf_win(); HDC dc = U(GetWindowDC)(w);
    CHECK(dc != NULL);
    PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL, 32, 24);
    int f = U(ChoosePixelFormat)(dc, &p);
    BOOL ok = U(SetPixelFormat)(dc, f, &p);
    t_note("SetPixelFormat on GetWindowDC -> %d", ok);
    if (ok) CHECK(U(GetPixelFormat)(dc) == f);
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
static LRESULT CALLBACK pf_wndproc(HWND h, UINT m, WPARAM w, LPARAM l) { return U(DefWindowProcA)(h, m, w, l); }
TEST(pixfmt_owndc_class_window) {
    REQUIRE(U(SetPixelFormat)); REQUIRE(U(RegisterClassA));
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = pf_wndproc; wc.hInstance = GetModuleHandleA(NULL);
    wc.style = CS_OWNDC; wc.lpszClassName = "EPPixfmtOwnDC";
    ATOM at = U(RegisterClassA)(&wc);
    CHECKF(at != 0, "RegisterClassA err %lu", GetLastError());
    HWND w = t_make_window("EPPixfmtOwnDC", WS_OVERLAPPED, 0, 0, 0, 32, 32, NULL);
    CHECKF(w != NULL, "CreateWindow err %lu", GetLastError());
    if (w) {
        HDC dc = U(GetDC)(w);
        PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL, 32, 24);
        int f = U(ChoosePixelFormat)(dc, &p);
        CHECK(U(SetPixelFormat)(dc, f, &p));
        HDC dc2 = U(GetDC)(w);
        CHECK(dc2 == dc);
        CHECK(U(GetPixelFormat)(dc2) == f);
        U(ReleaseDC)(w, dc2); U(ReleaseDC)(w, dc);
        pf_win_free(w);
    }
    U(UnregisterClassA)("EPPixfmtOwnDC", GetModuleHandleA(NULL));
}

/* ---- SwapBuffers ---- */
TEST(pixfmt_swap_window_no_format_note) {
    REQUIRE(U(SwapBuffers));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    SET_ERR(0);
    BOOL r = U(SwapBuffers)(dc);
    t_note("SwapBuffers without pixel format -> %d err %lu", r, GetLastError());
    CHECK(r == TRUE || r == FALSE);
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_swap_single_buffered_note) {
    REQUIRE(U(SwapBuffers));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL, 32, 24);
    int f = U(ChoosePixelFormat)(dc, &p);
    PIXELFORMATDESCRIPTOR q; memset(&q, 0, sizeof q); U(DescribePixelFormat)(dc, f, sizeof q, &q);
    CHECK(U(SetPixelFormat)(dc, f, &p));
    SET_ERR(0);
    BOOL r = U(SwapBuffers)(dc);
    t_note("SwapBuffers on %s-buffered format %d -> %d err %lu", (q.dwFlags & PFD_DOUBLEBUFFER) ? "double" : "single", f, r, GetLastError());
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_swap_double_buffered_window) {
    REQUIRE(U(SwapBuffers));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, 32, 24);
    int f = U(ChoosePixelFormat)(dc, &p);
    PIXELFORMATDESCRIPTOR q; memset(&q, 0, sizeof q); U(DescribePixelFormat)(dc, f, sizeof q, &q);
    if (!(q.dwFlags & PFD_DOUBLEBUFFER)) { t_note("no double-buffered format available"); }
    else {
        CHECK(U(SetPixelFormat)(dc, f, &p));
        SET_ERR(0);
        BOOL r = U(SwapBuffers)(dc);
        t_note("SwapBuffers double-buffered window (no GL context) -> %d err %lu", r, GetLastError());
        CHECK(r == TRUE || r == FALSE);
    }
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_swap_screen_dc_note) {
    REQUIRE(U(SwapBuffers));
    HDC dc = ep_scr();
    SET_ERR(0);
    BOOL r = U(SwapBuffers)(dc);
    t_note("SwapBuffers(desktop DC) -> %d err %lu", r, GetLastError());
    ep_scr_free(dc);
}

/* ---- memory DCs ---- */
TEST(pixfmt_mem_dc_default_bitmap_note) {
    REQUIRE(U(DescribePixelFormat));
    HDC m = U(CreateCompatibleDC)(NULL);
    int n = pf_count(m);
    t_note("formats on default mem DC: %d", n);
    CHECK(n >= 0);
    PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_BITMAP | PFD_SUPPORT_GDI, 1, 0);
    int f = U(ChoosePixelFormat)(m, &p);
    t_note("choose on 1bpp mem DC -> %d", f);
    CHECK(U(GetPixelFormat)(m) == 0);
    U(DeleteDC)(m);
}
TEST(pixfmt_mem_dc_dibsection_describe) {
    REQUIRE(U(DescribePixelFormat));
    HBITMAP b, o; HDC m = ep_memdc32(16, 16, &b, &o, NULL);
    CHECK(m != NULL);
    if (m) {
        int n = pf_count(m);
        t_note("formats on 32bpp DIB mem DC: %d", n);
        int bitmap_ok = 0;
        for (int i = 1; i <= n && i < 500; i++) {
            PIXELFORMATDESCRIPTOR p; memset(&p, 0, sizeof p);
            U(DescribePixelFormat)(m, i, sizeof p, &p);
            if (p.dwFlags & PFD_DRAW_TO_BITMAP) bitmap_ok++;
            CHECKF(p.nSize == 40 && p.nVersion == 1, "idx %d", i);
        }
        CHECKF(n < 1 || bitmap_ok > 0, "no DRAW_TO_BITMAP format among %d", n);
        ep_memdc_free(m, b, o);
    }
}
TEST(pixfmt_mem_dc_dibsection_choose_set) {
    REQUIRE(U(SetPixelFormat));
    HBITMAP b, o; HDC m = ep_memdc32(16, 16, &b, &o, NULL);
    CHECK(m != NULL);
    if (m) {
        PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL | PFD_SUPPORT_GDI, 32, 24);
        int f = U(ChoosePixelFormat)(m, &p);
        t_note("choose(DRAW_TO_BITMAP|GL|GDI, 32) on DIB DC -> %d", f);
        if (f > 0) {
            PIXELFORMATDESCRIPTOR q; memset(&q, 0, sizeof q); U(DescribePixelFormat)(m, f, sizeof q, &q);
            CHECK(q.dwFlags & PFD_DRAW_TO_BITMAP);
            SET_ERR(0);
            BOOL ok = U(SetPixelFormat)(m, f, &p);
            t_note("SetPixelFormat(mem) -> %d err %lu", ok, GetLastError());
            if (ok) {
                CHECK(U(GetPixelFormat)(m) == f);
                SET_ERR(0);
                BOOL again = U(SetPixelFormat)(m, f, &p);
                t_note("second SetPixelFormat(mem) -> %d", again);
            }
        }
        ep_memdc_free(m, b, o);
    }
}
TEST(pixfmt_mem_dc_get_unset_zero) {
    REQUIRE(U(GetPixelFormat));
    HBITMAP b, o; HDC m = ep_memdc32(8, 8, &b, &o, NULL);
    if (m) { CHECK(U(GetPixelFormat)(m) == 0); ep_memdc_free(m, b, o); }
}
TEST(pixfmt_mem_dc_swap_note) {
    REQUIRE(U(SwapBuffers));
    HBITMAP b, o; HDC m = ep_memdc32(8, 8, &b, &o, NULL);
    CHECK(m != NULL);
    if (m) {
        SET_ERR(0);
        BOOL r = U(SwapBuffers)(m);
        t_note("SwapBuffers(mem DC, no format) -> %d err %lu", r, GetLastError());
        ep_memdc_free(m, b, o);
    }
}
TEST(pixfmt_mem_dc_swap_after_set_note) {
    REQUIRE(U(SwapBuffers));
    HBITMAP b, o; HDC m = ep_memdc32(8, 8, &b, &o, NULL);
    if (m) {
        PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_BITMAP | PFD_SUPPORT_GDI | PFD_SUPPORT_OPENGL, 32, 24);
        int f = U(ChoosePixelFormat)(m, &p);
        if (f > 0 && U(SetPixelFormat)(m, f, &p)) {
            SET_ERR(0);
            BOOL r = U(SwapBuffers)(m);
            t_note("SwapBuffers(mem DC, single-buffered fmt %d) -> %d err %lu", f, r, GetLastError());
        }
        ep_memdc_free(m, b, o);
    }
}
TEST(pixfmt_mem_dc_set_window_format_rejected_note) {
    REQUIRE(U(SetPixelFormat));
    HWND w = pf_win(); HDC wd = U(GetDC)(w);
    PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL, 32, 24);
    int f = U(ChoosePixelFormat)(wd, &p);
    HBITMAP b, o; HDC m = ep_memdc32(8, 8, &b, &o, NULL);
    if (m) {
        SET_ERR(0);
        BOOL ok = U(SetPixelFormat)(m, f, &p);
        t_note("SetPixelFormat(mem DC, window-only format %d) -> %d err %lu", f, ok, GetLastError());
        ep_memdc_free(m, b, o);
    }
    U(ReleaseDC)(w, wd); pf_win_free(w);
}

/* ---- resources ---- */
TEST(pixfmt_gdi_balance_window_cycle) {
    REQUIRE(U(SetPixelFormat)); REQUIRE(U(GetGuiResources));
    DWORD before = ep_gui();
    for (int i = 0; i < 20; i++) {
        HWND w = pf_win(); HDC dc = U(GetDC)(w);
        PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, 32, 24);
        int f = U(ChoosePixelFormat)(dc, &p);
        U(SetPixelFormat)(dc, f, &p);
        U(GetPixelFormat)(dc);
        U(ReleaseDC)(w, dc); pf_win_free(w);
    }
    DWORD after = ep_gui();
    CHECKF(after == before, "before %lu after %lu", before, after);
}
TEST(pixfmt_gdi_balance_describe_loop) {
    REQUIRE(U(DescribePixelFormat)); REQUIRE(U(GetGuiResources));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    DWORD before = ep_gui();
    for (int i = 0; i < 200; i++) pf_count(dc);
    CHECK(ep_gui() == before);
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_repeated_choose_memdc_cycles) {
    REQUIRE(U(ChoosePixelFormat)); REQUIRE(U(GetGuiResources));
    DWORD before = ep_gui();
    for (int i = 0; i < 20; i++) {
        HBITMAP b, o; HDC m = ep_memdc32(4, 4, &b, &o, NULL);
        if (!m) break;
        PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_BITMAP | PFD_SUPPORT_GDI, 32, 0);
        U(ChoosePixelFormat)(m, &p);
        ep_memdc_free(m, b, o);
    }
    CHECK(ep_gui() == before);
}

TEST(pixfmt_describe_reserved_and_layers) {
    REQUIRE(U(DescribePixelFormat));
    HWND w = pf_win(); HDC dc = U(GetDC)(w);
    int n = pf_count(dc); if (n > 500) n = 500;
    for (int i = 1; i <= n; i++) {
        PIXELFORMATDESCRIPTOR p; memset(&p, 0xCC, sizeof p);
        U(DescribePixelFormat)(dc, i, sizeof p, &p);
        if (p.nSize != sizeof p) continue;
        if (p.iLayerType != PFD_MAIN_PLANE) { CHECKF(0, "idx %d layer type %u", i, p.iLayerType); break; }
        if (p.bReserved != 0 && p.bReserved != 0xCC) { /* overlay/underlay counts are legal */ }
        if (p.iPixelType == PFD_TYPE_RGBA) {
            if (p.cRedShift + p.cRedBits > 32 || p.cGreenShift + p.cGreenBits > 32 || p.cBlueShift + p.cBlueBits > 32) { CHECKF(0, "idx %d shifts out of range", i); break; }
        }
        if (p.cAccumBits < p.cAccumRedBits || p.cAccumBits < p.cAccumAlphaBits) { CHECKF(0, "idx %d accum %u < %u/%u", i, p.cAccumBits, p.cAccumRedBits, p.cAccumAlphaBits); break; }
        if (p.cDepthBits > 32 || p.cStencilBits > 16) { CHECKF(0, "idx %d depth %u stencil %u", i, p.cDepthBits, p.cStencilBits); break; }
    }
    U(ReleaseDC)(w, dc); pf_win_free(w);
}
TEST(pixfmt_mem_dib32_generic_bitmap_format) {
    REQUIRE(U(ChoosePixelFormat)); REQUIRE(U(SetPixelFormat)); REQUIRE(U(GetPixelFormat));
    if (ep_is_wine()) { t_skip("wine: generic memory DC formats differ"); return; }
    HBITMAP b, o; HDC m = ep_memdc32(16, 16, &b, &o, NULL);
    CHECK(m != NULL);
    if (m) {
        PIXELFORMATDESCRIPTOR p; pf_fill(&p, PFD_DRAW_TO_BITMAP | PFD_SUPPORT_GDI | PFD_SUPPORT_OPENGL, 32, 24);
        int f = U(ChoosePixelFormat)(m, &p);
        CHECKF(f >= 1, "choose returned %d", f);
        if (f >= 1) {
            PIXELFORMATDESCRIPTOR q; memset(&q, 0, sizeof q);
            CHECK(U(DescribePixelFormat)(m, f, sizeof q, &q) >= f);
            CHECK(q.dwFlags & PFD_DRAW_TO_BITMAP);
            CHECK(q.dwFlags & PFD_SUPPORT_GDI);
            CHECK(U(GetPixelFormat)(m) == 0);
            BOOL ok = U(SetPixelFormat)(m, f, &q);
            CHECKF(ok, "SetPixelFormat on memory DC failed err %lu", GetLastError());
            if (ok) CHECK(U(GetPixelFormat)(m) == f);
        }
        ep_memdc_free(m, b, o);
    }
}
TEST(pixfmt_describe_null_buffer_with_nbytes_zero_on_memdc) {
    REQUIRE(U(DescribePixelFormat));
    HBITMAP b, o; HDC m = ep_memdc32(8, 8, &b, &o, NULL);
    CHECK(m != NULL);
    if (m) {
        int a = U(DescribePixelFormat)(m, 1, 0, NULL);
        int c = U(DescribePixelFormat)(m, 1, sizeof(PIXELFORMATDESCRIPTOR), NULL);
        CHECKF(a == c, "nBytes 0: %d vs %d", a, c);
        ep_memdc_free(m, b, o);
    }
}
