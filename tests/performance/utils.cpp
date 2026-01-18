#include "../../src/utils.h"
#include "benchmark_utils.h"
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
static void toLowerInplace(benchmark::State& state, Args&&... args)
{
  auto args_tuple = std::make_tuple(std::move(args)...);
  for (auto _ : state) {
    string str = get<0>(args_tuple);
    START();
    ::toLowerInplace(str);
    benchmark::DoNotOptimize(str);
    END();
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

template <class... Args>
static void toUpperInplace(benchmark::State& state, Args&&... args)
{
  auto args_tuple = std::make_tuple(std::move(args)...);
  for (auto _ : state) {
    string str = get<0>(args_tuple);
    START();
    ::toUpperInplace(str);
    benchmark::DoNotOptimize(str);
    END();
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
BENCHMARK_CAPTURE(toLower, ascii, "ABCDEFGHIJKLMNOPQRST")->Name("utils/toLower/ascii");
BENCHMARK_CAPTURE(toLower, unicode, "ÄÜöabC/テスト/жзИЙ/ԱբգԴ")
    ->Name("utils/toLower/unicode");
BENCHMARK_CAPTURE(toLowerInplace, ascii, "ABCDEFGHIJKLMNOPQRST")
    ->Name("utils/toLowerInplace/ascii")
    ->UseManualTime();
BENCHMARK_CAPTURE(toLowerInplace, unicode, "ÄÜöabC/テスト/жзИЙ/ԱբգԴ")
    ->Name("utils/toLowerInplace/unicode")
    ->UseManualTime();
BENCHMARK_CAPTURE(toUpper, ascii, "abcdefghijklmnopqrst")->Name("utils/toUpper/ascii");
BENCHMARK_CAPTURE(toUpper, unicode, "ÄÜöabC/テスト/жзИЙ/ԱբգԴ")
    ->Name("utils/toUpper/unicode");
BENCHMARK_CAPTURE(toUpperInplace, ascii, "abcdefghijklmnopqrst")
    ->Name("utils/toUpperInplace/ascii")
    ->UseManualTime();
BENCHMARK_CAPTURE(toUpperInplace, unicode, "ÄÜöabC/テスト/жзИЙ/ԱբգԴ")
    ->Name("utils/toUpperInplace/unicode")
    ->UseManualTime();
BENCHMARK(getParentPath)->Name("utils/getParentPath");
BENCHMARK(getFileNameFromPath)->Name("utils/getFileNameFromPath");
BENCHMARK(createEnv)->Name("utils/createEnv");

}  // namespace benchmarks
