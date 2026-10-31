/* Direct NtGdi* syscalls through gdi32full's thunks, invalid-handle paths.
 * Each function takes a GDI handle first; a bogus / NULL / deleted / wrong-type handle must make it fail with 0. */
#include "gap9.h"

enum { K_DC, K_RGN, K_OBJ };
enum { V_BOGUS, V_NULL, V_DELETED, V_WRONGTYPE, V_NEGONE, V_HIGHBITS };

static HGDIOBJ mkrgn(void)
{
    FARPROC c = t_proc_gdi32full("NtGdiCreateRectRgn");
    return c ? (HGDIOBJ)g9_call(c, 0, 0, 4, 4) : NULL;
}

static ULONG_PTR make_handle(int kind, int variant, HGDIOBJ *keep)
{
    *keep = NULL;
    switch (variant) {
    case V_BOGUS: return G9_BOGUS;
    case V_NULL: return 0;
    case V_NEGONE: return (ULONG_PTR)-1;
    case V_HIGHBITS: return 0xFFFFFFFF00000001ull;
    case V_DELETED: {
        HGDIOBJ h = NULL;
        if (kind == K_DC) { h = U(CreateCompatibleDC)(NULL); if (h) U(DeleteDC)((HDC)h); }
        else if (kind == K_RGN) { h = mkrgn(); if (h) g9_ntdel(h); }
        else { h = U(CreateBitmap)(1, 1, 1, 1, NULL); if (h) U(DeleteObject)(h); }
        return (ULONG_PTR)h;
    }
    case V_WRONGTYPE: {
        if (kind == K_OBJ) return 0;      /* every GDI object is a valid argument for the object-generic calls */
        HGDIOBJ h = kind == K_DC ? mkrgn() : (HGDIOBJ)U(CreateCompatibleDC)(NULL);
        *keep = h;
        return (ULONG_PTR)h;
    }
    }
    return 0;
}
static void drop_handle(int kind, HGDIOBJ keep)
{
    if (!keep) return;
    if (kind == K_DC) g9_ntdel(keep);   /* wrong-type for a DC call is a region */
    else U(DeleteDC)((HDC)keep);
}

static void bogus_body(const char *name, int kind, int soft, int variant)
{
    FARPROC p = t_proc_gdi32full(name);
    if (!p) { t_skip("missing export %s", name); return; }
    if (g9_wine()) { t_skip("wine: direct syscall %s", name); return; }
    if (g9_class(p, NULL) != G9_SYSCALL) { t_skip("%s is not a plain syscall thunk", name); return; }
    if (variant == V_WRONGTYPE && kind == K_OBJ) { t_skip("n/a"); return; }
    HGDIOBJ keep;
    DWORD before = g9_gdi_count();
    ULONG_PTR h = make_handle(kind, variant, &keep);
    DWORD r = (DWORD)g9_call8(p, h, 0, 0, 0, 0, 0, 0, 0);
    drop_handle(kind, keep);
    if (soft) t_note("%s(0x%llx) = 0x%lx", name, (unsigned long long)h, (unsigned long)r);
    else CHECKF(r == 0, "%s(0x%llx) = 0x%lx", name, (unsigned long long)h, (unsigned long)r);
    EXPECT_EQ(g9_gdi_count(), before);
}

#define NB(name, kind, soft) \
    TEST(gap9_ntbogus_##name##_bogus)    { bogus_body(#name, kind, soft, V_BOGUS); } \
    TEST(gap9_ntbogus_##name##_null)     { bogus_body(#name, kind, soft, V_NULL); } \
    TEST(gap9_ntbogus_##name##_deleted)  { bogus_body(#name, kind, soft, V_DELETED); } \
    TEST(gap9_ntbogus_##name##_wrong)    { bogus_body(#name, kind, soft, V_WRONGTYPE); } \
    TEST(gap9_ntbogus_##name##_neg1)     { bogus_body(#name, kind, soft, V_NEGONE); }

NB(NtGdiAbortPath, K_DC, 0)
NB(NtGdiBeginPath, K_DC, 0)
NB(NtGdiCloseFigure, K_DC, 0)
NB(NtGdiEndPath, K_DC, 0)
NB(NtGdiFlattenPath, K_DC, 0)
NB(NtGdiWidenPath, K_DC, 0)
NB(NtGdiEllipse, K_DC, 0)
NB(NtGdiRectangle, K_DC, 0)
NB(NtGdiRoundRect, K_DC, 0)
NB(NtGdiLineTo, K_DC, 0)
NB(NtGdiMoveTo, K_DC, 0)
NB(NtGdiPatBlt, K_DC, 0)
NB(NtGdiFillPath, K_DC, 0)
NB(NtGdiStrokePath, K_DC, 0)
NB(NtGdiStrokeAndFillPath, K_DC, 0)
NB(NtGdiSaveDC, K_DC, 0)
NB(NtGdiRestoreDC, K_DC, 0)
NB(NtGdiExcludeClipRect, K_DC, 0)
NB(NtGdiIntersectClipRect, K_DC, 0)
NB(NtGdiOffsetClipRgn, K_DC, 0)
NB(NtGdiExtSelectClipRgn, K_DC, 0)
NB(NtGdiSelectBitmap, K_DC, 0)
NB(NtGdiSelectBrush, K_DC, 0)
NB(NtGdiSelectPen, K_DC, 0)
NB(NtGdiSelectFont, K_DC, 0)
NB(NtGdiGetDCDword, K_DC, 0)
NB(NtGdiGetDCPoint, K_DC, 0)
NB(NtGdiGetAndSetDCDword, K_DC, 0)
NB(NtGdiSetBrushOrg, K_DC, 0)
NB(NtGdiPathToRegion, K_DC, 0)
NB(NtGdiSetMetaRgn, K_DC, 0)
NB(NtGdiGetAppClipBox, K_DC, 0)
NB(NtGdiCancelDC, K_DC, 1)
NB(NtGdiSelectClipPath, K_DC, 1)
NB(NtGdiSetMiterLimit, K_DC, 1)
NB(NtGdiGetDeviceCaps, K_DC, 1)

NB(NtGdiGetRgnBox, K_RGN, 0)
NB(NtGdiOffsetRgn, K_RGN, 0)
NB(NtGdiPtInRegion, K_RGN, 0)
NB(NtGdiRectInRegion, K_RGN, 0)
NB(NtGdiSetRectRgn, K_RGN, 0)
NB(NtGdiGetRegionData, K_RGN, 1)

NB(NtGdiDeleteObjectApp, K_OBJ, 0)
NB(NtGdiExtGetObjectW, K_OBJ, 0)
NB(NtGdiGetBitmapBits, K_OBJ, 0)
NB(NtGdiGetBitmapDimension, K_OBJ, 0)
NB(NtGdiSetBitmapDimension, K_OBJ, 0)
NB(NtGdiUnrealizeObject, K_OBJ, 0)

/* the same handle-first calls with a pile of nonzero garbage in the other arguments */
static void garbage_body(const char *name, int kind)
{
    FARPROC p = t_proc_gdi32full(name);
    if (!p) { t_skip("missing export %s", name); return; }
    if (g9_wine()) { t_skip("wine: direct syscall %s", name); return; }
    if (g9_class(p, NULL) != G9_SYSCALL) { t_skip("not a plain thunk"); return; }
    DWORD before = g9_gdi_count();
    DWORD r = (DWORD)g9_call8(p, G9_BOGUS, 1, 2, 3, 4, 5, 6, 7);
    CHECKF(r == 0, "%s(bogus, 1..7) = 0x%lx", name, (unsigned long)r);
    EXPECT_EQ(g9_gdi_count(), before);
}
#define NG(name, kind) TEST(gap9_ntgarbage_##name) { garbage_body(#name, kind); }
NG(NtGdiAbortPath, K_DC) NG(NtGdiBeginPath, K_DC) NG(NtGdiEllipse, K_DC) NG(NtGdiRectangle, K_DC) NG(NtGdiRoundRect, K_DC)
NG(NtGdiLineTo, K_DC) NG(NtGdiPatBlt, K_DC) NG(NtGdiSaveDC, K_DC) NG(NtGdiRestoreDC, K_DC) NG(NtGdiSelectBitmap, K_DC)
NG(NtGdiSelectBrush, K_DC) NG(NtGdiSelectPen, K_DC) NG(NtGdiSelectFont, K_DC) NG(NtGdiGetRgnBox, K_RGN) NG(NtGdiOffsetRgn, K_RGN)
NG(NtGdiDeleteObjectApp, K_OBJ) NG(NtGdiExtGetObjectW, K_OBJ)

/* Two-handle region calls with both handles invalid. */
TEST(gap9_nt_combinergn_bogus_handles)
{
    FARPROC p = t_proc_gdi32full("NtGdiCombineRgn");
    REQUIRE(p);
    G9_NOWINE("direct syscall");
    for (int mode = RGN_AND; mode <= RGN_COPY; mode++) {
        EXPECT_EQ((DWORD)g9_call(p, G9_BOGUS, G9_BOGUS, G9_BOGUS, mode), 0);
        EXPECT_EQ((DWORD)g9_call(p, 0, 0, 0, mode), 0);
    }
}

TEST(gap9_nt_combinergn_valid_dest_bogus_source)
{
    FARPROC p = t_proc_gdi32full("NtGdiCombineRgn");
    REQUIRE(p);
    G9_NOWINE("direct syscall");
    HRGN d = U(CreateRectRgn)(0, 0, 10, 10);
    REQUIRE(d);
    EXPECT_EQ((DWORD)g9_call(p, (ULONG_PTR)d, G9_BOGUS, G9_BOGUS, RGN_OR), 0);
    EXPECT_EQ((DWORD)g9_call(p, (ULONG_PTR)d, (ULONG_PTR)d, G9_BOGUS, RGN_OR), 0);
    RECT rc; EXPECT_EQ(U(GetRgnBox)(d, &rc), SIMPLEREGION);
    CHECK(rc.right == 10 && rc.bottom == 10);
    g9_ntdel(d);
}

TEST(gap9_nt_equalrgn_bogus)
{
    FARPROC p = t_proc_gdi32full("NtGdiEqualRgn");
    REQUIRE(p);
    G9_NOWINE("direct syscall");
    HRGN r = U(CreateRectRgn)(0, 0, 3, 3);
    REQUIRE(r);
    EXPECT_EQ((DWORD)g9_call(p, G9_BOGUS, G9_BOGUS, 0, 0), 0);
    EXPECT_EQ((DWORD)g9_call(p, (ULONG_PTR)r, G9_BOGUS, 0, 0), 0);
    EXPECT_EQ((DWORD)g9_call(p, G9_BOGUS, (ULONG_PTR)r, 0, 0), 0);
    g9_ntdel(r);
}

TEST(gap9_nt_arcinternal_bogus_dc)
{
    FARPROC p = t_proc_gdi32full("NtGdiArcInternal");
    REQUIRE(p);
    G9_NOWINE("direct syscall");
    for (int type = 0; type < 4; type++)
        EXPECT_EQ((DWORD)g9_call8(p, type, G9_BOGUS, 0, 0, 10, 10, 0, 0), 0);
}

TEST(gap9_nt_getpixel_setpixel_bogus_dc)
{
    FARPROC g = t_proc_gdi32full("NtGdiGetPixel"), s = t_proc_gdi32full("NtGdiSetPixel");
    REQUIRE(g); REQUIRE(s);
    G9_NOWINE("direct syscall");
    DWORD r = (DWORD)g9_call(g, G9_BOGUS, 0, 0, 0);
    CHECKF(r == CLR_INVALID, "GetPixel bogus = 0x%lx", (unsigned long)r);
    r = (DWORD)g9_call(s, G9_BOGUS, 0, 0, RGB(1, 2, 3));
    CHECKF(r == (DWORD)-1, "SetPixel bogus = 0x%lx", (unsigned long)r);
}

TEST(gap9_nt_setlayout_getlayout_bogus_dc)
{
    FARPROC s = t_proc_gdi32full("NtGdiSetLayout");
    REQUIRE(s);
    G9_NOWINE("direct syscall");
    DWORD r = (DWORD)g9_call(s, G9_BOGUS, 0, LAYOUT_RTL, 0);
    t_note("NtGdiSetLayout(bogus) = 0x%lx", (unsigned long)r);
}

TEST(gap9_nt_createrectrgn_extremes)
{
    FARPROC c = t_proc_gdi32full("NtGdiCreateRectRgn");
    REQUIRE(c);
    G9_NOWINE("direct syscall");
    DWORD before = g9_gdi_count();
    static const int tab[][4] = { {0,0,0,0}, {5,5,5,5}, {10,10,0,0}, {-5,-5,5,5}, {-1,-1,1,1} };
    for (size_t i = 0; i < sizeof tab / sizeof *tab; i++) {
        HRGN r = (HRGN)g9_call8(c, tab[i][0], tab[i][1], tab[i][2], tab[i][3], 0, 0, 0, 0);
        CHECKF(r != NULL, "case %zu", i);
        if (r) {
            EXPECT_EQ(U(GetObjectType)(r), OBJ_REGION);
            RECT rc; int t = U(GetRgnBox)(r, &rc);
            CHECKF(t == SIMPLEREGION || t == NULLREGION, "case %zu type %d", i, t);
            g9_ntdel(r);
        }
    }
    EXPECT_EQ(g9_gdi_count(), before);
}
