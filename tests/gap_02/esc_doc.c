/* AbortDocImpl / EndDocImpl / EndPageImpl / EscapeImpl / ExtEscapeImpl / CreateDCAImpl.
 * No printer is ever opened: print-job calls only see memory/display/metafile DCs or junk handles. */
#include "g2.h"

UDECL(int, CreateDCAImpl_dummy, (void));
typedef HDC (WINAPI *CreateDCAImpl_pfn)(LPCSTR, LPCSTR, LPCSTR, const DEVMODEA *);

static HDC (WINAPI *cdca(void))(LPCSTR, LPCSTR, LPCSTR, const DEVMODEA *) {
    FARPROC p = t_proc_gdi32full("CreateDCAImpl");
    if (!p && g2_wine()) p = t_proc("CreateDCA");
    return (void *)p;
}

/* ------------------------------ AbortDoc ------------------------------ */
#define NEED_ABORT() do { SKIP_ON_WINE("print/escape paths differ from Windows"); REQUIRE(IMPL(AbortDoc)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); } while (0)
TEST(gap2_abortdoc_memdc_is_not_printer) {
    NEED_ABORT();
    HDC dc = U(CreateCompatibleDC)(NULL);
    SetLastError(0);
    int r = IMPL(AbortDoc)(dc);
    CHECK(r == -1); EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    U(DeleteDC)(dc);
}
TEST(gap2_abortdoc_junk_handles) {
    NEED_ABORT();
    for (int i = 0; i < G2_NJUNK; i++) {
        SetLastError(0);
        int r = IMPL(AbortDoc)((HDC)g2_junk[i]);
        CHECKF(r == -1, "handle %#llx -> %d", (unsigned long long)g2_junk[i], r);
        EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    }
}
TEST(gap2_abortdoc_stale_dc) {
    NEED_ABORT();
    HDC dc = g2_stale_dc();
    SetLastError(0);
    CHECK(IMPL(AbortDoc)(dc) == -1);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}
TEST(gap2_abortdoc_display_dc) {
    NEED_ABORT(); REQUIRE(cdca());
    HDC dc = cdca()("DISPLAY", NULL, NULL, NULL);
    if (!dc) { t_skip("no display DC"); return; }
    SetLastError(0);
    CHECK(IMPL(AbortDoc)(dc) == -1);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    U(DeleteDC)(dc);
}
TEST(gap2_abortdoc_non_dc_objects) {
    NEED_ABORT();
    HGDIOBJ o[3] = { U(CreateSolidBrush)(0), U(CreateRectRgn)(0, 0, 1, 1), U(CreateBitmap)(1, 1, 1, 1, NULL) };
    for (int i = 0; i < 3; i++) {
        SetLastError(0);
        CHECKF(IMPL(AbortDoc)((HDC)o[i]) == -1, "obj %d", i);
        t_note("AbortDoc(non-DC %d) err %lu", i, GetLastError());
        CHECK(U(GetObjectType)(o[i]) != 0);
        U(DeleteObject)(o[i]);
    }
}
TEST(gap2_abortdoc_wmf_dc) {
    NEED_ABORT(); REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    SKIP_ON_WINE("WMF DC handling differs");
    HDC m = U(CreateMetaFileW)(NULL);
    if (!m) { t_skip("no WMF DC"); return; }
    SetLastError(0);
    CHECK(IMPL(AbortDoc)(m) == -1);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    HMETAFILE h = U(CloseMetaFile)(m); if (h) U(DeleteMetaFile)(h);
}
TEST(gap2_abortdoc_emf_dc_no_document) {
    NEED_ABORT(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    int r = IMPL(AbortDoc)(m);
    t_note("AbortDoc(EMF DC) -> %d", r);
    if (!g2_wine()) CHECK(r == 1);
    /* the metafile DC stays usable */
    CHECK(U(Rectangle)(m, 0, 0, 4, 4));
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    CHECK(h != NULL);
    if (h) U(DeleteEnhMetaFile)(h);
}
TEST(gap2_abortdoc_emf_dc_repeat) {
    NEED_ABORT(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    SKIP_ON_WINE("EMF DC AbortDoc differs");
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    for (int i = 0; i < 5; i++) CHECKF(IMPL(AbortDoc)(m) == 1, "call %d", i);
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    if (h) U(DeleteEnhMetaFile)(h);
}
TEST(gap2_abortdoc_closed_emf_dc) {
    NEED_ABORT(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    SetLastError(0);
    CHECK(IMPL(AbortDoc)(m) == -1);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    if (h) U(DeleteEnhMetaFile)(h);
}
TEST(gap2_abortdoc_equals_public_export) {
    NEED_ABORT(); REQUIRE(U(AbortDoc));
    HDC dc = U(CreateCompatibleDC)(NULL);
    SetLastError(0); int a = U(AbortDoc)(dc); DWORD ea = GetLastError();
    SetLastError(0); int b = IMPL(AbortDoc)(dc); DWORD eb = GetLastError();
    CHECK(a == b); CHECK(ea == eb);
    U(DeleteDC)(dc);
}

/* ------------------------------ EndDoc ------------------------------ */
#define NEED_ENDDOC() do { SKIP_ON_WINE("print/escape paths differ from Windows"); REQUIRE(IMPL(EndDoc)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); } while (0)
TEST(gap2_enddoc_memdc) {
    NEED_ENDDOC();
    HDC dc = U(CreateCompatibleDC)(NULL);
    SetLastError(0);
    int r = IMPL(EndDoc)(dc);
    CHECK(r == -1); EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    U(DeleteDC)(dc);
}
TEST(gap2_enddoc_junk_handles) {
    NEED_ENDDOC();
    for (int i = 0; i < G2_NJUNK; i++) {
        SetLastError(0);
        int r = IMPL(EndDoc)((HDC)g2_junk[i]);
        CHECKF(r == -1, "handle %#llx -> %d", (unsigned long long)g2_junk[i], r);
        EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    }
}
TEST(gap2_enddoc_stale_dc) {
    NEED_ENDDOC();
    SetLastError(0);
    CHECK(IMPL(EndDoc)(g2_stale_dc()) == -1);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}
TEST(gap2_enddoc_display_dc) {
    NEED_ENDDOC(); REQUIRE(cdca());
    HDC dc = cdca()("DISPLAY", NULL, NULL, NULL);
    if (!dc) { t_skip("no display DC"); return; }
    SetLastError(0);
    CHECK(IMPL(EndDoc)(dc) == -1);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    U(DeleteDC)(dc);
}
TEST(gap2_enddoc_emf_dc_no_document) {
    NEED_ENDDOC(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    int r = IMPL(EndDoc)(m);
    t_note("EndDoc(EMF DC) -> %d", r);
    if (!g2_wine()) CHECK(r == 1);
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    CHECK(h != NULL);
    if (h) U(DeleteEnhMetaFile)(h);
}
TEST(gap2_enddoc_closed_emf_dc) {
    NEED_ENDDOC(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    SetLastError(0);
    CHECK(IMPL(EndDoc)(m) == -1);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    if (h) U(DeleteEnhMetaFile)(h);
}
TEST(gap2_enddoc_wmf_dc) {
    NEED_ENDDOC(); REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    SKIP_ON_WINE("WMF DC handling differs");
    HDC m = U(CreateMetaFileW)(NULL);
    if (!m) { t_skip("no WMF DC"); return; }
    SetLastError(0);
    CHECK(IMPL(EndDoc)(m) == -1);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    HMETAFILE h = U(CloseMetaFile)(m); if (h) U(DeleteMetaFile)(h);
}
TEST(gap2_enddoc_equals_public_export) {
    NEED_ENDDOC(); REQUIRE(U(EndDoc));
    HDC dc = U(CreateCompatibleDC)(NULL);
    SetLastError(0); int a = U(EndDoc)(dc); DWORD ea = GetLastError();
    SetLastError(0); int b = IMPL(EndDoc)(dc); DWORD eb = GetLastError();
    CHECK(a == b); CHECK(ea == eb);
    U(DeleteDC)(dc);
}
TEST(gap2_enddoc_after_abortdoc_same_failure) {
    NEED_ENDDOC(); REQUIRE(IMPL(AbortDoc));
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(IMPL(AbortDoc)(dc) == -1);
    SetLastError(0);
    CHECK(IMPL(EndDoc)(dc) == -1);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    U(DeleteDC)(dc);
}

/* ------------------------------ EndPage ------------------------------ */
#define NEED_ENDPAGE() do { SKIP_ON_WINE("print/escape paths differ from Windows"); REQUIRE(IMPL(EndPage)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); } while (0)
TEST(gap2_endpage_memdc) {
    NEED_ENDPAGE();
    HDC dc = U(CreateCompatibleDC)(NULL);
    SetLastError(0);
    int r = IMPL(EndPage)(dc);
    CHECK(r == -1); EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    U(DeleteDC)(dc);
}
TEST(gap2_endpage_junk_handles) {
    NEED_ENDPAGE();
    for (int i = 0; i < G2_NJUNK; i++) {
        SetLastError(0);
        int r = IMPL(EndPage)((HDC)g2_junk[i]);
        CHECKF(r == -1, "handle %#llx -> %d", (unsigned long long)g2_junk[i], r);
        EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    }
}
TEST(gap2_endpage_stale_dc) {
    NEED_ENDPAGE();
    SetLastError(0);
    CHECK(IMPL(EndPage)(g2_stale_dc()) == -1);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}
TEST(gap2_endpage_display_dc) {
    NEED_ENDPAGE(); REQUIRE(cdca());
    HDC dc = cdca()("DISPLAY", NULL, NULL, NULL);
    if (!dc) { t_skip("no display DC"); return; }
    SetLastError(0);
    CHECK(IMPL(EndPage)(dc) == -1);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    U(DeleteDC)(dc);
}
TEST(gap2_endpage_emf_dc_without_page) {
    NEED_ENDPAGE(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    SKIP_ON_WINE("EMF EndPage behaviour differs");
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    SetLastError(0);
    int r = IMPL(EndPage)(m);
    CHECK(r == -1); EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    CHECK(U(Rectangle)(m, 0, 0, 3, 3));
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    CHECK(h != NULL);
    if (h) U(DeleteEnhMetaFile)(h);
}
TEST(gap2_endpage_wmf_dc) {
    NEED_ENDPAGE(); REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    SKIP_ON_WINE("WMF DC handling differs");
    HDC m = U(CreateMetaFileW)(NULL);
    if (!m) { t_skip("no WMF DC"); return; }
    SetLastError(0);
    CHECK(IMPL(EndPage)(m) == -1);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    HMETAFILE h = U(CloseMetaFile)(m); if (h) U(DeleteMetaFile)(h);
}
TEST(gap2_endpage_closed_emf_dc) {
    NEED_ENDPAGE(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    SetLastError(0);
    CHECK(IMPL(EndPage)(m) == -1);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    if (h) U(DeleteEnhMetaFile)(h);
}
TEST(gap2_endpage_equals_public_export) {
    NEED_ENDPAGE(); REQUIRE(U(EndPage));
    HDC dc = U(CreateCompatibleDC)(NULL);
    SetLastError(0); int a = U(EndPage)(dc); DWORD ea = GetLastError();
    SetLastError(0); int b = IMPL(EndPage)(dc); DWORD eb = GetLastError();
    CHECK(a == b); CHECK(ea == eb);
    U(DeleteDC)(dc);
}

/* ------------------------------ Escape ------------------------------ */
#define NEED_ESC() do { SKIP_ON_WINE("print/escape paths differ from Windows"); REQUIRE(IMPL(Escape)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); } while (0)

/* QUERYESCSUPPORT on a plain (non-metafile, non-printer) DC. Returns -1 for codes that are forwarded to the driver. */
static int query_expect_plain(unsigned c) {
    switch (c) {
    case 0x11: case 0x19: case 0x25: case 0x26: case 0x2a: case 0x202: case 0x1000: case 0x1001: case 0x1002:
    case 0x1013: case 0x1014: case 0x1015: case 0x1016: return -1;
    case 0x100: case 0xc01: return 1;
    default: return 0;
    }
}
/* codes for which an altdc (anything that is not a plain DC; also NULL) reports "supported" */
static int query_lab(unsigned c) {
    return (c >= 8 && c <= 14) || c == 1 || c == 2 || c == 3 || c == 5 || c == 0x13 || (c >= 0x100e && c <= 0x1010);
}
TEST(gap2_escape_query_plain_dc_all_codes) {
    NEED_ESC();
    HDC dc = U(CreateCompatibleDC)(NULL);
    int bad = 0, kern = 0;
    for (unsigned c = 0; c <= 0xffff; c++) {
        unsigned short code = (unsigned short)c;
        int r = IMPL(Escape)(dc, QUERYESCSUPPORT, 2, (LPCSTR)&code, NULL);
        int e = query_expect_plain(c);
        if (e < 0) { kern++; continue; }
        if (r != e) { if (bad++ < 8) CHECKF(0, "code %#x -> %d want %d", c, r, e); }
    }
    CHECKF(bad == 0, "%d mismatches", bad);
    t_note("%d codes forwarded to the display driver", kern);
    U(DeleteDC)(dc);
}
TEST(gap2_escape_query_null_dc_quirk) {
    NEED_ESC();
    SKIP_ON_WINE("wine does not special-case NULL DC");
    int bad = 0;
    for (unsigned c = 0; c <= 0x1020; c++) {
        if (query_expect_plain(c) < 0 && !query_lab(c)) { if (c != 0x100 && c != 0xc01) continue; }
        if (c == 0x100 || c == 0xc01) continue;
        unsigned short code = (unsigned short)c;
        int r = IMPL(Escape)(NULL, QUERYESCSUPPORT, 2, (LPCSTR)&code, NULL);
        int e = query_lab(c);
        if (query_expect_plain(c) < 0) continue;
        if (r != e) { if (bad++ < 8) CHECKF(0, "code %#x -> %d want %d", c, r, e); }
    }
    CHECKF(bad == 0, "%d mismatches", bad);
}
TEST(gap2_escape_query_null_input) {
    NEED_ESC();
    HDC dc = U(CreateCompatibleDC)(NULL);
    HDC hs[] = { dc, NULL, (HDC)g2_junk[3] };
    for (int i = 0; i < 3; i++) {
        SetLastError(0);
        int r = IMPL(Escape)(hs[i], QUERYESCSUPPORT, 0, NULL, NULL);
        CHECKF(r == -1, "dc %d -> %d", i, r);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    }
    U(DeleteDC)(dc);
}
TEST(gap2_escape_query_ignores_cbin) {
    NEED_ESC();
    HDC dc = U(CreateCompatibleDC)(NULL);
    unsigned short code = 0x100;
    static const int cbs[] = { 0, 1, 2, 3, 100, -1 };
    for (unsigned i = 0; i < sizeof cbs / sizeof cbs[0]; i++)
        CHECKF(IMPL(Escape)(dc, QUERYESCSUPPORT, cbs[i], (LPCSTR)&code, NULL) == 1, "cbIn %d", cbs[i]);
    U(DeleteDC)(dc);
}
TEST(gap2_escape_query_high_word_ignored) {
    NEED_ESC();
    HDC dc = U(CreateCompatibleDC)(NULL);
    DWORD code = 0xABCD0100;   /* only the low 16 bits are examined */
    CHECK(IMPL(Escape)(dc, QUERYESCSUPPORT, 4, (LPCSTR)&code, NULL) == 1);
    code = 0xFFFF0000 | 0x13;
    CHECK(IMPL(Escape)(dc, QUERYESCSUPPORT, 4, (LPCSTR)&code, NULL) == 0);
    U(DeleteDC)(dc);
}
TEST(gap2_escape_unknown_codes_plain_dc) {
    NEED_ESC();
    HDC dc = U(CreateCompatibleDC)(NULL);
    int bad = 0;
    for (unsigned c = 0; c <= 0xffff; c++) {
        if (c == QUERYESCSUPPORT || c == 5 || c == 0x100 || c == 0xc01) continue;
        int r = IMPL(Escape)(dc, (int)c, 0, NULL, NULL);
        if (r != 0) { if (bad++ < 8) CHECKF(0, "code %#x -> %d", c, r); }
    }
    CHECKF(bad == 0, "%d mismatches", bad);
    int r = IMPL(Escape)(dc, -1, 0, NULL, NULL); CHECK(r == 0);
    r = IMPL(Escape)(dc, INT_MIN, 0, NULL, NULL); CHECK(r == 0);
    U(DeleteDC)(dc);
}
TEST(gap2_escape_print_codes_on_memdc_do_nothing) {
    NEED_ESC();
    HDC dc = U(CreateCompatibleDC)(NULL);
    SetLastError(0x7777);
    CHECK(IMPL(Escape)(dc, NEWFRAME, 0, NULL, NULL) == 0);
    CHECK(IMPL(Escape)(dc, NEXTBAND, 0, NULL, NULL) == 0);
    CHECK(IMPL(Escape)(dc, ABORTDOC, 0, NULL, NULL) == 0);
    CHECK(IMPL(Escape)(dc, ENDDOC, 0, NULL, NULL) == 0);
    CHECK(IMPL(Escape)(dc, STARTDOC, 4, "doc", NULL) == 0);
    CHECK(IMPL(Escape)(dc, SETABORTPROC, 0, NULL, NULL) == 0);
    CHECK(IMPL(Escape)(dc, FLUSHOUTPUT, 0, NULL, NULL) == 0);
    CHECK(IMPL(Escape)(dc, DRAFTMODE, 0, NULL, NULL) == 0);
    CHECK(IMPL(Escape)(dc, SETCOLORTABLE, 0, NULL, NULL) == 0);
    CHECK(IMPL(Escape)(dc, GETPHYSPAGESIZE, 0, NULL, NULL) == 0);
    CHECK(IMPL(Escape)(dc, GETPRINTINGOFFSET, 0, NULL, NULL) == 0);
    EXPECT_ERR(0, 0x7777);
    U(DeleteDC)(dc);
}
TEST(gap2_escape_scaling_factor_zeroed) {
    NEED_ESC();
    SKIP_ON_WINE("wine returns scaling factor differently");
    HDC dc = U(CreateCompatibleDC)(NULL);
    POINT pt = { 7, 7 };
    CHECK(IMPL(Escape)(dc, GETSCALINGFACTOR, 0, NULL, &pt) == 0);
    CHECK(pt.x == 0 && pt.y == 0);
    CHECK(IMPL(Escape)(dc, GETSCALINGFACTOR, 0, NULL, NULL) == 0);
    U(DeleteDC)(dc);
}
TEST(gap2_escape_null_dc_unknown_code) {
    NEED_ESC();
    SKIP_ON_WINE("wine path differs");
    for (int i = 0; i < G2_NJUNK; i++) {
        SetLastError(0);
        int r = IMPL(Escape)((HDC)g2_junk[i], 99, 0, NULL, NULL);
        CHECKF(r == 0, "handle %#llx -> %d", (unsigned long long)g2_junk[i], r);
        EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    }
}
TEST(gap2_escape_null_dc_print_codes) {
    NEED_ESC();
    SKIP_ON_WINE("wine path differs");
    static const int codes[] = { NEWFRAME, NEXTBAND, ABORTDOC, ENDDOC, SETABORTPROC, FLUSHOUTPUT };
    for (unsigned i = 0; i < sizeof codes / sizeof codes[0]; i++) {
        SetLastError(0);
        int r = IMPL(Escape)(NULL, codes[i], 0, NULL, NULL);
        CHECKF(r == 0, "code %d -> %d", codes[i], r);
        EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    }
}
TEST(gap2_escape_getcolortable_missing_input) {
    NEED_ESC();
    SKIP_ON_WINE("wine path differs");
    HDC dc = U(CreateCompatibleDC)(NULL);
    PALETTEENTRY pe;
    HDC hs[] = { dc, NULL };
    for (int i = 0; i < 2; i++) {
        SetLastError(0);
        int r = IMPL(Escape)(hs[i], GETCOLORTABLE, 2, NULL, &pe);
        CHECKF(r == -1, "dc %d -> %d", i, r);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    }
    U(DeleteDC)(dc);
}
TEST(gap2_escape_getcolortable_true_color_dc) {
    NEED_ESC(); REQUIRE(U(GetDeviceCaps));
    G2DC d; if (!g2_mk(&d, 4, 4)) { CHECK(0); return; }
    short idx = 0; PALETTEENTRY pe; memset(&pe, 0x77, sizeof pe);
    int r = IMPL(Escape)(d.dc, GETCOLORTABLE, 2, (LPCSTR)&idx, &pe);
    CHECKF(r == -1, "-> %d", r);
    g2_free(&d);
}
static void esc_c01(int comp, int bpp, int expect_flag) {
    HDC dc = U(CreateCompatibleDC)(NULL);
    BITMAPINFOHEADER h; memset(&h, 0, sizeof h);
    h.biSize = sizeof h; h.biWidth = 1; h.biHeight = 1; h.biPlanes = 1; h.biBitCount = (WORD)bpp; h.biCompression = comp;
    BYTE out[4] = { 0xaa, 0xbb, 0xcc, 0xdd };
    int r = IMPL(Escape)(dc, 0xC01, sizeof h, (LPCSTR)&h, out);
    CHECKF(r == 1, "comp %d bpp %d -> %d", comp, bpp, r);
    CHECKF(out[0] == (expect_flag ? 0x0f : 0) && out[1] == 0 && out[2] == 0 && out[3] == 0,
           "comp %d bpp %d out %02x %02x %02x %02x", comp, bpp, out[0], out[1], out[2], out[3]);
    U(DeleteDC)(dc);
}
TEST(gap2_escape_c01_rgb_formats) {
    NEED_ESC(); SKIP_ON_WINE("0xC01 unsupported");
    static const int bpps[] = { 0, 1, 2, 3, 4, 8, 15, 16, 24, 32, 64 };
    for (unsigned i = 0; i < sizeof bpps / sizeof bpps[0]; i++) {
        int b = bpps[i];
        esc_c01(BI_RGB, b, b == 1 || b == 4 || b == 8 || b == 16 || b == 24 || b == 32);
    }
}
TEST(gap2_escape_c01_rle_formats) {
    NEED_ESC(); SKIP_ON_WINE("0xC01 unsupported");
    static const int bpps[] = { 1, 4, 8, 16, 24, 32 };
    for (unsigned i = 0; i < sizeof bpps / sizeof bpps[0]; i++) {
        esc_c01(BI_RLE8, bpps[i], bpps[i] == 8);
        esc_c01(BI_RLE4, bpps[i], bpps[i] == 4);
    }
}
TEST(gap2_escape_c01_bitfields_and_others) {
    NEED_ESC(); SKIP_ON_WINE("0xC01 unsupported");
    static const int bpps[] = { 1, 4, 8, 16, 24, 32 };
    for (unsigned i = 0; i < sizeof bpps / sizeof bpps[0]; i++) {
        esc_c01(BI_BITFIELDS, bpps[i], bpps[i] == 16 || bpps[i] == 32);
        esc_c01(BI_JPEG, bpps[i], 0);
        esc_c01(BI_PNG, bpps[i], 0);
        esc_c01(99, bpps[i], 0);
        esc_c01(-1, bpps[i], 0);
    }
}
TEST(gap2_escape_c01_null_input) {
    NEED_ESC(); SKIP_ON_WINE("0xC01 unsupported");
    HDC dc = U(CreateCompatibleDC)(NULL);
    BYTE out[4] = { 1, 2, 3, 4 };
    SetLastError(0);
    int r = IMPL(Escape)(dc, 0xC01, 0x28, NULL, out);
    CHECK(r == -1); EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    CHECK(out[0] == 0 && out[1] == 0 && out[2] == 0 && out[3] == 0);
    U(DeleteDC)(dc);
}
TEST(gap2_escape_c01_any_dc_type) {
    NEED_ESC(); SKIP_ON_WINE("0xC01 unsupported");
    /* the answer does not depend on the DC passed in, not even a bogus one */
    BITMAPINFOHEADER h; memset(&h, 0, sizeof h);
    h.biSize = sizeof h; h.biPlanes = 1; h.biBitCount = 24;
    for (int i = 0; i < G2_NJUNK; i++) {
        BYTE out[4] = { 0 };
        int r = IMPL(Escape)((HDC)g2_junk[i], 0xC01, sizeof h, (LPCSTR)&h, out);
        CHECKF(r == 1 && out[0] == 0x0f, "handle %#llx -> %d/%02x", (unsigned long long)g2_junk[i], r, out[0]);
    }
}

/* ------------------------------ ExtEscape ------------------------------ */
#define NEED_EXTESC() do { SKIP_ON_WINE("print/escape paths differ from Windows"); REQUIRE(IMPL(ExtEscape)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); } while (0)
TEST(gap2_extescape_query_pair_kern_always_supported) {
    NEED_EXTESC();
    SKIP_ON_WINE("0x102 handled differently");
    DWORD code = 0x102;   /* GETPAIRKERNTABLE */
    HDC dc = U(CreateCompatibleDC)(NULL);
    HDC hs[] = { dc, NULL, (HDC)1, (HDC)0xdeadbeef };
    for (int i = 0; i < 4; i++)
        CHECKF(IMPL(ExtEscape)(hs[i], QUERYESCSUPPORT, 4, (LPCSTR)&code, 0, NULL) == 1, "dc %d", i);
    U(DeleteDC)(dc);
}
TEST(gap2_extescape_invalid_handle_returns_zero) {
    NEED_EXTESC();
    SKIP_ON_WINE("wine path differs");
    BYTE in[8] = { 0 }, out[8];
    for (int i = 0; i < G2_NJUNK; i++) {
        int r = IMPL(ExtEscape)((HDC)g2_junk[i], 0x7777, sizeof in, (LPCSTR)in, sizeof out, (LPSTR)out);
        CHECKF(r == 0, "handle %#llx -> %d", (unsigned long long)g2_junk[i], r);
    }
}
TEST(gap2_extescape_stale_dc) {
    NEED_EXTESC();
    SKIP_ON_WINE("wine path differs");
    CHECK(IMPL(ExtEscape)(g2_stale_dc(), 0x7777, 0, NULL, 0, NULL) <= 0);
}
TEST(gap2_extescape_unsupported_code_plain_dc) {
    NEED_EXTESC();
    HDC dc = U(CreateCompatibleDC)(NULL);
    BYTE in[4] = { 1, 2, 3, 4 };
    int r = IMPL(ExtEscape)(dc, 0x7ABC, sizeof in, (LPCSTR)in, 0, NULL);
    t_note("ExtEscape(memDC, 0x7ABC) -> %d", r);
    CHECK(r <= 0);
    U(DeleteDC)(dc);
}
TEST(gap2_extescape_query_codes_plain_dc) {
    NEED_EXTESC();
    HDC dc = U(CreateCompatibleDC)(NULL);
    static const DWORD codes[] = { NEWFRAME, NEXTBAND, SETCOLORTABLE, GETPHYSPAGESIZE, 0x0f, 0x7ABC };
    for (unsigned i = 0; i < sizeof codes / sizeof codes[0]; i++) {
        DWORD c = codes[i];
        int r = IMPL(ExtEscape)(dc, QUERYESCSUPPORT, 4, (LPCSTR)&c, 0, NULL);
        t_note("ExtEscape(memDC, QUERYESCSUPPORT, %#lx) -> %d", c, r);
        CHECKF(r == 0 || r == 1, "code %#lx -> %d", c, r);
    }
    U(DeleteDC)(dc);
}
TEST(gap2_extescape_etm_with_truetype_font) {
    NEED_EXTESC(); REQUIRE(U(CreateFontIndirectW)); REQUIRE(U(SelectObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    LOGFONTW lf; memset(&lf, 0, sizeof lf);
    lf.lfHeight = -24; lf.lfCharSet = DEFAULT_CHARSET; lstrcpyW(lf.lfFaceName, L"Arial");
    HFONT f = U(CreateFontIndirectW)(&lf);
    HGDIOBJ old = U(SelectObject)(dc, f);
    BYTE buf[0x40]; memset(buf, 0xEE, sizeof buf);
    int r = IMPL(ExtEscape)(dc, 0x100 /*GETEXTENDEDTEXTMETRIC*/, 4, "\x00\x00\x00\x00", 0x34, (LPSTR)buf);
    t_note("GETEXTENDEDTEXTMETRIC -> %d, emSize=%u", r, *(unsigned short *)buf);
    if (r == 1) {
        CHECK(buf[0x34] == 0xEE);   /* at most 0x34 bytes are written */
        CHECK(*(short *)buf > 0);
    }
    U(SelectObject)(dc, old); U(DeleteObject)(f); U(DeleteDC)(dc);
}
TEST(gap2_extescape_etm_out_limit) {
    NEED_EXTESC(); REQUIRE(U(CreateFontIndirectW)); REQUIRE(U(SelectObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    LOGFONTW lf; memset(&lf, 0, sizeof lf);
    lf.lfHeight = -24; lstrcpyW(lf.lfFaceName, L"Times New Roman");
    HFONT f = U(CreateFontIndirectW)(&lf);
    HGDIOBJ old = U(SelectObject)(dc, f);
    BYTE buf[0x40]; memset(buf, 0xEE, sizeof buf);
    int r = IMPL(ExtEscape)(dc, 0x100, 0, NULL, 0x10, (LPSTR)buf);   /* cbOutput below the structure size is honoured */
    if (r == 1) { CHECK(buf[0x10] == 0xEE); CHECK(buf[0x30] == 0xEE); }
    t_note("ETM with cbOutput=16 -> %d", r);
    U(SelectObject)(dc, old); U(DeleteObject)(f); U(DeleteDC)(dc);
}
TEST(gap2_extescape_emf_dc_unknown_code) {
    NEED_EXTESC(); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC m = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!m) { t_skip("no EMF DC"); return; }
    BYTE in[16] = { 0 };
    int r = IMPL(ExtEscape)(m, 0x7ABC, sizeof in, (LPCSTR)in, 0, NULL);
    t_note("ExtEscape(EMF DC) -> %d", r);
    HENHMETAFILE h = U(CloseEnhMetaFile)(m);
    CHECK(h != NULL);
    if (h) U(DeleteEnhMetaFile)(h);
}

/* ------------------------------ CreateDCAImpl ------------------------------ */
#define NEED_CDC() do { REQUIRE(cdca()); REQUIRE(U(DeleteDC)); REQUIRE(U(GetDeviceCaps)); } while (0)
TEST(gap2_createdca_display) {
    NEED_CDC();
    HDC dc = cdca()("DISPLAY", NULL, NULL, NULL);
    if (!dc) { t_skip("no display"); return; }
    G2_EQ(U(GetObjectType)(dc), OBJ_DC);
    G2_EQ(U(GetDeviceCaps)(dc, TECHNOLOGY), DT_RASDISPLAY);
    CHECK(U(GetDeviceCaps)(dc, HORZRES) > 0 && U(GetDeviceCaps)(dc, VERTRES) > 0);
    CHECK(U(DeleteDC)(dc));
}
TEST(gap2_createdca_display_case_insensitive) {
    NEED_CDC();
    static const char *const n[] = { "display", "Display", "DISPLAY", "dIsPlAy" };
    for (int i = 0; i < 4; i++) {
        HDC dc = cdca()(n[i], NULL, NULL, NULL);
        CHECKF(dc != NULL, "driver '%s'", n[i]);
        if (dc) { G2_EQ(U(GetDeviceCaps)(dc, TECHNOLOGY), DT_RASDISPLAY); U(DeleteDC)(dc); }
    }
}
TEST(gap2_createdca_matches_screen_dc) {
    NEED_CDC(); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC dc = cdca()("DISPLAY", NULL, NULL, NULL);
    if (!dc) { t_skip("no display"); return; }
    HDC s = U(GetDC)(NULL);
    static const int caps[] = { HORZRES, VERTRES, BITSPIXEL, PLANES, LOGPIXELSX, LOGPIXELSY, NUMCOLORS, SIZEPALETTE, RASTERCAPS };
    for (unsigned i = 0; i < sizeof caps / sizeof caps[0]; i++)
        CHECKF(U(GetDeviceCaps)(dc, caps[i]) == U(GetDeviceCaps)(s, caps[i]), "cap %d", caps[i]);
    U(ReleaseDC)(NULL, s);
    U(DeleteDC)(dc);
}
TEST(gap2_createdca_default_objects) {
    NEED_CDC(); REQUIRE(U(GetCurrentObject)); REQUIRE(U(GetStockObject));
    HDC dc = cdca()("DISPLAY", NULL, NULL, NULL);
    if (!dc) { t_skip("no display"); return; }
    G2_EQ(U(GetCurrentObject)(dc, OBJ_PEN), U(GetStockObject)(BLACK_PEN));
    G2_EQ(U(GetCurrentObject)(dc, OBJ_BRUSH), U(GetStockObject)(WHITE_BRUSH));
    G2_EQ(U(GetCurrentObject)(dc, OBJ_FONT), U(GetStockObject)(SYSTEM_FONT));
    U(DeleteDC)(dc);
}
TEST(gap2_createdca_empty_devmode_for_display) {
    NEED_CDC();
    DEVMODEA dm; memset(&dm, 0, sizeof dm);   /* dmDeviceName empty -> ignored for DISPLAY */
    HDC dc = cdca()("DISPLAY", NULL, NULL, &dm);
    CHECK(dc != NULL);
    if (dc) { G2_EQ(U(GetDeviceCaps)(dc, TECHNOLOGY), DT_RASDISPLAY); U(DeleteDC)(dc); }
}
TEST(gap2_createdca_output_ignored_for_display) {
    NEED_CDC();
    HDC dc = cdca()("DISPLAY", NULL, "FILE:", NULL);
    t_note("CreateDCA(DISPLAY, output FILE:) -> %p", dc);
    if (dc) U(DeleteDC)(dc);
}
TEST(gap2_createdca_unknown_driver_fails) {
    NEED_CDC();
    SetLastError(0);
    HDC dc = cdca()("NoSuchDriverGap2", NULL, NULL, NULL);
    CHECK(dc == NULL);
    t_note("unknown driver last error %lu", GetLastError());
    if (dc) U(DeleteDC)(dc);
}
TEST(gap2_createdca_unknown_driver_and_device_fails) {
    NEED_CDC();
    HDC dc = cdca()("NoSuchDriverGap2", "NoSuchDeviceGap2", NULL, NULL);
    CHECK(dc == NULL);
    if (dc) U(DeleteDC)(dc);
}
TEST(gap2_createdca_unknown_display_device) {
    NEED_CDC();
    SKIP_ON_WINE("wine opens any display name");
    HDC dc = cdca()(NULL, "\\\\.\\DISPLAY9999", NULL, NULL);
    CHECK(dc == NULL);
    t_note("DISPLAY9999 last error %lu", GetLastError());
    if (dc) U(DeleteDC)(dc);
}
TEST(gap2_createdca_all_null) {
    NEED_CDC();
    SetLastError(0);
    HDC dc = cdca()(NULL, NULL, NULL, NULL);
    t_note("CreateDCA(NULL x4) -> %p err %lu", dc, GetLastError());
    if (dc) U(DeleteDC)(dc);
}
TEST(gap2_createdca_empty_driver) {
    NEED_CDC();
    HDC dc = cdca()("", NULL, NULL, NULL);
    t_note("CreateDCA(\"\") -> %p", dc);
    if (dc) U(DeleteDC)(dc);
}
TEST(gap2_createdca_primary_device_name) {
    NEED_CDC(); REQUIRE(U(EnumDisplayDevicesA));
    DISPLAY_DEVICEA dd; memset(&dd, 0, sizeof dd); dd.cb = sizeof dd;
    BOOL found = FALSE;
    for (DWORD i = 0; U(EnumDisplayDevicesA)(NULL, i, &dd, 0); i++) {
        if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) { found = TRUE; break; }
        dd.cb = sizeof dd;
    }
    if (!found) { t_skip("no primary display device"); return; }
    HDC dc = cdca()(NULL, dd.DeviceName, NULL, NULL);
    CHECKF(dc != NULL, "device %s", dd.DeviceName);
    if (dc) {
        G2_EQ(U(GetDeviceCaps)(dc, TECHNOLOGY), DT_RASDISPLAY);
        U(DeleteDC)(dc);
    }
    dc = cdca()("DISPLAY", dd.DeviceName, NULL, NULL);
    CHECK(dc != NULL);
    if (dc) U(DeleteDC)(dc);
}
TEST(gap2_createdca_gdi_count) {
    NEED_CDC();
    DWORD base = g2_gdi_count();
    HDC dcs[20]; int n = 0;
    for (int i = 0; i < 20; i++) { dcs[i] = cdca()("DISPLAY", NULL, NULL, NULL); if (dcs[i]) n++; }
    EXPECT_GDI(base + n);
    for (int i = 0; i < 20; i++) if (dcs[i]) CHECK(U(DeleteDC)(dcs[i]));
    EXPECT_GDI(base);
}
TEST(gap2_createdca_distinct_handles) {
    NEED_CDC();
    HDC a = cdca()("DISPLAY", NULL, NULL, NULL), b = cdca()("DISPLAY", NULL, NULL, NULL);
    CHECK(a != NULL && b != NULL && a != b);
    if (a) U(DeleteDC)(a);
    if (b) U(DeleteDC)(b);
}
TEST(gap2_createdca_dc_state_independent) {
    NEED_CDC();
    HDC a = cdca()("DISPLAY", NULL, NULL, NULL), b = cdca()("DISPLAY", NULL, NULL, NULL);
    if (!a || !b) { t_skip("no display"); if (a) U(DeleteDC)(a); if (b) U(DeleteDC)(b); return; }
    U(SetBkColor)(a, RGB(1, 2, 3)); U(SetTextColor)(a, RGB(4, 5, 6)); U(SetMapMode)(a, MM_LOMETRIC);
    CHECK(U(GetBkColor)(b) == RGB(255, 255, 255));
    CHECK(U(GetTextColor)(b) == RGB(0, 0, 0));
    CHECK(U(GetMapMode)(b) == MM_TEXT);
    U(DeleteDC)(a); U(DeleteDC)(b);
}
TEST(gap2_createdca_equals_public_export) {
    NEED_CDC(); REQUIRE(U(CreateDCA));
    HDC a = U(CreateDCA)("DISPLAY", NULL, NULL, NULL), b = cdca()("DISPLAY", NULL, NULL, NULL);
    CHECK((a != NULL) == (b != NULL));
    if (a && b) CHECK(U(GetDeviceCaps)(a, BITSPIXEL) == U(GetDeviceCaps)(b, BITSPIXEL));
    if (a) U(DeleteDC)(a);
    if (b) U(DeleteDC)(b);
    SetLastError(0); a = U(CreateDCA)("NoSuchDriverGap2", NULL, NULL, NULL); DWORD ea = GetLastError();
    SetLastError(0); b = cdca()("NoSuchDriverGap2", NULL, NULL, NULL); DWORD eb = GetLastError();
    CHECK(a == NULL && b == NULL);
    CHECK(ea == eb);
}
TEST(gap2_createdca_wide_parity) {
    NEED_CDC(); REQUIRE(U(CreateDCW));
    HDC a = U(CreateDCW)(L"DISPLAY", NULL, NULL, NULL), b = cdca()("DISPLAY", NULL, NULL, NULL);
    CHECK((a != NULL) == (b != NULL));
    if (a && b) {
        CHECK(U(GetDeviceCaps)(a, HORZRES) == U(GetDeviceCaps)(b, HORZRES));
        CHECK(U(GetDeviceCaps)(a, LOGPIXELSX) == U(GetDeviceCaps)(b, LOGPIXELSX));
    }
    if (a) U(DeleteDC)(a);
    if (b) U(DeleteDC)(b);
}
TEST(gap2_createdca_dc_usable_for_text_metrics) {
    NEED_CDC(); REQUIRE(U(GetTextMetricsA));
    HDC dc = cdca()("DISPLAY", NULL, NULL, NULL);
    if (!dc) { t_skip("no display"); return; }
    TEXTMETRICA tm; memset(&tm, 0, sizeof tm);
    CHECK(U(GetTextMetricsA)(dc, &tm));
    CHECK(tm.tmHeight > 0);
    U(DeleteDC)(dc);
}
