#ifndef GAP5_H
#define GAP5_H
#include "t.h"

/* Undocumented / internal gdi32 + gdi32full entry points. All are resolved
 * through GetProcAddress (gdi32 first, then gdi32full). */
UDECL(void,   GdiSetLastError, (DWORD));
UDECL(DWORD,  GdiSupportsFontChangeEvent, (void));
UDECL(DWORD,  GdiReleaseLocalDC, (HDC));
UDECL(DWORD,  GdiSetAttrs, (HDC));
UDECL(ULONG_PTR, GdiPlayJournal, (HDC, LPCWSTR, DWORD, DWORD, int));
UDECL(ULONG_PTR, GdiPlayDCScript, (void *, void *, void *, void *, void *, void *));
UDECL(ULONG_PTR, GdiPlayScript, (void *, void *, void *, void *, void *, void *));
UDECL(ULONG_PTR, gdiPlaySpoolStream, (void *, void *, void *, void *, void *, void *));
UDECL(ULONG_PTR, GditGetCallerTLStorage, (void *, void *, ULONG, ULONGLONG));
UDECL(ULONG_PTR, GditPushCallerInfo, (void *, void *, ULONG, ULONGLONG));
UDECL(void,   GditPopCallerInfo, (void *, void *, ULONG, ULONGLONG));
UDECL(void *, GdiTrackHCreate, (void *));
UDECL(void,   GdiTrackHDelete, (void *));
UDECL(void,   GdiSetServerAttr, (void *, void *));
UDECL(DWORD,  GdiValidateHandle, (HGDIOBJ));
UDECL(DWORD,  GdiReleaseDC, (HDC));
UDECL(void *, GdiQueryTable, (void));
UDECL(DWORD,  GdiIsUMPDSandboxingEnabled, (void));
UDECL(void,   GdiLoadType1Fonts, (void));
UDECL(void,   GdiQueryFonts, (void));
UDECL(BOOL,   GdiSetPixelFormat, (HDC, int));
UDECL(BOOL,   GdiSwapBuffers, (HDC));
UDECL(ULONG_PTR, GdiPlayEMF, (LPCWSTR, void *, LPCWSTR, void *, void *));
UDECL(ULONG_PTR, GdiPlayPageEMF, (void *, void *, void *, void *, void *));
UDECL(void,   GdiPlayPrivatePageEMF, (void *, void *, void *));
UDECL(BOOLEAN, GdiStartPageEMF, (void *));
UDECL(DWORD,  GdiStartDocEMF, (void *, void *));
UDECL(DWORD,  GdiResetDCEMF, (void *, void *));
UDECL(BOOL,   LpkInitialize, (void));
UDECL(void *, LpkGetEditControl, (void));
UDECL(BOOL,   LpkpInitializeEditControl, (void *, int));
UDECL(BOOL,   LpkExtTextOut, (HDC, int, int, UINT, const RECT *, LPCWSTR, UINT, const INT *, int));
UDECL(BOOL,   LpkGetTextExtentExPoint, (HDC, LPCWSTR, int, int, int *, int *, SIZE *, DWORD, int));
UDECL(ULONG_PTR, LpkGetCharacterPlacement, (HDC, LPCWSTR, UINT, int, GCP_RESULTSW *, DWORD, int));

#define G(n) ((n##_pfn)t_proc(#n))

static inline int g5_is_wine(void) {
    HMODULE nt = GetModuleHandleA("ntdll.dll");
    return nt && GetProcAddress(nt, "wine_get_version") != NULL;
}
static inline DWORD g5_rtl_lasterr(void) {
    ULONG (WINAPI *f)(void) = (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetLastWin32Error");
    return f ? f() : GetLastError();
}
static inline DWORD g5_gdi_count(void) {
    DWORD (WINAPI *f)(HANDLE, DWORD) = (void *)t_proc("GetGuiResources");
    return f ? f(GetCurrentProcess(), 0) : 0;
}
/* 32bpp top-down DIB section selected into a memory DC. */
typedef struct { HDC dc; HBITMAP bmp, old; DWORD *bits; int w, h; } g5_mem;
static inline int g5_mem_create(g5_mem *m, int w, int h) {
    HDC (WINAPI *ccdc)(HDC) = (void *)t_proc("CreateCompatibleDC");
    HBITMAP (WINAPI *cds)(HDC, const BITMAPINFO *, UINT, void **, HANDLE, DWORD) = (void *)t_proc("CreateDIBSection");
    HGDIOBJ (WINAPI *so)(HDC, HGDIOBJ) = (void *)t_proc("SelectObject");
    BITMAPINFO bi; void *bits = NULL;
    memset(m, 0, sizeof *m);
    if (!ccdc || !cds || !so) return 0;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    m->dc = ccdc(NULL);
    if (!m->dc) return 0;
    m->bmp = cds(m->dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!m->bmp) { ((BOOL (WINAPI *)(HDC))t_proc("DeleteDC"))(m->dc); m->dc = NULL; return 0; }
    m->old = so(m->dc, m->bmp); m->bits = bits; m->w = w; m->h = h;
    memset(bits, 0, (size_t)w * h * 4);
    return 1;
}
static inline void g5_mem_free(g5_mem *m) {
    if (!m->dc) return;
    ((HGDIOBJ (WINAPI *)(HDC, HGDIOBJ))t_proc("SelectObject"))(m->dc, m->old);
    ((BOOL (WINAPI *)(HGDIOBJ))t_proc("DeleteObject"))(m->bmp);
    ((BOOL (WINAPI *)(HDC))t_proc("DeleteDC"))(m->dc);
    m->dc = NULL;
}
#endif
