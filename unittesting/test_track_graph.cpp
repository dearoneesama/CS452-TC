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
