#pragma once

// log functions to prevent segfault when spdlog::get("usvfs") returns nullptr
namespace logger
{
template <typename... Args>
void log(spdlog::level::level_enum level, spdlog::format_string_t<Args...> fmt,
         Args&&... args)
{
  auto logger = spdlog::get("usvfs");
  if (logger == nullptr) {
    logger = spdlog::create<spdlog::sinks::stdout_color_sink_mt>("usvfs");
    logger->set_pattern("%H:%M:%S.%e [%L] %v");
    logger->set_level(spdlog::level::info);
  } else {
    logger->log(level, fmt, std::forward<Args>(args)...);
  }
}

template <typename... Args>
void trace(spdlog::format_string_t<Args...> fmt, Args&&... args)
{
  log(spdlog::level::trace, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void debug(spdlog::format_string_t<Args...> fmt, Args&&... args)
{
  log(spdlog::level::debug, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void info(spdlog::format_string_t<Args...> fmt, Args&&... args)
{
  log(spdlog::level::info, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void warn(spdlog::format_string_t<Args...> fmt, Args&&... args)
{
  log(spdlog::level::warn, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void error(spdlog::format_string_t<Args...> fmt, Args&&... args)
{
  log(spdlog::level::err, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void critical(spdlog::format_string_t<Args...> fmt, Args&&... args)
{
  log(spdlog::level::critical, fmt, std::forward<Args>(args)...);
}
}  // namespace logger
