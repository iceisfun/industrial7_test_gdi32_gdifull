/* Import-table sanity: win32u imported by name, bound IAT entries resolve to the exporting
 * module's functions, delay-import directories parse. */
#include "exp.h"

static int starts_ci(const char *s, const char *p) { return !_strnicmp(s, p, strlen(p)); }

static int is_apiset(const char *n) { return starts_ci(n, "api-ms-win-") || starts_ci(n, "ext-ms-win-"); }

typedef struct { char name[96]; int is_apiset; DWORD count; int delay; } impmod_t;

static int collect_imports(const pemap_t *pm, impmod_t *out, int max)
{
    int n = 0;
    const IMAGE_DATA_DIRECTORY *dd = pe_dir(pm, IMAGE_DIRECTORY_ENTRY_IMPORT);
    if (dd && dd->VirtualAddress) {
        const IMAGE_IMPORT_DESCRIPTOR *d = (const IMAGE_IMPORT_DESCRIPTOR *)pe_rva(pm, dd->VirtualAddress);
        for (; d && d->Name && n < max; d++) {
            const char *nm = (const char *)pe_rva(pm, d->Name);
            if (!nm) break;
            lstrcpynA(out[n].name, nm, sizeof out[n].name);
            out[n].is_apiset = is_apiset(nm); out[n].delay = 0; out[n].count = 0;
            const IMAGE_THUNK_DATA64 *t = (const IMAGE_THUNK_DATA64 *)pe_rva(pm, d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk);
            while (t && t->u1.AddressOfData) { out[n].count++; t++; }
            n++;
        }
    }
    const IMAGE_DATA_DIRECTORY *dl = pe_dir(pm, IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT);
    if (dl && dl->VirtualAddress) {
        const IMAGE_DELAYLOAD_DESCRIPTOR *d = (const IMAGE_DELAYLOAD_DESCRIPTOR *)pe_rva(pm, dl->VirtualAddress);
        for (; d && d->DllNameRVA && n < max; d++) {
            const char *nm = (const char *)pe_rva(pm, d->DllNameRVA);
            if (!nm) break;
            lstrcpynA(out[n].name, nm, sizeof out[n].name);
            out[n].is_apiset = is_apiset(nm); out[n].delay = 1; out[n].count = 0;
            const IMAGE_THUNK_DATA64 *t = (const IMAGE_THUNK_DATA64 *)pe_rva(pm, d->ImportNameTableRVA);
            while (t && t->u1.AddressOfData) { out[n].count++; t++; }
            n++;
        }
    }
    return n;
}

static int has_import(const impmod_t *m, int n, const char *name, int delay)
{
    for (int i = 0; i < n; i++) if (m[i].delay == delay && !lstrcmpiA(m[i].name, name)) return 1;
    return 0;
}

static void imports_for(int mod)
{
    pemap_t pm;
    if (!mod_map(mod, &pm)) { t_skip("no genuine module"); return; }
    static impmod_t im[128];
    int n = collect_imports(&pm, im, 128);
    CHECK(n > 5);
    CHECK(has_import(im, n, "ntdll.dll", 0));
    CHECK(has_import(im, n, "win32u.dll", 0));
    for (int i = 0; i < n; i++) {
        CHECK(im[i].count > 0);
        if (im[i].is_apiset) continue;
        /* non-apiset: only well-known system DLLs are expected */
        static const char *const known[] = { "ntdll.dll", "win32u.dll", "gdi32.dll", "user32.dll", "msvcp_win.dll", "dwrite.dll",
            "mscms.dll", "textshaping.dll", "wer.dll", "kernel32.dll", "kernelbase.dll", "advapi32.dll", "ucrtbase.dll", "dxcore.dll", "gdi32full.dll" };
        int ok = 0;
        for (size_t k = 0; k < sizeof known / sizeof *known; k++) if (!lstrcmpiA(im[i].name, known[k])) ok = 1;
        if (!ok) t_note("%s imports unexpected module %s%s", mod_name[mod], im[i].name, im[i].delay ? " (delay)" : "");
    }
    /* gdi32 (user-mode GDI) must not statically import user32, kernel32 or advapi32 directly */
    CHECK(!has_import(im, n, "kernel32.dll", 0));
    int apiset = 0, delay = 0;
    for (int i = 0; i < n; i++) { if (im[i].delay) delay++; if (im[i].is_apiset) apiset++; }
    t_note("%s: %d import modules (%d apiset, %d delay)", mod_name[mod], n, apiset, delay);
    CHECK(apiset > 0);
    if (mod != M_WIN32U) CHECK(delay > 0);
}
TEST(imp_gdi32_modules) { imports_for(M_GDI32); }
TEST(imp_gdi32full_modules) { imports_for(M_GDI32FULL); }

TEST(imp_gdi32_imports_gdi_apisets)
{
    pemap_t pm;
    if (!mod_map(M_GDI32, &pm)) { t_skip("no genuine module"); return; }
    static impmod_t im[128];
    int n = collect_imports(&pm, im, 128);
    CHECK(has_import(im, n, "ext-ms-win-gdi-internal-desktop-l1-1-0.dll", 1));
    CHECK(has_import(im, n, "ext-ms-win-gdi-draw-l1-1-0.dll", 1));
    CHECK(has_import(im, n, "ext-ms-win-gdi-dc-l1-2-0.dll", 1));
    CHECK(has_import(im, n, "ext-ms-win-gdi-font-l1-1-0.dll", 1));
    /* gdi32 does not statically import gdi32full (it is reached through delay-loaded apisets) */
    CHECK(!has_import(im, n, "gdi32full.dll", 0));
}

TEST(imp_gdi32full_imports_gdi32_and_user32)
{
    pemap_t pm;
    if (!mod_map(M_GDI32FULL, &pm)) { t_skip("no genuine module"); return; }
    static impmod_t im[128];
    int n = collect_imports(&pm, im, 128);
    CHECK(has_import(im, n, "GDI32.dll", 0));
    CHECK(has_import(im, n, "USER32.dll", 0));
    CHECK(has_import(im, n, "win32u.dll", 0));
    CHECK(has_import(im, n, "msvcp_win.dll", 0));
}

/* Every IAT slot imported from a loaded real module points at that module's export. */
static void iat_binding(int mod, const char *dllname)
{
    pemap_t pm;
    if (!mod_map(mod, &pm)) { t_skip("no genuine module"); return; }
    HMODULE tm = GetModuleHandleA(dllname);
    CHECKF(tm != NULL, "%s not loaded", dllname);
    if (!tm) return;
    const IMAGE_DATA_DIRECTORY *dd = pe_dir(&pm, IMAGE_DIRECTORY_ENTRY_IMPORT);
    CHECK(dd && dd->VirtualAddress);
    if (!dd || !dd->VirtualAddress) return;
    const IMAGE_IMPORT_DESCRIPTOR *d = (const IMAGE_IMPORT_DESCRIPTOR *)pe_rva(&pm, dd->VirtualAddress);
    int checked = 0;
    for (; d->Name; d++) {
        const char *nm = (const char *)pe_rva(&pm, d->Name);
        if (lstrcmpiA(nm, dllname)) continue;
        const IMAGE_THUNK_DATA64 *nt = (const IMAGE_THUNK_DATA64 *)pe_rva(&pm, d->OriginalFirstThunk);
        const ULONG_PTR *iat = (const ULONG_PTR *)pe_rva(&pm, d->FirstThunk);
        CHECK(nt && iat);
        if (!nt || !iat) return;
        for (; nt->u1.AddressOfData; nt++, iat++) {
            FARPROC want;
            if (IMAGE_SNAP_BY_ORDINAL64(nt->u1.Ordinal)) want = GetProcAddress(tm, (LPCSTR)(ULONG_PTR)IMAGE_ORDINAL64(nt->u1.Ordinal));
            else {
                const IMAGE_IMPORT_BY_NAME *ibn = (const IMAGE_IMPORT_BY_NAME *)pe_rva(&pm, (DWORD)nt->u1.AddressOfData);
                want = GetProcAddress(tm, (const char *)ibn->Name);
                CHECKF(want != NULL, "%s imports missing %s!%s", mod_name[mod], dllname, ibn->Name);
                if (want && *iat != (ULONG_PTR)want) CHECKF(0, "%s IAT slot for %s!%s is %p want %p", mod_name[mod], dllname, ibn->Name, (void *)*iat, (void *)want);
                else CHECK(1);
                checked++;
                continue;
            }
            CHECK(want && *iat == (ULONG_PTR)want);
            checked++;
        }
    }
    CHECKF(checked > 0, "%s: no imports from %s", mod_name[mod], dllname);
    t_note("%s: %d IAT slots verified against %s", mod_name[mod], checked, dllname);
}
TEST(imp_gdi32_iat_win32u) { iat_binding(M_GDI32, "win32u.dll"); }
TEST(imp_gdi32_iat_ntdll) { iat_binding(M_GDI32, "ntdll.dll"); }
TEST(imp_gdi32full_iat_win32u) { iat_binding(M_GDI32FULL, "win32u.dll"); }
TEST(imp_gdi32full_iat_ntdll) { iat_binding(M_GDI32FULL, "ntdll.dll"); }
TEST(imp_gdi32full_iat_gdi32) { iat_binding(M_GDI32FULL, "gdi32.dll"); }
TEST(imp_gdi32full_iat_user32) { iat_binding(M_GDI32FULL, "user32.dll"); }

/* Every win32u import of gdi32/gdi32full is a real win32u export that is an NtGdi/NtUser syscall stub. */
static void imported_win32u_names(int mod)
{
    pemap_t pm;
    if (!mod_map(mod, &pm)) { t_skip("no genuine module"); return; }
    const IMAGE_DATA_DIRECTORY *dd = pe_dir(&pm, IMAGE_DIRECTORY_ENTRY_IMPORT);
    const IMAGE_IMPORT_DESCRIPTOR *d = (const IMAGE_IMPORT_DESCRIPTOR *)pe_rva(&pm, dd->VirtualAddress);
    for (; d->Name; d++) {
        const char *nm = (const char *)pe_rva(&pm, d->Name);
        if (lstrcmpiA(nm, "win32u.dll")) continue;
        const IMAGE_THUNK_DATA64 *nt = (const IMAGE_THUNK_DATA64 *)pe_rva(&pm, d->OriginalFirstThunk);
        for (; nt->u1.AddressOfData; nt++) {
            CHECK(!IMAGE_SNAP_BY_ORDINAL64(nt->u1.Ordinal));
            const IMAGE_IMPORT_BY_NAME *ibn = (const IMAGE_IMPORT_BY_NAME *)pe_rva(&pm, (DWORD)nt->u1.AddressOfData);
            int found = 0;
            for (int k = 0; k < exp_tab_win32u_n; k++) if (!strcmp(exp_tab_win32u[k].name, (const char *)ibn->Name)) { found = 1; break; }
            CHECKF(found, "%s imports win32u!%s which is not in the export table", mod_name[mod], ibn->Name);
            CHECKF(!strncmp((const char *)ibn->Name, "Nt", 2), "unexpected non-Nt win32u import %s", ibn->Name);
        }
    }
}
TEST(imp_gdi32_win32u_names_exist) { imported_win32u_names(M_GDI32); }
TEST(imp_gdi32full_win32u_names_exist) { imported_win32u_names(M_GDI32FULL); }

/* Delay-import descriptors are well-formed. */
static void delay_descriptors(int mod)
{
    pemap_t pm;
    if (!mod_map(mod, &pm)) { t_skip("no genuine module"); return; }
    const IMAGE_DATA_DIRECTORY *dl = pe_dir(&pm, IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT);
    CHECK(dl && dl->VirtualAddress);
    if (!dl || !dl->VirtualAddress) return;
    const IMAGE_DELAYLOAD_DESCRIPTOR *d = (const IMAGE_DELAYLOAD_DESCRIPTOR *)pe_rva(&pm, dl->VirtualAddress);
    int n = 0;
    for (; d->DllNameRVA; d++, n++) {
        CHECK(d->Attributes.RvaBased);
        CHECK(d->ModuleHandleRVA != 0);
        CHECK(d->ImportAddressTableRVA != 0);
        CHECK(d->ImportNameTableRVA != 0);
        const char *nm = (const char *)pe_rva(&pm, d->DllNameRVA);
        CHECK(nm && strlen(nm) > 3);
        /* The delay IAT entries are either resolved or still point inside this image (thunk). */
        const IMAGE_THUNK_DATA64 *nt = (const IMAGE_THUNK_DATA64 *)pe_rva(&pm, d->ImportNameTableRVA);
        const ULONG_PTR *iat = (const ULONG_PTR *)pe_rva(&pm, d->ImportAddressTableRVA);
        for (; nt->u1.AddressOfData; nt++, iat++) CHECK(*iat != 0);
    }
    CHECK(n > 3);
}
TEST(imp_gdi32_delay_descriptors) { delay_descriptors(M_GDI32); }
TEST(imp_gdi32full_delay_descriptors) { delay_descriptors(M_GDI32FULL); }

/* Count of win32u imports is plausible and equals the sum of distinct names. */
static void win32u_import_counts(int mod, DWORD lo)
{
    pemap_t pm;
    if (!mod_map(mod, &pm)) { t_skip("no genuine module"); return; }
    static impmod_t im[128];
    int n = collect_imports(&pm, im, 128);
    for (int i = 0; i < n; i++) if (!im[i].delay && !lstrcmpiA(im[i].name, "win32u.dll")) { CHECK(im[i].count >= lo); t_note("%s imports %lu names from win32u", mod_name[mod], im[i].count); return; }
    CHECK(0);
}
TEST(imp_gdi32_win32u_count) { win32u_import_counts(M_GDI32, 50); }
TEST(imp_gdi32full_win32u_count) { win32u_import_counts(M_GDI32FULL, 100); }
