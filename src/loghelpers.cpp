#include "loghelpers.h"

spdlog::level::level_enum ConvertLogLevel(LogLevel level)
{
  switch (level) {
  case LogLevel::Trace:
    return spdlog::level::trace;
  case LogLevel::Debug:
    return spdlog::level::debug;
  case LogLevel::Info:
    return spdlog::level::info;
  case LogLevel::Warning:
    return spdlog::level::warn;
  case LogLevel::Error:
    return spdlog::level::err;
  default:
    return spdlog::level::debug;
  }
}

LogLevel ConvertLogLevel(spdlog::level::level_enum level)
{
  switch (level) {
  case spdlog::level::trace:
    return LogLevel::Trace;
  case spdlog::level::debug:
    return LogLevel::Debug;
  case spdlog::level::info:
    return LogLevel::Info;
  case spdlog::level::warn:
    return LogLevel::Warning;
  case spdlog::level::err:
    return LogLevel::Error;
  default:
    return LogLevel::Debug;
  }
}
