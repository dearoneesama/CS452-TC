#include <catch_amalgamated.hpp>
#include <etl/string_view.h>
#include "../format.hpp"

TEST_CASE("sformat usage", "[format]") {
  char s[50];
  REQUIRE(troll::snformat(s, "abcde{}", 0) == 6);
  REQUIRE(etl::string_view{s} == "abcde0");
  REQUIRE(troll::snformat(s, "abc {} de {} {}{} yolo", 12, -44, 7, "hehe") == 24);
  REQUIRE(etl::string_view{s} == "abc 12 de -44 7hehe yolo");

  SECTION("no overflow") {
    char s[11];
    s[9] = 'B';
    s[10] = 'A';
    REQUIRE(troll::snformat(s, 10, "12345678901") == 9);
    REQUIRE(s[9] == '\0');
    REQUIRE(s[10] == 'A');
    REQUIRE(troll::snformat(s, 10, "abcde{}", 12345678) == 9);
    REQUIRE(etl::string_view{s} == "abcde5678"); // !!!!
    REQUIRE(s[9] == '\0');
    REQUIRE(s[10] == 'A');
    REQUIRE(troll::snformat(s, 10, "abc{}de", 12345) == 9);
    REQUIRE(etl::string_view{s} == "abc12345d");
  }
}
