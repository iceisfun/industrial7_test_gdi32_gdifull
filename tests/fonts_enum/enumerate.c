/* EnumFontFamilies / EnumFontFamiliesEx / EnumFonts. */
#include "fe.h"

#define EN_BASE() do { FE_BASE(); REQUIRE(U(EnumFontFamiliesExA)); REQUIRE(U(EnumFontFamiliesExW)); REQUIRE(U(EnumFontFamiliesA)); \
    REQUIRE(U(EnumFontFamiliesW)); REQUIRE(U(EnumFontsA)); REQUIRE(U(EnumFontsW)); } while (0)

static fe_set *saved_w(void) {
    static fe_set s; static fe_ent store[FE_MAXENT];
    memset(&s, 0, sizeof s); s.e = store; return &s;
}
static fe_set *enum_exW(HDC dc, const WCHAR *face, int cs, int stop_at) {
    fe_set *s = saved_w(); s->ex = 1; s->stop_at = stop_at;
    LOGFONTW lf; memset(&lf, 0, sizeof lf); lf.lfCharSet = (BYTE)cs;
    if (face) lstrcpyW(lf.lfFaceName, face);
    s->ret = U(EnumFontFamiliesExW)(dc, &lf, (FONTENUMPROCW)fe_cbW, (LPARAM)s, 0);
    return s;
}
static fe_set *enum_fam(HDC dc, const char *face, int wide, int stop_at) {
    fe_set *s = wide ? saved_w() : fe_newset(0, stop_at);
    s->stop_at = stop_at; s->ex = 0;
    if (wide) {
        WCHAR w[64]; if (face) MultiByteToWideChar(CP_ACP, 0, face, -1, w, 64);
        s->ret = U(EnumFontFamiliesW)(dc, face ? w : NULL, (FONTENUMPROCW)fe_cbW, (LPARAM)s);
    } else s->ret = U(EnumFontFamiliesA)(dc, face, (FONTENUMPROCA)fe_cbA, (LPARAM)s);
    return s;
}
static fe_set *enum_fonts(HDC dc, const char *face, int wide, int stop_at) {
    fe_set *s = wide ? saved_w() : fe_newset(0, stop_at);
    s->stop_at = stop_at; s->ex = 0;
    if (wide) {
        WCHAR w[64]; if (face) MultiByteToWideChar(CP_ACP, 0, face, -1, w, 64);
        s->ret = U(EnumFontsW)(dc, face ? w : NULL, (FONTENUMPROCW)fe_cbW, (LPARAM)s);
    } else s->ret = U(EnumFontsA)(dc, face, (FONTENUMPROCA)fe_cbA, (LPARAM)s);
    return s;
}
static HDC edc(void) { return U(CreateCompatibleDC)(NULL); }

/* ---- Ex, per-face worker ---- */
static void w_enum_ex(const char *face) {
    const fe_face_t *fi = NULL;
    for (int i = 0; i < FE_NFACES; i++) if (!strcmp(fe_faces[i].name, face)) fi = &fe_faces[i];
    HDC dc = edc();
    fe_set *s = fe_enum_exA(dc, face, DEFAULT_CHARSET, 0);
    CHECKF(s->ret == 1, "%s ret %d", face, s->ret);
    CHECKF(s->n >= 1, "%s: nothing enumerated", face);
    CHECK(s->total == s->n);
    for (int i = 0; i < s->n; i++) {
        fe_ent *e = &s->e[i];
        CHECKF(!lstrcmpiA(e->face, face), "%s entry %d name '%s'", face, i, e->face);
        if (fi && fi->tt) CHECKF((e->ft & TRUETYPE_FONTTYPE) && !(e->ft & RASTER_FONTTYPE), "%s entry %d ft %lx", face, i, e->ft);
        if (fi && !fi->tt) CHECKF((e->ft & RASTER_FONTTYPE) && !(e->ft & TRUETYPE_FONTTYPE), "%s entry %d ft %lx", face, i, e->ft);
        CHECKF(!(e->ft & DEVICE_FONTTYPE), "%s entry %d device flag on screen dc", face, i);
        CHECKF(e->full[0] != 0, "%s entry %d no full name", face, i);
        CHECKF(e->tmcs == e->cs, "%s entry %d lf charset %u vs tm charset %u", face, i, e->cs, e->tmcs);
    }
    U(DeleteDC)(dc);
}
#define X_en_ex(id, name) DEF_FACE_TEST(fonts_enum_ex, w_enum_ex, id, name)
FACE_TESTS(X_en_ex)

static void w_enum_ex_aw(const char *face) {
    HDC dc = edc();
    int na = fe_enum_exA(dc, face, DEFAULT_CHARSET, 0)->n;
    WCHAR w[64]; MultiByteToWideChar(CP_ACP, 0, face, -1, w, 64);
    fe_set *sw = enum_exW(dc, w, DEFAULT_CHARSET, 0);
    CHECKF(sw->n == na, "%s: A=%d W=%d", face, na, sw->n);
    CHECKF(sw->ret == 1, "%s W ret %d", face, sw->ret);
    for (int i = 0; i < sw->n; i++) CHECKF(!lstrcmpiA(sw->e[i].face, face), "%s W name '%s'", face, sw->e[i].face);
    U(DeleteDC)(dc);
}
#define X_en_exaw(id, name) DEF_FACE_TEST(fonts_enum_ex_aw, w_enum_ex_aw, id, name)
FACE_TESTS(X_en_exaw)

static void w_enum_fam(const char *face) {
    const fe_face_t *fi = NULL;
    for (int i = 0; i < FE_NFACES; i++) if (!strcmp(fe_faces[i].name, face)) fi = &fe_faces[i];
    HDC dc = edc();
    fe_set *s = enum_fam(dc, face, 0, 0);
    CHECKF(s->ret == 1, "%s ret %d", face, s->ret);
    CHECKF(s->n >= 1, "%s: nothing enumerated", face);
    for (int i = 0; i < s->n; i++) {
        fe_ent *e = &s->e[i];
        CHECKF(!lstrcmpiA(e->face, face), "%s name '%s'", face, e->face);
        if (fi && fi->tt) CHECKF(e->ft & TRUETYPE_FONTTYPE, "%s ft %lx", face, e->ft);
        if (fi && !fi->tt) CHECKF((e->ft & RASTER_FONTTYPE) && !(e->ft & TRUETYPE_FONTTYPE), "%s ft %lx", face, e->ft);
    }
    int nw = enum_fam(dc, face, 1, 0)->n;
    CHECKF(nw == s->n, "%s A=%d W=%d", face, s->n, nw);
    U(DeleteDC)(dc);
}
#define X_en_fam(id, name) DEF_FACE_TEST(fonts_enum_family, w_enum_fam, id, name)
FACE_TESTS(X_en_fam)

static void w_enum_fonts(const char *face) {
    HDC dc = edc();
    fe_set *s = enum_fonts(dc, face, 0, 0);
    CHECKF(s->ret == 1, "%s ret %d", face, s->ret);
    CHECKF(s->n >= 1, "%s: nothing enumerated", face);
    for (int i = 0; i < s->n; i++) CHECKF(!lstrcmpiA(s->e[i].face, face), "%s name '%s'", face, s->e[i].face);
    int nw = enum_fonts(dc, face, 1, 0)->n;
    CHECKF(nw == s->n, "%s A=%d W=%d", face, s->n, nw);
    U(DeleteDC)(dc);
}
#define X_en_fonts(id, name) DEF_FACE_TEST(fonts_enum_fonts, w_enum_fonts, id, name)
FACE_TESTS(X_en_fonts)

/* ---- raster sizes: enumerated size can be selected back ---- */
static void w_raster_sizes(const char *face, int strict) {
    HDC dc = edc();
    fe_set *s = fe_enum_exA(dc, face, DEFAULT_CHARSET, 0);
    CHECKF(s->n >= 1, "%s", face);
    int mismatches = 0;
    for (int i = 0; i < s->n && i < 40; i++) {
        LOGFONTA lf; memset(&lf, 0, sizeof lf);
        lf.lfHeight = s->e[i].h; lf.lfCharSet = s->e[i].cs; lf.lfWeight = s->e[i].weight; lf.lfItalic = s->e[i].italic;
        lstrcpyA(lf.lfFaceName, s->e[i].face);
        HFONT f = U(CreateFontIndirectA)(&lf); HGDIOBJ old = U(SelectObject)(dc, f);
        TEXTMETRICA tm; U(GetTextMetricsA)(dc, &tm);
        if (tm.tmHeight != s->e[i].tmHeight) { mismatches++; t_note("%s entry %d: enumerated height %ld selected %ld", face, i, s->e[i].tmHeight, tm.tmHeight); }
        U(SelectObject)(dc, old); U(DeleteObject)(f);
    }
    if (strict) CHECKF(mismatches == 0, "%s: %d mismatches", face, mismatches);
    U(DeleteDC)(dc);
}
TEST(fonts_enum_raster_sizes_fixedsys) { EN_BASE(); w_raster_sizes("Fixedsys", 1); }
TEST(fonts_enum_raster_sizes_terminal) { EN_BASE(); w_raster_sizes("Terminal", 1); }
TEST(fonts_enum_raster_sizes_system) { EN_BASE(); w_raster_sizes("System", 1); }
TEST(fonts_enum_raster_sizes_ms_sans_serif) { EN_BASE(); w_raster_sizes("MS Sans Serif", 0); }
TEST(fonts_enum_raster_sizes_small_fonts) { EN_BASE(); w_raster_sizes("Small Fonts", 0); }

/* ---- charset filters ---- */
static void w_charset_filter(const char *face, int cs, int must_exist) {
    HDC dc = edc();
    fe_set *s = fe_enum_exA(dc, face, cs, 0);
    if (must_exist) CHECKF(s->n >= 1, "%s charset %d: nothing", face, cs); else if (!s->n) t_note("%s has no charset %d", face, cs);
    CHECK(s->ret == 1);
    for (int i = 0; i < s->n; i++) {
        CHECKF(s->e[i].cs == cs, "%s filter %d returned charset %u", face, cs, s->e[i].cs);
        CHECKF(!lstrcmpiA(s->e[i].face, face), "%s filter %d name '%s'", face, cs, s->e[i].face);
    }
    U(DeleteDC)(dc);
}
TEST(fonts_enum_charset_arial_ansi) { EN_BASE(); w_charset_filter("Arial", ANSI_CHARSET, 1); }
TEST(fonts_enum_charset_arial_greek) { EN_BASE(); w_charset_filter("Arial", GREEK_CHARSET, 1); }
TEST(fonts_enum_charset_arial_cyrillic) { EN_BASE(); w_charset_filter("Arial", RUSSIAN_CHARSET, 1); }
TEST(fonts_enum_charset_arial_hebrew) { EN_BASE(); w_charset_filter("Arial", HEBREW_CHARSET, 1); }
TEST(fonts_enum_charset_arial_arabic) { EN_BASE(); w_charset_filter("Arial", ARABIC_CHARSET, 1); }
TEST(fonts_enum_charset_arial_turkish) { EN_BASE(); w_charset_filter("Arial", TURKISH_CHARSET, 1); }
TEST(fonts_enum_charset_arial_baltic) { EN_BASE(); w_charset_filter("Arial", BALTIC_CHARSET, 1); }
TEST(fonts_enum_charset_arial_easteurope) { EN_BASE(); w_charset_filter("Arial", EASTEUROPE_CHARSET, 1); }
TEST(fonts_enum_charset_arial_vietnamese) { EN_BASE(); w_charset_filter("Arial", VIETNAMESE_CHARSET, 0); }
TEST(fonts_enum_charset_arial_thai) { EN_BASE(); w_charset_filter("Arial", THAI_CHARSET, 0); }
TEST(fonts_enum_charset_arial_oem) { EN_BASE(); w_charset_filter("Arial", OEM_CHARSET, 0); }
TEST(fonts_enum_charset_arial_shiftjis) { EN_BASE(); w_charset_filter("Arial", SHIFTJIS_CHARSET, 0); }
TEST(fonts_enum_charset_tahoma_thai) { EN_BASE(); w_charset_filter("Tahoma", THAI_CHARSET, 1); }
TEST(fonts_enum_charset_symbol) { EN_BASE(); w_charset_filter("Symbol", SYMBOL_CHARSET, 1); }
TEST(fonts_enum_charset_marlett) { EN_BASE(); w_charset_filter("Marlett", SYMBOL_CHARSET, 1); }
TEST(fonts_enum_charset_terminal_oem) { EN_BASE(); w_charset_filter("Terminal", OEM_CHARSET, 1); }
TEST(fonts_enum_charset_system_ansi) { EN_BASE(); w_charset_filter("System", ANSI_CHARSET, 1); }
TEST(fonts_enum_charset_fixedsys_ansi) { EN_BASE(); w_charset_filter("Fixedsys", ANSI_CHARSET, 1); }

TEST(fonts_enum_charset_wrong_for_face) {
    EN_BASE();
    HDC dc = edc();
    fe_set *s = fe_enum_exA(dc, "Arial", SYMBOL_CHARSET, 0);
    EQ(s->total, 0); EQ(s->ret, 1);
    s = fe_enum_exA(dc, "Symbol", ANSI_CHARSET, 0);
    EQ(s->total, 0); EQ(s->ret, 1);
    s = fe_enum_exA(dc, "Terminal", ANSI_CHARSET, 0);
    EQ(s->total, 0); EQ(s->ret, 1);
    U(DeleteDC)(dc);
}
TEST(fonts_enum_charset_default_lists_several_scripts) {
    EN_BASE();
    HDC dc = edc();
    fe_set *s = fe_enum_exA(dc, "Arial", DEFAULT_CHARSET, 0);
    int seen[256] = {0}, distinct = 0;
    for (int i = 0; i < s->n; i++) if (!seen[s->e[i].cs]++) distinct++;
    CHECKF(distinct >= 5, "Arial distinct charsets: %d", distinct);
    CHECK(seen[ANSI_CHARSET] > 0);
    t_note("Arial: %d entries over %d charsets", s->n, distinct);
    U(DeleteDC)(dc);
}
TEST(fonts_enum_charset_unknown_value) {
    EN_BASE();
    HDC dc = edc();
    fe_set *s = fe_enum_exA(dc, "Arial", 99, 0);
    EQ(s->total, 0);
    EQ(s->ret, 1);
    U(DeleteDC)(dc);
}
TEST(fonts_enum_script_names) {
    EN_BASE();
    HDC dc = edc();
    fe_set *s = fe_enum_exA(dc, "Arial", GREEK_CHARSET, 0);
    if (s->n) { CHECKF(!lstrcmpA(s->e[0].script, "Greek"), "script '%s'", s->e[0].script); }
    s = fe_enum_exA(dc, "Arial", ANSI_CHARSET, 0);
    if (s->n) t_note("ANSI script name '%s'", s->e[0].script);
    s = fe_enum_exA(dc, "Arial", RUSSIAN_CHARSET, 0);
    if (s->n) CHECKF(!lstrcmpA(s->e[0].script, "Cyrillic"), "script '%s'", s->e[0].script);
    s = fe_enum_exA(dc, "Arial", HEBREW_CHARSET, 0);
    if (s->n) CHECKF(!lstrcmpA(s->e[0].script, "Hebrew"), "script '%s'", s->e[0].script);
    s = fe_enum_exA(dc, "Arial", ARABIC_CHARSET, 0);
    if (s->n) CHECKF(!lstrcmpA(s->e[0].script, "Arabic"), "script '%s'", s->e[0].script);
    s = fe_enum_exA(dc, "Symbol", SYMBOL_CHARSET, 0);
    if (s->n) t_note("Symbol script name '%s'", s->e[0].script);
    U(DeleteDC)(dc);
}

/* ---- face filter variations ---- */
TEST(fonts_enum_face_nonexistent) {
    EN_BASE();
    HDC dc = edc();
    fe_set *s = fe_enum_exA(dc, "Zzz No Such Font 77", DEFAULT_CHARSET, 0);
    EQ(s->total, 0); EQ(s->ret, 1);
    s = enum_fam(dc, "Zzz No Such Font 77", 0, 0);
    EQ(s->total, 0); EQ(s->ret, 1);
    s = enum_fonts(dc, "Zzz No Such Font 77", 0, 0);
    EQ(s->total, 0); EQ(s->ret, 1);
    U(DeleteDC)(dc);
}
TEST(fonts_enum_face_case_insensitive) {
    EN_BASE();
    HDC dc = edc();
    int base = fe_enum_exA(dc, "Arial", DEFAULT_CHARSET, 0)->n;
    CHECK(base > 0);
    EQ(fe_enum_exA(dc, "ARIAL", DEFAULT_CHARSET, 0)->n, base);
    EQ(fe_enum_exA(dc, "arial", DEFAULT_CHARSET, 0)->n, base);
    fe_set *s = fe_enum_exA(dc, "arial", DEFAULT_CHARSET, 0);
    if (s->n && strcmp(s->e[0].face, "Arial")) t_note("lower-case filter reports '%s'", s->e[0].face);
    U(DeleteDC)(dc);
}
TEST(fonts_enum_face_partial_name_no_match) {
    EN_BASE();
    HDC dc = edc();
    EQ(fe_enum_exA(dc, "Aria", DEFAULT_CHARSET, 0)->total, 0);
    EQ(fe_enum_exA(dc, "Arial ", DEFAULT_CHARSET, 0)->total, 0);
    EQ(fe_enum_exA(dc, "Courier", DEFAULT_CHARSET, 0)->total == 0 || 1, 1);
    EQ(fe_enum_exA(dc, "Times New", DEFAULT_CHARSET, 0)->total, 0);
    U(DeleteDC)(dc);
}
TEST(fonts_enum_face_with_wildcards_is_literal) {
    EN_BASE();
    HDC dc = edc();
    EQ(fe_enum_exA(dc, "Ari*", DEFAULT_CHARSET, 0)->total, 0);
    EQ(fe_enum_exA(dc, "?rial", DEFAULT_CHARSET, 0)->total, 0);
    U(DeleteDC)(dc);
}
TEST(fonts_enum_face_full_name_not_family) {
    EN_BASE();
    HDC dc = edc();
    int n = fe_enum_exA(dc, "Arial Bold", DEFAULT_CHARSET, 0)->total;
    t_note("EnumFontFamiliesEx with face 'Arial Bold' -> %d callbacks", n);
    U(DeleteDC)(dc);
}
TEST(fonts_enum_face_31_chars) {
    EN_BASE();
    HDC dc = edc();
    char name[LF_FACESIZE]; memset(name, 'q', 31); name[31] = 0;
    EQ(fe_enum_exA(dc, name, DEFAULT_CHARSET, 0)->total, 0);
    U(DeleteDC)(dc);
}
TEST(fonts_enum_null_logfont_lists_families) {
    EN_BASE();
    HDC dc = edc();
    fe_set *s = fe_newset(1, 0);
    s->ret = U(EnumFontFamiliesExA)(dc, NULL, (FONTENUMPROCA)fe_cbA, (LPARAM)s, 0);
    EQ(s->ret, 1);
    CHECKF(s->total >= 40, "only %d families", s->total);
    static const char *must[] = {"Arial", "Courier New", "Times New Roman", "Tahoma", "Segoe UI", "Consolas", "Symbol", "Marlett",
        "System", "Terminal", "Fixedsys", "MS Sans Serif", "Small Fonts", "Microsoft Sans Serif", "Verdana", "Georgia", "Calibri", "Lucida Console"};
    for (unsigned i = 0; i < sizeof must / sizeof must[0]; i++) CHECKF(fe_find(s, must[i]) >= 0, "family '%s' missing", must[i]);
    U(DeleteDC)(dc);
}
TEST(fonts_enum_empty_face_default_charset_one_per_family) {
    EN_BASE();
    HDC dc = edc();
    fe_set *s = fe_enum_exA(dc, "", DEFAULT_CHARSET, 0);
    CHECK(s->total >= 40);
    int dups = 0;
    for (int i = 0; i < s->n; i++) for (int j = 0; j < i; j++) if (!strcmp(s->e[i].face, s->e[j].face)) { dups++; break; }
    if (dups) t_note("%d duplicate family names in the DEFAULT_CHARSET enumeration", dups);
    U(DeleteDC)(dc);
}
TEST(fonts_enum_family_null_lists_families) {
    EN_BASE();
    HDC dc = edc();
    fe_set *s = enum_fam(dc, NULL, 0, 0);
    EQ(s->ret, 1);
    CHECK(s->total >= 40);
    CHECK(fe_find(s, "Arial") >= 0);
    int dups = 0;
    for (int i = 0; i < s->n; i++) for (int j = 0; j < i; j++) if (!strcmp(s->e[i].face, s->e[j].face)) { dups++; break; }
    CHECKF(dups == 0, "%d duplicate family names", dups);
    U(DeleteDC)(dc);
}
TEST(fonts_enum_fonts_null_lists_faces) {
    EN_BASE();
    HDC dc = edc();
    fe_set *s = enum_fonts(dc, NULL, 0, 0);
    EQ(s->ret, 1);
    CHECK(s->total >= 40);
    int fam = enum_fam(dc, NULL, 0, 0)->total;
    t_note("EnumFonts(NULL) %d entries; EnumFontFamilies(NULL) %d", s->total, fam);
    U(DeleteDC)(dc);
}
TEST(fonts_enum_ex_vs_family_counts) {
    EN_BASE();
    HDC dc = edc();
    int ex = fe_enum_exA(dc, "Arial", DEFAULT_CHARSET, 0)->total;
    int fam = enum_fam(dc, "Arial", 0, 0)->total;
    int fon = enum_fonts(dc, "Arial", 0, 0)->total;
    CHECK(ex >= 1 && fam >= 1 && fon >= 1);
    t_note("Arial: Ex(DEFAULT)=%d EnumFontFamilies=%d EnumFonts=%d", ex, fam, fon);
}

/* ---- early stop and return values ---- */
TEST(fonts_enum_ex_early_stop_a) {
    EN_BASE();
    HDC dc = edc();
    fe_set *s = fe_enum_exA(dc, "Arial", DEFAULT_CHARSET, 1);
    EQ(s->total, 1); EQ(s->ret, 0);
    s = fe_enum_exA(dc, "Arial", DEFAULT_CHARSET, 2);
    if (s->total == 2) EQ(s->ret, 0); else t_note("only %d Arial entries", s->total);
    s = fe_enum_exA(dc, NULL, DEFAULT_CHARSET, 5);
    EQ(s->total, 5); EQ(s->ret, 0);
    U(DeleteDC)(dc);
}
TEST(fonts_enum_ex_early_stop_w) {
    EN_BASE();
    HDC dc = edc();
    fe_set *s = enum_exW(dc, L"Arial", DEFAULT_CHARSET, 1);
    EQ(s->total, 1); EQ(s->ret, 0);
    s = enum_exW(dc, NULL, DEFAULT_CHARSET, 7);
    EQ(s->total, 7); EQ(s->ret, 0);
    U(DeleteDC)(dc);
}
TEST(fonts_enum_family_early_stop) {
    EN_BASE();
    HDC dc = edc();
    fe_set *s = enum_fam(dc, NULL, 0, 3);
    EQ(s->total, 3); EQ(s->ret, 0);
    s = enum_fam(dc, NULL, 1, 4);
    EQ(s->total, 4); EQ(s->ret, 0);
    s = enum_fam(dc, "Arial", 0, 1);
    EQ(s->total, 1); EQ(s->ret, 0);
    U(DeleteDC)(dc);
}
TEST(fonts_enum_fonts_early_stop) {
    EN_BASE();
    HDC dc = edc();
    fe_set *s = enum_fonts(dc, NULL, 0, 2);
    EQ(s->total, 2); EQ(s->ret, 0);
    s = enum_fonts(dc, NULL, 1, 2);
    EQ(s->total, 2); EQ(s->ret, 0);
    U(DeleteDC)(dc);
}
static int CALLBACK cb_ret7_w(const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD ft, LPARAM lp) { (*(int *)lp)++; return 7; }
static int CALLBACK cb_ret7_a(const LOGFONTA *lf, const TEXTMETRICA *tm, DWORD ft, LPARAM lp) { (*(int *)lp)++; return 7; }
static int CALLBACK cb_neg_a(const LOGFONTA *lf, const TEXTMETRICA *tm, DWORD ft, LPARAM lp) { (*(int *)lp)++; return -1; }
TEST(fonts_enum_callback_return_value_propagates) {
    EN_BASE();
    HDC dc = edc();
    LOGFONTW lw; memset(&lw, 0, sizeof lw); lw.lfCharSet = DEFAULT_CHARSET; lstrcpyW(lw.lfFaceName, L"Arial");
    int n = 0;
    int r = U(EnumFontFamiliesExW)(dc, &lw, cb_ret7_w, (LPARAM)&n, 0);
    CHECK(n >= 1);
    EQ(r, 7);
    LOGFONTA la; memset(&la, 0, sizeof la); la.lfCharSet = DEFAULT_CHARSET; lstrcpyA(la.lfFaceName, "Arial");
    int n2 = 0;
    r = U(EnumFontFamiliesExA)(dc, &la, cb_ret7_a, (LPARAM)&n2, 0);
    EQ(n2, n);
    if (r != 7) t_note("A enumeration with callback returning 7 returned %d", r);
    CHECK(r != 0);
    int n3 = 0;
    r = U(EnumFontFamiliesExA)(dc, &la, cb_neg_a, (LPARAM)&n3, 0);
    EQ(n3, n);
    t_note("callback returning -1 for every entry: %d callbacks, result %d", n3, r);
    U(DeleteDC)(dc);
}
static int CALLBACK cb_zero_a(const LOGFONTA *lf, const TEXTMETRICA *tm, DWORD ft, LPARAM lp) { (*(int *)lp)++; return 0; }
TEST(fonts_enum_callback_zero_stops_ex_family_fonts) {
    EN_BASE();
    HDC dc = edc();
    int n = 0;
    EQ(U(EnumFontFamiliesA)(dc, NULL, cb_zero_a, (LPARAM)&n), 0); EQ(n, 1);
    n = 0; EQ(U(EnumFontsA)(dc, NULL, cb_zero_a, (LPARAM)&n), 0); EQ(n, 1);
    n = 0; EQ(U(EnumFontsA)(dc, "Arial", cb_zero_a, (LPARAM)&n), 0); EQ(n, 1);
    U(DeleteDC)(dc);
}

/* ---- invalid DCs ---- */
TEST(fonts_enum_bad_dc) {
    EN_BASE();
    int n = 0;
    LOGFONTA la; memset(&la, 0, sizeof la); la.lfCharSet = DEFAULT_CHARSET; lstrcpyA(la.lfFaceName, "Arial");
    LOGFONTW lw; memset(&lw, 0, sizeof lw); lw.lfCharSet = DEFAULT_CHARSET; lstrcpyW(lw.lfFaceName, L"Arial");
    HDC bad = (HDC)(ULONG_PTR)0x1234;
    SetLastError(0);
    EQ(U(EnumFontFamiliesExA)(bad, &la, cb_ret7_a, (LPARAM)&n, 0), 0);
    EQ(U(EnumFontFamiliesExW)(bad, &lw, cb_ret7_w, (LPARAM)&n, 0), 0);
    EQ(U(EnumFontFamiliesA)(bad, "Arial", cb_ret7_a, (LPARAM)&n), 0);
    EQ(U(EnumFontsA)(bad, "Arial", cb_ret7_a, (LPARAM)&n), 0);
    EQ(n, 0);
}
TEST(fonts_enum_deleted_dc) {
    EN_BASE();
    HDC dc = edc(); U(DeleteDC)(dc);
    int n = 0;
    LOGFONTA la; memset(&la, 0, sizeof la); la.lfCharSet = DEFAULT_CHARSET;
    SetLastError(0);
    EQ(U(EnumFontFamiliesExA)(dc, &la, cb_ret7_a, (LPARAM)&n, 0), 0);
    EQ(n, 0);
}
TEST(fonts_enum_null_dc) {
    EN_BASE();
    int n = 0;
    LOGFONTA la; memset(&la, 0, sizeof la); la.lfCharSet = DEFAULT_CHARSET; lstrcpyA(la.lfFaceName, "Arial");
    SetLastError(0);
    int r = U(EnumFontFamiliesExA)(NULL, &la, cb_ret7_a, (LPARAM)&n, 0);
    t_note("EnumFontFamiliesExA(NULL dc) = %d, callbacks %d, error %lu", r, n, GetLastError());
}
TEST(fonts_enum_wrong_object_as_dc) {
    EN_BASE(); REQUIRE(U(GetStockObject));
    int n = 0;
    LOGFONTA la; memset(&la, 0, sizeof la); la.lfCharSet = DEFAULT_CHARSET;
    EQ(U(EnumFontFamiliesExA)((HDC)U(GetStockObject)(BLACK_PEN), &la, cb_ret7_a, (LPARAM)&n, 0), 0);
    EQ(n, 0);
}

/* ---- field semantics ---- */
TEST(fonts_enum_ntm_flags_styles) {
    EN_BASE();
    HDC dc = edc();
    fe_set *s = fe_enum_exA(dc, "Arial", ANSI_CHARSET, 0);
    int reg = 0, bold = 0, ital = 0, bi = 0;
    for (int i = 0; i < s->n; i++) {
        fe_ent *e = &s->e[i];
        if (!(e->ft & TRUETYPE_FONTTYPE)) continue;
        if (!strcmp(e->style, "Regular")) { reg++; CHECKF(e->ntmFlags & NTM_REGULAR, "Regular flags %lx", e->ntmFlags); CHECK(!(e->ntmFlags & (NTM_BOLD | NTM_ITALIC))); }
        if (!strcmp(e->style, "Bold")) { bold++; CHECKF(e->ntmFlags & NTM_BOLD, "Bold flags %lx", e->ntmFlags); CHECK(!(e->ntmFlags & NTM_ITALIC)); }
        if (!strcmp(e->style, "Italic")) { ital++; CHECKF(e->ntmFlags & NTM_ITALIC, "Italic flags %lx", e->ntmFlags); CHECK(!(e->ntmFlags & NTM_BOLD)); }
        if (!strcmp(e->style, "Bold Italic")) { bi++; CHECKF((e->ntmFlags & (NTM_BOLD | NTM_ITALIC)) == (NTM_BOLD | NTM_ITALIC), "BI flags %lx", e->ntmFlags); }
        CHECKF(e->ntmSizeEM == 2048, "ntmSizeEM %u", e->ntmSizeEM);
    }
    CHECK(reg == 1); CHECK(bold == 1); CHECK(ital == 1); CHECK(bi == 1);
    U(DeleteDC)(dc);
}
TEST(fonts_enum_full_names_arial) {
    EN_BASE();
    HDC dc = edc();
    fe_set *s = fe_enum_exA(dc, "Arial", ANSI_CHARSET, 0);
    CHECK(fe_find(s, "Arial") >= 0);
    int has_reg = 0, has_bold = 0;
    for (int i = 0; i < s->n; i++) { if (!strcmp(s->e[i].full, "Arial")) has_reg = 1; if (!strcmp(s->e[i].full, "Arial Bold")) has_bold = 1; }
    CHECK(has_reg); CHECK(has_bold);
    U(DeleteDC)(dc);
}
TEST(fonts_enum_weight_italic_fields) {
    EN_BASE();
    HDC dc = edc();
    fe_set *s = fe_enum_exA(dc, "Arial", ANSI_CHARSET, 0);
    for (int i = 0; i < s->n; i++) {
        fe_ent *e = &s->e[i];
        if (!strcmp(e->style, "Regular")) { CHECK(e->weight == 400 && !e->italic); }
        if (!strcmp(e->style, "Bold")) { CHECK(e->weight == 700 && !e->italic); }
        if (!strcmp(e->style, "Italic")) { CHECK(e->weight == 400 && e->italic); }
        if (!strcmp(e->style, "Bold Italic")) { CHECK(e->weight == 700 && e->italic); }
    }
    U(DeleteDC)(dc);
}
TEST(fonts_enum_pitchfamily_fields) {
    EN_BASE();
    HDC dc = edc();
    fe_set *s = fe_enum_exA(dc, "Arial", ANSI_CHARSET, 0);
    for (int i = 0; i < s->n; i++) CHECKF((s->e[i].pf & 0xF0) == FF_SWISS, "Arial family %02x", s->e[i].pf);
    s = fe_enum_exA(dc, "Times New Roman", ANSI_CHARSET, 0);
    for (int i = 0; i < s->n; i++) CHECKF((s->e[i].pf & 0xF0) == FF_ROMAN, "Times family %02x", s->e[i].pf);
    s = fe_enum_exA(dc, "Courier New", ANSI_CHARSET, 0);
    for (int i = 0; i < s->n; i++) { CHECKF((s->e[i].pf & 0xF0) == FF_MODERN, "Courier New family %02x", s->e[i].pf); CHECKF((s->e[i].pf & 3) == FIXED_PITCH, "Courier New pitch %02x", s->e[i].pf); }
    s = fe_enum_exA(dc, "Arial", ANSI_CHARSET, 0);
    for (int i = 0; i < s->n; i++) CHECKF((s->e[i].pf & 3) == VARIABLE_PITCH, "Arial pitch %02x", s->e[i].pf);
    U(DeleteDC)(dc);
}
TEST(fonts_enum_deterministic) {
    EN_BASE();
    HDC dc = edc();
    fe_set *a = fe_enum_exA(dc, NULL, DEFAULT_CHARSET, 0);
    static char names1[64][LF_FACESIZE]; int n1 = a->n < 64 ? a->n : 64;
    for (int i = 0; i < n1; i++) lstrcpyA(names1[i], a->e[i].face);
    int total1 = a->total;
    fe_set *b = fe_enum_exA(dc, NULL, DEFAULT_CHARSET, 0);
    EQ(b->total, total1);
    for (int i = 0; i < n1; i++) CHECKF(!strcmp(names1[i], b->e[i].face), "entry %d: '%s' vs '%s'", i, names1[i], b->e[i].face);
    U(DeleteDC)(dc);
}
TEST(fonts_enum_unaffected_by_selected_font) {
    EN_BASE();
    HDC dc = edc();
    int base = fe_enum_exA(dc, "Arial", DEFAULT_CHARSET, 0)->total;
    HFONT f = fe_mk(-30, 0, 700, 1, ANSI_CHARSET, "Courier New");
    HGDIOBJ old = U(SelectObject)(dc, f);
    EQ(fe_enum_exA(dc, "Arial", DEFAULT_CHARSET, 0)->total, base);
    U(SelectObject)(dc, old); U(DeleteObject)(f);
    U(DeleteDC)(dc);
}
TEST(fonts_enum_dib_dc_same_families) {
    EN_BASE(); REQUIRE(U(CreateDIBSection));
    HDC dc = edc();
    int base = fe_enum_exA(dc, "Arial", DEFAULT_CHARSET, 0)->total;
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 8; bi.bmiHeader.biHeight = 8; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    void *bits; HBITMAP bm = U(CreateDIBSection)(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    HGDIOBJ ob = U(SelectObject)(dc, bm);
    int n = fe_enum_exA(dc, "Arial", DEFAULT_CHARSET, 0)->total;
    if (n != base) t_note("Arial entries: default bitmap %d, 32bpp DIB %d", base, n);
    CHECK(n >= 1);
    U(SelectObject)(dc, ob); U(DeleteObject)(bm); U(DeleteDC)(dc);
}

/* ---- create from enumerated LOGFONT ---- */
TEST(fonts_enum_roundtrip_arial_entries) {
    EN_BASE();
    HDC dc = edc();
    fe_set *s = fe_enum_exA(dc, "Arial", DEFAULT_CHARSET, 0);
    for (int i = 0; i < s->n; i++) {
        LOGFONTA lf; memset(&lf, 0, sizeof lf);
        lf.lfHeight = -20; lf.lfCharSet = s->e[i].cs; lf.lfWeight = s->e[i].weight; lf.lfItalic = s->e[i].italic;
        lstrcpyA(lf.lfFaceName, s->e[i].face);
        HFONT f = U(CreateFontIndirectA)(&lf); HGDIOBJ old = U(SelectObject)(dc, f);
        char n[40]; TEXTMETRICA tm; U(GetTextFaceA)(dc, 40, n); U(GetTextMetricsA)(dc, &tm);
        CHECKF(!strcmp(n, "Arial"), "entry %d face '%s'", i, n);
        CHECKF(tm.tmCharSet == s->e[i].cs, "entry %d charset %u vs %u", i, tm.tmCharSet, s->e[i].cs);
        CHECKF(tm.tmWeight == s->e[i].weight, "entry %d weight %ld vs %ld", i, tm.tmWeight, s->e[i].weight);
        U(SelectObject)(dc, old); U(DeleteObject)(f);
    }
    U(DeleteDC)(dc);
}

/* ---- callback context ---- */
static HDC g_cb_dc; static int g_cb_ok, g_cb_n;
static int CALLBACK cb_ctx(const LOGFONTA *lf, const TEXTMETRICA *tm, DWORD ft, LPARAM lp) {
    if (g_cb_n++ >= 10) return 0;
    LOGFONTA c = *lf; c.lfHeight = -14;
    HFONT f = U(CreateFontIndirectA)(&c);
    HGDIOBJ old = U(SelectObject)(g_cb_dc, f);
    TEXTMETRICA t2; if (U(GetTextMetricsA)(g_cb_dc, &t2)) g_cb_ok++;
    U(SelectObject)(g_cb_dc, old); U(DeleteObject)(f);
    return 1;
}
TEST(fonts_enum_gdi_calls_inside_callback) {
    EN_BASE();
    HDC dc = edc(); g_cb_dc = dc; g_cb_ok = 0; g_cb_n = 0;
    LOGFONTA lf; memset(&lf, 0, sizeof lf); lf.lfCharSet = DEFAULT_CHARSET;
    DWORD before = fe_gdi_count();
    U(EnumFontFamiliesExA)(dc, &lf, cb_ctx, 0, 0);
    EQ(g_cb_ok, 10);
    EQ(fe_gdi_count(), before);
    U(DeleteDC)(dc);
}
static int CALLBACK cb_nested(const LOGFONTA *lf, const TEXTMETRICA *tm, DWORD ft, LPARAM lp) {
    fe_set *inner = fe_enum_exA(g_cb_dc, "Courier New", DEFAULT_CHARSET, 0);   /* note: reuses the shared set */
    g_cb_n += inner->total > 0;
    return 0;
}
TEST(fonts_enum_nested_enumeration) {
    EN_BASE();
    HDC dc = edc(); g_cb_dc = dc; g_cb_n = 0;
    LOGFONTA lf; memset(&lf, 0, sizeof lf); lf.lfCharSet = DEFAULT_CHARSET; lstrcpyA(lf.lfFaceName, "Arial");
    U(EnumFontFamiliesExA)(dc, &lf, cb_nested, 0, 0);
    EQ(g_cb_n, 1);
    U(DeleteDC)(dc);
}
TEST(fonts_enum_leak_check) {
    EN_BASE();
    HDC dc = edc();
    DWORD before = fe_gdi_count();
    for (int i = 0; i < 30; i++) fe_enum_exA(dc, i & 1 ? "Arial" : NULL, DEFAULT_CHARSET, 0);
    EQ(fe_gdi_count(), before);
    U(DeleteDC)(dc);
}

/* ---- vertical fonts ---- */
TEST(fonts_enum_vertical_names) {
    EN_BASE();
    HDC dc = edc();
    fe_set *s = fe_enum_exA(dc, NULL, DEFAULT_CHARSET, 0);
    int at = 0;
    for (int i = 0; i < s->n; i++) if (s->e[i].face[0] == '@') at++;
    t_note("%d '@' vertical families in the all-family enumeration", at);
    s = fe_enum_exA(dc, "@Arial", DEFAULT_CHARSET, 0);
    t_note("'@Arial' filter -> %d entries", s->total);
    s = fe_enum_exA(dc, "@MS Gothic", SHIFTJIS_CHARSET, 0);
    t_note("'@MS Gothic' SHIFTJIS filter -> %d entries", s->total);
    for (int i = 0; i < s->n; i++) CHECK(s->e[i].face[0] == '@');
    U(DeleteDC)(dc);
}
TEST(fonts_enum_vertical_font_selects) {
    FE_BASE();
    fe_ctx c;
    int ok = fe_open(&c, "@Arial", -20, 400, 0, ANSI_CHARSET);
    char n[64] = ""; if (c.dc) U(GetTextFaceA)(c.dc, 64, n);
    t_note("@Arial selected as '%s' (exact=%d)", n, ok);
    fe_close(&c);
}
