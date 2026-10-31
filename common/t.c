#include "t.h"
#include <setjmp.h>

static HMODULE g_u32;
static int g_pass, g_fail, g_skip, g_cur_fail, g_cur_skip;
static const char *g_cur;
static jmp_buf g_jb;
static volatile int g_in_test;

HMODULE t_user32(void) { if (!g_u32) g_u32 = LoadLibraryA("user32.dll"); return g_u32; }
static HMODULE g_gdi, g_full;
HMODULE t_gdi32(void) { if (!g_gdi) g_gdi = LoadLibraryA("gdi32.dll"); return g_gdi; }
HMODULE t_gdi32full(void) { if (!g_full) g_full = LoadLibraryA("gdi32full.dll"); return g_full; }
/* Lookup order: gdi32 (public face, incl. forwarders), gdi32full, then user32. */
FARPROC t_proc(const char *n) {
    FARPROC p = GetProcAddress(t_gdi32(), n);
    if (!p && t_gdi32full()) p = GetProcAddress(g_full, n);
    if (!p) p = GetProcAddress(t_user32(), n);
    return p;
}
FARPROC t_proc_gdi32(const char *n) { return GetProcAddress(t_gdi32(), n); }
FARPROC t_proc_gdi32full(const char *n) { return t_gdi32full() ? GetProcAddress(g_full, n) : NULL; }
FARPROC t_proc_ord(unsigned o) { return GetProcAddress(t_gdi32(), (LPCSTR)(ULONG_PTR)o); }

#define MAXT 8192
static struct { const char *n; t_fn f; } g_t[MAXT];
static int g_nt;
void t_register(const char *n, t_fn f) { if (g_nt < MAXT) { g_t[g_nt].n = n; g_t[g_nt].f = f; g_nt++; } }

void t_check(int ok, const char *file, int line, const char *expr, const char *fmt, ...) {
    if (ok) { g_pass++; return; }
    g_fail++; g_cur_fail++;
    printf("  FAIL %s:%d: %s", file, line, expr);
    if (fmt) { va_list a; va_start(a, fmt); printf(" ("); vprintf(fmt, a); printf(")"); va_end(a); }
    printf("\n");
}
void t_note(const char *fmt, ...) { va_list a; va_start(a, fmt); printf("  NOTE "); vprintf(fmt, a); printf("\n"); va_end(a); }
void t_skip(const char *fmt, ...) {
    va_list a; va_start(a, fmt); printf("  SKIP "); vprintf(fmt, a); printf("\n"); va_end(a);
    g_cur_skip = 1;
}

static LONG CALLBACK veh(PEXCEPTION_POINTERS ep) {
    DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (!g_in_test) return EXCEPTION_CONTINUE_SEARCH;
    if (c == 0x40010006 || c == 0x406D1388 || c == DBG_PRINTEXCEPTION_C || c == 0x4001000A) return EXCEPTION_CONTINUE_SEARCH;
    if ((c & 0xF0000000) != 0xC0000000) return EXCEPTION_CONTINUE_SEARCH;
    printf("  FAIL exception 0x%08lx at %p in %s\n", c, ep->ExceptionRecord->ExceptionAddress, g_cur);
    g_fail++; g_cur_fail++;
    g_in_test = 0;
    longjmp(g_jb, 1);
    return EXCEPTION_CONTINUE_SEARCH;
}

static const char *g_cls = "T32TestClass";
static LRESULT CALLBACK defproc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcA(h, m, w, l); }
const char *t_cls(void) {
    static int done;
    if (!done) {
        WNDCLASSA wc; memset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = defproc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = g_cls;
        wc.hCursor = NULL; wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        ATOM (WINAPI *rc)(const WNDCLASSA *) = (void *)t_proc("RegisterClassA");
        if (rc) rc(&wc);
        done = 1;
    }
    return g_cls;
}
HWND t_make_window(const char *cls, DWORD st, DWORD ex, int x, int y, int w, int h, HWND parent) {
    HWND (WINAPI *cw)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID) = (void *)t_proc("CreateWindowExA");
    if (!cw) return NULL;
    if (!cls) cls = t_cls();
    return cw(ex, cls, "t", st, x, y, w, h, parent, NULL, GetModuleHandleA(NULL), NULL);
}
void t_pump(void) {
    BOOL (WINAPI *pm)(LPMSG, HWND, UINT, UINT, UINT) = (void *)t_proc("PeekMessageA");
    BOOL (WINAPI *tm)(const MSG *) = (void *)t_proc("TranslateMessage");
    LRESULT (WINAPI *dm)(const MSG *) = (void *)t_proc("DispatchMessageA");
    MSG m; int n = 0;
    if (!pm) return;
    while (n++ < 10000 && pm(&m, NULL, 0, 0, PM_REMOVE)) { if (tm) tm(&m); if (dm) dm(&m); }
}

int main(int argc, char **argv) {
    const char *filter = argc > 1 ? argv[1] : NULL;
    int ran = 0, tskip = 0, tfail = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    AddVectoredExceptionHandler(1, veh);
    if (!t_user32() || !t_gdi32()) { printf("FATAL cannot load user32/gdi32\n"); return 2; }
    for (int i = 0; i < g_nt; i++) {
        if (filter && !strstr(g_t[i].n, filter)) continue;
        g_cur = g_t[i].n; g_cur_fail = g_cur_skip = 0;
        printf("RUN  %s\n", g_cur);
        SetLastError(0);
        if (!setjmp(g_jb)) { g_in_test = 1; g_t[i].f(); g_in_test = 0; }
        ran++;
        if (g_cur_fail) tfail++;
        else if (g_cur_skip) tskip++;
        printf("%s %s\n", g_cur_fail ? "FAIL" : g_cur_skip ? "SKIP" : "OK  ", g_cur);
    }
    printf("SUMMARY tests=%d failed=%d skipped=%d checks_pass=%d checks_fail=%d\n", ran, tfail, tskip, g_pass, g_fail);
    return tfail ? 1 : 0;
}
