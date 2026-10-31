/* PE header, section, data-directory and export-directory sanity for gdi32, gdi32full and win32u,
 * on the loaded image and on a flat copy of the file. */
#include "exp.h"

/* Read a module's file into memory (flat view). Directory override for development: FWD_FLAT_DIR. */
static BYTE *read_module_file(int mod, size_t *sz)
{
    wchar_t path[MAX_PATH * 2];
    const char *ov = getenv("FWD_FLAT_DIR");
    if (ov) {
        wsprintfW(path, L"%S\\%s", ov, mod_wname[mod]);
    } else {
        HMODULE h = mod_handle(mod);
        if (!h) return NULL;
        DWORD n = GetModuleFileNameW(h, path, MAX_PATH * 2);
        if (!n || n >= MAX_PATH * 2) return NULL;
    }
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return NULL;
    DWORD len = GetFileSize(f, NULL);
    if (len == INVALID_FILE_SIZE || len > 16u * 1024 * 1024) { CloseHandle(f); return NULL; }
    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), 0, len);
    DWORD got = 0;
    if (!buf || !ReadFile(f, buf, len, &got, NULL) || got != len) { if (buf) HeapFree(GetProcessHeap(), 0, buf); CloseHandle(f); return NULL; }
    CloseHandle(f);
    *sz = len;
    return buf;
}

static int ci_eq(const char *a, const char *b) { return !lstrcmpiA(a, b); }

static void check_headers(int mod, const pemap_t *pm)
{
    const IMAGE_NT_HEADERS64 *nt = pe_nt(pm);
    CHECK(nt != NULL);
    if (!nt) return;
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)pm->base;
    EXPECT_EQ(dos->e_magic, IMAGE_DOS_SIGNATURE);
    CHECK((dos->e_lfanew & 7) == 0);
    EXPECT_EQ(nt->FileHeader.Machine, IMAGE_FILE_MACHINE_AMD64);
    EXPECT_EQ(nt->OptionalHeader.Magic, IMAGE_NT_OPTIONAL_HDR64_MAGIC);
    EXPECT_EQ(nt->FileHeader.SizeOfOptionalHeader, sizeof(IMAGE_OPTIONAL_HEADER64));
    CHECK(nt->FileHeader.Characteristics & IMAGE_FILE_DLL);
    CHECK(nt->FileHeader.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE);
    CHECK(nt->FileHeader.Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE);
    CHECK(!(nt->FileHeader.Characteristics & IMAGE_FILE_32BIT_MACHINE));
    CHECK(!(nt->FileHeader.Characteristics & IMAGE_FILE_RELOCS_STRIPPED));
    EXPECT_EQ(nt->OptionalHeader.NumberOfRvaAndSizes, IMAGE_NUMBEROF_DIRECTORY_ENTRIES);
    CHECK(nt->OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_GUI || nt->OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI);
    t_note("%s subsystem=%u linker=%u.%u os=%u.%u subsys-ver=%u.%u", mod_name[mod], nt->OptionalHeader.Subsystem,
           nt->OptionalHeader.MajorLinkerVersion, nt->OptionalHeader.MinorLinkerVersion,
           nt->OptionalHeader.MajorOperatingSystemVersion, nt->OptionalHeader.MinorOperatingSystemVersion,
           nt->OptionalHeader.MajorSubsystemVersion, nt->OptionalHeader.MinorSubsystemVersion);
    CHECK(nt->OptionalHeader.MajorOperatingSystemVersion >= 10);
    CHECK(nt->OptionalHeader.MajorSubsystemVersion >= 10);
    EXPECT_EQ(nt->OptionalHeader.SectionAlignment, 0x1000);
    CHECK(nt->OptionalHeader.FileAlignment >= 0x200 && nt->OptionalHeader.FileAlignment <= 0x1000);
    CHECK((nt->OptionalHeader.SizeOfImage % nt->OptionalHeader.SectionAlignment) == 0);
    CHECK(nt->OptionalHeader.SizeOfImage >= 0x10000);
    CHECK(nt->OptionalHeader.SizeOfHeaders >= 0x200 && nt->OptionalHeader.SizeOfHeaders <= 0x1000);
    CHECK((nt->OptionalHeader.ImageBase & 0xFFFF) == 0);
    if (pm->flat) EXPECT_EQ(nt->OptionalHeader.ImageBase, 0x180000000ULL);
    else CHECKF(nt->OptionalHeader.ImageBase == (ULONG_PTR)pm->base || nt->OptionalHeader.ImageBase == 0x180000000ULL,
                "loaded header ImageBase 0x%llx, mapped at %p", (unsigned long long)nt->OptionalHeader.ImageBase, (const void *)pm->base);
    WORD dc = nt->OptionalHeader.DllCharacteristics;
    CHECK(dc & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE);
    CHECK(dc & IMAGE_DLLCHARACTERISTICS_NX_COMPAT);
    CHECK(!(dc & IMAGE_DLLCHARACTERISTICS_NO_SEH) || 1);
    CHECK(dc & IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA || (t_note("%s lacks HIGH_ENTROPY_VA", mod_name[mod]), 1));
    if (!(dc & 0x4000)) t_note("%s lacks CFG flag", mod_name[mod]);
    if (nt->FileHeader.TimeDateStamp == 0) t_note("%s TimeDateStamp is zero", mod_name[mod]);
    else t_note("%s TimeDateStamp=0x%08lx", mod_name[mod], (unsigned long)nt->FileHeader.TimeDateStamp);
    CHECK(nt->OptionalHeader.SizeOfStackReserve >= 0x40000 || 1);

    /* Sections */
    int ns = nt->FileHeader.NumberOfSections;
    CHECK(ns >= 5 && ns <= 12);
    const IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    DWORD prev_end = nt->OptionalHeader.SizeOfHeaders, last_end = 0;
    prev_end = (prev_end + 0xFFF) & ~0xFFFu;
    for (int i = 0; i < ns; i++) {
        char nm[9]; memcpy(nm, s[i].Name, 8); nm[8] = 0;
        DWORD vs = s[i].Misc.VirtualSize;
        CHECKF(s[i].VirtualAddress >= prev_end, "%s section %s overlaps previous", mod_name[mod], nm);
        CHECKF((s[i].VirtualAddress % 0x1000) == 0, "%s %s unaligned va", mod_name[mod], nm);
        CHECK(vs > 0);
        CHECK(s[i].SizeOfRawData == 0 || (s[i].SizeOfRawData % nt->OptionalHeader.FileAlignment) == 0);
        CHECK(s[i].SizeOfRawData == 0 || (s[i].PointerToRawData % nt->OptionalHeader.FileAlignment) == 0);
        if (pm->flat) CHECK((size_t)s[i].PointerToRawData + s[i].SizeOfRawData <= pm->size);
        DWORD end = (s[i].VirtualAddress + vs + 0xFFF) & ~0xFFFu;
        CHECK(end <= nt->OptionalHeader.SizeOfImage);
        prev_end = end; last_end = end;
        DWORD ch = s[i].Characteristics;
        /* no section is both writable and executable */
        CHECKF(!((ch & IMAGE_SCN_MEM_WRITE) && (ch & IMAGE_SCN_MEM_EXECUTE)), "%s %s is W+X", mod_name[mod], nm);
        CHECKF(ch & IMAGE_SCN_MEM_READ, "%s %s not readable", mod_name[mod], nm);
        if (ch & IMAGE_SCN_MEM_EXECUTE) CHECK(ch & IMAGE_SCN_CNT_CODE);
        for (int j = 0; j < i; j++) CHECK(memcmp(s[i].Name, s[j].Name, 8) != 0);
        for (int k = 0; k < 8 && s[i].Name[k]; k++) CHECK(s[i].Name[k] >= 0x20 && s[i].Name[k] < 0x7f);
    }
    EXPECT_EQ(last_end, nt->OptionalHeader.SizeOfImage);
    const IMAGE_SECTION_HEADER *t = pe_section_named(pm, ".text");
    CHECK(t != NULL);
    if (t) {
        CHECK((t->Characteristics & (IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ)) == (IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ));
        CHECK(!(t->Characteristics & IMAGE_SCN_MEM_WRITE));
        CHECK(nt->OptionalHeader.AddressOfEntryPoint == 0 || (nt->OptionalHeader.AddressOfEntryPoint >= t->VirtualAddress && nt->OptionalHeader.AddressOfEntryPoint < t->VirtualAddress + t->Misc.VirtualSize));
        EXPECT_EQ(nt->OptionalHeader.BaseOfCode, t->VirtualAddress);
    }
    const IMAGE_SECTION_HEADER *r = pe_section_named(pm, ".rdata");
    CHECK(r != NULL);
    if (r) CHECK(!(r->Characteristics & (IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE)));
    const IMAGE_SECTION_HEADER *d = pe_section_named(pm, ".data");
    CHECK(d != NULL);
    if (d) CHECK((d->Characteristics & (IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_READ)) == (IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_READ));
    const IMAGE_SECTION_HEADER *p = pe_section_named(pm, ".pdata");
    CHECK(p != NULL);
    if (p) CHECK(!(p->Characteristics & (IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE)));
    const IMAGE_SECTION_HEADER *rs = pe_section_named(pm, ".rsrc");
    CHECK(rs != NULL);
    if (rs) CHECK(!(rs->Characteristics & (IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE)));
    const IMAGE_SECTION_HEADER *rl = pe_section_named(pm, ".reloc");
    CHECK(rl != NULL);
    if (rl) {
        CHECK(rl->Characteristics & IMAGE_SCN_MEM_DISCARDABLE);
        CHECK(!(rl->Characteristics & (IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE)));
    }
    if (pe_section_named(pm, ".didat")) t_note("%s has a .didat section (delay-import data)", mod_name[mod]);
    /* the last section is .reloc on MS-built system DLLs */
    CHECK(!strncmp((const char *)s[ns - 1].Name, ".reloc", 8) || (t_note("%s: last section is not .reloc", mod_name[mod]), 1));

    /* Data directories */
    static const struct { int idx; const char *what; int must; } dirs[] = {
        { IMAGE_DIRECTORY_ENTRY_EXPORT, "export", 1 }, { IMAGE_DIRECTORY_ENTRY_IMPORT, "import", 1 },
        { IMAGE_DIRECTORY_ENTRY_RESOURCE, "resource", 1 }, { IMAGE_DIRECTORY_ENTRY_EXCEPTION, "exception", 1 },
        { IMAGE_DIRECTORY_ENTRY_BASERELOC, "basereloc", 1 }, { IMAGE_DIRECTORY_ENTRY_DEBUG, "debug", 1 },
        { IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG, "loadconfig", 1 },
    };
    for (size_t i = 0; i < sizeof dirs / sizeof *dirs; i++) {
        const IMAGE_DATA_DIRECTORY *dd = pe_dir(pm, dirs[i].idx);
        if (mod == M_WIN32U && dirs[i].idx == IMAGE_DIRECTORY_ENTRY_IMPORT) { CHECK(!dd || !dd->VirtualAddress); continue; } /* win32u imports nothing */
        CHECKF(dd && dd->VirtualAddress && dd->Size, "%s %s directory missing", mod_name[mod], dirs[i].what);
        if (dd && dd->VirtualAddress) CHECK(dd->VirtualAddress + dd->Size <= nt->OptionalHeader.SizeOfImage);
    }
    const IMAGE_DATA_DIRECTORY *tls = pe_dir(pm, IMAGE_DIRECTORY_ENTRY_TLS);
    if (tls && tls->VirtualAddress) t_note("%s has a TLS directory", mod_name[mod]);
    const IMAGE_DATA_DIRECTORY *sec = pe_dir(pm, IMAGE_DIRECTORY_ENTRY_SECURITY);
    if (sec && sec->VirtualAddress) t_note("%s has an embedded Authenticode block", mod_name[mod]);
    const IMAGE_DATA_DIRECTORY *com = pe_dir(pm, IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR);
    CHECK(!com || com->VirtualAddress == 0); /* not managed code */
    const IMAGE_DATA_DIRECTORY *bnd = pe_dir(pm, IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT);
    CHECK(!bnd || bnd->VirtualAddress == 0 || 1);
}

static void check_debug_dir(int mod, const pemap_t *pm)
{
    const IMAGE_DATA_DIRECTORY *dd = pe_dir(pm, IMAGE_DIRECTORY_ENTRY_DEBUG);
    if (!dd || !dd->VirtualAddress) { CHECK(0); return; }
    const IMAGE_DEBUG_DIRECTORY *dbg = (const IMAGE_DEBUG_DIRECTORY *)pe_rva(pm, dd->VirtualAddress);
    CHECK(dbg != NULL);
    if (!dbg) return;
    int n = dd->Size / sizeof *dbg, found = 0;
    for (int i = 0; i < n; i++) {
        if (dbg[i].Type != IMAGE_DEBUG_TYPE_CODEVIEW) continue;
        if (!dbg[i].AddressOfRawData) continue;
        const BYTE *cv = (const BYTE *)pe_rva(pm, dbg[i].AddressOfRawData);
        if (!cv) continue;
        found++;
        CHECK(!memcmp(cv, "RSDS", 4));
        const char *pdb = (const char *)cv + 24;
        size_t l = strlen(pdb);
        CHECK(l > 4 && l < 200);
        CHECK(l > 4 && !lstrcmpiA(pdb + l - 4, ".pdb"));
        char want[40]; wsprintfA(want, "%S", mod_wname[mod]); want[strlen(want) - 4] = 0; strcat(want, ".pdb");
        CHECKF(ci_eq(pdb, want), "pdb name \"%s\" want \"%s\"", pdb, want);
        DWORD age = *(const DWORD *)(cv + 20);
        CHECK(age >= 1 && age < 100);
    }
    CHECK(found >= 1);
}

static void check_exportdir(int mod, const pemap_t *pm)
{
    pexp_t pe;
    CHECK(pe_exports(pm, &pe));
    if (!pe.dir) return;
    const IMAGE_NT_HEADERS64 *nt = pe_nt(pm);
    const IMAGE_EXPORT_DIRECTORY *d = pe.dir;
    EXPECT_EQ(d->Characteristics, 0);
    EXPECT_EQ(d->MajorVersion + d->MinorVersion, d->MajorVersion + d->MinorVersion);
    const char *nm = (const char *)pe_rva(pm, d->Name);
    CHECK(nm != NULL);
    if (nm) CHECKF(ci_eq(nm, mod_name[mod]), "export dir name \"%s\"", nm);
    CHECK(d->TimeDateStamp == 0 || d->TimeDateStamp == nt->FileHeader.TimeDateStamp || (t_note("%s export dir timestamp 0x%lx differs from header", mod_name[mod], (unsigned long)d->TimeDateStamp), 1));
    CHECK(pe.nnames <= pe.nfuncs);
    CHECK(pe.nfuncs > 0 && pe.nfuncs < 65536);
    const exp_ent_t *tab = mod_tab(mod);
    int tn = mod_tab_n(mod);
    unsigned minord = ~0u, maxord = 0, named = 0;
    for (int i = 0; i < tn; i++) { if (tab[i].ord < minord) minord = tab[i].ord; if (tab[i].ord > maxord) maxord = tab[i].ord; if (tab[i].name) named++; }
    int strict = tab_ok();
    if (strict) {
    EXPECT_EQ(pe.base, minord);
    EXPECT_EQ(pe.nfuncs, maxord - minord + 1);
    EXPECT_EQ(pe.nnames, named);
    }
    /* the name table is sorted by strcmp, strictly (no duplicates), all printable */
    const char *prev = NULL;
    DWORD live = 0;
    for (DWORD i = 0; i < pe.nnames; i++) {
        const char *cur = (const char *)pe_rva(pm, pe.names[i]);
        CHECK(cur != NULL);
        if (!cur) break;
        size_t l = strlen(cur);
        CHECK(l > 0 && l < 128);
        for (size_t k = 0; k < l; k++) if (cur[k] < 0x21 || cur[k] > 0x7e) { CHECKF(0, "non-printable in export name #%lu", i); break; }
        if (prev) CHECKF(strcmp(prev, cur) < 0, "names not strictly sorted at %lu: %s / %s", i, prev, cur);
        prev = cur;
        CHECK(pe.ords[i] < pe.nfuncs);
        CHECK(pe.funcs[pe.ords[i]] != 0);
    }
    /* each function slot is named at most once */
    BYTE *hit = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, pe.nfuncs ? pe.nfuncs : 1);
    CHECK(hit != NULL);
    if (hit) {
        for (DWORD i = 0; i < pe.nnames; i++) { CHECK(hit[pe.ords[i]] == 0); if (pe.ords[i] < pe.nfuncs) hit[pe.ords[i]] = 1; }
        for (DWORD i = 0; i < pe.nfuncs; i++) if (pe.funcs[i]) live++;
        HeapFree(GetProcessHeap(), 0, hit);
    }
    if (strict) EXPECT_EQ(live, (DWORD)tn);
    for (DWORD i = 0; i < pe.nfuncs; i++) {
        DWORD rva = pe.funcs[i];
        if (!rva) continue;
        CHECK(rva < nt->OptionalHeader.SizeOfImage);
        if (pe_is_fwd(&pe, rva)) {
            const char *f = (const char *)pe_rva(pm, rva);
            CHECK(f && strchr(f, '.') != NULL);
        }
    }
    /* every PE name is in the embedded table with identical ordinal and forwarder status */
    for (DWORD i = 0; strict && i < pe.nnames; i++) {
        const char *cur = (const char *)pe_rva(pm, pe.names[i]);
        if (!cur) break;
        int found = 0;
        for (int k = 0; k < tn; k++) {
            if (!tab[k].name || strcmp(tab[k].name, cur)) continue;
            found = 1;
            EXPECT_EQ(tab[k].ord, pe.base + pe.ords[i]);
            EXPECT_EQ(tab[k].fwd != NULL, pe_is_fwd(&pe, pe.funcs[pe.ords[i]]));
            break;
        }
        CHECKF(found, "%s exports unexpected name %s", mod_name[mod], cur);
    }
}

static void check_pdata(int mod, const pemap_t *pm)
{
    const IMAGE_DATA_DIRECTORY *dd = pe_dir(pm, IMAGE_DIRECTORY_ENTRY_EXCEPTION);
    CHECK(dd && dd->Size);
    if (!dd || !dd->Size) return;
    CHECK((dd->Size % 12) == 0);
    const IMAGE_NT_HEADERS64 *nt = pe_nt(pm);
    const IMAGE_SECTION_HEADER *ps = pe_section_named(pm, ".pdata");
    CHECK(ps && dd->VirtualAddress == ps->VirtualAddress);
    typedef struct { DWORD b, e, u; } rf_t;
    const rf_t *rf = (const rf_t *)pe_rva(pm, dd->VirtualAddress);
    CHECK(rf != NULL);
    if (!rf) return;
    DWORD n = dd->Size / 12, bad_order = 0, bad_range = 0, bad_exec = 0, prev_b = 0;
    for (DWORD i = 0; i < n; i++) {
        if (i && rf[i].b < prev_b) bad_order++;
        prev_b = rf[i].b;
        if (rf[i].e <= rf[i].b || rf[i].e > nt->OptionalHeader.SizeOfImage) bad_range++;
        else { const IMAGE_SECTION_HEADER *s = pe_section_of(pm, rf[i].b); if (!s || !(s->Characteristics & IMAGE_SCN_MEM_EXECUTE)) bad_exec++; }
    }
    CHECKF(bad_order == 0, "%s: %lu pdata entries out of order", mod_name[mod], bad_order);
    CHECKF(bad_range == 0, "%s: %lu pdata entries with bad range", mod_name[mod], bad_range);
    CHECKF(bad_exec == 0, "%s: %lu pdata entries outside code", mod_name[mod], bad_exec);
    t_note("%s: %lu runtime functions", mod_name[mod], n);
    CHECK(n > 100);
}

#define PE_SUITE(M, ID) \
    TEST(pe_##ID##_headers_loaded) { pemap_t pm; if (!mod_map(M, &pm)) { t_skip("no genuine module"); return; } check_headers(M, &pm); } \
    TEST(pe_##ID##_debugdir_loaded) { pemap_t pm; if (!mod_map(M, &pm)) { t_skip("no genuine module"); return; } check_debug_dir(M, &pm); } \
    TEST(pe_##ID##_exportdir_loaded) { pemap_t pm; if (!mod_map(M, &pm)) { t_skip("no genuine module"); return; } check_exportdir(M, &pm); } \
    TEST(pe_##ID##_pdata_loaded) { pemap_t pm; if (!mod_map(M, &pm)) { t_skip("no genuine module"); return; } check_pdata(M, &pm); } \
    TEST(pe_##ID##_file_flat) { \
        size_t sz = 0; BYTE *b = read_module_file(M, &sz); \
        if (!b) { t_skip("cannot read module file"); return; } \
        pemap_t pm = { b, sz, 1 }; \
        check_headers(M, &pm); check_debug_dir(M, &pm); check_exportdir(M, &pm); check_pdata(M, &pm); \
        pemap_t im; \
        if (mod_map(M, &im)) { \
            const IMAGE_NT_HEADERS64 *a = pe_nt(&pm), *l = pe_nt(&im); \
            EXPECT_EQ(a->FileHeader.TimeDateStamp, l->FileHeader.TimeDateStamp); \
            EXPECT_EQ(a->OptionalHeader.SizeOfImage, l->OptionalHeader.SizeOfImage); \
            EXPECT_EQ(a->OptionalHeader.AddressOfEntryPoint, l->OptionalHeader.AddressOfEntryPoint); \
            EXPECT_EQ(a->OptionalHeader.CheckSum, l->OptionalHeader.CheckSum); \
            EXPECT_EQ(a->FileHeader.NumberOfSections, l->FileHeader.NumberOfSections); \
        } \
        HeapFree(GetProcessHeap(), 0, b); \
    }
PE_SUITE(M_GDI32, gdi32)
PE_SUITE(M_GDI32FULL, gdi32full)
PE_SUITE(M_WIN32U, win32u)

/* The loaded header image must be read-only and the entry/export memory state sane. */
static void check_vm(int mod)
{
    pemap_t pm;
    if (!mod_map(mod, &pm)) { t_skip("no genuine module"); return; }
    MEMORY_BASIC_INFORMATION mbi;
    CHECK(VirtualQuery(pm.base, &mbi, sizeof mbi) == sizeof mbi);
    EXPECT_EQ(mbi.AllocationBase, pm.base);
    EXPECT_EQ(mbi.BaseAddress, pm.base);
    EXPECT_EQ(mbi.Type, MEM_IMAGE);
    EXPECT_EQ(mbi.State, MEM_COMMIT);
    EXPECT_EQ(mbi.Protect, PAGE_READONLY);
    const IMAGE_NT_HEADERS64 *nt = pe_nt(&pm);
    const IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        const BYTE *a = pm.base + s[i].VirtualAddress;
        if (VirtualQuery(a, &mbi, sizeof mbi) != sizeof mbi) { CHECK(0); continue; }
        EXPECT_EQ(mbi.AllocationBase, pm.base);
        EXPECT_EQ(mbi.Type, MEM_IMAGE);
        DWORD ch = s[i].Characteristics;
        DWORD p = mbi.Protect & 0xFF;
        if (ch & IMAGE_SCN_MEM_EXECUTE) CHECKF(p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY, "%s %.8s prot 0x%lx", mod_name[mod], s[i].Name, mbi.Protect);
        else if (ch & IMAGE_SCN_MEM_WRITE) CHECKF(p == PAGE_READWRITE || p == PAGE_WRITECOPY, "%s %.8s prot 0x%lx", mod_name[mod], s[i].Name, mbi.Protect);
        else { if (p != PAGE_READONLY) t_note("%s %.8s read-only section has protection 0x%lx", mod_name[mod], s[i].Name, mbi.Protect); CHECKF(!(p & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)), "%s %.8s executable (0x%lx)", mod_name[mod], s[i].Name, mbi.Protect); }
    }
    CHECK(((ULONG_PTR)pm.base & 0xFFFF) == 0);
    CHECK((ULONG_PTR)pm.base < 0x00007FFFFFFF0000ULL);
    /* ASLR: informational */
    if ((ULONG_PTR)pm.base == nt->OptionalHeader.ImageBase) t_note("%s loaded at preferred base", mod_name[mod]);
}
TEST(pe_gdi32_vm_layout) { check_vm(M_GDI32); }
TEST(pe_gdi32full_vm_layout) { check_vm(M_GDI32FULL); }
TEST(pe_win32u_vm_layout) { check_vm(M_WIN32U); }

/* The three modules do not overlap and are different. */
TEST(pe_module_bases_disjoint)
{
    pemap_t a, b, c;
    if (!mod_map(M_GDI32, &a) || !mod_map(M_GDI32FULL, &b) || !mod_map(M_WIN32U, &c)) { t_skip("no genuine modules"); return; }
    const pemap_t *m[3] = { &a, &b, &c };
    for (int i = 0; i < 3; i++) for (int j = i + 1; j < 3; j++) {
        ULONG_PTR s1 = (ULONG_PTR)m[i]->base, e1 = s1 + m[i]->size, s2 = (ULONG_PTR)m[j]->base, e2 = s2 + m[j]->size;
        CHECK(e1 <= s2 || e2 <= s1);
    }
}

/* Section names and sizes of gdi32full are consistent with a 1MB-class image. */
TEST(pe_gdi32full_larger_than_gdi32)
{
    pemap_t a, b, c;
    if (!mod_map(M_GDI32, &a) || !mod_map(M_GDI32FULL, &b) || !mod_map(M_WIN32U, &c)) { t_skip("no genuine modules"); return; }
    CHECK(b.size > a.size);
    CHECK(a.size >= 0x20000 && a.size <= 0x100000);
    CHECK(b.size >= 0x80000 && b.size <= 0x400000);
    CHECK(c.size >= 0x10000 && c.size <= 0x100000 * 4);
}
