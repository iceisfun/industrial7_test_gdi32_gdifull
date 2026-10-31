/* Shared helpers for the gap_04 area (undocumented Gdi* exports). */
#ifndef GAP4_H
#define GAP4_H
#include "t.h"
#include <stdlib.h>

#define SOFT(c) do { if (!(c)) t_note("%s:%d soft expectation not met: %s", __FILE__, __LINE__, #c); } while (0)

static int g4_wine(void) {
    HMODULE n = GetModuleHandleA("ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version") != NULL;
}
/* Skip (a no-op on real Windows) tests that hit unimplemented emulator stubs. */
#define G4_WINE_SKIP(why) do { if (g4_wine()) { t_skip("emulator: " why); return; } } while (0)

static DWORD g4_gdi_count(void) {
    DWORD (WINAPI *g)(HANDLE, DWORD) = (void *)t_proc("GetGuiResources");
    return g ? g(GetCurrentProcess(), 0) : 0;
}

static int g4_ok(const char *n) {
    static const char *stubs[] = { "GdiConvertFont", "GdiConvertPalette", "GdiConvertRegion", "GdiGetLocalBrush",
        "GdiGetLocalDC", "GdiGetLocalFont", "GdiConvertEnhMetaFile", "GdiConvertMetaFilePict", "GdiCreateLocalEnhMetaFile",
        "GdiCreateLocalMetaFilePict", "GdiDeleteLocalDC", "GdiConvertBitmap", "GdiConvertBrush", "GdiConvertDC", NULL };
    if (g4_wine()) for (int i = 0; stubs[i]; i++) if (!strcmp(stubs[i], n)) return 0;
    return t_proc(n) != NULL;
}
#define RQ(n) do { if (!g4_ok(#n)) { t_skip("unavailable: " #n); return; } } while (0)

/* Declarations for the undocumented exports (all resolve through gdi32.dll). */
UDECL(HANDLE, GdiConvertFont, (HANDLE));
UDECL(HANDLE, GdiConvertPalette, (HANDLE));
UDECL(HANDLE, GdiConvertRegion, (HANDLE));
UDECL(HANDLE, GdiConvertBitmap, (HANDLE));
UDECL(HANDLE, GdiConvertBrush, (HANDLE));
UDECL(HANDLE, GdiConvertDC, (HANDLE));
UDECL(HANDLE, GdiGetLocalBrush, (HANDLE));
UDECL(HANDLE, GdiGetLocalDC, (HANDLE));
UDECL(HANDLE, GdiGetLocalFont, (HANDLE));
UDECL(HANDLE, GdiConvertEnhMetaFile, (HENHMETAFILE));
UDECL(HANDLE, GdiConvertMetaFilePict, (HGLOBAL));
UDECL(HENHMETAFILE, GdiCreateLocalEnhMetaFile, (HANDLE));
UDECL(HGLOBAL, GdiCreateLocalMetaFilePict, (HANDLE));
UDECL(void *, GdiConvertToDevmodeW, (const DEVMODEA *));
UDECL(DWORD, GdiGetBitmapBitsSize, (const void *));
UDECL(int, GdiDeleteLocalDC, (HANDLE));
UDECL(BOOL, GdiIsMetaFileDC, (HDC));
UDECL(DWORD, GdiIsMetaPrintDC, (HDC));
UDECL(DWORD, GdiIsPlayMetafileDC, (HDC));
UDECL(BYTE, GdiIsScreenDC, (HDC));
UDECL(ULONG_PTR, GdiFixUpHandle, (DWORD));
UDECL(LONG, GdiGetEntry, (ULONG_PTR, void *));
UDECL(UINT, GdiGetCodePage, (HDC));
UDECL(int, GdiHandleBeingTracked, (void *));
UDECL(int, GdiIsTrackingEnabled, (void));
UDECL(int, GdiDescribePixelFormat, (HDC, int, UINT, PIXELFORMATDESCRIPTOR *));
UDECL(HANDLE, GdiGetDC, (void *));
UDECL(DWORD, GdiGetPageCount, (void *));
UDECL(void *, GdiGetPageHandle, (void *, DWORD, DWORD *));
UDECL(BOOL, GdiGetDevmodeForPage, (void *, DWORD, void **, void **));
UDECL(BOOL, GdiEndDocEMF, (void *));
UDECL(BYTE, GdiEndPageEMF, (void *, BYTE));
UDECL(BOOL, GdiDeleteSpoolFileHandle, (void *));
UDECL(void *, GdiGetSpoolFileHandle, (LPWSTR, DEVMODEW *, LPWSTR));
UDECL(BOOL, GdiDrawStream, (HDC, DWORD, void *));
UDECL(void, GdiVoid, (void));

/* GDI entry as the shared handle table presents it. */
typedef struct { void *pKernel; DWORD pid; WORD upper; BYTE objt; BYTE flags; void *pUser; } G4ENTRY;

static HENHMETAFILE g4_make_emf(int rects) {
    HDC dc = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    if (!dc) return NULL;
    for (int i = 0; i < rects; i++) U(Rectangle)(dc, 2 + i, 2 + i, 40 + i, 30 + i);
    return U(CloseEnhMetaFile)(dc);
}
static HMETAFILE g4_make_wmf(int rects) {
    HDC dc = U(CreateMetaFileW)(NULL);
    if (!dc) return NULL;
    for (int i = 0; i < rects; i++) U(Rectangle)(dc, 2 + i, 2 + i, 40 + i, 30 + i);
    return U(CloseMetaFile)(dc);
}
#endif
