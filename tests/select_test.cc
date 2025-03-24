#include <fmt/core.h>
#include <gtest/gtest.h>
#include <optional>

#include "test_util.h"
#include "uvco/loop/loop.h"
#include "uvco/promise/multipromise.h"
#include "uvco/promise/promise.h"
#include "uvco/promise/select.h"
#include "uvco/run.h"

using namespace uvco;

namespace {

TEST(SelectTest, selectBasic) {
  auto setup = [](const Loop &loop) -> uvco::Promise<void> {
    Promise<int> promise1 = []() -> uvco::Promise<int> {
      co_await yield();
      co_return 123;
    }();
    Promise<int> promise2 = []() -> uvco::Promise<int> {
      co_await yield();
      co_await yield();
      co_return 234;
    }();

    auto selectSet = SelectSet{promise1, promise2};
    auto selected = co_await selectSet;
    EXPECT_EQ(selected.size(), 1);
    EXPECT_EQ(co_await std::get<0>(selected[0]), 123);

    // Selecting an already finished promise is okay.
    auto selectSet2 = SelectSet{promise1, promise2};
    auto selected2 = co_await selectSet2;
    EXPECT_EQ(selected2.size(), 1);
    EXPECT_EQ(co_await std::get<1>(selected2[0]), 234);
  };

  run_loop(setup);
}

TEST(SelectTest, selectReturnsSimultaneously) {
  auto simultaneousSelect = [](const Loop &loop) -> Promise<void> {
    auto promise1 = []() -> uvco::Promise<int> {
      co_await yield();
      co_return 1;
    };
    auto promise2 = []() -> uvco::Promise<int> {
      co_await yield();
      co_return 2;
    };

    auto promiseObject1 = promise1();
    auto promiseObject2 = promise2();

    auto selectSet = SelectSet{promiseObject1, promiseObject2};
    auto selected = co_await selectSet;
    EXPECT_EQ(selected.size(), 2);
    EXPECT_EQ(co_await std::get<0>(selected[0]), 1);
    EXPECT_EQ(co_await std::get<1>(selected[1]), 2);
    co_return;
  };

  run_loop(simultaneousSelect);
}

TEST(SelectTest, selectSetMany) {
  auto firstPass = [](const Loop &loop) -> Promise<void> {
    auto promise1 = []() -> uvco::Promise<int> {
      co_await yield();
      co_return 1;
    };
    auto promise2 = []() -> uvco::Promise<int> {
      co_await yield();
      co_await yield();
      co_return 2;
    };
    auto promise3 = []() -> uvco::Promise<int> {
      co_await yield();
      co_await yield();
      co_return 3;
    };
    auto promise4 = []() -> uvco::Promise<int> {
      co_await yield();
      co_await yield();
      co_return 4;
    };

    auto promiseObject1 = promise1();
    auto promiseObject1a = promise1();
    auto promiseObject2 = promise2();
    auto promiseObject2a = promise2();
    auto promiseObject3 = promise3();
    auto promiseObject3a = promise3();
    auto promiseObject4 = promise4();
    auto promiseObject4a = promise4();

    auto selectSet = SelectSet{
        promiseObject1,  promiseObject2,  promiseObject3,  promiseObject4,
        promiseObject1a, promiseObject2a, promiseObject3a, promiseObject4a};
    const auto selected = co_await selectSet;
    EXPECT_EQ(selected.size(), 2);
    EXPECT_EQ(co_await std::get<0>(selected[0]), 1);
    EXPECT_EQ(co_await std::get<4>(selected[1]), 1);
    co_return;
  };

  run_loop(firstPass);
}

TEST(SelectTest, onlyCheckOne) {
  auto setup = [](const Loop &loop) -> uvco::Promise<void> {
    auto promise1 = []() -> uvco::Promise<void> {
      co_await yield();
      co_return;
    };
    auto promise2 = []() -> uvco::Promise<void> {
      co_await yield();
      co_await yield();
      co_await yield();
      co_return;
    };

    auto promiseObject1 = promise1();
    auto promiseObject2 = promise2();

    auto selectSet = SelectSet{promiseObject1, promiseObject2};
    auto selected = co_await selectSet;
    EXPECT_EQ(selected.size(), 1);
    co_await std::get<0>(selected[0]);
    // The second promise is not checked; it is finished anyway after co_return,
    // in order to free the loop.
    co_return;
  };

  run_loop(setup);
}

TEST(SelectTest, selectVoid) {
  auto setup = [](const Loop &loop) -> uvco::Promise<void> {
    auto promise1 = []() -> uvco::Promise<void> { co_await yield(); };
    auto promise2 = []() -> uvco::Promise<void> {
      co_await yield();
      co_await yield();
    };

    auto promiseObject1 = promise1();
    auto promiseObject2 = promise2();

    auto selectSet = SelectSet{promiseObject1, promiseObject2};
    auto selected = co_await selectSet;
    EXPECT_EQ(selected.size(), 1);
    co_await std::get<0>(selected[0]);
    co_return;
  };

  run_loop(setup);
}

TEST(SelectTest, DISABLED_benchmark) {
  auto setup = [](const Loop &loop) -> uvco::Promise<void> {
    constexpr unsigned count = 1000000;

    // Finely orchestrated generators, yielding one after another.
    MultiPromise<unsigned> gen1 = []() -> uvco::MultiPromise<unsigned> {
      for (unsigned i = 0; i < count; ++i) {
        co_yield i;
        co_await yield();
      }
    }();
    MultiPromise<unsigned> gen2 = []() -> uvco::MultiPromise<unsigned> {
      for (unsigned i = 0; i < count; ++i) {
        co_await yield();
        co_yield i;
        co_await yield();
      }
    }();

    // Each iteration takes about 500 ns on my unsignedel Core i5-7300U @ 2.6 GHz
    // (your machine is likely faster).
    for (unsigned i = 0; i < count; ++i) {
      Promise<std::optional<unsigned>> promise1 = gen1.next();
      Promise<std::optional<unsigned>> promise2 = gen2.next();

      // Ensure that all promises are awaited .Imagine the next() coroutines as
      // "background threads"; the uvco rules say that only one coroutine may
      // await a promise at a time, so we can't issue two calls to
      // `MultiPromise::next()` right after another.
      auto result1 = co_await SelectSet{promise1, promise2};
      EXPECT_EQ(result1.size(), 1);
      EXPECT_EQ(co_await std::get<0>(result1[0]), i);
      auto result2 = co_await SelectSet{promise1, promise2};
      EXPECT_EQ(result2.size(), 1);
      EXPECT_EQ(co_await std::get<1>(result2[0]), i);
    }

    co_return;
  };

  run_loop(setup);
}

TEST(SelectTest, reliableSelectLoop) {
  auto setup = [](const Loop &loop) -> uvco::Promise<void> {
    constexpr unsigned count = 10;

    // Finely orchestrated generators, yielding one after another.
    MultiPromise<unsigned> gen1 = []() -> uvco::MultiPromise<unsigned> {
      for (unsigned i = 0; i < count; ++i) {
        co_yield i;
        co_await yield();
      }
    }();
    MultiPromise<unsigned> gen2 = []() -> uvco::MultiPromise<unsigned> {
      for (unsigned i = 0; i < count; ++i) {
        co_await yield();
        co_await yield();
        co_yield i;
      }
    }();

    Promise<std::optional<unsigned>> promise1 = gen1.next();
    Promise<std::optional<unsigned>> promise2 = gen2.next();
    bool promise1Done = false;
    bool promise2Done = false;

    // On my Core i5-7300U, this takes about 600 ns per iteration with three
    // yields per two items. The baseline - no yield() calls - is about 270 ns
    // per iteration.
    while (!promise1Done || !promise2Done) {
      auto result = co_await SelectSet{promise1, promise2};
      for (auto &promise : result) {
        switch (promise.index()) {
        case 0:
          if (co_await std::get<0>(promise) == std::nullopt) {
            promise1Done = true;
          } else {
            promise1 = gen1.next();
          }
          break;
        case 1:
          if (co_await std::get<1>(promise) == std::nullopt) {
            promise2Done = true;
          } else {
            promise2 = gen2.next();
          }
          break;
        default:
          EXPECT_FALSE(true);
          co_return;
        }
      }
    }

    EXPECT_FALSE((co_await gen1.next()).has_value());
    EXPECT_FALSE((co_await gen2.next()).has_value());
    EXPECT_FALSE((co_await gen1).has_value());
    EXPECT_FALSE((co_await gen2).has_value());

    co_return;
  };

  run_loop(setup);
}

} // namespace
