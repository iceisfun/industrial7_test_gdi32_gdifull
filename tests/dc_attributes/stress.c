/* Attributes must survive unrelated DC operations; loops; concurrent threads each using a private DC. */
#include "dca.h"

static void set_all_nondef(HDC d, DWORD *want) {
    for (int i = 0; i < dca_nattrs; i++) {
        const DcaAttr *a = &dca_attrs[i];
        want[i] = a->avail() ? dca_nondef(a) : 0;
        if (a->avail()) a->set(d, want[i]);
    }
}
static int count_changed(HDC d, const DWORD *want, const char *what) {
    int bad = 0;
    for (int i = 0; i < dca_nattrs; i++) {
        const DcaAttr *a = &dca_attrs[i];
        if (!a->avail()) continue;
        DWORD g = a->get(d);
        if (g != want[i]) {
            bad++;
            if (a->rt_savedc) CHECKF(0, "%s changed by %s: 0x%lx want 0x%lx", a->name, what, g, want[i]);
            else t_note("%s changed by %s: 0x%lx want 0x%lx", a->name, what, g, want[i]);
        }
    }
    return bad;
}
#define OPTEST(nm, ...) TEST(dca_keep_##nm) { \
    MDC m; if (!mdc_make(&m, 64, 64)) { t_skip("mdc"); return; } \
    DWORD want[32]; set_all_nondef(m.dc, want); \
    { __VA_ARGS__ } \
    count_changed(m.dc, want, #nm); CHECK(1); \
    mdc_free(&m); }

OPTEST(patblt, { REQUIRE(U(PatBlt)); U(PatBlt)(m.dc, 0, 0, 32, 32, BLACKNESS); U(PatBlt)(m.dc, 4, 4, 8, 8, DSTINVERT); })
OPTEST(rectangle, { REQUIRE(U(Rectangle)); U(Rectangle)(m.dc, 2, 2, 40, 40); U(Ellipse)(m.dc, 2, 2, 40, 40); U(RoundRect)(m.dc, 2, 2, 40, 40, 5, 5); })
OPTEST(lines, { REQUIRE(U(LineTo)); U(MoveToEx)(m.dc, 1, 1, NULL); U(LineTo)(m.dc, 50, 20); U(LineTo)(m.dc, 5, 60); })
OPTEST(textout, { REQUIRE(U(TextOutA)); U(TextOutA)(m.dc, 2, 2, "Hello world", 11); U(ExtTextOutA)(m.dc, 2, 20, ETO_OPAQUE, NULL, "abc", 3, NULL); })
OPTEST(bitblt, { REQUIRE(U(BitBlt)); MDC s; if (mdc_make(&s, 16, 16)) { U(BitBlt)(m.dc, 0, 0, 16, 16, s.dc, 0, 0, SRCCOPY); U(StretchBlt)(m.dc, 0, 0, 32, 32, s.dc, 0, 0, 16, 16, SRCCOPY); mdc_free(&s); } })
OPTEST(setpixel, { REQUIRE(U(SetPixel)); U(SetPixel)(m.dc, 3, 3, 0x123456); U(SetPixelV)(m.dc, 4, 4, 0x654321); U(GetPixel)(m.dc, 3, 3); })
OPTEST(clipping, { REQUIRE(U(IntersectClipRect)); U(IntersectClipRect)(m.dc, 5, 5, 30, 30); U(ExcludeClipRect)(m.dc, 10, 10, 12, 12); U(SelectClipRgn)(m.dc, NULL); })
OPTEST(mapmodes, {
    REQUIRE(U(SetMapMode));
    static const int mm[] = { MM_LOMETRIC, MM_HIMETRIC, MM_LOENGLISH, MM_HIENGLISH, MM_TWIPS, MM_ISOTROPIC, MM_ANISOTROPIC, MM_TEXT };
    for (int i = 0; i < 8; i++) { U(SetMapMode)(m.dc, mm[i]); count_changed(m.dc, want, "SetMapMode"); }
})
OPTEST(viewport, { REQUIRE(U(SetViewportOrgEx)); U(SetViewportOrgEx)(m.dc, 7, 9, NULL); U(SetWindowOrgEx)(m.dc, 3, 4, NULL); U(OffsetViewportOrgEx)(m.dc, 1, 1, NULL); U(SetViewportExtEx)(m.dc, 100, 100, NULL); U(SetWindowExtEx)(m.dc, 50, 50, NULL); U(SetViewportOrgEx)(m.dc, 0, 0, NULL); U(SetWindowOrgEx)(m.dc, 0, 0, NULL); })
OPTEST(select_objects, {
    HPEN p = U(CreatePen)(PS_DASH, 1, 255); HBRUSH b = U(CreateHatchBrush)(HS_CROSS, 255); HFONT f = U(CreateFontA)(14, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma");
    HGDIOBJ o1 = U(SelectObject)(m.dc, p), o2 = U(SelectObject)(m.dc, b), o3 = U(SelectObject)(m.dc, f);
    U(SelectObject)(m.dc, o1); U(SelectObject)(m.dc, o2); U(SelectObject)(m.dc, o3);
    U(DeleteObject)(p); U(DeleteObject)(b); U(DeleteObject)(f);
})
OPTEST(path, { REQUIRE(U(BeginPath)); U(BeginPath)(m.dc); U(MoveToEx)(m.dc, 5, 5, NULL); U(LineTo)(m.dc, 30, 5); U(LineTo)(m.dc, 30, 30); U(CloseFigure)(m.dc); U(EndPath)(m.dc); U(FillPath)(m.dc); })
OPTEST(gdiflush, { REQUIRE(U(GdiFlush)); for (int i = 0; i < 10; i++) U(GdiFlush)(); U(GdiSetBatchLimit)(1); U(GdiSetBatchLimit)(0); })
OPTEST(getdevicecaps, { REQUIRE(U(GetDeviceCaps)); for (int i = 0; i < 120; i += 2) U(GetDeviceCaps)(m.dc, i); })
OPTEST(textmetrics, { REQUIRE(U(GetTextMetricsA)); TEXTMETRICA tm; SIZE sz; U(GetTextMetricsA)(m.dc, &tm); U(GetTextExtentPoint32A)(m.dc, "abc", 3, &sz); })

TEST(dca_keep_after_graphics_mode_switch) {
    REQUIRE(U(SetGraphicsMode));
    MDC m; if (!mdc_make(&m, 16, 16)) { t_skip("mdc"); return; }
    DWORD want[32]; set_all_nondef(m.dc, want);
    int idx_gfx = 10;
    U(SetGraphicsMode)(m.dc, GM_COMPATIBLE);
    U(SetGraphicsMode)(m.dc, GM_ADVANCED);
    U(SetGraphicsMode)(m.dc, GM_COMPATIBLE);
    for (int i = 0; i < dca_nattrs; i++) {
        const DcaAttr *a = &dca_attrs[i];
        if (!a->avail() || i == idx_gfx) continue;
        DWORD g = a->get(m.dc);
        if (a->rt_savedc) CHECKF(g == want[i], "%s changed by graphics mode switch: 0x%lx", a->name, g);
        else if (g != want[i]) t_note("%s changed by graphics mode switch: 0x%lx want 0x%lx", a->name, g, want[i]);
    }
    mdc_free(&m);
}
TEST(dca_loop_50000_bkcolor) {
    REQUIRE(U(SetBkColor)); REQUIRE(U(GetBkColor));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    COLORREF last = 0xFFFFFF; int bad = 0;
    for (int i = 0; i < 50000; i++) {
        COLORREF c = (i * 2654435761u) & 0xFFFFFF;
        if (U(SetBkColor)(m.dc, c) != last) bad++;
        last = c;
    }
    CHECKF(bad == 0, "%d mismatches", bad);
    CHECK(U(GetBkColor)(m.dc) == last);
    mdc_free(&m);
}
TEST(dca_loop_alternating_set_get_all_attributes) {
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    int bad = 0;
    for (int round = 0; round < 500; round++) {
        for (int i = 0; i < dca_nattrs; i++) {
            const DcaAttr *a = &dca_attrs[i];
            if (!a->avail()) continue;
            DWORD v = a->vals[round % a->nv];
            a->set(m.dc, v);
            if (a->get(m.dc) != v) bad++;
        }
    }
    CHECKF(bad == 0, "%d mismatches", bad);
    mdc_free(&m);
}
typedef struct { int id, bad, done; } Worker;
static DWORD WINAPI worker(LPVOID p) {
    Worker *w = p;
    HDC d = U(CreateCompatibleDC)(NULL);
    if (!d) { w->bad = -1; return 0; }
    for (int i = 0; i < 3000; i++) {
        COLORREF c = (w->id * 0x111111 + i * 7) & 0xFFFFFF;
        U(SetBkColor)(d, c); U(SetTextColor)(d, ~c & 0xFFFFFF);
        U(SetBkMode)(d, (i & 1) ? TRANSPARENT : OPAQUE);
        U(SetROP2)(d, 1 + (i + w->id) % 16);
        U(SetTextCharacterExtra)(d, i - w->id);
        if (U(GetBkColor)(d) != c || U(GetTextColor)(d) != (~c & 0xFFFFFF) || U(GetBkMode)(d) != ((i & 1) ? TRANSPARENT : OPAQUE) ||
            U(GetROP2)(d) != 1 + (i + w->id) % 16 || U(GetTextCharacterExtra)(d) != i - w->id) w->bad++;
    }
    U(DeleteDC)(d);
    w->done = 1;
    return 0;
}
TEST(dca_threads_private_dcs_no_crosstalk) {
    REQUIRE(U(SetBkColor)); REQUIRE(U(SetTextColor)); REQUIRE(U(SetBkMode)); REQUIRE(U(SetROP2)); REQUIRE(U(SetTextCharacterExtra));
    enum { N = 4 };
    Worker w[N]; HANDLE h[N]; memset(w, 0, sizeof w);
    DWORD before = dca_gui();
    for (int i = 0; i < N; i++) { w[i].id = i + 1; h[i] = CreateThread(NULL, 0, worker, &w[i], 0, NULL); }
    for (int i = 0; i < N; i++) if (h[i]) { WaitForSingleObject(h[i], 60000); CloseHandle(h[i]); }
    for (int i = 0; i < N; i++) CHECKF(w[i].done && w[i].bad == 0, "worker %d done=%d bad=%d", i, w[i].done, w[i].bad);
    CHECKF(dca_gui() == before, "GDI objects %lu -> %lu", before, dca_gui());
}
typedef struct { HDC dc; int ok; } Shared;
static DWORD WINAPI shared_reader(LPVOID p) {
    Shared *s = p;
    s->ok = (U(GetBkColor)(s->dc) == 0x123456 && U(GetTextColor)(s->dc) == 0x654321 && U(GetBkMode)(s->dc) == TRANSPARENT && U(GetROP2)(s->dc) == R2_XORPEN);
    U(SetBkColor)(s->dc, 0x0A0B0C);
    return 0;
}
TEST(dca_threads_dc_shared_between_threads) {
    REQUIRE(U(SetBkColor)); REQUIRE(U(SetTextColor)); REQUIRE(U(SetBkMode)); REQUIRE(U(SetROP2));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    U(SetBkColor)(m.dc, 0x123456); U(SetTextColor)(m.dc, 0x654321); U(SetBkMode)(m.dc, TRANSPARENT); U(SetROP2)(m.dc, R2_XORPEN);
    Shared s = { m.dc, 0 };
    HANDLE t = CreateThread(NULL, 0, shared_reader, &s, 0, NULL);
    if (t) { WaitForSingleObject(t, 10000); CloseHandle(t); }
    CHECK(s.ok);
    CHECK(U(GetBkColor)(m.dc) == 0x0A0B0C);
    mdc_free(&m);
}
TEST(dca_text_charextra_logical_units) {
    REQUIRE(U(SetTextCharacterExtra)); REQUIRE(U(GetTextCharacterExtra)); REQUIRE(U(SetMapMode));
    MDC m; if (!mdc_make(&m, 8, 8)) { t_skip("mdc"); return; }
    static const int mm[] = { MM_TEXT, MM_LOMETRIC, MM_HIMETRIC, MM_TWIPS };
    for (int i = 0; i < 4; i++) {
        U(SetMapMode)(m.dc, mm[i]);
        U(SetTextCharacterExtra)(m.dc, 25);
        CHECKF(U(GetTextCharacterExtra)(m.dc) == 25, "mode %d: %d", mm[i], U(GetTextCharacterExtra)(m.dc));
        U(SetTextCharacterExtra)(m.dc, 0);
    }
    mdc_free(&m);
}
TEST(dca_note_display_environment) {
    REQUIRE(U(GetDeviceCaps));
    HDC s = dca_screen(); if (!s) { t_skip("no screen"); return; }
    t_note("screen %dx%d, %d bpp, %d dpi, %d planes", U(GetDeviceCaps)(s, HORZRES), U(GetDeviceCaps)(s, VERTRES), U(GetDeviceCaps)(s, BITSPIXEL), U(GetDeviceCaps)(s, LOGPIXELSX), U(GetDeviceCaps)(s, PLANES));
    dca_unscreen(s);
    CHECK(1);
}
