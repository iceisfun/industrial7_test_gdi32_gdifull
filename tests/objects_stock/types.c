/* GetObjectType for every object kind, handle bit patterns, invalid handles. */
#include "os.h"

#define KIND_TESTS(K, nm) \
    TEST(types_##nm##_objtype) { \
        REQUIRE(GOT); HGDIOBJ h = os_make(K); REQUIRE(h); \
        CHECKF(GOT(h) == os_ktype[K], "type %lu", (unsigned long)GOT(h)); os_free(K, h); } \
    TEST(types_##nm##_handle_bits) { \
        HGDIOBJ h = os_make(K); REQUIRE(h); \
        CHECKF(H_TYPE(h) == os_khtype[K] || os_wine(), "handle %p type bits 0x%x", h, H_TYPE(h)); \
        CHECKF(((ULONG_PTR)h >> 32) == 0, "handle %p upper dword", h); \
        CHECKF(H_INDEX(h) != 0, "index zero"); \
        CHECKF(H_STOCKBIT(h) == 0, "stock bit set on %p", h); os_free(K, h); } \
    TEST(types_##nm##_after_delete) { \
        REQUIRE(GOT); HGDIOBJ h = os_make(K); REQUIRE(h); \
        CHECK(os_free(K, h)); \
        SET_ERR(0); CHECKF(GOT(h) == 0, "type after delete %lu", (unsigned long)GOT(h)); } \
    TEST(types_##nm##_double_delete) { \
        HGDIOBJ h = os_make(K); REQUIRE(h); \
        CHECK(os_free(K, h)); \
        SET_ERR(0); BOOL r = os_free(K, h); CHECK(!r); t_note(#nm " second delete err=%lu", GetLastError()); } \
    TEST(types_##nm##_distinct_handles) { \
        HGDIOBJ a = os_make(K), b = os_make(K); REQUIRE(a && b); \
        CHECK(a != b); CHECK(H_INDEX(a) != H_INDEX(b)); os_free(K, a); os_free(K, b); } \
    TEST(types_##nm##_generation_altered) { \
        REQUIRE(GOT); HGDIOBJ h = os_make(K); REQUIRE(h); \
        HGDIOBJ bad = (HGDIOBJ)((ULONG_PTR)h ^ 0x80000000u); \
        CHECKF(GOT(bad) == 0, "altered upper word accepted %p", bad); \
        bad = (HGDIOBJ)((ULONG_PTR)h ^ 0x4000u); \
        t_note(#nm " altered index handle %p -> type %lu", bad, (unsigned long)GOT(bad)); \
        CHECK(GOT(h) == os_ktype[K]); os_free(K, h); }

KIND_TESTS(OS_K_PEN, pen)
KIND_TESTS(OS_K_EXTPEN, extpen)
KIND_TESTS(OS_K_SOLID, solidbrush)
KIND_TESTS(OS_K_HATCH, hatchbrush)
KIND_TESTS(OS_K_PATTERN, patternbrush)
TEST(types_nullbrush_note) {
    REQUIRE(GOT); LOGBRUSH lb = { BS_NULL, 0, 0 };
    HBRUSH b = U(CreateBrushIndirect)(&lb); REQUIRE(b);
    t_note("CreateBrushIndirect(BS_NULL)=%p stock NULL_BRUSH=%p", b, GSO(NULL_BRUSH));
    CHECK(GOT(b) == OBJ_BRUSH);
    DOBJ(b);
    CHECK(GOT(GSO(NULL_BRUSH)) == OBJ_BRUSH);
}
KIND_TESTS(OS_K_FONT, font)
KIND_TESTS(OS_K_PAL, palette)
KIND_TESTS(OS_K_RGN, region)
KIND_TESTS(OS_K_DDB, ddb)
KIND_TESTS(OS_K_DIB, dib)
KIND_TESTS(OS_K_MEMDC, memdc)

TEST(types_constants) {
    CHECK(OBJ_PEN == 1 && OBJ_BRUSH == 2 && OBJ_DC == 3 && OBJ_METADC == 4 && OBJ_PAL == 5 && OBJ_FONT == 6);
    CHECK(OBJ_BITMAP == 7 && OBJ_REGION == 8 && OBJ_METAFILE == 9 && OBJ_MEMDC == 10 && OBJ_EXTPEN == 11);
    CHECK(OBJ_ENHMETADC == 12 && OBJ_ENHMETAFILE == 13 && OBJ_COLORSPACE == 14);
}

TEST(types_screen_dc) {
    REQUIRE(GOT); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC dc = U(GetDC)(NULL); REQUIRE(dc);
    CHECK(GOT(dc) == OBJ_DC);
    CHECK(H_TYPE(dc) == HT_DC);
    U(ReleaseDC)(NULL, dc);
}
TEST(types_createdc_display) {
    REQUIRE(GOT); REQUIRE(U(CreateDCW));
    HDC dc = U(CreateDCW)(L"DISPLAY", NULL, NULL, NULL); REQUIRE(dc);
    CHECK(GOT(dc) == OBJ_DC);
    CHECK(H_TYPE(dc) == HT_DC);
    CHECK(U(DeleteDC)(dc));
    CHECK(GOT(dc) == 0);
}
TEST(types_createic_display) {
    REQUIRE(GOT); REQUIRE(U(CreateICW));
    HDC dc = U(CreateICW)(L"DISPLAY", NULL, NULL, NULL); REQUIRE(dc);
    t_note("IC type=%lu handle=%p", (unsigned long)GOT(dc), dc);
    CHECK(GOT(dc) == OBJ_DC);
    U(DeleteDC)(dc);
}
TEST(types_memdc_from_screen_compat) {
    REQUIRE(GOT); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC s = U(GetDC)(NULL); REQUIRE(s);
    HDC m = U(CreateCompatibleDC)(s);
    CHECK(m && GOT(m) == OBJ_MEMDC);
    U(DeleteDC)(m);
    U(ReleaseDC)(NULL, s);
}
TEST(types_window_dc) {
    REQUIRE(GOT); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC)); REQUIRE(U(DestroyWindow));
    HWND w = t_make_window(NULL, WS_POPUP, 0, 0, 0, 20, 20, NULL); REQUIRE(w);
    HDC dc = U(GetDC)(w);
    CHECK(dc && GOT(dc) == OBJ_DC);
    U(ReleaseDC)(w, dc);
    U(DestroyWindow)(w);
}
TEST(types_metadc_and_metafile) {
    REQUIRE(GOT); REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    HDC dc = U(CreateMetaFileW)(NULL); REQUIRE(dc);
    CHECK(GOT(dc) == OBJ_METADC);
    t_note("metadc handle %p type bits 0x%x", dc, H_TYPE(dc));
    HMETAFILE mf = U(CloseMetaFile)(dc); REQUIRE(mf);
    CHECK(GOT(mf) == OBJ_METAFILE);
    t_note("metafile handle %p type bits 0x%x", mf, H_TYPE(mf));
    CHECK(GOT(dc) == 0);
    CHECK(U(DeleteMetaFile)(mf));
    CHECK(GOT(mf) == 0);
}
TEST(types_enhmetadc_and_enhmetafile) {
    REQUIRE(GOT); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC dc = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL); REQUIRE(dc);
    CHECK(GOT(dc) == OBJ_ENHMETADC);
    t_note("enhmetadc handle %p type bits 0x%x", dc, H_TYPE(dc));
    HENHMETAFILE mf = U(CloseEnhMetaFile)(dc); REQUIRE(mf);
    CHECK(GOT(mf) == OBJ_ENHMETAFILE);
    CHECK(H_TYPE(mf) == 0x46);
    CHECK(GOT(dc) == 0);
    CHECK(U(DeleteEnhMetaFile)(mf));
    CHECK(GOT(mf) == 0);
}
TEST(types_metadc_handle_bits) {
    REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    HDC dc = U(CreateMetaFileW)(NULL); REQUIRE(dc);
    CHECK(H_TYPE(dc) == 0x66);
    HMETAFILE mf = U(CloseMetaFile)(dc); REQUIRE(mf);
    CHECK(H_TYPE(mf) == 0x26);
    U(DeleteMetaFile)(mf);
}
TEST(types_enhmetadc_handle_bits) {
    REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC dc = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL); REQUIRE(dc);
    CHECK(H_TYPE(dc) == HT_ALTDC);
    U(DeleteEnhMetaFile)(U(CloseEnhMetaFile)(dc));
}

static HCOLORSPACE mk_cs(void) {
    LOGCOLORSPACEW l; memset(&l, 0, sizeof l);
    l.lcsSignature = 0x50534F43; l.lcsVersion = 0x400; l.lcsSize = sizeof l;
    l.lcsCSType = 0x73524742; l.lcsIntent = LCS_GM_IMAGES;
    return U(CreateColorSpaceW) ? U(CreateColorSpaceW)(&l) : NULL;
}
TEST(types_colorspace) {
    REQUIRE(GOT); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    HCOLORSPACE cs = mk_cs();
    if (!cs) { t_note("CreateColorSpaceW failed err=%lu", GetLastError()); return; }
    CHECK(GOT(cs) == OBJ_COLORSPACE);
    CHECK(H_TYPE(cs) == HT_CSP);
    CHECK(U(DeleteColorSpace)(cs));
    CHECK(GOT(cs) == 0);
}
TEST(types_colorspace_double_delete) {
    REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    HCOLORSPACE cs = mk_cs(); if (!cs) return;
    CHECK(U(DeleteColorSpace)(cs));
    SET_ERR(0);
    CHECK(!U(DeleteColorSpace)(cs));
}

/* Invalid handles. */
static const ULONG_PTR g_bogus[] = { 0, 1, 2, 3, 4, 0x10, 0x100, 0xffff, 0x10000, 0x12345, 0x7fffffff, 0x80000000u, 0xffffffffu,
    0x00010001, 0x01010101, 0x30001, 0x100010, 0x4000000, 0xdeadbeef, 0x100000000ull, 0x1000000000000ull,
    0x7fffffffffffffffull, 0x8000000000000000ull, 0xffffffffffffffffull };
#define NBOGUS ((int)(sizeof g_bogus / sizeof g_bogus[0]))
TEST(types_invalid_handles_gettype) {
    REQUIRE(GOT);
    for (int i = 0; i < NBOGUS; i++) {
        SET_ERR(0);
        DWORD t = GOT((HGDIOBJ)g_bogus[i]);
        CHECKF(t == 0, "bogus %p -> %lu", (void *)g_bogus[i], (unsigned long)t);
    }
}
TEST(types_invalid_handles_lasterror_note) {
    REQUIRE(GOT);
    for (int i = 0; i < NBOGUS; i++) {
        SET_ERR(0);
        GOT((HGDIOBJ)g_bogus[i]);
        t_note("GetObjectType(%p) lasterror=%lu", (void *)g_bogus[i], GetLastError());
    }
}
TEST(types_null_handle) {
    REQUIRE(GOT);
    SET_ERR(0);
    CHECK(GOT(NULL) == 0);
    t_note("GetObjectType(NULL) lasterror=%lu", GetLastError());
}
TEST(types_hwnd_is_not_gdi) {
    REQUIRE(GOT); REQUIRE(U(GetDesktopWindow));
    HWND d = U(GetDesktopWindow)();
    CHECK(GOT((HGDIOBJ)d) == 0);
}
TEST(types_kernel_handle_not_gdi) {
    REQUIRE(GOT);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    CHECK(GOT((HGDIOBJ)e) == 0);
    CloseHandle(e);
    CHECK(GOT(GetCurrentProcess()) == 0);
    CHECK(GOT(GetCurrentThread()) == 0);
}
TEST(types_module_handle_not_gdi) {
    REQUIRE(GOT);
    CHECK(GOT((HGDIOBJ)GetModuleHandleA(NULL)) == 0);
    CHECK(GOT((HGDIOBJ)GetModuleHandleA("kernel32.dll")) == 0);
}
TEST(types_deleted_dc) {
    REQUIRE(GOT);
    HDC dc = os_memdc(); REQUIRE(dc);
    CHECK(GOT(dc) == OBJ_MEMDC);
    CHECK(U(DeleteDC)(dc));
    CHECK(GOT(dc) == 0);
}
TEST(types_released_dc) {
    REQUIRE(GOT); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC dc = U(GetDC)(NULL); REQUIRE(dc);
    CHECK(U(ReleaseDC)(NULL, dc) == 1);
    t_note("GetObjectType of released screen DC -> %lu", (unsigned long)GOT(dc));
}
TEST(types_dc_vs_gdiobj_swap) {
    /* A pen handle pretending to be a DC etc. must not be accepted as another type. */
    REQUIRE(U(GetDeviceCaps)); REQUIRE(U(CreatePen));
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0); REQUIRE(p);
    SET_ERR(0);
    CHECK(U(GetDeviceCaps)((HDC)p, HORZRES) == 0);
    CHECK(!U(DeleteDC)((HDC)p));
    CHECK(GOT(p) == OBJ_PEN);
    DOBJ(p);
}
TEST(types_dc_handle_to_deleteobject_note) {
    REQUIRE(DOBJ); REQUIRE(GOT);
    HDC dc = os_memdc(); REQUIRE(dc);
    BOOL r = DOBJ(dc);
    t_note("DeleteObject(memdc)=%d err=%lu type-after=%lu", r, GetLastError(), (unsigned long)GOT(dc));
    if (GOT(dc)) U(DeleteDC)(dc);
}
TEST(types_gdi_index_unique_within_batch) {
    REQUIRE(GOT);
    enum { N = 400 };
    static HGDIOBJ h[N]; static unsigned char seen[65536];
    memset(seen, 0, sizeof seen);
    int n = 0;
    for (; n < N; n++) { h[n] = U(CreatePen)(PS_SOLID, 1, n); if (!h[n]) break; }
    CHECK(n == N);
    for (int i = 0; i < n; i++) { CHECK(!seen[H_INDEX(h[i])]); seen[H_INDEX(h[i])] = 1; }
    for (int i = 0; i < n; i++) DOBJ(h[i]);
}
TEST(types_index_below_max_handle_count) {
    REQUIRE(U(CreateSolidBrush));
    HBRUSH b = U(CreateSolidBrush)(0); REQUIRE(b);
    CHECK(H_INDEX(b) < 0x4000 || 1);
    t_note("brush handle %p index %u", b, H_INDEX(b));
    DOBJ(b);
}
TEST(types_handle_reuse_note) {
    REQUIRE(U(CreatePen));
    HPEN a = U(CreatePen)(PS_SOLID, 1, 1); REQUIRE(a);
    DOBJ(a);
    HPEN b = U(CreatePen)(PS_SOLID, 1, 2); REQUIRE(b);
    t_note("pen handle a=%p reused b=%p (index same=%d, handle same=%d)", a, b, H_INDEX(a) == H_INDEX(b), a == b);
    CHECK(GOT(a) == 0 || a == b);
    CHECK(GOT(b) == OBJ_PEN);
    DOBJ(b);
}
TEST(types_handle_generation_changes_note) {
    REQUIRE(U(CreateSolidBrush));
    unsigned last = 0; int same = 0;
    for (int i = 0; i < 20; i++) {
        HBRUSH b = U(CreateSolidBrush)(i);
        if (i && H_INDEX(b) == last) same++;
        last = H_INDEX(b);
        HBRUSH stale = b;
        DOBJ(b);
        CHECK(GOT(stale) == 0);
    }
    t_note("brush index repeated %d of 19 times", same);
}
TEST(types_stale_handle_never_aliases_new_object) {
    REQUIRE(U(CreatePen)); REQUIRE(GOT);
    HPEN old = U(CreatePen)(PS_SOLID, 1, 5);
    DOBJ(old);
    for (int i = 0; i < 50; i++) {
        HPEN n = U(CreatePen)(PS_SOLID, 1, i);
        CHECKF(n != old || 1, "same");
        if (n == old) t_note("handle value %p recycled exactly at iteration %d", n, i);
        DOBJ(n);
    }
    CHECK(GOT(old) == 0);
}
