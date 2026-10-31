/* OPM / DDC-CI / DX-interop / sprite / DWM stubs. Every function here is a thin syscall stub in
 * gdi32.dll (gdi32full.dll for a few); only deliberately invalid arguments are used, so nothing
 * touches a monitor, the DWM or any DX surface. */
#include "g2.h"

UDECL(LONG, DestroyOPMProtectedOutput, (HANDLE));
UDECL(LONG, SetOPMSigningKeyAndSequenceNumbers, (HANDLE, PVOID));
UDECL(LONG, DDCCIGetCapabilitiesString, (HANDLE, LPSTR, DWORD));
UDECL(LONG, DDCCIGetCapabilitiesStringLength, (HANDLE, DWORD *));
UDECL(LONG, DDCCIGetTimingReport, (HANDLE, void *));
UDECL(LONG, DDCCIGetVCPFeature, (HANDLE, DWORD, DWORD *, DWORD *, DWORD *));
UDECL(LONG, DDCCISaveCurrentSettings, (HANDLE));
UDECL(LONG, DDCCISetVCPFeature, (HANDLE, DWORD, DWORD));
UDECL(ULONG_PTR, BeginGdiRendering, (HBITMAP, ULONG, ULONGLONG));
UDECL(ULONG_PTR, EndGdiRendering, (HBITMAP, ULONG, ULONG *, ULONGLONG));
UDECL(ULONG_PTR, DwmCreatedBitmapRemotingOutput, (void));
UDECL(BOOL, DwmGetDirtyRgnImpl, (DWORD, LPWSTR, LPWSTR, UINT));
UDECL(LONG, DdCreateFullscreenSprite, (HDC, ULONG, HANDLE *, HDC *));
UDECL(LONG, DdDestroyFullscreenSprite, (HDC, HANDLE));
UDECL(LONG, DdNotifyFullscreenSpriteUpdate, (HDC, HANDLE));
UDECL(ULONG, DdQueryVisRgnUniqueness, (void));

#define G2_INVPARAM ((LONG)0xC000000D)

/* physical monitor handles that can never be owned by this process */
static const ULONG_PTR mon_bad[] = { 0, (ULONG_PTR)-1, 0x100000000ULL, 0xdeadbeef, 0x7fffffff, 0xffffffff, 0xffffffffffffff00ULL };
#define NMON ((int)(sizeof mon_bad / sizeof mon_bad[0]))

TEST(gap2_stub_gdi32_exports_present) {
    static const char *const names[] = {
        "DestroyOPMProtectedOutput", "SetOPMSigningKeyAndSequenceNumbers", "DDCCIGetCapabilitiesString",
        "DDCCIGetCapabilitiesStringLength", "DDCCIGetTimingReport", "DDCCIGetVCPFeature", "DDCCISaveCurrentSettings",
        "DDCCISetVCPFeature", "BeginGdiRendering", "EndGdiRendering", "DwmCreatedBitmapRemotingOutput",
        "DdCreateFullscreenSprite", "DdDestroyFullscreenSprite", "DdNotifyFullscreenSpriteUpdate",
        "DdQueryVisRgnUniqueness", "InternalDeleteDC", "InternalDeleteObject", "AddFontResourceTracking" };
    if (g2_wine()) { t_note("wine: gdi32 export presence not enforced"); }
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) {
        FARPROC p = t_proc_gdi32(names[i]);
        if (g2_wine()) { if (!p) t_note("wine lacks gdi32!%s", names[i]); continue; }
        CHECKF(p != NULL, "gdi32!%s", names[i]);
    }
}
TEST(gap2_stub_gdi32full_exports_present) {
    static const char *const names[] = {
        "DwmCreatedBitmapRemotingOutput", "DwmGetDirtyRgnImpl", "MF_DeleteObject", "MF16_DeleteObject", "AbortDocImpl",
        "AddFontResourceTracking", "AddFontResourceWImpl", "BeginPathImpl", "CloseFigureImpl", "CreateDCAImpl",
        "CreateFontIndirectWImpl", "CreateScalableFontResourceWImpl", "EndDocImpl", "EndPageImpl", "EndPathImpl",
        "EscapeImpl", "ExtEscapeImpl", "ExtSelectClipRgnImpl", "ExtTextOutAImpl", "ExtTextOutWImpl", "FillPathImpl",
        "GetClipBoxImpl", "IntersectClipRectImpl", "LineToImpl" };
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) {
        FARPROC p = t_proc_gdi32full(names[i]);
        if (g2_wine()) { if (!p) t_note("wine lacks gdi32full!%s", names[i]); continue; }
        CHECKF(p != NULL, "gdi32full!%s", names[i]);
    }
}
TEST(gap2_stub_impl_names_not_in_gdi32) {
    /* the *Impl names are gdi32full-only; gdi32 does not export them */
    static const char *const names[] = { "DwmGetDirtyRgnImpl", "MF_DeleteObject", "MF16_DeleteObject", "AbortDocImpl",
        "AddFontResourceWImpl", "BeginPathImpl", "EndPathImpl", "LineToImpl", "FillPathImpl", "EscapeImpl" };
    if (g2_wine()) { t_skip("wine export layout differs"); return; }
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++)
        CHECKF(t_proc_gdi32(names[i]) == NULL, "gdi32!%s unexpectedly present", names[i]);
}

/* ---------------- OPM ---------------- */
TEST(gap2_opm_destroy_invalid_handles) {
    REQUIRE(G32(DestroyOPMProtectedOutput));
    SKIP_ON_WINE("OPM stubs unimplemented");
    for (int i = 0; i < NMON; i++) {
        LONG r = G32(DestroyOPMProtectedOutput)((HANDLE)mon_bad[i]);
        CHECKF(r < 0, "handle %#llx -> %#lx", (unsigned long long)mon_bad[i], r);
    }
}
TEST(gap2_opm_destroy_repeatable) {
    REQUIRE(G32(DestroyOPMProtectedOutput));
    SKIP_ON_WINE("OPM stubs unimplemented");
    LONG a = G32(DestroyOPMProtectedOutput)(NULL), b = G32(DestroyOPMProtectedOutput)(NULL);
    CHECK(a < 0); G2_EQ(a, b);
}
TEST(gap2_opm_set_signing_key_invalid_handles) {
    REQUIRE(G32(SetOPMSigningKeyAndSequenceNumbers));
    SKIP_ON_WINE("OPM stubs unimplemented");
    BYTE params[256]; memset(params, 0, sizeof params);
    for (int i = 0; i < NMON; i++) {
        LONG r = G32(SetOPMSigningKeyAndSequenceNumbers)((HANDLE)mon_bad[i], params);
        CHECKF(r < 0, "handle %#llx -> %#lx", (unsigned long long)mon_bad[i], r);
    }
}
TEST(gap2_opm_set_signing_key_garbage_params) {
    REQUIRE(G32(SetOPMSigningKeyAndSequenceNumbers));
    SKIP_ON_WINE("OPM stubs unimplemented");
    BYTE params[256]; memset(params, 0xA5, sizeof params);
    LONG r = G32(SetOPMSigningKeyAndSequenceNumbers)(NULL, params);
    CHECK(r < 0);
    t_note("garbage params status %#lx", r);
}

/* ---------------- DDC/CI ---------------- */
TEST(gap2_ddcci_caps_string_zero_length) {
    REQUIRE(G32(DDCCIGetCapabilitiesString));
    SKIP_ON_WINE("DDC/CI stubs unimplemented");
    char buf[16]; memset(buf, 'x', sizeof buf);
    /* a zero length is rejected before the monitor handle is even looked at */
    for (int i = 0; i < NMON; i++) {
        LONG r = G32(DDCCIGetCapabilitiesString)((HANDLE)mon_bad[i], buf, 0);
        CHECKF(r == G2_INVPARAM, "handle %#llx -> %#lx", (unsigned long long)mon_bad[i], r);
    }
    CHECK(buf[0] == 'x' && buf[15] == 'x');
}
TEST(gap2_ddcci_caps_string_invalid_handle) {
    REQUIRE(G32(DDCCIGetCapabilitiesString));
    SKIP_ON_WINE("DDC/CI stubs unimplemented");
    static const DWORD lens[] = { 1, 2, 16, 256, 4096 };
    for (int i = 0; i < NMON; i++)
        for (unsigned j = 0; j < sizeof lens / sizeof lens[0]; j++) {
            char buf[4096]; memset(buf, 'x', sizeof buf);
            LONG r = G32(DDCCIGetCapabilitiesString)((HANDLE)mon_bad[i], buf, lens[j]);
            CHECKF(r < 0, "handle %#llx len %lu -> %#lx", (unsigned long long)mon_bad[i], lens[j], r);
            CHECKF(buf[0] == 'x', "buffer written on failure");
        }
}
TEST(gap2_ddcci_caps_len_invalid_handle) {
    REQUIRE(G32(DDCCIGetCapabilitiesStringLength));
    SKIP_ON_WINE("DDC/CI stubs unimplemented");
    for (int i = 0; i < NMON; i++) {
        DWORD len = 0xdeadbeef;
        LONG r = G32(DDCCIGetCapabilitiesStringLength)((HANDLE)mon_bad[i], &len);
        CHECKF(r < 0, "handle %#llx -> %#lx", (unsigned long long)mon_bad[i], r);
        CHECKF(len == 0xdeadbeef, "length written on failure: %#lx", len);
    }
}
TEST(gap2_ddcci_timing_report_invalid_handle) {
    REQUIRE(G32(DDCCIGetTimingReport));
    SKIP_ON_WINE("DDC/CI stubs unimplemented");
    for (int i = 0; i < NMON; i++) {
        BYTE rep[16]; memset(rep, 0xCC, sizeof rep);
        LONG r = G32(DDCCIGetTimingReport)((HANDLE)mon_bad[i], rep);
        CHECKF(r < 0, "handle %#llx -> %#lx", (unsigned long long)mon_bad[i], r);
        int untouched = 1; for (int k = 0; k < 16; k++) if (rep[k] != 0xCC) untouched = 0;
        CHECKF(untouched, "report written on failure");
    }
}
TEST(gap2_ddcci_get_vcp_invalid_handle) {
    REQUIRE(G32(DDCCIGetVCPFeature));
    SKIP_ON_WINE("DDC/CI stubs unimplemented");
    static const DWORD codes[] = { 0, 0x10, 0x12, 0xD6, 0xFF, 0xFFFFFFFF };
    for (int i = 0; i < NMON; i++)
        for (unsigned j = 0; j < sizeof codes / sizeof codes[0]; j++) {
            DWORD type = 0xdeadbeef, cur = 0xdeadbeef, mx = 0xdeadbeef;
            LONG r = G32(DDCCIGetVCPFeature)((HANDLE)mon_bad[i], codes[j], &type, &cur, &mx);
            CHECKF(r < 0, "handle %#llx code %#lx -> %#lx", (unsigned long long)mon_bad[i], codes[j], r);
            CHECK(type == 0xdeadbeef && cur == 0xdeadbeef && mx == 0xdeadbeef);
        }
}
TEST(gap2_ddcci_get_vcp_optional_outputs_null) {
    REQUIRE(G32(DDCCIGetVCPFeature));
    SKIP_ON_WINE("DDC/CI stubs unimplemented");
    DWORD cur = 0xdeadbeef;
    LONG r = G32(DDCCIGetVCPFeature)(NULL, 0x10, NULL, &cur, NULL);
    CHECK(r < 0); CHECK(cur == 0xdeadbeef);
}
TEST(gap2_ddcci_set_vcp_invalid_handle) {
    REQUIRE(G32(DDCCISetVCPFeature));
    SKIP_ON_WINE("DDC/CI stubs unimplemented");
    /* never reaches hardware: the handle is rejected first (value 0 for brightness would be a visible change) */
    for (int i = 0; i < NMON; i++) {
        LONG r = G32(DDCCISetVCPFeature)((HANDLE)mon_bad[i], 0x10, 0);
        CHECKF(r < 0, "handle %#llx -> %#lx", (unsigned long long)mon_bad[i], r);
    }
}
TEST(gap2_ddcci_save_settings_invalid_handle) {
    REQUIRE(G32(DDCCISaveCurrentSettings));
    SKIP_ON_WINE("DDC/CI stubs unimplemented");
    for (int i = 0; i < NMON; i++) {
        LONG r = G32(DDCCISaveCurrentSettings)((HANDLE)mon_bad[i]);
        CHECKF(r < 0, "handle %#llx -> %#lx", (unsigned long long)mon_bad[i], r);
    }
}
TEST(gap2_ddcci_same_status_for_all_bad_handles) {
    REQUIRE(G32(DDCCISaveCurrentSettings)); REQUIRE(G32(DDCCIGetCapabilitiesStringLength));
    SKIP_ON_WINE("DDC/CI stubs unimplemented");
    /* the table lookup yields one fixed failure status for unknown handles */
    LONG first = G32(DDCCISaveCurrentSettings)((HANDLE)mon_bad[0]);
    for (int i = 1; i < NMON; i++) G2_EQ(G32(DDCCISaveCurrentSettings)((HANDLE)mon_bad[i]), first);
    DWORD len;
    LONG f2 = G32(DDCCIGetCapabilitiesStringLength)(NULL, &len);
    for (int i = 1; i < NMON; i++) G2_EQ(G32(DDCCIGetCapabilitiesStringLength)((HANDLE)mon_bad[i], &len), f2);
    t_note("unknown-monitor-handle status %#lx", first);
}

/* ---------------- BeginGdiRendering / EndGdiRendering ---------------- */
TEST(gap2_gdirender_begin_invalid_bitmaps) {
    REQUIRE(G32(BeginGdiRendering));
    SKIP_ON_WINE("DX interop stubs unimplemented");
    for (int i = 0; i < G2_NJUNK; i++) {
        ULONG_PTR r = G32(BeginGdiRendering)((HBITMAP)g2_junk[i], 0, 0);
        CHECKF((DWORD)r == 0, "bitmap %#llx -> %#llx", (unsigned long long)g2_junk[i], (unsigned long long)r);
        r = G32(BeginGdiRendering)((HBITMAP)g2_junk[i], 1, 0);
        CHECK((DWORD)r == 0);
    }
}
TEST(gap2_gdirender_begin_plain_bitmaps) {
    REQUIRE(G32(BeginGdiRendering));
    SKIP_ON_WINE("DX interop stubs unimplemented");
    /* ordinary GDI bitmaps are not DirectX surfaces: no interop path, FALSE, bitmap untouched */
    G2DC d; if (!g2_mk(&d, 8, 8)) { CHECK(0); return; }
    CHECK((DWORD)G32(BeginGdiRendering)(d.bmp, 0, 0) == 0);
    CHECK((DWORD)G32(BeginGdiRendering)(d.bmp, 1, 0) == 0);
    d.bits[0] = 0x123456;
    CHECK(g2_px(&d, 0, 0) == 0x123456);
    HBITMAP cb = U(CreateBitmap)(4, 4, 1, 32, NULL);
    CHECK(cb != NULL);
    CHECK((DWORD)G32(BeginGdiRendering)(cb, 0, 0) == 0);
    G2_EQ(U(GetObjectType)(cb), OBJ_BITMAP);
    U(DeleteObject)(cb);
    g2_free(&d);
}
TEST(gap2_gdirender_begin_wrong_object_types) {
    REQUIRE(G32(BeginGdiRendering));
    SKIP_ON_WINE("DX interop stubs unimplemented");
    HGDIOBJ objs[4];
    objs[0] = U(CreateSolidBrush)(RGB(1, 2, 3));
    objs[1] = U(CreatePen)(PS_SOLID, 1, 0);
    objs[2] = U(CreateRectRgn)(0, 0, 1, 1);
    objs[3] = U(GetStockObject)(BLACK_BRUSH);
    for (int i = 0; i < 4; i++) {
        CHECKF((DWORD)G32(BeginGdiRendering)((HBITMAP)objs[i], 0, 0) == 0, "obj %d", i);
        CHECKF(U(GetObjectType)(objs[i]) != 0, "object %d destroyed", i);
    }
    for (int i = 0; i < 3; i++) U(DeleteObject)(objs[i]);
}
TEST(gap2_gdirender_end_invalid_bitmaps_reports_zero) {
    REQUIRE(G32(EndGdiRendering));
    SKIP_ON_WINE("DX interop stubs unimplemented");
    for (int i = 0; i < G2_NJUNK; i++) {
        ULONG out = 0xdeadbeef;
        ULONG_PTR r = G32(EndGdiRendering)((HBITMAP)g2_junk[i], 0, &out, 0);
        CHECKF((DWORD)r == 0, "bitmap %#llx -> %#llx", (unsigned long long)g2_junk[i], (unsigned long long)r);
        CHECKF(out == 0, "out flag %#lx", out);
    }
}
TEST(gap2_gdirender_end_null_out_pointer) {
    REQUIRE(G32(EndGdiRendering));
    SKIP_ON_WINE("DX interop stubs unimplemented");
    CHECK((DWORD)G32(EndGdiRendering)(NULL, 0, NULL, 0) == 0);
    G2DC d; if (!g2_mk(&d, 4, 4)) { CHECK(0); return; }
    CHECK((DWORD)G32(EndGdiRendering)(d.bmp, 0, NULL, 0) == 0);
    g2_free(&d);
}
TEST(gap2_gdirender_end_plain_bitmap) {
    REQUIRE(G32(EndGdiRendering));
    SKIP_ON_WINE("DX interop stubs unimplemented");
    G2DC d; if (!g2_mk(&d, 4, 4)) { CHECK(0); return; }
    ULONG out = 0xdeadbeef;
    CHECK((DWORD)G32(EndGdiRendering)(d.bmp, 0, &out, 0) == 0);
    CHECK(out == 0);
    out = 0xdeadbeef;
    CHECK((DWORD)G32(EndGdiRendering)(d.bmp, 1, &out, 0) == 0);
    CHECK(out == 0);
    g2_free(&d);
}
TEST(gap2_gdirender_begin_end_sequence_no_dx) {
    REQUIRE(G32(BeginGdiRendering)); REQUIRE(G32(EndGdiRendering));
    SKIP_ON_WINE("DX interop stubs unimplemented");
    G2DC d; if (!g2_mk(&d, 4, 4)) { CHECK(0); return; }
    for (int i = 0; i < 20; i++) {
        ULONG out = 7;
        CHECK((DWORD)G32(BeginGdiRendering)(d.bmp, 0, 0) == 0);
        CHECK((DWORD)G32(EndGdiRendering)(d.bmp, 0, &out, 0) == 0);
        CHECK(out == 0);
    }
    /* the bitmap is still fully usable for GDI */
    U(PatBlt)(d.dc, 0, 0, 4, 4, BLACKNESS);
    CHECK(g2_px(&d, 1, 1) == 0);
    U(PatBlt)(d.dc, 0, 0, 4, 4, WHITENESS);
    CHECK(g2_px(&d, 1, 1) == 0xffffff);
    g2_free(&d);
}

/* ---------------- DWM ---------------- */
TEST(gap2_dwm_remoting_output_returns_one) {
    REQUIRE(G32(DwmCreatedBitmapRemotingOutput));
    SKIP_ON_WINE("DWM stubs unimplemented");
    /* a no-op in every process but the DWM */
    for (int i = 0; i < 3; i++) {
        ULONG_PTR r = G32(DwmCreatedBitmapRemotingOutput)();
        CHECKF((DWORD)r == 1, "call %d -> %#llx", i, (unsigned long long)r);
    }
}
TEST(gap2_dwm_remoting_output_both_modules) {
    DwmCreatedBitmapRemotingOutput_pfn a = G32(DwmCreatedBitmapRemotingOutput), b = G32F(DwmCreatedBitmapRemotingOutput);
    REQUIRE(a); REQUIRE(b);
    SKIP_ON_WINE("DWM stubs unimplemented");
    CHECK((DWORD)a() == 1);
    CHECK((DWORD)b() == 1);
}
TEST(gap2_dwm_remoting_output_keeps_gdi_state) {
    REQUIRE(G32(DwmCreatedBitmapRemotingOutput));
    SKIP_ON_WINE("DWM stubs unimplemented");
    DWORD before = g2_gdi_count();
    G32(DwmCreatedBitmapRemotingOutput)();
    G2_EQ(g2_gdi_count(), before);
}
TEST(gap2_dwm_dirty_rgn_not_supported) {
    REQUIRE(G32F(DwmGetDirtyRgnImpl));
    static const UINT cmds[] = { 0, 1, 2, 3, 0xff, 0xffffffff };
    for (unsigned i = 0; i < sizeof cmds / sizeof cmds[0]; i++) {
        SetLastError(0x1234);
        BOOL r = G32F(DwmGetDirtyRgnImpl)(0, NULL, NULL, cmds[i]);
        CHECKF(r == 0, "cmd %u", cmds[i]);
        EXPECT_ERR(r, ERROR_NOT_SUPPORTED);
    }
}
TEST(gap2_dwm_dirty_rgn_ignores_arguments) {
    REQUIRE(G32F(DwmGetDirtyRgnImpl));
    WCHAR a[] = L"cmid", b[] = L"file.icm";
    SetLastError(0);
    BOOL r = G32F(DwmGetDirtyRgnImpl)(0xffffffff, a, b, 1);
    CHECK(r == 0); EXPECT_ERR(r, ERROR_NOT_SUPPORTED);
    CHECK(a[0] == L'c' && b[0] == L'f');
}
TEST(gap2_dwm_dirty_rgn_is_update_icm_reg_key) {
    /* three export names share one code address */
    FARPROC d = t_proc_gdi32full("DwmGetDirtyRgnImpl"), a = t_proc_gdi32full("UpdateICMRegKeyA"), w = t_proc_gdi32full("UpdateICMRegKeyW");
    REQUIRE(d);
    if (!a || !w) { t_skip("UpdateICMRegKey exports absent"); return; }
    CHECK(a == d); CHECK(w == d);
}
TEST(gap2_dwm_update_icm_reg_key_public) {
    /* public UpdateICMRegKeyA/W go through the same stub: always FALSE / ERROR_NOT_SUPPORTED */
    REQUIRE(U(UpdateICMRegKeyA)); REQUIRE(U(UpdateICMRegKeyW));
    SKIP_ON_WINE("wine implements UpdateICMRegKey differently");
    SetLastError(0);
    BOOL r = U(UpdateICMRegKeyA)(0, NULL, NULL, ICM_DELETEPROFILE);
    CHECK(r == 0); EXPECT_ERR(r, ERROR_NOT_SUPPORTED);
    SetLastError(0);
    r = U(UpdateICMRegKeyW)(0, NULL, NULL, ICM_ADDPROFILE);
    CHECK(r == 0); EXPECT_ERR(r, ERROR_NOT_SUPPORTED);
}

/* ---------------- DirectDraw sprites / vis-rgn ---------------- */
TEST(gap2_dd_sprite_create_invalid_dc) {
    REQUIRE(G32(DdCreateFullscreenSprite));
    SKIP_ON_WINE("Dd sprite stubs unimplemented");
    for (int i = 0; i < G2_NJUNK; i++) {
        HANDLE spr = (HANDLE)PTR(0x5a5a); HDC out = (HDC)PTR(0x5a5a);
        LONG r = G32(DdCreateFullscreenSprite)((HDC)g2_junk[i], 0, &spr, &out);
        CHECKF(r == G2_INVPARAM, "dc %#llx -> %#lx", (unsigned long long)g2_junk[i], r);
        CHECK(spr == PTR(0x5a5a) && out == PTR(0x5a5a));
    }
}
TEST(gap2_dd_sprite_create_stale_dc) {
    REQUIRE(G32(DdCreateFullscreenSprite));
    SKIP_ON_WINE("Dd sprite stubs unimplemented");
    HDC d = g2_stale_dc();
    HANDLE spr = NULL; HDC out = NULL;
    CHECK(G32(DdCreateFullscreenSprite)(d, 0, &spr, &out) == G2_INVPARAM);
    CHECK(spr == NULL && out == NULL);
}
TEST(gap2_dd_sprite_destroy_invalid_dc) {
    REQUIRE(G32(DdDestroyFullscreenSprite));
    SKIP_ON_WINE("Dd sprite stubs unimplemented");
    for (int i = 0; i < G2_NJUNK; i++) {
        CHECKF(G32(DdDestroyFullscreenSprite)((HDC)g2_junk[i], NULL) == G2_INVPARAM, "dc %#llx", (unsigned long long)g2_junk[i]);
        CHECKF(G32(DdDestroyFullscreenSprite)((HDC)g2_junk[i], PTR(0x1000)) == G2_INVPARAM, "dc %#llx", (unsigned long long)g2_junk[i]);
    }
    CHECK(G32(DdDestroyFullscreenSprite)(g2_stale_dc(), NULL) == G2_INVPARAM);
}
TEST(gap2_dd_sprite_notify_invalid_dc) {
    REQUIRE(G32(DdNotifyFullscreenSpriteUpdate));
    SKIP_ON_WINE("Dd sprite stubs unimplemented");
    for (int i = 0; i < G2_NJUNK; i++) {
        CHECKF(G32(DdNotifyFullscreenSpriteUpdate)((HDC)g2_junk[i], NULL) == G2_INVPARAM, "dc %#llx", (unsigned long long)g2_junk[i]);
        CHECKF(G32(DdNotifyFullscreenSpriteUpdate)((HDC)g2_junk[i], PTR(0x1000)) == G2_INVPARAM, "dc %#llx", (unsigned long long)g2_junk[i]);
    }
    CHECK(G32(DdNotifyFullscreenSpriteUpdate)(g2_stale_dc(), NULL) == G2_INVPARAM);
}
TEST(gap2_dd_sprite_wrong_object_types) {
    REQUIRE(G32(DdCreateFullscreenSprite)); REQUIRE(G32(DdDestroyFullscreenSprite)); REQUIRE(G32(DdNotifyFullscreenSpriteUpdate));
    SKIP_ON_WINE("Dd sprite stubs unimplemented");
    HGDIOBJ o[3] = { U(CreateSolidBrush)(0), U(CreateRectRgn)(0, 0, 2, 2), U(CreateBitmap)(2, 2, 1, 1, NULL) };
    for (int i = 0; i < 3; i++) {
        HANDLE s = NULL; HDC out = NULL;
        CHECKF(G32(DdCreateFullscreenSprite)((HDC)o[i], 0, &s, &out) == G2_INVPARAM, "obj %d", i);
        CHECKF(G32(DdDestroyFullscreenSprite)((HDC)o[i], NULL) == G2_INVPARAM, "obj %d", i);
        CHECKF(G32(DdNotifyFullscreenSpriteUpdate)((HDC)o[i], NULL) == G2_INVPARAM, "obj %d", i);
        CHECKF(U(GetObjectType)(o[i]) != 0, "obj %d destroyed", i);
        U(DeleteObject)(o[i]);
    }
}
TEST(gap2_dd_vis_rgn_uniqueness_callable) {
    REQUIRE(G32(DdQueryVisRgnUniqueness));
    SKIP_ON_WINE("Dd stubs unimplemented");
    ULONG a = G32(DdQueryVisRgnUniqueness)();
    ULONG b = G32(DdQueryVisRgnUniqueness)();
    t_note("vis-rgn uniqueness %lu then %lu", a, b);
    /* the counter only ever advances (modulo wrap); it never goes back by a large step */
    CHECK((LONG)(b - a) >= 0);
}
TEST(gap2_dd_vis_rgn_uniqueness_ignores_memory_dc_drawing) {
    REQUIRE(G32(DdQueryVisRgnUniqueness));
    SKIP_ON_WINE("Dd stubs unimplemented");
    G2DC d; if (!g2_mk(&d, 16, 16)) { CHECK(0); return; }
    ULONG a = G32(DdQueryVisRgnUniqueness)();
    for (int i = 0; i < 50; i++) U(PatBlt)(d.dc, 0, 0, 16, 16, i & 1 ? BLACKNESS : WHITENESS);
    ULONG b = G32(DdQueryVisRgnUniqueness)();
    /* other windows may move concurrently, so only a monotonic relation is enforced */
    CHECK((LONG)(b - a) >= 0);
    t_note("uniqueness delta over memory-DC drawing: %lu", b - a);
    g2_free(&d);
}
