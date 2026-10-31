/* Minimal test harness. user32 is never linked: every call goes through
 * LoadLibrary/GetProcAddress. Signatures come from the headers via typeof. */
#ifndef T_H
#define T_H
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stddef.h>

/* Resolve an export; returns NULL (and records it) if absent. */
FARPROC t_proc(const char *name);
FARPROC t_proc_ord(unsigned ordinal);
HMODULE t_user32(void);
HMODULE t_gdi32(void);
HMODULE t_gdi32full(void);
FARPROC t_proc_gdi32(const char *name);      /* gdi32.dll export only (forwarders resolve) */
FARPROC t_proc_gdi32full(const char *name);  /* gdi32full.dll export only */
#define UG(name)  ((__typeof__(&name))t_proc_gdi32(#name))
#define UF(name)  ((__typeof__(&name))t_proc_gdi32full(#name))

/* U(CreateDCW)(...) : pointer typed from the SDK declaration; resolved gdi32 -> gdi32full -> user32.
 * user32/gdi32 are never linked. */
#define U(name) ((__typeof__(&name))t_proc(#name))
/* Declare a pointer type for functions not in the SDK headers. */
#define UDECL(ret, name, args) typedef ret (WINAPI *name##_pfn) args
#define UN(name) ((name##_pfn)t_proc(#name))

/* Test registration: TEST(area_name) { ... } */
typedef void (*t_fn)(void);
void t_register(const char *name, t_fn fn);
#define TEST(n) \
    static void test_##n(void); \
    static void __attribute__((constructor)) reg_##n(void) { t_register(#n, test_##n); } \
    static void test_##n(void)

/* Assertions: all non-fatal, counted, printed on failure. */
void t_check(int ok, const char *file, int line, const char *expr, const char *fmt, ...);
#define CHECK(c)            t_check(!!(c), __FILE__, __LINE__, #c, NULL)
#define CHECKF(c, ...)      t_check(!!(c), __FILE__, __LINE__, #c, __VA_ARGS__)
#define EXPECT_EQ(a, b)     t_check((ULONG_PTR)(a) == (ULONG_PTR)(b), __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)(ULONG_PTR)(a), (unsigned long long)(ULONG_PTR)(b))
#define EXPECT_NE(a, b)     t_check((ULONG_PTR)(a) != (ULONG_PTR)(b), __FILE__, __LINE__, #a " != " #b, "got 0x%llx", (unsigned long long)(ULONG_PTR)(a))
#define EXPECT_STR(a, b)    t_check(!strcmp((a), (b)), __FILE__, __LINE__, #a " == " #b, "got \"%s\" want \"%s\"", (a), (b))
#define EXPECT_WSTR(a, b)   t_check(!lstrcmpW((a), (b)), __FILE__, __LINE__, #a " == " #b, "wide string mismatch")
/* Call result + GetLastError expectation: failure-path checks. */
#define EXPECT_ERR(call_result_zero_or_null, err) do { \
    DWORD e_ = GetLastError(); \
    t_check(e_ == (DWORD)(err), __FILE__, __LINE__, #err, "GetLastError got %lu want %lu", e_, (DWORD)(err)); } while (0)
#define SET_ERR(e)          SetLastError(e)
/* Behaviour that differs between Windows versions / is observed rather than
 * documented: logged, never fails the run. */
void t_note(const char *fmt, ...);
/* Skip the rest of the current test (missing export, no desktop, ...). */
void t_skip(const char *fmt, ...);
#define REQUIRE(ptr) do { if (!(ptr)) { t_skip("missing export: " #ptr); return; } } while (0)

/* Helpers */
HWND t_make_window(const char *cls, DWORD style, DWORD exstyle, int x, int y, int w, int h, HWND parent); /* class registered on demand with a default proc */
void t_pump(void);                 /* drain this thread's queue (PeekMessage loop) */
const char *t_cls(void);           /* name of the default test window class */

#endif
