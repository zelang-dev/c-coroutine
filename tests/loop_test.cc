#include <gtest/gtest.h>
#include <sys/socket.h>
#include <uv.h>

#include "test_util.h"

#include "uvco/exception.h"
#include "uvco/loop/loop.h"
#include "uvco/promise/promise.h"
#include "uvco/stream.h"

#include <coroutine>
#include <string>

namespace {

using namespace uvco;

TEST(LoopTest, createNestedLoopFails) {
  auto setup = [](const Loop & /*loop*/) -> uvco::Promise<void> {
    auto innerSetup = [](const Loop & /*loop*/) -> uvco::Promise<void> {
      EXPECT_THROW({ Loop innerLoop; }, UvcoException);
      co_return;
    };
    run_loop(innerSetup);
    co_return;
  };

  EXPECT_THROW({ run_loop(setup); }, UvcoException);
}

TEST(LoopTest, noLoop) {
  EXPECT_THROW({ Loop::enqueue(std::coroutine_handle<>{}); }, UvcoException);
}

TEST(LoopTest, exceptionLeavesLoop) {
  auto inner = [](const Loop &loop) -> uvco::Promise<void> {
    co_await yield();
    TtyStream tty = TtyStream::tty(loop, -1);
    co_await tty.write("Hello");
  };
  auto setup = [&inner](const Loop &loop) -> uvco::Promise<void> {
    co_await inner(loop);
    co_return;
  };

  EXPECT_THROW({ run_loop(setup); }, UvcoException);
}

} // namespace
