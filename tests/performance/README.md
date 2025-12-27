# Reducing variance
See [here](https://github.com/google/benchmark/blob/main/docs/reducing_variance.md) how to reduce run variance

# Example run

## Command:
```shell
# set cpu governor to performance
sudo cpupower frequency-set -g performance
# disable cpu boosting
echo 0 | sudo tee /sys/devices/system/cpu/cpufreq/boost
# run benchmark on cpu 0
taskset -c 0 ./usvfs-performance-tests --benchmark_repetitions=20 --benchmark_min_warmup_time=2 --benchmark_report_aggregates_only=true
```

## Output
```
2025-12-27T16:37:01+01:00
Running ./usvfs-performance-tests
Run on (32 X 4340.27 MHz CPU s)
CPU Caches:
  L1 Data 48 KiB (x16)
  L1 Instruction 32 KiB (x16)
  L2 Unified 1024 KiB (x16)
  L3 Unified 98304 KiB (x2)
Load Average: 1.17, 1.29, 1.62
---------------------------------------------------------------------
Benchmark                           Time             CPU   Iterations
---------------------------------------------------------------------
open_mean                         467 ns          467 ns           20
open_median                       465 ns          465 ns           20
open_stddev                      4.80 ns         4.80 ns           20
open_cv                          1.03 %          1.03 %            20
usvfs_open_mean                   461 ns          461 ns           20
usvfs_open_median                 460 ns          459 ns           20
usvfs_open_stddev                5.45 ns         5.46 ns           20
usvfs_open_cv                    1.18 %          1.18 %            20
iequals/ascii_mean               9.37 ns         9.36 ns           20
iequals/ascii_median             9.39 ns         9.39 ns           20
iequals/ascii_stddev            0.124 ns        0.124 ns           20
iequals/ascii_cv                 1.33 %          1.33 %            20
iequals/unicode_mean             34.1 ns         34.0 ns           20
iequals/unicode_median           34.1 ns         34.0 ns           20
iequals/unicode_stddev          0.004 ns        0.003 ns           20
iequals/unicode_cv               0.01 %          0.01 %            20
iendsWith/unicode_mean           52.5 ns         52.4 ns           20
iendsWith/unicode_median         52.5 ns         52.4 ns           20
iendsWith/unicode_stddev        0.047 ns        0.046 ns           20
iendsWith/unicode_cv             0.09 %          0.09 %            20
istartsWith_mean                 47.4 ns         47.3 ns           20
istartsWith_median               47.4 ns         47.4 ns           20
istartsWith_stddev              0.221 ns        0.221 ns           20
istartsWith_cv                   0.47 %          0.47 %            20
toLower_mean                     47.6 ns         47.6 ns           20
toLower_median                   47.6 ns         47.6 ns           20
toLower_stddev                  0.031 ns        0.031 ns           20
toLower_cv                       0.06 %          0.06 %            20
toUpper_mean                     46.6 ns         46.6 ns           20
toUpper_median                   46.6 ns         46.6 ns           20
toUpper_stddev                  0.027 ns        0.027 ns           20
toUpper_cv                       0.06 %          0.06 %            20
getParentPath_mean               3.23 ns         3.23 ns           20
getParentPath_median             3.23 ns         3.23 ns           20
getParentPath_stddev            0.001 ns        0.001 ns           20
getParentPath_cv                 0.02 %          0.02 %            20
getFileNameFromPath_mean         2.03 ns         2.03 ns           20
getFileNameFromPath_median       2.03 ns         2.03 ns           20
getFileNameFromPath_stddev      0.003 ns        0.003 ns           20
getFileNameFromPath_cv           0.13 %          0.13 %            20
createFiletree_mean              31.6 ns         31.6 ns           20
createFiletree_median            31.5 ns         31.5 ns           20
createFiletree_stddev           0.179 ns        0.179 ns           20
createFiletree_cv                0.57 %          0.57 %            20
addItemToFiletree_mean            162 ns          162 ns           20
addItemToFiletree_median          162 ns          162 ns           20
addItemToFiletree_stddev        0.251 ns        0.251 ns           20
addItemToFiletree_cv             0.15 %          0.15 %            20
findInFiletree_mean              82.9 ns         82.8 ns           20
findInFiletree_median            82.8 ns         82.8 ns           20
findInFiletree_stddev           0.206 ns        0.208 ns           20
findInFiletree_cv                0.25 %          0.25 %            20
```
