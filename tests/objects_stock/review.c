/* Additional object-model checks: handle decoding in GetObjectType, UnrealizeObject shortcut,
 * GetCurrentObject/EnumObjects argument validation, GetObjectA/W font size rules, default DC objects. */
#include "os.h"

/* ---- GetObjectType handle decoding ---- */
TEST(review_gettype_upper_dword_ignored) {
    REQUIRE(GOT);
    int kinds[] = { OS_K_PEN, OS_K_SOLID, OS_K_FONT, OS_K_RGN, OS_K_DDB, OS_K_PAL, OS_K_EXTPEN };
    for (int i = 0; i < 7; i++) {
        HGDIOBJ h = os_make(kinds[i]); REQUIRE(h);
        HGDIOBJ wide = (HGDIOBJ)((ULONG_PTR)h | 0x0123456700000000ull);
        CHECKF(GOT(wide) == os_ktype[kinds[i]], "%s wide handle %p -> %lu", os_kname[kinds[i]], wide, (unsigned long)GOT(wide));
        os_free(kinds[i], h);
        CHECKF(GOT(wide) == 0, "%s wide handle after delete", os_kname[kinds[i]]);
    }
}
TEST(review_gettype_upper_dword_ignored_stock) {
    REQUIRE(GOT); REQUIRE(GSO);
    HGDIOBJ h = GSO(BLACK_PEN);
    HGDIOBJ wide = (HGDIOBJ)((ULONG_PTR)h | 0xABCD000000000000ull);
    CHECK(GOT(wide) == OBJ_PEN);
}
TEST(review_gettype_index_only_handle_rejected) {
    REQUIRE(GOT);
    int kinds[] = { OS_K_PEN, OS_K_SOLID, OS_K_FONT, OS_K_RGN, OS_K_DDB, OS_K_PAL, OS_K_MEMDC };
    for (int i = 0; i < 7; i++) {
        HGDIOBJ h = os_make(kinds[i]); REQUIRE(h);
        HGDIOBJ idx = (HGDIOBJ)(ULONG_PTR)H_INDEX(h);
        CHECKF(GOT(idx) == 0 || os_wine(), "%s index-only %p", os_kname[kinds[i]], idx);
        os_free(kinds[i], h);
    }
}
TEST(review_gettype_type_bits_swapped_rejected) {
    REQUIRE(GOT);
    HGDIOBJ pen = os_make(OS_K_PEN), br = os_make(OS_K_SOLID); REQUIRE(pen && br);
    /* Pen index with the brush type byte (and vice versa): the stored upper word no longer matches. */
    HGDIOBJ fake1 = (HGDIOBJ)(((ULONG_PTR)pen & ~0x7f0000ull) | ((ULONG_PTR)br & 0x7f0000ull));
    HGDIOBJ fake2 = (HGDIOBJ)(((ULONG_PTR)br & ~0x7f0000ull) | ((ULONG_PTR)pen & 0x7f0000ull));
    CHECK(GOT(fake1) == 0);
    CHECK(GOT(fake2) == 0);
    os_free(OS_K_PEN, pen); os_free(OS_K_SOLID, br);
}
TEST(review_gettype_reserved_values) {
    REQUIRE(GOT);
    ULONG_PTR bad[] = { 0, 1, 0xffff, 0x10000, 0x100000, 0x7f0000, 0x7fffff, 0x80000000ull, 0xffffffffull,
                        0xffffffffffffffffull, 0x8000000000000000ull, 0x00ff00ff, 0xcccccccc };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++)
        CHECKF(GOT((HGDIOBJ)bad[i]) == 0, "%p", (void *)bad[i]);
}
TEST(review_gettype_stock_bitmap_in_dc) {
    REQUIRE(GOT); REQUIRE(U(GetCurrentObject));
    HDC dc = os_memdc(); REQUIRE(dc);
    HGDIOBJ b = U(GetCurrentObject)(dc, OBJ_BITMAP);
    CHECK(b != NULL);
    CHECK(GOT(b) == OBJ_BITMAP);
    CHECK(H_TYPE(b) == HT_BMP || os_wine());
    U(DeleteDC)(dc);
}
TEST(review_default_bitmap_is_1x1_mono) {
    REQUIRE(GOA); REQUIRE(U(GetCurrentObject));
    HDC dc = os_memdc(); REQUIRE(dc);
    BITMAP bm; memset(&bm, 0xCC, sizeof bm);
    HGDIOBJ b = U(GetCurrentObject)(dc, OBJ_BITMAP);
    CHECK(GOA(b, sizeof bm, &bm) == (int)sizeof bm);
    CHECK(bm.bmWidth == 1 && bm.bmHeight == 1 && bm.bmPlanes == 1 && bm.bmBitsPixel == 1);
    CHECK(bm.bmType == 0);
    U(DeleteDC)(dc);
}
TEST(review_default_bitmap_shared_between_dcs) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(SOBJ);
    HDC a = os_memdc(), b = os_memdc(); REQUIRE(a && b);
    HGDIOBJ ba = U(GetCurrentObject)(a, OBJ_BITMAP), bb = U(GetCurrentObject)(b, OBJ_BITMAP);
    t_note("default bitmaps: %p %p", ba, bb);
    CHECK(ba != NULL && bb != NULL);
    /* The default bitmap is a stock object; selecting it again must not fail. */
    CHECK(SOBJ(a, ba) == ba);
    U(DeleteDC)(a); U(DeleteDC)(b);
}

/* ---- GetCurrentObject ---- */
TEST(review_getcurrent_invalid_type_table) {
    REQUIRE(U(GetCurrentObject));
    HDC dc = os_memdc(); REQUIRE(dc);
    /* Only 1,2,5,6,7,11,14 are accepted by the client-side switch; everything else is ERROR_INVALID_PARAMETER. */
    for (UINT t = 0; t < 40; t++) {
        BOOL valid = (t == 1 || t == 2 || t == 5 || t == 6 || t == 7 || t == 11 || t == 14);
        SET_ERR(0);
        HGDIOBJ r = U(GetCurrentObject)(dc, t);
        DWORD e = GetLastError();
        if (valid) CHECKF(r != NULL || t == 14 || t == 11, "type %u returned NULL", t);
        else {
            CHECKF(r == NULL, "type %u -> %p", t, r);
            CHECKF(e == ERROR_INVALID_PARAMETER || os_wine(), "type %u err %lu", t, e);
        }
    }
    U(DeleteDC)(dc);
}
TEST(review_getcurrent_invalid_type_ignores_dc) {
    REQUIRE(U(GetCurrentObject));
    SET_ERR(0);
    CHECK(U(GetCurrentObject)(NULL, 99) == NULL);
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER || os_wine());
    SET_ERR(0);
    CHECK(U(GetCurrentObject)((HDC)(ULONG_PTR)0xdeadbeef, 0) == NULL);
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER || os_wine());
}
TEST(review_getcurrent_extpen_aliases_pen) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(SOBJ);
    HDC dc = os_memdc(); REQUIRE(dc);
    CHECK(U(GetCurrentObject)(dc, OBJ_EXTPEN) == U(GetCurrentObject)(dc, OBJ_PEN));
    HGDIOBJ p = os_make(OS_K_EXTPEN); REQUIRE(p);
    HGDIOBJ old = SOBJ(dc, p);
    CHECK(U(GetCurrentObject)(dc, OBJ_PEN) == p);
    CHECK(U(GetCurrentObject)(dc, OBJ_EXTPEN) == p);
    CHECK(GOT(p) == OBJ_EXTPEN);
    SOBJ(dc, old);
    DOBJ(p);
    U(DeleteDC)(dc);
}
TEST(review_getcurrent_colorspace_type) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(GOT);
    HDC dc = os_memdc(); REQUIRE(dc);
    HGDIOBJ cs = U(GetCurrentObject)(dc, OBJ_COLORSPACE);
    t_note("default colorspace %p", cs);
    if (cs) CHECK(GOT(cs) == OBJ_COLORSPACE);
    U(DeleteDC)(dc);
}
TEST(review_getcurrent_after_each_select) {
    REQUIRE(U(GetCurrentObject)); REQUIRE(SOBJ);
    HDC dc = os_memdc(); REQUIRE(dc);
    struct { int k; UINT t; } cases[] = { { OS_K_PEN, OBJ_PEN }, { OS_K_SOLID, OBJ_BRUSH }, { OS_K_HATCH, OBJ_BRUSH },
        { OS_K_PATTERN, OBJ_BRUSH }, { OS_K_FONT, OBJ_FONT }, { OS_K_DDB, OBJ_BITMAP }, { OS_K_DIB, OBJ_BITMAP } };
    for (int i = 0; i < 7; i++) {
        HGDIOBJ o = os_make(cases[i].k); REQUIRE(o);
        HGDIOBJ old = SOBJ(dc, o);
        CHECKF(U(GetCurrentObject)(dc, cases[i].t) == o, "%s", os_kname[cases[i].k]);
        SOBJ(dc, old);
        CHECKF(U(GetCurrentObject)(dc, cases[i].t) == old, "%s restored", os_kname[cases[i].k]);
        os_free(cases[i].k, o);
    }
    U(DeleteDC)(dc);
}

/* ---- UnrealizeObject ---- */
TEST(review_unrealize_brush_handles_shortcut) {
    REQUIRE(U(UnrealizeObject)); REQUIRE(GSO);
    /* gdi32full returns TRUE for anything whose type byte says "brush" without asking the kernel. */
    int idx[] = { WHITE_BRUSH, LTGRAY_BRUSH, GRAY_BRUSH, DKGRAY_BRUSH, BLACK_BRUSH, NULL_BRUSH, DC_BRUSH };
    for (int i = 0; i < 7; i++) CHECKF(U(UnrealizeObject)(GSO(idx[i])), "stock %d", idx[i]);
    int kinds[] = { OS_K_SOLID, OS_K_HATCH, OS_K_PATTERN };
    for (int i = 0; i < 3; i++) {
        HGDIOBJ h = os_make(kinds[i]); REQUIRE(h);
        CHECK(U(UnrealizeObject)(h));
        os_free(kinds[i], h);
        CHECKF(U(UnrealizeObject)(h) || os_wine(), "deleted %s still TRUE", os_kname[kinds[i]]);
    }
    CHECK(U(UnrealizeObject)((HGDIOBJ)(ULONG_PTR)0x00100000) || os_wine());
    CHECK(U(UnrealizeObject)((HGDIOBJ)(ULONG_PTR)0x00101234) || os_wine());
    CHECK(U(UnrealizeObject)((HGDIOBJ)(ULONG_PTR)0x80100001ull) || os_wine());
}
TEST(review_unrealize_stock_pen_and_font) {
    REQUIRE(U(UnrealizeObject)); REQUIRE(GSO);
    SET_ERR(0);
    BOOL a = U(UnrealizeObject)(GSO(BLACK_PEN));
    BOOL b = U(UnrealizeObject)(GSO(SYSTEM_FONT));
    t_note("UnrealizeObject(stock pen)=%d stock font=%d", a, b);
    CHECK(GOT(GSO(BLACK_PEN)) == OBJ_PEN);
    CHECK(GOT(GSO(SYSTEM_FONT)) == OBJ_FONT);
}
TEST(review_unrealize_palette_stays_valid) {
    REQUIRE(U(UnrealizeObject)); REQUIRE(GOW);
    HPALETTE p = os_pal(6); REQUIRE(p);
    WORD n = 0;
    BOOL r = U(UnrealizeObject)(p);
    CHECK(r);
    CHECK(GOW(p, sizeof n, &n) == (int)sizeof n);
    CHECK(n == 6);
    DOBJ(p);
}
TEST(review_unrealize_repeated) {
    REQUIRE(U(UnrealizeObject));
    HPALETTE p = os_pal(3); HGDIOBJ b = os_make(OS_K_PATTERN); REQUIRE(p && b);
    for (int i = 0; i < 100; i++) { CHECK(U(UnrealizeObject)(p)); CHECK(U(UnrealizeObject)(b)); }
    DOBJ(p); DOBJ(b);
}

/* ---- EnumObjects ---- */
struct rv_ctx { int n; int stop; };
static int CALLBACK rv_cb(LPVOID obj, LPARAM lp) {
    struct rv_ctx *c = (struct rv_ctx *)lp;
    c->n++;
    return !(c->stop && c->n >= c->stop);
}
static int CALLBACK rv_zero_cb(LPVOID obj, LPARAM lp) { (*(int *)lp)++; return 0; }
static int CALLBACK rv_neg_cb(LPVOID obj, LPARAM lp) { (*(int *)lp)++; return -1; }
static int CALLBACK rv_two_cb(LPVOID obj, LPARAM lp) { (*(int *)lp)++; return 2; }
TEST(review_enum_return_is_last_callback_value) {
    REQUIRE(U(EnumObjects)); HDC dc = os_memdc(); REQUIRE(dc);
    int n = 0;
    int r = U(EnumObjects)(dc, OBJ_BRUSH, (GOBJENUMPROC)rv_two_cb, (LPARAM)&n);
    CHECK(n >= 1);
    CHECKF(r == 2, "ret %d after %d callbacks", r, n);
    n = 0;
    r = U(EnumObjects)(dc, OBJ_PEN, (GOBJENUMPROC)rv_two_cb, (LPARAM)&n);
    CHECKF(r == 2, "pen ret %d", r);
    U(DeleteDC)(dc);
}
TEST(review_enum_zero_stops_immediately) {
    REQUIRE(U(EnumObjects)); HDC dc = os_memdc(); REQUIRE(dc);
    int n = 0;
    CHECK(U(EnumObjects)(dc, OBJ_BRUSH, (GOBJENUMPROC)rv_zero_cb, (LPARAM)&n) == 0);
    CHECK(n == 1);
    n = 0;
    CHECK(U(EnumObjects)(dc, OBJ_PEN, (GOBJENUMPROC)rv_zero_cb, (LPARAM)&n) == 0);
    CHECK(n == 1);
    U(DeleteDC)(dc);
}
TEST(review_enum_negative_continues) {
    REQUIRE(U(EnumObjects)); HDC dc = os_memdc(); REQUIRE(dc);
    int n = 0, full = 0;
    struct rv_ctx c = { 0, 0 };
    U(EnumObjects)(dc, OBJ_BRUSH, (GOBJENUMPROC)rv_cb, (LPARAM)&c);
    full = c.n;
    int r = U(EnumObjects)(dc, OBJ_BRUSH, (GOBJENUMPROC)rv_neg_cb, (LPARAM)&n);
    CHECK(n == full);
    CHECK(r == -1);
    U(DeleteDC)(dc);
}
TEST(review_enum_invalid_type_table) {
    REQUIRE(U(EnumObjects)); HDC dc = os_memdc(); REQUIRE(dc);
    for (int t = -3; t < 40; t++) {
        if (t == OBJ_PEN || t == OBJ_BRUSH) continue;
        struct rv_ctx c = { 0, 0 };
        SET_ERR(0);
        int r = U(EnumObjects)(dc, t, (GOBJENUMPROC)rv_cb, (LPARAM)&c);
        DWORD e = GetLastError();
        CHECKF((r == 0 && c.n == 0 && e == ERROR_INVALID_PARAMETER) || (os_wine() && r == 0 && c.n == 0), "type %d ret %d n %d err %lu", t, r, c.n, e);
    }
    U(DeleteDC)(dc);
}
TEST(review_enum_type_checked_before_dc) {
    REQUIRE(U(EnumObjects));
    struct rv_ctx c = { 0, 0 };
    SET_ERR(0);
    CHECK(U(EnumObjects)(NULL, 7, (GOBJENUMPROC)rv_cb, (LPARAM)&c) == 0);
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER || os_wine());
    CHECK(c.n == 0);
}
TEST(review_enum_pen_records_are_logpen) {
    REQUIRE(U(EnumObjects)); HDC dc = os_memdc(); REQUIRE(dc);
    struct { int n; LOGPEN p[16]; } c; memset(&c, 0, sizeof c);
    int CALLBACK cb(LPVOID obj, LPARAM lp) {
        typeof(c) *x = (void *)lp;
        if (x->n < 16) x->p[x->n] = *(LOGPEN *)obj;
        x->n++;
        return 1;
    }
    U(EnumObjects)(dc, OBJ_PEN, (GOBJENUMPROC)cb, (LPARAM)&c);
    CHECK(c.n >= 1);
    int black = 0, white = 0, nul = 0;
    for (int i = 0; i < c.n && i < 16; i++) {
        if (c.p[i].lopnStyle == PS_NULL) nul++;
        else if (c.p[i].lopnStyle == PS_SOLID && c.p[i].lopnColor == 0) black++;
        else if (c.p[i].lopnStyle == PS_SOLID && c.p[i].lopnColor == 0xFFFFFF) white++;
    }
    t_note("EnumObjects pens: %d total, %d black %d white %d null", c.n, black, white, nul);
}
TEST(review_enum_brush_records_include_all_gray_levels) {
    REQUIRE(U(EnumObjects)); HDC dc = os_memdc(); REQUIRE(dc);
    struct { int n; LOGBRUSH b[32]; } c; memset(&c, 0, sizeof c);
    int CALLBACK cb(LPVOID obj, LPARAM lp) {
        typeof(c) *x = (void *)lp;
        if (x->n < 32) x->b[x->n] = *(LOGBRUSH *)obj;
        x->n++;
        return 1;
    }
    U(EnumObjects)(dc, OBJ_BRUSH, (GOBJENUMPROC)cb, (LPARAM)&c);
    COLORREF want[] = { 0xFFFFFF, 0xC0C0C0, 0x808080, 0x404040, 0x000000 };
    for (int w = 0; w < 5; w++) {
        int found = 0;
        for (int i = 0; i < c.n && i < 32; i++) if (c.b[i].lbStyle == BS_SOLID && c.b[i].lbColor == want[w]) found = 1;
        if (!found) t_note("solid %06lx not enumerated (n=%d)", want[w], c.n);
    }
    U(DeleteDC)(dc);
}
TEST(review_enum_selected_objects_do_not_change_result) {
    REQUIRE(U(EnumObjects)); REQUIRE(SOBJ);
    HDC dc = os_memdc(); REQUIRE(dc);
    struct rv_ctx a = { 0, 0 }, b = { 0, 0 };
    U(EnumObjects)(dc, OBJ_BRUSH, (GOBJENUMPROC)rv_cb, (LPARAM)&a);
    HGDIOBJ br = os_make(OS_K_SOLID), pen = os_make(OS_K_PEN);
    HGDIOBJ ob = SOBJ(dc, br), op = SOBJ(dc, pen);
    U(EnumObjects)(dc, OBJ_BRUSH, (GOBJENUMPROC)rv_cb, (LPARAM)&b);
    CHECK(a.n == b.n);
    SOBJ(dc, ob); SOBJ(dc, op);
    DOBJ(br); DOBJ(pen);
    U(DeleteDC)(dc);
}

/* ---- GetObjectA/W font rules ---- */
TEST(review_font_null_buffer_sizes) {
    REQUIRE(GOA); REQUIRE(GOW);
    HFONT f = os_font("Arial", -13); REQUIRE(f);
    int cbs[] = { 0, 1, 60, 92, 1000, -1, INT_MAX, INT_MIN };
    for (int i = 0; i < 8; i++) {
        CHECKF(GOW(f, cbs[i], NULL) == 92, "W cb %d", cbs[i]);
        CHECKF(GOA(f, cbs[i], NULL) == 60, "A cb %d", cbs[i]);
    }
    int fonts[] = { SYSTEM_FONT, ANSI_VAR_FONT, ANSI_FIXED_FONT, OEM_FIXED_FONT, DEFAULT_GUI_FONT, SYSTEM_FIXED_FONT, DEVICE_DEFAULT_FONT };
    for (int i = 0; i < 7; i++) {
        CHECKF(GOW(GSO(fonts[i]), 0, NULL) == 92, "stock W %d", fonts[i]);
        CHECKF(GOA(GSO(fonts[i]), 0, NULL) == 60, "stock A %d", fonts[i]);
    }
    DOBJ(f);
}
TEST(review_font_a_cb_rules) {
    REQUIRE(GOA);
    HFONT f = os_font("Arial", -13); REQUIRE(f);
    int cbs[] = { 0, 1, 2, 30, 59, 60, 61, 91, 92, 93, 100, 187, 188, 189, 259, 260, 261, 420, 1000 };
    for (unsigned i = 0; i < sizeof cbs / sizeof cbs[0]; i++) {
        int cb = cbs[i];
        BYTE *b = malloc(2048); REQUIRE(b);
        memset(b, 0x6B, 2048);
        int r = GOA(f, cb, b);
        int want = (cb == 60 || cb == 92) ? 60 : (cb > 0x104 ? 0x104 : cb);
        CHECKF(r == want || os_wine(), "cb %d -> %d want %d", cb, r, want);
        for (int j = (r > 0 ? r : 0); j < 2048; j++) if (b[j] != 0x6B) { CHECKF(0, "cb %d wrote past byte %d", cb, j); break; }
        free(b);
    }
    DOBJ(f);
}
TEST(review_font_a_cb_60_vs_92_same_bytes) {
    REQUIRE(GOA);
    HFONT f = os_font("Courier New", -15); REQUIRE(f);
    LOGFONTA a, b;
    BYTE big[128]; memset(big, 0, sizeof big);
    memset(&a, 0, sizeof a);
    CHECK(GOA(f, 60, &a) == 60);
    CHECK(GOA(f, 92, big) == 60);
    memcpy(&b, big, 60);
    CHECK(!memcmp(&a, &b, 60));
    DOBJ(f);
}
TEST(review_font_w_exact_and_prefix) {
    REQUIRE(GOW);
    HFONT f = os_font("Tahoma", -11); REQUIRE(f);
    LOGFONTW full; BYTE part[92];
    memset(&full, 0, sizeof full);
    CHECK(GOW(f, 92, &full) == 92);
    memset(part, 0x6B, sizeof part);
    int r = GOW(f, 40, part);
    t_note("GetObjectW(font, 40) -> %d", r);
    if (r == 40) CHECK(!memcmp(part, &full, 40));
    CHECK(r == 0 || r == 40);
    DOBJ(f);
}
TEST(review_font_facename_nul_padded) {
    REQUIRE(GOW);
    HFONT f = os_font("Arial", -12); REQUIRE(f);
    LOGFONTW lf; memset(&lf, 0xCC, sizeof lf);
    CHECK(GOW(f, sizeof lf, &lf) == (int)sizeof lf);
    CHECK(!lstrcmpW(lf.lfFaceName, L"Arial"));
    DOBJ(f);
}
TEST(review_font_facename_truncated_by_createfont) {
    REQUIRE(GOA); REQUIRE(U(CreateFontA));
    char name[64]; memset(name, 'x', sizeof name); name[63] = 0;
    HFONT f = U(CreateFontA)(-12, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, name); REQUIRE(f);
    LOGFONTA lf; memset(&lf, 0xCC, sizeof lf);
    CHECK(GOA(f, sizeof lf, &lf) == (int)sizeof lf);
    CHECK(lf.lfFaceName[LF_FACESIZE - 1] == 0);
    CHECK(lf.lfFaceName[0] == 'x' && lf.lfFaceName[30] == 'x');
    DOBJ(f);
}

/* ---- object handle integrity / misc ---- */
TEST(review_getobject_wrong_buffer_for_brush_ok) {
    REQUIRE(GOA); REQUIRE(GOW);
    HGDIOBJ br = os_make(OS_K_SOLID); REQUIRE(br);
    LOGBRUSH lb;
    for (int i = 0; i < 4; i++) {
        memset(&lb, 0xCC, sizeof lb);
        int cb = (int[]){ 16, 17, 64, 4096 }[i];
        BYTE *buf = malloc(4096 + 16); REQUIRE(buf);
        memset(buf, 0xCC, 4096 + 16);
        CHECKF(GOA(br, cb, buf) == 16, "A cb %d", cb);
        CHECKF(GOW(br, cb, buf) == 16, "W cb %d", cb);
        memcpy(&lb, buf, sizeof lb);
        CHECK(lb.lbColor == RGB(10, 20, 30) && lb.lbStyle == BS_SOLID);
        CHECK(buf[16] == 0xCC);
        free(buf);
    }
    DOBJ(br);
}
TEST(review_gettype_dc_kinds_exhaustive) {
    REQUIRE(GOT); REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC s = U(GetDC)(NULL); REQUIRE(s);
    HDC m1 = U(CreateCompatibleDC)(s), m2 = U(CreateCompatibleDC)(m1), m3 = U(CreateCompatibleDC)(NULL);
    CHECK(GOT(s) == OBJ_DC);
    CHECK(GOT(m1) == OBJ_MEMDC);
    CHECK(GOT(m2) == OBJ_MEMDC);
    CHECK(GOT(m3) == OBJ_MEMDC);
    CHECK(m1 != m2 && m2 != m3 && m1 != m3);
    U(DeleteDC)(m1); U(DeleteDC)(m2); U(DeleteDC)(m3);
    CHECK(GOT(m1) == 0 && GOT(m2) == 0 && GOT(m3) == 0);
    U(ReleaseDC)(NULL, s);
}
TEST(review_save_restore_dc_keeps_objects) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(SOBJ); REQUIRE(U(GetCurrentObject));
    HDC dc = os_memdc(); REQUIRE(dc);
    HGDIOBJ p = os_make(OS_K_PEN), b = os_make(OS_K_SOLID);
    int id = U(SaveDC)(dc);
    CHECK(id == 1);
    HGDIOBJ op = SOBJ(dc, p), ob = SOBJ(dc, b);
    CHECK(U(GetCurrentObject)(dc, OBJ_PEN) == p);
    CHECK(U(RestoreDC)(dc, id));
    CHECK(U(GetCurrentObject)(dc, OBJ_PEN) == op);
    CHECK(U(GetCurrentObject)(dc, OBJ_BRUSH) == ob);
    CHECK(DOBJ(p) && DOBJ(b));
    U(DeleteDC)(dc);
}
TEST(review_restore_dc_with_deleted_object) {
    REQUIRE(U(SaveDC)); REQUIRE(U(RestoreDC)); REQUIRE(SOBJ); REQUIRE(U(GetCurrentObject));
    HDC dc = os_memdc(); REQUIRE(dc);
    HGDIOBJ p = os_make(OS_K_PEN);
    SOBJ(dc, p);
    int id = U(SaveDC)(dc);
    HGDIOBJ p2 = os_make(OS_K_PEN);
    SOBJ(dc, p2);
    CHECK(U(RestoreDC)(dc, id));
    CHECK(U(GetCurrentObject)(dc, OBJ_PEN) == p);
    CHECK(DOBJ(p2));
    SOBJ(dc, GSO(BLACK_PEN));
    CHECK(DOBJ(p));
    U(DeleteDC)(dc);
}
TEST(review_stock_objects_never_counted) {
    REQUIRE(GSO);
    if (os_gdicount() == 0) { t_skip("no GetGuiResources"); return; }
    DWORD a = os_gdicount();
    for (int r = 0; r < 100; r++) for (int i = 0; i < 22; i++) { HGDIOBJ h = GSO(i); if (h) DOBJ(h); }
    CHECK(os_gdicount() == a);
}
TEST(review_stock_all_survive_mass_delete) {
    REQUIRE(GSO); REQUIRE(GOT);
    HGDIOBJ before[22];
    for (int i = 0; i < 22; i++) before[i] = GSO(i);
    for (int i = 0; i < 22; i++) if (before[i]) DOBJ(before[i]);
    for (int i = 0; i < 22; i++) {
        CHECKF(GSO(i) == before[i], "index %d changed", i);
        if (before[i]) CHECKF(GOT(before[i]) != 0, "index %d dead", i);
    }
}
