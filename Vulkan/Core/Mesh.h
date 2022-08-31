#pragma once

#include <filesystem>
#include <vector>

#include "glm/glm.hpp"

struct Vertex
{
	glm::vec3 Position;
	glm::vec2 TexCoords;
};

class Mesh
{
public:
	Mesh(const std::filesystem::path& path);

	const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
	const std::vector<uint32_t>& GetIndices() const { return m_Indices; }

private:
	std::vector<Vertex> m_Vertices;
	std::vector<uint32_t> m_Indices;
};
