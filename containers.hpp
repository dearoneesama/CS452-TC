#pragma once

#include <etl/intrusive_queue.h>

static_assert(sizeof(char) == 1UL);

namespace troll {
  using forward_link = etl::forward_link<0>;

  /**
   * a free list preallocates slabs of memory to hold objects.
   * T must be derived from Link. when an object is unallocated (in
   * this list), the pointer is used for internal linking; when it is allocated,
   * the pointer is invalid and free to use in other intrusive containers.
  */
  template<class T, size_t Capacity, class Link = forward_link>
  class free_list {
  public:
    using value_type = T;
    using size_type = size_t;
    using pointer = value_type *;
    using const_pointer = const value_type *;
    using reference = value_type &;
    using const_reference = const value_type &;
    using link_type = Link;

    static constexpr auto capacity = Capacity;

    constexpr free_list() {
      for (size_t i = 0; i < capacity; ++i) {
        auto space = reinterpret_cast<pointer>(buffer + i * sizeof(T));
        queue.push(*space);
      }
    }

    free_list(free_list &) = delete;
    free_list(free_list &&) = delete;

    /**
     * allocate memory from slabs for the object type, and construct the object
     * using args.
    */
    template<class ...Args>
    pointer allocate(Args &&...args) {
      if (num_allocated() == capacity) {
        __builtin_unreachable();
      }
      pointer space = &queue.front();
      queue.pop();
      space = new (space) T(std::forward<Args>(args)...);
    }

    /**
     * deallocate this object.
    */
    void free(link_type *ptr) {
      if (!in_my_buffer(ptr)) {
        return;
      }
      static_cast<pointer>(ptr)->~T();
      queue.push(*ptr);
    }

    bool in_my_buffer(void *ptr) noexcept {
      return ptr >= buffer && ptr < buffer + capacity * sizeof(T);
    }

    /**
     * relative index (unique) of the object in the buffer.
    */
    size_type index_of(link_type *ptr) noexcept {
      return (reinterpret_cast<uintptr_t>(ptr) - reinterpret_cast<uintptr_t>(buffer)) / sizeof(T);
    }

    size_type num_allocated() const {
      return capacity - queue.size();
    }

  private:
    char buffer[capacity * sizeof(T)];
    etl::intrusive_queue<T, Link> queue;
  };

  /**
   * a priority+round robin based priority queue with NPriorities priorities.
   * T must be derived from Link. When object is popped, the link pointer
   * is invalid and free to use.
   * 
   * higher priority => popped early. priority starts from 0 (lowest).
  */
  template<class T, size_t NPriorities, class Link = forward_link>
  class intrusive_priority_scheduling_queue {
  public:
    using value_type = T;
    using size_type = size_t;
    using pointer = value_type *;
    using const_pointer = const value_type *;
    using reference = value_type &;
    using const_reference = const value_type &;
    using link_type = Link;
    using priority_type = int;

    struct tuple {
      reference value;
      priority_type priority;
    };

    static constexpr auto num_priorities = NPriorities;

    constexpr intrusive_priority_scheduling_queue() = default;
    intrusive_priority_scheduling_queue(intrusive_priority_scheduling_queue &) = delete;

    size_type size() const {
      return size_;
    }

    /**
     * adds an element with priority. it is placed at the back of its queue.
    */
    void push(link_type &value, priority_type priority) {
      if (priority < 0 || static_cast<size_type>(priority) >= num_priorities) {
        __builtin_unreachable();
      }
      queues[priority].push(value);
      ++size_;
    }

    /**
     * gets the front element and its priority according to priority. if there exists
     * multiple elements with same priority, then fifo order.
    */
    tuple front_tuple() {
      if (!size_) {
        __builtin_unreachable();
      }
      for (size_t i = 0; i < num_priorities; ++i) {
        priority_type p = num_priorities - 1 - i;
        if (!queues[p].empty()) {
          reference item = queues[p].front();
          return {item, p};
        }
      }
      __builtin_unreachable();
    }

    reference front() {
      return front_tuple().value;
    }

    reference pop() {
      auto &&[value, p] = front_tuple();
      queues[p].pop();
      --size_;
      return value;
    }

  private:
    etl::intrusive_queue<T, Link> queues[num_priorities];
    size_type size_ = 0;
  };
}
