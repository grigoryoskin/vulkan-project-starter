#pragma once

#include <vector>
#include <unordered_map>
#include <array>
#include <string>
#include "../utils/glm.h"
#include "../utils/vulkan.h"

struct SharedUniformBufferObject
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 lightPos;
};

struct UniformBufferObject
{
    UniformBufferObject() {}
    UniformBufferObject(const glm::mat4 &model) : model(model) {}
    glm::mat4 model;
};

enum class MeshType
{
    ePlane,
    eCube
};

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();

    bool operator==(const Vertex &other) const;
};

namespace std
{
    template <>
    struct hash<Vertex>
    {
        size_t operator()(Vertex const &vertex) const
        {
            return ((hash<glm::vec3>()(vertex.pos) ^
                     (hash<glm::vec3>()(vertex.normal) << 1)) >>
                    1) ^
                   (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}

struct Mesh
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    Mesh() = default;

    Mesh(std::string model_path);

    Mesh(MeshType type);

    void initPlane();
};
