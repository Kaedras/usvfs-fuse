#include "../../src/utils.h"
#include "benchmark_utils.h"

#include "usvfs-fuse/usvfsmanager.h"
#include <benchmark/benchmark.h>
#include <cmath>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

using namespace std;
namespace fs = std::filesystem;

namespace
{
constexpr int bufferSize = 4096;
}

namespace benchmarks
{
static const fs::path base      = fs::temp_directory_path() / "usvfs";
static const fs::path src       = base / "src";
static const fs::path mnt       = base / "mnt";
static const fs::path file      = src / "0" / "0.txt";
static const fs::path fileUsvfs = mnt / "0.txt";

static void DoSetup_usvfs(const benchmark::State&)
{
  fs::create_directories(mnt);
  fs::create_directories(src / "0");

  char buffer[bufferSize];
  memset(buffer, 'A', sizeof(buffer));

  ofstream ofs(file);
  ofs.write(buffer, sizeof(buffer));
  ofs.close();

  const auto usvfs = UsvfsManager::instance();
  usvfs->setLogFile("/tmp/usvfs.log");
  usvfs->setLogLevel(LogLevel::Warning);
  usvfs->virtualLinkDirectoryStatic((src / "0").string(), mnt.string(), 0);
  usvfs->mount();
  this_thread::sleep_for(10ms);
}

static void DoSetup(const benchmark::State&)
{
  fs::create_directories(file.parent_path());

  char buffer[bufferSize];
  memset(buffer, 'A', sizeof(buffer));

  ofstream ofs(file);
  ofs.write(buffer, sizeof(buffer));
  ofs.close();
}

static void DoTeardown_usvfs(const benchmark::State&)
{
  const auto usvfs = UsvfsManager::instance();
  usvfs->unmount();
  this_thread::sleep_for(10ms);
  fs::remove_all(base);
}

static void DoTeardown(const benchmark::State&)
{
  fs::remove_all(base);
}

// stat
static void Stat(benchmark::State& state)
{
  struct stat st{};
  for (auto _ : state) {
    if (stat(file.c_str(), &st) != 0) {
      assert(false);
    }
  }
}

static void Stat_USVFS(benchmark::State& state)
{
  struct stat st{};
  for (auto _ : state) {
    if (stat(fileUsvfs.c_str(), &st) != 0) {
      assert(false);
    }
  }
}

// open
static void Open(benchmark::State& state)
{
  for (auto _ : state) {
    const int fd = open(file.c_str(), O_RDONLY);

    state.PauseTiming();
    assert(fd != -1);
    close(fd);
    state.ResumeTiming();
  }
}

static void Open_USVFS(benchmark::State& state)
{
  for (auto _ : state) {
    const int fd = open(fileUsvfs.c_str(), O_RDONLY);

    state.PauseTiming();
    assert(fd != -1);
    close(fd);
    state.ResumeTiming();
  }
}

// lseek
static void Lseek(benchmark::State& state)
{
  const int fd = open(file.c_str(), O_RDONLY);
  assert(fd != -1);

  for (auto _ : state) {
    off_t result = lseek(fd, 0, SEEK_SET);
    benchmark::DoNotOptimize(result);
    assert(result == 0);
  }

  close(fd);
}

static void Lseek_USVFS(benchmark::State& state)
{
  const int fd = open(file.c_str(), O_RDONLY);
  assert(fd != -1);

  for (auto _ : state) {
    off_t result = lseek(fd, 0, SEEK_SET);
    benchmark::DoNotOptimize(result);
    assert(result == 0);
  }

  close(fd);
}

// sync
static void Sync(benchmark::State& state)
{
  const int fd = open(file.c_str(), O_RDONLY);
  assert(fd != -1);

  for (auto _ : state) {
    int result = syncfs(fd);
    benchmark::DoNotOptimize(result);
    assert(result == 0);
  }

  close(fd);
}

static void Sync_USVFS(benchmark::State& state)
{
  const int fd = open(file.c_str(), O_RDONLY);
  assert(fd != -1);

  for (auto _ : state) {
    int result = syncfs(fd);
    benchmark::DoNotOptimize(result);
    assert(result == 0);
  }

  close(fd);
}

// close
static void Close(benchmark::State& state)
{
  for (auto _ : state) {
    state.PauseTiming();
    const int fd = open(file.c_str(), O_RDONLY);
    assert(fd != -1);
    state.ResumeTiming();

    close(fd);
  }
}

static void Close_USVFS(benchmark::State& state)
{
  for (auto _ : state) {
    state.PauseTiming();
    const int fd = open(fileUsvfs.c_str(), O_RDONLY);
    assert(fd != -1);
    state.ResumeTiming();

    close(fd);
  }
}

// read
static void Read(benchmark::State& state)
{
  char buffer[bufferSize];
  const int fd = open(file.c_str(), O_RDONLY);
  assert(fd != -1);
  for (auto _ : state) {
    state.PauseTiming();
    lseek(fd, 0, SEEK_SET);
    state.ResumeTiming();

    ssize_t n = read(fd, buffer, sizeof(buffer));
    benchmark::DoNotOptimize(n);
    assert(n == sizeof(buffer));
  }
  close(fd);
}

static void Read_USVFS(benchmark::State& state)
{
  char buffer[bufferSize];
  const int fd = open(fileUsvfs.c_str(), O_RDONLY);
  assert(fd != -1);
  for (auto _ : state) {
    state.PauseTiming();
    lseek(fd, 0, SEEK_SET);
    state.ResumeTiming();

    ssize_t n = read(fd, buffer, sizeof(buffer));
    benchmark::DoNotOptimize(n);
    assert(n == sizeof(buffer));
  }
  close(fd);
}

// write
static void Write(benchmark::State& state)
{
  char buffer[bufferSize];
  memset(buffer, 'A', sizeof(buffer));
  const int fd = open(file.c_str(), O_WRONLY | O_TRUNC);
  assert(fd != -1);
  for (auto _ : state) {
    state.PauseTiming();
    lseek(fd, 0, SEEK_SET);
    state.ResumeTiming();

    ssize_t n = write(fd, buffer, sizeof(buffer));
    benchmark::DoNotOptimize(n);
    assert(n == sizeof(buffer));
  }
  close(fd);
}

static void Write_USVFS(benchmark::State& state)
{
  char buffer[bufferSize];
  memset(buffer, 'A', sizeof(buffer));
  const int fd = open(fileUsvfs.c_str(), O_WRONLY | O_TRUNC);
  assert(fd != -1);
  for (auto _ : state) {
    state.PauseTiming();
    lseek(fd, 0, SEEK_SET);
    state.ResumeTiming();

    ssize_t n = write(fd, buffer, sizeof(buffer));
    benchmark::DoNotOptimize(n);
    assert(n == sizeof(buffer));
  }
  close(fd);
}

static void usvfsVirtualLinkDirectoryStatic(benchmark::State& state)
{
  const int count            = static_cast<int>(state.range(0));
  static constexpr int depth = 2;
  static constexpr int width = 10;

  fs::create_directories(mnt);
  const auto usvfs = UsvfsManager::instance();
  usvfs->setLogLevel(LogLevel::Info);

  createModFilesOnDisk(src, width, depth, count);

  // calculate the total item count
  const double fileCount  = (1 + width * (pow(width, depth) - 1) / (width - 1)) * count;
  state.counters["files"] = fileCount;

  for (auto _ : state) {
    START();
    for (int i = 0; i < count; ++i) {
      usvfs->virtualLinkDirectoryStatic(src / to_string(i), mnt);
    }
    END();
    usvfs->clearVirtualMappings();
  }
  fs::remove_all(base);
}

BENCHMARK(Stat)->Name("usvfs/stat")->Setup(DoSetup)->Teardown(DoTeardown);
BENCHMARK(Stat_USVFS)
    ->Name("usvfs/stat_usvfs")
    ->Setup(DoSetup_usvfs)
    ->Teardown(DoTeardown_usvfs);

BENCHMARK(Open)->Name("usvfs/open")->Setup(DoSetup)->Teardown(DoTeardown);
BENCHMARK(Open_USVFS)
    ->Name("usvfs/open_usvfs")
    ->Setup(DoSetup_usvfs)
    ->Teardown(DoTeardown_usvfs);

BENCHMARK(Lseek)->Name("usvfs/lseek")->Setup(DoSetup)->Teardown(DoTeardown);
BENCHMARK(Lseek_USVFS)
    ->Name("usvfs/lseek_usvfs")
    ->Setup(DoSetup_usvfs)
    ->Teardown(DoTeardown_usvfs);

BENCHMARK(Sync)->Name("usvfs/sync")->Setup(DoSetup)->Teardown(DoTeardown);
BENCHMARK(Sync_USVFS)
    ->Name("usvfs/sync_usvfs")
    ->Setup(DoSetup_usvfs)
    ->Teardown(DoTeardown_usvfs);

BENCHMARK(Close)->Name("usvfs/close")->Setup(DoSetup)->Teardown(DoTeardown);
BENCHMARK(Close_USVFS)
    ->Name("usvfs/close_usvfs")
    ->Setup(DoSetup_usvfs)
    ->Teardown(DoTeardown_usvfs);

BENCHMARK(Read)->Name("usvfs/read")->Setup(DoSetup)->Teardown(DoTeardown);
BENCHMARK(Read_USVFS)
    ->Name("usvfs/read_usvfs")
    ->Setup(DoSetup_usvfs)
    ->Teardown(DoTeardown_usvfs);

BENCHMARK(Write)->Name("usvfs/write")->Setup(DoSetup)->Teardown(DoTeardown);
BENCHMARK(Write_USVFS)
    ->Name("usvfs/write_usvfs")
    ->Setup(DoSetup_usvfs)
    ->Teardown(DoTeardown_usvfs);

BENCHMARK(usvfsVirtualLinkDirectoryStatic)
    ->Name("usvfs/virtualLinkDirectoryStatic")
    ->UseManualTime()
    ->RangeMultiplier(10)
    ->Range(1, 10000);

}  // namespace benchmarks
