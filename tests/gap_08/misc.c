/* Eng* helpers implemented in user mode (gdi32full): semaphores, time, code pages,
 * resources, module loading, string conversion, printer-driver queries. */
#include "g8.h"
#include <limits.h>
#include <wchar.h>

/* ================= semaphores ================= */
#define NEED_SEM() do { REQUIRE(UN(EngCreateSemaphore)); REQUIRE(UN(EngDeleteSemaphore)); \
                        REQUIRE(UN(EngAcquireSemaphore)); REQUIRE(UN(EngReleaseSemaphore)); } while (0)

TEST(gap8_engsemaphore_create_layout) {
    NEED_SEM();
    RTL_CRITICAL_SECTION *cs = UN(EngCreateSemaphore)();
    CHECK(cs != NULL);
    if (!cs) return;
    EXPECT_EQ((ULONG_PTR)cs & 7, 0);
    EXPECT_EQ(cs->LockCount, -1);
    EXPECT_EQ(cs->RecursionCount, 0);
    EXPECT_EQ(cs->OwningThread, NULL);
    EXPECT_EQ(cs->SpinCount, 0);
    CHECK(cs->DebugInfo != NULL);
    UN(EngDeleteSemaphore)(cs);
}
TEST(gap8_engsemaphore_lives_in_process_heap) {
    NEED_SEM();
    RTL_CRITICAL_SECTION *cs = UN(EngCreateSemaphore)();
    CHECK(cs != NULL);
    if (!cs) return;
    HANDLE hp = GetProcessHeap();
    CHECK(HeapValidate(hp, 0, cs));
    EXPECT_EQ(HeapSize(hp, 0, cs), 0x28);
    EXPECT_EQ(sizeof(RTL_CRITICAL_SECTION), 0x28);
    UN(EngDeleteSemaphore)(cs);
}
TEST(gap8_engsemaphore_acquire_release_state) {
    NEED_SEM();
    RTL_CRITICAL_SECTION *cs = UN(EngCreateSemaphore)();
    CHECK(cs != NULL);
    if (!cs) return;
    UN(EngAcquireSemaphore)(cs);
    EXPECT_EQ(cs->LockCount, 0);
    EXPECT_EQ(cs->RecursionCount, 1);
    EXPECT_EQ((ULONG_PTR)cs->OwningThread, GetCurrentThreadId());
    UN(EngReleaseSemaphore)(cs);
    EXPECT_EQ(cs->LockCount, -1);
    EXPECT_EQ(cs->RecursionCount, 0);
    EXPECT_EQ(cs->OwningThread, NULL);
    UN(EngDeleteSemaphore)(cs);
}
TEST(gap8_engsemaphore_recursive) {
    NEED_SEM();
    RTL_CRITICAL_SECTION *cs = UN(EngCreateSemaphore)();
    CHECK(cs != NULL);
    if (!cs) return;
    for (int i = 1; i <= 5; i++) {
        UN(EngAcquireSemaphore)(cs);
        EXPECT_EQ(cs->RecursionCount, i);
        EXPECT_EQ(cs->LockCount, i - 1);
    }
    for (int i = 4; i >= 0; i--) {
        UN(EngReleaseSemaphore)(cs);
        EXPECT_EQ(cs->RecursionCount, i);
    }
    EXPECT_EQ(cs->LockCount, -1);
    UN(EngDeleteSemaphore)(cs);
}
TEST(gap8_engsemaphore_try_enter_interop) {
    NEED_SEM();
    RTL_CRITICAL_SECTION *cs = UN(EngCreateSemaphore)();
    CHECK(cs != NULL);
    if (!cs) return;
    CHECK(TryEnterCriticalSection(cs));
    UN(EngAcquireSemaphore)(cs);
    EXPECT_EQ(cs->RecursionCount, 2);
    UN(EngReleaseSemaphore)(cs);
    LeaveCriticalSection(cs);
    EXPECT_EQ(cs->RecursionCount, 0);
    EnterCriticalSection(cs);
    UN(EngReleaseSemaphore)(cs);
    EXPECT_EQ(cs->OwningThread, NULL);
    UN(EngDeleteSemaphore)(cs);
}

static struct { RTL_CRITICAL_SECTION *cs; HANDLE started, got; volatile LONG try_ok; } g_sem;
static DWORD WINAPI try_thread(LPVOID a) {
    g_sem.try_ok = TryEnterCriticalSection(g_sem.cs) ? 1 : 0;
    if (g_sem.try_ok) LeaveCriticalSection(g_sem.cs);
    return 0;
}
static DWORD WINAPI block_thread(LPVOID a) {
    SetEvent(g_sem.started);
    UN(EngAcquireSemaphore)(g_sem.cs);
    SetEvent(g_sem.got);
    UN(EngReleaseSemaphore)(g_sem.cs);
    return 0;
}
TEST(gap8_engsemaphore_cross_thread_exclusion) {
    NEED_SEM();
    g_sem.cs = UN(EngCreateSemaphore)();
    CHECK(g_sem.cs != NULL);
    if (!g_sem.cs) return;
    UN(EngAcquireSemaphore)(g_sem.cs);
    g_sem.try_ok = -1;
    HANDLE t = CreateThread(NULL, 0, try_thread, NULL, 0, NULL);
    WaitForSingleObject(t, 20000); CloseHandle(t);
    EXPECT_EQ(g_sem.try_ok, 0);
    UN(EngReleaseSemaphore)(g_sem.cs);
    g_sem.try_ok = -1;
    t = CreateThread(NULL, 0, try_thread, NULL, 0, NULL);
    WaitForSingleObject(t, 20000); CloseHandle(t);
    EXPECT_EQ(g_sem.try_ok, 1);
    UN(EngDeleteSemaphore)(g_sem.cs);
}
TEST(gap8_engsemaphore_cross_thread_blocking) {
    NEED_SEM();
    g_sem.cs = UN(EngCreateSemaphore)();
    CHECK(g_sem.cs != NULL);
    if (!g_sem.cs) return;
    g_sem.started = CreateEventA(NULL, TRUE, FALSE, NULL);
    g_sem.got = CreateEventA(NULL, TRUE, FALSE, NULL);
    UN(EngAcquireSemaphore)(g_sem.cs);
    HANDLE t = CreateThread(NULL, 0, block_thread, NULL, 0, NULL);
    EXPECT_EQ(WaitForSingleObject(g_sem.started, 20000), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(g_sem.got, 150), WAIT_TIMEOUT);
    UN(EngReleaseSemaphore)(g_sem.cs);
    EXPECT_EQ(WaitForSingleObject(g_sem.got, 20000), WAIT_OBJECT_0);
    WaitForSingleObject(t, 20000); CloseHandle(t);
    CloseHandle(g_sem.started); CloseHandle(g_sem.got);
    UN(EngDeleteSemaphore)(g_sem.cs);
}
TEST(gap8_engsemaphore_many_distinct) {
    NEED_SEM();
    enum { N = 500 };
    static void *h[N];
    for (int i = 0; i < N; i++) {
        h[i] = UN(EngCreateSemaphore)();
        if (!h[i]) { CHECKF(0, "alloc %d", i); break; }
        for (int j = i > 8 ? i - 8 : 0; j < i; j++) if (h[i] == h[j]) { CHECKF(0, "dup %d %d", i, j); }
    }
    for (int i = 0; i < N; i++) if (h[i]) { UN(EngAcquireSemaphore)(h[i]); UN(EngReleaseSemaphore)(h[i]); UN(EngDeleteSemaphore)(h[i]); }
    CHECK(1);
}
TEST(gap8_engsemaphore_independent_instances) {
    NEED_SEM();
    RTL_CRITICAL_SECTION *a = UN(EngCreateSemaphore)(), *b = UN(EngCreateSemaphore)();
    CHECK(a && b && a != b);
    if (!a || !b) { if (a) UN(EngDeleteSemaphore)(a); if (b) UN(EngDeleteSemaphore)(b); return; }
    UN(EngAcquireSemaphore)(a);
    EXPECT_EQ(b->RecursionCount, 0);
    UN(EngAcquireSemaphore)(b);
    EXPECT_EQ(a->RecursionCount, 1);
    EXPECT_EQ(b->RecursionCount, 1);
    UN(EngReleaseSemaphore)(a);
    EXPECT_EQ(b->RecursionCount, 1);
    UN(EngReleaseSemaphore)(b);
    UN(EngDeleteSemaphore)(a); UN(EngDeleteSemaphore)(b);
}
TEST(gap8_engsemaphore_delete_null) {
    NEED_SEM();
    void (WINAPI *del)(PVOID) = (void *)UN(EngDeleteSemaphore);
    SET_ERR(0x7777);
    del(NULL);
    EXPECT_ERR(0, 0x7777);
}
TEST(gap8_engsemaphore_create_delete_loop) {
    NEED_SEM();
    for (int i = 0; i < 5000; i++) {
        void *s = UN(EngCreateSemaphore)();
        if (!s) { CHECKF(0, "iter %d", i); return; }
        UN(EngDeleteSemaphore)(s);
    }
    CHECK(1);
}
TEST(gap8_engsemaphore_exports) {
    G8_NOWINE_ALWAYS();
    CHECK(t_proc_gdi32("EngCreateSemaphore") != NULL);
    CHECK(t_proc_gdi32("EngDeleteSemaphore") != NULL);
    CHECK(t_proc_gdi32("EngAcquireSemaphore") != NULL);
    CHECK(t_proc_gdi32("EngReleaseSemaphore") != NULL);
    CHECK(t_proc_gdi32full("EngCreateSemaphore") != NULL);
    CHECK(t_proc_gdi32full("EngDeleteSemaphore") != NULL);
    CHECK(t_proc_gdi32full("EngAcquireSemaphore") != NULL);
    CHECK(t_proc_gdi32full("EngReleaseSemaphore") != NULL);
}
TEST(gap8_engsemaphore_both_modules_interchangeable) {
    REQUIRE(t_proc_gdi32full("EngCreateSemaphore")); REQUIRE(t_proc_gdi32("EngDeleteSemaphore"));
    PVOID (WINAPI *cr)(void) = (void *)t_proc_gdi32full("EngCreateSemaphore");
    void (WINAPI *de)(PVOID) = (void *)t_proc_gdi32("EngDeleteSemaphore");
    void (WINAPI *ac)(PVOID) = (void *)t_proc_gdi32full("EngAcquireSemaphore");
    void (WINAPI *re)(PVOID) = (void *)t_proc_gdi32("EngReleaseSemaphore");
    RTL_CRITICAL_SECTION *cs = cr();
    CHECK(cs != NULL);
    if (!cs) return;
    ac(cs); EXPECT_EQ(cs->RecursionCount, 1); re(cs); EXPECT_EQ(cs->RecursionCount, 0);
    de(cs);
}

/* ================= EngQueryLocalTime ================= */
static ULONGLONG st_to_ull(const SYSTEMTIME *st) {
    FILETIME ft; SystemTimeToFileTime(st, &ft);
    return ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}
TEST(gap8_engquerylocaltime_matches_local_time) {
    REQUIRE(UN(EngQueryLocalTime));
    for (int tries = 0; tries < 3; tries++) {
        SYSTEMTIME a, b; USHORT t[8];
        GetLocalTime(&a);
        UN(EngQueryLocalTime)(t);
        GetLocalTime(&b);
        SYSTEMTIME e; memset(&e, 0, sizeof e);
        e.wYear = t[0]; e.wMonth = t[1]; e.wDay = t[2]; e.wHour = t[3]; e.wMinute = t[4]; e.wSecond = t[5]; e.wMilliseconds = t[6];
        ULONGLONG ua = st_to_ull(&a), ub = st_to_ull(&b), ue = st_to_ull(&e);
        if (ue >= ua && ue <= ub) {
            CHECK(1);
            if (a.wDay == b.wDay) EXPECT_EQ(t[7], a.wDayOfWeek);
            return;
        }
    }
    CHECK(0);
}
TEST(gap8_engquerylocaltime_field_ranges) {
    REQUIRE(UN(EngQueryLocalTime));
    USHORT t[8];
    UN(EngQueryLocalTime)(t);
    CHECK(t[0] >= 2020 && t[0] < 3000);
    CHECK(t[1] >= 1 && t[1] <= 12);
    CHECK(t[2] >= 1 && t[2] <= 31);
    CHECK(t[3] <= 23); CHECK(t[4] <= 59); CHECK(t[5] <= 59); CHECK(t[6] <= 999); CHECK(t[7] <= 6);
}
TEST(gap8_engquerylocaltime_weekday_consistent) {
    REQUIRE(UN(EngQueryLocalTime));
    USHORT t[8];
    UN(EngQueryLocalTime)(t);
    SYSTEMTIME e; memset(&e, 0, sizeof e);
    e.wYear = t[0]; e.wMonth = t[1]; e.wDay = t[2];
    FILETIME ft; SYSTEMTIME back;
    CHECK(SystemTimeToFileTime(&e, &ft) && FileTimeToSystemTime(&ft, &back));
    EXPECT_EQ(back.wDayOfWeek, t[7]);
}
TEST(gap8_engquerylocaltime_field_order_not_systemtime) {
    REQUIRE(UN(EngQueryLocalTime));
    /* ENG_TIME_FIELDS puts the weekday last, unlike SYSTEMTIME where it is third. */
    USHORT t[8]; SYSTEMTIME s;
    UN(EngQueryLocalTime)(t); GetLocalTime(&s);
    EXPECT_EQ(t[0], s.wYear);
    EXPECT_EQ(t[1], s.wMonth);
    if (t[2] == s.wDay) CHECK(1); else t_note("day changed during test");
}
TEST(gap8_engquerylocaltime_writes_exactly_16_bytes) {
    REQUIRE(UN(EngQueryLocalTime));
    USHORT buf[16];
    for (int i = 0; i < 16; i++) buf[i] = 0xCDCD;
    UN(EngQueryLocalTime)(buf);
    for (int i = 8; i < 16; i++) EXPECT_EQ(buf[i], 0xCDCD);
    CHECK(buf[0] != 0xCDCD);
}
TEST(gap8_engquerylocaltime_monotonic_ms) {
    REQUIRE(UN(EngQueryLocalTime));
    USHORT a[8], b[8];
    UN(EngQueryLocalTime)(a); Sleep(30); UN(EngQueryLocalTime)(b);
    SYSTEMTIME sa = {a[0], a[1], 0, a[2], a[3], a[4], a[5], a[6]}, sb = {b[0], b[1], 0, b[2], b[3], b[4], b[5], b[6]};
    ULONGLONG ua = st_to_ull(&sa), ub = st_to_ull(&sb);
    CHECK(ub >= ua);
    CHECK(ub - ua < 600000000ULL); /* < 60 s */
}
TEST(gap8_engquerylocaltime_thread) {
    REQUIRE(UN(EngQueryLocalTime));
    static USHORT tt[8];
    DWORD WINAPI f(LPVOID p) { UN(EngQueryLocalTime)(tt); return 0; }
    HANDLE h = CreateThread(NULL, 0, f, NULL, 0, NULL);
    WaitForSingleObject(h, 20000); CloseHandle(h);
    CHECK(tt[0] >= 2020);
}

/* ================= EngGetCurrentCodePage ================= */
TEST(gap8_enggetcurrentcodepage_values) {
    REQUIRE(UN(EngGetCurrentCodePage));
    USHORT oem = 0xFFFF, ansi = 0xFFFF;
    UN(EngGetCurrentCodePage)(&oem, &ansi);
    EXPECT_EQ(oem, (USHORT)GetOEMCP());
    EXPECT_EQ(ansi, (USHORT)GetACP());
}
TEST(gap8_enggetcurrentcodepage_argument_order) {
    REQUIRE(UN(EngGetCurrentCodePage));
    USHORT a = 1, b = 2;
    UN(EngGetCurrentCodePage)(&a, &b);
    CHECK(a == (USHORT)GetOEMCP() && b == (USHORT)GetACP());
    if (GetOEMCP() != GetACP()) CHECK(a != b);
}
TEST(gap8_enggetcurrentcodepage_no_overrun) {
    REQUIRE(UN(EngGetCurrentCodePage));
    USHORT buf[4] = { 0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA };
    UN(EngGetCurrentCodePage)(&buf[1], &buf[2]);
    EXPECT_EQ(buf[0], 0xAAAA);
    EXPECT_EQ(buf[3], 0xAAAA);
    EXPECT_EQ(buf[1], (USHORT)GetOEMCP());
    EXPECT_EQ(buf[2], (USHORT)GetACP());
}

/* ================= EngFindResource / EngLoadModule / EngFreeModule ================= */
static const wchar_t *sysmods[] = { L"kernel32.dll", L"user32.dll", L"gdi32.dll", L"ntdll.dll", L"advapi32.dll" };

TEST(gap8_engfindresource_version_info) {
    REQUIRE(UN(EngFindResource));
    for (unsigned i = 0; i < sizeof sysmods / sizeof *sysmods; i++) {
        HMODULE m = GetModuleHandleW(sysmods[i]);
        if (!m) continue;
        ULONG sz = 0xDEADBEEF;
        void *p = UN(EngFindResource)(m, 1, (int)(ULONG_PTR)RT_VERSION, &sz);
        CHECKF(p != NULL, "%ls", sysmods[i]);
        if (!p) continue;
        HRSRC r = FindResourceExW(m, MAKEINTRESOURCEW(16), MAKEINTRESOURCEW(1), 0);
        CHECK(r != NULL);
        EXPECT_EQ(sz, SizeofResource(m, r));
        EXPECT_EQ(p, LockResource(LoadResource(m, r)));
        CHECK(sz > 0x30);
        CHECK(!wcscmp((WCHAR *)((BYTE *)p + 6), L"VS_VERSION_INFO"));
        CHECK(*(WORD *)p <= sz);
    }
}
TEST(gap8_engfindresource_name_truncated_to_16_bits) {
    REQUIRE(UN(EngFindResource));
    HMODULE m = GetModuleHandleW(L"kernel32.dll");
    ULONG s1 = 0, s2 = 0;
    void *a = UN(EngFindResource)(m, 1, 16, &s1);
    void *b = UN(EngFindResource)(m, 0x10001, 16, &s2);
    CHECK(a != NULL);
    EXPECT_EQ(a, b);
    EXPECT_EQ(s1, s2);
}
TEST(gap8_engfindresource_type_masked_to_16_bits) {
    REQUIRE(UN(EngFindResource));
    HMODULE m = GetModuleHandleW(L"kernel32.dll");
    ULONG s1 = 0, s2 = 0;
    void *a = UN(EngFindResource)(m, 1, 16, &s1);
    void *b = UN(EngFindResource)(m, 1, 0x70010, &s2);
    void *c = UN(EngFindResource)(m, 1, (int)0xFFFF0010, &s2);
    CHECK(a != NULL);
    EXPECT_EQ(a, b);
    EXPECT_EQ(a, c);
}
TEST(gap8_engfindresource_not_found_zeroes_size) {
    REQUIRE(UN(EngFindResource));
    HMODULE m = GetModuleHandleW(L"kernel32.dll");
    static const int names[] = { 0xFFFE, 0x7777, 0x5A5A };
    for (unsigned i = 0; i < 3; i++) {
        ULONG sz = 0xDEADBEEF;
        void *p = UN(EngFindResource)(m, names[i], 16, &sz);
        CHECKF(p == NULL, "name 0x%x", names[i]);
        EXPECT_EQ(sz, 0);
    }
    ULONG sz = 0xDEADBEEF;
    EXPECT_EQ(UN(EngFindResource)(m, 1, 0x7FFE, &sz), NULL);
    EXPECT_EQ(sz, 0);
}
TEST(gap8_engfindresource_not_found_error_code) {
    REQUIRE(UN(EngFindResource));
    HMODULE m = GetModuleHandleW(L"kernel32.dll");
    ULONG sz;
    SET_ERR(0);
    CHECK(UN(EngFindResource)(m, 0x7777, 16, &sz) == NULL);
    DWORD e = GetLastError();
    t_note("error for missing name: %lu (1814 expected)", e);
    SET_ERR(0);
    CHECK(UN(EngFindResource)(m, 1, 0x7FFE, &sz) == NULL);
    t_note("error for missing type: %lu (1813 expected)", GetLastError());
    CHECK(e == ERROR_RESOURCE_NAME_NOT_FOUND || e == ERROR_RESOURCE_TYPE_NOT_FOUND || e == ERROR_RESOURCE_DATA_NOT_FOUND);
}
TEST(gap8_engfindresource_exe_without_resources) {
    REQUIRE(UN(EngFindResource));
    ULONG sz = 0x1234;
    void *p = UN(EngFindResource)(GetModuleHandleW(NULL), 1, 16, &sz);
    t_note("exe resource lookup -> %p size %lu", p, sz);
    if (!p) EXPECT_EQ(sz, 0);
}
TEST(gap8_engfindresource_matches_both_exports) {
    REQUIRE(t_proc_gdi32("EngFindResource")); REQUIRE(t_proc_gdi32full("EngFindResource"));
    PVOID (WINAPI *a)(HANDLE, int, int, ULONG *) = (void *)t_proc_gdi32("EngFindResource");
    PVOID (WINAPI *b)(HANDLE, int, int, ULONG *) = (void *)t_proc_gdi32full("EngFindResource");
    HMODULE m = GetModuleHandleW(L"user32.dll");
    ULONG s1 = 0, s2 = 0;
    void *p1 = a(m, 1, 16, &s1), *p2 = b(m, 1, 16, &s2);
    CHECK(p1 != NULL);
    EXPECT_EQ(p1, p2);
    EXPECT_EQ(s1, s2);
}
TEST(gap8_engfindresource_other_types) {
    REQUIRE(UN(EngFindResource));
    /* user32 carries string tables (type 6), accelerators/dialogs vary: only check parity with FindResourceExW. */
    HMODULE m = GetModuleHandleW(L"user32.dll");
    int found = 0;
    for (int type = 1; type <= 24; type++)
        for (int name = 1; name <= 12; name++) {
            ULONG sz = 7;
            void *p = UN(EngFindResource)(m, name, type, &sz);
            HRSRC r = FindResourceExW(m, MAKEINTRESOURCEW(type), MAKEINTRESOURCEW(name), 0);
            if (r && SizeofResource(m, r)) {
                found++;
                if (p != LockResource(LoadResource(m, r)) || sz != SizeofResource(m, r)) CHECKF(0, "type %d name %d mismatch", type, name);
            } else if (p || sz) CHECKF(0, "type %d name %d: unexpected hit", type, name);
        }
    CHECK(found >= 1);
}
static HMODULE load_data(const wchar_t *n) { return UN(EngLoadModule)((LPWSTR)n); }
#define NEED_MOD() do { REQUIRE(UN(EngLoadModule)); REQUIRE(UN(EngFreeModule)); } while (0)

TEST(gap8_engloadmodule_null_and_empty) {
    NEED_MOD();
    SET_ERR(0);
    CHECK(UN(EngLoadModule)(NULL) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(load_data(L"") == NULL);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(load_data(L"   ") == NULL);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(load_data(L" ") == NULL);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
}
TEST(gap8_engloadmodule_not_found) {
    NEED_MOD();
    static const wchar_t *names[] = { L"no_such_module_gap8.dll", L"C:\\no_such_dir_gap8\\x.dll", L"no_such_module_gap8", L".\\nope_gap8.dll" };
    for (unsigned i = 0; i < 4; i++) {
        SET_ERR(0);
        HMODULE m = load_data(names[i]);
        CHECKF(m == NULL, "%ls", names[i]);
        if (m) { UN(EngFreeModule)(m); continue; }
        DWORD e = GetLastError();
        CHECKF(e == ERROR_MOD_NOT_FOUND || e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND, "%ls err %lu", names[i], e);
    }
}
TEST(gap8_engloadmodule_not_found_exact_error) {
    NEED_MOD();
    SET_ERR(0);
    CHECK(load_data(L"no_such_module_gap8.dll") == NULL);
    DWORD e = GetLastError();
    t_note("missing module error %lu", e);
    CHECK(e == ERROR_MOD_NOT_FOUND || e == ERROR_FILE_NOT_FOUND);
}
TEST(gap8_engloadmodule_overlong_name) {
    NEED_MOD();
    static wchar_t big[40000];
    for (int i = 0; i < 39990; i++) big[i] = L'a';
    big[39990] = 0;
    SET_ERR(0);
    HMODULE m = load_data(big);
    CHECK(m == NULL);
    t_note("overlong name error %lu", GetLastError());
    if (m) UN(EngFreeModule)(m);
}
TEST(gap8_engloadmodule_as_datafile) {
    NEED_MOD();
    wchar_t path[MAX_PATH + 16];
    GetSystemDirectoryW(path, MAX_PATH);
    wcscat(path, L"\\msimg32.dll");
    HMODULE m = load_data(path);
    CHECK(m != NULL);
    if (!m) return;
    t_note("datafile handle %p (low bits %d)", (void *)m, (int)((ULONG_PTR)m & 3));
    /* not mapped as an executable image: no entry in the loader list for this name */
    ULONG sz = 0;
    void *p = UN(EngFindResource)(m, 1, 16, &sz);
    CHECK(p != NULL);
    CHECK(sz > 0x30);
    if (p) CHECK(!wcscmp((WCHAR *)((BYTE *)p + 6), L"VS_VERSION_INFO"));
    CHECK(UN(EngFreeModule)(m));
}
TEST(gap8_engloadmodule_data_handle_is_resource_only) {
    NEED_MOD();
    wchar_t path[MAX_PATH + 16];
    GetSystemDirectoryW(path, MAX_PATH);
    wcscat(path, L"\\msimg32.dll");
    HMODULE pre = GetModuleHandleW(L"msimg32.dll");
    HMODULE m = load_data(path);
    CHECK(m != NULL);
    if (!m) return;
    if (!pre) CHECK(GetModuleHandleW(L"msimg32.dll") == NULL);
    CHECK(UN(EngFreeModule)(m));
    if (!pre) CHECK(GetModuleHandleW(L"msimg32.dll") == NULL);
}
TEST(gap8_engloadmodule_trailing_spaces_trimmed) {
    NEED_MOD();
    HMODULE m = load_data(L"msimg32.dll   ");
    CHECK(m != NULL);
    if (m) CHECK(UN(EngFreeModule)(m));
}
TEST(gap8_engloadmodule_relative_name_searches_system) {
    NEED_MOD();
    HMODULE m = load_data(L"msimg32.dll");
    CHECK(m != NULL);
    if (m) CHECK(UN(EngFreeModule)(m));
    m = load_data(L"MSIMG32");
    t_note("extensionless name -> %p", (void *)m);
    if (m) UN(EngFreeModule)(m);
}
TEST(gap8_engloadmodule_repeat_balanced) {
    NEED_MOD();
    wchar_t path[MAX_PATH + 16];
    GetSystemDirectoryW(path, MAX_PATH);
    wcscat(path, L"\\msimg32.dll");
    HMODULE h[20];
    for (int i = 0; i < 20; i++) { h[i] = load_data(path); if (!h[i]) { CHECKF(0, "iter %d", i); } }
    for (int i = 1; i < 20; i++) if (h[i] && h[i] == h[0]) t_note("data-file handles are shared");
    for (int i = 0; i < 20; i++) if (h[i]) CHECK(UN(EngFreeModule)(h[i]));
}
TEST(gap8_engloadmodule_loaded_image_module) {
    NEED_MOD();
    HMODULE m = load_data(L"kernel32.dll");
    CHECK(m != NULL);
    t_note("kernel32 via EngLoadModule: %p vs image %p", (void *)m, (void *)GetModuleHandleW(L"kernel32.dll"));
    if (m) {
        ULONG sz = 0;
        CHECK(UN(EngFindResource)(m, 1, 16, &sz) != NULL);
        if ((ULONG_PTR)m & 3) CHECK(UN(EngFreeModule)(m));   /* a real data-file mapping; an image handle is left alone */
    }
    CHECK(GetModuleHandleW(L"kernel32.dll") != NULL);
}
TEST(gap8_engfreemodule_null) {
    NEED_MOD();
    SET_ERR(0);
    CHECK(!UN(EngFreeModule)(NULL));
    t_note("FreeModule(NULL) error %lu", GetLastError());
    CHECK(GetLastError() != 0);
}
TEST(gap8_engfreemodule_regular_loadlibrary_refcount) {
    NEED_MOD();
    if (GetModuleHandleW(L"msimg32.dll")) { t_skip("msimg32 already loaded"); return; }
    HMODULE a = LoadLibraryW(L"msimg32.dll");
    HMODULE b = LoadLibraryW(L"msimg32.dll");
    CHECK(a != NULL && a == b);
    CHECK(UN(EngFreeModule)(a));
    CHECK(GetModuleHandleW(L"msimg32.dll") != NULL);
    CHECK(UN(EngFreeModule)(b));
    CHECK(GetModuleHandleW(L"msimg32.dll") == NULL);
}
TEST(gap8_engfreemodule_not_loaded_image_base) {
    NEED_MOD();
    /* an aligned address that is not a loaded module */
    SET_ERR(0);
    BOOL r = UN(EngFreeModule)((HMODULE)(ULONG_PTR)0x10000);
    t_note("FreeModule(0x10000) = %d err %lu", r, GetLastError());
    CHECK(!r);
}
TEST(gap8_engfreemodule_exports) {
    G8_NOWINE_ALWAYS();
    CHECK(t_proc_gdi32("EngFreeModule") != NULL);
    CHECK(t_proc_gdi32("EngLoadModule") != NULL);
    CHECK(t_proc_gdi32full("EngFreeModule") != NULL);
    CHECK(t_proc_gdi32full("EngLoadModule") != NULL);
    CHECK(t_proc_gdi32full("EngFindResource") != NULL);
}

/* ================= string conversion ================= */
#define NEED_CONV() do { REQUIRE(UN(EngMultiByteToWideChar)); REQUIRE(UN(EngWideCharToMultiByte)); } while (0)

TEST(gap8_engmbtowc_basic_ascii) {
    NEED_CONV();
    WCHAR out[16]; memset(out, 0xEE, sizeof out);
    int n = UN(EngMultiByteToWideChar)(1252, out, sizeof out, "Hello", 5);
    EXPECT_EQ(n, 5);
    CHECK(!wmemcmp(out, L"Hello", 5));
    EXPECT_EQ(out[5], 0xEEEE);
}
TEST(gap8_engmbtowc_with_terminator) {
    NEED_CONV();
    WCHAR out[16];
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1252, out, sizeof out, "Hello", 6), 6);
    CHECK(!wcscmp(out, L"Hello"));
}
TEST(gap8_engmbtowc_minus_one_is_nul_terminated) {
    NEED_CONV();
    WCHAR out[16];
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1252, out, sizeof out, "Hello", -1), 6);
    CHECK(!wcscmp(out, L"Hello"));
}
TEST(gap8_engmbtowc_size_query_zero) {
    NEED_CONV();
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1252, NULL, 0, "Hello", 5), 5);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1252, NULL, 0, "Hello", -1), 6);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1252, NULL, 0, "a", 1), 1);
}
TEST(gap8_engmbtowc_size_query_one_byte_buffer) {
    NEED_CONV();
    /* byte count 1 rounds down to zero characters, which is the size-query form */
    WCHAR out[4] = { 0x1111, 0x1111, 0x1111, 0x1111 };
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1252, out, 1, "Hello", 5), 5);
    EXPECT_EQ(out[0], 0x1111);
}
TEST(gap8_engmbtowc_buffer_edges) {
    NEED_CONV();
    WCHAR out[16];
    SET_ERR(0);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1252, out, 10, "Hello", 5), 5);          /* exact */
    SET_ERR(0);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1252, out, 9, "Hello", 5), 0);           /* exact - 1 byte: only 4 chars */
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    SET_ERR(0);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1252, out, 8, "Hello", 5), 0);
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    SET_ERR(0);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1252, out, 2, "Hello", 5), 0);
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1252, out, 11, "Hello", 5), 5);          /* odd larger */
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1252, out, 32, "Hello", 5), 5);
}
TEST(gap8_engmbtowc_single_char_fits_exactly) {
    NEED_CONV();
    WCHAR out[2] = { 0x7777, 0x7777 };
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1252, out, 2, "Z", 1), 1);
    EXPECT_EQ(out[0], L'Z');
    EXPECT_EQ(out[1], 0x7777);
}
TEST(gap8_engmbtowc_invalid_params) {
    NEED_CONV();
    WCHAR out[8];
    SET_ERR(0);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1252, out, sizeof out, "Hello", 0), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1252, out, sizeof out, NULL, 5), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(99999, out, sizeof out, "Hello", 5), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(0xFFFFFFFF, out, sizeof out, "Hello", 5), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    int r2 = UN(EngMultiByteToWideChar)(1252, out, sizeof out, "Hello", -2);
    t_note("cbIn -2 -> %d (error %lu)", r2, GetLastError());
}
TEST(gap8_engmbtowc_codepages_high_bytes) {
    NEED_CONV();
    WCHAR out[4];
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1252, out, sizeof out, "\x80", 1), 1); EXPECT_EQ(out[0], 0x20AC);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1252, out, sizeof out, "\xE9", 1), 1); EXPECT_EQ(out[0], 0x00E9);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1252, out, sizeof out, "\x99", 1), 1); EXPECT_EQ(out[0], 0x2122);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(437, out, sizeof out, "\xE9", 1), 1);  EXPECT_EQ(out[0], 0x0398);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(437, out, sizeof out, "\xB0", 1), 1);  EXPECT_EQ(out[0], 0x2591);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1251, out, sizeof out, "\xC0", 1), 1); EXPECT_EQ(out[0], 0x0410);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1253, out, sizeof out, "\xE1", 1), 1); EXPECT_EQ(out[0], 0x03B1);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(28591, out, sizeof out, "\xFF", 1), 1); EXPECT_EQ(out[0], 0x00FF);
}
TEST(gap8_engmbtowc_dbcs_cp932) {
    NEED_CONV();
    WCHAR out[4];
    EXPECT_EQ(UN(EngMultiByteToWideChar)(932, out, sizeof out, "\x82\xA0", 2), 1);
    EXPECT_EQ(out[0], 0x3042);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(932, out, sizeof out, "\x82\xA0\x41", 3), 2);
    EXPECT_EQ(out[1], L'A');
    EXPECT_EQ(UN(EngMultiByteToWideChar)(932, NULL, 0, "\x82\xA0\x82\xA2", 4), 2);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(932, out, sizeof out, "\xB1", 1), 1);   /* half-width katakana */
    EXPECT_EQ(out[0], 0xFF71);
}
TEST(gap8_engmbtowc_utf8) {
    NEED_CONV();
    WCHAR out[8];
    EXPECT_EQ(UN(EngMultiByteToWideChar)(CP_UTF8, out, sizeof out, "\xC3\xA9", 2), 1); EXPECT_EQ(out[0], 0x00E9);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(CP_UTF8, out, sizeof out, "\xE2\x82\xAC", 3), 1); EXPECT_EQ(out[0], 0x20AC);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(CP_UTF8, out, sizeof out, "\xF0\x9F\x98\x80", 4), 2);
    EXPECT_EQ(out[0], 0xD83D); EXPECT_EQ(out[1], 0xDE00);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(CP_UTF8, NULL, 0, "\xF0\x9F\x98\x80", 4), 2);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(CP_UTF8, out, 2, "\xF0\x9F\x98\x80", 4), 0); /* one char of room for a pair */
}
TEST(gap8_engmbtowc_utf8_invalid_replaced) {
    NEED_CONV();
    WCHAR out[8];
    /* flags are always 0 so malformed input is replaced with U+FFFD, not rejected */
    int n = UN(EngMultiByteToWideChar)(CP_UTF8, out, sizeof out, "\xFF", 1);
    EXPECT_EQ(n, 1);
    if (n == 1) EXPECT_EQ(out[0], 0xFFFD);
    n = UN(EngMultiByteToWideChar)(CP_UTF8, out, sizeof out, "\xC3", 1);
    EXPECT_EQ(n, 1);
    if (n == 1) EXPECT_EQ(out[0], 0xFFFD);
}
TEST(gap8_engmbtowc_acp_and_oemcp_symbolic) {
    NEED_CONV();
    WCHAR a[4], b[4];
    EXPECT_EQ(UN(EngMultiByteToWideChar)(CP_ACP, a, sizeof a, "A", 1), 1);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(CP_OEMCP, b, sizeof b, "A", 1), 1);
    EXPECT_EQ(a[0], L'A'); EXPECT_EQ(b[0], L'A');
    EXPECT_EQ(UN(EngMultiByteToWideChar)(GetACP(), a, sizeof a, "\xE9", 1), 1);
    WCHAR c[4];
    EXPECT_EQ(UN(EngMultiByteToWideChar)(CP_ACP, c, sizeof c, "\xE9", 1), 1);
    EXPECT_EQ(a[0], c[0]);
}
TEST(gap8_engmbtowc_embedded_nul) {
    NEED_CONV();
    WCHAR out[8];
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1252, out, sizeof out, "a\0b", 3), 3);
    EXPECT_EQ(out[1], 0); EXPECT_EQ(out[2], L'b');
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1252, out, sizeof out, "a\0b", -1), 2);
}
TEST(gap8_engmbtowc_matches_win32_api) {
    NEED_CONV();
    static const char *s[] = { "", "x", "Hello, World", "caf\xE9", "\x80\x81\x8D\x8F\x90\x9D" };
    for (unsigned i = 1; i < 5; i++) {
        WCHAR a[64], b[64];
        int la = UN(EngMultiByteToWideChar)(1252, a, sizeof a, (LPSTR)s[i], (int)strlen(s[i]));
        int lb = MultiByteToWideChar(1252, 0, s[i], (int)strlen(s[i]), b, 64);
        EXPECT_EQ(la, lb);
        CHECK(!wmemcmp(a, b, lb));
    }
}
TEST(gap8_engmbtowc_long_string) {
    NEED_CONV();
    static char in[20000]; static WCHAR out[20000];
    memset(in, 'q', sizeof in);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1252, out, sizeof out, in, sizeof in), 20000);
    EXPECT_EQ(UN(EngMultiByteToWideChar)(1252, out, 39998, in, sizeof in), 0);
}

TEST(gap8_engwctomb_basic_ascii) {
    NEED_CONV();
    char out[16]; memset(out, 0xEE, sizeof out);
    EXPECT_EQ(UN(EngWideCharToMultiByte)(1252, L"Hello", 10, out, sizeof out), 5);
    CHECK(!memcmp(out, "Hello", 5));
    EXPECT_EQ((unsigned char)out[5], 0xEE);
}
TEST(gap8_engwctomb_with_terminator) {
    NEED_CONV();
    char out[16];
    EXPECT_EQ(UN(EngWideCharToMultiByte)(1252, L"Hello", 12, out, sizeof out), 6);
    CHECK(!strcmp(out, "Hello"));
}
TEST(gap8_engwctomb_minus_one_is_nul_terminated) {
    NEED_CONV();
    char out[16];
    EXPECT_EQ(UN(EngWideCharToMultiByte)(1252, L"Hello", -1, out, sizeof out), 6);
    CHECK(!strcmp(out, "Hello"));
}
TEST(gap8_engwctomb_size_query) {
    NEED_CONV();
    EXPECT_EQ(UN(EngWideCharToMultiByte)(1252, L"Hello", 10, NULL, 0), 5);
    EXPECT_EQ(UN(EngWideCharToMultiByte)(1252, L"Hello", -1, NULL, 0), 6);
    EXPECT_EQ(UN(EngWideCharToMultiByte)(CP_UTF8, L"\x00E9", 2, NULL, 0), 2);
    EXPECT_EQ(UN(EngWideCharToMultiByte)(CP_UTF8, L"\x20AC", 2, NULL, 0), 3);
}
TEST(gap8_engwctomb_byte_count_input_semantics) {
    NEED_CONV();
    char out[16];
    EXPECT_EQ(UN(EngWideCharToMultiByte)(1252, L"Hello", 2, out, sizeof out), 1);   /* 2 bytes = 1 char */
    EXPECT_EQ(out[0], 'H');
    EXPECT_EQ(UN(EngWideCharToMultiByte)(1252, L"Hello", 3, out, sizeof out), 1);   /* odd rounds down */
    EXPECT_EQ(UN(EngWideCharToMultiByte)(1252, L"Hello", 9, out, sizeof out), 4);
    EXPECT_EQ(UN(EngWideCharToMultiByte)(1252, L"Hello", 10, out, sizeof out), 5);
}
TEST(gap8_engwctomb_buffer_edges) {
    NEED_CONV();
    char out[16];
    EXPECT_EQ(UN(EngWideCharToMultiByte)(1252, L"Hello", 10, out, 5), 5);
    SET_ERR(0);
    EXPECT_EQ(UN(EngWideCharToMultiByte)(1252, L"Hello", 10, out, 4), 0);
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    SET_ERR(0);
    EXPECT_EQ(UN(EngWideCharToMultiByte)(1252, L"Hello", 10, out, 1), 0);
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(UN(EngWideCharToMultiByte)(1252, L"Hello", 10, out, 6), 5);
}
TEST(gap8_engwctomb_invalid_params) {
    NEED_CONV();
    char out[8];
    SET_ERR(0);
    EXPECT_EQ(UN(EngWideCharToMultiByte)(1252, L"Hello", 0, out, sizeof out), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_EQ(UN(EngWideCharToMultiByte)(1252, NULL, 10, out, sizeof out), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_EQ(UN(EngWideCharToMultiByte)(99999, L"Hello", 10, out, sizeof out), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_EQ(UN(EngWideCharToMultiByte)(0xFFFFFFFF, L"Hello", 10, out, sizeof out), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_EQ(UN(EngWideCharToMultiByte)(1252, L"Hello", 1, out, sizeof out), 0);   /* 1 byte = 0 chars */
    t_note("cbIn 1 error %lu", GetLastError());
}
TEST(gap8_engwctomb_codepages) {
    NEED_CONV();
    char out[8];
    EXPECT_EQ(UN(EngWideCharToMultiByte)(1252, L"\x20AC", 2, out, sizeof out), 1); EXPECT_EQ((unsigned char)out[0], 0x80);
    EXPECT_EQ(UN(EngWideCharToMultiByte)(1252, L"\x00E9", 2, out, sizeof out), 1); EXPECT_EQ((unsigned char)out[0], 0xE9);
    EXPECT_EQ(UN(EngWideCharToMultiByte)(437, L"\x0398", 2, out, sizeof out), 1);  EXPECT_EQ((unsigned char)out[0], 0xE9);
    EXPECT_EQ(UN(EngWideCharToMultiByte)(1251, L"\x0410", 2, out, sizeof out), 1); EXPECT_EQ((unsigned char)out[0], 0xC0);
    EXPECT_EQ(UN(EngWideCharToMultiByte)(932, L"\x3042", 2, out, sizeof out), 2);
    EXPECT_EQ((unsigned char)out[0], 0x82); EXPECT_EQ((unsigned char)out[1], 0xA0);
}
TEST(gap8_engwctomb_unmappable_uses_default_char) {
    NEED_CONV();
    char out[8];
    /* flags 0, no default char supplied: system default ('?') is substituted, no failure */
    EXPECT_EQ(UN(EngWideCharToMultiByte)(1252, L"\x3042", 2, out, sizeof out), 1);
    EXPECT_EQ(out[0], '?');
    EXPECT_EQ(UN(EngWideCharToMultiByte)(1252, L"\x4E2D\x6587", 4, out, sizeof out), 2);
    EXPECT_EQ(out[0], '?'); EXPECT_EQ(out[1], '?');
}
TEST(gap8_engwctomb_utf8) {
    NEED_CONV();
    char out[16];
    EXPECT_EQ(UN(EngWideCharToMultiByte)(CP_UTF8, L"\x00E9", 2, out, sizeof out), 2);
    EXPECT_EQ((unsigned char)out[0], 0xC3); EXPECT_EQ((unsigned char)out[1], 0xA9);
    EXPECT_EQ(UN(EngWideCharToMultiByte)(CP_UTF8, L"\x20AC", 2, out, sizeof out), 3);
    EXPECT_EQ(UN(EngWideCharToMultiByte)(CP_UTF8, L"\xD83D\xDE00", 4, out, sizeof out), 4);
    EXPECT_EQ((unsigned char)out[0], 0xF0); EXPECT_EQ((unsigned char)out[1], 0x9F);
    EXPECT_EQ((unsigned char)out[2], 0x98); EXPECT_EQ((unsigned char)out[3], 0x80);
    EXPECT_EQ(UN(EngWideCharToMultiByte)(CP_UTF8, L"\xD83D\xDE00", 4, out, 3), 0);
}
TEST(gap8_engwctomb_lone_surrogate_utf8) {
    NEED_CONV();
    char out[16];
    int n = UN(EngWideCharToMultiByte)(CP_UTF8, L"\xD83D", 2, out, sizeof out);
    t_note("lone high surrogate -> %d bytes", n);
    CHECK(n == 3 || n == 0 || n == 1);
}
TEST(gap8_engwctomb_acp_oemcp_symbolic) {
    NEED_CONV();
    char a[8], b[8];
    EXPECT_EQ(UN(EngWideCharToMultiByte)(CP_ACP, L"A", 2, a, sizeof a), 1);
    EXPECT_EQ(UN(EngWideCharToMultiByte)(CP_OEMCP, L"A", 2, b, sizeof b), 1);
    EXPECT_EQ(a[0], 'A'); EXPECT_EQ(b[0], 'A');
}
TEST(gap8_engwctomb_matches_win32_api) {
    NEED_CONV();
    static const WCHAR *s[] = { L"x", L"Hello, World", L"caf\x00E9", L"\x20AC\x2122" };
    for (unsigned i = 0; i < 4; i++) {
        char a[64], b[64];
        int len = (int)wcslen(s[i]);
        int la = UN(EngWideCharToMultiByte)(1252, (LPWSTR)s[i], len * 2, a, sizeof a);
        int lb = WideCharToMultiByte(1252, 0, s[i], len, b, 64, NULL, NULL);
        EXPECT_EQ(la, lb);
        CHECK(!memcmp(a, b, lb));
    }
}
TEST(gap8_engconv_round_trip_cp1252) {
    NEED_CONV();
    char all[224]; WCHAR w[300]; char back[300];
    for (int i = 0; i < 224; i++) all[i] = (char)(0x20 + i);
    int nw = UN(EngMultiByteToWideChar)(1252, w, sizeof w, all, 224);
    EXPECT_EQ(nw, 224);
    int nb = UN(EngWideCharToMultiByte)(1252, w, nw * 2, back, sizeof back);
    EXPECT_EQ(nb, 224);
    /* five bytes in 1252 are undefined (81 8D 8F 90 9D) and map to C1 controls */
    CHECK(!memcmp(all, back, 224));
}
TEST(gap8_engconv_exports) {
    G8_NOWINE_ALWAYS();
    CHECK(t_proc_gdi32("EngMultiByteToWideChar") != NULL);
    CHECK(t_proc_gdi32("EngWideCharToMultiByte") != NULL);
    CHECK(t_proc_gdi32("EngMultiByteToUnicodeN") != NULL);
    CHECK(t_proc_gdi32("EngUnicodeToMultiByteN") != NULL);
    CHECK(t_proc_gdi32full("EngMultiByteToWideChar") != NULL);
    CHECK(t_proc_gdi32full("EngMultiByteToUnicodeN") != NULL);
}

/* ---- the *N variants map straight to the Rtl routines (ANSI code page based) ---- */
TEST(gap8_engmbtounicoden_ascii) {
    REQUIRE(UN(EngMultiByteToUnicodeN));
    WCHAR out[16]; ULONG cb = 0xFFFF;
    memset(out, 0xEE, sizeof out);
    LONG st = UN(EngMultiByteToUnicodeN)(out, sizeof out, &cb, "Hello", 5);
    EXPECT_EQ(st, 0);
    EXPECT_EQ(cb, 10);
    CHECK(!wmemcmp(out, L"Hello", 5));
    EXPECT_EQ(out[5], 0xEEEE);
}
TEST(gap8_engmbtounicoden_truncation) {
    REQUIRE(UN(EngMultiByteToUnicodeN));
    WCHAR out[16]; ULONG cb = 0xFFFF;
    memset(out, 0xEE, sizeof out);
    LONG st = UN(EngMultiByteToUnicodeN)(out, 6, &cb, "Hello", 5);
    EXPECT_EQ(st, 0);
    EXPECT_EQ(cb, 6);
    CHECK(!wmemcmp(out, L"Hel", 3));
    EXPECT_EQ(out[3], 0xEEEE);
    cb = 0xFFFF;
    UN(EngMultiByteToUnicodeN)(out, 5, &cb, "Hello", 5);   /* odd byte limit */
    EXPECT_EQ(cb, 4);
}
TEST(gap8_engmbtounicoden_zero_sizes) {
    REQUIRE(UN(EngMultiByteToUnicodeN));
    WCHAR out[4] = { 0x1111 }; ULONG cb = 0xFFFF;
    UN(EngMultiByteToUnicodeN)(out, 0, &cb, "Hello", 5);
    EXPECT_EQ(cb, 0);
    EXPECT_EQ(out[0], 0x1111);
    cb = 0xFFFF;
    UN(EngMultiByteToUnicodeN)(out, 8, &cb, "", 0);
    EXPECT_EQ(cb, 0);
}
TEST(gap8_engmbtounicoden_null_result_size) {
    REQUIRE(UN(EngMultiByteToUnicodeN));
    WCHAR out[16];
    LONG st = UN(EngMultiByteToUnicodeN)(out, sizeof out, NULL, "Hi", 2);
    EXPECT_EQ(st, 0);
    CHECK(!wmemcmp(out, L"Hi", 2));
}
TEST(gap8_engmbtounicoden_embedded_nul_kept) {
    REQUIRE(UN(EngMultiByteToUnicodeN));
    WCHAR out[8]; ULONG cb = 0;
    UN(EngMultiByteToUnicodeN)(out, sizeof out, &cb, "a\0b", 3);
    EXPECT_EQ(cb, 6);
    EXPECT_EQ(out[1], 0); EXPECT_EQ(out[2], L'b');
}
TEST(gap8_engunicodetomultibyten_ascii) {
    REQUIRE(UN(EngUnicodeToMultiByteN));
    char out[16]; ULONG cb = 0xFFFF;
    memset(out, 0xEE, sizeof out);
    LONG st = UN(EngUnicodeToMultiByteN)(out, sizeof out, &cb, L"Hello", 10);
    EXPECT_EQ(st, 0);
    EXPECT_EQ(cb, 5);
    CHECK(!memcmp(out, "Hello", 5));
    EXPECT_EQ((unsigned char)out[5], 0xEE);
}
TEST(gap8_engunicodetomultibyten_truncation_and_odd) {
    REQUIRE(UN(EngUnicodeToMultiByteN));
    char out[16]; ULONG cb = 0xFFFF;
    UN(EngUnicodeToMultiByteN)(out, 3, &cb, L"Hello", 10);
    EXPECT_EQ(cb, 3);
    CHECK(!memcmp(out, "Hel", 3));
    cb = 0xFFFF;
    UN(EngUnicodeToMultiByteN)(out, sizeof out, &cb, L"Hello", 9);   /* odd byte count */
    EXPECT_EQ(cb, 4);
    cb = 0xFFFF;
    UN(EngUnicodeToMultiByteN)(out, 0, &cb, L"Hello", 10);
    EXPECT_EQ(cb, 0);
}

/* ================= EngComputeGlyphSet ================= */
TEST(gap8_engcomputeglyphset_without_driver_context) {
    G8_NOWINE(); REQUIRE(UN(EngComputeGlyphSet));
    static const int cp[] = { 0, 437, 1252, 932, 65001, -1, 99999 };
    for (unsigned i = 0; i < sizeof cp / sizeof *cp; i++) {
        HGLOBAL g = UN(EngComputeGlyphSet)(cp[i], 0x20, 0x60);
        CHECKF(g == NULL, "codepage %d", cp[i]);
        if (g) GlobalFree(g);
    }
}
TEST(gap8_engcomputeglyphset_ranges) {
    G8_NOWINE(); REQUIRE(UN(EngComputeGlyphSet));
    static const int first[] = { 0, 1, 0x20, 0xFFFF, -1 };
    static const int cnt[]   = { 0, 1, 0x100, 0xFFFF, -1, INT_MAX };
    for (unsigned i = 0; i < 5; i++) for (unsigned j = 0; j < 6; j++) {
        HGLOBAL g = UN(EngComputeGlyphSet)(1252, first[i], cnt[j]);
        CHECKF(g == NULL, "first %d count %d", first[i], cnt[j]);
        if (g) GlobalFree(g);
    }
}
TEST(gap8_engcomputeglyphset_repeat_no_leak) {
    G8_NOWINE(); REQUIRE(UN(EngComputeGlyphSet));
    UINT before = g8_gdicount();
    for (int i = 0; i < 100; i++) {
        HGLOBAL g = UN(EngComputeGlyphSet)(1252, 0x20, 0x60);
        if (g) { CHECKF(0, "non-null at %d", i); GlobalFree(g); break; }
    }
    EXPECT_EQ(g8_gdicount(), before);
}
TEST(gap8_engcomputeglyphset_thread) {
    G8_NOWINE(); REQUIRE(UN(EngComputeGlyphSet));
    static HGLOBAL volatile r;
    DWORD WINAPI f(LPVOID p) { r = UN(EngComputeGlyphSet)(1252, 0x20, 0x60); return 0; }
    r = (HGLOBAL)1;
    HANDLE h = CreateThread(NULL, 0, f, NULL, 0, NULL);
    WaitForSingleObject(h, 20000); CloseHandle(h);
    CHECK(r == NULL);
}
TEST(gap8_engcomputeglyphset_exports) {
    G8_NOWINE_ALWAYS();
    CHECK(t_proc_gdi32("EngComputeGlyphSet") != NULL);
    CHECK(t_proc_gdi32full("EngComputeGlyphSet") != NULL);
}

/* ================= printer-driver queries on bogus hdev ================= */
TEST(gap8_enggetdrivername_bogus_hdev) {
    G8_NOWINE(); REQUIRE(UN(EngGetDriverName));
    static const ULONG_PTR v[] = { 0, 1, 0x1000, 0xDEADBEEF, (ULONG_PTR)-1 };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++)
        CHECKF(UN(EngGetDriverName)((HANDLE)v[i]) == NULL, "hdev %llx", (unsigned long long)v[i]);
}
TEST(gap8_enggetprinterdatafilename_bogus_hdev) {
    G8_NOWINE(); REQUIRE(UN(EngGetPrinterDataFileName));
    static const ULONG_PTR v[] = { 0, 1, 0x1000, 0xDEADBEEF, (ULONG_PTR)-1 };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++)
        CHECKF(UN(EngGetPrinterDataFileName)((HANDLE)v[i]) == NULL, "hdev %llx", (unsigned long long)v[i]);
}
TEST(gap8_enggetdrivername_display_dc_handle_is_not_hdev) {
    G8_NOWINE(); REQUIRE(UN(EngGetDriverName)); REQUIRE(UN(EngGetPrinterDataFileName)); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(DeleteDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(dc != NULL);
    CHECK(UN(EngGetDriverName)((HANDLE)dc) == NULL);
    CHECK(UN(EngGetPrinterDataFileName)((HANDLE)dc) == NULL);
    U(DeleteDC)(dc);
}
TEST(gap8_enggetdrivername_repeat_and_thread) {
    G8_NOWINE(); REQUIRE(UN(EngGetDriverName));
    for (int i = 0; i < 100; i++) if (UN(EngGetDriverName)((HANDLE)(ULONG_PTR)(0x1000 + i))) { CHECKF(0, "iter %d", i); return; }
    CHECK(1);
}
TEST(gap8_engqueryemfinfo_always_false) {
    REQUIRE(UN(EngQueryEMFInfo));
    UCHAR info[256]; memset(info, 0x5A, sizeof info);
    EXPECT_EQ(UN(EngQueryEMFInfo)(NULL, NULL), 0);
    EXPECT_EQ(UN(EngQueryEMFInfo)(NULL, info), 0);
    EXPECT_EQ(UN(EngQueryEMFInfo)((HANDLE)(ULONG_PTR)0x1234, info), 0);
    EXPECT_EQ(UN(EngQueryEMFInfo)((HANDLE)(ULONG_PTR)-1, (PVOID)(ULONG_PTR)-1), 0);   /* never dereferenced */
    for (unsigned i = 0; i < sizeof info; i++) if (info[i] != 0x5A) { CHECKF(0, "buffer touched at %u", i); break; }
}
TEST(gap8_engqueryemfinfo_exports_and_lasterror) {
    G8_NOWINE_ALWAYS();
    REQUIRE(UN(EngQueryEMFInfo));
    CHECK(t_proc_gdi32("EngQueryEMFInfo") != NULL);
    CHECK(t_proc_gdi32full("EngQueryEMFInfo") != NULL);
    SET_ERR(0x4242);
    UN(EngQueryEMFInfo)(NULL, NULL);
    EXPECT_ERR(0, 0x4242);
}
TEST(gap8_engqueryemfinfo_both_exports_agree) {
    REQUIRE(t_proc_gdi32("EngQueryEMFInfo")); REQUIRE(t_proc_gdi32full("EngQueryEMFInfo"));
    BOOL (WINAPI *a)(HANDLE, PVOID) = (void *)t_proc_gdi32("EngQueryEMFInfo");
    BOOL (WINAPI *b)(HANDLE, PVOID) = (void *)t_proc_gdi32full("EngQueryEMFInfo");
    EXPECT_EQ(a(NULL, NULL), 0);
    EXPECT_EQ(b(NULL, NULL), 0);
}

/* ================= export presence for every name in this area ================= */
TEST(gap8_all_area_exports_in_gdi32) {
    G8_NOWINE_ALWAYS();
    static const char *const n[] = {
        "EngAlphaBlend","EngAssociateSurface","EngBitBlt","EngCheckAbort","EngComputeGlyphSet","EngCopyBits","EngCreateBitmap",
        "EngCreateClip","EngCreateDeviceBitmap","EngCreateDeviceSurface","EngCreatePalette","EngCreateSemaphore","EngDeleteClip",
        "EngDeletePalette","EngDeletePath","EngDeleteSemaphore","EngDeleteSurface","EngEraseSurface","EngFillPath","EngFindResource",
        "EngFreeModule","EngGetDriverName","EngGetPrinterDataFileName","EngGradientFill","EngLineTo","EngLoadModule","EngLockSurface",
        "EngMarkBandingSurface","EngMultiByteToUnicodeN","EngMultiByteToWideChar","EngPaint","EngPlgBlt","EngQueryEMFInfo",
        "EngQueryLocalTime","EngReleaseSemaphore","EngStretchBlt","EngStretchBltROP","EngStrokeAndFillPath","EngStrokePath","EngTextOut" };
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++) CHECKF(t_proc_gdi32(n[i]) != NULL, "%s", n[i]);
}
TEST(gap8_nt_syscall_names_exported_by_gdi32full) {
    G8_NOWINE_ALWAYS();
    static const char *const n[] = {
        "NtGdiEngAlphaBlend","NtGdiEngAssociateSurface","NtGdiEngBitBlt","NtGdiEngCheckAbort","NtGdiEngComputeGlyphSet",
        "NtGdiEngCopyBits","NtGdiEngCreateClip","NtGdiEngDeleteClip","NtGdiEngDeletePath","NtGdiEngEraseSurface","NtGdiEngFillPath",
        "NtGdiEngGradientFill","NtGdiEngLineTo","NtGdiEngLockSurface","NtGdiEngMarkBandingSurface","NtGdiEngPaint","NtGdiEngPlgBlt",
        "NtGdiEngStretchBlt","NtGdiEngStretchBltROP","NtGdiEngStrokeAndFillPath","NtGdiEngStrokePath","NtGdiEngTextOut" };
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++) CHECKF(t_proc_gdi32full(n[i]) != NULL, "%s", n[i]);
}
TEST(gap8_gdi32_stub_vs_gdi32full_address_note) {
    G8_NOWINE_ALWAYS();
    static const char *const n[] = { "EngCreateBitmap", "EngCreateSemaphore", "EngQueryLocalTime", "EngFindResource", "EngLoadModule" };
    for (unsigned i = 0; i < 5; i++) {
        FARPROC a = t_proc_gdi32(n[i]), b = t_proc_gdi32full(n[i]);
        t_note("%s gdi32=%p gdi32full=%p %s", n[i], (void *)a, (void *)b, a == b ? "(same)" : "(distinct)");
        CHECK(a != NULL && b != NULL);
    }
}
