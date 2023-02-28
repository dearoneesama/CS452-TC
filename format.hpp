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

  char *strcontcpy(char *dest, const char *src) noexcept;

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
        p = strcontcpy(p, "\033[");
        // font
        if ((font & ansi_font::bold) == ansi_font::bold)                   p = strcontcpy(p, "1;");
        if ((font & ansi_font::dim) == ansi_font::dim)                     p = strcontcpy(p, "2;");
        if ((font & ansi_font::italic) == ansi_font::italic)               p = strcontcpy(p, "3;");
        if ((font & ansi_font::underline) == ansi_font::underline)         p = strcontcpy(p, "4;");
        if ((font & ansi_font::blink) == ansi_font::blink)                 p = strcontcpy(p, "5;");
        if ((font & ansi_font::reverse) == ansi_font::reverse)             p = strcontcpy(p, "7;");
        if ((font & ansi_font::hidden) == ansi_font::hidden)               p = strcontcpy(p, "8;");
        if ((font & ansi_font::strikethrough) == ansi_font::strikethrough) p = strcontcpy(p, "9;");
        // fg color
        switch (fg_color) {
          case ansi_color::black:   p = strcontcpy(p, "30;"); break;
          case ansi_color::red:     p = strcontcpy(p, "31;"); break;
          case ansi_color::green:   p = strcontcpy(p, "32;"); break;
          case ansi_color::yellow:  p = strcontcpy(p, "33;"); break;
          case ansi_color::blue:    p = strcontcpy(p, "34;"); break;
          case ansi_color::magenta: p = strcontcpy(p, "35;"); break;
          case ansi_color::cyan:    p = strcontcpy(p, "36;"); break;
          case ansi_color::white:   p = strcontcpy(p, "37;"); break;
          default: break;
        }
        // bg color
        switch (bg_color) {
          case ansi_color::black:   p = strcontcpy(p, "40;"); break;
          case ansi_color::red:     p = strcontcpy(p, "41;"); break;
          case ansi_color::green:   p = strcontcpy(p, "42;"); break;
          case ansi_color::yellow:  p = strcontcpy(p, "43;"); break;
          case ansi_color::blue:    p = strcontcpy(p, "44;"); break;
          case ansi_color::magenta: p = strcontcpy(p, "45;"); break;
          case ansi_color::cyan:    p = strcontcpy(p, "46;"); break;
          case ansi_color::white:   p = strcontcpy(p, "47;"); break;
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
        strcontcpy(buf, "\033[0m");
      return {buf, disabler_str_size};
    }

    // the number of extra characters needed to wrap any string with the style.
    static constexpr size_t wrapper_str_size = enabler_str_size + disabler_str_size;
  };

  template<class Heading, class TitleIt, class Style>
  struct tabulate_title_row_args {
    using heading_type = Heading;
    using title_it_type = TitleIt;
    using style_type = Style;
    // the heading (first column) used for the title row
    Heading heading;
    // title range
    TitleIt begin, end;
    Style style;
  };

  template<class Heading, class TitleIt, class Style>
  tabulate_title_row_args(Heading, TitleIt, TitleIt, Style) -> tabulate_title_row_args<Heading, TitleIt, Style>;

  template<class Heading, class ElemIt, class Style>
  struct tabulate_elem_row_args {
    using heading_type = Heading;
    using elem_it_type = ElemIt;
    using style_type = Style;
    // the heading (first column) used for the element row
    Heading heading;
    // element range
    ElemIt begin;
    Style style;
  };

  template<class Heading, class ElemIt, class Style>
  tabulate_elem_row_args(Heading, ElemIt, Style) -> tabulate_elem_row_args<Heading, ElemIt, Style>;

  /**
   * single-use iterator to tabulate text. it outputs text line by line.
   * 
   * the MaxLineWidth needs to be sufficiently big to hold enough elements per row.
   */
  template<size_t MaxLineWidth, size_t Pad, class DividerStyle, class TitleRowArgs, class ...ElemRowArgs>
  class tabulate {
  public:
    using size_type = size_t;
    static constexpr size_type max_line_width = MaxLineWidth;
    static constexpr size_type pad = Pad;
    using divider_style_type = DividerStyle;
    using title_row_args_type = TitleRowArgs;
    using elem_row_args_type = std::tuple<ElemRowArgs...>;
    static constexpr size_type num_elem_row_args = sizeof...(ElemRowArgs);

    char divider_horizontal = '-';
    char divider_vertical = '|';
    char divider_cross = '+';

  template<class Tit, class ...Elems>
  constexpr tabulate(size_type elems_per_row, Tit &&title, Elems &&...elems)
    : elems_per_row_{elems_per_row}
    , title_row_args_{std::forward<Tit>(title)}
    , elem_row_args_{std::forward<Elems>(elems)...}
  {
    auto title_heading = sformat<pad>("{}", title_row_args_.heading);
    has_heading_ = title_heading.size();
    auto total_pad = elems_per_row_ * pad + (has_heading_ ? pad : 0);
    // write the divider line
    char *p = divider_text_;
    p = strcontcpy(p, divider_style_type::enabler_str().data());
    *p++ = divider_cross;
    for (size_t i = 0; i < total_pad; ++i) {
      *p++ = divider_horizontal;
    }
    *p++ = divider_cross;
    p = strcontcpy(p, divider_style_type::disabler_str().data());
    *p = '\0';

    // prepare prefix and suffix for title row
    troll::pad(title_text_, sizeof title_text_, "", 0, padding::left);
    p = title_text_;
    p = strcontcpy(p, divider_style_type::enabler_str().data());
    *p++ = divider_vertical;
    p = strcontcpy(p, divider_style_type::disabler_str().data());
    p = strcontcpy(p, title_row_args_.style.enabler_str().data());
    if (has_heading_) {
      troll::pad(p, pad, title_heading.data(), title_heading.size(), padding::middle);
      p += pad;
    }
    title_begin_ = p;
    p += elems_per_row_ * pad;
    p = strcontcpy(p, title_row_args_.style.disabler_str().data());
    p = strcontcpy(p, divider_style_type::enabler_str().data());
    *p++ = divider_vertical;
    p = strcontcpy(p, divider_style_type::disabler_str().data());
    *p = '\0';

    // prepare prefix and suffix for element rows
    prepare_elem_row_(std::make_index_sequence<num_elem_row_args>{});
  }

private:
  template<size_type ...I>
  void prepare_elem_row_(std::index_sequence<I...>) {
    (prepare_elem_row_<I>(), ...);
  }

  template<size_type I>
  void prepare_elem_row_() {
    auto &args = std::get<I>(elem_row_args_);
    char *p = std::get<I>(elem_texts_);
    troll::pad(p, sizeof std::get<I>(elem_texts_), "", 0, padding::left);
    p = strcontcpy(p, divider_style_type::enabler_str().data());
    *p++ = divider_vertical;
    p = strcontcpy(p, divider_style_type::disabler_str().data());
    p = strcontcpy(p, args.style.enabler_str().data());
    if (has_heading_) {
      auto heading = sformat<pad>("{}", args.heading);
      troll::pad(p, pad, heading.data(), heading.size(), padding::middle);
      p += pad;
    }
    elem_begins_[I] = p;
    p += elems_per_row_ * pad;
    p = strcontcpy(p, args.style.disabler_str().data());
    p = strcontcpy(p, divider_style_type::enabler_str().data());
    *p++ = divider_vertical;
    p = strcontcpy(p, divider_style_type::disabler_str().data());
    *p = '\0';
  }

  public:
    // single use iterator
    class iterator {
    public:
      using difference_type = size_t;  // never
      using value_type = ::etl::string_view;
      using pointer = const value_type *;
      using reference = const value_type &;
      using iterator_category = std::input_iterator_tag;

      constexpr bool operator==(const iterator &rhs) const {
        return that_ == rhs.that_;
      }

      constexpr bool operator!=(const iterator &rhs) const {
          return !(*this == rhs);
      }

    private:
      // helper functions used to retrieve tuple elements dynamically
      template<size_type I = 0>
      [[noreturn]] constexpr std::enable_if_t<I == num_elem_row_args, value_type> get_elem_text_(size_type) const {
        __builtin_unreachable();
      }

      template<size_type I = 0>
      constexpr std::enable_if_t<I < num_elem_row_args, value_type> get_elem_text_(size_type idx) const {
        if (idx == 0) {
          return std::get<I>(that_->elem_texts_);
        }
        return get_elem_text_<I + 1>(idx - 1);
      }

    public:
      constexpr value_type operator*() const {
        switch (that_->state_) {
          case state::top_line:
          case state::middle_line:
            return that_->divider_text_;
          case state::title_line:
            return that_->title_text_;
          case state::elem_line:
            return get_elem_text_(that_->state_which_elem_);
          default:
            __builtin_unreachable();
        }
      }

      constexpr iterator &operator++() {
        if (that_->state_ == state::top_line) {
          char buf[pad + 1];
          // write down the titles
          size_type titles = 0;
          auto &ta = that_->title_row_args_;
          for (; ta.begin != ta.end && titles < that_->elems_per_row_; ++ta.begin, ++titles) {
            auto sz = snformat(buf, pad + 1, "{}", *ta.begin);
            troll::pad(that_->title_begin_ + titles * that_->pad, that_->pad, buf, sz, padding::middle);
          }
          // in case row is not full
          troll::pad(that_->title_begin_ + titles * that_->pad, that_->elems_per_row_ * that_->pad - titles * that_->pad, "", 0, padding::left);

          if (titles == 0) {
            // no more
            that_ = nullptr;
            return *this;
          }

          // write down the elements
          do_elem_row_(std::make_index_sequence<num_elem_row_args>{}, titles);
        }
        // advance state
        switch (that_->state_) {
          case state::top_line: that_->state_ = state::title_line; break;
          case state::title_line: that_->state_ = state::middle_line; break;
          case state::middle_line: that_->state_ = state::elem_line; break;
          case state::elem_line:
            if (that_->state_which_elem_ == num_elem_row_args - 1) {
              that_->state_which_elem_ = 0;
              that_->state_ = state::top_line;
            } else {
              ++that_->state_which_elem_;
              that_->state_ = state::middle_line;
            }
          default: break;
        }
        return *this;
      }

    private:
      template<size_type ...I>
      void do_elem_row_(std::index_sequence<I...>, size_type titles) {
        (do_elem_row_<I>(titles), ...);
      }

      template<size_type I>
      void do_elem_row_(size_type titles) {
        char buf[pad + 1];
        auto &args = std::get<I>(that_->elem_row_args_);
        char *p = that_->elem_begins_[I];
        for (size_type elems = 0; elems < titles; ++args.begin, ++elems) {
          auto sz = snformat(buf, pad + 1, "{}", *args.begin);
          troll::pad(p + elems * that_->pad, that_->pad, buf, sz, padding::middle);
        }
        // in case row is not full
        troll::pad(p + titles * that_->pad, that_->elems_per_row_ * that_->pad - titles * that_->pad, "", 0, padding::left);
      }

      friend class tabulate;
      iterator(tabulate *tab) : that_{tab} {}
      iterator(const iterator &) = delete;
      iterator &operator=(const iterator &) = delete;
      tabulate *that_;
    };

  constexpr iterator begin() {
    return iterator{this};
  }

  constexpr iterator end() {
    return iterator{nullptr};
  }

  private:
    friend class iterator;

    bool has_heading_ = false;
    size_type elems_per_row_;
    title_row_args_type title_row_args_;
    elem_row_args_type elem_row_args_;
    static constexpr auto divider_wrapper_size_ = divider_style_type::wrapper_str_size;
    char divider_text_[max_line_width + divider_wrapper_size_];
    char title_text_[max_line_width + title_row_args_type::style_type::wrapper_str_size + divider_wrapper_size_ * 2];
    char *title_begin_ = 0;
    std::tuple<char[max_line_width + ElemRowArgs::style_type::wrapper_str_size + divider_wrapper_size_ * 2]...> elem_texts_;
    char *elem_begins_[num_elem_row_args];

    enum class state : char {
      top_line = 0,
      title_line,
      middle_line,
      elem_line,
      end,
    } state_ = state::top_line;
    size_type state_which_elem_ = 0;
  };

  /**
   * make a tabulator. refer to tests for usage.
   */
  template<size_t MaxLineWidth, size_t Pad, class DividerStyle, class TitleRowArgs, class ...ElemRowArgs>
  constexpr auto make_tabulate(size_t elems_per_row, DividerStyle, TitleRowArgs &&title, ElemRowArgs &&...elems) {
    return tabulate<MaxLineWidth, Pad, DividerStyle, TitleRowArgs, ElemRowArgs...>{
      elems_per_row, std::forward<TitleRowArgs>(title), std::forward<ElemRowArgs>(elems)...
    };
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
