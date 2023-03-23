#pragma once

#include <etl/to_string.h>
#include <etl/queue.h>
#include <etl/optional.h>

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
        } else if constexpr (std::is_same_v<Decay, char>) {
          // print char instead of number
          s.assign(1, a0);
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

  using static_ansi_style_options_none_t = static_ansi_style_options<>;
  static constexpr static_ansi_style_options_none_t static_ansi_style_options_none{};

  template<class Heading, class TitleIt, class HeadingStyle, class TitleStyle>
  struct tabulate_title_row_args {
    using heading_type = Heading;
    using title_it_type = TitleIt;
    using heading_style_type = HeadingStyle;
    using title_style_type = TitleStyle;
    static constexpr bool style_is_same = std::is_same_v<HeadingStyle, TitleStyle>;
    // the heading (first column) used for the title row
    Heading heading;
    // title range
    TitleIt begin, end;

    // apply different styles on heading (left) and real titles
    template<class Hding, class It1, class It2>
    constexpr tabulate_title_row_args(Hding &&heading, It1 &&begin, It2 &&end, HeadingStyle, TitleStyle)
      : heading{std::forward<Hding>(heading)}, begin{std::forward<It1>(begin)}, end{std::forward<It2>(end)} {}

    // apply the same style on heading (left) and real titles
    template<class Hding, class It1, class It2>
    constexpr tabulate_title_row_args(Hding &&heading, It1 &&begin, It2 &&end, TitleStyle)
      : heading{std::forward<Hding>(heading)}, begin{std::forward<It1>(begin)}, end{std::forward<It2>(end)} {}

    // no heading
    template<class It1, class It2>
    constexpr tabulate_title_row_args(It1 &&begin, It2 &&end, TitleStyle)
      : heading{""}, begin{std::forward<It1>(begin)}, end{std::forward<It2>(end)} {}
  };

  template<class Heading, class TitleIt, class HeadingStyle, class TitleStyle>
  tabulate_title_row_args(Heading, TitleIt, TitleIt, HeadingStyle, TitleStyle) -> tabulate_title_row_args<Heading, TitleIt, HeadingStyle, TitleStyle>;

  template<class Heading, class TitleIt, class TitleStyle>
  tabulate_title_row_args(Heading, TitleIt, TitleIt, TitleStyle) -> tabulate_title_row_args<Heading, TitleIt, TitleStyle, TitleStyle>;

  template<class TitleIt, class TitleStyle>
  tabulate_title_row_args(TitleIt, TitleIt, TitleStyle) -> tabulate_title_row_args<const char *, TitleIt, TitleStyle, TitleStyle>;

  template<class Heading, class ElemIt, class HeadingStyle, class ElemStyle>
  struct tabulate_elem_row_args {
    using heading_type = Heading;
    using elem_it_type = ElemIt;
    using heading_style_type = HeadingStyle;
    using elem_style_type = ElemStyle;
    static constexpr bool style_is_same = std::is_same_v<HeadingStyle, ElemStyle>;
    // the heading (first column) used for the element row
    Heading heading;
    // element range
    ElemIt begin;

    // apply different styles on heading (left) and real elements
    template<class Hding, class It>
    constexpr tabulate_elem_row_args(Hding &&heading, It &&begin, HeadingStyle, ElemStyle)
      : heading{std::forward<Hding>(heading)}, begin{std::forward<It>(begin)} {}

    // apply the same style on heading (left) and real elements
    template<class Hding, class It>
    constexpr tabulate_elem_row_args(Hding &&heading, It &&begin, ElemStyle)
      : heading{std::forward<Hding>(heading)}, begin{std::forward<It>(begin)} {}

    // no heading
    template<class It>
    constexpr tabulate_elem_row_args(It &&begin, ElemStyle)
      : heading{""}, begin{std::forward<It>(begin)} {}
  };

  template<class Heading, class ElemIt, class HeadingStyle, class ElemStyle>
  tabulate_elem_row_args(Heading, ElemIt, HeadingStyle, ElemStyle) -> tabulate_elem_row_args<Heading, ElemIt, HeadingStyle, ElemStyle>;

  template<class Heading, class ElemIt, class ElemStyle>
  tabulate_elem_row_args(Heading, ElemIt, ElemStyle) -> tabulate_elem_row_args<Heading, ElemIt, ElemStyle, ElemStyle>;

  template<class ElemIt, class ElemStyle>
  tabulate_elem_row_args(ElemIt, ElemStyle) -> tabulate_elem_row_args<const char *, ElemIt, ElemStyle, ElemStyle>;

  /**
   * helper class to tabulate text. it outputs text line by line.
   */
  template<size_t ElemsPerRow, size_t HeadingPadding, size_t ContentPadding, class DividerStyle, class TitleRowArgs, class ...ElemRowArgs>
  class tabulate {
  public:
    using size_type = size_t;
    using divider_style_type = DividerStyle;
    using title_row_args_type = TitleRowArgs;
    using elem_row_args_type = std::tuple<ElemRowArgs...>;
    static constexpr size_type num_elem_row_args = sizeof...(ElemRowArgs);

    static constexpr size_type elems_per_row = ElemsPerRow;
    static constexpr size_type heading_padding = HeadingPadding;
    static constexpr size_type content_padding = ContentPadding;
    // calculated based on the number of elements per row and formatting, excluding escapes.
    static constexpr size_type max_line_width = HeadingPadding + ElemsPerRow * ContentPadding + 10/*safety*/;

    char divider_horizontal = '-';
    char divider_vertical = '|';
    char divider_cross = '+';

    template<class Tit, class ...Elems>
    constexpr tabulate(Tit &&title, Elems &&...elems)
      : title_row_args_{std::forward<Tit>(title)}
      , elem_row_args_{std::forward<Elems>(elems)...}
    {
      auto title_heading = sformat<heading_padding>("{}", title_row_args_.heading);
      has_heading_ = title_heading.size();
      auto total_pad = elems_per_row * content_padding + (has_heading_ ? heading_padding : 0);
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

      if constexpr (title_row_args_type::style_is_same) {
        p = strcontcpy(p, title_row_args_type::title_style_type::enabler_str().data());
        if (has_heading_) {
          troll::pad(p, heading_padding, title_heading.data(), title_heading.size(), padding::middle);
          p += heading_padding;
        }
      } else {
        if (has_heading_) {
          p = strcontcpy(p, title_row_args_type::heading_style_type::enabler_str().data());
          troll::pad(p, heading_padding, title_heading.data(), title_heading.size(), padding::middle);
          p += heading_padding;
          p = strcontcpy(p, title_row_args_type::heading_style_type::disabler_str().data());
        }
        p = strcontcpy(p, title_row_args_type::title_style_type::enabler_str().data());
      }
      title_begin_ = p;
      p += elems_per_row * content_padding;
      p = strcontcpy(p, title_row_args_type::title_style_type::disabler_str().data());
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
      using ArgT = std::decay_t<decltype(args)>;
      char *p = std::get<I>(elem_texts_);
      troll::pad(p, sizeof std::get<I>(elem_texts_), "", 0, padding::left);
      p = strcontcpy(p, divider_style_type::enabler_str().data());
      *p++ = divider_vertical;
      p = strcontcpy(p, divider_style_type::disabler_str().data());
      auto heading = sformat<heading_padding>("{}", args.heading);

      if constexpr (ArgT::style_is_same) {
        p = strcontcpy(p, ArgT::elem_style_type::enabler_str().data());
        if (has_heading_) {
          troll::pad(p, heading_padding, heading.data(), heading.size(), padding::middle);
          p += heading_padding;
        }
      } else {
        if (has_heading_) {
          p = strcontcpy(p, ArgT::heading_style_type::enabler_str().data());
          troll::pad(p, heading_padding, heading.data(), heading.size(), padding::middle);
          p += heading_padding;
          p = strcontcpy(p, ArgT::heading_style_type::disabler_str().data());
        }
        p = strcontcpy(p, ArgT::elem_style_type::enabler_str().data());
      }
      elem_begins_[I] = p;
      p += elems_per_row * content_padding;
      p = strcontcpy(p, ArgT::elem_style_type::disabler_str().data());
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
        // put the most likely to trail first
        return state_ == rhs.state_ &&
          that_ == rhs.that_ &&
          title_it_ == rhs.title_it_ &&
          state_which_elem_ == rhs.state_which_elem_;
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
        switch (state_) {
          case state::top_line:
          case state::middle_line:
            return that_->divider_text_;
          case state::title_line:
            return that_->title_text_;
          case state::elem_line:
            return get_elem_text_(state_which_elem_);
          default:
            __builtin_unreachable();
        }
      }

      constexpr iterator &operator++() {
        if (state_ == state::top_line) {
          char buf[content_padding + 1];
          // write down the titles
          size_type titles = 0;
          auto &end = that_->title_row_args_.end;
          for (; title_it_ != end && titles < elems_per_row; ++title_it_, ++titles) {
            auto sz = snformat(buf, content_padding + 1, "{}", *title_it_);
            troll::pad(that_->title_begin_ + titles * content_padding, content_padding, buf, sz, padding::middle);
          }
          // in case row is not full
          troll::pad(that_->title_begin_ + titles * content_padding, elems_per_row * content_padding - titles * content_padding, "", 0, padding::left);

          if (titles == 0) {
            // no more
            state_ = state::end;
            state_which_elem_ = 0;
            return *this;
          }

          // write down the elements
          do_elem_row_(std::make_index_sequence<num_elem_row_args>{}, titles);
        }
        // advance state
        switch (state_) {
          case state::top_line: state_ = state::title_line; break;
          case state::title_line:
            state_ = num_elem_row_args ? state::middle_line : state::top_line;
            break;
          case state::middle_line: state_ = state::elem_line; break;
          case state::elem_line:
            if (state_which_elem_ == num_elem_row_args - 1) {
              state_which_elem_ = 0;
              state_ = state::top_line;
            } else {
              ++state_which_elem_;
              state_ = state::middle_line;
            }
          default: break;
        }
        return *this;
      }

    private:
      template<size_type ...I>
      void do_elem_row_(std::index_sequence<I...>, size_type titles) {
        (void)titles;  // suppress unused warning if there is no element rows
        (do_elem_row_<I>(titles), ...);
      }

      template<size_type I>
      void do_elem_row_(size_type titles) {
        char buf[content_padding + 1];
        auto &it = std::get<I>(*elem_its_);
        char *p = that_->elem_begins_[I];
        for (size_type elems = 0; elems < titles; ++it, ++elems) {
          auto sz = snformat(buf, content_padding + 1, "{}", *it);
          troll::pad(p + elems * content_padding, content_padding, buf, sz, padding::middle);
        }
        // in case row is not full
        troll::pad(p + titles * content_padding, elems_per_row * content_padding - titles * content_padding, "", 0, padding::left);
      }

      friend class tabulate;

      tabulate *that_;
      typename title_row_args_type::title_it_type title_it_;

      ::etl::optional<std::tuple<typename ElemRowArgs::elem_it_type...>> elem_its_;

      enum class state : char {
        top_line = 0,
        title_line,
        middle_line,
        elem_line,
        end,
      } state_;
      size_type state_which_elem_ = 0;

      template<class T, class E>
      iterator(tabulate *tab, T &&title_begin, E &&elem_begins, state s = state::top_line)
        : that_{tab}, title_it_{std::forward<T>(title_begin)}, elem_its_{std::forward<E>(elem_begins)}, state_{s} {}
    };

    constexpr iterator begin() {
      return iterator{this, title_row_args_.begin, project_elem_its_(std::make_index_sequence<num_elem_row_args>{})};
    }

    constexpr iterator end() {
      return iterator{this, title_row_args_.end, ::etl::nullopt, iterator::state::end};
    }

    /**
     * reset this object to use a new range of data.
     */
    template<class Tit1, class Tit2, class ...Elems>
    constexpr void reset_src_iterator(Tit1 &&title_begin, Tit2 &&title_end, Elems &&...elem_begins)
    {
      title_row_args_.begin = std::forward<Tit1>(title_begin);
      title_row_args_.end = std::forward<Tit2>(title_end);
      reset_elem_begins_(std::make_index_sequence<num_elem_row_args>{}, std::forward<Elems>(elem_begins)...);
    }

    /**
     * returns col, row and a string to be used to patch a printed table assuming
     * value has changed. use only when a field in the table is supposed to be modified.
     * if elements are added or deleted from the src iterable, use reset_src_iterator() instead.
     * - ArgRow: ith row as provided to make_tabulate(), eg. 0 -> title row, 1 -> first element row
     * - it_index: index of the value in that element row
     * - v: value that changed
     */
    template<size_t ArgRow, class V>
    constexpr auto patch_str(size_t it_index, const V &v) {
      size_t col = 1 + (has_heading_ ? HeadingPadding : 0) + (it_index % elems_per_row) * ContentPadding;
      size_t skip_full_rows = it_index / elems_per_row;
      size_t row = (skip_full_rows * (1 + num_elem_row_args) + ArgRow) * 2 + 1;
      auto padded = pad<ContentPadding>(sformat<ContentPadding>("{}", v), padding::middle);

      if constexpr (ArgRow == 0) {
        using style = typename title_row_args_type::title_style_type;
        auto str = sformat<style::wrapper_str_size + ContentPadding>(
          "{}{}{}", style::enabler_str(), padded, style::disabler_str()
        );
        return std::make_tuple(row, col, str);
      } else {
        using style = typename std::tuple_element_t<ArgRow - 1, elem_row_args_type>::elem_style_type;
        auto str = sformat<style::wrapper_str_size + ContentPadding>(
          "{}{}{}", style::enabler_str(), padded, style::disabler_str()
        );
        return std::make_tuple(row, col, str);
      }
    }

  private:
    template<size_type ...I, class ...Elems>
    constexpr void reset_elem_begins_(std::index_sequence<I...>, Elems &&...elem_begins) {
      ((void)(std::get<I>(elem_row_args_).begin = std::forward<Elems>(elem_begins)), ...);
    }

    template<size_type ...I>
    constexpr auto project_elem_its_(std::index_sequence<I...>) {
      return std::make_tuple(std::get<I>(elem_row_args_).begin...);
    }

    friend class iterator;

    bool has_heading_ = false;
    title_row_args_type title_row_args_;
    elem_row_args_type elem_row_args_;

    static constexpr auto divider_wrapper_size_ = divider_style_type::wrapper_str_size;
    char divider_text_[max_line_width + divider_wrapper_size_];

    char title_text_[  // precalculate buffer size
      max_line_width
      + title_row_args_type::heading_style_type::wrapper_str_size
      + (title_row_args_type::style_is_same ? 0 : title_row_args_type::title_style_type::wrapper_str_size)
      + divider_wrapper_size_ * 2
    ];
    char *title_begin_ = 0;

    std::tuple<char[  // precalculate buffer size
      max_line_width
      + ElemRowArgs::heading_style_type::wrapper_str_size
      + (ElemRowArgs::style_is_same ? 0 : ElemRowArgs::elem_style_type::wrapper_str_size)
      + divider_wrapper_size_ * 2
    ]...> elem_texts_;
    char *elem_begins_[num_elem_row_args ? num_elem_row_args : 1];
  };

  /**
   * make a tabulator.
   * template arguments:
   *   - ElemsPerRow: the number of elements in each row
   *   - HeadingPadding: the padding for the heading
   *   - ContentPadding: the padding for the content between each column
   *  arguments:
   *   - divider_style: troll::static_ansi_style
   *   - title_row_args: troll::tabulate_title_row_args
   *   - elem_row_args: troll::tabulate_elem_row_args
   */
  template<size_t ElemsPerRow, size_t HeadingPadding, size_t ContentPadding, class DividerStyle, class TitleRowArgs, class ...ElemRowArgs>
  constexpr auto make_tabulate(DividerStyle, TitleRowArgs &&title, ElemRowArgs &&...elems) {
    return tabulate<ElemsPerRow, HeadingPadding, ContentPadding, DividerStyle, TitleRowArgs, ElemRowArgs...>{
      std::forward<TitleRowArgs>(title), std::forward<ElemRowArgs>(elems)...
    };
  }

  /**
   * make a tabulator.
   * template arguments:
   *   - ElemsPerRow: the number of elements in each row
   *   - Padding: padding between columns
   *  arguments:
   *   - divider_style: troll::static_ansi_style
   *   - title_row_args: troll::tabulate_title_row_args
   *   - elem_row_args: troll::tabulate_elem_row_args
   */
  template<size_t ElemsPerRow, size_t Padding, class DividerStyle, class TitleRowArgs, class ...ElemRowArgs>
  constexpr auto make_tabulate(DividerStyle, TitleRowArgs &&title, ElemRowArgs &&...elems) {
    return tabulate<ElemsPerRow, Padding, Padding, DividerStyle, TitleRowArgs, ElemRowArgs...>{
      std::forward<TitleRowArgs>(title), std::forward<ElemRowArgs>(elems)...
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
    const char *content = *ref.text ? ref.text : "\033[K";
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
