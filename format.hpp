#pragma once

#include <etl/to_string.h>
#include <etl/queue.h>

// screen-printing utilities
#define LEN_LITERAL(x) (sizeof(x) / sizeof(x[0]) - 1)

#define SC_CLRSCR "\033[1;1H\033[2J"  // clear entire screen
#define SC_MOVSCR "\033[;H"           // move cursor to the top
#define SC_CLRLNE "\033[2K\r"         // clear current line
#define SC_HIDCUR "\033[?25l"         // hide cursor
#define SC_SHWCUR "\033[?25h"         // display cursor

namespace troll {

  template<class T>
  struct is_etl_string : std::false_type {};

  template<size_t N>
  struct is_etl_string<::etl::string<N>> : std::true_type {};

  // https://gist.github.com/cleoold/0f992b0fdee7dc3d6d0ed4497044d261 (young)
  constexpr inline char *snformat_impl(char *dest, size_t destlen, const char *format) {
    for (size_t i = destlen - 1; *format && i--;) {
      *dest++ = *format++;
    }
    *dest = '\0';
    return dest;
  }

  template<class Arg0, class ...Args>
  constexpr inline char *snformat_impl(char *dest, size_t destlen, const char *format, const Arg0 &a0, const Args &...args) {
    for (size_t i = destlen - 1; *format && i;) {
      if (*format == '{' && *(format + 1) == '}') {
        size_t real_len = i + 1;
        ::etl::string_ext s{dest, real_len};
        using Decay = std::decay_t<Arg0>;
        if constexpr (std::is_pointer_v<Decay> && std::is_same_v<std::remove_const_t<std::remove_pointer_t<Decay>>, char>) {
          // const char * <- to_string will print numbers instead
          s.assign(a0);
        } else if constexpr (is_etl_string<Decay>::value) {
          // etl::to_string does not support etl::string arg
          s.assign(a0);
        } else {
          ::etl::to_string(a0, s);
        }
        char *end = dest + s.length();
        return s.length() < real_len ? snformat_impl(end, real_len - s.length(), format + 2, args...) : end;
      }
      *dest++ = *format++;
      --i;
    }
    *dest = '\0';
    return dest;
  }

  /**
   * formats string into the buffer. returns length of result string excluding \0.
   * the function does not overflow the buffer.
   * the dest buffer needs an extra char to hold \0.
  */
  template<class ...Args>
  constexpr inline size_t snformat(char *dest, size_t destlen, const char *format, const Args &...args) {
    return snformat_impl(dest, destlen, format, args...) - dest;
  }

  /**
   * similar to the other snformat but the buffer size is automatically deduced.
  */
  template<size_t N, class ...Args>
  constexpr inline size_t snformat(char (&dest)[N], const char *format, const Args &...args) {
    static_assert(N);
    return snformat(dest, N, format, args...);
  }

  /**
   * bug: extra copy required.
  */
  template<size_t N, class ...Args>
  constexpr inline ::etl::string<N> sformat(const char *format, const Args &...args) {
    char buf[N + 1];
    snformat(buf, format, args...);
    return buf;
  }

  enum class padding {
    left,
    middle,
    right,
  };

  /**
   * pads the string to the specified length. the function writes to every character in the dest
   * buffer of length destlen and does not overflow it. this overload does not output the
   * terminating \0.
   * 
   * src: note ansi codes (colors) are not supported.
  */
  void pad(char *dest, size_t destlen, const char *src, size_t srclen, padding p, char padchar = ' ');

  /**
   * pads the string to the specified length. the function writes to every character in the dest
   * buffer except the last one which is always set to \0.
   * 
   * src: note ansi codes (colors) are not supported.
  */
  template<size_t DestLen, size_t SrcLen>
  constexpr void pad(char (&dest)[DestLen], const char (&src)[SrcLen], padding p, char padchar = ' ') {
    pad(dest, DestLen - 1, src, SrcLen - 1, p, padchar);
    dest[DestLen - 1] = '\0';
  }

  template<size_t DestLen>
  constexpr void pad(::etl::string<DestLen> &dest, ::etl::string_view src, padding p, char padchar = ' ') {
    return pad(dest.data(), DestLen, src.data(), src.size(), p, padchar);
  }

  /**
   * single-use iterator to tabulate text. it outputs text line by line.
   * 
   * the MaxLineWidth needs to be sufficiently big to hold enough elements per row.
   */
  template<size_t MaxLineWidth, size_t Pad, class TitleIt, class ElemsIt>
  class tabulate_1d {
  public:
    using size_type = size_t;
    static constexpr size_type max_line_width = MaxLineWidth;
    static constexpr size_type pad = Pad;

    /**
     * - t_begin: start of title range
     * - t_end: end of title range
     * - e_begin: start of element range. this assumes the number of elements equal that of titles
     * - elems_row: number of elements per row. nonzero.
     */
    constexpr tabulate_1d(TitleIt t_begin, TitleIt t_end, ElemsIt e_begin, size_type elems_row)
      : t_begin_{t_begin}, t_end_{t_end}, e_begin_{e_begin}, elems_row_{elems_row} {
      troll::pad(bar_text_, pad * elems_row + 2, "", 0, padding::left, '-');
      // extra + at both sides
      bar_text_[0] = '+';
      bar_text_[pad * elems_row + 1] = '+';
      bar_text_[pad * elems_row + 2] = '\0';
    }

    // single-use iterator
    class iterator {
    public:
      using difference_type = size_t;  // never
      using value_type = ::etl::string_view;
      using pointer = const value_type *;
      using reference = const value_type &;
      using iterator_category = std::input_iterator_tag;

      constexpr bool operator==(const iterator &other) const {
        return that_ == other.that_;
      }

      constexpr bool operator!=(const iterator &other) const {
          return !(*this == other);
      }

      constexpr value_type operator*() const {
        switch (that_->state_) {
          case state::top_line:
          case state::middle_line:
            return that_->bar_text_;
          case state::title_line:
            return that_->title_text_;
          case state::elem_line:
            return that_->elem_text_;
          default:
            __builtin_unreachable();
        }
      }

      constexpr iterator &operator++() {
        if (that_->state_ == state::top_line) {
          char buf[pad + 1];
          troll::pad(that_->title_text_, sizeof that_->title_text_, "", 0, padding::left);
          that_->title_text_[0] = '|';
          // get titles
          size_type titles = 0;
          for (; that_->t_begin_ != that_->t_end_ && titles < that_->elems_row_; ++that_->t_begin_, ++titles) {
            auto sz = snformat(buf, pad + 1, "{}", *that_->t_begin_);
            troll::pad(that_->title_text_ + titles * that_->pad + 1, that_->pad, buf, sz, padding::middle);
          }

          if (titles == 0) {
            // no more
            that_ = nullptr;
            return *this;
          }

          that_->title_text_[that_->elems_row_ * that_->pad + 1] = '|';
          that_->title_text_[that_->elems_row_ * that_->pad + 2] = '\0';

          troll::pad(that_->elem_text_, sizeof that_->elem_text_, "", 0, padding::left);
          that_->elem_text_[0] = '|';
          // get elements
          size_type elems = 0;
          for (; elems < titles; ++that_->e_begin_, ++elems) {
            auto sz = snformat(buf, pad + 1, "{}", *that_->e_begin_);
            troll::pad(that_->elem_text_ + elems * that_->pad + 1, that_->pad, buf, sz, padding::middle);
          }

          that_->elem_text_[that_->elems_row_ * that_->pad + 1] = '|';
          that_->elem_text_[that_->elems_row_ * that_->pad + 2] = '\0';
        }

        // advance state
        that_->state_ = static_cast<state>((static_cast<char>(that_->state_) + 1) % (static_cast<char>(state::end)));
        return *this;
      }

    private:
      friend class tabulate_1d;

      constexpr iterator(tabulate_1d *that)
        : that_{that} {}

      iterator(iterator &) = delete;
      iterator &operator=(iterator &) = delete;

      tabulate_1d *that_;
    };

    constexpr iterator begin() {
      return iterator{this};
    }

    constexpr iterator end() {
      return iterator{nullptr};
    }

  private:
    TitleIt t_begin_, t_end_;
    ElemsIt e_begin_;
    size_type elems_row_;
    char title_text_[max_line_width], elem_text_[max_line_width], bar_text_[max_line_width];

    enum class state : char {
      top_line = 0,
      title_line,
      middle_line,
      elem_line,
      end,
    } state_ = state::top_line;

    friend class iterator;
  };

  template<size_t MaxLineWidth, size_t Pad, class TitleIt, class ElemsIt>
  auto make_tabulate_1d(TitleIt t_begin, TitleIt t_end, ElemsIt e_begin, size_t elems_row) {
    return tabulate_1d<MaxLineWidth, Pad, TitleIt, ElemsIt>{t_begin, t_end, e_begin, elems_row};
  }

  /**
   * the class supports specifying text at a certain line and at a certain column.
   * it maintains an internal queue for texts, and when on demand, it will output a
   * string with ansi escape codes which can be used in writing to a terminal.
   * 
   * the usage of this class means that it takes entire control of the terminal ui,
   * mostly if not all.
   */
  template<size_t MaxLineWidth, size_t MaxLines, size_t MaxQueueSize = MaxLines>
  class OutputControl {
    static_assert(MaxLineWidth);
    static_assert(MaxLines);
    static_assert(MaxQueueSize);
  public:
    using size_type = size_t;
    static constexpr size_type max_line_width = MaxLineWidth;
    static constexpr size_type max_lines = MaxLines;
    static constexpr size_type max_queue_size = MaxQueueSize;

    constexpr OutputControl() {
      snformat(move_cursor_to_bottom_, "\033[{};1H", max_lines + 1);
    }

    /**
     * submit a text to be outputted at a certain line and column. returns the number of
     * characters that were successfully enqueued.
     * 
     * - line: 0-based
     * - column: 0-based
     * - text: do not include ansi escape codes other than color etc. but notice that colors
     *         occupy character buffers.
     *         text is null-terminated. if it is a nullptr, then it corresponds to clearing
     *         the line. the call will return 0.
     */
    size_type enqueue(size_type line, size_type column, const char *text) {
      if (queue_.full()) {
        return 0;
      }
      // TODO: emplace only if text compares different
      queue_.emplace();
      Request &ref = queue_.back();
      ref.line = line;
      ref.column = column;
      return text ? snformat(ref.text, max_line_width, text) : (ref.text[0] = 0);
    }

  /**
   * get a string ready to be outputted to a terminal, or nullptr if there is none.
   * the string returned is a reference to an internal buffer, and it will be invalid
   * after the next call to this function.
   */
  ::etl::string_view dequeue() {
    if (queue_.empty()) {
      return {"", size_type(0)};
    }
    Request &ref = queue_.front();
    const char *content = ref.text ? ref.text : "\033[K";
    // output ansi code to relocate the cursor, and write text
    auto sz = snformat(current_text_, "\033[{};{}H{}", ref.line + 1, ref.column + 1, content);
    // at the end, locate the cursor to the bottom. if user wants to dodge the OutputControl
    // and prints text directly, this will make sure it would behave as expected.
    sz += snformat(current_text_ + sz, sizeof current_text_ - sz, move_cursor_to_bottom_);
    queue_.pop();
    return {current_text_, sz};
  }

  bool empty() const {
    return queue_.empty();
  }

  private:
    struct Request {
      size_type line;
      size_type column;
      char text[max_line_width];
    };

    static constexpr size_type ansi_code_size = 22;

    char move_cursor_to_bottom_[10];
    char current_text_[max_line_width + ansi_code_size + sizeof move_cursor_to_bottom_];
    ::etl::queue<Request, max_queue_size> queue_;
  };
}  // namespace troll
