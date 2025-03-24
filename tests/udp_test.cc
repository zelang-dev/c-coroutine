
#include <fmt/core.h>
#include <gtest/gtest.h>
#include <uv.h>

#include "uvco/exception.h"
#include "uvco/loop/loop.h"
#include "uvco/name_resolution.h"
#include "uvco/promise/multipromise.h"
#include "uvco/promise/promise.h"
#include "uvco/run.h"
#include "uvco/udp.h"

#include "test_util.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace uvco;

Promise<void> udpServer(const Loop &loop, unsigned expect, unsigned &received) {
  Udp server{loop};
  co_await server.bind("::1", 9999, 0);

  MultiPromise<std::pair<std::string, AddressHandle>> packets =
      server.receiveMany();

  for (uint32_t counter = 0; counter < expect; ++counter) {
    auto recvd = co_await packets;
    if (!recvd) {
      break;
    }
    ++received;
    std::string &buffer = recvd->first;
    auto &from = recvd->second;
    co_await server.send(buffer, from);
  }
  EXPECT_EQ(server.getSockname().toString(), "[::1]:9999");
  // Necessary for the receiver promise to return and not leak memory!
  server.stopReceiveMany(packets);
  EXPECT_FALSE((co_await packets).has_value());
  co_await server.close();
  co_return;
}

Promise<void> udpClient(const Loop &loop, unsigned send, unsigned &sent) {
  // Ensure server has started.
  co_await yield();
  std::string msg = "Hello there!";

  Udp client{loop};

  // Before any operation: EBADF.
  EXPECT_THROW({ client.getPeername().value(); }, UvcoException);
  EXPECT_THROW({ client.getSockname().family(); }, UvcoException);

  co_await client.bind("::1", 7777);

  EXPECT_FALSE(client.getPeername());

  co_await client.connect("::1", 9999);

  // Repeat but with AddressHandle
  const AddressHandle dest{"::1", 9999};
  co_await client.connect(dest);

  EXPECT_TRUE(client.getPeername());
  EXPECT_EQ(client.getPeername()->toString(), "[::1]:9999");

  for (uint32_t i = 0; i < send; ++i) {
    co_await client.send(msg, {});
    ++sent;
    auto response = co_await client.receiveOne();
  }

  co_await client.close();
  co_return;
}

Promise<void> join(Promise<void> promise1, Promise<void> promise2) {
  co_await promise1;
  co_await promise2;
}

TEST(UdpTest, testPingPong) {
  constexpr unsigned pingPongCount = 10;
  unsigned sent = 0;
  unsigned received = 0;
  auto setup = [&](const Loop &loop) -> uvco::Promise<void> {
    return join(udpServer(loop, pingPongCount, received),
                udpClient(loop, pingPongCount, sent));
  };

  run_loop(setup);
  EXPECT_EQ(sent, received);
  EXPECT_EQ(sent, pingPongCount);
}

TEST(UdpTest, DISABLED_benchmarkPingPong) {
  constexpr unsigned pingPongCount = 100000;
  unsigned sent = 0;
  unsigned received = 0;
  auto setup = [&](const Loop &loop) -> uvco::Promise<void> {
    return join(udpServer(loop, pingPongCount, received),
                udpClient(loop, pingPongCount, sent));
  };

  run_loop(setup);
  EXPECT_EQ(sent, received);
  EXPECT_EQ(sent, pingPongCount);
}

// As opposed to the ping pong test, measure raw throughput without latency
// considerations.

Promise<void> udpSource(const Loop &loop, unsigned send, unsigned &sent) {
  // Ensure server has started.
  co_await yield();
  std::string msg = "Hello there!";
  const AddressHandle dst{"::1", 9999};

  Udp client{loop};

  co_await client.bind("::1", 7777);

  for (uint32_t i = 0; i < send / 2; ++i) {
    // This exercises the packet queue.
    client.send(msg, dst);
    ++sent;
    co_await client.send(msg, dst);
    ++sent;
  }

  co_await client.close();
  co_return;
}

Promise<void> udpSink(const Loop &loop, unsigned expect, unsigned &received) {
  Udp server{loop};
  co_await server.bind("::1", 9999, 0);

  MultiPromise<std::pair<std::string, AddressHandle>> packets =
      server.receiveMany();

  // Account for potentially lost packets - let loop finish a bit early.
  constexpr static unsigned tolerance = 3;
  expect -= tolerance;

  for (uint32_t counter = 0; counter < expect; ++counter) {
    // TODO: currently we can only receive one packet at a time, the UDP socket
    // needs an additional internal queue if there is more than one packet at a
    // time.
    auto recvd = co_await packets;
    if (!recvd) {
      break;
    }
    ++received;
  }
  received += tolerance;
  server.stopReceiveMany(packets);
  EXPECT_FALSE((co_await packets).has_value());
  co_await server.close();
  co_return;
}

TEST(UdpTest, DISABLED_benchmarkThroughput) {
  constexpr unsigned throughputCount = 100000;
  unsigned sent = 0;
  unsigned received = 0;
  auto setup = [&](const Loop &loop) -> uvco::Promise<void> {
    return join(udpSink(loop, throughputCount, received),
                udpSource(loop, throughputCount, sent));
  };

  run_loop(setup);
  EXPECT_EQ(sent, received);
  EXPECT_EQ(sent, throughputCount);
}

TEST(UdpTest, testDropReceiver) {
  auto setup = [&](const Loop &loop) -> uvco::Promise<void> {
    Udp server{loop};
    co_await server.bind("::1", 9999, 0);

    Udp client{loop};
    co_await client.bind("::1", 7777);
    co_await client.connect("::1", 9999);

    MultiPromise<std::pair<std::string, AddressHandle>> packets =
        server.receiveMany();

    std::string fromClient{"Hello there!"};
    co_await client.send(fromClient, {});

    const auto receivedFromClient = co_await packets;
    EXPECT_TRUE(receivedFromClient.has_value());

    std::string msg = "Hello there!";
    co_await server.send(msg, AddressHandle{"::1", 7777});
    co_await server.close();

    const auto receivedFromServer = co_await client.receiveOne();
    EXPECT_EQ(msg, receivedFromServer);

    co_await client.close();
  };

  run_loop(setup);
}

TEST(UdpTest, cancelWhileReceiving) {
  auto setup = [&](const Loop &loop) -> uvco::Promise<void> {
    Udp server{loop};
    co_await server.bind("::1", 9999, 0);

    MultiPromise<std::pair<std::string, AddressHandle>> packets =
        server.receiveMany();

    co_await yield();
    server.stopReceiveMany(packets);
    co_await server.close();
  };

  run_loop(setup);
}

TEST(UdpTest, testTtl) {
  auto setup = [&](const Loop &loop) -> uvco::Promise<void> {
    Udp server{loop};
    co_await server.bind("::1", 9999, 0);
    server.setTtl(10);
    co_await server.close();
  };

  run_loop(setup);
}

TEST(UdpTest, testBroadcast) {
  auto setup = [&](const Loop &loop) -> uvco::Promise<void> {
    Udp server{loop};
    co_await server.bind("::1", 9999);
    server.setBroadcast(true);
    try {
      std::vector<char> buf(10, 'a');
      co_await server.send(buf, AddressHandle{"255.255.255.255", 9988});
    } catch (const UvcoException &e) {
      fmt::print(stderr, "Caught exception: {}\n", e.what());
    }
    co_await server.close();
  };

  run_loop(setup);
}

TEST(UdpTest, simultaneousReceiveDies) {
  auto setup = [&](const Loop &loop) -> uvco::Promise<void> {
    Udp server{loop};
    co_await server.bind("::1", 9999, 0);

    MultiPromise<std::pair<std::string, AddressHandle>> packets =
        server.receiveMany();

    EXPECT_DEATH({ auto packets2 = server.receiveMany(); }, "dataIsNull");

    co_await server.close();
  };

  run_loop(setup);
}

TEST(UdpTest, simultaneousReceiveOneDies) {
  auto setup = [&](const Loop &loop) -> uvco::Promise<void> {
    Udp server{loop};
    co_await server.bind("::1", 9999, 0);

    Promise<std::string> packet = server.receiveOne();
    EXPECT_DEATH({ auto packet = server.receiveOne(); }, "dataIsNull");

    co_await server.close();
  };

  run_loop(setup);
}

TEST(UdpTest, udpNoClose) {
  uint64_t counter = 0;
  const uv_udp_t *underlying{};
  auto setup = [&counter,
                &underlying](const Loop &loop) -> uvco::Promise<void> {
    Udp udp = Udp{loop};
    const AddressHandle dest{"::1", 38212};
    underlying = udp.underlying();

    std::string message = "Hello";
    co_await udp.send(message, dest);
    ++counter;
  };

  run_loop(setup);
  EXPECT_EQ(counter, 1);

  // This test checks what happens if a coroutine finishes without closing the
  // stream. In order to satisfy asan, we still need to free the memory in the
  // end.
  delete underlying;
}

TEST(UdpTest, sendNoAddress) {
  auto setup = [](const Loop &loop) -> uvco::Promise<void> {
    Udp udp{loop};
    std::string message = "Hello";
    try {
      co_await udp.send(message, {});
      // Shouldn't reach here.
      EXPECT_FALSE(true);
    } catch (const UvcoException &e) {
      fmt::print(stderr, "Caught exception: {}\n", e.what());
    }
    co_await udp.close();
  };

  run_loop(setup);
}

TEST(UdpTest, closedWhileReceiving) {
  auto setup = [&](const Loop &loop) -> uvco::Promise<void> {
    Udp udp{loop};
    co_await udp.bind("::1", 9999);
    auto receiver = udp.receiveMany();
    co_await udp.close();
    EXPECT_FALSE((co_await receiver).has_value());
  };

  run_loop(setup);
}

TEST(UdpTest, closeWhileReceivingInOtherCoroutine) {
  auto co_waiter =
      [](MultiPromise<std::pair<std::string, AddressHandle>> packets)
      -> uvco::Promise<void> { EXPECT_FALSE((co_await packets).has_value()); };
  auto setup = [&](const Loop &loop) -> uvco::Promise<void> {
    Udp udp{loop};
    co_await udp.bind("::1", 9999);
    auto receiver = udp.receiveMany();
    Promise<void> waiter = co_waiter(receiver);
    co_await udp.close();
  };

  run_loop(setup);
}

} // namespace
