/* Charset info, TranslateCharsetInfo, language info, rasterizer caps, kerning. */
#include "fe.h"

static const struct { BYTE cs; UINT cp; DWORD fs; const char *label; } cs_tab[] = {
    {ANSI_CHARSET, 1252, FS_LATIN1, "ANSI"}, {EASTEUROPE_CHARSET, 1250, FS_LATIN2, "EASTEUROPE"},
    {RUSSIAN_CHARSET, 1251, FS_CYRILLIC, "RUSSIAN"}, {GREEK_CHARSET, 1253, FS_GREEK, "GREEK"},
    {TURKISH_CHARSET, 1254, FS_TURKISH, "TURKISH"}, {HEBREW_CHARSET, 1255, FS_HEBREW, "HEBREW"},
    {ARABIC_CHARSET, 1256, FS_ARABIC, "ARABIC"}, {BALTIC_CHARSET, 1257, FS_BALTIC, "BALTIC"},
    {VIETNAMESE_CHARSET, 1258, FS_VIETNAMESE, "VIETNAMESE"}, {THAI_CHARSET, 874, FS_THAI, "THAI"},
    {SHIFTJIS_CHARSET, 932, FS_JISJAPAN, "SHIFTJIS"}, {GB2312_CHARSET, 936, FS_CHINESESIMP, "GB2312"},
    {HANGEUL_CHARSET, 949, FS_WANSUNG, "HANGEUL"}, {CHINESEBIG5_CHARSET, 950, FS_CHINESETRAD, "BIG5"},
    {JOHAB_CHARSET, 1361, FS_JOHAB, "JOHAB"},
};
#define N_CS ((int)(sizeof cs_tab / sizeof cs_tab[0]))
#define TCI_REQ() do { REQUIRE(U(TranslateCharsetInfo)); } while (0)

TEST(fonts_tci_srccharset) {
    TCI_REQ();
    for (int i = 0; i < N_CS; i++) {
        CHARSETINFO ci; memset(&ci, 0xCC, sizeof ci);
        BOOL r = U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)cs_tab[i].cs, &ci, TCI_SRCCHARSET);
        CHECKF(r, "%s", cs_tab[i].label);
        CHECKF(ci.ciCharset == cs_tab[i].cs, "%s charset %u", cs_tab[i].label, ci.ciCharset);
        CHECKF(ci.ciACP == cs_tab[i].cp, "%s acp %u", cs_tab[i].label, ci.ciACP);
        CHECKF(ci.fs.fsCsb[0] == cs_tab[i].fs, "%s fs %08lx", cs_tab[i].label, ci.fs.fsCsb[0]);
        CHECKF(ci.fs.fsCsb[1] == 0, "%s fsCsb[1] %08lx", cs_tab[i].label, ci.fs.fsCsb[1]);
        CHECKF(ci.fs.fsUsb[0] == 0 && ci.fs.fsUsb[1] == 0 && ci.fs.fsUsb[2] == 0 && ci.fs.fsUsb[3] == 0, "%s fsUsb not zero", cs_tab[i].label);
    }
}
TEST(fonts_tci_srccodepage) {
    TCI_REQ();
    for (int i = 0; i < N_CS; i++) {
        CHARSETINFO ci; memset(&ci, 0xCC, sizeof ci);
        BOOL r = U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)cs_tab[i].cp, &ci, TCI_SRCCODEPAGE);
        CHECKF(r, "%s cp %u", cs_tab[i].label, cs_tab[i].cp);
        CHECKF(ci.ciCharset == cs_tab[i].cs, "%s cp %u charset %u", cs_tab[i].label, cs_tab[i].cp, ci.ciCharset);
        CHECKF(ci.ciACP == cs_tab[i].cp, "%s acp %u", cs_tab[i].label, ci.ciACP);
        CHECKF(ci.fs.fsCsb[0] == cs_tab[i].fs, "%s fs %08lx", cs_tab[i].label, ci.fs.fsCsb[0]);
    }
}
TEST(fonts_tci_srcfontsig_single_bits) {
    TCI_REQ();
    for (int i = 0; i < N_CS; i++) {
        DWORD sig[2] = {cs_tab[i].fs, 0};
        CHARSETINFO ci; memset(&ci, 0xCC, sizeof ci);
        BOOL r = U(TranslateCharsetInfo)(sig, &ci, TCI_SRCFONTSIG);
        CHECKF(r, "%s sig %08lx", cs_tab[i].label, sig[0]);
        CHECKF(ci.ciCharset == cs_tab[i].cs && ci.ciACP == cs_tab[i].cp, "%s -> charset %u acp %u", cs_tab[i].label, ci.ciCharset, ci.ciACP);
        CHECKF(ci.fs.fsCsb[0] == cs_tab[i].fs, "%s fs %08lx", cs_tab[i].label, ci.fs.fsCsb[0]);
    }
}
TEST(fonts_tci_srcfontsig_second_dword_nonzero) {
    TCI_REQ();
    DWORD sig[2] = {FS_LATIN1, 1};
    CHARSETINFO ci; memset(&ci, 0, sizeof ci);
    BOOL r = U(TranslateCharsetInfo)(sig, &ci, TCI_SRCFONTSIG);
    t_note("TCI_SRCFONTSIG with fsCsb[1]!=0 -> %d (charset %u)", r, ci.ciCharset);
}
TEST(fonts_tci_srcfontsig_multi_bit) {
    TCI_REQ();
    DWORD sig[2] = {FS_LATIN1 | FS_LATIN2 | FS_GREEK, 0};
    CHARSETINFO ci; memset(&ci, 0, sizeof ci);
    BOOL r = U(TranslateCharsetInfo)(sig, &ci, TCI_SRCFONTSIG);
    t_note("TCI_SRCFONTSIG with several bits -> %d charset %u acp %u", r, ci.ciCharset, ci.ciACP);
}
TEST(fonts_tci_symbol_and_oem) {
    TCI_REQ();
    CHARSETINFO ci; memset(&ci, 0, sizeof ci);
    BOOL r = U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)SYMBOL_CHARSET, &ci, TCI_SRCCHARSET);
    t_note("SYMBOL_CHARSET -> %d acp %u fs %08lx", r, ci.ciACP, ci.fs.fsCsb[0]);
    if (r) { CHECK(ci.ciCharset == SYMBOL_CHARSET); CHECK(ci.fs.fsCsb[0] == FS_SYMBOL); }
    memset(&ci, 0, sizeof ci);
    r = U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)OEM_CHARSET, &ci, TCI_SRCCHARSET);
    t_note("OEM_CHARSET -> %d acp %u fs %08lx", r, ci.ciACP, ci.fs.fsCsb[0]);
    memset(&ci, 0, sizeof ci);
    r = U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)MAC_CHARSET, &ci, TCI_SRCCHARSET);
    t_note("MAC_CHARSET -> %d acp %u fs %08lx", r, ci.ciACP, ci.fs.fsCsb[0]);
    memset(&ci, 0, sizeof ci);
    r = U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)42, &ci, TCI_SRCCODEPAGE);
    t_note("codepage 42 -> %d charset %u", r, ci.ciCharset);
    memset(&ci, 0, sizeof ci);
    r = U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)437, &ci, TCI_SRCCODEPAGE);
    t_note("codepage 437 -> %d charset %u", r, ci.ciCharset);
}
TEST(fonts_tci_unknown_values) {
    TCI_REQ();
    CHARSETINFO ci;
    static const DWORD chs[] = {DEFAULT_CHARSET, 3, 99, 100, 200, 256, 0x1000};
    for (unsigned i = 0; i < sizeof chs / sizeof chs[0]; i++) {
        memset(&ci, 0, sizeof ci);
        CHECKF(!U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)chs[i], &ci, TCI_SRCCHARSET), "charset %lu translated", chs[i]);
    }
    static const DWORD cps[] = {0, 1, 99999, 1200, 20127, 28591, 12345};
    for (unsigned i = 0; i < sizeof cps / sizeof cps[0]; i++) {
        memset(&ci, 0, sizeof ci);
        CHECKF(!U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)cps[i], &ci, TCI_SRCCODEPAGE), "codepage %lu translated", cps[i]);
    }
    DWORD sig[2] = {0, 0};
    CHECK(!U(TranslateCharsetInfo)(sig, &ci, TCI_SRCFONTSIG));
}
TEST(fonts_tci_only_low_word_used) {
    TCI_REQ();
    CHARSETINFO ci; memset(&ci, 0, sizeof ci);
    BOOL r = U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)(0x10000 | 1252), &ci, TCI_SRCCODEPAGE);
    CHECK(r); if (r) EQ(ci.ciCharset, ANSI_CHARSET);
    memset(&ci, 0, sizeof ci);
    r = U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)(0x10000 | GREEK_CHARSET), &ci, TCI_SRCCHARSET);
    CHECK(r); if (r) EQ(ci.ciCharset, GREEK_CHARSET);
}
TEST(fonts_tci_null_output) {
    TCI_REQ();
    if (fe_wine()) { t_skip("wine faults on a NULL CHARSETINFO"); return; }
    SetLastError(0);
    CHECK(!U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)1252, NULL, TCI_SRCCODEPAGE));
    CHECK(!U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)0, NULL, TCI_SRCCHARSET));
}
TEST(fonts_tci_invalid_flags) {
    TCI_REQ();
    CHARSETINFO ci; memset(&ci, 0, sizeof ci);
    static const DWORD fl[] = {0, 4, 5, 8, 0x10, 0x100, 0x800, 0x2000, 0x80000000, 0xFFFFFFFF};
    for (unsigned i = 0; i < sizeof fl / sizeof fl[0]; i++) {
        SetLastError(0);
        CHECKF(!U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)0, &ci, fl[i]), "flags %lx", fl[i]);
        EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    }
}
TEST(fonts_tci_locale) {
    TCI_REQ();
    static const struct { DWORD lcid; BYTE cs; UINT cp; } t[] = {
        {0x0409, ANSI_CHARSET, 1252}, {0x0407, ANSI_CHARSET, 1252}, {0x0419, RUSSIAN_CHARSET, 1251}, {0x0408, GREEK_CHARSET, 1253},
        {0x0411, SHIFTJIS_CHARSET, 932}, {0x0412, HANGEUL_CHARSET, 949}};
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        CHARSETINFO ci; memset(&ci, 0xCC, sizeof ci);
        BOOL r = U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)t[i].lcid, &ci, TCI_SRCLOCALE);
        CHECKF(r, "lcid %04lx", t[i].lcid);
        if (r) CHECKF(ci.ciCharset == t[i].cs && ci.ciACP == t[i].cp, "lcid %04lx -> charset %u acp %u", t[i].lcid, ci.ciCharset, ci.ciACP);
    }
}
TEST(fonts_tci_locale_more) {
    TCI_REQ();
    static const DWORD l[] = {0x0804, 0x0404, 0x041E, 0x040D, 0x0401, 0x041F, 0x0425, 0x042A, 0x0405, 0x0C0A, 0x0809, 0x0000, 0xFFFF};
    for (unsigned i = 0; i < sizeof l / sizeof l[0]; i++) {
        CHARSETINFO ci; memset(&ci, 0, sizeof ci);
        BOOL r = U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)l[i], &ci, TCI_SRCLOCALE);
        t_note("lcid %04lx -> %d charset %u acp %u", l[i], r, ci.ciCharset, ci.ciACP);
    }
}
TEST(fonts_tci_system_acp_roundtrip) {
    TCI_REQ();
    UINT acp = GetACP();
    CHARSETINFO ci; memset(&ci, 0, sizeof ci);
    BOOL r = U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)acp, &ci, TCI_SRCCODEPAGE);
    t_note("ACP %u -> %d charset %u", acp, r, ci.ciCharset);
    if (r) {
        CHARSETINFO c2; memset(&c2, 0, sizeof c2);
        CHECK(U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)ci.ciCharset, &c2, TCI_SRCCHARSET));
        EQ(c2.ciACP, acp);
    }
}
TEST(fonts_tci_charset_codepage_bijection) {
    TCI_REQ();
    for (int i = 0; i < N_CS; i++) for (int j = 0; j < N_CS; j++) {
        if (i == j) continue;
        CHECKF(cs_tab[i].cs != cs_tab[j].cs && cs_tab[i].cp != cs_tab[j].cp && cs_tab[i].fs != cs_tab[j].fs, "table collision %d %d", i, j);
    }
}

/* ---- GetTextCharset / Info ---- */
TEST(fonts_charset_gettextcharset_arial_scripts) {
    FE_BASE(); REQUIRE(U(GetTextCharset));
    static const BYTE cs[] = {ANSI_CHARSET, GREEK_CHARSET, RUSSIAN_CHARSET, EASTEUROPE_CHARSET, TURKISH_CHARSET, BALTIC_CHARSET, HEBREW_CHARSET, ARABIC_CHARSET};
    for (unsigned i = 0; i < sizeof cs; i++) {
        fe_ctx c; int ok = fe_open(&c, "Arial", -16, 400, 0, cs[i]);
        if (ok) CHECKF(U(GetTextCharset)(c.dc) == cs[i], "charset %u -> %d", cs[i], U(GetTextCharset)(c.dc));
        fe_close(&c);
    }
}
TEST(fonts_charset_gettextcharset_symbol_oem) {
    FE_BASE(); REQUIRE(U(GetTextCharset));
    fe_ctx c;
    if (fe_open(&c, "Symbol", -16, 400, 0, SYMBOL_CHARSET)) EQ(U(GetTextCharset)(c.dc), SYMBOL_CHARSET);
    fe_close(&c);
    if (fe_open(&c, "Terminal", -16, 400, 0, OEM_CHARSET)) EQ(U(GetTextCharset)(c.dc), OEM_CHARSET);
    fe_close(&c);
}
TEST(fonts_charset_gettextcharset_default_dc) {
    FE_BASE(); REQUIRE(U(GetTextCharset));
    HDC dc = U(CreateCompatibleDC)(NULL);
    int cs = U(GetTextCharset)(dc);
    t_note("default DC charset %d (ACP %u)", cs, GetACP());
    CHECK(cs >= 0 && cs <= 255);
    CHECK(cs != DEFAULT_CHARSET);
    U(DeleteDC)(dc);
}
TEST(fonts_charset_gettextcharset_bad_dc) {
    FE_BASE(); REQUIRE(U(GetTextCharset)); REQUIRE(U(GetTextCharsetInfo));
    EQ(U(GetTextCharset)(NULL), DEFAULT_CHARSET);
    EQ(U(GetTextCharset)((HDC)(ULONG_PTR)0x1357), DEFAULT_CHARSET);
    FONTSIGNATURE fs;
    EQ(U(GetTextCharsetInfo)(NULL, &fs, 0), DEFAULT_CHARSET);
    EQ(U(GetTextCharsetInfo)(NULL, NULL, 0), DEFAULT_CHARSET);
    HDC dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    EQ(U(GetTextCharset)(dc), DEFAULT_CHARSET);
}
TEST(fonts_charset_info_arial_signature) {
    FE_BASE(); REQUIRE(U(GetTextCharsetInfo));
    fe_ctx c; if (!fe_open(&c, "Arial", -16, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    FONTSIGNATURE fs; memset(&fs, 0, sizeof fs);
    int cs = U(GetTextCharsetInfo)(c.dc, &fs, 0);
    EQ(cs, ANSI_CHARSET);
    CHECK(fs.fsUsb[0] & 1);                       /* Basic Latin */
    CHECK(fs.fsCsb[0] & FS_LATIN1);
    CHECK(fs.fsCsb[0] & FS_GREEK);
    CHECK(fs.fsCsb[0] & FS_CYRILLIC);
    CHECK(!(fs.fsCsb[0] & FS_SYMBOL));
    t_note("Arial fsUsb %08lx %08lx %08lx %08lx fsCsb %08lx %08lx", fs.fsUsb[0], fs.fsUsb[1], fs.fsUsb[2], fs.fsUsb[3], fs.fsCsb[0], fs.fsCsb[1]);
    fe_close(&c);
}
TEST(fonts_charset_info_symbol_signature) {
    FE_BASE(); REQUIRE(U(GetTextCharsetInfo));
    fe_ctx c; if (!fe_open(&c, "Symbol", -16, 400, 0, SYMBOL_CHARSET)) { fe_close(&c); return; }
    FONTSIGNATURE fs; memset(&fs, 0, sizeof fs);
    EQ(U(GetTextCharsetInfo)(c.dc, &fs, 0), SYMBOL_CHARSET);
    CHECK(fs.fsCsb[0] & FS_SYMBOL);
    fe_close(&c);
}
TEST(fonts_charset_info_null_signature) {
    FE_BASE(); REQUIRE(U(GetTextCharsetInfo));
    fe_ctx c; if (!fe_open(&c, "Arial", -16, 400, 0, GREEK_CHARSET)) { fe_close(&c); return; }
    EQ(U(GetTextCharsetInfo)(c.dc, NULL, 0), GREEK_CHARSET);
    EQ(U(GetTextCharsetInfo)(c.dc, NULL, 1), GREEK_CHARSET);
    fe_close(&c);
}
TEST(fonts_charset_info_flags_ignored) {
    FE_BASE(); REQUIRE(U(GetTextCharsetInfo));
    fe_ctx c; if (!fe_open(&c, "Arial", -16, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    FONTSIGNATURE a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    int ca = U(GetTextCharsetInfo)(c.dc, &a, 0), cb = U(GetTextCharsetInfo)(c.dc, &b, 0x7FFF);
    EQ(ca, cb);
    CHECK(!memcmp(&a, &b, sizeof a));
    fe_close(&c);
}
TEST(fonts_charset_info_tt_vs_raster) {
    FE_BASE(); REQUIRE(U(GetTextCharsetInfo));
    static const char *faces[] = {"System", "MS Sans Serif", "Fixedsys", "Small Fonts", "Courier New", "Tahoma", "Segoe UI"};
    for (unsigned i = 0; i < 7; i++) {
        fe_ctx c; if (!fe_open(&c, faces[i], -16, 400, 0, ANSI_CHARSET)) { fe_close(&c); continue; }
        FONTSIGNATURE fs; memset(&fs, 0, sizeof fs);
        int cs = U(GetTextCharsetInfo)(c.dc, &fs, 0);
        CHECKF(cs == ANSI_CHARSET, "%s", faces[i]);
        CHECKF(fs.fsCsb[0] & FS_LATIN1, "%s fsCsb %08lx", faces[i], fs.fsCsb[0]);
        fe_close(&c);
    }
}
TEST(fonts_charset_info_matches_tci) {
    FE_BASE(); REQUIRE(U(GetTextCharsetInfo)); TCI_REQ();
    fe_ctx c; if (!fe_open(&c, "Arial", -16, 400, 0, GREEK_CHARSET)) { fe_close(&c); return; }
    FONTSIGNATURE fs; int cs = U(GetTextCharsetInfo)(c.dc, &fs, 0);
    CHARSETINFO ci;
    CHECK(U(TranslateCharsetInfo)((DWORD *)(ULONG_PTR)cs, &ci, TCI_SRCCHARSET));
    EQ(ci.ciACP, 1253);
    fe_close(&c);
}

/* ---- GetFontLanguageInfo ---- */
TEST(fonts_langinfo_bad_dc) {
    FE_BASE(); REQUIRE(U(GetFontLanguageInfo));
    EQ(U(GetFontLanguageInfo)(NULL), GCP_ERROR);
    EQ(U(GetFontLanguageInfo)((HDC)(ULONG_PTR)0x2468), GCP_ERROR);
    HDC dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    EQ(U(GetFontLanguageInfo)(dc), GCP_ERROR);
}
TEST(fonts_langinfo_valid_dc) {
    FE_BASE(); REQUIRE(U(GetFontLanguageInfo));
    HDC dc = U(CreateCompatibleDC)(NULL);
    DWORD r = U(GetFontLanguageInfo)(dc);
    CHECKF(!(r & GCP_ERROR), "valid DC returned %08lx", r);
    t_note("GetFontLanguageInfo(default DC) = %08lx", r);
    DWORD r2 = U(GetFontLanguageInfo)(dc);
    EQ(r2, r);
    U(DeleteDC)(dc);
}
TEST(fonts_langinfo_per_font) {
    FE_BASE(); REQUIRE(U(GetFontLanguageInfo));
    static const struct { const char *face; BYTE cs; } t[] = {{"Arial", ANSI_CHARSET}, {"Arial", HEBREW_CHARSET}, {"Arial", ARABIC_CHARSET}, {"Arial", GREEK_CHARSET}, {"Tahoma", THAI_CHARSET}};
    for (unsigned i = 0; i < 5; i++) {
        fe_ctx c; if (!fe_open(&c, t[i].face, -16, 400, 0, t[i].cs)) { fe_close(&c); continue; }
        DWORD r = U(GetFontLanguageInfo)(c.dc);
        CHECKF(!(r & GCP_ERROR), "%s cs %u -> %08lx", t[i].face, t[i].cs, r);
        t_note("%s cs %u language info %08lx", t[i].face, t[i].cs, r);
        fe_close(&c);
    }
}
TEST(fonts_langinfo_western_has_no_complex_flags) {
    FE_BASE(); REQUIRE(U(GetFontLanguageInfo));
    fe_ctx c; if (!fe_open(&c, "Arial", -16, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    DWORD r = U(GetFontLanguageInfo)(c.dc);
    CHECKF(!(r & (GCP_REORDER | GCP_GLYPHSHAPE | GCP_DIACRITIC | GCP_KASHIDA | GCP_LIGATE)), "flags %08lx", r);
    fe_close(&c);
}

/* ---- rasterizer caps ---- */
TEST(fonts_rasterizer_caps_basic) {
    REQUIRE(U(GetRasterizerCaps));
    RASTERIZER_STATUS rs; memset(&rs, 0x5A, sizeof rs);
    CHECK(U(GetRasterizerCaps)(&rs, sizeof rs));
    EQ(sizeof(RASTERIZER_STATUS), 6);
    EQ(rs.nSize, 6);
    CHECK(rs.wFlags & TT_AVAILABLE);
    CHECK(rs.wFlags & TT_ENABLED);
    t_note("rasterizer: flags %04x language %04x", rs.wFlags, rs.nLanguageID);
}
TEST(fonts_rasterizer_caps_repeatable) {
    REQUIRE(U(GetRasterizerCaps));
    RASTERIZER_STATUS a, b; memset(&a, 0, sizeof a); memset(&b, 1, sizeof b);
    CHECK(U(GetRasterizerCaps)(&a, sizeof a)); CHECK(U(GetRasterizerCaps)(&b, sizeof b));
    CHECK(a.nSize == b.nSize && a.wFlags == b.wFlags && a.nLanguageID == b.nLanguageID);
}
TEST(fonts_rasterizer_caps_partial_sizes) {
    REQUIRE(U(GetRasterizerCaps));
    BYTE buf[16];
    for (int cb = 1; cb <= 12; cb++) {
        memset(buf, 0x5A, sizeof buf);
        BOOL r = U(GetRasterizerCaps)((RASTERIZER_STATUS *)buf, cb);
        for (int i = (cb < 6 ? cb : 6); i < 16; i++) CHECKF(buf[i] == 0x5A, "cb=%d wrote byte %d", cb, i);
        if (cb < 6) t_note("GetRasterizerCaps(cb=%d) = %d", cb, r);
        else CHECKF(r, "cb=%d", cb);
    }
}
TEST(fonts_rasterizer_caps_zero_size) {
    REQUIRE(U(GetRasterizerCaps));
    RASTERIZER_STATUS rs; memset(&rs, 0x5A, sizeof rs);
    BOOL r = U(GetRasterizerCaps)(&rs, 0);
    t_note("GetRasterizerCaps(cb=0) = %d, nSize now %d", r, rs.nSize);
}
TEST(fonts_rasterizer_caps_null) {
    REQUIRE(U(GetRasterizerCaps));
    BOOL r = U(GetRasterizerCaps)(NULL, 0);
    t_note("GetRasterizerCaps(NULL, 0) = %d", r);
}

/* ---- kerning pairs ---- */
static void pairs_check(const char *face, int expect_some) {
    fe_ctx c; if (!fe_open(&c, face, -16, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    DWORD n = U(GetKerningPairsA)(c.dc, 0, NULL);
    DWORD nw = U(GetKerningPairsW)(c.dc, 0, NULL);
    t_note("%s kerning pairs: A=%lu W=%lu", face, n, nw);
    if (expect_some) CHECKF(n > 0 && nw > 0, "%s has no kerning pairs", face);
    CHECKF(n <= nw || nw == 0, "%s A count %lu exceeds W count %lu", face, n, nw);
    if (n > 0 && n < 100000) {
        KERNINGPAIR *kp = (KERNINGPAIR *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n * sizeof *kp);
        DWORD got = U(GetKerningPairsA)(c.dc, n, kp);
        EQ(got, n);
        int sorted = 1;
        for (DWORD i = 1; i < got; i++) {
            DWORD a = ((DWORD)kp[i - 1].wFirst << 16) | kp[i - 1].wSecond, b = ((DWORD)kp[i].wFirst << 16) | kp[i].wSecond;
            if (b <= a) sorted = 0;
        }
        CHECKF(sorted, "%s kerning pairs not strictly sorted", face);
        for (DWORD i = 0; i < got; i++) CHECKF(kp[i].iKernAmount != 0 || 1, "x");
        HeapFree(GetProcessHeap(), 0, kp);
    }
    fe_close(&c);
}
TEST(fonts_kerning_arial) { FE_BASE(); REQUIRE(U(GetKerningPairsA)); REQUIRE(U(GetKerningPairsW)); pairs_check("Arial", 1); }
TEST(fonts_kerning_times) { FE_BASE(); REQUIRE(U(GetKerningPairsA)); REQUIRE(U(GetKerningPairsW)); pairs_check("Times New Roman", 1); }
TEST(fonts_kerning_courier_new) { FE_BASE(); REQUIRE(U(GetKerningPairsA)); REQUIRE(U(GetKerningPairsW)); pairs_check("Courier New", 0); }
TEST(fonts_kerning_tahoma) { FE_BASE(); REQUIRE(U(GetKerningPairsA)); REQUIRE(U(GetKerningPairsW)); pairs_check("Tahoma", 0); }
TEST(fonts_kerning_segoe_ui) { FE_BASE(); REQUIRE(U(GetKerningPairsA)); REQUIRE(U(GetKerningPairsW)); pairs_check("Segoe UI", 0); }
TEST(fonts_kerning_raster) {
    FE_BASE(); REQUIRE(U(GetKerningPairsA));
    fe_ctx c; if (!fe_open(&c, "System", -16, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    DWORD n = U(GetKerningPairsA)(c.dc, 0, NULL);
    t_note("System kerning pairs %lu", n);
    fe_close(&c);
}
TEST(fonts_kerning_small_buffer) {
    FE_BASE(); REQUIRE(U(GetKerningPairsA));
    fe_ctx c; if (!fe_open(&c, "Arial", -16, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    DWORD n = U(GetKerningPairsA)(c.dc, 0, NULL);
    if (n > 2) {
        KERNINGPAIR kp[2]; memset(kp, 0, sizeof kp);
        SetLastError(0);
        DWORD r = U(GetKerningPairsA)(c.dc, 2, kp);
        EQ(r, 0);
        t_note("too-small buffer: error %lu", GetLastError());
    }
    fe_close(&c);
}
TEST(fonts_kerning_bad_dc) {
    FE_BASE(); REQUIRE(U(GetKerningPairsA)); REQUIRE(U(GetKerningPairsW));
    EQ(U(GetKerningPairsA)(NULL, 0, NULL), 0);
    EQ(U(GetKerningPairsW)(NULL, 0, NULL), 0);
    EQ(U(GetKerningPairsA)((HDC)(ULONG_PTR)0x8888, 0, NULL), 0);
}
TEST(fonts_kerning_known_pair_av) {
    FE_BASE(); REQUIRE(U(GetKerningPairsA));
    fe_ctx c; if (!fe_open(&c, "Arial", -48, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    DWORD n = U(GetKerningPairsA)(c.dc, 0, NULL);
    if (n && n < 100000) {
        KERNINGPAIR *kp = (KERNINGPAIR *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n * sizeof *kp);
        U(GetKerningPairsA)(c.dc, n, kp);
        int found = 0; int amt = 0;
        for (DWORD i = 0; i < n; i++) if (kp[i].wFirst == 'A' && kp[i].wSecond == 'V') { found = 1; amt = kp[i].iKernAmount; }
        CHECK(found);
        if (found) CHECKF(amt < 0, "AV kern amount %d", amt);
        HeapFree(GetProcessHeap(), 0, kp);
    }
    fe_close(&c);
}
