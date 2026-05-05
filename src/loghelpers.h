#pragma once

#include "usvfs-fuse/logging.h"
#include <spdlog/common.h>

spdlog::level::level_enum ConvertLogLevel(LogLevel level);
LogLevel ConvertLogLevel(spdlog::level::level_enum level);
