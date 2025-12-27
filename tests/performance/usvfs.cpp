#include <benchmark/benchmark.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>
#include <thread>

#include "usvfs_fuse/usvfsmanager.h"

using namespace std;
namespace fs = std::filesystem;

namespace benchmarks
{
static const fs::path base = fs::temp_directory_path() / "usvfs";
static const fs::path src  = base / "src";
static const fs::path mnt  = base / "mnt";
static const fs::path file = src / "0" / "0.txt";

static void DoSetup_usvfs(const benchmark::State& state)
{
  spdlog::set_level(spdlog::level::warn);
  fs::create_directories(mnt);
  fs::create_directories(src / "0");

  ofstream ofs(file);
  ofs << "test";

  auto usvfs = UsvfsManager::instance();
  spdlog::get("usvfs")->set_level(spdlog::level::warn);
  usvfs->usvfsVirtualLinkDirectoryStatic((src / "0").c_str(), mnt.c_str(),
                                         linkFlag::RECURSIVE);
  usvfs->mount();
  this_thread::sleep_for(10ms);
}

static void DoSetup(const benchmark::State& state)
{
  fs::create_directories(file.parent_path());

  ofstream ofs(file);
  ofs << "test";
}

static void DoTeardown_usvfs(const benchmark::State& state)
{
  auto usvfs = UsvfsManager::instance();
  usvfs->unmount();
  fs::remove_all(base);
}

static void DoTeardown(const benchmark::State& state)
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

BENCHMARK(open)->Setup(DoSetup)->Teardown(DoTeardown);
BENCHMARK(open)->Name("usvfs_open")->Setup(DoSetup_usvfs)->Teardown(DoTeardown_usvfs);

}  // namespace benchmarks
