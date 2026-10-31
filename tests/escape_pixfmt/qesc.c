#include "ep.h"
/* Escape(QUERYESCSUPPORT) on stock (non-printer) DCs. Escape() with iEscape==8 answers many ids in gdi32full itself
 * without involving a driver; ids listed here return 0 on display and memory DCs. */
static void qesc_both(int id, int expect) {
    int q = id;
    HDC d = ep_scr(); HDC m = U(CreateCompatibleDC)(NULL);
    CHECK(d && m);
    if (d) { int r = U(Escape)(d, QUERYESCSUPPORT, sizeof q, (LPCSTR)&q, NULL); CHECKF(r == expect, "screen id %d got %d want %d", id, r, expect); }
    if (m) { int r = U(Escape)(m, QUERYESCSUPPORT, sizeof q, (LPCSTR)&q, NULL); CHECKF(r == expect, "mem id %d got %d want %d", id, r, expect); }
    if (m) U(DeleteDC)(m);
    if (d) ep_scr_free(d);
}
/* ids forwarded to the display driver: result is driver dependent, only record it */
static void qesc_note(int id) {
    int q = id;
    HDC d = ep_scr(); HDC m = U(CreateCompatibleDC)(NULL);
    CHECK(d && m);
    if (d) { int r = U(Escape)(d, QUERYESCSUPPORT, sizeof q, (LPCSTR)&q, NULL); t_note("screen QUERYESCSUPPORT %d -> %d", id, r); }
    if (m) { int r = U(Escape)(m, QUERYESCSUPPORT, sizeof q, (LPCSTR)&q, NULL); t_note("mem QUERYESCSUPPORT %d -> %d", id, r); }
    if (m) U(DeleteDC)(m);
    if (d) ep_scr_free(d);
}
#define QN(n) TEST(escape_qesc_driver_##n) { REQUIRE(U(Escape)); qesc_note(n); }
QN(17) QN(25) QN(37) QN(38) QN(42) QN(4096) QN(4097) QN(4098) QN(4115) QN(4116) QN(4117) QN(4118)
TEST(escape_qesc_zero_0) { REQUIRE(U(Escape)); qesc_both(0, 0); }
TEST(escape_qesc_newframe_1) { REQUIRE(U(Escape)); qesc_both(1, 1); }
TEST(escape_qesc_abortdoc_2) { REQUIRE(U(Escape)); qesc_both(2, 1); }
TEST(escape_qesc_nextband_3) { REQUIRE(U(Escape)); qesc_both(3, 1); }
TEST(escape_qesc_setcolortable_4) { REQUIRE(U(Escape)); qesc_both(4, 0); }
TEST(escape_qesc_getcolortable_5) { REQUIRE(U(Escape)); qesc_both(5, 1); }
TEST(escape_qesc_flushoutput_6) { REQUIRE(U(Escape)); qesc_both(6, 0); }
TEST(escape_qesc_draftmode_7) { REQUIRE(U(Escape)); qesc_both(7, 0); }
TEST(escape_qesc_queryesc_8) { REQUIRE(U(Escape)); qesc_both(8, 1); }
TEST(escape_qesc_setabortproc_9) { REQUIRE(U(Escape)); qesc_both(9, 1); }
TEST(escape_qesc_startdoc_10) { REQUIRE(U(Escape)); qesc_both(10, 1); }
TEST(escape_qesc_enddoc_11) { REQUIRE(U(Escape)); qesc_both(11, 1); }
TEST(escape_qesc_getphyspagesize_12) { REQUIRE(U(Escape)); qesc_both(12, 1); }
TEST(escape_qesc_getprintingoffset_13) { REQUIRE(U(Escape)); qesc_both(13, 1); }
TEST(escape_qesc_getscalingfactor_14) { REQUIRE(U(Escape)); qesc_both(14, 1); }
TEST(escape_qesc_mfcomment_15) { REQUIRE(U(Escape)); qesc_both(15, 0); }
TEST(escape_qesc_getpenwidth_16) { REQUIRE(U(Escape)); qesc_both(16, 0); }
TEST(escape_qesc_selectpapersource_18) { REQUIRE(U(Escape)); qesc_both(18, 0); }
TEST(escape_qesc_passthrough_19) { REQUIRE(U(Escape)); qesc_both(19, 1); }
TEST(escape_qesc_gettechnology_20) { REQUIRE(U(Escape)); qesc_both(20, 0); }
TEST(escape_qesc_setlinecap_21) { REQUIRE(U(Escape)); qesc_both(21, 0); }
TEST(escape_qesc_setlinejoin_22) { REQUIRE(U(Escape)); qesc_both(22, 0); }
TEST(escape_qesc_setmiterlimit_23) { REQUIRE(U(Escape)); qesc_both(23, 0); }
TEST(escape_qesc_bandinfo_24) { REQUIRE(U(Escape)); qesc_both(24, 0); }
TEST(escape_qesc_getvectorpensize_26) { REQUIRE(U(Escape)); qesc_both(26, 0); }
TEST(escape_qesc_getvectorbrushsize_27) { REQUIRE(U(Escape)); qesc_both(27, 0); }
TEST(escape_qesc_enableduplex_28) { REQUIRE(U(Escape)); qesc_both(28, 0); }
TEST(escape_qesc_getsetpaperbins_29) { REQUIRE(U(Escape)); qesc_both(29, 0); }
TEST(escape_qesc_getsetprintorient_30) { REQUIRE(U(Escape)); qesc_both(30, 0); }
TEST(escape_qesc_enumpaperbins_31) { REQUIRE(U(Escape)); qesc_both(31, 0); }
TEST(escape_qesc_setdibscaling_32) { REQUIRE(U(Escape)); qesc_both(32, 0); }
TEST(escape_qesc_epsprinting_33) { REQUIRE(U(Escape)); qesc_both(33, 0); }
TEST(escape_qesc_enumpapermetrics_34) { REQUIRE(U(Escape)); qesc_both(34, 0); }
TEST(escape_qesc_getsetpapermetrics_35) { REQUIRE(U(Escape)); qesc_both(35, 0); }
TEST(escape_qesc_id36_36) { REQUIRE(U(Escape)); qesc_both(36, 0); }
TEST(escape_qesc_id39_39) { REQUIRE(U(Escape)); qesc_both(39, 0); }
TEST(escape_qesc_id40_40) { REQUIRE(U(Escape)); qesc_both(40, 0); }
TEST(escape_qesc_id41_41) { REQUIRE(U(Escape)); qesc_both(41, 0); }
TEST(escape_qesc_id43_43) { REQUIRE(U(Escape)); qesc_both(43, 0); }
TEST(escape_qesc_id100_100) { REQUIRE(U(Escape)); qesc_both(100, 0); }
TEST(escape_qesc_id255_255) { REQUIRE(U(Escape)); qesc_both(255, 0); }
TEST(escape_qesc_getextenttable_257) { REQUIRE(U(Escape)); qesc_both(257, 0); }
TEST(escape_qesc_getpairkerntable_258) { REQUIRE(U(Escape)); qesc_both(258, 0); }
TEST(escape_qesc_gettrackkerntable_259) { REQUIRE(U(Escape)); qesc_both(259, 0); }
TEST(escape_qesc_exttextout_512) { REQUIRE(U(Escape)); qesc_both(512, 0); }
TEST(escape_qesc_getfacename_513) { REQUIRE(U(Escape)); qesc_both(513, 0); }
TEST(escape_qesc_downloadface_514) { REQUIRE(U(Escape)); qesc_note(514); }
TEST(escape_qesc_enablerelativewidths_768) { REQUIRE(U(Escape)); qesc_both(768, 0); }
TEST(escape_qesc_enablepairkerning_769) { REQUIRE(U(Escape)); qesc_both(769, 0); }
TEST(escape_qesc_setkerntrack_770) { REQUIRE(U(Escape)); qesc_both(770, 0); }
TEST(escape_qesc_setalljustvalues_771) { REQUIRE(U(Escape)); qesc_both(771, 0); }
TEST(escape_qesc_setcharset_772) { REQUIRE(U(Escape)); qesc_both(772, 0); }
TEST(escape_qesc_stretchblt_2048) { REQUIRE(U(Escape)); qesc_both(2048, 0); }
TEST(escape_qesc_id1003_4099) { REQUIRE(U(Escape)); qesc_both(4099, 0); }
TEST(escape_qesc_id1005_4101) { REQUIRE(U(Escape)); qesc_both(4101, 0); }
TEST(escape_qesc_id1008_4104) { REQUIRE(U(Escape)); qesc_both(4104, 0); }
TEST(escape_qesc_id100d_4109) { REQUIRE(U(Escape)); qesc_both(4109, 0); }
TEST(escape_qesc_beginpath_ps_inject_4110) { REQUIRE(U(Escape)); qesc_both(4110, 1); }
TEST(escape_qesc_id100f_4111) { REQUIRE(U(Escape)); qesc_both(4111, 1); }
TEST(escape_qesc_id1010_4112) { REQUIRE(U(Escape)); qesc_both(4112, 1); }
TEST(escape_qesc_id1011_4113) { REQUIRE(U(Escape)); qesc_both(4113, 0); }
TEST(escape_qesc_id1012_4114) { REQUIRE(U(Escape)); qesc_both(4114, 0); }
TEST(escape_qesc_id1017_4119) { REQUIRE(U(Escape)); qesc_both(4119, 0); }
TEST(escape_qesc_id2000_8192) { REQUIRE(U(Escape)); qesc_both(8192, 0); }
TEST(escape_qesc_id7fff_32767) { REQUIRE(U(Escape)); qesc_both(32767, 0); }
TEST(escape_qesc_idffff_65535) { REQUIRE(U(Escape)); qesc_both(65535, 0); }
TEST(escape_qesc_getextendedtextmetrics) { REQUIRE(U(Escape)); qesc_both(256, 1); }
TEST(escape_qesc_querydibsupport) { REQUIRE(U(Escape)); qesc_both(3073, 1); }
