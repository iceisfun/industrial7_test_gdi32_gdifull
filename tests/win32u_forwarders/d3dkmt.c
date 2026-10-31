/* D3DKMT* entry points exported by gdi32.dll, called only with invalid arguments, plus one safe success path. */
#include "exp.h"

typedef LONG (WINAPI *kmt_fn)(void *);
#define NT_ERR(s) ((LONG)(s) < 0)

static kmt_fn kmt(const char *name)
{
    HMODULE g = mod_handle(M_GDI32);
    if (!g && is_wine()) g = LoadLibraryA("gdi32.dll");
    return g ? (kmt_fn)GetProcAddress(g, name) : NULL;
}

/* One call with NULL, a zeroed block, and a block whose first dword is a bogus handle. */
static void invalid_arg_checks(const char *name, int hard_zero)
{
    kmt_fn f = kmt(name);
    if (!f) { t_skip("missing export %s", name); return; }
    static BYTE zero[8192] __attribute__((aligned(16)));
    LONG st = f(NULL);
    CHECKF(NT_ERR(st), "%s(NULL) = 0x%08lx", name, st);
    t_note("%s(NULL) = 0x%08lx", name, (unsigned long)st);
    memset(zero, 0, sizeof zero);
    st = f(zero);
    if (hard_zero) CHECKF(NT_ERR(st), "%s(zeroed) = 0x%08lx", name, st);
    t_note("%s(zeroed) = 0x%08lx", name, (unsigned long)st);
    memset(zero, 0, sizeof zero);
    *(DWORD *)zero = 0xDEADBEEF;
    st = f(zero);
    if (hard_zero) CHECKF(NT_ERR(st), "%s(bogus handle) = 0x%08lx", name, st);
    t_note("%s(bogus handle) = 0x%08lx", name, (unsigned long)st);
    /* the input block is not scribbled on beyond what the API documents: first dword is preserved by handle-in calls */
}
#define KT(n, hard) TEST(d3dkmt_invalid_##n) { invalid_arg_checks("D3DKMT" #n, hard); }
KT(CloseAdapter, 1)
KT(QueryAdapterInfo, 1)
KT(CreateDevice, 1)
KT(DestroyDevice, 1)
KT(CreateContext, 1)
KT(DestroyContext, 1)
KT(Escape, 1)
KT(CheckVidPnExclusiveOwnership, 1)
KT(GetDeviceState, 1)
KT(WaitForVerticalBlankEvent, 1)
KT(CheckOcclusion, 0)
KT(DestroyAllocation, 1)
KT(DestroySynchronizationObject, 1)
KT(Lock, 1)
KT(Unlock, 1)
KT(Render, 1)
KT(Present, 1)
KT(SetQueuedLimit, 1)
KT(GetScanLine, 1)
KT(GetRuntimeData, 0)
KT(GetSharedPrimaryHandle, 1)
KT(Evict, 1)
KT(MakeResident, 1)
KT(QueryResourceInfo, 1)
KT(QueryAllocationResidency, 1)
KT(SetAllocationPriority, 1)
KT(GetAllocationPriority, 1)
KT(SetContextSchedulingPriority, 1)
KT(GetContextSchedulingPriority, 1)
KT(CheckSharedResourceAccess, 1)
KT(GetDisplayModeList, 1)
KT(CheckMonitorPowerState, 1)
KT(GetPresentHistory, 0)
KT(GetOverlayState, 1)
KT(DestroyOverlay, 1)
KT(FlipOverlay, 1)
KT(GetMultisampleMethodList, 1)
KT(CreateOverlay, 1)
KT(CheckExclusiveOwnership, 0)
KT(GetDWMVerticalBlankEvent, 0)
KT(GetSharedResourceAdapterLuid, 1)
KT(OpenSynchronizationObject, 1)
KT(QueryVideoMemoryInfo, 1)
KT(PollDisplayChildren, 0)
KT(CreateDCFromMemory, 1)
KT(DestroyDCFromMemory, 1)

/* Exact statuses for the user-mode implementations that validate their own arguments. */
TEST(d3dkmt_createsynchronizationobject_validation)
{
    kmt_fn f = kmt("D3DKMTCreateSynchronizationObject");
    REQUIRE(f);
    EXPECT_EQ((ULONG)f(NULL), 0xC000000D);
    static BYTE blk[256] __attribute__((aligned(16)));
    memset(blk, 0, sizeof blk);          /* Info.Type = 0: not a valid synchronization object type */
    EXPECT_EQ((ULONG)f(blk), 0xC000000D);
    memset(blk, 0, sizeof blk);
    *(DWORD *)(blk + 4) = 99;
    EXPECT_EQ((ULONG)f(blk), 0xC000000D);
    memset(blk, 0, sizeof blk);
    *(DWORD *)(blk + 4) = 1;             /* semaphore with bogus device handle */
    LONG st = f(blk);
    CHECKF(NT_ERR(st), "0x%08lx", st);
}

TEST(d3dkmt_signal_wait_null)
{
    kmt_fn s = kmt("D3DKMTSignalSynchronizationObject"), w = kmt("D3DKMTWaitForSynchronizationObject");
    REQUIRE(s && w);
    EXPECT_EQ((ULONG)s(NULL), 0xC000000D);
    EXPECT_EQ((ULONG)w(NULL), 0xC000000D);
    static BYTE blk[1024] __attribute__((aligned(16)));
    memset(blk, 0, sizeof blk);
    LONG st = s(blk);
    CHECKF(NT_ERR(st), "signal(zero) = 0x%08lx", st);
    memset(blk, 0, sizeof blk);
    st = w(blk);
    CHECKF(NT_ERR(st), "wait(zero) = 0x%08lx", st);
    memset(blk, 0, sizeof blk);
    *(DWORD *)blk = 0xDEADBEEF; *(DWORD *)(blk + 4) = 0;   /* bogus device handle, zero objects */
    st = w(blk);
    CHECKF(NT_ERR(st), "wait(bogus, count 0) = 0x%08lx", st);
    st = s(blk);
    CHECKF(NT_ERR(st), "signal(bogus, count 0) = 0x%08lx", st);
}

TEST(d3dkmt_openresource_null)
{
    kmt_fn f = kmt("D3DKMTOpenResource");
    REQUIRE(f);
    EXPECT_EQ((ULONG)f(NULL), 0xC000000D);
}

TEST(d3dkmt_openadapter_from_displayname_bogus)
{
    kmt_fn f = kmt("D3DKMTOpenAdapterFromGdiDisplayName");
    REQUIRE(f);
    static BYTE blk[128] __attribute__((aligned(16)));
    memset(blk, 0, sizeof blk);
    wchar_t *nm = (wchar_t *)blk;
    lstrcpyW(nm, L"\\\\.\\NoSuchGdiDisplay99");
    LONG st = f(blk);
    CHECKF(NT_ERR(st), "0x%08lx", st);
    t_note("D3DKMTOpenAdapterFromGdiDisplayName(bogus) = 0x%08lx", (unsigned long)st);
    CHECKF((ULONG)st == 0xC0000001 || NT_ERR(st), "unexpected 0x%08lx", st);
}

TEST(d3dkmt_registertrim_functions_exported)
{
    HMODULE g = mod_handle(M_GDI32);
    REQUIRE(g);
    CHECK(GetProcAddress(g, "D3DKMTRegisterTrimNotification") != NULL);
    CHECK(GetProcAddress(g, "D3DKMTUnregisterTrimNotification") != NULL);
}

/* ---- safe success path: open the adapter behind a screen DC, close it ---- */
typedef struct { HDC hdc; DWORD hadapter; LUID luid; DWORD vidpn; } openhdc_t;
typedef struct { DWORD hadapter; } closeadapter_t;

TEST(d3dkmt_openadapter_fromhdc_screen_roundtrip)
{
    kmt_fn open = kmt("D3DKMTOpenAdapterFromHdc"), close = kmt("D3DKMTCloseAdapter");
    HDC (WINAPI *gd)(HWND) = (void *)t_proc("GetDC");
    int (WINAPI *rd)(HWND, HDC) = (void *)t_proc("ReleaseDC");
    REQUIRE(open && close && gd && rd);
    HDC dc = gd(NULL);
    REQUIRE(dc);
    openhdc_t o; memset(&o, 0, sizeof o);
    o.hdc = dc;
    LONG st = open(&o);
    t_note("D3DKMTOpenAdapterFromHdc(screen) = 0x%08lx hAdapter=0x%lx luid=%08lx:%08lx vidpn=%lu", (unsigned long)st, o.hadapter,
           (unsigned long)o.luid.HighPart, (unsigned long)o.luid.LowPart, o.vidpn);
    EXPECT_EQ(o.hdc, dc);   /* the DC member is always restored */
    if (st < 0) { rd(NULL, dc); t_skip("no WDDM adapter behind the screen DC (status 0x%08lx)", (unsigned long)st); return; }
    CHECK(o.hadapter != 0);
    CHECK(o.luid.HighPart != 0 || o.luid.LowPart != 0);
    closeadapter_t c = { o.hadapter };
    EXPECT_EQ((ULONG)close(&c), 0);
    /* second close of the same handle must fail */
    c.hadapter = o.hadapter;
    LONG st2 = close(&c);
    CHECKF(NT_ERR(st2), "double close = 0x%08lx", st2);
    t_note("double close = 0x%08lx", (unsigned long)st2);
    rd(NULL, dc);
}

TEST(d3dkmt_openadapter_fromhdc_invalid_dcs)
{
    kmt_fn open = kmt("D3DKMTOpenAdapterFromHdc");
    HDC (WINAPI *cc)(HDC) = (void *)t_proc("CreateCompatibleDC");
    BOOL (WINAPI *dd)(HDC) = (void *)t_proc("DeleteDC");
    REQUIRE(open && cc && dd);
    openhdc_t o; memset(&o, 0, sizeof o);
    o.hdc = NULL;
    LONG st = open(&o);
    CHECKF(NT_ERR(st), "NULL hdc: 0x%08lx", st);
    t_note("OpenAdapterFromHdc(NULL hdc) = 0x%08lx", (unsigned long)st);
    EXPECT_EQ(o.hdc, NULL);
    memset(&o, 0, sizeof o);
    o.hdc = (HDC)(ULONG_PTR)0x12345;
    st = open(&o);
    CHECKF(NT_ERR(st), "bogus hdc: 0x%08lx", st);
    EXPECT_EQ(o.hdc, (HDC)(ULONG_PTR)0x12345);
    HDC m = cc(NULL);
    REQUIRE(m);
    memset(&o, 0, sizeof o);
    o.hdc = m;
    st = open(&o);
    t_note("OpenAdapterFromHdc(memory DC) = 0x%08lx", (unsigned long)st);
    CHECKF(NT_ERR(st), "memory DC: 0x%08lx", st);
    EXPECT_EQ(o.hdc, m);
    dd(m);
}

/* ---- the forwarded entry points are literally the win32u syscall stubs ---- */
TEST(d3dkmt_forwarded_exports_equal_win32u_stubs)
{
    HMODULE g = mod_handle(M_GDI32), w = mod_handle(M_WIN32U);
    REQUIRE(g && w);
    int cnt = 0;
    for (int i = 0; i < exp_tab_gdi32_n; i++) {
        const exp_ent_t *e = &exp_tab_gdi32[i];
        if (!e->name || strncmp(e->name, "D3DKMT", 6) || !e->fwd || strncmp(e->fwd, "WIN32U.", 7)) continue;
        FARPROC a = GetProcAddress(g, e->name), b = GetProcAddress(w, e->fwd + 7);
        CHECKF(a && a == b, "%s -> %s", e->name, e->fwd);
        const char *sym = e->fwd + 7;
        CHECKF(!strncmp(sym, "NtGdiDdDDI", 10), "%s forwards to unexpected %s", e->name, sym);
        cnt++;
    }
    CHECK(cnt > 180);
    t_note("%d D3DKMT exports forward to win32u NtGdiDdDDI*", cnt);
}

/* ---- win32u syscall stubs ---- */
TEST(win32u_syscall_stub_shape)
{
    pemap_t pm;
    if (!mod_map(M_WIN32U, &pm)) { t_skip("no genuine module"); return; }
    pexp_t pe; CHECK(pe_exports(&pm, &pe));
    int total = 0, good = 0, nums_ok = 0;
    static unsigned char seen[0x2000];
    int dup = 0;
    for (DWORD i = 0; i < pe.nnames; i++) {
        const char *nm = (const char *)pe_rva(&pm, pe.names[i]);
        const BYTE *code = (const BYTE *)pe_rva(&pm, pe.funcs[pe.ords[i]]);
        if (!nm || !code) continue;
        total++;
        /* mov r10, rcx ; mov eax, imm32 */
        if (code[0] == 0x4C && code[1] == 0x8B && code[2] == 0xD1 && code[3] == 0xB8) {
            good++;
            DWORD num = *(const DWORD *)(code + 4);
            if (num >= 0x1000 && num < 0x2000) { nums_ok++; if (seen[num]) dup++; seen[num] = 1; }
            /* the stub contains `syscall; ret` and the legacy int 2e path */
            int has_sys = 0, has_int = 0;
            for (int k = 8; k < 32; k++) { if (code[k] == 0x0F && code[k + 1] == 0x05) has_sys = 1; if (code[k] == 0xCD && code[k + 1] == 0x2E) has_int = 1; }
            if (!has_sys || !has_int) CHECKF(0, "%s stub lacks syscall/int2e", nm);
        } else t_note("win32u!%s is not a plain syscall stub (first bytes %02x %02x %02x %02x)", nm, code[0], code[1], code[2], code[3]);
    }
    t_note("win32u: %d exports, %d syscall stubs, %d with win32k syscall numbers, %d duplicate numbers", total, good, nums_ok, dup);
    CHECK(total > 1000);
    CHECK(good * 100 >= total * 95);
    CHECK(nums_ok == good);
}
