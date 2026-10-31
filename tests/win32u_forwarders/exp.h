/* Shared declarations and PE parsing helpers for the win32u_forwarders area. */
#ifndef EXP_H
#define EXP_H
#include "t.h"
#include <stdlib.h>

typedef struct { const char *name; unsigned ord; const char *fwd; } exp_ent_t;
extern const exp_ent_t exp_tab_gdi32[];
extern const int exp_tab_gdi32_n;
extern const exp_ent_t exp_tab_gdi32full[];
extern const int exp_tab_gdi32full_n;
extern const exp_ent_t exp_tab_win32u[];
extern const int exp_tab_win32u_n;

/* A mapped PE: either an image mapping (RVA == offset from base) or a flat file view. */
typedef struct { const BYTE *base; size_t size; int flat; } pemap_t;

static const IMAGE_NT_HEADERS64 *pe_nt(const pemap_t *m)
{
    const IMAGE_DOS_HEADER *d = (const IMAGE_DOS_HEADER *)m->base;
    if (!m->base || d->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    if (d->e_lfanew <= 0 || d->e_lfanew > 0x1000) return NULL;
    const IMAGE_NT_HEADERS64 *nt = (const IMAGE_NT_HEADERS64 *)(m->base + d->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return NULL;
    return nt;
}

static const void *pe_rva(const pemap_t *m, DWORD rva)
{
    const IMAGE_NT_HEADERS64 *nt = pe_nt(m);
    if (!nt) return NULL;
    if (!m->flat) return m->base + rva;
    if (rva < nt->OptionalHeader.SizeOfHeaders) return m->base + rva;
    const IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        DWORD vs = s[i].Misc.VirtualSize ? s[i].Misc.VirtualSize : s[i].SizeOfRawData;
        if (rva >= s[i].VirtualAddress && rva < s[i].VirtualAddress + vs) {
            DWORD off = rva - s[i].VirtualAddress;
            if (off >= s[i].SizeOfRawData) return NULL;
            return m->base + s[i].PointerToRawData + off;
        }
    }
    return NULL;
}

static const IMAGE_DATA_DIRECTORY *pe_dir(const pemap_t *m, int idx)
{
    const IMAGE_NT_HEADERS64 *nt = pe_nt(m);
    if (!nt || idx >= (int)nt->OptionalHeader.NumberOfRvaAndSizes) return NULL;
    return &nt->OptionalHeader.DataDirectory[idx];
}

/* Parsed export directory. */
typedef struct {
    const IMAGE_EXPORT_DIRECTORY *dir;
    DWORD dir_rva, dir_size;
    const DWORD *funcs, *names;
    const WORD *ords;      /* name ordinals (index into funcs) */
    DWORD nfuncs, nnames, base;
} pexp_t;

static int pe_exports(const pemap_t *m, pexp_t *e)
{
    memset(e, 0, sizeof *e);
    const IMAGE_DATA_DIRECTORY *d = pe_dir(m, IMAGE_DIRECTORY_ENTRY_EXPORT);
    if (!d || !d->VirtualAddress) return 0;
    e->dir_rva = d->VirtualAddress; e->dir_size = d->Size;
    e->dir = (const IMAGE_EXPORT_DIRECTORY *)pe_rva(m, d->VirtualAddress);
    if (!e->dir) return 0;
    e->nfuncs = e->dir->NumberOfFunctions; e->nnames = e->dir->NumberOfNames; e->base = e->dir->Base;
    e->funcs = (const DWORD *)pe_rva(m, e->dir->AddressOfFunctions);
    e->names = (const DWORD *)pe_rva(m, e->dir->AddressOfNames);
    e->ords = (const WORD *)pe_rva(m, e->dir->AddressOfNameOrdinals);
    return e->funcs != NULL && (e->nnames == 0 || (e->names && e->ords));
}

static int pe_is_fwd(const pexp_t *e, DWORD rva)
{
    return rva >= e->dir_rva && rva < e->dir_rva + e->dir_size;
}

static const IMAGE_SECTION_HEADER *pe_section_of(const pemap_t *m, DWORD rva)
{
    const IMAGE_NT_HEADERS64 *nt = pe_nt(m);
    const IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        DWORD vs = s[i].Misc.VirtualSize ? s[i].Misc.VirtualSize : s[i].SizeOfRawData;
        if (rva >= s[i].VirtualAddress && rva < s[i].VirtualAddress + vs) return &s[i];
    }
    return NULL;
}

static const IMAGE_SECTION_HEADER *pe_section_named(const pemap_t *m, const char *name)
{
    const IMAGE_NT_HEADERS64 *nt = pe_nt(m);
    const IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++)
        if (!strncmp((const char *)s[i].Name, name, 8) && strlen(name) <= 8) return &s[i];
    return NULL;
}

/* Index into funcs[] of a named export, or -1. */
static int pe_find_name(const pemap_t *m, const pexp_t *e, const char *name)
{
    int lo = 0, hi = (int)e->nnames - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const char *n = (const char *)pe_rva(m, e->names[mid]);
        if (!n) return -1;
        int c = strcmp(name, n);
        if (c == 0) return e->ords[mid];
        if (c < 0) hi = mid - 1; else lo = mid + 1;
    }
    return -1;
}

/* Module descriptors */
enum { M_GDI32, M_GDI32FULL, M_WIN32U, M_COUNT };
static const char *const mod_name[M_COUNT] = { "gdi32.dll", "gdi32full.dll", "win32u.dll" };
static const wchar_t *const mod_wname[M_COUNT] = { L"gdi32.dll", L"gdi32full.dll", L"win32u.dll" };

static const exp_ent_t *mod_tab(int m) { return m == M_GDI32 ? exp_tab_gdi32 : m == M_GDI32FULL ? exp_tab_gdi32full : exp_tab_win32u; }
static int mod_tab_n(int m) { return m == M_GDI32 ? exp_tab_gdi32_n : m == M_GDI32FULL ? exp_tab_gdi32full_n : exp_tab_win32u_n; }

static int is_wine(void);
/* PE-structure tests describe genuine Windows binaries; an emulator's builtin modules are
 * skipped (mod_handle returns NULL) unless FWD_ON_WINE is set in the environment. */
static HMODULE mod_handle(int m)
{
    if (is_wine() && !getenv("FWD_ON_WINE")) return NULL;
    return LoadLibraryA(mod_name[m]);
}

/* Image-mapped view of a loaded module. With FWD_FLAT_DIR set (development aid) the file in that
 * directory is parsed as a flat view instead. */
static int mod_map(int m, pemap_t *pm)
{
    const char *ov = getenv("FWD_FLAT_DIR");
    if (ov) {
        static pemap_t cache[M_COUNT];
        if (!cache[m].base) {
            char path[MAX_PATH * 2];
            wsprintfA(path, "%s\\%s", ov, mod_name[m]);
            HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
            if (f == INVALID_HANDLE_VALUE) return 0;
            DWORD len = GetFileSize(f, NULL), got = 0;
            BYTE *b = (BYTE *)HeapAlloc(GetProcessHeap(), 0, len);
            if (!b || !ReadFile(f, b, len, &got, NULL) || got != len) { CloseHandle(f); return 0; }
            CloseHandle(f);
            cache[m].base = b; cache[m].size = len; cache[m].flat = 1;
        }
        *pm = cache[m];
        return 1;
    }
    HMODULE h = mod_handle(m);
    if (!h) return 0;
    pm->base = (const BYTE *)h; pm->flat = 0; pm->size = 0;
    const IMAGE_NT_HEADERS64 *nt = pe_nt(pm);
    if (!nt) return 0;
    pm->size = nt->OptionalHeader.SizeOfImage;
    return 1;
}

static int is_wine(void)
{
    return GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version") != NULL;
}

/* The embedded tables describe the Windows 10 19041 servicing family (19041..19045). */
static int tab_ok(void)
{
    static int cached = -1;
    if (cached < 0) {
        OSVERSIONINFOW ov; memset(&ov, 0, sizeof ov); ov.dwOSVersionInfoSize = sizeof ov;
        LONG (WINAPI *rtl)(OSVERSIONINFOW *) = (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
        cached = 1;
        if (rtl && rtl(&ov) == 0 && !getenv("FWD_ANY_BUILD") && (ov.dwMajorVersion != 10 || ov.dwBuildNumber < 19041 || ov.dwBuildNumber > 19045)) {
            t_note("OS build %lu.%lu.%lu is outside the 19041 family the export tables describe", ov.dwMajorVersion, ov.dwMinorVersion, ov.dwBuildNumber);
            cached = 0;
        }
    }
    return cached;
}
#define NEED_TAB() do { if (!tab_ok()) { t_skip("export tables do not describe this build"); return; } } while (0)

/* Letter bucket used for chunked tests: 'A'..'Z' for names, '_' otherwise. */
static int name_bucket(const char *n)
{
    char c = n[0];
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c >= 'A' && c <= 'Z') return c;
    return '_';
}
#endif
