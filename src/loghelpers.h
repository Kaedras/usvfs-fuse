#pragma once

#include "usvfs-fuse/logging.h"
#include <spdlog/common.h>

spdlog::level::level_enum ConvertLogLevel(LogLevel level) noexcept;
LogLevel ConvertLogLevel(spdlog::level::level_enum level) noexcept;
