#include "usvfs-fuse/virtualfiletreeitem.h"
#include <benchmark/benchmark.h>
#include <filesystem>
#include <iostream>

using namespace std;
namespace fs = std::filesystem;

namespace benchmarks
{
static void createFiletree(benchmark::State& state)
{
  for (auto _ : state) {
    VirtualFileTreeItem root("/", "/tmp", dir);
  }
}

static void addItemToFiletree(benchmark::State& state)
{
  for (auto _ : state) {
    auto root = VirtualFileTreeItem("/", "/tmp", dir);
    root.add("/a", "/tmp/a", file);
  }
}

static void findInFiletree(benchmark::State& state)
{
  auto root = VirtualFileTreeItem("/", "/tmp", dir);
  root.add("/a", "/tmp/a", file);

  for (auto _ : state) {
    auto result = root.find("/a");
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(createFiletree);
BENCHMARK(addItemToFiletree);
BENCHMARK(findInFiletree);

}  // namespace benchmarks
