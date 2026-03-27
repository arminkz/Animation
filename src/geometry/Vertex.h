#pragma once

#include "stdafx.h"

struct Vertex {
    glm::vec3 pos;
    glm::vec4 color;
    glm::vec2 texCoord;
    glm::vec3 normal;
    glm::vec3 tangent;

    static VkVertexInputBindingDescription getBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

    bool operator==(const Vertex& other) const;
};


// Hash function for Vertex struct
namespace std {
    // Specialize std::hash for Vertex to allow it to be used in unordered_map and unordered_set
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            size_t h1 = hash<glm::vec3>()(vertex.pos);
            size_t h2 = hash<glm::vec4>()(vertex.color);
            size_t h3 = hash<glm::vec2>()(vertex.texCoord);
            size_t h4 = hash<glm::vec3>()(vertex.normal);
            size_t h5 = hash<glm::vec3>()(vertex.tangent);

            // Combine hashes (a common pattern for combining multiple hashes)
            size_t combined = h1;
            combined ^= h2 + 0x9e3779b9 + (combined << 6) + (combined >> 2);
            combined ^= h3 + 0x9e3779b9 + (combined << 6) + (combined >> 2);
            combined ^= h4 + 0x9e3779b9 + (combined << 6) + (combined >> 2);
            combined ^= h5 + 0x9e3779b9 + (combined << 6) + (combined >> 2);

            return combined;
        }
    };
}