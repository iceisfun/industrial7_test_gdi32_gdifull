/* Uniscribe internal allocator exports (UspAllocCache / UspAllocTemp / UspFreeMem) and the Eng semaphore helpers.
 *   UspAllocCache(int cb, void **out)   : HeapAlloc from the private Uniscribe heap; cb < 0 or out == NULL -> E_INVALIDARG
 *   UspAllocTemp(ULONG cb, void **out)  : per-thread stack allocator, falls back to the heap; (int)cb < 0 -> E_INVALIDARG
 *   UspFreeMem(void *p)                 : p == NULL -> E_INVALIDARG; frees either kind
 * All three return an HRESULT-like value in eax (0 == success). */
#include "g7.h"

typedef DWORD (WINAPI *usp_alloc_cache_t)(int, void **);
typedef DWORD (WINAPI *usp_alloc_temp_t)(ULONG, void **);
typedef DWORD (WINAPI *usp_free_t)(void *);

#define MEM_REQ() \
    usp_alloc_cache_t ac = (usp_alloc_cache_t)t_proc("UspAllocCache"); REQUIRE(ac); \
    usp_alloc_temp_t at = (usp_alloc_temp_t)t_proc("UspAllocTemp"); REQUIRE(at); \
    usp_free_t fm = (usp_free_t)t_proc("UspFreeMem"); REQUIRE(fm); \
    if (g7_is_wine()) { t_skip("wine: internal allocator not implemented the same"); return; }

TEST(gap7_usp_exports_resolve) {
    if (g7_is_wine()) { t_skip("wine: no such exports"); return; }
    CHECK(t_proc_gdi32("UspAllocCache") != NULL);
    CHECK(t_proc_gdi32("UspAllocTemp") != NULL);
    CHECK(t_proc_gdi32("UspFreeMem") != NULL);
    CHECK(t_proc_gdi32full("UspAllocCache") != NULL);
    CHECK(t_proc_gdi32full("UspAllocTemp") != NULL);
    CHECK(t_proc_gdi32full("UspFreeMem") != NULL);
}
TEST(gap7_usp_gdi32_and_full_equivalent) {
    usp_alloc_cache_t a1 = (usp_alloc_cache_t)t_proc_gdi32("UspAllocCache"), a2 = (usp_alloc_cache_t)t_proc_gdi32full("UspAllocCache");
    usp_free_t f1 = (usp_free_t)t_proc_gdi32("UspFreeMem"), f2 = (usp_free_t)t_proc_gdi32full("UspFreeMem");
    if (!a1 || !a2 || !f1 || !f2) { t_skip("missing"); return; }
    if (g7_is_wine()) { t_skip("wine"); return; }
    void *p = NULL, *q = NULL;
    EXPECT_EQ(a1(64, &p), 0); EXPECT_EQ(a2(64, &q), 0);
    CHECK(p && q && p != q);
    /* memory from one entry point may be released through the other */
    EXPECT_EQ(f2(p), 0); EXPECT_EQ(f1(q), 0);
}

/* ---- UspAllocCache ---- */
TEST(gap7_alloccache_null_out) {
    MEM_REQ();
    EXPECT_EQ(ac(16, NULL), G7_E_INVALIDARG);
    EXPECT_EQ(ac(0, NULL), G7_E_INVALIDARG);
    EXPECT_EQ(ac(-1, NULL), G7_E_INVALIDARG);
}
TEST(gap7_alloccache_negative_size) {
    MEM_REQ();
    static const int neg[] = { -1, -2, -16, -4096, 0x80000000, 0xC0000000, -0x7FFFFFFF };
    for (int i = 0; i < (int)(sizeof neg / sizeof *neg); i++) {
        void *p = (void *)0x1111;
        EXPECT_EQ(ac(neg[i], &p), G7_E_INVALIDARG);
        EXPECT_EQ(p, 0x1111); /* output untouched on the argument-check path */
    }
}
TEST(gap7_alloccache_sizes_roundtrip) {
    MEM_REQ();
    static const int sz[] = { 1, 2, 3, 7, 8, 15, 16, 17, 31, 32, 63, 64, 100, 255, 256, 1000, 4095, 4096, 4097, 65535, 65536, 100000, 1 << 20 };
    for (int i = 0; i < (int)(sizeof sz / sizeof *sz); i++) {
        void *p = NULL;
        DWORD hr = ac(sz[i], &p);
        CHECKF(hr == 0 && p != NULL, "size %d hr %lx", sz[i], (unsigned long)hr);
        if (hr || !p) continue;
        CHECK(((ULONG_PTR)p & 7) == 0); /* heap allocations are at least 8-byte aligned; x64 gives 16 */
        memset(p, 0xC3, sz[i]);
        CHECK(((BYTE *)p)[0] == 0xC3 && ((BYTE *)p)[sz[i] - 1] == 0xC3);
        EXPECT_EQ(fm(p), 0);
    }
}
TEST(gap7_alloccache_zero_size) {
    MEM_REQ();
    void *p = NULL;
    DWORD hr = ac(0, &p);
    EXPECT_EQ(hr, 0);
    CHECK(p != NULL); /* HeapAlloc(…, 0) returns a valid minimal block */
    if (p) EXPECT_EQ(fm(p), 0);
}
TEST(gap7_alloccache_distinct_blocks) {
    MEM_REQ();
    void *p[32];
    for (int i = 0; i < 32; i++) { p[i] = NULL; EXPECT_EQ(ac(48, &p[i]), 0); if (p[i]) memset(p[i], i + 1, 48); }
    for (int i = 0; i < 32; i++) {
        if (!p[i]) continue;
        int bad = 0; for (int k = 0; k < 48; k++) bad += ((BYTE *)p[i])[k] != i + 1;
        CHECKF(bad == 0, "block %d clobbered", i);
        for (int j = i + 1; j < 32; j++) CHECK(p[i] != p[j]);
    }
    for (int i = 0; i < 32; i++) if (p[i]) EXPECT_EQ(fm(p[i]), 0);
}
TEST(gap7_alloccache_free_any_order) {
    MEM_REQ();
    /* cache blocks are plain heap blocks: they may be freed in any order */
    void *p[8];
    for (int i = 0; i < 8; i++) { p[i] = NULL; ac(128 + i, &p[i]); }
    static const int ord[8] = { 3, 0, 7, 1, 6, 2, 5, 4 };
    for (int i = 0; i < 8; i++) if (p[ord[i]]) EXPECT_EQ(fm(p[ord[i]]), 0);
}
TEST(gap7_alloccache_repeat_loop) {
    MEM_REQ();
    int fails = 0;
    for (int i = 0; i < 2000; i++) {
        void *p = NULL;
        if (ac(1 + (i % 700), &p) != 0 || !p) { fails++; continue; }
        ((BYTE *)p)[0] = (BYTE)i;
        if (fm(p) != 0) fails++;
    }
    CHECK(fails == 0);
}
TEST(gap7_alloccache_large) {
    MEM_REQ();
    void *p = NULL;
    DWORD hr = ac(16 << 20, &p);
    CHECKF(hr == 0 && p, "16MB: hr %lx", (unsigned long)hr);
    if (p) { ((BYTE *)p)[0] = 1; ((BYTE *)p)[(16 << 20) - 1] = 2; EXPECT_EQ(fm(p), 0); }
}
static void g7_cache_thr(void *a) { void **out = a; usp_alloc_cache_t ac = (usp_alloc_cache_t)t_proc("UspAllocCache"); ac(256, out); if (*out) memset(*out, 0x42, 256); }
TEST(gap7_alloccache_cross_thread_free) {
    MEM_REQ();
    void *p = NULL;
    g7_run_thread(g7_cache_thr, &p);
    CHECK(p != NULL);
    if (p) {
        CHECK(((BYTE *)p)[0] == 0x42 && ((BYTE *)p)[255] == 0x42); /* survives thread exit: shared process heap */
        EXPECT_EQ(fm(p), 0);
    }
}

/* ---- UspAllocTemp ---- */
TEST(gap7_alloctemp_null_out) {
    MEM_REQ();
    EXPECT_EQ(at(16, NULL), G7_E_INVALIDARG);
    EXPECT_EQ(at(0, NULL), G7_E_INVALIDARG);
}
TEST(gap7_alloctemp_negative_size) {
    MEM_REQ();
    static const ULONG neg[] = { 0x80000000u, 0x80000001u, 0xFFFFFFFFu, 0xFFFFFFF0u, 0xC0000000u };
    for (int i = 0; i < (int)(sizeof neg / sizeof *neg); i++) {
        void *p = (void *)0x2222;
        EXPECT_EQ(at(neg[i], &p), G7_E_INVALIDARG);
        EXPECT_EQ(p, 0x2222);
    }
}
TEST(gap7_alloctemp_sizes_roundtrip) {
    MEM_REQ();
    static const ULONG sz[] = { 1, 2, 4, 8, 15, 16, 17, 64, 100, 255, 256, 1000, 4096, 8192, 10000, 16000, 20000, 65536, 1 << 20 };
    for (int i = 0; i < (int)(sizeof sz / sizeof *sz); i++) {
        void *p = NULL;
        DWORD hr = at(sz[i], &p);
        CHECKF(hr == 0 && p != NULL, "size %lu hr %lx", (unsigned long)sz[i], (unsigned long)hr);
        if (hr || !p) continue;
        memset(p, 0x3C, sz[i]);
        CHECK(((BYTE *)p)[sz[i] - 1] == 0x3C);
        EXPECT_EQ(fm(p), 0);
    }
}
TEST(gap7_alloctemp_zero_size) {
    MEM_REQ();
    void *p = NULL;
    DWORD hr = at(0, &p);
    CHECKF(hr == 0 || hr == G7_E_OUTOFMEMORY, "hr %lx", (unsigned long)hr);
    if (hr == 0 && p) EXPECT_EQ(fm(p), 0);
    else t_note("UspAllocTemp(0) hr=%lx p=%p", (unsigned long)hr, p);
}
TEST(gap7_alloctemp_lifo_nested) {
    MEM_REQ();
    void *a = NULL, *b = NULL, *c = NULL;
    EXPECT_EQ(at(100, &a), 0); EXPECT_EQ(at(200, &b), 0); EXPECT_EQ(at(300, &c), 0);
    CHECK(a && b && c && a != b && b != c && a != c);
    if (!(a && b && c)) return;
    memset(a, 1, 100); memset(b, 2, 200); memset(c, 3, 300);
    CHECK(((BYTE *)a)[99] == 1 && ((BYTE *)b)[0] == 2 && ((BYTE *)b)[199] == 2 && ((BYTE *)c)[0] == 3);
    /* disjoint ranges */
    CHECK((BYTE *)a + 100 <= (BYTE *)b || (BYTE *)b + 200 <= (BYTE *)a);
    CHECK((BYTE *)b + 200 <= (BYTE *)c || (BYTE *)c + 300 <= (BYTE *)b);
    EXPECT_EQ(fm(c), 0); EXPECT_EQ(fm(b), 0); EXPECT_EQ(fm(a), 0);
}
TEST(gap7_alloctemp_reuse_after_free) {
    MEM_REQ();
    /* allocate/free pairs many times; the stack allocator should keep succeeding */
    int fails = 0;
    for (int i = 0; i < 5000; i++) {
        void *p = NULL;
        if (at(16 + (i % 900), &p) != 0 || !p) { fails++; continue; }
        memset(p, 0x5A, 16);
        if (fm(p) != 0) fails++;
    }
    CHECK(fails == 0);
}
TEST(gap7_alloctemp_deep_stack) {
    MEM_REQ();
    void *p[200]; int n = 0;
    for (; n < 200; n++) { p[n] = NULL; if (at(512, &p[n]) != 0 || !p[n]) break; memset(p[n], n, 512); }
    CHECK(n == 200);
    for (int i = 0; i < n; i++) { int bad = 0; for (int k = 0; k < 512; k++) bad += ((BYTE *)p[i])[k] != (BYTE)i; CHECKF(bad == 0, "block %d clobbered", i); }
    for (int i = n - 1; i >= 0; i--) EXPECT_EQ(fm(p[i]), 0);
}
TEST(gap7_alloctemp_mixed_with_cache) {
    MEM_REQ();
    void *t1 = NULL, *c1 = NULL, *t2 = NULL;
    EXPECT_EQ(at(64, &t1), 0); EXPECT_EQ(ac(64, &c1), 0); EXPECT_EQ(at(64, &t2), 0);
    CHECK(t1 && c1 && t2 && t1 != c1 && c1 != t2 && t1 != t2);
    if (t2) EXPECT_EQ(fm(t2), 0);
    if (c1) EXPECT_EQ(fm(c1), 0);
    if (t1) EXPECT_EQ(fm(t1), 0);
}
static void g7_temp_thr(void *a) {
    int *ok = a; usp_alloc_temp_t at = (usp_alloc_temp_t)t_proc("UspAllocTemp"); usp_free_t fm = (usp_free_t)t_proc("UspFreeMem");
    void *p = NULL; *ok = 0;
    if (at(1000, &p) == 0 && p) { memset(p, 1, 1000); *ok = fm(p) == 0; }
}
TEST(gap7_alloctemp_other_thread_independent) {
    MEM_REQ();
    void *mine = NULL; EXPECT_EQ(at(256, &mine), 0);
    int ok = 0; g7_run_thread(g7_temp_thr, &ok);
    CHECK(ok);
    if (mine) { memset(mine, 7, 256); EXPECT_EQ(fm(mine), 0); }
}

/* ---- UspFreeMem ---- */
TEST(gap7_freemem_null) {
    MEM_REQ();
    EXPECT_EQ(fm(NULL), G7_E_INVALIDARG);
    for (int i = 0; i < 10; i++) EXPECT_EQ(fm(NULL), G7_E_INVALIDARG);
}
TEST(gap7_freemem_null_keeps_allocator_usable) {
    MEM_REQ();
    fm(NULL);
    void *p = NULL;
    EXPECT_EQ(at(32, &p), 0);
    if (p) EXPECT_EQ(fm(p), 0);
}

/* ---- Eng semaphores (gdi32full: thin wrappers over a heap-allocated CRITICAL_SECTION) ---- */
UDECL(void *, EngCreateSemaphore, (void));
UDECL(void, EngDeleteSemaphore, (void *));
UDECL(void, EngAcquireSemaphore, (void *));
UDECL(void, EngReleaseSemaphore, (void *));

/* Where the real exports are absent (non-Windows sanity runs) a CRITICAL_SECTION shim with the documented
 * gdi32full semantics is used so that the test logic itself still gets exercised. */
static void *WINAPI shim_create(void) { CRITICAL_SECTION *c = HeapAlloc(GetProcessHeap(), 0, 0x28); if (c) InitializeCriticalSection(c); return c; }
static void WINAPI shim_delete(void *c) { if (c) { DeleteCriticalSection(c); HeapFree(GetProcessHeap(), 0, c); } }
static void WINAPI shim_acq(void *c) { EnterCriticalSection(c); }
static void WINAPI shim_rel(void *c) { LeaveCriticalSection(c); }
static EngCreateSemaphore_pfn g7_cs(void) { EngCreateSemaphore_pfn f = UN(EngCreateSemaphore); return f ? f : shim_create; }
static EngDeleteSemaphore_pfn g7_ds(void) { EngDeleteSemaphore_pfn f = UN(EngDeleteSemaphore); return f ? f : shim_delete; }
static EngAcquireSemaphore_pfn g7_aq(void) { EngAcquireSemaphore_pfn f = UN(EngAcquireSemaphore); return f ? f : shim_acq; }
static EngReleaseSemaphore_pfn g7_rl(void) { EngReleaseSemaphore_pfn f = UN(EngReleaseSemaphore); return f ? f : shim_rel; }
#define SEM_REQ() \
    EngCreateSemaphore_pfn cs = g7_cs(); EngDeleteSemaphore_pfn ds = g7_ds(); \
    EngAcquireSemaphore_pfn aq = g7_aq(); EngReleaseSemaphore_pfn rl = g7_rl()

TEST(gap7_engsem_exports) {
    if (g7_is_wine()) { t_skip("wine: no such exports"); return; }
    CHECK(t_proc_gdi32("EngAcquireSemaphore") != NULL);
    CHECK(t_proc_gdi32full("EngAcquireSemaphore") != NULL);
    CHECK(t_proc_gdi32("EngReleaseSemaphore") != NULL);
    CHECK(t_proc_gdi32full("EngReleaseSemaphore") != NULL);
}
TEST(gap7_engsem_basic_acquire_release) {
    SEM_REQ();
    void *s = cs(); CHECK(s != NULL); if (!s) return;
    CRITICAL_SECTION *c = s;
    CHECK(c->LockCount == -1); CHECK(c->RecursionCount == 0);
    aq(s);
    CHECK(c->RecursionCount == 1);
    CHECK((DWORD)(ULONG_PTR)c->OwningThread == GetCurrentThreadId());
    rl(s);
    CHECK(c->RecursionCount == 0);
    CHECK(c->OwningThread == NULL);
    ds(s);
}
TEST(gap7_engsem_recursive) {
    SEM_REQ();
    void *s = cs(); if (!s) { t_skip("no sem"); return; }
    CRITICAL_SECTION *c = s;
    for (int i = 1; i <= 10; i++) { aq(s); EXPECT_EQ(c->RecursionCount, i); }
    for (int i = 9; i >= 0; i--) { rl(s); EXPECT_EQ(c->RecursionCount, i); }
    ds(s);
}
TEST(gap7_engsem_independent_objects) {
    SEM_REQ();
    void *a = cs(), *b = cs();
    CHECK(a && b && a != b);
    if (a && b) {
        aq(a); aq(b);
        CHECK(((CRITICAL_SECTION *)a)->RecursionCount == 1 && ((CRITICAL_SECTION *)b)->RecursionCount == 1);
        rl(b); rl(a);
    }
    if (a) ds(a);
    if (b) ds(b);
}
TEST(gap7_engsem_works_on_plain_critical_section) {
    SEM_REQ();
    CRITICAL_SECTION c; InitializeCriticalSection(&c);
    aq(&c);
    CHECK(c.RecursionCount == 1);
    CHECK(TryEnterCriticalSection(&c)); /* same thread: recursive */
    LeaveCriticalSection(&c);
    rl(&c);
    CHECK(c.RecursionCount == 0);
    DeleteCriticalSection(&c);
}
static void g7_sem_try(void *a) { void **p = a; *(int *)p[1] = TryEnterCriticalSection(p[0]); if (*(int *)p[1]) LeaveCriticalSection(p[0]); }
TEST(gap7_engsem_excludes_other_thread) {
    SEM_REQ();
    void *s = cs(); if (!s) { t_skip("no sem"); return; }
    int got = -1; void *args[2] = { s, &got };
    aq(s);
    g7_run_thread(g7_sem_try, args);
    EXPECT_EQ(got, 0); /* another thread cannot take it while we own it */
    rl(s);
    got = -1;
    g7_run_thread(g7_sem_try, args);
    EXPECT_EQ(got, 1); /* free again */
    ds(s);
}
static void g7_sem_acq(void *a) { void **p = a; EngAcquireSemaphore_pfn aq = g7_aq(); EngReleaseSemaphore_pfn rl = g7_rl(); aq(p[0]); (*(int *)p[1])++; rl(p[0]); }
TEST(gap7_engsem_blocks_until_release) {
    SEM_REQ();
    void *s = cs(); if (!s) { t_skip("no sem"); return; }
    volatile int done = 0; void *args[2] = { s, (void *)&done };
    aq(s);
    g7_thr_ctx ctx = { g7_sem_acq, args };
    HANDLE h = CreateThread(NULL, 0, g7_thr_main, &ctx, 0, NULL);
    Sleep(150);
    EXPECT_EQ(done, 0); /* waiter is blocked */
    rl(s);
    if (h) { DWORD w = WaitForSingleObject(h, 10000); CHECK(w == WAIT_OBJECT_0); CloseHandle(h); }
    EXPECT_EQ(done, 1);
    ds(s);
}
TEST(gap7_engsem_stress_two_threads) {
    SEM_REQ();
    void *s = cs(); if (!s) { t_skip("no sem"); return; }
    static volatile int counter; counter = 0;
    void *args[2] = { s, (void *)&counter };
    g7_thr_ctx ctx = { g7_sem_acq, args };
    HANDLE h[4];
    for (int i = 0; i < 4; i++) h[i] = CreateThread(NULL, 0, g7_thr_main, &ctx, 0, NULL);
    for (int i = 0; i < 4; i++) if (h[i]) { WaitForSingleObject(h[i], 10000); CloseHandle(h[i]); }
    EXPECT_EQ(counter, 4);
    ds(s);
}
TEST(gap7_engsem_create_delete_loop) {
    SEM_REQ();
    int bad = 0;
    for (int i = 0; i < 2000; i++) {
        void *s = cs(); if (!s) { bad++; continue; }
        aq(s); rl(s); ds(s);
    }
    CHECK(bad == 0);
    ds(NULL); /* EngDeleteSemaphore explicitly tolerates NULL */
}
TEST(gap7_engsem_gdi32_and_full_same_effect) {
    SEM_REQ();
    if (g7_is_wine()) { t_skip("wine"); return; }
    EngAcquireSemaphore_pfn a1 = (EngAcquireSemaphore_pfn)t_proc_gdi32("EngAcquireSemaphore");
    EngAcquireSemaphore_pfn a2 = (EngAcquireSemaphore_pfn)t_proc_gdi32full("EngAcquireSemaphore");
    EngReleaseSemaphore_pfn r1 = (EngReleaseSemaphore_pfn)t_proc_gdi32("EngReleaseSemaphore");
    EngReleaseSemaphore_pfn r2 = (EngReleaseSemaphore_pfn)t_proc_gdi32full("EngReleaseSemaphore");
    if (!a1 || !a2 || !r1 || !r2) { t_skip("missing"); return; }
    void *s = cs(); if (!s) { t_skip("no sem"); return; }
    CRITICAL_SECTION *c = s;
    a1(s); a2(s); EXPECT_EQ(c->RecursionCount, 2);
    r2(s); r1(s); EXPECT_EQ(c->RecursionCount, 0);
    ds(s);
}
