/* Shared static helpers for the color_icm area. */
#ifndef CIH_H
#define CIH_H
#include "t.h"
#include <stdlib.h>

#define CI_ERR_INVALID_COLORSPACE 2017
#define CI_ERR_PROFILE_NOT_FOUND  2016
#define CI_ERR_ICM_NOT_ENABLED    2018
#define CI_LCS_CALIBRATED_RGB     0u
#define CI_LCS_sRGB               0x73524742u
#define CI_LCS_WINDOWS            0x57696E20u
#define CI_LCS_LINK               0x4C494E4Bu
#define CI_LCSW_SIZE              588u
#define CI_LCSA_SIZE              328u
#define CI_STOCK_COLORSPACE       20

typedef char ci_assert_w[(sizeof(LOGCOLORSPACEW) == 588) ? 1 : -1];
typedef char ci_assert_a[(sizeof(LOGCOLORSPACEA) == 328) ? 1 : -1];

static int ci_wine(void) {
    HMODULE n = GetModuleHandleA("ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version") != NULL;
}
/* Wine stubs color spaces/ICM; skip those tests there unless CI_RUN_ALL is set. Never skips on Windows. */
static int ci_stubbed(void) { return ci_wine() && !getenv("CI_RUN_ALL"); }
#define CI_NOWINE() do { if (ci_stubbed()) { t_skip("wine stubs color spaces"); return; } } while (0)
/* For calls wine aborts on (unimplemented stubs): always skipped there. */
#define CI_NOWINE_ABORT() do { if (ci_wine()) { t_skip("unimplemented under wine"); return; } } while (0)
static int ci_gdi_count(void) {
    if (!U(GetGuiResources) || ci_wine()) return -1;
    return (int)U(GetGuiResources)(GetCurrentProcess(), 0 /* GR_GDIOBJECTS */);
}

static void ci_initw(LOGCOLORSPACEW *w, DWORD type, DWORD intent) {
    memset(w, 0, sizeof *w);
    w->lcsSignature = 0x50534F43; /* 'PSOC' */
    w->lcsVersion = 0x400;
    w->lcsSize = CI_LCSW_SIZE;
    w->lcsCSType = type;
    w->lcsIntent = intent;
}
static void ci_inita(LOGCOLORSPACEA *a, DWORD type, DWORD intent) {
    memset(a, 0, sizeof *a);
    a->lcsSignature = 0x50534F43;
    a->lcsVersion = 0x400;
    a->lcsSize = CI_LCSA_SIZE;
    a->lcsCSType = type;
    a->lcsIntent = intent;
}
static void ci_endpoints(CIEXYZTRIPLE *e) {
    e->ciexyzRed.ciexyzX = 0x28000000;   e->ciexyzRed.ciexyzY = 0x13000000;   e->ciexyzRed.ciexyzZ = 0x01000000;
    e->ciexyzGreen.ciexyzX = 0x12000000; e->ciexyzGreen.ciexyzY = 0x26000000; e->ciexyzGreen.ciexyzZ = 0x04000000;
    e->ciexyzBlue.ciexyzX = 0x0B000000;  e->ciexyzBlue.ciexyzY = 0x04000000;  e->ciexyzBlue.ciexyzZ = 0x2D000000;
}
/* Create a color space through the W entry point (NULL if unavailable). */
static HCOLORSPACE ci_mkw(DWORD type, DWORD intent) {
    LOGCOLORSPACEW w;
    if (!U(CreateColorSpaceW)) return NULL;
    ci_initw(&w, type, intent);
    if (type == CI_LCS_CALIBRATED_RGB) ci_endpoints(&w.lcsEndpoints);
    return U(CreateColorSpaceW)(&w);
}
static HCOLORSPACE ci_mka(DWORD type, DWORD intent) {
    LOGCOLORSPACEA a;
    if (!U(CreateColorSpaceA)) return NULL;
    ci_inita(&a, type, intent);
    if (type == CI_LCS_CALIBRATED_RGB) ci_endpoints(&a.lcsEndpoints);
    return U(CreateColorSpaceA)(&a);
}
static HCOLORSPACE ci_default_cs(void) {
    return U(GetStockObject) ? (HCOLORSPACE)U(GetStockObject)(CI_STOCK_COLORSPACE) : NULL;
}

/* 32bpp top-down DIB-section memory DC. */
typedef struct { HDC dc; HBITMAP bm, old; void *bits; int w, h; } ci_mem;
static int ci_mem_open(ci_mem *m, int w, int h) {
    BITMAPINFO bi;
    memset(m, 0, sizeof *m);
    if (!U(CreateCompatibleDC) || !U(CreateDIBSection) || !U(SelectObject)) return 0;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    m->dc = U(CreateCompatibleDC)(NULL);
    if (!m->dc) return 0;
    m->bm = U(CreateDIBSection)(m->dc, &bi, DIB_RGB_COLORS, &m->bits, NULL, 0);
    if (!m->bm) { U(DeleteDC)(m->dc); m->dc = NULL; return 0; }
    m->old = (HBITMAP)U(SelectObject)(m->dc, m->bm);
    m->w = w; m->h = h;
    return 1;
}
static void ci_mem_close(ci_mem *m) {
    if (!m->dc) return;
    U(SelectObject)(m->dc, m->old);
    U(DeleteObject)(m->bm);
    U(DeleteDC)(m->dc);
    m->dc = NULL;
}
static HDC ci_bogus_dc(void) { /* a DC handle that was valid and is now deleted */
    HDC d = U(CreateCompatibleDC)(NULL);
    U(DeleteDC)(d);
    return d;
}
#endif
