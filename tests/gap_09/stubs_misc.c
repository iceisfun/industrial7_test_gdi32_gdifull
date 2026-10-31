/* GdiEntry*, Moderncore*, and the other trivial stub exports of gdi32/gdi32full. */
#include "gap9.h"

static void other_xor(const char *name)
{
    FARPROC p = t_proc_gdi32full(name);
    if (!p) { t_skip("missing gdi32full export %s", name); return; }
    CHECK(g9_in_module(p, t_gdi32full()));
    if (g9_wine()) { t_skip("wine: %s", name); return; }
    CHECKF(g9_class(p, NULL) == G9_XOR0, "%s class %d", name, g9_class(p, NULL));
    if (g9_class(p, NULL) != G9_XOR0) return;
    EXPECT_EQ(g9_call(p, 0, 0, 0, 0), 0);
    EXPECT_EQ(g9_call(p, G9_BOGUS, G9_BOGUS, G9_BOGUS, G9_BOGUS), 0);
    EXPECT_EQ(g9_call8(p, 1, 2, 3, 4, 5, 6, 7, 8), 0);
    /* repeated calls keep the same result */
    for (int i = 0; i < 16; i++) if (g9_call(p, i, i, i, i) != 0) { CHECKF(0, "%s call %d", name, i); break; }
    /* a gdi32.dll export of the same name, when there is one, reaches the same code */
    FARPROC g = t_proc_gdi32(name);
    if (g) {
        EXPECT_EQ(g9_call(g, 0, 0, 0, 0), 0);
        t_note("%s also exported by gdi32 (%s)", name, g9_follow(g) == g9_follow(p) ? "same target" : "own wrapper");
    }
}
#define X(n) TEST(gap9_other0_##n) { other_xor(#n); }
GAP9_OTHER_XOR_LIST(X)
#undef X

/* ---- GdiEntry1..16 ---- */
static void gdientry_same_target(int i)
{
    char nm[32]; wsprintfA(nm, "GdiEntry%d", i);
    FARPROC g = t_proc_gdi32(nm), f = t_proc_gdi32full(nm);
    if (!g) { t_skip("missing gdi32 %s", nm); return; }
    if (g9_wine()) { t_skip("wine: %s", nm); return; }
    CHECK(g9_in_module(g, t_gdi32()));
    if (i == 13) { CHECK(f == NULL); return; }   /* gdi32-only data reader */
    CHECKF(f != NULL, "gdi32full!%s", nm);
    if (!f) return;
    CHECK(g9_in_module(f, t_gdi32full()));
    CHECKF(((BYTE *)g)[0] == 0xFF && ((BYTE *)g)[1] == 0x25, "%s thunk in gdi32", nm);
    CHECKF(g9_follow(g) == (const BYTE *)f, "%s: gdi32 thunk reaches gdi32full export", nm);
}
#define GE(i) TEST(gap9_gdientry_target_##i) { gdientry_same_target(i); }
GE(1) GE(2) GE(3) GE(4) GE(5) GE(6) GE(7) GE(8) GE(9) GE(10) GE(11) GE(12) GE(13) GE(14) GE(15) GE(16)

TEST(gap9_gdientry_count)
{
    G9_NOWINE("export set differs");
    int g = 0, f = 0;
    for (int i = 0; i < 40; i++) {
        char nm[32]; wsprintfA(nm, "GdiEntry%d", i);
        if (t_proc_gdi32(nm)) g++;
        if (t_proc_gdi32full(nm)) f++;
    }
    CHECKF(g == 16, "gdi32 has %d", g);
    CHECKF(f == 15, "gdi32full has %d", f);
    CHECK(t_proc_gdi32("GdiEntry0") == NULL);
    CHECK(t_proc_gdi32("GdiEntry17") == NULL);
}

TEST(gap9_gdientry_stub_group_returns_zero)
{
    G9_NOWINE("GdiEntry stubs");
    static const int stubs[] = { 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 14, 15 };
    for (size_t k = 0; k < sizeof stubs / sizeof *stubs; k++) {
        char nm[32]; wsprintfA(nm, "GdiEntry%d", stubs[k]);
        FARPROC g = t_proc_gdi32(nm), f = t_proc_gdi32full(nm);
        REQUIRE(g); REQUIRE(f);
        CHECKF(g9_class(f, NULL) == G9_XOR0, "%s", nm);
        EXPECT_EQ(g9_call(g, 1, 2, 3, 4), 0);
        EXPECT_EQ(g9_call(f, 0, 0, 0, 0), 0);
        /* all stubs share one body */
        FARPROC f1 = t_proc_gdi32full("GdiEntry1");
        CHECKF(g9_follow(f) == g9_follow(f1), "%s shares GdiEntry1 body", nm);
    }
}

TEST(gap9_gdientry12_is_plain_return)
{
    G9_NOWINE("GdiEntry12");
    FARPROC f = t_proc_gdi32full("GdiEntry12");
    REQUIRE(f);
    const BYTE *b = (const BYTE *)f;
    CHECKF(b[0] == 0xC2 || b[0] == 0xC3, "first byte 0x%02x", b[0]);
    /* "ret"; callable with anything */
    g9_call(f, 0, 0, 0, 0);
    g9_call(f, G9_BOGUS, G9_BOGUS, G9_BOGUS, G9_BOGUS);
    CHECK(1);
}

TEST(gap9_gdientry16_returns_one)
{
    G9_NOWINE("GdiEntry16");
    FARPROC f = t_proc_gdi32full("GdiEntry16"), g = t_proc_gdi32("GdiEntry16");
    REQUIRE(f); REQUIRE(g);
    CHECK(g9_class(f, NULL) == G9_RET1);
    EXPECT_EQ((DWORD)g9_call(f, 0, 0, 0, 0), 1);
    EXPECT_EQ((DWORD)g9_call(f, G9_BOGUS, G9_BOGUS, 0, 0), 1);
    EXPECT_EQ((DWORD)g9_call(g, 5, 6, 7, 8), 1);
}

TEST(gap9_gdientry13_reads_devcaps_dword)
{
    FARPROC g = t_proc_gdi32("GdiEntry13");
    REQUIRE(g);
    void *const *mem = (void *const *)t_proc_gdi32("pGdiSharedMemory");
    void *const *dc = (void *const *)t_proc_gdi32("pGdiDevCaps");
    REQUIRE(mem); REQUIRE(dc);
    if (!*mem) { t_skip("no shared memory"); return; }
    DWORD want = *(const DWORD *)((const BYTE *)*mem + 0x180090);
    DWORD got = (DWORD)g9_call(g, 0, 0, 0, 0);
    EXPECT_EQ(got, want);
    /* offset 0x90 inside the DEVCAPS block that pGdiDevCaps points at */
    EXPECT_EQ(*(const DWORD *)((const BYTE *)*dc + 0x90), want);
    EXPECT_EQ((DWORD)g9_call(g, 1, 2, 3, 4), want);
    t_note("GdiEntry13() = 0x%lx", (unsigned long)got);
}

/* ---- Moderncore*: gdi32 trampolines resolve the gdi32full implementation by name, which is a stub ---- */
TEST(gap9_moderncore_createicw_returns_null)
{
    G9_NOWINE("Moderncore");
    FARPROC g = t_proc_gdi32("ModerncoreCreateICW");
    REQUIRE(g);
    EXPECT_EQ(g9_call(g, 0, 0, 0, 0), 0);
    static const WCHAR disp[] = L"DISPLAY";
    EXPECT_EQ(g9_call(g, (ULONG_PTR)disp, 0, 0, 0), 0);
    EXPECT_EQ(g9_call(g, (ULONG_PTR)disp, (ULONG_PTR)disp, 0, 0), 0);
    DWORD before = g9_gdi_count();
    for (int i = 0; i < 20; i++) g9_call(g, (ULONG_PTR)disp, 0, 0, 0);
    EXPECT_EQ(g9_gdi_count(), before);
}

TEST(gap9_moderncore_deletedc_returns_zero)
{
    G9_NOWINE("Moderncore");
    FARPROC g = t_proc_gdi32("ModerncoreDeleteDC");
    REQUIRE(g);
    EXPECT_EQ(g9_call(g, 0, 0, 0, 0), 0);
    EXPECT_EQ(g9_call(g, G9_BOGUS, 0, 0, 0), 0);
    /* a real DC is left alone: the call must not delete it */
    HDC (WINAPI *ccdc)(HDC) = (void *)t_proc("CreateCompatibleDC");
    BOOL (WINAPI *dd)(HDC) = (void *)t_proc("DeleteDC");
    DWORD (WINAPI *got)(HGDIOBJ) = (void *)t_proc("GetObjectType");
    REQUIRE(ccdc); REQUIRE(dd); REQUIRE(got);
    HDC dc = ccdc(NULL);
    REQUIRE(dc);
    EXPECT_EQ(g9_call(g, (ULONG_PTR)dc, 0, 0, 0), 0);
    EXPECT_EQ(got(dc), OBJ_MEMDC);
    CHECK(dd(dc));
}

TEST(gap9_moderncore_gdiinit_returns_zero)
{
    G9_NOWINE("Moderncore");
    FARPROC g = t_proc_gdi32("ModerncoreGdiInit");
    REQUIRE(g);
    EXPECT_EQ(g9_call(g, 0, 0, 0, 0), 0);
    EXPECT_EQ(g9_call(g, 1, 1, 1, 1), 0);
}

TEST(gap9_moderncore_gdi32_trampoline_shape)
{
    G9_NOWINE("Moderncore");
    const char *const n[] = { "ModerncoreCreateICW", "ModerncoreDeleteDC", "ModerncoreGdiInit" };
    for (int i = 0; i < 3; i++) {
        FARPROC g = t_proc_gdi32(n[i]), f = t_proc_gdi32full(n[i]);
        REQUIRE(g); REQUIRE(f);
        CHECK(g9_in_module(g, t_gdi32()));
        CHECK(g9_in_module(f, t_gdi32full()));
        CHECKF(g != f, "%s: gdi32 has its own wrapper", n[i]);
        CHECKF(g9_class(f, NULL) == G9_XOR0, "%s in gdi32full", n[i]);
    }
}

/* ---- Eng* exports that gdi32 re-exports as raw syscall thunks ---- */
TEST(gap9_eng_transparentblt_shape)
{
    G9_NOWINE("Eng thunks");
    FARPROC p = t_proc_gdi32("EngTransparentBlt");
    REQUIRE(p);
    CHECK(g9_in_module(p, t_gdi32()));
    unsigned no = 0;
    int c = g9_class(p, &no);
    CHECKF(c == G9_SYSCALL, "class %d", c);
    CHECKF(no >= 0x1000 && no < 0x2000, "sysno 0x%x", no);
    FARPROC nt = t_proc_gdi32full("NtGdiEngTransparentBlt");
    REQUIRE(nt);
    CHECK(g9_follow(p) == g9_follow(nt));
    /* NtGdiEngTransparentBlt is the same service */
    HMODULE u = g9_win32u();
    if (u) CHECK(g9_follow(p) == (const BYTE *)GetProcAddress(u, "NtGdiEngTransparentBlt"));
}

TEST(gap9_eng_unlocksurface_shape)
{
    G9_NOWINE("Eng thunks");
    FARPROC p = t_proc_gdi32("EngUnlockSurface");
    REQUIRE(p);
    CHECK(g9_in_module(p, t_gdi32()));
    unsigned no = 0;
    int c = g9_class(p, &no);
    CHECKF(c == G9_SYSCALL, "class %d", c);
    CHECKF(no >= 0x1000 && no < 0x2000, "sysno 0x%x", no);
    FARPROC nt = t_proc_gdi32full("NtGdiEngUnlockSurface");
    REQUIRE(nt);
    CHECK(g9_follow(p) == g9_follow(nt));
    FARPROC lk = t_proc_gdi32full("NtGdiEngLockSurface");
    if (lk) { unsigned n2; if (g9_class(lk, &n2) == G9_SYSCALL) CHECK(n2 != no); }
}

TEST(gap9_eng_exports_not_in_gdi32full)
{
    G9_NOWINE("export set differs");
    /* EngTransparentBlt / EngUnlockSurface are gdi32-only; the two string helpers live in both */
    CHECK(t_proc_gdi32("EngTransparentBlt") != NULL);
    CHECK(t_proc_gdi32("EngUnlockSurface") != NULL);
    CHECK(t_proc_gdi32full("EngTransparentBlt") == NULL);
    CHECK(t_proc_gdi32full("EngUnlockSurface") == NULL);
    CHECK(t_proc_gdi32("EngWideCharToMultiByte") != NULL);
    CHECK(t_proc_gdi32full("EngWideCharToMultiByte") != NULL);
    CHECK(t_proc_gdi32("EngUnicodeToMultiByteN") != NULL);
    CHECK(t_proc_gdi32full("EngUnicodeToMultiByteN") != NULL);
}
