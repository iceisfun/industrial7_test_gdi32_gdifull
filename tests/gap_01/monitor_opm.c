#include "gap.h"

/* Physical-monitor and OPM/COPP exports are thin gdi32 stubs over win32kbase syscalls that return an
 * NTSTATUS (not a Win32 error). Device-name parameters are UNICODE_STRINGs ("\\.\DISPLAYn").
 * Only read-only queries and invalid-argument paths are exercised; no protected output is created. */

#define BOGUS_DEV L"\\\\.\\DISPLAY9999"
static const ULONG_PTR k_bogus_handles[] = { 0, 1, 2, 0x1234, 0xffff, 0x10000, 0x7fffffff, 0xffffffff, ~(ULONG_PTR)0, 0x0123456789abcdefULL };
#define NBOGUS (sizeof k_bogus_handles / sizeof *k_bogus_handles)

/* ---- GetNumberOfPhysicalMonitors ---- */
TEST(gap1_numphys_bogus_device_fails) {
    GetNumberOfPhysicalMonitors_pfn f = UN(GetNumberOfPhysicalMonitors);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    GAP_USTR u; gap_ustr(&u, BOGUS_DEV);
    DWORD n = 0x1234;
    LONG st = f(&u, &n);
    t_note("status %#lx", st);
    CHECK(GAP_FAILED(st));
    CHECK(n == 0x1234);
}
TEST(gap1_numphys_garbage_names_fail) {
    GetNumberOfPhysicalMonitors_pfn f = UN(GetNumberOfPhysicalMonitors);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    static const WCHAR *names[] = { L"x", L"DISPLAY1", L"\\\\.\\", L"\\\\.\\DISPLAY", L"\\\\.\\DISPLAYX", L"\\\\.\\PHYSICALDRIVE0", L"C:\\", L"\\\\.\\DISPLAY0", L"\\\\.\\DISPLAY-1" };
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++) {
        GAP_USTR u; gap_ustr(&u, names[i]);
        DWORD n = 0x4321;
        LONG st = f(&u, &n);
        CHECKF(GAP_FAILED(st), "name %u status %#lx", i, st);
        CHECKF(n == 0x4321, "name %u output written", i);
    }
}
TEST(gap1_numphys_empty_name_fails) {
    GetNumberOfPhysicalMonitors_pfn f = UN(GetNumberOfPhysicalMonitors);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    GAP_USTR u; gap_ustr(&u, L"");
    DWORD n = 0x55;
    LONG st = f(&u, &n);
    t_note("empty name status %#lx", st);
    CHECK(GAP_FAILED(st));
}
TEST(gap1_numphys_zero_length_with_valid_buffer) {
    GetNumberOfPhysicalMonitors_pfn f = UN(GetNumberOfPhysicalMonitors);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    WCHAR dev[64];
    if (!gap_primary_device(dev, NULL)) { t_skip("no monitor"); return; }
    GAP_USTR u; gap_ustr(&u, dev); u.Length = 0;
    DWORD n = 0x66;
    LONG st = f(&u, &n);
    t_note("Length=0 with valid buffer status %#lx n=%lu", st, n);
}
TEST(gap1_numphys_odd_length_note) {
    GetNumberOfPhysicalMonitors_pfn f = UN(GetNumberOfPhysicalMonitors);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    WCHAR dev[64];
    if (!gap_primary_device(dev, NULL)) { t_skip("no monitor"); return; }
    GAP_USTR u; gap_ustr(&u, dev); u.Length -= 1;
    DWORD n = 0x77;
    LONG st = f(&u, &n);
    t_note("odd Length status %#lx", st);
    CHECK(n == 0x77 || !GAP_FAILED(st));
}
TEST(gap1_numphys_primary_monitor) {
    GetNumberOfPhysicalMonitors_pfn f = UN(GetNumberOfPhysicalMonitors);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    WCHAR dev[64];
    if (!gap_primary_device(dev, NULL)) { t_skip("no monitor"); return; }
    GAP_USTR u; gap_ustr(&u, dev);
    DWORD n = 0xdead;
    LONG st = f(&u, &n);
    t_note("GetNumberOfPhysicalMonitors(%ls) -> %#lx count %lu", dev, st, n);
    if (!GAP_FAILED(st)) CHECK(n >= 1 && n <= 16);
    else CHECK(n == 0xdead);
}
TEST(gap1_numphys_primary_repeatable) {
    GetNumberOfPhysicalMonitors_pfn f = UN(GetNumberOfPhysicalMonitors);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    WCHAR dev[64];
    if (!gap_primary_device(dev, NULL)) { t_skip("no monitor"); return; }
    GAP_USTR u; gap_ustr(&u, dev);
    DWORD a = 0, b = 0;
    LONG sa = f(&u, &a), sb = f(&u, &b);
    CHECK(sa == sb);
    if (!GAP_FAILED(sa)) CHECK(a == b);
}
TEST(gap1_numphys_case_insensitive_name) {
    GetNumberOfPhysicalMonitors_pfn f = UN(GetNumberOfPhysicalMonitors);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    WCHAR dev[64], low[64];
    if (!gap_primary_device(dev, NULL)) { t_skip("no monitor"); return; }
    lstrcpyW(low, dev); CharLowerW(low);
    GAP_USTR a, b; gap_ustr(&a, dev); gap_ustr(&b, low);
    DWORD na = 0, nb = 0;
    LONG sa = f(&a, &na), sb = f(&b, &nb);
    t_note("upper status %#lx lower status %#lx", sa, sb);
}
TEST(gap1_numphys_matches_suggested_opm_array_size) {
    GetNumberOfPhysicalMonitors_pfn f = UN(GetNumberOfPhysicalMonitors);
    GetSuggestedOPMProtectedOutputArraySize_pfn g = UN(GetSuggestedOPMProtectedOutputArraySize);
    REQUIRE(f); REQUIRE(g);
    SKIP_WINE("kernel stub");
    WCHAR dev[64];
    if (!gap_primary_device(dev, NULL)) { t_skip("no monitor"); return; }
    GAP_USTR u; gap_ustr(&u, dev);
    DWORD a = 0, b = 0;
    LONG sa = f(&u, &a), sb = g(&u, &b);
    CHECK(sa == sb);
    if (!GAP_FAILED(sa) && !GAP_FAILED(sb)) CHECK(a == b);
}
TEST(gap1_numphys_status_not_win32_error) {
    GetNumberOfPhysicalMonitors_pfn f = UN(GetNumberOfPhysicalMonitors);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    GAP_USTR u; gap_ustr(&u, BOGUS_DEV);
    DWORD n = 0;
    SetLastError(0x6e6e);
    LONG st = f(&u, &n);
    CHECK(GAP_FAILED(st));
    t_note("last error after failing call: %lu", GetLastError());
}

/* ---- GetSuggestedOPMProtectedOutputArraySize ---- */
TEST(gap1_suggestopm_bogus_device_fails) {
    GetSuggestedOPMProtectedOutputArraySize_pfn f = UN(GetSuggestedOPMProtectedOutputArraySize);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    GAP_USTR u; gap_ustr(&u, BOGUS_DEV);
    DWORD n = 0x99;
    LONG st = f(&u, &n);
    CHECK(GAP_FAILED(st));
    CHECK(n == 0x99);
}
TEST(gap1_suggestopm_primary_note) {
    GetSuggestedOPMProtectedOutputArraySize_pfn f = UN(GetSuggestedOPMProtectedOutputArraySize);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    WCHAR dev[64];
    if (!gap_primary_device(dev, NULL)) { t_skip("no monitor"); return; }
    GAP_USTR u; gap_ustr(&u, dev);
    DWORD n = 0xdead;
    LONG st = f(&u, &n);
    t_note("GetSuggestedOPMProtectedOutputArraySize -> %#lx count %lu", st, n);
    if (!GAP_FAILED(st)) CHECK(n >= 1);
}
TEST(gap1_suggestopm_empty_and_garbage_names) {
    GetSuggestedOPMProtectedOutputArraySize_pfn f = UN(GetSuggestedOPMProtectedOutputArraySize);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    static const WCHAR *names[] = { L"", L"x", L"\\\\.\\DISPLAY", L"\\\\.\\DISPLAY12345" };
    for (unsigned i = 0; i < 4; i++) {
        GAP_USTR u; gap_ustr(&u, names[i]);
        DWORD n = 0x42;
        CHECKF(GAP_FAILED(f(&u, &n)), "name %u", i);
        CHECKF(n == 0x42, "name %u", i);
    }
}

/* ---- GetPhysicalMonitors / description / destroy ---- */
TEST(gap1_getphys_bogus_device_fails) {
    GetPhysicalMonitors_pfn f = UN(GetPhysicalMonitors);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    GAP_USTR u; gap_ustr(&u, BOGUS_DEV);
    HANDLE h[4] = { 0 }; DWORD got = 0x77;
    LONG st = f(&u, 4, &got, h);
    CHECK(GAP_FAILED(st));
    CHECK(got == 0x77);
    CHECK(h[0] == NULL && h[3] == NULL);
}
TEST(gap1_getphys_zero_count_bogus_device) {
    GetPhysicalMonitors_pfn f = UN(GetPhysicalMonitors);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    GAP_USTR u; gap_ustr(&u, BOGUS_DEV);
    HANDLE h[1] = { 0 }; DWORD got = 0x77;
    CHECK(GAP_FAILED(f(&u, 0, &got, h)));
}
TEST(gap1_getphys_huge_count_bogus_device) {
    GetPhysicalMonitors_pfn f = UN(GetPhysicalMonitors);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    GAP_USTR u; gap_ustr(&u, BOGUS_DEV);
    HANDLE h[1] = { 0 }; DWORD got = 0x77;
    static const DWORD counts[] = { 1, 16, 0x1000, 0x7fffffff, 0xffffffff };
    for (unsigned i = 0; i < sizeof counts / sizeof *counts; i++) {
        LONG st = f(&u, counts[i], &got, h);
        CHECKF(GAP_FAILED(st), "count %#lx status %#lx", counts[i], st);
    }
}
TEST(gap1_getphys_roundtrip_primary_then_destroy) {
    GetNumberOfPhysicalMonitors_pfn nf = UN(GetNumberOfPhysicalMonitors);
    GetPhysicalMonitors_pfn gf = UN(GetPhysicalMonitors);
    GetPhysicalMonitorDescription_pfn df = UN(GetPhysicalMonitorDescription);
    DestroyPhysicalMonitorInternal_pfn xf = UN(DestroyPhysicalMonitorInternal);
    REQUIRE(nf); REQUIRE(gf); REQUIRE(df); REQUIRE(xf);
    SKIP_WINE("kernel stub");
    WCHAR dev[64];
    if (!gap_primary_device(dev, NULL)) { t_skip("no monitor"); return; }
    GAP_USTR u; gap_ustr(&u, dev);
    DWORD n = 0;
    LONG st = nf(&u, &n);
    if (GAP_FAILED(st) || n == 0 || n > 16) { t_skip("no physical monitors (status %#lx n %lu)", st, n); return; }
    HANDLE h[16]; memset(h, 0, sizeof h);
    DWORD got = 0;
    st = gf(&u, n, &got, h);
    t_note("GetPhysicalMonitors -> %#lx got %lu first handle %p", st, got, h[0]);
    if (GAP_FAILED(st)) return;
    CHECK(got >= 1 && got <= n);
    for (DWORD i = 0; i < got && i < 16; i++) {
        CHECKF(h[i] != NULL, "handle %lu", i);
        WCHAR desc[128 + 8]; for (int k = 0; k < 136; k++) desc[k] = 0x7777;
        LONG ds = df(h[i], 0x80, desc);
        t_note("monitor %lu description status %#lx \"%ls\"", i, ds, ds >= 0 ? desc : L"");
        if (!GAP_FAILED(ds)) {
            int term = 0; for (int k = 0; k < 128; k++) if (!desc[k]) { term = 1; break; }
            CHECK(term);
        }
        for (int k = 128; k < 136; k++) if (desc[k] != 0x7777) { CHECKF(0, "overrun at %d", k); break; }
        LONG xs = xf(h[i]);
        CHECKF(!GAP_FAILED(xs), "destroy status %#lx", xs);
        LONG xs2 = xf(h[i]);
        t_note("second destroy of same handle -> %#lx", xs2);
    }
}
TEST(gap1_physdesc_wrong_length_invalid_parameter) {
    GetPhysicalMonitorDescription_pfn f = UN(GetPhysicalMonitorDescription);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    static const DWORD lens[] = { 0, 1, 0x7f, 0x81, 0x100, 0x1000, 0x7fffffff, 0x80000000, 0xffffffff };
    WCHAR buf[256]; memset(buf, 0x55, sizeof buf);
    for (unsigned i = 0; i < sizeof lens / sizeof *lens; i++) {
        LONG st = f((HANDLE)(ULONG_PTR)0x1234, lens[i], buf);
        CHECKF((ULONG)st == 0xC000000DUL, "len %#lx status %#lx", lens[i], st);
    }
    CHECK(buf[0] == 0x5555 && buf[255] == 0x5555);
}
TEST(gap1_physdesc_wrong_length_ignores_handle) {
    GetPhysicalMonitorDescription_pfn f = UN(GetPhysicalMonitorDescription);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    WCHAR buf[256];
    for (unsigned i = 0; i < NBOGUS; i++) {
        LONG st = f((HANDLE)k_bogus_handles[i], 0x40, buf);
        CHECKF((ULONG)st == 0xC000000DUL, "h %#llx status %#lx", (unsigned long long)k_bogus_handles[i], st);
    }
}
TEST(gap1_physdesc_bogus_handle_fails) {
    GetPhysicalMonitorDescription_pfn f = UN(GetPhysicalMonitorDescription);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    for (unsigned i = 0; i < NBOGUS; i++) {
        WCHAR buf[128 + 16]; memset(buf, 0x55, sizeof buf);
        LONG st = f((HANDLE)k_bogus_handles[i], 0x80, buf);
        CHECKF(GAP_FAILED(st), "h %#llx status %#lx", (unsigned long long)k_bogus_handles[i], st);
        CHECKF(buf[0] == 0x5555, "h %#llx buffer written", (unsigned long long)k_bogus_handles[i]);
    }
}
TEST(gap1_destroyphys_bogus_handles_note) {
    DestroyPhysicalMonitorInternal_pfn f = UN(DestroyPhysicalMonitorInternal);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    for (unsigned i = 0; i < NBOGUS; i++) {
        LONG st = f((HANDLE)k_bogus_handles[i]);
        t_note("DestroyPhysicalMonitorInternal(%#llx) -> %#lx", (unsigned long long)k_bogus_handles[i], st);
    }
}

/* ---- certificates ---- */
TEST(gap1_certsize_bogus_device_fails) {
    GetCertificateSize_pfn f = UN(GetCertificateSize);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    GAP_USTR u; gap_ustr(&u, BOGUS_DEV);
    for (DWORD t = 0; t < 4; t++) {
        DWORD sz = 0x3131;
        LONG st = f(&u, t, &sz);
        CHECKF(GAP_FAILED(st), "type %lu status %#lx", t, st);
        CHECKF(sz == 0x3131, "type %lu output written", t);
    }
}
TEST(gap1_certsize_bad_type_values_bogus_device) {
    GetCertificateSize_pfn f = UN(GetCertificateSize);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    GAP_USTR u; gap_ustr(&u, BOGUS_DEV);
    static const DWORD ts[] = { 3, 4, 0x100, 0x7fffffff, 0xffffffff };
    for (unsigned i = 0; i < sizeof ts / sizeof *ts; i++) {
        DWORD sz = 0x3131;
        CHECKF(GAP_FAILED(f(&u, ts[i], &sz)), "type %#lx", ts[i]);
    }
}
TEST(gap1_certsize_primary_each_type_note) {
    GetCertificateSize_pfn f = UN(GetCertificateSize);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    WCHAR dev[64];
    if (!gap_primary_device(dev, NULL)) { t_skip("no monitor"); return; }
    GAP_USTR u; gap_ustr(&u, dev);
    for (DWORD t = 0; t < 3; t++) {
        DWORD sz = 0xdead;
        LONG st = f(&u, t, &sz);
        t_note("GetCertificateSize(type %lu) -> %#lx size %lu", t, st, sz);
        if (GAP_FAILED(st)) CHECKF(sz == 0xdead, "type %lu output written on failure", t);
    }
}
TEST(gap1_cert_bogus_device_fails) {
    GetCertificate_pfn f = UN(GetCertificate);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    GAP_USTR u; gap_ustr(&u, BOGUS_DEV);
    BYTE buf[256]; memset(buf, 0xaa, sizeof buf);
    for (DWORD t = 0; t < 3; t++) {
        CHECKF(GAP_FAILED(f(&u, t, buf, sizeof buf)), "type %lu", t);
    }
    CHECK(buf[0] == 0xaa && buf[255] == 0xaa);
}
TEST(gap1_cert_bogus_device_zero_size) {
    GetCertificate_pfn f = UN(GetCertificate);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    GAP_USTR u; gap_ustr(&u, BOGUS_DEV);
    BYTE buf[4] = { 0 };
    CHECK(GAP_FAILED(f(&u, 0, buf, 0)));
    CHECK(GAP_FAILED(f(&u, 0, buf, 0xffffffff)));
}
TEST(gap1_cert_primary_size_boundaries) {
    GetCertificateSize_pfn sf = UN(GetCertificateSize);
    GetCertificate_pfn cf = UN(GetCertificate);
    REQUIRE(sf); REQUIRE(cf);
    SKIP_WINE("kernel stub");
    WCHAR dev[64];
    if (!gap_primary_device(dev, NULL)) { t_skip("no monitor"); return; }
    GAP_USTR u; gap_ustr(&u, dev);
    for (DWORD t = 0; t < 3; t++) {
        DWORD sz = 0;
        LONG st = sf(&u, t, &sz);
        if (GAP_FAILED(st) || sz == 0 || sz > 65536) { t_note("type %lu: no certificate (st %#lx sz %lu)", t, st, sz); continue; }
        BYTE *buf = (BYTE *)malloc(sz + 32);
        memset(buf, 0xaa, sz + 32);
        st = cf(&u, t, buf, sz);
        t_note("type %lu exact-size fetch %#lx", t, st);
        for (DWORD i = sz; i < sz + 32; i++) if (buf[i] != 0xaa) { CHECKF(0, "type %lu overrun", t); break; }
        memset(buf, 0xaa, sz + 32);
        st = cf(&u, t, buf, sz - 1);
        CHECKF((ULONG)st == 0xC01E050EUL, "type %lu too-small status %#lx", t, st);
        CHECK(buf[0] == 0xaa);
        free(buf);
    }
}
TEST(gap1_certsize_by_handle_bogus_handles_fail) {
    GetCertificateSizeByHandle_pfn f = UN(GetCertificateSizeByHandle);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    for (unsigned i = 0; i < NBOGUS; i++) {
        DWORD sz = 0x5151;
        LONG st = f((HANDLE)k_bogus_handles[i], 0, &sz);
        CHECKF(GAP_FAILED(st), "h %#llx status %#lx", (unsigned long long)k_bogus_handles[i], st);
        CHECKF(sz == 0x5151, "h %#llx output written", (unsigned long long)k_bogus_handles[i]);
    }
}
TEST(gap1_certsize_by_handle_types_bogus) {
    GetCertificateSizeByHandle_pfn f = UN(GetCertificateSizeByHandle);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    static const DWORD ts[] = { 0, 1, 2, 3, 0xffffffff };
    for (unsigned i = 0; i < sizeof ts / sizeof *ts; i++) {
        DWORD sz = 0x5151;
        CHECKF(GAP_FAILED(f((HANDLE)(ULONG_PTR)0x1234, ts[i], &sz)), "type %#lx", ts[i]);
    }
}
TEST(gap1_cert_by_handle_bogus_handles_fail) {
    GetCertificateByHandle_pfn f = UN(GetCertificateByHandle);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    for (unsigned i = 0; i < NBOGUS; i++) {
        BYTE buf[64]; memset(buf, 0xaa, sizeof buf);
        LONG st = f((HANDLE)k_bogus_handles[i], 0, buf, sizeof buf);
        CHECKF(GAP_FAILED(st), "h %#llx status %#lx", (unsigned long long)k_bogus_handles[i], st);
        CHECKF(buf[0] == 0xaa, "h %#llx buffer written", (unsigned long long)k_bogus_handles[i]);
    }
}
TEST(gap1_cert_by_handle_zero_size_bogus) {
    GetCertificateByHandle_pfn f = UN(GetCertificateByHandle);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    BYTE buf[4];
    CHECK(GAP_FAILED(f((HANDLE)(ULONG_PTR)0x1234, 1, buf, 0)));
    CHECK(GAP_FAILED(f(NULL, 1, buf, 0)));
}

/* ---- OPM handle-based calls ---- */
TEST(gap1_opmrandom_bogus_handles_fail) {
    GetOPMRandomNumber_pfn f = UN(GetOPMRandomNumber);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    for (unsigned i = 0; i < NBOGUS; i++) {
        BYTE rn[16 + 16]; memset(rn, 0xaa, sizeof rn);
        LONG st = f((HANDLE)k_bogus_handles[i], rn);
        CHECKF(GAP_FAILED(st), "h %#llx status %#lx", (unsigned long long)k_bogus_handles[i], st);
        CHECKF(rn[0] == 0xaa && rn[15] == 0xaa, "h %#llx buffer written", (unsigned long long)k_bogus_handles[i]);
    }
}
TEST(gap1_opminfo_bogus_handles_fail) {
    GetOPMInformation_pfn f = UN(GetOPMInformation);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    BYTE *in = (BYTE *)calloc(1, 8192), *out = (BYTE *)calloc(1, 8192);
    for (unsigned i = 0; i < NBOGUS; i++) {
        memset(out, 0xaa, 8192);
        LONG st = f((HANDLE)k_bogus_handles[i], in, out);
        CHECKF(GAP_FAILED(st), "h %#llx status %#lx", (unsigned long long)k_bogus_handles[i], st);
        CHECKF(out[0] == 0xaa, "h %#llx output written", (unsigned long long)k_bogus_handles[i]);
    }
    free(in); free(out);
}
TEST(gap1_coppinfo_bogus_handles_fail) {
    GetCOPPCompatibleOPMInformation_pfn f = UN(GetCOPPCompatibleOPMInformation);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    BYTE *in = (BYTE *)calloc(1, 8192), *out = (BYTE *)calloc(1, 8192);
    for (unsigned i = 0; i < NBOGUS; i++) {
        memset(out, 0xaa, 8192);
        LONG st = f((HANDLE)k_bogus_handles[i], in, out);
        CHECKF(GAP_FAILED(st), "h %#llx status %#lx", (unsigned long long)k_bogus_handles[i], st);
        CHECKF(out[0] == 0xaa, "h %#llx output written", (unsigned long long)k_bogus_handles[i]);
    }
    free(in); free(out);
}
TEST(gap1_opmconfigure_bogus_handles_fail) {
    ConfigureOPMProtectedOutput_pfn f = UN(ConfigureOPMProtectedOutput);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    BYTE *p = (BYTE *)calloc(1, 8192);
    for (unsigned i = 0; i < NBOGUS; i++) {
        LONG st = f((HANDLE)k_bogus_handles[i], p, 0, NULL);
        CHECKF(GAP_FAILED(st), "h %#llx status %#lx", (unsigned long long)k_bogus_handles[i], st);
    }
    free(p);
}
TEST(gap1_opmconfigure_with_additional_data_bogus_handle) {
    ConfigureOPMProtectedOutput_pfn f = UN(ConfigureOPMProtectedOutput);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    BYTE *p = (BYTE *)calloc(1, 8192);
    BYTE extra[64]; memset(extra, 0x11, sizeof extra);
    CHECK(GAP_FAILED(f((HANDLE)(ULONG_PTR)0x1234, p, sizeof extra, extra)));
    CHECK(GAP_FAILED(f(NULL, p, 1, extra)));
    free(p);
}
TEST(gap1_opm_all_failures_leave_lasterror_semantics_note) {
    GetOPMRandomNumber_pfn f = UN(GetOPMRandomNumber);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    BYTE rn[16];
    SetLastError(0x7a7a);
    LONG st = f(NULL, rn);
    CHECK(GAP_FAILED(st));
    t_note("GetOPMRandomNumber(NULL) status %#lx last error %lu", st, GetLastError());
}

/* ---- OPM creation (failure paths only) ---- */
TEST(gap1_createopm_bogus_device_fails) {
    CreateOPMProtectedOutputs_pfn f = UN(CreateOPMProtectedOutputs);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    GAP_USTR u; gap_ustr(&u, BOGUS_DEV);
    HANDLE hs[4] = { 0 }; DWORD got = 0x88;
    for (DWORD vos = 0; vos < 3; vos++) {
        LONG st = f(&u, vos, 4, &got, hs);
        CHECKF(GAP_FAILED(st), "vos %lu status %#lx", vos, st);
        CHECKF(got == 0x88 && hs[0] == NULL, "vos %lu outputs written", vos);
    }
}
TEST(gap1_createopm_bad_semantics_bogus_device) {
    CreateOPMProtectedOutputs_pfn f = UN(CreateOPMProtectedOutputs);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    GAP_USTR u; gap_ustr(&u, BOGUS_DEV);
    HANDLE hs[1] = { 0 }; DWORD got = 0;
    static const DWORD vs[] = { 3, 0x100, 0xffffffff };
    for (unsigned i = 0; i < sizeof vs / sizeof *vs; i++)
        CHECKF(GAP_FAILED(f(&u, vs[i], 1, &got, hs)), "vos %#lx", vs[i]);
}
TEST(gap1_createopm_zero_and_huge_count_bogus_device) {
    CreateOPMProtectedOutputs_pfn f = UN(CreateOPMProtectedOutputs);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    GAP_USTR u; gap_ustr(&u, BOGUS_DEV);
    HANDLE hs[1] = { 0 }; DWORD got = 0;
    CHECK(GAP_FAILED(f(&u, 0, 0, &got, hs)));
    CHECK(GAP_FAILED(f(&u, 0, 0xffffffff, &got, hs)));
    CHECK(GAP_FAILED(f(&u, 0, 0x7fffffff, &got, hs)));
}
TEST(gap1_createopm_single_bogus_luid_fails) {
    CreateOPMProtectedOutput_pfn f = UN(CreateOPMProtectedOutput);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    struct { LUID luid; ULONG target; ULONG vos; } in;
    memset(&in, 0, sizeof in);
    in.luid.LowPart = 0x7eadbeef; in.luid.HighPart = 0x1234; in.target = 0x7fffffff; in.vos = 0;
    HANDLE h = (HANDLE)(ULONG_PTR)0x5555;
    LONG st = f(&in, &h);
    t_note("CreateOPMProtectedOutput(bogus luid) -> %#lx handle %p", st, h);
    CHECK(GAP_FAILED(st));
    if (!GAP_FAILED(st)) t_note("unexpected success; handle leaked");
}
TEST(gap1_createopm_single_each_semantics_bogus_luid) {
    CreateOPMProtectedOutput_pfn f = UN(CreateOPMProtectedOutput);
    REQUIRE(f);
    SKIP_WINE("kernel stub");
    for (ULONG vos = 0; vos < 5; vos++) {
        struct { LUID luid; ULONG target; ULONG vos; } in;
        memset(&in, 0, sizeof in);
        in.luid.LowPart = 0x7eadbeef; in.luid.HighPart = 0x4321; in.target = 0x7ffffff0; in.vos = vos;
        HANDLE h = NULL;
        LONG st = f(&in, &h);
        CHECKF(GAP_FAILED(st), "vos %lu status %#lx", vos, st);
    }
}

/* ---- export placement ---- */
static const char *k_gdi32only[] = {
    "GetBitmapDpiScaleValue", "GetCurrentDpiInfo", "ScaleRgn", "ScaleValues", "SetMagicColors", "HT_Get8BPPFormatPalette",
    "HT_Get8BPPMaskPalette", "GetNumberOfPhysicalMonitors", "GetPhysicalMonitors", "GetPhysicalMonitorDescription",
    "DestroyPhysicalMonitorInternal", "GetCertificate", "GetCertificateByHandle", "GetCertificateSize", "GetCertificateSizeByHandle",
    "GetOPMInformation", "GetOPMRandomNumber", "GetCOPPCompatibleOPMInformation", "GetSuggestedOPMProtectedOutputArraySize",
    "CreateOPMProtectedOutput", "CreateOPMProtectedOutputs", "ConfigureOPMProtectedOutput",
};
static const char *k_both[] = {
    "GetBrushAttributes", "SetBrushAttributes", "ClearBrushAttributes", "GetDCDpiScaleValue", "MirrorRgn", "SetVirtualResolution",
    "SetLayoutWidth", "PolyPatBlt", "StartFormPage", "EndFormPage", "IsValidEnhMetaRecord", "IsValidEnhMetaRecordOffExt",
    "UpdateICMRegKeyA", "UpdateICMRegKeyW",
};
static const char *k_fullonly[] = { "DocumentEventEx", "DeleteEMFSpoolData", "IcmDeleteLocalDC", "IcmReleaseCachedColorSpace" };

TEST(gap1_exports_gdi32_only_set) {
    SKIP_WINE("export layout / stub behaviour differs");
    for (unsigned i = 0; i < sizeof k_gdi32only / sizeof *k_gdi32only; i++) {
        CHECKF(t_proc_gdi32(k_gdi32only[i]) != NULL, "%s missing from gdi32", k_gdi32only[i]);
        CHECKF(t_proc_gdi32full(k_gdi32only[i]) == NULL, "%s unexpectedly in gdi32full", k_gdi32only[i]);
    }
}
TEST(gap1_exports_both_modules_set) {
    SKIP_WINE("export layout / stub behaviour differs");
    for (unsigned i = 0; i < sizeof k_both / sizeof *k_both; i++) {
        CHECKF(t_proc_gdi32(k_both[i]) != NULL, "%s missing from gdi32", k_both[i]);
        CHECKF(t_proc_gdi32full(k_both[i]) != NULL, "%s missing from gdi32full", k_both[i]);
    }
}
TEST(gap1_exports_gdi32full_only_set) {
    SKIP_WINE("export layout / stub behaviour differs");
    for (unsigned i = 0; i < sizeof k_fullonly / sizeof *k_fullonly; i++) {
        CHECKF(t_proc_gdi32full(k_fullonly[i]) != NULL, "%s missing from gdi32full", k_fullonly[i]);
        CHECKF(t_proc_gdi32(k_fullonly[i]) == NULL, "%s unexpectedly in gdi32", k_fullonly[i]);
    }
}
TEST(gap1_exports_gdi32_forwarders_resolve_into_gdi32full) {
    SKIP_WINE("export layout / stub behaviour differs");
    /* a forwarded export resolves (via GetProcAddress) to the address inside gdi32full.dll */
    HMODULE full = t_gdi32full();
    REQUIRE(full);
    for (unsigned i = 0; i < sizeof k_both / sizeof *k_both; i++) {
        FARPROC a = t_proc_gdi32(k_both[i]), b = t_proc_gdi32full(k_both[i]);
        if (!a || !b) continue;
        t_note("%s gdi32=%p gdi32full=%p same=%d", k_both[i], a, b, a == b);
    }
}
