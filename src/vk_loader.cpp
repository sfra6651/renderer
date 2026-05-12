#include "vk_loader.h"

#include "vk_engine.h"
#include "vk_types.h"
#include "lib/utils.h"

#include "stb_image.h"
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(
    VulkanEngine* engine, std::filesystem::path filePath) {
    log("Loading GLTF: ", filePath);

    auto dataResult = fastgltf::GltfDataBuffer::FromPath(filePath);
    if (dataResult.error() != fastgltf::Error::None) {
        logErr("Failed to open glTF file: ",
            fastgltf::getErrorMessage(dataResult.error()));
        return {};
    }

    constexpr auto gltfOptions = fastgltf::Options::LoadExternalBuffers;

    fastgltf::Parser parser{};
    auto load = parser.loadGltfBinary(
        dataResult.get(), filePath.parent_path(), gltfOptions);
    if (load.error() != fastgltf::Error::None) {
        logErr("Failed to parse glTF: ",
            fastgltf::getErrorMessage(load.error()));
        return {};
    }

    fastgltf::Asset gltf = std::move(load.get());

    std::vector<std::shared_ptr<MeshAsset>> meshes;

    // reuse the same vectors across meshes so we don't keep reallocating
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    for (fastgltf::Mesh& mesh : gltf.meshes) {
        MeshAsset newmesh;
        newmesh.name = std::string(mesh.name);

        indices.clear();
        vertices.clear();

        for (auto&& p : mesh.primitives) {
            GeoSurface newSurface;
            newSurface.startIndex = (uint32_t)indices.size();
            newSurface.count
                = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

            size_t initial_vtx = vertices.size();

            // load indices
            {
                fastgltf::Accessor& indexaccessor
                    = gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + indexaccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(
                    gltf, indexaccessor, [&](std::uint32_t idx) {
                        indices.push_back(idx + (uint32_t)initial_vtx);
                    });
            }

            // load vertex positions (always present)
            {
                fastgltf::Accessor& posAccessor
                    = gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(
                    gltf, posAccessor, [&](glm::vec3 v, size_t index) {
                        Vertex newvtx;
                        newvtx.position = v;
                        newvtx.normal = { 1, 0, 0 };
                        newvtx.color = glm::vec4{ 1.f };
                        newvtx.uv_x = 0;
                        newvtx.uv_y = 0;
                        vertices[initial_vtx + index] = newvtx;
                    });
            }

            // load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf,
                    gltf.accessors[normals->accessorIndex],
                    [&](glm::vec3 v, size_t index) {
                        vertices[initial_vtx + index].normal = v;
                    });
            }

            // load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf,
                    gltf.accessors[uv->accessorIndex],
                    [&](glm::vec2 v, size_t index) {
                        vertices[initial_vtx + index].uv_x = v.x;
                        vertices[initial_vtx + index].uv_y = v.y;
                    });
            }

            // load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf,
                    gltf.accessors[colors->accessorIndex],
                    [&](glm::vec4 v, size_t index) {
                        vertices[initial_vtx + index].color = v;
                    });
            }

            newmesh.surfaces.push_back(newSurface);
        }

        // debug: show normals as vertex colors
        constexpr bool OverrideColors = true;
        if (OverrideColors) {
            for (Vertex& vtx : vertices) {
                vtx.color = glm::vec4(vtx.normal, 1.f);
            }
        }

        newmesh.meshBuffers = engine->uploadMesh(indices, vertices);

        meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newmesh)));
    }

    return meshes;
}
