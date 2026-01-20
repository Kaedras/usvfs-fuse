#include "../../src/virtualfiletreeitem.h"
#include "benchmark_utils.h"
#include <benchmark/benchmark.h>
#include <filesystem>
#include <iostream>

using namespace std;
namespace fs = std::filesystem;

#define CREATE_FILE_TREE_WITH_DEPTH()                                                  \
  auto root     = VirtualFileTreeItem::create("/", "/tmp", dir);                       \
  int64_t depth = state.range(0);                                                      \
  string path;                                                                         \
  for (int i = 0; i < depth; ++i) {                                                    \
    if (root->add(path + "/a", "/tmp" + path + "/a", dir) == nullptr) {                \
      state.SkipWithError("error building file tree");                                 \
    }                                                                                  \
    if (root->add(path + "/b", "/tmp" + path + "/b", dir) == nullptr) {                \
      state.SkipWithError("error building file tree");                                 \
    }                                                                                  \
    if (root->add(path + "/c", "/tmp" + path + "/c", dir) == nullptr) {                \
      state.SkipWithError("error building file tree");                                 \
    }                                                                                  \
    path += "/a";                                                                      \
  }

namespace benchmarks
{
static void createFiletree(benchmark::State& state)
{
  for (auto _ : state) {
    benchmark::DoNotOptimize(VirtualFileTreeItem::create("/", "/tmp", dir));
  }
}

static void copyEmptyFiletree(benchmark::State& state)
{
  auto root = VirtualFileTreeItem::create("/", "/tmp", dir);
  for (auto _ : state) {
    auto copy = root;
    benchmark::DoNotOptimize(copy);
  }
}

static void copyFiletree(benchmark::State& state)
{
  CREATE_FILE_TREE_WITH_DEPTH();

  for (auto _ : state) {
    auto copy = root->clone();
    benchmark::DoNotOptimize(copy);
  }
}

static void addItemToFiletree(benchmark::State& state)
{
  auto root = VirtualFileTreeItem::create("/", "/tmp", dir);
  for (auto _ : state) {
    auto copy = root->clone();
    START();
    copy->add("/a", "/tmp/a", dir);
    END();
  }
}

static void addMultipleItemsToFiletree(benchmark::State& state)
{
  int width = state.range(0);
  int depth = state.range(1);
  for (auto _ : state) {
    auto root = VirtualFileTreeItem::create("/", "/tmp", dir);
    START();
    function<void(const string&, const string&, int)> addLevel =
        [&](const string& virtualPath, const string& realPath, int currentDepth) {
          if (currentDepth > depth) {
            return;
          }

          for (int i = 0; i < width; ++i) {
            string childVirtualPath = virtualPath + "/" + to_string(i);
            string childRealPath    = realPath + "/" + to_string(i);

            root->add(childVirtualPath, childRealPath, dir);
            addLevel(childVirtualPath, childRealPath, currentDepth + 1);
          }
        };

    addLevel("", "/tmp", 1);
    END();
  }
}

static void findInFiletree(benchmark::State& state)
{
  CREATE_FILE_TREE_WITH_DEPTH();

  for (auto _ : state) {
    auto result = root->find(path);
    benchmark::DoNotOptimize(result);
  }
}

static void eraseFromFiletree(benchmark::State& state)
{
  CREATE_FILE_TREE_WITH_DEPTH();

  for (auto _ : state) {
    auto copy = root->clone();
    START();
    copy->erase(path);
    END();
  }
}

static void mergeFiletrees(benchmark::State& state)
{
  auto a = VirtualFileTreeItem::create("/", "/tmp", dir);

  a->add("/a", "/tmp/a", dir);
  a->add("/b", "/tmp/b", dir);
  a->add("/c", "/tmp/c", dir);

  auto b = VirtualFileTreeItem::create("/", "/tmp", dir);
  b->add("/a", "/tmp/a", dir);
  b->add("/a/a", "/tmp/a/a", dir);
  b->add("/c", "/tmp/3", dir);
  b->add("/d", "/tmp/d", dir);

  for (auto _ : state) {
    auto merged = a;
    START();
    *merged += *b;
    benchmark::DoNotOptimize(merged);
    END();
  }
}

BENCHMARK(createFiletree)->Name("filetree/create");
BENCHMARK(copyEmptyFiletree)->Name("filetree/copyEmpty");
BENCHMARK(copyFiletree)->Name("filetree/copy")->DenseRange(1, 10);
BENCHMARK(addItemToFiletree)->Name("filetree/add")->UseManualTime();
BENCHMARK(addMultipleItemsToFiletree)
    ->Name("filetree/addMultiple")
    ->UseManualTime()
    ->ArgsProduct({benchmark::CreateDenseRange(1, 5, 1),
                   benchmark::CreateDenseRange(1, 5, 1)});
BENCHMARK(findInFiletree)->Name("filetree/find")->DenseRange(1, 10);
BENCHMARK(eraseFromFiletree)
    ->Name("filetree/erase")
    ->UseManualTime()
    ->DenseRange(1, 10);
BENCHMARK(mergeFiletrees)->Name("filetree/merge")->UseManualTime();

}  // namespace benchmarks
