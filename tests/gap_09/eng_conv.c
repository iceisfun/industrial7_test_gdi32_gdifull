/* EngWideCharToMultiByte / EngUnicodeToMultiByteN.
 *  EngWideCharToMultiByte(cp, wsz, cbBytes, out, cbOut) -> WideCharToMultiByte(cp, 0, wsz, cbBytes >> 1, out, cbOut, NULL, NULL)
 *  (shift is a 64-bit logical shift of the sign-extended count, truncated to int)
 *  EngUnicodeToMultiByteN is RtlUnicodeToMultiByteN (ANSI code page). */
#include "gap9.h"

typedef int (WINAPI *ewc_fn)(UINT, const WCHAR *, int, char *, int);
typedef LONG (WINAPI *eum_fn)(char *, ULONG, ULONG *, const WCHAR *, ULONG);

static ewc_fn EWC(void) { return (ewc_fn)t_proc_gdi32("EngWideCharToMultiByte"); }
static ewc_fn EWCF(void) { return (ewc_fn)t_proc_gdi32full("EngWideCharToMultiByte"); }
static eum_fn EUM(void) { return (eum_fn)t_proc_gdi32("EngUnicodeToMultiByteN"); }
static eum_fn EUMF(void) { return (eum_fn)t_proc_gdi32full("EngUnicodeToMultiByteN"); }

static const WCHAR hello[] = L"Hello";

TEST(gap9_engwc_ascii_exact)
{
    ewc_fn f = EWC(); REQUIRE(f);
    char buf[16]; memset(buf, 0x55, sizeof buf);
    SET_ERR(0);
    EXPECT_EQ(f(1252, hello, 10, buf, sizeof buf), 5);
    CHECK(!memcmp(buf, "Hello", 5));
    EXPECT_EQ((BYTE)buf[5], 0x55);   /* no terminator written for counted input */
}

TEST(gap9_engwc_query_size_null_buffer)
{
    ewc_fn f = EWC(); REQUIRE(f);
    SET_ERR(0);
    EXPECT_EQ(f(1252, hello, 10, NULL, 0), 5);
    EXPECT_EQ(f(CP_UTF8, hello, 10, NULL, 0), 5);
    EXPECT_EQ(f(CP_ACP, hello, 10, NULL, 0), 5);
    EXPECT_EQ(f(CP_OEMCP, hello, 10, NULL, 0), 5);
}

TEST(gap9_engwc_buffer_too_small)
{
    ewc_fn f = EWC(); REQUIRE(f);
    char buf[16];
    for (int cb = 1; cb < 5; cb++) {
        memset(buf, 0x55, sizeof buf);
        SET_ERR(0);
        int r = f(1252, hello, 10, buf, cb);
        EXPECT_EQ(r, 0);
        EXPECT_ERR(r, ERROR_INSUFFICIENT_BUFFER);
    }
    memset(buf, 0x55, sizeof buf);
    SET_ERR(0);
    EXPECT_EQ(f(1252, hello, 10, buf, 5), 5);   /* exact fit */
}

TEST(gap9_engwc_byte_count_is_halved)
{
    ewc_fn f = EWC(); REQUIRE(f);
    char buf[16];
    for (int chars = 1; chars <= 5; chars++) {
        memset(buf, 0, sizeof buf);
        EXPECT_EQ(f(1252, hello, chars * 2, buf, sizeof buf), chars);
        CHECK(!memcmp(buf, "Hello", chars));
    }
}

TEST(gap9_engwc_odd_byte_count_rounds_down)
{
    ewc_fn f = EWC(); REQUIRE(f);
    char buf[16]; memset(buf, 0, sizeof buf);
    EXPECT_EQ(f(1252, hello, 7, buf, sizeof buf), 3);
    CHECK(!memcmp(buf, "Hel", 3));
    EXPECT_EQ(f(1252, hello, 9, NULL, 0), 4);
    EXPECT_EQ(f(1252, hello, 3, NULL, 0), 1);
}

TEST(gap9_engwc_zero_count_fails)
{
    ewc_fn f = EWC(); REQUIRE(f);
    char buf[8];
    SET_ERR(0);
    EXPECT_EQ(f(1252, hello, 0, buf, sizeof buf), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_EQ(f(1252, hello, 1, buf, sizeof buf), 0);   /* 1 >> 1 == 0 */
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
}

TEST(gap9_engwc_negative_count_means_nul_terminated)
{
    /* (unsigned 64-bit)(long long)-1 >> 1 == 0x7FFF...FFFF, truncated to int -1: WideCharToMultiByte then
     * converts through the terminator. -2 behaves the same. */
    ewc_fn f = EWC(); REQUIRE(f);
    char buf[16]; memset(buf, 0x55, sizeof buf);
    SET_ERR(0);
    EXPECT_EQ(f(1252, hello, -1, buf, sizeof buf), 6);
    CHECK(!memcmp(buf, "Hello\0", 6));
    memset(buf, 0x55, sizeof buf);
    EXPECT_EQ(f(1252, hello, -2, buf, sizeof buf), 6);
    CHECK(!memcmp(buf, "Hello\0", 6));
    EXPECT_EQ(f(1252, hello, -1, NULL, 0), 6);
}

TEST(gap9_engwc_other_negative_counts_observed)
{
    /* -4 and -6 reach WideCharToMultiByte as -2 and -3: only -1 is documented to mean "NUL-terminated" */
    ewc_fn f = EWC(); REQUIRE(f);
    char buf[16];
    for (int cb = -4; cb >= -8; cb -= 2) {
        SET_ERR(0);
        int r = f(1252, hello, cb, buf, sizeof buf);
        t_note("EngWideCharToMultiByte(cb=%d) = %d err %lu", cb, r, (unsigned long)GetLastError());
    }
    CHECK(1);
}

TEST(gap9_engwc_null_source)
{
    ewc_fn f = EWC(); REQUIRE(f);
    char buf[8];
    SET_ERR(0);
    EXPECT_EQ(f(1252, NULL, 10, buf, sizeof buf), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
}

TEST(gap9_engwc_invalid_codepage)
{
    ewc_fn f = EWC(); REQUIRE(f);
    char buf[8];
    static const UINT bad[] = { 0xFFFF, 99999, 0x12345678 };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        int r = f(bad[i], hello, 10, buf, sizeof buf);
        EXPECT_EQ(r, 0);
        DWORD e = GetLastError();
        CHECKF(e == ERROR_INVALID_PARAMETER, "cp %u err %lu", bad[i], e);
    }
}

TEST(gap9_engwc_latin1_chars_cp1252)
{
    ewc_fn f = EWC(); REQUIRE(f);
    static const WCHAR s[] = { 0x00E9, 0x00FC, 0x00DF, 0x20AC, 0x2022, 0 };
    char buf[8] = { 0 };
    EXPECT_EQ(f(1252, s, 10, buf, sizeof buf), 5);
    EXPECT_EQ((BYTE)buf[0], 0xE9);
    EXPECT_EQ((BYTE)buf[1], 0xFC);
    EXPECT_EQ((BYTE)buf[2], 0xDF);
    EXPECT_EQ((BYTE)buf[3], 0x80);
    EXPECT_EQ((BYTE)buf[4], 0x95);
}

TEST(gap9_engwc_cp437_box_and_accents)
{
    ewc_fn f = EWC(); REQUIRE(f);
    static const WCHAR s[] = { 0x00E9, 0x2500, 0x2588, 0 };
    char buf[8] = { 0 };
    EXPECT_EQ(f(437, s, 6, buf, sizeof buf), 3);
    EXPECT_EQ((BYTE)buf[0], 0x82);
    EXPECT_EQ((BYTE)buf[1], 0xC4);
    EXPECT_EQ((BYTE)buf[2], 0xDB);
}

TEST(gap9_engwc_utf8_multibyte)
{
    ewc_fn f = EWC(); REQUIRE(f);
    static const WCHAR s[] = { 0x0041, 0x00E9, 0x20AC, 0xD83D, 0xDE00, 0 };   /* A e-acute euro grin(surrogate pair) */
    BYTE buf[16] = { 0 };
    EXPECT_EQ(f(CP_UTF8, s, 10, (char *)buf, sizeof buf), 1 + 2 + 3 + 4);
    static const BYTE want[] = { 0x41, 0xC3, 0xA9, 0xE2, 0x82, 0xAC, 0xF0, 0x9F, 0x98, 0x80 };
    CHECK(!memcmp(buf, want, sizeof want));
    EXPECT_EQ(f(CP_UTF8, s, 10, NULL, 0), 10);
    SET_ERR(0);
    EXPECT_EQ(f(CP_UTF8, s, 10, (char *)buf, 9), 0);
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
}

TEST(gap9_engwc_ascii_codepage_unmappable_is_question_mark)
{
    ewc_fn f = EWC(); REQUIRE(f);
    static const WCHAR s[] = { 'a', 0x4E2D, 'b', 0 };
    char buf[8] = { 0 };
    int r = f(20127, s, 6, buf, sizeof buf);
    EXPECT_EQ(r, 3);
    EXPECT_EQ(buf[0], 'a');
    EXPECT_EQ(buf[1], '?');
    EXPECT_EQ(buf[2], 'b');
}

TEST(gap9_engwc_embedded_nul_is_converted)
{
    ewc_fn f = EWC(); REQUIRE(f);
    static const WCHAR s[] = { 'a', 0, 'b', 0 };
    char buf[8]; memset(buf, 0x55, sizeof buf);
    EXPECT_EQ(f(1252, s, 6, buf, sizeof buf), 3);
    EXPECT_EQ(buf[0], 'a'); EXPECT_EQ(buf[1], 0); EXPECT_EQ(buf[2], 'b');
}

TEST(gap9_engwc_zero_out_size_with_buffer_is_query)
{
    ewc_fn f = EWC(); REQUIRE(f);
    char buf[8]; memset(buf, 0x55, sizeof buf);
    EXPECT_EQ(f(1252, hello, 10, buf, 0), 5);
    EXPECT_EQ((BYTE)buf[0], 0x55);
}

TEST(gap9_engwc_negative_out_size_invalid)
{
    ewc_fn f = EWC(); REQUIRE(f);
    char buf[8];
    SET_ERR(0);
    EXPECT_EQ(f(1252, hello, 10, buf, -1), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
}

TEST(gap9_engwc_matches_widechartomultibyte)
{
    ewc_fn f = EWC(); REQUIRE(f);
    static const UINT cps[] = { 1252, 437, 850, CP_UTF8, 1250, 20127, CP_ACP, CP_OEMCP };
    static const WCHAR s[] = { 'T', 'e', 's', 't', 0x00E9, 0x00E4, 0x0416, 0x20AC, '!', 0 };
    for (size_t i = 0; i < sizeof cps / sizeof *cps; i++) {
        char a[64], b[64]; memset(a, 1, sizeof a); memset(b, 2, sizeof b);
        int ra = f(cps[i], s, 18, a, sizeof a);
        int rb = WideCharToMultiByte(cps[i], 0, s, 9, b, sizeof b, NULL, NULL);
        EXPECT_EQ(ra, rb);
        if (ra > 0) CHECKF(!memcmp(a, b, ra), "cp %u", cps[i]);
    }
}

TEST(gap9_engwc_gdi32_and_gdi32full_agree)
{
    ewc_fn a = EWC(), b = EWCF(); REQUIRE(a); REQUIRE(b);
    char x[16] = { 0 }, y[16] = { 0 };
    EXPECT_EQ(a(1252, hello, 10, x, sizeof x), b(1252, hello, 10, y, sizeof y));
    CHECK(!strcmp(x, y));
    EXPECT_EQ(a(1252, hello, 4, NULL, 0), b(1252, hello, 4, NULL, 0));
}

TEST(gap9_engwc_long_string)
{
    ewc_fn f = EWC(); REQUIRE(f);
    WCHAR *w = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, 20001 * sizeof(WCHAR));
    char *o = (char *)HeapAlloc(GetProcessHeap(), 0, 20001);
    REQUIRE(w); REQUIRE(o);
    for (int i = 0; i < 20000; i++) w[i] = (WCHAR)('A' + i % 26);
    w[20000] = 0;
    EXPECT_EQ(f(1252, w, 40000, o, 20001), 20000);
    CHECK(o[19999] == (char)('A' + 19999 % 26));
    EXPECT_EQ(f(1252, w, 40000, NULL, 0), 20000);
    HeapFree(GetProcessHeap(), 0, w); HeapFree(GetProcessHeap(), 0, o);
}

/* ---------------- EngUnicodeToMultiByteN ---------------- */
TEST(gap9_engum_ascii)
{
    eum_fn f = EUM(); REQUIRE(f);
    char buf[16]; memset(buf, 0x55, sizeof buf);
    ULONG n = 0xCCCC;
    LONG st = f(buf, sizeof buf, &n, hello, 10);
    EXPECT_EQ(st, 0);
    EXPECT_EQ(n, 5);
    CHECK(!memcmp(buf, "Hello", 5));
    EXPECT_EQ((BYTE)buf[5], 0x55);
}

TEST(gap9_engum_truncates_without_error)
{
    eum_fn f = EUM(); REQUIRE(f);
    char buf[16];
    for (ULONG mx = 0; mx <= 5; mx++) {
        memset(buf, 0x55, sizeof buf);
        ULONG n = 0xCCCC;
        LONG st = f(buf, mx, &n, hello, 10);
        EXPECT_EQ(st, 0);
        EXPECT_EQ(n, mx);
        CHECK(!memcmp(buf, "Hello", mx));
        EXPECT_EQ((BYTE)buf[mx], 0x55);
    }
}

TEST(gap9_engum_null_result_count)
{
    eum_fn f = EUM(); REQUIRE(f);
    char buf[16]; memset(buf, 0, sizeof buf);
    EXPECT_EQ(f(buf, sizeof buf, NULL, hello, 10), 0);
    CHECK(!strcmp(buf, "Hello"));
}

TEST(gap9_engum_zero_length_source)
{
    eum_fn f = EUM(); REQUIRE(f);
    char buf[4]; memset(buf, 0x55, sizeof buf);
    ULONG n = 0xCCCC;
    EXPECT_EQ(f(buf, sizeof buf, &n, hello, 0), 0);
    EXPECT_EQ(n, 0);
    EXPECT_EQ((BYTE)buf[0], 0x55);
}

TEST(gap9_engum_odd_source_length)
{
    eum_fn f = EUM(); REQUIRE(f);
    char buf[16]; memset(buf, 0, sizeof buf);
    ULONG n = 0xCCCC;
    LONG st = f(buf, sizeof buf, &n, hello, 9);
    t_note("odd byte count: status 0x%08lx n=%lu", (unsigned long)st, (unsigned long)n);
    if (st == 0) EXPECT_EQ(n, 4);
}

TEST(gap9_engum_zero_capacity_writes_nothing)
{
    /* unlike a size query, a zero-capacity call just reports 0 bytes produced */
    eum_fn f = EUM(); REQUIRE(f);
    char buf[4]; memset(buf, 0x55, sizeof buf);
    ULONG n = 0xCCCC;
    LONG st = f(buf, 0, &n, hello, 10);
    EXPECT_EQ(st, 0);
    EXPECT_EQ(n, 0);
    EXPECT_EQ((BYTE)buf[0], 0x55);
}

TEST(gap9_engum_embedded_nul)
{
    eum_fn f = EUM(); REQUIRE(f);
    static const WCHAR s[] = { 'x', 0, 'y' };
    char buf[8]; memset(buf, 0x55, sizeof buf);
    ULONG n = 0;
    EXPECT_EQ(f(buf, sizeof buf, &n, s, 6), 0);
    EXPECT_EQ(n, 3);
    EXPECT_EQ(buf[0], 'x'); EXPECT_EQ(buf[1], 0); EXPECT_EQ(buf[2], 'y');
}

TEST(gap9_engum_is_ansi_codepage_conversion)
{
    eum_fn f = EUM(); REQUIRE(f);
    static const WCHAR s[] = { 'a', 0x00E9, 'z', 0x0416 };
    char a[16] = { 0 }, b[16] = { 0 };
    ULONG n = 0;
    EXPECT_EQ(f(a, sizeof a, &n, s, 8), 0);
    int r = WideCharToMultiByte(CP_ACP, 0, s, 4, b, sizeof b, NULL, NULL);
    t_note("ACP=%u Eng n=%lu Win32 n=%d", GetACP(), (unsigned long)n, r);
    if (GetACP() == 1252) { EXPECT_EQ(n, 4); EXPECT_EQ((BYTE)a[1], 0xE9); EXPECT_EQ(a[0], 'a'); EXPECT_EQ(a[2], 'z'); }
}

TEST(gap9_engum_gdi32_and_gdi32full_agree)
{
    eum_fn a = EUM(), b = EUMF(); REQUIRE(a); REQUIRE(b);
    char x[16] = { 0 }, y[16] = { 0 };
    ULONG nx = 0, ny = 0;
    EXPECT_EQ(a(x, 16, &nx, hello, 10), b(y, 16, &ny, hello, 10));
    EXPECT_EQ(nx, ny);
    CHECK(!strcmp(x, y));
}

TEST(gap9_engum_large)
{
    eum_fn f = EUM(); REQUIRE(f);
    WCHAR *w = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, 30000 * sizeof(WCHAR));
    char *o = (char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 30001);
    REQUIRE(w); REQUIRE(o);
    for (int i = 0; i < 30000; i++) w[i] = (WCHAR)('a' + i % 26);
    ULONG n = 0;
    EXPECT_EQ(f(o, 30001, &n, w, 60000), 0);
    EXPECT_EQ(n, 30000);
    CHECK(o[29999] == (char)('a' + 29999 % 26));
    HeapFree(GetProcessHeap(), 0, w); HeapFree(GetProcessHeap(), 0, o);
}

TEST(gap9_engum_ascii_all_printable)
{
    eum_fn f = EUM(); REQUIRE(f);
    WCHAR w[96]; char o[96];
    for (int i = 0; i < 95; i++) w[i] = (WCHAR)(0x20 + i);
    memset(o, 0, sizeof o);
    ULONG n = 0;
    EXPECT_EQ(f(o, sizeof o, &n, w, 95 * 2), 0);
    EXPECT_EQ(n, 95);
    for (int i = 0; i < 95; i++) if (o[i] != 0x20 + i) { CHECKF(0, "char %d", i); break; }
}
