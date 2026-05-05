#include "../../src/utils.h"
#include "benchmark_utils.h"

#include "usvfs-fuse/usvfsmanager.h"
#include <benchmark/benchmark.h>
#include <cmath>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <unistd.h>

using namespace std;
namespace fs = std::filesystem;

namespace benchmarks
{
static const fs::path base = fs::temp_directory_path() / "usvfs";
static const fs::path src  = base / "src";
static const fs::path mnt  = base / "mnt";
static const fs::path file = src / "0" / "0.txt";

static void DoSetup_usvfs(const benchmark::State&)
{
  fs::create_directories(mnt);
  fs::create_directories(src / "0");

  ofstream ofs(file);
  ofs << "test";

  auto usvfs = UsvfsManager::instance();
  usvfs->setLogLevel(LogLevel::Warning);
  usvfs->usvfsVirtualLinkDirectoryStatic((src / "0").c_str(), mnt.c_str(), 0);
  usvfs->mount();
  this_thread::sleep_for(10ms);
}

static void DoSetup(const benchmark::State&)
{
  fs::create_directories(file.parent_path());

  ofstream ofs(file);
  ofs << "test";
}

static void DoTeardown_usvfs(const benchmark::State&)
{
  auto usvfs = UsvfsManager::instance();
  usvfs->unmount();
  fs::remove_all(base);
}

static void DoTeardown(const benchmark::State&)
{
  fs::remove_all(base);
}

static void open(benchmark::State& state)
{
  for (auto _ : state) {
    int fd = ::open(file.c_str(), O_RDONLY);
    close(fd);
  }
}

static void usvfsVirtualLinkDirectoryStatic(benchmark::State& state)
{
  const int count            = static_cast<int>(state.range(0));
  static constexpr int depth = 2;
  static constexpr int width = 10;

  fs::create_directories(mnt);
  auto usvfs = UsvfsManager::instance();
  usvfs->setLogLevel(LogLevel::Info);

  createModFilesOnDisk(src, width, depth, count);

  // calculate the total item count
  const double fileCount  = (1 + width * (pow(width, depth) - 1) / (width - 1)) * count;
  state.counters["files"] = fileCount;

  for (auto _ : state) {
    START();
    for (int i = 0; i < count; ++i) {
      usvfs->usvfsVirtualLinkDirectoryStatic(src / to_string(i), mnt);
    }
    END();
    usvfs->usvfsClearVirtualMappings();
  }
  fs::remove_all(mnt);
}

BENCHMARK(open)->Name("usvfs/open")->Setup(DoSetup)->Teardown(DoTeardown);
BENCHMARK(open)
    ->Name("usvfs/usvfs_open")
    ->Setup(DoSetup_usvfs)
    ->Teardown(DoTeardown_usvfs);
BENCHMARK(usvfsVirtualLinkDirectoryStatic)
    ->Name("usvfs/virtualLinkDirectoryStatic")
    ->UseManualTime()
    ->RangeMultiplier(10)
    ->Range(1, 10000);

}  // namespace benchmarks
