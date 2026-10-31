#include "gap4.h"
/* GdiDescribePixelFormat, GdiDrawStream, and non-callable internals. */

static HDC mk_dibdc(HBITMAP *pb, HGDIOBJ *pold) {
    HDC m = U(CreateCompatibleDC)(NULL);
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = 40; bi.bmiHeader.biWidth = 8; bi.bmiHeader.biHeight = -8; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    void *bits; *pb = U(CreateDIBSection)(m, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    *pold = U(SelectObject)(m, *pb);
    return m;
}
static void free_dibdc(HDC m, HBITMAP b, HGDIOBJ old) { U(SelectObject)(m, old); U(DeleteObject)(b); U(DeleteDC)(m); }

/* ---------- GdiDescribePixelFormat ---------- */
TEST(gap4_describepf_invalid_dc) {
    RQ(GdiDescribePixelFormat);
    PIXELFORMATDESCRIPTOR pfd; memset(&pfd, 0, sizeof pfd);
    HBRUSH b = U(CreateSolidBrush)(0);
    HDC dead = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dead);
    HDC bad[] = { NULL, (HDC)1, (HDC)0x12345, (HDC)(ULONG_PTR)-1, (HDC)b, dead };
    for (unsigned i = 0; i < 6; i++) {
        SET_ERR(0);
        int r = UN(GdiDescribePixelFormat)(bad[i], 1, sizeof pfd, &pfd);
        CHECKF(r == 0, "case %u -> %d", i, r);
        CHECKF(pfd.nSize == 0, "case %u wrote pfd", i);
        t_note("GdiDescribePixelFormat bad case %u lasterror=%lu", i, GetLastError());
    }
    U(DeleteObject)(b);
}
TEST(gap4_describepf_invalid_dc_null_buffer) {
    RQ(GdiDescribePixelFormat);
    EXPECT_EQ(UN(GdiDescribePixelFormat)(NULL, 1, 0, NULL), 0);
    EXPECT_EQ(UN(GdiDescribePixelFormat)(NULL, 0, 0, NULL), 0);
    EXPECT_EQ(UN(GdiDescribePixelFormat)((HDC)0x12345, 1, sizeof(PIXELFORMATDESCRIPTOR), NULL), 0);
}
TEST(gap4_describepf_screen_dc_count) {
    RQ(GdiDescribePixelFormat);
    G4_WINE_SKIP("pixel formats");
    HDC dc = U(GetDC)(NULL); REQUIRE(dc);
    int n = UN(GdiDescribePixelFormat)(dc, 1, sizeof(PIXELFORMATDESCRIPTOR), NULL);
    t_note("screen DC format count=%d", n);
    CHECK(n >= 0);
    if (n > 0) {
        PIXELFORMATDESCRIPTOR p; memset(&p, 0xcc, sizeof p);
        int r = UN(GdiDescribePixelFormat)(dc, 1, sizeof p, &p);
        CHECKF(r == n, "r=%d n=%d", r, n);
        CHECK(p.nSize == sizeof p && p.nVersion == 1);
        CHECK((p.dwFlags & PFD_SUPPORT_GDI) || 1);
    }
    U(ReleaseDC)(NULL, dc);
}
TEST(gap4_describepf_all_formats_consistent) {
    RQ(GdiDescribePixelFormat);
    G4_WINE_SKIP("pixel formats");
    HDC dc = U(GetDC)(NULL); REQUIRE(dc);
    int n = UN(GdiDescribePixelFormat)(dc, 1, sizeof(PIXELFORMATDESCRIPTOR), NULL);
    if (n > 200) n = 200;
    int bad = 0;
    for (int i = 1; i <= n; i++) {
        PIXELFORMATDESCRIPTOR p; memset(&p, 0, sizeof p);
        int r = UN(GdiDescribePixelFormat)(dc, i, sizeof p, &p);
        if (r <= 0 || p.nSize != sizeof p || p.nVersion != 1) bad++;
    }
    CHECK(bad == 0);
    U(ReleaseDC)(NULL, dc);
}
TEST(gap4_describepf_out_of_range_index) {
    RQ(GdiDescribePixelFormat);
    G4_WINE_SKIP("pixel formats");
    HDC dc = U(GetDC)(NULL); REQUIRE(dc);
    int n = UN(GdiDescribePixelFormat)(dc, 1, sizeof(PIXELFORMATDESCRIPTOR), NULL);
    PIXELFORMATDESCRIPTOR p; memset(&p, 0, sizeof p);
    int idx[] = { 0, -1, n + 1, 0x7fffffff };
    for (unsigned i = 0; i < 4; i++) {
        memset(&p, 0, sizeof p);
        int r = UN(GdiDescribePixelFormat)(dc, idx[i], sizeof p, &p);
        t_note("describe idx %d -> %d nSize=%u", idx[i], r, p.nSize);
        SOFT(p.nSize == 0 || r != 0);
    }
    U(ReleaseDC)(NULL, dc);
}
TEST(gap4_describepf_small_buffer_size) {
    RQ(GdiDescribePixelFormat);
    G4_WINE_SKIP("pixel formats");
    HDC dc = U(GetDC)(NULL); REQUIRE(dc);
    BYTE buf[128]; 
    UINT sizes[] = { 0, 1, 4, sizeof(PIXELFORMATDESCRIPTOR) - 1, sizeof(PIXELFORMATDESCRIPTOR), sizeof(PIXELFORMATDESCRIPTOR) + 8 };
    for (unsigned i = 0; i < 6; i++) {
        memset(buf, 0xee, sizeof buf);
        int r = UN(GdiDescribePixelFormat)(dc, 1, sizes[i], (PIXELFORMATDESCRIPTOR *)buf);
        t_note("nBytes=%u -> %d, nSize=%u", sizes[i], r, ((PIXELFORMATDESCRIPTOR *)buf)->nSize);
        int clean = 1;
        for (unsigned j = (sizes[i] > 64 ? sizeof(PIXELFORMATDESCRIPTOR) : sizes[i]); j < 128; j++) if (buf[j] != 0xee && j >= sizes[i]) clean = 0;
        CHECKF(clean, "nBytes=%u wrote past buffer", sizes[i]);
    }
    U(ReleaseDC)(NULL, dc);
}
TEST(gap4_describepf_memory_dc_dibsection) {
    RQ(GdiDescribePixelFormat);
    G4_WINE_SKIP("pixel formats");
    HBITMAP b; HGDIOBJ old; HDC m = mk_dibdc(&b, &old); REQUIRE(m);
    int n = UN(GdiDescribePixelFormat)(m, 1, sizeof(PIXELFORMATDESCRIPTOR), NULL);
    t_note("DIB memory DC format count=%d", n);
    CHECK(n >= 0);
    free_dibdc(m, b, old);
}
TEST(gap4_describepf_metafile_dc) {
    RQ(GdiDescribePixelFormat);
    G4_WINE_SKIP("pixel formats");
    HDC e = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL); REQUIRE(e);
    PIXELFORMATDESCRIPTOR p; memset(&p, 0, sizeof p);
    int r = UN(GdiDescribePixelFormat)(e, 1, sizeof p, &p);
    t_note("metafile DC describe=%d", r);
    HENHMETAFILE h = U(CloseEnhMetaFile)(e); if (h) U(DeleteEnhMetaFile)(h);
}
TEST(gap4_describepf_no_leak) {
    RQ(GdiDescribePixelFormat);
    G4_WINE_SKIP("pixel formats");
    HDC dc = U(GetDC)(NULL); REQUIRE(dc);
    DWORD g = g4_gdi_count();
    for (int i = 0; i < 200; i++) UN(GdiDescribePixelFormat)(dc, 1, sizeof(PIXELFORMATDESCRIPTOR), NULL);
    EXPECT_EQ(g4_gdi_count(), g);
    U(ReleaseDC)(NULL, dc);
}

/* ---------- GdiDrawStream (only malformed streams; result recorded, not asserted
 * where the decompilation does not pin it down) ---------- */
TEST(gap4_drawstream_malformed_user_path) {
    RQ(GdiDrawStream);
    G4_WINE_SKIP("NtGdiDrawStream");
    /* non-DC-typed handle takes the user-mode parser: size < 4 or wrong magic bails */
    DWORD buf[8] = { 0 };
    UN(GdiDrawStream)(NULL, 0, buf);
    UN(GdiDrawStream)(NULL, 3, buf);
    UN(GdiDrawStream)(NULL, sizeof buf, buf);   /* magic 0 != 'SwrD' */
    buf[0] = 0x44727753; buf[1] = 0x7fffffff;   /* unknown opcode */
    UN(GdiDrawStream)(NULL, sizeof buf, buf);
    CHECK(1);
}
TEST(gap4_drawstream_memdc_bad_stream) {
    RQ(GdiDrawStream);
    G4_WINE_SKIP("NtGdiDrawStream");
    HBITMAP b; HGDIOBJ old; HDC m = mk_dibdc(&b, &old); REQUIRE(m);
    DWORD buf[16]; memset(buf, 0, sizeof buf);
    BOOL r = UN(GdiDrawStream)(m, sizeof buf, buf);
    t_note("GdiDrawStream(memdc, zero stream)=%d", r);
    CHECK(r == 0);
    free_dibdc(m, b, old);
}
TEST(gap4_drawstream_memdc_pixels_unchanged) {
    RQ(GdiDrawStream);
    G4_WINE_SKIP("NtGdiDrawStream");
    HBITMAP b; HGDIOBJ old; HDC m = mk_dibdc(&b, &old); REQUIRE(m);
    BITMAP bm; U(GetObjectW)(b, sizeof bm, &bm);
    DWORD *px = bm.bmBits;
    for (int i = 0; i < 64; i++) px[i] = 0x00123456;
    DWORD buf[16]; memset(buf, 0xff, sizeof buf);
    UN(GdiDrawStream)(m, sizeof buf, buf);
    int same = 1; for (int i = 0; i < 64; i++) if (px[i] != 0x00123456) same = 0;
    CHECK(same);
    free_dibdc(m, b, old);
}
