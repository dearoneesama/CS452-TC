#pragma once

#include <fpm/fixed.hpp>
#include <troll_util/format.hpp>

template<class B, class I, auto F, auto R>
struct troll::to_stringer<fpm::fixed<B, I, F, R>> {
  void operator()(const fpm::fixed<B, I, F, R> f, ::etl::istring &s) const {
    auto int_part = static_cast<int>(f);
    auto first_frac = __builtin_abs(static_cast<int>(f * 10)) % 10;
    auto second_frac = __builtin_abs(static_cast<int>(f * 100)) % 10;
    sformat(s, "{}.{}{}", int_part, first_frac, second_frac);
  }
};
