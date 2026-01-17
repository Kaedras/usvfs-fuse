#include "usvfs-fuse/utils.h"
#include <benchmark/benchmark.h>

using namespace std;

namespace benchmarks
{
template <class... Args>
static void iequals(benchmark::State& state, Args&&... args)
{
  auto args_tuple = std::make_tuple(std::move(args)...);
  for (auto _ : state) {
    auto result = ::iequals(get<0>(args_tuple), get<0>(args_tuple));
    benchmark::DoNotOptimize(result);
  }
}

template <class... Args>
static void iendsWith(benchmark::State& state, Args&&... args)
{
  auto args_tuple = std::make_tuple(std::move(args)...);
  for (auto _ : state) {
    auto result = ::iendsWith(get<0>(args_tuple), get<0>(args_tuple));
    benchmark::DoNotOptimize(result);
  }
}

template <class... Args>
static void istartsWith(benchmark::State& state, Args&&... args)
{
  auto args_tuple = std::make_tuple(std::move(args)...);
  for (auto _ : state) {
    auto result = ::iendsWith(get<0>(args_tuple), get<0>(args_tuple));
    benchmark::DoNotOptimize(result);
  }
}

template <class... Args>
static void toLower(benchmark::State& state, Args&&... args)
{
  auto args_tuple = std::make_tuple(std::move(args)...);
  for (auto _ : state) {
    auto result = ::toLower(get<0>(args_tuple));
    benchmark::DoNotOptimize(result);
  }
}

template <class... Args>
static void toUpper(benchmark::State& state, Args&&... args)
{
  auto args_tuple = std::make_tuple(std::move(args)...);
  for (auto _ : state) {
    auto result = ::toUpper(get<0>(args_tuple));
    benchmark::DoNotOptimize(result);
  }
}

static void getParentPath(benchmark::State& state)
{
  for (auto _ : state) {
    auto result = ::getParentPath("/a/b/c");
    benchmark::DoNotOptimize(result);
  }
}

static void getFileNameFromPath(benchmark::State& state)
{
  for (auto _ : state) {
    auto result = ::getFileNameFromPath("/a/b/c");
    benchmark::DoNotOptimize(result);
  }
}

static void createEnv(benchmark::State& state)
{
  for (auto _ : state) {
    auto result = ::createEnv();
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK_CAPTURE(iequals, ascii, "abc", "aBC")->Name("utils/iequals/ascii");
BENCHMARK_CAPTURE(iequals, unicode, "テストtest", "テストtESt")
    ->Name("utils/iequals/unicode");
BENCHMARK_CAPTURE(iendsWith, ascii, "test", "ST")->Name("utils/iendsWith/ascii");
BENCHMARK_CAPTURE(iendsWith, unicode, "テストtest", "ストtEST")
    ->Name("utils/iendsWith/unicode");
BENCHMARK_CAPTURE(istartsWith, ascii, "abc", "AB")->Name("utils/istartsWith/ascii");
BENCHMARK_CAPTURE(istartsWith, unicode, "テストtest", "テストT")
    ->Name("utils/istartsWith/unicode");
BENCHMARK_CAPTURE(toLower, ascii, "ABC")->Name("utils/toLower/ascii");
BENCHMARK_CAPTURE(toLower, unicode, "テスト")->Name("utils/toLower/unicode");
BENCHMARK_CAPTURE(toUpper, ascii, "abc")->Name("utils/toUpper/ascii");
BENCHMARK_CAPTURE(toUpper, unicode, "テスト")->Name("utils/toUpper/unicode");
BENCHMARK(getParentPath)->Name("utils/getParentPath");
BENCHMARK(getFileNameFromPath)->Name("utils/getFileNameFromPath");
BENCHMARK(createEnv)->Name("utils/createEnv");

}  // namespace benchmarks
