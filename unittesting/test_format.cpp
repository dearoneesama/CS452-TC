#include <catch_amalgamated.hpp>
#include "../generic/format.hpp"

TEST_CASE("sformat usage", "[format]") {
  REQUIRE(troll::sformat<50>("{} and {} {}", fpm::fixed_16_16{1} / 4 * 3, fpm::fixed_16_16{-13} / 3, fpm::fixed_16_16{}) == "0.75 and -4.33 0.00");
}
