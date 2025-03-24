
#include <boost/assert.hpp>
#include <gtest/gtest.h>

#include "test_util.h"
#include "uvco/exception.h"
#include "uvco/name_resolution.h"
#include "uvco/promise/multipromise.h"
#include "uvco/promise/promise.h"
#include "uvco/run.h"
#include "uvco/tcp.h"
#include "uvco/tcp_stream.h"

#include <optional>
#include <string>
#include <utility>

namespace {
using namespace uvco;

Promise<void> echoReceived(TcpStream stream, bool &received, bool &responded) {
  std::optional<std::string> chunk = co_await stream.read();
  BOOST_ASSERT(chunk);
  received = true;
  co_await stream.writeBorrowed(*chunk);
  responded = true;
  co_await stream.shutdown();
  co_await stream.closeReset();
}

Promise<void> echoTcpServer(const Loop &loop, bool &received, bool &responded) {
  AddressHandle addr{"127.0.0.1", 8090};
  TcpServer server{loop, addr};

  MultiPromise<TcpStream> clients = server.listen();

  std::optional<TcpStream> maybeClient = co_await clients;
  BOOST_ASSERT(maybeClient);

  TcpStream client{nullptr};
  client = std::move(*maybeClient);

  client.keepAlive(true);
  client.noDelay(true);

  AddressHandle peer = client.getPeerName();
  AddressHandle ours = client.getSockName();

  EXPECT_EQ(ours.toString(), "127.0.0.1:8090");
  EXPECT_EQ(peer.address(), "127.0.0.1");

  Promise<void> clientLoop =
      echoReceived(std::move(client), received, responded);
  co_await clientLoop;
  co_await server.close();
}

Promise<void> sendTcpClient(const Loop &loop, bool &sent,
                            bool &responseReceived) {
  // Also test move ctors.
  TcpClient client{loop, "127.0.0.1", 8090};
  TcpClient client2{std::move(client)};
  client = std::move(client2);

  TcpStream stream = co_await client.connect();

  co_await stream.write("Hello World");
  sent = true;
  std::optional<std::string> response = co_await stream.read();
  responseReceived = true;

  EXPECT_EQ(response, "Hello World");
  co_await stream.close();
}

Promise<void> join(Promise<void> promise1, Promise<void> promise2) {
  co_await promise1;
  co_await promise2;
}

} // namespace

TEST(TcpTest, singlePingPong) {
  bool sent = false;
  bool received = false;
  bool responded = false;
  bool responseReceived = false;

  auto setup = [&](const uvco::Loop &loop) -> uvco::Promise<void> {
    return join(echoTcpServer(loop, received, responded),
                sendTcpClient(loop, sent, responseReceived));
  };

  run_loop(setup);
  EXPECT_TRUE(sent);
  EXPECT_TRUE(received);
  EXPECT_TRUE(responded);
  EXPECT_TRUE(responseReceived);
}

TEST(TcpTest, validBind) {
  auto setup = [&](const Loop &loop) -> uvco::Promise<void> {
    AddressHandle addr{"127.0.0.1", 0};
    TcpServer server{loop, addr};
    co_await server.close();
  };

  run_loop(setup);
}

TEST(TcpTest, invalidLocalhostConnect) {
  auto main = [](const Loop &loop) -> Promise<void> {
    const AddressHandle addr{"::1", 39856};
    TcpClient client{loop, addr};
    EXPECT_THROW(
        {
          TcpStream stream = co_await client.connect();
          co_await stream.close();
        },
        UvcoException);
  };

  run_loop(main);
}
