/* Shared helpers for the 16-bit metafile tests. */
#ifndef WMF_H
#define WMF_H
#include "t.h"
#include <stdlib.h>

#pragma pack(push, 1)
typedef struct {
    WORD  type, hdrsize, ver;
    DWORD size;           /* in WORDs */
    WORD  nobj;
    DWORD maxrec;         /* in WORDs */
    WORD  nparm;
} WMFHDR;
typedef struct {
    DWORD key; WORD hmf; SHORT l, t, r, b; WORD inch; DWORD res; WORD sum;
} APMHDR;
#pragma pack(pop)
#define APM_KEY 0x9AC6CDD7u

/* Bit buffer of a metafile. */
typedef struct { BYTE *p; UINT n; } WB;

static inline DWORD r_size(const BYTE *r) { DWORD d; memcpy(&d, r, 4); return d; }
static inline WORD  r_func(const BYTE *r) { WORD w; memcpy(&w, r + 4, 2); return w; }
static inline WORD  r_parm(const BYTE *r, int i) { WORD w; memcpy(&w, r + 6 + 2 * i, 2); return w; }
static inline SHORT r_sparm(const BYTE *r, int i) { return (SHORT)r_parm(r, i); }
static inline DWORD r_dparm(const BYTE *r, int i) { DWORD d; memcpy(&d, r + 6 + 2 * i, 4); return d; }

static inline void wb_free(WB *b) { free(b->p); b->p = NULL; b->n = 0; }

/* Fetch the bits of a memory metafile handle (size via GetMetaFileBitsEx). */
static BYTE *wb_get(HMETAFILE h, WB *b) {
    UINT (WINAPI *gb)(HMETAFILE, UINT, LPVOID) = U(GetMetaFileBitsEx);
    b->p = NULL; b->n = 0;
    if (!gb || !h) return NULL;
    UINT n = gb(h, 0, NULL);
    if (!n) return NULL;
    b->p = malloc(n);
    if (!b->p) return NULL;
    b->n = gb(h, n, b->p);
    return b->p;
}

static inline const WMFHDR *wb_hdr(const WB *b) { return (const WMFHDR *)b->p; }
/* First record, or NULL. */
static inline const BYTE *wb_first(const WB *b) { return b->n >= 0x12 + 6 ? b->p + 0x12 : NULL; }
/* Next record after r; NULL at the end of the buffer or on a malformed record. */
static const BYTE *wb_next(const WB *b, const BYTE *r) {
    DWORD s = r_size(r);
    if (s < 3) return NULL;
    const BYTE *n = r + (size_t)s * 2;
    if (n + 6 > b->p + b->n) return NULL;
    return n;
}
static int wb_nrecs(const WB *b) {
    int n = 0;
    for (const BYTE *r = wb_first(b); r; r = wb_next(b, r)) { n++; if (r_func(r) == 0) break; }
    return n;
}
/* nth (0-based) record with the given function, or NULL. */
static const BYTE *wb_find(const WB *b, WORD fn, int nth) {
    for (const BYTE *r = wb_first(b); r; r = wb_next(b, r)) {
        if (r_func(r) == fn && nth-- == 0) return r;
        if (r_func(r) == 0) break;
    }
    return NULL;
}
static int wb_count(const WB *b, WORD fn) {
    int n = 0;
    while (wb_find(b, fn, n)) n++;
    return n;
}
/* Structural validation: header fields, record walk, trailing EOF, sizes. */
static int wb_valid(const WB *b) {
    if (!b->p || b->n < 0x12 + 6) return 0;
    const WMFHDR *h = wb_hdr(b);
    if (h->hdrsize != 9 || h->ver != 0x300 || h->nparm != 0) return 0;
    if (h->size * 2 != b->n) return 0;
    DWORD mx = 0; const BYTE *last = NULL; UINT total = 18;
    for (const BYTE *r = wb_first(b); r; r = wb_next(b, r)) {
        DWORD s = r_size(r);
        if (s > mx) mx = s;
        total += s * 2;
        last = r;
        if (r_func(r) == 0) break;
    }
    if (!last || r_func(last) != 0 || r_size(last) != 3) return 0;
    if (total != b->n) return 0;
    if (h->maxrec != mx) return 0;
    return 1;
}

/* Create a memory metafile DC. */
static HDC mf_new(void) { return U(CreateMetaFileA) ? U(CreateMetaFileA)(NULL) : NULL; }

/* Close a DC and fetch bits (consumes the HMETAFILE unless keep != NULL). */
static BOOL mf_finish(HDC dc, WB *b, HMETAFILE *keep) {
    HMETAFILE h = U(CloseMetaFile)(dc);
    if (!h) { b->p = NULL; b->n = 0; return FALSE; }
    wb_get(h, b);
    if (keep) *keep = h; else U(DeleteMetaFile)(h);
    return b->p != NULL;
}

static DWORD gdi_count(void) {
    DWORD (WINAPI *g)(HANDLE, DWORD) = U(GetGuiResources);
    return g ? g(GetCurrentProcess(), 0) : 0;
}

/* Wine detection for calls that abort under that emulator. */
static int on_wine(void) {
    return GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version") != NULL;
}

/* Temp-file helpers. */
static void tmp_path(char *out, const char *leaf) {
    char d[MAX_PATH]; GetTempPathA(MAX_PATH, d);
    snprintf(out, MAX_PATH, "%s%s_%lu", d, leaf, GetCurrentProcessId());
}
static BYTE *file_read(const char *path, UINT *n) {
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    *n = 0;
    if (f == INVALID_HANDLE_VALUE) return NULL;
    DWORD sz = GetFileSize(f, NULL), got = 0;
    BYTE *p = malloc(sz ? sz : 1);
    if (p) ReadFile(f, p, sz, &got, NULL);
    CloseHandle(f);
    *n = got;
    return p;
}

/* DIB-section memory DC for deterministic rendering. */
typedef struct { HDC dc; HBITMAP bmp, old; DWORD *bits; int w, h; } DIBDC;
static int dibdc_make(DIBDC *d, int w, int h) {
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    memset(d, 0, sizeof *d);
    d->w = w; d->h = h;
    d->dc = U(CreateCompatibleDC)(NULL);
    if (!d->dc) return 0;
    d->bmp = U(CreateDIBSection)(d->dc, &bi, DIB_RGB_COLORS, (void **)&d->bits, NULL, 0);
    if (!d->bmp) { U(DeleteDC)(d->dc); d->dc = NULL; return 0; }
    d->old = U(SelectObject)(d->dc, d->bmp);
    memset(d->bits, 0xFF, (size_t)w * h * 4);
    return 1;
}
static void dibdc_free(DIBDC *d) {
    if (!d->dc) return;
    U(SelectObject)(d->dc, d->old);
    U(DeleteObject)(d->bmp);
    U(DeleteDC)(d->dc);
    d->dc = NULL;
}
static void dibdc_clear(DIBDC *d, DWORD v) {
    for (int i = 0; i < d->w * d->h; i++) d->bits[i] = v;
}
static int dibdc_diff(const DIBDC *a, const DIBDC *b) {
    int n = 0;
    for (int i = 0; i < a->w * a->h; i++) n += ((a->bits[i] ^ b->bits[i]) & 0xFFFFFF) != 0;
    return n;
}
static int dibdc_nonwhite(const DIBDC *a) {
    int n = 0;
    for (int i = 0; i < a->w * a->h; i++) n += (a->bits[i] & 0xFFFFFF) != 0xFFFFFF;
    return n;
}
#define DIBPX(d, x, y) ((d)->bits[(y) * (d)->w + (x)] & 0xFFFFFF)

#define WMF_REQ_BASE() \
    REQUIRE(U(CreateMetaFileA)); REQUIRE(U(CloseMetaFile)); REQUIRE(U(DeleteMetaFile)); \
    REQUIRE(U(GetMetaFileBitsEx))

#endif
