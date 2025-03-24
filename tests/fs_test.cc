
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <gtest/gtest.h>
#include <uv.h>

#include "test_util.h"
#include "uvco/exception.h"
#include "uvco/fs.h"
#include "uvco/loop/loop.h"
#include "uvco/promise/multipromise.h"
#include "uvco/promise/promise.h"
#include "uvco/run.h"

#include <algorithm>
#include <cstddef>
#include <fcntl.h>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

using namespace uvco;

TEST(FsTest, OpenFile) {
  auto setup = [](const Loop &loop) -> Promise<void> {
    auto file = co_await File::open(loop, "/dev/null", O_RDONLY);
    EXPECT_GT(file.file(), 0);
    co_await file.close();
  };

  run_loop(setup);
}

TEST(FsTest, FileNotFound) {
  auto setup = [](const Loop &loop) -> Promise<void> {
    try {
      co_await File::open(loop, "/dev/does_not_exist", O_RDONLY);
      EXPECT_FALSE(true);
    } catch (const UvcoException &e) {
      EXPECT_EQ(e.status, UV_ENOENT);
    }
  };

  run_loop(setup);
}

TEST(FsTest, forgetClose) {
  auto setup = [](const Loop &loop) -> Promise<void> {
    // At the moment this works without crashing asan.
    auto file = co_await File::open(loop, "/dev/null", O_RDONLY);
    EXPECT_GT(file.file(), 0);
  };

  run_loop(setup);
}

// Read some zeros.
TEST(FsTest, simpleRead) {
  static constexpr size_t bufSize = 32;

  auto setup = [](const Loop &loop) -> Promise<void> {
    auto file = co_await File::open(loop, "/dev/zero", O_RDONLY);
    EXPECT_GT(file.file(), 0);

    std::string buffer(bufSize, 'x');
    EXPECT_EQ(bufSize, buffer.size());

    size_t read = co_await file.read(buffer);

    EXPECT_EQ(bufSize, read);
    EXPECT_EQ(bufSize, buffer.size());
    EXPECT_TRUE(std::all_of(buffer.begin(), buffer.end(),
                            [](char c) { return c == 0; }));

    co_await file.close();
  };

  run_loop(setup);
}

TEST(FsTest, simpleReadWriteUnlink) {
  static constexpr std::string_view contents = "Hello World\n";
  static constexpr std::string_view fileName = "/tmp/_uvco_test_file";
  auto setup = [](const Loop &loop) -> Promise<void> {
    auto file = co_await File::open(loop, fileName, O_RDWR | O_CREAT);

    co_await file.write(contents);

    std::string buffer(64, '\0');

    const size_t bytesRead = co_await file.read(buffer, 0);

    EXPECT_EQ(contents.size(), bytesRead);
    EXPECT_EQ(contents, buffer);

    co_await file.close();

    co_await File::unlink(loop, fileName);
  };

  run_loop(setup);
}

TEST(FsTest, mkDirRmDir) {
  static constexpr std::string_view dirName = "/tmp/_uvco_test_dir";
  auto setup = [](const Loop &loop) -> Promise<void> {
    co_await Directory::mkdir(loop, dirName);

    try {
      co_await Directory::mkdir(loop, dirName);
    } catch (const UvcoException &e) {
      EXPECT_EQ(e.status, UV_EEXIST);
    }

    co_await Directory::rmdir(loop, dirName);
  };

  run_loop(setup);
}

TEST(FsTest, openDir) {
  static constexpr std::string_view dirName = "/tmp";
  auto setup = [](const Loop &loop) -> Promise<void> {
    auto dir = co_await Directory::open(loop, dirName);
    // Test move ctor.
    auto dir1 = std::move(dir);
    dir = std::move(dir1);

    auto entries = co_await dir.read();

    EXPECT_GT(entries.size(), 0);

    co_await dir.close();
  };

  run_loop(setup);
}

TEST(FsTest, scanDir) {
  static constexpr std::string_view dirName = "/tmp";
  auto setup = [](const Loop &loop) -> Promise<void> {
    auto entries = Directory::readAll(loop, dirName);
    unsigned count = 0;
    std::optional<Directory::DirEnt> entry;

    while ((entry = co_await entries).has_value()) {
      EXPECT_GT(entry->name.size(), 0);
      ++count;
    }
    EXPECT_GT(count, 0);
  };

  run_loop(setup);
}

TEST(FsWatchTest, basicFileWatch) {
  static constexpr std::string_view filename = "/tmp/_uvco_watch_test";

  auto setup = [](const Loop &loop) -> Promise<void> {
    auto file = co_await File::open(loop, filename, O_RDWR | O_CREAT);

    auto watch = co_await FsWatch::create(loop, filename);
    MultiPromise<FsWatch::FileEvent> watcher = watch.watch();

    co_await file.write("Hello World\n");

    const std::optional<FsWatch::FileEvent> event = co_await watcher;
    BOOST_ASSERT(event.has_value());
    EXPECT_EQ(event->path, "_uvco_watch_test");
    EXPECT_EQ(event->events, UV_CHANGE);

    co_await watch.stopWatch(std::move(watcher));
    co_await watch.close();
    co_await file.close();
    co_await File::unlink(loop, filename);
    co_return;
  };

  run_loop(setup);
}

TEST(FsWatchTest, basicDirWatch) {
  static constexpr std::string_view dirName = "/tmp/_uvco_watch_test_dir";
  static constexpr std::string_view filename = "/tmp/_uvco_watch_test_dir/file";
  static constexpr std::string_view dirName2 =
      "/tmp/_uvco_watch_test_dir/subdir";
  static constexpr std::string_view filename2 =
      "/tmp/_uvco_watch_test_dir/subdir/subfile";

  auto setup = [](const Loop &loop) -> Promise<void> {
    co_await Directory::mkdir(loop, dirName);
    co_await Directory::mkdir(loop, dirName2);

    auto file = co_await File::open(loop, filename, O_RDWR | O_CREAT);

    auto watch = co_await FsWatch::create(loop, dirName);
    MultiPromise<FsWatch::FileEvent> watcher = watch.watch();

    // Check that FsWatch::create is not recursive
    auto file2 = co_await File::open(loop, filename2, O_RDWR | O_CREAT);
    co_await file2.write("Hello World\n");
    co_await file.write("Hello World\n");

    const std::optional<FsWatch::FileEvent> event = co_await watcher;
    BOOST_ASSERT(event.has_value());
    EXPECT_EQ(event->path, "file");
    EXPECT_EQ(event->events, UV_CHANGE);

    co_await watch.stopWatch(std::move(watcher));
    co_await watch.close();
    co_await file.close();
    co_await file2.close();
    co_await File::unlink(loop, filename);
    co_await File::unlink(loop, filename2);
    co_await Directory::rmdir(loop, dirName2);
    co_await Directory::rmdir(loop, dirName);
    co_return;
  };

  run_loop(setup);
}

// This test shoould only work on macOS and Windows
// https://docs.libuv.org/en/v1.x/fs_event.html#c.uv_fs_event_start
TEST(DISABLED_FsWatchTest, watchRecursive) {
  static constexpr std::string_view dirName = "/tmp/_uvco_watch_test_dir";
  static constexpr std::string_view subdirName =
      "/tmp/_uvco_watch_test_dir/subdir";
  static constexpr std::string_view filename =
      "/tmp/_uvco_watch_test_dir/subdir/file";

  auto setup = [](const Loop &loop) -> Promise<void> {
    co_await Directory::mkdir(loop, dirName);
    co_await Directory::mkdir(loop, subdirName);

    auto watch = co_await FsWatch::createRecursive(loop, dirName);
    MultiPromise<FsWatch::FileEvent> watcher = watch.watch();

    auto file = co_await File::open(loop, filename, O_RDWR | O_CREAT);
    co_await file.write("Hello World\n");

    const std::optional<FsWatch::FileEvent> event = co_await watcher;
    BOOST_ASSERT(event.has_value());
    EXPECT_EQ(event->path, "subdir/file");
    EXPECT_EQ(event->events, UV_CHANGE);

    co_await watch.stopWatch(std::move(watcher));
    co_await watch.close();
    co_await file.close();
    co_await File::unlink(loop, filename);
    co_await Directory::rmdir(loop, subdirName);
    co_await Directory::rmdir(loop, dirName);
    co_return;
  };

  run_loop(setup);
}

TEST(FsWatchTest, watchNonExisting) {
  static constexpr std::string_view filename =
      "/tmp/_uvco_watch_test_non_existing";

  auto setup = [](const Loop &loop) -> Promise<void> {
    try {
      auto watcher = co_await FsWatch::create(loop, filename);
      EXPECT_FALSE(true) << "Expected UvcoException";
    } catch (const UvcoException &e) {
      EXPECT_EQ(e.status, UV_ENOENT);
    }
    co_await yield();
    co_return;
  };

  run_loop(setup);
}

TEST(FsWatchTest, repeatedWatchFails) {
  static constexpr std::string_view filename = "/tmp/_uvco_watch_test";

  auto setup = [](const Loop &loop) -> Promise<void> {
    auto file = co_await File::open(loop, filename, O_RDWR | O_CREAT);

    auto watch = co_await FsWatch::create(loop, filename);
    MultiPromise<FsWatch::FileEvent> watcher = watch.watch();

    try {
      auto eventGenerator2 = watch.watch();
      EXPECT_FALSE(true) << "Expected UvcoException";
    } catch (const UvcoException &e) {
      EXPECT_EQ(e.status, UV_EBUSY);
    }

    co_await watch.stopWatch(std::move(watcher));
    co_await watch.close();
    co_await file.close();
    co_await File::unlink(loop, filename);
    co_return;
  };

  run_loop(setup);
}

} // namespace
