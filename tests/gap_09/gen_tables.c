/* Table-generated per-export tests: D3DKMT*, NtGdi*, DdEntry*, and the stub group in gdi32full. */
#include "gap9.h"

/* ---- D3DKMT: gdi32.dll exports a bare win32k syscall stub per function ---- */
static int kmt_soft(const char *n)
{
    static const char *const soft[] = { "D3DKMTCheckOcclusion", "D3DKMTGetPresentHistory", "D3DKMTGetRuntimeData",
        "D3DKMTCheckExclusiveOwnership", "D3DKMTGetDWMVerticalBlankEvent", "D3DKMTPollDisplayChildren", NULL };
    for (int i = 0; soft[i]; i++) if (!strcmp(soft[i], n)) return 1;
    return 0;
}

static void kmt_body(const char *name)
{
    FARPROC p = t_proc_gdi32(name);
    if (!p) { t_skip("missing export %s", name); return; }
    if (g9_wine()) { t_skip("wine: %s not called", name); return; }
    CHECK(g9_in_module(p, t_gdi32()));
    unsigned no = 0;
    int c = g9_class(p, &no);
    CHECKF(c == G9_SYSCALL, "%s class %d", name, c);
    if (c == G9_SYSCALL) CHECKF(no >= 0x1000 && no < 0x2000, "%s sysno 0x%x", name, no);
    if (c != G9_SYSCALL) return;
    LONG st = (LONG)g9_call(p, 0, 0, 0, 0);
    if (kmt_soft(name)) t_note("%s(NULL) = 0x%08lx", name, (unsigned long)st);
    else CHECKF(NT_ERR9(st), "%s(NULL) = 0x%08lx", name, (unsigned long)st);
    st = (LONG)g9_call(p, 1, 0, 0, 0);
    if (kmt_soft(name)) t_note("%s(1) = 0x%08lx", name, (unsigned long)st);
    else CHECKF(NT_ERR9(st), "%s((void*)1) = 0x%08lx", name, (unsigned long)st);
}
#define X(n) TEST(gap9_kmt_##n) { kmt_body(#n); }
GAP9_D3DKMT_LIST(X)
#undef X

/* All D3DKMT stubs are distinct win32k services. */
TEST(gap9_kmt_syscall_numbers_distinct)
{
    G9_NOWINE("syscall stub decode");
    static const char *const names[] = {
#define X(n) #n,
        GAP9_D3DKMT_LIST(X)
#undef X
    };
    unsigned nums[sizeof names / sizeof *names];
    int n = 0, dup = 0;
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        FARPROC p = t_proc_gdi32(names[i]);
        if (!p) continue;
        unsigned no;
        if (g9_class(p, &no) != G9_SYSCALL) continue;
        for (int j = 0; j < n; j++) if (nums[j] == no) { dup++; t_note("%s shares sysno 0x%x", names[i], no); }
        nums[n++] = no;
    }
    CHECK(n > 200);
    CHECK(dup == 0);
}

TEST(gap9_kmt_export_count)
{
    G9_NOWINE("export set differs");
    int n = 0;
#define X(m) n++;
    GAP9_D3DKMT_LIST(X)
#undef X
    CHECK(n == 224);
    int found = 0;
#define X(m) if (t_proc_gdi32(#m)) found++;
    GAP9_D3DKMT_LIST(X)
#undef X
    CHECKF(found == n, "found %d of %d", found, n);
}

/* ---- NtGdi*: gdi32full.dll jumps through its import table to win32u ---- */
static void nt_body(const char *name)
{
    FARPROC p = t_proc_gdi32full(name);
    if (!p) { t_skip("missing export %s", name); return; }
    CHECK(g9_in_module(p, t_gdi32full()));
    if (g9_wine()) { t_skip("wine: %s thunk shape", name); return; }
    CHECKF(((BYTE *)p)[0] == 0xFF && ((BYTE *)p)[1] == 0x25, "%s is a jmp thunk", name);
    unsigned no = 0;
    int c = g9_class(p, &no);
    CHECKF(c == G9_SYSCALL, "%s class %d", name, c);
    if (c == G9_SYSCALL) CHECKF(no >= 0x1000 && no < 0x2000, "%s sysno 0x%x", name, no);
    HMODULE u = g9_win32u();
    if (u) {
        FARPROC w = GetProcAddress(u, name);
        CHECKF(w != NULL, "win32u!%s", name);
        if (w) CHECKF(g9_follow(p) == (const BYTE *)w, "%s resolves to win32u!%s", name, name);
    }
}
#define X(n) TEST(gap9_nt_shape_##n) { nt_body(#n); }
GAP9_NTGDI_LIST(X)
#undef X

TEST(gap9_nt_syscall_numbers_distinct)
{
    G9_NOWINE("syscall stub decode");
    static const char *const names[] = {
#define X(n) #n,
        GAP9_NTGDI_LIST(X)
#undef X
    };
    static unsigned nums[600];
    int n = 0, dup = 0;
    for (size_t i = 0; i < sizeof names / sizeof *names && n < 600; i++) {
        FARPROC p = t_proc_gdi32full(names[i]);
        unsigned no;
        if (!p || g9_class(p, &no) != G9_SYSCALL) continue;
        for (int j = 0; j < n; j++) if (nums[j] == no) { dup++; t_note("%s shares sysno 0x%x", names[i], no); }
        nums[n++] = no;
    }
    CHECK(n > 250);
    CHECK(dup == 0);
}

/* NtGdi exports present in gdi32full that are plain "xor eax,eax; ret" stubs (DirectDraw/D3D/video-port entry
 * points removed from win32k): any arguments, result 0. */
static void xor_body(const char *name)
{
    FARPROC p = t_proc_gdi32full(name);
    if (!p) { t_skip("missing export %s", name); return; }
    CHECK(g9_in_module(p, t_gdi32full()));
    if (g9_wine()) { t_skip("wine: %s stub", name); return; }
    CHECKF(g9_class(p, NULL) == G9_XOR0, "%s class %d", name, g9_class(p, NULL));
    if (g9_class(p, NULL) != G9_XOR0) return;
    EXPECT_EQ(g9_call(p, 0, 0, 0, 0), 0);
    EXPECT_EQ(g9_call(p, (ULONG_PTR)-1, (ULONG_PTR)-1, (ULONG_PTR)-1, (ULONG_PTR)-1), 0);
    EXPECT_EQ(g9_call8(p, G9_BOGUS, 1, 2, 3, 4, 5, 6, 7), 0);
    FARPROC g = t_proc_gdi32(name);
    if (g) EXPECT_EQ(g9_call(g, 1, 2, 3, 4), 0);
}
#define X(n) TEST(gap9_stub0_##n) { xor_body(#n); }
GAP9_NTGDI_XOR_LIST(X)
#undef X

/* ---- DdEntryN in gdi32: jmp through gdi32full's matching NtGdiDd, NtGdiD3d and NtGdiDxg export ---- */
static void dd_body(const char *entry, const char *target)
{
    FARPROC p = t_proc_gdi32(entry);
    if (!p) { t_skip("missing export %s", entry); return; }
    CHECK(g9_in_module(p, t_gdi32()));
    if (g9_wine()) { t_skip("wine: %s thunk", entry); return; }
    CHECKF(((BYTE *)p)[0] == 0xFF && ((BYTE *)p)[1] == 0x25, "%s is a jmp thunk", entry);
    FARPROC t = t_proc_gdi32full(target);
    CHECKF(t != NULL, "gdi32full!%s", target);
    if (!t) return;
    /* DdEntryN lands on the very same code as the NtGdi export it wraps */
    CHECKF(g9_follow(p) == g9_follow(t), "%s -> %s", entry, target);
    if (g9_is_xor_name(target)) {
        CHECKF(g9_class(p, NULL) == G9_XOR0, "%s class", entry);
        EXPECT_EQ(g9_call(p, 0, 0, 0, 0), 0);
        EXPECT_EQ(g9_call8(p, G9_BOGUS, 0x1000, 0, 0, 0, 0, 0, 0), 0);
    } else {
        unsigned no = 0;
        CHECKF(g9_class(p, &no) == G9_SYSCALL, "%s class", entry);
        CHECKF(no >= 0x1000 && no < 0x2000, "%s sysno 0x%x", entry, no);
    }
}
#define X(e, t) TEST(gap9_##e) { dd_body(#e, #t); }
GAP9_DDMAP_LIST(X)
#undef X

TEST(gap9_dd_entries_count)
{
    G9_NOWINE("export set differs");
    int n = 0;
    for (int i = 0; i < 80; i++) {
        char nm[32]; wsprintfA(nm, "DdEntry%d", i);
        if (t_proc_gdi32(nm)) n++;
    }
    CHECKF(n == 57, "found %d", n);
    CHECK(t_proc_gdi32("DdEntry57") == NULL);
    CHECK(t_proc_gdi32("DdEntry-1") == NULL);
    /* gdi32full does not export the DdEntry names */
    CHECK(t_proc_gdi32full("DdEntry0") == NULL);
}
