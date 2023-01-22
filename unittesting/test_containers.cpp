#include <catch_amalgamated.hpp>
#include "../containers.hpp"

struct test_elem : public troll::forward_link {
  alignas(16) char mark;
  test_elem(char mark_) : mark{mark_} {}
};

TEST_CASE("free list usage", "[containers]") {
  troll::free_list<test_elem, 5> fl;
  REQUIRE(fl.capacity == 5);
  REQUIRE(fl.num_allocated() == 0);

  SECTION("full alloc and free first") {
    auto *a = fl.allocate('a'),
         *b = fl.allocate('b'),
         *c = fl.allocate('c');

    REQUIRE(fl.num_allocated() == 3);
    REQUIRE(a->mark == 'a');
    REQUIRE(b->mark == 'b');
    REQUIRE(c->mark == 'c');

    auto *d = fl.allocate('d'),
         *e = fl.allocate('e');

    REQUIRE(d->mark == 'd');
    REQUIRE(e->mark == 'e');

    REQUIRE(fl.in_my_buffer(a));
    REQUIRE(fl.in_my_buffer(b));
    REQUIRE(fl.in_my_buffer(e));
    char outside;
    REQUIRE(!fl.in_my_buffer(&outside));

    fl.free(a);
    REQUIRE(fl.num_allocated() == 4);
    fl.free(b);
    auto *f = fl.allocate('f'),
         *g = fl.allocate('g');

    REQUIRE(f->mark == 'f');
    REQUIRE(g->mark == 'g');
    // reuses previous mem
    REQUIRE(a->mark == 'f');
    REQUIRE(b->mark == 'g');
  }

  SECTION("alloc full and shrink to zero do not crash") {
    test_elem *elems[5];
    for (int i = 0; i < 5; ++i) {
      elems[i] = fl.allocate(i);
    }
    for (int i = 0; i < 5; ++i) {
      fl.free(elems[i]);
    }
    REQUIRE(fl.num_allocated() == 0);
    REQUIRE(fl.allocate('x')->mark == 'x');
  }

  SECTION("many allocs and frees") {
    REQUIRE(fl.num_allocated() == 0);
    auto *a = fl.allocate('a'),
         &b = *fl.allocate('b'),
         *c = fl.allocate('c');
    fl.free(&b);
    auto *d = fl.allocate('d'),
         *e = fl.allocate('e');

    REQUIRE(d->mark == 'd');
    REQUIRE(e->mark == 'e');

    fl.free(e);
    auto *f = fl.allocate('f');
    fl.free(c);
    REQUIRE(f->mark == 'f');
    auto *g = fl.allocate('g');
    REQUIRE(g->mark == 'g');

    fl.free(a);
    fl.free(g);
    auto *h = fl.allocate('h');

    REQUIRE(d->mark == 'd');
    REQUIRE(f->mark == 'f');
    REQUIRE(h->mark == 'h');
    REQUIRE(fl.num_allocated() == 3);
  }
}

TEST_CASE("scheduling queue", "[containers]") {
  troll::intrusive_priority_scheduling_queue<test_elem, 4> q;
  REQUIRE(q.size() == 0);
  test_elem data[] = {'0', '1', '2', '3', '4', '5', '6', '7'};

  SECTION("by priority") {
    q.push(data[0], 0);
    q.push(data[4], 1);
    q.push(data[6], 3);
    q.push(data[2], 2);
    // compare pointers
    REQUIRE(&q.pop() == data + 6);
    REQUIRE(&q.pop() == data + 2);
    REQUIRE(&q.pop() == data + 4);
    q.push(data[5], 3);
    REQUIRE(&q.pop() == data + 5);
    REQUIRE(&q.pop() == data + 0);
    REQUIRE(q.size() == 0);
  }

  SECTION("round robin on same priority") {
    q.push(data[0], 0); q.push(data[1], 0);
    q.push(data[2], 1); q.push(data[3], 1); q.push(data[4], 1);
    q.push(data[5], 3); q.push(data[6], 3); q.push(data[7], 3);

    test_elem *top;
    REQUIRE((top = &q.pop()) == data + 5);
    q.push(*top, 3);
    REQUIRE((top = &q.pop()) == data + 6);
    q.push(*top, 3);
    REQUIRE((top = &q.pop()) == data + 7);
    q.push(*top, 3);
    REQUIRE(&q.pop() == data + 5);
    REQUIRE(&q.pop() == data + 6);
    REQUIRE(&q.pop() == data + 7);

    REQUIRE(&q.pop() == data + 2);
    REQUIRE(&q.pop() == data + 3);
  }
}

