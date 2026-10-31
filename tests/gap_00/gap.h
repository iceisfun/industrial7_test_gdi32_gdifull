/* helpers for the gap_00 area */
#ifndef GAP_H
#define GAP_H
#include "t.h"
#include <wingdi.h>

#ifndef GDI_ERROR
#define GDI_ERROR 0xFFFFFFFFUL
#endif

static int gap_is_wine(void) {
    static int v = -1;
    if (v < 0) v = GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version") != NULL;
    return v;
}
/* skip (no-op on Windows) for calls that abort or are unimplemented under wine */
#define SKIP_WINE(why) do { if (gap_is_wine()) { t_skip("wine: " why); return; } } while (0)

/* Observed-not-documented expectation: logged, never fails. */
#define SOFT(c, what) do { if (!(c)) t_note("soft: %s (%s:%d)", what, __FILE__, __LINE__); } while (0)

static DWORD gap_gdi_count(void) {
    DWORD (WINAPI *f)(HANDLE, DWORD) = (void *)t_proc("GetGuiResources");
    return f ? f(GetCurrentProcess(), 0) : 0;
}

/* 32bpp top-down DIB section selected into a fresh memory DC */
typedef struct { HDC dc; HBITMAP bm; HGDIOBJ obm; void *bits; int w, h; } gap_mdc;
static int gap_mdc_open(gap_mdc *m, int w, int h) {
    memset(m, 0, sizeof *m);
    HDC (WINAPI *ccdc)(HDC) = (void *)t_proc("CreateCompatibleDC");
    HBITMAP (WINAPI *cds)(HDC, const BITMAPINFO *, UINT, void **, HANDLE, DWORD) = (void *)t_proc("CreateDIBSection");
    HGDIOBJ (WINAPI *so)(HDC, HGDIOBJ) = (void *)t_proc("SelectObject");
    if (!ccdc || !cds || !so) return 0;
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    m->dc = ccdc(NULL);
    if (!m->dc) return 0;
    m->bm = cds(m->dc, &bi, DIB_RGB_COLORS, &m->bits, NULL, 0);
    if (!m->bm) { U(DeleteDC)(m->dc); m->dc = NULL; return 0; }
    m->obm = so(m->dc, m->bm);
    m->w = w; m->h = h;
    return 1;
}
static void gap_mdc_close(gap_mdc *m) {
    if (m->dc) {
        if (m->obm) U(SelectObject)(m->dc, m->obm);
        U(DeleteDC)(m->dc);
    }
    if (m->bm) U(DeleteObject)(m->bm);
    memset(m, 0, sizeof *m);
}

/* memory DC with a font selected */
typedef struct { gap_mdc m; HFONT f; HGDIOBJ of; int ok; WCHAR face[LF_FACESIZE]; } gap_fx;
static int gap_fx_open(gap_fx *x, const WCHAR *face, int height) {
    memset(x, 0, sizeof *x);
    if (!gap_mdc_open(&x->m, 64, 64)) return 0;
    LOGFONTW lf; memset(&lf, 0, sizeof lf);
    lf.lfHeight = height; lf.lfWeight = FW_NORMAL; lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = NONANTIALIASED_QUALITY;
    lstrcpynW(lf.lfFaceName, face, LF_FACESIZE);
    x->f = U(CreateFontIndirectW)(&lf);
    if (!x->f) return 0;
    x->of = U(SelectObject)(x->m.dc, x->f);
    U(GetTextFaceW)(x->m.dc, LF_FACESIZE, x->face);
    x->ok = !lstrcmpiW(x->face, face);
    return 1;
}
static void gap_fx_close(gap_fx *x) {
    if (x->m.dc && x->of) U(SelectObject)(x->m.dc, x->of);
    if (x->f) U(DeleteObject)(x->f);
    gap_mdc_close(&x->m);
    memset(x, 0, sizeof *x);
}
/* open Arial 40px (TrueType); skips test if Arial isn't what got realised */
#define FX_OPEN_OR_SKIP(x, face, h) do { \
    if (!gap_fx_open(&(x), (face), (h)) || !(x).ok) { gap_fx_close(&(x)); t_skip("font " #face " unavailable"); return; } } while (0)

#define TAG_HEAD 0x64616568UL  /* 'head' as read from a little-endian file DWORD */
#define TAG_CMAP 0x70616D63UL
#define TAG_NAME 0x656D616EUL

#endif
