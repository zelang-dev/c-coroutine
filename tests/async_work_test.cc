
#include <fcntl.h>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <gtest/gtest.h>
#include <uv.h>

#include "test_util.h"
#include "uvco/async_work.h"
#include "uvco/loop/loop.h"
#include "uvco/promise/promise.h"

#include <atomic>
#include <functional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

using namespace uvco;

TEST(AsyncWorkTest, basicVoidFunction) {
  bool ran = false;
  auto work = [&]() -> void { ran = true; };

  auto setup = [&](const Loop &loop) -> Promise<void> {
    co_await submitWork<void>(loop, work);
  };

  run_loop(setup);
  EXPECT_TRUE(ran);
}

TEST(AsyncWorkTest, scheduleMany) {
  static constexpr unsigned num = 100;
  std::atomic<unsigned> counter = 0;
  std::vector<unsigned> order;
  order.resize(num);
  auto work = [&](unsigned) -> void { ++counter; };

  auto setup = [&](const Loop &loop) -> Promise<void> {
    std::vector<Promise<void>> promises;
    for (unsigned i = 0; i < num; ++i) {
      promises.push_back(submitWork<void>(loop, [i, &work]() { work(i); }));
    }

    unsigned waited{};
    for (auto &p : promises) {
      co_await p;
      ++waited;
    }
    EXPECT_EQ(waited, num);
  };

  run_loop(setup);
  EXPECT_EQ(counter, num);
}

TEST(AsyncWorkTest, resultReturned) {
  static constexpr unsigned num = 100;

  auto work = [](unsigned i) -> unsigned { return i; };

  auto setup = [&](const Loop &loop) -> Promise<void> {
    std::vector<Promise<unsigned>> promises;
    for (unsigned i = 0; i < num; ++i) {
      promises.push_back(
          submitWork(loop, std::function<unsigned()>{
                               [i, &work]() -> unsigned { return work(i); }}));
    }

    unsigned waited{};
    for (auto &p : promises) {
      auto result = co_await p;
      EXPECT_EQ(result, waited);
      ++waited;
    }
    EXPECT_EQ(waited, num);
  };

  run_loop(setup);
}

TEST(AsyncWorkTest, exceptionThrown) {
  auto work = []() -> void { throw std::runtime_error("test"); };

  auto setup = [&](const Loop &loop) -> Promise<void> {
    auto p = submitWork<void>(loop, work);
    try {
      co_await p;
      EXPECT_TRUE(false) << "Expected exception";
    } catch (const std::runtime_error &e) {
      EXPECT_STREQ(e.what(), "test");
    }
  };

  run_loop(setup);
}

TEST(AsyncWorkTest, execptionThrownForValue) {
  auto work = []() -> unsigned { throw std::runtime_error("test"); };

  auto setup = [&](const Loop &loop) -> Promise<void> {
    auto p = submitWork<unsigned>(loop, work);
    try {
      co_await p;
      EXPECT_TRUE(false) << "Expected exception";
    } catch (const std::runtime_error &e) {
      EXPECT_STREQ(e.what(), "test");
    }
  };
  run_loop(setup);
}

TEST(AsyncWorkTest, workNotAwaited) {
  // Test that work on the threadpool finishes even if the main function returns
  // early.
  bool workRan = false;
  auto work = [&workRan]() -> void {
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
    workRan = true;
  };

  auto setup = [&](const Loop &loop) -> Promise<void> {
    submitWork<void>(loop, work);
    co_return;
  };

  run_loop(setup);
  EXPECT_TRUE(workRan);
}

} // namespace
