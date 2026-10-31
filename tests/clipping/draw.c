#include "clip.h"

/* Rendering with a clip must equal the unclipped rendering masked by the clip region. */
#define W 64
#define H 64
UDECL(BOOL, GdiGradientFill, (HDC, PTRIVERTEX, ULONG, PVOID, ULONG, ULONG));
UDECL(BOOL, GdiAlphaBlend, (HDC, int, int, int, int, HDC, int, int, int, int, BLENDFUNCTION));
UDECL(BOOL, GdiTransparentBlt, (HDC, int, int, int, int, HDC, int, int, int, int, UINT));

enum { OP_RECTANGLE, OP_ELLIPSE, OP_ROUNDRECT, OP_POLYGON, OP_POLYLINE, OP_BEZIER, OP_ARC, OP_PIE, OP_CHORD,
       OP_LINETO, OP_LINE_THICK, OP_FILLRGN, OP_PAINTRGN, OP_FRAMERGN, OP_INVERTRGN, OP_FILLRECT, OP_FRAMERECT,
       OP_PATBLT_PAT, OP_PATBLT_INV, OP_PATBLT_WHITE, OP_BITBLT, OP_STRETCH_UP, OP_STRETCH_DOWN, OP_STRETCH_FLIP,
       OP_SETDIBITS, OP_STRETCHDIBITS, OP_SETPIXEL, OP_FLOODFILL, OP_EXTTEXT_OPAQUE, OP_TEXTOUT, OP_GRADIENT,
       OP_ALPHABLEND, OP_TRANSPARENT, OP_POLYPOLYGON, OP_POLYBEZIERTO, OP_ANGLEARC, OP_EXTTEXT_CLIPPED, OP_COUNT };

static DWORD g_src[32 * 32];
static void do_op(int op, MDC *m) {
    HDC dc = m->dc;
    HGDIOBJ wp = U(GetStockObject)(WHITE_PEN), wb = U(GetStockObject)(WHITE_BRUSH);
    HGDIOBJ op0 = U(SelectObject)(dc, wp), ob0 = U(SelectObject)(dc, wb);
    U(SetBkMode)(dc, OPAQUE);
    U(SetBkColor)(dc, 0xFFFFFF); U(SetTextColor)(dc, 0x0000FF);
    HRGN rg = U(CreateEllipticRgn)(4, 4, 60, 56);
    for (int i = 0; i < 32 * 32; i++) g_src[i] = 0x00FFFFFF;
    switch (op) {
    case OP_RECTANGLE: U(Rectangle)(dc, 2, 2, 62, 62); break;
    case OP_ELLIPSE: U(Ellipse)(dc, 1, 1, 63, 63); break;
    case OP_ROUNDRECT: U(RoundRect)(dc, 2, 2, 62, 62, 20, 20); break;
    case OP_POLYGON: { POINT p[] = {{4, 4}, {60, 20}, {30, 60}, {10, 40}}; U(Polygon)(dc, p, 4); } break;
    case OP_POLYLINE: { POINT p[] = {{0, 0}, {63, 10}, {5, 40}, {60, 63}}; U(Polyline)(dc, p, 4); } break;
    case OP_BEZIER: { POINT p[] = {{2, 2}, {60, 2}, {2, 60}, {60, 60}}; U(PolyBezier)(dc, p, 4); } break;
    case OP_ARC: U(Arc)(dc, 2, 2, 62, 62, 62, 32, 2, 32); break;
    case OP_PIE: U(Pie)(dc, 2, 2, 62, 62, 62, 20, 10, 62); break;
    case OP_CHORD: U(Chord)(dc, 2, 2, 62, 62, 62, 20, 10, 62); break;
    case OP_LINETO: U(MoveToEx)(dc, 0, 0, NULL); U(LineTo)(dc, 63, 63); U(MoveToEx)(dc, 63, 0, NULL); U(LineTo)(dc, 0, 63); break;
    case OP_LINE_THICK: { HPEN p = U(CreatePen)(PS_SOLID, 7, 0xFFFFFF); HGDIOBJ o = U(SelectObject)(dc, p);
        U(MoveToEx)(dc, 3, 10, NULL); U(LineTo)(dc, 60, 50); U(SelectObject)(dc, o); U(DeleteObject)(p); } break;
    case OP_FILLRGN: { HBRUSH b = U(CreateSolidBrush)(0xFFFFFF); U(FillRgn)(dc, rg, b); U(DeleteObject)(b); } break;
    case OP_PAINTRGN: U(PaintRgn)(dc, rg); break;
    case OP_FRAMERGN: { HBRUSH b = U(CreateSolidBrush)(0xFFFFFF); U(FrameRgn)(dc, rg, b, 3, 3); U(DeleteObject)(b); } break;
    case OP_INVERTRGN: U(InvertRgn)(dc, rg); break;
    case OP_FILLRECT: { RECT r = {1, 1, 63, 63}; U(FillRect)(dc, &r, (HBRUSH)wb); } break;
    case OP_FRAMERECT: { RECT r = {1, 1, 63, 63}; U(FrameRect)(dc, &r, (HBRUSH)wb); } break;
    case OP_PATBLT_PAT: U(PatBlt)(dc, 0, 0, W, H, PATCOPY); break;
    case OP_PATBLT_INV: U(PatBlt)(dc, 0, 0, W, H, DSTINVERT); break;
    case OP_PATBLT_WHITE: U(PatBlt)(dc, 0, 0, W, H, WHITENESS); break;
    case OP_BITBLT: { MDC s; if (mdc_create(&s, 32, 32)) { for (int i = 0; i < 32 * 32; i++) s.bits[i] = (i % 32 < 16) ? 0xFFFFFF : 0x0000FF;
            U(BitBlt)(dc, 7, 9, 32, 32, s.dc, 0, 0, SRCCOPY); mdc_destroy(&s); } } break;
    case OP_STRETCH_UP: { MDC s; if (mdc_create(&s, 8, 8)) { for (int i = 0; i < 64; i++) s.bits[i] = (i % 3) ? 0xFFFFFF : 0x00FF00;
            U(SetStretchBltMode)(dc, COLORONCOLOR); U(StretchBlt)(dc, 3, 3, 58, 58, s.dc, 0, 0, 8, 8, SRCCOPY); mdc_destroy(&s); } } break;
    case OP_STRETCH_DOWN: { MDC s; if (mdc_create(&s, 128, 128)) { for (int i = 0; i < 128 * 128; i++) s.bits[i] = (i % 5) ? 0xFFFFFF : 0x00FF00;
            U(SetStretchBltMode)(dc, COLORONCOLOR); U(StretchBlt)(dc, 2, 2, 60, 60, s.dc, 0, 0, 128, 128, SRCCOPY); mdc_destroy(&s); } } break;
    case OP_STRETCH_FLIP: { MDC s; if (mdc_create(&s, 16, 16)) { for (int i = 0; i < 256; i++) s.bits[i] = (i % 7) ? 0xFFFFFF : 0x00FF00;
            U(SetStretchBltMode)(dc, COLORONCOLOR); U(StretchBlt)(dc, 60, 60, -50, -50, s.dc, 0, 0, 16, 16, SRCCOPY); mdc_destroy(&s); } } break;
    case OP_SETDIBITS: case OP_STRETCHDIBITS: {
        BITMAPINFO bi; memset(&bi, 0, sizeof bi);
        bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 32; bi.bmiHeader.biHeight = -32;
        bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
        for (int i = 0; i < 32 * 32; i++) g_src[i] = ((i / 32 + i % 32) & 1) ? 0xFFFFFF : 0x0000FF;
        if (op == OP_SETDIBITS) U(SetDIBitsToDevice)(dc, 5, 8, 32, 32, 0, 0, 0, 32, g_src, &bi, DIB_RGB_COLORS);
        else { U(SetStretchBltMode)(dc, COLORONCOLOR); U(StretchDIBits)(dc, 2, 2, 60, 60, 0, 0, 32, 32, g_src, &bi, DIB_RGB_COLORS, SRCCOPY); }
    } break;
    case OP_SETPIXEL: for (int i = 0; i < 64; i += 2) U(SetPixelV)(dc, i, (i * 7) % 64, 0xFFFFFF); break;
    case OP_FLOODFILL: { U(Rectangle)(dc, 1, 1, 62, 62); HBRUSH b = U(CreateSolidBrush)(0x00FF00); HGDIOBJ o = U(SelectObject)(dc, b);
        U(ExtFloodFill)(dc, 30, 30, 0xFFFFFF, FLOODFILLBORDER); U(SelectObject)(dc, o); U(DeleteObject)(b); } break;
    case OP_EXTTEXT_OPAQUE: { RECT r = {3, 5, 61, 57}; U(SetBkColor)(dc, 0x123456); U(ExtTextOutA)(dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL); } break;
    case OP_TEXTOUT: { HGDIOBJ f = U(GetStockObject)(SYSTEM_FONT); HGDIOBJ of = U(SelectObject)(dc, f);
        U(TextOutA)(dc, 2, 2, "WWWWWWWWWW", 10); U(TextOutA)(dc, 2, 30, "iiiiiiiiii", 10); U(SelectObject)(dc, of); } break;
    case OP_EXTTEXT_CLIPPED: { HGDIOBJ f = U(GetStockObject)(SYSTEM_FONT); HGDIOBJ of = U(SelectObject)(dc, f);
        RECT r = {10, 10, 50, 30}; U(ExtTextOutA)(dc, 6, 12, ETO_OPAQUE | ETO_CLIPPED, &r, "MMMMMMMM", 8, NULL); U(SelectObject)(dc, of); } break;
    case OP_GRADIENT: { UN(GdiGradientFill); TRIVERTEX v[2] = { {0, 0, 0xFF00, 0x8000, 0x4000, 0}, {64, 64, 0x2000, 0xFF00, 0xFF00, 0} };
        GRADIENT_RECT gr = {0, 1}; GdiGradientFill_pfn f = UN(GdiGradientFill); if (f) f(dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H); } break;
    case OP_ALPHABLEND: { GdiAlphaBlend_pfn f = UN(GdiAlphaBlend); MDC s; if (f && mdc_create(&s, 32, 32)) { for (int i = 0; i < 1024; i++) s.bits[i] = 0xFF0000FF;
            BLENDFUNCTION bf = { AC_SRC_OVER, 0, 128, 0 }; f(dc, 5, 5, 50, 50, s.dc, 0, 0, 32, 32, bf); mdc_destroy(&s); } } break;
    case OP_TRANSPARENT: { GdiTransparentBlt_pfn f = UN(GdiTransparentBlt); MDC s; if (f && mdc_create(&s, 32, 32)) { for (int i = 0; i < 1024; i++) s.bits[i] = (i % 32 < 10) ? 0 : 0xFFFFFF;
            f(dc, 4, 4, 56, 56, s.dc, 0, 0, 32, 32, 0); mdc_destroy(&s); } } break;
    case OP_POLYPOLYGON: { POINT p[] = {{2, 2}, {30, 2}, {30, 30}, {2, 30}, {34, 34}, {60, 34}, {47, 60}}; INT c[2] = {4, 3}; U(PolyPolygon)(dc, p, c, 2); } break;
    case OP_POLYBEZIERTO: { POINT p[] = {{60, 2}, {2, 30}, {60, 60}}; U(MoveToEx)(dc, 2, 2, NULL); U(PolyBezierTo)(dc, p, 3); } break;
    case OP_ANGLEARC: U(MoveToEx)(dc, 32, 32, NULL); U(AngleArc)(dc, 32, 32, 28, 0.0f, 270.0f); break;
    }
    U(DeleteObject)(rg);
    U(SelectObject)(dc, op0); U(SelectObject)(dc, ob0);
}
static HRGN make_clip(int kind) {
    switch (kind) {
    case 0: return rr(10, 10, 50, 40);
    case 1: { HRGN a = rr(0, 0, W, H), b = rr(20, 20, 44, 44); U(CombineRgn)(a, a, b, RGN_DIFF); U(DeleteObject)(b); return a; }
    case 2: return U(CreateEllipticRgn)(6, 6, 58, 58);
    default: { HRGN a = rr(0, 0, 20, 64), b = rr(30, 5, 64, 30); U(CombineRgn)(a, a, b, RGN_OR); U(DeleteObject)(b); return a; }
    }
}
static void run_op(int op) {
    REQUIRE(U(GetStockObject)); REQUIRE(U(SelectObject)); REQUIRE(U(Rectangle)); REQUIRE(U(Ellipse)); REQUIRE(U(RoundRect));
    REQUIRE(U(Polygon)); REQUIRE(U(Polyline)); REQUIRE(U(PolyBezier)); REQUIRE(U(Arc)); REQUIRE(U(Pie)); REQUIRE(U(Chord));
    REQUIRE(U(MoveToEx)); REQUIRE(U(LineTo)); REQUIRE(U(CreatePen)); REQUIRE(U(CreateSolidBrush)); REQUIRE(U(FillRgn));
    REQUIRE(U(PaintRgn)); REQUIRE(U(FrameRgn)); REQUIRE(U(InvertRgn)); REQUIRE(U(FillRect)); REQUIRE(U(FrameRect));
    REQUIRE(U(BitBlt)); REQUIRE(U(StretchBlt)); REQUIRE(U(SetStretchBltMode)); REQUIRE(U(SetDIBitsToDevice)); REQUIRE(U(StretchDIBits));
    REQUIRE(U(SetPixelV)); REQUIRE(U(ExtFloodFill)); REQUIRE(U(ExtTextOutA)); REQUIRE(U(TextOutA)); REQUIRE(U(SetBkMode));
    REQUIRE(U(SetBkColor)); REQUIRE(U(SetTextColor)); REQUIRE(U(CreateEllipticRgn)); REQUIRE(U(PolyPolygon)); REQUIRE(U(PolyBezierTo)); REQUIRE(U(AngleArc));
    NEED_CLIP();
    if (op == OP_GRADIENT) REQUIRE(UN(GdiGradientFill));
    if (op == OP_ALPHABLEND) REQUIRE(UN(GdiAlphaBlend));
    if (op == OP_TRANSPARENT) REQUIRE(UN(GdiTransparentBlt));
    if (clip_is_wine() && (op == OP_GRADIENT || op == OP_ALPHABLEND || op == OP_TRANSPARENT)) { /* run anyway; guarded by REQUIRE */ }
    MDC ref; CHECK(mdc_create(&ref, W, H)); if (!ref.dc) return;
    do_op(op, &ref);
    int total = 0; for (int i = 0; i < W * H; i++) if (ref.bits[i] & 0xFFFFFF) total++;
    CHECKF(total > 0 || op == OP_FLOODFILL, "op %d drew nothing", op);
    for (int kind = 0; kind < 4; kind++) {
        MDC m; CHECK(mdc_create(&m, W, H)); if (!m.dc) break;
        HRGN c = make_clip(kind);
        U(SelectClipRgn)(m.dc, c);
        do_op(op, &m);
        int bad = 0, inside = 0;
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            DWORD e = U(PtInRegion)(c, x, y) ? (ref.bits[y * W + x] & 0xFFFFFF) : 0;
            DWORD g = m.bits[y * W + x] & 0xFFFFFF;
            if (e != g) bad++;
            if (g) inside++;
        }
        CHECKF(bad == 0, "op %d clip kind %d: %d mismatching pixels (drawn %d)", op, kind, bad, inside);
        U(DeleteObject)(c);
        mdc_destroy(&m);
    }
    mdc_destroy(&ref);
}
#define OPTEST(nm, op) TEST(clipping_draw_##nm) { run_op(op); }
OPTEST(rectangle, OP_RECTANGLE) OPTEST(ellipse, OP_ELLIPSE) OPTEST(roundrect, OP_ROUNDRECT) OPTEST(polygon, OP_POLYGON)
OPTEST(polyline, OP_POLYLINE) OPTEST(polybezier, OP_BEZIER) OPTEST(arc, OP_ARC) OPTEST(pie, OP_PIE) OPTEST(chord, OP_CHORD)
OPTEST(lineto, OP_LINETO) OPTEST(line_thick, OP_LINE_THICK) OPTEST(fillrgn, OP_FILLRGN) OPTEST(paintrgn, OP_PAINTRGN)
OPTEST(framergn, OP_FRAMERGN) OPTEST(invertrgn, OP_INVERTRGN) OPTEST(fillrect, OP_FILLRECT) OPTEST(framerect, OP_FRAMERECT)
OPTEST(patblt_patcopy, OP_PATBLT_PAT) OPTEST(patblt_dstinvert, OP_PATBLT_INV) OPTEST(patblt_whiteness, OP_PATBLT_WHITE)
OPTEST(bitblt_srccopy, OP_BITBLT) OPTEST(stretchblt_up, OP_STRETCH_UP) OPTEST(stretchblt_down, OP_STRETCH_DOWN)
OPTEST(stretchblt_flip, OP_STRETCH_FLIP) OPTEST(setdibitstodevice, OP_SETDIBITS) OPTEST(stretchdibits, OP_STRETCHDIBITS)
OPTEST(setpixelv, OP_SETPIXEL) OPTEST(floodfill, OP_FLOODFILL) OPTEST(exttextout_opaque, OP_EXTTEXT_OPAQUE)
OPTEST(textout, OP_TEXTOUT) OPTEST(exttextout_etoclipped, OP_EXTTEXT_CLIPPED) OPTEST(gradientfill, OP_GRADIENT)
OPTEST(alphablend, OP_ALPHABLEND) OPTEST(transparentblt, OP_TRANSPARENT) OPTEST(polypolygon, OP_POLYPOLYGON)
OPTEST(polybezierto, OP_POLYBEZIERTO) OPTEST(anglearc, OP_ANGLEARC)

/* ---- pixel specifics ---------------------------------------------------- */
TEST(clipping_draw_patblt_rect_count) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 100, 100)); if (!m.dc) return;
    U(IntersectClipRect)(m.dc, 10, 20, 60, 70);
    U(PatBlt)(m.dc, 0, 0, 100, 100, WHITENESS);
    EXPECT_EQ(mdc_count(&m, 0xFFFFFF), 50 * 50);
    CHECK(mdc_px(&m, 10, 20) == 0xFFFFFF && mdc_px(&m, 59, 69) == 0xFFFFFF);
    CHECK(mdc_px(&m, 9, 20) == 0 && mdc_px(&m, 60, 69) == 0 && mdc_px(&m, 59, 70) == 0 && mdc_px(&m, 10, 19) == 0);
    mdc_destroy(&m);
}
TEST(clipping_draw_empty_clip_draws_nothing) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 32, 32)); if (!m.dc) return;
    HRGN e = rr(0, 0, 0, 0); U(SelectClipRgn)(m.dc, e);
    U(PatBlt)(m.dc, 0, 0, 32, 32, WHITENESS);
    EXPECT_EQ(mdc_count(&m, 0xFFFFFF), 0);
    U(DeleteObject)(e); mdc_destroy(&m);
}
TEST(clipping_draw_nullregion_via_exclude_all) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 32, 32)); if (!m.dc) return;
    EXPECT_EQ(U(ExcludeClipRect)(m.dc, 0, 0, 32, 32), NULLREGION);
    U(PatBlt)(m.dc, 0, 0, 32, 32, WHITENESS);
    EXPECT_EQ(mdc_count(&m, 0xFFFFFF), 0);
    U(SelectClipRgn)(m.dc, NULL);
    U(PatBlt)(m.dc, 0, 0, 32, 32, WHITENESS);
    EXPECT_EQ(mdc_count(&m, 0xFFFFFF), 32 * 32);
    mdc_destroy(&m);
}
TEST(clipping_draw_clip_bigger_than_surface_no_overrun) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 16, 16)); if (!m.dc) return;
    HRGN s = rr(-1000, -1000, 1000, 1000); U(SelectClipRgn)(m.dc, s);
    U(PatBlt)(m.dc, -50, -50, 200, 200, WHITENESS);
    EXPECT_EQ(mdc_count(&m, 0xFFFFFF), 256);
    U(DeleteObject)(s); mdc_destroy(&m);
}
TEST(clipping_draw_getpixel_outside_clip) {
    NEED_CLIP(); REQUIRE(U(GetPixel)); MDC m; CHECK(mdc_create(&m, 32, 32)); if (!m.dc) return;
    mdc_clear(&m, 0x00112233);
    U(IntersectClipRect)(m.dc, 8, 8, 16, 16);
    COLORREF in = U(GetPixel)(m.dc, 10, 10), out = U(GetPixel)(m.dc, 2, 2), off = U(GetPixel)(m.dc, 40, 40);
    t_note("GetPixel inside=%lx outside-clip=%lx off-surface=%lx", (unsigned long)in, (unsigned long)out, (unsigned long)off);
    CHECK(in == 0x00332211);
    CHECK(off == CLR_INVALID);
    mdc_destroy(&m);
}
TEST(clipping_draw_setpixel_outside_clip) {
    NEED_CLIP(); REQUIRE(U(SetPixel)); MDC m; CHECK(mdc_create(&m, 32, 32)); if (!m.dc) return;
    U(IntersectClipRect)(m.dc, 8, 8, 16, 16);
    COLORREF r1 = U(SetPixel)(m.dc, 10, 10, 0xFFFFFF), r2 = U(SetPixel)(m.dc, 2, 2, 0xFFFFFF);
    t_note("SetPixel inside ret=%lx outside ret=%lx", (unsigned long)r1, (unsigned long)r2);
    CHECK(r1 != CLR_INVALID);
    CHECK(mdc_px(&m, 10, 10) == 0xFFFFFF && mdc_px(&m, 2, 2) == 0);
    mdc_destroy(&m);
}
TEST(clipping_draw_line_endpoints_clip_edge) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 32, 32)); if (!m.dc) return;
    U(SelectObject)(m.dc, U(GetStockObject)(WHITE_PEN));
    U(IntersectClipRect)(m.dc, 4, 4, 12, 12);
    U(MoveToEx)(m.dc, 0, 8, NULL); U(LineTo)(m.dc, 31, 8);
    RECT cr = {4, 4, 12, 12};
    EXPECT_EQ(mdc_outside(&m, &cr), 0);
    EXPECT_EQ(mdc_count(&m, 0xFFFFFF), 8);
    mdc_destroy(&m);
}
TEST(clipping_draw_bitblt_source_clip_ignored_note) {
    NEED_CLIP(); MDC d, s; CHECK(mdc_create(&d, 32, 32)); CHECK(mdc_create(&s, 32, 32)); if (!d.dc || !s.dc) return;
    mdc_clear(&s, 0xFFFFFF);
    U(IntersectClipRect)(s.dc, 0, 0, 8, 8);                 /* clip on the SOURCE dc */
    U(BitBlt)(d.dc, 0, 0, 32, 32, s.dc, 0, 0, SRCCOPY);
    int n = mdc_count(&d, 0xFFFFFF);
    t_note("source-clip blit copied %d of 1024 px", n);
    CHECK(n == 1024 || n == 64);
    mdc_destroy(&d); mdc_destroy(&s);
}
TEST(clipping_draw_bitblt_dest_clip_only) {
    NEED_CLIP(); MDC d, s; CHECK(mdc_create(&d, 32, 32)); CHECK(mdc_create(&s, 32, 32)); if (!d.dc || !s.dc) return;
    mdc_clear(&s, 0xFFFFFF);
    U(IntersectClipRect)(d.dc, 5, 5, 15, 15);
    CHECK(U(BitBlt)(d.dc, 0, 0, 32, 32, s.dc, 0, 0, SRCCOPY));
    EXPECT_EQ(mdc_count(&d, 0xFFFFFF), 100);
    mdc_destroy(&d); mdc_destroy(&s);
}
TEST(clipping_draw_bitblt_empty_clip_returns) {
    NEED_CLIP(); MDC d, s; CHECK(mdc_create(&d, 32, 32)); CHECK(mdc_create(&s, 32, 32)); if (!d.dc || !s.dc) return;
    mdc_clear(&s, 0xFFFFFF);
    HRGN e = rr(0, 0, 0, 0); U(SelectClipRgn)(d.dc, e);
    BOOL ok = U(BitBlt)(d.dc, 0, 0, 32, 32, s.dc, 0, 0, SRCCOPY);
    t_note("BitBlt with empty clip returns %d", ok);
    EXPECT_EQ(mdc_count(&d, 0xFFFFFF), 0);
    U(DeleteObject)(e); mdc_destroy(&d); mdc_destroy(&s);
}
TEST(clipping_draw_self_blit_clipped) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 32, 32)); if (!m.dc) return;
    U(PatBlt)(m.dc, 0, 0, 16, 32, WHITENESS);              /* left half white */
    U(IntersectClipRect)(m.dc, 16, 0, 24, 32);
    CHECK(U(BitBlt)(m.dc, 16, 0, 16, 32, m.dc, 0, 0, SRCCOPY));
    RECT cr = {16, 0, 24, 32};
    EXPECT_EQ(mdc_inside(&m, &cr), 8 * 32);
    EXPECT_EQ(mdc_count(&m, 0xFFFFFF), 16 * 32 + 8 * 32);
    mdc_destroy(&m);
}
TEST(clipping_draw_clip_applied_per_call_not_cached) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 32, 32)); if (!m.dc) return;
    for (int i = 0; i < 16; i++) {
        mdc_clear(&m, 0);
        U(SelectClipRgn)(m.dc, NULL);
        U(IntersectClipRect)(m.dc, i, i, i + 4, i + 4);
        U(PatBlt)(m.dc, 0, 0, 32, 32, WHITENESS);
        CHECKF(mdc_count(&m, 0xFFFFFF) == 16 && mdc_px(&m, i, i) == 0xFFFFFF, "i=%d", i);
    }
    mdc_destroy(&m);
}

/* ---- different bit depths ---------------------------------------------- */
static int px_nonzero(const BYTE *bits, int stride, int bpp, int x, int y) {
    const BYTE *row = bits + (size_t)y * stride;
    switch (bpp) {
    case 1: return (row[x >> 3] >> (7 - (x & 7))) & 1;
    case 4: return (x & 1) ? (row[x >> 1] & 0xF) != 0 : (row[x >> 1] >> 4) != 0;
    case 8: return row[x] != 0;
    case 16: return *(const WORD *)(row + x * 2) != 0;
    case 24: return row[x * 3] | row[x * 3 + 1] | row[x * 3 + 2];
    default: return *(const DWORD *)(row + x * 4) != 0;
    }
}
static void depth_case(int bpp) {
    NEED_CLIP();
    struct { BITMAPINFOHEADER h; RGBQUAD pal[256]; } bi; memset(&bi, 0, sizeof bi);
    bi.h.biSize = sizeof bi.h; bi.h.biWidth = 37; bi.h.biHeight = -29; bi.h.biPlanes = 1; bi.h.biBitCount = bpp; bi.h.biCompression = BI_RGB;
    for (int i = 1; i < 256; i++) bi.pal[i].rgbRed = bi.pal[i].rgbGreen = bi.pal[i].rgbBlue = 255;
    HDC dc = U(CreateCompatibleDC)(NULL); void *bits = NULL;
    HBITMAP bmp = U(CreateDIBSection)(dc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    CHECK(bmp != NULL); if (!bmp) { U(DeleteDC)(dc); return; }
    HGDIOBJ old = U(SelectObject)(dc, bmp);
    int stride = ((37 * bpp + 31) / 32) * 4;
    memset(bits, 0, (size_t)stride * 29);
    U(IntersectClipRect)(dc, 3, 4, 20, 17);
    U(PatBlt)(dc, 0, 0, 37, 29, WHITENESS);
    int inside = 0, outside = 0;
    for (int y = 0; y < 29; y++) for (int x = 0; x < 37; x++) {
        int nz = px_nonzero(bits, stride, bpp, x, y), in = x >= 3 && x < 20 && y >= 4 && y < 17;
        if (nz && in) inside++; else if (nz) outside++;
    }
    CHECKF(inside == 17 * 13, "bpp %d inside %d", bpp, inside);
    CHECKF(outside == 0, "bpp %d outside %d", bpp, outside);
    RECT r; EXPECT_EQ(U(GetClipBox)(dc, &r), SIMPLEREGION); EXPECT_RECT(r, 3, 4, 20, 17);
    U(SelectObject)(dc, old); U(DeleteObject)(bmp); U(DeleteDC)(dc);
}
#define DEPTH(b) TEST(clipping_depth_##b) { depth_case(b); }
DEPTH(1) DEPTH(4) DEPTH(8) DEPTH(16) DEPTH(24) DEPTH(32)

TEST(clipping_depth_bottom_up_dib) {
    NEED_CLIP(); MDC m; CHECK(mdc_create(&m, 16, 16)); if (!m.dc) return;
    /* bottom-up DIB: clip rect coordinates stay in device (top-left) space */
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 16; bi.bmiHeader.biHeight = 16; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    DWORD *bits; HBITMAP b2 = U(CreateDIBSection)(m.dc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    HGDIOBJ old = U(SelectObject)(m.dc, b2);
    memset(bits, 0, 16 * 16 * 4);
    U(IntersectClipRect)(m.dc, 0, 0, 16, 4);               /* top four rows */
    U(PatBlt)(m.dc, 0, 0, 16, 16, WHITENESS);
    int top = 0, bottom = 0;
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) if (bits[y * 16 + x] & 0xFFFFFF) { if (y >= 12) top++; else bottom++; }
    EXPECT_EQ(top, 64); EXPECT_EQ(bottom, 0);               /* row 12..15 in memory == top rows on screen */
    U(SelectObject)(m.dc, old); U(DeleteObject)(b2); mdc_destroy(&m);
}
TEST(clipping_depth_monochrome_ddb_memdc) {
    NEED_CLIP(); REQUIRE(U(CreateBitmap)); REQUIRE(U(GetPixel));
    HDC dc = U(CreateCompatibleDC)(NULL); HBITMAP b = U(CreateBitmap)(20, 20, 1, 1, NULL);
    HGDIOBJ old = U(SelectObject)(dc, b);
    RECT r; EXPECT_EQ(U(GetClipBox)(dc, &r), SIMPLEREGION); EXPECT_RECT(r, 0, 0, 20, 20);
    U(PatBlt)(dc, 0, 0, 20, 20, BLACKNESS);
    U(IntersectClipRect)(dc, 5, 5, 10, 10);
    U(PatBlt)(dc, 0, 0, 20, 20, WHITENESS);
    int white = 0;
    for (int y = 0; y < 20; y++) for (int x = 0; x < 20; x++) if (U(GetPixel)(dc, x, y) == 0xFFFFFF) white++;
    EXPECT_EQ(white, 25);
    U(SelectObject)(dc, old); U(DeleteObject)(b); U(DeleteDC)(dc);
}
TEST(clipping_depth_compatible_bitmap_clipbox) {
    NEED_CLIP(); REQUIRE(U(CreateCompatibleBitmap)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC scr = U(GetDC)(NULL); if (!scr) { t_skip("no screen dc"); return; }
    HBITMAP b = U(CreateCompatibleBitmap)(scr, 77, 33);
    HDC dc = U(CreateCompatibleDC)(scr); HGDIOBJ old = U(SelectObject)(dc, b);
    RECT r; EXPECT_EQ(U(GetClipBox)(dc, &r), SIMPLEREGION); EXPECT_RECT(r, 0, 0, 77, 33);
    U(SelectObject)(dc, old); U(DeleteObject)(b); U(DeleteDC)(dc); U(ReleaseDC)(NULL, scr);
}
