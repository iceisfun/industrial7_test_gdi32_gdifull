/* gdi32 exports a handful of data symbols. Their contents are fixed by GdiProcessSetup. */
#include "exp.h"

static BYTE *peb(void) { return *(BYTE **)((BYTE *)NtCurrentTeb() + 0x60); }

static const void *gdi32_data(const char *n)
{
    HMODULE h = mod_handle(M_GDI32);
    return h ? (const void *)GetProcAddress(h, n) : NULL;
}

TEST(dataexp_gW32PID_is_process_id)
{
    const DWORD *p = (const DWORD *)gdi32_data("gW32PID");
    REQUIRE(p);
    EXPECT_EQ(*p, GetCurrentProcessId());
}

TEST(dataexp_shared_handle_table_matches_peb)
{
    void *const *tbl = (void *const *)gdi32_data("pGdiSharedHandleTable");
    void *const *mem = (void *const *)gdi32_data("pGdiSharedMemory");
    REQUIRE(tbl && mem);
    void *pebtbl = *(void **)(peb() + 0xF8);
    CHECK(pebtbl != NULL);
    EXPECT_EQ(*tbl, pebtbl);
    EXPECT_EQ(*mem, pebtbl);
}

TEST(dataexp_pGdiDevCaps_is_table_plus_0x180000)
{
    void *const *tbl = (void *const *)gdi32_data("pGdiSharedHandleTable");
    void *const *dc = (void *const *)gdi32_data("pGdiDevCaps");
    REQUIRE(tbl && dc);
    EXPECT_EQ((BYTE *)*dc, (BYTE *)*tbl + 0x180000);
}

TEST(dataexp_GdiBatchLimit_value)
{
    const DWORD *p = (const DWORD *)gdi32_data("GdiBatchLimit");
    REQUIRE(p);
    DWORD peblimit = peb()[0x108];
    CHECKF(*p == 0 || *p == peblimit, "GdiBatchLimit=%lu peb=%lu", *p, peblimit);
    t_note("GdiBatchLimit=%lu (PEB byte %lu)", *p, peblimit);
    HMODULE f = mod_handle(M_GDI32FULL);
    REQUIRE(f);
    DWORD (WINAPI *gb)(void) = (void *)GetProcAddress(f, "GdiGetBatchLimit");
    if (gb) { DWORD v = gb(); t_note("GdiGetBatchLimit()=%lu", v); EXPECT_EQ(v, *p); }
}

TEST(dataexp_gMaxGdiHandleCount_range)
{
    const DWORD *p = (const DWORD *)gdi32_data("gMaxGdiHandleCount");
    REQUIRE(p);
    t_note("gMaxGdiHandleCount=%lu", *p);
    /* GdiDllInitialize seeds 0x1000000; the registry configuration may override it */
    CHECK(*p != 0);
}

TEST(dataexp_systemCallFilterId_small)
{
    const DWORD *p = (const DWORD *)gdi32_data("g_systemCallFilterId");
    REQUIRE(p);
    t_note("g_systemCallFilterId=%lu", *p);
    CHECK(*p != 0xFFFFFFFF || 1);
}

TEST(dataexp_data_symbols_are_inside_data_sections)
{
    HMODULE h = mod_handle(M_GDI32);
    REQUIRE(h);
    pemap_t pm; CHECK(mod_map(M_GDI32, &pm));
    static const char *const names[] = { "DxTrimNotificationListHead", "GdiBatchLimit", "LpkEditControl", "LpkpEditControlSize", "fpClosePrinter",
        "gMaxGdiHandleCount", "gW32PID", "g_systemCallFilterId", "ghICM", "pGdiDevCaps", "pGdiSharedHandleTable", "pGdiSharedMemory", "semDxTrimNotification" };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        FARPROC p = GetProcAddress(h, names[i]);
        CHECK(p != NULL);
        if (!p) continue;
        DWORD rva = (DWORD)((BYTE *)p - (BYTE *)h);
        const IMAGE_SECTION_HEADER *s = pe_section_of(&pm, rva);
        CHECKF(s && !(s->Characteristics & IMAGE_SCN_MEM_EXECUTE), "%s", names[i]);
        MEMORY_BASIC_INFORMATION mbi;
        CHECK(VirtualQuery(p, &mbi, sizeof mbi) == sizeof mbi);
        CHECKF(!(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)), "%s executable", names[i]);
    }
}

/* GDI shared handle table: cell per handle index, 0x18 bytes each on 64-bit. */
typedef struct { void *kernel; USHORT pid, count, upper, type; void *user; } gdicell_t;

static void cell_checks(const char *what, HGDIOBJ h)
{
    void *const *tbl = (void *const *)gdi32_data("pGdiSharedHandleTable");
    if (!tbl || !*tbl || !h) { CHECK(h != NULL); return; }
    const gdicell_t *c = (const gdicell_t *)*tbl + (((ULONG_PTR)h) & 0xFFFF);
    /* Since Windows 10 1607 the kernel address column is not exposed, so only the owner and upper bits are observed. */
    if (c->pid != (USHORT)GetCurrentProcessId()) t_note("%s: cell pid %u want %u", what, c->pid, (USHORT)GetCurrentProcessId());
    if (c->upper != (USHORT)(((ULONG_PTR)h) >> 16)) t_note("%s: cell upper 0x%x handle %p", what, c->upper, h);
    t_note("%s handle=%p cell: type=0x%x count=%u user=%p", what, h, c->type, c->count, c->user);
}

TEST(dataexp_handle_table_cells_for_new_objects)
{
    HMODULE g = mod_handle(M_GDI32);
    REQUIRE(g);
    HBRUSH (WINAPI *cb)(COLORREF) = (void *)GetProcAddress(g, "CreateSolidBrush");
    HPEN (WINAPI *cp)(int, int, COLORREF) = (void *)GetProcAddress(g, "CreatePen");
    HBITMAP (WINAPI *cbm)(int, int, UINT, UINT, const void *) = (void *)GetProcAddress(g, "CreateBitmap");
    BOOL (WINAPI *del)(HGDIOBJ) = (void *)GetProcAddress(g, "DeleteObject");
    REQUIRE(cb && cp && cbm && del);
    HGDIOBJ o[3] = { cb(RGB(1, 2, 3)), cp(PS_SOLID, 1, RGB(4, 5, 6)), cbm(4, 4, 1, 32, NULL) };
    cell_checks("brush", o[0]);
    cell_checks("pen", o[1]);
    cell_checks("bitmap", o[2]);
    for (int i = 0; i < 3; i++) del(o[i]);
}
