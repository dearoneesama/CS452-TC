#pragma once

#include <cstdint>
#include <utility>
#include <iterator>

namespace utils {
void uint32_to_buffer(char* buffer, uint32_t n);
uint32_t buffer_to_uint32(char* buffer);
}

namespace troll {
  /**
   * one-time iterator to transform a range.
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
        return that_ == other.that_;
      }

      constexpr bool operator!=(const iterator &other) const noexcept {
        return !(*this == other);
      }

      constexpr iterator &operator++() noexcept {
        if (that_->begin_ == that_->end_) {
          that_ = nullptr;
        } else {
          ++that_->begin_;
        }
        return *this;
      }

      constexpr auto operator*() const noexcept {
        return that_->fn_(*that_->begin_);
      }

    private:
      constexpr iterator(it_transform *that) noexcept : that_(that) {}
      friend class it_transform;
      it_transform *that_;
    };

    constexpr iterator begin() noexcept {
      return iterator(this);
    }
    constexpr iterator end() noexcept {
      return iterator(nullptr);
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
