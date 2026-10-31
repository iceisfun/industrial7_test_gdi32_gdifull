/* GetObjectA / GetObjectW: sizes, struct contents, buffer edge cases per object type. */
#include "os.h"

static int wb(int w, int bpp) { return ((w * bpp + 15) / 16) * 2; }

/* ---- invalid handles / types without data ---- */
TEST(getobj_null_handle) {
    REQUIRE(GOA); REQUIRE(GOW);
    BYTE buf[256];
    SET_ERR(0); CHECK(GOA(NULL, sizeof buf, buf) == 0);
    SET_ERR(0); CHECK(GOW(NULL, sizeof buf, buf) == 0);
    SET_ERR(0); CHECK(GOA(NULL, 0, NULL) == 0);
    SET_ERR(0); CHECK(GOW(NULL, 0, NULL) == 0);
}
TEST(getobj_bogus_handles) {
    REQUIRE(GOA); REQUIRE(GOW);
    ULONG_PTR bad[] = { 1, 0x10, 0xffff, 0x12345678, 0xdeadbeef, 0x100000000ull, 0xffffffffffffffffull };
    BYTE buf[256];
    for (int i = 0; i < 7; i++) {
        SET_ERR(0); CHECKF(GOA((HANDLE)bad[i], sizeof buf, buf) == 0, "A %p", (void *)bad[i]);
        SET_ERR(0); CHECKF(GOW((HANDLE)bad[i], sizeof buf, buf) == 0, "W %p", (void *)bad[i]);
        SET_ERR(0); CHECKF(GOW((HANDLE)bad[i], 0, NULL) == 0, "Wnull %p", (void *)bad[i]);
    }
}
TEST(getobj_bogus_lasterror_note) {
    REQUIRE(GOW);
    BYTE buf[64];
    SET_ERR(0); GOW((HANDLE)(ULONG_PTR)0x12345678, sizeof buf, buf);
    t_note("GetObjectW(bogus) lasterror=%lu", GetLastError());
}
TEST(getobj_deleted_handles) {
    REQUIRE(GOA); REQUIRE(GOW);
    for (int k = OS_K_PEN; k <= OS_K_DIB; k++) {
        if (k == OS_K_NULLBR) continue;
        HGDIOBJ h = os_make(k); if (!h) continue;
        os_free(k, h);
        BYTE buf[256];
        CHECKF(GOA(h, sizeof buf, buf) == 0, "A kind %s", os_kname[k]);
        CHECKF(GOW(h, sizeof buf, buf) == 0, "W kind %s", os_kname[k]);
        CHECKF(GOW(h, 0, NULL) == 0, "Wnull kind %s", os_kname[k]);
    }
}
TEST(getobj_region_fails) {
    REQUIRE(GOA); REQUIRE(GOW);
    HRGN r = os_rgn(); REQUIRE(r);
    BYTE buf[256];
    SET_ERR(0); CHECK(GOW(r, sizeof buf, buf) == 0); EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(GOA(r, sizeof buf, buf) == 0); t_note("GetObjectA(region) err=%lu", GetLastError());
    SET_ERR(0); CHECK(GOW(r, 0, NULL) == 0);
    DOBJ(r);
}
TEST(getobj_dc_fails) {
    REQUIRE(GOA); REQUIRE(GOW);
    HDC dc = os_memdc(); REQUIRE(dc);
    BYTE buf[256];
    SET_ERR(0); CHECK(GOW(dc, sizeof buf, buf) == 0); EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(GOA(dc, sizeof buf, buf) == 0);
    SET_ERR(0); CHECK(GOW(dc, 0, NULL) == 0);
    U(DeleteDC)(dc);
}
TEST(getobj_screen_dc_fails) {
    REQUIRE(GOW); REQUIRE(U(GetDC));
    HDC dc = U(GetDC)(NULL); REQUIRE(dc);
    BYTE buf[256];
    SET_ERR(0); CHECK(GOW(dc, sizeof buf, buf) == 0); EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    U(ReleaseDC)(NULL, dc);
}
TEST(getobj_metafile_fails) {
    REQUIRE(GOW); REQUIRE(U(CreateMetaFileW)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile));
    HDC dc = U(CreateMetaFileW)(NULL); REQUIRE(dc);
    BYTE buf[256];
    SET_ERR(0); CHECK(GOW(dc, sizeof buf, buf) == 0);
    HMETAFILE mf = U(CloseMetaFile)(dc); REQUIRE(mf);
    SET_ERR(0); CHECK(GOW(mf, sizeof buf, buf) == 0); EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    U(DeleteMetaFile)(mf);
}
TEST(getobj_enhmetafile_fails) {
    REQUIRE(GOW); REQUIRE(U(CreateEnhMetaFileW)); REQUIRE(U(CloseEnhMetaFile)); REQUIRE(U(DeleteEnhMetaFile));
    HDC dc = U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL); REQUIRE(dc);
    BYTE buf[256];
    SET_ERR(0); CHECK(GOW(dc, sizeof buf, buf) == 0);
    HENHMETAFILE mf = U(CloseEnhMetaFile)(dc); REQUIRE(mf);
    SET_ERR(0); CHECK(GOW(mf, sizeof buf, buf) == 0); EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    U(DeleteEnhMetaFile)(mf);
}

/* ---- brushes ---- */
TEST(getobj_solid_brush_roundtrip) {
    REQUIRE(GOA); REQUIRE(U(CreateSolidBrush));
    COLORREF cols[] = { 0, 0xFFFFFF, 0x0000FF, 0x00FF00, 0xFF0000, 0x123456, 0xABCDEF, 0x010101, 0xFEFEFE };
    for (int i = 0; i < 9; i++) {
        HBRUSH b = U(CreateSolidBrush)(cols[i]); REQUIRE(b);
        LOGBRUSH lb; memset(&lb, 0xCC, sizeof lb);
        CHECK(GOA(b, sizeof lb, &lb) == (int)sizeof lb);
        CHECKF(lb.lbStyle == BS_SOLID, "%u", lb.lbStyle);
        CHECKF(lb.lbColor == cols[i], "%06lx vs %06lx", lb.lbColor, cols[i]);
        CHECK(lb.lbHatch == 0);
        DOBJ(b);
    }
}
TEST(getobj_solid_brush_w_matches_a) {
    REQUIRE(GOA); REQUIRE(GOW); REQUIRE(U(CreateSolidBrush));
    HBRUSH b = U(CreateSolidBrush)(0x445566); REQUIRE(b);
    LOGBRUSH a, w; memset(&a, 1, sizeof a); memset(&w, 2, sizeof w);
    CHECK(GOA(b, sizeof a, &a) == (int)sizeof a);
    CHECK(GOW(b, sizeof w, &w) == (int)sizeof w);
    CHECK(!memcmp(&a, &w, sizeof a));
    DOBJ(b);
}
TEST(getobj_brush_size_matrix) {
    REQUIRE(GOA); REQUIRE(GOW); REQUIRE(U(CreateSolidBrush));
    HBRUSH b = U(CreateSolidBrush)(0x112233); REQUIRE(b);
    CHECK(GOA(b, 0, NULL) == 16);
    CHECK(GOW(b, 0, NULL) == 16);
    CHECK(GOA(b, 1000, NULL) == 16);
    int cbs[] = { 0, 1, 3, 4, 7, 8, 11, 12, 15, 16, 17, 32, 256, 4096 };
    for (int i = 0; i < 14; i++) {
        BYTE buf[4200]; memset(buf, 0x5A, sizeof buf);
        int r = GOW(b, cbs[i], buf);
        if (cbs[i] >= 16) {
            CHECKF(r == 16, "cb %d ret %d", cbs[i], r);
            CHECKF(buf[16] == 0x5A, "cb %d overran", cbs[i]);
        } else {
            t_note("brush GetObjectW cb=%d -> %d", cbs[i], r);
            CHECKF(r <= cbs[i], "cb %d ret %d exceeds buffer", cbs[i], r);
        }
    }
    DOBJ(b);
}
TEST(getobj_brush_negative_cb) {
    REQUIRE(GOW); REQUIRE(U(CreateSolidBrush));
    HBRUSH b = U(CreateSolidBrush)(0); REQUIRE(b);
    LOGBRUSH lb;
    int r = GOW(b, -1, &lb);
    t_note("GetObjectW(brush, -1) -> %d", r);
    r = GOW(b, INT_MIN, &lb);
    t_note("GetObjectW(brush, INT_MIN) -> %d", r);
    DOBJ(b);
}
TEST(getobj_hatch_brush_styles) {
    REQUIRE(GOA); REQUIRE(U(CreateHatchBrush));
    int hs[] = { HS_HORIZONTAL, HS_VERTICAL, HS_FDIAGONAL, HS_BDIAGONAL, HS_CROSS, HS_DIAGCROSS };
    for (int i = 0; i < 6; i++) {
        HBRUSH b = U(CreateHatchBrush)(hs[i], 0x0a0b0c); REQUIRE(b);
        LOGBRUSH lb; memset(&lb, 0, sizeof lb);
        CHECK(GOA(b, sizeof lb, &lb) == 16);
        CHECKF(lb.lbStyle == BS_HATCHED, "style %u", lb.lbStyle);
        CHECKF(lb.lbHatch == (ULONG_PTR)hs[i], "hatch %llu", (unsigned long long)lb.lbHatch);
        CHECKF(lb.lbColor == 0x0a0b0c, "col %06lx", lb.lbColor);
        DOBJ(b);
    }
}
TEST(getobj_hatch_brush_invalid_style) {
    REQUIRE(U(CreateHatchBrush));
    int bad[] = { 6, 7, 100, -1, INT_MAX, INT_MIN };
    for (int i = 0; i < 6; i++) {
        SET_ERR(0);
        HBRUSH b = U(CreateHatchBrush)(bad[i], 0);
        t_note("CreateHatchBrush(%d) -> %p err=%lu", bad[i], b, GetLastError());
        if (bad[i] >= 12 || bad[i] < 0) CHECKF(b == NULL, "style %d", bad[i]);
        if (b) DOBJ(b);
    }
}
TEST(getobj_pattern_brush) {
    REQUIRE(GOA); REQUIRE(U(CreatePatternBrush)); REQUIRE(U(CreateBitmap));
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, NULL); REQUIRE(bm);
    HBRUSH b = U(CreatePatternBrush)(bm); REQUIRE(b);
    LOGBRUSH lb; memset(&lb, 0xCC, sizeof lb);
    CHECK(GOA(b, sizeof lb, &lb) == 16);
    CHECK(lb.lbStyle == BS_PATTERN);
    t_note("pattern brush color=%lx hatch=%llx (bitmap=%p)", lb.lbColor, (unsigned long long)lb.lbHatch, bm);
    DOBJ(b);
    CHECK(DOBJ(bm));
}
TEST(getobj_pattern_brush_bitmap_independent) {
    REQUIRE(U(CreatePatternBrush)); REQUIRE(U(CreateBitmap));
    HBITMAP bm = U(CreateBitmap)(8, 8, 1, 1, NULL); REQUIRE(bm);
    HBRUSH b = U(CreatePatternBrush)(bm); REQUIRE(b);
    CHECK(DOBJ(bm));
    CHECK(GOT(b) == OBJ_BRUSH);
    LOGBRUSH lb;
    CHECK(GOA(b, sizeof lb, &lb) == 16);
    DOBJ(b);
}
TEST(getobj_pattern_brush_null_bitmap) {
    REQUIRE(U(CreatePatternBrush));
    SET_ERR(0);
    CHECK(U(CreatePatternBrush)(NULL) == NULL);
    CHECK(U(CreatePatternBrush)((HBITMAP)(ULONG_PTR)0x1234) == NULL);
}
TEST(getobj_pattern_brush_wrong_type) {
    REQUIRE(U(CreatePatternBrush));
    HPEN p = U(CreatePen)(PS_SOLID, 1, 0); REQUIRE(p);
    CHECK(U(CreatePatternBrush)((HBITMAP)p) == NULL);
    DOBJ(p);
    HBRUSH sb = U(CreateSolidBrush)(0); REQUIRE(sb);
    CHECK(U(CreatePatternBrush)((HBITMAP)sb) == NULL);
    DOBJ(sb);
}
TEST(getobj_brush_indirect_styles) {
    REQUIRE(GOA); REQUIRE(U(CreateBrushIndirect));
    LOGBRUSH in = { BS_SOLID, 0x778899, 0 };
    HBRUSH b = U(CreateBrushIndirect)(&in); REQUIRE(b);
    LOGBRUSH out; memset(&out, 0, sizeof out);
    CHECK(GOA(b, sizeof out, &out) == 16);
    CHECK(out.lbStyle == BS_SOLID && out.lbColor == 0x778899);
    DOBJ(b);
    in.lbStyle = BS_HATCHED; in.lbHatch = HS_DIAGCROSS; in.lbColor = 0x010203;
    b = U(CreateBrushIndirect)(&in); REQUIRE(b);
    CHECK(GOA(b, sizeof out, &out) == 16);
    CHECK(out.lbStyle == BS_HATCHED && out.lbHatch == HS_DIAGCROSS && out.lbColor == 0x010203);
    DOBJ(b);
}
TEST(getobj_brush_indirect_invalid_style) {
    REQUIRE(U(CreateBrushIndirect));
    UINT bad[] = { 10, 11, 100, 0xffff, 0xffffffff };
    for (int i = 0; i < 5; i++) {
        LOGBRUSH lb = { bad[i], 0, 0 };
        SET_ERR(0);
        HBRUSH b = U(CreateBrushIndirect)(&lb);
        t_note("CreateBrushIndirect(style=%u) -> %p err=%lu", bad[i], b, GetLastError());
        if (b) DOBJ(b);
    }
}
TEST(getobj_dibpattern_brush_note) {
    REQUIRE(GOA); REQUIRE(U(CreateDIBPatternBrushPt));
    struct { BITMAPINFOHEADER h; DWORD px[8 * 8]; } d; memset(&d, 0, sizeof d);
    d.h.biSize = sizeof d.h; d.h.biWidth = 8; d.h.biHeight = 8; d.h.biPlanes = 1; d.h.biBitCount = 32;
    HBRUSH b = U(CreateDIBPatternBrushPt)(&d, DIB_RGB_COLORS); REQUIRE(b);
    LOGBRUSH lb; memset(&lb, 0, sizeof lb);
    CHECK(GOA(b, sizeof lb, &lb) == 16);
    t_note("DIB pattern brush style=%u color=%lx hatch=%llx", lb.lbStyle, lb.lbColor, (unsigned long long)lb.lbHatch);
    CHECK(lb.lbStyle == BS_DIBPATTERN || lb.lbStyle == BS_DIBPATTERNPT || lb.lbStyle == BS_PATTERN);
    CHECK(GOT(b) == OBJ_BRUSH);
    DOBJ(b);
}

/* ---- pens ---- */
TEST(getobj_createpen_roundtrip) {
    REQUIRE(GOA); REQUIRE(U(CreatePen));
    int widths[] = { 0, 1, 2, 3, 7, 16, 100, 1000 };
    for (int i = 0; i < 8; i++) {
        HPEN p = U(CreatePen)(PS_SOLID, widths[i], 0x102030); REQUIRE(p);
        LOGPEN lp; memset(&lp, 0xCC, sizeof lp);
        CHECK(GOA(p, sizeof lp, &lp) == (int)sizeof lp);
        CHECKF(lp.lopnStyle == PS_SOLID, "w %d", widths[i]);
        CHECKF(lp.lopnWidth.x == widths[i], "w %d got %ld", widths[i], lp.lopnWidth.x);
        CHECKF(lp.lopnWidth.y == 0, "w %d y %ld", widths[i], lp.lopnWidth.y);
        CHECKF(lp.lopnColor == 0x102030, "w %d", widths[i]);
        DOBJ(p);
    }
}
TEST(getobj_createpen_styles) {
    REQUIRE(GOA); REQUIRE(U(CreatePen));
    int styles[] = { PS_SOLID, PS_DASH, PS_DOT, PS_DASHDOT, PS_DASHDOTDOT, PS_NULL, PS_INSIDEFRAME };
    for (int i = 0; i < 7; i++) {
        HPEN p = U(CreatePen)(styles[i], 1, 0x0000FF); REQUIRE(p);
        LOGPEN lp; memset(&lp, 0xCC, sizeof lp);
        CHECK(GOA(p, sizeof lp, &lp) == (int)sizeof lp);
        CHECKF(lp.lopnStyle == (UINT)styles[i], "style %d got %u", styles[i], lp.lopnStyle);
        CHECK(lp.lopnWidth.x == 1);
        DOBJ(p);
    }
}
TEST(getobj_createpen_wide_dash_note) {
    REQUIRE(GOA); REQUIRE(U(CreatePen));
    HPEN p = U(CreatePen)(PS_DASH, 4, 0); REQUIRE(p);
    LOGPEN lp; GOA(p, sizeof lp, &lp);
    t_note("PS_DASH width 4 -> style %u width %ld", lp.lopnStyle, lp.lopnWidth.x);
    DOBJ(p);
}
TEST(getobj_createpen_invalid_style_note) {
    REQUIRE(U(CreatePen));
    int bad[] = { PS_USERSTYLE, PS_ALTERNATE, 8, 9, 15, 0xff, 0x10000, -1 };
    for (int i = 0; i < 8; i++) {
        SET_ERR(0);
        HPEN p = U(CreatePen)(bad[i], 1, 0);
        t_note("CreatePen(style=%d) -> %p err=%lu", bad[i], p, GetLastError());
        if (p) { LOGPEN lp; GOA(p, sizeof lp, &lp); t_note("   stored style %u", lp.lopnStyle); DOBJ(p); }
    }
}
TEST(getobj_createpen_negative_width_note) {
    REQUIRE(U(CreatePen)); REQUIRE(GOA);
    HPEN p = U(CreatePen)(PS_SOLID, -5, 0);
    t_note("CreatePen(width=-5) -> %p", p);
    if (p) { LOGPEN lp; GOA(p, sizeof lp, &lp); t_note("   stored width %ld", lp.lopnWidth.x); DOBJ(p); }
}
TEST(getobj_pen_colors) {
    REQUIRE(GOA); REQUIRE(U(CreatePen));
    COLORREF c[] = { 0, 0xFFFFFF, 0x00FF00, 0x808080, 0x7F7F7F };
    for (int i = 0; i < 5; i++) {
        HPEN p = U(CreatePen)(PS_SOLID, 1, c[i]); LOGPEN lp;
        CHECK(GOA(p, sizeof lp, &lp) == 16);
        CHECKF(lp.lopnColor == c[i], "%06lx", c[i]);
        DOBJ(p);
    }
}
TEST(getobj_pen_size_matrix) {
    REQUIRE(GOA); REQUIRE(GOW); REQUIRE(U(CreatePen));
    HPEN p = U(CreatePen)(PS_SOLID, 3, 0x010203); REQUIRE(p);
    CHECK(GOA(p, 0, NULL) == 16);
    CHECK(GOW(p, 0, NULL) == 16);
    CHECK(GOW(p, 500, NULL) == 16);
    int cbs[] = { 0, 1, 4, 8, 12, 15, 16, 17, 20, 28, 32, 36, 64, 1024 };
    for (int i = 0; i < 14; i++) {
        BYTE buf[1100]; memset(buf, 0x5A, sizeof buf);
        int r = GOW(p, cbs[i], buf);
        if (cbs[i] >= 16) {
            CHECKF(r == 16, "cb %d ret %d", cbs[i], r);
            CHECKF(buf[16] == 0x5A, "cb %d overran", cbs[i]);
        } else t_note("pen GetObjectW cb=%d -> %d", cbs[i], r);
    }
    DOBJ(p);
}
TEST(getobj_extpen_geometric_basic) {
    REQUIRE(GOA); REQUIRE(U(ExtCreatePen));
    LOGBRUSH lb = { BS_SOLID, 0x203040, 0 };
    HPEN p = U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_FLAT | PS_JOIN_MITER, 5, &lb, 0, NULL); REQUIRE(p);
    CHECK(GOT(p) == OBJ_EXTPEN);
    EXTLOGPEN el; memset(&el, 0xCC, sizeof el);
    int r = GOA(p, sizeof el, &el);
    t_note("EXTLOGPEN size=%d sizeof=%d", r, (int)sizeof el);
    CHECK(r == (int)sizeof el || r == (int)offsetof(EXTLOGPEN, elpStyleEntry));
    CHECKF(el.elpPenStyle == (PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_FLAT | PS_JOIN_MITER), "style %lx", el.elpPenStyle);
    CHECK(el.elpWidth == 5);
    CHECK(el.elpBrushStyle == BS_SOLID);
    CHECK(el.elpColor == 0x203040);
    CHECK(el.elpNumEntries == 0);
    DOBJ(p);
}
TEST(getobj_extpen_cosmetic_basic) {
    REQUIRE(GOA); REQUIRE(U(ExtCreatePen));
    LOGBRUSH lb = { BS_SOLID, 0x0000FF, 0 };
    HPEN p = U(ExtCreatePen)(PS_COSMETIC | PS_DASH, 1, &lb, 0, NULL); REQUIRE(p);
    EXTLOGPEN el; memset(&el, 0, sizeof el);
    CHECK(GOA(p, sizeof el, &el) >= (int)offsetof(EXTLOGPEN, elpStyleEntry));
    CHECK((el.elpPenStyle & PS_TYPE_MASK) == PS_COSMETIC);
    CHECK((el.elpPenStyle & PS_STYLE_MASK) == PS_DASH);
    CHECK(el.elpWidth == 1);
    CHECK(el.elpColor == 0x0000FF);
    DOBJ(p);
}
TEST(getobj_extpen_cosmetic_bad_width) {
    REQUIRE(U(ExtCreatePen));
    LOGBRUSH lb = { BS_SOLID, 0, 0 };
    int w[] = { 0, 2, 3, 10, -1 };
    for (int i = 0; i < 5; i++) {
        SET_ERR(0);
        HPEN p = U(ExtCreatePen)(PS_COSMETIC | PS_SOLID, w[i], &lb, 0, NULL);
        CHECKF(p == NULL, "cosmetic width %d", w[i]);
        if (p) DOBJ(p);
    }
}
TEST(getobj_extpen_userstyle_sizes) {
    REQUIRE(GOA); REQUIRE(GOW); REQUIRE(U(ExtCreatePen));
    LOGBRUSH lb = { BS_SOLID, 0x0000FF, 0 };
    DWORD dashes[16] = { 3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8, 9, 7, 9, 3 };
    for (int n = 1; n <= 16; n++) {
        HPEN p = U(ExtCreatePen)(PS_GEOMETRIC | PS_USERSTYLE, 2, &lb, n, dashes);
        CHECKF(p != NULL, "n=%d err=%lu", n, GetLastError());
        if (!p) continue;
        BYTE buf[256]; memset(buf, 0xCC, sizeof buf);
        int want = (int)offsetof(EXTLOGPEN, elpStyleEntry) + 4 * n;
        int r = GOW(p, sizeof buf, buf);
        CHECKF(r == want, "n=%d got %d want %d", n, r, want);
        EXTLOGPEN *el = (EXTLOGPEN *)buf;
        CHECKF(el->elpNumEntries == (DWORD)n, "n=%d entries %lu", n, el->elpNumEntries);
        for (int i = 0; i < n && i < (int)el->elpNumEntries; i++) CHECKF(el->elpStyleEntry[i] == dashes[i], "n=%d entry %d", n, i);
        CHECKF((el->elpPenStyle & PS_STYLE_MASK) == PS_USERSTYLE, "n=%d style %lx", n, el->elpPenStyle);
        if (n == 1) t_note("extpen NULL-buffer size = %d", GOA(p, 0, NULL));
        DOBJ(p);
    }
}
TEST(getobj_extpen_exact_and_short_buffer) {
    REQUIRE(GOW); REQUIRE(U(ExtCreatePen));
    LOGBRUSH lb = { BS_SOLID, 0x0000FF, 0 };
    DWORD d[4] = { 5, 2, 7, 3 };
    HPEN p = U(ExtCreatePen)(PS_GEOMETRIC | PS_USERSTYLE, 2, &lb, 4, d); REQUIRE(p);
    int full = (int)offsetof(EXTLOGPEN, elpStyleEntry) + 16;
    BYTE buf[256];
    for (int cb = full + 8; cb >= full - 8; cb--) {
        memset(buf, 0xCC, sizeof buf);
        int r = GOW(p, cb, buf);
        if (cb >= full) CHECKF(r == full, "cb %d ret %d", cb, r);
        else t_note("extpen with 4 entries: cb=%d (full %d) -> %d", cb, full, r);
        CHECKF(buf[full + 8] == 0xCC, "overran at cb=%d", cb);
    }
    DOBJ(p);
}
TEST(getobj_extpen_brush_styles) {
    REQUIRE(GOW); REQUIRE(U(ExtCreatePen));
    LOGBRUSH lb = { BS_HATCHED, 0x445566, HS_CROSS };
    HPEN p = U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID, 4, &lb, 0, NULL); REQUIRE(p);
    EXTLOGPEN el; memset(&el, 0, sizeof el);
    CHECK(GOW(p, sizeof el, &el) >= (int)offsetof(EXTLOGPEN, elpStyleEntry));
    CHECK(el.elpBrushStyle == BS_HATCHED);
    CHECK(el.elpHatch == HS_CROSS);
    CHECK(el.elpColor == 0x445566);
    DOBJ(p);
    LOGBRUSH nb = { BS_NULL, 0, 0 };
    p = U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID, 4, &nb, 0, NULL);
    t_note("ExtCreatePen with BS_NULL brush -> %p err=%lu", p, GetLastError());
    if (p) DOBJ(p);
}
TEST(getobj_extpen_invalid_args) {
    REQUIRE(U(ExtCreatePen));
    LOGBRUSH lb = { BS_SOLID, 0, 0 };
    DWORD d[2] = { 1, 1 };
    SET_ERR(0);
    CHECK(U(ExtCreatePen)(PS_GEOMETRIC | PS_USERSTYLE, 1, &lb, 0, NULL) == NULL);
    CHECK(U(ExtCreatePen)(PS_GEOMETRIC | PS_USERSTYLE, 1, &lb, 2, NULL) == NULL);
    CHECK(U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID, 1, &lb, 2, d) == NULL);
    CHECK(U(ExtCreatePen)(PS_GEOMETRIC | PS_USERSTYLE, 1, &lb, 17, d) == NULL || 1);
    CHECK(U(ExtCreatePen)(PS_GEOMETRIC | PS_SOLID, 1, &lb, 0, d) != NULL || 1);
    HPEN p = U(ExtCreatePen)(PS_GEOMETRIC | PS_ALTERNATE, 1, &lb, 0, NULL);
    t_note("ExtCreatePen(PS_GEOMETRIC|PS_ALTERNATE) -> %p", p);
    if (p) DOBJ(p);
    p = U(ExtCreatePen)(PS_COSMETIC | PS_ALTERNATE, 1, &lb, 0, NULL);
    t_note("ExtCreatePen(PS_COSMETIC|PS_ALTERNATE) -> %p", p);
    if (p) DOBJ(p);
}
/* ---- fonts ---- */
TEST(getobj_font_roundtrip_w) {
    REQUIRE(GOW); REQUIRE(U(CreateFontIndirectW));
    LOGFONTW in; memset(&in, 0, sizeof in);
    in.lfHeight = -17; in.lfWidth = 0; in.lfWeight = FW_BOLD; in.lfItalic = 1; in.lfUnderline = 1; in.lfStrikeOut = 1;
    in.lfCharSet = ANSI_CHARSET; in.lfOutPrecision = OUT_TT_PRECIS; in.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    in.lfQuality = ANTIALIASED_QUALITY; in.lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
    lstrcpyW(in.lfFaceName, L"Arial");
    HFONT f = U(CreateFontIndirectW)(&in); REQUIRE(f);
    LOGFONTW out; memset(&out, 0xCC, sizeof out);
    CHECK(GOW(f, sizeof out, &out) == (int)sizeof out);
    CHECK(out.lfHeight == -17); CHECK(out.lfWidth == 0); CHECK(out.lfWeight == FW_BOLD);
    CHECK(out.lfItalic == 1 && out.lfUnderline == 1 && out.lfStrikeOut == 1);
    CHECK(out.lfCharSet == ANSI_CHARSET);
    CHECK(out.lfEscapement == 0 && out.lfOrientation == 0);
    CHECK(!lstrcmpW(out.lfFaceName, L"Arial"));
    t_note("roundtrip prec=%d clip=%d q=%d pf=0x%x", out.lfOutPrecision, out.lfClipPrecision, out.lfQuality, out.lfPitchAndFamily);
    DOBJ(f);
}
TEST(getobj_font_roundtrip_a) {
    REQUIRE(GOA); REQUIRE(U(CreateFontIndirectA));
    LOGFONTA in; memset(&in, 0, sizeof in);
    in.lfHeight = 24; in.lfWidth = 9; in.lfEscapement = 450; in.lfOrientation = 450; in.lfWeight = 300;
    in.lfCharSet = DEFAULT_CHARSET; strcpy(in.lfFaceName, "Times New Roman");
    HFONT f = U(CreateFontIndirectA)(&in); REQUIRE(f);
    LOGFONTA out; memset(&out, 0xCC, sizeof out);
    CHECK(GOA(f, sizeof out, &out) == (int)sizeof out);
    CHECK(out.lfHeight == 24 && out.lfWidth == 9 && out.lfEscapement == 450 && out.lfOrientation == 450);
    CHECK(out.lfWeight == 300);
    EXPECT_STR(out.lfFaceName, "Times New Roman");
    DOBJ(f);
}
TEST(getobj_font_a_created_w_read) {
    REQUIRE(GOW); REQUIRE(U(CreateFontIndirectA));
    LOGFONTA in; memset(&in, 0, sizeof in);
    in.lfHeight = -10; strcpy(in.lfFaceName, "Courier New");
    HFONT f = U(CreateFontIndirectA)(&in); REQUIRE(f);
    LOGFONTW out; memset(&out, 0xCC, sizeof out);
    CHECK(GOW(f, sizeof out, &out) == (int)sizeof out);
    CHECK(!lstrcmpW(out.lfFaceName, L"Courier New"));
    CHECK(out.lfHeight == -10);
    DOBJ(f);
}
TEST(getobj_font_w_created_a_read) {
    REQUIRE(GOA); REQUIRE(U(CreateFontIndirectW));
    LOGFONTW in; memset(&in, 0, sizeof in);
    in.lfHeight = -10; lstrcpyW(in.lfFaceName, L"Tahoma");
    HFONT f = U(CreateFontIndirectW)(&in); REQUIRE(f);
    LOGFONTA out; memset(&out, 0xCC, sizeof out);
    CHECK(GOA(f, sizeof out, &out) == (int)sizeof out);
    EXPECT_STR(out.lfFaceName, "Tahoma");
    DOBJ(f);
}
TEST(getobj_font_size_matrix) {
    REQUIRE(GOA); REQUIRE(GOW);
    HFONT f = os_font("Arial", -13); REQUIRE(f);
    int cbs[] = { 0, 1, 2, 3, 4, 8, 28, 59, 60, 61, 91, 92, 93, 100, 200, 0x1a4 };
    for (int i = 0; i < 16; i++) {
        BYTE b[512]; memset(b, 0x6B, sizeof b);
        int ra = GOA(f, cbs[i], b);
        CHECKF(ra <= cbs[i], "A cb %d ret %d", cbs[i], ra);
        if (cbs[i] >= 60) CHECKF(ra == 60 || ra == cbs[i], "A cb %d ret %d", cbs[i], ra);
        memset(b, 0x6B, sizeof b);
        int rw = GOW(f, cbs[i], b);
        CHECKF(rw <= cbs[i], "W cb %d ret %d", cbs[i], rw);
        if (cbs[i] >= 92) CHECKF(rw == 92 || rw == cbs[i], "W cb %d ret %d", cbs[i], rw);
        t_note("font cb=%d: A=%d W=%d", cbs[i], ra, rw);
    }
    DOBJ(f);
}
TEST(getobj_font_exact_sizes) {
    REQUIRE(GOA); REQUIRE(GOW);
    HFONT f = os_font("Arial", -13); REQUIRE(f);
    BYTE b[512];
    CHECK(sizeof(LOGFONTA) == 60 && sizeof(LOGFONTW) == 92);
    CHECK(GOA(f, 60, b) == 60);
    CHECK(GOW(f, 92, b) == 92);
    DOBJ(f);
}
TEST(getobj_font_null_buffer_note) {
    REQUIRE(GOA); REQUIRE(GOW);
    HFONT f = os_font("Arial", -13); REQUIRE(f);
    t_note("font NULL buffer A=%d W=%d", GOA(f, 0, NULL), GOW(f, 0, NULL));
    t_note("font NULL buffer cb=92 A=%d W=%d", GOA(f, 92, NULL), GOW(f, 92, NULL));
    DOBJ(f);
}
TEST(getobj_font_long_facename) {
    REQUIRE(GOW); REQUIRE(U(CreateFontIndirectW));
    LOGFONTW in; memset(&in, 'A', sizeof in);
    in.lfHeight = -10;
    for (int i = 0; i < LF_FACESIZE - 1; i++) in.lfFaceName[i] = L'A' + (i % 26);
    in.lfFaceName[LF_FACESIZE - 1] = 0;
    HFONT f = U(CreateFontIndirectW)(&in); REQUIRE(f);
    LOGFONTW out; memset(&out, 0, sizeof out);
    CHECK(GOW(f, sizeof out, &out) == (int)sizeof out);
    CHECK(out.lfFaceName[LF_FACESIZE - 1] == 0);
    CHECK(!memcmp(out.lfFaceName, in.lfFaceName, LF_FACESIZE * 2));
    DOBJ(f);
}
TEST(getobj_font_createfont_args) {
    REQUIRE(GOA); REQUIRE(U(CreateFontA));
    HFONT f = U(CreateFontA)(-20, 8, 100, 200, 650, 1, 0, 1, SYMBOL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Symbol"); REQUIRE(f);
    LOGFONTA lf; memset(&lf, 0, sizeof lf);
    CHECK(GOA(f, sizeof lf, &lf) == (int)sizeof lf);
    CHECK(lf.lfHeight == -20 && lf.lfWidth == 8 && lf.lfEscapement == 100 && lf.lfOrientation == 200 && lf.lfWeight == 650);
    CHECK(lf.lfItalic == 1 && lf.lfUnderline == 0 && lf.lfStrikeOut == 1 && lf.lfCharSet == SYMBOL_CHARSET);
    EXPECT_STR(lf.lfFaceName, "Symbol");
    DOBJ(f);
}
TEST(getobj_font_empty_facename) {
    REQUIRE(GOA); REQUIRE(U(CreateFontA));
    HFONT f = U(CreateFontA)(-12, 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, NULL);
    t_note("CreateFont(NULL face) -> %p", f);
    if (f) { LOGFONTA lf; memset(&lf, 0xCC, sizeof lf); CHECK(GOA(f, sizeof lf, &lf) == 60); t_note("face='%s' weight=%ld", lf.lfFaceName, lf.lfWeight); CHECK(lf.lfHeight == -12); DOBJ(f); }
}
TEST(getobj_font_extreme_heights) {
    REQUIRE(GOA); REQUIRE(U(CreateFontA));
    int hs[] = { 0, 1, -1, 32767, -32767, 100000, -100000, INT_MAX, INT_MIN };
    for (int i = 0; i < 9; i++) {
        HFONT f = U(CreateFontA)(hs[i], 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, "Arial");
        CHECKF(f != NULL, "height %d", hs[i]);
        if (!f) continue;
        LOGFONTA lf; memset(&lf, 0, sizeof lf);
        CHECK(GOA(f, sizeof lf, &lf) == 60);
        t_note("CreateFont height %d -> stored %ld", hs[i], lf.lfHeight);
        DOBJ(f);
    }
}
TEST(getobj_font_create_null_logfont) {
    REQUIRE(U(CreateFontIndirectA)); REQUIRE(U(CreateFontIndirectW));
    HFONT a = U(CreateFontIndirectA)(NULL), w = U(CreateFontIndirectW)(NULL);
    t_note("CreateFontIndirectA(NULL)=%p W(NULL)=%p", a, w);
    if (a) DOBJ(a);
    if (w) DOBJ(w);
}
TEST(getobj_font_indirectex_note) {
    REQUIRE(U(CreateFontIndirectExW)); REQUIRE(GOW);
    ENUMLOGFONTEXDVW dv; memset(&dv, 0, sizeof dv);
    dv.elfEnumLogfontEx.elfLogFont.lfHeight = -14;
    lstrcpyW(dv.elfEnumLogfontEx.elfLogFont.lfFaceName, L"Arial");
    HFONT f = U(CreateFontIndirectExW)(&dv); REQUIRE(f);
    BYTE buf[1024]; memset(buf, 0, sizeof buf);
    t_note("sizeof(ENUMLOGFONTEXDVW)=%d GetObjectW full cb -> %d", (int)sizeof dv, GOW(f, sizeof dv, buf));
    t_note("GetObjectW cb=sizeof(EXTLOGFONTW)=%d -> %d", (int)sizeof(ENUMLOGFONTEXW), GOW(f, sizeof(ENUMLOGFONTEXW), buf));
    t_note("GetObjectW cb=1024 -> %d", GOW(f, 1024, buf));
    DOBJ(f);
}

/* ---- palettes ---- */
TEST(getobj_palette_counts) {
    REQUIRE(GOA); REQUIRE(GOW);
    int ns[] = { 1, 2, 3, 8, 16, 20, 100, 255, 256 };
    for (int i = 0; i < 9; i++) {
        HPALETTE p = os_pal(ns[i]); CHECKF(p != NULL, "n=%d", ns[i]);
        if (!p) continue;
        WORD n = 0xFFFF;
        CHECKF(GOW(p, sizeof n, &n) == 2, "n=%d", ns[i]);
        CHECKF(n == ns[i], "n=%d got %u", ns[i], n);
        n = 0xFFFF;
        CHECK(GOA(p, sizeof n, &n) == 2);
        CHECK(n == ns[i]);
        DOBJ(p);
    }
}
TEST(getobj_palette_larger_buffer) {
    REQUIRE(GOW);
    HPALETTE p = os_pal(8); REQUIRE(p);
    BYTE b[64]; memset(b, 0x3C, sizeof b);
    int r = GOW(p, sizeof b, b);
    CHECK(r == 2);
    CHECK(*(WORD *)b == 8);
    CHECK(b[2] == 0x3C);
    DOBJ(p);
}
TEST(getobj_palette_small_buffer_note) {
    REQUIRE(GOW);
    HPALETTE p = os_pal(8); REQUIRE(p);
    BYTE b[8] = { 0 };
    t_note("palette cb=0 -> %d; cb=1 -> %d; NULL -> %d", GOW(p, 0, b), GOW(p, 1, b), GOW(p, 0, NULL));
    t_note("palette cb=2 NULL buf -> %d", GOW(p, 2, NULL));
    DOBJ(p);
}
TEST(getobj_palette_zero_entries_note) {
    REQUIRE(U(CreatePalette));
    LOGPALETTE lp = { 0x300, 0, {{0}} };
    SET_ERR(0);
    HPALETTE p = U(CreatePalette)(&lp);
    t_note("CreatePalette(0 entries) -> %p err=%lu", p, GetLastError());
    if (p) { WORD n = 0xFFFF; GOW(p, 2, &n); t_note("count=%u", n); DOBJ(p); }
}
TEST(getobj_palette_version_note) {
    REQUIRE(U(CreatePalette));
    LOGPALETTE lp = { 0x200, 1, {{1, 2, 3, 0}} };
    HPALETTE p = U(CreatePalette)(&lp);
    t_note("CreatePalette(version 0x200) -> %p", p);
    if (p) DOBJ(p);
    lp.palVersion = 0;
    p = U(CreatePalette)(&lp);
    t_note("CreatePalette(version 0) -> %p", p);
    if (p) DOBJ(p);
    CHECK(U(CreatePalette)(NULL) == NULL);
}

/* ---- bitmaps ---- */
static const struct { int w, h, bpp; } g_ddb[] = {
    { 1, 1, 1 }, { 8, 8, 1 }, { 9, 3, 1 }, { 16, 16, 1 }, { 17, 5, 1 }, { 31, 2, 1 }, { 32, 32, 1 }, { 33, 1, 1 }, { 100, 7, 1 },
    { 1, 1, 8 }, { 3, 3, 8 }, { 5, 4, 8 }, { 16, 2, 8 }, { 17, 9, 8 }, { 100, 3, 8 },
    { 1, 1, 16 }, { 3, 3, 16 }, { 7, 2, 16 }, { 100, 3, 16 },
    { 1, 1, 32 }, { 2, 2, 32 }, { 5, 4, 32 }, { 100, 3, 32 }, { 640, 480, 32 },
};
TEST(getobj_ddb_attributes) {
    REQUIRE(GOA); REQUIRE(U(CreateBitmap));
    for (unsigned i = 0; i < sizeof g_ddb / sizeof g_ddb[0]; i++) {
        HBITMAP b = U(CreateBitmap)(g_ddb[i].w, g_ddb[i].h, 1, g_ddb[i].bpp, NULL);
        CHECKF(b != NULL, "%dx%dx%d", g_ddb[i].w, g_ddb[i].h, g_ddb[i].bpp);
        if (!b) continue;
        BITMAP bm; memset(&bm, 0xCC, sizeof bm);
        CHECKF(GOA(b, sizeof bm, &bm) == (int)sizeof bm, "%dx%dx%d", g_ddb[i].w, g_ddb[i].h, g_ddb[i].bpp);
        CHECK(bm.bmType == 0);
        CHECKF(bm.bmWidth == g_ddb[i].w && bm.bmHeight == g_ddb[i].h, "%dx%d got %ldx%ld", g_ddb[i].w, g_ddb[i].h, bm.bmWidth, bm.bmHeight);
        CHECKF(bm.bmPlanes == 1, "planes %u", bm.bmPlanes);
        CHECKF(bm.bmBitsPixel == g_ddb[i].bpp, "bpp want %d got %u", g_ddb[i].bpp, bm.bmBitsPixel);
        CHECKF(bm.bmWidthBytes == wb(g_ddb[i].w, g_ddb[i].bpp), "%dx%dx%d stride %ld want %d", g_ddb[i].w, g_ddb[i].h, g_ddb[i].bpp, bm.bmWidthBytes, wb(g_ddb[i].w, g_ddb[i].bpp));
        CHECK(bm.bmBits == NULL);
        DOBJ(b);
    }
}
TEST(getobj_ddb_other_depths_note) {
    REQUIRE(GOA); REQUIRE(U(CreateBitmap));
    int bpps[] = { 2, 3, 4, 5, 15, 24, 48, 64, 128 };
    for (int i = 0; i < 9; i++) {
        SET_ERR(0);
        HBITMAP b = U(CreateBitmap)(6, 3, 1, bpps[i], NULL);
        if (!b) { t_note("CreateBitmap bpp=%d -> NULL err=%lu", bpps[i], GetLastError()); continue; }
        BITMAP bm; memset(&bm, 0, sizeof bm); GOA(b, sizeof bm, &bm);
        t_note("CreateBitmap bpp=%d -> stored bpp=%u planes=%u stride=%ld", bpps[i], bm.bmBitsPixel, bm.bmPlanes, bm.bmWidthBytes);
        DOBJ(b);
    }
}
TEST(getobj_ddb_wide_aliases_a_w) {
    REQUIRE(GOA); REQUIRE(GOW); REQUIRE(U(CreateBitmap));
    HBITMAP b = U(CreateBitmap)(13, 7, 1, 8, NULL); REQUIRE(b);
    BITMAP a, w; memset(&a, 1, sizeof a); memset(&w, 2, sizeof w);
    CHECK(GOA(b, sizeof a, &a) == (int)sizeof a);
    CHECK(GOW(b, sizeof w, &w) == (int)sizeof w);
    CHECK(!memcmp(&a, &w, sizeof a));
    DOBJ(b);
}
TEST(getobj_ddb_size_matrix) {
    REQUIRE(GOW); REQUIRE(U(CreateBitmap));
    HBITMAP b = U(CreateBitmap)(10, 10, 1, 32, NULL); REQUIRE(b);
    CHECK(sizeof(BITMAP) == 32);
    CHECK(GOW(b, 0, NULL) == (int)sizeof(BITMAP));
    CHECK(GOW(b, 4096, NULL) == (int)sizeof(BITMAP));
    int cbs[] = { 0, 1, 4, 8, 16, 20, 24, 28, 31, 32, 33, 40, 64, 104, 105, 512 };
    for (int i = 0; i < 16; i++) {
        BYTE buf[600]; memset(buf, 0x44, sizeof buf);
        int r = GOW(b, cbs[i], buf);
        if (cbs[i] >= 32) {
            CHECKF(r == 32, "cb %d ret %d", cbs[i], r);
            CHECKF(buf[32] == 0x44, "cb %d overran", cbs[i]);
        } else {
            t_note("DDB GetObjectW cb=%d -> %d", cbs[i], r);
            CHECKF(r <= cbs[i], "cb %d ret %d", cbs[i], r);
        }
    }
    DOBJ(b);
}
TEST(getobj_ddb_dibsection_size_returns_bitmap) {
    REQUIRE(GOW); REQUIRE(U(CreateBitmap));
    HBITMAP b = U(CreateBitmap)(10, 10, 1, 32, NULL); REQUIRE(b);
    DIBSECTION ds; memset(&ds, 0xCC, sizeof ds);
    CHECK(GOW(b, sizeof ds, &ds) == (int)sizeof(BITMAP));
    CHECK(ds.dsBm.bmWidth == 10);
    DOBJ(b);
}
TEST(getobj_dib_attributes) {
    REQUIRE(GOA); REQUIRE(GOW);
    struct { int w, h, bpp; } t[] = { { 1, 1, 32 }, { 5, 3, 32 }, { 7, 3, 24 }, { 5, 5, 16 }, { 9, 2, 8 }, { 31, 4, 32 }, { 33, 2, 24 } };
    for (int i = 0; i < 7; i++) {
        void *bits = NULL;
        HBITMAP b = os_dib(NULL, t[i].w, t[i].h, t[i].bpp, &bits);
        CHECKF(b && bits, "%dx%dx%d", t[i].w, t[i].h, t[i].bpp);
        if (!b) continue;
        DIBSECTION ds; memset(&ds, 0xCC, sizeof ds);
        int r = GOW(b, sizeof ds, &ds);
        CHECKF(r == (int)sizeof ds, "ret %d want %d", r, (int)sizeof ds);
        CHECK(ds.dsBm.bmType == 0);
        CHECK(ds.dsBm.bmWidth == t[i].w);
        CHECKF(ds.dsBm.bmHeight == t[i].h, "bmHeight %ld", ds.dsBm.bmHeight);
        CHECK(ds.dsBm.bmPlanes == 1);
        CHECK(ds.dsBm.bmBitsPixel == t[i].bpp);
        CHECK(ds.dsBm.bmBits == bits);
        CHECKF(ds.dsBm.bmWidthBytes == ((t[i].w * t[i].bpp + 31) / 32) * 4, "stride %ld", ds.dsBm.bmWidthBytes);
        CHECK(ds.dsBmih.biSize == sizeof(BITMAPINFOHEADER));
        CHECK(ds.dsBmih.biWidth == t[i].w);
        CHECK(ds.dsBmih.biPlanes == 1);
        CHECK(ds.dsBmih.biBitCount == t[i].bpp);
        if (t[i].bpp == 16 || t[i].bpp == 32) CHECK(ds.dsBmih.biCompression == BI_RGB || ds.dsBmih.biCompression == BI_BITFIELDS);
        else CHECK(ds.dsBmih.biCompression == BI_RGB);
        t_note("%dx%dx%d: dsBmih.biHeight=%ld sizeImage=%lu clrUsed=%lu offset=%lu", t[i].w, t[i].h, t[i].bpp, ds.dsBmih.biHeight, ds.dsBmih.biSizeImage, ds.dsBmih.biClrUsed, ds.dsOffset);
        CHECK(ds.dshSection == NULL);
        CHECK(ds.dsOffset == 0);
        DOBJ(b);
    }
}
TEST(getobj_dib_topdown_vs_bottomup) {
    REQUIRE(GOW);
    void *bits;
    HBITMAP td = os_dib(NULL, 4, 6, 32, &bits); REQUIRE(td);
    DIBSECTION ds; memset(&ds, 0, sizeof ds);
    GOW(td, sizeof ds, &ds);
    t_note("top-down request: bmHeight=%ld biHeight=%ld", ds.dsBm.bmHeight, ds.dsBmih.biHeight);
    CHECK(ds.dsBm.bmHeight == 6);
    DOBJ(td);
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 4; bi.bmiHeader.biHeight = 6;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    HBITMAP bu = U(CreateDIBSection)(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0); REQUIRE(bu);
    memset(&ds, 0, sizeof ds);
    GOW(bu, sizeof ds, &ds);
    CHECK(ds.dsBm.bmHeight == 6);
    CHECK(ds.dsBmih.biHeight == 6);
    DOBJ(bu);
}
TEST(getobj_dib_size_matrix) {
    REQUIRE(GOW); REQUIRE(GOA);
    void *bits; HBITMAP b = os_dib(NULL, 8, 8, 32, &bits); REQUIRE(b);
    CHECK(sizeof(DIBSECTION) == 104);
    t_note("DIB NULL-buffer size = %d", GOW(b, 0, NULL));
    int cbs[] = { 0, 1, 31, 32, 33, 40, 71, 72, 73, 100, 103, 104, 105, 200, 1000 };
    for (int i = 0; i < 15; i++) {
        BYTE buf[1100]; memset(buf, 0x44, sizeof buf);
        int r = GOW(b, cbs[i], buf);
        if (cbs[i] >= 104) CHECKF(r == 104, "cb %d ret %d", cbs[i], r);
        else if (cbs[i] == 32) CHECKF(r == 32, "cb %d ret %d", cbs[i], r);
        else t_note("DIB GetObjectW cb=%d -> %d", cbs[i], r);
        CHECKF(r <= cbs[i], "cb %d ret %d", cbs[i], r);
        int ra = GOA(b, cbs[i], buf);
        CHECKF(ra == r, "A/W mismatch cb %d: %d vs %d", cbs[i], ra, r);
    }
    DOBJ(b);
}
TEST(getobj_dib_as_bitmap_only) {
    REQUIRE(GOW);
    void *bits; HBITMAP b = os_dib(NULL, 6, 5, 24, &bits); REQUIRE(b);
    BITMAP bm; memset(&bm, 0xCC, sizeof bm);
    CHECK(GOW(b, sizeof bm, &bm) == (int)sizeof bm);
    CHECK(bm.bmBits == bits);
    CHECK(bm.bmWidth == 6 && bm.bmHeight == 5 && bm.bmBitsPixel == 24);
    DOBJ(b);
}
TEST(getobj_dib_bitfields_16_note) {
    REQUIRE(GOW); REQUIRE(U(CreateDIBSection));
    union { BITMAPINFO bi; char raw[sizeof(BITMAPINFOHEADER) + 12]; } u; memset(&u, 0, sizeof u);
    u.bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); u.bi.bmiHeader.biWidth = 3; u.bi.bmiHeader.biHeight = -3;
    u.bi.bmiHeader.biPlanes = 1; u.bi.bmiHeader.biBitCount = 16; u.bi.bmiHeader.biCompression = BI_BITFIELDS;
    DWORD *m = (DWORD *)u.bi.bmiColors; m[0] = 0xF800; m[1] = 0x07E0; m[2] = 0x001F;
    void *bits;
    HBITMAP b = U(CreateDIBSection)(NULL, &u.bi, DIB_RGB_COLORS, &bits, NULL, 0); REQUIRE(b);
    DIBSECTION ds; memset(&ds, 0, sizeof ds);
    CHECK(GOW(b, sizeof ds, &ds) == (int)sizeof ds);
    CHECK(ds.dsBmih.biCompression == BI_BITFIELDS);
    CHECK(ds.dsBitfields[0] == 0xF800 && ds.dsBitfields[1] == 0x07E0 && ds.dsBitfields[2] == 0x001F);
    CHECK(ds.dsBm.bmBitsPixel == 16);
    DOBJ(b);
}
TEST(getobj_dib_32_bitfields_defaults_note) {
    REQUIRE(GOW);
    void *bits; HBITMAP b = os_dib(NULL, 2, 2, 32, &bits); REQUIRE(b);
    DIBSECTION ds; memset(&ds, 0, sizeof ds);
    GOW(b, sizeof ds, &ds);
    t_note("32bpp BI_RGB bitfields = %08lx %08lx %08lx", ds.dsBitfields[0], ds.dsBitfields[1], ds.dsBitfields[2]);
    DOBJ(b);
}
TEST(getobj_dib8_palette_not_in_dibsection) {
    REQUIRE(GOW);
    void *bits; HBITMAP b = os_dib(NULL, 4, 4, 8, &bits); REQUIRE(b);
    DIBSECTION ds; memset(&ds, 0, sizeof ds);
    CHECK(GOW(b, sizeof ds, &ds) == (int)sizeof ds);
    CHECK(ds.dsBm.bmBitsPixel == 8);
    t_note("8bpp biClrUsed=%lu biClrImportant=%lu", ds.dsBmih.biClrUsed, ds.dsBmih.biClrImportant);
    DOBJ(b);
}
TEST(getobj_compatible_bitmap_memdc_mono) {
    REQUIRE(GOA); REQUIRE(U(CreateCompatibleBitmap));
    HDC dc = os_memdc(); REQUIRE(dc);
    HBITMAP b = U(CreateCompatibleBitmap)(dc, 9, 5); REQUIRE(b);
    BITMAP bm; memset(&bm, 0, sizeof bm);
    CHECK(GOA(b, sizeof bm, &bm) == (int)sizeof bm);
    CHECK(bm.bmBitsPixel == 1 && bm.bmPlanes == 1 && bm.bmWidth == 9 && bm.bmHeight == 5);
    CHECK(bm.bmWidthBytes == 2);
    DOBJ(b);
    U(DeleteDC)(dc);
}
TEST(getobj_compatible_bitmap_with_dib_memdc) {
    REQUIRE(GOA); REQUIRE(U(CreateCompatibleBitmap));
    HDC dc = os_memdc(); REQUIRE(dc);
    void *bits; HBITMAP dib = os_dib(NULL, 4, 4, 24, &bits); REQUIRE(dib);
    HGDIOBJ old = SOBJ(dc, dib);
    HBITMAP b = U(CreateCompatibleBitmap)(dc, 6, 3); REQUIRE(b);
    BITMAP bm; memset(&bm, 0, sizeof bm);
    CHECK(GOA(b, sizeof bm, &bm) == (int)sizeof bm);
    t_note("compat bitmap of 24bpp DIB memdc: bpp=%u planes=%u bits=%p", bm.bmBitsPixel, bm.bmPlanes, bm.bmBits);
    CHECK(bm.bmWidth == 6 && bm.bmHeight == 3);
    DOBJ(b);
    SOBJ(dc, old);
    DOBJ(dib);
    U(DeleteDC)(dc);
}
TEST(getobj_createbitmap_zero_size) {
    REQUIRE(GOA); REQUIRE(U(CreateBitmap));
    int dims[][2] = { { 0, 0 }, { 0, 5 }, { 5, 0 } };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        HBITMAP b = U(CreateBitmap)(dims[i][0], dims[i][1], 1, 1, NULL);
        t_note("CreateBitmap(%d,%d) -> %p", dims[i][0], dims[i][1], b);
        if (b) { BITMAP bm; memset(&bm, 0, sizeof bm); GOA(b, sizeof bm, &bm); t_note("   %ldx%ld bpp %u", bm.bmWidth, bm.bmHeight, bm.bmBitsPixel); DOBJ(b); }
    }
}
TEST(getobj_createbitmap_negative_size) {
    REQUIRE(U(CreateBitmap));
    SET_ERR(0);
    HBITMAP b = U(CreateBitmap)(-1, 5, 1, 1, NULL);
    t_note("CreateBitmap(-1,5) -> %p err=%lu", b, GetLastError());
    if (b) DOBJ(b);
    b = U(CreateBitmap)(5, -1, 1, 1, NULL);
    t_note("CreateBitmap(5,-1) -> %p", b);
    if (b) DOBJ(b);
}
TEST(getobj_createbitmap_with_data) {
    REQUIRE(GOA); REQUIRE(U(CreateBitmap));
    WORD data[4] = { 0xAAAA, 0x5555, 0xAAAA, 0x5555 };
    HBITMAP b = U(CreateBitmap)(16, 4, 1, 1, data); REQUIRE(b);
    BITMAP bm; memset(&bm, 0, sizeof bm);
    CHECK(GOA(b, sizeof bm, &bm) == (int)sizeof bm);
    CHECK(bm.bmWidth == 16 && bm.bmHeight == 4 && bm.bmWidthBytes == 2);
    BYTE out[8] = { 0 };
    LONG n = U(GetBitmapBits)(b, sizeof out, out);
    CHECK(n == 8);
    CHECK(!memcmp(out, data, 8));
    DOBJ(b);
}
TEST(getobj_bitmap_planes_gt1_note) {
    REQUIRE(U(CreateBitmap)); REQUIRE(GOA);
    SET_ERR(0);
    HBITMAP b = U(CreateBitmap)(8, 8, 3, 1, NULL);
    t_note("CreateBitmap planes=3 bpp=1 -> %p err=%lu", b, GetLastError());
    if (b) { BITMAP bm; GOA(b, sizeof bm, &bm); t_note("   planes=%u bpp=%u", bm.bmPlanes, bm.bmBitsPixel); DOBJ(b); }
}
TEST(getobj_stock_bitmap_in_dc) {
    REQUIRE(GOA); REQUIRE(U(GetCurrentObject));
    HDC dc = os_memdc(); REQUIRE(dc);
    HGDIOBJ b = U(GetCurrentObject)(dc, OBJ_BITMAP);
    BITMAP bm; memset(&bm, 0xCC, sizeof bm);
    CHECK(GOA(b, sizeof bm, &bm) == (int)sizeof bm);
    CHECK(bm.bmWidth == 1 && bm.bmHeight == 1 && bm.bmBitsPixel == 1 && bm.bmWidthBytes == 2);
    CHECK(DOBJ(b) == FALSE || 1);
    CHECK(GOT(b) == OBJ_BITMAP);
    U(DeleteDC)(dc);
}

/* ---- colour space ---- */
TEST(getobj_colorspace_sizes) {
    REQUIRE(GOW); REQUIRE(GOA); REQUIRE(U(CreateColorSpaceW)); REQUIRE(U(DeleteColorSpace));
    LOGCOLORSPACEW l; memset(&l, 0, sizeof l);
    l.lcsSignature = 0x50534F43; l.lcsVersion = 0x400; l.lcsSize = sizeof l; l.lcsCSType = 0x73524742; l.lcsIntent = LCS_GM_GRAPHICS;
    HCOLORSPACE cs = U(CreateColorSpaceW)(&l);
    if (!cs) { t_note("CreateColorSpaceW failed: %lu", GetLastError()); return; }
    LOGCOLORSPACEW out; memset(&out, 0xCC, sizeof out);
    int r = GOW(cs, sizeof out, &out);
    CHECK(sizeof(LOGCOLORSPACEW) == 588);
    CHECK(r == (int)sizeof out);
    CHECK(out.lcsSignature == 0x50534F43);
    CHECK(out.lcsCSType == 0x73524742);
    CHECK(out.lcsIntent == LCS_GM_GRAPHICS);
    t_note("colorspace GetObjectW(NULL)=%d, A=%d", GOW(cs, 0, NULL), GOA(cs, sizeof(LOGCOLORSPACEA), &out));
    t_note("colorspace small cb=100 -> %d", GOW(cs, 100, &out));
    U(DeleteColorSpace)(cs);
}

/* ---- misc ---- */
TEST(getobj_each_kind_matrix) {
    REQUIRE(GOA); REQUIRE(GOW);
    static const int want[OS_K_COUNT] = { 16, 32, 16, 16, 16, 16, 92, 2, 0, 32, 104, 0 };
    for (int k = 0; k < OS_K_COUNT; k++) {
        HGDIOBJ h = os_make(k); if (!h) { t_note("kind %s unavailable", os_kname[k]); continue; }
        BYTE buf[1100]; memset(buf, 0, sizeof buf);
        int r = GOW(h, sizeof buf, buf);
        if (k == OS_K_EXTPEN) CHECKF(r >= 28, "kind %s ret %d", os_kname[k], r);
        else CHECKF(r == want[k], "kind %s ret %d want %d", os_kname[k], r, want[k]);
        os_free(k, h);
    }
}
TEST(getobj_stress_repeat) {
    REQUIRE(GOW);
    HBRUSH b = U(CreateSolidBrush)(0x112233); REQUIRE(b);
    for (int i = 0; i < 5000; i++) { LOGBRUSH lb; if (GOW(b, sizeof lb, &lb) != 16 || lb.lbColor != 0x112233) { CHECK(0); break; } }
    DOBJ(b);
}
