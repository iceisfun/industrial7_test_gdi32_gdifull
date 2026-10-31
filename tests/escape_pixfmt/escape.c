#include "ep.h"

/* ---- Escape ---- */
TEST(escape_null_in_queryesc_screen) {
    EP_WINE_SKIP("null input pointer dereferenced");
    REQUIRE(U(Escape));
    HDC d = ep_scr();
    SET_ERR(0);
    int r = U(Escape)(d, QUERYESCSUPPORT, sizeof(int), NULL, NULL);
    CHECKF(r == -1, "got %d", r);
    EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    ep_scr_free(d);
}
TEST(escape_null_in_queryesc_mem) {
    EP_WINE_SKIP("null input pointer dereferenced");
    REQUIRE(U(Escape)); REQUIRE(U(CreateCompatibleDC));
    HDC m = U(CreateCompatibleDC)(NULL);
    SET_ERR(0);
    int r = U(Escape)(m, QUERYESCSUPPORT, 0, NULL, NULL);
    CHECKF(r == -1, "got %d", r);
    EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    U(DeleteDC)(m);
}
TEST(escape_queryesc_cbin_zero_valid_ptr) {
    REQUIRE(U(Escape));
    HDC d = ep_scr();
    int q = 0x100; /* GETEXTENDEDTEXTMETRICS: answered inside gdi32full, cjIn is not consulted */
    int r = U(Escape)(d, QUERYESCSUPPORT, 0, (LPCSTR)&q, NULL);
    if (r != 1) t_note("cbIn=0 query returned %d", r);
    q = 0x1234;
    r = U(Escape)(d, QUERYESCSUPPORT, 0, (LPCSTR)&q, NULL);
    CHECKF(r == 0, "got %d", r);
    ep_scr_free(d);
}
TEST(escape_queryesc_uses_low_word_only) {
    REQUIRE(U(Escape));
    HDC d = ep_scr();
    int q = 0x00010003; /* low word NEXTBAND, high word garbage */
    int r = U(Escape)(d, QUERYESCSUPPORT, sizeof q, (LPCSTR)&q, NULL);
    CHECKF(r == 0, "got %d", r);
    q = (int)0xFFFF0100; /* low word GETEXTENDEDTEXTMETRICS */
    r = U(Escape)(d, QUERYESCSUPPORT, sizeof q, (LPCSTR)&q, NULL);
    CHECKF(r == 1, "got %d", r);
    ep_scr_free(d);
}
TEST(escape_queryesc_pout_ignored) {
    REQUIRE(U(Escape));
    HDC d = ep_scr();
    char junk[16]; memset(junk, 0x5a, sizeof junk);
    int q = 0x100;
    int r = U(Escape)(d, QUERYESCSUPPORT, sizeof q, (LPCSTR)&q, junk);
    CHECKF(r == 1, "got %d", r);
    CHECK(junk[0] == 0x5a && junk[15] == 0x5a);
    ep_scr_free(d);
}
TEST(escape_queryesc_repeatable) {
    REQUIRE(U(Escape));
    HDC d = ep_scr();
    int q = 0xc01;
    for (int i = 0; i < 50; i++) { int r = U(Escape)(d, QUERYESCSUPPORT, sizeof q, (LPCSTR)&q, NULL); if (r != 1) { CHECKF(0, "iter %d got %d", i, r); break; } }
    ep_scr_free(d);
}
TEST(escape_queryesc_after_dc_deleted_note) {
    REQUIRE(U(Escape));
    HDC m = U(CreateCompatibleDC)(NULL);
    U(DeleteDC)(m);
    int q = 3;
    int r = U(Escape)(m, QUERYESCSUPPORT, sizeof q, (LPCSTR)&q, NULL);
    /* a dead handle no longer has an LO_DC_TYPE entry; observed only */
    t_note("Escape(QUERYESCSUPPORT, NEXTBAND) on deleted DC -> %d (err %lu)", r, GetLastError());
}
TEST(escape_queryesc_bogus_handles_note) {
    REQUIRE(U(Escape));
    int q = 3;
    HDC hs[3] = { NULL, EP_BOGUS_DC, EP_BOGUS_DC2 };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        int r = U(Escape)(hs[i], QUERYESCSUPPORT, sizeof q, (LPCSTR)&q, NULL);
        t_note("Escape(bogus %p, QUERYESCSUPPORT NEXTBAND) -> %d (err %lu)", hs[i], r, GetLastError());
    }
}
TEST(escape_queryesc_printer_only_ids_ext_path_note) {
    REQUIRE(U(Escape));
    static const int ids[] = { SETCOPYCOUNT, DRAWPATTERNRECT, POSTSCRIPT_DATA, POSTSCRIPT_IGNORE, 0x2a, 0x202, 0x1000, 0x1001, 0x1002, 0x1013, 0x1015, 0x1016 };
    HDC d = ep_scr(), m = U(CreateCompatibleDC)(NULL);
    for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        int q = ids[i];
        int a = U(Escape)(d, QUERYESCSUPPORT, sizeof q, (LPCSTR)&q, NULL);
        int b = U(Escape)(m, QUERYESCSUPPORT, sizeof q, (LPCSTR)&q, NULL);
        CHECKF(a >= 0 && b >= 0, "id 0x%x screen %d mem %d", q, a, b);
        if (a || b) t_note("QUERYESCSUPPORT 0x%x supported: screen=%d mem=%d", q, a, b);
    }
    U(DeleteDC)(m); ep_scr_free(d);
}
TEST(escape_getcolortable_null_in) {
    EP_WINE_SKIP("null input pointer dereferenced");
    REQUIRE(U(Escape));
    HDC d = ep_scr();
    SET_ERR(0);
    int r = U(Escape)(d, GETCOLORTABLE, sizeof(int), NULL, NULL);
    CHECKF(r == -1, "got %d", r);
    EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    ep_scr_free(d);
}
TEST(escape_getcolortable_out_of_range_index) {
    REQUIRE(U(Escape));
    HDC d = ep_scr();
    PALETTEENTRY pe; memset(&pe, 0, sizeof pe);
    short idx = 0x7fff;
    int r = U(Escape)(d, GETCOLORTABLE, sizeof idx, (LPCSTR)&idx, (LPVOID)&pe);
    /* GetSystemPaletteEntries fails when the index is beyond the palette (or the device has none) */
    CHECKF(r == -1, "got %d", r);
    ep_scr_free(d);
}
TEST(escape_getcolortable_memdc_1bpp) {
    REQUIRE(U(Escape)); REQUIRE(U(CreateCompatibleDC));
    HDC m = U(CreateCompatibleDC)(NULL);
    PALETTEENTRY pe[2]; short idx = 0;
    int r = U(Escape)(m, GETCOLORTABLE, sizeof idx, (LPCSTR)&idx, (LPVOID)pe);
    t_note("GETCOLORTABLE on mono mem DC -> %d", r);
    CHECK(r == -1 || r >= 0);
    U(DeleteDC)(m);
}
TEST(escape_unsupported_nextband_screen) {
    REQUIRE(U(Escape));
    HDC d = ep_scr();
    RECT rc = {1,2,3,4};
    int r = U(Escape)(d, NEXTBAND, 0, NULL, (LPVOID)&rc);
    CHECKF(r <= 0, "got %d", r);
    t_note("NEXTBAND screen -> %d err %lu", r, GetLastError());
    ep_scr_free(d);
}
TEST(escape_unsupported_nextband_mem) {
    REQUIRE(U(Escape));
    HDC m = U(CreateCompatibleDC)(NULL);
    RECT rc = {1,2,3,4};
    int r = U(Escape)(m, NEXTBAND, 0, NULL, (LPVOID)&rc);
    CHECKF(r <= 0, "got %d", r);
    U(DeleteDC)(m);
}
TEST(escape_unsupported_gettechnology) {
    REQUIRE(U(Escape));
    HDC d = ep_scr();
    char buf[64]; memset(buf, 0, sizeof buf);
    int r = U(Escape)(d, GETTECHNOLOGY, 0, NULL, buf);
    CHECKF(r <= 0, "got %d", r);
    ep_scr_free(d);
}
TEST(escape_unsupported_enableduplex) {
    REQUIRE(U(Escape));
    HDC d = ep_scr();
    WORD v = 1;
    int r = U(Escape)(d, ENABLEDUPLEX, sizeof v, (LPCSTR)&v, NULL);
    CHECKF(r <= 0, "got %d", r);
    ep_scr_free(d);
}
TEST(escape_unsupported_getscalingfactor) {
    REQUIRE(U(Escape));
    HDC d = ep_scr();
    POINT pt = {7, 7};
    int r = U(Escape)(d, GETSCALINGFACTOR, 0, NULL, (LPVOID)&pt);
    CHECKF(r <= 0, "got %d", r);
    ep_scr_free(d);
}
TEST(escape_unsupported_setcopycount) {
    REQUIRE(U(Escape));
    HDC d = ep_scr();
    WORD cnt = 2, outc = 0;
    int r = U(Escape)(d, SETCOPYCOUNT, sizeof cnt, (LPCSTR)&cnt, (LPVOID)&outc);
    CHECKF(r <= 0, "got %d", r);
    ep_scr_free(d);
}
TEST(escape_unsupported_getphyspagesize) {
    REQUIRE(U(Escape));
    HDC d = ep_scr();
    POINT pt = {7, 7};
    int r = U(Escape)(d, GETPHYSPAGESIZE, 0, NULL, (LPVOID)&pt);
    CHECKF(r <= 0, "got %d", r);
    ep_scr_free(d);
}
TEST(escape_unsupported_getprintingoffset) {
    REQUIRE(U(Escape));
    HDC d = ep_scr();
    POINT pt = {7, 7};
    int r = U(Escape)(d, GETPRINTINGOFFSET, 0, NULL, (LPVOID)&pt);
    CHECKF(r <= 0, "got %d", r);
    ep_scr_free(d);
}
TEST(escape_unsupported_newframe_abortdoc_enddoc_note) {
    REQUIRE(U(Escape));
    HDC d = ep_scr();
    static const int ids[] = { NEWFRAME, ABORTDOC, ENDDOC, FLUSHOUTPUT, DRAFTMODE };
    for (unsigned i = 0; i < 5; i++) {
        SET_ERR(0);
        int r = U(Escape)(d, ids[i], 0, NULL, NULL);
        CHECKF(r <= 0, "escape %d got %d", ids[i], r);
        t_note("Escape(screen,%d) -> %d err %lu", ids[i], r, GetLastError());
    }
    ep_scr_free(d);
}
TEST(escape_startdoc_escape_on_screen_fails) {
    REQUIRE(U(Escape));
    HDC d = ep_scr();
    char name[] = "gdi32 corpus";
    SET_ERR(0);
    int r = U(Escape)(d, STARTDOC, (int)strlen(name), name, NULL);
    CHECKF(r <= 0, "got %d", r);
    t_note("Escape(STARTDOC) screen -> %d err %lu", r, GetLastError());
    ep_scr_free(d);
}
TEST(escape_unknown_ids_unsupported_screen) {
    REQUIRE(U(Escape));
    HDC d = ep_scr();
    static const int ids[] = { 0x4321, 0x7777, 0xFFFF };
    for (unsigned i = 0; i < 3; i++) {
        int r = U(Escape)(d, ids[i], 0, NULL, NULL);
        CHECKF(r <= 0, "id 0x%x got %d", ids[i], r);
    }
    ep_scr_free(d);
}
TEST(escape_passthrough_screen_not_supported) {
    REQUIRE(U(Escape));
    HDC d = ep_scr();
    WORD buf[4] = { 2, 'h', 'i', 0 };
    int r = U(Escape)(d, PASSTHROUGH, sizeof buf, (LPCSTR)buf, NULL);
    CHECKF(r <= 0, "got %d", r);
    ep_scr_free(d);
}
TEST(escape_gdi_balance) {
    REQUIRE(U(Escape)); REQUIRE(U(GetGuiResources));
    DWORD b = ep_gui();
    int q = 3;
    for (int i = 0; i < 200; i++) {
        HDC d = ep_scr(); U(Escape)(d, QUERYESCSUPPORT, sizeof q, (LPCSTR)&q, NULL); ep_scr_free(d);
    }
    DWORD a = ep_gui();
    CHECKF(a == b, "before %lu after %lu", b, a);
}

/* ---- ExtEscape ---- */
TEST(extescape_queryesc_getpairkerntable) {
    REQUIRE(U(ExtEscape));
    HDC d = ep_scr(), m = U(CreateCompatibleDC)(NULL);
    int q = GETPAIRKERNTABLE;
    CHECK(U(ExtEscape)(d, QUERYESCSUPPORT, sizeof q, (LPCSTR)&q, 0, NULL) == 1);
    CHECK(U(ExtEscape)(m, QUERYESCSUPPORT, sizeof q, (LPCSTR)&q, 0, NULL) == 1);
    U(DeleteDC)(m); ep_scr_free(d);
}
TEST(extescape_queryesc_pairkern_on_bogus_dc) {
    REQUIRE(U(ExtEscape));
    int q = GETPAIRKERNTABLE;
    HDC hs[3] = { NULL, EP_BOGUS_DC, EP_BOGUS_DC2 };
    for (int i = 0; i < 3; i++) {
        int r = U(ExtEscape)(hs[i], QUERYESCSUPPORT, sizeof q, (LPCSTR)&q, 0, NULL);
        CHECKF(r == 1, "handle %p got %d", hs[i], r);
    }
}
TEST(extescape_unknown_id_screen) {
    REQUIRE(U(ExtEscape));
    HDC d = ep_scr();
    int in = 0x1234;
    int r = U(ExtEscape)(d, 0x7777, sizeof in, (LPCSTR)&in, 0, NULL);
    CHECKF(r <= 0, "got %d", r);
    ep_scr_free(d);
}
TEST(extescape_unknown_id_mem) {
    REQUIRE(U(ExtEscape));
    HDC m = U(CreateCompatibleDC)(NULL);
    int in = 0x1234;
    int r = U(ExtEscape)(m, 0x7777, sizeof in, (LPCSTR)&in, 0, NULL);
    CHECKF(r <= 0, "got %d", r);
    U(DeleteDC)(m);
}
TEST(extescape_queryesc_unknown_id) {
    REQUIRE(U(ExtEscape));
    HDC d = ep_scr();
    int q = 0x7777;
    int r = U(ExtEscape)(d, QUERYESCSUPPORT, sizeof q, (LPCSTR)&q, 0, NULL);
    CHECKF(r <= 0, "got %d", r);
    ep_scr_free(d);
}
TEST(extescape_null_in_nonzero_cb) {
    REQUIRE(U(ExtEscape));
    HDC d = ep_scr();
    int r = U(ExtEscape)(d, 0x7777, 64, NULL, 0, NULL);
    CHECKF(r <= 0, "got %d", r);
    ep_scr_free(d);
}
TEST(extescape_null_out_nonzero_cb) {
    REQUIRE(U(ExtEscape));
    HDC d = ep_scr();
    int in = 1;
    int r = U(ExtEscape)(d, 0x7777, sizeof in, (LPCSTR)&in, 64, NULL);
    CHECKF(r <= 0, "got %d", r);
    ep_scr_free(d);
}
TEST(extescape_out_buffer_untouched_on_unsupported) {
    REQUIRE(U(ExtEscape));
    HDC d = ep_scr();
    char out[32]; memset(out, 0xCC, sizeof out);
    int in = 1;
    int r = U(ExtEscape)(d, 0x7777, sizeof in, (LPCSTR)&in, sizeof out, out);
    CHECKF(r <= 0, "got %d", r);
    int same = 1; for (int i = 0; i < 32; i++) if ((unsigned char)out[i] != 0xCC) same = 0;
    CHECK(same);
    ep_scr_free(d);
}
TEST(extescape_null_dc) {
    REQUIRE(U(ExtEscape));
    int in = 1;
    SET_ERR(0);
    int r = U(ExtEscape)(NULL, 0x7777, sizeof in, (LPCSTR)&in, 0, NULL);
    CHECKF(r <= 0, "got %d", r);
    t_note("ExtEscape(NULL) -> %d err %lu", r, GetLastError());
}
TEST(extescape_bogus_dc) {
    REQUIRE(U(ExtEscape));
    int in = 1;
    SET_ERR(0);
    int r = U(ExtEscape)(EP_BOGUS_DC, 0x7777, sizeof in, (LPCSTR)&in, 0, NULL);
    CHECKF(r <= 0, "got %d", r);
    t_note("ExtEscape(bogus) -> %d err %lu", r, GetLastError());
}
TEST(extescape_deleted_dc) {
    REQUIRE(U(ExtEscape));
    HDC m = U(CreateCompatibleDC)(NULL);
    U(DeleteDC)(m);
    int in = 1;
    int r = U(ExtEscape)(m, 0x7777, sizeof in, (LPCSTR)&in, 0, NULL);
    CHECKF(r <= 0, "got %d", r);
}
TEST(extescape_getextendedtextmetrics_note) {
    REQUIRE(U(ExtEscape)); REQUIRE(U(SelectObject)); REQUIRE(U(GetStockObject));
    HDC m = U(CreateCompatibleDC)(NULL);
    HGDIOBJ old = U(SelectObject)(m, U(GetStockObject)(SYSTEM_FONT));
    WORD etm[64]; memset(etm, 0, sizeof etm);
    int r = U(ExtEscape)(m, GETEXTENDEDTEXTMETRICS, 0, NULL, 0x34, (LPSTR)etm);
    t_note("GETEXTENDEDTEXTMETRICS on stock font -> %d, etmSize=%u", r, etm[0]);
    CHECK(r == 0 || r == 1);
    U(SelectObject)(m, old);
    U(DeleteDC)(m);
}
TEST(extescape_getpairkerntable_zero_note) {
    REQUIRE(U(ExtEscape));
    HDC m = U(CreateCompatibleDC)(NULL);
    int r = U(ExtEscape)(m, GETPAIRKERNTABLE, 0, NULL, 0, NULL);
    t_note("GETPAIRKERNTABLE(0 out) -> %d", r);
    CHECK(r >= 0);
    U(DeleteDC)(m);
}
TEST(extescape_setcopycount_screen) {
    REQUIRE(U(ExtEscape));
    HDC d = ep_scr();
    int cnt = 3, outc = 0;
    int r = U(ExtEscape)(d, SETCOPYCOUNT, sizeof cnt, (LPCSTR)&cnt, sizeof outc, (LPSTR)&outc);
    CHECKF(r <= 0, "got %d", r);
    ep_scr_free(d);
}
TEST(extescape_negative_cb_note) {
    REQUIRE(U(ExtEscape));
    HDC d = ep_scr();
    int in = 1;
    int r = U(ExtEscape)(d, 0x7777, -1, (LPCSTR)&in, 0, NULL);
    t_note("ExtEscape cbInput=-1 -> %d err %lu", r, GetLastError());
    CHECK(r <= 0);
    ep_scr_free(d);
}
TEST(extescape_gdi_balance) {
    REQUIRE(U(ExtEscape)); REQUIRE(U(GetGuiResources));
    DWORD b = ep_gui();
    int q = GETPAIRKERNTABLE;
    for (int i = 0; i < 200; i++) { HDC d = ep_scr(); U(ExtEscape)(d, QUERYESCSUPPORT, sizeof q, (LPCSTR)&q, 0, NULL); ep_scr_free(d); }
    CHECK(ep_gui() == b);
}

/* ---- DrawEscape ---- */
TEST(drawescape_null_dc) {
    REQUIRE(U(DrawEscape));
    int in = 1;
    SET_ERR(0);
    int r = U(DrawEscape)(NULL, 0x7777, sizeof in, (LPCSTR)&in);
    CHECKF(r == 0, "got %d", r);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
}
TEST(drawescape_bogus_dc) {
    REQUIRE(U(DrawEscape));
    int in = 1;
    SET_ERR(0);
    int r = U(DrawEscape)(EP_BOGUS_DC, 0x7777, sizeof in, (LPCSTR)&in);
    CHECKF(r == 0, "got %d", r);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
}
TEST(drawescape_bogus_dc2) {
    REQUIRE(U(DrawEscape));
    SET_ERR(0);
    int r = U(DrawEscape)(EP_BOGUS_DC2, 1, 0, NULL);
    CHECKF(r == 0, "got %d", r);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
}
TEST(drawescape_unsupported_screen) {
    REQUIRE(U(DrawEscape));
    HDC d = ep_scr();
    int in = 1;
    int r = U(DrawEscape)(d, 0x7777, sizeof in, (LPCSTR)&in);
    CHECKF(r <= 0, "got %d", r);
    ep_scr_free(d);
}
TEST(drawescape_unsupported_mem) {
    REQUIRE(U(DrawEscape));
    HDC m = U(CreateCompatibleDC)(NULL);
    int in = 1;
    int r = U(DrawEscape)(m, 0x7777, sizeof in, (LPCSTR)&in);
    CHECKF(r <= 0, "got %d", r);
    U(DeleteDC)(m);
}
TEST(drawescape_queryesc_screen) {
    REQUIRE(U(DrawEscape));
    HDC d = ep_scr();
    int q = 0x7777;
    int r = U(DrawEscape)(d, QUERYESCSUPPORT, sizeof q, (LPCSTR)&q);
    CHECKF(r <= 0, "got %d", r);
    ep_scr_free(d);
}
TEST(drawescape_null_in_nonzero_cb) {
    REQUIRE(U(DrawEscape));
    HDC d = ep_scr();
    int r = U(DrawEscape)(d, 0x7777, 16, NULL);
    CHECKF(r <= 0, "got %d", r);
    ep_scr_free(d);
}
TEST(drawescape_deleted_dc) {
    REQUIRE(U(DrawEscape));
    HDC m = U(CreateCompatibleDC)(NULL);
    U(DeleteDC)(m);
    int in = 1;
    int r = U(DrawEscape)(m, 0x7777, sizeof in, (LPCSTR)&in);
    CHECKF(r <= 0, "got %d", r);
}

/* ---- StartDoc family on non-printer DCs ---- */
static void startdoc_null_dc_cb(DWORD cb) {
    DOCINFOW di; memset(&di, 0, sizeof di);
    di.cbSize = cb; di.lpszDocName = L"corpus";
    SET_ERR(0);
    int r = U(StartDocW)(NULL, &di);
    CHECKF(r == -1, "cbSize %lu got %d", cb, r);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
}
#define SD_CB(n, cb) TEST(startdoc_w_nulldc_cbsize_##n) { REQUIRE(U(StartDocW)); startdoc_null_dc_cb(cb); }
SD_CB(0, 0) SD_CB(1, 1) SD_CB(8, 8) SD_CB(0x18, 0x18) SD_CB(0x20, 0x20) SD_CB(0x28, 0x28) SD_CB(0x30, 0x30) SD_CB(0xffff, 0xffff) SD_CB(max, 0xffffffffu)
TEST(startdoc_w_null_dc_null_docinfo) {
    REQUIRE(U(StartDocW));
    SET_ERR(0);
    int r = U(StartDocW)(NULL, NULL);
    CHECKF(r == -1, "got %d", r);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
}
TEST(startdoc_a_null_dc_null_docinfo) {
    REQUIRE(U(StartDocA));
    SET_ERR(0);
    int r = U(StartDocA)(NULL, NULL);
    CHECKF(r == -1, "got %d", r);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
}
TEST(startdoc_w_bogus_dc) {
    REQUIRE(U(StartDocW));
    DOCINFOW di; memset(&di, 0, sizeof di); di.cbSize = sizeof di; di.lpszDocName = L"x";
    SET_ERR(0);
    int r = U(StartDocW)(EP_BOGUS_DC, &di);
    CHECKF(r == -1, "got %d", r);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
}
TEST(startdoc_a_bogus_dc) {
    REQUIRE(U(StartDocA));
    DOCINFOA di; memset(&di, 0, sizeof di); di.cbSize = sizeof di; di.lpszDocName = "x";
    SET_ERR(0);
    int r = U(StartDocA)(EP_BOGUS_DC2, &di);
    CHECKF(r == -1, "got %d", r);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
}
TEST(startdoc_w_deleted_dc) {
    REQUIRE(U(StartDocW));
    HDC m = U(CreateCompatibleDC)(NULL); U(DeleteDC)(m);
    DOCINFOW di; memset(&di, 0, sizeof di); di.cbSize = sizeof di; di.lpszDocName = L"x";
    SET_ERR(0);
    int r = U(StartDocW)(m, &di);
    CHECKF(r == -1, "got %d", r);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
}
/* StartDocA converts name/output to a 260-WCHAR buffer; len+1 > 260 -> ERROR_FILENAME_EXCED_RANGE before any DC check */
static void sda_len(int which, int len, int expect_exceed, HDC dc) {
    char *s = (char *)HeapAlloc(GetProcessHeap(), 0, len + 1);
    if (!s) return;
    memset(s, 'a', len); s[len] = 0;
    DOCINFOA di; memset(&di, 0, sizeof di); di.cbSize = sizeof di;
    if (which == 0) { di.lpszDocName = s; } else { di.lpszDocName = "d"; di.lpszOutput = s; }
    SET_ERR(0);
    int r = U(StartDocA)(dc, &di);
    DWORD e = GetLastError();
    CHECKF(r == -1, "len %d got %d", len, r);
    if (expect_exceed) CHECKF(e == ERROR_FILENAME_EXCED_RANGE, "len %d err %lu", len, e);
    else CHECKF(e != ERROR_FILENAME_EXCED_RANGE, "len %d unexpected 206", len);
    HeapFree(GetProcessHeap(), 0, s);
}
#define SD_LEN(n, which, len, ex) TEST(startdoc_a_##n) { REQUIRE(U(StartDocA)); sda_len(which, len, ex, NULL); }
SD_LEN(docname_len0, 0, 0, 0) SD_LEN(docname_len1, 0, 1, 0) SD_LEN(docname_len100, 0, 100, 0) SD_LEN(docname_len258, 0, 258, 0) SD_LEN(docname_len259, 0, 259, 0)
SD_LEN(docname_len260, 0, 260, 1) SD_LEN(docname_len261, 0, 261, 1) SD_LEN(docname_len1000, 0, 1000, 1) SD_LEN(docname_len20000, 0, 20000, 1)
SD_LEN(output_len0, 1, 0, 0) SD_LEN(output_len259, 1, 259, 0) SD_LEN(output_len260, 1, 260, 1) SD_LEN(output_len261, 1, 261, 1) SD_LEN(output_len5000, 1, 5000, 1)
TEST(startdoc_a_toolong_on_screen_dc) {
    REQUIRE(U(StartDocA));
    HDC d = ep_scr();
    sda_len(0, 300, 1, d);
    sda_len(1, 300, 1, d);
    ep_scr_free(d);
}
TEST(startdoc_a_toolong_on_mem_dc) {
    REQUIRE(U(StartDocA));
    HDC m = U(CreateCompatibleDC)(NULL);
    sda_len(0, 260, 1, m);
    sda_len(1, 260, 1, m);
    U(DeleteDC)(m);
}
TEST(startdoc_a_both_names_max_legal) {
    REQUIRE(U(StartDocA));
    char a[260], b[260]; memset(a, 'n', 259); a[259] = 0; memset(b, 'o', 259); b[259] = 0;
    DOCINFOA di; memset(&di, 0, sizeof di); di.cbSize = sizeof di; di.lpszDocName = a; di.lpszOutput = b;
    SET_ERR(0);
    int r = U(StartDocA)(NULL, &di);
    CHECK(r == -1);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
}
TEST(startdoc_a_docname_ok_output_toolong) {
    REQUIRE(U(StartDocA));
    char b[300]; memset(b, 'o', 299); b[299] = 0;
    DOCINFOA di; memset(&di, 0, sizeof di); di.cbSize = sizeof di; di.lpszDocName = "ok"; di.lpszOutput = b;
    SET_ERR(0);
    int r = U(StartDocA)(NULL, &di);
    CHECK(r == -1);
    EXPECT_ERR(r, ERROR_FILENAME_EXCED_RANGE);
}
TEST(startdoc_screen_dc_fails_w) {
    REQUIRE(U(StartDocW));
    HDC d = ep_scr();
    DOCINFOW di; memset(&di, 0, sizeof di); di.cbSize = sizeof di; di.lpszDocName = L"corpus doc";
    SET_ERR(0);
    int r = U(StartDocW)(d, &di);
    CHECKF(r <= 0, "got %d", r);
    t_note("StartDocW(screen) -> %d err %lu", r, GetLastError());
    if (r > 0) U(AbortDoc)(d);
    ep_scr_free(d);
}
TEST(startdoc_screen_dc_fails_a) {
    REQUIRE(U(StartDocA));
    HDC d = ep_scr();
    DOCINFOA di; memset(&di, 0, sizeof di); di.cbSize = sizeof di; di.lpszDocName = "corpus doc";
    SET_ERR(0);
    int r = U(StartDocA)(d, &di);
    CHECKF(r <= 0, "got %d", r);
    t_note("StartDocA(screen) -> %d err %lu", r, GetLastError());
    ep_scr_free(d);
}
TEST(startdoc_mem_dc_fails_w) {
    REQUIRE(U(StartDocW));
    HDC m = U(CreateCompatibleDC)(NULL);
    DOCINFOW di; memset(&di, 0, sizeof di); di.cbSize = sizeof di; di.lpszDocName = L"corpus doc";
    int r = U(StartDocW)(m, &di);
    CHECKF(r <= 0, "got %d", r);
    t_note("StartDocW(mem) -> %d err %lu", r, GetLastError());
    U(DeleteDC)(m);
}
TEST(startdoc_mem_dc_null_docinfo) {
    REQUIRE(U(StartDocW));
    HDC m = U(CreateCompatibleDC)(NULL);
    int r = U(StartDocW)(m, NULL);
    CHECKF(r <= 0, "got %d", r);
    U(DeleteDC)(m);
}
TEST(startdoc_mem_dc_dibsection) {
    REQUIRE(U(StartDocW)); REQUIRE(U(CreateDIBSection));
    HBITMAP b, o; HDC m = ep_memdc32(8, 8, &b, &o, NULL);
    CHECK(m != NULL);
    if (m) {
        DOCINFOW di; memset(&di, 0, sizeof di); di.cbSize = sizeof di; di.lpszDocName = L"x";
        int r = U(StartDocW)(m, &di);
        CHECKF(r <= 0, "got %d", r);
        ep_memdc_free(m, b, o);
    }
}
TEST(startdoc_datatype_emf_null_dc) {
    REQUIRE(U(StartDocA));
    DOCINFOA di; memset(&di, 0, sizeof di); di.cbSize = sizeof di; di.lpszDocName = "x"; di.lpszDatatype = "emf";
    SET_ERR(0);
    CHECK(U(StartDocA)(NULL, &di) == -1);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    di.lpszDatatype = "RAW"; di.fwType = 1;
    SET_ERR(0);
    CHECK(U(StartDocA)(NULL, &di) == -1);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}

/* ---- EndDoc / StartPage / EndPage / AbortDoc ---- */
#define DOCFN_TESTS(fn) \
TEST(fn##_null_dc) { REQUIRE(U(fn)); SET_ERR(0); int r = U(fn)(NULL); CHECKF(r == -1, "got %d", r); EXPECT_ERR(r, ERROR_INVALID_HANDLE); } \
TEST(fn##_bogus_dc) { REQUIRE(U(fn)); SET_ERR(0); int r = U(fn)(EP_BOGUS_DC); CHECKF(r == -1, "got %d", r); EXPECT_ERR(r, ERROR_INVALID_HANDLE); } \
TEST(fn##_bogus_dc2) { REQUIRE(U(fn)); SET_ERR(0); int r = U(fn)(EP_BOGUS_DC2); CHECKF(r == -1, "got %d", r); EXPECT_ERR(r, ERROR_INVALID_HANDLE); } \
TEST(fn##_screen_dc) { REQUIRE(U(fn)); HDC d = ep_scr(); SET_ERR(0); int r = U(fn)(d); CHECKF(r == -1, "got %d", r); EXPECT_ERR(r, ERROR_INVALID_HANDLE); ep_scr_free(d); } \
TEST(fn##_mem_dc) { REQUIRE(U(fn)); HDC m = U(CreateCompatibleDC)(NULL); SET_ERR(0); int r = U(fn)(m); CHECKF(r == -1, "got %d", r); EXPECT_ERR(r, ERROR_INVALID_HANDLE); U(DeleteDC)(m); } \
TEST(fn##_deleted_dc) { REQUIRE(U(fn)); HDC m = U(CreateCompatibleDC)(NULL); U(DeleteDC)(m); SET_ERR(0); int r = U(fn)(m); CHECKF(r == -1, "got %d", r); EXPECT_ERR(r, ERROR_INVALID_HANDLE); } \
TEST(fn##_dibsection_dc) { REQUIRE(U(fn)); HBITMAP b, o; HDC m = ep_memdc32(4, 4, &b, &o, NULL); CHECK(m != NULL); if (m) { SET_ERR(0); int r = U(fn)(m); CHECKF(r == -1, "got %d", r); EXPECT_ERR(r, ERROR_INVALID_HANDLE); ep_memdc_free(m, b, o); } }
DOCFN_TESTS(EndDoc)
DOCFN_TESTS(StartPage)
DOCFN_TESTS(AbortDoc)
#define DOCFN_SOFT(fn) \
TEST(fn##_null_dc) { REQUIRE(U(fn)); SET_ERR(0); int r = U(fn)(NULL); CHECKF(r <= 0, "got %d", r); t_note(#fn "(NULL) err %lu", GetLastError()); } \
TEST(fn##_bogus_dc) { REQUIRE(U(fn)); SET_ERR(0); int r = U(fn)(EP_BOGUS_DC); CHECKF(r <= 0, "got %d", r); } \
TEST(fn##_screen_dc) { REQUIRE(U(fn)); HDC d = ep_scr(); SET_ERR(0); int r = U(fn)(d); CHECKF(r <= 0, "got %d", r); t_note(#fn "(screen) -> %d err %lu", r, GetLastError()); ep_scr_free(d); } \
TEST(fn##_mem_dc) { REQUIRE(U(fn)); HDC m = U(CreateCompatibleDC)(NULL); int r = U(fn)(m); CHECKF(r <= 0, "got %d", r); U(DeleteDC)(m); } \
TEST(fn##_deleted_dc) { REQUIRE(U(fn)); HDC m = U(CreateCompatibleDC)(NULL); U(DeleteDC)(m); int r = U(fn)(m); CHECKF(r <= 0, "got %d", r); }
DOCFN_SOFT(EndPage)

/* ---- SetAbortProc ---- */
static BOOL CALLBACK abortproc(HDC dc, int err) { return TRUE; }
TEST(setabortproc_null_dc) {
    REQUIRE(U(SetAbortProc));
    SET_ERR(0);
    int r = U(SetAbortProc)(NULL, abortproc);
    CHECKF(r == -1, "got %d", r);
    t_note("SetAbortProc(NULL) err %lu", GetLastError());
}
TEST(setabortproc_bogus_dc) {
    REQUIRE(U(SetAbortProc));
    SET_ERR(0);
    int r = U(SetAbortProc)(EP_BOGUS_DC, abortproc);
    CHECKF(r == -1, "got %d", r);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
}
TEST(setabortproc_bogus_dc_nullproc) {
    REQUIRE(U(SetAbortProc));
    SET_ERR(0);
    int r = U(SetAbortProc)(EP_BOGUS_DC2, NULL);
    CHECKF(r == -1, "got %d", r);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
}
TEST(setabortproc_screen_dc) {
    REQUIRE(U(SetAbortProc));
    HDC d = ep_scr();
    CHECK(U(SetAbortProc)(d, abortproc) == -1);
    CHECK(U(SetAbortProc)(d, NULL) == -1);
    ep_scr_free(d);
}
TEST(setabortproc_mem_dc) {
    REQUIRE(U(SetAbortProc));
    HDC m = U(CreateCompatibleDC)(NULL);
    CHECK(U(SetAbortProc)(m, abortproc) == -1);
    CHECK(U(SetAbortProc)(m, NULL) == -1);
    U(DeleteDC)(m);
}
TEST(setabortproc_dibsection_dc) {
    REQUIRE(U(SetAbortProc));
    HBITMAP b, o; HDC m = ep_memdc32(4, 4, &b, &o, NULL);
    CHECK(m != NULL);
    if (m) { CHECK(U(SetAbortProc)(m, abortproc) == -1); ep_memdc_free(m, b, o); }
}
TEST(setabortproc_deleted_dc) {
    REQUIRE(U(SetAbortProc));
    HDC m = U(CreateCompatibleDC)(NULL); U(DeleteDC)(m);
    SET_ERR(0);
    int r = U(SetAbortProc)(m, abortproc);
    CHECKF(r == -1, "got %d", r);
}
TEST(setabortproc_does_not_change_last_error_screen) {
    REQUIRE(U(SetAbortProc));
    HDC d = ep_scr();
    SET_ERR(0x1234);
    U(SetAbortProc)(d, abortproc);
    DWORD e = GetLastError();
    t_note("last error after SetAbortProc(screen): %lu", e);
    ep_scr_free(d);
}
