#include "uvco/promise/promise.h"
#include "uvco/run.h"
#include "uvco/tcp.h"
#include "uvco/tcp_stream.h"

#include <fmt/core.h>
#include <optional>
#include <string>
#include <sys/socket.h>

using namespace uvco;

// Using co_await in a function turns it into a coroutine. You can co_await all
// Promise and MultiPromise values; the right thing will happen.
Promise<void> testHttpRequest(const Loop &loop) {
  TcpClient client{loop, "borgac.net", 80, AF_UNSPEC};
  TcpStream stream = co_await client.connect();

  co_await stream.write(
      fmt::format("HEAD / HTTP/1.0\r\nHost: borgac.net\r\n\r\n"));
  do {
    std::optional<std::string> chunk = co_await stream.read();
    if (chunk) {
      fmt::print("Got chunk: >> {} <<\n", *chunk);
    } else {
      break;
    }
  } while (true);
  co_await stream.closeReset();
}

// Manual setup: this will be part of uvco later.
void run_loop() {
  // As described in the first example.
  uvco::runMain<void>([](const Loop &loop) -> uvco::Promise<void> {
    return testHttpRequest(loop);
  });
}

int main() {
  run_loop();
  return 0;
}
