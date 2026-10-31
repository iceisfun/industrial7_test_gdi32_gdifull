/* GetGlyphOutlineA/W: parameters, buffer sizing, metrics, bitmaps, native/bezier structure. */
#include "gc.h"

#define NEED_GO() do { REQUIRE(U(GetGlyphOutlineW)); REQUIRE(U(GetGlyphOutlineA)); GC_REQ_BASE(); } while (0)
static BYTE obuf[1 << 21] __attribute__((aligned(8)));
static BYTE obuf2[1 << 21] __attribute__((aligned(8)));

static DWORD goW(HDC dc, UINT ch, UINT fmt, GLYPHMETRICS *gm, DWORD cb, void *buf, const MAT2 *m)
{ return U(GetGlyphOutlineW)(dc, ch, fmt, gm, cb, buf, (MAT2 *)m); }
static DWORD goA(HDC dc, UINT ch, UINT fmt, GLYPHMETRICS *gm, DWORD cb, void *buf, const MAT2 *m)
{ return U(GetGlyphOutlineA)(dc, ch, fmt, gm, cb, buf, (MAT2 *)m); }
static int w32(HDC dc, UINT ch) { INT w = -12345; return U(GetCharWidth32W)(dc, ch, ch, &w) ? w : -12345; }
static int fx(FIXED f) { return f.value; }

typedef struct { int ok, contours, closed, curves, points; } Poly;
/* Validate a GGO_NATIVE (bezier=0) or GGO_BEZIER (bezier=1) buffer of n bytes. */
static Poly poly_parse(const BYTE *p, DWORD n, int bezier)
{
    Poly r; DWORD off = 0; memset(&r, 0, sizeof r); r.ok = 1;
    while (off < n) {
        const TTPOLYGONHEADER *h = (const TTPOLYGONHEADER *)(p + off); DWORD pos, end; POINTFX last;
        if (off + sizeof *h > n || h->cb < sizeof *h || off + h->cb > n || h->cb % 4 || h->dwType != TT_POLYGON_TYPE) { r.ok = 0; return r; }
        r.contours++; end = off + h->cb; pos = off + sizeof *h; last = h->pfxStart;
        while (pos < end) {
            const TTPOLYCURVE *c = (const TTPOLYCURVE *)(p + pos); DWORD sz;
            if (pos + 4 > end || c->cpfx == 0) { r.ok = 0; return r; }
            sz = 4 + c->cpfx * sizeof(POINTFX);
            if (pos + sz > end) { r.ok = 0; return r; }
            if (bezier) { if (c->wType != TT_PRIM_LINE && c->wType != TT_PRIM_CSPLINE) { r.ok = 0; return r; } if (c->wType == TT_PRIM_CSPLINE && c->cpfx % 3) { r.ok = 0; return r; } }
            else if (c->wType != TT_PRIM_LINE && c->wType != TT_PRIM_QSPLINE) { r.ok = 0; return r; }
            last = c->apfx[c->cpfx - 1]; r.curves++; r.points += c->cpfx; pos += sz;
        }
        if (pos != end) { r.ok = 0; return r; }
        if (last.x.value == h->pfxStart.x.value && last.x.fract == h->pfxStart.x.fract && last.y.value == h->pfxStart.y.value && last.y.fract == h->pfxStart.y.fract) r.closed++;
        off = end;
    }
    return r;
}

/* ---- parameters ---- */
TEST(outline_null_mat2_is_error)
{
    GC g; GLYPHMETRICS gm; DWORD r, e; NEED_GO();
    if (!gc_open(&g, "Arial", -32)) return;
    memset(&gm, 0, sizeof gm);
    SetLastError(GC_ERRSENT); r = goW(g.dc, 'A', GGO_METRICS, &gm, 0, NULL, NULL); e = GetLastError();
    EXPECT_EQ(r, GDI_ERROR); EXPECT_EQ(e, GC_ERRSENT);
    SetLastError(GC_ERRSENT); r = goA(g.dc, 'A', GGO_METRICS, &gm, 0, NULL, NULL); e = GetLastError();
    EXPECT_EQ(r, GDI_ERROR); EXPECT_EQ(e, GC_ERRSENT);
    SetLastError(GC_ERRSENT); r = goW(g.dc, 'A', GGO_NATIVE, &gm, 0, NULL, NULL); e = GetLastError();
    EXPECT_EQ(r, GDI_ERROR); EXPECT_EQ(e, GC_ERRSENT);
    gc_close(&g);
}

TEST(outline_null_glyphmetrics_is_error)
{
    GC g; MAT2 m = gc_ident(); DWORD r, e; NEED_GO();
    if (!gc_open(&g, "Arial", -32)) return;
    SetLastError(GC_ERRSENT); r = goW(g.dc, 'A', GGO_METRICS, NULL, 0, NULL, &m); e = GetLastError();
    EXPECT_EQ(r, GDI_ERROR); EXPECT_EQ(e, GC_ERRSENT);
    SetLastError(GC_ERRSENT); r = goW(g.dc, 'A', GGO_NATIVE, NULL, 0, NULL, &m); e = GetLastError();
    EXPECT_EQ(r, GDI_ERROR); EXPECT_EQ(e, GC_ERRSENT);
    gc_close(&g);
}

TEST(outline_invalid_dc_is_error)
{
    GLYPHMETRICS gm; MAT2 m = gc_ident(); HDC dc; HGDIOBJ pen; DWORD r; NEED_GO();
    memset(&gm, 0xAB, sizeof gm);
    SetLastError(GC_ERRSENT);
    r = goW(NULL, 'A', GGO_METRICS, &gm, 0, NULL, &m);
    EXPECT_EQ(r, GDI_ERROR);
    t_note("NULL dc GetGlyphOutlineW err %lu", GetLastError());
    EXPECT_EQ(goW((HDC)(ULONG_PTR)0x12345, 'A', GGO_NATIVE, &gm, 0, NULL, &m), GDI_ERROR);
    EXPECT_EQ(goA(NULL, 'A', GGO_BITMAP, &gm, 0, NULL, &m), GDI_ERROR);
    dc = U(CreateCompatibleDC)(NULL); U(DeleteDC)(dc);
    EXPECT_EQ(goW(dc, 'A', GGO_METRICS, &gm, 0, NULL, &m), GDI_ERROR);
    pen = U(CreatePen)(PS_SOLID, 1, 0);
    EXPECT_EQ(goW((HDC)pen, 'A', GGO_METRICS, &gm, 0, NULL, &m), GDI_ERROR);
    U(DeleteObject)(pen);
}

TEST(outline_invalid_dc_clears_metrics)
{
    GLYPHMETRICS gm; MAT2 m = gc_ident(); DWORD r; NEED_GO();
    memset(&gm, 0xAB, sizeof gm);
    r = goW(NULL, ' ', GGO_NATIVE, &gm, 0, NULL, &m);
    EXPECT_EQ(r, GDI_ERROR);
    t_note("after failure on NULL dc: bbX=%u bbY=%u", gm.gmBlackBoxX, gm.gmBlackBoxY);
}

TEST(outline_tiny_buffer_is_error)
{
    GC g; GLYPHMETRICS gm; MAT2 m = gc_ident(); UINT tiny; DWORD r; static const UINT fmts[] = { GGO_NATIVE, GGO_BEZIER, GGO_BITMAP, GGO_GRAY2_BITMAP, GGO_GRAY4_BITMAP, GGO_GRAY8_BITMAP };
    unsigned i; NEED_GO();
    if (!gc_open(&g, "Tahoma", 72)) { if (!gc_open(&g, "Arial", 72)) return; }
    for (i = 0; i < 6; i++) {
        tiny = 0;
        r = goA(g.dc, 'A', fmts[i], &gm, sizeof tiny, &tiny, &m);
        CHECKF(r == GDI_ERROR, "fmt %u returned %lx", fmts[i], r);
    }
    gc_close(&g);
}

TEST(outline_buffer_boundaries_all_formats)
{
    static const UINT fmts[] = { GGO_NATIVE, GGO_BEZIER, GGO_BITMAP, GGO_GRAY2_BITMAP, GGO_GRAY4_BITMAP, GGO_GRAY8_BITMAP };
    static const UINT chs[] = { 'A', 'g', '8', '@' };
    GC g; GLYPHMETRICS gm; MAT2 m = gc_ident(); unsigned i, c; NEED_GO();
    if (!gc_open(&g, "Arial", -40)) return;
    for (i = 0; i < 6; i++) for (c = 0; c < 4; c++) {
        DWORD need = goW(g.dc, chs[c], fmts[i], &gm, 0, NULL, &m), r;
        CHECKF(need != GDI_ERROR && need > 4, "fmt %u ch %c need %lx", fmts[i], chs[c], need);
        if (need == GDI_ERROR) continue;
        memset(obuf, 0xEE, need + 16);
        r = goW(g.dc, chs[c], fmts[i], &gm, need, obuf, &m);
        CHECKF(r == need, "fmt %u ch %c exact: %lx vs %lx", fmts[i], chs[c], r, need);
        CHECKF(obuf[need] == 0xEE, "fmt %u wrote past exact size", fmts[i]);
        r = goW(g.dc, chs[c], fmts[i], &gm, need - 1, obuf, &m);
        CHECKF(r == GDI_ERROR, "fmt %u ch %c need-1: %lx", fmts[i], chs[c], r);
        r = goW(g.dc, chs[c], fmts[i], &gm, need + 100, obuf, &m);
        CHECKF(r == need, "fmt %u ch %c oversize: %lx vs %lx", fmts[i], chs[c], r, need);
        r = goW(g.dc, chs[c], fmts[i], &gm, need, NULL, &m);
        t_note("fmt %u cb=%lu pvBuffer=NULL -> %lx", fmts[i], need, r);
    }
    gc_close(&g);
}

TEST(outline_size_query_with_nonnull_buffer_zero_cb)
{
    GC g; GLYPHMETRICS gm; MAT2 m = gc_ident(); DWORD a, b; BYTE dummy[8] = { 0 }; NEED_GO();
    if (!gc_open(&g, "Arial", -40)) return;
    a = goW(g.dc, 'B', GGO_NATIVE, &gm, 0, NULL, &m);
    b = goW(g.dc, 'B', GGO_NATIVE, &gm, 0, dummy, &m);
    CHECK(a == b && a != GDI_ERROR);
    CHECK(dummy[0] == 0 && dummy[7] == 0);
    gc_close(&g);
}

TEST(outline_space_all_formats)
{
    static const UINT fmts[] = { GGO_METRICS, GGO_NATIVE, GGO_BEZIER, GGO_BITMAP, GGO_GRAY2_BITMAP, GGO_GRAY4_BITMAP, GGO_GRAY8_BITMAP };
    GC g; MAT2 m = gc_ident(); unsigned i; NEED_GO();
    if (!gc_open(&g, "Arial", -48)) return;
    for (i = 0; i < 7; i++) {
        GLYPHMETRICS gm; DWORD r;
        memset(&gm, 0xAB, sizeof gm);
        SetLastError(GC_ERRSENT);
        r = goW(g.dc, ' ', fmts[i], &gm, 0, NULL, &m);
        if (fmts[i] == GGO_METRICS) CHECKF(r != GDI_ERROR, "metrics %lx", r);
        else CHECKF(r == 0, "fmt %u space size %lx", fmts[i], r);
        CHECKF(GetLastError() == GC_ERRSENT, "fmt %u error changed to %lu", fmts[i], GetLastError());
        CHECKF(gm.gmBlackBoxX == 1 && gm.gmBlackBoxY == 1, "fmt %u bb %ux%u", fmts[i], gm.gmBlackBoxX, gm.gmBlackBoxY);
        CHECKF(gm.gmCellIncX == w32(g.dc, ' '), "fmt %u cellinc %d width %d", fmts[i], gm.gmCellIncX, w32(g.dc, ' '));
        CHECK(gm.gmCellIncY == 0);
    }
    gc_close(&g);
}

TEST(outline_space_A_equals_W)
{
    GC g; MAT2 m = gc_ident(); GLYPHMETRICS a, w; NEED_GO();
    if (!gc_open(&g, "Times New Roman", -30)) return;
    CHECK(goA(g.dc, ' ', GGO_METRICS, &a, 0, NULL, &m) != GDI_ERROR);
    CHECK(goW(g.dc, ' ', GGO_METRICS, &w, 0, NULL, &m) != GDI_ERROR);
    CHECK(!memcmp(&a, &w, sizeof a));
    gc_close(&g);
}

/* ---- metrics ---- */
TEST(outline_metrics_vs_abc_arial)
{
    static const int hs[] = { -12, -16, -32, -48 }; GC g; int hi; UINT c; NEED_GO();
    for (hi = 0; hi < 4; hi++) {
        if (!gc_open(&g, "Arial", hs[hi])) continue;
        for (c = 0x21; c <= 0x7E; c++) {
            GLYPHMETRICS gm; ABC abc; MAT2 m = gc_ident();
            CHECK(goW(g.dc, c, GGO_METRICS, &gm, 0, NULL, &m) != GDI_ERROR);
            CHECK(U(GetCharABCWidthsW)(g.dc, c, c, &abc));
            CHECKF(gm.gmCellIncX == abc.abcA + (int)abc.abcB + abc.abcC && gm.gmCellIncX == w32(g.dc, c), "h=%d %c cellinc %d abc %d w %d", hs[hi], c, gm.gmCellIncX, abc.abcA + (int)abc.abcB + abc.abcC, w32(g.dc, c));
            CHECKF(abs((int)gm.gmBlackBoxX - (int)abc.abcB) <= 1, "h=%d %c bbx %u abcB %u", hs[hi], c, gm.gmBlackBoxX, abc.abcB);
            CHECKF(abs(gm.gmptGlyphOrigin.x - abc.abcA) <= 1, "h=%d %c origin %ld abcA %d", hs[hi], c, gm.gmptGlyphOrigin.x, abc.abcA);
            CHECK(gm.gmCellIncY == 0);
            CHECK(gm.gmBlackBoxX >= 1 && gm.gmBlackBoxY >= 1);
        }
        gc_close(&g);
    }
}

TEST(outline_cellinc_vs_width32_all_faces)
{
    GC g; int fi, hi, off = 0; UINT c; NEED_GO();
    for (fi = 0; fi < GC_NFACES; fi++) for (hi = 0; hi < GC_NHEIGHTS; hi++) {
        if (!gc_open(&g, gc_faces[fi], gc_heights[hi])) continue;
        for (c = 0x20; c <= 0x7E; c++) {
            GLYPHMETRICS gm; MAT2 m = gc_ident();
            CHECK(goW(g.dc, c, GGO_METRICS, &gm, 0, NULL, &m) != GDI_ERROR);
            if (gm.gmCellIncX != w32(g.dc, c)) off++;
            CHECKF(abs(gm.gmCellIncX - w32(g.dc, c)) <= 1, "%s %d %c cellinc %d w %d", g.face, g.h, c, gm.gmCellIncX, w32(g.dc, c));
        }
        gc_close(&g);
    }
    if (off) t_note("%d (face,size,char) with cellIncX != GetCharWidth32", off);
}

TEST(outline_metrics_bbox_scales_with_size)
{
    GC a, b; UINT c; NEED_GO();
    if (!gc_open(&a, "Arial", -24)) return;
    if (!gc_open(&b, "Arial", -96)) { gc_close(&a); return; }
    for (c = 'A'; c <= 'Z'; c++) {
        GLYPHMETRICS ga, gb; MAT2 m = gc_ident();
        goW(a.dc, c, GGO_METRICS, &ga, 0, NULL, &m); goW(b.dc, c, GGO_METRICS, &gb, 0, NULL, &m);
        CHECKF(abs((int)gb.gmBlackBoxX - 4 * (int)ga.gmBlackBoxX) <= 8 && abs((int)gb.gmBlackBoxY - 4 * (int)ga.gmBlackBoxY) <= 8,
               "%c 24px %ux%u 96px %ux%u", c, ga.gmBlackBoxX, ga.gmBlackBoxY, gb.gmBlackBoxX, gb.gmBlackBoxY);
    }
    gc_close(&a); gc_close(&b);
}

TEST(outline_metrics_repeatable)
{
    GC g; GLYPHMETRICS a, b; MAT2 m = gc_ident(); int i; NEED_GO();
    if (!gc_open(&g, "Segoe UI", -19)) return;
    goW(g.dc, 'k', GGO_METRICS, &a, 0, NULL, &m);
    for (i = 0; i < 20; i++) { goW(g.dc, 'k', GGO_METRICS, &b, 0, NULL, &m); CHECK(!memcmp(&a, &b, sizeof a)); }
    gc_close(&g);
}

TEST(outline_metrics_ignore_buffer_content)
{
    GC g; GLYPHMETRICS a, b; MAT2 m = gc_ident(); BYTE buf[64]; DWORD r; NEED_GO();
    if (!gc_open(&g, "Arial", -24)) return;
    goW(g.dc, 'R', GGO_METRICS, &a, 0, NULL, &m);
    memset(buf, 0x99, sizeof buf);
    r = goW(g.dc, 'R', GGO_METRICS, &b, sizeof buf, buf, &m);
    t_note("GGO_METRICS with cb=64 buffer -> %lx", r);
    CHECK(r != GDI_ERROR || gc_wine());
    CHECK(!memcmp(&a, &b, sizeof a) || r == GDI_ERROR);
    gc_close(&g);
}

TEST(outline_metrics_vs_font_tables_at_em_height)
{
    GC g; int upem, i; static const UINT chs[] = { 'A', 'H', 'x', 'g', 'W', 'i', 'O', '.' }; NEED_GO();
    if (!gc_open(&g, "Arial", -2048)) { if (!gc_open(&g, "Arial", -1000)) return; }
    upem = gc_upem(g.dc);
    gc_close(&g);
    if (upem <= 0) return;
    if (!gc_open(&g, "Arial", -upem)) return;
    {
        DWORD lh, hh, hl, hg, hm; BYTE *head = gc_table(g.dc, "head", &lh), *hhea = gc_table(g.dc, "hhea", &hh), *loca = gc_table(g.dc, "loca", &hl),
              *glyf = gc_table(g.dc, "glyf", &hg), *hmtx = gc_table(g.dc, "hmtx", &hm);
        if (head && hhea && loca && glyf && hmtx) {
            int longloca = gc_be16s(head + 50), nh = gc_be16(hhea + 34);
            for (i = 0; i < 8; i++) {
                WCHAR wc = (WCHAR)chs[i]; WORD gid; GLYPHMETRICS gm; MAT2 m = gc_ident(); unsigned o0, o1; const BYTE *gp; int xmin, ymin, xmax, ymax, adv;
                U(GetGlyphIndicesW)(g.dc, &wc, 1, &gid, 0);
                if (longloca) { o0 = gc_be32(loca + gid * 4); o1 = gc_be32(loca + gid * 4 + 4); }
                else { o0 = gc_be16(loca + gid * 2) * 2; o1 = gc_be16(loca + gid * 2 + 2) * 2; }
                if (o1 <= o0 || o0 + 10 > hg) continue;
                gp = glyf + o0;
                xmin = gc_be16s(gp + 2); ymin = gc_be16s(gp + 4); xmax = gc_be16s(gp + 6); ymax = gc_be16s(gp + 8);
                adv = gc_be16(hmtx + (gid < nh ? gid : nh - 1) * 4);
                CHECK(goW(g.dc, chs[i], GGO_METRICS | GGO_UNHINTED, &gm, 0, NULL, &m) != GDI_ERROR);
                CHECKF(abs(gm.gmCellIncX - adv) <= 1, "%c cellinc %d hmtx %d", chs[i], gm.gmCellIncX, adv);
                CHECKF(abs(gm.gmptGlyphOrigin.x - xmin) <= 1, "%c origin.x %ld xMin %d", chs[i], gm.gmptGlyphOrigin.x, xmin);
                CHECKF(abs(gm.gmptGlyphOrigin.y - ymax) <= 1, "%c origin.y %ld yMax %d", chs[i], gm.gmptGlyphOrigin.y, ymax);
                CHECKF(abs((int)gm.gmBlackBoxX - (xmax - xmin)) <= 2, "%c bbx %u width %d", chs[i], gm.gmBlackBoxX, xmax - xmin);
                CHECKF(abs((int)gm.gmBlackBoxY - (ymax - ymin)) <= 2, "%c bby %u height %d", chs[i], gm.gmBlackBoxY, ymax - ymin);
            }
        } else t_note("font tables unavailable");
        free(head); free(hhea); free(loca); free(glyf); free(hmtx);
    }
    gc_close(&g);
}

TEST(outline_unhinted_close_to_hinted)
{
    static const int hs[] = { -16, -24, -48 }; GC g; int hi; UINT c; NEED_GO();
    for (hi = 0; hi < 3; hi++) {
        if (!gc_open(&g, "Arial", hs[hi])) continue;
        for (c = 'A'; c <= 'z'; c++) {
            GLYPHMETRICS a, b; MAT2 m = gc_ident();
            if (c > 'Z' && c < 'a') continue;
            CHECK(goW(g.dc, c, GGO_METRICS, &a, 0, NULL, &m) != GDI_ERROR);
            CHECK(goW(g.dc, c, GGO_METRICS | GGO_UNHINTED, &b, 0, NULL, &m) != GDI_ERROR);
            CHECKF(abs((int)a.gmBlackBoxX - (int)b.gmBlackBoxX) <= 2 && abs((int)a.gmBlackBoxY - (int)b.gmBlackBoxY) <= 2 && abs(a.gmCellIncX - b.gmCellIncX) <= 2,
                   "h=%d %c hinted %ux%u/%d unhinted %ux%u/%d", hs[hi], c, a.gmBlackBoxX, a.gmBlackBoxY, a.gmCellIncX, b.gmBlackBoxX, b.gmBlackBoxY, b.gmCellIncX);
        }
        gc_close(&g);
    }
}

/* ---- A vs W, glyph index ---- */
TEST(outline_A_equals_W_ascii)
{
    static const UINT fmts[] = { GGO_METRICS, GGO_NATIVE, GGO_BITMAP, GGO_GRAY4_BITMAP }; GC g; MAT2 m = gc_ident(); unsigned f; UINT c; NEED_GO();
    if (!gc_open(&g, "Arial", -20)) return;
    for (f = 0; f < 4; f++) for (c = 0x21; c <= 0x7E; c++) {
        GLYPHMETRICS a, w; DWORD ra, rw, n;
        ra = goA(g.dc, c, fmts[f], &a, 0, NULL, &m); rw = goW(g.dc, c, fmts[f], &w, 0, NULL, &m);
        CHECKF(ra == rw && !memcmp(&a, &w, sizeof a), "fmt %u %c: %lx vs %lx", fmts[f], c, ra, rw);
        if (fmts[f] != GGO_METRICS && ra != GDI_ERROR && ra && ra < sizeof obuf) {
            n = ra;
            CHECK(goA(g.dc, c, fmts[f], &a, n, obuf, &m) == n); CHECK(goW(g.dc, c, fmts[f], &w, n, obuf2, &m) == n);
            CHECKF(!memcmp(obuf, obuf2, n), "fmt %u %c data differs", fmts[f], c);
        }
    }
    gc_close(&g);
}

TEST(outline_A_high_bytes_map_via_charset)
{
    static const struct { UINT a, w; } mp[] = { {0xE9,0xE9}, {0xFC,0xFC}, {0xA9,0xA9}, {0x80,0x20AC}, {0x93,0x201C}, {0x99,0x2122} }; GC g; MAT2 m = gc_ident(); unsigned i; NEED_GO();
    if (!gc_open(&g, "Arial", -32)) return;
    for (i = 0; i < 6; i++) {
        GLYPHMETRICS a, w;
        CHECK(goA(g.dc, mp[i].a, GGO_METRICS, &a, 0, NULL, &m) != GDI_ERROR);
        CHECK(goW(g.dc, mp[i].w, GGO_METRICS, &w, 0, NULL, &m) != GDI_ERROR);
        CHECKF(!memcmp(&a, &w, sizeof a), "0x%x vs U+%04x", mp[i].a, mp[i].w);
    }
    gc_close(&g);
}

TEST(outline_glyph_index_matches_char)
{
    static const UINT fmts[] = { GGO_METRICS, GGO_NATIVE, GGO_BITMAP, GGO_GRAY8_BITMAP }; GC g; MAT2 m = gc_ident(); unsigned f; UINT c; NEED_GO(); REQUIRE(U(GetGlyphIndicesW));
    if (!gc_open(&g, "Arial", -28)) return;
    for (c = 'A'; c <= 'Z'; c += 3) {
        WCHAR wc = (WCHAR)c; WORD gid;
        U(GetGlyphIndicesW)(g.dc, &wc, 1, &gid, 0);
        for (f = 0; f < 4; f++) {
            GLYPHMETRICS a, b; DWORD ra = goW(g.dc, c, fmts[f], &a, 0, NULL, &m), rb = goW(g.dc, gid, fmts[f] | GGO_GLYPH_INDEX, &b, 0, NULL, &m);
            CHECKF(ra == rb && !memcmp(&a, &b, sizeof a), "fmt %u %c gid %u: %lx vs %lx", fmts[f], c, gid, ra, rb);
            if (f && ra && ra != GDI_ERROR) {
                goW(g.dc, c, fmts[f], &a, ra, obuf, &m); goW(g.dc, gid, fmts[f] | GGO_GLYPH_INDEX, &b, ra, obuf2, &m);
                CHECKF(!memcmp(obuf, obuf2, ra), "fmt %u %c data differs", fmts[f], c);
            }
            /* the A entry point honours GGO_GLYPH_INDEX as well */
            {
                GLYPHMETRICS c2; DWORD rc = goA(g.dc, gid, fmts[f] | GGO_GLYPH_INDEX, &c2, 0, NULL, &m);
                CHECKF(rc == rb && !memcmp(&c2, &b, sizeof b), "A glyph-index fmt %u %c", fmts[f], c);
            }
        }
    }
    gc_close(&g);
}

TEST(outline_glyph_index_special_values_observed)
{
    static const UINT gids[] = { 0, 1, 3, 0x7FFF, 0xFFFE, 0xFFFF, 0x10000, 0xFFFFFFFF }; GC g; MAT2 m = gc_ident(); unsigned i; NEED_GO();
    if (!gc_open(&g, "Arial", -28)) return;
    for (i = 0; i < 8; i++) {
        GLYPHMETRICS gm; DWORD r = goW(g.dc, gids[i], GGO_METRICS | GGO_GLYPH_INDEX, &gm, 0, NULL, &m);
        t_note("glyph index %x -> %lx (bb %ux%u)", gids[i], r, gm.gmBlackBoxX, gm.gmBlackBoxY);
    }
    CHECK(goW(g.dc, 0, GGO_METRICS | GGO_GLYPH_INDEX, &(GLYPHMETRICS){0}, 0, NULL, &m) != GDI_ERROR);
    gc_close(&g);
}

TEST(outline_char_out_of_range_observed)
{
    static const UINT chs[] = { 0, 1, 0x1F, 0x7F, 0xE000, 0xFFFF, 0x10000, 0x1F600, 0xFFFFFFFF }; GC g; MAT2 m = gc_ident(); unsigned i; NEED_GO();
    if (!gc_open(&g, "Arial", -28)) return;
    for (i = 0; i < 9; i++) {
        GLYPHMETRICS gm; DWORD r = goW(g.dc, chs[i], GGO_METRICS, &gm, 0, NULL, &m);
        t_note("char %x -> %lx (bb %ux%u)", chs[i], r, gm.gmBlackBoxX, gm.gmBlackBoxY);
    }
    gc_close(&g);
}

/* ---- bitmaps ---- */
static DWORD aligned_mono(const GLYPHMETRICS *g) { return ((g->gmBlackBoxX + 31) / 32 * 4) * g->gmBlackBoxY; }
static DWORD aligned_gray(const GLYPHMETRICS *g) { return ((g->gmBlackBoxX + 3) & ~3u) * g->gmBlackBoxY; }

TEST(outline_bitmap_size_formulas)
{
    static const int hs[] = { -12, -16, -32, -72 }; GC g; int hi; UINT c; NEED_GO();
    for (hi = 0; hi < 4; hi++) {
        if (!gc_open(&g, "Arial", hs[hi])) continue;
        for (c = 0x21; c <= 0x7E; c++) {
            GLYPHMETRICS gm; MAT2 m = gc_ident(); DWORD r;
            r = goW(g.dc, c, GGO_BITMAP, &gm, 0, NULL, &m);
            CHECKF(r == aligned_mono(&gm), "h=%d %c mono %lu formula %lu (bb %ux%u)", hs[hi], c, r, aligned_mono(&gm), gm.gmBlackBoxX, gm.gmBlackBoxY);
            r = goW(g.dc, c, GGO_GRAY2_BITMAP, &gm, 0, NULL, &m);
            CHECKF(r == aligned_gray(&gm), "h=%d %c gray2 %lu formula %lu", hs[hi], c, r, aligned_gray(&gm));
            r = goW(g.dc, c, GGO_GRAY4_BITMAP, &gm, 0, NULL, &m);
            CHECKF(r == aligned_gray(&gm), "h=%d %c gray4 %lu formula %lu", hs[hi], c, r, aligned_gray(&gm));
            r = goW(g.dc, c, GGO_GRAY8_BITMAP, &gm, 0, NULL, &m);
            CHECKF(r == aligned_gray(&gm), "h=%d %c gray8 %lu formula %lu", hs[hi], c, r, aligned_gray(&gm));
        }
        gc_close(&g);
    }
}

TEST(outline_bitmap_padding_bits_clear_and_ink_present)
{
    GC g; UINT c; NEED_GO();
    if (!gc_open(&g, "Arial", -40)) return;
    for (c = 0x21; c <= 0x7E; c++) {
        GLYPHMETRICS gm; MAT2 m = gc_ident(); DWORD r = goW(g.dc, c, GGO_BITMAP, &gm, sizeof obuf, obuf, &m); DWORD stride, y, x, ink = 0;
        if (r == GDI_ERROR || !r) { CHECK(0); continue; }
        stride = (gm.gmBlackBoxX + 31) / 32 * 4;
        for (y = 0; y < gm.gmBlackBoxY; y++) for (x = 0; x < stride * 8; x++) {
            int bit = (obuf[y * stride + x / 8] >> (7 - (x & 7))) & 1;
            if (bit) { ink++; if (x >= gm.gmBlackBoxX) { CHECKF(0, "%c padding bit set at row %lu col %lu", c, y, x); y = gm.gmBlackBoxY; break; } }
        }
        CHECKF(ink > 0, "%c has no ink", c);
    }
    gc_close(&g);
}

TEST(outline_bitmap_rectangular_glyphs_are_solid)
{
    static const UINT chs[] = { 'I', 'l', '|' }; GC g; unsigned i; NEED_GO();
    if (!gc_open(&g, "Arial", -48)) return;
    for (i = 0; i < 3; i++) {
        GLYPHMETRICS gm; MAT2 m = gc_ident(); DWORD r = goW(g.dc, chs[i], GGO_BITMAP, &gm, sizeof obuf, obuf, &m), stride, y, x, ink = 0;
        if (r == GDI_ERROR || !r) { CHECK(0); continue; }
        stride = (gm.gmBlackBoxX + 31) / 32 * 4;
        for (y = 0; y < gm.gmBlackBoxY; y++) for (x = 0; x < gm.gmBlackBoxX; x++) ink += (obuf[y * stride + x / 8] >> (7 - (x & 7))) & 1;
        if (ink != gm.gmBlackBoxX * gm.gmBlackBoxY) t_note("%c: ink %lu of %ux%u box", chs[i], ink, gm.gmBlackBoxX, gm.gmBlackBoxY);
        CHECKF(ink > 0, "%c has no ink", chs[i]);
        for (y = 1; y < gm.gmBlackBoxY; y++) { int same = !memcmp(obuf, obuf + y * stride, (gm.gmBlackBoxX + 7) / 8); if (chs[i] == 'I') CHECKF(same, "I row %lu differs from row 0", y); else if (!same) t_note("%c row %lu differs from row 0", chs[i], y); }
        if (chs[i] == 'I') CHECKF(ink == gm.gmBlackBoxX * gm.gmBlackBoxY, "I ink %lu of %ux%u", ink, gm.gmBlackBoxX, gm.gmBlackBoxY);
    }
    gc_close(&g);
}

TEST(outline_gray_values_bounded)
{
    static const struct { UINT fmt; BYTE max; } gf[] = { { GGO_GRAY2_BITMAP, 4 }, { GGO_GRAY4_BITMAP, 16 }, { GGO_GRAY8_BITMAP, 64 } };
    GC g; unsigned f; UINT c; NEED_GO();
    if (!gc_open(&g, "Arial", -36)) return;
    for (f = 0; f < 3; f++) for (c = 'A'; c <= 'z'; c++) {
        GLYPHMETRICS gm; MAT2 m = gc_ident(); DWORD r, i; BYTE mx = 0;
        if (c > 'Z' && c < 'a') continue;
        r = goW(g.dc, c, gf[f].fmt, &gm, sizeof obuf, obuf, &m);
        if (r == GDI_ERROR || !r) { CHECK(0); continue; }
        for (i = 0; i < r; i++) if (obuf[i] > mx) mx = obuf[i];
        CHECKF(mx <= gf[f].max && mx > 0, "fmt %u %c max %u", gf[f].fmt, c, mx);
    }
    gc_close(&g);
}

TEST(outline_gray_solid_glyph_reaches_max_level)
{
    static const struct { UINT fmt; BYTE max; } gf[] = { { GGO_GRAY2_BITMAP, 4 }, { GGO_GRAY4_BITMAP, 16 }, { GGO_GRAY8_BITMAP, 64 } };
    GC g; unsigned f; NEED_GO();
    if (!gc_open(&g, "Arial", -60)) return;
    for (f = 0; f < 3; f++) {
        GLYPHMETRICS gm; MAT2 m = gc_ident(); DWORD r = goW(g.dc, 'I', gf[f].fmt, &gm, sizeof obuf, obuf, &m), i, full = 0;
        if (r == GDI_ERROR || !r) { CHECK(0); continue; }
        for (i = 0; i < r; i++) if (obuf[i] == gf[f].max) full++;
        CHECKF(full >= (gm.gmBlackBoxX - 2) * (gm.gmBlackBoxY - 2), "fmt %u full cells %lu", gf[f].fmt, full);
    }
    gc_close(&g);
}

TEST(outline_mono_bitmap_vs_gray_bbox_close)
{
    GC g; UINT c; NEED_GO();
    if (!gc_open(&g, "Arial", -30)) return;
    for (c = 'A'; c <= 'Z'; c++) {
        GLYPHMETRICS a, b; MAT2 m = gc_ident();
        goW(g.dc, c, GGO_BITMAP, &a, 0, NULL, &m); goW(g.dc, c, GGO_GRAY8_BITMAP, &b, 0, NULL, &m);
        CHECKF(abs((int)a.gmBlackBoxX - (int)b.gmBlackBoxX) <= 2 && abs((int)a.gmBlackBoxY - (int)b.gmBlackBoxY) <= 2 && a.gmCellIncX == b.gmCellIncX, "%c %ux%u vs %ux%u", c, a.gmBlackBoxX, a.gmBlackBoxY, b.gmBlackBoxX, b.gmBlackBoxY);
    }
    gc_close(&g);
}

TEST(outline_big_gray8_bitmap)
{
    GC g; GLYPHMETRICS gm; MAT2 m = gc_ident(); DWORD r; NEED_GO();
    if (!gc_open(&g, "Arial", -400)) return;
    r = goW(g.dc, 'W', GGO_GRAY8_BITMAP, &gm, 0, NULL, &m);
    CHECK(r != GDI_ERROR && r == aligned_gray(&gm) && r > 50000 && r < sizeof obuf);
    if (r != GDI_ERROR && r < sizeof obuf) CHECK(goW(g.dc, 'W', GGO_GRAY8_BITMAP, &gm, r, obuf, &m) == r);
    gc_close(&g);
}

/* ---- native / bezier ---- */
TEST(outline_native_structure_valid_all_faces)
{
    GC g; int fi, hi; UINT c; NEED_GO();
    for (fi = 0; fi < GC_NFACES; fi++) for (hi = 0; hi < GC_NHEIGHTS; hi += 2) {
        int notclosed = 0;
        if (!gc_open(&g, gc_faces[fi], gc_heights[hi])) continue;
        for (c = 0x21; c <= 0x7E; c++) {
            GLYPHMETRICS gm; MAT2 m = gc_ident(); DWORD r = goW(g.dc, c, GGO_NATIVE, &gm, 0, NULL, &m); Poly p;
            if (r == GDI_ERROR) { CHECKF(0, "%s %d %c native failed", g.face, g.h, c); continue; }
            if (!r) continue;
            CHECKF(r < sizeof obuf, "size %lu", r);
            CHECK(goW(g.dc, c, GGO_NATIVE, &gm, r, obuf, &m) == r);
            p = poly_parse(obuf, r, 0);
            CHECKF(p.ok && p.contours > 0, "%s %d %c invalid polygon data (%lu bytes)", g.face, g.h, c, r);
            if (p.closed != p.contours) notclosed++;
        }
        if (notclosed) t_note("%s %d: %d glyphs with a contour whose last point != start", g.face, g.h, notclosed);
        gc_close(&g);
    }
}

TEST(outline_bezier_structure_valid_all_faces)
{
    GC g; int fi; UINT c; NEED_GO();
    for (fi = 0; fi < GC_NFACES; fi++) {
        if (!gc_open(&g, gc_faces[fi], -40)) continue;
        for (c = 0x21; c <= 0x7E; c++) {
            GLYPHMETRICS gm; MAT2 m = gc_ident(); DWORD r = goW(g.dc, c, GGO_BEZIER, &gm, 0, NULL, &m); Poly p;
            if (r == GDI_ERROR) { CHECKF(0, "%s %c bezier failed", g.face, c); continue; }
            if (!r) continue;
            CHECK(goW(g.dc, c, GGO_BEZIER, &gm, r, obuf, &m) == r);
            p = poly_parse(obuf, r, 1);
            CHECKF(p.ok && p.contours > 0, "%s %c invalid bezier data", g.face, c);
        }
        gc_close(&g);
    }
}

TEST(outline_native_and_bezier_have_same_contours)
{
    GC g; UINT c; NEED_GO();
    if (!gc_open(&g, "Times New Roman", -48)) return;
    for (c = 0x21; c <= 0x7E; c++) {
        GLYPHMETRICS gm; MAT2 m = gc_ident(); DWORD rn = goW(g.dc, c, GGO_NATIVE, &gm, 0, NULL, &m), rb = goW(g.dc, c, GGO_BEZIER, &gm, 0, NULL, &m); Poly pn, pb;
        if (rn == GDI_ERROR || rb == GDI_ERROR || !rn) continue;
        goW(g.dc, c, GGO_NATIVE, &gm, rn, obuf, &m); goW(g.dc, c, GGO_BEZIER, &gm, rb, obuf2, &m);
        pn = poly_parse(obuf, rn, 0); pb = poly_parse(obuf2, rb, 1);
        CHECKF(pn.contours == pb.contours, "%c native %d bezier %d contours", c, pn.contours, pb.contours);
    }
    gc_close(&g);
}

TEST(outline_contour_counts_arial)
{
    static const struct { UINT c; int n; } t[] = { {'O',2}, {'I',1}, {'B',3}, {'l',1}, {'i',2}, {'8',3}, {'o',2}, {'e',2}, {'.',1}, {':',2}, {'H',1}, {'A',2}, {'P',2}, {'D',2}, {'0',2}, {'1',1} };
    GC g; unsigned i; NEED_GO();
    if (!gc_open(&g, "Arial", -64)) return;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        GLYPHMETRICS gm; MAT2 m = gc_ident(); DWORD r = goW(g.dc, t[i].c, GGO_NATIVE, &gm, 0, NULL, &m); Poly p;
        if (r == GDI_ERROR || !r) { CHECK(0); continue; }
        goW(g.dc, t[i].c, GGO_NATIVE, &gm, r, obuf, &m);
        p = poly_parse(obuf, r, 0);
        CHECKF(p.ok && p.contours == t[i].n, "%c: %d contours, want %d", t[i].c, p.contours, t[i].n);
    }
    gc_close(&g);
}

TEST(outline_native_header_start_inside_bbox)
{
    GC g; UINT c; NEED_GO();
    if (!gc_open(&g, "Arial", -100)) return;
    for (c = 'A'; c <= 'Z'; c++) {
        GLYPHMETRICS gm; MAT2 m = gc_ident(); DWORD r = goW(g.dc, c, GGO_NATIVE, &gm, 0, NULL, &m); Poly p; DWORD off = 0;
        if (r == GDI_ERROR || !r) continue;
        goW(g.dc, c, GGO_NATIVE, &gm, r, obuf, &m);
        p = poly_parse(obuf, r, 0);
        CHECK(p.ok);
        while (p.ok && off < r) {
            const TTPOLYGONHEADER *h = (const TTPOLYGONHEADER *)(obuf + off);
            int x = fx(h->pfxStart.x), y = fx(h->pfxStart.y);
            CHECKF(x >= gm.gmptGlyphOrigin.x - 2 && x <= gm.gmptGlyphOrigin.x + (int)gm.gmBlackBoxX + 2, "%c start x %d", c, x);
            CHECKF(y <= gm.gmptGlyphOrigin.y + 2 && y >= gm.gmptGlyphOrigin.y - (int)gm.gmBlackBoxY - 2, "%c start y %d", c, y);
            off += h->cb;
        }
    }
    gc_close(&g);
}

TEST(outline_native_coords_scale_with_mat2)
{
    GC g; MAT2 m1 = gc_ident(), m2 = { {0,2}, {0,0}, {0,0}, {0,2} }; UINT c; NEED_GO();
    if (!gc_open(&g, "Arial", -60)) return;
    for (c = 'A'; c <= 'Z'; c += 5) {
        GLYPHMETRICS g1, g2; DWORD r1 = goW(g.dc, c, GGO_NATIVE | GGO_UNHINTED, &g1, 0, NULL, &m1), r2 = goW(g.dc, c, GGO_NATIVE | GGO_UNHINTED, &g2, 0, NULL, &m2);
        const TTPOLYGONHEADER *h1, *h2;
        if (r1 == GDI_ERROR || r2 == GDI_ERROR || !r1) { CHECK(0); continue; }
        CHECKF(r1 == r2, "%c size %lu vs %lu", c, r1, r2);
        goW(g.dc, c, GGO_NATIVE | GGO_UNHINTED, &g1, r1, obuf, &m1); goW(g.dc, c, GGO_NATIVE | GGO_UNHINTED, &g2, r2, obuf2, &m2);
        h1 = (const TTPOLYGONHEADER *)obuf; h2 = (const TTPOLYGONHEADER *)obuf2;
        CHECKF(abs(fx(h2->pfxStart.x) - 2 * fx(h1->pfxStart.x)) <= 2 && abs(fx(h2->pfxStart.y) - 2 * fx(h1->pfxStart.y)) <= 2, "%c start (%d,%d) vs (%d,%d)", c,
               fx(h1->pfxStart.x), fx(h1->pfxStart.y), fx(h2->pfxStart.x), fx(h2->pfxStart.y));
    }
    gc_close(&g);
}

/* ---- MAT2 ---- */
TEST(outline_mat2_scale_doubles_metrics)
{
    GC g; MAT2 m1 = gc_ident(), m2 = { {0,2}, {0,0}, {0,0}, {0,2} }; UINT c; NEED_GO();
    if (!gc_open(&g, "Arial", -32)) return;
    for (c = 'A'; c <= 'Z'; c++) {
        GLYPHMETRICS a, b; goW(g.dc, c, GGO_METRICS | GGO_UNHINTED, &a, 0, NULL, &m1); goW(g.dc, c, GGO_METRICS | GGO_UNHINTED, &b, 0, NULL, &m2);
        CHECKF(abs((int)b.gmBlackBoxX - 2 * (int)a.gmBlackBoxX) <= 2 && abs((int)b.gmBlackBoxY - 2 * (int)a.gmBlackBoxY) <= 2 && abs(b.gmCellIncX - 2 * a.gmCellIncX) <= 2,
               "%c %ux%u/%d -> %ux%u/%d", c, a.gmBlackBoxX, a.gmBlackBoxY, a.gmCellIncX, b.gmBlackBoxX, b.gmBlackBoxY, b.gmCellIncX);
    }
    gc_close(&g);
}

TEST(outline_mat2_fractional_scale)
{
    GC g; MAT2 m1 = gc_ident(), mh = { {0x8000,0}, {0,0}, {0,0}, {0x8000,0} }; GLYPHMETRICS a, b; NEED_GO();
    if (!gc_open(&g, "Arial", -64)) return;
    goW(g.dc, 'M', GGO_METRICS | GGO_UNHINTED, &a, 0, NULL, &m1); goW(g.dc, 'M', GGO_METRICS | GGO_UNHINTED, &b, 0, NULL, &mh);
    CHECKF(abs((int)b.gmBlackBoxX * 2 - (int)a.gmBlackBoxX) <= 3 && abs((int)b.gmBlackBoxY * 2 - (int)a.gmBlackBoxY) <= 3 && abs(b.gmCellIncX * 2 - a.gmCellIncX) <= 3,
           "full %ux%u/%d half %ux%u/%d", a.gmBlackBoxX, a.gmBlackBoxY, a.gmCellIncX, b.gmBlackBoxX, b.gmBlackBoxY, b.gmCellIncX);
    gc_close(&g);
}

TEST(outline_mat2_vertical_flip)
{
    GC g; MAT2 m1 = gc_ident(), mf = { {0,1}, {0,0}, {0,0}, {-1,0xFFFF & 0} }; GLYPHMETRICS a, b; UINT c; NEED_GO();
    mf.eM22.value = -1; mf.eM22.fract = 0;
    if (!gc_open(&g, "Arial", -64)) return;
    for (c = 'A'; c <= 'Z'; c += 4) {
        goW(g.dc, c, GGO_METRICS | GGO_UNHINTED, &a, 0, NULL, &m1); goW(g.dc, c, GGO_METRICS | GGO_UNHINTED, &b, 0, NULL, &mf);
        CHECKF(abs((int)a.gmBlackBoxX - (int)b.gmBlackBoxX) <= 1 && abs((int)a.gmBlackBoxY - (int)b.gmBlackBoxY) <= 1, "%c bb %ux%u vs %ux%u", c, a.gmBlackBoxX, a.gmBlackBoxY, b.gmBlackBoxX, b.gmBlackBoxY);
        CHECKF(abs(b.gmptGlyphOrigin.y - ((int)a.gmBlackBoxY - a.gmptGlyphOrigin.y)) <= 2, "%c origin.y %ld flipped %ld (bby %u orig %ld)", c, a.gmptGlyphOrigin.y, b.gmptGlyphOrigin.y, a.gmBlackBoxY, a.gmptGlyphOrigin.y);
        CHECK(abs(a.gmCellIncX - b.gmCellIncX) <= 1);
    }
    gc_close(&g);
}

TEST(outline_mat2_rotate_90_swaps_box_and_advance)
{
    GC g; MAT2 m1 = gc_ident(), mr; GLYPHMETRICS a, b; UINT c; NEED_GO();
    memset(&mr, 0, sizeof mr); mr.eM12.value = -1; mr.eM21.value = 1;
    if (!gc_open(&g, "Arial", -64)) return;
    for (c = 'A'; c <= 'Z'; c += 4) {
        goW(g.dc, c, GGO_METRICS | GGO_UNHINTED, &a, 0, NULL, &m1); goW(g.dc, c, GGO_METRICS | GGO_UNHINTED, &b, 0, NULL, &mr);
        CHECKF(abs((int)a.gmBlackBoxX - (int)b.gmBlackBoxY) <= 2 && abs((int)a.gmBlackBoxY - (int)b.gmBlackBoxX) <= 2, "%c bb %ux%u rotated %ux%u", c, a.gmBlackBoxX, a.gmBlackBoxY, b.gmBlackBoxX, b.gmBlackBoxY);
        CHECKF(abs(b.gmCellIncX) <= 1 && abs(abs(b.gmCellIncY) - a.gmCellIncX) <= 1, "%c cellinc (%d,%d) rotated (%d,%d)", c, a.gmCellIncX, a.gmCellIncY, b.gmCellIncX, b.gmCellIncY);
    }
    gc_close(&g);
}

TEST(outline_mat2_horizontal_flip_negates_advance)
{
    GC g; MAT2 m1 = gc_ident(), mf; GLYPHMETRICS a, b; NEED_GO();
    memset(&mf, 0, sizeof mf); mf.eM11.value = -1; mf.eM22.value = 1;
    if (!gc_open(&g, "Arial", -64)) return;
    goW(g.dc, 'F', GGO_METRICS | GGO_UNHINTED, &a, 0, NULL, &m1); goW(g.dc, 'F', GGO_METRICS | GGO_UNHINTED, &b, 0, NULL, &mf);
    CHECKF(abs(b.gmCellIncX + a.gmCellIncX) <= 1, "cellinc %d -> %d", a.gmCellIncX, b.gmCellIncX);
    CHECKF(abs((int)a.gmBlackBoxX - (int)b.gmBlackBoxX) <= 1, "bbx %u vs %u", a.gmBlackBoxX, b.gmBlackBoxX);
    gc_close(&g);
}

TEST(outline_mat2_shear_grows_one_axis)
{
    /* MAT2 uses the XFORM row convention (x' = x*eM11 + y*eM21); either way a 0.5 shear grows exactly one box dimension by ~half the other. */
    GC g; MAT2 m1 = gc_ident(), s21 = { {0,1}, {0,0}, {0x8000,0}, {0,1} }, s12 = { {0,1}, {0x8000,0}, {0,0}, {0,1} }; GLYPHMETRICS a, b, c; NEED_GO();
    if (!gc_open(&g, "Arial", -64)) return;
    goW(g.dc, 'I', GGO_METRICS | GGO_UNHINTED, &a, 0, NULL, &m1);
    goW(g.dc, 'I', GGO_METRICS | GGO_UNHINTED, &b, 0, NULL, &s21);
    goW(g.dc, 'I', GGO_METRICS | GGO_UNHINTED, &c, 0, NULL, &s12);
    t_note("shear eM21: %ux%u, shear eM12: %ux%u (plain %ux%u)", b.gmBlackBoxX, b.gmBlackBoxY, c.gmBlackBoxX, c.gmBlackBoxY, a.gmBlackBoxX, a.gmBlackBoxY);
    CHECKF(abs((int)b.gmBlackBoxX - ((int)a.gmBlackBoxX + (int)a.gmBlackBoxY / 2)) <= 3 && abs((int)b.gmBlackBoxY - (int)a.gmBlackBoxY) <= 1, "eM21 shear: %ux%u", b.gmBlackBoxX, b.gmBlackBoxY);
    CHECKF(abs((int)c.gmBlackBoxY - ((int)a.gmBlackBoxY + (int)a.gmBlackBoxX / 2)) <= 3 && abs((int)c.gmBlackBoxX - (int)a.gmBlackBoxX) <= 1, "eM12 shear: %ux%u", c.gmBlackBoxX, c.gmBlackBoxY);
    gc_close(&g);
}

TEST(outline_mat2_zero_matrix_observed)
{
    GC g; MAT2 z; GLYPHMETRICS gm; DWORD r; NEED_GO();
    memset(&z, 0, sizeof z);
    if (!gc_open(&g, "Arial", -64)) return;
    memset(&gm, 0xAB, sizeof gm);
    r = goW(g.dc, 'A', GGO_METRICS, &gm, 0, NULL, &z);
    t_note("zero MAT2 metrics -> %lx bb %ux%u cell %d", r, gm.gmBlackBoxX, gm.gmBlackBoxY, gm.gmCellIncX);
    r = goW(g.dc, 'A', GGO_BITMAP, &gm, 0, NULL, &z);
    t_note("zero MAT2 bitmap size -> %lx", r);
    gc_close(&g);
}

TEST(outline_mat2_identity_fract_encodings)
{
    /* 1.0 may be encoded as {0,1}; {0xFFFF,0} is just below one */
    GC g; MAT2 m1 = gc_ident(), mb = { {0xFFFF,0}, {0,0}, {0,0}, {0xFFFF,0} }; GLYPHMETRICS a, b; NEED_GO();
    if (!gc_open(&g, "Arial", -64)) return;
    goW(g.dc, 'M', GGO_METRICS | GGO_UNHINTED, &a, 0, NULL, &m1); goW(g.dc, 'M', GGO_METRICS | GGO_UNHINTED, &b, 0, NULL, &mb);
    CHECKF(abs((int)a.gmBlackBoxX - (int)b.gmBlackBoxX) <= 1 && abs(a.gmCellIncX - b.gmCellIncX) <= 1, "%ux%u vs %ux%u", a.gmBlackBoxX, a.gmBlackBoxY, b.gmBlackBoxX, b.gmBlackBoxY);
    gc_close(&g);
}

/* ---- fonts and dc kinds ---- */
TEST(outline_raster_font_formats_observed)
{
    static const UINT fmts[] = { GGO_METRICS, GGO_NATIVE, GGO_BEZIER, GGO_BITMAP, GGO_GRAY2_BITMAP, GGO_GRAY4_BITMAP, GGO_GRAY8_BITMAP }; GC g; MAT2 m = gc_ident(); unsigned i; NEED_GO();
    if (!gc_open_ex(&g, "Terminal", -12, 400, OEM_CHARSET, 0)) return;
    for (i = 0; i < 7; i++) {
        GLYPHMETRICS gm; DWORD r; memset(&gm, 0, sizeof gm);
        SetLastError(GC_ERRSENT);
        r = goW(g.dc, 'A', fmts[i], &gm, 0, NULL, &m);
        t_note("Terminal fmt %u -> %lx err %lu bb %ux%u cell %d", fmts[i], r, GetLastError(), gm.gmBlackBoxX, gm.gmBlackBoxY, gm.gmCellIncX);
    }
    gc_close(&g);
}

TEST(outline_stock_system_font_metrics_observed)
{
    HDC dc; HGDIOBJ old; GLYPHMETRICS gm; MAT2 m = gc_ident(); DWORD r; NEED_GO(); REQUIRE(U(GetStockObject));
    dc = U(CreateCompatibleDC)(NULL);
    old = U(SelectObject)(dc, U(GetStockObject)(SYSTEM_FIXED_FONT));
    r = goW(dc, 'A', GGO_METRICS, &gm, 0, NULL, &m);
    t_note("SYSTEM_FIXED_FONT metrics -> %lx bb %ux%u cell %d", r, gm.gmBlackBoxX, gm.gmBlackBoxY, gm.gmCellIncX);
    r = goW(dc, 'A', GGO_BITMAP, &gm, 0, NULL, &m);
    t_note("SYSTEM_FIXED_FONT bitmap size -> %lx", r);
    U(SelectObject)(dc, old);
    U(DeleteDC)(dc);
}

TEST(outline_symbol_and_marlett_structures)
{
    static const char *const fs[] = { "Symbol", "Marlett" }; GC g; int f; UINT c; NEED_GO();
    for (f = 0; f < 2; f++) {
        int good = 0;
        if (!gc_open(&g, fs[f], -48)) continue;
        for (c = 0x30; c <= 0x7A; c++) {
            GLYPHMETRICS gm; MAT2 m = gc_ident(); DWORD r = goW(g.dc, c, GGO_NATIVE, &gm, 0, NULL, &m); Poly p;
            if (r == GDI_ERROR || !r) continue;
            goW(g.dc, c, GGO_NATIVE, &gm, r, obuf, &m);
            p = poly_parse(obuf, r, 0);
            CHECKF(p.ok, "%s %c", g.face, c);
            good++;
        }
        CHECKF(good > 20, "%s: %d glyphs with outlines", g.face, good);
        gc_close(&g);
    }
}

TEST(outline_bold_italic_variants_have_valid_data)
{
    GC g; LOGFONTA lf; HFONT f; unsigned v; UINT c; NEED_GO();
    if (!gc_open(&g, "Times New Roman", -40)) return;
    for (v = 0; v < 3; v++) {
        memset(&lf, 0, sizeof lf); lf.lfHeight = -40; lf.lfWeight = v == 1 ? FW_BOLD : FW_NORMAL; lf.lfItalic = v != 0; lstrcpyA(lf.lfFaceName, "Times New Roman");
        f = U(CreateFontIndirectA)(&lf); CHECK(gc_setfont(&g, f));
        for (c = 'a'; c <= 'z'; c++) {
            GLYPHMETRICS gm; MAT2 m = gc_ident(); DWORD r = goW(g.dc, c, GGO_NATIVE, &gm, 0, NULL, &m);
            if (r == GDI_ERROR || !r) { CHECK(0); continue; }
            goW(g.dc, c, GGO_NATIVE, &gm, r, obuf, &m);
            CHECKF(poly_parse(obuf, r, 0).ok, "variant %u %c", v, c);
        }
    }
    gc_close(&g);
}

TEST(outline_world_transform_does_not_change_outline)
{
    GC g; GLYPHMETRICS a, b; MAT2 m = gc_ident(); XFORM xf = { 2.0f, 0, 0, 2.0f, 0, 0 }; DWORD ra, rb; NEED_GO(); REQUIRE(U(SetGraphicsMode)); REQUIRE(U(SetWorldTransform));
    if (!gc_open(&g, "Arial", -40)) return;
    ra = goW(g.dc, 'G', GGO_METRICS, &a, 0, NULL, &m);
    U(SetGraphicsMode)(g.dc, GM_ADVANCED);
    U(SetWorldTransform)(g.dc, &xf);
    rb = goW(g.dc, 'G', GGO_METRICS, &b, 0, NULL, &m);
    t_note("world transform x2: bb %ux%u -> %ux%u (ret %lx %lx)", a.gmBlackBoxX, a.gmBlackBoxY, b.gmBlackBoxX, b.gmBlackBoxY, ra, rb);
    CHECK(rb != GDI_ERROR);
    U(SetWorldTransform)(g.dc, &(XFORM){ 1, 0, 0, 1, 0, 0 });
    U(SetGraphicsMode)(g.dc, GM_COMPATIBLE);
    gc_close(&g);
}

TEST(outline_mapmode_text_vs_anisotropic_observed)
{
    GC g; GLYPHMETRICS a, b; MAT2 m = gc_ident(); NEED_GO(); REQUIRE(U(SetMapMode)); REQUIRE(U(SetViewportExtEx)); REQUIRE(U(SetWindowExtEx));
    if (!gc_open(&g, "Arial", -40)) return;
    goW(g.dc, 'G', GGO_METRICS, &a, 0, NULL, &m);
    U(SetMapMode)(g.dc, MM_ANISOTROPIC); U(SetWindowExtEx)(g.dc, 1, 1, NULL); U(SetViewportExtEx)(g.dc, 2, 2, NULL);
    CHECK(goW(g.dc, 'G', GGO_METRICS, &b, 0, NULL, &m) != GDI_ERROR);
    t_note("anisotropic 2x: bb %ux%u cell %d -> %ux%u cell %d", a.gmBlackBoxX, a.gmBlackBoxY, a.gmCellIncX, b.gmBlackBoxX, b.gmBlackBoxY, b.gmCellIncX);
    U(SetMapMode)(g.dc, MM_TEXT);
    gc_close(&g);
}

struct othr { HDC dc; DWORD r; GLYPHMETRICS gm; };
static DWORD WINAPI othr_proc(LPVOID p)
{
    struct othr *t = p; MAT2 m = gc_ident();
    t->r = goW(t->dc, 'R', GGO_BITMAP, &t->gm, sizeof obuf2, obuf2, &m);
    return 0;
}
TEST(outline_cross_thread_dc)
{
    GC g; struct othr t; HANDLE h; GLYPHMETRICS gm; MAT2 m = gc_ident(); DWORD r; NEED_GO();
    if (!gc_open(&g, "Arial", -32)) return;
    memset(&t, 0, sizeof t); t.dc = g.dc;
    h = CreateThread(NULL, 0, othr_proc, &t, 0, NULL);
    CHECK(h != NULL);
    if (h) { WaitForSingleObject(h, 10000); CloseHandle(h); }
    r = goW(g.dc, 'R', GGO_BITMAP, &gm, sizeof obuf, obuf, &m);
    CHECK(r != GDI_ERROR && r == t.r && !memcmp(&gm, &t.gm, sizeof gm) && !memcmp(obuf, obuf2, r));
    gc_close(&g);
}

TEST(outline_no_gdi_leak)
{
    GC g; int before, after, i; MAT2 m = gc_ident(); GLYPHMETRICS gm; NEED_GO();
    if (!gc_open(&g, "Arial", -32)) return;
    before = gc_gdicount();
    for (i = 0; i < 400; i++) { goW(g.dc, 'A' + (i % 26), (i & 1) ? GGO_NATIVE : GGO_GRAY8_BITMAP, &gm, sizeof obuf, obuf, &m); goW(g.dc, 'A', GGO_METRICS, &gm, 0, NULL, NULL); }
    after = gc_gdicount();
    if (before >= 0) CHECKF(before == after, "%d -> %d", before, after);
    gc_close(&g);
}

TEST(outline_font_deselected_dc_default_font_works)
{
    HDC dc; GLYPHMETRICS gm; MAT2 m = gc_ident(); DWORD r; NEED_GO();
    dc = U(CreateCompatibleDC)(NULL);
    r = goW(dc, 'A', GGO_METRICS, &gm, 0, NULL, &m);
    t_note("default font metrics ret %lx bb %ux%u", r, gm.gmBlackBoxX, gm.gmBlackBoxY);
    U(DeleteDC)(dc);
}
