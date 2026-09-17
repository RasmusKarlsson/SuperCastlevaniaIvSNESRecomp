#pragma once
#include <array>
#include <cstdint>

namespace cv4tuning {
enum Parameter {
  Walk, Crouch, Jump, Gravity, AirSpeed, AirControl, FallSpeed,
  LeatherDamage, ChainDamage, LongDamage, LeatherLimp, ChainLimp, LongLimp,
  DaggerDamage, AxeDamage, WaterDamage, CrossDamage,
  LeatherLength, ChainLength, LongLength,
  DaggerCost, AxeCost, WaterCost, CrossCost, ClockCost, Count
};
struct Definition { const char *key, *label; int normal, minimum, maximum; };
inline constexpr Definition definitions[Count] = {
  {"walk_percent", "Walk speed (%)", 100, 25, 300},
  {"crouch_percent", "Crouch speed (%)", 100, 0, 300},
  {"jump_percent", "Jump strength (%)", 100, 50, 200},
  {"gravity_percent", "Gravity (%)", 100, 25, 200},
  {"air_speed_percent", "Air speed (%)", 100, 25, 300},
  {"air_control_percent", "Air steering (%)", 100, 25, 300},
  {"fall_speed", "Maximum fall speed (pixels/frame)", 8, 4, 12},
  {"leather_damage", "Leather whip attack points", 32, 1, 255},
  {"chain_damage", "Chain whip attack points", 48, 1, 255},
  {"long_damage", "Long whip attack points", 48, 1, 255},
  {"leather_limp_damage", "Leather dangling attack points", 8, 1, 255},
  {"chain_limp_damage", "Chain dangling attack points", 12, 1, 255},
  {"long_limp_damage", "Long dangling attack points", 12, 1, 255},
  {"dagger_damage", "Dagger attack points", 32, 1, 255},
  {"axe_damage", "Axe attack points", 48, 1, 255},
  {"holy_water_damage", "Holy water attack points", 48, 1, 255},
  {"cross_damage", "Cross attack points", 48, 1, 255},
  {"leather_links", "Leather whip links", 5, 1, 7},
  {"chain_links", "Chain whip links", 5, 1, 7},
  {"long_links", "Long whip links", 7, 1, 7},
  {"dagger_cost", "Dagger heart cost", 1, 0, 20},
  {"axe_cost", "Axe heart cost", 1, 0, 20},
  {"holy_water_cost", "Holy water heart cost", 1, 0, 20},
  {"cross_cost", "Cross heart cost", 1, 0, 20},
  {"clock_cost", "Stopwatch heart cost", 5, 0, 20},
};
struct Values {
  std::array<int, Count> value{};
  Values() { for (int i=0; i<Count; ++i) value[i]=definitions[i].normal; }
};
void init(const uint8_t *rom, unsigned size);
bool apply(const Values &values);
bool available();
}
