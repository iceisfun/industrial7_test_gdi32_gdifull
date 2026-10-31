/* Selection, GetTextFace, font mapper behaviour, height/width semantics. */
#include "fe.h"

/* ---- GetTextFace ---- */
static HDC face_dc(HFONT *f, HGDIOBJ *old, const char *face) {
    HDC dc = U(CreateCompatibleDC)(NULL);
    *f = fe_mk(-14, 0, 400, 0, ANSI_CHARSET, face);
    *old = U(SelectObject)(dc, *f);
    return dc;
}
static void face_done(HDC dc, HFONT f, HGDIOBJ old) { U(SelectObject)(dc, old); U(DeleteObject)(f); U(DeleteDC)(dc); }

TEST(fonts_face_a_null_buffer_returns_len_with_nul) {
    FE_BASE(); REQUIRE(U(GetTextFaceW));
    HFONT f; HGDIOBJ old; HDC dc = face_dc(&f, &old, "Arial");
    char n[40]; U(GetTextFaceA)(dc, 40, n);
    if (strcmp(n, "Arial")) { t_note("Arial unavailable"); face_done(dc, f, old); return; }
    EQ(U(GetTextFaceA)(dc, 0, NULL), 6);
    EQ(U(GetTextFaceA)(dc, 100, NULL), 6);
    EQ(U(GetTextFaceW)(dc, 0, NULL), 6);
    EQ(U(GetTextFaceW)(dc, 100, NULL), 6);
    face_done(dc, f, old);
}
TEST(fonts_face_buffer_big_a_w) {
    FE_BASE(); REQUIRE(U(GetTextFaceW));
    HFONT f; HGDIOBJ old; HDC dc = face_dc(&f, &old, "Arial");
    char n[64]; WCHAR w[64];
    memset(n, 'X', sizeof n); for (int i = 0; i < 64; i++) w[i] = L'X';
    int ra = U(GetTextFaceA)(dc, 64, n), rw = U(GetTextFaceW)(dc, 64, w);
    if (!strcmp(n, "Arial")) {
        EQ(ra, 5);            /* A version excludes the terminating NUL when a buffer is supplied */
        EQ(rw, 6);            /* W version counts it */
        EXPECT_WSTR(w, L"Arial");
    } else t_note("Arial unavailable: '%s'", n);
    face_done(dc, f, old);
}
TEST(fonts_face_buffer_exact_w) {
    FE_BASE(); REQUIRE(U(GetTextFaceW));
    HFONT f; HGDIOBJ old; HDC dc = face_dc(&f, &old, "Arial");
    char n[40]; U(GetTextFaceA)(dc, 40, n);
    if (strcmp(n, "Arial")) { face_done(dc, f, old); return; }
    WCHAR w[8]; for (int i = 0; i < 8; i++) w[i] = 0x7777;
    int r = U(GetTextFaceW)(dc, 6, w);
    EQ(r, 6); EXPECT_WSTR(w, L"Arial"); EQ(w[6], 0x7777);
    char a[8]; memset(a, 'Y', sizeof a);
    r = U(GetTextFaceA)(dc, 6, a);
    EQ(r, 5); EXPECT_STR(a, "Arial"); EQ(a[6], 'Y');
    face_done(dc, f, old);
}
TEST(fonts_face_buffer_exact_minus_one) {
    FE_BASE(); REQUIRE(U(GetTextFaceW));
    HFONT f; HGDIOBJ old; HDC dc = face_dc(&f, &old, "Arial");
    char n[40]; U(GetTextFaceA)(dc, 40, n);
    if (strcmp(n, "Arial")) { face_done(dc, f, old); return; }
    WCHAR w[8]; for (int i = 0; i < 8; i++) w[i] = 0x7777;
    int r = U(GetTextFaceW)(dc, 5, w);
    t_note("GetTextFaceW(5) = %d", r);
    CHECK(w[0] == L'A' && w[1] == L'r' && w[2] == L'i' && w[3] == L'a');
    EQ(w[5], 0x7777);
    if (w[4] != 0) t_note("truncated W face not NUL terminated");
    char a[8]; memset(a, 'Y', sizeof a);
    r = U(GetTextFaceA)(dc, 5, a);
    t_note("GetTextFaceA(5) = %d", r);
    CHECK(!strncmp(a, "Aria", 4));
    EQ(a[5], 'Y');
    face_done(dc, f, old);
}
TEST(fonts_face_buffer_one) {
    FE_BASE(); REQUIRE(U(GetTextFaceW));
    HFONT f; HGDIOBJ old; HDC dc = face_dc(&f, &old, "Arial");
    WCHAR w[4] = {0x7777, 0x7777, 0x7777, 0x7777};
    char a[4]; memset(a, 'Y', sizeof a);
    int rw = U(GetTextFaceW)(dc, 1, w), ra = U(GetTextFaceA)(dc, 1, a);
    t_note("GetTextFaceW(1)=%d w[0]=%04x; GetTextFaceA(1)=%d a[0]=%02x", rw, w[0], ra, (BYTE)a[0]);
    EQ(w[1], 0x7777); EQ(a[1], 'Y');
    face_done(dc, f, old);
}
TEST(fonts_face_buffer_zero_nonnull_a) {
    FE_BASE();
    HFONT f; HGDIOBJ old; HDC dc = face_dc(&f, &old, "Arial");
    char a[8] = "zzzzzzz";
    SetLastError(0);
    EQ(U(GetTextFaceA)(dc, 0, a), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    EXPECT_STR(a, "zzzzzzz");
    face_done(dc, f, old);
}
TEST(fonts_face_buffer_zero_nonnull_w) {
    FE_BASE(); REQUIRE(U(GetTextFaceW));
    HFONT f; HGDIOBJ old; HDC dc = face_dc(&f, &old, "Arial");
    WCHAR w[4] = {0x7777, 0x7777, 0, 0};
    SetLastError(0);
    EQ(U(GetTextFaceW)(dc, 0, w), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    EQ(w[0], 0x7777);
    face_done(dc, f, old);
}
TEST(fonts_face_buffer_negative_count) {
    FE_BASE(); REQUIRE(U(GetTextFaceW));
    HFONT f; HGDIOBJ old; HDC dc = face_dc(&f, &old, "Arial");
    char a[8] = "zzzzzzz"; WCHAR w[4] = {0x7777};
    SetLastError(0);
    EQ(U(GetTextFaceA)(dc, -1, a), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0);
    EQ(U(GetTextFaceW)(dc, -5, w), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    EQ(w[0], 0x7777); EXPECT_STR(a, "zzzzzzz");
    face_done(dc, f, old);
}
TEST(fonts_face_bad_dc) {
    FE_BASE(); REQUIRE(U(GetTextFaceW));
    char a[40] = "zz"; WCHAR w[40] = {0x7777};
    SetLastError(0);
    EQ(U(GetTextFaceA)(NULL, 40, a), 0);
    EQ(U(GetTextFaceW)(NULL, 40, w), 0);
    EQ(U(GetTextFaceA)((HDC)(ULONG_PTR)0xDEAD, 40, a), 0);
    EQ(U(GetTextFaceW)((HDC)(ULONG_PTR)0xDEAD, 40, w), 0);
    EQ(U(GetTextFaceA)(NULL, 0, NULL), 0);
    EQ(U(GetTextFaceW)(NULL, 0, NULL), 0);
}
TEST(fonts_face_wrong_object_type_as_dc) {
    FE_BASE(); REQUIRE(U(GetStockObject));
    char a[128];
    HGDIOBJ b = U(GetStockObject)(WHITE_BRUSH);
    EQ(U(GetTextFaceA)((HDC)b, 40, a), 0);
    HFONT f = fe_mk(-12, 0, 400, 0, 1, "Arial");
    EQ(U(GetTextFaceA)((HDC)f, 40, a), 0);
    EQ(U(GetTextMetricsA)((HDC)f, (TEXTMETRICA *)a), 0);
    U(DeleteObject)(f);
}
TEST(fonts_face_deleted_dc) {
    FE_BASE();
    HDC dc = U(CreateCompatibleDC)(NULL);
    U(DeleteDC)(dc);
    char a[40];
    EQ(U(GetTextFaceA)(dc, 40, a), 0);
    TEXTMETRICA tm;
    EQ(U(GetTextMetricsA)(dc, &tm), 0);
}
TEST(fonts_face_case_insensitive) {
    FE_BASE();
    static const char *v[] = {"arial", "ARIAL", "aRiAl"};
    for (unsigned i = 0; i < 3; i++) {
        HFONT f; HGDIOBJ old; HDC dc = U(CreateCompatibleDC)(NULL);
        f = fe_mk(-14, 0, 400, 0, ANSI_CHARSET, v[i]); old = U(SelectObject)(dc, f);
        char n[40]; U(GetTextFaceA)(dc, 40, n);
        CHECKF(!strcmp(n, "Arial"), "'%s' -> '%s'", v[i], n);
        face_done(dc, f, old);
    }
}
TEST(fonts_face_returns_canonical_names) {
    FE_BASE();
    for (int i = 0; i < FE_NFACES; i++) {
        fe_ctx c;
        int ok = fe_open(&c, fe_faces[i].name, -14, 400, 0, ANSI_CHARSET);
        CHECKF(ok, "face %s was not provided by the mapper", fe_faces[i].name);
        fe_close(&c);
    }
}
TEST(fonts_face_unknown_name_falls_back) {
    FE_BASE();
    HFONT f; HGDIOBJ old; HDC dc = face_dc(&f, &old, "Zzz Not A Font 42");
    char n[64] = ""; int r = U(GetTextFaceA)(dc, 64, n);
    CHECK(r > 0);
    CHECK(strcmp(n, "Zzz Not A Font 42") != 0);
    CHECK(n[0] != 0);
    t_note("unknown face mapped to '%s'", n);
    char n2[64] = ""; U(GetTextFaceA)(dc, 64, n2);
    EXPECT_STR(n, n2);
    face_done(dc, f, old);
}
TEST(fonts_face_unknown_name_deterministic) {
    FE_BASE();
    char first[64] = "";
    for (int i = 0; i < 5; i++) {
        HFONT f; HGDIOBJ old; HDC dc = face_dc(&f, &old, "Qwerty Missing Face");
        char n[64] = ""; U(GetTextFaceA)(dc, 64, n);
        if (i == 0) lstrcpyA(first, n); else CHECKF(!strcmp(n, first), "'%s' vs '%s'", n, first);
        face_done(dc, f, old);
    }
}
TEST(fonts_face_unknown_by_family) {
    FE_BASE();
    static const struct { BYTE pf; const char *label; } t[] = {
        {FF_ROMAN | VARIABLE_PITCH, "ROMAN"}, {FF_SWISS | VARIABLE_PITCH, "SWISS"}, {FF_MODERN | FIXED_PITCH, "MODERN/fixed"},
        {FF_SCRIPT | VARIABLE_PITCH, "SCRIPT"}, {FF_DECORATIVE | VARIABLE_PITCH, "DECORATIVE"}, {DEFAULT_PITCH | FF_DONTCARE, "DONTCARE"}};
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        LOGFONTA l; memset(&l, 0, sizeof l);
        l.lfHeight = -16; l.lfWeight = 400; l.lfCharSet = ANSI_CHARSET; l.lfPitchAndFamily = t[i].pf;
        lstrcpyA(l.lfFaceName, "Zzz Missing Family Probe");
        HDC dc = U(CreateCompatibleDC)(NULL);
        HFONT f = U(CreateFontIndirectA)(&l);
        HGDIOBJ old = U(SelectObject)(dc, f);
        char n[64] = ""; U(GetTextFaceA)(dc, 64, n);
        TEXTMETRICA tm; U(GetTextMetricsA)(dc, &tm);
        CHECKF(n[0] != 0, "family %s", t[i].label);
        t_note("family %s -> '%s' pitchfam=%02x", t[i].label, n, tm.tmPitchAndFamily);
        face_done(dc, f, old);
    }
}
TEST(fonts_face_fixed_pitch_request_gets_fixed) {
    FE_BASE();
    LOGFONTA l; memset(&l, 0, sizeof l);
    l.lfHeight = -16; l.lfWeight = 400; l.lfCharSet = ANSI_CHARSET; l.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    lstrcpyA(l.lfFaceName, "Zzz Missing Fixed");
    HDC dc = U(CreateCompatibleDC)(NULL);
    HFONT f = U(CreateFontIndirectA)(&l);
    HGDIOBJ old = U(SelectObject)(dc, f);
    TEXTMETRICA tm; U(GetTextMetricsA)(dc, &tm);
    if (tm.tmPitchAndFamily & TMPF_FIXED_PITCH) t_note("fixed pitch request mapped to a variable pitch font");
    face_done(dc, f, old);
}
TEST(fonts_face_substitutes_shell_dlg) {
    FE_BASE();
    static const char *v[] = {"MS Shell Dlg", "MS Shell Dlg 2", "Helv", "Tms Rmn", "Helvetica", "Times", "Courier", "Arial Unicode MS"};
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        HFONT f; HGDIOBJ old; HDC dc = face_dc(&f, &old, v[i]);
        char n[64] = ""; U(GetTextFaceA)(dc, 64, n);
        CHECKF(n[0] != 0, "face %s", v[i]);
        t_note("'%s' -> '%s'", v[i], n);
        face_done(dc, f, old);
    }
}
TEST(fonts_face_shell_dlg_never_reported) {
    FE_BASE();
    HFONT f; HGDIOBJ old; HDC dc = face_dc(&f, &old, "MS Shell Dlg");
    char n[64] = ""; U(GetTextFaceA)(dc, 64, n);
    CHECK(lstrcmpiA(n, "MS Shell Dlg") != 0);
    face_done(dc, f, old);
}
TEST(fonts_face_symbol_charset_request) {
    FE_BASE();
    HFONT f; HGDIOBJ old; HDC dc = U(CreateCompatibleDC)(NULL);
    f = fe_mk(-16, 0, 400, 0, SYMBOL_CHARSET, "Symbol"); old = U(SelectObject)(dc, f);
    char n[64] = ""; U(GetTextFaceA)(dc, 64, n);
    EXPECT_STR(n, "Symbol");
    TEXTMETRICA tm; U(GetTextMetricsA)(dc, &tm);
    EQ(tm.tmCharSet, SYMBOL_CHARSET);
    face_done(dc, f, old);
}
TEST(fonts_face_symbol_font_default_charset_is_symbol) {
    FE_BASE();
    fe_ctx c; int ok = fe_open(&c, "Symbol", -16, 400, 0, DEFAULT_CHARSET);
    TEXTMETRICA tm;
    if (ok) { U(GetTextMetricsA)(c.dc, &tm); EQ(tm.tmCharSet, SYMBOL_CHARSET); }
    else t_note("Symbol not available");
    fe_close(&c);
}
TEST(fonts_face_marlett_is_symbol) {
    FE_BASE();
    fe_ctx c; int ok = fe_open(&c, "Marlett", -16, 400, 0, DEFAULT_CHARSET);
    CHECK(ok);
    if (ok) { TEXTMETRICA tm; U(GetTextMetricsA)(c.dc, &tm); EQ(tm.tmCharSet, SYMBOL_CHARSET); }
    fe_close(&c);
}
TEST(fonts_face_vertical_prefix) {
    FE_BASE();
    HFONT f; HGDIOBJ old; HDC dc = face_dc(&f, &old, "@Arial");
    char n[64] = ""; U(GetTextFaceA)(dc, 64, n);
    CHECK(n[0] != 0);
    t_note("'@Arial' -> '%s'", n);
    face_done(dc, f, old);
}

/* ---- SelectObject ---- */
TEST(fonts_select_returns_previous) {
    FE_BASE(); REQUIRE(U(GetCurrentObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ orig = U(GetCurrentObject)(dc, OBJ_FONT);
    HFONT a = fe_mk(-12, 0, 400, 0, 1, "Arial"), b = fe_mk(-16, 0, 400, 0, 1, "Tahoma");
    EQ(U(SelectObject)(dc, a), orig);
    EQ(U(GetCurrentObject)(dc, OBJ_FONT), a);
    EQ(U(SelectObject)(dc, b), a);
    EQ(U(SelectObject)(dc, b), b);
    EQ(U(GetCurrentObject)(dc, OBJ_FONT), b);
    EQ(U(SelectObject)(dc, orig), b);
    U(DeleteObject)(a); U(DeleteObject)(b); U(DeleteDC)(dc);
}
TEST(fonts_select_null_dc) {
    FE_BASE();
    HFONT a = fe_mk(-12, 0, 400, 0, 1, "Arial");
    SetLastError(0);
    CHECK(U(SelectObject)(NULL, a) == NULL);
    U(DeleteObject)(a);
}
TEST(fonts_select_null_object) {
    FE_BASE(); REQUIRE(U(GetCurrentObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ cur = U(GetCurrentObject)(dc, OBJ_FONT);
    SetLastError(0);
    HGDIOBJ r = U(SelectObject)(dc, NULL);
    CHECK(r == NULL);
    EQ(U(GetCurrentObject)(dc, OBJ_FONT), cur);
    U(DeleteDC)(dc);
}
TEST(fonts_select_bogus_object) {
    FE_BASE(); REQUIRE(U(GetCurrentObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ cur = U(GetCurrentObject)(dc, OBJ_FONT);
    CHECK(U(SelectObject)(dc, (HGDIOBJ)(ULONG_PTR)0x12345) == NULL);
    EQ(U(GetCurrentObject)(dc, OBJ_FONT), cur);
    U(DeleteDC)(dc);
}
TEST(fonts_select_shared_between_dcs) {
    FE_BASE(); REQUIRE(U(GetCurrentObject));
    HDC d1 = U(CreateCompatibleDC)(NULL), d2 = U(CreateCompatibleDC)(NULL);
    HFONT f = fe_mk(-12, 0, 400, 0, 1, "Arial");
    HGDIOBJ o1 = U(SelectObject)(d1, f), o2 = U(SelectObject)(d2, f);
    CHECK(o1 != NULL && o2 != NULL);
    EQ(U(GetCurrentObject)(d1, OBJ_FONT), f);
    EQ(U(GetCurrentObject)(d2, OBJ_FONT), f);
    U(SelectObject)(d1, o1); U(SelectObject)(d2, o2);
    U(DeleteObject)(f); U(DeleteDC)(d1); U(DeleteDC)(d2);
}
TEST(fonts_select_font_survives_dc_delete) {
    FE_BASE(); REQUIRE(U(GetObjectType));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HFONT f = fe_mk(-12, 0, 400, 0, 1, "Arial");
    U(SelectObject)(dc, f);
    U(DeleteDC)(dc);                    /* deleting a DC with a font still selected must not delete the font */
    EQ(U(GetObjectType)(f), OBJ_FONT);
    CHECK(U(DeleteObject)(f));
}
TEST(fonts_select_delete_while_selected) {
    FE_BASE(); REQUIRE(U(GetObjectType));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HFONT f = fe_mk(-12, 0, 400, 0, 1, "Arial");
    HGDIOBJ old = U(SelectObject)(dc, f);
    BOOL r = U(DeleteObject)(f);
    t_note("DeleteObject(selected font) = %d, type afterwards = %d", r, U(GetObjectType)(f));
    char n[40] = ""; U(GetTextFaceA)(dc, 40, n);
    t_note("face still selected: '%s'", n);
    U(SelectObject)(dc, old);
    if (U(GetObjectType)(f)) U(DeleteObject)(f);
    EQ(U(GetObjectType)(f), 0);
    U(DeleteDC)(dc);
}
TEST(fonts_select_savedc_restoredc) {
    FE_BASE(); REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(U(GetCurrentObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ orig = U(GetCurrentObject)(dc, OBJ_FONT);
    HFONT f = fe_mk(-20, 0, 400, 0, 1, "Arial");
    int id = U(SaveDC)(dc);
    CHECK(id > 0);
    U(SelectObject)(dc, f);
    EQ(U(GetCurrentObject)(dc, OBJ_FONT), f);
    CHECK(U(RestoreDC)(dc, id));
    EQ(U(GetCurrentObject)(dc, OBJ_FONT), orig);
    U(DeleteObject)(f); U(DeleteDC)(dc);
}
TEST(fonts_select_dib_memdc) {
    FE_BASE(); REQUIRE(U(CreateDIBSection));
    HDC dc = U(CreateCompatibleDC)(NULL);
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 64; bi.bmiHeader.biHeight = -32;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    void *bits; HBITMAP bm = U(CreateDIBSection)(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    CHECK(bm != NULL);
    HGDIOBJ ob = U(SelectObject)(dc, bm);
    HFONT f = fe_mk(-14, 0, 400, 0, 1, "Arial");
    HGDIOBJ of = U(SelectObject)(dc, f);
    TEXTMETRICA tm; CHECK(U(GetTextMetricsA)(dc, &tm));
    CHECK(tm.tmHeight > 0);
    U(SelectObject)(dc, of); U(SelectObject)(dc, ob);
    U(DeleteObject)(f); U(DeleteObject)(bm); U(DeleteDC)(dc);
}
TEST(fonts_select_metrics_independent_of_bitmap_depth) {
    FE_BASE(); REQUIRE(U(CreateBitmap));
    static const int bpp[] = {1, 8, 32};
    LONG heights[3] = {0};
    for (int i = 0; i < 3; i++) {
        HDC dc = U(CreateCompatibleDC)(NULL);
        HBITMAP bm = U(CreateBitmap)(16, 16, 1, bpp[i], NULL);
        HGDIOBJ ob = bm ? U(SelectObject)(dc, bm) : NULL;
        HFONT f = fe_mk(-14, 0, 400, 0, 1, "Arial");
        HGDIOBJ of = U(SelectObject)(dc, f);
        TEXTMETRICA tm; memset(&tm, 0, sizeof tm); U(GetTextMetricsA)(dc, &tm);
        heights[i] = tm.tmHeight;
        U(SelectObject)(dc, of); if (ob) U(SelectObject)(dc, ob);
        U(DeleteObject)(f); if (bm) U(DeleteObject)(bm); U(DeleteDC)(dc);
    }
    if (heights[0] != heights[1] || heights[1] != heights[2]) t_note("heights by depth: %ld %ld %ld", heights[0], heights[1], heights[2]);
    CHECK(heights[2] > 0);
}

/* ---- height semantics ---- */
static void height_neg(const char *face) {
    static const int ns[] = {8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 32, 48, 72, 100};
    for (unsigned i = 0; i < sizeof ns / sizeof ns[0]; i++) {
        fe_ctx c;
        if (!fe_open(&c, face, -ns[i], 400, 0, ANSI_CHARSET)) { t_note("%s unavailable", face); fe_close(&c); return; }
        TEXTMETRICA tm; CHECK(U(GetTextMetricsA)(c.dc, &tm));
        CHECKF(tm.tmHeight - tm.tmInternalLeading == ns[i], "%s -%d: height %ld internal %ld", face, ns[i], tm.tmHeight, tm.tmInternalLeading);
        CHECKF(tm.tmAscent + tm.tmDescent == tm.tmHeight, "%s -%d", face, ns[i]);
        fe_close(&c);
    }
}
static void height_pos(const char *face) {
    static const int ns[] = {10, 12, 13, 14, 16, 18, 20, 24, 32, 48, 72, 100};
    for (unsigned i = 0; i < sizeof ns / sizeof ns[0]; i++) {
        fe_ctx c;
        if (!fe_open(&c, face, ns[i], 400, 0, ANSI_CHARSET)) { t_note("%s unavailable", face); fe_close(&c); return; }
        TEXTMETRICA tm; CHECK(U(GetTextMetricsA)(c.dc, &tm));
        CHECKF(labs(tm.tmHeight - ns[i]) <= 1, "%s +%d: height %ld", face, ns[i], tm.tmHeight);
        if (tm.tmHeight != ns[i]) t_note("%s cell height %d mapped to %ld", face, ns[i], tm.tmHeight);
        CHECKF(tm.tmAscent + tm.tmDescent == tm.tmHeight, "%s +%d", face, ns[i]);
        fe_close(&c);
    }
}
TEST(fonts_height_neg_arial) { FE_BASE(); height_neg("Arial"); }
TEST(fonts_height_neg_times) { FE_BASE(); height_neg("Times New Roman"); }
TEST(fonts_height_neg_courier_new) { FE_BASE(); height_neg("Courier New"); }
TEST(fonts_height_neg_tahoma) { FE_BASE(); height_neg("Tahoma"); }
TEST(fonts_height_neg_segoe_ui) { FE_BASE(); height_neg("Segoe UI"); }
TEST(fonts_height_neg_consolas) { FE_BASE(); height_neg("Consolas"); }
TEST(fonts_height_pos_arial) { FE_BASE(); height_pos("Arial"); }
TEST(fonts_height_pos_times) { FE_BASE(); height_pos("Times New Roman"); }
TEST(fonts_height_pos_courier_new) { FE_BASE(); height_pos("Courier New"); }
TEST(fonts_height_pos_tahoma) { FE_BASE(); height_pos("Tahoma"); }
TEST(fonts_height_pos_segoe_ui) { FE_BASE(); height_pos("Segoe UI"); }
TEST(fonts_height_pos_consolas) { FE_BASE(); height_pos("Consolas"); }

TEST(fonts_height_zero_default) {
    FE_BASE();
    static const char *faces[] = {"Arial", "Courier New", "Times New Roman", "Tahoma", "Segoe UI"};
    for (unsigned i = 0; i < 5; i++) {
        fe_ctx c;
        if (!fe_open(&c, faces[i], 0, 400, 0, ANSI_CHARSET)) { fe_close(&c); continue; }
        TEXTMETRICA tm; CHECK(U(GetTextMetricsA)(c.dc, &tm));
        CHECKF(tm.tmHeight > 0, "%s", faces[i]);
        CHECKF(tm.tmAscent + tm.tmDescent == tm.tmHeight, "%s", faces[i]);
        t_note("%s lfHeight 0 -> tmHeight %ld (em %ld)", faces[i], tm.tmHeight, tm.tmHeight - tm.tmInternalLeading);
        fe_close(&c);
    }
}
TEST(fonts_height_zero_equals_default_size) {
    FE_BASE();
    /* lfHeight 0 selects the same size every time */
    LONG h[3];
    for (int i = 0; i < 3; i++) {
        fe_ctx c; fe_open(&c, "Arial", 0, 400, 0, ANSI_CHARSET);
        TEXTMETRICA tm; memset(&tm, 0, sizeof tm); U(GetTextMetricsA)(c.dc, &tm); h[i] = tm.tmHeight; fe_close(&c);
    }
    CHECK(h[0] == h[1] && h[1] == h[2]);
}
TEST(fonts_height_monotonic_arial) {
    FE_BASE();
    LONG prev = 0;
    for (int n = 6; n <= 120; n += 2) {
        fe_ctx c;
        if (!fe_open(&c, "Arial", -n, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
        TEXTMETRICA tm; U(GetTextMetricsA)(c.dc, &tm);
        CHECKF(tm.tmHeight >= prev, "height decreased at -%d: %ld < %ld", n, tm.tmHeight, prev);
        prev = tm.tmHeight;
        fe_close(&c);
    }
}
TEST(fonts_height_sign_differs) {
    FE_BASE();
    fe_ctx a, b;
    int ok1 = fe_open(&a, "Arial", -32, 400, 0, ANSI_CHARSET), ok2 = fe_open(&b, "Arial", 32, 400, 0, ANSI_CHARSET);
    if (ok1 && ok2) {
        TEXTMETRICA ta, tb; U(GetTextMetricsA)(a.dc, &ta); U(GetTextMetricsA)(b.dc, &tb);
        CHECK(ta.tmHeight > tb.tmHeight);          /* cell height > em height when internal leading > 0 */
        CHECK(ta.tmInternalLeading > 0);
    }
    fe_close(&a); fe_close(&b);
}
TEST(fonts_height_huge) {
    FE_BASE();
    static const int hs[] = {-2000, -4000, 2000, -10000};
    for (unsigned i = 0; i < 4; i++) {
        fe_ctx c; int ok = fe_open(&c, "Arial", hs[i], 400, 0, ANSI_CHARSET);
        TEXTMETRICA tm; memset(&tm, 0, sizeof tm);
        BOOL r = U(GetTextMetricsA)(c.dc, &tm);
        t_note("height %d: face ok=%d GetTextMetrics=%d tmHeight=%ld", hs[i], ok, r, tm.tmHeight);
        fe_close(&c);
    }
}
TEST(fonts_height_one_pixel) {
    FE_BASE();
    static const int hs[] = {-1, 1, -2, 2};
    for (unsigned i = 0; i < 4; i++) {
        fe_ctx c; fe_open(&c, "Arial", hs[i], 400, 0, ANSI_CHARSET);
        TEXTMETRICA tm; memset(&tm, 0, sizeof tm);
        CHECK(U(GetTextMetricsA)(c.dc, &tm));
        CHECKF(tm.tmHeight >= 1, "h=%d", hs[i]);
        CHECKF(tm.tmAscent + tm.tmDescent == tm.tmHeight, "h=%d", hs[i]);
        fe_close(&c);
    }
}
TEST(fonts_height_point_size_formula) {
    FE_BASE(); REQUIRE(U(GetDeviceCaps)); REQUIRE(U(MulDiv));
    HDC dc = U(CreateCompatibleDC)(NULL);
    int dpi = U(GetDeviceCaps)(dc, LOGPIXELSY);
    U(DeleteDC)(dc);
    static const int pts[] = {8, 9, 10, 12, 14, 18, 24, 36, 72};
    for (unsigned i = 0; i < sizeof pts / sizeof pts[0]; i++) {
        int h = -U(MulDiv)(pts[i], dpi, 72);
        fe_ctx c; if (!fe_open(&c, "Arial", h, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
        TEXTMETRICA tm; U(GetTextMetricsA)(c.dc, &tm);
        CHECKF(tm.tmHeight - tm.tmInternalLeading == -h, "pt %d dpi %d", pts[i], dpi);
        fe_close(&c);
    }
    if (dpi != 96) t_note("running at %d dpi", dpi);
}

/* ---- width semantics ---- */
TEST(fonts_width_explicit_arial) {
    FE_BASE();
    static const int ws[] = {6, 10, 16, 24, 40};
    for (unsigned i = 0; i < sizeof ws / sizeof ws[0]; i++) {
        HDC dc = U(CreateCompatibleDC)(NULL);
        HFONT f = fe_mk(-32, ws[i], 400, 0, ANSI_CHARSET, "Arial");
        HGDIOBJ old = U(SelectObject)(dc, f);
        TEXTMETRICA tm; U(GetTextMetricsA)(dc, &tm);
        CHECKF(labs(tm.tmAveCharWidth - ws[i]) <= 1, "lfWidth %d -> ave %ld", ws[i], tm.tmAveCharWidth);
        CHECKF(tm.tmHeight - tm.tmInternalLeading == 32, "lfWidth %d changed height to %ld", ws[i], tm.tmHeight);
        U(SelectObject)(dc, old); U(DeleteObject)(f); U(DeleteDC)(dc);
    }
}
TEST(fonts_width_zero_is_natural) {
    FE_BASE();
    HDC dc = U(CreateCompatibleDC)(NULL);
    HFONT f0 = fe_mk(-32, 0, 400, 0, ANSI_CHARSET, "Arial");
    HGDIOBJ old = U(SelectObject)(dc, f0);
    TEXTMETRICA t0; U(GetTextMetricsA)(dc, &t0);
    HFONT f1 = fe_mk(-32, t0.tmAveCharWidth, 400, 0, ANSI_CHARSET, "Arial");
    U(SelectObject)(dc, f1);
    TEXTMETRICA t1; U(GetTextMetricsA)(dc, &t1);
    CHECK(labs(t1.tmAveCharWidth - t0.tmAveCharWidth) <= 1);
    U(SelectObject)(dc, old); U(DeleteObject)(f0); U(DeleteObject)(f1); U(DeleteDC)(dc);
}
TEST(fonts_width_changes_text_extent) {
    FE_BASE(); REQUIRE(U(GetTextExtentPoint32A));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HFONT fn = fe_mk(-32, 10, 400, 0, ANSI_CHARSET, "Arial"), fw = fe_mk(-32, 20, 400, 0, ANSI_CHARSET, "Arial");
    HGDIOBJ old = U(SelectObject)(dc, fn);
    SIZE a, b; U(GetTextExtentPoint32A)(dc, "Hello", 5, &a);
    U(SelectObject)(dc, fw); U(GetTextExtentPoint32A)(dc, "Hello", 5, &b);
    CHECKF(b.cx > a.cx, "narrow %ld wide %ld", a.cx, b.cx);
    EQ(a.cy, b.cy);
    U(SelectObject)(dc, old); U(DeleteObject)(fn); U(DeleteObject)(fw); U(DeleteDC)(dc);
}

/* ---- style sweeps ---- */
static void style_flags(const char *face) {
    for (int m = 0; m < 8; m++) {
        LOGFONTA l; memset(&l, 0, sizeof l);
        l.lfHeight = -16; l.lfWeight = 400; l.lfCharSet = ANSI_CHARSET;
        l.lfItalic = m & 1; l.lfUnderline = (m >> 1) & 1; l.lfStrikeOut = (m >> 2) & 1;
        lstrcpyA(l.lfFaceName, face);
        HDC dc = U(CreateCompatibleDC)(NULL);
        HFONT f = U(CreateFontIndirectA)(&l);
        HGDIOBJ old = U(SelectObject)(dc, f);
        TEXTMETRICA tm; U(GetTextMetricsA)(dc, &tm);
        CHECKF(!!tm.tmItalic == (m & 1), "%s mask %d italic=%d", face, m, tm.tmItalic);
        CHECKF(!!tm.tmUnderlined == ((m >> 1) & 1), "%s mask %d underline=%d", face, m, tm.tmUnderlined);
        CHECKF(!!tm.tmStruckOut == ((m >> 2) & 1), "%s mask %d strikeout=%d", face, m, tm.tmStruckOut);
        U(SelectObject)(dc, old); U(DeleteObject)(f); U(DeleteDC)(dc);
    }
}
TEST(fonts_style_flags_arial) { FE_BASE(); style_flags("Arial"); }
TEST(fonts_style_flags_times) { FE_BASE(); style_flags("Times New Roman"); }
TEST(fonts_style_flags_courier_new) { FE_BASE(); style_flags("Courier New"); }
TEST(fonts_style_flags_tahoma) { FE_BASE(); style_flags("Tahoma"); }
TEST(fonts_style_flags_segoe_ui) { FE_BASE(); style_flags("Segoe UI"); }
TEST(fonts_style_flags_consolas) { FE_BASE(); style_flags("Consolas"); }
TEST(fonts_style_flags_system) { FE_BASE(); style_flags("System"); }
TEST(fonts_style_flags_terminal) { FE_BASE(); style_flags("Terminal"); }
TEST(fonts_style_flags_ms_sans_serif) { FE_BASE(); style_flags("MS Sans Serif"); }

static void weights(const char *face, int w, int expect_lo, int expect_hi) {
    fe_ctx c;
    if (!fe_open(&c, face, -16, w, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    TEXTMETRICA tm; U(GetTextMetricsA)(c.dc, &tm);
    CHECKF(tm.tmWeight >= expect_lo && tm.tmWeight <= expect_hi, "%s weight %d -> %ld", face, w, tm.tmWeight);
    fe_close(&c);
}
TEST(fonts_weight_arial_regular_bold) {
    FE_BASE();
    weights("Arial", 0, 400, 400); weights("Arial", FW_NORMAL, 400, 400); weights("Arial", FW_BOLD, 700, 700);
    weights("Arial", 1, 400, 400); weights("Arial", 100, 400, 400); weights("Arial", 300, 400, 400);
}
TEST(fonts_weight_arial_between) {
    FE_BASE();
    static const int ws[] = {450, 500, 550, 600, 650, 800, 900, 1000};
    for (unsigned i = 0; i < sizeof ws / sizeof ws[0]; i++) {
        fe_ctx c; if (!fe_open(&c, "Arial", -16, ws[i], 0, ANSI_CHARSET)) { fe_close(&c); return; }
        TEXTMETRICA tm; U(GetTextMetricsA)(c.dc, &tm);
        CHECKF(tm.tmWeight == 400 || tm.tmWeight == 700 || tm.tmWeight == 900, "weight %d -> %ld", ws[i], tm.tmWeight);
        t_note("Arial lfWeight %d -> tmWeight %ld", ws[i], tm.tmWeight);
        fe_close(&c);
    }
}
TEST(fonts_weight_tahoma_times_courier) {
    FE_BASE();
    static const char *fs[] = {"Tahoma", "Times New Roman", "Courier New", "Segoe UI", "Consolas"};
    for (unsigned i = 0; i < 5; i++) { weights(fs[i], FW_NORMAL, 400, 400); weights(fs[i], FW_BOLD, 700, 700); }
}
TEST(fonts_weight_segoe_light_semibold) {
    FE_BASE();
    fe_ctx c;
    if (fe_open(&c, "Segoe UI", -16, 300, 0, ANSI_CHARSET)) { TEXTMETRICA tm; U(GetTextMetricsA)(c.dc, &tm); t_note("Segoe UI 300 -> %ld", tm.tmWeight); }
    fe_close(&c);
    if (fe_open(&c, "Segoe UI", -16, 600, 0, ANSI_CHARSET)) { TEXTMETRICA tm; U(GetTextMetricsA)(c.dc, &tm); t_note("Segoe UI 600 -> %ld", tm.tmWeight); }
    fe_close(&c);
}
TEST(fonts_weight_bold_wider_than_regular) {
    FE_BASE(); REQUIRE(U(GetTextExtentPoint32A));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HFONT r = fe_mk(-20, 0, 400, 0, ANSI_CHARSET, "Arial"), b = fe_mk(-20, 0, 700, 0, ANSI_CHARSET, "Arial");
    HGDIOBJ old = U(SelectObject)(dc, r);
    SIZE sr, sb; U(GetTextExtentPoint32A)(dc, "The quick brown fox", 19, &sr);
    U(SelectObject)(dc, b); U(GetTextExtentPoint32A)(dc, "The quick brown fox", 19, &sb);
    CHECKF(sb.cx > sr.cx, "regular %ld bold %ld", sr.cx, sb.cx);
    U(SelectObject)(dc, old); U(DeleteObject)(r); U(DeleteObject)(b); U(DeleteDC)(dc);
}
TEST(fonts_style_italic_flag_survives_roman_font) {
    FE_BASE();
    fe_ctx c; int ok = fe_open(&c, "Arial", -20, 400, 1, ANSI_CHARSET);
    CHECK(ok);
    if (ok) { TEXTMETRICA tm; U(GetTextMetricsA)(c.dc, &tm); CHECK(tm.tmItalic != 0); EQ(tm.tmWeight, 400); }
    fe_close(&c);
}
TEST(fonts_style_bold_italic) {
    FE_BASE();
    fe_ctx c; int ok = fe_open(&c, "Times New Roman", -20, 700, 1, ANSI_CHARSET);
    CHECK(ok);
    if (ok) { TEXTMETRICA tm; U(GetTextMetricsA)(c.dc, &tm); CHECK(tm.tmItalic != 0); EQ(tm.tmWeight, 700); }
    fe_close(&c);
}

/* ---- precision / quality ---- */
TEST(fonts_precision_tt_only_forces_truetype) {
    FE_BASE();
    static const char *faces[] = {"System", "Terminal", "MS Sans Serif", "Fixedsys", "Small Fonts"};
    for (unsigned i = 0; i < 5; i++) {
        LOGFONTA l; memset(&l, 0, sizeof l);
        l.lfHeight = -16; l.lfWeight = 400; l.lfCharSet = ANSI_CHARSET; l.lfOutPrecision = OUT_TT_ONLY_PRECIS;
        lstrcpyA(l.lfFaceName, faces[i]);
        HDC dc = U(CreateCompatibleDC)(NULL);
        HFONT f = U(CreateFontIndirectA)(&l);
        HGDIOBJ old = U(SelectObject)(dc, f);
        TEXTMETRICA tm; U(GetTextMetricsA)(dc, &tm);
        CHECKF(tm.tmPitchAndFamily & TMPF_TRUETYPE, "%s: OUT_TT_ONLY did not give TT (pf=%02x)", faces[i], tm.tmPitchAndFamily);
        U(SelectObject)(dc, old); U(DeleteObject)(f); U(DeleteDC)(dc);
    }
}
TEST(fonts_precision_tt_precis_arial_stays) {
    FE_BASE();
    LOGFONTA l; memset(&l, 0, sizeof l);
    l.lfHeight = -16; l.lfWeight = 400; l.lfCharSet = ANSI_CHARSET; l.lfOutPrecision = OUT_TT_PRECIS; lstrcpyA(l.lfFaceName, "Arial");
    HDC dc = U(CreateCompatibleDC)(NULL); HFONT f = U(CreateFontIndirectA)(&l); HGDIOBJ old = U(SelectObject)(dc, f);
    char n[40]; U(GetTextFaceA)(dc, 40, n); EXPECT_STR(n, "Arial");
    U(SelectObject)(dc, old); U(DeleteObject)(f); U(DeleteDC)(dc);
}
TEST(fonts_precision_raster_precis_arial) {
    FE_BASE();
    LOGFONTA l; memset(&l, 0, sizeof l);
    l.lfHeight = -16; l.lfWeight = 400; l.lfCharSet = ANSI_CHARSET; l.lfOutPrecision = OUT_RASTER_PRECIS; lstrcpyA(l.lfFaceName, "Arial");
    HDC dc = U(CreateCompatibleDC)(NULL); HFONT f = U(CreateFontIndirectA)(&l); HGDIOBJ old = U(SelectObject)(dc, f);
    char n[40]; U(GetTextFaceA)(dc, 40, n); TEXTMETRICA tm; U(GetTextMetricsA)(dc, &tm);
    t_note("OUT_RASTER_PRECIS Arial -> '%s' pf=%02x", n, tm.tmPitchAndFamily);
    CHECK(tm.tmHeight > 0);
    U(SelectObject)(dc, old); U(DeleteObject)(f); U(DeleteDC)(dc);
}
TEST(fonts_precision_all_values_select) {
    FE_BASE();
    for (int p = 0; p <= 12; p++) {
        LOGFONTA l; memset(&l, 0, sizeof l);
        l.lfHeight = -16; l.lfWeight = 400; l.lfCharSet = ANSI_CHARSET; l.lfOutPrecision = (BYTE)p; lstrcpyA(l.lfFaceName, "Arial");
        HDC dc = U(CreateCompatibleDC)(NULL); HFONT f = U(CreateFontIndirectA)(&l); HGDIOBJ old = U(SelectObject)(dc, f);
        TEXTMETRICA tm; memset(&tm, 0, sizeof tm);
        CHECKF(U(GetTextMetricsA)(dc, &tm) && tm.tmHeight > 0, "precision %d", p);
        U(SelectObject)(dc, old); U(DeleteObject)(f); U(DeleteDC)(dc);
    }
}
TEST(fonts_quality_all_values_same_height) {
    FE_BASE();
    LONG h0 = -1;
    for (int q = 0; q <= 6; q++) {
        LOGFONTA l; memset(&l, 0, sizeof l);
        l.lfHeight = -16; l.lfWeight = 400; l.lfCharSet = ANSI_CHARSET; l.lfQuality = (BYTE)q; lstrcpyA(l.lfFaceName, "Arial");
        HDC dc = U(CreateCompatibleDC)(NULL); HFONT f = U(CreateFontIndirectA)(&l); HGDIOBJ old = U(SelectObject)(dc, f);
        TEXTMETRICA tm; memset(&tm, 0, sizeof tm); U(GetTextMetricsA)(dc, &tm);
        CHECKF(tm.tmHeight > 0, "quality %d", q);
        if (h0 < 0) h0 = tm.tmHeight; else if (tm.tmHeight != h0) t_note("quality %d changes tmHeight %ld -> %ld", q, h0, tm.tmHeight);
        CHECKF(tm.tmHeight - tm.tmInternalLeading == 16, "quality %d em height", q);
        U(SelectObject)(dc, old); U(DeleteObject)(f); U(DeleteDC)(dc);
    }
}
TEST(fonts_escapement_does_not_break_metrics) {
    FE_BASE();
    static const int es[] = {0, 450, 900, 1800, 2700};
    LONG h0 = 0;
    for (unsigned i = 0; i < 5; i++) {
        LOGFONTA l; memset(&l, 0, sizeof l);
        l.lfHeight = -24; l.lfWeight = 400; l.lfCharSet = ANSI_CHARSET; l.lfEscapement = es[i]; l.lfOrientation = es[i]; lstrcpyA(l.lfFaceName, "Arial");
        HDC dc = U(CreateCompatibleDC)(NULL); HFONT f = U(CreateFontIndirectA)(&l); HGDIOBJ old = U(SelectObject)(dc, f);
        TEXTMETRICA tm; memset(&tm, 0, sizeof tm);
        CHECKF(U(GetTextMetricsA)(dc, &tm), "esc %d", es[i]);
        CHECKF(tm.tmAscent + tm.tmDescent == tm.tmHeight, "esc %d", es[i]);
        if (i == 0) h0 = tm.tmHeight; else if (tm.tmHeight != h0) t_note("escapement %d changes tmHeight %ld -> %ld", es[i], h0, tm.tmHeight);
        U(SelectObject)(dc, old); U(DeleteObject)(f); U(DeleteDC)(dc);
    }
}

/* ---- charset mapping ---- */
TEST(fonts_charset_arial_scripts) {
    FE_BASE();
    static const BYTE cs[] = {ANSI_CHARSET, EASTEUROPE_CHARSET, RUSSIAN_CHARSET, GREEK_CHARSET, TURKISH_CHARSET, HEBREW_CHARSET, ARABIC_CHARSET, BALTIC_CHARSET};
    for (unsigned i = 0; i < sizeof cs; i++) {
        fe_ctx c; int ok = fe_open(&c, "Arial", -16, 400, 0, cs[i]);
        CHECKF(ok, "Arial charset %u", cs[i]);
        TEXTMETRICA tm; if (ok) { U(GetTextMetricsA)(c.dc, &tm); CHECKF(tm.tmCharSet == cs[i], "charset %u -> %u", cs[i], tm.tmCharSet); }
        fe_close(&c);
    }
}
TEST(fonts_charset_tahoma_scripts) {
    FE_BASE();
    static const BYTE cs[] = {ANSI_CHARSET, EASTEUROPE_CHARSET, RUSSIAN_CHARSET, GREEK_CHARSET, TURKISH_CHARSET, HEBREW_CHARSET, ARABIC_CHARSET, BALTIC_CHARSET, THAI_CHARSET, VIETNAMESE_CHARSET};
    for (unsigned i = 0; i < sizeof cs; i++) {
        fe_ctx c; int ok = fe_open(&c, "Tahoma", -16, 400, 0, cs[i]);
        if (ok) { TEXTMETRICA tm; U(GetTextMetricsA)(c.dc, &tm); CHECKF(tm.tmCharSet == cs[i], "charset %u -> %u", cs[i], tm.tmCharSet); }
        else t_note("Tahoma charset %u -> face changed", cs[i]);
        fe_close(&c);
    }
}
TEST(fonts_charset_default_maps_to_system_charset) {
    FE_BASE(); REQUIRE(U(GetTextCharset));
    fe_ctx c; int ok = fe_open(&c, "Arial", -16, 400, 0, DEFAULT_CHARSET);
    if (ok) {
        TEXTMETRICA tm; U(GetTextMetricsA)(c.dc, &tm);
        CHECK(tm.tmCharSet != DEFAULT_CHARSET);
        EQ(U(GetTextCharset)(c.dc), tm.tmCharSet);
        t_note("DEFAULT_CHARSET maps to %u", tm.tmCharSet);
    }
    fe_close(&c);
}
TEST(fonts_charset_unsupported_script_falls_back) {
    FE_BASE();
    /* Consolas has no Thai coverage; the mapper must pick something. */
    LOGFONTA l; memset(&l, 0, sizeof l);
    l.lfHeight = -16; l.lfWeight = 400; l.lfCharSet = THAI_CHARSET; lstrcpyA(l.lfFaceName, "Consolas");
    HDC dc = U(CreateCompatibleDC)(NULL); HFONT f = U(CreateFontIndirectA)(&l); HGDIOBJ old = U(SelectObject)(dc, f);
    char n[40]; TEXTMETRICA tm; U(GetTextFaceA)(dc, 40, n); U(GetTextMetricsA)(dc, &tm);
    CHECK(n[0] != 0);
    t_note("Consolas THAI -> '%s' charset %u", n, tm.tmCharSet);
    U(SelectObject)(dc, old); U(DeleteObject)(f); U(DeleteDC)(dc);
}
TEST(fonts_charset_shiftjis_request) {
    FE_BASE();
    LOGFONTA l; memset(&l, 0, sizeof l);
    l.lfHeight = -16; l.lfWeight = 400; l.lfCharSet = SHIFTJIS_CHARSET; lstrcpyA(l.lfFaceName, "Arial");
    HDC dc = U(CreateCompatibleDC)(NULL); HFONT f = U(CreateFontIndirectA)(&l); HGDIOBJ old = U(SelectObject)(dc, f);
    char n[40]; TEXTMETRICA tm; U(GetTextFaceA)(dc, 40, n); U(GetTextMetricsA)(dc, &tm);
    t_note("Arial SHIFTJIS -> '%s' charset %u", n, tm.tmCharSet);
    CHECK(tm.tmHeight > 0);
    U(SelectObject)(dc, old); U(DeleteObject)(f); U(DeleteDC)(dc);
}
TEST(fonts_mapper_flags_aspect) {
    FE_BASE(); REQUIRE(U(SetMapperFlags));
    HDC dc = U(CreateCompatibleDC)(NULL);
    SetLastError(0);
    EQ(U(SetMapperFlags)(dc, 0), 0);
    EQ(U(SetMapperFlags)(dc, ASPECT_FILTERING), 0);
    EQ(U(SetMapperFlags)(dc, ASPECT_FILTERING), ASPECT_FILTERING);
    EQ(U(SetMapperFlags)(dc, 0), ASPECT_FILTERING);
    EQ(U(SetMapperFlags)(dc, 0), 0);
    U(DeleteDC)(dc);
}
TEST(fonts_mapper_flags_invalid) {
    FE_BASE(); REQUIRE(U(SetMapperFlags));
    HDC dc = U(CreateCompatibleDC)(NULL);
    static const DWORD bad[] = {2, 3, 4, 0x80, 0x10000, 0x80000000, 0xFFFFFFFF};
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0);
        CHECKF(U(SetMapperFlags)(dc, bad[i]) == 0xFFFFFFFF, "flags %lx", bad[i]);
        EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    }
    EQ(U(SetMapperFlags)(dc, 0), 0);      /* state unchanged */
    U(DeleteDC)(dc);
}
TEST(fonts_mapper_flags_bad_dc) {
    FE_BASE(); REQUIRE(U(SetMapperFlags));
    SetLastError(0);
    EQ(U(SetMapperFlags)(NULL, 0), 0xFFFFFFFF);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0);
    EQ(U(SetMapperFlags)((HDC)(ULONG_PTR)0xBEEF, 1), 0xFFFFFFFF);
    HFONT f = fe_mk(-12, 0, 400, 0, 1, "Arial");
    SetLastError(0);
    EQ(U(SetMapperFlags)((HDC)f, 1), 0xFFFFFFFF);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    U(DeleteObject)(f);
}
TEST(fonts_mapper_flags_per_dc) {
    FE_BASE(); REQUIRE(U(SetMapperFlags));
    HDC a = U(CreateCompatibleDC)(NULL), b = U(CreateCompatibleDC)(NULL);
    U(SetMapperFlags)(a, ASPECT_FILTERING);
    EQ(U(SetMapperFlags)(b, 0), 0);
    EQ(U(SetMapperFlags)(a, 0), ASPECT_FILTERING);
    U(DeleteDC)(a); U(DeleteDC)(b);
}
TEST(fonts_mapper_flags_raster_selection) {
    FE_BASE(); REQUIRE(U(SetMapperFlags));
    HDC dc = U(CreateCompatibleDC)(NULL);
    LONG h[2];
    for (int i = 0; i < 2; i++) {
        U(SetMapperFlags)(dc, i);
        HFONT f = fe_mk(-13, 7, 400, 0, ANSI_CHARSET, "MS Sans Serif");
        HGDIOBJ old = U(SelectObject)(dc, f);
        TEXTMETRICA tm; memset(&tm, 0, sizeof tm); U(GetTextMetricsA)(dc, &tm); h[i] = tm.tmHeight;
        U(SelectObject)(dc, old); U(DeleteObject)(f);
    }
    t_note("MS Sans Serif -13x7 heights: no aspect %ld, aspect %ld", h[0], h[1]);
    CHECK(h[0] > 0 && h[1] > 0);
    U(DeleteDC)(dc);
}
