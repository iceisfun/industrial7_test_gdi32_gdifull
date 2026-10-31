/* Sanity-run only (-DG8_SHIM): emulate user-mode helpers that an emulator lacks, mirroring the
 * Windows implementation, so the test logic itself can be exercised. Never part of normal builds. */
#ifndef G8_SHIM_H
#define G8_SHIM_H
static PVOID WINAPI shim_EngCreateSemaphore_f(void) {
    CRITICAL_SECTION *c = HeapAlloc(GetProcessHeap(), 0, 0x28);
    if (c) InitializeCriticalSection(c);
    return c;
}
static void WINAPI shim_EngDeleteSemaphore_f(PVOID p) { if (p) { DeleteCriticalSection(p); HeapFree(GetProcessHeap(), 0, p); } }
static void WINAPI shim_EngAcquireSemaphore_f(PVOID p) { EnterCriticalSection(p); }
static void WINAPI shim_EngReleaseSemaphore_f(PVOID p) { LeaveCriticalSection(p); }
static void WINAPI shim_EngQueryLocalTime_f(USHORT *t) {
    SYSTEMTIME s; GetLocalTime(&s);
    t[0]=s.wYear; t[1]=s.wMonth; t[2]=s.wDay; t[3]=s.wHour; t[4]=s.wMinute; t[5]=s.wSecond; t[6]=s.wMilliseconds; t[7]=s.wDayOfWeek;
}
static void WINAPI shim_EngGetCurrentCodePage_f(USHORT *oem, USHORT *ansi) { *ansi = (USHORT)GetACP(); *oem = (USHORT)GetOEMCP(); }
static PVOID WINAPI shim_EngFindResource_f(HANDLE h, int iName, int iType, ULONG *ps) {
    PVOID r = NULL; DWORD sz = 0;
    HRSRC hr = FindResourceExW(h, (LPCWSTR)(ULONG_PTR)(iType & 0xffff), (LPCWSTR)(ULONG_PTR)(USHORT)iName, 0);
    if (hr) { sz = SizeofResource(h, hr); if (sz) { HGLOBAL g = LoadResource(h, hr); if (g) r = LockResource(g); } }
    *ps = sz; return r;
}
static HMODULE WINAPI shim_EngLoadModule_f(LPWSTR p) { return LoadLibraryExW(p, NULL, 2); }
static BOOL WINAPI shim_EngFreeModule_f(HMODULE h) { return FreeLibrary(h); }
static int WINAPI shim_EngMultiByteToWideChar_f(UINT cp, LPWSTR o, int cbo, LPSTR i, int cbi) { return MultiByteToWideChar(cp, 0, i, cbi, o, (int)((unsigned)(cbo) >> 1)); }
static int WINAPI shim_EngWideCharToMultiByte_f(UINT cp, LPWSTR i, int cbi, LPSTR o, int cbo) { return WideCharToMultiByte(cp, 0, i, (int)(ULONG_PTR)(((ULONGLONG)(LONGLONG)cbi) >> 1) , o, cbo, NULL, NULL); }
static BOOL WINAPI shim_EngQueryEMFInfo_f(HANDLE a, PVOID b) { return 0; }
static HGLOBAL WINAPI shim_EngComputeGlyphSet_f(int a, int b, int c) { return NULL; }
static PVOID WINAPI shim_EngGetDriverName_f(HANDLE h) { return NULL; }
static LONG WINAPI shim_EngMultiByteToUnicodeN_f(LPWSTR o, ULONG mo, PULONG pc, LPSTR i, ULONG ci) {
    typedef LONG (NTAPI *f_t)(LPWSTR, ULONG, PULONG, LPSTR, ULONG);
    f_t f = (f_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlMultiByteToUnicodeN"); return f(o, mo, pc, i, ci);
}
static LONG WINAPI shim_EngUnicodeToMultiByteN_f(LPSTR o, ULONG mo, PULONG pc, LPWSTR i, ULONG ci) {
    typedef LONG (NTAPI *f_t)(LPSTR, ULONG, PULONG, LPWSTR, ULONG);
    f_t f = (f_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlUnicodeToMultiByteN"); return f(o, mo, pc, i, ci);
}
static FARPROC g8_get(const char *n, FARPROC shim) { FARPROC p = t_proc(n); return p ? p : shim; }
#undef UN
#define UN(n) ((n##_pfn)g8_get(#n, (FARPROC)shim_##n))
#define shim_EngCreateSemaphore (shim_EngCreateSemaphore_f)
#define shim_EngDeleteSemaphore (shim_EngDeleteSemaphore_f)
#define shim_EngAcquireSemaphore (shim_EngAcquireSemaphore_f)
#define shim_EngReleaseSemaphore (shim_EngReleaseSemaphore_f)
#define shim_EngQueryLocalTime (shim_EngQueryLocalTime_f)
#define shim_EngGetCurrentCodePage (shim_EngGetCurrentCodePage_f)
#define shim_EngFindResource (shim_EngFindResource_f)
#define shim_EngLoadModule (shim_EngLoadModule_f)
#define shim_EngFreeModule (shim_EngFreeModule_f)
#define shim_EngMultiByteToWideChar (shim_EngMultiByteToWideChar_f)
#define shim_EngWideCharToMultiByte (shim_EngWideCharToMultiByte_f)
#define shim_EngQueryEMFInfo (shim_EngQueryEMFInfo_f)
#define shim_EngComputeGlyphSet (shim_EngComputeGlyphSet_f)
#define shim_EngGetDriverName (shim_EngGetDriverName_f)
#define shim_EngGetPrinterDataFileName (shim_EngGetDriverName_f)
#define shim_EngMultiByteToUnicodeN (shim_EngMultiByteToUnicodeN_f)
#define shim_EngUnicodeToMultiByteN (shim_EngUnicodeToMultiByteN_f)
#define shim_EngCreateBitmap ((FARPROC)0)
#define shim_EngCreateDeviceBitmap ((FARPROC)0)
#define shim_EngCreateDeviceSurface ((FARPROC)0)
#define shim_EngDeleteSurface ((FARPROC)0)
#define shim_EngCreatePalette ((FARPROC)0)
#define shim_EngDeletePalette ((FARPROC)0)
#define shim_EngAssociateSurface ((FARPROC)0)
#define shim_EngMarkBandingSurface ((FARPROC)0)
#define shim_EngLockSurface ((FARPROC)0)
#define shim_EngCreateClip ((FARPROC)0)
#define shim_EngBitBlt ((FARPROC)0)
#define shim_EngCopyBits ((FARPROC)0)
#define shim_EngPaint ((FARPROC)0)
#define shim_EngLineTo ((FARPROC)0)
#define shim_EngTextOut ((FARPROC)0)
#define shim_EngEraseSurface ((FARPROC)0)
#define shim_EngGradientFill ((FARPROC)0)
#define shim_EngStretchBltROP ((FARPROC)0)
#endif
