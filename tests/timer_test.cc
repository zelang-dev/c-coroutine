#include <gtest/gtest.h>

#include "uvco/loop/loop.h"
#include "uvco/promise/multipromise.h"
#include "uvco/promise/promise.h"
#include "uvco/timer.h"

#include "test_util.h"

#include <cstdint>
#include <optional>

namespace {

using namespace uvco;

TEST(TimerTest, simpleWait) {
  bool ran = false;
  auto setup = [&](const Loop &loop) -> Promise<void> {
    co_await sleep(loop, 1);
    ran = true;
  };

  run_loop(setup);
  EXPECT_TRUE(ran);
}

TEST(TimerTest, tickerTest) {
  constexpr static uint64_t count = 3;
  uint64_t counter = 0;
  auto setup = [&](const Loop &loop) -> Promise<void> {
    auto ticker = tick(loop, 1, count);
    MultiPromise<uint64_t> tickerProm = ticker->ticker();

    while (true) {
      std::optional<uint64_t> got = co_await tickerProm;
      if (got) {
        EXPECT_EQ(*got, counter);
        ++counter;
      } else {
        break;
      }
    }
    co_await ticker->close();
  };

  run_loop(setup);
  EXPECT_EQ(counter, count);
}

TEST(TimerTest, infiniteTickerTest) {
  constexpr static uint64_t count = 3;
  uint64_t counter = 0;
  auto setup = [&](const Loop &loop) -> Promise<void> {
    auto ticker = tick(loop, 1, 0);
    MultiPromise<uint64_t> tickerProm = ticker->ticker();
    for (counter = 0; counter < count; ++counter) {
      EXPECT_EQ(counter, *(co_await tickerProm));
    }
    co_await ticker->close();
  };

  run_loop(setup);
  EXPECT_EQ(counter, count);
}

TEST(TimerTest, finiteTickerTest) {
  constexpr static uint64_t stopAfter = 3;
  uint64_t counter = 0;
  auto setup = [&](const Loop &loop) -> Promise<void> {
    auto ticker = tick(loop, 1, stopAfter);
    MultiPromise<uint64_t> tickerProm = ticker->ticker();
    for (counter = 0; counter < stopAfter; ++counter) {
      EXPECT_EQ(counter, *(co_await tickerProm));
    }
    co_await ticker->close();
  };

  run_loop(setup);
  EXPECT_EQ(counter, stopAfter);
}

} // namespace
