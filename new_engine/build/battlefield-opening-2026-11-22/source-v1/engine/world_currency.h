#ifndef VALWOOD_WORLD_CURRENCY_H
#define VALWOOD_WORLD_CURRENCY_H
#include <cstdint>
#include <string>
// All persisted balances and awards are copper base units. Existing save
// receipts retain their original integer value; denominations are lossless.
namespace world_currency {
constexpr std::uint64_t copper_per_silver=100, copper_per_gold=10000;
struct Coins {std::uint64_t gold,silver,copper;};
constexpr Coins split(std::uint64_t value){return {value/copper_per_gold,(value/copper_per_silver)%100,value%100};}
inline std::string format(std::uint64_t value){const auto c=split(value);return std::to_string(c.gold)+"g "+std::to_string(c.silver)+"s "+std::to_string(c.copper)+"c";}
}
#endif
