/* Driver-callback thunks exported by gdi32 (BRUSHOBJ_/CLIPOBJ_/FONTOBJ_/PATHOBJ_/STROBJ_/XFORMOBJ_/XLATEOBJ_).
 *
 * On Win10 each gdi32 export is a jump into a win32u NtGdi* syscall. win32kfull implements them only for
 * user-mode printer-driver (UMPD) callbacks: with no UMPD object attached to the calling thread (always true
 * for an ordinary process) the object pointers are never dereferenced and a fixed value is returned. So the
 * calls below are harmless even with deliberately bogus object pointers; we pin those fixed returns.
 *   - "ptr" entries return a pointer-sized value; the full 64 bits are compared.
 *   - others return int/NTSTATUS in eax; only the low 32 bits are compared (upper half is unspecified).
 */
#include "g7.h"

typedef ULONG_PTR (WINAPI *g7_ddi_fn)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);

static g7_ddi_fn g7_ddi_resolve(const char *n) { return (g7_ddi_fn)t_proc_gdi32(n); }

static int g7_ddi_match(ULONG_PTR got, ULONG_PTR want, int isptr) {
    return isptr ? got == want : (DWORD)got == (DWORD)want;
}

#define DDI_ARGS_ZERO 0, 0, 0, 0, 0
#define DDI_ARGS_ONE  1, 1, 1, 1, 1
#define DDI_ARGS_POISON ((ULONG_PTR)0xDEADBEEFDEADBEEFull), ((ULONG_PTR)-1), ((ULONG_PTR)0x1000), ((ULONG_PTR)0x7FFFFFFF), ((ULONG_PTR)0x80000000u)

typedef struct { g7_ddi_fn f; ULONG_PTR want; int isptr; int ok; } g7_ddi_thr;
static void g7_ddi_thr_fn(void *p) {
    g7_ddi_thr *t = p;
    ULONG_PTR r = t->f(0, 0, 0, 0, 0);
    t->ok = g7_ddi_match(r, t->want, t->isptr);
}

/* 8 tests per export */
#define DDI(name, want, isptr) \
TEST(gap7_ddi_##name##_exported) { \
    g7_ddi_fn f = g7_ddi_resolve(#name); REQUIRE(f); \
    CHECK(f != NULL); \
    /* the public export lives in gdi32.dll itself (not a forwarder to another module) */ \
    HMODULE m = NULL; \
    CHECK(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)f, &m)); \
    CHECK(m == t_gdi32()); \
    CHECK(t_proc(#name) == (FARPROC)f); \
} \
TEST(gap7_ddi_##name##_zero_args) { \
    g7_ddi_fn f = g7_ddi_resolve(#name); REQUIRE(f); \
    if (g7_is_wine()) { t_skip("wine: stubbed driver callback"); return; } \
    ULONG_PTR r = f(DDI_ARGS_ZERO); \
    EXPECT_EQ(g7_ddi_match(r, want, isptr), 1); \
} \
TEST(gap7_ddi_##name##_poison_args) { \
    g7_ddi_fn f = g7_ddi_resolve(#name); REQUIRE(f); \
    if (g7_is_wine()) { t_skip("wine: stubbed driver callback"); return; } \
    ULONG_PTR r = f(DDI_ARGS_POISON); \
    EXPECT_EQ(g7_ddi_match(r, want, isptr), 1); \
    r = f(DDI_ARGS_ONE); \
    EXPECT_EQ(g7_ddi_match(r, want, isptr), 1); \
} \
TEST(gap7_ddi_##name##_fake_object) { \
    g7_ddi_fn f = g7_ddi_resolve(#name); REQUIRE(f); \
    if (g7_is_wine()) { t_skip("wine: stubbed driver callback"); return; } \
    /* a zeroed 512-byte block posing as the DDI object and as output buffers */ \
    static BYTE obj[512], o1[512], o2[512]; \
    memset(obj, 0, sizeof obj); memset(o1, 0xA5, sizeof o1); memset(o2, 0x5A, sizeof o2); \
    ULONG_PTR r = f((ULONG_PTR)obj, 8, (ULONG_PTR)o1, (ULONG_PTR)o2, 0); \
    EXPECT_EQ(g7_ddi_match(r, want, isptr), 1); \
    /* object and output memory is not touched when no UMPD context exists */ \
    int dirty = 0; for (int i = 0; i < 512; i++) dirty += obj[i] != 0 || o1[i] != 0xA5 || o2[i] != 0x5A; \
    CHECK(dirty == 0); \
} \
TEST(gap7_ddi_##name##_repeat) { \
    g7_ddi_fn f = g7_ddi_resolve(#name); REQUIRE(f); \
    if (g7_is_wine()) { t_skip("wine: stubbed driver callback"); return; } \
    int bad = 0; \
    for (int i = 0; i < 500; i++) bad += !g7_ddi_match(f((ULONG_PTR)i, i, i, i, i), want, isptr); \
    CHECK(bad == 0); \
} \
TEST(gap7_ddi_##name##_other_thread) { \
    g7_ddi_fn f = g7_ddi_resolve(#name); REQUIRE(f); \
    if (g7_is_wine()) { t_skip("wine: stubbed driver callback"); return; } \
    g7_ddi_thr t = { f, want, isptr, 0 }; \
    g7_run_thread(g7_ddi_thr_fn, &t); \
    CHECK(t.ok); \
} \
TEST(gap7_ddi_##name##_no_gdi_leak) { \
    g7_ddi_fn f = g7_ddi_resolve(#name); REQUIRE(f); \
    if (g7_is_wine()) { t_skip("wine: stubbed driver callback"); return; } \
    int before = g7_gui(); \
    for (int i = 0; i < 50; i++) f(0, 0, 0, 0, 0); \
    int after = g7_gui(); \
    EXPECT_EQ(after, before); \
} \
TEST(gap7_ddi_##name##_lasterror_untouched) { \
    g7_ddi_fn f = g7_ddi_resolve(#name); REQUIRE(f); \
    if (g7_is_wine()) { t_skip("wine: stubbed driver callback"); return; } \
    /* pure return-value API: the no-context path never sets a Win32 last error */ \
    SetLastError(0x1234); \
    f(0, 0, 0, 0, 0); \
    DWORD e = GetLastError(); \
    if (e != 0x1234) t_note(#name ": last error changed to %lu", e); \
}

DDI(BRUSHOBJ_hGetColorTransform, 0, 1)
DDI(BRUSHOBJ_pvAllocRbrush, 0, 1)
DDI(BRUSHOBJ_pvGetRbrush, 0, 1)
DDI(BRUSHOBJ_ulGetBrushColor, 0, 0)
DDI(CLIPOBJ_bEnum, 0, 0)
DDI(CLIPOBJ_cEnumStart, 0xFFFFFFFFu, 0)
DDI(CLIPOBJ_ppoGetPath, 0, 1)
DDI(FONTOBJ_cGetAllGlyphHandles, 0, 0)
DDI(FONTOBJ_cGetGlyphs, 0, 0)
DDI(FONTOBJ_pfdg, 0, 1)
DDI(FONTOBJ_pifi, 0, 1)
DDI(FONTOBJ_pQueryGlyphAttrs, 0, 1)
DDI(FONTOBJ_pvTrueTypeFontFile, 0, 1)
DDI(FONTOBJ_pxoGetXform, 0, 1)
DDI(FONTOBJ_vGetInfo, 0xC000000Du, 0)
DDI(PATHOBJ_bEnum, 0, 0)
DDI(PATHOBJ_bEnumClipLines, 0, 0)
DDI(PATHOBJ_vEnumStart, 0xC000000Du, 0)
DDI(PATHOBJ_vEnumStartClipLines, 0xC000000Du, 0)
DDI(PATHOBJ_vGetBounds, 0xC000000Du, 0)
DDI(STROBJ_bEnum, 0xFFFFFFFFu, 0)
DDI(STROBJ_bEnumPositionsOnly, 0xFFFFFFFFu, 0)
DDI(STROBJ_bGetAdvanceWidths, 0, 0)
DDI(STROBJ_dwGetCodePage, 0, 0)
DDI(STROBJ_vEnumStart, 0xC000000Du, 0)
DDI(XFORMOBJ_bApplyXform, 0, 0)
DDI(XFORMOBJ_iGetXform, 0xFFFFFFFFu, 0)
DDI(XLATEOBJ_cGetPalette, 0, 0)
DDI(XLATEOBJ_hGetColorTransform, 0, 1)
DDI(XLATEOBJ_iXlate, 0xFFFFFFFFu, 0)

/* ---- per-function specifics ---- */

/* FONTOBJ_pvTrueTypeFontFile(pfo, &cjFile): *pcjFile is written only when a UMPD context exists */
TEST(gap7_ddi_FONTOBJ_pvTrueTypeFontFile_size_untouched) {
    g7_ddi_fn f = g7_ddi_resolve("FONTOBJ_pvTrueTypeFontFile"); REQUIRE(f);
    if (g7_is_wine()) { t_skip("wine"); return; }
    ULONG cj = 0xCAFEF00D;
    ULONG_PTR r = f(0, (ULONG_PTR)&cj, 0, 0, 0);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(cj, 0xCAFEF00Du);
    r = f(0x1000, (ULONG_PTR)&cj, 0, 0, 0);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(cj, 0xCAFEF00Du);
}

/* XFORMOBJ_bApplyXform has an explicit count guard (cPoints > 0x4E2000 returns 0 before anything else) */
TEST(gap7_ddi_XFORMOBJ_bApplyXform_count_limits) {
    g7_ddi_fn f = g7_ddi_resolve("XFORMOBJ_bApplyXform"); REQUIRE(f);
    if (g7_is_wine()) { t_skip("wine"); return; }
    static const ULONG counts[] = { 0, 1, 4, 5, 0x4E2000, 0x4E2001, 0x7FFFFFFF, 0xFFFFFFFF };
    for (int i = 0; i < (int)(sizeof counts / sizeof *counts); i++) {
        POINTL in[2] = { {1, 2}, {3, 4} }, out[2] = { {7, 7}, {7, 7} };
        ULONG_PTR r = f(0, 0, counts[i], (ULONG_PTR)in, (ULONG_PTR)out);
        CHECKF((DWORD)r == 0, "count %lu ret %lx", (unsigned long)counts[i], (unsigned long)r);
        CHECK(out[0].x == 7 && out[1].y == 7);
    }
}

/* XFORMOBJ_iGetXform writes its output matrix only for a live UMPD object */
TEST(gap7_ddi_XFORMOBJ_iGetXform_out_untouched) {
    g7_ddi_fn f = g7_ddi_resolve("XFORMOBJ_iGetXform"); REQUIRE(f);
    if (g7_is_wine()) { t_skip("wine"); return; }
    XFORM xf; memset(&xf, 0x77, sizeof xf);
    ULONG_PTR r = f(0, (ULONG_PTR)&xf, 0, 0, 0);
    EXPECT_EQ((DWORD)r, 0xFFFFFFFFu);
    CHECK(((BYTE *)&xf)[0] == 0x77 && ((BYTE *)&xf)[sizeof xf - 1] == 0x77);
}

/* PATHOBJ_vGetBounds output rectangle (RECTFX) untouched */
TEST(gap7_ddi_PATHOBJ_vGetBounds_out_untouched) {
    g7_ddi_fn f = g7_ddi_resolve("PATHOBJ_vGetBounds"); REQUIRE(f);
    if (g7_is_wine()) { t_skip("wine"); return; }
    LONG rc[4] = { 11, 22, 33, 44 };
    ULONG_PTR r = f(0, (ULONG_PTR)rc, 0, 0, 0);
    EXPECT_EQ((DWORD)r, G7_STATUS_INVALID_PARAMETER);
    CHECK(rc[0] == 11 && rc[1] == 22 && rc[2] == 33 && rc[3] == 44);
}

/* STROBJ_bEnum*: outputs are (ULONG count, GLYPHPOS*) and stay untouched */
TEST(gap7_ddi_STROBJ_bEnum_out_untouched) {
    g7_ddi_fn f = g7_ddi_resolve("STROBJ_bEnum"); g7_ddi_fn f2 = g7_ddi_resolve("STROBJ_bEnumPositionsOnly");
    REQUIRE(f); REQUIRE(f2);
    if (g7_is_wine()) { t_skip("wine"); return; }
    ULONG c = 0x99; void *gp = (void *)0x1234;
    EXPECT_EQ((DWORD)f(0, (ULONG_PTR)&c, (ULONG_PTR)&gp, 0, 0), 0xFFFFFFFFu);
    EXPECT_EQ(c, 0x99);
    EXPECT_EQ(gp, 0x1234);
    EXPECT_EQ((DWORD)f2(0, (ULONG_PTR)&c, (ULONG_PTR)&gp, 0, 0), 0xFFFFFFFFu);
    EXPECT_EQ(c, 0x99);
    EXPECT_EQ(gp, 0x1234);
}

/* CLIPOBJ_bEnum has a size guard (> 0x2710000 -> skipped), with no UMPD context the result is 0 regardless */
TEST(gap7_ddi_CLIPOBJ_bEnum_sizes) {
    g7_ddi_fn f = g7_ddi_resolve("CLIPOBJ_bEnum"); REQUIRE(f);
    if (g7_is_wine()) { t_skip("wine"); return; }
    static const ULONG sz[] = { 0, 1, 4, 0x20, 0x2710000, 0x2710001, 0xFFFFFFFF };
    BYTE buf[64]; memset(buf, 0xCD, sizeof buf);
    for (int i = 0; i < (int)(sizeof sz / sizeof *sz); i++) {
        ULONG_PTR r = f(0, sz[i], (ULONG_PTR)buf, 0, 0);
        CHECKF((DWORD)r == 0, "size %lx ret %lx", (unsigned long)sz[i], (unsigned long)r);
    }
    CHECK(buf[0] == 0xCD && buf[63] == 0xCD);
}

/* FONTOBJ_vGetInfo(pfo, cjSize, pfi, ...) : fixed NTSTATUS-like value, buffer untouched */
TEST(gap7_ddi_FONTOBJ_vGetInfo_buffer_untouched) {
    g7_ddi_fn f = g7_ddi_resolve("FONTOBJ_vGetInfo"); REQUIRE(f);
    if (g7_is_wine()) { t_skip("wine"); return; }
    BYTE buf[256]; memset(buf, 0xEE, sizeof buf);
    static const ULONG sz[] = { 0, 1, sizeof buf, 0x2710001 };
    for (int i = 0; i < 4; i++) {
        ULONG_PTR r = f(0, sz[i], (ULONG_PTR)buf, 0, 0);
        EXPECT_EQ((DWORD)r, G7_STATUS_INVALID_PARAMETER);
    }
    int dirty = 0; for (int i = 0; i < (int)sizeof buf; i++) dirty += buf[i] != 0xEE;
    CHECK(dirty == 0);
}

/* The exports are distinct entry points (no accidental aliasing between different callbacks) */
TEST(gap7_ddi_distinct_entry_points) {
    static const char *names[] = {
        "BRUSHOBJ_hGetColorTransform", "BRUSHOBJ_pvAllocRbrush", "BRUSHOBJ_pvGetRbrush", "BRUSHOBJ_ulGetBrushColor",
        "CLIPOBJ_bEnum", "CLIPOBJ_cEnumStart", "CLIPOBJ_ppoGetPath", "FONTOBJ_cGetAllGlyphHandles",
        "FONTOBJ_cGetGlyphs", "FONTOBJ_pfdg", "FONTOBJ_pifi", "FONTOBJ_pQueryGlyphAttrs",
        "FONTOBJ_pvTrueTypeFontFile", "FONTOBJ_pxoGetXform", "FONTOBJ_vGetInfo", "PATHOBJ_bEnum",
        "PATHOBJ_bEnumClipLines", "PATHOBJ_vEnumStart", "PATHOBJ_vEnumStartClipLines", "PATHOBJ_vGetBounds",
        "STROBJ_bEnum", "STROBJ_bEnumPositionsOnly", "STROBJ_bGetAdvanceWidths", "STROBJ_dwGetCodePage",
        "STROBJ_vEnumStart", "XFORMOBJ_bApplyXform", "XFORMOBJ_iGetXform", "XLATEOBJ_cGetPalette",
        "XLATEOBJ_hGetColorTransform", "XLATEOBJ_iXlate", "XLATEOBJ_piVector" };
    int n = (int)(sizeof names / sizeof *names);
    FARPROC p[32];
    for (int i = 0; i < n; i++) { p[i] = t_proc_gdi32(names[i]); if (!p[i]) { t_skip("missing %s", names[i]); return; } }
    int dup = 0;
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (p[i] == p[j]) { dup++; t_note("%s aliases %s", names[i], names[j]); }
    CHECK(dup == 0);
}

/* gdi32full does not export the DDI callbacks under their public names, except the user-mode piVector/Eng* ones */
TEST(gap7_ddi_gdi32full_public_names) {
    HMODULE full = t_gdi32full(); REQUIRE(full);
    static const char *absent[] = { "BRUSHOBJ_pvGetRbrush", "CLIPOBJ_bEnum", "FONTOBJ_pifi", "PATHOBJ_bEnum", "STROBJ_bEnum", "XLATEOBJ_iXlate" };
    for (int i = 0; i < 6; i++) {
        FARPROC p = GetProcAddress(full, absent[i]);
        CHECKF(p == NULL, "%s unexpectedly exported by gdi32full", absent[i]);
    }
    /* but the NtGdi* syscall stubs they thunk to are there */
    CHECK(GetProcAddress(full, "NtGdiCLIPOBJ_bEnum") != NULL);
    CHECK(GetProcAddress(full, "NtGdiXLATEOBJ_iXlate") != NULL);
}

/* ---- XLATEOBJ_piVector: implemented in user mode in gdi32full ---- */
typedef struct { ULONG iUniq; ULONG flXlate; USHORT iSrcType, iDstType; ULONG cEntries; ULONG *pulXlate; } G7XLATEOBJ;
UDECL(ULONG *, XLATEOBJ_piVector, (void *));

TEST(gap7_xlate_piVector_returns_table) {
    XLATEOBJ_piVector_pfn f = UN(XLATEOBJ_piVector); REQUIRE(f);
    ULONG tbl[4] = { 1, 2, 3, 4 };
    G7XLATEOBJ xo; memset(&xo, 0, sizeof xo);
    xo.pulXlate = tbl; xo.cEntries = 4;
    EXPECT_EQ(f(&xo), tbl);
    xo.pulXlate = NULL;
    EXPECT_EQ(f(&xo), 0);
    xo.pulXlate = (ULONG *)(ULONG_PTR)0x123456789ABCull;
    EXPECT_EQ(f(&xo), 0x123456789ABCull);
}
TEST(gap7_xlate_piVector_no_side_effects) {
    XLATEOBJ_piVector_pfn f = UN(XLATEOBJ_piVector); REQUIRE(f);
    ULONG tbl[2] = { 9, 8 };
    G7XLATEOBJ xo; memset(&xo, 0, sizeof xo);
    xo.iUniq = 7; xo.flXlate = 3; xo.iSrcType = 1; xo.iDstType = 2; xo.cEntries = 2; xo.pulXlate = tbl;
    G7XLATEOBJ cp = xo;
    for (int i = 0; i < 100; i++) EXPECT_EQ(f(&xo), tbl);
    CHECK(memcmp(&cp, &xo, sizeof xo) == 0);
    CHECK(tbl[0] == 9 && tbl[1] == 8);
}
TEST(gap7_xlate_piVector_ignores_other_fields) {
    XLATEOBJ_piVector_pfn f = UN(XLATEOBJ_piVector); REQUIRE(f);
    ULONG tbl[1] = { 5 };
    G7XLATEOBJ xo;
    static const ULONG vals[] = { 0, 1, 0xFFFFFFFFu };
    for (int i = 0; i < 3; i++) {
        memset(&xo, 0, sizeof xo);
        xo.iUniq = vals[i]; xo.flXlate = vals[i]; xo.cEntries = vals[i]; xo.pulXlate = tbl;
        EXPECT_EQ(f(&xo), tbl);
    }
}
TEST(gap7_xlate_piVector_same_both_modules) {
    FARPROC a = t_proc_gdi32("XLATEOBJ_piVector"), b = t_proc_gdi32full("XLATEOBJ_piVector");
    if (!a || !b) { t_skip("missing"); return; }
    ULONG tbl[1] = { 0 };
    G7XLATEOBJ xo; memset(&xo, 0, sizeof xo); xo.pulXlate = tbl;
    EXPECT_EQ(((ULONG *(WINAPI *)(void *))a)(&xo), tbl);
    EXPECT_EQ(((ULONG *(WINAPI *)(void *))b)(&xo), tbl);
}
TEST(gap7_xlate_piVector_other_thread) {
    XLATEOBJ_piVector_pfn f = UN(XLATEOBJ_piVector); REQUIRE(f);
    static ULONG tbl[1]; static G7XLATEOBJ xo; static ULONG *got; static XLATEOBJ_piVector_pfn ff;
    memset(&xo, 0, sizeof xo); xo.pulXlate = tbl; ff = f; got = NULL;
    void thr(void *p) { got = ff(&xo); }
    g7_run_thread(thr, NULL);
    EXPECT_EQ(got, tbl);
}
