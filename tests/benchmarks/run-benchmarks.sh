#!/bin/sh
set -e

SCRIPT_PATH=$(dirname "$(realpath "$0")")
BENCHMARK=$(realpath "$SCRIPT_PATH/../../build/RelWithDebInfo/tests/benchmarks/usvfs-benchmarks")

mkdir -p "$SCRIPT_PATH/results"

# always executing the benchmark on the same core by using `taskset -c <core>` is important on heterogeneous CPUs
exec taskset -c 0 "$BENCHMARK" --benchmark_repetitions=20 --benchmark_min_warmup_time=1 \
  --benchmark_out="$SCRIPT_PATH/results/result-$(date -Iseconds).out" --benchmark_out_format=json
