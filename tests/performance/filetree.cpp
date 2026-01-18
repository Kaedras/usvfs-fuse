#include "../../src/virtualfiletreeitem.h"
#include <benchmark/benchmark.h>
#include <filesystem>
#include <iostream>

using namespace std;
namespace fs = std::filesystem;

#define START() auto start = std::chrono::high_resolution_clock::now()
#define END()                                                                          \
  auto end = std::chrono::high_resolution_clock::now();                                \
  auto elapsed_seconds =                                                               \
      std::chrono::duration_cast<std::chrono::duration<double>>(end - start);          \
  state.SetIterationTime(elapsed_seconds.count())

#define CREATE_FILE_TREE_WITH_DEPTH()                                                  \
  VirtualFileTreeItem root("/", "/tmp", dir);                                          \
  int64_t depth = state.range(0);                                                      \
  string path;                                                                         \
  for (int i = 0; i < depth; ++i) {                                                    \
    if (root.add(path + "/a", "/tmp" + path + "/a", dir) == nullptr) {                 \
      state.SkipWithError("error building file tree");                                 \
    }                                                                                  \
    if (root.add(path + "/b", "/tmp" + path + "/b", dir) == nullptr) {                 \
      state.SkipWithError("error building file tree");                                 \
    }                                                                                  \
    if (root.add(path + "/c", "/tmp" + path + "/c", dir) == nullptr) {                 \
      state.SkipWithError("error building file tree");                                 \
    }                                                                                  \
    path += "/a";                                                                      \
  }

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
  CREATE_FILE_TREE_WITH_DEPTH();

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
    START();
    copy.add("/a", "/tmp/a", dir);
    END();
  }
}

static void addMultipleItemsToFiletree(benchmark::State& state)
{
  const VirtualFileTreeItem root("/", "/tmp", dir);
  for (auto _ : state) {
    auto copy = root;
    START();
    copy.add("/a", "/tmp/a", dir);
    copy.add("/a/a", "/tmp/a/a", file);
    copy.add("/a/a/a", "/tmp/a/a/a", file);
    END();
  }
}

static void findInFiletree(benchmark::State& state)
{
  CREATE_FILE_TREE_WITH_DEPTH();

  for (auto _ : state) {
    auto result = root.find(path);
    benchmark::DoNotOptimize(result);
  }
}

static void eraseFromFiletree(benchmark::State& state)
{
  CREATE_FILE_TREE_WITH_DEPTH();

  for (auto _ : state) {
    auto copy = root;
    START();
    copy.erase(path);
    END();
  }
}

static void mergeFiletrees(benchmark::State& state)
{
  VirtualFileTreeItem a("/", "/tmp", dir);
  a.add("/a", "/tmp/a", dir);
  a.add("/b", "/tmp/b", dir);
  a.add("/c", "/tmp/c", dir);

  VirtualFileTreeItem b("/", "/tmp", dir);
  b.add("/a", "/tmp/a", dir);
  b.add("/a/a", "/tmp/a/a", dir);
  b.add("/c", "/tmp/3", dir);
  b.add("/d", "/tmp/d", dir);

  for (auto _ : state) {
    auto merged = a;
    START();
    merged += b;
    benchmark::DoNotOptimize(merged);
    END();
  }
}

BENCHMARK(createFiletree)->Name("filetree/create");
BENCHMARK(copyEmptyFiletree)->Name("filetree/copyEmpty");
BENCHMARK(copyFiletree)->Name("filetree/copy")->DenseRange(1, 10);
BENCHMARK(addItemToFiletree)->Name("filetree/add")->UseManualTime();
BENCHMARK(addMultipleItemsToFiletree)->Name("filetree/addMultiple")->UseManualTime();
BENCHMARK(findInFiletree)->Name("filetree/find")->DenseRange(1, 10);
BENCHMARK(eraseFromFiletree)
    ->Name("filetree/erase")
    ->UseManualTime()
    ->DenseRange(1, 10);
BENCHMARK(mergeFiletrees)->Name("filetree/merge")->UseManualTime();

}  // namespace benchmarks
