#include <benchmark/benchmark.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <thread>

#include "usvfs-fuse/usvfsmanager.h"

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
  usvfs->usvfsVirtualLinkDirectoryStatic((src / "0").c_str(), mnt.c_str(),
                                         linkFlag::RECURSIVE);
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

BENCHMARK(open)->Name("usvfs/open")->Setup(DoSetup)->Teardown(DoTeardown);
BENCHMARK(open)
    ->Name("usvfs/usvfs_open")
    ->Setup(DoSetup_usvfs)
    ->Teardown(DoTeardown_usvfs);

}  // namespace benchmarks
