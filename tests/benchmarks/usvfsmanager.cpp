#include "../../src/utils.h"
#include "benchmark_utils.h"

#include "usvfs-fuse/usvfsmanager.h"

#include "constants.h"

#include <benchmark/benchmark.h>
#include <cmath>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

namespace
{
void usvfsVirtualLinkDirectoryStatic(benchmark::State& state)
{
  const int count            = static_cast<int>(state.range(0));
  static constexpr int depth = 2;
  static constexpr int width = 10;

  fs::create_directories(paths::mnt);
  const auto usvfs = UsvfsManager::instance();
  usvfs->setLogLevel(LogLevel::Info);

  createModFilesOnDisk(paths::src, width, depth, count);

  // calculate the total item count
  const double fileCount  = (1 + width * (pow(width, depth) - 1) / (width - 1)) * count;
  state.counters["files"] = fileCount;

  for (auto _ : state) {
    START();
    for (int i = 0; i < count; ++i) {
      usvfs->virtualLinkDirectoryStatic(paths::src / to_string(i), paths::mnt);
    }
    END();
    usvfs->clearVirtualMappings();
  }
  fs::remove_all(paths::base);
}
}  // namespace

BENCHMARK(usvfsVirtualLinkDirectoryStatic)
    ->Name("usvfsManager/virtualLinkDirectoryStatic")
    ->UseManualTime()
    ->RangeMultiplier(10)
    ->Range(1, 10000);
