#pragma once
#include "bundle.h"
#include "cmmn.h"
#include <reactphysics3d/collision/PolygonVertexArray.h>

using geom_file::vertex;

constexpr static vk::VertexInputAttributeDescription vertex_attribute_description[] = {
    vk::VertexInputAttributeDescription{
                                        0, 0, vk::Format::eR32G32B32Sfloat, offsetof(vertex, position)},
    vk::VertexInputAttributeDescription{
                                        1, 0, vk::Format::eR32G32B32Sfloat, offsetof(vertex, normal)  },
    vk::VertexInputAttributeDescription{
                                        2, 0, vk::Format::eR32G32Sfloat,    offsetof(vertex, texcoord)},
};

#include "device.h"

struct mesh {
    std::unique_ptr<buffer> vertex_buffer;
    std::unique_ptr<buffer> index_buffer;
    uint32_t                vertex_count, index_count;

    mesh(
        device*                           dev,
        uint32_t                          vcount,
        size_t                            vsize,
        uint32_t                          icount,
        const std::function<void(void*)>& write_buffer
    );

    template<typename VertexT>
    mesh(device* dev, const std::vector<VertexT>& vertices, const std::vector<uint16>& indices)
        : mesh(
            dev,
            (uint32_t)vertices.size(),
            sizeof(VertexT),
            (uint32_t)indices.size(),
            [&](void* staging_map) {
                memcpy(staging_map, vertices.data(), sizeof(VertexT) * vertices.size());
                memcpy(
                    (char*)staging_map + sizeof(VertexT) * vertices.size(),
                    indices.data(),
                    sizeof(uint16) * indices.size()
                );
            }
        ) {}
};
