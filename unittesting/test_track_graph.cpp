#include <catch_amalgamated.hpp>

#include "../track_graph.hpp"


TEST_CASE("next_sensor usage", "[next_sensor]") {
  auto const *E12 = tracks::valid_nodes().at("E12");
  auto const *D11 = tracks::valid_nodes().at("D11");
  auto result = tracks::next_sensor(E12, {{ 7, tracks::switch_dir_t::S }});
  REQUIRE(std::get<0>(result.value()) == D11);
  REQUIRE(std::get<1>(result.value()) == 50 + 231);

  auto const *C8 = tracks::valid_nodes().at("C8");
  result = tracks::next_sensor(E12, {{ 7, tracks::switch_dir_t::C }, { 18, tracks::switch_dir_t::S }});
  REQUIRE(std::get<0>(result.value()) == C8);

  SECTION("adjacent") {
    auto const *C16 = tracks::valid_nodes().at("C16");
    result = tracks::next_sensor(D11, {});
    REQUIRE(std::get<0>(result.value()) == C16);
  }
}

TEST_CASE("find_path usage", "find_path") {
  auto const *E11 = tracks::valid_nodes().at("E11"),
             *E10 = tracks::valid_nodes().at("E10"),
             *E12 = tracks::valid_nodes().at("E12"),
             *E13 = tracks::valid_nodes().at("E13"),
             *D5 = tracks::valid_nodes().at("D5"),
             *D6 = tracks::valid_nodes().at("D6"),
             *D10 = tracks::valid_nodes().at("D10"),
             *D11 = tracks::valid_nodes().at("D11"),
             *D12 = tracks::valid_nodes().at("D12"),
             *BR7 = tracks::valid_nodes().at("BR7"),
             *BR8 = tracks::valid_nodes().at("BR8"),
             *BR9 = tracks::valid_nodes().at("BR9"),
             *MR8 = tracks::valid_nodes().at("MR8"),
             *MR9 = tracks::valid_nodes().at("MR9");

  SECTION("same sensor only calc offsets") {
    auto result = tracks::find_path(E12, E12, 106, 200, {});
    REQUIRE(result->size() == 1);
    auto &seg1_vec = std::get<0>(result->at(0));
    auto seg1_dist = std::get<1>(result->at(0));
    REQUIRE(seg1_vec.size() == 1);
    REQUIRE(seg1_vec[0] == E12);
    REQUIRE(seg1_dist == 94);
    auto seg1_str = tracks::stringify_node_path_segment<50>(result->at(0));
    REQUIRE(seg1_str == "(E12;94)");
    REQUIRE(seg1_str.size() == 8);
  }

  SECTION("opposite sensor do nothing but reverse") {
    auto result = tracks::find_path(E11, E12, 0, 0, {});
    REQUIRE(result->size() == 2);
    auto &seg1_vec = std::get<0>(result->at(0));
    REQUIRE(seg1_vec.size() == 1);
    REQUIRE(seg1_vec[0] == E11);
    auto &seg2_vec = std::get<0>(result->at(1));
    REQUIRE(seg2_vec.size() == 1);
    REQUIRE(seg2_vec[0] == E12);
  }

  SECTION("E12->D11 section straight") {
    auto result = tracks::find_path(E12, D11, 0, 16, {});
    REQUIRE(result->size() == 1);
    auto &seg1_vec = std::get<0>(result->at(0));
    auto seg1_dist = std::get<1>(result->at(0));
    REQUIRE(seg1_vec.size() == 3);
    REQUIRE(seg1_vec[0] == E12);
    REQUIRE(seg1_vec[1] == BR7);
    REQUIRE(seg1_vec[2] == D11);
    REQUIRE(seg1_dist == 50 + 231 + 16);
    auto seg1_str = tracks::stringify_node_path_segment<50>(result->at(0));
    REQUIRE(seg1_str == "(E12,BR7,D11;297)");
    REQUIRE(seg1_str.size() == 17);
  }

  SECTION("E12->D11->D12 reverse at destination") {
    auto result = tracks::find_path(E12, D12, 0, 0, {});
    REQUIRE(result->size() == 2);
    auto &seg1_vec = std::get<0>(result->at(0));
    auto seg1_dist = std::get<1>(result->at(0));
    REQUIRE(seg1_vec.size() == 3);
    REQUIRE(seg1_vec[0] == E12);
    REQUIRE(seg1_vec[1] == BR7);
    REQUIRE(seg1_vec[2] == D11);
    REQUIRE(seg1_dist == 50 + 231);
    auto &seg2_vec = std::get<0>(result->at(1));
    auto seg2_dist = std::get<1>(result->at(1));
    REQUIRE(seg2_vec.size() == 1);
    REQUIRE(seg2_vec[0] == D12);
    REQUIRE(seg2_dist == 0);
  }

  SECTION("E12->E13 reverse at beginning, then in middle") {
    auto result = tracks::find_path(E12, E13, 0, 0, {});
    REQUIRE(result->size() == 3);
    auto &seg1_vec = std::get<0>(result->at(0));
    auto seg1_dist = std::get<1>(result->at(0));
    REQUIRE(seg1_vec.size() == 1);
    REQUIRE(seg1_vec[0] == E12);
    REQUIRE(seg1_dist == 0);

    auto &seg2_vec = std::get<0>(result->at(1));
    auto seg2_dist = std::get<1>(result->at(1));
    REQUIRE(seg2_vec.size() == 5);
    REQUIRE(seg2_vec[0] == E11);
    REQUIRE(seg2_vec[1] == D10);
    REQUIRE(seg2_vec[2] == MR8);
    REQUIRE(seg2_vec[3] == BR9);
    REQUIRE(seg2_vec[4] == D5);
    REQUIRE(seg2_dist == 1079);

    auto &seg3_vec = std::get<0>(result->at(2));
    auto seg3_dist = std::get<1>(result->at(2));
    REQUIRE(seg3_vec.size() == 5);
    REQUIRE(seg3_vec[0] == D6);
    REQUIRE(seg3_vec[1] == MR9);
    REQUIRE(seg3_vec[2] == BR8);
    REQUIRE(seg3_vec[3] == E10);
    REQUIRE(seg3_vec[4] == E13);
    REQUIRE(seg3_dist == 1009);
  }
}

TEST_CASE("walk_sensor nonsegment", "[walk_sensor]") {
  auto const *E12 = tracks::valid_nodes().at("E12"),
             *D11 = tracks::valid_nodes().at("D11"),
             *C16 = tracks::valid_nodes().at("C16");

  SECTION("dist is too small") {
    auto result = tracks::walk_sensor(E12, 0, 30, {{ 7, tracks::switch_dir_t::S }});
    REQUIRE(result.size() == 2);
    REQUIRE(result[0] == E12);
    REQUIRE(result[1] == D11);
  }

  SECTION("offset is big to cover 3 sensors") {
    auto result = tracks::walk_sensor(E12, 270, 30, {{ 7, tracks::switch_dir_t::S }});
    REQUIRE(result.size() == 3);
    REQUIRE(result[0] == E12);
    REQUIRE(result[1] == D11);
    REQUIRE(result[2] == C16);
  }
}

TEST_CASE("walk_sensor on path segment", "[walk_sensor]") {
  auto const *D6 = tracks::valid_nodes().at("D6"),
             *D15 = tracks::valid_nodes().at("D15"),
             *E10 = tracks::valid_nodes().at("E10"),
             *E13 = tracks::valid_nodes().at("E13"),
             *MR9 = tracks::valid_nodes().at("MR9"),
             *BR8 = tracks::valid_nodes().at("BR8");
  tracks::node_path_segment_vec_t seg1 = {
    D6, MR9, BR8, E10, E13,
  };

  SECTION("dist is small to cover next sensor only") {
    auto result = tracks::walk_sensor(0, 0, 30, seg1, {});
    REQUIRE(result.size() == 2);
    REQUIRE(result[0] == D6);
    REQUIRE(result[1] == E10);
  }

  SECTION("dist is small to cover next sensor only, with j") {
    auto result = tracks::walk_sensor(3, 0, 30, seg1, {});
    REQUIRE(result.size() == 2);
    REQUIRE(result[0] == E10);
    REQUIRE(result[1] == E13);
  }

  SECTION("dist can be covered by the path") {
    auto result = tracks::walk_sensor(0, 8, 1000, seg1, {});
    REQUIRE(result.size() == 3);
    REQUIRE(result[0] == D6);
    REQUIRE(result[1] == E10);
    REQUIRE(result[2] == E13);
  }

  SECTION("dist cannot be covered by the path") {
    auto result = tracks::walk_sensor(0, 10, 1000, seg1, {{ 17, tracks::switch_dir_t::C}});
    REQUIRE(result.size() == 4);
    REQUIRE(result[0] == D6);
    REQUIRE(result[1] == E10);
    REQUIRE(result[2] == E13);
    REQUIRE(result[3] == D15);
  }
}
