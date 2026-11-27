#pragma once
#include <filesystem>
struct PrototypeCheckpoint;
bool WritePrototypeCheckpoint(const std::filesystem::path& path, const PrototypeCheckpoint& state);
bool ReadPrototypeCheckpoint(const std::filesystem::path& path, PrototypeCheckpoint& state);
