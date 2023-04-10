#include "track_consts.hpp"

namespace tracks {
  etl::array<int, num_switches> const &valid_switches() {
    static etl::array<int, num_switches> valid_switches = {
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
      11, 12, 13, 14, 15, 16, 17, 18,
      153, 154, 155, 156,
    };
    return valid_switches;
  }

  etl::unordered_set<int, num_switches> const &broken_switches() {
    static etl::unordered_set<int, num_switches> broken_switches = {
#if IS_TRACK_A == 1
      156,
#else
      4, 5, 7,
#endif
    };
    return broken_switches;
  }

  etl::array<int, num_trains> const &valid_trains() {
    static etl::array<int, num_trains> valid_trains = {
      1, 2, 24, 58, 74, 78,
    };
    return valid_trains;
  }

  troll::string_map<const track_node *, 4, TRACK_MAX> const &valid_nodes() {
    static bool initialized = false;
    static track_node raw_nodes[TRACK_MAX];
    static troll::string_map<const track_node *, 4, TRACK_MAX> valid_nodes;

    if (!initialized) {
#if IS_TRACK_A == 1
      init_tracka(raw_nodes);
#else
      init_trackb(raw_nodes);
#endif
      for (size_t i = 0; i < TRACK_MAX; ++i) {
        valid_nodes[raw_nodes[i].name] = &raw_nodes[i];
      }
      initialized = true;
    }

    return valid_nodes;
  }

  fp &train_speed(int train, int target_speed_level, int old_speed_level) {
    static etl::unordered_map<int, etl::array<fp, 15>, num_trains>
    from_below = {
      {1, {
        fp{0}, fp{8.928571}, fp{12.855580}, fp{15.583554}, fp{36.042945},
        fp{69.117647}, fp{110.849057}, fp{158.783784}, fp{209.821429}, fp{279.761905},
        fp{345.588235}, fp{391.666667}, fp{451.923077}, fp{534.090909}, fp{587.500000}
      }},
      {2, {
        fp{0}, fp{11.773547}, fp{73.437500}, fp{125.000000}, fp{172.794118},
        fp{217.592593}, fp{279.761905}, fp{326.388889}, fp{391.666667}, fp{419.642857},
        fp{489.583333}, fp{534.090909}, fp{587.500000}, fp{587.500000}, fp{587.500000},
      }},
      {24, {
        fp{0}, fp{11.064030}, fp{14.399510}, fp{17.381657}, fp{38.149351},
        fp{72.530864}, fp{112.980769}, fp{163.194444}, fp{217.592593}, fp{279.761905},
        fp{345.588235}, fp{419.642857}, fp{489.583333}, fp{534.090909}, fp{534.090909},
      }},
      {58, {
        fp{0}, fp{13.231982}, fp{16.051913}, fp{17.128280}, fp{32.638889},
        fp{64.560440}, fp{99.576271}, fp{143.292683}, fp{183.593750}, fp{244.791667},
        fp{293.750000}, fp{367.187500}, fp{451.923077}, fp{457.792208}, fp{489.583333},
      }},
      {74, {
        fp{0}, fp{9.873950}, fp{77.302632}, fp{130.555556}, fp{167.857143},
        fp{225.961538}, fp{267.045455}, fp{230.392157}, fp{367.187500}, fp{419.642857},
        fp{489.583333}, fp{489.583333}, fp{451.923077}, fp{451.923077}, fp{451.923077},
      }},
      {78, {
        fp{0}, fp{6.977435}, fp{11.043233}, fp{15.025575}, fp{26.345291},
        fp{51.535088}, fp{89.015152}, fp{127.717391}, fp{172.794118}, fp{225.961538},
        fp{279.761905}, fp{309.210526}, fp{391.666667}, fp{419.642857}, fp{489.583333},
      }},
    },

    from_above = {
      {1, {
        fp{0}, fp{11.064030}, fp{12.912088}, fp{22.086466}, fp{53.409091},
        fp{83.928571}, fp{130.555556}, fp{183.593750}, fp{244.791667}, fp{293.750000},
        fp{367.187500}, fp{451.923077}, fp{489.583333}, fp{534.090909}, fp{534.090909},
      }},
      {2, {
        fp{0}, fp{12.447034}, fp{73.437500}, fp{125.000000}, fp{172.794118},
        fp{217.592593}, fp{279.761905}, fp{326.388889}, fp{391.666667}, fp{419.642857},
        fp{489.583333}, fp{534.090909}, fp{587.500000}, fp{587.500000}, fp{587.500000},
      }},
      {24, {
        fp{0}, fp{11.750000}, fp{15.708556}, fp{24.077869}, fp{55.952381},
        fp{89.015152}, fp{136.627907}, fp{189.516129}, fp{244.791667}, fp{309.210526},
        fp{391.666667}, fp{419.642857}, fp{489.583333}, fp{534.090909}, fp{534.090909},
      }},
      {58, {
        fp{0}, fp{14.088729}, fp{14.987245}, fp{23.221344}, fp{54.906542},
        fp{85.144928}, fp{117.500000}, fp{167.857143}, fp{217.592593}, fp{267.045455},
        fp{326.388889}, fp{419.642857}, fp{489.583333}, fp{534.090909}, fp{534.090909},
      }},
      {74, {
        fp{0}, fp{8.983180}, fp{77.302632}, fp{133.522727}, fp{167.857143},
        fp{225.961538}, fp{279.761905}, fp{326.388889}, fp{367.187500}, fp{419.642857},
        fp{451.923077}, fp{489.583333}, fp{451.923077}, fp{451.923077}, fp{451.923077},
        //              ^^^ avoid this one!!
      }},
      {78, {
        fp{0}, fp{10.416667}, fp{12.239583}, fp{14.949109}, fp{42.266187},
        fp{66.761364}, fp{104.910714}, fp{158.783784}, fp{195.833333}, fp{244.791667},
        fp{293.750000}, fp{367.187500}, fp{391.666667}, fp{451.923077}, fp{451.923077},
      }},
    };

    target_speed_level = target_speed_level >= 16 ? target_speed_level - 16 : target_speed_level;
    old_speed_level = old_speed_level >= 16 ? old_speed_level - 16 : old_speed_level;

    return (target_speed_level >= old_speed_level ? from_below : from_above)[train][target_speed_level];
  }

  fp &train_acceleration(int train, int target_speed_level, int old_speed_level) {
    // use 0~v model; might work
    static etl::unordered_map<int, etl::array<fp, 15>, num_trains>
    accel_from_zero = {
      {1, {
        fp{0}, fp{803.997787}, fp{803.997787}, fp{803.997787}, fp{803.997787},
        fp{803.997787}, fp{803.997787}, fp{323.907892}, fp{326.111347}, fp{211.531685},
        fp{177.166744}, fp{235.280334}, fp{193.234940}, fp{177.275937}, fp{188.919677},
      }},
      {2, {
        fp{0}, fp{86.805556}, fp{86.805556}, fp{86.805556}, fp{87.213526},
        fp{99.793636}, fp{93.174671}, fp{99.097402}, fp{102.405059}, fp{107.005286},
        fp{112.244898}, fp{112.244898}, fp{112.244898}, fp{109.105336}, fp{112.244898},
      }},
      {24, {
        fp{0}, fp{133.999631}, fp{133.999631}, fp{133.999631}, fp{133.999631},
        fp{133.999631}, fp{277.903583}, fp{277.903583}, fp{221.936889}, fp{211.531685},
        fp{202.022973}, fp{187.198313}, fp{182.970870}, fp{192.620263}, fp{192.620263},
      }},
      {58, {
        fp{0}, fp{800}, fp{800}, fp{800}, fp{800},
        fp{800}, fp{800}, fp{246.153366}, fp{188.239665}, fp{161.953946},
        fp{177.003205}, fp{145.267782}, fp{147.748919}, fp{146.968810}, fp{134.658337},
      }},
      {74, {
        fp{0}, fp{1100}, fp{1100}, fp{300.789760}, fp{181.613391},
        fp{99.217043}, fp{110.097687}, fp{805.686858}, fp{113.062189}, fp{113.980665},
        fp{106.095679}, fp{106.095679}, fp{95.221607}, fp{95.221607}, fp{90.401052},
      }},
      {78, {
        fp{0}, fp{234.704372}, fp{234.704372}, fp{234.704372}, fp{234.704372},
        fp{234.704372}, fp{69.728535}, fp{83.094095}, fp{87.213526}, fp{89.758218},
        fp{93.174671}, fp{96.731195}, fp{86.946596}, fp{95.336496}, fp{85.937500},
      }},
    },

    accel_to_zero = {
      {1, {
        fp{0}, fp{16.526593}, fp{16.526593}, fp{17.346226}, fp{32.477347},
        fp{55.549409}, fp{81.916756}, fp{100.849160}, fp{112.308755}, fp{135.879728},
        fp{136.962418}, fp{127.835648}, fp{138.935012}, fp{132.675860}, fp{132.752404},
      }},
      {2, {
        fp{0}, fp{9.901172}, fp{38.521903}, fp{52.083333}, fp{64.348722},
        fp{83.651124}, fp{108.703782}, fp{109.824440}, fp{151.883938}, fp{150.512930},
        fp{173.689739}, fp{189.662965}, fp{204.234467}, fp{188.816329}, fp{186.369465},
      }},
      {24, {
        fp{0}, fp{17.278824}, fp{17.278824}, fp{15.106100}, fp{36.384324},
        fp{59.780980}, fp{79.779089}, fp{95.115810}, fp{106.636343}, fp{126.236651},
        fp{136.337019}, fp{150.770657}, fp{153.648616}, fp{142.626550}, fp{112.304370},
      }},
      {58, {
        fp{0}, fp{21.885668}, fp{18.404564}, fp{20.955569}, fp{35.509902},
        fp{49.619647}, fp{69.826998}, fp{78.972281}, fp{92.600728}, fp{103.315448},
        fp{105.230564}, fp{115.830464}, fp{127.646542}, fp{104.786853}, fp{99.871600},
      }},
      {74, {
        fp{0}, fp{9.749488}, fp{56.374499}, fp{60.016736}, fp{64.921706},
        fp{96.337013}, fp{101.876107}, fp{61.012122}, fp{148.161165}, fp{170.970998},
        fp{210.256000}, fp{178.874508}, fp{145.881762}, fp{134.012118}, fp{129.590398},
      }},
      {78, {
        fp{0}, fp{4.868459}, fp{8.710928}, fp{16.126280}, fp{49.576742},
        fp{41.497895}, fp{66.030810}, fp{85.851221}, fp{96.315507}, fp{112.463914},
        fp{130.444539}, fp{98.568195}, fp{127.835648}, fp{111.455777}, fp{133.607492},
      }},
    };

    target_speed_level = target_speed_level >= 16 ? target_speed_level - 16 : target_speed_level;
    old_speed_level = old_speed_level >= 16 ? old_speed_level - 16 : old_speed_level;

    if (target_speed_level >= old_speed_level) {
      return accel_from_zero[train][target_speed_level];
    } else {
      return accel_to_zero[train][old_speed_level];
    }
  }

  std::tuple<int, int> accel_deaccel_distance(int train, int steady_speed_level) {
    auto v = train_speed(train, steady_speed_level, 0);
    auto a_accel = train_acceleration(train, steady_speed_level, 0);
    auto a_deaccel = train_acceleration(train, 0, steady_speed_level);
    if (a_accel == fp{} || a_deaccel == fp{}) {
      return { 0, 0 };
    }
    // v^2 = 2as
    auto vvd2 = (v * v) / 2;
    return { int{vvd2 / a_accel}, int{vvd2 / a_deaccel} };
  }

  std::tuple<fp, fp> accel_deaccel_time(int train, int steady_speed_level) {
    auto v = train_speed(train, steady_speed_level, 0);
    auto a_accel = train_acceleration(train, steady_speed_level, 0);
    auto a_deaccel = train_acceleration(train, 0, steady_speed_level);
    if (a_accel == fp{} || a_deaccel == fp{}) {
      return { fp{}, fp{} };
    }
    // v = at
    return { v / a_accel, v / a_deaccel };
  }

  int find_max_speed_level_for_dist(int train, int distance) {
    for (int i = 14; i > 0; --i) {
      auto [accel_dist, deaccel_dist] = accel_deaccel_distance(train, i);
      if (accel_dist + deaccel_dist <= distance) {
        return i;
      }
    }
    return 1;
  }
}
