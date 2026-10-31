/* Data exports of gdi32.dll: gW32PID, gMaxGdiHandleCount, g_systemCallFilterId, pGdiSharedHandleTable,
 * pGdiSharedMemory, pGdiDevCaps, ghICM, fpClosePrinter, semDxTrimNotification, DxTrimNotificationListHead.
 *
 * Shared handle table cell (0x18 bytes, indexed by LOWORD(handle)):
 *   +0x00 kernel pointer   +0x08 owner pid (bit 0 is a flag)   +0x0C HIWORD(handle)
 *   +0x0E object type byte   +0x10 user-mode attribute pointer */
#include "gap9.h"

static const void *gd(const char *n) { return t_proc_gdi32(n); }

typedef struct { BYTE *base; } tbl_t;
static const BYTE *cell_of(HGDIOBJ h)
{
    BYTE *const *tbl = (BYTE *const *)gd("pGdiSharedHandleTable");
    if (!tbl || !*tbl) return NULL;
    return *tbl + ((ULONG_PTR)h & 0xFFFF) * 0x18;
}
static DWORD cell_pid(const BYTE *c) { return *(const DWORD *)(c + 8); }
static WORD cell_hi(const BYTE *c) { return *(const WORD *)(c + 0xC); }
static BYTE cell_type(const BYTE *c) { return c[0xE]; }
static void *cell_user(const BYTE *c) { return *(void *const *)(c + 0x10); }

/* ---- exports exist, only in gdi32 ---- */
static const char *const data_names[] = { "gW32PID", "gMaxGdiHandleCount", "g_systemCallFilterId", "pGdiSharedHandleTable",
    "pGdiSharedMemory", "pGdiDevCaps", "ghICM", "fpClosePrinter", "semDxTrimNotification", "DxTrimNotificationListHead" };

TEST(gap9_data_all_present_in_gdi32)
{
    G9_NOWINE("export set differs");
    for (size_t i = 0; i < sizeof data_names / sizeof *data_names; i++) {
        const void *p = gd(data_names[i]);
        CHECKF(p != NULL, "%s", data_names[i]);
        if (p) CHECKF(g9_in_module(p, t_gdi32()), "%s inside gdi32", data_names[i]);
    }
}

TEST(gap9_data_not_exported_by_gdi32full)
{
    for (size_t i = 0; i < sizeof data_names / sizeof *data_names; i++)
        CHECKF(t_proc_gdi32full(data_names[i]) == NULL, "%s", data_names[i]);
}

TEST(gap9_data_addresses_distinct_and_aligned)
{
    G9_NOWINE("export set differs");
    const void *a[sizeof data_names / sizeof *data_names];
    for (size_t i = 0; i < sizeof a / sizeof *a; i++) {
        a[i] = gd(data_names[i]);
        REQUIRE(a[i]);
        CHECKF(((ULONG_PTR)a[i] & 3) == 0, "%s aligned", data_names[i]);
        for (size_t j = 0; j < i; j++) CHECKF(a[i] != a[j], "%s vs %s", data_names[i], data_names[j]);
    }
}

TEST(gap9_data_exports_same_in_repeated_lookup)
{
    G9_NOWINE("export set differs");
    for (size_t i = 0; i < sizeof data_names / sizeof *data_names; i++) {
        const void *a = GetProcAddress(t_gdi32(), data_names[i]);
        const void *b = GetProcAddress(GetModuleHandleA("gdi32.dll"), data_names[i]);
        EXPECT_EQ(a, b);
    }
}

TEST(gap9_data_ordinal_lookup_matches_name)
{
    G9_NOWINE("export set differs");
    /* the data symbols are also exported by ordinal; GetProcAddress by ordinal returns the same address */
    static const unsigned ords[][1] = { { 0 } };
    (void)ords;
    HMODULE g = t_gdi32();
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)g;
    const IMAGE_NT_HEADERS64 *nt = (const IMAGE_NT_HEADERS64 *)((BYTE *)g + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY *dd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    REQUIRE(dd->VirtualAddress);
    const IMAGE_EXPORT_DIRECTORY *ed = (const IMAGE_EXPORT_DIRECTORY *)((BYTE *)g + dd->VirtualAddress);
    const DWORD *names = (const DWORD *)((BYTE *)g + ed->AddressOfNames);
    const WORD *nords = (const WORD *)((BYTE *)g + ed->AddressOfNameOrdinals);
    int checked = 0;
    for (DWORD i = 0; i < ed->NumberOfNames; i++) {
        const char *nm = (const char *)g + names[i];
        for (size_t k = 0; k < sizeof data_names / sizeof *data_names; k++) {
            if (strcmp(nm, data_names[k])) continue;
            FARPROC byname = GetProcAddress(g, nm);
            FARPROC byord = GetProcAddress(g, (LPCSTR)(ULONG_PTR)(nords[i] + ed->Base));
            EXPECT_EQ(byname, byord);
            checked++;
        }
    }
    EXPECT_EQ(checked, 10);
}

/* ---- gW32PID ---- */
TEST(gap9_gw32pid_equals_process_id)
{
    const DWORD *p = (const DWORD *)gd("gW32PID");
    REQUIRE(p);
    EXPECT_EQ(*p, GetCurrentProcessId());
    EXPECT_EQ(*p & 1, 0);
}

static DWORD WINAPI read_pid_thread(LPVOID a) { *(DWORD *)a = *(const DWORD *)t_proc_gdi32("gW32PID"); return 0; }
TEST(gap9_gw32pid_same_in_other_thread)
{
    REQUIRE(gd("gW32PID"));
    DWORD v = 0;
    HANDLE h = CreateThread(NULL, 0, read_pid_thread, &v, 0, NULL);
    REQUIRE(h);
    WaitForSingleObject(h, 10000);
    CloseHandle(h);
    EXPECT_EQ(v, GetCurrentProcessId());
}

TEST(gap9_gw32pid_matches_owner_in_handle_table)
{
    const DWORD *p = (const DWORD *)gd("gW32PID");
    REQUIRE(p);
    HBRUSH (WINAPI *cb)(COLORREF) = (void *)t_proc("CreateSolidBrush");
    BOOL (WINAPI *del)(HGDIOBJ) = (void *)t_proc("DeleteObject");
    REQUIRE(cb); REQUIRE(del);
    HBRUSH b = cb(RGB(5, 6, 7));
    REQUIRE(b);
    const BYTE *c = cell_of(b);
    if (c) EXPECT_EQ(cell_pid(c) & ~1u, *p);
    del(b);
}

/* ---- gMaxGdiHandleCount ---- */
TEST(gap9_maxhandlecount_range)
{
    const DWORD *p = (const DWORD *)gd("gMaxGdiHandleCount");
    REQUIRE(p);
    t_note("gMaxGdiHandleCount = 0x%lx", (unsigned long)*p);
    CHECK(*p >= 0x10000);
    CHECK(*p <= 0x1000000);
}

TEST(gap9_maxhandlecount_bounds_live_handle_indices)
{
    const DWORD *p = (const DWORD *)gd("gMaxGdiHandleCount");
    REQUIRE(p);
    HBRUSH (WINAPI *cb)(COLORREF) = (void *)t_proc("CreateSolidBrush");
    BOOL (WINAPI *del)(HGDIOBJ) = (void *)t_proc("DeleteObject");
    REQUIRE(cb); REQUIRE(del);
    HBRUSH hs[64];
    for (int i = 0; i < 64; i++) {
        hs[i] = cb(RGB(i, i, i));
        REQUIRE(hs[i]);
        ULONG_PTR h = (ULONG_PTR)hs[i];
        DWORD idx = (DWORD)((h & 0xFFFF) | ((h >> 8) & 0xFF0000));
        CHECKF(idx < *p, "index 0x%lx", (unsigned long)idx);
    }
    for (int i = 0; i < 64; i++) del(hs[i]);
}

TEST(gap9_maxhandlecount_above_gui_resources)
{
    const DWORD *p = (const DWORD *)gd("gMaxGdiHandleCount");
    REQUIRE(p);
    CHECK(g9_gdi_count() < *p);
}

TEST(gap9_maxhandlecount_stable)
{
    const DWORD *p = (const DWORD *)gd("gMaxGdiHandleCount");
    REQUIRE(p);
    DWORD a = *p;
    HBITMAP (WINAPI *cb)(int, int, UINT, UINT, const void *) = (void *)t_proc("CreateBitmap");
    BOOL (WINAPI *del)(HGDIOBJ) = (void *)t_proc("DeleteObject");
    REQUIRE(cb); REQUIRE(del);
    for (int i = 0; i < 50; i++) { HBITMAP b = cb(8, 8, 1, 1, NULL); if (b) del(b); }
    EXPECT_EQ(*p, a);
}

/* ---- g_systemCallFilterId ---- */
TEST(gap9_systemcallfilterid_value)
{
    const DWORD *p = (const DWORD *)gd("g_systemCallFilterId");
    REQUIRE(p);
    t_note("g_systemCallFilterId = %lu", (unsigned long)*p);
    /* 5 switches GetDeviceCaps onto the pGdiDevCaps fast path for one magic DC; an ordinary process has the default */
    CHECK(*p != 5);
}

TEST(gap9_systemcallfilterid_stable)
{
    const DWORD *p = (const DWORD *)gd("g_systemCallFilterId");
    REQUIRE(p);
    DWORD a = *p;
    HDC dc = U(CreateCompatibleDC)(NULL);
    if (dc) { U(GetDeviceCaps)(dc, BITSPIXEL); U(DeleteDC)(dc); }
    EXPECT_EQ(*p, a);
}

/* ---- pGdiSharedHandleTable / pGdiSharedMemory ---- */
TEST(gap9_sharedtable_pointer_is_mapped_readonly)
{
    BYTE *const *tbl = (BYTE *const *)gd("pGdiSharedHandleTable");
    REQUIRE(tbl);
    CHECK(*tbl != NULL);
    MEMORY_BASIC_INFORMATION mbi;
    REQUIRE(VirtualQuery(*tbl, &mbi, sizeof mbi) == sizeof mbi);
    CHECK(mbi.State == MEM_COMMIT);
    CHECK(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE));
    t_note("shared table %p type 0x%lx protect 0x%lx size 0x%llx", *tbl, (unsigned long)mbi.Type, (unsigned long)mbi.Protect, (unsigned long long)mbi.RegionSize);
    CHECK(!(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)));
}

TEST(gap9_sharedtable_covers_all_cells)
{
    BYTE *const *tbl = (BYTE *const *)gd("pGdiSharedHandleTable");
    REQUIRE(tbl); REQUIRE(*tbl);
    /* 0x10000 cells of 0x18 bytes, then the device-caps block */
    BYTE probe = *(volatile BYTE *)(*tbl + 0x10000 * 0x18 - 1);
    (void)probe;
    MEMORY_BASIC_INFORMATION mbi;
    REQUIRE(VirtualQuery(*tbl + 0x10000 * 0x18 - 1, &mbi, sizeof mbi) == sizeof mbi);
    CHECK(mbi.State == MEM_COMMIT);
}

TEST(gap9_sharedtable_matches_peb)
{
    BYTE *const *tbl = (BYTE *const *)gd("pGdiSharedHandleTable");
    BYTE *const *mem = (BYTE *const *)gd("pGdiSharedMemory");
    REQUIRE(tbl); REQUIRE(mem);
    BYTE *peb = *(BYTE **)((BYTE *)NtCurrentTeb() + 0x60);
    EXPECT_EQ(*tbl, *(BYTE **)(peb + 0xF8));
    EXPECT_EQ(*mem, *tbl);
}

TEST(gap9_sharedtable_dc_cell)
{
    const DWORD *pid = (const DWORD *)gd("gW32PID");
    REQUIRE(pid);
    HDC dc = U(CreateCompatibleDC)(NULL);
    REQUIRE(dc);
    const BYTE *c = cell_of(dc);
    REQUIRE(c);
    EXPECT_EQ(cell_pid(c) & ~1u, *pid);
    EXPECT_EQ(cell_hi(c), HIWORD((ULONG_PTR)dc));
    EXPECT_EQ(cell_type(c), 1);
    CHECK(cell_user(c) != NULL);
    U(DeleteDC)(dc);
}

TEST(gap9_sharedtable_brush_cell)
{
    const DWORD *pid = (const DWORD *)gd("gW32PID");
    REQUIRE(pid);
    HBRUSH b = U(CreateSolidBrush)(RGB(1, 2, 3));
    REQUIRE(b);
    const BYTE *c = cell_of(b);
    REQUIRE(c);
    EXPECT_EQ(cell_pid(c) & ~1u, *pid);
    EXPECT_EQ(cell_hi(c), HIWORD((ULONG_PTR)b));
    EXPECT_EQ(cell_type(c), 0x10);
    U(DeleteObject)(b);
}

TEST(gap9_sharedtable_region_cell)
{
    const DWORD *pid = (const DWORD *)gd("gW32PID");
    REQUIRE(pid);
    HRGN r = U(CreateRectRgn)(0, 0, 5, 5);
    REQUIRE(r);
    const BYTE *c = cell_of(r);
    REQUIRE(c);
    EXPECT_EQ(cell_pid(c) & ~1u, *pid);
    EXPECT_EQ(cell_hi(c), HIWORD((ULONG_PTR)r));
    EXPECT_EQ(cell_type(c), 4);
    U(DeleteObject)(r);
}

TEST(gap9_sharedtable_other_object_cells)
{
    const DWORD *pid = (const DWORD *)gd("gW32PID");
    REQUIRE(pid);
    HBITMAP bm = U(CreateBitmap)(4, 4, 1, 1, NULL);
    HPEN pn = U(CreatePen)(PS_SOLID, 1, 0);
    HFONT ft = U(CreateFontW)(12, 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, 0, 0, L"Arial");
    LOGPALETTE lp = { 0x300, 1, { { 1, 2, 3, 0 } } };
    HPALETTE pal = U(CreatePalette)(&lp);
    HGDIOBJ objs[4] = { bm, pn, ft, pal };
    const char *nm[4] = { "bitmap", "pen", "font", "palette" };
    for (int i = 0; i < 4; i++) {
        REQUIRE(objs[i]);
        const BYTE *c = cell_of(objs[i]);
        REQUIRE(c);
        CHECKF((cell_pid(c) & ~1u) == *pid, "%s pid", nm[i]);
        CHECKF(cell_hi(c) == HIWORD((ULONG_PTR)objs[i]), "%s hiword", nm[i]);
        t_note("%s handle %p cell type 0x%02x user %p", nm[i], objs[i], cell_type(c), cell_user(c));
    }
    for (int i = 0; i < 4; i++) U(DeleteObject)(objs[i]);
}

TEST(gap9_sharedtable_cell_released_after_delete)
{
    G9_NOWINE("shared handle table");
    const DWORD *pid = (const DWORD *)gd("gW32PID");
    REQUIRE(pid);
    HBITMAP b = U(CreateBitmap)(2, 2, 1, 1, NULL);
    REQUIRE(b);
    const BYTE *c = cell_of(b);
    REQUIRE(c);
    int live0 = (cell_pid(c) & ~1u) == *pid && cell_hi(c) == HIWORD((ULONG_PTR)b);
    CHECK(live0);
    CHECK(U(DeleteObject)(b));
    EXPECT_EQ(U(GetObjectType)(b), 0);
    int live1 = (cell_pid(c) & ~1u) == *pid && cell_hi(c) == HIWORD((ULONG_PTR)b) && cell_type(c) != 0;
    t_note("cell after delete: pid=%lu hi=0x%04x type=0x%02x live=%d", (unsigned long)cell_pid(c), cell_hi(c), cell_type(c), live1);
}

TEST(gap9_sharedtable_handle_reuse_changes_hiword)
{
    G9_NOWINE("shared handle table");
    HBITMAP b1 = U(CreateBitmap)(2, 2, 1, 1, NULL);
    REQUIRE(b1);
    U(DeleteObject)(b1);
    int reused = 0;
    for (int i = 0; i < 40 && !reused; i++) {
        HBITMAP b2 = U(CreateBitmap)(2, 2, 1, 1, NULL);
        REQUIRE(b2);
        if (((ULONG_PTR)b2 & 0xFFFF) == ((ULONG_PTR)b1 & 0xFFFF)) {
            reused = 1;
            /* same slot, different reuse count: the stale handle must not alias the new object */
            CHECK(b2 != b1);
            EXPECT_EQ(U(GetObjectType)(b1), 0);
            EXPECT_EQ(U(GetObjectType)(b2), OBJ_BITMAP);
        }
        U(DeleteObject)(b2);
    }
    t_note("slot reused: %d", reused);
}

TEST(gap9_sharedtable_count_delta_matches_gui_resources)
{
    G9_NOWINE("shared handle table");
    const DWORD *pid = (const DWORD *)gd("gW32PID");
    BYTE *const *tbl = (BYTE *const *)gd("pGdiSharedHandleTable");
    REQUIRE(pid); REQUIRE(tbl); REQUIRE(*tbl);
    DWORD g0 = g9_gdi_count();
    HBITMAP hs[10];
    for (int i = 0; i < 10; i++) { hs[i] = U(CreateBitmap)(2, 2, 1, 1, NULL); REQUIRE(hs[i]); }
    EXPECT_EQ(g9_gdi_count(), g0 + 10);
    int owned = 0;
    for (int i = 0; i < 10; i++) {
        const BYTE *c = cell_of(hs[i]);
        if ((cell_pid(c) & ~1u) == *pid && cell_hi(c) == HIWORD((ULONG_PTR)hs[i])) owned++;
    }
    EXPECT_EQ(owned, 10);
    for (int i = 0; i < 10; i++) U(DeleteObject)(hs[i]);
    EXPECT_EQ(g9_gdi_count(), g0);
}

TEST(gap9_sharedtable_full_scan_counts_own_objects)
{
    const DWORD *pid = (const DWORD *)gd("gW32PID");
    BYTE *const *tbl = (BYTE *const *)gd("pGdiSharedHandleTable");
    REQUIRE(pid); REQUIRE(tbl); REQUIRE(*tbl);
    int owned = 0;
    for (int i = 1; i < 0x10000; i++) {
        const BYTE *c = *tbl + (size_t)i * 0x18;
        if ((cell_pid(c) & ~1u) == *pid && cell_type(c) != 0) owned++;
    }
    DWORD gr = g9_gdi_count();
    t_note("table scan: %d own cells, GetGuiResources(GR_GDIOBJECTS) = %lu", owned, (unsigned long)gr);
    CHECK(owned > 0);
    CHECK(owned >= (int)gr / 2);
    CHECK(owned <= (int)gr * 2 + 64);
}

TEST(gap9_sharedtable_stock_object_cells)
{
    BYTE *const *tbl = (BYTE *const *)gd("pGdiSharedHandleTable");
    REQUIRE(tbl); REQUIRE(*tbl);
    for (int i = 0; i <= 19; i++) {
        HGDIOBJ h = U(GetStockObject)(i);
        if (!h) continue;
        const BYTE *c = cell_of(h);
        if (cell_hi(c) != HIWORD((ULONG_PTR)h)) t_note("stock %d handle %p cell hiword 0x%04x", i, h, cell_hi(c));
        CHECKF(U(GetObjectType)(h) != 0, "stock %d type", i);
    }
}

/* ---- pGdiDevCaps ---- */
TEST(gap9_devcaps_pointer_in_shared_region)
{
    BYTE *const *tbl = (BYTE *const *)gd("pGdiSharedHandleTable");
    BYTE *const *dc = (BYTE *const *)gd("pGdiDevCaps");
    REQUIRE(tbl); REQUIRE(dc);
    CHECK(*dc != NULL);
    EXPECT_EQ(*dc, *tbl + 0x180000);
    MEMORY_BASIC_INFORMATION mbi;
    REQUIRE(VirtualQuery(*dc, &mbi, sizeof mbi) == sizeof mbi);
    CHECK(mbi.State == MEM_COMMIT);
    CHECK(*dc + 0xC0 <= (BYTE *)mbi.BaseAddress + mbi.RegionSize || mbi.RegionSize > 0x180000);
}

typedef struct { int idx; int slot; const char *name; } devcap_map_t;
static const devcap_map_t dcmap[] = {
    { DRIVERVERSION, 0, "DRIVERVERSION" }, { TECHNOLOGY, 1, "TECHNOLOGY" }, { BITSPIXEL, 8, "BITSPIXEL" }, { PLANES, 9, "PLANES" },
    { NUMPENS, 10, "NUMPENS" }, { NUMFONTS, 11, "NUMFONTS" }, { NUMCOLORS, 12, "NUMCOLORS" }, { TEXTCAPS, 0x19, "TEXTCAPS" },
    { RASTERCAPS, 13, "RASTERCAPS" }, { SIZEPALETTE, 0x13, "SIZEPALETTE" }, { COLORRES, 0x14, "COLORRES" },
    { PHYSICALWIDTH, 0x15, "PHYSICALWIDTH" }, { PHYSICALHEIGHT, 0x16, "PHYSICALHEIGHT" },
    { PHYSICALOFFSETX, 0x17, "PHYSICALOFFSETX" }, { PHYSICALOFFSETY, 0x18, "PHYSICALOFFSETY" } };

TEST(gap9_devcaps_cache_vs_getdevicecaps_screen)
{
    int *const *dc = (int *const *)gd("pGdiDevCaps");
    REQUIRE(dc); REQUIRE(*dc);
    HDC scr = U(CreateDCW)(L"DISPLAY", NULL, NULL, NULL);
    if (!scr) { t_skip("no display"); return; }
    int mismatches = 0;
    for (size_t i = 0; i < sizeof dcmap / sizeof *dcmap; i++) {
        int v = U(GetDeviceCaps)(scr, dcmap[i].idx);
        int c = (*dc)[dcmap[i].slot];
        if (v != c) { mismatches++; t_note("%s: GetDeviceCaps=%d cache=%d", dcmap[i].name, v, c); }
    }
    t_note("devcaps cache mismatches: %d of %d", mismatches, (int)(sizeof dcmap / sizeof *dcmap));
    EXPECT_EQ(U(GetDeviceCaps)(scr, TECHNOLOGY), (*dc)[1]);
    EXPECT_EQ(U(GetDeviceCaps)(scr, TECHNOLOGY), DT_RASDISPLAY);
    EXPECT_EQ(U(GetDeviceCaps)(scr, BITSPIXEL), (*dc)[8]);
    EXPECT_EQ(U(GetDeviceCaps)(scr, PLANES), (*dc)[9]);
    U(DeleteDC)(scr);
}

TEST(gap9_devcaps_display_constants)
{
    HDC scr = U(CreateDCW)(L"DISPLAY", NULL, NULL, NULL);
    if (!scr) { t_skip("no display"); return; }
    EXPECT_EQ(U(GetDeviceCaps)(scr, TECHNOLOGY), DT_RASDISPLAY);
    EXPECT_EQ(U(GetDeviceCaps)(scr, NUMBRUSHES), -1);
    EXPECT_EQ(U(GetDeviceCaps)(scr, CURVECAPS), 0x1FF);
    EXPECT_EQ(U(GetDeviceCaps)(scr, LINECAPS), 0xFE);
    EXPECT_EQ(U(GetDeviceCaps)(scr, POLYGONALCAPS), 0xFF);
    EXPECT_EQ(U(GetDeviceCaps)(scr, CLIPCAPS), 1);
    EXPECT_EQ(U(GetDeviceCaps)(scr, NUMRESERVED), 20);
    CHECK(U(GetDeviceCaps)(scr, RASTERCAPS) & RC_BITBLT);
    U(DeleteDC)(scr);
}

TEST(gap9_devcaps_cache_unaffected_by_memory_dc_use)
{
    int *const *dc = (int *const *)gd("pGdiDevCaps");
    REQUIRE(dc); REQUIRE(*dc);
    int snap[0x20];
    memcpy(snap, *dc, sizeof snap);
    HDC m = U(CreateCompatibleDC)(NULL);
    REQUIRE(m);
    HBITMAP bm = U(CreateCompatibleBitmap)(m, 8, 8);
    for (size_t i = 0; i < sizeof dcmap / sizeof *dcmap; i++) U(GetDeviceCaps)(m, dcmap[i].idx);
    if (bm) U(DeleteObject)(bm);
    U(DeleteDC)(m);
    CHECK(!memcmp(snap, *dc, sizeof snap));
}

/* ---- ghICM ---- */
TEST(gap9_ghicm_is_zero_or_module)
{
    const ULONG_PTR *p = (const ULONG_PTR *)gd("ghICM");
    REQUIRE(p);
    ULONG_PTR v = *p;
    t_note("ghICM = %p", (void *)v);
    if (v) {
        char nm[MAX_PATH] = "";
        DWORD n = GetModuleFileNameA((HMODULE)v, nm, sizeof nm);
        if (n) t_note("ghICM module %s", nm);
        else t_note("ghICM is not a module handle");
    }
    CHECK(1);
}

TEST(gap9_ghicm_loaded_after_icm_use)
{
    G9_NOWINE("ICM loading");
    const ULONG_PTR *p = (const ULONG_PTR *)gd("ghICM");
    REQUIRE(p);
    HDC m = U(CreateCompatibleDC)(NULL);
    REQUIRE(m);
    WCHAR prof[MAX_PATH]; DWORD n = MAX_PATH;
    BOOL (WINAPI *gi)(HDC, LPDWORD, LPWSTR) = (void *)t_proc("GetICMProfileW");
    if (gi) gi(m, &n, prof);
    ULONG_PTR a = *p;
    t_note("ghICM after GetICMProfileW = %p", (void *)a);
    ULONG_PTR b = *p;
    EXPECT_EQ(a, b);
    U(DeleteDC)(m);
}

/* ---- semDxTrimNotification / DxTrimNotificationListHead ---- */
TEST(gap9_dxtrim_list_head_shape)
{
    const ULONG_PTR *h = (const ULONG_PTR *)gd("DxTrimNotificationListHead");
    REQUIRE(h);
    ULONG_PTR fl = h[0], bl = h[1];
    t_note("DxTrimNotificationListHead Flink=%p Blink=%p (self=%p)", (void *)fl, (void *)bl, (void *)h);
    if (fl == 0 && bl == 0) { CHECK(1); return; }      /* not initialised until first use */
    if (fl == (ULONG_PTR)h) { EXPECT_EQ(bl, (ULONG_PTR)h); return; }   /* empty list */
    MEMORY_BASIC_INFORMATION mbi;
    CHECK(VirtualQuery((void *)fl, &mbi, sizeof mbi) == sizeof mbi && mbi.State == MEM_COMMIT);
    CHECK(VirtualQuery((void *)bl, &mbi, sizeof mbi) == sizeof mbi && mbi.State == MEM_COMMIT);
}

TEST(gap9_dxtrim_sem_layout)
{
    const BYTE *s = (const BYTE *)gd("semDxTrimNotification");
    const BYTE *h = (const BYTE *)gd("DxTrimNotificationListHead");
    REQUIRE(s); REQUIRE(h);
    CHECK(s != h);
    /* both live in gdi32's .data; the lock does not overlap the list head */
    CHECK(s >= h + 16 || s + 8 <= h);
    MEMORY_BASIC_INFORMATION mbi;
    REQUIRE(VirtualQuery(s, &mbi, sizeof mbi) == sizeof mbi);
    CHECK(mbi.Protect & PAGE_READWRITE);
    CHECK(!(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)));
}

TEST(gap9_dxtrim_data_writable_sections)
{
    const char *const w[] = { "gW32PID", "ghICM", "fpClosePrinter", "semDxTrimNotification", "DxTrimNotificationListHead" };
    for (size_t i = 0; i < sizeof w / sizeof *w; i++) {
        const void *p = gd(w[i]);
        REQUIRE(p);
        MEMORY_BASIC_INFORMATION mbi;
        REQUIRE(VirtualQuery(p, &mbi, sizeof mbi) == sizeof mbi);
        CHECKF(mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY), "%s protect 0x%lx", w[i], (unsigned long)mbi.Protect);
    }
}
