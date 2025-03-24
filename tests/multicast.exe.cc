/*
 * This is a real-world example used to debug a multicast issue.
 *
 * After sending an initial small packet to the --multicast-address,
 * this program will listen for packets on that multicast address,
 * and reply to each incoming packet with a small burst of reply
 * packets to the sender.
 *
 * It is included to serve as example for how to write UDP-based
 * code using uv-co, including interaction with multicast features.
 */

#include <boost/program_options.hpp>
#include <boost/program_options/detail/parsers.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options/variables_map.hpp>
#include <fmt/core.h>
#include <fmt/format.h>

#include "uvco/exception.h"
#include "uvco/name_resolution.h"
#include "uvco/promise/promise.h"
#include "uvco/run.h"
#include "uvco/timer.h"
#include "uvco/udp.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

using namespace uvco;

struct Options {
  const Loop &loop;

  std::string listenAddress = "0.0.0.0";
  std::string multicastAddress = "239.253.1.1";
  uint16_t port = 8012;
};

Options parseOptions(const Loop &loop, int argc, const char **argv) {
  namespace po = boost::program_options;
  Options options{loop};
  bool help = false;
  po::options_description desc;
  desc.add_options()("listen-address",
                     po::value<std::string>(&options.listenAddress),
                     "Listen/connect address")(
      "multicast-address", po::value<std::string>(&options.multicastAddress),
      "Listen/connect address")("port", po::value<uint16_t>(&options.port),
                                "Listen/connect port")(
      "help,h", po::bool_switch(&help), "Display help");
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);
  if (help) {
    std::cerr << desc << std::endl;
    std::exit(0);
  }
  return options;
}

Promise<void> sendSome(const Options &opt, AddressHandle dst,
                       size_t packets = 5, int interval = 1) {
  Udp udp{opt.loop};
  std::string message = "Hello back";

  for (size_t i = 0; i < packets; i++) {
    co_await udp.send(message, dst);
    co_await sleep(opt.loop, 50 * interval);
  }
  co_await udp.close();
}

Promise<void> printPackets(Options opt) {
  Udp udp{opt.loop};
  std::vector<Promise<void>> active;

  try {
    co_await udp.bind(opt.multicastAddress, opt.port);

    udp.joinMulticast(opt.multicastAddress, opt.listenAddress);
    udp.setMulticastLoop(false);

    // Send initial message to multicast group.
    AddressHandle dst{opt.multicastAddress, opt.port};
    // constexpr static std::string_view hello = "hello first";
    // udp.send(hello, dst);

    fmt::print(stderr, "waiting for packets\n");
    while (true) {
      const auto [packet, from] = co_await udp.receiveOneFrom();
      fmt::print("Received packet: {} from {}\n", packet, from.toString());
      // sendSome(opt, from);
    }
  } catch (const UvcoException &e) {
    fmt::print(stderr, "exception: {}\n", e.what());
  }
  co_await udp.close();
}

int main(int argc, const char **argv) {

  runMain<void>([&](const Loop &loop) {
    Options opt = parseOptions(loop, argc, argv);
    // Promise can be dropped! The coroutine still lives on the event loop.
    return printPackets(opt);
  });
}
