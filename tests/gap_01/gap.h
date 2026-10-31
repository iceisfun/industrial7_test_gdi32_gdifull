#ifndef GAP_H
#define GAP_H
#include "t.h"

/* wine detection: some NtGdi stubs abort or are unimplemented there; no-op on Windows */
static int gap_is_wine(void) {
    HMODULE n = GetModuleHandleA("ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version") != NULL;
}
#define SKIP_WINE(why) do { if (gap_is_wine()) { t_skip("wine: " why); return; } } while (0)

typedef struct { USHORT Length, MaximumLength; PWSTR Buffer; } GAP_USTR;
typedef LONG GAP_NTSTATUS;
#define GAP_FAILED(s) ((LONG)(s) < 0)
#define STATUS_INVALID_PARAMETER_ 0xC000000DL

/* undocumented / header-less exports */
UDECL(DWORD,  GetBrushAttributes,   (HBRUSH));
UDECL(HBRUSH, SetBrushAttributes,   (HBRUSH, DWORD));
UDECL(HBRUSH, ClearBrushAttributes, (HBRUSH, DWORD));
UDECL(DWORD,  GetBitmapAttributes,  (HBITMAP));
UDECL(ULONG,  GetBitmapDpiScaleValue, (HBITMAP));
UDECL(ULONG,  GetDCDpiScaleValue,   (HDC));
UDECL(BOOL,   GetCurrentDpiInfo,    (HMONITOR, void *));
UDECL(BOOL,   MirrorRgn,            (HWND, HRGN));
UDECL(BOOL,   ScaleRgn,             (HDC, HRGN));
UDECL(BOOL,   ScaleValues,          (HDC, LONG *, ULONG));
UDECL(BOOL,   SetVirtualResolution, (HDC, int, int, int, int));
UDECL(BOOL,   SetMagicColors,       (HDC, ULONG, ULONG));
UDECL(DWORD,  SetLayoutWidth,       (HDC, LONG, DWORD));
UDECL(DWORD,  GetLayout,            (HDC));
UDECL(DWORD,  SetLayout,            (HDC, DWORD));
UDECL(BOOL,   PolyPatBlt,           (HDC, DWORD, const void *, ULONG, ULONG));
UDECL(int,    StartFormPage,        (HDC));
UDECL(int,    EndFormPage,          (HDC));
UDECL(int,    DocumentEventEx,      (void *, HANDLE, HDC, int, ULONG, void *, ULONG, void *));
UDECL(void,   DeleteEMFSpoolData,   (void *));
UDECL(BOOL,   IsValidEnhMetaRecord, (const void *, const void *));
UDECL(BOOL,   IsValidEnhMetaRecordOffExt, (const void *, const void *, ULONG, ULONG));
UDECL(BOOL,   UpdateICMRegKeyA,     (DWORD, LPSTR, LPSTR, UINT));
UDECL(BOOL,   UpdateICMRegKeyW,     (DWORD, LPWSTR, LPWSTR, UINT));
UDECL(BOOL,   IcmDeleteLocalDC,     (HDC, void *, void *));
UDECL(BOOL,   IcmReleaseCachedColorSpace, (void *));
UDECL(ULONG,  HT_Get8BPPFormatPalette, (void *, USHORT, USHORT, USHORT));
UDECL(ULONG,  HT_Get8BPPMaskPalette, (void *, BOOL, BYTE, USHORT, USHORT, USHORT));
UDECL(GAP_NTSTATUS, GetNumberOfPhysicalMonitors, (GAP_USTR *, DWORD *));
UDECL(GAP_NTSTATUS, GetPhysicalMonitors, (GAP_USTR *, DWORD, DWORD *, HANDLE *));
UDECL(GAP_NTSTATUS, GetPhysicalMonitorDescription, (HANDLE, DWORD, WCHAR *));
UDECL(GAP_NTSTATUS, DestroyPhysicalMonitorInternal, (HANDLE));
UDECL(GAP_NTSTATUS, GetCertificate, (GAP_USTR *, DWORD, void *, DWORD));
UDECL(GAP_NTSTATUS, GetCertificateSize, (GAP_USTR *, DWORD, DWORD *));
UDECL(GAP_NTSTATUS, GetCertificateByHandle, (HANDLE, DWORD, void *, DWORD));
UDECL(GAP_NTSTATUS, GetCertificateSizeByHandle, (HANDLE, DWORD, DWORD *));
UDECL(GAP_NTSTATUS, GetOPMInformation, (HANDLE, void *, void *));
UDECL(GAP_NTSTATUS, GetOPMRandomNumber, (HANDLE, void *));
UDECL(GAP_NTSTATUS, GetCOPPCompatibleOPMInformation, (HANDLE, void *, void *));
UDECL(GAP_NTSTATUS, GetSuggestedOPMProtectedOutputArraySize, (GAP_USTR *, DWORD *));
UDECL(GAP_NTSTATUS, CreateOPMProtectedOutput, (void *, HANDLE *));
UDECL(GAP_NTSTATUS, CreateOPMProtectedOutputs, (GAP_USTR *, DWORD, DWORD, DWORD *, HANDLE *));
UDECL(GAP_NTSTATUS, ConfigureOPMProtectedOutput, (HANDLE, void *, DWORD, void *));

#define GAP_STOCK_BIT 0x00800000ULL

static DWORD gap_gdi_count(void) {
    DWORD (WINAPI *g)(HANDLE, DWORD) = (void *)t_proc("GetGuiResources");
    return g ? g(GetCurrentProcess(), 0) : 0;
}

/* a memory DC holding a w x h 32bpp top-down DIB section; *old receives the previous bitmap */
typedef struct { HDC dc; HBITMAP bmp, old; DWORD *bits; int w, h; } GapDib;
static int gap_dib_make(GapDib *d, int w, int h) {
    memset(d, 0, sizeof *d);
    HDC (WINAPI *ccdc)(HDC) = (void *)t_proc("CreateCompatibleDC");
    HBITMAP (WINAPI *cds)(HDC, const BITMAPINFO *, UINT, void **, HANDLE, DWORD) = (void *)t_proc("CreateDIBSection");
    HGDIOBJ (WINAPI *sel)(HDC, HGDIOBJ) = (void *)t_proc("SelectObject");
    if (!ccdc || !cds || !sel) return 0;
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    d->dc = ccdc(NULL);
    if (!d->dc) return 0;
    void *bits = NULL;
    d->bmp = cds(d->dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!d->bmp || !bits) { U(DeleteDC)(d->dc); d->dc = NULL; return 0; }
    d->bits = bits; d->w = w; d->h = h;
    d->old = sel(d->dc, d->bmp);
    return 1;
}
static void gap_dib_free(GapDib *d) {
    if (!d->dc) return;
    U(SelectObject)(d->dc, d->old);
    U(DeleteObject)(d->bmp);
    U(DeleteDC)(d->dc);
    memset(d, 0, sizeof *d);
}
static void gap_dib_fill(GapDib *d, DWORD v) { for (int i = 0; i < d->w * d->h; i++) d->bits[i] = v; }
#define GAP_RGB32(c) ((((c) & 0xff) << 16) | ((c) & 0xff00) | (((c) >> 16) & 0xff))

/* display device name (e.g. \\.\DISPLAY1) of the primary monitor; returns 0 if unavailable */
static int gap_primary_device(WCHAR *out, HMONITOR *hm) {
    HMONITOR (WINAPI *mfp)(POINT, DWORD) = (void *)t_proc("MonitorFromPoint");
    BOOL (WINAPI *gmi)(HMONITOR, MONITORINFO *) = (void *)t_proc("GetMonitorInfoW");
    if (!mfp || !gmi) return 0;
    POINT p = {0, 0};
    HMONITOR m = mfp(p, MONITOR_DEFAULTTOPRIMARY);
    if (!m) return 0;
    MONITORINFOEXW mi; memset(&mi, 0, sizeof mi); mi.cbSize = sizeof mi;
    if (!gmi(m, (MONITORINFO *)&mi)) return 0;
    lstrcpyW(out, mi.szDevice);
    if (hm) *hm = m;
    return 1;
}
static void gap_ustr(GAP_USTR *u, const WCHAR *s) {
    u->Length = (USHORT)(lstrlenW(s) * 2); u->MaximumLength = (USHORT)(u->Length + 2); u->Buffer = (PWSTR)s;
}
#endif
