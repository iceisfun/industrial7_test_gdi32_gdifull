#include "t.h"
TEST(smoke_createcompatibledc) {
    REQUIRE(U(CreateCompatibleDC));
    HDC dc = U(CreateCompatibleDC)(NULL);
    CHECK(dc != NULL);
    REQUIRE(U(DeleteDC));
    CHECK(U(DeleteDC)(dc));
    CHECK(UG(GetDeviceCaps) != NULL);
    CHECK(UF(GetDeviceCaps) == NULL || 1);
}
