#include "Mesh.h"

#include "../tiny_obj_loader.h"

#include <string>
#include <iostream>

Mesh::Mesh(const std::filesystem::path& path)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.u8string().c_str()))
	{
		std::cerr << "Failed to load mesh: " << path << '\n';
		return;
	}

	for (auto& shape : shapes)
	{
		for (auto& index : shape.mesh.indices)
		{
			Vertex& vertex = m_Vertices.emplace_back();
			vertex.Position = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2],
			};
			vertex.TexCoords = {
				attrib.texcoords[2 * index.texcoord_index + 0],
				1.f - attrib.texcoords[2 * index.texcoord_index + 1]
			};
			m_Indices.push_back((uint32_t)m_Indices.size());
		}
	}
}
