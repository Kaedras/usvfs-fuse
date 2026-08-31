#pragma once

constexpr int bufferSize = 4096;

namespace paths
{
static const std::filesystem::path base =
    std::filesystem::temp_directory_path() / "usvfs";
static const std::filesystem::path src       = base / "src";
static const std::filesystem::path mnt       = base / "mnt";
static const std::filesystem::path file      = src / "0" / "0.txt";
static const std::filesystem::path fileUsvfs = mnt / "0.txt";
}  // namespace paths
