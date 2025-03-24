// uvco (c) 2024 Lewin Bormann. See LICENSE for specific terms.

#include <uv.h>

#include "test_util.h"

#include "uvco/exception.h"
#include "uvco/loop/loop.h"
#include "uvco/promise/promise.h"
#include "uvco/run.h"
#include "uvco/uds.h"
#include "uvco/uds_stream.h"

#include <fmt/core.h>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

namespace {
using namespace uvco;

constexpr std::string_view testSocketPath = "/tmp/_uvco_test.sock";

Promise<void> pingPongServer(const Loop &loop) {
  UnixStreamServer server{loop, testSocketPath};
  auto listener = server.listen();
  try {
    std::optional<UnixStream> stream = co_await listener;
    if (!stream) {
      fmt::print(stderr, "No stream\n");
      co_return;
    }
    fmt::print(stderr, "Server at {}\n", stream->getSockName());
    fmt::print(stderr, "Received connection from client at {}\n",
               stream->getPeerName());
    std::optional<std::string> message = co_await stream->read();
    if (!message) {
      throw UvcoException{UV_EOF, "No data from client"};
    }

    co_await stream->write(
        fmt::format("Hello, back! I received '{}'\n", message.value()));
    co_await stream->close();
  } catch (const UvcoException &e) {
    fmt::print(stderr, "Error: {}\n", e.what());
    throw;
  }

  fmt::print(stderr, "Closing server\n");
  co_await server.close();
  fmt::print(stderr, "Listen finished\n");
}

Promise<void> pingPongClient(const Loop &loop) {
  UnixStreamClient client{loop};
  try {
    UnixStream stream = co_await client.connect(testSocketPath);
    fmt::print(stderr, "Client connecting from {}\n", stream.getSockName());
    fmt::print(stderr, "Client connected to server at {}\n",
               stream.getPeerName());

    co_await stream.write("Hello from client");
    fmt::print(stderr, "Sent; waiting for reply\n");

    std::optional<std::string> buf = co_await stream.read();
    if (!buf) {
      throw UvcoException{UV_EOF, "No data from server"};
    }
    fmt::print(stderr, "Received: {}", *buf);
    co_await stream.close();
  } catch (const UvcoException &e) {
    fmt::print(stderr, "Error: {}\n", e.what());
    throw;
  }
}

TEST(UdsTest, UnixStreamPingPong) {
  auto setup = [](const Loop &loop) -> Promise<void> {
    try {
      Promise<void> server = pingPongServer(loop);
      co_await yield();
      Promise<void> client = pingPongClient(loop);
      co_await server;
      co_await client;
    } catch (const UvcoException &e) {
      fmt::print(stderr, "Error in setup function: {}\n", e.what());
      throw;
    }
  };

  run_loop(setup);
}

TEST(UdsTest, UnixStreamFailConnect) {
  auto setup = [](const Loop &loop) -> Promise<void> {
    try {
      UnixStreamClient client{loop};
      UnixStream stream = co_await client.connect("/tmp/does_not_exist.sock");
    } catch (const UvcoException &e) {
      EXPECT_EQ(UV_ENOENT, e.status);
      throw;
    }
  };

  EXPECT_THROW({ run_loop(setup); }, UvcoException);
}

TEST(UdsTest, UnixStreamListenerStop) {
  auto setup = [](const Loop &loop) -> Promise<void> {
    UnixStreamServer server{loop, testSocketPath};
    auto listener = server.listen();
    co_await yield();
    co_await server.close();
    // The following line would result in a crash (intentional) because the
    // listener generator has returned.
    // co_await listener;
    co_return;
  };

  run_loop(setup);
}

} // namespace
