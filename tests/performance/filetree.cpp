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

static void copyEmptyFiletree(benchmark::State& state)
{
  VirtualFileTreeItem root("/", "/tmp", dir);
  for (auto _ : state) {
    auto copy = root;
    benchmark::DoNotOptimize(copy);
  }
}

static void copyFiletree(benchmark::State& state)
{
  VirtualFileTreeItem root("/", "/tmp", dir);
  root.add("/a", "/tmp/a", dir);
  root.add("/a/a", "/tmp/a/a", file);
  for (auto _ : state) {
    auto copy = root;
    benchmark::DoNotOptimize(copy);
  }
}

static void addItemToFiletree(benchmark::State& state)
{
  const VirtualFileTreeItem root("/", "/tmp", dir);
  for (auto _ : state) {
    auto copy = root;
    copy.add("/a", "/tmp/a", dir);
    copy.add("/a/a", "/tmp/a/a", file);
  }
}

static void findInFiletree(benchmark::State& state)
{
  VirtualFileTreeItem root("/", "/tmp", dir);
  root.add("/a", "/tmp/a", file);

  for (auto _ : state) {
    auto result = root.find("/a");
    benchmark::DoNotOptimize(result);
  }
}

static void eraseFromFiletree(benchmark::State& state)
{
  VirtualFileTreeItem root("/", "/tmp", dir);
  root.add("/a", "/tmp/a", dir);
  for (auto _ : state) {
    auto copy = root;
    copy.erase("/a");
  }
}

static void mergeFileTrees(benchmark::State& state)
{
  VirtualFileTreeItem a("/", "/tmp", dir);
  a.add("/a", "/tmp/a", dir);
  a.add("/b", "/tmp/b", dir);

  VirtualFileTreeItem b("/", "/tmp", dir);
  b.add("/c", "/tmp/c", dir);
  b.add("/d", "/tmp/d", dir);

  for (auto _ : state) {
    auto merged = a;
    merged += b;
    benchmark::DoNotOptimize(merged);
  }
}

BENCHMARK(createFiletree);
BENCHMARK(copyEmptyFiletree);
BENCHMARK(copyFiletree);
BENCHMARK(addItemToFiletree);
BENCHMARK(findInFiletree);
BENCHMARK(eraseFromFiletree);
BENCHMARK(mergeFileTrees);

}  // namespace benchmarks
