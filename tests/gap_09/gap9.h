/* Shared helpers for the gap_09 area: thunk inspection, raw calls, DIB helpers. */
#ifndef GAP9_H
#define GAP9_H
#include "t.h"
#include "tables.h"

#define NT_ERR9(s) ((LONG)(s) < 0)

static int g9_wine(void)
{
    static int v = -1;
    if (v < 0) v = GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version") != NULL;
    return v;
}
#define G9_NOWINE(what) do { if (g9_wine()) { t_skip("wine: %s", what); return; } } while (0)

/* Raw call with up to 12 register/stack arguments; callee that takes fewer ignores the rest. */
typedef ULONG_PTR (WINAPI *g9_fn12)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR,
                                    ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);
static ULONG_PTR g9_call(FARPROC f, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d)
{
    return ((g9_fn12)f)(a, b, c, d, 0, 0, 0, 0, 0, 0, 0, 0);
}
static ULONG_PTR g9_call5(FARPROC f, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e)
{
    return ((g9_fn12)f)(a, b, c, d, e, 0, 0, 0, 0, 0, 0, 0);
}
static ULONG_PTR g9_call8(FARPROC f, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d,
                          ULONG_PTR e, ULONG_PTR g, ULONG_PTR h, ULONG_PTR i)
{
    return ((g9_fn12)f)(a, b, c, d, e, g, h, i, 0, 0, 0, 0);
}

/* Follow "jmp qword ptr [rip+disp]" thunks (import trampolines). */
static const BYTE *g9_follow(const void *fn)
{
    const BYTE *p = (const BYTE *)fn;
    for (int i = 0; p && i < 4; i++) {
        if (p[0] == 0xFF && p[1] == 0x25) {
            LONG d = *(const LONG *)(p + 2);
            p = *(const BYTE *const *)(p + 6 + d);
        } else break;
    }
    return p;
}

enum { G9_OTHER = 0, G9_XOR0, G9_RET1, G9_SYSCALL };
/* Classify the final target of a function: stub returning 0, stub returning 1, or win32k syscall stub. */
static int g9_class(const void *fn, unsigned *sysno)
{
    const BYTE *p = g9_follow(fn);
    if (sysno) *sysno = 0;
    if (!p) return G9_OTHER;
    if (p[0] == 0x33 && p[1] == 0xC0 && p[2] == 0xC3) return G9_XOR0;
    if (p[0] == 0xB8 && p[1] == 1 && !p[2] && !p[3] && !p[4] && p[5] == 0xC3) return G9_RET1;
    if (p[0] == 0x4C && p[1] == 0x8B && p[2] == 0xD1 && p[3] == 0xB8) {
        if (sysno) *sysno = *(const DWORD *)(p + 4);
        return G9_SYSCALL;
    }
    return G9_OTHER;
}

static int g9_in_module(const void *p, HMODULE m)
{
    HMODULE h = NULL;
    if (!p || !GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &h)) return 0;
    return h == m;
}

static const char *const g9_xor_names[] = {
#define X(n) #n,
    GAP9_NTGDI_XOR_LIST(X)
    GAP9_OTHER_XOR_LIST(X)
#undef X
    NULL
};
static int g9_is_xor_name(const char *n)
{
    for (int i = 0; g9_xor_names[i]; i++) if (!strcmp(g9_xor_names[i], n)) return 1;
    return 0;
}

static HMODULE g9_win32u(void) { return GetModuleHandleA("win32u.dll"); }

/* A handle value that cannot be a live GDI object: index 0x1234, bogus type/reuse bits. */
#define G9_BOGUS ((ULONG_PTR)0x5A1F1234)

/* Memory DC with a 32bpp top-down DIBSection (w x h), pixels via *bits. */
typedef struct { HDC dc; HBITMAP bm, old; DWORD *bits; int w, h; } g9_dib_t;
static int g9_dib_open(g9_dib_t *d, int w, int h)
{
    memset(d, 0, sizeof *d);
    HDC (WINAPI *ccdc)(HDC) = (void *)t_proc("CreateCompatibleDC");
    HBITMAP (WINAPI *cds)(HDC, const BITMAPINFO *, UINT, void **, HANDLE, DWORD) = (void *)t_proc("CreateDIBSection");
    HGDIOBJ (WINAPI *so)(HDC, HGDIOBJ) = (void *)t_proc("SelectObject");
    if (!ccdc || !cds || !so) return 0;
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    d->dc = ccdc(NULL);
    if (!d->dc) return 0;
    d->bm = cds(d->dc, &bi, DIB_RGB_COLORS, (void **)&d->bits, NULL, 0);
    if (!d->bm) { BOOL (WINAPI *dd)(HDC) = (void *)t_proc("DeleteDC"); dd(d->dc); d->dc = NULL; return 0; }
    d->old = so(d->dc, d->bm);
    d->w = w; d->h = h;
    return 1;
}
static void g9_dib_close(g9_dib_t *d)
{
    HGDIOBJ (WINAPI *so)(HDC, HGDIOBJ) = (void *)t_proc("SelectObject");
    BOOL (WINAPI *del)(HGDIOBJ) = (void *)t_proc("DeleteObject");
    BOOL (WINAPI *dd)(HDC) = (void *)t_proc("DeleteDC");
    if (d->dc && d->old) so(d->dc, d->old);
    if (d->bm) del(d->bm);
    if (d->dc) dd(d->dc);
    memset(d, 0, sizeof *d);
}

/* Delete through the raw syscall so cached-handle recycling in gdi32full's DeleteObject cannot skew counts. */
static BOOL g9_ntdel(void *h)
{
    FARPROC f = t_proc_gdi32full("NtGdiDeleteObjectApp");
    return f ? (BOOL)(DWORD)g9_call(f, (ULONG_PTR)h, 0, 0, 0) : FALSE;
}

static DWORD g9_gdi_count(void)
{
    DWORD (WINAPI *gr)(HANDLE, DWORD) = (void *)t_proc("GetGuiResources");
    return gr ? gr(GetCurrentProcess(), 0 /* GR_GDIOBJECTS */) : 0;
}

#endif
