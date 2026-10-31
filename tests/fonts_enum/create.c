/* Font creation, LOGFONT round trips, handle lifetime. */
#include "fe.h"

TEST(fonts_create_struct_sizes) {
    CHECK(sizeof(LOGFONTA) == 60);
    CHECK(sizeof(LOGFONTW) == 92);
    CHECK(sizeof(ENUMLOGFONTEXA) == 188);
    CHECK(sizeof(ENUMLOGFONTEXW) == 348);
    CHECK(sizeof(ENUMLOGFONTEXDVA) == 260);
    CHECK(sizeof(ENUMLOGFONTEXDVW) == 420);
    CHECK(sizeof(TEXTMETRICA) == 56);
    CHECK(sizeof(TEXTMETRICW) == 60);
    CHECK(sizeof(DESIGNVECTOR) == 72);
}

TEST(fonts_create_fonta_basic) {
    REQUIRE(U(CreateFontA)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectType));
    HFONT f = U(CreateFontA)(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");
    CHECK(f != NULL);
    EQ(U(GetObjectType)(f), OBJ_FONT);
    CHECK(U(DeleteObject)(f));
}

TEST(fonts_create_fontw_basic) {
    REQUIRE(U(CreateFontW)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectType));
    HFONT f = U(CreateFontW)(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Arial");
    CHECK(f != NULL);
    EQ(U(GetObjectType)(f), OBJ_FONT);
    CHECK(U(DeleteObject)(f));
}

TEST(fonts_create_fonta_all_zero_null_face) {
    REQUIRE(U(CreateFontA)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectA));
    HFONT f = U(CreateFontA)(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL);
    CHECK(f != NULL);
    LOGFONTA lf; memset(&lf, 0x55, sizeof lf);
    EQ(U(GetObjectA)(f, sizeof lf, &lf), sizeof lf);
    EQ(lf.lfHeight, 0); EQ(lf.lfWidth, 0); EQ(lf.lfWeight, 0);
    EQ(lf.lfCharSet, 0); EQ(lf.lfFaceName[0], 0);
    U(DeleteObject)(f);
}

TEST(fonts_create_fontw_all_zero_null_face) {
    REQUIRE(U(CreateFontW)); REQUIRE(U(DeleteObject));
    HFONT f = U(CreateFontW)(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL);
    CHECK(f != NULL);
    if (f) U(DeleteObject)(f);
}

TEST(fonts_create_fonta_empty_face) {
    REQUIRE(U(CreateFontA)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectA));
    HFONT f = U(CreateFontA)(-10, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, "");
    CHECK(f != NULL);
    LOGFONTA lf; memset(&lf, 0x55, sizeof lf);
    EQ(U(GetObjectA)(f, sizeof lf, &lf), sizeof lf);
    EQ(lf.lfFaceName[0], 0);
    EQ(lf.lfHeight, -10);
    U(DeleteObject)(f);
}

TEST(fonts_create_indirect_null) {
    REQUIRE(U(CreateFontIndirectA)); REQUIRE(U(CreateFontIndirectW));
    SetLastError(0);
    CHECK(U(CreateFontIndirectA)(NULL) == NULL);
    CHECK(U(CreateFontIndirectW)(NULL) == NULL);
}

TEST(fonts_create_indirectex_dvnumaxes_limit) {
    REQUIRE(U(CreateFontIndirectExA)); REQUIRE(U(CreateFontIndirectExW)); REQUIRE(U(DeleteObject));
    ENUMLOGFONTEXDVA a; ENUMLOGFONTEXDVW w;
    memset(&a, 0, sizeof a); memset(&w, 0, sizeof w);
    a.elfEnumLogfontEx.elfLogFont.lfHeight = -12;
    lstrcpyA((char *)a.elfEnumLogfontEx.elfLogFont.lfFaceName, "Arial");
    w.elfEnumLogfontEx.elfLogFont.lfHeight = -12;
    lstrcpyW(w.elfEnumLogfontEx.elfLogFont.lfFaceName, L"Arial");
    a.elfDesignVector.dvNumAxes = 0; w.elfDesignVector.dvNumAxes = 0;
    HFONT f = U(CreateFontIndirectExA)(&a); CHECK(f != NULL); if (f) U(DeleteObject)(f);
    f = U(CreateFontIndirectExW)(&w); CHECK(f != NULL); if (f) U(DeleteObject)(f);
    a.elfDesignVector.dvNumAxes = 17; w.elfDesignVector.dvNumAxes = 17;
    f = U(CreateFontIndirectExA)(&a); if (f) { t_note("ExA accepted dvNumAxes=17"); U(DeleteObject)(f); }
    CHECK(U(CreateFontIndirectExW)(&w) == NULL);
    a.elfDesignVector.dvNumAxes = 0xFFFFFFFF; w.elfDesignVector.dvNumAxes = 0xFFFFFFFF;
    f = U(CreateFontIndirectExA)(&a); if (f) { t_note("ExA accepted dvNumAxes=-1"); U(DeleteObject)(f); }
    CHECK(U(CreateFontIndirectExW)(&w) == NULL);
}

TEST(fonts_create_indirectex_dvnumaxes_16) {
    REQUIRE(U(CreateFontIndirectExW)); REQUIRE(U(DeleteObject));
    ENUMLOGFONTEXDVW w; memset(&w, 0, sizeof w);
    w.elfEnumLogfontEx.elfLogFont.lfHeight = -12;
    lstrcpyW(w.elfEnumLogfontEx.elfLogFont.lfFaceName, L"Arial");
    w.elfDesignVector.dvReserved = STAMP_DESIGNVECTOR;
    w.elfDesignVector.dvNumAxes = 16;
    HFONT f = U(CreateFontIndirectExW)(&w);
    if (!f) t_note("CreateFontIndirectExW with 16 axes returned NULL (err %lu)", GetLastError());
    if (f) U(DeleteObject)(f);
}

TEST(fonts_create_indirectex_roundtrip_a) {
    REQUIRE(U(CreateFontIndirectExA)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectA));
    ENUMLOGFONTEXDVA a; memset(&a, 0, sizeof a);
    LOGFONTA *l = &a.elfEnumLogfontEx.elfLogFont;
    l->lfHeight = -21; l->lfWeight = 700; l->lfItalic = 1; l->lfCharSet = ANSI_CHARSET;
    lstrcpyA(l->lfFaceName, "Times New Roman");
    lstrcpyA((char *)a.elfEnumLogfontEx.elfFullName, "ignored full");
    HFONT f = U(CreateFontIndirectExA)(&a);
    CHECK(f != NULL);
    if (!f) return;
    LOGFONTA o; memset(&o, 0, sizeof o);
    EQ(U(GetObjectA)(f, sizeof o, &o), 60);
    EQ(o.lfHeight, -21); EQ(o.lfWeight, 700); EQ(o.lfItalic, 1);
    EXPECT_STR(o.lfFaceName, "Times New Roman");
    ENUMLOGFONTEXDVA big; memset(&big, 0x77, sizeof big);
    EQ(U(GetObjectA)(f, sizeof big, &big), sizeof big);
    EXPECT_STR(big.elfEnumLogfontEx.elfLogFont.lfFaceName, "Times New Roman");
    EQ(big.elfDesignVector.dvNumAxes, 0);
    U(DeleteObject)(f);
}

TEST(fonts_create_indirectex_roundtrip_w) {
    REQUIRE(U(CreateFontIndirectExW)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectW));
    ENUMLOGFONTEXDVW w; memset(&w, 0, sizeof w);
    LOGFONTW *l = &w.elfEnumLogfontEx.elfLogFont;
    l->lfHeight = 33; l->lfWidth = 7; l->lfUnderline = 1;
    lstrcpyW(l->lfFaceName, L"Courier New");
    HFONT f = U(CreateFontIndirectExW)(&w);
    CHECK(f != NULL);
    if (!f) return;
    LOGFONTW o; memset(&o, 0, sizeof o);
    EQ(U(GetObjectW)(f, sizeof o, &o), sizeof o);
    EQ(o.lfHeight, 33); EQ(o.lfWidth, 7); EQ(o.lfUnderline, 1);
    EXPECT_WSTR(o.lfFaceName, L"Courier New");
    U(DeleteObject)(f);
}

/* ---- GetObject round trips ---- */
static void rt_one(const LOGFONTA *in) {
    HFONT f = U(CreateFontIndirectA)(in);
    CHECKF(f != NULL, "face=%s h=%ld", in->lfFaceName, in->lfHeight);
    if (!f) return;
    LOGFONTA o; memset(&o, 0xCC, sizeof o);
    int r = U(GetObjectA)(f, sizeof o, &o);
    EQ(r, sizeof o);
    EQ(o.lfHeight, in->lfHeight); EQ(o.lfWidth, in->lfWidth);
    EQ(o.lfEscapement, in->lfEscapement); EQ(o.lfOrientation, in->lfOrientation);
    EQ(o.lfWeight, in->lfWeight); EQ(o.lfItalic, in->lfItalic);
    EQ(o.lfUnderline, in->lfUnderline); EQ(o.lfStrikeOut, in->lfStrikeOut);
    EQ(o.lfCharSet, in->lfCharSet); EQ(o.lfOutPrecision, in->lfOutPrecision);
    EQ(o.lfClipPrecision, in->lfClipPrecision); EQ(o.lfQuality, in->lfQuality);
    EQ(o.lfPitchAndFamily, in->lfPitchAndFamily);
    EXPECT_STR(o.lfFaceName, in->lfFaceName);
    /* wide view of the same object */
    LOGFONTW w; memset(&w, 0xCC, sizeof w);
    r = U(GetObjectW)(f, sizeof w, &w);
    EQ(r, sizeof w);
    EQ(w.lfHeight, in->lfHeight); EQ(w.lfWeight, in->lfWeight);
    EQ(w.lfCharSet, in->lfCharSet); EQ(w.lfQuality, in->lfQuality);
    char back[64]; WideCharToMultiByte(CP_ACP, 0, w.lfFaceName, -1, back, sizeof back, NULL, NULL);
    EXPECT_STR(back, in->lfFaceName);
    U(DeleteObject)(f);
}
static LOGFONTA rt_base(void) {
    LOGFONTA l; memset(&l, 0, sizeof l);
    l.lfHeight = -16; l.lfWeight = FW_NORMAL; l.lfCharSet = DEFAULT_CHARSET;
    lstrcpyA(l.lfFaceName, "Arial");
    return l;
}
#define RT_REQ() do { REQUIRE(U(CreateFontIndirectA)); REQUIRE(U(GetObjectA)); REQUIRE(U(GetObjectW)); REQUIRE(U(DeleteObject)); } while (0)

TEST(fonts_create_rt_height_sweep) {
    RT_REQ();
    static const int hs[] = {-1, 1, -2, 2, -8, 8, -12, 12, -13, -72, 72, -100, 100, -500, 500, -1000, 1000, 0};
    for (unsigned i = 0; i < sizeof hs / sizeof hs[0]; i++) { LOGFONTA l = rt_base(); l.lfHeight = hs[i]; rt_one(&l); }
}
TEST(fonts_create_rt_height_extremes) {
    RT_REQ();
    static const int hs[] = {INT_MAX, INT_MIN, 0x7FFF, -0x7FFF, 0x8000, 65536, -65536};
    for (unsigned i = 0; i < sizeof hs / sizeof hs[0]; i++) {
        LOGFONTA l = rt_base(); l.lfHeight = hs[i];
        HFONT f = U(CreateFontIndirectA)(&l);
        CHECKF(f != NULL, "h=%d", hs[i]);
        if (f) {
            LOGFONTA o; int r = U(GetObjectA)(f, sizeof o, &o);
            EQ(r, sizeof o);
            if (o.lfHeight != hs[i]) t_note("height %d stored as %ld", hs[i], o.lfHeight);
            U(DeleteObject)(f);
        }
    }
}
TEST(fonts_create_rt_width_sweep) {
    RT_REQ();
    static const int ws[] = {0, 1, 5, 10, 50, 100, 1000};
    for (unsigned i = 0; i < sizeof ws / sizeof ws[0]; i++) { LOGFONTA l = rt_base(); l.lfWidth = ws[i]; rt_one(&l); }
}
TEST(fonts_create_rt_width_negative) {
    RT_REQ();
    LOGFONTA l = rt_base(); l.lfWidth = -5;
    HFONT f = U(CreateFontIndirectA)(&l);
    CHECK(f != NULL);
    if (f) {
        LOGFONTA o; EQ(U(GetObjectA)(f, sizeof o, &o), sizeof o);
        if (o.lfWidth != -5) t_note("negative width stored as %ld", o.lfWidth);
        U(DeleteObject)(f);
    }
}
TEST(fonts_create_rt_escapement_sweep) {
    RT_REQ();
    static const int es[] = {0, 1, 450, 900, 1800, 2700, 3599, 3600, 5000, -900, -1};
    for (unsigned i = 0; i < sizeof es / sizeof es[0]; i++) {
        LOGFONTA l = rt_base(); l.lfEscapement = es[i]; l.lfOrientation = -es[i]; rt_one(&l);
    }
}
TEST(fonts_create_rt_weight_sweep) {
    RT_REQ();
    static const int ws[] = {0, 1, 100, 200, 300, 399, 400, 401, 500, 600, 700, 800, 900, 950, 1000, 1001, 5000};
    for (unsigned i = 0; i < sizeof ws / sizeof ws[0]; i++) { LOGFONTA l = rt_base(); l.lfWeight = ws[i]; rt_one(&l); }
}
TEST(fonts_create_rt_weight_named) {
    RT_REQ();
    CHECK(FW_DONTCARE == 0 && FW_THIN == 100 && FW_EXTRALIGHT == 200 && FW_LIGHT == 300 && FW_NORMAL == 400);
    CHECK(FW_MEDIUM == 500 && FW_SEMIBOLD == 600 && FW_BOLD == 700 && FW_EXTRABOLD == 800 && FW_HEAVY == 900);
    LOGFONTA l = rt_base(); l.lfWeight = FW_BOLD; rt_one(&l);
}
TEST(fonts_create_rt_flags_bool) {
    RT_REQ();
    for (int m = 0; m < 8; m++) {
        LOGFONTA l = rt_base(); l.lfItalic = m & 1; l.lfUnderline = (m >> 1) & 1; l.lfStrikeOut = (m >> 2) & 1; rt_one(&l);
    }
}
TEST(fonts_create_rt_flags_nonbool) {
    RT_REQ();
    static const BYTE vs[] = {2, 7, 0x80, 0xFF};
    for (unsigned i = 0; i < sizeof vs; i++) {
        LOGFONTA l = rt_base(); l.lfItalic = vs[i]; l.lfUnderline = vs[i]; l.lfStrikeOut = vs[i];
        HFONT f = U(CreateFontIndirectA)(&l); CHECK(f != NULL);
        if (!f) continue;
        LOGFONTA o; EQ(U(GetObjectA)(f, sizeof o, &o), sizeof o);
        if (o.lfItalic != vs[i]) t_note("lfItalic %u stored as %u", vs[i], o.lfItalic);
        U(DeleteObject)(f);
    }
}
TEST(fonts_create_rt_charset_sweep) {
    RT_REQ();
    static const BYTE cs[] = {ANSI_CHARSET, DEFAULT_CHARSET, SYMBOL_CHARSET, SHIFTJIS_CHARSET, HANGEUL_CHARSET, GB2312_CHARSET,
        CHINESEBIG5_CHARSET, OEM_CHARSET, JOHAB_CHARSET, HEBREW_CHARSET, ARABIC_CHARSET, GREEK_CHARSET, TURKISH_CHARSET,
        VIETNAMESE_CHARSET, THAI_CHARSET, EASTEUROPE_CHARSET, RUSSIAN_CHARSET, MAC_CHARSET, BALTIC_CHARSET};
    for (unsigned i = 0; i < sizeof cs; i++) { LOGFONTA l = rt_base(); l.lfCharSet = cs[i]; rt_one(&l); }
}
TEST(fonts_create_rt_charset_unknown) {
    RT_REQ();
    static const BYTE cs[] = {3, 5, 100, 127, 200, 254};
    for (unsigned i = 0; i < sizeof cs; i++) {
        LOGFONTA l = rt_base(); l.lfCharSet = cs[i];
        HFONT f = U(CreateFontIndirectA)(&l);
        CHECKF(f != NULL, "charset %u", cs[i]);
        if (f) U(DeleteObject)(f);
    }
}
TEST(fonts_create_rt_outprecision_sweep) {
    RT_REQ();
    static const BYTE ps[] = {OUT_DEFAULT_PRECIS, OUT_STRING_PRECIS, OUT_CHARACTER_PRECIS, OUT_STROKE_PRECIS, OUT_TT_PRECIS,
        OUT_DEVICE_PRECIS, OUT_RASTER_PRECIS, OUT_TT_ONLY_PRECIS, OUT_OUTLINE_PRECIS, OUT_SCREEN_OUTLINE_PRECIS, OUT_PS_ONLY_PRECIS};
    for (unsigned i = 0; i < sizeof ps; i++) { LOGFONTA l = rt_base(); l.lfOutPrecision = ps[i]; rt_one(&l); }
}
TEST(fonts_create_rt_clipprecision_sweep) {
    RT_REQ();
    static const BYTE ps[] = {CLIP_DEFAULT_PRECIS, CLIP_CHARACTER_PRECIS, CLIP_STROKE_PRECIS, CLIP_LH_ANGLES, CLIP_TT_ALWAYS,
        CLIP_DFA_DISABLE, CLIP_EMBEDDED, CLIP_LH_ANGLES | CLIP_TT_ALWAYS};
    for (unsigned i = 0; i < sizeof ps; i++) { LOGFONTA l = rt_base(); l.lfClipPrecision = ps[i]; rt_one(&l); }
}
TEST(fonts_create_rt_quality_sweep) {
    RT_REQ();
    for (int q = 0; q <= 6; q++) { LOGFONTA l = rt_base(); l.lfQuality = (BYTE)q; rt_one(&l); }
}
TEST(fonts_create_rt_quality_unknown) {
    RT_REQ();
    static const BYTE q[] = {7, 8, 100, 255};
    for (unsigned i = 0; i < sizeof q; i++) {
        LOGFONTA l = rt_base(); l.lfQuality = q[i];
        HFONT f = U(CreateFontIndirectA)(&l); CHECK(f != NULL);
        if (f) U(DeleteObject)(f);
    }
}
TEST(fonts_create_rt_pitchfamily_sweep) {
    RT_REQ();
    static const BYTE fam[] = {FF_DONTCARE, FF_ROMAN, FF_SWISS, FF_MODERN, FF_SCRIPT, FF_DECORATIVE};
    static const BYTE pit[] = {DEFAULT_PITCH, FIXED_PITCH, VARIABLE_PITCH};
    for (unsigned i = 0; i < sizeof fam; i++)
        for (unsigned j = 0; j < sizeof pit; j++) { LOGFONTA l = rt_base(); l.lfPitchAndFamily = fam[i] | pit[j]; rt_one(&l); }
}
TEST(fonts_create_rt_pitchfamily_extra_bits) {
    RT_REQ();
    static const BYTE v[] = {0x03, 0x04, 0x08, 0x0F, 0x0C, 0xFF, 0x88};
    for (unsigned i = 0; i < sizeof v; i++) {
        LOGFONTA l = rt_base(); l.lfPitchAndFamily = v[i];
        HFONT f = U(CreateFontIndirectA)(&l); CHECK(f != NULL);
        if (f) { LOGFONTA o; U(GetObjectA)(f, sizeof o, &o); if (o.lfPitchAndFamily != v[i]) t_note("pitchfamily %02x stored as %02x", v[i], o.lfPitchAndFamily); U(DeleteObject)(f); }
    }
}
TEST(fonts_create_rt_facenames) {
    RT_REQ();
    static const char *names[] = {"Arial", "arial", "ARIAL", "Courier New", "Times New Roman", "Tahoma", "Segoe UI", "Consolas",
        "Symbol", "Marlett", "System", "Terminal", "Fixedsys", "MS Sans Serif", "Small Fonts", "Does Not Exist 123", "A", "@Arial",
        "MS Shell Dlg", "MS Shell Dlg 2", " Arial", "Arial ", "Font With Spaces In It"};
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) { LOGFONTA l = rt_base(); lstrcpyA(l.lfFaceName, names[i]); rt_one(&l); }
}
TEST(fonts_create_rt_facename_len31) {
    RT_REQ();
    LOGFONTA l = rt_base();
    memset(l.lfFaceName, 'x', 31); l.lfFaceName[31] = 0;
    rt_one(&l);
}
TEST(fonts_create_rt_facename_len30_and_1) {
    RT_REQ();
    LOGFONTA l = rt_base();
    memset(l.lfFaceName, 'Q', 30); l.lfFaceName[30] = 0; rt_one(&l);
    memset(&l.lfFaceName, 0, sizeof l.lfFaceName); l.lfFaceName[0] = 'Q'; rt_one(&l);
}
TEST(fonts_create_facename_len32_unterminated_a) {
    RT_REQ();
    LOGFONTA l = rt_base();
    memset(l.lfFaceName, 'z', 32);
    HFONT f = U(CreateFontIndirectA)(&l);
    CHECK(f != NULL);
    if (f) {
        LOGFONTA o; memset(&o, 0, sizeof o);
        EQ(U(GetObjectA)(f, sizeof o, &o), sizeof o);
        t_note("32-char unterminated face stored with strlen=%d", (int)strnlen(o.lfFaceName, 32));
        U(DeleteObject)(f);
    }
}
TEST(fonts_create_facename_len32_unterminated_w) {
    REQUIRE(U(CreateFontIndirectW)); REQUIRE(U(GetObjectW)); REQUIRE(U(DeleteObject));
    LOGFONTW l; memset(&l, 0, sizeof l);
    l.lfHeight = -12;
    for (int i = 0; i < 32; i++) l.lfFaceName[i] = L'z';
    HFONT f = U(CreateFontIndirectW)(&l);
    CHECK(f != NULL);
    if (f) {
        LOGFONTW o; memset(&o, 0, sizeof o);
        EQ(U(GetObjectW)(f, sizeof o, &o), sizeof o);
        int n = 0; while (n < 32 && o.lfFaceName[n]) n++;
        t_note("32-wchar unterminated face stored with length=%d", n);
        U(DeleteObject)(f);
    }
}
TEST(fonts_create_fonta_long_face_32plus) {
    REQUIRE(U(CreateFontA)); REQUIRE(U(DeleteObject));
    char name[80]; memset(name, 'L', 70); name[70] = 0;
    HFONT f = U(CreateFontA)(-12, 0, 0, 0, 400, 0, 0, 0, 1, 0, 0, 0, 0, name);
    CHECK(f != NULL);
    if (f) U(DeleteObject)(f);
}
TEST(fonts_create_fontw_long_face_32plus) {
    REQUIRE(U(CreateFontW)); REQUIRE(U(DeleteObject));
    WCHAR name[80]; for (int i = 0; i < 70; i++) name[i] = L'L'; name[70] = 0;
    HFONT f = U(CreateFontW)(-12, 0, 0, 0, 400, 0, 0, 0, 1, 0, 0, 0, 0, name);
    CHECK(f != NULL);
    if (f) U(DeleteObject)(f);
}

TEST(fonts_create_fonta_params_pass_through) {
    REQUIRE(U(CreateFontA)); REQUIRE(U(GetObjectA)); REQUIRE(U(DeleteObject));
    HFONT f = U(CreateFontA)(-11, 6, 450, 300, 600, 1, 1, 1, GREEK_CHARSET, OUT_TT_PRECIS, CLIP_LH_ANGLES, PROOF_QUALITY,
                             FIXED_PITCH | FF_ROMAN, "Tahoma");
    CHECK(f != NULL);
    LOGFONTA o; memset(&o, 0, sizeof o);
    EQ(U(GetObjectA)(f, sizeof o, &o), sizeof o);
    EQ(o.lfHeight, -11); EQ(o.lfWidth, 6); EQ(o.lfEscapement, 450); EQ(o.lfOrientation, 300);
    EQ(o.lfWeight, 600); EQ(o.lfItalic, 1); EQ(o.lfUnderline, 1); EQ(o.lfStrikeOut, 1);
    EQ(o.lfCharSet, GREEK_CHARSET); EQ(o.lfOutPrecision, OUT_TT_PRECIS); EQ(o.lfClipPrecision, CLIP_LH_ANGLES);
    EQ(o.lfQuality, PROOF_QUALITY); EQ(o.lfPitchAndFamily, FIXED_PITCH | FF_ROMAN);
    EXPECT_STR(o.lfFaceName, "Tahoma");
    U(DeleteObject)(f);
}
TEST(fonts_create_fontw_params_pass_through) {
    REQUIRE(U(CreateFontW)); REQUIRE(U(GetObjectW)); REQUIRE(U(DeleteObject));
    HFONT f = U(CreateFontW)(15, 8, 100, 200, 300, 0, 1, 0, RUSSIAN_CHARSET, OUT_RASTER_PRECIS, CLIP_STROKE_PRECIS, DRAFT_QUALITY,
                             VARIABLE_PITCH | FF_SWISS, L"Segoe UI");
    CHECK(f != NULL);
    LOGFONTW o; memset(&o, 0, sizeof o);
    EQ(U(GetObjectW)(f, sizeof o, &o), sizeof o);
    EQ(o.lfHeight, 15); EQ(o.lfWidth, 8); EQ(o.lfEscapement, 100); EQ(o.lfOrientation, 200);
    EQ(o.lfWeight, 300); EQ(o.lfItalic, 0); EQ(o.lfUnderline, 1); EQ(o.lfStrikeOut, 0);
    EQ(o.lfCharSet, RUSSIAN_CHARSET); EQ(o.lfOutPrecision, OUT_RASTER_PRECIS);
    EQ(o.lfClipPrecision, CLIP_STROKE_PRECIS); EQ(o.lfQuality, DRAFT_QUALITY);
    EQ(o.lfPitchAndFamily, VARIABLE_PITCH | FF_SWISS);
    EXPECT_WSTR(o.lfFaceName, L"Segoe UI");
    U(DeleteObject)(f);
}
TEST(fonts_create_fonta_byte_truncation) {
    /* flag parameters are DWORD but stored in BYTE members */
    REQUIRE(U(CreateFontA)); REQUIRE(U(GetObjectA)); REQUIRE(U(DeleteObject));
    HFONT f = U(CreateFontA)(-12, 0, 0, 0, 400, 0x101, 0x100, 0x1FF, 0x100 | ANSI_CHARSET, 0x100, 0x100, 0x100, 0x100, "Arial");
    CHECK(f != NULL);
    LOGFONTA o; memset(&o, 0xAA, sizeof o);
    EQ(U(GetObjectA)(f, sizeof o, &o), sizeof o);
    EQ(o.lfItalic, 1); EQ(o.lfUnderline, 0); EQ(o.lfStrikeOut, 0xFF);
    EQ(o.lfCharSet, 0); EQ(o.lfOutPrecision, 0); EQ(o.lfClipPrecision, 0);
    EQ(o.lfQuality, 0); EQ(o.lfPitchAndFamily, 0);
    U(DeleteObject)(f);
}

/* ---- GetObject size semantics (A) ---- */
TEST(fonts_create_getobjecta_null_buffer) {
    RT_REQ();
    LOGFONTA l = rt_base();
    HFONT f = U(CreateFontIndirectA)(&l);
    CHECK(f != NULL);
    EQ(U(GetObjectA)(f, 0, NULL), 60);
    EQ(U(GetObjectA)(f, 100, NULL), 60);
    EQ(U(GetObjectW)(f, 0, NULL), 92);
    EQ(U(GetObjectW)(f, 1000, NULL), 92);
    U(DeleteObject)(f);
}
TEST(fonts_create_getobjecta_size_variants) {
    RT_REQ();
    LOGFONTA l = rt_base();
    HFONT f = U(CreateFontIndirectA)(&l);
    CHECK(f != NULL);
    BYTE buf[600]; memset(buf, 0, sizeof buf);
    EQ(U(GetObjectA)(f, 60, buf), 60);
    EQ(U(GetObjectA)(f, 92, buf), 60);   /* 0x5c is treated as a LOGFONT request */
    EQ(U(GetObjectA)(f, 188, buf), 188);
    EQ(U(GetObjectA)(f, 260, buf), 260);
    EQ(U(GetObjectA)(f, 1, buf), 1);
    EQ(U(GetObjectA)(f, 4, buf), 4);
    EQ(U(GetObjectA)(f, 59, buf), 59);
    EQ(U(GetObjectA)(f, 61, buf), 61);
    EQ(U(GetObjectA)(f, 0, buf), 0);
    U(DeleteObject)(f);
}
TEST(fonts_create_getobjecta_partial_copy) {
    RT_REQ();
    LOGFONTA l = rt_base(); l.lfHeight = -23;
    HFONT f = U(CreateFontIndirectA)(&l);
    BYTE buf[64]; memset(buf, 0xEE, sizeof buf);
    int r = U(GetObjectA)(f, 4, buf);
    EQ(r, 4);
    EQ(((LOGFONTA *)buf)->lfHeight, -23);
    EQ(buf[4], 0xEE);                   /* nothing written past the requested size */
    U(DeleteObject)(f);
}
TEST(fonts_create_getobjectw_size_variants) {
    RT_REQ();
    LOGFONTA l = rt_base();
    HFONT f = U(CreateFontIndirectA)(&l);
    BYTE buf[600]; memset(buf, 0, sizeof buf);
    EQ(U(GetObjectW)(f, 92, buf), 92);
    int r = U(GetObjectW)(f, 348, buf); if (r != 348) t_note("GetObjectW(348) = %d", r);
    r = U(GetObjectW)(f, 420, buf); if (r != 420) t_note("GetObjectW(420) = %d", r);
    r = U(GetObjectW)(f, 60, buf); t_note("GetObjectW(60) = %d", r);
    r = U(GetObjectW)(f, 4, buf); t_note("GetObjectW(4) = %d", r);
    r = U(GetObjectW)(f, 91, buf); t_note("GetObjectW(91) = %d", r);
    U(DeleteObject)(f);
}
TEST(fonts_create_getobjecta_enumlogfontex) {
    RT_REQ();
    LOGFONTA l = rt_base();
    HFONT f = U(CreateFontIndirectA)(&l);
    ENUMLOGFONTEXA e; memset(&e, 0x33, sizeof e);
    EQ(U(GetObjectA)(f, sizeof e, &e), sizeof e);
    EXPECT_STR(e.elfLogFont.lfFaceName, "Arial");
    t_note("created font ENUMLOGFONTEXA: full='%s' style='%s' script='%s'", e.elfFullName, e.elfStyle, e.elfScript);
    U(DeleteObject)(f);
}
TEST(fonts_create_getobject_bad_handles) {
    RT_REQ();
    LOGFONTA o;
    SetLastError(0);
    EQ(U(GetObjectA)(NULL, sizeof o, &o), 0);
    EQ(U(GetObjectW)(NULL, sizeof o, &o), 0);
    EQ(U(GetObjectA)((HANDLE)(ULONG_PTR)0x12345, sizeof o, &o), 0);
    EQ(U(GetObjectA)((HANDLE)(ULONG_PTR)0xFFFFFFFF, sizeof o, &o), 0);
}

/* ---- handle lifetime ---- */
TEST(fonts_create_delete_twice) {
    FE_BASE(); REQUIRE(U(GetObjectType)); REQUIRE(U(GetObjectA));
    HFONT f = fe_mk(-12, 0, 400, 0, 1, "Arial");
    CHECK(f != NULL);
    EQ(U(GetObjectType)(f), OBJ_FONT);
    CHECK(U(DeleteObject)(f));
    SetLastError(0);
    CHECK(!U(DeleteObject)(f));
    EQ(U(GetObjectType)(f), 0);
    LOGFONTA o;
    EQ(U(GetObjectA)(f, sizeof o, &o), 0);
}
TEST(fonts_create_deleted_not_selectable) {
    FE_BASE();
    HDC dc = U(CreateCompatibleDC)(NULL);
    HFONT f = fe_mk(-12, 0, 400, 0, 1, "Arial");
    U(DeleteObject)(f);
    SetLastError(0);
    HGDIOBJ old = U(SelectObject)(dc, f);
    CHECK(old == NULL);
    char name[40];
    CHECK(U(GetTextFaceA)(dc, sizeof name, name) > 0);   /* DC still has its previous font */
    U(DeleteDC)(dc);
}
TEST(fonts_create_distinct_handles) {
    FE_BASE();
    HFONT a = fe_mk(-12, 0, 400, 0, 1, "Arial"), b = fe_mk(-12, 0, 400, 0, 1, "Arial");
    CHECK(a != NULL && b != NULL);
    CHECK(a != b);
    U(DeleteObject)(a); U(DeleteObject)(b);
}
TEST(fonts_create_handle_type_bits) {
    FE_BASE(); REQUIRE(U(GetObjectType));
    HFONT f = fe_mk(-12, 0, 400, 0, 1, "Arial");
    CHECK(f != NULL);
    /* GDI handle: type field 0x0A in bits 16..22 for fonts */
    EQ(((ULONG_PTR)f >> 16) & 0x7F, 0x0A);
    U(DeleteObject)(f);
}
TEST(fonts_create_leak_loop) {
    FE_BASE(); REQUIRE(U(GetGuiResources));
    DWORD before = fe_gdi_count();
    for (int i = 0; i < 400; i++) {
        HFONT f = fe_mk(-8 - (i % 40), 0, 400 + (i % 3) * 100, i & 1, 1, i % 2 ? "Arial" : "Tahoma");
        if (!f) { CHECK(f != NULL); break; }
        U(DeleteObject)(f);
    }
    EQ(fe_gdi_count(), before);
}
TEST(fonts_create_leak_select_loop) {
    FE_BASE(); REQUIRE(U(GetGuiResources));
    DWORD before = fe_gdi_count();
    for (int i = 0; i < 100; i++) {
        HDC dc = U(CreateCompatibleDC)(NULL);
        HFONT f = fe_mk(-10 - i % 20, 0, 400, 0, 1, "Arial");
        HGDIOBJ old = U(SelectObject)(dc, f);
        TEXTMETRICA tm; U(GetTextMetricsA)(dc, &tm);
        U(SelectObject)(dc, old);
        U(DeleteObject)(f);
        U(DeleteDC)(dc);
    }
    EQ(fe_gdi_count(), before);
}
TEST(fonts_create_gdi_count_delta) {
    FE_BASE(); REQUIRE(U(GetGuiResources));
    HFONT fs[50];
    DWORD before = fe_gdi_count();
    for (int i = 0; i < 50; i++) fs[i] = fe_mk(-12 - i, 0, 400, 0, 1, "Arial");
    DWORD mid = fe_gdi_count();
    if (mid != before + 50) t_note("50 fonts changed GDI count by %ld", (long)(mid - before));
    CHECK(mid >= before + 50);
    for (int i = 0; i < 50; i++) U(DeleteObject)(fs[i]);
    EQ(fe_gdi_count(), before);
}
TEST(fonts_create_many_alive) {
    FE_BASE();
    enum { N = 2000 };
    HFONT *fs = (HFONT *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, N * sizeof(HFONT));
    int made = 0;
    for (int i = 0; i < N; i++) { fs[i] = fe_mk(-9 - i % 50, 0, 400, 0, 1, "Arial"); if (!fs[i]) break; made++; }
    CHECK(made == N);
    for (int i = 0; i < made; i++) U(DeleteObject)(fs[i]);
    HeapFree(GetProcessHeap(), 0, fs);
}

TEST(fonts_create_delete_stock_font) {
    FE_BASE(); REQUIRE(U(GetStockObject)); REQUIRE(U(GetObjectType));
    HGDIOBJ s = U(GetStockObject)(SYSTEM_FONT);
    CHECK(s != NULL);
    EQ(U(GetObjectType)(s), OBJ_FONT);
    BOOL r = U(DeleteObject)(s);
    t_note("DeleteObject(stock SYSTEM_FONT) = %d", r);
    EQ(U(GetObjectType)(s), OBJ_FONT);
    EQ(U(GetStockObject)(SYSTEM_FONT), s);
}
TEST(fonts_create_stock_fonts_valid) {
    FE_BASE(); REQUIRE(U(GetStockObject)); REQUIRE(U(GetObjectType));
    static const int ids[] = {OEM_FIXED_FONT, ANSI_FIXED_FONT, ANSI_VAR_FONT, SYSTEM_FONT, DEVICE_DEFAULT_FONT, SYSTEM_FIXED_FONT, DEFAULT_GUI_FONT};
    for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        HGDIOBJ s = U(GetStockObject)(ids[i]);
        CHECKF(s != NULL, "stock %d", ids[i]);
        if (s) EQ(U(GetObjectType)(s), OBJ_FONT);
    }
}
TEST(fonts_create_stock_font_faces) {
    FE_BASE(); REQUIRE(U(GetStockObject));
    static const struct { int id; const char *name; int hard; } t[] = {
        {SYSTEM_FONT, "System", 1}, {OEM_FIXED_FONT, "Terminal", 1}, {SYSTEM_FIXED_FONT, "Fixedsys", 1},
        {ANSI_FIXED_FONT, "Courier", 0}, {ANSI_VAR_FONT, "MS Sans Serif", 0}, {DEFAULT_GUI_FONT, "Segoe UI", 0}, {DEVICE_DEFAULT_FONT, "System", 0}};
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ orig = U(GetCurrentObject)(dc, OBJ_FONT);
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        HGDIOBJ s = U(GetStockObject)(t[i].id);
        U(SelectObject)(dc, s);
        char n[64] = ""; U(GetTextFaceA)(dc, sizeof n, n);
        if (t[i].hard) CHECKF(!lstrcmpA(n, t[i].name), "stock %d face '%s' want '%s'", t[i].id, n, t[i].name);
        else if (lstrcmpA(n, t[i].name)) t_note("stock %d face '%s' (expected '%s')", t[i].id, n, t[i].name);
    }
    U(SelectObject)(dc, orig);
    U(DeleteDC)(dc);
}
TEST(fonts_create_default_dc_font_is_system) {
    FE_BASE(); REQUIRE(U(GetCurrentObject)); REQUIRE(U(GetStockObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ cur = U(GetCurrentObject)(dc, OBJ_FONT);
    CHECK(cur != NULL);
    char n[40] = ""; U(GetTextFaceA)(dc, sizeof n, n);
    EXPECT_STR(n, "System");
    CHECK(cur == U(GetStockObject)(SYSTEM_FONT) || cur == U(GetStockObject)(DEVICE_DEFAULT_FONT));
    U(DeleteDC)(dc);
}

/* ---- threads ---- */
static DWORD WINAPI thr_use(LPVOID p) {
    HFONT f = (HFONT)p;
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ old = U(SelectObject)(dc, f);
    TEXTMETRICA tm; memset(&tm, 0, sizeof tm);
    BOOL ok = old && U(GetTextMetricsA)(dc, &tm);
    if (old) U(SelectObject)(dc, old);
    U(DeleteDC)(dc);
    return ok ? (DWORD)tm.tmHeight : 0;
}
TEST(fonts_create_cross_thread_use) {
    FE_BASE();
    HFONT f = fe_mk(-20, 0, 400, 0, 1, "Arial");
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ old = U(SelectObject)(dc, f);
    TEXTMETRICA tm; U(GetTextMetricsA)(dc, &tm);
    U(SelectObject)(dc, old); U(DeleteDC)(dc);
    HANDLE th = CreateThread(NULL, 0, thr_use, f, 0, NULL);
    CHECK(th != NULL);
    if (th) {
        WaitForSingleObject(th, 20000);
        DWORD h = 0; GetExitCodeThread(th, &h); CloseHandle(th);
        EQ(h, tm.tmHeight);
    }
    U(DeleteObject)(f);
}
static DWORD WINAPI thr_create(LPVOID p) { *(HFONT *)p = fe_mk(-14, 0, 700, 0, 1, "Arial"); return 0; }
TEST(fonts_create_thread_created_survives_exit) {
    FE_BASE(); REQUIRE(U(GetObjectA)); REQUIRE(U(GetObjectType));
    HFONT f = NULL;
    HANDLE th = CreateThread(NULL, 0, thr_create, &f, 0, NULL);
    CHECK(th != NULL);
    if (!th) return;
    WaitForSingleObject(th, 20000); CloseHandle(th);
    CHECK(f != NULL);
    if (!f) return;
    LOGFONTA o;
    EQ(U(GetObjectA)(f, sizeof o, &o), sizeof o);
    EQ(o.lfHeight, -14); EQ(o.lfWeight, 700);
    EQ(U(GetObjectType)(f), OBJ_FONT);
    CHECK(U(DeleteObject)(f));
}
static DWORD WINAPI thr_delete(LPVOID p) { return (DWORD)U(DeleteObject)((HFONT)p); }
TEST(fonts_create_delete_from_other_thread) {
    FE_BASE(); REQUIRE(U(GetObjectType));
    HFONT f = fe_mk(-14, 0, 400, 0, 1, "Arial");
    HANDLE th = CreateThread(NULL, 0, thr_delete, f, 0, NULL);
    CHECK(th != NULL);
    if (!th) { U(DeleteObject)(f); return; }
    WaitForSingleObject(th, 20000);
    DWORD r = 0; GetExitCodeThread(th, &r); CloseHandle(th);
    EQ(r, 1);
    EQ(U(GetObjectType)(f), 0);
}

/* ---- A vs W creation parity ---- */
static void metrics_of(const char *face, int wide, TEXTMETRICW *out) {
    HDC dc = U(CreateCompatibleDC)(NULL);
    HFONT f;
    if (wide) {
        WCHAR w[40]; MultiByteToWideChar(CP_ACP, 0, face, -1, w, 40);
        f = U(CreateFontW)(-17, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, w);
    } else f = U(CreateFontA)(-17, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, face);
    HGDIOBJ old = U(SelectObject)(dc, f);
    U(GetTextMetricsW)(dc, out);
    U(SelectObject)(dc, old); U(DeleteObject)(f); U(DeleteDC)(dc);
}
TEST(fonts_create_a_w_parity_metrics) {
    FE_BASE(); REQUIRE(U(CreateFontA)); REQUIRE(U(CreateFontW)); REQUIRE(U(GetTextMetricsW));
    for (int i = 0; i < FE_NFACES; i++) {
        TEXTMETRICW a, w;
        metrics_of(fe_faces[i].name, 0, &a); metrics_of(fe_faces[i].name, 1, &w);
        CHECKF(a.tmHeight == w.tmHeight && a.tmAscent == w.tmAscent && a.tmAveCharWidth == w.tmAveCharWidth &&
               a.tmWeight == w.tmWeight && a.tmPitchAndFamily == w.tmPitchAndFamily && a.tmCharSet == w.tmCharSet,
               "face %s", fe_faces[i].name);
    }
}
TEST(fonts_create_indirect_a_w_parity_facename) {
    FE_BASE(); REQUIRE(U(CreateFontIndirectW)); REQUIRE(U(GetTextFaceW));
    LOGFONTW lw; memset(&lw, 0, sizeof lw); lw.lfHeight = -12; lstrcpyW(lw.lfFaceName, L"Courier New");
    HFONT f = U(CreateFontIndirectW)(&lw);
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ old = U(SelectObject)(dc, f);
    WCHAR wn[40]; char an[40];
    int rw = U(GetTextFaceW)(dc, 40, wn), ra = U(GetTextFaceA)(dc, 40, an);
    EXPECT_WSTR(wn, L"Courier New"); EXPECT_STR(an, "Courier New");
    EQ(rw, 12); EQ(ra, 11);
    U(SelectObject)(dc, old); U(DeleteObject)(f); U(DeleteDC)(dc);
}
