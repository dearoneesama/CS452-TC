#include <catch_amalgamated.hpp>
#include <etl/string_view.h>
#include "../format.hpp"

TEST_CASE("sformat usage", "[format]") {
  char s[50];
  REQUIRE(troll::sformat(s, "abcde{}", 0) == 6);
  REQUIRE(etl::string_view{s} == "abcde0");
  // have to convert to string view if arg is string
  REQUIRE(troll::sformat(s, "abc {} de {} {}{} yolo", 12, -44, 7, etl::string_view{"hehe"}) == 24);
  REQUIRE(etl::string_view{s} == "abc 12 de -44 7hehe yolo");
}
