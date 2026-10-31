/* Shared helpers for the glyphs_charwidths area. All static. */
#ifndef GC_H
#define GC_H
#include "t.h"
#include <stdlib.h>
#include <math.h>

#ifndef GGO_UNHINTED
#define GGO_UNHINTED 0x100
#endif
#ifndef GGO_BEZIER
#define GGO_BEZIER 3
#endif
#ifndef GGI_MARK_NONEXISTING_GLYPHS
#define GGI_MARK_NONEXISTING_GLYPHS 1
#endif

typedef struct { HDC dc; HBITMAP bmp, obmp; HFONT font, ofont; int h; char face[64]; } GC;

static int gc_wine(void)
{
    HMODULE n = GetModuleHandleA("ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version") != NULL;
}

static const char *const gc_faces[] = { "Arial", "Times New Roman", "Courier New", "Segoe UI", "Symbol", "Marlett", "Consolas", "Tahoma" };
#define GC_NFACES ((int)(sizeof gc_faces / sizeof gc_faces[0]))
static const int gc_heights[] = { -8, -12, -16, -24, -48, -100 };
#define GC_NHEIGHTS ((int)(sizeof gc_heights / sizeof gc_heights[0]))

static int gc_is_symbolic(const char *face) { return !strcmp(face, "Symbol") || !strcmp(face, "Marlett"); }

static HFONT gc_mkfont(const char *face, int h, int weight, int charset, int width)
{
    LOGFONTA lf;
    if (!U(CreateFontIndirectA)) return NULL;
    memset(&lf, 0, sizeof lf);
    lf.lfHeight = h; lf.lfWidth = width; lf.lfWeight = weight; lf.lfCharSet = (BYTE)charset;
    lf.lfOutPrecision = OUT_TT_PRECIS; lf.lfClipPrecision = CLIP_DEFAULT_PRECIS; lf.lfQuality = DEFAULT_QUALITY;
    lstrcpynA(lf.lfFaceName, face, LF_FACESIZE);
    return U(CreateFontIndirectA)(&lf);
}

/* Memory DC with a 32bpp DIB section and the requested font selected. Returns 0 (no skip) if the face is unavailable. */
static int gc_open_ex(GC *g, const char *face, int h, int weight, int charset, int width)
{
    BITMAPINFO bi; void *bits = NULL; char got[80];
    memset(g, 0, sizeof *g);
    if (!U(CreateCompatibleDC) || !U(CreateDIBSection) || !U(SelectObject) || !U(DeleteObject) || !U(GetTextFaceA) || !U(DeleteDC)) return 0;
    g->dc = U(CreateCompatibleDC)(NULL);
    if (!g->dc) return 0;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 16; bi.bmiHeader.biHeight = -16;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    g->bmp = U(CreateDIBSection)(g->dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (g->bmp) g->obmp = (HBITMAP)U(SelectObject)(g->dc, g->bmp);
    g->font = gc_mkfont(face, h, weight, charset, width);
    if (!g->font) goto fail;
    g->ofont = (HFONT)U(SelectObject)(g->dc, g->font);
    got[0] = 0;
    U(GetTextFaceA)(g->dc, sizeof got, got);
    if (lstrcmpiA(got, face)) { t_note("font '%s' not available (got '%s')", face, got); goto fail; }
    g->h = h; lstrcpynA(g->face, face, sizeof g->face);
    return 1;
fail:
    if (g->ofont) U(SelectObject)(g->dc, g->ofont);
    if (g->font) U(DeleteObject)(g->font);
    if (g->obmp) U(SelectObject)(g->dc, g->obmp);
    if (g->bmp) U(DeleteObject)(g->bmp);
    U(DeleteDC)(g->dc);
    memset(g, 0, sizeof *g);
    return 0;
}
static int gc_open(GC *g, const char *face, int h)
{
    return gc_open_ex(g, face, h, FW_NORMAL, gc_is_symbolic(face) ? SYMBOL_CHARSET : ANSI_CHARSET, 0);
}
/* Replace the font on an open GC. */
static int gc_setfont(GC *g, HFONT f)
{
    HFONT old = (HFONT)U(SelectObject)(g->dc, f);
    if (!old) return 0;
    if (g->font && g->font != f) U(DeleteObject)(g->font);
    g->font = f;
    return 1;
}
static void gc_close(GC *g)
{
    if (!g->dc) return;
    if (g->ofont) U(SelectObject)(g->dc, g->ofont);
    if (g->font) U(DeleteObject)(g->font);
    if (g->obmp) U(SelectObject)(g->dc, g->obmp);
    if (g->bmp) U(DeleteObject)(g->bmp);
    U(DeleteDC)(g->dc);
    memset(g, 0, sizeof *g);
}

static int gc_gdicount(void)
{
    if (!U(GetGuiResources) || gc_wine()) return -1; /* wine stub misbehaves */
    return (int)U(GetGuiResources)(GetCurrentProcess(), 0 /* GR_GDIOBJECTS */);
}

/* ---- TrueType table helpers (via GetFontData) ---- */
static unsigned gc_be16(const BYTE *p) { return (p[0] << 8) | p[1]; }
static int gc_be16s(const BYTE *p) { return (short)((p[0] << 8) | p[1]); }
static unsigned gc_be32(const BYTE *p) { return ((unsigned)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

static BYTE *gc_table(HDC dc, const char *tag, DWORD *len)
{
    DWORD t, n; BYTE *b;
    if (!U(GetFontData)) return NULL;
    memcpy(&t, tag, 4);
    n = U(GetFontData)(dc, t, 0, NULL, 0);
    if (n == GDI_ERROR || n == 0 || n > 0x800000) return NULL;
    b = (BYTE *)malloc(n);
    if (!b) return NULL;
    if (U(GetFontData)(dc, t, 0, b, n) != n) { free(b); return NULL; }
    if (len) *len = n;
    return b;
}

/* Glyph id for a BMP code point, 0 if missing. Handles (3,1) fmt 4, (3,10) fmt 12, (3,0) symbol fmt 4. */
static unsigned gc_cmap(const BYTE *cm, DWORD len, unsigned ch)
{
    unsigned n, i, off = 0, enc = 99;
    if (!cm || len < 4) return 0;
    n = gc_be16(cm + 2);
    for (i = 0; i < n && 4 + i * 8 + 8 <= len; i++) {
        unsigned pid = gc_be16(cm + 4 + i * 8), eid = gc_be16(cm + 6 + i * 8);
        unsigned o = gc_be32(cm + 8 + i * 8);
        if (pid == 3 && eid == 10) { off = o; enc = 10; break; }
        if (pid == 3 && eid == 1 && enc != 10) { off = o; enc = 1; }
        if (pid == 3 && eid == 0 && enc == 99) { off = o; enc = 0; }
    }
    if (enc == 99 || off + 4 > len) return 0;
    if (enc == 0) ch += 0xF000;
    {
        unsigned fmt = gc_be16(cm + off);
        if (fmt == 4) {
            unsigned segx2 = gc_be16(cm + off + 6), s;
            const BYTE *endc = cm + off + 14, *startc = endc + segx2 + 2, *delta = startc + segx2, *ro = delta + segx2;
            if (off + 16 + segx2 * 4 > len) return 0;
            for (s = 0; s < segx2 / 2; s++) {
                if (ch <= gc_be16(endc + s * 2)) {
                    unsigned st = gc_be16(startc + s * 2), r;
                    if (ch < st) return 0;
                    r = gc_be16(ro + s * 2);
                    if (!r) return (ch + gc_be16(delta + s * 2)) & 0xFFFF;
                    {
                        const BYTE *p = ro + s * 2 + r + (ch - st) * 2;
                        unsigned g;
                        if (p + 2 > cm + len) return 0;
                        g = gc_be16(p);
                        return g ? (g + gc_be16(delta + s * 2)) & 0xFFFF : 0;
                    }
                }
            }
            return 0;
        } else if (fmt == 12) {
            unsigned ng = gc_be32(cm + off + 12), s;
            for (s = 0; s < ng && off + 16 + s * 12 + 12 <= len; s++) {
                unsigned a = gc_be32(cm + off + 16 + s * 12), b = gc_be32(cm + off + 20 + s * 12), g = gc_be32(cm + off + 24 + s * 12);
                if (ch >= a && ch <= b) return g + (ch - a);
            }
        }
    }
    return 0;
}

static int gc_upem(HDC dc)
{
    DWORD l; BYTE *h = gc_table(dc, "head", &l); int u = 0;
    if (h && l >= 20) u = gc_be16(h + 18);
    free(h);
    return u;
}

/* Kerning value from the 'kern' table (format 0 subtables) for a glyph pair; returns 1 if found. */
static int gc_kern_lookup(const BYTE *k, DWORD len, unsigned l, unsigned r, int *val)
{
    unsigned n, t, off = 4;
    if (!k || len < 4) return 0;
    n = gc_be16(k + 2);
    for (t = 0; t < n && off + 14 <= len; t++) {
        unsigned slen = gc_be16(k + off + 2), cov = gc_be16(k + off + 4);
        if ((cov >> 8) == 0 && (cov & 1)) {
            unsigned np = gc_be16(k + off + 6), lo = 0, hi = np;
            unsigned key = (l << 16) | r;
            while (lo < hi) {
                unsigned mid = (lo + hi) / 2, mk;
                const BYTE *p = k + off + 14 + mid * 6;
                if (p + 6 > k + len) break;
                mk = (gc_be16(p) << 16) | gc_be16(p + 2);
                if (mk == key) { *val = gc_be16s(p + 4); return 1; }
                if (mk < key) lo = mid + 1; else hi = mid;
            }
        }
        if (!slen) break;
        off += slen;
    }
    return 0;
}

#define GC_REQ_BASE() do { REQUIRE(U(CreateCompatibleDC)); REQUIRE(U(CreateDIBSection)); REQUIRE(U(CreateFontIndirectA)); \
    REQUIRE(U(SelectObject)); REQUIRE(U(DeleteObject)); REQUIRE(U(DeleteDC)); REQUIRE(U(GetTextFaceA)); } while (0)

#define GC_ERRSENT 0xDEADBEEFu
#define GC_FIXED(v) ((FIXED){ 0, (short)(v) })
static MAT2 gc_ident(void) { MAT2 m = { {0,1}, {0,0}, {0,0}, {0,1} }; return m; }

#endif
