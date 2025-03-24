
#include <boost/assert.hpp>
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <uv.h>

#include "test_util.h"
#include "uvco/exception.h"
#include "uvco/loop/loop.h"
#include "uvco/pipe.h"
#include "uvco/promise/promise.h"
#include "uvco/stream.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace uvco;

// Doesn't work on Github Actions because no TTY is attached.
TEST(TtyTest, DISABLED_stdioTest) {
  uint64_t counter = 0;
  auto setup = [&counter](const Loop &loop) -> uvco::Promise<void> {
    std::vector<TtyStream> ttys;
    ttys.emplace_back(TtyStream::stdin(loop));
    ttys.emplace_back(TtyStream::stdout(loop));
    ttys.emplace_back(TtyStream::stderr(loop));

    for (auto &tty : ttys) {
      co_await tty.write(" ");
      ++counter;
      co_await tty.close();
      ++counter;
    }
  };

  run_loop(setup);
  EXPECT_EQ(counter, 6);
}

TEST(TtyTest, stdoutNoClose) {
  uint64_t counter = 0;
  const uv_tty_t *underlying{};
  auto setup = [&counter,
                &underlying](const Loop &loop) -> uvco::Promise<void> {
    TtyStream stdout = TtyStream::stdout(loop);
    underlying = (uv_tty_t *)stdout.underlying();

    co_await stdout.write(" ");
    ++counter;
  };

  run_loop(setup);
  EXPECT_EQ(counter, 1);

  // This test checks what happens if a coroutine finishes without closing the
  // stream. In order to satisfy asan, we still need to free the memory in the
  // end.
  delete underlying;
}

TEST(TtyTest, invalidFd) {
  auto setup = [](const Loop &loop) -> uvco::Promise<void> {
    EXPECT_THROW({ TtyStream tty = TtyStream::tty(loop, -1); }, UvcoException);
    co_return;
  };

  run_loop(setup);
}

TEST(TtyTest, closeWhileReading) {
  auto setup = [](const Loop &loop) -> uvco::Promise<void> {
    TtyStream tty = TtyStream::stdin(loop);
    auto reader = tty.read();
    co_await tty.close();
    EXPECT_FALSE((co_await reader).has_value());
  };

  run_loop(setup);
}

TEST(PipeTest, danglingReadDies) {
  auto f = [](const Loop &loop) -> uvco::Promise<std::optional<std::string>> {
    auto [read, write] = pipe(loop);
    auto _ = write.write("Hello");
    return read.read();
  };
  auto setup = [&f](const Loop &loop) -> uvco::Promise<void> {
    co_await f(loop);
  };
  EXPECT_DEATH(run_loop(setup), R"(stream must outlive reader coroutine)");
}

TEST(PipeTest, pipePingPong) {
  auto setup = [&](const Loop &loop) -> uvco::Promise<void> {
    auto [read, write] = pipe(loop);

    co_await write.write("Hello\n");
    co_await write.write("Hello");
    EXPECT_EQ(co_await read.read(), std::make_optional("Hello\nHello"));
    co_await read.close();
    co_await write.close();
  };

  run_loop(setup);
}

TEST(PipeTest, doubleReadDies) {
  auto setup = [&](const Loop &loop) -> uvco::Promise<void> {
    auto [read, write] = pipe(loop);

    Promise<std::optional<std::string>> readPromise = read.read();
    EXPECT_THROW({ co_await read.read(); }, UvcoException);
    co_await read.close();
    co_await write.close();
  };

  run_loop(setup);
}

TEST(PipeTest, largeWriteRead) {
  std::ifstream urandom("/dev/urandom", std::ios::binary);
  std::array<char, 1024> buffer{};
  urandom.read(buffer.data(), buffer.size());

  auto setup = [&](const Loop &loop) -> uvco::Promise<void> {
    auto [read, write] = pipe(loop);

    for (unsigned i = 0; i < 10; ++i) {
      EXPECT_EQ(buffer.size(), co_await write.write(
                                   std::string(buffer.data(), buffer.size())));
    }
    co_await write.close();

    size_t bytesRead{};

    while (true) {
      // Read random chunk size.
      auto chunk = co_await read.read(732);
      if (!chunk.has_value()) {
        break;
      }
      bytesRead += chunk->size();
    }
    BOOST_ASSERT(bytesRead == 10240);
    co_await read.close();
  };

  run_loop(setup);
}

TEST(PipeTest, readIntoBuffer) {
  auto setup = [&](const Loop &loop) -> uvco::Promise<void> {
    auto [read, write] = pipe(loop);

    co_await write.write("Hello");
    std::array<char, 32> buffer{};
    size_t bytesRead = co_await read.read(buffer);
    EXPECT_EQ(bytesRead, 5);
    EXPECT_EQ(std::string(buffer.data(), bytesRead), "Hello");
    co_await read.close();
    co_await write.close();
  };
  run_loop(setup);
}

} // namespace
