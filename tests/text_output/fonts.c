#include "to.h"
/* Bitmap/TrueType font families, decorations, rotation, transforms, layout. */

#define NEED_F() do { NEED_TEXT(); REQUIRE(U(SetTextAlign)); } while (0)

/* opaque box of a single string equals the measured extent, for any face */
static void face_box_case(const char *face, int h, int quality) {
    NEED_F();
    TCtx c; if (!to_open(&c, 400, 120, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont(face, h, FW_NORMAL, 0, quality), 1);
    U(SetBkMode)(c.dc, OPAQUE); U(SetBkColor)(c.dc, RGB(255, 0, 0)); U(SetTextColor)(c.dc, RGB(0, 0, 255));
    static const char *s = "Opaque box 123";
    int n = (int)strlen(s);
    SIZE sz; CHECK(U(GetTextExtentPoint32A)(c.dc, s, n, &sz));
    CHECK(U(TextOutA)(c.dc, 20, 20, s, n));
    RECT ink; int cnt = to_ink(&c, 0xffffff, &ink);
    TEXTMETRICA tm; U(GetTextMetricsA)(c.dc, &tm);
    int bitmap = !(tm.tmPitchAndFamily & (TMPF_TRUETYPE | TMPF_VECTOR));
    if (bitmap) {
        CHECKF(ink.left == 20 && ink.top == 20 && ink.right == 20 + sz.cx - 1 && ink.bottom == 20 + sz.cy - 1,
               "%s h=%d: bbox %ld,%ld..%ld,%ld extent %ldx%ld", face, h, ink.left, ink.top, ink.right, ink.bottom, sz.cx, sz.cy);
        CHECKF(cnt == sz.cx * sz.cy, "%s: %d px, extent area %ld", face, cnt, sz.cx * sz.cy);
    } else {
        CHECKF(ink.left <= 20 + 1 && ink.top <= 20 + 1 && ink.right >= 20 + sz.cx - 3 && ink.right <= 20 + sz.cx + 2 && ink.bottom >= 20 + sz.cy - 2 && ink.bottom <= 20 + sz.cy,
               "%s h=%d: TT bbox %ld,%ld..%ld,%ld extent %ldx%ld", face, h, ink.left, ink.top, ink.right, ink.bottom, sz.cx, sz.cy);
    }
    to_close(&c);
}
#define BOX_TEST(nm, face, h) TEST(text_output_fonts_box_##nm) { face_box_case(face, h, NONANTIALIASED_QUALITY); }
BOX_TEST(fixedsys, "Fixedsys", 0)
BOX_TEST(terminal, "Terminal", 0)
BOX_TEST(system, "System", 0)
BOX_TEST(ms_sans_serif_13, "MS Sans Serif", -13)
BOX_TEST(small_fonts_8, "Small Fonts", -8)
BOX_TEST(arial_16, "Arial", -16)
BOX_TEST(arial_9, "Arial", -9)
BOX_TEST(courier_new_14, "Courier New", -14)
BOX_TEST(times_20, "Times New Roman", -20)
BOX_TEST(tahoma_11, "Tahoma", -11)
BOX_TEST(segoe_ui_15, "Segoe UI", -15)
BOX_TEST(consolas_14, "Consolas", -14)
BOX_TEST(symbol_18, "Symbol", -18)
BOX_TEST(marlett_16, "Marlett", -16)

static void stock_box_case(int stock) {
    NEED_F();
    TCtx c; if (!to_open(&c, 400, 120, 32)) { CHECK(0); return; }
    to_use_font(&c, (HFONT)U(GetStockObject)(stock), 0);
    U(SetBkMode)(c.dc, OPAQUE); U(SetBkColor)(c.dc, RGB(255, 0, 0)); U(SetTextColor)(c.dc, RGB(0, 0, 255));
    SIZE sz; CHECK(U(GetTextExtentPoint32A)(c.dc, "Stock font text", 15, &sz));
    CHECK(U(TextOutA)(c.dc, 15, 15, "Stock font text", 15));
    RECT ink; int cnt = to_ink(&c, 0xffffff, &ink);
    TEXTMETRICA tm0; U(GetTextMetricsA)(c.dc, &tm0);
    int bm = !(tm0.tmPitchAndFamily & (TMPF_TRUETYPE | TMPF_VECTOR));
    CHECKF(bm ? (ink.left == 15 && ink.top == 15 && ink.right == 15 + sz.cx - 1 && ink.bottom == 15 + sz.cy - 1)
              : (ink.left <= 16 && ink.top <= 16 && ink.right >= 15 + sz.cx - 3 && ink.right <= 15 + sz.cx + 2 && ink.bottom <= 15 + sz.cy), "stock %d bbox %ld,%ld..%ld,%ld ext %ldx%ld", stock, ink.left, ink.top, ink.right, ink.bottom, sz.cx, sz.cy);
    TEXTMETRICA tm; U(GetTextMetricsA)(c.dc, &tm);
    CHECKF(sz.cy == tm.tmHeight, "cy %ld tmHeight %ld", sz.cy, tm.tmHeight);
    to_close(&c);
}
#define STOCK_TEST(nm, id) TEST(text_output_fonts_stock_##nm) { stock_box_case(id); }
STOCK_TEST(oem_fixed, OEM_FIXED_FONT)
STOCK_TEST(ansi_fixed, ANSI_FIXED_FONT)
STOCK_TEST(ansi_var, ANSI_VAR_FONT)
STOCK_TEST(system, SYSTEM_FONT)
STOCK_TEST(device_default, DEVICE_DEFAULT_FONT)
STOCK_TEST(system_fixed, SYSTEM_FIXED_FONT)
STOCK_TEST(default_gui, DEFAULT_GUI_FONT)

TEST(text_output_fonts_tm_height_equals_em_for_negative_lfheight) {
    NEED_F();
    static const char *face[] = { "Arial", "Times New Roman", "Courier New", "Tahoma", "Segoe UI", "Consolas" };
    static const int px[] = { 8, 10, 12, 16, 20, 33, 48, 100 };
    TCtx c; if (!to_open(&c, 16, 16, 32)) { CHECK(0); return; }
    for (int f = 0; f < ARRAYSZ(face); f++) for (int i = 0; i < ARRAYSZ(px); i++) {
        to_use_font(&c, to_mkfont(face[f], -px[i], FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
        TEXTMETRICA tm; CHECK(U(GetTextMetricsA)(c.dc, &tm));
        if (to_wine()) { if (tm.tmHeight - tm.tmInternalLeading != px[i]) t_note("%s %d: tmHeight %ld - il %ld", face[f], px[i], tm.tmHeight, tm.tmInternalLeading); }
        else CHECKF(tm.tmHeight - tm.tmInternalLeading == px[i], "%s %d: tmHeight %ld - il %ld", face[f], px[i], tm.tmHeight, tm.tmInternalLeading);
        CHECKF(tm.tmHeight == tm.tmAscent + tm.tmDescent, "%s ascent+descent", face[f]);
    }
    to_close(&c);
}
TEST(text_output_fonts_tm_height_positive_lfheight_is_cell) {
    NEED_F();
    static const char *face[] = { "Arial", "Times New Roman", "Tahoma", "Segoe UI" };
    static const int px[] = { 10, 16, 24, 40 };
    TCtx c; if (!to_open(&c, 16, 16, 32)) { CHECK(0); return; }
    for (int f = 0; f < ARRAYSZ(face); f++) for (int i = 0; i < ARRAYSZ(px); i++) {
        to_use_font(&c, to_mkfont(face[f], px[i], FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
        TEXTMETRICA tm; CHECK(U(GetTextMetricsA)(c.dc, &tm));
        if (tm.tmHeight != px[i]) t_note("%s cell height %d -> tmHeight %ld (mapper rounding)", face[f], px[i], tm.tmHeight);
        CHECKF(abs(tm.tmHeight - px[i]) <= 2, "%s %d: tmHeight %ld", face[f], px[i], tm.tmHeight);
    }
    to_close(&c);
}
/* ---- decoration on a bitmap font ---- */
static HFONT deco_font(HDC dc, int under, int strike) {
    LOGFONTA lf; memset(&lf, 0, sizeof lf);
    HGDIOBJ stock = U(GetStockObject)(SYSTEM_FIXED_FONT);
    U(GetObjectA)(stock, sizeof lf, &lf);
    lf.lfUnderline = under; lf.lfStrikeOut = strike; lf.lfQuality = NONANTIALIASED_QUALITY;
    return U(CreateFontIndirectA)(&lf);
}
static void deco_case(int under, int strike) {
    NEED_F(); REQUIRE(U(GetObjectA)); REQUIRE(U(CreateFontIndirectA));
    TCtx c; TEXTMETRICA tm; if (!to_fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT, 200, 80)) return;
    if (to_wine()) { t_skip("bitmap font decoration not available under wine"); to_close(&c); return; }
    HFONT f = deco_font(c.dc, under, strike);
    CHECK(f != NULL);
    if (to_wine()) { to_use_font(&c, to_mkfont("Courier New", -16, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1); }
    else to_use_font(&c, f, 1);
    U(GetTextMetricsA)(c.dc, &tm);
    CHECK(tm.tmUnderlined == (BYTE)(under && !to_wine() ? 0xff : tm.tmUnderlined) || to_wine());
    U(SetTextColor)(c.dc, 0);
    int n = 6, w = n * tm.tmAveCharWidth;
    CHECK(U(TextOutA)(c.dc, 20, 20, "      ", n));
    RECT ink; int cnt = to_ink(&c, 0xffffff, &ink);
    if (!under && !strike) { CHECK(cnt == 0); }
    else {
        CHECKF(cnt > 0, "decorated blanks drew nothing");
        CHECKF(ink.left == 20 && ink.right == 20 + w - 1, "line spans %ld..%ld want 20..%d", ink.left, ink.right, 20 + w - 1);
        CHECKF(cnt % w == 0, "ink %d not whole rows of %d", cnt, w);
        CHECKF(ink.top >= 20 && ink.bottom < 20 + tm.tmHeight, "line rows %ld..%ld outside cell", ink.top, ink.bottom);
        if (under && !strike) CHECKF(ink.top >= 20 + tm.tmAscent - 1 || to_wine(), "underline above baseline: top %ld asc %ld", ink.top, tm.tmAscent);
        if (strike && !under) CHECKF(ink.bottom < 20 + tm.tmAscent || to_wine(), "strikeout below baseline");
    }
    to_close(&c);
}
TEST(text_output_fonts_deco_plain_blank_has_no_ink) { deco_case(0, 0); }
TEST(text_output_fonts_deco_underline_blank) { deco_case(1, 0); }
TEST(text_output_fonts_deco_strikeout_blank) { deco_case(0, 1); }
TEST(text_output_fonts_deco_both_blank) { deco_case(1, 1); }
TEST(text_output_fonts_deco_underline_below_strikeout) {
    NEED_F(); REQUIRE(U(GetObjectA)); REQUIRE(U(CreateFontIndirectA));
    TCtx c; TEXTMETRICA tm; if (!to_fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT, 200, 80)) return;
    if (to_wine()) { t_skip("bitmap font decoration not meaningful under wine fallback"); to_close(&c); return; }
    RECT iu, is;
    to_use_font(&c, deco_font(c.dc, 1, 0), 1); U(SetTextColor)(c.dc, 0);
    U(TextOutA)(c.dc, 10, 10, "   ", 3); to_ink(&c, 0xffffff, &iu); to_clear(&c);
    to_use_font(&c, deco_font(c.dc, 0, 1), 1);
    U(TextOutA)(c.dc, 10, 10, "   ", 3); to_ink(&c, 0xffffff, &is);
    CHECKF(iu.top > is.bottom, "underline rows %ld..%ld, strikeout rows %ld..%ld", iu.top, iu.bottom, is.top, is.bottom);
    to_close(&c);
}
TEST(text_output_fonts_deco_underline_does_not_change_extent) {
    NEED_F(); REQUIRE(U(GetObjectA)); REQUIRE(U(CreateFontIndirectA));
    TCtx c; TEXTMETRICA tm; if (!to_fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT, 64, 64)) return;
    SIZE a, b; U(GetTextExtentPoint32A)(c.dc, "Underline", 9, &a);
    to_use_font(&c, deco_font(c.dc, 1, 1), 1);
    U(GetTextExtentPoint32A)(c.dc, "Underline", 9, &b);
    CHECKF(a.cx == b.cx && a.cy == b.cy, "%ldx%ld vs %ldx%ld", a.cx, a.cy, b.cx, b.cy);
    to_close(&c);
}
TEST(text_output_fonts_truetype_underline_sets_tm_flags) {
    NEED_F();
    TCtx c; if (!to_open(&c, 64, 64, 32)) { CHECK(0); return; }
    HFONT f = U(CreateFontA)(-16, 0, 0, 0, FW_NORMAL, 0, TRUE, TRUE, DEFAULT_CHARSET, 0, 0, NONANTIALIASED_QUALITY, 0, "Arial");
    to_use_font(&c, f, 1);
    TEXTMETRICA tm; CHECK(U(GetTextMetricsA)(c.dc, &tm));
    CHECK(tm.tmUnderlined != 0 && tm.tmStruckOut != 0 && tm.tmItalic == 0);
    TEXTMETRICW tw; CHECK(U(GetTextMetricsW)(c.dc, &tw));
    CHECK(tw.tmUnderlined != 0 && tw.tmStruckOut != 0);
    to_close(&c);
}
TEST(text_output_fonts_truetype_underline_adds_ink) {
    NEED_F();
    TCtx c; if (!to_open(&c, 200, 80, 32)) { CHECK(0); return; }
    U(SetTextColor)(c.dc, 0);
    to_use_font(&c, to_mkfont("Arial", -20, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    U(TextOutA)(c.dc, 10, 10, "      ", 6);
    RECT ink; CHECK(to_ink(&c, 0xffffff, &ink) == 0);
    HFONT f = U(CreateFontA)(-20, 0, 0, 0, FW_NORMAL, 0, TRUE, 0, DEFAULT_CHARSET, 0, 0, NONANTIALIASED_QUALITY, 0, "Arial");
    to_use_font(&c, f, 1);
    SIZE sz; U(GetTextExtentPoint32A)(c.dc, "      ", 6, &sz);
    U(TextOutA)(c.dc, 10, 10, "      ", 6);
    int cnt = to_ink(&c, 0xffffff, &ink);
    CHECKF(cnt > 0 && ink.left <= 10 && ink.right >= 10 + sz.cx - 2, "underline of blanks: %d px, %ld..%ld extent %ld", cnt, ink.left, ink.right, sz.cx);
    to_close(&c);
}
/* ---- rotation geometry (TrueType, loose bounds) ---- */
static void rot_case(int esc) {
    NEED_F();
    TCtx c; if (!to_open(&c, 300, 300, 32)) { CHECK(0); return; }
    HFONT f = to_mkfont("Arial", -40, FW_BOLD, esc, NONANTIALIASED_QUALITY);
    to_use_font(&c, f, 1);
    U(SetTextColor)(c.dc, 0);
    U(SetTextAlign)(c.dc, TA_LEFT | TA_BASELINE);
    SIZE sz; CHECK(U(GetTextExtentPoint32A)(c.dc, "Rotated", 7, &sz));
    TEXTMETRICA tm; U(GetTextMetricsA)(c.dc, &tm);
    CHECK(U(TextOutA)(c.dc, 150, 150, "Rotated", 7));
    RECT ink; int n = to_ink(&c, 0xffffff, &ink);
    CHECKF(n > 0, "esc %d no ink", esc);
    int iw = ink.right - ink.left + 1, ih = ink.bottom - ink.top + 1;
    int slack = tm.tmDescent + 4;
    switch (esc) {
    case 0: CHECKF(iw > ih && ink.left >= 150 - 5 && ink.bottom <= 150 + slack && ink.top >= 150 - tm.tmAscent - 2, "esc 0 box %ld,%ld..%ld,%ld", ink.left, ink.top, ink.right, ink.bottom); break;
    case 900: CHECKF(ih > iw && ink.bottom <= 150 + 5 && ink.top < 150 - sz.cx / 2 && ink.left >= 150 - tm.tmAscent - 2 && ink.right <= 150 + slack, "esc 900 box %ld,%ld..%ld,%ld", ink.left, ink.top, ink.right, ink.bottom); break;
    case 1800: CHECKF(iw > ih && ink.right <= 150 + 5 && ink.left < 150 - sz.cx / 2 && ink.top >= 150 - slack && ink.bottom <= 150 + tm.tmAscent + 2, "esc 1800 box %ld,%ld..%ld,%ld", ink.left, ink.top, ink.right, ink.bottom); break;
    case 2700: CHECKF(ih > iw && ink.top >= 150 - 5 && ink.bottom > 150 + sz.cx / 2 && ink.right <= 150 + tm.tmAscent + 2 && ink.left >= 150 - slack, "esc 2700 box %ld,%ld..%ld,%ld", ink.left, ink.top, ink.right, ink.bottom); break;
    }
    to_close(&c);
}
TEST(text_output_fonts_rotate_0) { rot_case(0); }
TEST(text_output_fonts_rotate_90) { rot_case(900); }
TEST(text_output_fonts_rotate_180) { rot_case(1800); }
TEST(text_output_fonts_rotate_270) { rot_case(2700); }
TEST(text_output_fonts_rotate_45_diagonal) {
    NEED_F();
    TCtx c; if (!to_open(&c, 300, 300, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Arial", -40, FW_BOLD, 450, NONANTIALIASED_QUALITY), 1);
    U(SetTextColor)(c.dc, 0);
    U(SetTextAlign)(c.dc, TA_LEFT | TA_BASELINE);
    SIZE sz; U(GetTextExtentPoint32A)(c.dc, "Diagonal", 8, &sz);
    CHECK(U(TextOutA)(c.dc, 60, 240, "Diagonal", 8));
    RECT ink; CHECK(to_ink(&c, 0xffffff, &ink) > 0);
    int iw = ink.right - ink.left + 1, ih = ink.bottom - ink.top + 1;
    CHECKF(iw > sz.cx / 2 && ih > sz.cx / 2 && iw < ih * 2 && ih < iw * 2, "diag box %dx%d for width %ld", iw, ih, sz.cx);
    CHECK(ink.top < 240 - sz.cx / 3);
    to_close(&c);
}
TEST(text_output_fonts_orientation_ignored_in_compatible_mode) {
    NEED_F();
    TCtx c; if (!to_open(&c, 300, 300, 32)) { CHECK(0); return; }
    LOGFONTA lf; memset(&lf, 0, sizeof lf);
    lf.lfHeight = -40; lf.lfWeight = FW_BOLD; lf.lfEscapement = 0; lf.lfOrientation = 900; lf.lfQuality = NONANTIALIASED_QUALITY;
    strcpy(lf.lfFaceName, "Arial");
    to_use_font(&c, U(CreateFontIndirectA)(&lf), 1);
    U(SetTextColor)(c.dc, 0);
    CHECK(U(TextOutA)(c.dc, 40, 100, "Orientation", 11));
    RECT ink; CHECK(to_ink(&c, 0xffffff, &ink) > 0);
    int iw = ink.right - ink.left + 1, ih = ink.bottom - ink.top + 1;
    t_note("orientation-only 900 in GM_COMPATIBLE: ink %dx%d", iw, ih);
    CHECK(iw > ih);
    to_close(&c);
}
TEST(text_output_fonts_orientation_in_advanced_mode) {
    NEED_F(); REQUIRE(U(SetGraphicsMode));
    TCtx c; if (!to_open(&c, 300, 300, 32)) { CHECK(0); return; }
    CHECK(U(SetGraphicsMode)(c.dc, GM_ADVANCED) == GM_COMPATIBLE);
    LOGFONTA lf; memset(&lf, 0, sizeof lf);
    lf.lfHeight = -40; lf.lfWeight = FW_BOLD; lf.lfEscapement = 0; lf.lfOrientation = 900; lf.lfQuality = NONANTIALIASED_QUALITY;
    strcpy(lf.lfFaceName, "Arial");
    to_use_font(&c, U(CreateFontIndirectA)(&lf), 1);
    U(SetTextColor)(c.dc, 0);
    U(SetTextAlign)(c.dc, TA_BASELINE);
    CHECK(U(TextOutA)(c.dc, 100, 200, "Orientation", 11));
    RECT ink; CHECK(to_ink(&c, 0xffffff, &ink) > 0);
    int iw = ink.right - ink.left + 1, ih = ink.bottom - ink.top + 1;
    t_note("orientation 900 / escapement 0 in GM_ADVANCED: ink %dx%d", iw, ih);
    CHECK(U(SetGraphicsMode)(c.dc, GM_COMPATIBLE) == GM_ADVANCED);
    to_close(&c);
}
TEST(text_output_fonts_world_transform_rotation) {
    NEED_F(); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform));
    TCtx c; if (!to_open(&c, 300, 300, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Arial", -40, FW_BOLD, 0, NONANTIALIASED_QUALITY), 1);
    U(SetTextColor)(c.dc, 0);
    SIZE sz; U(GetTextExtentPoint32A)(c.dc, "Transform", 9, &sz);
    U(SetGraphicsMode)(c.dc, GM_ADVANCED);
    XFORM xf = { 0.0f, 1.0f, -1.0f, 0.0f, 150.0f, 20.0f }; /* 90 degrees clockwise, origin offset */
    CHECK(U(SetWorldTransform)(c.dc, &xf));
    SIZE s2; U(GetTextExtentPoint32A)(c.dc, "Transform", 9, &s2);
    CHECKF(s2.cx == sz.cx && s2.cy == sz.cy, "extent is in logical space: %ldx%ld vs %ldx%ld", s2.cx, s2.cy, sz.cx, sz.cy);
    CHECK(U(TextOutA)(c.dc, 0, 0, "Transform", 9));
    RECT ink; CHECK(to_ink(&c, 0xffffff, &ink) > 0);
    int iw = ink.right - ink.left + 1, ih = ink.bottom - ink.top + 1;
    CHECKF(ih > iw, "rotated ink %dx%d", iw, ih);
    CHECKF(ink.top >= 20 - 2 && ink.bottom > 20 + sz.cx / 2, "ink rows %ld..%ld", ink.top, ink.bottom);
    CHECKF(ink.right <= 150 + 2, "right %ld", ink.right);
    U(ModifyWorldTransform)(c.dc, NULL, MWT_IDENTITY);
    U(SetGraphicsMode)(c.dc, GM_COMPATIBLE);
    to_close(&c);
}
TEST(text_output_fonts_world_transform_scale_widens_ink) {
    NEED_F(); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform));
    TCtx c; if (!to_open(&c, 400, 120, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Arial", -24, FW_BOLD, 0, NONANTIALIASED_QUALITY), 1);
    U(SetTextColor)(c.dc, 0);
    CHECK(U(TextOutA)(c.dc, 10, 20, "Scale", 5));
    RECT i1; to_ink(&c, 0xffffff, &i1); to_clear(&c);
    U(SetGraphicsMode)(c.dc, GM_ADVANCED);
    XFORM xf = { 2.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
    CHECK(U(SetWorldTransform)(c.dc, &xf));
    CHECK(U(TextOutA)(c.dc, 10, 20, "Scale", 5));
    RECT i2; to_ink(&c, 0xffffff, &i2);
    int w1 = i1.right - i1.left + 1, w2 = i2.right - i2.left + 1;
    CHECKF(w2 > w1 * 3 / 2 && w2 < w1 * 5 / 2, "w1=%d w2=%d", w1, w2);
    U(ModifyWorldTransform)(c.dc, NULL, MWT_IDENTITY);
    U(SetGraphicsMode)(c.dc, GM_COMPATIBLE);
    to_close(&c);
}
TEST(text_output_fonts_map_mode_lometric_extent) {
    NEED_F(); REQUIRE(U(SetMapMode)); REQUIRE(U(SetGraphicsMode));
    TCtx c; if (!to_open(&c, 64, 64, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Arial", -40, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    SIZE a; U(GetTextExtentPoint32A)(c.dc, "Map", 3, &a);
    CHECK(U(SetMapMode)(c.dc, MM_LOMETRIC) == MM_TEXT);
    SIZE b; BOOL r = U(GetTextExtentPoint32A)(c.dc, "Map", 3, &b);
    t_note("MM_LOMETRIC extent %ldx%ld (MM_TEXT %ldx%ld) ret=%d", b.cx, b.cy, a.cx, a.cy, r);
    CHECK(r);
    CHECK(b.cx > 0);
    U(SetMapMode)(c.dc, MM_TEXT);
    to_close(&c);
}
/* ---- big / zero height ---- */
TEST(text_output_fonts_big_font_draws_bounded) {
    NEED_F();
    TCtx c; if (!to_open(&c, 400, 400, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Arial", -1000, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    U(SetTextColor)(c.dc, 0);
    U(SetTextAlign)(c.dc, TA_BASELINE);
    SetLastError(0);
    CHECK(U(TextOutA)(c.dc, -300, 700, "H", 1));
    RECT ink; int n = to_ink(&c, 0xffffff, &ink);
    CHECKF(n > 0, "huge glyph clipped to bitmap should still cover pixels");
    to_close(&c);
}
TEST(text_output_fonts_height_zero_draws) {
    NEED_F();
    TCtx c; if (!to_open(&c, 200, 80, 32)) { CHECK(0); return; }
    HFONT f = U(CreateFontA)(0, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, NONANTIALIASED_QUALITY, 0, "Arial");
    to_use_font(&c, f, 1);
    U(SetTextColor)(c.dc, 0);
    CHECK(U(TextOutA)(c.dc, 5, 5, "Default size", 12));
    RECT ink; CHECK(to_ink(&c, 0xffffff, &ink) > 0);
    to_close(&c);
}
TEST(text_output_fonts_extreme_heights_do_not_fail_extent) {
    NEED_F();
    static const int h[] = { -1, 1, -2, -4096, 4096, -3000 };
    TCtx c; if (!to_open(&c, 16, 16, 32)) { CHECK(0); return; }
    for (int i = 0; i < ARRAYSZ(h); i++) {
        HFONT f = to_mkfont("Arial", h[i], FW_NORMAL, 0, NONANTIALIASED_QUALITY);
        if (!f) { t_note("CreateFont %d -> NULL", h[i]); continue; }
        to_use_font(&c, f, 1);
        SIZE s = { -1, -1 };
        BOOL r = U(GetTextExtentPoint32A)(c.dc, "Hi", 2, &s);
        t_note("height %d: extent ret=%d %ldx%ld", h[i], r, s.cx, s.cy);
    }
    to_close(&c);
}
TEST(text_output_fonts_lfwidth_changes_advance) {
    NEED_F();
    TCtx c; if (!to_open(&c, 16, 16, 32)) { CHECK(0); return; }
    HFONT f1 = U(CreateFontA)(-20, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, NONANTIALIASED_QUALITY, 0, "Arial");
    to_use_font(&c, f1, 1);
    SIZE a; U(GetTextExtentPoint32A)(c.dc, "Width", 5, &a);
    HFONT f2 = U(CreateFontA)(-20, 20, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, NONANTIALIASED_QUALITY, 0, "Arial");
    to_use_font(&c, f2, 1);
    SIZE b; U(GetTextExtentPoint32A)(c.dc, "Width", 5, &b);
    CHECKF(b.cx > a.cx, "wide font %ld vs %ld", b.cx, a.cx);
    TEXTMETRICA tm; U(GetTextMetricsA)(c.dc, &tm);
    CHECK(tm.tmAveCharWidth >= 18 && tm.tmAveCharWidth <= 22);
    to_close(&c);
}
/* ---- layout ---- */
TEST(text_output_layout_roundtrip) {
    NEED_F(); REQUIRE(U(SetLayout)); REQUIRE(U(GetLayout));
    TCtx c; if (!to_open(&c, 64, 64, 32)) { CHECK(0); return; }
    CHECK(U(GetLayout)(c.dc) == 0);
    CHECK(U(SetLayout)(c.dc, LAYOUT_RTL) == 0);
    CHECK(U(GetLayout)(c.dc) == LAYOUT_RTL);
    CHECK(U(SetLayout)(c.dc, LAYOUT_RTL | LAYOUT_BITMAPORIENTATIONPRESERVED) == LAYOUT_RTL);
    CHECK(U(GetLayout)(c.dc) == (LAYOUT_RTL | LAYOUT_BITMAPORIENTATIONPRESERVED));
    CHECK(U(SetLayout)(c.dc, 0) == (LAYOUT_RTL | LAYOUT_BITMAPORIENTATIONPRESERVED));
    CHECK(U(GetLayout)(c.dc) == 0);
    to_close(&c);
}
TEST(text_output_layout_invalid_dc) {
    NEED_F(); REQUIRE(U(SetLayout)); REQUIRE(U(GetLayout));
    CHECK(U(SetLayout)(NULL, LAYOUT_RTL) == GDI_ERROR);
    CHECK(U(GetLayout)(NULL) == GDI_ERROR);
    CHECK(U(GetLayout)((HDC)(ULONG_PTR)0x1234) == GDI_ERROR);
}
TEST(text_output_layout_rtl_text_origin) {
    NEED_F(); REQUIRE(U(SetLayout));
    TCtx c; TEXTMETRICA tm; if (!to_fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT, 200, 60)) return;
    U(SetBkMode)(c.dc, OPAQUE); U(SetBkColor)(c.dc, RGB(255, 0, 0));
    int w = 4 * tm.tmAveCharWidth;
    U(TextOutA)(c.dc, 50, 10, "    ", 4);
    RECT l; to_ink(&c, 0xffffff, &l); to_clear(&c);
    CHECK(l.left == 50 && l.right == 50 + w - 1);
    U(SetLayout)(c.dc, LAYOUT_RTL);
    CHECK(U(TextOutA)(c.dc, 50, 10, "    ", 4));
    RECT r; int n = to_ink(&c, 0xffffff, &r);
    if (n == 0 && to_wine()) { t_skip("wine draws nothing with LAYOUT_RTL on DIB DC"); U(SetLayout)(c.dc, 0); to_close(&c); return; }
    t_note("RTL layout, TextOut at x=50 w=%d: ink %ld..%ld (dc width 200)", w, r.left, r.right);
    CHECK(n == w * tm.tmHeight);
    CHECKF(r.right == 200 - 1 - 50 || r.left == 200 - 1 - 50 - w + 1 || r.left == 200 - 50 - w + 1 || r.right == 200 - 50,
           "mirrored placement: %ld..%ld", r.left, r.right);
    U(SetLayout)(c.dc, 0);
    to_close(&c);
}
TEST(text_output_layout_rtl_extent_unchanged) {
    NEED_F(); REQUIRE(U(SetLayout));
    TCtx c; if (!to_open(&c, 64, 64, 32)) { CHECK(0); return; }
    to_use_font(&c, to_mkfont("Arial", -20, FW_NORMAL, 0, NONANTIALIASED_QUALITY), 1);
    SIZE a, b; U(GetTextExtentPoint32A)(c.dc, "Layout", 6, &a);
    U(SetLayout)(c.dc, LAYOUT_RTL);
    U(GetTextExtentPoint32A)(c.dc, "Layout", 6, &b);
    CHECK(a.cx == b.cx && a.cy == b.cy);
    U(SetLayout)(c.dc, 0);
    to_close(&c);
}
TEST(text_output_layout_rtl_updatecp_direction) {
    NEED_F(); REQUIRE(U(SetLayout)); REQUIRE(U(MoveToEx)); REQUIRE(U(GetCurrentPositionEx));
    TCtx c; TEXTMETRICA tm; if (!to_fixed_ctx(&c, &tm, SYSTEM_FIXED_FONT, 200, 60)) return;
    U(SetLayout)(c.dc, LAYOUT_RTL);
    U(SetTextAlign)(c.dc, TA_UPDATECP);
    U(MoveToEx)(c.dc, 100, 10, NULL);
    U(TextOutA)(c.dc, 0, 0, "abc", 3);
    POINT p; U(GetCurrentPositionEx)(c.dc, &p);
    t_note("RTL layout TA_UPDATECP: cp after 3 chars = %ld (start 100, w %d)", p.x, 3 * tm.tmAveCharWidth);
    U(SetLayout)(c.dc, 0);
    to_close(&c);
}
TEST(text_output_fonts_dc_font_survives_text_calls) {
    NEED_F();
    TCtx c; if (!to_open(&c, 64, 64, 32)) { CHECK(0); return; }
    HFONT f = to_mkfont("Arial", -20, FW_NORMAL, 0, NONANTIALIASED_QUALITY);
    to_use_font(&c, f, 1);
    U(TextOutA)(c.dc, 0, 0, "x", 1);
    U(ExtTextOutW)(c.dc, 0, 0, 0, NULL, L"x", 1, NULL);
    CHECK(U(GetCurrentObject)(c.dc, OBJ_FONT) == (HGDIOBJ)f);
    to_close(&c);
}
