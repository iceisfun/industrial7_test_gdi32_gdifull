/* Export table resolution: every named export resolves, forwarders match the
 * embedded table and the target module's export, ordinal-only exports resolve. */
#include "exp.h"

#define LETTERS(X) X(A) X(B) X(C) X(D) X(E) X(F) X(G) X(H) X(I) X(L) X(M) X(N) X(O) X(P) X(Q) X(R) X(S) X(T) X(U) X(V) X(W) X(X)

static int is_exec_rva(const pemap_t *m, DWORD rva)
{
    const IMAGE_SECTION_HEADER *s = pe_section_of(m, rva);
    return s && (s->Characteristics & IMAGE_SCN_MEM_EXECUTE) && (s->Characteristics & IMAGE_SCN_CNT_CODE);
}

/* gdi32 exports a few data symbols (not code). */
static const char *const gdi32_data_exports[] = {
    "DxTrimNotificationListHead", "GdiBatchLimit", "LpkEditControl", "LpkpEditControlSize", "fpClosePrinter",
    "gMaxGdiHandleCount", "gW32PID", "g_systemCallFilterId", "ghICM", "pGdiDevCaps", "pGdiSharedHandleTable",
    "pGdiSharedMemory", "semDxTrimNotification",
};
static int is_data_export(int mod, const char *name)
{
    if (mod != M_GDI32) return 0;
    for (size_t i = 0; i < sizeof gdi32_data_exports / sizeof *gdi32_data_exports; i++)
        if (!strcmp(name, gdi32_data_exports[i])) return 1;
    return 0;
}

/* Named exports of one bucket resolve through GetProcAddress and agree with a manual walk. */
static void resolve_bucket(int mod, int letter)
{
    HMODULE h = mod_handle(mod);
    REQUIRE(h);
    pemap_t pm; pexp_t pe;
    CHECK(mod_map(mod, &pm));
    CHECK(pe_exports(&pm, &pe));
    const exp_ent_t *tab = mod_tab(mod);
    int n = mod_tab_n(mod), seen = 0;
    for (int i = 0; i < n; i++) {
        if (!tab[i].name || name_bucket(tab[i].name) != letter) continue;
        seen++;
        SetLastError(0xdeadbeef);
        FARPROC p = GetProcAddress(h, tab[i].name);
        CHECKF(p != NULL, "%s!%s unresolved", mod_name[mod], tab[i].name);
        int idx = pe_find_name(&pm, &pe, tab[i].name);
        CHECKF(idx >= 0, "%s!%s not in name table", mod_name[mod], tab[i].name);
        if (idx >= 0) {
            EXPECT_EQ(pe.base + idx, tab[i].ord);
            DWORD rva = pe.funcs[idx];
            if (!pe_is_fwd(&pe, rva) && p) EXPECT_EQ((const BYTE *)h + rva, (const BYTE *)p);
        }
        /* exact-case lookup only */
        if (p) {
            char low[200]; size_t k = 0;
            for (; tab[i].name[k] && k < sizeof low - 1; k++) low[k] = (char)((tab[i].name[k] >= 'A' && tab[i].name[k] <= 'Z') ? tab[i].name[k] + 32 : tab[i].name[k]);
            low[k] = 0;
            if (strcmp(low, tab[i].name) && pe_find_name(&pm, &pe, low) < 0) {
                SetLastError(0);
                CHECKF(GetProcAddress(h, low) == NULL, "%s resolved case-insensitively", low);
                EXPECT_ERR(0, ERROR_PROC_NOT_FOUND);
            }
        }
    }
    (void)seen;
}

/* Forwarder semantics for one bucket. */
static void fwd_bucket(int mod, int letter)
{
    HMODULE h = mod_handle(mod);
    REQUIRE(h);
    pemap_t pm; pexp_t pe;
    CHECK(mod_map(mod, &pm));
    CHECK(pe_exports(&pm, &pe));
    HMODULE w32u = LoadLibraryA("win32u.dll");
    const exp_ent_t *tab = mod_tab(mod);
    int n = mod_tab_n(mod);
    for (int i = 0; i < n; i++) {
        if (!tab[i].name || name_bucket(tab[i].name) != letter) continue;
        int idx = pe_find_name(&pm, &pe, tab[i].name);
        if (idx < 0) { CHECKF(0, "%s missing", tab[i].name); continue; }
        DWORD rva = pe.funcs[idx];
        int isf = pe_is_fwd(&pe, rva);
        EXPECT_EQ(isf, tab[i].fwd != NULL);
        FARPROC p = GetProcAddress(h, tab[i].name);
        if (!p) { CHECKF(0, "%s unresolved", tab[i].name); continue; }
        if (!isf) {
            if (is_data_export(mod, tab[i].name)) {
                const IMAGE_SECTION_HEADER *ds = pe_section_of(&pm, rva);
                CHECKF(ds && !(ds->Characteristics & IMAGE_SCN_MEM_EXECUTE), "%s data export in code", tab[i].name);
            } else
                CHECKF(is_exec_rva(&pm, rva), "%s rva 0x%lx not in code section", tab[i].name, rva);
            continue;
        }
        const char *fs = (const char *)((const BYTE *)h + rva);
        CHECKF(!strcmp(fs, tab[i].fwd), "%s forwarder \"%s\" want \"%s\"", tab[i].name, fs, tab[i].fwd);
        /* split MODULE.Symbol */
        const char *dot = strchr(tab[i].fwd, '.');
        CHECK(dot != NULL);
        if (!dot) continue;
        char tm[128]; size_t ml = (size_t)(dot - tab[i].fwd);
        if (ml >= sizeof tm - 5) { CHECKF(0, "module name too long"); continue; }
        memcpy(tm, tab[i].fwd, ml); strcpy(tm + ml, ".dll");
        const char *sym = dot + 1;
        HMODULE th = NULL;
        if (!lstrcmpiA(tm, "win32u.dll")) th = w32u;
        else if (!lstrcmpiA(tm, "gdi32.dll")) th = LoadLibraryA("gdi32.dll");
        else th = LoadLibraryA(tm);
        if (!th) { t_note("%s!%s -> %s not loadable (err %lu)", mod_name[mod], tab[i].name, tm, GetLastError()); continue; }
        FARPROC tp = (sym[0] == '#') ? GetProcAddress(th, (LPCSTR)(ULONG_PTR)atoi(sym + 1)) : GetProcAddress(th, sym);
        if (!lstrcmpiA(tm, "win32u.dll") || !lstrcmpiA(tm, "gdi32.dll")) {
            CHECKF(tp != NULL, "%s target %s.%s missing", tab[i].name, tm, sym);
            CHECKF(p == tp, "%s: %p vs target %p", tab[i].name, (void *)p, (void *)tp);
        } else if (p != tp) {
            t_note("%s!%s -> %s.%s address differs (%p vs %p)", mod_name[mod], tab[i].name, tm, sym, (void *)p, (void *)tp);
        }
        if (!lstrcmpiA(tm, "win32u.dll") && p) {
            /* The resolved address must lie inside win32u's image. */
            ULONG_PTR a = (ULONG_PTR)p, b = (ULONG_PTR)w32u;
            pemap_t wm; mod_map(M_WIN32U, &wm);
            CHECK(a >= b && a < b + wm.size);
        }
        /* name of the target symbol is exported by the table of win32u */
        if (!lstrcmpiA(tm, "win32u.dll")) {
            int found = 0;
            for (int k = 0; k < exp_tab_win32u_n; k++) if (!strcmp(exp_tab_win32u[k].name, sym)) { found = 1; break; }
            CHECKF(found, "%s not in win32u table", sym);
        }
    }
}

#define MK(L) \
    TEST(exp_gdi32_resolve_##L) { resolve_bucket(M_GDI32, #L[0]); } \
    TEST(exp_gdi32full_resolve_##L) { resolve_bucket(M_GDI32FULL, #L[0]); } \
    TEST(exp_gdi32_forward_##L) { fwd_bucket(M_GDI32, #L[0]); } \
    TEST(exp_gdi32full_forward_##L) { fwd_bucket(M_GDI32FULL, #L[0]); }
LETTERS(MK)
#undef MK

/* win32u exports in chunks of 100 */
static void win32u_chunk(int c)
{
    HMODULE h = mod_handle(M_WIN32U);
    REQUIRE(h);
    pemap_t pm; pexp_t pe;
    CHECK(mod_map(M_WIN32U, &pm));
    CHECK(pe_exports(&pm, &pe));
    for (int i = c * 100; i < (c + 1) * 100 && i < exp_tab_win32u_n; i++) {
        const exp_ent_t *e = &exp_tab_win32u[i];
        FARPROC p = GetProcAddress(h, e->name);
        CHECKF(p != NULL, "win32u!%s unresolved", e->name);
        int idx = pe_find_name(&pm, &pe, e->name);
        CHECKF(idx >= 0, "win32u!%s not in name table", e->name);
        if (idx < 0 || !p) continue;
        EXPECT_EQ(pe.base + idx, e->ord);
        CHECK(!pe_is_fwd(&pe, pe.funcs[idx]));
        CHECKF(is_exec_rva(&pm, pe.funcs[idx]), "win32u!%s not in code", e->name);
        FARPROC q = GetProcAddress(h, (LPCSTR)(ULONG_PTR)e->ord);
        CHECKF(q == p, "win32u!%s ordinal %u differs", e->name, e->ord);
    }
}
#define WC(n) TEST(exp_win32u_resolve_chunk##n) { win32u_chunk(n); }
WC(0) WC(1) WC(2) WC(3) WC(4) WC(5) WC(6) WC(7) WC(8) WC(9) WC(10) WC(11) WC(12) WC(13)

TEST(exp_win32u_table_covers_chunks)
{
    CHECK(exp_tab_win32u_n > 0 && exp_tab_win32u_n <= 1400);
    t_note("win32u table has %d exports (chunks cover %d)", exp_tab_win32u_n, 14 * 100);
    CHECK(exp_tab_win32u_n <= 14 * 100);
}

/* ---- ordinal-only exports ---- */
static void ordinal_only(int mod, int expect_count)
{
    HMODULE h = mod_handle(mod);
    REQUIRE(h);
    pemap_t pm; pexp_t pe;
    CHECK(mod_map(mod, &pm));
    CHECK(pe_exports(&pm, &pe));
    const exp_ent_t *tab = mod_tab(mod);
    int n = mod_tab_n(mod), cnt = 0;
    for (int i = 0; i < n; i++) {
        if (tab[i].name) continue;
        cnt++;
        SetLastError(0);
        FARPROC p = GetProcAddress(h, (LPCSTR)(ULONG_PTR)tab[i].ord);
        CHECKF(p != NULL, "ordinal %u unresolved", tab[i].ord);
        if (!p) continue;
        DWORD rva = pe.funcs[tab[i].ord - pe.base];
        CHECK(!pe_is_fwd(&pe, rva));
        EXPECT_EQ((const BYTE *)h + rva, (const BYTE *)p);
        ULONG_PTR a = (ULONG_PTR)p, b = (ULONG_PTR)h;
        CHECK(a > b && a < b + pm.size);
        CHECKF(is_exec_rva(&pm, rva), "ordinal %u not in executable section", tab[i].ord);
        /* none of the names resolves to the same RVA via the name table */
        int named = 0;
        for (DWORD k = 0; k < pe.nnames; k++) if (pe.ords[k] == tab[i].ord - pe.base) named = 1;
        CHECK(!named);
    }
    EXPECT_EQ(cnt, expect_count);
}
TEST(exp_gdi32_ordinal_only) { ordinal_only(M_GDI32, 14); }
TEST(exp_gdi32full_ordinal_only) { ordinal_only(M_GDI32FULL, 10); }

static void ordinal_by_name(int mod, int letter)
{
    HMODULE h = mod_handle(mod);
    REQUIRE(h);
    const exp_ent_t *tab = mod_tab(mod);
    int n = mod_tab_n(mod);
    for (int i = 0; i < n; i++) {
        if (!tab[i].name || name_bucket(tab[i].name) != letter) continue;
        FARPROC a = GetProcAddress(h, tab[i].name), b = GetProcAddress(h, (LPCSTR)(ULONG_PTR)tab[i].ord);
        CHECKF(a && a == b, "%s: name %p ordinal(%u) %p", tab[i].name, (void *)a, tab[i].ord, (void *)b);
    }
}
#define MO(L) \
    TEST(exp_gdi32_byordinal_##L) { ordinal_by_name(M_GDI32, #L[0]); } \
    TEST(exp_gdi32full_byordinal_##L) { ordinal_by_name(M_GDI32FULL, #L[0]); }
LETTERS(MO)

/* ---- negative lookups ---- */
static void negative_lookups(int mod)
{
    HMODULE h = mod_handle(mod);
    REQUIRE(h);
    static const char *const bad[] = { "NoSuchExport", "", "CreateDCWW", "createdcw_", "NtGdiNoSuchThing", "BitBlt\x01", "@1000", "#1000", "AbortDocc" };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0);
        FARPROC p = GetProcAddress(h, bad[i]);
        CHECKF(p == NULL, "\"%s\" resolved", bad[i]);
        if (!p) EXPECT_ERR(0, ERROR_PROC_NOT_FOUND);
    }
    static const unsigned ords[] = { 1, 2, 999, 0xFFFF, 0x7FFF, 0x8000, 2999, 5000 };
    for (size_t i = 0; i < sizeof ords / sizeof *ords; i++) {
        SetLastError(0);
        FARPROC p = GetProcAddress(h, (LPCSTR)(ULONG_PTR)ords[i]);
        CHECKF(p == NULL, "ordinal %u resolved", ords[i]);
        if (!p) EXPECT_ERR(0, ERROR_PROC_NOT_FOUND);
    }
    /* ordinal 0 / null name: result is NULL; last error is only observed */
    SetLastError(0);
    CHECK(GetProcAddress(h, (LPCSTR)0) == NULL);
    t_note("GetProcAddress(ordinal 0) last error %lu", GetLastError());
}
TEST(exp_gdi32_negative_lookups) { negative_lookups(M_GDI32); }
TEST(exp_gdi32full_negative_lookups) { negative_lookups(M_GDI32FULL); }
TEST(exp_win32u_negative_lookups) { negative_lookups(M_WIN32U); }

/* holes in the ordinal space return NULL */
static void ordinal_holes(int mod)
{
    HMODULE h = mod_handle(mod);
    REQUIRE(h);
    pemap_t pm; pexp_t pe;
    CHECK(mod_map(mod, &pm));
    CHECK(pe_exports(&pm, &pe));
    int holes = 0;
    for (DWORD i = 0; i < pe.nfuncs; i++) {
        if (pe.funcs[i]) continue;
        holes++;
        SetLastError(0);
        CHECK(GetProcAddress(h, (LPCSTR)(ULONG_PTR)(pe.base + i)) == NULL);
        EXPECT_ERR(0, ERROR_PROC_NOT_FOUND);
    }
    t_note("%s: %d unused ordinal slots of %lu", mod_name[mod], holes, pe.nfuncs);
}
TEST(exp_gdi32_ordinal_holes) { ordinal_holes(M_GDI32); }
TEST(exp_gdi32full_ordinal_holes) { ordinal_holes(M_GDI32FULL); }
TEST(exp_win32u_ordinal_holes) { ordinal_holes(M_WIN32U); }

/* ---- cross-module relationships ---- */
TEST(exp_gdi32full_forwards_to_gdi32)
{
    HMODULE g = mod_handle(M_GDI32), f = mod_handle(M_GDI32FULL);
    REQUIRE(g && f);
    static const char *const names[] = { "CreateDCW", "CreateICW", "DeleteDC" };
    for (int i = 0; i < 3; i++) {
        FARPROC a = GetProcAddress(f, names[i]), b = GetProcAddress(g, names[i]);
        CHECKF(a && a == b, "%s: gdi32full %p gdi32 %p", names[i], (void *)a, (void *)b);
        CHECK(a >= (FARPROC)g && (ULONG_PTR)a < (ULONG_PTR)g + 0x40000);
    }
}

TEST(exp_gdi32_win32u_forwarders_land_in_win32u)
{
    HMODULE g = mod_handle(M_GDI32), w = mod_handle(M_WIN32U);
    REQUIRE(g && w);
    pemap_t wm; CHECK(mod_map(M_WIN32U, &wm));
    int cnt = 0;
    for (int i = 0; i < exp_tab_gdi32_n; i++) {
        if (!exp_tab_gdi32[i].fwd || strncmp(exp_tab_gdi32[i].fwd, "WIN32U.", 7)) continue;
        cnt++;
        ULONG_PTR a = (ULONG_PTR)GetProcAddress(g, exp_tab_gdi32[i].name);
        CHECKF(a >= (ULONG_PTR)w && a < (ULONG_PTR)w + wm.size, "%s not in win32u", exp_tab_gdi32[i].name);
    }
    CHECK(cnt >= 200);
    t_note("%d gdi32 exports forward to WIN32U", cnt);
}

TEST(exp_gdi32_all_d3dkmt_exported)
{
    HMODULE g = mod_handle(M_GDI32);
    REQUIRE(g);
    int cnt = 0;
    for (int i = 0; i < exp_tab_gdi32_n; i++) {
        if (!exp_tab_gdi32[i].name || strncmp(exp_tab_gdi32[i].name, "D3DKMT", 6)) continue;
        cnt++;
        CHECK(GetProcAddress(g, exp_tab_gdi32[i].name) != NULL);
    }
    CHECK(cnt >= 200);
    CHECK(GetProcAddress(LoadLibraryA("gdi32full.dll"), "D3DKMTCloseAdapter") == NULL);
}

TEST(exp_gdi32_gdi32full_same_name_distinct_or_forward)
{
    HMODULE g = mod_handle(M_GDI32), f = mod_handle(M_GDI32FULL);
    REQUIRE(g && f);
    int both = 0, same = 0;
    for (int i = 0; i < exp_tab_gdi32_n; i++) {
        if (!exp_tab_gdi32[i].name) continue;
        FARPROC a = GetProcAddress(g, exp_tab_gdi32[i].name), b = GetProcAddress(f, exp_tab_gdi32[i].name);
        if (a && b) { both++; if (a == b) same++; }
    }
    t_note("%d names exported by both gdi32 and gdi32full; %d resolve to the same address", both, same);
    CHECK(both > 100);
    CHECK(same >= 3);
}

/* Table row counts (19041 family). Named counts are informational beyond the hard floor. */
TEST(exp_table_counts)
{
    for (int m = 0; m < M_COUNT; m++) {
        pemap_t pm; pexp_t pe;
        if (!mod_map(m, &pm)) { t_skip("no genuine module"); return; }
        CHECK(pe_exports(&pm, &pe));
        EXPECT_EQ(pe.nnames, (m == M_GDI32 ? 974 : m == M_GDI32FULL ? 1011 : 1317));
        t_note("%s: base=%lu nfuncs=%lu nnames=%lu", mod_name[m], pe.base, pe.nfuncs, pe.nnames);
    }
}
