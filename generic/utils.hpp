#pragma once

#include <cstdint>
#include <utility>
#include <iterator>
#include <etl/queue.h>
#include <etl/string.h>
#include "../kern/user_syscall_typed.hpp"

namespace utils {

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

  /**
   * standard-layout string type for message passing.
   */
  template<size_t N>
  struct sd_buffer {
    constexpr sd_buffer() {
      data[0] = '\0';
    }

    constexpr sd_buffer(etl::string_view str) {
      auto sz = str.size();
      memcpy(data, str.data(), sz);
      data[sz] = '\0';
    }

    template<size_t M>
    constexpr sd_buffer(etl::string<M> const &str) {
      auto sz = str.size();
      memcpy(data, str.data(), sz);
      data[sz] = '\0';
    }

    constexpr sd_buffer(const char *str) {
      for (size_t i = 0; i < N; ++i) {
        data[i] = str[i];
        if (str[i] == '\0') {
          break;
        }
      }
    }

    char &operator[](size_t i) {
      return data[i];
    }

    operator etl::string_view() const {
      return etl::string_view(data);
    }

    template<size_t M>
    operator etl::string<M>() const {
      return etl::string<M>(data);
    }
    char data[N];
  };

  template<size_t N>
  sd_buffer(etl::string<N>) -> sd_buffer<N>;

  /**
   * a shorthand to create a courier task.
   */
  template<auto Header, size_t MaxValueSize = 128, size_t MaxQueueSize = 30>
  class courier_runner {
  public:
    static constexpr auto header = Header;
    static constexpr auto max_value_size = MaxValueSize;
    static constexpr auto max_queue_size = MaxQueueSize;

    courier_runner(priority_t priority, etl::string_view dest) {
      tid_ = Create(priority, &subtask_run_this_function_);
      SendValue(tid_, *dest.data(), dest.size(), null_reply);
    }

    void try_reply() {
      if (!ready_ || queue_.empty()) {
        return;
      }
      auto &front = queue_.front();
      ReplyValue(tid_, front.data, front.len);
      queue_.pop();
      ready_ = false;
    }

    template<class T>
    void push(const T &data, size_t len = sizeof(T)) {
      queue_.emplace();
      queue_.back().len = len;
      memcpy(queue_.back().data, &data, len);
    }

    void make_ready() {
      ready_ = true;
    }

  private:
    static void subtask_run_this_function_() {
      char buf[max_value_size];
      tid_t parent_tid;
      // receive my destination
      auto len = ReceiveValue(parent_tid, *buf);
      ReplyValue(parent_tid, null_reply);
      buf[len] = '\0';
      auto dest = etl::string<40>(buf);
      auto target = TaskFinder(dest.data());

      while (true) {
        auto len = SendValue(parent_tid, Header, buf);
        SendValue(target(), buf, len, null_reply);
      }
    }

    tid_t tid_ {};
    bool ready_ {};
    struct elem_t {
      char data[max_value_size];
      size_t len;
    };
    etl::queue<elem_t, max_queue_size> queue_;
  };
}

namespace troll {
  /**
   * moving average of infinite count of values (for floating/fixed point types)
   */
  template<class T>
  class pseudo_moving_average {
  public:
    constexpr void add(const T val) noexcept {
      a_ += (val - a_) / ++n_;
    }

    constexpr T value() const noexcept {
      return a_;
    }
  private:
    T a_ {};
    size_t n_ {};
  };
}  // namespace troll
