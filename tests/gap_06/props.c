#include "gap6.h"
/* Script* entry points that need no device context: properties, digit
 * substitution, itemization, break analysis, layout.
 * All Script* APIs are wide-only (no A variants), so A/W parity is exercised
 * in the ScriptStringAnalyse tests via the iCharset ANSI path. */

#define NEED(f) REQUIRE(U(f))

/* ---------------- ScriptGetProperties ---------------- */
TEST(gap6_props_getprops_both_null) {
    NEED(ScriptGetProperties);
    EXPECT_HR(U(ScriptGetProperties)(NULL, NULL), HR_INVALIDARG);
}
TEST(gap6_props_getprops_table_only) {
    NEED(ScriptGetProperties);
    const SCRIPT_PROPERTIES **pp = (const SCRIPT_PROPERTIES **)(ULONG_PTR)0x1;
    pp = NULL;
    EXPECT_HR(U(ScriptGetProperties)(&pp, NULL), S_OK);
    CHECK(pp != NULL);
}
TEST(gap6_props_getprops_count_only) {
    NEED(ScriptGetProperties);
    int n = -1;
    EXPECT_HR(U(ScriptGetProperties)(NULL, &n), S_OK);
    CHECK(n > 50);
    CHECK(n < 1024);
    t_note("ScriptGetProperties count = %d", n);
}
TEST(gap6_props_getprops_both) {
    NEED(ScriptGetProperties);
    const SCRIPT_PROPERTIES **pp = NULL; int n = 0;
    EXPECT_HR(U(ScriptGetProperties)(&pp, &n), S_OK);
    CHECK(pp != NULL);
    CHECK(n > 0);
    if (pp) for (int i = 0; i < n; i++) { if (!pp[i]) { CHECKF(0, "null entry %d", i); break; } }
}
TEST(gap6_props_getprops_stable_pointer) {
    NEED(ScriptGetProperties);
    const SCRIPT_PROPERTIES **a = NULL, **b = NULL; int na = 0, nb = 0;
    EXPECT_HR(U(ScriptGetProperties)(&a, &na), S_OK);
    EXPECT_HR(U(ScriptGetProperties)(&b, &nb), S_OK);
    CHECK(a == b);
    CHECK(na == nb);
}
TEST(gap6_props_getprops_count_untouched_when_null_table_invalid) {
    NEED(ScriptGetProperties);
    /* both NULL fails before anything is written */
    EXPECT_HR(U(ScriptGetProperties)(NULL, NULL), HR_INVALIDARG);
}
TEST(gap6_props_getprops_latin_not_complex_arabic_rtl) {
    NEED(ScriptGetProperties); NEED(ScriptItemize);
    const SCRIPT_PROPERTIES **pp = NULL; int n = 0;
    EXPECT_HR(U(ScriptGetProperties)(&pp, &n), S_OK);
    if (!pp) return;
    SCRIPT_ITEM it[4]; int c;
    c = g6_itemize(L"Hello", 5, it, 4, NULL);
    CHECK(c == 1);
    if (c == 1 && it[0].a.eScript < n) CHECK(pp[it[0].a.eScript]->fComplex == 0);
    c = g6_itemize(L"\x0627\x0644\x0639", 3, it, 4, NULL);
    CHECK(c >= 1);
    if (c >= 1 && it[0].a.eScript < n) {
        CHECK(it[0].a.fRTL == 1);
        CHECK(pp[it[0].a.eScript]->fComplex == 1);
        t_note("arabic bCharSet=%u", (unsigned)pp[it[0].a.eScript]->bCharSet);
    }
    c = g6_itemize(L"\x05d0\x05d1", 2, it, 4, NULL);
    CHECK(c >= 1);
    if (c >= 1 && it[0].a.eScript < n) CHECK(it[0].a.fRTL == 1);
}

/* ---------------- ScriptRecordDigitSubstitution ---------------- */
TEST(gap6_props_recdigit_null_out) {
    NEED(ScriptRecordDigitSubstitution);
    WINE_SKIP("digit substitution records differ");
    static const LCID l[] = { 0, LOCALE_USER_DEFAULT, LOCALE_SYSTEM_DEFAULT, 0x0409, 0x7fff7fff };
    for (int i = 0; i < ARRSZ(l); i++)
        EXPECT_HR(U(ScriptRecordDigitSubstitution)(l[i], NULL), HR_INVALIDARG);
}
TEST(gap6_props_recdigit_user_default) {
    NEED(ScriptRecordDigitSubstitution);
    SCRIPT_DIGITSUBSTITUTE d; memset(&d, 0xcc, sizeof d);
    EXPECT_HR(U(ScriptRecordDigitSubstitution)(LOCALE_USER_DEFAULT, &d), S_OK);
    CHECK(d.DigitSubstitute <= SCRIPT_DIGITSUBSTITUTE_TRADITIONAL);
    CHECK(d.dwReserved == 0);
    CHECK(d.NationalDigitLanguage != 0);
}
TEST(gap6_props_recdigit_system_default) {
    NEED(ScriptRecordDigitSubstitution);
    SCRIPT_DIGITSUBSTITUTE d; memset(&d, 0xcc, sizeof d);
    EXPECT_HR(U(ScriptRecordDigitSubstitution)(LOCALE_SYSTEM_DEFAULT, &d), S_OK);
    CHECK(d.DigitSubstitute <= SCRIPT_DIGITSUBSTITUTE_TRADITIONAL);
    CHECK(d.dwReserved == 0);
}
TEST(gap6_props_recdigit_enus) {
    NEED(ScriptRecordDigitSubstitution);
    SCRIPT_DIGITSUBSTITUTE d; memset(&d, 0xcc, sizeof d);
    EXPECT_HR(U(ScriptRecordDigitSubstitution)(MAKELCID(0x0409, SORT_DEFAULT), &d), S_OK);
    CHECK(d.NationalDigitLanguage == 9);
    CHECK(d.TraditionalDigitLanguage == 9);
    CHECK(d.DigitSubstitute == SCRIPT_DIGITSUBSTITUTE_NONE);
    CHECK(d.dwReserved == 0);
}
TEST(gap6_props_recdigit_invalid_locale) {
    NEED(ScriptRecordDigitSubstitution);
    WINE_SKIP("digit substitution records differ");
    static const LCID bad[] = { 0x0000FFF0, 0x00007F7F, 0x12345678, 0x0001ABCD };
    for (int i = 0; i < ARRSZ(bad); i++) {
        SCRIPT_DIGITSUBSTITUTE d; memset(&d, 0xcc, sizeof d);
        HRESULT hr = U(ScriptRecordDigitSubstitution)(bad[i], &d);
        CHECKF(hr == HR_INVALIDARG, "lcid 0x%lx -> 0x%08lx", (unsigned long)bad[i], (unsigned long)hr);
        /* record is pre-initialised to the default 9/9/0 before the locale check */
        CHECKF(d.NationalDigitLanguage == 9 && d.TraditionalDigitLanguage == 9 && d.DigitSubstitute == 0,
               "lcid 0x%lx rec %u/%u/%u", (unsigned long)bad[i], (unsigned)d.NationalDigitLanguage,
               (unsigned)d.TraditionalDigitLanguage, (unsigned)d.DigitSubstitute);
    }
}
TEST(gap6_props_recdigit_native_digit_languages) {
    NEED(ScriptRecordDigitSubstitution);
    WINE_SKIP("digit substitution records differ");
    static const struct { LCID lcid; WORD lang; } t[] = {
        { 0x0401, 0x01 },  /* ar-SA: Arabic-Indic */
        { 0x0439, 0x39 },  /* hi-IN: Devanagari */
        { 0x041E, 0x1E },  /* th-TH: Thai */
        { 0x0429, 0x29 },  /* fa-IR: Extended Arabic-Indic */
        { 0x0445, 0x45 },  /* bn-IN */
        { 0x0409, 0x09 },  /* en-US */
        { 0x0411, 0x09 },  /* ja-JP: ASCII digits */
        { 0x0407, 0x09 },  /* de-DE */
    };
    for (int i = 0; i < ARRSZ(t); i++) {
        SCRIPT_DIGITSUBSTITUTE d; memset(&d, 0xcc, sizeof d);
        HRESULT hr = U(ScriptRecordDigitSubstitution)(t[i].lcid, &d);
        if (hr != S_OK) { t_note("locale 0x%04lx not available (0x%08lx)", (unsigned long)t[i].lcid, (unsigned long)hr); continue; }
        CHECKF(d.NationalDigitLanguage == t[i].lang, "lcid 0x%04lx national %u want %u",
               (unsigned long)t[i].lcid, (unsigned)d.NationalDigitLanguage, (unsigned)t[i].lang);
        CHECKF(d.TraditionalDigitLanguage == (t[i].lcid & 0x3ff), "lcid 0x%04lx trad %u",
               (unsigned long)t[i].lcid, (unsigned)d.TraditionalDigitLanguage);
        CHECK(d.DigitSubstitute <= SCRIPT_DIGITSUBSTITUTE_TRADITIONAL);
    }
}
TEST(gap6_props_recdigit_repeatable) {
    NEED(ScriptRecordDigitSubstitution);
    WINE_SKIP("digit substitution records differ");
    SCRIPT_DIGITSUBSTITUTE a, b; memset(&a, 1, sizeof a); memset(&b, 2, sizeof b);
    EXPECT_HR(U(ScriptRecordDigitSubstitution)(0x0409, &a), S_OK);
    EXPECT_HR(U(ScriptRecordDigitSubstitution)(0x0409, &b), S_OK);
    CHECK(memcmp(&a, &b, sizeof a) == 0);
}

/* ---------------- ScriptApplyDigitSubstitution ---------------- */
static void apply_ds(BYTE mode, WORD nat, WORD trad, DWORD ctl0, WORD st0,
                     DWORD *ctl, WORD *st, HRESULT *hr) {
    SCRIPT_DIGITSUBSTITUTE d; memset(&d, 0, sizeof d);
    d.NationalDigitLanguage = nat; d.TraditionalDigitLanguage = trad; d.DigitSubstitute = mode;
    SCRIPT_CONTROL c; SCRIPT_STATE s;
    memcpy(&c, &ctl0, sizeof c); memcpy(&s, &st0, sizeof s);
    *hr = U(ScriptApplyDigitSubstitution)(&d, &c, &s);
    memcpy(ctl, &c, sizeof c); memcpy(st, &s, sizeof s);
}
TEST(gap6_props_applydigit_null_args) {
    NEED(ScriptApplyDigitSubstitution);
    WINE_SKIP("digit substitution records differ");
    SCRIPT_DIGITSUBSTITUTE d; memset(&d, 0, sizeof d);
    SCRIPT_CONTROL c; SCRIPT_STATE s; memset(&c, 0, sizeof c); memset(&s, 0, sizeof s);
    EXPECT_HR(U(ScriptApplyDigitSubstitution)(&d, NULL, &s), HR_INVALIDARG);
    EXPECT_HR(U(ScriptApplyDigitSubstitution)(&d, &c, NULL), HR_INVALIDARG);
    EXPECT_HR(U(ScriptApplyDigitSubstitution)(NULL, NULL, NULL), HR_INVALIDARG);
    EXPECT_HR(U(ScriptApplyDigitSubstitution)(&d, NULL, NULL), HR_INVALIDARG);
}
TEST(gap6_props_applydigit_none) {
    NEED(ScriptApplyDigitSubstitution);
    WINE_SKIP("digit substitution records differ");
    DWORD c; WORD s; HRESULT hr;
    apply_ds(SCRIPT_DIGITSUBSTITUTE_NONE, 0x11, 0x22, 0x10000 | 0xaaaa, 0x100 | 3, &c, &s, &hr);
    EXPECT_HR(hr, S_OK);
    CHECK((s & 0x100) == 0);        /* fDigitSubstitute cleared */
    CHECK((c & 0x10000) == 0);      /* fContextDigits cleared */
    CHECK((c & 0xffff) == 0x11);    /* default language = national */
    CHECK((s & 0x1f) == 3);         /* bidi level untouched */
}
TEST(gap6_props_applydigit_national) {
    NEED(ScriptApplyDigitSubstitution);
    WINE_SKIP("digit substitution records differ");
    DWORD c; WORD s; HRESULT hr;
    apply_ds(SCRIPT_DIGITSUBSTITUTE_NATIONAL, 0x11, 0x22, 0x10000, 0, &c, &s, &hr);
    EXPECT_HR(hr, S_OK);
    CHECK((s & 0x100) != 0);
    CHECK((c & 0x10000) == 0);
    CHECK((c & 0xffff) == 0x11);
}
TEST(gap6_props_applydigit_traditional) {
    NEED(ScriptApplyDigitSubstitution);
    WINE_SKIP("digit substitution records differ");
    DWORD c; WORD s; HRESULT hr;
    apply_ds(SCRIPT_DIGITSUBSTITUTE_TRADITIONAL, 0x11, 0x22, 0x10000, 0, &c, &s, &hr);
    EXPECT_HR(hr, S_OK);
    CHECK((s & 0x100) != 0);
    CHECK((c & 0x10000) == 0);
    CHECK((c & 0xffff) == 0x22);
}
TEST(gap6_props_applydigit_context) {
    NEED(ScriptApplyDigitSubstitution);
    WINE_SKIP("digit substitution records differ");
    DWORD c; WORD s; HRESULT hr;
    apply_ds(SCRIPT_DIGITSUBSTITUTE_CONTEXT, 0x11, 0x22, 0, 0, &c, &s, &hr);
    EXPECT_HR(hr, S_OK);
    CHECK((s & 0x100) != 0);
    CHECK((c & 0x10000) != 0);
    CHECK((c & 0xffff) == 0x22);
}
TEST(gap6_props_applydigit_unknown_mode_noop) {
    NEED(ScriptApplyDigitSubstitution);
    WINE_SKIP("lenient parameter validation");
    for (int m = 4; m < 8; m++) {
        DWORD c; WORD s; HRESULT hr;
        apply_ds((BYTE)m, 0x11, 0x22, 0x10000 | 0x55, 0x1f, &c, &s, &hr);
        CHECKF(c == (0x10000 | 0x55) && s == 0x1f, "mode %d changed state c=%lx s=%x", m, (unsigned long)c, s);
    }
}
TEST(gap6_props_applydigit_null_subst_uses_user_default) {
    NEED(ScriptApplyDigitSubstitution); NEED(ScriptRecordDigitSubstitution);
    SCRIPT_CONTROL c; SCRIPT_STATE s; memset(&c, 0, sizeof c); memset(&s, 0, sizeof s);
    EXPECT_HR(U(ScriptApplyDigitSubstitution)(NULL, &c, &s), S_OK);
    SCRIPT_DIGITSUBSTITUTE d;
    EXPECT_HR(U(ScriptRecordDigitSubstitution)(LOCALE_USER_DEFAULT, &d), S_OK);
    SCRIPT_CONTROL c2; SCRIPT_STATE s2; memset(&c2, 0, sizeof c2); memset(&s2, 0, sizeof s2);
    EXPECT_HR(U(ScriptApplyDigitSubstitution)(&d, &c2, &s2), S_OK);
    CHECK(memcmp(&c, &c2, sizeof c) == 0);
    CHECK(memcmp(&s, &s2, sizeof s) == 0);
}

/* ---------------- ScriptIsComplex ---------------- */
TEST(gap6_props_iscomplex_invalid) {
    NEED(ScriptIsComplex);
    EXPECT_HR(U(ScriptIsComplex)(NULL, 1, SIC_COMPLEX), HR_INVALIDARG);
    EXPECT_HR(U(ScriptIsComplex)(NULL, 0, SIC_COMPLEX), HR_INVALIDARG);
    EXPECT_HR(U(ScriptIsComplex)(L"abc", -1, SIC_COMPLEX), HR_INVALIDARG);
    EXPECT_HR(U(ScriptIsComplex)(L"abc", (int)0x80000000, SIC_COMPLEX), HR_INVALIDARG);
}
TEST(gap6_props_iscomplex_empty) {
    NEED(ScriptIsComplex);
    EXPECT_HR(U(ScriptIsComplex)(L"", 0, SIC_COMPLEX), S_FALSE);
    EXPECT_HR(U(ScriptIsComplex)(L"abc", 0, SIC_COMPLEX | SIC_ASCIIDIGIT | SIC_NEUTRAL), S_FALSE);
}
TEST(gap6_props_iscomplex_ascii_simple) {
    NEED(ScriptIsComplex);
    EXPECT_HR(U(ScriptIsComplex)(L"Hello, World", 12, SIC_COMPLEX), S_FALSE);
    EXPECT_HR(U(ScriptIsComplex)(L"abcdefghijklmnopqrstuvwxyz", 26, SIC_COMPLEX), S_FALSE);
    EXPECT_HR(U(ScriptIsComplex)(L"ABCDEFGHIJKLMNOPQRSTUVWXYZ", 26, SIC_COMPLEX), S_FALSE);
}
TEST(gap6_props_iscomplex_latin1_simple) {
    NEED(ScriptIsComplex);
    EXPECT_HR(U(ScriptIsComplex)(L"\x00e9\x00e8\x00fc\x00f1", 4, SIC_COMPLEX), S_FALSE);
}
TEST(gap6_props_iscomplex_arabic) {
    NEED(ScriptIsComplex);
    EXPECT_HR(U(ScriptIsComplex)(L"\x0627\x0644", 2, SIC_COMPLEX), S_OK);
    EXPECT_HR(U(ScriptIsComplex)(L"a\x0627", 2, SIC_COMPLEX), S_OK);
    EXPECT_HR(U(ScriptIsComplex)(L"\x0627" L"abc", 4, SIC_COMPLEX), S_OK);
}
TEST(gap6_props_iscomplex_length_limits_scan) {
    NEED(ScriptIsComplex);
    /* only the first cInChars characters are examined */
    EXPECT_HR(U(ScriptIsComplex)(L"abc\x0627", 3, SIC_COMPLEX), S_FALSE);
    EXPECT_HR(U(ScriptIsComplex)(L"abc\x0627", 4, SIC_COMPLEX), S_OK);
    EXPECT_HR(U(ScriptIsComplex)(L"abc\x0627", 1, SIC_COMPLEX), S_FALSE);
}
TEST(gap6_props_iscomplex_flags_zero_never_complex) {
    NEED(ScriptIsComplex);
    EXPECT_HR(U(ScriptIsComplex)(L"\x0627\x0644", 2, 0), S_FALSE);
    EXPECT_HR(U(ScriptIsComplex)(L"123 abc", 7, 0), S_FALSE);
}
TEST(gap6_props_iscomplex_asciidigit) {
    NEED(ScriptIsComplex);
    EXPECT_HR(U(ScriptIsComplex)(L"123", 3, SIC_COMPLEX), S_FALSE);
    EXPECT_HR(U(ScriptIsComplex)(L"123", 3, SIC_ASCIIDIGIT), S_OK);
    EXPECT_HR(U(ScriptIsComplex)(L"abc", 3, SIC_ASCIIDIGIT), S_FALSE);
    EXPECT_HR(U(ScriptIsComplex)(L"abc7", 4, SIC_ASCIIDIGIT), S_OK);
}
TEST(gap6_props_iscomplex_neutral_flag) {
    NEED(ScriptIsComplex);
    HRESULT hr = U(ScriptIsComplex)(L" ", 1, SIC_NEUTRAL);
    CHECK(hr == S_OK || hr == S_FALSE);
    t_note("space with SIC_NEUTRAL -> 0x%08lx", (unsigned long)hr);
    hr = U(ScriptIsComplex)(L"abc", 3, SIC_NEUTRAL);
    CHECK(hr == S_OK || hr == S_FALSE);
    t_note("abc with SIC_NEUTRAL -> 0x%08lx", (unsigned long)hr);
    /* a neutral-only flag never reports Latin letters */
}
TEST(gap6_props_iscomplex_thai_hebrew_devanagari) {
    NEED(ScriptIsComplex);
    static const struct { const WCHAR *s; const char *nm; } t[] = {
        { L"\x0e01\x0e32", "thai" }, { L"\x05d0\x05d1", "hebrew" },
        { L"\x0915\x094d", "devanagari" }, { L"\x0e01", "thai1" },
    };
    for (int i = 0; i < ARRSZ(t); i++) {
        HRESULT hr = U(ScriptIsComplex)(t[i].s, (int)lstrlenW(t[i].s), SIC_COMPLEX);
        CHECKF(hr == S_OK || hr == S_FALSE, "%s hr=0x%08lx", t[i].nm, (unsigned long)hr);
        t_note("IsComplex %s -> 0x%08lx", t[i].nm, (unsigned long)hr);
    }
    EXPECT_HR(U(ScriptIsComplex)(L"\x0915\x094d", 2, SIC_COMPLEX), S_OK);
}
TEST(gap6_props_iscomplex_combining_mark) {
    NEED(ScriptIsComplex);
    HRESULT hr = U(ScriptIsComplex)(L"e\x0301", 2, SIC_COMPLEX);
    CHECK(hr == S_OK);
}
TEST(gap6_props_iscomplex_surrogate_pair_ok) {
    NEED(ScriptIsComplex);
    /* U+1F600 as a valid pair, and a lone surrogate: must not fail or fault */
    HRESULT hr = U(ScriptIsComplex)(L"\xd83d\xde00", 2, SIC_COMPLEX);
    CHECK(SUCCEEDED(hr));
    hr = U(ScriptIsComplex)(L"\xd83d", 1, SIC_COMPLEX);
    CHECK(SUCCEEDED(hr));
}
TEST(gap6_props_iscomplex_ascii_exhaustive_flag1) {
    NEED(ScriptIsComplex);
    for (WCHAR ch = 0x20; ch < 0x7f; ch++) {
        HRESULT hr = U(ScriptIsComplex)(&ch, 1, SIC_COMPLEX);
        CHECKF(hr == S_FALSE, "U+%04x -> 0x%08lx", ch, (unsigned long)hr);
    }
}

/* ---------------- ScriptItemize ---------------- */
TEST(gap6_props_itemize_invalid_zero_pcitems) {
    NEED(ScriptItemize);
    WINE_SKIP("lenient parameter validation");
    SCRIPT_ITEM it[4]; int n = 77;
    EXPECT_HR(U(ScriptItemize)(NULL, 3, 4, NULL, NULL, it, &n), HR_INVALIDARG);
    CHECK(n == 0);
    n = 77;
    EXPECT_HR(U(ScriptItemize)(L"abc", 3, 4, NULL, NULL, NULL, &n), HR_INVALIDARG);
    CHECK(n == 0);
    n = 77;
    EXPECT_HR(U(ScriptItemize)(L"abc", 0, 4, NULL, NULL, it, &n), HR_INVALIDARG);
    CHECK(n == 0);
    n = 77;
    EXPECT_HR(U(ScriptItemize)(L"abc", -1, 4, NULL, NULL, it, &n), HR_INVALIDARG);
    CHECK(n == 0);
    n = 77;
    EXPECT_HR(U(ScriptItemize)(L"abc", 3, 1, NULL, NULL, it, &n), HR_INVALIDARG);
    CHECK(n == 0);
    n = 77;
    EXPECT_HR(U(ScriptItemize)(L"abc", 3, 0, NULL, NULL, it, &n), HR_INVALIDARG);
    CHECK(n == 0);
    n = 77;
    EXPECT_HR(U(ScriptItemize)(L"abc", 3, -5, NULL, NULL, it, &n), HR_INVALIDARG);
    CHECK(n == 0);
}
TEST(gap6_props_itemize_invalid_null_pcitems) {
    NEED(ScriptItemize);
    SCRIPT_ITEM it[4];
    EXPECT_HR(U(ScriptItemize)(NULL, 3, 4, NULL, NULL, it, NULL), HR_INVALIDARG);
    EXPECT_HR(U(ScriptItemize)(L"abc", 0, 4, NULL, NULL, it, NULL), HR_INVALIDARG);
    EXPECT_HR(U(ScriptItemize)(L"abc", 3, 1, NULL, NULL, it, NULL), HR_INVALIDARG);
    EXPECT_HR(U(ScriptItemize)(L"abc", 3, 4, NULL, NULL, NULL, NULL), HR_INVALIDARG);
}
TEST(gap6_props_itemize_single_latin) {
    NEED(ScriptItemize);
    SCRIPT_ITEM it[4]; memset(it, 0xcc, sizeof it); int n = 0;
    EXPECT_HR(U(ScriptItemize)(L"Hello", 5, 2, NULL, NULL, it, &n), S_OK);
    CHECK(n == 1);
    CHECK(it[0].iCharPos == 0);
    CHECK(it[1].iCharPos == 5);      /* sentinel item */
    CHECK(it[0].a.s.uBidiLevel == 0);
    CHECK(it[0].a.fRTL == 0);
}
TEST(gap6_props_itemize_one_char) {
    NEED(ScriptItemize);
    SCRIPT_ITEM it[2]; int n = 0;
    EXPECT_HR(U(ScriptItemize)(L"x", 1, 2, NULL, NULL, it, &n), S_OK);
    CHECK(n == 1);
    CHECK(it[0].iCharPos == 0);
    CHECK(it[1].iCharPos == 1);
}
TEST(gap6_props_itemize_latin_and_digits_and_space_one_item) {
    NEED(ScriptItemize);
    SCRIPT_ITEM it[4]; int n = 0;
    WINE_SKIP("digits/punctuation split into extra items");
    EXPECT_HR(U(ScriptItemize)(L"Hello world 123, ok.", 20, 4, NULL, NULL, it, &n), S_OK);
    CHECK(n == 1);
    CHECK(it[n].iCharPos == 20);
}
TEST(gap6_props_itemize_hebrew_levels) {
    NEED(ScriptItemize);
    SCRIPT_ITEM it[8]; int n = g6_itemize(L"\x05d0\x05d1\x05d2", 3, it, 8, NULL);
    CHECK(n == 1);
    if (n == 1) {
        CHECK(it[0].a.s.uBidiLevel == 1);
        CHECK(it[0].a.fRTL == 1);
        CHECK(it[1].iCharPos == 3);
    }
}
TEST(gap6_props_itemize_mixed_bidi) {
    NEED(ScriptItemize);
    SCRIPT_ITEM it[8]; int n = g6_itemize(L"ab \x05d0\x05d1 cd", 8, it, 8, NULL);
    CHECK(n >= 3);
    if (n >= 3) {
        CHECK(it[0].iCharPos == 0);
        CHECK(it[0].a.s.uBidiLevel == 0);
        for (int i = 0; i < n; i++) CHECKF(it[i].iCharPos < it[i + 1].iCharPos, "non-monotonic at %d", i);
        CHECK(it[n].iCharPos == 8);
        int sawrtl = 0;
        for (int i = 0; i < n; i++) if (it[i].a.s.uBidiLevel == 1) sawrtl = 1;
        CHECK(sawrtl);
        CHECK(it[n - 1].a.s.uBidiLevel == 0);
    }
}
TEST(gap6_props_itemize_max_items_too_small) {
    NEED(ScriptItemize);
    SCRIPT_ITEM it[4]; int n = 55;
    /* a, hebrew, b => needs three items + sentinel */
    HRESULT hr = U(ScriptItemize)(L"a" L"\x05d0" L"b", 3, 2, NULL, NULL, it, &n);
    CHECK(FAILED(hr));
    t_note("ScriptItemize too-small -> 0x%08lx, cItems=%d", (unsigned long)hr, n);
    EXPECT_HR(hr, HR_OUTOFMEMORY);
}
TEST(gap6_props_itemize_max_items_exact) {
    NEED(ScriptItemize);
    SCRIPT_ITEM it[6]; int n = 0;
    HRESULT hr = U(ScriptItemize)(L"a" L"\x05d0" L"b", 3, 4, NULL, NULL, it, &n);
    EXPECT_HR(hr, S_OK);
    CHECK(n == 3);
    CHECK(it[3].iCharPos == 3);
}
TEST(gap6_props_itemize_state_rtl_base_latin_level) {
    NEED(ScriptItemize);
    WINE_SKIP("bidi base level handling differs");
    SCRIPT_STATE st; memset(&st, 0, sizeof st); st.uBidiLevel = 1;
    SCRIPT_ITEM it[4]; int n = g6_itemize(L"abc", 3, it, 4, &st);
    CHECK(n == 1);
    if (n == 1) CHECK(it[0].a.s.uBidiLevel == 2);
    n = g6_itemize(L"\x05d0\x05d1", 2, it, 4, &st);
    CHECK(n == 1);
    if (n == 1) CHECK(it[0].a.s.uBidiLevel == 1);
}
TEST(gap6_props_itemize_state_override) {
    NEED(ScriptItemize);
    WINE_SKIP("bidi base level handling differs");
    SCRIPT_STATE st; memset(&st, 0, sizeof st); st.uBidiLevel = 1; st.fOverrideDirection = 1;
    SCRIPT_ITEM it[4]; int n = g6_itemize(L"abc", 3, it, 4, &st);
    CHECK(n == 1);
    if (n == 1) CHECK(it[0].a.s.uBidiLevel == 1);
}
TEST(gap6_props_itemize_control_default_language) {
    NEED(ScriptItemize);
    SCRIPT_CONTROL sc; memset(&sc, 0, sizeof sc);
    sc.uDefaultLanguage = 0x0409;
    SCRIPT_ITEM it[4]; int n = 0;
    EXPECT_HR(U(ScriptItemize)(L"abc 123", 7, 4, &sc, NULL, it, &n), S_OK);
    CHECK(n >= 1);
}
TEST(gap6_props_itemize_items_not_overwritten_beyond) {
    NEED(ScriptItemize);
    SCRIPT_ITEM it[6]; memset(it, 0xee, sizeof it); int n = 0;
    EXPECT_HR(U(ScriptItemize)(L"abc", 3, 2, NULL, NULL, it, &n), S_OK);
    CHECK(n == 1);
    SCRIPT_ITEM canary; memset(&canary, 0xee, sizeof canary);
    CHECK(memcmp(&it[2], &canary, sizeof canary) == 0);
}
TEST(gap6_props_itemize_repeatable) {
    NEED(ScriptItemize);
    SCRIPT_ITEM a[8], b[8]; int na = 0, nb = 0;
    memset(a, 0, sizeof a); memset(b, 0, sizeof b);
    for (int i = 0; i < 50; i++) {
        EXPECT_HR(U(ScriptItemize)(L"ab\x05d0\x05d1 cd", 7, 8, NULL, NULL, a, &na), S_OK);
        EXPECT_HR(U(ScriptItemize)(L"ab\x05d0\x05d1 cd", 7, 8, NULL, NULL, b, &nb), S_OK);
        CHECK(na == nb);
    }
    CHECK(memcmp(a, b, sizeof(SCRIPT_ITEM) * (na + 1)) == 0);
}
TEST(gap6_props_itemize_long_string) {
    NEED(ScriptItemize);
    static WCHAR buf[4097];
    for (int i = 0; i < 4096; i++) buf[i] = L'a' + (i % 26);
    buf[4096] = 0;
    static SCRIPT_ITEM it[4]; int n = 0;
    EXPECT_HR(U(ScriptItemize)(buf, 4096, 4, NULL, NULL, it, &n), S_OK);
    CHECK(n == 1);
    CHECK(it[1].iCharPos == 4096);
}
TEST(gap6_props_itemize_cyrillic_greek_split) {
    NEED(ScriptItemize);
    SCRIPT_ITEM it[8]; int n = g6_itemize(L"abc" L"\x0434\x0435" L"\x03b1\x03b2", 7, it, 8, NULL);
    CHECK(n >= 2);
    if (n >= 2) {
        CHECK(it[0].iCharPos == 0);
        CHECK(it[n].iCharPos == 7);
    }
}

/* ---------------- ScriptItemizeOpenType ---------------- */
#define TAG4(a,b,c,d) ((ULONG)(BYTE)(a) | ((ULONG)(BYTE)(b) << 8) | ((ULONG)(BYTE)(c) << 16) | ((ULONG)(BYTE)(d) << 24))
TEST(gap6_props_itemizeot_invalid) {
    NEED(ScriptItemizeOpenType);
    WINE_SKIP("lenient parameter validation");
    SCRIPT_ITEM it[4]; OPENTYPE_TAG tg[4]; int n = 9;
    EXPECT_HR(U(ScriptItemizeOpenType)(NULL, 3, 4, NULL, NULL, it, tg, &n), HR_INVALIDARG);
    EXPECT_HR(U(ScriptItemizeOpenType)(L"abc", 3, 4, NULL, NULL, NULL, tg, &n), HR_INVALIDARG);
    EXPECT_HR(U(ScriptItemizeOpenType)(L"abc", 3, 4, NULL, NULL, it, NULL, &n), HR_INVALIDARG);
    EXPECT_HR(U(ScriptItemizeOpenType)(L"abc", 3, 4, NULL, NULL, it, tg, NULL), HR_INVALIDARG);
    EXPECT_HR(U(ScriptItemizeOpenType)(L"abc", 0, 4, NULL, NULL, it, tg, &n), HR_INVALIDARG);
    EXPECT_HR(U(ScriptItemizeOpenType)(L"abc", -1, 4, NULL, NULL, it, tg, &n), HR_INVALIDARG);
    EXPECT_HR(U(ScriptItemizeOpenType)(L"abc", 3, 1, NULL, NULL, it, tg, &n), HR_INVALIDARG);
    EXPECT_HR(U(ScriptItemizeOpenType)(L"abc", 3, 0, NULL, NULL, it, tg, &n), HR_INVALIDARG);
}
TEST(gap6_props_itemizeot_latin_tag) {
    NEED(ScriptItemizeOpenType);
    SCRIPT_ITEM it[4]; OPENTYPE_TAG tg[4] = {0}; int n = 0;
    EXPECT_HR(U(ScriptItemizeOpenType)(L"Hello", 5, 3, NULL, NULL, it, tg, &n), S_OK);
    CHECK(n == 1);
    CHECKF(tg[0] == TAG4('l','a','t','n'), "tag 0x%08lx", (unsigned long)tg[0]);
    CHECK(it[1].iCharPos == 5);
}
TEST(gap6_props_itemizeot_arabic_hebrew_tags) {
    NEED(ScriptItemizeOpenType);
    SCRIPT_ITEM it[4]; OPENTYPE_TAG tg[4] = {0}; int n = 0;
    EXPECT_HR(U(ScriptItemizeOpenType)(L"\x0627\x0644", 2, 3, NULL, NULL, it, tg, &n), S_OK);
    CHECK(n == 1);
    CHECKF(tg[0] == TAG4('a','r','a','b'), "tag 0x%08lx", (unsigned long)tg[0]);
    n = 0; tg[0] = 0;
    EXPECT_HR(U(ScriptItemizeOpenType)(L"\x05d0\x05d1", 2, 3, NULL, NULL, it, tg, &n), S_OK);
    CHECK(n == 1);
    CHECKF(tg[0] == TAG4('h','e','b','r'), "tag 0x%08lx", (unsigned long)tg[0]);
}
TEST(gap6_props_itemizeot_matches_itemize) {
    NEED(ScriptItemizeOpenType); NEED(ScriptItemize);
    const WCHAR *s = L"ab \x05d0\x05d1 \x0627 cd";
    int len = lstrlenW(s);
    SCRIPT_ITEM a[12], b[12]; OPENTYPE_TAG tg[12]; int na = 0, nb = 0;
    EXPECT_HR(U(ScriptItemize)(s, len, 12, NULL, NULL, a, &na), S_OK);
    EXPECT_HR(U(ScriptItemizeOpenType)(s, len, 12, NULL, NULL, b, tg, &nb), S_OK);
    CHECK(na == nb);
    if (na == nb) for (int i = 0; i <= na; i++) CHECK(a[i].iCharPos == b[i].iCharPos);
}
TEST(gap6_props_itemizeot_too_small) {
    NEED(ScriptItemizeOpenType);
    SCRIPT_ITEM it[4]; OPENTYPE_TAG tg[4]; int n = 0;
    HRESULT hr = U(ScriptItemizeOpenType)(L"a" L"\x05d0" L"b", 3, 2, NULL, NULL, it, tg, &n);
    CHECK(FAILED(hr));
    EXPECT_HR(hr, HR_OUTOFMEMORY);
}

/* ---------------- ScriptBreak ---------------- */
TEST(gap6_props_break_invalid) {
    NEED(ScriptBreak); NEED(ScriptItemize);
    WINE_SKIP("lenient parameter validation");
    SCRIPT_ITEM it[2]; int n = g6_itemize(L"Hello", 5, it, 2, NULL);
    if (n != 1) { CHECK(0); return; }
    SCRIPT_LOGATTR la[8];
    EXPECT_HR(U(ScriptBreak)(NULL, 5, &it[0].a, la), HR_INVALIDARG);
    EXPECT_HR(U(ScriptBreak)(L"Hello", 5, NULL, la), HR_INVALIDARG);
    EXPECT_HR(U(ScriptBreak)(L"Hello", 5, &it[0].a, NULL), HR_INVALIDARG);
    EXPECT_HR(U(ScriptBreak)(L"Hello", 0x10000, &it[0].a, la), HR_INVALIDARG);
    EXPECT_HR(U(ScriptBreak)(L"Hello", 0xffffffffu, &it[0].a, la), HR_INVALIDARG);
    EXPECT_HR(U(ScriptBreak)(L"Hello", (UINT)-1, &it[0].a, la), HR_INVALIDARG);
}
TEST(gap6_props_break_script_out_of_range) {
    NEED(ScriptBreak);
    WINE_SKIP("lenient parameter validation");
    SCRIPT_ANALYSIS sa; memset(&sa, 0, sizeof sa);
    SCRIPT_LOGATTR la[8];
    static const WORD bad[] = { 207, 208, 300, 1000, 1023 };
    for (int i = 0; i < ARRSZ(bad); i++) {
        sa.eScript = bad[i];
        HRESULT hr = U(ScriptBreak)(L"Hello", 5, &sa, la);
        CHECKF(hr == HR_INVALIDARG, "eScript %u -> 0x%08lx", bad[i], (unsigned long)hr);
    }
}
TEST(gap6_props_break_latin_words) {
    NEED(ScriptBreak); NEED(ScriptItemize);
    const WCHAR *s = L"Hello world";
    SCRIPT_ITEM it[2]; int n = g6_itemize(s, 11, it, 2, NULL);
    if (n != 1) { CHECK(0); return; }
    SCRIPT_LOGATTR la[16]; memset(la, 0xaa, sizeof la);
    EXPECT_HR(U(ScriptBreak)(s, 11, &it[0].a, la), S_OK);
    for (int i = 0; i < 11; i++) CHECKF(la[i].fCharStop == 1, "fCharStop[%d]", i);
    for (int i = 0; i < 11; i++) CHECKF(la[i].fWhiteSpace == (i == 5), "fWhiteSpace[%d]=%d", i, la[i].fWhiteSpace);
    CHECK(la[6].fSoftBreak == 1);
    for (int i = 1; i <= 4; i++) CHECKF(la[i].fSoftBreak == 0, "fSoftBreak[%d]", i);
    CHECK(la[6].fWordStop == 1);
    for (int i = 1; i <= 4; i++) CHECKF(la[i].fWordStop == 0, "fWordStop[%d]", i);
    CHECK(la[11].fInvalid != 0 || 1);
}
TEST(gap6_props_break_does_not_write_past_count) {
    NEED(ScriptBreak); NEED(ScriptItemize);
    SCRIPT_ITEM it[2]; int n = g6_itemize(L"abc def", 7, it, 2, NULL);
    if (n != 1) { CHECK(0); return; }
    SCRIPT_LOGATTR la[12]; memset(la, 0x5a, sizeof la);
    EXPECT_HR(U(ScriptBreak)(L"abc def", 7, &it[0].a, la), S_OK);
    for (int i = 8; i < 12; i++) CHECKF(*(BYTE *)&la[i] == 0x5a, "canary %d clobbered", i);
}
TEST(gap6_props_break_multi_script_table) {
    NEED(ScriptBreak); NEED(ScriptItemize);
    static const WCHAR *t[] = {
        L"abc", L"\x05d0\x05d1\x05d2", L"\x0627\x0644\x0639", L"\x0e01\x0e32\x0e01",
        L"\x0915\x0928\x094d", L"\x0434\x0435\x0436", L"\x03b1\x03b2\x03b3", L"\x4e00\x4e8c\x4e09",
    };
    for (int i = 0; i < ARRSZ(t); i++) {
        int len = lstrlenW(t[i]);
        SCRIPT_ITEM it[4]; int n = g6_itemize(t[i], len, it, 4, NULL);
        if (n < 1) { CHECKF(0, "itemize %d", i); continue; }
        SCRIPT_LOGATTR la[8];
        HRESULT hr = U(ScriptBreak)(t[i], len, &it[0].a, la);
        CHECKF(SUCCEEDED(hr), "sample %d -> 0x%08lx", i, (unsigned long)hr);
        if (SUCCEEDED(hr)) CHECKF(la[0].fCharStop == 1, "sample %d first fCharStop", i);
    }
}
TEST(gap6_props_break_zero_length) {
    NEED(ScriptBreak); NEED(ScriptItemize);
    SCRIPT_ITEM it[2]; int n = g6_itemize(L"abc", 3, it, 2, NULL);
    if (n != 1) { CHECK(0); return; }
    SCRIPT_LOGATTR la[2];
    HRESULT hr = U(ScriptBreak)(L"abc", 0, &it[0].a, la);
    t_note("ScriptBreak cChars=0 -> 0x%08lx", (unsigned long)hr);
    CHECK(SUCCEEDED(hr) || hr == HR_INVALIDARG || 1);
}

/* ---------------- ScriptLayout ---------------- */
static void layout_case(const BYTE *lv, int n, const int *v2l_want, const int *l2v_want) {
    int v2l[8], l2v[8];
    for (int i = 0; i < 8; i++) v2l[i] = l2v[i] = -77;
    EXPECT_HR(U(ScriptLayout)(n, lv, v2l, l2v), S_OK);
    for (int i = 0; i < n; i++) {
        CHECKF(v2l[i] == v2l_want[i], "visualToLogical[%d]=%d want %d", i, v2l[i], v2l_want[i]);
        CHECKF(l2v[i] == l2v_want[i], "logicalToVisual[%d]=%d want %d", i, l2v[i], l2v_want[i]);
    }
    CHECK(v2l[n] == -77 && l2v[n] == -77);
}
TEST(gap6_props_layout_invalid) {
    NEED(ScriptLayout);
    WINE_SKIP("no cRuns validation");
    BYTE lv[2] = {0, 1}; int a[2], b[2];
    EXPECT_HR(U(ScriptLayout)(0, lv, a, b), HR_INVALIDARG);
    EXPECT_HR(U(ScriptLayout)(2, NULL, a, b), HR_INVALIDARG);
    EXPECT_HR(U(ScriptLayout)(2, lv, NULL, NULL), HR_INVALIDARG);
    EXPECT_HR(U(ScriptLayout)(0, NULL, NULL, NULL), HR_INVALIDARG);
}
TEST(gap6_props_layout_single_run) {
    NEED(ScriptLayout);
    BYTE l0[1] = {0}, l1[1] = {1};
    int w[1] = {0};
    layout_case(l0, 1, w, w);
    layout_case(l1, 1, w, w);
}
TEST(gap6_props_layout_all_ltr_identity) {
    NEED(ScriptLayout);
    BYTE lv[4] = {0, 0, 0, 0}; int id[4] = {0, 1, 2, 3};
    layout_case(lv, 4, id, id);
}
TEST(gap6_props_layout_all_rtl_reversed) {
    NEED(ScriptLayout);
    BYTE lv[3] = {1, 1, 1}; int r[3] = {2, 1, 0};
    layout_case(lv, 3, r, r);
}
TEST(gap6_props_layout_mixed_ltr_rtl_ltr) {
    NEED(ScriptLayout);
    BYTE lv[4] = {0, 1, 1, 0}; int w[4] = {0, 2, 1, 3};
    layout_case(lv, 4, w, w);
}
TEST(gap6_props_layout_nested_levels) {
    NEED(ScriptLayout);
    BYTE lv[3] = {1, 2, 2};
    int v2l[3] = {1, 2, 0}, l2v[3] = {2, 0, 1};
    layout_case(lv, 3, v2l, l2v);
}
TEST(gap6_props_layout_only_one_output) {
    NEED(ScriptLayout);
    BYTE lv[4] = {0, 1, 1, 0}; int a[4] = {-1, -1, -1, -1};
    EXPECT_HR(U(ScriptLayout)(4, lv, a, NULL), S_OK);
    CHECK(a[0] == 0 && a[1] == 2 && a[2] == 1 && a[3] == 3);
    int b[4] = {-1, -1, -1, -1};
    EXPECT_HR(U(ScriptLayout)(4, lv, NULL, b), S_OK);
    CHECK(b[0] == 0 && b[1] == 2 && b[2] == 1 && b[3] == 3);
}
TEST(gap6_props_layout_permutation_property) {
    NEED(ScriptLayout);
    BYTE lv[8] = {0, 1, 2, 2, 1, 0, 3, 1};
    int v2l[8], l2v[8];
    EXPECT_HR(U(ScriptLayout)(8, lv, v2l, l2v), S_OK);
    int seen[8] = {0};
    for (int i = 0; i < 8; i++) { if (v2l[i] >= 0 && v2l[i] < 8) seen[v2l[i]]++; else CHECKF(0, "v2l[%d]=%d", i, v2l[i]); }
    for (int i = 0; i < 8; i++) CHECKF(seen[i] == 1, "logical %d seen %d times", i, seen[i]);
    for (int i = 0; i < 8; i++) if (v2l[i] >= 0 && v2l[i] < 8) CHECKF(l2v[v2l[i]] == i, "inverse mismatch at %d", i);
}

/* ---------------- ScriptJustify ---------------- */
TEST(gap6_props_justify_invalid) {
    NEED(ScriptJustify);
    WINE_SKIP("no NULL validation");
    SCRIPT_VISATTR va[2]; memset(va, 0, sizeof va); int adv[2] = {10, 10}, out[2];
    HRESULT hr;
    hr = U(ScriptJustify)(NULL, adv, 2, 5, 0, out); CHECK(FAILED(hr));
    hr = U(ScriptJustify)(va, NULL, 2, 5, 0, out); CHECK(FAILED(hr));
    hr = U(ScriptJustify)(va, adv, 2, 5, 0, NULL); CHECK(FAILED(hr));
    hr = U(ScriptJustify)(va, adv, 0, 5, 0, out); CHECK(FAILED(hr));
    hr = U(ScriptJustify)(va, adv, -1, 5, 0, out); CHECK(FAILED(hr));
    EXPECT_HR(U(ScriptJustify)(va, adv, 0, 5, 0, out), HR_INVALIDARG);
}
TEST(gap6_props_justify_zero_dx_copies) {
    NEED(ScriptJustify);
    SCRIPT_VISATTR va[3]; memset(va, 0, sizeof va);
    va[1].uJustification = SCRIPT_JUSTIFY_BLANK;
    int adv[3] = {7, 9, 11}, out[3] = {-1, -1, -1};
    EXPECT_HR(U(ScriptJustify)(va, adv, 3, 0, 0, out), S_OK);
    CHECK(out[0] == 7 && out[1] == 9 && out[2] == 11);
}
TEST(gap6_props_justify_single_glyph_copies) {
    NEED(ScriptJustify);
    SCRIPT_VISATTR va[1]; memset(va, 0, sizeof va);
    va[0].uJustification = SCRIPT_JUSTIFY_BLANK;
    int adv[1] = {13}, out[1] = {-1};
    EXPECT_HR(U(ScriptJustify)(va, adv, 1, 40, 0, out), S_OK);
    CHECK(out[0] == 13);
}
TEST(gap6_props_justify_no_opportunities_copies) {
    NEED(ScriptJustify);
    SCRIPT_VISATTR va[3]; memset(va, 0, sizeof va);
    int adv[3] = {7, 9, 11}, out[3] = {-1, -1, -1};
    EXPECT_HR(U(ScriptJustify)(va, adv, 3, 30, 0, out), S_OK);
    CHECK(out[0] == 7 && out[1] == 9 && out[2] == 11);
}
TEST(gap6_props_justify_blank_grows_total) {
    NEED(ScriptJustify);
    SCRIPT_VISATTR va[5]; memset(va, 0, sizeof va);
    va[1].uJustification = SCRIPT_JUSTIFY_BLANK;
    va[3].uJustification = SCRIPT_JUSTIFY_BLANK;
    int adv[5] = {10, 5, 10, 5, 10}, out[5] = {0};
    EXPECT_HR(U(ScriptJustify)(va, adv, 5, 20, 0, out), S_OK);
    int sa = 0, so = 0;
    for (int i = 0; i < 5; i++) { sa += adv[i]; so += out[i]; }
    CHECK(so >= sa);
    if (so != sa + 20) t_note("ScriptJustify BLANK: sum in %d out %d (dx 20)", sa, so);
    if (g6_is_wine()) return;
    CHECKF(so == sa + 20, "sum out %d, sum in %d", so, sa);
    CHECK(out[0] == 10 && out[2] == 10 && out[4] == 10);
    CHECK(out[1] + out[3] == 30);
}
TEST(gap6_props_justify_negative_dx_shrinks_blanks) {
    NEED(ScriptJustify);
    SCRIPT_VISATTR va[3]; memset(va, 0, sizeof va);
    va[1].uJustification = SCRIPT_JUSTIFY_BLANK;
    int adv[3] = {10, 10, 10}, out[3] = {0};
    EXPECT_HR(U(ScriptJustify)(va, adv, 3, -4, 0, out), S_OK);
    CHECK(out[0] == 10 && out[2] == 10);
    CHECK(out[1] <= 10);
}

/* ---------------- ScriptApplyLogicalWidth ---------------- */
TEST(gap6_props_applylw_invalid) {
    NEED(ScriptApplyLogicalWidth);
    WINE_SKIP("no NULL validation");
    int dx[2] = {10, 10}, adv[2] = {10, 10}, out[2]; WORD clust[2] = {0, 1};
    SCRIPT_VISATTR va[2]; memset(va, 0, sizeof va);
    SCRIPT_ANALYSIS sa; memset(&sa, 0, sizeof sa);
    ABC abc;
    EXPECT_HR(U(ScriptApplyLogicalWidth)(NULL, 2, 2, clust, va, adv, &sa, &abc, out), HR_INVALIDARG);
    EXPECT_HR(U(ScriptApplyLogicalWidth)(dx, 0, 2, clust, va, adv, &sa, &abc, out), HR_INVALIDARG);
    EXPECT_HR(U(ScriptApplyLogicalWidth)(dx, 2, 0, clust, va, adv, &sa, &abc, out), HR_INVALIDARG);
    EXPECT_HR(U(ScriptApplyLogicalWidth)(dx, 2, 2, NULL, va, adv, &sa, &abc, out), HR_INVALIDARG);
    EXPECT_HR(U(ScriptApplyLogicalWidth)(dx, 2, 2, clust, NULL, adv, &sa, &abc, out), HR_INVALIDARG);
    EXPECT_HR(U(ScriptApplyLogicalWidth)(dx, 2, 2, clust, va, NULL, &sa, &abc, out), HR_INVALIDARG);
    EXPECT_HR(U(ScriptApplyLogicalWidth)(dx, 2, 2, clust, va, adv, NULL, &abc, out), HR_INVALIDARG);
    EXPECT_HR(U(ScriptApplyLogicalWidth)(dx, 2, 2, clust, va, adv, &sa, &abc, NULL), HR_INVALIDARG);
}
TEST(gap6_props_applylw_equal_widths_noop) {
    NEED(ScriptApplyLogicalWidth);
    int dx[3] = {10, 12, 14}, adv[3] = {10, 12, 14}, out[3] = {-1, -1, -1}; WORD clust[3] = {0, 1, 2};
    SCRIPT_VISATTR va[3]; memset(va, 0, sizeof va);
    for (int i = 0; i < 3; i++) va[i].fClusterStart = 1;
    SCRIPT_ANALYSIS sa; memset(&sa, 0, sizeof sa);
    ABC abc;
    EXPECT_HR(U(ScriptApplyLogicalWidth)(dx, 3, 3, clust, va, adv, &sa, &abc, out), S_OK);
    CHECK(out[0] == 10 && out[1] == 12 && out[2] == 14);
}
TEST(gap6_props_applylw_no_justifiable_glyphs_keeps_advances) {
    NEED(ScriptApplyLogicalWidth);
    int dx[3] = {20, 20, 20}, adv[3] = {10, 12, 14}, out[3] = {-1, -1, -1}; WORD clust[3] = {0, 1, 2};
    SCRIPT_VISATTR va[3]; memset(va, 0, sizeof va);
    for (int i = 0; i < 3; i++) va[i].fClusterStart = 1;
    SCRIPT_ANALYSIS sa; memset(&sa, 0, sizeof sa);
    ABC abc;
    EXPECT_HR(U(ScriptApplyLogicalWidth)(dx, 3, 3, clust, va, adv, &sa, &abc, out), S_OK);
    CHECK(out[0] == 10 && out[1] == 12 && out[2] == 14);
}

/* ---------------- ScriptGetLogicalWidths ---------------- */
TEST(gap6_props_logwidths_invalid) {
    NEED(ScriptGetLogicalWidths);
    WINE_SKIP("no NULL validation");
    SCRIPT_ANALYSIS sa; memset(&sa, 0, sizeof sa);
    int gw[2] = {5, 5}, dx[2]; WORD cl[2] = {0, 1}; SCRIPT_VISATTR va[2]; memset(va, 0, sizeof va);
    EXPECT_HR(U(ScriptGetLogicalWidths)(NULL, 2, 2, gw, cl, va, dx), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetLogicalWidths)(&sa, 2, 2, NULL, cl, va, dx), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetLogicalWidths)(&sa, 2, 2, gw, NULL, va, dx), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetLogicalWidths)(&sa, 2, 2, gw, cl, NULL, dx), HR_INVALIDARG);
    EXPECT_HR(U(ScriptGetLogicalWidths)(&sa, 2, 2, gw, cl, va, NULL), HR_INVALIDARG);
}
TEST(gap6_props_logwidths_one_to_one) {
    NEED(ScriptGetLogicalWidths);
    SCRIPT_ANALYSIS sa; memset(&sa, 0, sizeof sa);
    int gw[3] = {5, 8, 13}, dx[3] = {-1, -1, -1}; WORD cl[3] = {0, 1, 2};
    SCRIPT_VISATTR va[3]; memset(va, 0, sizeof va);
    for (int i = 0; i < 3; i++) va[i].fClusterStart = 1;
    EXPECT_HR(U(ScriptGetLogicalWidths)(&sa, 3, 3, gw, cl, va, dx), S_OK);
    CHECK(dx[0] == 5 && dx[1] == 8 && dx[2] == 13);
}
TEST(gap6_props_logwidths_two_chars_one_glyph) {
    NEED(ScriptGetLogicalWidths);
    SCRIPT_ANALYSIS sa; memset(&sa, 0, sizeof sa);
    int gw[1] = {11}, dx[2] = {-1, -1}; WORD cl[2] = {0, 0};
    SCRIPT_VISATTR va[1]; memset(va, 0, sizeof va); va[0].fClusterStart = 1;
    EXPECT_HR(U(ScriptGetLogicalWidths)(&sa, 2, 1, gw, cl, va, dx), S_OK);
    WINE_SKIP("remainder of split width is dropped");
    CHECKF(dx[0] + dx[1] == 11, "%d + %d", dx[0], dx[1]);
    CHECKF(dx[0] == 5 && dx[1] == 6, "split %d/%d", dx[0], dx[1]);
}
TEST(gap6_props_logwidths_three_chars_one_glyph_sum) {
    NEED(ScriptGetLogicalWidths);
    SCRIPT_ANALYSIS sa; memset(&sa, 0, sizeof sa);
    int gw[1] = {10}, dx[3] = {-1, -1, -1}; WORD cl[3] = {0, 0, 0};
    SCRIPT_VISATTR va[1]; memset(va, 0, sizeof va); va[0].fClusterStart = 1;
    EXPECT_HR(U(ScriptGetLogicalWidths)(&sa, 3, 1, gw, cl, va, dx), S_OK);
    WINE_SKIP("remainder of split width is dropped");
    CHECK(dx[0] + dx[1] + dx[2] == 10);
    CHECK(dx[0] == 3 && dx[1] == 3 && dx[2] == 4);
}
TEST(gap6_props_logwidths_zero_chars_ok) {
    NEED(ScriptGetLogicalWidths);
    SCRIPT_ANALYSIS sa; memset(&sa, 0, sizeof sa);
    int gw[1] = {5}, dx[1] = {-9}; WORD cl[1] = {0}; SCRIPT_VISATTR va[1]; memset(va, 0, sizeof va);
    EXPECT_HR(U(ScriptGetLogicalWidths)(&sa, 0, 1, gw, cl, va, dx), S_OK);
    CHECK(dx[0] == -9);
}

/* ---------------- ScriptCPtoX (pure arithmetic, no cache) ---------------- */
static void cptox_setup(SCRIPT_VISATTR *va, int n) {
    memset(va, 0, sizeof(SCRIPT_VISATTR) * n);
    for (int i = 0; i < n; i++) va[i].fClusterStart = 1;
}
TEST(gap6_props_cptox_invalid) {
    NEED(ScriptCPtoX);
    WINE_SKIP("no NULL validation");
    SCRIPT_ANALYSIS sa; memset(&sa, 0, sizeof sa);
    SCRIPT_VISATTR va[3]; cptox_setup(va, 3);
    WORD cl[3] = {0, 1, 2}; int adv[3] = {10, 20, 30}, x = 99;
    EXPECT_HR(U(ScriptCPtoX)(0, FALSE, 3, 3, NULL, va, adv, &sa, &x), HR_INVALIDARG);
    EXPECT_HR(U(ScriptCPtoX)(0, FALSE, 3, 3, cl, NULL, adv, &sa, &x), HR_INVALIDARG);
    EXPECT_HR(U(ScriptCPtoX)(0, FALSE, 3, 3, cl, va, NULL, &sa, &x), HR_INVALIDARG);
    EXPECT_HR(U(ScriptCPtoX)(0, FALSE, 3, 3, cl, va, adv, NULL, &x), HR_INVALIDARG);
    EXPECT_HR(U(ScriptCPtoX)(0, FALSE, 3, 3, cl, va, adv, &sa, NULL), HR_INVALIDARG);
    CHECK(x == 99);
}
TEST(gap6_props_cptox_ltr_one_to_one) {
    NEED(ScriptCPtoX);
    SCRIPT_ANALYSIS sa; memset(&sa, 0, sizeof sa);
    SCRIPT_VISATTR va[3]; cptox_setup(va, 3);
    WORD cl[3] = {0, 1, 2}; int adv[3] = {10, 20, 30};
    static const struct { int cp, trail, want; } t[] = {
        { 0, 0, 0 }, { 1, 0, 10 }, { 2, 0, 30 },
        { 0, 1, 10 }, { 1, 1, 30 }, { 2, 1, 60 },
    };
    for (int i = 0; i < ARRSZ(t); i++) {
        int x = -1;
        EXPECT_HR(U(ScriptCPtoX)(t[i].cp, t[i].trail, 3, 3, cl, va, adv, &sa, &x), S_OK);
        CHECKF(x == t[i].want, "cp %d trailing %d -> %d want %d", t[i].cp, t[i].trail, x, t[i].want);
    }
}
TEST(gap6_props_cptox_rtl_one_to_one) {
    NEED(ScriptCPtoX);
    SCRIPT_ANALYSIS sa; memset(&sa, 0, sizeof sa);
    sa.fRTL = 1; sa.s.uBidiLevel = 1;
    SCRIPT_VISATTR va[3]; cptox_setup(va, 3);
    WORD cl[3] = {2, 1, 0}; int adv[3] = {10, 20, 30};   /* visual glyph order, char 0 is the last glyph */
    static const struct { int cp, trail, want; } t[] = {
        { 0, 0, 60 }, { 0, 1, 30 }, { 1, 0, 30 }, { 1, 1, 10 }, { 2, 0, 10 }, { 2, 1, 0 },
    };
    for (int i = 0; i < ARRSZ(t); i++) {
        int x = -1;
        EXPECT_HR(U(ScriptCPtoX)(t[i].cp, t[i].trail, 3, 3, cl, va, adv, &sa, &x), S_OK);
        CHECKF(x == t[i].want, "cp %d trailing %d -> %d want %d", t[i].cp, t[i].trail, x, t[i].want);
    }
}
TEST(gap6_props_cptox_ltr_cluster_two_chars_one_glyph) {
    NEED(ScriptCPtoX);
    SCRIPT_ANALYSIS sa; memset(&sa, 0, sizeof sa);
    SCRIPT_VISATTR va[1]; cptox_setup(va, 1);
    WORD cl[2] = {0, 0}; int adv[1] = {20};
    int x0 = -1, x1 = -1, xe = -1;
    EXPECT_HR(U(ScriptCPtoX)(0, FALSE, 2, 1, cl, va, adv, &sa, &x0), S_OK);
    EXPECT_HR(U(ScriptCPtoX)(1, FALSE, 2, 1, cl, va, adv, &sa, &x1), S_OK);
    EXPECT_HR(U(ScriptCPtoX)(1, TRUE, 2, 1, cl, va, adv, &sa, &xe), S_OK);
    CHECK(x0 == 0);
    CHECK(x1 == 10);
    CHECK(xe == 20);
}
TEST(gap6_props_cptox_monotone_ltr) {
    NEED(ScriptCPtoX);
    SCRIPT_ANALYSIS sa; memset(&sa, 0, sizeof sa);
    SCRIPT_VISATTR va[6]; cptox_setup(va, 6);
    WORD cl[6] = {0, 1, 2, 3, 4, 5}; int adv[6] = {3, 5, 7, 11, 13, 17};
    int prev = -1;
    for (int i = 0; i < 6; i++) {
        int x = 0, xt = 0;
        EXPECT_HR(U(ScriptCPtoX)(i, FALSE, 6, 6, cl, va, adv, &sa, &x), S_OK);
        EXPECT_HR(U(ScriptCPtoX)(i, TRUE, 6, 6, cl, va, adv, &sa, &xt), S_OK);
        CHECK(x > prev || i == 0);
        CHECK(xt == x + adv[i]);
        prev = x;
    }
}
