#pragma once

#include <cstdint>
#include <utility>
#include <iterator>

namespace utils {
  void uint32_to_buffer(char* buffer, uint32_t n);
  uint32_t buffer_to_uint32(char* buffer);

  template<class EnumT, class ClsT>
  struct enumed_class {
    EnumT header;
    ClsT data;
    // NOTE: if sizeof(EnumT == 8) and sizeof(ClsT == 1) (eg char),
    // then sizeof(enumed_class) is not necessarily 9!

    template<class T>
    auto &data_as() {
      return *reinterpret_cast<T *>(&data);
    }
  };

  template<class EnumT, class ClsT>
  enumed_class(EnumT, ClsT) -> enumed_class<EnumT, ClsT>;
}

namespace troll {
  /**
   * iterator to transform a range.
   */
  template<class InputIt, class Fn>
  class it_transform {
  public:
    constexpr it_transform(InputIt begin, InputIt end, Fn fn) noexcept
      : begin_(begin), end_(end), fn_(std::move(fn)) {}

    class iterator {
    public:
      using difference_type = size_t;  // never
      using value_type = decltype(std::declval<Fn>()(*std::declval<InputIt>()));
      using pointer = const value_type *;
      using reference = const value_type &;
      using iterator_category = std::input_iterator_tag;

      constexpr bool operator==(const iterator &other) const noexcept {
        return curr_ == other.curr_ && that_ == other.that_;
      }

      constexpr bool operator!=(const iterator &other) const noexcept {
        return !(*this == other);
      }

      constexpr iterator &operator++() noexcept {
        ++curr_;
        return *this;
      }

      constexpr decltype(auto) operator*() const noexcept {
        return that_->fn_(*curr_);
      }

    private:
      constexpr iterator(it_transform *that, InputIt begin) noexcept : that_(that), curr_(begin) {}
      friend class it_transform;
      it_transform *that_;
      InputIt curr_;
    };

    constexpr iterator begin() noexcept {
      return iterator(this, begin_);
    }
    constexpr iterator end() noexcept {
      return iterator(this, end_);
    }

    constexpr void reset_src_iterator(InputIt begin, InputIt end) noexcept {
      begin_ = begin;
      end_ = end;
    }

  private:
    friend class iterator;
    InputIt begin_, end_;
    Fn fn_;
  };

  template<class InputIt, class Fn>
  it_transform(InputIt, InputIt, Fn) -> it_transform<InputIt, Fn>;
}  // namespace troll
