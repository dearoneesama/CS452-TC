#include <catch_amalgamated.hpp>
#include <etl/string_view.h>
#include "../format.hpp"

TEST_CASE("sformat usage", "[format]") {
  char s[50];
  REQUIRE(troll::snformat(s, "abcde{}", 0) == 6);
  REQUIRE(etl::string_view{s} == "abcde0");
  REQUIRE(troll::snformat(s, "abc {} de {} {}{} yolo", 12, -44, 7, "hehe") == 24);
  REQUIRE(etl::string_view{s} == "abc 12 de -44 7hehe yolo");
  REQUIRE(troll::sformat<50>("abc {} a", 12) == "abc 12 a");

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

TEST_CASE("pad string usage", "pad") {
  SECTION("pad left sufficient space") {
    char s[11];
    troll::pad(s, "abcd", troll::padding::left, '.');
    REQUIRE(etl::string_view{s} == "abcd......");
  }

  SECTION("pad left insufficient space") {
    char s[11];
    troll::pad(s, "12345123456", troll::padding::left, '.');
    REQUIRE(etl::string_view{s} == "1234512345");
  }

  SECTION("pad middle sufficient space and even") {
    char s[11];
    troll::pad(s, "abcd", troll::padding::middle, '.');
    REQUIRE(etl::string_view{s} == "...abcd...");
  }

  SECTION("pad middle sufficient space and uneven") {
    char s[11];
    troll::pad(s, "abcde", troll::padding::middle, '-');
    REQUIRE(etl::string_view{s} == "--abcde---");
  }

  SECTION("pad middle insufficient space") {
    char s[11];
    troll::pad(s, "123451234567", troll::padding::middle, '.');
    REQUIRE(etl::string_view{s} == "1234512345");
  }

  SECTION("pad right sufficient space") {
    char s[7];
    troll::pad(s, "abcd", troll::padding::right, '.');
    REQUIRE(etl::string_view{s} == "..abcd");
  }

  SECTION("pad right insufficient space") {
    char s[7];
    troll::pad(s, "12345123456", troll::padding::right, '.');
    REQUIRE(etl::string_view{s} == "123451");
  }

  SECTION("pad right etl") {
    REQUIRE(troll::pad<10>("123456789", troll::padding::right) == " 123456789");
  }
}

TEST_CASE("tabulate 1d usage", "[tabulate1d]") {
  SECTION("truncate only element") {
    const char *titles[] = {"TooLongTitle"};
                            // ^ 7 + 5
    int data[] = {123};
    auto tab = troll::make_tabulate_1d<50, 7>(titles, titles + 1, data, 4);
    const char expected[] =
R"(+----------------------------+
|TooLong                     |
+----------------------------+
|  123                       |
+----------------------------+
)";
    etl::string<sizeof expected + 10> act;
    for (etl::string_view sv : tab) {
      act += sv.data();
      act += "\n";
    }
    REQUIRE(act == expected);
  }

  SECTION("normal usage") {
    const char *titles[] = {"tita1", "tita2", "titb3", "titc4", "titx5", "titw6", "tita7", "titu8", "titz9", "titz10"};
    int data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto tab = troll::make_tabulate_1d<50, 7>(titles, titles + 10, data, 4);
    const char expected[] =
R"(+----------------------------+
| tita1  tita2  titb3  titc4 |
+----------------------------+
|   1      2      3      4   |
+----------------------------+
| titx5  titw6  tita7  titu8 |
+----------------------------+
|   5      6      7      8   |
+----------------------------+
| titz9 titz10               |
+----------------------------+
|   9     10                 |
+----------------------------+
)";
    etl::string<sizeof expected + 10> act;
    for (etl::string_view sv : tab) {
      act += sv.data();
      act += "\n";
    }
    REQUIRE(act == expected);
  }
}

TEST_CASE("output control usage", "[OutputControl]") {
  troll::OutputControl<20, 5> oc;
  REQUIRE(oc.enqueue(0, 5, "content") == 7);
  REQUIRE(oc.enqueue(14, 4, "abc edd uyi") == 11);
  auto res = oc.dequeue();
  REQUIRE(res == "\033[1;6Hcontent\033[6;1H");
  REQUIRE(res.size() == 19);
  res = oc.dequeue();
  REQUIRE(res == "\033[15;5Habc edd uyi\033[6;1H");
  REQUIRE(res.size() == 24);
  REQUIRE(oc.empty());
  res = oc.dequeue();
  REQUIRE(res.size() == 0);
}
