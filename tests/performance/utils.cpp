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

static void toLower(benchmark::State& state)
{
  for (auto _ : state) {
    auto result = ::toLower("ABC");
    benchmark::DoNotOptimize(result);
  }
}
static void toUpper(benchmark::State& state)
{
  for (auto _ : state) {
    auto result = ::toUpper("abc");
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

BENCHMARK_CAPTURE(iequals, ascii, "abc", "aBC");
BENCHMARK_CAPTURE(iequals, unicode, "テストtest", "テストtESt");
BENCHMARK_CAPTURE(iendsWith, ascii, "test", "ST");
BENCHMARK_CAPTURE(iendsWith, unicode, "テストtest", "ストtEST");
BENCHMARK_CAPTURE(istartsWith, ascii, "abc", "AB");
BENCHMARK_CAPTURE(istartsWith, unicode, "テストtest", "テストT");
BENCHMARK(toLower);
BENCHMARK(toUpper);
BENCHMARK(getParentPath);
BENCHMARK(getFileNameFromPath);
BENCHMARK(createEnv);

}  // namespace benchmarks
