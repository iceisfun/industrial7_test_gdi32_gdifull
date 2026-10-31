/* AddFontResourceTracking / AddFontResourceWImpl / CreateFontIndirectWImpl / CreateScalableFontResourceWImpl.
 * Only stock system font files are added (and always removed again); temp files are deleted. */
#include "g2.h"

typedef int (WINAPI *AddFontResourceTracking_pfn)(LPCSTR, int);
typedef int (WINAPI *AddFontResourceWImpl_pfn)(LPCWSTR);
typedef HFONT (WINAPI *CreateFontIndirectWImpl_pfn)(const LOGFONTW *);
typedef BOOL (WINAPI *CreateScalableFontResourceWImpl_pfn)(DWORD, LPCWSTR, LPCWSTR, LPCWSTR);

static AddFontResourceTracking_pfn aft(void) {
    FARPROC p = t_proc_gdi32full("AddFontResourceTracking");
    if (!p) p = t_proc_gdi32("AddFontResourceTracking");
    return (AddFontResourceTracking_pfn)p;
}
static AddFontResourceWImpl_pfn afw(void) {
    FARPROC p = t_proc_gdi32full("AddFontResourceWImpl");
    if (!p && g2_wine()) p = t_proc("AddFontResourceW");
    return (AddFontResourceWImpl_pfn)p;
}
static CreateFontIndirectWImpl_pfn cfi(void) {
    FARPROC p = t_proc_gdi32full("CreateFontIndirectWImpl");
    if (!p && g2_wine()) p = t_proc("CreateFontIndirectW");
    return (CreateFontIndirectWImpl_pfn)p;
}
static CreateScalableFontResourceWImpl_pfn csf(void) {
    FARPROC p = t_proc_gdi32full("CreateScalableFontResourceWImpl");
    if (!p && g2_wine()) p = t_proc("CreateScalableFontResourceW");
    return (CreateScalableFontResourceWImpl_pfn)p;
}

static int fonts_dir(char *buf, int n) {
    UINT l = GetWindowsDirectoryA(buf, n);
    if (!l || l + 8 >= (UINT)n) return 0;
    strcat(buf, "\\Fonts\\");
    return 1;
}
static int font_path(const char *file, char *out, int n) {
    if (!fonts_dir(out, n)) return 0;
    strcat(out, file);
    return GetFileAttributesA(out) != INVALID_FILE_ATTRIBUTES;
}
static void widen(const char *s, WCHAR *w, int n) { MultiByteToWideChar(CP_ACP, 0, s, -1, w, n); }

/* ------------------------------ AddFontResourceTracking ------------------------------ */
#define NEED_AFT() do { SKIP_ON_WINE("tracking entry point is a stub"); REQUIRE(aft()); REQUIRE(U(RemoveFontResourceA)); } while (0)
TEST(gap2_aft_missing_file) {
    NEED_AFT();
    SetLastError(0);
    EXPECT_EQ(aft()("C:\\no\\such\\dir\\gap2_missing.ttf", 0), 0);
    t_note("missing file last error %lu", GetLastError());
}
TEST(gap2_aft_empty_string) {
    NEED_AFT();
    EXPECT_EQ(aft()("", 0), 0);
}
TEST(gap2_aft_directory) {
    NEED_AFT();
    char d[MAX_PATH];
    if (!fonts_dir(d, sizeof d)) { t_skip("no windows dir"); return; }
    EXPECT_EQ(aft()(d, 0), 0);
    d[strlen(d) - 1] = 0;
    EXPECT_EQ(aft()(d, 0), 0);
}
TEST(gap2_aft_not_a_font_file) {
    NEED_AFT();
    char p[MAX_PATH], t[MAX_PATH];
    GetTempPathA(sizeof t, t);
    snprintf(p, sizeof p, "%sgap2_nf_%lu.ttf", t, GetCurrentProcessId());
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot write temp file"); return; }
    DWORD w; WriteFile(h, "this is not a font file", 23, &w, NULL); CloseHandle(h);
    EXPECT_EQ(aft()(p, 0), 0);
    DeleteFileA(p);
}
TEST(gap2_aft_zero_length_file) {
    NEED_AFT();
    char p[MAX_PATH], t[MAX_PATH];
    GetTempPathA(sizeof t, t);
    snprintf(p, sizeof p, "%sgap2_zero_%lu.ttf", t, GetCurrentProcessId());
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot write temp file"); return; }
    CloseHandle(h);
    EXPECT_EQ(aft()(p, 0), 0);
    DeleteFileA(p);
}
TEST(gap2_aft_system_font_add_remove) {
    NEED_AFT();
    char p[MAX_PATH];
    if (!font_path("arial.ttf", p, sizeof p)) { t_skip("arial.ttf not present"); return; }
    int n = aft()(p, 0);
    CHECK(n == 1);
    if (n) CHECK(U(RemoveFontResourceA)(p));
}
TEST(gap2_aft_matches_addfontresourcea) {
    NEED_AFT(); REQUIRE(U(AddFontResourceA));
    static const char *const files[] = { "arial.ttf", "cour.ttf", "times.ttf", "tahoma.ttf", "consola.ttf", "marlett.ttf", "symbol.ttf", "segoeui.ttf" };
    for (unsigned i = 0; i < sizeof files / sizeof files[0]; i++) {
        char p[MAX_PATH];
        if (!font_path(files[i], p, sizeof p)) continue;
        int a = U(AddFontResourceA)(p);
        int b = aft()(p, 0);
        CHECKF(a == b, "%s: AddFontResourceA %d tracking %d", files[i], a, b);
        CHECKF(b >= 1, "%s: nothing added", files[i]);
        if (a) U(RemoveFontResourceA)(p);
        if (b) U(RemoveFontResourceA)(p);
    }
}
TEST(gap2_aft_tracking_id_irrelevant_on_hard_drive) {
    NEED_AFT();
    char p[MAX_PATH];
    if (!font_path("arial.ttf", p, sizeof p)) { t_skip("arial.ttf not present"); return; }
    static const int ids[] = { 0, 1, 2, -1, 0x7fffffff, (int)0x80000000 };
    for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        int n = aft()(p, ids[i]);
        CHECKF(n == 1, "id %d -> %d", ids[i], n);
        if (n) CHECK(U(RemoveFontResourceA)(p));
    }
}
TEST(gap2_aft_balanced_refcount) {
    NEED_AFT();
    char p[MAX_PATH];
    if (!font_path("cour.ttf", p, sizeof p)) { t_skip("cour.ttf not present"); return; }
    int added = 0;
    for (int i = 0; i < 5; i++) if (aft()(p, 0)) added++;
    CHECK(added == 5);
    int removed = 0;
    for (int i = 0; i < added; i++) if (U(RemoveFontResourceA)(p)) removed++;
    CHECK(removed == added);
}
TEST(gap2_aft_case_insensitive_path) {
    NEED_AFT();
    char p[MAX_PATH], q[MAX_PATH];
    if (!font_path("arial.ttf", p, sizeof p)) { t_skip("arial.ttf not present"); return; }
    strcpy(q, p); CharUpperA(q);
    int a = aft()(p, 0), b = aft()(q, 0);
    CHECK(a == 1); CHECK(b == 1);
    if (a) U(RemoveFontResourceA)(p);
    if (b) U(RemoveFontResourceA)(q);
}
TEST(gap2_aft_forward_slashes_and_dots) {
    NEED_AFT();
    char p[MAX_PATH], d[MAX_PATH];
    if (!font_path("arial.ttf", p, sizeof p)) { t_skip("arial.ttf not present"); return; }
    fonts_dir(d, sizeof d);
    snprintf(p, sizeof p, "%s.\\arial.ttf", d);
    int n = aft()(p, 0);
    t_note("path with .\\ component -> %d", n);
    if (n) U(RemoveFontResourceA)(p);
}
TEST(gap2_aft_bare_filename_search) {
    NEED_AFT();
    char p[MAX_PATH];
    if (!font_path("arial.ttf", p, sizeof p)) { t_skip("arial.ttf not present"); return; }
    int n = aft()("arial.ttf", 0);
    t_note("bare file name -> %d", n);
    if (n) U(RemoveFontResourceA)("arial.ttf");
}
TEST(gap2_aft_bitmap_fon) {
    NEED_AFT();
    char p[MAX_PATH];
    if (!font_path("serife.fon", p, sizeof p)) { t_skip("serife.fon not present"); return; }
    int n = aft()(p, 0);
    CHECK(n >= 1);
    if (n) U(RemoveFontResourceA)(p);
}
TEST(gap2_aft_collection_returns_face_count) {
    NEED_AFT();
    char p[MAX_PATH];
    if (!font_path("msgothic.ttc", p, sizeof p)) { t_skip("msgothic.ttc not present"); return; }
    int n = aft()(p, 0);
    t_note("msgothic.ttc faces added: %d", n);
    CHECK(n >= 2);
    if (n) U(RemoveFontResourceA)(p);
}
TEST(gap2_aft_copy_of_font_in_temp) {
    NEED_AFT();
    char src[MAX_PATH], dst[MAX_PATH], t[MAX_PATH];
    if (!font_path("arial.ttf", src, sizeof src)) { t_skip("arial.ttf not present"); return; }
    GetTempPathA(sizeof t, t);
    snprintf(dst, sizeof dst, "%sgap2_copy_%lu.ttf", t, GetCurrentProcessId());
    if (!CopyFileA(src, dst, FALSE)) { t_skip("cannot copy font"); return; }
    int n = aft()(dst, 7);
    CHECK(n >= 1);
    if (n) {
        CHECK(U(RemoveFontResourceA)(dst));
        SetLastError(0);
        CHECK(!U(RemoveFontResourceA)(dst));   /* nothing left to remove */
    }
    int ok = 0;
    for (int i = 0; i < 40 && !ok; i++) { ok = DeleteFileA(dst); if (!ok) Sleep(50); }
    if (!ok) t_note("temp font copy still locked");
}
TEST(gap2_aft_remove_without_add) {
    NEED_AFT();
    SetLastError(0);
    BOOL r = U(RemoveFontResourceA)("C:\\no\\such\\gap2_never_added.ttf");
    CHECK(!r);
    t_note("RemoveFontResource of never-added file err %lu", GetLastError());
}
TEST(gap2_aft_gdi_object_count_stable) {
    NEED_AFT();
    char p[MAX_PATH];
    if (!font_path("arial.ttf", p, sizeof p)) { t_skip("arial.ttf not present"); return; }
    DWORD base = g2_gdi_count();
    for (int i = 0; i < 20; i++) { if (aft()(p, 0)) U(RemoveFontResourceA)(p); }
    EXPECT_GDI(base);
}
TEST(gap2_aft_max_path_length_name) {
    NEED_AFT();
    char p[MAX_PATH + 1];
    memset(p, 'a', 250); strcpy(p + 250, ".ttf");
    EXPECT_EQ(aft()(p, 0), 0);
    char q[MAX_PATH + 1];
    strcpy(q, "C:\\");
    memset(q + 3, 'b', 200); strcpy(q + 203, ".ttf");
    EXPECT_EQ(aft()(q, 0), 0);
}
TEST(gap2_aft_one_char_names) {
    NEED_AFT();
    EXPECT_EQ(aft()("x", 0), 0);
    EXPECT_EQ(aft()("\\", 0), 0);
    EXPECT_EQ(aft()(".", 0), 0);
    EXPECT_EQ(aft()("..", 0), 0);
}
TEST(gap2_aft_invalid_characters) {
    NEED_AFT();
    SKIP_ON_WINE("wine chokes on wildcard characters");
    EXPECT_EQ(aft()("C:\\bad|name?.ttf", 0), 0);
    EXPECT_EQ(aft()("C:\\bad<name>.ttf", 0), 0);
}

/* ------------------------------ AddFontResourceWImpl ------------------------------ */
#define NEED_AFW() do { REQUIRE(afw()); REQUIRE(U(RemoveFontResourceW)); } while (0)
TEST(gap2_afw_missing_file) {
    NEED_AFW();
    EXPECT_EQ(afw()(L"C:\\no\\such\\dir\\gap2_missing.ttf"), 0);
}
TEST(gap2_afw_empty_string) {
    NEED_AFW();
    EXPECT_EQ(afw()(L""), 0);
}
TEST(gap2_afw_system_font_add_remove) {
    NEED_AFW();
    char p[MAX_PATH]; WCHAR w[MAX_PATH];
    if (!font_path("arial.ttf", p, sizeof p)) { t_skip("arial.ttf not present"); return; }
    widen(p, w, MAX_PATH);
    int n = afw()(w);
    CHECK(n == 1);
    if (n) CHECK(U(RemoveFontResourceW)(w));
}
TEST(gap2_afw_matches_public_w_and_tracking) {
    NEED_AFW(); REQUIRE(U(AddFontResourceW)); REQUIRE(aft());
    static const char *const files[] = { "arial.ttf", "cour.ttf", "tahoma.ttf", "consola.ttf", "marlett.ttf" };
    for (unsigned i = 0; i < sizeof files / sizeof files[0]; i++) {
        char p[MAX_PATH]; WCHAR w[MAX_PATH];
        if (!font_path(files[i], p, sizeof p)) continue;
        widen(p, w, MAX_PATH);
        int a = afw()(w), b = U(AddFontResourceW)(w), c = aft()(p, 0);
        CHECKF(a == b && b == c && a >= 1, "%s: impl %d public %d tracking %d", files[i], a, b, c);
        if (a) U(RemoveFontResourceW)(w);
        if (b) U(RemoveFontResourceW)(w);
        if (c) U(RemoveFontResourceW)(w);
    }
}
TEST(gap2_afw_not_a_font_file) {
    NEED_AFW();
    char p[MAX_PATH], t[MAX_PATH]; WCHAR w[MAX_PATH];
    GetTempPathA(sizeof t, t);
    snprintf(p, sizeof p, "%sgap2_nfw_%lu.fon", t, GetCurrentProcessId());
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot write temp file"); return; }
    BYTE junk[512]; memset(junk, 0x5A, sizeof junk);
    DWORD wr; WriteFile(h, junk, sizeof junk, &wr, NULL); CloseHandle(h);
    widen(p, w, MAX_PATH);
    EXPECT_EQ(afw()(w), 0);
    DeleteFileA(p);
}
TEST(gap2_afw_unicode_name_missing) {
    NEED_AFW();
    static const WCHAR n[] = { 'C', ':', '\\', 0x4e2d, 0x6587, '.', 't', 't', 'f', 0 };
    EXPECT_EQ(afw()(n), 0);
}
TEST(gap2_afw_added_font_enumerable) {
    NEED_AFW(); REQUIRE(U(EnumFontFamiliesExW)); REQUIRE(U(CreateCompatibleDC));
    char p[MAX_PATH]; WCHAR w[MAX_PATH];
    if (!font_path("marlett.ttf", p, sizeof p)) { t_skip("marlett.ttf not present"); return; }
    widen(p, w, MAX_PATH);
    int n = afw()(w);
    CHECK(n >= 1);
    HDC dc = U(CreateCompatibleDC)(NULL);
    LOGFONTW lf; memset(&lf, 0, sizeof lf); lf.lfCharSet = DEFAULT_CHARSET; lstrcpyW(lf.lfFaceName, L"Marlett");
    struct cnt { int n; } c = { 0 };
    int CALLBACK cb(const LOGFONTW *l, const TEXTMETRICW *t, DWORD ty, LPARAM lp) { ((struct cnt *)lp)->n++; return 1; }
    U(EnumFontFamiliesExW)(dc, &lf, (FONTENUMPROCW)cb, (LPARAM)&c, 0);
    CHECK(c.n >= 1);
    U(DeleteDC)(dc);
    if (n) U(RemoveFontResourceW)(w);
}

/* ------------------------------ CreateFontIndirectWImpl ------------------------------ */
#define NEED_CFI() do { REQUIRE(cfi()); REQUIRE(U(GetObjectW)); REQUIRE(U(DeleteObject)); REQUIRE(U(GetObjectType)); } while (0)
static void lf_init(LOGFONTW *lf, const WCHAR *face, int h) {
    memset(lf, 0, sizeof *lf);
    lf->lfHeight = h; lf->lfCharSet = DEFAULT_CHARSET; lf->lfWeight = FW_NORMAL;
    lstrcpyW(lf->lfFaceName, face);
}
TEST(gap2_cfi_null) {
    NEED_CFI();
    SKIP_ON_WINE("last-error behaviour differs");
    SetLastError(0x4321);
    HFONT f = cfi()(NULL);
    CHECK(f == NULL);
    EXPECT_ERR(0, 0x4321);   /* NULL input is rejected without touching the last error */
}
TEST(gap2_cfi_basic) {
    NEED_CFI();
    LOGFONTW lf; lf_init(&lf, L"Arial", -16);
    HFONT f = cfi()(&lf);
    CHECK(f != NULL);
    EXPECT_EQ(U(GetObjectType)(f), OBJ_FONT);
    CHECK(U(DeleteObject)(f));
}
TEST(gap2_cfi_zeroed_logfont) {
    NEED_CFI();
    LOGFONTW lf; memset(&lf, 0, sizeof lf);
    HFONT f = cfi()(&lf);
    CHECK(f != NULL);
    LOGFONTW o; memset(&o, 0x55, sizeof o);
    EXPECT_EQ(U(GetObjectW)(f, sizeof o, &o), sizeof(LOGFONTW));
    CHECK(o.lfHeight == 0 && o.lfWidth == 0 && o.lfWeight == 0 && o.lfFaceName[0] == 0);
    U(DeleteObject)(f);
}
TEST(gap2_cfi_distinct_handles) {
    NEED_CFI();
    LOGFONTW lf; lf_init(&lf, L"Arial", -16);
    HFONT a = cfi()(&lf), b = cfi()(&lf);
    CHECK(a && b && a != b);
    U(DeleteObject)(a); U(DeleteObject)(b);
}
TEST(gap2_cfi_height_roundtrip) {
    NEED_CFI();
    static const int hs[] = { -72, -48, -24, -12, -8, -1, 0, 1, 8, 12, 24, 100, 1000, -1000, 5000, -5000, 32767, -32768 };
    for (unsigned i = 0; i < sizeof hs / sizeof hs[0]; i++) {
        LOGFONTW lf; lf_init(&lf, L"Tahoma", hs[i]);
        HFONT f = cfi()(&lf);
        CHECKF(f != NULL, "height %d", hs[i]);
        if (!f) continue;
        LOGFONTW o; memset(&o, 0, sizeof o);
        U(GetObjectW)(f, sizeof o, &o);
        CHECKF(o.lfHeight == hs[i], "height %d came back %ld", hs[i], o.lfHeight);
        U(DeleteObject)(f);
    }
}
TEST(gap2_cfi_extreme_height_creates_object) {
    NEED_CFI();
    static const int hs[] = { INT_MAX, INT_MIN, 0x7fff0000, -0x7fff0000 };
    for (unsigned i = 0; i < sizeof hs / sizeof hs[0]; i++) {
        LOGFONTW lf; lf_init(&lf, L"Arial", hs[i]);
        HFONT f = cfi()(&lf);
        CHECKF(f != NULL, "height %d", hs[i]);
        if (f) {
            LOGFONTW o; U(GetObjectW)(f, sizeof o, &o);
            t_note("extreme height %d -> stored %ld", hs[i], o.lfHeight);
            U(DeleteObject)(f);
        }
    }
}
TEST(gap2_cfi_weight_roundtrip) {
    NEED_CFI();
    static const int ws[] = { 0, 1, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000 };
    for (unsigned i = 0; i < sizeof ws / sizeof ws[0]; i++) {
        LOGFONTW lf; lf_init(&lf, L"Arial", -16); lf.lfWeight = ws[i];
        HFONT f = cfi()(&lf);
        CHECKF(f != NULL, "weight %d", ws[i]);
        if (!f) continue;
        LOGFONTW o; U(GetObjectW)(f, sizeof o, &o);
        CHECKF(o.lfWeight == ws[i], "weight %d came back %ld", ws[i], o.lfWeight);
        U(DeleteObject)(f);
    }
}
TEST(gap2_cfi_flags_roundtrip) {
    NEED_CFI();
    for (int m = 0; m < 8; m++) {
        LOGFONTW lf; lf_init(&lf, L"Times New Roman", -20);
        lf.lfItalic = !!(m & 1); lf.lfUnderline = !!(m & 2); lf.lfStrikeOut = !!(m & 4);
        HFONT f = cfi()(&lf);
        CHECK(f != NULL); if (!f) continue;
        LOGFONTW o; U(GetObjectW)(f, sizeof o, &o);
        CHECKF(o.lfItalic == lf.lfItalic && o.lfUnderline == lf.lfUnderline && o.lfStrikeOut == lf.lfStrikeOut, "mask %d", m);
        U(DeleteObject)(f);
    }
}
TEST(gap2_cfi_flag_bytes_nonzero_kept) {
    NEED_CFI();
    LOGFONTW lf; lf_init(&lf, L"Arial", -16);
    lf.lfItalic = 0x7f; lf.lfUnderline = 2; lf.lfStrikeOut = 0xff;
    HFONT f = cfi()(&lf);
    CHECK(f != NULL); if (!f) return;
    LOGFONTW o; U(GetObjectW)(f, sizeof o, &o);
    t_note("italic/underline/strike bytes stored as %u/%u/%u", o.lfItalic, o.lfUnderline, o.lfStrikeOut);
    CHECK(o.lfItalic != 0 && o.lfUnderline != 0 && o.lfStrikeOut != 0);
    U(DeleteObject)(f);
}
TEST(gap2_cfi_width_escapement_orientation) {
    NEED_CFI();
    static const int es[] = { 0, 1, 450, 900, 1800, 2700, 3599, 3600, -900, 7200 };
    for (unsigned i = 0; i < sizeof es / sizeof es[0]; i++) {
        LOGFONTW lf; lf_init(&lf, L"Arial", -16);
        lf.lfEscapement = es[i]; lf.lfOrientation = es[i] / 2; lf.lfWidth = (LONG)i * 3;
        HFONT f = cfi()(&lf);
        CHECKF(f != NULL, "esc %d", es[i]); if (!f) continue;
        LOGFONTW o; U(GetObjectW)(f, sizeof o, &o);
        CHECKF(o.lfEscapement == es[i], "escapement %d came back %ld", es[i], o.lfEscapement);
        CHECKF(o.lfOrientation == es[i] / 2, "orientation %d came back %ld", es[i] / 2, o.lfOrientation);
        CHECKF(o.lfWidth == (LONG)i * 3, "width");
        U(DeleteObject)(f);
    }
}
TEST(gap2_cfi_charset_roundtrip) {
    NEED_CFI();
    static const BYTE cs[] = { ANSI_CHARSET, DEFAULT_CHARSET, SYMBOL_CHARSET, SHIFTJIS_CHARSET, HANGEUL_CHARSET, GB2312_CHARSET,
                               CHINESEBIG5_CHARSET, GREEK_CHARSET, TURKISH_CHARSET, HEBREW_CHARSET, ARABIC_CHARSET, BALTIC_CHARSET,
                               RUSSIAN_CHARSET, THAI_CHARSET, EASTEUROPE_CHARSET, OEM_CHARSET, 77, 0xfe, 0xff };
    for (unsigned i = 0; i < sizeof cs; i++) {
        LOGFONTW lf; lf_init(&lf, L"Arial", -16); lf.lfCharSet = cs[i];
        HFONT f = cfi()(&lf);
        CHECKF(f != NULL, "charset %u", cs[i]); if (!f) continue;
        LOGFONTW o; U(GetObjectW)(f, sizeof o, &o);
        CHECKF(o.lfCharSet == cs[i], "charset %u came back %u", cs[i], o.lfCharSet);
        U(DeleteObject)(f);
    }
}
TEST(gap2_cfi_precision_quality_pitch_observed) {
    NEED_CFI();
    LOGFONTW lf; lf_init(&lf, L"Arial", -16);
    lf.lfOutPrecision = OUT_TT_ONLY_PRECIS; lf.lfClipPrecision = CLIP_LH_ANGLES; lf.lfQuality = ANTIALIASED_QUALITY;
    lf.lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
    HFONT f = cfi()(&lf);
    CHECK(f != NULL); if (!f) return;
    LOGFONTW o; U(GetObjectW)(f, sizeof o, &o);
    t_note("outprec %u clipprec %u quality %u pitchfam %#x", o.lfOutPrecision, o.lfClipPrecision, o.lfQuality, o.lfPitchAndFamily);
    U(DeleteObject)(f);
}
TEST(gap2_cfi_facename_roundtrip) {
    NEED_CFI();
    static const WCHAR *const names[] = { L"Arial", L"Courier New", L"Times New Roman", L"Tahoma", L"Segoe UI", L"Consolas",
                                          L"Marlett", L"Symbol", L"System", L"Terminal", L"Fixedsys", L"MS Sans Serif",
                                          L"Small Fonts", L"", L"a", L"NoSuchFaceGap2" };
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) {
        LOGFONTW lf; lf_init(&lf, names[i], -14);
        HFONT f = cfi()(&lf);
        CHECKF(f != NULL, "face %u", i); if (!f) continue;
        LOGFONTW o; U(GetObjectW)(f, sizeof o, &o);
        CHECKF(!lstrcmpW(o.lfFaceName, names[i]), "face %u came back '%ls'", i, o.lfFaceName);
        U(DeleteObject)(f);
    }
}
TEST(gap2_cfi_facename_31_chars) {
    NEED_CFI();
    LOGFONTW lf; lf_init(&lf, L"", -14);
    for (int i = 0; i < 31; i++) lf.lfFaceName[i] = L'A' + (i % 26);
    HFONT f = cfi()(&lf);
    CHECK(f != NULL); if (!f) return;
    LOGFONTW o; U(GetObjectW)(f, sizeof o, &o);
    CHECK(!memcmp(o.lfFaceName, lf.lfFaceName, 32 * sizeof(WCHAR)));
    U(DeleteObject)(f);
}
TEST(gap2_cfi_facename_unterminated) {
    NEED_CFI();
    LOGFONTW lf; lf_init(&lf, L"", -14);
    for (int i = 0; i < LF_FACESIZE; i++) lf.lfFaceName[i] = L'B';
    HFONT f = cfi()(&lf);
    CHECK(f != NULL); if (!f) return;
    LOGFONTW o; memset(&o, 0, sizeof o);
    U(GetObjectW)(f, sizeof o, &o);
    t_note("unterminated face: last stored char %#x", o.lfFaceName[LF_FACESIZE - 1]);
    U(DeleteObject)(f);
}
TEST(gap2_cfi_garbage_logfont_fields) {
    NEED_CFI();
    LOGFONTW lf; memset(&lf, 0xFF, sizeof lf);
    lf.lfFaceName[0] = L'A'; lf.lfFaceName[1] = 0;
    HFONT f = cfi()(&lf);
    CHECK(f != NULL);
    if (f) { CHECK(U(DeleteObject)(f)); }
}
TEST(gap2_cfi_getobject_size_variants) {
    NEED_CFI();
    LOGFONTW lf; lf_init(&lf, L"Arial", -16);
    HFONT f = cfi()(&lf);
    CHECK(f != NULL); if (!f) return;
    LOGFONTW o;
    EXPECT_EQ(U(GetObjectW)(f, 0, NULL), sizeof(LOGFONTW));   /* size query */
    EXPECT_EQ(U(GetObjectW)(f, sizeof o, &o), sizeof(LOGFONTW));
    EXPECT_EQ(U(GetObjectW)(f, sizeof o + 100, &o), sizeof(LOGFONTW));
    int r = U(GetObjectW)(f, 20, &o);
    CHECK(r == 20 || r == 0);
    r = U(GetObjectW)(f, 1, &o);
    CHECK(r <= 1);
    U(DeleteObject)(f);
}
TEST(gap2_cfi_selected_into_dc) {
    NEED_CFI(); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(SelectObject)); REQUIRE(U(GetCurrentObject));
    HDC dc = U(CreateCompatibleDC)(NULL);
    LOGFONTW lf; lf_init(&lf, L"Arial", -20);
    HFONT f = cfi()(&lf);
    HGDIOBJ old = U(SelectObject)(dc, f);
    CHECK(old != NULL);
    EXPECT_EQ(U(GetCurrentObject)(dc, OBJ_FONT), f);
    CHECK(U(SelectObject)(dc, old) == f);
    CHECK(U(DeleteObject)(f));
    U(DeleteDC)(dc);
}
TEST(gap2_cfi_metrics_follow_height) {
    NEED_CFI(); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(SelectObject)); REQUIRE(U(GetTextMetricsW));
    HDC dc = U(CreateCompatibleDC)(NULL);
    static const int hs[] = { -10, -16, -20, -32, -48 };
    for (unsigned i = 0; i < 5; i++) {
        LOGFONTW lf; lf_init(&lf, L"Arial", hs[i]);
        HFONT f = cfi()(&lf);
        HGDIOBJ old = U(SelectObject)(dc, f);
        TEXTMETRICW tm; memset(&tm, 0, sizeof tm);
        CHECK(U(GetTextMetricsW)(dc, &tm));
        int em = tm.tmHeight - tm.tmInternalLeading;
        t_note("Arial %d: em height %d", hs[i], em);
        CHECKF(em >= -hs[i] - 1 && em <= -hs[i] + 1, "em %d for %d", em, hs[i]);
        U(SelectObject)(dc, old); U(DeleteObject)(f);
    }
    U(DeleteDC)(dc);
}
TEST(gap2_cfi_positive_height_is_cell_height) {
    NEED_CFI(); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(SelectObject)); REQUIRE(U(GetTextMetricsW));
    HDC dc = U(CreateCompatibleDC)(NULL);
    LOGFONTW lf; lf_init(&lf, L"Arial", 24);
    HFONT f = cfi()(&lf);
    HGDIOBJ old = U(SelectObject)(dc, f);
    TEXTMETRICW tm; memset(&tm, 0, sizeof tm);
    CHECK(U(GetTextMetricsW)(dc, &tm));
    CHECKF(tm.tmHeight >= 23 && tm.tmHeight <= 25, "cell height %ld", tm.tmHeight);
    U(SelectObject)(dc, old); U(DeleteObject)(f); U(DeleteDC)(dc);
}
TEST(gap2_cfi_unknown_face_is_substituted) {
    NEED_CFI(); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(SelectObject)); REQUIRE(U(GetTextFaceW));
    HDC dc = U(CreateCompatibleDC)(NULL);
    LOGFONTW lf; lf_init(&lf, L"NoSuchFaceGap2", -16);
    HFONT f = cfi()(&lf);
    HGDIOBJ old = U(SelectObject)(dc, f);
    WCHAR face[LF_FACESIZE + 8] = L"";
    int n = U(GetTextFaceW)(dc, LF_FACESIZE + 8, face);
    CHECK(n > 0);
    CHECK(lstrcmpiW(face, L"NoSuchFaceGap2") != 0);
    t_note("substituted with '%ls'", face);
    U(SelectObject)(dc, old); U(DeleteObject)(f); U(DeleteDC)(dc);
}
TEST(gap2_cfi_realized_face_matches_request) {
    NEED_CFI(); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(SelectObject)); REQUIRE(U(GetTextFaceW));
    HDC dc = U(CreateCompatibleDC)(NULL);
    static const WCHAR *const names[] = { L"Arial", L"Courier New", L"Times New Roman", L"Tahoma" };
    for (unsigned i = 0; i < 4; i++) {
        LOGFONTW lf; lf_init(&lf, names[i], -16);
        HFONT f = cfi()(&lf);
        HGDIOBJ old = U(SelectObject)(dc, f);
        WCHAR face[LF_FACESIZE] = L"";
        U(GetTextFaceW)(dc, LF_FACESIZE, face);
        if (lstrcmpiW(face, names[i])) t_note("requested %ls realised %ls", names[i], face);
        U(SelectObject)(dc, old); U(DeleteObject)(f);
    }
    U(DeleteDC)(dc);
}
TEST(gap2_cfi_gdi_count) {
    NEED_CFI();
    DWORD base = g2_gdi_count();
    LOGFONTW lf; lf_init(&lf, L"Arial", -16);
    HFONT f[100];
    for (int i = 0; i < 100; i++) { lf.lfHeight = -10 - i; f[i] = cfi()(&lf); CHECK(f[i] != NULL); }
    EXPECT_GDI(base + 100);
    for (int i = 0; i < 100; i++) CHECK(U(DeleteObject)(f[i]));
    EXPECT_GDI(base);
}
TEST(gap2_cfi_double_delete) {
    NEED_CFI();
    LOGFONTW lf; lf_init(&lf, L"Arial", -16);
    HFONT f = cfi()(&lf);
    CHECK(U(DeleteObject)(f));
    CHECK(!U(DeleteObject)(f));
    EXPECT_EQ(U(GetObjectType)(f), 0);
    SetLastError(0);
    EXPECT_EQ(U(GetObjectW)(f, sizeof lf, &lf), 0);
}
TEST(gap2_cfi_logfont_not_modified) {
    NEED_CFI();
    LOGFONTW lf; lf_init(&lf, L"Arial", -16);
    lf.lfEscapement = 123; lf.lfQuality = 5;
    LOGFONTW copy = lf;
    HFONT f = cfi()(&lf);
    CHECK(!memcmp(&lf, &copy, sizeof lf));
    if (f) U(DeleteObject)(f);
}
TEST(gap2_cfi_a_w_parity) {
    NEED_CFI(); REQUIRE(U(CreateFontIndirectA)); REQUIRE(U(GetObjectA));
    LOGFONTA la; memset(&la, 0, sizeof la);
    la.lfHeight = -18; la.lfWeight = 700; la.lfItalic = 1; la.lfCharSet = ANSI_CHARSET; strcpy(la.lfFaceName, "Arial");
    LOGFONTW lw; lf_init(&lw, L"Arial", -18); lw.lfWeight = 700; lw.lfItalic = 1; lw.lfCharSet = ANSI_CHARSET;
    HFONT fa = U(CreateFontIndirectA)(&la), fw = cfi()(&lw);
    CHECK(fa && fw);
    LOGFONTA oa; LOGFONTW ow; memset(&oa, 0, sizeof oa); memset(&ow, 0, sizeof ow);
    U(GetObjectA)(fa, sizeof oa, &oa); U(GetObjectW)(fw, sizeof ow, &ow);
    CHECK(oa.lfHeight == ow.lfHeight && oa.lfWeight == ow.lfWeight && oa.lfItalic == ow.lfItalic && oa.lfCharSet == ow.lfCharSet);
    char conv[LF_FACESIZE]; WideCharToMultiByte(CP_ACP, 0, ow.lfFaceName, -1, conv, sizeof conv, NULL, NULL);
    CHECK(!strcmp(conv, oa.lfFaceName));
    if (fa) U(DeleteObject)(fa);
    if (fw) U(DeleteObject)(fw);
}
TEST(gap2_cfi_same_code_as_public_export) {
    FARPROC impl = t_proc_gdi32full("CreateFontIndirectWImpl"), pub = t_proc_gdi32full("CreateFontIndirectW");
    REQUIRE(impl); REQUIRE(pub);
    CHECK(impl == pub);
}
TEST(gap2_cfi_ocr_a_charset) {
    NEED_CFI();
    /* only an app-compat shim rewrites the charset for OCR-A; unshimmed processes keep what they passed */
    LOGFONTW lf; lf_init(&lf, L"OCR-A", -16); lf.lfCharSet = DEFAULT_CHARSET;
    HFONT f = cfi()(&lf);
    CHECK(f != NULL); if (!f) return;
    LOGFONTW o; U(GetObjectW)(f, sizeof o, &o);
    CHECK(o.lfCharSet == DEFAULT_CHARSET || o.lfCharSet == SYMBOL_CHARSET);
    U(DeleteObject)(f);
}
static DWORD WINAPI cfi_thread(LPVOID p) {
    LOGFONTW lf; lf_init(&lf, L"Arial", -16);
    *(HFONT *)p = cfi()(&lf);
    return 0;
}
TEST(gap2_cfi_font_from_other_thread_usable) {
    NEED_CFI(); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(SelectObject));
    HFONT f = NULL;
    HANDLE t = CreateThread(NULL, 0, cfi_thread, &f, 0, NULL);
    WaitForSingleObject(t, 5000); CloseHandle(t);
    CHECK(f != NULL); if (!f) return;
    HDC dc = U(CreateCompatibleDC)(NULL);
    HGDIOBJ old = U(SelectObject)(dc, f);
    CHECK(old != NULL);
    U(SelectObject)(dc, old);
    CHECK(U(DeleteObject)(f));
    U(DeleteDC)(dc);
}

/* ------------------------------ CreateScalableFontResourceWImpl ------------------------------ */
#define NEED_CSF() do { REQUIRE(csf()); } while (0)
TEST(gap2_csf_null_arguments) {
    NEED_CSF();
    SKIP_ON_WINE("wine returns different error");
    WCHAR fot[] = L"x.fot", ttf[] = L"arial.ttf";
    SetLastError(0); CHECK(!csf()(0, NULL, ttf, NULL)); EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0); CHECK(!csf()(0, fot, NULL, NULL)); EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0); CHECK(!csf()(0, NULL, NULL, NULL)); EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0); CHECK(!csf()(1, NULL, NULL, L"C:\\")); EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
}
TEST(gap2_csf_null_args_all_hidden_flags) {
    NEED_CSF();
    SKIP_ON_WINE("wine returns different error");
    static const DWORD fl[] = { 0, 1, 2, 0xff, 0xffffffff };
    for (unsigned i = 0; i < sizeof fl / sizeof fl[0]; i++) {
        SetLastError(0);
        CHECKF(!csf()(fl[i], NULL, NULL, NULL), "flag %lu", fl[i]);
        EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    }
}
static int tmp_fot(WCHAR *w, int n, const char *tag) {
    char t[MAX_PATH], p[MAX_PATH];
    GetTempPathA(sizeof t, t);
    snprintf(p, sizeof p, "%sgap2_%s_%lu.fot", t, tag, GetCurrentProcessId());
    DeleteFileA(p);
    widen(p, w, n);
    return 1;
}
TEST(gap2_csf_create_fot) {
    NEED_CSF();
    char fd[MAX_PATH], p[MAX_PATH];
    if (!font_path("arial.ttf", p, sizeof p)) { t_skip("arial.ttf not present"); return; }
    fonts_dir(fd, sizeof fd);
    WCHAR fot[MAX_PATH], ttf[] = L"arial.ttf", dir[MAX_PATH];
    tmp_fot(fot, MAX_PATH, "c1"); widen(fd, dir, MAX_PATH);
    BOOL r = csf()(0, fot, ttf, dir);
    CHECK(r);
    if (r) {
        char fa[MAX_PATH]; WideCharToMultiByte(CP_ACP, 0, fot, -1, fa, sizeof fa, NULL, NULL);
        WIN32_FILE_ATTRIBUTE_DATA d;
        CHECK(GetFileAttributesExA(fa, GetFileExInfoStandard, &d));
        CHECK(d.nFileSizeLow > 0);
        DeleteFileA(fa);
    }
}
TEST(gap2_csf_dir_without_trailing_backslash) {
    NEED_CSF();
    char fd[MAX_PATH], p[MAX_PATH];
    if (!font_path("arial.ttf", p, sizeof p)) { t_skip("arial.ttf not present"); return; }
    fonts_dir(fd, sizeof fd); fd[strlen(fd) - 1] = 0;
    WCHAR fot[MAX_PATH], ttf[] = L"arial.ttf", dir[MAX_PATH];
    tmp_fot(fot, MAX_PATH, "c2"); widen(fd, dir, MAX_PATH);
    BOOL r = csf()(0, fot, ttf, dir);
    CHECK(r);
    char fa[MAX_PATH]; WideCharToMultiByte(CP_ACP, 0, fot, -1, fa, sizeof fa, NULL, NULL);
    DeleteFileA(fa);
}
TEST(gap2_csf_full_path_font_no_dir) {
    NEED_CSF();
    char p[MAX_PATH];
    if (!font_path("arial.ttf", p, sizeof p)) { t_skip("arial.ttf not present"); return; }
    WCHAR fot[MAX_PATH], ttf[MAX_PATH];
    tmp_fot(fot, MAX_PATH, "c3"); widen(p, ttf, MAX_PATH);
    BOOL r = csf()(0, fot, ttf, NULL);
    CHECK(r);
    BOOL r2;
    {
        char fa[MAX_PATH]; WideCharToMultiByte(CP_ACP, 0, fot, -1, fa, sizeof fa, NULL, NULL);
        DeleteFileA(fa);
        r2 = csf()(0, fot, ttf, L"");
        CHECK(r2);
        DeleteFileA(fa);
    }
}
TEST(gap2_csf_hidden_flag_variants) {
    NEED_CSF();
    char p[MAX_PATH];
    if (!font_path("cour.ttf", p, sizeof p)) { t_skip("cour.ttf not present"); return; }
    WCHAR ttf[MAX_PATH]; widen(p, ttf, MAX_PATH);
    static const DWORD fl[] = { 0, 1 };
    for (int i = 0; i < 2; i++) {
        WCHAR fot[MAX_PATH]; tmp_fot(fot, MAX_PATH, i ? "h1" : "h0");
        BOOL r = csf()(fl[i], fot, ttf, NULL);
        CHECKF(r, "hidden=%lu", fl[i]);
        char fa[MAX_PATH]; WideCharToMultiByte(CP_ACP, 0, fot, -1, fa, sizeof fa, NULL, NULL);
        DeleteFileA(fa);
    }
}
TEST(gap2_csf_existing_fot_refused) {
    NEED_CSF();
    char p[MAX_PATH];
    if (!font_path("cour.ttf", p, sizeof p)) { t_skip("cour.ttf not present"); return; }
    WCHAR ttf[MAX_PATH], fot[MAX_PATH]; widen(p, ttf, MAX_PATH); tmp_fot(fot, MAX_PATH, "ex");
    char fa[MAX_PATH]; WideCharToMultiByte(CP_ACP, 0, fot, -1, fa, sizeof fa, NULL, NULL);
    if (!csf()(0, fot, ttf, NULL)) { t_skip("first create failed"); return; }
    SetLastError(0);
    BOOL r = csf()(0, fot, ttf, NULL);
    CHECK(!r);
    t_note("create over existing .fot: err %lu", GetLastError());
    DeleteFileA(fa);
}
TEST(gap2_csf_missing_font_file) {
    NEED_CSF();
    WCHAR fot[MAX_PATH]; tmp_fot(fot, MAX_PATH, "mf");
    SetLastError(0);
    BOOL r = csf()(0, fot, L"gap2_no_such_font.ttf", L"C:\\Windows\\Fonts\\");
    CHECK(!r);
    t_note("missing font err %lu", GetLastError());
    char fa[MAX_PATH]; WideCharToMultiByte(CP_ACP, 0, fot, -1, fa, sizeof fa, NULL, NULL);
    CHECK(GetFileAttributesA(fa) == INVALID_FILE_ATTRIBUTES);
    DeleteFileA(fa);
}
TEST(gap2_csf_not_a_font) {
    NEED_CSF();
    char t[MAX_PATH], p[MAX_PATH];
    GetTempPathA(sizeof t, t);
    snprintf(p, sizeof p, "%sgap2_csfbad_%lu.ttf", t, GetCurrentProcessId());
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot write temp"); return; }
    DWORD w; WriteFile(h, "garbage garbage garbage", 23, &w, NULL); CloseHandle(h);
    WCHAR ttf[MAX_PATH], fot[MAX_PATH]; widen(p, ttf, MAX_PATH); tmp_fot(fot, MAX_PATH, "nf");
    BOOL r = csf()(0, fot, ttf, NULL);
    CHECK(!r);
    char fa[MAX_PATH]; WideCharToMultiByte(CP_ACP, 0, fot, -1, fa, sizeof fa, NULL, NULL);
    DeleteFileA(fa); DeleteFileA(p);
}
TEST(gap2_csf_matches_public_export) {
    NEED_CSF(); REQUIRE(U(CreateScalableFontResourceW));
    char p[MAX_PATH];
    if (!font_path("arial.ttf", p, sizeof p)) { t_skip("arial.ttf not present"); return; }
    WCHAR ttf[MAX_PATH], f1[MAX_PATH], f2[MAX_PATH];
    widen(p, ttf, MAX_PATH); tmp_fot(f1, MAX_PATH, "pa"); tmp_fot(f2, MAX_PATH, "pb");
    BOOL a = U(CreateScalableFontResourceW)(0, f1, ttf, NULL), b = csf()(0, f2, ttf, NULL);
    CHECK(a == b);
    char fa[MAX_PATH];
    WideCharToMultiByte(CP_ACP, 0, f1, -1, fa, sizeof fa, NULL, NULL); DeleteFileA(fa);
    WideCharToMultiByte(CP_ACP, 0, f2, -1, fa, sizeof fa, NULL, NULL); DeleteFileA(fa);
    SetLastError(0); a = U(CreateScalableFontResourceW)(0, NULL, NULL, NULL); DWORD ea = GetLastError();
    SetLastError(0); b = csf()(0, NULL, NULL, NULL); DWORD eb = GetLastError();
    CHECK(!a && !b);
    CHECK(ea == eb);
}
TEST(gap2_csf_empty_names) {
    NEED_CSF();
    WCHAR fot[MAX_PATH]; tmp_fot(fot, MAX_PATH, "en");
    SetLastError(0);
    CHECK(!csf()(0, L"", L"", NULL));
    SetLastError(0);
    CHECK(!csf()(0, fot, L"", NULL));
    SetLastError(0);
    CHECK(!csf()(0, L"", L"arial.ttf", NULL));
}
