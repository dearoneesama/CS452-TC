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

  template<size_t N, class ...Args>
  constexpr inline ::etl::string<N> sformat(const char *format, const Args &...args) {
    ::etl::string<N> buf;
    auto sz = snformat(buf.data(), N + 1, format, args...);
    buf.uninitialized_resize(sz);
    return buf;
  }

  enum class padding {
    left,
    middle,
    right,
  };

  /**
   * pads the string to the specified length. the function writes to every character in the dest
   * buffer of length dest_pad_len and does not overflow it. this overload does not output the
   * terminating \0.
   * 
   * src: note ansi codes (colors) are not supported.
  */
  void pad(char *__restrict__ dest, size_t dest_pad_len, const char *__restrict__ src, size_t srclen, padding p, char padchar = ' ');

  /**
   * pads the string to the specified length. the function writes to every character in the dest
   * buffer except the last one which is always set to \0.
   * 
   * src: note ansi codes (colors) are not supported.
  */
  template<size_t DestPadLen, size_t SrcLen>
  constexpr void pad(char (& __restrict__ dest)[DestPadLen], const char (& __restrict__ src)[SrcLen], padding p, char padchar = ' ') {
    pad(dest, DestPadLen - 1, src, SrcLen - 1, p, padchar);
    dest[DestPadLen - 1] = '\0';
  }

  template<size_t DestPadLen>
  constexpr ::etl::string<DestPadLen> pad(::etl::string_view src, padding p, char padchar = ' ') {
    ::etl::string<DestPadLen> buf;
    pad(buf.data(), DestPadLen, src.data(), src.size(), p, padchar);
    buf.uninitialized_resize(DestPadLen);
    return buf;
  }

  enum class ansi_font: uint8_t {
    none          = 0,            // enabler,    disabler
    bold          = 0b0000'0001,  // "\033[1m", "\033[22m"
    dim           = 0b0000'0010,  // "\033[2m", "\033[22m"
    italic        = 0b0000'0100,  // "\033[3m", "\033[23m"
    underline     = 0b0000'1000,  // "\033[4m", "\033[24m"
    blink         = 0b0001'0000,  // "\033[5m", "\033[25m"
    reverse       = 0b0010'0000,  // "\033[7m", "\033[27m"
    hidden        = 0b0100'0000,  // "\033[8m", "\033[28m"
    strikethrough = 0b1000'0000,  // "\033[9m", "\033[29m"
  };

  constexpr inline ansi_font operator|(ansi_font a, ansi_font b) {
    return static_cast<ansi_font>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
  }

  constexpr inline ansi_font operator&(ansi_font a, ansi_font b) {
    return static_cast<ansi_font>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
  }

  enum class ansi_color: uint8_t {
                  // foreground, background
    none    = 0,  // "\033[39m", "\033[49m"
    black   = 1,  // "\033[30m", "\033[40m"
    red     = 2,  // "\033[31m", "\033[41m"
    green   = 3,  // "\033[32m", "\033[42m"
    yellow  = 4,  // "\033[33m", "\033[43m"
    blue    = 5,  // "\033[34m", "\033[44m"
    magenta = 6,  // "\033[35m", "\033[45m"
    cyan    = 7,  // "\033[36m", "\033[46m"
    white   = 8,  // "\033[37m", "\033[47m"
  };

  /**
   * a compile-time object that holds ansi style options and does escape string
   * things.
   */
  template<ansi_font Font = ansi_font::none, ansi_color FgColor = ansi_color::none, ansi_color BgColor = ansi_color::none>
  struct static_ansi_style_options {
    static constexpr auto font = Font;
    static constexpr auto fg_color = FgColor;
    static constexpr auto bg_color = BgColor;

  private:
    static constexpr size_t num_params_
      = __builtin_popcount(static_cast<uint8_t>(font))
      + (fg_color == ansi_color::none ? 0 : 1)
      + (bg_color == ansi_color::none ? 0 : 1);
    static constexpr size_t num_semicolons_ = num_params_ ? num_params_ - 1 : 0;

    static char *write_to(char *dest, const char *src) {
      while (*src) *dest++ = *src++;
      return dest;
    }

  public:
    // the size of the escape string used to start the style (excluding \0).
    static constexpr size_t enabler_str_size
      = (num_params_ ? (LEN_LITERAL("\033[m")
      + __builtin_popcount(static_cast<uint8_t>(font)) * 1
      + (fg_color == ansi_color::none ? 0 : 2)
      + (bg_color == ansi_color::none ? 0 : 2)
      + num_semicolons_) : 0);

    // the escape string used to start the style.
    static ::etl::string_view enabler_str() {
      static char buf[enabler_str_size + 1] = "";

      if (!*buf && num_params_) {
        char *p = buf;
        p = write_to(p, "\033[");
        // font
        if ((font & ansi_font::bold) == ansi_font::bold)                   p = write_to(p, "1;");
        if ((font & ansi_font::dim) == ansi_font::dim)                     p = write_to(p, "2;");
        if ((font & ansi_font::italic) == ansi_font::italic)               p = write_to(p, "3;");
        if ((font & ansi_font::underline) == ansi_font::underline)         p = write_to(p, "4;");
        if ((font & ansi_font::blink) == ansi_font::blink)                 p = write_to(p, "5;");
        if ((font & ansi_font::reverse) == ansi_font::reverse)             p = write_to(p, "7;");
        if ((font & ansi_font::hidden) == ansi_font::hidden)               p = write_to(p, "8;");
        if ((font & ansi_font::strikethrough) == ansi_font::strikethrough) p = write_to(p, "9;");
        // fg color
        switch (fg_color) {
          case ansi_color::black:   p = write_to(p, "30;"); break;
          case ansi_color::red:     p = write_to(p, "31;"); break;
          case ansi_color::green:   p = write_to(p, "32;"); break;
          case ansi_color::yellow:  p = write_to(p, "33;"); break;
          case ansi_color::blue:    p = write_to(p, "34;"); break;
          case ansi_color::magenta: p = write_to(p, "35;"); break;
          case ansi_color::cyan:    p = write_to(p, "36;"); break;
          case ansi_color::white:   p = write_to(p, "37;"); break;
          default: break;
        }
        // bg color
        switch (bg_color) {
          case ansi_color::black:   p = write_to(p, "40;"); break;
          case ansi_color::red:     p = write_to(p, "41;"); break;
          case ansi_color::green:   p = write_to(p, "42;"); break;
          case ansi_color::yellow:  p = write_to(p, "43;"); break;
          case ansi_color::blue:    p = write_to(p, "44;"); break;
          case ansi_color::magenta: p = write_to(p, "45;"); break;
          case ansi_color::cyan:    p = write_to(p, "46;"); break;
          case ansi_color::white:   p = write_to(p, "47;"); break;
          default: break;
        }
        if (*(p - 1) == ';') --p;
        if (num_params_) *p++ = 'm';
        *p = '\0';
      }

      return {buf, enabler_str_size};
    }

    // the size of the escape string used to end the style (excluding \0).
    // we are just using the reset escape string for now.
    static constexpr size_t disabler_str_size = num_params_ ? LEN_LITERAL("\033[0m") : 0;

    // the escape string used to end the style.
    static ::etl::string_view disabler_str() {
      static char buf[disabler_str_size + 1] = "";
      if (!*buf && num_params_)
        write_to(buf, "\033[0m");
      return {buf, disabler_str_size};
    }

    // the number of extra characters needed to wrap any string with the style.
    static constexpr size_t wrapper_str_size = enabler_str_size + disabler_str_size;
  };

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
