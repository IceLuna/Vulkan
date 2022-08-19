#include "FileSystem.h"

#include <fstream>

namespace FileSystem
{
	bool Write(const std::filesystem::path& path, const DataBuffer& buffer)
	{
		if (!std::filesystem::exists(path))
			std::filesystem::create_directories(path.parent_path());

		std::ofstream stream(path, std::ios::binary | std::ios::trunc);

		if (!stream)
		{
			stream.close();
			return false;
		}

		stream.write((const char*)buffer.Data, buffer.Size);
		stream.close();
		return true;
	}

	DataBuffer Read(const std::filesystem::path& path)
	{
		std::ifstream stream(path, std::ios::binary | std::ios::ate);

		std::streampos end = stream.tellg();
		stream.seekg(0, std::ios::beg);
		size_t size = end - stream.tellg();
		assert(size != 0); // "Empty file"

		DataBuffer buffer;
		buffer.Allocate(size);
		stream.read((char*)buffer.Data, buffer.Size);

		return buffer;
	}
}