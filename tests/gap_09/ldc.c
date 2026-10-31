/* Internal local-DC (LDC) plumbing exported by gdi32/gdi32full:
 *  pldcGet(HDC) -> LDC*, vSetPldc(HDC, LDC*), plinkGet(HANDLE) -> LINK*, bDeleteLDC(LDC*), vFreeUFIHashTable(table, flags),
 *  hGetPEBHandle(type, param), bCreateDCW/hdcCreateDCW (CreateDCW/CreateICW back ends), fpClosePrinter (data),
 *  vDeleteLOCALFONT / UnloadUserModePrinterDriver (resolve only: they take private structures).
 * LDC layout used here (from the creator): qword 0 = owning handle, dword at +0x0C = LO_* type, list head at +0x80. */
#include "gap9.h"

typedef PVOID (WINAPI *pldcGet_fn)(HDC);
typedef void (WINAPI *vSetPldc_fn)(HDC, PVOID);
typedef PVOID (WINAPI *plinkGet_fn)(HANDLE);
typedef BOOL (WINAPI *bDeleteLDC_fn)(PVOID);
typedef void (WINAPI *vFreeUFI_fn)(PVOID *, UINT);
typedef HANDLE (WINAPI *hGetPEB_fn)(int, UINT);
typedef HDC (WINAPI *bCreateDCW_fn)(const WCHAR *, const WCHAR *, const WCHAR *, const DEVMODEW *, int, int);

static pldcGet_fn PG(void) { return (pldcGet_fn)t_proc_gdi32("pldcGet"); }
static vSetPldc_fn VS(void) { return (vSetPldc_fn)t_proc_gdi32("vSetPldc"); }
static plinkGet_fn PL(void) { return (plinkGet_fn)t_proc_gdi32full("plinkGet"); }

static HDC mk_mem_dc(void)
{
    HDC (WINAPI *c)(HDC) = (void *)t_proc("CreateCompatibleDC");
    return c ? c(NULL) : NULL;
}
static void del_dc(HDC dc) { BOOL (WINAPI *d)(HDC) = (void *)t_proc("DeleteDC"); if (d && dc) d(dc); }
static DWORD obj_type(HGDIOBJ h) { DWORD (WINAPI *g)(HGDIOBJ) = (void *)t_proc("GetObjectType"); return g ? g(h) : 0; }

/* ---- pldcGet ---- */
TEST(gap9_pldcget_null_and_bogus)
{
    pldcGet_fn f = PG(); REQUIRE(f);
    EXPECT_EQ(f(NULL), 0);
    EXPECT_EQ(f((HDC)G9_BOGUS), 0);
    EXPECT_EQ(f((HDC)(ULONG_PTR)-1), 0);
    EXPECT_EQ(f((HDC)(ULONG_PTR)0xFFFF), 0);
    EXPECT_EQ(f((HDC)(ULONG_PTR)0x10000), 0);
    EXPECT_EQ(f((HDC)(ULONG_PTR)1), 0);
    EXPECT_EQ(f((HDC)(ULONG_PTR)0x7FFFFFFF), 0);
}

TEST(gap9_pldcget_plain_memory_dc_has_no_ldc)
{
    pldcGet_fn f = PG(); REQUIRE(f);
    HDC dc = mk_mem_dc(); REQUIRE(dc);
    EXPECT_EQ(f(dc), 0);
    EXPECT_EQ(f(dc), 0);
    del_dc(dc);
}

TEST(gap9_pldcget_non_dc_objects)
{
    pldcGet_fn f = PG(); REQUIRE(f);
    HBRUSH (WINAPI *cb)(COLORREF) = (void *)t_proc("CreateSolidBrush");
    HPEN (WINAPI *cp)(int, int, COLORREF) = (void *)t_proc("CreatePen");
    HBITMAP (WINAPI *cbm)(int, int, UINT, UINT, const void *) = (void *)t_proc("CreateBitmap");
    HRGN (WINAPI *cr)(int, int, int, int) = (void *)t_proc("CreateRectRgn");
    BOOL (WINAPI *del)(HGDIOBJ) = (void *)t_proc("DeleteObject");
    REQUIRE(cb); REQUIRE(cp); REQUIRE(cbm); REQUIRE(cr); REQUIRE(del);
    HGDIOBJ o[4] = { cb(RGB(1, 2, 3)), cp(PS_SOLID, 1, 0), cbm(2, 2, 1, 1, NULL), cr(0, 0, 4, 4) };
    for (int i = 0; i < 4; i++) { CHECK(o[i] != NULL); EXPECT_EQ(f((HDC)o[i]), 0); }
    EXPECT_EQ(f((HDC)U(GetStockObject)(BLACK_PEN)), 0);
    EXPECT_EQ(f((HDC)U(GetStockObject)(SYSTEM_FONT)), 0);
    for (int i = 0; i < 4; i++) del(o[i]);
}

TEST(gap9_pldcget_deleted_dc)
{
    pldcGet_fn f = PG(); REQUIRE(f);
    HDC dc = mk_mem_dc(); REQUIRE(dc);
    del_dc(dc);
    EXPECT_EQ(f(dc), 0);
}

TEST(gap9_pldcget_metafile_dc_has_ldc_owned_by_handle)
{
    pldcGet_fn f = PG(); REQUIRE(f);
    HDC (WINAPI *cm)(HDC, LPCWSTR, const RECT *, LPCWSTR) = (void *)t_proc("CreateEnhMetaFileW");
    HENHMETAFILE (WINAPI *ce)(HDC) = (void *)t_proc("CloseEnhMetaFile");
    BOOL (WINAPI *de)(HENHMETAFILE) = (void *)t_proc("DeleteEnhMetaFile");
    REQUIRE(cm); REQUIRE(ce); REQUIRE(de);
    HDC m = cm(NULL, NULL, NULL, NULL);
    REQUIRE(m);
    EXPECT_EQ(obj_type(m), OBJ_ENHMETADC);
    void *ldc = f(m);
    CHECK(ldc != NULL);
    if (ldc) {
        EXPECT_EQ(*(HDC *)ldc, m);
        t_note("metafile DC LDC type dword = 0x%lx", (unsigned long)*(DWORD *)((BYTE *)ldc + 0x0C));
    }
    EXPECT_EQ(f(m), ldc);   /* stable */
    HENHMETAFILE h = ce(m);
    CHECK(h != NULL);
    if (h) de(h);
    EXPECT_EQ(f(m), 0);     /* handle is gone */
}

TEST(gap9_pldcget_two_metafile_dcs_distinct_ldcs)
{
    pldcGet_fn f = PG(); REQUIRE(f);
    HDC (WINAPI *cm)(HDC, LPCWSTR, const RECT *, LPCWSTR) = (void *)t_proc("CreateEnhMetaFileW");
    HENHMETAFILE (WINAPI *ce)(HDC) = (void *)t_proc("CloseEnhMetaFile");
    BOOL (WINAPI *de)(HENHMETAFILE) = (void *)t_proc("DeleteEnhMetaFile");
    REQUIRE(cm); REQUIRE(ce); REQUIRE(de);
    HDC a = cm(NULL, NULL, NULL, NULL), b = cm(NULL, NULL, NULL, NULL);
    REQUIRE(a); REQUIRE(b);
    void *la = f(a), *lb = f(b);
    CHECK(la && lb && la != lb);
    if (la && lb) { EXPECT_EQ(*(HDC *)la, a); EXPECT_EQ(*(HDC *)lb, b); }
    HENHMETAFILE ha = ce(a), hb = ce(b);
    if (ha) de(ha);
    if (hb) de(hb);
}

TEST(gap9_pldcget_metafile_dc_does_not_leak_objects)
{
    pldcGet_fn f = PG(); REQUIRE(f);
    HDC (WINAPI *cm)(HDC, LPCWSTR, const RECT *, LPCWSTR) = (void *)t_proc("CreateEnhMetaFileW");
    HENHMETAFILE (WINAPI *ce)(HDC) = (void *)t_proc("CloseEnhMetaFile");
    BOOL (WINAPI *de)(HENHMETAFILE) = (void *)t_proc("DeleteEnhMetaFile");
    REQUIRE(cm); REQUIRE(ce); REQUIRE(de);
    DWORD before = g9_gdi_count();
    for (int i = 0; i < 50; i++) {
        HDC m = cm(NULL, NULL, NULL, NULL);
        if (!m) break;
        f(m);
        HENHMETAFILE h = ce(m);
        if (h) de(h);
    }
    EXPECT_EQ(g9_gdi_count(), before);
}

TEST(gap9_pldcget_display_dc_without_printer_has_no_ldc)
{
    pldcGet_fn f = PG(); REQUIRE(f);
    HDC (WINAPI *cdc)(LPCWSTR, LPCWSTR, LPCWSTR, const DEVMODEW *) = (void *)t_proc("CreateDCW");
    REQUIRE(cdc);
    HDC dc = cdc(L"DISPLAY", NULL, NULL, NULL);
    if (!dc) { t_skip("no display DC"); return; }
    void *ldc = f(dc);
    t_note("display DC ldc = %p", ldc);
    CHECK(ldc == NULL);
    del_dc(dc);
}

/* ---- vSetPldc ---- */
TEST(gap9_vsetpldc_roundtrip_on_memory_dc)
{
    pldcGet_fn g = PG(); vSetPldc_fn s = VS(); REQUIRE(g); REQUIRE(s);
    HDC dc = mk_mem_dc(); REQUIRE(dc);
    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 0x130);
    REQUIRE(buf);
    EXPECT_EQ(g(dc), 0);
    s(dc, buf);
    EXPECT_EQ(g(dc), buf);
    EXPECT_EQ(*(HDC *)buf, dc);            /* back pointer written */
    s(dc, NULL);                           /* detach before the DC goes away */
    EXPECT_EQ(g(dc), 0);
    EXPECT_EQ(*(HDC *)buf, dc);            /* NULL ldc: nothing written */
    del_dc(dc);
    HeapFree(GetProcessHeap(), 0, buf);
}

TEST(gap9_vsetpldc_replace_pointer)
{
    pldcGet_fn g = PG(); vSetPldc_fn s = VS(); REQUIRE(g); REQUIRE(s);
    HDC dc = mk_mem_dc(); REQUIRE(dc);
    BYTE *a = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 0x130);
    BYTE *b = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 0x130);
    REQUIRE(a); REQUIRE(b);
    s(dc, a); EXPECT_EQ(g(dc), a);
    s(dc, b); EXPECT_EQ(g(dc), b);
    EXPECT_EQ(*(HDC *)a, dc); EXPECT_EQ(*(HDC *)b, dc);
    s(dc, NULL);
    EXPECT_EQ(g(dc), 0);
    del_dc(dc);
    HeapFree(GetProcessHeap(), 0, a); HeapFree(GetProcessHeap(), 0, b);
}

TEST(gap9_vsetpldc_bogus_handle_only_writes_backpointer)
{
    pldcGet_fn g = PG(); vSetPldc_fn s = VS(); REQUIRE(g); REQUIRE(s);
    BYTE buf[0x130]; memset(buf, 0, sizeof buf);
    s((HDC)G9_BOGUS, buf);
    EXPECT_EQ(*(ULONG_PTR *)buf, G9_BOGUS);
    EXPECT_EQ(g((HDC)G9_BOGUS), 0);
    memset(buf, 0, sizeof buf);
    s(NULL, buf);
    EXPECT_EQ(*(ULONG_PTR *)buf, 0);
    s((HDC)G9_BOGUS, NULL);   /* nothing to write, nothing to fault on */
    CHECK(1);
}

TEST(gap9_vsetpldc_non_dc_object_unaffected)
{
    pldcGet_fn g = PG(); vSetPldc_fn s = VS(); REQUIRE(g); REQUIRE(s);
    HBRUSH (WINAPI *cb)(COLORREF) = (void *)t_proc("CreateSolidBrush");
    BOOL (WINAPI *del)(HGDIOBJ) = (void *)t_proc("DeleteObject");
    REQUIRE(cb); REQUIRE(del);
    HBRUSH br = cb(RGB(9, 9, 9));
    REQUIRE(br);
    BYTE buf[0x130]; memset(buf, 0, sizeof buf);
    s((HDC)br, buf);
    EXPECT_EQ(*(ULONG_PTR *)buf, (ULONG_PTR)br);
    EXPECT_EQ(g((HDC)br), 0);
    CHECK(del(br));
}

TEST(gap9_vsetpldc_deleted_dc_stays_unattached)
{
    pldcGet_fn g = PG(); vSetPldc_fn s = VS(); REQUIRE(g); REQUIRE(s);
    HDC dc = mk_mem_dc(); REQUIRE(dc);
    del_dc(dc);
    BYTE buf[0x130]; memset(buf, 0, sizeof buf);
    s(dc, buf);
    EXPECT_EQ(g(dc), 0);
}

TEST(gap9_vsetpldc_does_not_change_dc_state)
{
    pldcGet_fn g = PG(); vSetPldc_fn s = VS(); REQUIRE(g); REQUIRE(s);
    int (WINAPI *sm)(HDC, int) = (void *)t_proc("SetMapMode");
    int (WINAPI *gm)(HDC) = (void *)t_proc("GetMapMode");
    REQUIRE(sm); REQUIRE(gm);
    HDC dc = mk_mem_dc(); REQUIRE(dc);
    sm(dc, MM_LOENGLISH);
    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 0x130);
    REQUIRE(buf);
    s(dc, buf);
    EXPECT_EQ(g(dc), buf);
    s(dc, NULL);
    EXPECT_EQ(obj_type(dc), OBJ_MEMDC);
    EXPECT_EQ(gm(dc), MM_LOENGLISH);
    del_dc(dc);
    HeapFree(GetProcessHeap(), 0, buf);
}

/* ---- plinkGet ---- */
TEST(gap9_plinkget_null_and_unknown)
{
    plinkGet_fn f = PL(); REQUIRE(f);
    EXPECT_EQ(f(NULL), 0);
    EXPECT_EQ(f((HANDLE)G9_BOGUS), 0);
    EXPECT_EQ(f((HANDLE)(ULONG_PTR)-1), 0);
    EXPECT_EQ(f((HANDLE)(ULONG_PTR)1), 0);
    EXPECT_EQ(f((HANDLE)(ULONG_PTR)0x80), 0);
    EXPECT_EQ(f((HANDLE)(ULONG_PTR)0x7F), 0);
}

TEST(gap9_plinkget_plain_dc_has_no_link)
{
    plinkGet_fn f = PL(); REQUIRE(f);
    HDC dc = mk_mem_dc(); REQUIRE(dc);
    EXPECT_EQ(f(dc), 0);
    del_dc(dc);
}

TEST(gap9_plinkget_same_hash_bucket_no_match)
{
    /* the table is hashed on the low 7 bits: probe all 128 buckets with values that cannot be registered */
    plinkGet_fn f = PL(); REQUIRE(f);
    for (ULONG_PTR i = 0; i < 128; i++) {
        PVOID r = f((HANDLE)(0x7E000000 + i));
        if (r) { CHECKF(0, "bucket %lu non-empty for bogus key", (unsigned long)i); break; }
    }
    CHECK(1);
}

TEST(gap9_plinkget_metafile_dc_link_is_stable)
{
    plinkGet_fn f = PL(); REQUIRE(f);
    HDC (WINAPI *cm)(HDC, LPCWSTR, const RECT *, LPCWSTR) = (void *)t_proc("CreateEnhMetaFileW");
    HENHMETAFILE (WINAPI *ce)(HDC) = (void *)t_proc("CloseEnhMetaFile");
    BOOL (WINAPI *de)(HENHMETAFILE) = (void *)t_proc("DeleteEnhMetaFile");
    REQUIRE(cm); REQUIRE(ce); REQUIRE(de);
    HDC m = cm(NULL, NULL, NULL, NULL);
    REQUIRE(m);
    PVOID a = f(m), b = f(m);
    EXPECT_EQ(a, b);
    t_note("plinkGet(metafile DC) = %p", a);
    HENHMETAFILE h = ce(m);
    if (h) {
        PVOID c = f(h);
        t_note("plinkGet(closed hemf) = %p", c);
        de(h);
        EXPECT_EQ(f(h), 0);
    }
}

/* ---- bDeleteLDC ---- */
static BYTE *new_ldc(void)
{
    BYTE *p = (BYTE *)LocalAlloc(LPTR, 0x130);
    if (p) { *(BYTE **)(p + 0x80) = p + 0x80; *(BYTE **)(p + 0x88) = p + 0x80; }
    return p;
}

TEST(gap9_bdeleteldc_empty)
{
    bDeleteLDC_fn f = (bDeleteLDC_fn)t_proc_gdi32("bDeleteLDC"); REQUIRE(f);
    BYTE *p = new_ldc(); REQUIRE(p);
    EXPECT_EQ(f(p), 1);
    CHECK(HeapValidate(GetProcessHeap(), 0, NULL));
}

TEST(gap9_bdeleteldc_with_owned_heap_block)
{
    bDeleteLDC_fn f = (bDeleteLDC_fn)t_proc_gdi32("bDeleteLDC"); REQUIRE(f);
    BYTE *p = new_ldc(); REQUIRE(p);
    void *blk = HeapAlloc(GetProcessHeap(), 0, 256);
    REQUIRE(blk);
    memset(blk, 0xAB, 256);
    *(void **)(p + 0x60) = blk;
    EXPECT_EQ(f(p), 1);
    CHECK(HeapValidate(GetProcessHeap(), 0, NULL));
}

TEST(gap9_bdeleteldc_with_list_nodes)
{
    bDeleteLDC_fn f = (bDeleteLDC_fn)t_proc_gdi32("bDeleteLDC"); REQUIRE(f);
    for (int count = 1; count <= 6; count++) {
        BYTE *p = new_ldc(); REQUIRE(p);
        BYTE *head = p + 0x80;
        BYTE *prev = head;
        for (int i = 0; i < count; i++) {
            BYTE *n = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 48);
            REQUIRE(n);
            *(BYTE **)prev = n;          /* prev->Flink = n */
            *(BYTE **)(n + 8) = prev;    /* n->Blink = prev */
            *(BYTE **)n = head;          /* n->Flink = head */
            prev = n;
        }
        *(BYTE **)(head + 8) = prev;
        *(DWORD *)(p + 0x78) = count;
        EXPECT_EQ(f(p), 1);
        CHECK(HeapValidate(GetProcessHeap(), 0, NULL));
    }
}

TEST(gap9_bdeleteldc_heap_block_and_list_together)
{
    bDeleteLDC_fn f = (bDeleteLDC_fn)t_proc_gdi32("bDeleteLDC"); REQUIRE(f);
    BYTE *p = new_ldc(); REQUIRE(p);
    BYTE *head = p + 0x80;
    BYTE *n = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 64);
    REQUIRE(n);
    *(BYTE **)head = n; *(BYTE **)(n + 8) = head; *(BYTE **)n = head; *(BYTE **)(head + 8) = n;
    *(DWORD *)(p + 0x78) = 1;
    *(void **)(p + 0x60) = HeapAlloc(GetProcessHeap(), 0, 64);
    EXPECT_EQ(f(p), 1);
    CHECK(HeapValidate(GetProcessHeap(), 0, NULL));
}

TEST(gap9_bdeleteldc_repeated_create_destroy)
{
    bDeleteLDC_fn f = (bDeleteLDC_fn)t_proc_gdi32("bDeleteLDC"); REQUIRE(f);
    for (int i = 0; i < 500; i++) {
        BYTE *p = new_ldc();
        if (!p) break;
        if (f(p) != 1) { CHECKF(0, "iteration %d", i); break; }
    }
    CHECK(HeapValidate(GetProcessHeap(), 0, NULL));
}

/* ---- vFreeUFIHashTable ---- */
static void ufi_build(PVOID *tbl, int bucket, int nodes, int flagbit, void ***payloads, int *np, int payload_off)
{
    for (int i = 0; i < nodes; i++) {
        BYTE *n = (BYTE *)LocalAlloc(LPTR, 0x40);
        *(BYTE **)(n + 8) = (BYTE *)tbl[bucket];
        n[0x10] = (BYTE)flagbit;
        void *pl = LocalAlloc(LPTR, 24);
        *(void **)(n + payload_off) = pl;
        tbl[bucket] = n;
        payloads[(*np)++] = (void **)pl;
    }
}

TEST(gap9_vfreeufi_null_table)
{
    vFreeUFI_fn f = (vFreeUFI_fn)t_proc_gdi32full("vFreeUFIHashTable"); REQUIRE(f);
    f(NULL, 0);
    f(NULL, 1);
    f(NULL, 0xFFFFFFFF);
    CHECK(1);
}

TEST(gap9_vfreeufi_empty_table)
{
    vFreeUFI_fn f = (vFreeUFI_fn)t_proc_gdi32full("vFreeUFIHashTable"); REQUIRE(f);
    PVOID tbl[32]; memset(tbl, 0, sizeof tbl);
    f(tbl, 0);
    f(tbl, 1);
    for (int i = 0; i < 32; i++) EXPECT_EQ(tbl[i], 0);
}

TEST(gap9_vfreeufi_flags0_keeps_payloads)
{
    vFreeUFI_fn f = (vFreeUFI_fn)t_proc_gdi32full("vFreeUFIHashTable"); REQUIRE(f);
    PVOID tbl[32]; memset(tbl, 0, sizeof tbl);
    void **pl[16]; int np = 0;
    ufi_build(tbl, 0, 3, 0, (void ***)pl, &np, 0x30);
    ufi_build(tbl, 17, 2, 1, (void ***)pl, &np, 0x18);
    ufi_build(tbl, 31, 1, 0, (void ***)pl, &np, 0x30);
    f(tbl, 0);
    /* payloads are ours to free: they must still be valid heap blocks */
    CHECK(HeapValidate(GetProcessHeap(), 0, NULL));
    for (int i = 0; i < np; i++) { CHECK(HeapValidate(GetProcessHeap(), 0, pl[i])); LocalFree(pl[i]); }
    CHECK(HeapValidate(GetProcessHeap(), 0, NULL));
}

TEST(gap9_vfreeufi_flags1_frees_payloads)
{
    vFreeUFI_fn f = (vFreeUFI_fn)t_proc_gdi32full("vFreeUFIHashTable"); REQUIRE(f);
    PVOID tbl[32]; memset(tbl, 0, sizeof tbl);
    void **pl[16]; int np = 0;
    ufi_build(tbl, 3, 4, 0, (void ***)pl, &np, 0x30);   /* bit0 clear -> payload at +0x30 */
    ufi_build(tbl, 9, 4, 1, (void ***)pl, &np, 0x18);   /* bit0 set   -> payload at +0x18 */
    f(tbl, 1);
    CHECK(HeapValidate(GetProcessHeap(), 0, NULL));
}

TEST(gap9_vfreeufi_null_payload_pointers_allowed)
{
    vFreeUFI_fn f = (vFreeUFI_fn)t_proc_gdi32full("vFreeUFIHashTable"); REQUIRE(f);
    PVOID tbl[32]; memset(tbl, 0, sizeof tbl);
    for (int i = 0; i < 5; i++) {
        BYTE *n = (BYTE *)LocalAlloc(LPTR, 0x40);
        *(BYTE **)(n + 8) = (BYTE *)tbl[5];
        n[0x10] = (BYTE)(i & 1);
        tbl[5] = n;
    }
    f(tbl, 1);
    CHECK(HeapValidate(GetProcessHeap(), 0, NULL));
}

TEST(gap9_vfreeufi_all_buckets_long_chains)
{
    vFreeUFI_fn f = (vFreeUFI_fn)t_proc_gdi32full("vFreeUFIHashTable"); REQUIRE(f);
    PVOID tbl[32]; memset(tbl, 0, sizeof tbl);
    for (int b = 0; b < 32; b++)
        for (int i = 0; i < 20; i++) {
            BYTE *n = (BYTE *)LocalAlloc(LPTR, 0x40);
            *(BYTE **)(n + 8) = (BYTE *)tbl[b];
            tbl[b] = n;
        }
    f(tbl, 0);
    CHECK(HeapValidate(GetProcessHeap(), 0, NULL));
}

TEST(gap9_vfreeufi_flags_only_bit0_matters)
{
    vFreeUFI_fn f = (vFreeUFI_fn)t_proc_gdi32full("vFreeUFIHashTable"); REQUIRE(f);
    for (UINT fl = 0; fl < 8; fl++) {
        PVOID tbl[32]; memset(tbl, 0, sizeof tbl);
        void **pl[8]; int np = 0;
        ufi_build(tbl, 1, 2, 0, (void ***)pl, &np, 0x30);
        f(tbl, fl * 2);   /* bit0 clear in every case: payloads survive */
        for (int i = 0; i < np; i++) LocalFree(pl[i]);
        CHECK(HeapValidate(GetProcessHeap(), 0, NULL));
    }
}

/* ---- hGetPEBHandle ---- */
TEST(gap9_hgetpebhandle_returns_zero_or_live_object)
{
    hGetPEB_fn f = (hGetPEB_fn)t_proc_gdi32("hGetPEBHandle"); REQUIRE(f);
    BOOL (WINAPI *del)(HGDIOBJ) = (void *)t_proc("DeleteObject");
    REQUIRE(del);
    DWORD before = g9_gdi_count();
    for (int type = 0; type <= 2; type++) {
        HANDLE h = f(type, 0x00112233);
        if (!h) { t_note("hGetPEBHandle(%d) = NULL (cache empty)", type); continue; }
        DWORD ot = obj_type(h);
        t_note("hGetPEBHandle(%d) -> %p objtype %lu", type, h, (unsigned long)ot);
        CHECKF(ot == OBJ_BRUSH || ot == OBJ_PEN || ot == OBJ_REGION || ot == OBJ_EXTPEN, "type %d object type %lu", type, (unsigned long)ot);
        CHECK(del(h));
    }
    EXPECT_EQ(g9_gdi_count(), before);
}

TEST(gap9_hgetpebhandle_after_brush_churn)
{
    hGetPEB_fn f = (hGetPEB_fn)t_proc_gdi32("hGetPEBHandle"); REQUIRE(f);
    HBRUSH (WINAPI *cb)(COLORREF) = (void *)t_proc("CreateSolidBrush");
    BOOL (WINAPI *del)(HGDIOBJ) = (void *)t_proc("DeleteObject");
    REQUIRE(cb); REQUIRE(del);
    DWORD before = g9_gdi_count();
    for (int i = 0; i < 40; i++) { HBRUSH b = cb(RGB(i, i, i)); if (b) del(b); }
    HANDLE h = f(0, RGB(1, 2, 3));
    if (h) {
        EXPECT_EQ(obj_type(h), OBJ_BRUSH);
        CHECK(del(h));
    } else t_note("brush cache empty after churn");
    EXPECT_EQ(g9_gdi_count(), before);
}

TEST(gap9_hgetpebhandle_drains_to_empty)
{
    hGetPEB_fn f = (hGetPEB_fn)t_proc_gdi32("hGetPEBHandle"); REQUIRE(f);
    BOOL (WINAPI *del)(HGDIOBJ) = (void *)t_proc("DeleteObject");
    REQUIRE(del);
    HANDLE got[64]; int n = 0;
    for (int i = 0; i < 64; i++) { HANDLE h = f(0, 0); if (!h) break; got[n++] = h; }
    CHECK(n < 64);   /* the per-process cache is finite */
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) if (got[i] == got[j]) { CHECKF(0, "duplicate handle %p", got[i]); }
        del(got[i]);
    }
}

/* ---- bCreateDCW / hdcCreateDCW ---- */
static bCreateDCW_fn BC(void) { return (bCreateDCW_fn)t_proc_gdi32("bCreateDCW"); }

TEST(gap9_bcreatedcw_display_driver_name)
{
    bCreateDCW_fn f = BC(); REQUIRE(f);
    HDC dc = f(L"DISPLAY", NULL, NULL, NULL, 0, 0);
    if (!dc) { t_skip("no display DC"); return; }
    EXPECT_EQ(obj_type(dc), OBJ_DC);
    int (WINAPI *gdc)(HDC, int) = (void *)t_proc("GetDeviceCaps");
    REQUIRE(gdc);
    EXPECT_EQ(gdc(dc, TECHNOLOGY), DT_RASDISPLAY);
    HDC ref = U(CreateDCW)(L"DISPLAY", NULL, NULL, NULL);
    if (ref) {
        EXPECT_EQ(gdc(dc, HORZRES), gdc(ref, HORZRES));
        EXPECT_EQ(gdc(dc, VERTRES), gdc(ref, VERTRES));
        EXPECT_EQ(gdc(dc, BITSPIXEL), gdc(ref, BITSPIXEL));
        del_dc(ref);
    }
    del_dc(dc);
}

TEST(gap9_bcreatedcw_display_case_insensitive)
{
    bCreateDCW_fn f = BC(); REQUIRE(f);
    static const WCHAR *const names[] = { L"display", L"Display", L"DiSpLaY" };
    for (int i = 0; i < 3; i++) {
        HDC dc = f(names[i], NULL, NULL, NULL, 0, 0);
        if (!dc) { CHECKF(0, "name %d", i); continue; }
        EXPECT_EQ(obj_type(dc), OBJ_DC);
        del_dc(dc);
    }
}

TEST(gap9_bcreatedcw_information_context_flag)
{
    bCreateDCW_fn f = BC(); REQUIRE(f);
    HDC dc = f(L"DISPLAY", NULL, NULL, NULL, 1, 0);
    if (!dc) { t_skip("no display IC"); return; }
    EXPECT_EQ(obj_type(dc), OBJ_DC);
    int (WINAPI *gdc)(HDC, int) = (void *)t_proc("GetDeviceCaps");
    EXPECT_EQ(gdc(dc, TECHNOLOGY), DT_RASDISPLAY);
    del_dc(dc);
}

TEST(gap9_bcreatedcw_device_name_display_path)
{
    bCreateDCW_fn f = BC(); REQUIRE(f);
    /* a \\.\DISPLAYn device name in either slot selects the display path */
    HDC a = f(L"\\\\.\\DISPLAY1", NULL, NULL, NULL, 0, 0);
    HDC b = f(NULL, L"\\\\.\\DISPLAY1", NULL, NULL, 0, 0);
    HDC c = f(L"DISPLAY", L"\\\\.\\DISPLAY1", NULL, NULL, 0, 0);
    t_note("DISPLAY1 via driver=%p device=%p both=%p", a, b, c);
    CHECK((a != NULL) == (b != NULL));
    CHECK((b != NULL) == (c != NULL));
    if (a) EXPECT_EQ(obj_type(a), OBJ_DC);
    if (b) EXPECT_EQ(obj_type(b), OBJ_DC);
    if (c) EXPECT_EQ(obj_type(c), OBJ_DC);
    del_dc(a); del_dc(b); del_dc(c);
}

TEST(gap9_bcreatedcw_all_null_fails)
{
    bCreateDCW_fn f = BC(); REQUIRE(f);
    HDC dc = f(NULL, NULL, NULL, NULL, 0, 0);
    CHECK(dc == NULL);
    if (dc) del_dc(dc);
    HDC dc2 = f(NULL, NULL, NULL, NULL, 1, 0);
    CHECK(dc2 == NULL);
    if (dc2) del_dc(dc2);
}

TEST(gap9_bcreatedcw_matches_createdcw_for_failures)
{
    bCreateDCW_fn f = BC(); REQUIRE(f);
    static const WCHAR *const drv[] = { L"gap9_no_such_driver", L"\\\\.\\NOTADISPLAY", L"WINSPOOL_gap9", L"" };
    for (int i = 0; i < 4; i++) {
        HDC a = f(drv[i], NULL, NULL, NULL, 0, 0);
        HDC b = U(CreateDCW)(drv[i], NULL, NULL, NULL);
        CHECKF((a != NULL) == (b != NULL), "driver %d: bCreateDCW %p CreateDCW %p", i, a, b);
        del_dc(a); del_dc(b);
    }
}

TEST(gap9_bcreatedcw_bogus_printer_device)
{
    bCreateDCW_fn f = BC(); REQUIRE(f);
    SET_ERR(0);
    HDC dc = f(L"WINSPOOL", L"gap9 no such printer", NULL, NULL, 0, 0);
    DWORD e = GetLastError();
    CHECK(dc == NULL);
    t_note("nonexistent printer: dc=%p err=%lu", dc, (unsigned long)e);
    if (dc) del_dc(dc);
}

TEST(gap9_bcreatedcw_repeated_display_no_leak)
{
    bCreateDCW_fn f = BC(); REQUIRE(f);
    DWORD before = g9_gdi_count();
    for (int i = 0; i < 100; i++) {
        HDC dc = f(L"DISPLAY", NULL, NULL, NULL, i & 1, 0);
        if (!dc) { t_skip("no display"); return; }
        del_dc(dc);
    }
    EXPECT_EQ(g9_gdi_count(), before);
}

TEST(gap9_bcreatedcw_ic_and_dc_have_distinct_handles)
{
    bCreateDCW_fn f = BC(); REQUIRE(f);
    HDC a = f(L"DISPLAY", NULL, NULL, NULL, 0, 0), b = f(L"DISPLAY", NULL, NULL, NULL, 1, 0);
    if (!a || !b) { del_dc(a); del_dc(b); t_skip("no display"); return; }
    CHECK(a != b);
    del_dc(a); del_dc(b);
}

TEST(gap9_hdccreatedcw_display_flag)
{
    FARPROC f = t_proc_gdi32full("hdcCreateDCW");
    REQUIRE(f);
    if (g9_wine()) { t_skip("wine: hdcCreateDCW"); return; }
    /* (device, output, devmode, isDisplay, isIC): NULL device with the display flag = primary display */
    HDC dc = (HDC)g9_call8(f, 0, 0, 0, 1, 0, 0, 0, 0);
    if (!dc) { t_skip("no display DC"); return; }
    EXPECT_EQ(obj_type(dc), OBJ_DC);
    int (WINAPI *gdc)(HDC, int) = (void *)t_proc("GetDeviceCaps");
    EXPECT_EQ(gdc(dc, TECHNOLOGY), DT_RASDISPLAY);
    del_dc(dc);
}

TEST(gap9_hdccreatedcw_exported_from_gdi32full_only)
{
    G9_NOWINE("export set differs");
    CHECK(t_proc_gdi32full("hdcCreateDCW") != NULL);
    CHECK(t_proc_gdi32("hdcCreateDCW") == NULL);
    CHECK(t_proc_gdi32("bCreateDCW") != NULL);
    CHECK(t_proc_gdi32full("bCreateDCW") == NULL);
}

/* ---- fpClosePrinter (data export) ---- */
TEST(gap9_fpcloseprinter_is_encoded_pointer_after_spooler_load)
{
    G9_NOWINE("spooler loading");
    ULONGLONG *p = (ULONGLONG *)t_proc_gdi32("fpClosePrinter");
    REQUIRE(p);
    PVOID (NTAPI *dec)(PVOID) = (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlDecodePointer");
    REQUIRE(dec);
    /* the print-driver path of CreateDCW loads winspool.drv and fills the table */
    HDC dc = U(CreateDCW)(L"WINSPOOL", L"gap9 no such printer", NULL, NULL);
    if (dc) del_dc(dc);
    HMODULE ws = GetModuleHandleA("winspool.drv");
    if (!ws) { t_note("winspool.drv not loaded; fpClosePrinter=0x%llx", *p); return; }
    FARPROC want = GetProcAddress(ws, "ClosePrinter");
    CHECK(want != NULL);
    CHECK(*p != 0);
    CHECK(*p != (ULONGLONG)(ULONG_PTR)want);   /* stored obfuscated */
    EXPECT_EQ(dec((PVOID)(ULONG_PTR)*p), want);
}

TEST(gap9_fpcloseprinter_data_is_stable_between_reads)
{
    ULONGLONG *p = (ULONGLONG *)t_proc_gdi32("fpClosePrinter");
    REQUIRE(p);
    ULONGLONG a = *p, b = *p;
    EXPECT_EQ(a, b);
    CHECK(g9_in_module(p, t_gdi32()));
}

/* ---- remaining private exports: resolve only ---- */
TEST(gap9_vdeletelocalfont_resolves)
{
    G9_NOWINE("export set differs");
    /* takes a LOCALFONT* and pushes it onto a global free list: never called with a fake block */
    CHECK(t_proc_gdi32full("vDeleteLOCALFONT") != NULL);
    CHECK(t_proc_gdi32("vDeleteLOCALFONT") == NULL);
}

TEST(gap9_unloadumpd_resolves)
{
    G9_NOWINE("export set differs");
    CHECK(t_proc_gdi32full("UnloadUserModePrinterDriver") != NULL);
    CHECK(t_proc_gdi32("UnloadUserModePrinterDriver") == NULL);
}

TEST(gap9_ldc_helper_exports_present_in_expected_modules)
{
    G9_NOWINE("export set differs");
    CHECK(t_proc_gdi32("pldcGet") != NULL);
    CHECK(t_proc_gdi32("vSetPldc") != NULL);
    CHECK(t_proc_gdi32("bDeleteLDC") != NULL);
    CHECK(t_proc_gdi32("hGetPEBHandle") != NULL);
    CHECK(t_proc_gdi32full("plinkGet") != NULL);
    CHECK(t_proc_gdi32full("vFreeUFIHashTable") != NULL);
    CHECK(t_proc_gdi32full("pldcGet") == NULL);
    CHECK(t_proc_gdi32full("vSetPldc") == NULL);
    CHECK(t_proc_gdi32full("hGetPEBHandle") == NULL);
    CHECK(t_proc_gdi32full("bDeleteLDC") == NULL);
}
