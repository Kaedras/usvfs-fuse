#pragma once

#include <chrono>

#define START() auto start = std::chrono::high_resolution_clock::now()
#define END()                                                                          \
  auto end = std::chrono::high_resolution_clock::now();                                \
  auto elapsed_seconds =                                                               \
      std::chrono::duration_cast<std::chrono::duration<double>>(end - start);          \
  state.SetIterationTime(elapsed_seconds.count())
