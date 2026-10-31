/* Additional edge cases: handle lifetime, selection rules, parity, boundaries. */
#include "fe.h"

TEST(fonts_extra_getobjecta_oversize_clamps) {
    FE_BASE(); REQUIRE(U(GetObjectA));
    HFONT f = fe_mk(-12, 0, 400, 0, ANSI_CHARSET, "Arial");
    CHECK(f != NULL);
    BYTE buf[700]; memset(buf, 0, sizeof buf);
    int r = U(GetObjectA)(f, 300, buf);
    if (fe_wine()) t_note("GetObjectA(300) = %d", r); else EQ(r, 260);
    U(DeleteObject)(f);
}
TEST(fonts_extra_deleted_font_handle_reuse) {
    FE_BASE();
    HFONT f = fe_mk(-12, 0, 400, 0, ANSI_CHARSET, "Arial");
    CHECK(f != NULL);
    CHECK(U(DeleteObject)(f));
    SetLastError(0);
    CHECK(!U(DeleteObject)(f));
    LOGFONTA l;
    EQ(U(GetObjectA)(f, sizeof l, &l), 0);
    EQ(U(GetObjectType)(f), 0);
    HDC dc = U(CreateCompatibleDC)(NULL);
    SetLastError(0);
    EQ(U(SelectObject)(dc, f), 0);
    U(DeleteDC)(dc);
}
TEST(fonts_extra_select_font_returns_previous) {
    FE_BASE();
    HDC dc = U(CreateCompatibleDC)(NULL);
    HFONT a = fe_mk(-12, 0, 400, 0, ANSI_CHARSET, "Arial");
    HFONT b = fe_mk(-20, 0, 700, 0, ANSI_CHARSET, "Arial");
    HGDIOBJ def = U(GetCurrentObject)(dc, OBJ_FONT);
    CHECK(def != NULL);
    EQ(U(SelectObject)(dc, a), def);
    EQ(U(GetCurrentObject)(dc, OBJ_FONT), a);
    EQ(U(SelectObject)(dc, b), a);
    EQ(U(SelectObject)(dc, b), b);
    EQ(U(SelectObject)(dc, def), b);
    CHECK(U(DeleteObject)(a)); CHECK(U(DeleteObject)(b));
    U(DeleteDC)(dc);
}
TEST(fonts_extra_selected_font_not_deleted) {
    FE_BASE();
    HDC dc = U(CreateCompatibleDC)(NULL);
    HFONT a = fe_mk(-12, 0, 400, 0, ANSI_CHARSET, "Arial");
    HGDIOBJ def = U(SelectObject)(dc, a);
    SetLastError(0);
    BOOL r = U(DeleteObject)(a);
    t_note("DeleteObject(selected font) = %d", r);
    LOGFONTA l;
    if (!r) EQ(U(GetObjectA)(a, sizeof l, &l), 60);
    U(SelectObject)(dc, def);
    U(DeleteObject)(a);
    U(DeleteDC)(dc);
}
TEST(fonts_extra_select_wrong_type_into_dc) {
    FE_BASE(); REQUIRE(U(CreatePen));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HFONT a = fe_mk(-12, 0, 400, 0, ANSI_CHARSET, "Arial");
    HGDIOBJ def = U(SelectObject)(dc, a);
    HPEN pen = U(CreatePen)(PS_SOLID, 1, 0);
    HGDIOBJ old = U(SelectObject)(dc, pen);
    CHECK(old != NULL);
    EQ(U(GetCurrentObject)(dc, OBJ_FONT), a);
    U(SelectObject)(dc, old);
    U(SelectObject)(dc, def);
    U(DeleteObject)(pen); U(DeleteObject)(a);
    U(DeleteDC)(dc);
}
TEST(fonts_extra_font_object_type) {
    FE_BASE();
    HFONT f = fe_mk(-12, 0, 400, 0, ANSI_CHARSET, "Arial");
    EQ(U(GetObjectType)(f), OBJ_FONT);
    U(DeleteObject)(f);
}
TEST(fonts_extra_face_ab_parity) {
    FE_BASE(); REQUIRE(U(GetTextFaceW));
    static const char *faces[] = {"Arial", "Courier New", "Tahoma", "Segoe UI", "Times New Roman"};
    for (unsigned i = 0; i < 5; i++) {
        fe_ctx c; if (!fe_open(&c, faces[i], -14, 400, 0, ANSI_CHARSET)) { fe_close(&c); continue; }
        char a[64]; WCHAR w[64]; char conv[64];
        int la = U(GetTextFaceA)(c.dc, sizeof a, a), lw = U(GetTextFaceW)(c.dc, 64, w);
        CHECKF(la > 0 && lw > 0, "%s", faces[i]);
        CHECKF((int)strlen(a) == la, "%s a len", faces[i]);
        CHECKF((int)wcslen(w) + 1 == lw, "%s w len %d", faces[i], lw);
        WideCharToMultiByte(CP_ACP, 0, w, -1, conv, sizeof conv, NULL, NULL);
        CHECKF(!strcmp(a, conv), "%s '%s' vs '%s'", faces[i], a, conv);
        EQ(U(GetTextFaceA)(c.dc, 0, NULL), la + 1);
        EQ(U(GetTextFaceW)(c.dc, 0, NULL), lw);
        fe_close(&c);
    }
}
TEST(fonts_extra_face_exact_minus_one_truncates) {
    FE_BASE();
    fe_ctx c; if (!fe_open(&c, "Arial", -14, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    char buf[16];
    memset(buf, 'Z', sizeof buf);
    int r = U(GetTextFaceA)(c.dc, 5, buf);
    t_note("GetTextFaceA(5) = %d buf '%.5s'", r, buf);
    CHECK(r <= 5);
    CHECK(buf[5] == 'Z');
    memset(buf, 'Z', sizeof buf);
    r = U(GetTextFaceA)(c.dc, 6, buf);
    EQ(r, 5);
    CHECK(!memcmp(buf, "Arial", 6));
    CHECK(buf[6] == 'Z');
    fe_close(&c);
}
TEST(fonts_extra_mapper_flags_roundtrip) {
    FE_BASE(); REQUIRE(U(SetMapperFlags));
    HDC dc = U(CreateCompatibleDC)(NULL);
    EQ(U(SetMapperFlags)(dc, 0), 0);
    EQ(U(SetMapperFlags)(dc, 1), 0);
    EQ(U(SetMapperFlags)(dc, 1), 1);
    EQ(U(SetMapperFlags)(dc, 0), 1);
    EQ(U(SetMapperFlags)(dc, 0), 0);
    SetLastError(0);
    EQ(U(SetMapperFlags)(dc, 2), GDI_ERROR);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0);
    EQ(U(SetMapperFlags)(dc, 0x80000000), GDI_ERROR);
    EQ(U(SetMapperFlags)(dc, 0), 0);
    U(DeleteDC)(dc);
}
TEST(fonts_extra_same_params_same_metrics) {
    FE_BASE();
    TEXTMETRICA t1, t2;
    fe_ctx c; if (!fe_open(&c, "Arial", -17, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    U(GetTextMetricsA)(c.dc, &t1); fe_close(&c);
    fe_open(&c, "Arial", -17, 400, 0, ANSI_CHARSET);
    U(GetTextMetricsA)(c.dc, &t2); fe_close(&c);
    CHECK(!memcmp(&t1, &t2, sizeof t1));
}
TEST(fonts_extra_bold_heavier_italic_flag) {
    FE_BASE();
    TEXTMETRICA r, b, it;
    fe_ctx c;
    if (!fe_open(&c, "Arial", -20, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    U(GetTextMetricsA)(c.dc, &r); fe_close(&c);
    fe_open(&c, "Arial", -20, 700, 0, ANSI_CHARSET); U(GetTextMetricsA)(c.dc, &b); fe_close(&c);
    fe_open(&c, "Arial", -20, 400, 1, ANSI_CHARSET); U(GetTextMetricsA)(c.dc, &it); fe_close(&c);
    CHECK(r.tmWeight < b.tmWeight);
    EQ(r.tmItalic, 0);
    CHECK(it.tmItalic != 0);
    CHECK(r.tmHeight == b.tmHeight);
}
TEST(fonts_extra_textmetrics_ab_null_and_bad) {
    FE_BASE(); REQUIRE(U(GetTextMetricsW));
    TEXTMETRICA a; TEXTMETRICW w;
    SetLastError(0);
    EQ(U(GetTextMetricsA)(NULL, &a), 0);
    EQ(U(GetTextMetricsW)(NULL, &w), 0);
    EQ(U(GetTextMetricsA)((HDC)(ULONG_PTR)0x1234, &a), 0);
}
TEST(fonts_extra_createfont_leak_loop) {
    FE_BASE(); REQUIRE(U(GetGuiResources));
    DWORD before = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    for (int i = 0; i < 2000; i++) {
        HFONT f = fe_mk(-(8 + i % 40), 0, 100 * (1 + i % 9), i & 1, DEFAULT_CHARSET, "Arial");
        if (!f) { CHECK(f != NULL); break; }
        U(DeleteObject)(f);
    }
    DWORD after = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    EQ(after, before);
}
TEST(fonts_extra_stock_font_not_deletable_selected) {
    FE_BASE();
    HGDIOBJ s = U(GetStockObject)(DEFAULT_GUI_FONT);
    CHECK(s != NULL);
    EQ(U(GetObjectType)(s), OBJ_FONT);
    LOGFONTA l; memset(&l, 0, sizeof l);
    EQ(U(GetObjectA)(s, sizeof l, &l), 60);
    CHECK(l.lfFaceName[0] != 0);
    U(DeleteObject)(s);   /* deleting a stock object must leave it usable */
    EQ(U(GetObjectA)(s, sizeof l, &l), 60);
}
TEST(fonts_extra_enum_null_callback) {
    FE_BASE(); REQUIRE(U(EnumFontFamiliesExA)); REQUIRE(U(EnumFontFamiliesA));
    if (fe_wine()) { t_skip("wine faults on NULL callbacks"); return; }
    HDC dc = U(CreateCompatibleDC)(NULL);
    LOGFONTA lf; memset(&lf, 0, sizeof lf); lf.lfCharSet = DEFAULT_CHARSET; lstrcpyA(lf.lfFaceName, "Arial");
    int r = U(EnumFontFamiliesExA)(dc, &lf, NULL, 0, 0);
    t_note("EnumFontFamiliesExA(NULL proc) = %d", r);
    U(DeleteDC)(dc);
}
TEST(fonts_extra_enum_bad_dc) {
    FE_BASE(); REQUIRE(U(EnumFontFamiliesExA));
    if (fe_wine()) { t_skip("wine differs on bad-DC enumeration"); return; }
    LOGFONTA lf; memset(&lf, 0, sizeof lf); lf.lfCharSet = DEFAULT_CHARSET;
    t_note("EnumFontFamiliesExA(NULL dc) ok");
    (void)lf;
}
