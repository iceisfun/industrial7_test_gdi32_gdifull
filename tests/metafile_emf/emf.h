/* Shared helpers for the enhanced-metafile tests. */
#ifndef EMF_H
#define EMF_H
#include "t.h"
#include <stdlib.h>

/* Observed-but-unverified expectation: logged, never fails. */
#define SOFT(c) do { if (!(c)) t_note("%s:%d soft expectation not met: %s", __FILE__, __LINE__, #c); } while (0)

#define E_ERR_INVALID_HANDLE 6
#define E_ERR_INVALID_DATA 13
#define E_ERR_INVALID_PARAM 87
#define E_ERR_INSUFFICIENT 122
#define E_ERR_FILENAME_EXCED 206

static int emf_wine(void) {
    HMODULE n = GetModuleHandleA("ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version") != NULL;
}

/* ---- GDI object accounting ---- */
static DWORD emf_gdi_count(void) {
    DWORD (WINAPI *g)(HANDLE, DWORD) = (void *)t_proc("GetGuiResources");
    return g ? g(GetCurrentProcess(), 0 /*GR_GDIOBJECTS*/) : 0;
}

/* ---- creation helpers ---- */
static HDC emf_begin(void) { return U(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL); }
static HENHMETAFILE emf_end(HDC dc) { return U(CloseEnhMetaFile)(dc); }

/* A 32bpp top-down DIB section memory DC of w x h, filled with `fill`.
 * Returns the DC; *pbits gets the pixels. Free with emf_dib_free. */
typedef struct { HDC dc; HBITMAP bmp, old; DWORD *bits; int w, h; } emf_dib;
static int emf_dib_make(emf_dib *d, int w, int h, DWORD fill) {
    BITMAPINFO bi; void *bits = NULL;
    memset(d, 0, sizeof *d);
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    d->dc = U(CreateCompatibleDC)(NULL);
    if (!d->dc) return 0;
    d->bmp = U(CreateDIBSection)(d->dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!d->bmp) { U(DeleteDC)(d->dc); d->dc = NULL; return 0; }
    d->old = U(SelectObject)(d->dc, d->bmp);
    d->bits = bits; d->w = w; d->h = h;
    for (int i = 0; i < w * h; i++) d->bits[i] = fill;
    return 1;
}
static void emf_dib_free(emf_dib *d) {
    if (!d->dc) return;
    U(SelectObject)(d->dc, d->old);
    U(DeleteObject)(d->bmp);
    U(DeleteDC)(d->dc);
    memset(d, 0, sizeof *d);
}
static DWORD emf_px(const emf_dib *d, int x, int y) { return d->bits[y * d->w + x] & 0xFFFFFF; }
static void emf_dib_fill(emf_dib *d, DWORD v) { for (int i = 0; i < d->w * d->h; i++) d->bits[i] = v; }
#define BGR(r,g,b) ((DWORD)(((b) << 0) | ((g) << 8) | ((r) << 16)))   /* 0xRRGGBB as stored by a 32bpp DIB */

/* ---- record log via EnumEnhMetaFile ---- */
#define MAXLOG 4096
typedef struct {
    int n;
    DWORD type[MAXLOG];
    DWORD size[MAXLOG];
    int nobj;           /* handle-table size reported to callback */
    LPARAM seen_param;
    int stop_at;        /* return 0 when n reaches this (0: never) */
    int bad_dc;         /* callback got a DC different from the enum DC */
    HDC want_dc;
    int dparm0_valid;
} emf_log;

static int CALLBACK emf_log_cb(HDC dc, HANDLETABLE *ht, const ENHMETARECORD *r, int nobj, LPARAM lp) {
    emf_log *l = (emf_log *)lp;
    l->seen_param = lp;
    l->nobj = nobj;
    if (l->want_dc && dc != l->want_dc) l->bad_dc++;
    if (l->n < MAXLOG) { l->type[l->n] = r->iType; l->size[l->n] = r->nSize; }
    l->n++;
    if (l->stop_at && l->n >= l->stop_at) return 0;
    return 1;
}

/* Enumerate hemf with a scratch memory DC; returns EnumEnhMetaFile's result. */
static BOOL emf_enum(HENHMETAFILE h, emf_log *l) {
    HDC dc = U(CreateCompatibleDC)(NULL);
    RECT rc = {0, 0, 100, 100};
    BOOL r;
    memset(l, 0, sizeof *l);
    l->want_dc = dc;
    r = U(EnumEnhMetaFile)(dc, h, emf_log_cb, l, &rc);
    U(DeleteDC)(dc);
    return r;
}

/* ---- raw bits ---- */
static BYTE *emf_bits(HENHMETAFILE h, UINT *pn) {
    UINT n = U(GetEnhMetaFileBits)(h, 0, NULL);
    BYTE *b;
    if (pn) *pn = n;
    if (!n) return NULL;
    b = malloc(n);
    if (b && U(GetEnhMetaFileBits)(h, n, b) != n) { free(b); return NULL; }
    return b;
}
/* nth (0-based) record of `type` in a bits buffer, or NULL. Walks safely. */
static const ENHMETARECORD *emf_find(const BYTE *b, UINT n, DWORD type, int nth) {
    UINT off = 0;
    while (off + 8 <= n) {
        const ENHMETARECORD *r = (const ENHMETARECORD *)(b + off);
        if (r->nSize < 8 || (r->nSize & 3) || off + r->nSize > n) return NULL;
        if (r->iType == type && nth-- == 0) return r;
        off += r->nSize;
    }
    return NULL;
}
static int emf_count_recs(const BYTE *b, UINT n) {
    UINT off = 0; int c = 0;
    while (off + 8 <= n) {
        const ENHMETARECORD *r = (const ENHMETARECORD *)(b + off);
        if (r->nSize < 8 || (r->nSize & 3) || off + r->nSize > n) return -1;
        off += r->nSize; c++;
    }
    return off == n ? c : -1;
}

/* Is `exp` (n entries) a subsequence of the logged types? */
static int emf_has_seq(const emf_log *l, const DWORD *exp, int n) {
    int j = 0;
    for (int i = 0; i < l->n && i < MAXLOG && j < n; i++) if (l->type[i] == exp[j]) j++;
    return j == n;
}

/* Hand-built minimal EMF: header of hdrsize (88/100/108) + EOF. Returns total bytes. */
static UINT emf_build_min(BYTE *buf, UINT hdrsize) {
    ENHMETAHEADER h; DWORD eof[5];
    memset(&h, 0, sizeof h);
    h.iType = EMR_HEADER; h.nSize = hdrsize;
    h.rclBounds.left = 0; h.rclBounds.top = 0; h.rclBounds.right = -1; h.rclBounds.bottom = -1;
    h.rclFrame.left = 0; h.rclFrame.top = 0; h.rclFrame.right = -1; h.rclFrame.bottom = -1;
    h.dSignature = ENHMETA_SIGNATURE; h.nVersion = 0x10000;
    h.nBytes = hdrsize + 20; h.nRecords = 2; h.nHandles = 1;
    h.szlDevice.cx = 1024; h.szlDevice.cy = 768; h.szlMillimeters.cx = 320; h.szlMillimeters.cy = 240;
    memset(buf, 0, hdrsize + 20);
    memcpy(buf, &h, hdrsize <= sizeof h ? hdrsize : sizeof h);
    if (hdrsize >= 108) { ((DWORD *)buf)[25] = 320000; ((DWORD *)buf)[26] = 240000; }
    eof[0] = EMR_EOF; eof[1] = 20; eof[2] = 0; eof[3] = 16; eof[4] = 20;
    memcpy(buf + hdrsize, eof, 20);
    return hdrsize + 20;
}

/* temp file names */
static void emf_tmpname(WCHAR *out, const WCHAR *tag) {
    WCHAR dir[MAX_PATH];
    DWORD n = GetTempPathW(MAX_PATH, dir);
    static int ctr;
    if (!n) lstrcpyW(dir, L".\\");
    wsprintfW(out, L"%sg32emf_%lu_%u_%s.emf", dir, GetCurrentProcessId(), ++ctr, tag);
}
static BYTE *emf_readfile(const WCHAR *path, DWORD *pn) {
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    DWORD sz, got; BYTE *b;
    if (f == INVALID_HANDLE_VALUE) return NULL;
    sz = GetFileSize(f, NULL);
    b = malloc(sz ? sz : 1);
    if (!b || !ReadFile(f, b, sz, &got, NULL) || got != sz) { CloseHandle(f); free(b); return NULL; }
    CloseHandle(f);
    *pn = sz; return b;
}
static int emf_writefile(const WCHAR *path, const void *data, DWORD n) {
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD put; int ok;
    if (f == INVALID_HANDLE_VALUE) return 0;
    ok = WriteFile(f, data, n, &put, NULL) && put == n;
    CloseHandle(f);
    return ok;
}
#endif
