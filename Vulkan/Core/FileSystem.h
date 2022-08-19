#pragma once

#include "DataBuffer.h"
#include <filesystem>

namespace FileSystem
{
	bool Write(const std::filesystem::path& path, const DataBuffer& buffer);
	DataBuffer Read(const std::filesystem::path& path);
}
