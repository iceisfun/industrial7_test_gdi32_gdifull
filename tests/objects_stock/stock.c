/* GetStockObject over the full range, with attributes read back via GetObject. */
#include "os.h"

enum { SK_BRUSH, SK_PEN, SK_FONT, SK_PAL };
static const struct { int idx; const char *name; int kind; } g_stock[] = {
    { WHITE_BRUSH, "WHITE_BRUSH", SK_BRUSH }, { LTGRAY_BRUSH, "LTGRAY_BRUSH", SK_BRUSH },
    { GRAY_BRUSH, "GRAY_BRUSH", SK_BRUSH }, { DKGRAY_BRUSH, "DKGRAY_BRUSH", SK_BRUSH },
    { BLACK_BRUSH, "BLACK_BRUSH", SK_BRUSH }, { NULL_BRUSH, "NULL_BRUSH", SK_BRUSH },
    { WHITE_PEN, "WHITE_PEN", SK_PEN }, { BLACK_PEN, "BLACK_PEN", SK_PEN }, { NULL_PEN, "NULL_PEN", SK_PEN },
    { OEM_FIXED_FONT, "OEM_FIXED_FONT", SK_FONT }, { ANSI_FIXED_FONT, "ANSI_FIXED_FONT", SK_FONT },
    { ANSI_VAR_FONT, "ANSI_VAR_FONT", SK_FONT }, { SYSTEM_FONT, "SYSTEM_FONT", SK_FONT },
    { DEVICE_DEFAULT_FONT, "DEVICE_DEFAULT_FONT", SK_FONT }, { DEFAULT_PALETTE, "DEFAULT_PALETTE", SK_PAL },
    { SYSTEM_FIXED_FONT, "SYSTEM_FIXED_FONT", SK_FONT }, { DEFAULT_GUI_FONT, "DEFAULT_GUI_FONT", SK_FONT },
    { DC_BRUSH, "DC_BRUSH", SK_BRUSH }, { DC_PEN, "DC_PEN", SK_PEN },
};
#define NSTOCK ((int)(sizeof g_stock / sizeof g_stock[0]))

static DWORD kind_objtype(int k) { return k == SK_BRUSH ? OBJ_BRUSH : k == SK_PEN ? OBJ_PEN : k == SK_FONT ? OBJ_FONT : OBJ_PAL; }
static unsigned kind_ht(int k) { return k == SK_BRUSH ? HT_BRUSH : k == SK_PEN ? HT_PEN : k == SK_FONT ? HT_FONT : HT_PAL; }

TEST(stock_all_valid_nonnull) {
    REQUIRE(GSO);
    for (int i = 0; i < NSTOCK; i++) {
        HGDIOBJ h = GSO(g_stock[i].idx);
        CHECKF(h != NULL, "%s", g_stock[i].name);
    }
}
TEST(stock_all_valid_gettype) {
    REQUIRE(GSO); REQUIRE(GOT);
    for (int i = 0; i < NSTOCK; i++) {
        HGDIOBJ h = GSO(g_stock[i].idx);
        CHECKF(GOT(h) == kind_objtype(g_stock[i].kind), "%s type %lu", g_stock[i].name, (unsigned long)GOT(h));
    }
}
TEST(stock_handle_type_bits) {
    REQUIRE(GSO);
    for (int i = 0; i < NSTOCK; i++) {
        HGDIOBJ h = GSO(g_stock[i].idx);
        CHECKF(H_TYPE(h) == kind_ht(g_stock[i].kind), "%s handle %p type bits 0x%x", g_stock[i].name, h, H_TYPE(h));
    }
}
TEST(stock_handle_upper_dword_zero) {
    REQUIRE(GSO);
    for (int i = 0; i < NSTOCK; i++) {
        HGDIOBJ h = GSO(g_stock[i].idx);
        CHECKF(((ULONG_PTR)h >> 32) == 0, "%s %p", g_stock[i].name, h);
    }
}
TEST(stock_handle_stock_bit_note) {
    REQUIRE(GSO);
    for (int i = 0; i < NSTOCK; i++) {
        HGDIOBJ h = GSO(g_stock[i].idx);
        t_note("%s handle=%p stockbit=%u", g_stock[i].name, h, H_STOCKBIT(h));
    }
}
TEST(stock_stable_handles) {
    REQUIRE(GSO);
    for (int i = 0; i < NSTOCK; i++) {
        HGDIOBJ a = GSO(g_stock[i].idx), b = GSO(g_stock[i].idx), c = GSO(g_stock[i].idx);
        CHECKF(a == b && b == c, "%s", g_stock[i].name);
    }
}
TEST(stock_distinct_handles) {
    REQUIRE(GSO);
    for (int i = 0; i < NSTOCK; i++)
        for (int j = i + 1; j < NSTOCK; j++) {
            HGDIOBJ a = GSO(g_stock[i].idx), b = GSO(g_stock[j].idx);
            if (a == b) t_note("%s and %s share a handle %p", g_stock[i].name, g_stock[j].name, a);
            if (g_stock[i].kind != g_stock[j].kind) CHECKF(a != b, "%s vs %s", g_stock[i].name, g_stock[j].name);
        }
}
TEST(stock_brushes_distinct) {
    REQUIRE(GSO);
    int idx[] = { WHITE_BRUSH, LTGRAY_BRUSH, GRAY_BRUSH, DKGRAY_BRUSH, BLACK_BRUSH, NULL_BRUSH, DC_BRUSH };
    for (int i = 0; i < 7; i++) for (int j = i + 1; j < 7; j++)
        CHECKF(GSO(idx[i]) != GSO(idx[j]), "%d vs %d", idx[i], idx[j]);
}
TEST(stock_pens_distinct) {
    REQUIRE(GSO);
    int idx[] = { WHITE_PEN, BLACK_PEN, NULL_PEN, DC_PEN };
    for (int i = 0; i < 4; i++) for (int j = i + 1; j < 4; j++)
        CHECKF(GSO(idx[i]) != GSO(idx[j]), "%d vs %d", idx[i], idx[j]);
}
TEST(stock_null_brush_alias) {
    REQUIRE(GSO);
    CHECK(HOLLOW_BRUSH == NULL_BRUSH);
    CHECK(STOCK_LAST == 19);
    CHECK(GSO(HOLLOW_BRUSH) == GSO(NULL_BRUSH));
}
TEST(stock_constants) {
    CHECK(WHITE_BRUSH == 0 && LTGRAY_BRUSH == 1 && GRAY_BRUSH == 2 && DKGRAY_BRUSH == 3 && BLACK_BRUSH == 4 && NULL_BRUSH == 5);
    CHECK(WHITE_PEN == 6 && BLACK_PEN == 7 && NULL_PEN == 8);
    CHECK(OEM_FIXED_FONT == 10 && ANSI_FIXED_FONT == 11 && ANSI_VAR_FONT == 12 && SYSTEM_FONT == 13);
    CHECK(DEVICE_DEFAULT_FONT == 14 && DEFAULT_PALETTE == 15 && SYSTEM_FIXED_FONT == 16 && DEFAULT_GUI_FONT == 17);
    CHECK(DC_BRUSH == 18 && DC_PEN == 19);
}

/* Invalid indices. */
TEST(stock_invalid_above_range) {
    REQUIRE(GSO);
    int bad[] = { 22, 23, 24, 31, 32, 33, 64, 100, 255, 256, 1000, 0x7fff, 0x10000, 0x7fffffff };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SET_ERR(0);
        CHECKF(GSO(bad[i]) == NULL, "index %d", bad[i]);
    }
}
TEST(stock_invalid_negative) {
    REQUIRE(GSO);
    int bad[] = { -1, -2, -5, -100, -0x10000, INT_MIN, INT_MIN + 1 };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++)
        CHECKF(GSO(bad[i]) == NULL, "index %d", bad[i]);
}
TEST(stock_unused_index_9_note) {
    REQUIRE(GSO);
    t_note("GetStockObject(9)=%p", GSO(9));
}
TEST(stock_index_20_21_note) {
    REQUIRE(GSO);
    t_note("GetStockObject(20)=%p (21)=%p", GSO(20), GSO(21));
}
TEST(stock_invalid_lasterror_note) {
    REQUIRE(GSO);
    SET_ERR(0x1234);
    HGDIOBJ h = GSO(99);
    CHECK(h == NULL);
    t_note("GetLastError after GetStockObject(99)=%lu", GetLastError());
}
TEST(stock_invalid_repeated) {
    REQUIRE(GSO);
    for (int i = 0; i < 50; i++) CHECK(GSO(50 + i) == NULL);
}

/* Brushes. */
static const struct { int idx; COLORREF c; UINT style; } g_br[] = {
    { WHITE_BRUSH, 0xFFFFFF, BS_SOLID }, { LTGRAY_BRUSH, 0xC0C0C0, BS_SOLID }, { GRAY_BRUSH, 0x808080, BS_SOLID },
    { DKGRAY_BRUSH, 0x404040, BS_SOLID }, { BLACK_BRUSH, 0x000000, BS_SOLID }, { NULL_BRUSH, 0, BS_NULL },
};
TEST(stock_brush_logbrush) {
    REQUIRE(GSO); REQUIRE(GOA);
    for (int i = 0; i < 6; i++) {
        LOGBRUSH lb; memset(&lb, 0xCC, sizeof lb);
        int r = GOA(GSO(g_br[i].idx), sizeof lb, &lb);
        CHECKF(r == (int)sizeof(LOGBRUSH), "idx %d ret %d", g_br[i].idx, r);
        CHECKF(lb.lbStyle == g_br[i].style, "idx %d style %u", g_br[i].idx, lb.lbStyle);
        if (g_br[i].style == BS_SOLID) {
            CHECKF(lb.lbColor == g_br[i].c, "idx %d color %06lx", g_br[i].idx, lb.lbColor);
            CHECKF(lb.lbHatch == 0, "idx %d hatch %llx", g_br[i].idx, (unsigned long long)lb.lbHatch);
        }
    }
}
TEST(stock_brush_logbrush_w) {
    REQUIRE(GSO); REQUIRE(GOW);
    for (int i = 0; i < 6; i++) {
        LOGBRUSH lb; memset(&lb, 0xCC, sizeof lb);
        int r = GOW(GSO(g_br[i].idx), sizeof lb, &lb);
        CHECKF(r == (int)sizeof(LOGBRUSH), "idx %d ret %d", g_br[i].idx, r);
        CHECKF(lb.lbStyle == g_br[i].style, "idx %d", g_br[i].idx);
        if (g_br[i].style == BS_SOLID) CHECKF(lb.lbColor == g_br[i].c, "idx %d", g_br[i].idx);
    }
}
TEST(stock_brush_null_buffer_size) {
    REQUIRE(GSO); REQUIRE(GOA); REQUIRE(GOW);
    for (int i = 0; i < 6; i++) {
        CHECKF(GOA(GSO(g_br[i].idx), 0, NULL) == (int)sizeof(LOGBRUSH), "A %d", g_br[i].idx);
        CHECKF(GOW(GSO(g_br[i].idx), 0, NULL) == (int)sizeof(LOGBRUSH), "W %d", g_br[i].idx);
        CHECKF(GOA(GSO(g_br[i].idx), 64, NULL) == (int)sizeof(LOGBRUSH), "A64 %d", g_br[i].idx);
    }
}
TEST(stock_brush_larger_buffer) {
    REQUIRE(GSO); REQUIRE(GOA);
    for (int i = 0; i < 6; i++) {
        BYTE buf[64]; memset(buf, 0xAB, sizeof buf);
        int r = GOA(GSO(g_br[i].idx), sizeof buf, buf);
        CHECKF(r == (int)sizeof(LOGBRUSH), "idx %d ret %d", g_br[i].idx, r);
        CHECKF(buf[sizeof(LOGBRUSH)] == 0xAB && buf[63] == 0xAB, "idx %d overran", g_br[i].idx);
    }
}
TEST(stock_brush_small_buffer_note) {
    REQUIRE(GSO); REQUIRE(GOA);
    LOGBRUSH lb; memset(&lb, 0, sizeof lb);
    int sizes[] = { 0, 1, 4, 8, 12, 15 };
    for (int i = 0; i < 6; i++) t_note("WHITE_BRUSH GetObject cb=%d -> %d", sizes[i], GOA(GSO(WHITE_BRUSH), sizes[i], &lb));
}
TEST(stock_dcbrush_logbrush) {
    REQUIRE(GSO); REQUIRE(GOA);
    LOGBRUSH lb; memset(&lb, 0xCC, sizeof lb);
    int r = GOA(GSO(DC_BRUSH), sizeof lb, &lb);
    CHECK(r == (int)sizeof lb);
    CHECK(lb.lbStyle == BS_SOLID);
    t_note("DC_BRUSH color=%06lx", lb.lbColor);
}
TEST(stock_dcbrush_default_color) {
    REQUIRE(U(GetDCBrushColor)); REQUIRE(U(SetDCBrushColor));
    HDC dc = os_memdc(); REQUIRE(dc);
    CHECK(U(GetDCBrushColor)(dc) == RGB(255, 255, 255));
    CHECK(U(SetDCBrushColor)(dc, RGB(1, 2, 3)) == RGB(255, 255, 255));
    CHECK(U(GetDCBrushColor)(dc) == RGB(1, 2, 3));
    CHECK(U(SetDCBrushColor)(dc, RGB(4, 5, 6)) == RGB(1, 2, 3));
    U(DeleteDC)(dc);
}
TEST(stock_dcpen_default_color) {
    REQUIRE(U(GetDCPenColor)); REQUIRE(U(SetDCPenColor));
    HDC dc = os_memdc(); REQUIRE(dc);
    CHECK(U(GetDCPenColor)(dc) == RGB(0, 0, 0));
    CHECK(U(SetDCPenColor)(dc, RGB(9, 8, 7)) == RGB(0, 0, 0));
    CHECK(U(GetDCPenColor)(dc) == RGB(9, 8, 7));
    U(DeleteDC)(dc);
}
TEST(stock_dcbrush_invalid_dc) {
    REQUIRE(U(GetDCBrushColor)); REQUIRE(U(SetDCBrushColor));
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef, (HDC)(LONG_PTR)-1 };
    for (int i = 0; i < 4; i++) {
        CHECKF(U(GetDCBrushColor)(bad[i]) == CLR_INVALID, "get %p", bad[i]);
        CHECKF(U(SetDCBrushColor)(bad[i], 0x123456) == CLR_INVALID, "set %p", bad[i]);
    }
}
TEST(stock_dcpen_invalid_dc) {
    REQUIRE(U(GetDCPenColor)); REQUIRE(U(SetDCPenColor));
    HDC bad[] = { NULL, (HDC)(ULONG_PTR)1, (HDC)(ULONG_PTR)0xdeadbeef, (HDC)(LONG_PTR)-1 };
    for (int i = 0; i < 4; i++) {
        CHECKF(U(GetDCPenColor)(bad[i]) == CLR_INVALID, "get %p", bad[i]);
        CHECKF(U(SetDCPenColor)(bad[i], 0x123456) == CLR_INVALID, "set %p", bad[i]);
    }
}
TEST(stock_dcbrush_color_per_dc) {
    REQUIRE(U(GetDCBrushColor)); REQUIRE(U(SetDCBrushColor));
    HDC a = os_memdc(), b = os_memdc(); REQUIRE(a && b);
    U(SetDCBrushColor)(a, RGB(10, 0, 0));
    U(SetDCBrushColor)(b, RGB(0, 20, 0));
    CHECK(U(GetDCBrushColor)(a) == RGB(10, 0, 0));
    CHECK(U(GetDCBrushColor)(b) == RGB(0, 20, 0));
    U(DeleteDC)(a); U(DeleteDC)(b);
}
TEST(stock_dcbrush_color_high_byte) {
    REQUIRE(U(GetDCBrushColor)); REQUIRE(U(SetDCBrushColor));
    HDC dc = os_memdc(); REQUIRE(dc);
    U(SetDCBrushColor)(dc, 0x02112233);
    t_note("DC brush color after setting 0x02112233: %08lx", U(GetDCBrushColor)(dc));
    U(SetDCBrushColor)(dc, CLR_INVALID);
    t_note("DC brush color after setting CLR_INVALID: %08lx", U(GetDCBrushColor)(dc));
    U(DeleteDC)(dc);
}

/* Pens. */
TEST(stock_pen_logpen) {
    REQUIRE(GSO); REQUIRE(GOA);
    struct { int idx; UINT st; COLORREF c; int chkc; } t[] = {
        { WHITE_PEN, PS_SOLID, 0xFFFFFF, 1 }, { BLACK_PEN, PS_SOLID, 0, 1 }, { NULL_PEN, PS_NULL, 0, 0 } };
    for (int i = 0; i < 3; i++) {
        LOGPEN lp; memset(&lp, 0xCC, sizeof lp);
        int r = GOA(GSO(t[i].idx), sizeof lp, &lp);
        CHECKF(r == (int)sizeof(LOGPEN), "idx %d ret %d", t[i].idx, r);
        CHECKF(lp.lopnStyle == t[i].st, "idx %d style %u", t[i].idx, lp.lopnStyle);
        if (t[i].chkc) CHECKF(lp.lopnColor == t[i].c, "idx %d color %06lx", t[i].idx, lp.lopnColor);
        t_note("stock pen %d width=(%ld,%ld) color=%06lx", t[i].idx, lp.lopnWidth.x, lp.lopnWidth.y, lp.lopnColor);
        CHECKF(lp.lopnWidth.y == 0 || lp.lopnWidth.y == 0xCCCCCCCC, "idx %d width.y", t[i].idx);
    }
}
TEST(stock_pen_logpen_w) {
    REQUIRE(GSO); REQUIRE(GOW);
    LOGPEN lp; memset(&lp, 0, sizeof lp);
    CHECK(GOW(GSO(BLACK_PEN), sizeof lp, &lp) == (int)sizeof lp);
    CHECK(lp.lopnStyle == PS_SOLID && lp.lopnColor == 0);
    CHECK(GOW(GSO(WHITE_PEN), sizeof lp, &lp) == (int)sizeof lp);
    CHECK(lp.lopnColor == 0xFFFFFF);
}
TEST(stock_pen_null_buffer_size) {
    REQUIRE(GSO); REQUIRE(GOA); REQUIRE(GOW);
    int idx[] = { WHITE_PEN, BLACK_PEN, NULL_PEN, DC_PEN };
    for (int i = 0; i < 4; i++) {
        CHECKF(GOA(GSO(idx[i]), 0, NULL) == (int)sizeof(LOGPEN), "A %d", idx[i]);
        CHECKF(GOW(GSO(idx[i]), 0, NULL) == (int)sizeof(LOGPEN), "W %d", idx[i]);
    }
}
TEST(stock_pen_small_buffer_note) {
    REQUIRE(GSO); REQUIRE(GOA);
    LOGPEN lp; memset(&lp, 0, sizeof lp);
    int sizes[] = { 0, 1, 4, 8, 12, 15 };
    for (int i = 0; i < 6; i++) t_note("BLACK_PEN GetObject cb=%d -> %d", sizes[i], GOA(GSO(BLACK_PEN), sizes[i], &lp));
}
TEST(stock_pen_larger_buffer) {
    REQUIRE(GSO); REQUIRE(GOA);
    BYTE buf[64]; memset(buf, 0x77, sizeof buf);
    int r = GOA(GSO(BLACK_PEN), sizeof buf, buf);
    t_note("BLACK_PEN GetObject cb=64 -> %d", r);
    CHECK(r == (int)sizeof(LOGPEN));
    CHECK(buf[63] == 0x77);
}
TEST(stock_dcpen_logpen) {
    REQUIRE(GSO); REQUIRE(GOA);
    LOGPEN lp; memset(&lp, 0, sizeof lp);
    CHECK(GOA(GSO(DC_PEN), sizeof lp, &lp) == (int)sizeof lp);
    CHECK(lp.lopnStyle == PS_SOLID);
    t_note("DC_PEN width=%ld color=%06lx", lp.lopnWidth.x, lp.lopnColor);
}

/* Fonts. */
static const int g_fonts[] = { OEM_FIXED_FONT, ANSI_FIXED_FONT, ANSI_VAR_FONT, SYSTEM_FONT, DEVICE_DEFAULT_FONT, SYSTEM_FIXED_FONT, DEFAULT_GUI_FONT };
#define NFONTS 7
TEST(stock_font_logfontw_exact) {
    REQUIRE(GSO); REQUIRE(GOW);
    for (int i = 0; i < NFONTS; i++) {
        LOGFONTW lf; memset(&lf, 0xCC, sizeof lf);
        int r = GOW(GSO(g_fonts[i]), sizeof lf, &lf);
        CHECKF(r == (int)sizeof(LOGFONTW), "idx %d ret %d", g_fonts[i], r);
        CHECKF(lf.lfFaceName[0] != 0 || os_wine(), "idx %d empty face", g_fonts[i]);
        CHECKF(wmemchr(lf.lfFaceName, 0, LF_FACESIZE) != NULL, "idx %d face unterminated", g_fonts[i]);
    }
}
TEST(stock_font_logfonta_exact) {
    REQUIRE(GSO); REQUIRE(GOA);
    for (int i = 0; i < NFONTS; i++) {
        LOGFONTA lf; memset(&lf, 0xCC, sizeof lf);
        int r = GOA(GSO(g_fonts[i]), sizeof lf, &lf);
        CHECKF(r == (int)sizeof(LOGFONTA), "idx %d ret %d", g_fonts[i], r);
        CHECKF(lf.lfFaceName[0] != 0 || os_wine(), "idx %d", g_fonts[i]);
        CHECKF(memchr(lf.lfFaceName, 0, LF_FACESIZE) != NULL, "idx %d", g_fonts[i]);
    }
}
TEST(stock_font_a_w_facename_agree) {
    REQUIRE(GSO); REQUIRE(GOA); REQUIRE(GOW);
    for (int i = 0; i < NFONTS; i++) {
        LOGFONTA a; LOGFONTW w; char conv[LF_FACESIZE * 2];
        memset(&a, 0, sizeof a); memset(&w, 0, sizeof w);
        GOA(GSO(g_fonts[i]), sizeof a, &a);
        GOW(GSO(g_fonts[i]), sizeof w, &w);
        WideCharToMultiByte(CP_ACP, 0, w.lfFaceName, -1, conv, sizeof conv, NULL, NULL);
        CHECKF(!strcmp(conv, a.lfFaceName), "idx %d A=%s W=%s", g_fonts[i], a.lfFaceName, conv);
        CHECKF(a.lfHeight == w.lfHeight && a.lfWeight == w.lfWeight && a.lfCharSet == w.lfCharSet &&
               a.lfPitchAndFamily == w.lfPitchAndFamily && a.lfWidth == w.lfWidth, "idx %d attrs", g_fonts[i]);
    }
}
TEST(stock_font_attributes_note) {
    REQUIRE(GSO); REQUIRE(GOW);
    for (int i = 0; i < NFONTS; i++) {
        LOGFONTW lf; memset(&lf, 0, sizeof lf);
        GOW(GSO(g_fonts[i]), sizeof lf, &lf);
        t_note("font %d: h=%ld w=%ld esc=%ld orient=%ld weight=%ld it=%d ul=%d so=%d cs=%d out=%d clip=%d q=%d pf=0x%x face=%ls",
               g_fonts[i], lf.lfHeight, lf.lfWidth, lf.lfEscapement, lf.lfOrientation, lf.lfWeight, lf.lfItalic, lf.lfUnderline,
               lf.lfStrikeOut, lf.lfCharSet, lf.lfOutPrecision, lf.lfClipPrecision, lf.lfQuality, lf.lfPitchAndFamily, lf.lfFaceName);
    }
}
TEST(stock_font_oem_charset) {
    REQUIRE(GSO); REQUIRE(GOW);
    LOGFONTW lf; memset(&lf, 0, sizeof lf);
    CHECK(GOW(GSO(OEM_FIXED_FONT), sizeof lf, &lf) == (int)sizeof lf);
    CHECK(lf.lfCharSet == OEM_CHARSET);
}
TEST(stock_font_zero_escapement) {
    REQUIRE(GSO); REQUIRE(GOW);
    for (int i = 0; i < NFONTS; i++) {
        LOGFONTW lf; memset(&lf, 0xCC, sizeof lf);
        GOW(GSO(g_fonts[i]), sizeof lf, &lf);
        CHECKF(lf.lfEscapement == 0 && lf.lfOrientation == 0, "idx %d", g_fonts[i]);
        CHECKF(lf.lfUnderline == 0 && lf.lfStrikeOut == 0, "idx %d", g_fonts[i]);
    }
}
TEST(stock_font_weight_range) {
    REQUIRE(GSO); REQUIRE(GOW);
    for (int i = 0; i < NFONTS; i++) {
        LOGFONTW lf; memset(&lf, 0, sizeof lf);
        GOW(GSO(g_fonts[i]), sizeof lf, &lf);
        CHECKF(lf.lfWeight >= 0 && lf.lfWeight <= 1000, "idx %d weight %ld", g_fonts[i], lf.lfWeight);
    }
}
TEST(stock_font_null_buffer_size_note) {
    REQUIRE(GSO); REQUIRE(GOA); REQUIRE(GOW);
    for (int i = 0; i < NFONTS; i++)
        t_note("font %d NULL-buffer size A=%d W=%d", g_fonts[i], GOA(GSO(g_fonts[i]), 0, NULL), GOW(GSO(g_fonts[i]), 0, NULL));
}
TEST(stock_font_small_buffers_note) {
    REQUIRE(GSO); REQUIRE(GOW);
    BYTE buf[256];
    int sizes[] = { 0, 1, 4, 28, 91 };
    for (int i = 0; i < 5; i++) {
        memset(buf, 0, sizeof buf);
        t_note("ANSI_VAR_FONT GetObjectW cb=%d -> %d", sizes[i], GOW(GSO(ANSI_VAR_FONT), sizes[i], buf));
    }
}
TEST(stock_font_big_buffer_note) {
    REQUIRE(GSO); REQUIRE(GOW); REQUIRE(GOA);
    BYTE buf[512]; memset(buf, 0, sizeof buf);
    t_note("ANSI_VAR_FONT GetObjectW cb=512 -> %d", GOW(GSO(ANSI_VAR_FONT), 512, buf));
    t_note("ANSI_VAR_FONT GetObjectA cb=512 -> %d", GOA(GSO(ANSI_VAR_FONT), 512, buf));
}
TEST(stock_font_select_textmetrics) {
    REQUIRE(GSO); REQUIRE(U(GetTextMetricsW));
    HDC dc = os_memdc(); REQUIRE(dc);
    for (int i = 0; i < NFONTS; i++) {
        HGDIOBJ old = SOBJ(dc, GSO(g_fonts[i]));
        CHECKF(old != NULL, "select font %d", g_fonts[i]);
        TEXTMETRICW tm; memset(&tm, 0, sizeof tm);
        CHECKF(U(GetTextMetricsW)(dc, &tm), "tm %d", g_fonts[i]);
        CHECKF(tm.tmHeight > 0 && tm.tmAveCharWidth > 0 && tm.tmMaxCharWidth >= tm.tmAveCharWidth, "font %d tm h=%ld avg=%ld", g_fonts[i], tm.tmHeight, tm.tmAveCharWidth);
        CHECKF(U(GetCurrentObject)(dc, OBJ_FONT) == GSO(g_fonts[i]), "current font %d", g_fonts[i]);
        SOBJ(dc, old);
    }
    U(DeleteDC)(dc);
}
TEST(stock_font_gettextface_note) {
    REQUIRE(GSO); REQUIRE(U(GetTextFaceW)); REQUIRE(GOW);
    HDC dc = os_memdc(); REQUIRE(dc);
    for (int i = 0; i < NFONTS; i++) {
        HGDIOBJ old = SOBJ(dc, GSO(g_fonts[i]));
        WCHAR face[LF_FACESIZE + 1] = { 0 };
        LOGFONTW lf; memset(&lf, 0, sizeof lf);
        GOW(GSO(g_fonts[i]), sizeof lf, &lf);
        int n = U(GetTextFaceW)(dc, LF_FACESIZE, face);
        t_note("font %d logfont face=%ls textface=%ls (n=%d)", g_fonts[i], lf.lfFaceName, face, n);
        CHECKF(n > 0, "font %d", g_fonts[i]);
        SOBJ(dc, old);
    }
    U(DeleteDC)(dc);
}
TEST(stock_default_gui_font_vs_system) {
    REQUIRE(GSO); REQUIRE(GOW);
    LOGFONTW g, s; memset(&g, 0, sizeof g); memset(&s, 0, sizeof s);
    GOW(GSO(DEFAULT_GUI_FONT), sizeof g, &g);
    GOW(GSO(SYSTEM_FONT), sizeof s, &s);
    CHECK(GSO(DEFAULT_GUI_FONT) != GSO(SYSTEM_FONT));
    t_note("DEFAULT_GUI_FONT %ls h=%ld; SYSTEM_FONT %ls h=%ld", g.lfFaceName, g.lfHeight, s.lfFaceName, s.lfHeight);
}
TEST(stock_default_gui_font_dpi_note) {
    REQUIRE(GSO); REQUIRE(GOW); REQUIRE(U(SetThreadDpiAwarenessContext));
    LOGFONTW lf; memset(&lf, 0, sizeof lf);
    DPI_AWARENESS_CONTEXT ctxs[] = { DPI_AWARENESS_CONTEXT_UNAWARE, DPI_AWARENESS_CONTEXT_SYSTEM_AWARE, DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE };
    DPI_AWARENESS_CONTEXT orig = U(SetThreadDpiAwarenessContext)(ctxs[0]);
    for (int i = 0; i < 3; i++) {
        U(SetThreadDpiAwarenessContext)(ctxs[i]);
        HGDIOBJ f = GSO(DEFAULT_GUI_FONT);
        GOW(f, sizeof lf, &lf);
        t_note("ctx %d: DEFAULT_GUI_FONT=%p h=%ld face=%ls", i, f, lf.lfHeight, lf.lfFaceName);
        CHECK(f != NULL);
        CHECK(GOT(f) == OBJ_FONT);
    }
    if (orig) U(SetThreadDpiAwarenessContext)(orig);
}

/* Palette. */
TEST(stock_default_palette) {
    REQUIRE(GSO); REQUIRE(GOW);
    WORD n = 0xFFFF;
    CHECK(GOW(GSO(DEFAULT_PALETTE), sizeof n, &n) == (int)sizeof n);
    CHECK(n == 20);
}
TEST(stock_default_palette_a) {
    REQUIRE(GSO); REQUIRE(GOA);
    WORD n = 0xFFFF;
    CHECK(GOA(GSO(DEFAULT_PALETTE), sizeof n, &n) == (int)sizeof n);
    CHECK(n == 20);
}
TEST(stock_default_palette_entries) {
    REQUIRE(GSO); REQUIRE(U(GetPaletteEntries));
    PALETTEENTRY pe[32]; memset(pe, 0xCC, sizeof pe);
    UINT r = U(GetPaletteEntries)(GSO(DEFAULT_PALETTE), 0, 32, pe);
    CHECK(r == 20);
    CHECK(U(GetPaletteEntries)(GSO(DEFAULT_PALETTE), 0, 0, NULL) == 20);
    /* First entry black, last entry white; system colours. */
    CHECK(pe[0].peRed == 0 && pe[0].peGreen == 0 && pe[0].peBlue == 0);
    CHECK(pe[19].peRed == 255 && pe[19].peGreen == 255 && pe[19].peBlue == 255);
    CHECK(pe[1].peRed == 0x80 && pe[1].peGreen == 0 && pe[1].peBlue == 0);
    CHECK(pe[7].peRed == 0xC0 && pe[7].peGreen == 0xC0 && pe[7].peBlue == 0xC0);
}
TEST(stock_default_palette_null_size_note) {
    REQUIRE(GSO); REQUIRE(GOW);
    t_note("DEFAULT_PALETTE GetObjectW(0,NULL)=%d", GOW(GSO(DEFAULT_PALETTE), 0, NULL));
}
TEST(stock_default_palette_selected_in_new_dc) {
    REQUIRE(GSO); REQUIRE(U(GetCurrentObject));
    HDC dc = os_memdc(); REQUIRE(dc);
    CHECK(U(GetCurrentObject)(dc, OBJ_PAL) == GSO(DEFAULT_PALETTE));
    U(DeleteDC)(dc);
}

/* Default objects of a fresh DC are stock. */
TEST(stock_new_dc_default_pen_brush) {
    REQUIRE(GSO); REQUIRE(U(GetCurrentObject));
    HDC dc = os_memdc(); REQUIRE(dc);
    CHECK(U(GetCurrentObject)(dc, OBJ_PEN) == GSO(BLACK_PEN));
    CHECK(U(GetCurrentObject)(dc, OBJ_BRUSH) == GSO(WHITE_BRUSH));
    t_note("fresh memdc font=%p system=%p ansivar=%p", U(GetCurrentObject)(dc, OBJ_FONT), GSO(SYSTEM_FONT), GSO(ANSI_VAR_FONT));
    HGDIOBJ bmp = U(GetCurrentObject)(dc, OBJ_BITMAP);
    CHECK(bmp != NULL && GOT(bmp) == OBJ_BITMAP);
    BITMAP bm; memset(&bm, 0, sizeof bm);
    CHECK(GOA(bmp, sizeof bm, &bm) == (int)sizeof bm);
    CHECK(bm.bmWidth == 1 && bm.bmHeight == 1 && bm.bmBitsPixel == 1 && bm.bmPlanes == 1);
    U(DeleteDC)(dc);
}
TEST(stock_new_dc_default_pen_is_stock_via_select) {
    REQUIRE(GSO);
    HDC dc = os_memdc(); REQUIRE(dc);
    HGDIOBJ p = SOBJ(dc, GSO(NULL_PEN));
    CHECK(p == GSO(BLACK_PEN));
    HGDIOBJ b = SOBJ(dc, GSO(NULL_BRUSH));
    CHECK(b == GSO(WHITE_BRUSH));
    CHECK(SOBJ(dc, p) == GSO(NULL_PEN));
    CHECK(SOBJ(dc, b) == GSO(NULL_BRUSH));
    U(DeleteDC)(dc);
}

/* Stock objects can be selected, queried, "deleted" and remain valid. */
TEST(stock_select_each_into_dc) {
    REQUIRE(GSO);
    HDC dc = os_memdc(); REQUIRE(dc);
    for (int i = 0; i < NSTOCK; i++) {
        if (g_stock[i].kind == SK_PAL) continue;
        HGDIOBJ h = GSO(g_stock[i].idx);
        HGDIOBJ old = SOBJ(dc, h);
        CHECKF(old != NULL, "%s", g_stock[i].name);
        CHECKF(SOBJ(dc, old) == h, "%s restore", g_stock[i].name);
    }
    U(DeleteDC)(dc);
}
TEST(stock_delete_keeps_valid) {
    REQUIRE(GSO); REQUIRE(DOBJ); REQUIRE(GOT);
    for (int i = 0; i < NSTOCK; i++) {
        HGDIOBJ h = GSO(g_stock[i].idx);
        BOOL r = DOBJ(h);
        t_note("DeleteObject(%s) -> %d, err %lu", g_stock[i].name, r, GetLastError());
        CHECKF(GOT(h) == kind_objtype(g_stock[i].kind), "%s still valid", g_stock[i].name);
        CHECKF(GSO(g_stock[i].idx) == h, "%s same handle", g_stock[i].name);
    }
}
TEST(stock_delete_repeated) {
    REQUIRE(GSO); REQUIRE(DOBJ); REQUIRE(GOT);
    for (int k = 0; k < 20; k++) {
        DOBJ(GSO(BLACK_PEN)); DOBJ(GSO(WHITE_BRUSH)); DOBJ(GSO(SYSTEM_FONT));
    }
    CHECK(GOT(GSO(BLACK_PEN)) == OBJ_PEN);
    CHECK(GOT(GSO(WHITE_BRUSH)) == OBJ_BRUSH);
    CHECK(GOT(GSO(SYSTEM_FONT)) == OBJ_FONT);
}
TEST(stock_delete_does_not_change_count) {
    REQUIRE(GSO); REQUIRE(DOBJ);
    DWORD a = os_gdicount();
    for (int i = 0; i < NSTOCK; i++) DOBJ(GSO(g_stock[i].idx));
    CHECK(os_gdicount() == a);
}
TEST(stock_get_does_not_change_count) {
    REQUIRE(GSO);
    DWORD a = os_gdicount();
    for (int k = 0; k < 100; k++) for (int i = 0; i < NSTOCK; i++) GSO(g_stock[i].idx);
    CHECK(os_gdicount() == a);
}
TEST(stock_still_usable_after_delete_in_dc) {
    REQUIRE(GSO); REQUIRE(DOBJ);
    HDC dc = os_memdc(); REQUIRE(dc);
    HGDIOBJ old = SOBJ(dc, GSO(GRAY_BRUSH));
    DOBJ(GSO(GRAY_BRUSH));
    HGDIOBJ cur = U(GetCurrentObject)(dc, OBJ_BRUSH);
    CHECK(cur == GSO(GRAY_BRUSH));
    LOGBRUSH lb; memset(&lb, 0, sizeof lb);
    CHECK(GOA(cur, sizeof lb, &lb) == (int)sizeof lb);
    CHECK(lb.lbColor == 0x808080);
    SOBJ(dc, old);
    U(DeleteDC)(dc);
}
TEST(stock_unrealize_note) {
    REQUIRE(GSO); REQUIRE(U(UnrealizeObject));
    for (int i = 0; i < NSTOCK; i++) {
        BOOL r = U(UnrealizeObject)(GSO(g_stock[i].idx));
        t_note("UnrealizeObject(%s)=%d", g_stock[i].name, r);
    }
}

/* Cross thread. */
static HGDIOBJ g_th_h[32];
static DWORD WINAPI th_stock(LPVOID p) {
    for (int i = 0; i < NSTOCK; i++) g_th_h[i] = GSO(g_stock[i].idx);
    return 0;
}
TEST(stock_same_handles_other_thread) {
    REQUIRE(GSO);
    memset(g_th_h, 0, sizeof g_th_h);
    HANDLE t = CreateThread(NULL, 0, th_stock, NULL, 0, NULL);
    CHECK(t != NULL);
    if (!t) return;
    WaitForSingleObject(t, 10000); CloseHandle(t);
    for (int i = 0; i < NSTOCK; i++) CHECKF(g_th_h[i] == GSO(g_stock[i].idx), "%s", g_stock[i].name);
}
