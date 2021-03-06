#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <vector>
#include <unordered_map>
#include <array>
#include <string>
#include "Mesh.h"

VkVertexInputBindingDescription Vertex::getBindingDescription()
{
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 3> Vertex::getAttributeDescriptions()
{
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, normal);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

    return attributeDescriptions;
}

bool Vertex::operator==(const Vertex &other) const
{
    return pos == other.pos && normal == other.normal && texCoord == other.texCoord;
}

Mesh::Mesh(MeshType type)
{
    switch (type)
    {
    case MeshType::ePlane:
        initPlane();
        break;
    default:
        return;
    }
}
void Mesh::initPlane()
{
    float quadVertices[] = {
        // positions        // texture Coords
        -1.0f,
        -1.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        -1.0f,
        0.0f,
        1.0f,
        0.0f,
        1.0f,
        1.0f,
        0.0f,
        1.0f,
        1.0f,
        -1.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
    };

    for (int i = 0; i < 4; i++)
    {
        Vertex v{};
        v.pos = glm::vec3(quadVertices[5 * i], quadVertices[5 * i + 1], quadVertices[5 * i + 2]);
        v.normal = glm::vec3(0.0f, 0.0f, 0.0f);
        v.texCoord = glm::vec2(quadVertices[5 * i + 3], quadVertices[5 * i + 4]);
        vertices.push_back(v);
    }

    indices = {0, 3, 2, 2, 1, 0};
}

Mesh::Mesh(std::string model_path)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, model_path.c_str()))
    {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    for (const auto &shape : shapes)
    {
        for (const auto &index : shape.mesh.indices)
        {
            Vertex vertex{};
            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]};

            vertex.normal = {
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2]};

            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]};

            if (uniqueVertices.count(vertex) == 0)
            {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }

            indices.push_back(uniqueVertices[vertex]);
        }
    }
}
