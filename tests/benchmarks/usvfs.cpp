#include "../../src/utils.h"
#include "benchmark_utils.h"
#include "constants.h"

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

namespace benchmarks
{

static void DoSetup_usvfs(const benchmark::State&)
{
  fs::create_directories(paths::mnt);
  fs::create_directories(paths::src / "0");

  char buffer[bufferSize];
  memset(buffer, 'A', sizeof(buffer));

  ofstream ofs(paths::file);
  ofs.write(buffer, sizeof(buffer));
  ofs.close();

  const auto usvfs = UsvfsManager::instance();
  usvfs->setLogFile("/tmp/usvfs.log");
  usvfs->setLogLevel(LogLevel::Warning);
  usvfs->virtualLinkDirectoryStatic((paths::src / "0").string(), paths::mnt.string(),
                                    0);
  usvfs->mount();
  this_thread::sleep_for(10ms);
}

static void DoSetup(const benchmark::State&)
{
  fs::create_directories(paths::file.parent_path());

  char buffer[bufferSize];
  memset(buffer, 'A', sizeof(buffer));

  ofstream ofs(paths::file);
  ofs.write(buffer, sizeof(buffer));
  ofs.close();
}

static void DoTeardown_usvfs(const benchmark::State&)
{
  const auto usvfs = UsvfsManager::instance();
  usvfs->unmount();
  this_thread::sleep_for(10ms);
  fs::remove_all(paths::base);
}

static void DoTeardown(const benchmark::State&)
{
  fs::remove_all(paths::base);
}

// stat
static void Stat(benchmark::State& state)
{
  struct stat st{};
  for (auto _ : state) {
    if (stat(paths::file.c_str(), &st) != 0) {
      assert(false);
    }
  }
}

static void Stat_USVFS(benchmark::State& state)
{
  struct stat st{};
  for (auto _ : state) {
    if (stat(paths::fileUsvfs.c_str(), &st) != 0) {
      assert(false);
    }
  }
}

// open
static void Open(benchmark::State& state)
{
  for (auto _ : state) {
    const int fd = open(paths::file.c_str(), O_RDONLY);

    state.PauseTiming();
    assert(fd != -1);
    close(fd);
    state.ResumeTiming();
  }
}

static void Open_USVFS(benchmark::State& state)
{
  for (auto _ : state) {
    const int fd = open(paths::fileUsvfs.c_str(), O_RDONLY);

    state.PauseTiming();
    assert(fd != -1);
    close(fd);
    state.ResumeTiming();
  }
}

// lseek
static void Lseek(benchmark::State& state)
{
  const int fd = open(paths::file.c_str(), O_RDONLY);
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
  const int fd = open(paths::file.c_str(), O_RDONLY);
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
  const int fd = open(paths::file.c_str(), O_RDONLY);
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
  const int fd = open(paths::file.c_str(), O_RDONLY);
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
    const int fd = open(paths::file.c_str(), O_RDONLY);
    assert(fd != -1);
    state.ResumeTiming();

    close(fd);
  }
}

static void Close_USVFS(benchmark::State& state)
{
  for (auto _ : state) {
    state.PauseTiming();
    const int fd = open(paths::fileUsvfs.c_str(), O_RDONLY);
    assert(fd != -1);
    state.ResumeTiming();

    close(fd);
  }
}

// read
static void Read(benchmark::State& state)
{
  char buffer[bufferSize];
  const int fd = open(paths::file.c_str(), O_RDONLY);
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
  const int fd = open(paths::fileUsvfs.c_str(), O_RDONLY);
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
  const int fd = open(paths::file.c_str(), O_WRONLY | O_TRUNC);
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
  const int fd = open(paths::fileUsvfs.c_str(), O_WRONLY | O_TRUNC);
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

}  // namespace benchmarks
