/* Additional export-table, loader and cross-thread checks. Table-only tests run anywhere; the rest need genuine modules. */
#include "exp.h"

/* ---------- table integrity (independent of the host OS) ---------- */
static void table_integrity(const exp_ent_t *t, int n, int expect_named_min)
{
    CHECK(n > 100);
    int named = 0;
    for (int i = 0; i < n; i++) {
        if (i) CHECKF(t[i].ord > t[i - 1].ord, "ordinal order at row %d (%u after %u)", i, t[i].ord, t[i - 1].ord);
        if (!t[i].name) { CHECK(!t[i].fwd); continue; }
        named++;
        CHECK(t[i].name[0] != 0);
        for (int k = 0; k < i; k++) if (t[k].name && !strcmp(t[k].name, t[i].name)) { CHECKF(0, "duplicate name %s", t[i].name); break; }
    }
    CHECKF(named >= expect_named_min, "named rows %d", named);
}
TEST(extra_table_integrity_gdi32) { table_integrity(exp_tab_gdi32, exp_tab_gdi32_n, 900); }
TEST(extra_table_integrity_gdi32full) { table_integrity(exp_tab_gdi32full, exp_tab_gdi32full_n, 1000); }
TEST(extra_table_integrity_win32u) { table_integrity(exp_tab_win32u, exp_tab_win32u_n, 1300); }

TEST(extra_table_ordinal_only_counts)
{
    int a = 0, b = 0;
    for (int i = 0; i < exp_tab_gdi32_n; i++) if (!exp_tab_gdi32[i].name) a++;
    for (int i = 0; i < exp_tab_gdi32full_n; i++) if (!exp_tab_gdi32full[i].name) b++;
    EXPECT_EQ(a, 14);
    EXPECT_EQ(b, 10);
}

TEST(extra_table_forwarder_strings_wellformed)
{
    for (int m = 0; m < M_COUNT; m++) {
        const exp_ent_t *t = mod_tab(m);
        for (int i = 0; i < mod_tab_n(m); i++) {
            if (!t[i].fwd) continue;
            const char *dot = strchr(t[i].fwd, '.');
            CHECKF(dot && dot > t[i].fwd && dot[1], "%s: bad forwarder \"%s\"", t[i].name, t[i].fwd);
            if (!dot) continue;
            CHECKF(!strchr(dot + 1, '.'), "%s: extra dot in \"%s\"", t[i].name, t[i].fwd);
            CHECKF(strlen(t[i].fwd) < 120, "%s: long forwarder", t[i].name);
        }
    }
}

TEST(extra_table_win32u_forwarders_target_win32u_exports)
{
    int cnt = 0;
    for (int i = 0; i < exp_tab_gdi32_n; i++) {
        const char *f = exp_tab_gdi32[i].fwd;
        if (!f || strncmp(f, "WIN32U.", 7)) continue;
        int found = 0;
        for (int k = 0; k < exp_tab_win32u_n; k++) if (!strcmp(exp_tab_win32u[k].name, f + 7)) { found = 1; break; }
        CHECKF(found, "%s -> %s is not a win32u export", exp_tab_gdi32[i].name, f);
        CHECKF(!strncmp(f + 7, "Nt", 2), "%s -> %s is not an Nt service", exp_tab_gdi32[i].name, f);
        cnt++;
    }
    EXPECT_EQ(cnt, 208);
}

TEST(extra_table_win32u_names_are_nt)
{
    int odd = 0;
    for (int i = 0; i < exp_tab_win32u_n; i++)
        if (strncmp(exp_tab_win32u[i].name, "Nt", 2)) { odd++; t_note("win32u exports non-Nt symbol %s", exp_tab_win32u[i].name); }
    CHECK(odd <= 2);
}

TEST(extra_table_gdi32full_forwards_only_to_gdi32)
{
    for (int i = 0; i < exp_tab_gdi32full_n; i++) {
        const char *f = exp_tab_gdi32full[i].fwd;
        if (!f) continue;
        CHECKF(!strncmp(f, "GDI32.", 6), "%s -> %s", exp_tab_gdi32full[i].name, f);
        /* the target symbol exists in the gdi32 table */
        int found = 0;
        for (int k = 0; k < exp_tab_gdi32_n; k++) if (exp_tab_gdi32[k].name && !strcmp(exp_tab_gdi32[k].name, f + 6)) found = 1;
        CHECKF(found, "%s target %s missing from gdi32", exp_tab_gdi32full[i].name, f);
    }
}

TEST(extra_table_gdi32_apiset_forwarders)
{
    int n = 0;
    for (int i = 0; i < exp_tab_gdi32_n; i++) {
        const char *f = exp_tab_gdi32[i].fwd;
        if (!f || strncmp(f, "ext-ms-win-", 11)) continue;
        n++;
        CHECKF(strstr(f, "-l1-"), "%s: apiset name without contract version: %s", exp_tab_gdi32[i].name, f);
    }
    EXPECT_EQ(n, 54);
}

TEST(extra_table_d3dkmt_not_in_gdi32full)
{
    for (int i = 0; i < exp_tab_gdi32full_n; i++)
        if (exp_tab_gdi32full[i].name) CHECKF(strncmp(exp_tab_gdi32full[i].name, "D3DKMT", 6), "%s", exp_tab_gdi32full[i].name);
}

/* ---------- module-level checks ---------- */
TEST(extra_gdi32full_has_no_d3dkmt_exports)
{
    NEED_TAB();
    HMODULE g = mod_handle(M_GDI32), f = mod_handle(M_GDI32FULL);
    REQUIRE(g && f);
    int n = 0;
    for (int i = 0; i < exp_tab_gdi32_n; i++) {
        const char *nm = exp_tab_gdi32[i].name;
        if (!nm || strncmp(nm, "D3DKMT", 6)) continue;
        SetLastError(0);
        CHECKF(GetProcAddress(f, nm) == NULL, "gdi32full exports %s", nm);
        EXPECT_ERR(0, ERROR_PROC_NOT_FOUND);
        n++;
    }
    CHECK(n > 200);
}

TEST(extra_ordinal_and_name_calls_agree)
{
    NEED_TAB();
    HMODULE g = mod_handle(M_GDI32);
    REQUIRE(g);
    static const char *const names[] = { "GetStockObject", "GetObjectType", "GdiFlush", "GetDeviceCaps" };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        unsigned ord = 0;
        for (int k = 0; k < exp_tab_gdi32_n; k++) if (exp_tab_gdi32[k].name && !strcmp(exp_tab_gdi32[k].name, names[i])) ord = exp_tab_gdi32[k].ord;
        CHECKF(ord != 0, "%s not in table", names[i]);
        if (!ord) continue;
        FARPROC a = GetProcAddress(g, names[i]), b = GetProcAddress(g, (LPCSTR)(ULONG_PTR)ord);
        CHECKF(a && a == b, "%s: name %p ord %p", names[i], (void *)a, (void *)b);
    }
    HGDIOBJ (WINAPI *gso)(int) = (void *)GetProcAddress(g, "GetStockObject");
    HGDIOBJ (WINAPI *gso2)(int) = NULL;
    for (int k = 0; k < exp_tab_gdi32_n; k++) if (exp_tab_gdi32[k].name && !strcmp(exp_tab_gdi32[k].name, "GetStockObject")) gso2 = (void *)GetProcAddress(g, (LPCSTR)(ULONG_PTR)exp_tab_gdi32[k].ord);
    REQUIRE(gso && gso2);
    for (int i = 0; i <= 19; i++) EXPECT_EQ(gso(i), gso2(i));
}

TEST(extra_datafile_nonexistent_file)
{
    SetLastError(0);
    HMODULE h = LoadLibraryExW(L"C:\\no_such_dir_fwd_test\\gdi32.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECK(h == NULL);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_FILE_NOT_FOUND, "err %lu", e);
    SetLastError(0);
    h = LoadLibraryExW(L"C:\\no_such_gdi32_fwd_test.dll", NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    CHECK(h == NULL);
    e = GetLastError();
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_FILE_NOT_FOUND, "err %lu", e);
}

TEST(extra_loadlibrary_empty_and_null_names)
{
    SetLastError(0);
    CHECK(LoadLibraryW(L"") == NULL);
    t_note("LoadLibraryW(\"\") err=%lu", GetLastError());
    SetLastError(0);
    CHECK(GetModuleHandleW(L"gdi32.dll\\") == NULL);
    t_note("GetModuleHandleW(\"gdi32.dll\\\") err=%lu", GetLastError());
}

TEST(extra_getprocaddress_long_name)
{
    HMODULE g = mod_handle(M_GDI32);
    REQUIRE(g);
    char *buf = (char *)HeapAlloc(GetProcessHeap(), 0, 4097);
    REQUIRE(buf);
    memset(buf, 'A', 4096); buf[4096] = 0;
    SetLastError(0);
    CHECK(GetProcAddress(g, buf) == NULL);
    EXPECT_ERR(0, ERROR_PROC_NOT_FOUND);
    HeapFree(GetProcessHeap(), 0, buf);
}

TEST(extra_stub_bytes_not_padding)
{
    NEED_TAB();
    HMODULE g = mod_handle(M_GDI32), f = mod_handle(M_GDI32FULL);
    REQUIRE(g && f);
    for (int m = 0; m < 2; m++) {
        HMODULE h = m ? f : g;
        const exp_ent_t *t = m ? exp_tab_gdi32full : exp_tab_gdi32;
        int n = m ? exp_tab_gdi32full_n : exp_tab_gdi32_n, jumps = 0, code = 0;
        for (int i = 0; i < n; i++) {
            if (t[i].fwd) continue;
            const BYTE *p = (const BYTE *)GetProcAddress(h, t[i].name ? t[i].name : (LPCSTR)(ULONG_PTR)t[i].ord);
            if (!p) continue;
            if (t[i].name && !m) {
                static const char *const data[] = { "DxTrimNotificationListHead", "GdiBatchLimit", "LpkEditControl", "LpkpEditControlSize", "fpClosePrinter",
                    "gMaxGdiHandleCount", "gW32PID", "g_systemCallFilterId", "ghICM", "pGdiDevCaps", "pGdiSharedHandleTable", "pGdiSharedMemory", "semDxTrimNotification" };
                int isd = 0;
                for (size_t k = 0; k < sizeof data / sizeof *data; k++) if (!strcmp(data[k], t[i].name)) isd = 1;
                if (isd) continue;
            }
            code++;
            CHECKF(!(p[0] == 0 && p[1] == 0), "%s!%s starts with zero bytes", mod_name[m], t[i].name ? t[i].name : "<ordinal>");
            CHECKF(!(p[0] == 0xCC && p[1] == 0xCC), "%s!%s starts with int3 padding", mod_name[m], t[i].name ? t[i].name : "<ordinal>");
            if (p[0] == 0xFF && p[1] == 0x25) jumps++;
            else if (p[0] == 0x48 && p[1] == 0xFF && p[2] == 0x25) jumps++;
        }
        t_note("%s: %d code exports, %d begin with an indirect jump", mod_name[m], code, jumps);
        CHECK(code > 500);
    }
}

/* ---------- GDI handle layout ---------- */
static unsigned htype(HGDIOBJ h) { return (unsigned)(((ULONG_PTR)h >> 16) & 0x7F); }
static int hstock(HGDIOBJ h) { return (((ULONG_PTR)h >> 16) & 0x80) != 0; }

TEST(extra_handle_type_bits)
{
    if (is_wine()) { t_skip("wine"); return; }
    HBRUSH (WINAPI *cb)(COLORREF) = (void *)t_proc("CreateSolidBrush");
    HBITMAP (WINAPI *cbm)(int, int, UINT, UINT, const void *) = (void *)t_proc("CreateBitmap");
    HRGN (WINAPI *cr)(int, int, int, int) = (void *)t_proc("CreateRectRgn");
    HDC (WINAPI *cdc)(HDC) = (void *)t_proc("CreateCompatibleDC");
    BOOL (WINAPI *dd)(HDC) = (void *)t_proc("DeleteDC");
    BOOL (WINAPI *del)(HGDIOBJ) = (void *)t_proc("DeleteObject");
    REQUIRE(cb && cbm && cr && cdc && dd && del);
    HGDIOBJ br = cb(RGB(1, 2, 3)), bm = cbm(2, 2, 1, 32, NULL), rg = cr(0, 0, 3, 3);
    HDC dc = cdc(NULL);
    CHECK(br && bm && rg && dc);
    EXPECT_EQ(htype(dc), 0x01);
    EXPECT_EQ(htype(rg), 0x04);
    EXPECT_EQ(htype(bm), 0x05);
    EXPECT_EQ(htype(br), 0x10);
    if (hstock(br) || hstock(bm) || hstock(rg) || hstock(dc)) t_note("a user-created handle has bit 23 set");
    t_note("handles: dc=%p rgn=%p bitmap=%p brush=%p", dc, rg, bm, br);
    CHECK(br != bm && bm != rg);
    del(br); del(bm); del(rg); dd(dc);
}

TEST(extra_stock_handles_have_stock_bit)
{
    if (is_wine()) { t_skip("wine"); return; }
    HGDIOBJ (WINAPI *gso)(int) = (void *)t_proc("GetStockObject");
    REQUIRE(gso);
    for (int i = 0; i <= 19; i++) {
        if (i == 9) continue;
        HGDIOBJ h = gso(i);
        CHECKF(h != NULL, "stock %d", i);
        if (h && !hstock(h)) t_note("stock object %d handle %p lacks bit 23", i, h);
    }
    CHECK(htype(gso(WHITE_BRUSH)) == 0x10);
    CHECK(htype(gso(DEFAULT_PALETTE)) == 0x08);
}

TEST(extra_handle_reuse_changes_upper_bits)
{
    HBRUSH (WINAPI *cb)(COLORREF) = (void *)t_proc("CreateSolidBrush");
    BOOL (WINAPI *del)(HGDIOBJ) = (void *)t_proc("DeleteObject");
    DWORD (WINAPI *got)(HGDIOBJ) = (void *)t_proc("GetObjectType");
    REQUIRE(cb && del && got);
    HGDIOBJ first = cb(RGB(9, 9, 9));
    CHECK(del(first));
    EXPECT_EQ(got(first), 0);
    /* a stale handle stays invalid even after its slot is reused */
    int reused = 0;
    HGDIOBJ list[40];
    for (int i = 0; i < 40; i++) { list[i] = cb(RGB(i, 0, 0)); if (((ULONG_PTR)list[i] & 0xFFFF) == ((ULONG_PTR)first & 0xFFFF)) { reused = 1; CHECK(list[i] != first); } }
    EXPECT_EQ(got(first), 0);
    for (int i = 0; i < 40; i++) del(list[i]);
    t_note("handle slot reused: %d", reused);
}

/* ---------- threads ---------- */
static DWORD WINAPI flush_thread(LPVOID p)
{
    BOOL (WINAPI *fl)(void) = (void *)t_proc("GdiFlush");
    LONG *fail = (LONG *)p;
    for (int i = 0; i < 100; i++) if (!fl()) (*fail)++;
    return 0;
}
TEST(extra_gdiflush_from_threads)
{
    REQUIRE(t_proc("GdiFlush"));
    LONG fail[4] = { 0 };
    HANDLE th[4];
    for (int i = 0; i < 4; i++) th[i] = CreateThread(NULL, 0, flush_thread, &fail[i], 0, NULL);
    for (int i = 0; i < 4; i++) CHECK(th[i] != NULL);
    CHECK(WaitForMultipleObjects(4, th, TRUE, 30000) == WAIT_OBJECT_0);
    for (int i = 0; i < 4; i++) { if (th[i]) CloseHandle(th[i]); EXPECT_EQ(fail[i], 0); }
}

TEST(extra_batchlimit_roundtrip)
{
    DWORD (WINAPI *gb)(void) = (void *)t_proc("GdiGetBatchLimit");
    DWORD (WINAPI *sb)(DWORD) = (void *)t_proc("GdiSetBatchLimit");
    REQUIRE(gb && sb);
    DWORD orig = gb();
    DWORD prev = sb(orig ? orig : 1);
    t_note("original limit %lu, SetBatchLimit returned %lu, now %lu", orig, prev, gb());
    CHECK(prev == orig);
    DWORD cur = gb();
    if (orig) EXPECT_EQ(cur, orig);
    sb(orig);    /* leave the process as found */
    CHECK(gb() == orig || orig == 0);
}

static DWORD WINAPI churn_thread(LPVOID p)
{
    HBRUSH (WINAPI *cb)(COLORREF) = (void *)t_proc("CreateSolidBrush");
    BOOL (WINAPI *del)(HGDIOBJ) = (void *)t_proc("DeleteObject");
    DWORD (WINAPI *got)(HGDIOBJ) = (void *)t_proc("GetObjectType");
    LONG *fail = (LONG *)p;
    HGDIOBJ b[200];
    for (int round = 0; round < 5; round++) {
        for (int i = 0; i < 200; i++) { b[i] = cb(RGB(i, round, 0)); if (!b[i] || got(b[i]) != OBJ_BRUSH) (*fail)++; }
        for (int i = 0; i < 200; i++) { if (b[i] && !del(b[i])) (*fail)++; if (b[i] && got(b[i]) != 0) (*fail)++; }
    }
    return 0;
}
TEST(extra_handle_churn_8_threads)
{
    DWORD (WINAPI *ggr)(HANDLE, DWORD) = (void *)t_proc("GetGuiResources");
    REQUIRE(ggr && t_proc("CreateSolidBrush") && t_proc("DeleteObject") && t_proc("GetObjectType"));
    DWORD before = ggr(GetCurrentProcess(), GR_GDIOBJECTS);
    LONG fail[8] = { 0 };
    HANDLE th[8];
    for (int i = 0; i < 8; i++) th[i] = CreateThread(NULL, 0, churn_thread, &fail[i], 0, NULL);
    for (int i = 0; i < 8; i++) CHECK(th[i] != NULL);
    CHECK(WaitForMultipleObjects(8, th, TRUE, 60000) == WAIT_OBJECT_0);
    for (int i = 0; i < 8; i++) { if (th[i]) CloseHandle(th[i]); EXPECT_EQ(fail[i], 0); }
    DWORD after = ggr(GetCurrentProcess(), GR_GDIOBJECTS);
    if (before) EXPECT_EQ(after, before);
    DWORD peak = ggr(GetCurrentProcess(), 2);
    if (before) CHECK(peak >= before + 200);
}

typedef struct { HBITMAP bm; HGDIOBJ got; HDC dc; } sel_t;
static DWORD WINAPI select_thread(LPVOID p)
{
    sel_t *s = (sel_t *)p;
    HDC (WINAPI *cdc)(HDC) = (void *)t_proc("CreateCompatibleDC");
    HGDIOBJ (WINAPI *so)(HDC, HGDIOBJ) = (void *)t_proc("SelectObject");
    s->dc = cdc(NULL);
    s->got = so(s->dc, s->bm);
    return 0;
}
TEST(extra_bitmap_single_dc_across_threads)
{
    HDC (WINAPI *cdc)(HDC) = (void *)t_proc("CreateCompatibleDC");
    BOOL (WINAPI *dd)(HDC) = (void *)t_proc("DeleteDC");
    HGDIOBJ (WINAPI *so)(HDC, HGDIOBJ) = (void *)t_proc("SelectObject");
    HBITMAP (WINAPI *cbm)(int, int, UINT, UINT, const void *) = (void *)t_proc("CreateBitmap");
    BOOL (WINAPI *del)(HGDIOBJ) = (void *)t_proc("DeleteObject");
    REQUIRE(cdc && dd && so && cbm && del);
    HDC dc = cdc(NULL);
    HBITMAP bm = cbm(8, 8, 1, 32, NULL);
    REQUIRE(dc && bm);
    HGDIOBJ old = so(dc, bm);
    CHECK(old != NULL);
    sel_t s = { bm, (HGDIOBJ)1, NULL };
    HANDLE th = CreateThread(NULL, 0, select_thread, &s, 0, NULL);
    CHECK(th != NULL);
    if (th) { CHECK(WaitForSingleObject(th, 30000) == WAIT_OBJECT_0); CloseHandle(th); }
    /* a bitmap lives in only one DC at a time, regardless of the thread asking */
    CHECK(s.got == NULL);
    if (s.dc) dd(s.dc);
    so(dc, old);
    th = CreateThread(NULL, 0, select_thread, &s, 0, NULL);
    if (th) { WaitForSingleObject(th, 30000); CloseHandle(th); }
    CHECK(s.got != NULL);          /* free again after deselect */
    if (s.dc) dd(s.dc);
    dd(dc);
    del(bm);
}

/* ---------- D3DKMT determinism ---------- */
typedef LONG (WINAPI *kmt_fn2)(void *);
TEST(extra_d3dkmt_status_deterministic)
{
    HMODULE g = mod_handle(M_GDI32);
    REQUIRE(g);
    static const char *const names[] = { "D3DKMTCloseAdapter", "D3DKMTQueryAdapterInfo", "D3DKMTDestroyDevice", "D3DKMTGetDeviceState", "D3DKMTEscape" };
    static BYTE blk[4096];
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        kmt_fn2 f = (kmt_fn2)GetProcAddress(g, names[i]);
        if (!f) { t_skip("missing %s", names[i]); continue; }
        memset(blk, 0, sizeof blk);
        LONG a = f(blk);
        memset(blk, 0, sizeof blk);
        LONG b = f(blk);
        CHECKF(a == b, "%s: 0x%08lx then 0x%08lx", names[i], (unsigned long)a, (unsigned long)b);
        CHECKF(a < 0, "%s(zeroed) = 0x%08lx", names[i], (unsigned long)a);
        /* the input block is not modified by a call that fails argument validation */
        BYTE nz = 0;
        for (int k = 0; k < (int)sizeof blk; k++) nz |= blk[k];
        t_note("%s zero-block status 0x%08lx, block %s", names[i], (unsigned long)a, nz ? "modified" : "untouched");
    }
}

TEST(extra_d3dkmt_ordinal_vs_name)
{
    NEED_TAB();
    HMODULE g = mod_handle(M_GDI32);
    REQUIRE(g);
    int n = 0;
    for (int i = 0; i < exp_tab_gdi32_n; i++) {
        const char *nm = exp_tab_gdi32[i].name;
        if (!nm || strncmp(nm, "D3DKMT", 6)) continue;
        FARPROC a = GetProcAddress(g, nm), b = GetProcAddress(g, (LPCSTR)(ULONG_PTR)exp_tab_gdi32[i].ord);
        CHECKF(a && a == b, "%s", nm);
        n++;
    }
    CHECK(n > 200);
}
