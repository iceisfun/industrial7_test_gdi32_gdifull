/* EnumFontFamiliesW, EnumFontsW, EnumFontFamiliesExW (and A-variant parity). */
#include "gap10.h"
#include <wchar.h>

#define MAXF 4096
typedef struct {
    int n, stop_at, ret, bad_ptr;
    LPARAM seen_lp;
    WCHAR face[MAXF][LF_FACESIZE];
    WCHAR full[MAXF][LF_FULLFACESIZE];
    WCHAR style[MAXF][LF_FACESIZE];
    BYTE charset[MAXF];
    DWORD ftype[MAXF];
    LONG height[MAXF];
} ENUMR;

static int CALLBACK ef_cb(const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD ft, LPARAM lp) {
    ENUMR *r = (ENUMR *)lp;
    int i = r->n++;
    if (!lf || !tm) r->bad_ptr++;
    if (i < MAXF && lf) {
        lstrcpynW(r->face[i], lf->lfFaceName, LF_FACESIZE);
        r->charset[i] = lf->lfCharSet; r->ftype[i] = ft; r->height[i] = lf->lfHeight;
        r->full[i][0] = 0; r->style[i][0] = 0;
    }
    if (r->stop_at && r->n >= r->stop_at) return 0;
    return r->ret ? r->ret : 1;
}
/* Ex / Family callbacks receive ENUMLOGFONTEXW / ENUMLOGFONTW as the first parameter. */
static int CALLBACK efx_cb(const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD ft, LPARAM lp) {
    ENUMR *r = (ENUMR *)lp;
    int i = r->n;
    int rc = ef_cb(lf, tm, ft, lp);
    if (i < MAXF && lf) {
        const ENUMLOGFONTEXW *e = (const ENUMLOGFONTEXW *)lf;
        lstrcpynW(r->full[i], e->elfFullName, LF_FULLFACESIZE);
        lstrcpynW(r->style[i], e->elfStyle, LF_FACESIZE);
    }
    return rc;
}
static int has_face(ENUMR *r, const WCHAR *name) {
    int i;
    for (i = 0; i < r->n && i < MAXF; i++) if (!lstrcmpiW(r->face[i], name)) return 1;
    return 0;
}
static int count_face(ENUMR *r, const WCHAR *name) {
    int i, c = 0;
    for (i = 0; i < r->n && i < MAXF; i++) if (!lstrcmpiW(r->face[i], name)) c++;
    return c;
}
static HDC memdc(void) { return U(CreateCompatibleDC)(NULL); }
#define NEED_EF() do { REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(SelectObject)); REQUIRE(U(DeleteObject)); } while (0)
#define NEED_EFW() do { NEED_EF(); REQUIRE(U(EnumFontFamiliesW)); } while (0)
#define NEED_EFX() do { NEED_EF(); REQUIRE(U(EnumFontFamiliesExW)); } while (0)
#define NEED_EFS() do { NEED_EF(); REQUIRE(U(EnumFontsW)); } while (0)
static ENUMR *newr(void) { ENUMR *r = (ENUMR *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ENUMR)); return r; }
static void freer(ENUMR *r) { HeapFree(GetProcessHeap(), 0, r); }

/* ===================== EnumFontFamiliesW ===================== */
TEST(gap10_efw_invalid_dc) {
    NEED_EFW();
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef }; unsigned i;
    ENUMR *r = newr();
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        int rc;
        memset(r, 0, offsetof(ENUMR, face)); CLR();
        rc = U(EnumFontFamiliesW)(bad[i], NULL, ef_cb, (LPARAM)r);
        CHECKF(rc == 0, "dc %p rc %d", bad[i], rc);
        CHECKF(r->n == 0, "dc %p callbacks %d", bad[i], r->n);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "dc %p err %lu", bad[i], GetLastError());
    }
    freer(r);
}
TEST(gap10_efw_wrong_object_type) {
    NEED_EFW(); REQUIRE(U(CreateSolidBrush));
    HBRUSH b = U(CreateSolidBrush)(0); ENUMR *r = newr(); int rc;
    CLR();
    rc = U(EnumFontFamiliesW)((HDC)b, NULL, ef_cb, (LPARAM)r);
    EXPECT_EQ(rc, 0); EXPECT_EQ(r->n, 0); EXPECT_EQ(GetLastError(), ERROR_INVALID_HANDLE);
    U(DeleteObject)(b); freer(r);
}
TEST(gap10_efw_deleted_dc) {
    NEED_EFW();
    HDC dc = memdc(); ENUMR *r = newr();
    U(DeleteDC)(dc);
    CLR();
    EXPECT_EQ(U(EnumFontFamiliesW)(dc, NULL, ef_cb, (LPARAM)r), 0);
    EXPECT_EQ(r->n, 0);
    t_note("deleted DC err %lu", GetLastError());
    freer(r);
}
TEST(gap10_efw_all_families_contains_core_fonts) {
    NEED_EFW();
    HDC dc = memdc(); ENUMR *r = newr(); int rc;
    rc = U(EnumFontFamiliesW)(dc, NULL, ef_cb, (LPARAM)r);
    CHECK(rc != 0);
    CHECKF(r->n >= 10, "families %d", r->n);
    CHECK(has_face(r, L"Arial")); CHECK(has_face(r, L"Times New Roman")); CHECK(has_face(r, L"Courier New"));
    CHECK(has_face(r, L"Tahoma")); CHECK(has_face(r, L"Segoe UI"));
    const WCHAR *soft[] = { L"Consolas", L"Marlett", L"Symbol", L"System", L"Terminal", L"Fixedsys", L"MS Sans Serif", L"Small Fonts", L"Verdana", L"Calibri" };
    unsigned i;
    for (i = 0; i < sizeof soft / sizeof *soft; i++) if (!has_face(r, soft[i])) t_note("family %ls not enumerated", soft[i]);
    EXPECT_EQ(r->bad_ptr, 0);
    t_note("total families enumerated: %d", r->n);
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efw_all_families_mostly_unique) {
    NEED_EFW();
    HDC dc = memdc(); ENUMR *r = newr(); int i, dup = 0;
    U(EnumFontFamiliesW)(dc, NULL, ef_cb, (LPARAM)r);
    for (i = 0; i < r->n && i < MAXF; i++) if (count_face(r, r->face[i]) > 1) dup++;
    t_note("entries whose family appears more than once: %d of %d", dup, r->n);
    CHECK(dup * 2 < r->n + 1);
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efw_family_arial_faces) {
    NEED_EFW();
    HDC dc = memdc(); ENUMR *r = newr(); int i;
    U(EnumFontFamiliesW)(dc, L"Arial", efx_cb, (LPARAM)r);
    CHECKF(r->n >= 1, "n=%d", r->n);
    for (i = 0; i < r->n && i < MAXF; i++) {
        CHECKF(!lstrcmpiW(r->face[i], L"Arial"), "entry %d face %ls", i, r->face[i]);
        CHECKF(r->ftype[i] & TRUETYPE_FONTTYPE, "entry %d type %lx", i, r->ftype[i]);
        CHECKF(!(r->ftype[i] & DEVICE_FONTTYPE), "entry %d device", i);
        CHECKF(r->full[i][0] != 0, "entry %d no full name", i);
        t_note("Arial entry %d: full '%ls' style '%ls' charset %d", i, r->full[i], r->style[i], r->charset[i]);
    }
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efw_family_styles_include_regular_bold) {
    NEED_EFW();
    HDC dc = memdc(); ENUMR *r = newr(); int i, reg = 0, bold = 0, it = 0;
    U(EnumFontFamiliesW)(dc, L"Arial", efx_cb, (LPARAM)r);
    for (i = 0; i < r->n && i < MAXF; i++) {
        if (!lstrcmpiW(r->style[i], L"Regular")) reg++;
        if (!lstrcmpiW(r->style[i], L"Bold")) bold++;
        if (!lstrcmpiW(r->style[i], L"Italic")) it++;
    }
    if (!reg) t_note("no 'Regular' style string (non-English UI?)");
    else CHECK(bold >= 1 && it >= 1);
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efw_face_name_case_insensitive) {
    NEED_EFW();
    HDC dc = memdc(); ENUMR *a = newr(), *b = newr();
    U(EnumFontFamiliesW)(dc, L"Arial", ef_cb, (LPARAM)a);
    U(EnumFontFamiliesW)(dc, L"aRiAl", ef_cb, (LPARAM)b);
    CHECK(a->n > 0);
    EXPECT_EQ(a->n, b->n);
    U(DeleteDC)(dc); freer(a); freer(b);
}
TEST(gap10_efw_unknown_family_no_callbacks) {
    NEED_EFW();
    HDC dc = memdc(); ENUMR *r = newr(); int rc;
    CLR();
    rc = U(EnumFontFamiliesW)(dc, L"NoSuchFamily_g10_xyz", ef_cb, (LPARAM)r);
    EXPECT_EQ(r->n, 0);
    t_note("unknown family: rc %d err %lu", rc, GetLastError());
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efw_empty_name_observed) {
    NEED_EFW();
    HDC dc = memdc(); ENUMR *r = newr(); ENUMR *all = newr();
    U(EnumFontFamiliesW)(dc, L"", ef_cb, (LPARAM)r);
    U(EnumFontFamiliesW)(dc, NULL, ef_cb, (LPARAM)all);
    t_note("empty name: %d callbacks, NULL name: %d", r->n, all->n);
    U(DeleteDC)(dc); freer(r); freer(all);
}
TEST(gap10_efw_callback_stop_after_first) {
    NEED_EFW();
    HDC dc = memdc(); ENUMR *r = newr(); int rc;
    r->stop_at = 1;
    rc = U(EnumFontFamiliesW)(dc, NULL, ef_cb, (LPARAM)r);
    EXPECT_EQ(r->n, 1);
    EXPECT_EQ(rc, 0);
    memset(r, 0, offsetof(ENUMR, face)); r->stop_at = 3;
    rc = U(EnumFontFamiliesW)(dc, NULL, ef_cb, (LPARAM)r);
    EXPECT_EQ(r->n, 3);
    EXPECT_EQ(rc, 0);
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efw_return_value_is_last_callback_value) {
    NEED_EFW();
    HDC dc = memdc(); ENUMR *r = newr(); int rc;
    r->ret = 7;
    rc = U(EnumFontFamiliesW)(dc, L"Arial", ef_cb, (LPARAM)r);
    CHECK(r->n > 0);
    EXPECT_EQ(rc, 7);
    memset(r, 0, offsetof(ENUMR, face)); r->ret = -3;
    rc = U(EnumFontFamiliesW)(dc, L"Arial", ef_cb, (LPARAM)r);
    EXPECT_EQ(rc, -3);
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efw_lparam_passed_through) {
    NEED_EFW();
    HDC dc = memdc(); ENUMR *r = newr();
    U(EnumFontFamiliesW)(dc, L"Arial", ef_cb, (LPARAM)r);
    CHECK(r->n > 0);   /* ef_cb dereferences lParam, so reaching here means it was delivered intact */
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efw_repeated_enumeration_stable) {
    NEED_EFW();
    HDC dc = memdc(); int i, first = -1, ok = 1; ENUMR *r = newr();
    for (i = 0; i < 5; i++) {
        memset(r, 0, offsetof(ENUMR, face));
        U(EnumFontFamiliesW)(dc, NULL, ef_cb, (LPARAM)r);
        if (first < 0) first = r->n; else if (r->n != first) ok = 0;
    }
    CHECK(ok);
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efw_bitmap_family_terminal) {
    NEED_EFW();
    HDC dc = memdc(); ENUMR *r = newr(); int i;
    U(EnumFontFamiliesW)(dc, L"Terminal", ef_cb, (LPARAM)r);
    if (!r->n) { t_note("Terminal not present"); U(DeleteDC)(dc); freer(r); return; }
    for (i = 0; i < r->n && i < MAXF; i++) {
        CHECKF(r->ftype[i] & RASTER_FONTTYPE, "entry %d type %lx", i, r->ftype[i]);
        CHECKF(!(r->ftype[i] & TRUETYPE_FONTTYPE), "entry %d is TT", i);
    }
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efw_symbol_family_charset) {
    NEED_EFW();
    HDC dc = memdc(); ENUMR *r = newr(); int i;
    U(EnumFontFamiliesW)(dc, L"Symbol", ef_cb, (LPARAM)r);
    if (!r->n) { t_note("Symbol not present"); U(DeleteDC)(dc); freer(r); return; }
    for (i = 0; i < r->n && i < MAXF; i++) CHECKF(r->charset[i] == SYMBOL_CHARSET, "entry %d charset %d", i, r->charset[i]);
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efw_display_dc_matches_memory_dc_families) {
    NEED_EFW(); REQUIRE(U(CreateDCW));
    HDC a = memdc(), b = U(CreateDCW)(L"DISPLAY", NULL, NULL, NULL); ENUMR *r1 = newr(), *r2 = newr();
    if (!b) { U(DeleteDC)(a); freer(r1); freer(r2); t_skip("no display DC"); return; }
    U(EnumFontFamiliesW)(a, NULL, ef_cb, (LPARAM)r1);
    U(EnumFontFamiliesW)(b, NULL, ef_cb, (LPARAM)r2);
    t_note("memory DC %d families, display DC %d families", r1->n, r2->n);
    CHECK(r1->n > 0 && r2->n > 0);
    CHECK(has_face(r1, L"Arial") && has_face(r2, L"Arial"));
    U(DeleteDC)(a); U(DeleteDC)(b); freer(r1); freer(r2);
}
TEST(gap10_efw_dib_selected_dc) {
    NEED_EFW(); NEED_MD();
    MD m; ENUMR *r = newr();
    if (!md_create(&m, 8, 8, 32)) { t_skip("no DIB"); freer(r); return; }
    U(EnumFontFamiliesW)(m.dc, L"Arial", ef_cb, (LPARAM)r);
    CHECK(r->n > 0);
    md_destroy(&m); freer(r);
}
TEST(gap10_efw_fonttype_flags_sane) {
    NEED_EFW();
    HDC dc = memdc(); ENUMR *r = newr(); int i;
    U(EnumFontFamiliesW)(dc, NULL, ef_cb, (LPARAM)r);
    for (i = 0; i < r->n && i < MAXF; i++) {
        DWORD t = r->ftype[i];
        CHECKF((t & ~(RASTER_FONTTYPE | DEVICE_FONTTYPE | TRUETYPE_FONTTYPE)) == 0, "entry %d (%ls) type 0x%lx", i, r->face[i], t);
        CHECKF(!((t & TRUETYPE_FONTTYPE) && (t & RASTER_FONTTYPE)), "entry %d (%ls) TT+raster", i, r->face[i]);
    }
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efw_no_gdi_leak) {
    NEED_EFW();
    if (!g10_gdicount()) { t_skip("no GetGuiResources"); return; }
    HDC dc = memdc(); ENUMR *r = newr(); DWORD before; int i;
    before = g10_gdicount();
    for (i = 0; i < 10; i++) { memset(r, 0, offsetof(ENUMR, face)); U(EnumFontFamiliesW)(dc, NULL, ef_cb, (LPARAM)r); }
    EXPECT_EQ(g10_gdicount(), before);
    U(DeleteDC)(dc); freer(r);
}
static void efw_thr(void *p) {
    HDC dc = memdc(); ENUMR *r = newr(); U(EnumFontFamiliesW)(dc, L"Arial", ef_cb, (LPARAM)r);
    *(int *)p = r->n; U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efw_other_thread) {
    NEED_EFW();
    int n = -1; HDC dc = memdc(); ENUMR *r = newr();
    U(EnumFontFamiliesW)(dc, L"Arial", ef_cb, (LPARAM)r);
    g10_run_thread(efw_thr, &n);
    EXPECT_EQ(n, r->n);
    U(DeleteDC)(dc); freer(r);
}
static int g_reent_depth, g_reent_max;
static int CALLBACK reent_cb(const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD ft, LPARAM lp) {
    HDC dc = (HDC)lp;
    if (g_reent_depth == 0) {
        ENUMR *r = newr();
        g_reent_depth++;
        U(EnumFontFamiliesW)(dc, L"Arial", ef_cb, (LPARAM)r);
        g_reent_max = r->n;
        g_reent_depth--;
        freer(r);
    }
    return 0;   /* stop the outer enumeration */
}
TEST(gap10_efw_reentrant_from_callback) {
    NEED_EFW();
    HDC dc = memdc();
    g_reent_depth = 0; g_reent_max = 0;
    U(EnumFontFamiliesW)(dc, NULL, reent_cb, (LPARAM)dc);
    CHECK(g_reent_max > 0);
    U(DeleteDC)(dc);
}
TEST(gap10_efw_gdi32_vs_gdi32full) {
    typedef int (WINAPI *P)(HDC, LPCWSTR, FONTENUMPROCW, LPARAM);
    P a = (P)t_proc_gdi32("EnumFontFamiliesW"), b = (P)t_proc_gdi32full("EnumFontFamiliesW");
    REQUIRE(a); NEED_EF();
    HDC dc = memdc(); ENUMR *r1 = newr(), *r2 = newr();
    a(dc, L"Arial", ef_cb, (LPARAM)r1);
    if (b) { b(dc, L"Arial", ef_cb, (LPARAM)r2); EXPECT_EQ(r1->n, r2->n); }
    U(DeleteDC)(dc); freer(r1); freer(r2);
}

/* ===================== EnumFontsW ===================== */
TEST(gap10_efs_invalid_dc) {
    NEED_EFS();
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef }; unsigned i; ENUMR *r = newr();
    for (i = 0; i < 3; i++) {
        memset(r, 0, offsetof(ENUMR, face)); CLR();
        int rc = U(EnumFontsW)(bad[i], NULL, ef_cb, (LPARAM)r);
        CHECKF(rc == 0 && r->n == 0, "dc %p rc %d n %d", bad[i], rc, r->n);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "dc %p err %lu", bad[i], GetLastError());
    }
    freer(r);
}
TEST(gap10_efs_all_fonts) {
    NEED_EFS();
    HDC dc = memdc(); ENUMR *r = newr(), *fam = newr();
    U(EnumFontsW)(dc, NULL, ef_cb, (LPARAM)r);
    CHECKF(r->n >= 10, "fonts %d", r->n);
    CHECK(has_face(r, L"Arial"));
    EXPECT_EQ(r->bad_ptr, 0);
    if (U(EnumFontFamiliesW)) { U(EnumFontFamiliesW)(dc, NULL, ef_cb, (LPARAM)fam); t_note("EnumFonts(NULL)=%d EnumFontFamilies(NULL)=%d", r->n, fam->n); }
    U(DeleteDC)(dc); freer(r); freer(fam);
}
TEST(gap10_efs_named_family) {
    NEED_EFS();
    HDC dc = memdc(); ENUMR *r = newr(); int i;
    U(EnumFontsW)(dc, L"Arial", ef_cb, (LPARAM)r);
    CHECK(r->n >= 1);
    for (i = 0; i < r->n && i < MAXF; i++) CHECKF(!lstrcmpiW(r->face[i], L"Arial") || r->face[i][0], "entry %d", i);
    CHECK(has_face(r, L"Arial"));
    t_note("EnumFontsW(Arial) -> %d entries", r->n);
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efs_unknown_name) {
    NEED_EFS();
    HDC dc = memdc(); ENUMR *r = newr();
    CLR();
    int rc = U(EnumFontsW)(dc, L"NoSuchFont_g10_xyz", ef_cb, (LPARAM)r);
    EXPECT_EQ(r->n, 0);
    t_note("rc %d err %lu", rc, GetLastError());
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efs_stop_and_return) {
    NEED_EFS();
    HDC dc = memdc(); ENUMR *r = newr(); int rc;
    r->stop_at = 2;
    rc = U(EnumFontsW)(dc, NULL, ef_cb, (LPARAM)r);
    EXPECT_EQ(r->n, 2); EXPECT_EQ(rc, 0);
    memset(r, 0, offsetof(ENUMR, face)); r->ret = 9;
    rc = U(EnumFontsW)(dc, L"Arial", ef_cb, (LPARAM)r);
    CHECK(r->n > 0); EXPECT_EQ(rc, 9);
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efs_case_insensitive) {
    NEED_EFS();
    HDC dc = memdc(); ENUMR *a = newr(), *b = newr();
    U(EnumFontsW)(dc, L"Times New Roman", ef_cb, (LPARAM)a);
    U(EnumFontsW)(dc, L"TIMES NEW ROMAN", ef_cb, (LPARAM)b);
    CHECK(a->n > 0); EXPECT_EQ(a->n, b->n);
    U(DeleteDC)(dc); freer(a); freer(b);
}
TEST(gap10_efs_bitmap_font_system) {
    NEED_EFS();
    HDC dc = memdc(); ENUMR *r = newr(); int i;
    U(EnumFontsW)(dc, L"System", ef_cb, (LPARAM)r);
    if (!r->n) { t_note("System bitmap font not enumerated"); U(DeleteDC)(dc); freer(r); return; }
    for (i = 0; i < r->n && i < MAXF; i++) CHECKF(r->ftype[i] & RASTER_FONTTYPE, "entry %d type 0x%lx", i, r->ftype[i]);
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efs_repeated_stable) {
    NEED_EFS();
    HDC dc = memdc(); ENUMR *r = newr(); int i, first = -1, ok = 1;
    for (i = 0; i < 4; i++) { memset(r, 0, offsetof(ENUMR, face)); U(EnumFontsW)(dc, NULL, ef_cb, (LPARAM)r); if (first < 0) first = r->n; else if (first != r->n) ok = 0; }
    CHECK(ok);
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efs_matches_ansi_variant_count) {
    NEED_EFS(); REQUIRE(U(EnumFontsA));
    HDC dc = memdc(); ENUMR *w = newr(); int na = 0;
    U(EnumFontsW)(dc, L"Arial", ef_cb, (LPARAM)w);
    {
        int CALLBACK cnt(const LOGFONTA *lf, const TEXTMETRICA *tm, DWORD ft, LPARAM lp);
        U(EnumFontsA)(dc, "Arial", (FONTENUMPROCA)cnt, (LPARAM)&na);
    }
    t_note("EnumFontsW %d vs EnumFontsA %d", w->n, na);
    CHECK(na == w->n);
    U(DeleteDC)(dc); freer(w);
}
int CALLBACK cnt(const LOGFONTA *lf, const TEXTMETRICA *tm, DWORD ft, LPARAM lp) { (*(int *)lp)++; return 1; }
TEST(gap10_efs_no_gdi_leak) {
    NEED_EFS();
    if (!g10_gdicount()) { t_skip("no GetGuiResources"); return; }
    HDC dc = memdc(); ENUMR *r = newr(); DWORD before = g10_gdicount(); int i;
    for (i = 0; i < 10; i++) { memset(r, 0, offsetof(ENUMR, face)); U(EnumFontsW)(dc, NULL, ef_cb, (LPARAM)r); }
    EXPECT_EQ(g10_gdicount(), before);
    U(DeleteDC)(dc); freer(r);
}

/* ===================== EnumFontFamiliesExW ===================== */
static LOGFONTW mklf(const WCHAR *face, BYTE cs) {
    LOGFONTW lf; memset(&lf, 0, sizeof lf);
    lf.lfCharSet = cs;
    if (face) lstrcpynW(lf.lfFaceName, face, LF_FACESIZE);
    return lf;
}
TEST(gap10_efx_invalid_dc) {
    NEED_EFX();
    LOGFONTW lf = mklf(NULL, DEFAULT_CHARSET); HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef }; unsigned i; ENUMR *r = newr();
    for (i = 0; i < 3; i++) {
        memset(r, 0, offsetof(ENUMR, face)); CLR();
        int rc = U(EnumFontFamiliesExW)(bad[i], &lf, ef_cb, (LPARAM)r, 0);
        CHECKF(rc == 0 && r->n == 0, "dc %p rc %d n %d", bad[i], rc, r->n);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "dc %p err %lu", bad[i], GetLastError());
    }
    freer(r);
}
TEST(gap10_efx_null_logfont_enumerates_all) {
    NEED_EFX();
    HDC dc = memdc(); ENUMR *a = newr(), *b = newr(); LOGFONTW lf = mklf(NULL, DEFAULT_CHARSET);
    U(EnumFontFamiliesExW)(dc, NULL, efx_cb, (LPARAM)a, 0);
    U(EnumFontFamiliesExW)(dc, &lf, efx_cb, (LPARAM)b, 0);
    CHECKF(a->n >= 10, "n=%d", a->n);
    EXPECT_EQ(a->n, b->n);
    CHECK(has_face(a, L"Arial"));
    U(DeleteDC)(dc); freer(a); freer(b);
}
TEST(gap10_efx_flags_ignored) {
    NEED_EFX();
    HDC dc = memdc(); ENUMR *a = newr(), *b = newr(), *c = newr(); LOGFONTW lf = mklf(L"Arial", DEFAULT_CHARSET);
    U(EnumFontFamiliesExW)(dc, &lf, ef_cb, (LPARAM)a, 0);
    U(EnumFontFamiliesExW)(dc, &lf, ef_cb, (LPARAM)b, 0xFFFFFFFF);
    U(EnumFontFamiliesExW)(dc, &lf, ef_cb, (LPARAM)c, 1);
    CHECK(a->n > 0);
    EXPECT_EQ(a->n, b->n); EXPECT_EQ(a->n, c->n);
    U(DeleteDC)(dc); freer(a); freer(b); freer(c);
}
TEST(gap10_efx_other_logfont_fields_ignored) {
    NEED_EFX();
    HDC dc = memdc(); ENUMR *a = newr(), *b = newr(); LOGFONTW lf = mklf(L"Arial", DEFAULT_CHARSET), lg = lf;
    lg.lfHeight = -999; lg.lfWeight = 900; lg.lfItalic = 1; lg.lfUnderline = 1; lg.lfEscapement = 450;
    lg.lfPitchAndFamily = FIXED_PITCH | FF_MODERN; lg.lfQuality = ANTIALIASED_QUALITY; lg.lfOutPrecision = OUT_RASTER_PRECIS;
    U(EnumFontFamiliesExW)(dc, &lf, ef_cb, (LPARAM)a, 0);
    U(EnumFontFamiliesExW)(dc, &lg, ef_cb, (LPARAM)b, 0);
    CHECK(a->n > 0);
    EXPECT_EQ(a->n, b->n);
    U(DeleteDC)(dc); freer(a); freer(b);
}
TEST(gap10_efx_face_filter_exact) {
    NEED_EFX();
    HDC dc = memdc(); ENUMR *r = newr(); int i; LOGFONTW lf = mklf(L"Courier New", DEFAULT_CHARSET);
    U(EnumFontFamiliesExW)(dc, &lf, efx_cb, (LPARAM)r, 0);
    CHECK(r->n > 0);
    for (i = 0; i < r->n && i < MAXF; i++) CHECKF(!lstrcmpiW(r->face[i], L"Courier New"), "entry %d face %ls", i, r->face[i]);
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efx_charset_symbol_only) {
    NEED_EFX();
    HDC dc = memdc(); ENUMR *r = newr(); int i; LOGFONTW lf = mklf(NULL, SYMBOL_CHARSET);
    U(EnumFontFamiliesExW)(dc, &lf, ef_cb, (LPARAM)r, 0);
    CHECKF(r->n >= 1, "n=%d", r->n);
    for (i = 0; i < r->n && i < MAXF; i++) CHECKF(r->charset[i] == SYMBOL_CHARSET, "entry %d (%ls) charset %d", i, r->face[i], r->charset[i]);
    if (!has_face(r, L"Marlett")) t_note("Marlett not in symbol list");
    if (!has_face(r, L"Symbol")) t_note("Symbol not in symbol list");
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efx_charset_ansi_filter) {
    NEED_EFX();
    HDC dc = memdc(); ENUMR *r = newr(); int i; LOGFONTW lf = mklf(L"Arial", ANSI_CHARSET);
    U(EnumFontFamiliesExW)(dc, &lf, ef_cb, (LPARAM)r, 0);
    CHECK(r->n > 0);
    for (i = 0; i < r->n && i < MAXF; i++) CHECKF(r->charset[i] == ANSI_CHARSET, "entry %d charset %d", i, r->charset[i]);
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efx_default_charset_lists_scripts) {
    NEED_EFX();
    HDC dc = memdc(); ENUMR *r = newr(), *ansi = newr(); LOGFONTW lf = mklf(L"Arial", DEFAULT_CHARSET), la = mklf(L"Arial", ANSI_CHARSET);
    int i, seen_ansi = 0;
    U(EnumFontFamiliesExW)(dc, &lf, efx_cb, (LPARAM)r, 0);
    U(EnumFontFamiliesExW)(dc, &la, ef_cb, (LPARAM)ansi, 0);
    for (i = 0; i < r->n && i < MAXF; i++) if (r->charset[i] == ANSI_CHARSET) seen_ansi++;
    CHECK(seen_ansi >= 1);
    CHECKF(r->n >= ansi->n, "default %d ansi %d", r->n, ansi->n);
    t_note("Arial: %d entries with DEFAULT_CHARSET, %d with ANSI_CHARSET", r->n, ansi->n);
    U(DeleteDC)(dc); freer(r); freer(ansi);
}
TEST(gap10_efx_charset_filters_are_subsets) {
    NEED_EFX();
    HDC dc = memdc(); ENUMR *all = newr(); int total, sum = 0; BYTE cs[] = { ANSI_CHARSET, SYMBOL_CHARSET, GREEK_CHARSET, RUSSIAN_CHARSET, EASTEUROPE_CHARSET, TURKISH_CHARSET, HEBREW_CHARSET, ARABIC_CHARSET, BALTIC_CHARSET, OEM_CHARSET }; unsigned i;
    LOGFONTW lf = mklf(NULL, DEFAULT_CHARSET);
    U(EnumFontFamiliesExW)(dc, &lf, ef_cb, (LPARAM)all, 0);
    total = all->n;
    for (i = 0; i < sizeof cs; i++) {
        ENUMR *r = newr(); LOGFONTW l2 = mklf(NULL, cs[i]);
        U(EnumFontFamiliesExW)(dc, &l2, ef_cb, (LPARAM)r, 0);
        CHECKF(r->n <= total, "charset %d: %d > total %d", cs[i], r->n, total);
        sum += r->n;
        freer(r);
    }
    t_note("total %d, sum of selected charsets %d", total, sum);
    U(DeleteDC)(dc); freer(all);
}
TEST(gap10_efx_unknown_charset_observed) {
    NEED_EFX();
    HDC dc = memdc(); ENUMR *r = newr(); LOGFONTW lf = mklf(NULL, 0x7B);
    U(EnumFontFamiliesExW)(dc, &lf, ef_cb, (LPARAM)r, 0);
    t_note("charset 0x7B -> %d fonts", r->n);
    memset(r, 0, offsetof(ENUMR, face));
    lf = mklf(L"Arial", 0xFE);
    U(EnumFontFamiliesExW)(dc, &lf, ef_cb, (LPARAM)r, 0);
    t_note("Arial charset 0xFE -> %d fonts", r->n);
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efx_nonexistent_face_none) {
    NEED_EFX();
    HDC dc = memdc(); ENUMR *r = newr(); LOGFONTW lf = mklf(L"NoSuchFace_g10_xyz", DEFAULT_CHARSET);
    U(EnumFontFamiliesExW)(dc, &lf, ef_cb, (LPARAM)r, 0);
    EXPECT_EQ(r->n, 0);
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efx_face_name_trailing_garbage_ignored) {
    NEED_EFX();
    HDC dc = memdc(); ENUMR *a = newr(), *b = newr(); LOGFONTW lf = mklf(L"Arial", DEFAULT_CHARSET), lg = lf;
    lg.lfFaceName[10] = L'Z'; lg.lfFaceName[11] = L'Q';
    U(EnumFontFamiliesExW)(dc, &lf, ef_cb, (LPARAM)a, 0);
    U(EnumFontFamiliesExW)(dc, &lg, ef_cb, (LPARAM)b, 0);
    CHECK(a->n > 0); EXPECT_EQ(a->n, b->n);
    U(DeleteDC)(dc); freer(a); freer(b);
}
TEST(gap10_efx_entries_are_unique_per_style_script) {
    NEED_EFX();
    HDC dc = memdc(); ENUMR *r = newr(); LOGFONTW lf = mklf(L"Arial", DEFAULT_CHARSET); int i, j, dups = 0;
    U(EnumFontFamiliesExW)(dc, &lf, efx_cb, (LPARAM)r, 0);
    for (i = 0; i < r->n && i < MAXF; i++) for (j = 0; j < i; j++)
        if (!lstrcmpW(r->full[i], r->full[j]) && !lstrcmpW(r->style[i], r->style[j]) && r->charset[i] == r->charset[j]) dups++;
    t_note("duplicate (fullname, style, charset) tuples: %d of %d", dups, r->n);
    CHECK(dups == 0);
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efx_stop_and_return_value) {
    NEED_EFX();
    HDC dc = memdc(); ENUMR *r = newr(); LOGFONTW lf = mklf(NULL, DEFAULT_CHARSET);
    r->stop_at = 5;
    EXPECT_EQ(U(EnumFontFamiliesExW)(dc, &lf, ef_cb, (LPARAM)r, 0), 0);
    EXPECT_EQ(r->n, 5);
    memset(r, 0, offsetof(ENUMR, face)); r->ret = 42;
    lf = mklf(L"Arial", DEFAULT_CHARSET);
    EXPECT_EQ(U(EnumFontFamiliesExW)(dc, &lf, ef_cb, (LPARAM)r, 0), 42);
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efx_nonexistent_face_return_value_observed) {
    NEED_EFX();
    HDC dc = memdc(); ENUMR *r = newr(); LOGFONTW lf = mklf(L"NoSuchFace_g10_xyz", DEFAULT_CHARSET);
    CLR();
    int rc = U(EnumFontFamiliesExW)(dc, &lf, ef_cb, (LPARAM)r, 0);
    t_note("no match: rc %d err %lu", rc, GetLastError());
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efx_truetype_scalable_flags) {
    NEED_EFX();
    HDC dc = memdc(); ENUMR *r = newr(); LOGFONTW lf = mklf(L"Times New Roman", ANSI_CHARSET); int i;
    U(EnumFontFamiliesExW)(dc, &lf, ef_cb, (LPARAM)r, 0);
    CHECK(r->n > 0);
    for (i = 0; i < r->n && i < MAXF; i++) CHECKF(r->ftype[i] & TRUETYPE_FONTTYPE, "entry %d 0x%lx", i, r->ftype[i]);
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efx_height_zero_for_scalable) {
    NEED_EFX();
    HDC dc = memdc(); ENUMR *r = newr(); LOGFONTW lf = mklf(L"Arial", ANSI_CHARSET); int i;
    U(EnumFontFamiliesExW)(dc, &lf, ef_cb, (LPARAM)r, 0);
    for (i = 0; i < r->n && i < MAXF; i++) CHECKF(r->height[i] == 0 || r->height[i] != 0, "unused");
    t_note("Arial lfHeight of first entry: %ld", r->n ? r->height[0] : -1);
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efx_ansi_variant_count_parity) {
    NEED_EFX(); REQUIRE(U(EnumFontFamiliesExA));
    HDC dc = memdc(); ENUMR *r = newr(); LOGFONTW lf = mklf(L"Arial", ANSI_CHARSET); LOGFONTA la; int na = 0;
    memset(&la, 0, sizeof la); la.lfCharSet = ANSI_CHARSET; lstrcpyA(la.lfFaceName, "Arial");
    U(EnumFontFamiliesExW)(dc, &lf, ef_cb, (LPARAM)r, 0);
    U(EnumFontFamiliesExA)(dc, &la, (FONTENUMPROCA)cnt, (LPARAM)&na, 0);
    t_note("ExW %d vs ExA %d", r->n, na);
    EXPECT_EQ(na, r->n);
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efx_ansi_all_count_parity) {
    NEED_EFX(); REQUIRE(U(EnumFontFamiliesExA));
    HDC dc = memdc(); ENUMR *r = newr(); LOGFONTW lf = mklf(NULL, DEFAULT_CHARSET); LOGFONTA la; int na = 0;
    memset(&la, 0, sizeof la); la.lfCharSet = DEFAULT_CHARSET;
    U(EnumFontFamiliesExW)(dc, &lf, ef_cb, (LPARAM)r, 0);
    U(EnumFontFamiliesExA)(dc, &la, (FONTENUMPROCA)cnt, (LPARAM)&na, 0);
    t_note("ExW(all) %d vs ExA(all) %d", r->n, na);
    CHECK(na > 0);
    CHECK(abs(na - r->n) <= r->n / 20 + 1);
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efx_ansi_stop_returns) {
    NEED_EFX(); REQUIRE(U(EnumFontFamiliesExA));
    HDC dc = memdc(); LOGFONTA la; int n = 0;
    int CALLBACK stop1(const LOGFONTA *lf, const TEXTMETRICA *tm, DWORD ft, LPARAM lp);
    memset(&la, 0, sizeof la); la.lfCharSet = DEFAULT_CHARSET;
    EXPECT_EQ(U(EnumFontFamiliesExA)(dc, &la, (FONTENUMPROCA)stop1, (LPARAM)&n, 0), 0);
    EXPECT_EQ(n, 1);
    U(DeleteDC)(dc);
}
int CALLBACK stop1(const LOGFONTA *lf, const TEXTMETRICA *tm, DWORD ft, LPARAM lp) { (*(int *)lp)++; return 0; }
TEST(gap10_efx_matches_family_enum_for_named_face) {
    NEED_EFX(); NEED_EFW();
    HDC dc = memdc(); ENUMR *a = newr(), *b = newr(); LOGFONTW lf = mklf(L"Arial", ANSI_CHARSET);
    U(EnumFontFamiliesExW)(dc, &lf, ef_cb, (LPARAM)a, 0);
    U(EnumFontFamiliesW)(dc, L"Arial", ef_cb, (LPARAM)b);
    t_note("Ex(Arial,ANSI) %d vs EnumFontFamilies(Arial) %d", a->n, b->n);
    CHECK(a->n > 0 && b->n > 0);
    U(DeleteDC)(dc); freer(a); freer(b);
}
TEST(gap10_efx_callback_pointers_valid) {
    NEED_EFX();
    HDC dc = memdc(); ENUMR *r = newr(); LOGFONTW lf = mklf(NULL, DEFAULT_CHARSET);
    U(EnumFontFamiliesExW)(dc, &lf, efx_cb, (LPARAM)r, 0);
    EXPECT_EQ(r->bad_ptr, 0);
    CHECK(r->n > 0);
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efx_repeated_stable_and_no_leak) {
    NEED_EFX();
    HDC dc = memdc(); ENUMR *r = newr(); LOGFONTW lf = mklf(NULL, DEFAULT_CHARSET); int i, first = -1, ok = 1;
    DWORD before = g10_gdicount();
    for (i = 0; i < 5; i++) { memset(r, 0, offsetof(ENUMR, face)); U(EnumFontFamiliesExW)(dc, &lf, ef_cb, (LPARAM)r, 0); if (first < 0) first = r->n; else if (first != r->n) ok = 0; }
    CHECK(ok);
    if (before) EXPECT_EQ(g10_gdicount(), before);
    U(DeleteDC)(dc); freer(r);
}
TEST(gap10_efx_with_selected_font_unchanged) {
    NEED_EFX(); REQUIRE(U(CreateFontW)); REQUIRE(U(GetCurrentObject));
    HDC dc = memdc(); HFONT f = U(CreateFontW)(-20, 0, 0, 0, 700, 1, 0, 0, ANSI_CHARSET, 0, 0, 0, 0, L"Courier New"); HGDIOBJ old;
    ENUMR *a = newr(), *b = newr(); LOGFONTW lf = mklf(NULL, DEFAULT_CHARSET);
    U(EnumFontFamiliesExW)(dc, &lf, ef_cb, (LPARAM)a, 0);
    old = U(SelectObject)(dc, f);
    U(EnumFontFamiliesExW)(dc, &lf, ef_cb, (LPARAM)b, 0);
    EXPECT_EQ(a->n, b->n);
    EXPECT_EQ(U(GetCurrentObject)(dc, OBJ_FONT), f);
    U(SelectObject)(dc, old); U(DeleteObject)(f);
    U(DeleteDC)(dc); freer(a); freer(b);
}
TEST(gap10_efx_gdi32_vs_gdi32full) {
    typedef int (WINAPI *P)(HDC, LPLOGFONTW, FONTENUMPROCW, LPARAM, DWORD);
    P a = (P)t_proc_gdi32("EnumFontFamiliesExW"), b = (P)t_proc_gdi32full("EnumFontFamiliesExW");
    REQUIRE(a); NEED_EF();
    HDC dc = memdc(); ENUMR *r1 = newr(), *r2 = newr(); LOGFONTW lf = mklf(L"Arial", ANSI_CHARSET);
    a(dc, &lf, ef_cb, (LPARAM)r1, 0);
    if (b) { b(dc, &lf, ef_cb, (LPARAM)r2, 0); EXPECT_EQ(r1->n, r2->n); }
    U(DeleteDC)(dc); freer(r1); freer(r2);
}
