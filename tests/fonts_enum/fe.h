/* Shared helpers for the fonts_enum area (static; included by each file). */
#ifndef FE_H
#define FE_H
#include "t.h"

#define FE_BASE() do { \
    REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC)); REQUIRE(U(SelectObject)); \
    REQUIRE(U(DeleteObject)); REQUIRE(U(CreateFontIndirectA)); REQUIRE(U(GetTextFaceA)); \
    REQUIRE(U(GetTextMetricsA)); } while (0)

/* Single-evaluation equality check (arguments may have side effects). */
#define EQ(a, b) do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a), b_ = (unsigned long long)(ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", a_, b_); } while (0)

#ifndef GDI_ERROR
#define GDI_ERROR 0xFFFFFFFFu
#endif
#define FE_TAG(a, b, c, d) ((DWORD)(BYTE)(a) | ((DWORD)(BYTE)(b) << 8) | ((DWORD)(BYTE)(c) << 16) | ((DWORD)(BYTE)(d) << 24))

static int fe_wine(void) {
    return GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version") != NULL;
}

static unsigned fe_be16(const BYTE *p) { return (p[0] << 8) | p[1]; }
static unsigned fe_be32(const BYTE *p) { return ((unsigned)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

typedef struct { const char *name; int tt; int fixed; int fam; } fe_face_t;
/* fam: FF_* nibble expected, or -1 when not asserted */
static const fe_face_t fe_faces[] = {
    {"Arial", 1, 0, FF_SWISS}, {"Courier New", 1, 1, FF_MODERN}, {"Times New Roman", 1, 0, FF_ROMAN},
    {"Tahoma", 1, 0, -1}, {"Segoe UI", 1, 0, -1}, {"Consolas", 1, 1, -1}, {"Symbol", 1, 0, -1},
    {"Marlett", 1, -1, -1}, {"System", 0, 0, -1}, {"Terminal", 0, 1, -1}, {"Fixedsys", 0, 1, -1},
    {"MS Sans Serif", 0, 0, -1}, {"Small Fonts", 0, 0, -1},
};
#define FE_NFACES ((int)(sizeof fe_faces / sizeof fe_faces[0]))

static HFONT fe_mk(int h, int w, int weight, int italic, int cs, const char *face) {
    LOGFONTA lf;
    memset(&lf, 0, sizeof lf);
    lf.lfHeight = h; lf.lfWidth = w; lf.lfWeight = weight; lf.lfItalic = (BYTE)italic;
    lf.lfCharSet = (BYTE)cs; lf.lfOutPrecision = OUT_DEFAULT_PRECIS; lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY; lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    lstrcpynA(lf.lfFaceName, face, LF_FACESIZE);
    return U(CreateFontIndirectA)(&lf);
}

typedef struct { HDC dc; HFONT f; HGDIOBJ old; } fe_ctx;

/* Create DC + font, select it. Returns 1 when the face the mapper chose equals `face`. */
static int fe_open(fe_ctx *c, const char *face, int h, int weight, int italic, int cs) {
    char got[LF_FACESIZE + 8] = "";
    c->dc = U(CreateCompatibleDC)(NULL);
    c->f = fe_mk(h, 0, weight, italic, cs, face);
    c->old = (c->dc && c->f) ? U(SelectObject)(c->dc, c->f) : NULL;
    if (!c->old) return 0;
    U(GetTextFaceA)(c->dc, sizeof got, got);
    return !lstrcmpiA(got, face);
}
static void fe_close(fe_ctx *c) {
    if (c->dc && c->old) U(SelectObject)(c->dc, c->old);
    if (c->f) U(DeleteObject)(c->f);
    if (c->dc) U(DeleteDC)(c->dc);
    c->dc = NULL; c->f = NULL; c->old = NULL;
}

static int fe_dpi(void) {
    int dpi = 96;
    HDC dc = U(CreateCompatibleDC)(NULL);
    if (dc && U(GetDeviceCaps)) dpi = U(GetDeviceCaps)(dc, LOGPIXELSY);
    if (dc) U(DeleteDC)(dc);
    return dpi;
}

static DWORD fe_gdi_count(void) {
    return U(GetGuiResources) ? U(GetGuiResources)(GetCurrentProcess(), 0 /*GR_GDIOBJECTS*/) : 0;
}

/* ---- enumeration collector ---- */
typedef struct {
    char face[LF_FACESIZE]; char full[LF_FULLFACESIZE]; char style[LF_FACESIZE]; char script[LF_FACESIZE];
    BYTE cs, pf; LONG h; LONG weight; BYTE italic; DWORD ft; DWORD ntmFlags; UINT ntmSizeEM; BYTE tmcs;
    LONG tmHeight, tmAscent, tmDescent;
} fe_ent;
#define FE_MAXENT 4096
typedef struct { fe_ent *e; int n; int total; int stop_at; int ex; int ret; } fe_set;

static int CALLBACK fe_cbA(const LOGFONTA *lf, const TEXTMETRICA *tm, DWORD ft, LPARAM lp) {
    fe_set *s = (fe_set *)lp;
    s->total++;
    if (s->n < FE_MAXENT) {
        fe_ent *e = &s->e[s->n++];
        memset(e, 0, sizeof *e);
        lstrcpynA(e->face, lf->lfFaceName, LF_FACESIZE);
        e->cs = lf->lfCharSet; e->pf = lf->lfPitchAndFamily; e->h = lf->lfHeight;
        e->weight = lf->lfWeight; e->italic = lf->lfItalic; e->ft = ft;
        e->tmcs = tm->tmCharSet; e->tmHeight = tm->tmHeight; e->tmAscent = tm->tmAscent; e->tmDescent = tm->tmDescent;
        if (s->ex) {
            const ENUMLOGFONTEXA *x = (const ENUMLOGFONTEXA *)lf;
            lstrcpynA(e->full, (const char *)x->elfFullName, LF_FULLFACESIZE);
            lstrcpynA(e->style, (const char *)x->elfStyle, LF_FACESIZE);
            lstrcpynA(e->script, (const char *)x->elfScript, LF_FACESIZE);
        } else {
            const ENUMLOGFONTA *x = (const ENUMLOGFONTA *)lf;
            lstrcpynA(e->full, (const char *)x->elfFullName, LF_FULLFACESIZE);
            lstrcpynA(e->style, (const char *)x->elfStyle, LF_FACESIZE);
        }
        if (ft & TRUETYPE_FONTTYPE) {
            const NEWTEXTMETRICA *n = (const NEWTEXTMETRICA *)tm;
            e->ntmFlags = n->ntmFlags; e->ntmSizeEM = n->ntmSizeEM;
        }
    }
    return s->stop_at && s->total >= s->stop_at ? 0 : 1;
}
static int CALLBACK fe_cbW(const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD ft, LPARAM lp) {
    fe_set *s = (fe_set *)lp;
    s->total++;
    if (s->n < FE_MAXENT) {
        fe_ent *e = &s->e[s->n++];
        memset(e, 0, sizeof *e);
        WideCharToMultiByte(CP_UTF8, 0, lf->lfFaceName, -1, e->face, LF_FACESIZE, NULL, NULL);
        e->cs = lf->lfCharSet; e->pf = lf->lfPitchAndFamily; e->h = lf->lfHeight;
        e->weight = lf->lfWeight; e->italic = lf->lfItalic; e->ft = ft;
        e->tmcs = tm->tmCharSet; e->tmHeight = tm->tmHeight;
        if (s->ex) {
            const ENUMLOGFONTEXW *x = (const ENUMLOGFONTEXW *)lf;
            WideCharToMultiByte(CP_UTF8, 0, x->elfFullName, -1, e->full, LF_FULLFACESIZE, NULL, NULL);
            WideCharToMultiByte(CP_UTF8, 0, x->elfStyle, -1, e->style, LF_FACESIZE, NULL, NULL);
            WideCharToMultiByte(CP_UTF8, 0, x->elfScript, -1, e->script, LF_FACESIZE, NULL, NULL);
        }
        if (ft & TRUETYPE_FONTTYPE) {
            const NEWTEXTMETRICW *n = (const NEWTEXTMETRICW *)tm;
            e->ntmFlags = n->ntmFlags; e->ntmSizeEM = n->ntmSizeEM;
        }
    }
    return s->stop_at && s->total >= s->stop_at ? 0 : 1;
}

static fe_set *fe_newset(int ex, int stop_at) {
    static fe_set s;
    static fe_ent store[FE_MAXENT];
    memset(&s, 0, sizeof s);
    s.e = store; s.ex = ex; s.stop_at = stop_at;
    return &s;
}

/* EnumFontFamiliesExA with a face / charset filter */
static fe_set *fe_enum_exA(HDC dc, const char *face, int cs, int stop_at) {
    fe_set *s = fe_newset(1, stop_at);
    LOGFONTA lf; memset(&lf, 0, sizeof lf);
    lf.lfCharSet = (BYTE)cs;
    if (face) lstrcpynA(lf.lfFaceName, face, LF_FACESIZE);
    s->ret = U(EnumFontFamiliesExA)(dc, &lf, (FONTENUMPROCA)fe_cbA, (LPARAM)s, 0);
    return s;
}
static int fe_find(fe_set *s, const char *face) {
    for (int i = 0; i < s->n; i++) if (!lstrcmpiA(s->e[i].face, face)) return i;
    return -1;
}
static int fe_face_exists(const char *face) {
    HDC dc = U(CreateCompatibleDC)(NULL);
    int r = 0;
    if (dc && U(EnumFontFamiliesExA)) { fe_set *s = fe_enum_exA(dc, face, DEFAULT_CHARSET, 0); r = fe_find(s, face) >= 0; }
    if (dc) U(DeleteDC)(dc);
    return r;
}

/* path of a stock TrueType file in the Windows fonts directory */
static void fe_fontfile(char *out, size_t n, const char *leaf) {
    char w[MAX_PATH] = "C:\\Windows";
    GetWindowsDirectoryA(w, sizeof w);
    wsprintfA(out, "%s\\Fonts\\%s", w, leaf);
    (void)n;
}
static BYTE *fe_readfile(const char *path, DWORD *size) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    BYTE *buf = NULL; DWORD got = 0, sz;
    if (h == INVALID_HANDLE_VALUE) return NULL;
    sz = GetFileSize(h, NULL);
    if (sz != INVALID_FILE_SIZE && sz < 32u * 1024 * 1024) {
        buf = (BYTE *)HeapAlloc(GetProcessHeap(), 0, sz ? sz : 1);
        if (buf && !ReadFile(h, buf, sz, &got, NULL)) { HeapFree(GetProcessHeap(), 0, buf); buf = NULL; }
        if (buf && got != sz) { HeapFree(GetProcessHeap(), 0, buf); buf = NULL; }
    }
    CloseHandle(h);
    if (buf) *size = sz;
    return buf;
}
static void fe_free(void *p) { if (p) HeapFree(GetProcessHeap(), 0, p); }

static int fe_req_cs(const char *face) {
    if (!lstrcmpA(face, "Terminal")) return OEM_CHARSET;
    if (!lstrcmpA(face, "Symbol") || !lstrcmpA(face, "Marlett")) return SYMBOL_CHARSET;
    return ANSI_CHARSET;
}
#define FACE_TESTS(X) X(arial, "Arial") X(courier_new, "Courier New") X(times, "Times New Roman") X(tahoma, "Tahoma") \
    X(segoe_ui, "Segoe UI") X(consolas, "Consolas") X(symbol, "Symbol") X(marlett, "Marlett") X(system, "System") \
    X(terminal, "Terminal") X(fixedsys, "Fixedsys") X(ms_sans_serif, "MS Sans Serif") X(small_fonts, "Small Fonts")
#define TT_FACE_TESTS(X) X(arial, "Arial") X(courier_new, "Courier New") X(times, "Times New Roman") X(tahoma, "Tahoma") \
    X(segoe_ui, "Segoe UI") X(consolas, "Consolas") X(symbol, "Symbol") X(marlett, "Marlett")
#define DEF_FACE_TEST(prefix, worker, id, name) TEST(prefix##_##id) { FE_BASE(); worker(name); }
#endif
