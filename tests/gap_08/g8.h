/* Shared helpers for the gap_08 area (Eng* user-mode entry points). */
#ifndef G8_H
#define G8_H
#include "t.h"
#include <stdint.h>

/* wine detection: unimplemented stubs there may abort, so such tests skip. */
static int g8_wine(void) {
    HMODULE n = GetModuleHandleA("ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version") != NULL;
}
#define G8_NOWINE_ALWAYS() do { if (g8_wine()) { t_skip("not run under wine"); return; } } while (0)
#ifdef G8_SHIM
#define G8_NOWINE() do { } while (0)
#else
#define G8_NOWINE() G8_NOWINE_ALWAYS()
#endif

/* Declarations for the DDI entry points (not in the SDK headers we include). */
UDECL(HBITMAP,  EngCreateBitmap,        (SIZE sizl, LONG lWidth, ULONG iFormat, ULONG fl, PVOID pvBits));
UDECL(HBITMAP,  EngCreateDeviceBitmap,  (ULONG_PTR dhsurf, SIZE sizl, ULONG iFormat));
UDECL(HBITMAP,  EngCreateDeviceSurface, (ULONG_PTR dhsurf, SIZE sizl, ULONG iFormat));
UDECL(BOOL,     EngDeleteSurface,       (HBITMAP hsurf));
UDECL(HPALETTE, EngCreatePalette,       (ULONG iMode, ULONG cColors, ULONG *pulColors, ULONG flRed, ULONG flGreen, ULONG flBlue));
UDECL(BOOL,     EngDeletePalette,       (HPALETTE hpal));
UDECL(BOOL,     EngAssociateSurface,    (HBITMAP hsurf, HANDLE hdev, ULONG flHooks));
UDECL(BOOL,     EngMarkBandingSurface,  (HBITMAP hsurf));
UDECL(PVOID,    EngLockSurface,         (HBITMAP hsurf));
UDECL(PVOID,    EngCreateClip,          (void));
UDECL(HGLOBAL,  EngComputeGlyphSet,     (int nCodePage, int nFirstChar, int cChars));
UDECL(PVOID,    EngCreateSemaphore,     (void));
UDECL(void,     EngDeleteSemaphore,     (PVOID hsem));
UDECL(void,     EngAcquireSemaphore,    (PVOID hsem));
UDECL(void,     EngReleaseSemaphore,    (PVOID hsem));
UDECL(void,     EngQueryLocalTime,      (USHORT *ptf));
UDECL(void,     EngGetCurrentCodePage,  (USHORT *oem, USHORT *ansi));
UDECL(PVOID,    EngFindResource,        (HANDLE h, int iName, int iType, ULONG *pulSize));
UDECL(HMODULE,  EngLoadModule,          (LPWSTR pwsz));
UDECL(BOOL,     EngFreeModule,          (HMODULE h));
UDECL(PVOID,    EngGetDriverName,       (HANDLE hdev));
UDECL(PVOID,    EngGetPrinterDataFileName, (HANDLE hdev));
UDECL(BOOL,     EngQueryEMFInfo,        (HANDLE hdev, PVOID pinfo));
UDECL(int,      EngMultiByteToWideChar, (UINT cp, LPWSTR out, int cbOut, LPSTR in, int cbIn));
UDECL(int,      EngWideCharToMultiByte, (UINT cp, LPWSTR in, int cbIn, LPSTR out, int cbOut));
UDECL(LONG,     EngMultiByteToUnicodeN, (LPWSTR out, ULONG maxOut, PULONG pcbOut, LPSTR in, ULONG cbIn));
UDECL(LONG,     EngUnicodeToMultiByteN, (LPSTR out, ULONG maxOut, PULONG pcbOut, LPWSTR in, ULONG cbIn));

#ifdef G8_SHIM
#include "g8_shim.h"
#endif

#define SZ(w, h) ((SIZE){ (w), (h) })

static UINT g8_gdicount(void) {
    DWORD (WINAPI *f)(HANDLE, DWORD) = (void *)t_proc("GetGuiResources");
    return f ? f(GetCurrentProcess(), 0) : 0;
}

/* DDI-style formats */
enum { G8_1BPP = 1, G8_4BPP, G8_8BPP, G8_16BPP, G8_24BPP, G8_32BPP, G8_4RLE, G8_8RLE };
#endif
