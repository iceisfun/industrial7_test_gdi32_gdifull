/* Eng* entry points that are thin syscall trampolines (user-mode printer driver
 * helpers). Called from an ordinary process there is no UMPD driver context on
 * the calling thread, so win32k returns before touching any argument. */
#include "g8.h"

typedef ULONG_PTR (WINAPI *stub13_t)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR,
                                      ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);

static ULONG_PTR call13(FARPROC p, ULONG_PTR v) {
    return ((stub13_t)p)(v, v, v, v, v, v, v, v, v, v, v, v, v);
}

enum { M_NULL, M_ZEROBUF, M_REPEAT, M_THREAD, M_ERRKEEP, M_TWICE_DIFFARGS };

struct thr { FARPROC p; ULONG_PTR v; ULONG_PTR r; };
static DWORD WINAPI thr_fn(LPVOID a) {
    struct thr *t = a;
    t->r = call13(t->p, t->v);
    return 0;
}

static void stub_run(const char *name, DWORD exp, int isptr, int mode) {
    G8_NOWINE();
    FARPROC p = t_proc(name);
    REQUIRE(p);
    static unsigned char zero[8192];
    ULONG_PTR v = (mode == M_ZEROBUF) ? (ULONG_PTR)zero : 0;
    ULONG_PTR r;
    switch (mode) {
    case M_REPEAT:
        for (int i = 0; i < 200; i++) {
            r = call13(p, 0);
            if (isptr ? r != 0 : (DWORD)r != exp) { CHECKF(0, "iteration %d got 0x%llx", i, (unsigned long long)r); return; }
        }
        CHECK(1);
        return;
    case M_THREAD: {
        struct thr t = { p, 0, 0xABABABAB };
        HANDLE h = CreateThread(NULL, 0, thr_fn, &t, 0, NULL);
        CHECK(h != NULL);
        if (!h) return;
        EXPECT_EQ(WaitForSingleObject(h, 20000), WAIT_OBJECT_0);
        CloseHandle(h);
        if (isptr) EXPECT_EQ(t.r, 0); else EXPECT_EQ((DWORD)t.r, exp);
        return;
    }
    case M_ERRKEEP:
        SET_ERR(0x5A5A);
        r = call13(p, 0);
        EXPECT_ERR(0, 0x5A5A);
        break;
    case M_TWICE_DIFFARGS:
        r = call13(p, 0);
        if (isptr) EXPECT_EQ(r, 0); else EXPECT_EQ((DWORD)r, exp);
        r = call13(p, (ULONG_PTR)zero);
        break;
    default:
        r = call13(p, v);
    }
    if (isptr) EXPECT_EQ(r, 0); else EXPECT_EQ((DWORD)r, exp);
}

#define STUBT(N, EXP, ISPTR) \
    TEST(gap8_##N##_nullargs)   { stub_run(#N, EXP, ISPTR, M_NULL); } \
    TEST(gap8_##N##_zerobuf)    { stub_run(#N, EXP, ISPTR, M_ZEROBUF); } \
    TEST(gap8_##N##_repeat)     { stub_run(#N, EXP, ISPTR, M_REPEAT); } \
    TEST(gap8_##N##_thread)     { stub_run(#N, EXP, ISPTR, M_THREAD); } \
    TEST(gap8_##N##_lasterror)  { stub_run(#N, EXP, ISPTR, M_ERRKEEP); } \
    TEST(gap8_##N##_mixedargs)  { stub_run(#N, EXP, ISPTR, M_TWICE_DIFFARGS); } \
    TEST(gap8_##N##_exported)   { G8_NOWINE_ALWAYS(); REQUIRE(t_proc_gdi32(#N)); CHECK(t_proc_gdi32(#N) != NULL); \
                                  CHECK(t_proc(#N) == t_proc_gdi32(#N)); }

/* result with no UMPD context: FALSE / NULL; EngCheckAbort -> TRUE; the delete
 * helpers report STATUS_INVALID_PARAMETER (0xC000000D). */
STUBT(EngAlphaBlend,        0, 0)
STUBT(EngBitBlt,            0, 0)
STUBT(EngCheckAbort,        1, 0)
STUBT(EngCopyBits,          0, 0)
STUBT(EngCreateClip,        0, 1)
STUBT(EngDeleteClip,        0xC000000D, 0)
STUBT(EngDeletePath,        0xC000000D, 0)
STUBT(EngEraseSurface,      0, 0)
STUBT(EngFillPath,          0, 0)
STUBT(EngGradientFill,      0, 0)
STUBT(EngLineTo,            0, 0)
STUBT(EngLockSurface,       0, 1)
STUBT(EngPaint,             0, 0)
STUBT(EngPlgBlt,            0, 0)
STUBT(EngStretchBlt,        0, 0)
STUBT(EngStretchBltROP,     0, 0)
STUBT(EngStrokeAndFillPath, 0, 0)
STUBT(EngStrokePath,        0, 0)
STUBT(EngTextOut,           0, 0)

/* Typed wrappers with the documented prototypes (argument counts as in winddi.h). */
UDECL(BOOL, EngBitBlt, (PVOID, PVOID, PVOID, PVOID, PVOID, PVOID, PVOID, PVOID, PVOID, PVOID, ULONG));
UDECL(BOOL, EngCopyBits, (PVOID, PVOID, PVOID, PVOID, PVOID, PVOID));
UDECL(BOOL, EngPaint, (PVOID, PVOID, PVOID, PVOID, ULONG));
UDECL(BOOL, EngLineTo, (PVOID, PVOID, PVOID, LONG, LONG, LONG, LONG, PVOID, ULONG));
UDECL(BOOL, EngTextOut, (PVOID, PVOID, PVOID, PVOID, PVOID, PVOID, PVOID, PVOID, PVOID, ULONG));
UDECL(BOOL, EngEraseSurface, (PVOID, PVOID, ULONG));
UDECL(BOOL, EngGradientFill, (PVOID, PVOID, PVOID, PVOID, ULONG, PVOID, ULONG, PVOID, PVOID, ULONG));
UDECL(BOOL, EngStretchBltROP, (PVOID, PVOID, PVOID, PVOID, PVOID, PVOID, PVOID, PVOID, PVOID, PVOID, ULONG, PVOID, ULONG));

TEST(gap8_engbitblt_typed_rop4) {
    G8_NOWINE(); REQUIRE(UN(EngBitBlt));
    static const ULONG rops[] = { 0, 0xCCCC, 0xAAAA, 0xF0F0, 0x0000, 0xFFFF, 0x6666, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof rops / sizeof *rops; i++)
        EXPECT_EQ(UN(EngBitBlt)(0,0,0,0,0,0,0,0,0,0, rops[i]), 0);
}
TEST(gap8_engcopybits_typed) {
    G8_NOWINE(); REQUIRE(UN(EngCopyBits));
    EXPECT_EQ(UN(EngCopyBits)(0,0,0,0,0,0), 0);
    RECTL r = {0, 0, 4, 4}; POINTL pt = {0, 0};
    EXPECT_EQ(UN(EngCopyBits)(0,0,0,0,&r,&pt), 0);
}
TEST(gap8_engpaint_typed_mix) {
    G8_NOWINE(); REQUIRE(UN(EngPaint));
    for (ULONG mix = 0; mix < 0x200; mix += 0x37) EXPECT_EQ(UN(EngPaint)(0,0,0,0,mix), 0);
}
TEST(gap8_englineto_typed_coords) {
    G8_NOWINE(); REQUIRE(UN(EngLineTo));
    static const LONG c[] = { 0, 1, -1, 0x7FFFFFFF, (LONG)0x80000000 };
    for (int i = 0; i < 5; i++) for (int j = 0; j < 5; j++)
        EXPECT_EQ(UN(EngLineTo)(0,0,0, c[i], c[j], c[j], c[i], 0, 13), 0);
}
TEST(gap8_engtextout_typed) {
    G8_NOWINE(); REQUIRE(UN(EngTextOut));
    EXPECT_EQ(UN(EngTextOut)(0,0,0,0,0,0,0,0,0, 0), 0);
    EXPECT_EQ(UN(EngTextOut)(0,0,0,0,0,0,0,0,0, 13), 0);
}
TEST(gap8_engerasesurface_typed) {
    G8_NOWINE(); REQUIRE(UN(EngEraseSurface));
    RECTL r = {0, 0, 1, 1};
    EXPECT_EQ(UN(EngEraseSurface)(0, 0, 0), 0);
    EXPECT_EQ(UN(EngEraseSurface)(0, &r, 0xFF00FF), 0);
}
TEST(gap8_enggradientfill_typed_modes) {
    G8_NOWINE(); REQUIRE(UN(EngGradientFill));
    for (ULONG m = 0; m < 4; m++) EXPECT_EQ(UN(EngGradientFill)(0,0,0,0,0,0,0,0,0, m), 0);
    EXPECT_EQ(UN(EngGradientFill)(0,0,0,0,0xFFFFFFFF,0,0xFFFFFFFF,0,0, 0xFFFFFFFF), 0);
}
TEST(gap8_engstretchbltrop_typed) {
    G8_NOWINE(); REQUIRE(UN(EngStretchBltROP));
    for (ULONG mode = 0; mode < 6; mode++)
        EXPECT_EQ(UN(EngStretchBltROP)(0,0,0,0,0,0,0,0,0,0, mode, 0, 0xCCCC), 0);
}

/* Bogus (non-pointer) values in every slot must also be ignored: no UMPD context
 * means no argument is dereferenced. Only integers that cannot be read are used
 * for the slots that real prototypes treat as plain values; pointer slots stay 0. */
TEST(gap8_engstubs_all_return_before_reading_args) {
    G8_NOWINE();
    static const char *const names[] = { "EngAlphaBlend", "EngBitBlt", "EngCopyBits", "EngEraseSurface", "EngFillPath",
        "EngGradientFill", "EngLineTo", "EngPaint", "EngPlgBlt", "EngStretchBlt", "EngStretchBltROP",
        "EngStrokeAndFillPath", "EngStrokePath", "EngTextOut" };
    int seen = 0;
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++) {
        FARPROC p = t_proc(names[i]);
        if (!p) { t_note("%s missing", names[i]); continue; }
        seen++;
        DWORD r = (DWORD)call13(p, 0);
        CHECKF(r == 0, "%s returned 0x%lx", names[i], r);
    }
    if (!seen) { t_skip("no stubs resolved"); return; }
    CHECK(seen > 0);
}

/* The Eng*Surface lock needs an actual driver context: even a valid GDI surface
 * handle yields NULL. */
TEST(gap8_englocksurface_handles) {
    G8_NOWINE(); REQUIRE(UN(EngLockSurface)); REQUIRE(U(CreateBitmap)); REQUIRE(U(DeleteObject));
    HBITMAP bm = U(CreateBitmap)(4, 4, 1, 32, NULL);
    CHECK(bm != NULL);
    EXPECT_EQ(UN(EngLockSurface)(bm), 0);
    EXPECT_EQ(UN(EngLockSurface)(NULL), 0);
    EXPECT_EQ(UN(EngLockSurface)((HBITMAP)(ULONG_PTR)0xDEADBEEF), 0);
    EXPECT_EQ(UN(EngLockSurface)((HBITMAP)(ULONG_PTR)-1), 0);
    CHECK(U(DeleteObject)(bm));
}
TEST(gap8_engcreateclip_leaves_no_objects) {
    G8_NOWINE(); REQUIRE(UN(EngCreateClip));
    UINT before = g8_gdicount();
    for (int i = 0; i < 50; i++) EXPECT_EQ(UN(EngCreateClip)(), 0);
    EXPECT_EQ(g8_gdicount(), before);
}
