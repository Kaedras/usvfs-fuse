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

static void copyFiletree(benchmark::State& state)
{
  const VirtualFileTreeItem root("/", "/tmp", dir);
  for (auto _ : state) {
    auto copy = root;
    benchmark::DoNotOptimize(copy);
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

static void eraseFromFiletree(benchmark::State& state)
{
  for (auto _ : state) {
    auto root = VirtualFileTreeItem("/", "/tmp", dir);
    root.add("/a", "/tmp/a", dir);
    root.erase("/a");
  }
}

BENCHMARK(createFiletree);
BENCHMARK(copyFiletree);
BENCHMARK(addItemToFiletree);
BENCHMARK(findInFiletree);
BENCHMARK(eraseFromFiletree);

}  // namespace benchmarks
