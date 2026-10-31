/* Exports presence and bk-colour fills at different surface depths (exact where the format is exact). */
#include "dca.h"

static const char *const names[] = {
    "SetBkColor", "GetBkColor", "SetTextColor", "GetTextColor", "SetBkMode", "GetBkMode", "SetROP2", "GetROP2",
    "SetPolyFillMode", "GetPolyFillMode", "SetStretchBltMode", "GetStretchBltMode", "SetArcDirection", "GetArcDirection",
    "SetMiterLimit", "GetMiterLimit", "SetRelAbs", "GetRelAbs", "SetTextAlign", "GetTextAlign", "SetTextCharacterExtra",
    "GetTextCharacterExtra", "SetTextJustification", "SetLayout", "GetLayout", "SetGraphicsMode", "GetGraphicsMode",
    "SetMapperFlags", "SetBrushOrgEx", "GetBrushOrgEx", "SetDCBrushColor", "GetDCBrushColor", "SetDCPenColor", "GetDCPenColor",
    "MoveToEx", "LineTo", "GetCurrentPositionEx", "SaveDC", "RestoreDC", "UnrealizeObject",
};
TEST(dca_exports_in_gdi32) {
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++)
        CHECKF(t_proc_gdi32(names[i]) != NULL, "gdi32.dll does not export %s", names[i]);
}
TEST(dca_exports_in_gdi32full) {
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) {
        if (!t_proc_gdi32full(names[i])) t_note("gdi32full.dll does not export %s", names[i]);
    }
    CHECK(t_gdi32full() != NULL);
}
TEST(dca_exports_gdi32_and_gdi32full_agree_on_behaviour) {
    REQUIRE(UG(SetBkColor)); REQUIRE(UF(SetBkColor)); REQUIRE(UG(GetBkColor));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    COLORREF p = UG(SetBkColor)(m.dc, 0x112233);
    CHECK(p == 0xFFFFFF);
    p = UF(SetBkColor)(m.dc, 0x445566);
    CHECK(p == 0x112233);
    CHECK(UG(GetBkColor)(m.dc) == 0x445566);
    SetLastError(0);
    CHECK(UG(SetBkColor)(NULL, 1) == CLR_INVALID);
    CHECK(UF(SetBkColor)(NULL, 1) == CLR_INVALID);
    mdc_free(&m);
}

/* ---- surface helpers ---- */
typedef struct { HDC dc; HBITMAP bm; HGDIOBJ old; BYTE *bits; int w, h, bpp, stride; } Surf;
static int surf_make(Surf *s, int bpp, int w, int h) {
    struct { BITMAPINFOHEADER h; RGBQUAD pal[256]; } bi;
    memset(&bi, 0, sizeof bi);
    bi.h.biSize = sizeof bi.h; bi.h.biWidth = w; bi.h.biHeight = -h; bi.h.biPlanes = 1; bi.h.biBitCount = bpp; bi.h.biCompression = BI_RGB;
    if (bpp <= 8) {
        int n = 1 << bpp;
        bi.h.biClrUsed = n;
        for (int i = 0; i < n; i++) bi.pal[i].rgbRed = bi.pal[i].rgbGreen = bi.pal[i].rgbBlue = (BYTE)(i * 255 / (n - 1));
    }
    s->dc = U(CreateCompatibleDC)(NULL);
    if (!s->dc) return 0;
    void *bits = NULL;
    s->bm = U(CreateDIBSection)(s->dc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!s->bm) { U(DeleteDC)(s->dc); return 0; }
    s->old = U(SelectObject)(s->dc, s->bm);
    s->bits = bits; s->w = w; s->h = h; s->bpp = bpp; s->stride = ((w * bpp + 31) / 32) * 4;
    return 1;
}
static void surf_free(Surf *s) { U(SelectObject)(s->dc, s->old); U(DeleteObject)(s->bm); U(DeleteDC)(s->dc); }
static unsigned surf_px(Surf *s, int x, int y) {
    U(GdiFlush)();
    BYTE *r = s->bits + y * s->stride;
    switch (s->bpp) {
    case 1: return (r[x >> 3] >> (7 - (x & 7))) & 1;
    case 4: return (x & 1) ? (r[x >> 1] & 15) : (r[x >> 1] >> 4);
    case 8: return r[x];
    case 16: return ((WORD *)r)[x];
    case 24: return r[x * 3] | (r[x * 3 + 1] << 8) | (r[x * 3 + 2] << 16);
    default: return ((DWORD *)r)[x] & 0xffffff;
    }
}
static void fill_bk(Surf *s, COLORREF c) {
    RECT r = { 0, 0, s->w, s->h };
    U(SetBkColor)(s->dc, c);
    U(ExtTextOutA)(s->dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL);
}
#define NEEDFILL() do { REQUIRE(U(SetBkColor)); REQUIRE(U(ExtTextOutA)); REQUIRE(U(CreateDIBSection)); } while (0)

TEST(dca_depth_fill_32bpp) {
    NEEDFILL();
    Surf s; if (!surf_make(&s, 32, 8, 8)) { t_skip("surface"); return; }
    static const COLORREF c[] = { 0, 0xFFFFFF, RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255), RGB(1, 2, 3), RGB(254, 253, 252), RGB(128, 64, 32) };
    for (unsigned i = 0; i < 8; i++) { fill_bk(&s, c[i]); CHECKF(surf_px(&s, 3, 3) == dca_px(c[i]), "i=%u 0x%06x", i, surf_px(&s, 3, 3)); }
    surf_free(&s);
}
TEST(dca_depth_fill_24bpp) {
    NEEDFILL();
    Surf s; if (!surf_make(&s, 24, 8, 8)) { t_skip("surface"); return; }
    static const COLORREF c[] = { 0, 0xFFFFFF, RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255), RGB(1, 2, 3), RGB(254, 253, 252), RGB(128, 64, 32) };
    for (unsigned i = 0; i < 8; i++) { fill_bk(&s, c[i]); CHECKF(surf_px(&s, 3, 3) == dca_px(c[i]), "i=%u 0x%06x", i, surf_px(&s, 3, 3)); }
    surf_free(&s);
}
TEST(dca_depth_fill_16bpp_primaries) {
    NEEDFILL();
    Surf s; if (!surf_make(&s, 16, 8, 8)) { t_skip("surface"); return; }
    static const struct { COLORREF c; unsigned px; } t[] = {
        { 0, 0x0000 }, { 0xFFFFFF, 0x7FFF }, { RGB(255, 0, 0), 0x7C00 }, { RGB(0, 255, 0), 0x03E0 }, { RGB(0, 0, 255), 0x001F } };
    for (unsigned i = 0; i < 5; i++) { fill_bk(&s, t[i].c); CHECKF(surf_px(&s, 3, 3) == t[i].px, "i=%u got 0x%04x want 0x%04x", i, surf_px(&s, 3, 3), t[i].px); }
    surf_free(&s);
}
TEST(dca_depth_fill_8bpp_grey_palette) {
    NEEDFILL();
    Surf s; if (!surf_make(&s, 8, 8, 8)) { t_skip("surface"); return; }
    int bad = 0;
    for (int v = 0; v < 256; v += 5) { fill_bk(&s, RGB(v, v, v)); if (surf_px(&s, 3, 3) != (unsigned)v) bad++; }
    CHECKF(bad == 0, "%d grey levels mapped to the wrong index", bad);
    surf_free(&s);
}
TEST(dca_depth_fill_4bpp_grey_palette) {
    NEEDFILL();
    Surf s; if (!surf_make(&s, 4, 8, 8)) { t_skip("surface"); return; }
    for (int k = 0; k < 16; k++) { fill_bk(&s, RGB(k * 17, k * 17, k * 17)); CHECKF(surf_px(&s, 3, 3) == (unsigned)k && surf_px(&s, 4, 3) == (unsigned)k, "k=%d got %u/%u", k, surf_px(&s, 3, 3), surf_px(&s, 4, 3)); }
    surf_free(&s);
}
TEST(dca_depth_fill_1bpp_black_white) {
    NEEDFILL();
    Surf s; if (!surf_make(&s, 1, 8, 8)) { t_skip("surface"); return; }
    fill_bk(&s, RGB(255, 255, 255));
    CHECKF(surf_px(&s, 3, 3) == 1 && surf_px(&s, 7, 7) == 1, "white -> %u", surf_px(&s, 3, 3));
    fill_bk(&s, RGB(0, 0, 0));
    CHECKF(surf_px(&s, 3, 3) == 0 && surf_px(&s, 7, 7) == 0, "black -> %u", surf_px(&s, 3, 3));
    surf_free(&s);
}
TEST(dca_depth_attr_unaffected_by_depth_of_fill) {
    NEEDFILL();
    static const int bpps[] = { 1, 4, 8, 16, 24, 32 };
    for (int i = 0; i < 6; i++) {
        Surf s; if (!surf_make(&s, bpps[i], 8, 8)) { t_skip("surface %d", bpps[i]); continue; }
        fill_bk(&s, 0x123456);
        CHECKF(U(GetBkColor)(s.dc) == 0x123456, "%dbpp GetBkColor 0x%06lx", bpps[i], U(GetBkColor)(s.dc));
        surf_free(&s);
    }
}
TEST(dca_depth_rop2_black_white_all_depths) {
    NEEDFILL(); REQUIRE(U(SetROP2)); REQUIRE(U(MoveToEx)); REQUIRE(U(LineTo)); REQUIRE(U(CreatePen));
    static const int bpps[] = { 1, 4, 8, 16, 24, 32 };
    static const unsigned white[] = { 1, 15, 255, 0x7FFF, 0xFFFFFF, 0xFFFFFF };
    for (int i = 0; i < 6; i++) {
        Surf s; if (!surf_make(&s, bpps[i], 16, 8)) { t_skip("surface %d", bpps[i]); continue; }
        fill_bk(&s, RGB(255, 255, 255));
        HPEN pen = U(CreatePen)(PS_SOLID, 1, RGB(255, 255, 255));
        HGDIOBJ op = U(SelectObject)(s.dc, pen);
        U(SetROP2)(s.dc, R2_BLACK);
        U(MoveToEx)(s.dc, 1, 3, NULL); U(LineTo)(s.dc, 10, 3);
        CHECKF(surf_px(&s, 5, 3) == 0, "%dbpp R2_BLACK -> %u", bpps[i], surf_px(&s, 5, 3));
        U(SetROP2)(s.dc, R2_WHITE);
        U(MoveToEx)(s.dc, 1, 5, NULL); U(LineTo)(s.dc, 10, 5);
        CHECKF(surf_px(&s, 5, 5) == white[i] || (bpps[i] == 16 && surf_px(&s, 5, 5) == 0xFFFF), "%dbpp R2_WHITE -> 0x%x", bpps[i], surf_px(&s, 5, 5));
        U(SetROP2)(s.dc, R2_NOT);
        U(MoveToEx)(s.dc, 1, 3, NULL); U(LineTo)(s.dc, 10, 3);
        CHECKF(surf_px(&s, 5, 3) == white[i] || (bpps[i] == 16 && surf_px(&s, 5, 3) == 0xFFFF), "%dbpp R2_NOT of black -> 0x%x", bpps[i], surf_px(&s, 5, 3));
        U(SelectObject)(s.dc, op); U(DeleteObject)(pen);
        surf_free(&s);
    }
}
