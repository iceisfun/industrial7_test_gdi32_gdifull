/* Shared static helpers for the objects_stock area. */
#ifndef OS_H
#define OS_H
#include "t.h"
#include <limits.h>
#include <wchar.h>

#define GOA U(GetObjectA)
#define GOW U(GetObjectW)
#define GOT U(GetObjectType)
#define GSO U(GetStockObject)
#define DOBJ U(DeleteObject)
#define SOBJ U(SelectObject)

#define H_TYPE(h)  ((unsigned)(((ULONG_PTR)(h) >> 16) & 0x7f))
#define H_INDEX(h) ((unsigned)((ULONG_PTR)(h) & 0xffff))
#define H_STOCKBIT(h) ((unsigned)(((ULONG_PTR)(h) >> 23) & 1))

/* Handle type values stored in bits 16..22 of a GDI handle. */
#define HT_DC 0x01
#define HT_RGN 0x04
#define HT_BMP 0x05
#define HT_PAL 0x08
#define HT_CSP 0x09
#define HT_FONT 0x0a
#define HT_BRUSH 0x10
#define HT_ALTDC 0x21
#define HT_PEN 0x30
#define HT_EXTPEN 0x50

#ifndef OBJ_PEN
#define OBJ_PEN 1
#endif

static int os_wine(void) {
    HMODULE n = GetModuleHandleA("ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version") != NULL;
}

static DWORD os_gdicount(void) {
    DWORD (WINAPI *f)(HANDLE, DWORD) = (void *)t_proc("GetGuiResources");
    return f ? f(GetCurrentProcess(), 0 /* GR_GDIOBJECTS */) : 0xFFFFFFFFu;
}

/* Create a top-down DIB section of the given depth (8 bpp gets a grey palette). */
static HBITMAP os_dib(HDC dc, int w, int h, int bpp, void **bits) {
    HBITMAP (WINAPI *cds)(HDC, const BITMAPINFO *, UINT, VOID **, HANDLE, DWORD) = (void *)t_proc("CreateDIBSection");
    union { BITMAPINFO bi; char raw[sizeof(BITMAPINFOHEADER) + 256 * 4]; } u;
    void *b = NULL;
    memset(&u, 0, sizeof u);
    u.bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    u.bi.bmiHeader.biWidth = w;
    u.bi.bmiHeader.biHeight = -h;
    u.bi.bmiHeader.biPlanes = 1;
    u.bi.bmiHeader.biBitCount = (WORD)bpp;
    u.bi.bmiHeader.biCompression = BI_RGB;
    if (bpp == 8) {
        for (int i = 0; i < 256; i++) { u.bi.bmiColors[i].rgbRed = u.bi.bmiColors[i].rgbGreen = u.bi.bmiColors[i].rgbBlue = (BYTE)i; }
        u.bi.bmiHeader.biClrUsed = 256;
    }
    if (!cds) return NULL;
    HBITMAP r = cds(dc, &u.bi, DIB_RGB_COLORS, &b, NULL, 0);
    if (bits) *bits = b;
    return r;
}

static HPALETTE os_pal(int n) {
    HPALETTE (WINAPI *cp)(const LOGPALETTE *) = (void *)t_proc("CreatePalette");
    struct { WORD ver, n; PALETTEENTRY e[256]; } lp;
    if (!cp || n < 0 || n > 256) return NULL;
    memset(&lp, 0, sizeof lp);
    lp.ver = 0x300; lp.n = (WORD)n;
    for (int i = 0; i < n; i++) { lp.e[i].peRed = (BYTE)i; lp.e[i].peGreen = (BYTE)(255 - i); lp.e[i].peBlue = (BYTE)(i * 3); }
    return cp((const LOGPALETTE *)&lp);
}

static HRGN os_rgn(void) {
    HRGN (WINAPI *f)(int, int, int, int) = (void *)t_proc("CreateRectRgn");
    return f ? f(1, 2, 11, 12) : NULL;
}

static HFONT os_font(const char *face, int h) {
    HFONT (WINAPI *f)(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCSTR) = (void *)t_proc("CreateFontA");
    return f ? f(h, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, face) : NULL;
}

static HDC os_memdc(void) {
    HDC (WINAPI *f)(HDC) = (void *)t_proc("CreateCompatibleDC");
    return f ? f(NULL) : NULL;
}

/* One object of each creatable kind. kind: see OS_K_* */
enum { OS_K_PEN, OS_K_EXTPEN, OS_K_SOLID, OS_K_HATCH, OS_K_PATTERN, OS_K_NULLBR, OS_K_FONT, OS_K_PAL, OS_K_RGN,
       OS_K_DDB, OS_K_DIB, OS_K_MEMDC, OS_K_COUNT };
static const char *const os_kname[OS_K_COUNT] = { "pen", "extpen", "solidbrush", "hatchbrush", "patternbrush", "nullbrush",
    "font", "palette", "region", "ddb", "dibsection", "memdc" };

static HGDIOBJ os_make(int kind) {
    switch (kind) {
    case OS_K_PEN: return U(CreatePen) ? U(CreatePen)(PS_SOLID, 2, RGB(1, 2, 3)) : NULL;
    case OS_K_EXTPEN: {
        LOGBRUSH lb = { BS_SOLID, RGB(9, 8, 7), 0 };
        return U(ExtCreatePen) ? U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID, 3, &lb, 0, NULL) : NULL;
    }
    case OS_K_SOLID: return U(CreateSolidBrush) ? U(CreateSolidBrush)(RGB(10, 20, 30)) : NULL;
    case OS_K_HATCH: return U(CreateHatchBrush) ? U(CreateHatchBrush)(HS_CROSS, RGB(10, 20, 30)) : NULL;
    case OS_K_PATTERN: {
        HBITMAP b = U(CreateBitmap) ? U(CreateBitmap)(8, 8, 1, 1, NULL) : NULL;
        HGDIOBJ r = NULL;
        if (b && U(CreatePatternBrush)) r = U(CreatePatternBrush)(b);
        if (b && U(DeleteObject)) U(DeleteObject)(b);
        return r;
    }
    case OS_K_NULLBR: {
        LOGBRUSH lb = { BS_NULL, 0, 0 };
        return U(CreateBrushIndirect) ? U(CreateBrushIndirect)(&lb) : NULL;
    }
    case OS_K_FONT: return os_font("Arial", -12);
    case OS_K_PAL: return os_pal(8);
    case OS_K_RGN: return os_rgn();
    case OS_K_DDB: return U(CreateBitmap) ? U(CreateBitmap)(5, 4, 1, 1, NULL) : NULL;
    case OS_K_DIB: return os_dib(NULL, 4, 4, 32, NULL);
    case OS_K_MEMDC: return os_memdc();
    }
    return NULL;
}
static BOOL os_free(int kind, HGDIOBJ h) {
    if (!h) return TRUE;
    if (kind == OS_K_MEMDC) return U(DeleteDC)(h);
    return U(DeleteObject)(h);
}
/* Expected GetObjectType per kind. */
static const DWORD os_ktype[OS_K_COUNT] = { OBJ_PEN, OBJ_EXTPEN, OBJ_BRUSH, OBJ_BRUSH, OBJ_BRUSH, OBJ_BRUSH, OBJ_FONT,
    OBJ_PAL, OBJ_REGION, OBJ_BITMAP, OBJ_BITMAP, OBJ_MEMDC };
static const unsigned os_khtype[OS_K_COUNT] = { HT_PEN, HT_EXTPEN, HT_BRUSH, HT_BRUSH, HT_BRUSH, HT_BRUSH, HT_FONT,
    HT_PAL, HT_RGN, HT_BMP, HT_BMP, HT_DC };

#endif
