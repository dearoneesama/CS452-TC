#include <catch_amalgamated.hpp>
#include <etl/string_view.h>
#include "../format.hpp"
#include "../utils.hpp"

TEST_CASE("sformat usage", "[format]") {
  char s[50];
  REQUIRE(troll::snformat(s, "abcde{}", 0) == 6);
  REQUIRE(etl::string_view{s} == "abcde0");
  REQUIRE(troll::snformat(s, "abc {} de {} {}{} yolo", 12, -44, 7, "hehe") == 24);
  REQUIRE(etl::string_view{s} == "abc 12 de -44 7hehe yolo");
  REQUIRE(troll::sformat<50>("abc {} a {} ", 12, 'b') == "abc 12 a b ");
  REQUIRE(troll::sformat<50>("{} and {} {}", fpm::fixed_16_16{1} / 4 * 3, fpm::fixed_16_16{-13} / 3, fpm::fixed_16_16{}) == "0.75 and -4.33 0.00");

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

TEST_CASE("static_ansi_style_options usage", "[static_ansi_style_options]") {
  SECTION("usual usage") {
    using styles = troll::static_ansi_style_options<
      troll::ansi_font::bold | troll::ansi_font::italic | troll::ansi_font::underline,
      troll::ansi_color::red
    >;
    REQUIRE(styles::enabler_str_size == 11);
    REQUIRE(styles::enabler_str() == "\033[1;3;4;31m");
    REQUIRE(styles::enabler_str() == "\033[1;3;4;31m");
    REQUIRE(styles::disabler_str_size == 4);
    REQUIRE(styles::disabler_str() == "\033[0m");
  }

  SECTION("only one bg style") {
    using styles = troll::static_ansi_style_options<
      troll::ansi_font::none,
      troll::ansi_color::none,
      troll::ansi_color::cyan
    >;
    REQUIRE(styles::enabler_str_size == 5);
    REQUIRE(styles::enabler_str() == "\033[46m");
    REQUIRE(styles::disabler_str_size == 4);
    REQUIRE(styles::disabler_str() == "\033[0m");
  }

  SECTION("no styles") {
    using styles = troll::static_ansi_style_options<>;
    REQUIRE(styles::enabler_str_size == 0);
    REQUIRE(styles::enabler_str() == "");
    REQUIRE(styles::disabler_str_size == 0);
    REQUIRE(styles::disabler_str() == "");
  }
}

TEST_CASE("tabulate usage", "[tabulate]") {
  static const auto compare = [](auto &tab, auto expected) {
    etl::string<1000> act;
    for (etl::string_view sv : tab) {
      act += sv.data();
      act += "\n";
    }
    REQUIRE(act == expected);
  };

  SECTION("truncate only element") {
    const char *titles[] = {"TooLongTitle"};
                            // ^ 7 + 5
    int data[] = {123};
    auto tab = troll::make_tabulate<4, 7>(
      troll::static_ansi_style_options_none,
      troll::tabulate_title_row_args{"", titles, titles + 1, troll::static_ansi_style_options_none},
      troll::tabulate_elem_row_args{"", data, troll::static_ansi_style_options_none}
    );
    const char expected[] =
R"(+----------------------------+
|TooLong                     |
+----------------------------+
|  123                       |
+----------------------------+
)";
    compare(tab, expected);
  }

  SECTION("normal usage with one field and no style and no headings") {
    const char *titles[] = {"tita1", "tita2", "titb3", "titc4", "titx5", "titw6", "tita7", "titu8", "titz9", "titz10"};
    int data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto tab = troll::make_tabulate<4, 0, 7>(
      troll::static_ansi_style_options_none,
      troll::tabulate_title_row_args{titles, titles + 10, troll::static_ansi_style_options_none},
      troll::tabulate_elem_row_args{data, troll::static_ansi_style_options_none}
    );
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
    compare(tab, expected);
  }

  SECTION("multiple fields and styles") {
    const char *titles[] = {"tita1", "tita2", "titb3", "titc4", "titx5", "titw6", "tita7", "titu8"};
    int data[] = {1, 2, 3, 4, 57, 6, 7, 8};
    int data2[] = {1, 2, 3, 44, 5, 6, 7, 8};
    auto tab = troll::make_tabulate<8, 10>(
      troll::static_ansi_style_options<troll::ansi_font::none, troll::ansi_color::blue>{},
      troll::tabulate_title_row_args{"heading1", titles, titles + 8, troll::static_ansi_style_options<troll::ansi_font::bold>{}},
      troll::tabulate_elem_row_args{"elem1", data, troll::static_ansi_style_options_none},
      troll::tabulate_elem_row_args{"elem2", data2, troll::static_ansi_style_options<troll::ansi_font::none, troll::ansi_color::red>{}},
      troll::tabulate_elem_row_args{"elem1", titles, troll::static_ansi_style_options_none}
    );
    const char expected[] =
      "\033[34m+------------------------------------------------------------------------------------------+\033[0m\n"
      "\033[34m|\033[0m\033[1m heading1   tita1     tita2     titb3     titc4     titx5     titw6     tita7     titu8   \033[0m\033[34m|\033[0m\n"
      "\033[34m+------------------------------------------------------------------------------------------+\033[0m\n"
      "\033[34m|\033[0m  elem1       1         2         3         4         57        6         7         8     \033[34m|\033[0m\n"
      "\033[34m+------------------------------------------------------------------------------------------+\033[0m\n"
      "\033[34m|\033[0m\033[31m  elem2       1         2         3         44        5         6         7         8     \033[0m\033[34m|\033[0m\n"
      "\033[34m+------------------------------------------------------------------------------------------+\033[0m\n"
      "\033[34m|\033[0m  elem1     tita1     tita2     titb3     titc4     titx5     titw6     tita7     titu8   \033[34m|\033[0m\n"
      "\033[34m+------------------------------------------------------------------------------------------+\033[0m\n";
    compare(tab, expected);
  }

  SECTION("different styles and paddings used for heading and contents") {
    using namespace troll;
    const char *titles[] = {"tita1", "tita2", "titb3", "titc4"};
    int data[] = {1, 2, 3, 4};
    int data2[] = {1, 2, 3, 44};
    auto tab = make_tabulate<4, 20, 6>(
      static_ansi_style_options<ansi_font::none, ansi_color::yellow>{},
      tabulate_title_row_args{"heading1:", titles, titles + 4, /*heading*/static_ansi_style_options<ansi_font::bold>{}, /*contents*/static_ansi_style_options<ansi_font::italic>{}},
      tabulate_elem_row_args{"elem1:", data, /*heading*/static_ansi_style_options_none, /*contents*/static_ansi_style_options<ansi_font::bold>{}},
      tabulate_elem_row_args{"elem2:", data2, /*heading*/static_ansi_style_options<ansi_font::none, ansi_color::green>{}, /*contents*/static_ansi_style_options<ansi_font::none, ansi_color::red>{}}
    );

    const char *expected = 
      "\033[33m+--------------------------------------------+\033[0m\n"
      "\033[33m|\033[0m\033[1m     heading1:      \033[0m\033[3mtita1 tita2 titb3 titc4 \033[0m\033[33m|\033[0m\n"
      "\033[33m+--------------------------------------------+\033[0m\n"
      "\033[33m|\033[0m       elem1:       \033[1m  1     2     3     4   \033[0m\033[33m|\033[0m\n"
      "\033[33m+--------------------------------------------+\033[0m\n"
      "\033[33m|\033[0m\033[32m       elem2:       \033[0m\033[31m  1     2     3     44  \033[0m\033[33m|\033[0m\n"
      "\033[33m+--------------------------------------------+\033[0m\n";

    compare(tab, expected);
  }

  SECTION("use with range transform and resetting") {
    struct triple {
      int a, b, c;
    };
    triple data[] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    auto get_a = troll::it_transform(data, data + 3, [](auto &e) { return e.a; });
    auto get_b = troll::it_transform(data, data + 3, [](auto &e) { return e.b; });
    auto get_c = troll::it_transform(data, data + 3, [](auto &e) { return e.c; });

    auto tab = troll::make_tabulate<3, 7>(
      troll::static_ansi_style_options_none,
      troll::tabulate_title_row_args{"", get_a.begin(), get_a.end(), troll::static_ansi_style_options_none},
      troll::tabulate_elem_row_args{"", get_b.begin(), troll::static_ansi_style_options_none},
      troll::tabulate_elem_row_args{"", get_c.begin(), troll::static_ansi_style_options_none}
    );

    const char expected[] =
R"(+---------------------+
|   1      4      7   |
+---------------------+
|   2      5      8   |
+---------------------+
|   3      6      9   |
+---------------------+
)";
    compare(tab, expected);
    // iterate again
    compare(tab, expected);

    // mutate source
    data[0].a = 10;
    data[0].b = 11;
    data[0].c = 12;
    data[1].a = 13;
    data[1].b = 14;

    const char expected2[] =
R"(+---------------------+
|  10     13      7   |
+---------------------+
|  11     14      8   |
+---------------------+
|  12      6      9   |
+---------------------+
)";
    compare(tab, expected2);
    // iterate again
    compare(tab, expected2);
    compare(tab, expected2);

    // reset source
    triple data2[] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};

    get_a.reset_src_iterator(data2, data2 + 3);
    get_b.reset_src_iterator(data2, data2 + 3);
    get_c.reset_src_iterator(data2, data2 + 3);
    tab.reset_src_iterator(get_a.begin(), get_a.end(), get_b.begin(), get_c.begin());

    compare(tab, expected);
  }

  SECTION("only title row") {
    const char *titles[] = {"tita1", "tita2", "titb3", "titc4", "titx5", "titw6", "tita7", "titu8"};
    auto tab = troll::make_tabulate<8, 10>(
      troll::static_ansi_style_options_none,
      troll::tabulate_title_row_args{"heading1", titles, titles + 8, troll::static_ansi_style_options_none}
    );

    const char *expected =
R"(+------------------------------------------------------------------------------------------+
| heading1   tita1     tita2     titb3     titc4     titx5     titw6     tita7     titu8   |
+------------------------------------------------------------------------------------------+
)";

    compare(tab, expected);
  }

  SECTION("do patch") {
    const char *titles[] = {"tita1", "tita2", "titb3", "titc4", "titx5", "titw6", "tita7", "titu8", "tt9"};
    int data[] = {1, 2, 3, 4, 57, 6, 7, 8, 9};
    int data2[] = {1, 2, 3, 44, 5, 6, 7, 8, 9};
    auto tab = troll::make_tabulate<4, 12, 10>(
      troll::static_ansi_style_options<troll::ansi_font::none, troll::ansi_color::blue>{},
      troll::tabulate_title_row_args{"heading1", titles, titles + 9, troll::static_ansi_style_options<troll::ansi_font::bold>{}},
      troll::tabulate_elem_row_args{"elem1", data, troll::static_ansi_style_options_none},
      troll::tabulate_elem_row_args{"elem2", data2, troll::static_ansi_style_options<troll::ansi_font::none, troll::ansi_color::red>{}},
      troll::tabulate_elem_row_args{"elem1", titles, troll::static_ansi_style_options_none}
    );

    // patch title
    auto [row, col, pat] = tab.patch_str<0>(0, 1234);  // tita1
    REQUIRE(row == 1);
    REQUIRE(col == 13);
    REQUIRE(pat == "\033[1m   1234   \033[0m");
    auto [row2, col2, pat2] = tab.patch_str<0>(5, 1234);  // titw6
    REQUIRE(row2 == 9);
    REQUIRE(col2 == 23);

    // patch elem1
    auto [row3, col3, pat3] = tab.patch_str<1>(0, 1234);  // 1
    REQUIRE(row3 == 3);
    REQUIRE(col3 == 13);
    REQUIRE(pat3 == "   1234   ");
    auto [row4, col4, pat4] = tab.patch_str<1>(6, 1234);  // 7
    REQUIRE(row4 == 11);
    REQUIRE(col4 == 33);

    // patch elem2
    auto [row5, col5, pat5] = tab.patch_str<2>(2, 4321);  // 3
    REQUIRE(row5 == 5);
    REQUIRE(col5 == 33);
    REQUIRE(pat5 == "\033[31m   4321   \033[0m");
    auto [row6, col6, pat6] = tab.patch_str<2>(7, 4321);  // 8
    REQUIRE(row6 == 13);
    REQUIRE(col6 == 43);

    // patch elem3
    auto [row7, col7, pat7] = tab.patch_str<3>(8, 4321);  // tt9
    REQUIRE(row7 == 23);
    REQUIRE(col7 == 13);
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
