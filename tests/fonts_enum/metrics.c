/* TEXTMETRIC / OUTLINETEXTMETRIC / GetFontData / extents per face. */
#include "fe.h"

static const fe_face_t *face_info(const char *name) {
    for (int i = 0; i < FE_NFACES; i++) if (!lstrcmpA(fe_faces[i].name, name)) return &fe_faces[i];
    return NULL;
}

/* ---- GetTextMetricsA invariants ---- */
static void w_tm_basic(const char *face) {
    static const int hs[] = {-8, -10, -12, -16, -20, -24, -32, 0};
    const fe_face_t *fi = face_info(face);
    for (unsigned i = 0; i < sizeof hs / sizeof hs[0]; i++) {
        fe_ctx c;
        if (!fe_open(&c, face, hs[i], 400, 0, fe_req_cs(face))) { t_note("%s unavailable at %d", face, hs[i]); fe_close(&c); return; }
        TEXTMETRICA tm; memset(&tm, 0xCC, sizeof tm);
        CHECK(U(GetTextMetricsA)(c.dc, &tm));
        CHECKF(tm.tmHeight > 0, "%s h=%d", face, hs[i]);
        CHECKF(tm.tmAscent + tm.tmDescent == tm.tmHeight, "%s h=%d asc %ld desc %ld height %ld", face, hs[i], tm.tmAscent, tm.tmDescent, tm.tmHeight);
        CHECKF(tm.tmAscent > 0 && tm.tmDescent >= 0, "%s h=%d", face, hs[i]);
        CHECKF(tm.tmInternalLeading >= 0 && tm.tmInternalLeading < tm.tmHeight, "%s h=%d internal %ld", face, hs[i], tm.tmInternalLeading);
        CHECKF(tm.tmExternalLeading >= 0, "%s h=%d", face, hs[i]);
        CHECKF(tm.tmAveCharWidth > 0 && tm.tmMaxCharWidth >= tm.tmAveCharWidth, "%s h=%d ave %ld max %ld", face, hs[i], tm.tmAveCharWidth, tm.tmMaxCharWidth);
        CHECKF(tm.tmWeight >= 100 && tm.tmWeight <= 1000, "%s weight %ld", face, tm.tmWeight);
        CHECKF(tm.tmOverhang >= 0, "%s overhang %ld", face, tm.tmOverhang);
        CHECKF(tm.tmDigitizedAspectX > 0 && tm.tmDigitizedAspectY > 0, "%s aspect", face);
        CHECKF(tm.tmFirstChar <= tm.tmLastChar, "%s first %u last %u", face, tm.tmFirstChar, tm.tmLastChar);
        CHECKF(tm.tmItalic == 0 && tm.tmUnderlined == 0 && tm.tmStruckOut == 0, "%s style flags", face);
        if (fi) {
            if (fi->tt) CHECKF(tm.tmPitchAndFamily & TMPF_TRUETYPE, "%s TMPF_TRUETYPE missing (%02x)", face, tm.tmPitchAndFamily);
            else CHECKF(!(tm.tmPitchAndFamily & (TMPF_TRUETYPE | TMPF_VECTOR)), "%s raster has TT/vector bit (%02x)", face, tm.tmPitchAndFamily);
            if (fi->fixed == 1) CHECKF(!(tm.tmPitchAndFamily & TMPF_FIXED_PITCH), "%s should be fixed pitch (%02x)", face, tm.tmPitchAndFamily);
            if (fi->fixed == 0) CHECKF(tm.tmPitchAndFamily & TMPF_FIXED_PITCH, "%s should be variable pitch (%02x)", face, tm.tmPitchAndFamily);
            if (fi->fam >= 0) CHECKF((tm.tmPitchAndFamily & 0xF0) == fi->fam, "%s family %02x", face, tm.tmPitchAndFamily & 0xF0);
        }
        if (strcmp(face, "Symbol") && strcmp(face, "Marlett")) CHECKF(tm.tmBreakChar == 0x20, "%s break char %u", face, tm.tmBreakChar);
        CHECKF((tm.tmPitchAndFamily & 0x08) == 0, "%s reserved bit set (%02x)", face, tm.tmPitchAndFamily);
        fe_close(&c);
    }
}
#define X_tm_basic(id, name) DEF_FACE_TEST(fonts_tm_basic, w_tm_basic, id, name)
FACE_TESTS(X_tm_basic)

static void w_tm_charset(const char *face) {
    fe_ctx c; int cs = fe_req_cs(face);
    if (!fe_open(&c, face, -16, 400, 0, cs)) { fe_close(&c); return; }
    TEXTMETRICA tm; CHECK(U(GetTextMetricsA)(c.dc, &tm));
    CHECKF(tm.tmCharSet == cs, "%s requested %d got %u", face, cs, tm.tmCharSet);
    if (U(GetTextCharset)) EQ(U(GetTextCharset)(c.dc), tm.tmCharSet);
    fe_close(&c);
}
#define X_tm_charset(id, name) DEF_FACE_TEST(fonts_tm_charset, w_tm_charset, id, name)
FACE_TESTS(X_tm_charset)

static void w_tm_ascii_range(const char *face) {
    fe_ctx c; int cs = fe_req_cs(face);
    if (!fe_open(&c, face, -16, 400, 0, cs)) { fe_close(&c); return; }
    TEXTMETRICA a; TEXTMETRICW w;
    CHECK(U(GetTextMetricsA)(c.dc, &a)); CHECK(U(GetTextMetricsW)(c.dc, &w));
    if (strcmp(face, "Symbol") && strcmp(face, "Marlett")) {
        CHECKF(a.tmFirstChar <= 0x20 && a.tmLastChar >= 0x7E, "%s A range %u..%u", face, a.tmFirstChar, a.tmLastChar);
        CHECKF(w.tmFirstChar <= 0x20 && w.tmLastChar >= 0x7E, "%s W range %u..%u", face, w.tmFirstChar, w.tmLastChar);
        CHECKF(a.tmDefaultChar >= a.tmFirstChar && a.tmDefaultChar <= a.tmLastChar, "%s default %u", face, a.tmDefaultChar);
    }
    t_note("%s A:first=%u last=%u def=%u brk=%u  W:first=%u last=%u def=%u brk=%u", face, a.tmFirstChar, a.tmLastChar, a.tmDefaultChar,
           a.tmBreakChar, w.tmFirstChar, w.tmLastChar, w.tmDefaultChar, w.tmBreakChar);
    fe_close(&c);
}
#define X_tm_range(id, name) DEF_FACE_TEST(fonts_tm_range, w_tm_ascii_range, id, name)
FACE_TESTS(X_tm_range)

static void w_tm_aw(const char *face) {
    static const int hs[] = {-9, -13, -18, -27};
    for (unsigned i = 0; i < 4; i++) {
        fe_ctx c;
        if (!fe_open(&c, face, hs[i], 400, 0, fe_req_cs(face))) { fe_close(&c); return; }
        TEXTMETRICA a; TEXTMETRICW w; memset(&a, 0, sizeof a); memset(&w, 0, sizeof w);
        CHECK(U(GetTextMetricsA)(c.dc, &a)); CHECK(U(GetTextMetricsW)(c.dc, &w));
        CHECKF(a.tmHeight == w.tmHeight && a.tmAscent == w.tmAscent && a.tmDescent == w.tmDescent, "%s %d", face, hs[i]);
        CHECKF(a.tmInternalLeading == w.tmInternalLeading && a.tmExternalLeading == w.tmExternalLeading, "%s %d", face, hs[i]);
        CHECKF(a.tmAveCharWidth == w.tmAveCharWidth && a.tmWeight == w.tmWeight && a.tmOverhang == w.tmOverhang, "%s %d", face, hs[i]);
        CHECKF(a.tmDigitizedAspectX == w.tmDigitizedAspectX && a.tmDigitizedAspectY == w.tmDigitizedAspectY, "%s %d", face, hs[i]);
        CHECKF(a.tmItalic == w.tmItalic && a.tmUnderlined == w.tmUnderlined && a.tmStruckOut == w.tmStruckOut, "%s %d", face, hs[i]);
        CHECKF(a.tmPitchAndFamily == w.tmPitchAndFamily && a.tmCharSet == w.tmCharSet, "%s %d", face, hs[i]);
        if (a.tmMaxCharWidth != w.tmMaxCharWidth) t_note("%s %d max char width A %ld W %ld", face, hs[i], a.tmMaxCharWidth, w.tmMaxCharWidth);
        fe_close(&c);
    }
}
#define X_tm_aw(id, name) DEF_FACE_TEST(fonts_tm_aw, w_tm_aw, id, name)
FACE_TESTS(X_tm_aw)

static void w_tm_repeat(const char *face) {
    fe_ctx c;
    if (!fe_open(&c, face, -15, 400, 0, fe_req_cs(face))) { fe_close(&c); return; }
    TEXTMETRICA a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    CHECK(U(GetTextMetricsA)(c.dc, &a)); CHECK(U(GetTextMetricsA)(c.dc, &b));
    CHECKF(!memcmp(&a, &b, sizeof a), "%s repeated metrics differ", face);
    fe_close(&c);
}
#define X_tm_repeat(id, name) DEF_FACE_TEST(fonts_tm_repeat, w_tm_repeat, id, name)
FACE_TESTS(X_tm_repeat)

static void w_tm_extent(const char *face) {
    fe_ctx c;
    if (!fe_open(&c, face, -16, 400, 0, fe_req_cs(face))) { fe_close(&c); return; }
    TEXTMETRICA tm; U(GetTextMetricsA)(c.dc, &tm);
    SIZE s; memset(&s, 0xCC, sizeof s);
    CHECK(U(GetTextExtentPoint32A)(c.dc, "Hxg", 3, &s));
    CHECKF(s.cy == tm.tmHeight, "%s cy %ld tmHeight %ld", face, s.cy, tm.tmHeight);
    CHECK(s.cx > 0);
    SIZE e; memset(&e, 0xCC, sizeof e);
    CHECK(U(GetTextExtentPoint32A)(c.dc, "", 0, &e));
    EQ(e.cx, 0);
    SIZE one, two;
    U(GetTextExtentPoint32A)(c.dc, "H", 1, &one); U(GetTextExtentPoint32A)(c.dc, "HH", 2, &two);
    CHECKF(two.cx >= one.cx && two.cx <= 2 * one.cx + 2, "%s HH %ld H %ld", face, two.cx, one.cx);
    fe_close(&c);
}
#define X_tm_extent(id, name) DEF_FACE_TEST(fonts_tm_extent, w_tm_extent, id, name)
FACE_TESTS(X_tm_extent)

static void w_fixed_width(const char *face) {
    fe_ctx c;
    if (!fe_open(&c, face, -16, 400, 0, fe_req_cs(face))) { fe_close(&c); return; }
    TEXTMETRICA tm; U(GetTextMetricsA)(c.dc, &tm);
    CHECKF(!(tm.tmPitchAndFamily & TMPF_FIXED_PITCH), "%s not fixed", face);
    if (!strcmp(face, "Courier New") || !strcmp(face, "Terminal") || !strcmp(face, "Fixedsys"))
        CHECKF(tm.tmAveCharWidth == tm.tmMaxCharWidth, "%s ave %ld max %ld", face, tm.tmAveCharWidth, tm.tmMaxCharWidth);
    else if (tm.tmAveCharWidth != tm.tmMaxCharWidth) t_note("%s ave %ld max %ld", face, tm.tmAveCharWidth, tm.tmMaxCharWidth);
    int w[95]; int ok = U(GetCharWidth32A)(c.dc, 0x20, 0x7E, w);
    CHECK(ok);
    for (int i = 1; ok && i < 95; i++) CHECKF(w[i] == w[0], "%s char %02x width %d vs %d", face, 0x20 + i, w[i], w[0]);
    SIZE a, b;
    U(GetTextExtentPoint32A)(c.dc, "WWWWWWWW", 8, &a); U(GetTextExtentPoint32A)(c.dc, "iiiiiiii", 8, &b);
    EQ(a.cx, b.cx);
    EQ(a.cx, 8 * w[0]);
    fe_close(&c);
}
TEST(fonts_fixed_courier_new) { FE_BASE(); REQUIRE(U(GetCharWidth32A)); REQUIRE(U(GetTextExtentPoint32A)); w_fixed_width("Courier New"); }
TEST(fonts_fixed_consolas) { FE_BASE(); REQUIRE(U(GetCharWidth32A)); REQUIRE(U(GetTextExtentPoint32A)); w_fixed_width("Consolas"); }
TEST(fonts_fixed_terminal) { FE_BASE(); REQUIRE(U(GetCharWidth32A)); REQUIRE(U(GetTextExtentPoint32A)); w_fixed_width("Terminal"); }
TEST(fonts_fixed_fixedsys) { FE_BASE(); REQUIRE(U(GetCharWidth32A)); REQUIRE(U(GetTextExtentPoint32A)); w_fixed_width("Fixedsys"); }

TEST(fonts_fixed_courier_sizes) {
    FE_BASE();
    for (int n = 8; n <= 40; n += 4) {
        fe_ctx c; if (!fe_open(&c, "Courier New", -n, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
        TEXTMETRICA tm; U(GetTextMetricsA)(c.dc, &tm);
        CHECKF(labs(tm.tmAveCharWidth - tm.tmMaxCharWidth) <= 1, "-%d ave %ld max %ld", n, tm.tmAveCharWidth, tm.tmMaxCharWidth);
        if (tm.tmAveCharWidth != tm.tmMaxCharWidth) t_note("Courier New -%d ave %ld max %ld", n, tm.tmAveCharWidth, tm.tmMaxCharWidth);
        CHECKF(!(tm.tmPitchAndFamily & TMPF_FIXED_PITCH), "-%d", n);
        fe_close(&c);
    }
}
TEST(fonts_variable_arial_widths_differ) {
    FE_BASE();
    fe_ctx c; if (!fe_open(&c, "Arial", -16, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    SIZE a, b; U(GetTextExtentPoint32A)(c.dc, "WWWWWWWW", 8, &a); U(GetTextExtentPoint32A)(c.dc, "iiiiiiii", 8, &b);
    CHECK(a.cx > b.cx);
    TEXTMETRICA tm; U(GetTextMetricsA)(c.dc, &tm);
    CHECK(tm.tmPitchAndFamily & TMPF_FIXED_PITCH);
    CHECK(tm.tmMaxCharWidth > tm.tmAveCharWidth);
    fe_close(&c);
}

TEST(fonts_tm_bad_dc) {
    FE_BASE(); REQUIRE(U(GetTextMetricsW));
    TEXTMETRICA a; TEXTMETRICW w;
    memset(&a, 0x5A, sizeof a); memset(&w, 0x5A, sizeof w);
    SetLastError(0);
    EQ(U(GetTextMetricsA)(NULL, &a), 0);
    EQ(U(GetTextMetricsW)(NULL, &w), 0);
    EQ(U(GetTextMetricsA)((HDC)(ULONG_PTR)0xABCD, &a), 0);
    EQ(U(GetTextMetricsW)((HDC)(ULONG_PTR)0xABCD, &w), 0);
}
TEST(fonts_tm_default_font_system) {
    FE_BASE();
    HDC dc = U(CreateCompatibleDC)(NULL);
    TEXTMETRICA tm; memset(&tm, 0, sizeof tm);
    CHECK(U(GetTextMetricsA)(dc, &tm));
    CHECK(tm.tmHeight > 0);
    CHECK(!(tm.tmPitchAndFamily & TMPF_TRUETYPE));
    t_note("System font: height %ld ave %ld weight %ld dpi %d", tm.tmHeight, tm.tmAveCharWidth, tm.tmWeight, fe_dpi());
    U(DeleteDC)(dc);
}
TEST(fonts_tm_dpi_relationship) {
    FE_BASE(); REQUIRE(U(GetDeviceCaps));
    fe_ctx c; if (!fe_open(&c, "Arial", -16, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    TEXTMETRICA tm; U(GetTextMetricsA)(c.dc, &tm);
    t_note("Arial -16 digitized aspect %ld x %ld; LOGPIXELS %d x %d", tm.tmDigitizedAspectX, tm.tmDigitizedAspectY,
           U(GetDeviceCaps)(c.dc, LOGPIXELSX), U(GetDeviceCaps)(c.dc, LOGPIXELSY));
    CHECK(tm.tmDigitizedAspectX > 0);
    fe_close(&c);
}
TEST(fonts_tm_mapmode_text_vs_anisotropic) {
    FE_BASE(); REQUIRE(U(SetMapMode));
    HDC dc = U(CreateCompatibleDC)(NULL);
    HFONT f = fe_mk(-16, 0, 400, 0, ANSI_CHARSET, "Arial"); HGDIOBJ old = U(SelectObject)(dc, f);
    TEXTMETRICA t1; U(GetTextMetricsA)(dc, &t1);
    U(SetMapMode)(dc, MM_LOMETRIC);
    TEXTMETRICA t2; U(GetTextMetricsA)(dc, &t2);
    t_note("MM_TEXT tmHeight %ld, MM_LOMETRIC (same font) tmHeight %ld", t1.tmHeight, t2.tmHeight);
    CHECK(t2.tmHeight > 0);
    U(SelectObject)(dc, old); U(DeleteObject)(f); U(DeleteDC)(dc);
}
TEST(fonts_tm_mm_lometric_negative_height) {
    FE_BASE(); REQUIRE(U(SetMapMode));
    HDC dc = U(CreateCompatibleDC)(NULL);
    U(SetMapMode)(dc, MM_LOMETRIC);
    HFONT f = fe_mk(-120, 0, 400, 0, ANSI_CHARSET, "Arial");   /* 12 mm em height */
    HGDIOBJ old = U(SelectObject)(dc, f);
    TEXTMETRICA tm; memset(&tm, 0, sizeof tm); CHECK(U(GetTextMetricsA)(dc, &tm));
    CHECK(tm.tmHeight > 0);
    t_note("MM_LOMETRIC -120 -> tmHeight (logical) %ld", tm.tmHeight);
    U(SelectObject)(dc, old); U(DeleteObject)(f); U(DeleteDC)(dc);
}

/* ---- OUTLINETEXTMETRIC ---- */
static void w_otm(const char *face) {
    fe_ctx c; if (!fe_open(&c, face, -20, 400, 0, fe_req_cs(face))) { fe_close(&c); return; }
    UINT n = U(GetOutlineTextMetricsA)(c.dc, 0, NULL);
    CHECKF(n > sizeof(OUTLINETEXTMETRICA), "%s size %u", face, n);
    EQ(U(GetOutlineTextMetricsA)(c.dc, 12345, NULL), n);
    UINT nw = U(GetOutlineTextMetricsW)(c.dc, 0, NULL);
    CHECKF(nw > sizeof(OUTLINETEXTMETRICW), "%s W size %u", face, nw);
    if (n > sizeof(OUTLINETEXTMETRICA) && n < 65536) {
        BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n + 16);
        memset(buf + n, 0xA5, 16);
        OUTLINETEXTMETRICA *o = (OUTLINETEXTMETRICA *)buf;
        UINT r = U(GetOutlineTextMetricsA)(c.dc, n, o);
        CHECKF(r != 0, "%s fill failed", face);
        CHECKF(o->otmSize == n, "%s otmSize %u vs %u", face, o->otmSize, n);
        CHECKF(buf[n] == 0xA5 && buf[n + 15] == 0xA5, "%s wrote past end", face);
        TEXTMETRICA tm; U(GetTextMetricsA)(c.dc, &tm);
        CHECKF(o->otmTextMetrics.tmHeight == tm.tmHeight && o->otmTextMetrics.tmAscent == tm.tmAscent &&
               o->otmTextMetrics.tmAveCharWidth == tm.tmAveCharWidth && o->otmTextMetrics.tmWeight == tm.tmWeight, "%s tm mismatch", face);
        int sym = !strcmp(face, "Marlett") || !strcmp(face, "Symbol");
        if (!sym) CHECKF(o->otmAscent > 0 && o->otmDescent < 0, "%s ascent %d descent %d", face, o->otmAscent, o->otmDescent);
        CHECKF(o->otmEMSquare > 0, "%s em", face);
        if (!sym) CHECKF(o->otmsUnderscoreSize > 0, "%s underscore size", face);
        ULONG_PTR offs[4] = {(ULONG_PTR)o->otmpFamilyName, (ULONG_PTR)o->otmpFaceName, (ULONG_PTR)o->otmpStyleName, (ULONG_PTR)o->otmpFullName};
        for (int i = 0; i < 4; i++) CHECKF(offs[i] >= sizeof(OUTLINETEXTMETRICA) && offs[i] < n, "%s string %d offset %llu", face, i, (unsigned long long)offs[i]);
        if (offs[0] < n) {
            const char *fam = (const char *)buf + offs[0];
            CHECKF(!lstrcmpiA(fam, face), "%s family '%s'", face, fam);
            t_note("%s OTM: face='%s' style='%s' full='%s' em=%u", face, buf + offs[1], buf + offs[2], buf + offs[3], o->otmEMSquare);
        }
        HeapFree(GetProcessHeap(), 0, buf);
    }
    fe_close(&c);
}
#define X_otm(id, name) DEF_FACE_TEST(fonts_otm, w_otm, id, name)
TT_FACE_TESTS(X_otm)

static void w_otm_w(const char *face) {
    fe_ctx c; if (!fe_open(&c, face, -20, 400, 0, fe_req_cs(face))) { fe_close(&c); return; }
    UINT n = U(GetOutlineTextMetricsW)(c.dc, 0, NULL);
    CHECKF(n > sizeof(OUTLINETEXTMETRICW), "%s", face);
    if (n > sizeof(OUTLINETEXTMETRICW) && n < 65536) {
        BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n);
        OUTLINETEXTMETRICW *o = (OUTLINETEXTMETRICW *)buf;
        CHECK(U(GetOutlineTextMetricsW)(c.dc, n, o) != 0);
        CHECKF(o->otmSize == n, "%s otmSize", face);
        ULONG_PTR fo = (ULONG_PTR)o->otmpFamilyName;
        CHECKF(fo >= sizeof(OUTLINETEXTMETRICW) && fo + 2 <= n, "%s family offset %llu", face, (unsigned long long)fo);
        if (fo < n) {
            char fam[80]; WideCharToMultiByte(CP_ACP, 0, (WCHAR *)(buf + fo), -1, fam, sizeof fam, NULL, NULL);
            CHECKF(!lstrcmpiA(fam, face), "%s W family '%s'", face, fam);
        }
        CHECK(o->otmTextMetrics.tmHeight > 0);
        HeapFree(GetProcessHeap(), 0, buf);
    }
    fe_close(&c);
}
#define X_otmw(id, name) DEF_FACE_TEST(fonts_otmw, w_otm_w, id, name)
TT_FACE_TESTS(X_otmw)

static void w_otm_raster(const char *face) {
    fe_ctx c; if (!fe_open(&c, face, -16, 400, 0, fe_req_cs(face))) { fe_close(&c); return; }
    EQ(U(GetOutlineTextMetricsA)(c.dc, 0, NULL), 0);
    EQ(U(GetOutlineTextMetricsW)(c.dc, 0, NULL), 0);
    BYTE buf[2048]; memset(buf, 0, sizeof buf);
    EQ(U(GetOutlineTextMetricsA)(c.dc, sizeof buf, (OUTLINETEXTMETRICA *)buf), 0);
    fe_close(&c);
}
#define X_otmr(id, name) DEF_FACE_TEST(fonts_otm_raster, w_otm_raster, id, name)
#define RASTER_TESTS(X) X(system, "System") X(terminal, "Terminal") X(fixedsys, "Fixedsys") X(ms_sans_serif, "MS Sans Serif") X(small_fonts, "Small Fonts")
RASTER_TESTS(X_otmr)

TEST(fonts_otm_bad_dc) {
    FE_BASE(); REQUIRE(U(GetOutlineTextMetricsA)); REQUIRE(U(GetOutlineTextMetricsW));
    EQ(U(GetOutlineTextMetricsA)(NULL, 0, NULL), 0);
    EQ(U(GetOutlineTextMetricsW)(NULL, 0, NULL), 0);
    EQ(U(GetOutlineTextMetricsA)((HDC)(ULONG_PTR)0x777, 0, NULL), 0);
    BYTE buf[600];
    EQ(U(GetOutlineTextMetricsW)(NULL, sizeof buf, (OUTLINETEXTMETRICW *)buf), 0);
}
TEST(fonts_otm_small_buffer) {
    FE_BASE(); REQUIRE(U(GetOutlineTextMetricsA));
    fe_ctx c; if (!fe_open(&c, "Arial", -20, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    UINT n = U(GetOutlineTextMetricsA)(c.dc, 0, NULL);
    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n + 8);
    memset(buf, 0x6B, n + 8);
    static const UINT sizes[] = {1, 4, 8, sizeof(OUTLINETEXTMETRICA) - 1, sizeof(OUTLINETEXTMETRICA)};
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        memset(buf, 0x6B, n + 8);
        UINT r = U(GetOutlineTextMetricsA)(c.dc, sizes[i], (OUTLINETEXTMETRICA *)buf);
        t_note("GetOutlineTextMetricsA(cb=%u of %u) = %u; byte at cb = %02x", sizes[i], n, r, sizes[i] < n + 8 ? buf[sizes[i]] : 0);
        CHECKF(sizes[i] >= n || buf[sizes[i]] == 0x6B || r == 0, "cb=%u wrote past the caller's size", sizes[i]);
    }
    HeapFree(GetProcessHeap(), 0, buf);
    fe_close(&c);
}
TEST(fonts_otm_italic_angle) {
    FE_BASE();
    fe_ctx c; if (!fe_open(&c, "Arial", -20, 400, 1, ANSI_CHARSET)) { fe_close(&c); return; }
    UINT n = U(GetOutlineTextMetricsA)(c.dc, 0, NULL);
    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n);
    OUTLINETEXTMETRICA *o = (OUTLINETEXTMETRICA *)buf;
    if (n && U(GetOutlineTextMetricsA)(c.dc, n, o)) {
        CHECK(o->otmTextMetrics.tmItalic != 0);
        t_note("Arial italic: otmItalicAngle=%d fsSelection=%04x style='%s'", o->otmItalicAngle, o->otmfsSelection, buf + (ULONG_PTR)o->otmpStyleName);
        CHECK(o->otmItalicAngle <= 0);
    }
    HeapFree(GetProcessHeap(), 0, buf);
    fe_close(&c);
}
TEST(fonts_otm_bold_selection) {
    FE_BASE();
    fe_ctx c; if (!fe_open(&c, "Arial", -20, 700, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    UINT n = U(GetOutlineTextMetricsA)(c.dc, 0, NULL);
    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n);
    OUTLINETEXTMETRICA *o = (OUTLINETEXTMETRICA *)buf;
    if (n && U(GetOutlineTextMetricsA)(c.dc, n, o)) {
        CHECK(o->otmTextMetrics.tmWeight == 700);
        CHECKF(o->otmfsSelection & 0x20, "bold bit missing in fsSelection %04x", o->otmfsSelection);
        CHECKF(!lstrcmpA((char *)buf + (ULONG_PTR)o->otmpStyleName, "Bold"), "style '%s'", buf + (ULONG_PTR)o->otmpStyleName);
        CHECKF(!lstrcmpA((char *)buf + (ULONG_PTR)o->otmpFullName, "Arial Bold"), "full '%s'", buf + (ULONG_PTR)o->otmpFullName);
    }
    HeapFree(GetProcessHeap(), 0, buf);
    fe_close(&c);
}
TEST(fonts_otm_arial_regular_strings) {
    FE_BASE();
    fe_ctx c; if (!fe_open(&c, "Arial", -20, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    UINT n = U(GetOutlineTextMetricsA)(c.dc, 0, NULL);
    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n);
    OUTLINETEXTMETRICA *o = (OUTLINETEXTMETRICA *)buf;
    if (n && U(GetOutlineTextMetricsA)(c.dc, n, o)) {
        EQ(o->otmEMSquare, 2048);
        EQ(o->otmItalicAngle, 0);
        CHECKF(!lstrcmpA((char *)buf + (ULONG_PTR)o->otmpFamilyName, "Arial"), "family");
        CHECKF(!lstrcmpA((char *)buf + (ULONG_PTR)o->otmpFaceName, "Arial"), "face");
        CHECKF(!lstrcmpA((char *)buf + (ULONG_PTR)o->otmpStyleName, "Regular"), "style");
        CHECKF(!lstrcmpA((char *)buf + (ULONG_PTR)o->otmpFullName, "Arial"), "full");
        CHECK(o->otmMacAscent > 0);
        CHECK(o->otmsStrikeoutSize > 0);
        t_note("Arial OTM: ascent %d descent %d linegap %u capheight %u xheight %u", o->otmAscent, o->otmDescent, o->otmLineGap, o->otmsCapEmHeight, o->otmsXHeight);
    }
    HeapFree(GetProcessHeap(), 0, buf);
    fe_close(&c);
}

/* ---- GetFontData ---- */
static const struct { const char *face, *file; } tt_files[] = {
    {"Arial", "arial.ttf"}, {"Courier New", "cour.ttf"}, {"Times New Roman", "times.ttf"}, {"Tahoma", "tahoma.ttf"},
    {"Segoe UI", "segoeui.ttf"}, {"Consolas", "consola.ttf"}, {"Symbol", "symbol.ttf"}, {"Marlett", "marlett.ttf"}};

static void w_fontdata(const char *face) {
    if (!U(GetFontData)) { t_skip("no GetFontData"); return; }
    fe_ctx c; if (!fe_open(&c, face, -16, 400, 0, fe_req_cs(face))) { fe_close(&c); return; }
    DWORD hs = U(GetFontData)(c.dc, FE_TAG('h', 'e', 'a', 'd'), 0, NULL, 0);
    EQ(hs, 54);
    BYTE head[64]; memset(head, 0, sizeof head);
    EQ(U(GetFontData)(c.dc, FE_TAG('h', 'e', 'a', 'd'), 0, head, 54), 54);
    EQ(fe_be32(head + 12), 0x5F0F3CF5);                       /* magic number */
    unsigned upm = fe_be16(head + 18);
    CHECKF(upm >= 16 && upm <= 16384, "%s unitsPerEm %u", face, upm);
    DWORD sz;
    sz = U(GetFontData)(c.dc, FE_TAG('c', 'm', 'a', 'p'), 0, NULL, 0); CHECKF(sz != GDI_ERROR && sz >= 4, "%s cmap size %lu", face, sz);
    sz = U(GetFontData)(c.dc, FE_TAG('n', 'a', 'm', 'e'), 0, NULL, 0); CHECKF(sz != GDI_ERROR && sz >= 6, "%s name size %lu", face, sz);
    sz = U(GetFontData)(c.dc, FE_TAG('O', 'S', '/', '2'), 0, NULL, 0); CHECKF(sz != GDI_ERROR && sz >= 68, "%s OS/2 size %lu", face, sz);
    sz = U(GetFontData)(c.dc, FE_TAG('h', 'h', 'e', 'a'), 0, NULL, 0); EQ(sz, 36);
    sz = U(GetFontData)(c.dc, FE_TAG('m', 'a', 'x', 'p'), 0, NULL, 0); CHECKF(sz == 32 || sz == 6, "%s maxp size %lu", face, sz);
    sz = U(GetFontData)(c.dc, FE_TAG('p', 'o', 's', 't'), 0, NULL, 0); CHECKF(sz != GDI_ERROR && sz >= 32, "%s post size %lu", face, sz);
    EQ(U(GetFontData)(c.dc, FE_TAG('z', 'z', 'z', 'z'), 0, NULL, 0), GDI_ERROR);
    BYTE tmp[16];
    EQ(U(GetFontData)(c.dc, FE_TAG('z', 'z', 'z', 'z'), 0, tmp, sizeof tmp), GDI_ERROR);
    EQ(U(GetFontData)(c.dc, FE_TAG('t', 'c', 'f', 'f'), 0, NULL, 0), GDI_ERROR);
    /* head read in two pieces equals the whole */
    BYTE p1[20], p2[34]; memset(p1, 0, sizeof p1); memset(p2, 0, sizeof p2);
    EQ(U(GetFontData)(c.dc, FE_TAG('h', 'e', 'a', 'd'), 0, p1, 20), 20);
    EQ(U(GetFontData)(c.dc, FE_TAG('h', 'e', 'a', 'd'), 20, p2, 34), 34);
    CHECK(!memcmp(p1, head, 20) && !memcmp(p2, head + 20, 34));
    /* maxp numGlyphs */
    BYTE mx[8]; memset(mx, 0, sizeof mx);
    if (U(GetFontData)(c.dc, FE_TAG('m', 'a', 'x', 'p'), 0, mx, 6) == 6) CHECKF(fe_be16(mx + 4) >= 1, "%s numGlyphs", face);
    fe_close(&c);
}
#define X_fd(id, name) DEF_FACE_TEST(fonts_data_tables, w_fontdata, id, name)
TT_FACE_TESTS(X_fd)

static void w_fontdata_file(const char *face) {
    const char *leaf = NULL;
    for (unsigned i = 0; i < sizeof tt_files / sizeof tt_files[0]; i++) if (!strcmp(tt_files[i].face, face)) leaf = tt_files[i].file;
    if (!leaf || !U(GetFontData)) return;
    char path[MAX_PATH + 32]; fe_fontfile(path, sizeof path, leaf);
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_note("%s not readable", path); return; }
    DWORD fsz = GetFileSize(h, NULL); CloseHandle(h);
    fe_ctx c; if (!fe_open(&c, face, -16, 400, 0, fe_req_cs(face))) { fe_close(&c); return; }
    DWORD n = U(GetFontData)(c.dc, 0, 0, NULL, 0);
    CHECKF(n == fsz, "%s whole-file size %lu vs file %lu", face, n, fsz);
    BYTE first[4] = {9, 9, 9, 9};
    EQ(U(GetFontData)(c.dc, 0, 0, first, 4), 4);
    CHECKF(fe_be32(first) == 0x00010000 || fe_be32(first) == 0x74727565 || fe_be32(first) == 0x4F54544F, "%s sfnt version %08x", face, fe_be32(first));
    BYTE last[4] = {0};
    if (fsz > 8) EQ(U(GetFontData)(c.dc, 0, fsz - 4, last, 4), 4);
    BYTE *whole = fe_readfile(path, &fsz);
    if (whole && fsz > 8) {
        CHECK(!memcmp(first, whole, 4));
        CHECK(!memcmp(last, whole + fsz - 4, 4));
        BYTE mid[64]; DWORD off = fsz / 2;
        EQ(U(GetFontData)(c.dc, 0, off, mid, 64), 64);
        CHECK(!memcmp(mid, whole + off, 64));
    }
    fe_free(whole);
    fe_close(&c);
}
#define X_fdf(id, name) DEF_FACE_TEST(fonts_data_file, w_fontdata_file, id, name)
TT_FACE_TESTS(X_fdf)

TEST(fonts_data_cmap_header) {
    FE_BASE(); REQUIRE(U(GetFontData));
    fe_ctx c; if (!fe_open(&c, "Arial", -16, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    BYTE h[4] = {0};
    EQ(U(GetFontData)(c.dc, FE_TAG('c', 'm', 'a', 'p'), 0, h, 4), 4);
    EQ(fe_be16(h), 0);
    CHECK(fe_be16(h + 2) >= 1);
    fe_close(&c);
}
TEST(fonts_data_os2_weightclass) {
    FE_BASE(); REQUIRE(U(GetFontData));
    static const struct { int w; unsigned cls; } t[] = {{400, 400}, {700, 700}};
    for (int i = 0; i < 2; i++) {
        fe_ctx c; if (!fe_open(&c, "Arial", -16, t[i].w, 0, ANSI_CHARSET)) { fe_close(&c); return; }
        BYTE o[8] = {0};
        EQ(U(GetFontData)(c.dc, FE_TAG('O', 'S', '/', '2'), 0, o, 8), 8);
        EQ(fe_be16(o + 4), t[i].cls);
        fe_close(&c);
    }
}
TEST(fonts_data_name_table_header) {
    FE_BASE(); REQUIRE(U(GetFontData));
    fe_ctx c; if (!fe_open(&c, "Times New Roman", -16, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    BYTE h[6] = {0};
    EQ(U(GetFontData)(c.dc, FE_TAG('n', 'a', 'm', 'e'), 0, h, 6), 6);
    CHECK(fe_be16(h) <= 1);
    unsigned count = fe_be16(h + 2), strofs = fe_be16(h + 4);
    CHECK(count >= 4 && strofs >= 6 + count * 12);
    fe_close(&c);
}
TEST(fonts_data_tag_case_sensitive) {
    FE_BASE(); REQUIRE(U(GetFontData));
    fe_ctx c; if (!fe_open(&c, "Arial", -16, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    DWORD r = U(GetFontData)(c.dc, FE_TAG('H', 'E', 'A', 'D'), 0, NULL, 0);
    if (r != GDI_ERROR) t_note("upper-case tag 'HEAD' found, size %lu", r);
    fe_close(&c);
}
TEST(fonts_data_offset_edges) {
    FE_BASE(); REQUIRE(U(GetFontData));
    fe_ctx c; if (!fe_open(&c, "Arial", -16, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    BYTE b[128]; DWORD tag = FE_TAG('h', 'e', 'a', 'd');
    memset(b, 0, sizeof b);
    DWORD r = U(GetFontData)(c.dc, tag, 0, b, 100);
    t_note("head: request 100 bytes of a 54-byte table -> %lu", r);
    r = U(GetFontData)(c.dc, tag, 53, b, 1); EQ(r, 1);
    r = U(GetFontData)(c.dc, tag, 53, b, 2); t_note("head: offset 53 request 2 -> %lu", r);
    r = U(GetFontData)(c.dc, tag, 54, b, 1); t_note("head: offset 54 request 1 -> %lu", r);
    r = U(GetFontData)(c.dc, tag, 54, NULL, 0); t_note("head: offset 54 size query -> %lu", r);
    r = U(GetFontData)(c.dc, tag, 1000, b, 1); t_note("head: offset 1000 request 1 -> %lu", r);
    r = U(GetFontData)(c.dc, tag, 0xFFFFFFF0, b, 4); t_note("head: offset 0xFFFFFFF0 request 4 -> %lu", r);
    r = U(GetFontData)(c.dc, tag, 10, NULL, 0); t_note("head: offset 10 size query -> %lu", r);
    r = U(GetFontData)(c.dc, tag, 0, b, 0); t_note("head: non-NULL buffer cb=0 -> %lu", r);
    fe_close(&c);
}
TEST(fonts_data_bad_dc) {
    FE_BASE(); REQUIRE(U(GetFontData));
    BYTE b[16];
    EQ(U(GetFontData)(NULL, FE_TAG('h', 'e', 'a', 'd'), 0, b, 16), GDI_ERROR);
    EQ(U(GetFontData)(NULL, 0, 0, NULL, 0), GDI_ERROR);
    EQ(U(GetFontData)((HDC)(ULONG_PTR)0x4242, 0, 0, b, 4), GDI_ERROR);
}
TEST(fonts_data_raster_font) {
    FE_BASE(); REQUIRE(U(GetFontData));
    fe_ctx c; if (!fe_open(&c, "System", -16, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    DWORD r = U(GetFontData)(c.dc, FE_TAG('h', 'e', 'a', 'd'), 0, NULL, 0);
    CHECKF(r == GDI_ERROR, "head of raster font -> %lu", r);
    r = U(GetFontData)(c.dc, 0, 0, NULL, 0);
    t_note("whole-file size of raster System font: %lu", r);
    fe_close(&c);
}
TEST(fonts_data_stable_across_sizes) {
    FE_BASE(); REQUIRE(U(GetFontData));
    DWORD sizes[3];
    static const int hs[] = {-10, -40, -100};
    for (int i = 0; i < 3; i++) {
        fe_ctx c; if (!fe_open(&c, "Arial", hs[i], 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
        sizes[i] = U(GetFontData)(c.dc, FE_TAG('c', 'm', 'a', 'p'), 0, NULL, 0);
        fe_close(&c);
    }
    CHECK(sizes[0] == sizes[1] && sizes[1] == sizes[2]);
}
TEST(fonts_data_bold_italic_are_other_files) {
    FE_BASE(); REQUIRE(U(GetFontData));
    DWORD sz[2];
    for (int i = 0; i < 2; i++) {
        fe_ctx c; if (!fe_open(&c, "Arial", -16, i ? 700 : 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
        sz[i] = U(GetFontData)(c.dc, 0, 0, NULL, 0);
        fe_close(&c);
    }
    CHECK(sz[0] != 0 && sz[1] != 0);
    if (sz[0] == sz[1]) t_note("regular and bold Arial report the same file size %lu", sz[0]);
}

/* ---- Unicode ranges ---- */
static void w_ranges(const char *face, int must_latin) {
    if (!U(GetFontUnicodeRanges)) { t_skip("no GetFontUnicodeRanges"); return; }
    fe_ctx c; if (!fe_open(&c, face, -16, 400, 0, fe_req_cs(face))) { fe_close(&c); return; }
    DWORD n = U(GetFontUnicodeRanges)(c.dc, NULL);
    if (must_latin) CHECKF(n >= sizeof(GLYPHSET), "%s size %lu", face, n);
    else if (n < sizeof(GLYPHSET)) t_note("%s ranges size %lu", face, n);
    if (n >= sizeof(GLYPHSET) && n < 4 * 1024 * 1024) {
        GLYPHSET *gs = (GLYPHSET *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n + 16);
        DWORD r = U(GetFontUnicodeRanges)(c.dc, gs);
        EQ(r, n);
        EQ(gs->cbThis, n);
        CHECK(gs->cRanges >= 1 && gs->cGlyphsSupported >= 1);
        CHECKF(sizeof(GLYPHSET) + (gs->cRanges - 1) * sizeof(WCRANGE) == n, "%s cbThis vs cRanges", face);
        DWORD sum = 0; int sorted = 1;
        for (DWORD i = 0; i < gs->cRanges; i++) {
            sum += gs->ranges[i].cGlyphs;
            if (i && gs->ranges[i].wcLow <= (WCHAR)(gs->ranges[i - 1].wcLow + gs->ranges[i - 1].cGlyphs - 1)) sorted = 0;
        }
        CHECKF(sum == gs->cGlyphsSupported, "%s sum %lu vs %lu", face, sum, gs->cGlyphsSupported);
        CHECKF(sorted, "%s ranges not strictly increasing", face);
        if (must_latin) {
            for (int ch = 'A'; ch <= 'Z'; ch++) {
                int covered = 0;
                for (DWORD i = 0; i < gs->cRanges; i++) if (ch >= gs->ranges[i].wcLow && ch < gs->ranges[i].wcLow + gs->ranges[i].cGlyphs) covered = 1;
                CHECKF(covered, "%s lacks %c", face, ch);
            }
        }
        t_note("%s: %lu ranges, %lu glyphs, flAccel %lx", face, gs->cRanges, gs->cGlyphsSupported, gs->flAccel);
        HeapFree(GetProcessHeap(), 0, gs);
    }
    fe_close(&c);
}
TEST(fonts_ranges_arial) { FE_BASE(); w_ranges("Arial", 1); }
TEST(fonts_ranges_times) { FE_BASE(); w_ranges("Times New Roman", 1); }
TEST(fonts_ranges_courier_new) { FE_BASE(); w_ranges("Courier New", 1); }
TEST(fonts_ranges_tahoma) { FE_BASE(); w_ranges("Tahoma", 1); }
TEST(fonts_ranges_segoe_ui) { FE_BASE(); w_ranges("Segoe UI", 1); }
TEST(fonts_ranges_consolas) { FE_BASE(); w_ranges("Consolas", 1); }
TEST(fonts_ranges_symbol) { FE_BASE(); w_ranges("Symbol", 0); }
TEST(fonts_ranges_marlett) { FE_BASE(); w_ranges("Marlett", 0); }
TEST(fonts_ranges_bad_dc) {
    FE_BASE(); REQUIRE(U(GetFontUnicodeRanges));
    EQ(U(GetFontUnicodeRanges)(NULL, NULL), 0);
    EQ(U(GetFontUnicodeRanges)((HDC)(ULONG_PTR)0x999, NULL), 0);
}
TEST(fonts_ranges_arial_covers_basic_latin_start) {
    FE_BASE(); REQUIRE(U(GetFontUnicodeRanges));
    fe_ctx c; if (!fe_open(&c, "Arial", -16, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    DWORD n = U(GetFontUnicodeRanges)(c.dc, NULL);
    GLYPHSET *gs = (GLYPHSET *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n + 16);
    if (U(GetFontUnicodeRanges)(c.dc, gs)) {
        CHECK(gs->ranges[0].wcLow <= 0x20);
        int latin1 = 0, greek = 0, cyr = 0;
        for (DWORD i = 0; i < gs->cRanges; i++) {
            WCHAR lo = gs->ranges[i].wcLow, hi = (WCHAR)(lo + gs->ranges[i].cGlyphs - 1);
            if (lo <= 0xE9 && hi >= 0xE9) latin1 = 1;
            if (lo <= 0x3B1 && hi >= 0x3B1) greek = 1;
            if (lo <= 0x416 && hi >= 0x416) cyr = 1;
        }
        CHECK(latin1); CHECK(greek); CHECK(cyr);
    }
    HeapFree(GetProcessHeap(), 0, gs);
    fe_close(&c);
}
TEST(fonts_ranges_raster_font) {
    FE_BASE(); REQUIRE(U(GetFontUnicodeRanges));
    fe_ctx c; if (!fe_open(&c, "System", -16, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    DWORD n = U(GetFontUnicodeRanges)(c.dc, NULL);
    t_note("GetFontUnicodeRanges(System) size query = %lu", n);
    fe_close(&c);
}

/* ---- char widths ---- */
TEST(fonts_charwidth_a_w_agree) {
    FE_BASE(); REQUIRE(U(GetCharWidth32A)); REQUIRE(U(GetCharWidth32W));
    fe_ctx c; if (!fe_open(&c, "Arial", -16, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    int a[95], w[95];
    CHECK(U(GetCharWidth32A)(c.dc, 0x20, 0x7E, a)); CHECK(U(GetCharWidth32W)(c.dc, 0x20, 0x7E, w));
    for (int i = 0; i < 95; i++) CHECKF(a[i] == w[i], "char %02x A %d W %d", 0x20 + i, a[i], w[i]);
    fe_close(&c);
}
TEST(fonts_charwidth_invalid_range) {
    FE_BASE(); REQUIRE(U(GetCharWidth32A));
    fe_ctx c; if (!fe_open(&c, "Arial", -16, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    int w[8];
    SetLastError(0);
    EQ(U(GetCharWidth32A)(c.dc, 0x30, 0x20, w), 0);
    EQ(U(GetCharWidth32A)(NULL, 0x20, 0x21, w), 0);
    fe_close(&c);
}
TEST(fonts_charwidth_matches_extent) {
    FE_BASE(); REQUIRE(U(GetCharWidth32A));
    fe_ctx c; if (!fe_open(&c, "Tahoma", -16, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    int w[26]; U(GetCharWidth32A)(c.dc, 'a', 'z', w);
    for (int i = 0; i < 26; i++) {
        char ch = (char)('a' + i); SIZE s; U(GetTextExtentPoint32A)(c.dc, &ch, 1, &s);
        CHECKF(s.cx == w[i], "char %c extent %ld width %d", ch, s.cx, w[i]);
    }
    fe_close(&c);
}
TEST(fonts_abc_widths_sum) {
    FE_BASE(); REQUIRE(U(GetCharABCWidthsA));
    fe_ctx c; if (!fe_open(&c, "Arial", -24, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    ABC abc[26]; int w[26];
    CHECK(U(GetCharABCWidthsA)(c.dc, 'A', 'Z', abc));
    U(GetCharWidth32A)(c.dc, 'A', 'Z', w);
    for (int i = 0; i < 26; i++) CHECKF(abc[i].abcA + (int)abc[i].abcB + abc[i].abcC == w[i], "char %c abc %d %u %d width %d", 'A' + i, abc[i].abcA, abc[i].abcB, abc[i].abcC, w[i]);
    fe_close(&c);
}
TEST(fonts_abc_raster_fails) {
    FE_BASE(); REQUIRE(U(GetCharABCWidthsA));
    fe_ctx c; if (!fe_open(&c, "System", -16, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    ABC abc[4];
    BOOL r = U(GetCharABCWidthsA)(c.dc, 'A', 'D', abc);
    t_note("GetCharABCWidthsA on a raster font -> %d", r);
    fe_close(&c);
}
TEST(fonts_abcfloat_tt) {
    FE_BASE(); REQUIRE(U(GetCharABCWidthsFloatA));
    fe_ctx c; if (!fe_open(&c, "Arial", -24, 400, 0, ANSI_CHARSET)) { fe_close(&c); return; }
    ABCFLOAT f[4];
    CHECK(U(GetCharABCWidthsFloatA)(c.dc, 'A', 'D', f));
    for (int i = 0; i < 4; i++) CHECK(f[i].abcfB > 0);
    fe_close(&c);
}
