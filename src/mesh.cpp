#include "mesh.h"
#include "geometry_set.h"
#include "renderer.h"

mesh::mesh(
    device*                           dev,
    uint32_t                          vcount,
    size_t                            _vsize,
    uint32_t                          icount,
    const std::function<void(void*)>& write_buffer
)
    : vertex_count(vcount), index_count(icount) {
    auto vsize = _vsize * vcount, isize = sizeof(uint16) * icount;
    auto staging_buffer = std::make_unique<buffer>(
        dev,
        vsize + isize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible
    );
    auto* staging_map = staging_buffer->map();
    write_buffer(staging_map);
    staging_buffer->unmap();

    vertex_buffer = std::make_unique<buffer>(
        dev,
        vsize,
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal
    );
    index_buffer = std::make_unique<buffer>(
        dev,
        isize,
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal
    );

    auto upload_commands = dev->alloc_tmp_cmd_buffer();
    upload_commands.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit}
    );
    // could definitly collect all the staging information + copy commands and then do this all at
    // once
    upload_commands.copyBuffer(
        staging_buffer->buf, vertex_buffer->buf, {vk::BufferCopy(0, 0, vsize)}
    );
    upload_commands.copyBuffer(
        staging_buffer->buf, index_buffer->buf, {vk::BufferCopy(vsize, 0, isize)}
    );
    upload_commands.end();
    dev->graphics_qu.submit(
        {
            vk::SubmitInfo{0, nullptr, nullptr, 1, &upload_commands}
    },
        nullptr
    );
    dev->tmp_upload_buffers.emplace_back(std::move(staging_buffer));
}

renderable::renderable(
    const std::shared_ptr<class geometry_set>& geo_src,
    size_t                                     mesh_index,
    std::shared_ptr<material>                  mat
)
    : geo_src(geo_src), mesh_index(mesh_index), mat(std::move(mat)) {
    if(geo_src != nullptr) {
        const auto& hdr = geo_src->header(mesh_index);
        bounds          = aabb(hdr.aabb_min, hdr.aabb_max);
        m               = geo_src->load_mesh(mesh_index);
    }
}
