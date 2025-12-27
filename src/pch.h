#pragma once

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <linux/openat2.h>
#include <memory>
#include <mutex>
#include <poll.h>
#include <print>
#include <ranges>
#include <sched.h>
#include <set>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

// Qt
#include <QProcess>

extern "C"
{
#include <sys/pidfd.h>
}

// icu
#include <unicode/unistr.h>

// spdlog
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>

// libfuse
#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 317
#endif
#include <fuse.h>
