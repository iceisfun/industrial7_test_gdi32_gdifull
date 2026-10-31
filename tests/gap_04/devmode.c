#include "gap4.h"
/* GdiConvertToDevmodeW: DEVMODEA -> freshly heap-allocated DEVMODEW. */

#define A_BUF 600
static void fill_a(BYTE *buf, WORD size, WORD extra) {
    for (int i = 0; i < A_BUF; i++) buf[i] = (BYTE)(i * 7 + 3);
    DEVMODEA *a = (DEVMODEA *)buf;
    memset(a->dmDeviceName, 0, 32);
    strcpy((char *)a->dmDeviceName, "GapDev4 Printer");
    a->dmSpecVersion = 0x0401;
    a->dmDriverVersion = 0x0102;
    a->dmSize = size;
    a->dmDriverExtra = extra;
    memset(a->dmFormName, 0, 32);
    if (size >= sizeof(DEVMODEA)) strcpy((char *)a->dmFormName, "FormNameGap4");
}
static void free_dm(void *p) { if (p) HeapFree(GetProcessHeap(), 0, p); }

static void check_conv(WORD size, WORD extra) {
    if (g4_wine()) { t_skip("emulator devmode conversion differs"); return; }
    BYTE buf[A_BUF];
    fill_a(buf, size, extra);
    DEVMODEA *a = (DEVMODEA *)buf;
    BYTE *w = UN(GdiConvertToDevmodeW)(a);
    if (size < 0x27) { CHECKF(w == NULL, "size %u extra %u -> %p", size, extra, w); return; }
    if (!w) { CHECKF(w != NULL, "size %u extra %u", size, extra); return; }
    DEVMODEW *d = (DEVMODEW *)w;
    CHECK(d->dmSpecVersion == 0x0401);
    CHECK(d->dmDriverVersion == 0x0102);
    CHECK(d->dmDriverExtra == extra);
    EXPECT_WSTR(d->dmDeviceName, L"GapDev4 Printer");
    for (int i = 16; i < 32; i++) CHECKF(d->dmDeviceName[i] == 0, "devname wchar %d", i);
    WORD wsize = (WORD)(size + 0x20);
    if (size > 0x65) { wsize = (WORD)(size + 0x40); if (wsize > 0xdc) wsize = 0xdc; }
    CHECKF(d->dmSize == wsize, "size %u: dmSize %u want %u", size, d->dmSize, wsize);
    /* fixed block A+0x28.. -> W+0x48.. */
    size_t n1 = 0x1e;
    if ((unsigned)size - 0x28 < 0x1e) n1 = (unsigned)size - 0x28;
    if (size < 0x28) n1 = 0x1e;
    CHECKF(!memcmp(w + 0x48, buf + 0x28, n1), "size %u fixed block", size);
    if (size > 0x65) {
        EXPECT_WSTR(d->dmFormName, size >= sizeof(DEVMODEA) ? L"FormNameGap4" : d->dmFormName);
        size_t n2 = (size_t)size - 0x66;
        if ((size_t)wsize - 0xa6 <= n2) n2 = (size_t)wsize - 0xa6;
        CHECKF(!memcmp(w + 0xa6, buf + 0x66, n2), "size %u tail block (%zu)", size, n2);
        if (extra) CHECKF(!memcmp(w + wsize, buf + size, extra), "size %u driver extra", size);
    }
    free_dm(w);
}

TEST(gap4_devmode_standard_a) {
    RQ(GdiConvertToDevmodeW);
    check_conv(sizeof(DEVMODEA), 0);
}
TEST(gap4_devmode_standard_with_extra) {
    RQ(GdiConvertToDevmodeW);
    check_conv(sizeof(DEVMODEA), 8);
    check_conv(sizeof(DEVMODEA), 1);
    check_conv(sizeof(DEVMODEA), 100);
}
TEST(gap4_devmode_sizes_too_small) {
    RQ(GdiConvertToDevmodeW);
    WORD s[] = { 0, 1, 2, 0x20, 0x24, 0x25, 0x26 };
    for (unsigned i = 0; i < sizeof s / sizeof s[0]; i++) check_conv(s[i], 4);
}
TEST(gap4_devmode_size_min_accepted) {
    RQ(GdiConvertToDevmodeW);
    check_conv(0x27, 0); check_conv(0x28, 0); check_conv(0x29, 0);
}
TEST(gap4_devmode_size_partial_range) {
    RQ(GdiConvertToDevmodeW);
    for (WORD s = 0x28; s <= 0x65; s += 3) check_conv(s, 6);
    check_conv(0x65, 4);
}
TEST(gap4_devmode_size_boundary_66) {
    RQ(GdiConvertToDevmodeW);
    check_conv(0x66, 0); check_conv(0x67, 4); check_conv(0x68, 4);
}
TEST(gap4_devmode_size_larger_than_a) {
    RQ(GdiConvertToDevmodeW);
    check_conv(160, 4); check_conv(200, 8); check_conv(0x9c + 20, 0);
}
TEST(gap4_devmode_size_capped_at_dc) {
    RQ(GdiConvertToDevmodeW);
    /* wsize = size+0x40 capped at 0xdc (sizeof DEVMODEW) */
    check_conv(0x9c, 0); check_conv(0xa0, 0); check_conv(0xb0, 0); check_conv(0x100, 0);
}
TEST(gap4_devmode_allocation_independent) {
    RQ(GdiConvertToDevmodeW);
    BYTE buf[A_BUF]; fill_a(buf, sizeof(DEVMODEA), 0);
    void *a = UN(GdiConvertToDevmodeW)((DEVMODEA *)buf);
    void *b = UN(GdiConvertToDevmodeW)((DEVMODEA *)buf);
    CHECK(a && b && a != b);
    free_dm(a); free_dm(b);
}
TEST(gap4_devmode_heap_block_freeable) {
    G4_WINE_SKIP("devmode");
    RQ(GdiConvertToDevmodeW);
    BYTE buf[A_BUF]; fill_a(buf, sizeof(DEVMODEA), 12);
    void *a = UN(GdiConvertToDevmodeW)((DEVMODEA *)buf);
    REQUIRE(a);
    SIZE_T sz = HeapSize(GetProcessHeap(), 0, a);
    CHECKF(sz == (SIZE_T)(12 + 0xdc), "heap size %zu", sz);
    CHECK(HeapValidate(GetProcessHeap(), 0, a));
    CHECK(HeapFree(GetProcessHeap(), 0, a));
}
TEST(gap4_devmode_does_not_modify_source) {
    RQ(GdiConvertToDevmodeW);
    BYTE buf[A_BUF], ref[A_BUF]; fill_a(buf, sizeof(DEVMODEA), 4); memcpy(ref, buf, A_BUF);
    free_dm(UN(GdiConvertToDevmodeW)((DEVMODEA *)buf));
    CHECK(!memcmp(buf, ref, A_BUF));
}
TEST(gap4_devmode_devicename_full32_no_nul) {
    G4_WINE_SKIP("devmode");
    RQ(GdiConvertToDevmodeW);
    BYTE buf[A_BUF]; fill_a(buf, sizeof(DEVMODEA), 0);
    DEVMODEA *a = (DEVMODEA *)buf;
    memset(a->dmDeviceName, 'Q', 32);
    DEVMODEW *d = UN(GdiConvertToDevmodeW)(a);
    REQUIRE(d);
    for (int i = 0; i < 32; i++) CHECKF(d->dmDeviceName[i] == L'Q', "wchar %d", i);
    free_dm(d);
}
TEST(gap4_devmode_devicename_empty) {
    G4_WINE_SKIP("devmode");
    RQ(GdiConvertToDevmodeW);
    BYTE buf[A_BUF]; fill_a(buf, sizeof(DEVMODEA), 0);
    DEVMODEA *a = (DEVMODEA *)buf; memset(a->dmDeviceName, 0, 32);
    DEVMODEW *d = UN(GdiConvertToDevmodeW)(a);
    REQUIRE(d);
    for (int i = 0; i < 32; i++) CHECKF(d->dmDeviceName[i] == 0, "wchar %d", i);
    free_dm(d);
}
TEST(gap4_devmode_known_fields) {
    G4_WINE_SKIP("devmode");
    RQ(GdiConvertToDevmodeW);
    DEVMODEA a; memset(&a, 0, sizeof a);
    strcpy((char *)a.dmDeviceName, "Dev"); a.dmSize = sizeof a;
    a.dmFields = DM_ORIENTATION | DM_PAPERSIZE | DM_COPIES | DM_COLOR | DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
    a.dmOrientation = DMORIENT_LANDSCAPE; a.dmPaperSize = DMPAPER_A4; a.dmCopies = 3; a.dmColor = DMCOLOR_COLOR;
    a.dmBitsPerPel = 32; a.dmPelsWidth = 1920; a.dmPelsHeight = 1080;
    strcpy((char *)a.dmFormName, "A4");
    DEVMODEW *d = UN(GdiConvertToDevmodeW)(&a);
    REQUIRE(d);
    CHECK(d->dmFields == a.dmFields);
    CHECK(d->dmOrientation == DMORIENT_LANDSCAPE && d->dmPaperSize == DMPAPER_A4 && d->dmCopies == 3);
    CHECK(d->dmColor == DMCOLOR_COLOR);
    CHECK(d->dmBitsPerPel == 32 && d->dmPelsWidth == 1920 && d->dmPelsHeight == 1080);
    EXPECT_WSTR(d->dmDeviceName, L"Dev");
    EXPECT_WSTR(d->dmFormName, L"A4");
    free_dm(d);
}
TEST(gap4_devmode_layout_assumptions) {
    CHECK(sizeof(DEVMODEA) == 156 && sizeof(DEVMODEW) == 220);
    CHECK(offsetof(DEVMODEA, dmFormName) == 0x46);
    CHECK(offsetof(DEVMODEW, dmFormName) == 0x66);
    CHECK(offsetof(DEVMODEA, dmSize) == 0x24 && offsetof(DEVMODEW, dmSize) == 0x44);
}
TEST(gap4_devmode_both_modules) {
    G4_WINE_SKIP("no gdi32full");
    CHECK(t_proc_gdi32("GdiConvertToDevmodeW") != NULL);
    CHECK(t_proc_gdi32full("GdiConvertToDevmodeW") != NULL);
}
